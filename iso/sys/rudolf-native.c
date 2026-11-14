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
#include <cairo-xlib.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
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

/* ------------------------------------------------------------------ state */
static char HOST[64] = "127.0.0.1", TOKEN[64];
static int PORT = 8080, SW, SH, BARH = 46, TH = 35;
static double Z = 1;                          /* Display Properties' display scaling (scale: 75 to 250 %), as the page's zoom */
static int TOPRES;                            /* the development link's banner, at the top of the screen (0: none) */
static WebKitHardwareAccelerationPolicy HWPOL = WEBKIT_HARDWARE_ACCELERATION_POLICY_ON_DEMAND;
static int SOFT;                              /* --draw=soft: the lighter effects, as rudolf-webkit */
static const char *STATEF;                    /* RUDOLF_NATIVE_STATE: what is where, for the tests */
typedef struct {
	char bs[16], b1[16], b2[16], dyn[48], dm[8], wpfit[12], tb[8], tbstyle[12], tbacm[12], tbbstyle[12], tbbacm[12];
	char twstyle[12], twacm[12], accent[16], df[8], tz[64], acct[80], wpacct[80], icons[12], ff[48];
	int tbac, tbbac, h24, wpimg, wallpaper, notes, npins, scale, darkui; double lat, doff, gen; char pins[24][40];
} Settings;
static Settings S;
typedef struct { char k[40], name[80], icon[48], kw[200], ic[24]; } App;
static App *APPS; static int NAPPS; static GHashTable *BG;   /* key -> "dark\nlight" */
static GHashTable *ICONS;                     /* an app's picture (App.ic) -> GdkPixbuf, as the page draws it (desktop/apps) */
static GdkPixbuf *pix_from_bytes(GByteArray *b);
static GdkPixbuf *LOGO30, *LOGO84;           /* the page's own logo pictures: the start button's and the sign-in screen's (rudolfd's page) */
static char VER[32];                          /* Rudolf OS's version, for the start button (the page's RVER) */
typedef struct { char id[80], n[128], r[12]; int key, old; } Acct;
static Acct ACC[32]; static int NACC, GUEST_OK, SETUP; static char LASTACC[80];
static char USER_ID[80], USER_N[128], USER_R[12];   /* signed in ("" = not) */
static int LOCKED;
/* the tray */
static int NET_RSSI = 1, NET_UP = -1, NET_WIFI, VOL = -1, MUTED, BAT = -1, CHARGING;

/* where windows go: below the development link's banner, beside the taskbar */
static int work_top(void) { return TOPRES + (!strcmp(S.tb, "top") ? BARH : 0); }
static int work_h(void) { return SH - TOPRES - BARH; }

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
static void state_write(void);
typedef struct JV JV; static void di_layout(void); static void di_load(JV *cat);   /* (the desktop's icons, below) */
static void wg_paint(cairo_t *cr); static void wg_load(JV *v); static void wg_dirty_kind(const char *t); static void wg_picker(void);   /* (the widgets, below) */
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

/* ------------------------------------------------------------------ the wallpaper */
static cairo_surface_t *DESKC; static int DESKD;   /* (the desktop as drawn, and whether to draw it again) */
static GtkWidget *DESK, *DESKAREA, *BAR, *BARAREA, *MENU, *SIGN, *DLBAN, *DLASK, *PWR;
static void desk_dirty(void) { DESKD = 1; if (DESKAREA) gtk_widget_queue_draw(DESKAREA); }
static void desk_shadows(void) { if (DESKAREA) gtk_widget_queue_draw(DESKAREA); }
static cairo_surface_t *WALL;   /* drawn at the screen's size */
static char WALL_KEY[300];
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
	if (S.wpimg && S.wallpaper && S.wpacct[0]) snprintf(path, sizeof path, "/api/desktop/wallpaper?acct=%s", S.wpacct);
	else if (S.dyn[0] && BG) { const char *v = g_hash_table_lookup(BG, S.dyn); if (v) { const char *nl = strchr(v, '\n'); if (dark || !nl) snprintf(path, sizeof path, "/%.*s", nl ? (int)(nl - v) : (int)strlen(v), v); else snprintf(path, sizeof path, "/%s", nl + 1); } }
	snprintf(key, sizeof key, "%s|%s|%s|%s|%s|%s|%d|%d", path, S.bs, S.b1, S.b2, S.wpfit, S.wpacct, SW, (int)S.gen);
	if (WALL && !strcmp(key, WALL_KEY)) return;
	snprintf(WALL_KEY, sizeof WALL_KEY, "%s", key);
	if (!WALL) WALL = cairo_image_surface_create(CAIRO_FORMAT_RGB24, SW, SH);
	cairo_t *cr = cairo_create(WALL); GdkPixbuf *p = path[0] ? pix_get(path) : NULL;
	WALL_MISS = S.wpimg && !p;
	if (p) {   /* behind a picture: the first colour (a picture of your own), black or light grey (a drawn one) */
		RGB bg = S.wpimg ? hexc(S.b1, "#000000") : dark ? (RGB){0, 0, 0} : hexc("#e5e7eb", NULL); src(cr, bg, 1); cairo_paint(cr);
		paint_pix(cr, p, SW, SH, S.wpimg ? S.wpfit : "fill"); g_object_unref(p);
	} else pattern_bg(cr, SW, SH);
	cairo_destroy(cr);
	if (DESKAREA) desk_dirty();
}
static gboolean desk_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)w; (void)d; if (WALL) { cairo_set_source_surface(cr, WALL, 0, 0); cairo_paint(cr); } return TRUE; }

/* ------------------------------------------------------------------ app windows (WebKit, one-app mode) */
typedef struct AppWin { GtkWidget *win, *view; char k[40], title[256], icon[48]; int closing, id, sized, hidden; gint64 close_t; } AppWin;   /* (hidden: a page doing something for the shell unseen) */
static GList *WINS;   /* AppWin*, oldest first */
static AppWin *ACTIVE;
static WebKitWebContext *CTX; static WebKitUserContentManager *UCM; static int NEXTID;
static void bar_redraw(void);
static void app_open(const char *k, const char *file);
static void app_open_ex(const char *k, const char *file, const char *extra, int hidden);
static void page_done(JSCValue *v);
static void frame_redraw_app(AppWin *a);
static const App *app_find(const char *k) { for (int i = 0; i < NAPPS; i++) if (!strcmp(APPS[i].k, k)) return &APPS[i]; return NULL; }
static void appwin_size(AppWin *a, int w, int h);
static void on_msg(WebKitUserContentManager *m, WebKitJavascriptResult *r, gpointer d);
static void ctx_make(void)
{
	if (CTX) return;
	char *home = g_build_filename(g_get_home_dir(), ".rudolf-native", NULL), *data = g_build_filename(home, "data", NULL), *cache = g_build_filename(home, "cache", NULL);
	WebKitWebsiteDataManager *m = webkit_website_data_manager_new("base-data-directory", data, "base-cache-directory", cache, NULL);
	WebKitMemoryPressureSettings *mps = webkit_memory_pressure_settings_new();   /* as rudolf-webkit (1.5.8) */
	webkit_memory_pressure_settings_set_memory_limit(mps, 768); webkit_memory_pressure_settings_set_conservative_threshold(mps, 0.33);
	webkit_memory_pressure_settings_set_strict_threshold(mps, 0.5); webkit_memory_pressure_settings_set_kill_threshold(mps, 0); webkit_memory_pressure_settings_set_poll_interval(mps, 15);
	CTX = WEBKIT_WEB_CONTEXT(g_object_new(WEBKIT_TYPE_WEB_CONTEXT, "website-data-manager", m, "memory-pressure-settings", mps, NULL));
	webkit_memory_pressure_settings_free(mps); webkit_web_context_set_cache_model(CTX, WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER);
	g_free(home); g_free(data); g_free(cache); g_object_unref(m);
	/* one user content manager for every app window: WebKit gives a window opened by another the opener's anyway, so the
	   pages say which window they are (w, from the address: oneapp59.js) */
	UCM = webkit_user_content_manager_new();
	webkit_user_content_manager_register_script_message_handler(UCM, "rudolfNative");
	g_signal_connect(UCM, "script-message-received::rudolfNative", G_CALLBACK(on_msg), NULL);
	if (SOFT) webkit_user_content_manager_add_style_sheet(UCM, webkit_user_style_sheet_new(
		"*,*::before,*::after{box-shadow:none!important;text-shadow:none!important;backdrop-filter:none!important}", WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES, WEBKIT_USER_STYLE_LEVEL_USER, NULL, NULL));
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
	if (!file || !file[0] || !k || !k[0] || strspn(k, "abcdefghijklmnopqrstuvwxyz0123456789_-") != strlen(k)) return;
	char *ex = g_strdup_printf("&op=drop&in=%s", k); app_open_ex("do-fs", file, ex, !strcmp(k, "trash")); g_free(ex);
}
static guint VALLOC_T;
static gboolean view_alloc_late(gpointer d) { (void)d; VALLOC_T = 0; state_write(); return G_SOURCE_REMOVE; }
static void on_view_alloc(GtkWidget *w, GdkRectangle *r, gpointer d) { (void)w; (void)r; (void)d; if (!VALLOC_T) VALLOC_T = g_timeout_add(200, view_alloc_late, NULL); }
static void on_view_close(WebKitWebView *v, gpointer d) { (void)v; appwin_close_now(d); }
static void on_msg(WebKitUserContentManager *m, WebKitJavascriptResult *r, gpointer d)
{
	(void)m; (void)d; JSCValue *v = webkit_javascript_result_get_js_value(r); if (!jsc_value_is_object(v)) return;
	char ws[16] = ""; jsstr(v, "w", ws, sizeof ws); int id = atoi(ws); AppWin *a = NULL;
	for (GList *l = WINS; l; l = l->next) if (((AppWin *)l->data)->id == id) a = l->data;
	if (!a) return;
	char op[16] = "", x[256] = "", f[1024] = ""; jsstr(v, "op", op, sizeof op);
	if (!strcmp(op, "close")) appwin_close_now(a);
	else if (!strcmp(op, "done")) page_done(v);
	else if (!strcmp(op, "title")) { jsstr(v, "icon", x, sizeof x); if (x[0]) snprintf(a->icon, sizeof a->icon, "%s", x); bar_redraw(); }
	else if (!strcmp(op, "drop")) { jsstr(v, "in", x, sizeof x); jsstr(v, "file", f, sizeof f); drop_in(f, x); }
	else if (!strcmp(op, "open")) { jsstr(v, "app", x, sizeof x); jsstr(v, "file", f, sizeof f); if (x[0]) app_open(x, f[0] ? f : NULL); }
	else if (!strcmp(op, "size") && !a->sized) { a->sized = 1;
		/* (a page doing something for the shell (#app=do-NAME) that made an app's window is that app's window now: the taskbar's
		   button and what is dropped on it are the app's) */
		jsstr(v, "app", x, sizeof x); if (!strncmp(a->k, "do-", 3) && x[0] && strlen(x) < sizeof a->k && strspn(x, "abcdefghijklmnopqrstuvwxyz0123456789_-") == strlen(x)) { snprintf(a->k, sizeof a->k, "%s", x); bar_redraw(); } jsstr(v, "ww", x, sizeof x); int ww = atoi(x); jsstr(v, "wh", x, sizeof x); int wh = atoi(x); appwin_size(a, ww, wh); }
}
static gboolean on_crash_reload(gpointer v) { webkit_web_view_reload(WEBKIT_WEB_VIEW(v)); return G_SOURCE_REMOVE; }
static void on_crash(WebKitWebView *v, WebKitWebProcessTerminationReason r, gpointer d)
{
	(void)d; fprintf(stderr, "rudolf-native: an app's page stopped (%d): loading it again\n", (int)r); g_timeout_add_seconds(1, on_crash_reload, v);
}
static void clients_forget(AppWin *a);
static void on_win_destroy(GtkWidget *w, gpointer d)
{
	(void)w; AppWin *a = d; WINS = g_list_remove(WINS, a); if (ACTIVE == a) ACTIVE = NULL;
	clients_forget(a);   /* (its frame goes when X says so: not this window's any more) */
	g_free(a); bar_redraw(); state_write();
}
static gboolean on_win_focus(GtkWidget *w, GdkEvent *e, gpointer d) { (void)w; (void)e; ACTIVE = d; bar_redraw(); return FALSE; }
/* ask the app to close, as its ✕ does (it may ask about unsaved work); a second ask after 5 s closes it anyway */
static void appwin_ask_close(AppWin *a)
{
	gint64 now = g_get_monotonic_time();
	if (a->closing && now - a->close_t > 5000000) { appwin_close_now(a); return; }
	a->closing = 1; a->close_t = now;
	webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(a->view), "window.__oneAppClose?window.__oneAppClose():window.close()", -1, NULL, NULL, NULL, NULL, NULL);
}
static gboolean on_delete(GtkWidget *w, GdkEvent *e, gpointer d) { (void)w; (void)e; appwin_ask_close(d); return TRUE; }
/* an app window, its view related to rel's (then WebKit loads it, as a window the page opened) or loading its own */
static GtkWidget *on_create(WebKitWebView *v, WebKitNavigationAction *na, gpointer d);
static AppWin *appwin_new(const char *k, WebKitWebView *rel, int id)
{
	AppWin *a = g_new0(AppWin, 1); snprintf(a->k, sizeof a->k, "%s", k); a->id = id;
	const App *ap = app_find(k); snprintf(a->title, sizeof a->title, "%s", ap ? ap->name : k); snprintf(a->icon, sizeof a->icon, "%s", ap ? ap->icon : "");
	a->view = rel ? GTK_WIDGET(g_object_new(WEBKIT_TYPE_WEB_VIEW, "related-view", rel, "user-content-manager", UCM, NULL))
		: GTK_WIDGET(g_object_new(WEBKIT_TYPE_WEB_VIEW, "web-context", CTX, "user-content-manager", UCM, NULL));
	WebKitSettings *st = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(a->view));
	g_object_set(st, "enable-javascript", TRUE, "javascript-can-access-clipboard", TRUE, "enable-webaudio", TRUE, "enable-media-stream", TRUE,
		"enable-webgl", TRUE, "media-playback-requires-user-gesture", FALSE, "enable-page-cache", FALSE, "enable-write-console-messages-to-stdout", TRUE,
		"javascript-can-open-windows-automatically", TRUE, NULL);
	webkit_settings_set_hardware_acceleration_policy(st, HWPOL);
	g_signal_connect(a->view, "decide-policy", G_CALLBACK(on_decide), NULL);
	g_signal_connect(a->view, "permission-request", G_CALLBACK(on_perm), NULL);
	g_signal_connect(a->view, "notify::title", G_CALLBACK(on_title), a);
	g_signal_connect(a->view, "close", G_CALLBACK(on_view_close), a);
	g_signal_connect(a->view, "web-process-terminated", G_CALLBACK(on_crash), a);
	g_signal_connect(a->view, "create", G_CALLBACK(on_create), a);
	g_signal_connect(a->view, "size-allocate", G_CALLBACK(on_view_alloc), a);
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
	char *fe = file ? g_uri_escape_string(file, NULL, FALSE) : NULL;
	char *u = g_strdup_printf("http://%s:%d/#app=%s&w=%d&sw=%d&sh=%d%s%s%s", HOST, PORT, k, id, SW, SH, fe ? "&file=" : "", fe ? fe : "", extra ? extra : ""); g_free(fe); return u;
}
/* windows asked for and not made yet (made through an open window's window.open: see app_open) */
typedef struct { char k[40]; int id, hidden; } Pending;
static GQueue PENDING = G_QUEUE_INIT;
static GtkWidget *on_create(WebKitWebView *v, WebKitNavigationAction *na, gpointer d)
{
	(void)na; (void)d; Pending *p = g_queue_pop_head(&PENDING);
	if (!p) return NULL;   /* (a page's own window.open: the desktop page sends websites to the browser itself) */
	AppWin *a = appwin_new(p->k, v, p->id); a->hidden = p->hidden; g_free(p);
	g_signal_connect(a->view, "ready-to-show", G_CALLBACK(on_ready), a);
	return a->view;
}
/* an app in a window of its own. All app windows share one WebKit web process: the first starts it, the others are
   opened by an open one (window.open: the page's own, webNativeOpen, as the page sends websites elsewhere), which WebKit keeps in its process (a view merely made "related" to another
   got a process of its own, WebKitGTK 2.52, as navigations may change process) */
