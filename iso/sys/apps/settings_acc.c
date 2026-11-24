/*
 * rudolf-settings: Accessibility (base_v24.html accPanelS and its Mouse and pointer block; settings25.js, Vision, Hearing, Voice control,
 * Keyboard and Pointer assistance; settings31.js, the page as 1.4.4 arranged it: Language and region gone, the mouse's rows in Mouse and
 * touchpad, a link to it; tts25.js, each language's voice and its sample). 1.6.3, docs/plan-1.6.3.md Part 3.
 * As the page: a working copy of the account's accessibility settings (AKEYS) until Apply (secFooter: Apply, "Settings applied.";
 * Revert; Reset to defaults puts back DEFS.acc and keeps it at once, "Defaults restored."), saved as persistS saves them
 * (bd95set:ACCT). Speak a sample and each language's ▶ ask rudolfd's eSpeak NG (say?lang=&text=, as ttsEspeak; Japanese read into kana at
 * build time as jaKana reads it) and play its answer through PulseAudio. The Preview's cursors are the page's own (CURS, CURCOL), drawn
 * from their paths. GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
#pragma GCC diagnostic ignored "-Wunused-variable"
#include "settings-gen.h"
#pragma GCC diagnostic pop

/* ================================================================== the values: S (the working copy) and SC (saved), each key's JSON */
#define NA ((int)G_N_ELEMENTS(ACCDEF))
static char *W_[64], *C_[64]; static int KEEP;
static int ki(const char *k) { for (int i = 0; i < NA; i++) if (!strcmp(ACCDEF[i][0], k)) return i; return -1; }
static char *jv(RuiJ *v)
{
	int t = rui_jtype(v); char b[64];
	if (t == 1) return g_strdup(rui_jval(v) ? "true" : "false");
	if (t == 2) { g_ascii_formatd(b, sizeof b, "%.15g", rui_jval(v)); return g_strdup(b); }
	if (t == 3) { char *e = set_jesc(rui_jsval(v)), *r = g_strdup_printf("\"%s\"", e); g_free(e); return r; }
	if (t == 5) { GString *s = g_string_new("{"); for (int i = 0; rui_jkey(v, i); i++) { char *e = set_jesc(rui_jkey(v, i)), *x = jv(rui_jprop(v, rui_jkey(v, i))); g_string_append_printf(s, "%s\"%s\":%s", i ? "," : "", e, x ? x : "null"); g_free(e); g_free(x); } g_string_append_c(s, '}'); return g_string_free(s, FALSE); }
	return NULL;
}
static void load(void)
{
	RuiJ *o = set_stored(); for (int i = 0; i < NA; i++) { g_free(C_[i]); g_free(W_[i]); C_[i] = jv(rui_jprop(o, ACCDEF[i][0])); W_[i] = C_[i] ? g_strdup(C_[i]) : NULL; } rui_jfree(o);
}
static const char *eff(char **A, int i) { return A[i] ? A[i] : ACCDEF[i][1]; }
static const char *g(const char *k) { int i = ki(k); return i < 0 ? "null" : eff(W_, i); }
static int gb(const char *k) { return !strcmp(g(k), "true"); }
static double gn(const char *k) { return g_ascii_strtod(g(k), NULL); }
static char *gs(const char *k) { const char *j = g(k); char *w = g_strdup_printf("{\"x\":%s}", j); RuiJ *o = rui_jparse(w); g_free(w); char *r = g_strdup(set_jstr(o, "x", "")); rui_jfree(o); return r; }
static int dirty(void) { for (int i = 0; i < NA; i++) if (strcmp(eff(W_, i), eff(C_, i))) return 1; return 0; }
static void set(const char *k, char *json) { int i = ki(k); if (i < 0) { g_free(json); return; } g_free(W_[i]); W_[i] = json; set_note("acc %s %s", k, json); p_footer_update(); set_state_soon(); }
static void setb(const char *k, int on) { set(k, g_strdup(on ? "true" : "false")); }
static void sets(const char *k, const char *v) { char *e = set_jesc(v); set(k, g_strdup_printf("\"%s\"", e)); g_free(e); }
static void commit(int all)
{
	const char **K = g_new0(const char *, NA), **V = g_new0(const char *, NA); int n = 0;
	for (int i = 0; i < NA; i++) { if (!all && !strcmp(eff(W_, i), eff(C_, i)) && C_[i]) continue; if (!all && !W_[i]) continue; K[n] = ACCDEF[i][0]; V[n] = eff(W_, i); n++; }
	if (n) set_store(K, V, n); set_note("persistS acc %d", n); g_free(K); g_free(V);
	for (int i = 0; i < NA; i++) { g_free(C_[i]); C_[i] = g_strdup(eff(W_, i)); if (!W_[i]) W_[i] = g_strdup(C_[i]); }
}

