/*
 * librudolf-ui: see rudolf-ui.h. Built with mkui.py's header (the page's styles and language tables, rudolf-native's own code):
 *   python3 mkui.py rui-gen.h && gcc -c -I. rudolf-ui.c $(pkg-config --cflags gtk+-3.0)      (iso/sys/ui/build.sh does both)
 * GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <fontconfig/fontconfig.h>
#include "rudolf-ui.h"

/* what rudolf-native's code (below, from rui-gen.h) reads: the settings it was written for */
static struct { char dm[16]; double doff, lat; } S = { "auto", 0, NAN };
#include "rui-gen.h"

RuiSettings RUI;
static char HOST[64] = "127.0.0.1", TOKEN[64], APP[32];
static int PORT = 8080;
static GtkCssProvider *CSSP;
typedef struct { void (*cb)(void *); void *d; GtkWidget *w; } Cb;
static GArray *SETCB;

/* ------------------------------------------------------------------ rudolfd over HTTP (as rudolf-native's http()) */
int rui_http(const char *method, const char *path, const char *body, size_t blen, char **out, size_t *outn) { return rui_http_h(method, path, NULL, body, blen, out, outn); }
/* (with headers of the caller's: "Range: bytes=0-99\r\n"; the answer's body kept whole, a file's bytes with their NULs, *outn long) */
int rui_http_h(const char *method, const char *path, const char *hdrs, const char *body, size_t blen, char **out, size_t *outn)
{
	if (out) *out = NULL; if (outn) *outn = 0;
	int s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0); if (s < 0) return 0;
	struct timeval tv = { 30, 0 }; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv); setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
	struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(PORT) }; inet_pton(AF_INET, HOST, &a.sin_addr);
	if (connect(s, (struct sockaddr *)&a, sizeof a)) { close(s); return 0; }
	GString *h = g_string_new(NULL);
	g_string_printf(h, "%s %s HTTP/1.0\r\nHost: %s:%d\r\nX-Rudolf-Token: %s\r\nContent-Length: %zu\r\nConnection: close\r\n%s\r\n", method, path, HOST, PORT, TOKEN, blen, hdrs ? hdrs : "");
	int okw = send(s, h->str, h->len, MSG_NOSIGNAL) == (ssize_t)h->len; g_string_free(h, TRUE);
	for (size_t o = 0; okw && o < blen;) { ssize_t w = send(s, body + o, blen - o, MSG_NOSIGNAL); if (w <= 0) { okw = 0; break; } o += w; }
	GByteArray *b = g_byte_array_new(); char buf[16384]; ssize_t n;
	while (okw && (n = recv(s, buf, sizeof buf, 0)) > 0) g_byte_array_append(b, (guint8 *)buf, n);
	close(s);
	int code = 0; guint8 z = 0; g_byte_array_append(b, &z, 1);
	if (b->len > 12 && sscanf((char *)b->data, "HTTP/%*s %d", &code) == 1) {
		char *e = strstr((char *)b->data, "\r\n\r\n");
		if (e && out) { size_t at = e + 4 - (char *)b->data; if (outn) *outn = b->len - 1 - at; *out = g_memdup2(e + 4, b->len - at); }   /* (with the NUL added after it) */
	}
	g_byte_array_free(b, TRUE);
	return code;
}
/* the token, as rudolf-native gets it: written into rudolfd's page (GET /), which rudolfd gives only to the desktop's processes.
   The page is read only as far as the token (it comes after the copy of the page's storage, which is not kept) */
static int get_token(void)
{
	int s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0); if (s < 0) return 0;
	struct timeval tv = { 30, 0 }; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
	struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(PORT) }; inet_pton(AF_INET, HOST, &a.sin_addr);
	if (connect(s, (struct sockaddr *)&a, sizeof a)) { close(s); return 0; }
	char req[160]; int rl = snprintf(req, sizeof req, "GET / HTTP/1.0\r\nHost: %s:%d\r\nConnection: close\r\n\r\n", HOST, PORT);
	if (send(s, req, rl, MSG_NOSIGNAL) != rl) { close(s); return 0; }
	static const char K[] = "window.RUDOLFD={token:\""; char win[32768 + 128]; size_t have = 0; ssize_t n; int ok = 0;
	while ((n = recv(s, win + have, sizeof win - 1 - have, 0)) > 0) {
		have += n; win[have] = 0;
		char *t = memmem(win, have, K, sizeof K - 1);
		if (t && memchr(t + sizeof K - 1, '"', win + have - t - (sizeof K - 1))) { t += sizeof K - 1; size_t l = strcspn(t, "\"");
			if (l > 0 && l < sizeof TOKEN && strspn(t, "0123456789abcdefABCDEFghijklmnopqrstuvwxyzGHIJKLMNOPQRSTUVWXYZ_-") >= l) { memcpy(TOKEN, t, l); TOKEN[l] = 0; ok = 1; } break; }
		if (have > sizeof win / 2) { memmove(win, win + have - 128, 128); have = 128; }   /* (the end kept: the token's start may be cut) */
	}
	close(s); explicit_bzero(win, sizeof win); return ok;
}
struct RuiJ;   /* (rudolf-native's JV) */
RuiJ *rui_jparse(const char *s) { return (RuiJ *)jparse(s); }
void rui_jfree(RuiJ *j) { jfree((JV *)j); }
RuiJ *rui_jprop(RuiJ *o, const char *k) { return (RuiJ *)jprop((JV *)o, k); }
RuiJ *rui_jat(RuiJ *a, int i) { return (RuiJ *)jat((JV *)a, i); }
int rui_jlen(RuiJ *a) { JV *j = (JV *)a; return j && (j->t == 4 || j->t == 5) ? j->len : 0; }
double rui_jnum(RuiJ *o, const char *k, double d) { return jnum((JV *)o, k, d); }
int rui_jbool(RuiJ *o, const char *k) { return jbool((JV *)o, k); }
const char *rui_jstr(RuiJ *o, const char *k) { JV *v = jprop((JV *)o, k); return v && v->t == 3 ? v->s : NULL; }
int rui_jtype(RuiJ *j) { return j ? ((JV *)j)->t : -1; }
double rui_jval(RuiJ *j) { JV *v = (JV *)j; return v && (v->t == 1 || v->t == 2) ? v->n : 0; }
const char *rui_jsval(RuiJ *j) { JV *v = (JV *)j; return v && v->t == 3 ? v->s : NULL; }
const char *rui_jkey(RuiJ *o, int i) { JV *v = (JV *)o; return v && v->t == 5 && i >= 0 && i < v->len ? v->k[i] : NULL; }
RuiJ *rui_get(const char *path)
{
	char *t = NULL; int c = rui_http("GET", path, NULL, 0, &t, NULL); JV *v = c == 200 ? jparse(t) : NULL; g_free(t); return (RuiJ *)v;
}
int rui_post(const char *path, const char *body, char *err, size_t errn)
{
	char *t = NULL; int c = rui_http("POST", path, body, body ? strlen(body) : 0, &t, NULL);
	if (err && errn) { err[0] = 0; JV *v = t ? jparse(t) : NULL; JV *e = jprop(v, "error"); if (e && e->t == 3) snprintf(err, errn, "%s", e->s);
		else if (c != 200) snprintf(err, errn, c ? "rudolfd answered %d" : "rudolfd did not answer", c); jfree(v); }
	g_free(t); return c;
}

/* ------------------------------------------------------------------ the translations (the page's trText) */
static int lang_i(void) { for (int i = 0; RUI_LANGS[i]; i++) if (!strcmp(RUI_LANGS[i], RUI.lang)) return i + 1; return 0; }
static const char *tr_find(const char *en, int li)
{
	size_t lo = 0, hi = G_N_ELEMENTS(RUI_TR);
	while (lo < hi) { size_t m = (lo + hi) / 2; int c = strcmp(RUI_TR[m][0], en); if (!c) return RUI_TR[m][li]; if (c < 0) lo = m + 1; else hi = m; }
	return NULL;
}
const char *rui_tr(const char *en)
{
	int li = lang_i(); if (!li || !en || !*en) return en;
	const char *w = tr_find(en, li); if (w) return w;
	/* (the page's second try: what is not a letter or a digit at either end kept as it is, the words between translated) */
	static GHashTable *made; if (!made) made = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	char *key = g_strdup_printf("%d\x1f%s", li, en); const char *got = g_hash_table_lookup(made, key); if (got) { g_free(key); return *got ? got : en; }
	const char *a = en, *b = en + strlen(en);
	while (*a) { gunichar c = g_utf8_get_char(a); if (g_unichar_isalnum(c)) break; a = g_utf8_next_char(a); }
	while (b > a) { const char *p = g_utf8_prev_char(b); if (g_unichar_isalnum(g_utf8_get_char(p))) break; b = p; }
	char *mid = g_strndup(a, b - a); const char *t = *mid ? tr_find(mid, li) : NULL; char *res = t ? g_strdup_printf("%.*s%s%s", (int)(a - en), en, t, b) : g_strdup("");
	g_free(mid); g_hash_table_insert(made, key, res); return *res ? res : en;
}

