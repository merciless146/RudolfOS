/*
 * rudolf-settings: Storage and backup (settings25.js storPanel, with dl27.js's Downloads kept on the disk, storage56.js's On this
 * computer's disk and The Trash, rename56.js's Find duplicates…, stick30.js's Back up to a USB stick and Make a Rudolf OS USB stick,
 * keep29.js's Keep files on this drive). 1.6.3, docs/plan-1.6.3.md Part 3 (PR 7b).
 * What the page itself knows (the browser storage's estimate, the account's files in C:, the Trash, the Downloads kept on the disk) is
 * worked out by the native desktop's hidden page (storage56.js storSnap, asked through the shell: "stor OP") and kept in the page's
 * storage, bd95stor:ACCT, which this page reads; what needs the page (Empty the Trash, a download's Delete, the old items' rule) is done
 * there too ("stor OP ARG"), and what it opens (File Explorer, the duplicates, a backup, a restore, the App Store's Linux apps, Steam,
 * Emu, the Trash, a download, a backup to a drive) in a page of one action ("storgo W ARG"). rudolfd's parts are drawn here: storage,
 * storage/measure, keepfiles, disks, info, sys/status, install/images, stick/info, sys/urlcheck, mkstick (with the administrator's
 * prompt and the size typed, as stkConfirm), the job followed as stkJob and stkProgress follow it. GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "settings.h"

static char *fmt_b(double b)
{
	if (!(b >= 0)) return g_strdup("NaN GB"); if (b < 1024) return g_strdup_printf("%.0f B", b); if (b < 1048576) return g_strdup_printf("%.1f KB", b / 1024);
	if (b < 1073741824) return g_strdup_printf("%.2f MB", b / 1048576); return g_strdup_printf("%.1f GB", b / 1073741824);
}
static char *gb(double b) { return g_strdup_printf("%.1f GB", b / 1e9); }   /* (stkGB) */
static double tw(const char *t, double px, int wt) { double w = 0; s_lines(t ? t : "", px, wt, 0, &w); return w; }
static double sp_w(void) { double sp, a1; s_lines("a a", 13, 400, 0, &sp); s_lines("aa", 13, 400, 0, &a1); return sp - a1; }
static double word_max(const char *t, double px, int wt) { double m = 0; char **w = g_regex_split_simple("\\s+", t ? t : "", 0, 0); for (int i = 0; w[i]; i++) if (*w[i]) m = fmax(m, tw(w[i], px, wt)); g_strfreev(w); return m; }
static void redraw(void) { set_show_keep("stor"); }
static void ask(const char *a) { rui_shell(SW, a); set_note("%s", a); }
static int jb(RuiJ *o, const char *k) { RuiJ *v = o ? rui_jprop(o, k) : NULL; return v && ((rui_jtype(v) == 1 && rui_jval(v)) || (rui_jtype(v) == 2 && rui_jval(v)) || (rui_jtype(v) == 3 && *rui_jsval(v))); }
static double jnum(RuiJ *o, const char *k) { RuiJ *v = o ? rui_jprop(o, k) : NULL; return v && rui_jtype(v) == 2 ? rui_jval(v) : NAN; }
static RuiJ *post(const char *path, char *err, size_t en)
{
	char p[600], *out = NULL; snprintf(p, sizeof p, "/api/%s", path); int c = rui_http("POST", p, NULL, 0, &out, NULL); RuiJ *j = out ? rui_jparse(out) : NULL; g_free(out); set_note("POST %s %d", path, c);
	if (c != 200 || !j) { if (err) snprintf(err, en, "%s", j ? set_jstr(j, "error", "Request failed") : "rudolfd did not answer"); rui_jfree(j); return NULL; } return j;
}
static RuiJ *get(const char *path) { char p[300]; snprintf(p, sizeof p, "/api/%s", path); return rui_get(p); }

/* ================================================================== a .tmt of the page's, up to four columns (CSS's automatic table
   layout: each column between its narrowest and widest contents), each cell writing, an icon and a name in bold over small writing
   (storage56.js's lines), a bar (.stbar, 80px), or a button; a cell's writing at the right when asked */
