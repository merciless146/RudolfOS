#!/usr/bin/env python3
# Rudolf OS's Dreamcast test game, made for Emu's checks (1.6.2): a tiny program written here, SH-4 machine code
# assembled by this script, on a disc made here: a .gdi with its three tracks and a .cdi (DiscJuggler's layout: an
# audio session, then a data session), each with an ISO 9660 file system holding 1ST_READ.BIN. No Sega code or data:
# the IP.BIN is written here too, its header (the fields a disc says what it is with) and, where a disc's bootstrap
# goes (0x300), three instructions that jump to 1ST_READ.BIN. It boots with Flycast's own HLE BIOS (reios: it loads
# IP.BIN to 0x8C008000, 1ST_READ.BIN to 0x8C010000, descrambled on a CD, and runs 0xAC008300); no BIOS files.
#   The program fills the screen (320x240, RGB565, drawn by the CPU into video memory) with red; green while
#   controller A's A button is held; blue while its Start is held (the controller read through the Maple bus).
#   python3 mkdc.py OUT-DIR     (writes OUT-DIR/gdi/Rudolf DC Test.gdi, track01.iso, track02.raw, track03.iso,
#                                OUT-DIR/Rudolf DC Test.cdi)
# GPL-2.0-or-later. The scrambling of 1ST_READ.BIN on a CD is the inverse of the descrambling Marcus Comstedt
# described (http://mc.pp.se/dc/sw.html), written here from that description.
import os, struct, sys

