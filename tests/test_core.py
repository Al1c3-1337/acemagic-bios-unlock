#!/usr/bin/env python3
"""Test both production modes without executing firmware services. MIT license."""
import argparse
import ctypes as c
import hashlib
import json
import os
from pathlib import Path
import random
import re
import struct
import sys
from pe import inspect

ROOT=Path(__file__).resolve().parents[1]
lib=c.CDLL(str(ROOT/'build'/('core.dll' if os.name=='nt' else 'core.so')))
lib.rx16_prepare.argtypes=[c.c_int,c.c_void_p,c.c_uint64,c.c_void_p,c.c_uint64,c.POINTER(c.c_uint64)]
lib.rx16_prepare.restype=c.c_int
lib.rx16_tables.argtypes=[c.c_int,c.c_void_p,c.c_uint64,c.c_int];lib.rx16_tables.restype=c.c_int
lib.rx16_image.argtypes=[c.c_void_p,c.c_uint64,c.c_int];lib.rx16_image.restype=c.c_int

def array(name,file):
    text=(ROOT/'src'/file).read_text()
    body=re.search(r'\b'+name+r'\[[^]]*\]\s*=\s*\{([^}]+)\}',text).group(1)
    return bytes(int(v.strip(),0) for v in body.split(',') if v.strip())

def fixture():
    # Construct a minimal valid public fixture instead of distributing a ROM.
    guid=array('setup_formset','rx16_core.h')
    formset=b'\x0e\x97'+guid+b'\0'*5
    store=array('setup_var','rx16_core.h')
    control=b'\x0a\x82\x46\x02'+array('hide_question','rx16_core.h')
    control+=bytes.fromhex('0907000000000029022902')
    gates=(bytes.fromhex('0a821206080001002902'))*22
    form=b'\x01\x86\x11\x27\0\0'+control+gates+b'\x29\x02'
    memory=array('memory_original','rx16_memory_form.h')
    forms=formset+store+form+memory+b'\x29\x02'
    pkg=struct.pack('<I',0x02000000|len(forms)+4)+forms
    return guid+struct.pack('<I',20+len(pkg)+4)+pkg+b'\x04\0\0\xdf'

def prepare(mode,data,capacity=None):
    src=c.create_string_buffer(data,len(data));cap=len(data)+16 if capacity is None else capacity
    out=c.create_string_buffer(b'\xa5'*cap,cap);size=c.c_uint64()
    result=lib.rx16_prepare(mode,src,len(data),out,cap,c.byref(size))
    assert bytes(src)==data
    return result,size.value,bytes(out)

def verify_package(data):
    assert len(data)==struct.unpack_from('<I',data,16)[0]
    pos=20;numeric={}
    while pos<len(data):
        length=int.from_bytes(data[pos:pos+3],'little');assert length>=4 and pos+length<=len(data)
        if data[pos+3]==2:
            q=pos+4;depth=0
            while q<pos+length:
                op=data[q];size=data[q+1]&127;scope=data[q+1]>>7
                assert size>=2 and q+size<=pos+length
                if op==7:
                    qid,store,offset=struct.unpack_from('<HHH',data,q+6)
                    if qid in [0x26,0x27]:
                        assert (store,offset)==(0xf101,0xb0)
                        assert data[q+12:q+14]==b'\x10\x11'
                        numeric[qid]=struct.unpack_from('<HHH',data,q+14)
                if op==0x29: depth-=1;assert depth>=0
                else: depth+=scope
                q+=size
            assert depth==0
        pos+=length
    return numeric

def exercise(original):
    verify_package(original)
    old=array('memory_original','rx16_memory_form.h');new=array('memory_replacement','rx16_memory_form.h')
    where=original.index(old)
    results={}
    for mode in [1,2]:
        r,size,out=prepare(mode,original);assert r==1
        patched=out[:size];assert out[size:]==b'\xa5'*(len(out)-size)
        results[mode]=patched
        nums=verify_package(patched)
        if mode==1:
            assert size==len(original) and len([1 for a,b in zip(original,patched) if a!=b])==23
            assert patched[where:where+len(old)]==old
            assert prepare(1,patched)[0]==2
        else:
            assert size==len(original)-18 and patched[where:where+len(new)]==new
            assert original[where+len(old):]==patched[where+len(new):]
            assert nums=={0x26:(3200,5600,200),0x27:(3200,4800,200)}
            assert prepare(2,patched)[0]==2
        r,_,bad=prepare(mode,original,size-1);assert r<0 and set(bad)=={0xa5}
        for n in [0,1,16,20,23,24,128,len(original)-1]:
            r,_,bad=prepare(mode,original[:n]);assert r<0 and set(bad)=={0xa5}
    # Memory mode must never stack over the active unlock transformation.
    r,_,bad=prepare(2,results[1]);assert r<0 and set(bad)=={0xa5}
    for i in range(len(old)):
        bad=bytearray(original);bad[where+i]^=0x80
        r,_,out=prepare(2,bytes(bad));assert r<0 and set(out)=={0xa5}
    for mode in [0,3,-1]:
        r,_,out=prepare(mode,original);assert r<0 and set(out)=={0xa5}
    random.seed(163200)
    for i in range(1500):
        bad=bytearray(original)
        for j in range(random.randrange(1,8)):bad[random.randrange(len(bad))]=random.randrange(256)
        for mode in [1,2]:
            r,sz,out=prepare(mode,bytes(bad))
            if r<0:assert set(out)=={0xa5}
            else:assert r in [1,2] and sz>0

