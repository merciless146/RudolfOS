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
#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <math.h>
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
static int PORT = 8080, SW, SH, BARH = 46;
static WebKitHardwareAccelerationPolicy HWPOL = WEBKIT_HARDWARE_ACCELERATION_POLICY_ON_DEMAND;
static int SOFT;                              /* --draw=soft: the lighter effects, as rudolf-webkit */
static const char *STATEF;                    /* RUDOLF_NATIVE_STATE: what is where, for the tests */
typedef struct {
	char bs[16], b1[16], b2[16], dyn[48], dm[8], wpfit[12], tb[8], tbstyle[12], tbacm[12], tbbstyle[12], tbbacm[12];
	char twstyle[12], twacm[12], accent[16], df[8], tz[64], acct[80];
	int tbac, tbbac, h24, wpimg, wallpaper, notes, npins; double lat, doff, gen; char pins[24][40];
} Settings;
static Settings S;
typedef struct { char k[40], name[80], icon[48], kw[200]; } App;
static App *APPS; static int NAPPS; static GHashTable *BG;   /* key -> "dark\nlight" */
typedef struct { char id[80], n[128], r[12]; int key, old; } Acct;
static Acct ACC[32]; static int NACC, GUEST_OK, SETUP; static char LASTACC[80];
static char USER_ID[80], USER_N[128], USER_R[12];   /* signed in ("" = not) */
static int LOCKED;
/* the tray */
static int NET_UP = -1, NET_WIFI, VOL = -1, MUTED, BAT = -1, CHARGING;

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
static GtkWidget *DESK, *DESKAREA, *BAR, *BARAREA;
static cairo_surface_t *WALL;   /* drawn at the screen's size */
static char WALL_KEY[300];
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
	if (S.wpimg && S.wallpaper && S.acct[0]) snprintf(path, sizeof path, "/api/desktop/wallpaper?acct=%s", S.acct);
	else if (S.dyn[0] && BG) { const char *v = g_hash_table_lookup(BG, S.dyn); if (v) { const char *nl = strchr(v, '\n'); if (dark || !nl) snprintf(path, sizeof path, "/%.*s", nl ? (int)(nl - v) : (int)strlen(v), v); else snprintf(path, sizeof path, "/%s", nl + 1); } }
	snprintf(key, sizeof key, "%s|%s|%s|%s|%s|%s|%d|%d", path, S.bs, S.b1, S.b2, S.wpfit, S.acct, SW, (int)S.gen);
	if (WALL && !strcmp(key, WALL_KEY)) return;
	snprintf(WALL_KEY, sizeof WALL_KEY, "%s", key);
	if (!WALL) WALL = cairo_image_surface_create(CAIRO_FORMAT_RGB24, SW, SH);
	cairo_t *cr = cairo_create(WALL); GdkPixbuf *p = path[0] ? pix_get(path) : NULL;
	if (p) {   /* behind a picture: the first colour (a picture of your own), black or light grey (a drawn one) */
		RGB bg = S.wpimg ? hexc(S.b1, "#000000") : dark ? (RGB){0, 0, 0} : hexc("#e5e7eb", NULL); src(cr, bg, 1); cairo_paint(cr);
		paint_pix(cr, p, SW, SH, S.wpimg ? S.wpfit : "fill"); g_object_unref(p);
	} else pattern_bg(cr, SW, SH);
	cairo_destroy(cr);
	if (DESKAREA) gtk_widget_queue_draw(DESKAREA);
}
static gboolean desk_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)w; (void)d; if (WALL) { cairo_set_source_surface(cr, WALL, 0, 0); cairo_paint(cr); } return TRUE; }

