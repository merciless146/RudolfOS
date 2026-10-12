/* Rudolf OS DS test (ARM7): reads the touch chip and the X/Y/pen bits, shares them at 0x02FFF000. */
#define H(a) (*(volatile unsigned short *)(a))
#define SH ((volatile unsigned short *)0x02FFF000)
static unsigned tsc(unsigned cmd) {
	H(0x040001C0) = 0x8A02; H(0x040001C2) = cmd; while (H(0x040001C0) & 0x80) ;
	H(0x040001C2) = 0; while (H(0x040001C0) & 0x80) ; unsigned hi = H(0x040001C2) & 0xFF;
	H(0x040001C0) = 0x8202; H(0x040001C2) = 0; while (H(0x040001C0) & 0x80) ; unsigned lo = H(0x040001C2) & 0xFF;
	return ((hi << 8) | lo) >> 3;
}
void main7(void) {
	for (;;) {
		unsigned keyxy = H(0x04000136);
		int down = !(keyxy & 0x40);
		if (down) { SH[0] = tsc(0xD0); SH[1] = tsc(0x90); }
		SH[2] = down; SH[3] = keyxy;
	}
}
__attribute__((naked, section(".text.start"))) void _start(void) { __asm__("ldr sp, =0x0380FF00\n bl main7\n b ."); }
