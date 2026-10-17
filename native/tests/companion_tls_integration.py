#!/usr/bin/env python3
"""Companion grants share M4 dispatch, persistence and authorization on both transports."""
import argparse
import pathlib
import struct
import sys
import tempfile
import time

sys.dont_write_bytecode = True
from binary_tls_integration import peer
from tls_integration import certificate, start_server, stop


class Session:
    def __init__(self, connection, binary):
        self.peer = connection
        self.binary = binary
        self.sequence = 0

    def command(self, command, expected, end=None):
        self.sequence += 1
        if self.binary:
            self.peer.send(self.sequence, command.encode())
        else:
            self.peer.connection.sendall(command.encode() + b'\n')
        replies = []
        while True:
            if self.binary:
                kind, sequence, payload = self.peer.frame()
                expected_sequence = 0 if expected.startswith('CHAT ') else self.sequence
                assert kind == 2 and sequence == expected_sequence, (kind, sequence, payload)
            else:
                payload = self.peer.line().rstrip(b'\n')
            replies.append(payload.decode())
            if end is None or payload.startswith(end.encode()):
                break
        result = '\n'.join(replies)
        # Never include the submitted credential in test diagnostics.
        assert expected in result, (expected, result)
        return result


def fields(response):
    return dict(item.split('=', 1) for item in response.split() if '=' in item)