/* ------------------------------------------------------------------ the settings and the theme */
static double line_h(double px, int wt); static void font_options(void);
double rui_px(double css) { return css * RUI.z; }
const char *rui_version(void) { return RUI_RVER; }
const char *rui_version_date(void) { return RUI_RVERDATE; }
/* the theme for the settings now: mkui.py's CSS with the account's values in, every length scaled by the display scaling */
static void theme_apply(void)
{
	const char *src = RUI.dark ? RUI_CSS_DARK : RUI_CSS_LIGHT; GString *o = g_string_new(NULL);
	RGB ac = hexc(RUI.accent, "#2563eb"); char sel[32]; snprintf(sel, sizeof sel, "rgba(%d,%d,%d,0.3)", (int)lround(ac.r * 255), (int)lround(ac.g * 255), (int)lround(ac.b * 255));
	char fs[16], ls[16]; snprintf(fs, sizeof fs, "%g", RUI.fs * 1.0); snprintf(ls, sizeof ls, "%.2f", RUI_PAGE[0].h4_ls[0] * 13 * RUI.z);
	char fam[160]; { GString *f = g_string_new(NULL); char **v = g_strsplit(RUI.family, ",", -1); for (int i = 0; v[i]; i++) { char *x = g_strstrip(v[i]); g_string_append_printf(f, "%s\"%s\"", i ? ", " : "", x); } g_strfreev(v); snprintf(fam, sizeof fam, "%s", f->str); g_string_free(f, TRUE); }
	for (const char *p = src; *p;) {
		if (*p == '@') { const char *e = strchr(p + 1, '@'); if (e) { char k[16]; snprintf(k, sizeof k, "%.*s", (int)(e - p - 1), p + 1);
			const char *v = !strcmp(k, "AC") ? RUI.accent : !strcmp(k, "ACFG") || !strcmp(k, "PRIFG") ? (!strcmp(k, "PRIFG") ? "#fff" : RUI.acfg) : !strcmp(k, "FONT") ? fam : !strcmp(k, "TX") ? RUI.tx : !strcmp(k, "SELBG") ? sel : !strcmp(k, "SELFG") ? "inherit" : !strcmp(k, "LS") ? ls : NULL;
			if (!strcmp(k, "FS")) { g_string_append_printf(o, "%g", RUI.fs * RUI.z); p = e + 1; continue; }
			if (v) { g_string_append(o, v); p = e + 1; continue; } } }
		/* a length: NUMBERpx, scaled */
		if ((g_ascii_isdigit(*p) || (*p == '.' && g_ascii_isdigit(p[1]))) && (p == src || !g_ascii_isalnum(p[-1]) || p[-1] == ' ')) {
			char *e; double v = g_ascii_strtod(p, &e); if (e[0] == 'p' && e[1] == 'x') { char b[32]; g_ascii_formatd(b, sizeof b, "%.2f", v * RUI.z); g_string_append(o, b); g_string_append(o, "px"); p = e + 2; continue; } }
		g_string_append_c(o, *p++);
	}
	/* the page's line box: GTK makes a line of writing as tall as its font's ascent and descent added and rounded up (16 px for
	   DejaVu Sans at 13 px), the page as each rounded (15 px): the difference taken from under every line GTK writes */
	{ PangoFontMap *fm = pango_cairo_font_map_get_default(); PangoContext *c = pango_font_map_create_context(fm); pango_cairo_context_set_font_options(c, gdk_screen_get_font_options(gdk_screen_get_default()));
	  PangoLayout *l = pango_layout_new(c); PangoFontDescription *f = pango_font_description_from_string(RUI.family); pango_font_description_set_absolute_size(f, RUI.fs * RUI.z * PANGO_SCALE);
	  pango_layout_set_font_description(l, f); pango_layout_set_text(l, "Ag", -1); int w, h; pango_layout_get_pixel_size(l, &w, &h); double d = h - line_h(RUI.fs, 400) * RUI.z;
	  pango_font_description_free(f); g_object_unref(l); g_object_unref(c);
	  if (d > 0) { const RuiPage *p = &RUI_PAGE[RUI.dark ? 1 : 0]; g_string_append_printf(o, ".rui-body label, .rui-dlg label, tooltip label { margin-bottom: -%.0fpx; }\nentry.rui { padding-bottom: %.2fpx; }\n", d, fmax(0, p->in_pad[2] * RUI.z - d)); } }
	if (!CSSP) { CSSP = gtk_css_provider_new(); gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(CSSP), GTK_STYLE_PROVIDER_PRIORITY_USER); }
	GError *err = NULL; if (!gtk_css_provider_load_from_data(CSSP, o->str, -1, &err)) { g_printerr("librudolf-ui: the theme: %s\n", err ? err->message : "?"); g_clear_error(&err); }
	g_string_free(o, TRUE);
}
static int settings_read(void)
{
	JV *v = (JV *)rui_get("/api/desktop/settings"); if (!v) return 0;
	JV *s = jprop(v, "s"); RuiSettings n; memset(&n, 0, sizeof n);
	jstr(s, "accent", n.accent, sizeof n.accent); { RGB c; if (!hex2rgb(n.accent, &c)) snprintf(n.accent, sizeof n.accent, "#2563eb"); }
	jstr(s, "dm", S.dm, sizeof S.dm); if (!S.dm[0]) snprintf(S.dm, sizeof S.dm, "auto"); snprintf(n.dm, sizeof n.dm, "%s", S.dm);
	S.doff = jnum(s, "doff", 0); S.lat = jnum(s, "lat", NAN);
	{ JV *dk = jprop(s, "darkui"); int darkui = !(dk && dk->t == 1 && !dk->n); n.dark = darkui && is_dark(); }   /* (themeApply: S.darkui!==false and dynResolve's mode) */
	jstr(s, "ff", n.ff, sizeof n.ff); ui_family(n.ff); snprintf(n.family, sizeof n.family, "%s", UIFAM);
	n.fs = (int)jnum(s, "fs", 13); if (n.fs < 6 || n.fs > 72) n.fs = 13;
	n.scale = (int)jnum(s, "scale", 100); if (n.scale < 75 || n.scale > 250) n.scale = 100; n.z = n.scale / 100.0;
	jstr(s, "lang", n.lang, sizeof n.lang); if (!n.lang[0]) snprintf(n.lang, sizeof n.lang, "en");
	{ char cb[16] = ""; jstr(s, "cb", cb, sizeof cb); n.hc = !strcmp(cb, "contrast"); }
	jstr(s, "tx", n.tx, sizeof n.tx); { RGB c; if (!hex2rgb(n.tx, &c)) snprintf(n.tx, sizeof n.tx, "#000000"); }
	snprintf(n.acfg, sizeof n.acfg, "%s", ink_black(hexc(n.accent, "#2563eb")) ? "#000000" : "#ffffff");   /* (acFg) */
	n.gen = RUI.gen; int changed = memcmp(&n, &RUI, sizeof n) != 0;   /* (what the look depends on: not every change of the page's settings) */
	n.gen = jnum(v, "gen", 0); jfree(v); RUI = n; return changed ? 1 : 2;
}
void rui_on_settings(void (*cb)(void *), void *d) { Cb c = { cb, d, NULL }; if (!SETCB) SETCB = g_array_new(FALSE, FALSE, sizeof(Cb)); g_array_append_val(SETCB, c); }
/* (a widget's own: forgotten when the widget goes) */
static void cb_gone(GtkWidget *w, gpointer d) { (void)d; for (guint i = 0; SETCB && i < SETCB->len; i++) if (g_array_index(SETCB, Cb, i).w == w) { g_array_remove_index(SETCB, i); i--; } }
static void on_settings_w(GtkWidget *w, void (*cb)(void *), void *d) { Cb c = { cb, d, w }; if (!SETCB) SETCB = g_array_new(FALSE, FALSE, sizeof(Cb)); g_array_append_val(SETCB, c); g_signal_connect(w, "destroy", G_CALLBACK(cb_gone), NULL); }
static void redraw_all(void) { GList *l = gtk_window_list_toplevels(); for (GList *x = l; x; x = x->next) gtk_widget_queue_draw(x->data); g_list_free(l); }
/* followed live: the page's settings' "gen" (every change the page saves), and light or dark as the day turns */
static gboolean settings_tick(gpointer d)
{
	(void)d; JV *v = (JV *)rui_get("/api/desktop/settings"); if (!v) return G_SOURCE_CONTINUE;
	double g = jnum(v, "gen", 0); jfree(v);
	int dk = RUI.dark; if (!strcmp(S.dm, "auto")) { int nd = is_dark(); dk = nd != RUI.dark; }
	if (g != RUI.gen || dk) { if (settings_read() == 1) { font_options(); theme_apply(); redraw_all(); GArray *l = SETCB ? g_array_copy(SETCB) : NULL;   /* (a callback may make or destroy widgets) */
		for (guint i = 0; l && i < l->len; i++) { Cb *c = &g_array_index(l, Cb, i); int live = !c->w; for (guint k = 0; !live && SETCB && k < SETCB->len; k++) if (g_array_index(SETCB, Cb, k).w == c->w && g_array_index(SETCB, Cb, k).d == c->d) live = 1; if (live) c->cb(c->d); }
		if (l) g_array_unref(l); } }
	return G_SOURCE_CONTINUE;
}
/* the writing as Chromium writes the page: antialiased in grey, not hinted (measured against Chromium with fontconfig saying slight
   hinting and subpixel colours, as on GitHub's runner, and saying neither, as here: Chromium's letters were the same, grey and
   unhinted, both times; GTK's own defaults, or fontconfig's, made them 4 to 10 % different), antialiasing off only where fontconfig
   turns it off for the font; and the font's own line height (metrics not rounded up to whole pixels each: Pango's 17 px lines for
   DejaVu Sans at 13 px against the page's 15) */
static void font_options(void)
{
	FcPattern *p = FcPatternCreate(); char fam[96]; snprintf(fam, sizeof fam, "%.*s", (int)strcspn(RUI.family, ","), RUI.family);
	FcPatternAddString(p, FC_FAMILY, (const FcChar8 *)fam); FcPatternAddDouble(p, FC_PIXEL_SIZE, RUI.fs * RUI.z);
	FcConfigSubstitute(NULL, p, FcMatchPattern); FcDefaultSubstitute(p); FcResult r; FcPattern *m = FcFontMatch(NULL, p, &r);
	FcBool aa = FcTrue; if (m) { FcPatternGetBool(m, FC_ANTIALIAS, 0, &aa); FcPatternDestroy(m); } FcPatternDestroy(p);
	GtkSettings *gs = gtk_settings_get_default(); g_object_set(gs, "gtk-xft-antialias", aa ? 1 : 0, "gtk-xft-hinting", 0, "gtk-xft-hintstyle", "hintnone", "gtk-xft-rgba", "none", NULL);
	cairo_font_options_t *fo = cairo_font_options_create(); cairo_font_options_set_antialias(fo, aa ? CAIRO_ANTIALIAS_GRAY : CAIRO_ANTIALIAS_NONE);
	cairo_font_options_set_hint_style(fo, CAIRO_HINT_STYLE_NONE); cairo_font_options_set_hint_metrics(fo, CAIRO_HINT_METRICS_OFF);
	gdk_screen_set_font_options(gdk_screen_get_default(), fo); cairo_font_options_destroy(fo);
}
int rui_init(int *argc, char ***argv, const char *app)
{
	snprintf(APP, sizeof APP, "%s", app ? app : "app");
	/* (the base as rudolf-native's --base, for the tests: RUI_BASE=http://127.0.0.1:PORT; rudolfd's own address otherwise) */
	const char *b = getenv("RUI_BASE");
	if (b && !strncmp(b, "http://", 7)) { char h[64]; int p = 0; if (sscanf(b + 7, "%63[^:/]:%d", h, &p) >= 1) { snprintf(HOST, sizeof HOST, "%s", h); if (p > 0) PORT = p; } }
	gdk_set_allowed_backends("x11"); gtk_init(argc, argv);
	font_options();
	g_set_prgname(g_strdup_printf("rudolf-%s", APP));
	memset(&RUI, 0, sizeof RUI); snprintf(RUI.accent, sizeof RUI.accent, "#2563eb"); snprintf(RUI.acfg, sizeof RUI.acfg, "#ffffff"); RUI.fs = 13; RUI.z = 1; RUI.scale = 100;
	snprintf(RUI.lang, sizeof RUI.lang, "en"); snprintf(RUI.tx, sizeof RUI.tx, "#000000"); ui_family(NULL); snprintf(RUI.family, sizeof RUI.family, "%s", UIFAM);
	int ok = get_token();
	if (ok) settings_read(); font_options(); if (!ok) g_printerr("librudolf-ui: rudolfd gave no token (only the desktop's own programs are given it)\n");
	theme_apply();
	g_timeout_add_seconds(3, settings_tick, NULL);
	return ok;
}

/* ------------------------------------------------------------------ drawing in the page's look */
static RuiC pcol(RuiC c)
{
	if (c.a >= 0) return c;
	if (c.a == -2) { RGB t = hexc(RUI.tx, "#000000"); return (RuiC){ t.r, t.g, t.b, 1 }; }
	RGB a = hexc(RUI.accent, "#2563eb"); return (RuiC){ a.r, a.g, a.b, c.a <= -1.001 ? -c.a - 1 : 1 };
}
static const RuiPage *PG(void) { return &RUI_PAGE[RUI.dark ? 1 : 0]; }
static void setc(cairo_t *cr, RuiC c, double alpha) { c = pcol(c); cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a * alpha); }
void rui_color(cairo_t *cr, const char *w, double alpha)
{
	const RuiPage *p = PG();
	if (!strcmp(w, "ac")) { RGB a = hexc(RUI.accent, "#2563eb"); cairo_set_source_rgba(cr, a.r, a.g, a.b, alpha); return; }
	if (!strcmp(w, "acfg")) { RGB a = hexc(RUI.acfg, "#ffffff"); cairo_set_source_rgba(cr, a.r, a.g, a.b, alpha); return; }
	setc(cr, !strcmp(w, "wbg") ? p->wbg : !strcmp(w, "bg2") ? p->bg2 : !strcmp(w, "bd") ? p->bd : !strcmp(w, "mut") ? p->mut : p->fg, alpha);
}
/* a line of writing as the page writes it: the Font setting's family at a CSS size (the display scaling applied) */
PangoLayout *rui_layout(cairo_t *cr, const char *text, double px, int weight)
{
	PangoLayout *l = pango_cairo_create_layout(cr); PangoFontDescription *f = pango_font_description_from_string(RUI.family);
	{ const cairo_font_options_t *so = gdk_screen_get_font_options(gdk_screen_get_default()); if (so) pango_cairo_context_set_font_options(pango_layout_get_context(l), so);
	  pango_context_set_round_glyph_positions(pango_layout_get_context(l), FALSE); pango_layout_context_changed(l); }
	pango_font_description_set_absolute_size(f, px * RUI.z * PANGO_SCALE); pango_font_description_set_weight(f, weight);
	pango_layout_set_font_description(l, f); pango_font_description_free(f); pango_layout_set_text(l, text ? text : "", -1); return l;
}
/* writing in a box (CSS px from the box's corner, its line box centred as CSS centres a line of normal line-height) */
static double text_w(cairo_t *cr, const char *t, double px, int wt) { PangoLayout *l = rui_layout(cr, t, px, wt); int w, h; pango_layout_get_pixel_size(l, &w, &h); g_object_unref(l); return w / RUI.z; }
static void text_in(cairo_t *cr, const char *t, double x, double y, double w, double h, double px, int wt, int align)
{
	PangoLayout *l = rui_layout(cr, t, px, wt); PangoRectangle ink, lg; pango_layout_get_pixel_extents(l, &ink, &lg); (void)ink;
	double z = RUI.z, tx = align == RUI_RIGHT ? x * z + w * z - lg.width : x * z;
	if (align == 2) tx = x * z + (w * z - lg.width) / 2;
	cairo_save(cr); cairo_rectangle(cr, x * z, y * z, w * z, h * z); cairo_clip(cr);
	cairo_move_to(cr, round(tx), round(y * z + (h * z - lg.height) / 2)); pango_cairo_show_layout(cr, l); cairo_restore(cr); g_object_unref(l);
}
static void rrect(cairo_t *cr, double x, double y, double w, double h, double r)
{
	r = fmin(r, fmin(w, h) / 2); cairo_new_sub_path(cr);
	cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0); cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
	cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI); cairo_arc(cr, x + r, y + r, r, M_PI, 1.5 * M_PI); cairo_close_path(cr);
}
/* the page's line box for its font: CSS's "normal" line height, as Chromium takes it from the font (ascent + descent) */
static double line_h(double px, int wt)
{
	PangoFontMap *fm = pango_cairo_font_map_get_default(); PangoContext *c = pango_font_map_create_context(fm);
	{ const cairo_font_options_t *so = gdk_screen_get_font_options(gdk_screen_get_default()); if (so) pango_cairo_context_set_font_options(c, so); }
	PangoFontDescription *f = pango_font_description_from_string(RUI.family); pango_font_description_set_absolute_size(f, px * RUI.z * PANGO_SCALE); pango_font_description_set_weight(f, wt);
	PangoFontMetrics *m = pango_context_get_metrics(c, f, NULL);   /* (Chromium: the ascent and the descent each rounded) */
	double h = round(pango_font_metrics_get_ascent(m) / (double)PANGO_SCALE) + round(pango_font_metrics_get_descent(m) / (double)PANGO_SCALE);
	pango_font_metrics_unref(m); pango_font_description_free(f); g_object_unref(c); return h / RUI.z;
}

