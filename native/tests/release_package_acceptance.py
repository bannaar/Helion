#!/usr/bin/env python3
"""Audit produced packages before extraction; rehearse installed TEST/Production upgrades."""
import argparse
import difflib
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import runpy
import subprocess
import tarfile
import tempfile
import sys
sys.dont_write_bytecode = True
from release_rehearsal import rehearse, run

SECRET_PATTERNS = (
    rb'-----BEGIN [A-Z ]*PRIVATE KEY-----',
    rb'\$scrypt\$\d+\$\d+\$\d+\$[0-9a-f]{32}\$[0-9a-f]{64}',
    rb'\$sha256\$[0-9a-f]{64}',
    rb'hc1\.[0-9a-f]{16}\.[0-9a-f]{64}',
)


def state_difference(before, after):
    def redacted(data):
        rows=[]
        for line in data.decode().splitlines():
            fields=line.split('\t')
            if fields[0] in ('H','P'):fields[2]='<credential>'
            if fields[0]=='T' and len(fields)==6:fields=['T','<companion-record>']
            rows.append('\t'.join(fields))
        return rows
    return ('record_multiset_equal='+str(sorted(before.splitlines())==sorted(after.splitlines()))+'\n'+
            '\n'.join(list(difflib.unified_diff(redacted(before),redacted(after)))[:50]))


def compatibility_records(payload):
    # M7 stores companion tokens in an unordered_map. Its iteration order is
    # not persistence identity; every byte of each token record still matters.
    tokens=[];ordered=[]
    for row in payload.splitlines():
        (tokens if row.startswith(b'T\t') and len(row.split(b'\t'))==6 else ordered).append(row)
    return ordered,sorted(tokens)