enum { C_TEXT, C_LINE, C_BAR, C_BTN };
typedef struct { int k, right; char *t, *icon, *b, *s; double frac; GtkWidget *w; } TCell;
typedef struct { TCell c[4]; } TRow;
typedef struct { GtkWidget *a; GArray *rows; int nc; double cw[4], W; } NTab;
static void cell_mm(TCell *c, double *mn, double *mx)
{
	switch (c->k) {
	case C_TEXT: *mx = tw(c->t, 13, 400); *mn = word_max(c->t, 13, 400); break;
	case C_LINE: *mx = fmax(tw(c->icon, 13, 400) + sp_w() + tw(c->b, 13, 700), tw(c->s, 10.8333, 400)); *mn = fmax(fmax(tw(c->icon, 13, 400), word_max(c->b, 13, 700)), word_max(c->s, 10.8333, 400)); break;
	case C_BAR: *mx = *mn = 82; break;
	default: *mx = c->w ? p_nat_w(c->w) : 0; *mn = c->w ? fmin(*mx, word_max(gtk_button_get_label(GTK_BUTTON(c->w)), 13, 400) + 22) : 0;
		if (c->w && g_object_get_data(G_OBJECT(c->w), "w2")) { double w2 = p_nat_w(g_object_get_data(G_OBJECT(c->w), "w2")); *mx += sp_w() + w2; *mn = fmax(*mn, w2); } break; }
	*mx += 12; *mn += 12;
}
static void ntab_cols(NTab *t, double W)
{
	double mn[4] = { 0 }, mx[4] = { 0 }, smn = 0, smx = 0;
	for (guint i = 0; i < t->rows->len; i++) { TRow *r = &g_array_index(t->rows, TRow, i); for (int c = 0; c < t->nc; c++) { double a, b; cell_mm(&r->c[c], &a, &b); mn[c] = fmax(mn[c], a); mx[c] = fmax(mx[c], b); } }
	for (int c = 0; c < t->nc; c++) { smn += mn[c]; smx += mx[c]; }
	for (int c = 0; c < t->nc; c++) t->cw[c] = smx <= W ? mx[c] + (W - smx) * mx[c] / fmax(1, smx) : mn[c] + fmax(0, W - smn) * (smx - smn > 0 ? (mx[c] - mn[c]) / (smx - smn) : 1.0 / t->nc);
	t->W = W;
}
static double btn_h(TCell *c, double w) { if (!c->w) return 0; double nw = p_nat_w(c->w); return nw <= w + .5 ? p_nat_h(c->w) : p_nat_h(c->w) + 15 * (s_lines(gtk_button_get_label(GTK_BUTTON(c->w)), 13, 400, w - 22, NULL) - 1); }
static double cell_h(TCell *c, double w)
{
	switch (c->k) { case C_TEXT: return s_lines(c->t, 13, 400, w, NULL) * 15; case C_LINE: return 15 + (c->s && *c->s ? s_lines(c->s, 10.8333, 400, w, NULL) : 1) * 15; case C_BAR: return 10; default: return btn_h(c, w); }
}
static double row_h(NTab *t, TRow *r) { double h = 0; for (int c = 0; c < t->nc; c++) h = fmax(h, cell_h(&r->c[c], t->cw[c] - 12)); return h + 9; }
static double ntab_lay(void *d, double x, double y, double w)
{
	NTab *t = d; ntab_cols(t, w); double h = 0;
	for (guint i = 0; i < t->rows->len; i++) { TRow *r = &g_array_index(t->rows, TRow, i); double rh = row_h(t, r), cx = 0;
		for (int c = 0; c < t->nc; c++) { TCell *e = &r->c[c]; if (e->k == C_BTN && e->w) { double bw = fmin(p_nat_w(e->w), t->cw[c] - 12), bh = btn_h(e, t->cw[c] - 12);
				p_place(e->w, x + cx + 6, y + h + (rh - 1 - bh) / 2, bw, bh); GtkWidget *w2 = g_object_get_data(G_OBJECT(e->w), "w2"); if (w2) p_place(w2, x + cx + 6 + bw + sp_w(), y + h + (rh - 1 - p_nat_h(w2)) / 2, 0, 0); if (bw < p_nat_w(e->w) - .5) { GtkWidget *l = gtk_bin_get_child(GTK_BIN(e->w)); gtk_label_set_line_wrap(GTK_LABEL(l), TRUE); gtk_label_set_justify(GTK_LABEL(l), GTK_JUSTIFY_CENTER); } }
			cx += t->cw[c]; }
		h += rh; }
	p_place(t->a, x, y, w, MAX(1, h)); return h;
}
static gboolean ntab_draw(GtkWidget *wg, cairo_t *cr, gpointer d)
{
	NTab *t = d; double W = gtk_widget_get_allocated_width(wg) / s_z(), y = 0; if (fabs(W - t->W) > .1) ntab_cols(t, W);
	for (guint i = 0; i < t->rows->len; i++) { TRow *r = &g_array_index(t->rows, TRow, i); double rh = row_h(t, r), cx = 0;
		s_col(cr, "td", 1); cairo_rectangle(cr, 0, y + rh - 1, W, 1); cairo_fill(cr);
		for (int c = 0; c < t->nc; c++) { TCell *e = &r->c[c]; double cw = t->cw[c] - 12, ch = cell_h(e, cw), top = y + (rh - 1 - ch) / 2; s_col(cr, "fg", 1);
			if (e->k == C_TEXT) { if (e->right) { int n = s_lines(e->t, 13, 400, cw, NULL); if (n == 1) s_text(cr, e->t, cx + 6 + cw - tw(e->t, 13, 400), top, 13, 400, 0, 15);
					else { PangoLayout *l = s_layout(e->t, 13, 400, cw); pango_layout_set_alignment(l, PANGO_ALIGN_RIGHT); double z = s_z(); cairo_save(cr); cairo_scale(cr, 1 / z, 1 / z); cairo_move_to(cr, (cx + 6) * z, top * z); pango_cairo_show_layout(cr, l); cairo_restore(cr); g_object_unref(l); } }
				else s_text(cr, e->t, cx + 6, top, 13, 400, cw, 15); }
			else if (e->k == C_LINE) { s_text(cr, e->icon, cx + 6, top, 13, 400, 0, 15); s_text(cr, e->b, cx + 6 + tw(e->icon, 13, 400) + sp_w(), top, 13, 700, 0, 15);
				cairo_save(cr); cairo_push_group(cr); s_text(cr, e->s, cx + 6, top + 15 + (15 - 13) / 2.0 + .5, 10.8333, 400, cw, 15); cairo_pop_group_to_source(cr); cairo_paint_with_alpha(cr, .7); cairo_restore(cr); }
			else if (e->k == C_BAR) { double bx = cx + 6, by = top; s_rrect(cr, bx + .5, by + .5, 81, 9, 4.5); rui_color(cr, "bg2", 1); cairo_fill_preserve(cr); rui_color(cr, "bd", 1); cairo_set_line_width(cr, 1); cairo_stroke(cr);
				cairo_save(cr); s_rrect(cr, bx + 1, by + 1, 80, 8, 4); cairo_clip(cr); rui_color(cr, "ac", 1); cairo_rectangle(cr, bx + 1, by + 1, 80 * e->frac, 8); cairo_fill(cr); cairo_restore(cr); }
			cx += t->cw[c]; }
		y += rh; }
	return FALSE;
}
static NTab *ntab(int nc) { NTab *t = g_new0(NTab, 1); t->nc = nc; t->rows = g_array_new(FALSE, TRUE, sizeof(TRow)); t->a = gtk_drawing_area_new(); g_signal_connect(t->a, "draw", G_CALLBACK(ntab_draw), t); p_put(t->a); return t; }
static TRow *nrow(NTab *t) { TRow r; memset(&r, 0, sizeof r); g_array_append_val(t->rows, r); return &g_array_index(t->rows, TRow, t->rows->len - 1); }
static void ctext(TCell *c, const char *s) { c->k = C_TEXT; c->t = g_strdup(s ? s : ""); }

/* (.stbar: 14 px inside its 1px border, round, the accent's fill) */
static double bar_lay(void *d, double x, double y, double w) { p_place(d, x, y, w, 16); return 16; }
static gboolean bar_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)d; double W = gtk_widget_get_allocated_width(w) / s_z(), f = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), "f")) / 1e6; s_rrect(cr, .5, .5, W - 1, 15, 7.5); rui_color(cr, "bg2", 1); cairo_fill_preserve(cr); rui_color(cr, "bd", 1); cairo_set_line_width(cr, 1); cairo_stroke(cr);
	cairo_save(cr); s_rrect(cr, 1, 1, W - 2, 14, 7); cairo_clip(cr); rui_color(cr, "ac", 1); cairo_rectangle(cr, 1, 1, (W - 2) * f, 14); cairo_fill(cr); cairo_restore(cr); return FALSE;
}
static void p_bar(double frac, double mt) { GtkWidget *a = gtk_drawing_area_new(); g_object_set_data(G_OBJECT(a), "f", GINT_TO_POINTER((int)lround(fmax(0, fmin(1, frac)) * 1e6))); g_signal_connect(a, "draw", G_CALLBACK(bar_draw), NULL); p_put(a); p_custom(bar_lay, a, mt, 0); }

/* buttons in a .rmrow (flex, wrapping, gap 8 px) */
typedef struct { GtkWidget *b[8]; int n; } BRow;
static double brow_lay(void *d, double x, double y, double w)
{
	BRow *r = d; double cx = 0, cy = 0, lh = 0;
	for (int k = 0; k < r->n; k++) { double bw = p_nat_w(r->b[k]), bh = p_nat_h(r->b[k]); if (cx > 0 && cx + bw > w + .5) { cx = 0; cy += lh + 8; lh = 0; } p_place(r->b[k], x + cx, y + cy, 0, 0); cx += bw + 8; lh = fmax(lh, bh); }
	return cy + lh;
}
static GtkWidget *bbtn(BRow *r, const char *l, int pri, GCallback cb, void *d, const char *name) { GtkWidget *b = rui_button(l, pri); if (cb) g_signal_connect(b, "clicked", cb, d); p_put(b); if (name) p_label_named(name, b); r->b[r->n++] = b; return b; }

/* .acr rows (grid: 1fr 110px 90px, gap 8, 4px above and below, a line under): the writing, a button (its words wrap in 110 px) */
typedef struct { GtkWidget *a; char *t[8]; GtkWidget *b[8]; int n; } Acr;
static double acr_rh(Acr *r, int i) { double bh = p_nat_w(r->b[i]) > 110.5 ? 23 + 15 * (s_lines(gtk_button_get_label(GTK_BUTTON(r->b[i])), 13, 400, 88, NULL) - 1) : 23; return fmax(15, bh) + 8 + 1; }
static double acr_lay(void *d, double x, double y, double w)
{
	Acr *r = d; double cy = 0, cw = w - 110 - 90 - 16;
	for (int i = 0; i < r->n; i++) { double h = acr_rh(r, i); if (p_nat_w(r->b[i]) > 110.5) { GtkWidget *l = gtk_bin_get_child(GTK_BIN(r->b[i])); gtk_label_set_line_wrap(GTK_LABEL(l), TRUE); gtk_label_set_justify(GTK_LABEL(l), GTK_JUSTIFY_CENTER); }
		p_place(r->b[i], x + cw + 8, y + cy + 4, 110, h - 9); cy += h; }
	p_place(r->a, x, y, w, MAX(1, cy)); return cy;
}
static gboolean acr_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Acr *r = d; double W = gtk_widget_get_allocated_width(w) / s_z(), cy = 0;
	for (int i = 0; i < r->n; i++) { double h = acr_rh(r, i); s_col(cr, "acr", 1); cairo_rectangle(cr, 0, cy + h - 1, W, 1); cairo_fill(cr); s_col(cr, "fg", 1); s_text(cr, r->t[i], 0, cy + (h - 1 - 15) / 2, 13, 400, W - 216, 15); cy += h; }
	return FALSE;
}
static Acr *acr(void) { Acr *r = g_new0(Acr, 1); r->a = gtk_drawing_area_new(); g_signal_connect(r->a, "draw", G_CALLBACK(acr_draw), r); p_put(r->a); return r; }
static GtkWidget *acr_row(Acr *r, const char *t, const char *b, GCallback cb, void *d, const char *name)
{
	if (r->n >= 8) return NULL; r->t[r->n] = g_strdup(t); GtkWidget *x = rui_button(b, 0); g_signal_connect(x, "clicked", cb, d); p_put(x); if (name) p_label_named(name, x); r->b[r->n++] = x; return x;
}

