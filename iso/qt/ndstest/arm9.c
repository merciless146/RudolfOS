/* Rudolf OS DS test (ARM9): the lower screen shows a colour for the buttons held and a white square where
   the screen is touched (positions come from the ARM7 at 0x02FFF000). GPL-2.0-or-later. */
#define R(a) (*(volatile unsigned int *)(a))
#define H(a) (*(volatile unsigned short *)(a))
#define SH ((volatile unsigned short *)0x02FFF000)
void main9(void) {
	H(0x04000304) = 0x0003;            /* both screens on, 2D engine A shown on the lower screen */
	R(0x04000000) = 0x00020000;        /* engine A: show VRAM bank A directly */
	*(volatile unsigned char *)0x04000240 = 0x80;   /* VRAM A: enabled for the CPU */
	volatile unsigned short *fb = (volatile unsigned short *)0x06800000;
	for (;;) {
		while (H(0x04000006) != 192) ;  /* wait for the vertical blank */
		unsigned k = ~H(0x04000130) & 0x3FF;   /* A B Select Start Right Left Up Down R L */
		unsigned xy = SH[3];                     /* X Y from the ARM7 (bits 0, 1), active low */
		unsigned short c = 0x2800;               /* dark blue */
		if (k & 1) c = 0x001F;                   /* A: red */
		else if (k & 2) c = 0x03E0;              /* B: green */
		else if (!(xy & 1)) c = 0x7C1F;          /* X: magenta */
		else if (k & 8) c = 0x7FE0;              /* Start: cyan */
		else if (k & 0x40) c = 0x03FF;           /* Up: yellow */
		for (int i = 0; i < 256 * 192; i++) fb[i] = c;
		if (SH[2]) {                             /* pen down: an 8x8 white square */
			int x = SH[0] >> 4, y = SH[1] >> 4;
			for (int yy = y - 4; yy < y + 4; yy++) for (int xx = x - 4; xx < x + 4; xx++)
				if (xx >= 0 && xx < 256 && yy >= 0 && yy < 192) fb[yy * 256 + xx] = 0x7FFF;
		}
		while (H(0x04000006) == 192) ;
	}
}
__attribute__((naked, section(".text.start"))) void _start(void) { __asm__("ldr sp, =0x02FF0000\n bl main9\n b ."); }
