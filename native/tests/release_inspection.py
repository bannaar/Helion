#!/usr/bin/env python3
"""Installed manifest binding, channel and failure checks, without network access."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

parser=argparse.ArgumentParser()
parser.add_argument('--build',required=True)
parser.add_argument('--kind',required=True)
args=parser.parse_args()
checker=Path(__file__).resolve().parents[1]/'scripts/helion-release-check'
with tempfile.TemporaryDirectory(prefix='helion-release-inspect-') as name:
    for index, (flags, message) in enumerate((
            (['-DHELION_RELEASE_CHANNEL=ADMIN'], 'HELION_RELEASE_CHANNEL must be STABLE or TEST'),
            (['-DHELION_PACKAGE_FLAVOR=combined', '-DHELION_BUILD_CLIENT=OFF'],
             'combined packages require both server and client'),
            (['-DHELION_PACKAGE_FLAVOR=server'], 'server packages require'),
            (['-DHELION_PACKAGE_FLAVOR=client'], 'client packages require'))):
        configured = subprocess.run(['cmake', '-S', str(Path(__file__).resolve().parents[2]),
            '-B', str(Path(name)/f'invalid-{index}'), *flags], capture_output=True, text=True, timeout=30)
        assert configured.returncode != 0 and message in configured.stderr, configured.stderr
    prefix=Path(name)/'installed'
    subprocess.run(['cmake','--install',args.build,'--prefix',str(prefix)],check=True,capture_output=True)
    for installed in (prefix, *prefix.rglob('*')):
        assert not installed.stat().st_mode & 0o6022, installed
    path=prefix/'share/helion'/args.kind/'release-manifest.json'
    original=path.read_bytes();manifest=json.loads(original)
    def inspect(success=True,channel=None):
        command=[str(checker),'--prefix',str(prefix),'--kind',args.kind]
        if channel:command+=['--channel',channel]
        result=subprocess.run(command,capture_output=True,text=True,timeout=15)
        assert (result.returncode==0)==success,(result.stdout,result.stderr)
    inspect(channel=manifest['release_channel'])
    inspect(False,'TEST' if manifest['release_channel']=='STABLE' else 'STABLE')
    for field,value in [('manifest_version',99),('binary_protocol',99),('source_sha256','bad'),('release_channel','ADMIN')]:
        changed=dict(manifest,**{field:value});path.write_text(json.dumps(changed))
        inspect(False)
    path.write_bytes(original)
    path.chmod(0o666);inspect(False);path.chmod(0o644)
    component=manifest['components'][0];binary=prefix/'bin'/('helion_'+component)
    moved=binary.with_suffix('.saved');binary.rename(moved)
    inspect(False);moved.rename(binary)
    binary.chmod(0o777);inspect(False);binary.chmod(0o755)
    inspect()
    print('invalid build configurations, installed identity binding, required files, channels, versions, missing binary and unsafe permissions passed')