/* ------------------------------------------------------------------ windows and GTK's controls */
static void on_dlg_esc(void);
static gboolean win_key(GtkWidget *w, GdkEventKey *e, gpointer d) { (void)w; (void)d; if (e->keyval == GDK_KEY_Escape && rui_dialog_box()) { on_dlg_esc(); return TRUE; } return FALSE; }
RuiWin *rui_window(const char *app, const char *title, int w, int h)
{
	RuiWin *r = g_new0(RuiWin, 1); snprintf(r->app, sizeof r->app, "%s", app);
	r->win = gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_window_set_title(GTK_WINDOW(r->win), rui_tr(title));
	char cls[48]; snprintf(cls, sizeof cls, "rudolf-%s", app); gtk_window_set_wmclass(GTK_WINDOW(r->win), cls, "Rudolf");
	gtk_window_set_default_size(GTK_WINDOW(r->win), (int)lround(w * RUI.z), (int)lround(h * RUI.z));
	gtk_style_context_add_class(gtk_widget_get_style_context(r->win), "rui-body");
	r->overlay = gtk_overlay_new(); gtk_container_add(GTK_CONTAINER(r->win), r->overlay);
	r->body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); gtk_container_add(GTK_CONTAINER(r->overlay), r->body);
	g_signal_connect(r->win, "key-press-event", G_CALLBACK(win_key), r);
	return r;
}
static void cls(GtkWidget *w, const char *c) { gtk_style_context_add_class(gtk_widget_get_style_context(w), c); }
/* letters placed as Chromium places them: each at its exact advance, not rounded to a whole pixel one by one (Pango's default, which
   makes a line of Liberation Sans at 14 px 3 px longer than the page's) */
static void exact(GtkWidget *w) { if (GTK_IS_BIN(w) && gtk_bin_get_child(GTK_BIN(w))) w = gtk_bin_get_child(GTK_BIN(w)); pango_context_set_round_glyph_positions(gtk_widget_get_pango_context(w), FALSE); }
GtkWidget *rui_button(const char *label, int pri) { GtkWidget *b = gtk_button_new_with_label(rui_tr(label)); exact(b); cls(b, "rui"); if (pri) cls(b, "pri"); gtk_widget_set_valign(b, GTK_ALIGN_CENTER); return b; }
GtkWidget *rui_entry(const char *ph) { GtkWidget *e = gtk_entry_new(); exact(e); cls(e, "rui"); if (ph) gtk_entry_set_placeholder_text(GTK_ENTRY(e), rui_tr(ph)); gtk_entry_set_width_chars(GTK_ENTRY(e), 1); gtk_widget_set_valign(e, GTK_ALIGN_CENTER); return e; }
GtkWidget *rui_label(const char *t) { GtkWidget *l = gtk_label_new(rui_tr(t)); exact(l); gtk_label_set_xalign(GTK_LABEL(l), 0); return l; }
GtkWidget *rui_heading(const char *t)
{
	char *u = g_utf8_strup(rui_tr(t), -1); GtkWidget *l = gtk_label_new(u); g_free(u); exact(l); gtk_label_set_xalign(GTK_LABEL(l), 0); cls(l, "rui-h4");
	const RuiPage *p = PG(); gtk_widget_set_margin_top(l, (int)lround(rui_px(p->h4_m[0]))); gtk_widget_set_margin_bottom(l, (int)lround(rui_px(p->h4_m[2]))); return l;
}
GtkWidget *rui_msg(const char *t) { GtkWidget *l = gtk_label_new(rui_tr(t)); exact(l); gtk_label_set_xalign(GTK_LABEL(l), 0); gtk_label_set_line_wrap(GTK_LABEL(l), TRUE); cls(l, "rui-msg"); gtk_widget_set_size_request(l, -1, (int)lround(rui_px(PG()->msg_mh[0]))); gtk_label_set_yalign(GTK_LABEL(l), 0); return l; }

/* ---- the tabs (.tmtabs) */
typedef struct { void (*cb)(int, void *); void *d; int n, on; GtkWidget *b[16]; } Tabs;
static void tab_clicked(GtkButton *b, gpointer d) { Tabs *t = d; for (int i = 0; i < t->n; i++) if (t->b[i] == (GtkWidget *)b) { rui_tabs_set(g_object_get_data(G_OBJECT(b), "rui-tabs"), i); if (t->cb) t->cb(i, t->d); } }
GtkWidget *rui_tabs(const char *const *labels, int n, void (*cb)(int, void *), void *d)
{
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2); cls(box, "rui-tabs"); Tabs *t = g_new0(Tabs, 1); t->cb = cb; t->d = d; t->n = MIN(n, 16);
	g_object_set_data_full(G_OBJECT(box), "rui-t", t, g_free);
	for (int i = 0; i < t->n; i++) { GtkWidget *b = gtk_button_new_with_label(rui_tr(labels[i])); exact(b); t->b[i] = b; g_object_set_data(G_OBJECT(b), "rui-tabs", box);
		gtk_widget_set_can_focus(b, FALSE); g_signal_connect(b, "clicked", G_CALLBACK(tab_clicked), t); gtk_box_pack_start(GTK_BOX(box), b, FALSE, FALSE, 0); }
	rui_tabs_set(box, 0); return box;
}
void rui_tabs_set(GtkWidget *box, int i)
{
	Tabs *t = g_object_get_data(G_OBJECT(box), "rui-t"); if (!t) return; t->on = i;
	for (int k = 0; k < t->n; k++) { GtkStyleContext *c = gtk_widget_get_style_context(t->b[k]); if (k == i) gtk_style_context_add_class(c, "on"); else gtk_style_context_remove_class(c, "on"); }
}
GtkWidget *rui_tabs_button(GtkWidget *box, int i) { Tabs *t = g_object_get_data(G_OBJECT(box), "rui-t"); return t && i >= 0 && i < t->n ? t->b[i] : NULL; }

/* ---- the check box (label.ck): Chromium's own check box (13 px, its blue when on), the words beside it */
typedef struct { int on; char *label; void (*cb)(int, void *); void *d; double ww, wh; } Check;   /* (ww, wh: wrapped in that width: rui_check_wrap) */
static gboolean check_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Check *c = d; double z = RUI.z, lh = RUI.fs * 1.35;   /* (label.ck: line-height 1.35) */
	(void)w;
	/* the box: 13 px, with Chromium's margins for a check box (3 px, 4 at the left), at the line's start (align-items: flex-start) */
	double bx = 4, by = 3, s = 13;
	cairo_save(cr); cairo_scale(cr, z, z);
	if (c->on) { cairo_set_source_rgb(cr, 0, 117 / 255.0, 1); rrect(cr, bx, by, s, s, 2); cairo_fill(cr);
		cairo_set_source_rgb(cr, 1, 1, 1); cairo_set_line_width(cr, 1.8); cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE); cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
		cairo_move_to(cr, bx + 3.2, by + 6.8); cairo_line_to(cr, bx + 5.4, by + 9.2); cairo_line_to(cr, bx + 9.9, by + 3.9); cairo_stroke(cr); }
	else { double al = gtk_widget_is_sensitive(w) ? 1 : .3;   /* (a disabled one, as Chromium draws it: faded) */
		cairo_set_source_rgb(cr, RUI.dark ? .23 : 1, RUI.dark ? .23 : 1, RUI.dark ? .23 : 1); rrect(cr, bx + .5, by + .5, s - 1, s - 1, 2); cairo_fill_preserve(cr); cairo_set_source_rgba(cr, .46, .46, .46, al); cairo_set_line_width(cr, 1); cairo_stroke(cr); }
	cairo_restore(cr);
	/* the words: 8 px after the box (gap 8 px, the box's own 3 px margins) */
	GtkStyleContext *sc = gtk_widget_get_style_context(w); GdkRGBA fg; gtk_style_context_get_color(sc, gtk_style_context_get_state(sc), &fg); gdk_cairo_set_source_rgba(cr, &fg);
	int wt0 = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), "rui-ck-wt")); double tx0 = g_object_get_data(G_OBJECT(w), "rui-ck-inl") ? bx + s + 3 + RUI.fs * .2769 : bx + s + 3 + 8;
	if (c->ww > 0) {   /* (its words wrapped, each line in the label's line box: Settings' long check boxes) */
		PangoLayout *l = rui_layout(cr, c->label, RUI.fs, wt0 ? wt0 : 400); pango_layout_set_width(l, (int)((c->ww - tx0) * RUI.z * PANGO_SCALE)); pango_layout_set_wrap(l, PANGO_WRAP_WORD_CHAR);
		PangoFontMetrics *m = pango_context_get_metrics(pango_layout_get_context(l), pango_layout_get_font_description(l), NULL);
		double a = round(pango_font_metrics_get_ascent(m) / (double)PANGO_SCALE), de = round(pango_font_metrics_get_descent(m) / (double)PANGO_SCALE); pango_font_metrics_unref(m);
		PangoLayoutIter *it = pango_layout_get_iter(l); int i = 0;
		do { cairo_move_to(cr, round(tx0 * RUI.z), round(i * lh * RUI.z + (lh * RUI.z - a - de) / 2 + a)); pango_cairo_show_layout_line(cr, pango_layout_iter_get_line_readonly(it)); i++; } while (pango_layout_iter_next_line(it));
		pango_layout_iter_free(it); g_object_unref(l); return TRUE; }
	{ int wt = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), "rui-ck-wt")); double tx = g_object_get_data(G_OBJECT(w), "rui-ck-inl") ? bx + s + 3 + RUI.fs * .2769 : bx + s + 3 + 8;   /* (1.6.3: a bold one; one in a plain <label>: its words a space after the box) */
	  text_in(cr, c->label, tx, 0, 2000, lh, RUI.fs, wt ? wt : 400, 0); }   /* (the gap: 8 px after the box's own margin) */
	return TRUE;
}
static gboolean check_press(GtkWidget *w, GdkEventButton *e, gpointer d) { Check *c = d; if (e->button != 1 || e->type != GDK_BUTTON_RELEASE) return e->button == 1;   /* (on release, as a button is clicked) */ c->on = !c->on; gtk_widget_queue_draw(w); if (c->cb) c->cb(c->on, c->d); return TRUE; }
static void check_free(gpointer d) { Check *c = d; g_free(c->label); g_free(c); }
static void check_size(GtkWidget *w) { Check *c = g_object_get_data(G_OBJECT(w), "rui-ck"); cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_A8, 1, 1); cairo_t *cr = cairo_create(s);
	int wt = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), "rui-ck-wt")); int tw = (int)lround(rui_px(4 + 13 + 3 + 8 + text_w(cr, c->label, RUI.fs, wt ? wt : 400) + 1)); cairo_destroy(cr); cairo_surface_destroy(s);
	if (c->ww > 0) { gtk_widget_set_size_request(w, (int)lround(rui_px(c->ww)), (int)lround(rui_px(c->wh))); return; }
	gtk_widget_set_size_request(w, tw, (int)lround(rui_px(fmax(19, RUI.fs * 1.35)))); }
void rui_check_style(GtkWidget *w, int weight, int inl) { g_object_set_data(G_OBJECT(w), "rui-ck-wt", GINT_TO_POINTER(weight)); g_object_set_data(G_OBJECT(w), "rui-ck-inl", GINT_TO_POINTER(inl)); check_size(w); gtk_widget_queue_draw(w); }
void rui_check_wrap(GtkWidget *w, double css_w, double css_h) { Check *c = g_object_get_data(G_OBJECT(w), "rui-ck"); if (!c) return; c->ww = css_w; c->wh = css_h; check_size(w); gtk_widget_queue_draw(w); }
static void check_re(void *d) { check_size(d); }
GtkWidget *rui_check(const char *label, int on, void (*cb)(int, void *), void *d)
{
	Check *c = g_new0(Check, 1); c->on = on; c->label = g_strdup(rui_tr(label)); c->cb = cb; c->d = d;
	GtkWidget *w = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(w), "rui-ck", c, check_free); gtk_widget_add_events(w, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
	g_signal_connect(w, "draw", G_CALLBACK(check_draw), c); g_signal_connect(w, "button-press-event", G_CALLBACK(check_press), c); g_signal_connect(w, "button-release-event", G_CALLBACK(check_press), c);
	check_size(w); on_settings_w(w, check_re, w); return w;
}
int rui_check_get(GtkWidget *w) { Check *c = g_object_get_data(G_OBJECT(w), "rui-ck"); return c && c->on; }
void rui_check_set(GtkWidget *w, int on) { Check *c = g_object_get_data(G_OBJECT(w), "rui-ck"); if (c && c->on != !!on) { c->on = !!on; gtk_widget_queue_draw(w); } }