/* ================================================================== the page's own figures (bd95stor:ACCT), asked again of the hidden
   page when the page opens and after each change made there; drawn again when the new ones come */
static RuiJ *SNAP; static double SNAP_T, ASKED_T; static guint POLL;
static void snap_load(void) { rui_jfree(SNAP); char k[120]; snprintf(k, sizeof k, "bd95stor:%s", ACCT); RuiJ *o = set_ls_get(k); SNAP = NULL; if (o) { char *s = NULL; GString *b = g_string_new(NULL); set_jw(b, rui_jprop(o, "x")); s = g_string_free(b, FALSE); SNAP = rui_jparse(s); g_free(s); rui_jfree(o); } }
static int POLLN;
static gboolean snap_poll(gpointer d)
{
	(void)d; char k[120]; snprintf(k, sizeof k, "bd95stor:%s", ACCT); RuiJ *o = set_ls_get(k); double t = o ? jnum(rui_jprop(o, "x"), "t") : NAN; rui_jfree(o);
	if (t == t && t > SNAP_T) { POLL = 0; redraw(); return G_SOURCE_REMOVE; } if (++POLLN > 16) { POLL = 0; return G_SOURCE_REMOVE; } return G_SOURCE_CONTINUE;
}
static void snap_ask(const char *op, const char *arg)
{
	char a[300]; snprintf(a, sizeof a, "stor %s%s%s", op, arg ? " " : "", arg ? arg : ""); ask(a); ASKED_T = set_now(); POLLN = 0; if (!POLL) POLL = g_timeout_add(500, snap_poll, NULL);
}
static void go(const char *w, const char *arg) { char a[300]; snprintf(a, sizeof a, "storgo %s%s%s", w, arg ? " " : "", arg ? arg : ""); ask(a); }

/* ================================================================== the top: the browser storage's bar and line; Your home folder */
static void on_empty(GtkButton *b, gpointer d) { (void)b; (void)d; snap_ask("emptytrash", NULL); }
static void on_go(GtkButton *b, gpointer d) { (void)b; go(d, NULL); }
static void top_sec(void)
{
	double used = jnum(SNAP, "used"), quota = jnum(SNAP, "quota");
	p_bar(quota > 0 ? fmax(.01, used / quota) : .01, 0);
	if (SNAP) { char *u = fmt_b(used), *q = quota > 0 ? fmt_b(quota) : NULL, *t = g_strdup_printf("%s used%s%s%s · %s%s", u, q ? " of " : "", q ? q : "", q ? " available to Rudolf OS" : "", jb(SNAP, "idb") ? "IndexedDB" : "browser storage", jb(SNAP, "pers") ? " · protected from automatic clean-up" : "");
		p_label_named("sti2", p_msg(t, 0)); g_free(u); g_free(q); g_free(t); }
	else p_label_named("sti2", p_msg("Measuring…", 0));
	p_h4("Your home folder"); NTab *t = ntab(3); RuiJ *h = rui_jprop(SNAP, "home");
	for (int i = 0; h && i < rui_jlen(h); i++) { RuiJ *e = rui_jat(h, i); TRow *r = nrow(t); ctext(&r->c[0], rui_jsval(rui_jat(e, 0)) ? rui_jsval(rui_jat(e, 0)) : ""); char *f = g_strdup_printf("%.0f file(s)", rui_jval(rui_jat(e, 1))), *s = fmt_b(rui_jval(rui_jat(e, 2))); ctext(&r->c[1], f); ctext(&r->c[2], s); g_free(f); g_free(s); }
	if (SNAP) { TRow *r = nrow(t); ctext(&r->c[0], "Trash"); char *f = g_strdup_printf("%.0f item(s)", jnum(SNAP, "trN")), *s = fmt_b(jnum(SNAP, "trB")); ctext(&r->c[1], f); ctext(&r->c[2], s); g_free(f); g_free(s); }
	p_custom(ntab_lay, t, 0, 0);
	BRow *r = g_new0(BRow, 1); bbtn(r, "Empty the Trash", 0, G_CALLBACK(on_empty), NULL, "et"); bbtn(r, "Open File Explorer", 0, G_CALLBACK(on_go), "home", "ex");
	if (ACCT[0] && jb(SNAP, "dups")) bbtn(r, "Find duplicates…", 0, G_CALLBACK(on_go), "dups", "dup56"); p_custom(brow_lay, r, 14, 0);
}

/* ================================================================== Downloads kept on the disk (dl27.js) */
static void on_dlopen(GtkButton *b, gpointer d) { (void)b; go("dlopen", d); }
static void on_dldel(GtkButton *b, gpointer d) { (void)b; snap_ask("dldel", d); }
static void dl_sec(void)
{
	RuiJ *dl = rui_jprop(SNAP, "dl"); if (!dl || rui_jtype(dl) != 4) return;
	p_h4("Downloads kept on the disk"); double tot = 0; for (int i = 0; i < rui_jlen(dl); i++) tot += rui_jval(rui_jat(rui_jat(dl, i), 1));
	char *tt = fmt_b(tot); GString *m = g_string_new(NULL); g_string_printf(m, "%d file(s), %s.", rui_jlen(dl), tt); g_free(tt);
	RuiJ *sp = get("dl/space"); if (sp && jnum(sp, "total") > 0) { char *f = fmt_b(jnum(sp, "free")), *t = fmt_b(jnum(sp, "total")); g_string_append_printf(m, " The disk has %s free of %s.%s", f, t, jb(sp, "live") ? " Running live: they are kept in memory and gone after you shut down." : ""); g_free(f); g_free(t); } rui_jfree(sp);
	p_msg_m(m->str, 0, 10, 13); g_string_free(m, TRUE);
	NTab *t = ntab(rui_jlen(dl) ? 3 : 1);
	if (!rui_jlen(dl)) ctext(&nrow(t)->c[0], "None. Downloads larger than 1.5 MB are kept here.");
	for (int i = 0; i < rui_jlen(dl) && i < 40; i++) { RuiJ *e = rui_jat(dl, i); const char *n = rui_jsval(rui_jat(e, 0)) ? rui_jsval(rui_jat(e, 0)) : ""; TRow *r = nrow(t); ctext(&r->c[0], n); char *s = fmt_b(rui_jval(rui_jat(e, 1))); ctext(&r->c[1], s); g_free(s);
		r->c[2].k = C_BTN; r->c[2].w = rui_button("Open", 0); char *k = g_strdup(n); g_signal_connect_data(r->c[2].w, "clicked", G_CALLBACK(on_dlopen), k, (GClosureNotify)(void (*)(void))g_free, 0); p_put(r->c[2].w);
		/* (Delete beside it: the same cell, as the page's "Open Delete") */
		GtkWidget *x = rui_button("Delete", 0); g_signal_connect_data(x, "clicked", G_CALLBACK(on_dldel), g_strdup(n), (GClosureNotify)(void (*)(void))g_free, 0); p_put(x); g_object_set_data(G_OBJECT(r->c[2].w), "w2", x); }
	p_custom(ntab_lay, t, 0, 0);
}

