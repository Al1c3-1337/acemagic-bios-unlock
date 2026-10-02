#!/usr/bin/env python3
"""Create verified source and USB release archives from an allowlist. MIT."""
import hashlib
import json
from pathlib import Path
import re
import zipfile

ROOT=Path(__file__).resolve().parents[1]
ROOT_FILES=['README.md','START_HERE.txt','LICENSE','THIRD_PARTY_NOTICES.md',
            'CHANGELOG.md','RELEASE_NOTES.md','CONTRIBUTING.md','VERSION','EDK2_COMMIT',
            '.gitignore','.gitattributes']

def digest(data):return hashlib.sha256(data).hexdigest()

def public_files():
    paths=[ROOT/p for p in ROOT_FILES]
    for glob in ['src/*.c','src/*.h','tests/*.c','tests/*.py','tools/*.py','tools/*.json',
                 'docs/*.md','docs/EDK2-LICENSE.txt','.github/workflows/*.yml','.github/ISSUE_TEMPLATE/*.yml']:
        paths.extend(ROOT.glob(glob))
    return {p.relative_to(ROOT).as_posix():p.read_bytes() for p in sorted(set(paths))}

def manifest(files):
    return ''.join(f'{digest(data)}  {name}\n' for name,data in sorted(files.items())).encode('ascii')

def archive(path,files):
    with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for name,data in sorted(files.items()):
            info=zipfile.ZipInfo(name,date_time=(1980,1,1,0,0,0))
            info.compress_type=zipfile.ZIP_DEFLATED;info.create_system=3;info.external_attr=0o100644<<16
            z.writestr(info,data,compress_type=zipfile.ZIP_DEFLATED,compresslevel=9)
    with zipfile.ZipFile(path) as z:
        if z.testzip() is not None:raise SystemExit('Corrupt archive: '+path.name)
        for name,data in files.items():assert z.read(name)==data

def main():
    build=ROOT/'build';dist=ROOT/'dist';dist.mkdir(exist_ok=True)
    meta=json.loads((build/'build.json').read_text())
    tests=json.loads((build/'test-results.json').read_text())
    efi=(build/'RX16Toolkit.efi').read_bytes();version=(ROOT/'VERSION').read_text().strip()
    if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+(?:-[A-Za-z0-9.]+)?',version):raise SystemExit('Invalid VERSION.')
    if not tests.get('passed') or tests['efi_sha256']!=digest(efi) or meta['efi_sha256']!=digest(efi):
        raise SystemExit('Build and passing test report must match the exact EFI binary.')
    if meta['version']!=version:raise SystemExit('VERSION changed; rebuild first.')
    for name,expected in meta['inputs'].items():
        if digest((ROOT/name).read_bytes())!=expected:raise SystemExit('Build input changed: '+name)
    source=public_files()
    source['SOURCE_SHA256SUMS.txt']=manifest(source)
    usb={name:data for name,data in source.items() if name in
         ['README.md','START_HERE.txt','LICENSE','THIRD_PARTY_NOTICES.md','CHANGELOG.md','RELEASE_NOTES.md'] or name.startswith('docs/')}
    usb['RX16Toolkit.efi']=efi;usb['EFI/BOOT/BOOTX64.EFI']=efi
    usb['BUILD_INFO.json']=(build/'build.json').read_bytes()
    usb['TEST_RESULTS.json']=(build/'test-results.json').read_bytes()
    usb['SHA256SUMS.txt']=manifest(usb)
    products={f'rx16-toolkit-{version}-usb.zip':usb,f'rx16-toolkit-{version}-source.zip':source}
    for name,files in products.items():archive(dist/name,files)
    (dist/'SHA256SUMS.txt').write_bytes(manifest({name:(dist/name).read_bytes() for name in products}))
    print('Packaged:',', '.join(products),'and SHA256SUMS.txt')

if __name__=='__main__':main()
