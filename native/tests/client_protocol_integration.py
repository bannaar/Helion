#!/usr/bin/env python3
"""Exercise the real graphical client's transport against an adversarial TLS peer."""
import argparse
import contextlib
import os
import pathlib
import socket
import ssl
import struct
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from tls_integration import binary_frame, certificate, receive_binary


def line(connection):
    result = b''
    while not result.endswith(b'\n'):
        part = connection.recv(1)
        assert part and len(result) < 4096, result
        result += part
    return result


def closed(connection):
    try:
        assert connection.recv(1) == b'', 'client accepted invalid protocol traffic'
    except (ConnectionResetError, ssl.SSLEOFError):
        pass


@contextlib.contextmanager
def client_peer(client, root, cert, key, legacy=False):
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(cert, key)
    with socket.socket() as listener:
        listener.bind(('127.0.0.1', 0))
        listener.listen(1)
        listener.settimeout(10)
        command = [client, 'localhost', str(listener.getsockname()[1]), '--ca', str(cert),
                   '--renderer', 'core', '--acceptance-run', str(root / 'captures')]
        if legacy:
            command.append('--legacy-protocol')
        environment = dict(os.environ, HELION_ACCEPTANCE='1', SDL_VIDEODRIVER='x11')
        process = subprocess.Popen(command, env=environment, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            raw, _ = listener.accept()
            with context.wrap_socket(raw, server_side=True) as connection:
                connection.settimeout(5)
                yield connection
        finally:
            process.terminate()
            try:
                process.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.communicate(timeout=5)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--client', required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='helion-client-protocol-') as directory:
        root = pathlib.Path(directory)
        cert, key = certificate(root, 'trusted', 'DNS:localhost,IP:127.0.0.1')
        greeting = b'WELCOME Helion/2\nINFO binary-version=1\n'
        ack = b'OK PROTOCOL BINARY 1\n'
        with client_peer(args.client, root, cert, key) as connection:
            connection.sendall(greeting)
            assert line(connection) == b'PROTOCOL BINARY 1\n'
            # ACK and binary bytes deliberately occupy the same TLS record.
            connection.sendall(ack + binary_frame(2, 0, b'INFO ready'))
            kind, request, payload = receive_binary(connection)
            assert kind == 1 and request > 0 and payload.startswith(b'CREATE ')
        with client_peer(args.client, root, cert, key, legacy=True) as connection:
            connection.sendall(greeting)
            assert line(connection).startswith(b'CREATE '), 'explicit legacy mode negotiated binary'
        for response in (b'WELCOME Helion/3\n', greeting + ack):
            with client_peer(args.client, root, cert, key) as connection:
                connection.sendall(response)
                closed(connection)
        invalid_snapshot = bytearray(57)
        invalid_snapshot[36] = 2
        bad_flags = bytearray(binary_frame(2, 0, b'INFO test'))
        bad_flags[5] = 1
        failures = [
            b'ERR CLIENT_UPDATE_REQUIRED binary-version=2\n',
            b'ERR unknown-command\n', b'OK PROTOCOL BINARY 2\n',
            ack + binary_frame(2, 0, b'INFO test', version=2),
            ack + bytes(bad_flags),
            ack + b'HB' + struct.pack('!BBHII', 1, 2, 0, 4097, 0),
            ack + binary_frame(3, 0, b'\0\0\0'),
            ack + binary_frame(4, 0, invalid_snapshot),
            ack + binary_frame(2, 0, b'INFO bad\n'),
            ack + binary_frame(2, 1, b'INFO future-id'),
            ack + binary_frame(2, 0, b'OK PROTOCOL BINARY 1'),
        ]
        for response in failures:
            with client_peer(args.client, root, cert, key) as connection:
                connection.sendall(greeting)
                assert line(connection) == b'PROTOCOL BINARY 1\n'
                connection.sendall(response)
                closed(connection)
    print('Native client TLS negotiation, explicit compatibility, coalescing and rejection passed')


if __name__ == '__main__':
    main()