def real_firmware(setup_path,ami_path):
    setup=setup_path.read_bytes();ami=ami_path.read_bytes()
    assert hashlib.sha256(setup).hexdigest()=='ddd259c811933cd370e15626f8ba82f8a9dcaeaf6d251b75d56d1b853dd375ee'
    length=struct.unpack_from('<I',setup,0x1a070)[0]
    exercise(setup[0x1a060:0x1a060+length])
    for data,which in [(setup,0),(ami,1)]:
        mapped=inspect(data)['mapped'];buf=c.create_string_buffer(mapped,len(mapped))
        assert lib.rx16_image(buf,len(mapped),which)==1
    mapped=inspect(ami)['mapped']
    for mode,offsets in [(1,[0x54bf0]),(2,[0x549a0,0x54bf0])]:
        buf=c.create_string_buffer(mapped,len(mapped))
        assert lib.rx16_tables(mode,buf,len(mapped),1)==1
        assert [i for i,(a,b) in enumerate(zip(mapped,bytes(buf))) if a!=b]==offsets
        assert lib.rx16_tables(mode,buf,len(mapped),0)==2
        assert lib.rx16_tables(3-mode,buf,len(mapped),1)<0
        assert lib.rx16_tables(mode,buf,len(mapped),-1)==2 and bytes(buf)==mapped
        for offset in [0,0x1234,0x549a0,0x54bf0]:
            bad=bytearray(mapped);bad[offset]^=0x80;buf=c.create_string_buffer(bytes(bad),len(bad))
            assert lib.rx16_tables(mode,buf,len(bad),1)<0 and bytes(buf)==bad
    mock=c.CDLL(str(ROOT/'build'/('mock.dll' if os.name=='nt' else 'mock.so')))
    mock.mock_run.argtypes=[c.c_void_p,c.c_uint64,c.c_void_p,c.c_uint64,c.c_void_p,c.c_uint64,c.c_char_p,c.c_int,c.POINTER(c.c_int)]
    mock.mock_run.restype=c.c_int
    setup_mapped=inspect(setup)['mapped'];hii=setup[0x1a060:0x1a060+length]
    for mode,form in [(b'U',0x2711),(b'M',0x279c)]:
        for sequence,failure,updates,opens,restored in [(mode+b'ORx',0,2,1,1),
            (mode+b'Qx',0,1,0,0),(b'DQx',0,0,0,1),
            (mode+b'x',1,2,0,1),(mode+b'x',2,2,0,1),(mode+b'x',3,2,0,1)]:
            sb=c.create_string_buffer(setup_mapped,len(setup_mapped));ab=c.create_string_buffer(mapped,len(mapped))
            hb=c.create_string_buffer(hii,len(hii));result=(c.c_int*6)()
            assert mock.mock_run(sb,len(sb),ab,len(ab),hb,len(hb),sequence,failure,result)==0
            assert list(result)==[updates,opens,form if opens else 0,0,0,restored],(sequence,failure,list(result))
            if restored:assert bytes(ab)==mapped
            assert bytes(sb)==setup_mapped
    print('PASS: actual app with mocked EFI services; both pages, diagnostics, exit, undo, partial-update failure, read-back corruption, owner mismatch; zero settings writes')

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--setup',type=Path);ap.add_argument('--amitse',type=Path)
    args=ap.parse_args()
    if bool(args.setup)!=bool(args.amitse):ap.error('Supply both --setup and --amitse.')
    exercise(fixture())
    # Fail closed for a foreign or truncated loaded image.
    for size in [0,16,4096,0x32500,0x69c80]:
        buf=c.create_string_buffer(size)
        for mode in [1,2]:assert lib.rx16_tables(mode,buf,size,1)<0
    if args.setup:real_firmware(args.setup,args.amitse)
    efi=(ROOT/'build/RX16Toolkit.efi').read_bytes();pe=inspect(efi)
    assert pe['machine']==0x8664 and pe['subsystem']==10
    assert pe['imports']==(0,0) and pe['certificate']==(0,0)
    report={'passed':True,'efi_sha256':hashlib.sha256(efi).hexdigest(),
        'public_fixture':True,'private_firmware':bool(args.setup),
        'simulated_firmware_services':bool(args.setup),
        'hardware_tested_combined_binary':False,
        'coverage':['both mode transformations','original storage bindings','input rejection',
                    '196 targeted mutations','1500 random mutations per fixture, both modes',
                    'x64 EFI format, no OS imports, unsigned']}
    (ROOT/'build/test-results.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS:',', '.join(report['coverage']))
    print('Private firmware checks:',report['private_firmware'])
    print('Hardware execution of combined binary: not tested')

if __name__=='__main__':main()
