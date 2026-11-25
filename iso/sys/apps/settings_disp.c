/*
 * rudolf-settings: Display Properties (base_v24.html dispPanel0 and dispPanelS, with its modules: wallpaper31.js, aurora58.js, statue58.js,
 * lock58.js, settings25.js, settings31.js, accent56.js, litepoint60.js, native25.js, gpudrv61.js, saver31.js, tablet58.js, mon61.js,
 * tbbtn59.js, tbprog61.js, titlebar59.js). 1.6.3, docs/plan-1.6.3.md Part 3, PR 5: IN PROGRESS (not yet in the test's pages, not yet
 * the page's whole). As the page: a working copy of DKEYS (the keys and defaults the live page has after every module:
 * desktop/settings-disp.json, read by mksettings.py) until Apply (secFooter), saved as persistS saves them (bd95set:ACCT).
 * Done so far: the backgrounds (each one's thumbnail from the page's own a/NAME.jpg), Appearance, Background style, Color 1 / Color 2,
 * Font, Font size, Window and Icon text colour, Icon size. GPL-2.0-or-later.
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

/* ================================================================== the values: S and SC for DKEYS, each key's JSON */
#define NK ((int)G_N_ELEMENTS(DKEYS))
static char *W_[96], *C_[96]; static int KEEP;
static int ki(const char *k) { for (int i = 0; i < NK; i++) if (!strcmp(DKEYS[i][0], k)) return i; return -1; }
static char *jv(RuiJ *v)
{
	int t = rui_jtype(v); char b[64];
	if (t == 1) return g_strdup(rui_jval(v) ? "true" : "false");
	if (t == 2) { g_ascii_formatd(b, sizeof b, "%.15g", rui_jval(v)); return g_strdup(b); }
	if (t == 3) { char *e = set_jesc(rui_jsval(v)), *r = g_strdup_printf("\"%s\"", e); g_free(e); return r; }
	return NULL;
}
static void load(void) { RuiJ *o = set_stored(); for (int i = 0; i < NK; i++) { g_free(C_[i]); g_free(W_[i]); C_[i] = jv(rui_jprop(o, DKEYS[i][0])); W_[i] = C_[i] ? g_strdup(C_[i]) : NULL; } rui_jfree(o); }
static const char *eff(char **A, int i) { return A[i] ? A[i] : DKEYS[i][1]; }
static const char *g(const char *k) { int i = ki(k); return i < 0 ? "null" : eff(W_, i); }
static char *gs(const char *k) { const char *j = g(k); if (*j != '"') return g_strdup(j); char *w = g_strdup_printf("{\"x\":%s}", j); RuiJ *o = rui_jparse(w); g_free(w); char *r = g_strdup(set_jstr(o, "x", "")); rui_jfree(o); return r; }
static double gn(const char *k) { return g_ascii_strtod(g(k), NULL); }
static int dirty(void) { for (int i = 0; i < NK; i++) if (strcmp(eff(W_, i), eff(C_, i))) return 1; return 0; }
static void set(const char *k, char *json) { int i = ki(k); if (i < 0) { g_free(json); return; } g_free(W_[i]); W_[i] = json; set_note("disp %s %s", k, json); p_footer_update(); set_state_soon(); }
static void sets(const char *k, const char *v) { char *e = set_jesc(v); set(k, g_strdup_printf("\"%s\"", e)); g_free(e); }
static void commit(int all)
{
	const char **K = g_new0(const char *, NK), **V = g_new0(const char *, NK); int n = 0;
	for (int i = 0; i < NK; i++) { if (!all && !strcmp(eff(W_, i), eff(C_, i))) continue; K[n] = DKEYS[i][0]; V[n] = eff(W_, i); n++; }
	if (n) set_store(K, V, n); set_note("persistS disp %d", n); g_free(K); g_free(V);
	for (int i = 0; i < NK; i++) { g_free(C_[i]); C_[i] = g_strdup(eff(W_, i)); if (!W_[i]) W_[i] = g_strdup(C_[i]); }
}

/* ================================================================== the backgrounds (.dynw: four columns 8 px apart, each .dynb a button
   with its .dth thumbnail, 'center/cover', and its name in 11 px) */
static const char *www(void) { const char *w = getenv("RUDOLF_SET_WWW"); return w && *w ? w : "/usr/share/rudolf/www"; }
typedef struct { const char *k, *n, *th; GdkPixbuf *pb; GtkWidget *a; } Thumb;
static GPtrArray *TH;
static int light_now(void) { char *m = gs("dm"); int l = strcmp(m, "dark") != 0; g_free(m); return l; }   /* (dynResolve: locked modes; automatic as light until sunrise and sunset are worked out) */
static gboolean th_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Thumb *t = d; double z = s_z(), W = gtk_widget_get_allocated_width(w) / z, H = gtk_widget_get_allocated_height(w) / z; char *cur = gs("dyn"); int on = !strcmp(cur, t->k); g_free(cur);
	cairo_save(cr); cairo_scale(cr, z, z);
	if (on) { s_col(cr, "ac", .267); s_rrect(cr, -2, -2, W + 4, H + 4, 10); cairo_fill(cr); }
	if (RUI.dark) s_col(cr, "bg2", 1); else cairo_set_source_rgb(cr, 1, 1, 1); s_rrect(cr, 0, 0, W, H, 8); cairo_fill(cr);
	if (on) s_col(cr, "ac", 1); else s_col(cr, "bd", 1); cairo_set_line_width(cr, 2); s_rrect(cr, 1, 1, W - 2, H - 2, 7); cairo_stroke(cr);
	double tx = 5, ty = 5, tw = W - 7, th = H - 10 - 3 - 12.6;   /* (.dth runs past the button's padding to its border: the page's width 100% plus its own padding) */ cairo_save(cr); s_rrect(cr, tx, ty, tw, th, 5); cairo_clip(cr);
	cairo_set_source_rgb(cr, 0xe5 / 255.0, 0xe7 / 255.0, 0xeb / 255.0); cairo_paint(cr);
	if (t->pb) { double iw = gdk_pixbuf_get_width(t->pb), ih = gdk_pixbuf_get_height(t->pb), sc = fmax(tw / iw, th / ih); cairo_translate(cr, tx + (tw - iw * sc) / 2, ty + (th - ih * sc) / 2); cairo_scale(cr, sc, sc); gdk_cairo_set_source_pixbuf(cr, t->pb, 0, 0); cairo_paint(cr); }
	else { cairo_set_source_rgb(cr, 107 / 255.0, 114 / 255.0, 128 / 255.0); double cw; s_lines("✕", 18, 400, 0, &cw); cairo_restore(cr); cairo_restore(cr); cairo_set_source_rgb(cr, 107 / 255.0, 114 / 255.0, 128 / 255.0);
		s_text(cr, "✕", tx + (tw - cw) / 2, ty + (th - s_lh(18, 400)) / 2, 18, 400, 0, 0); goto name; }
	cairo_restore(cr); cairo_restore(cr);
