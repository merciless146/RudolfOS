#!/usr/bin/env python3
# Rudolf OS's Nintendo 64 test game, made for Emu's checks (1.6.2): a tiny program written here, MIPS machine code
# assembled by this script. No Nintendo code or data: the header's boot-code area holds this program itself, not an
# IPL3 (Mupen64Plus' HLE boot copies ROM 0x40-0xFFF to the RSP's memory and runs it there, without the checksum a
# real console's PIF makes; the game would not start on a real console).
#   It fills the screen with one colour, drawn by the CPU into RDRAM and by the RDP (a fill rectangle), on two
#   buffers swapped every frame: red; green while controller 1's A is held; blue while Start is held. The first time
#   A is pressed it writes "RUDOLF64" to EEPROM block 0 (the Advanced Homebrew ROM Header asks for a 4 Kbit EEPROM).
#   python3 mkn64.py OUT.z64 [OUT.n64 OUT.v64]   (.z64 big-endian; .n64 32-bit words swapped; .v64 16-bit swapped)
# GPL-2.0-or-later.
import struct, sys

R = {n: i for i, n in enumerate('zero at v0 v1 a0 a1 a2 a3 t0 t1 t2 t3 t4 t5 t6 t7 s0 s1 s2 s3 s4 s5 s6 s7 t8 t9 k0 k1 gp sp fp ra'.split())}

class Asm:
    def __init__(s, base): s.base, s.code, s.labels, s.fix = base, [], {}, []
    def here(s): return s.base + 4 * len(s.code)
    def label(s, n): s.labels[n] = s.here()
    def w(s, x): s.code.append(x & 0xFFFFFFFF)
    def r(s, fn, rd, rs, rt, sa=0): s.w((R[rs] << 21) | (R[rt] << 16) | (R[rd] << 11) | (sa << 6) | fn)
    def i(s, op, rt, rs, imm): s.w((op << 26) | (R[rs] << 21) | (R[rt] << 16) | (imm & 0xFFFF))
    def nop(s): s.w(0)
    # R-type
    def addu(s, d, a, b): s.r(0x21, d, a, b)
    def or_(s, d, a, b): s.r(0x25, d, a, b)
    def and_(s, d, a, b): s.r(0x24, d, a, b)
    def xor(s, d, a, b): s.r(0x26, d, a, b)
    def sll(s, d, t, sa): s.w((R[t] << 16) | (R[d] << 11) | (sa << 6) | 0)
    def srl(s, d, t, sa): s.w((R[t] << 16) | (R[d] << 11) | (sa << 6) | 2)
    # I-type
    def addiu(s, t, a, imm): s.i(0x09, t, a, imm)
    def sltiu(s, t, a, imm): s.i(0x0B, t, a, imm)
    def andi(s, t, a, imm): s.i(0x0C, t, a, imm)
    def ori(s, t, a, imm): s.i(0x0D, t, a, imm)
    def lui(s, t, imm): s.i(0x0F, t, 'zero', imm)
    def lw(s, t, off, b): s.i(0x23, t, b, off)
    def lbu(s, t, off, b): s.i(0x24, t, b, off)
    def sw(s, t, off, b): s.i(0x2B, t, b, off)
    def li(s, t, v):
        v &= 0xFFFFFFFF; s.lui(t, v >> 16); s.ori(t, t, v & 0xFFFF)
    def _br(s, op, a, b, lab): s.fix.append((len(s.code), lab)); s.i(op, b, a, 0); s.nop()
    def beq(s, a, b, lab): s._br(0x04, a, b, lab)
    def bne(s, a, b, lab): s._br(0x05, a, b, lab)
    def b(s, lab): s.beq('zero', 'zero', lab)
    def done(s):
        for at, lab in s.fix:
            off = (s.labels[lab] - (s.base + 4 * at + 4)) // 4
            assert -32768 <= off < 32768
            s.code[at] |= off & 0xFFFF
        return b''.join(struct.pack('>I', c) for c in s.code)

VI, SI, DPC = 0xA4400000, 0xA4800000, 0xA4100000
FB = (0x00100000, 0x00140000)           # two 320x240 RGBA5551 buffers in RDRAM (physical)
PIFCMD, PIFOUT, EEPCMD, DL = 0x00300000, 0x00300040, 0x00300080, 0x00310000
RED, GREEN, BLUE = 0xF801, 0x07C1, 0x003F

