#!/usr/bin/env python3
"""Real TLS/binary authorization, durable transactions and isolated recovery."""
import argparse
import hashlib
import json
import pathlib
import re
import socket
import ssl
import subprocess
import sys
import tempfile
import time
sys.dont_write_bytecode = True
from tls_integration import certificate, free_port, start_server, stop
from binary_tls_integration import Peer

PASSWORD = 'M6-private-invitation-secret'

class Session(Peer):
    def command(self, text, marker=None):
        self.connection.sendall(text.encode() + b'\n')
        output = self.line()
        while marker and marker not in output:
            output += self.line()
        return output.decode()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--server', required=True)
    server = parser.parse_args().server
    with tempfile.TemporaryDirectory(prefix='helion-m6-admin-') as directory:
        base = pathlib.Path(directory)
        verifier = subprocess.run([server, '--hash-password'], input=PASSWORD + '\n',
                                  capture_output=True, text=True, check=True).stdout.strip()
        assert len(verifier) == 115

        def setup(name, environment, economy=None):
            root = base / name
            root.mkdir(mode=0o700)
            cert, key = certificate(root, 'identity', 'DNS:localhost,IP:127.0.0.1')
            cert.chmod(0o600)
            access = root / 'access'
            access.write_text(f'HELION_ACCESS_V1 {environment} {name}\n' + ''.join(
                f'{account} {role} {verifier}\n' for account, role in (
                    ('admin', 'DEV_ADMIN'), ('tester', 'INVITED_TESTER'), ('normal', 'NORMAL_PLAYER'))))
            access.chmod(0o600)
            extra = ['--environment', environment, '--access-file', str(access)]
            if economy:
                extra += ['--test-economy', economy]
            return root, cert, key, extra

        test, cert, key, extra = setup('private', 'test')
        production, prod_cert, prod_key, prod_extra = setup('canonical', 'production')

        def recovery(action, identity, password=PASSWORD, additions=()):
            return subprocess.run([server, str(free_port()), str(test / 'state.db'), *extra,
                                   '--cert', str(cert), '--key', str(key), '--test-recovery', 'TEST', action, identity,
                                   '--operator', 'admin', *additions], input=password + '\n',
                                  capture_output=True, text=True, timeout=12)

        def admin(p, identity, operation, target='tester', field='gsc', value='-', expected='OK TESTADMIN'):
            answer = p.command(f'TESTADMIN TEST {identity} {operation} {target} {field} {value}')
            assert expected in answer, (operation, field, answer)
            return answer

        process, port = start_server(server, test, cert, key, 'state.db', extra)
        try:
            # Zero connected players does not terminate or reset the universe.
            time.sleep(.15)
            assert process.poll() is None
            with_session = Session(port, cert)
            try:
                assert 'test-environment' not in admin(with_session, 1, 'ADD', value='10', expected='ERR player-authentication-required')
                assert 'private-test-access-denied' in with_session.command('CREATE stranger ' + PASSWORD + ' Stranger')
                assert 'private-test-access-denied' in with_session.command('CREATE admin Wrong-password-2026 Admin')
                assert 'OK CREATED' in with_session.command('CREATE tester ' + PASSWORD + ' Tester')
                admin(with_session, 2, 'ADD', value='10', expected='ERR dev-admin-required')
                assert 'ERR' in with_session.command('TESTADMIN TEST 3 ADD tester gsc 10 role=DEV_ADMIN environment=test')
                economy = with_session.command('ECONOMY', b'ECONOMY END')
                assert 'sell-price=100 buy-price=80 stock=100000' in economy
                assert 'price=100 canonical-price=2800' in with_session.command('SHIPYARD LIST', b'SHIPYARD END')
                assert 'cost=100' in with_session.command('SHIPYARD BUY TITAN_MULE', b'SHIPYARD END')
                assert 'OK CREATED' in with_session.command('CREATE normal ' + PASSWORD + ' Normal')
                admin(with_session, 4, 'ADD', value='10', expected='ERR dev-admin-required')
                assert 'OK CREATED' in with_session.command('CREATE admin ' + PASSWORD + ' Admin')
                admin(with_session, 10, 'ADD', value='250')
                assert 'value=1650' in admin(with_session, 11, 'GET')
                assert 'replay=1' in admin(with_session, 10, 'ADD', value='250')
                admin(with_session, 10, 'ADD', value='251', expected='ERR request-id-conflict')
                admin(with_session, 12, 'ADD', field='experience', value='75')
                assert 'replay=1' in admin(with_session, 12, 'ADD', field='experience', value='75')
                assert 'value=75' in admin(with_session, 101, 'GET', field='experience')
                # Failure to durably write an audit intent denies the mutation.
                audit_path = test / 'state.db.audit.jsonl'
                audit_path.chmod(0o644)
                admin(with_session, 102, 'ADD', value='900', expected='ERR audit-unavailable')
                audit_path.chmod(0o600)
                assert 'value=1650' in admin(with_session, 103, 'GET')
                admin(with_session, 13, 'ADD', field='food', value='2')
                admin(with_session, 13, 'ADD', field='food', value='2')
                assert 'value=2' in admin(with_session, 14, 'GET', field='food')
                admin(with_session, 15, 'ADD', field='ore', value='9999', expected='ERR ship-invariant-violation')
                admin(with_session, 16, 'SET', field='fuel', value='9999', expected='ERR quantity-out-of-range')
                admin(with_session, 17, 'GRANT', field='ship', value='COMPACT_MILITIA')
                assert 'replay=1' in admin(with_session, 17, 'GRANT', field='ship', value='COMPACT_MILITIA')
                admin(with_session, 18, 'GRANT', field='module', value='pulse-laser')
                admin(with_session, 19, 'REVOKE', field='module', value='pulse-laser')
                admin(with_session, 20, 'TELEPORT', field='station', value='1')
                admin(with_session, 21, 'RESET', target='universe', field='markets', value='seed')
                admin(with_session, 22, 'SET', field='gsc', value='-1', expected='ERR invalid-quantity')
                admin(with_session, 23, 'SET', field='gsc', value='999999999999999999999999999', expected='ERR invalid-quantity')
                # Malformed credential-shaped input is never included in audit.
                assert 'ERR' in with_session.command('TESTADMIN TEST 24 SET tester password ' + PASSWORD)
                assert 'ERR' in with_session.command('TESTADMIN TEST 25 ADD tester gsc ' + PASSWORD)
                token_reply = with_session.command('COMPANION ISSUE')
                bearer = re.search(r'token=(\S+)', token_reply).group(1)
                with SessionContext(port, cert) as companion:
                    assert 'OK COMPANION AUTH' in companion.command('COMPANION AUTH ' + bearer)
                    admin(companion, 26, 'ADD', value='10', expected='ERR player-authentication-required')
                    assert 'ERR scope-denied' in companion.command('LAUNCH')
                # The negotiated binary envelope reaches the same authorization.
                with SessionContext(port, cert) as binary:
                    binary.negotiate()
                    binary.send(1, ('LOGIN admin ' + PASSWORD).encode())
                    assert b'OK LOGIN' in binary.frame()[2]
                    binary.send(2, b'TESTADMIN TEST 27 ADD tester gsc 50')
                    assert binary.frame()[2].startswith(b'OK TESTADMIN')
                    binary.send(3, b'TESTADMIN TEST 27 ADD tester gsc 50')
                    assert b'replay=1' in binary.frame()[2]
            finally:
                with_session.close()
            time.sleep(.1)
            assert process.poll() is None
        finally:
            stop(process)

        audit_file = test / 'state.db.audit.jsonl'
        records = [json.loads(row) for row in audit_file.read_text().splitlines()]
        assert all(row['environment'] == 'test' and row['instance'] == 'private' for row in records)
        assert {'intent', 'success', 'failure', 'denied', 'replayed'} <= {row['result'] for row in records}
        assert any(row['account'] == 'tester' and row['role'] == 'INVITED_TESTER' and row['result'] == 'denied' for row in records)
        assert any(row['before'] == '1400' and row['after'] == '1650' and row['requested'] == '250' for row in records)
        assert PASSWORD not in audit_file.read_text() and bearer not in audit_file.read_text() and verifier not in audit_file.read_text()
        assert audit_file.stat().st_mode & 0o777 == 0o600

        # An unauthenticated client cannot stream unbounded fsynced admin logs
        # over one connection. The limit rejection is itself audited.
        process, port = start_server(server, test, cert, key, 'state.db', extra)
        try:
            with SessionContext(port, cert) as limited:
                for request_id in range(64):
                    admin(limited, request_id, 'GET', expected='ERR player-authentication-required')
                admin(limited, 65, 'GET', expected='ERR admin-request-limit')
                limited.closed()
        finally:
            stop(process)
        assert json.loads(audit_file.read_text().splitlines()[-1])['reason'] == 'admin-request-limit'

        # Offline recovery must acquire the same exclusive lock and authenticate.
        assert recovery('SNAPSHOT', 'checkpoint', password='wrong-password').returncode != 0
        assert recovery('SNAPSHOT', 'checkpoint').returncode == 0
        snapshot = test / '.helion-test-snapshots/checkpoint.snapshot'
        assert snapshot.is_file() and snapshot.stat().st_mode & 0o777 == 0o600
        snapshot_before = snapshot.read_bytes()
        assert recovery('SNAPSHOT', 'checkpoint').returncode != 0  # immutable ID
        assert snapshot.read_bytes() == snapshot_before

        process, port = start_server(server, test, cert, key, 'state.db', extra)
        try:
            assert recovery('RESET', 'locked').returncode != 0
            with SessionContext(port, cert) as p:
                assert 'OK LOGIN' in p.command('LOGIN admin ' + PASSWORD)
                assert 'replay=1' in admin(p, 10, 'ADD', value='250')
                assert 'value=1700' in admin(p, 30, 'GET')
                admin(p, 31, 'ADD', field='experience', value='1')
                admin(p, 32, 'RESET', field='player', value='starter')
                assert 'value=1500' in admin(p, 33, 'GET')
                admin(p, 34, 'ADD', value='777')
        finally:
            stop(process)
        assert recovery('RESTORE', 'checkpoint').returncode == 0
        # Receipt history survives rollback, preventing old grants from replaying.
        process, port = start_server(server, test, cert, key, 'state.db', extra)
        try:
            with SessionContext(port, cert) as p:
                assert 'OK LOGIN' in p.command('LOGIN admin ' + PASSWORD)
                assert 'value=1700' in admin(p, 35, 'GET')
                assert 'replay=1' in admin(p, 34, 'ADD', value='777')
                assert 'value=1700' in admin(p, 36, 'GET')
                assert 'ERR invalid-companion-token' in p.command('COMPANION AUTH ' + bearer)
        finally:
            stop(process)
        assert recovery('RESET', 'reseed').returncode == 0
        assert recovery('RESET', 'reseed').returncode != 0
        before_invalid = (test / 'state.db').read_bytes()
        linked = snapshot.with_name('linked.snapshot')
        linked.symlink_to(snapshot)
        assert recovery('RESTORE', 'linked').returncode != 0
        assert (test / 'state.db').read_bytes() == before_invalid
        # Corrupt hash, identity, schema, and a correctly hashed invalid payload.
        for index, mutation in enumerate((
            snapshot_before.replace(b' test ', b' production ', 1),
            snapshot_before.replace(b'SNAPSHOT_V1', b'SNAPSHOT_V9', 1),
            snapshot_before + b'invalid',
            snapshot_before.replace(b' test private ', b' test another ', 1),
        )):
            bad_id = 'bad' + str(index)
            bad = snapshot.with_name(bad_id + '.snapshot')
            bad.write_bytes(mutation.replace(b' checkpoint ', (' ' + bad_id + ' ').encode(), 1))
            bad.chmod(0o600)
            assert recovery('RESTORE', bad_id).returncode != 0
            assert (test / 'state.db').read_bytes() == before_invalid
        header, payload = snapshot_before.split(b'\n', 1)
        payload = payload.replace(b'E\ttest\n', b'E\tproduction\n', 1)
        parts = header.decode().split(); parts[4] = 'wrong-state'; parts[-1] = hashlib.sha256(payload).hexdigest()
        bad = snapshot.with_name('wrong-state.snapshot')
        bad.write_bytes(' '.join(parts).encode() + b'\n' + payload); bad.chmod(0o600)
        assert recovery('RESTORE', 'wrong-state').returncode != 0
        assert (test / 'state.db').read_bytes() == before_invalid

        # Canonical Production may start with no players and keeps normal prices.
        process, port = start_server(server, production, prod_cert, prod_key, 'state.db', prod_extra)
        try:
            time.sleep(.15)
            assert process.poll() is None
            with SessionContext(port, prod_cert) as p:
                assert 'OK CREATED' in p.command('CREATE admin ' + PASSWORD + ' Admin')
                admin(p, 40, 'ADD', target='admin', value='100', expected='ERR test-environment-required')
                assert 'sell-price=20' in p.command('ECONOMY', b'ECONOMY END policy=PRODUCTION_LIKE')
                assert 'price=2800 canonical-price=2800' in p.command('SHIPYARD LIST', b'SHIPYARD END')
                assert 'ERR insufficient-credits' in p.command('SHIPYARD BUY TITAN_MULE', b'SHIPYARD END')
                assert 'total=20' in p.command('BUY food 1', b'ACTIVE SHIP')
                assert 'credits=1480' in p.command('PROFILE')
                assert 'OK CREATED' in p.command('CREATE é ' + PASSWORD + ' LegacyIdentifier')
                admin(p, 41, 'ADD', target='admin', value='100', expected='ERR test-environment-required')
            time.sleep(.15)
            assert process.poll() is None, 'Production remains operational after logout'
        finally:
            stop(process)
        canonical = (production / 'state.db').read_bytes()
        assert canonical.startswith(b'E\tproduction\n') and b'\t1480\t0\t' in canonical
        production_audit = [json.loads(row) for row in (production / 'state.db.audit.jsonl').read_text().splitlines()]
        assert any(row['result'] == 'denied' for row in production_audit)
        assert any(row['account_id_hex'] == 'c3a9' and row['result'] == 'denied' for row in production_audit)
        assert recovery('RESET', 'another').returncode == 0
        assert (production / 'state.db').read_bytes() == canonical
        process, port = start_server(server, production, prod_cert, prod_key, 'state.db', prod_extra)
        try:
            with SessionContext(port, prod_cert) as p:
                assert 'OK LOGIN' in p.command('LOGIN admin ' + PASSWORD)
                assert 'credits=1480' in p.command('PROFILE')
        finally:
            stop(process)
        # Even a fresh filename in a claimed TEST directory cannot become Production.
        crossed = subprocess.run([server, str(free_port()), str(test / 'fresh.db'), '--environment', 'production',
                                  '--cert', str(cert), '--key', str(key)], capture_output=True, text=True, timeout=8)
        assert crossed.returncode != 0 and 'storage environment' in crossed.stderr
        assert not (test / 'fresh.db').exists()
        nested = test / 'nested-production'
        nested.mkdir(mode=0o700)
        crossed_nested = subprocess.run([server, str(free_port()), str(nested / 'state.db'), '--environment', 'production',
                                         '--cert', str(prod_cert), '--key', str(prod_key)], capture_output=True, text=True, timeout=8)
        assert crossed_nested.returncode != 0 and 'nested cross-environment' in crossed_nested.stderr
        shared_key = subprocess.run([server, str(free_port()), str(production / 'state.db'), *prod_extra,
                                     '--cert', str(cert), '--key', str(key)], capture_output=True, text=True, timeout=8)
        assert shared_key.returncode != 0 and 'cross-environment credential' in shared_key.stderr
        for options in (['--environment', 'test'], ['--environment', 'production', '--environment', 'test']):
            ambiguous = subprocess.run([server, str(free_port()), str(base / 'ambiguous.db'), *options,
                                       '--cert', str(cert), '--key', str(key)], capture_output=True, text=True, timeout=8)
            assert ambiguous.returncode != 0
        realistic, rcert, rkey, rextra = setup('realistic', 'test', 'production-like')
        process, port = start_server(server, realistic, rcert, rkey, 'state.db', rextra)
        try:
            with SessionContext(port, rcert) as p:
                assert 'OK CREATED' in p.command('CREATE tester ' + PASSWORD + ' Tester')
                assert 'sell-price=20' in p.command('ECONOMY', b'ECONOMY END policy=PRODUCTION_LIKE')
        finally:
            stop(process)
        print('M6 TLS/binary roles, invitations, audit redaction, economy isolation, durable grants, offline reset/snapshot/restore and zero-player persistence passed')


class SessionContext(Session):
    def __enter__(self): return self
    def __exit__(self, *args): self.close()

if __name__ == '__main__':
    main()