name:
	{ s_col(cr, "fg", 1); const char *n = rui_tr(t->n); double nw; s_lines(n, 11, 400, 0, &nw); s_text(cr, n, (W - nw) / 2, H - 5 - 12.6, 11, 400, 0, 12.65); }
	return TRUE;
}
static void th_redraw(void) { for (guint i = 0; TH && i < TH->len; i++) gtk_widget_queue_draw(((Thumb *)g_ptr_array_index(TH, i))->a); }
static gboolean th_press(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)w; Thumb *t = d; if (e->button != 1 || e->type != GDK_BUTTON_PRESS) return FALSE; sets("dyn", t->k);
	if (*t->k) { char *wp = gs("wpimg"); if (*wp) sets("wpimg", ""); g_free(wp); } th_redraw(); return TRUE;
}
static void th_free(gpointer p) { Thumb *t = p; if (t->pb) g_object_unref(t->pb); g_free(t); }
static void th_load(Thumb *t, int light)
{
	const SetDyn *d = NULL; for (guint i = 0; i < G_N_ELEMENTS(DYNS); i++) if (!strcmp(DYNS[i].k, t->k)) d = &DYNS[i];
	if (!d) return; const char *rel = light ? d->tl : d->td; if (!*rel) return; char *f = g_build_filename(www(), rel, NULL);
	t->pb = gdk_pixbuf_new_from_file(f, NULL); g_free(f);
}
static double grid_lay(void *d, double x, double y, double w)
{
	int which = GPOINTER_TO_INT(d); double cw = (w - 3 * 8) / 4, h = 105.5; int n = 0;
	for (guint i = 0; i < TH->len; i++) { Thumb *t = g_ptr_array_index(TH, i); int live = !*t->k || ({ int L = 0; for (guint j = 0; j < G_N_ELEMENTS(DYNS); j++) if (!strcmp(DYNS[j].k, t->k)) L = DYNS[j].live; L; });
		if (live != !which) continue; p_place(t->a, x + (n % 4) * (cw + 8), y + (n / 4) * (h + 8), cw, h); n++; }
	return n ? ((n + 3) / 4) * (h + 8) - 8 : 0;
}
static Thumb *th_new(const char *k, const char *n, int light)
{
	Thumb *t = g_new0(Thumb, 1); t->k = k; t->n = n; th_load(t, light); t->a = gtk_drawing_area_new(); gtk_widget_add_events(t->a, GDK_BUTTON_PRESS_MASK); gtk_widget_set_tooltip_text(t->a, rui_tr(n));
	g_object_set_data_full(G_OBJECT(t->a), "th", t, th_free); g_signal_connect(t->a, "draw", G_CALLBACK(th_draw), t); g_signal_connect(t->a, "button-press-event", G_CALLBACK(th_press), t); p_put(t->a);
	char nm[40]; snprintf(nm, sizeof nm, "dyn-%s", *k ? k : "none"); p_label_named(nm, t->a); g_ptr_array_add(TH, t); return t;
}
/* (a block of the grid that is laid out by the page itself: the label at its left, centred, the block in the cell) */
typedef struct { const char *label; GtkWidget *la; double (*lay)(void *, double, double, double); void *d; } Row;
static gboolean rl_draw(GtkWidget *w, cairo_t *cr, gpointer d) { Row *r = d; s_col(cr, "fg", 1); double H = gtk_widget_get_allocated_height(w) / s_z(); int n = s_lines(rui_tr(r->label), 13, 400, 130, NULL); s_text(cr, rui_tr(r->label), 0, (H - n * s_lh(13, 400)) / 2, 13, 400, 130, 0); return TRUE; }
static double row_lay(void *d, double x, double y, double w) { Row *r = d; double h = r->lay(r->d, x + 140, y, w - 140); p_place(r->la, x, y, 130, h); return h; }
static void row(const char *label, double (*lay)(void *, double, double, double), void *d, double mb)
{
	Row *r = g_new0(Row, 1); r->label = label; r->lay = lay; r->d = d; r->la = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(r->la), "row", r, g_free); g_signal_connect(r->la, "draw", G_CALLBACK(rl_draw), r); p_put(r->la);
	p_custom(row_lay, r, 0, mb); p_custom_find(label, 0);
}

/* ================================================================== the page's drop-downs, as the live page fills them (DSELS) */
static void on_sel(const char *v, void *d) { const char *k = d; const char *j = g(k); if (*j == '"') sets(k, v); else set(k, g_strdup(v)); }
static GtkWidget *dsel(const char *id, const char *k)
{
	const SetDSel *s = NULL; for (guint i = 0; i < G_N_ELEMENTS(DSELS); i++) if (!strcmp(DSELS[i].id, id)) s = &DSELS[i]; if (!s) return gtk_label_new(id);
	const char **v = g_new0(const char *, s->n), **l = g_new0(const char *, s->n); for (int i = 0; i < s->n; i++) { v[i] = s->o[i][0]; l[i] = s->o[i][1]; }
	char *cur = gs(k); GtkWidget *w = s_select(v, l, s->n, cur, on_sel, (gpointer)k); g_free(cur); g_free(v); g_free(l); p_label_named(id, w); return w;
}

