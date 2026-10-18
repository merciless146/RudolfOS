/*
 * rudolf-display - shows the Rudolf OS X screen on the Linux framebuffer and
 * feeds keyboard, mouse, touchpad and touchscreen input into it.
 *
 * Xvfb renders the desktop into a memory-mapped XWD file (-fbdir). This
 * program copies changed bands of that image to /dev/fb0 about 60 times a
 * second, draws the current X cursor (via XFixes) on top, and turns evdev
 * events from /dev/input/event* into XTest events. It needs no GPU driver:
 * the firmware framebuffer (VESA/GOP via simpledrm) is enough.
 *
 *   rudolf-display --size          print "WIDTH HEIGHT" of /dev/fb0
 *   rudolf-display XWD-FILE        run (DISPLAY must point at the Xvfb)
 *   rudolf-display --input         run with a real X server (Xorg on the graphics card): only
 *                                  feed input and manage windows; Xorg draws screen and cursor
 *   rudolf-display --xsize         print "WIDTH HEIGHT" of the X screen
 *   rudolf-display --shot          write the X screen to stdout as a PPM image
 *   rudolf-display --modes         JSON: the screen modes and refresh rates of the monitor (Xorg)
 *   rudolf-display --clip-get      print the text on the clipboard (CLIPBOARD, shared by Chromium and Linux apps)
 *   rudolf-display --mode WxH|auto HZ   set the monitor's mode of that size with the nearest refresh rate (Xorg);
 *                                  exit status 3 when the monitor has no mode of that size (the session then
 *                                  uses the software display, which scales)
 *
 * The software display scales too: when Xvfb was started at another size than the framebuffer (Settings >
 * Display > Resolution), the picture is scaled smoothly and keeps its shape (black bars fill the rest).
 * RUDOLF_FPS (30 or 60) sets how often the screen is updated.
 *
 * Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <linux/kd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "rrcompat.h"

/* XTest / XFixes prototypes (headers are not needed at run time) */
extern int XTestFakeKeyEvent(Display *, unsigned int, Bool, unsigned long);
extern int XTestFakeButtonEvent(Display *, unsigned int, Bool, unsigned long);
extern int XTestFakeMotionEvent(Display *, int, int, int, unsigned long);
extern int XTestFakeRelativeMotionEvent(Display *, int, int, unsigned long);
typedef struct {
	short x, y;
	unsigned short width, height;
	unsigned short xhot, yhot;
	unsigned long cursor_serial;
	unsigned long *pixels;
	Atom atom;
	const char *name;
} XFixesCursorImage;
extern XFixesCursorImage *XFixesGetCursorImage(Display *);
extern void XFixesSelectCursorInput(Display *, Window, unsigned long);
extern Bool XFixesQueryExtension(Display *, int *, int *);

static int fbfd = -1, fbw, fbh, fbbpp, fbstride, fbswap;
static uint8_t *fbmem;
static size_t fblen;
static const uint8_t *xpix;          /* Xvfb pixels */
static int xw, xh, xstride, xbpp;
static uint8_t *shadow;              /* what the X image looked like, 32 bpp */
static Display *dpy;
static int ttyfd = -1;
static volatile int quit;
static int meta_down;
static void fit_windows(void);
static void wm_set(int game);
static void wm_cycle(void);

/* cursor */
static uint32_t *cimg; static int cw, ch, chx, chy;
static int curx = -1, cury = -1, drawn_x, drawn_y, drawn_w, drawn_h, cursor_drawn;
static double accel = 1.0, speed = 1.0;

/* scaling (software display): the X picture is shown in the rectangle sx0,sy0 sdw x sdh of the framebuffer,
   keeping its shape; the rest stays black. outb holds the scaled picture (framebuffer-sized). */
static int scaled, sx0, sy0, sdw, sdh;
static int *col_x, *row_y; static uint16_t *col_f, *row_f;
static uint32_t *outb; static uint8_t *srow_dirty;
/* where the X screen is on the physical panel, for touchscreens: the panel is pw x ph and shows the X screen
   in mx0,my0 mw x mh (the software display's scaling; Xorg shows it on the whole panel) */
static int pw, ph, mx0, my0, mw, mh;
static int frame_ms = 16;

static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

static void fb_open(void)
{
	struct fb_var_screeninfo v;
	struct fb_fix_screeninfo f;
	fbfd = open("/dev/fb0", O_RDWR);
	if (fbfd < 0) { perror("rudolf-display: /dev/fb0"); exit(2); }
	if (ioctl(fbfd, FBIOGET_VSCREENINFO, &v) || ioctl(fbfd, FBIOGET_FSCREENINFO, &f)) { perror("fb ioctl"); exit(2); }
	fbw = v.xres; fbh = v.yres; fbbpp = v.bits_per_pixel; fbstride = f.line_length;
	fbswap = fbbpp >= 24 && v.red.offset == 0;
	fblen = f.smem_len;
}