/* ================================================================== On this computer's disk (storage56.js storDisk) */
static const char *const LINES[][4] = {   /* (STOR_LINES) */
	{ "files", "📄", "Your files", "Desktop, Documents, Pictures, Music and Videos" }, { "others", "👥", "Other accounts' files", "" }, { "downloads", "⬇️", "Downloads", "yours, and what Linux apps download" },
	{ "trash", "🗑️", "The Trash", "files waiting until the Trash is emptied" }, { "flatpak", "🐧", "Linux apps", "App Store apps and their data" }, { "steam", "🎮", "Steam's games", "inside the Steam app" },
	{ "debian", "🐧", "Linux (Debian)", ".deb packages and Windows programs (Wine)" }, { "games", "🕹️", "Emu's games", "" }, { "ai", "🦌", "Blitzen's model", "the offline assistant" },
	{ "updates", "🔄", "The system", "Rudolf OS, and its spare copy for updates and going back" }, { "rest", "⚙️", "Everything else", "the browser's cache, settings and logs" } };
static RuiJ *SDJ; static guint SDPOLL;
static gboolean sd_poll(gpointer d) { (void)d; RuiJ *j = get("storage"); if (!j) { SDPOLL = 0; return G_SOURCE_REMOVE; } int m = jb(j, "measuring"); rui_jfree(SDJ); SDJ = j; redraw(); if (!m) { SDPOLL = 0; return G_SOURCE_REMOVE; } return G_SOURCE_CONTINUE; }
static void sd_measure(void) { char er[200]; RuiJ *j = post("storage/measure", er, sizeof er); if (j) { rui_jfree(SDJ); SDJ = j; if (jb(j, "measuring") && !SDPOLL) SDPOLL = g_timeout_add(1500, sd_poll, NULL); } }
static void on_measure(GtkButton *b, gpointer d) { (void)b; (void)d; sd_measure(); redraw(); }
static void on_sda(GtkButton *b, gpointer d) { (void)b; const char *k = d;
	if (!strcmp(k, "files")) go("home", NULL); else if (!strcmp(k, "downloads")) go("dl", NULL); else if (!strcmp(k, "trash")) go("trash", NULL); else if (!strcmp(k, "flatpak")) go("applinux", NULL);
	else if (!strcmp(k, "steam")) go("steam", NULL); else if (!strcmp(k, "games")) go("emu", NULL); else if (!strcmp(k, "debian")) set_show("apps"); }
static int my_disk_acct(void)
{
	RuiJ *o = set_ls_get("bd95user"); RuiJ *u = o ? rui_jprop(o, "x") : NULL; int d = 0; if (u && rui_jtype(u) == 3) { RuiJ *p = rui_jparse(rui_jsval(u)); RuiJ *a = rui_jprop(p, "accts");
		for (int i = 0; a && i < rui_jlen(a); i++) if (!strcmp(set_jstr(rui_jat(a, i), "id", ""), ACCT)) d = jb(rui_jat(a, i), "disk"); rui_jfree(p); }
	else if (u) { RuiJ *a = rui_jprop(u, "accts"); for (int i = 0; a && i < rui_jlen(a); i++) if (!strcmp(set_jstr(rui_jat(a, i), "id", ""), ACCT)) d = jb(rui_jat(a, i), "disk"); }
	rui_jfree(o); return d;
}
static void disk_sec(void)
{
	p_h4("On this computer's disk"); if (!SDJ) { SDJ = get("storage"); RuiJ *r = rui_jprop(SDJ, "result"); if (!r || set_now() - jnum(r, "time") > 600) sd_measure(); }
	RuiJ *r = rui_jprop(SDJ, "result");
	if (!r) { p_label_named("sdm", p_msg(jb(SDJ, "measuring") ? "Measuring… (a large Steam library takes a little while)" : SDJ ? "Not measured yet." : "Measuring…", 0)); p_bar(0, 0); }
	else { RuiJ *dk = rui_jprop(r, "disk"); double u = jnum(dk, "used"), t = jnum(dk, "total"), f = jnum(dk, "free"); char *a = fmt_b(u == u ? u : 0), *b = fmt_b(t == t ? t : 0), *c = fmt_b(f == f ? f : 0), tm[32]; time_t tt = (time_t)jnum(r, "time"); struct tm lt; localtime_r(&tt, &lt); strftime(tm, sizeof tm, "%I:%M %p", &lt);
		char *m = g_strdup_printf("%s used of %s · %s free · measured %s%s", a, b, c, tm, jb(SDJ, "measuring") ? " · measuring again…" : ""); p_label_named("sdm", p_msg(m, 0)); g_free(m); g_free(a); g_free(b); g_free(c);
		p_bar(fmax(.01, t > 0 ? u / t : 0), 0); }
	NTab *t = ntab(4);
	if (r) { RuiJ *it = rui_jprop(r, "items"), *ac = rui_jprop(r, "accounts"), *me = NULL; double v[11] = { 0 };
		if (my_disk_acct()) for (int i = 0; ac && i < rui_jlen(ac); i++) if (!strcmp(set_jstr(rui_jat(ac, i), "id", ""), ACCT)) me = rui_jat(ac, i);
		for (int k = 0; k < 11; k++) { double x = jnum(it, LINES[k][0]); v[k] = x == x ? x : 0; }
		if (me) { double o = 0; for (int i = 0; ac && i < rui_jlen(ac); i++) { RuiJ *a = rui_jat(ac, i); if (a == me) continue; double x[3] = { jnum(a, "files"), jnum(a, "trash"), jnum(a, "downloads") }; for (int q = 0; q < 3; q++) if (x[q] == x[q]) o += x[q]; }
			v[0] = jnum(me, "files"); v[3] = jnum(me, "trash"); v[1] = o; } else v[1] = 0;
		int ord[11], n = 0; for (int k = 0; k < 11; k++) if (v[k] > 0) ord[n++] = k;
		for (int i = 1; i < n; i++) for (int j = i; j > 0 && v[ord[j]] > v[ord[j - 1]]; j--) { int x = ord[j]; ord[j] = ord[j - 1]; ord[j - 1] = x; }
		double mx = n ? v[ord[0]] : 1;
		for (int i = 0; i < n; i++) { int k = ord[i]; TRow *w = nrow(t); w->c[0].k = C_LINE; w->c[0].icon = g_strdup(LINES[k][1]); w->c[0].b = g_strdup(LINES[k][2]); w->c[0].s = g_strdup(LINES[k][3]);
			w->c[1].k = C_BAR; w->c[1].frac = fmax(.01, v[k] / mx); char *s = fmt_b(v[k]); ctext(&w->c[2], s); w->c[2].right = 1; g_free(s); w->c[3].k = C_BTN;
			const char *l = !strcmp(LINES[k][0], "files") || !strcmp(LINES[k][0], "downloads") ? "Open" : !strcmp(LINES[k][0], "trash") ? "Open the Trash" : !strcmp(LINES[k][0], "flatpak") || !strcmp(LINES[k][0], "debian") ? "Manage"
				: !strcmp(LINES[k][0], "steam") && *set_jstr(SNAP, "steam", "") ? "Open Steam" : !strcmp(LINES[k][0], "games") && *set_jstr(SNAP, "emu", "") ? "Open Emu" : NULL;
			if (l) { w->c[3].w = rui_button(l, 0); g_signal_connect(w->c[3].w, "clicked", G_CALLBACK(on_sda), (gpointer)LINES[k][0]); p_put(w->c[3].w); char nm[24]; snprintf(nm, sizeof nm, "sda_%s", LINES[k][0]); p_label_named(nm, w->c[3].w); } } }
	p_custom(ntab_lay, t, 0, 0);
	BRow *b = g_new0(BRow, 1); bbtn(b, "Measure again", 0, G_CALLBACK(on_measure), NULL, "sda"); p_custom(brow_lay, b, 14, 0);
}