static void app_open(const char *k, const char *file) { app_open_ex(k, file, NULL, 0); }
/* (extra: more of the address, already escaped; hidden: a page the shell asks to do something, never shown) */
static void app_open_ex(const char *k, const char *file, const char *extra, int hidden)
{
	if (!USER_ID[0] || LOCKED || strspn(k, "abcdefghijklmnopqrstuvwxyz0123456789_-") != strlen(k) || strlen(k) > 39) return;
	ctx_make(); int id = ++NEXTID; char *u = app_url(k, file, id, extra);
	AppWin *rel = NULL; for (GList *l = WINS; l; l = l->next) { AppWin *x = l->data; if (x->view && webkit_web_view_get_uri(WEBKIT_WEB_VIEW(x->view)) && !x->closing) { rel = x; break; } }
	if (rel) {
		Pending *p = g_new0(Pending, 1); snprintf(p->k, sizeof p->k, "%s", k); p->id = id; p->hidden = hidden; g_queue_push_tail(&PENDING, p);
		char *js = g_strdup_printf("(typeof webNativeOpen=='function'?webNativeOpen:window.open)(%s,'_blank')", "decodeURIComponent('%s')"), *ue = g_uri_escape_string(u, NULL, FALSE), *js2 = g_strdup_printf(js, ue);
		webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(rel->view), js2, -1, NULL, NULL, NULL, NULL, NULL);
		g_free(js); g_free(js2); g_free(ue); g_free(u); return;
	}
	AppWin *a = appwin_new(k, NULL, id); a->hidden = hidden; webkit_web_view_load_uri(WEBKIT_WEB_VIEW(a->view), u); g_free(u);
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
#define BW 1   /* (the page's windows' 1 px edge; the title bar's height, TH, follows the display scaling) */
typedef struct Client { Window w, frame, rz[3]; int x, y, cw, ch, min, max, full, sx, sy, scw, sch, ignore_unmap, hover, press, hidden; char title[256]; AppWin *app; Window trans; Time last_click; } Client;
static GList *CLIENTS;   /* oldest first */
static Client *FOCUS;
static Display *DPY; static Window ROOT; static int WM_OK;
static Atom A_PROTO, A_DEL, A_TAKE, A_WMSTATE, A_NSTATE, A_NFULL, A_NACTIVE, A_NLIST, A_NNAME, A_UTF8, A_NSUP, A_NCHECK, A_CHANGE, A_NHIDDEN;
static Cursor CUR[9];
static int wm_err;
static int wm_errh(Display *d, XErrorEvent *e) { (void)d; if (e->error_code == BadAccess) wm_err = 1; return 0; }
static void clients_forget(AppWin *a) { for (GList *l = CLIENTS; l; l = l->next) if (((Client *)l->data)->app == a) ((Client *)l->data)->app = NULL; }
static Client *client_of(Window w) { for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (c->w == w || c->frame == w || (w && (c->rz[0] == w || c->rz[1] == w || c->rz[2] == w))) return c; } return NULL; }
static int rz_of(Client *c, Window w) { for (int i = 0; i < 3; i++) if (c->rz[i] && c->rz[i] == w) return i; return -1; }
static Client *client_of_app(AppWin *a) { for (GList *l = CLIENTS; l; l = l->next) if (((Client *)l->data)->app == a) return l->data; return NULL; }
static int fw_of(Client *c) { return c->cw + 2 * BW; }
static int fh_of(Client *c) { return c->ch + TH + BW; }
static void wm_title(Client *c)
{
	if (c->app) { snprintf(c->title, sizeof c->title, "%s", c->app->title); return; }
	Atom type; int fmt; unsigned long n = 0, after; unsigned char *d = NULL; c->title[0] = 0;
	if (XGetWindowProperty(DPY, c->w, A_NNAME, 0, 256, False, A_UTF8, &type, &fmt, &n, &after, &d) == Success && d && n) snprintf(c->title, sizeof c->title, "%s", (char *)d);
	if (d) XFree(d);
	if (!c->title[0]) { char *nm = NULL; if (XFetchName(DPY, c->w, &nm) && nm) { snprintf(c->title, sizeof c->title, "%s", nm); XFree(nm); } }
	for (char *p = c->title; *p; p++) if (*p == '\n' || *p == '\t') *p = ' ';
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
	e.xconfigure.x = c->full ? 0 : c->x + BW; e.xconfigure.y = c->full ? 0 : c->y + TH; e.xconfigure.width = c->cw; e.xconfigure.height = c->ch;
	XSendEvent(DPY, c->w, False, StructureNotifyMask, &e);
}
static void frame_shape(Client *c);
/* (1.6.0 Part 12) the page's resize handles (base_v24.html addResize: .rsz, a few pixels inside each edge, larger at the
   corners): see-through input windows over the app's edges (left, right, bottom; the top edge is the title bar's) */