/* ------------------------------------------------------------------ app windows (WebKit, one-app mode) */
typedef struct AppWin { GtkWidget *win, *view; char k[40], title[256], icon[48]; int closing; gint64 close_t; } AppWin;
static GList *WINS;   /* AppWin*, oldest first */
static AppWin *ACTIVE;
static WebKitWebContext *CTX;
static void bar_redraw(void);
static void app_open(const char *k, const char *file);
static const App *app_find(const char *k) { for (int i = 0; i < NAPPS; i++) if (!strcmp(APPS[i].k, k)) return &APPS[i]; return NULL; }
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
static void appwin_close_now(AppWin *a) { if (a->win) gtk_widget_destroy(a->win); }
static void on_title(GObject *o, GParamSpec *p, gpointer d)
{
	(void)o; (void)p; AppWin *a = d; const char *t = webkit_web_view_get_title(WEBKIT_WEB_VIEW(a->view));
	if (t && *t) { snprintf(a->title, sizeof a->title, "%s", t); gtk_window_set_title(GTK_WINDOW(a->win), t); bar_redraw(); state_write(); }
}
static void on_view_close(WebKitWebView *v, gpointer d) { (void)v; appwin_close_now(d); }
static void on_msg(WebKitUserContentManager *m, WebKitJavascriptResult *r, gpointer d)
{
	(void)m; AppWin *a = d; JSCValue *v = webkit_javascript_result_get_js_value(r); if (!jsc_value_is_object(v)) return;
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
static AppWin *appwin_new(const char *k, WebKitWebView *rel)
{
	AppWin *a = g_new0(AppWin, 1); snprintf(a->k, sizeof a->k, "%s", k);
	const App *ap = app_find(k); snprintf(a->title, sizeof a->title, "%s", ap ? ap->name : k); snprintf(a->icon, sizeof a->icon, "%s", ap ? ap->icon : "");
	WebKitUserContentManager *ucm = webkit_user_content_manager_new();
	webkit_user_content_manager_register_script_message_handler(ucm, "rudolfNative");
	g_signal_connect(ucm, "script-message-received::rudolfNative", G_CALLBACK(on_msg), a);
	if (SOFT) webkit_user_content_manager_add_style_sheet(ucm, webkit_user_style_sheet_new(
		"*,*::before,*::after{box-shadow:none!important;text-shadow:none!important;backdrop-filter:none!important}", WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES, WEBKIT_USER_STYLE_LEVEL_USER, NULL, NULL));
	a->view = rel ? GTK_WIDGET(g_object_new(WEBKIT_TYPE_WEB_VIEW, "related-view", rel, "user-content-manager", ucm, NULL))
		: GTK_WIDGET(g_object_new(WEBKIT_TYPE_WEB_VIEW, "web-context", CTX, "user-content-manager", ucm, NULL));
	g_object_unref(ucm);
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
	int n = g_list_length(WINS), w = SW * 7 / 10, h = (SH - BARH) * 8 / 10; if (w < 640) w = SW; if (h < 400) h = SH - BARH;
	gtk_window_set_default_size(GTK_WINDOW(a->win), w, h);
	gtk_window_move(GTK_WINDOW(a->win), 40 + (n % 8) * 28, (strcmp(S.tb, "top") ? 20 : BARH + 20) + (n % 8) * 28);
	gtk_container_add(GTK_CONTAINER(a->win), a->view);
	g_signal_connect(a->win, "destroy", G_CALLBACK(on_win_destroy), a);
	g_signal_connect(a->win, "delete-event", G_CALLBACK(on_delete), a);
	g_signal_connect(a->win, "focus-in-event", G_CALLBACK(on_win_focus), a);
	WINS = g_list_append(WINS, a); ACTIVE = a;
	return a;
}
static void appwin_show(AppWin *a) { gtk_widget_show_all(a->win); gtk_window_present(GTK_WINDOW(a->win)); bar_redraw(); state_write(); }
static void on_ready(WebKitWebView *v, gpointer d) { (void)v; appwin_show(d); }
static char *app_url(const char *k, const char *file)
{
	char *fe = file ? g_uri_escape_string(file, NULL, FALSE) : NULL;
	char *u = g_strdup_printf("http://%s:%d/#app=%s%s%s", HOST, PORT, k, fe ? "&file=" : "", fe ? fe : ""); g_free(fe); return u;
}
/* windows asked for and not made yet (made through an open window's window.open: see app_open) */
typedef struct { char k[40]; } Pending;
static GQueue PENDING = G_QUEUE_INIT;
static GtkWidget *on_create(WebKitWebView *v, WebKitNavigationAction *na, gpointer d)
{
	(void)na; (void)d; Pending *p = g_queue_pop_head(&PENDING);
	if (!p) return NULL;   /* (a page's own window.open: the desktop page sends websites to the browser itself) */
	AppWin *a = appwin_new(p->k, v); g_free(p);
	g_signal_connect(a->view, "ready-to-show", G_CALLBACK(on_ready), a);
	return a->view;
}
/* an app in a window of its own. All app windows share one WebKit web process: the first starts it, the others are
   opened by an open one (window.open: the page's own, webNativeOpen, as the page sends websites elsewhere), which WebKit keeps in its process (a view merely made "related" to another
   got a process of its own, WebKitGTK 2.52, as navigations may change process) */
static void app_open(const char *k, const char *file)
{
	if (!USER_ID[0] || LOCKED || strspn(k, "abcdefghijklmnopqrstuvwxyz0123456789_-") != strlen(k) || strlen(k) > 39) return;
	ctx_make(); char *u = app_url(k, file);
	AppWin *rel = NULL; for (GList *l = WINS; l; l = l->next) { AppWin *x = l->data; if (x->view && webkit_web_view_get_uri(WEBKIT_WEB_VIEW(x->view)) && !x->closing) { rel = x; break; } }
	if (rel) {
		Pending *p = g_new0(Pending, 1); snprintf(p->k, sizeof p->k, "%s", k); g_queue_push_tail(&PENDING, p);
		char *js = g_strdup_printf("(typeof webNativeOpen=='function'?webNativeOpen:window.open)(%s,'_blank')", "decodeURIComponent('%s')"), *ue = g_uri_escape_string(u, NULL, FALSE), *js2 = g_strdup_printf(js, ue);
		webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(rel->view), js2, -1, NULL, NULL, NULL, NULL, NULL);
		g_free(js); g_free(js2); g_free(ue); g_free(u); return;
	}
	AppWin *a = appwin_new(k, NULL); webkit_web_view_load_uri(WEBKIT_WEB_VIEW(a->view), u); g_free(u);
	appwin_show(a);
}
static void appwin_activate(AppWin *a)
{
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
static void hit_add(int x, int y, int w, int h, const char *k, const char *a) { if (NHITS >= 96) return; Hit *t = &HITS[NHITS++]; t->x = x; t->y = y; t->w = w; t->h = h; snprintf(t->kind, sizeof t->kind, "%s", k); snprintf(t->arg, sizeof t->arg, "%s", a ? a : ""); }
static const char *pin_icon(const char *k) { const App *a = app_find(k); return a && a->icon[0] ? a->icon : "▫"; }
static void tray_text(int which, char *o, size_t n)
{
	if (which == 0) snprintf(o, n, "%s", NET_UP < 0 ? "🌐" : !NET_UP ? "✈" : NET_WIFI ? "📶" : "🖧");
	else if (which == 1) { if (VOL < 0) snprintf(o, n, "🔊"); else snprintf(o, n, "%s %d%%", MUTED || !VOL ? "🔇" : "🔊", VOL); }
	else if (which == 2) snprintf(o, n, "%s %d%%", CHARGING ? "⚡" : "🔋", BAT);
	else snprintf(o, n, S.notes ? "🔔 %d" : "🔔", S.notes);
}
static gboolean bar_draw(GtkWidget *wd, cairo_t *cr, gpointer d)
{
	(void)d; int W = gtk_widget_get_allocated_width(wd), H = gtk_widget_get_allocated_height(wd), top = !strcmp(S.tb, "top");
	Look B = bar_look(), K = btn_look(); int plain = !btn_own() && !strcmp(B.st, "standard") && !S.tbac;
	RGB fg = btn_own() ? K.ink : B.ink;
	bar_fill(cr, &B, W, H, top); NHITS = 0;
	int x = 3, by = (H - 34) / 2, i = 0;
	/* the start button: the logo and the name */
	hit_add(x, by, 139, 34, "start", NULL);
	face(cr, &K, x, by, 139, 34, HOVER == i, PRESS == i, plain); i++;
	if (LOGO) { cairo_save(cr); cairo_translate(cr, x + 8, by + 6); double s = 22.0 / gdk_pixbuf_get_width(LOGO); cairo_scale(cr, s, s);
		gdk_cairo_set_source_pixbuf(cr, LOGO, 0, 0); if (B.black && !btn_own()) { cairo_paint_with_alpha(cr, .25); } else cairo_paint(cr); cairo_restore(cr); }
	text_at(cr, "Rudolf OS", x + 36, by, 100, 34, "Sans Bold 10", fg, 0);
	x += 143;
	/* the pinned apps */
	for (int p = 0; p < S.npins; p++, i++) {
		hit_add(x, by, 34, 34, "pin", S.pins[p]);
		face(cr, &K, x, by, 34, 34, HOVER == i, PRESS == i, plain);
		text_at(cr, pin_icon(S.pins[p]), x, by, 34, 34, "Sans 15", fg, 1); x += 36;
	}
	x += 6;
	/* the tray and the clock, from the right */
	char t[64]; int rx = W - 3;
	clock_text(); int cw = 140; rx -= cw;
	hit_add(rx, by, cw, 34, "clock", NULL); face(cr, &K, rx, by, cw, 34, HOVER == NHITS - 1 + 0 && 0, 0, 0);
	if (plain) { rrect(cr, rx, by + 1, cw, 32, 5); src(cr, hexc("#1f2937", NULL), 1); cairo_fill(cr); }
	text_at(cr, CLOCK_T, rx + 8, by, 70, 34, "Sans 11", fg, 0); text_at(cr, CLOCK_D, rx + 70, by, cw - 76, 34, "Sans 8", fg, 2);
	int tray_i0 = NHITS;
	for (int k = 3; k >= 0; k--) {
		if (k == 2 && BAT < 0) continue;
		tray_text(k, t, sizeof t); int tw = k == 1 && VOL >= 0 ? 62 : k == 2 ? 64 : k == 3 && S.notes ? 48 : 34; rx -= tw + 4;
		static const char *const KIND[] = {"net", "vol", "bat", "notes"};
		hit_add(rx, by + 4, tw, 26, KIND[k], NULL);
		int me = i + (NHITS - 1 - tray_i0) + 64; face(cr, &K, rx, by + 4, tw, 26, HOVER == me, 0, 0);
		if (plain) { rrect(cr, rx, by + 4, tw, 26, 5); src(cr, hexc(HOVER == me ? "#374151" : "#1f2937", NULL), 1); cairo_fill(cr); }
		text_at(cr, t, rx, by + 4, tw, 26, "Sans 10", fg, 1);
	}
	/* the open windows, between */
	int avail = rx - 8 - x, n = g_list_length(WINS), tw = n ? fmin(200, avail / n) : 0, j = 0;
	for (GList *l = WINS; l && tw >= 40; l = l->next, j++) {
		AppWin *a = l->data; int wx = x + j * tw; char lab[320]; snprintf(lab, sizeof lab, "%s %s", a->icon, a->title);
		hit_add(wx, by, tw - 4, 34, "win", a->k); HITS[NHITS - 1].arg[0] = 0; snprintf(HITS[NHITS - 1].arg, sizeof HITS[0].arg, "%p", (void *)a);
		int me = 200 + j; face(cr, &K, wx, by, tw - 4, 34, HOVER == me, 0, 0);
		if (plain) { rrect(cr, wx, by, tw - 4, 34, 5); src(cr, hexc(HOVER == me ? "#374151" : "#1f2937", NULL), 1); cairo_fill(cr); }
		if (a == ACTIVE) { src(cr, accent(), 1); cairo_rectangle(cr, wx + 6, top ? by + 1 : by + 31, tw - 16, 3); cairo_fill(cr); }
		text_at(cr, lab, wx + 8, by, tw - 20, 34, "Sans 10", fg, 0);
	}
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
	else if (!strcmp(t->kind, "win")) { for (GList *l = WINS; l; l = l->next) { char p[40]; snprintf(p, sizeof p, "%p", l->data); if (!strcmp(p, t->arg)) { appwin_activate(l->data); return; } } }
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
	gtk_window_move(GTK_WINDOW(BAR), 0, top ? 0 : SH - BARH); gtk_window_resize(GTK_WINDOW(BAR), SW, BARH);
}

/* ------------------------------------------------------------------ the start menu */
static GtkWidget *MENU, *MSEARCH, *MLIST, *MHEAD, *MBTN[4]; static GtkCssProvider *CSS;
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
		".rn-card button.rn-go{background:%s}.rn-card entry{min-width:260px}", ac, ac);
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
		char lab[200]; snprintf(lab, sizeof lab, "%s   %s", APPS[i].icon, APPS[i].name);
		GtkWidget *row = gtk_list_box_row_new(), *l = gtk_label_new(lab); gtk_label_set_xalign(GTK_LABEL(l), 0); gtk_container_add(GTK_CONTAINER(row), l);
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
	gtk_entry_set_text(GTK_ENTRY(MSEARCH), ""); int top = !strcmp(S.tb, "top");
	gtk_window_move(GTK_WINDOW(MENU), 3, top ? BARH + 2 : SH - BARH - 522);
	gtk_widget_show_all(MENU); gtk_window_present(GTK_WINDOW(MENU)); gtk_widget_grab_focus(MSEARCH);
	GdkWindow *gw = gtk_widget_get_window(MENU); if (gw) gdk_window_focus(gw, GDK_CURRENT_TIME);
	state_write();
}

