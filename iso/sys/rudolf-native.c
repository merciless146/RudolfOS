/*
 * rudolf-native - the native desktop engine, a prototype (Rudolf OS 1.5.9, docs/plan-1.5.9.md Part 12).
 *
 * Settings › System health › Desktop engine › Native (test): the session starts this instead of the desktop page, as the
 * desktop user:   rudolf-native --size=WxH [--draw=gpu|mixed|soft] [--base=http://127.0.0.1:8080]
 * One GTK 3 process draws what the desktop page draws around the apps, natively:
 *   - the wallpaper: a picture of your own, a drawn background's light or dark picture, or the colours and patterns;
 *   - the taskbar: the start button, the pinned apps, the open windows, the tray (network, sound, battery, the
 *     notifications' count) and the clock, in the taskbar's styles and the accent colour (accent56.js, tbbtn59.js);
 *   - the start menu (the page's own list, its search box, Log Off, Shut Down... and its Power dialog);
 *   - the sign-in and lock screens, the password checked by rudolfd (desktop/login: it never gives out a hash).
 * (1.6.0 Part 12, "The rule") all of it as the page draws it, part by part: iso/qt/native/same_t.sh compares each part
 * with the page's own drawing in Chromium (the page's CSS ported: sizes, colours, the font through Pango, corners,
 * gloss, edges and shadows).
 * The apps stay web pages: each opens in a WebKit window of its own showing the desktop page in one-app mode
 * (index.html#app=NAME, oneapp59.js); all of them share one WebKit web process (related-view).
 * Everything it knows comes from rudolfd's API (docs/api.md: the desktop/ requests), with the token rudolfd writes into its page.
 *
 * Built like rudolf-webkit:  gcc -O2 -o rudolf-native rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm
 * WebP pictures (the drawn backgrounds) through WebKit's own libwebp, opened when needed (no new library).
 * Tests: iso/qt/host/native59_t.sh (under Xvfb, a stand-in rudolfd), iso/qt/native/same_t.sh (the same look as Standard).
 * Copyright (C) 2026 Rudolf OS contributors.
 * GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <webkit2/webkit2.h>
#include <libsoup/soup.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/XKBlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/XInput2.h>
#include <cairo-xlib.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <math.h>
#include <stdint.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "litepoint.h"   /* (1.6.0 Part 17: the window transitions' scenes, the page's litepoint60.js in C) */
#include "rsynth.h"     /* (1.6.0: the Rudolf sounds' renderer, the page's sndrs60.js rsRender in C) */

/* ------------------------------------------------------------------ state */
static char HOST[64] = "127.0.0.1", TOKEN[64];
static int PORT = 8080, SW, SH, BARH = 46, TH = 35;
static double Z = 1;                          /* Display Properties' display scaling (scale: 75 to 250 %), as the page's zoom */
static int TOPRES;                            /* the development link's banner, at the top of the screen (0: none) */
static WebKitHardwareAccelerationPolicy HWPOL = WEBKIT_HARDWARE_ACCELERATION_POLICY_ON_DEMAND;
static int SOFT;                              /* --draw=soft: the lighter effects, as rudolf-webkit */
static const char *STATEF;                    /* RUDOLF_NATIVE_STATE: what is where, for the tests */
/* (2026-11-16, the owner's Vivobook: an app's key as the page has it: its own apps' (notepad), and the Library's, Linux's and Windows'
   programs' (pkg:rudolf-emberreach, lx:com.discordapp.Discord.desktop, lx:deb-starport-ge): letters, digits and _ - : . +) */
#define KN 128
static int key_ok(const char *k) { size_t n = k ? strlen(k) : 0; return n && n < KN && strspn(k, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-:.+") == n; }
typedef struct {
	char bs[16], b1[16], b2[16], dyn[48], dm[8], wpfit[12], tb[8], tbstyle[12], tbacm[12], tbbstyle[12], tbbacm[12];
	char twstyle[12], twacm[12], accent[16], df[8], tz[64], acct[80], wpacct[80], icons[12], ff[48];
	char tbpcol[16];   /* (1.6.1 Part 25: the progress fill's colour on a minimized window's taskbar button, tbprog61.js S.tbpcol) */
	int tbac, tbbac, h24, wpimg, wallpaper, notes, npins, scale, darkui; double lat, doff, gen; char pins[24][KN];
	int nl, dnd, sr, mag, hc, vk, touch;   /* (quick settings' switches: item 3) */
	int nlv, nlsched; double magz;   /* (1.6.0: Night light's strength and hours, the magnifier's zoom: the shell does them for the whole screen) */
	int aur, rmotion; char aurm[12], lnm[12];   /* (the dynamic backgrounds' moving parts: test findings N11, N12) */
	int fs; char tx[16], cb[12];   /* (the title bars: the Font size, the Window text colour, the colour vision filter) */
	int idlerest, idlemin;   /* (1.6.1 Part 23 C: idle windows give their memory back: -1 not chosen (on under 6 GB), 0, 1; after how many minutes, 0: 10) */
	int lpac, lpbr; char lpacm[12];   /* (1.6.0 Part 17: the window transitions' colour, its shade, their light 0 to 100) */
} Settings;
static Settings S;
static gint64 LP_S2S_T;   /* (1.6.0 Part 17: when an app window's page last asked for another window: it opens side to side) */
typedef struct { char k[KN], name[80], icon[48], kw[200], ic[24]; int x; } App;   /* (x: it opens no window of the page's: a Linux or Windows program, a Library app run by rudolfd) */
static App *APPS; static int NAPPS; static GHashTable *BG;   /* key -> "dark\nlight" */
static GHashTable *ICONS;                     /* an app's picture (App.ic) -> GdkPixbuf, as the page draws it (desktop/apps) */
static GdkPixbuf *pix_from_bytes(GByteArray *b);
static GdkPixbuf *LOGO30, *LOGO84;           /* the page's own logo pictures: the start button's and the sign-in screen's (rudolfd's page) */
static char VER[32];                          /* Rudolf OS's version, for the start button (the page's RVER) */
typedef struct { char id[80], n[128], r[12]; int key, old; } Acct;
static Acct ACC[32]; static int NACC, GUEST_OK, SETUP; static char LASTACC[80];
static char USER_ID[80], USER_N[128], USER_R[12];   /* signed in ("" = not) */
static char WEB_DIR[400];   /* (1.6.0 Part 22) the account's encrypted folder for Oracle's websites (rudolfd's "web"), "" = Standard's profile */
static int LOCKED;
/* the tray */
static int NET_CELL, NET_SIG = -1;   /* (1.6.0 Part 5: mobile data in use, its signal 0-100: net31.js's bars) */
static int NET_RSSI = 1, NET_UP = -1, NET_WIFI, VOL = -1, MUTED, BAT = -1, CHARGING, MIC_N = 0, MIC_MUTED;   /* (MIC_N: apps using a microphone, call59.js) */ static char NET_DESC[200];   /* (net31.js net31Desc of the network used) */

/* where windows go: below the development link's banner, beside the taskbar */
/* (1.6.0 Part 12, item 3's last part) the taskbar at the left or the right (body.left/.right #bar: a column as wide as its
   widest part, max-content, from the screen's top to its bottom); BARW its width, measured as the page's (bar_measure) */
static int BARW = 0;
static int tb_side(void) { return !strcmp(S.tb, "left") ? 1 : !strcmp(S.tb, "right") ? 2 : 0; }
static int bar_x(void) { return tb_side() == 2 ? SW - BARW : 0; }
static int bar_y(void) { return tb_side() || !strcmp(S.tb, "top") ? TOPRES : SH - BARH; }
static int bar_w(void) { return tb_side() ? BARW : SW; }
static int bar_h(void) { return tb_side() ? SH - TOPRES : BARH; }
static int desk_left(void) { return tb_side() == 1 ? BARW + (int)lround(4 * Z) : 0; }   /* (#desk's inset: var(--bw) beside a bar at the left or the right) */
static int desk_w(void) { return SW - (tb_side() ? BARW + (int)lround(4 * Z) : 0); }
static int work_top(void) { return TOPRES + (!strcmp(S.tb, "top") ? BARH : 0); }
static int work_h(void) { return SH - TOPRES - (tb_side() ? 0 : BARH); }

/* ------------------------------------------------------------------ rudolfd over HTTP (127.0.0.1) */
/* one request; the status code (0: rudolfd did not answer); *out (may be NULL) gets the body, NUL-terminated */
static int http(const char *method, const char *path, const void *body, size_t blen, GByteArray **out)
{
	if (out) *out = NULL;
	int s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0); if (s < 0) return 0;
	struct timeval tv = { 30, 0 }; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv); setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
	struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(PORT) }; inet_pton(AF_INET, HOST, &a.sin_addr);
	if (connect(s, (struct sockaddr *)&a, sizeof a)) { close(s); return 0; }
	GString *h = g_string_new(NULL);
	g_string_printf(h, "%s %s HTTP/1.0\r\nHost: %s:%d\r\nX-Rudolf-Token: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n", method, path, HOST, PORT, TOKEN, blen);
	int okw = send(s, h->str, h->len, MSG_NOSIGNAL) == (ssize_t)h->len; g_string_free(h, TRUE);
	for (size_t o = 0; okw && o < blen;) { ssize_t w = send(s, (const char *)body + o, blen - o, MSG_NOSIGNAL); if (w <= 0) { okw = 0; break; } o += w; }
	GByteArray *b = g_byte_array_new(); char buf[16384]; ssize_t n;
	while (okw && (n = recv(s, buf, sizeof buf, 0)) > 0) g_byte_array_append(b, (guint8 *)buf, n);
	close(s);
	int code = 0; guint8 z = 0; g_byte_array_append(b, &z, 1);
	if (b->len > 12 && sscanf((char *)b->data, "HTTP/%*s %d", &code) == 1) {
		char *e = strstr((char *)b->data, "\r\n\r\n");
		if (e && out) { size_t at = e + 4 - (char *)b->data; g_byte_array_remove_range(b, 0, at); *out = b; b = NULL; }
	}
	if (b) g_byte_array_free(b, TRUE);
	return code;
}
static char *http_text(const char *path, int *code)
{
	GByteArray *b; int c = http("GET", path, NULL, 0, &b); if (code) *code = c;
	if (!b) return NULL; return (char *)g_byte_array_free(b, FALSE);
}
/* the token, as the page gets it: written into rudolfd's page (GET /) */
static int get_token(void)
{
	char *p = http_text("/", NULL); if (!p) return 0;
	char *t = strstr(p, "token:\""); int ok = 0;
	if (t) { t += 7; size_t n = strcspn(t, "\""); if (n > 0 && n < sizeof TOKEN) { memcpy(TOKEN, t, n); TOKEN[n] = 0; ok = 1; } }
	/* (1.6.0 Part 12) the page's own pictures of the logo: the start button's and the sign-in screen's */
	static const char *const AT[2] = { "<button id=\"start\" title=\"Start\"><img class=\"rlogo\" alt=\"Start\" src=\"data:image/png;base64,", "<div class=\"lav\"><img class=\"rlogo\" alt=\"\" src=\"data:image/png;base64," };
	for (int i = 0; i < 2 && ok; i++) { char *q = strstr(p, AT[i]); if (!q) continue; q += strlen(AT[i]); size_t n = strcspn(q, "\""); char c = q[n]; q[n] = 0;
		gsize bn = 0; guchar *raw = g_base64_decode(q, &bn); q[n] = c; GByteArray *ba = g_byte_array_new_take(raw, bn); guint8 z = 0; g_byte_array_append(ba, &z, 1);
		GdkPixbuf *px = pix_from_bytes(ba); g_byte_array_free(ba, TRUE); if (px) { GdkPixbuf **dst = i ? &LOGO84 : &LOGO30; if (*dst) g_object_unref(*dst); *dst = px; } }
	g_free(p); return ok;
}

/* ------------------------------------------------------------------ JSON (a small reader of our own: a JSCContext made in
   this process stops WebKit's views from drawing, found on WebKitGTK 2.52) */
typedef struct JV { int t; double n; char *s; int len; struct JV **v; char **k; } JV;   /* t: 0 null 1 bool 2 number 3 string 4 array 5 object */
static void jfree(JV *j) { if (!j) return; for (int i = 0; i < j->len; i++) { jfree(j->v[i]); if (j->k) g_free(j->k[i]); } g_free(j->v); g_free(j->k); g_free(j->s); g_free(j); }
static const char *jws(const char *p) { while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++; return p; }
static char *jstring(const char **pp)
{
	const char *p = *pp + 1; GString *o = g_string_new(NULL);
	while (*p && *p != '"') {
		if (*p != '\\') { g_string_append_c(o, *p++); continue; }
		p++;
		if (*p == 'u') { unsigned c = 0; for (int i = 1; i <= 4; i++) { int x = g_ascii_xdigit_value(p[i]); if (x < 0) { g_string_free(o, TRUE); return NULL; } c = c << 4 | x; } p += 5;
			if (c >= 0xd800 && c < 0xdc00 && p[0] == '\\' && p[1] == 'u') { unsigned d = 0; int ok = 1; for (int i = 2; i <= 5; i++) { int x = g_ascii_xdigit_value(p[i]); if (x < 0) ok = 0; else d = d << 4 | x; }
				if (ok && d >= 0xdc00 && d < 0xe000) { c = 0x10000 + ((c - 0xd800) << 10) + (d - 0xdc00); p += 6; } }
			g_string_append_unichar(o, c); continue; }
		char c = *p == 'n' ? '\n' : *p == 't' ? '\t' : *p == 'r' ? '\r' : *p == 'b' ? '\b' : *p == 'f' ? '\f' : *p; if (!*p) break; g_string_append_c(o, c); p++;
	}
	if (*p != '"') { g_string_free(o, TRUE); return NULL; }
	*pp = p + 1; return g_string_free(o, FALSE);
}
static JV *jvalue(const char **pp, int depth)
{
	const char *p = jws(*pp); if (depth > 64) return NULL;
	JV *j = g_new0(JV, 1);
	if (*p == '"') { j->t = 3; if (!(j->s = jstring(&p))) { g_free(j); return NULL; } }
	else if (*p == '{' || *p == '[') {
		int obj = *p == '{'; char end = obj ? '}' : ']'; j->t = obj ? 5 : 4; p = jws(p + 1);
		while (*p && *p != end) {
			char *key = NULL;
			if (obj) { if (*p != '"' || !(key = jstring(&p))) { jfree(j); return NULL; } p = jws(p); if (*p != ':') { g_free(key); jfree(j); return NULL; } p++; }
			JV *x = jvalue(&p, depth + 1); if (!x) { g_free(key); jfree(j); return NULL; }
			j->v = g_renew(JV *, j->v, j->len + 1); j->v[j->len] = x; if (obj) { j->k = g_renew(char *, j->k, j->len + 1); j->k[j->len] = key; } j->len++;
			p = jws(p); if (*p == ',') p = jws(p + 1); else if (*p != end) { jfree(j); return NULL; }
		}
		if (*p != end) { jfree(j); return NULL; }
		p++;
	}
	else if (!strncmp(p, "true", 4)) { j->t = 1; j->n = 1; p += 4; } else if (!strncmp(p, "false", 5)) { j->t = 1; p += 5; } else if (!strncmp(p, "null", 4)) { p += 4; }
	else { char *e; j->n = g_ascii_strtod(p, &e); if (e == p) { g_free(j); return NULL; } j->t = 2; p = e; }
	*pp = p; return j;
}
static JV *jparse(const char *s) { if (!s) return NULL; const char *p = s; JV *j = jvalue(&p, 0); if (j && j->t != 5) { jfree(j); j = NULL; } return j; }
static JV *jprop(JV *o, const char *k) { if (!o || o->t != 5) return NULL; for (int i = 0; i < o->len; i++) if (!strcmp(o->k[i], k)) return o->v[i]; return NULL; }
static void jstr(JV *o, const char *k, char *out, size_t n)
{
	JV *v = jprop(o, k);
	if (v && v->t == 3) snprintf(out, n, "%s", v->s); else if (v && v->t == 2) { char b[40]; g_ascii_formatd(b, sizeof b, "%.15g", v->n); snprintf(out, n, "%s", b); }
}
static double jnum(JV *o, const char *k, double d) { JV *v = jprop(o, k); return v && v->t == 2 ? v->n : d; }
static int jbool(JV *o, const char *k) { JV *v = jprop(o, k); return v && (v->t == 1 || v->t == 2 ? v->n != 0 : v->t == 3 ? v->s[0] != 0 : v->t >= 4); }
static JV *jat(JV *a, int i) { return a && a->t == 4 && i >= 0 && i < a->len ? a->v[i] : NULL; }
static JV *jget(const char *path) { int c; char *t = http_text(path, &c); JV *v = c == 200 ? jparse(t) : NULL; g_free(t); return v; }
/* a message from a page (WebKit gives it as a JavaScript value) */
static void jsstr(JSCValue *o, const char *k, char *out, size_t n)
{
	JSCValue *v = jsc_value_object_get_property(o, k);
	if (v && (jsc_value_is_string(v) || jsc_value_is_number(v))) { char *s = jsc_value_to_string(v); snprintf(out, n, "%s", s); g_free(s); }
	if (v) g_object_unref(v);
}

/* ------------------------------------------------------------------ colours (accent56.js, a11y25.js, ported) */
typedef struct { double r, g, b; } RGB;
static int hex2rgb(const char *h, RGB *o)
{
	if (!h) return 0; if (*h == '#') h++;
	unsigned v; char x[8];
	if (strlen(h) == 3) { snprintf(x, sizeof x, "%c%c%c%c%c%c", h[0], h[0], h[1], h[1], h[2], h[2]); h = x; }
	if (strlen(h) != 6 || strspn(h, "0123456789abcdefABCDEF") != 6 || sscanf(h, "%x", &v) != 1) return 0;
	o->r = (v >> 16 & 255) / 255.0; o->g = (v >> 8 & 255) / 255.0; o->b = (v & 255) / 255.0; return 1;
}
static RGB hexc(const char *h, const char *def) { RGB c = {0, 0, 0}; if (!hex2rgb(h, &c)) hex2rgb(def, &c); return c; }
static void rgb2hsl(RGB c, double *h, double *s, double *l)
{
	double mx = fmax(c.r, fmax(c.g, c.b)), mn = fmin(c.r, fmin(c.g, c.b)); *l = (mx + mn) / 2; *h = 0; *s = 0;
	if (mx != mn) { double d = mx - mn; *s = *l > .5 ? d / (2 - mx - mn) : d / (mx + mn);
		*h = mx == c.r ? (c.g - c.b) / d + (c.g < c.b ? 6 : 0) : mx == c.g ? (c.b - c.r) / d + 2 : (c.r - c.g) / d + 4; *h *= 60; }
	*s *= 100; *l *= 100;
}
static RGB hsl2rgb(double h, double s, double l)
{
	h = fmod(fmod(h, 360) + 360, 360); s /= 100; l /= 100;
	double a = s * fmin(l, 1 - l); double f[3]; int ns[3] = {0, 8, 4};
	for (int i = 0; i < 3; i++) { double k = fmod(ns[i] + h / 30, 12); f[i] = l - a * fmax(-1, fmin(k - 3, fmin(9 - k, 1))); }
	RGB o = { round(f[0] * 255) / 255, round(f[1] * 255) / 255, round(f[2] * 255) / 255 }; return o;
}
static RGB variant(RGB c, const char *mode)
{
	if (mode && !strcmp(mode, "negative")) { RGB o = { 1 - c.r, 1 - c.g, 1 - c.b }; return o; }
	if (mode && !strcmp(mode, "comp")) { double h, s, l; rgb2hsl(c, &h, &s, &l); return hsl2rgb(h + 180, s, l); }
	return c;
}
static double lin(double v) { return v <= .03928 ? v / 12.92 : pow((v + .055) / 1.055, 2.4); }
static int ink_black(RGB c) { double Y = .2126 * lin(c.r) + .7152 * lin(c.g) + .0722 * lin(c.b); return !(1.05 / (Y + .05) >= (Y + .05) / .05); }
static RGB shade(RGB c, double d) { double h, s, l; rgb2hsl(c, &h, &s, &l); return hsl2rgb(h, s, fmax(0, fmin(100, l + d))); }
static double lightness(RGB c) { double h, s, l; rgb2hsl(c, &h, &s, &l); return l; }
static RGB grey_of(RGB c) { return hsl2rgb(0, 0, lightness(c)); }
static void src(cairo_t *cr, RGB c, double a) { cairo_set_source_rgba(cr, c.r, c.g, c.b, a); }
static int style_ok(const char *s) { return s && (!strcmp(s, "wire") || !strcmp(s, "mono") || !strcmp(s, "candy") || !strcmp(s, "retro")); }
static RGB accent(void) { return hexc(S.accent, "#2563eb"); }
static void rrect(cairo_t *cr, double x, double y, double w, double h, double r)
{
	r = fmin(r, fmin(w, h) / 2); cairo_new_sub_path(cr);
	cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0); cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
	cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI); cairo_arc(cr, x + r, y + r, r, M_PI, 3 * M_PI / 2); cairo_close_path(cr);
}
/* ------------------------------------------------------------------ text */
static void text_at(cairo_t *cr, const char *t, double x, double y, double w, double h, const char *font, RGB c, int align /* 0 left 1 centre 2 right */)
{
	PangoLayout *pl = pango_cairo_create_layout(cr); PangoFontDescription *fd = pango_font_description_from_string(font);
	pango_layout_set_font_description(pl, fd); pango_layout_set_text(pl, t, -1);
	pango_layout_set_ellipsize(pl, PANGO_ELLIPSIZE_END); pango_layout_set_width(pl, (int)(w * PANGO_SCALE));
	pango_layout_set_alignment(pl, align == 1 ? PANGO_ALIGN_CENTER : align == 2 ? PANGO_ALIGN_RIGHT : PANGO_ALIGN_LEFT);
	int lw, lh; pango_layout_get_pixel_size(pl, &lw, &lh);
	src(cr, c, 1); cairo_move_to(cr, x, y + (h - lh) / 2); pango_cairo_show_layout(cr, pl);
	pango_font_description_free(fd); g_object_unref(pl);
}

/* the same at the display scaling (x, y, w, h in the screen's pixels; the font's size scaled) */
static void text_atz(cairo_t *cr, const char *t, double x, double y, double w, double h, const char *font, RGB c, int align)
{
	cairo_save(cr); cairo_scale(cr, Z, Z); text_at(cr, t, x / Z, y / Z, w / Z, h / Z, font, c, align); cairo_restore(cr);
}

/* ------------------------------------------------------------------ the tests' view of the screen */
typedef struct { int x, y, w, h; char kind[12], arg[80]; } Hit;
static Hit HITS[96]; static int NHITS;
/* (1.6.1 Part 1: several monitors, below: "several monitors". MON[0] the main one, at 0,0, whose size SW and SH are; a window's place
   (x, y) is on the monitor showing its workspace, mon_ox and mon_oy that monitor's place) */
typedef struct { char name[40]; int x, y, w, h, ws; GtkWidget *bar, *barea, *wall, *warea, *cov; cairo_surface_t *wsurf; char wkey[300]; Hit hits[96]; int nhits; } Mon;
static Mon MON[3]; static int NMON = 1, MONK_CUR, HOV_BAR, PRS_BAR, MON_DRAWK, WALL_SIDE, MENU_MON, FL_MON, STATE_LATER;
static void mon_walls_queue(void); static void mon_bars_queue(void);
static void state_write(void); static void kb_sync(void);
typedef struct JV JV; static void di_layout(void); static void di_load(JV *cat); static int di_shown(void); static int di_n(void);
static gint64 ICONS_SINCE;   /* (when the account signed in, until its desktop's icons are there: the log's line) */   /* (the desktop's icons, below) */
static gint64 UNLOCK_T; static void sys_load(JV *v); static void sc_load(JV *v); static void wg_paint(cairo_t *cr); static void wg_load(JV *v); static void jumps_load(JV *v); static void wg_dirty_kind(const char *t); static void wg_picker(void);   /* (the widgets, below) */
static int wg_shown(void); static int NWG_ALL_DIRTY;
static gboolean wg_press(GdkEventButton *e); static gboolean wg_motion(GdkEventMotion *e); static gboolean wg_release(GdkEventButton *e); static gboolean note_key(GdkEventKey *e); static int NF;

/* ------------------------------------------------------------------ pictures */
static GdkPixbuf *pix_from_bytes(GByteArray *b)
{
	if (!b || b->len < 12) return NULL;
	if (!memcmp(b->data, "RIFF", 4) && !memcmp(b->data + 8, "WEBP", 4)) {
		/* WebP: WebKit's own libwebp, opened now (the system layer has it for WebKit; no new library) */
		static uint8_t *(*dec)(const uint8_t *, size_t, int *, int *); static void (*fr)(void *); static int tried;
		if (!tried) { tried = 1; void *h = dlopen("libwebp.so.7", RTLD_NOW | RTLD_LOCAL); if (!h) h = dlopen("libwebp.so", RTLD_NOW | RTLD_LOCAL);
			if (h) { dec = (uint8_t *(*)(const uint8_t *, size_t, int *, int *))dlsym(h, "WebPDecodeRGBA"); fr = (void (*)(void *))dlsym(h, "WebPFree"); } }
		int w = 0, hh = 0; uint8_t *px = dec ? dec(b->data, b->len - 1, &w, &hh) : NULL;
		if (!px) return NULL;
		GdkPixbuf *p = gdk_pixbuf_new_from_data(px, GDK_COLORSPACE_RGB, TRUE, 8, w, hh, w * 4, NULL, NULL), *c = gdk_pixbuf_copy(p);
		g_object_unref(p); if (fr) fr(px); else free(px); return c;
	}
	GdkPixbufLoader *ld = gdk_pixbuf_loader_new(); GdkPixbuf *p = NULL;
	if (gdk_pixbuf_loader_write(ld, b->data, b->len - 1, NULL) && gdk_pixbuf_loader_close(ld, NULL)) { p = gdk_pixbuf_loader_get_pixbuf(ld); if (p) g_object_ref(p); }
	else gdk_pixbuf_loader_close(ld, NULL);
	g_object_unref(ld); return p;
}
static GdkPixbuf *pix_get(const char *path) { GByteArray *b; int c = http("GET", path, NULL, 0, &b); GdkPixbuf *p = c == 200 ? pix_from_bytes(b) : NULL; if (b) g_byte_array_free(b, TRUE); return p; }
static GdkPixbuf *LOGO;
/* (the owner's Vivobook, 2026-11-16: a menu "tearing" when one replaced another) There is no compositor: each pop-up of the shell (a
   menu, the start menu, the tray's pop-ups, the indicator, a dialog) draws over a copy of what is behind it. A pop-up that replaces
   another was copying the screen at once, before the windows uncovered by the old one had drawn themselves again (X leaves an
   uncovered window's old pixels there until its owner draws): a strip of the old menu in the new one's copy, there for as long as the
   new menu was. So each pop-up taken away gives back what was behind it, and a copy of the screen taken within STALE_US after has
   those parts from there, as they were under it (screen_copy). */
typedef struct { int x, y; GdkPixbuf *bg; gint64 t; } Stale;
static Stale STALE[6];
#define STALE_US 800000
static void pop_gone(GdkPixbuf *bg, int x, int y)
{
	if (!bg) return; int k = 0;
	for (int i = 0; i < 6; i++) { if (!STALE[i].bg) { k = i; break; } if (STALE[i].t < STALE[k].t) k = i; }
	if (STALE[k].bg) g_object_unref(STALE[k].bg);
	STALE[k].x = x; STALE[k].y = y; STALE[k].bg = g_object_ref(bg); STALE[k].t = g_get_monotonic_time();
}
static GdkPixbuf *screen_copy(int x, int y, int w, int h)
{
	GdkPixbuf *pb = gdk_pixbuf_get_from_window(gdk_get_default_root_window(), x, y, w, h); if (!pb) return NULL;
	gint64 now = g_get_monotonic_time(); int ord[6], n = 0;
	for (int i = 0; i < 6; i++) { if (!STALE[i].bg) continue; if (now - STALE[i].t > STALE_US) { g_object_unref(STALE[i].bg); STALE[i].bg = NULL; continue; } ord[n++] = i; }
	for (int i = 1; i < n; i++) for (int j = i; j > 0 && STALE[ord[j]].t < STALE[ord[j - 1]].t; j--) { int t = ord[j]; ord[j] = ord[j - 1]; ord[j - 1] = t; }   /* (the oldest first: a newer one's copy is the truer) */
	int pw = gdk_pixbuf_get_width(pb), ph = gdk_pixbuf_get_height(pb);
	for (int k = 0; k < n; k++) { Stale *st = &STALE[ord[k]]; int sw = gdk_pixbuf_get_width(st->bg), sh = gdk_pixbuf_get_height(st->bg);
		int x0 = MAX(x, st->x), y0 = MAX(y, st->y), x1 = MIN(x + pw, st->x + sw), y1 = MIN(y + ph, st->y + sh);
		if (x1 > x0 && y1 > y0) gdk_pixbuf_copy_area(st->bg, x0 - st->x, y0 - st->y, x1 - x0, y1 - y0, pb, x0 - x, y0 - y); }
	return pb;
}

/* ------------------------------------------------------------------ the wallpaper */
static cairo_surface_t *DESKC; static int DESKD;   /* (the desktop as drawn, and whether to draw it again) */
static GtkWidget *DESK, *DESKAREA, *BAR, *BARAREA, *MENU, *SIGN, *DLBAN, *DLASK, *PWR;
/* (the tray's pop-ups: item 3, below) */
static void fl_open(int kind); static void cal_events_load(JV *v); static void nt_load(JV *v);
enum { FL_NONE, FL_CAL, FL_VOL, FL_QS, FL_NC, FL_NET, FL_BRI };   /* (the tray's pop-ups: the calendar, the volume, quick settings, the notifications' centre, the network; 1.6.0: the brightness, osd31.js briPop) */
static double NETB[4];   /* (as drawn: the network's button, for its pop-up's place) */
static int nt_unread(void);
static double VOLB[4], CLKD[2];   /* (as drawn, the page's units from the bar's corner: the volume's button; the clock's date, its <small>) */
/* (1.6.0) the screen's brightness (0-100, -1: unknown) and whether it has a backlight (the taskbar's #bri, as drawn: BRIB); the on-screen
   keyboard shown (#vkb; its ⌨ on the taskbar: VKTB); the magnifier as saved and its zoom; Ctrl+Alt+Del's Task Manager asked for (its
   window above every window when it comes) */
static int BRI = -1, BRI_HAS, VK_SHOWN, MAG_SAVED = -1, TM_TOP; static double MAGZ = 2, BRIB[4], VKTB[4];
static void a11y_apply(void); static void vk_sync(void); static void vk_msg(JSCValue *v); static char *shell_script(void); static void bri_set(int v); static void hk_post(const char *q, int bri);
static void power(const char *what); static void fl_close(void); static void gamma_apply(void); static void mag_set(int on); static int mag_can(void); static void sc_settings(void);
static void mag_zoom(double dz); static void bri_load(void); static double fl_lh(cairo_t *cr, int wt, double px);
static void desk_dirty(void) { DESKD = 1; if (DESKAREA) gtk_widget_queue_draw(DESKAREA); }
typedef struct JV JV; static void fx_cat(JV *cat); static void fx_draw(cairo_t *cr); static void fx_update(void); static int FXD;   /* (the dynamic backgrounds' moving parts: below) */
static void desk_shadows(void) { if (DESKAREA) gtk_widget_queue_draw(DESKAREA); mon_walls_queue(); }
static cairo_surface_t *WALL;   /* drawn at the screen's size */
static char WALL_KEY[300];
/* (the owner's Vivobook, 2026-11-16) Display Properties' choices not applied yet, previewed on the whole screen as Standard's page shows
   them at once (its live S, applySet): Settings' window gives them (op "prev": its S, and the picture of your own chosen, which only its
   page has until it is applied), the shell draws with them instead of the saved settings until they are applied or discarded (an
   empty "prev"), or that window closes */
static char *PREV_S; static GdkPixbuf *PREV_WP; static int PREV_W; static unsigned PREV_N; static guint PREV_KEEP;   /* (applied: kept until the saved settings come, op "set", 3 s at most) */
static int WALL_MISS;           /* a picture of your own chosen and not there yet: drawn as soon as rudolfd has it (tick_settings) */
/* light or dark (the drawn backgrounds): the setting, else daylight at the latitude (the sun's height at noon and
   the day's length, as the page's sunCalc roughly), else 7 to 19 o'clock */
static int is_dark(void)
{
	if (!strcmp(S.dm, "dark")) return 1; if (!strcmp(S.dm, "light")) return 0;
	time_t t = time(NULL) + (time_t)(S.doff / 1000); struct tm lt; localtime_r(&t, &lt);
	double hour = lt.tm_hour + lt.tm_min / 60.0, day = 12;
	if (!isnan(S.lat) && fabs(S.lat) < 90) {
		double decl = -23.44 * cos(2 * M_PI / 365 * (lt.tm_yday + 10)) * M_PI / 180, la = S.lat * M_PI / 180, c = -tan(la) * tan(decl);
		day = c >= 1 ? 0 : c <= -1 ? 24 : 2 * acos(c) * 180 / M_PI / 15;
	}
	return !(hour >= 12 - day / 2 && hour < 12 + day / 2);
}
static void pattern_bg(cairo_t *cr, int w, int h)
{
	RGB a = hexc(S.b1, "#008080"), c = hexc(S.b2, "#000080"); const char *bs = S.bs;
	if (!strcmp(bs, "gradient") || !strcmp(bs, "vertical")) {
		cairo_pattern_t *g = !strcmp(bs, "vertical") ? cairo_pattern_create_linear(0, 0, 0, h) : cairo_pattern_create_linear(0, 0, w, h);
		cairo_pattern_add_color_stop_rgb(g, 0, a.r, a.g, a.b); cairo_pattern_add_color_stop_rgb(g, 1, c.r, c.g, c.b);
		cairo_set_source(cr, g); cairo_paint(cr); cairo_pattern_destroy(g); return;
	}
	if (!strcmp(bs, "radial")) {
		double r = sqrt((double)w * w + (double)h * h) / 2; cairo_pattern_t *g = cairo_pattern_create_radial(w / 2.0, h * .4, 0, w / 2.0, h * .4, r);
		cairo_pattern_add_color_stop_rgb(g, 0, c.r, c.g, c.b); cairo_pattern_add_color_stop_rgb(g, 1, a.r, a.g, a.b);
		cairo_set_source(cr, g); cairo_paint(cr); cairo_pattern_destroy(g); return;
	}
	src(cr, a, 1); cairo_paint(cr); src(cr, c, 1);
	if (!strcmp(bs, "checker")) { for (int y = 0; y < h; y += 16) for (int x = 0; x < w; x += 16) if (((x / 16) + (y / 16)) & 1) cairo_rectangle(cr, x, y, 16, 16); cairo_fill(cr); }
	else if (!strcmp(bs, "dots")) { for (int y = 0; y < h; y += 20) for (int x = 0; x < w; x += 20) { cairo_new_sub_path(cr); cairo_arc(cr, x + 10, y + 10, 3.5, 0, 2 * M_PI); } cairo_fill(cr); }
	else if (!strcmp(bs, "grid")) { for (int y = 0; y < h; y += 24) cairo_rectangle(cr, 0, y, w, 1); for (int x = 0; x < w; x += 24) cairo_rectangle(cr, x, 0, 1, h); cairo_fill(cr); }
	else if (!strcmp(bs, "stripes")) { cairo_save(cr); cairo_rotate(cr, M_PI / 4); double D = w + h; for (double x = -D; x < D; x += 32) cairo_rectangle(cr, x + 16, -D, 16, 3 * D); cairo_fill(cr); cairo_restore(cr); }
}
static void paint_pix(cairo_t *cr, GdkPixbuf *p, int w, int h, const char *fit)
{
	int pw = gdk_pixbuf_get_width(p), ph = gdk_pixbuf_get_height(p);
	if (!strcmp(fit, "tile")) { gdk_cairo_set_source_pixbuf(cr, p, 0, 0); cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT); cairo_paint(cr); return; }
	double sx = (double)w / pw, sy = (double)h / ph, s = !strcmp(fit, "fit") ? fmin(sx, sy) : !strcmp(fit, "center") ? 1 : fmax(sx, sy);
	cairo_save(cr);
	if (!strcmp(fit, "stretch")) cairo_scale(cr, sx, sy);
	else { cairo_translate(cr, (w - pw * s) / 2, (h - ph * s) / 2); cairo_scale(cr, s, s); }
	gdk_cairo_set_source_pixbuf(cr, p, 0, 0); cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
	if (strcmp(fit, "center")) cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_PAD);   /* (no dark edges from scaling) */
	if (strcmp(fit, "fill") && strcmp(fit, "stretch")) { cairo_rectangle(cr, 0, 0, pw, ph); cairo_fill(cr); } else cairo_paint(cr);
	cairo_restore(cr);
}
static void wall_make(void)
{
	char path[300] = "", key[300]; int dark = is_dark();
	/* (wpacct: the signed-in account, or before sign-in the last one; rudolfd makes the copy from the page's database
	   mirror when no page has given it yet: 1.5.9 step 7) */
	int pv = S.wpimg && PREV_S && PREV_WP;   /* (a picture of your own chosen in Settings, not applied yet: its page's copy) */
	if (pv) snprintf(path, sizeof path, "prev:%u", PREV_N);
	else if (S.wpimg && S.wallpaper && S.wpacct[0]) snprintf(path, sizeof path, "/api/desktop/wallpaper?acct=%s", S.wpacct);
	else if (S.dyn[0] && BG) { const char *v = g_hash_table_lookup(BG, S.dyn); if (v) { const char *nl = strchr(v, '\n'); if (dark || !nl) snprintf(path, sizeof path, "/%.*s", nl ? (int)(nl - v) : (int)strlen(v), v); else snprintf(path, sizeof path, "/%s", nl + 1); } }
	snprintf(key, sizeof key, "%s|%s|%s|%s|%s|%s|%d|%d", path, S.bs, S.b1, S.b2, S.wpfit, S.wpacct, SW, (int)S.gen);
	if (WALL && !strcmp(key, WALL_KEY)) return;
	snprintf(WALL_KEY, sizeof WALL_KEY, "%s", key);
	if (!WALL) WALL = cairo_image_surface_create(CAIRO_FORMAT_RGB24, SW, SH);
	cairo_t *cr = cairo_create(WALL); GdkPixbuf *p = pv ? g_object_ref(PREV_WP) : path[0] ? pix_get(path) : NULL;
	WALL_MISS = S.wpimg && !p;
	if (p) {   /* behind a picture: the first colour (a picture of your own), black or light grey (a drawn one) */
		RGB bg = S.wpimg ? hexc(S.b1, "#000000") : dark ? (RGB){0, 0, 0} : hexc("#e5e7eb", NULL); src(cr, bg, 1); cairo_paint(cr);
		paint_pix(cr, p, SW, SH, S.wpimg ? S.wpfit : "fill"); g_object_unref(p);
	} else pattern_bg(cr, SW, SH);
	cairo_destroy(cr);
	if (WALL_SIDE) return;   /* (1.6.1 Part 1: another monitor's wallpaper) */
	if (DESKAREA) desk_dirty();
	fx_update();
}
static gboolean desk_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)w; (void)d; if (WALL) { cairo_set_source_surface(cr, WALL, 0, 0); cairo_paint(cr); } return TRUE; }

/* ------------------------------------------------------------------ app windows (WebKit, one-app mode) */
typedef struct AppWin { GtkWidget *win, *view; char k[KN], title[256], icon[48]; int closing, id, sized, hidden, reveal, modal, pw, ph, wasontop, mfull, pready, sready, s2s; gint64 close_t, born; GdkPixbuf *pix; guint wait_t; int rest, rasking; gint64 idle_t; char *rurl; char rwhy[80]; } AppWin;   /* (rest: 1.6.1 Part 23 C, resting: its page unloaded; rurl: its address to load again, with &rest=; rwhy: why it may not rest now) */   /* (wait_t: its window's upper bound before it is shown anyway, the owner's Vivobook report 1 of 2026-11-16) */   /* (pready: the portal's page (do-portal) is signed in and takes requests, 1.6.0) */   /* (hidden: a page doing something for the shell unseen; modal: a page's dialog over the screen, 2 when the page has no window of its own yet: test findings N2, N4) */
static void ev_flush(AppWin *a);   /* (1.6.0: rudolfd's events that waited for a page) */
static GList *WINS;   /* AppWin*, oldest first */
static AppWin *ACTIVE;
static WebKitWebContext *CTX; static WebKitUserContentManager *UCM; static int NEXTID;
static void bar_redraw(void);
static void app_open(const char *k, const char *file);
static void app_open_ex(const char *k, const char *file, const char *extra, int hidden);
static AppWin *bg_page(void); static int is_bg(const AppWin *a); static int bg_route(const char *k, const char *file, const char *extra); static gint64 BG_OPEN_T;   /* (1.6.1: the shell's one hidden page, do-bg) */
static void appwin_show(AppWin *a);
static void srch_ready(AppWin *a);
static void pt_msg(AppWin *a, JSCValue *v); static void pt_reset(void);   /* (1.6.0: the portal's requests, below) */
static void svc_msg(AppWin *a, JSCValue *v); static void svc_reset(void); static AppWin *svc_page(void); static gint64 SVC_SIGNIN;   /* (1.6.0 Part 12, item 12: the desktop's service page, below; when the account signed in, ms) */
static void snd_page(const char *k, double v, const char *pj); static void snd_click(void);
static int fl_outside(void); static void seat_grab(GtkWidget *w, int owner); static void qs_keep(void);
static int oracle_open(const char *file, const char *extra);   /* (1.5.9, 12.3c: Oracle in Rudolf's Chromium) */
static void page_done(JSCValue *v);
static void frame_redraw_app(AppWin *a);
static const App *app_find(const char *k) { for (int i = 0; i < NAPPS; i++) if (!strcmp(APPS[i].k, k)) return &APPS[i]; return NULL; }
static void appwin_size(AppWin *a, int w, int h);
static void appwin_modal(AppWin *a, int on);
static void appwin_reveal(AppWin *a);
static void on_load_ready(WebKitWebView *v, WebKitLoadEvent ev, gpointer d);
static void prev_msg(AppWin *a, JSCValue *v); static void prev_end(void); static void sav_preview(const char *style);
static void appwin_fs(AppWin *a, int on);
static gboolean apps_reload(gpointer d);
static void wins_ask(AppWin *to); static void win_op(const char *id, const char *what);   /* (Task Manager's windows: test finding N3) */
static void on_msg(WebKitUserContentManager *m, WebKitJavascriptResult *r, gpointer d);
static char *gpu_script(void);
static void ctx_make(void)
{
	if (CTX) return;
	char *home = g_build_filename(g_get_home_dir(), ".rudolf-native", NULL), *data = g_build_filename(home, "data", NULL), *cache = g_build_filename(home, "cache", NULL);
	/* (N7, N8) an origin's share of the disk as Chromium's (60 %; WebKit's own is 1 GB), so navigator.storage.estimate()'s quota is the
	   size Standard says */
	WebKitWebsiteDataManager *m = webkit_website_data_manager_new("base-data-directory", data, "base-cache-directory", cache,
#if WEBKIT_CHECK_VERSION(2, 42, 0)
		"origin-storage-ratio", 0.6, "total-storage-ratio", 0.8,
#endif
		NULL);
	WebKitMemoryPressureSettings *mps = webkit_memory_pressure_settings_new();   /* as rudolf-webkit (1.5.8) */
	/* 1.6.1 (the owner's Vivobook, 2026-11-20: six web processes of 250-380 MB, 400-470 MB at their start): each window's page
	   frees its caches and collects its garbage sooner: the limit follows the computer's memory (a sixth of it, 384 to 768 MB; the
	   Vivobook's 3.3 GB: 569 MB, so from about 190 MB it tidies and from about 285 MB it does so hard), looked at every 5 s, not 15
	   (a page's start, where its peak is, takes a few seconds) */
	{ unsigned lim = 768; FILE *mi = fopen("/proc/meminfo", "r"); char ln[128]; long long kb = 0;
	  while (mi && fgets(ln, sizeof ln, mi)) if (sscanf(ln, "MemTotal: %lld kB", &kb) == 1) break; if (mi) fclose(mi);
	  if (kb > 0) { long long m = kb / 1024 / 6; lim = (unsigned)(m < 384 ? 384 : m > 768 ? 768 : m); }
	  webkit_memory_pressure_settings_set_memory_limit(mps, lim); }
	webkit_memory_pressure_settings_set_conservative_threshold(mps, 0.33);
	webkit_memory_pressure_settings_set_strict_threshold(mps, 0.5); webkit_memory_pressure_settings_set_kill_threshold(mps, 0); webkit_memory_pressure_settings_set_poll_interval(mps, 5);
	CTX = WEBKIT_WEB_CONTEXT(g_object_new(WEBKIT_TYPE_WEB_CONTEXT, "website-data-manager", m, "memory-pressure-settings", mps, NULL));
	webkit_memory_pressure_settings_free(mps); webkit_web_context_set_cache_model(CTX, WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER);
	g_free(home); g_free(data); g_free(cache); g_object_unref(m);
	/* one user content manager for every app window: WebKit gives a window opened by another the opener's anyway, so the
	   pages say which window they are (w, from the address: oneapp59.js) */
	UCM = webkit_user_content_manager_new();
	webkit_user_content_manager_register_script_message_handler(UCM, "rudolfNative");
	g_signal_connect(UCM, "script-message-received::rudolfNative", G_CALLBACK(on_msg), NULL);
	{ char *g = gpu_script(); if (g) { webkit_user_content_manager_add_script(UCM, webkit_user_script_new(g, WEBKIT_USER_CONTENT_INJECT_TOP_FRAME, WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START, NULL, NULL)); g_free(g); } }
	{ char *g = shell_script(); webkit_user_content_manager_add_script(UCM, webkit_user_script_new(g, WEBKIT_USER_CONTENT_INJECT_TOP_FRAME, WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START, NULL, NULL)); g_free(g); }   /* (1.6.0) */
	if (SOFT) webkit_user_content_manager_add_style_sheet(UCM, webkit_user_style_sheet_new(
		"*,*::before,*::after{box-shadow:none!important;text-shadow:none!important;backdrop-filter:none!important}", WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES, WEBKIT_USER_STYLE_LEVEL_USER, NULL, NULL));
}
static void json_esc(GString *s, const char *v);
/* (test finding N8) the graphics chip's name, as Chromium gives it to the page (WEBGL_debug_renderer_info: "ANGLE (VENDOR, RENDERER,
   OpenGL VERSION)", what Settings › About shows under Graphics); WebKit gives every page "Apple GPU". Read once from a GL context of
   a window of its own (GTK's, through libepoxy, which GTK has loaded), given to the pages as window.__oaGpu (oneapp59.js) */
/* (1.6.0, the owner's system report: "Pages drawn with: Apple GPU", and so in QEMU) where GDK has no OpenGL of its own (X without
   GLX's acceleration, or GTK refusing it), the chip from EGL, which WebKit itself draws with: a context without a surface on the X
   display (libEGL, which WebKit has loaded), its GL_VENDOR, GL_RENDERER and GL_VERSION */
static void egl_names(char *ven, size_t nv, char *ren, size_t nr, char *ver, size_t nvr)
{
	typedef void *(*GetDisp)(void *); typedef unsigned (*Init)(void *, int *, int *); typedef unsigned (*Bind)(unsigned);
	typedef void *(*MkCtx)(void *, void *, void *, const int *); typedef unsigned (*Cur)(void *, void *, void *, void *);
	typedef unsigned (*DelCtx)(void *, void *); typedef void *(*Proc)(const char *); typedef const unsigned char *(*GetStr)(unsigned);
	typedef unsigned (*Choose)(void *, const int *, void **, int, int *);
	void *h = dlopen("libEGL.so.1", RTLD_NOW | RTLD_LOCAL); if (!h) return;
	GetDisp gd = (GetDisp)dlsym(h, "eglGetDisplay"); Init in = (Init)dlsym(h, "eglInitialize"); Bind bi = (Bind)dlsym(h, "eglBindAPI"); MkCtx mk = (MkCtx)dlsym(h, "eglCreateContext");
	Cur cu = (Cur)dlsym(h, "eglMakeCurrent"); DelCtx dc = (DelCtx)dlsym(h, "eglDestroyContext"); Proc gp = (Proc)dlsym(h, "eglGetProcAddress"); Choose ch = (Choose)dlsym(h, "eglChooseConfig");
	(void)nvr;
	if (!gd || !in || !bi || !mk || !cu || !dc || !gp || !ch) return;
	void *d = gd(gdk_x11_get_default_xdisplay()); int ma, mi; if (!d || !in(d, &ma, &mi)) return;
	enum { E_NONE = 0x3038, E_OPENGL_ES_API = 0x30A0, E_RENDERABLE = 0x3040, E_ES2_BIT = 0x0004, E_CTX_VER = 0x3098, E_SURFACE_TYPE = 0x3033, E_PBUFFER_BIT = 0x0001 };
	bi(E_OPENGL_ES_API); const int ca[] = { E_RENDERABLE, E_ES2_BIT, E_SURFACE_TYPE, E_PBUFFER_BIT, E_NONE }, xa[] = { E_CTX_VER, 2, E_NONE };
	void *cfg = NULL; int n = 0; ch(d, ca, &cfg, 1, &n);
	void *c = mk(d, n > 0 ? cfg : NULL, NULL, xa);
	if (c && cu(d, NULL, NULL, c)) {   /* (EGL_KHR_surfaceless_context: Mesa's) */
		GetStr gs = (GetStr)gp("glGetString");
		if (gs) { const char *a = (const char *)gs(0x1F00), *b = (const char *)gs(0x1F01), *v = (const char *)gs(0x1F02);
			if (a) snprintf(ven, nv, "%s", a); if (b) snprintf(ren, nr, "%s", b);
			if (v) { const char *x = strstr(v, "OpenGL ES "); if (x) v = x + 10; sscanf(v, "%63s", ver); } }
		cu(d, NULL, NULL, NULL); }
	if (c) dc(d, c);
	/* (libEGL stays loaded and the display initialised: WebKit uses the same) */
}
static char *gpu_script(void)
{
	char ven[128] = "", ren[256] = "", ver[64] = "";
	/* (a window of its own, never shown: GDK draws a window that has had a GL context with GL from then on, and the desktop's went blank) */
	GtkWidget *tw = gtk_window_new(GTK_WINDOW_POPUP); gtk_window_resize(GTK_WINDOW(tw), 1, 1); gtk_widget_realize(tw);
	GdkWindow *w = getenv("RUDOLF_NATIVE_TEST_NOGDKGL") ? NULL : gtk_widget_get_window(tw);   /* (tests: as where GDK has no OpenGL) */
	GError *e = NULL; GdkGLContext *gl = w ? gdk_window_create_gl_context(w, &e) : NULL;
	typedef const unsigned char *(*GetStr)(unsigned); GetStr *gs = (GetStr *)dlsym(RTLD_DEFAULT, "epoxy_glGetString");
	if (gl && gdk_gl_context_realize(gl, NULL) && gs && *gs) { gdk_gl_context_make_current(gl);
		const char *a = (const char *)(*gs)(0x1F00), *b = (const char *)(*gs)(0x1F01), *c = (const char *)(*gs)(0x1F02);
		if (a) snprintf(ven, sizeof ven, "%s", a); if (b) snprintf(ren, sizeof ren, "%s", b); if (c) sscanf(c, "%63s", ver); gdk_gl_context_clear_current(); }
	if (e) g_error_free(e); if (gl) g_object_unref(gl); gtk_widget_destroy(tw);
	if (!ren[0]) egl_names(ven, sizeof ven, ren, sizeof ren, ver, sizeof ver);
	if (!ren[0]) return NULL;
	/* (ANGLE's names for the vendors it knows) */
	const char *v = strstr(ven, "Intel") ? "Intel" : (strstr(ven, "AMD") || strstr(ven, "ATI")) ? "AMD" : strstr(ven, "NVIDIA") ? "NVIDIA" : ven;
	char *r = g_strdup_printf("ANGLE (%s, %s, OpenGL %s)", v, ren, ver), *vv = g_strdup_printf("Google Inc. (%s)", v);
	GString *js = g_string_new("window.__oaGpu={r:"); json_esc(js, r); g_string_append(js, ",v:"); json_esc(js, vv); g_string_append(js, "};");
	g_free(r); g_free(vv); return g_string_free(js, FALSE);
}
static int local_url(const char *u)
{
	if (!u || (strncmp(u, "http:", 5) && strncmp(u, "https:", 6))) return 1;
	char p[96]; snprintf(p, sizeof p, "http://%s:%d/", HOST, PORT); return !strncmp(u, p, strlen(p));
}
static gboolean on_decide(WebKitWebView *v, WebKitPolicyDecision *dec, WebKitPolicyDecisionType type, gpointer d)
{
	(void)v; (void)d; if (type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) return FALSE;
	const char *u = webkit_uri_request_get_uri(webkit_navigation_action_get_request(webkit_navigation_policy_decision_get_navigation_action(WEBKIT_NAVIGATION_POLICY_DECISION(dec))));
	if (!local_url(u)) { fprintf(stderr, "rudolf-native: an app window does not leave the desktop for %s\n", u); webkit_policy_decision_ignore(dec); return TRUE; }
	return FALSE;
}
static gboolean on_perm(WebKitWebView *v, WebKitPermissionRequest *r, gpointer d) { (void)v; (void)d; webkit_permission_request_allow(r); return TRUE; }
/* (the page says it closed in two ways, its message and window.close(): the second finds the window gone) */
static void appwin_close_now(AppWin *a) { if (g_list_find(WINS, a) && a->win) { GtkWidget *w = a->win; a->win = NULL; gtk_widget_destroy(w); } }
static void on_title(GObject *o, GParamSpec *p, gpointer d)
{
	(void)o; (void)p; AppWin *a = d; const char *t = webkit_web_view_get_title(WEBKIT_WEB_VIEW(a->view));
	if (t && *t) { snprintf(a->title, sizeof a->title, "%s", t); gtk_window_set_title(GTK_WINDOW(a->win), t); frame_redraw_app(a); bar_redraw(); state_write(); }
}
/* a file dropped on an app's window (the desktop's icon, or another window's files): done by the page as a11y25.js's
   __dropFileOnWindow does it for that app, in a page of its own (oneapp59.js oaFs op=drop): its window is the file's */
static void drop_in(const char *file, const char *k)
{
	if (!file || !file[0] || !key_ok(k)) return;
	char *ex = g_strdup_printf("&op=drop&in=%s", k); app_open_ex("do-fs", file, ex, !strcmp(k, "trash")); g_free(ex);
}
static guint VALLOC_T;
static gboolean view_alloc_late(gpointer d) { (void)d; VALLOC_T = 0; state_write(); return G_SOURCE_REMOVE; }
static void on_view_alloc(GtkWidget *w, GdkRectangle *r, gpointer d) { (void)w; (void)r; (void)d; if (!VALLOC_T) VALLOC_T = g_timeout_add(200, view_alloc_late, NULL); }
static void on_view_close(WebKitWebView *v, gpointer d) { (void)v; appwin_close_now(d); }
/* (a message's fields, from WebKit's message handler (JSCValue) or Oracle's Chromium (JV, 12.3c): the same ops) */
typedef void (*FGet)(void *v, const char *k, char *o, size_t n);
static void fget_js(void *v, const char *k, char *o, size_t n);
static void fget_jv(void *v, const char *k, char *o, size_t n);
static void toast_addf(AppWin *from, const char *sid, FGet g, void *v);
static void nt_notef(FGet g, void *v);
static void toast_add(AppWin *from, JSCValue *v);
static void tb_settings(const char *page);
static void nt_note(JSCValue *v);
static void open_with(const char *file, const char *key);
static void kbcap_msg(AppWin *a, int on);
static void hfs_msg(AppWin *a, JSCValue *v);   /* (folders on this device: test finding N7) */
static void ctx_msg(AppWin *a, JSCValue *v);
static int FC_N;   /* (1.6.1 Part 28: what File Explorer's clipboard holds, as the pages say: op "fc") */
static void tbp_app(AppWin *a, const char *v);   /* (1.6.1 Part 25: a window's work in progress, for its minimized taskbar button) */   /* (an app's right-click menu, over the screen: test finding N21) */
/* (1.5.9, 12.3c) Oracle's Chromium and the WebKit windows: the files one (rudolfd's mirror), the indicator of the keys the native
   desktop's, Oracle's new windows at an address or private */
static GPid OR_PID; static int OR_RUN; static char OR_CTL[300]; static const char *TOR_CTL_P;   /* (Part 16: the Tor windows' rudolf-launch FIFO, once it runs) */
static void oracle_tell(const char *line); static void oracle_only(const char *line);
static void mirror_to_webkit(const char *id); static void settings_changed(const char *id, AppWin *from);
static void osd_show(const char *k, const char *lv, const char *label, const char *n);
static char *open_extra(const char *url, const char *priv);
static int id_ok(const char *s) { return strspn(s, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") == strlen(s) && strlen(s) < 64; }
static void on_msg(WebKitUserContentManager *m, WebKitJavascriptResult *r, gpointer d)
{
	(void)m; (void)d; JSCValue *v = webkit_javascript_result_get_js_value(r); if (!jsc_value_is_object(v)) return;
	{ char o0[8] = ""; jsstr(v, "op", o0, sizeof o0); if (!strcmp(o0, "vk")) { vk_msg(v); return; } }   /* (1.6.0: the on-screen keyboard's page, a window of no app) */
	char ws[16] = ""; jsstr(v, "w", ws, sizeof ws); int id = atoi(ws); AppWin *a = NULL;
	for (GList *l = WINS; l; l = l->next) if (((AppWin *)l->data)->id == id) a = l->data;
	if (!a) return;
	char op[16] = "", x[256] = "", f[1024] = ""; jsstr(v, "op", op, sizeof op);
	/* (a program's page, opened unseen (app_open_ex: reveal), that makes a window of the page's after all, or one of its dialogs: shown now) */
	if (a->hidden && a->reveal && (!strcmp(op, "size") || !strcmp(op, "modal"))) { a->hidden = 0; a->reveal = 0; appwin_show(a); }
	if (!strcmp(op, "close")) appwin_close_now(a);
	else if (!strcmp(op, "done")) page_done(v);
	else if (!strcmp(op, "title")) { jsstr(v, "icon", x, sizeof x); if (x[0]) snprintf(a->icon, sizeof a->icon, "%s", x);
		/* (test finding N14) the window's own picture when it is not its app's (the page's icSvg of W.icon): its title bar and taskbar button */
		JSCValue *pv = jsc_value_object_get_property(v, "png"); if (pv && jsc_value_is_string(pv)) { char *u = jsc_value_to_string(pv); static const char PRE[] = "data:image/png;base64,";
			if (a->pix) { g_object_unref(a->pix); a->pix = NULL; }
			if (!strncmp(u, PRE, sizeof PRE - 1)) { gsize n = 0; guchar *raw = g_base64_decode(u + sizeof PRE - 1, &n); GByteArray *b = g_byte_array_new_take(raw, n); guint8 z = 0; g_byte_array_append(b, &z, 1); a->pix = pix_from_bytes(b); g_byte_array_free(b, TRUE); }
			g_free(u); frame_redraw_app(a); } if (pv) g_object_unref(pv);
		bar_redraw(); }
	else if (!strcmp(op, "drop")) { jsstr(v, "in", x, sizeof x); jsstr(v, "file", f, sizeof f); drop_in(f, x); }
	else if (!strcmp(op, "fc")) { jsstr(v, "n", x, sizeof x); FC_N = atoi(x); state_write(); }   /* (1.6.1 Part 28: File Explorer's clipboard holds N items: the desktop's Paste) */
	else if (!strcmp(op, "openwith")) { jsstr(v, "key", x, sizeof x); jsstr(v, "file", f, sizeof f); open_with(f, x); }   /* (Open with: the app's own window) */
	else if (!strcmp(op, "toast")) toast_add(a, v);   /* (a notification's pop-up, drawn by the shell: item 3) */
	else if (!strcmp(op, "note")) nt_note(v);   /* (a notification kept: in the centre at once, before the page's storage reaches rudolfd) */
	else if (!strcmp(op, "open")) { jsstr(v, "app", x, sizeof x); jsstr(v, "file", f, sizeof f); char pg[48] = ""; jsstr(v, "page", pg, sizeof pg);
		if (!(f[0] && !strcmp(a->k, "explorer"))) LP_S2S_T = g_get_monotonic_time();   /* (Part 17: opened by another window) */
		char u[1024] = "", pv[4] = ""; jsstr(v, "url", u, sizeof u); jsstr(v, "priv", pv, sizeof pv); char *ex = open_extra(u, pv);
		if (!strcmp(x, "settings") && pg[0]) tb_settings(pg);
		else if (!strcmp(x, "help") && pg[0] && id_ok(pg)) { char hx[96]; snprintf(hx, sizeof hx, "&page=%s", pg); app_open_ex("help", NULL, hx, 0); }   /* (Help at a topic: What's new, 1.6.0) */
		else if (x[0]) app_open_ex(x, f[0] ? f : NULL, ex, 0); g_free(ex); }   /* (Settings at a page: its own window there; Oracle at an address) */
	else if (!strcmp(op, "mirror")) { jsstr(v, "id", x, sizeof x); if (id_ok(x)) { char l[100]; snprintf(l, sizeof l, "mirror %s\n", x); oracle_tell(l); } }   /* (the files an app's window saved: Oracle's pages take them) */
	else if (!strcmp(op, "set")) { jsstr(v, "id", x, sizeof x); if (id_ok(x)) settings_changed(x, a); }   /* (a window saved its settings: the shell and the other windows at once) */
	else if (!strcmp(op, "webclear")) oracle_only("webclear\n");   /* (1.6.0 Part 21: an account deleted here: Oracle's page clears its websites, tabs26.js) */
	else if (!strcmp(op, "osd")) { char k[16] = "", lv[8] = "", n[8] = ""; jsstr(v, "k", k, sizeof k); jsstr(v, "lv", lv, sizeof lv); jsstr(v, "label", x, sizeof x); jsstr(v, "n", n, sizeof n); osd_show(k, lv, x, n); }
	else if (!strcmp(op, "fs")) { jsstr(v, "on", x, sizeof x); appwin_fs(a, x[0] == '1'); }   /* (the page's own full screen: the window covers the screen, test finding N1) */
	else if (!strcmp(op, "kbcap")) { jsstr(v, "on", x, sizeof x); kbcap_msg(a, x[0] == '1'); }   /* (a page that has every key for a moment: the computer check's keyboard, Settings' Press keys…) */
	else if (!strcmp(op, "hfs")) hfs_msg(a, v);
	else if (!strcmp(op, "ctx")) ctx_msg(a, v);   /* (File Explorer's Folders on this device: the folder picker and its files, N7) */
	else if (!strcmp(op, "sfx")) { char k[16] = "", vv[24] = ""; jsstr(v, "k", k, sizeof k); jsstr(v, "v", vv, sizeof vv); char *pj = NULL; JSCValue *pv = jsc_value_object_get_property(v, "p");
		if (pv && jsc_value_is_string(pv)) pj = jsc_value_to_string(pv); if (pv) g_object_unref(pv); snd_page(k, g_ascii_strtod(vv, NULL), pj); g_free(pj); }   /* (2026-11-16: a page's sound, played here once) */
	else if (!strcmp(op, "portal")) pt_msg(a, v);
	else if (!strcmp(op, "svc")) svc_msg(a, v);   /* (1.6.0 Part 12, item 12: the desktop's service page is signed in and runs the background work) */   /* (1.6.0, the owner's Vivobook: the portal's page is ready, or a request opens an app's window) */
	else if (!strcmp(op, "prev")) prev_msg(a, v);   /* (Display Properties' choices, previewed on the whole screen: the owner's Vivobook, 2026-11-16) */
	else if (!strcmp(op, "saver")) { jsstr(v, "style", x, sizeof x); sav_preview(x); }   /* (Display Properties' screen saver Preview: full screen, as Standard's) */
	else if (!strcmp(op, "ready")) { if (!a->rest) appwin_reveal(a); }   /* (its page has drawn the app at its window's size: shown; 1.6.1: not the resting page's) */
	else if (!strcmp(op, "login")) appwin_reveal(a);   /* (no native session: the page's own sign-in screen, which this window does need) */
	else if (!strcmp(op, "srch")) srch_ready(a);   /* (2026-11-16: the start menu's search, Standard's: its page is ready) */
	else if (!strcmp(op, "cat")) g_idle_add(apps_reload, NULL);   /* (a page gave its catalogue again: the statue's picture, N12; read at once, as the page gave it before it said so: the owner's Vivobook, 2026-11-16) */
	else if (!strcmp(op, "wins")) wins_ask(a);   /* (Task Manager: every window, as Standard's one page has them) */
	else if (!strcmp(op, "winop")) { jsstr(v, "id", x, sizeof x); jsstr(v, "do", f, sizeof f); win_op(x, f); }
	else if (!strcmp(op, "modal")) { jsstr(v, "on", x, sizeof x); appwin_modal(a, x[0] == '1'); }   /* (a page's dialog: over the whole screen, as Standard's) */
	else if (!strcmp(op, "prog")) { jsstr(v, "v", x, sizeof x); tbp_app(a, x); }   /* (1.6.1 Part 25: its work in progress, for its minimized taskbar button) */
	else if (!strcmp(op, "pwr")) { jsstr(v, "a", x, sizeof x); if (!strcmp(x, "reboot") || !strcmp(x, "poweroff")) power(x); }   /* (1.6.0: Restart into Windows, once the page has asked the firmware) */
	else if (!strcmp(op, "size") && !a->sized) { a->sized = 1; ev_flush(a);
		/* (a page doing something for the shell (#app=do-NAME) that made an app's window is that app's window now: the taskbar's
		   button and what is dropped on it are the app's) */
		jsstr(v, "app", x, sizeof x); if (!strncmp(a->k, "do-", 3) && !is_bg(a) && key_ok(x)) { snprintf(a->k, sizeof a->k, "%s", x); bar_redraw(); } jsstr(v, "ww", x, sizeof x); int ww = atoi(x); jsstr(v, "wh", x, sizeof x); int wh = atoi(x); appwin_size(a, ww, wh); }
}
static gboolean on_crash_reload(gpointer v) { webkit_web_view_reload(WEBKIT_WEB_VIEW(v)); return G_SOURCE_REMOVE; }
static void on_crash(WebKitWebView *v, WebKitWebProcessTerminationReason r, gpointer d)
{
	fprintf(stderr, "rudolf-native: an app's page stopped (%d): loading it again\n", (int)r); if (d) ((AppWin *)d)->sready = 0;
	if (d && is_bg(d)) { ((AppWin *)d)->pready = 0; BG_OPEN_T = g_get_monotonic_time(); }   /* (1.6.1: the hidden page loaded again) */
	g_timeout_add_seconds(1, on_crash_reload, v);
}
static void clients_forget(AppWin *a); static void kbcap_gone(AppWin *a); static void srch_gone(AppWin *a);
static void on_win_destroy(GtkWidget *w, gpointer d)
{
	(void)w; AppWin *a = d; WINS = g_list_remove(WINS, a); if (ACTIVE == a) ACTIVE = NULL; if (a->wait_t) { g_source_remove(a->wait_t); a->wait_t = 0; }
	if (PREV_S && PREV_W == a->id) prev_end();   /* (Settings closed with its choices not applied: the saved ones again) */
	kbcap_gone(a); srch_gone(a); if (a->pix) { g_object_unref(a->pix); a->pix = NULL; }
	clients_forget(a);   /* (its frame goes when X says so: not this window's any more) */
	g_free(a->rurl); g_free(a); bar_redraw(); state_write();
}
static gboolean on_win_focus(GtkWidget *w, GdkEvent *e, gpointer d) { (void)w; (void)e; ACTIVE = d; bar_redraw(); return FALSE; }
/* ask the app to close, as its ✕ does (it may ask about unsaved work); a second ask after 5 s closes it anyway */
static void appwin_ask_close(AppWin *a)
{
	if (a->rest) { appwin_close_now(a); return; }   /* (1.6.1: a resting window has no page to ask, and nothing unsaved: it rested only so) */
	gint64 now = g_get_monotonic_time();
	if (a->closing && now - a->close_t > 5000000) { appwin_close_now(a); return; }
	a->closing = 1; a->close_t = now;
	webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(a->view), "window.__oneAppClose?window.__oneAppClose():window.close()", -1, NULL, NULL, NULL, NULL, NULL);
}
static gboolean on_delete(GtkWidget *w, GdkEvent *e, gpointer d) { (void)w; (void)e; appwin_ask_close(d); return TRUE; }
/* an app window, its view related to rel's (then WebKit loads it, as a window the page opened) or loading its own */
static GtkWidget *on_create(WebKitWebView *v, WebKitNavigationAction *na, gpointer d);
/* (1.6.0 Part 17) a window another window opened (a dialog, Settings at a page) opens side to side: an app window's page asked for it
   within the last 3 s (File Explorer opening a file: corner to corner, as from the desktop): LP_S2S_T */
/* an app window's view, its page's settings and the shell's signals on it (1.6.1 Part 23 C: also a resting window's empty one, and the
   view its page comes back in) */
static GtkWidget *view_make(AppWin *a, WebKitWebView *rel)
{
	GtkWidget *view = rel ? GTK_WIDGET(g_object_new(WEBKIT_TYPE_WEB_VIEW, "related-view", rel, "user-content-manager", UCM, NULL))
		: GTK_WIDGET(g_object_new(WEBKIT_TYPE_WEB_VIEW, "web-context", CTX, "user-content-manager", UCM, NULL));
	WebKitSettings *st = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(view));
	g_object_set(st, "enable-javascript", TRUE, "javascript-can-access-clipboard", TRUE, "enable-webaudio", TRUE, "enable-media-stream", TRUE,
		"enable-webgl", TRUE, "media-playback-requires-user-gesture", FALSE, "enable-page-cache", FALSE, "enable-write-console-messages-to-stdout", TRUE,
		"javascript-can-open-windows-automatically", TRUE, NULL);
	webkit_settings_set_hardware_acceleration_policy(st, HWPOL);
#if WEBKIT_CHECK_VERSION(2, 42, 0)
	/* (test findings N7, N8) navigator.storage.estimate(), which Chromium has: File Explorer's status line and Settings › About say
	   how much of its storage the page uses (WebKitGTK leaves the Storage API off) */
	{ WebKitFeatureList *fl = webkit_settings_get_all_features();
	  for (gsize i = 0; i < webkit_feature_list_get_length(fl); i++) { WebKitFeature *ft = webkit_feature_list_get(fl, i); const char *id = webkit_feature_get_identifier(ft);
		if (!strcmp(id, "StorageAPI") || !strcmp(id, "StorageAPIEstimate")) webkit_settings_set_feature_enabled(st, ft, TRUE); }
	  webkit_feature_list_unref(fl); }
#endif
	g_signal_connect(view, "decide-policy", G_CALLBACK(on_decide), NULL);
	g_signal_connect(view, "permission-request", G_CALLBACK(on_perm), NULL);
	g_signal_connect(view, "notify::title", G_CALLBACK(on_title), a);
	g_signal_connect(view, "close", G_CALLBACK(on_view_close), a);
	g_signal_connect(view, "web-process-terminated", G_CALLBACK(on_crash), a);
	g_signal_connect(view, "create", G_CALLBACK(on_create), a);
	g_signal_connect(view, "size-allocate", G_CALLBACK(on_view_alloc), a);
	g_signal_connect(view, "load-changed", G_CALLBACK(on_load_ready), a);
	return view;
}
static AppWin *appwin_new(const char *k, WebKitWebView *rel, int id)
{
	AppWin *a = g_new0(AppWin, 1); snprintf(a->k, sizeof a->k, "%s", k); a->id = id; a->born = g_get_monotonic_time();
	if (LP_S2S_T && g_get_monotonic_time() - LP_S2S_T < 3000000) { a->s2s = 1; LP_S2S_T = 0; }
	const App *ap = app_find(k); snprintf(a->title, sizeof a->title, "%s", ap ? ap->name : k); snprintf(a->icon, sizeof a->icon, "%s", ap ? ap->icon : "");
	a->view = view_make(a, rel);
	a->win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(a->win), a->title); gtk_window_set_decorated(GTK_WINDOW(a->win), FALSE);
	int n = g_list_length(WINS), w = SW * 7 / 10, h = work_h() * 8 / 10; if (w < 640) w = SW; if (h < 400) h = work_h();
	gtk_window_set_default_size(GTK_WINDOW(a->win), w, h);
	gtk_window_move(GTK_WINDOW(a->win), 40 + (n % 8) * 28, work_top() + 20 + (n % 8) * 28);
	gtk_container_add(GTK_CONTAINER(a->win), a->view);
	g_signal_connect(a->win, "destroy", G_CALLBACK(on_win_destroy), a);
	g_signal_connect(a->win, "delete-event", G_CALLBACK(on_delete), a);
	g_signal_connect(a->win, "focus-in-event", G_CALLBACK(on_win_focus), a);
	WINS = g_list_append(WINS, a); ACTIVE = a;
	return a;
}
static void wm_manage_app(AppWin *a);
static void appwin_show(AppWin *a) { if (a->hidden) { gtk_widget_realize(a->view); return; } gtk_widget_show_all(a->win); wm_manage_app(a); gtk_window_present(GTK_WINDOW(a->win)); bar_redraw(); state_write(); }
static void on_ready(WebKitWebView *v, gpointer d) { (void)v; appwin_show(d); }
static char *app_url(const char *k, const char *file, int id, const char *extra)
{
	char *fe = file ? g_uri_escape_string(file, NULL, FALSE) : NULL, *ke = g_uri_escape_string(k, NULL, FALSE);   /* (lx:...: escaped, as "+" would be a space) */
	char *u = g_strdup_printf("http://%s:%d/#app=%s&w=%d&sw=%d&sh=%d%s%s%s", HOST, PORT, ke, id, SW, SH, fe ? "&file=" : "", fe ? fe : "", extra ? extra : ""); g_free(fe); g_free(ke); return u;
}
/* windows asked for and not made yet (made through an open window's window.open: see app_open) */
typedef struct { char k[KN]; int id, hidden, reveal; AppWin *into; } Pending;   /* (into: a resting window's page coming back, 1.6.1) */
static GQueue PENDING = G_QUEUE_INIT;
static GtkWidget *on_create(WebKitWebView *v, WebKitNavigationAction *na, gpointer d)
{
	(void)na; (void)d; Pending *p = g_queue_pop_head(&PENDING);
	if (!p) return NULL;
	if (!USER_ID[0]) { g_free(p); return NULL; }   /* (asked for before signing out, as the start menu's search page: not made after) */   /* (a page's own window.open: the desktop page sends websites to the browser itself) */
	/* (1.6.1 Part 23 C: a resting window's page, back in its own window: a view of the opener's process, as every window's) */
	if (p->into) { AppWin *a = g_list_find(WINS, p->into) ? p->into : NULL; g_free(p); if (!a || !a->win) return NULL;
		GtkWidget *nv = view_make(a, v), *old = a->view; a->view = nv; if (old) gtk_container_remove(GTK_CONTAINER(a->win), old);
		gtk_container_add(GTK_CONTAINER(a->win), nv); gtk_widget_show(nv); gtk_widget_grab_focus(nv); return nv; }   /* (the keyboard's in its window: to its page) */
	AppWin *a = appwin_new(p->k, v, p->id); a->hidden = p->hidden; a->reveal = p->reveal; g_free(p);
	g_signal_connect(a->view, "ready-to-show", G_CALLBACK(on_ready), a);
	return a->view;
}
/* an app in a window of its own. All app windows share one WebKit web process: the first starts it, the others are
   opened by an open one (window.open: the page's own, webNativeOpen, as the page sends websites elsewhere), which WebKit keeps in its process (a view merely made "related" to another
   got a process of its own, WebKitGTK 2.52, as navigations may change process) */
static void app_open(const char *k, const char *file) { app_open_ex(k, file, NULL, 0); }
/* (extra: more of the address, already escaped; hidden: a page the shell asks to do something, never shown) */
/* 1.6.2 Part 14 (the owner's Vivobook, 2026-11-22: "taskmgr's page is not ready after 2500 ms", then "never said it was ready", while
   Steam started and the hidden page's desktop/apps took 192 s) an app's page is opened by window.open from the first page with an
   address (the shell's hidden page, do-bg), so it is in that page's web process (one process for them: their memory), and waits for
   it: Task Manager, the app for a desktop that is stuck, came as a blank cover, then as it was. Task Manager's page has a web process
   of its own, as Standard's Task Manager is not held back by another app's work; the other apps' pages stay together. */
static int own_process(const char *k) { return k && !strcmp(k, "taskmgr"); }
static void app_open_ex(const char *k, const char *file, const char *extra, int hidden)
{
	if (!USER_ID[0] || LOCKED || !key_ok(k)) return;
	/* 1.6.1 (the owner's Vivobook, 2026-11-20: a web process of the whole desktop page for each) a single action the shell has a page do
	   unseen (do-qs, do-nc, do-pin, do-mute, do-desk, do-fs, do-short) goes to its one hidden page, signed in already (bg_route); a
	   page of its own only while that one is not there yet, or does not take it */
	if (hidden == 1 && bg_route(k, file, extra)) return;
	/* (2026-11-16, the owner's Vivobook) a Linux or Windows program, or a Library app rudolfd runs (the catalogue's "x"): its page opens it unseen,
	   as Standard's launch() starts it, and is shown only if it makes a window of the page's after all (a question, a download's dialog) */
	int reveal = hidden == 2; if (reveal) hidden = 1;   /* (2: the same for a page of the shell's: a file opened from the start menu's search) */
	{ const App *ap = app_find(k); if (!hidden && ap && ap->x) { hidden = 1; reveal = 1; } }
	/* (1.5.9, docs/plan-1.5.9.md 12.3c) Oracle runs in Rudolf's own Chromium with the shell's real tabs, as on Standard; without
	   Chromium or the shell, here as before, and the page says once that websites work better on Standard */
	static int nochrome_said;
	if (!strcmp(k, "browser") && !hidden) { if (oracle_open(file, extra)) return; if (!nochrome_said++) { char *ex = g_strdup_printf("%s&nochrome=1", extra ? extra : ""); app_open_ex(k, file, ex, hidden); g_free(ex); return; } }
	ctx_make(); int id = ++NEXTID; char *u = app_url(k, file, id, extra);
	AppWin *rel = NULL; if (!own_process(k)) for (GList *l = WINS; l; l = l->next) { AppWin *x = l->data; if (x->view && webkit_web_view_get_uri(WEBKIT_WEB_VIEW(x->view)) && !x->closing) { rel = x; break; } }
	if (rel) {
		Pending *p = g_new0(Pending, 1); snprintf(p->k, sizeof p->k, "%s", k); p->id = id; p->hidden = hidden; p->reveal = reveal; g_queue_push_tail(&PENDING, p);
		char *js = g_strdup_printf("(typeof webNativeOpen=='function'?webNativeOpen:window.open)(%s,'_blank')", "decodeURIComponent('%s')"), *ue = g_uri_escape_string(u, NULL, FALSE), *js2 = g_strdup_printf(js, ue);
		webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(rel->view), js2, -1, NULL, NULL, NULL, NULL, NULL);
		g_free(js); g_free(js2); g_free(ue); g_free(u); return;
	}
	AppWin *a = appwin_new(k, NULL, id); a->hidden = hidden; a->reveal = reveal; webkit_web_view_load_uri(WEBKIT_WEB_VIEW(a->view), u); g_free(u);
	appwin_show(a);
}

/* ------------------------------------------------------------------ the window manager (step 4)
   rudolf-native is the X window manager (SubstructureRedirect on the root, through a GDK event filter, so this one
   GTK process does both). Every top-level window that is not override-redirect gets a frame: an X window of ours with
   the title bar (the title bar styles and colours of Display Properties, titlebar59.js: the active window coloured,
   the others grey), minimise, maximise and close; moving (the title bar), resizing (the edges), double-click to
   maximise, focus on click (XSetInputFocus and WM_TAKE_FOCUS, as rudolf-display gives it), WM_STATE, _NET_CLIENT_LIST,
   _NET_ACTIVE_WINDOW. Dialogs (WM_TRANSIENT_FOR) are centred over their window. Full screen (_NET_WM_STATE, games and
   videos) covers the whole screen, taskbar too, without the frame. Our own app windows are framed the same way (our
   own requests are not redirected to us, so they are framed as they are shown). The taskbar lists them all. */
static double tw_tbh(void);   /* (the title bar's height in the page's pixels: tw_geom) */
static int BW = 1;   /* (the page's windows' 1 px edge, 2 px with high contrast; the title bar's height, TH, follows the display scaling, the font size and the touch mode: tw_th) */
typedef struct Client { Window w, frame, rz[4]; int x, y, cw, ch, min, max, full, sx, sy, scw, sch, ignore_unmap, hover, press, hidden, ontop; char title[256]; AppWin *app; Window trans; Time last_click;
	char ak[40]; Window clip; int il, it, ir, ib; int ws, off, mk; gint64 oask, used; int opened; char ptitle[256]; int wait; Window cover; int lpclip; LPRect lpcr;
	int csd, gl, gr, gt, gb, nodeco, tbcut;
	int pg; double pgf, pgt; gint64 pgb0, pgch, pgend; } Client;   /* (1.6.1 Part 25: its page's progress, for its minimized taskbar button: pg 0 none, 1 a percentage (pgt; drawn from pgf, changed at pgch), 2 busy since pgb0, 3 ended at pgend) */   /* (1.6.1 Part 15: csd, the app draws its own title bar (_MOTIF_WM_HINTS, no decorations): Rudolf's frame keeps only its edge; gl..gb, GTK's
	   invisible margins around it (_GTK_FRAME_EXTENTS), cut away by the clip window as Oracle's own bar is) */   /* (ws: its workspace, 1 to NWS; off: not shown, another workspace's: item 4) */   /* (ak: the app another program's window is, as Oracle's Chromium; clip and the insets: its own title bar and edges hidden) */
static void wopen_start(Client *c);
static void tbp_set(Client *c, const char *v); static int oracle_at(Client *c, int x, int y, int w, int h);   /* (1.6.1 Part 25, below) */
static int mon_k(Client *c), mon_ox(Client *c), mon_oy(Client *c), mon_of_ws(int n); static void mon_go(int n), mon_move(Client *c, int n), mon_show(int k, int n, int focus), mon_keep(void), mon_drop(Client *c);
static void csd_extents(Client *c); static void csd_read(Client *c, XWindowAttributes *wa); static void tb_cut_apply(Client *c); static void csd_update(Client *c); static void net_state_sync(Client *c); static void csd_max_first(Client *c); static void csd_frame_draw(Client *c);   /* (1.6.1 Part 15, below) */
static int lp_min(Client *c); static void lp_unmin(Client *c); static int lp_maxi(Client *c, int on); static void lp_gone(Client *c);   /* (1.6.0 Part 17) */
static void lp_menu_open(GtkWidget *pw, int wx, int wy, int kind, double x, double y, double w, double h, double bx, double by, int corner);
static void lp_menu_closed(GtkWidget *pw, double x, double y, double w, double h); static void lp_tbb(Client *c); static int lp_dir(void); static void lp_draw_clip(cairo_t *cr, GtkWidget *pw, int wx, int wy); static int lp_hit_mid(const char *kind, const char *arg, double *x, double *y, LPRect *r);   /* (Part 17b) */
static Window LP_LITW; static double LP_LITA; static RGB LP_CORE;   /* (the taskbar button a point drops into, lit) */
static GList *CLIENTS;   /* oldest first */
static Client *FOCUS;
static Display *DPY; static Window ROOT; static int WM_OK;
static Atom A_PROTO, A_DEL, A_TAKE, A_WMSTATE, A_NSTATE, A_NFULL, A_NACTIVE, A_NLIST, A_NNAME, A_UTF8, A_NSUP, A_NCHECK, A_CHANGE, A_NHIDDEN;
static Cursor CUR[9];
static int wm_err;
static int wm_errh(Display *d, XErrorEvent *e) { (void)d; if (e->error_code == BadAccess) wm_err = 1; return 0; }
static void clients_forget(AppWin *a) { for (GList *l = CLIENTS; l; l = l->next) if (((Client *)l->data)->app == a) ((Client *)l->data)->app = NULL; }
static Client *client_of(Window w) { for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (c->w == w || c->frame == w || (w && (c->clip == w || c->rz[0] == w || c->rz[1] == w || c->rz[2] == w || c->rz[3] == w))) return c; } return NULL; }
static int rz_of(Client *c, Window w) { for (int i = 0; i < 4; i++) if (c->rz[i] && c->rz[i] == w) return i; return -1; }
static Client *client_of_app(AppWin *a) { for (GList *l = CLIENTS; l; l = l->next) if (((Client *)l->data)->app == a) return l->data; return NULL; }
static int fw_of(Client *c) { return c->cw + 2 * BW; }
static int th_of(Client *c) { return c->csd ? BW : TH; }   /* (the frame's top: its edge and Rudolf's title bar, or the edge alone: 1.6.1 Part 15) */
static int fh_of(Client *c) { return c->ch + th_of(c) + BW; }
static void wm_title(Client *c)
{
	if (c->app) { snprintf(c->title, sizeof c->title, "%s", c->app->title); return; }
	Atom type; int fmt; unsigned long n = 0, after; unsigned char *d = NULL; c->title[0] = 0;
	if (XGetWindowProperty(DPY, c->w, A_NNAME, 0, 256, False, A_UTF8, &type, &fmt, &n, &after, &d) == Success && d && n) snprintf(c->title, sizeof c->title, "%s", (char *)d);
	if (d) XFree(d);
	if (!c->title[0]) { char *nm = NULL; if (XFetchName(DPY, c->w, &nm) && nm) { snprintf(c->title, sizeof c->title, "%s", nm); XFree(nm); } }
	for (char *p = c->title; *p; p++) if (*p == '\n' || *p == '\t') *p = ' ';
	/* (Oracle's Chromium: an app's window is titled "Rudolf OS - PAGE'S TITLE", the shell's name first; the page's alone) */
	/* (1.6.0) the title its page has (rudolf-launch: "title X Y W H ..."), whatever Chromium calls its window: Rudolf's Chromium 158 kept
	   an Oracle window's own title at the shell's name ("Rudolf OS"), so the frame and the taskbar said "Oracle" for every page */
	if (c->ak[0] && c->ptitle[0]) { snprintf(c->title, sizeof c->title, "%s", c->ptitle); return; }
	if (c->ak[0] && !strncmp(c->title, "Rudolf OS - ", 12)) memmove(c->title, c->title + 12, strlen(c->title + 12) + 1);
	else if (c->ak[0] && !strcmp(c->title, "Rudolf OS")) { const App *ap = app_find(c->ak); snprintf(c->title, sizeof c->title, "%s", ap ? ap->name : "Oracle"); }
}
static void set_wm_state(Client *c, long st) { long v[2] = {st, None}; XChangeProperty(DPY, c->w, A_WMSTATE, A_WMSTATE, 32, PropModeReplace, (unsigned char *)v, 2); }
static void client_list(void)
{
	Window l[256]; int n = 0; for (GList *x = CLIENTS; x && n < 256; x = x->next) l[n++] = ((Client *)x->data)->w;
	XChangeProperty(DPY, ROOT, A_NLIST, XA_WINDOW, 32, PropModeReplace, (unsigned char *)l, n);
	Window a = FOCUS ? FOCUS->w : None; XChangeProperty(DPY, ROOT, A_NACTIVE, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&a, 1);
}
/* the client's real place on the screen, told to it (ICCCM 4.1.5), as a moved window does not hear it otherwise */
static void tell_geom(Client *c)
{
	XEvent e = {0}; e.xconfigure.type = ConfigureNotify; e.xconfigure.event = c->w; e.xconfigure.window = c->w;
	e.xconfigure.x = mon_ox(c) + (c->full ? 0 : c->x + BW); e.xconfigure.y = mon_oy(c) + (c->full ? 0 : c->y + th_of(c)); e.xconfigure.width = c->cw; e.xconfigure.height = c->ch;
	if (c->clip && !c->full) { e.xconfigure.x -= c->il; e.xconfigure.y -= c->it; e.xconfigure.width += c->il + c->ir; e.xconfigure.height += c->it + c->ib; }
	XSendEvent(DPY, c->w, False, StructureNotifyMask, &e);
}
static void frame_shape(Client *c);
/* (1.6.0 Part 12) the page's resize handles (base_v24.html addResize: .rsz, a few pixels inside each edge, larger at the
   corners): see-through input windows over the app's edges (left, right, bottom; the top edge is the title bar's) */
static int RZG(void) { return (int)lround(4 * Z); }
static void rz_place(Client *c)
{
	int fw = c->cw + 2 * BW, fh = c->ch + th_of(c) + BW, g = RZG(), TH = th_of(c);
	for (int i = 0; i < 4; i++) {
		if (!c->rz[i]) { XSetWindowAttributes at = {0}; at.event_mask = ButtonPressMask | ButtonReleaseMask | PointerMotionMask; c->rz[i] = XCreateWindow(DPY, c->frame, 0, 0, 1, 1, 0, 0, InputOnly, CopyFromParent, CWEventMask, &at); }
		if (c->max || c->full || (i == 3 && !c->csd)) { XUnmapWindow(DPY, c->rz[i]); continue; }
		if (i == 3) XMoveResizeWindow(DPY, c->rz[i], 0, 0, fw, (int)lround(3 * Z)); else if (i == 0) XMoveResizeWindow(DPY, c->rz[i], 0, TH, g, fh - TH); else if (i == 1) XMoveResizeWindow(DPY, c->rz[i], fw - g, TH, g, fh - TH); else XMoveResizeWindow(DPY, c->rz[i], 0, fh - g, fw, g);
		XMapRaised(DPY, c->rz[i]);
	}
}
static void rz_to_frame(Client *c, int i, int *x, int *y) { int TH = th_of(c), fw = c->cw + 2 * BW, fh = c->ch + TH + BW, g = RZG(); if (i == 3) return; if (i == 0) *y += TH; else if (i == 1) { *x += fw - g; *y += TH; } else *y += fh - g; }
static struct { Client *c; int edge, px, py, x, y, w, h, kbd, kx, ky; } DRAG;   /* edge: 0 move, else 1 left 2 right 4 top 8 bottom; kbd: with the keyboard, the pointer at kx ky (1.6.1 Part 15) */
static void place(Client *c)
{
	int TH = th_of(c), ox = mon_ox(c), oy = mon_oy(c), mw = NMON > 1 ? MON[mon_k(c)].w : SW, mh = NMON > 1 ? MON[mon_k(c)].h : SH; c->mk = mon_k(c);   /* (1.6.1 Part 1: on its monitor) */
	if (c->full) { XMoveResizeWindow(DPY, c->frame, ox - BW, oy - TH, mw + 2 * BW, mh + TH + BW); c->cw = mw; c->ch = mh; }
	else XMoveResizeWindow(DPY, c->frame, ox + c->x, oy + c->y, fw_of(c), fh_of(c));
	/* (Oracle's Chromium: its own title bar and edges cut away by the clip window; full screen, it draws none) */
	if (c->clip) { XMoveResizeWindow(DPY, c->clip, BW, TH, c->cw, c->ch);
		if (c->full) XMoveResizeWindow(DPY, c->w, 0, 0, c->cw, c->ch); else XMoveResizeWindow(DPY, c->w, -c->il, -c->it, c->cw + c->il + c->ir, c->ch + c->it + c->ib); }
	else XMoveResizeWindow(DPY, c->w, BW, TH, c->cw, c->ch);
	/* (our own app window: GTK resizes itself to the size it was last asked for (its requests are not redirected: the shell's
	   connection is GTK's), so it is asked for the window's size) */
	if (c->app && c->app->win && !c->full) { int gw = 0, gh = 0; gtk_window_get_size(GTK_WINDOW(c->app->win), &gw, &gh); if (gw != c->cw || gh != c->ch) gtk_window_resize(GTK_WINDOW(c->app->win), c->cw, c->ch); }
	frame_shape(c); rz_place(c); tell_geom(c); csd_extents(c); if (!DRAG.c) desk_shadows();
}
static void frame_draw(Client *c);
static void btn_rects(Client *c, int r[3][4]);
static void frames_redraw(void) { for (GList *l = CLIENTS; l; l = l->next) frame_draw(l->data); }
/* our own windows above the frames: the taskbar (unless a window is full screen), the start menu, the sign-in screen */
static void mon_restack(int part);
static void wm_restack(void)
{
	if (!WM_OK) return;
	if (DESK && gtk_widget_get_window(DESK)) XLowerWindow(DPY, GDK_WINDOW_XID(gtk_widget_get_window(DESK)));
	mon_restack(0);   /* (1.6.1 Part 1: the other monitors' wallpapers below, their taskbars above the windows, their covers on top) */
	for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (c->ontop && !c->min) XRaiseWindow(DPY, c->frame); }   /* (Always on top: item 3) */
	if (!(FOCUS && FOCUS->full) && BAR && gtk_widget_get_visible(BAR)) XRaiseWindow(DPY, GDK_WINDOW_XID(gtk_widget_get_window(BAR)));
	mon_restack(1);
	if (MENU && gtk_widget_get_visible(MENU)) XRaiseWindow(DPY, GDK_WINDOW_XID(gtk_widget_get_window(MENU)));
	if (SIGN && gtk_widget_get_visible(SIGN)) XRaiseWindow(DPY, GDK_WINDOW_XID(gtk_widget_get_window(SIGN)));
	mon_restack(2);
	if (PWR && gtk_widget_get_visible(PWR)) XRaiseWindow(DPY, GDK_WINDOW_XID(gtk_widget_get_window(PWR)));
	/* the development link's banner and its question above everything, full-screen windows too */
	if (DLBAN && gtk_widget_get_visible(DLBAN)) XRaiseWindow(DPY, GDK_WINDOW_XID(gtk_widget_get_window(DLBAN)));
	if (DLASK && gtk_widget_get_visible(DLASK)) XRaiseWindow(DPY, GDK_WINDOW_XID(gtk_widget_get_window(DLASK)));
}
static void grab_clicks(Client *c, int on)
{
	if (on) XGrabButton(DPY, AnyButton, AnyModifier, c->w, False, ButtonPressMask, GrabModeSync, GrabModeAsync, None, None);
	else XUngrabButton(DPY, AnyButton, AnyModifier, c->w);
}
static int has_proto(Window w, Atom p) { Atom *l = NULL; int n = 0, r = 0; if (XGetWMProtocols(DPY, w, &l, &n)) { for (int i = 0; i < n; i++) if (l[i] == p) r = 1; XFree(l); } return r; }
static void send_proto(Window w, Atom p, Time t)
{
	XEvent e = {0}; e.xclient.type = ClientMessage; e.xclient.window = w; e.xclient.message_type = A_PROTO; e.xclient.format = 32;
	e.xclient.data.l[0] = p; e.xclient.data.l[1] = t; XSendEvent(DPY, w, False, NoEventMask, &e);
}
#define NWS 3
#define BARWS (NMON > 1 ? MON[MONK_CUR].ws : WSC)   /* (1.6.1 Part 1: the workspace this taskbar's monitor shows) */
static int WSC = 1;   /* (the workspace shown: item 4) */
static void toast_text(const char *t, const char *b, int ms); static void fget_tt(void *v, const char *k, char *o, size_t n);
static char TLOG[32][200]; static int TLOG_N;   /* (the last toasts, for the tests' view) */
static int ws_of(Client *c); static void ws_apply(void); static gboolean ws_said(gpointer d);
static void appwin_wake(AppWin *a);
static void wm_focus(Client *c)
{
	if (c && !c->ws) c = NULL;   /* (a window going, in no workspace: 12.3c) */
	if (c && NMON > 1 && ws_of(c) != WSC) { if (c->off) mon_show(MAX(0, mon_of_ws(WSC)), ws_of(c), 0); else { WSC = ws_of(c); bar_redraw(); } }   /* (1.6.1 Part 1: shown on another monitor: that one is in use) */
	else if (c && c->off && ws_of(c) != WSC) { WSC = ws_of(c); ws_apply(); bar_redraw(); g_timeout_add(80, ws_said, GINT_TO_POINTER(WSC)); }   /* (W.focus: wsGo(W.ws) first) */
	Client *old = FOCUS; FOCUS = c;
	if (c && c->app && c->app->rest) appwin_wake(c->app);   /* (1.6.1 Part 23 C: its page loaded again, back where it was) */
	if (c) {
		c->used = g_get_monotonic_time();   /* (the page's MRU: item 5) */
		if (c->min) { c->min = 0; if (!c->off) XMapWindow(DPY, c->frame); set_wm_state(c, NormalState); net_state_sync(c); lp_unmin(c); }   /* (Part 17: the drop backward) */
		XRaiseWindow(DPY, c->frame);
		/* its dialogs stay above it */
		for (GList *l = CLIENTS; l; l = l->next) { Client *d = l->data; if (d->trans == c->w && !d->min) XRaiseWindow(DPY, d->frame); }
		XWMHints *h = XGetWMHints(DPY, c->w); int input = !h || !(h->flags & InputHint) || h->input; if (h) XFree(h);
		if (input) XSetInputFocus(DPY, c->w, RevertToPointerRoot, CurrentTime);
		/* (with a real server time: Wine takes the keys it reads through DirectInput only then, as rudolf-display found) */
		if (has_proto(c->w, A_TAKE)) send_proto(c->w, A_TAKE, DESK && gtk_widget_get_window(DESK) ? gdk_x11_get_server_time(gtk_widget_get_window(DESK)) : CurrentTime);
		grab_clicks(c, 0);
	} else if (DESK && gtk_widget_get_window(DESK) && !(MENU && gtk_widget_get_visible(MENU)) && !(SIGN && gtk_widget_get_visible(SIGN)))
		XSetInputFocus(DPY, GDK_WINDOW_XID(gtk_widget_get_window(DESK)), RevertToPointerRoot, CurrentTime);   /* (no window: the desktop has the keyboard, as the page's) */
	if (old && old != c && g_list_find(CLIENTS, old)) { grab_clicks(old, 1); frame_draw(old); }
	if (c) frame_draw(c);
	wm_restack(); client_list(); bar_redraw(); kb_sync(); state_write();
}
static Client *top_client(void)
{
	/* the topmost window still shown (stacking order from the server) */
	Window r, p, *k = NULL; unsigned n = 0; Client *best = NULL;
	if (XQueryTree(DPY, ROOT, &r, &p, &k, &n)) for (unsigned i = 0; i < n; i++) { Client *c = client_of(k[i]); if (c && !c->min && !c->hidden && !c->off && c->frame == k[i]) best = c; }
	if (k) XFree(k); return best;
}
/* ---- (1.6.0 Part 12, item 4) the workspaces, as the page's (shell25.js: NWS 3, WS; wsGo, wsMove): a window is in one; only the
   current one's windows are shown and listed on the taskbar; going to another brings its last used window to the front and
   says so ("Workspace N", a quiet pop-up of 1.2 s); a window focused in another workspace takes the desktop there */
static int ws_has(int n) { for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (!c->trans && c->ws == n) return 1; } return 0; }
static int ws_of(Client *c) { Client *o = c->trans ? client_of(c->trans) : NULL; return o && o != c ? o->ws : c->ws; }
static void max_geom_ext(Client *c); static void frame_draw(Client *c);
static void ws_apply(void)
{
	desk_shadows();
	for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; int off = NMON > 1 ? mon_of_ws(ws_of(c)) < 0 : ws_of(c) != WSC;   /* (1.6.1 Part 1: shown on any monitor) */
		if (NMON > 1 && !off && c->mk != mon_k(c)) { if (c->max) { c->max = 0; max_geom_ext(c); c->max = 1; } place(c); frame_draw(c); }
		if (off && !c->off) { c->off = 1; if (!c->min && !c->hidden) XUnmapWindow(DPY, c->frame); }
		else if (!off && c->off) { c->off = 0; if (!c->min && !c->hidden) XMapWindow(DPY, c->frame); } }
	if (FOCUS && FOCUS->off) FOCUS = NULL;
}
/* ("Workspace N", once the taskbar shows it: the pop-up's window keeps a copy of what is behind it when it comes) */
static gboolean ws_said(gpointer d) { char t[32]; snprintf(t, sizeof t, "Workspace %d", GPOINTER_TO_INT(d)); if (!S.dnd) toast_text(t, "", 1200); return G_SOURCE_REMOVE; }
static void ws_go(int n)
{
	if (NMON > 1) { mon_go(n); return; }   /* (1.6.1 Part 1) */
	if (n < 1) n = 1; if (n > NWS) n = NWS; if (n == WSC) { bar_redraw(); return; }
	WSC = n; ws_apply(); wm_focus(top_client()); bar_redraw(); state_write();
	g_timeout_add(80, ws_said, GINT_TO_POINTER(WSC));
}
static void ws_move(Client *c, int n) { if (!c) return; if (NMON > 1) { mon_move(c, n); return; } if (n < 1) n = 1; if (n > NWS) n = NWS; c->ws = n; ws_apply(); if (!FOCUS) wm_focus(top_client()); bar_redraw(); state_write(); }
/* ---- 1.6.1 (docs/plan-1.6.1.md Part 2): the mobile build's three panels, the groundwork only, as the page's (panels61.js).
   On the mobile build only (/etc/rudolf/build-kind is "mobile"; RUDOLF_NATIVE_BUILD_KIND names another file for the tests):
   a panel is a workspace shown full screen (each window maximised as it comes, its title bar kept), and a horizontal
   swipe (a finger, which X gives as the pointer, or the pointer itself) on the desktop or on a window's title bar
   switches to the next workspace (to the left) or the one before (to the right), decided as the page decides it
   (pn_swipe = pnSwipe); the keys (Ctrl+Alt+arrows) as before. On the desktop builds nothing here does anything. The
   phone's own interface (its status bar, shade, app grid) is 2.0's. */
static int pn_on(void)
{
	static int on = -1;
	if (on < 0) { const char *f = getenv("RUDOLF_NATIVE_BUILD_KIND"); char *k = NULL; g_file_get_contents(f && *f ? f : "/etc/rudolf/build-kind", &k, NULL, NULL); on = k && !strncmp(k, "mobile", 6); g_free(k); }
	return on;
}
static int pn_swipe(int dx, int dy, long ms, int w)
{
	double far = MAX(48, w * 0.15);
	if (ms < 0 || ms > 900 || abs(dx) < far || abs(dx) < abs(dy) * 1.5) return 0;
	return dx < 0 ? 1 : -1;
}
static void pn_go(int d) { int n = WSC + d; if (d && n >= 1 && n <= NWS) ws_go(n); }
static struct { Client *c; int px, py; Time t; } PN;
static void wm_maximise_now(Client *c, int on);
static void pn_client(Client *c) { if (pn_on() && !c->trans && !c->full) wm_maximise_now(c, 1); }
/* a press on a maximised window's title bar (not on its buttons): followed to its release */
static int pn_press(Client *c, XButtonEvent *e)
{
	if (!pn_on() || !c->max || c->full || e->y >= th_of(c)) return 0;
	PN.c = c; PN.px = e->x_root; PN.py = e->y_root; PN.t = e->time;
	XGrabPointer(DPY, c->frame, False, ButtonReleaseMask | PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None, e->time);
	return 1;
}
static int pn_release(XButtonEvent *e)
{
	if (!PN.c) return 0;
	XUngrabPointer(DPY, e->time); PN.c = NULL;
	int dx = e->x_root - PN.px, dy = e->y_root - PN.py, d = pn_swipe(dx, dy, (long)(e->time - PN.t), SW);
	fprintf(stderr, "rudolf-native: a title bar's swipe %d,%d in %ld ms: %s\n", dx, dy, (long)(e->time - PN.t), d ? (d > 0 ? "the next workspace" : "the one before") : "none");
	pn_go(d); return 1;
}
/* on the desktop: seen before its own handlers (the icons, the selection box), which go on as before */
static gboolean pn_desk_event(GtkWidget *w, GdkEvent *e, gpointer d)
{
	(void)w; (void)d; static double px, py; static guint32 pt; static int down;
	if (e->type == GDK_BUTTON_PRESS && e->button.button == 1) { px = e->button.x_root; py = e->button.y_root; pt = e->button.time; down = 1; }
	else if (e->type == GDK_BUTTON_RELEASE && e->button.button == 1 && down) { down = 0; int dx = (int)(e->button.x_root - px), dy = (int)(e->button.y_root - py), d = pn_swipe(dx, dy, (long)(e->button.time - pt), SW);
		if (d) fprintf(stderr, "rudolf-native: the desktop's swipe %d,%d: %s\n", dx, dy, d > 0 ? "the next workspace" : "the one before"); pn_go(d); }
	return FALSE;
}
static void pn_init(GtkWidget *desk) { if (pn_on()) g_signal_connect(desk, "event", G_CALLBACK(pn_desk_event), NULL); }
static void wm_minimise(Client *c)
{
	desk_shadows();
	if (c->min) return; c->min = 1; if (!lp_min(c)) XUnmapWindow(DPY, c->frame); set_wm_state(c, IconicState); net_state_sync(c);   /* (Part 17: unmapped once it has dropped into its button) */
	if (FOCUS == c) { FOCUS = NULL; wm_focus(top_client()); }
	bar_redraw(); state_write();
}
/* (the owner's photo) a maximised window where the page puts it (base_v24.html .mx: left 0, top 0 of #desk, its width and height #desk's
   less 6 px, its edge and round corners kept): #desk is the screen beside the taskbar (below a bar at the top, 2 px apart; over a bar at
   the bottom by 4 px; beside one at the left or the right, 4 px apart) */
static int desk_top(void) { return TOPRES + (!strcmp(S.tb, "top") ? BARH + (int)lround(2 * Z) : 0); }
static void max_geom(Client *c)
{
	int top = !strcmp(S.tb, "top"), dt = desk_top(), dh = tb_side() ? SH - TOPRES : top ? SH - dt : SH - TOPRES - BARH + (int)lround(4 * Z);
	c->x = desk_left(); c->y = dt; c->cw = (int)lround(desk_w() - 6 * Z); c->ch = (int)lround(dh - 6 * Z) - (th_of(c) - BW);
	if (NMON > 1) { int k = mon_k(c); c->cw += MON[k].w - SW; c->ch += MON[k].h - SH; }   /* (1.6.1 Part 1: its own monitor's desk) */
}
static void max_geom_ext(Client *c) { max_geom(c); }
static void wm_maximise_now(Client *c, int on);
static void wm_maximise(Client *c, int on)
{
	if (c->full || on == c->max) return;
	if (lp_maxi(c, on)) return;   /* (Part 17: the frame grows or gathers first; the size changes once, from there) */
	wm_maximise_now(c, on);
}
static void wm_maximise_now(Client *c, int on)
{
	if (c->full || on == c->max) return;
	if (on) { c->sx = c->x; c->sy = c->y; c->scw = c->cw; c->sch = c->ch; max_geom(c); }
	else { c->x = c->sx; c->y = c->sy; c->cw = c->scw; c->ch = c->sch; }
	c->max = on; place(c); frame_draw(c); net_state_sync(c); state_write();
}
static void wm_fullscreen(Client *c, int on)
{
	if (on == c->full) return;
	if (on) { if (!c->max) { c->sx = c->x; c->sy = c->y; c->scw = c->cw; c->sch = c->ch; } c->full = 1; }
	else { c->full = 0; if (c->max) { c->max = 0; wm_maximise(c, 1); } else { c->x = c->sx; c->y = c->sy; c->cw = c->scw; c->ch = c->sch; } }
	net_state_sync(c);
	place(c); frame_draw(c); wm_focus(c);
}
static void wm_close(Client *c)
{
	if (c->app) { appwin_ask_close(c->app); return; }   /* (our own: the page is asked, as its ✕ does) */
	/* (1.5.9, 12.3c) Oracle's window: its page is asked, as its own ✕ does (Chromium's window closed by the window manager asked
	   "Download is in progress: exit Chromium anyway?", which Standard never shows); rudolf-launch finds the page by the window's place.
	   A second ✕ after 5 s closes it anyway */
	gint64 now = g_get_monotonic_time();
	if (c->ak[0] && !c->trans && (OR_RUN || (TOR_CTL_P && TOR_CTL_P[0])) && (!c->oask || now - c->oask < 5000000)) { if (c->oask) return; c->oask = now;
		int x = 0, y = 0; Window ch; XWindowAttributes wa; if (XTranslateCoordinates(DPY, c->w, ROOT, 0, 0, &x, &y, &ch) && XGetWindowAttributes(DPY, c->w, &wa)) {
			char l[96]; snprintf(l, sizeof l, "close %d %d %d %d\n", x, y, wa.width, wa.height); oracle_tell(l); return; } }
	if (has_proto(c->w, A_DEL)) send_proto(c->w, A_DEL, CurrentTime); else XKillClient(DPY, c->w);
}
/* ------------------------------------------------------------------ Oracle in Rudolf's Chromium (1.5.9, docs/plan-1.5.9.md 12.3c)
   On Standard, Oracle's websites are real browser pages: the desktop page runs in the shell (iso/shell, an Isolated Web App
   in Rudolf's Chromium), which gives Oracle its tabs (Controlled Frames). Here Oracle's window is that too: one Chromium for
   every Oracle window (rudolf-launch --oracle, which opens the shell's windows over Chromium's private pipe, as the session's
   Standard does), the shell with #app=browser&... (it passes its # on to the page: Oracle alone in one-app mode, with the real
   tabs), Standard's profile and flags (the session writes them: /run/rudolf/oracle-chrome, its first line the shell's ID, then
   the program and its arguments, one a line). Chromium starts with the first Oracle window and goes with the last; its windows
   are framed here as the others, as Oracle (its picture, its title, the page's), with Chromium's own title bar and edges cut
   away (an Isolated Web App's window always has them: neither the "use system title bar" setting nor "borderless" applies).
   RUDOLF_ORACLE_CMD and RUDOLF_LAUNCH change the files (tests); RUDOLF_ORACLE_INSET="TOP LEFT RIGHT BOTTOM" Chromium's edges. */
static guint OR_BUSY;
static void oracle_busy(int on)
{
	if (!WM_OK) return;
	static Cursor watch; if (!watch) watch = XCreateFontCursor(DPY, XC_watch);
	XDefineCursor(DPY, ROOT, on ? watch : CUR[0]); XFlush(DPY);
	if (OR_BUSY) { g_source_remove(OR_BUSY); OR_BUSY = 0; }
}
static gboolean oracle_busy_end(gpointer d) { (void)d; OR_BUSY = 0; oracle_busy(0); return G_SOURCE_REMOVE; }
/* (12.3c step 5) what Oracle's page in Chromium tells the native desktop (oneapp59.js oaPost, through rudolf-launch's binding
   rudolfNativeOracle): "msg JSON" lines, as the WebKit windows' rudolfNative messages. Another app asked for: a window of
   its own (Settings at a page: Settings' own window there) */
static void tb_settings(const char *page); static void tb_jump(const char *k, const char *label);
static int tor_open(const char *urls); static void tor_back(const char *urls);   /* (1.5.9, Part 16: Oracle's Tor windows) */
static void oracle_hide(int x, int y, int w, int h); static void oracle_title(int x, int y, int w, int h, const char *t);
static gboolean oracle_msgs(GIOChannel *ch, GIOCondition c, gpointer d)
{
	(void)d; if (c & (G_IO_HUP | G_IO_ERR)) { if (!(c & G_IO_IN)) return G_SOURCE_REMOVE; }
	gchar *l = NULL; gsize n = 0; GIOStatus st = g_io_channel_read_line(ch, &l, &n, NULL, NULL);
	if (st == G_IO_STATUS_EOF || st == G_IO_STATUS_ERROR) { g_free(l); return G_SOURCE_REMOVE; }
	char sid[80] = ""; const char *js = NULL; int hx, hy, hw, hh;
	/* (rudolf-launch: Oracle's last window is closing while a download is still arriving: off the screen and the taskbar until its
	   page has filed it and closes) */
	if (l && sscanf(l, "hide %d %d %d %d", &hx, &hy, &hw, &hh) == 4) { oracle_hide(hx, hy, hw, hh); g_free(l); return G_SOURCE_CONTINUE; }
	{ char pv[8] = ""; if (l && sscanf(l, "prog %d %d %d %d %7s", &hx, &hy, &hw, &hh, pv) == 5) {   /* (1.6.1 Part 25: an Oracle window's progress, found by its place as its title is) */
		for (GList *k = CLIENTS; k; k = k->next) { Client *c = k->data; if (oracle_at(c, hx, hy, hw, hh)) { tbp_set(c, strcmp(pv, "-") ? pv : ""); break; } }
		g_free(l); return G_SOURCE_CONTINUE; } }
	{ int nn = 0; if (l && sscanf(l, "title %d %d %d %d %n", &hx, &hy, &hw, &hh, &nn) == 4 && nn > 0 && l[nn] == '"') {
		g_strchomp(l); char *o = g_strdup_printf("{\"t\":%s}", l + nn); JV *v = jparse(o); char tt[256] = ""; if (v) { jstr(v, "t", tt, sizeof tt); jfree(v); }
		g_free(o); if (tt[0]) oracle_title(hx, hy, hw, hh, tt); g_free(l); return G_SOURCE_CONTINUE; } }
	if (l && !strncmp(l, "msg ", 4)) { const char *b = strchr(l + 4, ' '); if (b && b - (l + 4) < (long)sizeof sid && l[4] != '{') { memcpy(sid, l + 4, b - (l + 4)); sid[b - (l + 4)] = 0; js = b + 1; } }
	if (js && js[0] == '{' && strspn(sid, "0123456789ABCDEFabcdef") == strlen(sid)) { JV *v = jparse(js); char op[16] = "", app[KN] = "", file[1024] = "", pg[48] = "", jp[64] = "", u[1024] = "", pv[4] = "";
		if (v) { jstr(v, "op", op, sizeof op); jstr(v, "app", app, sizeof app); jstr(v, "file", file, sizeof file); jstr(v, "page", pg, sizeof pg); jstr(v, "jump", jp, sizeof jp); jstr(v, "url", u, sizeof u); jstr(v, "priv", pv, sizeof pv); }
		/* (an app's jump, as its taskbar button's menu: Oracle's New private tab, a window of its own) */
		if (!strcmp(op, "open") && app[0] && jp[0] && USER_ID[0] && !LOCKED) { tb_jump(app, jp); }   /* (the page knows its jumps; one it does not know closes its window) */
		else if (!strcmp(op, "open") && app[0] && USER_ID[0] && !LOCKED) { char *ex = open_extra(u, pv); if (!strcmp(app, "settings") && pg[0]) tb_settings(pg); else app_open_ex(app, file[0] ? file : NULL, ex, 0); g_free(ex); }
		else if (!strcmp(op, "sfx") && v) { char k[16] = "", vv[24] = "", pj[4096] = ""; jstr(v, "k", k, sizeof k); jstr(v, "v", vv, sizeof vv); jstr(v, "p", pj, sizeof pj); snd_page(k, g_ascii_strtod(vv, NULL), pj); }   /* (2026-11-16: Oracle's window's sounds) */
		else if (!strcmp(op, "mirror") && v) { char id[64] = ""; jstr(v, "id", id, sizeof id); if (id_ok(id)) mirror_to_webkit(id); }   /* (the files Oracle's page saved: the app windows take them) */
		else if (!strcmp(op, "set") && v) { char id[64] = ""; jstr(v, "id", id, sizeof id); if (id_ok(id)) settings_changed(id, NULL); }   /* (Oracle's page saved its settings) */
		else if (!strcmp(op, "osd") && v) { char k[16] = "", lv[8] = "", lb[256] = "", n[8] = ""; jstr(v, "k", k, sizeof k); jstr(v, "lv", lv, sizeof lv); jstr(v, "label", lb, sizeof lb); jstr(v, "n", n, sizeof n); osd_show(k, lv, lb, n); }
		else if (!strcmp(op, "openwith") && v) { char key[KN] = ""; jstr(v, "key", key, sizeof key); open_with(file, key); }   /* (Open with: the app's own window) */
		else if ((!strcmp(op, "tor") || !strcmp(op, "untor")) && v && USER_ID[0] && !LOCKED) { static char us[13 * 2048]; jstr(v, "url", us, sizeof us); if (op[0] == 't') tor_open(us); else tor_back(us); }   /* (Part 16: a Tor window with these tabs; and back) */
		else if (!strcmp(op, "toast") && v) toast_addf(NULL, sid, fget_jv, v);   /* (the page's notification: the native shell's pop-up) */
		else if (!strcmp(op, "note") && v) nt_notef(fget_jv, v);   /* (and in the centre, counted, at once) */
		jfree(v); }
	g_free(l); return G_SOURCE_CONTINUE;
}
/* a toast's click, for Oracle's page: its notification's own action, run in that page (rudolf-launch: act SESSION ID) */
static void oracle_act(const char *sid, const char *nid)
{
	if ((!OR_RUN && !(TOR_CTL_P && TOR_CTL_P[0])) || strspn(nid, "0123456789.") != strlen(nid) || !nid[0] || strspn(sid, "0123456789ABCDEFabcdef") != strlen(sid)) return;
	char line[160]; snprintf(line, sizeof line, "act %s %s\n", sid, nid);
	int f = open(OR_CTL, O_WRONLY | O_NONBLOCK | O_CLOEXEC); if (f >= 0) { if (write(f, line, strlen(line)) < 0) {} close(f); }
	if (TOR_CTL_P && TOR_CTL_P[0]) { f = open(TOR_CTL_P, O_WRONLY | O_NONBLOCK | O_CLOEXEC); if (f >= 0) { if (write(f, line, strlen(line)) < 0) {} close(f); } }   /* (a Tor window's page: Part 16) */
}
/* (1.6.0 Part 21) a line for Oracle's rudolf-launch only (not the Tor windows') */
static void oracle_only(const char *line)
{
	int f = OR_RUN ? open(OR_CTL, O_WRONLY | O_NONBLOCK | O_CLOEXEC) : -1; if (f >= 0) { if (write(f, line, strlen(line)) < 0) {} close(f); }
}
/* a line for rudolf-launch (its FIFO): what Oracle's pages are to do */
static void oracle_tell(const char *line)
{
	int f = OR_RUN ? open(OR_CTL, O_WRONLY | O_NONBLOCK | O_CLOEXEC) : -1; if (f >= 0) { if (write(f, line, strlen(line)) < 0) {} close(f); }
	/* (and the Tor windows' rudolf-launch: their pages and windows, Part 16) */
	f = TOR_CTL_P && TOR_CTL_P[0] ? open(TOR_CTL_P, O_WRONLY | O_NONBLOCK | O_CLOEXEC) : -1; if (f >= 0) { if (write(f, line, strlen(line)) < 0) {} close(f); }
}
/* (12.3c) the files Oracle's page saved (rudolfd's mirror has them): every app window's page takes them into WebKit's storage */
static void mirror_to_webkit(const char *id)
{
	char js[160]; snprintf(js, sizeof js, "window.__oaMirrorIn&&__oaMirrorIn('%s')", id);
	for (GList *l = WINS; l; l = l->next) { AppWin *a = l->data; if (a->view && !a->closing) webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(a->view), js, -1, NULL, NULL, NULL, NULL, NULL); }
}
/* (1.6.0, the owner's report 3) a window saved its settings (oneapp59.js op "set", once its storage is in rudolfd's copy): what the
   shell draws read again now, not at its next look (every 3 s), and every other app window and Oracle's pages told (__oaSetIn),
   as Standard's one page changes everything at once; WebKit's storage events need not cross between the windows' processes */
static void settings_load(void);
static void apps_load(void);
static void settings_changed(const char *id, AppWin *from)
{
	if (!USER_ID[0] || strcmp(id, USER_ID)) return;
	if (PREV_KEEP) prev_end();   /* (Settings' choices applied: the saved ones now, the same) */
	double g0 = S.gen; settings_load(); if (S.gen != g0) apps_load();
	char js[160]; snprintf(js, sizeof js, "window.__oaSetIn&&__oaSetIn('%s')", id);
	for (GList *l = WINS; l; l = l->next) { AppWin *a = l->data; if (a != from && a->view && !a->closing) webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(a->view), js, -1, NULL, NULL, NULL, NULL, NULL); }
	char ln[100]; snprintf(ln, sizeof ln, "set %s\n", id); oracle_tell(ln);
}
/* (12.3c) Oracle's new window at an address, or a private one at an address: more of its address (#app=browser&url=...&priv=1) */
static char *open_extra(const char *url, const char *priv)
{
	GString *e = g_string_new(NULL);
	if (url && url[0] && (!strncmp(url, "http://", 7) || !strncmp(url, "https://", 8) || !strncmp(url, "about:", 6) || !strncmp(url, "file:///", 8))) {
		char *x = g_uri_escape_string(url, NULL, FALSE); g_string_append_printf(e, "&url=%s", x); g_free(x);
		if (priv && !strcmp(priv, "1")) g_string_append(e, "&priv=1"); }
	if (!e->len) { g_string_free(e, TRUE); return NULL; }
	return g_string_free(e, FALSE);
}
static void clip_keep_restore(void);
static void oracle_exited(GPid p, gint st, gpointer d) { (void)d; (void)st; g_spawn_close_pid(p); if (p == OR_PID) { OR_RUN = 0; OR_PID = 0; clip_keep_restore(); state_write(); } }
/* the shell's #...: letters, digits and the address's own punctuation (the rest escaped) */
static void oracle_hash(GString *h, const char *s) { for (const char *c = s; *c; c++) { if (g_ascii_isalnum(*c) || strchr("&=._-~%+", *c)) g_string_append_c(h, *c); else g_string_append_printf(h, "%%%02X", (unsigned char)*c); } }
/* (1.6.0 Part 22) "--user-data-dir=" for Oracle in the account's encrypted folder, or NULL (then Standard's profile, as before).
   L: oracle-chrome's lines (the shell's ID, then Chromium and its arguments). Its copy of the shell, when it has one and it is
   not the system's (a release changed it), is replaced first as the session does for Standard's profile (rudolf-shellfix: the
   shell's storage goes with it, so websites ask to sign in again, as there). */
/* (1.6.0 Part 22) the account's websites as they were before, in Standard's shared profile, not encrypted: removed once, when its
   encrypted profile is first made (the account signs in to websites again, as Settings says). Chromium keeps a partition in
   Default/Storage/ext/<domain>/<the first 6 bytes of SHA-256 of its name, in hex> (content/browser/storage_partition_impl_map.cc);
   the shell names this account's "web-a<ID>" (tabs26.js rtPart, the shell's partName), before 1.6.0 "web-u<hash of the name>". */
static void oracle_part_dir(const char *name, char *hex)
{
	gchar *h = g_compute_checksum_for_string(G_CHECKSUM_SHA256, name, -1); for (int i = 0; i < 12; i++) hex[i] = g_ascii_toupper(h[i]); hex[12] = 0; g_free(h);
}
static void oracle_old_parts_rm(gchar **L, int n)
{
	const char *shared = NULL; for (int i = 1; i < n; i++) if (!strncmp(L[i], "--user-data-dir=", 16)) shared = L[i] + 16;
	if (!shared || !USER_ID[0]) return;
	char p1[120] = "web-a", p2[120] = "web-u"; int k = 5;
	for (const char *c = USER_ID; *c && k < 5 + 32; c++) if (g_ascii_isalnum(*c)) p1[k++] = *c;
	p1[k] = 0;
	{ guint32 h = 0x811c9dc5; for (const char *c = USER_N; *c; c++) { h ^= (guchar)g_ascii_tolower(*c); h *= 0x01000193; }
	  char b36[16]; int j = 0; do { int d = h % 36; b36[j++] = d < 10 ? '0' + d : 'a' + d - 10; h /= 36; } while (h && j < 15);
	  for (int i = 0; i < j; i++) p2[5 + i] = b36[j - 1 - i]; p2[5 + j] = 0; }
	const char *names[2] = { p1, p2 };
	char *ext = g_build_filename(shared, "Default", "Storage", "ext", NULL); GDir *d = g_dir_open(ext, 0, NULL); const char *dom;
	while (d && (dom = g_dir_read_name(d))) for (int i = 0; i < 2; i++) {
		char hex[13], low[13]; oracle_part_dir(names[i], hex); for (int j = 0; j < 13; j++) low[j] = g_ascii_tolower(hex[j]);
		for (int c = 0; c < 2; c++) { char *pd = g_build_filename(ext, dom, c ? low : hex, NULL);
			if (g_file_test(pd, G_FILE_TEST_IS_DIR)) { char *argv[] = { "rm", "-rf", "--", pd, NULL }; g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, NULL);
				fprintf(stderr, "rudolf-native: Oracle: %s's websites from before (not encrypted) removed from the shared profile\n", USER_ID); }
			g_free(pd); } }
	if (d) g_dir_close(d); g_free(ext);
}
static char *oracle_web_profile(gchar **L, int n)
{
	if (n < 2 || access(WEB_DIR, W_OK | X_OK)) { if (WEB_DIR[0]) fprintf(stderr, "rudolf-native: Oracle: %s is not open: Standard's profile\n", WEB_DIR); return NULL; }
	char *prof = g_build_filename(WEB_DIR, "chromium", NULL); int fresh = access(prof, F_OK) != 0; g_mkdir_with_parents(prof, 0700);
	if (fresh) oracle_old_parts_rm(L, n);
	const char *bundle = getenv("RUDOLF_SHELL_BUNDLE"); if (!bundle) bundle = "/usr/share/rudolf/www/iwa/shell.swbn";
	const char *fix = getenv("RUDOLF_SHELLFIX"); if (!fix) fix = "/usr/bin/rudolf-shellfix";
	int have = 0, same = 0; char *iwa = g_build_filename(prof, "Default", "iwa", NULL); GDir *dd = g_dir_open(iwa, 0, NULL); gchar *sys = NULL; gsize sl = 0;
	if (dd && g_file_get_contents(bundle, &sys, &sl, NULL)) { const char *e; while ((e = g_dir_read_name(dd))) { char *f = g_build_filename(iwa, e, "main.swbn", NULL); gchar *b = NULL; gsize bl = 0;
		if (g_file_get_contents(f, &b, &bl, NULL)) { have = 1; if (bl == sl && !memcmp(b, sys, sl)) same = 1; } g_free(b); g_free(f); } }
	if (dd) g_dir_close(dd); g_free(sys); g_free(iwa);
	if (have && !same && !access(fix, X_OK)) {
		char *argv[] = { (char *)fix, L[1], prof, (char *)bundle, L[0], NULL }; gint st = 0; GError *e = NULL;
		fprintf(stderr, "rudolf-native: Oracle: the shell in %s is not the system's: replacing it\n", prof);
		if (!g_spawn_sync(NULL, argv, NULL, G_SPAWN_DEFAULT, NULL, NULL, NULL, NULL, &st, &e) || st) { fprintf(stderr, "rudolf-native: Oracle: the shell was not replaced (%s)\n", e ? e->message : "rudolf-shellfix"); }
		if (e) g_error_free(e);
	}
	char *r = g_strdup_printf("--user-data-dir=%s", prof); g_free(prof); return r;
}
static int oracle_open(const char *file, const char *extra)
{
	const char *cf = getenv("RUDOLF_ORACLE_CMD"); if (!cf) cf = "/run/rudolf/oracle-chrome";
	const char *ln = getenv("RUDOLF_LAUNCH"); if (!ln) ln = "/usr/bin/rudolf-launch";
	if (!OR_RUN) {
		gchar *t = NULL; if (!g_file_get_contents(cf, &t, NULL, NULL) || access(ln, X_OK)) { g_free(t); fprintf(stderr, "rudolf-native: Oracle: no Chromium with the shell (%s): its WebKit window\n", cf); return 0; }
		gchar **L = g_strsplit(t, "\n", 0); g_free(t); int n = 0; while (L[n]) n++;
		if (n < 2 || strlen(L[0]) != 56 || access(L[1], X_OK)) { g_strfreev(L); fprintf(stderr, "rudolf-native: Oracle: %s names no Chromium or shell\n", cf); return 0; }
		snprintf(OR_CTL, sizeof OR_CTL, "%s/rudolf-oracle.ctl", g_get_user_runtime_dir()); unlink(OR_CTL);
		if (mkfifo(OR_CTL, 0600)) { g_strfreev(L); fprintf(stderr, "rudolf-native: Oracle: %s: %s\n", OR_CTL, strerror(errno)); return 0; }
		GPtrArray *a = g_ptr_array_new(); g_ptr_array_add(a, (gpointer)ln); g_ptr_array_add(a, "--oracle"); g_ptr_array_add(a, OR_CTL); g_ptr_array_add(a, L[0]);
		/* (1.6.0 Part 22; the owner: "option 1 for native") an account whose files are on the disk: Oracle's profile in its encrypted
		   folder (rudolfd's "web", /persist/oracle/ID: locked with the home folder at sign-out), in place of Standard's shared one */
		char *wprof = WEB_DIR[0] && USER_ID[0] && strcmp(USER_R, "guest") ? oracle_web_profile(L, n) : NULL;
		for (int i = 1; i < n; i++) if (L[i][0] && !(wprof && !strncmp(L[i], "--user-data-dir=", 16))) g_ptr_array_add(a, L[i]);
		if (wprof) g_ptr_array_add(a, wprof);
		g_ptr_array_add(a, NULL); GError *er = NULL;
		int out = -1, ok = g_spawn_async_with_pipes(NULL, (gchar **)a->pdata, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &OR_PID, NULL, &out, NULL, &er);
		if (ok && out >= 0) { GIOChannel *ch = g_io_channel_unix_new(out); g_io_channel_set_close_on_unref(ch, TRUE); g_io_add_watch(ch, G_IO_IN | G_IO_HUP | G_IO_ERR, oracle_msgs, NULL); g_io_channel_unref(ch); }
		g_ptr_array_free(a, TRUE); g_strfreev(L); g_free(wprof);
		if (!ok) { fprintf(stderr, "rudolf-native: Oracle: %s\n", er ? er->message : "?"); if (er) g_error_free(er); unlink(OR_CTL); return 0; }
		OR_RUN = 1; g_child_watch_add(OR_PID, oracle_exited, NULL);
	}
	/* the window: Oracle's place and size as an app window's first (the page's own size comes with its title) */
	int n = g_list_length(CLIENTS) % 8, w = SW * 7 / 10, h = work_h() * 8 / 10; if (w < 640) w = SW; if (h < 400) h = work_h();
	GString *hs = g_string_new("app=browser"); if (file && *file) { g_string_append(hs, "&file="); oracle_hash(hs, file); }
	if (extra) oracle_hash(hs, extra);
	char *line = g_strdup_printf("open %d %d %d %d %s\n", w, h, 40 + n * 28, work_top() + 20 + n * 28, hs->str); g_string_free(hs, TRUE);
	int ok = 0; for (int i = 0; i < 50 && !ok; i++) { int f = open(OR_CTL, O_WRONLY | O_NONBLOCK | O_CLOEXEC); if (f >= 0) { ok = write(f, line, strlen(line)) == (ssize_t)strlen(line); close(f); } if (!ok) g_usleep(100000); }
	g_free(line);
	if (!ok) { fprintf(stderr, "rudolf-native: Oracle: Chromium did not take the window\n"); return 0; }
	/* (a cold Chromium takes 1 to 3 s: the pointer says so until the window is there) */
	oracle_busy(1); OR_BUSY = g_timeout_add_seconds(20, oracle_busy_end, NULL);
	return 1;
}
/* ------------------------------------------------------------------ Oracle's Tor windows (1.5.9, docs/plan-1.5.9.md Part 16)
   A Tor window is Rudolf's Chromium of its own going through Tor: started here as Oracle's (rudolf-launch --oracle) with what
   the session writes in /run/rudolf/tor-chrome (the shell's ID, then the program and its arguments: Tor's SOCKS5 proxy, so
   no name is looked up outside Tor, WebRTC only through it), a profile of its own in the user's runtime folder (memory),
   made fresh each time and removed when Chromium goes; each window the shell at #app=browser&tor=1&url=...&url=..., which
   the page opens as Oracle alone, private. Framed as Oracle (RudolfTor, as RudolfOracle); what its pages tell the native
   desktop is read as Oracle's (oracle_msgs). Oracle's page asks with op "tor" (its tabs, one a line), and a Tor window gives
   its tabs back to a normal Oracle window with op "untor". RUDOLF_TOR_CMD changes the file (tests). */
static GPid TOR_PID; static int TOR_RUN; static char TOR_CTL[300], TOR_PROF[300];
static void tor_rmprof(void) { if (!TOR_PROF[0]) return; gchar *a[] = { "rm", "-rf", TOR_PROF, NULL }; g_spawn_sync(NULL, a, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, NULL, NULL, NULL); }
static void tor_exited(GPid p, gint st, gpointer d) { (void)d; (void)st; g_spawn_close_pid(p); if (p == TOR_PID) { TOR_RUN = 0; TOR_PID = 0; tor_rmprof(); unlink(TOR_CTL); state_write(); } }
/* the address's own tabs: each http(s), about: or file:/// address, escaped, at most 12 */
static GString *tor_hash(const char *base, const char *urls)
{
	GString *h = g_string_new(base); int n = 0; gchar **L = g_strsplit(urls ? urls : "", "\n", 13);
	for (int i = 0; L[i] && n < 12; i++) { const char *u = L[i]; if (strlen(u) > 2000 || !(!strncmp(u, "http://", 7) || !strncmp(u, "https://", 8) || !strncmp(u, "about:", 6) || !strncmp(u, "file:///", 8))) continue;
		char *e = g_uri_escape_string(u, NULL, FALSE); g_string_append_printf(h, "&url=%s", e); g_free(e); n++; }
	g_strfreev(L); return h;
}
static int tor_open(const char *urls)
{
	if (!USER_ID[0] || LOCKED) return 0;
	const char *cf = getenv("RUDOLF_TOR_CMD"); if (!cf) cf = "/run/rudolf/tor-chrome";
	const char *ln = getenv("RUDOLF_LAUNCH"); if (!ln) ln = "/usr/bin/rudolf-launch";
	if (!TOR_RUN) {
		gchar *t = NULL; if (!g_file_get_contents(cf, &t, NULL, NULL) || access(ln, X_OK)) { g_free(t); fprintf(stderr, "rudolf-native: Tor window: no Chromium with the shell (%s)\n", cf);
			toast_text("The Tor window could not open", "Browse with Tor needs Rudolf's Chromium with its shell, which this computer does not have.", 8000); return 0; }
		gchar **L = g_strsplit(t, "\n", 0); g_free(t); int n = 0; while (L[n]) n++;
		if (n < 2 || strlen(L[0]) != 56 || access(L[1], X_OK)) { g_strfreev(L); fprintf(stderr, "rudolf-native: Tor window: %s names no Chromium or shell\n", cf); return 0; }
		snprintf(TOR_CTL, sizeof TOR_CTL, "%s/rudolf-tor.ctl", g_get_user_runtime_dir()); unlink(TOR_CTL);
		snprintf(TOR_PROF, sizeof TOR_PROF, "%s/rudolf-tor-profile", g_get_user_runtime_dir()); tor_rmprof();
		if (mkfifo(TOR_CTL, 0600) || mkdir(TOR_PROF, 0700)) { g_strfreev(L); fprintf(stderr, "rudolf-native: Tor window: %s\n", strerror(errno)); return 0; }
		/* (WebRTC only through the proxy in its tabs too: the command line's --force-webrtc-ip-handling-policy reaches Chromium's own
		   pages, not the shell's Controlled Frames, which read the profile's own setting) */
		{ char *dd = g_build_filename(TOR_PROF, "Default", NULL), *pf = g_build_filename(dd, "Preferences", NULL); mkdir(dd, 0700);
		  g_file_set_contents(pf, "{\"webrtc\":{\"ip_handling_policy\":\"disable_non_proxied_udp\",\"multiple_routes_enabled\":false,\"nonproxied_udp_enabled\":false}}", -1, NULL); g_free(dd); g_free(pf); }
		char *pa = g_strdup_printf("--user-data-dir=%s", TOR_PROF);
		/* (1.6.0, plan Part 20: a HOME of its own in the throwaway profile, so Chromium's certificate database (~/.pki/nssdb) is
		   new and empty there: no smart card module, so a website reached through Tor is never offered the card) */
		char *th = g_build_filename(TOR_PROF, "home", NULL); mkdir(th, 0700); char **tenv = g_environ_setenv(g_get_environ(), "HOME", th, TRUE);
		GPtrArray *a = g_ptr_array_new(); g_ptr_array_add(a, (gpointer)ln); g_ptr_array_add(a, "--oracle"); g_ptr_array_add(a, TOR_CTL); g_ptr_array_add(a, L[0]);
		for (int i = 1; i < n; i++) if (L[i][0]) g_ptr_array_add(a, L[i]);
		g_ptr_array_add(a, pa); g_ptr_array_add(a, NULL); GError *er = NULL;
		int out = -1, ok = g_spawn_async_with_pipes(NULL, (gchar **)a->pdata, tenv, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &TOR_PID, NULL, &out, NULL, &er);
		if (ok && out >= 0) { GIOChannel *ch = g_io_channel_unix_new(out); g_io_channel_set_close_on_unref(ch, TRUE); g_io_add_watch(ch, G_IO_IN | G_IO_HUP | G_IO_ERR, oracle_msgs, NULL); g_io_channel_unref(ch); }
		g_ptr_array_free(a, TRUE); g_strfreev(L); g_free(pa); g_strfreev(tenv); g_free(th);
		if (!ok) { fprintf(stderr, "rudolf-native: Tor window: %s\n", er ? er->message : "?"); if (er) g_error_free(er); unlink(TOR_CTL); tor_rmprof(); return 0; }
		TOR_RUN = 1; TOR_CTL_P = TOR_CTL; g_child_watch_add(TOR_PID, tor_exited, NULL);
	}
	int n = g_list_length(CLIENTS) % 8, w = SW * 7 / 10, h = work_h() * 8 / 10; if (w < 640) w = SW; if (h < 400) h = work_h();
	GString *hs = tor_hash("app=browser&tor=1", urls);
	char *line = g_strdup_printf("open %d %d %d %d %s\n", w, h, 40 + n * 28, work_top() + 20 + n * 28, hs->str); g_string_free(hs, TRUE);
	int ok = 0; for (int i = 0; i < 50 && !ok; i++) { int f = open(TOR_CTL, O_WRONLY | O_NONBLOCK | O_CLOEXEC); if (f >= 0) { ok = write(f, line, strlen(line)) == (ssize_t)strlen(line); close(f); } if (!ok) g_usleep(100000); }
	g_free(line);
	if (!ok) { fprintf(stderr, "rudolf-native: Tor window: Chromium did not take the window\n"); return 0; }
	oracle_busy(1); OR_BUSY = g_timeout_add_seconds(20, oracle_busy_end, NULL);
	return 1;
}
/* a Tor window's tabs, back in a normal Oracle window */
static void tor_back(const char *urls) { GString *hs = tor_hash("&untor=1", urls); app_open_ex("browser", NULL, hs->str, 0); g_string_free(hs, TRUE); }
/* (12.3c step 5) Oracle's keys, as on Standard. Chromium acts on Ctrl+W, Ctrl+F4, Ctrl+T, Ctrl+N, Ctrl+Q and Alt+F4 before a page
   sees them (closing Oracle's window, or quitting Chromium with every Oracle window on Ctrl+Q); on Standard rudolf-display hands
   them to the desktop's Chromium window (class RudolfDesktop) as Ctrl+F20..F23 and Alt+F20, which the page and Oracle's real
   tabs read as the keys again (rudolf-display.c desk_key; shell26.js DESK_SUB; the shell's PAGE_JS). Oracle's Chromium here is
   RudolfOracle, which rudolf-display leaves alone, so the window manager does the same for Oracle's windows: those keys are
   grabbed on them and given back through XTest as the same F20..F23 key codes (evdev's, + 8), Ctrl, Shift and Alt still held. */
typedef int (*XtFake)(Display *, unsigned, Bool, unsigned long);
static XtFake xt_fake(void)
{
	static XtFake f; static int tried; if (tried) return f; tried = 1;
	void *h = dlopen("libXtst.so.6", RTLD_NOW | RTLD_LOCAL); if (h) f = (XtFake)dlsym(h, "XTestFakeKeyEvent");
	if (!f) fprintf(stderr, "rudolf-native: no XTest (libXtst.so.6): Oracle's Ctrl+W, T, N, Q and Alt+F4 stay Chromium's\n");
	return f;
}
#define OKEY_F20 198   /* (KEY_F20 + 8; F21..F23 follow) */
static int oracle_key_sub(unsigned kc, unsigned st)
{
	int ctrl = (st & ControlMask) != 0, alt = (st & Mod1Mask) != 0;
	if (ctrl && !alt) { if (kc == XKeysymToKeycode(DPY, XK_w) || kc == XKeysymToKeycode(DPY, XK_F4)) return OKEY_F20; if (kc == XKeysymToKeycode(DPY, XK_t)) return OKEY_F20 + 1;
		if (kc == XKeysymToKeycode(DPY, XK_n)) return OKEY_F20 + 2; if (kc == XKeysymToKeycode(DPY, XK_q)) return OKEY_F20 + 3; }
	if (alt && !ctrl && kc == XKeysymToKeycode(DPY, XK_F4)) return OKEY_F20;
	return 0;
}
static void oracle_keys(Window w)
{
	if (!xt_fake()) return;
	static const unsigned M[4] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
	const KeySym ck[5] = { XK_w, XK_F4, XK_t, XK_n, XK_q };
	for (int i = 0; i < 4; i++) for (int sh = 0; sh < 2; sh++) {
		unsigned s = M[i] | (sh ? ShiftMask : 0);
		for (int k = 0; k < 5; k++) XGrabKey(DPY, XKeysymToKeycode(DPY, ck[k]), ControlMask | s, w, False, GrabModeAsync, GrabModeAsync);
		XGrabKey(DPY, XKeysymToKeycode(DPY, XK_F4), Mod1Mask | s, w, False, GrabModeAsync, GrabModeAsync);
	}
}
static int oracle_key(XKeyEvent *e)
{
	int sub = oracle_key_sub(e->keycode, e->state); XtFake f = xt_fake(); if (!sub || !f) return 0;
	/* (the grab ends here, so the key codes reach Oracle's window, which has the keyboard; the key's own release goes there too) */
	XUngrabKeyboard(DPY, e->time);
	/* (1.6.0, the owner's Vivobook, 2026-11-17: Ctrl+T, W and N did nothing in Oracle's website tabs on Native, while passing here) the
	   key comes when this program reads it: on the laptop the desktop is busy now and then (a catalogue read, a page's work), and Ctrl
	   was already let go by then, so Oracle got a bare F21, which nothing reads. The modifiers the key was pressed with and that are
	   up now are held again around it, as rudolf-display's substitution has them on Standard */
	static const KeySym MK[3][2] = { { XK_Control_L, XK_Control_R }, { XK_Shift_L, XK_Shift_R }, { XK_Alt_L, XK_Alt_R } };
	static const unsigned MM[3] = { ControlMask, ShiftMask, Mod1Mask };
	char km[32] = {0}; XQueryKeymap(DPY, km); KeyCode again[3] = {0}; int n = 0;
	for (int i = 0; i < 3; i++) { if (!(e->state & MM[i])) continue; int down = 0;
		for (int j = 0; j < 2; j++) { KeyCode kc = XKeysymToKeycode(DPY, MK[i][j]); if (kc && (km[kc / 8] & (1 << (kc % 8)))) down = 1; }
		if (!down && (again[n] = XKeysymToKeycode(DPY, MK[i][0]))) f(DPY, again[n++], True, CurrentTime); }
	f(DPY, (unsigned)sub, True, CurrentTime); f(DPY, (unsigned)sub, False, CurrentTime);
	for (int i = n - 1; i >= 0; i--) f(DPY, again[i], False, CurrentTime);
	XFlush(DPY);
	if (n) fprintf(stderr, "rudolf-native: Oracle's key handed over after its modifiers were let go (the desktop was busy): held again around it\n");
	return 1;
}
static void oracle_client(Client *c, XWindowAttributes *wa)
{
	XClassHint ch = {0}; if (!XGetClassHint(DPY, c->w, &ch)) return;
	int is = ch.res_class && (!strcmp(ch.res_class, "RudolfOracle") || !strcmp(ch.res_class, "RudolfTor"));   /* (and Oracle's Tor windows: Part 16) */ if (ch.res_name) XFree(ch.res_name); if (ch.res_class) XFree(ch.res_class);
	if (!is || c->trans) return;
	snprintf(c->ak, sizeof c->ak, "browser");
	const char *in = getenv("RUDOLF_ORACLE_INSET"); int t = 28, l = 4, r = 4, b = 4; if (in) sscanf(in, "%d %d %d %d", &t, &l, &r, &b);
	c->it = t; c->il = l; c->ir = r; c->ib = b; c->cw = wa->width - l - r; c->ch = wa->height - t - b; if (c->cw < 100) c->cw = 100; if (c->ch < 100) c->ch = 100;
	oracle_keys(c->w);
	oracle_busy(0);
}
static void oracle_gone(void) { }
/* (1.6.0) an Oracle window's title as its page has it, the window found by its place (Chromium's own bounds); kept for a window not
   managed yet (its first title comes as it opens), given to it as it is (oracle_client) */
static struct { int x, y, w, h; char t[256]; gint64 at; } OR_TQ[8];
static int oracle_at(Client *c, int x, int y, int w, int h)
{
	int ax, ay; Window ch; XWindowAttributes wa;
	if (!c->ak[0] || c->trans || !XTranslateCoordinates(DPY, c->w, ROOT, 0, 0, &ax, &ay, &ch) || !XGetWindowAttributes(DPY, c->w, &wa)) return 0;
	return abs(ax - x) <= 2 && abs(ay - y) <= 2 && abs(wa.width - w) <= 2 && abs(wa.height - h) <= 2;
}
static void oracle_title_set(Client *c, const char *t)
{
	if (!strcmp(c->ptitle, t)) return;
	snprintf(c->ptitle, sizeof c->ptitle, "%s", t); for (char *p = c->ptitle; *p; p++) if (*p == '\n' || *p == '\t') *p = ' ';
	wm_title(c); frame_draw(c); bar_redraw(); state_write();
}
static void oracle_title(int x, int y, int w, int h, const char *t)
{
	for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (oracle_at(c, x, y, w, h)) { oracle_title_set(c, t); return; } }
	fprintf(stderr, "rudolf-native: Oracle's title \"%.60s\" for a window at %d,%d %dx%d not here yet\n", t, x, y, w, h);
	int k = 0; for (int i = 1; i < 8; i++) if (OR_TQ[i].at < OR_TQ[k].at) k = i;
	OR_TQ[k].x = x; OR_TQ[k].y = y; OR_TQ[k].w = w; OR_TQ[k].h = h; OR_TQ[k].at = g_get_monotonic_time(); snprintf(OR_TQ[k].t, sizeof OR_TQ[k].t, "%s", t);
}
static void oracle_title_take(Client *c)
{
	gint64 now = g_get_monotonic_time(); int best = -1;
	for (int i = 0; i < 8; i++) if (OR_TQ[i].t[0] && now - OR_TQ[i].at < 20 * G_USEC_PER_SEC && oracle_at(c, OR_TQ[i].x, OR_TQ[i].y, OR_TQ[i].w, OR_TQ[i].h) && (best < 0 || OR_TQ[i].at > OR_TQ[best].at)) best = i;
	if (best >= 0) { oracle_title_set(c, OR_TQ[best].t); OR_TQ[best].t[0] = 0; }
}
/* (12.3c) Oracle's window found by its place (Chromium's own bounds): put in no workspace (ws 0), so neither shown nor listed */
static void oracle_hide(int x, int y, int w, int h)
{
	for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; int ax, ay; Window ch; XWindowAttributes wa;
		if (!c->ak[0] || c->trans || !XTranslateCoordinates(DPY, c->w, ROOT, 0, 0, &ax, &ay, &ch) || !XGetWindowAttributes(DPY, c->w, &wa)) continue;
		if (abs(ax - x) > 2 || abs(ay - y) > 2 || abs(wa.width - w) > 2 || abs(wa.height - h) > 2) continue;
		c->ws = 0; ws_apply(); if (FOCUS == c || !FOCUS) wm_focus(top_client()); bar_redraw(); state_write(); return; }
}
static void appwin_wait_arm(AppWin *a);
static void wm_manage(Window w, AppWin *own)
{
	if (client_of(w)) return;
	XWindowAttributes wa; if (!XGetWindowAttributes(DPY, w, &wa) || wa.override_redirect) return;
	Client *c = g_new0(Client, 1); c->w = w; c->app = own; c->cw = wa.width; c->ch = wa.height; c->ws = WSC;
	if (!XGetTransientForHint(DPY, w, &c->trans)) c->trans = None;
	if (!own) oracle_client(c, &wa);   /* (1.5.9, 12.3c: Oracle's Chromium is Oracle's window) */
	if (!own && !c->ak[0]) csd_read(c, &wa);   /* (1.6.1 Part 15: an app drawing its own title bar) */
	wm_title(c);
	int ay = work_top(), ah = work_h(), TH = th_of(c);
	int big = c->csd && !c->trans && wa.width >= SW && wa.height >= SH;   /* (a borderless window the screen's size: a game's full screen, as rudolf-display takes it) */
	if (c->cw > SW - 2 * BW) c->cw = SW - 2 * BW; if (c->ch > ah - TH - BW) c->ch = ah - TH - BW;
	Client *owner = c->trans ? client_of(c->trans) : NULL;
	if (owner) { c->x = owner->x + (fw_of(owner) - fw_of(c)) / 2; c->y = owner->y + (fh_of(owner) - fh_of(c)) / 2; }
	else if (own || (wa.x <= 0 && wa.y <= 0)) { int n = g_list_length(CLIENTS) % 8; c->x = 40 + n * 28; c->y = ay + 16 + n * 28; }
	else { c->x = wa.x + c->il - BW; c->y = wa.y + c->it - TH; }
	if (c->x + fw_of(c) > SW) c->x = SW - fw_of(c); if (c->x < 0) c->x = 0;
	if (c->y + fh_of(c) > ay + ah) c->y = ay + ah - fh_of(c); if (c->y < ay) c->y = ay;
	XSetWindowAttributes at = {0}; at.event_mask = SubstructureRedirectMask | SubstructureNotifyMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | ExposureMask | LeaveWindowMask;
	at.background_pixel = BlackPixel(DPY, DefaultScreen(DPY));
	c->frame = XCreateWindow(DPY, ROOT, mon_ox(c) + c->x, mon_oy(c) + c->y, fw_of(c), fh_of(c), 0, CopyFromParent, InputOutput, CopyFromParent, CWEventMask | CWBackPixel, &at);
	/* (our own windows share GDK's connection to X: selecting events replaces GDK's own mask, so GDK's stays (its sizes, its
	   exposures; else a page keeps its first size, larger than its window, and is not drawn again where a window uncovers it)) */
	XSelectInput(DPY, w, PropertyChangeMask | (own ? wa.your_event_mask : StructureNotifyMask));
	XAddToSaveSet(DPY, w); XSetWindowBorderWidth(DPY, w, 0);
	if (wa.map_state != IsUnmapped) c->ignore_unmap++;   /* (reparenting a shown window unmaps it once) */
	if ((c->ak[0] && c->it) || c->nodeco) {   /* (an app drawing its own title bar too: GTK's margins, and Part 15's cut, as Oracle's) */
		XSetWindowAttributes ca = {0}; ca.event_mask = SubstructureNotifyMask | SubstructureRedirectMask; ca.background_pixel = WhitePixel(DPY, DefaultScreen(DPY));
		c->clip = XCreateWindow(DPY, c->frame, BW, TH, c->cw, c->ch, 0, CopyFromParent, InputOutput, CopyFromParent, CWEventMask | CWBackPixel, &ca); XMapWindow(DPY, c->clip);
		XReparentWindow(DPY, w, c->clip, -c->il, -c->it); XResizeWindow(DPY, w, c->cw + c->il + c->ir, c->ch + c->it + c->ib); }
	else { XReparentWindow(DPY, w, c->frame, BW, TH); XResizeWindow(DPY, w, c->cw, c->ch); }
	/* (the owner's Vivobook report 1, 2026-11-16) an app's own window is shown once its page has drawn the app at the app's size
	   (appwin_size, then the page's "ready": appwin_reveal), not while the page is still starting at the shell's first size (the
	   whole page, its sign-in screen, at 70 % of the screen); a slow page's window comes after WAIT_MS anyway, its page covered by
	   a plain background of the app's colour until it is ready (appwin_wait_late) */
	if (own && !own->sized && !own->modal && !own->hidden && strcmp(own->k, "do-saver")) { c->wait = 1; c->hidden = 1; }
	CLIENTS = g_list_append(CLIENTS, c);
	pn_client(c);   /* (1.6.1 Part 2: the mobile build's panels) */
	XMapWindow(DPY, w); frame_shape(c); rz_place(c); if (!c->hidden) XMapWindow(DPY, c->frame); set_wm_state(c, NormalState); desk_shadows();
	grab_clicks(c, 1); tell_geom(c);
	/* asked for full screen before it was shown */
	Atom type; int fmt; unsigned long n = 0, after; unsigned char *d = NULL;
	if (XGetWindowProperty(DPY, w, A_NSTATE, 0, 32, False, XA_ATOM, &type, &fmt, &n, &after, &d) == Success && d) { for (unsigned long i = 0; i < n; i++) if (((Atom *)d)[i] == A_NFULL) { c->full = 0; wm_fullscreen(c, 1); } XFree(d); }
	if (own && !strcmp(own->k, "do-saver")) { c->ontop = 1; wm_fullscreen(c, 1); }   /* (the screen saver: over everything, the taskbar too: item 6) */
	if (own && TM_TOP && !strcmp(own->k, "taskmgr")) { c->ontop = 1; TM_TOP = 0; }   /* (1.6.0: Ctrl+Alt+Del's Task Manager, above every window) */
	if (c->ak[0]) { XSync(DPY, False); oracle_title_take(c); }   /* (1.6.0: Oracle's page's title, when it came first) */
	if (big && !c->full) wm_fullscreen(c, 1);
	if (!c->full) csd_max_first(c);
	csd_extents(c);
	if (c->wait) { appwin_wait_arm(own); client_list(); bar_redraw(); state_write(); return; }
	wm_focus(c);
	if (!own) wopen_start(c);   /* (another program's window (Oracle's, a Linux app's) opens as the page's do; an app's own, at its page's size: appwin_size) */
}
static void wm_unmanage(Client *c, int gone)
{
	if (DRAG.c == c) { XUngrabPointer(DPY, CurrentTime); if (DRAG.kbd) XUngrabKeyboard(DPY, CurrentTime); DRAG.c = NULL; DRAG.kbd = 0; }   /* (1.6.1 Part 15: a window going while moved) */
	lp_gone(c);   /* (Part 17: its frame folding away where it was) */
	desk_shadows();
	CLIENTS = g_list_remove(CLIENTS, c);
	if (!gone) { XReparentWindow(DPY, c->w, ROOT, mon_ox(c) + c->x + BW - c->il, mon_oy(c) + c->y + th_of(c) - c->it); XRemoveFromSaveSet(DPY, c->w); set_wm_state(c, WithdrawnState); grab_clicks(c, 0); }
	if (c->clip) XDestroyWindow(DPY, c->clip);
	XDestroyWindow(DPY, c->frame);
	if (c->ak[0]) oracle_gone();
	if (FOCUS == c) { FOCUS = NULL; wm_focus(top_client()); }
	g_free(c); client_list(); bar_redraw(); state_write();
}
/* the lock screen: every window out of sight, then back */
static void wm_hide_all(int hide)
{
	desk_shadows();
	for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (c->min) continue;
		if (hide && !c->hidden) { c->hidden = 1; if (!c->off) XUnmapWindow(DPY, c->frame); } else if (!hide && c->hidden && c->wait != 1) { c->hidden = 0; if (!c->off) XMapWindow(DPY, c->frame); } }
	if (!hide) wm_focus(FOCUS ? FOCUS : top_client());
}
/* the title bars' height or the work area changed (the display scaling, the development link's banner) */
static void wm_relayout(void)
{
	for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data;
		if (c->max) max_geom(c);
		else if (!c->full && c->y < work_top()) c->y = work_top();
		place(c); frame_draw(c); }
	state_write();
}
/* moving and resizing with the pointer */
static int edge_at(Client *c, int x, int y)
{
	int fw = fw_of(c), fh = fh_of(c), e = 0, g = RZG(), k = (int)lround(14 * Z);
	if (x < g) e |= 1; if (x >= fw - g) e |= 2; if (y < (int)lround(3 * Z)) e |= 4; if (y >= fh - g) e |= 8;
	if (e & 3) { if (y < k) e |= 4; if (y >= fh - k) e |= 8; } if (e & 12) { if (x < k) e |= 1; if (x >= fw - k) e |= 2; }   /* (the corners' handles) */
	return e;
}
static int btn_at(Client *c, int x, int y) { if (c->csd) return 0; int r[3][4]; btn_rects(c, r); for (int i = 0; i < 3; i++) if (x >= r[i][0] && x < r[i][0] + r[i][2] && y >= r[i][1] && y < r[i][1] + r[i][3]) return i + 1; return 0; }
static Cursor cursor_for(int e) { int i = e == 1 ? 1 : e == 2 ? 2 : e == 4 ? 3 : e == 8 ? 4 : e == 5 ? 5 : e == 6 ? 6 : e == 9 ? 7 : e == 10 ? 8 : 0; return CUR[i]; }
static void win_menu_at(Client *c, int x, int y);
/* (base_v24.html, a middle-click on a title bar) the window behind every other, the one now in front in use */
static void wm_lower(Client *c)
{
	XLowerWindow(DPY, c->frame); wm_restack();
	Window r, p, *k = NULL; unsigned n = 0; Client *top = NULL;
	if (XQueryTree(DPY, ROOT, &r, &p, &k, &n)) for (unsigned i = 0; i < n; i++) { Client *o = client_of(k[i]); if (o && o != c && !o->min && !o->hidden && !o->off && !o->trans && o->frame == k[i]) top = o; }
	if (k) XFree(k);
	if (top) wm_focus(top); else state_write();
}
/* ---- (1.6.1, docs/plan-1.6.1.md Part 15, item 2) one title bar for apps that draw their own. Electron and Chromium apps
   (Discord, Steam's new interface, VS Code), GTK 3 and 4 apps with a header bar ask the window manager for no decorations
   (_MOTIF_WM_HINTS, decorations 0) and draw their own bar: such a window gets no title bar of Rudolf's. Its frame keeps the
   edge (1 px, 2 px with high contrast), the round corners and the resize edges (a fourth handle along the top); the taskbar,
   Alt+Tab, the overview, the workspaces and the transitions as for every window. For their own bar to work, what they ask:
   _NET_WM_MOVERESIZE (moving by their bar, resizing by their edges, the keyboard's move and size), done as a drag of Rudolf's
   own title bar is (drag_motion: the same limits); _NET_WM_STATE_MAXIMIZED_VERT and _HORZ (their maximize button, a
   double-click on their bar): Rudolf's maximize and restore, with its transition; _NET_FRAME_EXTENTS on their windows;
   _GTK_FRAME_EXTENTS (GTK's invisible margins, its shadows, cut away by the clip window as Oracle's own bar is: 1.5.9 12.3c);
   _GTK_SHOW_WINDOW_MENU (a right-click on a GTK header bar: the window's menu, as the taskbar button's). Rudolf's own app
   windows and Oracle's are framed as before. */
static Atom A_MOTIF, A_GTKFE, A_NFE, A_NREQFE, A_NMOVERESIZE, A_NMAXV, A_NMAXH, A_GTKMENU;
static void csd_atoms(void)
{
	if (A_MOTIF) return;
	A_MOTIF = XInternAtom(DPY, "_MOTIF_WM_HINTS", False); A_GTKFE = XInternAtom(DPY, "_GTK_FRAME_EXTENTS", False); A_NFE = XInternAtom(DPY, "_NET_FRAME_EXTENTS", False);
	A_NREQFE = XInternAtom(DPY, "_NET_REQUEST_FRAME_EXTENTS", False); A_NMOVERESIZE = XInternAtom(DPY, "_NET_WM_MOVERESIZE", False);
	A_NMAXV = XInternAtom(DPY, "_NET_WM_STATE_MAXIMIZED_VERT", False); A_NMAXH = XInternAtom(DPY, "_NET_WM_STATE_MAXIMIZED_HORZ", False); A_GTKMENU = XInternAtom(DPY, "_GTK_SHOW_WINDOW_MENU", False);
}
/* no decorations asked for (MWM_HINTS_DECORATIONS set, decorations 0), as rudolf-display's borderless() reads it */
static int csd_nodeco(Window w)
{
	csd_atoms(); Atom type; int fmt; unsigned long n = 0, after; unsigned char *d = NULL; int r = 0;
	if (XGetWindowProperty(DPY, w, A_MOTIF, 0, 5, False, AnyPropertyType, &type, &fmt, &n, &after, &d) == Success && d && fmt == 32 && n >= 3) r = (((long *)d)[0] & 2) && ((long *)d)[2] == 0;
	if (d) XFree(d);
	return r;
}
/* GTK's margins (left, right, top, bottom), each 0 to 200 */
static void csd_gtk(Window w, int *l, int *r, int *t, int *b)
{
	csd_atoms(); Atom type; int fmt; unsigned long n = 0, after; unsigned char *d = NULL; *l = *r = *t = *b = 0;
	if (XGetWindowProperty(DPY, w, A_GTKFE, 0, 4, False, XA_CARDINAL, &type, &fmt, &n, &after, &d) == Success && d && fmt == 32 && n == 4) {
		long *v = (long *)d; int *o[4] = {l, r, t, b}; for (int i = 0; i < 4; i++) *o[i] = v[i] < 0 ? 0 : v[i] > 200 ? 200 : (int)v[i]; }
	if (d) XFree(d);
}
/* (wm_manage, before the frame is made) another program's window that draws its own title bar */
static void csd_read(Client *c, XWindowAttributes *wa)
{
	c->nodeco = c->csd = !c->app && !c->ak[0] && csd_nodeco(c->w); if (!c->csd) return;
	csd_gtk(c->w, &c->gl, &c->gr, &c->gt, &c->gb); c->il = c->gl; c->ir = c->gr; c->it = c->gt; c->ib = c->gb;
	tb_cut_apply(c);   /* (item 3: Rudolf's title bar chosen for it) */
	c->cw = MAX(1, wa->width - c->il - c->ir); c->ch = MAX(1, wa->height - c->it - c->ib);
}
/* (a window already framed whose hints change: Chromium's "Use system title bar and borders" turned on or off; GTK's margins
   as it is maximised) its frame redrawn for them; the clip window made if it has none yet */
static void csd_clip(Client *c)
{
	if (c->clip) return;
	XWindowAttributes wa; int mapped = XGetWindowAttributes(DPY, c->w, &wa) && wa.map_state != IsUnmapped;
	XSetWindowAttributes ca = {0}; ca.event_mask = SubstructureNotifyMask | SubstructureRedirectMask; ca.background_pixel = WhitePixel(DPY, DefaultScreen(DPY));
	c->clip = XCreateWindow(DPY, c->frame, BW, th_of(c), MAX(1, c->cw), MAX(1, c->ch), 0, CopyFromParent, InputOutput, CopyFromParent, CWEventMask | CWBackPixel, &ca); XMapWindow(DPY, c->clip);
	if (mapped) c->ignore_unmap++;   /* (reparenting a shown window unmaps it once) */
	XReparentWindow(DPY, c->w, c->clip, -c->il, -c->it);
}
static void csd_update(Client *c)
{
	if (c->app || c->ak[0] || !c->frame) return;
	int now = csd_nodeco(c->w), l = 0, r = 0, t = 0, b = 0; if (now) csd_gtk(c->w, &l, &r, &t, &b);
	if (now == c->nodeco && l == c->gl && r == c->gr && t == c->gt && b == c->gb) return;
	int ocut = c->tbcut;
	c->nodeco = c->csd = now; c->gl = l; c->gr = r; c->gt = t; c->gb = b; c->il = l; c->ir = r; c->it = t; c->ib = b;
	tb_cut_apply(c);   /* (item 3) */
	if (!c->max && !c->full) c->ch = MAX(1, c->ch + ocut - c->tbcut);   /* (item 3: the app's window keeps its height; what is cut is taken from what shows) */
	if (now || l || r || t || b) csd_clip(c);
	if (c->max) max_geom(c);
	place(c); frame_draw(c); state_write();
}
/* _NET_FRAME_EXTENTS (left, right, top, bottom) on another program's window: the frame's edges and its title bar, none full
   screen (not on Rudolf's own windows, nor Oracle's: they are cut as their pages have it) */
static void csd_extents(Client *c)
{
	if (!c || c->app || c->ak[0]) return; csd_atoms();
	long v[4] = { BW, BW, th_of(c), BW }; if (c->full) v[0] = v[1] = v[2] = v[3] = 0;
	XChangeProperty(DPY, c->w, A_NFE, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)v, 4);
}
/* _NET_WM_STATE as the window is: full screen, maximized (both ways), minimized (Rudolf's own windows: full screen alone, as before) */
static void net_state_sync(Client *c)
{
	if (!c) return; csd_atoms(); Atom a[4]; int n = 0;
	if (c->full) a[n++] = A_NFULL;
	if (!c->app) { if (c->max && !c->full) { a[n++] = A_NMAXV; a[n++] = A_NMAXH; } if (c->min) a[n++] = A_NHIDDEN; }
	XChangeProperty(DPY, c->w, A_NSTATE, XA_ATOM, 32, PropModeReplace, (unsigned char *)a, n);
}
/* (wm_manage) maximized before it was shown (an app restoring how it was left) */
static void csd_max_first(Client *c)
{
	csd_atoms(); Atom type; int fmt; unsigned long n = 0, after; unsigned char *d = NULL; int m = 0;
	if (XGetWindowProperty(DPY, c->w, A_NSTATE, 0, 32, False, XA_ATOM, &type, &fmt, &n, &after, &d) == Success && d) { for (unsigned long i = 0; i < n; i++) if (((Atom *)d)[i] == A_NMAXV || ((Atom *)d)[i] == A_NMAXH) m = 1; XFree(d); }
	if (m && !c->app && !c->full && !c->trans) wm_maximise_now(c, 1);
}
/* the drag: with the pointer (Rudolf's title bar or edges, or the app's _NET_WM_MOVERESIZE) until the button is let go, or with the
   keyboard (the app's own Move and Size: the arrows, Enter or a click ends it, Escape puts the window back) */
static void drag_motion(int rx, int ry);
static int drag_begin(Client *c, int ed, int rx, int ry, Time t, int kbd)
{
	DRAG.c = c; DRAG.edge = ed; DRAG.px = rx; DRAG.py = ry; DRAG.x = c->x; DRAG.y = c->y; DRAG.w = c->cw; DRAG.h = c->ch; DRAG.kbd = kbd; DRAG.kx = rx; DRAG.ky = ry;
	if (XGrabPointer(DPY, c->frame, False, ButtonReleaseMask | PointerMotionMask | (kbd ? ButtonPressMask : 0), GrabModeAsync, GrabModeAsync, None, cursor_for(ed), t) != GrabSuccess) { DRAG.c = NULL; DRAG.kbd = 0; return 0; }
	if (kbd && XGrabKeyboard(DPY, c->frame, False, GrabModeAsync, GrabModeAsync, t) != GrabSuccess) { XUngrabPointer(DPY, t); DRAG.c = NULL; DRAG.kbd = 0; return 0; }
	return 1;
}
static void drag_end(Time t, int cancel)
{
	Client *c = DRAG.c; if (!c) return;
	XUngrabPointer(DPY, t); if (DRAG.kbd) XUngrabKeyboard(DPY, t);
	DRAG.c = NULL; DRAG.kbd = 0;
	if (!g_list_find(CLIENTS, c)) return;
	if (cancel) { c->x = DRAG.x; c->y = DRAG.y; c->cw = DRAG.w; c->ch = DRAG.h; place(c); frame_draw(c); }
	else if (!DRAG.edge) mon_drop(c);   /* (1.6.1 Part 1: let go on another monitor) */
	tell_geom(c); desk_shadows(); state_write();
}
static void drag_key(XKeyEvent *e)
{
	KeySym k = XLookupKeysym(e, 0); int s = (int)lround(10 * Z);
	if (k == XK_Return || k == XK_KP_Enter || k == XK_space) { drag_end(e->time, 0); return; }
	if (k == XK_Escape) { drag_end(e->time, 1); return; }
	if (k == XK_Left || k == XK_KP_Left) DRAG.kx -= s; else if (k == XK_Right || k == XK_KP_Right) DRAG.kx += s;
	else if (k == XK_Up || k == XK_KP_Up) DRAG.ky -= s; else if (k == XK_Down || k == XK_KP_Down) DRAG.ky += s; else return;
	XWarpPointer(DPY, None, ROOT, 0, 0, 0, 0, DRAG.kx, DRAG.ky); drag_motion(DRAG.kx, DRAG.ky);
}
/* _NET_WM_MOVERESIZE: x_root, y_root, direction (0 top-left .. 7 left clockwise, 8 move, 9 size and 10 move with the keyboard,
   11 cancel), button, source. A maximized window is not moved by its bar, as Rudolf's own title bar does not move it */
static void csd_moveresize(Client *c, XClientMessageEvent *m)
{
	long dir = m->data.l[2];
	if (dir == 11) { if (DRAG.c == c) drag_end(CurrentTime, 0); return; }
	if (DRAG.c || c->full || c->min || c->hidden || c->off || dir < 0 || dir > 10) return;
	if (c->max) return;
	static const int ED[9] = {5, 4, 6, 2, 10, 8, 9, 1, 0};   /* (drag_motion's edges: 1 left, 2 right, 4 top, 8 bottom; 0 move) */
	if (c != FOCUS) wm_focus(c);
	if (dir >= 9) {
		int rx = dir == 10 ? c->x + fw_of(c) / 2 : c->x + fw_of(c) - 1, ry = dir == 10 ? c->y + th_of(c) / 2 + BW : c->y + fh_of(c) - 1;
		XWarpPointer(DPY, None, ROOT, 0, 0, 0, 0, rx, ry); drag_begin(c, dir == 10 ? 0 : 10, rx, ry, CurrentTime, 1); return; }
	Window r, ch; int rx, ry, wx, wy; unsigned mask = 0;
	XQueryPointer(DPY, ROOT, &r, &ch, &rx, &ry, &wx, &wy, &mask);
	if (!(mask & (Button1Mask | Button2Mask | Button3Mask))) return;   /* (the button let go before the request came: nothing to drag) */
	drag_begin(c, ED[dir], (int)m->data.l[0], (int)m->data.l[1], CurrentTime, 0);
}
/* the requests (wm_filter's client messages): 1 when it was one of these */
static int csd_message(XClientMessageEvent *m)
{
	csd_atoms(); Client *c = client_of(m->window);
	if (m->message_type == A_NREQFE) {   /* (before it is shown: what its frame will be) */
		if (c) { csd_extents(c); return 1; }
		long v[4] = { BW, BW, csd_nodeco(m->window) ? BW : TH, BW }; XChangeProperty(DPY, m->window, A_NFE, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)v, 4); return 1; }
	if (!c || c->w != m->window) return 0;
	if (m->message_type == A_NMOVERESIZE) { csd_moveresize(c, m); return 1; }
	if (m->message_type == A_GTKMENU) { if (!c->full) win_menu_at(c, (int)m->data.l[1], (int)m->data.l[2]); return 1; }
	if (m->message_type == A_NSTATE) {
		Atom p1 = (Atom)m->data.l[1], p2 = (Atom)m->data.l[2]; long act = m->data.l[0];
		if (p1 == A_NMAXV || p1 == A_NMAXH || p2 == A_NMAXV || p2 == A_NMAXH) { int on = act == 1 ? 1 : act == 0 ? 0 : !c->max; if (!c->full && !c->trans && on != c->max) wm_maximise(c, on); else net_state_sync(c); }
		return 0;   /* (full screen and minimize: wm_filter's own) */
	}
	return 0;
}
/* ---- (1.6.1, docs/plan-1.6.1.md Part 15, item 3) Rudolf's title bar instead, by choice, per app. The window's menu (its taskbar
   button's, Rudolf's title bar's, a GTK header bar's right-click) on a window that draws its own bar has "Title bar: the app's own" and
   "Title bar: Rudolf's". With Rudolf's, the app's own bar is cut away as Oracle's Chromium bar is (the clip window and its top inset), at
   its height from the list (/usr/share/rudolf/titlebars, "ID<TAB>cut<TAB>N": N the bar's height in the app's pixels at 100 %, scaled
   as the desktop is; ID the window's class, WM_CLASS), offered only for apps on it (their bar holds only the title and the three
   buttons); for any other the menu says what would be lost instead. Kept per account and app: bd95titlebar:ID in its settings
   (rudolfd's copy of the pages' storage), {"CLASS":"rudolf"}, read by the shell. */
typedef struct { char id[64]; int h; } TbCut;
static TbCut TBCUT[32]; static int NTBCUT = -1;
static char TBCH[2048], TBCH_FOR[80];   /* (the account's choices, as kept; whose) */
static void tbcut_load(void)
{
	if (NTBCUT >= 0) return; NTBCUT = 0;
	const char *f = getenv("RUDOLF_TITLEBARS") ? getenv("RUDOLF_TITLEBARS") : "/usr/share/rudolf/titlebars"; gchar *t = NULL;
	if (!g_file_get_contents(f, &t, NULL, NULL)) return;
	for (char *l = strtok(t, "\n"); l && NTBCUT < 32; l = strtok(NULL, "\n")) { char id[64], how[16]; int h = 0;
		if (l[0] != '#' && sscanf(l, "%63[^\t]\t%15[^\t]\t%d", id, how, &h) == 3 && !strcmp(how, "cut") && h > 0 && h <= 200) { snprintf(TBCUT[NTBCUT].id, sizeof TBCUT[0].id, "%s", id); TBCUT[NTBCUT++].h = h; } }
	g_free(t);
}
/* the window's app, as the list and the choices name it: its class (WM_CLASS), lower case; 0 when it has none fit for a key */
static int tb_key(Client *c, char *k, size_t n)
{
	XClassHint ch = {0}; k[0] = 0; if (!XGetClassHint(DPY, c->w, &ch)) return 0;
	snprintf(k, n, "%s", ch.res_class ? ch.res_class : ch.res_name ? ch.res_name : ""); if (ch.res_name) XFree(ch.res_name); if (ch.res_class) XFree(ch.res_class);
	for (char *p = k; *p; p++) *p = g_ascii_tolower(*p);
	return k[0] && strlen(k) < 60 && strspn(k, "abcdefghijklmnopqrstuvwxyz0123456789._-") == strlen(k);
}
static int tbcut_of(Client *c) { char k[64]; tbcut_load(); if (!tb_key(c, k, sizeof k)) return 0; for (int i = 0; i < NTBCUT; i++) if (!g_ascii_strcasecmp(TBCUT[i].id, k)) return TBCUT[i].h; return 0; }
static void tbch_load(void)
{
	if (!USER_ID[0] || !id_ok(USER_ID)) { TBCH[0] = TBCH_FOR[0] = 0; return; }
	if (!strcmp(TBCH_FOR, USER_ID)) return;
	char p[160]; snprintf(p, sizeof p, "/api/lsync?key=bd95titlebar%%3A%s", USER_ID); JV *r = jget(p); JV *v = jprop(r, "v");
	snprintf(TBCH, sizeof TBCH, "%s", v && v->t == 3 && v->s[0] == '{' ? v->s : "{}"); snprintf(TBCH_FOR, sizeof TBCH_FOR, "%s", USER_ID); jfree(r);
}
static int tb_rudolf(Client *c)
{
	char k[64]; if (!tb_key(c, k, sizeof k)) return 0; tbch_load(); JV *o = jparse(TBCH), *v = jprop(o, k);
	int r = v && v->t == 3 && !strcmp(v->s, "rudolf"); jfree(o); return r;
}
/* (csd_read, csd_update) the cut, when Rudolf's title bar is chosen for an app on the list: the window framed as any other */
static void tb_cut_apply(Client *c)
{
	int h = c->nodeco ? tbcut_of(c) : 0; c->tbcut = h && tb_rudolf(c) ? (int)lround(h * Z) : 0;
	if (c->tbcut) { c->csd = 0; c->it = c->gt + c->tbcut; }
}
static void tb_choose(Client *c, int rudolf)
{
	char k[64]; if (!tb_key(c, k, sizeof k) || !USER_ID[0] || !id_ok(USER_ID) || (rudolf && !tbcut_of(c))) return;
	tbch_load(); JV *o = jparse(TBCH); GString *js = g_string_new("{"); int n = 0;
	for (int i = 0; o && o->t == 5 && i < o->len; i++) { if (!strcmp(o->k[i], k) || o->v[i]->t != 3) continue; if (n++) g_string_append_c(js, ','); json_esc(js, o->k[i]); g_string_append_c(js, ':'); json_esc(js, o->v[i]->s); }
	if (rudolf) { if (n++) g_string_append_c(js, ','); json_esc(js, k); g_string_append(js, ":\"rudolf\""); }
	g_string_append_c(js, '}'); jfree(o);
	GString *b = g_string_new("{\"gen\":0,\"part\":1,\"set\":{"); char key[100]; snprintf(key, sizeof key, "bd95titlebar:%s", USER_ID); json_esc(b, key); g_string_append_c(b, ':'); json_esc(b, js->str); g_string_append(b, "},\"del\":[]}");
	if (http("POST", "/api/lsync", b->str, b->len, NULL) == 200) snprintf(TBCH, sizeof TBCH, "%s", js->str);
	g_string_free(js, TRUE); g_string_free(b, TRUE);
	/* every window of that app, now */
	for (GList *l = CLIENTS; l; l = l->next) { Client *o2 = l->data; char k2[64]; if (o2->nodeco && tb_key(o2, k2, sizeof k2) && !strcmp(k2, k)) { o2->gt = -1; csd_update(o2); } }
}
static void frame_press(Client *c, XButtonEvent *e)
{
	if (c != FOCUS && e->button != 2) wm_focus(c);
	/* (the owner's photo) the title bar's other buttons, as the page's (base_v24.html): a right-click there opens the window's menu at
	   the pointer (winMenu: the page's own rows, as the taskbar button's), a middle-click sends the window behind the others */
	int TH = th_of(c);
	if ((e->button == 3 || e->button == 2) && e->y < TH && !c->csd && !btn_at(c, e->x, e->y) && !c->full) { if (e->button == 3) win_menu_at(c, e->x_root, e->y_root); else wm_lower(c); return; }
	if (e->button != 1) return;
	int b = btn_at(c, e->x, e->y);
	if (b) { c->press = b; frame_draw(c); return; }
	if (pn_press(c, e)) return;   /* (1.6.1 Part 2: a swipe on the title bar, the mobile build's) */
	if (c->full) return;
	int ed = c->max ? 0 : edge_at(c, e->x, e->y);
	if (!ed && e->y < TH) { if (e->time - c->last_click < 400) { c->last_click = 0; wm_maximise(c, !c->max); return; } c->last_click = e->time; if (c->max) return; }
	else if (!ed) return;
	drag_begin(c, ed, e->x_root, e->y_root, e->time, 0);
}
static void drag_motion(int rx, int ry)
{
	Client *c = DRAG.c; int dx = rx - DRAG.px, dy = ry - DRAG.py;
	if (!DRAG.edge) { c->x = DRAG.x + dx; c->y = DRAG.y + dy; if (c->y < desk_top()) c->y = desk_top(); XMoveWindow(DPY, c->frame, mon_ox(c) + c->x, mon_oy(c) + c->y); return; }
	/* (the owner's photo: as the page's addResize, shell25.js: the window's new size from its size with its edges (offsetWidth,
	   offsetHeight) and the pointer's way, at least 200 by 120, its top not above the desktop's) */
	int x = DRAG.x, y = DRAG.y, w = DRAG.w, h = DRAG.h, ow = DRAG.w + 2 * BW, oh = DRAG.h + th_of(c) + BW, tbh = th_of(c) - BW, mw = (int)lround(200 * Z), mh = (int)lround(120 * Z);
	if (DRAG.edge & 1) { int nw = MAX(mw, ow - dx); w = nw; x = DRAG.x + (ow - nw); }
	if (DRAG.edge & 2) w = MAX(mw, ow + dx);
	if (DRAG.edge & 4) { int nh = MAX(mh, oh - dy), nt = MAX(desk_top(), DRAG.y + (oh - nh)); h = oh + DRAG.y - nt - tbh; y = nt; }
	if (DRAG.edge & 8) h = MAX(mh, oh + dy) - tbh;
	c->x = x; c->y = y; c->cw = w; c->ch = h; place(c); frame_draw(c);
}
static void frame_release(Client *c, XButtonEvent *e)
{
	if (pn_release(e)) return;   /* (1.6.1 Part 2) */
	if (DRAG.c) { drag_end(e->time, 0); return; }
	int b = c->press; c->press = 0; frame_draw(c);
	if (b && b == btn_at(c, e->x, e->y)) { if (b == 1) wm_minimise(c); else if (b == 2) wm_maximise(c, !c->max); else wm_close(c); }
}
static double snd_play(const char *k); static void snd_set(JV *s); static void snd_cat(JV *v);   /* (the desktop's sounds, below: report 2) */
static void power(const char *what); static int sc_key(XKeyEvent *e); static void sc_grab(void); static void xi_raw_init(void); static void kb_sync(void);
static int XI_OP; static void xi_raw(XIRawEvent *r, int press);
static GdkFilterReturn wm_filter(GdkXEvent *xev, GdkEvent *ev, gpointer d)
{
	(void)ev; (void)d; XEvent *e = xev; Client *c;
	switch (e->type) {
	case KeyPress:   /* (the page's global shortcuts: item 5, sc_key) */
		if (DRAG.c && DRAG.kbd) { drag_key(&e->xkey); return GDK_FILTER_REMOVE; }   /* (1.6.1 Part 15: an app's own Move or Size) */
		if ((c = client_of(e->xkey.window)) && c->ak[0] && e->xkey.window == c->w && oracle_key(&e->xkey)) return GDK_FILTER_REMOVE;   /* (Oracle's keys, as on Standard: 12.3c) */
		if (sc_key(&e->xkey)) return GDK_FILTER_REMOVE;
		break;
	case KeyRelease:
		if (DRAG.c && DRAG.kbd) return GDK_FILTER_REMOVE;
		if (sc_key(&e->xkey)) return GDK_FILTER_REMOVE;
		break;
	case GenericEvent:   /* (XInput 2's raw keys: Super alone, 1.6.0) */
		if (XI_OP >= 0 && e->xcookie.extension == XI_OP && e->xcookie.data && (e->xcookie.evtype == XI_RawKeyPress || e->xcookie.evtype == XI_RawKeyRelease)) {
			xi_raw(e->xcookie.data, e->xcookie.evtype == XI_RawKeyPress); return GDK_FILTER_REMOVE; }
		break;
	case MapRequest:
		if (!(c = client_of(e->xmaprequest.window))) wm_manage(e->xmaprequest.window, NULL); else { XMapWindow(DPY, c->w); wm_focus(c); }
		return GDK_FILTER_REMOVE;
	case ConfigureRequest: {
		XConfigureRequestEvent *r = &e->xconfigurerequest; c = client_of(r->window);
		if (!c) { XWindowChanges wc = { r->x, r->y, r->width, r->height, r->border_width, r->above, r->detail }; XConfigureWindow(DPY, r->window, r->value_mask, &wc); return GDK_FILTER_REMOVE; }
		if (c->w == r->window && !c->full && !c->max) {
			int dw = c->clip ? c->il + c->ir : 0, dh = c->clip ? c->it + c->ib : 0;   /* (Oracle's Chromium asks with its own edges) */
			if (r->value_mask & CWWidth) c->cw = r->width - dw; if (r->value_mask & CWHeight) c->ch = r->height - dh;
			if (r->value_mask & CWX) c->x = r->x + (c->clip ? c->il : 0) - BW - mon_ox(c); if (r->value_mask & CWY) c->y = r->y + (c->clip ? c->it : 0) - th_of(c) - mon_oy(c);
			/* (a window making itself the screen's size is a game going full screen: as rudolf-display does) */
			if (c->cw >= SW && c->ch >= SH) { wm_fullscreen(c, 1); return GDK_FILTER_REMOVE; }
			place(c); frame_draw(c);
		} else tell_geom(c);
		return GDK_FILTER_REMOVE; }
	case ConfigureNotify:
		/* our own app windows resize themselves without asking (GTK): the frame follows */
		if ((c = client_of(e->xconfigure.window)) && c->w == e->xconfigure.window && c->app && !c->full && !c->max && e->xconfigure.event == c->frame &&
			(e->xconfigure.width != c->cw || e->xconfigure.height != c->ch)) { c->cw = e->xconfigure.width; c->ch = e->xconfigure.height; XResizeWindow(DPY, c->frame, fw_of(c), fh_of(c)); frame_draw(c); }
		return GDK_FILTER_CONTINUE;
	case UnmapNotify:
		if ((c = client_of(e->xunmap.window)) && c->w == e->xunmap.window && (e->xunmap.event == c->frame || (c->clip && e->xunmap.event == c->clip) || e->xunmap.send_event)) {   /* (not GDK's own copy: our windows') */
			if (c->ignore_unmap > 0) { c->ignore_unmap--; return GDK_FILTER_CONTINUE; }
			wm_unmanage(c, 0);
		}
		return GDK_FILTER_CONTINUE;
	case DestroyNotify:
		if ((c = client_of(e->xdestroywindow.window)) && c->w == e->xdestroywindow.window) wm_unmanage(c, 1);
		return GDK_FILTER_CONTINUE;
	case PropertyNotify:
		if ((c = client_of(e->xproperty.window)) && c->w == e->xproperty.window && (e->xproperty.atom == XA_WM_NAME || e->xproperty.atom == A_NNAME)) { wm_title(c); frame_draw(c); bar_redraw(); state_write(); }
		if (c && c->w == e->xproperty.window && A_MOTIF && (e->xproperty.atom == A_MOTIF || e->xproperty.atom == A_GTKFE)) csd_update(c);   /* (1.6.1 Part 15) */
		return GDK_FILTER_CONTINUE;
	case Expose:
		if ((c = client_of(e->xexpose.window)) && c->frame == e->xexpose.window) { if (!e->xexpose.count) frame_draw(c); return GDK_FILTER_REMOVE; }
		return GDK_FILTER_CONTINUE;
	case ButtonPress:
		if (DRAG.c && DRAG.kbd) { drag_end(e->xbutton.time, 0); return GDK_FILTER_REMOVE; }   /* (a click ends the keyboard's move: 1.6.1 Part 15) */
		if ((c = client_of(e->xbutton.window))) { fl_outside();
			int ri = rz_of(c, e->xbutton.window); if (ri >= 0) { XButtonEvent b = e->xbutton; rz_to_frame(c, ri, &b.x, &b.y); frame_press(c, &b); return GDK_FILTER_REMOVE; }
			if (c->frame == e->xbutton.window) frame_press(c, &e->xbutton);
			else { wm_focus(c); XAllowEvents(DPY, ReplayPointer, e->xbutton.time); }   /* a click into a window: it gets the focus, and the click */
			return GDK_FILTER_REMOVE;
		}
		return GDK_FILTER_CONTINUE;
	case ButtonRelease:
		if (DRAG.c && DRAG.kbd) return GDK_FILTER_REMOVE;
		if ((c = client_of(e->xbutton.window)) && (c->frame == e->xbutton.window || rz_of(c, e->xbutton.window) >= 0)) { frame_release(c, &e->xbutton); return GDK_FILTER_REMOVE; }
		return GDK_FILTER_CONTINUE;
	case MotionNotify:
		if (DRAG.c) { drag_motion(e->xmotion.x_root, e->xmotion.y_root); return GDK_FILTER_REMOVE; }
		if ((c = client_of(e->xmotion.window)) && rz_of(c, e->xmotion.window) >= 0) { int x = e->xmotion.x, y = e->xmotion.y; rz_to_frame(c, rz_of(c, e->xmotion.window), &x, &y);
			XDefineCursor(DPY, e->xmotion.window, cursor_for(edge_at(c, x, y))); return GDK_FILTER_REMOVE; }
		if ((c = client_of(e->xmotion.window)) && c->frame == e->xmotion.window) {
			int h = btn_at(c, e->xmotion.x, e->xmotion.y); if (h != c->hover) { c->hover = h; frame_draw(c); }
			XDefineCursor(DPY, c->frame, c->max || c->full || h ? None : cursor_for(edge_at(c, e->xmotion.x, e->xmotion.y)));
			return GDK_FILTER_REMOVE;
		}
		return GDK_FILTER_CONTINUE;
	case LeaveNotify:
		if ((c = client_of(e->xcrossing.window)) && c->frame == e->xcrossing.window) { if (c->hover) { c->hover = 0; frame_draw(c); } return GDK_FILTER_REMOVE; }
		return GDK_FILTER_CONTINUE;
	case ClientMessage: {
		XClientMessageEvent *m = &e->xclient; if (csd_message(m)) return GDK_FILTER_REMOVE;   /* (1.6.1 Part 15: _NET_WM_MOVERESIZE, maximize, the frame's extents) */
		c = client_of(m->window);
		if (m->message_type == A_NSTATE && c) { long act = m->data.l[0];
			if ((Atom)m->data.l[1] == A_NFULL || (Atom)m->data.l[2] == A_NFULL) wm_fullscreen(c, act == 1 ? 1 : act == 0 ? 0 : !c->full);
			if ((Atom)m->data.l[1] == A_NHIDDEN && act == 1) wm_minimise(c);
			return GDK_FILTER_REMOVE; }
		if (m->message_type == A_NACTIVE) {
			if (c) wm_focus(c); else if (m->window != ROOT) XSetInputFocus(DPY, m->window, RevertToPointerRoot, CurrentTime);   /* (our own windows: the menu, the sign-in) */
			return GDK_FILTER_REMOVE; }
		if (m->message_type == A_CHANGE && c && m->data.l[0] == IconicState) { wm_minimise(c); return GDK_FILTER_REMOVE; }
		return GDK_FILTER_CONTINUE; }
	}
	return GDK_FILTER_CONTINUE;
}
static void wm_init(void)
{
	DPY = GDK_DISPLAY_XDISPLAY(gdk_display_get_default()); ROOT = DefaultRootWindow(DPY);
	XErrorHandler old = XSetErrorHandler(wm_errh); wm_err = 0;
	XWindowAttributes ra; XGetWindowAttributes(DPY, ROOT, &ra);
	XSelectInput(DPY, ROOT, ra.your_event_mask | SubstructureRedirectMask | SubstructureNotifyMask | PropertyChangeMask);
	XSync(DPY, False); XSetErrorHandler(old);
	if (wm_err) { fprintf(stderr, "rudolf-native: another window manager is running: app windows are not framed\n"); return; }
	gdk_x11_display_error_trap_push(gdk_display_get_default());   /* (windows that go away while we handle them) */
	WM_OK = 1;
	A_PROTO = XInternAtom(DPY, "WM_PROTOCOLS", False); A_DEL = XInternAtom(DPY, "WM_DELETE_WINDOW", False); A_TAKE = XInternAtom(DPY, "WM_TAKE_FOCUS", False);
	A_WMSTATE = XInternAtom(DPY, "WM_STATE", False); A_NSTATE = XInternAtom(DPY, "_NET_WM_STATE", False); A_NFULL = XInternAtom(DPY, "_NET_WM_STATE_FULLSCREEN", False);
	A_NHIDDEN = XInternAtom(DPY, "_NET_WM_STATE_HIDDEN", False); A_NACTIVE = XInternAtom(DPY, "_NET_ACTIVE_WINDOW", False); A_NLIST = XInternAtom(DPY, "_NET_CLIENT_LIST", False);
	A_NNAME = XInternAtom(DPY, "_NET_WM_NAME", False); A_UTF8 = XInternAtom(DPY, "UTF8_STRING", False); A_NSUP = XInternAtom(DPY, "_NET_SUPPORTED", False);
	A_NCHECK = XInternAtom(DPY, "_NET_SUPPORTING_WM_CHECK", False); A_CHANGE = XInternAtom(DPY, "WM_CHANGE_STATE", False);
	/* what we do (1.6.1 Part 15: _NET_WM_MOVERESIZE too, for the apps drawing their own title bar; Rudolf's own windows never ask it) */
	csd_atoms();
	Atom sup[] = {A_NSTATE, A_NFULL, A_NHIDDEN, A_NACTIVE, A_NLIST, A_NNAME, A_NCHECK, A_NMOVERESIZE, A_NMAXV, A_NMAXH, A_NFE, A_NREQFE, A_GTKFE, A_GTKMENU};
	XChangeProperty(DPY, ROOT, A_NSUP, XA_ATOM, 32, PropModeReplace, (unsigned char *)sup, sizeof sup / sizeof sup[0]);
	Window chk = XCreateSimpleWindow(DPY, ROOT, -10, -10, 1, 1, 0, 0, 0);
	XChangeProperty(DPY, ROOT, A_NCHECK, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&chk, 1);
	XChangeProperty(DPY, chk, A_NCHECK, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&chk, 1);
	XChangeProperty(DPY, chk, A_NNAME, A_UTF8, 8, PropModeReplace, (unsigned char *)"rudolf-native", 13);
	static const unsigned SH_[9] = {XC_left_ptr, XC_left_side, XC_right_side, XC_top_side, XC_bottom_side, XC_top_left_corner, XC_top_right_corner, XC_bottom_left_corner, XC_bottom_right_corner};
	for (int i = 0; i < 9; i++) CUR[i] = XCreateFontCursor(DPY, SH_[i]);
	/* the screen itself (where no window is, and while windows start) shows the arrow, not X's bare "✕" (1.5.9 step 7) */
	XDefineCursor(DPY, ROOT, CUR[0]);
	gdk_window_add_filter(NULL, wm_filter, NULL);
	xi_raw_init(); sc_grab();   /* (the page's shortcuts, the switcher and Super: item 5) */
	/* windows already there (rudolf-native started again): framed */
	Window r, p, *k = NULL; unsigned n = 0;
	if (XQueryTree(DPY, ROOT, &r, &p, &k, &n)) for (unsigned i = 0; i < n; i++) {
		XWindowAttributes wa; if (!XGetWindowAttributes(DPY, k[i], &wa) || wa.override_redirect || wa.map_state != IsViewable) continue;
		if (gdk_x11_window_lookup_for_display(gdk_display_get_default(), k[i]) || k[i] == chk) continue;
		wm_manage(k[i], NULL);
	}
	if (k) XFree(k);
}

static void wm_manage_app(AppWin *a)
{
	if (!WM_OK || !a->win) return;
	GdkWindow *g = gtk_widget_get_window(a->win); if (!g) return;
	XSync(DPY, False); wm_manage(GDK_WINDOW_XID(g), a);
}
/* (1.6.0 Part 12) the app's window at the size and place the page gives it (base_v24.html win(): W by H inside its 1 px
   edge, the title bar included; 40 + 26 n from the left and 20 + 26 n from the desktop's top for the n-th window, kept on
   the desktop) */
static void appwin_probe(AppWin *a);
static void appwin_size(AppWin *a, int w, int h)
{
	Client *c = WM_OK ? client_of_app(a) : NULL; if (c && a->modal && w >= 80 && h >= 60) { a->pw = w; a->ph = h; return; }   /* (after the dialog: N4) */
	if (c && c->hidden && !c->wait && !a->hidden && !LOCKED) { c->hidden = 0; if (!c->off && !c->min) XMapWindow(DPY, c->frame); wm_focus(c); }   /* (its window, after it asked: N4) */
	if (!c || c->max || c->full || w < 80 || h < 60) { if (c && c->wait) appwin_probe(a); return; }
	int n = 0; for (GList *l = CLIENTS; l; l = l->next) { Client *o = l->data; if (o != c && !o->trans) n++; }
	double dw = desk_w() / Z, avail = work_h() / Z; w = (int)fmin(w, dw - 8); h = (int)fmin(h, avail - 8);   /* (the page's #desk: beside a bar at the left or the right) */
	double x = fmax(0, fmin(40 + n * 26, dw - w - 4)), y = fmax(0, fmin(20 + n * 26, avail - h - 4));
	c->cw = (int)lround(w * Z); c->ch = (int)lround((h - tw_tbh()) * Z); c->x = desk_left() + (int)lround(x * Z); c->y = work_top() + (int)lround(y * Z);
	place(c); frame_draw(c);
	if (c->cover) XMoveResizeWindow(DPY, c->cover, BW, TH, c->cw, c->ch);
	state_write();
	if (c->wait) { appwin_probe(a); return; }   /* (shown once its page has drawn at this size) */
	if (!c->opened) wopen_start(c);   /* (its first place: it opens, N15) */
}
/* (the owner's Vivobook report 1) an app's window waiting for its page: the page is asked to say when it has drawn at the window's
   size (two frames after its size is the window's: "ready"); then it is shown, at its own size, and opens (N15) */
#define WAIT_MS 2500
static void appwin_probe(AppWin *a)
{
	Client *c = WM_OK ? client_of_app(a) : NULL; if (!c || !c->wait || !a->view) return;
	char js[900]; snprintf(js, sizeof js, "(()=>{const W=%d,H=%d,t0=Date.now();let sent=0;const go=()=>{if(sent)return;sent=1;try{webkit.messageHandlers.rudolfNative.postMessage({w:%d,op:'ready'})}catch(e){}};"
		"const fit=()=>{const d=devicePixelRatio||1;return Math.abs(innerWidth*d-W)<=2&&Math.abs(innerHeight*d-H)<=2};"
		"const f=()=>{if(sent)return;if(fit()||Date.now()-t0>1500){requestAnimationFrame(()=>requestAnimationFrame(go));setTimeout(go,300);return}setTimeout(f,16)};f()})()",
		c->cw, c->ch, a->id);
	webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(a->view), js, -1, NULL, NULL, NULL, NULL, NULL);
}
/* (an app's window's own colour: html.dark .win's background, else the page's white) */
static unsigned long app_bg_pixel(void) { RGB bg = hexc(is_dark() ? "#0f172a" : "#ffffff", NULL); return ((unsigned long)lround(bg.r * 255) << 16) | ((unsigned long)lround(bg.g * 255) << 8) | (unsigned long)lround(bg.b * 255); }
static void cover_gone(Client *c) { if (c && c->cover) { XDestroyWindow(DPY, c->cover); c->cover = 0; } }
static void appwin_reveal(AppWin *a)
{
	Client *c = WM_OK ? client_of_app(a) : NULL; if (a->wait_t) { g_source_remove(a->wait_t); a->wait_t = 0; }
	if (!c || !c->wait) return; int was = c->wait; c->wait = 0;
	if (was == 2) { cover_gone(c); state_write(); return; }   /* (shown already, covered: its page now) */
	/* (the page's picture is drawn into its window as X shows it: until then the app's colour there, not what was on the screen before) */
	if (c->hidden && !a->hidden && !LOCKED && a->modal != 2) { XSetWindowBackground(DPY, c->w, app_bg_pixel()); c->hidden = 0; if (!c->off && !c->min) { XMapWindow(DPY, c->frame); XSync(DPY, False); frame_draw(c); } wm_focus(c); }
	if (a->view) gtk_widget_queue_draw(a->view);
	bar_redraw(); state_write();
	if (!c->opened && !c->hidden) wopen_start(c);
}
static gboolean appwin_wait_last(gpointer d);
static gboolean appwin_wait_late(gpointer d)
{
	AppWin *a = d; if (!g_list_find(WINS, a)) return G_SOURCE_REMOVE; a->wait_t = 0;
	Client *c = WM_OK ? client_of_app(a) : NULL; if (!c || c->wait != 1) return G_SOURCE_REMOVE;
	fprintf(stderr, "rudolf-native: %s's page is not ready after %d ms: its window shown, covered until it is\n", a->k, WAIT_MS);
	c->wait = 2;
	if (c->hidden && !a->hidden && !LOCKED && a->modal != 2) {
		XSetWindowAttributes at = {0}; at.background_pixel = app_bg_pixel();
		c->cover = XCreateWindow(DPY, c->frame, BW, TH, c->cw, c->ch, 0, CopyFromParent, InputOutput, CopyFromParent, CWBackPixel, &at); XMapRaised(DPY, c->cover);
		c->hidden = 0; if (!c->off && !c->min) XMapWindow(DPY, c->frame); wm_focus(c); bar_redraw();
		if (!c->opened) wopen_start(c); }
	a->wait_t = g_timeout_add(10000, appwin_wait_last, a);   /* (and the cover gone after 10 s more, whatever the page says) */
	state_write(); return G_SOURCE_REMOVE;
}
static gboolean appwin_wait_last(gpointer d)
{
	AppWin *a = d; if (!g_list_find(WINS, a)) return G_SOURCE_REMOVE; a->wait_t = 0;
	Client *c = WM_OK ? client_of_app(a) : NULL; if (c && c->wait == 2) { fprintf(stderr, "rudolf-native: %s's page never said it was ready: shown as it is\n", a->k); appwin_reveal(a); }
	return G_SOURCE_REMOVE;
}
static void appwin_wait_arm(AppWin *a) { if (!a) return; if (a->wait_t) g_source_remove(a->wait_t); a->wait_t = g_timeout_add(WAIT_MS, appwin_wait_late, a); }
/* (a page that is not Rudolf's own one-app page never says it is ready: shown once it has loaded) */
static void on_load_ready(WebKitWebView *v, WebKitLoadEvent ev, gpointer d)
{
	AppWin *a = d; if (ev != WEBKIT_LOAD_FINISHED || !g_list_find(WINS, a)) return;
	char js[200]; snprintf(js, sizeof js, "(typeof ONEAPP=='undefined'||!ONEAPP)&&window.webkit&&webkit.messageHandlers.rudolfNative.postMessage({w:%d,op:'ready'})", a->id);
	webkit_web_view_evaluate_javascript(v, js, -1, NULL, NULL, NULL, NULL, NULL);
}
static void frame_redraw_app(AppWin *a) { Client *c = WM_OK ? client_of_app(a) : NULL; if (c) { wm_title(c); frame_draw(c); } }
/* (1.6.0 Part 12, test findings N2, N4, N10) a page's dialog (oneapp59.js: .rmodal, .natdlg) is over the whole desktop on Standard,
   dimmed and centred on the screen, with the windows and the taskbar under it. Here the app's window covers the screen while one
   is open (full screen, above the taskbar), its page given a picture of the screen as it was (the page dims it as Standard's
   dims the desktop), and goes back after. A page with no window of its own yet (an app asking before it opens one: the camera's
   "Allow access?", a game's download) is out of sight while the picture is taken, has no taskbar button while it asks (Standard
   has none) and is out of sight again after, until its window comes (its size then: a->pw, a->ph). */
static void modal_send(AppWin *a, GdkPixbuf *pb)
{
	gchar *buf = NULL; gsize n = 0; char *js = NULL;
	if (pb && gdk_pixbuf_save_to_buffer(pb, &buf, &n, "png", NULL, "compression", "1", NULL)) { gchar *b = g_base64_encode((guchar *)buf, n); js = g_strdup_printf("window.__oaModalBg&&__oaModalBg('data:image/png;base64,%s')", b); g_free(b); }
	else js = g_strdup("window.__oaModalBg&&__oaModalBg('')");
	if (a->view) webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(a->view), js, -1, NULL, NULL, NULL, NULL, NULL);
	g_free(js); g_free(buf); if (pb) g_object_unref(pb);
}
static gint64 PT_SHOWN;   /* (1.6.0, tests: when the portal's page last showed a dialog over the screen, ms since 1970) */
static void modal_cover(AppWin *a)
{
	Client *c = WM_OK ? client_of_app(a) : NULL; if (!c || !a->modal) return;
	if (is_bg(a)) PT_SHOWN = g_get_real_time() / 1000;
	GdkPixbuf *pb = screen_copy(0, 0, SW, SH);
	if (!c->full) a->wasontop = c->ontop; c->ontop = 1;
	if (c->hidden) { c->hidden = 0; if (!c->off && !c->min) XMapWindow(DPY, c->frame); }
	a->mfull = !c->full; if (!c->full) wm_fullscreen(c, 1); else wm_focus(c);
	bar_redraw(); state_write(); modal_send(a, pb);
}
/* (1.6.0 Part 12, test finding N1) the page's full screen (a video's ⛶, F11, a game): on Standard the page is the screen and an element
   made full screen fills it (shell26.js fsStandIn, the shell's stand-in); here the page is the app's window, so the page uses the same
   stand-in (oneapp59.js) and the window covers the screen, above the taskbar, for as long. (WebKit's own full screen showed a video
   black under the native shell, with WebKit's control bar alone.) */
static void appwin_fs(AppWin *a, int on)
{
	Client *c = WM_OK ? client_of_app(a) : NULL; if (!c || a->modal) return;
	if (on && !c->full) { a->wasontop = c->ontop; c->ontop = 1; wm_fullscreen(c, 1); }
	else if (!on && c->full) { c->ontop = a->wasontop; wm_fullscreen(c, 0); }
	bar_redraw(); state_write();
}
static gboolean modal_cover_late(gpointer d) { AppWin *a = d; if (g_list_find(WINS, a)) modal_cover(a); return G_SOURCE_REMOVE; }
static void appwin_modal(AppWin *a, int on)
{
	Client *c = WM_OK ? client_of_app(a) : NULL;
	if (on) { if (a->modal || a->hidden) return; a->modal = a->sized ? 1 : 2;
		if (c && c->wait) { if (a->wait_t) { g_source_remove(a->wait_t); a->wait_t = 0; } c->wait = 0; cover_gone(c); }   /* (its dialog now, over the screen) */
		if (!c) { modal_send(a, NULL); return; }
		/* (not the window itself in the picture: one with nothing of its own yet is taken away first, and what was under it drawn again) */
		if (a->modal == 2 && !c->hidden && !c->full) { c->hidden = 1; XUnmapWindow(DPY, c->frame); XSync(DPY, False); g_timeout_add(250, modal_cover_late, a); return; }
		modal_cover(a); return; }
	if (!a->modal) return; int was = a->modal; a->modal = 0;
	if (!c) return;
	if (a->mfull) { c->ontop = a->wasontop; if (c->full) wm_fullscreen(c, 0); }
	if (was == 2 && !a->sized) { c->hidden = 1; XUnmapWindow(DPY, c->frame); if (FOCUS == c) { FOCUS = NULL; wm_focus(top_client()); } }
	else if (a->pw) { int w = a->pw, h = a->ph; a->pw = a->ph = 0; appwin_size(a, w, h); }
	bar_redraw(); state_write();
}

static void appwin_activate(AppWin *a)
{
	Client *c = WM_OK ? client_of_app(a) : NULL;
	if (c) { if (c == FOCUS && !c->min) wm_minimise(c); else { int was = c->min; wm_focus(c); if (!was) lp_tbb(c); } return; }
	if (gtk_widget_get_visible(a->win) && ACTIVE == a) { gtk_widget_hide(a->win); ACTIVE = NULL; }   /* (step 4: minimise) */
	else { gtk_widget_show(a->win); gtk_window_present(GTK_WINDOW(a->win)); ACTIVE = a; }
	bar_redraw(); state_write();
}

/* ------------------------------------------------------------------ the page's CSS, drawn (1.6.0 Part 12, "The rule")
 * Native draws what the page's CSS draws, measured against it by iso/qt/native/same_t.sh: boxes with the page's
 * background colours, the Candy style's gloss, borders, corner radii and shadows (CSS's order: outer shadows, the
 * background, the gloss, the inset shadows, the border), and writing in the page's font (fonts47.js uiFontStack: the
 * Font setting, DejaVu Sans by default) at its sizes and weights, placed on the line as CSS places it. */
typedef struct { double r, g, b, a; } RGBA;
static RGBA ca(RGB c, double a) { RGBA o = { c.r, c.g, c.b, a }; return o; }
static RGBA cx(const char *hex, double a) { return ca(hexc(hex, "#000000"), a); }
static const RGBA CLEAR = { 0, 0, 0, 0 };
/* color-mix(in srgb, A P%, B) */
static RGBA cmix(RGBA a, double p, RGBA b)
{
	double q = 1 - p, al = a.a * p + b.a * q; RGBA o = { 0, 0, 0, al };
	if (al > 0) { o.r = (a.r * a.a * p + b.r * b.a * q) / al; o.g = (a.g * a.a * p + b.g * b.a * q) / al; o.b = (a.b * a.a * p + b.b * b.a * q) / al; }
	return o;
}
static void srca(cairo_t *cr, RGBA c) { cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a); }
typedef struct { double dx, dy, blur; RGBA c; int inset; } Shd;
typedef struct {
	RGBA bg; double gl0, gl1;   /* the gloss: white from gl0 (top) to gl1 (half way), none below 52 % (0: none) */
	int ng; RGBA gc[3]; double gp[3];   /* a background gradient from top to bottom (ng stops: colour, place) */
	double bw; RGBA bc; double rad; Shd sh[4]; int nsh; RGBA fg; int filt;   /* filt: 1 grayscale, 2 brightness(.22) */
} Box;
/* (1.6.0) High contrast: styles25.css html.hc .flyout, #menu, .ctx, .rmbox: a 2 px white edge */
static void hc_edge(Box *b) { if (S.hc) { b->bw = 2; b->bc = cx("#ffffff", 1); } }
static void shd_add(Box *b, double dx, double dy, double blur, RGBA c, int inset) { if (b->nsh < 4) { Shd s = { dx, dy, blur, c, inset }; b->sh[b->nsh++] = s; } }
/* (Chromium snaps a box's edges to whole pixels) */
static void snap(double *x, double *y, double *w, double *h) { double x1 = round((*x + *w) * Z) / Z, y1 = round((*y + *h) * Z) / Z; *x = round(*x * Z) / Z; *y = round(*y * Z) / Z; *w = x1 - *x; *h = y1 - *y; }   /* (the screen's pixels: with the display scaling too) */
static void box_paint(cairo_t *cr, double x, double y, double w, double h, const Box *b)
{
	snap(&x, &y, &w, &h); if (w <= 0 || h <= 0) return;
	double r = fmin(b->rad, fmin(w, h) / 2);
	/* outer shadows (the last listed first) */
	for (int i = b->nsh - 1; i >= 0; i--) { const Shd *s = &b->sh[i]; if (s->inset) continue;
		cairo_save(cr); rrect(cr, x - 4000, y - 4000, w + 8000, h + 8000, 0); rrect(cr, x, y, w, h, r); cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD); cairo_clip(cr);
		if (s->blur <= 0) { rrect(cr, x + s->dx, y + s->dy, w, h, r); srca(cr, s->c); cairo_fill(cr); }
		else { int n = 8; double sg = s->blur / 2; for (int k = 0; k < n; k++) { double e = sg * (2.0 * (k + .5) / n - 1) * 1.6; RGBA c = s->c; c.a /= n;
			rrect(cr, x + s->dx - e, y + s->dy - e, w + 2 * e, h + 2 * e, r + e > 0 ? r + e : 0); srca(cr, c); cairo_fill(cr); } }
		cairo_restore(cr); }
	cairo_save(cr); rrect(cr, x, y, w, h, r); cairo_clip(cr);
	if (b->bg.a > 0) { srca(cr, b->bg); cairo_paint(cr); }
	if (b->ng) { cairo_pattern_t *g = cairo_pattern_create_linear(0, y, 0, y + h); for (int i = 0; i < b->ng; i++) cairo_pattern_add_color_stop_rgba(g, b->gp[i], b->gc[i].r, b->gc[i].g, b->gc[i].b, b->gc[i].a);
		cairo_set_source(cr, g); cairo_paint(cr); cairo_pattern_destroy(g); }
	if (b->gl0 > 0) { cairo_pattern_t *g = cairo_pattern_create_linear(0, y, 0, y + h);
		cairo_pattern_add_color_stop_rgba(g, 0, 1, 1, 1, b->gl0); cairo_pattern_add_color_stop_rgba(g, .5, 1, 1, 1, b->gl1); cairo_pattern_add_color_stop_rgba(g, .52, 1, 1, 1, 0);
		cairo_set_source(cr, g); cairo_paint(cr); cairo_pattern_destroy(g); }
	/* inset shadows: inside the padding box, less the box moved by the offset */
	double bw = b->bw, ix = x + bw, iy = y + bw, iw = w - 2 * bw, ih = h - 2 * bw, ir = fmax(0, r - bw);
	for (int i = b->nsh - 1; i >= 0; i--) { const Shd *s = &b->sh[i]; if (!s->inset) continue;
		cairo_save(cr); rrect(cr, ix, iy, iw, ih, ir); cairo_clip(cr);
		rrect(cr, ix - 4000, iy - 4000, iw + 8000, ih + 8000, 0); rrect(cr, ix + s->dx, iy + s->dy, iw, ih, ir); cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
		srca(cr, s->c); cairo_fill(cr); cairo_restore(cr); }
	cairo_restore(cr);
	if (bw > 0 && b->bc.a > 0) { rrect(cr, x, y, w, h, r); rrect(cr, x + bw, y + bw, w - 2 * bw, h - 2 * bw, fmax(0, r - bw)); cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD); srca(cr, b->bc); cairo_fill(cr); cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING); }
}

/* (1.6.0 Part 12, "The rule") a shadow of something drawn: its shape (an A8 surface of the page's units at Z) blurred
   (CSS's blur radius: a Gaussian of half of it, three box blurs) and laid down in a colour, moved by dx, dy */
static void a8_blur(cairo_surface_t *m, double sigma)
{
	if (sigma <= 0) return; cairo_surface_flush(m);
	int w = cairo_image_surface_get_width(m), h = cairo_image_surface_get_height(m), st = cairo_image_surface_get_stride(m); unsigned char *d = cairo_image_surface_get_data(m);
	int r = (int)floor(sigma * 3 * sqrt(2 * M_PI) / 4 + .5); if (r < 1) return; unsigned char *t = g_malloc(MAX(w, h));
	for (int pass = 0; pass < 3; pass++) {
		for (int y = 0; y < h; y++) { unsigned char *row = d + y * st; int sum = 0; for (int x = -r; x <= r; x++) sum += row[CLAMP(x, 0, w - 1)];
			for (int x = 0; x < w; x++) { t[x] = sum / (2 * r + 1); sum += row[MIN(x + r + 1, w - 1)] - row[MAX(x - r, 0)]; } memcpy(row, t, w); }
		for (int x = 0; x < w; x++) { int sum = 0; for (int y = -r; y <= r; y++) sum += d[CLAMP(y, 0, h - 1) * st + x];
			for (int y = 0; y < h; y++) { t[y] = sum / (2 * r + 1); sum += d[MIN(y + r + 1, h - 1) * st + x] - d[MAX(y - r, 0) * st + x]; } for (int y = 0; y < h; y++) d[y * st + x] = t[y]; }
	}
	g_free(t); cairo_surface_mark_dirty(m);
}
/* ---- writing */
static char UIFAM[96] = "DejaVu Sans, Verdana, sans-serif";
/* fonts47.js uiFont and uiFontStack: the Font setting's family, then its look-alike, then the generic family */
static void ui_family(const char *ff)
{
	static const char *const OLD[][2] = {{"Tahoma", "DejaVu Sans"}, {"Verdana", "DejaVu Sans"}, {"Arial", "Liberation Sans"}, {"Trebuchet MS", "Fira Sans"}, {"Times New Roman", "Liberation Serif"},
		{"Georgia", "Gelasio"}, {"Comic Sans MS", "Comic Neue"}, {"Courier New", "Liberation Mono"}, {"Lucida Console", "DejaVu Sans Mono"}};
	static const char *const ALT[][2] = {{"DejaVu Sans", "Verdana"}, {"DejaVu Sans Condensed", "Tahoma"}, {"Liberation Sans", "Arial"}, {"Fira Sans", "Trebuchet MS"}, {"Liberation Serif", "Times New Roman"},
		{"Gelasio", "Georgia"}, {"DejaVu Serif", "Georgia"}, {"Comic Neue", "Comic Sans MS"}, {"Liberation Mono", "Courier New"}, {"DejaVu Sans Mono", "Lucida Console"}};
	static const char *const OK[] = {"DejaVu Sans", "DejaVu Sans Condensed", "DejaVu Serif", "Liberation Sans", "Liberation Serif", "Fira Sans", "Gelasio", "Comic Neue", "Atkinson Hyperlegible",
		"OpenDyslexic", "Liberation Mono", "DejaVu Sans Mono", NULL};
	const char *n = ff && *ff ? ff : "DejaVu Sans";
	for (unsigned i = 0; i < G_N_ELEMENTS(OLD); i++) if (!strcmp(OLD[i][0], n)) n = OLD[i][1];
	int ok = 0; for (int i = 0; OK[i]; i++) if (!strcmp(OK[i], n)) ok = 1; if (!ok) n = "DejaVu Sans";
	if (!strcmp(n, "DejaVu Sans Condensed")) { snprintf(UIFAM, sizeof UIFAM, "DejaVu Sans Condensed, DejaVu Sans, Tahoma, sans-serif"); return; }
	const char *alt = "Arial"; for (unsigned i = 0; i < G_N_ELEMENTS(ALT); i++) if (!strcmp(ALT[i][0], n)) alt = ALT[i][1];
	snprintf(UIFAM, sizeof UIFAM, "%s, %s, %s", n, alt, strstr(n, "Mono") ? "monospace" : strstr(n, "Serif") || strstr(n, "Gelasio") ? "serif" : "sans-serif");
}
#define SYSUI "system-ui, sans-serif"
static PangoLayout *css_layout(cairo_t *cr, const char *t, const char *fam, int wt, double px)
{
	PangoLayout *pl = pango_cairo_create_layout(cr);
	cairo_font_options_t *fo = cairo_font_options_create(); cairo_font_options_set_antialias(fo, CAIRO_ANTIALIAS_GRAY);
	cairo_font_options_set_hint_style(fo, CAIRO_HINT_STYLE_SLIGHT); cairo_font_options_set_hint_metrics(fo, CAIRO_HINT_METRICS_OFF);
	pango_cairo_context_set_font_options(pango_layout_get_context(pl), fo); cairo_font_options_destroy(fo);
	pango_context_set_round_glyph_positions(pango_layout_get_context(pl), FALSE); pango_layout_context_changed(pl);   /* (Chromium's advances: not whole pixels) */
	PangoFontDescription *fd = pango_font_description_new(); pango_font_description_set_family(fd, fam); pango_font_description_set_weight(fd, wt);
	pango_font_description_set_absolute_size(fd, px * PANGO_SCALE); pango_layout_set_font_description(pl, fd); pango_font_description_free(fd);
	pango_layout_set_text(pl, t, -1); return pl;
}
/* the font's ascent and descent as CSS takes them (whole pixels) */
static void css_metrics(PangoLayout *pl, double *asc, double *desc)
{
	PangoContext *pc = pango_layout_get_context(pl); PangoFontMetrics *m = pango_context_get_metrics(pc, pango_layout_get_font_description(pl), NULL);
	*asc = round(pango_font_metrics_get_ascent(m) / (double)PANGO_SCALE); *desc = round(pango_font_metrics_get_descent(m) / (double)PANGO_SCALE); pango_font_metrics_unref(m);
}
static double css_width(cairo_t *cr, const char *t, const char *fam, int wt, double px)
{
	PangoLayout *pl = css_layout(cr, t, fam, wt, px); PangoRectangle lr; pango_layout_get_extents(pl, NULL, &lr); g_object_unref(pl); return lr.width / (double)PANGO_SCALE;
}
/* writing on a line box at (x, top) of height LH (0: the font's own, CSS's "normal"), the baseline where CSS puts it;
   max: the width it may take (an ellipsis after); returns its width */
static double css_text(cairo_t *cr, const char *t, const char *fam, int wt, double px, double x, double top, double lh, RGBA c, double max)
{
	PangoLayout *pl = css_layout(cr, t, fam, wt, px); double a, d; css_metrics(pl, &a, &d); if (lh <= 0) lh = a + d;
	if (max > 0) { pango_layout_set_width(pl, (int)(max * PANGO_SCALE)); pango_layout_set_ellipsize(pl, PANGO_ELLIPSIZE_END); }
	PangoRectangle lr; pango_layout_get_extents(pl, NULL, &lr);
	double base = top + floor((lh - (a + d)) / 2) + a, b = pango_layout_get_baseline(pl) / (double)PANGO_SCALE;   /* (Blink: the half-leading floored) */
	{ double bx = x, by = base; cairo_user_to_device(cr, &bx, &by); by = round(by); cairo_device_to_user(cr, &bx, &by); base = by; }   /* (the baseline on a whole pixel of the screen, as Chromium's: with the display scaling too) */
	srca(cr, c); cairo_move_to(cr, x, base - b); pango_cairo_show_layout(cr, pl); g_object_unref(pl); return lr.width / (double)PANGO_SCALE;
}
/* the baseline CSS gives a line of this font in a box of height H (centred: a button's or a flex item's content) */
static double css_ascent(cairo_t *cr, const char *fam, int wt, double px, double *desc)
{
	PangoLayout *pl = css_layout(cr, "x", fam, wt, px); double a, d; css_metrics(pl, &a, &d); g_object_unref(pl); if (desc) *desc = d; return a;
}

/* ------------------------------------------------------------------ the taskbar's styles (accent56.js, tbbtn59.js), as
 * the page's style rules give them: what each part is given by the base rules, the taskbar's style and accent colour,
 * then the buttons' own style. Probed in the page under each style and compared by same_t.sh. */
enum { TB_BAR, TB_START, TB_PIN, TB_TASK, TB_TASKON, TB_WS, TB_WSON, TB_NET, TB_QSB, TB_TRAY, TB_BAT };
typedef struct { char st[12]; int on; RGBA bg, fg, bt, bh, hi, lo, sh; int light; } TbVars;
static int tb_style_ok(const char *s) { return style_ok(s); }
/* the bar's variables (acApply) */
static TbVars tb_vars(void)
{
	TbVars v; memset(&v, 0, sizeof v); snprintf(v.st, sizeof v.st, "%s", tb_style_ok(S.tbstyle) ? S.tbstyle : "standard");
	static const char *const BASE[][2] = {{"standard", "#111827"}, {"wire", "#111827"}, {"mono", "#3f3f46"}, {"candy", "#db2777"}, {"retro", "#c0c0c0"}};
	RGB b = hexc("#111827", NULL); for (int i = 0; i < 5; i++) if (!strcmp(BASE[i][0], v.st)) b = hexc(BASE[i][1], NULL);
	if (S.tbac) b = variant(accent(), S.tbacm);
	if (!strcmp(v.st, "mono")) b = grey_of(b);
	v.on = S.tbac || strcmp(v.st, "standard"); double l = lightness(b);
	v.bg = ca(b, 1); v.light = ink_black(b); v.fg = v.light ? cx("#000000", 1) : cx("#ffffff", 1);
	v.bt = ca(shade(b, l > 50 ? -10 : 10), 1); v.bh = ca(shade(b, l > 50 ? -18 : 18), 1);
	v.hi = ca(shade(b, fmin(30, 96 - l)), 1); v.lo = ca(shade(b, -fmin(30, l - 6)), 1); v.sh = ca(shade(b, -fmin(50, l - 2)), 1);
	return v;
}
/* the buttons' own variables (tbbApply) */
typedef struct { char st[12]; int on; RGBA bg, fg, hv, hi, lo, sh; int light; } TbbVars;
static TbbVars tbb_vars(void)
{
	TbbVars v; memset(&v, 0, sizeof v); snprintf(v.st, sizeof v.st, "%s", tb_style_ok(S.tbbstyle) ? S.tbbstyle : "standard");
	v.on = strcmp(v.st, "standard") || S.tbbac; if (!v.on) return v;
	static const char *const BASE[][2] = {{"standard", "#1f2937"}, {"wire", "#111827"}, {"mono", "#52525b"}, {"candy", "#db2777"}, {"retro", "#c0c0c0"}};
	RGB b = hexc("#1f2937", NULL); for (int i = 0; i < 5; i++) if (!strcmp(BASE[i][0], v.st)) b = hexc(BASE[i][1], NULL);
	if (S.tbbac) b = variant(accent(), S.tbbacm[0] ? S.tbbacm : "normal");
	if (!strcmp(v.st, "mono")) b = grey_of(b);
	double l = lightness(b); v.bg = ca(b, 1); int blk = ink_black(b); v.fg = blk ? cx("#000000", 1) : cx("#ffffff", 1); v.light = blk && strcmp(v.st, "wire");
	v.hv = ca(shade(b, l > 50 ? -8 : 10), 1); v.hi = ca(shade(b, fmin(30, 96 - l)), 1); v.lo = ca(shade(b, -fmin(30, l - 6)), 1); v.sh = ca(shade(b, -fmin(50, l - 2)), 1);
	return v;
}
/* a part of the taskbar: K, hovered, pressed (retro's :active) */
static Box tb_box(int k, int hover, int press)
{
	TbVars V = tb_vars(); TbbVars B = tbb_vars(); RGBA ac = ca(accent(), 1), grey = cx("#d1d5db", 1);
	Box b; memset(&b, 0, sizeof b); b.fg = cx("#ffffff", 1);
	int isbtn = k != TB_BAR && k != TB_TRAY;   /* (#bar button: everything but the bar and the tray) */
	int bx = k == TB_TASK || k == TB_TASKON || k == TB_WS || k == TB_NET || k == TB_TRAY;   /* accent56's BX (#tasks button, #wsw button:not(.on), #net, #vol, #tray) */
	/* the base rules (base_v24.html, styles25.css) */
	switch (k) {
	case TB_BAR: b.bg = cx("#111827", 1); shd_add(&b, 0, -1, 0, cx("#000000", .2), 0); break;
	case TB_START: b.rad = 6; if (hover) b.bg = cx("#ffffff", .133); break;
	case TB_PIN: b.rad = 6; b.bw = 1; b.bc = hover ? cx("#a9bff5", 1) : grey; if (hover) b.bg = cx("#ffffff", .1); break;
	case TB_TASK: b.rad = 5; b.bg = cx(hover ? "#374151" : "#1f2937", 1); b.fg = cx("#e5e7eb", 1); break;
	case TB_TASKON: b.rad = 5; b.bg = ac; break;
	case TB_WS: b.rad = 4; b.bw = 1; b.bc = hover ? cx("#a9bff5", 1) : grey; b.bg = cx(hover ? "#eaf0ff" : "#1f2937", 1); b.fg = cx("#94a3b8", 1); break;
	case TB_WSON: b.rad = 4; b.bw = 1; b.bc = hover ? cx("#a9bff5", 1) : grey; b.bg = ac; break;
	case TB_NET: b.rad = 5; b.bg = cx(hover ? "#374151" : "#1f2937", 1); break;
	case TB_QSB: b.rad = 6; b.fg = cx("#e5e7eb", 1); if (hover) b.bg = cx("#ffffff", .1); break;
	case TB_TRAY: b.rad = 5; b.bg = cx("#1f2937", 1); b.fg = cx("#e5e7eb", 1); break;
	case TB_BAT: b.fg = cx("#e5e7eb", 1); break;
	}
	/* the taskbar's accent colour and style (accent56.js) */
	if (V.on) {
		if (k == TB_BAR) { b.bg = V.bg; b.fg = V.fg; }
		if (bx) { b.bg = hover ? V.bh : V.bt; b.fg = V.fg; }
		if (k != TB_BAR) b.fg = V.fg;
		if (!strcmp(V.st, "wire")) {
			if (k == TB_BAR) { b.bg = ca((RGB){V.bg.r, V.bg.g, V.bg.b}, .45); b.nsh = 0; }
			else if (k == TB_WSON) { b.bg = CLEAR; b.bw = 2; b.bc = ac; b.rad = 3; }
			else { b.bg = hover ? cmix(V.fg, .14, CLEAR) : CLEAR; b.nsh = 0; b.bw = 1; b.bc = S.tbac ? V.hi : cmix(V.fg, .55, CLEAR); b.rad = 3; }
		} else if (!strcmp(V.st, "mono")) { if (k == TB_WSON) { b.bg = V.hi; b.fg = V.bg; } }
		else if (!strcmp(V.st, "candy")) {
			if (k == TB_BAR) { b.nsh = 0; shd_add(&b, 0, 1, 0, cx("#ffffff", .5), 1); shd_add(&b, 0, -1, 3, cx("#000000", .267), 0);
				b.ng = 3; b.gc[0] = V.hi; b.gc[1] = V.bg; b.gc[2] = V.lo; b.gp[0] = 0; b.gp[1] = .48; b.gp[2] = 1; }
			else { b.rad = 999; b.bw = 0;
				if (bx) { if (!hover) { b.gl0 = .55; b.gl1 = .15; } b.nsh = 0; shd_add(&b, 0, 1, 0, cx("#ffffff", .7), 1); shd_add(&b, 0, 1, 2, cx("#000000", .333), 0); }
				if (k == TB_WSON) { b.bg = ac; b.gl0 = .65; b.gl1 = .2; b.nsh = 0; shd_add(&b, 0, 1, 0, cx("#ffffff", 1), 1); shd_add(&b, 0, 1, 2, cx("#000000", .333), 0); } }
		} else if (!strcmp(V.st, "retro")) {
			if (k == TB_BAR) { b.bg = V.bg; b.nsh = 0; shd_add(&b, 0, 1, 0, V.hi, 1); shd_add(&b, 0, 2, 0, cx("#ffffff", .533), 1); }
			else { b.rad = 0; b.bw = 0; b.nsh = 0;
				/* (the hover colours of the base rules that are !important stay: the pinned apps' and the BX parts') */
				b.bg = k == TB_PIN && hover ? cx("#ffffff", .1) : bx && hover ? V.bh : k == TB_WSON ? V.bt : V.bg;
				if (k == TB_TRAY) { shd_add(&b, 1, 1, 0, V.lo, 1); shd_add(&b, -1, -1, 0, V.hi, 1); }
				else if (press || k == TB_WSON || k == TB_TASKON) { shd_add(&b, 1, 1, 0, V.sh, 1); shd_add(&b, -1, -1, 0, V.hi, 1); shd_add(&b, 2, 2, 0, V.lo, 1); }
				else { shd_add(&b, -1, -1, 0, V.sh, 1); shd_add(&b, 1, 1, 0, V.hi, 1); shd_add(&b, -2, -2, 0, V.lo, 1); shd_add(&b, 2, 2, 0, cx("#ffffff", .4), 1); } }
		}
		if (V.light && k == TB_START) b.filt = 2;
		if (!strcmp(V.st, "mono") && k != TB_BAR) b.filt = 1;
	}
	/* the buttons' own style (tbbtn59.js): every button but the current workspace and window, the pinned apps, the tray */
	if (B.on && k != TB_BAR && k != TB_WSON && k != TB_TASKON) {
		b.bg = hover ? B.hv : B.bg; b.gl0 = 0; b.fg = B.fg;
		if (!strcmp(B.st, "wire")) { b.bg = hover ? cmix(B.bg, .25, CLEAR) : CLEAR; b.nsh = 0; b.bw = 1; b.bc = cmix(B.bg, .7, cx("#ffffff", 1)); b.rad = 3; b.fg = V.on ? V.fg : cx("#ffffff", 1);
			if (k == TB_QSB || k == TB_TRAY || k == TB_BAT || k == TB_WS) b.fg = V.on ? V.fg : k == TB_WS ? cx("#ffffff", 1) : cx("#ffffff", 1); }
		else if (!strcmp(B.st, "mono")) { b.bw = 0; b.rad = 4; b.nsh = 0; if (k == TB_PIN || k == TB_TASK || k == TB_TRAY || k == TB_NET) b.filt = 1; }
		else if (!strcmp(B.st, "candy")) { b.rad = 999; b.bw = 0; b.gl0 = hover ? .65 : .55; b.gl1 = hover ? .2 : .15; b.nsh = 0; shd_add(&b, 0, 1, 0, cx("#ffffff", .7), 1); shd_add(&b, 0, 1, 2, cx("#000000", .333), 0); }
		else if (!strcmp(B.st, "retro")) { b.rad = 0; b.bw = 0; b.nsh = 0; RGBA lo = V.on ? V.lo : B.lo;
			if (k == TB_TRAY) { shd_add(&b, 1, 1, 0, B.lo, 1); shd_add(&b, -1, -1, 0, B.hi, 1); }
			else if (press) { shd_add(&b, 1, 1, 0, B.sh, 1); shd_add(&b, -1, -1, 0, B.hi, 1); shd_add(&b, 2, 2, 0, B.lo, 1); }
			else { shd_add(&b, -1, -1, 0, B.sh, 1); shd_add(&b, 1, 1, 0, B.hi, 1); shd_add(&b, -2, -2, 0, lo, 1); shd_add(&b, 2, 2, 0, cx("#ffffff", .4), 1); } }
		if (B.light && k == TB_START) b.filt = 2;
		(void)isbtn;
	}
	return b;
}

/* ---- the tray's pictures, as the page draws them (net31.js, base_v24.html volUpd, power31.js batDraw): SVG paths */
/* an SVG arc "M x0 y0 a r r 0 0 1 dx 0" (the page's Wi-Fi and sound waves: a sweep from left to right over the top) */
static void svg_arc_lr(cairo_t *cr, double x0, double y0, double r, double dx, int horiz)
{
	double c = dx / 2, d = sqrt(fmax(0, r * r - c * c));
	if (horiz) { double cx0 = x0 + c, cy0 = y0 + d; cairo_new_sub_path(cr); cairo_arc(cr, cx0, cy0, r, atan2(-d, -c), atan2(-d, c)); }
	else { double cy0 = y0 + c, cx0 = x0 - d; cairo_new_sub_path(cr); cairo_arc(cr, cx0, cy0, r, atan2(-c, d), atan2(c, d)); }
}
static void filt_src(cairo_t *cr, RGBA c, int filt)
{
	if (filt == 1) { double l = .2126 * c.r + .7152 * c.g + .0722 * c.b; c.r = c.g = c.b = l; }
	else if (filt == 2) { c.r *= .22; c.g *= .22; c.b *= .22; }
	srca(cr, c);
}
/* (x, y: the picture's top left, 22 px drawn from a 24 px view box) */
static void ic_net(cairo_t *cr, double x, double y, int filt)
{
	cairo_save(cr); cairo_translate(cr, x, y); cairo_scale(cr, 22 / 24.0, 22 / 24.0);
	int st = NET_UP > 0 ? 2 : 0;   /* on, else off */
	RGBA col = st == 2 ? cx("#34d399", 1) : cx("#9ca3af", 1), dim = cx("#4b5563", 1);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND); cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER); cairo_set_line_width(cr, 2);
	if (NET_CELL) {   /* (1.6.0 Part 5: net31.js's four bars from the modem's signal) */
		int bars = st != 2 ? 0 : NET_SIG < 0 ? 4 : NET_SIG > 75 ? 4 : NET_SIG > 50 ? 3 : NET_SIG > 25 ? 2 : NET_SIG > 0 ? 1 : 0;
		for (int k = 0; k < 4; k++) { rrect(cr, 2 + 5 * k, 17 - 4 * k, 4, 4 + 4 * k, 1); filt_src(cr, k < bars ? col : dim, filt); cairo_fill(cr); }
	} else if (NET_WIFI) {
		int bars = st != 2 || NET_RSSI > 0 ? 3 : NET_RSSI >= -60 ? 3 : NET_RSSI >= -70 ? 2 : 1;
		static const double A[3][3] = {{8.5, 16, 5}, {5, 12.5, 10}, {2, 9, 15}}; static const double DX[3] = {7, 14, 20};
		for (int k = 0; k < 3; k++) { svg_arc_lr(cr, A[k][0], A[k][1], A[k][2], DX[k], 1); filt_src(cr, k < bars ? col : dim, filt); cairo_stroke(cr); }
		cairo_arc(cr, 12, 19.5, 1.7, 0, 2 * M_PI); filt_src(cr, col, filt); cairo_fill(cr);
	} else {   /* (base_v24.html NETIC.wired) */
		rrect(cr, 6, 3, 12, 9, 1.5); filt_src(cr, col, filt); cairo_stroke(cr);
		cairo_move_to(cr, 9, 12); cairo_line_to(cr, 9, 15); cairo_line_to(cr, 15, 15); cairo_line_to(cr, 15, 12); cairo_move_to(cr, 12, 15); cairo_line_to(cr, 12, 20);
		for (int k = 0; k < 3; k++) { cairo_move_to(cr, 9 + 3 * k, 7); cairo_line_to(cr, 9 + 3 * k, 9); } cairo_stroke(cr);
	}
	if (st == 0) { cairo_set_line_width(cr, 2.4); cairo_move_to(cr, 3, 3); cairo_line_to(cr, 21, 21); filt_src(cr, cx("#ef4444", 1), filt); cairo_stroke(cr); }
	cairo_restore(cr);
}
static void ic_vol(cairo_t *cr, double x, double y, int filt)
{
	int v = VOL < 0 ? 70 : MUTED ? 0 : VOL, n = v <= 0 ? 0 : v <= 33 ? 1 : v <= 66 ? 2 : 3;
	cairo_save(cr); cairo_translate(cr, x, y); cairo_scale(cr, 22 / 24.0, 22 / 24.0);
	cairo_move_to(cr, 3, 9); cairo_line_to(cr, 7, 9); cairo_line_to(cr, 12, 5); cairo_line_to(cr, 12, 19); cairo_line_to(cr, 7, 15); cairo_line_to(cr, 3, 15); cairo_close_path(cr);
	filt_src(cr, cx("#e5e7eb", 1), filt); cairo_fill(cr);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND); cairo_set_line_width(cr, 2);
	static const double W[3][3] = {{15.5, 9.5, 3.6}, {18, 7, 7}, {20.5, 4.5, 10.5}}; static const double DY[3] = {5, 10, 15};
	if (n) for (int k = 0; k < n; k++) { svg_arc_lr(cr, W[k][0], W[k][1], W[k][2], DY[k], 0); filt_src(cr, cx("#3b82f6", 1), filt); cairo_stroke(cr); }
	else { cairo_move_to(cr, 16, 9); cairo_line_to(cr, 21, 15); cairo_move_to(cr, 21, 9); cairo_line_to(cr, 16, 15); filt_src(cr, cx("#9ca3af", 1), filt); cairo_stroke(cr); }
	cairo_restore(cr);
}
/* (1.6.0, osd31.js osdSvg('brightness'): eight rays round the sun, as many lit as eighths of the level, at least one) */
static void ic_bri(cairo_t *cr, double x, double y, int filt, int lvl)
{
	int on = (int)ceil((lvl < 0 ? 100 : lvl) / 12.5); if (on < 1) on = 1;
	cairo_save(cr); cairo_translate(cr, x, y); cairo_scale(cr, 22 / 24.0, 22 / 24.0); cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND); cairo_set_line_width(cr, 2);
	for (int i = 0; i < 8; i++) { double a = i * G_PI / 4; cairo_move_to(cr, 12 + cos(a) * 7.5, 12 + sin(a) * 7.5); cairo_line_to(cr, 12 + cos(a) * 10.5, 12 + sin(a) * 10.5); filt_src(cr, cx(i < on ? "#fbbf24" : "#6b7280", 1), filt); cairo_stroke(cr); }
	cairo_arc(cr, 12, 12, 4.5, 0, 2 * G_PI); filt_src(cr, cx("#fbbf24", 1), filt); cairo_fill(cr); cairo_restore(cr);
}
static void ic_bat(cairo_t *cr, double x, double y, int filt)
{
	int lv = BAT < 0 ? 0 : BAT > 100 ? 100 : BAT; RGBA col = cx(lv <= 10 ? "#ef4444" : lv <= 20 ? "#f59e0b" : "#22c55e", 1); double w = round(lv / 100.0 * 15);
	cairo_save(cr); cairo_translate(cr, x, y);
	rrect(cr, 1, 1, 20, 12, 2.5); cairo_set_line_width(cr, 1.6); filt_src(cr, cx("#e5e7eb", 1), filt); cairo_stroke(cr);
	rrect(cr, 22, 4.5, 2.5, 5, 1); cairo_fill(cr);
	if (w > 0) { rrect(cr, 3.2, 3.2, w, 7.6, 1); filt_src(cr, col, filt); cairo_fill(cr); }
	if (CHARGING) { cairo_move_to(cr, 12.5, 2.2); cairo_line_to(cr, 8.6, 7.6); cairo_line_to(cr, 11.6, 7.6); cairo_line_to(cr, 10.6, 12); cairo_line_to(cr, 14.7, 6.4); cairo_line_to(cr, 11.7, 6.4); cairo_close_path(cr);
		filt_src(cr, cx("#fde047", 1), filt); cairo_fill_preserve(cr); cairo_set_line_width(cr, .6); filt_src(cr, cx("#111111", 1), filt); cairo_stroke(cr); }
	cairo_restore(cr);
}
/* a picture (the logo, an app's) at x, y, s pixels square, through the part's filter */
static void pix_at(cairo_t *cr, GdkPixbuf *p, double x, double y, double s, int filt)
{
	if (!p) return; cairo_save(cr); cairo_translate(cr, round(x), round(y)); double k = s / gdk_pixbuf_get_width(p); cairo_scale(cr, k, k);   /* (Chromium puts pictures on whole pixels) */
	if (!filt) { gdk_cairo_set_source_pixbuf(cr, p, 0, 0); cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD); cairo_paint(cr); }
	else {   /* (grayscale(1) or brightness(.22), on a copy) */
		GdkPixbuf *q = gdk_pixbuf_copy(p); int n = gdk_pixbuf_get_n_channels(q), rs = gdk_pixbuf_get_rowstride(q); guchar *px = gdk_pixbuf_get_pixels(q);
		for (int j = 0; j < gdk_pixbuf_get_height(q); j++) for (int i = 0; i < gdk_pixbuf_get_width(q); i++) { guchar *o = px + j * rs + i * n;
			if (filt == 1) { int l = (int)lround(.2126 * o[0] + .7152 * o[1] + .0722 * o[2]); o[0] = o[1] = o[2] = l > 255 ? 255 : l; } else { o[0] = o[0] * .22; o[1] = o[1] * .22; o[2] = o[2] * .22; } }
		gdk_cairo_set_source_pixbuf(cr, q, 0, 0); cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD); cairo_paint(cr); g_object_unref(q);
	}
	cairo_restore(cr);
}
static GdkPixbuf *app_pix(const App *ap) { return ap && ap->ic[0] && ICONS ? g_hash_table_lookup(ICONS, ap->ic) : NULL; }

/* ---- the taskbar: the page's flex row (base_v24.html, styles25.css), measured as Chromium lays it out */
static int HOVER = -1, PRESS = -1;          /* (indexes into HITS, as drawn last) */
static char HOV_K[12], HOV_A[80], PRS_K[12], PRS_A[80];   /* the hovered and pressed parts, by kind and argument */
static char CLOCK_T[32], CLOCK_D[32];
static void clock_text(void)
{
	GTimeZone *tz = S.tz[0] ? g_time_zone_new_identifier(S.tz) : NULL; if (!tz) tz = g_time_zone_new_local();
	GDateTime *n0 = g_date_time_new_now(tz), *n = g_date_time_add_seconds(n0, S.doff / 1000.0);
	int h = g_date_time_get_hour(n), m = g_date_time_get_minute(n);
	if (S.h24) snprintf(CLOCK_T, sizeof CLOCK_T, "%02d:%02d", h, m); else snprintf(CLOCK_T, sizeof CLOCK_T, "%d:%02d %s", h % 12 ? h % 12 : 12, m, h < 12 ? "AM" : "PM");
	int Y = g_date_time_get_year(n), M = g_date_time_get_month(n), D = g_date_time_get_day_of_month(n);
	if (!strcmp(S.df, "dmy")) snprintf(CLOCK_D, sizeof CLOCK_D, "%02d/%02d/%04d", D, M, Y);
	else if (!strcmp(S.df, "ymd")) snprintf(CLOCK_D, sizeof CLOCK_D, "%04d/%02d/%02d", Y, M, D);
	else snprintf(CLOCK_D, sizeof CLOCK_D, "%02d/%02d/%04d", M, D, Y);
	g_date_time_unref(n); g_date_time_unref(n0); g_time_zone_unref(tz);
}
/* (the taskbar's places, in the page's units: kept in the screen's pixels) */
static void hit_add(double x, double y, double w, double h, const char *k, const char *a) { if (NHITS >= 96) return; Hit *t = &HITS[NHITS++]; t->x = (int)lround(x * Z); t->y = (int)lround(y * Z); t->w = (int)lround(w * Z); t->h = (int)lround(h * Z); snprintf(t->kind, sizeof t->kind, "%s", k); snprintf(t->arg, sizeof t->arg, "%s", a ? a : ""); }
/* the bar's own height (46 px, 47 with Wireframe's top edge) and where its middle is (from the bar's top) */
static double bar_mid(void) { return 23; }
/* the bar: TOP: its window is at the top; W, H: its size in the page's units; the hovered and pressed parts by kind */
/* ---- (1.6.1, docs/plan-1.6.1.md Part 25) a minimized window's work in progress on its taskbar button, as Standard draws it (tbprog61.js):
   the page says a percentage, busy or nothing (op "prog"; Oracle's in Chromium through rudolf-launch, by its window's place). The fill:
   3 px inside the button's edge, 3 px round corners, behind the picture and the title, the colour at half (high contrast: 65 %); only
   while the window is minimized. A percentage moves to its new width in 0.35 s (CSS's transition, linear); busy creeps towards 90 %
   (90 × (1 − e^(−t / 6 s))); the end goes to 100 % in 0.35 s, stays, and fades from 0.3 s to 0.8 s. */
#define TBP_MOVE 350.0
#define TBP_FULL 300.0
#define TBP_FADE 500.0
static guint TBP_T;
static RGB tbp_colour(void)
{
	static const char *const P[][2] = { {"green", "#16a34a"}, {"blue", "#2563eb"}, {"orange", "#ea580c"}, {"purple", "#9333ea"}, {"red", "#dc2626"} };
	const char *v = S.tbpcol[0] ? S.tbpcol : "green";
	if (!strcmp(v, "accent")) return accent();
	for (size_t i = 0; i < sizeof P / sizeof P[0]; i++) if (!strcmp(v, P[i][0])) return hexc(P[i][1], "#16a34a");
	if (v[0] == '#' && strlen(v) == 7 && strspn(v + 1, "0123456789abcdefABCDEF") == 6) return hexc(v, "#16a34a");
	return hexc("#16a34a", "#16a34a");
}
/* its width now (0-100; -1: nothing) and how opaque (0-1) */
static double tbp_frac(Client *c, gint64 now, double *alpha)
{
	if (alpha) *alpha = 1;
	if (c->pg == 1) { double dt = (now - c->pgch) / 1000.0; return S.rmotion || dt >= TBP_MOVE ? c->pgt : c->pgf + (c->pgt - c->pgf) * dt / TBP_MOVE; }
	if (c->pg == 2) return 90.0 * (1 - exp(-((now - c->pgb0) / 1000.0) / 6000.0));
	if (c->pg == 3) { double dt = (now - c->pgend) / 1000.0; if (dt >= TBP_FULL + TBP_FADE) return -1;
		if (alpha) *alpha = dt < TBP_FULL ? 1 : 1 - (dt - TBP_FULL) / TBP_FADE;
		return S.rmotion || dt >= TBP_MOVE ? 100 : c->pgf + (100 - c->pgf) * dt / TBP_MOVE; }
	return -1;
}
static gboolean tbp_tick(gpointer d)
{
	(void)d; gint64 now = g_get_monotonic_time(); int any = 0, draw = 0;
	for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (!c->pg) continue;
		if (c->pg == 3 && (!c->min || tbp_frac(c, now, NULL) < 0)) { c->pg = 0; draw |= c->min; state_write(); continue; }   /* (ended: gone; restored meanwhile: at once, as the page's) */
		any = 1; if (c->min) draw = 1; }
	if (draw) bar_redraw();
	if (!any) { TBP_T = 0; return G_SOURCE_REMOVE; }
	return G_SOURCE_CONTINUE;
}
static void tbp_set(Client *c, const char *v)
{
	gint64 now = g_get_monotonic_time(); double cur = tbp_frac(c, now, NULL); int was = c->pg;
	if (v && !strcmp(v, "busy")) { if (c->pg != 2) { c->pg = 2; c->pgb0 = now; } }
	else if (v && v[0] && strlen(v) < 4 && strspn(v, "0123456789") == strlen(v)) { double t = fmin(100, atoi(v));
		if (c->pg == 1 && t == c->pgt) return;
		c->pgf = (was == 1 || was == 2) && cur >= 0 ? cur : t; c->pgt = t; c->pgch = now; c->pg = 1; }
	else { if (c->pg != 1 && c->pg != 2) return; c->pgf = cur >= 0 ? cur : 0; c->pgend = now; c->pg = 3; }
	if (!TBP_T) TBP_T = g_timeout_add(100, tbp_tick, NULL);
	if (c->min) bar_redraw(); if (was != c->pg) state_write();
}
static void tbp_app(AppWin *a, const char *v) { Client *c = WM_OK ? client_of_app(a) : NULL; if (c) tbp_set(c, v); }
/* the fill in a window's button (x, y, w, h: the button; b: its box, for its border) */
static void tbp_paint(cairo_t *cr, Client *c, double x, double y, double w, double h, const Box *b)
{
	if (!c || !c->min || !c->pg) return; double al; double f = tbp_frac(c, g_get_monotonic_time(), &al); if (f < 0 || al <= 0) return;
	snap(&x, &y, &w, &h);   /* (the button where box_paint puts it: whole pixels, as Chromium places the page's) */
	double iw = (w - 2 * b->bw - 6) * fmin(100, f) / 100, ih = h - 2 * b->bw - 6; if (iw <= 0 || ih <= 0) return;
	cairo_save(cr); rrect(cr, x + b->bw + 3, y + b->bw + 3, iw, ih, 3); src(cr, tbp_colour(), (S.hc ? .65 : .5) * al); cairo_fill(cr); cairo_restore(cr);
}
static void bar_paint_v(cairo_t *cr, double W, double H);
static void bar_paint(cairo_t *cr, double W, double H, int top)
{
	(void)H; NHITS = 0; double mid = bar_mid(); int hv = HOVER, pr = PRESS;
	Box bar = tb_box(TB_BAR, 0, 0);
	/* (Wireframe's see-through bar shows the wallpaper behind, blurred: the window's own copy of it) */
	if (bar.bg.a < 1 && WALL) { cairo_save(cr); cairo_scale(cr, 1 / Z, 1 / Z); cairo_set_source_surface(cr, WALL, -bar_x(), -bar_y()); cairo_paint(cr); cairo_restore(cr); }
	Box bar2 = bar; bar2.nsh = 0; for (int i = 0; i < bar.nsh; i++) if (bar.sh[i].inset) bar2.sh[bar2.nsh++] = bar.sh[i];
	box_paint(cr, 0, 0, W, 46, &bar2);
	if (!strcmp(tb_vars().st, "wire") && tb_vars().on && top) { srca(cr, cmix(tb_vars().fg, .7, CLEAR)); cairo_rectangle(cr, 0, 45, W, 1); cairo_fill(cr); }
	/* which part is which: the hovered one by its index in HITS, as drawn last time (the same order each time) */
	(void)hv; (void)pr;
	#define HV(k, a) (HOV_BAR == MONK_CUR && !strcmp(HOV_K, k) && !strcmp(HOV_A, a))
	#define PR(k, a) (PRS_BAR == MONK_CUR && !strcmp(PRS_K, k) && !strcmp(PRS_A, a))
	int i = 0; double x = 3;
	/* the start button: the logo and the version (#start: padding 1px 10px 1px 4px, a 7 px gap; .stv 600 12px system-ui) */
	{ Box b = tb_box(TB_START, HV("start", ""), PR("start", "")); char sl[64]; snprintf(sl, sizeof sl, "Rudolf OS%s%s", VER[0] ? " " : "", VER);
		double tw = css_width(cr, sl, SYSUI, 600, 12), w = b.bw * 2 + 4 + 30 + 7 + tw + 10, y = mid - 17;
		box_paint(cr, x, y, w, 34, &b); hit_add(x, y, w, 34, "start", NULL);
		pix_at(cr, LOGO30 ? LOGO30 : LOGO, x + b.bw + 4, mid - 15, 30, b.filt);
		css_text(cr, sl, SYSUI, 600, 12, x + b.bw + 4 + 30 + 7, mid - 6, 12, (RGBA){b.fg.r, b.fg.g, b.fg.b, .9}, 0);
		x += w + 4; i++; }
	/* the pinned apps: 34 px buttons 2 px apart, their pictures at 24 px */
	if (S.npins) { for (int p = 0; p < S.npins; p++, i++) {
			Box b = tb_box(TB_PIN, HV("pin", S.pins[p]), PR("pin", S.pins[p])); double y = mid - 17; box_paint(cr, x, y, 34, 34, &b); hit_add(x, y, 34, 34, "pin", S.pins[p]);
			const App *ap = app_find(S.pins[p]); GdkPixbuf *px = app_pix(ap);
			if (px) pix_at(cr, px, x + 5, mid - 12, 24, b.filt); else css_text(cr, ap ? ap->icon : "▫", UIFAM, 400, 18, x + 5, y, 34, b.fg, 0);
			x += 34 + (p + 1 < S.npins ? 2 : 0); }
		x += 4; }
	double tasks_x = x;
	/* from the right: the clock (#tray), the notifications (#ncb), quick settings (#qsb), the sound (#vol), the network
	   (#net), the workspaces (#wsw), the battery (#bat) */
	double rx = W - 3; int ri = 64;   /* (the right side's parts: hover numbers from 64) */
	{ Box b = tb_box(TB_TRAY, HV("clock", ""), 0); clock_text();
		double t1 = css_width(cr, CLOCK_T, UIFAM, 400, 13), t2 = css_width(cr, CLOCK_D, UIFAM, 400, 13 * .85), w = b.bw * 2 + 10 + t1 + 8 + t2 + 10, h = b.bw * 2 + 32;
		rx -= w; double y = mid - h / 2; box_paint(cr, rx, y, w, h, &b); hit_add(rx, y, w, h, "clock", NULL);
		double tx = rx + b.bw + 10, ty = y + b.bw + 3; CLKD[0] = tx + t1 + 8; CLKD[1] = CLKD[0] + t2;
		css_text(cr, CLOCK_T, UIFAM, 400, 13, tx, ty, 26, b.fg, 0);
		/* (the date: .85em, on the same baseline) */
		double d1, a1 = css_ascent(cr, UIFAM, 400, 13, &d1), base = ty + floor((26 - (a1 + d1)) / 2) + a1;
		double a2, d2; a2 = css_ascent(cr, UIFAM, 400, 13 * .85, &d2);
		css_text(cr, CLOCK_D, UIFAM, 400, 13 * .85, tx + t1 + 8, base - a2 - (0), a2 + d2, (RGBA){b.fg.r, b.fg.g, b.fg.b, .78}, 0);
		rx -= 4; ri++; }
	static const char *const QK[2] = {"notes", "qs"}; const char *QT[2] = {S.dnd ? "🔕" : "🔔", "☰"};   /* (do not disturb: 🔕, shell25.js ncBadge) */
	for (int q = 0; q < 2 && !MONK_CUR; q++, ri++) {
		double qh = S.touch ? 40 : 30;   /* (touch mode: at least 40 px) */
		Box b = tb_box(TB_QSB, HV(QK[q], ""), PR(QK[q], "")); double tw = css_width(cr, QT[q], UIFAM, 400, 15), w = fmax(30, 12 + tw + 2 * b.bw), y = mid - qh / 2;
		rx -= w; box_paint(cr, rx, y, w, qh, &b); hit_add(rx, y, w, qh, QK[q], NULL);
		css_text(cr, QT[q], UIFAM, 400, 15, rx + (w - tw) / 2, y + b.bw, qh - 2 * b.bw, b.fg, 0);
		if (q == 0 && nt_unread() > 0) {   /* (#ncb's count, the page's ncUnread: those come since signing in or since the centre was opened; a red badge at its top right) */
			char n[16]; snprintf(n, sizeof n, "%d", nt_unread()); double nw = css_width(cr, n, UIFAM, 400, 9) + 8; Box bd; memset(&bd, 0, sizeof bd); bd.bg = cx("#ef4444", 1); bd.rad = 7;
			box_paint(cr, rx + w - nw, y + 1, nw, 13, &bd); css_text(cr, n, UIFAM, 400, 9, rx + w - nw + 4, y + 1, 13, cx("#ffffff", 1), 0); }
		rx -= 4;
	}
	/* (the apps' tray buttons before quick settings, .tapp: "In a call", 🎙️, or 🔇 with the microphone muted: call59.js) */
	if (MIC_N > 0 && !MONK_CUR) { Box b = tb_box(TB_QSB, HV("call", ""), PR("call", "")); const char *ic = MIC_MUTED ? "🔇" : "🎙️"; double tw = css_width(cr, ic, UIFAM, 400, 15), w = fmax(30, 12 + tw + 2 * b.bw), y = mid - 15;
		rx -= w; box_paint(cr, rx, y, w, 30, &b); hit_add(rx, y, w, 30, "call", NULL); css_text(cr, ic, UIFAM, 400, 15, rx + (w - tw) / 2, y + b.bw, 30 - 2 * b.bw, cx(MIC_MUTED ? "#b91c1c" : "#16a34a", 1), 0); rx -= 4; ri++; }
	/* (1.6.0) the on-screen keyboard's ⌨ while its switch is on (#vkt: 14 px, 0 4px, no background), and the brightness (#bri: as the
	   sound's button, with the brightness's picture) when the screen has a backlight (osd31.js) */
	if (S.vk && !MONK_CUR) { Box b = tb_box(TB_QSB, HV("vkt", ""), PR("vkt", "")); b.bg = CLEAR; b.bw = 0; b.nsh = 0; double tw = css_width(cr, "⌨", UIFAM, 400, 14), w = tw + 8, h = fl_lh(cr, 400, 14) + 2, y = mid - h / 2;
		rx -= w; hit_add(rx, y, w, h, "vkt", NULL); VKTB[0] = rx; VKTB[1] = y; VKTB[2] = w; VKTB[3] = h; css_text(cr, "⌨", UIFAM, 400, 14, rx + 4, y + 1, h - 2, b.fg, 0); rx -= 4; ri++; }
	/* (touch mode: styles25.css html.touch button{min-height:40px}, the tray's buttons at least 40 px tall) */
	const double th26 = S.touch ? 40 : 26;
	if (BRI_HAS && !MONK_CUR) { Box b = tb_box(TB_NET, 0, PR("bri", "")); double w = 36 + 2 * b.bw, y = mid - th26 / 2; rx -= w; box_paint(cr, rx, y, w, th26, &b); hit_add(rx, y, w, th26, "bri", NULL); BRIB[0] = rx; BRIB[1] = y; BRIB[2] = w; BRIB[3] = th26;
		ic_bri(cr, rx + b.bw + 7, y + (th26 - 22) / 2, b.filt, BRI); rx -= 4; ri++; }
	if (!MONK_CUR) { Box b = tb_box(TB_NET, HV("vol", ""), PR("vol", "")); double w = 36 + 2 * b.bw, y = mid - th26 / 2; rx -= w; box_paint(cr, rx, y, w, th26, &b); hit_add(rx, y, w, th26, "vol", NULL); VOLB[0] = rx; VOLB[1] = y; VOLB[2] = w; VOLB[3] = th26;
		ic_vol(cr, rx + b.bw + 7, y + (th26 - 22) / 2, b.filt); rx -= 4; ri++; }
	if (!MONK_CUR) { Box b = tb_box(TB_NET, HV("net", ""), PR("net", "")); double w = 34 + 2 * b.bw, y = mid - th26 / 2; rx -= w; box_paint(cr, rx, y, w, th26, &b); hit_add(rx, y, w, th26, "net", NULL); NETB[0] = rx; NETB[1] = y; NETB[2] = w; NETB[3] = th26;
		ic_net(cr, rx + b.bw + 6, y + (th26 - 22) / 2, b.filt); rx -= 4; ri++; }
	/* the workspaces (as the page's: the first is the one shown; the native shell has one until item 4) */
	rx -= 4 + 4 * 22 + 3 * 2;
	{ static const char *const WT[4] = {"1", "2", "3", "▦"}; double wx = rx;
		for (int k = 0; k < 4; k++, ri++) { Box b = tb_box(k + 1 == BARWS ? TB_WSON : TB_WS, HV("ws", WT[k]), PR("ws", WT[k])); double y = mid - 11;
			if (k < 3 && k + 1 != BARWS && ws_has(k + 1) && !tb_vars().on) b.fg = cx("#e5e7eb", 1);   /* (#wsw button.has: windows there) */
			box_paint(cr, wx, y, 22, 22, &b); hit_add(wx, y, 22, 22, "ws", WT[k]);
			double tw = css_width(cr, WT[k], UIFAM, 400, 11); css_text(cr, WT[k], UIFAM, 400, 11, wx + (22 - tw) / 2, y + b.bw, 22 - 2 * b.bw, b.fg, 0); wx += 24; } }
	rx -= 4 + 4;
	if (BAT >= 0 && !MONK_CUR) { Box b = tb_box(TB_BAT, HV("bat", ""), PR("bat", "")); char t[16]; snprintf(t, sizeof t, "%d%%", BAT); double tw = css_width(cr, t, SYSUI, 600, 12), w = 2 * b.bw + 6 + 26 + 3 + tw + 6, y = mid - 13;
		rx -= w; box_paint(cr, rx, y, w, 26, &b); hit_add(rx, y, w, 26, "bat", NULL);
		ic_bat(cr, rx + b.bw + 6, mid - 7, b.filt); css_text(cr, t, SYSUI, 600, 12, rx + b.bw + 6 + 26 + 3, y + b.bw, 26 - 2 * b.bw, b.fg, 0); rx -= 4; ri++; }
	/* the open windows (#tasks: buttons of at most 150 px, 3 px apart, their picture at 18 px and the title) */
	GList *TL = NULL; if (WM_OK) { for (GList *l = CLIENTS; l; l = l->next) if (!((Client *)l->data)->trans && ((Client *)l->data)->ws == BARWS) TL = g_list_append(TL, l->data); } else TL = g_list_copy(WINS);   /* (the workspace's own: item 4) */
	int n = g_list_length(TL), j = 0; double avail = rx - tasks_x, tw = n ? fmin(150, (avail - 3 * (n - 1)) / n) : 0; i = 200;
	for (GList *l = TL; l && tw >= 24; l = l->next, j++, i++) {
		Client *c = WM_OK ? l->data : NULL; AppWin *a = c ? c->app : l->data; int act = c ? c == FOCUS && !c->min : a == ACTIVE;
		char wid[24]; snprintf(wid, sizeof wid, "%lx", c ? (unsigned long)c->w : (unsigned long)(uintptr_t)a);
		double wx = tasks_x + j * (tw + 3), y = mid - 17; Box b = tb_box(act ? TB_TASKON : TB_TASK, HV("win", wid), PR("win", wid));
		box_paint(cr, wx, y, tw, 34, &b); hit_add(wx, y, tw, 34, "win", wid);
		tbp_paint(cr, c, wx, y, tw, 34, &b);   /* (1.6.1 Part 25: its work in progress, while minimized) */
		if (c && c->w == LP_LITW && LP_LITA > 0) { cairo_save(cr); cairo_set_source_rgba(cr, LP_CORE.r, LP_CORE.g, LP_CORE.b, .55 * LP_LITA); cairo_rectangle(cr, wx, y, tw, 34); cairo_fill(cr); cairo_restore(cr); }   /* (Part 17: lit as the point lands) */
		/* the line: the picture (vertical-align -4px) and the title, centred in the button's content (34 less 1 px above and below) */
		double d, A = css_ascent(cr, UIFAM, act ? 600 : 400, 13, &d), lt = fmin(-A, -14), lb = fmax(d, 4), base = y + b.bw + 1 + (32 - 2 * b.bw - (lb - lt)) / 2 - lt;
		const App *ap = a ? app_find(a->k) : c && c->ak[0] ? app_find(c->ak) : NULL; GdkPixbuf *px = a && a->pix ? a->pix : app_pix(ap); double tx = wx + b.bw + 6;
		cairo_save(cr); cairo_rectangle(cr, wx + b.bw + 6, y, tw - 2 * b.bw - 12, 34); cairo_clip(cr);
		if (px) { pix_at(cr, px, tx, base + 4 - 18, 18, b.filt); tx += 18; }
		const char *ti = a ? a->title : c->title, *ico = a ? a->icon : ap ? ap->icon : ""; char *tt = px ? g_strdup_printf(" %s", ti) : g_strdup_printf("%s%s%s", ico, ico[0] ? " " : "", ti);
		css_text(cr, tt, UIFAM, act ? 600 : 400, 13, tx, base - A, A + d, b.fg, wx + tw - b.bw - 6 - tx); g_free(tt);
		cairo_restore(cr);
	}
	g_list_free(TL);
	#undef HV
	#undef PR
}
/* ---- (1.6.0 Part 12, item 3's last part) the taskbar at the left or the right (base_v24.html body.left/.right #bar: a column,
   3 px of padding, its parts 4 px apart and as wide as the column (align-items: stretch), the column as wide as its widest
   part (width: max-content); the start button the logo over the version (styles25.css: 10 px, centred), the pinned apps in
   a row, the open windows' buttons 22 px high one under the other, then at the bottom the battery, the workspaces (4 px in
   from each side), the network, the sound, the call's 🎙️, quick settings, the notifications and the clock (its date hidden) */
static double bar_v_cw(cairo_t *cr)
{
	char sl[64]; snprintf(sl, sizeof sl, "Rudolf OS%s%s", VER[0] ? " " : "", VER); clock_text();
	Box bb = tb_box(TB_BAT, 0, 0), bn = tb_box(TB_NET, 0, 0), bq = tb_box(TB_QSB, 0, 0), bs = tb_box(TB_START, 0, 0), bt = tb_box(TB_TRAY, 0, 0);
	double w = fmax(30, css_width(cr, sl, SYSUI, 600, 10)) + 8 + 2 * bs.bw;
	if (S.npins) w = fmax(w, S.npins * 34 + (S.npins - 1) * 2);
	if (BAT >= 0) { char t[16]; snprintf(t, sizeof t, "%d%%", BAT); w = fmax(w, 2 * bb.bw + 6 + 26 + 3 + css_width(cr, t, SYSUI, 600, 12) + 6); }
	w = fmax(w, 4 + 4 * 22 + 3 * 2 + 4); w = fmax(w, 22 + 12 + 2 * bn.bw); w = fmax(w, 22 + 14 + 2 * bn.bw);
	w = fmax(w, fmax(30, 12 + css_width(cr, "🔔", UIFAM, 400, 15) + 2 * bq.bw));
	w = fmax(w, 2 * bt.bw + 20 + css_width(cr, CLOCK_T, UIFAM, 400, 13));
	return ceil(w);
}
static void bar_measure(void)
{
	if (!tb_side()) { BARW = 0; return; }
	cairo_surface_t *t = cairo_image_surface_create(CAIRO_FORMAT_A8, 1, 1); cairo_t *c = cairo_create(t); double cw = bar_v_cw(c); cairo_destroy(c); cairo_surface_destroy(t);
	BARW = (int)lround((cw + 6) * Z);
}
static void bar_paint_v(cairo_t *cr, double W, double H)
{
	NHITS = 0; Box bar = tb_box(TB_BAR, 0, 0);
	if (bar.bg.a < 1 && WALL) { cairo_save(cr); cairo_scale(cr, 1 / Z, 1 / Z); cairo_set_source_surface(cr, WALL, -bar_x(), -bar_y()); cairo_paint(cr); cairo_restore(cr); }
	Box bar2 = bar; bar2.nsh = 0; for (int i = 0; i < bar.nsh; i++) if (bar.sh[i].inset) bar2.sh[bar2.nsh++] = bar.sh[i];
	box_paint(cr, 0, 0, W, H, &bar2);
	#define HV(k, a) (HOV_BAR == MONK_CUR && !strcmp(HOV_K, k) && !strcmp(HOV_A, a))
	#define PR(k, a) (PRS_BAR == MONK_CUR && !strcmp(PRS_K, k) && !strcmp(PRS_A, a))
	double x = 3, cw = W - 6, y = 3;
	/* the start button (column: padding 4px 4px 5px, the logo, 3 px, .stv 10px/1.15, centred) */
	{ Box b = tb_box(TB_START, HV("start", ""), PR("start", "")); char sl[64]; snprintf(sl, sizeof sl, "Rudolf OS%s%s", VER[0] ? " " : "", VER);
		double lh = 11.5, h = 4 + 30 + 3 + lh + 5 + 2 * b.bw, tw = css_width(cr, sl, SYSUI, 600, 10);
		box_paint(cr, x, y, cw, h, &b); hit_add(x, y, cw, h, "start", NULL);
		pix_at(cr, LOGO30 ? LOGO30 : LOGO, x + (cw - 30) / 2, y + b.bw + 4, 30, b.filt);
		css_text(cr, sl, SYSUI, 600, 10, x + (cw - tw) / 2, y + b.bw + 4 + 30 + 3, lh, (RGBA){b.fg.r, b.fg.g, b.fg.b, .9}, 0);
		y += h + 4; }
	/* the pinned apps (a row, 2 px apart) */
	if (S.npins) { double px0 = x; for (int p = 0; p < S.npins; p++) {
			Box b = tb_box(TB_PIN, HV("pin", S.pins[p]), PR("pin", S.pins[p])); box_paint(cr, px0, y, 34, 34, &b); hit_add(px0, y, 34, 34, "pin", S.pins[p]);
			const App *ap = app_find(S.pins[p]); GdkPixbuf *px = app_pix(ap);
			if (px) pix_at(cr, px, px0 + 5, y + 5, 24, b.filt); else css_text(cr, ap ? ap->icon : "▫", UIFAM, 400, 18, px0 + 5, y, 34, b.fg, 0);
			px0 += 36; }
		y += 34 + 4; }
	double tasks_y = y;
	/* from the bottom: the clock (#tray: 3px 10px, 26 px, its date hidden), the notifications, quick settings, the call, the sound,
	   the network, the workspaces, the battery */
	double by = H - 3;
	{ Box b = tb_box(TB_TRAY, HV("clock", ""), 0); clock_text(); double h = 2 * b.bw + 32; by -= h;
		box_paint(cr, x, by, cw, h, &b); hit_add(x, by, cw, h, "clock", NULL); CLKD[0] = CLKD[1] = -1;
		css_text(cr, CLOCK_T, UIFAM, 400, 13, x + b.bw + 10, by + b.bw + 3, 26, b.fg, cw - 2 * b.bw - 20); by -= 4; }
	static const char *const QK[2] = {"notes", "qs"}; const char *QT[2] = {S.dnd ? "🔕" : "🔔", "☰"};
	for (int q = 0; q < 2 && !MONK_CUR; q++) {
		Box b = tb_box(TB_QSB, HV(QK[q], ""), PR(QK[q], "")); double tw = css_width(cr, QT[q], UIFAM, 400, 15); by -= 30;
		box_paint(cr, x, by, cw, 30, &b); hit_add(x, by, cw, 30, QK[q], NULL); css_text(cr, QT[q], UIFAM, 400, 15, x + (cw - tw) / 2, by + b.bw, 30 - 2 * b.bw, b.fg, 0);
		if (q == 0 && nt_unread() > 0) { char n[16]; snprintf(n, sizeof n, "%d", nt_unread()); double nw = css_width(cr, n, UIFAM, 400, 9) + 8; Box bd; memset(&bd, 0, sizeof bd); bd.bg = cx("#ef4444", 1); bd.rad = 7;
			box_paint(cr, x + cw - nw, by + 1, nw, 13, &bd); css_text(cr, n, UIFAM, 400, 9, x + cw - nw + 4, by + 1, 13, cx("#ffffff", 1), 0); }
		by -= 4; }
	if (MIC_N > 0 && !MONK_CUR) { Box b = tb_box(TB_QSB, HV("call", ""), PR("call", "")); const char *ic = MIC_MUTED ? "🔇" : "🎙️"; double tw = css_width(cr, ic, UIFAM, 400, 15); by -= 30;
		box_paint(cr, x, by, cw, 30, &b); hit_add(x, by, cw, 30, "call", NULL); css_text(cr, ic, UIFAM, 400, 15, x + (cw - tw) / 2, by + b.bw, 30 - 2 * b.bw, cx(MIC_MUTED ? "#b91c1c" : "#16a34a", 1), 0); by -= 4; }
	if (S.vk && !MONK_CUR) { Box b = tb_box(TB_QSB, HV("vkt", ""), PR("vkt", "")); double tw = css_width(cr, "⌨", UIFAM, 400, 14), h = fl_lh(cr, 400, 14) + 2; by -= h;
		hit_add(x, by, cw, h, "vkt", NULL); VKTB[0] = x; VKTB[1] = by; VKTB[2] = cw; VKTB[3] = h; css_text(cr, "⌨", UIFAM, 400, 14, x + (cw - tw) / 2, by + 1, h - 2, b.fg, 0); by -= 4; }
	if (BRI_HAS && !MONK_CUR) { Box b = tb_box(TB_NET, 0, PR("bri", "")); by -= 26; box_paint(cr, x, by, cw, 26, &b); hit_add(x, by, cw, 26, "bri", NULL); BRIB[0] = x; BRIB[1] = by; BRIB[2] = cw; BRIB[3] = 26;
		ic_bri(cr, x + (cw - 22) / 2, by + 2, b.filt, BRI); by -= 4; }
	if (!MONK_CUR) { Box b = tb_box(TB_NET, HV("vol", ""), PR("vol", "")); by -= 26; box_paint(cr, x, by, cw, 26, &b); hit_add(x, by, cw, 26, "vol", NULL); VOLB[0] = x; VOLB[1] = by; VOLB[2] = cw; VOLB[3] = 26;
		ic_vol(cr, x + (cw - 22) / 2, by + 2, b.filt); by -= 4; }
	if (!MONK_CUR) { Box b = tb_box(TB_NET, HV("net", ""), PR("net", "")); by -= 26; box_paint(cr, x, by, cw, 26, &b); hit_add(x, by, cw, 26, "net", NULL); NETB[0] = x; NETB[1] = by; NETB[2] = cw; NETB[3] = 26;
		ic_net(cr, x + (cw - 22) / 2, by + 2, b.filt); by -= 4; }
	{ static const char *const WT[4] = {"1", "2", "3", "▦"}; by -= 22; double wx = x + 4;
		for (int k = 0; k < 4; k++) { Box b = tb_box(k + 1 == BARWS ? TB_WSON : TB_WS, HV("ws", WT[k]), PR("ws", WT[k]));
			if (k < 3 && k + 1 != BARWS && ws_has(k + 1) && !tb_vars().on) b.fg = cx("#e5e7eb", 1);   /* (#wsw button.has) */
			box_paint(cr, wx, by, 22, 22, &b); hit_add(wx, by, 22, 22, "ws", WT[k]);
			double tw = css_width(cr, WT[k], UIFAM, 400, 11); css_text(cr, WT[k], UIFAM, 400, 11, wx + (22 - tw) / 2, by + b.bw, 22 - 2 * b.bw, b.fg, 0); wx += 24; }
		by -= 4; }
	if (BAT >= 0 && !MONK_CUR) { Box b = tb_box(TB_BAT, HV("bat", ""), PR("bat", "")); char t[16]; snprintf(t, sizeof t, "%d%%", BAT); by -= 26;
		box_paint(cr, x, by, cw, 26, &b); hit_add(x, by, cw, 26, "bat", NULL);
		ic_bat(cr, x + b.bw + 6, by + 6, b.filt); css_text(cr, t, SYSUI, 600, 12, x + b.bw + 6 + 26 + 3, by + b.bw, 26 - 2 * b.bw, b.fg, 0); by -= 4; }
	/* the open windows (#tasks: a column of 22 px buttons, 3 px apart; padding 1px 6px, the picture at 18 px and the title) */
	GList *TL = NULL; if (WM_OK) { for (GList *l = CLIENTS; l; l = l->next) if (!((Client *)l->data)->trans && ((Client *)l->data)->ws == BARWS) TL = g_list_append(TL, l->data); } else TL = g_list_copy(WINS);   /* (the workspace's own: item 4) */
	double ty = tasks_y;
	for (GList *l = TL; l && ty + 22 <= by + 4; l = l->next) {
		Client *c = WM_OK ? l->data : NULL; AppWin *a = c ? c->app : l->data; int act = c ? c == FOCUS && !c->min : a == ACTIVE;
		char wid[24]; snprintf(wid, sizeof wid, "%lx", c ? (unsigned long)c->w : (unsigned long)(uintptr_t)a);
		Box b = tb_box(act ? TB_TASKON : TB_TASK, HV("win", wid), PR("win", wid)); box_paint(cr, x, ty, cw, 22, &b); hit_add(x, ty, cw, 22, "win", wid);
		tbp_paint(cr, c, x, ty, cw, 22, &b);   /* (1.6.1 Part 25) */
		if (c && c->w == LP_LITW && LP_LITA > 0) { cairo_save(cr); cairo_set_source_rgba(cr, LP_CORE.r, LP_CORE.g, LP_CORE.b, .55 * LP_LITA); cairo_rectangle(cr, x, ty, cw, 22); cairo_fill(cr); cairo_restore(cr); }
		double d, A = css_ascent(cr, UIFAM, act ? 600 : 400, 13, &d), lt = fmin(-A, -14), lb = fmax(d, 4), base = ty + b.bw + 1 + (20 - 2 * b.bw - (lb - lt)) / 2 - lt;
		const App *ap = a ? app_find(a->k) : c && c->ak[0] ? app_find(c->ak) : NULL; GdkPixbuf *px = a && a->pix ? a->pix : app_pix(ap); double tx = x + b.bw + 6;
		cairo_save(cr); cairo_rectangle(cr, x + b.bw + 6, ty, cw - 2 * b.bw - 12, 22); cairo_clip(cr);
		if (px) { pix_at(cr, px, tx, base + 4 - 18, 18, b.filt); tx += 18; }
		const char *ti = a ? a->title : c->title, *ico = a ? a->icon : ap ? ap->icon : ""; char *tt = px ? g_strdup_printf(" %s", ti) : g_strdup_printf("%s%s%s", ico, ico[0] ? " " : "", ti);
		css_text(cr, tt, UIFAM, act ? 600 : 400, 13, tx, base - A, A + d, b.fg, x + cw - b.bw - 6 - tx); g_free(tt);
		cairo_restore(cr); ty += 22 + 3;
	}
	g_list_free(TL);
	#undef HV
	#undef PR
}
static gboolean bar_draw(GtkWidget *wd, cairo_t *cr, gpointer d)
{
	(void)d; int top = !strcmp(S.tb, "top");
	double W = gtk_widget_get_allocated_width(wd) / Z, H = gtk_widget_get_allocated_height(wd) / Z;
	cairo_scale(cr, Z, Z); if (tb_side()) bar_paint_v(cr, W, H); else bar_paint(cr, W, H, top); state_write();
	return TRUE;
}
/* the bar's shadow and Wireframe's top edge, which fall outside its window: drawn on the desktop (desk_area_draw) */
static void bar_outside(cairo_t *cr)
{
	if (!USER_ID[0] || LOCKED || !strcmp(S.tb, "top") || tb_side()) return;   /* (at the left or the right: its shadow, 1 px up, is off the screen) */
	Box b = tb_box(TB_BAR, 0, 0); TbVars V = tb_vars(); double y = (SH - BARH) / Z;
	cairo_save(cr); cairo_scale(cr, Z, Z);
	if (V.on && !strcmp(V.st, "wire")) { srca(cr, cmix(V.fg, .7, CLEAR)); cairo_rectangle(cr, 0, y - 1, SW / Z, 1); cairo_fill(cr); }
	for (int i = 0; i < b.nsh; i++) { const Shd *s = &b.sh[i]; if (s->inset) continue;
		cairo_save(cr); cairo_rectangle(cr, 0, 0, SW / Z, y); cairo_clip(cr);
		if (s->blur <= 0) { srca(cr, s->c); cairo_rectangle(cr, 0, y + s->dy, SW / Z, 1 - s->dy > 0 ? -s->dy : 1); cairo_fill(cr); }
		else { for (int k = 0; k < (int)ceil(s->blur * 1.5) + 1; k++) { double t = (k + .5 + s->dy) / (s->blur); double a = s->c.a * .5 * erfc(t / 1.0); RGBA c = s->c; c.a = a; srca(cr, c); cairo_rectangle(cr, 0, y - 1 - k, SW / Z, 1); cairo_fill(cr); } }
		cairo_restore(cr); }
	cairo_restore(cr);
}

static void bar_redraw(void) { if (BARAREA) gtk_widget_queue_draw(BARAREA); mon_bars_queue(); }
static int hit_at(double x, double y) { for (int i = 0; i < NHITS; i++) if (x >= HITS[i].x && x < HITS[i].x + HITS[i].w && y >= HITS[i].y && y < HITS[i].y + HITS[i].h) return i; return -1; }
static void menu_toggle(void);
static void bar_click(int h)
{
	if (h < 0) return; Hit *t = &HITS[h];
	if (strcmp(t->kind, "clock")) snd_click();   /* (2026-11-16: the start button, pins, windows' buttons and the tray's are buttons on Standard; the clock is not) */
	if (!strcmp(t->kind, "start")) menu_toggle();
	else if (!strcmp(t->kind, "pin")) {
		for (GList *l = WINS; l; l = l->next) { AppWin *a = l->data; Client *c = WM_OK ? client_of_app(a) : NULL; if (!a->hidden && !strcmp(a->k, t->arg) && (!c || c->ws == WSC)) { appwin_activate(a); return; } }   /* already open in this workspace: to the front (else a new window, as the page's) */
		for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (c->ak[0] && !strcmp(c->ak, t->arg) && !c->trans) { wm_focus(c); return; } }   /* (Oracle's Chromium: 12.3c) */
		app_open(t->arg, NULL);
	}
	else if (!strcmp(t->kind, "win")) {
		unsigned long id = strtoul(t->arg, NULL, 16);
		if (WM_OK) { Client *c = client_of((Window)id); if (c) { if (c == FOCUS && !c->min) wm_minimise(c); else { int was = c->min; wm_focus(c); if (!was) lp_tbb(c); } } return; }   /* (Part 17b: its outline traced) */
		for (GList *l = WINS; l; l = l->next) if ((unsigned long)(uintptr_t)l->data == id) { appwin_activate(l->data); return; }
	}
	else if (!strcmp(t->kind, "clock")) fl_open(1);   /* (the calendar under the clock, as the page's) */
	else if (!strcmp(t->kind, "vol")) fl_open(2);   /* (the volume's pop-up) */
	else if (!strcmp(t->kind, "net")) fl_open(FL_NET);   /* (the network's pop-up, net31.js net31Pop) */
	else if (!strcmp(t->kind, "bri")) fl_open(FL_BRI);   /* (1.6.0: the brightness's pop-up, briPop) */
	else if (!strcmp(t->kind, "vkt")) { VK_SHOWN = !VK_SHOWN; vk_sync(); state_write(); }   /* (1.6.0: the on-screen keyboard shown or hidden, vkt.onclick) */
	else if (!strcmp(t->kind, "call")) { char p[40]; snprintf(p, sizeof p, "/api/mic?muted=%d", !MIC_MUTED); GByteArray *b = NULL; JV *v = NULL;   /* (call59Toggle: the microphone muted for everything, or not) */
		if (http("POST", p, NULL, 0, &b) == 200 && b) v = jparse((char *)b->data); if (b) g_byte_array_free(b, TRUE);
		if (v) { MIC_MUTED = jbool(v, "muted"); JV *u = jprop(v, "users"); if (u && u->t == 4) MIC_N = u->len; } jfree(v); bar_redraw(); }
	else if (!strcmp(t->kind, "qs")) fl_open(3);   /* (quick settings) */
	else if (!strcmp(t->kind, "notes")) fl_open(4);   /* (the notifications' centre) */
	else if (!strcmp(t->kind, "bat")) { char ex[24]; snprintf(ex, sizeof ex, "&page=health"); app_open_ex("settings", NULL, ex, 0); }   /* (power31.js pwrShow: System health) */
	else if (!strcmp(t->kind, "ws")) { if (t->arg[0] >= '1' && t->arg[0] <= '0' + NWS) { if (NMON > 1) mon_show(MONK_CUR, t->arg[0] - '0', 1); else ws_go(t->arg[0] - '0'); } return; }   /* (1.6.1 Part 1: shown on this taskbar's monitor) */   /* (the workspaces: item 4; ▦, all windows, is item 5's) */
	else app_open("settings", NULL);
}
static void hk_set(int h, char *k, char *a) { if (h >= 0) { snprintf(k, 12, "%s", HITS[h].kind); snprintf(a, 80, "%s", HITS[h].arg); } else k[0] = a[0] = 0; }
static void bar_menu(GdkEventButton *e, int h);
/* (1.6.0) the wheel over the brightness's button: a step up or down, as osd31.js (rudolfd's brightness/step) */
static gboolean bar_scroll(GtkWidget *w, GdkEventScroll *e, gpointer d)
{
	(void)w; (void)d; int h = hit_at(e->x, e->y); if (h < 0 || strcmp(HITS[h].kind, "bri")) return FALSE;
	double dy = e->direction == GDK_SCROLL_UP ? -1 : e->direction == GDK_SCROLL_DOWN ? 1 : 0; if (e->direction == GDK_SCROLL_SMOOTH) dy = e->delta_y;
	if (dy) hk_post(dy < 0 ? "/api/brightness/step?d=up" : "/api/brightness/step?d=down", 1); return TRUE;
}
static int fl_bar_press(GdkEventButton *e, int h);
static void menu_hide(void);
static void mon_touch(int k);
static gboolean bar_press(GtkWidget *w, GdkEventButton *e, gpointer d) { (void)w; (void)d; mon_touch(MONK_CUR); PRS_BAR = MONK_CUR;
	/* (the page's start menu closes on any press outside it but its button (base_v24.html: document's pointerdown); a right-click on the
	   taskbar left the shell's open, under the taskbar's menu) */
	{ int h = hit_at(e->x, e->y); if (e->type == GDK_BUTTON_PRESS && MENU && gtk_widget_get_visible(MENU) && !(h >= 0 && !strcmp(HITS[h].kind, "start"))) menu_hide(); }
	if (fl_bar_press(e, hit_at(e->x, e->y))) return TRUE; if (e->type == GDK_BUTTON_PRESS && e->button == 3) { bar_menu(e, hit_at(e->x, e->y)); return TRUE; } if (e->type == GDK_BUTTON_PRESS && e->button == 1) { PRESS = hit_at(e->x, e->y); hk_set(PRESS, PRS_K, PRS_A); bar_redraw(); } return TRUE; }
static gboolean bar_release(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)w; (void)d; if (e->button != 1) return TRUE; int h = hit_at(e->x, e->y); int was = PRESS; PRESS = -1; PRS_K[0] = PRS_A[0] = 0; bar_redraw();
	if (h >= 0 && h == was) bar_click(h);
	return TRUE;
}
static gboolean bar_motion(GtkWidget *w, GdkEventMotion *e, gpointer d) { (void)w; (void)d; if (HOV_BAR != MONK_CUR) { HOVER = -1; HOV_K[0] = HOV_A[0] = 0; HOV_BAR = MONK_CUR; } int h = hit_at(e->x, e->y); if (h != HOVER) { HOVER = h; hk_set(h, HOV_K, HOV_A); bar_redraw(); } return FALSE; }
static gboolean bar_leave(GtkWidget *w, GdkEventCrossing *e, gpointer d) { (void)w; (void)e; (void)d; if (HOVER != -1) { HOVER = -1; HOV_K[0] = HOV_A[0] = 0; bar_redraw(); } return FALSE; }
static void bar_place(void)
{
	if (!BAR) return; int top = !strcmp(S.tb, "top");
	(void)top; gtk_widget_set_size_request(BAR, bar_w(), bar_h()); gtk_window_move(GTK_WINDOW(BAR), bar_x(), bar_y()); gtk_window_resize(GTK_WINDOW(BAR), bar_w(), bar_h());
}

/* ------------------------------------------------------------------ the windows' frames, as the page's (1.6.0 Part 12)
 * base_v24.html .win/.tb and titlebar59.js: a 1 px grey edge with 8 px corners, the title bar 34 px high (6px 8px
 * inside: the app's picture at 16 px, the title in 600 13px, then _ □ ✕, 24 by 22 px, 3 px apart), the active window's
 * bar the accent colour with white writing, the others light grey; the title bars' five styles and colour modes; the
 * window's shadow (0 10px 28px rgba(0,0,0,.28)) on the desktop beneath it. */
typedef struct { char st[12]; int on; RGBA bg, fg, bt, hi, lo, sh; } TwVars;
static TwVars tw_vars(void)
{
	TwVars v; memset(&v, 0, sizeof v); snprintf(v.st, sizeof v.st, "%s", style_ok(S.twstyle) ? S.twstyle : "standard");
	const char *m = S.twacm[0] && (!strcmp(S.twacm, "negative") || !strcmp(S.twacm, "comp")) ? S.twacm : "normal";
	v.on = strcmp(v.st, "standard") || strcmp(m, "normal");
	RGB b = variant(accent(), m); if (!strcmp(v.st, "mono")) b = grey_of(b); double l = lightness(b);
	v.bg = ca(b, 1); v.fg = ink_black(b) ? cx("#000000", 1) : cx("#ffffff", 1); v.bt = ca(shade(b, l > 50 ? -10 : 10), 1);
	v.hi = ca(shade(b, fmin(30, 96 - l)), 1); v.lo = ca(shade(b, -fmin(30, l - 6)), 1); v.sh = ca(shade(b, -fmin(50, l - 2)), 1);
	return v;
}
static double tw_rad(void) { return !strcmp(tw_vars().st, "retro") ? 0 : 8; }
/* (the owner's photo of 2026-11-15: the title bar as the page lays it out for every setting, not only the defaults) the bar's
   sizes in the page's pixels, as Chromium lays out .win>.tb (a flex row, 6px 8px inside, its items centred): the window's edge
   (1 px; 2 px white with high contrast, html.hc), the title in the Font size setting (S.fs: the bar grows past 34 px for large
   sizes), the touch mode's bigger bar and buttons (html.touch: at least 40 px inside, buttons 40 by 40), the picture's line
   (a 16 px picture 3 px below the baseline, as .ico) */
typedef struct { double b, tbh, C, a, d, lh, bw, bh, by, ty, iy; } TwGeom;
static TwGeom tw_geom(void)
{
	static TwGeom K; static char kf[128]; char kn[128]; double fs = S.fs >= 6 && S.fs <= 72 ? S.fs : 13; int touch = S.touch;
	snprintf(kn, sizeof kn, "%g %d %d %s", fs, touch, S.hc, UIFAM); if (!strcmp(kn, kf)) return K;   /* (measured once for the settings) */
	TwGeom g; memset(&g, 0, sizeof g);
	g.b = S.hc ? 2 : 1; g.bw = touch ? 40 : 24; g.bh = touch ? 40 : 22;
	{ cairo_surface_t *t = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 4, 4); cairo_t *cr = cairo_create(t); g.a = css_ascent(cr, UIFAM, 600, fs, &g.d); cairo_destroy(cr); cairo_surface_destroy(t); }
	g.lh = g.a + g.d; double ia = fmax(g.a, 13), id = fmax(g.d, 3), il = ia + id;   /* (the picture's line: its 16 px from 13 above the baseline to 3 below) */
	g.C = fmax(fmax(g.bh, g.lh), il); if (touch) g.C = fmax(g.C, 40); g.tbh = g.C + 12;
	g.by = 6 + (g.C - g.bh) / 2; g.ty = 6 + (g.C - g.lh) / 2; g.iy = 6 + (g.C - il) / 2 + (ia - 13);
	K = g; snprintf(kf, sizeof kf, "%s", kn); return g;
}
/* the window's edge and title bar together, in the screen's pixels (the frame's top: TH) */
static double tw_tbh(void) { return tw_geom().tbh; }
static int tw_th(void) { TwGeom g = tw_geom(); return (int)lround((g.b + g.tbh) * Z); }
static int tw_bw(void) { return S.hc ? (int)fmax(1, lround(2 * Z)) : 1; }
static void btn_rects(Client *c, int r[3][4])
{
	TwGeom g = tw_geom(); int candy = !strcmp(tw_vars().st, "candy"); double eb = BW / Z, fw = fw_of(c) / Z, x = fw - eb - 8 - g.bw - (candy ? 1 : 0);
	for (int i = 2; i >= 0; i--) { r[i][0] = (int)lround(x * Z); r[i][1] = (int)lround((eb + g.by) * Z); r[i][2] = (int)lround(g.bw * Z); r[i][3] = (int)lround(g.bh * Z); x -= g.bw + 3 + (candy ? 2 : 0); }
}
/* the frame's outline: round corners (X's shape: no compositor needed) */
static void frame_shape(Client *c)
{
	GdkWindow *g = gdk_x11_window_foreign_new_for_display(gdk_display_get_default(), c->frame); if (!g) return;
	int fw = fw_of(c), fh = fh_of(c); double r = c->full ? 0 : tw_rad() * Z;
	cairo_region_t *rg = cairo_region_create();
	for (int y = 0; y < fh; y++) { double dy = y < r ? r - y - .5 : y >= fh - r ? y - (fh - r) + .5 : 0, in = dy > 0 ? r - sqrt(fmax(0, r * r - dy * dy)) : 0;
		int x0 = (int)floor(in + .5); cairo_rectangle_int_t rc = { x0, y, fw - 2 * x0, 1 }; cairo_region_union_rectangle(rg, &rc); }
	/* (1.6.0 Part 17: during a window transition only the part of it the scene shows: X's shape again, no compositor) */
	if (c->lpclip) { int x0 = (int)floor(c->lpcr.x - c->x), y0 = (int)floor(c->lpcr.y - c->y), x1 = (int)ceil(c->lpcr.x + c->lpcr.w - c->x), y1 = (int)ceil(c->lpcr.y + c->lpcr.h - c->y), none = c->lpcr.w < .5 || c->lpcr.h < .5;
		cairo_rectangle_int_t rc = { x0, y0, none ? 0 : x1 - x0, none ? 0 : y1 - y0 }; cairo_region_intersect_rectangle(rg, &rc); }
	gdk_window_shape_combine_region(g, rg, 0, 0); cairo_region_destroy(rg); g_object_unref(g);
}
/* (the owner's photo) Settings › Accessibility › Color vision, which the page puts on the whole screen (base_v24.html applyCB: html's
   filter): grayscale(1), contrast(1.35) saturate(1.2) for High contrast, or the colour-blindness correction's matrix (cbValues), in
   sRGB, on the frame as drawn (the app's page inside filters itself) */
static void cb_filter(cairo_surface_t *sf)
{
	const char *m = S.cb; double M[3][3];
	if (!m[0] || !strcmp(m, "none")) return;
	if (!strcmp(m, "gray")) { static const double G[3][3] = {{.2126, .7152, .0722}, {.2126, .7152, .0722}, {.2126, .7152, .0722}}; memcpy(M, G, sizeof M); }
	else if (!strcmp(m, "contrast")) { double t = 1.2; double A[3][3] = {{.213 + .787 * t, .715 - .715 * t, .072 - .072 * t}, {.213 - .213 * t, .715 + .285 * t, .072 - .072 * t}, {.213 - .213 * t, .715 - .715 * t, .072 + .928 * t}};
		memcpy(M, A, sizeof M); }
	else { static const char *const K[3] = {"protan", "deutan", "tritan"}; static const double S0[3][3][3] = {{{.567, .433, 0}, {.558, .442, 0}, {0, .242, .758}}, {{.625, .375, 0}, {.7, .3, 0}, {0, .3, .7}}, {{.95, .05, 0}, {0, .433, .567}, {0, .475, .525}}};
		static const double CM[3][3][3] = {{{0, 0, 0}, {.7, 1, 0}, {.7, 0, 1}}, {{0, 0, 0}, {.7, 1, 0}, {.7, 0, 1}}, {{1, 0, .7}, {0, 1, .7}, {0, 0, 0}}};
		int k = -1; for (int i = 0; i < 3; i++) if (!strcmp(m, K[i])) k = i; if (k < 0) return;
		double E[3][3]; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) E[i][j] = (i == j) - S0[k][i][j];
		for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) { double v = (i == j); for (int q = 0; q < 3; q++) v += CM[k][i][q] * E[q][j]; M[i][j] = round(v * 10000) / 10000; } }
	int con = !strcmp(m, "contrast");
	cairo_surface_flush(sf); unsigned char *d = cairo_image_surface_get_data(sf); int st = cairo_image_surface_get_stride(sf), w = cairo_image_surface_get_width(sf), h = cairo_image_surface_get_height(sf);
	for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) { unsigned char *p = d + y * st + x * 4; double al = p[3] / 255.0; if (al <= 0) continue;
		double c[3] = { p[2] / 255.0 / al, p[1] / 255.0 / al, p[0] / 255.0 / al }, o[3];
		if (con) for (int i = 0; i < 3; i++) c[i] = fmin(1, fmax(0, (c[i] - .5) * 1.35 + .5));   /* (contrast first, then saturate) */
		for (int i = 0; i < 3; i++) o[i] = fmin(1, fmax(0, M[i][0] * c[0] + M[i][1] * c[1] + M[i][2] * c[2]));
		p[2] = (unsigned char)lround(o[0] * al * 255); p[1] = (unsigned char)lround(o[1] * al * 255); p[0] = (unsigned char)lround(o[2] * al * 255); }
	cairo_surface_mark_dirty(sf);
}
static int menu_dark(void);
static void frame_draw(Client *c)
{
	if (!c->frame || c->full) return;
	if (c->csd) { csd_frame_draw(c); return; }   /* (1.6.1 Part 15: the app's own title bar; Rudolf's frame its edge alone) */
	int fw = fw_of(c), fh = fh_of(c), act = c == FOCUS; TwVars V = tw_vars(); const char *st = V.st; TwGeom G = tw_geom(); double eb = BW / Z, TB = (TH - BW) / Z;   /* (the edge and the bar on whole pixels of the screen, as Chromium snaps them under the display scaling) */
	cairo_surface_t *sf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, fw, fh); cairo_t *cr = cairo_create(sf);   /* (drawn aside, then filtered as the page is: cb_filter) */
	cairo_scale(cr, Z, Z); double W = fw / Z, H = fh / Z, R = tw_rad();   /* (maximised too: the page's .win.max keeps its corners) */
	/* the window: its edge and its white */
	/* (second run, N27) the page's html.dark .win: #0f172a with a #334155 edge, the other windows' bars #1e293b with #cbd5e1 writing;
	   (the owner's photo) Oracle's window too, which is the page's window on Standard; high contrast: a 2 px white edge (html.hc .win) */
	int dk = menu_dark();
	srca(cr, cx(dk ? "#0f172a" : "#ffffff", 1)); cairo_paint(cr);
	Box win; memset(&win, 0, sizeof win); win.bg = cx(dk ? "#0f172a" : "#ffffff", 1); win.bw = eb; win.bc = S.hc ? cx("#ffffff", 1) : cx(dk ? "#334155" : "#9ca3af", 1); win.rad = R; box_paint(cr, 0, 0, W, H, &win);
	/* the title bar (inside the edge: 34 px or as tall as its writing, its top corners 7 px) */
	Box tb; memset(&tb, 0, sizeof tb); RGBA fg; double tbo = 1;   /* (tbo: the bar's opacity: .72 for the other windows in Wireframe, Candy, Retro) */
	/* (the other windows' bar, html.dark's where no style rule of !important sets it; Wireframe's writing is the window's own: the
	   Window text colour, or html.dark .win's) */
	RGBA ibg = cx(dk ? "#1e293b" : "#e5e7eb", 1), ifg = cx(dk ? "#cbd5e1" : "#6b7280", 1), wtx = dk ? cx("#e2e8f0", 1) : cx(S.tx[0] == '#' ? S.tx : "#000000", 1);
	if (!V.on) { tb.bg = act ? ca(accent(), 1) : ibg; fg = act ? cx("#ffffff", 1) : ifg; }
	else if (!strcmp(st, "wire")) { tb.bg = cmix(V.bg, .14, CLEAR); fg = wtx; shd_add(&tb, 0, -2, 0, V.bg, 1); if (act) shd_add(&tb, 0, 0, 0, V.bg, 2); else tbo = .72; }
	else if (!strcmp(st, "mono")) { tb.bg = act ? V.bg : cmix(V.bg, .35, cx("#9ca3af", 1)); fg = act ? V.fg : cx("#ffffff", 1); }
	else if (!strcmp(st, "candy")) { if (act) { tb.ng = 3; tb.gc[0] = V.hi; tb.gc[1] = V.bg; tb.gc[2] = V.lo; tb.gp[0] = 0; tb.gp[1] = .48; tb.gp[2] = 1; shd_add(&tb, 0, 1, 0, cx("#ffffff", .5), 1); fg = V.fg; }
		else { tb.bg = ibg; fg = ifg; tbo = .72; } }
	else if (!strcmp(st, "retro")) { if (act) { tb.bg = V.bg; fg = V.fg; } else { tb.bg = ibg; fg = ifg; tbo = .72; } }
	else { tb.bg = act ? V.bg : ibg; fg = act ? V.fg : ifg; }
	int grey = V.on && strcmp(st, "mono") && !act;   /* (twcol: the other windows' bars in grey) */
	cairo_push_group(cr);
	double tr = R > 0 ? 7 : 0;
	cairo_save(cr); rrect(cr, eb, eb, W - 2 * eb, TB, 0); cairo_clip(cr);
	if (tr > 0) { cairo_new_path(cr); cairo_move_to(cr, eb, eb + TB); cairo_line_to(cr, eb, eb + tr); cairo_arc(cr, eb + tr, eb + tr, tr, M_PI, 1.5 * M_PI); cairo_arc(cr, W - eb - tr, eb + tr, tr, 1.5 * M_PI, 2 * M_PI); cairo_line_to(cr, W - eb, eb + TB); cairo_close_path(cr); cairo_clip(cr); }
	if (tb.nsh && tb.sh[tb.nsh - 1].inset == 2) {   /* (Wireframe's active bar: a 1 px line of its colour all round as well) */
		tb.nsh--; box_paint(cr, eb, eb, W - 2 * eb, TB, &tb); srca(cr, V.bg); cairo_rectangle(cr, eb, eb, W - 2 * eb, TB); cairo_rectangle(cr, eb + 1, eb + 1, W - 2 * eb - 2, TB - 2); cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD); cairo_fill(cr); cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING); }
	else if (!strcmp(st, "retro") && act && V.on) { cairo_pattern_t *g = cairo_pattern_create_linear(eb, 0, W - eb, 0);
		cairo_pattern_add_color_stop_rgb(g, 0, V.lo.r, V.lo.g, V.lo.b); cairo_pattern_add_color_stop_rgb(g, .45, V.bg.r, V.bg.g, V.bg.b); cairo_pattern_add_color_stop_rgb(g, 1, V.hi.r, V.hi.g, V.hi.b);
		cairo_rectangle(cr, eb, eb, W - 2 * eb, TB); cairo_set_source(cr, g); cairo_fill(cr); cairo_pattern_destroy(g); }
	else box_paint(cr, eb, eb, W - 2 * eb, TB, &tb);
	cairo_restore(cr);
	/* the picture, the title, the buttons */
	int wt = !strcmp(st, "retro") && V.on ? 700 : 600; AppWin *a = c->app; const App *ap = a ? app_find(a->k) : c->ak[0] ? app_find(c->ak) : NULL; GdkPixbuf *px = a && a->pix ? a->pix : app_pix(ap);
	int filt = V.on && !strcmp(st, "mono") ? 1 : 0; double fs = S.fs >= 6 && S.fs <= 72 ? S.fs : 13, ix = eb + 8, ty = eb + G.ty;
	if (px) pix_at(cr, px, ix, eb + G.iy, 16, filt); else if (a && a->icon[0]) css_text(cr, a->icon, UIFAM, wt, fs, ix, ty, G.lh, fg, 0); else if (!a && ap && ap->icon[0]) css_text(cr, ap->icon, UIFAM, wt, fs, ix, ty, G.lh, fg, 0);
	int r[3][4]; btn_rects(c, r); double tx = ix + 16 + 3, tmax = r[0][0] / Z - (!strcmp(st, "candy") ? 1 : 0) - 3 - tx;
	const char *tt = a ? a->title : c->title;
	if (c->ontop) {   /* (Always on top: .win.ontop>.tb .t::after, " 📌" at 11 px on the title's line) */
		double pa, pd; pa = css_ascent(cr, UIFAM, wt, 11, &pd); double tw = css_width(cr, tt, UIFAM, wt, fs), pw = css_width(cr, " 📌", UIFAM, wt, 11);
		if (tw + pw <= tmax) { css_text(cr, tt, UIFAM, wt, fs, tx, ty, G.lh, fg, 0); css_text(cr, " 📌", UIFAM, wt, 11, tx + tw, ty + G.a - pa, pa + pd, fg, 0); }
		else css_text(cr, tt, UIFAM, wt, fs, tx, ty, G.lh, fg, tmax);
	} else css_text(cr, tt, UIFAM, wt, fs, tx, ty, G.lh, fg, tmax);
	static const char *const GL[3] = {"_", "□", "✕"};
	for (int i = 0; i < 3; i++) { double bx = r[i][0] / Z, by = r[i][1] / Z, bw = G.bw, bh = G.bh; int hv = c->hover == i + 1, pr = c->press == i + 1, cl = i == 2;
		Box b; memset(&b, 0, sizeof b); b.rad = 4; RGBA bf = fg;
		if (!V.on || !strcmp(st, "mono")) { if (hv) b.bg = cl ? cx("#e11d48", 1) : cx("#000000", .15); if (hv && cl) bf = cx("#ffffff", 1); }
		else if (!strcmp(st, "wire")) { b.rad = 3; b.bw = 1; b.bc = cmix(fg, .55, CLEAR); if (hv) { b.bg = cl ? cx("#e11d48", 1) : cmix(V.bg, .22, CLEAR); if (cl) { bf = cx("#ffffff", 1); b.bc = cx("#e11d48", 1); } } }
		else if (!strcmp(st, "candy")) { b.rad = 999; b.bg = hv && cl ? cx("#e11d48", 1) : V.bt; b.gl0 = .55; b.gl1 = .15; shd_add(&b, 0, 1, 0, cx("#ffffff", .7), 1); shd_add(&b, 0, 1, 2, cx("#000000", .333), 0); }
		else if (!strcmp(st, "retro")) { b.rad = 0; b.bg = cx(hv && !cl ? "#d4d4d4" : "#c0c0c0", 1); bf = hv && cl ? cx("#b91c1c", 1) : cx("#000000", 1);
			if (pr) { shd_add(&b, 1, 1, 0, cx("#404040", 1), 1); shd_add(&b, -1, -1, 0, cx("#ffffff", 1), 1); shd_add(&b, 2, 2, 0, cx("#808080", 1), 1); }
			else { shd_add(&b, -1, -1, 0, cx("#404040", 1), 1); shd_add(&b, 1, 1, 0, cx("#ffffff", 1), 1); shd_add(&b, -2, -2, 0, cx("#808080", 1), 1); shd_add(&b, 2, 2, 0, cx("#dfdfdf", 1), 1); } }
		else { if (hv) b.bg = cl ? cx("#e11d48", 1) : cx("#000000", .15); if (hv && cl) bf = cx("#ffffff", 1); }
		box_paint(cr, bx, by, bw, bh, &b);
		const char *g = GL[i];   /* (□ maximised too, as the page's) */ double gw = css_width(cr, g, "Arial", 600, 12);
		css_text(cr, g, "Arial", 600, 12, bx + (bw - gw) / 2, by + b.bw, bh - 2 * b.bw, bf, 0); }
	cairo_pop_group_to_source(cr);
	if (grey) {   /* (filter: grayscale(1)) */
		cairo_surface_t *g = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, fw, (int)ceil((eb + TB + 1) * Z)); cairo_t *gc = cairo_create(g); cairo_set_source(gc, cairo_get_source(cr)); cairo_paint(gc); cairo_destroy(gc);
		cairo_surface_flush(g); unsigned char *d = cairo_image_surface_get_data(g); int stw = cairo_image_surface_get_stride(g);
		for (int y = 0; y < cairo_image_surface_get_height(g); y++) for (int x = 0; x < fw; x++) { unsigned char *p = d + y * stw + x * 4; int l = (int)lround(.0722 * p[0] + .7152 * p[1] + .2126 * p[2]); p[0] = p[1] = p[2] = l > p[3] ? p[3] : l; }
		cairo_surface_mark_dirty(g); cairo_save(cr); cairo_identity_matrix(cr); cairo_set_source_surface(cr, g, 0, 0); cairo_paint_with_alpha(cr, tbo); cairo_restore(cr); cairo_surface_destroy(g);
	} else cairo_paint_with_alpha(cr, tbo);
	cairo_destroy(cr); cb_filter(sf);
	cairo_surface_t *xs = cairo_xlib_surface_create(DPY, c->frame, DefaultVisual(DPY, DefaultScreen(DPY)), fw, fh); cairo_t *xc = cairo_create(xs);
	cairo_set_operator(xc, CAIRO_OPERATOR_SOURCE); cairo_set_source_surface(xc, sf, 0, 0); cairo_paint(xc); cairo_destroy(xc); cairo_surface_destroy(xs); cairo_surface_destroy(sf);
}
/* (1.6.1 Part 15) */
/* the frame of a window drawing its own title bar: the window's edge alone (its white, or html.dark's, behind) */
static void csd_frame_draw(Client *c)
{
	int fw = fw_of(c), fh = fh_of(c), dk = menu_dark(); double eb = BW / Z;
	cairo_surface_t *sf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, fw, fh); cairo_t *cr = cairo_create(sf); cairo_scale(cr, Z, Z);
	srca(cr, cx(dk ? "#0f172a" : "#ffffff", 1)); cairo_paint(cr);
	Box win; memset(&win, 0, sizeof win); win.bg = cx(dk ? "#0f172a" : "#ffffff", 1); win.bw = eb; win.bc = S.hc ? cx("#ffffff", 1) : cx(dk ? "#334155" : "#9ca3af", 1); win.rad = tw_rad();
	box_paint(cr, 0, 0, fw / Z, fh / Z, &win); cairo_destroy(cr); cb_filter(sf);
	cairo_surface_t *xs = cairo_xlib_surface_create(DPY, c->frame, DefaultVisual(DPY, DefaultScreen(DPY)), fw, fh); cairo_t *xc = cairo_create(xs);
	cairo_set_operator(xc, CAIRO_OPERATOR_SOURCE); cairo_set_source_surface(xc, sf, 0, 0); cairo_paint(xc); cairo_destroy(xc); cairo_surface_destroy(xs); cairo_surface_destroy(sf);
}
/* the windows' shadows, on the desktop beneath them (0 10px 28px rgba(0,0,0,.28)) */
static void frames_shadow(cairo_t *cr)
{
	if (!WM_OK || LOCKED) return;
	for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (c->min || c->full || c->hidden || c->trans) continue;   /* (a maximised window's too, as the page's .win.max) */
		if (NMON > 1 && (c->off || mon_k(c) != MON_DRAWK)) continue;   /* (1.6.1 Part 1: each monitor's own windows) */
		double x = c->x / Z, y = c->y / Z, w = fw_of(c) / Z, h = fh_of(c) / Z; int pad = 40;
		cairo_surface_t *m = cairo_image_surface_create(CAIRO_FORMAT_A8, (int)ceil((w + 2 * pad) * Z), (int)ceil((h + 2 * pad) * Z)); cairo_t *mc = cairo_create(m);
		cairo_scale(mc, Z, Z); rrect(mc, pad, pad, w, h, tw_rad()); cairo_set_source_rgba(mc, 0, 0, 0, 1); cairo_fill(mc); cairo_destroy(mc); a8_blur(m, 14 * Z);
		cairo_save(cr); cairo_set_source_rgba(cr, 0, 0, 0, .28); cairo_mask_surface(cr, m, round((x - pad) * Z), round((y - pad + 10) * Z)); cairo_restore(cr); cairo_surface_destroy(m); }
}

/* ------------------------------------------------------------------ the start menu */
static GtkCssProvider *CSS;
static void menu_hide(void);
static void css_apply(void)
{
	if (!CSS) { CSS = gtk_css_provider_new(); gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(CSS), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION); }
	RGB a = accent(); char ac[16]; snprintf(ac, sizeof ac, "#%02x%02x%02x", (int)(a.r * 255), (int)(a.g * 255), (int)(a.b * 255));
	char *c = g_strdup_printf(
		".rn-menu{background:#111827;color:#f9fafb;border:1px solid #374151;border-radius:8px}"
		".rn-menu label{color:#f9fafb}.rn-menu .rn-head{font-weight:bold;padding:10px 12px}"
		".rn-menu entry{margin:0 10px 6px 10px}"
		".rn-menu list,.rn-menu row{background:transparent;color:#f9fafb}.rn-menu row:hover,.rn-menu row:selected{background:#1f2937}"
		".rn-menu row label{padding:6px 10px}.rn-menu .rn-power button{margin:6px 4px;background:#1f2937;color:#f9fafb;border:0;border-radius:5px;box-shadow:none;background-image:none}"
		".rn-menu .rn-power button:hover{background:%s}"
		".rn-card{background:rgba(17,24,39,.88);color:#f9fafb;border-radius:12px;padding:24px}.rn-card label{color:#f9fafb}"
		".rn-card .rn-big{font-size:22px;font-weight:bold}.rn-card .rn-msg{color:#fca5a5}"
		".rn-card button{background:#1f2937;color:#f9fafb;border:0;border-radius:6px;background-image:none;box-shadow:none;padding:6px 14px}"
		".rn-card button.rn-go{background:%s}.rn-card entry{min-width:260px}"
		".rn-menu label,.rn-menu entry,.rn-menu button{font-size:%.1fpt}", ac, ac, 10 * Z);   /* (the display scaling) */
	gtk_css_provider_load_from_data(CSS, c, -1, NULL); g_free(c);
}
static void signin_show(int lock);
static void power(const char *what)
{
	menu_hide();
	/* (1.6.0, the Rudolf sounds) signing out and locking: the page's sfx('logout'), core25.js userLogout and lockScreen */
	if (!strcmp(what, "lock") || !strcmp(what, "logout")) snd_play("logout");
	if (!strcmp(what, "lock")) { if (http("POST", "/api/desktop/lock", NULL, 0, NULL) == 200) signin_show(1); return; }
	if (!strcmp(what, "logout")) {
		GList *c = g_list_copy(WINS); for (GList *l = c; l; l = l->next) appwin_close_now(l->data); g_list_free(c);
		/* (1.6.0 Part 21, the owner's native rule: as on Standard, signing out closes Oracle too: its page saves its tabs for "the tabs
		   from last time" and clears Guest's websites, then its Chromium closes; rudolf-launch, "logout") */
		oracle_only("logout\n");
		/* (1.6.0 Part 22) its websites are in the account's encrypted folder: Chromium closed before rudolfd locks it (up to 6 s;
		   then rudolfd's lock finishes once Chromium's files are closed) */
		if (WEB_DIR[0]) { gint64 until = g_get_monotonic_time() + 6 * G_USEC_PER_SEC; while (OR_RUN && g_get_monotonic_time() < until) { if (!g_main_context_iteration(NULL, FALSE)) g_usleep(50000); } }
		WEB_DIR[0] = 0;
		pt_reset();   /* (the portal's requests: the next account's page starts afresh) */
		svc_reset();   /* (the service page ended with the windows: the next account's starts at its sign-in) */
		http("POST", "/api/desktop/logout", NULL, 0, NULL); USER_ID[0] = 0; signin_show(0); return;
	}
	/* (the shut-down chime first, as the page's: up to 3 s, report 2) */
	if (!strcmp(what, "shutdown") || !strcmp(what, "reboot") || !strcmp(what, "poweroff") || !strcmp(what, "restart")) { double l = snd_play("shutdown"); if (l > 0) g_usleep((gulong)(fmin(3, l + .15) * 1e6)); }
	char p[80]; snprintf(p, sizeof p, "/api/power?action=%s", what); http("POST", p, NULL, 0, NULL);
}
/* (1.6.0 Part 12, "The rule") the start menu as the page draws it (base_v24.html #menu, styles25.css): a white panel
   (dark with the dark theme) 244 px wide at the bottom left, 2 px above the taskbar, as tall as its list (at most the
   screen less 70 px), with its shadow; the search box, the account's line, then the page's own list, row by row as
   the page gave it (desktop/apps "menu": its separators, headings, greyed and indented rows), scrolled with the wheel.
   A row under the pointer is the accent colour. Typing searches the list. The shadow falls on what is behind the
   menu: a copy of the screen taken as it opens. */
typedef struct { char t, a[KN], v[KN], cls[48], txt[160], ic[24], emo[48]; double op, fs, fw, pl, pt, pb, ls; int up; RGBA col; double y, h; int shown; } MRow;
static MRow *MR; static int NMR;
static GdkPixbuf *MBG; static double MSCROLL, MCONT; static int MHOV = -1, MX, MY, MW, MH; static double MBX, MBY, MBW, MBH;   /* the window, and the menu in it */
static char MQ[128]; static GtkWidget *MAREA; static GtkIMContext *MIM;
static int MCARET; static guint MCARET_T;   /* the search box's caret, blinking as Chromium's (500 ms), shown again on each key */
static gboolean menu_blink(gpointer d) { (void)d; MCARET = !MCARET; if (MAREA) gtk_widget_queue_draw(MAREA); return G_SOURCE_CONTINUE; }
static void caret_restart(void) { MCARET = 1; if (MCARET_T) g_source_remove(MCARET_T); MCARET_T = g_timeout_add(500, menu_blink, NULL); }
/* (an input method for typing into what the shell draws, as GtkEntry has: dead keys, compose) */
static void im_realize(GtkWidget *w, gpointer im) { gtk_im_context_set_client_window(im, gtk_widget_get_window(w)); gtk_im_context_focus_in(im); }
/* (the page's html.dark, themeApply: the dark theme for the shell's parts unless turned off, when it is dark: dynResolve's mode) */
static int menu_dark(void) { return S.darkui && is_dark(); }
static void srch_later(void);
static void menu_hide(void) { if (MENU && gtk_widget_get_visible(MENU)) { gtk_widget_hide(MENU); pop_gone(MBG, MX, MY); lp_menu_closed(MENU, MBX * Z, MBY * Z, MBW * Z, MBH * Z); } MHOV = -1; if (MCARET_T) { g_source_remove(MCARET_T); MCARET_T = 0; } srch_later(); state_write(); }
static void menu_rows_load(JV *cat)
{
	free(MR); MR = NULL; NMR = 0; JV *m = jprop(cat, "menu");
	for (int i = 0; i < 400; i++) { JV *o = jat(m, i); if (!o) break; MR = realloc(MR, sizeof *MR * (NMR + 1)); MRow *r = &MR[NMR++]; memset(r, 0, sizeof *r);
		char t[8] = ""; jstr(o, "t", t, sizeof t); r->t = t[0] == 'h' ? 'h' : 'l';
		jstr(o, "a", r->a, sizeof r->a); jstr(o, "v", r->v, sizeof r->v); jstr(o, "cls", r->cls, sizeof r->cls); jstr(o, "txt", r->txt, sizeof r->txt); jstr(o, "ic", r->ic, sizeof r->ic); jstr(o, "emo", r->emo, sizeof r->emo);
		r->op = jnum(o, "op", 1); r->fs = jnum(o, "fs", 14); r->fw = jnum(o, "fw", 400); r->pl = jnum(o, "pl", 10); r->pt = jnum(o, "pt", 7); r->pb = jnum(o, "pb", 7); r->ls = jnum(o, "ls", 0); r->up = jbool(o, "up");
		char c[40] = ""; jstr(o, "col", c, sizeof c); int R = 0, G = 0, B = 0; r->col = sscanf(c, "rgb(%d, %d, %d)", &R, &G, &B) == 3 ? (RGBA){R / 255.0, G / 255.0, B / 255.0, 1} : cx("#000000", 1); }
}
static int mrow_match(const MRow *r) { if (!MQ[0]) return 1; if (r->t != 'l' || strstr(r->cls, "offh")) return 0; char *h = g_utf8_strdown(r->txt, -1), *q = g_utf8_strdown(MQ, -1); int m = strstr(h, q) != NULL; g_free(h); g_free(q); return m; }
/* the rows' places (from the list's top) */
static double menu_layout(cairo_t *cr)
{
	double y = 0;
	for (int i = 0; i < NMR; i++) { MRow *r = &MR[i]; r->shown = mrow_match(r); r->y = y; r->h = 0; if (!r->shown) continue;
		if (r->t == 'h') { r->h = 9; y += 9; continue; }
		double d, a = css_ascent(cr, UIFAM, (int)r->fw, r->fs, &d), lt = -a, lb = d;
		if (r->ic[0]) { lt = fmin(lt, -15); lb = fmax(lb, 5); }   /* (the picture: 20 px, vertical-align -5px) */
		r->h = r->pt + (lb - lt) + r->pb; y += r->h; }
	return y;
}
/* (second run, N30; N22) the start menu scrolls: Chromium's classic scrollbar on #menu's right, inside its edge, its content 15 px
   narrower (Rudolf's Chromium shows it; same_t.sh now does too) */
static double menu_sb(void) { return !MQ[0] && MCONT + 14 > MBH + .5 ? 15 : 0; }
static void menu_sb_draw(cairo_t *cr)
{
	if (!menu_sb()) return; double x = MBW - 1 - 15, y0 = 1, h = MBH - 2, track = h - 30, tot = MCONT + 14, th = fmax(17, track * MBH / tot), mx = fmax(1, tot - MBH), ty = y0 + 15 + (track - th) * fmin(1, MSCROLL / mx);
	srca(cr, cx("#fcfcfc", 1)); cairo_rectangle(cr, x, y0, 15, h); cairo_fill(cr);
	srca(cr, cx("#8b8b8b", 1)); rrect(cr, x + 3, ty, 9, th, 4.5); cairo_fill(cr);
	cairo_move_to(cr, x + 3, y0 + 11); cairo_line_to(cr, x + 12, y0 + 11); cairo_line_to(cr, x + 7.5, y0 + 6); cairo_close_path(cr); cairo_fill(cr);
	cairo_move_to(cr, x + 3, y0 + h - 11); cairo_line_to(cr, x + 12, y0 + h - 11); cairo_line_to(cr, x + 7.5, y0 + h - 6); cairo_close_path(cr); cairo_fill(cr);
}
/* (2026-11-16, the owner's Vivobook: "Packages is not in the start menu") the start menu's search as Standard's (shell25.js: #ssearch,
   #ssres, startSearch): typing hides the list and shows what is found, in groups (Calculator, Apps, Settings, Files) of at most 24
   rows, the first one chosen; ↑ ↓ move, Enter or a click opens, Escape closes; nothing found: "No results". It searched the start
   menu's own rows here, so Packages, which Standard finds among the apps (it has no row), was not found. The finding is the page's
   own startSearch, in a page of the shell's kept unseen while the menu is used (#app=do-srch, oneapp59.js __oaSrch); until it is
   there, the apps as startSearch finds them (by name and search words), from the catalogue. Drawn as styles25.css draws it: .ssg
   11 px upper case in --mut, 6px 4px 2px; .ssi 6 px padding, 6 px corners, its picture 22 px wide, 8 px apart, the place in small
   writing at most 100 px wide at the right; the chosen row (and the one under the pointer) in the accent colour, its writing --acfg.
   The menu is as tall as what it shows (#ssres at most 55 % of the screen, scrolling), from the taskbar up, as the page's #menu. */
typedef struct { char g[16], t[200], i[48], sub[200], a[8], arg[1024]; double y, h, tw, sw; int nl; } SItem;   /* (tw, sw: its words' and its place's widths; nl: its words' lines) */
static SItem SRI[24]; static int NSRI, SRCUR, SRHOV = -1, SRPG_OK; static double SRSCROLL, SRRH, SRCH, MBH0, MBY0; static AppWin *SRPG, *BG_SRCH; static gint64 SRPG_T; static guint SRCLOSE_T;   /* (BG_SRCH: the hidden page, once it can search: 1.6.1) */
static double srch_l11(void) { static double v; if (!v) { cairo_surface_t *t = cairo_image_surface_create(CAIRO_FORMAT_A8, 1, 1); cairo_t *c = cairo_create(t); double d, a = css_ascent(c, UIFAM, 400, 11, &d); v = a + d; cairo_destroy(c); cairo_surface_destroy(t); } return v; }
static double srch_l13(void) { static double v; if (!v) { cairo_surface_t *t = cairo_image_surface_create(CAIRO_FORMAT_A8, 1, 1); cairo_t *c = cairo_create(t); double d, a = css_ascent(c, UIFAM, 400, 13, &d); v = a + d; cairo_destroy(c); cairo_surface_destroy(t); } return v; }
static cairo_t *srch_cr(void) { static cairo_t *c; if (!c) { cairo_surface_t *t = cairo_image_surface_create(CAIRO_FORMAT_A8, 4, 4); c = cairo_create(t); cairo_surface_destroy(t); } return c; }
static PangoLayout *srch_words(cairo_t *cr, const SItem *it) { PangoLayout *pl = css_layout(cr, it->t, UIFAM, 400, 13); pango_layout_set_width(pl, (int)(fmax(1, it->tw) * PANGO_SCALE)); pango_layout_set_wrap(pl, PANGO_WRAP_WORD); return pl; }
/* a row as the page's flex box lays it out (.ssi: 6 px padding, the picture 22 px, 8 px gaps): its words (flex: 1, wrapped, never narrower
   than their longest word) and its place (at most 100 px, made narrower first, an ellipsis) */
static void srch_row(SItem *it, double w)
{
	cairo_t *cr = srch_cr(); double room = w - 12 - 22 - 8 - 8, sw = it->sub[0] ? fmin(100, css_width(cr, it->sub, UIFAM, 400, 13 * 5 / 6.0)) : 0, minw = 0;
	{ gchar **w = g_strsplit(it->t, " ", -1); for (int i = 0; w[i]; i++) if (w[i][0]) minw = fmax(minw, css_width(cr, w[i], UIFAM, 400, 13)); g_strfreev(w); }   /* (its longest word: min-content) */
	it->tw = fmax(room - sw, minw); it->sw = fmax(0, room - it->tw); if (it->sw > sw) it->sw = sw;
	PangoLayout *pl = srch_words(cr, it); it->nl = MAX(1, pango_layout_get_line_count(pl)); g_object_unref(pl);
	it->h = 12 + it->nl * srch_l13();
}
/* the rows' places (from #ssres's top) and the menu's size for them */
static void menu_fit(void)
{
	if (!MBH0) return;
	if (!MQ[0]) { MBH = MBH0; MBY = MBY0; return; }
	double y = 0, w = MBW - 14 - 8; for (int pass = 0; pass < 2; pass++) { y = 0; for (int i = 0; i < NSRI; i++) { if (i == 0 || strcmp(SRI[i].g, SRI[i - 1].g)) y += 6 + srch_l11() + 2; SRI[i].y = y; srch_row(&SRI[i], w); y += SRI[i].h; }
		if (pass || y <= .55 * SH / Z) break; w -= 15; }   /* (rows narrower by the scrollbar's 15 px when they scroll) */
	if (!NSRI) y = 6 + srch_l11() + 2;   /* ("No results": a group's heading) */
	SRCH = y; SRRH = fmin(y, .55 * SH / Z); SRSCROLL = fmin(SRSCROLL, fmax(0, SRCH - SRRH));
	MBH = fmin(37 + SRRH + 38 + 14, MBH0); MBY = (!strcmp(S.tb, "top") || tb_side()) ? MBY0 : MBY0 + MBH0 - MBH;
}
static int srch_sb(void) { return SRCH > SRRH + .5; }
static void srch_local(void)
{
	NSRI = 0; char *q = g_utf8_strdown(MQ, -1); g_strstrip(q); if (!q[0]) { g_free(q); return; }
	for (int i = 0; i < NAPPS && NSRI < 24; i++) { char *n = g_utf8_strdown(APPS[i].name, -1); int m = strstr(n, q) || strstr(APPS[i].kw, q); g_free(n); if (!m) continue;
		SItem *it = &SRI[NSRI++]; memset(it, 0, sizeof *it); snprintf(it->g, sizeof it->g, "Apps"); snprintf(it->t, sizeof it->t, "%s", APPS[i].name); snprintf(it->i, sizeof it->i, "%s", APPS[i].icon); snprintf(it->a, sizeof it->a, "app"); snprintf(it->arg, sizeof it->arg, "%s", APPS[i].k); }
	g_free(q);
}
static void srch_got(GObject *src, GAsyncResult *res, gpointer d)
{
	char *q = d; JSCValue *v = webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(src), res, NULL); char *t = v && jsc_value_is_string(v) ? jsc_value_to_string(v) : NULL; if (v) g_object_unref(v);
	if (t && t[0] == '{' && MENU && gtk_widget_get_visible(MENU) && !strcmp(q, MQ)) { JV *all = jparse(t), *l = jprop(all, "r"); NSRI = 0;
		for (int i = 0; l && i < l->len && NSRI < 24; i++) { JV *o = l->v[i]; SItem *it = &SRI[NSRI]; memset(it, 0, sizeof *it); jstr(o, "g", it->g, sizeof it->g); jstr(o, "t", it->t, sizeof it->t); jstr(o, "i", it->i, sizeof it->i); jstr(o, "sub", it->sub, sizeof it->sub); jstr(o, "a", it->a, sizeof it->a);
			jstr(o, !strcmp(it->a, "file") ? "p" : !strcmp(it->a, "copy") ? "v" : "k", it->arg, sizeof it->arg); if (it->t[0]) NSRI++; }
		jfree(all); if (SRCUR >= NSRI) SRCUR = 0; menu_fit(); if (MAREA) gtk_widget_queue_draw(MAREA); state_write(); }
	g_free(t); g_free(q);
}
static void srch_ask(void)
{
	if (!SRPG || !SRPG_OK || !g_list_find(WINS, SRPG) || !SRPG->view) return;
	GString *js = g_string_new("window.__oaSrch?__oaSrch("); json_esc(js, MQ); g_string_append(js, "):''");
	webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(SRPG->view), js->str, -1, NULL, NULL, NULL, srch_got, g_strdup(MQ)); g_string_free(js, TRUE);
}
/* (the page that searches: started when the menu opens, closed a while after it was last used) */
static void srch_page(void)
{
	if (SRCLOSE_T) { g_source_remove(SRCLOSE_T); SRCLOSE_T = 0; }
	if (SRPG && g_list_find(WINS, SRPG)) return; SRPG = NULL; SRPG_OK = 0;
	/* (1.6.1: the shell's hidden page searches, once it has said it can, op "srch") */
	{ AppWin *b = bg_page(); if (b && BG_SRCH == b) { SRPG = b; SRPG_OK = 1; SRPG_T = 0; return; } }
	if (SRPG_T && g_get_monotonic_time() - SRPG_T < 30 * G_USEC_PER_SEC) return;   /* (asked for already, coming) */
	SRPG_T = g_get_monotonic_time(); app_open_ex("do-srch", NULL, NULL, 1);
}
static void srch_gone(AppWin *a) { if (a == SRPG) { SRPG = NULL; SRPG_OK = 0; } if (a == BG_SRCH) BG_SRCH = NULL; }
static void srch_ready(AppWin *a) { if (is_bg(a)) { BG_SRCH = a; if (SRPG && SRPG != a && g_list_find(WINS, SRPG)) return; } SRPG = a; SRPG_OK = 1; SRPG_T = 0; if (MENU && gtk_widget_get_visible(MENU) && MQ[0]) srch_ask(); }
static gboolean srch_close(gpointer d) { (void)d; SRCLOSE_T = 0; if (MENU && gtk_widget_get_visible(MENU)) return G_SOURCE_REMOVE; if (SRPG && g_list_find(WINS, SRPG) && !is_bg(SRPG)) appwin_close_now(SRPG); SRPG = NULL; SRPG_OK = 0; return G_SOURCE_REMOVE; }
static void srch_later(void) { if (SRCLOSE_T) g_source_remove(SRCLOSE_T); SRCLOSE_T = SRPG || SRPG_T ? g_timeout_add_seconds(90, srch_close, NULL) : 0; }
static void srch_update(void)
{
	SRSCROLL = 0; SRCUR = 0; SRHOV = -1;
	if (!MQ[0]) NSRI = 0; else { if (!SRPG_OK) srch_local(); if (SRPG_OK) srch_ask(); else srch_page(); }
	menu_fit();
}
static void srch_do(int i)
{
	if (i < 0 || i >= NSRI) return; SItem it = SRI[i]; menu_hide(); MQ[0] = 0; NSRI = 0;
	if (!strcmp(it.a, "app")) { for (GList *l = WINS; l; l = l->next) { AppWin *w = l->data; if (!w->hidden && !strcmp(w->k, it.arg)) { appwin_activate(w); return; } } app_open(it.arg, NULL); }
	else if (!strcmp(it.a, "set")) { if (it.arg[0] && id_ok(it.arg)) tb_settings(it.arg); else sc_settings(); }
	else if (!strcmp(it.a, "file") && it.arg[0]) app_open_ex("do-fs", it.arg, "&op=open", 2);   /* (the file with its app, as the page's openPath) */
	else if (!strcmp(it.a, "copy")) gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), it.arg, -1);   /* (the sum's value copied, as the page's) */
}
/* the found rows: which one is at this place of the menu's window (-1: none) */
static int srch_at(double ex, double ey)
{
	if (!MQ[0]) return -1; double x = ex / Z - (MBX - MX / Z), y = ey / Z - (MBY - MY / Z) - 38 + SRSCROLL;
	if (x < 11 || x > MBW - 11 - (srch_sb() ? 15 : 0) || y < SRSCROLL || y > SRSCROLL + SRRH) return -1;
	for (int i = 0; i < NSRI; i++) if (y >= SRI[i].y && y < SRI[i].y + SRI[i].h) return i;
	return -1;
}
static void srch_draw(cairo_t *cr, double y0, int dark, RGBA ink)
{
	double x0 = 11, w = MBW - 14 - 8 - (srch_sb() ? 15 : 0); RGBA mut = cx(dark ? "#94a3b8" : "#6b7280", 1);
	cairo_save(cr); cairo_rectangle(cr, x0, y0, MBW - 22, SRRH); cairo_clip(cr); cairo_translate(cr, 0, y0 - SRSCROLL);
	if (!NSRI) css_text(cr, "NO RESULTS", UIFAM, 400, 11, x0 + 4, 6, 0, mut, 0);
	for (int i = 0; i < NSRI; i++) { SItem *it = &SRI[i];
		if (i == 0 || strcmp(it->g, SRI[i - 1].g)) { char *u = g_utf8_strup(it->g, -1); PangoLayout *pl = css_layout(cr, u, UIFAM, 400, 11); PangoAttrList *al = pango_attr_list_new();
			pango_attr_list_insert(al, pango_attr_letter_spacing_new((int)(.44 * PANGO_SCALE))); pango_layout_set_attributes(pl, al); pango_attr_list_unref(al);
			double a, d; css_metrics(pl, &a, &d); srca(cr, mut); cairo_move_to(cr, x0 + 4, round(it->y - 2 - srch_l11() + a) - pango_layout_get_baseline(pl) / (double)PANGO_SCALE); pango_cairo_show_layout(cr, pl); g_object_unref(pl); g_free(u); }
		int on = i == SRCUR || i == SRHOV; RGBA c = ink, cs = mut;
		if (on) { Box h; memset(&h, 0, sizeof h); h.bg = ca(accent(), 1); h.rad = 6; box_paint(cr, x0, it->y, w, it->h, &h); c = cs = ink_black(accent()) ? cx("#000000", 1) : cx("#ffffff", 1); }
		double ty = it->y + 6, mid = ty + (it->nl - 1) * srch_l13() / 2;   /* (align-items: center) */
		double iw = css_width(cr, it->i, UIFAM, 400, 13); css_text(cr, it->i, UIFAM, 400, 13, x0 + 6 + fmax(0, (22 - iw) / 2), mid, srch_l13(), c, 0);
		{ PangoLayout *pl = srch_words(cr, it); double a, d; css_metrics(pl, &a, &d); double base = round(ty + floor((srch_l13() - (a + d)) / 2) + a); srca(cr, c);
		  for (int k = 0; k < pango_layout_get_line_count(pl); k++) { cairo_move_to(cr, x0 + 6 + 22 + 8, base + k * srch_l13()); pango_cairo_show_layout_line(cr, pango_layout_get_line_readonly(pl, k)); }   /* (each line "normal" tall, as Blink's) */
		  g_object_unref(pl); }
		if (it->sw > 0) css_text(cr, it->sub, UIFAM, 400, 13 * 5 / 6.0, x0 + w - 6 - it->sw, mid, srch_l13(), cs, it->sw); }
	cairo_restore(cr);
	if (srch_sb()) { double x = x0 + MBW - 22 - 15, h = SRRH, track = h - 30, th = fmax(17, track * SRRH / SRCH), ty = y0 + 15 + (track - th) * fmin(1, SRSCROLL / fmax(1, SRCH - SRRH));
		srca(cr, cx("#fcfcfc", 1)); cairo_rectangle(cr, x, y0, 15, h); cairo_fill(cr); srca(cr, cx("#8b8b8b", 1)); rrect(cr, x + 3, ty, 9, th, 4.5); cairo_fill(cr);
		cairo_move_to(cr, x + 3, y0 + 11); cairo_line_to(cr, x + 12, y0 + 11); cairo_line_to(cr, x + 7.5, y0 + 6); cairo_close_path(cr); cairo_fill(cr);
		cairo_move_to(cr, x + 3, y0 + h - 11); cairo_line_to(cr, x + 12, y0 + h - 11); cairo_line_to(cr, x + 7.5, y0 + h - 6); cairo_close_path(cr); cairo_fill(cr); }
}
static gboolean menu_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)w; (void)d; int dark = menu_dark();
	if (MBG) { gdk_cairo_set_source_pixbuf(cr, MBG, 0, 0); cairo_paint(cr); }
	lp_draw_clip(cr, MENU, MX, MY);   /* (Part 17b: opening) */
	cairo_scale(cr, Z, Z); double ox = (MBX - MX / Z), oy = (MBY - MY / Z);   /* the menu's place in the window, in the page's units */
	cairo_translate(cr, ox, oy);
	/* its shadow: 0 12px 32px rgba(0,0,0,.3) */
	{ int pad = 48, sw = (int)ceil((MBW + 2 * pad) * Z), sh = (int)ceil((MBH + 2 * pad) * Z); cairo_surface_t *m = cairo_image_surface_create(CAIRO_FORMAT_A8, sw, sh); cairo_t *mc = cairo_create(m);
		cairo_scale(mc, Z, Z); rrect(mc, pad, pad, MBW, MBH, 10); cairo_set_source_rgba(mc, 0, 0, 0, 1); cairo_fill(mc); cairo_destroy(mc); a8_blur(m, 16 * Z);
		cairo_save(cr); cairo_scale(cr, 1 / Z, 1 / Z); cairo_set_source_rgba(cr, 0, 0, 0, .3); cairo_mask_surface(cr, m, -pad * Z, (12 - pad) * Z); cairo_restore(cr); cairo_surface_destroy(m); }
	Box b; memset(&b, 0, sizeof b); b.bg = cx(dark ? "#0f172a" : "#ffffff", 1); b.bw = 1; b.bc = cx(dark ? "#334155" : "#d1d5db", 1); b.rad = 10; hc_edge(&b); box_paint(cr, 0, 0, MBW, MBH, &b);
	RGBA ink = cx(dark ? "#e2e8f0" : "#000000", 1);
	cairo_save(cr); rrect(cr, 1, 1, MBW - 2, MBH - 2, 9); cairo_clip(cr);
	double cx0 = 7, y = 7 - MSCROLL, cw = MBW - 14 - menu_sb();
	/* the search box (.ssbox: 2px 4px 6px; the input 29 px, 6px 8px, 13.33 px, focused: a 2 px outline of the accent colour) */
	{ double ix = cx0 + 4, iy = y + 2, iw = cw - 8, ih = 29; Box o; memset(&o, 0, sizeof o); o.bg = ca(accent(), 1); o.rad = 8; box_paint(cr, ix - 2, iy - 2, iw + 4, ih + 4, &o);
		Box in; memset(&in, 0, sizeof in); in.bg = cx(dark ? "#0b1220" : "#ffffff", 1); in.bw = 1; in.bc = cx(dark ? "#334155" : "#d1d5db", 1); in.rad = 6; box_paint(cr, ix, iy, iw, ih, &in);
		/* (2026-11-16) a search field with words: Chromium's ✕ at its right (9 px, its blue shaded down), which takes room from the words */
		if (MQ[0]) { double cxx = ix + iw - 17, cyy = iy + ih / 2 - .5; cairo_pattern_t *g = cairo_pattern_create_linear(0, cyy - 4.5, 0, cyy + 4.5);
			cairo_pattern_add_color_stop_rgb(g, 0, .25, .46, .73); cairo_pattern_add_color_stop_rgb(g, 1, .16, .28, .47); cairo_set_source(cr, g); cairo_set_line_width(cr, 2); cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
			cairo_move_to(cr, cxx - 3, cyy - 3); cairo_line_to(cr, cxx + 3, cyy + 3); cairo_move_to(cr, cxx + 3, cyy - 3); cairo_line_to(cr, cxx - 3, cyy + 3); cairo_stroke(cr); cairo_pattern_destroy(g); }
		cairo_save(cr); cairo_rectangle(cr, ix + 9, iy + 1, iw - 18 - (MQ[0] ? 13 : 0), ih - 2); cairo_clip(cr);   /* (an input's text is cut at its padding, no ellipsis) */
		double tw = css_text(cr, MQ[0] ? MQ : "Search apps, settings and files", UIFAM, 400, 40 / 3.0, ix + 9, iy + 7, 0, MQ[0] ? ink : cx("#757575", 1), 0); cairo_restore(cr);
		if (MCARET) { srca(cr, ink); cairo_rectangle(cr, round(ix + 9 + (MQ[0] ? tw : 0)), iy + 7, 1, 15); cairo_fill(cr); } }
	if (MQ[0]) { srch_draw(cr, y + 31, dark, ink); y += SRRH; }   /* (what is found, under the box: the list is hidden) */
	y += 37;
	/* the account (.mh: 600 13px, 8px 10px 10px, a line below, 4 px apart) */
	{ char h[200]; snprintf(h, sizeof h, "👤 %s%s", USER_N, !strcmp(USER_R, "admin") ? " ★" : "");
		css_text(cr, h, UIFAM, 600, 13, cx0 + 10, y + 8, 0, ink, cw - 20); srca(cr, cx(dark ? "#334155" : "#d1d5db", 1)); cairo_rectangle(cr, cx0, y + 33, cw, 1); cairo_fill(cr); y += 38; }
	/* the list */
	if (MQ[0]) { cairo_restore(cr); return TRUE; }
	double ly = y; MCONT = menu_layout(cr) + 75;
	for (int i = 0; i < NMR; i++) { MRow *r = &MR[i]; if (!r->shown) continue; double ry = ly + r->y;
		if (ry > MBH || ry + r->h < 0) continue;
		if (r->t == 'h') { srca(cr, cx(dark ? "#334155" : "#d1d5db", 1)); cairo_rectangle(cr, cx0, ry + 4, cw, 1); cairo_fill(cr); continue; }
		int hv = i == MHOV && !strstr(r->cls, "offh");
		cairo_push_group(cr);
		if (hv) { Box h; memset(&h, 0, sizeof h); h.bg = ca(accent(), 1); h.rad = 6; box_paint(cr, cx0, ry, cw, r->h, &h); }
		RGBA c = hv ? cx("#ffffff", 1) : (r->col.r == 0 && r->col.g == 0 && r->col.b == 0 && dark ? ink : r->col);
		double d, a = css_ascent(cr, UIFAM, (int)r->fw, r->fs, &d), lt = -a, lb = d; if (r->ic[0]) { lt = fmin(lt, -15); lb = fmax(lb, 5); }
		double base = ry + r->pt - lt, x = cx0 + r->pl;
		GdkPixbuf *px = r->ic[0] && ICONS ? g_hash_table_lookup(ICONS, r->ic) : NULL;
		if (px) { pix_at(cr, px, x, base + 5 - 20, 20, 0); x += 20; }
		char t[200]; snprintf(t, sizeof t, "%s%s", px ? " " : "", r->txt); if (r->up) { char *u = g_utf8_strup(t, -1); snprintf(t, sizeof t, "%s", u); g_free(u); }
		PangoLayout *pl = css_layout(cr, t, UIFAM, (int)r->fw, r->fs);
		if (r->ls) { PangoAttrList *al = pango_attr_list_new(); pango_attr_list_insert(al, pango_attr_letter_spacing_new((int)(r->ls * PANGO_SCALE))); pango_layout_set_attributes(pl, al); pango_attr_list_unref(al); }
		srca(cr, c); cairo_move_to(cr, x, round(base) - pango_layout_get_baseline(pl) / (double)PANGO_SCALE); pango_cairo_show_layout(cr, pl); g_object_unref(pl);
		cairo_pop_group_to_source(cr); cairo_paint_with_alpha(cr, r->op);
	}
	menu_sb_draw(cr);
	cairo_restore(cr);
	return TRUE;
}
static int menu_row_at(double ex, double ey)
{
	double x = ex / Z - (MBX - MX / Z), y = ey / Z - (MBY - MY / Z); if (x < 7 || x > MBW - 7 - menu_sb() || y < 1 || y > MBH - 1) return -1;
	double ly = 7 - MSCROLL + 75;
	for (int i = 0; i < NMR; i++) { MRow *r = &MR[i]; if (r->shown && r->t == 'l' && y >= ly + r->y && y < ly + r->y + r->h) return strstr(r->cls, "offh") ? -1 : i; }
	return -1;
}
static void power_dialog(void);
static void menu_do(int i)
{
	if (i < 0 || i >= NMR) return; MRow *r = &MR[i]; char a[KN]; snprintf(a, sizeof a, "%s", r->a[0] ? r->a : r->v); menu_hide();
	if (!strcmp(a, "logoff")) { power("logout"); return; }
	if (!strcmp(a, "off")) { power_dialog(); return; }
	if (!strcmp(a, "docs")) { app_open("explorer", "C:\\PROGRAMS"); return; }
	if (app_find(a)) { for (GList *l = WINS; l; l = l->next) { AppWin *w = l->data; if (!w->hidden && !strcmp(w->k, a)) { appwin_activate(w); return; } } app_open(a, NULL); return; }
	/* (the page's other actions, in a one-app window: oneapp59.js "do-NAME") */
	char k[KN + 4]; snprintf(k, sizeof k, "do-%s", a); app_open(k, NULL);
}
static gboolean menu_motion(GtkWidget *w, GdkEventMotion *e, gpointer d) { (void)w; (void)d; if (MQ[0]) { int s = srch_at(e->x, e->y); if (s != SRHOV) { SRHOV = s; gtk_widget_queue_draw(MAREA); } return TRUE; } int h = menu_row_at(e->x, e->y); if (h != MHOV) { MHOV = h; gtk_widget_queue_draw(MAREA); } return TRUE; }
static gboolean menu_leave(GtkWidget *w, GdkEventCrossing *e, gpointer d) { (void)w; (void)e; (void)d; if (SRHOV != -1) { SRHOV = -1; gtk_widget_queue_draw(MAREA); } if (MHOV != -1) { MHOV = -1; gtk_widget_queue_draw(MAREA); } return TRUE; }
static gboolean menu_click(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)w; (void)d; if (e->type != GDK_BUTTON_PRESS || e->button != 1) return TRUE;
	double x = e->x / Z - (MBX - MX / Z), y = e->y / Z - (MBY - MY / Z);
	if (x < 0 || y < 0 || x > MBW || y > MBH) { menu_hide(); return TRUE; }   /* (outside: the shadow) */
	if (MQ[0]) { srch_do(srch_at(e->x, e->y)); return TRUE; }   /* (the search's rows are not the list's li: no sound, as Standard's .ssi) */
	{ int r = menu_row_at(e->x, e->y); if (r >= 0) snd_click(); menu_do(r); } return TRUE;
}
static gboolean menu_scroll(GtkWidget *w, GdkEventScroll *e, gpointer d)
{
	(void)w; (void)d; double dy = e->direction == GDK_SCROLL_UP ? -100 : e->direction == GDK_SCROLL_DOWN ? 100 : 0;
	if (e->direction == GDK_SCROLL_SMOOTH) { double sx, sy; gdk_event_get_scroll_deltas((GdkEvent *)e, &sx, &sy); dy = sy * 100; }
	if (MQ[0]) { SRSCROLL = fmin(fmax(0, SRCH - SRRH), fmax(0, SRSCROLL + dy)); SRHOV = srch_at(e->x, e->y); gtk_widget_queue_draw(MAREA); state_write(); return TRUE; }
	double mx = fmax(0, MCONT + 14 - MBH); MSCROLL = fmin(mx, fmax(0, MSCROLL + dy)); MHOV = menu_row_at(e->x, e->y); gtk_widget_queue_draw(MAREA); state_write(); return TRUE;
}
static gboolean on_menu_key(GtkWidget *w, GdkEventKey *e, gpointer d)
{
	(void)w; (void)d;
	if (e->keyval == GDK_KEY_Escape) { menu_hide(); return TRUE; }
	if (MQ[0] && (e->keyval == GDK_KEY_Return || e->keyval == GDK_KEY_KP_Enter)) { srch_do(SRCUR); return TRUE; }
	if (MQ[0] && (e->keyval == GDK_KEY_Down || e->keyval == GDK_KEY_Up)) { SRCUR = e->keyval == GDK_KEY_Down ? MIN(NSRI - 1, SRCUR + 1) : MAX(0, SRCUR - 1); if (SRCUR < 0) SRCUR = 0; gtk_widget_queue_draw(MAREA); state_write(); return TRUE; }
	if (e->keyval == GDK_KEY_Return || e->keyval == GDK_KEY_KP_Enter) { for (int i = 0; i < NMR; i++) if (MR[i].shown && MR[i].t == 'l' && !strstr(MR[i].cls, "offh")) { menu_do(MHOV >= 0 ? MHOV : i); break; } return TRUE; }
	if (e->keyval == GDK_KEY_Down || e->keyval == GDK_KEY_Up) { int s = e->keyval == GDK_KEY_Down ? 1 : -1, i = MHOV;
		for (int n = 0; n < NMR; n++) { i = (i + s + NMR) % NMR; if (MR[i].shown && MR[i].t == 'l' && !strstr(MR[i].cls, "offh")) break; } MHOV = i; gtk_widget_queue_draw(MAREA); return TRUE; }
	if (e->keyval == GDK_KEY_BackSpace) { size_t n = strlen(MQ); if (n) { const char *p = g_utf8_find_prev_char(MQ, MQ + n); MQ[p ? p - MQ : 0] = 0; } }
	else { if (e->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)) return FALSE; gtk_im_context_filter_keypress(MIM, e); return TRUE; }
	MSCROLL = 0; MHOV = -1; srch_update(); caret_restart(); gtk_widget_queue_draw(MAREA); state_write(); return TRUE;
}
static void menu_commit(GtkIMContext *im, const char *str, gpointer d)
{
	(void)im; (void)d; size_t l = strlen(MQ), n = strlen(str); for (const char *c = str; *c; c++) if ((unsigned char)*c < 32) return;
	if (l + n < sizeof MQ) { memcpy(MQ + l, str, n); MQ[l + n] = 0; }
	MSCROLL = 0; MHOV = -1; srch_update(); caret_restart(); gtk_widget_queue_draw(MAREA); state_write();
}
/* (a key's grab, the shell's own shortcuts: item 5, takes the keyboard for a moment without moving its focus: the menu stays then) */
static gboolean menu_out_late(gpointer d)
{
	(void)d; if (!MENU || !gtk_widget_get_visible(MENU) || !gtk_widget_get_window(MENU)) return G_SOURCE_REMOVE;
	Window f = None, r, p, *k = NULL; int rv; unsigned n; XGetInputFocus(DPY, &f, &rv); Window m = GDK_WINDOW_XID(gtk_widget_get_window(MENU));
	for (Window w = f; w && w != ROOT && w != PointerRoot; ) { if (w == m) return G_SOURCE_REMOVE; if (!XQueryTree(DPY, w, &r, &p, &k, &n)) break; if (k) XFree(k); k = NULL; w = p; }
	menu_hide(); return G_SOURCE_REMOVE;
}
static gboolean on_menu_out(GtkWidget *w, GdkEvent *e, gpointer d) { (void)w; (void)e; (void)d; if (WM_OK) g_timeout_add(30, menu_out_late, NULL); else menu_hide(); return FALSE; }
static void menu_make(void)
{
	MENU = gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_window_set_decorated(GTK_WINDOW(MENU), FALSE);
	gtk_window_set_type_hint(GTK_WINDOW(MENU), GDK_WINDOW_TYPE_HINT_DIALOG); gtk_window_set_keep_above(GTK_WINDOW(MENU), TRUE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(MENU), TRUE); gtk_window_set_title(GTK_WINDOW(MENU), "Start");
	MAREA = gtk_drawing_area_new(); gtk_container_add(GTK_CONTAINER(MENU), MAREA); gtk_widget_set_can_focus(MAREA, TRUE);
	gtk_widget_add_events(MAREA, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK | GDK_KEY_PRESS_MASK);
	g_signal_connect(MAREA, "draw", G_CALLBACK(menu_draw), NULL); g_signal_connect(MAREA, "button-press-event", G_CALLBACK(menu_click), NULL);
	g_signal_connect(MAREA, "motion-notify-event", G_CALLBACK(menu_motion), NULL); g_signal_connect(MAREA, "leave-notify-event", G_CALLBACK(menu_leave), NULL);
	g_signal_connect(MAREA, "scroll-event", G_CALLBACK(menu_scroll), NULL);
	g_signal_connect(MENU, "key-press-event", G_CALLBACK(on_menu_key), NULL);
	g_signal_connect(MENU, "focus-out-event", G_CALLBACK(on_menu_out), NULL);
	g_signal_connect(MENU, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	MIM = gtk_im_context_simple_new(); g_signal_connect(MIM, "commit", G_CALLBACK(menu_commit), NULL);
	g_signal_connect(MAREA, "realize", G_CALLBACK(im_realize), MIM);
}
static void menu_toggle(void)
{
	if (!MENU) menu_make();
	if (gtk_widget_get_visible(MENU)) { menu_hide(); return; }
	MQ[0] = 0; MSCROLL = 0; MHOV = -1; int top = !strcmp(S.tb, "top");
	/* the menu's size: 230 px wide inside 6 px of padding and a 1 px border; its list's height, at most the screen less 70 px */
	{ cairo_surface_t *t = cairo_image_surface_create(CAIRO_FORMAT_A8, 1, 1); cairo_t *c = cairo_create(t); MCONT = menu_layout(c) + 75; cairo_destroy(c); cairo_surface_destroy(t); }
	double vh = (SH - TOPRES) / Z; MBW = 244; MBH = fmin(MCONT, vh - 70) + 14; MBX = 2; MBY = top ? TOPRES / Z + 50 : (SH / Z) - 48 - MBH;
	/* (body.left #menu: left var(--bw), top 4px; body.right #menu: right var(--bw); --bw is the bar's width and 4 px) */
	if (tb_side()) { MBY = TOPRES / Z + 4; MBX = tb_side() == 1 ? BARW / Z + 4 : SW / Z - (BARW / Z + 4) - MBW; }
	int mk = MENU_MON = NMON > 1 ? MONK_CUR : 0, mlx = MON[mk].x * !!mk, mty = MON[mk].y * !!mk, mrx = mk ? MON[mk].x + MON[mk].w : SW, mby = mk ? MON[mk].y + MON[mk].h : SH;   /* (1.6.1 Part 1: from another monitor's taskbar, there) */
	if (mk) { MBX += (MON[mk].x + (tb_side() == 2 ? MON[mk].w - SW : 0)) / Z; MBY += (MON[mk].y + (!top && !tb_side() ? MON[mk].h - SH : 0)) / Z; }
	MBH0 = MBH; MBY0 = MBY; NSRI = 0; srch_page();   /* (its full size, which the search's shorter menu stays inside; the page that searches) */
	/* the window: the menu and its shadow's room (32 px around, 12 px lower), on the screen */
	int x0 = (int)floor((MBX - 32) * Z), y0 = (int)floor((MBY - 20) * Z), x1 = (int)ceil((MBX + MBW + 32) * Z), y1 = (int)ceil((MBY + MBH + 44) * Z);
	MX = MAX(mlx, x0); MY = MAX(mty + TOPRES, y0); MW = MIN(mrx, x1) - MX; MH = MIN(top || tb_side() ? mby : mby - BARH, y1) - MY;
	if (MBG) g_object_unref(MBG); MBG = screen_copy(MX, MY, MW, MH);
	gtk_window_resize(GTK_WINDOW(MENU), MW, MH); gtk_widget_set_size_request(MAREA, MW, MH); gtk_window_move(GTK_WINDOW(MENU), MX, MY);
	caret_restart(); gtk_widget_show_all(MENU); gtk_window_present(GTK_WINDOW(MENU)); gtk_widget_grab_focus(MAREA); wm_restack();
	{ double sx = MBX * Z, sy = MBY * Z + MBH * Z; lp_hit_mid("start", NULL, &sx, &sy, NULL); lp_menu_open(MENU, MX, MY, LP_SMO, MBX * Z, MBY * Z, MBW * Z, MBH * Z, sx, sy, 0); }   /* (Part 17b) */
	snd_play("menu");   /* (1.6.0, the Rudolf sounds: the start menu opening, as the page's #menu shown: sndrs60.js) */
	GdkWindow *gw = gtk_widget_get_window(MENU); if (gw) gdk_window_focus(gw, GDK_CURRENT_TIME);
	state_write();
}

/* (1.6.0 Part 12, "The rule") Start › Shut Down...: the page's Power dialog (core25.js shutdownDlg, .rmodal/.rmbox): the
   screen dimmed (rgba(15,23,42,.45)), a white box in the middle with its shadow, "Power", the question and four
   buttons (Shut down, Restart, Sleep, Cancel), the first one focused (a 2 px ring of the accent colour); Tab moves the
   focus, Enter or a click chooses. Shut down and Restart end the session first, Sleep locks the screen. */
/* (1.6.0) and, as boot30.js adds it, "⊞ Restart into Windows" after Restart when the firmware has a Windows entry (the Vivobook):
   the page's own steps (#app=do-pwr: the administrator's password, the firmware asked to start Windows once), then the restart */
static GtkWidget *PWRA; static GdkPixbuf *PWRBG; static int PWRF, PWRH = -1, NPWR = 4; static double PWRB[5][4]; static char PWRWN[16], PWRWT[128];
static const char *PWRT[5] = {"⏻ Shut down", "↻ Restart", "☾ Sleep", "Cancel"}, *PWRK[5] = {"poweroff", "reboot", "suspend", "x"};
static void pwr_rows(void)
{
	static const char *const T4[4] = {"⏻ Shut down", "↻ Restart", "☾ Sleep", "Cancel"}, *const K4[4] = {"poweroff", "reboot", "suspend", "x"};
	PWRWN[0] = 0; JV *m = jget("/api/bootmenu"), *w = jbool(m, "uefi") ? jprop(m, "windows") : NULL, *w0 = w && w->t == 4 ? jat(w, 0) : NULL;
	if (w0) { jstr(w0, "n", PWRWN, sizeof PWRWN); jstr(w0, "title", PWRWT, sizeof PWRWT); if (strspn(PWRWN, "0123456789ABCDEFabcdef") != strlen(PWRWN)) PWRWN[0] = 0; }
	jfree(m); NPWR = 0;
	for (int i = 0; i < 4; i++) { PWRT[NPWR] = T4[i]; PWRK[NPWR++] = K4[i]; if (i == 1 && PWRWN[0]) { PWRT[NPWR] = "⊞ Restart into Windows"; PWRK[NPWR++] = "windows"; } }
}
static void pwr_geom(cairo_t *cr, double *bx, double *by, double *bw, double *bh)
{
	double W = SW / Z, H = SH / Z, x = 0; for (int i = 0; i < NPWR; i++) { double w = css_width(cr, PWRT[i], UIFAM, 400, 13) + 22; PWRB[i][0] = x; PWRB[i][2] = w; PWRB[i][3] = 24; x += w + (i < NPWR - 1 ? 8 : 0); }
	*bw = x + 40; *bh = 18 + 18 + 13 + 15 + 14 + 24 + 6 + 15.59 + 18; *bx = (W - *bw) / 2; *by = (H - *bh) / 2;
	for (int i = 0; i < NPWR; i++) { PWRB[i][0] += *bx + 20; PWRB[i][1] = *by + 18 + 18 + 13 + 15 + 14; }
}
static gboolean pwr_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)w; (void)d; if (PWRBG) { gdk_cairo_set_source_pixbuf(cr, PWRBG, 0, 0); cairo_paint(cr); }
	cairo_set_source_rgba(cr, 15 / 255.0, 23 / 255.0, 42 / 255.0, .45); cairo_paint(cr); cairo_scale(cr, Z, Z);
	double bx, by, bw, bh; pwr_geom(cr, &bx, &by, &bw, &bh);
	{ int pad = 70; cairo_surface_t *m = cairo_image_surface_create(CAIRO_FORMAT_A8, (int)ceil((bw + 2 * pad) * Z), (int)ceil((bh + 2 * pad) * Z)); cairo_t *mc = cairo_create(m);
		cairo_scale(mc, Z, Z); rrect(mc, pad, pad, bw, bh, 12); cairo_set_source_rgba(mc, 0, 0, 0, 1); cairo_fill(mc); cairo_destroy(mc); a8_blur(m, 25 * Z);
		cairo_save(cr); cairo_scale(cr, 1 / Z, 1 / Z); cairo_set_source_rgba(cr, 0, 0, 0, .4); cairo_mask_surface(cr, m, (bx - pad) * Z, (by - pad + 20) * Z); cairo_restore(cr); cairo_surface_destroy(m); }
	Box b; memset(&b, 0, sizeof b); b.bg = cx("#ffffff", 1); b.rad = 12; hc_edge(&b); box_paint(cr, bx, by, bw, bh, &b);
	RGBA ink = cx("#111827", 1);
	css_text(cr, "Power", UIFAM, 700, 15.21, bx + 20, by + 18, 18, ink, 0);
	css_text(cr, "What do you want the computer to do?", UIFAM, 400, 13, bx + 20, by + 18 + 18 + 13, 15, ink, 0);
	for (int i = 0; i < NPWR; i++) { double *r = PWRB[i];
		if (i == PWRF) { Box o; memset(&o, 0, sizeof o); o.bg = ca(accent(), 1); o.rad = 6; box_paint(cr, r[0] - 2, r[1] - 2, r[2] + 4, r[3] + 4, &o); }
		Box k; memset(&k, 0, sizeof k); k.bg = cx(i == PWRH ? "#eaf0ff" : "#ffffff", 1); k.bw = 1; k.bc = cx(i == PWRH ? "#a9bff5" : "#d1d5db", 1); k.rad = 4; box_paint(cr, r[0], r[1], r[2], r[3], &k);
		double tw = css_width(cr, PWRT[i], UIFAM, 400, 13); css_text(cr, PWRT[i], UIFAM, 400, 13, r[0] + (r[2] - tw) / 2, r[1] + 1, 22, cx("#000000", 1), 0); }
	return TRUE;
}
static int pwr_at(double x, double y) { x /= Z; y /= Z; for (int i = 0; i < NPWR; i++) if (x >= PWRB[i][0] && x < PWRB[i][0] + PWRB[i][2] && y >= PWRB[i][1] && y < PWRB[i][1] + PWRB[i][3]) return i; return -1; }
static void pwr_close(void) { if (PWR) gtk_widget_hide(PWR); if (PWRBG) { g_object_unref(PWRBG); PWRBG = NULL; } state_write(); }
static void pwr_do(int i)
{
	if (i < 0) return; pwr_close(); const char *a = PWRK[i];
	if (!strcmp(a, "x")) return;
	if (!strcmp(a, "windows")) { char *t = g_uri_escape_string(PWRWT, NULL, FALSE), ex[300]; snprintf(ex, sizeof ex, "&op=windows&n=%s&t=%s", PWRWN, t); g_free(t); app_open_ex("do-pwr", NULL, ex, 0); return; }
	if (!strcmp(a, "suspend")) { power("lock"); char p[80]; snprintf(p, sizeof p, "/api/power?action=suspend"); http("POST", p, NULL, 0, NULL); return; }
	power(a);
}
static gboolean pwr_motion(GtkWidget *w, GdkEventMotion *e, gpointer d) { (void)w; (void)d; int h = pwr_at(e->x, e->y); if (h != PWRH) { PWRH = h; gtk_widget_queue_draw(PWRA); } return TRUE; }
static gboolean pwr_click(GtkWidget *w, GdkEventButton *e, gpointer d) { (void)w; (void)d; if (e->type == GDK_BUTTON_PRESS && e->button == 1) { int i = pwr_at(e->x, e->y); if (i >= 0) snd_click(); pwr_do(i); } return TRUE; }
static gboolean pwr_key(GtkWidget *w, GdkEventKey *e, gpointer d)
{
	(void)w; (void)d;
	if (e->keyval == GDK_KEY_Tab || e->keyval == GDK_KEY_ISO_Left_Tab || e->keyval == GDK_KEY_Right || e->keyval == GDK_KEY_Left) { int back = e->keyval == GDK_KEY_ISO_Left_Tab || e->keyval == GDK_KEY_Left || (e->state & GDK_SHIFT_MASK);
		PWRF = (PWRF + (back ? NPWR - 1 : 1)) % NPWR; gtk_widget_queue_draw(PWRA); return TRUE; }
	if (e->keyval == GDK_KEY_Escape) { pwr_do(NPWR - 1); return TRUE; }   /* (Cancel) */
	if (e->keyval == GDK_KEY_Return || e->keyval == GDK_KEY_KP_Enter || e->keyval == GDK_KEY_space) { pwr_do(PWRF); return TRUE; }
	return TRUE;
}
static void power_dialog(void)
{
	if (!PWR) { PWR = gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_window_set_decorated(GTK_WINDOW(PWR), FALSE); gtk_window_set_keep_above(GTK_WINDOW(PWR), TRUE);
		gtk_window_set_title(GTK_WINDOW(PWR), "Power"); gtk_window_set_skip_taskbar_hint(GTK_WINDOW(PWR), TRUE);
		PWRA = gtk_drawing_area_new(); gtk_container_add(GTK_CONTAINER(PWR), PWRA); gtk_widget_set_can_focus(PWRA, TRUE);
		gtk_widget_add_events(PWRA, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_KEY_PRESS_MASK);
		g_signal_connect(PWRA, "draw", G_CALLBACK(pwr_draw), NULL); g_signal_connect(PWRA, "button-press-event", G_CALLBACK(pwr_click), NULL);
		g_signal_connect(PWRA, "motion-notify-event", G_CALLBACK(pwr_motion), NULL); g_signal_connect(PWR, "key-press-event", G_CALLBACK(pwr_key), NULL);
		g_signal_connect(PWR, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL); }
	pwr_rows(); menu_hide(); fl_close();
	if (PWRBG) g_object_unref(PWRBG); PWRBG = screen_copy(0, 0, SW, SH);
	PWRF = 0; PWRH = -1; gtk_window_move(GTK_WINDOW(PWR), 0, 0); gtk_window_resize(GTK_WINDOW(PWR), SW, SH); gtk_widget_set_size_request(PWRA, SW, SH);
	gtk_widget_show_all(PWR); gtk_window_present(GTK_WINDOW(PWR)); gtk_widget_grab_focus(PWRA); wm_restack();
	GdkWindow *gw = gtk_widget_get_window(PWR); if (gw) gdk_window_focus(gw, GDK_CURRENT_TIME);
	state_write();
}

/* ------------------------------------------------------------------ the sign-in and lock screens
 * (1.6.0 Part 12, "The rule") drawn as the page draws them: the sign-in screen (base_v24.html #login, lock58.js: the
 * wallpaper 30 % darker, the time and the date at the top, the see-through card in the middle with the logo, "Rudolf
 * OS", the user name and the password, Log in and Guest, the power button at the bottom right) and the lock screen
 * (core25.js lockScreen: the time and the date over a dark box with 👤, the name, the password, Switch user and
 * Unlock). Passwords show as the page's *s. Tab moves between the fields and buttons, Enter signs in. */
static GtkWidget *SAREA; static GtkIMContext *SIM;
static int SIGN_LOCK, SIGN_BUSY, SFOC, SHOV = -1, SCARET; static guint SCARET_T;   /* SFOC: 0 the user name, 1 the password, 2 Log in/Unlock, 3 Guest/Switch user, 4 the power button */
static char SN_NAME[128], SN_PASS[256], SN_MSG[300];
/* (1.6.0, plan-1.6.1 Part 17: authenticator codes) rudolfd asked for the code after a right password: the password kept
   here for the second request, the field taking the code, as the page's code box after the password */
static int SN_CODE; static char SN_PW1[256];
static int SN_KEY;   /* 1.6.1 (plan-1.6.1 Part 18): a security key asked for after the password (rudolfd-fido.inc) */
static void sn_code_off(void) { SN_CODE = 0; SN_KEY = 0; explicit_bzero(SN_PW1, sizeof SN_PW1); }
static double SRECT[5][4];   /* each focusable part's place (the page's units) */
static void settings_load(void);
static void apps_load(void);
static gboolean sign_blink(gpointer d) { (void)d; SCARET = !SCARET; if (SAREA) gtk_widget_queue_draw(SAREA); return G_SOURCE_CONTINUE; }
static void sign_caret(void) { SCARET = 1; if (SCARET_T) g_source_remove(SCARET_T); SCARET_T = g_timeout_add(500, sign_blink, NULL); if (SAREA) gtk_widget_queue_draw(SAREA); }
static void stars(char *o, size_t n, const char *pw) { size_t k = g_utf8_strlen(pw, -1); if (k > n - 1) k = n - 1; memset(o, '*', k); o[k] = 0; }
/* writing with CSS's text-shadow (one shadow: dx, dy, blur, colour) */
static void text_shadowed(cairo_t *cr, const char *t, const char *fam, int wt, double px, double x, double top, double lh, RGBA c, double dy, double blur, RGBA sc)
{
	double tw = css_width(cr, t, fam, wt, px), a, d; a = css_ascent(cr, fam, wt, px, &d); if (lh <= 0) lh = a + d;
	int pad = (int)ceil(blur * 2 + 4); cairo_surface_t *m = cairo_image_surface_create(CAIRO_FORMAT_A8, (int)ceil((tw + 2 * pad) * Z), (int)ceil((lh + 2 * pad) * Z)); cairo_t *mc = cairo_create(m);
	cairo_scale(mc, Z, Z); css_text(mc, t, fam, wt, px, pad, pad, lh, cx("#000000", 1), 0); cairo_destroy(mc); a8_blur(m, blur / 2 * Z);
	cairo_save(cr); cairo_scale(cr, 1 / Z, 1 / Z); srca(cr, sc); cairo_mask_surface(cr, m, round((x - pad) * Z), round((top - pad + dy) * Z)); cairo_restore(cr); cairo_surface_destroy(m);
	css_text(cr, t, fam, wt, px, x, top, lh, c, 0);
}
static void sign_clock_texts(char *t, size_t tn, char *d, size_t dn)
{
	GTimeZone *tz = S.tz[0] ? g_time_zone_new_identifier(S.tz) : NULL; if (!tz) tz = g_time_zone_new_local();
	GDateTime *n0 = g_date_time_new_now(tz), *n = g_date_time_add_seconds(n0, S.doff / 1000.0); int h = g_date_time_get_hour(n), m = g_date_time_get_minute(n);
	if (S.h24) snprintf(t, tn, "%02d:%02d", h, m); else snprintf(t, tn, "%d:%02d %s", h % 12 ? h % 12 : 12, m, h < 12 ? "AM" : "PM");
	char *s = g_date_time_format(n, "%A, %B %-d"); snprintf(d, dn, "%s", s ? s : ""); g_free(s);
	g_date_time_unref(n); g_date_time_unref(n0); g_time_zone_unref(tz);
}
#define LOGINF "Tahoma, Arial, sans-serif"
/* a focus ring: CSS's outline, 2 px, OFF px outside the box */
static void focus_ring(cairo_t *cr, double x, double y, double w, double h, double r, double off, RGBA c)
{
	snap(&x, &y, &w, &h); double o = off + 2;
	rrect(cr, x - o, y - o, w + 2 * o, h + 2 * o, r + o); rrect(cr, x - off, y - off, w + 2 * off, h + 2 * off, r + off);
	cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD); srca(cr, c); cairo_fill(cr); cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
}
static void sign_input(cairo_t *cr, double x, double y, double w, double h, double r, double bw, RGBA bc, const char *fam, double px, double padl, const char *val, const char *ph, int foc, RGBA ring)
{
	if (foc) focus_ring(cr, x, y, w, h, r, SIGN_LOCK ? 2 : 0, ring);
	Box b; memset(&b, 0, sizeof b); b.bg = cx("#ffffff", 1); b.bw = bw; b.bc = bc; b.rad = r; box_paint(cr, x, y, w, h, &b);
	double a, d; a = css_ascent(cr, fam, 400, px, &d); double ty = y + (h - (a + d)) / 2;
	cairo_save(cr); cairo_rectangle(cr, x + bw + padl, y, w - 2 * (bw + padl), h); cairo_clip(cr);
	double tw = val[0] ? css_text(cr, val, fam, 400, px, x + bw + padl, ty, a + d, cx("#111111", 1), 0) : ph ? (css_text(cr, ph, fam, 400, px, x + bw + padl, ty, a + d, cx("#757575", 1), 0), 0) : 0;
	cairo_restore(cr);
	if (foc && SCARET) { cairo_rectangle(cr, round(x + bw + padl + tw), round(ty), 1, a + d); srca(cr, cx("#111111", 1)); cairo_fill(cr); }
}
static void sign_button(cairo_t *cr, double *r, const char *t, const char *fam, double px, RGBA bg, RGBA fg, double bw, RGBA bc, double rad, int foc, int hov)
{
	if (foc) focus_ring(cr, r[0], r[1], r[2], r[3], rad, SIGN_LOCK ? 2 : 0, SIGN_LOCK ? ca(accent(), 1) : cx("#60a5fa", 1));
	Box b; memset(&b, 0, sizeof b); b.bg = bg; b.bw = bw; b.bc = bc; b.rad = rad; if (hov) b.bg = cmix(bg, .88, cx("#000000", 1)); box_paint(cr, r[0], r[1], r[2], r[3], &b);
	double tw = css_width(cr, t, fam, 400, px); css_text(cr, t, fam, 400, px, r[0] + (r[2] - tw) / 2, r[1] + bw, r[3] - 2 * bw, fg, 0);
}
static gboolean signin_area_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)d; desk_draw(w, cr, NULL); cairo_set_source_rgba(cr, 0, 0, 0, .3); cairo_paint(cr);
	cairo_scale(cr, Z, Z); double W = SW / Z, H = (SH - TOPRES) / Z; cairo_translate(cr, 0, TOPRES / Z);
	char ct[32], cd[64]; sign_clock_texts(ct, sizeof ct, cd, sizeof cd); char st[256]; if (SN_CODE) snprintf(st, sizeof st, "%s", SN_PASS); else stars(st, sizeof st, SN_PASS);   /* (the code is shown as typed) */
	if (!SIGN_LOCK) {
		/* the time and the date (.lkck: 6 % from the top; 72px weight 200 and 18px, a shadow 0 2px 8px #0008) */
		double t0 = H * .06; RGBA sh = cx("#000000", .533);
		text_shadowed(cr, ct, LOGINF, 200, 72, (W - css_width(cr, ct, LOGINF, 200, 72)) / 2, t0, 82, cx("#ffffff", 1), 2, 8, sh);
		{ double tw = css_width(cr, cd, LOGINF, 400, 18); cairo_push_group(cr); text_shadowed(cr, cd, LOGINF, 400, 18, (W - tw) / 2, t0 + 82, 21, cx("#ffffff", 1), 2, 8, sh); cairo_pop_group_to_source(cr); cairo_paint_with_alpha(cr, .8); }
		/* the card: 370 px, 26px 24px inside, see-through white with a light edge */
		double ch = GUEST_OK ? 456 : 405, x0 = (W - 370) / 2, y0 = (H - ch) / 2, ix = x0 + 25, iw = 320;
		Box c; memset(&c, 0, sizeof c); c.bg = cx("#ffffff", .08); c.bw = 1; c.bc = cx("#ffffff", .19); c.rad = 14; box_paint(cr, x0, y0, 370, ch, &c);
		pix_at(cr, LOGO84 ? LOGO84 : LOGO, x0 + (370 - 84) / 2, y0 + 27, 84, 0);
		double tw = css_width(cr, "Rudolf OS", LOGINF, 700, 22); css_text(cr, "Rudolf OS", LOGINF, 700, 22, ix + (iw - tw) / 2, y0 + 119, 26, cx("#ffffff", 1), 0);
		tw = css_width(cr, "Runtime Driver Open Linux Framework", LOGINF, 400, 11); css_text(cr, "Runtime Driver Open Linux Framework", LOGINF, 400, 11, ix + (iw - tw) / 2, y0 + 149, 12, cx("#6b7280", 1), 0);
		css_text(cr, "User name", LOGINF, 400, 12, ix, y0 + 179, 14, cx("#ffffff", .85), 0);
		SRECT[0][0] = ix; SRECT[0][1] = y0 + 201; SRECT[0][2] = iw; SRECT[0][3] = 35; sign_input(cr, ix, y0 + 201, iw, 35, 6, 0, CLEAR, LOGINF, 15, 10, SN_NAME, NULL, SFOC == 0, cx("#60a5fa", 1));
		css_text(cr, SN_CODE ? "Code from your authenticator app" : "Password", LOGINF, 400, 12, ix, y0 + 244, 14, cx("#ffffff", .85), 0);
		SRECT[1][0] = ix; SRECT[1][1] = y0 + 266; SRECT[1][2] = iw; SRECT[1][3] = 35; sign_input(cr, ix, y0 + 266, iw, 35, 6, 0, CLEAR, LOGINF, 15, 10, st, NULL, SFOC == 1, cx("#60a5fa", 1));
		if (SN_MSG[0]) css_text(cr, SN_MSG, LOGINF, 400, 13, ix, y0 + 309, 18, cx("#fecaca", 1), iw);
		double *r = SRECT[2]; r[0] = ix; r[1] = y0 + 341; r[2] = iw; r[3] = 37; sign_button(cr, r, "Log in", LOGINF, 15, ca(accent(), 1), cx("#ffffff", 1), 0, CLEAR, 6, SFOC == 2, SHOV == 2);
		r = SRECT[3]; if (GUEST_OK) { r[0] = ix + 6; r[1] = y0 + 392; r[2] = iw - 6; r[3] = 37; sign_button(cr, r, "Guest", LOGINF, 15, ca(accent(), 1), cx("#ffffff", 1), 0, CLEAR, 6, SFOC == 3, SHOV == 3); } else r[2] = 0;
		/* the power button: a 48 px circle 22 px from the corner */
		r = SRECT[4]; r[0] = W - 70; r[1] = H - 70; r[2] = r[3] = 48;
		{ Box p; memset(&p, 0, sizeof p); p.bg = cx("#ffffff", SHOV == 4 ? .22 : .12); p.bw = 1; p.bc = cx("#ffffff", .25); p.rad = 24; box_paint(cr, r[0], r[1], 48, 48, &p);
			double pw = css_width(cr, "⏻", LOGINF, 400, 22); css_text(cr, "⏻", LOGINF, 400, 22, r[0] + (48 - pw) / 2, r[1] + 1, 46, cx("#ffffff", 1), 0); }
	} else {
		/* the lock screen: the time, the date and the box, as one column in the middle */
		double top = (H - 373.59) / 2, ink_a = 1; RGBA ink = cx("#e2e8f0", ink_a); const char *F = UIFAM;
		css_text(cr, ct, F, 200, 72, (W - css_width(cr, ct, F, 200, 72)) / 2, top, 84, ink, 0);
		{ double tw = css_width(cr, cd, F, 400, 18); css_text(cr, cd, F, 400, 18, (W - tw) / 2, top + 90, 21, (RGBA){ink.r, ink.g, ink.b, .8}, 0); }
		double bx = (W - 324) / 2, by = top + 147, ix = bx + 22, iw = 280;
		Box b; memset(&b, 0, sizeof b); b.bg = cx("#0f172a", .8); b.rad = 16; box_paint(cr, bx, by, 324, 226.59, &b);
		double tw = css_width(cr, "👤", F, 400, 54); css_text(cr, "👤", F, 400, 54, ix + (iw - tw) / 2, by + 22, 63, ink, 0);
		tw = css_width(cr, USER_N, F, 400, 20); css_text(cr, USER_N, F, 400, 20, ix + (iw - tw) / 2, by + 91, 24, ink, iw);
		SRECT[1][0] = ix; SRECT[1][1] = by + 123; SRECT[1][2] = iw; SRECT[1][3] = 23; SRECT[0][2] = 0;
		sign_input(cr, ix, by + 123, iw, 23, 4, 1, cx("#d1d5db", 1), F, 40 / 3.0, 6, st, SN_CODE ? "Code from your authenticator app" : "Password", SFOC == 1, ca(accent(), 1));
		if (SN_MSG[0]) css_text(cr, SN_MSG, F, 400, 13, ix, by + 152, 15.59, cx("#fca5a5", 1), iw);
		double uw = css_width(cr, "Unlock", F, 400, 13) + 22, sw = css_width(cr, "Switch user", F, 400, 13) + 22;
		double *r = SRECT[2]; r[0] = ix + iw - uw; r[1] = by + 181.6; r[2] = uw; r[3] = 23; sign_button(cr, r, "Unlock", F, 13, cx("#2563eb", 1), cx("#ffffff", 1), 1, cx("#2563eb", 1), 4, SFOC == 2, SHOV == 2);
		r = SRECT[3]; r[0] = SRECT[2][0] - 8 - sw; r[1] = by + 181.6; r[2] = sw; r[3] = 23; sign_button(cr, r, "Switch user", F, 13, cx(SHOV == 3 ? "#eaf0ff" : "#ffffff", 1), cx("#000000", 1), 1, cx(SHOV == 3 ? "#a9bff5" : "#d1d5db", 1), 4, SFOC == 3, 0);
		SRECT[4][2] = 0;
	}
	return TRUE;
}
/* (1.6.0, the owner's report 5) what Standard's desktop page does once at each sign-in (the notice after an update), done by a page
   unseen, not by every app window that opens: (1.6.1) the shell's one hidden page with &start=1 (oneapp59.js oaStartRun; #app=do-start
   before), opened now, at the sign-in, as the start-up page was; for a guest it opens as the service page did (bg_tick) */
static int BG_START; static void bg_open(void);
static void start_page(void) { if (USER_ID[0] && strcmp(USER_R, "guest")) { BG_START = 1; if (!bg_page()) bg_open(); } }
typedef struct { char *body, *path; int code; char *reply; } Job;
/* 1.6.1 (plan-1.6.1 Part 18): the password was right and the account has security keys: the password kept for the next
   request, the screen says to touch the key (in the page's words, fido61.js) and that request (with "k") waits for the
   touch; after a wrong key or no touch, Sign in (or Enter) asks again */
static void signin_go(int guest);
static gboolean sn_key_again(gpointer p) { (void)p; signin_go(0); return G_SOURCE_REMOVE; }
static void sn_key_step(int first)
{
	if (!SN_KEY) { SN_KEY = 1; SN_CODE = 0; snprintf(SN_PW1, sizeof SN_PW1, "%s", SN_PASS); }
	if (first) { snprintf(SN_MSG, sizeof SN_MSG, "%s: touch your security key (any of yours). If it is not plugged in, plug it in first.", SIGN_LOCK ? USER_N : SN_NAME); g_idle_add(sn_key_again, NULL); }
}
static gboolean signin_done(gpointer p)
{
	Job *j = p; SIGN_BUSY = 0;
	JV *v = jparse(j->reply);
	if (j->code == 200) {
		snd_play("login");   /* (as the page's sign-in and unlock: report 2) */
		if (!SIGN_LOCK) { jstr(v, "id", USER_ID, sizeof USER_ID); jstr(v, "n", USER_N, sizeof USER_N); jstr(v, "r", USER_R, sizeof USER_R); WEB_DIR[0] = 0; jstr(v, "web", WEB_DIR, sizeof WEB_DIR); }
		if (SIGN_LOCK && LOCKED) UNLOCK_T = g_get_monotonic_time();   /* (UNLOCKED_AT: no saver or lock for a minute: item 6) */
		LOCKED = 0; gtk_widget_hide(SIGN); explicit_bzero(SN_PASS, sizeof SN_PASS); sn_code_off(); SN_MSG[0] = 0; if (SCARET_T) { g_source_remove(SCARET_T); SCARET_T = 0; }
		if (!SIGN_LOCK) ICONS_SINCE = g_get_monotonic_time();
		settings_load(); apps_load(); if (BAR) gtk_widget_show_all(BAR); wm_restack();
		if (!SIGN_LOCK) { svc_reset(); SVC_SIGNIN = g_get_real_time() / 1000; start_page(); }   /* (1.6.1: the time first: the hidden page's since=) */
		if (WM_OK) wm_hide_all(0); else for (GList *l = WINS; l; l = l->next) gtk_widget_show(((AppWin *)l->data)->win);
	} else {
		char m[300] = "", nd[16] = ""; jstr(v, "error", m, sizeof m); jstr(v, "need", nd, sizeof nd);
		snprintf(SN_MSG, sizeof SN_MSG, "%s", m[0] ? m : j->code ? "The sign-in did not work." : "Rudolf OS's system service did not answer.");
		/* (1.6.0: the code is asked for: the password kept for the second request, the field cleared for the code) */
		if (!strcmp(nd, "key") && j->code != 429) sn_key_step(j->code == 401);
		else if (!strcmp(nd, "code") && j->code != 429) { if (!SN_CODE) { SN_CODE = 1; snprintf(SN_PW1, sizeof SN_PW1, "%s", SN_PASS); } }
		else sn_code_off();
		explicit_bzero(SN_PASS, sizeof SN_PASS); SFOC = 1; sign_caret();
		if (j->code && !(SN_KEY && j->code == 401)) snd_play("error");   /* (a wrong password, as the page's; not the key's first prompt) */
	}
	jfree(v);
	if (SAREA) gtk_widget_queue_draw(SAREA); state_write();
	g_free(j->reply); g_free(j->path); if (j->body) { explicit_bzero(j->body, strlen(j->body)); g_free(j->body); } g_free(j); return G_SOURCE_REMOVE;
}
static gpointer signin_thread(gpointer p)
{
	Job *j = p; GByteArray *b; j->code = http("POST", j->path, j->body, strlen(j->body), &b);
	j->reply = b ? (char *)g_byte_array_free(b, FALSE) : NULL; g_idle_add(signin_done, j); return NULL;
}
static void json_esc(GString *s, const char *v)
{
	g_string_append_c(s, '"');
	for (const unsigned char *c = (const unsigned char *)v; *c; c++) { if (*c == '"' || *c == '\\') { g_string_append_c(s, '\\'); g_string_append_c(s, *c); } else if (*c < 0x20) g_string_append_printf(s, "\\u%04x", *c); else g_string_append_c(s, *c); }
	g_string_append_c(s, '"');
}
static void signin_go(int guest)
{
	if (SIGN_BUSY) return; SIGN_BUSY = 1; SN_MSG[0] = 0;
	Job *j = g_new0(Job, 1); GString *b = g_string_new("{");
	if (guest) g_string_append(b, "\"guest\":true");
	else { if (!SIGN_LOCK) { g_string_append(b, "\"n\":"); json_esc(b, SN_NAME); g_string_append_c(b, ','); }
		if (SN_KEY) { g_string_append(b, "\"p\":"); json_esc(b, SN_PW1); g_string_append(b, ",\"k\":true"); }   /* (1.6.1: waits for the touch) */
		else if (SN_CODE) { g_string_append(b, "\"p\":"); json_esc(b, SN_PW1); g_string_append(b, ",\"c\":"); json_esc(b, SN_PASS); }
		else { g_string_append(b, "\"p\":"); json_esc(b, SN_PASS); } }
	g_string_append_c(b, '}'); j->body = g_string_free(b, FALSE); j->path = g_strdup(SIGN_LOCK ? "/api/desktop/unlock" : "/api/desktop/login");
	g_thread_unref(g_thread_new("signin", signin_thread, j));
}
static int sign_at(double x, double y) { x /= Z; y = (y - TOPRES) / Z; for (int i = 0; i < 5; i++) { double *r = SRECT[i]; if (r[2] > 0 && x >= r[0] && x < r[0] + r[2] && y >= r[1] && y < r[1] + r[3]) return i; } return -1; }
static void sign_act(int i)
{
	if (i == 2) signin_go(0);
	else if (i == 3) { if (SIGN_LOCK) power("logout"); else signin_go(1); }
	else if (i == 4) power_dialog();
}
static gboolean sign_click(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)w; (void)d; if (e->type != GDK_BUTTON_PRESS || e->button != 1) return TRUE; int i = sign_at(e->x, e->y);
	if (i == 0 || i == 1) { SFOC = i; sign_caret(); } else { if (SIGN_LOCK && (i == 2 || i == 3)) snd_click(); sign_act(i); }   /* (the lock screen's Unlock and Switch user are buttons; #login's none) */
	return TRUE;
}
static gboolean sign_motion(GtkWidget *w, GdkEventMotion *e, gpointer d) { (void)w; (void)d; int h = sign_at(e->x, e->y); if (h != SHOV) { SHOV = h; gtk_widget_queue_draw(SAREA); } return TRUE; }
static gboolean sign_key(GtkWidget *w, GdkEventKey *e, gpointer d)
{
	(void)w; (void)d; int n0 = SIGN_LOCK ? 1 : 0, n1 = SIGN_LOCK ? 3 : (GUEST_OK ? 4 : 4);
	if (e->keyval == GDK_KEY_Tab || e->keyval == GDK_KEY_ISO_Left_Tab) { int back = e->keyval == GDK_KEY_ISO_Left_Tab || (e->state & GDK_SHIFT_MASK);
		do { SFOC += back ? -1 : 1; if (SFOC > n1) SFOC = n0; if (SFOC < n0) SFOC = n1; } while (SRECT[SFOC][2] <= 0); sign_caret(); return TRUE; }
	if (e->keyval == GDK_KEY_Return || e->keyval == GDK_KEY_KP_Enter) { if (SFOC <= 1) signin_go(0); else sign_act(SFOC); return TRUE; }
	if (e->keyval == GDK_KEY_space && SFOC >= 2) { sign_act(SFOC); return TRUE; }
	if (SFOC > 1) return TRUE;
	char *t = SFOC == 0 ? SN_NAME : SN_PASS;
	if (e->keyval == GDK_KEY_BackSpace) { size_t n = strlen(t); if (n) { const char *p = g_utf8_find_prev_char(t, t + n); t[p ? p - t : 0] = 0; } sign_caret(); return TRUE; }
	if (e->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)) return TRUE;
	gtk_im_context_filter_keypress(SIM, e); return TRUE;
}
static void sign_commit(GtkIMContext *im, const char *str, gpointer d)
{
	(void)im; (void)d; if (SFOC > 1) return; char *t = SFOC == 0 ? SN_NAME : SN_PASS; size_t cap = SFOC == 0 ? sizeof SN_NAME : sizeof SN_PASS, l = strlen(t), n = strlen(str);
	for (const char *c = str; *c; c++) if ((unsigned char)*c < 32) return;
	if (l + n < cap) { memcpy(t + l, str, n); t[l + n] = 0; } sign_caret(); state_write();
}
static void signin_make(void)
{
	SIGN = gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_window_set_decorated(GTK_WINDOW(SIGN), FALSE); gtk_window_set_title(GTK_WINDOW(SIGN), "Sign in");
	gtk_window_set_keep_above(GTK_WINDOW(SIGN), TRUE); gtk_window_set_default_size(GTK_WINDOW(SIGN), SW, SH); gtk_window_move(GTK_WINDOW(SIGN), 0, 0);
	SAREA = gtk_drawing_area_new(); gtk_widget_set_can_focus(SAREA, TRUE); gtk_container_add(GTK_CONTAINER(SIGN), SAREA);
	gtk_widget_add_events(SAREA, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_KEY_PRESS_MASK);
	g_signal_connect(SAREA, "draw", G_CALLBACK(signin_area_draw), NULL); g_signal_connect(SAREA, "button-press-event", G_CALLBACK(sign_click), NULL);
	g_signal_connect(SAREA, "motion-notify-event", G_CALLBACK(sign_motion), NULL); g_signal_connect(SIGN, "key-press-event", G_CALLBACK(sign_key), NULL);
	g_signal_connect(SIGN, "delete-event", G_CALLBACK(gtk_true), NULL);
	SIM = gtk_im_context_simple_new(); g_signal_connect(SIM, "commit", G_CALLBACK(sign_commit), NULL);
	g_signal_connect(SAREA, "realize", G_CALLBACK(im_realize), SIM);
}
static void accounts_load(void)
{
	JV *v = jget("/api/desktop/accounts"); NACC = 0; GUEST_OK = 0; SETUP = 0; LASTACC[0] = 0;
	if (!v) return;
	GUEST_OK = jbool(v, "guest"); SETUP = jbool(v, "setup"); jstr(v, "last", LASTACC, sizeof LASTACC);
	JV *a = jprop(v, "accounts");
	for (int i = 0; i < 32; i++) { JV *o = jat(a, i); if (!o) break; Acct *c = &ACC[NACC++]; memset(c, 0, sizeof *c);
		jstr(o, "id", c->id, sizeof c->id); jstr(o, "n", c->n, sizeof c->n); jstr(o, "r", c->r, sizeof c->r); c->key = jbool(o, "key"); c->old = jbool(o, "old");  }
	 jfree(v);
}
static void signin_show(int lock)
{
	if (!SIGN) signin_make();
	sn_code_off();   /* (1.6.0: a new sign-in starts at the password) */
	SIGN_LOCK = lock; LOCKED = lock; menu_hide(); if (PWR) gtk_widget_hide(PWR); vk_sync();   /* (1.6.0: not the on-screen keyboard over the lock screen) */
	if (lock) { if (WM_OK) wm_hide_all(1); else for (GList *l = WINS; l; l = l->next) gtk_widget_hide(((AppWin *)l->data)->win); }
	if (!lock) { USER_ID[0] = 0; settings_load(); }
	accounts_load();
	/* the user name: the account signed in last (the page's: the last USER) */
	const char *pre = ""; for (int i = 0; i < NACC; i++) if (!strcmp(ACC[i].id, LASTACC)) pre = ACC[i].n;
	snprintf(SN_NAME, sizeof SN_NAME, "%s", lock ? USER_N : pre); explicit_bzero(SN_PASS, sizeof SN_PASS);
	snprintf(SN_MSG, sizeof SN_MSG, "%s", SETUP || lock ? "" : "Rudolf OS is not set up yet: start it once with the Standard desktop engine.");
	SFOC = 1; SHOV = -1;
	gtk_widget_show_all(SIGN); gtk_window_move(GTK_WINDOW(SIGN), 0, 0); gtk_window_present(GTK_WINDOW(SIGN));
	GdkWindow *gw = gtk_widget_get_window(SIGN); if (gw) gdk_window_focus(gw, GDK_CURRENT_TIME);
	gtk_widget_grab_focus(SAREA); sign_caret();
	if (BAR) gtk_widget_hide(BAR);
	wm_restack(); state_write();
}

/* ------------------------------------------------------------------ what rudolfd says */
static void settings_load(void)
{
	JV *v = jget("/api/desktop/settings"); if (!v) return;
	Settings n; memset(&n, 0, sizeof n); n.lat = NAN;
	JV *s = jprop(v, "s"), *ps = PREV_S ? jparse(PREV_S) : NULL; if (ps) s = ps;   /* (Settings' choices previewed, not applied yet) */
	jstr(s, "bs", n.bs, sizeof n.bs); jstr(s, "b1", n.b1, sizeof n.b1); jstr(s, "b2", n.b2, sizeof n.b2); jstr(s, "dyn", n.dyn, sizeof n.dyn); jstr(s, "dm", n.dm, sizeof n.dm);
	jstr(s, "wpfit", n.wpfit, sizeof n.wpfit); jstr(s, "tb", n.tb, sizeof n.tb); jstr(s, "tbstyle", n.tbstyle, sizeof n.tbstyle); jstr(s, "tbacm", n.tbacm, sizeof n.tbacm);
	jstr(s, "tbbstyle", n.tbbstyle, sizeof n.tbbstyle); jstr(s, "tbbacm", n.tbbacm, sizeof n.tbbacm); jstr(s, "twstyle", n.twstyle, sizeof n.twstyle); jstr(s, "twacm", n.twacm, sizeof n.twacm);
	jstr(s, "tbpcol", n.tbpcol, sizeof n.tbpcol); jstr(s, "accent", n.accent, sizeof n.accent); jstr(s, "df", n.df, sizeof n.df); jstr(s, "tz", n.tz, sizeof n.tz); jstr(s, "ff", n.ff, sizeof n.ff);
	char wp[64] = ""; jstr(s, "wpimg", wp, sizeof wp); n.wpimg = wp[0] != 0;
	jstr(s, "icons", n.icons, sizeof n.icons); n.scale = (int)jnum(s, "scale", 100); if (n.scale < 75 || n.scale > 250) n.scale = 100;
	n.lpac = jbool(s, "lpac"); jstr(s, "lpacm", n.lpacm, sizeof n.lpacm); n.lpbr = (int)jnum(s, "lpbr", 100);   /* (DEFS.disp lpac false, lpacm normal, lpbr 100) */
	n.nlv = (int)jnum(s, "nlv", 40); n.nlsched = jbool(s, "nlsched"); n.magz = jnum(s, "magz", 2);
	n.nl = jbool(s, "nl"); n.dnd = jbool(s, "dnd"); n.sr = jbool(s, "sr"); n.mag = jbool(s, "mag"); n.vk = jbool(s, "vk"); n.touch = jbool(s, "touch"); { char cb[16] = ""; jstr(s, "cb", cb, sizeof cb); n.hc = !strcmp(cb, "contrast"); }
	{ JV *dk = jprop(s, "darkui"); n.darkui = !(dk && dk->t == 1 && !dk->n); }   /* (S.darkui!==false) */
	{ JV *au = jprop(s, "aur"); n.aur = !(au && au->t == 1 && !au->n); } n.rmotion = jbool(s, "rmotion"); jstr(s, "aurm", n.aurm, sizeof n.aurm); jstr(s, "lnm", n.lnm, sizeof n.lnm);   /* (DEFS.disp aur:true) */
	n.tbac = jbool(s, "tbac"); n.tbbac = jbool(s, "tbbac"); n.h24 = jbool(s, "h24"); n.doff = jnum(s, "doff", 0); n.lat = jnum(s, "lat", NAN);
	{ JV *ir = jprop(s, "idlerest"); n.idlerest = ir && ir->t == 1 ? (ir->n ? 1 : 0) : -1; } n.idlemin = (int)jnum(s, "idlemin", 0); if (n.idlemin < 1 || n.idlemin > 240) n.idlemin = 0;
	n.fs = (int)jnum(s, "fs", 13); jstr(s, "tx", n.tx, sizeof n.tx); jstr(s, "cb", n.cb, sizeof n.cb);   /* (the title bars' writing: the page's body font size and colour) */
	if (!n.bs[0]) snprintf(n.bs, sizeof n.bs, "solid"); if (!n.b1[0]) snprintf(n.b1, sizeof n.b1, "#008080"); if (!n.b2[0]) snprintf(n.b2, sizeof n.b2, "#000080");
	if (!n.tb[0]) snprintf(n.tb, sizeof n.tb, "bottom"); if (!n.wpfit[0]) snprintf(n.wpfit, sizeof n.wpfit, "fill"); if (!n.dm[0]) snprintf(n.dm, sizeof n.dm, "auto");
	jstr(v, "acct", n.acct, sizeof n.acct); jstr(v, "wpacct", n.wpacct, sizeof n.wpacct); n.gen = jnum(v, "gen", 0); n.notes = (int)jnum(v, "notes", 0); n.wallpaper = jbool(v, "wallpaper");
	JV *p = jprop(v, "pins"); for (int i = 0; i < 24; i++) { JV *o = jat(p, i); if (!o) break; if (o->t == 3) snprintf(n.pins[n.npins++], KN, "%s", o->s);  }
	int moved = strcmp(n.tb, S.tb) != 0 || n.scale != S.scale; S = n; qs_keep(); ui_family(S.ff); snd_set(s);   /* (the desktop's sounds: report 2) */
	wg_load(v);   /* (the widgets: item 2) */
	cal_events_load(v);   /* (the calendar under the clock: item 3) */
	nt_load(v);   /* (the notifications' list: item 3) */
	sc_load(v);   /* (the account's own keys: item 5) */
	sys_load(v);   /* (the screen saver's and the lock's settings: item 6) */
	  jfree(v); jfree(ps);
	/* the page's display scaling: the taskbar, the title bars and the start menu at the page's size (1.5.9 step 7) */
	Z = S.scale / 100.0; BARH = (int)lround(46 * Z);
	{ int th = tw_th(), bw = tw_bw(); if (th != TH || bw != BW) moved = 1; TH = th; BW = bw; }   /* (the title bars' height and the windows' edge: tw_geom) */
	{ int ow = BARW; bar_measure(); if (BARW != ow) moved = 1; }   /* (the taskbar at the left or the right: as wide as its widest part) */
	css_apply(); wall_make(); if (moved) { bar_place(); if (WM_OK) wm_relayout(); di_layout(); if (DESKAREA) desk_dirty(); } bar_redraw(); if (SAREA) gtk_widget_queue_draw(SAREA); if (WM_OK) frames_redraw();
	a11y_apply();   /* (1.6.0: quick settings' switches for the whole screen) */
}
static gboolean prev_keep_end(gpointer d) { (void)d; PREV_KEEP = 0; prev_end(); return G_SOURCE_REMOVE; }
static void prev_end(void)
{
	if (PREV_KEEP) { g_source_remove(PREV_KEEP); PREV_KEEP = 0; }
	if (!PREV_S) return; g_free(PREV_S); PREV_S = NULL; if (PREV_WP) { g_object_unref(PREV_WP); PREV_WP = NULL; } PREV_W = 0; PREV_N++;
	settings_load(); state_write();
}
static void prev_msg(AppWin *a, JSCValue *v)
{
	if (!USER_ID[0] || strcmp(a->k, "settings")) return;
	JSCValue *sv = jsc_value_object_get_property(v, "s"); char *sj = sv && jsc_value_is_string(sv) ? jsc_value_to_string(sv) : NULL; if (sv) g_object_unref(sv);
	JV *chk = sj && *sj ? jparse(sj) : NULL;
	if (!chk) { g_free(sj); if (PREV_S && PREV_W == a->id) { char kp[4] = ""; jsstr(v, "keep", kp, sizeof kp); if (kp[0] == '1') { if (!PREV_KEEP) PREV_KEEP = g_timeout_add(3000, prev_keep_end, NULL); } else prev_end(); } return; }
	jfree(chk); if (PREV_KEEP) { g_source_remove(PREV_KEEP); PREV_KEEP = 0; } g_free(PREV_S); PREV_S = sj; PREV_W = a->id; PREV_N++;
	JSCValue *wv = jsc_value_object_get_property(v, "wp");
	if (wv && jsc_value_is_string(wv)) { char *u = jsc_value_to_string(wv); const char *b64 = u ? strstr(u, ";base64,") : NULL;
		if (u && !strncmp(u, "data:image/", 11) && b64) { gsize n = 0; guchar *raw = g_base64_decode(b64 + 8, &n); GByteArray *b = g_byte_array_new_take(raw, n); guint8 z = 0; g_byte_array_append(b, &z, 1);
			GdkPixbuf *px = pix_from_bytes(b); g_byte_array_free(b, TRUE); if (px) { if (PREV_WP) g_object_unref(PREV_WP); PREV_WP = px; } }
		else if (u && !*u && PREV_WP) { g_object_unref(PREV_WP); PREV_WP = NULL; }
		g_free(u); }
	if (wv) g_object_unref(wv);
	settings_load(); state_write();
}
static void apps_load(void)
{
	/* (the signed-in account's own last catalogue: its icons at once at sign-in, as Standard's page has them, before this sign-in's pages give it) */
	JV *v = NULL; if (USER_ID[0]) { char *e = g_uri_escape_string(USER_ID, NULL, FALSE), p[200]; snprintf(p, sizeof p, "/api/desktop/apps?acct=%s", e); g_free(e); v = jget(p); }
	if (!v) v = jget("/api/desktop/apps"); if (!v) return;
	JV *a = jprop(v, "apps"); App *L = NULL; int n = 0;
	for (int i = 0; i < 400; i++) { JV *o = jat(a, i); if (!o) break; L = realloc(L, sizeof *L * (n + 1)); App *x = &L[n++]; memset(x, 0, sizeof *x);
		jstr(o, "k", x->k, sizeof x->k); jstr(o, "name", x->name, sizeof x->name); jstr(o, "icon", x->icon, sizeof x->icon); jstr(o, "kw", x->kw, sizeof x->kw); jstr(o, "ic", x->ic, sizeof x->ic); x->x = jbool(o, "x"); }
	/* the apps' pictures (PNG, as data: addresses) and the version */
	if (ICONS) g_hash_table_destroy(ICONS); ICONS = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
	JV *ics = jprop(v, "icons");
	for (int i = 0; ics && ics->t == 5 && i < ics->len; i++) {
		JV *o = ics->v[i]; static const char PRE[] = "data:image/png;base64,"; if (!o || o->t != 3 || strncmp(o->s, PRE, sizeof PRE - 1)) continue;
		gsize n = 0; guchar *raw = g_base64_decode(o->s + sizeof PRE - 1, &n); GByteArray *b = g_byte_array_new_take(raw, n); guint8 z = 0; g_byte_array_append(b, &z, 1);
		GdkPixbuf *px = pix_from_bytes(b); g_byte_array_free(b, TRUE); if (px) g_hash_table_insert(ICONS, g_strdup(ics->k[i]), px);
	}
	VER[0] = 0; jstr(v, "ver", VER, sizeof VER);
	di_load(v); menu_rows_load(v); jumps_load(v); if (DESKAREA) desk_dirty();
	/* (2026-11-17, the owner's Vivobook: "the desktop still takes several minutes to add icons") the log says when they came, for the
	   owner's checks: the first catalogue with the account's icons after each sign-in */
	if (ICONS_SINCE && di_shown()) { fprintf(stderr, "rudolf-native: the desktop's icons shown %.1f s after the sign-in (%d icons)\n", (g_get_monotonic_time() - ICONS_SINCE) / 1e6, di_n()); ICONS_SINCE = 0; }
	free(APPS); APPS = L; NAPPS = n;
	if (BG) g_hash_table_destroy(BG); BG = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	JV *bg = jprop(v, "bg");
	if (bg && bg->t == 5) {
		for (int i = 0; i < bg->len; i++) { JV *o = bg->v[i]; const char *kk = bg->k[i]; char d[200] = "", l[200] = "", am[200] = ""; jstr(o, "d", d, sizeof d); jstr(o, "l", l, sizeof l); jstr(o, "am", am, sizeof am);
			if (d[0] && !strstr(d, "..") && !strstr(l, "..")) g_hash_table_insert(BG, g_strdup(kk), g_strdup_printf("%s\n%s", d, l[0] ? l : d));
			if (am[0] && !strstr(am, "..") && am[0] != '/' && !strchr(am, ':')) { char *k2 = g_strconcat("am:", kk, NULL); g_hash_table_insert(BG, k2, g_strdup(am)); } }
		}
	fx_cat(v);   /* (the statue's picture: N12) */
	snd_cat(v);   /* (the sounds' styles: report 2) */
	 jfree(v);
	WALL_KEY[0] = 0; wall_make(); bar_redraw(); if (MAREA) gtk_widget_queue_draw(MAREA);
}
static void tray_load(void)
{
	/* the network as the page picks it (net31.js net31Pick): the real adapters, the one with the default route and an
	   address, else the first one up with an address; Wi-Fi's signal from its rssi */
	JV *v = jget("/api/net"); int up = 0, wifi = 0, anywifi = 0, n = 0, pick = -1;
	JV *ifs = jprop(v, "interfaces");
	/* (1.6.0 Parts 3 and 5: a cellular modem's connection counts as real, also IPv6-only, where the CLAT carries the default route) */
	int clat = 0; for (int i = 0; i < 16; i++) { JV *o = jat(ifs, i); if (!o) break; char nm[32] = ""; jstr(o, "name", nm, sizeof nm); if (!strcmp(nm, "clat") && jbool(o, "default")) clat = 1; }
	for (int pass = 0; pass < 2 && pick < 0; pass++) for (int i = 0; i < 16; i++) { JV *o = jat(ifs, i); if (!o) break; if (jbool(o, "virtual") && !jbool(o, "cellular")) continue; char ip[64] = "", st[16] = "";
		jstr(o, "ip", ip, sizeof ip); jstr(o, "state", st, sizeof st); if (!pass) { n++; if (jbool(o, "wireless")) anywifi = 1; if (clat && jbool(o, "cellular")) { pick = i; break; } }
		if (ip[0] && (pass ? !strcmp(st, "up") : jbool(o, "default"))) { pick = i; break; } }
	NET_RSSI = 1; NET_CELL = 0; NET_SIG = -1;
	if (pick >= 0) { JV *o = jat(ifs, pick); up = 1; wifi = jbool(o, "wireless"); JV *r = jprop(o, "rssi"); if (r && r->t == 2) NET_RSSI = (int)r->n;
		/* (net31Desc: Wi-Fi with its name (wpa_cli's escapes undone: wfDecode), else Ethernet with its name and speed) */
		char nm[64] = "", ss[160] = "", dec[160] = ""; jstr(o, "name", nm, sizeof nm); jstr(o, "ssid", ss, sizeof ss);
		{ int j = 0; for (const char *c = ss; *c && j < (int)sizeof dec - 1; c++) { if (*c == '\\' && c[1]) { c++; if (*c == 'x' && g_ascii_isxdigit(c[1]) && g_ascii_isxdigit(c[2])) { dec[j++] = (char)(g_ascii_xdigit_value(c[1]) * 16 + g_ascii_xdigit_value(c[2])); c += 2; }
				else dec[j++] = *c == 'n' ? '\n' : *c == 'r' ? '\r' : *c == 't' ? '\t' : *c == 'e' ? 27 : *c; } else dec[j++] = *c; } dec[j] = 0; }
		double sp = jnum(o, "speed", 0); char spd[32] = ""; if (sp > 0) { if (sp >= 1000) { char g[24]; g_ascii_formatd(g, sizeof g, "%g", sp / 1000); snprintf(spd, sizeof spd, ", %s Gb/s", g); } else snprintf(spd, sizeof spd, ", %.0f Mb/s", sp); }
		if (jbool(o, "cellular")) { char ca[64] = "", te[16] = "", ct[96]; jstr(o, "carrier", ca, sizeof ca); jstr(o, "tech", te, sizeof te); NET_CELL = 1; JV *sg = jprop(o, "signal"); if (sg && sg->t == 2) NET_SIG = (int)sg->n;
			snprintf(ct, sizeof ct, "%s%s%s", ca, ca[0] && te[0] ? " · " : "", te); snprintf(NET_DESC, sizeof NET_DESC, "Mobile data — connected%s%s%s", ct[0] ? " (" : "", ct, ct[0] ? ")" : ""); }
		else if (wifi) snprintf(NET_DESC, sizeof NET_DESC, "Wi-Fi — connected%s%s%s", dec[0] ? " (" : "", dec, dec[0] ? ")" : ""); else snprintf(NET_DESC, sizeof NET_DESC, "Ethernet — connected (%s%s)", nm, spd); }
	else { wifi = anywifi || !n; NET_DESC[0] = 0; }
	NET_UP = v ? up : -1; NET_WIFI = wifi; jfree(v);
	v = jget("/api/volume"); if (v && jbool(v, "ok")) { VOL = (int)jnum(v, "volume", -1); MUTED = jbool(v, "muted"); } else VOL = -1; jfree(v);
	/* the battery as the page reads it (power31.js: rudolfd's power/status, its "battery" and "ac") */
	v = jget("/api/power/status"); BAT = -1; JV *b = jprop(v, "battery");
	if (b && b->t == 5) { char st[24] = ""; jstr(b, "status", st, sizeof st); double c = jnum(b, "capacity", -1);
		if (c >= 0 && c <= 100) { BAT = (int)c; CHARGING = !strcmp(st, "Charging") || (jbool(v, "ac") && strcmp(st, "Discharging")); } }
	jfree(v);
	bar_redraw();
}
static gboolean tick_settings(gpointer d)
{
	(void)d; JV *v = jget("/api/desktop/settings"); if (!v) return G_SOURCE_CONTINUE;
	double g = jnum(v, "gen", 0); char a[80] = ""; jstr(v, "acct", a, sizeof a); int notes = (int)jnum(v, "notes", 0), wp = jbool(v, "wallpaper"); jfree(v);
	if (g != S.gen || strcmp(a, S.acct) || notes != S.notes || (WALL_MISS && wp != S.wallpaper)) { int newg = g != S.gen; settings_load(); if (newg) apps_load(); }
	return G_SOURCE_CONTINUE;
}
static gboolean tick_tray(gpointer d) { (void)d; tray_load(); return G_SOURCE_CONTINUE; }
/* (call59.js call59Poll: every 5 s, the apps using a microphone and whether it is muted) */
static gboolean tick_mic(gpointer d) { (void)d; if (!USER_ID[0]) { if (MIC_N) { MIC_N = 0; bar_redraw(); } return G_SOURCE_CONTINUE; } JV *v = jget("/api/mic"); int n = 0, m = 0;
	if (v) { JV *u = jprop(v, "users"); n = u && u->t == 4 ? u->len : 0; m = jbool(v, "muted"); } jfree(v); if (n != MIC_N || m != MIC_MUTED) { MIC_N = n; MIC_MUTED = m; bar_redraw(); } return G_SOURCE_CONTINUE; }
static gboolean tick_clock(gpointer d) { (void)d; char o[32]; snprintf(o, sizeof o, "%s", CLOCK_T); clock_text(); if (strcmp(o, CLOCK_T)) { bar_redraw(); wg_dirty_kind("clock"); wg_dirty_kind("cal"); if (SAREA && SIGN && gtk_widget_get_visible(SIGN)) gtk_widget_queue_draw(SAREA); } if (S.dyn[0] && !S.wpimg) wall_make(); if (S.nl && S.nlsched) gamma_apply(); return G_SOURCE_CONTINUE; }
/* the session's watchdog and trial need to hear from the desktop, as the page's heartbeat */
static gboolean tick_heart(gpointer d) { (void)d; http("POST", "/api/heartbeat", NULL, 0, NULL); return G_SOURCE_CONTINUE; }

/* ------------------------------------------------------------------ the page's menus and dialogs, drawn by the shell
   (1.6.0 Part 12, item 1's second part) the right-click menus as the page's cmenu draws them (.ctx.cmenu: a white box,
   1 px border, 8 px corners, its shadow, focused (the accent's ring); a grey title; rows of a picture, the words and a
   key in a grey pill; lines between groups; the pointer's row on the accent colour; greyed rows), and the page's
   Rename and Open with dialogs (.rmodal .rmbox). What they do is the page's own: done by the page (#app=do-desk and
   do-fs, oneapp59.js), unseen when it has no window of its own. */
typedef struct { char label[96], icon[24], key[32], act[96]; int sep, dis; } CMI;
static GtkWidget *CMWIN, *CMAREA; static GdkPixbuf *CMBG; static CMI CMIT[24]; static int NCMI, CMHOV = -1; static char CMTITLE[160];
static double CMX, CMY, CMBW, CMBH, CMIW; static int CMWX, CMWY;   /* the box (the page's units, on the screen), its inside's width; the window's place */
static void (*CM_CB)(const char *act);
/* (CSS's generic sans-serif is Chromium's standard font, Arial: Liberation Sans here, not fontconfig's default) */
#define KBDF "Arial, sans-serif"
static const char *bdcol(void) { return menu_dark() ? "#334155" : "#d1d5db"; }
static double cm_row_h(const CMI *m) { return m->sep ? 7 : 25; }
static double cm_kbd_w(cairo_t *cr, const char *k) { return 8 + 2 + (k[0] ? css_width(cr, k, KBDF, 400, 11) : 0); }
static void cm_measure(cairo_t *cr)
{
	double w = 170, h = CMTITLE[0] ? 22 : 0;
	if (CMTITLE[0]) w = fmax(w, fmin(260, 40 + css_width(cr, CMTITLE, UIFAM, 600, 12)));
	for (int i = 0; i < NCMI; i++) { CMI *m = &CMIT[i]; h += cm_row_h(m); if (m->sep) continue;
		w = fmax(w, 6 + 18 + 8 + css_width(cr, m->label, UIFAM, 400, 13) + 8 + 14 + cm_kbd_w(cr, m->key) + 10); }
	CMIW = w; CMBW = w + 8 + 2; CMBH = h + 8 + 2;
}
static int CM_RING = 1;   /* (the menu shows its focus ring) */
static int CM_LATE;      /* (rows the page adds after placing its menu) */
static gboolean cm_draw(GtkWidget *wd, cairo_t *cr, gpointer d)
{
	(void)wd; (void)d; if (CMBG) { gdk_cairo_set_source_pixbuf(cr, CMBG, 0, 0); cairo_paint(cr); }
	lp_draw_clip(cr, CMWIN, CMWX, CMWY);   /* (Part 17b: opening) */
	cairo_translate(cr, -CMWX, -CMWY); cairo_scale(cr, Z, Z); int dark = menu_dark();
	Box b; memset(&b, 0, sizeof b); b.bg = cx(dark ? "#0f172a" : "#ffffff", 1); b.bw = 1; b.bc = cx(bdcol(), 1); b.rad = 8; shd_add(&b, 0, 10, 26, cx("#000000", .3), 0);
	box_paint(cr, CMX, CMY, CMBW, CMBH, &b);
	/* (focused, as the page's menu: outline 2px, 2px out; Chromium shows it only when nothing took the focus by the mouse first, so not for an icon's menu) */
	if (CM_RING) { double x = CMX - 4, y = CMY - 4, w = CMBW + 8, h = CMBH + 8; snap(&x, &y, &w, &h); rrect(cr, x, y, w, h, 12); rrect(cr, x + 2, y + 2, w - 4, h - 4, 10);
		cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD); srca(cr, ca(accent(), 1)); cairo_fill(cr); cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING); }
	RGBA ink = cx(dark ? "#e2e8f0" : "#000000", 1); double x = CMX + 5, y = CMY + 5;
	if (CMTITLE[0]) { css_text(cr, CMTITLE, UIFAM, 600, 12, x + 30, y + 4, 14, cx("#6b7280", 1), CMIW - 40); y += 22; }
	for (int i = 0; i < NCMI; i++) { CMI *m = &CMIT[i];
		if (m->sep) { double lx = x, ly = y + 3, lw = CMIW, lh = 1; snap(&lx, &ly, &lw, &lh); cairo_rectangle(cr, lx, ly, lw, fmax(1, lh)); srca(cr, cx(bdcol(), 1)); cairo_fill(cr); y += 7; continue; }
		int hv = i == CMHOV && !m->dis; RGBA c = hv ? cx("#ffffff", 1) : ink; double op = m->dis ? .45 : 1;
		if (op < 1) cairo_push_group(cr);
		if (hv) { Box h; memset(&h, 0, sizeof h); h.bg = ca(accent(), 1); h.rad = 5; box_paint(cr, x, y, CMIW, 25, &h); }
		if (m->icon[0]) { double iw = css_width(cr, m->icon, UIFAM, 400, 13); css_text(cr, m->icon, UIFAM, 400, 13, x + 6 + (18 - iw) / 2, y + 5, 15, c, 0); }
		css_text(cr, m->label, UIFAM, 400, 13, x + 6 + 18 + 8, y + 5, 15, c, 0);
		/* the key, in its grey pill (an empty one is a small pill, as the page's) */
		double kw = cm_kbd_w(cr, m->key), kh = m->key[0] ? 15 : 3, kx = x + CMIW - 10 - kw, ky = y + (25 - kh) / 2;
		cairo_push_group(cr); Box k; memset(&k, 0, sizeof k); k.bg = cx("#f3f4f6", 1); k.rad = 4; box_paint(cr, kx, ky, kw, kh, &k);
		{ double bx = kx, by = ky, bw = kw, bh = kh; snap(&bx, &by, &bw, &bh); rrect(cr, bx, by, bw, bh, 4); rrect(cr, bx + 1, by + 1, bw - 2, bh - 3, 3);
			cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD); srca(cr, cx("#d1d5db", 1)); cairo_fill(cr); cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING); }
		if (m->key[0]) css_text(cr, m->key, KBDF, 400, 11, kx + 5, ky + 1, 12, c, 0);
		cairo_pop_group_to_source(cr); cairo_paint_with_alpha(cr, .6);
		if (op < 1) { cairo_pop_group_to_source(cr); cairo_paint_with_alpha(cr, op); }
		y += 25; }
	return TRUE;
}
static int cm_at(double px, double py)
{
	double x = (px + CMWX) / Z, y = (py + CMWY) / Z, ry = CMY + 5 + (CMTITLE[0] ? 22 : 0);
	if (x < CMX + 5 || x >= CMX + 5 + CMIW) return -1;
	for (int i = 0; i < NCMI; i++) { double h = cm_row_h(&CMIT[i]); if (!CMIT[i].sep && y >= ry && y < ry + h) return i; ry += h; }
	return -1;
}
static void cm_close(void) { if (CMWIN && gtk_widget_get_visible(CMWIN)) { gdk_seat_ungrab(gdk_display_get_default_seat(gdk_display_get_default())); gtk_widget_hide(CMWIN); pop_gone(CMBG, CMWX, CMWY); } if (CMBG) { g_object_unref(CMBG); CMBG = NULL; } CMHOV = -1; state_write(); }
static void cm_choose(int i) { if (i < 0 || i >= NCMI || CMIT[i].sep || CMIT[i].dis) return; char a[96]; snprintf(a, sizeof a, "%s", CMIT[i].act); void (*cb)(const char *) = CM_CB; cm_close(); if (cb) cb(a); }
static gboolean cm_motion(GtkWidget *w, GdkEventMotion *e, gpointer d) { (void)w; (void)d; int h = cm_at(e->x, e->y); if (h != CMHOV) { CMHOV = h; gtk_widget_queue_draw(CMAREA); } return TRUE; }
static gboolean cm_press(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)w; (void)d; if (e->type != GDK_BUTTON_PRESS) return TRUE;
	double x = (e->x + CMWX) / Z, y = (e->y + CMWY) / Z;
	if (x < CMX || x >= CMX + CMBW || y < CMY || y >= CMY + CMBH) { cm_close(); return TRUE; }   /* (a click elsewhere closes it, as the page's) */
	if (e->button == 1) { int i = cm_at(e->x, e->y); if (i >= 0 && i < NCMI && !CMIT[i].sep) snd_click(); cm_choose(i); } return TRUE;   /* (.ctx div: a click sounds, as Standard's) */
}
static gboolean cm_key(GtkWidget *w, GdkEventKey *e, gpointer d)
{
	(void)w; (void)d;
	if (e->keyval == GDK_KEY_Escape) { cm_close(); return TRUE; }
	if (e->keyval == GDK_KEY_Down || e->keyval == GDK_KEY_Up) { int s = e->keyval == GDK_KEY_Down ? 1 : -1, i = CMHOV;
		for (int k = 0; k < NCMI; k++) { i = (i + s + NCMI) % NCMI; if (!CMIT[i].sep && !CMIT[i].dis) break; } CMHOV = i; gtk_widget_queue_draw(CMAREA); return TRUE; }
	if (e->keyval == GDK_KEY_Return || e->keyval == GDK_KEY_KP_Enter) { cm_choose(CMHOV); return TRUE; }
	return TRUE;
}
/* the menu at the pointer (X, Y: the screen's pixels), kept on the screen */
static void cm_open(double px, double py, const char *title, const CMI *items, int n, void (*cb)(const char *))
{
	cm_close(); NCMI = n > 24 ? 24 : n; memcpy(CMIT, items, sizeof(CMI) * NCMI); snprintf(CMTITLE, sizeof CMTITLE, "%s", title ? title : ""); CM_CB = cb;
	{ cairo_surface_t *t = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 4, 4); cairo_t *c = cairo_create(t); cm_measure(c); cairo_destroy(c); cairo_surface_destroy(t); }
	int ck = 0; for (int k = 1; k < NMON; k++) if (px >= MON[k].x && px < MON[k].x + MON[k].w && py >= MON[k].y && py < MON[k].y + MON[k].h) ck = k;   /* (1.6.1 Part 1: the monitor the point is on) */
	int cox = ck ? MON[ck].x : 0, coy = ck ? MON[ck].y : 0; px -= cox; py -= coy;
	double vw = (ck ? MON[ck].w : SW) / Z, vh = (ck ? MON[ck].h : SH) / Z; CMX = px / Z; CMY = py / Z;
	double ph = CMBH - CM_LATE * 25; CM_LATE = 0;   /* (a window's menu: placed by the page before its Print row was added, apps25b.js) */
	ph = round(ph); CMX = fmax(2, fmin(CMX, vw - round(CMBW) - 4)); CMY = fmax(2, CMY + ph > vh - 4 ? CMY - ph : CMY);   /* (offsetWidth and offsetHeight: whole pixels) */   /* (the page's cmenu: kept on the screen, above the pointer when it would not fit below) */
	if (!CMWIN) { CMWIN = gtk_window_new(GTK_WINDOW_POPUP); gtk_window_set_title(GTK_WINDOW(CMWIN), "Menu"); CMAREA = gtk_drawing_area_new(); gtk_widget_set_can_focus(CMAREA, TRUE);
		gtk_widget_add_events(CMAREA, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_KEY_PRESS_MASK); gtk_container_add(GTK_CONTAINER(CMWIN), CMAREA);
		g_signal_connect(CMAREA, "draw", G_CALLBACK(cm_draw), NULL); g_signal_connect(CMAREA, "motion-notify-event", G_CALLBACK(cm_motion), NULL);
		g_signal_connect(CMAREA, "button-press-event", G_CALLBACK(cm_press), NULL); g_signal_connect(CMAREA, "key-press-event", G_CALLBACK(cm_key), NULL); }
	/* (the window: the box, its ring and its shadow; what is behind copied, there being no compositor) */
	CMX += cox / Z; CMY += coy / Z;
	int x0 = (int)fmax(cox, floor((CMX - 30) * Z)), y0 = (int)fmax(coy, floor((CMY - 30) * Z)), x1 = (int)fmin(cox + vw * Z, ceil((CMX + CMBW + 30) * Z)), y1 = (int)fmin(coy + vh * Z, ceil((CMY + CMBH + 50) * Z));
	CMWX = x0; CMWY = y0; CMBG = screen_copy(x0, y0, x1 - x0, y1 - y0);
	gtk_widget_set_size_request(CMAREA, x1 - x0, y1 - y0); gtk_window_resize(GTK_WINDOW(CMWIN), x1 - x0, y1 - y0); gtk_window_move(GTK_WINDOW(CMWIN), x0, y0);
	CMHOV = -1; gtk_widget_show_all(CMWIN); XRaiseWindow(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), GDK_WINDOW_XID(gtk_widget_get_window(CMWIN)));
	seat_grab(CMWIN, TRUE);   /* (as the pop-ups': once the menu is on the screen) */
	{ double bx = CMX * Z, by = CMY * Z, bw = CMBW * Z, bh = CMBH * Z; int right = px > bx + bw / 2, below = py > by + bh / 2;
	  lp_menu_open(CMWIN, CMWX, CMWY, LP_CTX, bx, by, bw, bh, px, py, below ? (right ? 2 : 3) : (right ? 1 : 0)); }   /* (Part 17b: from its corner at the pointer) */
	gtk_widget_grab_focus(CMAREA); state_write();
}

/* the page's dialogs (.rmodal .rmbox): Rename (renameDlg) and Open with (files25.js openWith) */
enum { DLG_NONE, DLG_RENAME, DLG_OPENWITH };
static GtkWidget *DGWIN, *DGAREA; static GdkPixbuf *DGBG; static GtkIMContext *DGIM;
static int DGK, DGHOV = -1, DGFOC, DGSEL, DGCARET, DGALW, DGBUSY; static guint DGCARET_T;
static char DGFILE[512], DGNAME[128], DGVAL[128], DGMSG[200], DGEXT[16];
typedef struct { char k[KN], name[80], icon[24]; } Opener;
static Opener DGAPPS[12]; static int NDGAPPS;
typedef struct { double x, y, w, h; } DRect;
static DRect DGB[16]; static int NDGB;   /* the buttons (Rename: Cancel, Rename; Open with: the apps, then Cancel), the box */
static double DGX, DGY, DGW, DGH;
static gboolean dg_blink(gpointer d) { (void)d; DGCARET = !DGCARET; if (DGAREA) gtk_widget_queue_draw(DGAREA); return G_SOURCE_CONTINUE; }
static void dg_geom(void)
{
	double vw = SW / Z, vh = SH / Z; DGW = 360; NDGB = 0;
	if (DGK == DLG_RENAME) { DGH = 18 + 18 + 10 + 23 + 6 + 15.6 + 14 + 23 + 18; }
	else { double lh = NDGAPPS * 33 + (NDGAPPS > 1 ? (NDGAPPS - 1) * 4 : 0); DGH = 18 + 18 + 10 + lh + 8 + 19 + 14 + 23 + 18; }
	DGX = (vw - DGW) / 2; DGY = (vh - DGH) / 2;
}
static void dg_button(cairo_t *cr, const char *t, DRect r, int pri, int hv, int foc, int left)
{
	if (foc) { double x = r.x - 4, y = r.y - 4, w = r.w + 8, h = r.h + 8; snap(&x, &y, &w, &h); rrect(cr, x, y, w, h, 8); rrect(cr, x + 2, y + 2, w - 4, h - 4, 6);   /* (outline 2 px at offset 2) */
		cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD); srca(cr, cx("#2563eb", 1)); cairo_fill(cr); cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING); }
	Box k; memset(&k, 0, sizeof k); k.rad = 4; k.bw = 1;
	if (pri) { k.bg = ca(accent(), 1); k.bc = ca(accent(), 1); } else { k.bg = cx(hv ? "#eaf0ff" : "#ffffff", 1); k.bc = cx(hv ? "#a9bff5" : "#d1d5db", 1); }
	box_paint(cr, r.x, r.y, r.w, r.h, &k); RGBA c = cx(pri ? "#ffffff" : "#000000", 1);
	if (left) css_text(cr, t, UIFAM, 400, 13, r.x + 9, r.y + (r.h - 15) / 2, 15, c, r.w - 18);
	else { double tw = css_width(cr, t, UIFAM, 400, 13); css_text(cr, t, UIFAM, 400, 13, r.x + (r.w - tw) / 2, r.y + (r.h - 15) / 2, 15, c, 0); }
}
static gboolean dg_draw(GtkWidget *wd, cairo_t *cr, gpointer d)
{
	(void)wd; (void)d; if (DGBG) { gdk_cairo_set_source_pixbuf(cr, DGBG, 0, 0); cairo_paint(cr); }
	cairo_set_source_rgba(cr, 15 / 255.0, 23 / 255.0, 42 / 255.0, .45); cairo_paint(cr); cairo_scale(cr, Z, Z); dg_geom();
	Box b; memset(&b, 0, sizeof b); b.bg = cx("#ffffff", 1); b.rad = 12; shd_add(&b, 0, 20, 50, cx("#000000", .4), 0); box_paint(cr, DGX, DGY, DGW, DGH, &b);
	RGBA ink = cx("#111827", 1); double x = DGX + 20, y = DGY + 18;
	if (DGK == DLG_RENAME) {
		css_text(cr, "Rename", UIFAM, 700, 15.21, x, y, 18, ink, 0); y += 28;
		/* the field: 100 % wide plus its padding and border (334 px: the page's), focused, its words chosen at first */
		{ double fx = x - 4, fy = y - 4, fw = 334 + 8, fh = 23 + 8; snap(&fx, &fy, &fw, &fh); rrect(cr, fx, fy, fw, fh, 8); rrect(cr, fx + 2, fy + 2, fw - 4, fh - 4, 6);   /* (outline 2 px at offset 2) */
			cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD); srca(cr, cx("#2563eb", 1)); cairo_fill(cr); cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING); }
		Box in; memset(&in, 0, sizeof in); in.bg = cx("#ffffff", 1); in.bw = 1; in.bc = cx("#d1d5db", 1); in.rad = 4; box_paint(cr, x, y, 334, 23, &in);
		double tw = css_width(cr, DGVAL, UIFAM, 400, 13.3333);
		cairo_save(cr); cairo_rectangle(cr, x + 7, y + 1, 320, 21); cairo_clip(cr);
		if (DGSEL && DGVAL[0]) { double sx = round(x + 7), sy = round(y + 4), sw = round(x + 7 + tw) - sx, sh = 15; cairo_rectangle(cr, sx, sy, sw, sh); srca(cr, cx("#3367d1", 1)); cairo_fill(cr); }
		css_text(cr, DGVAL, UIFAM, 400, 13.3333, x + 7, y + 4, 15, cx(DGSEL ? "#ffffff" : "#111111", 1), 0);
		if (!DGSEL && DGCARET) { double cx0 = round(x + 7 + tw), cy0 = y + 4; cairo_rectangle(cr, cx0, cy0, 1, 15); srca(cr, cx("#111111", 1)); cairo_fill(cr); }
		cairo_restore(cr); y += 23 + 6;
		css_text(cr, DGMSG, UIFAM, 400, 13, x, y, 15.6, cx("#b91c1c", 1), 320); y += 15.6 + 14;
		double cw = css_width(cr, "Cancel", UIFAM, 400, 13) + 22, rw = css_width(cr, "Rename", UIFAM, 400, 13) + 22;
		DGB[0] = (DRect){ x + 320 - rw - 8 - cw, y, cw, 23 }; DGB[1] = (DRect){ x + 320 - rw, y, rw, 23 }; NDGB = 2;
		dg_button(cr, "Cancel", DGB[0], 0, DGHOV == 0, 0, 0); dg_button(cr, "Rename", DGB[1], 1, DGHOV == 1, 0, 0);
	} else {
		char h[300]; snprintf(h, sizeof h, "Open “%s” with", DGNAME); css_text(cr, h, UIFAM, 700, 15.21, x, y, 18, ink, 320); y += 28;
		for (int i = 0; i < NDGAPPS; i++) { char t[160]; snprintf(t, sizeof t, "%s %s", DGAPPS[i].icon, DGAPPS[i].name); DGB[i] = (DRect){ x, y, 320, 33 };
			dg_button(cr, t, DGB[i], 0, DGHOV == i, DGFOC == i, 1); y += 33 + 4; }
		y += 4;
		/* Always use this app for .EXT files (a checkbox as Chromium draws one, 13 px) */
		{ double bx = x + 4, by = y + 3; Box c; memset(&c, 0, sizeof c); c.bg = cx(DGALW ? "#2563eb" : "#ffffff", 1); c.bw = 1; c.bc = cx(DGALW ? "#2563eb" : "#767676", 1); c.rad = 2; box_paint(cr, bx, by, 13, 13, &c);
			if (DGALW) { cairo_move_to(cr, bx + 3, by + 6.5); cairo_line_to(cr, bx + 5.5, by + 9); cairo_line_to(cr, bx + 10, by + 4); cairo_set_source_rgb(cr, 1, 1, 1); cairo_set_line_width(cr, 1.8); cairo_stroke(cr); }
			DGB[NDGAPPS] = (DRect){ x, y, 320, 19 }; }
		char a[120]; snprintf(a, sizeof a, "Always use this app for .%s files", DGEXT[0] ? DGEXT : "?"); css_text(cr, a, UIFAM, 400, 13, x + 4 + 13 + 3 + 8, y, 17.55, ink, 0); y += 19 + 14;
		double cw = css_width(cr, "Cancel", UIFAM, 400, 13) + 22; DGB[NDGAPPS + 1] = (DRect){ x + 320 - cw, y, cw, 23 }; NDGB = NDGAPPS + 2;
		dg_button(cr, "Cancel", DGB[NDGAPPS + 1], 0, DGHOV == NDGAPPS + 1, 0, 0);
	}
	return TRUE;
}
static int dg_at(double px, double py) { double x = px / Z, y = py / Z; for (int i = 0; i < NDGB; i++) if (x >= DGB[i].x && x < DGB[i].x + DGB[i].w && y >= DGB[i].y && y < DGB[i].y + DGB[i].h) return i; return -1; }
static void dg_close(void)
{
	if (DGWIN && gtk_widget_get_visible(DGWIN)) { gtk_im_context_focus_out(DGIM); gdk_seat_ungrab(gdk_display_get_default_seat(gdk_display_get_default())); gtk_widget_hide(DGWIN); }
	if (DGBG) { g_object_unref(DGBG); DGBG = NULL; } if (DGCARET_T) { g_source_remove(DGCARET_T); DGCARET_T = 0; } DGK = DLG_NONE; DGBUSY = 0; state_write();
}
static char *qesc(const char *v) { return g_uri_escape_string(v, NULL, FALSE); }
static void dg_ok(void)
{
	if (DGK != DLG_RENAME || DGBUSY) return; DGBUSY = 1; DGMSG[0] = 0;
	/* (the owner's Vivobook, 2026-11-16) by the desktop's service page, signed in already, at once (else a page opened for it) */
	{ AppWin *sv = svc_page(); if (sv && sv->sready && sv->view && !sv->closing) { GString *js = g_string_new("window.__oaRename&&__oaRename("); json_esc(js, DGFILE); g_string_append_c(js, ','); json_esc(js, DGVAL); g_string_append(js, ")");
		webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(sv->view), js->str, -1, NULL, NULL, NULL, NULL, NULL); g_string_free(js, TRUE); gtk_widget_queue_draw(DGAREA); return; } }
	char *t = qesc(DGVAL), *ex = g_strdup_printf("&op=rename&to=%s", t); app_open_ex("do-fs", DGFILE, ex, 1); g_free(t); g_free(ex); gtk_widget_queue_draw(DGAREA);
}
/* (1.5.9, 12.3c) Open with chosen in an app's own page (oneapp59.js: its dialog, in Oracle's window or a WebKit window): the app's
   window of its own, as the shell's Open with dialog makes it */
/* (1.6.0 Part 12, test finding N7) File Explorer's "Folders on this device" (Import a folder…, Export selection to a folder…): on
   Standard the page has Chromium's folder picker (showDirectoryPicker: a folder of the computer chosen in a dialog, its files read
   and written by the page); WebKit has none, so the section was missing. Here the page's stand-in (oneapp59.js) asks the shell: "pick"
   shows a folder dialog (read, or read and write as asked) and answers a handle for the folder chosen; "ls", "read", "write", "mkdir",
   "stat" work inside a folder so chosen only (a name with a / or that is "." or ".." is refused, and so is a path that leaves the
   folder through a link), as the page's own handles do. The answer goes back to the page as __oaHfsIn(ID, JSON). */
typedef struct { char *path; int rw; AppWin *a; } HfsRoot;
static GPtrArray *HFS;
static void hfs_reply(AppWin *a, const char *id, GString *j)
{
	if (g_list_find(WINS, a) && a->view) { GString *js = g_string_new("window.__oaHfsIn&&__oaHfsIn("); json_esc(js, id); g_string_append(js, ","); json_esc(js, j->str); g_string_append(js, ")");
		webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(a->view), js->str, -1, NULL, NULL, NULL, NULL, NULL); g_string_free(js, TRUE); }
	g_string_free(j, TRUE);
}
static void hfs_err(AppWin *a, const char *id, const char *name, const char *msg) { GString *j = g_string_new("{\"err\":"); json_esc(j, name); g_string_append(j, ",\"msg\":"); json_esc(j, msg); g_string_append(j, "}"); hfs_reply(a, id, j); }
/* the path inside root T: its parts checked, then the real path kept under the root's real path */
static char *hfs_path(AppWin *a, const char *t, const char *rel, int must)
{
	int i = atoi(t); if (!HFS || i < 1 || i > (int)HFS->len) return NULL; HfsRoot *r = g_ptr_array_index(HFS, i - 1); if (r->a != a) return NULL;
	char **parts = g_strsplit(rel ? rel : "", "/", -1); GString *p = g_string_new(r->path); int bad = 0;
	for (char **x = parts; *x; x++) { if (!**x) continue; if (!strcmp(*x, ".") || !strcmp(*x, "..")) { bad = 1; break; } g_string_append_c(p, '/'); g_string_append(p, *x); }
	g_strfreev(parts); if (bad) { g_string_free(p, TRUE); return NULL; }
	char *rp = realpath(must ? p->str : r->path, NULL), *rr = realpath(r->path, NULL); int ok = rp && rr && (!strncmp(rp, rr, strlen(rr)) && (rp[strlen(rr)] == 0 || rp[strlen(rr)] == '/'));
	free(rp); free(rr); if (!ok) { g_string_free(p, TRUE); return NULL; }
	return g_string_free(p, FALSE);
}
static int hfs_name_ok(const char *n) { return n && *n && !strchr(n, '/') && strcmp(n, ".") && strcmp(n, "..") && strlen(n) < 256; }
static void hfs_msg(AppWin *a, JSCValue *v)
{
	char id[24] = "", what[16] = "", t[16] = "", p[1024] = "", n[260] = "", mode[16] = ""; jsstr(v, "id", id, sizeof id); jsstr(v, "do", what, sizeof what);
	jsstr(v, "t", t, sizeof t); jsstr(v, "p", p, sizeof p); jsstr(v, "n", n, sizeof n); jsstr(v, "mode", mode, sizeof mode);
	if (!strcmp(what, "pick")) {
		GtkWidget *d = gtk_file_chooser_dialog_new("Select a folder", GTK_WINDOW(a->win), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, "_Cancel", GTK_RESPONSE_CANCEL, "_Select", GTK_RESPONSE_ACCEPT, NULL);
		gtk_window_set_modal(GTK_WINDOW(d), TRUE); gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(d), g_get_home_dir());
		int r = gtk_dialog_run(GTK_DIALOG(d)); char *f = r == GTK_RESPONSE_ACCEPT ? gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d)) : NULL; gtk_widget_destroy(d);
		if (!f) { hfs_err(a, id, "AbortError", "The user aborted a request."); return; }
		if (!HFS) HFS = g_ptr_array_new(); HfsRoot *ro = g_new0(HfsRoot, 1); ro->path = f; ro->rw = !strcmp(mode, "readwrite"); ro->a = a; g_ptr_array_add(HFS, ro);
		char *bn = g_path_get_basename(f); GString *j = g_string_new(""); g_string_append_printf(j, "{\"t\":\"%u\",\"name\":", HFS->len); json_esc(j, bn); g_string_append(j, "}"); g_free(bn); hfs_reply(a, id, j); return; }
	int ti = atoi(t); HfsRoot *ro = HFS && ti >= 1 && ti <= (int)HFS->len ? g_ptr_array_index(HFS, ti - 1) : NULL;
	char *dir = hfs_path(a, t, p, 1); if (!dir || !ro) { g_free(dir); hfs_err(a, id, "NotFoundError", "A requested file or directory could not be found."); return; }
	if (!strcmp(what, "ls")) { GDir *gd = g_dir_open(dir, 0, NULL); GString *j = g_string_new("{\"l\":["); int k = 0; const char *e;
		while (gd && (e = g_dir_read_name(gd))) { char *f = g_build_filename(dir, e, NULL); g_string_append(j, k++ ? "," : ""); g_string_append(j, "["); json_esc(j, e); g_string_append_printf(j, ",%d]", g_file_test(f, G_FILE_TEST_IS_DIR)); g_free(f); }
		if (gd) g_dir_close(gd); g_string_append(j, "]}"); hfs_reply(a, id, j); g_free(dir); return; }
	if (!hfs_name_ok(n) && strcmp(what, "read")) { g_free(dir); hfs_err(a, id, "TypeError", "Name is not allowed."); return; }
	char *f = !strcmp(what, "read") ? g_strdup(dir) : g_build_filename(dir, n, NULL); g_free(dir);
	if (!strcmp(what, "read")) { gchar *b = NULL; gsize len = 0; struct stat sb;
		if (stat(f, &sb) || !S_ISREG(sb.st_mode) || sb.st_size > 64 * 1048576 || !g_file_get_contents(f, &b, &len, NULL)) hfs_err(a, id, "NotReadableError", "The file could not be read.");
		else { gchar *b64 = g_base64_encode((guchar *)b, len); GString *j = g_string_new("{\"b\":\""); g_string_append(j, b64); g_string_append_printf(j, "\",\"m\":%lld}", (long long)sb.st_mtime * 1000); g_free(b64); hfs_reply(a, id, j); }
		g_free(b); g_free(f); return; }
	if (!strcmp(what, "stat")) { int dirp = g_file_test(f, G_FILE_TEST_IS_DIR), there = g_file_test(f, G_FILE_TEST_EXISTS);
		GString *j = g_string_new(""); g_string_append_printf(j, "{\"there\":%d,\"dir\":%d}", there, dirp); hfs_reply(a, id, j); g_free(f); return; }
	if (!ro->rw) { g_free(f); hfs_err(a, id, "NotAllowedError", "The request is not allowed."); return; }
	if (!strcmp(what, "mkdir")) { if (g_mkdir_with_parents(f, 0755)) hfs_err(a, id, "InvalidModificationError", "The folder could not be made."); else hfs_reply(a, id, g_string_new("{}")); g_free(f); return; }
	if (!strcmp(what, "write")) { gsize len = 0; JSCValue *b = jsc_value_object_get_property(v, "b"); char *bs = b && jsc_value_is_string(b) ? jsc_value_to_string(b) : g_strdup(""); if (b) g_object_unref(b);
		guchar *d = g_base64_decode(bs, &len); g_free(bs);
		if (!g_file_set_contents(f, (char *)d, len, NULL)) hfs_err(a, id, "InvalidModificationError", "The file could not be written."); else hfs_reply(a, id, g_string_new("{}"));
		g_free(d); g_free(f); return; }
	g_free(f); hfs_err(a, id, "TypeError", "Unknown.");
}
static void open_with(const char *file, const char *key)
{
	if (!file || !file[0] || !key_ok(key) || !USER_ID[0] || LOCKED) return;
	char *ex = g_strdup_printf("&op=openwith&key=%s", key); app_open_ex("do-fs", file, ex, 0); g_free(ex);
}
static void dg_pick(int i)
{
	if (i < 0 || i >= NDGAPPS) return; char *k = qesc(DGAPPS[i].k), *ex = g_strdup_printf("&op=openwith&key=%s%s", k, DGALW ? "&always=1" : ""); char f[512]; snprintf(f, sizeof f, "%s", DGFILE);
	dg_close(); app_open_ex("do-fs", f, ex, 0); g_free(k); g_free(ex);
}
static gboolean dg_motion(GtkWidget *w, GdkEventMotion *e, gpointer d) { (void)w; (void)d; int h = dg_at(e->x, e->y); if (DGK == DLG_OPENWITH && h == NDGAPPS) h = -1; if (h != DGHOV) { DGHOV = h; gtk_widget_queue_draw(DGAREA); } return TRUE; }
static gboolean dg_press(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)w; (void)d; if (e->type != GDK_BUTTON_PRESS || e->button != 1) return TRUE; int i = dg_at(e->x, e->y);
	if (i >= 0 && !(DGK != DLG_RENAME && i == NDGAPPS)) snd_click();   /* (buttons and the list's rows (.odd div) sound; Always's box does not) */
	if (DGK == DLG_RENAME) { if (i == 0) dg_close(); else if (i == 1) dg_ok(); }
	else { if (i >= 0 && i < NDGAPPS) dg_pick(i); else if (i == NDGAPPS) { DGALW = !DGALW; gtk_widget_queue_draw(DGAREA); } else if (i == NDGAPPS + 1) dg_close(); }
	return TRUE;
}
static void dg_commit(GtkIMContext *im, const char *str, gpointer d)
{
	(void)im; (void)d; if (DGK != DLG_RENAME) return; if (DGSEL) { DGVAL[0] = 0; DGSEL = 0; }
	if (strlen(DGVAL) + strlen(str) < 60 + 1) strncat(DGVAL, str, sizeof DGVAL - strlen(DGVAL) - 1);   /* (maxlength 60) */
	DGCARET = 1; gtk_widget_queue_draw(DGAREA);
}
static gboolean dg_key(GtkWidget *w, GdkEventKey *e, gpointer d)
{
	(void)w; (void)d;
	if (e->keyval == GDK_KEY_Escape) { dg_close(); return TRUE; }
	if (DGK == DLG_OPENWITH) {
		if (e->keyval == GDK_KEY_Tab || e->keyval == GDK_KEY_Down) { DGFOC = (DGFOC + 1) % NDGAPPS; gtk_widget_queue_draw(DGAREA); }
		else if (e->keyval == GDK_KEY_ISO_Left_Tab || e->keyval == GDK_KEY_Up) { DGFOC = (DGFOC + NDGAPPS - 1) % NDGAPPS; gtk_widget_queue_draw(DGAREA); }
		else if (e->keyval == GDK_KEY_Return || e->keyval == GDK_KEY_KP_Enter || e->keyval == GDK_KEY_space) dg_pick(DGFOC);
		return TRUE; }
	if (e->keyval == GDK_KEY_Return || e->keyval == GDK_KEY_KP_Enter) { dg_ok(); return TRUE; }
	if (e->keyval == GDK_KEY_BackSpace || e->keyval == GDK_KEY_Delete) { if (DGSEL) { DGVAL[0] = 0; DGSEL = 0; } else if (e->keyval == GDK_KEY_BackSpace) { int n = (int)strlen(DGVAL); if (n) { const char *p = g_utf8_find_prev_char(DGVAL, DGVAL + n); DGVAL[p ? p - DGVAL : 0] = 0; } }
		DGCARET = 1; gtk_widget_queue_draw(DGAREA); return TRUE; }
	if (e->keyval == GDK_KEY_Left || e->keyval == GDK_KEY_Right || e->keyval == GDK_KEY_End || e->keyval == GDK_KEY_Home) { DGSEL = 0; gtk_widget_queue_draw(DGAREA); return TRUE; }
	if (gtk_im_context_filter_keypress(DGIM, e)) return TRUE;
	return TRUE;
}
static void dg_open(int kind)
{
	if (!DGWIN) { DGWIN = gtk_window_new(GTK_WINDOW_POPUP); gtk_window_set_title(GTK_WINDOW(DGWIN), "Dialog"); DGAREA = gtk_drawing_area_new(); gtk_widget_set_can_focus(DGAREA, TRUE);
		gtk_widget_add_events(DGAREA, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_KEY_PRESS_MASK); gtk_container_add(GTK_CONTAINER(DGWIN), DGAREA);
		g_signal_connect(DGAREA, "draw", G_CALLBACK(dg_draw), NULL); g_signal_connect(DGAREA, "motion-notify-event", G_CALLBACK(dg_motion), NULL);
		g_signal_connect(DGAREA, "button-press-event", G_CALLBACK(dg_press), NULL); g_signal_connect(DGAREA, "key-press-event", G_CALLBACK(dg_key), NULL);
		DGIM = gtk_im_multicontext_new(); g_signal_connect(DGIM, "commit", G_CALLBACK(dg_commit), NULL); g_signal_connect(DGAREA, "realize", G_CALLBACK(im_realize), DGIM); }
	cm_close(); if (DGBG) g_object_unref(DGBG); DGBG = screen_copy(0, 0, SW, SH);
	DGK = kind; DGHOV = -1; DGFOC = 0; DGALW = 0; DGBUSY = 0; DGMSG[0] = 0; DGCARET = 1;
	gtk_widget_set_size_request(DGAREA, SW, SH); gtk_window_resize(GTK_WINDOW(DGWIN), SW, SH); gtk_window_move(GTK_WINDOW(DGWIN), 0, 0);
	gtk_widget_show_all(DGWIN); XRaiseWindow(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), GDK_WINDOW_XID(gtk_widget_get_window(DGWIN)));
	seat_grab(DGWIN, TRUE);
	gtk_widget_grab_focus(DGAREA); gtk_im_context_focus_in(DGIM); if (DGCARET_T) g_source_remove(DGCARET_T); DGCARET_T = g_timeout_add(500, dg_blink, NULL); state_write();
}
static void rename_ask(const char *file, const char *name) { snprintf(DGFILE, sizeof DGFILE, "%s", file); snprintf(DGNAME, sizeof DGNAME, "%s", name); snprintf(DGVAL, sizeof DGVAL, "%s", name); DGSEL = 1; dg_open(DLG_RENAME); }
static void openwith_ask(const char *file, const char *ext, const char *apps_json)
{
	snprintf(DGFILE, sizeof DGFILE, "%s", file); const char *b = strrchr(file, '\\'); snprintf(DGNAME, sizeof DGNAME, "%s", b ? b + 1 : file); snprintf(DGEXT, sizeof DGEXT, "%s", ext);
	NDGAPPS = 0; char *w = g_strdup_printf("{\"a\":%s}", apps_json && *apps_json ? apps_json : "[]"); JV *v = jparse(w); g_free(w); JV *a = jprop(v, "a");
	for (int i = 0; a && i < a->len && NDGAPPS < 12; i++) { JV *o = a->v[i]; Opener *p = &DGAPPS[NDGAPPS]; if (!jat(o, 2)) continue;
		snprintf(p->k, sizeof p->k, "%s", jat(o, 0)->s ? jat(o, 0)->s : ""); snprintf(p->name, sizeof p->name, "%s", jat(o, 1)->s ? jat(o, 1)->s : ""); snprintf(p->icon, sizeof p->icon, "%s", jat(o, 2)->s ? jat(o, 2)->s : ""); NDGAPPS++; }
	jfree(v); if (NDGAPPS) dg_open(DLG_OPENWITH);
}
static gboolean apps_reload(gpointer d) { (void)d; apps_load(); return G_SOURCE_REMOVE; }
/* a page's answer to what the shell asked it to do (oneapp59.js oaFin) */
static void xd_done(JSCValue *v);   /* (1.6.1 Part 13: a drop's page finished, below) */
static void page_done(JSCValue *v)
{
	char err[300] = "", ask[24] = "", file[512] = "", name[128] = "", ext[16] = "", ok[8] = ""; char *apps = NULL;
	jsstr(v, "err", err, sizeof err); jsstr(v, "ask", ask, sizeof ask); jsstr(v, "file", file, sizeof file); jsstr(v, "name", name, sizeof name); jsstr(v, "ext", ext, sizeof ext); jsstr(v, "ok", ok, sizeof ok);
	JSCValue *ap = jsc_value_object_get_property(v, "apps"); if (ap && jsc_value_is_string(ap)) apps = jsc_value_to_string(ap); if (ap) g_object_unref(ap);
	apps_load();   /* (the page gave its catalogue again: the desktop's files as they are now) */
	{ char xd[4] = ""; jsstr(v, "xd", xd, sizeof xd); if (xd[0]) xd_done(v); }
	if (DGK == DLG_RENAME && DGBUSY) { DGBUSY = 0; if (err[0]) { snprintf(DGMSG, sizeof DGMSG, "%s", err); gtk_widget_queue_draw(DGAREA); } else dg_close(); }
	if (!strcmp(ask, "rename")) rename_ask(file, name);
	else if (!strcmp(ask, "openwith")) openwith_ask(file, ext, apps);
	g_free(apps); g_timeout_add(1500, apps_reload, NULL); state_write();
}

/* ------------------------------------------------------------------ files dropped from outside Rudolf OS (1.6.1, docs/plan-1.6.1.md Part 13)
   On Standard a file dragged from a Linux app (the Debian file manager, a Flatpak app, an AppImage) onto the desktop is read by
   Chromium and kept by the page in the Desktop folder (shell25.js, DESK's drop): anywhere on the desktop, also over a folder's icon
   or the Trash (their own drop handlers take only the page's own drags, FC.drag; the drop then reaches the desktop's); the taskbar
   takes none, and dropped text (no file) is refused. Here the desktop is an X drop target (XDND 5, GTK's): it says copy while a
   drag offering files (text/uri-list) is over it and a person is signed in, takes the file:// paths, and a page of its own
   (#app=do-fs&op=xdrop, oneapp59.js oaXdrop) does what Standard's page does with each file, reading it through rudolfd
   (desktop/dropped, rudolfd-drop.inc: the account's own files and the drives only, read by its Linux user; links refused); the
   drag is finished (XdndFinished, copy: the source keeps its file) once the page has kept them. Drops wait their turn (one page
   at a time, so two never put the same name). The paths go as JSON in the page's address, in parts of at most XD_PART. */
#define XD_PART 12000
typedef struct { char *ex; GdkDragContext *ctx; guint32 t; } XdJob;   /* (ctx: on a drop's last part only) */
static GQueue XD_Q = G_QUEUE_INIT; static XdJob *XD_CUR; static guint XD_TO; static int XD_N; static char XD_LAST[24] = "-";
static int xd_can(void) { return USER_ID[0] && !LOCKED && strcmp(USER_R, "guest") && strcmp(USER_ID, "guest"); }
/* (1.6.1, docs/plan-1.6.1.md Part 28: File Explorer to the desktop) a drag from one of the shell's own windows (an Explorer window's
   items: its page's FC.drag or FC.dragR) dropped on the desktop is done by that page, as Standard's page does it (deskxfer61.js
   __deskDrop): into the Desktop folder at the cell under the pointer, a desktop folder's icon (into it) or the Trash; a move, or a copy
   with Ctrl. The page writes the files and gives the shell the new icons' cell with its list (oaCatalog's "place", di_load). */
static int DI_HOV = -1, DXD_N; static char DXD_LAST[40] = "-";
static AppWin *xd_ours(GdkDragContext *c)
{
	GtkWidget *s = gtk_drag_get_source_widget(c); if (!s) return NULL;
	for (GList *l = WINS; l; l = l->next) { AppWin *a = l->data; if (a->view && !a->closing && (a->view == s || gtk_widget_is_ancestor(s, a->view))) return a; }
	return NULL;
}
static int xd_ctrl(GtkWidget *w, GdkDragContext *c)
{
	GdkModifierType m = 0; GdkWindow *gw = gtk_widget_get_window(w); GdkDevice *dv = gdk_drag_context_get_device(c);
	if (gw && dv) gdk_window_get_device_position(gw, dv, NULL, NULL, &m);
	return (m & GDK_CONTROL_MASK) || (gdk_drag_context_get_suggested_action(c) == GDK_ACTION_COPY && !(gdk_drag_context_get_actions(c) & GDK_ACTION_MOVE));
}
static int di_at(double x, double y); static void di_grid(int *nc, int *nr); static int di_w(void); static int di_h(void);
static const char *di_kind(int i); static const char *di_name(int i);
static void xd_hover(int t) { if (t != DI_HOV) { DI_HOV = t; if (DESKAREA) desk_dirty(); } }
static void xd_leave(GtkWidget *w, GdkDragContext *c, guint t, gpointer d) { (void)w; (void)c; (void)t; (void)d; xd_hover(-1); }
static int xd_files(GdkDragContext *c) { for (GList *l = gdk_drag_context_list_targets(c); l; l = l->next) { char *n = gdk_atom_name(GDK_POINTER_TO_ATOM(l->data)); int y = n && !strcmp(n, "text/uri-list"); g_free(n); if (y) return 1; } return 0; }
static void xd_next(void);
static void xd_end(int ok)
{
	XdJob *j = XD_CUR; XD_CUR = NULL; if (XD_TO) { g_source_remove(XD_TO); XD_TO = 0; }
	if (j) { if (j->ctx) { gtk_drag_finish(j->ctx, ok, FALSE, j->t); g_object_unref(j->ctx); XD_N++; } g_free(j->ex); g_free(j); }
	snprintf(XD_LAST, sizeof XD_LAST, "%s", ok ? "done" : "failed"); state_write(); xd_next();
}
static gboolean xd_late(gpointer d) { (void)d; XD_TO = 0; fprintf(stderr, "rudolf-native: a drop's page did not answer\n"); xd_end(0); return G_SOURCE_REMOVE; }
static void xd_next(void)
{
	if (XD_CUR) return; XD_CUR = g_queue_pop_head(&XD_Q); if (!XD_CUR) return;
	if (!xd_can()) { xd_end(0); return; }
	app_open_ex("do-fs", NULL, XD_CUR->ex, 1); XD_TO = g_timeout_add_seconds(120, xd_late, NULL);   /* (a part of 64 MB files takes a while) */
}
static void xd_done(JSCValue *v) { char err[64] = ""; jsstr(v, "err", err, sizeof err); if (XD_CUR) xd_end(!err[0]); }
static gboolean xd_motion(GtkWidget *w, GdkDragContext *c, gint x, gint y, guint t, gpointer d)
{
	(void)d; AppWin *src = xd_ours(c);
	if (src) {   /* (an Explorer window's items: move, or copy with Ctrl; a folder's icon and the Trash highlighted while over them) */
		int ok = xd_can() && !strcmp(src->k, "explorer"), i = ok ? di_at(x, y) : -1;
		xd_hover(i >= 0 && (!strcmp(di_kind(i), "dir") || !strcmp(di_kind(i), "trash")) ? i : -1);
		gdk_drag_status(c, ok ? (xd_ctrl(w, c) ? GDK_ACTION_COPY : GDK_ACTION_MOVE) : 0, t); return TRUE; }
	int ok = xd_can() && xd_files(c);
	gdk_drag_status(c, ok ? GDK_ACTION_COPY : 0, t); return TRUE;   /* (XdndStatus: copy, or refused) */
}
static gboolean xd_drop(GtkWidget *w, GdkDragContext *c, gint x, gint y, guint t, gpointer d)
{
	(void)d; AppWin *src = xd_ours(c); xd_hover(-1);
	if (src) {
		if (!xd_can() || strcmp(src->k, "explorer") || !src->view) { gtk_drag_finish(c, FALSE, FALSE, t); snprintf(DXD_LAST, sizeof DXD_LAST, "refused"); state_write(); return TRUE; }
		int i = di_at(x, y), nc, nr; di_grid(&nc, &nr); GString *js = g_string_new("window.__deskDrop?__deskDrop({");
		if (i >= 0 && !strcmp(di_kind(i), "dir")) { g_string_append(js, "on:"); json_esc(js, di_name(i)); snprintf(DXD_LAST, sizeof DXD_LAST, "folder"); }
		else if (i >= 0 && !strcmp(di_kind(i), "trash")) { g_string_append(js, "trash:1"); snprintf(DXD_LAST, sizeof DXD_LAST, "trash"); }
		else {   /* (the cell under the pointer, in the page's grid as di_xy draws it) */
			int c0 = (int)floor((x - desk_left() - 8 * Z) / di_w()), r0 = (int)floor((y - work_top() - ((!strcmp(S.tb, "top") ? 2 : 0) + 8) * Z) / di_h());
			if (c0 < 0) c0 = 0; if (r0 < 0) r0 = 0; if (c0 >= nc) c0 = nc - 1; if (r0 >= nr) r0 = nr - 1;
			g_string_append_printf(js, "c:%d,r:%d,nc:%d,nr:%d", c0, r0, nc, nr); snprintf(DXD_LAST, sizeof DXD_LAST, "desk %d,%d", c0, r0); }
		g_string_append_printf(js, ",copy:%d}):0", xd_ctrl(w, c));
		webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(src->view), js->str, -1, NULL, NULL, NULL, NULL, NULL); g_string_free(js, TRUE);
		gtk_drag_finish(c, TRUE, FALSE, t); DXD_N++; state_write(); return TRUE; }
	if (!xd_can() || !xd_files(c)) { gtk_drag_finish(c, FALSE, FALSE, t); snprintf(XD_LAST, sizeof XD_LAST, "refused"); state_write(); return TRUE; }
	gtk_drag_get_data(w, c, gdk_atom_intern_static_string("text/uri-list"), t); return TRUE;
}
static void xd_part(GString *b, int last, GdkDragContext *c, guint t)
{
	g_string_append_c(b, ']'); XdJob *j = g_new0(XdJob, 1); char *e = qesc(b->str); j->ex = g_strdup_printf("&op=xdrop&files=%s", e); g_free(e);
	if (last) { j->ctx = g_object_ref(c); j->t = t; } g_queue_push_tail(&XD_Q, j); g_string_assign(b, "[");
}
static void xd_received(GtkWidget *w, GdkDragContext *c, gint x, gint y, GtkSelectionData *sd, guint info, guint t, gpointer d)
{
	(void)w; (void)x; (void)y; (void)info; (void)d;
	char **u = sd ? gtk_selection_data_get_uris(sd) : NULL; GString *b = g_string_new("["); int n = 0, k = 0;
	for (int i = 0; u && u[i] && n < 256; i++) { char *host = NULL, *f = g_filename_from_uri(u[i], &host, NULL);   /* (file:// only, on this computer) */
		if (f && f[0] == '/' && (!host || !strcmp(host, "localhost"))) {
			GString *one = g_string_new(NULL); json_esc(one, f);
			if (k && b->len + one->len + 2 > XD_PART) { xd_part(b, 0, c, t); k = 0; }
			if (k) g_string_append_c(b, ','); g_string_append(b, one->str); g_string_free(one, TRUE); k++; n++; }
		g_free(f); g_free(host); }
	g_strfreev(u);
	if (!n) { g_string_free(b, TRUE); gtk_drag_finish(c, FALSE, FALSE, t); snprintf(XD_LAST, sizeof XD_LAST, "nofiles"); state_write(); return; }
	xd_part(b, 1, c, t); g_string_free(b, TRUE); snprintf(XD_LAST, sizeof XD_LAST, "copying"); state_write(); xd_next();
}
static void xd_init(GtkWidget *area)
{
	static const GtkTargetEntry T[] = { { (gchar *)"text/uri-list", GTK_TARGET_OTHER_APP, 0 } };
	gtk_drag_dest_set(area, 0, T, G_N_ELEMENTS(T), GDK_ACTION_COPY | GDK_ACTION_MOVE);
	g_signal_connect(area, "drag-leave", G_CALLBACK(xd_leave), NULL);
	g_signal_connect(area, "drag-motion", G_CALLBACK(xd_motion), NULL); g_signal_connect(area, "drag-drop", G_CALLBACK(xd_drop), NULL);
	g_signal_connect(area, "drag-data-received", G_CALLBACK(xd_received), NULL);
}

/* ------------------------------------------------------------------ the desktop's icons (1.6.0 Part 12, item 1)
   The page's desktop icons, as the page shows them for the account (desktop/apps "desk": each icon's name, what it
   opens and its picture; "cells": where the page put them), drawn on the wallpaper on the page's grid (84 x 86 at the
   icon size "is" and the display scaling). A click selects, a double-click (or Enter) opens, dragging moves an icon to
   the nearest free cell (kept by rudolfd for the shell: desktop/cells, the page's places used until then); the
   icon's menu (Open) and the desktop's (Arrange icons by name, Refresh, Display Properties, File Explorer, Task Manager,
   Settings). Files on the desktop and dropping files come with item 1's second part. */
typedef struct { char n[80], k[KN], kind[12], ic[24], icon[48], cell[96], fp[512]; int c, r, sel, full, own; } DIcon;   /* (own: a shortcut of the account's own, the page's .dapp) */
static struct { int i, on, moved; double sx, sy, dx, dy; } DID = { -1, 0, 0, 0, 0, 0, 0 };   /* (an icon being dragged, DX DY from its place) */   /* (fp: a file's or folder's path, cell: the page's key for its place) */
static DIcon DI[96]; static int NDI; static char DI_ACCT[80]; static int DI_IS = 100;
static char DI_MENU[8] = "-";   /* (the menu last opened, for the tests) */
static GHashTable *DI_CELLS;   /* name -> "c,r" (the shell's own, else the page's) */
static int di_w(void) { return (int)lround(84 * DI_IS / 100.0 * Z); }
static int di_h(void) { return (int)lround(86 * DI_IS / 100.0 * Z); }
static int di_shown(void) { return USER_ID[0] && !LOCKED && NDI && !strcmp(DI_ACCT, USER_ID); }
static int di_n(void) { return NDI; }
static const char *di_kind(int i) { return i >= 0 && i < NDI ? DI[i].kind : ""; }
static const char *di_name(int i) { return i >= 0 && i < NDI ? DI[i].n : ""; }
static void di_grid(int *nc, int *nr) { *nc = (desk_w() - 8) / di_w(); *nr = (work_h() - 8) / di_h(); if (*nc < 1) *nc = 1; if (*nr < 1) *nr = 1; }
static void di_xy(const DIcon *d, int *x, int *y) { *x = desk_left() + (int)lround(8 * Z) + d->c * di_w(); *y = work_top() + (int)lround(((!strcmp(S.tb, "top") ? 2 : 0) + 8) * Z) + d->r * di_h(); }   /* (the page's #desk: 2 px below a taskbar at the top) */
static int di_used(int c, int r, int skip) { for (int i = 0; i < NDI; i++) if (i != skip && DI[i].c == c && DI[i].r == r) return 1; return 0; }
/* the nearest free cell to (c, r), as the page's freeCell */
static void di_free(int c, int r, int skip, int *oc, int *orr)
{
	int nc, nr; di_grid(&nc, &nr);
	for (int k = 0; k < nc + nr; k++) for (int a = -k; a <= k; a++) for (int b = -k; b <= k; b++) {
		if (abs(a) != k && abs(b) != k) continue; int x = c + a, y = r + b;
		if (x >= 0 && y >= 0 && x < nc && y < nr && !di_used(x, y, skip)) { *oc = x; *orr = y; return; } }
	*oc = 0; *orr = 0;
}
/* the page's layIcons: icons with a place keep it (or the nearest free), the others fill the columns */
static void di_layout(void)
{
	int nc, nr; di_grid(&nc, &nr);
	for (int i = 0; i < NDI; i++) DI[i].c = DI[i].r = -1;
	for (int i = 0; i < NDI; i++) { const char *p = DI_CELLS ? g_hash_table_lookup(DI_CELLS, DI[i].cell) : NULL; int c, r;
		if (p && sscanf(p, "%d,%d", &c, &r) == 2) { int fc, fr; di_free(c < nc ? c : nc - 1, r < nr ? r : nr - 1, i, &fc, &fr); DI[i].c = fc; DI[i].r = fr; } }
	for (int i = 0; i < NDI; i++) if (DI[i].c < 0) { int done = 0;
		for (int c = 0; c < nc && !done; c++) for (int r = 0; r < nr && !done; r++) if (!di_used(c, r, i)) { DI[i].c = c; DI[i].r = r; done = 1; }
		if (!done) DI[i].c = DI[i].r = 0; }
}
static void di_save(void)
{
	if (!USER_ID[0]) return;
	GString *b = g_string_new("{");
	for (int i = 0; i < NDI; i++) { if (i) g_string_append_c(b, ','); json_esc(b, DI[i].cell); g_string_append_printf(b, ":[%d,%d]", DI[i].c, DI[i].r);
		char v[24]; snprintf(v, sizeof v, "%d,%d", DI[i].c, DI[i].r); g_hash_table_insert(DI_CELLS, g_strdup(DI[i].cell), g_strdup(v)); }
	g_string_append_c(b, '}'); char *a = g_uri_escape_string(USER_ID, NULL, FALSE), p[200]; snprintf(p, sizeof p, "/api/desktop/cells?acct=%s", a); g_free(a);
	http("POST", p, b->str, b->len, NULL); g_string_free(b, TRUE);
}
/* from desktop/apps (the page's list) and desktop/cells (the shell's places) */
static void di_load(JV *cat)
{
	/* (the chosen icons stay chosen when the catalogue comes again, as the page's elements keep their class) */
	char was[8][96]; int nw = 0; for (int i = 0; i < NDI && nw < 8; i++) if (DI[i].sel) g_strlcpy(was[nw++], DI[i].cell, sizeof was[0]);
	NDI = 0; DI_ACCT[0] = 0; if (!cat) return;
	jstr(cat, "acct", DI_ACCT, sizeof DI_ACCT); DI_IS = (int)jnum(cat, "is", 100); if (DI_IS < 50 || DI_IS > 200) DI_IS = 100;
	JV *a = jprop(cat, "desk");
	for (int i = 0; i < 96; i++) { JV *o = jat(a, i); if (!o) break; DIcon *d = &DI[NDI]; memset(d, 0, sizeof *d);
		jstr(o, "n", d->n, sizeof d->n); jstr(o, "k", d->k, sizeof d->k); jstr(o, "kind", d->kind, sizeof d->kind); jstr(o, "ic", d->ic, sizeof d->ic); jstr(o, "icon", d->icon, sizeof d->icon);
		jstr(o, "fp", d->fp, sizeof d->fp); jstr(o, "cell", d->cell, sizeof d->cell); if (!d->cell[0]) g_strlcpy(d->cell, d->n, sizeof d->cell); d->full = jbool(o, "full"); d->own = jbool(o, "own");
		for (int j = 0; j < nw; j++) if (!strcmp(was[j], d->cell)) d->sel = 1;
		if (d->n[0]) NDI++; }
	if (DI_CELLS) g_hash_table_destroy(DI_CELLS); DI_CELLS = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	JV *pc = jprop(cat, "cells");
	for (int i = 0; pc && pc->t == 5 && i < pc->len; i++) { JV *v = pc->v[i]; if (jat(v, 1)) { char s[24]; snprintf(s, sizeof s, "%d,%d", (int)jat(v, 0)->n, (int)jat(v, 1)->n); g_hash_table_insert(DI_CELLS, g_strdup(pc->k[i]), g_strdup(s)); } }
	if (DI_ACCT[0]) {   /* (the shell's own places, once it has moved an icon) */
		char *e = g_uri_escape_string(DI_ACCT, NULL, FALSE), p[200]; snprintf(p, sizeof p, "/api/desktop/cells?acct=%s", e); g_free(e);
		JV *mine = jget(p);
		for (int i = 0; mine && i < mine->len; i++) { JV *v = mine->v[i]; if (mine->k && jat(v, 1)) { char s[24]; snprintf(s, sizeof s, "%d,%d", (int)jat(v, 0)->n, (int)jat(v, 1)->n); g_hash_table_insert(DI_CELLS, g_strdup(mine->k[i]), g_strdup(s)); } }
		jfree(mine);
	}
	di_layout();
	/* (1.6.1 Part 28) icons just dropped or pasted on the desktop: the first at the cell the page was given (the pointer's, the menu's),
	   the others the nearest free ones, as the page's dkPlace (freeCell), here among the shell's own places; kept as the shell's */
	static double DI_PLT; JV *pl = jprop(cat, "place"), *pat = jprop(pl, "at"), *pn = jprop(pl, "n"); double pt = jnum(pl, "t", 0);
	if (pt > DI_PLT && jat(pat, 1) && jat(pat, 0)->t == 2 && jat(pat, 1)->t == 2 && pn && pn->t == 4) {
		DI_PLT = pt; int nc, nr, c0 = (int)jat(pat, 0)->n, r0 = (int)jat(pat, 1)->n, idx[96], k = 0; di_grid(&nc, &nr);
		if (c0 < 0) c0 = 0; if (r0 < 0) r0 = 0; if (c0 >= nc) c0 = nc - 1; if (r0 >= nr) r0 = nr - 1;
		for (int j = 0; j < pn->len && k < 96; j++) { JV *nv = pn->v[j]; if (!nv || nv->t != 3) continue; char cell[96]; snprintf(cell, sizeof cell, "file:%s", nv->s);
			for (int i = 0; i < NDI; i++) if (!strcmp(DI[i].cell, cell)) { DI[i].c = DI[i].r = -100; idx[k++] = i; break; } }
		for (int j = 0; j < k; j++) { int fc, fr; di_free(c0, r0, idx[j], &fc, &fr); DI[idx[j]].c = fc; DI[idx[j]].r = fr; }
		if (k) di_save();
	}
}
/* the desktop's icons as the page draws them (base_v24.html .ic, styles25.css): 76 px wide (the icon size's share),
   the picture 40 px in a line of the 34 px font (vertical-align: middle) with its drop shadow, the name below in the
   page's font with its text shadow, wrapped as the page wraps it; a selected icon's name on the accent colour */
static void di_draw(cairo_t *cr)
{
	if (!di_shown()) return;
	double k = DI_IS / 100.0, bw = 76 * k, pic = 40 * k, fpx = 13 * k;
	/* the picture's line: the 34 px font's ascent, descent and x-height (vertical-align: middle) */
	double a34, d34; a34 = css_ascent(cr, UIFAM, 400, 34 * k, &d34);
	double xh; { PangoLayout *pl = css_layout(cr, "x", UIFAM, 400, 34 * k); PangoRectangle ink; pango_layout_get_extents(pl, &ink, NULL); xh = ink.height / (double)PANGO_SCALE; g_object_unref(pl); }
	double imtop = -xh / 2 - pic / 2, ltop = fmin(-a34, imtop), lbot = fmax(d34, imtop + pic), bh = lbot - ltop;
	double fa, fd; fa = css_ascent(cr, UIFAM, 400, fpx, &fd);
	int drag = DID.on && DID.moved && DID.i >= 0 && DID.i < NDI;
	for (int j = 0; j < NDI; j++) { int i = !drag ? j : j == NDI - 1 ? DID.i : j < DID.i ? j : j + 1;   /* (the one dragged drawn last, over the others: z-index 5) */
		DIcon *d = &DI[i]; int sx, sy; di_xy(d, &sx, &sy); if (drag && i == DID.i) { sx += (int)lround(DID.dx); sy += (int)lround(DID.dy); } double X = sx / Z, Y = sy / Z;
		cairo_save(cr); cairo_scale(cr, Z, Z);
		/* the picture and its drop shadow (0 2px 3px rgba(0,0,0,.35)) */
		GdkPixbuf *p = d->ic[0] && ICONS ? g_hash_table_lookup(ICONS, d->ic) : NULL; double ix = round(X + (bw - pic) / 2), iy = round(Y + (imtop - ltop));
		if (p) {
			int mw = (int)ceil((pic + 16) * Z), mh = mw; cairo_surface_t *m = cairo_image_surface_create(CAIRO_FORMAT_A8, mw, mh); cairo_t *mc = cairo_create(m);
			cairo_scale(mc, Z, Z); pix_at(mc, p, 8, 8, pic, 0); cairo_destroy(mc); a8_blur(m, 1.5 * Z);
			cairo_save(cr); cairo_scale(cr, 1 / Z, 1 / Z); cairo_set_source_rgba(cr, 0, 0, 0, .35); cairo_mask_surface(cr, m, (ix - 8) * Z, (iy - 8 + 2) * Z); cairo_restore(cr); cairo_surface_destroy(m);
			pix_at(cr, p, ix, iy, pic, 0);
		} else css_text(cr, d->icon[0] ? d->icon : "▫", UIFAM, 400, 34 * k, X + (bw - css_width(cr, d->icon[0] ? d->icon : "▫", UIFAM, 400, 34 * k)) / 2, Y, bh, cx("#ffffff", 1), 0);
		/* the name */
		PangoLayout *pl = css_layout(cr, d->n, UIFAM, 400, fpx); pango_layout_set_width(pl, (int)((bw - 4) * PANGO_SCALE)); pango_layout_set_wrap(pl, PANGO_WRAP_WORD);
		pango_layout_set_alignment(pl, PANGO_ALIGN_CENTER); pango_layout_set_spacing(pl, 0);
		/* (each line centred in the 76 px with the name's 2 px padding at its start and its end: the first line's text
		   1 px to the right, the last line's 1 px to the left, as Chromium centres them) */
		double ty = Y + bh; int nl = pango_layout_get_line_count(pl);
		double lx[8], lw[8]; int ln = 0;
		{ PangoLayoutIter *it = pango_layout_get_iter(pl);
			do { PangoRectangle lr; pango_layout_iter_get_line_extents(it, NULL, &lr); if (ln < 8) { lw[ln] = lr.width / (double)PANGO_SCALE;
					double pl0 = ln == 0 ? 2 : 0, pr0 = ln == nl - 1 ? 2 : 0; lx[ln] = X + (bw - (lw[ln] + pl0 + pr0)) / 2 + pl0; } ln++;
			} while (pango_layout_iter_next_line(it));
			pango_layout_iter_free(it); }
		if (nl > 8) nl = 8;
		if (d->sel || i == DI_HOV) for (int l = 0; l < nl; l++) {   /* (.ic.sel span: the accent colour behind each line, 3 px corners) */
			double x0 = lx[l] - (l == 0 ? 2 : 0), w0 = lw[l] + (l == 0 ? 2 : 0) + (l == nl - 1 ? 2 : 0);
			Box sb; memset(&sb, 0, sizeof sb); sb.bg = ca(accent(), 1); sb.rad = 3; box_paint(cr, x0, ty + l * (fa + fd) - 1, w0, fa + fd + 2, &sb); }
		/* the text shadow (0 1px 2px rgba(0,0,0,.6)), then the name */
		int mw = (int)ceil((bw + 12) * Z), mh = (int)ceil((nl * (fa + fd) + 12) * Z);
		cairo_surface_t *m = cairo_image_surface_create(CAIRO_FORMAT_A8, mw, mh); cairo_t *mc = cairo_create(m); cairo_scale(mc, Z, Z);
		for (int pass = 0; pass < 2; pass++) { cairo_t *c2 = pass ? cr : mc; if (pass) cairo_set_source_rgb(cr, 1, 1, 1);
			for (int l = 0; l < nl; l++) { PangoLayoutLine *L = pango_layout_get_line_readonly(pl, l); PangoRectangle lr; pango_layout_line_get_extents(L, NULL, &lr);
				double bx = lx[l] - lr.x / (double)PANGO_SCALE, by = round(ty + l * (fa + fd) + fa);
				if (!pass) cairo_move_to(mc, bx - X + 6, by - ty + 6); else cairo_move_to(cr, bx, by);
				pango_cairo_show_layout_line(c2, L); }
			if (!pass) { cairo_destroy(mc); a8_blur(m, 1 * Z); cairo_save(cr); cairo_scale(cr, 1 / Z, 1 / Z); cairo_set_source_rgba(cr, 0, 0, 0, .6); cairo_mask_surface(cr, m, (X - 6) * Z, (ty - 6 + 1) * Z); cairo_restore(cr); cairo_surface_destroy(m); } }
		g_object_unref(pl);
		cairo_restore(cr);
	}
}
static gboolean desk_area_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	/* (the wallpaper and the icons kept as drawn: windows moved over the desktop only show it again, with their shadows) */
	if (!DESKC || DESKD || FXD) { if (!DESKC) DESKC = cairo_image_surface_create(CAIRO_FORMAT_RGB24, SW, SH); cairo_t *c2 = cairo_create(DESKC); desk_draw(w, c2, d); fx_draw(c2); bar_outside(c2); di_draw(c2); cairo_destroy(c2);
		if (DESKD) NWG_ALL_DIRTY = 1; DESKD = 0; FXD = 0; }   /* (a moving background's frame: the widgets keep what they show behind them) */
	cairo_set_source_surface(cr, DESKC, 0, 0); cairo_paint(cr); wg_paint(cr); frames_shadow(cr); return TRUE;   /* (the widgets over the desktop: item 2) */
}

/* ------------------------------------------------------------------ the dynamic backgrounds' moving parts (test findings N11, N12)
   On Standard, Glacier Lake at night has the northern lights over its sky and in its water (aurora58.js), and Spirit Falls its stone
   statue on the right bank, its falling petals and, at night, its sky lanterns (statue58.js); the native desktop drew the picture
   alone. Here they are drawn as the page draws them, over the picture and under the icons: the same curtains, colours (the accent
   colour by Display Properties' rule), masks (the picture's own, DYN.KEY.am, given with the catalogue), sizes and places (the
   picture's 1600×1067 laid as the background is: centre and cover), frames (the lights every 100 ms, the petals and lanterns every
   66 ms, as in Chromium) and stops (not while a window covers most of the screen or one is full screen, while locked or the screen
   saver runs; half the frames on battery; one still frame with Reduce motion). The statue is the page's own picture of it (its
   animal, light or dark, glow), drawn by the page and given with the catalogue (oneapp59.js oaCatalog: fx.statue). */
static GdkPixbuf *ST_PIX; static double ST_BOX[4]; static char ST_KEY[96];
static void fx_cat(JV *cat)
{
	JV *fx = jprop(cat, "fx"), *st = jprop(fx, "statue"); char png[64] = ""; jstr(st, "key", png, sizeof png);
	if (!st || !strcmp(png, ST_KEY)) { if (!st && ST_PIX) { g_object_unref(ST_PIX); ST_PIX = NULL; ST_KEY[0] = 0; } return; }
	snprintf(ST_KEY, sizeof ST_KEY, "%s", png); if (ST_PIX) { g_object_unref(ST_PIX); ST_PIX = NULL; }
	JV *b = jprop(st, "box"); for (int i = 0; i < 4; i++) { JV *x = jat(b, i); ST_BOX[i] = x && x->t == 2 ? x->n : 0; }
	JV *u = jprop(st, "png"); static const char PRE[] = "data:image/png;base64,";
	if (u && u->t == 3 && !strncmp(u->s, PRE, sizeof PRE - 1)) { gsize n = 0; guchar *raw = g_base64_decode(u->s + sizeof PRE - 1, &n); GByteArray *ba = g_byte_array_new_take(raw, n); guint8 z = 0; g_byte_array_append(ba, &z, 1); ST_PIX = pix_from_bytes(ba); g_byte_array_free(ba, TRUE); }
}
static GdkPixbuf *FX_MASK[2]; static char FX_MASKP[2][200];   /* (0: the lake's, 1: the falls') */
static cairo_surface_t *AUR_SKY, *AUR_CV, *PET_CV, *LAN_CV, *FX_MS[2];
static guint FX_T, PET_T; static gint64 FX_T0, PET_LAST; static int FX_SLOWT;
static void cover(double *s, double *ox, double *oy) { *s = fmax(SW / 1600.0, SH / 1067.0); *ox = (SW - 1600 * *s) / 2; *oy = (SH - 1067 * *s) / 2; }
static int fx_shown(void) { return S.dyn[0] && !S.wpimg && BG && g_hash_table_lookup(BG, S.dyn); }   /* (dynShown: not a picture of one's own) */
static int fx_aur(void) { char k[64]; snprintf(k, sizeof k, "am:%s", "lake"); return S.aur && !strcmp(S.dyn, "lake") && g_hash_table_lookup(BG, k) && fx_shown() && is_dark(); }
static int fx_oasis(void) { return !strcmp(S.dyn, "oasis") && fx_shown(); }
/* (DYNOV: seen, not covered by a window over 60 % of the screen, nothing full screen; with the power: half the frames on battery) */
static int fx_seen(void)
{
	if (LOCKED || !USER_ID[0]) return 0;
	for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (c->min || c->off || c->hidden || c->trans) continue;
		if (c->full) return 0; if ((double)fw_of(c) * fh_of(c) >= .6 * SW * SH) return 0; if (c->app && !strcmp(c->app->k, "do-saver")) return 0; }
	return 1;
}
static cairo_surface_t *fx_mask(int i)
{
	char k[64]; snprintf(k, sizeof k, "am:%s", i ? "oasis" : "lake"); const char *am = BG ? g_hash_table_lookup(BG, k) : NULL; if (!am) return NULL;
	if (strcmp(FX_MASKP[i], am)) { snprintf(FX_MASKP[i], sizeof FX_MASKP[i], "%s", am); if (FX_MASK[i]) g_object_unref(FX_MASK[i]); char pth[220]; snprintf(pth, sizeof pth, "/%s", am); FX_MASK[i] = pix_get(pth);
		if (FX_MS[i]) { cairo_surface_destroy(FX_MS[i]); FX_MS[i] = NULL; } }
	if (!FX_MASK[i]) return NULL;
	if (!FX_MS[i]) { int w = i ? 800 : 400, h = i ? 534 : 267; FX_MS[i] = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h); cairo_t *c = cairo_create(FX_MS[i]);
		cairo_scale(c, (double)w / gdk_pixbuf_get_width(FX_MASK[i]), (double)h / gdk_pixbuf_get_height(FX_MASK[i])); gdk_cairo_set_source_pixbuf(c, FX_MASK[i], 0, 0); cairo_paint(c); cairo_destroy(c); }
	return FX_MS[i];
}
/* ---- the northern lights (aurora58.js: aurColours, aurStrip, aurDraw) */
static const struct { double y0, amp, k, sp, h, ph, a; } AUR_RIB[3] = {{250, 50, .0042, .11, 210, 0, .78}, {300, 34, .0061, -.08, 150, 2.1, .6}, {190, 30, .0029, .06, 130, 4.2, .45}};
static void aur_draw(void)
{
	cairo_surface_t *m = fx_mask(0); if (!m) return;
	RGB base = variant(accent(), S.aurm[0] ? S.aurm : "normal"); double h, sa, l; rgb2hsl(base, &h, &sa, &l); sa = fmax(sa, 38);
	RGB lo = hsl2rgb(h, sa, fmax(52, fmin(68, l + 8))), hi = hsl2rgb(h + 38, fmax(30, sa - 8), fmax(40, fmin(58, l)));
	int still = S.rmotion; double t = still ? 40 : (g_get_monotonic_time() - FX_T0) / 1e6;
	if (!AUR_SKY) { AUR_SKY = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 400, 267); AUR_CV = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 400, 267); }
	cairo_t *k = cairo_create(AUR_SKY); cairo_set_operator(k, CAIRO_OPERATOR_CLEAR); cairo_paint(k); cairo_set_operator(k, CAIRO_OPERATOR_OVER); cairo_scale(k, .25, .25);
	for (int i = 0; i < 3; i++) for (int x = -24; x < 1624; x += 8) { const double *r = &AUR_RIB[i].y0;
		double by = r[0] + r[1] * sin(r[2] * x + r[5] + t * r[3]) + 12 * sin(.013 * x - t * .4 + r[5]),
			a = r[6] * (.5 + .5 * sin(.021 * x + t * 1.1 + r[5])) * (.55 + .45 * sin(.0057 * x - t * .45)) + .06, hh = r[4] * (.7 + .3 * sin(.009 * x + t * .5 + r[5]));
		a = fmax(0, fmin(1, a)); if (a <= 0 || hh <= 0) continue;
		cairo_pattern_t *g = cairo_pattern_create_linear(0, by, 0, by - hh);
		cairo_pattern_add_color_stop_rgba(g, 0, lo.r, lo.g, lo.b, 0); cairo_pattern_add_color_stop_rgba(g, .04, lo.r, lo.g, lo.b, a); cairo_pattern_add_color_stop_rgba(g, .22, lo.r, lo.g, lo.b, .75 * a);
		cairo_pattern_add_color_stop_rgba(g, .6, hi.r, hi.g, hi.b, .32 * a); cairo_pattern_add_color_stop_rgba(g, 1, hi.r, hi.g, hi.b, 0);
		cairo_rectangle(k, x, by - hh, 9, hh); cairo_set_source(k, g); cairo_fill(k); cairo_pattern_destroy(g); }
	cairo_destroy(k);
	cairo_t *g = cairo_create(AUR_CV); cairo_set_operator(g, CAIRO_OPERATOR_SOURCE); cairo_set_source_surface(g, AUR_SKY, 0, 0); cairo_paint(g);
	/* (the lake: the same light upside down about the water line, twice; the mask keeps it to the open water and dims it) */
	cairo_set_operator(g, CAIRO_OPERATOR_ADD); cairo_save(g); cairo_matrix_t mx; cairo_matrix_init(&mx, 1, 0, 0, -1, 0, 2 * 550 * .25); cairo_transform(g, &mx);
	cairo_set_source_surface(g, AUR_SKY, 0, 0); cairo_paint(g); cairo_paint(g); cairo_restore(g);
	cairo_set_operator(g, CAIRO_OPERATOR_DEST_IN); cairo_set_source_surface(g, m, 0, 0); cairo_paint(g); cairo_destroy(g);
}
/* ---- Spirit Falls: the petals (both), the sky lanterns (at night), statue58.js petNew, petStep, petDraw, lanNew, lanStep, lanDraw */
typedef struct { double x, y, s, ph, sp, vy, wy, life; int landed; RGB c; } Petal; typedef struct { double x, y, z, ph, vy, fl, life; } Lantern;
static Petal PS[70]; static Lantern LS[28]; static int PET_MODE = -1;   /* (0 light, 1 dark) */
static double rnd(void) { return g_random_double(); }
static void pet_new(Petal *p, int any)
{
	static const char *L[] = {"#ffc2d6", "#ffe3ec", "#ff9fc0", "#ffd3e1"}, *D[] = {"#e79ac0", "#f6c4dc", "#c66d9e", "#f0b3d0"};
	p->x = any ? rnd() * 1700 - 60 : rnd() * 640 - 80; p->y = any ? 60 + rnd() * 700 : 90 + rnd() * 380; p->s = 3 + rnd() * 3.5; p->ph = rnd() * 6.283; p->sp = .6 + rnd() * .8;
	p->vy = 10 + rnd() * 22; p->wy = 648 + pow(rnd(), .7) * 410; p->c = hexc((PET_MODE == 0 ? L : D)[(int)(rnd() * 4) & 3], "#ffc2d6"); p->landed = 0; p->life = 1;
}
static void lan_new(Lantern *p, int any)
{
	double z = .35 + rnd() * .65, x = rnd() < .74 ? rnd() * 1180 : 1180 + rnd() * 440; p->x = x; p->y = any ? 10 + rnd() * 190 : 190 + rnd() * 40; p->z = z; p->ph = rnd() * 6.283; p->vy = (3 + rnd() * 5) * z; p->fl = rnd() * 6.283; p->life = any ? 1 : 0;
}
static void pet_step(double dt, double t)
{
	double wind = 34 + 20 * sin(t * .23) + 10 * sin(t * .71 + 1);
	for (int i = 0; i < 70; i++) { Petal *p = &PS[i];
		if (!p->landed) { p->x += (wind * p->sp + 14 * sin(t * 1.3 + p->ph)) * dt; p->y += (p->vy + 10 * sin(t * 1.7 + p->ph * 2)) * dt; if (p->y >= p->wy) { p->landed = 1; p->y = p->wy; } }
		else { p->x += wind * .12 * dt; p->life -= dt / 9; }
		if (p->x > 1680 || p->life <= 0) pet_new(p, 0); }
	for (int i = 0; i < 28; i++) { Lantern *p = &LS[i]; p->y -= p->vy * dt; p->x += sin(t * .4 + p->ph) * 3 * dt * p->z; p->life = fmin(1, p->life + dt / 4); if (p->y < -30) lan_new(p, 0); }
}
static void pet_draw(double t)
{
	if (!PET_CV) { PET_CV = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 800, 534); LAN_CV = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 800, 534); }
	cairo_t *g = cairo_create(LAN_CV); cairo_set_operator(g, CAIRO_OPERATOR_CLEAR); cairo_paint(g); cairo_set_operator(g, CAIRO_OPERATOR_OVER);
	cairo_surface_t *m = fx_mask(1);
	if (PET_MODE == 1 && m) {
		RGB o = variant(accent(), S.lnm[0] ? S.lnm : "normal"); double h, sa, l; rgb2hsl(o, &h, &sa, &l); double s2 = fmax(sa, 60);
		RGB body = hsl2rgb(h, s2, fmax(60, fmin(72, l + 14))), glow = hsl2rgb(h, s2, 62), core = hsl2rgb(h, fmin(100, s2 + 10), 88);
		cairo_save(g); cairo_scale(g, .5, .5);
		for (int i = 0; i < 28; i++) { Lantern *p = &LS[i]; double sz = 14 * p->z, fl = .85 + .15 * sin(t * 7 + p->fl) * sin(t * 3.1 + p->ph), a = fmax(0, fmin(1, p->life)); if (a <= 0) continue;
			cairo_pattern_t *gr = cairo_pattern_create_radial(p->x, p->y, 0, p->x, p->y, sz * 3.2); cairo_pattern_add_color_stop_rgba(gr, 0, glow.r, glow.g, glow.b, .55 * fl * a); cairo_pattern_add_color_stop_rgba(gr, 1, glow.r, glow.g, glow.b, 0);
			cairo_rectangle(g, p->x - sz * 3.2, p->y - sz * 3.2, sz * 6.4, sz * 6.4); cairo_set_source(g, gr); cairo_fill(g); cairo_pattern_destroy(gr);
			cairo_move_to(g, p->x - sz * .55, p->y - sz * .75); cairo_line_to(g, p->x + sz * .55, p->y - sz * .75); cairo_line_to(g, p->x + sz * .42, p->y + sz * .6); cairo_line_to(g, p->x - sz * .42, p->y + sz * .6); cairo_close_path(g);
			src(g, body, a); cairo_fill_preserve(g);
			cairo_pattern_t *cg = cairo_pattern_create_linear(0, p->y - sz * .75, 0, p->y + sz * .6); cairo_pattern_add_color_stop_rgba(cg, 0, 1, 1, 1, 0); cairo_pattern_add_color_stop_rgba(cg, 1, core.r, core.g, core.b, .9 * fl * a);
			cairo_set_source(g, cg); cairo_fill(g); cairo_pattern_destroy(cg);
			cairo_set_source_rgba(g, 40 / 255.0, 20 / 255.0, 20 / 255.0, .35 * a); cairo_rectangle(g, p->x - sz * .55, p->y - sz * .78, sz * 1.1, sz * .12); cairo_fill(g);
			cairo_set_source_rgba(g, 1, 240 / 255.0, 200 / 255.0, fl * a); cairo_arc(g, p->x, p->y + sz * .5, sz * .12, 0, 6.283); cairo_fill(g); }
		cairo_restore(g); cairo_set_operator(g, CAIRO_OPERATOR_DEST_IN); cairo_set_source_surface(g, m, 0, 0); cairo_paint(g); }
	cairo_destroy(g);
	g = cairo_create(PET_CV); cairo_set_operator(g, CAIRO_OPERATOR_CLEAR); cairo_paint(g); cairo_set_operator(g, CAIRO_OPERATOR_OVER); cairo_scale(g, .5, .5);
	for (int i = 0; i < 70; i++) { Petal *p = &PS[i]; cairo_save(g); cairo_translate(g, p->x, p->y); double al = fmax(0, fmin(1, p->life)) * (p->landed ? .85 : 1);
		if (p->landed) { double tp = (p->y - 640) / 427; cairo_scale(g, 1, .4 + .2 * tp); } else { cairo_rotate(g, p->ph + t * p->sp * 1.6); cairo_scale(g, 1, .25 + .75 * fabs(cos(t * 2 * p->sp + p->ph))); }
		cairo_scale(g, p->s, p->s * .62); cairo_arc(g, 0, 0, 1, 0, 6.283); cairo_restore(g); src(g, p->c, al); cairo_fill(g); }
	cairo_destroy(g);
}
static gboolean aur_tick(gpointer d) { (void)d; if (!fx_seen()) return G_SOURCE_CONTINUE; if (!CHARGING && BAT >= 0 && (FX_SLOWT ^= 1)) return G_SOURCE_CONTINUE; aur_draw(); FXD = 1; if (DESKAREA) gtk_widget_queue_draw(DESKAREA); return G_SOURCE_CONTINUE; }
static gboolean pet_tick(gpointer d)
{
	(void)d; if (!fx_seen()) { PET_LAST = 0; return G_SOURCE_CONTINUE; } if (!CHARGING && BAT >= 0 && (FX_SLOWT ^= 1)) return G_SOURCE_CONTINUE;
	gint64 now = g_get_monotonic_time(); double dt = fmin(.2, PET_LAST ? (now - PET_LAST) / 1e6 : 0); PET_LAST = now; double t = (now - FX_T0) / 1e6;
	pet_step(dt, t); pet_draw(t); FXD = 1; if (DESKAREA) gtk_widget_queue_draw(DESKAREA); return G_SOURCE_CONTINUE;
}
/* (aurUpdate, stUpdate: what is shown with these settings; the timers started or stopped) */
static void fx_update(void)
{
	if (!FX_T0) FX_T0 = g_get_monotonic_time();
	int a = BG && fx_aur(), o = BG && fx_oasis();
	if (!a && FX_T) { g_source_remove(FX_T); FX_T = 0; }
	if (a) { aur_draw(); if (!S.rmotion && !FX_T) FX_T = g_timeout_add(100, aur_tick, NULL); else if (S.rmotion && FX_T) { g_source_remove(FX_T); FX_T = 0; } }
	if (!o && PET_T) { g_source_remove(PET_T); PET_T = 0; }
	if (o) { int mode = is_dark(); if (mode != PET_MODE) { PET_MODE = mode; for (int i = 0; i < 70; i++) pet_new(&PS[i], 1); for (int i = 0; i < 28; i++) lan_new(&LS[i], 1); }
		pet_draw(S.rmotion ? 0 : (g_get_monotonic_time() - FX_T0) / 1e6);
		if (!S.rmotion && !PET_T) PET_T = g_timeout_add(66, pet_tick, NULL); else if (S.rmotion && PET_T) { g_source_remove(PET_T); PET_T = 0; } }
	FXD = 1;
}
static void fx_draw(cairo_t *cr)
{
	if (!BG) return; double s, ox, oy; cover(&s, &ox, &oy);
	if (fx_aur() && AUR_CV) { cairo_save(cr); cairo_translate(cr, ox, oy); cairo_scale(cr, 1600 * s / 400, 1067 * s / 267); cairo_set_source_surface(cr, AUR_CV, 0, 0);
		cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR); cairo_paint(cr); cairo_restore(cr); }
	if (fx_oasis()) {
		if (LAN_CV && PET_MODE == 1) { cairo_save(cr); cairo_translate(cr, ox, oy); cairo_scale(cr, 1600 * s / 800, 1067 * s / 534); cairo_set_source_surface(cr, LAN_CV, 0, 0); cairo_paint(cr); cairo_restore(cr); }
		if (ST_PIX && ST_BOX[2] > 0) { cairo_save(cr); cairo_translate(cr, ox + ST_BOX[0] * s, oy + ST_BOX[1] * s);
			cairo_scale(cr, ST_BOX[2] * s / gdk_pixbuf_get_width(ST_PIX), ST_BOX[3] * s / gdk_pixbuf_get_height(ST_PIX)); gdk_cairo_set_source_pixbuf(cr, ST_PIX, 0, 0); cairo_paint(cr); cairo_restore(cr); }
		if (PET_CV) { cairo_save(cr); cairo_translate(cr, ox, oy); cairo_scale(cr, 1600 * s / 800, 1067 * s / 534); cairo_set_source_surface(cr, PET_CV, 0, 0); cairo_paint(cr); cairo_restore(cr); } }
}
static int di_at(double x, double y)
{
	if (!di_shown()) return -1;
	for (int i = 0; i < NDI; i++) { int ix, iy; di_xy(&DI[i], &ix, &iy); if (x >= ix && x < ix + di_w() && y >= iy && y < iy + di_h()) return i; }
	return -1;
}
static void di_page(const char *on, const char *item, int hidden);
static void di_open(int i)
{
	if (i < 0 || i >= NDI) return; DIcon *d = &DI[i];
	if (d->fp[0]) { di_page(d->n, "Open", 0); return; }   /* (a file: the page opens it with its app, a folder in File Explorer) */
	if (d->k[0]) app_open(d->k, NULL); else if (!strcmp(d->kind, "trash")) app_open("trash", NULL);
}
static void di_select(int i) { for (int k = 0; k < NDI; k++) DI[k].sel = k == i; if (DESKAREA) desk_dirty(); state_write(); }
static int di_cmp(const void *a, const void *b) { return g_utf8_collate(((const DIcon *)a)->n, ((const DIcon *)b)->n); }
static void di_arrange(void)
{
	qsort(DI, NDI, sizeof DI[0], di_cmp); int nc, nr; di_grid(&nc, &nr);
	for (int i = 0; i < NDI; i++) { DI[i].c = i / nr; DI[i].r = i % nr; }
	di_save(); desk_dirty(); state_write();
}
/* what the desktop's menus' items do: the shell's own (Arrange, Refresh, the apps), else the page's (#app=do-desk:
   unseen when the page's item opens no window: New folder, New text document, Move to Trash, Remove from desktop) */
static int di_sel(void) { for (int i = 0; i < NDI; i++) if (DI[i].sel) return i; return -1; }
/* (cell: 1.6.1 Part 28, Paste's: "C,R&nc=..&nr=.." the cell the menu was opened over, in the shell's grid; "-": none, as Ctrl+V) */
static void di_page_at(const char *on, const char *item, int hidden, const char *cell)
{
	char *o = qesc(on ? on : ""), *t = qesc(item), *ex = g_strdup_printf("%s%s&item=%s%s%s", on && *on ? "&on=" : "", on && *on ? o : "", t, cell ? "&cell=" : "", cell ? cell : "");
	app_open_ex("do-desk", NULL, ex, hidden); g_free(o); g_free(t); g_free(ex);
}
static void di_page(const char *on, const char *item, int hidden) { di_page_at(on, item, hidden, NULL); }
static char DI_MCELL[48] = "-";   /* (the cell under the desktop's menu) */
static void di_mcell(double x, double y)
{
	int nc, nr; di_grid(&nc, &nr); int c0 = (int)floor((x - desk_left() - 8 * Z) / di_w()), r0 = (int)floor((y - work_top() - ((!strcmp(S.tb, "top") ? 2 : 0) + 8) * Z) / di_h());
	if (c0 < 0) c0 = 0; if (r0 < 0) r0 = 0; if (c0 >= nc) c0 = nc - 1; if (r0 >= nr) r0 = nr - 1; snprintf(DI_MCELL, sizeof DI_MCELL, "%d,%d&nc=%d&nr=%d", c0, r0, nc, nr);
}
static void di_arrange(void);
/* (the owner's Vivobook, 2026-11-16: a new folder or text document showed in File Explorer long before it was on the desktop) done by the
   desktop's service page, signed in already (oneapp59.js __oaDeskNew), not by a page opened for it that had to load and sign in first */
static AppWin *svc_page(void);
static int svc_desk_new(int dir)
{
	AppWin *sv = svc_page(); if (!sv || !sv->sready || !sv->view || sv->closing) return 0;
	webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(sv->view), dir ? "window.__oaDeskNew&&__oaDeskNew(1)" : "window.__oaDeskNew&&__oaDeskNew(0)", -1, NULL, NULL, NULL, NULL, NULL);
	return 1;
}
static void on_di_act(const char *a)
{
	int i = di_sel(); DIcon *d = i >= 0 ? &DI[i] : NULL;
	if (!strcmp(a, "arrange")) di_arrange();
	else if (!strcmp(a, "refresh")) apps_load();
	else if (!strncmp(a, "app:", 4)) app_open(a + 4, NULL);
	else if (!strcmp(a, "wgpick")) wg_picker();
	else if (!strcmp(a, "open") && d) di_open(i);
	else if (!strcmp(a, "rename") && d && d->fp[0]) rename_ask(d->fp, d->n);
	else if (!strncmp(a, "page:", 5)) di_page(NULL, a + 5, 0);
	/* (1.6.1 Part 28: Paste, at the menu's cell; it and Paste into folder by the shell's hidden page, as New folder (its files reach the other
	   windows at once; a page of its own that closes again did not pass them on); a long one from a drive ends with a
	   notification, as on Standard; the progress window Standard also shows is not drawn by the hidden page) */
	else if (!strcmp(a, "pageh:Paste")) di_page_at(NULL, "Paste", 1, DI_MCELL);
	else if (!strncmp(a, "pageh:", 6) && (!strcmp(a + 6, "New folder") || !strcmp(a + 6, "New text document")) && svc_desk_new(!strcmp(a + 6, "New folder"))) {}
	else if (!strncmp(a, "pageh:", 6)) di_page(NULL, a + 6, 1);
	else if (!strncmp(a, "icon:", 5) && d) di_page(d->n, a + 5, 0);
	else if (!strncmp(a, "iconh:", 6) && d) di_page(d->n, a + 6, 1);
}
#define CM(L, I, K, A) { L, I, K, A, 0, 0 }
#define CMS { "", "", "", "", 1, 0 }
static void di_menu(GdkEventButton *e, int icon)
{
	/* the page's menus, item by item (base_v24.html deskMenu, iconMenu; shell25.js's for files and the Trash) */
	static const CMI DESKM[] = { CM("New folder", "📁", "", "pageh:New folder"), CM("New text document", "📄", "", "pageh:New text document"), CM("Add a widget…", "🧩", "", "wgpick"), CMS,
		CM("Paste", "", "Ctrl+V", "pageh:Paste"), CM("Arrange icons by name", "", "", "arrange"), CM("Desktop icons…", "🖥️", "", "page:Desktop icons…"), CM("Refresh", "", "F5", "refresh"), CMS,
		CM("Open File Explorer", "", "", "app:explorer"), CM("Open Command Prompt", "", "", "app:prompt"), CM("Show all windows", "", "Super+Tab", "overview"), CM("Task Manager", "📊", "Ctrl+Shift+Esc", "app:taskmgr"), CMS,
		CM("Open settings…", "", "", "app:settings") };
	static const CMI FILEM[] = { CM("Open", "↗", "", "icon:Open"), CM("Open with…", "", "", "iconh:Open with…"), CMS, CM("Cut", "", "Ctrl+X", "iconh:Cut"), CM("Copy", "", "Ctrl+C", "iconh:Copy"),
		CM("Paste into folder", "", "", "iconh:Paste into folder"), CMS, CM("Rename", "", "F2", "rename"), CM("Move to Trash", "", "Del", "iconh:Move to Trash"), CMS,
		CM("Properties", "", "", "icon:Properties") };
	static const CMI TRASHM[] = { CM("Open", "↗", "Enter", "open"), CM("Empty trash…", "", "", "icon:Empty trash…"), CMS, CM("Arrange icons by name", "", "", "arrange"), CMS, CM("Properties", "", "", "icon:Properties") };
	static const CMI APPM[] = { CM("Open", "↗", "Enter", "open"), CM("Remove from desktop", "➖", "Del", "iconh:Remove from desktop"), CMS, CM("Arrange icons by name", "", "", "arrange"),
		CM("Desktop icons…", "", "", "icon:Desktop icons…"), CMS, CM("Properties", "", "", "icon:Properties") };
	int i = di_sel(); DIcon *d = icon && i >= 0 ? &DI[i] : NULL; CMI m[24]; int n = 0; const char *title = "Desktop";
	if (!d) { n = G_N_ELEMENTS(DESKM); memcpy(m, DESKM, sizeof DESKM); for (int j = 0; j < n; j++) if (!strcmp(m[j].act, "pageh:Paste")) m[j].dis = !FC_N;   /* (greyed while nothing is cut or copied) */
		di_mcell(e->x, e->y); }
	else if (d->fp[0]) { n = G_N_ELEMENTS(FILEM); memcpy(m, FILEM, sizeof FILEM); title = d->n;   /* (Paste into folder: a folder's only, greyed while nothing is cut or copied) */
		for (int j = 0; j < n; j++) if (!strcmp(m[j].act, "iconh:Paste into folder")) { if (strcmp(d->kind, "dir")) { memmove(&m[j], &m[j + 1], (n - j - 1) * sizeof m[0]); n--; } else m[j].dis = !FC_N; break; } }
	else if (!strcmp(d->kind, "trash")) { n = G_N_ELEMENTS(TRASHM); memcpy(m, TRASHM, sizeof TRASHM); m[1].dis = !d->full; title = d->n; }
	else { n = G_N_ELEMENTS(APPM); memcpy(m, APPM, sizeof APPM); title = d->n; }
	CM_RING = !d; cm_open(e->x_root, e->y_root, title, m, n, on_di_act);
	snprintf(DI_MENU, sizeof DI_MENU, "%s", d ? (d->fp[0] ? "file" : "icon") : "desk"); state_write();
}
/* (1.6.1 Part 28) a click on the desktop gives it the keys, as the page's desktop takes them on Standard (Ctrl+V, Delete, F2 there, not in
   the window in front); the window keeps looking in use, as on Standard, and a click into it gives them back (its clicks watched again) */
static void desk_keys(void)
{
	if (!DESK || !gtk_widget_get_window(DESK) || !USER_ID[0] || LOCKED || (MENU && gtk_widget_get_visible(MENU)) || (SIGN && gtk_widget_get_visible(SIGN))) return;
	if (WM_OK && FOCUS && g_list_find(CLIENTS, FOCUS)) grab_clicks(FOCUS, 1);
	XSetInputFocus(DPY, GDK_WINDOW_XID(gtk_widget_get_window(DESK)), RevertToPointerRoot, CurrentTime); if (DESKAREA) gtk_widget_grab_focus(DESKAREA);
}
static gboolean desk_press(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)w; (void)d; menu_hide(); if (fl_outside()) return TRUE; if (wg_press(e)) return TRUE;   /* (a widget: item 2) */
	if (e->type == GDK_BUTTON_PRESS && e->button == 1) desk_keys();
	int i = di_at(e->x, e->y);
	if (e->type == GDK_2BUTTON_PRESS && e->button == 1) { if (i >= 0) di_open(i); return TRUE; }
	if (e->type != GDK_BUTTON_PRESS) return TRUE;
	if (e->button == 3) { if (!USER_ID[0] || LOCKED) return TRUE; di_select(i); di_menu(e, i >= 0); return TRUE; }
	if (e->button == 1) { di_select(i); DID.i = i; DID.on = i >= 0; DID.moved = 0; DID.sx = e->x; DID.sy = e->y; }
	return TRUE;
}
static gboolean desk_motion(GtkWidget *w, GdkEventMotion *e, gpointer d)
{
	(void)w; (void)d; if (!DID.on) { wg_motion(e); return FALSE; }
	if (!DID.moved && hypot(e->x - DID.sx, e->y - DID.sy) < 5) return FALSE;
	DID.moved = 1; DID.dx = e->x - DID.sx; DID.dy = e->y - DID.sy; desk_dirty(); return FALSE;   /* (the icon follows the pointer, as the page's) */
}
/* the window seen at X Y (the screen's pixels): the top one whose frame is there */
static Client *client_at(int x, int y)
{
	Window r, p, *k = NULL; unsigned n = 0; Client *best = NULL;
	if (XQueryTree(DPY, ROOT, &r, &p, &k, &n)) for (unsigned i = 0; i < n; i++) { Client *c = client_of(k[i]); if (!c || c->min || c->hidden || c->frame != k[i]) continue;
		XWindowAttributes a; if (XGetWindowAttributes(DPY, c->frame, &a) && a.map_state == IsViewable && x >= a.x && x < a.x + a.width && y >= a.y && y < a.y + a.height) best = c; }
	if (k) XFree(k); return best;
}
static gboolean desk_release(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)w; (void)d; if (wg_release(e)) return TRUE;
	if (e->button != 1 || !DID.on) return TRUE; DID.on = 0;
	if (DID.moved && DID.i >= 0 && DID.i < NDI) {
		DIcon *dd = &DI[DID.i]; DID.moved = 0; desk_dirty();
		/* (as the page's: a file or a shortcut of the account's own dropped on the Trash goes there; a file dropped on an app's window opens in it
		   (shell25.js deskIconDrag, desk54.js deskDrag, a11y25.js __dropFileOnWindow); else the icon goes to the nearest free place) */
		int t = di_at(e->x, e->y);
		if (t >= 0 && t != DID.i && !strcmp(DI[t].kind, "trash") && (dd->fp[0] || dd->own)) { di_page(dd->n, dd->fp[0] ? "Move to Trash" : "Remove from desktop", 1); state_write(); return TRUE; }
		Client *cw = dd->fp[0] && WM_OK ? client_at((int)e->x_root, (int)e->y_root) : NULL;
		if (cw && cw->app && !cw->app->hidden && strcmp(cw->app->k, "explorer")) { drop_in(dd->fp, cw->app->k); state_write(); return TRUE; }
		/* (the place under the icon's top-left corner, moved as the pointer was) */
		int nc, nr, ix, iy; di_grid(&nc, &nr); di_xy(&DI[DID.i], &ix, &iy); int c = (int)lround((ix + e->x - DID.sx - desk_left() - 8) / (double)di_w()), r = (int)lround((iy + e->y - DID.sy - work_top() - 8) / (double)di_h());
		if (c < 0) c = 0; if (r < 0) r = 0; if (c >= nc) c = nc - 1; if (r >= nr) r = nr - 1;
		int fc, fr; di_free(c, r, DID.i, &fc, &fr); DI[DID.i].c = fc; DI[DID.i].r = fr; di_save(); desk_dirty(); state_write();
	}
	return TRUE;
}
static gboolean desk_key(GtkWidget *w, GdkEventKey *e, gpointer d)
{
	(void)w; (void)d; if (NF >= 0 && note_key(e)) return TRUE;   /* (typing in a note) */
	if (e->keyval == GDK_KEY_Return || e->keyval == GDK_KEY_KP_Enter) { for (int i = 0; i < NDI; i++) if (DI[i].sel) { di_open(i); return TRUE; } }
	if (e->keyval == GDK_KEY_F5) { apps_load(); return TRUE; }
	/* (1.6.1 Part 28) Ctrl+V: Paste on the desktop (the icons where the desktop puts new ones); Ctrl+C, Ctrl+X: a chosen file's Copy, Cut */
	int ctl = (e->state & GDK_CONTROL_MASK) && !(e->state & (GDK_MOD1_MASK | GDK_SHIFT_MASK)); guint kv = gdk_keyval_to_lower(e->keyval);
	if (ctl && kv == GDK_KEY_v && USER_ID[0] && !LOCKED) { if (FC_N > 0) di_page_at(NULL, "Paste", 1, "-"); return TRUE; }
	int i = di_sel(); if (i < 0) return FALSE; DIcon *di = &DI[i];
	if (ctl && (kv == GDK_KEY_c || kv == GDK_KEY_x) && di->fp[0]) { di_page(di->n, kv == GDK_KEY_x ? "Cut" : "Copy", 1); return TRUE; }
	if (e->keyval == GDK_KEY_F2 && di->fp[0]) { rename_ask(di->fp, di->n); return TRUE; }
	if (e->keyval == GDK_KEY_Delete) { if (di->fp[0]) di_page(di->n, "Move to Trash", 1); else if (strcmp(di->kind, "trash")) di_page(di->n, "Remove from desktop", 1); return TRUE; }
	return FALSE;
}

/* ------------------------------------------------------------------ the desktop's own sounds (1.6.0, the owner's report 2)
   On Standard the page plays them (base_v24.html sfx: oscillators, each tone's gain up in 6 ms to .35 x the volume and down to
   nothing over its length): the start-up chime at the sign-in screen, the sign-in sound, the error at a wrong password, the
   shut-down chime. On Native the shell draws those screens, and played none. The same tones here, through PulseAudio's simple
   API (libpulse-simple, which the system has: opened when first needed), with the account's choices: Settings › Volume
   Controls' master and each sound's switch, volume and style (S.snd, given by rudolfd's desktop/settings), the styles' tones as
   the page has them (sndp, given with the start menu's list: oneapp59.js oaCatalog) */
typedef struct { double f, t, d, fe; int ty; } Tone;   /* (ty: 0 sine, 1 square, 2 triangle, 3 sawtooth) */
/* (1.6.0, the owner's Rudolf sounds, 2026-11-19) nine more categories (desktop/sndrs60.js RS_NEW), and the Rudolf style in every
   one: a style whose tone carries {w:'rs', G, E}, the synth's settings and events, rendered here by rsynth.h as the page renders
   them (rsRender), sample for sample */
static const char *const SNDK[] = { "startup", "login", "error", "click", "open", "close", "notify", "game", "shutdown",
	"logout", "update", "warning", "battery", "usbin", "usbout", "menu", "trash", "shot" };
#define SNDN 18
#define SNDS 12   /* (styles a category can have) */
#define SNDE_MAX 48   /* (a Rudolf style's events) */
static const int SNDV0[SNDN] = { 70, 70, 70, 40, 60, 60, 70, 50, 70, 60, 70, 60, 70, 60, 60, 40, 60, 60 };
typedef struct { RsEv *e; int n; double g[5]; } SndRs;   /* (a Rudolf style: its events, NULL for tones) */
static Tone SNDP[SNDN][SNDS][16]; static int SNDPN[SNDN][SNDS], SNDPC[SNDN]; static SndRs SNDR[SNDN][SNDS];
static int SND_MON = 1, SND_MV = 70, SND_ON[SNDN] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	SND_V[SNDN] = { 70, 70, 70, 40, 60, 60, 70, 50, 70, 60, 70, 60, 70, 60, 60, 40, 60, 60 }, SND_P[SNDN];
static int snd_i(const char *k) { for (int i = 0; i < SNDN; i++) if (!strcmp(SNDK[i], k)) return i; return -1; }
/* one style's tones from the page's pattern (its [Hz, start, length, wave, slide to, {w:'rs', G, E}]); the Rudolf style's events into *r */
static int snd_tones(JV *pat, Tone *T, SndRs *r)
{
	int n = 0; free(r->e); memset(r, 0, sizeof *r);
	for (int j = 0; pat && pat->t == 4 && j < pat->len && n < 16; j++) { JV *x = pat->v[j]; if (!x || x->t != 4 || x->len < 3) continue; Tone *t = &T[n++]; memset(t, 0, sizeof *t);
		t->f = x->v[0]->t == 2 ? x->v[0]->n : 440; t->t = x->v[1]->t == 2 ? x->v[1]->n : 0; t->d = x->v[2]->t == 2 ? x->v[2]->n : .1;
		if (x->len > 3 && x->v[3]->t == 3) t->ty = !strcmp(x->v[3]->s, "square") ? 1 : !strcmp(x->v[3]->s, "triangle") ? 2 : !strcmp(x->v[3]->s, "sawtooth") ? 3 : 0;
		if (x->len > 4 && x->v[4]->t == 2) t->fe = x->v[4]->n;
		JV *m = x->len > 5 ? x->v[5] : NULL, *w = jprop(m, "w"), *G = jprop(m, "G"), *E = jprop(m, "E");
		if (!r->e && w && w->t == 3 && !strcmp(w->s, "rs") && G && G->t == 4 && G->len == 5 && E && E->t == 4 && E->len > 0) {
			for (int g = 0; g < 5; g++) r->g[g] = G->v[g] && G->v[g]->t == 2 ? fmax(0, fmin(100, G->v[g]->n)) : 0;
			r->e = calloc(SNDE_MAX, sizeof *r->e);
			for (int e = 0; r->e && e < E->len && r->n < SNDE_MAX; e++) { JV *ev = E->v[e]; if (!ev || ev->t != 4 || ev->len < 2 || !ev->v[0] || ev->v[0]->t != 3 || !strchr("BPKSNT", ev->v[0]->s[0]) || !ev->v[0]->s[0]) continue;
				RsEv *q = &r->e[r->n]; memset(q, 0, sizeof *q); q->k = ev->v[0]->s[0];
				for (int a = 1; a < ev->len && q->nv < RS_MAXV; a++) q->v[q->nv++] = ev->v[a] && ev->v[a]->t == 2 && isfinite(ev->v[a]->n) ? fmax(-20000, fmin(20000, ev->v[a]->n)) : 0;
				if (q->v[0] < 0 || q->v[0] > 6) continue;   /* (when it starts: within the sound) */
				r->n++; }
			if (!r->n) { free(r->e); r->e = NULL; } } }
	return n;
}
static void snd_cat(JV *v)
{
	JV *o = jprop(v, "sndp"); if (!o || o->t != 5) return;
	for (int i = 0; i < o->len; i++) { int k = snd_i(o->k[i]); JV *ps = o->v[i]; if (k < 0 || !ps || ps->t != 4) continue; SNDPC[k] = 0;
		for (int p = 0; p < ps->len && p < SNDS; p++) { SNDPN[k][p] = snd_tones(ps->v[p], SNDP[k][p], &SNDR[k][p]); SNDPC[k] = p + 1; } }
}
static void snd_set(JV *s)
{
	JV *o = jprop(s, "snd"); SND_MON = 1; SND_MV = 70; for (int i = 0; i < SNDN; i++) { SND_ON[i] = 1; SND_V[i] = SNDV0[i]; SND_P[i] = 0; }
	if (!o || o->t != 5) return;
	JV *m = jprop(o, "master"); if (m && m->t == 5) { JV *x = jprop(m, "on"); if (x) SND_MON = jbool(m, "on"); SND_MV = (int)jnum(m, "vol", 70); }
	for (int i = 0; i < SNDN; i++) { JV *e = jprop(o, SNDK[i]); if (!e || e->t != 5) continue; JV *x = jprop(e, "on"); if (x) SND_ON[i] = jbool(e, "on"); SND_V[i] = (int)jnum(e, "vol", SNDV0[i]); SND_P[i] = (int)jnum(e, "p", 0); }
}
typedef struct { int16_t *b; size_t n; } SndBuf;
static gpointer snd_thread(gpointer d)
{
	SndBuf *q = d; static void *lib; static int tried;
	typedef struct { int format; uint32_t rate; uint8_t channels; } PaSpec;   /* (pa_sample_spec: PA_SAMPLE_S16LE is 3) */
	typedef void *(*New)(const char *, const char *, int, const char *, const char *, const PaSpec *, const void *, const void *, int *);
	typedef int (*Wr)(void *, const void *, size_t, int *); typedef int (*Dr)(void *, int *); typedef void (*Fr)(void *);
	if (!tried) { tried = 1; lib = dlopen("libpulse-simple.so.0", RTLD_NOW | RTLD_LOCAL); if (!lib) fprintf(stderr, "rudolf-native: the desktop's sounds: no libpulse-simple (%s)\n", dlerror()); }
	New pn = lib ? (New)dlsym(lib, "pa_simple_new") : NULL; Wr pw = lib ? (Wr)dlsym(lib, "pa_simple_write") : NULL; Dr pd = lib ? (Dr)dlsym(lib, "pa_simple_drain") : NULL; Fr pf = lib ? (Fr)dlsym(lib, "pa_simple_free") : NULL;
	if (pn && pw && pd && pf) { PaSpec ss = { 3, 44100, 1 }; int er = 0; void *p = pn(NULL, "Rudolf OS", 1, NULL, "the desktop's sounds", &ss, NULL, NULL, &er);
		if (p) { pw(p, q->b, q->n * 2, &er); pd(p, &er); pf(p); } else fprintf(stderr, "rudolf-native: the desktop's sounds: PulseAudio refused (%d)\n", er); }
	g_free(q->b); g_free(q); return NULL;
}
static int SND_PLAYED[SNDN];   /* (for the tests: how many times each was played) */
static double snd_out(int i, double v, const Tone *T, int n, const SndRs *r);
/* the sound called k, as the page's sfx(k): its length in seconds (0: not played) */
static double snd_play(const char *k)
{
	int i = snd_i(k); if (i < 0 || !SND_MON || !SND_ON[i]) return 0;
	double v = SND_MV / 100.0 * SND_V[i] / 100.0; if (v <= 0) return 0;
	int p = SND_P[i] >= 0 && SND_P[i] < SNDPC[i] && SNDPN[i][SND_P[i]] ? SND_P[i] : 0; return snd_out(i, v, SNDP[i][p], SNDPN[i][p], &SNDR[i][p]);
}
/* (2026-11-16, the owner's Vivobook: no window or button sounds on Native) a sound a page of the shell's plays, as the page's sfx() would on
   Standard (oneapp59.js: its volume, from Volume Controls, and its tones, the account's style), played here: a page's own Web Audio did
   not sound (a window's page has had no click yet, WebKit keeps its audio suspended), and the windows open and close through the shell */
static void snd_page(const char *k, double v, const char *pj)
{
	int i = snd_i(k); if (i < 0 || !(v > 0) || v > 1) return; Tone T[16]; SndRs r = {0}; JV *o = pj && pj[0] == '{' ? jparse(pj) : NULL;
	int n = snd_tones(jprop(o, "t"), T, &r); jfree(o); snd_out(i, v, T, n, &r); free(r.e);
}
/* the shell's own buttons, as the page's (base_v24.html: a click on a button, a start menu row, a menu's row, a dialog's list: "click") */
static void snd_click(void) { snd_play("click"); }
static double snd_out(int i, double v, const Tone *T, int n, const SndRs *r)
{
	if (r && r->e && r->n) {   /* (a Rudolf style: the page's samples, at the category's volume) */
		size_t N = 0; float *f = rs_render(r->e, r->n, r->g, &N); if (!f || !N) { free(f); return 0; }
		SndBuf *q = g_new0(SndBuf, 1); q->n = N; q->b = g_new(int16_t, N); for (size_t s = 0; s < N; s++) q->b[s] = (int16_t)lround(fmax(-1, fmin(1, f[s] * v)) * 32767); free(f);
		SND_PLAYED[i]++; g_thread_unref(g_thread_new("sound", snd_thread, q)); return (double)N / RS_SR; }
	static const Tone DEF[SNDN][4] = { { {523, 0, .14, 0, 0}, {659, .14, .14, 0, 0}, {784, .28, .14, 0, 0}, {1047, .42, .35, 0, 0} }, { {660, 0, .1, 0, 0}, {880, .1, .22, 0, 0} },
		{ {220, 0, .16, 0, 1}, {165, .17, .24, 0, 1} }, { {1300, 0, .035, 0, 1} }, { {500, 0, .06, 0, 0}, {760, .06, .09, 0, 0} }, { {760, 0, .06, 0, 0}, {500, .06, .09, 0, 0} },
		{ {880, 0, .1, 0, 0}, {1175, .1, .16, 0, 0} }, { {330, 0, .06, 0, 2}, {440, .05, .06, 0, 2} }, { {784, 0, .16, 0, 0}, {659, .16, .16, 0, 0}, {523, .32, .16, 0, 0}, {392, .48, .4, 0, 0} },
		/* (the new categories' Classic: sndrs60.js RS_STY) */
		{ {880, 0, .1, 0, 0}, {660, .1, .22, 0, 0} }, { {660, 0, .1, 0, 0}, {880, .1, .1, 0, 0}, {1175, .2, .24, 0, 0} }, { {660, 0, .12, 0, 2}, {660, .16, .18, 0, 2} },
		{ {880, 0, .1, 0, 0}, {740, .14, .1, 0, 0}, {587, .28, .2, 0, 0} }, { {700, 0, .07, 0, 0}, {1050, .08, .12, 0, 0} }, { {1050, 0, .07, 0, 0}, {700, .08, .12, 0, 0} },
		{ {900, 0, .04, 0, 0}, {1200, .04, .06, 0, 0} }, { {400, 0, .18, 120, 3} }, { {1800, 0, .03, 0, 1}, {1200, .06, .04, 0, 1} } };
	static const int DEFN[SNDN] = { 4, 2, 2, 1, 2, 2, 2, 2, 4, 2, 3, 2, 3, 2, 2, 2, 1, 2 };   /* (the page's PAT, until a page has given its styles) */
	if (!n) { T = DEF[i]; n = DEFN[i]; }
	double len = 0; for (int j = 0; j < n; j++) len = fmax(len, T[j].t + T[j].d + .03);
	const int R = 44100; size_t N = (size_t)((len + .05) * R); float *a = g_new0(float, N);
	for (int j = 0; j < n; j++) { const Tone *t = &T[j]; double ph = 0, g = 0, a1 = .35 * v, t1 = t->t + .006, t2 = t->t + t->d;
		for (size_t s = (size_t)(t->t * R); s < N && s < (size_t)((t->t + t->d + .03) * R); s++) { double x = (double)s / R;
			g = x < t1 ? .0001 + (a1 - .0001) * (x - t->t) / .006 : x < t2 ? a1 * pow(.0001 / a1, (x - t1) / fmax(1e-6, t2 - t1)) : .0001;
			double f = t->fe > 0 && t->d > 0 ? t->f * pow(t->fe / t->f, fmin(1, (x - t->t) / t->d)) : t->f; ph += f / R; ph -= floor(ph);
			double w = t->ty == 1 ? (ph < .5 ? 1 : -1) : t->ty == 2 ? 1 - 4 * fabs(ph - .5) : t->ty == 3 ? 2 * ph - 1 : sin(2 * M_PI * ph);
			a[s] += (float)(w * g); } }
	SndBuf *q = g_new0(SndBuf, 1); q->n = N; q->b = g_new(int16_t, N); for (size_t s = 0; s < N; s++) q->b[s] = (int16_t)lround(fmax(-1, fmin(1, a[s])) * 32767); g_free(a);
	SND_PLAYED[i]++; g_thread_unref(g_thread_new("sound", snd_thread, q)); return len;
}
/* ------------------------------------------------------------------ the desktop's widgets (1.6.0 Part 12, item 2)
   The page's widgets (shell25.js WTYPES and wgDraw): the clock, the calendar, the sticky note, the system monitor and
   the weather, each a box on the desktop where the account left it (the page's bd95widgets:ID, given by rudolfd's
   desktop/settings; what the shell changes kept by rudolfd, desktop/widgets, until the page's own list changes),
   drawn as the page's CSS draws it (styles25.css .wgt: 220 px wide, the note 240, rgba(15,23,42,.72) over the desktop
   blurred 6 px, 12 px corners, the shadow 0 8px 22px rgba(0,0,0,.3); the header's picture and name at 11 px with its
   ✕, at 80 %; the contents in the page's font). The header drags it (any button, as the page's pointer events), the ✕
   removes it, the desktop's menu's Add a widget… shows the page's picker; the contents and timers are the page's: the
   clock (every 10 s there; here when the minute changes), the system monitor every 3 s (rudolfd's stats), the weather
   from Open-Meteo every 15 minutes (a failed look-up again in 20 s, 1, 2, then 5 minutes; a click opens Weather), the
   note typed in as in the page's textarea (its menu, keys, undo; saved half a second after the last change). Each is
   drawn into a picture of its own (with its shadow and what is behind it, blurred), laid over the desktop when shown. */
static int wg_kind_n(const char *t); static int wg_nwin(void);
typedef struct { char t[12]; double x, y; char *txt; cairo_surface_t *sf; int ox, oy, dirty; double th; } Wg;   /* (th: the note's textarea's height) */
#define MAXWG 32
static Wg WG[MAXWG]; static int NWG;
static const char *const WGT[][3] = { { "clock", "🕒", "Clock" }, { "cal", "📅", "Calendar" }, { "note", "🗒️", "Sticky note" }, { "mon", "📈", "System monitor" }, { "weather", "⛅", "Weather" } };
static int wg_kind(const char *t) { for (int i = 0; i < 5; i++) if (!strcmp(WGT[i][0], t)) return i; return -1; }
static int wg_shown(void) { return USER_ID[0] && !LOCKED && NWG > 0; }
static guint WG_SAVE_T, WG_MON_T, WG_WX_T, WG_WXR_T;
static struct { int i, on, moved; double sx, sy, ox, oy; } WGD = { -1, 0, 0, 0, 0, 0, 0 };   /* (a widget being dragged by its header) */
static int WG_X = -1;   /* (the ✕ pressed: its widget) */
static char MON_CPU[32] = "—", MON_MEM[64] = "—";
/* the weather: 0 loading, 1 asks where, 2 shown, 3 no connection */
static int WX_ST, WX_N, WX_GEN; static char WX_T[24], WX_C[64], WX_L[200];
typedef struct { int ok, guess; double lat, lon; char name[160], cc[8], tz[64]; } WLoc;
static WLoc WL, WGUESS; static char WUNIT[4];
/* the note being typed in (its widget; -1: none), the caret and the selection's other end (bytes), its scrolling */
static int NF = -1, NCAR, NANC, NBLINK, NSELW; static guint NBLINK_T; static double NSCR; static GtkIMContext *NIM;
static GPtrArray *NUNDO, *NREDO;   /* (the texts before each change, with the caret: "CARET\nTEXT") */
#define NOTEF "Courier New, monospace"
static cairo_t *SCR;
static cairo_t *scr(void) { if (!SCR) { cairo_surface_t *t = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 4, 4); SCR = cairo_create(t); cairo_surface_destroy(t); } return SCR; }
static double lh_of(const char *fam, int wt, double px) { double d, a = css_ascent(scr(), fam, wt, px, &d); return a + d; }
static double wg_w(const Wg *w) { return !strcmp(w->t, "note") ? 240 : 220; }
static double wg_top0(void) { return (work_top() + (!strcmp(S.tb, "top") ? 2 * Z : 0)) / Z; }   /* (the page's #desk: its top, in the page's units) */
/* the calendar's month in System Settings' time zone: its first day's weekday (Sunday 0), its days, today */
static void cal_month(int *y, int *mo, int *d, int *st, int *n)
{
	GTimeZone *tz = S.tz[0] ? g_time_zone_new_identifier(S.tz) : NULL; if (!tz) tz = g_time_zone_new_local();
	GDateTime *n0 = g_date_time_new_now(tz), *t = g_date_time_add_seconds(n0, S.doff / 1000.0);
	*y = g_date_time_get_year(t); *mo = g_date_time_get_month(t); *d = g_date_time_get_day_of_month(t); g_date_time_unref(t); g_date_time_unref(n0); g_time_zone_unref(tz);
	GDate *f = g_date_new_dmy(1, *mo, *y); *st = g_date_get_weekday(f) % 7; *n = g_date_get_days_in_month(*mo, *y); g_date_free(f);
}
static const char WXERR[] = "Weather needs an internet connection. Trying again…";
static int wrap_lines(const char *t, const char *fam, int wt, double px, double w)
{
	PangoLayout *pl = css_layout(scr(), t, fam, wt, px); pango_layout_set_width(pl, (int)(w * PANGO_SCALE)); pango_layout_set_wrap(pl, PANGO_WRAP_WORD_CHAR);
	int n = pango_layout_get_line_count(pl); g_object_unref(pl); return n;
}
static double wg_body(const Wg *w)
{
	double l13 = lh_of(UIFAM, 400, 13), bw = wg_w(w) - 24;
	if (!strcmp(w->t, "clock")) return lh_of(UIFAM, 300, 34) + l13;
	if (!strcmp(w->t, "cal")) { int y, mo, d, st, n; cal_month(&y, &mo, &d, &st, &n); return lh_of(UIFAM, 700, 13) + ((st + n + 6) / 7 + 1) * (8 + lh_of(UIFAM, 400, 12)); }
	if (!strcmp(w->t, "note")) { double d; css_ascent(scr(), UIFAM, 400, 13, &d); return w->th + d; }   /* (the textarea on the line: its bottom on the baseline) */
	if (!strcmp(w->t, "mon")) return 3 * l13;
	if (WX_ST == 2) return lh_of(UIFAM, 300, 34) + 2 * l13;
	if (WX_ST == 1) return lh_of(UIFAM, 400, 15) + l13;
	if (WX_ST == 3) return wrap_lines(WXERR, UIFAM, 400, 13, bw) * l13;
	return l13;
}
static double wg_h(const Wg *w) { return 6 + lh_of(UIFAM, 400, 11) + 6 + 4 + wg_body(w) + 12; }
static void wg_rect(const Wg *w, double *x, double *y, double *ww, double *hh) { *x = desk_left() / Z + w->x; *y = wg_top0() + w->y; *ww = wg_w(w); *hh = wg_h(w); }
/* (a picture blurred: three box blurs a channel, as a8_blur) */
static void rgb_blur(cairo_surface_t *m, double sigma)
{
	if (sigma <= 0) return; cairo_surface_flush(m);
	int w = cairo_image_surface_get_width(m), h = cairo_image_surface_get_height(m), st = cairo_image_surface_get_stride(m); unsigned char *d = cairo_image_surface_get_data(m);
	int r = (int)floor(sigma * 3 * sqrt(2 * M_PI) / 4 + .5); if (r < 1) return; unsigned char *t = g_malloc(MAX(w, h) * 4);
	for (int pass = 0; pass < 3; pass++) for (int ch = 0; ch < 3; ch++) {
		for (int y = 0; y < h; y++) { unsigned char *row = d + y * st; int sum = 0; for (int x = -r; x <= r; x++) sum += row[CLAMP(x, 0, w - 1) * 4 + ch];
			for (int x = 0; x < w; x++) { t[x] = sum / (2 * r + 1); sum += row[MIN(x + r + 1, w - 1) * 4 + ch] - row[MAX(x - r, 0) * 4 + ch]; } for (int x = 0; x < w; x++) row[x * 4 + ch] = t[x]; }
		for (int x = 0; x < w; x++) { int sum = 0; for (int y = -r; y <= r; y++) sum += d[CLAMP(y, 0, h - 1) * st + x * 4 + ch];
			for (int y = 0; y < h; y++) { t[y] = sum / (2 * r + 1); sum += d[MIN(y + r + 1, h - 1) * st + x * 4 + ch] - d[MAX(y - r, 0) * st + x * 4 + ch]; } for (int y = 0; y < h; y++) d[y * st + x * 4 + ch] = t[y]; }
	}
	g_free(t); cairo_surface_mark_dirty(m);
}
/* ---- the note's text, as the textarea lays it out (14 px "Courier New", padding 6 px, wrapped at its width) */
static double note_lh(void) { return lh_of(NOTEF, 400, 14); }
static PangoLayout *note_layout(const Wg *w)
{
	PangoLayout *pl = css_layout(scr(), w->txt ? w->txt : "", NOTEF, 400, 14); pango_layout_set_width(pl, (int)((wg_w(w) - 24 - 12) * PANGO_SCALE));
	pango_layout_set_wrap(pl, PANGO_WRAP_WORD_CHAR); return pl;
}
/* the caret's line and place in it (the page's units, from the text's top-left) */
static void note_caret_xy(const Wg *w, int idx, double *x, double *y)
{
	PangoLayout *pl = note_layout(w); int line = 0, xp = 0; pango_layout_index_to_line_x(pl, idx, FALSE, &line, &xp); g_object_unref(pl);
	*x = xp / (double)PANGO_SCALE; *y = line * note_lh();
}
static void note_draw(cairo_t *cr, const Wg *w, int i, double tx, double ty, double tw, double th)
{
	Box b; memset(&b, 0, sizeof b); b.bg = cx("#fef9c3", 1); b.rad = 6; box_paint(cr, tx, ty, tw, th, &b);
	double bx = tx, by = ty, bw = tw, bh = th; snap(&bx, &by, &bw, &bh);
	cairo_save(cr); rrect(cr, bx, by, bw, bh, 6); cairo_clip(cr);
	double lh = note_lh(), a, dsc; a = css_ascent(cr, NOTEF, 400, 14, &dsc); double top = ty + 6 - (NF == i ? NSCR : 0);
	if (!w->txt || !w->txt[0]) css_text(cr, "Type a note…", NOTEF, 400, 14, tx + 6, top, lh, cx("#757575", 1), 0);   /* (the placeholder: Chromium's grey) */
	else {
		PangoLayout *pl = css_layout(cr, w->txt, NOTEF, 400, 14); pango_layout_set_width(pl, (int)((tw - 12) * PANGO_SCALE)); pango_layout_set_wrap(pl, PANGO_WRAP_WORD_CHAR);
		int nl = pango_layout_get_line_count(pl), s0 = MIN(NCAR, NANC), s1 = MAX(NCAR, NANC);
		for (int l = 0; l < nl; l++) { PangoLayoutLine *L = pango_layout_get_line_readonly(pl, l); double ly = top + l * lh; if (ly > ty + th || ly + lh < ty) continue;
			if (NF == i && s1 > s0) { int *rg = NULL, nr = 0; pango_layout_line_get_x_ranges(L, s0, s1, &rg, &nr);   /* (the selection, Chromium's colour) */
				for (int k = 0; k < nr; k++) { cairo_rectangle(cr, tx + 6 + rg[2 * k] / (double)PANGO_SCALE, ly, (rg[2 * k + 1] - rg[2 * k]) / (double)PANGO_SCALE, lh); srca(cr, cx(NSELW ? "#3390ff" : "#c8c8c8", 1)); cairo_fill(cr); } g_free(rg); }
			srca(cr, cx("#422006", 1)); cairo_move_to(cr, tx + 6, round(ly + a)); pango_cairo_show_layout_line(cr, L); }
		g_object_unref(pl);
	}
	if (NF == i && NBLINK && NCAR == NANC) { double cx0, cy0; note_caret_xy(w, NCAR, &cx0, &cy0); cairo_rectangle(cr, round(tx + 6 + cx0), top + cy0, 1, lh); srca(cr, cx("#422006", 1)); cairo_fill(cr); }
	cairo_restore(cr);
	/* the resizer, as Chromium draws it: two lines, dark with a light edge, in the bottom-right corner */
	double R = bx + bw, B = by + bh;
	for (int k = 0; k < 2; k++) { int a0 = k ? 5 : 9; for (int p = 0; p < 2; p++) { srca(cr, p ? cx("#ffffff", .6) : cx("#000000", .6));
			for (int s = 3; s <= a0; s++) cairo_rectangle(cr, R - s + p, B - (a0 + 3 - s), 1, 1); cairo_fill(cr); } }
}
static void wg_render(int i)
{
	Wg *w = &WG[i]; double X, Y, W, H; wg_rect(w, &X, &Y, &W, &H); int k = wg_kind(w->t); if (k < 0) return;
	int ox = (int)floor((X - 26) * Z), oy = (int)floor((Y - 18) * Z), sw = (int)ceil((X + W + 26) * Z) - ox, sh = (int)ceil((Y + H + 38) * Z) - oy;
	if (w->sf) cairo_surface_destroy(w->sf);
	w->sf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, sw, sh); w->ox = ox; w->oy = oy; w->dirty = 0;
	cairo_t *cr = cairo_create(w->sf); cairo_translate(cr, -ox, -oy); cairo_scale(cr, Z, Z);
	Box b; memset(&b, 0, sizeof b); b.rad = 12; shd_add(&b, 0, 8, 22, cx("#000000", .3), 0); box_paint(cr, X, Y, W, H, &b);
	double bx = X, by = Y, bw = W, bh = H; snap(&bx, &by, &bw, &bh);
	if (DESKC) {   /* (backdrop-filter: blur(6px): the desktop behind it, as drawn) */
		int m = (int)ceil(20 * Z), rx = (int)floor(bx * Z) - m, ry = (int)floor(by * Z) - m, rw = (int)ceil((bx + bw) * Z) + m - rx, rh = (int)ceil((by + bh) * Z) + m - ry;
		cairo_surface_t *bk = cairo_image_surface_create(CAIRO_FORMAT_RGB24, rw, rh); cairo_t *bc = cairo_create(bk);
		cairo_set_source_surface(bc, DESKC, -rx, -ry); cairo_pattern_set_extend(cairo_get_source(bc), CAIRO_EXTEND_PAD); cairo_paint(bc); cairo_destroy(bc); rgb_blur(bk, 6 * Z);
		cairo_save(cr); rrect(cr, bx, by, bw, bh, 12); cairo_clip(cr); cairo_identity_matrix(cr); cairo_set_source_surface(cr, bk, rx - ox, ry - oy); cairo_paint(cr); cairo_restore(cr);
		cairo_surface_destroy(bk);
	}
	memset(&b, 0, sizeof b); b.bg = cx("#0f172a", .72); b.rad = 12; box_paint(cr, X, Y, W, H, &b);
	RGBA ink = cx("#e2e8f0", 1); double l11 = lh_of(UIFAM, 400, 11), l13 = lh_of(UIFAM, 400, 13);
	/* the header (.wgh: opacity .8) */
	cairo_push_group(cr); char hd[64]; snprintf(hd, sizeof hd, "%s %s", WGT[k][1], WGT[k][2]);
	css_text(cr, hd, UIFAM, 400, 11, X + 8, Y + 6, l11, ink, 0);
	css_text(cr, "✕", UIFAM, 400, 11, X + W - 8 - 4 - css_width(cr, "✕", UIFAM, 400, 11), Y + 6, l11, ink, 0);
	cairo_pop_group_to_source(cr); cairo_paint_with_alpha(cr, .8);
	double cx0 = X + 12, cy0 = Y + 6 + l11 + 6 + 4, cw = W - 24;
	if (k == 0) { char t[32], d[64]; sign_clock_texts(t, sizeof t, d, sizeof d); double l34 = lh_of(UIFAM, 300, 34);
		css_text(cr, t, UIFAM, 300, 34, cx0, cy0, l34, ink, 0); css_text(cr, d, UIFAM, 400, 13, cx0, cy0 + l34, l13, ink, 0); }
	else if (k == 1) { int y, mo, d, st, n; cal_month(&y, &mo, &d, &st, &n);
		GDateTime *f = g_date_time_new_utc(y, mo, 1, 12, 0, 0); char *mt = g_date_time_format(f, "%B %Y"); g_date_time_unref(f);
		double lb = lh_of(UIFAM, 700, 13), l12 = lh_of(UIFAM, 400, 12), rh = 8 + l12, col = cw / 7, ty = cy0 + lb; css_text(cr, mt ? mt : "", UIFAM, 700, 13, cx0, cy0, lb, ink, 0); g_free(mt);
		for (int c = 0; c < 7; c++) { char s[2] = { "SMTWTFS"[c], 0 }; css_text(cr, s, UIFAM, 700, 12, cx0 + c * col + (col - css_width(cr, s, UIFAM, 700, 12)) / 2, ty + 4, l12, ink, 0); }
		for (int q = 1; q <= n; q++) { int p = st + q - 1, r = p / 7 + 1, c = p % 7; char s[12]; snprintf(s, sizeof s, "%d", q); double x = cx0 + c * col, yy = ty + r * rh;
			if (q == d) { double ex = x, ey = yy, ew = col, eh = rh; snap(&ex, &ey, &ew, &eh); cairo_save(cr); cairo_translate(cr, ex + ew / 2, ey + eh / 2); cairo_scale(cr, ew / 2, eh / 2);
				cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI); cairo_restore(cr); srca(cr, ca(accent(), 1)); cairo_fill(cr); }   /* (td.tod: the accent, border-radius 50 %) */
			css_text(cr, s, UIFAM, 400, 12, x + (col - css_width(cr, s, UIFAM, 400, 12)) / 2, yy + 4, l12, q == d ? cx("#ffffff", 1) : ink, 0); } }
	else if (k == 2) note_draw(cr, w, i, cx0, cy0, cw, w->th);
	else if (k == 3) { const char *L[3] = { "CPU: ", "Memory: ", "Windows: " }; char nw[16]; snprintf(nw, sizeof nw, "%d", wg_nwin());
		const char *V[3] = { MON_CPU, MON_MEM, nw };
		for (int r = 0; r < 3; r++) { double ww = css_text(cr, L[r], UIFAM, 400, 13, cx0, cy0 + r * l13, l13, ink, 0); css_text(cr, V[r], UIFAM, 700, 13, cx0 + ww, cy0 + r * l13, l13, ink, 0); } }
	else if (WX_ST == 2) { double l34 = lh_of(UIFAM, 300, 34); css_text(cr, WX_T, UIFAM, 300, 34, cx0, cy0, l34, ink, 0); css_text(cr, WX_C, UIFAM, 400, 13, cx0, cy0 + l34, l13, ink, 0);
		cairo_push_group(cr); css_text(cr, WX_L, UIFAM, 400, 13, cx0, cy0 + l34 + l13, l13, ink, 0); cairo_pop_group_to_source(cr); cairo_paint_with_alpha(cr, .75); }
	else if (WX_ST == 1) { double l15 = lh_of(UIFAM, 400, 15); css_text(cr, "⛅ Where are you?", UIFAM, 400, 15, cx0, cy0, l15, ink, 0);
		cairo_push_group(cr); css_text(cr, "Click to choose your city.", UIFAM, 400, 13, cx0, cy0 + l15, l13, ink, 0); cairo_pop_group_to_source(cr); cairo_paint_with_alpha(cr, .8); }
	else if (WX_ST == 3) { PangoLayout *pl = css_layout(cr, WXERR, UIFAM, 400, 13); pango_layout_set_width(pl, (int)(cw * PANGO_SCALE)); pango_layout_set_wrap(pl, PANGO_WRAP_WORD_CHAR);
		double a, d; css_metrics(pl, &a, &d); srca(cr, ink);
		for (int l = 0; l < pango_layout_get_line_count(pl); l++) { cairo_move_to(cr, cx0, round(cy0 + l * l13 + a)); pango_cairo_show_layout_line(cr, pango_layout_get_line_readonly(pl, l)); }
		g_object_unref(pl); }
	else css_text(cr, "Loading…", UIFAM, 400, 13, cx0, cy0, l13, ink, 0);
	cairo_destroy(cr);
}
/* (what is drawn again: the widget's old and new places) */
static void wg_queue(int i)
{
	if (!DESKAREA || i < 0 || i >= NWG) return; Wg *w = &WG[i];
	if (w->sf) gtk_widget_queue_draw_area(DESKAREA, w->ox, w->oy, cairo_image_surface_get_width(w->sf), cairo_image_surface_get_height(w->sf));
	double X, Y, W, H; wg_rect(w, &X, &Y, &W, &H); gtk_widget_queue_draw_area(DESKAREA, (int)floor((X - 26) * Z), (int)floor((Y - 18) * Z), (int)ceil((W + 52) * Z) + 2, (int)ceil((H + 56) * Z) + 2);
}
static void wg_dirty(int i) { if (i < 0 || i >= NWG) return; WG[i].dirty = 1; wg_queue(i); }
static void wg_dirty_kind(const char *t) { for (int i = 0; i < NWG; i++) if (!strcmp(WG[i].t, t)) wg_dirty(i); }
static void wg_paint(cairo_t *cr)
{
	if (NWG_ALL_DIRTY) { NWG_ALL_DIRTY = 0; for (int i = 0; i < NWG; i++) WG[i].dirty = 1; }   /* (what is behind them changed) */
	if (!wg_shown()) return;
	for (int i = 0; i < NWG; i++) { Wg *w = &WG[i]; if (wg_kind(w->t) < 0) continue; if (!w->sf || w->dirty) wg_render(i);
		cairo_set_source_surface(cr, w->sf, w->ox, w->oy); cairo_paint(cr); }
}
/* the list as JSON (the page's: t, x, y, txt) */
static void num_js(GString *s, double v) { if (v == floor(v) && fabs(v) < 1e15) g_string_append_printf(s, "%.0f", v); else { char b[40]; g_ascii_formatd(b, sizeof b, "%.15g", v); g_string_append(s, b); } }
static GString *wg_json(void)
{
	GString *b = g_string_new("[");
	for (int i = 0; i < NWG; i++) { Wg *w = &WG[i]; if (i) g_string_append_c(b, ','); g_string_append(b, "{\"t\":"); json_esc(b, w->t);
		g_string_append(b, ",\"x\":"); num_js(b, w->x); g_string_append(b, ",\"y\":"); num_js(b, w->y); g_string_append(b, ",\"txt\":"); json_esc(b, w->txt ? w->txt : ""); g_string_append_c(b, '}'); }
	g_string_append_c(b, ']'); return b;
}
static gboolean wg_save_now(gpointer d)
{
	(void)d; WG_SAVE_T = 0; if (!USER_ID[0]) return G_SOURCE_REMOVE;
	GString *b = wg_json(); char *a = g_uri_escape_string(USER_ID, NULL, FALSE), p[200]; snprintf(p, sizeof p, "/api/desktop/widgets?acct=%s", a); g_free(a);
	http("POST", p, b->str, b->len, NULL); g_string_free(b, TRUE); state_write(); return G_SOURCE_REMOVE;
}
static void wg_save(void) { if (WG_SAVE_T) { g_source_remove(WG_SAVE_T); WG_SAVE_T = 0; } wg_save_now(NULL); }
static void wg_save_soon(void) { if (WG_SAVE_T) g_source_remove(WG_SAVE_T); WG_SAVE_T = g_timeout_add(500, wg_save_now, NULL); }
/* ---- the system monitor (rudolfd's stats, as the page reads them) */
/* (the windows, as Standard's wins.length: each window the taskbar shows, Oracle's Chromium windows too, as Task Manager's list) */
static int tb_win(Client *c);
static int wg_nwin(void)
{
	int c = 0;
	if (WM_OK) { for (GList *l = CLIENTS; l; l = l->next) { Client *x = l->data; if (tb_win(x) && x->ws && (x->app || x->ak[0])) c++; } return c; }
	for (GList *l = WINS; l; l = l->next) if (!((AppWin *)l->data)->hidden) c++;
	return c;
}
static gboolean wg_mon(gpointer d)
{
	(void)d; JV *v = jget("/api/stats"); snprintf(MON_CPU, sizeof MON_CPU, "—"); snprintf(MON_MEM, sizeof MON_MEM, "—");
	JV *c = jprop(v, "cpu"), *t = jprop(v, "memTotal"), *a = jprop(v, "memAvailable");
	if (c && c->t == 2 && t && t->t == 2 && a && a->t == 2) { snprintf(MON_CPU, sizeof MON_CPU, "%.0f%%", c->n);
		snprintf(MON_MEM, sizeof MON_MEM, "%.0f / %.0f MB", floor((t->n - a->n) / 1048576 + .5), floor(t->n / 1048576 + .5)); }
	jfree(v); wg_dirty_kind("mon"); return G_SOURCE_CONTINUE;
}
/* ---- the weather (weather47.js weatherLoc, wxF, wxT; shell25.js weatherNow): Open-Meteo's forecast for the place
   chosen in Weather (bd95wloc), else for the city of System Settings' time zone (looked up with Open-Meteo's geocoding;
   kept here), else it asks */
static void wx_draw(void) { wg_dirty_kind("weather"); state_write(); }
static gboolean wx_retry(gpointer d);
static void wx_fail(int gen)
{
	if (gen != WX_GEN) return; WX_ST = 3; int n = ++WX_N; static const int AG[3] = { 20, 60, 120 };
	if (WG_WXR_T) g_source_remove(WG_WXR_T); WG_WXR_T = g_timeout_add_seconds(n <= 3 ? AG[n - 1] : 300, wx_retry, NULL); wx_draw();
}
typedef struct { void (*cb)(JV *, int); int gen; SoupMessage *m; } WxReq;
static void wx_got(GObject *src, GAsyncResult *res, gpointer d)
{
	WxReq *q = d; GBytes *b = soup_session_send_and_read_finish(SOUP_SESSION(src), res, NULL); JV *j = NULL;
	if (b && soup_message_get_status(q->m) == 200) { gsize n = 0; const char *p = g_bytes_get_data(b, &n); char *s = g_strndup(p ? p : "", n); j = jparse(s); g_free(s); }
	if (b) g_bytes_unref(b);
	q->cb(j, q->gen); jfree(j); g_object_unref(q->m); g_free(q);
}
static SoupSession *WXS;
static void wx_get(const char *url, void (*cb)(JV *, int), int gen)
{
	if (!WXS) { WXS = soup_session_new(); soup_session_set_timeout(WXS, 30); }
	SoupMessage *m = soup_message_new("GET", url); if (!m) { cb(NULL, gen); return; }
	WxReq *q = g_new0(WxReq, 1); q->cb = cb; q->gen = gen; q->m = m; soup_session_send_and_read_async(WXS, m, G_PRIORITY_DEFAULT, NULL, wx_got, q);
}
static const char *wx_base(void) { const char *b = getenv("RUDOLF_WX_BASE"); return b && *b ? b : "https://api.open-meteo.com"; }
static const char *wx_geo(void) { const char *b = getenv("RUDOLF_WX_GEO"); return b && *b ? b : "https://geocoding-api.open-meteo.com"; }
static WLoc WXLOC;   /* (the place being looked up) */
static int wx_f(void) { if (WUNIT[0]) return WUNIT[0] == 'F'; return WXLOC.ok && (!strcmp(WXLOC.cc, "US") || !strcmp(WXLOC.cc, "LR") || !strcmp(WXLOC.cc, "MM")); }
static void wx_forecast_got(JV *j, int gen)
{
	if (gen != WX_GEN) return; JV *c = jprop(j, "current"), *t = jprop(c, "temperature_2m"), *k = jprop(c, "weather_code"), *wd = jprop(c, "wind_speed_10m");
	if (!t || t->t != 2 || !wd || wd->t != 2) { wx_fail(gen); return; }   /* (the page: an answer without them is an error too) */
	int f = wx_f(); double v = f ? t->n * 9 / 5 + 32 : t->n; snprintf(WX_T, sizeof WX_T, "%.0f°%s", floor(v + .5), f ? "F" : "C");
	static const struct { int c; const char *s; } CODE[] = { {0, "☀️ Clear"}, {1, "🌤️ Mostly clear"}, {2, "⛅ Partly cloudy"}, {3, "☁️ Cloudy"}, {45, "🌫️ Fog"}, {48, "🌫️ Fog"}, {51, "🌦️ Drizzle"},
		{53, "🌦️ Drizzle"}, {55, "🌦️ Drizzle"}, {61, "🌧️ Rain"}, {63, "🌧️ Rain"}, {65, "🌧️ Heavy rain"}, {71, "🌨️ Snow"}, {73, "🌨️ Snow"}, {75, "🌨️ Heavy snow"}, {80, "🌦️ Showers"},
		{81, "🌦️ Showers"}, {82, "🌧️ Heavy showers"}, {95, "⛈️ Thunderstorm"} };
	snprintf(WX_C, sizeof WX_C, "🌡️"); for (unsigned i = 0; k && k->t == 2 && i < G_N_ELEMENTS(CODE); i++) if (CODE[i].c == k->n) snprintf(WX_C, sizeof WX_C, "%s", CODE[i].s);
	char city[160]; snprintf(city, sizeof city, "%s", WXLOC.name); city[strcspn(city, ",")] = 0;
	snprintf(WX_L, sizeof WX_L, "%s · wind %.0f %s", city, floor(wd->n + .5), f ? "mph" : "km/h");
	WX_ST = 2; WX_N = 0; wx_draw();
}
static void wx_forecast(int gen)
{
	char la[40], lo[40], u[600]; g_ascii_formatd(la, sizeof la, "%.15g", WXLOC.lat); g_ascii_formatd(lo, sizeof lo, "%.15g", WXLOC.lon);
	snprintf(u, sizeof u, "%s/v1/forecast?latitude=%s&longitude=%s&current=temperature_2m,weather_code,wind_speed_10m&timezone=auto&wind_speed_unit=%s", wx_base(), la, lo, wx_f() ? "mph" : "kmh");
	wx_get(u, wx_forecast_got, gen);
}
static void wloc_read(JV *o, WLoc *l)
{
	memset(l, 0, sizeof *l); if (!o || o->t != 5) return; JV *a = jprop(o, "lat"), *b = jprop(o, "lon"); if (!a || a->t != 2 || !b || b->t != 2) return;
	l->ok = 1; l->lat = a->n; l->lon = b->n; jstr(o, "name", l->name, sizeof l->name); jstr(o, "cc", l->cc, sizeof l->cc); jstr(o, "tz", l->tz, sizeof l->tz); l->guess = jbool(o, "guess");
}
static void wx_tz(char *tz, size_t n)
{
	if (S.tz[0]) { snprintf(tz, n, "%s", S.tz); return; }
	GTimeZone *z = g_time_zone_new_local(); snprintf(tz, n, "%s", g_time_zone_get_identifier(z)); g_time_zone_unref(z);
	if (tz[0] == '/' || !strcmp(tz, "localtime")) snprintf(tz, n, "UTC");
}
static void wx_geo_got(JV *j, int gen)
{
	if (gen != WX_GEN) return; if (!j) { wx_fail(gen); return; }
	char tz[64]; wx_tz(tz, sizeof tz); char reg[64]; snprintf(reg, sizeof reg, "%s", tz); reg[strcspn(reg, "/")] = 0;
	JV *r = jprop(j, "results"), *pick = NULL;
	for (int pass = 0; pass < 3 && !pick; pass++) for (int i = 0; r && i < r->len; i++) { JV *g = jat(r, i); char z[64] = ""; jstr(g, "timezone", z, sizeof z);
		if (pass == 2 || (pass == 0 && !strcmp(z, tz)) || (pass == 1 && !strncmp(z, reg, strlen(reg)) && (z[strlen(reg)] == '/' || !z[strlen(reg)]))) { pick = g; break; } }
	if (!pick) { WX_ST = 1; wx_draw(); return; }
	WLoc l; memset(&l, 0, sizeof l); l.ok = 1; l.guess = 1; l.lat = jnum(pick, "latitude", 0); l.lon = jnum(pick, "longitude", 0); snprintf(l.tz, sizeof l.tz, "%s", tz); jstr(pick, "country_code", l.cc, sizeof l.cc);
	char nm[80] = "", a1[80] = "", co[80] = ""; jstr(pick, "name", nm, sizeof nm); jstr(pick, "admin1", a1, sizeof a1); jstr(pick, "country", co, sizeof co);
	snprintf(l.name, sizeof l.name, "%s%s%s%s%s", nm, a1[0] && strcmp(a1, nm) ? ", " : "", a1[0] && strcmp(a1, nm) ? a1 : "", co[0] ? ", " : "", co);
	WGUESS = l; WXLOC = l; wx_forecast(gen);
}
static void wx_go(void)
{
	if (WG_WXR_T) { g_source_remove(WG_WXR_T); WG_WXR_T = 0; }
	int gen = ++WX_GEN;
	if (WL.ok && !WL.guess) { WXLOC = WL; wx_forecast(gen); return; }
	char tz[64]; wx_tz(tz, sizeof tz); char city[64] = "";
	if (strchr(tz, '/') && g_ascii_strncasecmp(tz, "Etc", 3) && g_ascii_strncasecmp(tz, "UTC", 3) && g_ascii_strncasecmp(tz, "GMT", 3) && g_ascii_strncasecmp(tz, "SystemV", 7)) {
		snprintf(city, sizeof city, "%s", strrchr(tz, '/') + 1); for (char *p = city; *p; p++) if (*p == '_') *p = ' '; }
	if (!city[0]) { WX_ST = 1; wx_draw(); return; }
	if (WL.ok && WL.guess && !strcmp(WL.tz, tz)) { WXLOC = WL; wx_forecast(gen); return; }
	if (WGUESS.ok && !strcmp(WGUESS.tz, tz)) { WXLOC = WGUESS; wx_forecast(gen); return; }
	char *e = g_uri_escape_string(city, NULL, FALSE), u[400]; snprintf(u, sizeof u, "%s/v1/search?language=en&count=10&name=%s", wx_geo(), e); g_free(e);
	wx_get(u, wx_geo_got, gen);
}
static gboolean wx_retry(gpointer d) { (void)d; WG_WXR_T = 0; if (wg_kind_n("weather")) wx_go(); return G_SOURCE_REMOVE; }
static gboolean wx_tick(gpointer d) { (void)d; wx_go(); return G_SOURCE_CONTINUE; }
/* the page's wgDraw: everything drawn again, the timers started again (the weather says Loading… and looks again) */
static void note_unfocus(void);
static void wg_rebuild(void)
{
	note_unfocus();
	if (WG_MON_T) { g_source_remove(WG_MON_T); WG_MON_T = 0; } if (WG_WX_T) { g_source_remove(WG_WX_T); WG_WX_T = 0; } if (WG_WXR_T) { g_source_remove(WG_WXR_T); WG_WXR_T = 0; }
	for (int i = 0; i < NWG; i++) { WG[i].dirty = 1; WG[i].th = 120; }
	if (USER_ID[0] && wg_kind_n("mon")) { wg_mon(NULL); WG_MON_T = g_timeout_add_seconds(3, wg_mon, NULL); }
	if (USER_ID[0] && wg_kind_n("weather")) { WX_ST = 0; WX_N = 0; wx_go(); WG_WX_T = g_timeout_add_seconds(15 * 60, wx_tick, NULL); }
	if (DESKAREA) gtk_widget_queue_draw(DESKAREA); state_write();
}
static void wg_clear(void) { for (int i = 0; i < NWG; i++) { g_free(WG[i].txt); if (WG[i].sf) cairo_surface_destroy(WG[i].sf); } memset(WG, 0, sizeof WG); NWG = 0; }
/* from desktop/settings: the list (unless one is being moved or typed in), the weather's place and unit */
static void wg_load(JV *v)
{
	WLoc nl; wloc_read(jprop(v, "wloc"), &nl); char nu[4] = ""; jstr(v, "wunit", nu, sizeof nu);
	int wx = memcmp(&nl, &WL, sizeof nl) || strcmp(nu, WUNIT); WL = nl; snprintf(WUNIT, sizeof WUNIT, "%s", nu);
	JV *a = jprop(v, "widgets"); if (WGD.on || WG_SAVE_T) return;
	Wg n[MAXWG]; int nn = 0; memset(n, 0, sizeof n);
	for (int i = 0; a && a->t == 4 && i < a->len && nn < MAXWG; i++) { JV *o = jat(a, i); if (!o || o->t != 5) continue; Wg *w = &n[nn++];
		jstr(o, "t", w->t, sizeof w->t); w->x = jnum(o, "x", 0); w->y = jnum(o, "y", 0); JV *t = jprop(o, "txt"); w->txt = g_strdup(t && t->t == 3 ? t->s : ""); w->th = 120; }
	/* (the same list: nothing to do) */
	int same = nn == NWG; for (int i = 0; same && i < nn; i++) same = !strcmp(n[i].t, WG[i].t) && n[i].x == WG[i].x && n[i].y == WG[i].y && !strcmp(n[i].txt, WG[i].txt ? WG[i].txt : "");
	/* (1.6.0, the owner's report 4: "CPU: –, Memory: –, Windows: 0" and the weather "Loading…" for good) a list taken while no one was
	   signed in here yet (rudolfd's session already there: this program started again with a session going on, or the settings
	   read every 3 s in the moment between rudolfd's sign-in and its answer) had no timers; the same list again never started them */
	int stale = same && USER_ID[0] && ((wg_kind_n("mon") && !WG_MON_T) || (wg_kind_n("weather") && !WG_WX_T));
	if (same && !stale) { for (int i = 0; i < nn; i++) g_free(n[i].txt); if (wx && wg_kind_n("weather")) { WX_ST = 0; wx_go(); } return; }
	if (stale) { for (int i = 0; i < nn; i++) g_free(n[i].txt); wg_rebuild(); return; }
	wg_clear(); memcpy(WG, n, sizeof n); NWG = nn; wg_rebuild();
}
/* ---- the picker (the page's widgetPicker: the menu "Add a widget", where the page puts it) */
static void on_wg_add(const char *a)
{
	if (strncmp(a, "wg:", 3) || wg_kind(a + 3) < 0 || NWG >= MAXWG || !USER_ID[0]) return;
	Wg *w = &WG[NWG]; memset(w, 0, sizeof *w); snprintf(w->t, sizeof w->t, "%s", a + 3);
	w->x = fmax(20, round(SW / Z) - 300 - NWG * 20); w->y = 40 + NWG * 30; w->txt = g_strdup(""); NWG++;
	wg_save(); wg_rebuild();
}
static void wg_picker(void)
{
	CMI m[5]; memset(m, 0, sizeof m);
	for (int i = 0; i < 5; i++) { snprintf(m[i].label, sizeof m[i].label, "%s", WGT[i][2]); snprintf(m[i].icon, sizeof m[i].icon, "%s", WGT[i][1]); snprintf(m[i].act, sizeof m[i].act, "wg:%s", WGT[i][0]); }
	CM_RING = 1; cm_open((SW / Z / 2 - 90) * Z, (SH / Z / 3) * Z, "Add a widget", m, 5, on_wg_add); snprintf(DI_MENU, sizeof DI_MENU, "wgpick"); state_write();
}
/* ---- the note (the page's textarea) */
static gboolean note_blink(gpointer d) { (void)d; NBLINK = !NBLINK; if (NF >= 0) wg_dirty(NF); return G_SOURCE_CONTINUE; }
static void note_caret(void)
{
	NBLINK = 1; if (NBLINK_T) g_source_remove(NBLINK_T); NBLINK_T = g_timeout_add(500, note_blink, NULL);
	if (NF < 0) return; Wg *w = &WG[NF]; double x, y, lh = note_lh(), vis = w->th - 12; note_caret_xy(w, NCAR, &x, &y);   /* (kept in sight, as the textarea scrolls) */
	if (y < NSCR) NSCR = y; if (y + lh > NSCR + vis) NSCR = y + lh - vis; if (NSCR < 0) NSCR = 0; wg_dirty(NF);
}
static void note_unfocus(void) { if (NF >= 0 && NF < NWG) { int i = NF; NF = -1; wg_dirty(i); } NF = -1; NSCR = 0; if (NBLINK_T) { g_source_remove(NBLINK_T); NBLINK_T = 0; } if (NIM) gtk_im_context_focus_out(NIM); state_write(); }
static void note_focus(int i)
{
	if (NF != i) { note_unfocus(); NF = i; NSCR = 0; NSELW = 1; if (NUNDO) g_ptr_array_set_size(NUNDO, 0); if (NREDO) g_ptr_array_set_size(NREDO, 0); }
	if (!NUNDO) { NUNDO = g_ptr_array_new_with_free_func(g_free); NREDO = g_ptr_array_new_with_free_func(g_free); }
	if (WM_OK) wm_focus(NULL); if (DESK && gtk_widget_get_window(DESK)) gdk_window_focus(gtk_widget_get_window(DESK), GDK_CURRENT_TIME); gtk_widget_grab_focus(DESKAREA);
	if (NIM) gtk_im_context_focus_in(NIM); note_caret(); state_write();
}
static int note_len(void) { return NF >= 0 && WG[NF].txt ? (int)strlen(WG[NF].txt) : 0; }
static void note_snap(GPtrArray *a) { g_ptr_array_add(a, g_strdup_printf("%d\n%s", NCAR, WG[NF].txt ? WG[NF].txt : "")); if (a->len > 100) g_ptr_array_remove_index(a, 0); }
static void note_changed(void) { wg_save_soon(); note_caret(); state_write(); }
/* the selection replaced by S (typing, pasting, deleting) */
static void note_put(const char *s)
{
	if (NF < 0) return; Wg *w = &WG[NF]; const char *t = w->txt ? w->txt : ""; int a = MIN(NCAR, NANC), b = MAX(NCAR, NANC);
	if (a == b && !*s) return; note_snap(NUNDO); g_ptr_array_set_size(NREDO, 0);
	GString *n = g_string_new_len(t, a); g_string_append(n, s); g_string_append(n, t + b); g_free(w->txt); w->txt = g_string_free(n, FALSE);
	NCAR = NANC = a + (int)strlen(s); note_changed();
}
static void note_undo(int redo)
{
	GPtrArray *from = redo ? NREDO : NUNDO, *to = redo ? NUNDO : NREDO; if (NF < 0 || !from->len) return;
	note_snap(to); char *s = g_ptr_array_steal_index(from, from->len - 1), *nl = strchr(s, '\n'); Wg *w = &WG[NF];
	/* (as Chromium: what comes back is selected, else the caret where the change was) */
	const char *o = w->txt ? w->txt : "", *t = nl ? nl + 1 : ""; int lo = (int)strlen(o), lt = (int)strlen(t), a = 0, b = 0;
	while (a < lo && a < lt && o[a] == t[a]) a++; while (b < lo - a && b < lt - a && o[lo - 1 - b] == t[lt - 1 - b]) b++;
	while (a > 0 && (t[a] & 0xc0) == 0x80) a--;
	g_free(w->txt); w->txt = g_strdup(t); NANC = a; NCAR = lt - b; if (NCAR < NANC) NCAR = NANC; g_free(s); note_changed();
}
static char *note_sel(void) { if (NF < 0) return NULL; int a = MIN(NCAR, NANC), b = MAX(NCAR, NANC); return b > a ? g_strndup(WG[NF].txt + a, b - a) : NULL; }
static void note_copy(int cut) { char *s = note_sel(); if (!s) return; gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), s, -1); g_free(s); if (cut) note_put(""); }
static void note_pasted(GtkClipboard *c, const char *t, gpointer d) { (void)c; int i = GPOINTER_TO_INT(d); if (t && NF == i) note_put(t); }
static void note_paste(void) { if (NF >= 0) gtk_clipboard_request_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), note_pasted, GINT_TO_POINTER(NF)); }
static void note_commit(GtkIMContext *im, const char *str, gpointer d) { (void)im; (void)d; if (NF >= 0) note_put(str); }
/* the place in the text at (X, Y) (the page's units, from the text's top-left) */
static int note_index(const Wg *w, double x, double y)
{
	PangoLayout *pl = note_layout(w); int nl = pango_layout_get_line_count(pl), l = (int)floor(y / note_lh()); if (l < 0) l = 0; if (l >= nl) l = nl - 1;
	int idx = 0, tr = 0; pango_layout_line_x_to_index(pango_layout_get_line_readonly(pl, l), (int)(x * PANGO_SCALE), &idx, &tr);
	const char *t = w->txt ? w->txt : ""; while (tr-- > 0 && t[idx]) idx = (int)(g_utf8_next_char(t + idx) - t);
	g_object_unref(pl); return idx;
}
static int note_word(const char *t, int i, int dir)   /* (Ctrl+arrows, a double-click: words) */
{
	int n = (int)strlen(t);
	if (dir < 0) { while (i > 0 && !g_unichar_isalnum(g_utf8_get_char(g_utf8_prev_char(t + i)))) i = (int)(g_utf8_prev_char(t + i) - t); while (i > 0 && g_unichar_isalnum(g_utf8_get_char(g_utf8_prev_char(t + i)))) i = (int)(g_utf8_prev_char(t + i) - t); }
	else { while (i < n && !g_unichar_isalnum(g_utf8_get_char(t + i))) i = (int)(g_utf8_next_char(t + i) - t); while (i < n && g_unichar_isalnum(g_utf8_get_char(t + i))) i = (int)(g_utf8_next_char(t + i) - t); }
	return i;
}
static gboolean note_key(GdkEventKey *e)
{
	if (NF < 0) return FALSE; Wg *w = &WG[NF]; const char *t = w->txt ? w->txt : ""; int n = (int)strlen(t), sh = e->state & GDK_SHIFT_MASK, ctl = e->state & GDK_CONTROL_MASK, kv = e->keyval;
	if (ctl && !(e->state & GDK_MOD1_MASK)) {
		if (kv == GDK_KEY_a || kv == GDK_KEY_A) { NANC = 0; NCAR = n; note_caret(); return TRUE; }
		if (kv == GDK_KEY_c || kv == GDK_KEY_C || kv == GDK_KEY_Insert) { note_copy(0); return TRUE; }
		if (kv == GDK_KEY_x || kv == GDK_KEY_X) { note_copy(1); return TRUE; }
		if (kv == GDK_KEY_v || kv == GDK_KEY_V) { note_paste(); return TRUE; }
		if (kv == GDK_KEY_z || kv == GDK_KEY_Z) { note_undo(sh); return TRUE; }
		if (kv == GDK_KEY_y || kv == GDK_KEY_Y) { note_undo(1); return TRUE; }
	}
	int mv = -1;
	if (kv == GDK_KEY_Left || kv == GDK_KEY_KP_Left) { if (!sh && NCAR != NANC) mv = MIN(NCAR, NANC); else mv = ctl ? note_word(t, NCAR, -1) : NCAR > 0 ? (int)(g_utf8_prev_char(t + NCAR) - t) : 0; }
	else if (kv == GDK_KEY_Right || kv == GDK_KEY_KP_Right) { if (!sh && NCAR != NANC) mv = MAX(NCAR, NANC); else mv = ctl ? note_word(t, NCAR, 1) : NCAR < n ? (int)(g_utf8_next_char(t + NCAR) - t) : n; }
	else if (kv == GDK_KEY_Up || kv == GDK_KEY_Down || kv == GDK_KEY_KP_Up || kv == GDK_KEY_KP_Down) { double x, y; note_caret_xy(w, NCAR, &x, &y); int up = kv == GDK_KEY_Up || kv == GDK_KEY_KP_Up;
		PangoLayout *pl = note_layout(w); int nl = pango_layout_get_line_count(pl), l = (int)lround(y / note_lh()) + (up ? -1 : 1); g_object_unref(pl);
		mv = l < 0 ? 0 : l >= nl ? n : note_index(w, x, l * note_lh() + 1); }
	else if (kv == GDK_KEY_Home || kv == GDK_KEY_End || kv == GDK_KEY_KP_Home || kv == GDK_KEY_KP_End) { int end = kv == GDK_KEY_End || kv == GDK_KEY_KP_End;
		if (ctl) mv = end ? n : 0; else { double x, y; note_caret_xy(w, NCAR, &x, &y); PangoLayout *pl = note_layout(w); int line = (int)lround(y / note_lh());
			PangoLayoutLine *L = pango_layout_get_line_readonly(pl, line); mv = L ? (end ? L->start_index + L->length : L->start_index) : NCAR;
			if (end && L && mv > L->start_index && t[mv - 1] == '\n') mv--; g_object_unref(pl); } }
	if (mv >= 0) { NCAR = mv; if (!sh) NANC = mv; note_caret(); state_write(); return TRUE; }
	if (kv == GDK_KEY_BackSpace) { if (NCAR == NANC && NCAR > 0) NANC = ctl ? note_word(t, NCAR, -1) : (int)(g_utf8_prev_char(t + NCAR) - t); note_put(""); return TRUE; }
	if (kv == GDK_KEY_Delete || kv == GDK_KEY_KP_Delete) { if (sh && !ctl) { note_copy(1); return TRUE; } if (NCAR == NANC && NCAR < n) NANC = ctl ? note_word(t, NCAR, 1) : (int)(g_utf8_next_char(t + NCAR) - t); note_put(""); return TRUE; }
	if (kv == GDK_KEY_Insert && sh) { note_paste(); return TRUE; }
	if (kv == GDK_KEY_Return || kv == GDK_KEY_KP_Enter) { note_put("\n"); return TRUE; }
	if (kv == GDK_KEY_Tab || kv == GDK_KEY_ISO_Left_Tab) { note_unfocus(); return TRUE; }   /* (the focus leaves the textarea) */
	if (kv == GDK_KEY_Escape) return TRUE;
	if (ctl || (e->state & GDK_MOD1_MASK)) return TRUE;
	if (NIM && gtk_im_context_filter_keypress(NIM, e)) return TRUE;
	return TRUE;
}
/* the textarea's menu (the page's textMenu) */
static void on_note_menu(const char *a)
{
	if (NF < 0) return; note_focus(NF);
	if (!strcmp(a, "undo")) note_undo(0); else if (!strcmp(a, "redo")) note_undo(1); else if (!strcmp(a, "cut")) note_copy(1); else if (!strcmp(a, "copy")) note_copy(0);
	else if (!strcmp(a, "paste")) note_paste(); else if (!strcmp(a, "delete")) note_put(""); else if (!strcmp(a, "all")) { NANC = 0; NCAR = note_len(); note_caret(); }
}
static void note_menu(GdkEventButton *e)
{
	int sel = NCAR != NANC;
	CMI m[9] = { CM("Undo", "", "Ctrl+Z", "undo"), CM("Redo", "", "Ctrl+Y", "redo"), CMS, CM("Cut", "", "Ctrl+X", "cut"), CM("Copy", "", "Ctrl+C", "copy"), CM("Paste", "", "Ctrl+V", "paste"),
		CM("Delete", "", "", "delete"), CMS, CM("Select all", "", "Ctrl+A", "all") };
	m[3].dis = m[4].dis = m[6].dis = !sel; CM_RING = 0; cm_open(e->x_root, e->y_root, "", m, 9, on_note_menu); snprintf(DI_MENU, sizeof DI_MENU, "text"); state_write();
}
/* ---- the pointer */
enum { WP_NONE, WP_HEAD, WP_X, WP_BODY, WP_TEXT, WP_RESIZE };
static int wg_at(double sx, double sy, int *part)
{
	*part = WP_NONE; if (!wg_shown()) return -1; double x = sx / Z, y = sy / Z;
	for (int i = NWG - 1; i >= 0; i--) { Wg *w = &WG[i]; if (wg_kind(w->t) < 0) continue; double X, Y, W, H; wg_rect(w, &X, &Y, &W, &H);
		if (x < X || x >= X + W || y < Y || y >= Y + H) continue; double hh = 6 + lh_of(UIFAM, 400, 11) + 6, xw = 4 + css_width(scr(), "✕", UIFAM, 400, 11) + 4;
		if (y < Y + hh) *part = x >= X + W - 8 - xw && x < X + W - 8 && y >= Y + 6 && y < Y + hh - 6 ? WP_X : WP_HEAD;
		else if (!strcmp(w->t, "note") && x >= X + 12 && x < X + W - 12 && y >= Y + hh + 4 && y < Y + hh + 4 + w->th) *part = x >= X + W - 12 - 14 && y >= Y + hh + 4 + w->th - 14 ? WP_RESIZE : WP_TEXT;
		else *part = WP_BODY;
		return i; }
	return -1;
}
static struct { int on, sel, rs; double sy, th0; } NDR;   /* (a selection being made with the pointer; the textarea being resized) */
static void note_point(GdkEventButton *e, int i, int ext)
{
	Wg *w = &WG[i]; double X, Y, W, H; wg_rect(w, &X, &Y, &W, &H); double hh = 6 + lh_of(UIFAM, 400, 11) + 6;
	int idx = note_index(w, e->x / Z - (X + 12 + 6), e->y / Z - (Y + hh + 4 + 6) + (NF == i ? NSCR : 0)); int was = NF == i; note_focus(i);
	NCAR = idx; if (!ext || !was) NANC = idx; note_caret();
}
static gboolean wg_press(GdkEventButton *e)
{
	int part, i = wg_at(e->x, e->y, &part); if (i < 0) { if (NF >= 0 && e->type == GDK_BUTTON_PRESS) note_unfocus(); return FALSE; }
	Wg *w = &WG[i];
	if (e->type == GDK_2BUTTON_PRESS && part == WP_TEXT && NF == i) { const char *t = w->txt ? w->txt : ""; NANC = note_word(t, NCAR, -1); NCAR = note_word(t, NANC, 1); note_caret(); return TRUE; }
	if (e->type != GDK_BUTTON_PRESS) return TRUE;
	if (part != WP_TEXT && part != WP_RESIZE && NF >= 0) note_unfocus();
	if (part == WP_HEAD) { WGD.i = i; WGD.on = 1; WGD.moved = 0; WGD.sx = e->x; WGD.sy = e->y; WGD.ox = w->x; WGD.oy = w->y; return TRUE; }   /* (any button, as the page's pointerdown) */
	if (part == WP_X) { if (e->button == 1) WG_X = i; return TRUE; }
	if (part == WP_TEXT) { if (e->button == 3) {   /* (a right-click keeps the selection it is in, else puts the caret there) */
			int in = 0; if (NF == i) { double X, Y, W, H; wg_rect(w, &X, &Y, &W, &H); int k = note_index(w, e->x / Z - (X + 12 + 6), e->y / Z - (Y + 6 + lh_of(UIFAM, 400, 11) + 6 + 4 + 6) + NSCR); in = k >= MIN(NCAR, NANC) && k <= MAX(NCAR, NANC); }
			if (!in) note_point(e, i, 0); note_menu(e); return TRUE; } if (e->button == 1) { note_point(e, i, (e->state & GDK_SHIFT_MASK) != 0); NDR.on = 1; } return TRUE; }
	if (part == WP_RESIZE && e->button == 1) { note_focus(i); NDR.rs = 1; NDR.sy = e->y; NDR.th0 = w->th; return TRUE; }
	if (part == WP_BODY && e->button == 1 && !strcmp(w->t, "weather")) { WG_X = -2 - i; return TRUE; }   /* (a click: press and release on it) */
	return TRUE;
}
static gboolean wg_motion(GdkEventMotion *e)
{
	if (WGD.on && WGD.i >= 0 && WGD.i < NWG) { Wg *w = &WG[WGD.i]; WGD.moved = 1; wg_queue(WGD.i);
		w->x = fmax(0, WGD.ox + round((e->x - WGD.sx) / Z)); w->y = fmax(0, WGD.oy + round((e->y - WGD.sy) / Z)); w->dirty = 1; wg_queue(WGD.i); return TRUE; }
	if (NDR.on && NF >= 0) { Wg *w = &WG[NF]; double X, Y, W, H; wg_rect(w, &X, &Y, &W, &H); double hh = 6 + lh_of(UIFAM, 400, 11) + 6;
		NCAR = note_index(w, e->x / Z - (X + 12 + 6), e->y / Z - (Y + hh + 4 + 6) + NSCR); note_caret(); return TRUE; }
	if (NDR.rs && NF >= 0) { wg_queue(NF); WG[NF].th = fmax(12, NDR.th0 + (e->y - NDR.sy) / Z); wg_dirty(NF); return TRUE; }
	/* the pointer's shape, as the page's CSS: move over a header, the text cursor over the note, a hand over the weather */
	int part, i = wg_at(e->x, e->y, &part); static int lastp = -1; int p = i < 0 ? 0 : part == WP_HEAD ? 1 : part == WP_TEXT ? 2 : part == WP_RESIZE ? 4 : part == WP_BODY && !strcmp(WG[i].t, "weather") ? 3 : 0;
	if (p != lastp && gtk_widget_get_window(DESKAREA)) { lastp = p; static const char *const N[] = { "default", "move", "text", "pointer", "se-resize" };
		GdkCursor *c = gdk_cursor_new_from_name(gdk_display_get_default(), N[p]); gdk_window_set_cursor(gtk_widget_get_window(DESKAREA), c); if (c) g_object_unref(c); }
	return i >= 0;
}
static gboolean wg_release(GdkEventButton *e)
{
	if (WGD.on) { WGD.on = 0; if (WGD.moved) wg_save(); WGD.i = -1; state_write(); return TRUE; }   /* (the page's pointerup: saved, moved or not) */
	if (NDR.on || NDR.rs) { NDR.on = NDR.rs = 0; return TRUE; }
	if (WG_X != -1) { int was = WG_X, part, i = wg_at(e->x, e->y, &part); WG_X = -1;
		if (was >= 0 && i == was && part == WP_X) { g_free(WG[i].txt); if (WG[i].sf) cairo_surface_destroy(WG[i].sf); memmove(&WG[i], &WG[i + 1], sizeof WG[0] * (NWG - i - 1)); NWG--; memset(&WG[NWG], 0, sizeof WG[0]); wg_save(); wg_rebuild(); }
		else if (was <= -2 && i == -2 - was && part == WP_BODY) app_open("weather", NULL);
		return TRUE; }
	return FALSE;
}
static gboolean on_desk_out(GtkWidget *w, GdkEvent *e, gpointer d) { (void)w; (void)e; (void)d; if (NF >= 0 && !(CMWIN && gtk_widget_get_visible(CMWIN))) note_unfocus(); return FALSE; }
static int wg_kind_n(const char *t) { int c = 0; for (int i = 0; i < NWG; i++) if (!strcmp(WG[i].t, t)) c++; return c; }

/* ------------------------------------------------------------------ the taskbar's menus (1.6.0 Part 12, item 3)
   The page's right-click menus on the taskbar, item by item (base_v24.html barMenu and the start button's; shell25.js
   the pinned apps' and winMenu; the tray's Sound, Network and Clock menus), with each app's own rows (private59.js
   APP_JUMPS, given in desktop/apps "jumps": Oracle's New private tab). A window's menu is read from its own page
   (oneapp59.js __oaWinMenu: its rows, Print / Save as PDF… and Force quit among them); what the shell does itself
   (Restore, Minimize, Maximize, Always on top, Center on screen, Close, Force quit, the taskbar's rows) it does, the
   rest the page does (a window's own rows in its page, __oaWinMenuDo; pinning, muting, Settings at a page and the jumps
   in a page of their own). */
typedef struct { char k[KN], label[96], icon[24], key[32]; } Jump;
static Jump JUMPS[16]; static int NJUMPS;
static Window TBM_W; static char TBM_K[KN]; static double TBM_X, TBM_Y;   /* (the window or app whose menu it is; where) */
static void jumps_load(JV *v)
{
	NJUMPS = 0; JV *j = jprop(v, "jumps");
	for (int i = 0; j && j->t == 5 && i < j->len; i++) for (int n = 0; n < 8 && NJUMPS < 16; n++) { JV *r = jat(j->v[i], n); if (!r) break; Jump *x = &JUMPS[NJUMPS++];
		snprintf(x->k, sizeof x->k, "%s", j->k[i]); JV *a = jat(r, 0), *b = jat(r, 1), *c = jat(r, 2);
		snprintf(x->label, sizeof x->label, "%s", a && a->t == 3 ? a->s : ""); snprintf(x->icon, sizeof x->icon, "%s", b && b->t == 3 ? b->s : ""); snprintf(x->key, sizeof x->key, "%s", c && c->t == 3 ? c->s : ""); }
}
/* (1.6.0 Part 12, test finding N3) Task Manager's Apps: on Standard the page's windows are all in its one page (wins); here each app's
   window is a page of its own, so Task Manager's page asks the shell (op "wins") and is given every window the taskbar would list in any
   workspace, oldest first (the app, title, icon, minimised, workspace, active), each with its page's own weight as Task Manager
   estimates it (oneapp59.js __oaMe, asked of every page at once; a page that does not answer in 400 ms has none). "Switch to" and
   "End task" come back as op "winop" (focus: brought to the front, its workspace shown; end: the window closed as Standard's End task
   closes it, without asking about unsaved work: the page's own close, oaDone, so its files are written first). */
typedef struct { AppWin *to; int n, left, sent; char **me; Client **cl; guint t; } WinsReq;
static void json_esc(GString *s, const char *v);
static void wins_free(WinsReq *q) { for (int i = 0; i < q->n; i++) g_free(q->me[i]); g_free(q->me); g_free(q->cl); g_free(q); }
static void wins_send(WinsReq *q)
{
	if (q->sent) return; q->sent = 1; if (q->t) { g_source_remove(q->t); q->t = 0; }
	if (g_list_find(WINS, q->to) && q->to->view) {
		GString *s = g_string_new("[");
		for (int i = 0, k = 0; i < q->n; i++) { Client *c = q->cl[i]; if (!g_list_find(CLIENTS, c)) continue; AppWin *a = c->app;
			char id[24]; snprintf(id, sizeof id, "%lx", (unsigned long)c->frame);
			g_string_append_printf(s, "%s{\"id\":\"%s\",\"w\":%d,\"app\":", k++ ? "," : "", id, a ? a->id : 0); json_esc(s, a ? a->k : c->ak);
			g_string_append(s, ",\"title\":"); json_esc(s, c->title); g_string_append(s, ",\"icon\":"); json_esc(s, a ? a->icon : "");
			g_string_append_printf(s, ",\"min\":%d,\"ws\":%d,\"act\":%d,\"rest\":%d,\"me\":%s}", c->min, c->ws, c == FOCUS, a ? a->rest : 0, q->me[i] && q->me[i][0] == '{' && !(a && a->rest) ? q->me[i] : "null"); }
		g_string_append(s, "]"); GString *js = g_string_new("window.__oaWinsIn&&__oaWinsIn("); json_esc(js, s->str); g_string_append(js, ")");
		webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(q->to->view), js->str, -1, NULL, NULL, NULL, NULL, NULL); g_string_free(js, TRUE); g_string_free(s, TRUE); }
	if (q->left <= 0) wins_free(q);   /* (else the last answer frees it) */
}
typedef struct { WinsReq *q; int i; } WinsAns;
static void wins_got(GObject *src, GAsyncResult *res, gpointer d)
{
	WinsAns *w = d; WinsReq *q = w->q; JSCValue *v = webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(src), res, NULL);
	if (v && jsc_value_is_string(v) && !q->sent) q->me[w->i] = jsc_value_to_string(v);
	if (v) g_object_unref(v); g_free(w); q->left--;
	if (q->left <= 0) { if (!q->sent) wins_send(q); else wins_free(q); }
}
static gboolean wins_late(gpointer d) { WinsReq *q = d; q->t = 0; wins_send(q); return G_SOURCE_REMOVE; }
static int tb_win(Client *c);
static void wins_ask(AppWin *to)
{
	WinsReq *q = g_new0(WinsReq, 1); q->to = to; int n = g_list_length(CLIENTS); q->me = g_new0(char *, n + 1); q->cl = g_new0(Client *, n + 1);
	for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (!tb_win(c) || !c->ws || (!c->app && !c->ak[0])) continue; q->cl[q->n++] = c; }
	q->left = 1;   /* (until every page is asked) */
	for (int i = 0; i < q->n; i++) { AppWin *a = q->cl[i]->app; if (!a || !a->view || a->rest) continue; WinsAns *w = g_new0(WinsAns, 1); w->q = q; w->i = i; q->left++;
		webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(a->view), "window.__oaMe?JSON.stringify(__oaMe()):''", -1, NULL, NULL, NULL, wins_got, w); }
	q->left--; if (q->left <= 0) wins_send(q); else q->t = g_timeout_add(400, wins_late, q);
}
static void win_op(const char *id, const char *what)
{
	Window f = (Window)strtoul(id, NULL, 16); Client *c = NULL; for (GList *l = CLIENTS; l; l = l->next) if (((Client *)l->data)->frame == f) c = l->data;
	if (!c || !f) return;
	if (!strcmp(what, "focus")) { wm_focus(c); return; }
	if (!strcmp(what, "end")) { if (c->app && c->app->view) webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(c->app->view), "window.__oaEnd?__oaEnd():window.close()", -1, NULL, NULL, NULL, NULL, NULL); else wm_close(c); }
}
static int tb_win(Client *c) { return c && !c->hidden && !c->trans && (!c->app || (!c->app->hidden && c->app->modal != 2 && strcmp(c->app->k, "do-saver"))); }   /* (not the screen saver: item 6) */   /* (the page's wins: the taskbar's windows) */
static int tb_count(int min) { int n = 0; for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (tb_win(c) && (min < 0 || c->min == min)) n++; } return n; }
static int app_running(const char *k) { for (GList *l = WINS; l; l = l->next) { AppWin *a = l->data; if (!a->hidden && !strcmp(a->k, k)) return 1; }
	for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (c->ak[0] && !strcmp(c->ak, k)) return 1; } return 0; }   /* (and Oracle's Chromium's windows: 12.3c) */
static void tb_page(const char *k, const char *extra, int hidden) { app_open_ex(k, NULL, extra, hidden); }
static void tb_settings(const char *page) { char *e = qesc(page), ex[96]; snprintf(ex, sizeof ex, "&page=%s", e); g_free(e); tb_page("settings", ex, 0); }
static void tb_jump(const char *k, const char *label) { char *e = qesc(label), ex[300]; snprintf(ex, sizeof ex, "&jump=%s", e); g_free(e); tb_page(k, ex, 0); }
static void on_tb_act(const char *a)
{
	if (!strcmp(a, "showdesk")) { for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (tb_win(c) && !c->min) wm_minimise(c); } }
	else if (!strcmp(a, "restoreall")) { GList *L = g_list_copy(CLIENTS); for (GList *l = L; l; l = l->next) { Client *c = l->data; if (g_list_find(CLIENTS, c) && tb_win(c) && c->min) wm_focus(c); } g_list_free(L); }
	else if (!strcmp(a, "cascade")) { int i = 0; GList *L = g_list_copy(CLIENTS); double dy = !strcmp(S.tb, "top") ? 2 : 0;   /* (the page's: 20 + 28 px a window, from the desktop's corner) */
		for (GList *l = L; l; l = l->next) { Client *c = l->data; if (!g_list_find(CLIENTS, c) || !tb_win(c) || c->min) continue; if (c->max) wm_maximise(c, 0);
			c->x = desk_left() + (int)lround((20 + i * 28) * Z); c->y = work_top() + (int)lround((dy + 20 + i * 28) * Z); place(c); wm_focus(c); i++; }
		g_list_free(L); }
	else if (!strcmp(a, "closeall")) { GList *L = g_list_copy(CLIENTS); for (GList *l = L; l; l = l->next) { Client *c = l->data; if (g_list_find(CLIENTS, c) && tb_win(c)) { if (c->app) appwin_ask_close(c->app); else wm_close(c); } } g_list_free(L); }
	else if (!strcmp(a, "tbset")) tb_settings("disp");
	else if (!strncmp(a, "set:", 4)) tb_settings(a + 4);
	else if (!strncmp(a, "app:", 4)) app_open(a + 4, NULL);
	else if (!strcmp(a, "run")) app_open("do-run", NULL);
	else if (!strcmp(a, "off")) power_dialog();
	else if (!strcmp(a, "mute")) tb_page("do-mute", NULL, 1);
	else if (!strncmp(a, "pnew:", 5)) app_open(a + 5, NULL);
	else if (!strncmp(a, "pclose:", 7)) { GList *L = g_list_copy(WINS); for (GList *l = L; l; l = l->next) { AppWin *w = l->data; if (g_list_find(WINS, w) && !w->hidden && !strcmp(w->k, a + 7)) appwin_ask_close(w); } g_list_free(L); }
	if (!strncmp(a, "pclose:", 7)) { GList *L = g_list_copy(CLIENTS); for (GList *l = L; l; l = l->next) { Client *c = l->data; if (g_list_find(CLIENTS, c) && c->ak[0] && !strcmp(c->ak, a + 7)) wm_close(c); } g_list_free(L); }
	else if (!strncmp(a, "pin0:", 5) || !strncmp(a, "pin1:", 5)) { char *e = qesc(a + 5), ex[120]; snprintf(ex, sizeof ex, "&k=%s&on=%c", e, a[3]); g_free(e); tb_page("do-pin", ex, 1); }
	else if (!strncmp(a, "jump:", 5)) { char k[KN]; const char *bar = strchr(a + 5, '|'); if (!bar) return; snprintf(k, sizeof k, "%.*s", (int)(bar - a - 5), a + 5); tb_jump(k, bar + 1); }
	else if (!strncmp(a, "wn:", 3) || !strncmp(a, "wp:", 3)) {
		Client *c = client_of(TBM_W); if (!c) return;
		if (!strcmp(a, "wn:restore")) { if (c->min) wm_focus(c); else if (c->max) wm_maximise(c, 0); }
		else if (!strcmp(a, "wn:min")) wm_minimise(c);
		else if (!strncmp(a, "wn:ws:", 6)) ws_move(c, atoi(a + 6));   /* (wsMove: the window goes; the workspace shown stays) */
		else if (!strcmp(a, "wn:max")) { wm_focus(c); wm_maximise(c, 1); }
		else if (!strcmp(a, "wn:top")) { c->ontop = !c->ontop; wm_focus(c); }
		else if (!strcmp(a, "wn:center")) { if (c->max) wm_maximise(c, 0); wm_focus(c); c->x = desk_left() + MAX(0, (desk_w() - fw_of(c)) / 2); c->y = work_top() + MAX(0, (work_h() - fh_of(c)) / 2); place(c); state_write(); }
		else if (!strcmp(a, "wn:close")) { if (c->app) appwin_ask_close(c->app); else wm_close(c); }
		else if (!strcmp(a, "wn:tbown") || !strcmp(a, "wn:tbrudolf")) tb_choose(c, a[5] == 'r');   /* (1.6.1 Part 15, item 3) */
		else if (!strcmp(a, "wn:fq")) { if (c->app) { char k[KN]; snprintf(k, sizeof k, "%s", c->app->k); GList *L = g_list_copy(WINS);   /* (the page's: every window of the app, without asking) */
				for (GList *l = L; l; l = l->next) { AppWin *w = l->data; if (g_list_find(WINS, w) && !w->hidden && !strcmp(w->k, k)) appwin_close_now(w); } g_list_free(L); } else XKillClient(DPY, c->w); }
		else if (!strncmp(a, "wp:", 3) && c->app && c->app->view) { GString *js = g_string_new("window.__oaWinMenuDo&&__oaWinMenuDo("); json_esc(js, a + 3); g_string_append_c(js, ')');
			webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(c->app->view), js->str, -1, NULL, NULL, NULL, NULL, NULL); g_string_free(js, TRUE); }
	}
	state_write();
}
static void tb_menu_show(const char *title, CMI *m, int n, int ring, const char *what)
{
	CM_RING = ring; cm_open(TBM_X, TBM_Y, title, m, n, on_tb_act); snprintf(DI_MENU, sizeof DI_MENU, "%s", what); state_write();
}
/* a window's menu, from its page */
static int jump_of(const char *k, const char *label) { for (int i = 0; i < NJUMPS; i++) if (!strcmp(JUMPS[i].k, k) && !strcmp(JUMPS[i].label, label)) return i; return -1; }
/* (1.6.0 Part 12, test finding N21) an app's own right-click menus (the page's shared cmenu: File Explorer's, a text box's, the apps'):
   on Standard the menu is the page's, which is the screen, so it opens at the pointer and may go past the window's edge, kept on the
   screen; in an app's own window it was kept inside the window and cut off (Explorer's last rows could not be reached). The page gives
   its rows (oneapp59.js: label, icon or tick, key, greyed, separators, the title) and where (the window's own pixels); the shell shows
   them as it shows the taskbar's menus (cm_open, as the page's cmenu: the same rows, look and placing), and the row chosen is done by
   the page (__oaCtxDo, by its index) */
static AppWin *CTX_A; static int CTX_GEN;
static void ctx_chosen(const char *act)
{
	if (strncmp(act, "ctx:", 4) || !CTX_A || !g_list_find(WINS, CTX_A) || !CTX_A->view) return;
	char js[96]; snprintf(js, sizeof js, "window.__oaCtxDo&&__oaCtxDo(%d,%d)", atoi(act + 4), CTX_GEN);
	webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(CTX_A->view), js, -1, NULL, NULL, NULL, NULL, NULL);
}
static void ctx_msg(AppWin *a, JSCValue *v)
{
	Client *c = WM_OK ? client_of_app(a) : NULL; if (!c) return;
	char xs[24] = "", ys[24] = "", gs[16] = "", title[160] = ""; jsstr(v, "x", xs, sizeof xs); jsstr(v, "y", ys, sizeof ys); jsstr(v, "gen", gs, sizeof gs); jsstr(v, "title", title, sizeof title);
	JSCValue *iv = jsc_value_object_get_property(v, "items"); char *is = iv && jsc_value_is_string(iv) ? jsc_value_to_string(iv) : NULL; if (iv) g_object_unref(iv);
	char *ws = is ? g_strdup_printf("{\"l\":%s}", is) : NULL; JV *top = ws ? jparse(ws) : NULL, *it = jprop(top, "l"); g_free(ws); g_free(is); if (!it || it->t != 4) { jfree(top); return; }
	CMI m[24]; int n = 0, k = 0; memset(m, 0, sizeof m);
	for (int i = 0; i < it->len && n < 24; i++) { JV *o = jat(it, i); CMI *x = &m[n++]; if (jbool(o, "sep")) { x->sep = 1; continue; }
		jstr(o, "label", x->label, sizeof x->label); jstr(o, "icon", x->icon, sizeof x->icon); jstr(o, "key", x->key, sizeof x->key); x->dis = jbool(o, "dis"); snprintf(x->act, sizeof x->act, "ctx:%d", k++); }
	jfree(top);
	int cx = mon_ox(c) + (c->full ? 0 : c->x + BW), cy = mon_oy(c) + (c->full ? 0 : c->y + TH);
	char rg[4] = ""; jsstr(v, "ring", rg, sizeof rg);   /* (the menu's focus ring as Chromium shows it after that click: the page says) */
	CTX_A = a; CTX_GEN = atoi(gs); CM_RING = rg[0] == '1'; cm_open(cx + atof(xs), cy + atof(ys), title, m, n, ctx_chosen);
}
/* (win_menu_build) its rows, after the others: a window drawing its own title bar */
static int tb_menu_rows(Client *c, CMI *m, int n, int max)
{
	if (c->app || c->ak[0] || !c->nodeco || n + 3 > max) return n;
	memset(&m[n], 0, sizeof m[n]); m[n++].sep = 1;
	if (tbcut_of(c)) {
		CMI *a = &m[n++]; memset(a, 0, sizeof *a); snprintf(a->label, sizeof a->label, "Title bar: the app's own"); snprintf(a->act, sizeof a->act, "wn:tbown"); snprintf(a->icon, sizeof a->icon, "%s", c->tbcut ? "" : "✓");
		CMI *b = &m[n++]; memset(b, 0, sizeof *b); snprintf(b->label, sizeof b->label, "Title bar: Rudolf's"); snprintf(b->act, sizeof b->act, "wn:tbrudolf"); snprintf(b->icon, sizeof b->icon, "%s", c->tbcut ? "✓" : "");
	} else { CMI *a = &m[n++]; memset(a, 0, sizeof *a); a->dis = 1; snprintf(a->label, sizeof a->label, "Title bar: the app's own (Rudolf's would hide its menus and buttons)"); snprintf(a->act, sizeof a->act, "wn:tbno"); }
	return n;
}
static void win_menu_build(Client *c, JV *pm)
{
	CMI m[24]; int n = 0; memset(m, 0, sizeof m); char title[256] = ""; jstr(pm, "title", title, sizeof title); if (!title[0]) snprintf(title, sizeof title, "%s", c->title);
	JV *it = jprop(pm, "items");
	if (!it) {   /* (a window without a page of its own: the page's rows the shell can do) */
		static const char *const R[][3] = { { "Restore", "❐", "" }, { "Minimize", "_", "" }, { "Maximize", "□", "" }, { "-", "", "" }, { "Always on top", "", "" }, { "Center on screen", "", "" }, { "-", "", "" }, { "Move to workspace ", "", "" }, { "-", "", "" }, { "Close", "✕", "Alt+F4" } };
		for (unsigned i = 0; i < G_N_ELEMENTS(R); i++) { CMI *x = &m[n++]; if (R[i][0][0] == '-') { x->sep = 1; continue; } snprintf(x->label, sizeof x->label, "%s", R[i][0]); snprintf(x->icon, sizeof x->icon, "%s", R[i][1]); snprintf(x->key, sizeof x->key, "%s", R[i][2]); }
	} else for (int i = 0; i < it->len && n < 24; i++) { JV *o = jat(it, i); CMI *x = &m[n++]; if (jbool(o, "sep")) { x->sep = 1; continue; }
		jstr(o, "label", x->label, sizeof x->label); jstr(o, "icon", x->icon, sizeof x->icon); jstr(o, "key", x->key, sizeof x->key); x->dis = jbool(o, "dis"); }
	/* (the page's "Move to workspace N" rows are its own page's (always workspace 1's): the window's, here) */
	{ int at = -1, k = 0; CMI t[24]; for (int i = 0; i < n; i++) { if (!m[i].sep && !strncmp(m[i].label, "Move to workspace ", 18)) { if (at < 0) { at = k;
				for (int w = 1; w <= NWS && k < 24; w++) if (w != c->ws) { memset(&t[k], 0, sizeof t[k]); snprintf(t[k].label, sizeof t[k].label, "Move to workspace %d", w); snprintf(t[k].act, sizeof t[k].act, "wn:ws:%d", w); k++; } } continue; }
			if (k < 24) t[k++] = m[i]; }
	  memcpy(m, t, k * sizeof t[0]); n = k; }
	for (int i = 0; i < n; i++) { CMI *x = &m[i]; if (x->sep) continue; const char *L = x->label;
		if (!strncmp(x->act, "wn:ws:", 6)) continue;
		if (!strcmp(L, "Restore")) { snprintf(x->act, sizeof x->act, "wn:restore"); x->dis = !(c->min || c->max); }
		else if (!strcmp(L, "Minimize")) { snprintf(x->act, sizeof x->act, "wn:min"); x->dis = c->min; }
		else if (!strcmp(L, "Maximize")) { snprintf(x->act, sizeof x->act, "wn:max"); x->dis = c->max; }
		else if (!strcmp(L, "Always on top")) { snprintf(x->act, sizeof x->act, "wn:top"); snprintf(x->icon, sizeof x->icon, "%s", c->ontop ? "✓" : ""); }
		else if (!strcmp(L, "Center on screen")) snprintf(x->act, sizeof x->act, "wn:center");
		else if (!strcmp(L, "Close")) snprintf(x->act, sizeof x->act, "wn:close");
		else if (!strncmp(L, "Force quit ", 11)) snprintf(x->act, sizeof x->act, "wn:fq");
		else if (c->app && jump_of(c->app->k, L) >= 0) snprintf(x->act, sizeof x->act, "jump:%.38s|%.50s", c->app->k, L);
		else snprintf(x->act, sizeof x->act, "wp:%.90s", L); }
	for (int i = 0; i < n; i++) if (!strcmp(m[i].label, "Print / Save as PDF…")) CM_LATE = 1;
	n = tb_menu_rows(c, m, n, 24);   /* (1.6.1 Part 15, item 3) */
	tb_menu_show(title, m, n, 0, "win");
}
static void win_menu_got(GObject *src, GAsyncResult *res, gpointer d)
{
	Window w = (Window)GPOINTER_TO_SIZE(d); JSCValue *v = webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(src), res, NULL);
	char *s = v && jsc_value_is_string(v) ? jsc_value_to_string(v) : NULL; JV *pm = s && *s ? jparse(s) : NULL; g_free(s); if (v) g_object_unref(v);
	Client *c = client_of(w); if (c && w == TBM_W) win_menu_build(c, pm); jfree(pm);
}
/* a window's menu at the pointer: its title bar's right-click (as the taskbar button's, from the window's page when it has one) */
static void win_menu_at(Client *c, int x, int y)
{
	if (!USER_ID[0] || LOCKED) return; TBM_X = x; TBM_Y = y; TBM_W = c->w;
	if (c->app && c->app->view) webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(c->app->view), "window.__oaWinMenu?__oaWinMenu():''", -1, NULL, NULL, NULL, win_menu_got, GSIZE_TO_POINTER(c->w));
	else win_menu_build(c, NULL);
}
static void bar_menu(GdkEventButton *e, int h)
{
	TBM_X = e->x_root; TBM_Y = e->y_root; const char *k = h >= 0 ? HITS[h].kind : "", *arg = h >= 0 ? HITS[h].arg : "";
	if (!USER_ID[0] || LOCKED) return;
	if (!strcmp(k, "start")) { menu_hide(); CMI m[6] = { CM("File Explorer", "📁", "", "app:explorer"), CM("Command Prompt", "⬛", "", "app:prompt"), CM("Settings", "⚙️", "", "app:settings"), CM("Run…", "🏃", "", "run"), CMS,
			CM("Sign out / shut down…", "🔌", "", "off") }; tb_menu_show("Rudolf OS", m, 6, 0, "start"); return; }
	if (!strcmp(k, "pin")) { const App *ap = app_find(arg); CMI m[12]; int n = 0; memset(m, 0, sizeof m); snprintf(TBM_K, sizeof TBM_K, "%s", arg);
		m[n] = (CMI)CM("Open a new window", "", "", ""); snprintf(m[n++].act, sizeof m[0].act, "pnew:%s", arg);
		for (int j = 0; j < NJUMPS && n < 8; j++) if (!strcmp(JUMPS[j].k, arg)) { CMI *x = &m[n++]; snprintf(x->label, sizeof x->label, "%s", JUMPS[j].label); snprintf(x->icon, sizeof x->icon, "%s", JUMPS[j].icon);
			snprintf(x->key, sizeof x->key, "%s", JUMPS[j].key); snprintf(x->act, sizeof x->act, "jump:%.38s|%.50s", arg, JUMPS[j].label); }
		m[n] = (CMI)CM("Close all windows", "", "", ""); snprintf(m[n].act, sizeof m[0].act, "pclose:%s", arg); m[n++].dis = !app_running(arg);
		m[n++] = (CMI)CMS; m[n] = (CMI)CM("Unpin from taskbar", "", "", ""); snprintf(m[n++].act, sizeof m[0].act, "pin0:%s", arg);
		tb_menu_show(ap ? ap->name : arg, m, n, 0, "pin"); return; }
	if (!strcmp(k, "win")) { Client *c = WM_OK ? client_of((Window)strtoul(arg, NULL, 16)) : NULL; if (!c) return; TBM_W = c->w;
		if (c->app && c->app->view) webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(c->app->view), "window.__oaWinMenu?__oaWinMenu():''", -1, NULL, NULL, NULL, win_menu_got, GSIZE_TO_POINTER(c->w));
		else win_menu_build(c, NULL);
		return; }
	if (!strcmp(k, "vol")) { CMI m[2] = { CM("", "", "", "mute"), CM("Volume controls…", "", "", "set:vol") }; snprintf(m[0].label, sizeof m[0].label, "%s", MUTED ? "Unmute" : "Mute"); tb_menu_show("Sound", m, 2, 0, "vol"); return; }
	if (!strcmp(k, "net")) { CMI m[1] = { CM("Network settings…", "", "", "set:net") }; tb_menu_show("Network", m, 1, 0, "net"); return; }
	if (!strcmp(k, "clock")) { CMI m[2] = { CM("Adjust date and time…", "", "", "set:sys"), CM("Language and accessibility…", "", "", "set:acc") }; tb_menu_show("Clock", m, 2, 1, "clock"); return; }
	CMI m[7] = { CM("Show desktop", "", "", "showdesk"), CM("Restore all windows", "", "", "restoreall"), CM("Cascade windows", "", "", "cascade"), CMS, CM("Close all windows", "", "", "closeall"), CMS, CM("Taskbar settings", "", "", "tbset") };
	int nw = tb_count(-1); m[0].dis = m[2].dis = m[4].dis = !nw; m[1].dis = !tb_count(1); tb_menu_show("Taskbar", m, 7, 1, "bar");
}

/* ------------------------------------------------------------------ the tray's pop-ups (1.6.0 Part 12, item 3, second part)
   The calendar under the clock (shell25.js #calp, .flyout: the time, the long date, the month with ◀ and ▶, the weeks from
   Sunday as the page's table lays them out, today in the accent colour, a day with an event of the Calendar's in bold and
   underlined, the next four events, Open Calendar; a double-click on a day opens the Calendar at it) and the volume's
   (osd31.js sysVolPop, .vp: the system volume as Chromium draws a range input and a checkbox, Mute, Desktop sounds…,
   placed over its button as flyPlace places it). Drawn by the shell in a window of their own, as the page's menus
   (CMWIN); a click outside closes them (one on the taskbar goes on to the taskbar), a click on their button again too. */
static const char *const FLMON[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
/* (the page's clockParts: System Settings' time zone and offset) */
static GDateTime *fl_now(void)
{
	GTimeZone *tz = S.tz[0] ? g_time_zone_new_identifier(S.tz) : NULL; if (!tz) tz = g_time_zone_new_local();
	GDateTime *n0 = g_date_time_new_now(tz), *n = g_date_time_add_seconds(n0, S.doff / 1000.0); g_date_time_unref(n0); g_time_zone_unref(tz); return n;
}
typedef struct { char d[12], t[12], title[160]; } CalEv;
static CalEv CALEV[64]; static int NCALEV;
static GtkWidget *FLWIN, *FLAREA; static GdkPixbuf *FLBG; static int FLK, FLWX, FLWY, FLHOV = -1, FLPRESS = -1, FL_Y, FL_M, FLVOL = -1, FLMUTE, FLDRAG;
static double FLX, FLY, FLW, FLH;   /* the box (the page's units, on the screen) */
static DRect FLB[48]; static char FLBK[48][16]; static int NFLB;   /* (what can be clicked: kind and place) */
static void cal_events_load(JV *v)
{
	NCALEV = 0; JV *a = jprop(v, "cal");
	for (int i = 0; a && a->t == 4 && i < a->len && NCALEV < 64; i++) { JV *o = jat(a, i); if (!o || o->t != 5) continue; CalEv *e = &CALEV[NCALEV++]; memset(e, 0, sizeof *e);
		jstr(o, "d", e->d, sizeof e->d); jstr(o, "t", e->t, sizeof e->t); jstr(o, "title", e->title, sizeof e->title); }
}
static void fl_btn(const char *k, double x, double y, double w, double h) { if (NFLB < 48) { FLB[NFLB] = (DRect){ x, y, w, h }; snprintf(FLBK[NFLB], sizeof FLBK[0], "%s", k); NFLB++; } }
static int fl_at(double px, double py) { double x = (px + FLWX) / Z, y = (py + FLWY) / Z; for (int i = NFLB - 1; i >= 0; i--) if (x >= FLB[i].x && x < FLB[i].x + FLB[i].w && y >= FLB[i].y && y < FLB[i].y + FLB[i].h) return i; return -1; }
/* a page button (base_v24.html button: white, 1 px #d1d5db, 4 px corners, 3 px 10 px; hovered #eaf0ff and #a9bff5; the dark theme's flyout's #1e293b) */
static void fl_button(cairo_t *cr, const char *t, double x, double y, double w, double h, int hv, int dark)
{
	Box k; memset(&k, 0, sizeof k); k.rad = 4; k.bw = 1;
	if (dark) { k.bg = cx("#1e293b", 1); k.bc = cx("#334155", 1); } else { k.bg = cx(hv ? "#eaf0ff" : "#ffffff", 1); k.bc = cx(hv ? "#a9bff5" : "#d1d5db", 1); }
	box_paint(cr, x, y, w, h, &k); double tw = css_width(cr, t, UIFAM, 400, 13), a, d; a = css_ascent(cr, UIFAM, 400, 13, &d);
	css_text(cr, t, UIFAM, 400, 13, x + (w - tw) / 2, y + (h - (a + d)) / 2, a + d, cx(dark ? "#e2e8f0" : "#000000", 1), 0);
}
static double fl_lh(cairo_t *cr, int wt, double px) { double d, a = css_ascent(cr, UIFAM, wt, px, &d); return a + d; }
/* ---- quick settings (shell25.js #qsp: the page's eight switches as tiles, three to a row, the network's line, Lock,
   Settings and Power) and the notifications' centre (#ncp: Do not disturb, Clear all, each notification with its time,
   its ✕ and, for a failure, Blitzen's ⚡ Why?). A switch, Do not disturb, ✕ and Clear all are the page's own: done by a
   page unseen (#app=do-qs&item=WORDS, #app=do-nc&op=...), shown at once here and read back from rudolfd after. */
typedef struct { char t[160], b[300], k[12], wl[24]; double at; } Note;   /* (wl, 1.6.1: its Blitzen button's own words, "Ask Blitzen"; "": ⚡ Why? for a failure) */
static Note NT[60]; static int NNT;
/* (dismissed or cleared here: an answer of rudolfd's from before the page's own storage had them gone does not bring them back,
   for 15 s; told by a page (op "note") and not in rudolfd's answer yet: kept in the list 15 s) */
static struct { double at; char t[160]; gint64 when; } NTX[16]; static int NNTX; static double NT_CLR; static gint64 NT_CLRT;
static Note NTP[8]; static gint64 NTPT[8]; static int NNTP;
static double NT_SEEN; static char NT_WHO[80];   /* (the page's ncUnread: those come since signing in, or since the centre was opened) */
static int nt_unread(void) { int n = 0; for (int i = 0; i < NNT; i++) if (NT[i].at > NT_SEEN) n++; return n; }
static void fl_resize(void);
static void nt_load(JV *v)
{
	NNT = 0; JV *a = jprop(v, "notif"); gint64 now = g_get_monotonic_time();
	{ int k = 0; for (int i = 0; i < NNTX; i++) if (now - NTX[i].when < 15000000) NTX[k++] = NTX[i]; NNTX = k; }
	for (int i = 0; a && a->t == 4 && i < a->len && NNT < 60; i++) { JV *o = jat(a, i); if (!o || o->t != 5) continue; Note *n = &NT[NNT++]; memset(n, 0, sizeof *n);
		jstr(o, "t", n->t, sizeof n->t); jstr(o, "b", n->b, sizeof n->b); jstr(o, "k", n->k, sizeof n->k); jstr(o, "wl", n->wl, sizeof n->wl); n->at = jnum(o, "at", 0);
		int gone = NT_CLRT && now - NT_CLRT < 15000000 && n->at <= NT_CLR; for (int j = 0; j < NNTX && !gone; j++) gone = NTX[j].at == n->at && !strcmp(NTX[j].t, n->t);
		if (gone) NNT--; }
	{ int k = 0; for (int i = 0; i < NNTP; i++) { int there = 0; for (int j = 0; j < NNT && !there; j++) there = NT[j].at == NTP[i].at && !strcmp(NT[j].t, NTP[i].t);
		if (there || now - NTPT[i] > 15000000) continue; NTP[k] = NTP[i]; NTPT[k++] = NTPT[i];
		if (NNT == 60) NNT--; memmove(&NT[1], &NT[0], NNT * sizeof NT[0]); NT[0] = NTP[i]; NNT++; } NNTP = k; }
	if (strcmp(NT_WHO, USER_ID)) { snprintf(NT_WHO, sizeof NT_WHO, "%s", USER_ID); NT_SEEN = g_get_real_time() / 1000.0; }   /* (signed in: what is there is seen) */
	bar_redraw();
	if (FLK && FLAREA) gtk_widget_queue_draw(FLAREA);
}
/* (a page's notification, oneapp59.js op "note": in the centre and counted at once) */
static void nt_note(JSCValue *v) { nt_notef(fget_js, v); }
static void nt_notef(FGet g, void *v)
{
	if (NNT == 60) NNT--; memmove(&NT[1], &NT[0], NNT * sizeof NT[0]); NNT++; Note *n = &NT[0]; memset(n, 0, sizeof *n); char at[40] = "";
	g(v, "t", n->t, sizeof n->t); g(v, "b", n->b, sizeof n->b); g(v, "k", n->k, sizeof n->k); g(v, "wl", n->wl, sizeof n->wl); g(v, "at", at, sizeof at); n->at = g_ascii_strtod(at, NULL);
	if (NNTP == 8) { memmove(&NTP[0], &NTP[1], 7 * sizeof NTP[0]); memmove(&NTPT[0], &NTPT[1], 7 * sizeof NTPT[0]); NNTP--; } NTP[NNTP] = *n; NTPT[NNTP++] = g_get_monotonic_time();
	bar_redraw(); if (FLK == FL_NC && FLAREA) fl_resize();
}
/* (blitzdiag57.js bzIsError: a failure, which Blitzen can be asked about) */
static int nt_why(const Note *n)
{
	if (n->wl[0]) return 1;   /* (1.6.1: a notification with its own Blitzen button) */
	if (!strcmp(n->k, "error") || !strcmp(n->k, "Error") || !strcmp(n->k, "Warning")) return 1;
	char t[480]; snprintf(t, sizeof t, "%s %s", n->t, n->b);
	return g_regex_match_simple("could ?n[o']t|cannot|can't|failed|failure|error|not found|refused|denied|crash|stopped|timed out|timeout|no answer|did not|went wrong|not respond|white screen|restarted", t, G_REGEX_CASELESS, 0);
}
/* (1.6.0 Part 2: the ninth, Airplane mode, is mobile60.js's: rudolfd's mobile/airplane, read when quick settings open, set
   from here in a thread of its own: turning the modems off can take seconds) */
static const char *const QSI[10] = {"🌙", "🌅", "🔕", "🗣️", "🔍", "◐", "⌨️", "🖐️", "✈️", "📶"},
	*const QSL[10] = {"Dark mode", "Night light", "Do not disturb", "Screen reader", "Magnifier", "High contrast", "On-screen keyboard", "Touch mode", "Airplane mode", "Mobile data"};
static int AIR_ON;
/* (1.6.0 Part 3: the tenth, Mobile data, only with a modem whose SIM is unlocked: connected or not, mobile60.js's) */
static int QSN = 9, MOB_ID = -1, MOB_ON;
static void mob_read(void)
{
	JV *v = jget("/api/mobile/status"), *a = jprop(v, "modems"); MOB_ID = -1; MOB_ON = 0;
	for (int i = 0; a && a->t == 4 && i < a->len; i++) { JV *o = jat(a, i); char lk[16] = "", st[24] = "", fl[24] = ""; jstr(o, "locked", lk, sizeof lk); jstr(o, "state", st, sizeof st); jstr(o, "failed", fl, sizeof fl);
		if (!jbool(o, "sim") || (lk[0] && strcmp(lk, "none") && strcmp(lk, "--")) || !strcmp(st, "failed")) continue;
		MOB_ID = (int)jnum(o, "id", 0); MOB_ON = !strcmp(st, "connected"); break; }
	jfree(v); QSN = MOB_ID >= 0 ? 10 : 9;
}
static gboolean mob_done(gpointer p) { (void)p; mob_read(); if (FLK == FL_QS && FLAREA) gtk_widget_queue_draw(FLAREA); return G_SOURCE_REMOVE; }
static gpointer mob_thread(gpointer p) { char q[64]; snprintf(q, sizeof q, "/api/mobile/connect?id=%d&on=%d", MOB_ID, GPOINTER_TO_INT(p)); http("POST", q, NULL, 0, NULL); g_idle_add(mob_done, NULL); return NULL; }
static void mob_set(int on) { if (MOB_ID < 0) return; MOB_ON = on; if (FLAREA) gtk_widget_queue_draw(FLAREA); g_thread_unref(g_thread_new("mobile", mob_thread, GINT_TO_POINTER(on))); }
/* (1.6.0 Part 3: the cellular connection's notices, rudolfd's mobile/notes, as the page's notify shows them: a toast and the centre) */
static long CELL_SEEN = -1;
static void fget_note4(void *v, const char *k, char *o, size_t n) { const char *const *f = v; snprintf(o, n, "%s", !strcmp(k, "t") ? f[0] : !strcmp(k, "b") ? f[1] : !strcmp(k, "k") ? f[2] : !strcmp(k, "at") ? f[3] : ""); }
static gboolean tick_cell(gpointer d)
{
	(void)d; char q[64]; snprintf(q, sizeof q, "/api/mobile/notes?since=%ld", CELL_SEEN < 0 ? 0 : CELL_SEEN); JV *v = jget(q); if (!v) return G_SOURCE_CONTINUE;
	long last = (long)jnum(v, "last", 0); JV *a = jprop(v, "notes");
	if (CELL_SEEN >= 0 && USER_ID[0]) for (int i = 0; a && a->t == 4 && i < a->len; i++) { JV *o = jat(a, i); char t[160] = "", b[300] = "", at[40];
		jstr(o, "t", t, sizeof t); jstr(o, "b", b, sizeof b); snprintf(at, sizeof at, "%.0f", g_get_real_time() / 1000.0);
		if (LOCKED && strncmp(t, "💬", strlen("💬"))) continue;   /* (Part 6: a new message's notice also on the lock screen: its sender only, unless allowed) */
		const char *f[4] = {t, b, "info", at}; nt_notef(fget_note4, (void *)f); toast_text(t, b, 8000); }
	CELL_SEEN = last; jfree(v); return G_SOURCE_CONTINUE;
}
static void air_read(void) { JV *v = jget("/api/mobile/airplane"); if (v) AIR_ON = jbool(v, "on"); jfree(v); }
static gboolean air_done(gpointer p) { AIR_ON = GPOINTER_TO_INT(p) - 1; if (FLK == FL_QS && FLAREA) gtk_widget_queue_draw(FLAREA); return G_SOURCE_REMOVE; }
static gpointer air_thread(gpointer p) { char q[48]; snprintf(q, sizeof q, "/api/mobile/airplane?on=%d", GPOINTER_TO_INT(p)); http("POST", q, NULL, 0, NULL);
	JV *v = jget("/api/mobile/airplane"); int on = v ? jbool(v, "on") : GPOINTER_TO_INT(p); jfree(v); g_idle_add(air_done, GINT_TO_POINTER(on + 1)); return NULL; }
static void air_set(int on) { AIR_ON = on; if (FLAREA) gtk_widget_queue_draw(FLAREA); g_thread_unref(g_thread_new("airplane", air_thread, GINT_TO_POINTER(on))); }
static int qs_get(int i) { switch (i) { case 0: return !strcmp(S.dm, "dark"); case 1: return S.nl; case 2: return S.dnd; case 3: return S.sr; case 4: return S.mag; case 5: return S.hc; case 6: return S.vk; case 8: return AIR_ON; case 9: return MOB_ON; default: return S.touch; } }
static void qs_set(int i, int v) { switch (i) { case 0: snprintf(S.dm, sizeof S.dm, "%s", v ? "dark" : "light"); break; case 1: S.nl = v; break; case 2: S.dnd = v; break; case 3: S.sr = v; break; case 4: S.mag = v; break; case 5: S.hc = v; break; case 6: S.vk = v; break; case 8: AIR_ON = v; break; case 9: MOB_ON = v; break; default: S.touch = v; } }
/* the line box of writing with pictures in it (a fallback font's taller line, as Chromium's) */
static double fl_line(cairo_t *cr, const char *t, int wt, double px)
{
	(void)t; double a, d, ea, ed; a = css_ascent(cr, UIFAM, wt, px, &d); ea = css_ascent(cr, "Noto Color Emoji", wt, px, &ed);   /* (the emoji font's ascent and descent, each rounded, the larger of each) */
	return fmax(a, ea) + fmax(d, ed);
}
static void qs_layout(cairo_t *cr, int draw, int dark, double vw, double vh, int top)
{
	RGBA fg = cx(dark ? "#e2e8f0" : "#111827", 1), mut = cx(dark ? "#94a3b8" : "#6b7280", 1); double W = 340, cw = (W - 12) / 3, l11 = fl_lh(cr, 400, 11), l20 = fl_lh(cr, 400, 20);
	/* the tiles' heights: each row as tall as its tallest (a name on two lines) */
	int nl8[10]; for (int i = 0; i < QSN; i++) { PangoLayout *pl = css_layout(cr, QSL[i], UIFAM, 400, 11); pango_layout_set_width(pl, (int)((cw - 10) * PANGO_SCALE)); pango_layout_set_wrap(pl, PANGO_WRAP_WORD); nl8[i] = pango_layout_get_line_count(pl); g_object_unref(pl); }
	int NR = (QSN + 2) / 3; double rh[4] = {0}; for (int r = 0; r < NR; r++) { int m = 1; for (int c = 0; c < 3; c++) { int i = r * 3 + c; if (i < QSN && nl8[i] > m) m = nl8[i]; } rh[r] = 2 + 20 + l20 + 4 + m * l11; }
	double l12 = fl_lh(cr, 400, 12), bh = 3 + 3 + 2 + fl_line(cr, "🔒 Lock", 400, 13);
	double ch = rh[0] + rh[1] + rh[2] + rh[3] + (NR - 3) * 6 + 12 + 8 + l12 + 10 + bh;
	FLW = W + 22; FLH = ch + 22; FLX = vw - 8 - FLW; FLY = top ? 52 + TOPRES / Z : vh - 52 - FLH;
	if (!draw) return;
	Box b; memset(&b, 0, sizeof b); b.bg = cx(dark ? "#0f172a" : "#ffffff", 1); b.bw = 1; b.bc = cx(dark ? "#334155" : "#d1d5db", 1); b.rad = 12; shd_add(&b, 0, 14, 36, cx("#000000", .35), 0); hc_edge(&b); box_paint(cr, FLX, FLY, FLW, FLH, &b);
	double x = FLX + 11, y = FLY + 11;
	for (int i = 0; i < QSN; i++) { int r = i / 3, c = i % 3, on = qs_get(i); double tx = x + c * (cw + 6), ty = y; for (int q = 0; q < r; q++) ty += rh[q] + 6;
		Box t; memset(&t, 0, sizeof t); t.rad = 10; t.bw = 1; if (on) { t.bg = ca(accent(), 1); t.bc = ca(accent(), 1); } else { t.bg = cx(dark ? "#1e293b" : "#f3f4f6", 1); t.bc = cx(dark ? "#334155" : "#d1d5db", 1); }
		box_paint(cr, tx, ty, cw, rh[r], &t); RGBA ink = on ? cx("#ffffff", 1) : fg;
		double iw = css_width(cr, QSI[i], UIFAM, 400, 20); css_text(cr, QSI[i], UIFAM, 400, 20, tx + (cw - iw) / 2, ty + 11, l20, ink, 0);
		PangoLayout *pl = css_layout(cr, QSL[i], UIFAM, 400, 11); pango_layout_set_width(pl, (int)((cw - 10) * PANGO_SCALE)); pango_layout_set_wrap(pl, PANGO_WRAP_WORD);
		double a, d; css_metrics(pl, &a, &d); int n = pango_layout_get_line_count(pl); srca(cr, ink);
		for (int k = 0; k < n; k++) { PangoLayoutLine *L = pango_layout_get_line_readonly(pl, k); PangoRectangle lr; pango_layout_line_get_extents(L, NULL, &lr);
			double lw = lr.width / (double)PANGO_SCALE; cairo_move_to(cr, tx + 5 + (cw - 10 - lw) / 2 - lr.x / (double)PANGO_SCALE, round(ty + 11 + l20 + 4 + k * l11 + a)); pango_cairo_show_layout_line(cr, L); }
		g_object_unref(pl); char k[16]; snprintf(k, sizeof k, "qs:%d", i); fl_btn(k, tx, ty, cw, rh[r]); }
	y += rh[0] + rh[1] + rh[2] + rh[3] + (NR - 3) * 6 + 12 + 8;
	{ char t[260]; snprintf(t, sizeof t, "🌐 %s", NET_UP > 0 && NET_DESC[0] ? NET_DESC : "Not connected"); css_text(cr, t, UIFAM, 400, 12, x, y, l12, mut, W); }
	y += l12 + 10;
	static const char *const QB[3] = {"🔒 Lock", "⚙️ Settings", "⏻ Power"}, *const QBK[3] = {"lock", "settings", "power"};
	for (int i = 0; i < 3; i++) { double bx = x + i * (cw + 6); fl_button(cr, QB[i], bx, y, cw, bh, FLHOV >= 0 && !strcmp(FLBK[FLHOV], QBK[i]), dark); fl_btn(QBK[i], bx, y, cw, bh); }
}
static double FLSCROLL;   /* (the notifications' centre scrolled: the page's flyout is at most 70 % of the screen's height) */
static void nc_layout(cairo_t *cr, int draw, int dark, double vw, double vh, int top)
{
	RGBA fg = cx(dark ? "#e2e8f0" : "#111827", 1), mut = cx(dark ? "#94a3b8" : "#6b7280", 1), bd = cx(dark ? "#334155" : "#d1d5db", 1); double W = 340;
	double l13 = fl_lh(cr, 400, 13), lb = fl_lh(cr, 700, 13), l12 = fl_lh(cr, 400, 12), bh = 3 + l13 + 3 + 2, sm = 13 * 5 / 6.0, xw = css_width(cr, "✕", UIFAM, 400, 13) + 8;
	double ih[60], list = 0;
	for (int i = 0; i < NNT; i++) { ih[i] = 2 + 16 + lb + 2 + (nt_why(&NT[i]) ? fmax(l12, 2 + fl_lh(cr, 400, 11)) : l12); list += ih[i] + 6; }
	if (!NNT) list = 28 + l13;
	double ch = bh + 8 + list; FLW = W + 22; FLH = fmin(ch + 22, vh * .7); FLX = vw - 8 - FLW; FLY = top ? 52 + TOPRES / Z : vh - 52 - FLH;
	double maxs = fmax(0, ch + 22 - FLH); if (FLSCROLL > maxs) FLSCROLL = maxs; if (FLSCROLL < 0) FLSCROLL = 0;
	if (!draw) return;
	Box b; memset(&b, 0, sizeof b); b.bg = cx(dark ? "#0f172a" : "#ffffff", 1); b.bw = 1; b.bc = bd; b.rad = 12; shd_add(&b, 0, 14, 36, cx("#000000", .35), 0); hc_edge(&b); box_paint(cr, FLX, FLY, FLW, FLH, &b);
	cairo_save(cr); rrect(cr, FLX + 1, FLY + 1, FLW - 2, FLH - 2, 11); cairo_clip(cr);
	double x = FLX + 11, y = FLY + 11 - FLSCROLL;
	/* Notifications · Do not disturb · Clear all */
	css_text(cr, "Notifications", UIFAM, 700, 13, x, y + (bh - lb) / 2, lb, fg, 0);
	double cw = css_width(cr, "Clear all", UIFAM, 400, 13) + 22, cx0 = x + W - cw; fl_button(cr, "Clear all", cx0, y, cw, bh, FLHOV >= 0 && !strcmp(FLBK[FLHOV], "clear"), dark); fl_btn("clear", cx0, y, cw, bh);
	{ double tw = css_width(cr, " Do not disturb", UIFAM, 400, 13), lw = 20 + 4 + tw, lx = cx0 - 6 - lw, cby = y + (bh - 13) / 2;
		Box c; memset(&c, 0, sizeof c); c.bg = cx(S.dnd ? "#0075ff" : "#ffffff", 1); c.bw = 1; c.bc = cx(S.dnd ? "#0075ff" : "#767676", 1); c.rad = 2; box_paint(cr, lx + 4, cby, 13, 13, &c);
		if (S.dnd) { cairo_move_to(cr, lx + 7, cby + 6.5); cairo_line_to(cr, lx + 9.5, cby + 9); cairo_line_to(cr, lx + 14, cby + 4); cairo_set_source_rgb(cr, 1, 1, 1); cairo_set_line_width(cr, 1.8); cairo_stroke(cr); }
		css_text(cr, " Do not disturb", UIFAM, 400, 13, lx + 24, y + (bh - l13) / 2, l13, fg, 0); fl_btn("dnd", lx, y + (bh - 19) / 2, lw, 19); }
	y += bh + 8;
	if (!NNT) { double tw = css_width(cr, "No new notifications", UIFAM, 400, 13); css_text(cr, "No new notifications", UIFAM, 400, 13, x + (W - tw) / 2, y + 14, l13, (RGBA){ fg.r, fg.g, fg.b, .65 }, 0); }
	for (int i = 0; i < NNT; i++) { Note *n = &NT[i]; int err = !strcmp(n->k, "error"); double h = ih[i];
		Box it; memset(&it, 0, sizeof it); it.bw = 1; it.bc = bd; it.rad = 8; box_paint(cr, x, y, W, h, &it);
		if (err) { cairo_save(cr); rrect(cr, x, y, W, h, 8); cairo_clip(cr); cairo_rectangle(cr, x, y, 4, h); srca(cr, cx("#ef4444", 1)); cairo_fill(cr); cairo_restore(cr); }
		double ix = x + (err ? 4 : 1) + 8, iw = W - (err ? 4 : 1) - 1 - 16, iy = y + 9;
		/* the title, its time (smaller), ✕ */
		char tm[32]; { GTimeZone *tz = S.tz[0] ? g_time_zone_new_identifier(S.tz) : NULL; if (!tz) tz = g_time_zone_new_local(); GDateTime *d = g_date_time_new_from_unix_utc((gint64)(n->at / 1000)), *l = d ? g_date_time_to_timezone(d, tz) : NULL;
			int hh = l ? g_date_time_get_hour(l) : 0, mm = l ? g_date_time_get_minute(l) : 0; if (S.h24) snprintf(tm, sizeof tm, "%02d:%02d", hh, mm); else snprintf(tm, sizeof tm, "%d:%02d %s", hh % 12 ? hh % 12 : 12, mm, hh < 12 ? "AM" : "PM");
			if (l) g_date_time_unref(l); if (d) g_date_time_unref(d); g_time_zone_unref(tz); }
		double tmw = css_width(cr, tm, UIFAM, 400, sm), xx = ix + iw - xw, tx = xx - 6 - tmw;
		css_text(cr, n->t, UIFAM, 700, 13, ix, iy, lb, fg, tx - 6 - ix);
		double as, ds; as = css_ascent(cr, UIFAM, 400, sm, &ds); double base = iy + floor((lb - (css_ascent(cr, UIFAM, 700, 13, NULL) + 0)) / 2);   /* (the small time on the title's middle) */
		(void)base; css_text(cr, tm, UIFAM, 400, sm, tx, iy + (lb - (as + ds)) / 2, as + ds, fg, 0);
		css_text(cr, "✕", UIFAM, 400, 13, xx + 4, iy, lb, cx(dark ? "#e2e8f0" : "#000000", 1), 0); { char k[16]; snprintf(k, sizeof k, "nx:%d", i); fl_btn(k, xx, iy, xw, lb); }
		double by = iy + lb + 2; int why = nt_why(n); double bl = why ? fmax(l12, 2 + fl_lh(cr, 400, 11)) : l12;
		double tw = css_text(cr, n->b, UIFAM, 400, 12, ix, by + floor((bl - l12) / 2), l12, mut, why ? iw - (n->wl[0] ? 100 : 70) : iw);
		char wlb[48]; snprintf(wlb, sizeof wlb, "⚡\uFE0E %s", n->wl[0] ? n->wl : "Why?");
		if (why) { double ww = css_width(cr, wlb, UIFAM, 400, 11) + 14, wx = ix + tw + 6; Box w; memset(&w, 0, sizeof w); w.bg = cx(dark ? "#1e293b" : "#ffffff", 1); w.bw = 1; w.bc = bd; w.rad = 4;
			box_paint(cr, wx, by, ww, bl, &w); css_text(cr, wlb, UIFAM, 400, 11, wx + 7, by + 1, bl - 2, cx(dark ? "#e2e8f0" : "#000000", 1), 0); char k[16]; snprintf(k, sizeof k, "why:%d", i); fl_btn(k, wx, by, ww, bl); }
		y += h + 6; }
	cairo_restore(cr);
}
/* ---- the calendar's table: Chromium's automatic table layout (each column its widest cell, the rest shared out as wide as they are) */
static void cal_cols(cairo_t *cr, int y, int mo, double total, double *cw)
{
	static const char *const DN[7] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"}; double mx[7], sum = 0;
	GDate *f = g_date_new_dmy(1, mo, y); int st = g_date_get_weekday(f) % 7, n = g_date_get_days_in_month(mo, y); g_date_free(f);
	for (int c = 0; c < 7; c++) mx[c] = css_width(cr, DN[c], UIFAM, 700, 12);
	for (int k = 1; k <= n; k++) { char s[8]; snprintf(s, sizeof s, "%d", k); int c = (st + k - 1) % 7; char key[12]; snprintf(key, sizeof key, "%04d-%02d-%02d", y, mo, k);
		int ev = 0; for (int i = 0; i < NCALEV; i++) if (!strcmp(CALEV[i].d, key)) ev = 1; mx[c] = fmax(mx[c], css_width(cr, s, UIFAM, ev ? 700 : 400, 12)); }
	for (int c = 0; c < 7; c++) { mx[c] += 8; sum += mx[c]; }
	for (int c = 0; c < 7; c++) cw[c] = sum < total ? mx[c] * total / sum : mx[c];
}
static int ev_cmp(const void *a, const void *b) { const CalEv *x = a, *y = b; char p[40], q[40]; snprintf(p, sizeof p, "%s%s", x->d, x->t); snprintf(q, sizeof q, "%s%s", y->d, y->t); return strcmp(p, q); }
/* ---- (item 3's last part) the network's pop-up (net31.js net31Pop, .np: the adapters, the one in use in the accent colour, the
   others tinted when hovered, Network Settings…; placed as flyPlace places it before the Wi-Fi part is filled in) */
typedef struct { char name[32], ssid[96], ip[64], state[16], cell[96]; int wireless, speed, cellular; } NetIf;
static NetIf NIF[8]; static int NNIF, NCUR = -1, NWTOOLS;
/* (wfDecode: the SSID as wpa_supplicant writes it, \xNN for its bytes) */
static void wf_decode(const char *s, char *o, size_t n)
{
	size_t k = 0; for (const char *p = s; *p && k + 1 < n; ) { if (p[0] == '\\' && p[1] == 'x' && g_ascii_isxdigit(p[2]) && g_ascii_isxdigit(p[3])) { o[k++] = (char)(g_ascii_xdigit_value(p[2]) * 16 + g_ascii_xdigit_value(p[3])); p += 4; } else o[k++] = *p++; }
	o[k] = 0; if (!g_utf8_validate(o, -1, NULL)) snprintf(o, n, "%s", s);
}
/* (net31Pick: the real adapters, the one with the default route and an address, else the first one up with an address) */
static void net_read(void)
{
	JV *v = jget("/api/net"); NNIF = 0; NCUR = -1; NWTOOLS = v ? jbool(v, "wifiTools") : 0; JV *ifs = jprop(v, "interfaces"); int dflt[8] = {0};
	int clat = 0; for (int i = 0; ifs && i < 32; i++) { JV *o = jat(ifs, i); if (!o) break; char nm[32] = ""; jstr(o, "name", nm, sizeof nm); if (!strcmp(nm, "clat") && jbool(o, "default")) clat = 1; }
	for (int i = 0; ifs && i < 32 && NNIF < 8; i++) { JV *o = jat(ifs, i); if (!o) break; if (jbool(o, "virtual") && !jbool(o, "cellular")) continue; NetIf *f = &NIF[NNIF]; memset(f, 0, sizeof *f);
		if ((f->cellular = jbool(o, "cellular"))) { char ca[64] = "", te[16] = ""; jstr(o, "carrier", ca, sizeof ca); jstr(o, "tech", te, sizeof te); snprintf(f->cell, sizeof f->cell, "%s%s%s", ca, ca[0] && te[0] ? " · " : "", te); if (clat) dflt[NNIF] = 2; }
		jstr(o, "name", f->name, sizeof f->name); char ss[96] = ""; jstr(o, "ssid", ss, sizeof ss); wf_decode(ss, f->ssid, sizeof f->ssid); jstr(o, "ip", f->ip, sizeof f->ip); jstr(o, "state", f->state, sizeof f->state);
		f->wireless = jbool(o, "wireless"); f->speed = (int)jnum(o, "speed", 0); if (!dflt[NNIF]) dflt[NNIF] = jbool(o, "default"); NNIF++; }
	for (int i = 0; i < NNIF && NCUR < 0; i++) if (dflt[i] && (NIF[i].ip[0] || dflt[i] == 2)) NCUR = i;
	for (int i = 0; i < NNIF && NCUR < 0; i++) if (NIF[i].ip[0] && !strcmp(NIF[i].state, "up")) NCUR = i;
	jfree(v);
}
static int fl_hov(const char *k) { return FLHOV >= 0 && !strcmp(FLBK[FLHOV], k); }
/* the lines a text breaks into at width W, as a browser breaks white-space:normal text */
static int css_wrap(cairo_t *cr, const char *t, const char *fam, int wt, double px, double w, char (*out)[320], int max)
{
	PangoLayout *pl = css_layout(cr, t, fam, wt, px); pango_layout_set_width(pl, (int)(w * PANGO_SCALE)); pango_layout_set_wrap(pl, PANGO_WRAP_WORD_CHAR);
	int n = pango_layout_get_line_count(pl), k = 0;
	for (int i = 0; i < n && k < max; i++) { PangoLayoutLine *ln = pango_layout_get_line_readonly(pl, i); int len = ln->length; while (len > 0 && t[ln->start_index + len - 1] == ' ') len--;
		snprintf(out[k++], 320, "%.*s", len, t + ln->start_index); }
	g_object_unref(pl); return k ? k : 1;
}
/* a page button with a font of its own (fl_button's, at PX) */
static void fl_button_px(cairo_t *cr, const char *t, double px, double x, double y, double w, double h, int hv, int dark)
{
	Box k; memset(&k, 0, sizeof k); k.rad = 4; k.bw = 1;
	if (dark) { k.bg = cx("#1e293b", 1); k.bc = cx("#334155", 1); } else { k.bg = cx(hv ? "#eaf0ff" : "#ffffff", 1); k.bc = cx(hv ? "#a9bff5" : "#d1d5db", 1); }
	box_paint(cr, x, y, w, h, &k); double tw = css_width(cr, t, UIFAM, 400, px), a, d; a = css_ascent(cr, UIFAM, 400, px, &d);
	css_text(cr, t, UIFAM, 400, px, x + (w - tw) / 2, y + (h - (a + d)) / 2, a + d, cx(dark ? "#e2e8f0" : "#000000", 1), 0);
}
static void fl_place_over(const double *B, int top)
{
	double vw = SW / Z, vh = SH / Z, bx = B[0] + bar_x() / Z, by = B[1] + bar_y() / Z, bw = B[2], bh = B[3];
	FLX = bx + bw / 2 - FLW / 2; FLY = top ? by + bh + 8 : by - FLH - 8;
	if (tb_side() == 1) { FLX = bx + bw + 8; FLY = by; } else if (tb_side() == 2) { FLX = bx - FLW - 8; FLY = by; }   /* (flyPlace: beside a bar at the left or the right) */
	FLX = fmax(4, fmin(vw - FLW - 4, FLX)); FLY = fmax(4, fmin(vh - FLH - 4, FLY));
}
/* the network (.np: 330 px of content, 10 px 12 px around it, 12 px type) */
static void fl_net(cairo_t *cr, int draw, int dark, int top)
{
	double l12 = fl_lh(cr, 400, 12), l13 = fl_lh(cr, 400, 13), l11 = fl_lh(cr, 400, 11); const NetIf *wl = NULL; for (int i = 0; i < NNIF && !wl; i++) if (NIF[i].wireless) wl = &NIF[i];
	char msg[200] = ""; if (wl && !NWTOOLS) snprintf(msg, sizeof msg, "This system has no Wi-Fi tools; update Rudolf OS or use a network cable.");
	char ml[6][320]; int nm = msg[0] ? css_wrap(cr, msg, UIFAM, 400, 13, 330, ml, 6) : 0; double mh = nm ? fmax(18, nm * l13) : 0;
	double rows = NNIF ? NNIF * (10 + l12) : l12, bh = 2 + 6 + l12;
	/* (placed as the page places it: by flyPlace before the Wi-Fi's part is filled in, so it grows downwards from there) */
	FLW = 330 + 24 + 2; FLH = 1 + 10 + l12 + 2 + l12 + 6 + rows + 8 + bh + 10 + 1; fl_place_over(NETB, top); FLH += nm ? 10 + mh : 0;   /* (an empty Wi-Fi part: its margin and the buttons' one as one) */
	if (!draw) return;
	Box b; memset(&b, 0, sizeof b); b.bg = cx("#ffffff", 1); b.bw = 1; b.bc = cx(dark ? "#334155" : "#d1d5db", 1); b.rad = 10; shd_add(&b, 0, 10, 26, cx("#000000", .3), 0); box_paint(cr, FLX, FLY, FLW, FLH, &b);
	RGBA ink = cx("#111111", 1); double x = FLX + 13, y = FLY + 11;
	css_text(cr, "Network", UIFAM, 700, 12, x, y, l12, ink, 0); y += l12 + 2;
	css_text(cr, NCUR >= 0 ? "Connected" : "Not connected", UIFAM, 400, 12, x, y, l12, cx("#4b5563", 1), 0); y += l12 + 6;
	if (!NNIF) { css_text(cr, "No network adapter was found.", UIFAM, 400, 12, x, y, l12, cx("#6b7280", 1), 0); y += l12; }
	for (int i = 0; i < NNIF; i++) { const NetIf *f = &NIF[i]; int on = i == NCUR; char key[16]; snprintf(key, sizeof key, "row:%d", i); double rh = 10 + l12;
		RGBA bg = on ? ca(accent(), 1) : fl_hov(key) ? ca(accent(), .18) : (RGBA){0, 0, 0, 0};
		if (bg.a > 0) { Box r; memset(&r, 0, sizeof r); r.bg = bg; r.rad = 6; box_paint(cr, x, y, 330, rh, &r); }
		RGBA c1 = on ? cx("#ffffff", 1) : ink, c2 = on ? cx("#ffffff", 1) : cx("#15803d", 1);
		char nm2[200]; if (f->cellular) snprintf(nm2, sizeof nm2, "Mobile data%s%s", f->cell[0] ? " · " : "", f->cell); else if (f->wireless) { if (f->ssid[0] && f->ip[0]) snprintf(nm2, sizeof nm2, "Wi-Fi · %s", f->ssid); else snprintf(nm2, sizeof nm2, "Wi-Fi"); } else snprintf(nm2, sizeof nm2, "Ethernet (%s)", f->name);
		const char *st = f->cellular && !on ? "In reserve" : f->cellular ? "Connected" : f->ip[0] ? "Connected" : !strcmp(f->state, "up") ? "No address" : f->wireless ? "Not connected" : "No cable";
		const char *ic = f->wireless || f->cellular ? "📶" : "🔌"; double iw = css_width(cr, ic, UIFAM, 400, 12), sw = css_width(cr, st, UIFAM, 400, 11);
		css_text(cr, ic, UIFAM, 400, 12, x + 6, y + 5, l12, c1, 0);
		double bx = x + 6 + iw + 8, bw = 330 - 12 - iw - 8 - 8 - sw; css_text(cr, nm2, UIFAM, 600, 12, bx, y + 5, l12, c1, bw);
		css_text(cr, st, UIFAM, 400, 11, x + 330 - 6 - sw, y + 5 + (l12 - l11) / 2, l11, c2, 0); fl_btn(key, x, y, 330, rh); y += rh; }
	if (nm) { y += 10; for (int i = 0; i < nm; i++) css_text(cr, ml[i], UIFAM, 400, 13, x, y + i * l13, l13, ink, 0); y += mh; }
	y += 8; { double tw = css_width(cr, "Network Settings…", UIFAM, 400, 12) + 22; fl_button_px(cr, "Network Settings…", 12, x + 330 - tw, y, tw, bh, fl_hov("npo"), 0); fl_btn("npo", x + 330 - tw, y, tw, bh); }
}
/* the CSS border of a notification: 1 px, the left one 4 px red for an error (the corners split between the sides) */
static void nc_rr(cairo_t *cr, double x, double y, double w, double h, double rxl, double ryl, double rxr, double ryr)
{
	cairo_new_sub_path(cr);
	cairo_save(cr); cairo_translate(cr, x + w - rxr, y + ryr); cairo_scale(cr, rxr, ryr); cairo_arc(cr, 0, 0, 1, -M_PI / 2, 0); cairo_restore(cr);
	cairo_save(cr); cairo_translate(cr, x + w - rxr, y + h - ryr); cairo_scale(cr, rxr, ryr); cairo_arc(cr, 0, 0, 1, 0, M_PI / 2); cairo_restore(cr);
	cairo_save(cr); cairo_translate(cr, x + rxl, y + h - ryl); cairo_scale(cr, rxl, ryl); cairo_arc(cr, 0, 0, 1, M_PI / 2, M_PI); cairo_restore(cr);
	cairo_save(cr); cairo_translate(cr, x + rxl, y + ryl); cairo_scale(cr, rxl, ryl); cairo_arc(cr, 0, 0, 1, M_PI, 3 * M_PI / 2); cairo_restore(cr);
	cairo_close_path(cr);
}
/* the box's size and place, and (DRAW) its drawing */
static void fl_layout(cairo_t *cr, int draw)
{
	int dark = menu_dark(); double vw = SW / Z, vh = SH / Z; int top = !strcmp(S.tb, "top"); NFLB = 0;
	RGBA fg = cx(dark ? "#e2e8f0" : "#111827", 1), mut = cx(dark ? "#94a3b8" : "#6b7280", 1);
	if (FLK == FL_CAL) {
		GDateTime *now = fl_now(); int ty = g_date_time_get_year(now), tm = g_date_time_get_month(now), td = g_date_time_get_day_of_month(now);
		char tim[32], lng[96]; int h = g_date_time_get_hour(now), mi = g_date_time_get_minute(now);
		if (S.h24) snprintf(tim, sizeof tim, "%02d:%02d", h, mi); else snprintf(tim, sizeof tim, "%d:%02d %s", h % 12 ? h % 12 : 12, mi, h < 12 ? "AM" : "PM");
		{ char *s = g_date_time_format(now, "%A, %B %-d, %Y"); snprintf(lng, sizeof lng, "%s", s ? s : ""); g_free(s); }
		char iso[12]; snprintf(iso, sizeof iso, "%04d-%02d-%02d", ty, tm, td); g_date_time_unref(now);
		GDate *f = g_date_new_dmy(1, FL_M, FL_Y); int st = g_date_get_weekday(f) % 7, n = g_date_get_days_in_month(FL_M, FL_Y); g_date_free(f); int rows = (st + n + 6) / 7;
		CalEv up[4]; int nu = 0; { CalEv all[64]; int na = 0; for (int i = 0; i < NCALEV; i++) if (strcmp(CALEV[i].d, iso) >= 0) all[na++] = CALEV[i]; qsort(all, na, sizeof all[0], ev_cmp); for (int i = 0; i < na && nu < 4; i++) up[nu++] = all[i]; }
		double l30 = fl_lh(cr, 300, 30), l13 = fl_lh(cr, 400, 13), l12 = fl_lh(cr, 400, 12), bh = 3 + l13 + 3 + 2, rh = 8 + l12;
		double ch = l30 + l13 + 8 + bh + 8 + (1 + rows) * rh + 8 + (nu ? nu : 1) * l12 + 8 + bh;
		FLW = 300 + 20 + 2; FLH = ch + 20 + 2; FLX = vw - 8 - FLW; FLY = top ? 52 + TOPRES / Z : vh - 52 - FLH;
		if (!draw) return;
		Box b; memset(&b, 0, sizeof b); b.bg = cx(dark ? "#0f172a" : "#ffffff", 1); b.bw = 1; b.bc = cx(dark ? "#334155" : "#d1d5db", 1); b.rad = 12; shd_add(&b, 0, 14, 36, cx("#000000", .35), 0); hc_edge(&b); box_paint(cr, FLX, FLY, FLW, FLH, &b);
		double x = FLX + 11, y = FLY + 11, W = 300;
		css_text(cr, tim, UIFAM, 300, 30, x, y, l30, fg, 0); y += l30; css_text(cr, lng, UIFAM, 400, 13, x, y, l13, mut, 0); y += l13 + 8;
		/* ◀ November 2026 ▶ */
		double pw = css_width(cr, "◀", UIFAM, 400, 13) + 22, nw = css_width(cr, "▶", UIFAM, 400, 13) + 22;
		fl_button(cr, "◀", x, y, pw, bh, FLHOV >= 0 && !strcmp(FLBK[FLHOV], "prev"), dark); fl_btn("prev", x, y, pw, bh);
		fl_button(cr, "▶", x + W - nw, y, nw, bh, FLHOV >= 0 && !strcmp(FLBK[FLHOV], "next"), dark); fl_btn("next", x + W - nw, y, nw, bh);
		{ char m[64]; snprintf(m, sizeof m, "%s %d", FLMON[FL_M - 1], FL_Y); double mw = css_width(cr, m, UIFAM, 700, 13), bx = x + pw + 6, bw = W - pw - nw - 12;
			css_text(cr, m, UIFAM, 700, 13, bx + (bw - mw) / 2, y + (bh - l13) / 2, l13, fg, 0); }
		y += bh + 8;
		double cw[7]; cal_cols(cr, FL_Y, FL_M, W, cw); static const char *const DN[7] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
		{ double cx0 = x; for (int c = 0; c < 7; c++) { double tw = css_width(cr, DN[c], UIFAM, 700, 12); css_text(cr, DN[c], UIFAM, 700, 12, cx0 + (cw[c] - tw) / 2, y + 4, l12, fg, 0); cx0 += cw[c]; } }
		for (int k = 1; k <= n; k++) { int c = (st + k - 1) % 7, r = (st + k - 1) / 7; double cx0 = x; for (int q = 0; q < c; q++) cx0 += cw[q]; double cy0 = y + rh * (r + 1);
			char s[8], key[12]; snprintf(s, sizeof s, "%d", k); snprintf(key, sizeof key, "%04d-%02d-%02d", FL_Y, FL_M, k);
			int ev = 0; for (int i = 0; i < NCALEV; i++) if (!strcmp(CALEV[i].d, key)) ev = 1; RGBA col = fg;
			if (k == td && FL_M == tm && FL_Y == ty) { double ex = cx0, ey = cy0, ew = cw[c], eh = rh; snap(&ex, &ey, &ew, &eh); cairo_save(cr); cairo_translate(cr, ex + ew / 2, ey + eh / 2); cairo_scale(cr, ew / 2, eh / 2);
				cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI); cairo_restore(cr); srca(cr, ca(accent(), 1)); cairo_fill(cr); col = cx("#ffffff", 1); }
			int wt = ev ? 700 : 400; double tw = css_width(cr, s, UIFAM, wt, 12), tx = cx0 + (cw[c] - tw) / 2; css_text(cr, s, UIFAM, wt, 12, tx, cy0 + 4, l12, col, 0);
			if (ev) { /* (text-decoration: underline, at the font's own place and thickness) */
				PangoLayout *pl = css_layout(cr, s, UIFAM, wt, 12); PangoFontMetrics *fm = pango_context_get_metrics(pango_layout_get_context(pl), pango_layout_get_font_description(pl), NULL);
				double up_ = pango_font_metrics_get_underline_position(fm) / (double)PANGO_SCALE, th = fmax(1, round(pango_font_metrics_get_underline_thickness(fm) / (double)PANGO_SCALE)); pango_font_metrics_unref(fm);
				double a, d; css_metrics(pl, &a, &d); g_object_unref(pl); double base = round(cy0 + 4 + floor((l12 - (a + d)) / 2) + a);
				cairo_rectangle(cr, tx, round(base - up_), tw, th); srca(cr, col); cairo_fill(cr); }
			fl_btn(key, cx0, cy0, cw[c], rh); }
		y += (1 + rows) * rh + 8;
		if (!nu) css_text(cr, "No upcoming events", UIFAM, 400, 12, x, y, l12, (RGBA){ fg.r, fg.g, fg.b, .7 }, 0);
		for (int i = 0; i < nu; i++) { char t[400]; snprintf(t, sizeof t, "📅 %.11s %.11s%s— %.159s", up[i].d, up[i].t, up[i].t[0] ? " " : "", up[i].title); css_text(cr, t, UIFAM, 400, 12, x, y + i * l12, l12, fg, W); }
		y += (nu ? nu : 1) * l12 + 8;
		fl_button(cr, "Open Calendar", x, y, W, bh, FLHOV >= 0 && !strcmp(FLBK[FLHOV], "open"), dark); fl_btn("open", x, y, W, bh);
		return;
	}
	if (FLK == FL_NET) { fl_net(cr, draw, dark, top); return; }
	if (FLK == FL_QS) { qs_layout(cr, draw, dark, vw, vh, top); return; }
	if (FLK == FL_NC) { nc_layout(cr, draw, dark, vw, vh, top); return; }
	if (FLK == FL_BRI) {   /* (1.6.0) the brightness (.vp .bvp: Brightness, its range from 5 to 100 % and the level) */
		double l12 = fl_lh(cr, 400, 12), lb = fl_lh(cr, 700, 12); FLW = 210 + 24 + 2; FLH = 10 + lb + 6 + 20 + 10 + 2; fl_place_over(BRIB, top);
		if (!draw) return;
		Box b; memset(&b, 0, sizeof b); b.bg = cx("#ffffff", 1); b.bw = 1; b.bc = cx(dark ? "#334155" : "#d1d5db", 1); b.rad = 10; shd_add(&b, 0, 10, 26, cx("#000000", .3), 0); box_paint(cr, FLX, FLY, FLW, FLH, &b);
		RGBA ink = cx("#111111", 1); double x = FLX + 13, y = FLY + 11;
		css_text(cr, "Brightness", UIFAM, 700, 12, x, y, lb, ink, 0); y += lb + 6;
		{ double rx = x + 2, ry = y + 2, rw = 154, v = BRI < 0 ? 1 : (CLAMP(BRI, 5, 100) - 5) / 95.0, tc = rx + 8 + (rw - 16) * v;
			Box t; memset(&t, 0, sizeof t); t.bg = cx("#efefef", 1); t.bw = 1; t.bc = cx("#b2b2b2", 1); t.rad = 4; box_paint(cr, rx, ry + 4, rw, 8, &t);
			cairo_save(cr); cairo_rectangle(cr, rx - 1, ry, tc - rx + 1, 16); cairo_clip(cr); Box f; memset(&f, 0, sizeof f); f.bg = cx("#0075ff", 1); f.bw = 1; f.bc = cx("#3b75ba", 1); f.rad = 4; box_paint(cr, rx, ry + 4, rw, 8, &f); cairo_restore(cr);
			cairo_arc(cr, tc, ry + 8, 8, 0, 2 * M_PI); srca(cr, cx("#0075ff", 1)); cairo_fill(cr); fl_btn("range", rx, ry, rw, 16);
			if (BRI >= 0) { char vt[16]; snprintf(vt, sizeof vt, "%d%%", BRI); double tw = css_width(cr, vt, UIFAM, 400, 12); css_text(cr, vt, UIFAM, 400, 12, x + 210 - tw, y + (20 - l12) / 2, l12, ink, 0); } }
		return;
	}
	/* the volume (.vp .svp) */
	double l12 = fl_lh(cr, 400, 12), l11 = fl_lh(cr, 400, 11), lb = fl_lh(cr, 700, 12);
	FLW = 210 + 24 + 2; FLH = 10 + lb + 6 + 20 + 8 + 19 + 8 + l11 + 10 + 2;
	fl_place_over(VOLB, top);
	if (!draw) return;
	Box b; memset(&b, 0, sizeof b); b.bg = cx("#ffffff", 1); b.bw = 1; b.bc = cx(dark ? "#334155" : "#d1d5db", 1); b.rad = 10; shd_add(&b, 0, 10, 26, cx("#000000", .3), 0); box_paint(cr, FLX, FLY, FLW, FLH, &b);
	RGBA ink = cx("#111111", 1); double x = FLX + 13, y = FLY + 11;
	css_text(cr, "Volume", UIFAM, 700, 12, x, y, lb, ink, 0); y += lb + 6;
	/* the range (Chromium's: an 8 px track, #efefef with a #b2b2b2 edge, the value's part #0075ff, a 16 px thumb) */
	{ double rx = x + 2, ry = y + 2, rw = 154, v = FLVOL < 0 ? 0 : FLVOL / 100.0, tc = rx + 8 + (rw - 16) * v;
		Box t; memset(&t, 0, sizeof t); t.bg = cx("#efefef", 1); t.bw = 1; t.bc = cx("#b2b2b2", 1); t.rad = 4; box_paint(cr, rx, ry + 4, rw, 8, &t);
		cairo_save(cr); cairo_rectangle(cr, rx - 1, ry, tc - rx + 1, 16); cairo_clip(cr); Box f; memset(&f, 0, sizeof f); f.bg = cx("#0075ff", 1); f.bw = 1; f.bc = cx("#3b75ba", 1); f.rad = 4; box_paint(cr, rx, ry + 4, rw, 8, &f); cairo_restore(cr);
		cairo_arc(cr, tc, ry + 8, 8, 0, 2 * M_PI); srca(cr, cx("#0075ff", 1)); cairo_fill(cr); fl_btn("range", rx, ry, rw, 16);
		char vt[16]; if (FLMUTE) snprintf(vt, sizeof vt, "Muted"); else snprintf(vt, sizeof vt, "%d%%", FLVOL < 0 ? 0 : FLVOL);
		double tw = css_width(cr, vt, UIFAM, 400, 12); css_text(cr, vt, UIFAM, 400, 12, x + 210 - tw, y + (20 - l12) / 2, l12, ink, 0); }
	y += 20 + 8;
	/* Mute (a checkbox as Chromium draws one, 13 px, 3 px 3 px 3 px 4 px around it) */
	{ double bx = x + 4, by = y + 3; Box c; memset(&c, 0, sizeof c); c.bg = cx(FLMUTE ? "#0075ff" : "#ffffff", 1); c.bw = 1; c.bc = cx(FLMUTE ? "#0075ff" : "#767676", 1); c.rad = 2; box_paint(cr, bx, by, 13, 13, &c);
		if (FLMUTE) { cairo_move_to(cr, bx + 3, by + 6.5); cairo_line_to(cr, bx + 5.5, by + 9); cairo_line_to(cr, bx + 10, by + 4); cairo_set_source_rgb(cr, 1, 1, 1); cairo_set_line_width(cr, 1.8); cairo_stroke(cr); }
		double a, d; a = css_ascent(cr, UIFAM, 400, 12, &d); css_text(cr, " Mute", UIFAM, 400, 12, bx + 13 + 3, by + 13 - a, a + d, ink, 0);   /* (the words' baseline on the box's bottom edge, as Chromium lines them up) */ fl_btn("mute", x, y, 210, 19); }
	y += 19 + 8;
	{ double tw = css_width(cr, "Desktop sounds…", UIFAM, 400, 11), a, d; a = css_ascent(cr, UIFAM, 400, 11, &d); RGBA bl = cx("#0000ee", 1);
		css_text(cr, "Desktop sounds…", UIFAM, 400, 11, x, y, l11, bl, 0); cairo_rectangle(cr, x, round(y + a) + 1, tw, 1); srca(cr, bl); cairo_fill(cr); fl_btn("sounds", x, y, 210, l11); }
}
static gboolean fl_draw(GtkWidget *wd, cairo_t *cr, gpointer d)
{
	(void)wd; (void)d; if (FLBG) { gdk_cairo_set_source_pixbuf(cr, FLBG, 0, 0); cairo_paint(cr); }
	lp_draw_clip(cr, FLWIN, FLWX, FLWY);   /* (Part 17b: opening) */
	cairo_translate(cr, -FLWX, -FLWY); cairo_scale(cr, Z, Z); fl_layout(cr, 1); return TRUE;
}
/* (2026-11-16, k160_t.py in QEMU: a click on the desktop did not close quick settings) a pop-up takes the pointer so that a click anywhere
   else comes to it and closes it, as the page's pointerdown does; the grab was asked for at once after showing the window, and X refuses a
   grab of a window not on the screen yet (a slower computer, QEMU: GrabNotViewable), so the clicks went past it. Asked again until it
   holds, while the pop-up is shown (and the desktop, the taskbar and the windows close it too: fl_outside) */
typedef struct { GtkWidget *w; int owner, n; } GrabTry;
static int FL_GRAB;   /* (tests: the pop-up holds the pointer) */
static gboolean seat_grab_try(gpointer d)
{
	GrabTry *t = d; GdkWindow *g = t->w && gtk_widget_get_visible(t->w) ? gtk_widget_get_window(t->w) : NULL;
	if (!g) { g_free(t); return G_SOURCE_REMOVE; }
	GdkGrabStatus st = gdk_window_is_viewable(g) ? gdk_seat_grab(gdk_display_get_default_seat(gdk_display_get_default()), g, GDK_SEAT_CAPABILITY_ALL, t->owner, NULL, NULL, NULL, NULL) : GDK_GRAB_NOT_VIEWABLE;
	if (t->w == FLWIN) { FL_GRAB = st == GDK_GRAB_SUCCESS; state_write(); }
	if (st == GDK_GRAB_SUCCESS || ++t->n > 80) { g_free(t); return G_SOURCE_REMOVE; }
	return G_SOURCE_CONTINUE;
}
static void seat_grab(GtkWidget *w, int owner) { GrabTry *t = g_new0(GrabTry, 1); t->w = w; t->owner = owner; if (seat_grab_try(t) == G_SOURCE_CONTINUE) g_timeout_add(25, seat_grab_try, t); }
/* a press the pop-up did not take (its grab not there yet): elsewhere, so it closes, as the page's */
static int fl_outside(void) { if (!FLK) return 0; fl_close(); return 1; }
/* (the taskbar pressed with a pop-up open that its grab missed: closed, as fl_press does; its own button closes it, another goes on to open its own) */
static int fl_bar_press(GdkEventButton *e, int h)
{
	if (!FLK || e->type != GDK_BUTTON_PRESS) return 0; int k = FLK; fl_close();
	return e->button != 1 || h < 0 || (k == FL_QS && !strcmp(HITS[h].kind, "qs")) || (k == FL_NC && !strcmp(HITS[h].kind, "notes")) || (k == FL_CAL && !strcmp(HITS[h].kind, "clock"))
		|| (k == FL_VOL && !strcmp(HITS[h].kind, "vol")) || (k == FL_NET && !strcmp(HITS[h].kind, "net")) || (k == FL_BRI && !strcmp(HITS[h].kind, "bri"));
}
static void fl_close(void)
{
	if (FLWIN && gtk_widget_get_visible(FLWIN)) { gdk_seat_ungrab(gdk_display_get_default_seat(gdk_display_get_default())); gtk_widget_hide(FLWIN); pop_gone(FLBG, FLWX, FLWY); }
	FL_GRAB = 0; if (FLBG) { g_object_unref(FLBG); FLBG = NULL; } FLK = FL_NONE; FLHOV = FLPRESS = -1; FLDRAG = 0; state_write();
}
static void fl_volume_set(const char *q) { char p[80]; snprintf(p, sizeof p, "/api/volume?%s", q); JV *v = NULL; GByteArray *b; if (http("POST", p, NULL, 0, &b) == 200 && b) { v = jparse((char *)b->data); }
	if (b) g_byte_array_free(b, TRUE); if (v && jbool(v, "ok")) { VOL = FLVOL = (int)jnum(v, "volume", FLVOL); MUTED = FLMUTE = jbool(v, "muted"); bar_redraw(); } jfree(v); }
static void fl_range(double px) { double x = (px + FLWX) / Z; int v = FLVOL;
	if (FLK == FL_BRI) { int b = BRI; for (int i = 0; i < NFLB; i++) if (!strcmp(FLBK[i], "range")) b = 5 + (int)lround((x - FLB[i].x - 8) / (154 - 16) * 95);
		b = CLAMP(b, 5, 100); if (b != BRI) { bri_set(b); gtk_widget_queue_draw(FLAREA); } return; } for (int i = 0; i < NFLB; i++) if (!strcmp(FLBK[i], "range")) v = (int)lround((x - FLB[i].x - 8) / (154 - 16) * 100);
	v = CLAMP(v, 0, 100); if (v == FLVOL && !FLMUTE) return; FLVOL = v; FLMUTE = 0; gtk_widget_queue_draw(FLAREA); char q[24]; snprintf(q, sizeof q, "set=%d", v); fl_volume_set(q); }
/* a switch changed: here at once, the page's own (#app=do-qs: saved as the page saves it, which the windows' pages hear) */
/* (2026-11-16, k160_t.py in QEMU: the shell's switches disagreed with the windows) a switch the shell changed stays as it showed it until the
   saved settings have it too (the page doing it saves it a moment later: settings read in between, another window's save, are older) */
static struct { int on; gint64 t; } QSP[8];
static void qs_pending(int i, int on) { if (i >= 0 && i < 8) { QSP[i].on = on; QSP[i].t = g_get_monotonic_time(); } }
static void qs_keep(void)
{
	gint64 now = g_get_monotonic_time();
	for (int i = 0; i < 8; i++) if (QSP[i].t) { if (now - QSP[i].t > 20 * G_USEC_PER_SEC || qs_get(i) == QSP[i].on) QSP[i].t = 0; else qs_set(i, QSP[i].on); }
}
/* (a JSON value written again as the page's JSON.stringify writes it: numbers as short as they read back the same) */
static void jv_dump(GString *o, const JV *v)
{
	if (!v || v->t == 0) { g_string_append(o, "null"); return; }
	if (v->t == 1) { g_string_append(o, v->n ? "true" : "false"); return; }
	if (v->t == 2) { if (!isfinite(v->n)) { g_string_append(o, "null"); return; } char b[40]; for (int p = 1; p <= 17; p++) { g_snprintf(b, sizeof b, "%.*g", p, v->n); if (g_ascii_strtod(b, NULL) == v->n) break; }
		char *e = strchr(b, ','); if (e) *e = '.'; g_string_append(o, b); return; }
	if (v->t == 3) { json_esc(o, v->s); return; }
	g_string_append_c(o, v->t == 4 ? '[' : '{');
	for (int i = 0; i < v->len; i++) { if (i) g_string_append_c(o, ','); if (v->t == 5) { json_esc(o, v->k[i]); g_string_append_c(o, ':'); } jv_dump(o, v->v[i]); }
	g_string_append_c(o, v->t == 4 ? ']' : '}');
}
/* (2026-11-16, k160_t.py in QEMU: the shell's switches and the windows' disagreed, on for the windows and off for the shell) a switch is
   saved where every window and the shell read it, at once, by the shell: the account's settings in rudolfd's copy of the pages' storage
   (bd95set:ID, as the page's tile saves it: dm, nl, dnd, sr, mag, cb, vk, touch), then the shell and every window told (settings_changed:
   the windows take it from that copy). The page doing the rest (#app=do-qs: the screen reader's words) finds it already so. */
static void qs_save(int i, int on)
{
	static const char *const K[8] = { "dm", "nl", "dnd", "sr", "mag", "cb", "vk", "touch" }; if (i < 0 || i > 7 || !USER_ID[0] || !id_ok(USER_ID)) return;
	char p[160]; snprintf(p, sizeof p, "/api/lsync?key=bd95set%%3A%s", USER_ID); JV *r = jget(p); JV *vv = jprop(r, "v"); JV *set = vv && vv->t == 3 && vv->s[0] == '{' ? jparse(vv->s) : NULL;
	if (!set) { jfree(r); return; }
	JV *x = jprop(set, K[i]); if (!x) { set->k = g_renew(char *, set->k, set->len + 1); set->v = g_renew(JV *, set->v, set->len + 1); set->k[set->len] = g_strdup(K[i]); x = set->v[set->len++] = g_new0(JV, 1); }
	g_free(x->s); x->s = NULL; if (i == 0 || i == 5) { x->t = 3; x->s = g_strdup(i == 0 ? (on ? "dark" : "light") : (on ? "contrast" : "none")); } else { x->t = 1; x->n = on; }
	GString *js = g_string_new(NULL); jv_dump(js, set); GString *b = g_string_new("{\"gen\":0,\"part\":1,\"set\":{"); char k[100]; snprintf(k, sizeof k, "bd95set:%s", USER_ID); json_esc(b, k); g_string_append_c(b, ':'); json_esc(b, js->str); g_string_append(b, "},\"del\":[]}");
	int c = http("POST", "/api/lsync", b->str, b->len, NULL); g_string_free(js, TRUE); g_string_free(b, TRUE); jfree(set); jfree(r);
	if (c == 200) settings_changed(USER_ID, NULL);
}
static void qs_toggle(int i)
{
	if (i < 0 || i > 7) return; int on = !qs_get(i); qs_set(i, on); qs_pending(i, on); qs_save(i, on); char *e = qesc(QSL[i]), ex[160]; snprintf(ex, sizeof ex, "&item=%s&on=%d", e, on); g_free(e); app_open_ex("do-qs", NULL, ex, 1);
	if (i == 2) bar_redraw();
	if (i == 4) { MAG_SAVED = on; if (on && S.magz > 0) MAGZ = S.magz; mag_set(on); }   /* (the magnifier now; its keys only zoom) */
	a11y_apply(); if (WM_OK && (i == 0 || i == 5)) frames_redraw(); state_write();   /* (1.6.0: at once, for the whole screen: its colour, the magnifier, the keyboard, touch mode's sizes) */
}
static void fl_act(const char *k, int dbl)
{
	if (!strcmp(k, "prev")) { if (--FL_M < 1) { FL_M = 12; FL_Y--; } }
	else if (!strcmp(k, "next")) { if (++FL_M > 12) { FL_M = 1; FL_Y++; } }
	else if (!strcmp(k, "open")) { fl_close(); app_open("calendar", NULL); return; }
	else if (k[0] >= '0' && k[0] <= '9') { if (dbl) { char ex[40]; snprintf(ex, sizeof ex, "&day=%s", k); fl_close(); app_open_ex("calendar", NULL, ex, 0); } return; }
	else if (!strcmp(k, "mute")) { char q[16]; snprintf(q, sizeof q, "muted=%d", !FLMUTE); fl_volume_set(q); }
	else if (!strcmp(k, "sounds")) { fl_close(); tb_settings("vol"); return; }
	else if (!strcmp(k, "npo")) { fl_close(); tb_settings("net"); return; }   /* (the network's Network Settings…) */
	else if (!strcmp(k, "qs:8")) { air_set(!AIR_ON); }
	else if (!strcmp(k, "qs:9")) { mob_set(!MOB_ON); }
	else if (!strncmp(k, "qs:", 3)) qs_toggle(atoi(k + 3));
	else if (!strcmp(k, "lock")) { fl_close(); power("lock"); return; }
	else if (!strcmp(k, "settings")) { fl_close(); sc_settings(); return; }   /* (Settings' window, the one open if there is one, as the page's settings()) */
	else if (!strcmp(k, "power")) { fl_close(); power_dialog(); return; }
	else if (!strcmp(k, "clear")) { NT_CLR = 0; for (int i = 0; i < NNT; i++) NT_CLR = fmax(NT_CLR, NT[i].at); NT_CLRT = g_get_monotonic_time(); NNT = 0; NNTP = 0; app_open_ex("do-nc", NULL, "&op=clear", 1); bar_redraw(); }
	else if (!strcmp(k, "dnd")) { S.dnd = !S.dnd; qs_pending(2, S.dnd); qs_save(2, S.dnd); char ex[40]; snprintf(ex, sizeof ex, "&op=dnd&on=%d", S.dnd); app_open_ex("do-nc", NULL, ex, 1); bar_redraw(); }
	else if (!strncmp(k, "nx:", 3)) { int i = atoi(k + 3); if (i < 0 || i >= NNT) return; char ex[40]; snprintf(ex, sizeof ex, "&op=dismiss&i=%d", i);
		if (NNTX == 16) { memmove(&NTX[0], &NTX[1], 15 * sizeof NTX[0]); NNTX--; } NTX[NNTX].at = NT[i].at; snprintf(NTX[NNTX].t, sizeof NTX[0].t, "%s", NT[i].t); NTX[NNTX++].when = g_get_monotonic_time();
		for (int j = 0; j < NNTP; j++) if (NTP[j].at == NT[i].at && !strcmp(NTP[j].t, NT[i].t)) { memmove(&NTP[j], &NTP[j + 1], (NNTP - j - 1) * sizeof NTP[0]); memmove(&NTPT[j], &NTPT[j + 1], (NNTP - j - 1) * sizeof NTPT[0]); NNTP--; break; }
		memmove(&NT[i], &NT[i + 1], (NNT - i - 1) * sizeof NT[0]); NNT--; app_open_ex("do-nc", NULL, ex, 1); bar_redraw(); }
	else if (!strncmp(k, "why:", 4)) { int i = atoi(k + 4); char ex[40]; snprintf(ex, sizeof ex, "&op=why&i=%d", i); fl_close(); app_open_ex("do-nc", NULL, ex, 0); return; }
	if (FLAREA) gtk_widget_queue_draw(FLAREA); state_write();
}
static void fl_resize(void);
static gboolean fl_scroll(GtkWidget *w, GdkEventScroll *e, gpointer d)
{
	(void)w; (void)d; if (FLK != FL_NC) return TRUE; double dy = e->direction == GDK_SCROLL_UP ? -1 : e->direction == GDK_SCROLL_DOWN ? 1 : 0; if (e->direction == GDK_SCROLL_SMOOTH) dy = e->delta_y;
	FLSCROLL += dy * 53; gtk_widget_queue_draw(FLAREA); return TRUE;   /* (Chromium's wheel step at 100 %) */
}
static gboolean fl_press(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)w; (void)d; double x = (e->x + FLWX) / Z, y = (e->y + FLWY) / Z;
	if (x < FLX || x >= FLX + FLW || y < FLY || y >= FLY + FLH) {
		if (e->type != GDK_BUTTON_PRESS) return TRUE;
		/* (a click elsewhere closes it; on the taskbar it goes on: another of the tray's buttons, not this one's again) */
		int kind = FLK; fl_close();
		double rx = e->x_root - bar_x(), ry = e->y_root - bar_y();
		/* (the clock's date is not the clock's own element: there the page's calendar closes and opens again, at this month) */
		if (BAR && rx >= 0 && rx < bar_w() && ry >= 0 && ry < bar_h() && e->button == 1) { int h = hit_at(rx, ry); int date = h >= 0 && !strcmp(HITS[h].kind, "clock") && rx / Z >= CLKD[0] && rx / Z < CLKD[1];
			if (h >= 0 && (date || !((kind == FL_CAL && !strcmp(HITS[h].kind, "clock")) || (kind == FL_VOL && !strcmp(HITS[h].kind, "vol")) || (kind == FL_NET && !strcmp(HITS[h].kind, "net")) || (kind == FL_QS && !strcmp(HITS[h].kind, "qs")) || (kind == FL_NC && !strcmp(HITS[h].kind, "notes")) || (kind == FL_BRI && !strcmp(HITS[h].kind, "bri"))))) bar_click(h); }   /* (at once: the button's release does not come to the taskbar after the grab) */
		return TRUE; }
	int i = fl_at(e->x, e->y); if (e->button != 1 || i < 0) return TRUE;
	if (e->type == GDK_2BUTTON_PRESS) { fl_act(FLBK[i], 1); return TRUE; }
	if (e->type != GDK_BUTTON_PRESS) return TRUE;
	if (!strcmp(FLBK[i], "range")) { FLDRAG = 1; fl_range(e->x); return TRUE; }
	FLPRESS = i; return TRUE;
}
static gboolean fl_release(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)w; (void)d; if (FLDRAG) { FLDRAG = 0; if (FLK == FL_VOL) snd_play("notify"); return TRUE; }   /* (the volume's range changed: osd31.js rg.onchange) */
	int i = fl_at(e->x, e->y), p = FLPRESS; FLPRESS = -1;
	if (i >= 0 && i == p) { char k[16]; snprintf(k, sizeof k, "%s", FLBK[i]);
		/* (2026-11-16) the pop-ups' buttons sound, as Standard's: ◀ ▶, Open Calendar, the tiles, Lock, Settings, Power, Clear all, a ✕, Why,
		   Network Settings; not a day, Mute's or Do not disturb's box, or the Desktop sounds link */
		if (!strcmp(k, "prev") || !strcmp(k, "next") || !strcmp(k, "open") || !strncmp(k, "qs:", 3) || !strcmp(k, "lock") || !strcmp(k, "settings") || !strcmp(k, "power")
			|| !strcmp(k, "clear") || !strncmp(k, "nx:", 3) || !strncmp(k, "why:", 4) || !strcmp(k, "npo")) snd_click();
		fl_act(k, 0); if (FLK) fl_resize(); }
	return TRUE;
}
static gboolean fl_motion(GtkWidget *w, GdkEventMotion *e, gpointer d)
{
	(void)w; (void)d; if (FLDRAG) { fl_range(e->x); return TRUE; }
	int h = fl_at(e->x, e->y); if (h >= 0 && (!strcmp(FLBK[h], "range") || !strncmp(FLBK[h], "qs:", 3) || !strcmp(FLBK[h], "dnd") || !strncmp(FLBK[h], "nx:", 3) || (FLBK[h][0] >= '0' && FLBK[h][0] <= '9'))) h = -1;
	if (h != FLHOV) { FLHOV = h; gtk_widget_queue_draw(FLAREA); }
	GdkWindow *g = gtk_widget_get_window(FLWIN); if (g) { static GdkCursor *ptr; if (!ptr) ptr = gdk_cursor_new_from_name(gdk_display_get_default(), "pointer"); int k = fl_at(e->x, e->y); gdk_window_set_cursor(g, k >= 0 && !strcmp(FLBK[k], "sounds") ? ptr : NULL); }
	return TRUE;
}
/* (its window: the box and its shadow; what is behind copied, there being no compositor) */
static void fl_resize(void)
{
	cairo_surface_t *t = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 4, 4); cairo_t *c = cairo_create(t); fl_layout(c, 1); cairo_destroy(c); cairo_surface_destroy(t);   /* (its size, and its buttons' places) */
	int fk = NMON > 1 ? FL_MON : 0, top = !strcmp(S.tb, "top");   /* (1.6.1 Part 1: opened from another monitor's taskbar: there, by its taskbar) */
	if (fk) { FLX += (MON[fk].x + (tb_side() == 2 ? MON[fk].w - SW : 0)) / Z; FLY += (MON[fk].y + (!top && !tb_side() ? MON[fk].h - SH : 0)) / Z; }
	int x0 = (int)fmax(fk ? MON[fk].x : 0, floor((FLX - 40) * Z)), y0 = (int)fmax(fk ? MON[fk].y : 0, floor((FLY - 40) * Z)), x1 = (int)fmin(fk ? MON[fk].x + MON[fk].w : SW, ceil((FLX + FLW + 40) * Z)), y1 = (int)fmin(fk ? MON[fk].y + MON[fk].h : SH, ceil((FLY + FLH + 60) * Z));
	int same = FLBG && FLWX == x0 && FLWY == y0 && gdk_pixbuf_get_width(FLBG) == x1 - x0 && gdk_pixbuf_get_height(FLBG) == y1 - y0;
	if (!same) { int vis = FLWIN && gtk_widget_get_visible(FLWIN); if (vis) { gtk_widget_hide(FLWIN); pop_gone(FLBG, FLWX, FLWY); }   /* (the copy of what is behind without the box itself) */
		if (vis) { GdkDisplay *dp = gdk_display_get_default(); gdk_display_sync(dp); }
		if (FLBG) g_object_unref(FLBG); FLWX = x0; FLWY = y0; FLBG = screen_copy(x0, y0, x1 - x0, y1 - y0);
		gtk_widget_set_size_request(FLAREA, x1 - x0, y1 - y0); gtk_window_resize(GTK_WINDOW(FLWIN), x1 - x0, y1 - y0); gtk_window_move(GTK_WINDOW(FLWIN), x0, y0);
		if (vis) { gtk_widget_show_all(FLWIN); XRaiseWindow(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), GDK_WINDOW_XID(gtk_widget_get_window(FLWIN))); seat_grab(FLWIN, FALSE); } }
	gtk_widget_queue_draw(FLAREA); state_write();
}
static void fl_open(int kind)
{
	if (!USER_ID[0] || LOCKED) return; int was = FLK; fl_close(); cm_close(); menu_hide(); if (was == kind) return;   /* (its button again: closed) */
	FL_MON = MONK_CUR;   /* (1.6.1 Part 1: the taskbar it was opened from) */
	if (!FLWIN) { FLWIN = gtk_window_new(GTK_WINDOW_POPUP); gtk_window_set_title(GTK_WINDOW(FLWIN), "Pop-up"); FLAREA = gtk_drawing_area_new();
		/* (the pointer grabbed for the window itself, not its owner's other windows: a click anywhere comes here, as the page's pointerdown) */
		gtk_widget_add_events(FLWIN, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK); gtk_container_add(GTK_CONTAINER(FLWIN), FLAREA);
		g_signal_connect(FLWIN, "scroll-event", G_CALLBACK(fl_scroll), NULL);
		g_signal_connect(FLAREA, "draw", G_CALLBACK(fl_draw), NULL); g_signal_connect(FLWIN, "button-press-event", G_CALLBACK(fl_press), NULL);
		g_signal_connect(FLWIN, "button-release-event", G_CALLBACK(fl_release), NULL); g_signal_connect(FLWIN, "motion-notify-event", G_CALLBACK(fl_motion), NULL); }
	if (kind == FL_QS) { air_read(); mob_read(); }   /* (1.6.0 Parts 2 and 3: Airplane mode and Mobile data, as they are now) */
	FLK = kind; FLHOV = FLPRESS = -1; FLSCROLL = 0;
	if (kind == FL_CAL) { GDateTime *n = fl_now(); FL_Y = g_date_time_get_year(n); FL_M = g_date_time_get_month(n); g_date_time_unref(n); }
	else if (kind == FL_NET) net_read();
	else if (kind == FL_BRI) bri_load();
	else { JV *v = jget("/api/volume"); if (v && jbool(v, "ok")) { FLVOL = VOL = (int)jnum(v, "volume", -1); FLMUTE = MUTED = jbool(v, "muted"); } else { FLVOL = VOL; FLMUTE = MUTED; } jfree(v); }
	if (kind == FL_NC) { for (int i = 0; i < NNT; i++) NT_SEEN = fmax(NT_SEEN, NT[i].at); bar_redraw(); }   /* (ncToggle: what is in the centre is seen) */
	if (FLBG) { g_object_unref(FLBG); FLBG = NULL; }
	fl_resize(); gtk_widget_show_all(FLWIN); XRaiseWindow(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), GDK_WINDOW_XID(gtk_widget_get_window(FLWIN)));
	{ int px = 0, py = 0, rx, ry; unsigned int mk; Window r0, c0; if (!DPY || !XQueryPointer(DPY, ROOT, &r0, &c0, &px, &py, &rx, &ry, &mk)) { px = (int)((FLX + FLW / 2) * Z); py = (int)((FLY + FLH / 2) * Z); }
	  lp_menu_open(FLWIN, FLWX, FLWY, LP_POP, FLX * Z, FLY * Z, FLW * Z, FLH * Z, px, py, 0); }   /* (Part 17b: from above the button clicked) */
	seat_grab(FLWIN, FALSE); state_write();
}

/* ---- the X server's Composite extension (libXcomposite, loaded when there): the windows' own pictures, which the server keeps up to
   date while a window is redirected (automatically: it is still drawn on the screen as before). The overview's pictures and what
   is behind a toast come from them. A window is redirected while anything here needs it (counted). */
typedef Window (*XcName)(Display *, Window); typedef void (*XcRedir)(Display *, Window, int);
static XcName XC_NAME; static XcRedir XC_RED, XC_UNRED;
static int xc_ok(void)
{
	static int tried; if (tried) return XC_NAME != NULL; tried = 1;
	void *h = dlopen("libXcomposite.so.1", RTLD_NOW | RTLD_LOCAL); if (h) { XC_NAME = (XcName)dlsym(h, "XCompositeNameWindowPixmap"); XC_RED = (XcRedir)dlsym(h, "XCompositeRedirectWindow"); XC_UNRED = (XcRedir)dlsym(h, "XCompositeUnredirectWindow"); }
	if (!XC_NAME || !XC_RED || !XC_UNRED) { XC_NAME = NULL; fprintf(stderr, "rudolf-native: no Composite (libXcomposite.so.1): the overview shows the windows' frames only\n"); }
	return XC_NAME != NULL;
}
/* (a window and those inside it asked to draw themselves again: a window just redirected starts with a copy of what the screen showed
   there, which may be another window over it) */
static void comp_expose(Window w, int depth)
{
	XClearArea(DPY, w, 0, 0, 0, 0, True); if (depth > 6) return;
	Window r0, p0, *kids = NULL; unsigned int n = 0; if (!XQueryTree(DPY, w, &r0, &p0, &kids, &n)) return;
	for (unsigned int i = 0; i < n; i++) { XWindowAttributes wa; if (XGetWindowAttributes(DPY, kids[i], &wa) && wa.map_state == IsViewable && wa.class == InputOutput) comp_expose(kids[i], depth + 1); }
	if (kids) XFree(kids);
}
static GHashTable *XC_REF;
static void comp_ref(Window w)
{
	if (!xc_ok()) return; if (!XC_REF) XC_REF = g_hash_table_new(NULL, NULL);
	int n = GPOINTER_TO_INT(g_hash_table_lookup(XC_REF, GSIZE_TO_POINTER(w))); if (!n) XC_RED(DPY, w, 0 /* CompositeRedirectAutomatic */);
	g_hash_table_insert(XC_REF, GSIZE_TO_POINTER(w), GINT_TO_POINTER(n + 1));
}
static void comp_unref(Window w)
{
	if (!xc_ok() || !XC_REF) return; int n = GPOINTER_TO_INT(g_hash_table_lookup(XC_REF, GSIZE_TO_POINTER(w))); if (!n) return;
	if (n == 1) { g_hash_table_remove(XC_REF, GSIZE_TO_POINTER(w)); XC_UNRED(DPY, w, 0); comp_expose(w, 0);   /* (back on the screen: drawn again there) */
		for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (c->frame == w && c->app && c->app->win) gtk_widget_queue_draw(c->app->win); } }   /* (an app's page: WebKit's own drawing too) */ else g_hash_table_insert(XC_REF, GSIZE_TO_POINTER(w), GINT_TO_POINTER(n - 1));
}
static Client *client_of_frame(Window f) { for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (c->frame == f) return c; } return NULL; }
/* what is on the screen behind window SELF in the rectangle (RX, RY, RW, RH), as the screen would show it without SELF: each window
   under it (in the X server's stacking order) from its own picture, a frame within its round corners. The windows are redirected
   the first time and added to HELD (unredirected by the caller when it no longer needs them). 0 without Composite. With no CR, the
   windows there are only redirected (before SELF is shown over them). */
static void comp_paint_one(cairo_t *cr, Window w, XWindowAttributes *wa, int rx, int ry)
{
	int x = wa->x + wa->border_width, y = wa->y + wa->border_width; Pixmap pm = XC_NAME(DPY, w); if (!pm) return;
	cairo_surface_t *xs = cairo_xlib_surface_create(DPY, pm, wa->visual, wa->width, wa->height);
	cairo_save(cr); Client *c = client_of_frame(w);
	if (c && !c->max && !c->full) { rrect(cr, x - rx, y - ry, wa->width, wa->height, tw_rad() * Z); cairo_clip(cr); }
	cairo_set_source_surface(cr, xs, x - rx, y - ry); cairo_paint(cr); cairo_restore(cr);
	cairo_surface_destroy(xs); XFreePixmap(DPY, pm);
}
/* ABOVE: the windows over SELF instead: one this has redirected (it was under SELF, and has been raised over it since: the taskbar)
   is given back to the X server, and draws itself again (a redirected window no longer hides what is under it, so SELF was drawn over
   it on the screen) */
static int comp_behind_or_above(cairo_t *cr, Window self, int rx, int ry, int rw, int rh, GPtrArray *held, int above)
{
	if (!xc_ok()) return 0; Window r0, p0, *kids = NULL; unsigned int n = 0; if (!XQueryTree(DPY, ROOT, &r0, &p0, &kids, &n)) return 0;
	if (cr && !above) { cairo_save(cr); cairo_set_source_rgb(cr, 0, 0, 0); cairo_paint(cr); cairo_restore(cr); }
	unsigned int i0 = 0; if (above) { while (i0 < n && kids[i0] != self) i0++; i0++; }
	for (unsigned int i = i0; i < n && (above || kids[i] != self); i++) { XWindowAttributes wa;
		if (above) { for (guint k = 0; k < held->len; k++) if ((Window)GPOINTER_TO_SIZE(g_ptr_array_index(held, k)) == kids[i]) {
				g_ptr_array_remove_index(held, k); comp_unref(kids[i]); comp_expose(kids[i], 0); break; }
			continue; }
		if (!XGetWindowAttributes(DPY, kids[i], &wa) || wa.map_state != IsViewable || wa.class == InputOnly) continue;
		int x = wa.x + wa.border_width, y = wa.y + wa.border_width; if (x >= rx + rw || y >= ry + rh || x + wa.width <= rx || y + wa.height <= ry) continue;
		if (!above) { gboolean had = FALSE; for (guint k = 0; k < held->len; k++) if ((Window)GPOINTER_TO_SIZE(g_ptr_array_index(held, k)) == kids[i]) { had = TRUE; break; }
			if (!had) { comp_ref(kids[i]); g_ptr_array_add(held, GSIZE_TO_POINTER(kids[i])); if (!cr || self) comp_expose(kids[i], 0); } }
		if (cr) comp_paint_one(cr, kids[i], &wa, rx, ry); }
	if (kids) XFree(kids); return 1;
}
static int comp_behind(cairo_t *cr, Window self, int rx, int ry, int rw, int rh, GPtrArray *held) { return comp_behind_or_above(cr, self, rx, ry, rw, rh, held, 0); }
static void comp_release(GPtrArray *held) { for (guint k = 0; k < held->len; k++) comp_unref((Window)GPOINTER_TO_SIZE(g_ptr_array_index(held, k))); g_ptr_array_set_size(held, 0); }

/* ------------------------------------------------------------------ the notifications' pop-ups (1.6.0 Part 12, item 3)
   A page's notification (shell25.js __notifCentre.add) shows as the page's toast (.ntoast, #ntoasts: 320 px of content,
   12 px from the screen's right edge and 56 px from its bottom, or its top with the taskbar there; one above the other,
   8 px apart, the newest at the bottom), sliding in (ntin: .25 s), going after its time (5 s; .35 s out, gone at .4 s).
   App windows' pages send theirs here (oneapp59.js, op "toast") rather than drawing them in their own window. A click
   does what the page's does: the notification's own action in its page, else the notifications' centre; ⚡ Why? asks
   Blitzen (as the centre's ⚡ Why?). Drawn in a window of its own over a copy of what is behind, as the pop-ups. */
typedef struct { char t[200], b[600], k[12], nid[40], sid[80], wl[24]; int why, act, win; gint64 born, ms; double h; } Toast;   /* (sid: Oracle's page in Chromium that sent it, 12.3c) */
static Toast TST[8]; static int NTST; static GtkWidget *TWIN, *TAREA; static GdkPixbuf *TBG; static int TWX, TWY, TWW, TWH; static guint TTICK;
static GPtrArray *THELD; static guint TLIVE;   /* (the windows behind redirected for their pictures; the timer drawing them again) */
static double TSX[8], TSY[8], TWHY[8][4];   /* (each one's place as drawn, the page's units on the screen; its ⚡ Why?) */
/* (a CSS cubic-bezier timing function at T) */
static double bezier(double x1, double y1, double x2, double y2, double t)
{
	double lo = 0, hi = 1, u = t; for (int i = 0; i < 40; i++) { u = (lo + hi) / 2; double x = 3 * (1 - u) * (1 - u) * u * x1 + 3 * (1 - u) * u * u * x2 + u * u * u; if (x < t) lo = u; else hi = u; }
	return 3 * (1 - u) * (1 - u) * u * y1 + 3 * (1 - u) * u * u * y2 + u * u * u;
}
static double ease_out(double t) { return bezier(0, 0, .58, 1, t); }
static double ease_css(double t) { return bezier(.25, .1, .25, 1, t); }
/* a toast's height (and its lines) */
static double toast_lay(cairo_t *cr, const Toast *t, char (*tl)[320], int *ntl, char (*bl)[320], int *nbl)
{
	double l13 = fl_lh(cr, 400, 13), l11 = fl_lh(cr, 400, 11);
	*ntl = css_wrap(cr, t->t, UIFAM, 700, 13, 320, tl, 4); *nbl = t->b[0] ? css_wrap(cr, t->b, UIFAM, 400, 13, 320, bl, 8) : 0;
	return 1 + 10 + *ntl * l13 + 2 + *nbl * l13 /* the body (an empty one still has its 2px margin) */ + (t->why ? fmax(l13, l11 + 3) : 0) + 10 + 1;
}
/* its state now: how far in (0 to 1), how far out (0 to 1); 0 when gone */
static int toast_at(const Toast *t, gint64 now, double *in, double *out)
{
	double e = (now - t->born) / 1000.0; *in = fmin(1, e / 250); *out = e > t->ms ? fmin(1, (e - t->ms) / 350) : 0; return e < t->ms + 400;
}
static void toast_paint(cairo_t *cr)
{
	int dark = menu_dark(), top = !strcmp(S.tb, "top"); double vw = SW / Z, vh = SH / Z, l13 = fl_lh(cr, 400, 13), l11 = fl_lh(cr, 400, 11); gint64 now = g_get_monotonic_time();
	RGBA fg = cx(dark ? "#e2e8f0" : "#111827", 1), mut = cx(dark ? "#94a3b8" : "#6b7280", 1), bd = cx(dark ? "#334155" : "#d1d5db", 1), wbg = cx(dark ? "#0f172a" : "#ffffff", 1);
	double hs[8], y; char tl[8][4][320], bl[8][8][320]; int ntl[8], nbl[8];
	for (int i = 0; i < NTST; i++) hs[i] = TST[i].h = toast_lay(cr, &TST[i], tl[i], &ntl[i], bl[i], &nbl[i]);
	if (top) { y = 56; for (int i = 0; i < NTST; i++) { TSY[i] = y; y += hs[i] + 8; } }
	else { y = vh - 56; for (int i = NTST - 1; i >= 0; i--) { y -= hs[i]; TSY[i] = y; y -= 8; } }
	for (int i = 0; i < NTST; i++) { const Toast *t = &TST[i]; double in, out; toast_at(t, now, &in, &out);
		double a = ease_out(in) * (1 - ease_css(out)), dx = 30 * (1 - ease_out(in)) + 30 * ease_css(out), x = vw - 12 - 349 + dx; TSX[i] = x;
		cairo_push_group(cr);
		Box b; memset(&b, 0, sizeof b); b.bg = wbg; b.bw = 1; b.bc = bd; b.rad = 10; shd_add(&b, 0, 10, 26, cx("#000000", .3), 0); box_paint(cr, x, TSY[i], 349, hs[i], &b);
		/* (the left edge: 4 px, the accent colour, red for an error) */
		{ RGBA lc = !strcmp(t->k, "error") ? cx("#ef4444", 1) : ca(accent(), 1); double yy = TSY[i], h = hs[i]; cairo_save(cr);
			cairo_new_path(cr); cairo_move_to(cr, x - 30, yy - 7.5); cairo_line_to(cr, x + 4, yy + 1); cairo_line_to(cr, x + 4, yy + h - 1); cairo_line_to(cr, x - 30, yy + h + 7.5); cairo_close_path(cr); cairo_clip(cr);
			cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD); nc_rr(cr, x, yy, 349, h, 10, 10, 10, 10); nc_rr(cr, x + 4, yy + 1, 344, h - 2, 6, 9, 9, 9); srca(cr, lc); cairo_fill(cr); cairo_restore(cr); }
		double tx = x + 16, ty = TSY[i] + 11;
		for (int k = 0; k < ntl[i]; k++) css_text(cr, tl[i][k], UIFAM, 700, 13, tx, ty + k * l13, l13, fg, 0); ty += ntl[i] * l13;
		{ ty += 2; for (int k = 0; k < nbl[i]; k++) css_text(cr, bl[i][k], UIFAM, 400, 13, tx, ty + k * l13, l13, mut, 0); ty += nbl[i] * l13; }
		char wlb[48]; snprintf(wlb, sizeof wlb, "⚡︎ %s", t->wl[0] ? t->wl : "Why?");
		if (t->why) { double ww = css_width(cr, wlb, UIFAM, 400, 11) + 14, wx = tx + 6, wy = ty + 1; Box k; memset(&k, 0, sizeof k); k.rad = 4; k.bw = 1;
			if (dark) { k.bg = cx("#1e293b", 1); k.bc = cx("#334155", 1); } else { k.bg = cx("#ffffff", 1); k.bc = cx("#d1d5db", 1); }
			box_paint(cr, wx, wy, ww, l11 + 2, &k); css_text(cr, wlb, UIFAM, 400, 11, wx + 7, wy + 1, l11, cx(dark ? "#e2e8f0" : "#000000", 1), 0);
			TWHY[i][0] = wx; TWHY[i][1] = wy; TWHY[i][2] = ww; TWHY[i][3] = l11 + 2; }
		cairo_pop_group_to_source(cr); cairo_paint_with_alpha(cr, a); }
}
static gboolean toast_draw(GtkWidget *wd, cairo_t *cr, gpointer d)
{
	(void)d;
	/* (what is behind the toasts and their shadows as it is now, from the windows' own pictures (Composite): a video, a window moved
	   under them; the copy taken when they came only without Composite) */
	if (!THELD) THELD = g_ptr_array_new();
	GdkWindow *gw = gtk_widget_get_window(gtk_widget_get_toplevel(wd)); int live = gw && comp_behind(cr, GDK_WINDOW_XID(gw), TWX, TWY, TWW, TWH, THELD);
	if (!live && TBG) { gdk_cairo_set_source_pixbuf(cr, TBG, 0, 0); cairo_paint(cr); }
	cairo_save(cr); cairo_translate(cr, -TWX, -TWY); cairo_scale(cr, Z, Z); toast_paint(cr); cairo_restore(cr);
	if (live) comp_behind_or_above(NULL, GDK_WINDOW_XID(gw), TWX, TWY, TWW, TWH, THELD, 1);
	return TRUE;
}
static gboolean toast_live(gpointer d) { (void)d; if (!NTST || !TWIN || !gtk_widget_get_visible(TWIN)) { TLIVE = 0; return G_SOURCE_REMOVE; } gtk_widget_queue_draw(TAREA); return G_SOURCE_CONTINUE; }
static void toast_place(void);
static gboolean toast_tick(gpointer d)
{
	gint64 now = g_get_monotonic_time(); int n = 0, moving = 0;
	for (int i = 0; i < NTST; i++) { double in, out; if (toast_at(&TST[i], now, &in, &out)) { TST[n++] = TST[i]; if (in < 1 || out > 0) moving = 1; } }
	int gone = n != NTST; NTST = n;
	if (gone) toast_place(); else if (TAREA) gtk_widget_queue_draw(TAREA);
	if (!NTST) { TTICK = 0; return G_SOURCE_REMOVE; }
	if (!moving) { /* (still: wake for the next one going) */ gint64 next = G_MAXINT64; for (int i = 0; i < NTST; i++) next = MIN(next, TST[i].born + TST[i].ms * 1000); TTICK = g_timeout_add(MAX(16, (int)((next - now) / 1000)), toast_tick, GINT_TO_POINTER(1)); return G_SOURCE_REMOVE; }
	if (d) { TTICK = g_timeout_add(16, toast_tick, NULL); return G_SOURCE_REMOVE; }   /* (woken to go: frames again) */
	return G_SOURCE_CONTINUE;
}
static void toast_anim(void) { if (TTICK) g_source_remove(TTICK); TTICK = g_timeout_add(16, toast_tick, NULL); }
/* its window: over the toasts and their shadows (what is behind copied without it) */
static void toast_place(void)
{
	if (!NTST) { if (TWIN) gtk_widget_hide(TWIN); if (TBG) { g_object_unref(TBG); TBG = NULL; } if (THELD) comp_release(THELD); if (TLIVE) { g_source_remove(TLIVE); TLIVE = 0; } state_write(); return; }
	if (!TWIN) { TWIN = gtk_window_new(GTK_WINDOW_POPUP); gtk_window_set_title(GTK_WINDOW(TWIN), "Notifications"); TAREA = gtk_drawing_area_new(); gtk_container_add(GTK_CONTAINER(TWIN), TAREA);
		gtk_widget_add_events(TWIN, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK); g_signal_connect(TAREA, "draw", G_CALLBACK(toast_draw), NULL); }
	cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 4, 4); cairo_t *c = cairo_create(s); toast_paint(c); cairo_destroy(c); cairo_surface_destroy(s);   /* (the heights and places) */
	double y0 = 1e9, y1 = -1e9, vw = SW / Z; for (int i = 0; i < NTST; i++) { y0 = fmin(y0, TSY[i]); y1 = fmax(y1, TSY[i] + TST[i].h); }
	int x0 = (int)fmax(0, floor((vw - 12 - 349 - 30) * Z)), x1 = SW, ya = (int)fmax(0, floor((y0 - 20) * Z)), yb = (int)fmin(SH, ceil((y1 + 40) * Z));
	int vis = gtk_widget_get_visible(TWIN), xc = xc_ok(); if (vis && !xc) { gtk_widget_hide(TWIN); gdk_display_sync(gdk_display_get_default()); }
	if (TBG) g_object_unref(TBG); TBG = NULL; TWX = x0; TWY = ya; TWW = x1 - x0; TWH = yb - ya; if (!xc || !vis) TBG = screen_copy(x0, ya, TWW, TWH);
	if (xc && !vis) { if (!THELD) THELD = g_ptr_array_new(); comp_behind(NULL, 0, x0, ya, TWW, TWH, THELD); }   /* (redirected before the toasts cover them) */
	gtk_widget_set_size_request(TAREA, TWW, TWH); gtk_window_resize(GTK_WINDOW(TWIN), TWW, TWH); gtk_window_move(GTK_WINDOW(TWIN), x0, ya);
	gtk_widget_show_all(TWIN); XRaiseWindow(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), GDK_WINDOW_XID(gtk_widget_get_window(TWIN)));
	/* (and there at once: shown already, GTK could leave it where it was while resizing it (from two toasts to one), and what is behind
	   would be drawn 15 px off) */
	gdk_window_move_resize(gtk_widget_get_window(TWIN), x0, ya, TWW, TWH);
	/* (only the toasts take clicks, as #ntoasts's pointer-events: none: their shadows and what is between them let them through) */
	{ cairo_region_t *rg = cairo_region_create(); for (int i = 0; i < NTST; i++) { cairo_rectangle_int_t r = { (int)floor((vw - 12 - 349) * Z) - x0, (int)floor(TSY[i] * Z) - ya, (int)ceil(349 * Z), (int)ceil(TST[i].h * Z) }; cairo_region_union_rectangle(rg, &r); }
		gdk_window_input_shape_combine_region(gtk_widget_get_window(TWIN), rg, 0, 0); cairo_region_destroy(rg); }
	if (xc && !TLIVE) TLIVE = g_timeout_add(50, toast_live, NULL);   /* (what is behind drawn again 20 times a second) */
	gtk_widget_queue_draw(TAREA); state_write();
}
/* ------------------------------------------------------------------ the window transitions (1.6.0, docs/plan-1.6.0.md Part 17)
   "Splitting-Merging LitePoint", the owner's design, as the page draws it (desktop/litepoint60.js): the same scenes (litepoint.h) at
   the same times (open 200 ms corner to corner, 220 ms side to side; maximize 180, minimize 200, restore 180, close 200). Without a
   compositor: the lines and the points of light are small windows of the shell's own (override-redirect, taking no clicks), a line
   a strip of one colour, a point a small disc drawn in it; the part of a window shown is its frame's X shape (frame_shape: lpclip);
   maximizing and restoring resize the window once (the frame drawn around where it will be first). Closing draws the frame where
   the window was (the window has gone by then). The light's brightness (Display Properties) dims the lines and makes the points
   smaller (no see-through without a compositor); at 0 there is no light, and the windows still fill, clear and drop on time.
   None with Reduce motion, on the lock screen, for a full-screen window and the shell's own unseen pages (do-*). This replaces
   test finding N15's fade (a window of the shell's over the new one, drawn from the windows' pictures). */
typedef struct { int on, kind, back, sw, max_on; Client *c; gint64 t0; LPGeo g; GtkWidget *pw; int wx, wy, pclip; LPRect pcr; } LPRun;   /* (sw: its new size given; max_on: maximized after; pw: a menu of the shell's own, its window at wx wy: Part 17b) */
#define LP_RUNS 8
#define LP_PL (LP_NL * 2)
#define LP_PP (LP_NP * 2)
static LPRun LPR[LP_RUNS]; static guint LP_TICK; static int LP_DONE; static LPRun *LP_LAST;
static Window LPLW[LP_PL], LPPW[LP_PP]; static GdkWindow *LPLG[LP_PL], *LPPG[LP_PP]; static int LPLM[LP_PL], LPPM[LP_PP];
static void wm_maximise_now(Client *c, int on);
static int lp_off(Client *c) { return !DPY || S.rmotion || LOCKED || (c && (c->full || c->hidden || c->off || (c->app && !strncmp(c->app->k, "do-", 3)))); }
static RGB lp_mix(RGB a, RGB b, double t) { RGB o = { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t }; return o; }
/* white, or the accent colour (Normal, Negative, Complementary) lifted to glow on a dark desktop; the line's core toward white */
static void lp_cols(RGB *glow, RGB *core)
{
	if (S.lpac) { RGB v = variant(accent(), S.lpacm[0] ? S.lpacm : "normal"); double h, sa, l; rgb2hsl(v, &h, &sa, &l); *glow = hsl2rgb(h, sa, fmax(l, 58));
		RGB w = { 1, 1, 1 }; *core = lp_mix(*glow, w, .45); }
	else { *glow = hexc("#cde4ff", "#cde4ff"); *core = hexc("#f2f7ff", "#f2f7ff"); }
}
static double lp_br(void) { return fmax(0, fmin(1, S.lpbr / 100.0)); }
static unsigned long lp_px(RGB c) { return ((unsigned long)lround(fmin(1, fmax(0, c.r)) * 255) << 16) | ((unsigned long)lround(fmin(1, fmax(0, c.g)) * 255) << 8) | (unsigned long)lround(fmin(1, fmax(0, c.b)) * 255); }
static Window lp_win(GdkWindow **gw)
{
	XSetWindowAttributes at; memset(&at, 0, sizeof at); at.override_redirect = True; at.background_pixel = 0xffffff; at.save_under = True;
	Window w = XCreateWindow(DPY, ROOT, -10, -10, 1, 1, 0, CopyFromParent, InputOutput, CopyFromParent, CWOverrideRedirect | CWBackPixel | CWSaveUnder, &at);
	GdkWindow *g = gdk_x11_window_foreign_new_for_display(gdk_display_get_default(), w);
	if (g) { cairo_region_t *e = cairo_region_create(); gdk_window_input_shape_combine_region(g, e, 0, 0); cairo_region_destroy(e); }   /* (clicks go through) */
	*gw = g; return w;
}
static void lp_unmap(Window *w, int *m, int n) { for (int i = 0; i < n; i++) if (m[i]) { XUnmapWindow(DPY, w[i]); m[i] = 0; } }
static void lp_line_put(int i, const double *L, RGB col, double br)
{
	double a = L[4] * br, dx = L[2] - L[0], dy = L[3] - L[1]; if (a <= .02 || hypot(dx, dy) < .5) { if (LPLM[i]) { XUnmapWindow(DPY, LPLW[i]); LPLM[i] = 0; } return; }
	if (!LPLW[i]) LPLW[i] = lp_win(&LPLG[i]);
	RGB dark = hexc("#101826", "#101826"); RGB c = lp_mix(dark, col, fmin(1, .25 + .75 * a)); double lw = fmax(2, round(1.6 * Z));
	int x0 = (int)floor(fmin(L[0], L[2]) - lw / 2), y0 = (int)floor(fmin(L[1], L[3]) - lw / 2), x1 = (int)ceil(fmax(L[0], L[2]) + lw / 2), y1 = (int)ceil(fmax(L[1], L[3]) + lw / 2);
	int w = MAX(1, x1 - x0), h = MAX(1, y1 - y0);
	XSetWindowBackground(DPY, LPLW[i], lp_px(c)); XMoveResizeWindow(DPY, LPLW[i], x0, y0, w, h);
	if (LPLG[i]) { if (fabs(dx) >= 1 && fabs(dy) >= 1) {   /* (a slanting line: the drop into the taskbar) */
			cairo_region_t *rg = cairo_region_create();
			if (fabs(dy) >= fabs(dx)) for (int y = 0; y < h; y++) { double yy = y0 + y + .5, x = L[0] + (yy - L[1]) * dx / dy - x0; cairo_rectangle_int_t r = { (int)floor(x - lw / 2), y, (int)ceil(lw), 1 }; cairo_region_union_rectangle(rg, &r); }
			else for (int x = 0; x < w; x++) { double xx = x0 + x + .5, y = L[1] + (xx - L[0]) * dy / dx - y0; cairo_rectangle_int_t r = { x, (int)floor(y - lw / 2), 1, (int)ceil(lw) }; cairo_region_union_rectangle(rg, &r); }
			gdk_window_shape_combine_region(LPLG[i], rg, 0, 0); cairo_region_destroy(rg); }
		else gdk_window_shape_combine_region(LPLG[i], NULL, 0, 0); }
	XMapRaised(DPY, LPLW[i]); XClearWindow(DPY, LPLW[i]); LPLM[i] = 1;
}
static void lp_point_put(int i, const double *P, RGB glow, double br)
{
	double k = P[2], red = P[3]; if (k <= 0 || br <= 0) { if (LPPM[i]) { XUnmapWindow(DPY, LPPW[i]); LPPM[i] = 0; } return; }
	if (!LPPW[i]) LPPW[i] = lp_win(&LPPG[i]);
	double R = fmax(2.5, 13 * k * .42 * Z * (.35 + .65 * br)); int d = (int)ceil(2 * R) + 1, x = (int)lround(P[0] - d / 2.0), y = (int)lround(P[1] - d / 2.0);
	RGB g = glow; if (red > 0) { g.r = g.r + (1 - g.r) * red; g.g = g.g + (70 / 255.0 - g.g) * red; g.b = g.b + (80 / 255.0 - g.b) * red; }
	RGB dark = hexc("#101826", "#101826"); g = lp_mix(dark, g, .4 + .6 * br);
	XSetWindowBackground(DPY, LPPW[i], lp_px(g)); XMoveResizeWindow(DPY, LPPW[i], x, y, d, d);
	if (LPPG[i]) { cairo_region_t *rg = cairo_region_create(); double r = d / 2.0;
		for (int yy = 0; yy < d; yy++) { double dy = yy + .5 - r, in = r - sqrt(fmax(0, r * r - dy * dy)); int a = (int)floor(in + .5); cairo_rectangle_int_t rc = { a, yy, d - 2 * a, 1 }; if (rc.width > 0) cairo_region_union_rectangle(rg, &rc); }
		gdk_window_shape_combine_region(LPPG[i], rg, 0, 0); cairo_region_destroy(rg); }
	XMapRaised(DPY, LPPW[i]); LPPM[i] = 1;
	cairo_surface_t *sf = cairo_xlib_surface_create(DPY, LPPW[i], DefaultVisual(DPY, DefaultScreen(DPY)), d, d); cairo_t *cr = cairo_create(sf);
	cairo_pattern_t *pt = cairo_pattern_create_radial(d / 2.0, d / 2.0, 0, d / 2.0, d / 2.0, d / 2.0);
	cairo_pattern_add_color_stop_rgb(pt, 0, 1, 1, 1); cairo_pattern_add_color_stop_rgb(pt, .3, 1, 1, 1); cairo_pattern_add_color_stop_rgb(pt, .55, g.r, g.g, g.b);
	{ RGB e = lp_mix(dark, g, .7); cairo_pattern_add_color_stop_rgb(pt, 1, e.r, e.g, e.b); }
	cairo_set_source(cr, pt); cairo_paint(cr); cairo_pattern_destroy(pt); cairo_destroy(cr); cairo_surface_destroy(sf);
}
/* (its new size, once: maximizing, as the frame starts to grow; restoring, at the end) */
static void lp_switch(LPRun *R)
{
	Client *c = R->c; if (!c || R->sw) return; R->sw = 1;
	if (R->kind == LP_MAX) { c->lpclip = 1; c->lpcr = R->g.f; }   /* (shown as it was until the scene says otherwise: no flash at its new size) */
	if (R->kind == LP_MAX || R->kind == LP_RES) { if (c->max != R->max_on) wm_maximise_now(c, R->max_on); }
}
/* (Part 17b) a menu of the shell's own (the start menu, a pop-up, a right-click menu): only the scene's part of its box drawn (its window
   draws what was behind it everywhere else, as it does around its shadow; not X's shape, which would let clicks through it) */
static void lp_pw_shape(LPRun *R, const LPScene *sc)
{
	if (!R->pw) return; int was = R->pclip; R->pclip = sc && sc->clip; if (R->pclip) R->pcr = sc->cr;
	if (R->pclip || was) gtk_widget_queue_draw(R->pw);
}
static void lp_draw_clip(cairo_t *cr, GtkWidget *pw, int wx, int wy)
{
	for (int r = 0; r < LP_RUNS; r++) if (LPR[r].on && LPR[r].pw == pw && LPR[r].pclip) { const LPRect *c = &LPR[r].pcr;
		cairo_rectangle(cr, c->x - wx, c->y - wy, c->w < .5 || c->h < .5 ? 0 : c->w, c->w < .5 || c->h < .5 ? 0 : c->h); cairo_clip(cr); return; }
}
static void lp_finish(LPRun *R)
{
	if (!R->on) return; if (R->pw) { lp_pw_shape(R, NULL); R->pw = NULL; } R->on = 0; LP_DONE++; Client *c = R->c; if (!c || !g_list_find(CLIENTS, c)) return;
	lp_switch(R); c->lpclip = 0;
	if (R->kind == LP_MIN && !R->back && c->min) XUnmapWindow(DPY, c->frame);   /* (minimized: gone once it has dropped into its button) */
	frame_shape(c); state_write();
}
static void lp_state(GString *s)
{
	Window wo = 0; for (int r = 0; r < LP_RUNS; r++) if (LPR[r].on && LPR[r].c && (LPR[r].kind == LP_C2C || LPR[r].kind == LP_S2S)) wo = LPR[r].c->w;
	g_string_append_printf(s, "wopen %lx\n", (unsigned long)wo);
	gint64 now = g_get_monotonic_time();
	for (int r = 0; r < LP_RUNS; r++) if (LPR[r].on) { double t = fmin(1, (now - LPR[r].t0) / (LP_MS[LPR[r].kind] * 1000.0));
		g_string_append_printf(s, "lp %s%s %lx %.2f\n", LP_NAME[LPR[r].kind], LPR[r].back ? "<" : "", LPR[r].c ? (unsigned long)LPR[r].c->w : 0UL, LPR[r].back ? 1 - t : t); }
	g_string_append_printf(s, "lpdone %d\nlplit %lx %.2f\n", LP_DONE, (unsigned long)LP_LITW, LP_LITA);
}
static gboolean lp_tick(gpointer d)
{
	(void)d; gint64 now = g_get_monotonic_time(); RGB glow, core; lp_cols(&glow, &core); LP_CORE = core; double br = lp_br();
	static double L[LP_PL][5], P[LP_PP][4]; int nl = 0, np = 0, any = 0; Window litw = 0; double lita = 0;
	for (int r = 0; r < LP_RUNS; r++) { LPRun *R = &LPR[r]; if (!R->on) continue;
		if (R->c && !g_list_find(CLIENTS, R->c)) { R->on = 0; continue; }
		if (R->pw && !gtk_widget_get_visible(R->pw)) { lp_finish(R); continue; }   /* (its menu closed meanwhile) */
		double t = (now - R->t0) / (LP_MS[R->kind] * 1000.0); t = t < 0 ? 0 : t > 1 ? 1 : t; if (R->back) t = 1 - t;
		LPScene sc; lp_scene(R->kind, t, &R->g, &sc);
		if (R->c && sc.geo_to && !R->sw) lp_switch(R);
		if (R->back ? t <= 0 : t >= 1) { lp_finish(R); continue; }
		any = 1;
		if (R->c) { Client *c = R->c; int was = c->lpclip; c->lpclip = sc.clip; if (sc.clip) c->lpcr = sc.cr; if (sc.clip || was) frame_shape(c); }
		if (R->pw) lp_pw_shape(R, &sc);
		for (int i = 0; i < sc.nl && nl < LP_PL; i++) memcpy(L[nl++], sc.L[i], sizeof L[0]);
		for (int i = 0; i < sc.np && np < LP_PP; i++) memcpy(P[np++], sc.P[i], sizeof P[0]);
		if (R->c && sc.lit > lita) { lita = sc.lit; litw = R->c->w; } }
	for (int i = 0; i < LP_PL; i++) { if (i < nl) lp_line_put(i, L[i], core, br); else if (LPLM[i]) { XUnmapWindow(DPY, LPLW[i]); LPLM[i] = 0; } }
	for (int i = 0; i < LP_PP; i++) { if (i < np) lp_point_put(i, P[i], glow, br); else if (LPPM[i]) { XUnmapWindow(DPY, LPPW[i]); LPPM[i] = 0; } }
	lita *= br; if (litw != LP_LITW || fabs(lita - LP_LITA) > .01) { LP_LITW = litw; LP_LITA = lita; bar_redraw(); }
	XFlush(DPY);
	if (!any) { LP_TICK = 0; lp_unmap(LPLW, LPLM, LP_PL); lp_unmap(LPPW, LPPM, LP_PP); if (LP_LITW) { LP_LITW = 0; LP_LITA = 0; bar_redraw(); } state_write(); return G_SOURCE_REMOVE; }
	return G_SOURCE_CONTINUE;
}
/* (a window's own transition still running when another starts: the old one ends at once, as it would have) */
static void lp_stop(Client *c) { for (int r = 0; r < LP_RUNS; r++) if (LPR[r].on && LPR[r].c == c) lp_finish(&LPR[r]); }
static int lp_start(Client *c, int kind, int back, const LPGeo *g, int max_on)
{
	if (c) lp_stop(c);
	LPRun *R = NULL; for (int r = 0; r < LP_RUNS; r++) if (!LPR[r].on) { R = &LPR[r]; break; } if (!R) return 0;
	memset(R, 0, sizeof *R); R->on = 1; R->kind = kind; R->back = back; R->c = c; R->g = *g; R->max_on = max_on; R->t0 = g_get_monotonic_time(); LP_LAST = R;
	if (c) { LPScene sc; lp_scene(kind, back ? 1 : 0, g, &sc); c->lpclip = sc.clip; c->lpcr = sc.cr; frame_shape(c); }   /* (its first moment at once) */
	if (!LP_TICK) LP_TICK = g_timeout_add(16, lp_tick, NULL);
	state_write(); return 1;
}
static int lp_viewable(Client *c, LPRect *r)
{
	XWindowAttributes wa; if (!XGetWindowAttributes(DPY, c->frame, &wa) || wa.map_state != IsViewable) return 0;
	r->x = wa.x; r->y = wa.y; r->w = wa.width; r->h = wa.height; return 1;
}
/* the middle of a window's taskbar button, on the screen (a point drops into it) */
static void lp_button(Client *c, const LPRect *f, double *x, double *y)
{
	char wid[24]; snprintf(wid, sizeof wid, "%lx", (unsigned long)c->w);
	for (int i = 0; i < NHITS; i++) if (!strcmp(HITS[i].kind, "win") && !strcmp(HITS[i].arg, wid)) { *x = bar_x() + HITS[i].x + HITS[i].w / 2.0; *y = bar_y() + HITS[i].y + HITS[i].h / 2.0; return; }
	*x = f->x + f->w / 2; *y = bar_y() + BARH / 2.0;
}
/* a window opening: corner to corner, or side to side when another window opened it (a dialog: transient; an app window's page asked) */
static void wopen_start(Client *c)
{
	if (!c || c->opened) return; c->opened = 1;
	LPGeo g; memset(&g, 0, sizeof g); if (lp_off(c) || c->min || !lp_viewable(c, &g.f)) return;
	lp_start(c, c->trans || (c->app && c->app->s2s) ? LP_S2S : LP_C2C, 0, &g, 0);
}
/* minimizing: the window stays mapped (minimized for everything else at once) until the point has dropped into its button */
static int lp_min(Client *c)
{
	LPGeo g; memset(&g, 0, sizeof g); if (lp_off(c) || !lp_viewable(c, &g.f)) return 0;
	lp_button(c, &g.f, &g.bx, &g.by); g.dir = lp_dir(); return lp_start(c, LP_MIN, 0, &g, 0);
}
static void lp_unmin(Client *c)
{
	LPGeo g; memset(&g, 0, sizeof g); if (lp_off(c)) return;
	g.f.x = c->x; g.f.y = c->y; g.f.w = fw_of(c); g.f.h = fh_of(c); lp_button(c, &g.f, &g.bx, &g.by); g.dir = lp_dir(); lp_start(c, LP_MIN, 1, &g, 0);
}
/* maximizing or restoring: the frame grows (or gathers) first; the window's size changes once */
static int lp_maxi(Client *c, int on)
{
	LPGeo g; memset(&g, 0, sizeof g); LPRect now; if (lp_off(c) || c->min || !lp_viewable(c, &now)) return 0;
	lp_stop(c); if (on == c->max) return 1;
	Client t = *c; if (on) max_geom(&t); else { t.x = c->sx; t.y = c->sy; t.cw = c->scw; t.ch = c->sch; }
	LPRect nw = { t.x, t.y, fw_of(&t), fh_of(&t) };
	if (on) { g.f = now; g.s = nw; } else { g.f = nw; g.s = now; }
	return lp_start(c, on ? LP_MAX : LP_RES, 0, &g, on);
}
/* closing (the window going for good): its frame drawn where it was, folding away; the window itself has gone */
static void lp_gone(Client *c)
{
	for (int r = 0; r < LP_RUNS; r++) if (LPR[r].on && LPR[r].c == c) { LPR[r].on = 0; LP_DONE++; }   /* (no more of it: it is going) */
	LPGeo g; memset(&g, 0, sizeof g); if (lp_off(c) || c->min || !c->opened || !lp_viewable(c, &g.f)) return;
	lp_start(NULL, LP_CLS, 0, &g, 0);
}
/* ---- (Part 17b, the owner, 2026-11-18) the start menu, the taskbar's pop-ups, right-click menus and its buttons, where they are,
   the light moving away from the taskbar's side: "upward-moving direction if the taskbar is on the bottom, downward-moving if the
   taskbar is on the top, etc." */
static int lp_dir(void) { return tb_side() == 1 ? 2 : tb_side() == 2 ? 3 : !strcmp(S.tb, "top") ? 1 : 0; }
/* the middle of a taskbar button (a hit: start, win ID), on the screen */
static int lp_hit_mid(const char *kind, const char *arg, double *x, double *y, LPRect *r)
{
	for (int i = 0; i < NHITS; i++) if (!strcmp(HITS[i].kind, kind) && (!arg || !strcmp(HITS[i].arg, arg))) {
		double hx = bar_x() + HITS[i].x, hy = bar_y() + HITS[i].y; if (x) *x = hx + HITS[i].w / 2.0; if (y) *y = hy + HITS[i].h / 2.0;
		if (r) { r->x = hx; r->y = hy; r->w = HITS[i].w; r->h = HITS[i].h; } return 1; }
	return 0;
}
/* a menu of the shell's opening (KIND smo, pop or ctx): its box X Y W H on the screen, its window PW at WX WY, the button or the pointer */
static void lp_menu_open(GtkWidget *pw, int wx, int wy, int kind, double x, double y, double w, double h, double bx, double by, int corner)
{
	if (lp_off(NULL) || !pw || w < 2 || h < 2) return;
	for (int r = 0; r < LP_RUNS; r++) if (LPR[r].on && LPR[r].pw == pw) lp_finish(&LPR[r]);
	LPGeo g; memset(&g, 0, sizeof g); g.f.x = x; g.f.y = y; g.f.w = w; g.f.h = h; g.bx = bx; g.by = by; g.dir = lp_dir(); g.corner = corner;
	if (!lp_start(NULL, kind, 0, &g, 0) || !LP_LAST) return;
	LP_LAST->pw = pw; LP_LAST->wx = wx; LP_LAST->wy = wy; { LPScene sc; lp_scene(kind, 0, &g, &sc); lp_pw_shape(LP_LAST, &sc); }
}
/* the start menu closing (gone at once): its frame folds back into the start button */
static void lp_menu_closed(GtkWidget *pw, double x, double y, double w, double h)
{
	for (int r = 0; r < LP_RUNS; r++) if (LPR[r].on && LPR[r].pw == pw) lp_finish(&LPR[r]);
	if (lp_off(NULL) || w < 2 || h < 2) return;
	LPGeo g; memset(&g, 0, sizeof g); g.f.x = x; g.f.y = y; g.f.w = w; g.f.h = h; g.dir = lp_dir();
	if (!lp_hit_mid("start", NULL, &g.bx, &g.by, NULL)) { g.bx = x; g.by = y + h; }
	lp_start(NULL, LP_SMC, 0, &g, 0);
}
/* a window's taskbar button, clicked to switch to it: its outline traced once */
static void lp_tbb(Client *c)
{
	if (lp_off(NULL) || !c) return; char wid[24]; snprintf(wid, sizeof wid, "%lx", (unsigned long)c->w);
	LPGeo g; memset(&g, 0, sizeof g); if (!lp_hit_mid("win", wid, NULL, NULL, &g.f)) return; g.dir = lp_dir(); lp_start(NULL, LP_TBB, 0, &g, 0);
}
/* ------------------------------------------------------------------ the keys' indicator (1.5.9, 12.3c)
   osd31.js's: the volume, the microphone or the brightness, at the top of the screen for 1.5 s (#osd: 28 px down, centred,
   at least 230 px wide, 10 px by 16 px inside, 12 px corners, rgba(17,24,39,.92); its picture, a bar and the words). On the native
   desktop the app windows' pages and Oracle's page send it here (op "osd") instead of drawing it inside their window. */
static GtkWidget *OWIN, *OAREA; static GdkPixbuf *OBG; static guint OTIMER; static char OK_[16], OLV[8], OLB[256]; static int ON_, OX, OY, OW, OH;
static void osd_icon(cairo_t *cr, double x, double y)
{
	cairo_save(cr); cairo_translate(cr, x, y); cairo_scale(cr, 22 / 24.0, 22 / 24.0); cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND); cairo_set_line_width(cr, 2);
	if (!strcmp(OK_, "brightness")) { int on = (int)ceil((ON_ > 0 ? ON_ : 1) / 12.5); if (on < 1) on = 1;
		for (int i = 0; i < 8; i++) { double a = i * G_PI / 4; srca(cr, cx(i < on ? "#fbbf24" : "#6b7280", 1)); cairo_move_to(cr, 12 + cos(a) * 7.5, 12 + sin(a) * 7.5); cairo_line_to(cr, 12 + cos(a) * 10.5, 12 + sin(a) * 10.5); cairo_stroke(cr); }
		srca(cr, cx("#fbbf24", 1)); cairo_arc(cr, 12, 12, 4.5, 0, 2 * G_PI); cairo_fill(cr); }
	else if (!strcmp(OK_, "mic")) { srca(cr, cx("#e5e7eb", 1)); nc_rr(cr, 9, 3, 6, 11, 3, 3, 3, 3); cairo_fill(cr);
		cairo_arc_negative(cr, 12, 11, 6, G_PI, 0); cairo_stroke(cr); cairo_move_to(cr, 12, 17); cairo_line_to(cr, 12, 21); cairo_stroke(cr);
		if (ON_) { cairo_set_line_width(cr, 2.2); srca(cr, cx("#ef4444", 1)); cairo_move_to(cr, 4, 4); cairo_line_to(cr, 20, 20); cairo_stroke(cr); } }
	else { srca(cr, cx("#e5e7eb", 1)); cairo_move_to(cr, 3, 9); cairo_line_to(cr, 7, 9); cairo_line_to(cr, 12, 5); cairo_line_to(cr, 12, 19); cairo_line_to(cr, 7, 15); cairo_line_to(cr, 3, 15); cairo_close_path(cr); cairo_fill(cr);
		if (ON_ > 0) { srca(cr, cx("#3b82f6", 1)); const double r[3] = { 3.6, 7, 10.5 }; for (int i = 0; i < ON_ && i < 3; i++) { cairo_arc(cr, 12, 12, r[i], -G_PI / 4.8, G_PI / 4.8); cairo_stroke(cr); } }
		else { srca(cr, cx("#9ca3af", 1)); cairo_move_to(cr, 16, 9); cairo_line_to(cr, 21, 15); cairo_move_to(cr, 21, 9); cairo_line_to(cr, 16, 15); cairo_stroke(cr); } }
	cairo_restore(cr);
}
/* (its size in the page's units: the words' width decides it when there is no bar) */
static void osd_size(cairo_t *cr, double *w, double *h)
{
	double tw = css_width(cr, OLB, UIFAM, 600, 13), lh = 0, d; lh = css_ascent(cr, UIFAM, 600, 13, &d) + d;
	*h = 20 + fmax(22, lh); *w = OLV[0] ? 230 : fmax(230, 32 + 22 + 10 + tw);
}
static void osd_paint(cairo_t *cr)
{
	double w, h; osd_size(cr, &w, &h);
	srca(cr, cx("#111827", .92)); nc_rr(cr, 0, 0, w, h, 12, 12, 12, 12); cairo_fill(cr);
	osd_icon(cr, 16, (h - 22) / 2);
	double tx = 16 + 22 + 10, lw = OLV[0] ? fmax(52, css_width(cr, OLB, UIFAM, 600, 13)) : css_width(cr, OLB, UIFAM, 600, 13);
	if (OLV[0]) { double bx = tx, bw = w - 16 - lw - 10 - bx, lv = CLAMP(atof(OLV), 0, 100);
		srca(cr, cx("#374151", 1)); nc_rr(cr, bx, h / 2 - 3, bw, 6, 3, 3, 3, 3); cairo_fill(cr);
		cairo_save(cr); nc_rr(cr, bx, h / 2 - 3, bw, 6, 3, 3, 3, 3); cairo_clip(cr); srca(cr, cx(!strcmp(OK_, "brightness") ? "#fbbf24" : "#3b82f6", 1)); cairo_rectangle(cr, bx, h / 2 - 3, bw * lv / 100, 6); cairo_fill(cr); cairo_restore(cr);
		tx = w - 16 - lw; }
	double tw = css_width(cr, OLB, UIFAM, 600, 13); css_text(cr, OLB, UIFAM, 600, 13, OLV[0] ? w - 16 - tw : tx, (h - 22) / 2, 22, cx("#f9fafb", 1), 0);
}
static gboolean osd_draw(GtkWidget *wd, cairo_t *cr, gpointer d)
{
	(void)wd; (void)d; if (OBG) { gdk_cairo_set_source_pixbuf(cr, OBG, 0, 0); cairo_paint(cr); }
	cairo_scale(cr, Z, Z); osd_paint(cr); return TRUE;
}
static gboolean osd_end(gpointer d) { (void)d; OTIMER = 0; if (OWIN && gtk_widget_get_visible(OWIN)) { gtk_widget_hide(OWIN); pop_gone(OBG, OX, OY); } OK_[0] = 0; state_write(); return G_SOURCE_REMOVE; }
static void osd_show(const char *k, const char *lv, const char *label, const char *n)
{
	if (!k || (strcmp(k, "volume") && strcmp(k, "mic") && strcmp(k, "brightness")) || !USER_ID[0] || LOCKED) return;
	if (FLK == FL_VOL) return;   /* (not while the volume's own pop-up is open, as the page's .svp) */
	snprintf(OK_, sizeof OK_, "%s", k); snprintf(OLV, sizeof OLV, "%s", lv && strspn(lv, "0123456789") == strlen(lv) ? lv : ""); snprintf(OLB, sizeof OLB, "%s", label ? label : ""); ON_ = n ? atoi(n) : 0;
	if (!OWIN) { OWIN = gtk_window_new(GTK_WINDOW_POPUP); gtk_window_set_title(GTK_WINDOW(OWIN), "Indicator"); OAREA = gtk_drawing_area_new(); gtk_container_add(GTK_CONTAINER(OWIN), OAREA); g_signal_connect(OAREA, "draw", G_CALLBACK(osd_draw), NULL); }
	cairo_surface_t *sf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 4, 4); cairo_t *c = cairo_create(sf); double w, h; osd_size(c, &w, &h); cairo_destroy(c); cairo_surface_destroy(sf);
	int nw = (int)ceil(w * Z), nh = (int)ceil(h * Z);
	/* (shown already at this size: what is behind is the copy it has; hidden and copied again, the window behind may not have
	   drawn itself yet) */
	if (!(gtk_widget_get_visible(OWIN) && OBG && nw == OW && nh == OH)) {
		if (gtk_widget_get_visible(OWIN)) { gtk_widget_hide(OWIN); gdk_display_sync(gdk_display_get_default()); pop_gone(OBG, OX, OY); }
		OW = nw; OH = nh; OX = (SW - OW) / 2; OY = (int)lround(28 * Z);
		if (OBG) g_object_unref(OBG); OBG = screen_copy(OX, OY, OW, OH); }
	gtk_widget_set_size_request(OAREA, OW, OH); gtk_window_resize(GTK_WINDOW(OWIN), OW, OH); gtk_window_move(GTK_WINDOW(OWIN), OX, OY);
	gtk_widget_show_all(OWIN); XRaiseWindow(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), GDK_WINDOW_XID(gtk_widget_get_window(OWIN)));
	{ cairo_region_t *rg = cairo_region_create(); gdk_window_input_shape_combine_region(gtk_widget_get_window(OWIN), rg, 0, 0); cairo_region_destroy(rg); }   /* (pointer-events: none) */
	gtk_widget_queue_draw(OAREA); if (OTIMER) g_source_remove(OTIMER); OTIMER = g_timeout_add(1500, osd_end, NULL); state_write();
}

/* ------------------------------------------------------------------ rudolfd's events (1.6.0)
   On Standard the desktop page hears them (core25.js, events/wait): the power key, the volume, brightness and microphone keys'
   indicator, Ctrl+Alt+Del's Task Manager, the charger, drives, Wi-Fi, the memory guard. On the native desktop the shell is the desktop:
   it asks (events/wait?shell=1; rudolfd gives an app's or Oracle's page none, which was how the power key's event sat until Oracle's
   page took it and drew the dialog in its own window minutes later), shows the power dialog, the indicator and Task Manager above
   every window, at once and once, and gives the page of one app window (the one in front, else the first) the rest (__oaEv); with no
   page open, they wait for the next one, at most a minute. */
static GString *EVQ; static gint64 EVQ_T;
static void tm_urgent(void);
static void bri_seen(int lv);
static int ev_page_ok(AppWin *a) { return a && a->view && !a->closing && !a->hidden && !a->rest && strncmp(a->k, "do-", 3); }
static void ev_to(AppWin *a, const char *t)
{
	char *e = g_uri_escape_string(t, NULL, FALSE), *js = g_strdup_printf("window.__oaEv&&__oaEv(decodeURIComponent('%s'))", e);
	webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(a->view), js, -1, NULL, NULL, NULL, NULL, NULL); g_free(js); g_free(e);
}
static AppWin *ev_page(void)
{
	if (ev_page_ok(ACTIVE)) return ACTIVE;
	for (GList *l = WINS; l; l = l->next) if (ev_page_ok(l->data)) return l->data;
	/* (1.6.0 Part 12, item 12) with no app window open: the desktop's service page, as Standard's one page hears them whatever is open */
	{ AppWin *sv = svc_page(); if (sv && sv->sready && sv->view) return sv; }
	return NULL;
}
static void ev_give(const char *t)
{
	AppWin *a = ev_page(); gint64 now = g_get_monotonic_time();
	if (a) { ev_to(a, t); return; }
	if (!EVQ) EVQ = g_string_new(NULL);
	if (EVQ->len && now - EVQ_T > 60000000) g_string_truncate(EVQ, 0);
	if (EVQ->len + strlen(t) < 16384) g_string_append(EVQ, t); EVQ_T = now;
}
/* (a page with its window now: what waited for one) */
static void ev_flush(AppWin *a)
{
	if (!EVQ || !EVQ->len || !(ev_page_ok(a) || (a && a == svc_page() && a->sready))) return;
	if (g_get_monotonic_time() - EVQ_T <= 60000000) ev_to(a, EVQ->str);
	g_string_truncate(EVQ, 0);
}
/* (every page: "power shutdown", each writes its files first) */
static void ev_all(const char *t) { for (GList *l = WINS; l; l = l->next) { AppWin *a = l->data; if (a->view && !a->closing) ev_to(a, t); } }
/* the volume's indicator from what is known (osd31.js osdShow: its level, Muted, No sound output) */
static void osd_volume(void)
{
	char lv[16] = "", lb[64], n[4]; int v = VOL;
	if (v < 0) snprintf(lb, sizeof lb, "No sound output"); else if (MUTED) { snprintf(lb, sizeof lb, "Muted"); snprintf(lv, sizeof lv, "0"); } else { snprintf(lb, sizeof lb, "%d%%", v); snprintf(lv, sizeof lv, "%d", v); }
	snprintf(n, sizeof n, "%d", MUTED || v <= 0 ? 0 : v <= 33 ? 1 : v <= 66 ? 2 : 3); osd_show("volume", lv, lb, n);
}
static void osd_bright(void)
{
	char lv[16] = "", lb[96], n[16];
	if (BRI < 0) snprintf(lb, sizeof lb, "This screen\xe2\x80\x99s brightness cannot be changed here"); else { snprintf(lb, sizeof lb, "%d%%", BRI); snprintf(lv, sizeof lv, "%d", BRI); }
	snprintf(n, sizeof n, "%d", BRI < 0 ? 100 : BRI); osd_show("brightness", lv, lb, n);
}
static gint64 BRI_EV_T;   /* (the last brightness the keys reported: the backlight's own change is not shown twice) */
static int ev_shell(const char *l)
{
	if (!strncmp(l, "power ", 6) && g_ascii_isdigit(l[6])) { if (!(PWR && gtk_widget_get_visible(PWR))) power_dialog(); return 1; }
	if (!strcmp(l, "power shutdown")) { ev_all("power shutdown\n"); return 1; }
	if (!strncmp(l, "osd volume ", 11)) { const char *v = l + 11; if (!strcmp(v, "none")) VOL = -1; else if (!strcmp(v, "muted")) MUTED = 1; else { VOL = atoi(v); MUTED = 0; } bar_redraw(); osd_volume(); return 1; }
	if (!strncmp(l, "osd brightness ", 15)) { const char *v = l + 15; BRI = !strcmp(v, "none") ? -1 : atoi(v); BRI_EV_T = g_get_monotonic_time(); bri_seen(BRI); bar_redraw(); osd_bright(); return 1; }
	if (!strncmp(l, "osd mic ", 8)) { int m = !strcmp(l + 8, "muted"); osd_show("mic", "", m ? "Microphone off" : "Microphone on", m ? "1" : "0"); return 1; }
	if (!strcmp(l, "taskmgr")) { tm_urgent(); return 1; }
	if (!strncmp(l, "power ac", 8) || !strncmp(l, "power battery", 13) || !strncmp(l, "power saver", 11)) tray_load();   /* (the battery's button now; Settings' page hears it too) */
	return 0;
}
static gboolean ev_got(gpointer d)
{
	char *t = d; GString *rest = g_string_new(NULL);
	for (char *l = t, *e; l && *l; l = e) { e = strchr(l, '\n'); if (e) *e++ = 0; g_strchomp(l); if (!*l) continue;
		if (!strncmp(l, "power", 5) || !strncmp(l, "osd ", 4) || !strcmp(l, "taskmgr")) fprintf(stderr, "rudolf-native: event %s\n", l);
		if (!ev_shell(l)) { g_string_append(rest, l); g_string_append_c(rest, '\n'); } }
	if (rest->len) ev_give(rest->str);
	g_string_free(rest, TRUE); g_free(t); state_write(); return G_SOURCE_REMOVE;
}
static gpointer ev_thread(gpointer d)
{
	(void)d;
	for (;;) {
		GByteArray *b = NULL; int c = http("GET", "/api/events/wait?shell=1", NULL, 0, &b);
		if (c == 200 && b) { JV *v = jparse((char *)b->data); JV *e = jprop(v, "events"); if (e && e->t == 3 && e->s && e->s[0]) g_idle_add(ev_got, g_strdup(e->s)); jfree(v); }
		if (b) g_byte_array_free(b, TRUE);
		if (c != 200) g_usleep(2500000);
	}
	return NULL;
}

/* ------------------------------------------------------------------ the portal's requests (1.6.0, the owner's Vivobook, 2026-11-16:
   Sober's sign-in spun for ever on Native; links and file dialogs from Linux apps did nothing)
   A Linux or Flatpak app's requests to the desktop go through the Rudolf portal (rudolf-portal: the keyring's prompts, file Open and
   Save dialogs, links, notifications) and wait in rudolfd until the desktop collects them (portal/poll). On Standard the one desktop
   page always does (linux26.js lxPortalPoll). Here there is no desktop page: each app's window was a page that polled for itself, so
   with no app window open nothing collected them (the keyring's prompt never came: the sign-in's spinner), and with windows open
   each drew them (inside its own window, maybe behind the app, or twice). Now the shell collects them itself, twice a second whether
   or not any window is open, and hands each request once to one page of its own kept unseen for it (#app=do-portal, oneapp59.js,
   opened at sign-in so it is ready at once): its dialogs are Standard's own (the keyring's password, Open and Save, "Open Claude?"),
   shown above every window as the page's dialogs are here (the window covering the screen); its notifications are the shell's
   toasts; links go to Oracle's window. What opens an app's window (a file or folder, a mail link) is done by a page of its own that
   becomes that window (#app=do-portalapp), as Standard opens it. The app windows no longer collect anything (linux26.js). */
static GHashTable *PT_GIVEN;   /* the requests handed out: id -> 1 (the portal's page) or 2 (a page of its own) */
static char *PT_IDS;           /* the requests rudolfd listed last: id,id,... */
/* 1.6.1 (the owner's Vivobook, 2026-11-20: six web processes, about 1 GB of a 3.3 GB laptop) the portal's page, the desktop's service
   page and the start-up page are one page (#app=do-bg, oneapp59.js), a web process of the whole desktop page less: opened at sign-in
   (the start-up page's moment), or once nothing else is starting (page_starting, up to 10 s), again if it ends, or does not say both
   "svc ready" and "portal ready" within 90 s; closed with the other windows at sign-out. The shell's single actions go to it too
   (bg_route), and the start menu's search (srch_page). */
#define BG_K "do-bg"
static gint64 BG_OPEN_T;   /* (when it was asked for, or its process stopped and it loads again) */
static gint64 BG_BUSY_T;   /* (since when another page has been starting, while it is not there) */
/* 1.6.1 (the owner's Vivobook, 2026-11-20: the memory rose at the start, every page loading the whole desktop at once, 400-470 MB
   each at its peak): a page still loading, or opened in the last 4 s (its scripts still starting after the load), keeps the
   hidden pages the shell opens by itself (the portal's, the desktop's service page) waiting, so their peaks do not come together */
static int page_starting(gint64 now)
{
	for (GList *l = WINS; l; l = l->next) { AppWin *w = l->data; if (!w->view || w->closing) continue;
		if (webkit_web_view_is_loading(WEBKIT_WEB_VIEW(w->view)) || now - w->born < 4 * G_USEC_PER_SEC) return 1; }
	return 0;
}
static int pt_id_ok(const char *s) { size_t n = strlen(s); return n && n <= 40 && strspn(s, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") == n; }
static AppWin *bg_page(void) { for (GList *l = WINS; l; l = l->next) { AppWin *a = l->data; if (!strcmp(a->k, BG_K) && !a->closing) return a; } return NULL; }
static int is_bg(const AppWin *a) { return a && !strcmp(a->k, BG_K); }
static AppWin *pt_page(void) { return bg_page(); }
static int SVC_N;   /* (how many times the hidden page was opened, for the tests) */
static void bg_open(void)
{
	char ex[96]; snprintf(ex, sizeof ex, "&since=%lld%s", (long long)SVC_SIGNIN, BG_START ? "&start=1" : ""); BG_START = 0;
	BG_OPEN_T = g_get_monotonic_time(); BG_BUSY_T = 0; SVC_N++;
	fprintf(stderr, "rudolf-native: the shell's hidden page is opened\n"); app_open_ex(BG_K, NULL, ex, 2); state_write();
}
/* (the hidden page there, or opened: once no other page is starting, or after 10 s of them; again 10 s after it was asked for) */
static void bg_tick(void)
{
	if (!USER_ID[0] || LOCKED) return;
	AppWin *p = bg_page(); gint64 now = g_get_monotonic_time(); int busy = page_starting(now);
	if (p || !busy) BG_BUSY_T = 0; else if (!BG_BUSY_T) BG_BUSY_T = now;
	if (!p) { if ((!BG_OPEN_T || now - BG_OPEN_T > 10 * G_USEC_PER_SEC) && (!busy || now - BG_BUSY_T >= 10 * G_USEC_PER_SEC)) bg_open(); }
	else if ((!p->sready || !p->pready) && BG_OPEN_T && now - BG_OPEN_T > 90 * G_USEC_PER_SEC) { fprintf(stderr, "rudolf-native: the shell's hidden page did not start: again\n"); appwin_close_now(p); BG_OPEN_T = 0; }
}
/* a single action for the hidden page: K, the rest of its address and its file as its own page (#app=K) would have had them, run there
   (oneapp59.js __oaDo, which tells the shell when it is done, op "done", as that page did); a page of its own when the hidden page is
   not ready yet or did not take it */
static const char *const BG_DO[] = {"do-qs", "do-nc", "do-pin", "do-mute", "do-desk", "do-fs", "do-short", NULL};
typedef struct { char k[KN]; char *file, *extra; } BgJob;
static int BG_NOROUTE;
static void bg_done(GObject *src, GAsyncResult *res, gpointer d)
{
	BgJob *j = d; JSCValue *v = webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(src), res, NULL); int ok = v && jsc_value_is_number(v) && jsc_value_to_int32(v) == 1; if (v) g_object_unref(v);
	if (!ok) { fprintf(stderr, "rudolf-native: the hidden page did not take %s: a page of its own\n", j->k); BG_NOROUTE = 1; app_open_ex(j->k, j->file, j->extra, 1); BG_NOROUTE = 0; }
	g_free(j->file); g_free(j->extra); g_free(j);
}
static void json_esc(GString *s, const char *v);
static int bg_route(const char *k, const char *file, const char *extra)
{
	if (BG_NOROUTE) return 0; int known = 0; for (int i = 0; BG_DO[i]; i++) if (!strcmp(k, BG_DO[i])) known = 1; if (!known) return 0;
	AppWin *p = bg_page(); if (!p || !p->view || !p->sready) return 0;
	/* (the address's rest as the page's own would have it: decoded there by URLSearchParams, as from location.hash) */
	GString *js = g_string_new("window.__oaDo?__oaDo("); json_esc(js, k); g_string_append_c(js, ','); json_esc(js, extra ? extra : ""); g_string_append_c(js, ','); json_esc(js, file ? file : ""); g_string_append(js, "):0");
	BgJob *j = g_new0(BgJob, 1); snprintf(j->k, sizeof j->k, "%s", k); j->file = g_strdup(file); j->extra = g_strdup(extra);
	webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(p->view), js->str, -1, NULL, NULL, NULL, bg_done, j); g_string_free(js, TRUE); return 1;
}
static void pt_tick(void)
{
	if (!USER_ID[0] || LOCKED) return;
	if (!PT_GIVEN) PT_GIVEN = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	char **L = g_strsplit(PT_IDS ? PT_IDS : "", ",", -1);
	/* (answered or withdrawn: forgotten) */
	GHashTableIter it; gpointer k; g_hash_table_iter_init(&it, PT_GIVEN);
	while (g_hash_table_iter_next(&it, &k, NULL)) if (!g_strv_contains((const char *const *)L, k)) g_hash_table_iter_remove(&it);
	/* (1.6.1: the portal's page is the shell's hidden page: opened, and opened again, by bg_tick) */
	bg_tick(); AppWin *p = pt_page();
	if (p && p->pready) {
		GString *give = g_string_new(NULL);
		for (int i = 0; L[i]; i++) if (pt_id_ok(L[i]) && !g_hash_table_contains(PT_GIVEN, L[i])) {
			g_hash_table_insert(PT_GIVEN, g_strdup(L[i]), GINT_TO_POINTER(1)); if (give->len) g_string_append_c(give, ','); g_string_append(give, L[i]); }
		if (give->len) {
			fprintf(stderr, "rudolf-native: the portal's requests %s to its page\n", give->str);
			char *js = g_strdup_printf("window.__oaPortal&&__oaPortal('%s')", give->str);
			webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(p->view), js, -1, NULL, NULL, NULL, NULL, NULL); g_free(js); state_write();
		}
		g_string_free(give, TRUE);
	}
	g_strfreev(L);
}
static void pt_reset(void) { if (PT_GIVEN) g_hash_table_remove_all(PT_GIVEN); BG_OPEN_T = 0; }
static gboolean pt_got(gpointer d) { g_free(PT_IDS); PT_IDS = d; pt_tick(); return G_SOURCE_REMOVE; }
static gpointer pt_thread(gpointer d)
{
	(void)d;
	for (;;) {
		if (USER_ID[0] && !LOCKED) {
			GByteArray *b = NULL; int c = http("POST", "/api/portal/poll", NULL, 0, &b);   /* (rudolfd answers the portal's requests to POST only) */
			if (c == 200 && b) { JV *v = jparse((char *)b->data), *r = jprop(v, "requests"); GString *ids = g_string_new(NULL);
				for (int i = 0; r && r->t == 4 && i < r->len; i++) { char id[64] = ""; jstr(r->v[i], "id", id, sizeof id); if (pt_id_ok(id)) { if (ids->len) g_string_append_c(ids, ','); g_string_append(ids, id); } }
				g_idle_add(pt_got, g_string_free(ids, FALSE)); jfree(v); }
			if (b) g_byte_array_free(b, TRUE);
			static int said; if (c != 200 && !said++) fprintf(stderr, "rudolf-native: rudolfd's portal/poll answered %d\n", c);
		}
		g_usleep(500000);
	}
	return NULL;
}
static void pt_msg(AppWin *a, JSCValue *v)
{
	char rq[64] = "", rd[4] = ""; jsstr(v, "req", rq, sizeof rq); jsstr(v, "ready", rd, sizeof rd);
	if (!is_bg(a)) return;   /* (only the portal's own page: the shell's hidden page) */
	if (!PT_GIVEN) PT_GIVEN = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	if (rd[0] == '1') {
		/* (ready; or loaded again after its page stopped: what it had been handed, once more) */
		a->pready = 1; GHashTableIter it; gpointer k, val; g_hash_table_iter_init(&it, PT_GIVEN);
		while (g_hash_table_iter_next(&it, &k, &val)) if (GPOINTER_TO_INT(val) == 1) g_hash_table_iter_remove(&it);
		fprintf(stderr, "rudolf-native: the portal's page is ready\n"); pt_tick(); state_write(); return;
	}
	if (pt_id_ok(rq)) { g_hash_table_replace(PT_GIVEN, g_strdup(rq), GINT_TO_POINTER(2)); char ex[80]; snprintf(ex, sizeof ex, "&req=%s", rq); app_open_ex("do-portalapp", NULL, ex, 2); }
}

/* ------------------------------------------------------------------ the desktop's service page (1.6.0 Part 12, item 12)
   On Standard one page is the whole desktop, and the timers its modules start as it loads run once whatever is open: the Clock's
   alarms and timers, the Calendar's reminders, the update check, the Debian and Linux apps' updates, Bluetooth's and Thunderbolt's
   questions, the session's save, the lag meter... Here each app window is a page of its own, which ran them all: with no window open
   none ran (an alarm did not ring), with three open each did (it rang three times). Now an app's window starts only what it needs
   (svc60.js), and one page of the shell's, never shown (#app=do-svc: not on the taskbar, not in Task Manager's apps), runs the rest
   once for the whole desktop: opened at sign-in, opened again if it ends or stops (WebKit reloads a page whose process stopped),
   closed with the other windows at sign-out (so a switch of user starts the next account's). Its notifications are the shell's
   toasts, its sounds the shell's, its dialogs over the screen as any page's (it is opened as a page of the shell's that is shown
   only for a dialog); an app it opens (a reminder's click) is a window of its own. What the shell does itself for the whole desktop
   (the heartbeat, the microphone's 🎙️, the cellular notices, the development link) it does not do again. rudolfd's events that are a
   page's go to the app window in front, else the first one, else this page. */
static AppWin *svc_page(void) { return bg_page(); }   /* (1.6.1: the shell's hidden page) */
static void svc_reset(void) { BG_OPEN_T = 0; }
static gboolean svc_tick(gpointer d)
{
	(void)d; bg_tick();   /* (1.6.1: the shell's hidden page, which runs the service page's part) */
	return G_SOURCE_CONTINUE;
}
static void svc_msg(AppWin *a, JSCValue *v)
{
	char rd[4] = ""; jsstr(v, "ready", rd, sizeof rd); if (!is_bg(a) || rd[0] != '1') return;
	/* (ready; or loaded again after its page stopped) */
	a->sready = 1; fprintf(stderr, "rudolf-native: the desktop's service page is ready\n"); if (!ev_page_ok(ACTIVE)) { int any = 0; for (GList *l = WINS; l; l = l->next) if (ev_page_ok(l->data)) any = 1; if (!any) ev_flush(a); }
	state_write();
}

/* ------------------------------------------------------------------ Task Manager from Ctrl+Alt+Del and Ctrl+Shift+Esc (tmtop57.js tmUrgent)
   above every window (Always on top, focused, restored if minimised, out of full screen), the desktop's processes first while it is
   open (the page's tmPriority, #app=taskmgr&urgent=1) */
static void tm_urgent(void)
{
	if (!USER_ID[0] || LOCKED) return;
	menu_hide(); if (FLK) fl_close();
	if (FOCUS && FOCUS->full && FOCUS->app) appwin_fs(FOCUS->app, 0);
	for (GList *l = WINS; l; l = l->next) { AppWin *w = l->data; if (w->hidden || w->closing || strcmp(w->k, "taskmgr")) continue;
		Client *c = WM_OK ? client_of_app(w) : NULL; if (c) { c->ontop = 1; if (c->min) { c->min = 0; XMapWindow(DPY, c->frame); set_wm_state(c, NormalState); } wm_focus(c); wm_restack(); }
		else appwin_activate(w);
		webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(w->view), "window.__oaTmUrgent&&__oaTmUrgent()", -1, NULL, NULL, NULL, NULL, NULL); state_write(); return; }
	TM_TOP = 1; app_open_ex("taskmgr", NULL, "&urgent=1", 0);
}

/* ------------------------------------------------------------------ the screen's brightness (osd31.js: #bri, briPop, the keys' indicator)
   The taskbar's button when the screen has a backlight (rudolfd's info), its pop-up (the level, 5 to 100 %), the wheel over it; the
   indicator when the brightness changes, from the keys (rudolfd's "osd brightness" events) or from the computer's own firmware (the
   Vivobook's Fn keys change the backlight themselves): the backlight is watched, and a change the keys did not report is shown */
static char BL_DIR[300]; static int BL_MAX = -1, BL_LAST = -1;
static void bri_seen(int lv) { BL_LAST = lv; }
static int bl_read(void)
{
	if (BL_MAX < 0) { BL_MAX = 0; GDir *d = g_dir_open("/sys/class/backlight", 0, NULL); const char *n;
		while (d && (n = g_dir_read_name(d))) { char p[400], *t = NULL; snprintf(p, sizeof p, "/sys/class/backlight/%s/max_brightness", n);
			if (g_file_get_contents(p, &t, NULL, NULL) && atoi(t) > 0) { BL_MAX = atoi(t); snprintf(BL_DIR, sizeof BL_DIR, "/sys/class/backlight/%s", n); g_free(t); break; } g_free(t); }
		if (d) g_dir_close(d); }
	if (BL_MAX <= 0) return -1;
	char p[400], *t = NULL; snprintf(p, sizeof p, "%s/brightness", BL_DIR); if (!g_file_get_contents(p, &t, NULL, NULL)) return -1;
	int c = atoi(t); g_free(t); return (c * 100 + BL_MAX / 2) / BL_MAX;
}
static gboolean bl_tick(gpointer d)
{
	(void)d; int lv = bl_read(); if (lv < 0) return G_SOURCE_CONTINUE;
	if (BL_LAST >= 0 && lv != BL_LAST && g_get_monotonic_time() - BRI_EV_T > 1500000 && USER_ID[0] && !LOCKED) { BRI = lv; bar_redraw(); osd_bright(); }
	BL_LAST = lv; return G_SOURCE_CONTINUE;
}
static void bri_load(void)
{
	JV *v = jget("/api/info"); BRI_HAS = v && jbool(v, "backlight"); jfree(v);
	if (BRI_HAS) { v = jget("/api/brightness"); JV *b = jprop(v, "brightness"); BRI = b && b->t == 2 ? (int)b->n : -1; jfree(v); }
}
typedef struct { char q[64]; int bri; } HkReq;
static gboolean hk_done(gpointer d) { (void)d; bar_redraw(); return G_SOURCE_REMOVE; }
static gboolean hk_bri_done(gpointer d) { int v = GPOINTER_TO_INT(d) - 1; if (v >= 0) { BRI = v; BRI_EV_T = g_get_monotonic_time(); bri_seen(v); osd_bright(); } bar_redraw(); return G_SOURCE_REMOVE; }
static gboolean hk_vol_done(gpointer d) { int r = GPOINTER_TO_INT(d); if (r) { VOL = (r >> 1) - 1; MUTED = r & 1; osd_volume(); } bar_redraw(); return G_SOURCE_REMOVE; }
static gpointer hk_thread(gpointer p)
{
	HkReq *q = p; GByteArray *b = NULL; JV *v = NULL;
	if (http("POST", q->q, NULL, 0, &b) == 200 && b) v = jparse((char *)b->data); if (b) g_byte_array_free(b, TRUE);
	if (q->bri == 1) { JV *x = jprop(v, "brightness"); g_idle_add(hk_bri_done, GINT_TO_POINTER(x && x->t == 2 ? (int)x->n + 1 : 0)); }
	else if (q->bri == 0 && v && jbool(v, "ok")) g_idle_add(hk_vol_done, GINT_TO_POINTER((((int)jnum(v, "volume", -1) + 1) << 1) | (jbool(v, "muted") ? 1 : 0)));
	else g_idle_add(hk_done, NULL);
	jfree(v); g_free(q); return NULL;
}
/* (POST in a thread of its own: the sound server or the backlight may take a moment) */
static void hk_post(const char *q, int bri) { HkReq *r = g_new0(HkReq, 1); snprintf(r->q, sizeof r->q, "%s", q); r->bri = bri; g_thread_unref(g_thread_new("hotkey", hk_thread, r)); }
static void bri_set(int v) { v = CLAMP(v, 5, 100); BRI = v; char q[48]; snprintf(q, sizeof q, "/api/brightness?value=%d", v); hk_post(q, 2); bar_redraw(); }

/* ------------------------------------------------------------------ the screen's colour: Night light and High contrast (a11y25.js themeApply,
   applyCB) for the whole screen. On Standard the page is the screen: #nightl, an orange (#ff8a00) layer multiplied over it at its strength
   (S.nlv / 100 * .55, only from 8 PM to 7 AM if asked), and High contrast's filter, contrast(1.35) saturate(1.2), on everything, the
   layer too. On the native desktop the windows are pages of their own and Linux's, so the shell gives the X screen that colour: its
   gamma ramps (XRandR, as xrandr --gamma; with the software display rudolf-display applies them as it copies the picture), the orange
   layer and the contrast as the page's. The pages then draw neither (oneapp59.js __oaShell.gamma), the saturation only. */
static int GAMMA_OK = -1, GAMMA_ON; static double GAMMA_K[3] = {1, 1, 1}; static int GAMMA_C;
static void *xrr_lib(void) { static void *h; static int tried; if (!tried) { tried = 1; h = dlopen("libXrandr.so.2", RTLD_NOW | RTLD_GLOBAL); } return h; }
static int nl_on(void) { if (!S.nl) return 0; if (!S.nlsched) return 1; time_t t = time(NULL); struct tm lt; localtime_r(&t, &lt); return lt.tm_hour >= 20 || lt.tm_hour < 7; }
static void gamma_apply(void)
{
	void *h = xrr_lib(); if (!h || !DPY) { GAMMA_OK = 0; return; }
	XRRScreenResources *(*getres)(Display *, Window) = (XRRScreenResources *(*)(Display *, Window))dlsym(h, "XRRGetScreenResourcesCurrent");
	int (*gsize)(Display *, RRCrtc) = (int (*)(Display *, RRCrtc))dlsym(h, "XRRGetCrtcGammaSize");
	XRRCrtcGamma *(*galloc)(int) = (XRRCrtcGamma *(*)(int))dlsym(h, "XRRAllocGamma");
	void (*gset)(Display *, RRCrtc, XRRCrtcGamma *) = (void (*)(Display *, RRCrtc, XRRCrtcGamma *))dlsym(h, "XRRSetCrtcGamma");
	void (*gfree)(XRRCrtcGamma *) = (void (*)(XRRCrtcGamma *))dlsym(h, "XRRFreeGamma");
	void (*rfree)(XRRScreenResources *) = (void (*)(XRRScreenResources *))dlsym(h, "XRRFreeScreenResources");
	int ev, er; Bool (*qe)(Display *, int *, int *) = (Bool (*)(Display *, int *, int *))dlsym(h, "XRRQueryExtension");
	if (!getres || !gsize || !galloc || !gset || !gfree || !rfree || !qe || !qe(DPY, &ev, &er)) { GAMMA_OK = 0; return; }
	int nl = nl_on(), hc = S.hc; double a = nl ? (S.nlv > 0 ? S.nlv : 40) / 100.0 * .55 : 0, k[3] = { 1, 1 - a * (1 - 138 / 255.0), 1 - a };
	if (GAMMA_OK == 1 && GAMMA_ON == (nl || hc) && GAMMA_C == hc && k[0] == GAMMA_K[0] && k[1] == GAMMA_K[1] && k[2] == GAMMA_K[2]) return;
	XRRScreenResources *r = getres(DPY, ROOT); if (!r) { GAMMA_OK = 0; return; }
	int done = 0;
	for (int i = 0; i < r->ncrtc; i++) { int n = gsize(DPY, r->crtcs[i]); if (n < 2) continue; XRRCrtcGamma *g = galloc(n); if (!g) continue;
		for (int j = 0; j < n; j++) { double x = j / (double)(n - 1);
			for (int c = 0; c < 3; c++) { double y = x * k[c]; if (hc) y = (y - .5) * 1.35 + .5; y = CLAMP(y, 0, 1); unsigned short u = (unsigned short)lround(y * 65535); if (c == 0) g->red[j] = u; else if (c == 1) g->green[j] = u; else g->blue[j] = u; } }
		gset(DPY, r->crtcs[i], g); gfree(g); done++; }
	rfree(r); XFlush(DPY);
	GAMMA_OK = done > 0; GAMMA_ON = nl || hc; GAMMA_C = hc; memcpy(GAMMA_K, k, sizeof k);
}

/* ------------------------------------------------------------------ the magnifier (a11y25.js magApply): the whole screen zoomed S.magz times
   round the pointer, as Standard's page scales itself (transform-origin at the pointer, so what is under the pointer stays there and a
   click goes where it went). Drawn by the shell over every window (the windows' own pictures, X's Composite extension, while it is
   on), its window letting the pointer through; Meta+= and Meta+- zoom, Meta+Esc ends it. Without Composite the pages zoom themselves
   as before. */
static int MAG_OK = -1, MAG_ON; static GtkWidget *MAGW, *MAGA; static guint MAG_T; static int MAGPX, MAGPY;
static void *xc_lib(void) { static void *h; static int tried; if (!tried) { tried = 1; h = dlopen("libXcomposite.so.1", RTLD_NOW | RTLD_GLOBAL); } return h; }
static int mag_can(void)
{
	if (MAG_OK >= 0) return MAG_OK; void *h = xc_lib(); MAG_OK = 0; if (!h || !DPY) return 0;
	Bool (*qe)(Display *, int *, int *) = (Bool (*)(Display *, int *, int *))dlsym(h, "XCompositeQueryExtension"); int ev, er;
	MAG_OK = qe && qe(DPY, &ev, &er) && dlsym(h, "XCompositeNameWindowPixmap") && dlsym(h, "XCompositeRedirectSubwindows") ? 1 : 0; return MAG_OK;
}
static gboolean mag_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)w; (void)d; if (!MAG_ON) return TRUE; void *h = xc_lib();
	Pixmap (*name)(Display *, Window) = (Pixmap (*)(Display *, Window))dlsym(h, "XCompositeNameWindowPixmap");
	Window me = GDK_WINDOW_XID(gtk_widget_get_window(MAGW)), rr, pr, *ch = NULL; unsigned n = 0;
	cairo_set_source_rgb(cr, 0, 0, 0); cairo_paint(cr);
	cairo_translate(cr, MAGPX, MAGPY); cairo_scale(cr, MAGZ, MAGZ); cairo_translate(cr, -MAGPX, -MAGPY);
	if (!XQueryTree(DPY, ROOT, &rr, &pr, &ch, &n)) return TRUE;
	/* (a window may go while it is read: GDK's own error trap, so an error from X never ends the shell) */
	GdkDisplay *gd = gdk_display_get_default(); gdk_x11_display_error_trap_push(gd);
	for (unsigned i = 0; i < n; i++) { if (ch[i] == me) continue; XWindowAttributes at; if (!XGetWindowAttributes(DPY, ch[i], &at) || at.map_state != IsViewable || at.class != InputOutput) continue;
		if (at.x >= SW || at.y >= SH || at.x + at.width <= 0 || at.y + at.height <= 0) continue;
		Pixmap pm = name(DPY, ch[i]); if (!pm) continue; int bw = at.border_width;
		cairo_surface_t *s = cairo_xlib_surface_create(DPY, pm, at.visual, at.width + 2 * bw, at.height + 2 * bw);
		cairo_set_source_surface(cr, s, at.x, at.y); cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR); cairo_paint(cr);
		cairo_set_source_rgb(cr, 0, 0, 0); cairo_surface_flush(cairo_get_target(cr)); cairo_surface_finish(s); cairo_surface_destroy(s); XFreePixmap(DPY, pm); }   /* (drawn before its picture goes) */
	XSync(DPY, False); gdk_x11_display_error_trap_pop_ignored(gd); if (ch) XFree(ch);
	return TRUE;
}
static gboolean mag_tick(gpointer d)
{
	(void)d; if (!MAG_ON || !MAGW) { MAG_T = 0; return G_SOURCE_REMOVE; }
	Window r, c; int rx, ry, wx, wy; unsigned m; if (XQueryPointer(DPY, ROOT, &r, &c, &rx, &ry, &wx, &wy, &m)) { MAGPX = rx; MAGPY = ry; }
	XRaiseWindow(DPY, GDK_WINDOW_XID(gtk_widget_get_window(MAGW))); gtk_widget_queue_draw(MAGA); return G_SOURCE_CONTINUE;
}
static void mag_set(int on)
{
	if (on && !mag_can()) on = 0;
	if (on == MAG_ON) { if (on && MAGA) gtk_widget_queue_draw(MAGA); return; }
	void *h = xc_lib(); MAG_ON = on;
	void (*redir)(Display *, Window, int) = h ? (void (*)(Display *, Window, int))dlsym(h, on ? "XCompositeRedirectSubwindows" : "XCompositeUnredirectSubwindows") : NULL;
	if (on) {
		if (redir) redir(DPY, ROOT, CompositeRedirectAutomatic);
		if (!MAGW) { MAGW = gtk_window_new(GTK_WINDOW_POPUP); gtk_window_set_title(GTK_WINDOW(MAGW), "Magnifier"); MAGA = gtk_drawing_area_new(); gtk_container_add(GTK_CONTAINER(MAGW), MAGA);
			g_signal_connect(MAGA, "draw", G_CALLBACK(mag_draw), NULL); gtk_widget_set_app_paintable(MAGW, TRUE); }
		gtk_window_move(GTK_WINDOW(MAGW), 0, 0); gtk_window_resize(GTK_WINDOW(MAGW), SW, SH); gtk_widget_set_size_request(MAGA, SW, SH); gtk_widget_show_all(MAGW);
		{ cairo_region_t *rg = cairo_region_create(); gdk_window_input_shape_combine_region(gtk_widget_get_window(MAGW), rg, 0, 0); cairo_region_destroy(rg); }   /* (the pointer goes through) */
		if (!MAG_T) MAG_T = g_timeout_add(50, mag_tick, NULL); mag_tick(NULL);
	} else { if (MAGW) gtk_widget_hide(MAGW); if (redir) redir(DPY, ROOT, CompositeRedirectAutomatic); }
}
static void mag_zoom(double dz)
{
	/* (magin: on at 1.5, else half a step more, at most 8; magout: half a step less, off at 1) */
	if (dz > 0) { if (!S.mag) { S.mag = 1; MAGZ = 1.5; } else MAGZ = fmin(8, MAGZ + .5); }
	else { MAGZ = fmax(1, MAGZ - .5); if (MAGZ <= 1) { S.mag = 0; MAGZ = 2; } }
	mag_set(S.mag); state_write();
}

/* ------------------------------------------------------------------ the on-screen keyboard (base_v24.html #vkb, vkSync, the taskbar's ⌨)
   The page's own keyboard (its keys for the language chosen, Shift, Caps, ⌫, Enter, Tab, Space), drawn by a page of its own
   (#app=do-vk) in a window that never takes the keyboard, at the bottom of the screen above the taskbar as on Standard, over every
   window; a key goes to the page of the window in front, which types it as Standard's keyboard does (vkInsert, vkSpecial). Its ✕ hides
   it, the taskbar's ⌨ shows it again; it is there while the switch is on. */
static GtkWidget *VKW, *VKV; static int VK_H = 292, VK_LOADED, VKX, VKY; static char VKKEYS[2048];
static void vk_place(void)
{
	if (!VKW) return; int top = !strcmp(S.tb, "top"), w = (int)lround(fmin(760, SW / Z * .98) * Z), h = (int)lround(VK_H * Z);
	VKX = (SW - w) / 2; VKY = SH - h - (int)lround((top ? 10 : 54) * Z);
	gtk_window_resize(GTK_WINDOW(VKW), w, h); gtk_widget_set_size_request(VKV, w, h); gtk_window_move(GTK_WINDOW(VKW), VKX, VKY);
}
static void vk_sync(void)
{
	int show = S.vk && VK_SHOWN && USER_ID[0] && !LOCKED;
	if (show && !VKW) {
		ctx_make(); VKW = gtk_window_new(GTK_WINDOW_POPUP); gtk_window_set_title(GTK_WINDOW(VKW), "On-screen keyboard"); gtk_window_set_accept_focus(GTK_WINDOW(VKW), FALSE);
		VKV = GTK_WIDGET(g_object_new(WEBKIT_TYPE_WEB_VIEW, "web-context", CTX, "user-content-manager", UCM, NULL)); gtk_container_add(GTK_CONTAINER(VKW), VKV);
		char *u = app_url("do-vk", NULL, 0, NULL); webkit_web_view_load_uri(WEBKIT_WEB_VIEW(VKV), u); g_free(u); }
	if (!VKW) return;
	if (show) { vk_place(); gtk_widget_show_all(VKW); XRaiseWindow(DPY, GDK_WINDOW_XID(gtk_widget_get_window(VKW))); } else gtk_widget_hide(VKW);
	bar_redraw();   /* (no state written here: a11y_apply runs inside settings_load, in the middle of signing in) */
}
/* (a window that is not a page of the shell's, Oracle's in Chromium or a Linux app's: the key typed as the keyboard would, through
   XTest; a character the keyboard map lacks is put on a spare key for the moment, as xdotool does) */
static gboolean vk_unspare(gpointer d) { KeySym z[2] = { NoSymbol, NoSymbol }; XChangeKeyboardMapping(DPY, GPOINTER_TO_INT(d), 2, z, 1); XFlush(DPY); return G_SOURCE_REMOVE; }
static void vk_xtest(const char *c, const char *sp)
{
	static void *h; static int tried; if (!tried) { tried = 1; h = dlopen("libXtst.so.6", RTLD_NOW); } if (!h || !DPY) return;
	int (*fake)(Display *, unsigned, Bool, unsigned long) = (int (*)(Display *, unsigned, Bool, unsigned long))dlsym(h, "XTestFakeKeyEvent"); if (!fake) return;
	KeySym ks = NoSymbol;
	if (sp && sp[0]) ks = !strcmp(sp, "bs") ? XK_BackSpace : !strcmp(sp, "enter") ? XK_Return : !strcmp(sp, "tab") ? XK_Tab : !strcmp(sp, "space") ? XK_space : NoSymbol;
	else if (c && c[0]) { gunichar u = g_utf8_get_char_validated(c, -1); if (u == (gunichar)-1 || u == (gunichar)-2) return; ks = u >= 0x20 && u < 0x7f ? (KeySym)u : u >= 0xa0 && u <= 0xff ? (KeySym)u : (KeySym)(0x01000000 | u); }
	if (ks == NoSymbol) return;
	KeyCode kc = XKeysymToKeycode(DPY, ks); int shift = 0, spare = 0;
	if (kc) { shift = XkbKeycodeToKeysym(DPY, kc, 0, 0) != ks && XkbKeycodeToKeysym(DPY, kc, 0, 1) == ks; }
	else { int lo, hi; XDisplayKeycodes(DPY, &lo, &hi); kc = (KeyCode)hi; KeySym m[2] = { ks, ks }; XChangeKeyboardMapping(DPY, kc, 2, m, 1); XSync(DPY, False); spare = 1; }
	KeyCode sh = XKeysymToKeycode(DPY, XK_Shift_L);
	if (shift && sh) fake(DPY, sh, True, CurrentTime);
	fake(DPY, kc, True, CurrentTime); fake(DPY, kc, False, CurrentTime);
	if (shift && sh) fake(DPY, sh, False, CurrentTime);
	XSync(DPY, False);
	if (spare) g_timeout_add(150, vk_unspare, GINT_TO_POINTER((int)kc));   /* (after the window has read the key) */
}
/* (a page's message: a key, a special key, its ✕, its height) */
static void vk_msg(JSCValue *v)
{
	char c[64] = "", sp[16] = "", x[8] = "", h[8] = ""; jsstr(v, "c", c, sizeof c); jsstr(v, "sp", sp, sizeof sp); jsstr(v, "x", x, sizeof x); jsstr(v, "h", h, sizeof h); jsstr(v, "keys", VKKEYS, sizeof VKKEYS);
	if (x[0] == '1') { VK_SHOWN = 0; vk_sync(); state_write(); return; }
	if (atoi(h) > 50 && atoi(h) < 800) { VK_H = atoi(h); VK_LOADED = 1; if (VKW && gtk_widget_get_visible(VKW)) vk_place(); return; }
	if (!c[0] && !sp[0]) return;
	Client *f = FOCUS; AppWin *a = f && f->app ? f->app : NULL; if (!a && !WM_OK) a = ACTIVE;
	if (!a || !a->view) { if (f) vk_xtest(c, sp); return; }   /* (Oracle's or a Linux app's window: typed as keys) */
	GString *js = g_string_new("window.__oaVkIn&&__oaVkIn("); json_esc(js, c); g_string_append_c(js, ','); json_esc(js, sp); g_string_append_c(js, ')');
	webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(a->view), js->str, -1, NULL, NULL, NULL, NULL, NULL); g_string_free(js, TRUE);
}

/* ------------------------------------------------------------------ quick settings' switches done for the whole screen (shell25.js qsToggle's)
   after a switch is changed here or in a page (Settings), and at sign-in: the screen's colour, the magnifier, the on-screen keyboard; the
   taskbar, menus and title bars in touch mode's sizes and high contrast's edges. The pages do the rest (their dark, high contrast and touch
   classes, the screen reader) as they hear of the change. */
static int A11Y_VK;
static void a11y_apply(void)
{
	gamma_apply();
	if (S.mag != MAG_SAVED) { MAG_SAVED = S.mag; if (S.mag && S.magz > 0) MAGZ = S.magz; mag_set(S.mag); } else if (mag_can()) S.mag = MAG_ON;   /* (on or off as saved; as the keys left it until then) */
	if (S.vk && !A11Y_VK) VK_SHOWN = 1; A11Y_VK = S.vk; vk_sync();   /* (a11yApply: switched on, it shows) */
	/* (touch mode: title bars at least 40 px, styles25.css html.touch .win>.tb; off again: the title bars' own height when the settings come) */
	static int TH0; int th = (int)lround(40 * Z);
	if (S.touch && TH < th) { TH0 = TH; TH = th; if (WM_OK) { wm_relayout(); frames_redraw(); } }
	else if (!S.touch && TH0) { if (TH == th) TH = TH0; TH0 = 0; if (WM_OK) { wm_relayout(); frames_redraw(); } }
	bar_redraw();
}
/* (the user script every page of the shell gets: what the shell does for the whole screen, so the page does not do it again) */
static char *shell_script(void)
{
	gamma_apply(); return g_strdup_printf("window.__oaShell={gamma:%d,mag:%d,vk:1};", GAMMA_OK == 1, mag_can());
}
/* (tests: is X's top window this one) */
static int x_on_top(GtkWidget *w)
{
	if (!w || !gtk_widget_get_visible(w) || !DPY) return 0; Window me = GDK_WINDOW_XID(gtk_widget_get_window(w)), rr, pr, *ch = NULL; unsigned n = 0; int top = 0;
	Client *c = client_of(me); if (c) me = c->frame;
	if (!XQueryTree(DPY, ROOT, &rr, &pr, &ch, &n)) return 0;
	GdkDisplay *gd = gdk_display_get_default(); gdk_x11_display_error_trap_push(gd);   /* (a window going meanwhile) */
	for (int i = (int)n - 1; i >= 0; i--) { XWindowAttributes at; if (!XGetWindowAttributes(DPY, ch[i], &at) || at.map_state != IsViewable || at.override_redirect) continue; top = ch[i] == me; break; }
	gdk_x11_display_error_trap_pop_ignored(gd); if (ch) XFree(ch); return top;
}

/* ------------------------------------------------------------------ the window keys (1.6.0 Part 12, item 5)
   shell25.js's keyboard shortcuts (SHORT: the registry with each account's own keys, bd95kb:ID from rudolfd's desktop/settings
   "kb", as Settings › Keyboard changes them), done by the shell; Super alone opens the start menu (metaAlone); Alt+Tab's switcher
   (#altab: the windows in the order last used, the second one chosen, Tab and Shift+Tab, Escape, chosen when Alt is let go) and
   Super+Tab's overview (all the workspace's windows side by side over the shaded desktop, as overview(): a click picks one).
   The page has no "snap left or right", so neither has the shell. */
typedef struct { const char *id, *def; void (*fn)(void); } Sc;
static Client *act_win(void)   /* (actWin: the last used window shown in this workspace) */
{
	Client *b = NULL; for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (!tb_win(c) || c->min || ws_of(c) != WSC) continue; if (!b || c->used > b->used) b = c; }
	return b;
}
static void sc_files(void) { app_open("explorer", NULL); }
static void sc_desktop(void) { for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (tb_win(c) && !c->min && ws_of(c) == WSC) wm_minimise(c); } }
static void sc_run(void) { app_open("do-run", NULL); }
static void sc_lock(void) { power("lock"); }
static void sc_settings(void) { for (GList *l = WINS; l; l = l->next) { AppWin *w = l->data; if (!w->hidden && !strcmp(w->k, "settings")) { Client *c = WM_OK ? client_of_app(w) : NULL; if (c) wm_focus(c); else appwin_activate(w); return; } } app_open("settings", NULL); }
static void sc_start(void) { menu_toggle(); }
static void ov_toggle(void);
static void sc_close(void) { Client *c = act_win(); if (c) wm_close(c); }
static void sc_max(void) { Client *c = act_win(); if (c && !c->max) { wm_focus(c); wm_maximise(c, 1); } }
static void sc_min(void) { Client *c = act_win(); if (!c) return; if (c->max) wm_maximise(c, 0); else wm_minimise(c); }
static void sc_wsl(void) { ws_go(WSC - 1); }
static void sc_wsr(void) { ws_go(WSC + 1); }
static void sc_wsm(int d) { Client *w = act_win(); if (w) { ws_move(w, w->ws + d); ws_go(w->ws); wm_focus(w); } }
static void sc_wsml(void) { sc_wsm(-1); }
static void sc_wsmr(void) { sc_wsm(1); }
static void sc_notif(void) { fl_open(FL_NC); }
static void sc_quick(void) { fl_open(FL_QS); }
static void sc_help(void) { app_open("help", NULL); }
/* (1.6.0) the rest of Standard's global keys (apps25a.js, a11y25.js, apps25b.js): Task Manager, the screen reader, the magnifier, and those
   the page in front does (the next keyboard layout, clipboard history, the emoji panel, a screenshot of an area): on Native the keys
   with Meta come to the shell, so it gives them to the page of the window in front (__oaShort), as Standard's one page has them */
static void sc_taskmgr(void) { tm_urgent(); }
static void sc_sr(void) { qs_toggle(3); }
static void sc_magin(void) { mag_zoom(1); }
static void sc_magout(void) { mag_zoom(-1); }
static void sc_magoff(void) { S.mag = 0; mag_set(0); state_write(); }
static void sc_page(const char *id)
{
	Client *f = FOCUS; AppWin *a = f && f->app ? f->app : NULL; if (!a && !WM_OK) a = ACTIVE; if (!a || !a->view || a->hidden) return;
	char js[96]; snprintf(js, sizeof js, "window.__oaShort&&__oaShort('%s')", id); webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(a->view), js, -1, NULL, NULL, NULL, NULL, NULL);
}
static void sc_kbl(void) { sc_page("kblnext"); }
static void sc_clip(void) { sc_page("clip"); }
static void sc_emoji(void) { sc_page("emoji"); }
static void sc_shotsel(void) { sc_page("shotsel"); }
static void sc_shot(void) { app_open_ex("do-short", NULL, "&id=shot", 1); }   /* (the whole screen, rudolfd's screenshot, kept as the page keeps it) */
/* (shell25.js's order; F11, the page's own full screen, is not the shell's: on Native the window in front keeps it) */
static const Sc SC[] = {
	{"files", "Meta+E", sc_files}, {"desktop", "Meta+D", sc_desktop}, {"run", "Meta+R", sc_run}, {"lock", "Meta+L", sc_lock}, {"settings", "Meta+I", sc_settings},
	{"start", "Ctrl+Escape", sc_start}, {"overview", "Meta+Tab", ov_toggle}, {"close", "Alt+F4", sc_close}, {"max", "Meta+ArrowUp", sc_max}, {"min", "Meta+ArrowDown", sc_min},
	{"wsl", "Ctrl+Alt+ArrowLeft", sc_wsl}, {"wsr", "Ctrl+Alt+ArrowRight", sc_wsr}, {"wsml", "Ctrl+Alt+Shift+ArrowLeft", sc_wsml}, {"wsmr", "Ctrl+Alt+Shift+ArrowRight", sc_wsmr},
	{"notif", "Meta+N", sc_notif}, {"quick", "Meta+A", sc_quick}, {"help", "F1", sc_help},
	{"taskmgr", "Ctrl+Shift+Escape", sc_taskmgr}, {"sr", "Ctrl+Alt+Z", sc_sr}, {"magin", "Meta+=", sc_magin}, {"magout", "Meta+-", sc_magout}, {"magoff", "Meta+Escape", sc_magoff},
	{"kblnext", "Meta+Space", sc_kbl}, {"clip", "Meta+V", sc_clip}, {"shot", "PrintScreen", sc_shot}, {"shotsel", "Meta+Shift+S", sc_shotsel}, {"emoji", "Meta+.", sc_emoji} };
#define NSC (int)G_N_ELEMENTS(SC)
static char SCKB[NSC][48];   /* (the account's own keys: the combination; \1: the default; "": one the shell cannot take) */
static unsigned SCMOD[NSC]; static KeyCode SCKC[NSC];
/* comboOf's "Ctrl+Alt+Shift+Meta+KEY" (e.key, a letter in capitals) as X's modifiers and key */
static int sc_parse(const char *s, unsigned *mod, KeySym *ks)
{
	*mod = 0; *ks = NoSymbol; if (!s || !*s) return 0;
	char b[48]; snprintf(b, sizeof b, "%s", s); char *k = b;
	for (;;) { char *p = strchr(k, '+'); if (!p || !p[1]) break; *p = 0;
		if (!strcmp(k, "Ctrl")) *mod |= ControlMask; else if (!strcmp(k, "Alt")) *mod |= Mod1Mask; else if (!strcmp(k, "Shift")) *mod |= ShiftMask; else if (!strcmp(k, "Meta")) *mod |= Mod4Mask; else return 0;
		k = p + 1; }
	static const char *const P[][2] = {{".", "period"}, {",", "comma"}, {"/", "slash"}, {"-", "minus"}, {"=", "equal"}, {";", "semicolon"}, {"'", "apostrophe"}, {"[", "bracketleft"},
		{"]", "bracketright"}, {"\\", "backslash"}, {"`", "grave"}, {"Space", "space"}, {"Enter", "Return"}, {"Backspace", "BackSpace"}, {"PageUp", "Prior"}, {"PageDown", "Next"},
		{"PrintScreen", "Print"}, {"ContextMenu", "Menu"}};
	for (unsigned i = 0; i < G_N_ELEMENTS(P); i++) if (!strcmp(k, P[i][0])) { *ks = XStringToKeysym(P[i][1]); return *ks != NoSymbol; }
	if (!strncmp(k, "Arrow", 5)) k += 5;
	if (strlen(k) == 1 && g_ascii_isalpha(*k)) { char l[2] = { g_ascii_tolower(*k), 0 }; *ks = XStringToKeysym(l); }
	else *ks = XStringToKeysym(k);
	return *ks != NoSymbol;
}
static int SC_LOADED;
static const char *const HWK[] = { "XF86AudioRaiseVolume", "XF86AudioLowerVolume", "XF86AudioMute", "XF86AudioMicMute", "XF86MonBrightnessUp", "XF86MonBrightnessDown", "XF86PowerOff", NULL };
static int hw_key(KeySym ks, unsigned st)
{
	const char *n = XKeysymToString(ks); if (!n) return 0;
	if (ks == XK_Delete && st == (ControlMask | Mod1Mask)) { tm_urgent(); return 1; }
	int i = 0; for (; HWK[i] && strcmp(HWK[i], n); i++) ; if (!HWK[i]) return 0;
	if (!strcmp(n, "XF86PowerOff")) { if (!(PWR && gtk_widget_get_visible(PWR))) power_dialog(); return 1; }
	if (!USER_ID[0] || LOCKED) return 1;
	if (!strcmp(n, "XF86AudioRaiseVolume")) hk_post("/api/volume?step=up", 0);
	else if (!strcmp(n, "XF86AudioLowerVolume")) hk_post("/api/volume?step=down", 0);
	else if (!strcmp(n, "XF86AudioMute")) hk_post("/api/volume?step=mute", 0);
	else if (!strcmp(n, "XF86MonBrightnessUp")) hk_post("/api/brightness/step?d=up", 1);
	else if (!strcmp(n, "XF86MonBrightnessDown")) hk_post("/api/brightness/step?d=down", 1);
	else if (!strcmp(n, "XF86AudioMicMute")) { char q[40]; snprintf(q, sizeof q, "/api/mic?muted=%d", !MIC_MUTED); MIC_MUTED = !MIC_MUTED; hk_post(q, 3); osd_show("mic", "", MIC_MUTED ? "Microphone off" : "Microphone on", MIC_MUTED ? "1" : "0"); }
	return 1;
}
static const char *sc_combo(int i) { return !SC_LOADED || SCKB[i][0] == 1 ? SC[i].def : SCKB[i]; }   /* (\1: the default) */
/* (1.6.0, the owner's computer check on the Vivobook) the keys a page asks to have to itself (op "kbcap"): the keyboard step of Task
   Manager's Check this computer, Settings › Keyboard's "Press keys…". On Standard the page's own shortcut handler sees every key first and
   those two take it there (the check's listener still sees F1 and Meta; Settings' capture is skipped by the handler); on Native the
   shell's grabs took the keys from the page (F1 opened Help in front of the check, Super the start menu, and the keys after them went
   there: 19 keys "did not answer"). While the window that asked is in front, the shell grabs none of its shortcuts and Super alone
   opens nothing; the hardware keys and Ctrl+Alt+Del stay the shell's, as rudolf-display keeps them before Chromium on Standard. */
static AppWin *KBCAP; static int AT_ON, OV_ON;
static int kb_free(void) { return KBCAP && g_list_find(WINS, KBCAP) && FOCUS && FOCUS->app == KBCAP; }
/* (1.6.0) Super by itself, as the page's metaAlone: seen from XInput 2's raw key events on the root window (every key, whichever
   window has the keyboard, without grabbing it), so the window in front gets Super's own press and release as a page does on
   Standard (the check's ❖ never answered: the grab kept it from every app). Without XInput 2: grabbed, as before. */
static int XI_OP = -1; static int SUPER_GRAB = 1;
static void xi_raw_init(void)
{
	typedef int (*XiSel)(Display *, Window, XIEventMask *, int);
	XiSel f = (XiSel)dlsym(RTLD_DEFAULT, "XISelectEvents");
	if (!f) { void *h = dlopen("libXi.so.6", RTLD_NOW | RTLD_LOCAL); if (h) f = (XiSel)dlsym(h, "XISelectEvents"); }
	int op, ev, er; if (!f || !XQueryExtension(DPY, "XInputExtension", &op, &ev, &er)) { fprintf(stderr, "rudolf-native: no XInput 2: Super is grabbed\n"); return; }
	unsigned char m[XIMaskLen(XI_LASTEVENT)] = {0}; XISetMask(m, XI_RawKeyPress); XISetMask(m, XI_RawKeyRelease);
	XIEventMask em = { XIAllMasterDevices, sizeof m, m };
	if (f(DPY, ROOT, &em, 1) == Success) { XI_OP = op; SUPER_GRAB = 0; }
}
static int META_ALONE;
static int KB_GRABBED = -1;   /* (the grabs as they are: 1 the shortcuts', 0 none while a page has the keys) */
static void sc_grab(void)
{
	static const unsigned M[4] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
	static KeyCode sl0, sr0, tb0, gr0;
	for (int i = 0; i < NSC; i++) { if (SCKC[i]) for (int m = 0; m < 4; m++) XUngrabKey(DPY, SCKC[i], SCMOD[i] | M[m], ROOT); SCKC[i] = 0; }
	for (int m = 0; m < 4; m++) { if (sl0) XUngrabKey(DPY, sl0, M[m], ROOT); if (sr0) XUngrabKey(DPY, sr0, M[m], ROOT);
		for (int s = 0; s < 2; s++) { if (tb0) XUngrabKey(DPY, tb0, Mod1Mask | (s ? ShiftMask : 0) | M[m], ROOT); if (gr0) XUngrabKey(DPY, gr0, Mod1Mask | (s ? ShiftMask : 0) | M[m], ROOT); } }
	sl0 = sr0 = tb0 = gr0 = 0;
	/* (1.6.0) the hardware keys and Ctrl+Alt+Del, whichever window is in front: on the computer rudolf-display takes them before X does
	   (and rudolfd's events bring them here), but where X has them (QEMU, xdotool, a keyboard X reads itself) the shell does what
	   Standard does: the volume and the brightness a step (rudolfd, as the keys' hotkey), the microphone muted, the power dialog, Task Manager */
	static int hw; if (!hw) { hw = 1;
		for (int i = 0; HWK[i]; i++) { KeyCode kc = XKeysymToKeycode(DPY, XStringToKeysym(HWK[i])); if (kc) for (int m = 0; m < 4; m++) XGrabKey(DPY, kc, M[m], ROOT, True, GrabModeAsync, GrabModeAsync); }
		{ KeyCode kc = XKeysymToKeycode(DPY, XK_Delete); if (kc) for (int m = 0; m < 4; m++) XGrabKey(DPY, kc, ControlMask | Mod1Mask | M[m], ROOT, True, GrabModeAsync, GrabModeAsync); } }
	KB_GRABBED = !kb_free();
	if (!KB_GRABBED) return;   /* (a page has the keys: kbcap) */
	/* (only the exact combinations: each with its modifiers, Caps Lock and Num Lock either way; every other key, a bare arrow or F-key,
	   Ctrl, Alt, reaches the window in front as on Standard) */
	for (int i = 0; i < NSC; i++) { KeySym ks; unsigned mod; if (!sc_parse(sc_combo(i), &mod, &ks)) continue; KeyCode kc = XKeysymToKeycode(DPY, ks); if (!kc) continue;
		SCKC[i] = kc; SCMOD[i] = mod; for (int m = 0; m < 4; m++) XGrabKey(DPY, kc, mod | M[m], ROOT, True, GrabModeAsync, GrabModeAsync); }
	/* (with the keys sent to the root window while held, not to the window in front: GTK's own windows take keys through XInput 2) */
	/* (Super by itself: the start menu when it is let go with nothing pressed between, from the raw events (xi_raw_init); Alt+Tab, Alt+`
	   and with Shift: the switcher) */
	KeyCode sl = XKeysymToKeycode(DPY, XK_Super_L), sr = XKeysymToKeycode(DPY, XK_Super_R), tb = XKeysymToKeycode(DPY, XK_Tab), gr = XKeysymToKeycode(DPY, XK_grave);
	if (!SUPER_GRAB) sl = sr = 0;
	sl0 = sl; sr0 = sr; tb0 = tb; gr0 = gr;
	for (int m = 0; m < 4; m++) { if (sl) XGrabKey(DPY, sl, M[m], ROOT, False, GrabModeAsync, GrabModeAsync); if (sr) XGrabKey(DPY, sr, M[m], ROOT, False, GrabModeAsync, GrabModeAsync);
		for (int s = 0; s < 2; s++) { if (tb) XGrabKey(DPY, tb, Mod1Mask | (s ? ShiftMask : 0) | M[m], ROOT, True, GrabModeAsync, GrabModeAsync); if (gr) XGrabKey(DPY, gr, Mod1Mask | (s ? ShiftMask : 0) | M[m], ROOT, True, GrabModeAsync, GrabModeAsync); } }
}
/* (the grabs again when the window in front changes to or from one that has the keys) */
static void kb_sync(void) { if (!WM_OK) return; if (!kb_free() && KBCAP && !g_list_find(WINS, KBCAP)) KBCAP = NULL; if (KB_GRABBED != !kb_free()) { sc_grab(); state_write(); } }
static void kbcap_gone(AppWin *a) { if (KBCAP == a) { KBCAP = NULL; kb_sync(); } }
static void kbcap_msg(AppWin *a, int on) { if (on) KBCAP = a; else if (KBCAP == a) KBCAP = NULL; META_ALONE = 0; kb_sync(); }
/* (a raw key: Super alone, as the page's metaAlone: any other key pressed between cancels it; a repeat is not a press) */
static void xi_raw(XIRawEvent *r, int press)
{
	if (r->flags & XIKeyRepeat) return;
	KeySym ks = XkbKeycodeToKeysym(DPY, (KeyCode)r->detail, 0, 0); int sup = ks == XK_Super_L || ks == XK_Super_R;
	if (press) { META_ALONE = sup && !kb_free(); return; }
	if (sup && META_ALONE) { META_ALONE = 0; if (USER_ID[0] && !LOCKED && !DGK && !AT_ON && !kb_free()) sc_start(); }
}
/* (rudolfd's desktop/settings "kb": the account's bd95kb:ID, {id: "Ctrl+...", id: ""}) */
static void sc_load(JV *v)
{
	char old[NSC][48]; memcpy(old, SCKB, sizeof old);
	for (int i = 0; i < NSC; i++) { SCKB[i][0] = 1; SCKB[i][1] = 0; }
	JV *kb = jprop(v, "kb");
	for (int i = 0; kb && kb->t == 5 && i < kb->len; i++) { JV *o = kb->v[i]; if (!o || o->t != 3 || strlen(o->s) >= 48) continue;
		for (int k = 0; k < NSC; k++) if (!strcmp(SC[k].id, kb->k[i]) && o->s[0]) { snprintf(SCKB[k], 48, "%s", o->s); unsigned m; KeySym ks; if (!sc_parse(o->s, &m, &ks)) SCKB[k][0] = 0; } }   /* ("": the default, as kbOf) */
	/* (a key given to another shortcut is that one's: Settings takes it from the first, as kbMap) */
	int was = SC_LOADED; SC_LOADED = 1; if (WM_OK && (!was || memcmp(old, SCKB, sizeof old))) sc_grab();
}
static void at_key(int back); static void at_end(int pick); static void ov_end(Client *pick);
/* the keys the shell has (wm_filter): 1 when taken */
static int sc_key(XKeyEvent *e)
{
	KeySym ks = XkbKeycodeToKeysym(DPY, e->keycode, 0, 0); unsigned st = e->state & (ControlMask | Mod1Mask | ShiftMask | Mod4Mask);
	if (e->type == KeyRelease) {
		if ((ks == XK_Super_L || ks == XK_Super_R) && META_ALONE && SUPER_GRAB) { META_ALONE = 0; if (USER_ID[0] && !LOCKED && !DGK && !kb_free()) sc_start(); return 1; }
		if (AT_ON && (ks == XK_Alt_L || ks == XK_Alt_R || ks == XK_Meta_L || !(e->state & Mod1Mask))) { at_end(1); return 1; }
		return SUPER_GRAB && (ks == XK_Super_L || ks == XK_Super_R);
	}
	if (SUPER_GRAB && (ks == XK_Super_L || ks == XK_Super_R)) { META_ALONE = 1; return 1; }
	META_ALONE = 0;
	if (AT_ON && ks == XK_Escape) { at_end(0); return 1; }
	if (OV_ON && ks == XK_Escape) { ov_end(NULL); return 1; }
	if ((ks == XK_Tab || ks == XK_grave || ks == XK_ISO_Left_Tab) && (st & Mod1Mask) && !(st & ControlMask)) { if (USER_ID[0] && !LOCKED) at_key((st & ShiftMask) != 0); return 1; }
	if (AT_ON) return 1;
	if (hw_key(ks, st)) return 1;
	for (int i = 0; i < NSC; i++) { if (!SCKC[i] || SCKC[i] != e->keycode || SCMOD[i] != st) continue;
		/* (Alt+F4 on Oracle's window: Chromium's own, as on Standard: 12.3c) */
		if (!strcmp(SC[i].id, "close") && FOCUS && FOCUS->ak[0] && !FOCUS->app && oracle_key(e)) return 1;
		if (USER_ID[0] && !LOCKED && !DGK) SC[i].fn();
		return 1; }
	return 0;
}

/* ---- Alt+Tab (shell25.js SWT, altDraw; styles25.css #altab, .atc): in the middle of the screen at 40 % of its height, 14 px inside,
   rgba(15,23,42,.92) with 14 px corners; each window 120 px wide in 10 px of padding and a 2 px edge, 10 px apart (wrapping at 90 % of
   the screen's width): its app's picture (34 px), its title (12 px, cut with an ellipsis) and "workspace N" (10 px, at .6); the one
   chosen in the accent colour, its words white */
static GtkWidget *AWIN, *AAREA; static GdkPixbuf *ABG; static int AT_I, AT_N, AX, AY, AW, AH; static double AFX, AFY; static Client *AT_L[64];
static int at_per(void) { int per = (int)floor((SW / Z * 0.9 - 28 + 10) / 154); return per < 1 ? 1 : per; }
static void at_size(double *w, double *h)
{
	int per = at_per(), n = AT_N, rows = (n + per - 1) / per, cols = n < per ? n : per;
	*w = 28 + cols * 154 - 10; *h = 28 + rows * 103 - 10;
}
static const char *at_icon(Client *c)
{
	const char *k = c->app ? c->app->k : c->ak; const App *a = k[0] ? app_find(k) : NULL;
	if (c->app && c->app->icon[0]) return c->app->icon; return a && a->icon[0] ? a->icon : "▢";
}
static void at_rect(int i, double *x, double *y)
{
	double w, h; at_size(&w, &h); int per = at_per(), r = i / per, col = i % per, k = AT_N - r * per; if (k > per) k = per;
	*x = 14 + (w - 28 - (k * 154 - 10)) / 2 + col * 154; *y = 14 + r * 103;
}
/* (pass 0: the boxes, where translate(-50%,-50%) puts them, on half a pixel; pass 1: the words and pictures, which Chromium puts on whole
   pixels) */
static void at_paint(cairo_t *cr, int pass)
{
	double w, h; at_size(&w, &h);
	if (!pass) { srca(cr, cx("#0f172a", .92)); nc_rr(cr, 0, 0, w, h, 14, 14, 14, 14); cairo_fill(cr); }
	for (int i = 0; i < AT_N; i++) { Client *c = AT_L[i]; double x, y; at_rect(i, &x, &y); int on = i == AT_I;
		if (!pass) { if (on) { srca(cr, ca(accent(), 1)); nc_rr(cr, x, y, 144, 93, 10, 10, 10, 10); cairo_fill(cr); } continue; }
		RGBA fg = on ? cx("#ffffff", 1) : cx("#e2e8f0", 1);
		const char *ic = at_icon(c); double iw = css_width(cr, ic, UIFAM, 400, 34); css_text(cr, ic, UIFAM, 400, 34, x + 12 + (120 - iw) / 2, y + 12, 40, fg, 0);
		double tw = css_width(cr, c->title, UIFAM, 400, 12); css_text(cr, c->title, UIFAM, 400, 12, x + 12 + (tw < 120 ? (120 - tw) / 2 : 0), y + 56, 14, fg, tw > 120 ? 120 : 0);
		char ws[48]; snprintf(ws, sizeof ws, "%sworkspace %d", c->min ? "minimized · " : "", ws_of(c)); RGBA f2 = fg; f2.a = on ? 1 : .6;   /* (the chosen one's: opacity 1, the page's highlight rule) */
		double ww = css_width(cr, ws, UIFAM, 400, 10); css_text(cr, ws, UIFAM, 400, 10, x + 12 + (ww < 120 ? (120 - ww) / 2 : 0), y + 70, 11, f2, ww > 120 ? 120 : 0); }
}
static gboolean at_draw(GtkWidget *wd, cairo_t *cr, gpointer d)
{
	(void)wd; (void)d; if (ABG) { gdk_cairo_set_source_pixbuf(cr, ABG, 0, 0); cairo_paint(cr); }
	cairo_save(cr); cairo_translate(cr, AFX, AFY); cairo_scale(cr, Z, Z); at_paint(cr, 0); cairo_restore(cr);
	cairo_translate(cr, round(AFX), round(AFY)); cairo_scale(cr, Z, Z); at_paint(cr, 1); return TRUE;   /* (translate(-50%,-50%): on half a pixel, as Chromium draws it) */
}
static gboolean at_click(GtkWidget *wd, GdkEventButton *e, gpointer d)
{
	(void)wd; (void)d; double x = (e->x - AFX) / Z, y = (e->y - AFY) / Z;
	for (int i = 0; i < AT_N; i++) { double ix, iy; at_rect(i, &ix, &iy); if (x >= ix && x < ix + 144 && y >= iy && y < iy + 93) { AT_I = i; at_end(1); return TRUE; } }
	return TRUE;
}
static void at_show(void)
{
	if (!AWIN) { AWIN = gtk_window_new(GTK_WINDOW_POPUP); gtk_window_set_title(GTK_WINDOW(AWIN), "Switch windows"); AAREA = gtk_drawing_area_new(); gtk_container_add(GTK_CONTAINER(AWIN), AAREA);
		gtk_widget_add_events(AAREA, GDK_BUTTON_PRESS_MASK); g_signal_connect(AAREA, "draw", G_CALLBACK(at_draw), NULL); g_signal_connect(AAREA, "button-press-event", G_CALLBACK(at_click), NULL); }
	double w, h; at_size(&w, &h); double fx = (SW / Z - w) / 2 * Z, fy = (SH / Z * 0.4 - h / 2) * Z; int nw = (int)ceil(w * Z + fx - floor(fx)), nh = (int)ceil(h * Z + fy - floor(fy));
	if (!(gtk_widget_get_visible(AWIN) && ABG && nw == AW && nh == AH)) {
		if (gtk_widget_get_visible(AWIN)) { gtk_widget_hide(AWIN); gdk_display_sync(gdk_display_get_default()); }
		AW = nw; AH = nh; AX = (int)floor(fx); AY = (int)floor(fy);
		if (ABG) g_object_unref(ABG); ABG = screen_copy(AX, AY, AW, AH); }
	AFX = fx - AX; AFY = fy - AY; gtk_widget_set_size_request(AAREA, AW, AH); gtk_window_resize(GTK_WINDOW(AWIN), AW, AH); gtk_window_move(GTK_WINDOW(AWIN), AX, AY);
	gtk_widget_show_all(AWIN); XRaiseWindow(DPY, GDK_WINDOW_XID(gtk_widget_get_window(AWIN))); gtk_widget_queue_draw(AAREA); state_write();
}
static int at_cmp(gconstpointer a, gconstpointer b) { gint64 x = ((const Client *)a)->used, y = ((const Client *)b)->used; return x > y ? -1 : x < y; }
static void at_key(int back)
{
	if (OV_ON) ov_end(NULL);
	if (!AT_ON) {
		GList *L = NULL; for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (tb_win(c) && c->ws) L = g_list_prepend(L, c); }
		L = g_list_sort(L, at_cmp); AT_N = 0; for (GList *l = L; l && AT_N < 64; l = l->next) AT_L[AT_N++] = l->data; g_list_free(L);
		if (!AT_N) return;
		AT_ON = 1; AT_I = 0; XGrabKeyboard(DPY, ROOT, False, GrabModeAsync, GrabModeAsync, CurrentTime);
	}
	AT_I = (AT_I + (back ? -1 : 1) + AT_N) % AT_N; at_show();
}
static void at_end(int pick)
{
	if (!AT_ON) return; AT_ON = 0; XUngrabKeyboard(DPY, CurrentTime); if (AWIN) gtk_widget_hide(AWIN);
	Client *c = pick && AT_I < AT_N ? AT_L[AT_I] : NULL; AT_N = 0;
	if (c && g_list_find(CLIENTS, c)) wm_focus(c); state_write();
}

/* ---- Super+Tab: the overview (shell25.js overview, ovEnd; styles25.css #ovsh, .win.ovw). This workspace's windows (minimised too) in
   a grid of ceil(sqrt(n)) columns over the desktop, each scaled to fit its cell less 40 by 60 px (never larger), centred in it, with a
   3 px ring in the accent colour and the shadow 0 12px 30px rgba(0,0,0,.4); the desktop under them shaded rgba(15,23,42,.55); a click
   on a window brings it to the front, elsewhere (or Escape, or Super+Tab again) goes back. The windows' pictures come from the X
   server's Composite extension (each window kept off screen while the overview is shown), loaded when there. */
typedef struct { Client *c; double x, y, w, h, s; int wasmin; cairo_surface_t *pic; Pixmap pm; int pw, ph; } OvW;
static GtkWidget *OVWIN, *OVAREA; static OvW OVL[32]; static int OVN; static guint OVT;
/* (#desk as the page has it: 42 px above a taskbar at the bottom, 48 px below one at the top, beside one at the left or right) */
static void ov_desk(double *x, double *y, double *w, double *h)
{
	int top = !strcmp(S.tb, "top"); *x = desk_left(); *w = desk_w();
	*y = TOPRES + (top ? lround(48 * Z) : 0); *h = SH - *y - (tb_side() || top ? 0 : lround(42 * Z));
}
static void ov_layout(void)
{
	double dx, dy, dw, dh; ov_desk(&dx, &dy, &dw, &dh); int n = OVN, cols = (int)ceil(sqrt(n)), rows = (n + cols - 1) / cols; double cw = dw / cols, ch = dh / rows;
	for (int i = 0; i < n; i++) { OvW *o = &OVL[i]; double w = fw_of(o->c), h = fh_of(o->c); double s = fmin(fmin((cw - 40 * Z) / w, (ch - 60 * Z) / h), 1);
		double cxx = (i % cols) * cw + cw / 2, cyy = (i / cols) * ch + ch / 2; o->s = s; o->w = w * s; o->h = h * s; o->x = dx + cxx - o->w / 2; o->y = dy + cyy - o->h / 2; }
}
/* (each window's picture: its pixmap under Composite, which the X server keeps up to date as the window draws itself, so a
   video or a clock in a window goes on moving in the overview as the page's own windows do on Standard (the test findings'
   "overview's pictures without a compositor": they were copied once when it opened); named again each time, as the server may give
   the window a new one) */
static void ov_free(OvW *o)
{
	if (o->pic) { cairo_surface_destroy(o->pic); o->pic = NULL; } if (o->pm) { XFreePixmap(DPY, o->pm); o->pm = 0; } o->pw = o->ph = 0;
}
static void ov_grab(void)
{
	for (int i = 0; i < OVN; i++) { OvW *o = &OVL[i];
		XWindowAttributes wa; if (!xc_ok() || !g_list_find(CLIENTS, o->c) || !XGetWindowAttributes(DPY, o->c->frame, &wa) || wa.map_state != IsViewable) { ov_free(o); continue; }
		ov_free(o); Pixmap pm = XC_NAME(DPY, o->c->frame); if (!pm) continue;
		o->pm = pm; o->pw = wa.width; o->ph = wa.height; o->pic = cairo_xlib_surface_create(DPY, pm, wa.visual, wa.width, wa.height); }
}
static gboolean ov_draw(GtkWidget *wd, cairo_t *cr, gpointer d)
{
	(void)wd; (void)d; double ox, oy, ow, oh; ov_desk(&ox, &oy, &ow, &oh);
	cairo_translate(cr, -ox, -oy);
	if (DESKC) { cairo_set_source_surface(cr, DESKC, 0, 0); cairo_paint(cr); } wg_paint(cr);
	srca(cr, cx("#0f172a", .55)); cairo_rectangle(cr, ox, oy, ow, oh); cairo_fill(cr);
	double r0 = tw_rad() * Z;
	for (int i = 0; i < OVN; i++) { OvW *o = &OVL[i]; double s = o->s, r = r0 * s;
		/* (the shadow, then the ring: box-shadow's second, then its first) */
		int pad = (int)ceil(40 * Z * s) + 4; cairo_surface_t *m = cairo_image_surface_create(CAIRO_FORMAT_A8, (int)ceil(o->w) + 2 * pad, (int)ceil(o->h) + 2 * pad); cairo_t *mc = cairo_create(m);
		rrect(mc, pad, pad, o->w, o->h, r); cairo_set_source_rgba(mc, 0, 0, 0, 1); cairo_fill(mc); cairo_destroy(mc); a8_blur(m, 15 * Z * s);
		cairo_save(cr); cairo_set_source_rgba(cr, 0, 0, 0, .4); cairo_mask_surface(cr, m, round(o->x - pad), round(o->y - pad + 12 * Z * s)); cairo_restore(cr); cairo_surface_destroy(m);
		srca(cr, ca(accent(), 1)); rrect(cr, o->x - 3 * Z * s, o->y - 3 * Z * s, o->w + 6 * Z * s, o->h + 6 * Z * s, r + 3 * Z * s); cairo_fill(cr);
		cairo_save(cr); rrect(cr, o->x, o->y, o->w, o->h, r); cairo_clip(cr);
		if (o->pic) { cairo_translate(cr, o->x, o->y); cairo_scale(cr, s, s); cairo_set_source_surface(cr, o->pic, 0, 0); cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD); cairo_paint(cr); }
		else { srca(cr, cx("#ffffff", 1)); cairo_paint(cr); }
		cairo_restore(cr); }
	return TRUE;
}
static gboolean ov_click(GtkWidget *wd, GdkEventButton *e, gpointer d)
{
	(void)wd; (void)d; double ox, oy, ow, oh; ov_desk(&ox, &oy, &ow, &oh); double x = e->x + ox, y = e->y + oy; Client *pick = NULL;
	for (int i = OVN - 1; i >= 0 && !pick; i--) { OvW *o = &OVL[i]; if (x >= o->x && x < o->x + o->w && y >= o->y && y < o->y + o->h) pick = o->c; }
	ov_end(pick); return TRUE;
}
static gboolean ov_live(gpointer d) { (void)d; if (!OV_ON) { OVT = 0; return G_SOURCE_REMOVE; } ov_grab(); if (OVAREA) gtk_widget_queue_draw(OVAREA); return G_SOURCE_CONTINUE; }
static gboolean ov_shot(gpointer d) { (void)d; OVT = 0; if (!OV_ON) return G_SOURCE_REMOVE; ov_grab(); if (OVAREA) gtk_widget_queue_draw(OVAREA); state_write();
	OVT = g_timeout_add(40, ov_live, NULL); return G_SOURCE_REMOVE; }   /* (then drawn again 25 times a second, the windows' pictures as they are) */
static void ov_start(void)
{
	OVN = 0; for (GList *l = CLIENTS; l && OVN < 32; l = l->next) { Client *c = l->data; if (tb_win(c) && c->ws && ws_of(c) == WSC) { memset(&OVL[OVN], 0, sizeof OVL[0]); OVL[OVN++].c = c; } }
	if (!OVN) { const char *f[3] = {"No windows are open on this workspace", "", "5000"}; nt_notef(fget_tt, (void *)f); toast_text(f[0], "", 5000); return; }
	if (AT_ON) at_end(0); if (MENU && gtk_widget_get_visible(MENU)) menu_hide(); if (FLK) fl_close();
	OV_ON = 1; ov_layout();
	if (!OVWIN) { OVWIN = gtk_window_new(GTK_WINDOW_POPUP); gtk_window_set_title(GTK_WINDOW(OVWIN), "Show all windows"); OVAREA = gtk_drawing_area_new(); gtk_container_add(GTK_CONTAINER(OVWIN), OVAREA);
		gtk_widget_add_events(OVAREA, GDK_BUTTON_PRESS_MASK); g_signal_connect(OVAREA, "draw", G_CALLBACK(ov_draw), NULL); g_signal_connect(OVAREA, "button-press-event", G_CALLBACK(ov_click), NULL); }
	{ double ox, oy, ow, oh; ov_desk(&ox, &oy, &ow, &oh); gtk_widget_set_size_request(OVAREA, (int)ow, (int)oh); gtk_window_resize(GTK_WINDOW(OVWIN), (int)ow, (int)oh); gtk_window_move(GTK_WINDOW(OVWIN), (int)ox, (int)oy); }
	gtk_widget_show_all(OVWIN); XRaiseWindow(DPY, GDK_WINDOW_XID(gtk_widget_get_window(OVWIN)));
	/* (under the overview: each window off screen, the minimised ones shown there too, as the page shows them) */
	for (int i = 0; i < OVN; i++) { OvW *o = &OVL[i]; comp_ref(o->c->frame); if (o->c->min) { o->wasmin = 1; XMapWindow(DPY, o->c->frame); } }
	XGrabKeyboard(DPY, ROOT, False, GrabModeAsync, GrabModeAsync, CurrentTime);
	XRaiseWindow(DPY, GDK_WINDOW_XID(gtk_widget_get_window(OVWIN))); XSync(DPY, False);
	if (OVT) g_source_remove(OVT); OVT = g_timeout_add(250, ov_shot, NULL); desk_shadows(); state_write();
}
static void ov_end(Client *pick)
{
	if (!OV_ON) return; OV_ON = 0; if (OVT) { g_source_remove(OVT); OVT = 0; } XUngrabKeyboard(DPY, CurrentTime);
	for (int i = 0; i < OVN; i++) { OvW *o = &OVL[i]; if (!g_list_find(CLIENTS, o->c)) { ov_free(o); continue; } comp_unref(o->c->frame);
		if (o->wasmin && o->c != pick && o->c->min) XUnmapWindow(DPY, o->c->frame); ov_free(o); }
	OVN = 0; if (OVWIN) gtk_widget_hide(OVWIN); desk_shadows();
	if (pick && g_list_find(CLIENTS, pick)) wm_focus(pick); state_write();
}
static void ov_toggle(void) { if (OV_ON) ov_end(NULL); else ov_start(); }

/* ------------------------------------------------------------------ the screen saver and its lock (1.6.0 Part 12, item 6)
   core25.js idleCheck, every 15 s: the time since the last input (rudolfd's idle, rudolf-display's count), the settings of
   Display Properties › Screen saver and Power (bd95sys: sson, ssmin, ssstyle, sstime, lockss, lockmin, suspmin), and what
   holds it off (rudolfd's media: sound playing, a microphone in use, an app keeping the screen on; something full screen; a
   minute after unlocking). The saver itself is still the page's (saver31.js ssRun) in a full-screen window of its own
   (#app=do-saver&style=NAME), above everything; input (rudolfd's idle under 1.5 s, checked every second, as ssRun) ends
   it, and the lock screen follows when it ran more than 3 s with "lock when the screen saver ends" (saverStop). Locking
   after lockmin and sleeping after the power source's time (Power and battery, else suspmin) as the page does. */
static struct { int sson, ssmin, lockss, lockmin, suspmin, sstime; char ssstyle[24]; } SYS = { 0, 10, 1, 0, 0, 1, "stars" };
static void sys_load(JV *v)
{
	JV *o = jprop(v, "sys"); JV *x;
	SYS.ssmin = (int)jnum(o, "ssmin", 10); SYS.lockmin = (int)jnum(o, "lockmin", 0); SYS.suspmin = (int)jnum(o, "suspmin", 0);
	x = jprop(o, "sson"); SYS.sson = x ? jbool(o, "sson") : SYS.ssmin > 0;   /* (core25.js: sson unset is ssmin > 0) */
	if (!SYS.ssmin) SYS.ssmin = 10;
	x = jprop(o, "lockss"); SYS.lockss = x ? jbool(o, "lockss") : 1;
	x = jprop(o, "sstime"); SYS.sstime = x ? jbool(o, "sstime") : 1;
	char st[24] = ""; jstr(o, "ssstyle", st, sizeof st); snprintf(SYS.ssstyle, sizeof SYS.ssstyle, "%s", st[0] && strspn(st, "abcdefghijklmnopqrstuvwxyz") == strlen(st) ? st : "stars");
}
static int SAV_PREV;   /* (Display Properties' Preview: ends on input as the saver does, never locking) */
static int SAV_ON, IDLE_N; static gint64 SAV_T; static guint SAV_POLL; static char SAV_WHY[96];
static AppWin *sav_win(void) { for (GList *l = WINS; l; l = l->next) { AppWin *a = l->data; if (!strcmp(a->k, "do-saver") && !a->closing) return a; } return NULL; }
static long long idle_ms(void) { JV *v = jget("/api/idle"); long long m = v ? (long long)jnum(v, "idle", -1) : -1; jfree(v); return m; }
static void sav_stop(int may_lock)
{
	if (!SAV_ON) return; SAV_ON = 0; if (SAV_POLL) { g_source_remove(SAV_POLL); SAV_POLL = 0; }
	AppWin *a = sav_win(); if (a) appwin_close_now(a);
	int lock = may_lock && !SAV_PREV && SYS.lockss && g_get_monotonic_time() - SAV_T > 3000000; SAV_PREV = 0;
	if (lock && !LOCKED) power("lock");
	state_write();
}
static gboolean sav_poll(gpointer d)
{
	(void)d; if (!SAV_ON) { SAV_POLL = 0; return G_SOURCE_REMOVE; }
	if (!sav_win() && g_get_monotonic_time() - SAV_T > 20000000) { SAV_ON = 0; SAV_POLL = 0; snprintf(SAV_WHY, sizeof SAV_WHY, "a screen saver whose picture was gone was ended"); state_write(); return G_SOURCE_REMOVE; }
	long long m = idle_ms(); if (m >= 0 && m < 1500 && g_get_monotonic_time() - SAV_T > 1500000) { SAV_POLL = 0; sav_stop(1); return G_SOURCE_REMOVE; }
	return G_SOURCE_CONTINUE;
}
static void sav_start(void)
{
	if (SAV_ON || !USER_ID[0] || LOCKED) return; SAV_ON = 1; SAV_T = g_get_monotonic_time();
	if (AT_ON) at_end(0); if (OV_ON) ov_end(NULL); if (MENU && gtk_widget_get_visible(MENU)) menu_hide(); if (FLK) fl_close();
	char ex[80]; snprintf(ex, sizeof ex, "&style=%s&time=%d", SYS.ssstyle, SYS.sstime); app_open_ex("do-saver", NULL, ex, 0);
	if (!SAV_POLL) SAV_POLL = g_timeout_add(1000, sav_poll, NULL); state_write();
}
static void sav_preview(const char *style)
{
	if (SAV_ON || !USER_ID[0] || LOCKED) return; int ok = style[0] && strlen(style) <= 24; for (const char *c = style; *c; c++) if (!g_ascii_isalnum(*c) && *c != '-' && *c != '_') ok = 0;
	SAV_ON = 1; SAV_PREV = 1; SAV_T = g_get_monotonic_time();
	if (MENU && gtk_widget_get_visible(MENU)) menu_hide(); if (FLK) fl_close();
	char ex[80]; snprintf(ex, sizeof ex, "&style=%s&time=%d", ok ? style : SYS.ssstyle, SYS.sstime); app_open_ex("do-saver", NULL, ex, 0);
	if (!SAV_POLL) SAV_POLL = g_timeout_add(1000, sav_poll, NULL); state_write();
}
static int full_shown(void) { for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (c->full && !c->min && !c->off && !(c->app && (!strcmp(c->app->k, "do-saver") || c->app->modal))) return 1; } return 0; }
static gboolean tick_idle(gpointer d)
{
	(void)d; if (!USER_ID[0] || !WM_OK) return G_SOURCE_CONTINUE;
	long long ms = idle_ms(); if (ms < 0) return G_SOURCE_CONTINUE; IDLE_N++;
	JV *m = jget("/api/media"); int busy = m && (jnum(m, "playing", 0) > 0 || jnum(m, "mic", 0) > 0 || jnum(m, "idleHeld", 0) > 0), held = m && jnum(m, "idleHeld", 0) > 0, sheld = m && jnum(m, "sleepHeld", 0) > 0; jfree(m);
	double idle = ms / 60000.0; int fresh = UNLOCK_T && g_get_monotonic_time() - UNLOCK_T < 60000000;
	if (SYS.sson && SYS.ssmin > 0 && idle >= SYS.ssmin && !SAV_ON) {
		const char *why = LOCKED || fresh ? "" : full_shown() ? "something is full screen" : busy ? (held ? "an app asked to keep the screen on" : "sound is playing or a microphone is in use") : NULL;
		if (why) { if (why[0]) snprintf(SAV_WHY, sizeof SAV_WHY, "the screen saver waits: %s", why); } else { SAV_WHY[0] = 0; sav_start(); } }
	if (SYS.lockmin > 0 && idle >= SYS.lockmin && !LOCKED && !fresh && !held) { sav_stop(0); power("lock"); }
	int sm = SYS.suspmin; JV *p = jget("/api/power/status"); JV *now = jprop(p, "now"); JV *sl = jprop(now, "sleep"); if (sl && sl->t == 2) sm = (int)sl->n; jfree(p);
	if (sm > 0 && idle >= sm && !busy && !held && !sheld && !fresh) { sav_stop(0); if (!LOCKED) power("lock"); http("POST", "/api/power?action=suspend", NULL, 0, NULL); }
	state_write(); return G_SOURCE_CONTINUE;
}

/* ------------------------------------------------------------------ the clipboard (1.5.9, 12.3c)
   On Standard Chromium is the desktop and never closes, so what was copied in Oracle stays on the clipboard; here Oracle's Chromium
   goes with its last window, and its clipboard with it. While it runs, what is copied is kept here (text) and put back on the
   clipboard when Chromium has gone and nothing else holds it. */
static char *CLIPKEEP;
static void clip_got(GtkClipboard *cb, const gchar *t, gpointer d) { (void)cb; (void)d; if (t && OR_RUN) { g_free(CLIPKEEP); CLIPKEEP = g_strdup(t); } }
static void clip_owner(GtkClipboard *cb, GdkEvent *e, gpointer d)
{
	(void)d; GdkEventOwnerChange *o = (GdkEventOwnerChange *)e;
	if (o->reason == GDK_OWNER_CHANGE_NEW_OWNER) { if (OR_RUN) gtk_clipboard_request_text(cb, clip_got, NULL); }
	else if (!OR_RUN) clip_keep_restore();
}
static void clip_keep_restore(void)
{
	if (!CLIPKEEP || OR_RUN) return;
	Display *x = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
	if (XGetSelectionOwner(x, XInternAtom(x, "CLIPBOARD", False)) == None) gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), CLIPKEEP, -1);
	g_free(CLIPKEEP); CLIPKEEP = NULL;
}
static void clip_init(void) { g_signal_connect(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), "owner-change", G_CALLBACK(clip_owner), NULL); }
static int toast_hit(double rx, double ry, int *why)
{
	double x = rx / Z, y = ry / Z; *why = 0;
	for (int i = 0; i < NTST; i++) if (x >= TSX[i] && x < TSX[i] + 349 && y >= TSY[i] && y < TSY[i] + TST[i].h) { if (TST[i].why && x >= TWHY[i][0] && x < TWHY[i][0] + TWHY[i][2] && y >= TWHY[i][1] && y < TWHY[i][1] + TWHY[i][3]) *why = 1; return i; }
	return -1;
}
static gboolean toast_press(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)w; (void)d; if (e->button != 1 || e->type != GDK_BUTTON_PRESS) return TRUE; int why, i = toast_hit(e->x_root, e->y_root, &why); if (i < 0) return TRUE;
	Toast t = TST[i]; memmove(&TST[i], &TST[i + 1], (NTST - i - 1) * sizeof TST[0]); NTST--; toast_place();   /* (d.remove()) */
	/* (⚡ Why?: Blitzen asked about it, as the centre's own (#app=do-nc&op=why&i=N); not kept yet: the centre) */
	if (why) { int k = -1; for (int j = 0; j < NNT && k < 0; j++) if (!strcmp(NT[j].t, t.t)) k = j; if (k >= 0) { char ex[40]; snprintf(ex, sizeof ex, "&op=why&i=%d", k); app_open_ex("do-nc", NULL, ex, 0); return TRUE; } }
	if (t.act && t.sid[0]) { oracle_act(t.sid, t.nid); return TRUE; }   /* (Oracle's page in Chromium: its own action, through rudolf-launch) */
	if (t.act) { for (GList *l = WINS; l; l = l->next) { AppWin *a = l->data; if (a->id == t.win && a->view) { char js[120]; snprintf(js, sizeof js, "window.__oaToastAct&&__oaToastAct('%s')", t.nid);
				webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(a->view), js, -1, NULL, NULL, NULL, NULL, NULL); return TRUE; } } return TRUE; }
	if (FLK == FL_NC) fl_close(); fl_open(FL_NC);   /* (ncToggle(true): the centre, opened again if it was) */
	return TRUE;
}
static gboolean toast_motion(GtkWidget *w, GdkEventMotion *e, gpointer d)
{
	(void)w; (void)d; GdkWindow *g = gtk_widget_get_window(TWIN); static GdkCursor *ptr; if (!ptr) ptr = gdk_cursor_new_from_name(gdk_display_get_default(), "pointer");
	int why; if (g) gdk_window_set_cursor(g, toast_hit(e->x_root, e->y_root, &why) >= 0 ? ptr : NULL); return TRUE;
}
static void fget_js(void *v, const char *k, char *o, size_t n) { jsstr((JSCValue *)v, k, o, n); }
static void fget_jv(void *v, const char *k, char *o, size_t n) { jstr((JV *)v, k, o, n); }
static void toast_add(AppWin *from, JSCValue *v) { toast_addf(from, NULL, fget_js, v); }
/* (a pop-up of the shell's own, as the page's notify(t, b, {quiet: true, toast: ms}): "Workspace N") */
static void fget_tt(void *v, const char *k, char *o, size_t n) { const char *const *f = v; snprintf(o, n, "%s", !strcmp(k, "t") ? f[0] : !strcmp(k, "b") ? f[1] : !strcmp(k, "ms") ? f[2] : ""); }
static void toast_text(const char *t, const char *b, int ms) { char m[16]; snprintf(m, sizeof m, "%d", ms); const char *f[3] = {t, b, m}; toast_addf(NULL, NULL, fget_tt, (void *)f); }
/* ------------------------------------------------------------------ game mode (1.6.1, docs/plan-1.6.1.md Part 19)
   The native desktop is the window manager, so it is what sees a game full screen (a Linux window, Oracle's window, one of
   its own app windows: a page's full screen): it tells rudolfd every 2 s while one is (game/on?src=native: root raises the
   processor's preference and, for a Linux game, lowers the desktop's own processor time; a source that stops saying so
   counts 6 s at most, so a crash of the native desktop leaves nothing behind), and game/off when none is. The pages hear
   of it from rudolfd (gamemode61.js): they hold their notifications and background work, as Standard's page does. The
   shell's own pop-ups (the cellular notices, its own messages) are held here too, but errors, and once the game is over
   one pop-up says how many came, as the page's does (game51.js). Not with Game mode off (rudolfd says so). */
static int GM61_ON, GM61_HELD;
static Atom A_NPID;
static Client *gm61_full(void)
{
	for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data;
		if (!c->full || c->hidden || c->off || c->min || (c->app && !strcmp(c->app->k, "do-saver"))) continue;
		return c; }
	return NULL;
}
static long gm61_pid(Window w)
{
	if (!A_NPID) A_NPID = XInternAtom(DPY, "_NET_WM_PID", False);
	Atom type; int fmt; unsigned long n, after; unsigned char *d = NULL; long p = 0;
	if (XGetWindowProperty(DPY, w, A_NPID, 0, 1, False, XA_CARDINAL, &type, &fmt, &n, &after, &d) == Success && d) { if (n == 1 && fmt == 32) p = (long)*(unsigned long *)d; XFree(d); }
	return p;
}
static gboolean gm61_tick(gpointer d)
{
	(void)d; Client *c = USER_ID[0] && DPY ? gm61_full() : NULL; int on = 0;
	if (c) { char p[160]; GByteArray *b = NULL; long pid = gm61_pid(c->w);
		snprintf(p, sizeof p, "/api/game/on?src=native&kind=%s&pid=%ld", c->app ? "page" : c->ak[0] ? "oracle" : "linux", pid > 1 ? pid : 0);
		if (http("POST", p, NULL, 0, &b) == 200 && b) { g_byte_array_append(b, (const guint8 *)"", 1); JV *v = jparse((char *)b->data); on = v && jbool(v, "enabled"); jfree(v); }
		if (b) g_byte_array_free(b, TRUE); }
	else if (GM61_ON) http("POST", "/api/game/off?src=native", NULL, 0, NULL);
	int was = GM61_ON; GM61_ON = on;
	if (was && !on && GM61_HELD) { char t[160]; snprintf(t, sizeof t, "%d notification%s came in. They are in the notification centre.", GM61_HELD, GM61_HELD == 1 ? "" : "s"); GM61_HELD = 0; toast_text("While you were playing", t, 6000); }
	return TRUE;
}
/* a pop-up held while a game is full screen (kept in the centre by whoever sent it, as with Do not disturb) */
static int gm61_hold(const char *kind) { if (!GM61_ON || !strcmp(kind, "error")) return 0; GM61_HELD++; return 1; }

/* ---- 1.6.2 (docs/plan-1.6.2.md Part 25b): Linux apps' progress on their taskbar buttons, as Standard's page draws it
   (lxprog62.js): rudolfd's portal/progress lists the signed-in account's apps showing progress (their desktop file ID, its
   StartupWMClass when Flatpak exports one, a whole percentage: rudolfd-lxprog.inc); each Linux app's window (no page of
   Rudolf's, not Oracle's) whose class or instance (WM_CLASS) is the ID, the ID's last part (of more than 2 letters) or its
   StartupWMClass, ignoring case, gets the same fill as Rudolf's own windows (tbp_set: only while minimized; a percentage,
   never the creeping fill; done: to 100 %, then it fades). A window's own progress (the XApp hint _NET_WM_XAPP_PROGRESS, which
   Firefox sets on X11) comes first, on that window alone. Asked once a second, only while there is such a window. */
static GHashTable *LXP_ON;   /* (the windows whose fill is their app's progress) */
static int lxp_match(const char *id, const char *wc, const char *cls, const char *inst)
{
	if (!id || !id[0] || ((!cls || !cls[0]) && (!inst || !inst[0]))) return 0;
	const char *last = strrchr(id, '.'); last = last ? last + 1 : id;
	const char *ks[3] = { id, strlen(last) > 2 ? last : NULL, wc && wc[0] ? wc : NULL };
	for (int i = 0; i < 3; i++) if (ks[i] && ((cls && cls[0] && !g_ascii_strcasecmp(ks[i], cls)) || (inst && inst[0] && !g_ascii_strcasecmp(ks[i], inst)))) return 1;
	return 0;
}
/* the window's own progress: the XApp hint _NET_WM_XAPP_PROGRESS (1-100; 0 or none: no progress), which Firefox sets on X11
   rather than sending the signal (rudolf-display reads the same for Standard) */
static long lxp_xapp(Window w)
{
	static Atom A_XPROG; if (!A_XPROG) A_XPROG = XInternAtom(DPY, "_NET_WM_XAPP_PROGRESS", False);
	Atom type; int fmt; unsigned long n = 0, after; unsigned char *d = NULL; long v = 0;
	if (XGetWindowProperty(DPY, w, A_XPROG, 0, 1, False, XA_CARDINAL, &type, &fmt, &n, &after, &d) == Success && d) { if (n == 1 && fmt == 32) v = (long)*(unsigned long *)d; XFree(d); }
	return v >= 1 && v <= 100 ? v : 0;
}
static int lxp_linux(Client *c) { return c && !c->app && !c->ak[0] && !c->trans; }
static gboolean lxp_tick(gpointer d)
{
	(void)d; if (!LXP_ON) LXP_ON = g_hash_table_new(NULL, NULL);
	if (!WM_OK || !DPY) return G_SOURCE_CONTINUE;
	int any = 0; for (GList *l = CLIENTS; l; l = l->next) if (lxp_linux(l->data)) any = 1;
	/* (windows gone: forgotten) */
	GHashTableIter hi; gpointer k; g_hash_table_iter_init(&hi, LXP_ON);
	while (g_hash_table_iter_next(&hi, &k, NULL)) { Client *c = client_of((Window)(uintptr_t)k); if (!c || c->w != (Window)(uintptr_t)k) g_hash_table_iter_remove(&hi); }
	if (!any) return G_SOURCE_CONTINUE;
	JV *apps = NULL, *v = NULL;
	if (USER_ID[0]) { GByteArray *b = NULL; if (http("POST", "/api/portal/progress", NULL, 0, &b) == 200 && b) { g_byte_array_append(b, (const guint8 *)"", 1); v = jparse((char *)b->data); }
		if (b) g_byte_array_free(b, TRUE); apps = jprop(v, "apps"); if (apps && apps->t != 4) apps = NULL; }
	for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (!lxp_linux(c)) continue;
		XClassHint ch = {0}; XGetClassHint(DPY, c->w, &ch); int sum = 0, n = 0;
		for (int i = 0; apps && i < apps->len; i++) { JV *a = apps->v[i]; char id[128] = "", wc[128] = ""; if (!a || a->t != 5) continue;
			jstr(a, "id", id, sizeof id); jstr(a, "wmclass", wc, sizeof wc); double pv = jnum(a, "v", -1);
			if (pv < 0 || pv > 100 || pv != (int)pv) continue;
			if (lxp_match(id, wc, ch.res_class, ch.res_name)) { sum += (int)pv; n++; } }
		if (ch.res_name) XFree(ch.res_name); if (ch.res_class) XFree(ch.res_class);
		{ long own = lxp_xapp(c->w); if (own > 0) { sum = (int)own; n = 1; } }   /* (the window's own first: it is exactly that window's) */
		gpointer key = (gpointer)(uintptr_t)c->w;
		if (n) { char t[8]; snprintf(t, sizeof t, "%d", (sum + n / 2) / n); tbp_set(c, t); g_hash_table_add(LXP_ON, key); }
		else if (g_hash_table_contains(LXP_ON, key)) { tbp_set(c, ""); g_hash_table_remove(LXP_ON, key); } }
	jfree(v); return G_SOURCE_CONTINUE;
}
static void toast_addf(AppWin *from, const char *sid, FGet g, void *v)
{
	if (!USER_ID[0]) return; if (NTST == 8) { memmove(&TST[0], &TST[1], 7 * sizeof TST[0]); NTST--; }
	Toast *t = &TST[NTST++]; memset(t, 0, sizeof *t); char x[32] = "";
	g(v, "t", t->t, sizeof t->t); g(v, "b", t->b, sizeof t->b); g(v, "k", t->k, sizeof t->k); g(v, "id", t->nid, sizeof t->nid);
	g(v, "ms", x, sizeof x); t->ms = atoi(x) > 0 ? atoi(x) : 5000; g(v, "why", x, sizeof x); t->why = !strcmp(x, "true") || !strcmp(x, "1"); g(v, "act", x, sizeof x); t->act = !strcmp(x, "true") || !strcmp(x, "1"); g(v, "wl", t->wl, sizeof t->wl);
	if (sid) snprintf(t->sid, sizeof t->sid, "%s", sid);
	if (!t->k[0]) snprintf(t->k, sizeof t->k, "info"); t->win = from ? from->id : 0; t->born = g_get_monotonic_time();
	if (STATEF) { TLOG_N++; snprintf(TLOG[TLOG_N % 32], sizeof TLOG[0], "%s|%s", from ? from->k : "-", t->t); }   /* (the tests count them, item 12: only with RUDOLF_NATIVE_STATE) */
	/* (1.6.0) Do not disturb: no pop-up, from any window or the shell, but an error's (shell25.js __notifCentre.add: !S.dnd||n.k=='error'), as on
	   Standard where one page decides; a window whose page had not heard of the switch yet no longer shows one */
	if (S.dnd && strcmp(t->k, "error")) { NTST--; return; }
	if (gm61_hold(t->k)) { NTST--; return; }   /* (1.6.1 Part 19: game mode) */
	toast_place(); if (!g_signal_handler_find(TWIN, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, toast_press, NULL)) { g_signal_connect(TWIN, "button-press-event", G_CALLBACK(toast_press), NULL); g_signal_connect(TWIN, "motion-notify-event", G_CALLBACK(toast_motion), NULL); }
	toast_anim();
}


/* ------------------------------------------------------------------ the development link (1.5.9 step 7)
   While the development link is on (Settings › Security › Development link, devlink40.js) the page drew a red banner
   over everything, and with "Ask first" asked about each command. In one-app windows the page no longer draws them (the
   banner covered the top of every app, Settings' search box among them): the shell shows the banner once, at the top
   of the screen (windows are kept below it), with End now, and asks about commands itself. */
static GtkWidget *DLAREA, *DLLAB, *DLCMD; static char DL_TEXT[600], DL_ASKED[64], DL_PEND[64]; static int DL_ON;
static double dl_end_x(int W) { return W - 90 * Z; }
static gboolean dl_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)d; int W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
	cairo_set_source_rgb(cr, .69, 0, .125); cairo_paint(cr);
	text_atz(cr, DL_TEXT, 10 * Z, 0, dl_end_x(W) - 20 * Z, H, "Sans Bold 9.5", (RGB){1, 1, 1}, 0);
	double bx = dl_end_x(W), bw = 80 * Z; rrect(cr, bx, 3 * Z, bw, H - 6 * Z, 4 * Z); cairo_set_source_rgb(cr, 1, 1, 1); cairo_fill(cr);
	text_atz(cr, "End now", bx, 3 * Z, bw, H - 6 * Z, "Sans Bold 9", (RGB){.69, 0, .125}, 1);
	return TRUE;
}
static gboolean tick_devlink(gpointer d);
static gboolean dl_press(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)d; if (e->button == 1 && e->x >= dl_end_x(gtk_widget_get_allocated_width(w))) { http("POST", "/api/devlink/off", NULL, 0, NULL); tick_devlink(NULL); }
	return TRUE;
}
static void dl_answer(GtkButton *b, gpointer d)
{
	(void)b; char p[160]; char *id = g_uri_escape_string(DL_PEND, NULL, FALSE); snprintf(p, sizeof p, "/api/devlink/answer?id=%s&a=%s", id, (char *)d); g_free(id);
	http("POST", p, NULL, 0, NULL); gtk_widget_hide(DLASK); DL_PEND[0] = 0; tick_devlink(NULL);
}
static void dl_show(int on)
{
	if (on && !DLBAN) {
		DLBAN = gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_window_set_decorated(GTK_WINDOW(DLBAN), FALSE); gtk_window_set_type_hint(GTK_WINDOW(DLBAN), GDK_WINDOW_TYPE_HINT_DOCK);
		gtk_window_set_title(GTK_WINDOW(DLBAN), "Development link"); gtk_window_set_keep_above(GTK_WINDOW(DLBAN), TRUE); gtk_window_set_skip_taskbar_hint(GTK_WINDOW(DLBAN), TRUE);
		DLAREA = gtk_drawing_area_new(); gtk_widget_add_events(DLAREA, GDK_BUTTON_PRESS_MASK);
		g_signal_connect(DLAREA, "draw", G_CALLBACK(dl_draw), NULL); g_signal_connect(DLAREA, "button-press-event", G_CALLBACK(dl_press), NULL);
		gtk_container_add(GTK_CONTAINER(DLBAN), DLAREA); g_signal_connect(DLBAN, "delete-event", G_CALLBACK(gtk_true), NULL);
	}
	int h = on ? (int)lround(24 * Z) : 0;
	if (on) { gtk_widget_set_size_request(DLBAN, SW, h); gtk_window_move(GTK_WINDOW(DLBAN), 0, 0); gtk_window_resize(GTK_WINDOW(DLBAN), SW, h); gtk_widget_show_all(DLBAN); gtk_widget_queue_draw(DLAREA); }
	else if (DLBAN) gtk_widget_hide(DLBAN);
	if (h != TOPRES) { TOPRES = h; bar_place(); if (WM_OK) wm_relayout(); }
	wm_restack();
}
static void dl_ask(const char *id, const char *what)
{
	if (!DLASK) {
		DLASK = gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_window_set_title(GTK_WINDOW(DLASK), "Development link: allow this command?"); gtk_window_set_decorated(GTK_WINDOW(DLASK), FALSE);
		gtk_window_set_keep_above(GTK_WINDOW(DLASK), TRUE); gtk_window_set_default_size(GTK_WINDOW(DLASK), (int)(560 * Z), -1);
		gtk_style_context_add_class(gtk_widget_get_style_context(DLASK), "rn-card");
		GtkWidget *b = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10), *t = gtk_label_new("🔧 Allow this command?"); gtk_container_add(GTK_CONTAINER(DLASK), b);
		gtk_style_context_add_class(gtk_widget_get_style_context(t), "rn-big"); gtk_box_pack_start(GTK_BOX(b), t, FALSE, FALSE, 0);
		DLLAB = gtk_label_new(""); gtk_label_set_line_wrap(GTK_LABEL(DLLAB), TRUE); gtk_label_set_xalign(GTK_LABEL(DLLAB), 0); gtk_box_pack_start(GTK_BOX(b), DLLAB, FALSE, FALSE, 0);
		DLCMD = gtk_label_new(""); gtk_label_set_line_wrap(GTK_LABEL(DLCMD), TRUE); gtk_label_set_selectable(GTK_LABEL(DLCMD), TRUE); gtk_label_set_xalign(GTK_LABEL(DLCMD), 0);
		gtk_label_set_max_width_chars(GTK_LABEL(DLCMD), 80); gtk_box_pack_start(GTK_BOX(b), DLCMD, FALSE, FALSE, 0);
		GtkWidget *w = gtk_label_new("It runs with full control of this computer. Refuse anything you did not expect. Not answered in 10 minutes, it is refused.");
		gtk_label_set_line_wrap(GTK_LABEL(w), TRUE); gtk_label_set_max_width_chars(GTK_LABEL(w), 70); gtk_box_pack_start(GTK_BOX(b), w, FALSE, FALSE, 0);
		GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8), *no = gtk_button_new_with_label("Refuse"), *yes = gtk_button_new_with_label("Allow");
		gtk_style_context_add_class(gtk_widget_get_style_context(yes), "rn-go"); g_object_set_data(G_OBJECT(DLASK), "no", no);
		g_signal_connect(no, "clicked", G_CALLBACK(dl_answer), "refuse"); g_signal_connect(yes, "clicked", G_CALLBACK(dl_answer), "allow");
		gtk_box_pack_end(GTK_BOX(row), yes, FALSE, FALSE, 0); gtk_box_pack_end(GTK_BOX(row), no, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(b), row, FALSE, FALSE, 0);
		g_signal_connect(DLASK, "delete-event", G_CALLBACK(gtk_true), NULL);
	}
	snprintf(DL_PEND, sizeof DL_PEND, "%s", id); gtk_label_set_text(GTK_LABEL(DLLAB), DL_TEXT[0] ? "A command from the development link:" : ""); gtk_label_set_text(GTK_LABEL(DLCMD), what);
	gtk_widget_show_all(DLASK); gtk_window_set_position(GTK_WINDOW(DLASK), GTK_WIN_POS_CENTER);
	int w, h; gtk_window_get_size(GTK_WINDOW(DLASK), &w, &h); gtk_window_move(GTK_WINDOW(DLASK), (SW - w) / 2, (SH - h) / 3);
	gtk_window_present(GTK_WINDOW(DLASK)); GdkWindow *g = gtk_widget_get_window(DLASK); if (g) gdk_window_focus(g, GDK_CURRENT_TIME);
	gtk_widget_grab_focus(g_object_get_data(G_OBJECT(DLASK), "no"));   /* (Refuse first, as the page) */
	wm_restack();
}
static gboolean tick_devlink(gpointer d)
{
	(void)d; JV *v = jget("/api/devlink"); int on = v && jbool(v, "on");
	if (on) {
		char repo[200] = "", st[40] = "", er[200] = ""; jstr(v, "repo", repo, sizeof repo); JV *x = jprop(v, "status"); jstr(x, "state", st, sizeof st); jstr(x, "error", er, sizeof er);
		int m = (int)lround(fmax(0, jnum(v, "secondsLeft", 0)) / 60); char left[40];
		if (m >= 60) snprintf(left, sizeof left, "%d h %d min", m / 60, m % 60); else snprintf(left, sizeof left, "%d min", m);
		snprintf(DL_TEXT, sizeof DL_TEXT, "⚠ Development link on: commands from GitHub (%s) can control this computer · %s left%s%s%s%s", repo, left,
			st[0] && strcmp(st, "on") ? " · " : "", st[0] && strcmp(st, "on") ? st : "", er[0] ? " · " : "", er);
	}
	if (on != DL_ON || on) { DL_ON = on; dl_show(on); }
	JV *p = on ? jprop(v, "pending") : NULL; char id[64] = "", kind[40] = "", what[2000] = "";
	if (p && p->t == 5) { jstr(p, "id", id, sizeof id); jstr(p, "kind", kind, sizeof kind); jstr(p, "what", what, sizeof what); }
	if (id[0] && strcmp(id, DL_ASKED)) { char repo[200] = "", t[2400]; jstr(v, "repo", repo, sizeof repo); snprintf(DL_ASKED, sizeof DL_ASKED, "%s", id);
		snprintf(t, sizeof t, "%s", what); dl_ask(id, t);
		char l[400]; snprintf(l, sizeof l, "Command %s from %s (%s):", id, repo, kind); gtk_label_set_text(GTK_LABEL(DLLAB), l); }
	else if (!id[0] && DLASK && gtk_widget_get_visible(DLASK)) { gtk_widget_hide(DLASK); DL_ASKED[0] = 0; }
	jfree(v); state_write(); return G_SOURCE_CONTINUE;
}

/* ------------------------------------------------------------------ the tests' view (RUDOLF_NATIVE_STATE) */
/* ------------------------------------------------------------------ 1.6.1 Part 1: several monitors, on the three workspaces
 * As the page's (desktop/mon61.js), which says the rules: up to three monitors (GDK's, RandR's, side by side as rudolf-display arranges
 * them, the main one at 0,0), each showing one of the three workspaces; which shows which, and which is the main one, from
 * /run/rudolf/monitors (rudolf-display's, by the monitors' names; RUDOLF_MON_RUN for the tests), else from the left. The main monitor is
 * the desktop as with one (SW and SH are its size: the taskbar with the tray, the icons, the Start menu, the notifications); each other
 * one has the wallpaper (MON[k].wall) and a taskbar of its own (MON[k].bar: bar_paint with MONK_CUR = k: the start button, the pinned
 * apps, its workspace's windows, the workspaces, the clock). A window's x and y are on the monitor showing its workspace (mon_ox, mon_oy
 * added where it goes on the screen); one dragged onto another monitor goes into that one's workspace; maximised, it fills its own. A
 * monitor plugged in or out: the windows kept inside the monitors (mon_keep). The sign-in and lock screens and the screen saver: the
 * other monitors covered (MON[k].cov). */
static int mon_of_ws(int n) { if (NMON < 2) return n == WSC ? 0 : -1; for (int k = 0; k < NMON; k++) if (MON[k].ws == n) return k; return -1; }
static int mon_k(Client *c) { if (NMON < 2) return 0; int k = mon_of_ws(ws_of(c)); return k < 0 ? 0 : k; }
static int mon_ox(Client *c) { return NMON < 2 ? 0 : MON[mon_k(c)].x - MON[0].x; }
static int mon_oy(Client *c) { return NMON < 2 ? 0 : MON[mon_k(c)].y - MON[0].y; }
static void mon_bars_queue(void) { for (int k = 1; k < NMON; k++) if (MON[k].barea) gtk_widget_queue_draw(MON[k].barea); }
static void mon_walls_queue(void) { for (int k = 1; k < NMON; k++) if (MON[k].warea) gtk_widget_queue_draw(MON[k].warea); }
/* the window last used in workspace N (the page's MRU) */
static Client *mon_top(int n) { Client *b = NULL; for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (c->trans || c->min || c->hidden || !c->ws || ws_of(c) != n || !(c->app || c->ak[0])) continue; if (!b || c->used > b->used) b = c; } return b; }
/* every window inside the monitor it shows on (the main one for a workspace not shown): mon61.js monKeep */
static void mon_keep(void)
{
	for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (c->trans || c->max || c->full || !c->ws) continue;
		int k = NMON > 1 ? mon_k(c) : 0, dw = desk_w() + (NMON > 1 ? MON[k].w - SW : 0), dh = work_h() + (NMON > 1 ? MON[k].h - SH : 0), ch = 0;
		int fw = fw_of(c), fh = fh_of(c), m8 = (int)lround(8 * Z);
		if (fw > dw - m8 && fw > (int)lround(200 * Z)) { c->cw = MAX((int)lround(200 * Z), dw - m8) - 2 * BW; ch = 1; }
		if (fh > dh - m8 && fh > (int)lround(120 * Z)) { c->ch = MAX((int)lround(120 * Z), dh - m8) - th_of(c) - BW; ch = 1; }
		int lx = c->x - desk_left(), ly = c->y - work_top(); fw = fw_of(c); fh = fh_of(c);
		if (lx > dw - (int)lround(60 * Z) || lx + fw < (int)lround(60 * Z)) { lx = MAX(0, MIN(dw - fw - (int)lround(4 * Z), lx)); c->x = desk_left() + lx; ch = 1; }
		if (ly > dh - (int)lround(40 * Z) || ly < 0) { ly = MAX(0, MIN(dh - fh - (int)lround(4 * Z), ly)); c->y = work_top() + ly; ch = 1; }
		if (ch || c->mk != mon_k(c)) { place(c); frame_draw(c); tell_geom(c); } }
}
static void mon_redraw(void) { bar_redraw(); desk_shadows(); state_write(); }
/* workspace N shown on monitor K (its taskbar's button): the monitor that showed it takes K's (monShow); FOCUS: its last used window */
static void mon_show(int k, int n, int focus)
{
	if (NMON < 2 || k < 0 || k >= NMON) { ws_go(n); return; } if (n < 1) n = 1; if (n > NWS) n = NWS;
	int j = mon_of_ws(n), was = MON[k].ws;
	if (j < 0) MON[k].ws = n; else if (j != k) { MON[j].ws = was; MON[k].ws = n; }
	WSC = n; ws_apply(); mon_keep();
	if (focus) { Client *t = mon_top(n); wm_focus(t); }
	mon_redraw(); if (j != k) g_timeout_add(80, ws_said, GINT_TO_POINTER(n));
}
/* Ctrl+Alt+arrows and the like (wsGo): a workspace shown on a monitor is the one in use; else shown on the monitor in use */
static void mon_go(int n)
{
	if (n < 1) n = 1; if (n > NWS) n = NWS; if (n == WSC) { bar_redraw(); return; }
	if (mon_of_ws(n) >= 0) { WSC = n; wm_focus(mon_top(n)); mon_redraw(); return; }
	mon_show(MAX(0, mon_of_ws(WSC)), n, 1);
}
/* "Move to workspace N": the window goes to the monitor showing it (wsMove) */
static void mon_move(Client *c, int n)
{
	if (n < 1) n = 1; if (n > NWS) n = NWS; int k0 = mon_of_ws(c->ws); c->ws = n; ws_apply();
	if (k0 >= 0 && mon_of_ws(n) >= 0) mon_keep();
	if (FOCUS && FOCUS->off) FOCUS = NULL; if (!FOCUS) wm_focus(top_client()); mon_redraw();
}
/* the monitor clicked on (its taskbar, its wallpaper) is the one in use */
static void mon_touch(int k) { if (NMON > 1 && k >= 0 && k < NMON && WSC != MON[k].ws) { WSC = MON[k].ws; bar_redraw(); state_write(); } }
/* a window let go on another monitor (its title bar's middle, 16 px down): into that monitor's workspace, where it is (monDrop) */
static void mon_drop(Client *c)
{
	if (NMON < 2 || c->max || c->full || c->off) return; int k = mon_k(c), fw = fw_of(c), fh = fh_of(c);
	int cx = MON[k].x + c->x + fw / 2, cy = MON[k].y + c->y + MIN((int)lround(16 * Z), fh / 2), j = -1;
	for (int i = 0; i < NMON; i++) if (cx >= MON[i].x && cx < MON[i].x + MON[i].w && cy >= MON[i].y && cy < MON[i].y + MON[i].h) j = i;
	if (j < 0 || j == k) return;
	c->x += MON[k].x - MON[j].x; c->y += MON[k].y - MON[j].y; c->ws = MON[j].ws; WSC = c->ws; ws_apply(); mon_keep(); place(c); frame_draw(c); tell_geom(c); mon_redraw();
}
/* ---- the other monitors' windows: the wallpaper (and the windows' shadows), the taskbar, the cover */
static gboolean mon_wall_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)w; int k = GPOINTER_TO_INT(d); if (k < 1 || k >= NMON) return TRUE;
	/* (wall_make for this monitor's size: the same picture or pattern, laid as the page lays the background on its box) */
	cairo_surface_t *sw0 = WALL; char key0[300]; snprintf(key0, sizeof key0, "%s", WALL_KEY); int w0 = SW, h0 = SH;
	WALL = MON[k].wsurf; snprintf(WALL_KEY, sizeof WALL_KEY, "%s", MON[k].wkey); SW = MON[k].w; SH = MON[k].h; WALL_SIDE = 1;
	wall_make(); MON[k].wsurf = WALL; snprintf(MON[k].wkey, sizeof MON[k].wkey, "%s", WALL_KEY);
	WALL_SIDE = 0; WALL = sw0; snprintf(WALL_KEY, sizeof WALL_KEY, "%s", key0); SW = w0; SH = h0;
	if (MON[k].wsurf) { cairo_set_source_surface(cr, MON[k].wsurf, 0, 0); cairo_paint(cr); }
	MON_DRAWK = k; frames_shadow(cr); MON_DRAWK = 0; return TRUE;
}
static gboolean mon_wall_press(GtkWidget *w, GdkEventButton *e, gpointer d) { (void)w; (void)e; mon_touch(GPOINTER_TO_INT(d)); if (MENU && gtk_widget_get_visible(MENU)) menu_hide(); return TRUE; }
/* the taskbar's handlers, with this monitor's places in HITS while they run */
static Hit MON_SAVE[96]; static int MON_SAVEN;
static void mb_enter(int k) { memcpy(MON_SAVE, HITS, sizeof HITS); MON_SAVEN = NHITS; memcpy(HITS, MON[k].hits, sizeof HITS); NHITS = MON[k].nhits; MONK_CUR = k; }
static void mb_leave(int k) { if (k < NMON) { memcpy(MON[k].hits, HITS, sizeof HITS); MON[k].nhits = NHITS; } memcpy(HITS, MON_SAVE, sizeof HITS); NHITS = MON_SAVEN; MONK_CUR = 0; if (STATE_LATER) { STATE_LATER = 0; state_write(); } }
static gboolean mb_draw(GtkWidget *w, cairo_t *cr, gpointer d) { int k = GPOINTER_TO_INT(d); if (k >= NMON) return TRUE; mb_enter(k); gboolean r = bar_draw(w, cr, NULL); mb_leave(k); return r; }
static gboolean mb_press(GtkWidget *w, GdkEventButton *e, gpointer d) { int k = GPOINTER_TO_INT(d); if (k >= NMON) return TRUE; mb_enter(k); gboolean r = bar_press(w, e, NULL); mb_leave(k); return r; }
static gboolean mb_release(GtkWidget *w, GdkEventButton *e, gpointer d) { int k = GPOINTER_TO_INT(d); if (k >= NMON) return TRUE; mb_enter(k); gboolean r = bar_release(w, e, NULL); mb_leave(k); return r; }
static gboolean mb_motion(GtkWidget *w, GdkEventMotion *e, gpointer d) { int k = GPOINTER_TO_INT(d); if (k >= NMON) return FALSE; mb_enter(k); gboolean r = bar_motion(w, e, NULL); mb_leave(k); return r; }
static gboolean mb_leave_ev(GtkWidget *w, GdkEventCrossing *e, gpointer d) { int k = GPOINTER_TO_INT(d); if (k >= NMON) return FALSE; if (HOV_BAR != k) return FALSE; mb_enter(k); gboolean r = bar_leave(w, e, NULL); mb_leave(k); return r; }
/* the covers: the sign-in screen's background (the page's .mcover: radial-gradient(circle at 30% 20%, #1e3a8a, #0b1020 70%)), black for the screen saver */
static int MON_DARK;
static gboolean mon_cov_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)d; double W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
	if (MON_DARK) { cairo_set_source_rgb(cr, 0, 0, 0); cairo_paint(cr); return TRUE; }
	double cx = .3 * W, cy = .2 * H, r = hypot(fmax(cx, W - cx), fmax(cy, H - cy)); cairo_pattern_t *g = cairo_pattern_create_radial(cx, cy, 0, cx, cy, r);
	cairo_pattern_add_color_stop_rgb(g, 0, 0x1e / 255., 0x3a / 255., 0x8a / 255.); cairo_pattern_add_color_stop_rgb(g, .7, 0x0b / 255., 0x10 / 255., 0x20 / 255.); cairo_pattern_add_color_stop_rgb(g, 1, 0x0b / 255., 0x10 / 255., 0x20 / 255.);
	cairo_set_source(cr, g); cairo_paint(cr); cairo_pattern_destroy(g); return TRUE;
}
static void mon_restack(int part)
{
	for (int k = 1; k < NMON; k++) {
		GtkWidget *x = part == 0 ? MON[k].wall : part == 1 ? MON[k].bar : MON[k].cov; if (!x || !gtk_widget_get_visible(x) || !gtk_widget_get_window(x)) continue;
		if (part == 0) XLowerWindow(DPY, GDK_WINDOW_XID(gtk_widget_get_window(x)));
		else if (part == 1) { int full = 0; for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (c->full && !c->off && !c->min && mon_k(c) == k) full = 1; } if (!full) XRaiseWindow(DPY, GDK_WINDOW_XID(gtk_widget_get_window(x))); }
		else XRaiseWindow(DPY, GDK_WINDOW_XID(gtk_widget_get_window(x))); }
}
/* (every half second, as the page's monCover) */
static gboolean mon_cover_tick(gpointer d)
{
	(void)d; if (NMON < 2) return G_SOURCE_CONTINUE; int saver = 0; for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (c->app && !strcmp(c->app->k, "do-saver")) saver = 1; }
	int lk = LOCKED || (SIGN && gtk_widget_get_visible(SIGN)), on = lk || saver, dark = saver && !lk;
	for (int k = 1; k < NMON; k++) { if (!MON[k].cov) continue; int vis = gtk_widget_get_visible(MON[k].cov);
		if (dark != MON_DARK && MON[k].cov) gtk_widget_queue_draw(MON[k].cov);
		if (on && !vis) { gtk_widget_show_all(MON[k].cov); XRaiseWindow(DPY, GDK_WINDOW_XID(gtk_widget_get_window(MON[k].cov))); } else if (!on && vis) gtk_widget_hide(MON[k].cov); }
	MON_DARK = dark;
	/* (the taskbars with the main one's: not before signing in) */
	int bv = BAR && gtk_widget_get_visible(BAR); for (int k = 1; k < NMON; k++) if (MON[k].bar && gtk_widget_get_visible(MON[k].bar) != bv) { if (bv) { gtk_widget_show_all(MON[k].bar); wm_restack(); } else gtk_widget_hide(MON[k].bar); }
	return G_SOURCE_CONTINUE;
}
static void mon_place_bar(int k)
{
	int side = tb_side(), top = !strcmp(S.tb, "top"), bw = side ? BARW : MON[k].w, bh = side ? MON[k].h - TOPRES : BARH;
	int x = MON[k].x + (side == 2 ? MON[k].w - BARW : 0), y = MON[k].y + (side || top ? TOPRES : MON[k].h - BARH);
	gtk_widget_set_size_request(MON[k].bar, bw, bh); gtk_window_move(GTK_WINDOW(MON[k].bar), x, y); gtk_window_resize(GTK_WINDOW(MON[k].bar), bw, bh);
}
static void mon_make(int k)
{
	Mon *m = &MON[k];
	m->wall = gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_window_set_decorated(GTK_WINDOW(m->wall), FALSE); gtk_window_set_type_hint(GTK_WINDOW(m->wall), GDK_WINDOW_TYPE_HINT_DESKTOP);
	gtk_window_set_title(GTK_WINDOW(m->wall), "Desktop"); gtk_window_set_keep_below(GTK_WINDOW(m->wall), TRUE); gtk_window_set_default_size(GTK_WINDOW(m->wall), m->w, m->h);
	m->warea = gtk_drawing_area_new(); gtk_widget_add_events(m->warea, GDK_BUTTON_PRESS_MASK); gtk_container_add(GTK_CONTAINER(m->wall), m->warea);
	g_signal_connect(m->warea, "draw", G_CALLBACK(mon_wall_draw), GINT_TO_POINTER(k)); g_signal_connect(m->warea, "button-press-event", G_CALLBACK(mon_wall_press), GINT_TO_POINTER(k));
	g_signal_connect(m->wall, "delete-event", G_CALLBACK(gtk_true), NULL);
	gtk_widget_set_size_request(m->wall, m->w, m->h); gtk_window_move(GTK_WINDOW(m->wall), m->x, m->y); gtk_widget_show_all(m->wall);
	m->bar = gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_window_set_decorated(GTK_WINDOW(m->bar), FALSE); gtk_window_set_type_hint(GTK_WINDOW(m->bar), GDK_WINDOW_TYPE_HINT_DOCK);
	gtk_window_set_title(GTK_WINDOW(m->bar), "Taskbar"); gtk_window_set_keep_above(GTK_WINDOW(m->bar), TRUE);
	m->barea = gtk_drawing_area_new(); gtk_widget_add_events(m->barea, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK);
	g_signal_connect(m->barea, "draw", G_CALLBACK(mb_draw), GINT_TO_POINTER(k)); g_signal_connect(m->barea, "button-press-event", G_CALLBACK(mb_press), GINT_TO_POINTER(k));
	g_signal_connect(m->barea, "button-release-event", G_CALLBACK(mb_release), GINT_TO_POINTER(k)); g_signal_connect(m->barea, "motion-notify-event", G_CALLBACK(mb_motion), GINT_TO_POINTER(k));
	g_signal_connect(m->barea, "leave-notify-event", G_CALLBACK(mb_leave_ev), GINT_TO_POINTER(k)); gtk_container_add(GTK_CONTAINER(m->bar), m->barea);
	g_signal_connect(m->bar, "delete-event", G_CALLBACK(gtk_true), NULL); mon_place_bar(k); gtk_widget_realize(m->bar);
	if (BAR && gtk_widget_get_visible(BAR)) gtk_widget_show_all(m->bar);
	m->cov = gtk_window_new(GTK_WINDOW_POPUP); gtk_window_set_title(GTK_WINDOW(m->cov), "Cover"); GtkWidget *ca = gtk_drawing_area_new(); gtk_container_add(GTK_CONTAINER(m->cov), ca);
	g_signal_connect(ca, "draw", G_CALLBACK(mon_cov_draw), NULL); gtk_widget_set_size_request(m->cov, m->w, m->h); gtk_window_move(GTK_WINDOW(m->cov), m->x, m->y);
}
static void mon_unmake(int k)
{
	Mon *m = &MON[k]; if (m->wall) gtk_widget_destroy(m->wall); if (m->bar) gtk_widget_destroy(m->bar); if (m->cov) gtk_widget_destroy(m->cov);
	if (m->wsurf) cairo_surface_destroy(m->wsurf); m->wall = m->warea = m->bar = m->barea = m->cov = NULL; m->wsurf = NULL; m->wkey[0] = 0; m->nhits = 0;
}
/* the monitors as GDK has them, with their workspaces from rudolf-display's list (mon61.js monNorm: the main one first, then from the left) */
static int mon_detect(Mon *L)
{
	GdkDisplay *dp = gdk_display_get_default(); int n = 0, g = dp ? gdk_display_get_n_monitors(dp) : 0, main[3] = {0};
	for (int i = 0; i < g && n < 3; i++) { GdkMonitor *m = gdk_display_get_monitor(dp, i); GdkRectangle r; if (!m) continue; gdk_monitor_get_geometry(m, &r); if (r.width <= 0 || r.height <= 0) continue;
		memset(&L[n], 0, sizeof L[n]); const char *nm = gdk_monitor_get_model(m); snprintf(L[n].name, sizeof L[n].name, "%s", nm && *nm ? nm : "monitor"); L[n].x = r.x; L[n].y = r.y; L[n].w = r.width; L[n].h = r.height; n++; }
	/* rudolf-display's list: each one's workspace and the main one, by name (else by place) */
	const char *fp = getenv("RUDOLF_MON_RUN"); char *t = NULL; if (!g_file_get_contents(fp && *fp ? fp : "/run/rudolf/monitors", &t, NULL, NULL)) t = NULL;
	JV *top = t ? jparse(t) : NULL, *a = jprop(top, "monitors"); g_free(t);
	for (int i = 0; i < n; i++) for (int j = 0; a && a->t == 4 && j < a->len; j++) { JV *o = jat(a, j); char nm[40] = ""; jstr(o, "name", nm, sizeof nm);
		if (!strcmp(nm, L[i].name) || ((int)jnum(o, "x", -1) == L[i].x && (int)jnum(o, "y", -1) == L[i].y)) { L[i].ws = (int)jnum(o, "ws", 0); main[i] = jbool(o, "main"); } }
	jfree(top);
	/* from the left, the main one first; each workspace once, the others given the first free ones */
	for (int i = 1; i < n; i++) for (int j = i; j > 0 && (L[j].x < L[j - 1].x || (L[j].x == L[j - 1].x && L[j].y < L[j - 1].y)); j--) { Mon x = L[j]; L[j] = L[j - 1]; L[j - 1] = x; int y = main[j]; main[j] = main[j - 1]; main[j - 1] = y; }
	int mi = 0; for (int i = n - 1; i >= 0; i--) if (main[i]) mi = i; if (mi) { Mon x = L[mi]; for (int i = mi; i > 0; i--) L[i] = L[i - 1]; L[0] = x; }
	int used = 0; for (int i = 0; i < n; i++) { if (L[i].ws < 1 || L[i].ws > NWS || (used & (1 << L[i].ws))) L[i].ws = 0; else used |= 1 << L[i].ws; }
	for (int i = 0; i < n; i++) if (!L[i].ws) for (int w = 1; w <= NWS; w++) if (!(used & (1 << w))) { L[i].ws = w; used |= 1 << w; break; }
	return n;
}
/* the main monitor's size changed: the desktop's window, its wallpaper and the taskbar with it */
static void mon_main_size(int w, int h)
{
	if (w == SW && h == SH) return; SW = w; SH = h;
	if (WALL) { cairo_surface_destroy(WALL); WALL = NULL; } WALL_KEY[0] = 0; if (DESKC) { cairo_surface_destroy(DESKC); DESKC = NULL; }
	if (DESK) { gtk_widget_set_size_request(DESK, SW, SH); gtk_window_resize(GTK_WINDOW(DESK), SW, SH); gtk_window_move(GTK_WINDOW(DESK), 0, 0); }
	if (DESK) { wall_make(); desk_dirty(); } bar_place();
}
static void mon_update(int init)
{
	Mon L[3]; int n = mon_detect(L);
	if (n < 2) { if (NMON < 2 && !init) return; for (int k = 1; k < NMON; k++) mon_unmake(k); NMON = 1;
		GdkScreen *scr = gdk_screen_get_default(); int w = n ? L[0].w : gdk_screen_get_width(scr), h = n ? L[0].h : gdk_screen_get_height(scr); MON[0].ws = WSC;
		if (n) snprintf(MON[0].name, sizeof MON[0].name, "%s", L[0].name); MON[0].x = MON[0].y = 0; MON[0].w = w; MON[0].h = h;
		if (!init) { mon_main_size(w, h); ws_apply(); for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (c->max) { c->max = 0; max_geom(c); c->max = 1; } if (!c->off) { place(c); frame_draw(c); } } mon_keep(); mon_redraw(); }
		else if (n) { SW = w; SH = h; }
		return; }
	int same = n == NMON; for (int i = 0; same && i < n; i++) same = !strcmp(L[i].name, MON[i].name) && L[i].x == MON[i].x && L[i].y == MON[i].y && L[i].w == MON[i].w && L[i].h == MON[i].h;
	if (same && !init) return;   /* (the same monitors: the workspaces each shows now kept) */
	for (int k = 1; k < NMON; k++) mon_unmake(k);
	for (int i = 0; i < n; i++) { Mon *m = &MON[i]; memset(m, 0, sizeof *m); snprintf(m->name, sizeof m->name, "%.39s", L[i].name); m->x = L[i].x; m->y = L[i].y; m->w = L[i].w; m->h = L[i].h; m->ws = L[i].ws; }
	NMON = n; if (mon_of_ws(WSC) < 0) WSC = MON[0].ws;
	if (init && !DESK) { SW = MON[0].w; SH = MON[0].h; return; }
	mon_main_size(MON[0].w, MON[0].h);
	for (int k = 1; k < NMON; k++) mon_make(k);
	ws_apply(); for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; c->mk = -1; if (c->max) { c->max = 0; max_geom(c); c->max = 1; } if (!c->off) { place(c); frame_draw(c); } }
	mon_keep(); wm_restack(); mon_redraw();
}
static guint MON_T;
static gboolean mon_later(gpointer d) { (void)d; MON_T = 0; mon_update(0); return G_SOURCE_REMOVE; }
static void mon_changed(void) { if (MON_T) g_source_remove(MON_T); MON_T = g_timeout_add(400, mon_later, NULL); }
static void mon_sig(GdkDisplay *d, GdkMonitor *m, gpointer u) { (void)d; (void)m; (void)u; mon_changed(); }
static void mon_sig2(GdkScreen *s, gpointer u) { (void)s; (void)u; mon_changed(); }
/* (rudolf-display's list rewritten: Settings › Display › Displays saved another arrangement) */
static gboolean mon_file_tick(gpointer d)
{
	(void)d; static gint64 last = -1; const char *fp = getenv("RUDOLF_MON_RUN"); struct stat st; gint64 mt = stat(fp && *fp ? fp : "/run/rudolf/monitors", &st) ? 0 : (gint64)st.st_mtim.tv_sec * 1000 + st.st_mtim.tv_nsec / 1000000;
	if (last >= 0 && mt != last) { Mon L[3]; int n = mon_detect(L); if (n == NMON && n > 1) { for (int i = 0; i < n; i++) for (int k = 0; k < n; k++) if (!strcmp(L[i].name, MON[k].name)) MON[k].ws = L[i].ws; if (mon_of_ws(WSC) < 0) WSC = MON[0].ws; ws_apply(); mon_keep(); mon_redraw(); } else mon_changed(); }
	last = mt; return G_SOURCE_CONTINUE;
}
static void mon_init(void)
{
	GdkDisplay *dp = gdk_display_get_default(); g_signal_connect(dp, "monitor-added", G_CALLBACK(mon_sig), NULL); g_signal_connect(dp, "monitor-removed", G_CALLBACK(mon_sig), NULL);
	g_signal_connect(gdk_screen_get_default(), "monitors-changed", G_CALLBACK(mon_sig2), NULL); g_signal_connect(gdk_screen_get_default(), "size-changed", G_CALLBACK(mon_sig2), NULL);
	mon_update(1);   /* (the main one's size, before the desktop's window is made) */
}
static void mon_start(void) { if (NMON > 1) { for (int k = 1; k < NMON; k++) mon_make(k); wm_restack(); } g_timeout_add(500, mon_cover_tick, NULL); g_timeout_add_seconds(1, mon_file_tick, NULL); }
/* (the tests' view: each monitor, its taskbar's places on the screen, each window's monitor) */
static void mon_state(GString *s)
{
	g_string_append_printf(s, "mons %d\n", NMON);
	for (int k = 0; k < NMON; k++) g_string_append_printf(s, "mmon %d %s %d %d %d %d %d\n", k, MON[k].name, MON[k].x, MON[k].y, MON[k].w, MON[k].h, NMON > 1 ? MON[k].ws : WSC);
	for (int k = 1; k < NMON; k++) { int bx = 0, by = 0; if (MON[k].bar && gtk_widget_get_window(MON[k].bar)) gdk_window_get_origin(gtk_widget_get_window(MON[k].bar), &bx, &by);
		for (int i = 0; i < MON[k].nhits; i++) { Hit *h = &MON[k].hits[i]; g_string_append_printf(s, "mhit %d %s %s %d %d %d %d\n", k, h->kind, h->arg[0] ? h->arg : "-", bx + h->x, by + h->y, h->w, h->h); } }
	for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (c->ws) g_string_append_printf(s, "clientmon %lx %d %d %d\n", (unsigned long)c->w, mon_k(c), mon_ox(c) + c->x, mon_oy(c) + c->y); }
}

static void state_write(void)
{
	if (!STATEF) return;
	if (MONK_CUR) { STATE_LATER = 1; return; }   /* (1.6.1 Part 1: another monitor's taskbar's places in HITS: once they are back) */
	GString *s = g_string_new(NULL); int by = bar_y(), bx = bar_x();
	g_string_append_printf(s, "look %s|%s|%d prev=%d\n", S.accent[0] ? S.accent : "-", S.twstyle[0] ? S.twstyle : "-", is_dark(), PREV_S ? 1 : 0);   /* (the accent colour, the title bars' style, dark: as drawn) */
	g_string_append_printf(s, "wallpaper %s|%s|%s|%s|%d|%s prev=%d\n", S.dyn[0] ? S.dyn : "-", S.bs, S.b1, S.b2, S.wpimg, S.wpfit, PREV_S ? 1 : 0);   /* (what the desktop is drawn with; prev: Settings' choices previewed, not applied) */
	g_string_append_printf(s, "screen %d %d\nuser %s\nlocked %d\nsignin %d\nmenu %d\nbar %d %d %d %d %d\nclock %s|%s\n", SW, SH, USER_ID[0] ? USER_N : "-", LOCKED,
		SIGN && gtk_widget_get_visible(SIGN), MENU && gtk_widget_get_visible(MENU), BAR && gtk_widget_get_visible(BAR), bx, by, bar_w(), bar_h(), CLOCK_T, CLOCK_D);
	g_string_append_printf(s, "xdrop %d %s %u\n", XD_N, XD_LAST, g_queue_get_length(&XD_Q) + !!XD_CUR);   /* (1.6.1 Part 13: drops finished, the last one's end, waiting) */
	g_string_append_printf(s, "ws %d\n", WSC);   /* (item 4: the workspace shown) */
	mon_state(s);   /* (1.6.1 Part 1: the monitors, their taskbars) */
	{ AppWin *pp = pt_page(); g_string_append_printf(s, "portal %s %u\nportalshown %lld\n", pp ? (pp->pready ? "ready" : "starting") : "-", PT_GIVEN ? g_hash_table_size(PT_GIVEN) : 0, (long long)PT_SHOWN); }
	for (int i = TLOG_N > 32 ? TLOG_N - 31 : 1; i <= TLOG_N; i++) g_string_append_printf(s, "toasted %d %s\n", i, TLOG[i % 32]);
	{ AppWin *sv = svc_page(); g_string_append_printf(s, "svc %s %d\n", sv ? (sv->sready ? "ready" : "starting") : "-", SVC_N); }   /* (item 12: the desktop's service page; how many times it was opened) */   /* (1.6.0: the portal's page; the requests handed out) */
	g_string_append_printf(s, "keys grabbed=%d superraw=%d kbcap=%s\n", KB_GRABBED, !SUPER_GRAB, KBCAP && g_list_find(WINS, KBCAP) ? KBCAP->k : "-");   /* (the computer check's keys, 1.6.0) */
	if (MENU && gtk_widget_get_visible(MENU)) g_string_append_printf(s, "menuq %s\n", MQ[0] ? MQ : "-");
	/* (2026-11-16) what the start menu's search found: srch GROUP X Y W H TEXT (the screen's pixels), the chosen one, the menu's box */
	if (MENU && gtk_widget_get_visible(MENU) && MQ[0]) { for (int i = 0; i < NSRI; i++) { double ry = MBY + 38 + SRI[i].y - SRSCROLL; g_string_append_printf(s, "srch %s %d %d %d %d %s\n", SRI[i].g, (int)lround((MBX + 11) * Z), (int)lround(ry * Z), (int)lround((MBW - 22) * Z), (int)lround(SRI[i].h * Z), SRI[i].t); }
		g_string_append_printf(s, "srchcur %d %d\nmenubox %d %d %d %d\n", SRCUR, SRPG_OK, (int)lround(MBX * Z), (int)lround(MBY * Z), (int)lround(MBW * Z), (int)lround(MBH * Z)); }   /* (the start menu's search, for the tests) */
	/* (item 5: Alt+Tab's switcher, the chosen one and the windows' titles; the overview's windows where they are drawn) */
	if (AT_ON) { g_string_append_printf(s, "altab %d %d %d %d %d %d", AT_N, AT_I, AX, AY, AW, AH); for (int i = 0; i < AT_N; i++) g_string_append_printf(s, "|%s", AT_L[i]->title); g_string_append_c(s, '\n'); }
	else g_string_append(s, "altab -\n");
	g_string_append_printf(s, "overview %d\n", OV_ON ? OVN : 0);
	lp_state(s);   /* (1.6.0 Part 17: the window transitions running: "wopen W" while one opens (N15's line), "lp KIND W T", the button lit) */
	g_string_append_printf(s, "saver %d %s|%s\nidlechecks %d\n", SAV_ON, SYS.ssstyle, SAV_WHY, IDLE_N);   /* (item 6) */
	for (int i = 0; OV_ON && i < OVN; i++) if (g_list_find(CLIENTS, OVL[i].c)) g_string_append_printf(s, "ovwin %lx %d %d %d %d %d\n", (unsigned long)OVL[i].c->w, (int)lround(OVL[i].x), (int)lround(OVL[i].y), (int)lround(OVL[i].w), (int)lround(OVL[i].h), OVL[i].pic != NULL);
	if (OK_[0]) g_string_append_printf(s, "osd %s %s|%s %d %d %d %d\n", OK_, OLV[0] ? OLV : "-", OLB, OX, OY, OW, OH);   /* (the keys' indicator) */
	{ static char last[300]; static int n; if (OK_[0]) { char l[300]; snprintf(l, sizeof l, "%s %s|%s", OK_, OLV[0] ? OLV : "-", OLB); if (strcmp(l, last)) { snprintf(last, sizeof last, "%s", l); n++; } }
	  if (last[0]) g_string_append_printf(s, "osdlast %d %s\n", n, last); }   /* (1.6.0, tests: the last indicator shown, and how many) */
	/* (1.6.0) quick settings' switches as the shell does them for the whole screen; the screen's colour (XRandR's gamma); the power dialog
	   (its rows, above every window); the lock screen above every window; the windows kept on top */
	g_string_append_printf(s, "a11y dark=%d nl=%d dnd=%d sr=%d mag=%d hc=%d vk=%d touch=%d magz=%g\ngamma %d %d\n", menu_dark(), nl_on(), S.dnd, S.sr, MAG_ON, S.hc, VKW && gtk_widget_get_visible(VKW), S.touch, MAGZ, GAMMA_OK == 1, GAMMA_ON);
	{ int pv = PWR && gtk_widget_get_visible(PWR); g_string_append_printf(s, "power %d\npowertop %d\npowerrows ", pv, x_on_top(PWR)); for (int i = 0; pv && i < NPWR; i++) g_string_append_printf(s, "|%s", PWRT[i]); g_string_append_c(s, '\n');
	  for (int i = 0; pv && i < NPWR; i++) g_string_append_printf(s, "pwrbtn %s %d %d\n", PWRK[i], (int)lround((PWRB[i][0] + PWRB[i][2] / 2) * Z), (int)lround((PWRB[i][1] + PWRB[i][3] / 2) * Z)); }
	g_string_append_printf(s, "signtop %d\nbright %d %d\nflgrab %d\n", x_on_top(SIGN), BRI_HAS, BRI, FL_GRAB);
	if (VKW && gtk_widget_get_visible(VKW)) { char *k = g_strdup(VKKEYS); for (char *t = strtok(k, "|"); t; t = strtok(NULL, "|")) { char ch[32]; int kx, ky; if (sscanf(t, "%31s %d %d", ch, &kx, &ky) == 3) { char *u = g_uri_unescape_string(ch, NULL); g_string_append_printf(s, "vkkey %s %d %d\n", u ? u : ch, VKX + kx, VKY + ky); g_free(u); } } g_free(k); }
	for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (c->ontop) g_string_append_printf(s, "ontop %s\n", c->app ? c->app->k : c->ak[0] ? c->ak : "-"); }
	g_string_append_printf(s, "notes %d %d %d\nmic %d %d\n", S.dnd, nt_unread(), NNT, MIC_N, MIC_MUTED);   /* (the bell: do not disturb, its count, the list's length) */   /* (item 3: the call's 🎙️; the notifications' pop-ups) */
	for (int i = 0; i < NTST; i++) g_string_append_printf(s, "toast %d %d %d %d %d|%s\n", TST[i].why, (int)lround(TSX[i] * Z), (int)lround(TSY[i] * Z), (int)lround(349 * Z), (int)lround(TST[i].h * Z), TST[i].t);
	for (int i = 0; i < NHITS; i++) g_string_append_printf(s, "hit %s %s %d %d %d %d\n", HITS[i].kind, HITS[i].arg[0] ? HITS[i].arg : "-", HITS[i].x + bx, HITS[i].y + by, HITS[i].w, HITS[i].h);
	/* the start menu's rows on the screen (mrow ACTION x y w h) */
	for (int i = 0; MENU && gtk_widget_get_visible(MENU) && !MQ[0] && i < NMR; i++) { MRow *r = &MR[i]; if (!r->shown || r->t != 'l' || strstr(r->cls, "offh")) continue;
		double y = MBY + 7 - MSCROLL + 75 + r->y; if (y < MBY || y + r->h > MBY + MBH) continue;
		g_string_append_printf(s, "mrow %s %d %d %d %d\n", r->a[0] ? r->a : r->v[0] ? r->v : "-", (int)lround((MBX + 7) * Z), (int)lround(y * Z), (int)lround((MBW - 14 - menu_sb()) * Z), (int)lround(r->h * Z)); }
	/* (2026-11-16, tests) the catalogue's programs (a key with ":", or opened unseen) and their pictures: catapp KEY X PICTURE-OR-- HAS-PICTURE; a menu row's picture */
	for (int i = 0; i < NAPPS; i++) if (APPS[i].x || strchr(APPS[i].k, ':')) g_string_append_printf(s, "catapp %s %d %s %d\n", APPS[i].k, APPS[i].x, APPS[i].ic[0] ? APPS[i].ic : "-", APPS[i].ic[0] && ICONS && g_hash_table_lookup(ICONS, APPS[i].ic) ? 1 : 0);
	for (int i = 0; i < NMR; i++) if (strchr(MR[i].v, ':')) g_string_append_printf(s, "mric %s %s %d\n", MR[i].v, MR[i].ic[0] ? MR[i].ic : "-", MR[i].ic[0] && ICONS && g_hash_table_lookup(ICONS, MR[i].ic) ? 1 : 0);
	if (SIGN) g_string_append_printf(s, "signmsg %s\nsignfields %s|%ld|%d\n", SN_MSG, SN_NAME, g_utf8_strlen(SN_PASS, -1), SFOC);   /* (the password's length only) */
	for (GList *l = WINS; l; l = l->next) { AppWin *a = l->data; GdkWindow *g = a->win ? gtk_widget_get_window(a->win) : NULL;
		Client *c = WM_OK ? client_of_app(a) : NULL;
		if (a->hidden) { g_string_append_printf(s, "page %s\n", a->k); continue; }   /* (2026-11-16: a page doing something unseen is no window: page KEY) */
		if (c && c->wait == 1) { g_string_append_printf(s, "appwait %s\n", a->k); continue; }   /* (2026-11-17: a window not shown yet, its page still drawing: not a window yet) */
		g_string_append_printf(s, "app %s %lx %d %s\n", a->k, g ? (unsigned long)GDK_WINDOW_XID(g) : 0UL, c ? c == FOCUS : a == ACTIVE, a->title);
		g_string_append_printf(s, "rest %s %d %d %s\n", a->k, a->rest, a->idle_t ? 1 : 0, a->rwhy[0] ? a->rwhy : "-"); }   /* (1.6.1 Part 23 C: resting, idle, why it may not rest) */
	for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (!c->ws) continue;   /* (no workspace: going, 12.3c) */
		if (c->wait == 1) { g_string_append_printf(s, "clientwait %lx %lx app=%s\n", (unsigned long)c->w, (unsigned long)c->frame, c->app ? c->app->k : "-"); continue; }
		g_string_append_printf(s, "client %lx %lx %d %d %d %d min=%d max=%d full=%d focus=%d app=%s %s\n", (unsigned long)c->w, (unsigned long)c->frame, c->x, c->y, c->cw, c->ch,
			c->min, c->max, c->full, c == FOCUS, c->app ? c->app->k : c->ak[0] ? c->ak : "-", c->title);
		g_string_append_printf(s, "clientws %lx %d %d\n", (unsigned long)c->w, c->ws, c->off);
		if (c->pg) g_string_append_printf(s, "clientprog %lx %d %.0f\n", (unsigned long)c->w, c->pg, c->pg == 1 ? c->pgt : 0);   /* (1.6.1 Part 25: its progress: 1 a percentage, 2 busy, 3 ending) */
		if (c->csd || c->clip) g_string_append_printf(s, "clientcsd %lx %d %d %d %d %d %d\n", (unsigned long)c->w, c->csd, c->il, c->it, c->ir, c->ib, c->tbcut);   /* (1.6.1 Part 15: its own title bar, its cut) */   /* (item 4: its workspace, shown or not) */
		if (c->ontop) g_string_append_printf(s, "clienttop %lx\n", (unsigned long)c->w);   /* (Always on top: the title bars' test) */
		if (c->hidden) g_string_append_printf(s, "clienthidden %lx\n", (unsigned long)c->w);   /* (1.6.0: out of sight, as the portal's page between its dialogs) */
		if (c->app && c->app->win && c->app->view && GTK_IS_WIDGET(c->app->view)) { GtkAllocation al; gtk_widget_get_allocation(c->app->view, &al); g_string_append_printf(s, "view %lx %d %d\n", (unsigned long)c->w, al.width, al.height); } }   /* (the page's own size: its window's) */
	g_string_append_printf(s, "wm %d\nicons %u %s\n", WM_OK, ICONS ? g_hash_table_size(ICONS) : 0, VER[0] ? VER : "-");
	for (int i = 0; di_shown() && i < NDI; i++) { int x, y; di_xy(&DI[i], &x, &y); g_string_append_printf(s, "dicon %d %d %d %d %d %d %s|%s|%s\n", DI[i].c, DI[i].r, x, y, di_w(), DI[i].sel, DI[i].k, DI[i].kind, DI[i].n); g_string_append_printf(s, "dicpic %d %s\n", DI[i].ic[0] && ICONS && g_hash_table_lookup(ICONS, DI[i].ic) ? 1 : DI[i].icon[0] ? 2 : 0, DI[i].n);   /* (its picture: 1 the page's, 2 its emoji, 0 none: blank) */ if (strchr(DI[i].k, ':')) g_string_append_printf(s, "diconic %s %s %d\n", DI[i].k, DI[i].ic[0] ? DI[i].ic : "-", DI[i].ic[0] && ICONS && g_hash_table_lookup(ICONS, DI[i].ic) ? 1 : 0); }
	g_string_append_printf(s, "dmenu %s\n", DI_MENU);
	g_string_append_printf(s, "deskxfer %d %d %s\n", FC_N, DXD_N, DXD_LAST);   /* (1.6.1 Part 28: the clipboard's items, Explorer drops on the desktop, the last one's target) */
	/* (item 2) the widgets on the screen: widget N TYPE X Y W H (the screen's pixels) PAGE-X PAGE-Y|TEXT; the note typed in; the weather */
	for (int i = 0; wg_shown() && i < NWG; i++) { double X, Y, W, H; wg_rect(&WG[i], &X, &Y, &W, &H); char *t = g_strescape(WG[i].txt ? WG[i].txt : "", NULL);
		g_string_append_printf(s, "widget %d %s %d %d %d %d %g %g|%s\n", i, WG[i].t, (int)lround(X * Z), (int)lround(Y * Z), (int)lround(W * Z), (int)lround(H * Z), WG[i].x, WG[i].y, t); g_free(t); }
	g_string_append_printf(s, "wgnote %d %d %d\nweather %d %s|%s|%s\n", NF, NCAR, NANC, WX_ST, WX_T, WX_C, WX_L);
	g_string_append_printf(s, "mon %s|%s|%d|%d\n", MON_CPU, MON_MEM, wg_nwin(), WG_MON_T != 0);   /* (the system monitor's values, its timer) */
	g_string_append(s, "sounds"); for (int i = 0; i < SNDN; i++) g_string_append_printf(s, " %s:%d", SNDK[i], SND_PLAYED[i]); g_string_append_c(s, '\n');   /* (report 2) */
	/* (the page's menu and dialogs as the shell shows them) */
	if (CMWIN && gtk_widget_get_visible(CMWIN)) { g_string_append_printf(s, "cmenu %d %d %d %d %s\n", (int)lround(CMX * Z), (int)lround(CMY * Z), (int)lround(CMBW * Z), (int)lround(CMBH * Z), CMTITLE[0] ? CMTITLE : "-");
		double ry = CMY + 5 + (CMTITLE[0] ? 22 : 0); for (int i = 0, k = 0; i < NCMI; i++) { if (!CMIT[i].sep && CMIT[i].icon[0]) g_string_append_printf(s, "cmicon %d %s\n", k, CMIT[i].icon); if (!CMIT[i].sep) k++; }
		for (int i = 0; i < NCMI; i++) { if (!CMIT[i].sep) g_string_append_printf(s, "cmitem %d %d %d %s|%s\n", (int)lround((CMX + 5 + 50) * Z), (int)lround((ry + 12) * Z), CMIT[i].dis, CMIT[i].act, CMIT[i].label); ry += cm_row_h(&CMIT[i]); } }
	g_string_append_printf(s, "dialog %s %s|%s\n", DGK == DLG_RENAME ? "rename" : DGK == DLG_OPENWITH ? "openwith" : "-", DGK ? DGNAME : "-", DGMSG);
	for (int i = 0; DGK && i < NDGB; i++) g_string_append_printf(s, "dgbtn %d %d %d\n", i, (int)lround((DGB[i].x + DGB[i].w / 2) * Z), (int)lround((DGB[i].y + DGB[i].h / 2) * Z));
	if (MENU && gtk_widget_get_visible(MENU)) g_string_append_printf(s, "mbox %d %d %d %d\n", (int)lround(MBX * Z), (int)lround(MBY * Z), (int)lround(MBW * Z), (int)lround(MBH * Z));   /* (the start menu's box, for the tests) */
	if (FLK) { g_string_append_printf(s, "fly %s %d %d %d %d %d-%02d %d %d\n", FLK == FL_CAL ? "cal" : FLK == FL_VOL ? "vol" : FLK == FL_NET ? "net" : FLK == FL_QS ? "qs" : FLK == FL_BRI ? "bri" : "nc", (int)lround(FLX * Z), (int)lround(FLY * Z), (int)lround(FLW * Z), (int)lround(FLH * Z), FL_Y, FL_M, FLVOL, FLMUTE);
		if (FLK == FL_QS) { g_string_append(s, "flyqs"); for (int i = 0; i < QSN; i++) if (qs_get(i)) g_string_append_printf(s, " %s", QSL[i]); g_string_append_c(s, '\n'); }
		if (FLK == FL_NC) { g_string_append_printf(s, "flync %d %d", NNT, S.dnd); for (int i = 0; i < NNT; i++) g_string_append_printf(s, "|%s", NT[i].t); g_string_append_c(s, '\n'); }
		for (int i = 0; i < NFLB; i++) g_string_append_printf(s, "flybtn %s %d %d\n", FLBK[i], (int)lround((FLB[i].x + FLB[i].w / 2) * Z), (int)lround((FLB[i].y + FLB[i].h / 2) * Z)); }
	else g_string_append(s, "fly -\n");
	g_string_append_printf(s, "oracle %d %d\ntor %d %d\n", OR_RUN, (int)OR_PID, TOR_RUN, (int)TOR_PID);   /* (12.3c: Oracle's Chromium running, rudolf-launch's process) */
	g_string_append_printf(s, "devlink %d %d\ndlask %s\n", DL_ON && DLBAN && gtk_widget_get_visible(DLBAN), TOPRES, DLASK && gtk_widget_get_visible(DLASK) ? DL_PEND : "-");
	char tmp[600]; snprintf(tmp, sizeof tmp, "%s.tmp", STATEF); g_file_set_contents(tmp, s->str, s->len, NULL); rename(tmp, STATEF); g_string_free(s, TRUE);
}

/* ------------------------------------------------------------------ idle windows give their memory back (1.6.1, docs/plan-1.6.1.md Part 23 C)
   The owner, 2026-11-20 ("if it won't cause any issues with Oracle or other Rudolf/Debian/Flatpak/Wine apps, then add it"): a Rudolf
   app's window minimized, or covered by one other window, for a while (Settings › System, Idle windows: on by default under 6 GB of
   memory, after 10 minutes) has its page unloaded (an empty page in its view: WebKit frees the page's memory, and a web process of its
   own, if it has one, keeps only WebKit's), and the page loaded again when the window is shown (focused, its taskbar button, no longer
   covered), back where it was: the page gives its state first (oneapp59.js __oaRest: its file, folder, tabs, scroll), which comes back
   as &rest= in its address. Only apps whose page says it can (REST_APPS: Notepad, File Explorer), and only when it says so now: not with
   unsaved changes, a dialog, sound or video playing, a recording, a call, a transfer or a download, a job running. Never: Oracle
   ("browser"), a program rudolfd runs (Linux, Debian, Flatpak, Wine, the Library's: separate X windows, their pages unseen), the shell's
   hidden pages, a page doing something for the shell (do-*), a window full screen, on top or in a dialog over the screen; and WebKit
   itself says when a page is playing sound or using the camera, the microphone or the screen. Task Manager shows it as "Resting (no
   memory used)". (The tests set the time in seconds: RUDOLF_NATIVE_REST_SECS.) */
static int mem_small(void)
{
	static int v = -1; if (v >= 0) return v; long long kb = 0; char ln[128]; FILE *f = fopen("/proc/meminfo", "r");
	while (f && fgets(ln, sizeof ln, f)) if (sscanf(ln, "MemTotal: %lld kB", &kb) == 1) break; if (f) fclose(f);
	return v = kb > 0 && kb < 6LL * 1024 * 1024;
}
static int rest_on(void) { return S.idlerest < 0 ? mem_small() : S.idlerest; }
static gint64 rest_after(void) { const char *e = getenv("RUDOLF_NATIVE_REST_SECS"); if (e && atoi(e) > 0) return (gint64)atoi(e) * G_USEC_PER_SEC; return (gint64)(S.idlemin > 0 ? S.idlemin : 10) * 60 * G_USEC_PER_SEC; }
/* (covered: one window above it, shown, holding all of its frame; or one full screen) */
static int covered(Client *c)
{
	if (!WM_OK || !c || c->min || c->off) return 0;
	Window r, p, *k = NULL; unsigned n = 0; int above = 0, cov = 0;
	if (XQueryTree(DPY, ROOT, &r, &p, &k, &n)) for (unsigned i = 0; i < n && !cov; i++) {
		if (k[i] == c->frame) { above = 1; continue; } if (!above) continue;
		Client *d = client_of(k[i]); if (!d || d == c || d->frame != k[i] || d->min || d->hidden || d->off) continue;
		if (d->full || (d->x <= c->x && d->y <= c->y && d->x + fw_of(d) >= c->x + fw_of(c) && d->y + fh_of(d) >= c->y + fh_of(c))) cov = 1; }
	if (k) XFree(k); return cov;
}
/* (what keeps a window from resting, as the shell sees it; NULL: nothing) */
static const char *rest_no(AppWin *a, Client *c)
{
	if (!a->view || a->closing || a->hidden || !a->sized) return "not an app's window";
	if (!strncmp(a->k, "do-", 3) || is_bg(a)) return "the shell's page";
	if (!strcmp(a->k, "browser")) return "Oracle";
	{ const App *ap = app_find(a->k); if ((ap && ap->x) || strchr(a->k, ':')) return "a program of its own"; }
	if (a->modal) return "a dialog"; if (c->full) return "full screen"; if (c->ontop) return "always on top";
	if (webkit_web_view_is_playing_audio(WEBKIT_WEB_VIEW(a->view))) return "playing sound";
#if WEBKIT_CHECK_VERSION(2, 34, 0)
	{ WebKitWebView *v = WEBKIT_WEB_VIEW(a->view); if (webkit_web_view_get_camera_capture_state(v) != WEBKIT_MEDIA_CAPTURE_STATE_NONE || webkit_web_view_get_microphone_capture_state(v) != WEBKIT_MEDIA_CAPTURE_STATE_NONE
		|| webkit_web_view_get_display_capture_state(v) != WEBKIT_MEDIA_CAPTURE_STATE_NONE) return "recording"; }
#endif
	return NULL;
}
static void rest_cover(Client *c)
{
	if (!c || c->cover) return; XSetWindowAttributes at = {0}; at.background_pixel = app_bg_pixel();
	c->cover = XCreateWindow(DPY, c->frame, BW, TH, c->cw, c->ch, 0, CopyFromParent, InputOutput, CopyFromParent, CWBackPixel, &at); XMapRaised(DPY, c->cover);
}
static void appwin_rest(AppWin *a, const char *st)
{
	Client *c = client_of_app(a); const char *u = webkit_web_view_get_uri(WEBKIT_WEB_VIEW(a->view)); if (!c || !u || strncmp(u, "http", 4)) return;
	GString *g = g_string_new(u); char *r = strstr(g->str, "&rest="); if (r) g_string_truncate(g, r - g->str);
	/* (not the file it was opened with: what it has open now is in its state) */
	{ char *f = strstr(g->str, "&file="); if (f) { char *e = strchr(f + 1, '&'); g_string_erase(g, f - g->str, e ? e - f : (gssize)strlen(f)); } }
	char *e = g_uri_escape_string(st, NULL, FALSE); g_string_append(g, "&rest="); g_string_append(g, e); g_free(e);
	g_free(a->rurl); a->rurl = g_string_free(g, FALSE); a->rest = 1; a->sready = 0;
	rest_cover(c); if (a->wait_t) { g_source_remove(a->wait_t); a->wait_t = 0; } c->wait = 2;
	fprintf(stderr, "rudolf-native: %s's window rests (its page closed)\n", a->k);
	/* (its view closed, and so its page, and its web process when it has one of its own (WebKit ends a process that has no page left:
	   the cache model keeps none); a view never loaded in its place, which has no process at all, so that a->view stays a view) */
	GtkWidget *old = a->view, *nv = view_make(a, NULL); a->view = nv; gtk_container_remove(GTK_CONTAINER(a->win), old);
	gtk_container_add(GTK_CONTAINER(a->win), nv); gtk_widget_show(nv); gtk_widget_grab_focus(nv); bar_redraw(); state_write();
}
static void appwin_wake(AppWin *a)
{
	if (!a || !a->rest || !a->rurl) return; a->rest = 0; a->idle_t = 0; Client *c = WM_OK ? client_of_app(a) : NULL;
	if (c) { rest_cover(c); c->wait = 2; }   /* (the app's colour until its page is ready, as a window opening: appwin_reveal) */
	if (a->wait_t) g_source_remove(a->wait_t); a->wait_t = g_timeout_add(10000, appwin_wait_last, a);
	fprintf(stderr, "rudolf-native: %s's window wakes (its page loaded again)\n", a->k);
	char *u = a->rurl; a->rurl = NULL;
	/* (opened as every window is, by another window's page (window.open: the same web process), into this window: on_create's into) */
	AppWin *rel = NULL; if (!own_process(a->k)) for (GList *l = WINS; l; l = l->next) { AppWin *x = l->data; if (x != a && x->view && !x->rest && webkit_web_view_get_uri(WEBKIT_WEB_VIEW(x->view)) && !x->closing) { rel = x; break; } }
	if (rel) { Pending *p = g_new0(Pending, 1); snprintf(p->k, sizeof p->k, "%s", a->k); p->id = a->id; p->into = a; g_queue_push_tail(&PENDING, p);
		char *ue = g_uri_escape_string(u, NULL, FALSE), *js = g_strdup_printf("(typeof webNativeOpen=='function'?webNativeOpen:window.open)(decodeURIComponent('%s'),'_blank')", ue);
		webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(rel->view), js, -1, NULL, NULL, NULL, NULL, NULL); g_free(js); g_free(ue); }
	else webkit_web_view_load_uri(WEBKIT_WEB_VIEW(a->view), u);
	g_free(u); state_write();
}
static void rest_got(GObject *src, GAsyncResult *res, gpointer d)
{
	int id = GPOINTER_TO_INT(d); JSCValue *v = webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(src), res, NULL); char *t = v && jsc_value_is_string(v) ? jsc_value_to_string(v) : NULL; if (v) g_object_unref(v);
	AppWin *a = NULL; for (GList *l = WINS; l; l = l->next) if (((AppWin *)l->data)->id == id) a = l->data;
	if (!a) { g_free(t); return; } a->rasking = 0; Client *c = WM_OK ? client_of_app(a) : NULL;
	JV *j = t && t[0] == '{' ? jparse(t) : NULL; char st[6000] = "", why[80] = "";
	int ok = j && jbool(j, "ok"); if (j) { jstr(j, "st", st, sizeof st); jstr(j, "why", why, sizeof why); jfree(j); } g_free(t);
	if (!ok) { snprintf(a->rwhy, sizeof a->rwhy, "%s", why[0] ? why : "its page cannot be restored"); a->idle_t = g_get_monotonic_time(); state_write(); return; }   /* (asked again after the same time) */
	if (!c || a->rest || !rest_on() || rest_no(a, c) || !(c->min || covered(c))) return;
	a->rwhy[0] = 0; appwin_rest(a, st);
}
static gboolean rest_tick(gpointer d)
{
	(void)d; if (!USER_ID[0] || LOCKED || !WM_OK) return G_SOURCE_CONTINUE;
	gint64 now = g_get_monotonic_time(); int on = rest_on(), ch = 0;
	for (GList *l = WINS; l; l = l->next) { AppWin *a = l->data; Client *c = client_of_app(a); if (!c) continue;
		if (a->rest) { if (!c->min && !c->off && !covered(c)) appwin_wake(a); continue; }
		const char *no = on ? rest_no(a, c) : "off"; int idle = c->min || covered(c);
		if (no || !idle) { if (a->idle_t || (no && strcmp(a->rwhy, no))) ch = 1; a->idle_t = 0; snprintf(a->rwhy, sizeof a->rwhy, "%s", no ? no : ""); continue; }
		if (!a->idle_t) { a->idle_t = now; ch = 1; continue; }
		if (now - a->idle_t >= rest_after() && !a->rasking) { a->rasking = 1;
			webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(a->view), "window.__oaRest?__oaRest():''", -1, NULL, NULL, NULL, rest_got, GINT_TO_POINTER(a->id)); } }
	if (ch) state_write();
	return G_SOURCE_CONTINUE;
}

/* ------------------------------------------------------------------ start */
/* (tests only: RUDOLF_NATIVE_TEST_EVAL=FIFO, never set by the session) each line "KEY JS" runs JS in the shown app window of that
   app and appends "KEY RESULT" to FIFO.out: the host tests read the WebKit windows' pages with it, as DevTools reads Chromium's */
static char *TEVAL_OUT;
static void teval_got(GObject *src, GAsyncResult *res, gpointer d)
{
	char *k = d; JSCValue *v = webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(src), res, NULL); char *t = v ? jsc_value_to_string(v) : g_strdup("ERROR");
	for (char *c = t; *c; c++) if (*c == '\n') *c = ' ';
	FILE *f = fopen(TEVAL_OUT, "a"); if (f) { fprintf(f, "%s %s\n", k, t); fclose(f); } g_free(t); g_free(k); if (v) g_object_unref(v);
}
static gboolean teval_line(GIOChannel *ch, GIOCondition c, gpointer d)
{
	(void)c; (void)d;
	/* (every line read now: two lines that came at once left the second in the channel's buffer, with no new event for it, so each
	   answer came one request late from then on: n160_t.py in QEMU) */
	do { gchar *l = NULL; gsize n = 0; GIOStatus st = g_io_channel_read_line(ch, &l, &n, NULL, NULL);
	if (st == G_IO_STATUS_EOF || st == G_IO_STATUS_ERROR) { g_free(l); return G_SOURCE_REMOVE; }
	if (st == G_IO_STATUS_AGAIN || !l) { g_free(l); break; }
	char *sp = strchr(l, ' ');
	if (sp) { *sp = 0; g_strchomp(sp + 1); AppWin *a = NULL; for (GList *w = WINS; w; w = w->next) { AppWin *x = w->data; if ((!strcmp(x->k, l) || (is_bg(x) && (!strcmp(l, "do-svc") || !strcmp(l, "do-portal")))) && (!x->hidden || is_bg(x)) && x->view) { a = x; break; } }   /* (and the desktop's service page, never shown: item 12; 1.6.1: the shell's hidden page, do-bg, also by the names of the pages it replaced) */
		if (a) webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(a->view), sp + 1, -1, NULL, NULL, NULL, teval_got, g_strdup(l));
		else { FILE *f = fopen(TEVAL_OUT, "a"); if (f) { fprintf(f, "%s NO WINDOW\n", l); fclose(f); } } }
	g_free(l); } while (g_io_channel_get_buffer_condition(ch) & G_IO_IN);
	return G_SOURCE_CONTINUE;
}
static void teval_init(void)
{
	const char *p = getenv("RUDOLF_NATIVE_TEST_EVAL"); if (!p || !*p) return;
	int fd = open(p, O_RDWR | O_NONBLOCK | O_CLOEXEC); if (fd < 0) return;
	TEVAL_OUT = g_strconcat(p, ".out", NULL); GIOChannel *ch = g_io_channel_unix_new(fd); g_io_channel_set_close_on_unref(ch, TRUE);
	g_io_add_watch(ch, G_IO_IN, teval_line, NULL); g_io_channel_unref(ch);
}

/* (1.6.0, the owner's health log: one start-up "ready" after 197 s, the others after 27-37 s) The health log's "the desktop was ready N s
   after start-up" comes from the desktop page when it first loads after a start (sec50.js). On Standard that page is up behind the
   sign-in screen; on Native the pages load only once someone has signed in (the first app's window, the start-up page), so the time
   included how long the sign-in screen waited for its password. The shell says it here instead, as soon as its sign-in screen (or the
   desktop, a session going on) is shown, with what took the time: when the shell itself started (the session's part before it) and
   how long it took to show its screen. Once per start-up, as the page's (a 'ready' newer than the last 'start'; within 15 minutes). */
static double proc_up(void) { double u = -1; FILE *f = fopen("/proc/uptime", "r"); if (f) { if (fscanf(f, "%lf", &u) != 1) u = -1; fclose(f); } return u; }
static double self_started(void)
{
	char b[1024]; FILE *f = fopen("/proc/self/stat", "r"); if (!f) return -1; size_t n = fread(b, 1, sizeof b - 1, f); fclose(f); b[n] = 0;
	char *p = strrchr(b, ')'); if (!p) return -1; unsigned long long st = 0; int i = 0;   /* (field 22, after the name in parentheses) */
	for (char *t = strtok(p + 2, " "); t; t = strtok(NULL, " ")) if (++i == 20) { st = strtoull(t, NULL, 10); break; }
	long hz = sysconf(_SC_CLK_TCK); return hz > 0 && st ? (double)st / hz : -1;
}
static gboolean ready_note(gpointer what)
{
	double st = self_started(), up0 = proc_up();
	GByteArray *b = NULL; if (http("GET", "/api/health?n=20", NULL, 0, &b) != 200 || !b) { if (b) g_byte_array_free(b, TRUE); return G_SOURCE_REMOVE; }
	g_byte_array_append(b, (const guint8 *)"", 1); JV *v = jparse((const char *)b->data); g_byte_array_free(b, TRUE);
	double up = jnum(v, "uptime", up0); if (up < 0 || up > 900) { jfree(v); return G_SOURCE_REMOVE; }   /* (rudolfd's uptime, as the page's) */
	double bk = jnum(v, "beforeKernel", -1);   /* (1.6.1: power-on to this kernel's clock: firmware, boot loader, a handover's first stage) */
	if (up0 >= 0 && st >= 0) st -= up0 - up;   /* (the shell's start on the same clock) */
	JV *ev = jprop(v, "events"); double ts = -1, tr = -1;   /* (newest first) */
	for (int i = 0; ev && i < ev->len; i++) { JV *e = jat(ev, i); char k[16] = ""; jstr(e, "k", k, sizeof k); double t = jnum(e, "t", -1);
		if (!strcmp(k, "start") && ts < 0) ts = t; if (!strcmp(k, "ready") && tr < 0) tr = t; }
	jfree(v);
	if (ts < 0 || (tr >= 0 && tr >= ts)) return G_SOURCE_REMOVE;
	char pw[48] = ""; if (bk > 0) snprintf(pw, sizeof pw, "; %.0f s from power-on", up + bk);
	char m[340]; snprintf(m, sizeof m, "the desktop was ready %.0f s after start-up (Native: %s shown; the desktop program started at %.0f s, its screen %.0f s later%s)", up, (const char *)what, st, st >= 0 ? up - st : -1, pw);
	GString *j = g_string_new("{\"k\":\"ready\",\"m\":"); json_esc(j, m); g_string_append(j, "}");
	http("POST", "/api/health", j->str, j->len, NULL); g_string_free(j, TRUE); fprintf(stderr, "rudolf-native: %s\n", m);
	return G_SOURCE_REMOVE;
}
int main(int argc, char **argv)
{
	for (int i = 1; i < argc; i++) {
		if (!strncmp(argv[i], "--size=", 7)) { if (sscanf(argv[i] + 7, "%dx%d", &SW, &SH) != 2) SW = SH = 0; }
		else if (!strcmp(argv[i], "--draw=soft")) { HWPOL = WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER; SOFT = 1; }
		else if (!strcmp(argv[i], "--draw=mixed")) { char n[8]; int c = (int)g_get_num_processors() - 1; snprintf(n, sizeof n, "%d", c < 1 ? 1 : c > 8 ? 8 : c);
			g_setenv("WEBKIT_SKIA_ENABLE_CPU_RENDERING", "1", TRUE); g_setenv("WEBKIT_SKIA_CPU_PAINTING_THREADS", n, TRUE); }
		else if (!strncmp(argv[i], "--base=http://", 14)) { char h[64]; int p = 0; if (sscanf(argv[i] + 14, "%63[^:/]:%d", h, &p) == 2 && p > 0) { snprintf(HOST, sizeof HOST, "%s", h); PORT = p; } }
	}
	STATEF = getenv("RUDOLF_NATIVE_STATE");
	/* (1.6.0, the owner's report 2: no sound from the app windows on the Vivobook) WebKit gave each media element (<audio>, <video>:
	   the Media Player, games, videos) a PulseAudio stream of its own, with the element's volume and mute set on the stream itself
	   and the stream named by its role (media.role "music"). PulseAudio keeps a stream's volume by that name (module-stream-restore,
	   kept across restarts), so once any page muted or turned down one (a muted video, the Media Player's mute, an app's volume at
	   0), every later sound of every app window started muted or at 0 %. Chromium mixes its pages' sounds itself, in one stream with
	   no role. WebKit's own mixer does the same: the elements' volumes in software, one stream for the window's sounds, named by the
	   program (as Chromium's), so nothing a page does is kept for the others. (It needs GStreamer's "inter" plug-in, shipped with
	   WebKit: addwebkit.sh) */
	if (!getenv("WEBKIT_GST_ENABLE_AUDIO_MIXER")) g_setenv("WEBKIT_GST_ENABLE_AUDIO_MIXER", "1", TRUE);
	g_set_prgname("RudolfNative"); gdk_set_program_class("RudolfNative");
	gtk_init(&argc, &argv);
	/* (1.6.0 Part 12, item 8) the apps' text as Chromium draws it: grey anti-aliasing, slight hinting (the shell's own Pango
	   text is drawn so already); without XSETTINGS, GTK would give WebKit full hinting, its letters wider than Chromium's */
	if (!getenv("RUDOLF_NATIVE_GTKHINT")) g_object_set(gtk_settings_get_default(), "gtk-xft-antialias", 1, "gtk-xft-hinting", 1, "gtk-xft-hintstyle", "hintslight", "gtk-xft-rgba", "none", NULL);
	/* (test finding N9) the apps' dates and times in Chromium's locale when the system has none: Chromium then uses en-US (a time
	   field "07:00 AM"), WebKit the C locale ("07:00", no AM or PM). Set for WebKit's own processes, after GTK has set this one's */
	{ const char *l = getenv("LC_ALL"); if (!l || !*l) l = getenv("LC_TIME"); if (!l || !*l) l = getenv("LANG");
	  if (!l || !*l || !strcmp(l, "C") || !strncmp(l, "C.", 2) || !strcmp(l, "POSIX")) { g_unsetenv("LC_ALL"); g_unsetenv("LC_TIME"); g_setenv("LANG", "en_US.UTF-8", TRUE); } }
	wm_init();
	if (SW <= 0 || SH <= 0) { GdkScreen *scr = gdk_screen_get_default(); SW = gdk_screen_get_width(scr); SH = gdk_screen_get_height(scr); }
	mon_init();   /* (1.6.1 Part 1: several monitors: SW and SH the main one's) */
	/* rudolfd may still be starting */
	for (int i = 0; i < 60 && !get_token(); i++) g_usleep(500000);
	if (!TOKEN[0]) { fprintf(stderr, "rudolf-native: rudolfd did not answer on %s:%d\n", HOST, PORT); return 1; }
	memset(&S, 0, sizeof S); S.lat = NAN; snprintf(S.tb, sizeof S.tb, "bottom");
	LOGO = pix_get("/favicon.png");
	/* signed in already (this program restarted): the session goes on. Known before the first settings (1.6.0, the owner's report
	   4: the widgets taken then had no timers) */
	JV *ses = jget("/api/desktop/session");
	if (ses) { jstr(ses, "id", USER_ID, sizeof USER_ID); jstr(ses, "n", USER_N, sizeof USER_N); jstr(ses, "r", USER_R, sizeof USER_R); jstr(ses, "web", WEB_DIR, sizeof WEB_DIR); jfree(ses); }
	css_apply(); settings_load(); apps_load(); tray_load();
	/* the wallpaper, below everything */
	DESK = gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_window_set_decorated(GTK_WINDOW(DESK), FALSE); gtk_window_set_type_hint(GTK_WINDOW(DESK), GDK_WINDOW_TYPE_HINT_DESKTOP);
	gtk_window_set_title(GTK_WINDOW(DESK), "Desktop"); gtk_window_set_keep_below(GTK_WINDOW(DESK), TRUE); gtk_window_set_default_size(GTK_WINDOW(DESK), SW, SH); gtk_window_move(GTK_WINDOW(DESK), 0, 0);
	DESKAREA = gtk_drawing_area_new(); g_signal_connect(DESKAREA, "draw", G_CALLBACK(desk_area_draw), NULL); gtk_container_add(GTK_CONTAINER(DESK), DESKAREA);
	/* the desktop's icons (1.6.0 Part 12, item 1) */
	gtk_widget_add_events(DESKAREA, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_KEY_PRESS_MASK); gtk_widget_set_can_focus(DESKAREA, TRUE);
	g_signal_connect(DESKAREA, "button-press-event", G_CALLBACK(desk_press), NULL); g_signal_connect(DESKAREA, "button-release-event", G_CALLBACK(desk_release), NULL);
	g_signal_connect(DESKAREA, "motion-notify-event", G_CALLBACK(desk_motion), NULL); g_signal_connect(DESKAREA, "key-press-event", G_CALLBACK(desk_key), NULL);
	g_signal_connect(DESK, "delete-event", G_CALLBACK(gtk_true), NULL);
	xd_init(DESKAREA);   /* (files dropped from outside: 1.6.1 Part 13) */
	pn_init(DESKAREA);   /* (1.6.1 Part 2: the mobile build's panels, swiped) */
	/* (the widgets' note: typing, item 2) */
	gtk_widget_add_events(DESKAREA, GDK_BUTTON_MOTION_MASK | GDK_FOCUS_CHANGE_MASK); NIM = gtk_im_context_simple_new(); g_signal_connect(NIM, "commit", G_CALLBACK(note_commit), NULL);
	g_signal_connect(DESKAREA, "realize", G_CALLBACK(im_realize), NIM); g_signal_connect(DESK, "focus-out-event", G_CALLBACK(on_desk_out), NULL);
	gtk_widget_show_all(DESK);
	/* the taskbar, above everything */
	BAR = gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_window_set_decorated(GTK_WINDOW(BAR), FALSE); gtk_window_set_type_hint(GTK_WINDOW(BAR), GDK_WINDOW_TYPE_HINT_DOCK);
	gtk_window_set_title(GTK_WINDOW(BAR), "Taskbar"); gtk_window_set_keep_above(GTK_WINDOW(BAR), TRUE); gtk_window_set_default_size(GTK_WINDOW(BAR), SW, BARH);
	BARAREA = gtk_drawing_area_new(); gtk_widget_add_events(BARAREA, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK);
	g_signal_connect(BARAREA, "scroll-event", G_CALLBACK(bar_scroll), NULL);
	g_signal_connect(BARAREA, "draw", G_CALLBACK(bar_draw), NULL); g_signal_connect(BARAREA, "button-press-event", G_CALLBACK(bar_press), NULL);
	g_signal_connect(BARAREA, "button-release-event", G_CALLBACK(bar_release), NULL); g_signal_connect(BARAREA, "motion-notify-event", G_CALLBACK(bar_motion), NULL);
	g_signal_connect(BARAREA, "leave-notify-event", G_CALLBACK(bar_leave), NULL); gtk_container_add(GTK_CONTAINER(BAR), BARAREA);
	g_signal_connect(BAR, "delete-event", G_CALLBACK(gtk_true), NULL);
	bar_place(); gtk_widget_realize(BAR);
	/* signed in already: the session goes on (read above); else the sign-in screen */
	if (USER_ID[0]) { gtk_widget_show_all(BAR); start_page(); }
	else { signin_show(0); snd_play("startup"); }   /* (the start-up chime at the sign-in screen, as the page's: report 2) */
	g_timeout_add(1500, ready_note, USER_ID[0] ? "the desktop" : "the sign-in screen");   /* (the health log's start-up time: 1.6.0) */
	g_timeout_add_seconds(3, tick_settings, NULL); g_timeout_add_seconds(10, tick_tray, NULL); g_timeout_add_seconds(5, tick_mic, NULL); g_timeout_add_seconds(15, tick_idle, NULL); g_timeout_add_seconds(5, tick_clock, NULL);
	tick_heart(NULL); g_timeout_add_seconds(10, tick_heart, NULL);
	tick_devlink(NULL); g_timeout_add_seconds(4, tick_devlink, NULL);   /* (as the page: every 4 s) */
	tick_cell(NULL); g_timeout_add_seconds(5, tick_cell, NULL);   /* (1.6.0 Part 3: the cellular notices, as the page: every 5 s) */
	/* (1.6.0) rudolfd's events: the native desktop is the one that hears them (the power key, the keys' indicator, Ctrl+Alt+Del);
	   the brightness's button and the backlight watched (the firmware's own changes shown as the keys' are) */
	bri_load(); bar_redraw(); BL_LAST = bl_read(); g_timeout_add(300, bl_tick, NULL);
	g_thread_unref(g_thread_new("events", ev_thread, NULL));
	g_thread_unref(g_thread_new("portal", pt_thread, NULL));   /* (1.6.0: the portal's requests, collected here) */
	g_timeout_add_seconds(2, gm61_tick, NULL);   /* (1.6.1 Part 19: game mode) */
	g_timeout_add_seconds(1, lxp_tick, NULL);   /* (1.6.2 Part 25b: Linux apps' progress on their taskbar buttons) */
	g_timeout_add_seconds(2, svc_tick, NULL);   /* (1.6.0 Part 12, item 12: the desktop's service page, kept open while an account is signed in) */
	g_timeout_add_seconds(2, rest_tick, NULL);   /* (1.6.1 Part 23 C: idle windows give their memory back) */
	teval_init(); clip_init();
	mon_start();   /* (1.6.1 Part 1: the other monitors' wallpapers, taskbars and covers) */
	state_write();
	gtk_main();
	return 0;
}