static GtkWidget *dsel_cb(const char *id, const char *cur, void (*cb)(const char *, void *))
{
	const SetDSel *s = NULL; for (guint i = 0; i < G_N_ELEMENTS(DSELS); i++) if (!strcmp(DSELS[i].id, id)) s = &DSELS[i]; if (!s) return gtk_label_new(id);
	const char **v = g_new0(const char *, s->n), **l = g_new0(const char *, s->n); for (int i = 0; i < s->n; i++) { v[i] = s->o[i][0]; l[i] = s->o[i][1]; }
	GtkWidget *w = s_select(v, l, s->n, cur, cb, NULL); g_free(v); g_free(l); p_label_named(id, w); return w;
}
/* ================================================================== colours (input type=color) */
static void rgb_of(const char *h, double *r, double *g2, double *b) { unsigned v = 0; if (strlen(h) == 4) { unsigned a, bb, c; sscanf(h + 1, "%1x%1x%1x", &a, &bb, &c); v = a * 17 << 16 | bb * 17 << 8 | c * 17; } else sscanf(h + 1, "%6x", &v); *r = (v >> 16 & 255) / 255.0; *g2 = (v >> 8 & 255) / 255.0; *b = (v & 255) / 255.0; }
static gboolean colour_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	double z = s_z(), W = gtk_widget_get_allocated_width(w) / z, H = gtk_widget_get_allocated_height(w) / z, r, gg, b; char *v = gs(d); cairo_save(cr); cairo_scale(cr, z, z);
	if (RUI.dark) cairo_set_source_rgb(cr, .23, .23, .23); else cairo_set_source_rgb(cr, 0xef / 255.0, 0xef / 255.0, 0xef / 255.0); s_rrect(cr, .5, .5, W - 1, H - 1, 2); cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 0x76 / 255.0, 0x76 / 255.0, 0x76 / 255.0); cairo_set_line_width(cr, 1); cairo_stroke(cr);
	rgb_of(v, &r, &gg, &b); cairo_set_source_rgb(cr, r, gg, b); cairo_rectangle(cr, 7, 5, W - 14, H - 10); cairo_fill_preserve(cr); cairo_set_source_rgb(cr, 0x77 / 255.0, 0x77 / 255.0, 0x77 / 255.0); cairo_stroke(cr);
	cairo_restore(cr); g_free(v); return TRUE;
}
static gboolean colour_press(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	if (e->button != 1 || e->type != GDK_BUTTON_PRESS) return FALSE; const char *k = d; GtkWidget *dl = gtk_color_chooser_dialog_new(rui_tr("Color"), GTK_WINDOW(SW->win)); char *v = gs(k); GdkRGBA c; gdk_rgba_parse(&c, v); g_free(v);
	gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(dl), FALSE); gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(dl), &c); set_note("colour open %s", k);
	if (gtk_dialog_run(GTK_DIALOG(dl)) == GTK_RESPONSE_OK) { gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(dl), &c); char h[8]; snprintf(h, sizeof h, "#%02x%02x%02x", (int)lround(c.red * 255), (int)lround(c.green * 255), (int)lround(c.blue * 255)); sets(k, h);
		if ((!strcmp(k, "b1") || !strcmp(k, "b2")) && strcmp(g("dyn"), "\"\"")) { sets("dyn", ""); th_redraw(); } gtk_widget_queue_draw(w); }
	gtk_widget_destroy(dl); return TRUE;
}
static GtkWidget *colour(const char *k, double w)
{
	GtkWidget *a = gtk_drawing_area_new(); gtk_widget_set_can_focus(a, TRUE); gtk_widget_add_events(a, GDK_BUTTON_PRESS_MASK); gtk_widget_set_size_request(a, (int)lround(w * s_z()), (int)lround(27 * s_z()));
	g_signal_connect(a, "draw", G_CALLBACK(colour_draw), (gpointer)k); g_signal_connect(a, "button-press-event", G_CALLBACK(colour_press), (gpointer)k); p_label_named(k, a); return a;
}


/* ================================================================== Northern lights (aurora58.js) and the Spirit Falls statue (statue58.js):
   .aurrow and .strow, 6 px under the row before, their .acrow lines (a check box or a word, then a drop-down 10 px after it, 25 px tall),
   their .msg 4 px under */
typedef struct { GtkWidget *ck, *sel, *msg, *lab2, *sel2; char *t; double mh; int statue; } Ext;
static Ext AUR, STA;
static char *st_text(void)
{
	char *a = gs("accent"); for (char *p = a; *p; p++) *p = g_ascii_tolower(*p); const char *an = NULL;
	for (guint i = 0; i < G_N_ELEMENTS(STOF); i++) if (!strcmp(STOF[i][0], a)) an = STOF[i][1];
	if (!an) { double r, gg, b, bd = 1e9; rgb_of(*a == '#' ? a : "#2563eb", &r, &gg, &b); an = "dog";
		for (guint i = 0; i < G_N_ELEMENTS(STOF); i++) { double r2, g2, b2; rgb_of(STOF[i][0], &r2, &g2, &b2); double d = pow((r - r2) * 255, 2) + pow((gg - g2) * 255, 2) + pow((b - b2) * 255, 2); if (d < bd) { bd = d; an = STOF[i][1]; } } }
	const char *nm = an; for (guint i = 0; i < G_N_ELEMENTS(STNAMES); i++) if (!strcmp(STNAMES[i][0], an)) nm = STNAMES[i][1]; g_free(a);
	return g_strdup_printf("The %s stands on the right bank. Each accent colour has its own statue: the twelve animals of the Chinese zodiac, a panda, an elephant, a leopard and a deer. Its carvings glow faintly in the accent colour, and at night paper lanterns rise into the sky in it: Normal is the accent colour; Negative is its negative; Complementary is the colour opposite it on the colour wheel.", nm);
}
static gboolean ext_msg_draw(GtkWidget *w, cairo_t *cr, gpointer d) { Ext *e = d; s_col(cr, "msg", 1); s_text(cr, e->t, 0, 0, 13, 400, gtk_widget_get_allocated_width(w) / s_z(), 0); return TRUE; }
static gboolean word_draw(GtkWidget *w, cairo_t *cr, gpointer d) { s_col(cr, "fg", 1); s_text(cr, rui_tr(d), 0, (gtk_widget_get_allocated_height(w) / s_z() - s_lh(13, 400)) / 2, 13, 400, 0, 0); return TRUE; }
static double wordw(const char *t) { double w; s_lines(rui_tr(t), 13, 400, 0, &w); return w; }
static double ext_lay(void *d, double x, double y, double w)
{
	Ext *e = d; double cy = y + 6, mh;
	if (!e->statue) { double cw = p_nat_w(e->ck); p_place(e->ck, x, cy + 3, 0, 0); p_place(e->sel, x + cw + 10, cy, 142, 25); }
	else { double ww = wordw("Glow"); p_place(e->ck, x, cy, ww + 1, 25); p_place(e->sel, x + ww + 10, cy, 142, 25); }
	cy += 25 + 4; mh = fmax(18, s_lines(e->t, 13, 400, w, NULL) * s_lh(13, 400)); p_place(e->msg, x, cy, w, mh); cy += mh;
	if (e->statue) { double ww = wordw("Sky lanterns at night"); p_place(e->lab2, x, cy, ww + 1, 25); p_place(e->sel2, x + ww + 10, cy, 142, 25); cy += 25; }
	return cy - y;
}
static void on_aur(int on, void *d) { (void)d; set("aur", g_strdup(on ? "true" : "false")); gtk_widget_set_sensitive(AUR.sel, on); }
static GtkWidget *word(const char *t) { GtkWidget *a = gtk_drawing_area_new(); g_signal_connect(a, "draw", G_CALLBACK(word_draw), (gpointer)t); return a; }
static GtkWidget *msgw(Ext *e) { GtkWidget *a = gtk_drawing_area_new(); g_signal_connect(a, "draw", G_CALLBACK(ext_msg_draw), e); return a; }
static double zero_lay(void *d, double x, double y, double w) { (void)d; (void)x; (void)y; (void)w; return 0; }
static void rows_ext(void)
{
	g_free(AUR.t); memset(&AUR, 0, sizeof AUR); AUR.t = g_strdup(rui_tr("The lights take the accent colour: Normal is the accent colour; Negative is its negative; Complementary is the colour opposite it on the colour wheel."));
	AUR.ck = rui_check("At night, with Glacier Lake", !strcmp(g("aur"), "true"), on_aur, NULL); AUR.sel = dsel("daurm", "aurm"); gtk_widget_set_sensitive(AUR.sel, !strcmp(g("aur"), "true")); AUR.msg = msgw(&AUR);
	p_put(AUR.ck); p_put(AUR.sel); p_put(AUR.msg); p_label_named("daur", AUR.ck); row("Northern lights", ext_lay, &AUR, 10); p_custom(zero_lay, NULL, 0, 0); p_custom_find("At night, with Glacier Lake", 0);
	g_free(STA.t); memset(&STA, 0, sizeof STA); STA.statue = 1; STA.t = st_text(); STA.ck = word("Glow"); STA.sel = dsel("dstm", "stm"); STA.msg = msgw(&STA); STA.lab2 = word("Sky lanterns at night"); STA.sel2 = dsel("dlnm", "lnm");
	p_put(STA.ck); p_put(STA.sel); p_put(STA.msg); p_put(STA.lab2); p_put(STA.sel2); row("Spirit Falls statue", ext_lay, &STA, 10);
}


