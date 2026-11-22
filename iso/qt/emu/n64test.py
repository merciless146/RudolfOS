#!/usr/bin/env python3
# Emu's Nintendo 64 test game, made for Emu's emulator test (iso/qt/emu/nativerun_t.sh): no Nintendo code, boot code
# or other material in it. GPL-2.0-or-later.
#
# The whole program is the ROM's boot code (0x40-0xFFF): an emulator's HLE start-up (Mupen64Plus's, which Rosalie's
# Mupen GUI uses: src/device/pif/bootrom_hle.c) copies those bytes to the RSP's memory and starts the CPU at 0xA4000040,
# as the console's PIF does with the cartridge's own boot code. It is hand-assembled here (a few MIPS instructions,
# checked against GNU binutils' disassembly by `n64test.py --check`): Nintendo's boot code (IPL3) is not used, and
# zeros cannot work there (the CPU would run through zeros, never reaching a program at 0x1000).
#
# What it does: sets the video interface to 320x240 16-bit, then every frame reads controller 1 through the PIF and
# fills the screen (by the CPU into the frame in memory and by an RDP fill rectangle, so both HLE and LLE graphics
# show it): RED (248,0,0) normally, GREEN (0,248,0) while A is held, BLUE (0,0,248) while Start is held. At start it
# writes "RUDOLF64" to EEPROM block 0 (the header says: 4 Kbit EEPROM, the homebrew header's save type).
#
#   n64test.py OUT.z64 [--n64 OUT.n64] [--v64 OUT.v64]     the game in each byte order
#   n64test.py --check                                    the encoder against mips-linux-gnu-objdump (if installed)
import struct, sys, subprocess, tempfile, os

R = {n: i for i, n in enumerate('zero at v0 v1 a0 a1 a2 a3 t0 t1 t2 t3 t4 t5 t6 t7 s0 s1 s2 s3 s4 s5 s6 s7 t8 t9 k0 k1 gp sp fp ra'.split())}

class Asm:
    def __init__(s, base): s.base, s.code, s.labels, s.fix = base, [], {}, []
    def pc(s): return s.base + 4 * len(s.code)
    def label(s, n): s.labels[n] = s.pc()
    def w(s, x): s.code.append(x & 0xFFFFFFFF)
    def i(s, op, rs, rt, imm): s.w(op << 26 | R[rs] << 21 | R[rt] << 16 | (imm & 0xFFFF))
    def lui(s, rt, imm): s.i(0x0F, 'zero', rt, imm)
    def ori(s, rt, rs, imm): s.i(0x0D, rs, rt, imm)
    def andi(s, rt, rs, imm): s.i(0x0C, rs, rt, imm)
    def addiu(s, rt, rs, imm): s.i(0x09, rs, rt, imm)
    def sltiu(s, rt, rs, imm): s.i(0x0B, rs, rt, imm)
    def lw(s, rt, off, rs): s.i(0x23, rs, rt, off)
    def lhu(s, rt, off, rs): s.i(0x25, rs, rt, off)
    def sw(s, rt, off, rs): s.i(0x2B, rs, rt, off)
    def sll(s, rd, rt, sa): s.w(R[rt] << 16 | R[rd] << 11 | sa << 6)
    def or_(s, rd, rs, rt): s.w(R[rs] << 21 | R[rt] << 16 | R[rd] << 11 | 0x25)
    def xor(s, rd, rs, rt): s.w(R[rs] << 21 | R[rt] << 16 | R[rd] << 11 | 0x26)
    def nop(s): s.w(0)
    def br(s, op, rs, rt, lab): s.fix.append((len(s.code), lab)); s.i(op, rs, rt, 0); s.nop()   # with its delay slot
    def beq(s, rs, rt, lab): s.br(0x04, rs, rt, lab)
    def bne(s, rs, rt, lab): s.br(0x05, rs, rt, lab)
    def li(s, rt, v):
        v &= 0xFFFFFFFF
        if v >> 16: s.lui(rt, v >> 16); (v & 0xFFFF) and s.ori(rt, rt, v & 0xFFFF)
        else: s.ori(rt, 'zero', v)
    def store(s, base, off, v): s.li('t1', v); s.sw('t1', off, base)   # base register + offset <- a constant
    def done(s):
        for at, lab in s.fix: s.code[at] |= ((s.labels[lab] - (s.base + 4 * at + 4)) >> 2) & 0xFFFF
        return b''.join(struct.pack('>I', x) for x in s.code)

