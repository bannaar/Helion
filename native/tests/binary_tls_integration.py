#!/usr/bin/env python3
"""Adversarial binary transport checks against the real authoritative TLS server."""
import argparse
import contextlib
import pathlib
import socket
import ssl
import struct
import sys
import tempfile

sys.dont_write_bytecode = True
from tls_integration import binary_frame, certificate, receive_binary, start_server, stop


class Peer:
    def __init__(self, port, cert):
        context = ssl.create_default_context(cafile=str(cert))
        self.connection = context.wrap_socket(
            socket.create_connection(('127.0.0.1', port), timeout=5), server_hostname='localhost')
        assert self.line() == b'WELCOME Helion/2\n'
        assert b'binary-version=1' in self.line()

    def line(self):
        data = b''
        while not data.endswith(b'\n'):
            chunk = self.connection.recv(1)
            assert chunk and len(data) <= 4096, data
            data += chunk
        return data

    def negotiate(self):
        self.connection.sendall(b'PROTOCOL BINARY 1\n')
        assert self.line() == b'OK PROTOCOL BINARY 1\n'

    def send(self, request_id, command, kind=1):
        self.connection.sendall(binary_frame(kind, request_id, command))

    def frame(self):
        return receive_binary(self.connection)

    def closed(self):
        try:
            assert self.connection.recv(1) == b'', 'terminal error did not close the session'
        except (ConnectionResetError, ssl.SSLEOFError):
            pass

    def close(self):
        self.connection.close()