/* ================================================================== speech (tts25.js ttsSpeak through rudolfd's eSpeak NG) */
typedef struct { char *path; } Say;
static gpointer say_thread(gpointer d)
{
	Say *s = d; char *out = NULL; size_t n = 0; int c = rui_http("POST", s->path, "", 0, &out, &n);
	if (c == 200 && out && n > 44 && !memcmp(out, "RIFF", 4)) {   /* (a WAV: its fmt and data chunks) */
		int ch = 1, rate = 22050, bits = 16; size_t at = 12; const char *pcm = NULL; size_t pn = 0;
		while (at + 8 <= n) { unsigned len = (unsigned char)out[at + 4] | (unsigned char)out[at + 5] << 8 | (unsigned char)out[at + 6] << 16 | (unsigned)(unsigned char)out[at + 7] << 24;
			if (!memcmp(out + at, "fmt ", 4) && at + 24 <= n) { ch = (unsigned char)out[at + 10]; rate = (unsigned char)out[at + 12] | (unsigned char)out[at + 13] << 8 | (unsigned char)out[at + 14] << 16; bits = (unsigned char)out[at + 22]; }
			if (!memcmp(out + at, "data", 4)) { pcm = out + at + 8; pn = MIN((size_t)len, n - at - 8); break; } at += 8 + len + (len & 1); }
		if (pcm && bits == 16 && ch >= 1) { size_t fr = pn / (2 * ch); short *m = g_new(short, fr + 1); for (size_t i = 0; i < fr; i++) { int a = 0; for (int k = 0; k < ch; k++) a += ((const short *)pcm)[i * ch + k]; m[i] = (short)(a / ch); }
			set_play_pcm(m, fr, rate); g_free(m); } }
	free(out); g_free(s->path); g_free(s); return NULL;
}
static void say(const char *text, const char *lang)
{
	const char *es = "en-us", *t = text; for (guint i = 0; i < G_N_ELEMENTS(TTSV); i++) if (!strcmp(TTSV[i][0], lang)) { es = TTSV[i][1]; if (!text) t = TTSV[i][2]; }
	if (!t) t = rui_tr("Hello! This is the text-to-speech voice.");
	char *e1 = g_uri_escape_string(es, "!'()*", FALSE), *e2 = g_uri_escape_string(t, "!'()*", FALSE); Say *s = g_new0(Say, 1); s->path = g_strdup_printf("/api/say?lang=%s&text=%s", e1, e2);
	set_note("say %s %s", es, t); g_free(e1); g_free(e2); g_thread_unref(g_thread_new("say", say_thread, s));
}
static char *lang_now(void) { return gs("lang"); }

