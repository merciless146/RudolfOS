/* Tests rudolf-display's scaling (Settings > Display > Resolution on the software display): where the picture
 * goes, black bars, colours, the cursor's place and touches mapped back to the desktop.
 *   gcc -O0 -w -o /tmp/scale_t iso/qt/scale_t.c -lX11 /usr/lib/x86_64-linux-gnu/libXfixes.so.3 /usr/lib/x86_64-linux-gnu/libXrandr.so.2 -lm && /tmp/scale_t */
#include <X11/Xlib.h>
#define XFlush(d) 0
#define main rd_display_main
#include "../sys/rudolf-display.c"
#undef main
static int absx = -1, absy = -1;
int XTestFakeKeyEvent(Display *d, unsigned k, Bool down, unsigned long t) { return 1; }
int XTestFakeButtonEvent(Display *d, unsigned b, Bool down, unsigned long t) { return 1; }
int XTestFakeMotionEvent(Display *d, int sc, int x, int y, unsigned long t) { absx = x; absy = y; return 1; }
int XTestFakeRelativeMotionEvent(Display *d, int x, int y, unsigned long t) { return 1; }
static int fails;
static void check(const char *n, int ok, const char *detail) { printf("%s %s  %s\n", ok ? "PASS" : "FAIL", n, detail); if (!ok) fails++; }
static uint8_t *xmem;
/* a framebuffer FW x FH and an X picture XW x XH filled by fill(x,y) */
static void setup(int FW, int FH, int XW, int XH, uint32_t (*fill)(int, int))
{
	scaled = 0; free(outb); outb = NULL; free(shadow); free(fbmem); free(xmem); free(srow_dirty); srow_dirty = NULL;
	fbw = FW; fbh = FH; fbbpp = 32; fbswap = 0; fbstride = FW * 4; fbmem = calloc((size_t)FW * FH, 4); memset(fbmem, 0x55, (size_t)FW * FH * 4);
	xw = XW; xh = XH; xstride = XW * 4; xmem = malloc((size_t)XW * XH * 4);
	for (int y = 0; y < XH; y++) for (int x = 0; x < XW; x++) ((uint32_t *)xmem)[(size_t)y * XW + x] = fill(x, y);
	xpix = xmem; shadow = malloc((size_t)XW * XH * 4); memset(shadow, 0xFF, (size_t)XW * XH * 4);
	cursor_drawn = 0; cimg = NULL; scale_setup(); sync_screen();
}
static uint32_t px(int x, int y) { return ((uint32_t *)fbmem)[(size_t)y * fbw + x] & 0xFFFFFF; }
static uint32_t red(int x, int y) { return 0xFF0000; }
static uint32_t ramp(int x, int y) { return (uint32_t)(x * 255 / 1919) << 8; }   /* green ramp left to right */
static void tap(int ax, int ay)   /* a touchscreen with range 0..4095 */
{
	struct dev v; memset(&v, 0, sizeof v); v.touchscreen = 1; v.kind = 4; v.absx_max = 4095; v.absy_max = 4095;
	struct input_event e = {0}; e.type = EV_ABS; e.code = ABS_X; e.value = ax; handle_event(&v, &e); e.code = ABS_Y; e.value = ay; handle_event(&v, &e);
	e.type = EV_SYN; e.code = SYN_REPORT; e.value = 0; handle_event(&v, &e);
}
int main(void)
{
	char d[200];
	/* 1920x1080 desktop on a 1280x800 (16:10) screen: full width, bars above and below */
	setup(1280, 800, 1920, 1080, red);
	snprintf(d, sizeof d, "picture %dx%d at %d,%d", sdw, sdh, sx0, sy0);
	check("16:9 on 16:10 keeps its shape (1280x720, 40-pixel bars)", scaled && sdw == 1280 && sdh == 720 && sx0 == 0 && sy0 == 40, d);
	snprintf(d, sizeof d, "top bar %06x, picture %06x, bottom bar %06x", px(640, 10), px(640, 400), px(640, 790));
	check("black bars, the picture in between", px(640, 10) == 0 && px(640, 790) == 0 && px(640, 400) == 0xFF0000 && px(0, 40) == 0xFF0000 && px(1279, 759) == 0xFF0000, d);
	/* a smooth ramp stays smooth: no column jumps more than a step */
	setup(1280, 800, 1920, 1080, ramp);
	int worst = 0, prev = -1, mono = 1;
	for (int x = 0; x < 1280; x++) { int g = px(x, 400) >> 8 & 255; if (prev >= 0) { if (g < prev) mono = 0; if (g - prev > worst) worst = g - prev; } prev = g; }
	snprintf(d, sizeof d, "largest step %d, ends %d..%d", worst, px(0, 400) >> 8 & 255, px(1279, 400) >> 8 & 255);
	check("scaled picture is smooth (bilinear)", mono && worst <= 2 && (px(0, 400) >> 8 & 255) <= 1 && (px(1279, 400) >> 8 & 255) >= 254, d);
	/* 1024x576 on a 1024x768 (4:3) screen: bars above and below; 1366x768 on 1280x1024 (5:4) */
	setup(1024, 768, 1024, 576, red); snprintf(d, sizeof d, "%dx%d at %d,%d", sdw, sdh, sx0, sy0);
	check("16:9 on 4:3", sdw == 1024 && sdh == 576 && sy0 == 96, d);
	setup(1280, 1024, 1366, 768, red); snprintf(d, sizeof d, "%dx%d at %d,%d", sdw, sdh, sx0, sy0);
	check("1366x768 on 5:4", sdw == 1280 && sdh == 719 && sx0 == 0 && sy0 == 152, d);
	/* a 16:9 desktop on a narrower 21:9 screen: bars left and right */
	setup(2560, 1080, 1920, 1080, red); snprintf(d, sizeof d, "%dx%d at %d,%d; left bar %06x", sdw, sdh, sx0, sy0, px(100, 500));
	check("16:9 on 21:9: bars left and right", sdw == 1920 && sdh == 1080 && sx0 == 320 && px(100, 500) == 0 && px(1280, 500) == 0xFF0000, d);
	/* same size: nothing changes */
	setup(1280, 800, 1280, 800, red); check("same size is not scaled", !scaled && px(0, 0) == 0xFF0000, "");
	/* only changed rows are redrawn */
	setup(1280, 800, 1920, 1080, red);
	((uint32_t *)xmem)[500 * 1920 + 960] = 0x0000FF; memset(fbmem + (size_t)100 * fbstride, 0x11, 4);   /* mark a row far away */
	sync_screen();
	snprintf(d, sizeof d, "row near 500: %06x, untouched row 100: %08x", px(640, 40 + 333), ((uint32_t *)fbmem)[100 * 1280]);
	check("a change redraws only the rows it touches", ((uint32_t *)fbmem)[100 * 1280] == 0x11111111, d);
	/* the cursor: the X position is shown at its scaled place */
	setup(1280, 800, 1920, 1080, red);
	uint32_t ci[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF}; cimg = ci; cw = ch = 2; chx = chy = 0;
	curx = 960; cury = 540; draw_cursor();
	snprintf(d, sizeof d, "white at 640,400: %06x", px(640, 400));
	check("cursor drawn at the scaled position", px(640, 400) == 0xFFFFFF && px(960, 540) != 0xFFFFFF, d);
	restore_cursor_area(); check("cursor area restored from the scaled picture", px(640, 400) == 0xFF0000, "");
	cimg = NULL;
	/* touches: the panel's middle is the desktop's middle; a touch on a bar goes to the desktop's edge */
	tap(2047, 2047); snprintf(d, sizeof d, "-> %d,%d", absx, absy); check("touch in the middle", abs(absx - 960) <= 2 && abs(absy - 540) <= 2, d);
	tap(0, 0); snprintf(d, sizeof d, "-> %d,%d", absx, absy); check("touch on the top bar goes to the top edge", absx == 0 && absy == 0, d);
	tap(4095, 4095); snprintf(d, sizeof d, "-> %d,%d", absx, absy); check("touch on the bottom bar goes to the bottom edge", absx >= 1917 && absy == 1079, d);
	tap(4095 / 4, 4095 * 140 / 800); snprintf(d, sizeof d, "-> %d,%d", absx, absy); check("touch 100 px into the picture", abs(absx - 480) <= 2 && abs(absy - 150) <= 2, d);
	printf(fails ? "%d failed\n" : "all passed\n", fails);
	return fails != 0;
}