/* ================================================================== The Trash (storage56.js's): how long items stay (bd95trauto:ACCT) */
static int TRDEL_ASKED;
static void on_trad(const char *v, void *d) { (void)d; char k[120]; snprintf(k, sizeof k, "bd95trauto:%s", ACCT); set_ls_set(k, v); set_note("svj %s %s", k, v); TRDEL_ASKED = 1; snap_ask("trauto", NULL); }
typedef struct { GtkWidget *a, *s; } TradRow;
static double trad_lay(void *d, double x, double y, double w)
{
	(void)w; TradRow *r = d; const char *t = "Delete items in the Trash for good once they are older than"; double lw = tw(t, 13, 400); p_place(r->a, x, y, lw + 2, 25); p_place(r->s, x + lw + sp_w(), y, 0, 0); return 25;
}
static gboolean trad_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)w; (void)d; s_col(cr, "fg", 1); s_text(cr, "Delete items in the Trash for good once they are older than", 0, 5, 13, 400, 0, 15); return FALSE; }
static void trash_sec(void)
{
	p_h4("The Trash"); double n = jnum(SNAP, "trN"), b = jnum(SNAP, "trB"); GString *m = g_string_new(NULL); double del = jnum(SNAP, "trDel");
	if (TRDEL_ASKED && del == del && del > 0) g_string_append_printf(m, "%.0f old item%s deleted. ", del, del == 1 ? " was" : "s were");
	if (n > 0) { char *s = fmt_b(b); g_string_append_printf(m, "%.0f item%s in your Trash, %s.", n, n == 1 ? "" : "s", s); g_free(s); } else g_string_append(m, "Your Trash is empty.");
	p_label_named("tras", p_msg_m(m->str, 0, 10, 13)); g_string_free(m, TRUE);
	static const char *const V[5] = { "0", "1", "14", "30", "60" }, *const L[5] = { "Never", "1 day", "14 days", "30 days", "60 days" };
	double days = jnum(SNAP, "trAuto"); char cur[16]; snprintf(cur, sizeof cur, "%.0f", days == days ? days : 0);
	TradRow *r = g_new0(TradRow, 1); r->a = gtk_drawing_area_new(); g_signal_connect(r->a, "draw", G_CALLBACK(trad_draw), NULL); p_put(r->a); r->s = s_select(V, L, 5, cur, on_trad, NULL); s_select_width(r->s, 0); p_put(r->s); p_label_named("trad", r->s);
	if (!ACCT[0] || jb(SNAP, "guest")) gtk_widget_set_sensitive(r->s, FALSE);
	p_custom(trad_lay, r, 0, 0); p_custom_find("Delete items in the Trash for good once they are older than", 0);
}

/* ================================================================== Backup (storPanel's, with stick30.js's Back up to a USB stick) */
static RuiJ *INFO, *DISKS, *UST, *IMGS;
static const char *own_disk(char *b, size_t n)   /* (stkDisk(info.mediaDev)) */
{
	const char *m = set_jstr(INFO, "mediaDev", ""); if (!strncmp(m, "/dev/", 5)) m += 5; snprintf(b, n, "%s", m);
	if (g_regex_match_simple("^(nvme\\d+n\\d+|mmcblk\\d+)p\\d+$", b, 0, 0)) *strrchr(b, 'p') = 0; else if (g_regex_match_simple("^(sd|vd)[a-z]+\\d+$", b, 0, 0)) { char *p = b + strlen(b); while (p > b && g_ascii_isdigit(p[-1])) *--p = 0; }
	return b;
}
static void on_stkbak(GtkButton *b, gpointer d) { (void)b; go("stkbak", d); }
static void backup_sec(void)
{
	p_h4("Backup"); p_msg_m("A backup contains every account's files (still encrypted), settings and apps.", 0, 10, 13);
	p_msg_m("\x03<b>Back up everything</b>: a backup file to keep somewhere safe (another drive, a stick, the cloud); restore it here or on another Rudolf OS.", 0, 10, 13);
	BRow *r = g_new0(BRow, 1); bbtn(r, "Back up everything…", 1, G_CALLBACK(on_go), "backup", "bk"); bbtn(r, "Restore a backup…", 0, G_CALLBACK(on_go), "restore", "rs"); p_custom(brow_lay, r, 14, 0);
	if (INFO) { char *t; if (jb(INFO, "live")) t = g_strdup("Running live: nothing is kept after you shut down. Install Rudolf OS or add a RUDOLFDATA partition to keep your work.");
		else { char *f = fmt_b(jnum(INFO, "persistFree")), *a = fmt_b(jnum(INFO, "persistTotal")); t = g_strdup_printf("Saved on the RUDOLFDATA partition: %s free of %s.", f, a); g_free(f); g_free(a); }
		p_label_named("sti3", p_msg(t, 0)); g_free(t); } else p_msg("", 0);
	p_msg_m("\x03<b>Back up to a USB stick</b>: the same backup, written straight to a plugged-in drive (in its RUDOLF-BACKUP folder, with a checksum). Use it before reinstalling or when moving to a new computer: a new Rudolf OS finds it and offers to restore it when it first starts.", 0, 10, 13);
	char od[64]; own_disk(od, sizeof od); RuiJ *dl = rui_jprop(DISKS, "disks"); Acr *a = NULL; int nb = 0;
	for (int i = 0; dl && i < rui_jlen(dl); i++) { RuiJ *k = rui_jat(dl, i); if (!jb(k, "removable") || !strcmp(set_jstr(k, "name", ""), od)) continue; RuiJ *ps = rui_jprop(k, "partitions");
		for (int q = 0; ps && q < rui_jlen(ps); q++) { RuiJ *p = rui_jat(ps, q); const char *fs = set_jstr(p, "fs", ""), *lb = set_jstr(p, "label", "");
			if (!*fs || !strcmp(fs, "iso9660") || !strcmp(fs, "swap") || !strcmp(lb, "RUDOLFDATA") || !strcmp(lb, "RUDOLF EFI")) continue;
			if (!a) a = acr(); char *g = gb(jnum(p, "bytes")), *t = g_strdup_printf("🔌 %s — %s — %s (%s)", *lb ? lb : set_jstr(p, "name", ""), *set_jstr(k, "model", "") ? set_jstr(k, "model", "") : "USB drive", g, fs), nm[16];
			snprintf(nm, sizeof nm, "stkb%d", nb++); acr_row(a, t, "Back up here", G_CALLBACK(on_stkbak), g_strdup(set_jstr(p, "name", "")), nm); g_free(g); g_free(t); } }
	if (a) p_custom(acr_lay, a, 0, 0); else p_msg_m("Plug in a USB stick or drive to back up to.", 0, 10, 13);
	p_msg("", 0);
	p_msg_m("\x03<b>A Rudolf OS stick with your files</b>: “Make a Rudolf OS USB stick” below, with “Also copy my files and settings”. Use it to take Rudolf OS itself, with your files, to another computer.", 0, 10, 13);
}