@contextlib.contextmanager
def peer(port, cert, binary=True):
    connection = Peer(port, cert)
    try:
        if binary:
            connection.negotiate()
        yield connection
    finally:
        connection.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--server', required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='helion-binary-tls-') as directory:
        root = pathlib.Path(directory)
        cert, key = certificate(root, 'trusted', 'DNS:localhost,IP:127.0.0.1')
        process, port = start_server(args.server, root, cert, key)
        try:
            with peer(port, cert, False) as p:
                p.connection.sendall(b'PROTOCOL BINARY 2\n')
                assert p.line() == b'ERR CLIENT_UPDATE_REQUIRED binary-version=1\n'
                p.connection.sendall(b'STATE\n')
                assert p.line() == b'STATE profiles=0 messages=0\n'
                p.negotiate()
                p.send(1, b'PROTOCOL BINARY 1')
                assert p.frame() == (2, 1, b'ERR protocol-negotiation-closed')
                p.send(2, b'STATE')
                assert p.frame() == (2, 2, b'STATE profiles=0 messages=0')
            for suffix in (b'STATE\n', b'STA'):
                with peer(port, cert, False) as p:
                    p.connection.sendall(b'PROTOCOL BINARY 1\n' + suffix)
                    assert p.line() == b'ERR protocol-negotiation-failed\n'
                    p.closed()
            with peer(port, cert, False) as p:
                # No magic-byte auto-detection or silent upgrade of legacy text.
                p.connection.sendall(binary_frame(1, 1, b'STATE') + b'\nSTATE\n')
                assert p.line() == b'ERR malformed-message\n'
                assert p.line() == b'STATE profiles=0 messages=0\n'

            state = binary_frame(1, 1, b'STATE')
            for split in range(1, len(state)):
                with peer(port, cert) as p:
                    p.connection.sendall(state[:split])
                    p.connection.sendall(state[split:])
                    assert p.frame() == (2, 1, b'STATE profiles=0 messages=0')
            with peer(port, cert) as p:
                p.connection.sendall(b''.join(binary_frame(1, i, b'STATE') for i in range(1, 9)))
                for i in range(1, 9):
                    assert p.frame() == (2, i, b'STATE profiles=0 messages=0')
                p.send(9, b'STATE' + b' ' * (4096 - 5))
                assert p.frame() == (2, 9, b'STATE profiles=0 messages=0')
                p.send(10, b'\x01\xff\x00', 3)
                assert p.frame() == (2, 10, b'ERR login-required')

            cases = []
            for offset, byte, error in ((0, 0, b'INVALID_MAGIC'), (2, 0, b'CLIENT_UPDATE_REQUIRED'),
                                        (2, 2, b'CLIENT_UPDATE_REQUIRED'), (3, 255, b'INVALID_MESSAGE_TYPE'),
                                        (4, 1, b'INVALID_FLAGS'), (5, 1, b'INVALID_FLAGS')):
                bad = bytearray(binary_frame(1, 2, b'STATE'))
                bad[offset] = byte
                cases.append((bytes(bad), 0, b'ERR ' + error))
            cases.append((b'HB' + struct.pack('!BBHII', 1, 1, 0, 4097, 2), 0, b'ERR OVERSIZED_PAYLOAD'))
            for kind in (2, 4):
                cases.append((binary_frame(kind, 2, b''), 2, b'ERR invalid-message-direction'))
            for payload in (b'\x02\x00\x00', b'\x01\x02\x00', b'\x01\x00\x02', b'\x01\x00'):
                cases.append((binary_frame(3, 2, payload), 2, b'ERR malformed-input'))
            for payload in (b'STATE\nQUIT', b'STATE\x00', b'STATE\r'):
                cases.append((binary_frame(1, 2, payload), 2, b'ERR malformed-message'))
            for request_id in (0, 1):
                cases.append((binary_frame(1, request_id, b'STATE'), request_id, b'ERR replayed-request'))
            for malformed, correlation, error in cases:
                with peer(port, cert) as p:
                    # The committed prefix must precede the failure in every read segmentation.
                    p.connection.sendall(state + malformed + binary_frame(1, 3, b'CREATE forbidden synthetic-password Forbidden'))
                    assert p.frame() == (2, 1, b'STATE profiles=0 messages=0')
                    assert p.frame() == (2, correlation, error)
                    p.closed()
            with peer(port, cert) as p:
                p.send(0xffffffff, b'STATE')
                assert p.frame() == (2, 0xffffffff, b'STATE profiles=0 messages=0')
                p.send(1, b'STATE')
                assert p.frame() == (2, 1, b'ERR replayed-request')
                p.closed()
            creation = binary_frame(1, 1, b'CREATE truncated synthetic-password Truncated')
            for size in (1, 13, 14, len(creation) - 1):
                with peer(port, cert) as p:
                    p.connection.sendall(creation[:size])
                with peer(port, cert) as p:
                    p.send(1, b'STATE')
                    assert p.frame() == (2, 1, b'STATE profiles=0 messages=0')

            with peer(port, cert) as sender, peer(port, cert) as observer:
                sender.send(1, b'CREATE sender synthetic-password Sender')
                assert sender.frame()[2].startswith(b'OK CREATED')
                observer.send(1, b'CREATE observer synthetic-password Observer')
                assert observer.frame()[2].startswith(b'OK CREATED')
                sender.send(900, b'CHAT correlation-check')
                for p in (sender, observer):
                    kind, correlation, payload = p.frame()
                    assert kind == 2 and correlation == 0 and b'correlation-check' in payload
                observer.send(2, b'STATE')
                assert observer.frame() == (2, 2, b'STATE profiles=2 messages=1')
                sender.send(901, b'\x01\xff\x00', 3)
                kind, correlation, flight = sender.frame()
                assert kind == 4 and correlation == 901 and len(flight) == 57
                initial_credits = struct.unpack('!I', flight[39:43])[0]
                assert sender.frame()[2].startswith(b'ACTIVE SHIP ')
                buy = binary_frame(1, 902, b'BUY food 1')
                sender.connection.sendall(buy + buy + binary_frame(1, 903, b'BUY food 1'))
                transaction = sender.frame()
                assert transaction == (2, 902, b'OK BOUGHT food quantity=1 total=20'), transaction
                kind, correlation, flight = sender.frame()
                assert kind == 4 and correlation == 902
                assert struct.unpack('!I', flight[39:43])[0] == initial_credits - 20
                assert struct.unpack('!H', flight[47:49])[0] == 1
                assert sender.frame()[2].startswith(b'ACTIVE SHIP ')
                assert sender.frame() == (2, 902, b'ERR replayed-request')
                sender.closed()
            # Fresh connection IDs can restart at 1; replay protection is connection-scoped.
            with peer(port, cert) as p:
                p.send(1, b'LOGIN sender synthetic-password')
                assert p.frame()[2].startswith(b'OK LOGIN')
                p.send(2, b'FLIGHT')
                _, _, flight = p.frame()
                assert struct.unpack('!H', flight[47:49])[0] == 1, 'replay repeated a durable purchase'
                assert p.frame()[2].startswith(b'ACTIVE SHIP ')
            with peer(port, cert, False) as p:
                p.connection.sendall(b'LOGIN sender synthetic-password\n')
                assert p.line().startswith(b'OK LOGIN')
                p.connection.sendall(b'PROTOCOL BINARY 1\n')
                assert p.line() == b'ERR protocol-negotiation-closed\n'
                p.connection.sendall(b'STATE\n')
                assert p.line() == b'STATE profiles=2 messages=1\n'
            print('Binary TLS negotiation, fragmentation, ordering, bounds, direction, typed authority, replay and legacy compatibility passed')
        finally:
            stop(process)


if __name__ == '__main__':
    main()
