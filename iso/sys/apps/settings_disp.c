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
	double tx = 5, ty = 5, tw = W - 10, th = H - 10 - 3 - 12.6; cairo_save(cr); s_rrect(cr, tx, ty, tw, th, 5); cairo_clip(cr);
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
	/* (to come: Lock and sign-in screen, inside the grid) */
	{ Cell *c = p_r2("Font"); c_add(c, dsel("ff", "ff"), 0, 0, 1); p_r2_more(c); }
	{ Cell *c = p_r2("Font size"); c_add(c, dsel("fs", "fs"), 0, 0, 1); p_r2_more(c); }
	{ Cell *c = p_r2("Window text color"); c_add(c, colour("tx", 52), 0, 0, 0); p_r2_more(c); }
	{ Cell *c = p_r2("Icon text color"); c_add(c, colour("ic", 52), 0, 0, 0); p_r2_more(c); }
	{ Cell *c = p_r2("Icon size"); GtkWidget *r = s_range(100, 200, 5, gn("is"), on_is, NULL); gtk_widget_set_size_request(r, (int)lround(129 * s_z()), (int)lround(16 * s_z())); c_add(c, r, 2, 2, 0); p_label_named("is", r); }
	/* (to come: Taskbar to Displays) */
	p_footer(dirty, act);
}