def exercise(server, root, cert, key, binary):
    database = 'binary.db' if binary else 'legacy.db'
    process, port = start_server(server, root, cert, key, database)
    try:
        with peer(port, cert, binary) as owner_peer, peer(port, cert, binary) as companion_peer:
            owner, companion = Session(owner_peer, binary), Session(companion_peer, binary)
            companion.command('COMPANION ISSUE', 'ERR login-required')
            companion.command('COMPANION AUTH malformed', 'ERR invalid-companion-token')
            owner.command('CREATE owner synthetic-password Owner', 'OK CREATED')
            issued = fields(owner.command('COMPANION ISSUE', 'OK COMPANION ISSUED'))
            assert issued['scope'] == 'profile.read'
            assert 2591990 <= int(issued['expires']) - time.time() <= 2592000
            token, token_id = issued['token'], issued['id']
            companion.command('COMPANION AUTH ' + token[:-1] + ('0' if token[-1] != '0' else '1'),
                              'ERR invalid-companion-token')
            # Also cover demotion from player authority and broadcast removal.
            companion.command('LOGIN owner synthetic-password', 'OK LOGIN')
            companion.command('COMPANION AUTH ' + token, 'OK COMPANION AUTH')
            owner.command('CHAT private-player-message', 'CHAT user=owner')
            companion.command('PROFILE', 'PROFILE user=owner')
            companion.command('STATE', 'STATE profiles=1 messages=1')
            companion.command('PROTOCOL BINARY 1', 'ERR protocol-negotiation-closed')
            denied = (
                'LOGIN owner synthetic-password', 'CREATE escape synthetic-password Escape',
                'COMPANION ISSUE', 'COMPANION LIST', 'COMPANION REVOKE ' + token_id,
                'COMPANION AUTH ' + token, 'CHAT forbidden', 'BUY food 1', 'SELL food 1',
                'LAUNCH', 'INPUT 1 0 0', 'FLIGHT', 'MINE', 'DOCK', 'CONTACTS', 'MISSION',
                'ACCEPT', 'TURNIN', 'UPGRADE hull', 'REPAIR', 'REFUEL', 'OUTFIT LIST',
                'SHIPYARD LIST', 'FIRE RAIDER-1', 'RECOVER', 'CAREER', 'GALAXY', 'ECONOMY', 'GALNET',
            )
            for request in denied:
                # Auth requests have an intentional per-connection budget.
                companion.command(request, 'ERR scope-denied')
            if binary:
                companion.sequence += 1
                companion_peer.send(companion.sequence, struct.pack('bbb', 1, 0, 0), kind=3)
                assert companion_peer.frame() == (2, companion.sequence, b'ERR scope-denied')
            owner.command('COMPANION REVOKE ' + token_id, 'OK COMPANION REVOKED')
            companion.command('PROFILE', 'ERR invalid-companion-token')
            companion.command('QUIT', 'OK BYE')
            with peer(port, cert, binary) as fresh:
                Session(fresh, binary).command('COMPANION AUTH ' + token, 'ERR invalid-companion-token')
            issued = fields(owner.command('COMPANION ISSUE', 'OK COMPANION ISSUED'))
            # Token quota and owner isolation are shared across wire modes.
            for _ in range(7):
                owner.command('COMPANION ISSUE', 'OK COMPANION ISSUED')
            owner.command('COMPANION ISSUE', 'ERR companion-token-limit')
            with peer(port, cert, not binary) as other_peer:
                other = Session(other_peer, not binary)
                other.command('CREATE stranger synthetic-password Stranger', 'OK CREATED')
                other.command('COMPANION REVOKE ' + issued['id'], 'ERR companion-token-not-found')
            metadata = owner.command('COMPANION LIST', 'id=' + issued['id'], 'COMPANION END')
            assert 'token=' not in metadata and '$sha256$' not in metadata
    finally:
        stop(process)
    data = root / database
    snapshot = data.read_text()
    assert token not in snapshot and issued['token'] not in snapshot
    assert 'T\t' + issued['id'] + '\towner\t$sha256$' in snapshot
    assert sum(line.startswith('Y\t') for line in snapshot.splitlines()) == 200
    assert sum(line.startswith('D\t') for line in snapshot.splitlines()) == 400
    assert any(line.startswith('T\t') and line.count('\t') == 1 for line in snapshot.splitlines())
    # origin/main's pre-M3 format: H profile projections and six-column T
    # grants, without environment, galaxy, ownership or telemetry records.
    legacy_rows = []
    for row in snapshot.splitlines():
        if row.startswith('H\t'):
            legacy_rows.append('\t'.join(row.split('\t')[:24]))
        elif row.startswith('T\t') and row.count('\t') == 5:
            legacy_rows.append(row)
    legacy_database = database + '.pre-m3'
    legacy_data = root / legacy_database
    legacy_data.write_text('\n'.join(legacy_rows) + '\n')
    legacy_process, legacy_port = start_server(server, root, cert, key, legacy_database)
    try:
        with peer(legacy_port, cert, binary) as migrated_peer:
            migrated = Session(migrated_peer, binary)
            migrated.command('COMPANION AUTH ' + issued['token'], 'OK COMPANION AUTH')
            migrated.command('PROFILE', 'PROFILE user=owner')
    finally:
        stop(legacy_process)
    migrated_snapshot = legacy_data.read_text()
    assert migrated_snapshot.startswith('E\tdevelopment\nV\tGALAXY_KNOWLEDGE\t1\n')
    assert sum(row.startswith('Y\t') for row in migrated_snapshot.splitlines()) == 200
    assert issued['token'] not in migrated_snapshot
    # Set only the grant expiry in a disposable persisted fixture. Keep the M3
    # record count/terminator intact; no production clock or expiry bypass.
    expiration = int(time.time()) + 5
    rows = snapshot.splitlines()
    for index, row in enumerate(rows):
        if row.startswith('T\t' + issued['id'] + '\t'):
            parts = row.split('\t')
            parts[4] = str(expiration)
            rows[index] = '\t'.join(parts)
    data.write_text('\n'.join(rows) + '\n')
    process, port = start_server(server, root, cert, key, database)
    try:
        with peer(port, cert, binary) as restored_peer:
            restored = Session(restored_peer, binary)
            restored.command('COMPANION AUTH ' + issued['token'], 'OK COMPANION AUTH')
            restored.command('PROFILE', 'PROFILE user=owner')
            time.sleep(max(0, expiration - time.time()) + .1)
            restored.command('PROFILE', 'ERR companion-token-expired')
            with peer(port, cert, binary) as expired_peer:
                expired = Session(expired_peer, binary)
                expired.command('COMPANION AUTH ' + issued['token'], 'ERR companion-token-expired')
                # Failed authentication does not grant authority. A subsequent
                # full player login can perform a durable write, pruning expiry.
                expired.command('LOGIN owner synthetic-password', 'OK LOGIN')
                expired.command('CHAT snapshot-after-expiry', 'CHAT user=owner')
    finally:
        stop(process)
    assert 'T\t' + issued['id'] + '\t' not in data.read_text()
    print(('binary' if binary else 'legacy') + ' companion TLS: grants, scope, typed input, quota, ownership, '
          'broadcast isolation, revocation, restart, expiry and M3 persistence passed')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--server', required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='helion-companion-tls-') as directory:
        root = pathlib.Path(directory)
        cert, key = certificate(root, 'trusted', 'DNS:localhost,IP:127.0.0.1')
        for binary in (True, False):
            exercise(args.server, root, cert, key, binary)


if __name__ == '__main__':
    main()
