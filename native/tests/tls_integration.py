#!/usr/bin/env python3
"""Fresh disposable certificates; real native executables; no network services."""
import argparse
import os
import pathlib
import signal
import socket
import ssl
import struct
import subprocess
import tempfile
import time


def run(args, **kwargs):
    timeout = kwargs.pop('timeout', 30)
    result = subprocess.run(args, capture_output=True, text=True, timeout=timeout, **kwargs)
    if result.returncode:
        raise AssertionError(f"command failed ({result.returncode}): {args}\n{result.stdout}\n{result.stderr}")
    return result


def certificate(root, name, san, expired=False):
    key, cert = root / (name + '.key'), root / (name + '.crt')
    if not expired:
        run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1',
             '-keyout', str(key), '-out', str(cert), '-subj', '/CN=localhost',
             '-addext', 'subjectAltName=' + san])
    else:
        csr, extensions = root / 'expired.csr', root / 'extensions.cnf'
        extensions.write_text('subjectAltName=' + san + '\n')
        run(['openssl', 'req', '-new', '-newkey', 'rsa:2048', '-nodes', '-keyout', str(key),
             '-out', str(csr), '-subj', '/CN=localhost'])
        run(['openssl', 'x509', '-req', '-in', str(csr), '-signkey', str(key), '-days', '-1',
             '-extfile', str(extensions), '-out', str(cert)])
    return cert, key


def free_port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def receive_until(connection, marker):
    received = b''
    for _ in range(20):
        received += connection.recv(4096)
        if marker in received:
            return received
    raise AssertionError(f'missing TLS response marker {marker!r}: {received!r}')


def binary_frame(message_type, request_id, payload, version=1):
    return b'HB' + struct.pack('!BBHII', version, message_type, 0, len(payload), request_id) + payload


def receive_exact(connection, size):
    received = b''
    while len(received) < size:
        chunk = connection.recv(size - len(received))
        if not chunk:
            raise AssertionError('truncated binary TLS response')
        received += chunk
    return received


def receive_binary(connection):
    header = receive_exact(connection, 14)
    magic, version, message_type, flags, length, request_id = struct.unpack('!2sBBHII', header)
    assert magic == b'HB' and version == 1 and flags == 0 and length <= 4096
    return message_type, request_id, receive_exact(connection, length)


