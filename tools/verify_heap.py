"""Check the actual ELF data, not just the presence of source macros."""
import struct,sys
from pathlib import Path
data=Path(sys.argv[1]).read_bytes()
assert data[:6]==b'\x7fELF\x01\x01','Expected little-endian ELF32'
shoff=struct.unpack_from('<I',data,32)[0]
stride,count=struct.unpack_from('<HH',data,46)
sections=[struct.unpack_from('<10I',data,shoff+i*stride) for i in range(count)]
found={}
for section in sections:
    if section[1]!=2:continue
    strings=sections[section[6]]
    for pos in range(section[4],section[4]+section[5],section[9]):
        name,value,size,info,other,index=struct.unpack_from('<IIIBBH',data,pos)
        start=strings[4]+name;end=data.index(b'\0',start)
        symbol=data[start:end].decode('ascii')
        if symbol not in ['sce_newlib_heap_threshold_kb_size','sce_newlib_heap_kb_size']:continue
        assert 0<index<len(sections),f'{symbol} must be defined'
        owner=sections[index]
        found[symbol]=struct.unpack_from('<I',data,owner[4]+value-owner[3])[0]
assert found.get('sce_newlib_heap_threshold_kb_size')==8192,found
assert found.get('sce_newlib_heap_kb_size')==0xffffffff,found
print('ELF heap verified: automatic sizing, explicit 8192 KiB reserve.')