def program():
    a = Asm(0xA4000040)
    # the video interface: NTSC, 320x240, 16-bit (libdragon's and the SDKs' usual values)
    a.lui('s0', VI >> 16)
    for off, v in ((0x00, 0x00003202), (0x04, FB[0]), (0x08, 320), (0x0C, 2), (0x10, 0), (0x14, 0x03E52239), (0x18, 0x0000020D),
                   (0x1C, 0x00000C15), (0x20, 0x0C150C15), (0x24, 0x006C02EC), (0x28, 0x002501FF), (0x2C, 0x000E0204),
                   (0x30, 0x00000200), (0x34, 0x00000400)):
        a.li('t0', v); a.sw('t0', off, 's0')
    # the PIF commands: controller 1's buttons (tx 1, rx 4, command 1), end; and an EEPROM write of block 0
    a.lui('s1', 0xA000 | (PIFCMD >> 16)); a.ori('s1', 's1', PIFCMD & 0xFFFF)
    pif = bytes([0x01, 0x04, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE]) + bytes(55) + b'\x01'
    eep = bytes([0, 0, 0, 0, 0x0A, 0x01, 0x05, 0x00]) + b'RUDOLF64' + bytes([0xFF, 0xFE]) + bytes(45) + b'\x01'
    for k in range(16): a.li('t0', struct.unpack('>I', pif[4 * k:4 * k + 4])[0]); a.sw('t0', 4 * k, 's1')
    for k in range(16): a.li('t0', struct.unpack('>I', eep[4 * k:4 * k + 4])[0]); a.sw('t0', 0x80 + 4 * k, 's1')
    a.lui('s2', SI >> 16); a.lui('s3', DPC >> 16)
    a.li('s4', 0xA0000000 | DL)
    a.addu('s5', 'zero', 'zero')        # s5: the buffer being drawn (0/1)
    a.addu('s6', 'zero', 'zero')        # s6: the EEPROM written once
    a.label('frame')
    # read the controller: RDRAM -> PIF, then PIF -> RDRAM (SI DMA), waiting for the SI each time
    a.li('t0', PIFCMD); a.sw('t0', 0x00, 's2'); a.li('t0', 0x1FC007C0); a.sw('t0', 0x10, 's2')
    a.label('si1'); a.lw('t0', 0x18, 's2'); a.andi('t0', 't0', 3); a.bne('t0', 'zero', 'si1')
    a.sw('zero', 0x18, 's2')
    a.li('t0', PIFOUT); a.sw('t0', 0x00, 's2'); a.li('t0', 0x1FC007C0); a.sw('t0', 0x04, 's2')
    a.label('si2'); a.lw('t0', 0x18, 's2'); a.andi('t0', 't0', 3); a.bne('t0', 'zero', 'si2')
    a.sw('zero', 0x18, 's2')
    a.lui('t1', 0xA000 | (PIFOUT >> 16)); a.ori('t1', 't1', PIFOUT & 0xFFFF)
    a.li('s7', RED)
    a.lbu('t2', 1, 't1'); a.andi('t2', 't2', 0xC0); a.bne('t2', 'zero', 'colour')   # no controller (an error in rx)
    a.lbu('t2', 3, 't1')                 # A B Z Start Up Down Left Right
    a.andi('t3', 't2', 0x80); a.beq('t3', 'zero', 'nota')
    a.li('s7', GREEN)
    a.bne('s6', 'zero', 'colour')
    a.addiu('s6', 'zero', 1)             # A, the first time: EEPROM block 0
    a.li('t0', EEPCMD); a.sw('t0', 0x00, 's2'); a.li('t0', 0x1FC007C0); a.sw('t0', 0x10, 's2')
    a.label('si3'); a.lw('t0', 0x18, 's2'); a.andi('t0', 't0', 3); a.bne('t0', 'zero', 'si3')
    a.sw('zero', 0x18, 's2')
    a.li('t0', EEPCMD + 0x40); a.sw('t0', 0x00, 's2'); a.li('t0', 0x1FC007C0); a.sw('t0', 0x04, 's2')
    a.label('si4'); a.lw('t0', 0x18, 's2'); a.andi('t0', 't0', 3); a.bne('t0', 'zero', 'si4')
    a.sw('zero', 0x18, 's2')
    a.b('colour')
    a.label('nota')
    a.andi('t3', 't2', 0x10); a.beq('t3', 'zero', 'colour')
    a.li('s7', BLUE)
    a.label('colour')
    # the buffer: t4 its physical address
    a.li('t4', FB[0]); a.beq('s5', 'zero', 'fb0'); a.li('t4', FB[1]); a.label('fb0')
    # the CPU's fill (uncached), two pixels a word
    a.sll('t5', 's7', 16); a.or_('t5', 't5', 's7')
    a.lui('t6', 0xA000); a.or_('t6', 't6', 't4'); a.li('t7', 320 * 240 // 2)
    a.label('fill'); a.sw('t5', 0, 't6'); a.addiu('t6', 't6', 4); a.addiu('t7', 't7', -1); a.bne('t7', 'zero', 'fill')
    # the RDP's fill: colour image, scissor, fill mode, fill colour, rectangle, full sync
    a.li('t0', 0x3F10013F); a.sw('t0', 0, 's4'); a.sw('t4', 4, 's4')
    a.li('t0', 0x2D000000); a.sw('t0', 8, 's4'); a.li('t0', 0x005003C0); a.sw('t0', 12, 's4')
    a.li('t0', 0x2F300000); a.sw('t0', 16, 's4'); a.sw('zero', 20, 's4')
    a.li('t0', 0x37000000); a.sw('t0', 24, 's4'); a.sw('t5', 28, 's4')
    a.li('t0', 0x364FC3BC); a.sw('t0', 32, 's4'); a.sw('zero', 36, 's4')
    a.li('t0', 0x29000000); a.sw('t0', 40, 's4'); a.sw('zero', 44, 's4')
    a.addiu('t0', 'zero', 1); a.sw('t0', 0x0C, 's3')     # DPC_STATUS: commands from RDRAM
    a.li('t0', DL); a.sw('t0', 0x00, 's3'); a.li('t0', DL + 48); a.sw('t0', 0x04, 's3')
    # wait for the vertical blank (the line counter near its end, then back at the top), then show this buffer
    a.label('vb1'); a.lw('t0', 0x10, 's0'); a.sltiu('t1', 't0', 0x200); a.bne('t1', 'zero', 'vb1')
    a.label('vb2'); a.lw('t0', 0x10, 's0'); a.sltiu('t1', 't0', 0x10); a.beq('t1', 'zero', 'vb2')
    a.sw('t4', 0x04, 's0')
    a.addiu('t0', 'zero', 1)
    a.xor('s5', 's5', 't0')
    a.b('frame')
    c = a.done(); assert len(c) <= 0xFC0, len(c)
    return c

def crc(rom):
    # the header's checksum over 1 MiB from 0x1000; the ROM is 2 MiB (the algorithm with the 6102-style seed); only arithmetic
    seed = 0xF8CA4DDC; t1 = t2 = t3 = t4 = t5 = t6 = seed; M = 0xFFFFFFFF
    for i in range(0x1000, 0x101000, 4):
        d = struct.unpack('>I', rom[i:i + 4])[0]
        if (t6 + d) & M < t6: t4 = (t4 + 1) & M
        t6 = (t6 + d) & M; t3 ^= d
        r = ((d << (d & 31)) | (d >> (32 - (d & 31)))) & M if d & 31 else d
        t5 = (t5 + r) & M
        t2 = t2 ^ r if t2 > d else t2 ^ t6 ^ d
        t1 = (t1 + (t5 ^ d)) & M
    return (t6 ^ t4 ^ t3) & M, (t5 ^ t2 ^ t1) & M

def rom():
    r = bytearray(0x200000)
    struct.pack_into('>IIII', r, 0, 0x80371240, 0x0000000F, 0x80000400, 0x0000144C)
    r[0x20:0x34] = b'RUDOLF EMU TEST     '
    r[0x3B:0x40] = b'NED' + b'E' + bytes([0x10])     # media N, cartridge ID "ED" (the Advanced Homebrew ROM Header), region E, EEPROM 4K
    c = program(); r[0x40:0x40 + len(c)] = c
    r[0x1000:0x1010] = b'Rudolf OS 1.6.2 '            # (no game code here: everything runs from the boot area)
    c1, c2 = crc(r); struct.pack_into('>II', r, 0x10, c1, c2)
    return bytes(r)

def n64(z): return b''.join(z[i:i + 4][::-1] for i in range(0, len(z), 4))
def v64(z): return b''.join(z[i:i + 2][::-1] for i in range(0, len(z), 2))

if __name__ == '__main__':
    z = rom(); outs = sys.argv[1:] or ['test.z64']
    for o in outs:
        open(o, 'wb').write(n64(z) if o.lower().endswith('.n64') else v64(z) if o.lower().endswith('.v64') else z)