/* ================================================================== Picture (wallpaper31.js): "None" or the picture, Choose from my files…,
   From this computer…, its message under (TO DO: the picture itself is kept in the page's IndexedDB, mirrored by rudolfd's idbm/put) */
typedef struct { GtkWidget *none, *bf, *bc, *msg; char *t; } Pic;
static Pic PIC;
static gboolean pic_msg_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)d; s_col(cr, "msg", 1); s_text(cr, PIC.t ? PIC.t : "", 0, 0, 13, 400, gtk_widget_get_allocated_width(w) / s_z(), 0); return TRUE; }
static gboolean pic_none_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)d; s_col(cr, "msg", 1); s_text(cr, rui_tr("None"), 0, (gtk_widget_get_allocated_height(w) / s_z() - s_lh(13, 400)) / 2, 13, 400, 0, 0); return TRUE; }
static double pic_lay(void *d, double x, double y, double w)
{
	(void)d; double nw = wordw("None"), bh = p_nat_h(PIC.bf); p_place(PIC.none, x, y + (bh - 18) / 2, nw + 1, 18);
	double bx = x + nw + 8; p_place(PIC.bf, bx, y, 0, 0); p_place(PIC.bc, bx + p_nat_w(PIC.bf) + 8, y, 0, 0);
	double mh = fmax(18, PIC.t && *PIC.t ? s_lines(PIC.t, 13, 400, w, NULL) * s_lh(13, 400) : 0); p_place(PIC.msg, x, y + bh + 4, w, mh); return bh + 4 + mh;
}
static void pic_todo(GtkButton *b, gpointer d) { (void)b; (void)d; g_free(PIC.t); PIC.t = g_strdup("(native: not yet)"); gtk_widget_queue_draw(PIC.msg); set_note("picture todo"); }
static void row_pic(void)
{
	PIC.none = gtk_drawing_area_new(); g_signal_connect(PIC.none, "draw", G_CALLBACK(pic_none_draw), NULL); PIC.msg = gtk_drawing_area_new(); g_signal_connect(PIC.msg, "draw", G_CALLBACK(pic_msg_draw), NULL);
	g_free(PIC.t); PIC.t = NULL; PIC.bf = rui_button("Choose from my files…", 0); PIC.bc = rui_button("From this computer…", 0); g_signal_connect(PIC.bf, "clicked", G_CALLBACK(pic_todo), NULL); g_signal_connect(PIC.bc, "clicked", G_CALLBACK(pic_todo), NULL);
	p_put(PIC.none); p_put(PIC.bf); p_put(PIC.bc); p_put(PIC.msg); p_label_named("wpf", PIC.bf); p_label_named("wpc", PIC.bc); row("Picture", pic_lay, NULL, 10);
}


/* ================================================================== Lock and sign-in screen (lock58.js), inside the grid after the colours:
   for everyone (bd95sys's lk, saved at once by an administrator), Choose a picture… (bd95lkpic: a JPEG at most 1920 x 1080) */
