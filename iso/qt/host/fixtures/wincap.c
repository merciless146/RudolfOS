/* 1.6.0 (the owner's Vivobook report of 2026-11-16: an app's window showed Rudolf's sign-in screen, or the desktop page at a large
 * size, before it shrank to the app): the native shell's window of an app, watched from the moment the shell makes it.
 *   wincap STATE APP MS N OUTDIR
 * STATE is rudolf-native's RUDOLF_NATIVE_STATE file; the window is the first "client ... app=APP" line in it (the test opens one
 * window of APP at a time). Every MS ms: whether its frame is on the screen (viewable) and its size, written to OUTDIR/log as
 * "I T V W H" (T: ms since the window was made); while it is on the screen, its picture as ImageMagick's import takes it (the frame,
 * then each of its windows on the screen with its own pixels: a page's window has 32-bit pixels, which the screen's own picture
 * leaves black under Xvfb) as OUTDIR/I.ppm, N pictures at most, and its outline on the log's line ("S" and 12 x 8 cells' mean
 * colours in hex, below its top 12 %: what a test in QEMU reads over the serial line; WINCAP_NOPPM=1: no pictures). Ends after N
 * pictures, or 60 s. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static int quiet(Display *d, XErrorEvent *e) { (void)d; (void)e; return 0; }
static double now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }
static unsigned long frame_of(const char *state, const char *app)
{
	FILE *f = fopen(state, "r"); if (!f) return 0; char l[2048], want[160]; unsigned long fr = 0; snprintf(want, sizeof want, " app=%s ", app);
	while (fgets(l, sizeof l, f)) { if (strncmp(l, "client ", 7) || !strstr(l, want)) continue; unsigned long w, x; if (sscanf(l + 7, "%lx %lx", &w, &x) == 2) { fr = x; break; } }
	fclose(f); return fr;
}
static void paste(Display *d, Window w, int ox, int oy, int W, int H, unsigned char *rgb)
{
	XWindowAttributes a; if (!XGetWindowAttributes(d, w, &a) || a.map_state != IsViewable) return;
	int x0 = ox < 0 ? -ox : 0, y0 = oy < 0 ? -oy : 0, x1 = a.width, y1 = a.height; if (ox + x1 > W) x1 = W - ox; if (oy + y1 > H) y1 = H - oy;
	if (x1 > x0 && y1 > y0) { XImage *im = XGetImage(d, w, x0, y0, x1 - x0, y1 - y0, AllPlanes, ZPixmap);
		if (im) { int rs = __builtin_ctzl(im->red_mask), gs = __builtin_ctzl(im->green_mask), bs = __builtin_ctzl(im->blue_mask);
			for (int y = 0; y < y1 - y0; y++) for (int x = 0; x < x1 - x0; x++) { unsigned long p = XGetPixel(im, x, y); unsigned char *o = rgb + 3 * ((size_t)(oy + y0 + y) * W + ox + x0 + x);
				o[0] = (p & im->red_mask) >> rs; o[1] = (p & im->green_mask) >> gs; o[2] = (p & im->blue_mask) >> bs; }
			XDestroyImage(im); } }
	Window r, p, *k = NULL; unsigned n = 0;
	if (XQueryTree(d, w, &r, &p, &k, &n)) { for (unsigned i = 0; i < n; i++) { XWindowAttributes c; if (XGetWindowAttributes(d, k[i], &c)) paste(d, k[i], ox + c.x + c.border_width, oy + c.y + c.border_width, W, H, rgb); } if (k) XFree(k); }
}
int main(int argc, char **argv)
{
	if (argc < 6) { fprintf(stderr, "wincap STATE APP MS N OUTDIR\n"); return 2; }
	Display *d = XOpenDisplay(NULL); if (!d) return 2; XSetErrorHandler(quiet);
	int ms = atoi(argv[3]), n = atoi(argv[4]), got = 0, i = 0; double t0 = now_ms(); unsigned long fr = 0;
	while (!(fr = frame_of(argv[1], argv[2]))) { if (now_ms() - t0 > 60000) return 1; struct timespec s = { 0, 3000000 }; nanosleep(&s, NULL); }
	char lp[1024]; snprintf(lp, sizeof lp, "%s/log", argv[5]); FILE *lg = fopen(lp, "w"); if (!lg) return 2;
	t0 = now_ms();
	while (got < n && now_ms() - t0 < 60000) {
		double t = now_ms(); XWindowAttributes a; int ok = XGetWindowAttributes(d, (Window)fr, &a), v = ok && a.map_state == IsViewable;
		fprintf(lg, "%d %.0f %d %d %d", i, t - t0, v, ok ? a.width : 0, ok ? a.height : 0);
		if (!ok) { fputc('\n', lg); break; }
		if (v) { unsigned char *rgb = calloc((size_t)a.width * a.height, 3); paste(d, (Window)fr, 0, 0, a.width, a.height, rgb);
			int y0 = a.height * 12 / 100; fputs(" S ", lg);
			for (int cy = 0; cy < 8; cy++) for (int cx = 0; cx < 12; cx++) { double m[3] = { 0, 0, 0 }; int n = 0;
				for (int y = y0 + cy * (a.height - y0) / 8; y < y0 + (cy + 1) * (a.height - y0) / 8; y++) for (int x = cx * a.width / 12; x < (cx + 1) * a.width / 12; x++) { unsigned char *p = rgb + 3 * ((size_t)y * a.width + x); m[0] += p[0]; m[1] += p[1]; m[2] += p[2]; n++; }
				for (int c = 0; c < 3; c++) fprintf(lg, "%02x", n ? (int)(m[c] / n) : 0); }
			if (!getenv("WINCAP_NOPPM")) { char pp[1024]; snprintf(pp, sizeof pp, "%s/%d.ppm", argv[5], i); FILE *o = fopen(pp, "wb");
				if (o) { fprintf(o, "P6\n%d %d\n255\n", a.width, a.height); fwrite(rgb, 3, (size_t)a.width * a.height, o); fclose(o); } }
			free(rgb); got++; }
		fputc('\n', lg); fflush(lg);
		i++; double el = now_ms() - t; if (el < ms) { struct timespec s = { 0, (long)((ms - el) * 1e6) }; nanosleep(&s, NULL); }
	}
	fclose(lg); XCloseDisplay(d); return 0;
}
