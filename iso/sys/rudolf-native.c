/*
 * rudolf-native - the native desktop engine, a prototype (Rudolf OS 1.5.9, docs/plan-1.5.9.md Part 12).
 *
 * Settings › System health › Desktop engine › Native (test): the session starts this instead of the desktop page, as the
 * desktop user:   rudolf-native --size=WxH [--draw=gpu|mixed|soft] [--base=http://127.0.0.1:8080]
 * One GTK 3 process draws what the desktop page draws around the apps, natively:
 *   - the wallpaper: a picture of your own, a drawn background's light or dark picture, or the colours and patterns;
 *   - the taskbar: the start button, the pinned apps, the open windows, the tray (network, sound, battery, the
 *     notifications' count) and the clock, in the taskbar's styles and the accent colour (accent56.js, tbbtn59.js);
 *   - the start menu (the apps, a search box, Sign out, Lock, Restart, Shut down);
 *   - the sign-in and lock screens, the password checked by rudolfd (desktop/login: it never gives out a hash).
 * The apps stay web pages: each opens in a WebKit window of its own showing the desktop page in one-app mode
 * (index.html#app=NAME, oneapp59.js); all of them share one WebKit web process (related-view).
 * Everything it knows comes from rudolfd's API (docs/api.md: the desktop/ requests), with the token rudolfd writes into its page.
 *
 * Built like rudolf-webkit:  gcc -O2 -o rudolf-native rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm
 * WebP pictures (the drawn backgrounds) through WebKit's own libwebp, opened when needed (no new library).
 * Tests: iso/qt/host/native59_t.sh (under Xvfb, a stand-in rudolfd). Copyright (C) 2026 Rudolf OS contributors.
 * GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <webkit2/webkit2.h>
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
static int PORT = 8080, SW, SH, BARH = 46, TH = 30;
static double Z = 1;                          /* Display Properties' display scaling (scale: 75 to 250 %), as the page's zoom */
static int TOPRES;                            /* the development link's banner, at the top of the screen (0: none) */
static WebKitHardwareAccelerationPolicy HWPOL = WEBKIT_HARDWARE_ACCELERATION_POLICY_ON_DEMAND;
static int SOFT;                              /* --draw=soft: the lighter effects, as rudolf-webkit */
static const char *STATEF;                    /* RUDOLF_NATIVE_STATE: what is where, for the tests */
typedef struct {
	char bs[16], b1[16], b2[16], dyn[48], dm[8], wpfit[12], tb[8], tbstyle[12], tbacm[12], tbbstyle[12], tbbacm[12];
	char twstyle[12], twacm[12], accent[16], df[8], tz[64], acct[80], wpacct[80], icons[12];
	int tbac, tbbac, h24, wpimg, wallpaper, notes, npins, scale; double lat, doff, gen; char pins[24][40];
} Settings;
static Settings S;
typedef struct { char k[40], name[80], icon[48], kw[200], ic[24]; } App;
static App *APPS; static int NAPPS; static GHashTable *BG;   /* key -> "dark\nlight" */
static GHashTable *ICONS;                     /* an app's picture (App.ic) -> GdkPixbuf, as the page draws it (desktop/apps) */
static char VER[32];                          /* Rudolf OS's version, for the start button (the page's RVER) */
typedef struct { char id[80], n[128], r[12]; int key, old; } Acct;
static Acct ACC[32]; static int NACC, GUEST_OK, SETUP; static char LASTACC[80];
static char USER_ID[80], USER_N[128], USER_R[12];   /* signed in ("" = not) */
static int LOCKED;
/* the tray */
static int NET_UP = -1, NET_WIFI, VOL = -1, MUTED, BAT = -1, CHARGING;

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
/* a style's colours: the base, the writing on it, and its shades (accent56.js acApply, tbbtn59.js, titlebar59.js) */
typedef struct { char st[12]; RGB base, ink, bt, bh, hi, lo, sh; int black; } Look;
static int style_ok(const char *s) { return s && (!strcmp(s, "wire") || !strcmp(s, "mono") || !strcmp(s, "candy") || !strcmp(s, "retro")); }
static Look look_make(const char *style, RGB base)
{
	Look L; memset(&L, 0, sizeof L); snprintf(L.st, sizeof L.st, "%s", style_ok(style) ? style : "standard");
	if (!strcmp(L.st, "mono")) base = grey_of(base);
	double l = lightness(base); L.base = base; L.black = ink_black(base); L.ink = L.black ? (RGB){0, 0, 0} : (RGB){1, 1, 1};
	L.bt = shade(base, l > 50 ? -10 : 10); L.bh = shade(base, l > 50 ? -18 : 18);
	L.hi = shade(base, fmin(30, 96 - l)); L.lo = shade(base, -fmin(30, l - 6)); L.sh = shade(base, -fmin(50, l - 2));
	return L;
}
static RGB accent(void) { return hexc(S.accent, "#2563eb"); }
static Look bar_look(void)
{
	static const char *const BASE[][2] = {{"standard", "#111827"}, {"wire", "#111827"}, {"mono", "#3f3f46"}, {"candy", "#db2777"}, {"retro", "#c0c0c0"}};
	const char *st = style_ok(S.tbstyle) ? S.tbstyle : "standard"; RGB b = {0.067, 0.094, 0.153};
	for (int i = 0; i < 5; i++) if (!strcmp(BASE[i][0], st)) b = hexc(BASE[i][1], "#111827");
	if (S.tbac) b = variant(accent(), S.tbacm);
	return look_make(st, b);
}
/* the buttons: their own style (tbbtn59.js), else the bar's */
static int btn_own(void) { return style_ok(S.tbbstyle) || S.tbbac; }
static Look btn_look(void)
{
	if (!btn_own()) return bar_look();
	static const char *const BASE[][2] = {{"standard", "#1f2937"}, {"wire", "#111827"}, {"mono", "#52525b"}, {"candy", "#db2777"}, {"retro", "#c0c0c0"}};
	const char *st = style_ok(S.tbbstyle) ? S.tbbstyle : "standard"; RGB b = hexc("#1f2937", NULL);
	for (int i = 0; i < 5; i++) if (!strcmp(BASE[i][0], st)) b = hexc(BASE[i][1], "#1f2937");
	if (S.tbbac) b = variant(accent(), S.tbbacm);
	return look_make(st, b);
}
static void rrect(cairo_t *cr, double x, double y, double w, double h, double r)
{
	r = fmin(r, fmin(w, h) / 2); cairo_new_sub_path(cr);
	cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0); cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
	cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI); cairo_arc(cr, x + r, y + r, r, M_PI, 3 * M_PI / 2); cairo_close_path(cr);
}
/* a button's face in a style: standard and mono flat, wire an outline, candy a glossy pill, retro raised edges */
static void face(cairo_t *cr, const Look *L, double x, double y, double w, double h, int hover, int pressed, int bar_standard_plain)
{
	const char *st = L->st;
	if (!strcmp(st, "wire")) {
		if (hover) { rrect(cr, x, y, w, h, 3); src(cr, L->base, .25); cairo_fill(cr); }
		rrect(cr, x + .5, y + .5, w - 1, h - 1, 3); src(cr, L->ink, .55); cairo_set_line_width(cr, 1); cairo_stroke(cr); return;
	}
	if (!strcmp(st, "candy")) {
		rrect(cr, x, y, w, h, h / 2); src(cr, hover ? L->bh : L->bt, 1); cairo_fill_preserve(cr);
		cairo_pattern_t *g = cairo_pattern_create_linear(0, y, 0, y + h);
		cairo_pattern_add_color_stop_rgba(g, 0, 1, 1, 1, .55); cairo_pattern_add_color_stop_rgba(g, .5, 1, 1, 1, .15); cairo_pattern_add_color_stop_rgba(g, .52, 1, 1, 1, 0);
		cairo_set_source(cr, g); cairo_fill(cr); cairo_pattern_destroy(g); return;
	}
	if (!strcmp(st, "retro")) {
		cairo_rectangle(cr, x, y, w, h); src(cr, hover ? L->bh : L->bt, 1); cairo_fill(cr);
		RGB a = pressed ? L->sh : L->hi, b = pressed ? L->hi : L->sh; cairo_set_line_width(cr, 1);
		src(cr, a, 1); cairo_move_to(cr, x + .5, y + h - .5); cairo_line_to(cr, x + .5, y + .5); cairo_line_to(cr, x + w - .5, y + .5); cairo_stroke(cr);
		src(cr, b, 1); cairo_move_to(cr, x + w - .5, y + .5); cairo_line_to(cr, x + w - .5, y + h - .5); cairo_line_to(cr, x + .5, y + h - .5); cairo_stroke(cr); return;
	}
	/* standard and mono: the usual dark grey buttons (or the colour's shade), lighter when the pointer is on them */
	if (bar_standard_plain && !hover) return;
	rrect(cr, x, y, w, h, !strcmp(st, "mono") ? 4 : 5); src(cr, hover ? L->bh : L->bt, 1); cairo_fill(cr);
}
/* the bar itself */
static void bar_fill(cairo_t *cr, const Look *L, double w, double h, int top)
{
	if (!strcmp(L->st, "wire")) { cairo_rectangle(cr, 0, 0, w, h); src(cr, L->base, .45); cairo_fill(cr);
		src(cr, L->ink, .7); cairo_rectangle(cr, 0, top ? h - 1 : 0, w, 1); cairo_fill(cr); return; }
	if (!strcmp(L->st, "candy")) { cairo_pattern_t *g = cairo_pattern_create_linear(0, 0, 0, h);
		cairo_pattern_add_color_stop_rgb(g, 0, L->hi.r, L->hi.g, L->hi.b); cairo_pattern_add_color_stop_rgb(g, .48, L->base.r, L->base.g, L->base.b); cairo_pattern_add_color_stop_rgb(g, 1, L->lo.r, L->lo.g, L->lo.b);
		cairo_rectangle(cr, 0, 0, w, h); cairo_set_source(cr, g); cairo_fill(cr); cairo_pattern_destroy(g); return; }
	cairo_rectangle(cr, 0, 0, w, h); src(cr, L->base, 1); cairo_fill(cr);
	if (!strcmp(L->st, "retro")) { src(cr, L->hi, 1); cairo_rectangle(cr, 0, top ? h - 2 : 0, w, 1); cairo_fill(cr); src(cr, (RGB){1, 1, 1}, 1); cairo_rectangle(cr, 0, top ? h - 1 : 1, w, 1); cairo_fill(cr); }
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
static GtkWidget *DESK, *DESKAREA, *BAR, *BARAREA, *MENU, *SIGN, *DLBAN, *DLASK;
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
	if (DESKAREA) gtk_widget_queue_draw(DESKAREA);
}
static gboolean desk_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)w; (void)d; if (WALL) { cairo_set_source_surface(cr, WALL, 0, 0); cairo_paint(cr); } return TRUE; }

