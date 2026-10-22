#!/usr/bin/env python3
"""Deterministic freshness, liveness, readiness and private-file probe checks."""
import json
import os
import pathlib
import subprocess
import sys
import tempfile
import time

probe = pathlib.Path(__file__).resolve().parents[1] / 'scripts/helion-service-status'
with tempfile.TemporaryDirectory(prefix='helion-health-') as directory:
    data = pathlib.Path(directory) / 'universe.db'
    health = pathlib.Path(str(data) + '.status.json')

    def check(changes, code):
        value = dict(schema=1, pid=os.getpid(), timestamp=time.time(), ready=True,
                     durable=True, phase='ONLINE')
        value.update(changes)
        health.write_text(json.dumps(value))
        health.chmod(0o600)
        result = subprocess.run([sys.executable, str(probe), str(data)], capture_output=True, text=True, timeout=5)
        assert result.returncode == code, result.stdout + result.stderr
        return json.loads(result.stdout)

    assert check({}, 0)['process_alive'] is True
    dead = subprocess.Popen([sys.executable, '-c', 'pass'])
    dead.wait(timeout=5)
    assert check({'pid': dead.pid}, 1)['process_alive'] is False
    assert check({'timestamp': time.time() - 10}, 1)['fresh'] is False
    assert check({'timestamp': time.time() + 30}, 1)['fresh'] is False
    for phase in ('DRAINING', 'SHUTTING_DOWN', 'OFFLINE', 'FATAL', 'DEGRADED'):
        assert check({'phase': phase, 'ready': False}, 1)['ready'] is False
    assert check({'durable': False}, 1)['healthy'] is False
    check({'pid': -1}, 2)
    check({'schema': 99}, 2)
    check({}, 0)
    health.chmod(0o644)
    assert subprocess.run([sys.executable, str(probe), str(data)], capture_output=True).returncode == 2
    health.chmod(0o600)
    alias = pathlib.Path(directory) / 'alias'
    health.rename(alias)
    health.symlink_to(alias)
    assert subprocess.run([sys.executable, str(probe), str(data)], capture_output=True).returncode == 2
    health.unlink()
    os.link(alias, health)
    assert subprocess.run([sys.executable, str(probe), str(data)], capture_output=True).returncode == 2
    print('health probe readiness, freshness, liveness identity and private-file checks PASS')
