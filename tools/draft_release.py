#!/usr/bin/env python3
"""Create a draft release for an existing, matching version tag. MIT license."""
import os
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
version=(ROOT/'VERSION').read_text().strip()
tag=os.environ.get('GITHUB_REF_NAME','')
if tag!='v'+version:raise SystemExit('Tag must exactly match v + VERSION.')
if not os.environ.get('GITHUB_REPOSITORY'):raise SystemExit('Run inside the repository release workflow.')
subprocess.run(['python3',str(ROOT/'tools/check_release.py')],check=True,cwd=ROOT)
args=['gh','release','create',tag,'--verify-tag','--draft','--title',f'RX16 Toolkit {version}',
      '--notes-file',str(ROOT/'RELEASE_NOTES.md')]
if '-' in version:args.append('--prerelease')
args.extend(str(ROOT/'dist'/name) for name in [f'rx16-toolkit-{version}-usb.zip',f'rx16-toolkit-{version}-source.zip','SHA256SUMS.txt'])
subprocess.run(args,check=True,cwd=ROOT)