/* ------------------------------------------------------------------ app windows (WebKit, one-app mode) */
typedef struct AppWin { GtkWidget *win, *view; char k[40], title[256], icon[48]; int closing, id; gint64 close_t; } AppWin;
static GList *WINS;   /* AppWin*, oldest first */
static AppWin *ACTIVE;
static WebKitWebContext *CTX; static WebKitUserContentManager *UCM; static int NEXTID;
static void bar_redraw(void);
static void app_open(const char *k, const char *file);
static void frame_redraw_app(AppWin *a);
static const App *app_find(const char *k) { for (int i = 0; i < NAPPS; i++) if (!strcmp(APPS[i].k, k)) return &APPS[i]; return NULL; }
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
static void on_view_close(WebKitWebView *v, gpointer d) { (void)v; appwin_close_now(d); }
static void on_msg(WebKitUserContentManager *m, WebKitJavascriptResult *r, gpointer d)
{
	(void)m; (void)d; JSCValue *v = webkit_javascript_result_get_js_value(r); if (!jsc_value_is_object(v)) return;
	char ws[16] = ""; jsstr(v, "w", ws, sizeof ws); int id = atoi(ws); AppWin *a = NULL;
	for (GList *l = WINS; l; l = l->next) if (((AppWin *)l->data)->id == id) a = l->data;
	if (!a) return;
	char op[16] = "", x[256] = "", f[1024] = ""; jsstr(v, "op", op, sizeof op);
	if (!strcmp(op, "close")) appwin_close_now(a);
	else if (!strcmp(op, "title")) { jsstr(v, "icon", x, sizeof x); if (x[0]) snprintf(a->icon, sizeof a->icon, "%s", x); bar_redraw(); }
	else if (!strcmp(op, "open")) { jsstr(v, "app", x, sizeof x); jsstr(v, "file", f, sizeof f); if (x[0]) app_open(x, f[0] ? f : NULL); }
}
static gboolean on_crash_reload(gpointer v) { webkit_web_view_reload(WEBKIT_WEB_VIEW(v)); return G_SOURCE_REMOVE; }
static void on_crash(WebKitWebView *v, WebKitWebProcessTerminationReason r, gpointer d)
{
	(void)d; fprintf(stderr, "rudolf-native: an app's page stopped (%d): loading it again\n", (int)r); g_timeout_add_seconds(1, on_crash_reload, v);
}
static void on_win_destroy(GtkWidget *w, gpointer d)
{
	(void)w; AppWin *a = d; WINS = g_list_remove(WINS, a); if (ACTIVE == a) ACTIVE = NULL; g_free(a); bar_redraw(); state_write();
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
static void appwin_show(AppWin *a) { gtk_widget_show_all(a->win); wm_manage_app(a); gtk_window_present(GTK_WINDOW(a->win)); bar_redraw(); state_write(); }
static void on_ready(WebKitWebView *v, gpointer d) { (void)v; appwin_show(d); }
static char *app_url(const char *k, const char *file, int id)
{
	char *fe = file ? g_uri_escape_string(file, NULL, FALSE) : NULL;
	char *u = g_strdup_printf("http://%s:%d/#app=%s&w=%d%s%s", HOST, PORT, k, id, fe ? "&file=" : "", fe ? fe : ""); g_free(fe); return u;
}
/* windows asked for and not made yet (made through an open window's window.open: see app_open) */
typedef struct { char k[40]; int id; } Pending;
static GQueue PENDING = G_QUEUE_INIT;
static GtkWidget *on_create(WebKitWebView *v, WebKitNavigationAction *na, gpointer d)
{
	(void)na; (void)d; Pending *p = g_queue_pop_head(&PENDING);
	if (!p) return NULL;   /* (a page's own window.open: the desktop page sends websites to the browser itself) */
	AppWin *a = appwin_new(p->k, v, p->id); g_free(p);
	g_signal_connect(a->view, "ready-to-show", G_CALLBACK(on_ready), a);
	return a->view;
}
/* an app in a window of its own. All app windows share one WebKit web process: the first starts it, the others are
   opened by an open one (window.open: the page's own, webNativeOpen, as the page sends websites elsewhere), which WebKit keeps in its process (a view merely made "related" to another
   got a process of its own, WebKitGTK 2.52, as navigations may change process) */
static void app_open(const char *k, const char *file)
{
	if (!USER_ID[0] || LOCKED || strspn(k, "abcdefghijklmnopqrstuvwxyz0123456789_-") != strlen(k) || strlen(k) > 39) return;
	ctx_make(); int id = ++NEXTID; char *u = app_url(k, file, id);
	AppWin *rel = NULL; for (GList *l = WINS; l; l = l->next) { AppWin *x = l->data; if (x->view && webkit_web_view_get_uri(WEBKIT_WEB_VIEW(x->view)) && !x->closing) { rel = x; break; } }
	if (rel) {
		Pending *p = g_new0(Pending, 1); snprintf(p->k, sizeof p->k, "%s", k); p->id = id; g_queue_push_tail(&PENDING, p);
		char *js = g_strdup_printf("(typeof webNativeOpen=='function'?webNativeOpen:window.open)(%s,'_blank')", "decodeURIComponent('%s')"), *ue = g_uri_escape_string(u, NULL, FALSE), *js2 = g_strdup_printf(js, ue);
		webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(rel->view), js2, -1, NULL, NULL, NULL, NULL, NULL);
		g_free(js); g_free(js2); g_free(ue); g_free(u); return;
	}
	AppWin *a = appwin_new(k, NULL, id); webkit_web_view_load_uri(WEBKIT_WEB_VIEW(a->view), u); g_free(u);
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
#define BW 4   /* (the title bar's height, TH, follows the display scaling) */
typedef struct Client { Window w, frame; int x, y, cw, ch, min, max, full, sx, sy, scw, sch, ignore_unmap, hover, press, hidden; char title[256]; AppWin *app; Window trans; Time last_click; } Client;
static GList *CLIENTS;   /* oldest first */
static Client *FOCUS;
static Display *DPY; static Window ROOT; static int WM_OK;
static Atom A_PROTO, A_DEL, A_TAKE, A_WMSTATE, A_NSTATE, A_NFULL, A_NACTIVE, A_NLIST, A_NNAME, A_UTF8, A_NSUP, A_NCHECK, A_CHANGE, A_NHIDDEN;
static Cursor CUR[9];
static int wm_err;
static int wm_errh(Display *d, XErrorEvent *e) { (void)d; if (e->error_code == BadAccess) wm_err = 1; return 0; }
static Client *client_of(Window w) { for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data; if (c->w == w || c->frame == w) return c; } return NULL; }
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
static void place(Client *c)
{
	if (c->full) { XMoveResizeWindow(DPY, c->frame, -BW, -TH, SW + 2 * BW, SH + TH + BW); XMoveResizeWindow(DPY, c->w, BW, TH, SW, SH); c->cw = SW; c->ch = SH; }
	else { XMoveResizeWindow(DPY, c->frame, c->x, c->y, fw_of(c), fh_of(c)); XMoveResizeWindow(DPY, c->w, BW, TH, c->cw, c->ch); }
	tell_geom(c);
}
/* the title bar's colours: the accent colour (Normal, Negative, Complementary) when active, grey otherwise */
static Look title_look(int active)
{
	const char *st = style_ok(S.twstyle) ? S.twstyle : "standard";
	RGB b = active ? variant(accent(), S.twacm) : hexc(!strcmp(st, "retro") ? "#808080" : "#9ca3af", NULL);
	return look_make(st, b);
}
static void btn_rects(Client *c, int r[3][4])
{
	int fw = fw_of(c), bw = (int)lround(34 * Z), gap = (int)lround(36 * Z), m = (int)lround(4 * Z);
	for (int i = 0; i < 3; i++) { r[i][0] = fw - BW - gap * (3 - i) - 2; r[i][1] = m; r[i][2] = bw; r[i][3] = TH - 2 * m; }
}
static void frame_draw(Client *c)
{
	if (!c->frame || c->full) return;
	int fw = fw_of(c), fh = fh_of(c), act = c == FOCUS;
	cairo_surface_t *sf = cairo_xlib_surface_create(DPY, c->frame, DefaultVisual(DPY, DefaultScreen(DPY)), fw, fh); cairo_t *cr = cairo_create(sf);
	Look L = title_look(act);
	/* the border and the bar */
	if (!strcmp(L.st, "wire")) { cairo_rectangle(cr, 0, 0, fw, fh); src(cr, hexc("#ffffff", NULL), 1); cairo_fill(cr); cairo_rectangle(cr, 0, 0, fw, TH); src(cr, L.base, .14); cairo_fill(cr);
		cairo_rectangle(cr, .5, .5, fw - 1, fh - 1); src(cr, L.base, 1); cairo_set_line_width(cr, 1); cairo_stroke(cr); cairo_rectangle(cr, 0, TH - 2, fw, 2); cairo_fill(cr); }
	else if (!strcmp(L.st, "candy")) { cairo_pattern_t *g = cairo_pattern_create_linear(0, 0, 0, TH);
		cairo_pattern_add_color_stop_rgb(g, 0, L.hi.r, L.hi.g, L.hi.b); cairo_pattern_add_color_stop_rgb(g, .48, L.base.r, L.base.g, L.base.b); cairo_pattern_add_color_stop_rgb(g, 1, L.lo.r, L.lo.g, L.lo.b);
		cairo_rectangle(cr, 0, 0, fw, fh); src(cr, L.lo, 1); cairo_fill(cr); cairo_rectangle(cr, 0, 0, fw, TH); cairo_set_source(cr, g); cairo_fill(cr); cairo_pattern_destroy(g); }
	else if (!strcmp(L.st, "retro")) { cairo_rectangle(cr, 0, 0, fw, fh); src(cr, hexc("#c0c0c0", NULL), 1); cairo_fill(cr);
		cairo_pattern_t *g = cairo_pattern_create_linear(0, 0, fw, 0); cairo_pattern_add_color_stop_rgb(g, 0, L.lo.r, L.lo.g, L.lo.b); cairo_pattern_add_color_stop_rgb(g, .45, L.base.r, L.base.g, L.base.b); cairo_pattern_add_color_stop_rgb(g, 1, L.hi.r, L.hi.g, L.hi.b);
		cairo_rectangle(cr, 3, 3, fw - 6, TH - 5); cairo_set_source(cr, g); cairo_fill(cr); cairo_pattern_destroy(g); }
	else { cairo_rectangle(cr, 0, 0, fw, fh); src(cr, L.base, 1); cairo_fill(cr); }
	/* the icon and the title */
	char t[320]; snprintf(t, sizeof t, "%s%s%s", c->app && c->app->icon[0] ? c->app->icon : "", c->app && c->app->icon[0] ? "  " : "", c->title);
	RGB ink = !strcmp(L.st, "wire") ? hexc("#111827", NULL) : L.ink;
	text_atz(cr, t, 10 * Z, 0, fw - 140 * Z, TH, !strcmp(L.st, "retro") ? "Sans Bold 10" : "Sans 10", ink, 0);
	/* minimise, maximise, close */
	int r[3][4]; btn_rects(c, r); static const char *const G[3] = {"–", "□", "✕"};
	for (int i = 0; i < 3; i++) {
		Look B = L; int hv = c->hover == i + 1;
		if (i == 2 && hv) B = look_make(L.st, hexc("#e11d48", NULL));
		if (!strcmp(L.st, "retro")) B = look_make("retro", hexc("#c0c0c0", NULL));
		face(cr, &B, r[i][0], r[i][1], r[i][2], r[i][3], hv, c->press == i + 1, 0);
		text_atz(cr, i == 1 && c->max ? "❐" : G[i], r[i][0], r[i][1], r[i][2], r[i][3], "Sans 10", !strcmp(B.st, "retro") ? hexc("#000000", NULL) : i == 2 && hv ? hexc("#ffffff", NULL) : ink, 1);
	}
	cairo_destroy(cr); cairo_surface_destroy(sf);
}
static void frames_redraw(void) { for (GList *l = CLIENTS; l; l = l->next) frame_draw(l->data); }
/* our own windows above the frames: the taskbar (unless a window is full screen), the start menu, the sign-in screen */
static void wm_restack(void)
{
	if (!WM_OK) return;
	if (DESK && gtk_widget_get_window(DESK)) XLowerWindow(DPY, GDK_WINDOW_XID(gtk_widget_get_window(DESK)));
	if (!(FOCUS && FOCUS->full) && BAR && gtk_widget_get_visible(BAR)) XRaiseWindow(DPY, GDK_WINDOW_XID(gtk_widget_get_window(BAR)));
	if (MENU && gtk_widget_get_visible(MENU)) XRaiseWindow(DPY, GDK_WINDOW_XID(gtk_widget_get_window(MENU)));
	if (SIGN && gtk_widget_get_visible(SIGN)) XRaiseWindow(DPY, GDK_WINDOW_XID(gtk_widget_get_window(SIGN)));
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
	}
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
	XSelectInput(DPY, w, PropertyChangeMask | (own ? 0 : StructureNotifyMask));
	XAddToSaveSet(DPY, w); XSetWindowBorderWidth(DPY, w, 0);
	if (wa.map_state != IsUnmapped) c->ignore_unmap++;   /* (reparenting a shown window unmaps it once) */
	XReparentWindow(DPY, w, c->frame, BW, TH); XResizeWindow(DPY, w, c->cw, c->ch);
	CLIENTS = g_list_append(CLIENTS, c);
	XMapWindow(DPY, w); XMapWindow(DPY, c->frame); set_wm_state(c, NormalState);
	grab_clicks(c, 1); tell_geom(c);
	/* asked for full screen before it was shown */
	Atom type; int fmt; unsigned long n = 0, after; unsigned char *d = NULL;
	if (XGetWindowProperty(DPY, w, A_NSTATE, 0, 32, False, XA_ATOM, &type, &fmt, &n, &after, &d) == Success && d) { for (unsigned long i = 0; i < n; i++) if (((Atom *)d)[i] == A_NFULL) { c->full = 0; wm_fullscreen(c, 1); } XFree(d); }
	wm_focus(c);
}
static void wm_unmanage(Client *c, int gone)
{
	CLIENTS = g_list_remove(CLIENTS, c);
	if (!gone) { XReparentWindow(DPY, c->w, ROOT, c->x + BW, c->y + TH); XRemoveFromSaveSet(DPY, c->w); set_wm_state(c, WithdrawnState); grab_clicks(c, 0); }
	XDestroyWindow(DPY, c->frame);
	if (FOCUS == c) { FOCUS = NULL; wm_focus(top_client()); }
	g_free(c); client_list(); bar_redraw(); state_write();
}
/* the lock screen: every window out of sight, then back */
static void wm_hide_all(int hide)
{
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
static struct { Client *c; int edge, px, py, x, y, w, h; } DRAG;   /* edge: 0 move, else 1 left 2 right 4 top 8 bottom */
static int edge_at(Client *c, int x, int y)
{
	int fw = fw_of(c), fh = fh_of(c), e = 0, g = 6;
	if (x < g) e |= 1; if (x >= fw - g) e |= 2; if (y < 3) e |= 4; if (y >= fh - g) e |= 8;
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
	if (DRAG.c) { XUngrabPointer(DPY, e->time); if (DRAG.c == c) tell_geom(c); DRAG.c = NULL; state_write(); return; }
	int b = c->press; c->press = 0; frame_draw(c);
	if (b && b == btn_at(c, e->x, e->y)) { if (b == 1) wm_minimise(c); else if (b == 2) wm_maximise(c, !c->max); else wm_close(c); }
}
static GdkFilterReturn wm_filter(GdkXEvent *xev, GdkEvent *ev, gpointer d)
{
	(void)ev; (void)d; XEvent *e = xev; Client *c;
	switch (e->type) {
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
		if ((c = client_of(e->xunmap.window)) && c->w == e->xunmap.window) {
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
			if (c->frame == e->xbutton.window) frame_press(c, &e->xbutton);
			else { wm_focus(c); XAllowEvents(DPY, ReplayPointer, e->xbutton.time); }   /* a click into a window: it gets the focus, and the click */
			return GDK_FILTER_REMOVE;
		}
		return GDK_FILTER_CONTINUE;
	case ButtonRelease:
		if ((c = client_of(e->xbutton.window)) && c->frame == e->xbutton.window) { frame_release(c, &e->xbutton); return GDK_FILTER_REMOVE; }
		return GDK_FILTER_CONTINUE;
	case MotionNotify:
		if (DRAG.c) { drag_motion(e->xmotion.x_root, e->xmotion.y_root); return GDK_FILTER_REMOVE; }
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
static void frame_redraw_app(AppWin *a) { Client *c = WM_OK ? client_of_app(a) : NULL; if (c) { wm_title(c); frame_draw(c); } }

static void appwin_activate(AppWin *a)
{
	Client *c = WM_OK ? client_of_app(a) : NULL;
	if (c) { if (c == FOCUS && !c->min) wm_minimise(c); else wm_focus(c); return; }
	if (gtk_widget_get_visible(a->win) && ACTIVE == a) { gtk_widget_hide(a->win); ACTIVE = NULL; }   /* (step 4: minimise) */
	else { gtk_widget_show(a->win); gtk_window_present(GTK_WINDOW(a->win)); ACTIVE = a; }
	bar_redraw(); state_write();
}

/* ------------------------------------------------------------------ the taskbar */
static int HOVER = -1, PRESS = -1;
static char CLOCK_T[32], CLOCK_D[32];
static void clock_text(void)
{
	GTimeZone *tz = S.tz[0] ? g_time_zone_new_identifier(S.tz) : NULL; if (!tz) tz = g_time_zone_new_local();
	GDateTime *n0 = g_date_time_new_now(tz), *n = g_date_time_add_seconds(n0, S.doff / 1000.0);
	int h = g_date_time_get_hour(n), m = g_date_time_get_minute(n);
	if (S.h24) snprintf(CLOCK_T, sizeof CLOCK_T, "%02d:%02d", h, m); else snprintf(CLOCK_T, sizeof CLOCK_T, "%d:%02d %s", h % 12 ? h % 12 : 12, m, h < 12 ? "AM" : "PM");
	int Y = g_date_time_get_year(n), M = g_date_time_get_month(n), D = g_date_time_get_day_of_month(n);
	if (!strcmp(S.df, "mdy")) snprintf(CLOCK_D, sizeof CLOCK_D, "%02d/%02d/%04d", M, D, Y);
	else if (!strcmp(S.df, "dmy")) snprintf(CLOCK_D, sizeof CLOCK_D, "%02d/%02d/%04d", D, M, Y);
	else snprintf(CLOCK_D, sizeof CLOCK_D, "%04d/%02d/%02d", Y, M, D);
	g_date_time_unref(n); g_date_time_unref(n0); g_time_zone_unref(tz);
}
/* (the taskbar's places, in the page's units: kept in the screen's pixels) */
static void hit_add(double x, double y, double w, double h, const char *k, const char *a) { if (NHITS >= 96) return; Hit *t = &HITS[NHITS++]; t->x = (int)lround(x * Z); t->y = (int)lround(y * Z); t->w = (int)lround(w * Z); t->h = (int)lround(h * Z); snprintf(t->kind, sizeof t->kind, "%s", k); snprintf(t->arg, sizeof t->arg, "%s", a ? a : ""); }
static void tray_text(int which, char *o, size_t n)
{
	if (which == 0) snprintf(o, n, "%s", NET_UP < 0 ? "🌐" : !NET_UP ? "✈" : NET_WIFI ? "📶" : "🖧");
	else if (which == 1) { if (VOL < 0) snprintf(o, n, "🔊"); else snprintf(o, n, "%s %d%%", MUTED || !VOL ? "🔇" : "🔊", VOL); }
	else if (which == 2) snprintf(o, n, "%s %d%%", CHARGING ? "⚡" : "🔋", BAT);
	else snprintf(o, n, S.notes ? "🔔 %d" : "🔔", S.notes);
}
/* an app's picture as the page draws it (desktop/apps: the icon style's PNG), else its emoji; SZ pixels, centred */
static void app_icon(cairo_t *cr, const App *ap, const char *emoji, double x, double y, double w, double h, double sz, RGB fg)
{
	GdkPixbuf *p = ap && ap->ic[0] && ICONS ? g_hash_table_lookup(ICONS, ap->ic) : NULL;
	if (p) { double s = sz / gdk_pixbuf_get_width(p); cairo_save(cr); cairo_translate(cr, x + (w - sz) / 2, y + (h - sz) / 2); cairo_scale(cr, s, s);
		gdk_cairo_set_source_pixbuf(cr, p, 0, 0); cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD); cairo_paint(cr); cairo_restore(cr); return; }
	char f[24]; snprintf(f, sizeof f, "Sans %d", (int)lround(sz * .62)); text_at(cr, emoji && *emoji ? emoji : "▫", x, y, w, h, f, fg, 1);
}
/* the taskbar, drawn as the page draws it (base_v24.html, accent56.js, tbbtn59.js): 46 px at 100 %, the display scaling
   applied (the page's zoom), the start button with the logo and the version, the pinned apps' pictures in outlined
   buttons, the open windows, the tray and the clock */
static gboolean bar_draw(GtkWidget *wd, cairo_t *cr, gpointer d)
{
	(void)d; int top = !strcmp(S.tb, "top");
	double W = gtk_widget_get_allocated_width(wd) / Z, H = gtk_widget_get_allocated_height(wd) / Z;
	cairo_scale(cr, Z, Z);
	Look B = bar_look(), K = btn_look(); int plain = !btn_own() && !strcmp(B.st, "standard") && !S.tbac;
	RGB fg = btn_own() ? K.ink : B.ink;
	bar_fill(cr, &B, W, H, top); NHITS = 0;
	double x = 3, by = (H - 34) / 2; int i = 0;
	/* the start button: the logo and the name */
	char sl[64]; snprintf(sl, sizeof sl, "Rudolf OS%s%s", VER[0] ? " " : "", VER);
	PangoLayout *pl = pango_cairo_create_layout(cr); PangoFontDescription *fd = pango_font_description_from_string("Sans Bold 9.5");
	pango_layout_set_font_description(pl, fd); pango_layout_set_text(pl, sl, -1); int lw, lh; pango_layout_get_pixel_size(pl, &lw, &lh); pango_font_description_free(fd); g_object_unref(pl);
	double sw = 36 + lw + 10; if (sw < 120) sw = 120; if (sw > 190) sw = 190;
	hit_add(x, by, sw, 34, "start", NULL);
	face(cr, &K, x, by, sw, 34, HOVER == i, PRESS == i, plain); i++;
	if (LOGO) { cairo_save(cr); cairo_translate(cr, x + 6, by + 5); double s = 24.0 / gdk_pixbuf_get_width(LOGO); cairo_scale(cr, s, s);
		gdk_cairo_set_source_pixbuf(cr, LOGO, 0, 0); cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
		if (btn_own() ? K.black : B.black) { cairo_paint_with_alpha(cr, .25); } else cairo_paint(cr); cairo_restore(cr); }
	text_at(cr, sl, x + 36, by, sw - 38, 34, "Sans Bold 9.5", fg, 0);
	x += sw + 6;
	/* the pinned apps: their pictures in outlined buttons (a lighter outline on the usual dark bar) */
	for (int p = 0; p < S.npins; p++, i++) {
		hit_add(x, by, 34, 34, "pin", S.pins[p]);
		if (plain) { if (HOVER == i) { rrect(cr, x, by, 34, 34, 6); cairo_set_source_rgba(cr, 1, 1, 1, .1); cairo_fill(cr); }
			rrect(cr, x + .5, by + .5, 33, 33, 6); cairo_set_source_rgba(cr, 1, 1, 1, .13); cairo_set_line_width(cr, 1); cairo_stroke(cr); }
		else face(cr, &K, x, by, 34, 34, HOVER == i, PRESS == i, 0);
		const App *ap = app_find(S.pins[p]); app_icon(cr, ap, ap ? ap->icon : NULL, x, by, 34, 34, 24, fg); x += 36;
	}
	x += 6;
	/* the tray and the clock, from the right */
	char t[64]; double rx = W - 3;
	clock_text(); double cw = 140; rx -= cw;
	hit_add(rx, by, cw, 34, "clock", NULL); face(cr, &K, rx, by, cw, 34, 0, 0, 0);
	if (plain) { rrect(cr, rx, by + 1, cw, 32, 5); src(cr, hexc("#1f2937", NULL), 1); cairo_fill(cr); }
	text_at(cr, CLOCK_T, rx + 8, by, 70, 34, "Sans 10.5", fg, 0); text_at(cr, CLOCK_D, rx + 70, by, cw - 76, 34, "Sans 8.5", fg, 2);
	int tray_i0 = NHITS;
	for (int k = 3; k >= 0; k--) {
		if (k == 2 && BAT < 0) continue;
		tray_text(k, t, sizeof t); double tw = k == 1 && VOL >= 0 ? 62 : k == 2 ? 64 : k == 3 && S.notes ? 48 : 34; rx -= tw + 4;
		static const char *const KIND[] = {"net", "vol", "bat", "notes"};
		hit_add(rx, by + 4, tw, 26, KIND[k], NULL);
		int me = i + (NHITS - 1 - tray_i0) + 64; face(cr, &K, rx, by + 4, tw, 26, HOVER == me, 0, 0);
		if (plain) { rrect(cr, rx, by + 4, tw, 26, 5); src(cr, hexc(HOVER == me ? "#374151" : "#1f2937", NULL), 1); cairo_fill(cr); }
		text_at(cr, t, rx, by + 4, tw, 26, "Sans 10", fg, 1);
	}
	/* the open windows, between */
	/* (managing windows: every framed window, dialogs left out; else our app windows) */
	GList *TL = NULL; if (WM_OK) { for (GList *l = CLIENTS; l; l = l->next) if (!((Client *)l->data)->trans) TL = g_list_append(TL, l->data); } else TL = g_list_copy(WINS);
	double avail = rx - 8 - x; int n = g_list_length(TL), j = 0; double tw = n ? fmin(200, avail / n) : 0;
	for (GList *l = TL; l && tw >= 40; l = l->next, j++) {
		Client *c = WM_OK ? l->data : NULL; AppWin *a = c ? c->app : l->data;
		double wx = x + j * tw; int act = c ? c == FOCUS : a == ACTIVE, minim = c && c->min;
		hit_add(wx, by, tw - 4, 34, "win", NULL); snprintf(HITS[NHITS - 1].arg, sizeof HITS[0].arg, "%lx", c ? (unsigned long)c->w : (unsigned long)(uintptr_t)a);
		int me = 200 + j; face(cr, &K, wx, by, tw - 4, 34, HOVER == me, 0, 0);
		if (plain) { rrect(cr, wx, by, tw - 4, 34, 5); src(cr, hexc(HOVER == me ? "#374151" : minim ? "#111827" : "#1f2937", NULL), 1); cairo_fill(cr); }
		if (act) { src(cr, accent(), 1); cairo_rectangle(cr, wx + 6, top ? by + 1 : by + 31, tw - 16, 3); cairo_fill(cr); }
		RGB ink = minim ? shade(fg, fg.r > .5 ? -35 : 35) : fg;
		const App *ap = a ? app_find(a->k) : NULL;
		if (a) app_icon(cr, ap, a->icon, wx + 4, by, 22, 34, 18, ink); else text_at(cr, "▣", wx + 4, by, 22, 34, "Sans 11", ink, 1);
		text_at(cr, a ? a->title : c->title, wx + 30, by, tw - 40, 34, "Sans 9.5", ink, 0);
	}
	g_list_free(TL);
	state_write();
	return TRUE;
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
	else app_open("settings", NULL);
}
static gboolean bar_press(GtkWidget *w, GdkEventButton *e, gpointer d) { (void)w; (void)d; if (e->type == GDK_BUTTON_PRESS && e->button == 1) { PRESS = hit_at(e->x, e->y); bar_redraw(); } return TRUE; }
static gboolean bar_release(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)w; (void)d; if (e->button != 1) return TRUE; int h = hit_at(e->x, e->y); int was = PRESS; PRESS = -1; bar_redraw();
	if (h >= 0 && h == was) bar_click(h);
	return TRUE;
}
static gboolean bar_motion(GtkWidget *w, GdkEventMotion *e, gpointer d) { (void)w; (void)d; int h = hit_at(e->x, e->y); if (h != HOVER) { HOVER = h; bar_redraw(); } return FALSE; }
static gboolean bar_leave(GtkWidget *w, GdkEventCrossing *e, gpointer d) { (void)w; (void)e; (void)d; if (HOVER != -1) { HOVER = -1; bar_redraw(); } return FALSE; }
static void bar_place(void)
{
	if (!BAR) return; int top = !strcmp(S.tb, "top");
	gtk_widget_set_size_request(BAR, SW, BARH); gtk_window_move(GTK_WINDOW(BAR), 0, top ? TOPRES : SH - BARH); gtk_window_resize(GTK_WINDOW(BAR), SW, BARH);
}

/* ------------------------------------------------------------------ the start menu */
static GtkWidget *MSEARCH, *MLIST, *MHEAD, *MBTN[4]; static GtkCssProvider *CSS;
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
static void menu_hide(void) { if (MENU) gtk_widget_hide(MENU); }
static void on_row(GtkListBox *b, GtkListBoxRow *r, gpointer d) { (void)b; (void)d; const char *k = g_object_get_data(G_OBJECT(r), "k"); if (k) { char kk[40]; snprintf(kk, sizeof kk, "%s", k); menu_hide(); app_open(kk, NULL); } }
static gboolean menu_filter(GtkListBoxRow *r, gpointer d)
{
	(void)d; const char *q = gtk_entry_get_text(GTK_ENTRY(MSEARCH)); if (!q || !*q) return TRUE;
	const char *h = g_object_get_data(G_OBJECT(r), "hay"); char *lq = g_utf8_strdown(q, -1); int m = h && strstr(h, lq); g_free(lq); return m;
}
static void on_search(GtkSearchEntry *e, gpointer d) { (void)e; (void)d; gtk_list_box_invalidate_filter(GTK_LIST_BOX(MLIST)); }
static void on_search_go(GtkEntry *e, gpointer d)
{
	(void)e; (void)d; GList *k = gtk_container_get_children(GTK_CONTAINER(MLIST));
	for (GList *l = k; l; l = l->next) if (gtk_widget_get_child_visible(l->data) && menu_filter(l->data, NULL)) { on_row(NULL, l->data, NULL); break; }
	g_list_free(k);
}
static void menu_fill(void)
{
	if (!MENU) return;
	GList *k = gtk_container_get_children(GTK_CONTAINER(MLIST)); for (GList *l = k; l; l = l->next) gtk_widget_destroy(l->data); g_list_free(k);
	for (int i = 0; i < NAPPS; i++) {
		/* the app's picture as the page draws it, else its emoji */
		GdkPixbuf *px = APPS[i].ic[0] && ICONS ? g_hash_table_lookup(ICONS, APPS[i].ic) : NULL; char lab[200];
		if (px) snprintf(lab, sizeof lab, "%s", APPS[i].name); else snprintf(lab, sizeof lab, "%s   %s", APPS[i].icon, APPS[i].name);
		GtkWidget *row = gtk_list_box_row_new(), *hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0), *l = gtk_label_new(lab); gtk_label_set_xalign(GTK_LABEL(l), 0);
		if (px) { int sz = (int)lround(22 * Z); GdkPixbuf *sm = gdk_pixbuf_scale_simple(px, sz, sz, GDK_INTERP_BILINEAR); GtkWidget *im = gtk_image_new_from_pixbuf(sm); g_object_unref(sm);
			gtk_widget_set_margin_start(im, 10); gtk_box_pack_start(GTK_BOX(hb), im, FALSE, FALSE, 0); }
		gtk_box_pack_start(GTK_BOX(hb), l, TRUE, TRUE, 0); gtk_container_add(GTK_CONTAINER(row), hb);
		g_object_set_data_full(G_OBJECT(row), "k", g_strdup(APPS[i].k), g_free);
		char *hay = g_strdup_printf("%s %s %s", APPS[i].name, APPS[i].k, APPS[i].kw), *lh = g_utf8_strdown(hay, -1); g_free(hay);
		g_object_set_data_full(G_OBJECT(row), "hay", lh, g_free);
		gtk_container_add(GTK_CONTAINER(MLIST), row);
	}
	gtk_widget_show_all(MLIST);
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
static void on_power(GtkButton *b, gpointer d) { (void)b; power(d); }
static gboolean on_menu_key(GtkWidget *w, GdkEventKey *e, gpointer d) { (void)w; (void)d; if (e->keyval == GDK_KEY_Escape) { menu_hide(); return TRUE; } return FALSE; }
static gboolean on_menu_out(GtkWidget *w, GdkEvent *e, gpointer d) { (void)w; (void)e; (void)d; menu_hide(); return FALSE; }
static void menu_make(void)
{
	MENU = gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_window_set_decorated(GTK_WINDOW(MENU), FALSE);
	gtk_window_set_type_hint(GTK_WINDOW(MENU), GDK_WINDOW_TYPE_HINT_DIALOG); gtk_window_set_keep_above(GTK_WINDOW(MENU), TRUE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(MENU), TRUE); gtk_window_set_default_size(GTK_WINDOW(MENU), 380, 520); gtk_window_set_title(GTK_WINDOW(MENU), "Start");
	gtk_style_context_add_class(gtk_widget_get_style_context(MENU), "rn-menu");
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); gtk_container_add(GTK_CONTAINER(MENU), box);
	MHEAD = gtk_label_new(""); gtk_label_set_xalign(GTK_LABEL(MHEAD), 0); gtk_style_context_add_class(gtk_widget_get_style_context(MHEAD), "rn-head"); gtk_box_pack_start(GTK_BOX(box), MHEAD, FALSE, FALSE, 0);
	MSEARCH = gtk_search_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(MSEARCH), "Search apps"); gtk_box_pack_start(GTK_BOX(box), MSEARCH, FALSE, FALSE, 0);
	g_signal_connect(MSEARCH, "search-changed", G_CALLBACK(on_search), NULL); g_signal_connect(MSEARCH, "activate", G_CALLBACK(on_search_go), NULL);
	GtkWidget *sc = gtk_scrolled_window_new(NULL, NULL); gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	MLIST = gtk_list_box_new(); gtk_list_box_set_filter_func(GTK_LIST_BOX(MLIST), menu_filter, NULL, NULL); gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(MLIST), TRUE);
	g_signal_connect(MLIST, "row-activated", G_CALLBACK(on_row), NULL);
	gtk_container_add(GTK_CONTAINER(sc), MLIST); gtk_box_pack_start(GTK_BOX(box), sc, TRUE, TRUE, 0);
	GtkWidget *pw = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0); gtk_style_context_add_class(gtk_widget_get_style_context(pw), "rn-power"); gtk_box_set_homogeneous(GTK_BOX(pw), TRUE);
	static const char *const P[][2] = {{"Sign out", "logout"}, {"Lock", "lock"}, {"Restart", "reboot"}, {"Shut down", "poweroff"}};
	for (int i = 0; i < 4; i++) { GtkWidget *b = MBTN[i] = gtk_button_new_with_label(P[i][0]); g_object_set_data(G_OBJECT(b), "what", (gpointer)P[i][1]); g_signal_connect(b, "clicked", G_CALLBACK(on_power), (gpointer)P[i][1]); gtk_box_pack_start(GTK_BOX(pw), b, TRUE, TRUE, 0); }
	gtk_box_pack_start(GTK_BOX(box), pw, FALSE, FALSE, 0);
	g_signal_connect(MENU, "key-press-event", G_CALLBACK(on_menu_key), NULL);
	g_signal_connect(MENU, "focus-out-event", G_CALLBACK(on_menu_out), NULL);
	g_signal_connect(MENU, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	menu_fill();
}
static void menu_toggle(void)
{
	if (!MENU) menu_make();
	if (gtk_widget_get_visible(MENU)) { menu_hide(); return; }
	char h[200]; snprintf(h, sizeof h, "👤 %s%s", USER_N, !strcmp(USER_R, "admin") ? " ★" : ""); gtk_label_set_text(GTK_LABEL(MHEAD), h);
	gtk_entry_set_text(GTK_ENTRY(MSEARCH), ""); int top = !strcmp(S.tb, "top"), mw = (int)lround(380 * Z), mh = (int)lround(520 * Z);
	if (mh > SH - BARH - TOPRES - 4) mh = SH - BARH - TOPRES - 4;
	gtk_window_resize(GTK_WINDOW(MENU), mw, mh); gtk_window_move(GTK_WINDOW(MENU), 3, top ? TOPRES + BARH + 2 : SH - BARH - mh - 2);
	gtk_widget_show_all(MENU); gtk_window_present(GTK_WINDOW(MENU)); gtk_widget_grab_focus(MSEARCH); wm_restack();
	GdkWindow *gw = gtk_widget_get_window(MENU); if (gw) gdk_window_focus(gw, GDK_CURRENT_TIME);
	state_write();
}