/* ---- scroll bars as Chromium draws them (15 px: the track, the arrows at both ends, a grey rounded thumb) */
#define SBW 15.0
typedef struct { GtkWidget *sw, *bar; GtkAdjustment *adj; double grab, grab_v; int need; guint idle; } Scr;
static gboolean sb_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Scr *s = d; double z = RUI.z, H = gtk_widget_get_allocated_height(w) / z;
	double lo = gtk_adjustment_get_lower(s->adj), up = gtk_adjustment_get_upper(s->adj), pg = gtk_adjustment_get_page_size(s->adj), v = gtk_adjustment_get_value(s->adj);
	cairo_scale(cr, z, z);
	cairo_set_source_rgb(cr, 252 / 255.0, 252 / 255.0, 252 / 255.0); cairo_paint(cr);
	cairo_set_source_rgb(cr, 139 / 255.0, 139 / 255.0, 139 / 255.0);
	/* the arrows: 9 px wide, 5 tall, centred in their 15 px buttons */
	cairo_move_to(cr, 7.5, 6); cairo_line_to(cr, 12, 11); cairo_line_to(cr, 3, 11); cairo_close_path(cr); cairo_fill(cr);
	cairo_move_to(cr, 3, H - 12); cairo_line_to(cr, 12, H - 12); cairo_line_to(cr, 7.5, H - 7); cairo_close_path(cr); cairo_fill(cr);
	double tl = H - 2 * SBW, len = up - lo > 0 ? fmax(25, tl * pg / (up - lo)) : tl, at = up - lo - pg > 0 ? (tl - len) * (v - lo) / (up - lo - pg) : 0;
	rrect(cr, 3, SBW + at + 4, 9, len - 8, 4.5); cairo_fill(cr);
	return TRUE;
}
/* (shown only when there is more than fits, as Chromium's: its width set after GTK's allocation, not during it) */
static gboolean sb_apply(gpointer d) { Scr *s = d; s->grab_v = s->grab_v; gtk_widget_set_size_request(s->bar, s->need ? (int)lround(rui_px(SBW)) : 0, -1); s->idle = 0; return G_SOURCE_REMOVE; }
static void sb_changed(GtkAdjustment *a, gpointer d) { Scr *s = d; (void)a; double pg = gtk_adjustment_get_page_size(s->adj), r = gtk_adjustment_get_upper(s->adj) - gtk_adjustment_get_lower(s->adj);
	int need = r > pg + .5; if (need != s->need) { s->need = need; if (!s->idle) s->idle = g_idle_add(sb_apply, s); } gtk_widget_queue_draw(s->bar); }
static gboolean sb_press(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	Scr *s = d; double z = RUI.z, y = e->y / z, H = gtk_widget_get_allocated_height(w) / z, pg = gtk_adjustment_get_page_size(s->adj), v = gtk_adjustment_get_value(s->adj);
	if (y < SBW) gtk_adjustment_set_value(s->adj, v - 40); else if (y > H - SBW) gtk_adjustment_set_value(s->adj, v + 40);
	else { s->grab = y; s->grab_v = v; (void)pg; }
	return TRUE;
}
static gboolean sb_motion(GtkWidget *w, GdkEventMotion *e, gpointer d)
{
	Scr *s = d; if (!(e->state & GDK_BUTTON1_MASK) || s->grab <= 0) return FALSE; double z = RUI.z, H = gtk_widget_get_allocated_height(w) / z, tl = H - 2 * SBW;
	double lo = gtk_adjustment_get_lower(s->adj), up = gtk_adjustment_get_upper(s->adj), pg = gtk_adjustment_get_page_size(s->adj), len = up - lo > 0 ? fmax(25, tl * pg / (up - lo)) : tl;
	if (tl - len > 0) gtk_adjustment_set_value(s->adj, s->grab_v + (e->y / z - s->grab) * (up - lo - pg) / (tl - len)); return TRUE;
}
static gboolean sb_release(GtkWidget *w, GdkEventButton *e, gpointer d) { (void)w; (void)e; ((Scr *)d)->grab = 0; return TRUE; }
static void sb_width(void *d) { Scr *s = d; gtk_widget_set_size_request(s->bar, s->need ? (int)lround(rui_px(SBW)) : 0, -1); }
GtkWidget *rui_scroller(GtkWidget *child)
{
	Scr *s = g_new0(Scr, 1);
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0); g_object_set_data_full(G_OBJECT(box), "rui-scr", s, g_free);
	s->sw = gtk_scrolled_window_new(NULL, NULL); gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(s->sw), GTK_POLICY_NEVER, GTK_POLICY_EXTERNAL);
	gtk_container_add(GTK_CONTAINER(s->sw), child); gtk_box_pack_start(GTK_BOX(box), s->sw, TRUE, TRUE, 0);
	{ GtkWidget *vp = gtk_bin_get_child(GTK_BIN(s->sw)); if (GTK_IS_VIEWPORT(vp)) gtk_container_set_focus_vadjustment(GTK_CONTAINER(vp), NULL); }   /* (a click does not scroll the page to what it focused) */
	s->adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(s->sw));
	s->bar = gtk_drawing_area_new(); gtk_widget_add_events(s->bar, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK); sb_width(s);
	g_signal_connect(s->bar, "draw", G_CALLBACK(sb_draw), s); g_signal_connect(s->bar, "button-press-event", G_CALLBACK(sb_press), s);
	g_signal_connect(s->bar, "motion-notify-event", G_CALLBACK(sb_motion), s); g_signal_connect(s->bar, "button-release-event", G_CALLBACK(sb_release), s);
	g_signal_connect(s->adj, "changed", G_CALLBACK(sb_changed), s); g_signal_connect(s->adj, "value-changed", G_CALLBACK(sb_changed), s);
	gtk_box_pack_start(GTK_BOX(box), s->bar, FALSE, FALSE, 0); on_settings_w(s->bar, sb_width, s);
	return box;
}

/* ------------------------------------------------------------------ the table (.tmt) */
typedef struct { char **c; long id; int group, muted[8], padl[8], bold[8]; char *btn[8]; double y, h; } Row;
struct RuiTable { GtkWidget *w; int n, nohead, wrap; char *title[8]; int align[8]; double width[8], colx[9], H, lastW; GPtrArray *rows; long sel; int hov;
	void (*row)(long, int, double, double, void *); void (*head)(int, void *); void (*btn)(long, int, void *); void *d, *bd; int pend, pwhich; long pid_; };
static void row_free(gpointer p) { Row *r = p; if (r->c) g_strfreev(r->c); for (int i = 0; i < 8; i++) g_free(r->btn[i]); g_free(r); }
static double head_h0(void) { const RuiPage *p = PG(); return p->th_pad[0] + line_h(p->tmt_fs[0], 700) + p->th_pad[2] + p->th_bw[0]; }
static double head_h(RuiTable *t) { return t->nohead ? 0 : head_h0(); }
static double btn_w(cairo_t *cr, const char *l) { return 1 + 10 + text_w(cr, l, PG()->tmt_fs[0], 400) + 10 + 1; }
static double cell_w(cairo_t *cr, Row *r, int c)
{
	const RuiPage *p = PG(); double w = p->td_pad[1] + r->padl[c] + text_w(cr, r->c[c] ? r->c[c] : "", p->tmt_fs[0], r->bold[c] ? 700 : 400) + p->td_pad[3];
	if (r->btn[c]) { w = p->td_pad[1] + p->td_pad[3]; char **v = g_strsplit(r->btn[c], "\n", -1); for (int k = 0; v[k]; k++) w += btn_w(cr, v[k]) + (k ? 4 : 0); g_strfreev(v); }
	return w;
}
/* the lines a cell's writing takes in its width (a table that wraps: .tmt's cells, white-space normal) */
static int cell_lines(cairo_t *cr, const char *s, double w, int wt)
{
	PangoLayout *l = rui_layout(cr, s, PG()->tmt_fs[0], wt); pango_layout_set_width(l, (int)(w * RUI.z * PANGO_SCALE)); pango_layout_set_wrap(l, PANGO_WRAP_WORD_CHAR);
	int n = pango_layout_get_line_count(l); g_object_unref(l); return n < 1 ? 1 : n;
}
/* CSS's automatic table layout (width 100 %): a column given a share keeps it; the others as wide as their widest writing, the width
   left shared out by those widths (or, when it does not fit, by them too, a wrapping table's cells taking more lines) */
