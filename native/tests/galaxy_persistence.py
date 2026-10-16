#!/usr/bin/env python3
"""Live 28/90/200-system migration, corruption, and viewer-isolation checks."""
import argparse
import difflib
import pathlib
import socket
import ssl
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from tls_integration import certificate, free_port, start_server, stop


SCHEMA = "V\tGALAXY_KNOWLEDGE\t1\n"


def request(connection, command, marker):
    if command:
        connection.sendall(command.encode() + b'\n')
    received = b''
    while marker.encode() not in received:
        chunk = connection.recv(4096)
        assert chunk, 'server closed query early'
        received += chunk
        assert len(received) < 1024 * 1024, 'unbounded response'
    return received.decode()


def query(binary, root, cert, key, commands=()):
    process, port = start_server(binary, root, cert, key, database='galaxy.db')
    try:
        context = ssl.create_default_context(cafile=str(cert))
        with socket.create_connection(('127.0.0.1', port), timeout=5) as plain:
            with context.wrap_socket(plain, server_hostname='localhost') as connection:
                request(connection, '', 'INFO commands=')
                return [request(connection, command, marker) for command, marker in commands]
    finally:
        stop(process)


def rejected(binary, root, cert, key, content, expected, environment='development'):
    database = root / 'galaxy.db'
    database.write_text(content)
    result = subprocess.run([binary, str(free_port()), str(database), '--cert', str(cert),
                             '--key', str(key), '--environment', environment],
                            capture_output=True, text=True, timeout=10)
    assert result.returncode != 0 and expected in result.stderr, result.stderr
    assert database.read_text() == content, 'failed load changed the source save'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--server', required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='helion-galaxy-') as directory:
        root = pathlib.Path(directory)
        database = root / 'galaxy.db'
        cert, key = certificate(root, 'trusted', 'DNS:localhost,IP:127.0.0.1')
        query(args.server, root, cert, key, [
            ('CREATE surveyor synthetic-password Surveyor', 'OK CREATED'),
            ('CREATE observer synthetic-password Observer', 'OK CREATED')])
        baseline = database.read_text()
        baseline_rows = baseline.splitlines(keepends=True)
        physical = [line for line in baseline_rows if line.startswith('Y\t')]
        assert len(physical) == 200 and baseline.count('\nD\t') == 400
        assert SCHEMA in baseline
        # Seeded profiles are docked; no simulation action is issued in this test.
        preserved = [line for line in baseline_rows if not line.startswith(('Y\t', 'D\t', 'V\t', 'Z\t'))]
        for legacy_count in (28, 90):
            legacy_physical = physical[:legacy_count]
            legacy_physical = [line.replace('Y\tDARKWARD\t0\tUNCLAIMED\t\tUNKNOWN',
                                           'Y\tDARKWARD\t0\tPLAYER_ALLIANCE\talliance.survey\tCHARTED')
                               for line in legacy_physical]
            # Put Y records after profiles to prove order-independent knowledge migration.
            database.write_text(''.join(preserved + legacy_physical))
            responses = query(args.server, root, cert, key, [
                ('LOGIN surveyor synthetic-password', 'OK LOGIN'), ('GALAXY', 'GALAXY END')])
            assert 'sovereign-id=alliance.survey' in responses[-1]
            migrated = database.read_text()
            rows = migrated.splitlines(keepends=True)
            assert sum(line.startswith('Y\t') for line in rows) == 200
            assert sum(line.startswith('D\t') for line in rows) == 400
            assert 'D\tsurveyor\tDARKWARD\tSURVEYED\n' in rows
            actual_preserved = [line for line in rows if not line.startswith(('Y\t', 'D\t', 'V\t', 'Z\t'))]
            assert actual_preserved == preserved, ''.join(difflib.unified_diff(preserved, actual_preserved))
            query(args.server, root, cert, key)
            assert database.read_text() == migrated, 'migration ran again or changed unrelated state'
        # Persistent viewer knowledge is independent of physical/public charting.
        current = baseline.replace('D\tsurveyor\tDARKWARD\tUNDETECTED',
                                   'D\tsurveyor\tDARKWARD\tSURVEYED')
        current = current.replace('D\tobserver\tCONCORDIA\tSURVEYED',
                                  'D\tobserver\tCONCORDIA\tUNDETECTED')
        database.write_text(current)
        responses = query(args.server, root, cert, key, [
            ('LOGIN surveyor synthetic-password', 'OK LOGIN'), ('GALAXY', 'GALAXY END'),
            ('LOGIN observer synthetic-password', 'OK LOGIN'), ('GALAXY', 'GALAXY END')])
        assert 'SYSTEM id=DARKWARD' in responses[1]
        assert 'DARKWARD' not in responses[3], 'another profile received hidden system or gate data'
        assert 'CONCORDIA' not in responses[3] and 'Crownspire' not in responses[3], \
            'territory summary leaked an unknown capital'
        assert database.read_text() == current
        # Absence of an entire block is corruption once the schema marker exists.
        no_knowledge = ''.join(line for line in baseline_rows if not line.startswith('D\t'))
        rejected(args.server, root, cert, key, no_knowledge, 'missing or invalid persistence terminator')
        no_physical = ''.join(line for line in baseline_rows if not line.startswith('Y\t'))
        rejected(args.server, root, cert, key, no_physical, 'missing or invalid persistence terminator')
        first_knowledge = next(line for line in baseline_rows if line.startswith('D\t'))
        rejected(args.server, root, cert, key, baseline.replace(first_knowledge, first_knowledge * 2, 1),
                 'duplicate commander galaxy knowledge')
        rejected(args.server, root, cert, key,
                 baseline.replace(first_knowledge, first_knowledge.replace('SURVEYED', 'INVALID'), 1),
                 'malformed or duplicate commander galaxy knowledge')
        rejected(args.server, root, cert, key, baseline.replace(SCHEMA, 'V\tGALAXY_KNOWLEDGE\t2\n'),
                 'invalid or duplicate knowledge schema version')
        # Near-legacy truncations and a same-sized wrong prefix must not migrate.
        for count in (27, 29, 89, 91, 199):
            rejected(args.server, root, cert, key, ''.join(preserved + physical[:count]),
                     'incomplete galaxy persistence')
        rejected(args.server, root, cert, key, ''.join(preserved + physical[1:29]),
                 'incomplete galaxy persistence')
        rejected(args.server, root, cert, key, baseline.replace(physical[0], physical[0] * 2, 1),
                 'duplicate galaxy state')
        rejected(args.server, root, cert, key,
                 baseline.replace(physical[0], physical[0].rstrip('\n') + '\t\tgarbage\n', 1),
                 'malformed or duplicate galaxy state')
        rejected(args.server, root, cert, key,
                 baseline.replace(first_knowledge, first_knowledge.rstrip('\n') + '\t\tgarbage\n', 1),
                 'malformed or duplicate commander galaxy knowledge')
        rejected(args.server, root, cert, key, baseline, 'persistence environment mismatch', 'production')
        rejected(args.server, root, cert, key, baseline, 'persistence environment mismatch', 'test')
        erased_capital = baseline.replace('Y\tCONCORDIA\t25\tNPC_FACTION\tfaction.commonwealth\tCHARTED',
                                         'Y\tCONCORDIA\t0\tUNCLAIMED\t\tCHARTED')
        assert erased_capital != baseline
        rejected(args.server, root, cert, key, erased_capital, 'invalid persisted galaxy state')
        rejected(args.server, root, cert, key, ''.join(baseline_rows[:-1]),
                 'missing or invalid persistence terminator')
        missing_profile = ''.join(line for line in baseline_rows
                                  if not line.startswith(('H\tobserver\t', 'S\tobserver\t', 'D\tobserver\t')))
        rejected(args.server, root, cert, key, missing_profile, 'missing or invalid persistence terminator')
        # Current-format omissions must never invoke legacy ship/economy reconstruction.
        for prefix in ('S\t', 'K\t', 'T\t'):
            missing_records = ''.join(line for line in baseline_rows if not line.startswith(prefix))
            assert missing_records != baseline
            rejected(args.server, root, cert, key, missing_records,
                     'missing or invalid persistence terminator')
        rejected(args.server, root, cert, key, baseline + 'M\tsurveyor\ttrailing\n',
                 'records after persistence terminator')
        print('Galaxy migration 28/90 -> 200, idempotence, asset preservation, viewer isolation, corruption and environment tests passed')


if __name__ == '__main__':
    main()