/* ------------------------------------------------------------------ the sign-in and lock screens */
static GtkWidget *SAREA, *SNAME, *SPASS, *SMSG, *SGO, *SGUEST, *STITLE, *SACCTS;
static int SIGN_LOCK, SIGN_BUSY;
static void settings_load(void);
static void apps_load(void);
static gboolean signin_area_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)d; desk_draw(w, cr, NULL); cairo_set_source_rgba(cr, 0, 0, 0, .35); cairo_paint(cr); return TRUE; }
typedef struct { char *body, *path; int code; char *reply; } Job;
static gboolean signin_done(gpointer p)
{
	Job *j = p; SIGN_BUSY = 0; gtk_widget_set_sensitive(SGO, TRUE);
	JV *v = jparse(j->reply);
	if (j->code == 200) {
		if (!SIGN_LOCK) { jstr(v, "id", USER_ID, sizeof USER_ID); jstr(v, "n", USER_N, sizeof USER_N); jstr(v, "r", USER_R, sizeof USER_R); }
		LOCKED = 0; gtk_widget_hide(SIGN); gtk_entry_set_text(GTK_ENTRY(SPASS), ""); gtk_label_set_text(GTK_LABEL(SMSG), "");
		settings_load(); apps_load(); if (BAR) gtk_widget_show_all(BAR); wm_restack();
		if (WM_OK) wm_hide_all(0); else for (GList *l = WINS; l; l = l->next) gtk_widget_show(((AppWin *)l->data)->win);
	} else {
		char m[300] = ""; jstr(v, "error", m, sizeof m);
		gtk_label_set_text(GTK_LABEL(SMSG), m[0] ? m : j->code ? "The sign-in did not work." : "Rudolf OS's system service did not answer.");
		gtk_entry_set_text(GTK_ENTRY(SPASS), ""); gtk_widget_grab_focus(SPASS);
	}
	jfree(v);
	state_write();
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
	if (SIGN_BUSY) return; SIGN_BUSY = 1; gtk_widget_set_sensitive(SGO, FALSE); gtk_label_set_text(GTK_LABEL(SMSG), "");
	Job *j = g_new0(Job, 1); GString *b = g_string_new("{");
	if (guest) g_string_append(b, "\"guest\":true");
	else { if (!SIGN_LOCK) { g_string_append(b, "\"n\":"); json_esc(b, gtk_entry_get_text(GTK_ENTRY(SNAME))); g_string_append_c(b, ','); }
		g_string_append(b, "\"p\":"); json_esc(b, gtk_entry_get_text(GTK_ENTRY(SPASS))); }
	g_string_append_c(b, '}'); j->body = g_string_free(b, FALSE); j->path = g_strdup(SIGN_LOCK ? "/api/desktop/unlock" : "/api/desktop/login");
	g_thread_unref(g_thread_new("signin", signin_thread, j));
}
static void on_sign_go(GtkWidget *w, gpointer d) { (void)w; signin_go(GPOINTER_TO_INT(d)); }
static void on_acct_pick(GtkButton *b, gpointer d) { (void)d; gtk_entry_set_text(GTK_ENTRY(SNAME), gtk_button_get_label(b)); gtk_widget_grab_focus(SPASS); }
static void on_spower(GtkButton *b, gpointer d) { (void)b; char p[80]; snprintf(p, sizeof p, "/api/power?action=%s", (char *)d); http("POST", p, NULL, 0, NULL); }
static void signin_make(void)
{
	SIGN = gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_window_set_decorated(GTK_WINDOW(SIGN), FALSE); gtk_window_set_title(GTK_WINDOW(SIGN), "Sign in");
	gtk_window_set_keep_above(GTK_WINDOW(SIGN), TRUE); gtk_window_set_default_size(GTK_WINDOW(SIGN), SW, SH); gtk_window_move(GTK_WINDOW(SIGN), 0, 0);
	GtkWidget *ov = gtk_overlay_new(); gtk_container_add(GTK_CONTAINER(SIGN), ov);
	SAREA = gtk_drawing_area_new(); g_signal_connect(SAREA, "draw", G_CALLBACK(signin_area_draw), NULL); gtk_container_add(GTK_CONTAINER(ov), SAREA);
	GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10); gtk_style_context_add_class(gtk_widget_get_style_context(card), "rn-card");
	gtk_widget_set_halign(card, GTK_ALIGN_CENTER); gtk_widget_set_valign(card, GTK_ALIGN_CENTER); gtk_overlay_add_overlay(GTK_OVERLAY(ov), card);
	STITLE = gtk_label_new("Rudolf OS"); gtk_style_context_add_class(gtk_widget_get_style_context(STITLE), "rn-big"); gtk_box_pack_start(GTK_BOX(card), STITLE, FALSE, FALSE, 0);
	SACCTS = gtk_flow_box_new(); gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(SACCTS), GTK_SELECTION_NONE); gtk_box_pack_start(GTK_BOX(card), SACCTS, FALSE, FALSE, 0);
	SNAME = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(SNAME), "User name"); gtk_box_pack_start(GTK_BOX(card), SNAME, FALSE, FALSE, 0);
	SPASS = gtk_entry_new(); gtk_entry_set_visibility(GTK_ENTRY(SPASS), FALSE); gtk_entry_set_placeholder_text(GTK_ENTRY(SPASS), "Password"); gtk_box_pack_start(GTK_BOX(card), SPASS, FALSE, FALSE, 0);
	g_signal_connect(SNAME, "activate", G_CALLBACK(on_sign_go), GINT_TO_POINTER(0)); g_signal_connect(SPASS, "activate", G_CALLBACK(on_sign_go), GINT_TO_POINTER(0));
	SMSG = gtk_label_new(""); gtk_label_set_line_wrap(GTK_LABEL(SMSG), TRUE); gtk_label_set_max_width_chars(GTK_LABEL(SMSG), 40); gtk_style_context_add_class(gtk_widget_get_style_context(SMSG), "rn-msg"); gtk_box_pack_start(GTK_BOX(card), SMSG, FALSE, FALSE, 0);
	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	SGO = gtk_button_new_with_label("Sign in"); gtk_style_context_add_class(gtk_widget_get_style_context(SGO), "rn-go"); g_signal_connect(SGO, "clicked", G_CALLBACK(on_sign_go), GINT_TO_POINTER(0));
	SGUEST = gtk_button_new_with_label("Guest"); g_signal_connect(SGUEST, "clicked", G_CALLBACK(on_sign_go), GINT_TO_POINTER(1));
	gtk_box_pack_end(GTK_BOX(row), SGO, FALSE, FALSE, 0); gtk_box_pack_end(GTK_BOX(row), SGUEST, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(card), row, FALSE, FALSE, 0);
	GtkWidget *pr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8); gtk_widget_set_halign(pr, GTK_ALIGN_END); gtk_widget_set_valign(pr, GTK_ALIGN_END); gtk_widget_set_margin_end(pr, 16); gtk_widget_set_margin_bottom(pr, 16);
	GtkWidget *rs = gtk_button_new_with_label("Restart"), *sd = gtk_button_new_with_label("Shut down");
	g_signal_connect(rs, "clicked", G_CALLBACK(on_spower), "reboot"); g_signal_connect(sd, "clicked", G_CALLBACK(on_spower), "poweroff");
	gtk_box_pack_start(GTK_BOX(pr), rs, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(pr), sd, FALSE, FALSE, 0);
	GtkWidget *pc = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0); gtk_style_context_add_class(gtk_widget_get_style_context(pc), "rn-card"); gtk_container_add(GTK_CONTAINER(pc), pr);
	gtk_widget_set_halign(pc, GTK_ALIGN_END); gtk_widget_set_valign(pc, GTK_ALIGN_END); gtk_overlay_add_overlay(GTK_OVERLAY(ov), pc);
	g_signal_connect(SIGN, "delete-event", G_CALLBACK(gtk_true), NULL);
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
	SIGN_LOCK = lock; LOCKED = lock; menu_hide();
	if (lock) { if (WM_OK) wm_hide_all(1); else for (GList *l = WINS; l; l = l->next) gtk_widget_hide(((AppWin *)l->data)->win); }
	if (!lock) { USER_ID[0] = 0; settings_load(); }
	accounts_load();
	GList *k = gtk_container_get_children(GTK_CONTAINER(SACCTS)); for (GList *l = k; l; l = l->next) gtk_widget_destroy(l->data); g_list_free(k);
	if (!lock) for (int i = 0; i < NACC && NACC > 1; i++) { GtkWidget *b = gtk_button_new_with_label(ACC[i].n); g_signal_connect(b, "clicked", G_CALLBACK(on_acct_pick), NULL); gtk_container_add(GTK_CONTAINER(SACCTS), b); }
	const char *pre = ""; for (int i = 0; i < NACC; i++) if (!strcmp(ACC[i].id, LASTACC)) pre = ACC[i].n;
	gtk_entry_set_text(GTK_ENTRY(SNAME), lock ? USER_N : pre); gtk_entry_set_text(GTK_ENTRY(SPASS), "");
	char t[200]; snprintf(t, sizeof t, lock ? "🔒 %s" : "Rudolf OS", USER_N); gtk_label_set_text(GTK_LABEL(STITLE), t);
	gtk_label_set_text(GTK_LABEL(SMSG), SETUP || lock ? "" : "Rudolf OS is not set up yet: start it once with the Standard desktop engine.");
	gtk_widget_show_all(SIGN); gtk_widget_set_visible(SNAME, !lock); gtk_widget_set_visible(SGUEST, !lock && GUEST_OK); gtk_widget_set_visible(SACCTS, !lock && NACC > 1);
	gtk_window_move(GTK_WINDOW(SIGN), 0, 0); gtk_window_present(GTK_WINDOW(SIGN));
	GdkWindow *gw = gtk_widget_get_window(SIGN); if (gw) gdk_window_focus(gw, GDK_CURRENT_TIME);
	gtk_widget_grab_focus(lock || *pre ? SPASS : SNAME);
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
	jstr(s, "accent", n.accent, sizeof n.accent); jstr(s, "df", n.df, sizeof n.df); jstr(s, "tz", n.tz, sizeof n.tz);
	char wp[64] = ""; jstr(s, "wpimg", wp, sizeof wp); n.wpimg = wp[0] != 0;
	jstr(s, "icons", n.icons, sizeof n.icons); n.scale = (int)jnum(s, "scale", 100); if (n.scale < 75 || n.scale > 250) n.scale = 100;
	n.tbac = jbool(s, "tbac"); n.tbbac = jbool(s, "tbbac"); n.h24 = jbool(s, "h24"); n.doff = jnum(s, "doff", 0); n.lat = jnum(s, "lat", NAN);
	if (!n.bs[0]) snprintf(n.bs, sizeof n.bs, "solid"); if (!n.b1[0]) snprintf(n.b1, sizeof n.b1, "#008080"); if (!n.b2[0]) snprintf(n.b2, sizeof n.b2, "#000080");
	if (!n.tb[0]) snprintf(n.tb, sizeof n.tb, "bottom"); if (!n.wpfit[0]) snprintf(n.wpfit, sizeof n.wpfit, "fill"); if (!n.dm[0]) snprintf(n.dm, sizeof n.dm, "auto");
	jstr(v, "acct", n.acct, sizeof n.acct); jstr(v, "wpacct", n.wpacct, sizeof n.wpacct); n.gen = jnum(v, "gen", 0); n.notes = (int)jnum(v, "notes", 0); n.wallpaper = jbool(v, "wallpaper");
	JV *p = jprop(v, "pins"); for (int i = 0; i < 24; i++) { JV *o = jat(p, i); if (!o) break; if (o->t == 3) snprintf(n.pins[n.npins++], 40, "%s", o->s);  }
	  jfree(v);
	int moved = strcmp(n.tb, S.tb) != 0 || n.scale != S.scale; S = n;
	/* the page's display scaling: the taskbar, the title bars and the start menu at the page's size (1.5.9 step 7) */
	Z = S.scale / 100.0; BARH = (int)lround(46 * Z); TH = (int)lround(30 * Z);
	css_apply(); wall_make(); if (moved) { bar_place(); if (WM_OK) wm_relayout(); } bar_redraw(); if (SAREA) gtk_widget_queue_draw(SAREA); if (WM_OK) frames_redraw();
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
	free(APPS); APPS = L; NAPPS = n;
	if (BG) g_hash_table_destroy(BG); BG = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	JV *bg = jprop(v, "bg");
	if (bg && bg->t == 5) {
		for (int i = 0; i < bg->len; i++) { JV *o = bg->v[i]; const char *kk = bg->k[i]; char d[200] = "", l[200] = ""; jstr(o, "d", d, sizeof d); jstr(o, "l", l, sizeof l);
			if (d[0] && !strstr(d, "..") && !strstr(l, "..")) g_hash_table_insert(BG, g_strdup(kk), g_strdup_printf("%s\n%s", d, l[0] ? l : d));  }
		}
	 jfree(v);
	menu_fill(); WALL_KEY[0] = 0; wall_make(); bar_redraw();
}
static void tray_load(void)
{
	JV *v = jget("/api/net"); int up = 0, wifi = 0;
	JV *ifs = jprop(v, "interfaces");
	for (int i = 0; i < 16; i++) { JV *o = jat(ifs, i); if (!o) break; char nm[32] = "", ip[64] = "", st[16] = ""; jstr(o, "name", nm, sizeof nm); jstr(o, "ip", ip, sizeof ip); jstr(o, "state", st, sizeof st);
		if (strcmp(nm, "lo") && ip[0] && strcmp(st, "down")) { up = 1; if (jbool(o, "wireless")) wifi = 1; }  }
	 NET_UP = v ? up : -1; NET_WIFI = wifi; jfree(v);
	v = jget("/api/volume"); if (v && jbool(v, "ok")) { VOL = (int)jnum(v, "volume", -1); MUTED = jbool(v, "muted"); } else VOL = -1; jfree(v);
	v = jget("/api/info"); BAT = -1; JV *b = jprop(v, "battery");
	for (int i = 0; i < 4; i++) { JV *o = jat(b, i); if (!o) break; char t[24] = "", st[24] = ""; jstr(o, "type", t, sizeof t); jstr(o, "status", st, sizeof st);
		if (!strcmp(t, "Battery")) { BAT = (int)jnum(o, "capacity", -1); CHARGING = !strcmp(st, "Charging") || !strcmp(st, "Full"); }  }
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
static gboolean tick_clock(gpointer d) { (void)d; char o[32]; snprintf(o, sizeof o, "%s", CLOCK_T); clock_text(); if (strcmp(o, CLOCK_T)) bar_redraw(); if (S.dyn[0] && !S.wpimg) wall_make(); return G_SOURCE_CONTINUE; }
/* the session's watchdog and trial need to hear from the desktop, as the page's heartbeat */
static gboolean tick_heart(gpointer d) { (void)d; http("POST", "/api/heartbeat", NULL, 0, NULL); return G_SOURCE_CONTINUE; }

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
	for (int i = 0; i < 4 && MENU && gtk_widget_get_visible(MENU); i++) { int wx, wy, x, y; GtkAllocation al; GdkWindow *g = gtk_widget_get_window(MENU); if (!g || !MBTN[i]) continue;
		gdk_window_get_origin(g, &wx, &wy); gtk_widget_get_allocation(MBTN[i], &al); gtk_widget_translate_coordinates(MBTN[i], MENU, 0, 0, &x, &y);
		g_string_append_printf(s, "menubtn %s %d %d %d %d\n", (char *)g_object_get_data(G_OBJECT(MBTN[i]), "what"), wx + x, wy + y, al.width, al.height); }
	if (SMSG) g_string_append_printf(s, "signmsg %s\n", gtk_label_get_text(GTK_LABEL(SMSG)));
	for (GList *l = WINS; l; l = l->next) { AppWin *a = l->data; GdkWindow *g = a->win ? gtk_widget_get_window(a->win) : NULL;
		Client *c = WM_OK ? client_of_app(a) : NULL;
		g_string_append_printf(s, "app %s %lx %d %s\n", a->k, g ? (unsigned long)GDK_WINDOW_XID(g) : 0UL, c ? c == FOCUS : a == ACTIVE, a->title); }
	for (GList *l = CLIENTS; l; l = l->next) { Client *c = l->data;
		g_string_append_printf(s, "client %lx %lx %d %d %d %d min=%d max=%d full=%d focus=%d app=%s %s\n", (unsigned long)c->w, (unsigned long)c->frame, c->x, c->y, c->cw, c->ch,
			c->min, c->max, c->full, c == FOCUS, c->app ? c->app->k : "-", c->title); }
	g_string_append_printf(s, "wm %d\nicons %u %s\n", WM_OK, ICONS ? g_hash_table_size(ICONS) : 0, VER[0] ? VER : "-");
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
	DESKAREA = gtk_drawing_area_new(); g_signal_connect(DESKAREA, "draw", G_CALLBACK(desk_draw), NULL); gtk_container_add(GTK_CONTAINER(DESK), DESKAREA);
	g_signal_connect(DESK, "delete-event", G_CALLBACK(gtk_true), NULL);
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