/* ================================================================== the Preview: the page's cursors in the size and colour chosen */
static GtkWidget *PREV;
static void rgb_of(const char *h, double *r, double *g2, double *b)
{
	unsigned v = 0; if (strlen(h) == 4) { unsigned a, bb, c; sscanf(h + 1, "%1x%1x%1x", &a, &bb, &c); v = a * 17 << 16 | bb * 17 << 8 | c * 17; } else sscanf(h + 1, "%6x", &v);
	*r = (v >> 16 & 255) / 255.0; *g2 = (v >> 8 & 255) / 255.0; *b = (v & 255) / 255.0;
}
static double prev_px(void) { char *c = gs("csize"); double px = atof(c); g_free(c); if (!(px > 0)) px = gn("csize"); return fmin(48, px > 0 ? px : 24); }
static gboolean prev_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)w; (void)d; double z = s_z(), px = prev_px(), t = px + 6; char *col = gs("ccol"); const char *f = "#111", *st = "#fff";
	for (guint i = 0; i < G_N_ELEMENTS(CURCOL); i++) if (!strcmp(CURCOL[i][0], col)) { f = CURCOL[i][1]; st = CURCOL[i][2]; }
	double sc = px / 24, lw = fmax(1.1, 1.4 / fmax(1, sc / 2)), r, gg, b;
	for (guint k = 0; k < G_N_ELEMENTS(CURS7); k++) { double x = k * (t + 6);
		cairo_save(cr); cairo_scale(cr, z, z); if (!strcmp(col, "black")) cairo_set_source_rgb(cr, 0xe5 / 255.0, 0xe7 / 255.0, 0xeb / 255.0); else cairo_set_source_rgb(cr, 0x37 / 255.0, 0x41 / 255.0, 0x51 / 255.0);
		s_rrect(cr, x, 0, t, t, 4); cairo_fill(cr);
		/* (the picture, px by px, shown "contain" in the tile: t / 24 a unit of its viewBox) */
		cairo_translate(cr, x, 0); cairo_scale(cr, t / 24, t / 24); const SetCur *c = &CURS7[k];
		for (int o = 0; o < c->n; o++) { const double *p = c->p + o * 7; switch ((int)p[0]) { case 0: cairo_move_to(cr, p[1], p[2]); break; case 1: cairo_line_to(cr, p[1], p[2]); break; case 2: cairo_curve_to(cr, p[1], p[2], p[3], p[4], p[5], p[6]); break; default: cairo_close_path(cr); } }
		cairo_set_fill_rule(cr, c->evenodd ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING); rgb_of(f, &r, &gg, &b); cairo_set_source_rgb(cr, r, gg, b); cairo_fill_preserve(cr);
		rgb_of(st, &r, &gg, &b); cairo_set_source_rgb(cr, r, gg, b); cairo_set_line_width(cr, lw * 24 / px); cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND); cairo_stroke(cr); cairo_restore(cr); }
	g_free(col); return TRUE;
}
static void prev_size(void) { if (!PREV) return; double t = prev_px() + 6; gtk_widget_set_size_request(PREV, (int)lround((7 * (t + 6) - 6) * s_z()), (int)lround(t * s_z())); gtk_widget_queue_draw(PREV); p_relayout(); }

/* ================================================================== the link to Mouse and touchpad (settings31.js .mvmoved) */
static const char MV1[] = "Pointer speed, the primary button, double-click speed, scroll speed and single-click opening are in ", MV2[] = "Mouse and touchpad", MV3[] = ".";
typedef struct { GtkWidget *a; double h; } Mv;
static char *mv_markup(void)
{
	char *a = g_markup_escape_text(rui_tr(MV1), -1), *b = g_markup_escape_text(rui_tr(MV2), -1); char *r = g_strdup_printf("\x03%s<span foreground=\"%s\" underline=\"single\">%s</span>%s", a, RUI.dark ? "#9e9eff" : "#0000ee", b, MV3); g_free(a); g_free(b); return r;
}
static double mv_lay(void *d, double x, double y, double w) { Mv *m = d; char *t = mv_markup(); m->h = fmax(18, s_lines(t, 13, 400, w, NULL) * s_lh(13, 400)); g_free(t); p_place(m->a, x, y, w, m->h); return m->h; }
static gboolean mv_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)d; char *t = mv_markup(); s_col(cr, "grey", 1); s_text(cr, t, 0, 0, 13, 400, gtk_widget_get_allocated_width(w) / s_z(), 0); g_free(t); return TRUE; }
static gboolean mv_press(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)d; if (e->button != 1 || e->type != GDK_BUTTON_PRESS) return FALSE; char *t = mv_markup(); PangoLayout *l = s_layout(t, 13, 400, gtk_widget_get_allocated_width(w) / s_z()); g_free(t);
	int idx, tr; double lh = s_lh(13, 400) * s_z(); int line = (int)(e->y / lh); PangoLayoutLine *ln = pango_layout_get_line_readonly(l, MIN(line, pango_layout_get_line_count(l) - 1));
	gboolean in = pango_layout_line_x_to_index(ln, (int)(e->x * PANGO_SCALE), &idx, &tr); const char *txt = pango_layout_get_text(l); long a = g_utf8_strlen(rui_tr(MV1), -1), b = a + g_utf8_strlen(rui_tr(MV2), -1), at = g_utf8_pointer_to_offset(txt, txt + idx);
	g_object_unref(l); if (in && at >= a && at < b) { set_note("open mouse"); set_show("mouse"); } return TRUE;
}