static int RZG(void) { return (int)lround(4 * Z); }
static void rz_place(Client *c)
{
	int fw = c->cw + 2 * BW, fh = c->ch + TH + BW, g = RZG();
	for (int i = 0; i < 3; i++) {
		if (!c->rz[i]) { XSetWindowAttributes at = {0}; at.event_mask = ButtonPressMask | ButtonReleaseMask | PointerMotionMask; c->rz[i] = XCreateWindow(DPY, c->frame, 0, 0, 1, 1, 0, 0, InputOnly, CopyFromParent, CWEventMask, &at); }
		if (c->max || c->full) { XUnmapWindow(DPY, c->rz[i]); continue; }
		if (i == 0) XMoveResizeWindow(DPY, c->rz[i], 0, TH, g, fh - TH); else if (i == 1) XMoveResizeWindow(DPY, c->rz[i], fw - g, TH, g, fh - TH); else XMoveResizeWindow(DPY, c->rz[i], 0, fh - g, fw, g);
		XMapRaised(DPY, c->rz[i]);
	}
}
static void rz_to_frame(Client *c, int i, int *x, int *y) { int fw = c->cw + 2 * BW, fh = c->ch + TH + BW, g = RZG(); if (i == 0) *y += TH; else if (i == 1) { *x += fw - g; *y += TH; } else *y += fh - g; }
static struct { Client *c; int edge, px, py, x, y, w, h; } DRAG;   /* edge: 0 move, else 1 left 2 right 4 top 8 bottom */
static void place(Client *c)
{
	if (c->full) { XMoveResizeWindow(DPY, c->frame, -BW, -TH, SW + 2 * BW, SH + TH + BW); XMoveResizeWindow(DPY, c->w, BW, TH, SW, SH); c->cw = SW; c->ch = SH; }
	else { XMoveResizeWindow(DPY, c->frame, c->x, c->y, fw_of(c), fh_of(c)); XMoveResizeWindow(DPY, c->w, BW, TH, c->cw, c->ch); }
	/* (our own app window: GTK resizes itself to the size it was last asked for (its requests are not redirected: the shell's
	   connection is GTK's), so it is asked for the window's size) */
	if (c->app && c->app->win && !c->full) { int gw = 0, gh = 0; gtk_window_get_size(GTK_WINDOW(c->app->win), &gw, &gh); if (gw != c->cw || gh != c->ch) gtk_window_resize(GTK_WINDOW(c->app->win), c->cw, c->ch); }
	frame_shape(c); rz_place(c); tell_geom(c); if (!DRAG.c) desk_shadows();
}
static void frame_draw(Client *c);
static void btn_rects(Client *c, int r[3][4]);
static void frames_redraw(void) { for (GList *l = CLIENTS; l; l = l->next) frame_draw(l->data); }
/* our own windows above the frames: the taskbar (unless a window is full screen), the start menu, the sign-in screen */
static void wm_restack(void)
{
	if (!WM_OK) return;
	if (DESK && gtk_widget_get_window(DESK)) XLowerWindow(DPY, GDK_WINDOW_XID(gtk_widget_get_window(DESK)));
	if (!(FOCUS && FOCUS->full) && BAR && gtk_widget_get_visible(BAR)) XRaiseWindow(DPY, GDK_WINDOW_XID(gtk_widget_get_window(BAR)));
	if (MENU && gtk_widget_get_visible(MENU)) XRaiseWindow(DPY, GDK_WINDOW_XID(gtk_widget_get_window(MENU)));
	if (SIGN && gtk_widget_get_visible(SIGN)) XRaiseWindow(DPY, GDK_WINDOW_XID(gtk_widget_get_window(SIGN)));
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
static void wm_focus(Client *c)
{
	Client *old = FOCUS; FOCUS = c;
	if (c) {
		if (c->min) { c->min = 0; XMapWindow(DPY, c->frame); set_wm_state(c, NormalState); }
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
	wm_restack(); client_list(); bar_redraw(); state_write();
}
static Client *top_client(void)
{
	/* the topmost window still shown (stacking order from the server) */
	Window r, p, *k = NULL; unsigned n = 0; Client *best = NULL;
	if (XQueryTree(DPY, ROOT, &r, &p, &k, &n)) for (unsigned i = 0; i < n; i++) { Client *c = client_of(k[i]); if (c && !c->min && !c->hidden && c->frame == k[i]) best = c; }
	if (k) XFree(k); return best;
}
static void wm_minimise(Client *c)
{
	desk_shadows();
	if (c->min) return; c->min = 1; XUnmapWindow(DPY, c->frame); set_wm_state(c, IconicState);
	if (FOCUS == c) { FOCUS = NULL; wm_focus(top_client()); }
	bar_redraw(); state_write();
}
static void wm_maximise(Client *c, int on)
{
	if (c->full || on == c->max) return;
	if (on) { c->sx = c->x; c->sy = c->y; c->scw = c->cw; c->sch = c->ch; c->x = 0; c->y = work_top(); c->cw = SW - 2 * BW; c->ch = work_h() - TH - BW; }
	else { c->x = c->sx; c->y = c->sy; c->cw = c->scw; c->ch = c->sch; }
	c->max = on; place(c); frame_draw(c); state_write();
}
static void wm_fullscreen(Client *c, int on)
{
	if (on == c->full) return;
	if (on) { if (!c->max) { c->sx = c->x; c->sy = c->y; c->scw = c->cw; c->sch = c->ch; } c->full = 1; }
	else { c->full = 0; if (c->max) { c->max = 0; wm_maximise(c, 1); } else { c->x = c->sx; c->y = c->sy; c->cw = c->scw; c->ch = c->sch; } }
	Atom a[1] = {A_NFULL}; XChangeProperty(DPY, c->w, A_NSTATE, XA_ATOM, 32, PropModeReplace, (unsigned char *)a, on ? 1 : 0);
	place(c); frame_draw(c); wm_focus(c);
}
static void wm_close(Client *c)
{
	if (c->app) { appwin_ask_close(c->app); return; }   /* (our own: the page is asked, as its ✕ does) */
	if (has_proto(c->w, A_DEL)) send_proto(c->w, A_DEL, CurrentTime); else XKillClient(DPY, c->w);
}
static void wm_manage(Window w, AppWin *own)
{
	if (client_of(w)) return;
	XWindowAttributes wa; if (!XGetWindowAttributes(DPY, w, &wa) || wa.override_redirect) return;
	Client *c = g_new0(Client, 1); c->w = w; c->app = own; c->cw = wa.width; c->ch = wa.height;
	if (!XGetTransientForHint(DPY, w, &c->trans)) c->trans = None;
	wm_title(c);
	int ay = work_top(), ah = work_h();
	if (c->cw > SW - 2 * BW) c->cw = SW - 2 * BW; if (c->ch > ah - TH - BW) c->ch = ah - TH - BW;
	Client *owner = c->trans ? client_of(c->trans) : NULL;
	if (owner) { c->x = owner->x + (fw_of(owner) - fw_of(c)) / 2; c->y = owner->y + (fh_of(owner) - fh_of(c)) / 2; }
	else if (own || (wa.x <= 0 && wa.y <= 0)) { int n = g_list_length(CLIENTS) % 8; c->x = 40 + n * 28; c->y = ay + 16 + n * 28; }
	else { c->x = wa.x - BW; c->y = wa.y - TH; }
	if (c->x + fw_of(c) > SW) c->x = SW - fw_of(c); if (c->x < 0) c->x = 0;
	if (c->y + fh_of(c) > ay + ah) c->y = ay + ah - fh_of(c); if (c->y < ay) c->y = ay;
	XSetWindowAttributes at = {0}; at.event_mask = SubstructureRedirectMask | SubstructureNotifyMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | ExposureMask | LeaveWindowMask;
	at.background_pixel = BlackPixel(DPY, DefaultScreen(DPY));
	c->frame = XCreateWindow(DPY, ROOT, c->x, c->y, fw_of(c), fh_of(c), 0, CopyFromParent, InputOutput, CopyFromParent, CWEventMask | CWBackPixel, &at);
	/* (our own windows share GDK's connection to X: selecting events replaces GDK's own mask, so GDK's stays (its sizes, its
	   exposures; else a page keeps its first size, larger than its window, and is not drawn again where a window uncovers it)) */
	XSelectInput(DPY, w, PropertyChangeMask | (own ? wa.your_event_mask : StructureNotifyMask));
	XAddToSaveSet(DPY, w); XSetWindowBorderWidth(DPY, w, 0);
	if (wa.map_state != IsUnmapped) c->ignore_unmap++;   /* (reparenting a shown window unmaps it once) */
	XReparentWindow(DPY, w, c->frame, BW, TH); XResizeWindow(DPY, w, c->cw, c->ch);
	CLIENTS = g_list_append(CLIENTS, c);
	XMapWindow(DPY, w); frame_shape(c); rz_place(c); XMapWindow(DPY, c->frame); set_wm_state(c, NormalState); desk_shadows();
	grab_clicks(c, 1); tell_geom(c);
	/* asked for full screen before it was shown */
	Atom type; int fmt; unsigned long n = 0, after; unsigned char *d = NULL;
	if (XGetWindowProperty(DPY, w, A_NSTATE, 0, 32, False, XA_ATOM, &type, &fmt, &n, &after, &d) == Success && d) { for (unsigned long i = 0; i < n; i++) if (((Atom *)d)[i] == A_NFULL) { c->full = 0; wm_fullscreen(c, 1); } XFree(d); }
	wm_focus(c);
}
static void wm_unmanage(Client *c, int gone)
{
	desk_shadows();
	CLIENTS = g_list_remove(CLIENTS, c);
	if (!gone) { XReparentWindow(DPY, c->w, ROOT, c->x + BW, c->y + TH); XRemoveFromSaveSet(DPY, c->w); set_wm_state(c, WithdrawnState); grab_clicks(c, 0); }
	XDestroyWindow(DPY, c->frame);
	if (FOCUS == c) { FOCUS = NULL; wm_focus(top_client()); }
	g_free(c); client_list(); bar_redraw(); state_write();
}
/* the lock screen: every window out of sight, then back */
static void wm_hide_all(int hide)
{
	desk_shadows();
	for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (c->min) continue;
		if (hide && !c->hidden) { c->hidden = 1; XUnmapWindow(DPY, c->frame); } else if (!hide && c->hidden) { c->hidden = 0; XMapWindow(DPY, c->frame); } }
	if (!hide) wm_focus(FOCUS ? FOCUS : top_client());
}
/* the title bars' height or the work area changed (the display scaling, the development link's banner) */
static void wm_relayout(void)
{
	for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data;
		if (c->max) { c->x = 0; c->y = work_top(); c->cw = SW - 2 * BW; c->ch = work_h() - TH - BW; }
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
static int btn_at(Client *c, int x, int y) { int r[3][4]; btn_rects(c, r); for (int i = 0; i < 3; i++) if (x >= r[i][0] && x < r[i][0] + r[i][2] && y >= r[i][1] && y < r[i][1] + r[i][3]) return i + 1; return 0; }
static Cursor cursor_for(int e) { int i = e == 1 ? 1 : e == 2 ? 2 : e == 4 ? 3 : e == 8 ? 4 : e == 5 ? 5 : e == 6 ? 6 : e == 9 ? 7 : e == 10 ? 8 : 0; return CUR[i]; }
static void frame_press(Client *c, XButtonEvent *e)
{
	if (c != FOCUS) wm_focus(c);
	if (e->button != 1) return;
	int b = btn_at(c, e->x, e->y);
	if (b) { c->press = b; frame_draw(c); return; }
	if (c->full) return;
	int ed = c->max ? 0 : edge_at(c, e->x, e->y);
	if (!ed && e->y < TH) { if (e->time - c->last_click < 400) { c->last_click = 0; wm_maximise(c, !c->max); return; } c->last_click = e->time; if (c->max) return; }
	else if (!ed) return;
	DRAG.c = c; DRAG.edge = ed; DRAG.px = e->x_root; DRAG.py = e->y_root; DRAG.x = c->x; DRAG.y = c->y; DRAG.w = c->cw; DRAG.h = c->ch;
	XGrabPointer(DPY, c->frame, False, ButtonReleaseMask | PointerMotionMask, GrabModeAsync, GrabModeAsync, None, cursor_for(ed), e->time);
}
static void drag_motion(int rx, int ry)
{
	Client *c = DRAG.c; int dx = rx - DRAG.px, dy = ry - DRAG.py;
	if (!DRAG.edge) { c->x = DRAG.x + dx; c->y = DRAG.y + dy; if (c->y < work_top()) c->y = work_top(); XMoveWindow(DPY, c->frame, c->x, c->y); return; }
	int x = DRAG.x, y = DRAG.y, w = DRAG.w, h = DRAG.h;
	if (DRAG.edge & 1) { w = DRAG.w - dx; if (w < 160) w = 160; x = DRAG.x + DRAG.w - w; }
	if (DRAG.edge & 2) { w = DRAG.w + dx; if (w < 160) w = 160; }
	if (DRAG.edge & 4) { h = DRAG.h - dy; if (h < 80) h = 80; y = DRAG.y + DRAG.h - h; }
	if (DRAG.edge & 8) { h = DRAG.h + dy; if (h < 80) h = 80; }
	c->x = x; c->y = y; c->cw = w; c->ch = h; place(c); frame_draw(c);
}
static void frame_release(Client *c, XButtonEvent *e)
{
	if (DRAG.c) { XUngrabPointer(DPY, e->time); if (DRAG.c == c) tell_geom(c); DRAG.c = NULL; desk_shadows(); state_write(); return; }
	int b = c->press; c->press = 0; frame_draw(c);
	if (b && b == btn_at(c, e->x, e->y)) { if (b == 1) wm_minimise(c); else if (b == 2) wm_maximise(c, !c->max); else wm_close(c); }
}
static void power(const char *what);
static GdkFilterReturn wm_filter(GdkXEvent *xev, GdkEvent *ev, gpointer d)
{
	(void)ev; (void)d; XEvent *e = xev; Client *c;
	switch (e->type) {
	case KeyPress:   /* (the page's global shortcuts the shell has: Meta+L, lock the screen: shell25.js) */
		if (e->xkey.window == ROOT && e->xkey.keycode == XKeysymToKeycode(DPY, XK_l) && (e->xkey.state & Mod4Mask)) { if (USER_ID[0] && !LOCKED) power("lock"); return GDK_FILTER_REMOVE; }
		break;
	case MapRequest:
		if (!(c = client_of(e->xmaprequest.window))) wm_manage(e->xmaprequest.window, NULL); else { XMapWindow(DPY, c->w); wm_focus(c); }
		return GDK_FILTER_REMOVE;
	case ConfigureRequest: {
		XConfigureRequestEvent *r = &e->xconfigurerequest; c = client_of(r->window);
		if (!c) { XWindowChanges wc = { r->x, r->y, r->width, r->height, r->border_width, r->above, r->detail }; XConfigureWindow(DPY, r->window, r->value_mask, &wc); return GDK_FILTER_REMOVE; }
		if (c->w == r->window && !c->full && !c->max) {
			if (r->value_mask & CWWidth) c->cw = r->width; if (r->value_mask & CWHeight) c->ch = r->height;
			if (r->value_mask & CWX) c->x = r->x - BW; if (r->value_mask & CWY) c->y = r->y - TH;
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
		if ((c = client_of(e->xunmap.window)) && c->w == e->xunmap.window && (e->xunmap.event == c->frame || e->xunmap.send_event)) {   /* (not GDK's own copy: our windows') */
			if (c->ignore_unmap > 0) { c->ignore_unmap--; return GDK_FILTER_CONTINUE; }
			if (e->xunmap.event == c->frame || e->xunmap.send_event) wm_unmanage(c, 0);
		}
		return GDK_FILTER_CONTINUE;
	case DestroyNotify:
		if ((c = client_of(e->xdestroywindow.window)) && c->w == e->xdestroywindow.window) wm_unmanage(c, 1);
		return GDK_FILTER_CONTINUE;
	case PropertyNotify:
		if ((c = client_of(e->xproperty.window)) && c->w == e->xproperty.window && (e->xproperty.atom == XA_WM_NAME || e->xproperty.atom == A_NNAME)) { wm_title(c); frame_draw(c); bar_redraw(); state_write(); }
		return GDK_FILTER_CONTINUE;
	case Expose:
		if ((c = client_of(e->xexpose.window)) && c->frame == e->xexpose.window) { if (!e->xexpose.count) frame_draw(c); return GDK_FILTER_REMOVE; }
		return GDK_FILTER_CONTINUE;
	case ButtonPress:
		if ((c = client_of(e->xbutton.window))) {
			int ri = rz_of(c, e->xbutton.window); if (ri >= 0) { XButtonEvent b = e->xbutton; rz_to_frame(c, ri, &b.x, &b.y); frame_press(c, &b); return GDK_FILTER_REMOVE; }
			if (c->frame == e->xbutton.window) frame_press(c, &e->xbutton);
			else { wm_focus(c); XAllowEvents(DPY, ReplayPointer, e->xbutton.time); }   /* a click into a window: it gets the focus, and the click */
			return GDK_FILTER_REMOVE;
		}
		return GDK_FILTER_CONTINUE;
	case ButtonRelease:
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
		XClientMessageEvent *m = &e->xclient; c = client_of(m->window);
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
	/* what we do (not _NET_WM_MOVERESIZE: GTK then moves its own windows itself) */
	Atom sup[] = {A_NSTATE, A_NFULL, A_NHIDDEN, A_NACTIVE, A_NLIST, A_NNAME, A_NCHECK};
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
	{ static const unsigned M[4] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask }; for (int i = 0; i < 4; i++) XGrabKey(DPY, XKeysymToKeycode(DPY, XK_l), Mod4Mask | M[i], ROOT, True, GrabModeAsync, GrabModeAsync); }
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
static void appwin_size(AppWin *a, int w, int h)
{
	Client *c = WM_OK ? client_of_app(a) : NULL; if (!c || c->max || c->full || w < 80 || h < 60) return;
	int n = 0; for (GList *l = CLIENTS; l; l = l->next) { Client *o = l->data; if (o != c && !o->trans) n++; }
	double dw = SW / Z, avail = work_h() / Z; w = (int)fmin(w, dw - 8); h = (int)fmin(h, avail - 8);
	double x = fmax(0, fmin(40 + n * 26, dw - w - 4)), y = fmax(0, fmin(20 + n * 26, avail - h - 4));
	c->cw = (int)lround(w * Z); c->ch = (int)lround((h - 34) * Z); c->x = (int)lround(x * Z); c->y = work_top() + (int)lround(y * Z);
	place(c); frame_draw(c); state_write();
}
static void frame_redraw_app(AppWin *a) { Client *c = WM_OK ? client_of_app(a) : NULL; if (c) { wm_title(c); frame_draw(c); } }

static void appwin_activate(AppWin *a)
{
	Client *c = WM_OK ? client_of_app(a) : NULL;
	if (c) { if (c == FOCUS && !c->min) wm_minimise(c); else wm_focus(c); return; }
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
static void shd_add(Box *b, double dx, double dy, double blur, RGBA c, int inset) { if (b->nsh < 4) { Shd s = { dx, dy, blur, c, inset }; b->sh[b->nsh++] = s; } }
/* (Chromium snaps a box's edges to whole pixels) */
static void snap(double *x, double *y, double *w, double *h) { double x1 = round(*x + *w), y1 = round(*y + *h); *x = round(*x); *y = round(*y); *w = x1 - *x; *h = y1 - *y; }
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
	srca(cr, c); cairo_move_to(cr, x, round(base) - b); pango_cairo_show_layout(cr, pl); g_object_unref(pl); return lr.width / (double)PANGO_SCALE;   /* (the baseline on a whole pixel, as Chromium's) */
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
	if (NET_WIFI) {
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
static void bar_paint(cairo_t *cr, double W, double H, int top)
{
	(void)H; NHITS = 0; double mid = bar_mid(); int hv = HOVER, pr = PRESS;
	Box bar = tb_box(TB_BAR, 0, 0);
	/* (Wireframe's see-through bar shows the wallpaper behind, blurred: the window's own copy of it) */
	if (bar.bg.a < 1 && WALL) { cairo_save(cr); cairo_scale(cr, 1 / Z, 1 / Z); cairo_set_source_surface(cr, WALL, 0, top ? -TOPRES : -(SH - BARH)); cairo_paint(cr); cairo_restore(cr); }
	Box bar2 = bar; bar2.nsh = 0; for (int i = 0; i < bar.nsh; i++) if (bar.sh[i].inset) bar2.sh[bar2.nsh++] = bar.sh[i];
	box_paint(cr, 0, 0, W, 46, &bar2);
	if (!strcmp(tb_vars().st, "wire") && tb_vars().on && top) { srca(cr, cmix(tb_vars().fg, .7, CLEAR)); cairo_rectangle(cr, 0, 45, W, 1); cairo_fill(cr); }
	/* which part is which: the hovered one by its index in HITS, as drawn last time (the same order each time) */
	(void)hv; (void)pr;
	#define HV(k, a) (!strcmp(HOV_K, k) && !strcmp(HOV_A, a))
	#define PR(k, a) (!strcmp(PRS_K, k) && !strcmp(PRS_A, a))
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
		double tx = rx + b.bw + 10, ty = y + b.bw + 3;
		css_text(cr, CLOCK_T, UIFAM, 400, 13, tx, ty, 26, b.fg, 0);
		/* (the date: .85em, on the same baseline) */
		double d1, a1 = css_ascent(cr, UIFAM, 400, 13, &d1), base = ty + floor((26 - (a1 + d1)) / 2) + a1;
		double a2, d2; a2 = css_ascent(cr, UIFAM, 400, 13 * .85, &d2);
		css_text(cr, CLOCK_D, UIFAM, 400, 13 * .85, tx + t1 + 8, base - a2 - (0), a2 + d2, (RGBA){b.fg.r, b.fg.g, b.fg.b, .78}, 0);
		rx -= 4; ri++; }
	static const char *const QK[2] = {"notes", "qs"}; static const char *const QT[2] = {"🔔", "☰"};
	for (int q = 0; q < 2; q++, ri++) {
		Box b = tb_box(TB_QSB, HV(QK[q], ""), PR(QK[q], "")); double tw = css_width(cr, QT[q], UIFAM, 400, 15), w = fmax(30, 12 + tw + 2 * b.bw), y = mid - 15;
		rx -= w; box_paint(cr, rx, y, w, 30, &b); hit_add(rx, y, w, 30, QK[q], NULL);
		css_text(cr, QT[q], UIFAM, 400, 15, rx + (w - tw) / 2, y + b.bw, 30 - 2 * b.bw, b.fg, 0);
		if (q == 0 && S.notes > 0) {   /* (#ncb's count: a red badge at its top right) */
			char n[16]; snprintf(n, sizeof n, "%d", S.notes); double nw = css_width(cr, n, UIFAM, 400, 9) + 8; Box bd; memset(&bd, 0, sizeof bd); bd.bg = cx("#ef4444", 1); bd.rad = 7;
			box_paint(cr, rx + w - nw, y + 1, nw, 13, &bd); css_text(cr, n, UIFAM, 400, 9, rx + w - nw + 4, y + 1, 13, cx("#ffffff", 1), 0); }
		rx -= 4;
	}
	{ Box b = tb_box(TB_NET, HV("vol", ""), PR("vol", "")); double w = 36 + 2 * b.bw, y = mid - 13; rx -= w; box_paint(cr, rx, y, w, 26, &b); hit_add(rx, y, w, 26, "vol", NULL);
		ic_vol(cr, rx + b.bw + 7, y + 2, b.filt); rx -= 4; ri++; }
	{ Box b = tb_box(TB_NET, HV("net", ""), PR("net", "")); double w = 34 + 2 * b.bw, y = mid - 13; rx -= w; box_paint(cr, rx, y, w, 26, &b); hit_add(rx, y, w, 26, "net", NULL);
		ic_net(cr, rx + b.bw + 6, y + 2, b.filt); rx -= 4; ri++; }
	/* the workspaces (as the page's: the first is the one shown; the native shell has one until item 4) */
	rx -= 4 + 4 * 22 + 3 * 2;
	{ static const char *const WT[4] = {"1", "2", "3", "▦"}; double wx = rx;
		for (int k = 0; k < 4; k++, ri++) { Box b = tb_box(k == 0 ? TB_WSON : TB_WS, HV("ws", WT[k]), PR("ws", WT[k])); double y = mid - 11;
			if (k == 0) b.fg = tb_vars().on && !strcmp(tb_vars().st, "mono") ? b.fg : b.fg; else if (WINS || CLIENTS) { /* (.has: windows there) */ }
			box_paint(cr, wx, y, 22, 22, &b); hit_add(wx, y, 22, 22, "ws", WT[k]);
			double tw = css_width(cr, WT[k], UIFAM, 400, 11); css_text(cr, WT[k], UIFAM, 400, 11, wx + (22 - tw) / 2, y + b.bw, 22 - 2 * b.bw, b.fg, 0); wx += 24; } }
	rx -= 4 + 4;
	if (BAT >= 0) { Box b = tb_box(TB_BAT, HV("bat", ""), PR("bat", "")); char t[16]; snprintf(t, sizeof t, "%d%%", BAT); double tw = css_width(cr, t, SYSUI, 600, 12), w = 2 * b.bw + 6 + 26 + 3 + tw + 6, y = mid - 13;
		rx -= w; box_paint(cr, rx, y, w, 26, &b); hit_add(rx, y, w, 26, "bat", NULL);
		ic_bat(cr, rx + b.bw + 6, mid - 7, b.filt); css_text(cr, t, SYSUI, 600, 12, rx + b.bw + 6 + 26 + 3, y + b.bw, 26 - 2 * b.bw, b.fg, 0); rx -= 4; ri++; }
	/* the open windows (#tasks: buttons of at most 150 px, 3 px apart, their picture at 18 px and the title) */
	GList *TL = NULL; if (WM_OK) { for (GList *l = CLIENTS; l; l = l->next) if (!((Client *)l->data)->trans) TL = g_list_append(TL, l->data); } else TL = g_list_copy(WINS);
	int n = g_list_length(TL), j = 0; double avail = rx - tasks_x, tw = n ? fmin(150, (avail - 3 * (n - 1)) / n) : 0; i = 200;
	for (GList *l = TL; l && tw >= 24; l = l->next, j++, i++) {
		Client *c = WM_OK ? l->data : NULL; AppWin *a = c ? c->app : l->data; int act = c ? c == FOCUS && !c->min : a == ACTIVE;
		char wid[24]; snprintf(wid, sizeof wid, "%lx", c ? (unsigned long)c->w : (unsigned long)(uintptr_t)a);
		double wx = tasks_x + j * (tw + 3), y = mid - 17; Box b = tb_box(act ? TB_TASKON : TB_TASK, HV("win", wid), PR("win", wid));
		box_paint(cr, wx, y, tw, 34, &b); hit_add(wx, y, tw, 34, "win", wid);
		/* the line: the picture (vertical-align -4px) and the title, centred in the button's content (34 less 1 px above and below) */
		double d, A = css_ascent(cr, UIFAM, act ? 600 : 400, 13, &d), lt = fmin(-A, -14), lb = fmax(d, 4), base = y + b.bw + 1 + (32 - 2 * b.bw - (lb - lt)) / 2 - lt;
		const App *ap = a ? app_find(a->k) : NULL; GdkPixbuf *px = app_pix(ap); double tx = wx + b.bw + 6;
		cairo_save(cr); cairo_rectangle(cr, wx + b.bw + 6, y, tw - 2 * b.bw - 12, 34); cairo_clip(cr);
		if (px) { pix_at(cr, px, tx, base + 4 - 18, 18, b.filt); tx += 18; }
		const char *ti = a ? a->title : c->title; char *tt = px ? g_strdup_printf(" %s", ti) : g_strdup_printf("%s%s%s", a && a->icon[0] ? a->icon : "", a && a->icon[0] ? " " : "", ti);
		css_text(cr, tt, UIFAM, act ? 600 : 400, 13, tx, base - A, A + d, b.fg, wx + tw - b.bw - 6 - tx); g_free(tt);
		cairo_restore(cr);
	}
	g_list_free(TL);
	#undef HV
	#undef PR
}
static gboolean bar_draw(GtkWidget *wd, cairo_t *cr, gpointer d)
{
	(void)d; int top = !strcmp(S.tb, "top");
	double W = gtk_widget_get_allocated_width(wd) / Z, H = gtk_widget_get_allocated_height(wd) / Z;
	cairo_scale(cr, Z, Z); bar_paint(cr, W, H, top); state_write();
	return TRUE;
}
/* the bar's shadow and Wireframe's top edge, which fall outside its window: drawn on the desktop (desk_area_draw) */
static void bar_outside(cairo_t *cr)
{
	if (!USER_ID[0] || LOCKED || !strcmp(S.tb, "top")) return;
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

static void bar_redraw(void) { if (BARAREA) gtk_widget_queue_draw(BARAREA); }
static int hit_at(double x, double y) { for (int i = 0; i < NHITS; i++) if (x >= HITS[i].x && x < HITS[i].x + HITS[i].w && y >= HITS[i].y && y < HITS[i].y + HITS[i].h) return i; return -1; }
static void menu_toggle(void);
static void bar_click(int h)
{
	if (h < 0) return; Hit *t = &HITS[h];
	if (!strcmp(t->kind, "start")) menu_toggle();
	else if (!strcmp(t->kind, "pin")) {
		for (GList *l = WINS; l; l = l->next) { AppWin *a = l->data; if (!strcmp(a->k, t->arg)) { appwin_activate(a); return; } }   /* already open: to the front */
		app_open(t->arg, NULL);
	}
	else if (!strcmp(t->kind, "win")) {
		unsigned long id = strtoul(t->arg, NULL, 16);
		if (WM_OK) { Client *c = client_of((Window)id); if (c) { if (c == FOCUS && !c->min) wm_minimise(c); else wm_focus(c); } return; }
		for (GList *l = WINS; l; l = l->next) if ((unsigned long)(uintptr_t)l->data == id) { appwin_activate(l->data); return; }
	}
	else if (!strcmp(t->kind, "clock")) app_open("clock", NULL);
	else if (!strcmp(t->kind, "ws")) return;   /* (the workspaces: plan 1.6.0 Part 12, item 4) */
	else app_open("settings", NULL);
}
static void hk_set(int h, char *k, char *a) { if (h >= 0) { snprintf(k, 12, "%s", HITS[h].kind); snprintf(a, 80, "%s", HITS[h].arg); } else k[0] = a[0] = 0; }
static gboolean bar_press(GtkWidget *w, GdkEventButton *e, gpointer d) { (void)w; (void)d; if (e->type == GDK_BUTTON_PRESS && e->button == 1) { PRESS = hit_at(e->x, e->y); hk_set(PRESS, PRS_K, PRS_A); bar_redraw(); } return TRUE; }
static gboolean bar_release(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)w; (void)d; if (e->button != 1) return TRUE; int h = hit_at(e->x, e->y); int was = PRESS; PRESS = -1; PRS_K[0] = PRS_A[0] = 0; bar_redraw();
	if (h >= 0 && h == was) bar_click(h);
	return TRUE;
}
static gboolean bar_motion(GtkWidget *w, GdkEventMotion *e, gpointer d) { (void)w; (void)d; int h = hit_at(e->x, e->y); if (h != HOVER) { HOVER = h; hk_set(h, HOV_K, HOV_A); bar_redraw(); } return FALSE; }
static gboolean bar_leave(GtkWidget *w, GdkEventCrossing *e, gpointer d) { (void)w; (void)e; (void)d; if (HOVER != -1) { HOVER = -1; HOV_K[0] = HOV_A[0] = 0; bar_redraw(); } return FALSE; }
static void bar_place(void)
{
	if (!BAR) return; int top = !strcmp(S.tb, "top");
	gtk_widget_set_size_request(BAR, SW, BARH); gtk_window_move(GTK_WINDOW(BAR), 0, top ? TOPRES : SH - BARH); gtk_window_resize(GTK_WINDOW(BAR), SW, BARH);
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
static void btn_rects(Client *c, int r[3][4])
{
	int candy = !strcmp(tw_vars().st, "candy"); double fw = fw_of(c) / Z, x = fw - 1 - 8 - 24 - (candy ? 1 : 0);
	for (int i = 2; i >= 0; i--) { r[i][0] = (int)lround(x * Z); r[i][1] = (int)lround(7 * Z); r[i][2] = (int)lround(24 * Z); r[i][3] = (int)lround(22 * Z); x -= 27 + (candy ? 2 : 0); }
}
/* the frame's outline: round corners (X's shape: no compositor needed) */
static void frame_shape(Client *c)
{
	GdkWindow *g = gdk_x11_window_foreign_new_for_display(gdk_display_get_default(), c->frame); if (!g) return;
	int fw = fw_of(c), fh = fh_of(c); double r = c->max || c->full ? 0 : tw_rad() * Z;
	cairo_region_t *rg = cairo_region_create();
	for (int y = 0; y < fh; y++) { double dy = y < r ? r - y - .5 : y >= fh - r ? y - (fh - r) + .5 : 0, in = dy > 0 ? r - sqrt(fmax(0, r * r - dy * dy)) : 0;
		int x0 = (int)floor(in + .5); cairo_rectangle_int_t rc = { x0, y, fw - 2 * x0, 1 }; cairo_region_union_rectangle(rg, &rc); }
	gdk_window_shape_combine_region(g, rg, 0, 0); cairo_region_destroy(rg); g_object_unref(g);
}
static void frame_draw(Client *c)
{
	if (!c->frame || c->full) return;
	int fw = fw_of(c), fh = fh_of(c), act = c == FOCUS; TwVars V = tw_vars(); const char *st = V.st;
	cairo_surface_t *sf = cairo_xlib_surface_create(DPY, c->frame, DefaultVisual(DPY, DefaultScreen(DPY)), fw, fh); cairo_t *cr = cairo_create(sf);
	cairo_scale(cr, Z, Z); double W = fw / Z, H = fh / Z, R = c->max ? 0 : tw_rad();
	/* the window: its edge and its white */
	srca(cr, cx("#ffffff", 1)); cairo_paint(cr);
	Box win; memset(&win, 0, sizeof win); win.bg = cx("#ffffff", 1); win.bw = 1; win.bc = cx("#9ca3af", 1); win.rad = R; box_paint(cr, 0, 0, W, H, &win);
	/* the title bar (inside the edge: 34 px, its top corners 7 px) */
	Box tb; memset(&tb, 0, sizeof tb); RGBA fg; double tbo = 1;   /* (tbo: the bar's opacity: .72 for the other windows in Wireframe, Candy, Retro) */
	if (!V.on) { tb.bg = act ? ca(accent(), 1) : cx("#e5e7eb", 1); fg = act ? cx("#ffffff", 1) : cx("#6b7280", 1); }
	else if (!strcmp(st, "wire")) { tb.bg = cmix(V.bg, .14, CLEAR); fg = cx("#000000", 1); shd_add(&tb, 0, -2, 0, V.bg, 1); if (act) shd_add(&tb, 0, 0, 0, V.bg, 2); else tbo = .72; }
	else if (!strcmp(st, "mono")) { tb.bg = act ? V.bg : cmix(V.bg, .35, cx("#9ca3af", 1)); fg = act ? V.fg : cx("#ffffff", 1); }
	else if (!strcmp(st, "candy")) { if (act) { tb.ng = 3; tb.gc[0] = V.hi; tb.gc[1] = V.bg; tb.gc[2] = V.lo; tb.gp[0] = 0; tb.gp[1] = .48; tb.gp[2] = 1; shd_add(&tb, 0, 1, 0, cx("#ffffff", .5), 1); fg = V.fg; }
		else { tb.bg = cx("#e5e7eb", 1); fg = cx("#6b7280", 1); tbo = .72; } }
	else if (!strcmp(st, "retro")) { if (act) { tb.bg = V.bg; fg = V.fg; } else { tb.bg = cx("#e5e7eb", 1); fg = cx("#6b7280", 1); tbo = .72; } }
	else { tb.bg = act ? V.bg : cx("#e5e7eb", 1); fg = act ? V.fg : cx("#6b7280", 1); }
	int grey = V.on && strcmp(st, "mono") && !act;   /* (twcol: the other windows' bars in grey) */
	cairo_push_group(cr);
	double tr = fmax(0, R - 1);
	cairo_save(cr); rrect(cr, 1, 1, W - 2, 34, 0); cairo_clip(cr);
	if (tr > 0) { cairo_new_path(cr); cairo_move_to(cr, 1, 35); cairo_line_to(cr, 1, 1 + tr); cairo_arc(cr, 1 + tr, 1 + tr, tr, M_PI, 1.5 * M_PI); cairo_arc(cr, W - 1 - tr, 1 + tr, tr, 1.5 * M_PI, 2 * M_PI); cairo_line_to(cr, W - 1, 35); cairo_close_path(cr); cairo_clip(cr); }
	if (tb.nsh && tb.sh[tb.nsh - 1].inset == 2) {   /* (Wireframe's active bar: a 1 px line of its colour all round as well) */
		tb.nsh--; box_paint(cr, 1, 1, W - 2, 34, &tb); srca(cr, V.bg); cairo_rectangle(cr, 1, 1, W - 2, 34); cairo_rectangle(cr, 2, 2, W - 4, 32); cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD); cairo_fill(cr); cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING); }
	else if (!strcmp(st, "retro") && act && V.on) { cairo_pattern_t *g = cairo_pattern_create_linear(1, 0, W - 1, 0);
		cairo_pattern_add_color_stop_rgb(g, 0, V.lo.r, V.lo.g, V.lo.b); cairo_pattern_add_color_stop_rgb(g, .45, V.bg.r, V.bg.g, V.bg.b); cairo_pattern_add_color_stop_rgb(g, 1, V.hi.r, V.hi.g, V.hi.b);
		cairo_rectangle(cr, 1, 1, W - 2, 34); cairo_set_source(cr, g); cairo_fill(cr); cairo_pattern_destroy(g); }
	else box_paint(cr, 1, 1, W - 2, 34, &tb);
	cairo_restore(cr);
	/* the picture, the title, the buttons */
	int wt = !strcmp(st, "retro") && V.on ? 700 : 600; AppWin *a = c->app; const App *ap = a ? app_find(a->k) : NULL; GdkPixbuf *px = app_pix(ap);
	int filt = V.on && !strcmp(st, "mono") ? 1 : 0;
	if (px) pix_at(cr, px, 9, 10, 16, filt); else if (a && a->icon[0]) css_text(cr, a->icon, UIFAM, wt, 13, 9, 10.5, 15, fg, 0);
	int r[3][4]; btn_rects(c, r); double tx = 1 + 8 + 16 + 3, tmax = r[0][0] / Z - 3 - tx;
	css_text(cr, a ? a->title : c->title, UIFAM, wt, 13, tx, 10.5, 15, fg, tmax);
	static const char *const G[3] = {"_", "□", "✕"};
	for (int i = 0; i < 3; i++) { double bx = r[i][0] / Z, by = 7, bw = 24, bh = 22; int hv = c->hover == i + 1, pr = c->press == i + 1, cl = i == 2;
		Box b; memset(&b, 0, sizeof b); b.rad = 4; RGBA bf = fg;
		if (!V.on || !strcmp(st, "mono")) { if (hv) b.bg = cl ? cx("#e11d48", 1) : cx("#000000", .15); if (hv && cl) bf = cx("#ffffff", 1); }
		else if (!strcmp(st, "wire")) { b.rad = 3; b.bw = 1; b.bc = cmix(fg, .55, CLEAR); if (hv) { b.bg = cl ? cx("#e11d48", 1) : cmix(V.bg, .22, CLEAR); if (cl) { bf = cx("#ffffff", 1); b.bc = cx("#e11d48", 1); } } }
		else if (!strcmp(st, "candy")) { b.rad = 999; b.bg = hv && cl ? cx("#e11d48", 1) : V.bt; b.gl0 = .55; b.gl1 = .15; shd_add(&b, 0, 1, 0, cx("#ffffff", .7), 1); shd_add(&b, 0, 1, 2, cx("#000000", .333), 0); }
		else if (!strcmp(st, "retro")) { b.rad = 0; b.bg = cx(hv && !cl ? "#d4d4d4" : "#c0c0c0", 1); bf = hv && cl ? cx("#b91c1c", 1) : cx("#000000", 1);
			if (pr) { shd_add(&b, 1, 1, 0, cx("#404040", 1), 1); shd_add(&b, -1, -1, 0, cx("#ffffff", 1), 1); shd_add(&b, 2, 2, 0, cx("#808080", 1), 1); }
			else { shd_add(&b, -1, -1, 0, cx("#404040", 1), 1); shd_add(&b, 1, 1, 0, cx("#ffffff", 1), 1); shd_add(&b, -2, -2, 0, cx("#808080", 1), 1); shd_add(&b, 2, 2, 0, cx("#dfdfdf", 1), 1); } }
		else { if (hv) b.bg = cl ? cx("#e11d48", 1) : cx("#000000", .15); if (hv && cl) bf = cx("#ffffff", 1); }
		box_paint(cr, bx, by, bw, bh, &b);
		const char *g = i == 1 && c->max ? "❐" : G[i]; double gw = css_width(cr, g, "Arial", 600, 12);
		css_text(cr, g, "Arial", 600, 12, bx + (bw - gw) / 2, by + b.bw, bh - 2 * b.bw, bf, 0); }
	cairo_pop_group_to_source(cr);
	if (grey) {   /* (filter: grayscale(1)) */
		cairo_surface_t *g = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, fw, (int)ceil(36 * Z)); cairo_t *gc = cairo_create(g); cairo_set_source(gc, cairo_get_source(cr)); cairo_paint(gc); cairo_destroy(gc);
		cairo_surface_flush(g); unsigned char *d = cairo_image_surface_get_data(g); int stw = cairo_image_surface_get_stride(g);
		for (int y = 0; y < cairo_image_surface_get_height(g); y++) for (int x = 0; x < fw; x++) { unsigned char *p = d + y * stw + x * 4; int l = (int)lround(.0722 * p[0] + .7152 * p[1] + .2126 * p[2]); p[0] = p[1] = p[2] = l > p[3] ? p[3] : l; }
		cairo_surface_mark_dirty(g); cairo_save(cr); cairo_identity_matrix(cr); cairo_set_source_surface(cr, g, 0, 0); cairo_paint_with_alpha(cr, tbo); cairo_restore(cr); cairo_surface_destroy(g);
	} else cairo_paint_with_alpha(cr, tbo);
	cairo_destroy(cr); cairo_surface_destroy(sf);
}
/* the windows' shadows, on the desktop beneath them (0 10px 28px rgba(0,0,0,.28)) */
static void frames_shadow(cairo_t *cr)
{
	if (!WM_OK || LOCKED) return;
	for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (c->min || c->full || c->max || c->hidden || c->trans) continue;
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
	if (!strcmp(what, "lock")) { if (http("POST", "/api/desktop/lock", NULL, 0, NULL) == 200) signin_show(1); return; }
	if (!strcmp(what, "logout")) {
		GList *c = g_list_copy(WINS); for (GList *l = c; l; l = l->next) appwin_close_now(l->data); g_list_free(c);
		http("POST", "/api/desktop/logout", NULL, 0, NULL); USER_ID[0] = 0; signin_show(0); return;
	}
	char p[80]; snprintf(p, sizeof p, "/api/power?action=%s", what); http("POST", p, NULL, 0, NULL);
}
/* (1.6.0 Part 12, "The rule") the start menu as the page draws it (base_v24.html #menu, styles25.css): a white panel
   (dark with the dark theme) 244 px wide at the bottom left, 2 px above the taskbar, as tall as its list (at most the
   screen less 70 px), with its shadow; the search box, the account's line, then the page's own list, row by row as
   the page gave it (desktop/apps "menu": its separators, headings, greyed and indented rows), scrolled with the wheel.
   A row under the pointer is the accent colour. Typing searches the list. The shadow falls on what is behind the
   menu: a copy of the screen taken as it opens. */
typedef struct { char t, a[40], v[40], cls[48], txt[160], ic[24], emo[48]; double op, fs, fw, pl, pt, pb, ls; int up; RGBA col; double y, h; int shown; } MRow;
static MRow *MR; static int NMR;
static GdkPixbuf *MBG; static double MSCROLL, MCONT; static int MHOV = -1, MX, MY, MW, MH; static double MBX, MBY, MBW, MBH;   /* the window, and the menu in it */
static char MQ[128]; static GtkWidget *MAREA; static GtkIMContext *MIM;
static int MCARET; static guint MCARET_T;   /* the search box's caret, blinking as Chromium's (500 ms), shown again on each key */
static gboolean menu_blink(gpointer d) { (void)d; MCARET = !MCARET; if (MAREA) gtk_widget_queue_draw(MAREA); return G_SOURCE_CONTINUE; }
static void caret_restart(void) { MCARET = 1; if (MCARET_T) g_source_remove(MCARET_T); MCARET_T = g_timeout_add(500, menu_blink, NULL); }
/* (an input method for typing into what the shell draws, as GtkEntry has: dead keys, compose) */
static void im_realize(GtkWidget *w, gpointer im) { gtk_im_context_set_client_window(im, gtk_widget_get_window(w)); gtk_im_context_focus_in(im); }
static int menu_dark(void) { return S.darkui; }
static void menu_hide(void) { if (MENU) gtk_widget_hide(MENU); MHOV = -1; if (MCARET_T) { g_source_remove(MCARET_T); MCARET_T = 0; } state_write(); }
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
static gboolean menu_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)w; (void)d; int dark = menu_dark();
	if (MBG) { gdk_cairo_set_source_pixbuf(cr, MBG, 0, 0); cairo_paint(cr); }
	cairo_scale(cr, Z, Z); double ox = (MBX - MX / Z), oy = (MBY - MY / Z);   /* the menu's place in the window, in the page's units */
	cairo_translate(cr, ox, oy);
	/* its shadow: 0 12px 32px rgba(0,0,0,.3) */
	{ int pad = 48, sw = (int)ceil((MBW + 2 * pad) * Z), sh = (int)ceil((MBH + 2 * pad) * Z); cairo_surface_t *m = cairo_image_surface_create(CAIRO_FORMAT_A8, sw, sh); cairo_t *mc = cairo_create(m);
		cairo_scale(mc, Z, Z); rrect(mc, pad, pad, MBW, MBH, 10); cairo_set_source_rgba(mc, 0, 0, 0, 1); cairo_fill(mc); cairo_destroy(mc); a8_blur(m, 16 * Z);
		cairo_save(cr); cairo_scale(cr, 1 / Z, 1 / Z); cairo_set_source_rgba(cr, 0, 0, 0, .3); cairo_mask_surface(cr, m, -pad * Z, (12 - pad) * Z); cairo_restore(cr); cairo_surface_destroy(m); }
	Box b; memset(&b, 0, sizeof b); b.bg = cx(dark ? "#0f172a" : "#ffffff", 1); b.bw = 1; b.bc = cx(dark ? "#334155" : "#d1d5db", 1); b.rad = 10; box_paint(cr, 0, 0, MBW, MBH, &b);
	RGBA ink = cx(dark ? "#e2e8f0" : "#000000", 1);
	cairo_save(cr); rrect(cr, 1, 1, MBW - 2, MBH - 2, 9); cairo_clip(cr);
	double cx0 = 7, y = 7 - MSCROLL, cw = MBW - 14;
	/* the search box (.ssbox: 2px 4px 6px; the input 29 px, 6px 8px, 13.33 px, focused: a 2 px outline of the accent colour) */
	{ double ix = cx0 + 4, iy = y + 2, iw = cw - 8, ih = 29; Box o; memset(&o, 0, sizeof o); o.bg = ca(accent(), 1); o.rad = 8; box_paint(cr, ix - 2, iy - 2, iw + 4, ih + 4, &o);
		Box in; memset(&in, 0, sizeof in); in.bg = cx(dark ? "#0b1220" : "#ffffff", 1); in.bw = 1; in.bc = cx(dark ? "#334155" : "#d1d5db", 1); in.rad = 6; box_paint(cr, ix, iy, iw, ih, &in);
		cairo_save(cr); cairo_rectangle(cr, ix + 9, iy + 1, iw - 18, ih - 2); cairo_clip(cr);   /* (an input's text is cut at its padding, no ellipsis) */
		double tw = css_text(cr, MQ[0] ? MQ : "Search apps, settings and files", UIFAM, 400, 40 / 3.0, ix + 9, iy + 7, 0, MQ[0] ? ink : cx("#757575", 1), 0); cairo_restore(cr);
		if (MCARET) { srca(cr, ink); cairo_rectangle(cr, round(ix + 9 + (MQ[0] ? tw : 0)), iy + 7, 1, 15); cairo_fill(cr); } }
	y += 37;
	/* the account (.mh: 600 13px, 8px 10px 10px, a line below, 4 px apart) */
	{ char h[200]; snprintf(h, sizeof h, "👤 %s%s", USER_N, !strcmp(USER_R, "admin") ? " ★" : "");
		css_text(cr, h, UIFAM, 600, 13, cx0 + 10, y + 8, 0, ink, cw - 20); srca(cr, cx(dark ? "#334155" : "#d1d5db", 1)); cairo_rectangle(cr, cx0, y + 33, cw, 1); cairo_fill(cr); y += 38; }
	/* the list */
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
	cairo_restore(cr);
	return TRUE;
}
static int menu_row_at(double ex, double ey)
{
	double x = ex / Z - (MBX - MX / Z), y = ey / Z - (MBY - MY / Z); if (x < 7 || x > MBW - 7 || y < 1 || y > MBH - 1) return -1;
	double ly = 7 - MSCROLL + 75;
	for (int i = 0; i < NMR; i++) { MRow *r = &MR[i]; if (r->shown && r->t == 'l' && y >= ly + r->y && y < ly + r->y + r->h) return strstr(r->cls, "offh") ? -1 : i; }
	return -1;
}
static void power_dialog(void);
static void menu_do(int i)
{
	if (i < 0 || i >= NMR) return; MRow *r = &MR[i]; char a[40]; snprintf(a, sizeof a, "%s", r->a[0] ? r->a : r->v); menu_hide();
	if (!strcmp(a, "logoff")) { power("logout"); return; }
	if (!strcmp(a, "off")) { power_dialog(); return; }
	if (!strcmp(a, "docs")) { app_open("explorer", "C:\\PROGRAMS"); return; }
	if (app_find(a)) { for (GList *l = WINS; l; l = l->next) { AppWin *w = l->data; if (!strcmp(w->k, a)) { appwin_activate(w); return; } } app_open(a, NULL); return; }
	/* (the page's other actions, in a one-app window: oneapp59.js "do-NAME") */
	char k[48]; snprintf(k, sizeof k, "do-%s", a); app_open(k, NULL);
}
static gboolean menu_motion(GtkWidget *w, GdkEventMotion *e, gpointer d) { (void)w; (void)d; int h = menu_row_at(e->x, e->y); if (h != MHOV) { MHOV = h; gtk_widget_queue_draw(MAREA); } return TRUE; }
static gboolean menu_leave(GtkWidget *w, GdkEventCrossing *e, gpointer d) { (void)w; (void)e; (void)d; if (MHOV != -1) { MHOV = -1; gtk_widget_queue_draw(MAREA); } return TRUE; }
static gboolean menu_click(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)w; (void)d; if (e->type != GDK_BUTTON_PRESS || e->button != 1) return TRUE;
	double x = e->x / Z - (MBX - MX / Z), y = e->y / Z - (MBY - MY / Z);
	if (x < 0 || y < 0 || x > MBW || y > MBH) { menu_hide(); return TRUE; }   /* (outside: the shadow) */
	menu_do(menu_row_at(e->x, e->y)); return TRUE;
}
static gboolean menu_scroll(GtkWidget *w, GdkEventScroll *e, gpointer d)
{
	(void)w; (void)d; double dy = e->direction == GDK_SCROLL_UP ? -100 : e->direction == GDK_SCROLL_DOWN ? 100 : 0;
	if (e->direction == GDK_SCROLL_SMOOTH) { double sx, sy; gdk_event_get_scroll_deltas((GdkEvent *)e, &sx, &sy); dy = sy * 100; }
	double mx = fmax(0, MCONT + 14 - MBH); MSCROLL = fmin(mx, fmax(0, MSCROLL + dy)); MHOV = menu_row_at(e->x, e->y); gtk_widget_queue_draw(MAREA); state_write(); return TRUE;
}
static gboolean on_menu_key(GtkWidget *w, GdkEventKey *e, gpointer d)
{
	(void)w; (void)d;
	if (e->keyval == GDK_KEY_Escape) { menu_hide(); return TRUE; }
	if (e->keyval == GDK_KEY_Return || e->keyval == GDK_KEY_KP_Enter) { for (int i = 0; i < NMR; i++) if (MR[i].shown && MR[i].t == 'l' && !strstr(MR[i].cls, "offh")) { menu_do(MHOV >= 0 ? MHOV : i); break; } return TRUE; }
	if (e->keyval == GDK_KEY_Down || e->keyval == GDK_KEY_Up) { int s = e->keyval == GDK_KEY_Down ? 1 : -1, i = MHOV;
		for (int n = 0; n < NMR; n++) { i = (i + s + NMR) % NMR; if (MR[i].shown && MR[i].t == 'l' && !strstr(MR[i].cls, "offh")) break; } MHOV = i; gtk_widget_queue_draw(MAREA); return TRUE; }
	if (e->keyval == GDK_KEY_BackSpace) { size_t n = strlen(MQ); if (n) { const char *p = g_utf8_find_prev_char(MQ, MQ + n); MQ[p ? p - MQ : 0] = 0; } }
	else { if (e->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)) return FALSE; gtk_im_context_filter_keypress(MIM, e); return TRUE; }
	MSCROLL = 0; MHOV = -1; caret_restart(); gtk_widget_queue_draw(MAREA); state_write(); return TRUE;
}
static void menu_commit(GtkIMContext *im, const char *str, gpointer d)
{
	(void)im; (void)d; size_t l = strlen(MQ), n = strlen(str); for (const char *c = str; *c; c++) if ((unsigned char)*c < 32) return;
	if (l + n < sizeof MQ) { memcpy(MQ + l, str, n); MQ[l + n] = 0; }
	MSCROLL = 0; MHOV = -1; caret_restart(); gtk_widget_queue_draw(MAREA); state_write();
}
static gboolean on_menu_out(GtkWidget *w, GdkEvent *e, gpointer d) { (void)w; (void)e; (void)d; menu_hide(); return FALSE; }
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
	/* the window: the menu and its shadow's room (32 px around, 12 px lower), on the screen */
	int x0 = (int)floor((MBX - 32) * Z), y0 = (int)floor((MBY - 20) * Z), x1 = (int)ceil((MBX + MBW + 32) * Z), y1 = (int)ceil((MBY + MBH + 44) * Z);
	MX = MAX(0, x0); MY = MAX(TOPRES, y0); MW = MIN(SW, x1) - MX; MH = MIN(top ? SH : SH - BARH, y1) - MY;
	if (MBG) g_object_unref(MBG); MBG = gdk_pixbuf_get_from_window(gdk_get_default_root_window(), MX, MY, MW, MH);
	gtk_window_resize(GTK_WINDOW(MENU), MW, MH); gtk_widget_set_size_request(MAREA, MW, MH); gtk_window_move(GTK_WINDOW(MENU), MX, MY);
	caret_restart(); gtk_widget_show_all(MENU); gtk_window_present(GTK_WINDOW(MENU)); gtk_widget_grab_focus(MAREA); wm_restack();
	GdkWindow *gw = gtk_widget_get_window(MENU); if (gw) gdk_window_focus(gw, GDK_CURRENT_TIME);
	state_write();
}