static void table_layout(RuiTable *t, cairo_t *cr, double W)
{
	const RuiPage *p = PG(); double mx[8] = {0}, sum = 0, fixed = 0;
	for (int c = 0; c < t->n; c++) if (!t->nohead) mx[c] = p->th_pad[1] + text_w(cr, t->title[c], p->tmt_fs[0], 700) + p->th_pad[3];
	for (guint i = 0; i < t->rows->len; i++) { Row *r = g_ptr_array_index(t->rows, i); if (r->group) continue;
		for (int c = 0; c < t->n; c++) { double w = cell_w(cr, r, c); if (w > mx[c]) mx[c] = w; } }
	for (int c = 0; c < t->n; c++) if (t->width[c] > 0 && t->width[c] < 1) fixed += t->width[c] * W; else sum += mx[c];
	double rest = W - fixed, x = 0; t->colx[0] = 0;
	for (int c = 0; c < t->n; c++) { double w = t->width[c] > 0 && t->width[c] < 1 ? t->width[c] * W : sum > 0 ? (rest >= sum ? mx[c] + (rest - sum) * mx[c] / sum : rest * mx[c] / sum) : 0;
		x += w; t->colx[c + 1] = x; }
	/* the rows' places */
	double y = head_h(t), lh = line_h(p->tmt_fs[0], 400);
	for (guint i = 0; i < t->rows->len; i++) { Row *r = g_ptr_array_index(t->rows, i); int lines = 1;
		if (t->wrap && !r->group) for (int c = 0; c < t->n; c++) if (!r->btn[c] && r->c[c] && *r->c[c]) { int k = cell_lines(cr, r->c[c], t->colx[c + 1] - t->colx[c] - p->td_pad[1] - p->td_pad[3] - r->padl[c], r->bold[c] ? 700 : 400); if (k > lines) lines = k; }
		r->y = y; r->h = p->td_pad[0] + lines * lh + p->td_pad[2] + p->td_bw[0]; if (r->btn[0] || r->btn[1] || r->btn[2] || r->btn[3] || r->btn[4] || r->btn[5] || r->btn[6] || r->btn[7]) r->h = fmax(r->h, p->td_pad[0] + lh + 6 + 2 + p->td_pad[2] + p->td_bw[0]);   /* (a cell with the page's buttons: 23 px of button in its padding) */
		y += r->h; }
	t->H = y; t->lastW = W;
}
static void tsize(RuiTable *t)
{
	/* (the rows' heights: a row of buttons is taller; a wrapping table's rows as its width gives them, once it has one) */
	double keep[9], lw = t->lastW; memcpy(keep, t->colx, sizeof keep);   /* (the columns as drawn kept: this only measures) */
	cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_A8, 1, 1); cairo_t *cr = cairo_create(s); table_layout(t, cr, t->lastW > 0 ? t->lastW : 600); cairo_destroy(cr); cairo_surface_destroy(s);
	memcpy(t->colx, keep, sizeof keep); if (lw > 0) t->lastW = lw;
	gtk_widget_set_size_request(t->w, -1, (int)ceil(rui_px(t->H)));
}
static void table_alloc(GtkWidget *w, GdkRectangle *a, gpointer d)
{
	RuiTable *t = d; (void)w; double W = a->width / RUI.z; if (!t->wrap || fabs(W - t->lastW) < .5) return; t->lastW = W; tsize(t);
}
static void cell_text(cairo_t *cr, const char *s, double x, double y, double w, double h, double fs, int wt, int align, int wrap)
{
	if (!wrap) { text_in(cr, s, x, y, w, h, fs, wt, align); return; }
	PangoLayout *l = rui_layout(cr, s, fs, wt); pango_layout_set_width(l, (int)(w * RUI.z * PANGO_SCALE)); pango_layout_set_wrap(l, PANGO_WRAP_WORD_CHAR);
	if (align == RUI_RIGHT) pango_layout_set_alignment(l, PANGO_ALIGN_RIGHT);
	int n = pango_layout_get_line_count(l), lh = (int)lround(rui_px(line_h(fs, wt)));
	for (int i = 0; i < n; i++) { PangoLayoutLine *ln = pango_layout_get_line_readonly(l, i); PangoRectangle lg; pango_layout_line_get_pixel_extents(ln, NULL, &lg);
		double tx = x * RUI.z + (align == RUI_RIGHT ? w * RUI.z - lg.width : 0); PangoRectangle ink2, lg2; pango_layout_line_get_pixel_extents(ln, &ink2, &lg2);
		cairo_move_to(cr, round(tx), round(y * RUI.z) + i * lh + round((lh - lg.height) / 2.0) - lg.y); pango_cairo_show_layout_line(cr, ln); }
	g_object_unref(l);
}
static gboolean table_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	RuiTable *t = d; const RuiPage *p = PG(); double z = RUI.z, W = gtk_widget_get_allocated_width(w) / z, fs = p->tmt_fs[0], hh = head_h(t);
	table_layout(t, cr, W);
	GtkStyleContext *sc = gtk_widget_get_style_context(w); GdkRGBA fg; gtk_style_context_get_color(sc, gtk_style_context_get_state(sc), &fg);
	/* the heading row */
	if (!t->nohead) {
		setc(cr, p->th_bg, 1); cairo_rectangle(cr, 0, 0, W * z, (hh - p->th_bw[0]) * z); cairo_fill(cr);
		setc(cr, p->th_bd, 1); cairo_rectangle(cr, 0, (hh - p->th_bw[0]) * z, W * z, p->th_bw[0] * z); cairo_fill(cr);
		for (int c = 0; c < t->n; c++) { gdk_cairo_set_source_rgba(cr, &fg);
			text_in(cr, t->title[c], t->colx[c] + p->th_pad[3], p->th_pad[0], t->colx[c + 1] - t->colx[c] - p->th_pad[1] - p->th_pad[3], line_h(fs, 700), fs, 700, t->align[c]); }
	}
	double lh = line_h(fs, 400);
	for (guint i = 0; i < t->rows->len; i++) {
		Row *r = g_ptr_array_index(t->rows, i); double y = r->y, rh = r->h; int on = !r->group && r->id == t->sel && t->sel >= 0, hov = !on && !r->group && (int)i == t->hov;
		if (r->group) { setc(cr, p->bg2, 1); cairo_rectangle(cr, 0, y * z, W * z, (rh - p->td_bw[0]) * z); cairo_fill(cr); }
		if (on) { rui_color(cr, "ac", 1); cairo_rectangle(cr, 0, y * z, W * z, (rh - p->td_bw[0]) * z); cairo_fill(cr); }
		if (hov) { setc(cr, p->tr_hov, 1); cairo_rectangle(cr, 0, y * z, W * z, (rh - p->td_bw[0]) * z); cairo_fill(cr); }
		setc(cr, p->td_bd, 1); cairo_rectangle(cr, 0, (y + rh - p->td_bw[0]) * z, W * z, p->td_bw[0] * z); cairo_fill(cr);
		double ty = y + p->td_pad[0], th = rh - p->td_pad[0] - p->td_pad[2] - p->td_bw[0];
		if (r->group) {
			gdk_cairo_set_source_rgba(cr, &fg); double bw = text_w(cr, r->c[0], fs, 700), sp = text_w(cr, " ", fs, 400);
			text_in(cr, r->c[0], p->td_pad[3], ty, W, lh, fs, 700, 0);
			if (RUI.dark) setc(cr, p->mut, 1); else gdk_cairo_set_source_rgba(cr, &fg);
			text_in(cr, r->c[1] ? r->c[1] : "", p->td_pad[3] + bw + sp, ty, W, lh, fs, 400, 0); continue; }
		for (int c = 0; c < t->n; c++) {
			double cx = t->colx[c] + p->td_pad[3] + r->padl[c], cw = t->colx[c + 1] - t->colx[c] - p->td_pad[1] - p->td_pad[3] - r->padl[c];
			if (r->btn[c]) {   /* (buttons in the cell: the page's button, centred in the row) */
				char **v = g_strsplit(r->btn[c], "\n", -1); double bx = cx, bh = lh + 6 + 2, by = y + (rh - p->td_bw[0] - bh) / 2;
				for (int k = 0; v[k]; k++) { double bw = btn_w(cr, v[k]); if (RUI.dark) cairo_set_source_rgb(cr, 30 / 255.0, 41 / 255.0, 59 / 255.0); else cairo_set_source_rgb(cr, 1, 1, 1);
					rrect(cr, (bx + .5) * z, (by + .5) * z, (bw - 1) * z, (bh - 1) * z, 4 * z); cairo_fill_preserve(cr); if (RUI.dark) cairo_set_source_rgb(cr, .2, .255, .333); else setc(cr, p->bd, 1); cairo_set_line_width(cr, z); cairo_stroke(cr);
					if (RUI.dark) cairo_set_source_rgb(cr, .886, .91, .941); else cairo_set_source_rgb(cr, 0, 0, 0); text_in(cr, v[k], bx + 1 + 10, by + 4, bw - 22, lh, fs, 400, 0); bx += bw + 4; }
				g_strfreev(v); continue; }
			if (on) rui_color(cr, "acfg", 1); else if (r->muted[c]) setc(cr, p->mut, 1); else gdk_cairo_set_source_rgba(cr, &fg);
			/* (vertical-align: middle, as .tmt td) */
			if (t->wrap) { int k = cell_lines(cr, r->c[c] ? r->c[c] : "", cw, r->bold[c] ? 700 : 400); cell_text(cr, r->c[c] ? r->c[c] : "", cx, ty + (th - k * lh) / 2, cw, k * lh, fs, r->bold[c] ? 700 : 400, t->align[c], 1); }
			else text_in(cr, r->c[c] ? r->c[c] : "", cx, ty + (th - lh) / 2, cw, lh, fs, r->bold[c] ? 700 : 400, t->align[c]);
		}
	}
	return TRUE;
}
static int row_at(RuiTable *t, double y) { y /= RUI.z; if (y < head_h(t)) return -1; for (guint i = 0; i < t->rows->len; i++) { Row *r = g_ptr_array_index(t->rows, i); if (y >= r->y && y < r->y + r->h) return i; } return -2; }
static gboolean table_press(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	RuiTable *t = d; if (t->row || t->head) gtk_widget_grab_focus(w); int i = row_at(t, e->y);
	if (i == -1 && e->button == 1) { double x = e->x / RUI.z; for (int c = 0; c < t->n; c++) if (x >= t->colx[c] && x < t->colx[c + 1]) { if (t->head) t->head(c, t->d); } return TRUE; }
	if (i < 0) return TRUE; Row *r = g_ptr_array_index(t->rows, i); if (r->group) return TRUE;
	if (e->button == 1) { double x = e->x / RUI.z; for (int c = 0; c < t->n; c++) if (r->btn[c] && x >= t->colx[c] && x < t->colx[c + 1]) {
			cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_A8, 1, 1); cairo_t *cr = cairo_create(s); char **v = g_strsplit(r->btn[c], "\n", -1);
			double bx = t->colx[c] + PG()->td_pad[3]; int hit = -1; for (int k = 0; v[k]; k++) { double bw = btn_w(cr, v[k]); if (x >= bx && x < bx + bw) hit = k; bx += bw + 4; }
			g_strfreev(v); cairo_destroy(cr); cairo_surface_destroy(s); if (hit >= 0 && t->btn) { t->pend = 1; t->pid_ = r->id; t->pwhich = c * 16 + hit; } return TRUE; } }   /* (done on the release, as a button is clicked) */
	if (e->button == 1 || e->button == 3) { if (t->row) t->row(r->id, e->button == 3, e->x_root, e->y_root, t->d); }
	return TRUE;
}
static gboolean table_release(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	RuiTable *t = d; (void)w; (void)e; if (!t->pend) return FALSE; t->pend = 0; long id = t->pid_; int which = t->pwhich;
	if (t->btn) t->btn(id, which, t->bd); return TRUE;
}
static gboolean table_motion(GtkWidget *w, GdkEventMotion *e, gpointer d) { RuiTable *t = d; int i = row_at(t, e->y); if (i != t->hov) { t->hov = i; gtk_widget_queue_draw(w); } return FALSE; }
static gboolean table_leave(GtkWidget *w, GdkEventCrossing *e, gpointer d) { (void)e; RuiTable *t = d; if (t->hov != -3) { t->hov = -3; gtk_widget_queue_draw(w); } return FALSE; }
static gboolean table_key(GtkWidget *w, GdkEventKey *e, gpointer d)
{
	RuiTable *t = d; (void)w; if (t->sel < 0 || !t->row) return FALSE;
	if (e->keyval == GDK_KEY_Return || e->keyval == GDK_KEY_KP_Enter) { t->row(t->sel, 2, 0, 0, t->d); return TRUE; }
	if (e->keyval == GDK_KEY_Delete) { t->row(t->sel, e->state & GDK_SHIFT_MASK ? 4 : 3, 0, 0, t->d); return TRUE; }
	if (e->keyval == GDK_KEY_Up || e->keyval == GDK_KEY_Down) { int at = -1; for (guint i = 0; i < t->rows->len; i++) { Row *r = g_ptr_array_index(t->rows, i); if (!r->group && r->id == t->sel) at = i; }
		for (int i = at + (e->keyval == GDK_KEY_Up ? -1 : 1); at >= 0 && i >= 0 && i < (int)t->rows->len; i += e->keyval == GDK_KEY_Up ? -1 : 1) { Row *r = g_ptr_array_index(t->rows, i); if (!r->group) { t->row(r->id, 5, 0, 0, t->d); break; } }
		return TRUE; }
	return FALSE;
}
static void table_free(GtkWidget *w, gpointer d) { RuiTable *t = d; (void)w; g_ptr_array_unref(t->rows); for (int c = 0; c < 8; c++) g_free(t->title[c]); g_free(t); }
static void table_re(void *d) { RuiTable *t = d; t->lastW = 0; tsize(t); gtk_widget_queue_draw(t->w); }
RuiTable *rui_table_new(int n)
{
	RuiTable *t = g_new0(RuiTable, 1); t->n = MIN(n, 8); t->rows = g_ptr_array_new_with_free_func(row_free); t->sel = -1; t->hov = -3;
	t->w = gtk_drawing_area_new(); gtk_widget_set_can_focus(t->w, TRUE); gtk_widget_add_events(t->w, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_KEY_PRESS_MASK);
	g_signal_connect(t->w, "draw", G_CALLBACK(table_draw), t); g_signal_connect(t->w, "button-press-event", G_CALLBACK(table_press), t); g_signal_connect(t->w, "button-release-event", G_CALLBACK(table_release), t);
	g_signal_connect(t->w, "motion-notify-event", G_CALLBACK(table_motion), t); g_signal_connect(t->w, "leave-notify-event", G_CALLBACK(table_leave), t);
	g_signal_connect(t->w, "key-press-event", G_CALLBACK(table_key), t); g_signal_connect(t->w, "size-allocate", G_CALLBACK(table_alloc), t);
	g_signal_connect_after(t->w, "destroy", G_CALLBACK(table_free), t);   /* (the table goes with its widget) */
	for (int c = 0; c < t->n; c++) t->title[c] = g_strdup("");
	tsize(t); on_settings_w(t->w, table_re, t); return t;
}
GtkWidget *rui_table_widget(RuiTable *t) { return t->w; }
void rui_table_nohead(RuiTable *t, int on) { t->nohead = on; }
int rui_table_button_rect(RuiTable *t, long id, int col, int k, GdkRectangle *o)
{
	memset(o, 0, sizeof *o); cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_A8, 1, 1); cairo_t *cr = cairo_create(s); int ok = 0;
	for (guint i = 0; i < t->rows->len && !ok; i++) { Row *r = g_ptr_array_index(t->rows, i); if (r->group || r->id != id || !r->btn[col]) continue; char **v = g_strsplit(r->btn[col], "\n", -1);
		double bx = t->colx[col] + PG()->td_pad[3], lh = line_h(PG()->tmt_fs[0], 400), bh = lh + 8;
		for (int j = 0; v[j]; j++) { double bw = btn_w(cr, v[j]); if (j == k) { int x, y; gtk_widget_translate_coordinates(t->w, gtk_widget_get_toplevel(t->w), (int)lround(rui_px(bx)), (int)lround(rui_px(r->y + (r->h - PG()->td_bw[0] - bh) / 2)), &x, &y);
				o->x = x; o->y = y; o->width = (int)lround(rui_px(bw)); o->height = (int)lround(rui_px(bh)); ok = 1; } bx += bw + 4; }
		g_strfreev(v); }
	cairo_destroy(cr); cairo_surface_destroy(s); return ok;
}
double rui_table_col_x(RuiTable *t, int c) { return c >= 0 && c <= t->n ? rui_px(t->colx[c]) : 0; }
void rui_table_wrap(RuiTable *t, int on) { t->wrap = on; }
void rui_table_col(RuiTable *t, int c, const char *title, int align, double width) { if (c < 0 || c >= t->n) return; g_free(t->title[c]); t->title[c] = g_strdup(rui_tr(title)); t->align[c] = align; t->width[c] = width; }
void rui_table_heading(RuiTable *t, int c, const char *title) { if (c < 0 || c >= t->n) return; g_free(t->title[c]); t->title[c] = g_strdup(title); }
void rui_table_clear(RuiTable *t) { g_ptr_array_set_size(t->rows, 0); t->hov = -3; }
int rui_table_add(RuiTable *t, const char *const *cells, long id)
{
	Row *r = g_new0(Row, 1); r->c = g_new0(char *, t->n + 1); for (int c = 0; c < t->n; c++) r->c[c] = g_strdup(cells[c] ? cells[c] : ""); r->id = id;
	g_ptr_array_add(t->rows, r); return t->rows->len - 1;
}
int rui_table_group(RuiTable *t, const char *bold, const char *rest)
{
	Row *r = g_new0(Row, 1); r->c = g_new0(char *, MAX(t->n, 2) + 1); r->c[0] = g_strdup(bold); r->c[1] = g_strdup(rest); r->group = 1; r->id = -1;
	for (int c = 2; c < t->n; c++) r->c[c] = g_strdup(""); g_ptr_array_add(t->rows, r); return t->rows->len - 1;
}
void rui_table_cell_style(RuiTable *t, int row, int col, int muted, int padl) { if (row < 0 || row >= (int)t->rows->len || col < 0 || col >= t->n) return; Row *r = g_ptr_array_index(t->rows, row); r->muted[col] = muted; r->padl[col] = padl; }
void rui_table_cell_bold(RuiTable *t, int row, int col) { if (row < 0 || row >= (int)t->rows->len || col < 0 || col >= t->n) return; ((Row *)g_ptr_array_index(t->rows, row))->bold[col] = 1; }
void rui_table_set_button(RuiTable *t, int row, int col, const char *label, void (*cb)(long, int, void *), void *d)
{
	if (row < 0 || row >= (int)t->rows->len || col < 0 || col >= t->n) return; Row *r = g_ptr_array_index(t->rows, row); g_free(r->btn[col]); r->btn[col] = g_strdup(label); t->btn = cb; t->bd = d;
}
void rui_table_select(RuiTable *t, long id) { t->sel = id; gtk_widget_queue_draw(t->w); }
long rui_table_selected(RuiTable *t) { return t->sel; }
void rui_table_done(RuiTable *t) { t->lastW = t->wrap ? gtk_widget_get_allocated_width(t->w) / RUI.z : 0; if (t->lastW < 2) t->lastW = 0; tsize(t); gtk_widget_queue_draw(t->w); }
void rui_table_on(RuiTable *t, void (*row)(long, int, double, double, void *), void (*head)(int, void *), void *d) { t->row = row; t->head = head; t->d = d; }
void rui_table_rect(RuiTable *t, long id, GdkRectangle *o)
{
	memset(o, 0, sizeof *o);
	for (guint i = 0; i < t->rows->len; i++) { Row *r = g_ptr_array_index(t->rows, i); if (!r->group && r->id == id) { int x, yy; gtk_widget_translate_coordinates(t->w, gtk_widget_get_toplevel(t->w), 0, (int)lround(rui_px(r->y)), &x, &yy);
		o->x = x; o->y = yy; o->width = gtk_widget_get_allocated_width(t->w); o->height = (int)lround(rui_px(r->h)); return; } }
}

