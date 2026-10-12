# Pack the two programs into a DS game image (rudolftest.nds) with a valid header checksum.
import struct,sys
a9=open('arm9.bin','rb').read();a7=open('arm7.bin','rb').read()
pad=lambda b,n:b+b'\0'*((-len(b))%n)
a9=pad(a9,0x200);a7=pad(a7,0x200)
h=bytearray(0x4000)
h[0:12]=b'RUDOLF TEST\0';h[12:16]=b'RTST';h[16:18]=b'01'
o9=0x4000;o7=o9+len(a9)
struct.pack_into('<IIII',h,0x20,o9,0x02000000,0x02000000,len(a9))
struct.pack_into('<IIII',h,0x30,o7,0x037F8000,0x037F8000,len(a7))
struct.pack_into('<I',h,0x80,o7+len(a7));struct.pack_into('<I',h,0x84,0x4000)
def crc16(d):
    c=0xFFFF
    for b in d:
        c^=b
        for _ in range(8):c=(c>>1)^0xA001 if c&1 else c>>1
    return c
struct.pack_into('<H',h,0x15C,0xCF56);struct.pack_into('<H',h,0x15E,crc16(h[:0x15E]))
open(sys.argv[1],'wb').write(bytes(h)+a9+a7);print('ok',len(h)+len(a9)+len(a7))
