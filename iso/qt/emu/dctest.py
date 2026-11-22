#!/usr/bin/env python3
# Emu's Dreamcast test disc, made for Emu's emulator test (iso/qt/emu/nativerun_t.sh): no Sega code, BIOS or other
# material from Sega in it. GPL-2.0-or-later.
#
# The disc's IP.BIN (its first 16 sectors) is made here: the header a Dreamcast disc carries (its hardware ID text,
# "SEGA SEGAKATANA", is how a disc says it is one; every other field is Rudolf's own), and at 0x300, where a start-up
# runs a disc's own boot code (Flycast's HLE BIOS, reios: core/reios/reios.cpp, jumps to 0xAC008300), the program
# itself: a few SH-4 instructions, hand-assembled (checked against GNU binutils' disassembly by `dctest.py --check`).
# No Sega boot code: the program needs none, and IP.BIN is not KallistiOS's IP.TMPL either. 1ST_READ.BIN is on the
# disc (the start-up looks for it) but holds only a note.
#
# What it does: shows a 640x480 RGB565 frame (the PowerVR's framebuffer registers) and, in a loop, reads controller A
# over the Maple bus and fills the frame: RED (255,0,0) normally, GREEN (0,255,0) while A is held, BLUE (0,0,255) while
# Start is held.
#
#   dctest.py --gdi DIR/NAME.gdi    a GD-ROM image: NAME.gdi, track01.bin (data), track02.raw (audio), track03.bin
#   dctest.py --cdi FILE.cdi        a CD-R image (DiscJuggler 2 format): an audio session, then the data session
#   dctest.py --iso FILE.iso        only the data track (2048-byte sectors, at LBA 45000)
#   dctest.py --check               the encoder against sh4-linux-gnu-objdump (if installed)
# (a .chd: chdman createcd -i NAME.gdi -o NAME.chd)
import struct, sys, os, subprocess, tempfile