/* write one pixel (0x00RRGGBB) to a framebuffer row pointer */
static inline void put_px(uint8_t *row, int x, uint32_t c)
{
	if (fbswap) c = (c & 0xFF00) | (c >> 16 & 0xFF) | (c & 0xFF) << 16;
	switch (fbbpp) {
	case 32: ((uint32_t *)row)[x] = c; break;
	case 24: row[x * 3] = c; row[x * 3 + 1] = c >> 8; row[x * 3 + 2] = c >> 16; break;
	case 16: ((uint16_t *)row)[x] = ((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x1F); break;
	}
}

static void fb_row(int y, int x0, int x1, const uint32_t *src)
{
	if (y < 0 || y >= fbh) return;
	if (x1 > fbw) x1 = fbw;
	uint8_t *row = fbmem + (size_t)y * fbstride;
	if (fbbpp == 32 && !fbswap) { memcpy(row + x0 * 4, src + x0, (x1 - x0) * 4); return; }
	for (int x = x0; x < x1; x++) put_px(row, x, src[x]);
}

/* what is on the screen without the cursor, in framebuffer coordinates */
static inline int bg_w(void) { return scaled ? fbw : xw < fbw ? xw : fbw; }
static inline int bg_h(void) { return scaled ? fbh : xh < fbh ? xh : fbh; }
static inline const uint32_t *bg_row(int y) { return scaled ? outb + (size_t)y * fbw : (const uint32_t *)(shadow + (size_t)y * xw * 4); }

static void restore_cursor_area(void)
{
	if (!cursor_drawn) return;
	int W = bg_w(), H = bg_h();
	for (int y = drawn_y; y < drawn_y + drawn_h; y++) {
		if (y < 0 || y >= H) continue;
		int x0 = drawn_x < 0 ? 0 : drawn_x, x1 = drawn_x + drawn_w > W ? W : drawn_x + drawn_w;
		if (x0 < x1) fb_row(y, x0, x1, bg_row(y));
	}
	cursor_drawn = 0;
}

static void draw_cursor(void)
{
	if (!cimg || curx < 0) return;
	/* the pointer's place on the framebuffer; the cursor image itself is not scaled, so it stays sharp */
	int px = scaled ? sx0 + (int)((long)curx * sdw / xw) : curx, py = scaled ? sy0 + (int)((long)cury * sdh / xh) : cury;
	int ox = px - chx, oy = py - chy, W = bg_w(), H = bg_h();
	for (int j = 0; j < ch; j++) {
		int y = oy + j;
		if (y < 0 || y >= H) continue;
		const uint32_t *srow = bg_row(y);
		uint8_t *frow = fbmem + (size_t)y * fbstride;
		for (int i = 0; i < cw; i++) {
			int x = ox + i;
			if (x < 0 || x >= W) continue;
			uint32_t p = cimg[j * cw + i], a = p >> 24;
			if (!a) continue;
			uint32_t b = srow[x];
			/* XFixes pixels are premultiplied ARGB */
			uint32_t r = ((p >> 16) & 255) + (((b >> 16) & 255) * (255 - a)) / 255;
			uint32_t g = ((p >> 8) & 255) + (((b >> 8) & 255) * (255 - a)) / 255;
			uint32_t bl = (p & 255) + ((b & 255) * (255 - a)) / 255;
			put_px(frow, x, (r > 255 ? 255 : r) << 16 | (g > 255 ? 255 : g) << 8 | (bl > 255 ? 255 : bl));
		}
	}
	drawn_x = ox; drawn_y = oy; drawn_w = cw; drawn_h = ch; cursor_drawn = 1;
}

static void fetch_cursor(void)
{
	XFixesCursorImage *ci = XFixesGetCursorImage(dpy);
	if (!ci) return;
	free(cimg);
	cw = ci->width; ch = ci->height; chx = ci->xhot; chy = ci->yhot;
	cimg = malloc(sizeof(uint32_t) * (cw * ch ? cw * ch : 1));
	for (int i = 0; i < cw * ch; i++) cimg[i] = (uint32_t)ci->pixels[i];
	XFree(ci);
}

/* the scaling tables: for each framebuffer column and row inside the picture, the X pixel to its left/above and
   the weight (0..256) of the next one */
static void scale_axis(int n, int src, int *pos, uint16_t *w)
{
	for (int i = 0; i < n; i++) {
		double f = (i + 0.5) * src / n - 0.5; if (f < 0) f = 0;
		int p = (int)f; if (p > src - 2) { p = src - 2; f = src - 1; }
		pos[i] = p; w[i] = (uint16_t)((f - p) * 256 + 0.5);
	}
}
static void scale_setup(void)
{
	scaled = (xw != fbw || xh != fbh) && xw >= 2 && xh >= 2;
	pw = xw; ph = xh; mx0 = my0 = 0; mw = xw; mh = xh;
	if (!scaled) return;
	if ((long)fbw * xh <= (long)fbh * xw) { sdw = fbw; sdh = (int)((long)fbw * xh / xw); } else { sdh = fbh; sdw = (int)((long)fbh * xw / xh); }
	sx0 = (fbw - sdw) / 2; sy0 = (fbh - sdh) / 2;
	pw = fbw; ph = fbh; mx0 = sx0; my0 = sy0; mw = sdw; mh = sdh;
	col_x = malloc(sizeof(int) * sdw); col_f = malloc(2 * sdw); row_y = malloc(sizeof(int) * sdh); row_f = malloc(2 * sdh);
	scale_axis(sdw, xw, col_x, col_f); scale_axis(sdh, xh, row_y, row_f);
	outb = calloc((size_t)fbw * fbh, 4); srow_dirty = calloc(xh, 1);
	for (int y = 0; y < fbh; y++) fb_row(y, 0, fbw, outb + (size_t)y * fbw);   /* black bars */
	fprintf(stderr, "rudolf-display: %dx%d shown at %dx%d+%d+%d on the %dx%d screen\n", xw, xh, sdw, sdh, sx0, sy0, fbw, fbh);
}
/* blend two pixels: w = 0 (all a) .. 256 (all b) */
static inline uint32_t mix(uint32_t a, uint32_t b, uint32_t w)
{
	uint32_t rb = ((a & 0xFF00FF) * (256 - w) + (b & 0xFF00FF) * w) >> 8 & 0xFF00FF;
	uint32_t g = ((a & 0x00FF00) * (256 - w) + (b & 0x00FF00) * w) >> 8 & 0x00FF00;
	return rb | g;
}
/* one framebuffer row of the scaled picture (bilinear) */
static void scale_row(int j)
{
	int y0 = row_y[j]; uint32_t wy = row_f[j];
	const uint32_t *a = (const uint32_t *)(shadow + (size_t)y0 * xw * 4), *b = (const uint32_t *)(shadow + (size_t)(y0 + 1) * xw * 4);
	uint32_t *o = outb + (size_t)(sy0 + j) * fbw + sx0;
	for (int i = 0; i < sdw; i++) {
		int x = col_x[i]; uint32_t wx = col_f[i];
		o[i] = mix(mix(a[x], a[x + 1], wx), mix(b[x], b[x + 1], wx), wy);
	}
}
static int sync_scaled(void)
{
	int changed = 0, band = 8;
	memset(srow_dirty, 0, xh);
	for (int y0 = 0; y0 < xh; y0 += band) {
		int y1 = y0 + band > xh ? xh : y0 + band, dirty = 0;
		for (int y = y0; y < y1 && !dirty; y++) if (memcmp(xpix + (size_t)y * xstride, shadow + (size_t)y * xw * 4, (size_t)xw * 4)) dirty = 1;
		if (!dirty) continue;
		changed = 1;
		for (int y = y0; y < y1; y++) { memcpy(shadow + (size_t)y * xw * 4, xpix + (size_t)y * xstride, (size_t)xw * 4); srow_dirty[y] = 1; }
	}
	if (!changed) return 0;
	int hit_cursor = 0;
	for (int j = 0; j < sdh; j++) {
		int y = row_y[j];
		if (!srow_dirty[y] && !srow_dirty[y + 1]) continue;
		scale_row(j);
		int fy = sy0 + j;
		fb_row(fy, sx0, sx0 + sdw, outb + (size_t)fy * fbw);
		if (cursor_drawn && fy >= drawn_y && fy < drawn_y + drawn_h) hit_cursor = 1;
	}
	if (hit_cursor) { cursor_drawn = 0; draw_cursor(); }
	return 1;
}

/* copy changed 8-line bands from the X image to the framebuffer */
static int sync_screen(void)
{
	if (scaled) return sync_scaled();
	int changed = 0, band = 8;
	int hh = xh < fbh ? xh : fbh, ww = xw < fbw ? xw : fbw;
	for (int y0 = 0; y0 < hh; y0 += band) {
		int y1 = y0 + band > hh ? hh : y0 + band, dirty = 0;
		for (int y = y0; y < y1 && !dirty; y++) {
			const uint8_t *s = xpix + (size_t)y * xstride;
			if (memcmp(s, shadow + (size_t)y * xw * 4, (size_t)xw * 4)) dirty = 1;
		}
		if (!dirty) continue;
		changed = 1;
		for (int y = y0; y < y1; y++) {
			memcpy(shadow + (size_t)y * xw * 4, xpix + (size_t)y * xstride, (size_t)xw * 4);
			fb_row(y, 0, ww, (uint32_t *)(shadow + (size_t)y * xw * 4));
		}
		if (cursor_drawn && drawn_y < y1 && drawn_y + drawn_h > y0) cursor_drawn = 0, draw_cursor();
	}
	return changed;
}

/* ------------------------------------------------------------ input */
/* Every keyboard, mouse, touchpad and touchscreen in /dev/input is read here and passed to X through XTest.
 * Touchpads (laptops): the finger moves the pointer (relative, with sub-pixel precision), a tap clicks (one
 * finger: left, two: right, three: middle), two fingers scroll both ways, a click on a clickpad is a right
 * click with two fingers down or in the pad's bottom-right corner, and taps and motion are ignored for a moment
 * after typing (palm rest). Settings come from /run/rudolf/pointer (written by rudolfd from Settings):
 *   speed accel tap natural typing      e.g. "1.000 1.000 1 0 1"
 * Game controllers are left to games. The devices found are listed in /run/rudolf/inputs for Settings. */
#define MAXDEV 32
struct dev {
	int fd; char path[64]; char name[96];
	int touchpad, touchscreen, buttonpad, kind;   /* kind: 1 keyboard 2 mouse 3 touchpad 4 touchscreen 5 other */
	int absx_min, absx_max, absy_min, absy_max;
	int ax, ay, lastx, lasty, have_last, touching, fingers, maxf;
	long long touch_t; int moved, travel, clicked, pressed_btn;
	int scroll_x, scroll_y;
} devs[MAXDEV];
static int ndev;
static int tap_enabled = 1, natural_scroll = 0, typing_guard = 1;
static long long last_key_ms = -100000;
static double rem_x, rem_y;   /* fractions of a pixel not yet moved */

static long long now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000LL + t.tv_nsec / 1000000; }
#define TESTBIT(b, a) ((a[(b) / 8] >> ((b) % 8)) & 1)

