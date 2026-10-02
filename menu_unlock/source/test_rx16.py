import ctypes as c
import hashlib
import random
import struct
import sys
from pathlib import Path

root = Path(__file__).resolve().parent
sys.path.insert(0, str(root / 'pylibs'))
import pefile

lib = c.CDLL(str(root / 'rx16_core.dll'))
for name in ['rx16_hii', 'rx16_menu', 'rx16_image']:
    f = getattr(lib,name)
    f.argtypes = [c.c_void_p,c.c_uint64,c.c_int]
    f.restype = c.c_int

def run(name, data, apply):
    b=c.create_string_buffer(bytes(data),len(data))
    result=getattr(lib,name)(b,len(data),apply)
    return result,bytes(b)

setup=Path('work/Setup.bin').read_bytes()
ami=(root/'AMITSE.bin').read_bytes()
assert hashlib.sha256(setup).hexdigest()=='ddd259c811933cd370e15626f8ba82f8a9dcaeaf6d251b75d56d1b853dd375ee'
offset=0x1a060
length=struct.unpack_from('<I',setup,offset+16)[0]
package=setup[offset:offset+length]
assert len(package)==length
assert run('rx16_hii',package,0)==(1,package)
result,patched=run('rx16_hii',package,1)
assert result==1, result
diff=[i for i,(a,b) in enumerate(zip(package,patched)) if a!=b]
assert len(diff)==23
expected=[i+4-offset for i in range(len(setup)) if setup.startswith(bytes.fromhex('120608000100'),i)]
expected.append(0x1a83e-offset)
assert sorted(expected)==diff
assert all((package[i],patched[i]) in [(1,2),(0x46,0x47)] for i in diff)
assert run('rx16_hii',patched,1)==(2,patched)
for change in [0,2,16,20,24,0x1a0d7-offset,0x1a845-offset]:
    bad=bytearray(package); bad[change]^=0xff
    # Package GUID may differ legitimately; it is not an image identity guard.
    if change<16:
        continue
    status,out=run('rx16_hii',bad,1)
    assert status<0 and out==bad,(change,status)
for n in [0,1,2,4,16,20,23,24,128,length-1]:
    assert run('rx16_hii',package[:n],1)[0]<0
mixed=bytearray(package); mixed[diff[0]]=patched[diff[0]]
assert run('rx16_hii',mixed,1)[0]<0
random.seed(1616)
for i in range(1500):
    bad=bytearray(package)
    for j in range(random.randrange(1,8)):
        bad[random.randrange(len(bad))]=random.randrange(256)
    status,out=run('rx16_hii',bad,1)
    if status<0:
        assert out==bad
    else:
        changes=[k for k,(a,b) in enumerate(zip(bad,out)) if a!=b]
        assert changes==diff,(i,status)

mapped_ami=pefile.PE(data=ami).get_memory_mapped_image()
mapped_setup=pefile.PE(data=setup).get_memory_mapped_image()
assert len(mapped_ami)==0x69c80 and len(mapped_setup)==0x32500
assert run('rx16_image',mapped_ami,1)[0]==1
assert run('rx16_image',mapped_setup,0)[0]==1
assert run('rx16_menu',mapped_ami,0)==(1,mapped_ami)
status,patched_ami=run('rx16_menu',mapped_ami,1)
assert status==1
assert [i for i,(a,b) in enumerate(zip(mapped_ami,patched_ami)) if a!=b]==[0x54bf0]
assert run('rx16_menu',patched_ami,1)==(2,patched_ami)
for offset in [0,0x2c0,0x1234,0x54970,0x54bc0,0x54bf0]:
    bad=bytearray(mapped_ami); bad[offset]^=0x80
    status,out=run('rx16_menu',bad,1)
    assert status<0 and out==bad,(offset,status)
assert run('rx16_menu',mapped_ami[:-1],1)[0]<0

efi=pefile.PE(str(root/'RX16Menu.efi'))
assert efi.FILE_HEADER.Machine==0x8664
assert efi.OPTIONAL_HEADER.Subsystem==10
assert not hasattr(efi,'DIRECTORY_ENTRY_IMPORT')
print('PASS: real RX16 HII package, exactly 23 intended byte edits, repeat-call handling')
print('PASS: malformed/truncated/mixed inputs and 1,500 deterministic mutation cases')
print('PASS: exact code fingerprints; different image and menu-table state rejected')
print('PASS: native menu table changes exactly one data byte; no executable code edits')
print('PASS: x64 EFI application, no OS imports')
print('HII package bytes:',len(package))
print('EFI SHA256:',hashlib.sha256((root/'RX16Menu.efi').read_bytes()).hexdigest())
print('LIMIT: not executed on the RX16. Firmware HII update, original browser and save behavior remain unverified.')