/* (1.6.0 Part 12, "The rule") Start › Shut Down...: the page's Power dialog (core25.js shutdownDlg, .rmodal/.rmbox): the
   screen dimmed (rgba(15,23,42,.45)), a white box in the middle with its shadow, "Power", the question and four
   buttons (Shut down, Restart, Sleep, Cancel), the first one focused (a 2 px ring of the accent colour); Tab moves the
   focus, Enter or a click chooses. Shut down and Restart end the session first, Sleep locks the screen. */
static GtkWidget *PWRA; static GdkPixbuf *PWRBG; static int PWRF, PWRH = -1; static double PWRB[4][4];
static const char *const PWRT[4] = {"⏻ Shut down", "↻ Restart", "☾ Sleep", "Cancel"}, *const PWRK[4] = {"poweroff", "reboot", "suspend", "x"};
static void pwr_geom(cairo_t *cr, double *bx, double *by, double *bw, double *bh)
{
	double W = SW / Z, H = SH / Z, x = 0; for (int i = 0; i < 4; i++) { double w = css_width(cr, PWRT[i], UIFAM, 400, 13) + 22; PWRB[i][0] = x; PWRB[i][2] = w; PWRB[i][3] = 24; x += w + (i < 3 ? 8 : 0); }
	*bw = x + 40; *bh = 18 + 18 + 13 + 15 + 14 + 24 + 6 + 15.59 + 18; *bx = (W - *bw) / 2; *by = (H - *bh) / 2;
	for (int i = 0; i < 4; i++) { PWRB[i][0] += *bx + 20; PWRB[i][1] = *by + 18 + 18 + 13 + 15 + 14; }
}
static gboolean pwr_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)w; (void)d; if (PWRBG) { gdk_cairo_set_source_pixbuf(cr, PWRBG, 0, 0); cairo_paint(cr); }
	cairo_set_source_rgba(cr, 15 / 255.0, 23 / 255.0, 42 / 255.0, .45); cairo_paint(cr); cairo_scale(cr, Z, Z);
	double bx, by, bw, bh; pwr_geom(cr, &bx, &by, &bw, &bh);
	{ int pad = 70; cairo_surface_t *m = cairo_image_surface_create(CAIRO_FORMAT_A8, (int)ceil((bw + 2 * pad) * Z), (int)ceil((bh + 2 * pad) * Z)); cairo_t *mc = cairo_create(m);
		cairo_scale(mc, Z, Z); rrect(mc, pad, pad, bw, bh, 12); cairo_set_source_rgba(mc, 0, 0, 0, 1); cairo_fill(mc); cairo_destroy(mc); a8_blur(m, 25 * Z);
		cairo_save(cr); cairo_scale(cr, 1 / Z, 1 / Z); cairo_set_source_rgba(cr, 0, 0, 0, .4); cairo_mask_surface(cr, m, (bx - pad) * Z, (by - pad + 20) * Z); cairo_restore(cr); cairo_surface_destroy(m); }
	Box b; memset(&b, 0, sizeof b); b.bg = cx("#ffffff", 1); b.rad = 12; box_paint(cr, bx, by, bw, bh, &b);
	RGBA ink = cx("#111827", 1);
	css_text(cr, "Power", UIFAM, 700, 15.21, bx + 20, by + 18, 18, ink, 0);
	css_text(cr, "What do you want the computer to do?", UIFAM, 400, 13, bx + 20, by + 18 + 18 + 13, 15, ink, 0);
	for (int i = 0; i < 4; i++) { double *r = PWRB[i];
		if (i == PWRF) { Box o; memset(&o, 0, sizeof o); o.bg = ca(accent(), 1); o.rad = 6; box_paint(cr, r[0] - 2, r[1] - 2, r[2] + 4, r[3] + 4, &o); }
		Box k; memset(&k, 0, sizeof k); k.bg = cx(i == PWRH ? "#eaf0ff" : "#ffffff", 1); k.bw = 1; k.bc = cx(i == PWRH ? "#a9bff5" : "#d1d5db", 1); k.rad = 4; box_paint(cr, r[0], r[1], r[2], r[3], &k);
		double tw = css_width(cr, PWRT[i], UIFAM, 400, 13); css_text(cr, PWRT[i], UIFAM, 400, 13, r[0] + (r[2] - tw) / 2, r[1] + 1, 22, cx("#000000", 1), 0); }
	return TRUE;
}
static int pwr_at(double x, double y) { x /= Z; y /= Z; for (int i = 0; i < 4; i++) if (x >= PWRB[i][0] && x < PWRB[i][0] + PWRB[i][2] && y >= PWRB[i][1] && y < PWRB[i][1] + PWRB[i][3]) return i; return -1; }
static void pwr_close(void) { if (PWR) gtk_widget_hide(PWR); if (PWRBG) { g_object_unref(PWRBG); PWRBG = NULL; } state_write(); }
static void pwr_do(int i)
{
	if (i < 0) return; pwr_close(); const char *a = PWRK[i];
	if (!strcmp(a, "x")) return;
	if (!strcmp(a, "suspend")) { power("lock"); char p[80]; snprintf(p, sizeof p, "/api/power?action=suspend"); http("POST", p, NULL, 0, NULL); return; }
	power(a);
}
static gboolean pwr_motion(GtkWidget *w, GdkEventMotion *e, gpointer d) { (void)w; (void)d; int h = pwr_at(e->x, e->y); if (h != PWRH) { PWRH = h; gtk_widget_queue_draw(PWRA); } return TRUE; }
static gboolean pwr_click(GtkWidget *w, GdkEventButton *e, gpointer d) { (void)w; (void)d; if (e->type == GDK_BUTTON_PRESS && e->button == 1) pwr_do(pwr_at(e->x, e->y)); return TRUE; }
static gboolean pwr_key(GtkWidget *w, GdkEventKey *e, gpointer d)
{
	(void)w; (void)d;
	if (e->keyval == GDK_KEY_Tab || e->keyval == GDK_KEY_ISO_Left_Tab || e->keyval == GDK_KEY_Right || e->keyval == GDK_KEY_Left) { int back = e->keyval == GDK_KEY_ISO_Left_Tab || e->keyval == GDK_KEY_Left || (e->state & GDK_SHIFT_MASK);
		PWRF = (PWRF + (back ? 3 : 1)) % 4; gtk_widget_queue_draw(PWRA); return TRUE; }
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
	if (PWRBG) g_object_unref(PWRBG); PWRBG = gdk_pixbuf_get_from_window(gdk_get_default_root_window(), 0, 0, SW, SH);
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
	char ct[32], cd[64]; sign_clock_texts(ct, sizeof ct, cd, sizeof cd); char st[256]; stars(st, sizeof st, SN_PASS);
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
		css_text(cr, "Password", LOGINF, 400, 12, ix, y0 + 244, 14, cx("#ffffff", .85), 0);
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
		sign_input(cr, ix, by + 123, iw, 23, 4, 1, cx("#d1d5db", 1), F, 40 / 3.0, 6, st, "Password", SFOC == 1, ca(accent(), 1));
		if (SN_MSG[0]) css_text(cr, SN_MSG, F, 400, 13, ix, by + 152, 15.59, cx("#fca5a5", 1), iw);
		double uw = css_width(cr, "Unlock", F, 400, 13) + 22, sw = css_width(cr, "Switch user", F, 400, 13) + 22;
		double *r = SRECT[2]; r[0] = ix + iw - uw; r[1] = by + 181.6; r[2] = uw; r[3] = 23; sign_button(cr, r, "Unlock", F, 13, cx("#2563eb", 1), cx("#ffffff", 1), 1, cx("#2563eb", 1), 4, SFOC == 2, SHOV == 2);
		r = SRECT[3]; r[0] = SRECT[2][0] - 8 - sw; r[1] = by + 181.6; r[2] = sw; r[3] = 23; sign_button(cr, r, "Switch user", F, 13, cx(SHOV == 3 ? "#eaf0ff" : "#ffffff", 1), cx("#000000", 1), 1, cx(SHOV == 3 ? "#a9bff5" : "#d1d5db", 1), 4, SFOC == 3, 0);
		SRECT[4][2] = 0;
	}
	return TRUE;
}
typedef struct { char *body, *path; int code; char *reply; } Job;
static gboolean signin_done(gpointer p)
{
	Job *j = p; SIGN_BUSY = 0;
	JV *v = jparse(j->reply);
	if (j->code == 200) {
		if (!SIGN_LOCK) { jstr(v, "id", USER_ID, sizeof USER_ID); jstr(v, "n", USER_N, sizeof USER_N); jstr(v, "r", USER_R, sizeof USER_R); }
		LOCKED = 0; gtk_widget_hide(SIGN); explicit_bzero(SN_PASS, sizeof SN_PASS); SN_MSG[0] = 0; if (SCARET_T) { g_source_remove(SCARET_T); SCARET_T = 0; }
		settings_load(); apps_load(); if (BAR) gtk_widget_show_all(BAR); wm_restack();
		if (WM_OK) wm_hide_all(0); else for (GList *l = WINS; l; l = l->next) gtk_widget_show(((AppWin *)l->data)->win);
	} else {
		char m[300] = ""; jstr(v, "error", m, sizeof m);
		snprintf(SN_MSG, sizeof SN_MSG, "%s", m[0] ? m : j->code ? "The sign-in did not work." : "Rudolf OS's system service did not answer.");
		explicit_bzero(SN_PASS, sizeof SN_PASS); SFOC = 1; sign_caret();
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
		g_string_append(b, "\"p\":"); json_esc(b, SN_PASS); }
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
	if (i == 0 || i == 1) { SFOC = i; sign_caret(); } else sign_act(i);
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
	SIGN_LOCK = lock; LOCKED = lock; menu_hide(); if (PWR) gtk_widget_hide(PWR);
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
	JV *s = jprop(v, "s");
	jstr(s, "bs", n.bs, sizeof n.bs); jstr(s, "b1", n.b1, sizeof n.b1); jstr(s, "b2", n.b2, sizeof n.b2); jstr(s, "dyn", n.dyn, sizeof n.dyn); jstr(s, "dm", n.dm, sizeof n.dm);
	jstr(s, "wpfit", n.wpfit, sizeof n.wpfit); jstr(s, "tb", n.tb, sizeof n.tb); jstr(s, "tbstyle", n.tbstyle, sizeof n.tbstyle); jstr(s, "tbacm", n.tbacm, sizeof n.tbacm);
	jstr(s, "tbbstyle", n.tbbstyle, sizeof n.tbbstyle); jstr(s, "tbbacm", n.tbbacm, sizeof n.tbbacm); jstr(s, "twstyle", n.twstyle, sizeof n.twstyle); jstr(s, "twacm", n.twacm, sizeof n.twacm);
	jstr(s, "accent", n.accent, sizeof n.accent); jstr(s, "df", n.df, sizeof n.df); jstr(s, "tz", n.tz, sizeof n.tz); jstr(s, "ff", n.ff, sizeof n.ff);
	char wp[64] = ""; jstr(s, "wpimg", wp, sizeof wp); n.wpimg = wp[0] != 0;
	jstr(s, "icons", n.icons, sizeof n.icons); n.scale = (int)jnum(s, "scale", 100); if (n.scale < 75 || n.scale > 250) n.scale = 100;
	n.darkui = jbool(s, "darkui"); n.tbac = jbool(s, "tbac"); n.tbbac = jbool(s, "tbbac"); n.h24 = jbool(s, "h24"); n.doff = jnum(s, "doff", 0); n.lat = jnum(s, "lat", NAN);
	if (!n.bs[0]) snprintf(n.bs, sizeof n.bs, "solid"); if (!n.b1[0]) snprintf(n.b1, sizeof n.b1, "#008080"); if (!n.b2[0]) snprintf(n.b2, sizeof n.b2, "#000080");
	if (!n.tb[0]) snprintf(n.tb, sizeof n.tb, "bottom"); if (!n.wpfit[0]) snprintf(n.wpfit, sizeof n.wpfit, "fill"); if (!n.dm[0]) snprintf(n.dm, sizeof n.dm, "auto");
	jstr(v, "acct", n.acct, sizeof n.acct); jstr(v, "wpacct", n.wpacct, sizeof n.wpacct); n.gen = jnum(v, "gen", 0); n.notes = (int)jnum(v, "notes", 0); n.wallpaper = jbool(v, "wallpaper");
	JV *p = jprop(v, "pins"); for (int i = 0; i < 24; i++) { JV *o = jat(p, i); if (!o) break; if (o->t == 3) snprintf(n.pins[n.npins++], 40, "%s", o->s);  }
	int moved = strcmp(n.tb, S.tb) != 0 || n.scale != S.scale; S = n; ui_family(S.ff);
	wg_load(v);   /* (the widgets: item 2) */
	  jfree(v);
	/* the page's display scaling: the taskbar, the title bars and the start menu at the page's size (1.5.9 step 7) */
	Z = S.scale / 100.0; BARH = (int)lround(46 * Z); TH = (int)lround(35 * Z);
	css_apply(); wall_make(); if (moved) { bar_place(); if (WM_OK) wm_relayout(); di_layout(); if (DESKAREA) desk_dirty(); } bar_redraw(); if (SAREA) gtk_widget_queue_draw(SAREA); if (WM_OK) frames_redraw();
}
static void apps_load(void)
{
	JV *v = jget("/api/desktop/apps"); if (!v) return;
	JV *a = jprop(v, "apps"); App *L = NULL; int n = 0;
	for (int i = 0; i < 400; i++) { JV *o = jat(a, i); if (!o) break; L = realloc(L, sizeof *L * (n + 1)); App *x = &L[n++]; memset(x, 0, sizeof *x);
		jstr(o, "k", x->k, sizeof x->k); jstr(o, "name", x->name, sizeof x->name); jstr(o, "icon", x->icon, sizeof x->icon); jstr(o, "kw", x->kw, sizeof x->kw); jstr(o, "ic", x->ic, sizeof x->ic); }
	/* the apps' pictures (PNG, as data: addresses) and the version */
	if (ICONS) g_hash_table_destroy(ICONS); ICONS = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
	JV *ics = jprop(v, "icons");
	for (int i = 0; ics && ics->t == 5 && i < ics->len; i++) {
		JV *o = ics->v[i]; static const char PRE[] = "data:image/png;base64,"; if (!o || o->t != 3 || strncmp(o->s, PRE, sizeof PRE - 1)) continue;
		gsize n = 0; guchar *raw = g_base64_decode(o->s + sizeof PRE - 1, &n); GByteArray *b = g_byte_array_new_take(raw, n); guint8 z = 0; g_byte_array_append(b, &z, 1);
		GdkPixbuf *px = pix_from_bytes(b); g_byte_array_free(b, TRUE); if (px) g_hash_table_insert(ICONS, g_strdup(ics->k[i]), px);
	}
	VER[0] = 0; jstr(v, "ver", VER, sizeof VER);
	di_load(v); menu_rows_load(v); if (DESKAREA) desk_dirty();
	free(APPS); APPS = L; NAPPS = n;
	if (BG) g_hash_table_destroy(BG); BG = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	JV *bg = jprop(v, "bg");
	if (bg && bg->t == 5) {
		for (int i = 0; i < bg->len; i++) { JV *o = bg->v[i]; const char *kk = bg->k[i]; char d[200] = "", l[200] = ""; jstr(o, "d", d, sizeof d); jstr(o, "l", l, sizeof l);
			if (d[0] && !strstr(d, "..") && !strstr(l, "..")) g_hash_table_insert(BG, g_strdup(kk), g_strdup_printf("%s\n%s", d, l[0] ? l : d));  }
		}
	 jfree(v);
	WALL_KEY[0] = 0; wall_make(); bar_redraw(); if (MAREA) gtk_widget_queue_draw(MAREA);
}
static void tray_load(void)
{
	/* the network as the page picks it (net31.js net31Pick): the real adapters, the one with the default route and an
	   address, else the first one up with an address; Wi-Fi's signal from its rssi */
	JV *v = jget("/api/net"); int up = 0, wifi = 0, anywifi = 0, n = 0, pick = -1;
	JV *ifs = jprop(v, "interfaces");
	for (int pass = 0; pass < 2 && pick < 0; pass++) for (int i = 0; i < 16; i++) { JV *o = jat(ifs, i); if (!o) break; if (jbool(o, "virtual")) continue; char ip[64] = "", st[16] = "";
		jstr(o, "ip", ip, sizeof ip); jstr(o, "state", st, sizeof st); if (!pass) { n++; if (jbool(o, "wireless")) anywifi = 1; }
		if (ip[0] && (pass ? !strcmp(st, "up") : jbool(o, "default"))) { pick = i; break; } }
	NET_RSSI = 1;
	if (pick >= 0) { JV *o = jat(ifs, pick); up = 1; wifi = jbool(o, "wireless"); JV *r = jprop(o, "rssi"); if (r && r->t == 2) NET_RSSI = (int)r->n; }
	else wifi = anywifi || !n;
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
static gboolean tick_clock(gpointer d) { (void)d; char o[32]; snprintf(o, sizeof o, "%s", CLOCK_T); clock_text(); if (strcmp(o, CLOCK_T)) { bar_redraw(); wg_dirty_kind("clock"); wg_dirty_kind("cal"); if (SAREA && SIGN && gtk_widget_get_visible(SIGN)) gtk_widget_queue_draw(SAREA); } if (S.dyn[0] && !S.wpimg) wall_make(); return G_SOURCE_CONTINUE; }
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
static gboolean cm_draw(GtkWidget *wd, cairo_t *cr, gpointer d)
{
	(void)wd; (void)d; if (CMBG) { gdk_cairo_set_source_pixbuf(cr, CMBG, 0, 0); cairo_paint(cr); }
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
static void cm_close(void) { if (CMWIN && gtk_widget_get_visible(CMWIN)) { gdk_seat_ungrab(gdk_display_get_default_seat(gdk_display_get_default())); gtk_widget_hide(CMWIN); } if (CMBG) { g_object_unref(CMBG); CMBG = NULL; } CMHOV = -1; state_write(); }
static void cm_choose(int i) { if (i < 0 || i >= NCMI || CMIT[i].sep || CMIT[i].dis) return; char a[96]; snprintf(a, sizeof a, "%s", CMIT[i].act); void (*cb)(const char *) = CM_CB; cm_close(); if (cb) cb(a); }
static gboolean cm_motion(GtkWidget *w, GdkEventMotion *e, gpointer d) { (void)w; (void)d; int h = cm_at(e->x, e->y); if (h != CMHOV) { CMHOV = h; gtk_widget_queue_draw(CMAREA); } return TRUE; }
static gboolean cm_press(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)w; (void)d; if (e->type != GDK_BUTTON_PRESS) return TRUE;
	double x = (e->x + CMWX) / Z, y = (e->y + CMWY) / Z;
	if (x < CMX || x >= CMX + CMBW || y < CMY || y >= CMY + CMBH) { cm_close(); return TRUE; }   /* (a click elsewhere closes it, as the page's) */
	if (e->button == 1) cm_choose(cm_at(e->x, e->y)); return TRUE;
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
	double vw = SW / Z, vh = SH / Z; CMX = px / Z; CMY = py / Z;
	if (CMX + CMBW > vw - 4) CMX = fmax(4, vw - CMBW - 4); if (CMY + CMBH > vh - 4) CMY = fmax(4, vh - CMBH - 4);
	if (!CMWIN) { CMWIN = gtk_window_new(GTK_WINDOW_POPUP); gtk_window_set_title(GTK_WINDOW(CMWIN), "Menu"); CMAREA = gtk_drawing_area_new(); gtk_widget_set_can_focus(CMAREA, TRUE);
		gtk_widget_add_events(CMAREA, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_KEY_PRESS_MASK); gtk_container_add(GTK_CONTAINER(CMWIN), CMAREA);
		g_signal_connect(CMAREA, "draw", G_CALLBACK(cm_draw), NULL); g_signal_connect(CMAREA, "motion-notify-event", G_CALLBACK(cm_motion), NULL);
		g_signal_connect(CMAREA, "button-press-event", G_CALLBACK(cm_press), NULL); g_signal_connect(CMAREA, "key-press-event", G_CALLBACK(cm_key), NULL); }
	/* (the window: the box, its ring and its shadow; what is behind copied, there being no compositor) */
	int x0 = (int)fmax(0, floor((CMX - 30) * Z)), y0 = (int)fmax(0, floor((CMY - 30) * Z)), x1 = (int)fmin(SW, ceil((CMX + CMBW + 30) * Z)), y1 = (int)fmin(SH, ceil((CMY + CMBH + 50) * Z));
	CMWX = x0; CMWY = y0; CMBG = gdk_pixbuf_get_from_window(gdk_get_default_root_window(), x0, y0, x1 - x0, y1 - y0);
	gtk_widget_set_size_request(CMAREA, x1 - x0, y1 - y0); gtk_window_resize(GTK_WINDOW(CMWIN), x1 - x0, y1 - y0); gtk_window_move(GTK_WINDOW(CMWIN), x0, y0);
	CMHOV = -1; gtk_widget_show_all(CMWIN); XRaiseWindow(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), GDK_WINDOW_XID(gtk_widget_get_window(CMWIN)));
	gdk_seat_grab(gdk_display_get_default_seat(gdk_display_get_default()), gtk_widget_get_window(CMWIN), GDK_SEAT_CAPABILITY_ALL, TRUE, NULL, NULL, NULL, NULL);
	gtk_widget_grab_focus(CMAREA); state_write();
}

