#!/usr/bin/env python3
"""Deterministic offline binary/package compatibility identity."""
import argparse
import json
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--binary', required=True)
parser.add_argument('--component', required=True)
parser.add_argument('--manifest', required=True)
args = parser.parse_args()
manifest = json.loads(Path(args.manifest).read_text())
for flag in ('--build-info', '--version'):
    result = subprocess.run([args.binary, flag], capture_output=True, text=True, check=True, timeout=10)
    assert not result.stderr
    identity = json.loads(result.stdout)
    assert identity.pop('component') == args.component
    assert identity == manifest
assert re.fullmatch(r'[0-9a-f]{64}', manifest['source_sha256'])
assert re.fullmatch(r'[0-9a-f]{64}', manifest['configuration_sha256'])
assert manifest['release_channel'] in ('STABLE', 'TEST')
assert manifest['binary_protocol'] == 1 and manifest['legacy_protocol'] == 2
assert manifest['persistence_compatibility'] == 'persistence-admin1'
assert manifest['client_server_compatibility'] == 'helion-tls-legacy2-binary1'
assert manifest['reverse_migration'] is False
assert manifest['version'] == '0.2.1'
assert 'timestamp' not in manifest
print('headless machine-readable build/version/channel/compatibility identity passed')