static const char *const LKK[7] = { "bg", "color", "blur", "dim", "clock", "pos", "user" }, *const LKD[7] = { "\"desktop\"", "\"#1e3a8a\"", "0", "30", "\"large\"", "\"center\"", "true" };
static char *LKV[7]; static GtkWidget *LKBV, *LKDV;
static void lk_load(void)
{
	RuiJ *o = set_ls_get("bd95sys"), *l = rui_jprop(rui_jprop(o, "x"), "lk");
	for (int i = 0; i < 7; i++) { g_free(LKV[i]); RuiJ *v = rui_jprop(l, LKK[i]); LKV[i] = v ? jv(v) : NULL; if (!LKV[i]) LKV[i] = g_strdup(LKD[i]); } rui_jfree(o);
}
static char *lks(int i) { char *w = g_strdup_printf("{\"x\":%s}", LKV[i]); RuiJ *o = rui_jparse(w); g_free(w); char *r = g_strdup(rui_jtype(rui_jprop(o, "x")) == 3 ? set_jstr(o, "x", "") : LKV[i]); rui_jfree(o); return r; }
/* (any JSON value written again as it was) */
static void jw(GString *b, RuiJ *v)
{
	int t = rui_jtype(v);
	if (t == 1) g_string_append(b, rui_jval(v) ? "true" : "false"); else if (t == 2) { char nb[40]; g_ascii_formatd(nb, sizeof nb, "%.17g", rui_jval(v)); g_string_append(b, nb); }
	else if (t == 3) { char *e = set_jesc(rui_jsval(v)); g_string_append_printf(b, "\"%s\"", e); g_free(e); }
	else if (t == 4) { g_string_append_c(b, '['); for (int i = 0; i < rui_jlen(v); i++) { if (i) g_string_append_c(b, ','); jw(b, rui_jat(v, i)); } g_string_append_c(b, ']'); }
	else if (t == 5) { g_string_append_c(b, '{'); for (int i = 0; rui_jkey(v, i); i++) { const char *k = rui_jkey(v, i); char *e = set_jesc(k); if (i) g_string_append_c(b, ','); g_string_append_printf(b, "\"%s\":", e); g_free(e); jw(b, rui_jprop(v, k)); } g_string_append_c(b, '}'); }
	else g_string_append(b, "null");
}
/* (sysSave: bd95sys with its lk replaced, the rest kept as it is) */
static void lk_save(void)
{
	RuiJ *o = set_ls_get("bd95sys"), *x = rui_jprop(o, "x"); GString *b = g_string_new("{"); int n = 0;
	for (int i = 0; rui_jtype(x) == 5 && rui_jkey(x, i); i++) { const char *k = rui_jkey(x, i); if (!strcmp(k, "lk")) continue; char *e = set_jesc(k); g_string_append_printf(b, "%s\"%s\":", n++ ? "," : "", e); g_free(e); jw(b, rui_jprop(x, k)); }
	g_string_append_printf(b, "%s\"lk\":{", n ? "," : ""); for (int i = 0; i < 7; i++) g_string_append_printf(b, "%s\"%s\":%s", i ? "," : "", LKK[i], LKV[i]); g_string_append(b, "}}");
	set_ls_set("bd95sys", b->str); set_note("sysSave lk %s", b->str); g_string_free(b, TRUE); rui_jfree(o);
}
static void lk_set(int i, char *json) { if (!set_admin()) { g_free(json); return; } g_free(LKV[i]); LKV[i] = json; lk_save(); }
static void on_lksel(const char *v, void *d) { int i = GPOINTER_TO_INT(d); char *e = set_jesc(v); lk_set(i, g_strdup_printf("\"%s\"", e)); g_free(e); if (i == 0) { KEEP = 1; set_show("disp"); } }
static void on_lkrng(double v, void *d) { int i = GPOINTER_TO_INT(d); char b[16]; snprintf(b, sizeof b, "%g", v); lk_set(i, g_strdup(b)); char t[24]; snprintf(t, sizeof t, i == 2 ? "%g px" : "%g %%", v); p_text_set(i == 2 ? LKBV : LKDV, t); }
static void on_lkuser(int on, void *d) { (void)d; lk_set(6, g_strdup(on ? "true" : "false")); }
static void lk_pick(GtkButton *b, gpointer d)
{
	(void)b; (void)d; GtkWidget *dl = gtk_file_chooser_dialog_new(rui_tr("Choose a picture…"), GTK_WINDOW(SW->win), GTK_FILE_CHOOSER_ACTION_OPEN, rui_tr("Cancel"), GTK_RESPONSE_CANCEL, rui_tr("Open"), GTK_RESPONSE_ACCEPT, NULL);
	GtkFileFilter *f = gtk_file_filter_new(); gtk_file_filter_add_pixbuf_formats(f); gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dl), f);
	if (gtk_dialog_run(GTK_DIALOG(dl)) == GTK_RESPONSE_ACCEPT) { char *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dl)); GdkPixbuf *pb = fn ? gdk_pixbuf_new_from_file(fn, NULL) : NULL; g_free(fn);
		if (!pb) set_notify("Lock and sign-in screen", "That file is not a picture.", 1);
		else { int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb); double sc = fmin(1, fmin(1920.0 / w, 1080.0 / h)); GdkPixbuf *s2 = gdk_pixbuf_scale_simple(pb, MAX(1, (int)lround(w * sc)), MAX(1, (int)lround(h * sc)), GDK_INTERP_BILINEAR);
			gchar *buf = NULL; gsize n = 0; if (gdk_pixbuf_save_to_buffer(s2, &buf, &n, "jpeg", NULL, "quality", "85", NULL)) { char *b64 = g_base64_encode((guchar *)buf, n), *j = g_strdup_printf("\"data:image/jpeg;base64,%s\"", b64);
				if (set_ls_set("bd95lkpic", j)) { lk_set(0, g_strdup("\"picture\"")); set_notify("Lock and sign-in screen", "Picture set.", 0); } else set_notify("Lock and sign-in screen", "The picture is too large to keep.", 1); g_free(j); g_free(b64); g_free(buf); }
			g_object_unref(s2); g_object_unref(pb); KEEP = 1; set_show("disp"); } }
	gtk_widget_destroy(dl);
}
static gboolean lkcol_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)d; double z = s_z(), W = gtk_widget_get_allocated_width(w) / z, H = gtk_widget_get_allocated_height(w) / z, r, gg, b; char *v = lks(1); cairo_save(cr); cairo_scale(cr, z, z);
	cairo_set_source_rgb(cr, 0xef / 255.0, 0xef / 255.0, 0xef / 255.0); s_rrect(cr, .5, .5, W - 1, H - 1, 2); cairo_fill_preserve(cr); cairo_set_source_rgb(cr, .46, .46, .46); cairo_set_line_width(cr, 1); cairo_stroke(cr);
	rgb_of(v, &r, &gg, &b); cairo_set_source_rgb(cr, r, gg, b); cairo_rectangle(cr, 7, 5, W - 14, H - 10); cairo_fill(cr); cairo_restore(cr); g_free(v); return TRUE; }
static gboolean lkcol_press(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)d; if (e->button != 1 || e->type != GDK_BUTTON_PRESS || !set_admin()) return FALSE; GtkWidget *dl = gtk_color_chooser_dialog_new(rui_tr("A colour"), GTK_WINDOW(SW->win)); char *v = lks(1); GdkRGBA c; gdk_rgba_parse(&c, v); g_free(v);
	gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(dl), FALSE); gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(dl), &c);
	if (gtk_dialog_run(GTK_DIALOG(dl)) == GTK_RESPONSE_OK) { gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(dl), &c); char h[16]; snprintf(h, sizeof h, "\"#%02x%02x%02x\"", (int)lround(c.red * 255), (int)lround(c.green * 255), (int)lround(c.blue * 255)); lk_set(1, g_strdup(h)); gtk_widget_queue_draw(w); }
	gtk_widget_destroy(dl); return TRUE;
}
static GtkWidget *lksel(int i, const char *id) { const SetDSel *s = NULL; for (guint k = 0; k < G_N_ELEMENTS(DSELS); k++) if (!strcmp(DSELS[k].id, id)) s = &DSELS[k]; if (!s) return gtk_label_new(id);
	const char **v = g_new0(const char *, s->n), **l = g_new0(const char *, s->n); for (int k = 0; k < s->n; k++) { v[k] = s->o[k][0]; l[k] = s->o[k][1]; }
	char *cur = lks(i); GtkWidget *w = s_select(v, l, s->n, cur, on_lksel, GINT_TO_POINTER(i)); g_free(cur); g_free(v); g_free(l); p_label_named(id, w); gtk_widget_set_sensitive(w, set_admin()); return w; }
static void lk_prev(GtkButton *b, gpointer d) { (void)b; (void)d; set_note("lock preview todo"); }
/* (the heading, as lock58.js's .sh4 in the grid: 14 px above it, 2 under, the grid's 10 px gap after it) */
static double lkh_lay(void *d, double x, double y, double w) { p_place(d, x, y, w, s_lh(13, 700)); return s_lh(13, 700); }
static gboolean lkh_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)w; (void)d; if (RUI.dark) s_col(cr, "msg", 1); else cairo_set_source_rgb(cr, 107 / 255.0, 114 / 255.0, 128 / 255.0); char *u = g_utf8_strup(rui_tr("Lock and sign-in screen"), -1);
	PangoLayout *l = s_layout(u, 13, 700, 0); PangoAttrList *al = pango_attr_list_new(); pango_attr_list_insert(al, pango_attr_letter_spacing_new((int)lround(.04 * 13 * s_z() * PANGO_SCALE))); pango_layout_set_attributes(l, al); pango_attr_list_unref(al);
	double de, a = s_asc(13, 700, &de); cairo_move_to(cr, 0, round(a * s_z())); pango_cairo_show_layout_line(cr, pango_layout_get_line_readonly(l, 0)); g_object_unref(l); g_free(u); return TRUE; }