/* the page's dialogs (.rmodal .rmbox): Rename (renameDlg) and Open with (files25.js openWith) */
enum { DLG_NONE, DLG_RENAME, DLG_OPENWITH };
static GtkWidget *DGWIN, *DGAREA; static GdkPixbuf *DGBG; static GtkIMContext *DGIM;
static int DGK, DGHOV = -1, DGFOC, DGSEL, DGCARET, DGALW, DGBUSY; static guint DGCARET_T;
static char DGFILE[512], DGNAME[128], DGVAL[128], DGMSG[200], DGEXT[16];
typedef struct { char k[40], name[80], icon[24]; } Opener;
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
	char *t = qesc(DGVAL), *ex = g_strdup_printf("&op=rename&to=%s", t); app_open_ex("do-fs", DGFILE, ex, 1); g_free(t); g_free(ex); gtk_widget_queue_draw(DGAREA);
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
	cm_close(); if (DGBG) g_object_unref(DGBG); DGBG = gdk_pixbuf_get_from_window(gdk_get_default_root_window(), 0, 0, SW, SH);
	DGK = kind; DGHOV = -1; DGFOC = 0; DGALW = 0; DGBUSY = 0; DGMSG[0] = 0; DGCARET = 1;
	gtk_widget_set_size_request(DGAREA, SW, SH); gtk_window_resize(GTK_WINDOW(DGWIN), SW, SH); gtk_window_move(GTK_WINDOW(DGWIN), 0, 0);
	gtk_widget_show_all(DGWIN); XRaiseWindow(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), GDK_WINDOW_XID(gtk_widget_get_window(DGWIN)));
	gdk_seat_grab(gdk_display_get_default_seat(gdk_display_get_default()), gtk_widget_get_window(DGWIN), GDK_SEAT_CAPABILITY_ALL, TRUE, NULL, NULL, NULL, NULL);
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
static void page_done(JSCValue *v)
{
	char err[300] = "", ask[24] = "", file[512] = "", name[128] = "", ext[16] = "", ok[8] = ""; char *apps = NULL;
	jsstr(v, "err", err, sizeof err); jsstr(v, "ask", ask, sizeof ask); jsstr(v, "file", file, sizeof file); jsstr(v, "name", name, sizeof name); jsstr(v, "ext", ext, sizeof ext); jsstr(v, "ok", ok, sizeof ok);
	JSCValue *ap = jsc_value_object_get_property(v, "apps"); if (ap && jsc_value_is_string(ap)) apps = jsc_value_to_string(ap); if (ap) g_object_unref(ap);
	apps_load();   /* (the page gave its catalogue again: the desktop's files as they are now) */
	if (DGK == DLG_RENAME && DGBUSY) { DGBUSY = 0; if (err[0]) { snprintf(DGMSG, sizeof DGMSG, "%s", err); gtk_widget_queue_draw(DGAREA); } else dg_close(); }
	if (!strcmp(ask, "rename")) rename_ask(file, name);
	else if (!strcmp(ask, "openwith")) openwith_ask(file, ext, apps);
	g_free(apps); g_timeout_add(1500, apps_reload, NULL); state_write();
}

