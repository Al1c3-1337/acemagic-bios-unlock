"""Minimal PE inspection for offline tests, no third-party Python packages. MIT."""
import struct

def inspect(data):
    assert data[:2]==b'MZ'
    pe=struct.unpack_from('<I',data,0x3c)[0]
    assert data[pe:pe+4]==b'PE\0\0'
    machine,sections=struct.unpack_from('<HH',data,pe+4)
    optional_size=struct.unpack_from('<H',data,pe+20)[0]
    opt=pe+24
    assert struct.unpack_from('<H',data,opt)[0]==0x20b
    image_size,header_size=struct.unpack_from('<II',data,opt+56)
    mapped=bytearray(image_size);mapped[:header_size]=data[:header_size]
    for i in range(sections):
        off=opt+optional_size+40*i
        vsize,rva,rawsize,raw=struct.unpack_from('<IIII',data,off+8)
        assert raw+rawsize<=len(data) and rva+rawsize<=image_size
        mapped[rva:rva+rawsize]=data[raw:raw+rawsize]
    return {'machine':machine,'subsystem':struct.unpack_from('<H',data,opt+68)[0],
        'imports':struct.unpack_from('<II',data,opt+120),
        'certificate':struct.unpack_from('<II',data,opt+144),'mapped':bytes(mapped)}