/* ------------------------------------------------------------------ the sign-in and lock screens */
static GtkWidget *SIGN, *SAREA, *SNAME, *SPASS, *SMSG, *SGO, *SGUEST, *STITLE, *SACCTS;
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
		settings_load(); apps_load(); if (BAR) gtk_widget_show_all(BAR);
		for (GList *l = WINS; l; l = l->next) gtk_widget_show(((AppWin *)l->data)->win);
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
	if (lock) for (GList *l = WINS; l; l = l->next) gtk_widget_hide(((AppWin *)l->data)->win);
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
	state_write();
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
	n.tbac = jbool(s, "tbac"); n.tbbac = jbool(s, "tbbac"); n.h24 = jbool(s, "h24"); n.doff = jnum(s, "doff", 0); n.lat = jnum(s, "lat", NAN);
	if (!n.bs[0]) snprintf(n.bs, sizeof n.bs, "solid"); if (!n.b1[0]) snprintf(n.b1, sizeof n.b1, "#008080"); if (!n.b2[0]) snprintf(n.b2, sizeof n.b2, "#000080");
	if (!n.tb[0]) snprintf(n.tb, sizeof n.tb, "bottom"); if (!n.wpfit[0]) snprintf(n.wpfit, sizeof n.wpfit, "fill"); if (!n.dm[0]) snprintf(n.dm, sizeof n.dm, "auto");
	jstr(v, "acct", n.acct, sizeof n.acct); n.gen = jnum(v, "gen", 0); n.notes = (int)jnum(v, "notes", 0); n.wallpaper = jbool(v, "wallpaper");
	JV *p = jprop(v, "pins"); for (int i = 0; i < 24; i++) { JV *o = jat(p, i); if (!o) break; if (o->t == 3) snprintf(n.pins[n.npins++], 40, "%s", o->s);  }
	  jfree(v);
	int moved = strcmp(n.tb, S.tb) != 0; S = n;
	css_apply(); wall_make(); if (moved) bar_place(); bar_redraw(); if (SAREA) gtk_widget_queue_draw(SAREA);
}
static void apps_load(void)
{
	JV *v = jget("/api/desktop/apps"); if (!v) return;
	JV *a = jprop(v, "apps"); App *L = NULL; int n = 0;
	for (int i = 0; i < 400; i++) { JV *o = jat(a, i); if (!o) break; L = realloc(L, sizeof *L * (n + 1)); App *x = &L[n++]; memset(x, 0, sizeof *x);
		jstr(o, "k", x->k, sizeof x->k); jstr(o, "name", x->name, sizeof x->name); jstr(o, "icon", x->icon, sizeof x->icon); jstr(o, "kw", x->kw, sizeof x->kw);  }
	
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
	double g = jnum(v, "gen", 0); char a[80] = ""; jstr(v, "acct", a, sizeof a); int notes = (int)jnum(v, "notes", 0); jfree(v);
	if (g != S.gen || strcmp(a, S.acct) || notes != S.notes) { int newg = g != S.gen; settings_load(); if (newg) apps_load(); }
	return G_SOURCE_CONTINUE;
}
static gboolean tick_tray(gpointer d) { (void)d; tray_load(); return G_SOURCE_CONTINUE; }
static gboolean tick_clock(gpointer d) { (void)d; char o[32]; snprintf(o, sizeof o, "%s", CLOCK_T); clock_text(); if (strcmp(o, CLOCK_T)) bar_redraw(); if (S.dyn[0] && !S.wpimg) wall_make(); return G_SOURCE_CONTINUE; }
/* the session's watchdog and trial need to hear from the desktop, as the page's heartbeat */
static gboolean tick_heart(gpointer d) { (void)d; http("POST", "/api/heartbeat", NULL, 0, NULL); return G_SOURCE_CONTINUE; }