/* ------------------------------------------------------------------ the desktop's icons (1.6.0 Part 12, item 1)
   The page's desktop icons, as the page shows them for the account (desktop/apps "desk": each icon's name, what it
   opens and its picture; "cells": where the page put them), drawn on the wallpaper on the page's grid (84 x 86 at the
   icon size "is" and the display scaling). A click selects, a double-click (or Enter) opens, dragging moves an icon to
   the nearest free cell (kept by rudolfd for the shell: desktop/cells, the page's places used until then); the
   icon's menu (Open) and the desktop's (Arrange icons by name, Refresh, Display Properties, File Explorer, Task Manager,
   Settings). Files on the desktop and dropping files come with item 1's second part. */
typedef struct { char n[80], k[40], kind[12], ic[24], icon[48], cell[96], fp[512]; int c, r, sel, full, own; } DIcon;   /* (own: a shortcut of the account's own, the page's .dapp) */
static struct { int i, on, moved; double sx, sy, dx, dy; } DID = { -1, 0, 0, 0, 0, 0, 0 };   /* (an icon being dragged, DX DY from its place) */   /* (fp: a file's or folder's path, cell: the page's key for its place) */
static DIcon DI[96]; static int NDI; static char DI_ACCT[80]; static int DI_IS = 100;
static char DI_MENU[8] = "-";   /* (the menu last opened, for the tests) */
static GHashTable *DI_CELLS;   /* name -> "c,r" (the shell's own, else the page's) */
static int di_w(void) { return (int)lround(84 * DI_IS / 100.0 * Z); }
static int di_h(void) { return (int)lround(86 * DI_IS / 100.0 * Z); }
static int di_shown(void) { return USER_ID[0] && !LOCKED && NDI && !strcmp(DI_ACCT, USER_ID); }
static void di_grid(int *nc, int *nr) { *nc = (SW - 8) / di_w(); *nr = (work_h() - 8) / di_h(); if (*nc < 1) *nc = 1; if (*nr < 1) *nr = 1; }
static void di_xy(const DIcon *d, int *x, int *y) { *x = (int)lround(8 * Z) + d->c * di_w(); *y = work_top() + (int)lround(((!strcmp(S.tb, "top") ? 2 : 0) + 8) * Z) + d->r * di_h(); }   /* (the page's #desk: 2 px below a taskbar at the top) */
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
		if (d->sel) for (int l = 0; l < nl; l++) {   /* (.ic.sel span: the accent colour behind each line, 3 px corners) */
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
	if (!DESKC || DESKD) { if (!DESKC) DESKC = cairo_image_surface_create(CAIRO_FORMAT_RGB24, SW, SH); cairo_t *c2 = cairo_create(DESKC); desk_draw(w, c2, d); bar_outside(c2); di_draw(c2); cairo_destroy(c2); DESKD = 0; NWG_ALL_DIRTY = 1; }
	cairo_set_source_surface(cr, DESKC, 0, 0); cairo_paint(cr); wg_paint(cr); frames_shadow(cr); return TRUE;   /* (the widgets over the desktop: item 2) */
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
static void di_page(const char *on, const char *item, int hidden)
{
	char *o = qesc(on ? on : ""), *t = qesc(item), *ex = g_strdup_printf("%s%s&item=%s", on && *on ? "&on=" : "", on && *on ? o : "", t);
	app_open_ex("do-desk", NULL, ex, hidden); g_free(o); g_free(t); g_free(ex);
}
static void di_arrange(void);
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
		CM("Arrange icons by name", "", "", "arrange"), CM("Desktop icons…", "🖥️", "", "page:Desktop icons…"), CM("Refresh", "", "F5", "refresh"), CMS,
		CM("Open File Explorer", "", "", "app:explorer"), CM("Open Command Prompt", "", "", "app:prompt"), CM("Show all windows", "", "Super+Tab", "overview"), CM("Task Manager", "📊", "Ctrl+Shift+Esc", "app:taskmgr"), CMS,
		CM("Open settings…", "", "", "app:settings") };
	static const CMI FILEM[] = { CM("Open", "↗", "", "icon:Open"), CM("Open with…", "", "", "iconh:Open with…"), CMS, CM("Rename", "", "F2", "rename"), CM("Move to Trash", "", "Del", "iconh:Move to Trash"), CMS,
		CM("Properties", "", "", "icon:Properties") };
	static const CMI TRASHM[] = { CM("Open", "↗", "Enter", "open"), CM("Empty trash…", "", "", "icon:Empty trash…"), CMS, CM("Arrange icons by name", "", "", "arrange"), CMS, CM("Properties", "", "", "icon:Properties") };
	static const CMI APPM[] = { CM("Open", "↗", "Enter", "open"), CM("Remove from desktop", "➖", "Del", "iconh:Remove from desktop"), CMS, CM("Arrange icons by name", "", "", "arrange"),
		CM("Desktop icons…", "", "", "icon:Desktop icons…"), CMS, CM("Properties", "", "", "icon:Properties") };
	int i = di_sel(); DIcon *d = icon && i >= 0 ? &DI[i] : NULL; CMI m[24]; int n = 0; const char *title = "Desktop";
	if (!d) { n = G_N_ELEMENTS(DESKM); memcpy(m, DESKM, sizeof DESKM); }
	else if (d->fp[0]) { n = G_N_ELEMENTS(FILEM); memcpy(m, FILEM, sizeof FILEM); title = d->n; }
	else if (!strcmp(d->kind, "trash")) { n = G_N_ELEMENTS(TRASHM); memcpy(m, TRASHM, sizeof TRASHM); m[1].dis = !d->full; title = d->n; }
	else { n = G_N_ELEMENTS(APPM); memcpy(m, APPM, sizeof APPM); title = d->n; }
	CM_RING = !d; cm_open(e->x_root, e->y_root, title, m, n, on_di_act);
	snprintf(DI_MENU, sizeof DI_MENU, "%s", d ? (d->fp[0] ? "file" : "icon") : "desk"); state_write();
}
static gboolean desk_press(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)w; (void)d; menu_hide(); if (wg_press(e)) return TRUE;   /* (a widget: item 2) */
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
		int nc, nr, ix, iy; di_grid(&nc, &nr); di_xy(&DI[DID.i], &ix, &iy); int c = (int)lround((ix + e->x - DID.sx - 8) / (double)di_w()), r = (int)lround((iy + e->y - DID.sy - work_top() - 8) / (double)di_h());
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
	int i = di_sel(); if (i < 0) return FALSE; DIcon *di = &DI[i];
	if (e->keyval == GDK_KEY_F2 && di->fp[0]) { rename_ask(di->fp, di->n); return TRUE; }
	if (e->keyval == GDK_KEY_Delete) { if (di->fp[0]) di_page(di->n, "Move to Trash", 1); else if (strcmp(di->kind, "trash")) di_page(di->n, "Remove from desktop", 1); return TRUE; }
	return FALSE;
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
static int wg_kind_n(const char *t);
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
static void wg_rect(const Wg *w, double *x, double *y, double *ww, double *hh) { *x = w->x; *y = wg_top0() + w->y; *ww = wg_w(w); *hh = wg_h(w); }
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
	else if (k == 3) { const char *L[3] = { "CPU: ", "Memory: ", "Windows: " }; char nw[16]; int c = 0; for (GList *l = WINS; l; l = l->next) if (!((AppWin *)l->data)->hidden) c++; snprintf(nw, sizeof nw, "%d", c);
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
	if (same) { for (int i = 0; i < nn; i++) g_free(n[i].txt); if (wx && wg_kind_n("weather")) { WX_ST = 0; wx_go(); } return; }
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
static void state_write(void)
{
	if (!STATEF) return;
	GString *s = g_string_new(NULL); int top = !strcmp(S.tb, "top"), by = top ? TOPRES : SH - BARH;
	g_string_append_printf(s, "screen %d %d\nuser %s\nlocked %d\nsignin %d\nmenu %d\nbar %d %d %d %d %d\nclock %s|%s\n", SW, SH, USER_ID[0] ? USER_N : "-", LOCKED,
		SIGN && gtk_widget_get_visible(SIGN), MENU && gtk_widget_get_visible(MENU), BAR && gtk_widget_get_visible(BAR), 0, by, SW, BARH, CLOCK_T, CLOCK_D);
	for (int i = 0; i < NHITS; i++) g_string_append_printf(s, "hit %s %s %d %d %d %d\n", HITS[i].kind, HITS[i].arg[0] ? HITS[i].arg : "-", HITS[i].x, HITS[i].y + by, HITS[i].w, HITS[i].h);
	/* the start menu's rows on the screen (mrow ACTION x y w h) */
	for (int i = 0; MENU && gtk_widget_get_visible(MENU) && i < NMR; i++) { MRow *r = &MR[i]; if (!r->shown || r->t != 'l' || strstr(r->cls, "offh")) continue;
		double y = MBY + 7 - MSCROLL + 75 + r->y; if (y < MBY || y + r->h > MBY + MBH) continue;
		g_string_append_printf(s, "mrow %s %d %d %d %d\n", r->a[0] ? r->a : r->v[0] ? r->v : "-", (int)lround((MBX + 7) * Z), (int)lround(y * Z), (int)lround((MBW - 14) * Z), (int)lround(r->h * Z)); }
	if (SIGN) g_string_append_printf(s, "signmsg %s\nsignfields %s|%ld|%d\n", SN_MSG, SN_NAME, g_utf8_strlen(SN_PASS, -1), SFOC);   /* (the password's length only) */
	for (GList *l = WINS; l; l = l->next) { AppWin *a = l->data; GdkWindow *g = a->win ? gtk_widget_get_window(a->win) : NULL;
		Client *c = WM_OK ? client_of_app(a) : NULL;
		g_string_append_printf(s, "app %s %lx %d %s\n", a->k, g ? (unsigned long)GDK_WINDOW_XID(g) : 0UL, c ? c == FOCUS : a == ACTIVE, a->title); }
	for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data;
		g_string_append_printf(s, "client %lx %lx %d %d %d %d min=%d max=%d full=%d focus=%d app=%s %s\n", (unsigned long)c->w, (unsigned long)c->frame, c->x, c->y, c->cw, c->ch,
			c->min, c->max, c->full, c == FOCUS, c->app ? c->app->k : "-", c->title);
		if (c->app && c->app->win && c->app->view && GTK_IS_WIDGET(c->app->view)) { GtkAllocation al; gtk_widget_get_allocation(c->app->view, &al); g_string_append_printf(s, "view %lx %d %d\n", (unsigned long)c->w, al.width, al.height); } }   /* (the page's own size: its window's) */
	g_string_append_printf(s, "wm %d\nicons %u %s\n", WM_OK, ICONS ? g_hash_table_size(ICONS) : 0, VER[0] ? VER : "-");
	for (int i = 0; di_shown() && i < NDI; i++) { int x, y; di_xy(&DI[i], &x, &y); g_string_append_printf(s, "dicon %d %d %d %d %d %d %s|%s|%s\n", DI[i].c, DI[i].r, x, y, di_w(), DI[i].sel, DI[i].k, DI[i].kind, DI[i].n); }
	g_string_append_printf(s, "dmenu %s\n", DI_MENU);
	/* (item 2) the widgets on the screen: widget N TYPE X Y W H (the screen's pixels) PAGE-X PAGE-Y|TEXT; the note typed in; the weather */
	for (int i = 0; wg_shown() && i < NWG; i++) { double X, Y, W, H; wg_rect(&WG[i], &X, &Y, &W, &H); char *t = g_strescape(WG[i].txt ? WG[i].txt : "", NULL);
		g_string_append_printf(s, "widget %d %s %d %d %d %d %g %g|%s\n", i, WG[i].t, (int)lround(X * Z), (int)lround(Y * Z), (int)lround(W * Z), (int)lround(H * Z), WG[i].x, WG[i].y, t); g_free(t); }
	g_string_append_printf(s, "wgnote %d %d %d\nweather %d %s|%s|%s\n", NF, NCAR, NANC, WX_ST, WX_T, WX_C, WX_L);
	/* (the page's menu and dialogs as the shell shows them) */
	if (CMWIN && gtk_widget_get_visible(CMWIN)) { g_string_append_printf(s, "cmenu %d %d %d %d %s\n", (int)lround(CMX * Z), (int)lround(CMY * Z), (int)lround(CMBW * Z), (int)lround(CMBH * Z), CMTITLE[0] ? CMTITLE : "-");
		double ry = CMY + 5 + (CMTITLE[0] ? 22 : 0); for (int i = 0; i < NCMI; i++) { if (!CMIT[i].sep) g_string_append_printf(s, "cmitem %d %d %d %s|%s\n", (int)lround((CMX + 5 + 50) * Z), (int)lround((ry + 12) * Z), CMIT[i].dis, CMIT[i].act, CMIT[i].label); ry += cm_row_h(&CMIT[i]); } }
	g_string_append_printf(s, "dialog %s %s|%s\n", DGK == DLG_RENAME ? "rename" : DGK == DLG_OPENWITH ? "openwith" : "-", DGK ? DGNAME : "-", DGMSG);
	for (int i = 0; DGK && i < NDGB; i++) g_string_append_printf(s, "dgbtn %d %d %d\n", i, (int)lround((DGB[i].x + DGB[i].w / 2) * Z), (int)lround((DGB[i].y + DGB[i].h / 2) * Z));
	g_string_append_printf(s, "devlink %d %d\ndlask %s\n", DL_ON && DLBAN && gtk_widget_get_visible(DLBAN), TOPRES, DLASK && gtk_widget_get_visible(DLASK) ? DL_PEND : "-");
	char tmp[600]; snprintf(tmp, sizeof tmp, "%s.tmp", STATEF); g_file_set_contents(tmp, s->str, s->len, NULL); rename(tmp, STATEF); g_string_free(s, TRUE);
}