/* ================================================================== Caption style: Size, Text, Background, the preview (one line) */
typedef struct { GtkWidget *fx, *sel, *fg, *bg, *txt, *bgd; double x[8]; } Cap;
static Cap CAP;
static void colour_btn_draw_(cairo_t *cr, const char *hex, double W, double H, int focus)
{
	double z = s_z(), r, gg, b; cairo_save(cr); cairo_scale(cr, z, z);
	if (RUI.dark) cairo_set_source_rgb(cr, .23, .23, .23); else cairo_set_source_rgb(cr, 0xef / 255.0, 0xef / 255.0, 0xef / 255.0); s_rrect(cr, .5, .5, W - 1, H - 1, 2); cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 0x76 / 255.0, 0x76 / 255.0, 0x76 / 255.0); cairo_set_line_width(cr, 1); cairo_stroke(cr);
	rgb_of(hex, &r, &gg, &b); cairo_set_source_rgb(cr, r, gg, b); cairo_rectangle(cr, 7, 5, W - 14, H - 10); cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 0x77 / 255.0, 0x77 / 255.0, 0x77 / 255.0); cairo_stroke(cr);
	if (focus) { s_col(cr, "ac", 1); cairo_set_line_width(cr, 2); s_rrect(cr, -1, -1, W + 2, H + 2, 3); cairo_stroke(cr); }
	cairo_restore(cr);
}
static gboolean colour_draw(GtkWidget *w, cairo_t *cr, gpointer d) { char *v = gs(d); colour_btn_draw_(cr, v, gtk_widget_get_allocated_width(w) / s_z(), gtk_widget_get_allocated_height(w) / s_z(), gtk_widget_has_visible_focus(w)); g_free(v); return TRUE; }
static gboolean cap_txt_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)w; (void)d; double z = s_z(), fs = 13 * gn("capsz") / 100, r, gg, b; char *bg = gs("capbg"), *fg = gs("capfg"); double tw; s_lines(rui_tr("Caption preview"), fs, 400, 0, &tw);
	double lh = s_lh(fs, 400); cairo_save(cr); cairo_scale(cr, z, z); rgb_of(bg, &r, &gg, &b); cairo_set_source_rgb(cr, r, gg, b); cairo_rectangle(cr, 0, 0, tw + 12, lh + 4); cairo_fill(cr); cairo_restore(cr);
	rgb_of(fg, &r, &gg, &b); cairo_set_source_rgb(cr, r, gg, b); s_text(cr, rui_tr("Caption preview"), 6, 2, fs, 400, 0, 0); g_free(bg); g_free(fg); return TRUE;
}
static double word_w(const char *t) { double w; s_lines(t, 13, 400, 0, &w); return w; }
static gboolean cap_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)w; (void)d; s_col(cr, "fg", 1); s_text(cr, rui_tr("Size"), 0, 9, 13, 400, 0, 0); s_text(cr, rui_tr("Text"), CAP.x[2], 9, 13, 400, 0, 0); s_text(cr, rui_tr("Background"), CAP.x[4], 9, 13, 400, 0, 0); return TRUE;
}
static void cap_lay(void)
{
	double sp = word_w("a a") - word_w("aa"), x = 0, fs = 13 * gn("capsz") / 100, tw; s_lines(rui_tr("Caption preview"), fs, 400, 0, &tw);
	x = word_w(rui_tr("Size")) + sp; CAP.x[1] = x; x += p_nat_w(CAP.sel) + sp; CAP.x[2] = x; x += word_w(rui_tr("Text")) + sp; CAP.x[3] = x; x += 50 + sp; CAP.x[4] = x; x += word_w(rui_tr("Background")) + sp; CAP.x[5] = x; x += 50 + sp; CAP.x[6] = x;
	double z = s_z(), lh = s_lh(fs, 400);
	gtk_fixed_move(GTK_FIXED(CAP.fx), CAP.sel, (int)lround(CAP.x[1] * z), (int)lround(4 * z)); gtk_widget_set_size_request(CAP.fg, (int)lround(50 * z), (int)lround(27 * z)); gtk_widget_set_size_request(CAP.bg, (int)lround(50 * z), (int)lround(27 * z));
	gtk_fixed_move(GTK_FIXED(CAP.fx), CAP.fg, (int)lround(CAP.x[3] * z), 0); gtk_fixed_move(GTK_FIXED(CAP.fx), CAP.bg, (int)lround(CAP.x[5] * z), 0);
	gtk_widget_set_size_request(CAP.txt, (int)lround((tw + 12) * z), (int)lround((lh + 4) * z)); gtk_fixed_move(GTK_FIXED(CAP.fx), CAP.txt, (int)lround(CAP.x[6] * z), (int)lround((22 - lh) * z));
	gtk_widget_set_size_request(CAP.fx, (int)lround((CAP.x[6] + tw + 12) * z), (int)lround(29 * z)); gtk_widget_set_size_request(CAP.bgd, (int)lround(CAP.x[6] * z), (int)lround(29 * z)); gtk_widget_queue_draw(CAP.fx); gtk_widget_queue_draw(CAP.txt);
}
static void colour_pick(GtkWidget *w, const char *k)
{
	GtkWidget *dl = gtk_color_chooser_dialog_new(rui_tr(!strcmp(k, "capfg") ? "Text" : "Background"), GTK_WINDOW(SW->win)); char *v = gs(k); GdkRGBA c; gdk_rgba_parse(&c, v); g_free(v);
	gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(dl), FALSE); gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(dl), &c); set_note("colour open %s", k); set_state_soon();
	if (gtk_dialog_run(GTK_DIALOG(dl)) == GTK_RESPONSE_OK) { gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(dl), &c); char h[8]; snprintf(h, sizeof h, "#%02x%02x%02x", (int)lround(c.red * 255), (int)lround(c.green * 255), (int)lround(c.blue * 255)); sets(k, h); gtk_widget_queue_draw(w); gtk_widget_queue_draw(CAP.txt); }
	gtk_widget_destroy(dl);
}
static gboolean colour_press(GtkWidget *w, GdkEventButton *e, gpointer d) { if (e->button != 1 || e->type != GDK_BUTTON_PRESS) return FALSE; gtk_widget_grab_focus(w); colour_pick(w, d); return TRUE; }
static gboolean colour_key(GtkWidget *w, GdkEventKey *e, gpointer d) { if (e->keyval == GDK_KEY_space || e->keyval == GDK_KEY_Return) { colour_pick(w, d); return TRUE; } return FALSE; }
static GtkWidget *colour_new(const char *k)
{
	GtkWidget *w = gtk_drawing_area_new(); gtk_widget_set_can_focus(w, TRUE); gtk_widget_add_events(w, GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK);
	g_signal_connect(w, "draw", G_CALLBACK(colour_draw), (gpointer)k); g_signal_connect(w, "button-press-event", G_CALLBACK(colour_press), (gpointer)k); g_signal_connect(w, "key-press-event", G_CALLBACK(colour_key), (gpointer)k); return w;
}
static void on_capsz(const char *v, void *d) { (void)d; set("capsz", g_strdup(v)); cap_lay(); p_relayout(); }

