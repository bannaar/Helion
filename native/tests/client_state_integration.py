#!/usr/bin/env python3
"""Drive real SDL retry and re-login against a controlled TLS peer, in both wire modes.

The peer supplies server snapshots; the guarded acceptance driver injects only
ordinary UI events. Real gameplay/persistence is separately covered by the
unchanged binary and legacy graphical journeys.
"""
import argparse
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
from client_protocol_integration import line


def run_case(client, root, cert, key, legacy, mode):
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(cert, key)
    capture_dir = root / f'{"legacy" if legacy else "binary"}-{mode}'
    with socket.socket() as listener:
        listener.bind(('127.0.0.1', 0))
        listener.listen(1)
        listener.settimeout(15)
        command = [client, 'localhost', str(listener.getsockname()[1]), '--ca', str(cert),
                   '--renderer', 'core', '--acceptance-run', str(capture_dir),
                   '--acceptance-phase', 'lifecycle']
        if legacy:
            command.append('--legacy-protocol')
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                                   env=dict(os.environ, HELION_ACCEPTANCE='1', SDL_VIDEODRIVER='x11'))
        try:
            for generation in range(2):
                raw, _ = listener.accept()
                with context.wrap_socket(raw, server_side=True) as connection:
                    connection.settimeout(10)
                    connection.sendall(b'WELCOME Helion/2\nINFO binary-version=1\n')
                    if not legacy:
                        assert line(connection) == b'PROTOCOL BINARY 1\n'
                        connection.sendall(b'OK PROTOCOL BINARY 1\n')
                    authenticated = False
                    samples = 0
                    prior_request = 0
                    while True:
                        if legacy:
                            request = line(connection).strip()
                            kind, request_id = 1, 0
                        else:
                            kind, request_id, request = receive_binary(connection)
                            if prior_request == 0:
                                assert request_id == 1, 'request sequencing did not reset on retry'
                            assert request_id > prior_request, (request_id, prior_request)
                            prior_request = request_id
                        def send(text):
                            payload = text.encode()
                            connection.sendall(payload + b'\n' if legacy else binary_frame(2, request_id, payload))
                        if kind == 1 and request.startswith((b'CREATE ', b'LOGIN ')):
                            assert request.startswith(b'CREATE ' if generation == 0 else b'LOGIN '), request
                            authenticated = True
                            send('OK CREATED user=acceptance_pilot display=Pilot' if generation == 0 else
                                 'OK LOGIN user=acceptance_pilot display=Pilot')
                            # A partial status extension cannot establish initial docking
                            # from a default-constructed ship, especially after retry.
                            send('COMBAT STATUS destroyed=0 weapon-cooldown=0')
                        elif request == b'STATE':
                            send('STATE players=0')
                        elif kind == 3 or request.startswith((b'FLIGHT', b'INPUT')):
                            assert authenticated, 'client sent gameplay before server authentication'
                            docked = generation == 0 or mode == 'docked'
                            destroyed = generation == 1 and mode == 'recovery'
                            hull = 0 if destroyed else 100
                            if legacy:
                                send(f'FLIGHT 0 0 0 0 0 {int(docked)} 0 1500 0 0 0 0 0 {hull} 100')
                                send('FUEL 100 100')
                                send(f'COMBAT STATUS destroyed={int(destroyed)} weapon-cooldown=0')
                            else:
                                payload = struct.pack('!9fBHIIHHBHHB',
                                                      0, 0, 0, 0, 0, 0, 100, 100, 0,
                                                      int(docked), 0, 1500, 0, 0, 0, 0, hull, 100, int(destroyed))
                                connection.sendall(binary_frame(4, request_id, payload))
                            samples += 1
                            if generation == 0 and samples >= 8:
                                # Force loss after the first known docked session. The driver
                                # must show DISCONNECTED, press R, negotiate, and log in afresh.
                                break
                        elif request == b'PROFILE':
                            send('PROFILE display=Pilot faction=helion')
                        elif request == b'CAREER':
                            send('CAREER first=0 supply=0 response=0 purchased=0 intro=1 complete=0')
                        elif request == b'GALNET':
                            send('GALNET END')
                        elif request == b'OUTFIT LIST':
                            send('LOADOUT owned=mining-basic fitted-mining=mining-basic')
                        elif request == b'CONTACTS':
                            send('CONTACTS END')
                        elif request == b'QUIT':
                            break
                        else:
                            raise AssertionError((kind, request))
            out, err = process.communicate(timeout=10)
            assert process.returncode == 0, (out, err)
            expected = 'STATION_SERVICES' if mode == 'docked' else 'SPACE_FLIGHT'
            assert f'ACCEPTANCE PASS phase=lifecycle state={expected}' in out, (out, err)
            assert 'ENGINE state=DISCONNECTED' in out and out.count('ENGINE state=LOGIN') == 2, out
            assert out.count('ENGINE state=ORBIT_VIEW') == 2, out
            assert out.count('ENGINE state=STATION_SERVICES') == (2 if mode == 'docked' else 1), out
            assert f'flight={3 if mode == "recovery" else 1 if mode == "docked" else 2}' in out, out
            assert 'Kepler-Accept-2026' not in out + err, 'credential leaked'
            assert (capture_dir / 'disconnected.bmp').stat().st_size > 1024
            assert (capture_dir / 'restored.bmp').stat().st_size > 1024
        finally:
            if process.poll() is None:
                process.terminate()
                process.communicate(timeout=5)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--client', required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='helion-client-state-') as directory:
        root = pathlib.Path(directory)
        cert, key = certificate(root, 'trusted', 'DNS:localhost,IP:127.0.0.1')
        for legacy in (False, True):
            for mode in ('docked', 'flight', 'recovery'):
                run_case(args.client, root, cert, key, legacy, mode)
    print('SDL explicit retry, fresh authentication and docked/flight/recovery restoration passed in both protocols')


if __name__ == '__main__':
    main()