def unpack(package, destination):
    content = (subprocess.run(['dpkg-deb','--fsys-tarfile',str(package)],check=True,capture_output=True).stdout
               if package.suffix=='.deb' else package.read_bytes())
    with tarfile.open(fileobj=io.BytesIO(content),mode='r:*') as archive:
        members=archive.getmembers()
        for member in members:
            path=PurePosixPath(member.name)
            assert not path.is_absolute() and '..' not in path.parts, member.name
            assert member.isdir() or member.isfile(), member.name
            assert not member.mode & 0o6022, member.name
            assert path.suffix not in ('.key','.pem','.crt','.db','.snapshot','.bundle'),member.name
            assert path.name not in ('.env','access','access.conf','commander.db'),member.name
            if member.isfile():
                assert member.size<=256*1024*1024
                body=archive.extractfile(member).read()
                assert not any(re.search(pattern,body) for pattern in SECRET_PATTERNS),member.name
        for member in members:
            target=destination/member.name
            if member.isdir():
                target.mkdir(parents=True,exist_ok=True);target.chmod(member.mode)
            else:
                target.parent.mkdir(parents=True,exist_ok=True)
                target.write_bytes(archive.extractfile(member).read());target.chmod(member.mode)
    return hashlib.sha256(package.read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--artifact',action='append',required=True,type=Path)
    parser.add_argument('--previous-server-package',type=Path)
    parser.add_argument('--previous-server-deb',type=Path,
                        help='optional trusted prior DEB for an isolated real dpkg version upgrade/removal')
    parser.add_argument('--report',required=True,type=Path)
    args=parser.parse_args()
    checker=runpy.run_path(str(Path(__file__).resolve().parents[1]/'scripts/helion-release-check'))['inspect']
    report={'artifacts':[], 'previous_package':None}
    with tempfile.TemporaryDirectory(prefix='helion-m8-packages-') as name:
        root=Path(name);installed={};test_server=None
        for i,package in enumerate(args.artifact):
            dest=root/str(i);dest.mkdir()
            sha=unpack(package,dest);prefix=dest/'usr'
            manifests=list((prefix/'share/helion').glob('*/release-manifest.json'))
            assert len(manifests)==1
            manifest=json.loads(manifests[0].read_text());kind=manifest['package_kind']
            assert checker(prefix,kind,manifest['release_channel'])==manifest
            for component in manifest['components']:
                dependencies=run(['ldd',str(prefix/'bin'/('helion_'+component))]).stdout.lower()
                assert 'not found' not in dependencies
                if component=='server':
                    assert not any(x in dependencies for x in ('libsdl','libgl','libx11'))
            if package.suffix=='.deb':
                package_name=run(['dpkg-deb','-f',str(package),'Package']).stdout.strip()
                assert package_name==('helion' if kind=='combined' else 'helion-'+kind)
                version=run(['dpkg-deb','-f',str(package),'Version']).stdout.strip()
                assert version==manifest['version']+('~test' if manifest['release_channel']=='TEST' else '')
                depends=run(['dpkg-deb','-f',str(package),'Depends']).stdout.lower()
                if kind=='server':assert 'sdl' not in depends and 'libgl' not in depends
                if kind=='client':assert 'sdl' in depends and 'libgl' in depends
                conflict=run(['dpkg-deb','-f',str(package),'Conflicts']).stdout
                assert ('helion-server' if kind=='combined' else 'helion') in conflict
            if kind=='combined':
                env=dict(os.environ,HELION_DATA_DIR=str(root/f'local-{i}'),HELION_PORT='0')
                from tls_integration import free_port
                env['HELION_PORT']=str(free_port())
                answer=run([str(prefix/'bin/helion-play'),'--check'],env=env)
                assert 'STATE profiles=0' in answer.stdout
            if kind=='server' and manifest['release_channel']=='TEST':test_server=prefix/'bin/helion_server'
            else:installed.setdefault(kind,prefix)
            report['artifacts'].append({'file':str(package),'sha256':sha,'kind':kind,
                                        'build_id':manifest['build_id'],'channel':manifest['release_channel']})
        assert set(installed)=={'server','client','combined'}
        server=installed['server']/'bin/helion_server';client=installed['client']/'bin/helion_client'
        prior=None
        if args.previous_server_package:
            dest=root/'previous';dest.mkdir();sha=unpack(args.previous_server_package,dest)
            prior=dest/'usr/bin/helion_server'
            assert prior.is_file()
            report['previous_package']={'file':str(args.previous_server_package),'sha256':sha}
        state=root/'rehearsal';state.mkdir()
        report['rehearsal']=rehearse(str(server),state,str(client),str(prior) if prior else None)
        if prior:
            # Tested M7 -> M8 compatibility allows the known M7 binary to load
            # current state unchanged. No reverse migration or file replacement.
            from tls_integration import free_port
            directory=state/'production';data=directory/'state.db'
            before=data.read_bytes()
            log=root/'prior-rollback.log'
            with log.open('w') as output:
                process=subprocess.Popen([str(prior),str(free_port()),str(data),'--environment','production',
                    '--cert',str(directory/'server.crt'),'--key',str(directory/'server.key')],
                    stdout=output,stderr=subprocess.STDOUT)
            import signal,time
            try:
                deadline=time.monotonic()+8
                while time.monotonic()<deadline:
                    if process.poll() is not None:raise AssertionError('M7 binary rollback failed: '+log.read_text())
                    status=json.loads(Path(str(data)+'.status.json').read_text())
                    if status.get('pid')==process.pid and status.get('ready'):break
                    time.sleep(.05)
                else:raise AssertionError('M7 rollback readiness timeout: '+log.read_text())
                process.send_signal(signal.SIGTERM)
                assert process.wait(timeout=10)==0,log.read_text()
            finally:
                if process.poll() is None:process.kill();process.wait()
            after=data.read_bytes()
            assert compatibility_records(after)==compatibility_records(before),state_difference(before,after)
            run([str(server),'4242',str(data),'--environment','production',
                 '--cert',str(directory/'server.crt'),'--key',str(directory/'server.key'),'--validate-state'])
            assert data.read_bytes()==after
            report['known_M7_binary_rollback']='PASS'
            report['rollback_state_check']='all ordered records and exact companion-record multiset preserved; current loader verified'
        if args.previous_server_deb:
            audited=root/'prior-deb-audit';audited.mkdir()
            prior_hash=unpack(args.previous_server_deb,audited)
            candidate=next(Path(item['file']) for item in report['artifacts']
                if item['kind']=='server' and item['channel']=='STABLE' and item['file'].endswith('.deb'))
            previous_version=run(['dpkg-deb','-f',str(args.previous_server_deb),'Version']).stdout.strip()
            next_version=run(['dpkg-deb','-f',str(candidate),'Version']).stdout.strip()
            run(['dpkg','--compare-versions',previous_version,'lt',next_version])
            install_root=root/'dpkg-upgrade';install_root.mkdir()
            data=state/'production/state.db';saved=data.read_bytes()
            retained=install_root/'var/lib/helion/state.db';retained.parent.mkdir(parents=True)
            retained.write_bytes(saved);retained.chmod(0o600)
            base=['dpkg','--root='+str(install_root),'--log='+str(root/'upgrade-dpkg.log'),'--force-not-root']
            def installed_version():
                return run(['dpkg-query','--admindir='+str(install_root/'var/lib/dpkg'),
                            '-W','-f=${Version}','helion-server']).stdout
            run(base+['--force-depends','-i',str(args.previous_server_deb)])
            assert installed_version()==previous_version
            run(base+['--force-depends','-i',str(candidate)])
            assert installed_version()==next_version
            checker(install_root/'usr','server','STABLE')
            assert retained.read_bytes()==saved and data.read_bytes()==saved
            run([str(install_root/'usr/bin/helion_server'),'4242',str(data),'--environment','production',
                 '--cert',str(data.parent/'server.crt'),'--key',str(data.parent/'server.key'),'--validate-state'])
            run(base+['-r','helion-server'])
            assert not (install_root/'usr/bin/helion_server').exists()
            assert retained.read_bytes()==saved and data.read_bytes()==saved
            report['dpkg_prior_upgrade']={'from':previous_version,'to':next_version,
                'prior_sha256':prior_hash,'state_preserved':True,'remove_preserved_state':True}
        if test_server:
            isolated=root/'test-channel';isolated.mkdir()
            report['test_channel_rehearsal']=rehearse(str(test_server),isolated)
        # Deliberately invalid archives are rejected before extraction.
        for payload in (b'invalid-package',):
            bad=root/'bad.tar.gz';bad.write_bytes(payload)
            try:unpack(bad,root/'bad');raise AssertionError('invalid package accepted')
            except tarfile.TarError:pass
        bad=root/'unsafe.tar.gz'
        with tarfile.open(bad,'w:gz') as archive:
            item=tarfile.TarInfo('../escape');item.size=1;archive.addfile(item,io.BytesIO(b'x'))
        try:unpack(bad,root/'bad')
        except AssertionError:pass
        else:raise AssertionError('unsafe path accepted')
        assert not (root/'escape').exists()
        report['invalid_package_rejection']='PASS'
        for index,secret in enumerate((b'-----BEGIN ENCRYPTED PRIVATE KEY-----',
                b'$scrypt$32768$8$1$'+b'a'*32+b'$'+b'b'*64,
                b'$sha256$'+b'a'*64,b'hc1.'+b'a'*16+b'.'+b'b'*64)):
            bad=root/f'secret-{index}.tar.gz'
            with tarfile.open(bad,'w:gz') as archive:
                item=tarfile.TarInfo('usr/share/leaked.txt');item.mode=0o644;item.size=len(secret)
                archive.addfile(item,io.BytesIO(secret))
            try:unpack(bad,root/f'secret-{index}')
            except AssertionError:pass
            else:raise AssertionError('secret-bearing package accepted')
        report['secret_fixture_rejection']='4/4 PASS'
        units=root/'units';units.mkdir()
        examples=installed['server']/'share/doc/helion-server/examples'
        for source in examples.iterdir():
            content=source.read_text().replace('/usr/bin/helion_server',str(server))
            (units/source.name).write_text(content)
        if Path('/usr/bin/systemd-analyze').exists():
            run(['systemd-analyze','verify',*[str(p) for p in units.iterdir()]])
            report['systemd_syntax']='PASS'
    args.report.write_text(json.dumps(report,indent=2)+'\n')
    print('all package contents, secrets, identity, dependencies, installed TLS/client, migration/backup and known rollback checks passed')


if __name__=='__main__':main()
