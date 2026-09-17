#!/usr/bin/env python3
"""Install to an isolated prefix, use the shipped launcher, then reopen a save."""
import argparse
import os
import pathlib
import socket
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--build', required=True)
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='helion-install-') as directory:
    root = pathlib.Path(directory)
    prefix = root / 'installed'
    subprocess.run(['cmake', '--install', args.build, '--prefix', str(prefix)], check=True,
                   capture_output=True, timeout=30)
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        port = sock.getsockname()[1]
    env = dict(os.environ, HELION_DATA_DIR=str(root / 'save'), HELION_PORT=str(port))
    launcher = str(prefix / 'bin' / 'helion-play')
    check = subprocess.run([launcher, '--check'], env=env, capture_output=True, text=True, timeout=30)
    assert check.returncode == 0, check.stderr
    if 'WELCOME Helion/2' not in check.stdout or 'STATE profiles=0' not in check.stdout:
        raise AssertionError(
            'launcher --check output mismatch\n'
            f'return code: {check.returncode}\n'
            f'stdout:\n{check.stdout}\n'
            f'stderr:\n{check.stderr}\n'
            f'certificate.log: {root / "save" / "certificate.log"}\n'
            f'server.log: {root / "save" / "server.log"}'
        )
    for commands, expected in (
        ('/create installed_pilot synthetic-password Installed Pilot\n/profile\n/quit\n', 'OK CREATED user=installed_pilot'),
        ('/login installed_pilot synthetic-password\n/profile\n/quit\n', 'OK LOGIN user=installed_pilot'),
    ):
        result = subprocess.run([launcher, '--terminal'], env=env, input=commands,
                                capture_output=True, text=True, timeout=20)
        assert result.returncode == 0, result.stderr
        assert expected in result.stdout, result.stdout
        if expected.startswith('OK CREATED'):
            assert 'PROFILE user=installed_pilot' in result.stdout, result.stdout
        with socket.socket() as sock:
            assert sock.connect_ex(('127.0.0.1', port)) != 0, 'launcher left server running'
    for private in ('commander.db', 'tls/server.key', 'tls/server.crt'):
        assert ((root / 'save' / private).stat().st_mode & 0o777) == 0o600
    assert (prefix / 'share/applications/helion.desktop').exists()
    assert (prefix / 'share/icons/hicolor/scalable/apps/helion.svg').exists()
    print('installed launcher: first-run TLS, account creation, saved login, permissions, desktop files and shutdown passed')