/* ------------------------------------------------------------------ the right-click menu (the page's cmenu: .ctx.cmenu) */
typedef struct { GtkWidget *win; char *title; RuiMenuItem *it; int n, hov; void (*cb)(int, void *); void *d; double w, h; } Menu;
static Menu *MENU;
static double mi_h(const RuiMenuItem *m) { const RuiPage *p = PG(); return m->sep ? p->hr_m[0] + p->hr_bw[0] + p->hr_m[2] : p->cmi_pad[0] + line_h(p->ctx_fs[0], 400) + p->cmi_pad[2]; }
static double mh_h(void) { const RuiPage *p = PG(); return p->cmh_pad[0] + line_h(p->cmh_fs[0], 600) + p->cmh_pad[2]; }
static void menu_close(void) { if (!MENU) return; Menu *m = MENU; MENU = NULL; gtk_widget_destroy(m->win); for (int i = 0; i < m->n; i++) { g_free((char *)m->it[i].label); g_free((char *)m->it[i].icon); g_free((char *)m->it[i].key); } g_free(m->it); g_free(m->title); g_free(m); }
static gboolean menu_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Menu *m = d; const RuiPage *p = PG(); double z = RUI.z, W = m->w, H = m->h, b = p->ctx_bw[0]; (void)w;
	/* (no compositor: what is behind the corners is the window's own; the box over it) */
	setc(cr, p->ctx_bg, 1); cairo_paint(cr);
	cairo_save(cr); cairo_scale(cr, z, z);
	setc(cr, p->ctx_bd, 1); cairo_set_line_width(cr, b); rrect(cr, b / 2, b / 2, W - b, H - b, p->ctx_r[0] - b / 2); cairo_stroke(cr);
	cairo_restore(cr);
	GdkRGBA fg; { RuiC c = pcol(p->ctx_fg); fg = (GdkRGBA){ c.r, c.g, c.b, 1 }; }
	double y = b + p->ctx_pad[0], x = b + p->ctx_pad[3], iw = W - 2 * b - p->ctx_pad[1] - p->ctx_pad[3];
	if (m->title) { setc(cr, p->cmh_fg, 1); text_in(cr, m->title, x + p->cmh_pad[3], y + p->cmh_pad[0], iw - p->cmh_pad[3] - p->cmh_pad[1], line_h(p->cmh_fs[0], 600), p->cmh_fs[0], 600, 0); y += mh_h(); }
	for (int i = 0; i < m->n; i++) {
		const RuiMenuItem *it = &m->it[i]; double h = mi_h(it);
		if (it->sep) { setc(cr, p->hr, 1); cairo_rectangle(cr, x * z, (y + p->hr_m[0]) * z, iw * z, p->hr_bw[0] * z); cairo_fill(cr); y += h; continue; }
		int hi = i == m->hov && !it->off; double a = it->off ? p->cmi_dis[0] : 1;
		if (hi) { rui_color(cr, "ac", 1); rrect(cr, x * z, y * z, iw * z, h * z, p->cmi_r[0] * z); cairo_fill(cr); }
		double lh = line_h(p->ctx_fs[0], 400), tx = x + p->cmi_pad[3], ty = y + p->cmi_pad[0];
		if (hi) cairo_set_source_rgb(cr, 1, 1, 1); else cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, a);
		if (it->icon && *it->icon) text_in(cr, it->icon, tx, ty, p->cmi_i[0], lh, p->ctx_fs[0], 400, 2);
		tx += p->cmi_i[0] + p->cmi_gap[0];
		/* the key's pill (kbd: its border, the page's grey, even with no key in it), at the right */
		double kfs = p->kbd_fs[0], kt = it->key && *it->key ? text_w(cr, it->key, kfs, 400) : 0, kw = p->kbd_pad[1] + kt + p->kbd_pad[3] + 2, kh = line_h(kfs, 400) + 1 + 2;
		double kx = x + iw - p->cmi_pad[1] - kw, ky = ty + (lh - kh) / 2 + 1;
		cairo_push_group(cr);
		setc(cr, p->kbd_bg, 1); rrect(cr, kx * z, ky * z, kw * z, kh * z, p->kbd_r[0] * z); cairo_fill(cr);
		setc(cr, p->kbd_bd, 1); cairo_set_line_width(cr, z); rrect(cr, (kx + .5) * z, (ky + .5) * z, (kw - 1) * z, (kh - 1.5) * z, p->kbd_r[0] * z); cairo_stroke(cr);
		cairo_rectangle(cr, kx * z + p->kbd_r[0] * z, (ky + kh - 2) * z, (kw - 2 * p->kbd_r[0]) * z, 2 * z); cairo_fill(cr);
		if (kt > 0) { if (hi) cairo_set_source_rgb(cr, 1, 1, 1); else cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, 1); text_in(cr, it->key, kx + 1 + p->kbd_pad[3], ky + 1, kt + 1, kh - 3, kfs, 400, 0); }
		cairo_pop_group_to_source(cr); cairo_paint_with_alpha(cr, p->kbd_op[0] * a);
		if (hi) cairo_set_source_rgb(cr, 1, 1, 1); else cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, a);
		text_in(cr, it->label, tx, ty, kx - p->kbd_ml[0] - tx, lh, p->ctx_fs[0], 400, 0);
		y += h;
	}
	return TRUE;
}
static int menu_at(Menu *m, double yy)
{
	const RuiPage *p = PG(); double y = p->ctx_bw[0] + p->ctx_pad[0] + (m->title ? mh_h() : 0), q = yy / RUI.z;
	for (int i = 0; i < m->n; i++) { double h = mi_h(&m->it[i]); if (q >= y && q < y + h) return m->it[i].sep ? -1 : i; y += h; }
	return -1;
}
static gboolean menu_motion(GtkWidget *w, GdkEventMotion *e, gpointer d) { Menu *m = d; int i = menu_at(m, e->y); if (i != m->hov) { m->hov = i; gtk_widget_queue_draw(w); } return TRUE; }
static gboolean menu_release(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	Menu *m = d; (void)w; int ins = e->x >= 0 && e->y >= 0 && e->x < m->w * RUI.z && e->y < m->h * RUI.z;
	if (!ins) { if (e->type == GDK_BUTTON_PRESS) menu_close(); return TRUE; }
	if (e->type != GDK_BUTTON_RELEASE) return TRUE;
	int i = menu_at(m, e->y); if (i < 0 || m->it[i].off) return TRUE;
	void (*cb)(int, void *) = m->cb; void *dd = m->d; menu_close(); if (cb) cb(i, dd); return TRUE;
}
static gboolean menu_key(GtkWidget *w, GdkEventKey *e, gpointer d)
{
	Menu *m = d; if (e->keyval == GDK_KEY_Escape) { menu_close(); return TRUE; }
	if (e->keyval == GDK_KEY_Down || e->keyval == GDK_KEY_Up) { int st = e->keyval == GDK_KEY_Down ? 1 : -1, i = m->hov;
		for (int k = 0; k < m->n; k++) { i = (i + st + m->n) % m->n; if (!m->it[i].sep && !m->it[i].off) break; } m->hov = i; gtk_widget_queue_draw(w); return TRUE; }
	if ((e->keyval == GDK_KEY_Return || e->keyval == GDK_KEY_KP_Enter) && m->hov >= 0 && !m->it[m->hov].off) { int i = m->hov; void (*cb)(int, void *) = m->cb; void *dd = m->d; menu_close(); if (cb) cb(i, dd); return TRUE; }
	return FALSE;
}
void rui_menu(GtkWidget *rel, double x, double y, const char *title, const RuiMenuItem *items, int n, void (*cb)(int, void *), void *d)
{
	menu_close(); Menu *m = g_new0(Menu, 1); MENU = m; m->n = n; m->cb = cb; m->d = d; m->hov = -1; m->title = title ? g_strdup(title) : NULL; m->it = g_new0(RuiMenuItem, n);
	for (int i = 0; i < n; i++) { m->it[i] = items[i]; m->it[i].label = g_strdup(items[i].label ? rui_tr(items[i].label) : ""); m->it[i].icon = g_strdup(items[i].icon ? items[i].icon : ""); m->it[i].key = g_strdup(items[i].key ? items[i].key : ""); }
	const RuiPage *p = PG(); cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_A8, 1, 1); cairo_t *cr = cairo_create(s);
	double iw = p->ctx_minw[0] - 2 * p->ctx_bw[0] - p->ctx_pad[1] - p->ctx_pad[3], h = 2 * p->ctx_bw[0] + p->ctx_pad[0] + p->ctx_pad[2] + (m->title ? mh_h() : 0);
	if (m->title) iw = fmax(iw, fmin(260, p->cmh_pad[3] + text_w(cr, m->title, p->cmh_fs[0], 600) + p->cmh_pad[1]));
	for (int i = 0; i < n; i++) { const RuiMenuItem *it = &m->it[i]; h += mi_h(it); if (it->sep) continue;
		double kt = it->key && *it->key ? text_w(cr, it->key, p->kbd_fs[0], 400) : 0;
		iw = fmax(iw, p->cmi_pad[3] + p->cmi_i[0] + p->cmi_gap[0] + text_w(cr, it->label, p->ctx_fs[0], 400) + p->cmi_gap[0] + p->kbd_ml[0] + p->kbd_pad[1] + kt + p->kbd_pad[3] + 2 + p->cmi_pad[1]); }
	cairo_destroy(cr); cairo_surface_destroy(s);
	m->w = ceil(iw + 2 * p->ctx_bw[0] + p->ctx_pad[1] + p->ctx_pad[3]); m->h = h;
	m->win = gtk_window_new(GTK_WINDOW_POPUP); gtk_window_set_type_hint(GTK_WINDOW(m->win), GDK_WINDOW_TYPE_HINT_POPUP_MENU);
	if (rel) gtk_window_set_transient_for(GTK_WINDOW(m->win), GTK_WINDOW(gtk_widget_get_toplevel(rel)));
	GtkWidget *a = gtk_drawing_area_new(); gtk_container_add(GTK_CONTAINER(m->win), a); gtk_widget_set_size_request(a, (int)ceil(m->w * RUI.z), (int)ceil(m->h * RUI.z));
	gtk_widget_add_events(a, GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
	g_signal_connect(a, "draw", G_CALLBACK(menu_draw), m); g_signal_connect(a, "motion-notify-event", G_CALLBACK(menu_motion), m);
	g_signal_connect(a, "button-release-event", G_CALLBACK(menu_release), m); g_signal_connect(a, "button-press-event", G_CALLBACK(menu_release), m);
	g_signal_connect(m->win, "key-press-event", G_CALLBACK(menu_key), m);
	/* kept on the screen, as the page keeps its menus in the window */
	GdkRectangle mon = { 0, 0, 1 << 15, 1 << 15 }; GdkDisplay *dp = gdk_display_get_default(); GdkMonitor *mo = gdk_display_get_monitor_at_point(dp, (int)x, (int)y); if (mo) gdk_monitor_get_geometry(mo, &mon);
	int wx = (int)x, wy = (int)y, ww = (int)ceil(m->w * RUI.z), wh = (int)ceil(m->h * RUI.z);
	if (wx + ww > mon.x + mon.width) wx = MAX(mon.x, wx - ww); if (wy + wh > mon.y + mon.height) wy = MAX(mon.y, mon.y + mon.height - wh);
	gtk_window_move(GTK_WINDOW(m->win), wx, wy); gtk_widget_show_all(m->win);
	GdkWindow *gw = gtk_widget_get_window(m->win); GdkSeat *seat = gdk_display_get_default_seat(dp);
	if (gw) gdk_seat_grab(seat, gw, GDK_SEAT_CAPABILITY_ALL, TRUE, NULL, NULL, NULL, NULL);
}
GtkWidget *rui_menu_window(void) { return MENU ? MENU->win : NULL; }