class SH4:   # just what the program uses; 16-bit little-endian instructions, PC-relative constants
    def __init__(s, base): s.base, s.code, s.labels, s.fix, s.pool = base, [], {}, [], []
    def pc(s): return s.base + 2 * len(s.code)
    def label(s, n): s.labels[n] = s.pc()
    def h(s, x): s.code.append(x & 0xFFFF)
    def const(s, n, v): s.pool.append((len(s.code), v)); s.h(0xD000 | n << 8)        # mov.l @(disp,PC),Rn
    def movi(s, n, i): s.h(0xE000 | n << 8 | (i & 0xFF))                              # mov #imm,Rn
    def st(s, m, d, n): s.h(0x1000 | n << 8 | m << 4 | d // 4)                         # mov.l Rm,@(disp,Rn)
    def stn(s, m, n): s.h(0x2002 | n << 8 | m << 4)                                    # mov.l Rm,@Rn
    def ld(s, m, n): s.h(0x6002 | n << 8 | m << 4)                                     # mov.l @Rm,Rn
    def ldw0(s, d, m): s.h(0x8500 | m << 4 | d // 2)                                   # mov.w @(disp,Rm),R0
    def addi(s, n, i): s.h(0x7000 | n << 8 | (i & 0xFF))                              # add #imm,Rn
    def dt(s, n): s.h(0x4010 | n << 8)                                                 # dt Rn
    def tst0(s, i): s.h(0xC800 | i)                                                    # tst #imm,R0
    def nop(s): s.h(0x0009)
    def bt(s, l): s.fix.append((len(s.code), l, 8)); s.h(0x8900)
    def bf(s, l): s.fix.append((len(s.code), l, 8)); s.h(0x8B00)
    def bra(s, l): s.fix.append((len(s.code), l, 12)); s.h(0xA000); s.nop()             # with its delay slot
    def put(s, addr, val): s.const(1, addr); s.const(0, val); s.stn(0, 1)              # [addr] <- val (r0, r1)
    def done(s):
        for at, l, bits in s.fix:
            d = (s.labels[l] - (s.base + 2 * at + 4)) // 2; assert -(1 << bits - 1) <= d < 1 << bits - 1
            s.code[at] |= d & ((1 << bits) - 1)
        if len(s.code) % 2: s.nop()
        lit = {}; words = []
        for _, v in s.pool:
            if v not in lit: lit[v] = len(words); words.append(v)
        pool0 = s.base + 2 * len(s.code)
        for at, v in s.pool:
            addr = pool0 + 4 * lit[v]; d = (addr - ((s.base + 2 * at) & ~3) - 4) // 4; assert 0 <= d < 256
            s.code[at] |= d
        return b''.join(struct.pack('<H', x) for x in s.code) + b''.join(struct.pack('<I', w) for w in words)

def rgb2(r, g, b): c = (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3); return c << 16 | c
RED, GREEN, BLUE = rgb2(255, 0, 0), rgb2(0, 255, 0), rgb2(0, 0, 255)
FB = 0x00200000                       # the frame's place in video memory
CMD, REPLY = 0xAC00A000, 0xAC00A100   # the Maple command and its reply, in main memory (uncached)

def program():
    a = SH4(0xAC008300)
    a.put(0xA05F8044, 0x00800005)     # FB_R_CTRL: on, RGB565, the pixel clock for 480 lines
    a.put(0xA05F8050, FB); a.put(0xA05F8054, FB)   # FB_R_SOF1, FB_R_SOF2 (the same: interlaced or not)
    a.put(0xA05F805C, 319 | 479 << 10 | 1 << 20)   # FB_R_SIZE: 640x480, 16-bit
    a.put(0xA05F6CE8, 1)              # SB_MMSEL: Maple words as they are
    a.put(0xA05F6C10, 0)              # SB_MDTSEL: started by the program
    a.put(0xA05F6C14, 1)              # SB_MDEN: Maple DMA on
    a.label('loop')
    a.const(8, CMD); a.const(9, REPLY)
    # the last (and only) transfer, port A, one word after the frame header; the reply's place; the frame: Get
    # condition (9) to port A's controller (0x20) from the console (0), one word: the controller function
    for k, v in enumerate([0x80000001, REPLY & 0x1FFFFFFF, 0x01002009, 0x01000000]): a.const(0, v); a.st(0, 4 * k, 8)
    a.movi(0, -1)
    for k in range(4): a.st(0, 4 * k, 9)   # no reply: nothing held
    a.put(0xA05F6C04, CMD & 0x1FFFFFFF)   # SB_MDSTAR
    a.put(0xA05F6C18, 1)                  # SB_MDST: go
    a.const(1, 0xA05F6C18); a.const(2, 2000000)
    a.label('wait'); a.ld(1, 0); a.tst0(1); a.bt('got'); a.dt(2); a.bf('wait')
    a.label('got')
    a.const(5, RED)
    a.ldw0(8, 9)                          # the buttons (a bit is 0 while held): A 0x4, Start 0x8
    a.tst0(4); a.bf('na'); a.const(5, GREEN); a.label('na')
    a.tst0(8); a.bf('ns'); a.const(5, BLUE); a.label('ns')
    a.const(3, 0xA5000000 | FB); a.const(4, 640 * 480 * 2 // 4)
    a.label('fill'); a.stn(5, 3); a.addi(3, 4); a.dt(4); a.bf('fill')
    a.bra('loop')
    return a.done()

def pad(s, n): s = s.encode() if isinstance(s, str) else s; assert len(s) <= n, s; return s + b' ' * (n - len(s))

def ipbin():
    h = (pad('SEGA SEGAKATANA', 16) + pad('RUDOLF OS EMU', 16) + pad('0000 GD-ROM1/1', 16) + pad('JUE', 8) +
         pad('E000F10', 8) + pad('RUDOLF-T01', 10) + pad('V1.000', 6) + pad('20261122', 8) + pad('', 8) +
         pad('1ST_READ.BIN', 16) + pad('RUDOLF OS', 16) + pad('RUDOLF OS EMU DREAMCAST TEST', 128))
    assert len(h) == 256
    ip = bytearray(32768); ip[:256] = h
    code = program(); assert len(code) < 0x3500; ip[0x300:0x300 + len(code)] = code
    return bytes(ip)

def both16(v): return struct.pack('<H', v) + struct.pack('>H', v)
def both32(v): return struct.pack('<I', v) + struct.pack('>I', v)
def dirrec(lba, size, flags, name):
    r = bytes([0, 0]) + both32(lba) + both32(size) + bytes([126, 11, 22, 12, 0, 0, 0, flags, 0, 0]) + both16(1) + bytes([len(name)]) + name
    r = bytes([len(r) + (len(r) & 1)]) + r[1:] + (b'\0' if len(r) & 1 else b'')
    return r

def iso(start):   # a small ISO 9660 file system whose sectors begin at LBA `start`: IP.BIN, then 1ST_READ.BIN
    note = b'Rudolf OS Emu Dreamcast test disc: the program is in IP.BIN (0x300). GPL-2.0-or-later.\n'
    first = note + b'\0' * (2048 - len(note))
    n = 22; sec = [b''] * n
    ip = ipbin()
    for i in range(16): sec[i] = ip[2048 * i:2048 * (i + 1)]
    root = dirrec(start + 20, 2048, 2, b'\0') + dirrec(start + 20, 2048, 2, b'\1') + dirrec(start + 21, len(first), 0, b'1ST_READ.BIN;1')
    sec[20] = root + b'\0' * (2048 - len(root)); sec[21] = first
    pt = lambda e: bytes([1, 0]) + (struct.pack('<I', start + 20) + struct.pack('<H', 1) if e == '<' else struct.pack('>I', start + 20) + struct.pack('>H', 1)) + b'\0\0'
    sec[18] = pt('<') + b'\0' * 2038; sec[19] = pt('>') + b'\0' * 2038
    d0 = b'0' * 16 + b'\0'
    pvd = (bytes([1]) + b'CD001' + bytes([1, 0]) + pad('', 32) + pad('RUDOLF_EMU_DC_TEST', 32) + b'\0' * 8 + both32(start + n) +
           b'\0' * 32 + both16(1) + both16(1) + both16(2048) + both32(10) + struct.pack('<I', start + 18) + b'\0' * 4 +
           struct.pack('>I', start + 19) + b'\0' * 4 + dirrec(start + 20, 2048, 2, b'\0') + pad('RUDOLF_EMU_DC_TEST', 128) +
           pad('RUDOLF OS', 128) + pad('DCTEST.PY', 128) + pad('', 128) + pad('', 37) + pad('', 37) + pad('', 37) +
           d0 * 4 + bytes([1, 0]))
    sec[16] = pvd + b'\0' * (2048 - len(pvd))
    sec[17] = bytes([255]) + b'CD001' + bytes([1]) + b'\0' * 2041
    assert all(len(x) == 2048 for x in sec)
    return b''.join(sec)

def bcd(x): return (x // 10) << 4 | x % 10
def raw(data, lba, mode=1):   # 2048-byte sectors as 2352-byte mode 1 sectors (sync, header; EDC/ECC left zero)
    out = bytearray()
    for i in range(0, len(data), 2048):
        f = lba + 150 + i // 2048
        out += b'\0' + b'\xff' * 10 + b'\0' + bytes([bcd(f // 4500), bcd(f // 75 % 60), bcd(f % 75), mode]) + data[i:i + 2048] + b'\0' * 288
    return bytes(out)

def gdi(path):
    d = os.path.dirname(path) or '.'; os.makedirs(d, exist_ok=True)
    low = iso(0)   # the low-density area's data track: the same small file system (as on real discs, a short one)
    open(os.path.join(d, 'track01.bin'), 'wb').write(raw(low + b'\0' * 2048 * (300 - len(low) // 2048), 0))
    open(os.path.join(d, 'track02.raw'), 'wb').write(b'\0' * 2352 * 300)     # 4 s of silence
    open(os.path.join(d, 'track03.bin'), 'wb').write(raw(iso(45000), 45000))
    open(path, 'w').write('3\n1 0 4 2352 track01.bin 0\n2 450 0 2352 track02.raw 0\n3 45000 4 2352 track03.bin 0\n')

def cdi(path):   # DiscJuggler 2 (0x80000004), as Flycast's reader (core/deps/chdpsr/cdipsr.cpp) and others read it
    START = 11702   # the data session's first sector (after the audio session's lead-out and the next lead-in)
    audio = b'\0' * 2352 * (150 + 300)
    data = b'\0' * 2352 * 150 + raw(iso(START), START)
    def track(mode, pregap, length, start, name=b'dctest.cdi'):
        mark = bytes([0, 0, 1, 0, 0, 0, 255, 255, 255, 255])
        return (struct.pack('<I', 0) + mark + mark + b'\0' * 4 + bytes([len(name)]) + name + b'\0' * 11 + b'\0' * 8 +
                struct.pack('<I', 0) + b'\0' * 2 + struct.pack('<II', pregap, length) + b'\0' * 6 + struct.pack('<I', mode) +
                b'\0' * 12 + struct.pack('<II', start, pregap + length) + b'\0' * 16 + struct.pack('<I', 2) + b'\0' * 29)
    sess_end = b'\0' * 12
    hdr = (struct.pack('<H', 2) + struct.pack('<H', 1) + track(0, 150, 300, 0) + sess_end +
           struct.pack('<H', 1) + track(1, 150, len(data) // 2352 - 150, START) + sess_end)
    body = audio + data
    open(path, 'wb').write(body + hdr + struct.pack('<II', 0x80000004, len(body)))

def check():
    od = 'sh4-linux-gnu-objdump'
    try: subprocess.run([od, '--version'], capture_output=True, check=True)
    except Exception: print('SKIP the encoder check: no sh4-linux-gnu-objdump'); return 0
    code = program()
    with tempfile.TemporaryDirectory() as d:
        p = os.path.join(d, 'c.bin'); open(p, 'wb').write(code)
        o = subprocess.run([od, '-D', '-b', 'binary', '-m', 'sh4', '-EL', '--adjust-vma=0xac008300', p], capture_output=True, text=True).stdout
    if '-v' in sys.argv: print(o)
    lines = [l for l in o.splitlines() if l.strip().startswith('ac00')]
    ncode = next(i for i, l in enumerate(lines) if 'bra' in l) + 2
    bad = [l for l in lines[:ncode] if '.word' in l or 'invalid' in l]
    print('FAIL the encoder: ' + '; '.join(bad) if bad else 'PASS the encoder: every instruction disassembles'); return 1 if bad else 0

if __name__ == '__main__':
    a = sys.argv[1:]
    if '--check' in a: sys.exit(check())
    if '--gdi' in a: gdi(a[a.index('--gdi') + 1])
    if '--cdi' in a: cdi(a[a.index('--cdi') + 1])
    if '--iso' in a: open(a[a.index('--iso') + 1], 'wb').write(iso(45000))
