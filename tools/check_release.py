#!/usr/bin/env python3
"""Verify finished archives, privacy exclusions, links and version metadata. MIT."""
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import zipfile

ROOT=Path(__file__).resolve().parents[1]

def check_manifest(content,files):
    entries={}
    for line in content.decode('ascii').splitlines():
        sha,name=line.split('  ',1);assert name not in entries
        assert hashlib.sha256(files[name]).hexdigest()==sha,name
        entries[name]=sha
    assert set(entries)==set(files)

def main():
    version=(ROOT/'VERSION').read_text().strip();dist=ROOT/'dist'
    archives={f'rx16-toolkit-{version}-{kind}.zip':(dist/f'rx16-toolkit-{version}-{kind}.zip').read_bytes() for kind in ['source','usb']}
    check_manifest((dist/'SHA256SUMS.txt').read_bytes(),archives)
    for name in archives:
        with zipfile.ZipFile(dist/name) as z:
            assert z.testzip() is None
            assert len(z.namelist())==len(set(z.namelist()))
            files={p:z.read(p) for p in z.namelist()}
        internal='SOURCE_SHA256SUMS.txt' if '-source.' in name else 'SHA256SUMS.txt'
        checksum=files.pop(internal);check_manifest(checksum,files)
        for path,data in files.items():
            p=PurePosixPath(path)
            assert not p.is_absolute() and '..' not in p.parts
            assert not any(part in ['.git','.deps','private-fixtures','__pycache__'] for part in p.parts)
            assert p.suffix.lower() not in ['.rom','.cap','.fd','.dll','.so','.obj','.bin']
            assert not p.name.endswith('_LOG.txt')
            for private in ['C:\\Users\\','C:/Users/','/Users/']:
                # The checker itself contains generic path patterns, but never personal data.
                if path=='tools/check_release.py':continue
                assert private.lower().encode() not in data.lower(),path
                assert private.lower().encode('utf-16le') not in data.lower(),path
            if path.endswith('.md'):
                for target in re.findall(r'\]\(([^)]+)\)',data.decode()):
                    if '://' in target or target.startswith('#'):continue
                    relative=PurePosixPath(path).parent/target.split('#')[0]
                    assert relative.as_posix() in files,(path,target)
        if '-source.' in name:
            assert all(p in files for p in ['src/main.c','tools/build.py','.github/workflows/ci.yml','.github/workflows/release.yml'])
            assert not any(p.endswith('.efi') for p in files)
            for doc in ['README.md','START_HERE.txt','RELEASE_NOTES.md','CHANGELOG.md']:
                assert version in files[doc].decode(),doc
        else:
            binary=files['RX16Toolkit.efi'];assert binary==files['EFI/BOOT/BOOTX64.EFI']
            report=json.loads(files['TEST_RESULTS.json']);assert report['passed']
            assert report['efi_sha256']==hashlib.sha256(binary).hexdigest()
            assert not report['hardware_tested_combined_binary']
        print('Verified:',name,'files=',len(files)+1)
    pins=json.loads((ROOT/'tools/action-pins.json').read_text())
    for workflow in (ROOT/'.github/workflows').glob('*.yml'):
        text=workflow.read_text()
        assert 'pull_request_target' not in text
        for action,sha in re.findall(r'uses: actions/([\w-]+)@([0-9a-f]+)',text):
            assert pins[action]==sha and len(sha)==40
    print('PASS: archive checksums, identical EFI copies, exclusions, document links and metadata')

if __name__=='__main__':main()