/* ================================================================== each language's voice (tts25.js ttsSettingsBox) */
typedef struct { GtkWidget *fx, *area, *b[16]; double c1, c2, c3; int n; } Tts;
static Tts TT;
static gboolean tts_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)w; (void)d; char *cur = lang_now(); s_col(cr, "fg", 1);
	for (int i = 0; i < TT.n; i++) { int b = !strcmp(SLANGS[i][0], cur); s_text(cr, SLANGS[i][1], 0, i * 22 + 2 + (18 - s_lh(12, 400)) / 2, 12, b ? 700 : 400, 0, 0);
		s_text(cr, rui_tr("eSpeak NG (built into Rudolf OS)"), TT.c1, i * 22 + 4, 12, 400, 0, 0); }
	g_free(cur); return TRUE;
}
static void tts_play(GtkButton *b, gpointer d) { (void)b; say(NULL, d); }
static void tts_lay(void)
{
	double z = s_z(), m = 0, l, bw = TT.n ? p_nat_w(TT.b[0]) : 27; char *cur = lang_now();
	for (int i = 0; i < TT.n; i++) { s_lines(SLANGS[i][1], 12, !strcmp(SLANGS[i][0], cur) ? 700 : 400, 0, &l); m = fmax(m, l); } g_free(cur);
	s_lines(rui_tr("eSpeak NG (built into Rudolf OS)"), 12, 400, 0, &l); TT.c1 = m + 8; TT.c2 = l + 8; TT.c3 = bw + 8;
	for (int i = 0; i < TT.n; i++) { gtk_widget_set_size_request(TT.b[i], (int)lround(bw * z), (int)lround(18 * z)); gtk_fixed_move(GTK_FIXED(TT.fx), TT.b[i], (int)lround((TT.c1 + TT.c2) * z), (int)lround((i * 22 + 2) * z)); }
	gtk_widget_set_size_request(TT.area, (int)lround((TT.c1 + TT.c2 + TT.c3) * z), (int)lround(TT.n * 22 * z)); gtk_widget_set_size_request(TT.fx, (int)lround((TT.c1 + TT.c2 + TT.c3) * z), (int)lround(TT.n * 22 * z));
}