def start_server(binary, root, cert, key, database='tls.db', extra=None):
    port = free_port()
    command = [binary, str(port), str(root / database)]
    if extra:
        command += extra
    command += ['--cert', str(cert), '--key', str(key)]
    process = subprocess.Popen(command,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(100):
        if process.poll() is not None:
            raise AssertionError('TLS server exited before accepting clients')
        try:
            with socket.create_connection(('127.0.0.1', port), timeout=.1):
                return process, port
        except OSError:
            time.sleep(.03)
    process.terminate()
    process.wait(timeout=8)
    raise AssertionError('TLS server did not start')


def stop(process):
    if process.poll() is None:
        process.send_signal(signal.SIGTERM)
    assert process.wait(timeout=8) == 0, 'clean TLS server shutdown'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--server', required=True)
    parser.add_argument('--gameplay', required=True)
    parser.add_argument('--client')
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='helion-tls-') as directory:
        root = pathlib.Path(directory)
        cert, key = certificate(root, 'trusted', 'DNS:localhost,IP:127.0.0.1')
        other, other_key = certificate(root, 'other', 'DNS:localhost')
        wrong, wrong_key = certificate(root, 'wrong', 'DNS:wrong.invalid')
        expired, expired_key = certificate(root, 'expired', 'DNS:localhost,IP:127.0.0.1', True)
        # Existing migration, lock, gameplay, rollback and restart suite now runs over TLS.
        print(run([args.gameplay, args.server, str(cert), str(key)], timeout=60).stdout.strip())
        missing = subprocess.run([args.server, str(free_port()), str(root / 'missing.db')], capture_output=True, timeout=8)
        assert missing.returncode != 0 and b'TLS requires' in missing.stderr
        mismatch = subprocess.run([args.server, str(free_port()), str(root / 'mismatch.db'), '--cert', str(cert),
                                   '--key', str(other_key)], capture_output=True, timeout=8)
        assert mismatch.returncode != 0
        assert not (root / 'missing.db').exists() and not (root / 'mismatch.db').exists()

        process, port = start_server(args.server, root, cert, key)
        try:
            context = ssl.create_default_context(cafile=str(cert))
            for identity, version in (('localhost', ssl.TLSVersion.TLSv1_2), ('127.0.0.1', ssl.TLSVersion.TLSv1_3)):
                context.minimum_version = version
                context.maximum_version = version
                with socket.create_connection(('127.0.0.1', port), timeout=3) as raw:
                    with context.wrap_socket(raw, server_hostname=identity) as secure:
                        assert secure.version() in ('TLSv1.2', 'TLSv1.3')
                        assert b'WELCOME Helion/2' in secure.recv(4096)
                        secure.sendall(b'STATE\nQUIT\n')
            # An old plaintext client gets neither a greeting nor protocol processing.
            with socket.create_connection(('127.0.0.1', port), timeout=3) as raw:
                raw.sendall(b'CREATE plaintext synthetic-password ShouldNotExist\n')
                try:
                    response = raw.recv(4096)
                except ConnectionResetError:
                    response = b''
                assert b'WELCOME' not in response and b'CREATED' not in response
            environment_context = ssl.create_default_context(cafile=str(cert))
            with socket.create_connection(('127.0.0.1', port), timeout=3) as raw:
                with environment_context.wrap_socket(raw, server_hostname='localhost') as secure:
                    assert b'WELCOME Helion/2' in receive_until(secure, b'binary-version=1')
                    secure.sendall(b'PROTOCOL BINARY 2\n')
                    assert b'ERR CLIENT_UPDATE_REQUIRED binary-version=1' in receive_until(
                        secure, b'CLIENT_UPDATE_REQUIRED')
                    secure.sendall(b'PROTOCOL BINARY 1\n')
                    assert b'OK PROTOCOL BINARY 1' in receive_until(secure, b'OK PROTOCOL BINARY 1')
                    secure.sendall(binary_frame(1, 1, b'STATE'))
                    message_type, request_id, payload = receive_binary(secure)
                    assert message_type == 2 and request_id == 1 and payload == b'STATE profiles=0 messages=0'
                    secure.sendall(binary_frame(
                        1, 2, b'CREATE binarypilot synthetic-password Binary Pilot'))
                    message_type, request_id, payload = receive_binary(secure)
                    assert message_type == 2 and request_id == 2 and payload.startswith(
                        b'OK CREATED user=binarypilot')
                    secure.sendall(binary_frame(3, 3, b'\x01\xff\x00'))
                    message_type, request_id, payload = receive_binary(secure)
                    assert message_type == 4 and request_id == 3 and len(payload) == 57
                    message_type, request_id, payload = receive_binary(secure)
                    assert message_type == 2 and request_id == 3 and payload.startswith(b'ACTIVE SHIP ')
                    secure.sendall(binary_frame(3, 4, b'\x02\x00\x00'))
                    message_type, request_id, payload = receive_binary(secure)
                    assert message_type == 2 and request_id == 4 and payload == b'ERR malformed-input'
            with socket.create_connection(('127.0.0.1', port), timeout=3) as raw:
                with environment_context.wrap_socket(raw, server_hostname='localhost') as secure:
                    receive_until(secure, b'binary-version=1')
                    secure.sendall(b'PROTOCOL BINARY 1\n')
                    receive_until(secure, b'OK PROTOCOL BINARY 1')
                    secure.sendall(binary_frame(1, 1, b'STATE'))
                    assert receive_binary(secure)[2] == b'STATE profiles=1 messages=0'
                    secure.sendall(binary_frame(1, 1, b'STATE'))
                    message_type, request_id, payload = receive_binary(secure)
                    assert message_type == 2 and request_id == 1 and payload == b'ERR replayed-request'
            print('binary protocol: negotiation, typed input/flight, compatibility and replay rejection passed')
            with socket.create_connection(('127.0.0.1', port), timeout=3) as raw:
                with environment_context.wrap_socket(raw, server_hostname='localhost') as secure:
                    assert b'WELCOME Helion/2' in secure.recv(4096)
                    secure.sendall(b'CREATE environmentpilot synthetic-password Environment Pilot\n')
                    receive_until(secure, b'OK CREATED user=environmentpilot')
                    secure.sendall(b'QUIT\n')
            if args.client:
                commands = '/protocol BINARY 1\n/create tls_pilot synthetic-password TLS Pilot\n/profile\n/quit\n'
                good = run([args.client, '127.0.0.1', str(port), '--ca', str(cert), '--terminal'], input=commands)
                assert 'OK CREATED user=tls_pilot' in good.stdout and 'PROFILE user=tls_pilot' in good.stdout
                assert 'Terminal compatibility mode uses bounded legacy text' in good.stdout
                assert 'OK PROTOCOL BINARY' not in good.stdout
                for trust in (other, None):
                    command = [args.client, '127.0.0.1', str(port), '--terminal']
                    if trust:
                        command += ['--ca', str(trust)]
                    bad = subprocess.run(command, input='/login tls_pilot synthetic-password\n',
                                         capture_output=True, text=True, timeout=10)
                    assert bad.returncode != 0 and 'verification failed' in bad.stderr
                print('native terminal client: encrypted account/profile flow and untrusted certificates passed')
        finally:
            stop(process)
        assert (root / 'tls.db').read_text().startswith('E\tdevelopment\n')
        crossed = subprocess.run(
            [args.server, str(free_port()), str(root / 'tls.db'), '--environment', 'test',
             '--cert', str(cert), '--key', str(key)], capture_output=True, text=True, timeout=8)
        assert crossed.returncode != 0 and 'persistence environment mismatch' in crossed.stderr
        legacy = root / 'untagged.db'
        legacy.write_text('M\tlegacy\tnot-production-state\n')
        untagged = subprocess.run(
            [args.server, str(free_port()), str(legacy), '--environment', 'production',
             '--cert', str(cert), '--key', str(key)], capture_output=True, text=True, timeout=8)
        assert untagged.returncode != 0 and 'untagged legacy persistence is development-only' in untagged.stderr
        malformed_environment = root / 'malformed-environment.db'
        malformed_environment.write_text('E\tdevelopment\textra\n')
        malformed = subprocess.run(
            [args.server, str(free_port()), str(malformed_environment), '--environment', 'development',
             '--cert', str(cert), '--key', str(key)], capture_output=True, text=True, timeout=8)
        assert malformed.returncode != 0 and 'malformed persistence environment' in malformed.stderr
        unknown = subprocess.run(
            [args.server, '--environment', 'staging', '--cert', str(cert), '--key', str(key)],
            capture_output=True, text=True, timeout=8)
        assert unknown.returncode == 2 and 'usage: helion_server' in unknown.stderr
        process, test_port = start_server(args.server, root, cert, key, 'test.db', ['--environment', 'test'])
        try:
            test_context = ssl.create_default_context(cafile=str(cert))
            with socket.create_connection(('127.0.0.1', test_port), timeout=3) as raw:
                with test_context.wrap_socket(raw, server_hostname='localhost') as secure:
                    assert b'WELCOME Helion/2' in secure.recv(4096)
                    secure.sendall(b'CREATE testidentity synthetic-password Test Identity\n')
                    receive_until(secure, b'OK CREATED user=testidentity')
                    secure.sendall(b'ECONOMY\n')
                    test_economy = receive_until(secure, b'ECONOMY END policy=TEST_100')
                    assert b'commodity=food sell-price=100 buy-price=80 stock=100000' in test_economy
                    secure.sendall(b'BUY food 1\n')
                    assert b'OK BOUGHT food quantity=1 total=100' in receive_until(secure, b'ACTIVE SHIP')
                    secure.sendall(b'OUTFIT LIST\n')
                    test_outfit = receive_until(secure, b'LOADOUT ')
                    assert b'MODULE id=pulse-laser' in test_outfit
                    assert b'price=100 canonical-price=650' in test_outfit
                    secure.sendall(b'SHIPYARD LIST\n')
                    test_shipyard = receive_until(secure, b'SHIPYARD END')
                    assert b'SHIPDEF id=TITAN_MULE' in test_shipyard
                    assert b'price=100 canonical-price=2800' in test_shipyard
                    secure.sendall(b'QUIT\n')
        finally:
            stop(process)
        assert (root / 'test.db').read_text().startswith('E\ttest\n')
        process, port = start_server(args.server, root, cert, key)
        stalled = socket.create_connection(('127.0.0.1', port), timeout=3)
        time.sleep(.1)
        started = time.monotonic()
        stop(process)
        stalled.close()
        assert time.monotonic() - started < 2, 'shutdown interrupts unfinished TLS handshakes'
        for invalid_cert, invalid_key in ((wrong, wrong_key), (expired, expired_key)):
            process, port = start_server(args.server, root, invalid_cert, invalid_key)
            try:
                if args.client:
                    bad = subprocess.run([args.client, '127.0.0.1', str(port), '--ca', str(invalid_cert), '--terminal'],
                                         input='/state\n', capture_output=True, text=True, timeout=10)
                    assert bad.returncode != 0 and 'verification failed' in bad.stderr
                else:
                    context = ssl.create_default_context(cafile=str(invalid_cert))
                    with socket.create_connection(('127.0.0.1', port), timeout=3) as raw:
                        try:
                            context.wrap_socket(raw, server_hostname='127.0.0.1')
                        except ssl.SSLCertVerificationError:
                            pass
                        else:
                            raise AssertionError('invalid certificate accepted')
            finally:
                stop(process)
        print('TLS: trusted DNS/IP, no plaintext, missing/mismatched keys, wrong-host and expired certificates passed')


if __name__ == '__main__':
    main()