/* ================================================================== Make a Rudolf OS USB stick (stick30.js stkSection) */
static const char *const BOOTOLD = "This computer started from a disc made before Rudolf OS supported Secure Boot (1.2.2). A stick made from it will not start with Secure Boot on: choose a downloaded image instead.";
static char *SKV;          /* (the choice: "@address", "" this Rudolf OS, or an image's path) */
static int SKF, SKBUSY;    /* (Also copy my files; a stick being made) */
static char *SKPT, *SKOK, *SKJ; static double SKP = -1;   /* (the progress line, the end's line, the job's lines; the bar's percent, <0 hidden) */
static RuiJ *img_of(const char *v) { RuiJ *l = rui_jprop(IMGS, "images"); for (int i = 0; v && *v && l && i < rui_jlen(l); i++) if (!strcmp(set_jstr(rui_jat(l, i), "path", ""), v)) return rui_jat(l, i); return NULL; }
static char *img_label(RuiJ *x) { double p = jnum(x, "parts"); return p == p && p ? g_strdup_printf("%s (%.0f parts, not joined yet)", set_jstr(x, "name", ""), p) : g_strdup(set_jstr(x, "name", "")); }
static char *stk_what(const char *v)
{
	if (!strcmp(v, "@address")) return g_strdup("The newest Rudolf OS from the update address, downloaded first and checked against its signature; then the same as below.");
	if (!*v) return g_strdup_printf("This Rudolf OS as it is on this computer%s: the stick starts on its own on any computer and keeps its own files.", jb(INFO, "updateInstalled") ? ", with its updates" : "");
	RuiJ *im = img_of(v); const char *n = im ? set_jstr(im, "name", "") : strrchr(v, '/') ? strrchr(v, '/') + 1 : v;
	if (im && g_regex_match_simple("-boot\\.iso$", n, G_REGEX_CASELESS, 0)) return g_strdup_printf("Boot files only (%s): the start-up files come from that image, and Rudolf OS itself from this computer.", n);
	return g_strdup_printf("A whole disc image (%s): everything on the stick comes from that file.", n);
}
static char *too_small(RuiJ *k) { double b = jnum(k, "bytes"); RuiJ *im = img_of(SKV); if (b < 1.8e9) return g_strdup("Too small: Rudolf OS needs a stick of at least 1.8 GB. Nothing is written.");
	double ib = im ? jnum(im, "bytes") : NAN; if (ib == ib && ib && b < ib + 256e6) { char *g = gb(ib + 256e6), *t = g_strdup_printf("Too small for %s: it needs about %s. Nothing is written.", set_jstr(im, "name", ""), g); g_free(g); return t; } return NULL; }
static void on_stks(const char *v, void *d) { (void)d; g_free(SKV); SKV = g_strdup(v); redraw(); }
static void on_stkf(int on, void *d) { (void)d; SKF = on; }
/* (STK_STEPS: the job's lines as the bar's steps) */
static const struct { const char *re; int v; const char *w; } STEPS[] = { { "^Getting the latest", 2, "Downloading the latest Rudolf OS" }, { "^Step 1/5", 5, "Writing Rudolf OS to the stick" }, { "^Step 2/5", 45, "Checking what was written" },
	{ "^The copy matches", 70, "Checked: the copy matches" }, { "^Step 3/5", 72, "Making its partitions" }, { "^Step [45]/5", 78, "Making its partitions" }, { "^Checking the installed system", 84, "Checking its system" },
	{ "^Checking the new stick", 88, "Checking the new stick" }, { "^Copying your files", 92, "Copying your files and settings" }, { "^Done:", 100, "Done" } };
static char *SKLAB; static int SKCHK; static double SKT0; static char SKDISK[32];
static void stk_show(void) { double s = set_now() - SKT0; char left[64] = ""; if (SKP >= 5 && SKP < 100 && s >= 10) { int r = (int)fmax(1, round(s * (100 - SKP) / SKP / 60)); snprintf(left, sizeof left, " · about %d minute%s left", r, r == 1 ? "" : "s"); }
	g_free(SKPT); SKPT = g_strdup_printf("%s (%.0f %%)%s", SKLAB ? SKLAB : "Starting…", SKP, left); }
