#!/usr/bin/env python3
"""M7 real-process Production lifecycle, failure, backup and boundary acceptance."""
import argparse
import hashlib
import json
import os
import pathlib
import re
import signal
import socket
import ssl
import subprocess
import sys
import tempfile
import time
sys.dont_write_bytecode = True
from tls_integration import certificate, free_port
from test_admin_integration import Session

PASSWORD = 'M7-production-secret-password'


def clean_diagnostics(output):
    if isinstance(output, bytes):
        output = output.decode(errors='replace')
    markers = ('AddressSanitizer', 'LeakSanitizer', 'UndefinedBehaviorSanitizer', 'runtime error:')
    assert not any(marker in output for marker in markers), 'sanitizer diagnostics in child process'


def await_condition(predicate, timeout=8):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(.025)
    raise AssertionError('condition deadline exceeded')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--server', required=True)
    parser.add_argument('--cxx', default='c++')
    args = parser.parse_args()
    server = str(pathlib.Path(args.server).resolve())
    probe = pathlib.Path(__file__).resolve().parents[1] / 'scripts/helion-service-status'
    with tempfile.TemporaryDirectory(prefix='helion-m7-') as directory:
        root = pathlib.Path(directory)
        prod = root / 'production'
        prod.mkdir(mode=0o700)
        cert, key = certificate(prod, 'server', 'DNS:localhost,IP:127.0.0.1')
        data = prod / 'universe.db'
        status_file = pathlib.Path(str(data) + '.status.json')
        logs = []
        running = []

        def command(target=data, environment='production'):
            return [server, str(free_port()), str(target), '--environment', environment,
                    '--cert', str(cert), '--key', str(key)]

        def status():
            try:
                return json.loads(status_file.read_text())
            except (FileNotFoundError, ValueError):
                return {}

        def start(extra_env=None):
            args = command() + ['--maintenance-warning-seconds', '1', '--drain-seconds', '1']
            log = prod / f'process-{len(logs)}.log'
            logs.append(log)
            stream = log.open('w')
            process = subprocess.Popen(args, stdout=stream, stderr=stream,
                                       env=dict(os.environ, **(extra_env or {})))
            stream.close()
            running.append(process)
            await_condition(lambda: process.poll() is not None or (
                status().get('pid') == process.pid and status().get('ready')))
            assert process.poll() is None, log.read_text()
            return process, int(args[1])

        def stop(process, expected=0):
            if process.poll() is None:
                process.send_signal(signal.SIGTERM)
            assert process.wait(timeout=8) == expected, logs[-1].read_text()

        def offline(action, identity='baseline', success=True, extras=()):
            result = subprocess.run(command() + ['--operations', 'PRODUCTION', action, identity,
                                                '--reason', 'm7-rehearsal', *extras],
                                    capture_output=True, text=True, timeout=8)
            clean_diagnostics(result.stderr)
            assert (result.returncode == 0) == success, (action, result.stdout, result.stderr)
            return result

        def fails(args):
            result = subprocess.run(args, capture_output=True, timeout=8)
            clean_diagnostics(result.stderr)
            assert result.returncode != 0, args

        try:
            # Invalid configuration never falls back or starts a listener.
            fails(command() + ['--environment', 'test'])
            fails(command(environment='unknown'))
            fails(command() + ['--max-clients', '0'])
            fails(command() + ['--drain-seconds', '6'])
            fails(command() + ['--bind', '127.0.0.1', '--bind', '0.0.0.0'])
            fails([server, str(free_port()), str(data), '--environment', 'production'])
            fails([server, str(free_port()), 'relative.db', '--environment', 'production',
                   '--cert', str(cert), '--key', str(key)])
            bad_key = prod / 'bad.key'
            bad_key.write_text('invalid-key')
            bad_key.chmod(0o600)
            fails([server, str(free_port()), str(data), '--environment', 'production',
                   '--cert', str(cert), '--key', str(bad_key)])
            key.chmod(0o644)
            fails(command())
            key.chmod(0o600)
            expired, expired_key = certificate(prod, 'expired', 'DNS:localhost', True)
            fails([server, str(free_port()), str(data), '--environment', 'production',
                   '--cert', str(expired), '--key', str(expired_key)])
            for invalid in ('', 'E\tproduction\nunknown-record\n', 'E\ttest\n', 'E\tproduction\nL\t99\t0\n',
                            'E\tproduction\nH\tpilot\t$invalid\tPilot\n'):
                data.write_text(invalid)
                before = data.read_bytes()
                fails(command())
                assert data.read_bytes() == before
            data.unlink()  # Disposable invalid fixture only.
            test = root / 'test'
            test.mkdir(mode=0o700)
            (test / '.helion-environment').write_text('HELION_STORAGE_V1 test qa convenience\n')
            (test / '.helion-environment').chmod(0o600)
            fails(command(target=test / 'fresh.db'))
            assert not (test / 'fresh.db').exists()
            nested = test / 'nested'
            nested.mkdir(mode=0o700)
            nested_key = nested / 'server.key'
            nested_key.write_bytes(key.read_bytes())
            nested_key.chmod(0o600)
            fails([server, str(free_port()), str(data), '--environment', 'production',
                   '--cert', str(cert), '--key', str(nested_key)])

            process, port = start()
            time.sleep(.6)
            assert process.poll() is None and status()['players'] == 0
            assert status()['ready'] and status()['durable']
            assert subprocess.run([sys.executable, str(probe), str(data)], capture_output=True).returncode == 0
            # Offline operations cannot run against a live universe lock.
            offline('BACKUP', success=False)
            legacy = Session(port, cert)
            assert 'OK CREATED' in legacy.command('CREATE pilot ' + PASSWORD + ' Pilot')
            assert 'OK BOUGHT' in legacy.command('BUY food 2', b'ACTIVE SHIP')
            assert 'test-environment-required' in legacy.command('TESTADMIN TEST 100 ADD pilot gsc 10')
            assert 'ERR' in legacy.command('TESTADMIN TEST 101 ADD pilot gsc 10 role=DEV_ADMIN')
            token = re.search(r'token=(\S+)', legacy.command('COMPANION ISSUE')).group(1)
            companion = Session(port, cert)
            assert 'OK COMPANION AUTH' in companion.command('COMPANION AUTH ' + token)
            assert 'test-environment-required' in companion.command('TESTADMIN TEST 102 ADD pilot gsc 10')
            assert 'scope-denied' in companion.command('LAUNCH')
            binary = Session(port, cert)
            binary.negotiate()
            binary.send(1, ('LOGIN pilot ' + PASSWORD).encode())
            assert b'OK LOGIN' in binary.frame()[2]
            wrong = Session(port, cert)
            assert 'invalid-login' in wrong.command('LOGIN pilot invalid-secret-password')
            wrong.close()
            await_condition(lambda: status().get('players') == 2 and status().get('auth_failures_total', 0) >= 1)
            assert status()['connections'] >= 3 and status()['saves_total'] >= 2
            assert status()['requests_total'] >= 10 and status()['last_save_timestamp'] > 0
            # A failed transaction rolls back, reports DEGRADED, refuses admission,
            # and a later durable save recovers readiness without resetting state.
            held = prod / 'transient-held.db'
            data.rename(held)
            data.mkdir()
            assert 'persistence-failed' in legacy.command('BUY food 1', b'ACTIVE SHIP')
            await_condition(lambda: status().get('phase') == 'DEGRADED')
            assert not status()['ready'] and status()['save_failures_total'] >= 1
            data.rmdir(); held.rename(data)
            assert 'OK BOUGHT' in legacy.command('BUY food 1', b'ACTIVE SHIP')
            assert 'OK SOLD' in legacy.command('SELL food 1', b'ACTIVE SHIP')
            await_condition(lambda: status().get('ready'))
            assert 'ERR' in legacy.command('UNKNOWN malformed-secret-sentinel')
            await_condition(lambda: status().get('malformed_total', 0) >= 1)
            # A half-complete handshake cannot hold shutdown indefinitely.
            stalled = socket.create_connection(('127.0.0.1', port), timeout=2)
            process.send_signal(signal.SIGUSR1)
            await_condition(lambda: status().get('phase') == 'MAINTENANCE_SOON')
            assert status()['ready']
            await_condition(lambda: status().get('phase') == 'DRAINING')
            assert not status()['ready']
            binary.send(2, b'PROFILE')
            assert binary.frame() == (2, 2, b'ERR service-draining')
            # Admission is refused before TLS, without a plaintext response.
            with socket.create_connection(('127.0.0.1', port), timeout=1) as new:
                new.settimeout(2)
                assert new.recv(32) == b''
            assert process.wait(timeout=8) == 0
            legacy.close(); companion.close(); binary.close(); stalled.close()
            assert status()['phase'] == 'OFFLINE' and not status()['ready']
            assert status()['backups_total'] == 1 and status()['last_backup_timestamp'] > 0
            assert subprocess.run([sys.executable, str(probe), str(data)], capture_output=True).returncode == 1
            saved = data.read_bytes()
            assert b'E\tproduction\n' == saved[:13] and len(re.findall(rb'^Y\t', saved, re.M)) == 200
            process, port = start()
            with_session = Session(port, cert)
            assert 'OK LOGIN' in with_session.command('LOGIN pilot ' + PASSWORD)
            profile = with_session.command('PROFILE')
            assert 'environment=production' in profile
            flight = with_session.command('FLIGHT', b'ACTIVE SHIP').splitlines()[0].split()
            assert int(flight[11]) == 2, flight
            with_session.close()
            stop(process)

            # Immutable Production backup; plans never overwrite authoritative bytes.
            before = data.read_bytes()
            offline('BACKUP')
            assert status()['backups_total'] == 1
            assert data.read_bytes() == before
            offline('BACKUP', success=False)
            offline('VERIFY_BACKUP')
            plan = offline('PLAN_RESTORE')
            assert json.loads(plan.stdout)['replaces_live_state'] is False
            assert data.read_bytes() == before
            backup = prod / '.helion-backups/baseline.snapshot'
            original = backup.read_bytes()
            assert original.startswith(b'HELION_BACKUP_V1 production local production-like baseline ')
            assert backup.stat().st_mode & 0o777 == 0o600
            for broken in (original[:-10], original + b'x', b'metadata-missing\n',
                           original.replace(b'production local', b'test local', 1),
                           original.replace(b'persistence-admin1', b'persistence-admin99', 1),
                           b'HELION_TEST_SNAPSHOT_V1 test local\n' + before):
                backup.write_bytes(broken)
                offline('VERIFY_BACKUP', success=False)
                offline('PLAN_RESTORE', success=False)
                assert data.read_bytes() == before
            # A correct checksum cannot make structurally invalid state compatible.
            header, payload = original.split(b'\n', 1)
            malformed = b'E\tproduction\nunknown-record\n'
            backup.write_bytes(header.rsplit(b' ', 1)[0] + b' ' + hashlib.sha256(malformed).hexdigest().encode() + b'\n' + malformed)
            offline('VERIFY_BACKUP', success=False)
            backup.write_bytes(original)
            data.write_text('damaged-live-state\n')
            offline('PLAN_RESTORE')
            assert data.read_text() == 'damaged-live-state\n'
            # Recovery rehearsal only in an isolated disposable directory, using
            # the verified payload; original live bytes remain separately preserved.
            rehearsal = root / 'rehearsal'
            rehearsal.mkdir(mode=0o700)
            (rehearsal / 'universe.db').write_bytes(payload)
            rehearsal_args = command(target=rehearsal / 'universe.db')
            recovery_process = subprocess.Popen(rehearsal_args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            running.append(recovery_process)
            recovered_status = rehearsal / 'universe.db.status.json'
            await_condition(lambda: recovered_status.exists() and json.loads(recovered_status.read_text()).get('ready'))
            recovered = Session(int(rehearsal_args[1]), cert)
            assert 'OK LOGIN' in recovered.command('LOGIN pilot ' + PASSWORD)
            flight = recovered.command('FLIGHT', b'ACTIVE SHIP').splitlines()[0].split()
            assert int(flight[11]) == 2, flight
            recovered.close()
            stop(recovery_process)
            data.write_bytes(before)  # Restore the deliberately corrupted fixture.

            # Final save failure exits nonzero; a backup failure also fails shutdown.
            process, _ = start()
            hold = prod / 'held.db'
            data.rename(hold)
            data.mkdir()
            stop(process, expected=1)
            assert status()['phase'] == 'FATAL' and status()['save_failures_total'] >= 1
            data.rmdir(); hold.rename(data)
            process, _ = start()
            (prod / '.helion-backups').chmod(0o755)
            stop(process, expected=1)
            assert status()['backup_failures_total'] == 1 and status()['phase'] == 'FATAL'
            (prod / '.helion-backups').chmod(0o700)
            # Deterministic post-rename directory fsync failure. Instrumentation
            # exists only in this separate test library, never in the server.
            library = root / 'fsync-fault.so'
            compiled = subprocess.run([args.cxx, '-std=c++17', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                                       '-shared', '-fPIC',
                                       str(pathlib.Path(__file__).with_name('fsync_fault.cpp')),
                                       '-o', str(library), '-ldl'], capture_output=True, text=True, timeout=30)
            assert compiled.returncode == 0, compiled.stderr
            dependencies = subprocess.run(['ldd', server], capture_output=True, text=True, check=True).stdout
            asan = re.search(r'libasan\.so\S*\s+=>\s+(\S+)', dependencies)
            # Keep the ASan runtime first; do not suppress runtime/link checks.
            preload = (asan.group(1) + ':' if asan else '') + str(library)
            trigger = root / 'fail-directory-sync'
            process, port = start({'LD_PRELOAD': preload, 'HELION_TEST_FSYNC_FAULT': str(trigger)})
            uncertain = Session(port, cert)
            assert 'OK LOGIN' in uncertain.command('LOGIN pilot ' + PASSWORD)
            old_credits = int(re.search(r'credits=(\d+)', uncertain.command('PROFILE')).group(1))
            trigger.write_text('enable test fault\n')
            uncertain.connection.sendall(b'BUY food 1\n')
            output = b''
            try:
                while True:
                    chunk = uncertain.connection.recv(4096)
                    if not chunk:
                        break
                    output += chunk
            except (ConnectionResetError, ssl.SSLEOFError):
                pass
            assert b'OK BOUGHT' not in output, 'uncertain commit acknowledged as successful'
            assert process.wait(timeout=8) == 1
            uncertain.close()
            assert status()['phase'] == 'FATAL' and not status()['durable']
            committed = next(line for line in data.read_text().splitlines() if line.startswith('H\tpilot\t')).split('\t')
            assert int(committed[6]) == old_credits - 20, 'committed rename was rolled back only in memory'
            text = '\n'.join(log.read_text() for log in logs)
            clean_diagnostics(text)
            structured = [json.loads(line) for line in text.splitlines() if line.startswith('{')]
            assert any(row['event'] == 'startup' for row in structured)
            assert any(row['event'] == 'lifecycle' and row['result'] == 'MAINTENANCE_SOON' for row in structured)
            assert any(row['event'] == 'lifecycle' and row['result'] == 'DRAINING' for row in structured)
            assert any(row['event'] == 'shutdown' and row['result'] == 'complete' for row in structured)
            assert any(row['event'] == 'shutdown' and row['result'] == 'failed' for row in structured)
            assert PASSWORD not in text and token not in text and 'invalid-secret-password' not in text
            assert 'malformed-secret-sentinel' not in text
            audit = pathlib.Path(str(data) + '.operations.jsonl').read_text()
            assert PASSWORD not in audit and token not in audit
            print('M7 Production startup, health/metrics, maintenance/drain, restart, backups, verification, rehearsal, failure and redaction PASS')
        finally:
            for process in running:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=8)


if __name__ == '__main__':
    main()