/* (.cbsw: the six colours the correction is seen on) */
static gboolean sw_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)w; (void)d; static const unsigned C[6] = { 0xe11d48, 0x16a34a, 0x2563eb, 0xf59e0b, 0x7c3aed, 0x0891b2 }; cairo_save(cr); cairo_scale(cr, s_z(), s_z());
	for (int i = 0; i < 6; i++) { double x = i * 40; cairo_set_source_rgb(cr, (C[i] >> 16) / 255.0, (C[i] >> 8 & 255) / 255.0, (C[i] & 255) / 255.0); s_rrect(cr, x, 0, 36, 22, 3); cairo_fill(cr);
		cairo_set_source_rgba(cr, 0, 0, 0, .2); cairo_set_line_width(cr, 1); s_rrect(cr, x + .5, .5, 35, 21, 2.5); cairo_stroke(cr); }
	cairo_restore(cr); return TRUE;
}
/* ================================================================== the changes */
static void on_sel(const char *v, void *d) { const char *k = d; if (!strcmp(k, "csize") && strcmp(v, "system")) set(k, g_strdup(v)); else if (!strcmp(k, "slowk") || !strcmp(k, "bounce") || !strcmp(k, "dwell") || !strcmp(k, "magz")) set(k, g_strdup(v)); else sets(k, v); if (!strcmp(k, "csize") || !strcmp(k, "ccol")) prev_size(); }
static void on_ck(int on, void *d)
{
	const char *k = d; setb(k, on);
	if (!strcmp(k, "tts") && on) { char *l = lang_now(); say(rui_tr("Text-to-speech"), l); g_free(l); }
}
static void on_sample(GtkButton *b, gpointer d) { (void)b; (void)d; char *l = lang_now(); say(NULL, l); g_free(l); }
static GtkWidget *sel(const char *k, const char *const *v, const char *const *l, int n)
{
	char *cur = NULL; const char *j = g(k); if (*j == '"') cur = gs(k); else cur = g_strdup(j); if (!strcmp(k, "csize") && !strcmp(cur, "system")) {}
	/* (the page's option for a number: its value as written: 1.5, 2) */
	GtkWidget *s = s_select(v, l, n, cur, on_sel, (gpointer)k); g_free(cur); p_label_named(k, s); return s;
}
static GtkWidget *ck(Cell *c, const char *k, const char *t) { GtkWidget *w = c_check(c, t, gb(k), on_ck, (gpointer)k); p_label_named(k, w); return w; }

/* ================================================================== the footer */
static void act(int what)
{
	if (what == 0) { commit(0); p_footer_note("Settings applied."); }
	else if (what == 1) { for (int i = 0; i < NA; i++) { g_free(W_[i]); W_[i] = C_[i] ? g_strdup(C_[i]) : NULL; } KEEP = 1; set_show("acc"); p_footer_note("Changes reverted."); }
	else { for (int i = 0; i < NA; i++) { g_free(W_[i]); W_[i] = g_strdup(ACCDEF[i][1]); } commit(1); char *l = lang_now(); snprintf(RUI.lang, sizeof RUI.lang, "%s", l); g_free(l); KEEP = 1; set_show("acc"); p_footer_note("Defaults restored."); }
}
static void gone(GtkWidget *w, gpointer d) { (void)w; (void)d; PREV = NULL; CAP.fx = NULL; TT.fx = NULL; }