static void write_inputs(void)
{
	static const char *const K[] = {"", "keyboard", "mouse", "touchpad", "touchscreen", "other"};
	FILE *f = fopen("/run/rudolf/inputs.new", "w");
	if (!f) return;
	for (int i = 0; i < ndev; i++) if (devs[i].kind >= 1 && devs[i].kind <= 4) fprintf(f, "%s|%s\n", K[devs[i].kind], devs[i].name);
	fclose(f); rename("/run/rudolf/inputs.new", "/run/rudolf/inputs");
}

/* what an input device is, from what it can report */
static int classify(struct dev *v, const uint8_t *evb, const uint8_t *keyb, const uint8_t *relb, const uint8_t *absb, const uint8_t *prop)
{
	if (TESTBIT(EV_KEY, evb) && (TESTBIT(BTN_GAMEPAD, keyb) || TESTBIT(BTN_JOYSTICK, keyb) || TESTBIT(BTN_TRIGGER_HAPPY1, keyb))) return 0;   /* controllers: games read them */
	if (TESTBIT(EV_ABS, evb) && TESTBIT(ABS_X, absb) && TESTBIT(ABS_Y, absb)) {
		int finger = TESTBIT(EV_KEY, evb) && TESTBIT(BTN_TOOL_FINGER, keyb);
		int touch = TESTBIT(EV_KEY, evb) && (TESTBIT(BTN_TOUCH, keyb) || TESTBIT(BTN_LEFT, keyb) || TESTBIT(BTN_STYLUS, keyb));
		if (finger && !TESTBIT(INPUT_PROP_DIRECT, prop)) { v->touchpad = 1; v->buttonpad = TESTBIT(INPUT_PROP_BUTTONPAD, prop); return 3; }
		if (touch) { v->touchscreen = 1; return 4; }   /* touchscreens, pens, and absolute pointers such as QEMU's tablet */
		return 0;
	}
	if (TESTBIT(EV_REL, evb) && TESTBIT(REL_X, relb)) return 2;
	if (TESTBIT(EV_KEY, evb) && TESTBIT(KEY_A, keyb) && TESTBIT(KEY_Z, keyb)) return 1;
	if (TESTBIT(EV_KEY, evb)) return 5;   /* power button, media keys... */
	return 0;
}