static int SKSEEN;
static gboolean stk_tick(gpointer d)
{
	(void)d; RuiJ *j = get("job"); if (!j) return G_SOURCE_CONTINUE; const char *log = set_jstr(j, "log", ""); char **ls = g_strsplit(log, "\n", -1); int n = g_strv_length(ls);
	for (; SKSEEN < n - 1; SKSEEN++) { const char *l = ls[SKSEEN]; for (guint i = 0; i < G_N_ELEMENTS(STEPS); i++) if (g_regex_match_simple(STEPS[i].re, l, 0, 0) && STEPS[i].v >= SKP) { SKP = STEPS[i].v; g_free(SKLAB); SKLAB = g_strdup(STEPS[i].w); if (STEPS[i].v == 70) SKCHK = 1; } }
	g_strfreev(ls); g_free(SKJ); SKJ = g_strdup(log); stk_show();
	double st = jnum(j, "status"); if (!jb(j, "running") && st == st && st >= 0) { int ok = st == 0; if (ok) { SKP = 100; g_free(SKLAB); SKLAB = g_strdup("Done"); stk_show(); }
		g_free(SKOK); SKOK = g_strdup(ok ? (SKCHK ? "✅ Checked: what is on the stick matches. It is safe to remove the stick now." : "✅ It is safe to remove the stick now.") : "❌ The stick could not be made (see the messages below). A stick that was being written is cleared, so it is never left half-made.");
		char *ll = g_strstrip(g_strdup(log)), *p = strrchr(ll, '\n');
		if (ok) { char *b = g_strdup_printf("%s can be removed. Start a computer from it with its boot menu (Esc or F8 on ASUS, often F12); Recovery mode on it starts this computer's Rudolf OS with safe settings.%s", SKDISK, strstr(log, "Warning: the stick gets this drive") ? " (Its boot files are older than this system: connect to the internet and make it again for Recovery mode.)" : ""); set_notify("The Rudolf OS USB stick is ready", b, 0); g_free(b); }
		else set_notify("The stick could not be made", *ll ? (p ? p + 1 : ll) : "See Settings › Storage and backup.", 1);
		g_free(ll); SKBUSY = 0; rui_jfree(j); redraw(); return G_SOURCE_REMOVE; }
	rui_jfree(j); redraw(); return G_SOURCE_CONTINUE;
}
static void stk_go(RuiJ *k)
{
	char *e = g_uri_escape_string(SKV, NULL, FALSE), *p = g_strdup_printf("mkstick?disk=%s&confirm=%s&image=%s&files=%d", set_jstr(k, "name", ""), set_jstr(k, "name", ""), e, SKF ? 1 : 0), er[400]; g_free(e);
	SKBUSY = 1; g_free(SKJ); SKJ = g_strdup(""); g_free(SKOK); SKOK = NULL; SKP = 0; g_free(SKLAB); SKLAB = NULL; SKCHK = 0; SKSEEN = 0; SKT0 = set_now(); snprintf(SKDISK, sizeof SKDISK, "%s", set_jstr(k, "name", "")); stk_show();
	RuiJ *j = post(p, er, sizeof er); g_free(p); if (!j) { SKBUSY = 0; SKP = -1; g_free(SKJ); SKJ = g_strdup(er); redraw(); return; } rui_jfree(j); redraw(); g_timeout_add(900, stk_tick, NULL);
}
/* (stkConfirm: the administrator types the size shown) */
static void cf_changed(GtkEditable *e, gpointer d) { char *v = g_strstrip(g_ascii_strup(gtk_entry_get_text(GTK_ENTRY(e)), -1)), *w = g_ascii_strup(d, -1); GRegex *re = g_regex_new("\\s+", 0, 0, NULL); char *v2 = g_regex_replace_literal(re, v, -1, 0, " ", 0, NULL); g_regex_unref(re);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(gtk_widget_get_toplevel(GTK_WIDGET(e))), GTK_RESPONSE_OK, !strcmp(v2, w)); g_free(v); g_free(v2); g_free(w); }
static int stk_confirm(RuiJ *k)
{
	char *sz = gb(jnum(k, "bytes")); GtkWidget *dl = gtk_dialog_new_with_buttons(rui_tr("Erase this drive?"), GTK_WINDOW(SW->win), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, rui_tr("Cancel"), GTK_RESPONSE_CANCEL, rui_tr("Erase and make the stick"), GTK_RESPONSE_OK, NULL);
	GtkWidget *ca = gtk_dialog_get_content_area(GTK_DIALOG(dl)); gtk_container_set_border_width(GTK_CONTAINER(ca), 12);
	GString *pl = g_string_new(NULL); RuiJ *ps = rui_jprop(k, "partitions"); for (int i = 0; ps && i < rui_jlen(ps); i++) { RuiJ *p = rui_jat(ps, i); const char *fs = set_jstr(p, "fs", ""), *lb = set_jstr(p, "label", ""); if (!*fs) continue; if (pl->len) g_string_append(pl, ", "); if (*lb) g_string_append_printf(pl, "“%s” (%s)", lb, fs); else g_string_append(pl, fs); }
	char *t1 = g_strdup_printf("%s — %s — %s", set_jstr(k, "name", ""), *set_jstr(k, "model", "") ? set_jstr(k, "model", "") : "USB drive", sz), *t2 = pl->len ? g_strdup_printf("Everything on it is erased: %s, with its files and name. If it is open in File Explorer, Rudolf OS closes it first.", pl->str) : g_strdup("Everything on it is erased."), *t3 = g_strdup_printf("To go on, type its size: %s", sz);
	const char *ts[3] = { t1, t2, t3 }; for (int i = 0; i < 3; i++) { GtkWidget *l = gtk_label_new(ts[i]); gtk_label_set_xalign(GTK_LABEL(l), 0); gtk_label_set_line_wrap(GTK_LABEL(l), TRUE); gtk_label_set_max_width_chars(GTK_LABEL(l), 60); gtk_box_pack_start(GTK_BOX(ca), l, FALSE, FALSE, 4); }
	GtkWidget *e = rui_entry(NULL); g_signal_connect(e, "changed", G_CALLBACK(cf_changed), sz); gtk_widget_set_size_request(e, (int)lround(120 * s_z()), -1); gtk_widget_set_halign(e, GTK_ALIGN_START); gtk_box_pack_start(GTK_BOX(ca), e, FALSE, FALSE, 4);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(dl), GTK_RESPONSE_OK, FALSE); gtk_widget_show_all(dl); p_label_named("stki", e); set_note("ask stkconfirm %s", sz); set_state_soon();
	int ok = gtk_dialog_run(GTK_DIALOG(dl)) == GTK_RESPONSE_OK; gtk_widget_destroy(dl); g_free(t1); g_free(t2); g_free(t3); g_string_free(pl, TRUE); g_free(sz); return ok;
}
static RuiJ *SKK;   /* (the drive being asked about) */
static void stk_allowed(int ok, void *d) { (void)d; if (!ok || !SKK) return; if (!stk_confirm(SKK)) return; stk_go(SKK); }
static void stk_make(RuiJ *k)
{
	SKK = k; char *g = gb(jnum(k, "bytes")), *r = g_strdup_printf("Make a Rudolf OS USB stick on %s (%s, %s). Everything on it is erased.", set_jstr(k, "name", ""), set_jstr(k, "model", ""), g); set_note("elevate %s", r); rui_elevate(SW, r, stk_allowed, NULL); g_free(g); g_free(r);
}
static void other_ok(int ok, void *d) { (void)d; if (ok && SKK) stk_make(SKK); }
static void on_stkmake(GtkButton *b, gpointer d)
{
	(void)b; RuiJ *dl = rui_jprop(DISKS, "disks"), *k = NULL; for (int i = 0; dl && i < rui_jlen(dl); i++) if (!strcmp(set_jstr(rui_jat(dl, i), "name", ""), d)) k = rui_jat(dl, i); if (!k) return; char *ts = too_small(k); if (ts) { g_free(ts); return; }
	if (strcmp(SKV, "@address")) { stk_make(k); return; }
	/* (the update address's release checked first, as Settings' Check this address; another person's build said so before anything is erased) */
	const char *url = set_jstr(UST, "url", ""); char *e = g_uri_escape_string(url, NULL, FALSE), *p = g_strdup_printf("sys/urlcheck?url=%s", e), er[400]; g_free(e); g_free(SKJ); SKJ = g_strdup_printf("Checking %s …", url); redraw();
	RuiJ *ck = post(p, er, sizeof er); g_free(p); if (!ck) { g_free(SKJ); SKJ = g_strdup(er); redraw(); return; }
	GString *L = g_string_new(NULL); RuiJ *ln = rui_jprop(ck, "lines"); int noboot = 0;
	for (int i = 0; ln && i < rui_jlen(ln); i++) { RuiJ *x = rui_jat(ln, i); const char *lv = rui_jsval(rui_jat(x, 0)) ? rui_jsval(rui_jat(x, 0)) : "", *t = rui_jsval(rui_jat(x, 1)) ? rui_jsval(rui_jat(x, 1)) : "";
		g_string_append_printf(L, "%s%s%s", i ? "\n" : "", !strcmp(lv, "ok") ? "✅ " : !strcmp(lv, "warn") ? "⚠️ " : "❌ ", t); if (strstr(t, "no boot image")) noboot = 1; }
	g_free(SKJ); SKJ = g_string_free(L, FALSE); redraw(); const char *key = set_jstr(ck, "key", "");
	if (!jb(ck, "good") && strcmp(key, "other")) { set_notify("Make a stick", "The update address is not set up properly (see the lines above), so no stick is made from it.", 1); rui_jfree(ck); return; }
	if (noboot) { set_notify("Make a stick", "That release has no boot image, so a stick cannot be made from it.", 1); rui_jfree(ck); return; }
	if (!strcmp(key, "other")) { SKK = k; char *r = g_strdup_printf("This is another build of Rudolf OS (Rudolf OS %s, key %s), from %s. The stick will get its updates from there, not from this computer's. With Secure Boot on it starts only where its maker's Secure Boot key is trusted. Its Recovery mode is the official Rudolf OS. Make the stick with it?", set_jstr(ck, "version", ""), set_jstr(ck, "fpr", ""), url);
		rui_elevate(SW, r, other_ok, NULL); g_free(r); rui_jfree(ck); return; }
	rui_jfree(ck); stk_make(k);
}
static char *stk_info_text(RuiJ *j)
{
	if (!j || !jb(j, "rudolf") || !rui_jprop(j, "info")) return NULL; RuiJ *i = rui_jprop(j, "info"); char v[4][80]; const char *ks[4] = { "image", "system", "rescue", "" };
	for (int q = 0; q < 3; q++) { RuiJ *x = rui_jprop(i, ks[q]); const char *ver = set_jstr(x, "version", ""); snprintf(v[q], sizeof v[q], *ver ? "Rudolf OS %s" : "Rudolf OS", ver); }
	int sys = rui_jprop(i, "system") && rui_jtype(rui_jprop(i, "system")) == 5, res = rui_jprop(i, "rescue") && rui_jtype(rui_jprop(i, "rescue")) == 5;
	if (!jb(i, "bootonly")) { if (sys) return g_strdup_printf("On it: a whole disc image (%s), updated to %s. It starts on its own.", v[0], v[1]); return g_strdup_printf("On it: a whole disc image (%s). It starts on its own.", v[0]); }
	if (sys && res) return g_strdup_printf("On it: a full rescue stick, %s, with its rescue system (%s).", v[1], v[2]);
	if (sys) return g_strdup_printf("On it: %s, without a rescue system (made before 1.5.8). Make it again to add one.", v[1]);
	if (res) return g_strdup_printf("On it: only its rescue system (%s) can start. Make it again.", v[2]);
	return g_strdup_printf("On it: boot files only (%s), with no system: it cannot start on its own. Make it again.", v[0]);
}
static void stick_sec(void)
{
	p_h4("Make a Rudolf OS USB stick");
	p_msg_m("Puts Rudolf OS on a USB stick or memory card (1.8 GB or more) that starts on any computer and keeps its files. Everything on that drive is erased.", 0, 10, 13);
	p_msg_m("\x03It is also a <b>rescue stick</b> for this computer: start from it and choose <b>Recovery mode</b> in its menu to start this computer's own Rudolf OS with safe settings (the Chromium desktop engine, no shell, safe graphics); a normal start runs the stick's own Rudolf OS. The stick gets the boot files of this Rudolf OS, downloaded when this drive's are older (Help › Starting from a USB stick).", 0, 10, 13);
	p_msg_m("\x03" "Each stick is a <b>full rescue stick</b>: this computer's system is copied onto it and kept a second time as its <b>rescue system</b>, which the stick's own updates never change, so it can always start. The stick's size is checked before anything is erased, and a stick that could not be finished is cleared, never left half-made.", 0, 10, 13);
	const char *v[24], *l[24]; char *own[24]; int n = 0, no = 0; const char *url = set_jstr(UST, "url", ""); RuiJ *pv = rui_jprop(INFO, "persistent");
	if (*url && !(pv && rui_jtype(pv) == 1 && !rui_jval(pv))) { v[n] = "@address"; own[no++] = g_strdup_printf("The latest from the update address (%s)", !strncmp(url, "https://", 8) ? url + 8 : url); l[n++] = own[no - 1]; }
	v[n] = ""; own[no++] = g_strdup_printf("This Rudolf OS %s%s", rui_version(), jb(INFO, "updateInstalled") ? " (with its updates)" : ""); l[n++] = own[no - 1];
	RuiJ *im = rui_jprop(IMGS, "images"); for (int i = 0; im && i < rui_jlen(im) && n < 22; i++) { RuiJ *x = rui_jat(im, i); if (jb(x, "boot")) continue; char *lb = img_label(x); v[n] = set_jstr(x, "path", ""); own[no++] = g_strdup_printf("%s (Downloads)", lb); l[n++] = own[no - 1]; g_free(lb); }
	if (!SKV) SKV = g_strdup(v[0]); { int have = 0; for (int i = 0; i < n; i++) if (!strcmp(v[i], SKV)) have = 1; if (!have) { g_free(SKV); SKV = g_strdup(v[0]); } }
	Cell *c = p_r2("Rudolf OS to put on it"); GtkWidget *s = s_select(v, l, n, SKV, on_stks, NULL); c_add(c, s, 0, 0, 1); p_label_named("stks", s); for (int i = 0; i < no; i++) g_free(own[i]);
	{ char *w = stk_what(SKV); p_label_named("stkw", p_msg_m(w, 0, 10, 13)); g_free(w); }
	if (jnum(INFO, "bootSecure") == 0 && !*SKV) p_msg_m(BOOTOLD, 0, 10, 13);
	p_label_named("stkf", p_check("Also copy my files and settings (Linux apps are not copied)", SKF, on_stkf, NULL));
	char od[64]; own_disk(od, sizeof od); RuiJ *dl = rui_jprop(DISKS, "disks"); Acr *a = NULL; int nt = 0, adm = set_admin();
	for (int i = 0; dl && i < rui_jlen(dl); i++) { RuiJ *k = rui_jat(dl, i); if (!jb(k, "removable") || jnum(k, "bytes") < 1.8e9 || !strcmp(set_jstr(k, "name", ""), od)) continue;
		int iso = 0; RuiJ *ps = rui_jprop(k, "partitions"); for (int q = 0; ps && q < rui_jlen(ps); q++) if (!strcmp(set_jstr(rui_jat(ps, q), "fs", ""), "iso9660")) iso = 1;
		if (!a) a = acr(); char *g = gb(jnum(k, "bytes")), *t = g_strdup_printf("🔌 %s — %s — %s%s", set_jstr(k, "name", ""), *set_jstr(k, "model", "") ? set_jstr(k, "model", "") : "USB drive", g, iso ? " (an older Rudolf OS stick)" : ""), nm[16];
		snprintf(nm, sizeof nm, "stkm%d", nt++); GtkWidget *b = acr_row(a, t, "Make the stick…", G_CALLBACK(on_stkmake), g_strdup(set_jstr(k, "name", "")), nm); char *ts = too_small(k); if (b) gtk_widget_set_sensitive(b, adm && !ts && !SKBUSY); g_free(g); g_free(t);
		if (iso || ts) { p_custom(acr_lay, a, 0, 0); a = NULL; if (iso) { char q2[80]; snprintf(q2, sizeof q2, "stick/info?disk=%s", set_jstr(k, "name", "")); RuiJ *j = get(q2); char *it = stk_info_text(j); if (it) p_msg(it, 0); g_free(it); rui_jfree(j); }
			if (ts) p_msg(ts, 0); } g_free(ts); }
	if (a) p_custom(acr_lay, a, 0, 0);
	if (!nt) p_msg_m("Plug in a USB stick or memory card of 1.8 GB or more (not the one Rudolf OS runs from), then open this page again.", 0, 10, 13);
	if (!adm && nt) p_msg_m("An administrator can make a stick.", 0, 10, 13);
	if (SKP >= 0) { p_bar(SKP / 100, 0); p_label_named("stkpt", p_msg(SKPT ? SKPT : "Starting…", 0)); }
	if (SKOK) p_label_named("stkok", p_msg_m(SKOK, 0, 10, 13));
	if (SKJ && *SKJ) p_label_named("stkj", p_msg_m(SKJ, 0, 13, 13));
}