void page_acc(void)
{
	if (!KEEP) load(); KEEP = 0;
	p_h3("Accessibility");
	/* the Mouse and pointer block, as settings31.js leaves it: Pointer and cursor */
	p_h4("Pointer and cursor");
	{ static const char *const sv[] = { "system", "24", "32", "48", "64", "96" }, *const sl[] = { "System default", "Normal (24 px)", "Medium (32 px)", "Large (48 px)", "Extra large (64 px)", "Huge (96 px)" };
		Cell *c = p_r2("Cursor size"); c_add(c, sel("csize", sv, sl, 6), 0, 0, 1); p_r2_more(c); }
	{ static const char *const cv[] = { "black", "white", "yellow" }, *const cl[] = { "Black with white outline", "White with black outline", "High-contrast yellow" };
		Cell *c = p_r2("Cursor color"); c_add(c, sel("ccol", cv, cl, 3), 0, 0, 1); p_r2_more(c); }
	{ Cell *c = p_r2("Preview"); PREV = gtk_drawing_area_new(); g_signal_connect(PREV, "draw", G_CALLBACK(prev_draw), NULL); g_signal_connect(PREV, "destroy", G_CALLBACK(gone), NULL); prev_size(); c_add(c, PREV, 0, 0, 0); p_label_named("curprev", PREV); p_r2_more(c); }
	{ Cell *c = p_r2("Pointer helpers"); p_r2_top(c, 4); ck(c, "clocate", "Show where the pointer is when I press Ctrl"); c_line(c, 6); ck(c, "chide", "Hide the pointer while I type"); c_line(c, 6);
		ck(c, "cclick", "Highlight clicks (blue = primary, orange = secondary)"); }
	{ Mv *m = g_new0(Mv, 1); m->a = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(m->a), "mv", m, g_free); gtk_widget_add_events(m->a, GDK_BUTTON_PRESS_MASK);
		g_signal_connect(m->a, "draw", G_CALLBACK(mv_draw), m); g_signal_connect(m->a, "button-press-event", G_CALLBACK(mv_press), m); p_put(m->a); p_label_named("mvgo", m->a); p_custom(mv_lay, m, 10, 0); }
	/* settings25.js's Vision, as settings31.js arranges it */
	p_h4("Vision");
	{ static const char *const v[] = { "none", "protan", "deutan", "tritan", "gray", "contrast" }, *const l[] = { "Off", "Protanopia (red-blind)", "Deuteranopia (green-blind)", "Tritanopia (blue-blind)", "Grayscale", "High contrast" };
		Cell *c = p_r2("Colour vision"); c_add(c, sel("cb", v, l, 6), 0, 0, 0); c_line(c, -2);
		GtkWidget *sw = gtk_drawing_area_new(); gtk_widget_set_size_request(sw, (int)lround((6 * 36 + 5 * 4) * s_z()), (int)lround(22 * s_z()));
		g_signal_connect(sw, "draw", G_CALLBACK(sw_draw), NULL); c_add(c, sw, 140, 0, 0); c_line(c, 10); p_r2_more(c); }
	{ Cell *c = p_r2("Screen reader"); ck(c, "sr", "Read the focused item, windows and notifications aloud (Ctrl+Alt+Z)"); c_msg(c, "Tab and the arrow keys move; F6 jumps between the taskbar, desktop and the active window; Ctrl stops speech.", 2); p_r2_more(c); }
	{ Cell *c = p_r2("Text-to-speech"); ck(c, "tts", "Read items under the pointer, window titles and alerts aloud"); c_line(c, 6);
		GtkWidget *b = rui_button("Speak a sample", 0); g_signal_connect(b, "clicked", G_CALLBACK(on_sample), NULL); c_add(c, b, 0, 0, 0); p_label_named("atp", b);
		c_msg_m(c, "Voice for each language:", 2, 6, 0); c_line(c, 4);
		TT.fx = gtk_fixed_new(); TT.area = gtk_drawing_area_new(); g_signal_connect(TT.area, "draw", G_CALLBACK(tts_draw), NULL); gtk_fixed_put(GTK_FIXED(TT.fx), TT.area, 0, 0); TT.n = (int)MIN(G_N_ELEMENTS(SLANGS), 16);
		for (int i = 0; i < TT.n; i++) { TT.b[i] = rui_button("▶", 0); gtk_style_context_add_class(gtk_widget_get_style_context(TT.b[i]), "set-small"); gtk_widget_set_tooltip_text(TT.b[i], rui_tr("Speak a sample in this language"));
			g_signal_connect(TT.b[i], "clicked", G_CALLBACK(tts_play), (gpointer)SLANGS[i][0]); gtk_fixed_put(GTK_FIXED(TT.fx), TT.b[i], 0, 0); char n[24]; snprintf(n, sizeof n, "tts-%s", SLANGS[i][0]); p_label_named(n, TT.b[i]); }
		gtk_widget_show_all(TT.fx); tts_lay(); c_add(c, TT.fx, 0, 0, 0); p_r2_more(c); }
	{ static const char *const v[] = { "1.5", "2", "3", "4", "6" }, *const l[] = { "1.5×", "2×", "3×", "4×", "6×" };
		Cell *c = p_r2("Magnifier"); ck(c, "mag", "Magnify the screen around the pointer (Super + = / −)"); c_line(c, 0); c_add(c, sel("magz", v, l, 5), 0, 0, 0); p_r2_more(c); }
	{ Cell *c = p_r2("Motion and effects"); ck(c, "rmotion", "Reduce motion"); c_line(c, 15); ck(c, "rtrans", "Reduce transparency"); p_r2_more(c); }
	{ Cell *c = p_r2("Reading"); ck(c, "dysfont", "Use a font designed for dyslexia (OpenDyslexic)"); }
	p_h4("Hearing");
	{ Cell *c = p_r2("Visual sound alerts"); ck(c, "vc", "Flash the screen and show a caption whenever the system plays a sound"); p_r2_more(c); }
	{ Cell *c = p_r2("Live captions"); ck(c, "livecap", "Show captions of what the microphone hears"); c_msg(c, "Needs speech recognition, which this browser runs online.", 2); p_r2_more(c); }
	{ static const char *const v[] = { "75", "100", "150", "200" }, *const l[] = { "75%", "100%", "150%", "200%" };
		Cell *c = p_r2("Caption style"); CAP.fx = gtk_fixed_new(); GtkWidget *bgd = gtk_drawing_area_new(); g_signal_connect(bgd, "draw", G_CALLBACK(cap_draw), NULL); gtk_fixed_put(GTK_FIXED(CAP.fx), bgd, 0, 0);
		CAP.bgd = bgd;
		CAP.sel = s_select(v, l, 4, NULL, on_capsz, NULL); { char *cur = g_strdup(g("capsz")); s_select_set(CAP.sel, cur); g_free(cur); } p_label_named("capsz", CAP.sel);
		CAP.fg = colour_new("capfg"); CAP.bg = colour_new("capbg"); CAP.txt = gtk_drawing_area_new(); g_signal_connect(CAP.txt, "draw", G_CALLBACK(cap_txt_draw), NULL);
		gtk_fixed_put(GTK_FIXED(CAP.fx), CAP.sel, 0, 0); gtk_fixed_put(GTK_FIXED(CAP.fx), CAP.fg, 0, 0); gtk_fixed_put(GTK_FIXED(CAP.fx), CAP.bg, 0, 0); gtk_fixed_put(GTK_FIXED(CAP.fx), CAP.txt, 0, 0);
		p_label_named("capfg", CAP.fg); p_label_named("capbg", CAP.bg); gtk_widget_show_all(CAP.fx); cap_lay(); c_add(c, CAP.fx, 0, 0, 0); }
	p_h4("Voice control");
	{ Cell *c = p_r2("Voice control"); ck(c, "voice", "Control Rudolf OS by voice (“open notepad”, “click save”, “type hello”)"); }
	p_h4("Virtual keyboard");
	{ Cell *c = p_r2("Virtual keyboard"); ck(c, "vk", "Show an on-screen keyboard whose keys match the selected language"); }
	p_h4("Keyboard assistance");
	{ Cell *c = p_r2("Sticky keys"); ck(c, "sticky", "Press Ctrl, Shift, Alt or Super once instead of holding it"); p_r2_more(c); }
	{ static const char *const v[] = { "0", "300", "500", "800", "1200" }, *const l[] = { "Off", "Accept a key after 300 ms", "Accept a key after 500 ms", "Accept a key after 800 ms", "Accept a key after 1200 ms" };
		Cell *c = p_r2("Slow keys"); c_add(c, sel("slowk", v, l, 5), 0, 0, 1); p_r2_more(c); }
	{ static const char *const v[] = { "0", "200", "400", "700", "1000" }, *const l[] = { "Off", "Ignore repeats within 200 ms", "Ignore repeats within 400 ms", "Ignore repeats within 700 ms", "Ignore repeats within 1000 ms" };
		Cell *c = p_r2("Bounce keys"); c_add(c, sel("bounce", v, l, 5), 0, 0, 1); }
	p_h4("Pointer assistance");
	{ Cell *c = p_r2("Mouse keys"); ck(c, "mkeys", "Move the pointer with the number pad (5 = click, + = double-click, − = right-click)"); p_r2_more(c); }
	{ static const char *const v[] = { "0", "800", "1200", "1800", "2500" }, *const l[] = { "Off", "Click when the pointer rests 0.8 s", "Click when the pointer rests 1.2 s", "Click when the pointer rests 1.8 s", "Click when the pointer rests 2.5 s" };
		Cell *c = p_r2("Dwell click"); c_add(c, sel("dwell", v, l, 5), 0, 0, 1); p_r2_more(c); }
	{ Cell *c = p_r2("Touch"); ck(c, "touch", "Touch mode: larger buttons and the keyboard opens when you tap a text box"); }
	p_footer(dirty, act);
}