static void scan_devices(void)
{
	DIR *d = opendir("/dev/input");
	struct dirent *e;
	int added = 0;
	if (!d) return;
	while ((e = readdir(d))) {
		if (strncmp(e->d_name, "event", 5)) continue;
		char p[64]; snprintf(p, sizeof p, "/dev/input/%s", e->d_name);
		int known = 0;
		for (int i = 0; i < ndev; i++) if (!strcmp(devs[i].path, p)) known = 1;
		if (known || ndev >= MAXDEV) continue;
		int fd = open(p, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (fd < 0) continue;
		uint8_t evb[EV_MAX / 8 + 1] = {0}, keyb[KEY_MAX / 8 + 1] = {0}, relb[REL_MAX / 8 + 1] = {0}, absb[ABS_MAX / 8 + 1] = {0}, prop[INPUT_PROP_MAX / 8 + 1] = {0};
		ioctl(fd, EVIOCGBIT(0, sizeof evb), evb);
		ioctl(fd, EVIOCGBIT(EV_KEY, sizeof keyb), keyb);
		ioctl(fd, EVIOCGBIT(EV_REL, sizeof relb), relb);
		ioctl(fd, EVIOCGBIT(EV_ABS, sizeof absb), absb);
		ioctl(fd, EVIOCGPROP(sizeof prop), prop);
		struct dev nd; memset(&nd, 0, sizeof nd);
		nd.kind = classify(&nd, evb, keyb, relb, absb, prop);
		if (!nd.kind) { close(fd); continue; }
		nd.fd = fd; strcpy(nd.path, p);
		ioctl(fd, EVIOCGNAME(sizeof nd.name - 1), nd.name);
		for (char *c = nd.name; *c; c++) if (*c == '|' || *c == '\n') *c = ' ';
		if (nd.touchpad || nd.touchscreen) {
			struct input_absinfo ax, ay;
			ioctl(fd, EVIOCGABS(ABS_X), &ax); ioctl(fd, EVIOCGABS(ABS_Y), &ay);
			nd.absx_min = ax.minimum; nd.absx_max = ax.maximum; nd.absy_min = ay.minimum; nd.absy_max = ay.maximum;
		}
		devs[ndev++] = nd; added = 1;
	}
	closedir(d);
	if (added) write_inputs();
}

static void btn(int b, int down) { XTestFakeButtonEvent(dpy, b, down, CurrentTime); }
static void click(int b) { btn(b, 1); btn(b, 0); }

static void rel_move(double dx, double dy)
{
	double f = speed;
	double m = dx * dx + dy * dy;
	if (accel > 1.0 && m > 16) f *= accel;
	rem_x += dx * f; rem_y += dy * f;
	int ix = (int)rem_x, iy = (int)rem_y;   /* whole pixels now, the rest later: slow finger movement still moves */
	rem_x -= ix; rem_y -= iy;
	if (ix || iy) XTestFakeRelativeMotionEvent(dpy, ix, iy, CurrentTime);
}

static int typing_now(void) { return typing_guard && now_ms() - last_key_ms < 300; }

static void touchpad_key(struct dev *v, int c, int down)
{
	if (c == BTN_TOOL_FINGER || c == BTN_TOOL_DOUBLETAP || c == BTN_TOOL_TRIPLETAP || c == BTN_TOOL_QUADTAP) {
		int n = c == BTN_TOOL_FINGER ? 1 : c == BTN_TOOL_DOUBLETAP ? 2 : c == BTN_TOOL_TRIPLETAP ? 3 : 4;
		if (down) v->fingers = n; else if (v->fingers == n) v->fingers = n > 1 ? n - 1 : 0;
		if (v->fingers > v->maxf && v->touching) v->maxf = v->fingers;
		v->have_last = 0;   /* the reported position jumps when a finger is added or lifted */
		return;
	}
	if (c == BTN_TOUCH) {
		if (down) { v->touching = 1; v->touch_t = now_ms(); v->moved = 0; v->travel = 0; v->clicked = 0; v->have_last = 0; v->maxf = v->fingers > 0 ? v->fingers : 1; }
		else {
			v->touching = 0;
			if (tap_enabled && !v->moved && !v->clicked && now_ms() - v->touch_t < 220 && !typing_now())
				click(v->maxf >= 3 ? 2 : v->maxf == 2 ? 3 : 1);
			v->scroll_x = v->scroll_y = 0;
		}
		return;
	}
	if (c == BTN_LEFT || c == BTN_RIGHT || c == BTN_MIDDLE) {
		if (down) {
			int b = c == BTN_RIGHT ? 3 : c == BTN_MIDDLE ? 2 : 1;
			if (b == 1) {
				int rx = v->absx_max - v->absx_min, ry = v->absy_max - v->absy_min;
				int corner = v->buttonpad && rx > 0 && ry > 0 && v->ax > v->absx_min + rx * 3 / 5 && v->ay > v->absy_min + ry * 4 / 5;
				if (v->fingers >= 2 || corner) b = 3;
			}
			v->pressed_btn = b; v->clicked = 1; btn(b, 1);
		} else if (v->pressed_btn) { btn(v->pressed_btn, 0); v->pressed_btn = 0; }
	}
}

static void touchpad_frame(struct dev *v)
{
	if (!v->touching) return;
	if (v->have_last) {
		int dx = v->ax - v->lastx, dy = v->ay - v->lasty;
		int rx = v->absx_max - v->absx_min; if (rx <= 0) rx = 1000;
		v->travel += abs(dx) + abs(dy);
		if (v->travel > rx / 60 + 2) v->moved = 1;   /* a tap may wobble a little; more than that is a movement */
		if (!typing_now()) {
			if (v->fingers >= 2) {
				int step = rx / 25 + 1, dir = natural_scroll ? -1 : 1;
				v->scroll_y += dy * dir; v->scroll_x += dx * dir;
				while (v->scroll_y > step) { click(5); v->scroll_y -= step; }
				while (v->scroll_y < -step) { click(4); v->scroll_y += step; }
				while (v->scroll_x > step) { click(7); v->scroll_x -= step; }
				while (v->scroll_x < -step) { click(6); v->scroll_x += step; }
			} else if (!v->pressed_btn || v->fingers == 1) {
				double k = 1.4 * xw / rx;   /* a full pad width is about 1.4 screen widths */
				rel_move(dx * k, dy * k);
			}
		}
	}
	v->lastx = v->ax; v->lasty = v->ay; v->have_last = 1;
}

/* Chromium keeps some shortcuts for itself and acts on them before the page sees them: Ctrl+W and Ctrl+F4 close
   the desktop's window (the desktop starts again, open windows are lost), Ctrl+Q quits Chromium, Ctrl+T and Ctrl+N
   open browser windows over the desktop. While the desktop's Chromium window (class RudolfDesktop) has the
   keyboard, those keys arrive as F20 (W, F4), F21 (T), F22 (N) and F23 (Q), still with Ctrl held; the desktop and
   Oracle's real tabs read Ctrl+F20 as Ctrl+W and so on (shell26.js: DESK_SUB). Linux apps and the browser window
   for sign-in sites get the keys unchanged. */
static int mod_ctrl, mod_alt;
static unsigned char key_sub[256];
static int desk_focused(void)
{
	static long long at; static int was;
	long long t = now_ms();
	if (t - at < 250) return was;
	at = t; was = 0;
	Window w = None, r, p, *ch; int rv; unsigned n;
	XGetInputFocus(dpy, &w, &rv);
	for (int i = 0; i < 12 && w != None && w != PointerRoot; i++) {
		XClassHint h = {0};
		if (XGetClassHint(dpy, w, &h)) {
			was = h.res_class && !strcmp(h.res_class, "RudolfDesktop");   /* the session starts it with --class=RudolfDesktop */
			if (h.res_name) XFree(h.res_name);
			if (h.res_class) XFree(h.res_class);
			return was;
		}
		if (!XQueryTree(dpy, w, &r, &p, &ch, &n)) break;
		if (ch) XFree(ch);
		if (p == r) break;
		w = p;
	}
	return was;
}
static int desk_key(int c, int down)
{
	if (c == KEY_LEFTCTRL || c == KEY_RIGHTCTRL) mod_ctrl = down ? mod_ctrl | (c == KEY_LEFTCTRL ? 1 : 2) : mod_ctrl & ~(c == KEY_LEFTCTRL ? 1 : 2);
	if (c == KEY_LEFTALT || c == KEY_RIGHTALT) mod_alt = down ? mod_alt | (c == KEY_LEFTALT ? 1 : 2) : mod_alt & ~(c == KEY_LEFTALT ? 1 : 2);
	if (c >= 256) return c;
	if (!down) { int s = key_sub[c]; key_sub[c] = 0; return s ? s : c; }
	int s = 0;
	if (mod_ctrl && !mod_alt) switch (c) {
	case KEY_W: case KEY_F4: s = KEY_F20; break;
	case KEY_T: s = KEY_F21; break;
	case KEY_N: s = KEY_F22; break;
	case KEY_Q: s = KEY_F23; break;
	}
	if (s && desk_focused()) { key_sub[c] = s; return s; }
	return c;
}

static void handle_event(struct dev *v, struct input_event *e)
{
	if (e->type == EV_KEY) {
		int c = e->code, down = e->value != 0;
		if (e->value == 2) return; /* autorepeat: X repeats by itself */
		if (v->touchpad) { touchpad_key(v, c, down); return; }
		if (c == KEY_LEFTMETA || c == KEY_RIGHTMETA) meta_down = down;
		if (c == KEY_ESC && meta_down) {
			if (down) { wm_set(0); fit_windows(); }
			return; /* not passed on, so the game does not also see Esc */
		}
		if (c == KEY_TAB && meta_down) {
			if (down) { wm_cycle(); fit_windows(); }
			return;
		}
		if (c == KEY_POWER && down) { int f = open("/run/rudolf/events", O_WRONLY | O_APPEND | O_CREAT, 0644); if (f >= 0) { dprintf(f, "power %lld\n", now_ms()); close(f); } return; }
		if (v->touchscreen) {
			if (c == BTN_TOUCH) { btn(1, down); return; }
			if (c == BTN_TOOL_FINGER || c == BTN_TOOL_DOUBLETAP || c == BTN_TOOL_TRIPLETAP || c == BTN_TOOL_PEN || c == BTN_TOOL_RUBBER) return;
		}
		switch (c) {
		case BTN_LEFT: btn(1, down); return;
		case BTN_RIGHT: btn(3, down); return;
		case BTN_MIDDLE: btn(2, down); return;
		case BTN_SIDE: case BTN_BACK: btn(8, down); return;
		case BTN_EXTRA: case BTN_FORWARD: btn(9, down); return;
		}
		if (c < 256) {
			if (down && v->kind == 1 && c != KEY_LEFTCTRL && c != KEY_RIGHTCTRL && c != KEY_LEFTSHIFT && c != KEY_RIGHTSHIFT && c != KEY_LEFTALT && c != KEY_RIGHTALT && c != KEY_LEFTMETA && c != KEY_RIGHTMETA)
				last_key_ms = now_ms();
			XTestFakeKeyEvent(dpy, desk_key(c, down) + 8, down, CurrentTime);
		}
	} else if (e->type == EV_REL) {
		if (e->code == REL_X) rel_move(e->value, 0);
		else if (e->code == REL_Y) rel_move(0, e->value);
		else if (e->code == REL_WHEEL) { for (int i = 0; i < abs(e->value); i++) click(e->value > 0 ? 4 : 5); }
		else if (e->code == REL_HWHEEL) { for (int i = 0; i < abs(e->value); i++) click(e->value > 0 ? 7 : 6); }
	} else if (e->type == EV_ABS) {
		if (e->code == ABS_X) v->ax = e->value;
		else if (e->code == ABS_Y) v->ay = e->value;
	} else if (e->type == EV_SYN && e->code == SYN_REPORT) {
		if (v->touchscreen) {
			int rx = v->absx_max - v->absx_min, ry = v->absy_max - v->absy_min;
			if (rx > 0 && ry > 0) {
				/* the touch's place on the panel, then on the X screen (the panel may show it scaled, with bars) */
				long px = (long)(v->ax - v->absx_min) * (pw > 0 ? pw : xw) / rx, py = (long)(v->ay - v->absy_min) * (ph > 0 ? ph : xh) / ry;
				long X = mw > 0 ? (px - mx0) * xw / mw : px, Y = mh > 0 ? (py - my0) * xh / mh : py;
				if (X < 0) X = 0; if (X >= xw) X = xw - 1; if (Y < 0) Y = 0; if (Y >= xh) Y = xh - 1;
				XTestFakeMotionEvent(dpy, -1, (int)X, (int)Y, CurrentTime);
			}
		} else if (v->touchpad) touchpad_frame(v);
		XFlush(dpy);
	}
}

/* Xorg shows the X screen on the whole panel; the software display may scale it (scale_setup) */
static void read_map(void) { if (!scaled) { pw = xw; ph = xh; mx0 = my0 = 0; mw = xw; mh = xh; } }

static void read_settings(void)
{
	read_map();
	FILE *f = fopen("/run/rudolf/pointer", "r");
	if (!f) return;
	double s = 1, a = 1; int tap = 1, nat = 0, typ = 1;
	int n = fscanf(f, "%lf %lf %d %d %d", &s, &a, &tap, &nat, &typ);
	if (n >= 2 && s > 0.1 && s < 10) { speed = s; accel = a; }
	if (n >= 5) { tap_enabled = tap != 0; natural_scroll = nat != 0; typing_guard = typ != 0; }
	fclose(f);
}

static void on_sig(int s) { (void)s; quit = 1; }

/* There is no separate window manager; this program acts as a minimal one.
 *  - Chromium with --class=RudolfDesktop is the Rudolf OS desktop, kept exactly full-screen.
 *  - Every other top-level window is an app window: Linux apps (Flatpak), the browser window for
 *    sign-in sites (--class=RudolfWeb) and games such as Sober. One of them, or the desktop, is shown.
 *    Apps and the browser window leave the desktop's taskbar visible (/run/rudolf/strut); games cover
 *    the whole screen. Dialogs (WM_TRANSIENT_FOR) are centred above their window.
 *  - /run/rudolf/wm says what to show: "desktop", "win <hex window id>", or the older "game" / "web"
 *    (the topmost game or browser window). rudolfd writes it when the desktop asks; a new app window
 *    is shown by itself; Super+Esc goes to the desktop and Super+Tab steps through the windows.
 *  - /run/rudolf/windows lists the app windows (id, browser?, class, instance, title) for the taskbar;
 *    /run/rudolf/wmcmd "close <hex>" asks a window to close (WM_DELETE_WINDOW, as a close button does).
 * Pop-ups and menus (override-redirect) are left alone. */
static int wm_strut_top, wm_strut_bottom;
static long long wm_mtime, cmd_mtime;
static char wm_mode[48] = "desktop";

static int class_has(Window w, const char *a, const char *b)
{
	XClassHint ch = {0};
	int r = 0;
	if (XGetClassHint(dpy, w, &ch)) {
		r = (ch.res_class && (strcasestr(ch.res_class, a) || (b && strcasestr(ch.res_class, b)))) || (ch.res_name && (strcasestr(ch.res_name, a) || (b && strcasestr(ch.res_name, b))));
		if (ch.res_name) XFree(ch.res_name);
		if (ch.res_class) XFree(ch.res_class);
	}
	return r;
}
static int is_web(Window w) { return class_has(w, "rudolfweb", NULL); }
static int is_game(Window w) { return class_has(w, "sober", "roblox"); }

static void wm_write(const char *mode)
{
	int f = open("/run/rudolf/wm.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (f < 0) return;
	dprintf(f, "%s\n", mode); close(f);
	rename("/run/rudolf/wm.tmp", "/run/rudolf/wm");
	snprintf(wm_mode, sizeof wm_mode, "%s", mode);
	struct stat st; if (!stat("/run/rudolf/wm", &st)) wm_mtime = (long long)st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec;
}
static void wm_set(int game) { if (!game) wm_write("desktop"); }
static void wm_show(Window w) { char b[40]; snprintf(b, sizeof b, "win %lx", (unsigned long)w); wm_write(b); }
static long long mtime_ns(const char *p) { struct stat st; return stat(p, &st) ? -1 : (long long)st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec; }
/* true when /run/rudolf/wm or wmcmd changed since we last looked (checked often, so switching is quick) */
static int wm_changed(void) { return mtime_ns("/run/rudolf/wm") != wm_mtime || mtime_ns("/run/rudolf/wmcmd") != cmd_mtime; }

static void wm_read(void)
{
	long long m = mtime_ns("/run/rudolf/wm");
	if (m == wm_mtime) return;
	wm_mtime = m;
	char b[48] = {0};
	int f = open("/run/rudolf/wm", O_RDONLY); if (f < 0) return;
	if (read(f, b, sizeof b - 1) > 0) { b[strcspn(b, "\n")] = 0; snprintf(wm_mode, sizeof wm_mode, "%s", b); }
	close(f);
	/* the desktop's taskbar stays visible below (or above) app windows: "bottom 48" or "top 48" */
	char sb[32] = {0}; f = open("/run/rudolf/strut", O_RDONLY); wm_strut_top = wm_strut_bottom = 0;
	if (f >= 0) { if (read(f, sb, sizeof sb - 1) > 0) { int v = atoi(sb + (sb[0] == 't' ? 4 : 7)); if (v < 0 || v > 200) v = 0; if (sb[0] == 't') wm_strut_top = v; else if (sb[0] == 'b') wm_strut_bottom = v; } close(f); }
}

static void close_window(Window w)
{
	Atom proto = XInternAtom(dpy, "WM_PROTOCOLS", False), del = XInternAtom(dpy, "WM_DELETE_WINDOW", False), *list = NULL;
	int n = 0, polite = 0;
	if (XGetWMProtocols(dpy, w, &list, &n)) { for (int i = 0; i < n; i++) if (list[i] == del) polite = 1; XFree(list); }
	if (polite) {
		XEvent e = {0}; e.xclient.type = ClientMessage; e.xclient.window = w; e.xclient.message_type = proto; e.xclient.format = 32;
		e.xclient.data.l[0] = del; e.xclient.data.l[1] = CurrentTime; XSendEvent(dpy, w, False, NoEventMask, &e);
	} else XKillClient(dpy, w);
}
static void wm_commands(void)
{
	long long m = mtime_ns("/run/rudolf/wmcmd");
	if (m == cmd_mtime) return;
	cmd_mtime = m;
	char b[64] = {0}; int f = open("/run/rudolf/wmcmd", O_RDONLY); if (f < 0) return;
	if (read(f, b, sizeof b - 1) > 0 && !strncmp(b, "close ", 6)) { unsigned long x = strtoul(b + 6, NULL, 16); if (x) close_window((Window)x); }
	close(f); unlink("/run/rudolf/wmcmd"); cmd_mtime = -1;
}

static void window_title(Window w, char *out, size_t n)
{
	Atom net = XInternAtom(dpy, "_NET_WM_NAME", False), utf8 = XInternAtom(dpy, "UTF8_STRING", False), type; int fmt; unsigned long cnt, after; unsigned char *d = NULL;
	out[0] = 0;
	if (XGetWindowProperty(dpy, w, net, 0, 256, False, utf8, &type, &fmt, &cnt, &after, &d) == Success && d && cnt) snprintf(out, n, "%s", (char *)d);
	if (d) XFree(d);
	if (!out[0]) { char *nm = NULL; if (XFetchName(dpy, w, &nm) && nm) { snprintf(out, n, "%s", nm); XFree(nm); } }
	for (char *c = out; *c; c++) if (*c == '\t' || *c == '\n') *c = ' ';
}

#define MAXWIN 64
static char wm_last_class[128];
static Window known[MAXWIN]; static int nknown;
static Window cur_list[MAXWIN]; static int ncur;   /* app windows, bottom to top */

static void fit_windows(void)
{
	Window root = DefaultRootWindow(dpy), r, p, *kids = NULL, desktop = 0, chromium = 0, tr;
	unsigned n = 0;
	int sw = DisplayWidth(dpy, DefaultScreen(dpy)), sh = DisplayHeight(dpy, DefaultScreen(dpy)), ax = 0, ay = 0;
	wm_read(); wm_commands();
	if (!XQueryTree(dpy, root, &r, &p, &kids, &n)) return;
	Window apps[MAXWIN]; int napps = 0, fresh = -1;
	/* the desktop first: the RudolfDesktop window, or (older sessions) the first Chromium */
	for (unsigned i = 0; i < n; i++) {
		XWindowAttributes wa;
		if (!XGetWindowAttributes(dpy, kids[i], &wa) || wa.override_redirect || wa.map_state != IsViewable) continue;
		if (class_has(kids[i], "rudolfdesktop", NULL)) { desktop = kids[i]; break; }
		if (!chromium && !is_web(kids[i]) && class_has(kids[i], "chrom", NULL)) chromium = kids[i];
	}
	if (!desktop) desktop = chromium;
	/* XQueryTree lists children bottom to top */
	for (unsigned i = 0; i < n; i++) {
		XWindowAttributes wa;
		if (!XGetWindowAttributes(dpy, kids[i], &wa) || wa.override_redirect || wa.map_state != IsViewable) continue;
		if (kids[i] == desktop) {
			if (wa.x != ax || wa.y != ay || wa.width != sw || wa.height != sh) XMoveResizeWindow(dpy, kids[i], ax, ay, sw, sh);
			continue;
		}
		if (XGetTransientForHint(dpy, kids[i], &tr) && tr) {
			/* dialog: centre it */
			int x = ax + (sw - wa.width) / 2, y = ay + (sh - wa.height) / 2;
			if (wa.width < sw && wa.height < sh && (abs(wa.x - x) > 1 || abs(wa.y - y) > 1)) XMoveWindow(dpy, kids[i], x < ax ? ax : x, y < ay ? ay : y);
			continue;
		}
		if (napps >= MAXWIN) continue;
		apps[napps++] = kids[i];
		int seen = 0; for (int k = 0; k < nknown; k++) if (known[k] == kids[i]) seen = 1;
		if (!seen) fresh = napps - 1;
		if (is_game(kids[i])) { if (wa.x != ax || wa.y != ay || wa.width != sw || wa.height != sh) XMoveResizeWindow(dpy, kids[i], ax, ay, sw, sh); }
		else {
			int y = ay + wm_strut_top, h = sh - wm_strut_top - wm_strut_bottom;
			if (wa.x != ax || wa.y != y || wa.width != sw || wa.height != h) XMoveResizeWindow(dpy, kids[i], ax, y, sw, h);
		}
	}
	memcpy(known, apps, napps * sizeof(Window)); nknown = napps;
	memcpy(cur_list, apps, napps * sizeof(Window)); ncur = napps;
	/* what to show */
	Window want = 0;
	if (fresh >= 0) { want = apps[fresh]; wm_show(want); }   /* a new app window appeared: show it */
	else if (!strncmp(wm_mode, "win ", 4)) {
		Window w = (Window)strtoul(wm_mode + 4, NULL, 16);
		for (int k = 0; k < napps; k++) if (apps[k] == w) want = w;
		if (!want) {
			/* the shown window went away: another window of the same app, else the desktop */
			for (int k = napps; k-- > 0;) { XClassHint ch = {0}; if (XGetClassHint(dpy, apps[k], &ch)) { if (ch.res_class && wm_last_class[0] && !strcmp(ch.res_class, wm_last_class)) want = apps[k]; if (ch.res_name) XFree(ch.res_name); if (ch.res_class) XFree(ch.res_class); } if (want) break; }
			if (want) wm_show(want); else wm_write("desktop");
		} else { XClassHint ch = {0}; if (XGetClassHint(dpy, want, &ch)) { if (ch.res_class) snprintf(wm_last_class, sizeof wm_last_class, "%s", ch.res_class); if (ch.res_name) XFree(ch.res_name); if (ch.res_class) XFree(ch.res_class); } }
	} else if (!strcmp(wm_mode, "game") || !strcmp(wm_mode, "web")) {
		int webm = !strcmp(wm_mode, "web");
		for (int k = napps; k-- > 0;) if (is_web(apps[k]) == webm) { want = apps[k]; break; }
	}
	if (!want) want = desktop;
	/* the list for the taskbar, rewritten when it changes */
	{
		static char last[8192]; char buf[8192]; size_t o = 0;
		for (int k = 0; k < napps && o < sizeof buf - 600; k++) {
			XClassHint ch = {0}; char t[256]; window_title(apps[k], t, sizeof t);
			XGetClassHint(dpy, apps[k], &ch);
			o += snprintf(buf + o, sizeof buf - o, "%lx\t%d\t%s\t%s\t%s\n", (unsigned long)apps[k], is_web(apps[k]), ch.res_class ? ch.res_class : "", ch.res_name ? ch.res_name : "", t);
			if (ch.res_name) XFree(ch.res_name); if (ch.res_class) XFree(ch.res_class);
		}
		buf[o] = 0;
		if (strcmp(buf, last)) {
			memcpy(last, buf, o + 1);
			int f = open("/run/rudolf/windows.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (f >= 0) { if (write(f, buf, o) < 0) {} close(f); rename("/run/rudolf/windows.tmp", "/run/rudolf/windows"); }
		}
	}
	Window top = 0;
	for (unsigned i = n; i-- > 0;) {                  /* topmost viewable normal window */
		XWindowAttributes wa;
		if (XGetWindowAttributes(dpy, kids[i], &wa) && !wa.override_redirect && wa.map_state == IsViewable) { top = kids[i]; break; }
	}
	static Window last_want;
	if (want) {
		Window owner;
		/* a dialog of the shown window, on top of it, keeps its place and the keyboard */
		int dialog = top && top != want && XGetTransientForHint(dpy, top, &owner) && owner == want;
		if (!dialog && top != want) XRaiseWindow(dpy, want);
		if (!dialog && (want != last_want || top != want)) XSetInputFocus(dpy, want, RevertToPointerRoot, CurrentTime);
	}
	last_want = want;
	if (kids) XFree(kids);
	XFlush(dpy);
}
/* Super+Tab: the desktop, then each app window in turn */
static void wm_cycle(void)
{
	fit_windows();
	if (!ncur) { wm_write("desktop"); return; }
	if (strncmp(wm_mode, "win ", 4)) { wm_show(cur_list[ncur - 1]); return; }
	Window w = (Window)strtoul(wm_mode + 4, NULL, 16); int at = -1;
	for (int k = 0; k < ncur; k++) if (cur_list[k] == w) at = k;
	if (at <= 0) wm_write("desktop"); else wm_show(cur_list[at - 1]);
}

/* windows come and go between our calls (BadWindow and the like): never fatal */
static int x_error(Display *d, XErrorEvent *e) { (void)d; (void)e; return 0; }
static Display *open_display(void)
{
	Display *d = NULL;
	for (int i = 0; i < 100 && !(d = XOpenDisplay(NULL)); i++) usleep(100000);
	if (d) XSetErrorHandler(x_error);
	if (!d) fprintf(stderr, "rudolf-display: cannot open X display\n");
	return d;
}

/* PPM of the root window (used by rudolfd's screenshot when Xorg draws the screen) */
static int shot(void)
{
	if (!(dpy = open_display())) return 2;
	int w = DisplayWidth(dpy, DefaultScreen(dpy)), h = DisplayHeight(dpy, DefaultScreen(dpy));
	XImage *im = XGetImage(dpy, DefaultRootWindow(dpy), 0, 0, w, h, AllPlanes, ZPixmap);
	if (!im) return 2;
	printf("P6\n%d %d\n255\n", w, h);
	uint8_t *row = malloc((size_t)w * 3);
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			unsigned long c = XGetPixel(im, x, y);
			row[x * 3] = c >> 16; row[x * 3 + 1] = c >> 8; row[x * 3 + 2] = c;
		}
		fwrite(row, 3, w, stdout);
	}
	return 0;
}

/* ------------------------------------------------------------ screen modes (Xorg, RandR) */
static double mode_hz(const XRRModeInfo *m)
{
	double v = m->vTotal;
	if (m->modeFlags & RR_DoubleScan) v *= 2;
	if (m->modeFlags & RR_Interlace) v /= 2;
	return m->hTotal && v > 0 ? (double)m->dotClock / (m->hTotal * v) : 0;
}
static const XRRModeInfo *mode_info(XRRScreenResources *r, RRMode id) { for (int i = 0; i < r->nmode; i++) if (r->modes[i].id == id) return &r->modes[i]; return NULL; }
/* the monitor: the first connected output (one that is lit, if any) */
static XRROutputInfo *rr_output(XRRScreenResources *r, RROutput *out)
{
	XRROutputInfo *best = NULL;
	for (int i = 0; i < r->noutput; i++) {
		XRROutputInfo *o = XRRGetOutputInfo(dpy, r, r->outputs[i]);
		if (!o) continue;
		if (o->connection == RR_Connected && o->nmode > 0 && (!best || (o->crtc && !best->crtc))) { if (best) XRRFreeOutputInfo(best); best = o; *out = r->outputs[i]; }
		else XRRFreeOutputInfo(o);
	}
	return best;
}
/* the monitor's own (preferred) mode */
static const XRRModeInfo *rr_native(XRRScreenResources *r, XRROutputInfo *o)
{
	const XRRModeInfo *n = NULL;
	if (o->npreferred > 0) n = mode_info(r, o->modes[0]);
	for (int i = 0; !n && i < o->nmode; i++) { const XRRModeInfo *m = mode_info(r, o->modes[i]); if (m && (!n || (long)m->width * m->height > (long)n->width * n->height)) n = m; }
	return n;
}
static int rr_open(XRRScreenResources **r, XRROutputInfo **o, RROutput *out)
{
	int ev, er;
	if (!(dpy = open_display())) return 2;
	if (!XRRQueryExtension(dpy, &ev, &er)) { fprintf(stderr, "rudolf-display: no RandR\n"); return 2; }
	/* the server accepts transforms (scaling) only from clients that said they speak RandR 1.3 or later */
	int ma = 1, mi = 5; XRRQueryVersion(dpy, &ma, &mi);
	if (ma < 1 || (ma == 1 && mi < 3)) fprintf(stderr, "rudolf-display: RandR %d.%d cannot scale\n", ma, mi);
	*r = XRRGetScreenResourcesCurrent(dpy, DefaultRootWindow(dpy));
	if (!*r || !(*o = rr_output(*r, out))) { fprintf(stderr, "rudolf-display: no monitor found\n"); return 2; }
	return 0;
}
/* --modes: {"output":..,"native":"WxH","current":{"w":..,"h":..,"hz":..},"modes":[{"w":..,"h":..,"hz":[..]},..]} */
static int list_modes(void)
{
	XRRScreenResources *r; XRROutputInfo *o; RROutput out;
	if (rr_open(&r, &o, &out)) return 2;
	const XRRModeInfo *n = rr_native(r, o), *cur = NULL;
	if (o->crtc) { XRRCrtcInfo *c = XRRGetCrtcInfo(dpy, r, o->crtc); if (c) { cur = mode_info(r, c->mode); XRRFreeCrtcInfo(c); } }
	printf("{\"ok\":true,\"output\":\"%.40s\",\"native\":\"%ux%u\"", o->name, n ? n->width : 0, n ? n->height : 0);
	if (cur) printf(",\"current\":{\"w\":%u,\"h\":%u,\"hz\":%.2f}", cur->width, cur->height, mode_hz(cur));
	printf(",\"modes\":[");
	int first = 1;
	for (int i = 0; i < o->nmode; i++) {
		const XRRModeInfo *m = mode_info(r, o->modes[i]); if (!m) continue;
		int dup = 0; for (int k = 0; k < i; k++) { const XRRModeInfo *p = mode_info(r, o->modes[k]); if (p && p->width == m->width && p->height == m->height) dup = 1; }
		if (dup) continue;
		printf("%s{\"w\":%u,\"h\":%u,\"hz\":[", first ? "" : ",", m->width, m->height); first = 0;
		int f2 = 1;
		for (int k = i; k < o->nmode; k++) { const XRRModeInfo *p = mode_info(r, o->modes[k]); if (p && p->width == m->width && p->height == m->height) { printf("%s%.2f", f2 ? "" : ",", mode_hz(p)); f2 = 0; } }
		printf("]}");
	}
	printf("]}\n");
	return 0;
}
static int mode_errors;
static int mode_error(Display *d, XErrorEvent *e)
{
	char t[120]; XGetErrorText(d, e->error_code, t, sizeof t);
	fprintf(stderr, "rudolf-display: --mode: X error %s (request %d.%d)\n", t, e->request_code, e->minor_code); mode_errors++; return 0;
}
static void write_file(const char *path, const char *text) { FILE *f = fopen(path, "w"); if (f) { fputs(text, f); fclose(f); } }
/* --mode WxH|auto HZ: the monitor's mode of that size with the nearest refresh rate. Writes
   /run/rudolf/screen-actual. A size the monitor does not have is not scaled here (Xorg's scaling is not
   dependable on every graphics card): exit status 3, and the session uses the software display instead. */
static int set_mode(const char *res, double hz)
{
	XRRScreenResources *r; XRROutputInfo *o; RROutput out;
	if (rr_open(&r, &o, &out)) return 2;
	const XRRModeInfo *n = rr_native(r, o), *pick = NULL;
	if (!n) return 2;
	unsigned W = 0, H = 0;
	if (!res || sscanf(res, "%ux%u", &W, &H) != 2 || W < 320 || H < 200 || W > 8192 || H > 8192) { W = n->width; H = n->height; }
	if (hz < 10 || hz > 500) hz = 60;
	for (int i = 0; i < o->nmode; i++) {
		const XRRModeInfo *m = mode_info(r, o->modes[i]);
		if (m && m->width == W && m->height == H && !(m->modeFlags & RR_Interlace) && (!pick || fabs(mode_hz(m) - hz) < fabs(mode_hz(pick) - hz))) pick = m;
	}
	if (!pick) { fprintf(stderr, "rudolf-display: the monitor has no %ux%u mode\n", W, H); return 3; }
	RRCrtc crtc = o->crtc ? o->crtc : o->ncrtc ? o->crtcs[0] : 0;
	if (!crtc) { fprintf(stderr, "rudolf-display: the monitor has no display controller\n"); return 2; }
	XSetErrorHandler(mode_error);
	XGrabServer(dpy);
	XRRSetCrtcConfig(dpy, r, crtc, CurrentTime, 0, 0, None, RR_Rotate_0, NULL, 0);
	unsigned long mmw = o->mm_width ? o->mm_width : W * 254 / 960, mmh = o->mm_height ? o->mm_height : H * 254 / 960;
	XRRSetScreenSize(dpy, DefaultRootWindow(dpy), W, H, (int)mmw, (int)mmh);
	Status st = XRRSetCrtcConfig(dpy, r, crtc, CurrentTime, 0, 0, pick->id, RR_Rotate_0, &out, 1);
	XSetWindowBackground(dpy, DefaultRootWindow(dpy), BlackPixel(dpy, DefaultScreen(dpy)));
	XClearWindow(dpy, DefaultRootWindow(dpy));
	XUngrabServer(dpy); XSync(dpy, False);
	char b[160];
	snprintf(b, sizeof b, "%u %u %.2f native %ux%u\n", W, H, mode_hz(pick), pick->width, pick->height); write_file("/run/rudolf/screen-actual", b);
	printf("%s", b);
	return st == 0 && !mode_errors ? 0 : 1;
}

/* --clip-get: ask the clipboard's owner for its text (UTF8_STRING) and print it */
static int clip_get(void)
{
	if (!(dpy = open_display())) return 2;
	Atom clip = XInternAtom(dpy, "CLIPBOARD", False), utf8 = XInternAtom(dpy, "UTF8_STRING", False), prop = XInternAtom(dpy, "RUDOLF_CLIP", False);
	if (XGetSelectionOwner(dpy, clip) == None) return 0;          /* nothing copied */
	Window w = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 1, 1, 0, 0, 0);
	XConvertSelection(dpy, clip, utf8, prop, w, CurrentTime); XFlush(dpy);
	for (int i = 0; i < 200; i++) {                               /* up to 2 seconds */
		while (XPending(dpy)) {
			XEvent e; XNextEvent(dpy, &e);
			if (e.type != SelectionNotify || e.xselection.requestor != w) continue;
			if (e.xselection.property == None) return 0;
			Atom type; int fmt; unsigned long n, left; unsigned char *data = NULL;
			if (XGetWindowProperty(dpy, w, prop, 0, 4 << 20, False, AnyPropertyType, &type, &fmt, &n, &left, &data) == Success && data) {
				fwrite(data, 1, n * (fmt / 8), stdout); XFree(data);
			}
			return 0;
		}
		usleep(10000);
	}
	return 1;
}