/* ================================================================== Keep files on this drive (keep29.js) */
static void on_restart(GtkButton *b, gpointer d) { (void)b; (void)d; char er[200]; RuiJ *j = post("power?action=reboot", er, sizeof er); rui_jfree(j); }
static void keep_sec(void)
{
	RuiJ *s = get("keepfiles"); if (!s) return; const char *why = set_jstr(s, "reason", "");
	if (!jb(s, "possible") && !jb(s, "pending") && strstr(why, "already keeps")) { rui_jfree(s); return; }
	p_h4("Keep files on this drive");
	if (jb(s, "pending")) { p_msg_m("Ready: restart Rudolf OS to finish. Everything you made in this session moves onto the drive.", 0, 10, 13); BRow *r = g_new0(BRow, 1); bbtn(r, "Restart now", 1, G_CALLBACK(on_restart), NULL, "kfr"); p_custom(brow_lay, r, 14, 0); }
	else if (jb(s, "possible")) { char *f = fmt_b(jnum(s, "freeBytes")), *t = g_strdup_printf("Rudolf OS is running live. This drive has %s of free space after Rudolf OS: use it to keep your files, settings and apps. Rudolf OS itself is not changed.", f); p_msg_m(t, 0, 10, 13); g_free(f); g_free(t);
		BRow *r = g_new0(BRow, 1); GtkWidget *b = bbtn(r, "Keep my files on this drive…", 1, G_CALLBACK(on_go), "keep", "kfm"); gtk_widget_set_sensitive(b, set_admin()); p_custom(brow_lay, r, 14, 0); }
	else { char *t = g_strdup_printf("Your files cannot be kept on this drive: %s.", *why ? why : "unknown reason"); p_msg_m(t, 0, 10, 13); g_free(t); }
	rui_jfree(s);
}

void page_stor(void)
{
	snap_load(); SNAP_T = jnum(SNAP, "t"); if (SNAP_T != SNAP_T) SNAP_T = 0;
	if (set_now() - ASKED_T > 30) snap_ask("measure", NULL);   /* (the figures worked out again when the page opens) */
	rui_jfree(INFO); INFO = get("info"); rui_jfree(DISKS); DISKS = get("disks"); rui_jfree(UST); UST = get("sys/status"); rui_jfree(IMGS); { char p[160]; char *e = g_uri_escape_string(ACCT, NULL, FALSE); snprintf(p, sizeof p, "install/images%s%s", ACCT[0] ? "?acct=" : "", ACCT[0] ? e : ""); g_free(e); IMGS = get(p); }
	p_h3("Storage and backup"); top_sec(); dl_sec(); disk_sec(); trash_sec(); backup_sec(); stick_sec(); keep_sec();
}
