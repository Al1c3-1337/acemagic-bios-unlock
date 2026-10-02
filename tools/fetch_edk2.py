#!/usr/bin/env python3
"""Fetch only the pinned EDK II header tree using Git. MIT license."""
from pathlib import Path
import re
import subprocess

ROOT=Path(__file__).resolve().parents[1]
pin=(ROOT/'EDK2_COMMIT').read_text().strip()
if not re.fullmatch('[0-9a-f]{40}',pin): raise SystemExit('Invalid EDK2_COMMIT.')
repo=ROOT/'.deps/edk2'
if not repo.exists():
    repo.mkdir(parents=True)
    subprocess.run(['git','init',str(repo)],check=True)
    subprocess.run(['git','-C',str(repo),'remote','add','origin','https://github.com/tianocore/edk2.git'],check=True)
url=subprocess.check_output(['git','-C',str(repo),'remote','get-url','origin'],text=True).strip()
if url!='https://github.com/tianocore/edk2.git': raise SystemExit('Unexpected dependency remote; refusing to reuse it.')
subprocess.run(['git','-C',str(repo),'sparse-checkout','set','MdePkg/Include'],check=True)
subprocess.run(['git','-C',str(repo),'fetch','--depth=1','--filter=blob:none','origin',pin],check=True)
subprocess.run(['git','-C',str(repo),'checkout','--detach',pin],check=True)
actual=subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip()
if actual!=pin: raise SystemExit('Dependency revision mismatch.')
print('Pinned EDK II headers ready:',pin)