class Sh4:
    """A little SH-4 assembler: 16-bit instructions, 32-bit constants from a pool after the code."""
    def __init__(s, base): s.base, s.code, s.labels, s.fix, s.lits = base, [], {}, [], []
    def here(s): return s.base + 2 * len(s.code)
    def label(s, n): s.labels[n] = s.here()
    def h(s, x): s.code.append(x & 0xFFFF)
    def movl_lit(s, value, n):          # mov.l @(disp,PC),Rn
        s.lits.append(value); s.fix.append(('lit', len(s.code), len(s.lits) - 1)); s.h(0xD000 | n << 8)
    def mov_imm(s, i, n): s.h(0xE000 | n << 8 | (i & 0xFF))
    def movl_st(s, m, n): s.h(0x2002 | n << 8 | m << 4)        # mov.l Rm,@Rn
    def movl_ld(s, m, n): s.h(0x6002 | n << 8 | m << 4)        # mov.l @Rm,Rn
    def movw_ld0(s, d, m): s.h(0x8500 | m << 4 | (d // 2))     # mov.w @(disp,Rm),R0
    def mov(s, m, n): s.h(0x6003 | n << 8 | m << 4)
    def add_imm(s, i, n): s.h(0x7000 | n << 8 | (i & 0xFF))
    def shll16(s, n): s.h(0x4028 | n << 8)
    def or_(s, m, n): s.h(0x200B | n << 8 | m << 4)
    def dt(s, n): s.h(0x4010 | n << 8)
    def tst_imm(s, i): s.h(0xC800 | i)                         # tst #imm,R0
    def and_imm(s, i): s.h(0xC900 | i)                         # and #imm,R0
    def cmpeq_imm(s, i): s.h(0x8800 | (i & 0xFF))              # cmp/eq #imm,R0
    def nop(s): s.h(0x0009)
    def jmp(s, m): s.h(0x402B | m << 8); s.nop()
    def _b(s, op, lab, bits): s.fix.append((op, len(s.code), lab)); s.h(0)
    def bt(s, lab): s._b('bt', lab, 8)
    def bf(s, lab): s._b('bf', lab, 8)
    def bra(s, lab): s._b('bra', lab, 12); s.nop()
    def done(s):
        if len(s.code) % 2: s.nop()
        pool = s.base + 2 * len(s.code)
        for f in s.fix:
            at = s.base + 2 * f[1]
            if f[0] == 'lit':
                d = (pool + 4 * f[2] - ((at & ~3) + 4)) // 4; assert 0 <= d < 256, d; s.code[f[1]] |= d
            else:
                d = (s.labels[f[2]] - (at + 4)) // 2
                if f[0] == 'bra': assert -2048 <= d < 2048; s.code[f[1]] = 0xA000 | (d & 0xFFF)
                else: assert -128 <= d < 128; s.code[f[1]] = (0x8900 if f[0] == 'bt' else 0x8B00) | (d & 0xFF)
        return b''.join(struct.pack('<H', c) for c in s.code) + b''.join(struct.pack('<I', v) for v in s.lits)

RED, GREEN, BLUE = 0xF800, 0x07E0, 0x001F
CMD, RECV = 0x0C100000, 0x0C200000      # the Maple command list and its answer (physical; P2 0xA... to read them)

def program():
    a = Sh4(0x8C010000)
    def poke(addr, v): a.movl_lit(addr, 1); a.movl_lit(v, 0); a.movl_st(0, 1)
    # the video: the frame buffer on, RGB565, 320x240 at 0x200000 in video memory (32-bit area)
    poke(0xA05F8044, 0x00000005)                               # FB_R_CTRL
    poke(0xA05F805C, (1 << 20) | (239 << 10) | 159)            # FB_R_SIZE: modulus 1, 240 lines, 160 words a line
    poke(0xA05F8050, 0x00200000); poke(0xA05F8054, 0x00200000)  # FB_R_SOF1, SOF2
    poke(0xA05F80E8, 0x00000100)                               # VO_CONTROL: pixels doubled, the picture not blanked (as reset leaves it)
    # the Maple bus: no swapping, software start, the usual speed and the whole of RAM allowed
    poke(0xA05F6CE8, 1); poke(0xA05F6C10, 0); poke(0xA05F6C80, 0x3A980000); poke(0xA05F6C8C, 0x6155407F); poke(0xA05F6C14, 1)
    # the command list: last, port A, 2 words; the answer's place; "get condition" for the controller (function 1)
    for k, v in enumerate((0x80000001, RECV, 0x01002009, 0x01000000)): poke(0xA0000000 | (CMD + 4 * k), v)
    a.label('frame')
    poke(0xA0000000 | RECV, 0xFFFFFFFF)
    poke(0xA05F6C04, CMD); poke(0xA05F6C18, 1)                 # SB_MDSTAR, SB_MDST: start
    a.movl_lit(0xA05F6C18, 1); a.label('wait'); a.movl_ld(1, 0); a.tst_imm(1); a.bf('wait')
    a.movl_lit(RED, 4)
    a.movl_lit(0xA0000000 | RECV, 1); a.movl_ld(1, 0); a.and_imm(0xFF); a.cmpeq_imm(8); a.bf('draw')   # a data transfer answer
    a.movw_ld0(8, 1)                                           # the buttons (0 = held): A 0x04, Start 0x08
    a.tst_imm(0x04); a.bf('nota'); a.movl_lit(GREEN, 4); a.bra('draw')
    a.label('nota'); a.tst_imm(0x08); a.bf('draw'); a.movl_lit(BLUE, 4)
    a.label('draw')
    a.mov(4, 0); a.shll16(0); a.or_(0, 4)                      # two pixels a word
    a.movl_lit(0xA5200000, 2); a.movl_lit(320 * 240 // 2, 3)
    a.label('fill'); a.movl_st(4, 2); a.add_imm(4, 2); a.dt(3); a.bf('fill')
    a.bra('frame')
    return a.done()

def ip_bin():
    ip = bytearray(0x8000)
    f = [('SEGA SEGAKATANA ', 16), ('RUDOLF OS TEST  ', 16), ('0000 CD-ROM1/1  ', 16), ('JUE     ', 8), ('E000F10 ', 8),
         ('T-00000   ', 10), ('V1.000', 6), ('20261122', 8), ('        ', 8), ('1ST_READ.BIN    ', 16),
         ('RUDOLF OS       ', 16), ('RUDOLF DC TEST'.ljust(128), 128)]
    o = 0
    for t, n in f: assert len(t) == n; ip[o:o + n] = t.encode(); o += n
    b = Sh4(0x8C008300); b.movl_lit(0x8C010000, 0); b.jmp(0)  # its bootstrap: to 1ST_READ.BIN
    c = b.done(); ip[0x300:0x300 + len(c)] = c
    return bytes(ip)

def scramble(data):
    """1ST_READ.BIN as a CD holds it: the slices in the order the BIOS's descrambling reads them."""
    seed = len(data) & 0xFFFF
    def rnd():
        nonlocal seed; seed = (seed * 2109 + 9273) & 0x7FFF; return (seed + 0xC000) & 0xFFFF
    out, src, size, chunk = bytearray(), data, len(data), 0x200000
    while chunk >= 32:
        while size >= chunk:
            part, src = src[:chunk], src[chunk:]; n = chunk // 32; idx = list(range(n))
            for i in range(n - 1, -1, -1):
                x = (rnd() * i) >> 16; idx[i], idx[x] = idx[x], idx[i]; out += part[32 * idx[i]:32 * idx[i] + 32]
            size -= chunk
        chunk >>= 1
    return bytes(out + src)

def iso(base, ip, files):
    """An ISO 9660 image whose blocks are numbered from BASE (the track's first sector on the disc); IP.BIN in its
    first 16 sectors (the system area), FILES (name, bytes) in the root directory."""
    S = 2048; both32 = lambda v: struct.pack('<I', v) + struct.pack('>I', v); both16 = lambda v: struct.pack('<H', v) + struct.pack('>H', v)
    date = bytes([126, 11, 22, 0, 0, 0, 0])
    def rec(lba, size, flags, name):
        n = len(name); r = bytes([0, 0]) + both32(lba) + both32(size) + date + bytes([flags, 0, 0]) + both16(1) + bytes([n]) + name
        if len(r) % 2: r += b'\0'
        return bytes([len(r)]) + r[1:]
    nxt, ext = 21, []
    for name, data in files: ext.append(nxt); nxt += (len(data) + S - 1) // S
    total = max(nxt, 300)
    root = rec(base + 20, S, 2, b'\0') + rec(base + 20, S, 2, b'\1')
    for (name, data), e in zip(files, ext): root += rec(base + e, len(data), 0, name.encode() + b';1')
    pt = lambda le: bytes([1, 0]) + (struct.pack('<I' if le else '>I', base + 20)) + (struct.pack('<H' if le else '>H', 1)) + b'\0\0'
    pvd = bytearray(S); pvd[0:8] = b'\x01CD001\x01\x00'; pvd[8:40] = b' ' * 32; pvd[40:72] = b'RUDOLF_DC_TEST'.ljust(32)
    pvd[80:88] = both32(base + total); pvd[120:124] = both16(1); pvd[124:128] = both16(1); pvd[128:132] = both16(S)
    pvd[132:140] = both32(10); pvd[140:144] = struct.pack('<I', base + 18); pvd[148:152] = struct.pack('>I', base + 19)
    pvd[156:190] = rec(base + 20, S, 2, b'\0'); pvd[190:813] = b' ' * 623
    for o in (813, 830, 847, 864): pvd[o:o + 17] = b'0' * 16 + b'\0'
    pvd[881] = 1
    img = bytearray(total * S); img[0:len(ip)] = ip; img[16 * S:17 * S] = pvd
    img[17 * S:17 * S + 7] = b'\xffCD001\x01'; img[18 * S:18 * S + 10] = pt(True); img[19 * S:19 * S + 10] = pt(False)
    img[20 * S:20 * S + len(root)] = root
    for (name, data), e in zip(files, ext): img[e * S:e * S + len(data)] = data
    return bytes(img)

def first_read():
    p = program(); return p + bytes((-len(p)) % 2048)

def gdi(out):
    os.makedirs(out, exist_ok=True)
    open(os.path.join(out, 'track01.iso'), 'wb').write(iso(0, b'', [('README.TXT', b'Rudolf OS: a Dreamcast test disc for Emu.\r\n')]))
    open(os.path.join(out, 'track02.raw'), 'wb').write(bytes(2352 * 300))
    open(os.path.join(out, 'track03.iso'), 'wb').write(iso(45000, ip_bin(), [('1ST_READ.BIN', first_read())]))
    open(os.path.join(out, 'Rudolf DC Test.gdi'), 'w').write('3\r\n1 0 4 2048 track01.iso 0\r\n2 450 0 2352 track02.raw 0\r\n3 45000 4 2048 track03.iso 0\r\n')

def cdi(path):
    """DiscJuggler 2.0's layout (as Flycast's cdipsr reads it): session 1 an audio track, session 2 the data track
    (mode 2, 2336-byte sectors) from block 11702, each track with a 150-sector pregap; then the descriptor, the
    version and the descriptor's place."""
    audio = 300; data = iso(11702, ip_bin(), [('1ST_READ.BIN', scramble(first_read()))]); n = len(data) // 2048
    body = bytearray(bytes(2352 * (150 + audio)))
    sub = b'\x00\x00\x08\x00\x00\x00\x08\x00'
    body += bytes(2336 * 150) + b''.join(sub + data[i * 2048:(i + 1) * 2048] + bytes(280) for i in range(n))
    mark = bytes([0, 0, 1, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF])
    def track(name, pregap, length, mode, lba, size_code):
        t = struct.pack('<I', 0) + mark + mark + bytes(4) + bytes([len(name)]) + name + bytes(11) + bytes(4) + bytes(4)
        t += struct.pack('<I', 0) + bytes(2) + struct.pack('<II', pregap, length) + bytes(6) + struct.pack('<I', mode)
        t += bytes(12) + struct.pack('<II', lba, pregap + length) + bytes(16) + struct.pack('<I', size_code) + bytes(29)
        return t
    head = struct.pack('<H', 2)
    head += struct.pack('<H', 1) + track(b'rudolf.cdi', 150, audio, 0, 0, 2) + bytes(12)
    head += struct.pack('<H', 1) + track(b'rudolf.cdi', 150, n, 2, 11702, 1) + bytes(12)
    off = len(body)
    open(path, 'wb').write(bytes(body) + head + struct.pack('<II', 0x80000004, off))

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else 'dc'
    gdi(os.path.join(out, 'gdi')); cdi(os.path.join(out, 'Rudolf DC Test.cdi'))
