#!/usr/bin/env python3
"""Installed TEST-first upgrade, migration failures and bounded lifecycle rehearsal."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import tempfile
import time
sys.dont_write_bytecode = True
from tls_integration import certificate, free_port
from test_admin_integration import Session
from binary_tls_integration import Peer
from operations_integration import await_condition, clean_diagnostics

PASSWORD = 'Synthetic-M8-pilot-password'


def run(command, success=True, **kwargs):
    kwargs.setdefault('cwd', '/')
    result = subprocess.run(command, capture_output=True, text=True, timeout=20, **kwargs)
    clean_diagnostics(result.stdout + result.stderr)
    assert (result.returncode == 0) == success, (result.returncode, result.stdout, result.stderr)
    return result


def food(session):
    snapshot = session.command('FLIGHT', b'ACTIVE SHIP')
    line = next(row for row in snapshot.splitlines() if row.startswith('FLIGHT '))
    return int(line.split()[11])


def rehearse(server, root, client=None, prior_server=None):
    metadata = json.loads(run([server, '--build-info']).stdout)
    verifier = run([server, '--hash-password'], input=PASSWORD+'\n').stdout.strip()
    counters = {'restarts': 0, 'protocols': 0, 'migration_rehearsals': 0, 'failure_cases': 0}
    library = root/'migration-fault.so'
    run(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-Werror','-shared','-fPIC',
         str(Path(__file__).with_name('migration_fault.cpp')),'-o',str(library),'-ldl'])
    dependencies = run(['ldd',str(server)]).stdout
    asan = re.search(r'libasan\.so\S*\s+=>\s+(\S+)',dependencies)
    preload = (asan.group(1)+':' if asan else '')+str(library)
    # STABLE candidates are rehearsed in real PRIVATE TEST before Production.
    for environment in ('test', 'production', 'development'):
        directory = root / environment
        directory.mkdir(mode=0o700)
        cert, key = certificate(directory, 'server', 'DNS:localhost,IP:127.0.0.1')
        cert.chmod(0o600)
        data = directory/'state.db'
        access = []
        if environment == 'test':
            roster = directory/'access'
            roster.write_text(f'HELION_ACCESS_V1 test release-qa\npilot INVITED_TESTER {verifier}\n')
            roster.chmod(0o600)
            access = ['--access-file', str(roster)]
        def command(binary=server):
            return [str(binary), str(free_port()), str(data), '--environment', environment,
                    '--cert', str(cert), '--key', str(key), *access]
        def offline(action=None, identity='upgrade', success=True):
            args = command()
            args += ['--operations', 'PRODUCTION', action, identity, '--reason', 'release-rehearsal'] if action else ['--validate-state']
            return run(args, success)
        def start(binary=server):
            args = command(binary)
            log = directory/f'process-{counters["restarts"]}.log'
            counters['restarts'] += 1
            with log.open('w') as stream:
                process = subprocess.Popen(args+['--maintenance-warning-seconds','1'], stdout=stream, stderr=stream, cwd=directory)
            def ready():
                try:
                    state = json.loads(Path(str(data)+'.status.json').read_text())
                    return state.get('pid') == process.pid and state.get('ready')
                except (OSError, ValueError): return False
            try:
                await_condition(lambda: process.poll() is not None or ready())
                assert process.poll() is None, log.read_text()
            except BaseException:
                if process.poll() is None: process.kill(); process.wait()
                raise
            return process, int(args[1]), log
        def stop(process, log, maintenance=False, expected=0):
            process.send_signal(signal.SIGUSR1 if maintenance else signal.SIGTERM)
            assert process.wait(timeout=10) == expected, log.read_text()
            clean_diagnostics(log.read_text())
            assert PASSWORD not in log.read_text()
        if metadata['release_channel'] == 'TEST' and environment == 'production':
            result = run(command(), False)
            assert 'incompatible release channel' in result.stderr
            assert not data.exists()
            counters['failure_cases'] += 1
            continue
        process, port, log = start(prior_server or server)
        try:
            p = Session(port, cert)
            assert 'OK CREATED' in p.command('CREATE pilot '+PASSWORD+' Pilot')
            assert 'OK MISSION ACCEPTED' in p.command('ACCEPT')
            assert 'mission-stage=1' in p.command('PROFILE')
            p.close()
        finally: stop(process, log)
        original = data.read_bytes()
        assert json.loads(offline().stdout)['migration_required'] is False
        assert data.read_bytes() == original
        if environment == 'production':
            # Current-format upgrade preserves exact bytes; backup is immutable.
            prepared = json.loads(offline('PREPARE_UPGRADE').stdout)
            assert prepared['source_sha256'] == hashlib.sha256(original).hexdigest()
            assert data.read_bytes() == original
            offline('PREPARE_UPGRADE', success=False)
            assert data.read_bytes() == original
            counters['failure_cases'] += 1
            # Failure after successful backup/migration finalization must not
            # emit prepared:true even though the backup itself is usable.
            fault_env = dict(os.environ,LD_PRELOAD=preload,
                HELION_TEST_MIGRATION_DESTINATION=str(data)+'.status.json',
                HELION_TEST_MIGRATION_AFTER_BACKUP=str(directory/'.helion-backups/finalization.snapshot'))
            failure = run(command()+['--operations','PRODUCTION','PREPARE_UPGRADE','finalization',
                                    '--reason','finalization-rehearsal'],False,env=fault_env)
            assert 'prepared' not in failure.stdout
            assert data.read_bytes() == original
            assert (directory/'.helion-backups/finalization.snapshot').is_file()
            counters['failure_cases'] += 1
        # A commander-only older record exercises credentials, owned-ship,
        # economy and galaxy initialization through the same loader.
        account = next(r for r in original.decode().splitlines() if r.startswith('H\t'))
        if environment == 'development':
            fields = account.split('\t'); fields[0] = 'P'; fields[2] = PASSWORD
            account = '\t'.join(fields)
            legacy_account = (account+'\n').encode()
        else:
            legacy_account = ('E\t'+environment+'\n'+account+'\n').encode()
        data.write_bytes(legacy_account)
        assert json.loads(offline().stdout)['migration_required'] is True
        assert data.read_bytes() == legacy_account
        if environment == 'production':
            offline('PREPARE_UPGRADE', 'legacy-account')
            assert (directory/'.helion-backups/legacy-account.snapshot').read_bytes().split(b'\n',1)[1] == legacy_account
        process, port, log = start()
        try:
            p = Session(port,cert)
            assert 'OK LOGIN' in p.command('LOGIN pilot '+PASSWORD)
            profile = p.command('PROFILE')
            assert 'credits=1500' in profile and 'owned-ships=1' in profile and 'mission-stage=1' in profile, profile
            assert food(p) == 0
            p.close()
        finally: stop(process,log)
        original = data.read_bytes()
        assert not any(r.startswith(b'P\t') for r in original.splitlines())
        assert sum(r.startswith(b'S\t') for r in original.splitlines()) == 1
        assert json.loads(offline().stdout)['migration_required'] is False
        counters['migration_rehearsals'] += 1
        for size in (28, 90):
            rows = original.decode().splitlines(keepends=True)
            preserved = [r for r in rows if not r.startswith(('Y\t','D\t','V\t','Z\t'))]
            legacy = ''.join(preserved+[r for r in rows if r.startswith('Y\t')][:size]).encode()
            data.write_bytes(legacy)
            assert json.loads(offline().stdout)['migration_required'] is True
            assert data.read_bytes() == legacy
            if environment == 'production':
                # A failed backup prevents even a valid migration.
                backup_dir = directory/'.helion-backups'
                moved = directory/'.saved-backups'
                backup_dir.rename(moved)
                backup_dir.write_text('blocked destination')
                try:
                    offline('PREPARE_UPGRADE', f'blocked-{size}', False)
                    assert data.read_bytes() == legacy
                    result = run(command(), False)  # Normal startup also cannot bypass backup.
                    assert data.read_bytes() == legacy
                finally:
                    backup_dir.unlink(); moved.rename(backup_dir)
                counters['failure_cases'] += 2
                fault_env = dict(os.environ,LD_PRELOAD=preload,HELION_TEST_MIGRATION_DESTINATION=str(data))
                failure = run(command()+['--operations','PRODUCTION','PREPARE_UPGRADE',f'failed-migration-{size}',
                                        '--reason','migration-fault-rehearsal'],False,env=fault_env)
                assert 'preparation failed; inspect state and retained backup' in failure.stderr
                assert data.read_bytes() == legacy
                assert (backup_dir/f'failed-migration-{size}.snapshot').read_bytes().split(b'\n',1)[1] == legacy
                journal = [json.loads(row) for row in Path(str(data)+'.operations.jsonl').read_text().splitlines()]
                assert any(row['backup']==f'failed-migration-{size}' and row['result']=='failed' for row in journal)
                counters['failure_cases'] += 1
                offline('PREPARE_UPGRADE', f'legacy-{size}')
                envelope = (backup_dir/f'legacy-{size}.snapshot').read_bytes()
                assert envelope.split(b'\n',1)[1] == legacy
            process, port, log = start()
            stop(process, log)
            current = data.read_bytes()
            assert sum(r.startswith(b'Y\t') for r in current.splitlines()) == 200
            assert sum(r.startswith(b'D\t') for r in current.splitlines()) == 200
            assert [r for r in current.decode().splitlines(keepends=True) if not r.startswith(('Y\t','D\t','V\t','Z\t'))] == preserved
            assert json.loads(offline().stdout)['migration_required'] is False
            process, port, log = start(); stop(process, log)
            assert data.read_bytes() == current, 'migration is not idempotent'
            counters['migration_rehearsals'] += 1
        if environment == 'production':
            # The ordinary startup path independently enforces successful backup
            # before migration, even when no upgrade CLI was requested.
            data.write_bytes(legacy)
            process, port, log = start(); stop(process,log)
            automatic = list((directory/'.helion-backups').glob('pre-migration-*.snapshot'))
            assert automatic and any(path.read_bytes().split(b'\n',1)[1] == legacy for path in automatic)
            assert json.loads(offline().stdout)['migration_required'] is False
            counters['migration_rehearsals'] += 1
        # Unsupported future/unknown required records and truncation fail closed.
        baseline = data.read_bytes()
        for invalid in (baseline.replace(b'GALAXY_KNOWLEDGE\t1',b'GALAXY_KNOWLEDGE\t99'),
                        b'E\t'+environment.encode()+b'\nREQUIRED_FUTURE\t2\n',
                        b'E\t'+environment.encode()+b'\nM\tPilot\tHello\tREQUIRED_FUTURE\n',
                        b'E\t'+environment.encode()+b'\nG\tEvent\tHeadline\tREQUIRED_FUTURE\n', baseline[:-30]):
            data.write_bytes(invalid)
            offline(success=False)
            assert data.read_bytes() == invalid
            if environment == 'production':
                offline('PREPARE_UPGRADE', 'invalid', False)
                assert data.read_bytes() == invalid
            counters['failure_cases'] += 1
        data.write_bytes(baseline)
        # Bounded repeat activity, reconnect, maintenance and restart through installed binaries.
        for cycle in range(3):
            process, port, log = start()
            try:
                baseline_fds = len(list(Path(f'/proc/{process.pid}/fd').iterdir()))
                if environment == 'production' and cycle == 0:
                    offline('PREPARE_UPGRADE','live-denied',False)
                    assert not (directory/'.helion-backups/live-denied.snapshot').exists()
                    assert process.poll() is None
                    counters['failure_cases'] += 1
                p = Session(port, cert)
                assert 'OK LOGIN' in p.command('LOGIN pilot '+PASSWORD)
                denial = 'ERR dev-admin-required' if environment == 'test' else 'ERR test-environment-required'
                assert denial in p.command('TESTADMIN TEST 100 ADD pilot gsc 1')
                price = 100 if environment == 'test' else 20
                assert f'OK BOUGHT food quantity=1 total={price}' in p.command('BUY food 1', b'ACTIVE SHIP')
                profile = p.command('PROFILE')
                assert f'credits={1500-(cycle+1)*price}' in profile, profile
                assert food(p) == cycle+1
                token = re.search(r'token=([^ ]+)',p.command('COMPANION ISSUE')).group(1)
                p.close()
                q = Session(port, cert)
                assert 'OK COMPANION AUTH' in q.command('COMPANION AUTH '+token)
                assert 'PROFILE user=pilot' in q.command('PROFILE')
                assert 'ERR scope-denied' in q.command('LAUNCH')
                q.close()
                b = Peer(port, cert); b.negotiate()
                b.send(1, ('LOGIN pilot '+PASSWORD).encode())
                assert b'OK LOGIN' in b.frame()[2]
                b.send(2,b'PROFILE'); assert b'PROFILE user=pilot' in b.frame()[2]
                b.close(); counters['protocols'] += 2
                if client:
                    answer = run([str(client), 'localhost', str(port), '--ca', str(cert), '--terminal'],
                                 input='/login pilot '+PASSWORD+'\n/profile\n/quit\n')
                    assert 'PROFILE user=pilot' in answer.stdout
                await_condition(lambda: json.loads(Path(str(data)+'.status.json').read_text())['connections'] == 0)
                assert len(list(Path(f'/proc/{process.pid}/fd').iterdir())) <= baseline_fds+1
            finally: stop(process, log, maintenance=cycle%2==0)
        process, port, log = start()
        try:
            p = Session(port, cert)
            assert 'OK LOGIN' in p.command('LOGIN pilot '+PASSWORD)
            assert food(p) == 3;p.close()
        finally:stop(process,log)
        if environment == 'production':
            # Post-migration listener failure is explicit, backup and state survive.
            import socket
            with socket.socket() as blocker:
                blocker.bind(('127.0.0.1',0)); blocker.listen()
                args = command(); args[1] = str(blocker.getsockname()[1])
                rows = data.read_text().splitlines(keepends=True)
                old_shape = ''.join([r for r in rows if not r.startswith(('Y\t','D\t','V\t','Z\t'))]+
                                    [r for r in rows if r.startswith('Y\t')][:90]).encode()
                data.write_bytes(old_shape)
                run(args,False)
                assert sum(r.startswith(b'Y\t') for r in data.read_bytes().splitlines()) == 200
                assert not json.loads(Path(str(data)+'.status.json').read_text())['ready']
                assert any(path.read_bytes().split(b'\n',1)[1] == old_shape for path in
                           (directory/'.helion-backups').glob('pre-migration-*.snapshot'))
                assert (directory/'.helion-backups/upgrade.snapshot').is_file()
            counters['failure_cases'] += 1
    print(json.dumps(counters,sort_keys=True))
    return counters


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--server',required=True)
    parser.add_argument('--client')
    parser.add_argument('--build')
    parser.add_argument('--prior-server')
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='helion-m8-rehearsal-') as name:
        root=Path(name)
        server,client=args.server,args.client
        if args.build:
            prefix=root/'installed'
            run(['cmake','--install',args.build,'--prefix',str(prefix)])
            server=str(prefix/'bin/helion_server')
            client=str(prefix/'bin/helion_client') if client else None
        rehearse(server,root,client,args.prior_server)


if __name__=='__main__':main()
