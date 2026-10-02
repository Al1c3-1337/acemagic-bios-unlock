import ctypes as c, hashlib, random, struct, sys
from pathlib import Path
root=Path(__file__).resolve().parent
sys.path.insert(0,str(root/'pylibs'))
import pefile
lib=c.CDLL(str(root/'rx16_memory_core.dll'))
lib.rx16_memory.argtypes=[c.c_void_p,c.c_uint64,c.c_void_p,c.c_uint64,c.POINTER(c.c_uint64)]
lib.rx16_memory.restype=c.c_int
lib.rx16_memory_menu.argtypes=[c.c_void_p,c.c_uint64,c.c_int]
lib.rx16_memory_menu.restype=c.c_int
b=Path('work/Setup.bin').read_bytes(); start=0x1a060
n=struct.unpack_from('<I',b,start+16)[0]; original=b[start:start+n]
def transform(data,capacity=None):
    src=c.create_string_buffer(data,len(data)); cap=len(data)+16 if capacity is None else capacity
    dst=c.create_string_buffer(bytes([0xa5])*cap,cap); size=c.c_uint64()
    result=lib.rx16_memory(src,len(data),dst,cap,c.byref(size))
    assert bytes(src)==data
    return result,size.value,bytes(dst)
result,size,out=transform(original)
assert result==1 and size==n-18
patched=out[:size]; assert out[size:]==b'\xa5'*34
assert struct.unpack_from('<I',patched,16)[0]==size
# Compare at package boundaries so all unrelated package bytes are preserved.
def packages(data):
    pos=20; result=[]
    while pos<len(data):
        length=int.from_bytes(data[pos:pos+3],'little')
        assert length>=4 and pos+length<=len(data)
        result.append((pos,data[pos+3],data[pos:pos+length]));pos+=length
    assert pos==len(data)
    return result
before=packages(original);after=packages(patched)
assert len(before)==len(after)
changed=[]
for i,((p,t,v),(q,u,w)) in enumerate(zip(before,after)):
    assert t==u
    if v!=w: changed.append(i); assert t==2 and len(w)==len(v)-18
assert len(changed)==1
p,t,v=before[changed[0]];q,u,w=after[changed[0]]
formpos=0x1ae0d-start-p
assert v[4:formpos]==w[4:formpos]
assert v[formpos+196:]==w[formpos+178:]
memory=w[formpos:formpos+178]
# Independently walk scopes and decode questions. Default values and original
# bindings must be preserved; speed fields must be numeric, decimal and bounded.
depth=0;pos=0;questions={};current=None;defaults={};options={}
while pos<len(memory):
    op=memory[pos];length=memory[pos+1]&127;scope=memory[pos+1]>>7
    assert length>=2 and pos+length<=len(memory)
    rec=memory[pos:pos+length]
    if op in [5,7]:
        qid,store,offset=struct.unpack_from('<HHH',rec,6)
        questions[qid]=(op,store,offset,rec[12:]);current=qid
    if op==9: options.setdefault(current,[]).append(rec)
    if op==0x5b: defaults[current]=struct.unpack_from('<H',rec,5)[0]
    if op==0x29: depth-=1; assert depth>=0
    else: depth+=scope
    pos+=length
assert depth==0
assert set(questions)=={0x25,0x26,0x27}
assert questions[0x25][:3]==(5,0xf101,0xaf)
assert [x[-1] for x in options[0x25]]==[255,1]
for qid,maximum in [(0x26,5600),(0x27,4800)]:
    op,store,offset,fields=questions[qid]
    assert (op,store,offset)==(7,0xf101,0xb0)
    assert fields[:2]==b'\x10\x11'
    assert struct.unpack_from('<HHH',fields,2)==(3200,maximum,200)
    assert defaults[qid]==maximum and qid not in options
assert transform(patched)[0]==2
# Refuse insufficient destination space and any change to the exact RAM form.
r,sz,dst=transform(original,n-19);assert r<0 and dst==b'\xa5'*(n-19)
for i in range(196):
    bad=bytearray(original);bad[0x1ae0d-start+i]^=0x80
    r,sz,dst=transform(bytes(bad));assert r<0 and dst==b'\xa5'*len(dst),(i,r)
for i in [0,1,2,16,20,23,24,128,n-1]:
    r,sz,dst=transform(original[:i]);assert r<0 and dst==b'\xa5'*len(dst)
random.seed(3200)
for i in range(1000):
    bad=bytearray(original)
    for j in range(random.randrange(1,8)): bad[random.randrange(n)]=random.randrange(256)
    r,sz,dst=transform(bytes(bad))
    if r<0: assert dst==b'\xa5'*len(dst)
    else: assert r==1 and sz==n-18
ami=pefile.PE(str(root/'AMITSE.bin')).get_memory_mapped_image()
buf=c.create_string_buffer(ami,len(ami))
assert lib.rx16_memory_menu(buf,len(ami),1)==1
assert [i for i,(x,y) in enumerate(zip(ami,bytes(buf))) if x!=y]==[0x549a0,0x54bf0]
assert lib.rx16_memory_menu(buf,len(ami),0)==2
assert lib.rx16_memory_menu(buf,len(ami),-1)==2 and bytes(buf)==ami
for offset in [0,0x1234,0x549a0,0x54bf0]:
    bad=bytearray(ami);bad[offset]^=0x80;buf=c.create_string_buffer(bytes(bad),len(bad))
    assert lib.rx16_memory_menu(buf,len(bad),1)<0 and bytes(buf)==bad
efi=pefile.PE(str(root/'RX16Memory.efi'))
assert efi.FILE_HEADER.Machine==0x8664 and efi.OPTIONAL_HEADER.Subsystem==10
assert not hasattr(efi,'DIRECTORY_ENTRY_IMPORT')
# Artifact for review with IFRExtractor, not a flash image.
(root/'memory_test_package.hii').write_bytes(patched)
print('PASS: exact real firmware package; unrelated bytes and all variable bindings preserved')
print('PASS: two numeric speed questions, 3200 minimum, 200 step, original upper limits and defaults')
print('PASS: intact scopes; malformed, truncated, 196 form mutations and 1000 random cases')
print('PASS: two temporary menu-table data bytes; exact undo; mismatched firmware rejected')
print('PASS: x64 EFI application, no OS imports')
print('EFI SHA256:',hashlib.sha256((root/'RX16Memory.efi').read_bytes()).hexdigest())
print('LIMIT: offline verification only; hardware saving and underclock remain untested.')