/* ------------------------------------------------------------------ dialogs over the window's body (the page's sdlg, .dlg) */
typedef struct { GtkWidget *dim, *box; RuiWin *w; void (*cb)(int, void *); void *d; int n; GtkWidget *entry; GtkWidget *who; GtkWidget *err; char *reason; void (*cred)(const char *, const char *, void *); } Dlg;   /* (cred: rui_elevate_cred's) */
static Dlg *DLG;
static void dlg_end(int i)
{
	if (!DLG) return; Dlg *g = DLG; DLG = NULL; gtk_widget_destroy(g->dim); void (*cb)(int, void *) = g->cb; void *d = g->d; void (*cr)(const char *, const char *, void *) = g->cred; g_free(g->reason); g_free(g); if (cb) cb(i, d); else if (cr && i != 1) cr(NULL, NULL, d);
}
static void on_dlg_esc(void) { dlg_end(-1); }
static void dlg_btn(GtkButton *b, gpointer d) { (void)d; dlg_end(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "rui-i"))); }
static gboolean dim_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)d; GtkStyleContext *c = gtk_widget_get_style_context(w); gtk_render_background(c, cr, 0, 0, gtk_widget_get_allocated_width(w), gtk_widget_get_allocated_height(w)); return FALSE; }
static Dlg *dlg_new(RuiWin *w, const char *title, const char *text, int maxw)
{
	if (DLG) dlg_end(-1);
	Dlg *g = g_new0(Dlg, 1); DLG = g; g->w = w;
	g->dim = gtk_event_box_new(); cls(g->dim, "rui-dim"); g_signal_connect(g->dim, "draw", G_CALLBACK(dim_draw), g);
	g->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); cls(g->box, "rui-dlg"); gtk_widget_set_halign(g->box, GTK_ALIGN_CENTER); gtk_widget_set_valign(g->box, GTK_ALIGN_CENTER);
	gtk_container_add(GTK_CONTAINER(g->dim), g->box);
	GtkWidget *t = gtk_label_new(rui_tr(title)); exact(t); cls(t, "rui-t"); gtk_label_set_xalign(GTK_LABEL(t), 0); gtk_label_set_line_wrap(GTK_LABEL(t), TRUE); gtk_label_set_max_width_chars(GTK_LABEL(t), 1);
	gtk_widget_set_margin_bottom(t, (int)lround(rui_px(8))); gtk_box_pack_start(GTK_BOX(g->box), t, FALSE, FALSE, 0);
	if (text) { GtkWidget *m = gtk_label_new(rui_tr(text)); exact(m); cls(m, "rui-m"); gtk_label_set_xalign(GTK_LABEL(m), 0); gtk_label_set_line_wrap(GTK_LABEL(m), TRUE); gtk_label_set_max_width_chars(GTK_LABEL(m), 1);
		gtk_box_pack_start(GTK_BOX(g->box), m, FALSE, FALSE, 0); g_object_set_data(G_OBJECT(g->box), "rui-text", m); }
	g_object_set_data(G_OBJECT(g->box), "rui-maxw", GINT_TO_POINTER(maxw));
	return g;
}
/* (the box's width: its writing's, at most MAXW CSS px as the page's max-width) */
static void dlg_show(Dlg *g, GtkWidget *row)
{
	gtk_widget_set_margin_top(row, (int)lround(rui_px(14))); gtk_box_pack_start(GTK_BOX(g->box), row, FALSE, FALSE, 0);
	int maxw = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(g->box), "rui-maxw"));
	gtk_overlay_add_overlay(GTK_OVERLAY(g->w->overlay), g->dim); gtk_widget_show_all(g->dim);
	int nat = 0, mn = 0; GList *ch = gtk_container_get_children(GTK_CONTAINER(g->box));
	for (GList *l = ch; l; l = l->next) { GtkWidget *c = l->data; if (GTK_IS_LABEL(c)) { PangoLayout *pl = gtk_widget_create_pango_layout(c, gtk_label_get_text(GTK_LABEL(c))); int w, h; pango_layout_get_size(pl, &w, &h); g_object_unref(pl); nat = MAX(nat, (int)ceil(w / (double)PANGO_SCALE)); }
		else { gtk_widget_get_preferred_width(c, &mn, NULL); nat = MAX(nat, mn); } }
	g_list_free(ch);
	int mw = (int)lround(rui_px(maxw - 32)), wrap = nat > mw; if (wrap) nat = mw;
	for (GList *l = ch = gtk_container_get_children(GTK_CONTAINER(g->box)); l; l = l->next) if (GTK_IS_LABEL(l->data)) {
		gtk_label_set_line_wrap(GTK_LABEL(l->data), wrap); if (!wrap) gtk_label_set_max_width_chars(GTK_LABEL(l->data), -1); gtk_widget_set_size_request(l->data, wrap ? nat : -1, -1); }
	g_list_free(ch);
}
void rui_confirm(RuiWin *w, const char *title, const char *text, const char *const *buttons, int n, int pri, void (*cb)(int, void *), void *d)
{
	Dlg *g = dlg_new(w, title, text, 380); g->cb = cb; g->d = d; g->n = n;
	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (int)lround(rui_px(6))); gtk_widget_set_halign(row, GTK_ALIGN_END);
	GtkWidget *last = NULL;
	for (int i = 0; i < n; i++) { GtkWidget *b = rui_button(buttons[i], 0); if (pri && i == n - 1) { cls(b, "pri"); }
		g_object_set_data(G_OBJECT(b), "rui-i", GINT_TO_POINTER(i)); g_signal_connect(b, "clicked", G_CALLBACK(dlg_btn), NULL); gtk_box_pack_start(GTK_BOX(row), b, FALSE, FALSE, 0); last = b; }
	dlg_show(g, row);
	if (last) { gtk_window_set_focus_visible(GTK_WINDOW(w->win), TRUE); gtk_widget_grab_focus(last); }   /* (the page focuses its last button: its ring shown) */
}
GtkWidget *rui_dialog_box(void) { return DLG ? DLG->box : NULL; }
/* (the page's dialog() with fields: its rows of a label and its input. The values are read while the dialog still stands
   -- dlg_end destroys it before it calls back -- so a press takes them first; Escape ends it with none taken, which is a
   Cancel, and the fields keep what they were given.) */
typedef struct { RuiField *f; int n; GtkWidget **wd; void (*cb)(int, const RuiField *, void *); void *d; } Form;
/* (a field's own words: an entry's, a choice's, or a check's "1"/"0") */
static char *form_get(GtkWidget *w)
{
	if (!w) return g_strdup("");
	if (GTK_IS_ENTRY(w)) return g_strdup(gtk_entry_get_text(GTK_ENTRY(w)));
	if (GTK_IS_COMBO_BOX(w)) { const char *id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(w)); return g_strdup(id ? id : ""); }
	return g_strdup(rui_check_get(w) ? "1" : "0");
}
static void form_take(Form *g)
{
	for (int i = 0; i < g->n; i++) { char *v = form_get(g->wd[i]); g_free((gpointer)g->f[i].value); g->f[i].value = v; }
}
static void form_done(int i, void *d)
{
	Form *g = d;
	if (g->cb) g->cb(i, g->f, g->d);
	for (int k = 0; k < g->n; k++) { g_free((gpointer)g->f[k].value); g_free((gpointer)g->f[k].label); }
	g_free(g->f); g_free(g->wd); g_free(g);
}
static void form_click(GtkButton *b, gpointer d)
{
	Form *g = d; form_take(g);   /* (the fields are read while they still stand: the dialog goes below them) */
	dlg_end(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "rui-i")));
}
/* (the page's file input: the path in its entry, and the button that chooses one) */
static void form_choose(GtkButton *b, gpointer d)
{
	(void)b; GtkWidget *e = d;
	GtkWidget *f = gtk_file_chooser_dialog_new(rui_tr("Choose a file"), DLG ? GTK_WINDOW(DLG->w->win) : NULL,
		GTK_FILE_CHOOSER_ACTION_OPEN, rui_tr("Cancel"), GTK_RESPONSE_CANCEL, rui_tr("Choose"), GTK_RESPONSE_ACCEPT, NULL);
	if (gtk_dialog_run(GTK_DIALOG(f)) == GTK_RESPONSE_ACCEPT) {
		char *p = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(f));
		if (p) { gtk_entry_set_text(GTK_ENTRY(e), p); g_free(p); }
	}
	gtk_widget_destroy(f);
}
GtkWidget *rui_form_field(GtkWidget *box, int i)
{
	Form *g = box ? g_object_get_data(G_OBJECT(box), "rui-form") : NULL;
	return g && i >= 0 && i < g->n ? g->wd[i] : NULL;
}
void rui_form_value(GtkWidget *w, const char *v)
{
	if (!w) return;
	if (GTK_IS_ENTRY(w)) { gtk_entry_set_text(GTK_ENTRY(w), v ? v : ""); return; }
	if (GTK_IS_COMBO_BOX(w)) {   /* (a choice's own words, as the form was given them: its ids, not its translated labels) */
		GtkTreeModel *m = gtk_combo_box_get_model(GTK_COMBO_BOX(w)); GtkTreeIter it; gboolean ok;
		for (ok = gtk_tree_model_get_iter_first(m, &it); ok; ok = gtk_tree_model_iter_next(m, &it)) {
			char *id = NULL; gtk_tree_model_get(m, &it, 1, &id, -1);
			int hit = id && v && !strcmp(id, v); g_free(id);
			if (hit) { gtk_combo_box_set_active_iter(GTK_COMBO_BOX(w), &it); return; }
		}
		gtk_combo_box_set_active(GTK_COMBO_BOX(w), 0); return;
	}
	rui_check_set(w, v && *v && strcmp(v, "0"));
}
void rui_form(RuiWin *w, const char *title, const char *text, const RuiField *f, int n, const char *const *buttons, int nb,
	int pri, void (*cb)(int i, const RuiField *f, void *d), void *d)
{
	Dlg *g = dlg_new(w, title, text, 380); g->n = nb;
	Form *fm = g_new0(Form, 1); fm->n = n; fm->cb = cb; fm->d = d;
	fm->f = g_new0(RuiField, n > 0 ? (gsize)n : 1);
	fm->wd = g_new0(GtkWidget *, n > 0 ? (gsize)n : 1);
	g->cb = form_done; g->d = fm;
	/* (the form's own fields: its caller's array is a dialog's arguments, and may be a frame the call that opened the dialog
	   does not keep -- the press on a button comes long after it returned) */
	for (int i = 0; i < n; i++) {
		fm->f[i].label = g_strdup(f[i].label ? f[i].label : "");
		fm->f[i].kind = f[i].kind;
		fm->f[i].value = g_strdup(f[i].value ? f[i].value : "");
		fm->f[i].opts = f[i].opts; fm->f[i].n = f[i].n;   /* (a choice's own list: its caller's, standing as long as the dialog does) */
	}
	g_object_set_data(G_OBJECT(g->box), "rui-form", fm);   /* (rui_form_field's, and the check's way into a field) */
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, (int)lround(rui_px(8)));
	for (int i = 0; i < n; i++) {
		GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (int)lround(rui_px(8)));
		GtkWidget *lab = gtk_label_new(rui_tr(fm->f[i].label)); exact(lab); cls(lab, "rui-m");
		gtk_label_set_xalign(GTK_LABEL(lab), 0); gtk_widget_set_size_request(lab, (int)lround(rui_px(110)), -1);
		gtk_box_pack_start(GTK_BOX(row), lab, FALSE, FALSE, 0);
		GtkWidget *wd = NULL, *pack = NULL;
		if (fm->f[i].kind == RUI_F_CHECK) { wd = rui_check("", fm->f[i].value[0] == '1', NULL, NULL); pack = wd; }
		else if (fm->f[i].kind == RUI_F_SEL) {
			wd = gtk_combo_box_text_new();
			for (int k = 0; k < fm->f[i].n; k++) gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(wd), fm->f[i].opts[k], rui_tr(fm->f[i].opts[k]));
			gtk_combo_box_set_active(GTK_COMBO_BOX(wd), 0); rui_form_value(wd, fm->f[i].value); pack = wd;
		} else if (fm->f[i].kind == RUI_F_FILE) {
			GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (int)lround(rui_px(6)));
			wd = rui_entry(NULL); if (*fm->f[i].value) gtk_entry_set_text(GTK_ENTRY(wd), fm->f[i].value);
			gtk_widget_set_hexpand(wd, TRUE); gtk_box_pack_start(GTK_BOX(h), wd, TRUE, TRUE, 0);
			GtkWidget *b = rui_button("Choose...", 0); g_signal_connect(b, "clicked", G_CALLBACK(form_choose), wd);
			gtk_box_pack_start(GTK_BOX(h), b, FALSE, FALSE, 0); pack = h;
		} else {
			wd = rui_entry(NULL);
			if (fm->f[i].kind == RUI_F_NUM) gtk_entry_set_input_purpose(GTK_ENTRY(wd), GTK_INPUT_PURPOSE_NUMBER);
			if (*fm->f[i].value) gtk_entry_set_text(GTK_ENTRY(wd), fm->f[i].value);
			pack = wd;
		}
		gtk_widget_set_hexpand(pack, TRUE); gtk_box_pack_start(GTK_BOX(row), pack, TRUE, TRUE, 0);
		fm->wd[i] = wd; gtk_box_pack_start(GTK_BOX(box), row, FALSE, FALSE, 0);
	}
	gtk_box_pack_start(GTK_BOX(g->box), box, FALSE, FALSE, 0);
	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (int)lround(rui_px(6))); gtk_widget_set_halign(row, GTK_ALIGN_END);
	GtkWidget *last = NULL;
	for (int i = 0; i < nb; i++) {
		GtkWidget *b = rui_button(buttons[i], 0); if (pri && i == nb - 1) cls(b, "pri");
		g_object_set_data(G_OBJECT(b), "rui-i", GINT_TO_POINTER(i)); g_signal_connect(b, "clicked", G_CALLBACK(form_click), fm);
		gtk_box_pack_start(GTK_BOX(row), b, FALSE, FALSE, 0); last = b;
	}
	dlg_show(g, row);
	if (last) { gtk_window_set_focus_visible(GTK_WINDOW(w->win), TRUE); gtk_widget_grab_focus(last); }
}