/* ------------------------------------------------------------------ the tests' view (RUDOLF_NATIVE_STATE) */
static void state_write(void)
{
	if (!STATEF) return;
	GString *s = g_string_new(NULL); int top = !strcmp(S.tb, "top"), by = top ? 0 : SH - BARH;
	g_string_append_printf(s, "screen %d %d\nuser %s\nlocked %d\nsignin %d\nmenu %d\nbar %d %d %d %d %d\nclock %s|%s\n", SW, SH, USER_ID[0] ? USER_N : "-", LOCKED,
		SIGN && gtk_widget_get_visible(SIGN), MENU && gtk_widget_get_visible(MENU), BAR && gtk_widget_get_visible(BAR), 0, by, SW, BARH, CLOCK_T, CLOCK_D);
	for (int i = 0; i < NHITS; i++) g_string_append_printf(s, "hit %s %s %d %d %d %d\n", HITS[i].kind, HITS[i].arg[0] ? HITS[i].arg : "-", HITS[i].x, HITS[i].y + by, HITS[i].w, HITS[i].h);
	for (int i = 0; i < 4 && MENU && gtk_widget_get_visible(MENU); i++) { int wx, wy, x, y; GtkAllocation al; GdkWindow *g = gtk_widget_get_window(MENU); if (!g || !MBTN[i]) continue;
		gdk_window_get_origin(g, &wx, &wy); gtk_widget_get_allocation(MBTN[i], &al); gtk_widget_translate_coordinates(MBTN[i], MENU, 0, 0, &x, &y);
		g_string_append_printf(s, "menubtn %s %d %d %d %d\n", (char *)g_object_get_data(G_OBJECT(MBTN[i]), "what"), wx + x, wy + y, al.width, al.height); }
	if (SMSG) g_string_append_printf(s, "signmsg %s\n", gtk_label_get_text(GTK_LABEL(SMSG)));
	for (GList *l = WINS; l; l = l->next) { AppWin *a = l->data; GdkWindow *g = a->win ? gtk_widget_get_window(a->win) : NULL;
		g_string_append_printf(s, "app %s %lx %d %s\n", a->k, g ? (unsigned long)GDK_WINDOW_XID(g) : 0UL, a == ACTIVE, a->title); }
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
	state_write();
	gtk_main();
	return 0;
}