RED, GREEN, BLUE = 0xF801, 0x07C1, 0x003F     # RGBA 5551
FB, FB2, W, H = 0x00100000, 0x00180000, 320, 240   # the two frames, in RDRAM
BUF, DL = 0x800, 0x900                         # the PIF block and the RDP's commands, in RDRAM

def program():
    a = Asm(0xA4000040)
    a.lui('s0', 0xA440)                       # the video interface (VI)
    for off, v in [(0x00, 0x0000320E), (0x04, FB), (0x08, W), (0x0C, 0x200), (0x10, 0), (0x14, 0x03E52239),
                   (0x18, 0x20D), (0x1C, 0xC15), (0x20, 0x0C150C15), (0x24, 0x006C02EC), (0x28, 0x002501FF),
                   (0x2C, 0x000E0204), (0x30, 0x200), (0x34, 0x400)]:
        a.store('s0', off, v)
    a.li('s6', FB2)                           # the frame drawn next (the VI shows the other: two frames, as games do)
    a.lui('s1', 0xA000 | (BUF >> 16)); a.addiu('s1', 's1', BUF & 0xFFFF)   # the PIF block (uncached)
    a.lui('s2', 0xA480)                       # the serial interface (SI)
    # EEPROM: skip the 4 controller channels, then the cartridge's: tx 10, rx 1, write (5), block 0, 8 bytes
    blk = [0x00000000, 0x0A010500] + list(struct.unpack('>2I', b'RUDOLF64')) + [0xFFFE0000] + [0] * 10 + [1]
    for k, v in enumerate(blk): a.store('s1', 4 * k, v)
    a.li('t9', 1); a.beq('zero', 'zero', 'pif')   # t9 = 1: the EEPROM write; then the main loop
    a.label('frame')
    # controller 1: tx 1, rx 4, read buttons (1); the end mark; the PIF's "run the commands" flag
    for k, v in enumerate([0xFF010401, 0xFFFFFFFF, 0xFE000000] + [0] * 12 + [1]): a.store('s1', 4 * k, v)
    a.li('t9', 0)
    a.label('pif')
    a.store('s2', 0x00, BUF); a.store('s2', 0x10, 0x1FC007C0)   # RDRAM to the PIF (64 bytes)
    a.label('w1'); a.lw('t0', 0x18, 's2'); a.andi('t0', 't0', 3); a.bne('t0', 'zero', 'w1')
    a.store('s2', 0x00, BUF); a.store('s2', 0x04, 0x1FC007C0)   # the PIF's answer back to RDRAM
    a.label('w2'); a.lw('t0', 0x18, 's2'); a.andi('t0', 't0', 3); a.bne('t0', 'zero', 'w2')
    a.bne('t9', 'zero', 'frame')
    # the colour: red, green while A (0x8000), blue while Start (0x1000)
    a.lhu('t2', 4, 's1')
    a.li('s3', RED << 16 | RED)
    a.andi('t0', 't2', 0x8000); a.beq('t0', 'zero', 'nA'); a.li('s3', GREEN << 16 | GREEN); a.label('nA')
    a.andi('t0', 't2', 0x1000); a.beq('t0', 'zero', 'nS'); a.li('s3', BLUE << 16 | BLUE); a.label('nS')
    # no controller (the rx byte's error bits): still red
    # the CPU fills the frame
    a.lui('t3', 0xA000); a.or_('t3', 't3', 's6'); a.li('t4', W * H * 2 // 4)
    a.label('fill'); a.sw('s3', 0, 't3'); a.addiu('t3', 't3', 4); a.addiu('t4', 't4', -1); a.bne('t4', 'zero', 'fill')
    # and the RDP: colour image, scissor, fill mode, fill colour, the rectangle, full sync
    a.lui('s4', 0xA000 | (DL >> 16)); a.addiu('s4', 's4', DL & 0xFFFF)
    dl = [0x3F10013F, 's6', 0x2D000000, 0x005003C0, 0x2F300000, 0, 0x27000000, 0, 0x37000000, 's3', 0x364FC3BC, 0,
          0x27000000, 0, 0x29000000, 0]
    for k, v in enumerate(dl):
        if isinstance(v, str): a.sw(v, 4 * k, 's4')
        else: a.store('s4', 4 * k, v)
    a.lui('s5', 0xA410)                       # the RDP's command registers (DPC)
    a.store('s5', 0x0C, 1)                    # commands from RDRAM (clear XBUS)
    a.store('s5', 0x00, DL); a.store('s5', 0x04, DL + 4 * len(dl))
    # the next frame: wait for the VI to leave lines 0-9, then to come back to them
    a.label('v1'); a.lw('t0', 0x10, 's0'); a.sltiu('t0', 't0', 10); a.bne('t0', 'zero', 'v1')
    a.label('v2'); a.lw('t0', 0x10, 's0'); a.sltiu('t0', 't0', 10); a.beq('t0', 'zero', 'v2')
    a.sw('s6', 0x04, 's0')                    # show it (VI_ORIGIN), draw the other next
    a.lui('t0', (FB ^ FB2) >> 16); a.xor('s6', 's6', 't0')
    a.beq('zero', 'zero', 'frame')
    code = a.done(); assert len(code) <= 0xFC0, len(code)
    return code

def crc6102(rom):   # the header's two check words, over 0x1000-0x100FFF (the public algorithm with the 6102 seed)
    M = 0xFFFFFFFF; t1 = t2 = t3 = t4 = t5 = t6 = 0xF8CA4DDC
    for i in range(0x1000, 0x101000, 4):
        d = struct.unpack_from('>I', rom, i)[0]
        if (t6 + d) & M < t6: t4 = (t4 + 1) & M
        t6 = (t6 + d) & M; t3 ^= d
        r = ((d << (d & 31)) | (d >> (32 - (d & 31)))) & M if d & 31 else d
        t5 = (t5 + r) & M
        t2 = t2 ^ r if t2 > d else t2 ^ t6 ^ d
        t1 = (t1 + (t5 ^ d)) & M
    return t6 ^ t4 ^ t3, t5 ^ t2 ^ t1

def rom():
    r = bytearray(0x200000)                   # 2 MiB, the program in the boot code's place
    struct.pack_into('>IIII', r, 0, 0x80371240, 0x0000000F, 0x80000400, 0x0000144C)
    r[0x20:0x34] = b'RUDOLF EMU N64 TEST '
    r[0x3B:0x40] = b'NEDE' + bytes([0x10])   # media N, id "ED" (the homebrew header), region E; save type 1: EEPROM 4K
    code = program(); r[0x40:0x40 + len(code)] = code
    r[0x1000:0x1010] = b'Rudolf OS Emu   '      # something after the boot code, so the checksum is not over zeros only
    struct.pack_into('>II', r, 0x10, *crc6102(r))
    return bytes(r)

def swap(b, order):   # .n64: little-endian words; .v64: byte-swapped halfwords
    out = bytearray(len(b))
    for i in range(0, len(b), 4):
        w = b[i:i + 4]; out[i:i + 4] = w[::-1] if order == 'n64' else bytes([w[1], w[0], w[3], w[2]])
    return bytes(out)

def check():
    od = 'mips-linux-gnu-objdump'
    try: subprocess.run([od, '--version'], capture_output=True, check=True)
    except Exception: print('SKIP the encoder check: no mips-linux-gnu-objdump'); return 0
    with tempfile.TemporaryDirectory() as d:
        p = os.path.join(d, 'c.bin'); open(p, 'wb').write(program())
        o = subprocess.run([od, '-D', '-b', 'binary', '-m', 'mips:4300', '-EB', '--adjust-vma=0xa4000040', p], capture_output=True, text=True).stdout
    bad = [l for l in o.splitlines() if '(bad)' in l or '.word' in l]
    print(o if '-v' in sys.argv else '\n'.join(o.splitlines()[-12:]))
    print('FAIL the encoder: ' + '; '.join(bad) if bad else 'PASS the encoder: every word disassembles'); return 1 if bad else 0

if __name__ == '__main__':
    if '--check' in sys.argv: sys.exit(check())
    a = sys.argv[1:]; b = rom(); open(a[0], 'wb').write(b)
    for k in ('--n64', '--v64'):
        if k in a: open(a[a.index(k) + 1], 'wb').write(swap(b, k[2:]))
