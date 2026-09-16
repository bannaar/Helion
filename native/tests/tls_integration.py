#!/usr/bin/env python3
"""Fresh disposable certificates; real native executables; no network services."""
import argparse
import os
import pathlib
import signal
import socket
import ssl
import subprocess
import tempfile
import time


def run(args, **kwargs):
    return subprocess.run(args, check=True, capture_output=True, text=True, timeout=30, **kwargs)


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


def start_server(binary, root, cert, key):
    port = free_port()
    process = subprocess.Popen([binary, str(port), str(root / 'tls.db'), '--cert', str(cert), '--key', str(key)],
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
        print(run([args.gameplay, args.server, str(cert), str(key)]).stdout.strip())
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
            if args.client:
                commands = '/create tls_pilot synthetic-password TLS Pilot\n/profile\n/quit\n'
                good = run([args.client, '127.0.0.1', str(port), '--ca', str(cert), '--terminal'], input=commands)
                assert 'OK CREATED user=tls_pilot' in good.stdout and 'PROFILE user=tls_pilot' in good.stdout
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
