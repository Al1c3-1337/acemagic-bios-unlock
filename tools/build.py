#!/usr/bin/env python3
"""Build the EFI application and a native library for offline tests. MIT license."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]

def compiler(name):
    override=os.environ.get(name.upper().replace('-','_'))
    found=override or shutil.which(name)
    if not found and os.name=='nt':
        for base in [Path(os.environ.get('ProgramFiles','C:/Program Files')),
                     Path(os.environ.get('ProgramFiles(x86)','C:/Program Files (x86)'))]:
            matches=sorted(base.glob(f'Microsoft Visual Studio/*/*/VC/Tools/Llvm/x64/bin/{name}.exe'))
            if matches: found=str(matches[-1]); break
    if not found: raise SystemExit(f'{name} not found; install LLVM or set {name.upper().replace("-","_")}.')
    return str(found)

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def input_files():
    return sorted([*ROOT.joinpath('src').glob('*'),*ROOT.joinpath('tests').glob('*.c'),
                   *ROOT.joinpath('tests').glob('*.py'),ROOT/'VERSION',ROOT/'EDK2_COMMIT',ROOT/'tools/build.py'])

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--edk2-include',type=Path,default=ROOT/'.deps/edk2/MdePkg/Include')
    args=ap.parse_args()
    inc=args.edk2_include.resolve()
    if not (inc/'Uefi.h').is_file(): raise SystemExit('EDK II headers missing. Run python tools/fetch_edk2.py first.')
    version=(ROOT/'VERSION').read_text().strip()
    if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+(?:-[A-Za-z0-9.]+)?',version): raise SystemExit('Invalid VERSION.')
    out=ROOT/'build';out.mkdir(exist_ok=True)
    (out/'version.h').write_text(f'#define RX16_VERSION_W L"{version}"\n',encoding='ascii')
    clang,link=compiler('clang'),compiler('lld-link')
    flags=['-ffreestanding','-fno-stack-protector','-fno-builtin','-O1','-Wall','-Wextra','-Werror']
    subprocess.run([clang,'--target=x86_64-pc-win32-coff',*flags,'-fshort-wchar','-mno-red-zone',
        '-I',str(inc),'-I',str(inc/'X64'),'-I',str(out),'-c',str(ROOT/'src/main.c'),'-o',str(out/'main.obj')],check=True)
    subprocess.run([link,'/subsystem:efi_application','/entry:efi_main','/nodefaultlib','/machine:x64',
        '/timestamp:0',f'/out:{out / "RX16Toolkit.efi"}',str(out/'main.obj')],check=True)
    if os.name=='nt':
        subprocess.run([clang,'--target=x86_64-pc-win32-coff',*flags,'-x','c','-DRX16_HOST_TEST','-c',
            str(ROOT/'src/toolkit_core.h'),'-o',str(out/'core.obj')],check=True)
        subprocess.run([link,'/dll','/noentry','/nodefaultlib','/machine:x64','/timestamp:0',
            f'/out:{out / "core.dll"}',str(out/'core.obj')],check=True)
    else:
        subprocess.run([clang,*flags,'-x','c','-DRX16_HOST_TEST','-shared','-fPIC',
            str(ROOT/'src/toolkit_core.h'),'-o',str(out/'core.so')],check=True)
    mock_flags=[*flags,'-fshort-wchar','-DRX16_HOST_TEST','-I',str(inc),'-I',str(inc/'X64'),'-I',str(out)]
    if os.name=='nt':
        subprocess.run([clang,'--target=x86_64-pc-win32-coff',*mock_flags,'-c',
            str(ROOT/'tests/mock_firmware.c'),'-o',str(out/'mock.obj')],check=True)
        subprocess.run([link,'/dll','/noentry','/nodefaultlib','/machine:x64','/timestamp:0',
            f'/out:{out / "mock.dll"}',str(out/'mock.obj')],check=True)
    else:
        subprocess.run([clang,*mock_flags,'-shared','-fPIC',str(ROOT/'tests/mock_firmware.c'),
            '-o',str(out/'mock.so')],check=True)
    manifest={'version':version,'edk2_commit':(ROOT/'EDK2_COMMIT').read_text().strip(),
        'compiler':subprocess.check_output([clang,'--version'],text=True).splitlines()[0],
        'efi_sha256':digest(out/'RX16Toolkit.efi'),
        'inputs':{p.relative_to(ROOT).as_posix():digest(p) for p in input_files()}}
    (out/'build.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf8')
    print(f'Built RX16Toolkit.efi {version}: {manifest["efi_sha256"]}')

if __name__=='__main__': main()