/* the administrator's permission (core25.js elevate): the account (an administrator), its password, Allow; rudolfd checks it
   (desktop/admin) as it checks a sign-in, so no app ever holds a password hash; refused attempts are counted there */
static void el_allow(GtkWidget *b, gpointer d)
{
	(void)b; (void)d; Dlg *g = DLG; if (!g || !g->entry) return;
	GString *j = g_string_new("{\"n\":"); char *n = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(g->who)); const char *pw = gtk_entry_get_text(GTK_ENTRY(g->entry));
	GString *q = g_string_new(NULL);
	for (const char *s = n ? n : ""; *s; s++) { unsigned char c = *s; if (c == '"' || c == '\\') g_string_append_printf(q, "\\%c", c); else if (c < 32) g_string_append_printf(q, "\\u%04x", c); else g_string_append_c(q, c); }
	g_string_append_printf(j, "\"%s\",\"p\":\"", q->str); g_string_free(q, TRUE);
	for (const char *s = pw; *s; s++) { unsigned char c = *s; if (c == '"' || c == '\\') g_string_append_printf(j, "\\%c", c); else if (c < 32) g_string_append_printf(j, "\\u%04x", c); else g_string_append_c(j, c); }
	g_string_append(j, "\",\"why\":"); { GString *r = g_string_new("\""); for (const char *s = g->reason ? g->reason : ""; *s; s++) { unsigned char c = *s; if (c == '"' || c == '\\') g_string_append_printf(r, "\\%c", c); else if (c >= 32) g_string_append_c(r, c); } g_string_append_c(r, '"'); g_string_append(j, r->str); g_string_free(r, TRUE); }
	g_string_append(j, "}");
	char err[200]; int c = rui_post("/api/desktop/admin", j->str, err, sizeof err); explicit_bzero(j->str, j->len); g_string_free(j, TRUE); g_free(n);
	if (c == 200 && g->cred) {   /* (the page's elevate({cred:true}): the name and password, checked, handed on for rudolfd to check again with the request; wiped after) */
		void (*cr)(const char *, const char *, void *) = g->cred; void *d = g->d; char *pn = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(g->who)), *pp = g_strdup(gtk_entry_get_text(GTK_ENTRY(g->entry)));
		gtk_entry_set_text(GTK_ENTRY(g->entry), ""); g->cb = NULL; dlg_end(1); cr(pn ? pn : "", pp, d); explicit_bzero(pp, strlen(pp)); g_free(pp); g_free(pn); return; }
	if (c == 200) { dlg_end(1); return; }
	gtk_label_set_text(GTK_LABEL(g->err), c == 429 ? err : rui_tr("Wrong password.")); gtk_entry_set_text(GTK_ENTRY(g->entry), "");
}
static void el_cancel(GtkWidget *b, gpointer d) { (void)b; (void)d; dlg_end(0); }
void rui_elevate(RuiWin *w, const char *reason, void (*cb)(int, void *), void *d)
{
	char t[120]; snprintf(t, sizeof t, "🛡️ %s", rui_tr("Administrator permission"));
	Dlg *g = dlg_new(w, t, reason, 420); g->cb = cb; g->d = d; g->reason = g_strdup(reason);
	GtkWidget *grid = gtk_grid_new(); gtk_grid_set_row_spacing(GTK_GRID(grid), (int)lround(rui_px(6))); gtk_grid_set_column_spacing(GTK_GRID(grid), (int)lround(rui_px(8)));
	gtk_widget_set_margin_top(grid, (int)lround(rui_px(10)));
	g->who = gtk_combo_box_text_new(); cls(g->who, "rui"); g->entry = rui_entry(NULL); gtk_entry_set_visibility(GTK_ENTRY(g->entry), FALSE); gtk_entry_set_input_purpose(GTK_ENTRY(g->entry), GTK_INPUT_PURPOSE_PASSWORD);
	RuiJ *acc = rui_get("/api/desktop/accounts"), *l = rui_jprop(acc, "accounts"); RuiJ *se = rui_get("/api/desktop/session"); const char *me = rui_jstr(se, "n");
	int at = 0, k = 0; for (int i = 0; i < rui_jlen(l); i++) { RuiJ *a = rui_jat(l, i); const char *r = rui_jstr(a, "r"), *n = rui_jstr(a, "n"); if (!r || strcmp(r, "admin") || !n) continue;
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(g->who), n); if (me && !strcmp(me, n)) at = k; k++; }
	gtk_combo_box_set_active(GTK_COMBO_BOX(g->who), at); rui_jfree(acc); rui_jfree(se);
	gtk_grid_attach(GTK_GRID(grid), rui_label("Account"), 0, 0, 1, 1); gtk_grid_attach(GTK_GRID(grid), g->who, 1, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), rui_label("Password"), 0, 1, 1, 1); gtk_grid_attach(GTK_GRID(grid), g->entry, 1, 1, 1, 1); gtk_widget_set_hexpand(g->entry, TRUE);
	gtk_box_pack_start(GTK_BOX(g->box), grid, FALSE, FALSE, 0);
	g->err = gtk_label_new(""); cls(g->err, "rui-msg"); gtk_label_set_xalign(GTK_LABEL(g->err), 0); gtk_box_pack_start(GTK_BOX(g->box), g->err, FALSE, FALSE, 0);
	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (int)lround(rui_px(8))); gtk_widget_set_halign(row, GTK_ALIGN_END);
	GtkWidget *c = rui_button("Cancel", 0), *a = rui_button("Allow", 1); g_signal_connect(c, "clicked", G_CALLBACK(el_cancel), NULL); g_signal_connect(a, "clicked", G_CALLBACK(el_allow), NULL);
	g_signal_connect(g->entry, "activate", G_CALLBACK(el_allow), NULL);
	gtk_box_pack_start(GTK_BOX(row), c, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(row), a, FALSE, FALSE, 0);
	dlg_show(g, row); gtk_widget_grab_focus(g->entry);
}
/* (the page's elevate(reason, {cred:true}): cred(n, p) once an administrator's password is right, the request then sent with them;
   NULL, NULL when cancelled) */
void rui_elevate_cred(RuiWin *w, const char *reason, void (*cred)(const char *n, const char *p, void *d), void *d)
{
	rui_elevate(w, reason, NULL, d); if (DLG) DLG->cred = cred;
}
static void about_ok(GtkWidget *b, gpointer d) { (void)b; (void)d; dlg_end(0); }
void rui_about(RuiWin *w, const char *name, const char *what)
{
	char ver[64] = ""; { FILE *f = fopen("/usr/share/rudolf/VERSION", "r"); if (f) { if (fgets(ver, sizeof ver, f)) ver[strcspn(ver, "\n")] = 0; fclose(f); } }
	char *t = g_strdup_printf("%s\n%s%s%s\n%s", rui_tr(what), rui_tr("Rudolf OS"), ver[0] ? " " : "", ver, rui_tr("A native app of Rudolf OS. GPL-2.0-or-later."));
	Dlg *g = dlg_new(w, name, t, 380); g_free(t);
	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0); gtk_widget_set_halign(row, GTK_ALIGN_END);
	GtkWidget *ok = rui_button("OK", 1); g_signal_connect(ok, "clicked", G_CALLBACK(about_ok), NULL); gtk_box_pack_start(GTK_BOX(row), ok, FALSE, FALSE, 0);
	dlg_show(g, row); gtk_widget_grab_focus(ok);
}

/* ------------------------------------------------------------------ notifications: as any program of the desktop's, through the
   desktop's notification service (rudolf-portal's org.freedesktop.Notifications), which hands them to the desktop's toasts */
typedef struct { char *app, *title, *body; int error; } Note;
static void notify_bus(GObject *src, GAsyncResult *res, gpointer d)
{
	(void)src; Note *n = d; GDBusConnection *c = res ? g_bus_get_finish(res, NULL) : NULL;
	if (c) { GVariantBuilder a, h; g_variant_builder_init(&a, G_VARIANT_TYPE("as")); g_variant_builder_init(&h, G_VARIANT_TYPE("a{sv}"));
		if (n->error) g_variant_builder_add(&h, "{sv}", "urgency", g_variant_new_byte(2));
		g_dbus_connection_call(c, "org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "Notify",
			g_variant_new("(susssasa{sv}i)", n->app, 0, "", n->title, n->body, &a, &h, 3000), NULL, G_DBUS_CALL_FLAGS_NONE, 2000, NULL, NULL, NULL); g_object_unref(c); }
	else g_printerr("librudolf-ui: %s%s%s\n", n->title, *n->body ? ": " : "", n->body);
	g_free(n->app); g_free(n->title); g_free(n->body); g_free(n);
}
void rui_notify(const char *title, const char *body, int error)
{
	/* (only the session's own bus: never one started for this program, which an X session without a bus would start) */
	const char *e = getenv("DBUS_SESSION_BUS_ADDRESS"), *x = getenv("XDG_RUNTIME_DIR"); char *sock = x ? g_strdup_printf("%s/bus", x) : NULL; int bus = (e && *e) || (sock && g_file_test(sock, G_FILE_TEST_EXISTS)); g_free(sock);
	Note *n = g_new0(Note, 1); n->app = g_strdup_printf("rudolf-%s", APP); n->title = g_strdup(rui_tr(title)); n->body = g_strdup(body ? body : ""); n->error = error;
	if (!bus) { notify_bus(NULL, NULL, n); return; }
	g_bus_get(G_BUS_TYPE_SESSION, NULL, notify_bus, n);
}

/* ------------------------------------------------------------------ asking the native shell (rudolf-native's native_ask): the request in
   the window's own property _RUDOLF_ASK, a ClientMessage to the screen's root (the window manager's); the windows' list comes back in
   the window's property _RUDOLF_WINS */
typedef struct { void (*cb)(const char *json, void *d); void *d; Atom a; } WinsCb;
void rui_shell(RuiWin *w, const char *ask)
{
	GdkWindow *g = gtk_widget_get_window(w->win); if (!g || !GDK_IS_X11_WINDOW(g)) return; Display *d = GDK_WINDOW_XDISPLAY(g); Window x = GDK_WINDOW_XID(g);
	Atom a = XInternAtom(d, "_RUDOLF_ASK", False), u = XInternAtom(d, "UTF8_STRING", False);
	XChangeProperty(d, x, a, u, 8, PropModeReplace, (const unsigned char *)ask, (int)strlen(ask));
	XEvent e; memset(&e, 0, sizeof e); e.xclient.type = ClientMessage; e.xclient.window = x; e.xclient.message_type = a; e.xclient.format = 32;
	XSendEvent(d, DefaultRootWindow(d), False, SubstructureRedirectMask | SubstructureNotifyMask, &e); XFlush(d);
}
static GdkFilterReturn wins_filter(GdkXEvent *xe, GdkEvent *ev, gpointer data)
{
	(void)ev; XEvent *e = xe; WinsCb *c = data;
	if (e->type != PropertyNotify || e->xproperty.atom != c->a || e->xproperty.state != PropertyNewValue) return GDK_FILTER_CONTINUE;
	Atom type; int fmt; unsigned long n = 0, after; unsigned char *p = NULL;
	if (XGetWindowProperty(e->xproperty.display, e->xproperty.window, c->a, 0, 1 << 20, True, AnyPropertyType, &type, &fmt, &n, &after, &p) == Success && p) {
		char *j = g_strndup((char *)p, n); XFree(p); if (c->cb) c->cb(j, c->d); g_free(j); }
	return GDK_FILTER_CONTINUE;
}
void rui_shell_wins(RuiWin *w, void (*cb)(const char *json, void *d), void *d)
{
	static WinsCb *C; GdkWindow *g = gtk_widget_get_window(w->win); if (!g || !GDK_IS_X11_WINDOW(g)) return;
	if (!C) { C = g_new0(WinsCb, 1); C->a = XInternAtom(GDK_WINDOW_XDISPLAY(g), "_RUDOLF_WINS", False); gdk_window_add_filter(g, wins_filter, C); }
	C->cb = cb; C->d = d; rui_shell(w, "wins");
}