/* ------------------------------------------------------------------ start */
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
	g_set_prgname("RudolfNative"); gdk_set_program_class("RudolfNative");
	gtk_init(&argc, &argv);
	wm_init();
	if (SW <= 0 || SH <= 0) { GdkScreen *scr = gdk_screen_get_default(); SW = gdk_screen_get_width(scr); SH = gdk_screen_get_height(scr); }
	/* rudolfd may still be starting */
	for (int i = 0; i < 60 && !get_token(); i++) g_usleep(500000);
	if (!TOKEN[0]) { fprintf(stderr, "rudolf-native: rudolfd did not answer on %s:%d\n", HOST, PORT); return 1; }
	memset(&S, 0, sizeof S); S.lat = NAN; snprintf(S.tb, sizeof S.tb, "bottom");
	LOGO = pix_get("/favicon.png");
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
	/* (the widgets' note: typing, item 2) */
	gtk_widget_add_events(DESKAREA, GDK_BUTTON_MOTION_MASK | GDK_FOCUS_CHANGE_MASK); NIM = gtk_im_context_simple_new(); g_signal_connect(NIM, "commit", G_CALLBACK(note_commit), NULL);
	g_signal_connect(DESKAREA, "realize", G_CALLBACK(im_realize), NIM); g_signal_connect(DESK, "focus-out-event", G_CALLBACK(on_desk_out), NULL);
	gtk_widget_show_all(DESK);
	/* the taskbar, above everything */
	BAR = gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_window_set_decorated(GTK_WINDOW(BAR), FALSE); gtk_window_set_type_hint(GTK_WINDOW(BAR), GDK_WINDOW_TYPE_HINT_DOCK);
	gtk_window_set_title(GTK_WINDOW(BAR), "Taskbar"); gtk_window_set_keep_above(GTK_WINDOW(BAR), TRUE); gtk_window_set_default_size(GTK_WINDOW(BAR), SW, BARH);
	BARAREA = gtk_drawing_area_new(); gtk_widget_add_events(BARAREA, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK);
	g_signal_connect(BARAREA, "draw", G_CALLBACK(bar_draw), NULL); g_signal_connect(BARAREA, "button-press-event", G_CALLBACK(bar_press), NULL);
	g_signal_connect(BARAREA, "button-release-event", G_CALLBACK(bar_release), NULL); g_signal_connect(BARAREA, "motion-notify-event", G_CALLBACK(bar_motion), NULL);
	g_signal_connect(BARAREA, "leave-notify-event", G_CALLBACK(bar_leave), NULL); gtk_container_add(GTK_CONTAINER(BAR), BARAREA);
	g_signal_connect(BAR, "delete-event", G_CALLBACK(gtk_true), NULL);
	bar_place(); gtk_widget_realize(BAR);
	/* signed in already (this program restarted): the session goes on; else the sign-in screen */
	JV *ses = jget("/api/desktop/session");
	if (ses) { jstr(ses, "id", USER_ID, sizeof USER_ID); jstr(ses, "n", USER_N, sizeof USER_N); jstr(ses, "r", USER_R, sizeof USER_R); jfree(ses); gtk_widget_show_all(BAR); }
	else signin_show(0);
	g_timeout_add_seconds(3, tick_settings, NULL); g_timeout_add_seconds(10, tick_tray, NULL); g_timeout_add_seconds(5, tick_clock, NULL);
	tick_heart(NULL); g_timeout_add_seconds(10, tick_heart, NULL);
	tick_devlink(NULL); g_timeout_add_seconds(4, tick_devlink, NULL);   /* (as the page: every 4 s) */
	state_write();
	gtk_main();
	return 0;
}