static void rows_lock(void)
{
	lk_load(); int adm = set_admin(); char *bg = lks(0);
	GtkWidget *h = gtk_drawing_area_new(); g_signal_connect(h, "draw", G_CALLBACK(lkh_draw), NULL); p_put(h); p_custom(lkh_lay, h, 24, 12); p_custom_find("Lock and sign-in screen", 1);
	{ Cell *c = p_r2("Background"); c_add(c, lksel(0, "lkbg"), 0, 0, 0);
		if (!strcmp(bg, "picture")) { GtkWidget *b = rui_button("Choose a picture…", 0); gtk_widget_set_sensitive(b, adm); g_signal_connect(b, "clicked", G_CALLBACK(lk_pick), NULL); c_add(c, b, wordw("a a") - wordw("aa"), 1, 0); p_label_named("lkpic", b); }
		if (!strcmp(bg, "color")) { GtkWidget *a = gtk_drawing_area_new(); gtk_widget_add_events(a, GDK_BUTTON_PRESS_MASK); gtk_widget_set_size_request(a, (int)lround(50 * s_z()), (int)lround(27 * s_z())); g_signal_connect(a, "draw", G_CALLBACK(lkcol_draw), NULL); g_signal_connect(a, "button-press-event", G_CALLBACK(lkcol_press), NULL); c_add(c, a, wordw("a a") - wordw("aa"), -1, 0); p_label_named("lkcol", a); }
		p_r2_more(c); }
	for (int i = 2; i <= 3; i++) { Cell *c = p_r2(i == 2 ? "Blur" : "Darken"); GtkWidget *r = s_range(0, i == 2 ? 20 : 80, 1, atof(LKV[i]), on_lkrng, GINT_TO_POINTER(i)); gtk_widget_set_size_request(r, (int)lround(129 * s_z()), (int)lround(16 * s_z())); gtk_widget_set_sensitive(r, adm);
		c_add(c, r, 2, 2, 0); p_label_named(i == 2 ? "lkblur" : "lkdim", r); char t[24]; snprintf(t, sizeof t, i == 2 ? "%s px" : "%s %%", LKV[i]); GtkWidget *v = c_text(c, t, 400); c_gap(v, 2 + wordw("a a") - wordw("aa")); c_dy(v, 6); if (i == 2) LKBV = v; else LKDV = v; c_line(c, 1); p_r2_more(c); }
	{ Cell *c = p_r2("Clock"); c_add(c, lksel(4, "lkck"), 0, 0, 1); p_r2_more(c); }
	{ Cell *c = p_r2("Position"); c_add(c, lksel(5, "lkpos"), 0, 0, 1); p_r2_more(c); }
	{ Cell *c = p_r2("Account"); GtkWidget *k = c_check(c, "Show the account’s picture and name when locked", !strcmp(LKV[6], "true"), on_lkuser, NULL); gtk_widget_set_sensitive(k, adm); p_label_named("lkuser", k); p_r2_more(c); }
	{ Cell *c = p_r2(""); GtkWidget *b = rui_button("Preview", 0); g_signal_connect(b, "clicked", G_CALLBACK(lk_prev), NULL); c_add(c, b, 0, 0, 0); p_label_named("lkprev", b); }
	{ char *t = g_strdup_printf("%s%s", rui_tr("For everyone on this computer, on the lock screen and the sign-in page."), adm ? "" : rui_tr(" Only administrators can change it.")); p_msg_m(t, 0, 10, 16); g_free(t); }
	g_free(bg);
}


/* ================================================================== .sp25's Taskbar (settings31.js's group, tbbtn59.js's buttons, tbprog61.js's
   progress colour): each change a part of the working copy (liveSet) */
static GtkWidget *TBBACM, *TBPC, *TBPCC;
static void on_ck(int on, void *d) { const char *k = d; set(k, g_strdup(on ? "true" : "false")); if (!strcmp(k, "tbbac") && TBBACM) gtk_widget_set_sensitive(TBBACM, on); if (!strcmp(k, "aur") && AUR.sel) gtk_widget_set_sensitive(AUR.sel, on); }
static GtkWidget *dck(Cell *c, const char *k, const char *t) { GtkWidget *w = c ? c_check(c, t, !strcmp(g(k), "true"), on_ck, (gpointer)k) : rui_check(t, !strcmp(g(k), "true"), on_ck, (gpointer)k); p_label_named(k, w); return w; }
static int tbp_listed(const char *v) { for (guint i = 0; i < G_N_ELEMENTS(DSELS); i++) if (!strcmp(DSELS[i].id, "dtbpc")) for (int k = 0; k < DSELS[i].n; k++) if (!strcmp(DSELS[i].o[k][0], v) && strcmp(v, "custom")) return 1; return 0; }
static void on_tbpc(const char *v, void *d) { (void)d; if (!strcmp(v, "custom")) { char *cur = gs("tbpcol"); if (*cur != '#') { g_free(cur); cur = g_strdup("#16a34a"); } sets("tbpcol", cur); g_free(cur); } else sets("tbpcol", v); KEEP = 1; p_relayout(); if (TBPCC) gtk_widget_set_visible(TBPCC, !strcmp(v, "custom")); }
static void sec_taskbar(void)
{
	p_h4("Taskbar");
	{ Cell *c = p_r2("Position"); c_add(c, dsel("tb", "tb"), 0, 0, 1); p_r2_more(c); }
	{ Cell *c = p_r2("Windows"); dck(c, "tbgroup", "Group windows of the same app"); p_r2_more(c); }
	{ Cell *c = p_r2("Style"); c_add(c, dsel("dtbs", "tbstyle"), 0, 0, 1); p_r2_more(c); }
	{ Cell *c = p_r2("Buttons"); c_add(c, dsel("dtbbs", "tbbstyle"), 0, 0, 1); p_r2_more(c); }
	{ Cell *c = p_r2("Button colours"); GtkWidget *k = dck(c, "tbbac", "Match the theme colours"); (void)k; TBBACM = dsel("dtbbacm", "tbbacm"); gtk_widget_set_sensitive(TBBACM, !strcmp(g("tbbac"), "true"));
		c_add(c, TBBACM, 10, 0, 0); c_msg(c, "The start button, pinned apps, open windows, workspaces and the tray. Ticked: the accent colour (Normal, its Negative, or Complementary: the colour opposite it on the colour wheel); unticked: each style's own colours.", 2); p_r2_more(c); }
	{ Cell *c = p_r2("Progress colour"); char *cur = gs("tbpcol"); int listed = tbp_listed(cur); TBPC = dsel_cb("dtbpc", listed ? cur : "custom", on_tbpc); c_add(c, TBPC, 0, 0, 0);
		TBPCC = colour("tbpcol", 50); c_add(c, TBPCC, wordw("a a") - wordw("aa"), -1, 0); if (listed) gtk_widget_set_no_show_all(TBPCC, TRUE);
		c_msg(c, "The fill on a minimized window's taskbar button while it is doing something (a download, a page loading, a copy, an installation).", 2); g_free(cur); }
}