int main(int argc, char **argv)
{
	if (argc > 1 && !strcmp(argv[1], "--size")) { fb_open(); printf("%d %d\n", fbw, fbh); return 0; }
	if (argc > 1 && !strcmp(argv[1], "--xsize")) {
		if (!(dpy = open_display())) return 2;
		printf("%d %d\n", DisplayWidth(dpy, DefaultScreen(dpy)), DisplayHeight(dpy, DefaultScreen(dpy))); return 0;
	}
	if (argc > 1 && !strcmp(argv[1], "--shot")) return shot();
	if (argc > 1 && !strcmp(argv[1], "--modes")) return list_modes();
	if (argc > 1 && !strcmp(argv[1], "--clip-get")) return clip_get();
	if (argc > 3 && !strcmp(argv[1], "--mode")) return set_mode(argv[2], atof(argv[3]));
	{ const char *e = getenv("RUDOLF_FPS"); int f = e ? atoi(e) : 0; if (f >= 10 && f <= 240) frame_ms = 1000 / f; }
	if (argc < 2) { fprintf(stderr, "usage: rudolf-display XWD-FILE | --input | --size | --xsize | --shot\n"); return 1; }
	int input_only = !strcmp(argv[1], "--input");
	int fev = 0, ferr;
	if (input_only) {
		if (!(dpy = open_display())) return 2;
		xw = DisplayWidth(dpy, DefaultScreen(dpy)); xh = DisplayHeight(dpy, DefaultScreen(dpy));
	} else {
		fb_open();
		fbmem = mmap(NULL, fblen, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
		if (fbmem == MAP_FAILED) { perror("mmap fb"); return 2; }
		int xf = -1;
		for (int i = 0; i < 200 && xf < 0; i++) { xf = open(argv[1], O_RDONLY); if (xf < 0) usleep(50000); }
		if (xf < 0) { perror(argv[1]); return 2; }
		struct stat st; fstat(xf, &st);
		const uint8_t *xmap = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, xf, 0);
		if (xmap == MAP_FAILED) { perror("mmap xwd"); return 2; }
		uint32_t hsize = be32(xmap), ncol = be32(xmap + 19 * 4);
		xw = be32(xmap + 16); xh = be32(xmap + 20); xbpp = be32(xmap + 44); xstride = be32(xmap + 48);
		if (xbpp != 32) { fprintf(stderr, "rudolf-display: Xvfb must use depth 24 (32 bpp), got %d\n", xbpp); return 2; }
		xpix = xmap + hsize + ncol * 12;
		shadow = calloc((size_t)xw * xh, 4);
		memset(shadow, 0xFF, (size_t)xw * xh * 4); /* force first full copy */
		scale_setup();
		if (!(dpy = open_display())) return 2;
		if (XFixesQueryExtension(dpy, &fev, &ferr)) XFixesSelectCursorInput(dpy, DefaultRootWindow(dpy), 1);
		fetch_cursor();
		/* keep the kernel console from drawing over us or eating keys (Xorg does this itself) */
		ttyfd = open("/dev/tty1", O_RDWR | O_CLOEXEC);
		if (ttyfd >= 0) { ioctl(ttyfd, KDSETMODE, KD_GRAPHICS); ioctl(ttyfd, KDSKBMODE, K_OFF); }
	}
	signal(SIGTERM, on_sig); signal(SIGINT, on_sig); signal(SIGHUP, SIG_IGN);

	long long last_scan = 0, last_set = 0, last_wmchk = 0, last_change = now_ms();
	int win_events = 0;
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureNotifyMask);
	while (!quit) {
		long long t = now_ms();
		if (t - last_scan > 2000) { scan_devices(); last_scan = t; }
		if (t - last_set > 1000) { read_settings(); fit_windows(); last_set = t; }
		else if (t - last_wmchk > 100) { last_wmchk = t; if (wm_changed()) fit_windows(); }
		struct pollfd pf[MAXDEV + 1];
		for (int i = 0; i < ndev; i++) pf[i].fd = devs[i].fd, pf[i].events = POLLIN;
		pf[ndev].fd = ConnectionNumber(dpy); pf[ndev].events = POLLIN;
		int idle = t - last_change > 1000;
		poll(pf, ndev + 1, input_only ? 250 : idle ? (frame_ms > 30 ? frame_ms : 30) : frame_ms);
		for (int i = 0; i < ndev; i++) {
			if (!(pf[i].revents & (POLLIN | POLLERR | POLLHUP))) continue;
			struct input_event ev[64];
			ssize_t n = read(devs[i].fd, ev, sizeof ev);
			if (n < 0 && errno != EAGAIN) { close(devs[i].fd); devs[i] = devs[--ndev]; i--; write_inputs(); continue; }
			for (int k = 0; k < n / (ssize_t)sizeof ev[0]; k++) handle_event(&devs[i], &ev[k]);
		}
		while (XPending(dpy)) {
			XEvent xe; XNextEvent(dpy, &xe);
			if (xe.type == MapNotify || xe.type == UnmapNotify || xe.type == DestroyNotify) win_events = 1;
			if (fev && xe.type == fev + 1) { restore_cursor_area(); fetch_cursor(); draw_cursor(); }
		}
		if (win_events) { win_events = 0; fit_windows(); }
		if (input_only) {
			/* the screen size can change (another monitor, a mode set by Xorg) */
			xw = DisplayWidth(dpy, DefaultScreen(dpy)); xh = DisplayHeight(dpy, DefaultScreen(dpy));
			continue;
		}
		Window r, c; int rx, ry, wx, wy; unsigned m;
		if (XQueryPointer(dpy, DefaultRootWindow(dpy), &r, &c, &rx, &ry, &wx, &wy, &m) && (rx != curx || ry != cury)) {
			restore_cursor_area(); curx = rx; cury = ry; draw_cursor(); last_change = t;
		}
		/* the screen is updated at most frame_ms apart (Settings > Display > Frame rate) */
		static long long last_sync;
		long long t2 = now_ms();
		if (t2 - last_sync >= frame_ms - 1) { last_sync = t2; if (sync_screen()) last_change = t; }
	}
	if (ttyfd >= 0) { ioctl(ttyfd, KDSKBMODE, K_UNICODE); ioctl(ttyfd, KDSETMODE, KD_TEXT); }
	return 0;
}