/* ================================================================== Title bars (titlebar59.js): Style, Colour and the two windows' preview
   (.twprev: 320 px, each window 36 px with a 1 px #9ca3af border, radius 8, its title bar in the accent colour; TO DO: the styles'
   own looks and the app's icon) */
static GtkWidget *TWPREV;
static gboolean twprev_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)w; (void)d; double z = s_z();
	for (int k = 0; k < 2; k++) { double y = k * 40; cairo_save(cr); cairo_scale(cr, z, z);
		cairo_set_source_rgb(cr, 1, 1, 1); s_rrect(cr, 0, y, 320, 36, 8); cairo_fill(cr);
		s_col(cr, "ac", 1); cairo_new_path(cr); cairo_move_to(cr, 1, y + 35); cairo_line_to(cr, 1, y + 8); cairo_arc(cr, 8, y + 8, 7, M_PI, 1.5 * M_PI); cairo_line_to(cr, 312, y + 1); cairo_arc(cr, 312, y + 8, 7, 1.5 * M_PI, 2 * M_PI); cairo_line_to(cr, 319, y + 35); cairo_close_path(cr); cairo_fill(cr);
		cairo_set_source_rgb(cr, 156 / 255.0, 163 / 255.0, 175 / 255.0); cairo_set_line_width(cr, 1); s_rrect(cr, .5, y + .5, 319, 35, 7.5); cairo_stroke(cr); cairo_restore(cr);
		cairo_set_source_rgb(cr, 1, 1, 1); s_text(cr, rui_tr(k ? "Another window" : "Active window"), 28, y + 11, 12, 600, 0, 14);
		const char *B[3] = { "_", "□", "✕" }; for (int b = 0; b < 3; b++) { double bw; s_lines(B[b], 12, 600, 0, &bw); s_text(cr, B[b], 233 + b * 27 + (24 - bw) / 2, y + 7 + 5, 12, 600, 0, 12); } }
	return TRUE;
}
static void sec_titlebars(void)
{
	p_h4("Title bars");
	{ Cell *c = p_r2("Style"); c_add(c, dsel("dtws", "twstyle"), 0, 0, 1); p_r2_more(c); }
	{ Cell *c = p_r2("Colour"); c_add(c, dsel("dtwm", "twacm"), 0, 0, 0);
		c_msg(c, "The title bar of the window you are using takes the accent colour: Normal is the accent colour; Negative is its negative; Complementary is the colour opposite it on the colour wheel. The styles are the taskbar's.", 2);
		c_line(c, 6); TWPREV = gtk_drawing_area_new(); gtk_widget_set_size_request(TWPREV, (int)lround(320 * s_z()), (int)lround(76 * s_z())); g_signal_connect(TWPREV, "draw", G_CALLBACK(twprev_draw), NULL); c_add(c, TWPREV, 0, 0, 0); p_label_named("twprev", TWPREV); }
}


/* ================================================================== Colours and theme (settings25.js, accent56.js, litepoint60.js): the accent
   colour (its own colour and the 16 swatches), where it is used, the dark theme, the icons (TO DO: .icprev's eight icons) */
static const unsigned SWC[16] = { 0xdc2626, 0xea580c, 0xd97706, 0xca8a04, 0x65a30d, 0x16a34a, 0x0d9488, 0x0891b2, 0x0284c7, 0x2563eb, 0x4f46e5, 0x9333ea, 0xc026d3, 0xdb2777, 0x92400e, 0x475569 };
static const char *const SWN[16] = { "Red", "Orange", "Amber", "Yellow", "Lime", "Green", "Teal", "Cyan", "Sky", "Blue", "Indigo", "Purple", "Fuchsia", "Pink", "Brown", "Slate" };
static GtkWidget *SWA, *LPV, *ACSEL[3];
static gboolean sw_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)w; (void)d; double z = s_z(); cairo_save(cr); cairo_scale(cr, z, z);
	for (int i = 0; i < 16; i++) { double x = i * 24.7, r = 20.7 / 2; cairo_set_source_rgb(cr, (SWC[i] >> 16) / 255.0, (SWC[i] >> 8 & 255) / 255.0, (SWC[i] & 255) / 255.0); cairo_arc(cr, x + r, r, r, 0, 2 * M_PI); cairo_fill(cr);
		cairo_set_source_rgb(cr, 1, 1, 1); cairo_set_line_width(cr, 2); cairo_arc(cr, x + r, r, r - 1, 0, 2 * M_PI); cairo_stroke(cr); }
	cairo_restore(cr); return TRUE;
}
static gboolean sw_press(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)d; if (e->button != 1 || e->type != GDK_BUTTON_PRESS) return FALSE; int i = (int)(e->x / s_z() / 24.7); if (i < 0 || i > 15) return TRUE;
	char h[16]; snprintf(h, sizeof h, "#%06x", SWC[i]); sets("accent", h); set_note("accent %s", SWN[i]); gtk_widget_queue_draw(w); if (SWA) gtk_widget_queue_draw(SWA); return TRUE;
}
static void on_lpbr(double v, void *d) { (void)d; char b[16]; snprintf(b, sizeof b, "%g", v); set("lpbr", g_strdup(b)); char t[16]; snprintf(t, sizeof t, "%g%%", v); p_text_set(LPV, t); }
static void on_acck(int on, void *d) { const char *k = d; set(k, g_strdup(on ? "true" : "false")); int i = !strcmp(k, "tbac") ? 0 : !strcmp(k, "icac") ? 1 : 2; if (ACSEL[i]) gtk_widget_set_sensitive(ACSEL[i], on); }
static gboolean icprev_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)d; double z = s_z(), W = gtk_widget_get_allocated_width(w) / z, r, gg, b; char *c1 = gs("b1"); rgb_of(c1, &r, &gg, &b); g_free(c1);
	cairo_save(cr); cairo_scale(cr, z, z); cairo_set_source_rgb(cr, r, gg, b); s_rrect(cr, 0, 0, W, 48, 8); cairo_fill(cr); cairo_restore(cr); return TRUE; }
static double icprev_lay(void *d, double x, double y, double w) { p_place(d, x, y, w, 48); return 48; }
static void sec_colours(void)
{
	p_h4("Colours and theme");
	{ Cell *c = p_r2("Accent colour"); SWA = colour("accent", 50); c_add(c, SWA, 0, 0, 0); GtkWidget *sw = gtk_drawing_area_new(); gtk_widget_add_events(sw, GDK_BUTTON_PRESS_MASK);
		gtk_widget_set_size_request(sw, (int)lround((16 * 24.7 - 4) * s_z()), (int)lround(20.7 * s_z())); g_signal_connect(sw, "draw", G_CALLBACK(sw_draw), NULL); g_signal_connect(sw, "button-press-event", G_CALLBACK(sw_press), NULL);
		c_add(c, sw, 6, 3.2, 0); p_label_named("acsw", sw); p_r2_more(c); }
	{ Cell *c = p_r2("Accent colour on"); static const char *const K[3][4] = { { "tbac", "The taskbar", "dtbacm", "tbacm" }, { "icac", "The desktop icons", "dicacm", "icacm" }, { "lpac", "The window transitions (LitePoint)", "dlpacm", "lpacm" } };
		for (int i = 0; i < 3; i++) { GtkWidget *k = c_check(c, K[i][1], !strcmp(g(K[i][0]), "true"), on_acck, (gpointer)K[i][0]); p_label_named(K[i][0], k); c_dy(k, 3); ACSEL[i] = dsel(K[i][2], K[i][3]); gtk_widget_set_sensitive(ACSEL[i], !strcmp(g(K[i][0]), "true")); c_add(c, ACSEL[i], 10, 0, 0); c_line(c, 0); }
		{ GtkWidget *l = c_label(c, "Window transitions' light", 2.5); (void)l; GtkWidget *r = s_range(0, 100, 5, gn("lpbr"), on_lpbr, NULL); gtk_widget_set_size_request(r, (int)lround(180 * s_z()), (int)lround(16 * s_z())); c_add(c, r, 12, 2, 0); p_label_named("lpbr", r);
			char t[16]; snprintf(t, sizeof t, "%g%%", gn("lpbr")); LPV = c_text(c, t, 400); c_gap(LPV, 12); c_dy(LPV, 2.5); }
		c_msg(c, "Normal is the accent colour; Negative is its negative; Complementary is the colour opposite it on the colour wheel. The desktop icons are Rudolf's own icons (apps' own pictures stay as they are). The window transitions are white unless ticked; their light from Off to full (Reduce motion turns them off).", 2); p_r2_more(c); }
	{ Cell *c = p_r2("Windows and menus"); dck(c, "darkui", "Use a dark theme when dark mode is on"); p_r2_more(c); }
	{ Cell *c = p_r2("Icons"); c_add(c, dsel("dic", "icons"), 0, 0, 1); p_r2_more(c); }
	{ Cell *c = p_r2("Icon style"); c_add(c, dsel("dis", "icstyle"), 0, 0, 0); c_line(c, 6); GtkWidget *a = gtk_drawing_area_new(); gtk_widget_set_size_request(a, -1, (int)lround(48 * s_z())); g_signal_connect(a, "draw", G_CALLBACK(icprev_draw), NULL); c_add(c, a, 0, 0, 1); p_label_named("icprev", a); p_r2_more(c); }
	{ Cell *c = p_r2("Icon colours"); dck(c, "icdyn", "Match icons to the wallpaper"); c_msg(c, "Icons take the colour opposite the wallpaper’s main colour on the colour wheel, light on dark wallpapers and dark on light ones, and follow the dynamic wallpaper from day to night.", 2); }
	(void)icprev_lay;
}

/* ================================================================== the footer */
static void act(int what)
{
	if (what == 0) commit(0);
	else if (what == 1) { for (int i = 0; i < NK; i++) { g_free(W_[i]); W_[i] = C_[i] ? g_strdup(C_[i]) : NULL; } KEEP = 1; set_show("disp"); p_footer_note("Changes reverted."); }
	else { for (int i = 0; i < NK; i++) { g_free(W_[i]); W_[i] = g_strdup(DKEYS[i][1]); } commit(1); KEEP = 1; set_show("disp"); }
}
static GtkWidget *DMI;
static void on_is(double v, void *d) { (void)d; char b[16]; snprintf(b, sizeof b, "%g", v); set("is", g_strdup(b)); }

void page_disp(void)
{
	if (!KEEP) load(); KEEP = 0; if (TH) g_ptr_array_unref(TH); TH = g_ptr_array_new(); int light = light_now();
	p_h3("Display Properties");
	th_new("", "None", light); for (int o = 1; o <= (int)G_N_ELEMENTS(DYNS); o++) for (guint i = 0; i < G_N_ELEMENTS(DYNS); i++) if (DYNS[i].live == o) th_new(DYNS[i].k, DYNS[i].n, light);
	for (guint i = 0; i < G_N_ELEMENTS(DYNS); i++) if (!DYNS[i].live) th_new(DYNS[i].k, DYNS[i].n, light);
	row("Dynamic Backgrounds", grid_lay, GINT_TO_POINTER(0), 10);
	row("Static Backgrounds", grid_lay, GINT_TO_POINTER(1), 10);
	{ Cell *c = p_r2("Appearance"); c_add(c, dsel("dm", "dm"), 0, 0, 0); DMI = c_msg_m(c, "Choose a dynamic or static background to use it in light or dark mode.", 3, 6, 0);   /* (.dmi: 12 px; its colour #4b5563 to be matched) */ p_r2_more(c); }
	rows_ext();
	{ Cell *c = p_r2("Background style"); c_add(c, dsel("bs", "bs"), 0, 0, 1); p_r2_more(c); }
	row_pic();
	{ Cell *c = p_r2("Color 1 / Color 2"); c_add(c, colour("b1", 50), 0, 0, 0); c_add(c, colour("b2", 50), 4.1, 0, 0); p_r2_more(c); }
	rows_lock();
	{ Cell *c = p_r2("Font"); c_add(c, dsel("ff", "ff"), 0, 0, 1); p_r2_more(c); }
	{ Cell *c = p_r2("Font size"); c_add(c, dsel("fs", "fs"), 0, 0, 1); p_r2_more(c); }
	{ Cell *c = p_r2("Window text color"); c_add(c, colour("tx", 52), 0, 0, 0); p_r2_more(c); }
	{ Cell *c = p_r2("Icon text color"); c_add(c, colour("ic", 52), 0, 0, 0); p_r2_more(c); }
	{ Cell *c = p_r2("Icon size"); GtkWidget *r = s_range(100, 200, 5, gn("is"), on_is, NULL); gtk_widget_set_size_request(r, (int)lround(129 * s_z()), (int)lround(16 * s_z())); c_add(c, r, 2, 2, 0); p_label_named("is", r); }
	sec_taskbar();
	sec_titlebars();
	sec_colours();
	/* (to come: Night light to Displays) */
	p_footer(dirty, act);
}
