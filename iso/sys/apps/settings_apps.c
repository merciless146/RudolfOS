/*
 * rudolf-settings: Apps (settings25.js appsPanel, with links60.js's Links, term26.js's Linux (Debian 13) and wineold62.js's row beside
 * Wine, blitzen26.js's Blitzen (assistant), tools56.js's For developers, blitzknow60.js's Blitzen: what it knows, blzdev60.js's Blitzen ›
 * Developer and phonecalls59.js's Calls to phones). 1.6.3, docs/plan-1.6.3.md Part 3 (PR 7). As the page:
 * - Default apps: each kind of file with the apps that open it (files25.js OPENERS, tried as appsPanel tries them; the Linux apps that
 *   say they open it, LX_MIME), kept at once in bd95defapps:ACCT as the page's svj;
 * - Links: the Linux apps' own kinds of link (x-scheme-handler/, LK_NO left out), kept in bd95links:ACCT;
 * - Installed apps: the App Store's (bd95rapps), Uninstall with the administrator's prompt;
 * - Linux: rudolfd's debian/status, debian/check, debian/update, debian/remove, debian/wine (the job followed as lxJobWait follows it),
 *   bd95debauto; the Windows programs from before (wine/old) and the Linux password in the page's own dialogs (do-wineold,
 *   do-debpasswd: the native desktop's page of one action, as Task Manager's Check this computer…);
 * - Blitzen: ai/status, ai/unload, ai/remove, bd95blzkeep, bd95blzoff (the administrator's prompt), bd95blzspeed;
 * - For developers: bd95devtools;
 * - Blitzen: what it knows: ai/know/learn, ai/know/notes, ai/know/forget; See what Blitzen knows… (do-blzknows);
 * - Blitzen › Developer: ai/dev/settings, ai/dev/token (what is typed goes to rudolfd and is forgotten here); Open Blitzen Developer
 *   (do-blzdev);
 * - Calls to phones: PHONE59, each opened in Oracle (the shell's "browser URL"); S.telApp's line and Ask each time.
 * DEFAPPS, LXMIME, LXALIAS, LKNO and PHONE59 are read from the page at build time (mksettings.py). GPL-2.0-or-later.
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

static char *fmt_b(double b)
{
	if (b < 0) b = 0; if (b < 1024) return g_strdup_printf("%.0f B", b); if (b < 1048576) return g_strdup_printf("%.1f KB", b / 1024);
	if (b < 1073741824) return g_strdup_printf("%.2f MB", b / 1048576); return g_strdup_printf("%.1f GB", b / 1073741824);
}
static double tw(const char *t, double px, int wt) { double w = 0; s_lines(t ? t : "", px, wt, 0, &w); return w; }
static double sp_w(void) { double sp, a1; s_lines("a a", 13, 400, 0, &sp); s_lines("aa", 13, 400, 0, &a1); return sp - a1; }
static void redraw(void) { set_show_keep("apps"); }
static gboolean redraw_later(gpointer d) { (void)d; redraw(); return G_SOURCE_REMOVE; }
static void ask(const char *a) { rui_shell(SW, a); set_note("%s", a); }

/* rudolfd: lxApi's POST (JSON with ok, else its error), the answer parsed (NULL: none) */
static RuiJ *api(const char *path, const char *body, char *err, size_t en)
{
	char p[400], *out = NULL; snprintf(p, sizeof p, "/api/%s", path); int c = rui_http("POST", p, body, body ? strlen(body) : 0, &out, NULL);
	RuiJ *j = out ? rui_jparse(out) : NULL; g_free(out); set_note("POST %s %d", path, c);
	RuiJ *ok = j ? rui_jprop(j, "ok") : NULL;
	if (!j || !ok || rui_jtype(ok) != 1 || !rui_jval(ok)) { if (err) snprintf(err, en, "%s", j ? set_jstr(j, "error", set_jstr(j, "message", "Request failed")) : c ? "Request failed" : "rudolfd did not answer"); rui_jfree(j); return NULL; }
	if (err) *err = 0; return j;
}
static int jb(RuiJ *o, const char *k) { RuiJ *v = o ? rui_jprop(o, k) : NULL; return v && ((rui_jtype(v) == 1 && rui_jval(v)) || (rui_jtype(v) == 2 && rui_jval(v)) || (rui_jtype(v) == 3 && *rui_jsval(v))); }
static double jnum(RuiJ *o, const char *k) { RuiJ *v = o ? rui_jprop(o, k) : NULL; return v && rui_jtype(v) == 2 ? rui_jval(v) : 0; }
/* the page's ld(k, dflt) for true/false */
static int ld_bool(const char *k, int dflt) { RuiJ *o = set_ls_get(k); RuiJ *v = o ? rui_jprop(o, "x") : NULL; int r = v && rui_jtype(v) == 1 ? (int)rui_jval(v) : dflt; rui_jfree(o); return r; }
static void svj_bool(const char *k, int on) { set_ls_set(k, on ? "true" : "false"); set_note("svj %s %s", k, on ? "true" : "false"); }
/* one property of an object kept as the page keeps it (m=ld(k,{}); m[p]=v or delete m[p]; svj(k,m)) */
static void ls_prop(const char *k, const char *p, const char *vjson)
{
	RuiJ *o = set_ls_get(k); RuiJ *m = o ? rui_jprop(o, "x") : NULL; GString *b = g_string_new("{"); int first = 1;
	if (m && rui_jtype(m) == 5) for (int i = 0; i < rui_jlen(m); i++) { const char *q = rui_jkey(m, i); if (!strcmp(q, p)) continue;
		if (!first) g_string_append_c(b, ','); first = 0; char *e = set_jesc(q); g_string_append_printf(b, "\"%s\":", e); g_free(e); set_jw(b, rui_jprop(m, q)); }
	if (vjson) { if (!first) g_string_append_c(b, ','); char *e = set_jesc(p); g_string_append_printf(b, "\"%s\":%s", e, vjson); g_free(e); }
	g_string_append_c(b, '}'); set_ls_set(k, b->str); set_note("svj %s %s", k, b->str); g_string_free(b, TRUE); rui_jfree(o);
}

/* ================================================================== the page's .tmt (CSS's automatic table layout): each row a name and
   its value, a value being writing, a control (a select) or writing with a line of writing and a button after it (wineold62.js's) */
typedef struct { char *a, *b, *x; GtkWidget *w, *xb; } ARow;
typedef struct { GPtrArray *rows; GtkWidget *a; double c0, W; int white; } ATab;
static void arow_free(gpointer p) { ARow *r = p; g_free(r->a); g_free(r->b); g_free(r->x); g_free(r); }
static double word_max(const char *t, double px, int wt) { double m = 0; char **w = g_regex_split_simple("\\s+", t ? t : "", 0, 0); for (int i = 0; w[i]; i++) if (*w[i]) m = fmax(m, tw(w[i], px, wt)); g_strfreev(w); return m; }
static void atab_cols(ATab *t, double W)
{
	double mn[2] = { 0, 0 }, mx[2] = { 0, 0 };
	for (guint i = 0; i < t->rows->len; i++) { ARow *r = g_ptr_array_index(t->rows, i);
		mx[0] = fmax(mx[0], tw(r->a, 13, 400) + 12); mn[0] = fmax(mn[0], word_max(r->a, 13, 400) + 12);
		if (r->w) { double w = p_nat_w(r->w) + 12; mx[1] = fmax(mx[1], w); mn[1] = fmax(mn[1], w); }
		else { double x = tw(r->b, 13, 400); if (r->x) x = fmax(x, tw(r->x, 13, 400) + sp_w() + p_nat_w(r->xb)); mx[1] = fmax(mx[1], x + 12);
			double m = word_max(r->b, 13, 400); if (r->x) m = fmax(m, fmax(word_max(r->x, 13, 400), p_nat_w(r->xb))); mn[1] = fmax(mn[1], m + 12); } }
	if (mx[0] + mx[1] <= W) t->c0 = mx[0] + (W - mx[0] - mx[1]) * mx[0] / fmax(1, mx[0] + mx[1]);
	else { double sp = (mx[0] - mn[0]) + (mx[1] - mn[1]); t->c0 = mn[0] + (W - mn[0] - mn[1]) * (sp > 0 ? (mx[0] - mn[0]) / sp : .5); }
	t->W = W;
}
/* the writing and its button after it in a line (inline-block: its line 23 px tall, the writing on the button's line), its height */
static double x_lay(ARow *r, double cw, double *bx, double *by, int *last)
{
	PangoLayout *l = s_layout(r->x, 13, 400, cw); int n = pango_layout_get_line_count(l); PangoRectangle lr; pango_layout_line_get_pixel_extents(pango_layout_get_line_readonly(l, n - 1), NULL, &lr);
	double z = s_z(), lw = lr.width / z, bw = p_nat_w(r->xb); g_object_unref(l);
	if (lw + sp_w() + bw <= cw + .5) { *bx = lw + sp_w(); *by = (n - 1) * 15; *last = n - 1; return (n - 1) * 15 + 23; }
	*bx = 0; *by = n * 15; *last = n; return n * 15 + 23;
}
static double arow_h(ATab *t, ARow *r)
{
	double c1 = t->W - t->c0 - 12; int n0 = s_lines(r->a, 13, 400, t->c0 - 12, NULL); double h1;
	if (r->w) h1 = p_nat_h(r->w); else { h1 = *r->b ? s_lines(r->b, 13, 400, c1, NULL) * 15 : 0; if (r->x) { double bx, by; int ln; h1 += (*r->b ? 6 : 0) + x_lay(r, c1, &bx, &by, &ln); } }
	return fmax(n0 * 15, h1) + 8 + 1;
}
static double atab_lay(void *d, double x, double y, double w)
{
	ATab *t = d; atab_cols(t, w); double h = 0;
	for (guint i = 0; i < t->rows->len; i++) { ARow *r = g_ptr_array_index(t->rows, i); double rh = arow_h(t, r);
		if (r->w) p_place(r->w, x + t->c0 + 6, y + h + (rh - 1 - p_nat_h(r->w)) / 2, 0, 0);
		if (r->x) { double c1 = w - t->c0 - 12, bx, by; int ln; double hb = *r->b ? s_lines(r->b, 13, 400, c1, NULL) * 15 + 6 : 0, hx = x_lay(r, c1, &bx, &by, &ln), top = (rh - 1 - (hb + hx)) / 2;
			p_place(r->xb, x + t->c0 + 6 + bx, y + h + top + hb + by, 0, 0); }
		h += rh; }
	p_place(t->a, x, y, w, MAX(1, h)); return h;
}
static gboolean atab_draw(GtkWidget *wg, cairo_t *cr, gpointer d)
{
	ATab *t = d; double W = gtk_widget_get_allocated_width(wg) / s_z(), y = 0; if (fabs(W - t->W) > .1) atab_cols(t, W);
	for (guint i = 0; i < t->rows->len; i++) { ARow *r = g_ptr_array_index(t->rows, i); double rh = arow_h(t, r), c1 = W - t->c0 - 12;
		if (t->white) cairo_set_source_rgba(cr, 0, 0, 0, 0x0d / 255.0); else s_col(cr, "td", 1); cairo_rectangle(cr, 0, y + rh - 1, W, 1); cairo_fill(cr);
		if (t->white) cairo_set_source_rgb(cr, 1, 1, 1); else s_col(cr, "fg", 1);
		int n0 = s_lines(r->a, 13, 400, t->c0 - 12, NULL); s_text(cr, r->a, 6, y + (rh - 1 - n0 * 15) / 2, 13, 400, t->c0 - 12, 15);
		if (!r->w) { double hb = *r->b ? s_lines(r->b, 13, 400, c1, NULL) * 15 : 0, hx = 0, bx = 0, by = 0; int ln = 0; if (r->x) hx = (*r->b ? 6 : 0) + x_lay(r, c1, &bx, &by, &ln);
			double top = y + (rh - 1 - hb - hx) / 2; s_text(cr, r->b, t->c0 + 6, top, 13, 400, c1, 15);
			if (r->x) { PangoLayout *l = s_layout(r->x, 13, 400, c1); int n = pango_layout_get_line_count(l), si = ln < n ? pango_layout_get_line_readonly(l, ln)->start_index : (int)strlen(r->x); g_object_unref(l);
				char *p1 = g_strndup(r->x, si); double ty = top + hb + (*r->b ? 6 : 0); s_text(cr, p1, t->c0 + 6, ty, 13, 400, c1, 15); g_free(p1);
				if (r->x[si]) s_text(cr, r->x + si, t->c0 + 6, ty + ln * 15 + 4, 13, 400, c1, 15); } }
		y += rh; }
	return TRUE;
}
static ATab *atab(void)
{
	ATab *t = g_new0(ATab, 1); t->rows = g_ptr_array_new_with_free_func(arow_free); t->a = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(t->a), "atab", t, NULL);
	g_signal_connect(t->a, "draw", G_CALLBACK(atab_draw), t); p_put(t->a); return t;
}
static ARow *arow(ATab *t, const char *a, const char *b, GtkWidget *w) { ARow *r = g_new0(ARow, 1); r->a = g_strdup(rui_tr(a)); r->b = g_strdup(b ? b : ""); r->w = w; if (w) p_put(w); g_ptr_array_add(t->rows, r); return r; }

/* ================================================================== blocks of the page's own: buttons in a .rmrow (flex, wrapping, gap
   8 px), label.chk (inline) check boxes in a line, a line of bold writing */
typedef struct { GtkWidget *b[8]; int n; GtkWidget *e; double eh; } BRow;
static double brow_lay(void *d, double x, double y, double w)
{
	BRow *r = d; double cx = 0, cy = 0, lh = 0;
	if (r->e) { double bw = 0; for (int k = 0; k < r->n; k++) bw += 8 + p_nat_w(r->b[k]); p_place(r->e, x, y, w - bw, r->eh); cx = w - bw + 8; lh = r->eh; }
	for (int k = 0; k < r->n; k++) { double bw = p_nat_w(r->b[k]), bh = p_nat_h(r->b[k]); if (cx > 0 && cx + bw > w + .5) { cx = 0; cy += lh + 8; lh = 0; }
		p_place(r->b[k], x + cx, y + cy, 0, 0); cx += bw + 8; lh = fmax(lh, bh); }
	return cy + lh;
}
static GtkWidget *bbtn(BRow *r, const char *l, int pri, GCallback cb, void *d, const char *name)
{
	GtkWidget *b = rui_button(l, pri); if (cb) g_signal_connect(b, "clicked", cb, d); p_put(b); if (name) p_label_named(name, b); r->b[r->n++] = b; return b;
}
static BRow *brow(void) { return g_new0(BRow, 1); }
static void brow_done(BRow *r, double mt) { p_custom(brow_lay, r, mt, 0); }
typedef struct { GtkWidget *c[3]; char *t[3]; int n; } CRow;
static double crow_lay(void *d, double x, double y, double w)
{
	(void)w; CRow *r = d; double cx = 0; for (int k = 0; k < r->n; k++) { p_place(r->c[k], x + cx, y + 2, 0, 0); cx += p_nat_w(r->c[k]) + sp_w(); } return 19;
}
static double none_lay(void *d, double x, double y, double w) { (void)d; (void)x; (void)y; (void)w; return 0; }
static GtkWidget *ccheck(CRow *r, const char *t, int on, void (*cb)(int, void *), void *d, const char *name) { GtkWidget *c = rui_check(t, on, cb, d); p_put(c); if (name) p_label_named(name, c); r->t[r->n] = g_strdup(t); r->c[r->n++] = c; return c; }
/* (a line of its own after a block, mt px under it; each check box found by Search in Settings) */
static void crow_done(CRow *r, double mt) { p_custom(crow_lay, r, mt, 0); p_custom_find(r->t[0], 0); for (int k = 1; k < r->n; k++) { p_custom(none_lay, NULL, 0, 0); p_custom_find(r->t[k], 0); } }
static double bold_lay(void *d, double x, double y, double w) { p_place(d, x, y, w, 15); return 15; }
static gboolean bold_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)d; s_col(cr, "fg", 1); s_text(cr, g_object_get_data(G_OBJECT(w), "t"), 0, 0, 13, 700, 0, 15); return TRUE; }
static void p_bold(const char *t) { GtkWidget *a = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(a), "t", g_strdup(rui_tr(t)), g_free); g_signal_connect(a, "draw", G_CALLBACK(bold_draw), NULL); p_put(a); p_custom(bold_lay, a, 0, 0); }

/* ================================================================== Default apps (appsPanel's table), Links (links60.js lkPanel) */
static char *lower(const char *s) { return g_ascii_strdown(s, -1); }
static RuiJ *DEBAPPS;   /* (Debian's apps: debian/apps, else the page's copy bd95debapps; with bd95lx's, the page's lxLaunchers) */
typedef struct { char desktop[200], name[200]; const char *mime; int deb; } Lx;
static GArray *LXL;
static void lx_load(void)
{
	if (LXL) g_array_free(LXL, TRUE); LXL = g_array_new(FALSE, TRUE, sizeof(Lx)); rui_jfree(DEBAPPS); DEBAPPS = NULL;
	/* (lxLaunchers: the Flatpak's and Linux apps rudolfd lists (bd95lx), each launcher, then Debian's) */
	RuiJ *o = set_ls_get("bd95lx"); RuiJ *l = o ? rui_jprop(o, "x") : NULL;
	for (int i = 0; l && i < rui_jlen(l); i++) { RuiJ *a = rui_jat(l, i), *ls = rui_jprop(a, "launchers");
		if (ls && rui_jlen(ls)) for (int k = 0; k < rui_jlen(ls); k++) { RuiJ *x = rui_jat(ls, k); Lx e = { 0 }; snprintf(e.desktop, sizeof e.desktop, "%s", set_jstr(x, "desktop", "")); snprintf(e.name, sizeof e.name, "%s", set_jstr(x, "name", "")); e.mime = NULL; g_array_append_val(LXL, e);
			g_array_index(LXL, Lx, LXL->len - 1).mime = g_intern_string(set_jstr(x, "mime", "")); }
		else { Lx e = { 0 }; snprintf(e.desktop, sizeof e.desktop, "%s", set_jstr(a, "id", "")); snprintf(e.name, sizeof e.name, "%s", set_jstr(a, "name", "")); e.mime = g_intern_string(""); g_array_append_val(LXL, e); } }
	rui_jfree(o);
	char er[200]; RuiJ *j = api("debian/apps", NULL, er, sizeof er); RuiJ *da = j ? rui_jprop(j, "apps") : NULL;
	if (!da || !rui_jlen(da)) { rui_jfree(j); j = set_ls_get("bd95debapps"); da = j ? rui_jprop(j, "x") : NULL; }
	DEBAPPS = j;
	for (int i = 0; da && i < rui_jlen(da); i++) { RuiJ *a = rui_jat(da, i); Lx e = { 0 }; snprintf(e.desktop, sizeof e.desktop, "deb-%s", set_jstr(a, "id", "")); snprintf(e.name, sizeof e.name, "%s", set_jstr(a, "name", ""));
		e.mime = g_intern_string(set_jstr(a, "mime", "")); e.deb = 1; g_array_append_val(LXL, e); }
}
/* (linux26.js lxHandles: the kind's MIME type, or one of its aliases, among the app's; a whole "type/" too) */
static int lx_handles(const Lx *x, const char *ext)
{
	char *e = lower(ext); const char *m = NULL; for (guint i = 0; i < G_N_ELEMENTS(LXMIME); i++) if (!strcmp(LXMIME[i][0], e)) m = LXMIME[i][1]; g_free(e);
	if (!m || !x->mime || !*x->mime) return 0; int ok = 0; char **ts = g_strsplit(x->mime, ";", -1);
	for (int i = 0; ts[i] && !ok; i++) { const char *t = ts[i]; if (!*t) continue; size_t tl = strlen(t); int star = tl > 2 && !strcmp(t + tl - 2, "/*");
		const char *ms[8]; int nm = 0; ms[nm++] = m; for (guint k = 0; k < G_N_ELEMENTS(LXALIAS) && nm < 8; k++) if (!strcmp(LXALIAS[k][0], m)) ms[nm++] = LXALIAS[k][1];
		for (int k = 0; k < nm && !ok; k++) ok = !strcmp(t, ms[k]) || (star && !strncmp(ms[k], t, tl - 1)); }
	g_strfreev(ts); return ok;
}
static void on_def(const char *v, void *d)
{
	char k[120]; snprintf(k, sizeof k, "bd95defapps:%s", ACCT);
	if (v && *v) { char *e = set_jesc(v), *j = g_strdup_printf("\"%s\"", e); ls_prop(k, d, j); g_free(j); g_free(e); } else ls_prop(k, d, NULL);
}
static void defapps(void)
{
	p_h4("Default apps"); p_msg_m("Choose which app opens each kind of file. You can also pick “Always use this app” in “Open with…”.", 0, 10, 13);
	char k[120]; snprintf(k, sizeof k, "bd95defapps:%s", ACCT); RuiJ *o = set_ls_get(k); RuiJ *d = o ? rui_jprop(o, "x") : NULL; ATab *t = atab();
	for (guint i = 0; i < G_N_ELEMENTS(DEFAPPS); i++) { const SetDefApp *a = &DEFAPPS[i]; const char *v[48], *l[48]; char *own[48]; int n = 0, no = 0;
		v[n] = ""; l[n++] = "Rudolf OS default";
		for (int q = 0; q < a->n; q++) { v[n] = a->o[q][0]; l[n++] = a->o[q][1]; }
		for (guint q = 0; q < LXL->len && n < 48; q++) { Lx *x = &g_array_index(LXL, Lx, q); if (!lx_handles(x, a->e)) continue;
			own[no++] = g_strdup_printf("lx:%s", x->desktop); v[n] = own[no - 1]; own[no++] = g_strdup_printf("%s (Linux app)", x->name); l[n++] = own[no - 1]; }
		const char *cur = set_jstr(d, a->e, ""); GtkWidget *s = s_select(v, l, n, cur, on_def, (void *)a->e); s_select_width(s, 0);
		char *e = lower(a->e), *lab = g_strdup_printf(".%s", e), nm[40]; snprintf(nm, sizeof nm, "da_%s", e); arow(t, lab, NULL, s); p_label_named(nm, s); g_free(e); g_free(lab);
		for (int q = 0; q < no; q++) g_free(own[q]); }
	p_custom(atab_lay, t, 0, 0); rui_jfree(o);
}
/* Links: a box of the page's (the lock screen's .lkp style, which the page's box shares: #020617, its border #475569, 8px round, 8px
   in, white writing), its heading, its words and a table of the kinds */
typedef struct { GtkWidget *a, *h; ATab *t; char *msg; } LkBox;
static double lk_lay(void *d, double x, double y, double w)
{
	LkBox *b = d; double iw = w - 18, cy = y + 1 + 8; p_place(b->h, x + 9, cy, iw, 0); cy += 16 + 15 + 10;
	int n = s_lines(b->msg, 13, 400, iw, NULL); g_object_set_data(G_OBJECT(b->a), "my", GINT_TO_POINTER((int)lround((cy - y) * 100))); cy += n * 15 + 13;
	if (b->t) cy += atab_lay(b->t, x + 9, cy, iw);
	double h = cy + 8 + 1 - y; p_place(b->a, x, y, w, h); return h;
}
static gboolean lk_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	LkBox *b = d; double W = gtk_widget_get_allocated_width(w) / s_z(), H = gtk_widget_get_allocated_height(w) / s_z();
	s_rrect(cr, .5, .5, W - 1, H - 1, 7.5); cairo_set_source_rgb(cr, 0x02 / 255.0, 0x06 / 255.0, 0x17 / 255.0); cairo_fill_preserve(cr); cairo_set_source_rgb(cr, 0x47 / 255.0, 0x55 / 255.0, 0x69 / 255.0); cairo_set_line_width(cr, 1); cairo_stroke(cr);
	cairo_set_source_rgb(cr, 1, 1, 1); s_text(cr, b->msg, 9, GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), "my")) / 100.0, 13, 400, W - 18, 15); return FALSE;
}
static gint pstrcmp(gconstpointer a, gconstpointer b) { return strcmp(*(char *const *)a, *(char *const *)b); }
static void on_link(const char *v, void *d) { char k[120]; snprintf(k, sizeof k, "bd95links:%s", ACCT); char *e = set_jesc(v), *j = g_strdup_printf("\"%s\"", e); ls_prop(k, d, j); g_free(j); g_free(e); }
static void links(void)
{
	/* (lkSchemes: the apps sorted by name, each x-scheme-handler/ kind lkOk allows, its apps in that order) */
	GArray *ix = g_array_new(FALSE, FALSE, sizeof(guint)); for (guint i = 0; i < LXL->len; i++) g_array_append_val(ix, i);
	for (guint i = 1; i < ix->len; i++) for (guint j = i; j > 0; j--) { Lx *a = &g_array_index(LXL, Lx, g_array_index(ix, guint, j - 1)), *b = &g_array_index(LXL, Lx, g_array_index(ix, guint, j));
		if (g_utf8_collate(a->name, b->name) <= 0) break; guint t = g_array_index(ix, guint, j); g_array_index(ix, guint, j) = g_array_index(ix, guint, j - 1); g_array_index(ix, guint, j - 1) = t; }
	GPtrArray *sch = g_ptr_array_new_with_free_func(g_free); GHashTable *by = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, (GDestroyNotify)g_ptr_array_unref);
	for (guint i = 0; i < ix->len; i++) { Lx *x = &g_array_index(LXL, Lx, g_array_index(ix, guint, i)); char **ts = g_strsplit(x->mime ? x->mime : "", ";", -1);
		for (int k = 0; ts[k]; k++) { char *t = g_ascii_strdown(g_strstrip(ts[k]), -1); if (strncmp(t, "x-scheme-handler/", 17)) { g_free(t); continue; } const char *s = t + 17; int ok = g_regex_match_simple("^[a-z][a-z0-9+.-]{1,39}$", s, 0, 0);
			for (guint q = 0; q < G_N_ELEMENTS(LKNO) && ok; q++) if (!strcmp(LKNO[q], s)) ok = 0;
			if (ok) { GPtrArray *l = g_hash_table_lookup(by, s); if (!l) { char *ks = g_strdup(s); g_ptr_array_add(sch, ks); l = g_ptr_array_new(); g_hash_table_insert(by, ks, l); }
				int seen = 0; for (guint q = 0; q < l->len; q++) if (!strcmp(((Lx *)g_ptr_array_index(l, q))->desktop, x->desktop)) seen = 1; if (!seen) g_ptr_array_add(l, x); }
			g_free(t); }
		g_strfreev(ts); }
	LkBox *b = g_new0(LkBox, 1); b->a = gtk_drawing_area_new(); g_signal_connect(b->a, "draw", G_CALLBACK(lk_draw), b); p_put(b->a);
	b->h = rui_heading("Links"); gtk_widget_set_margin_bottom(b->h, 0); p_put(b->h);
	b->msg = g_strdup(sch->len ? "Links of an app’s own kind (after signing in on a website, for example) open in that app, after you say yes. Choose which app opens each kind." : "No Linux app on this computer opens links of its own kind.");
	if (sch->len) { g_ptr_array_sort(sch, pstrcmp); char k[120]; snprintf(k, sizeof k, "bd95links:%s", ACCT); RuiJ *o = set_ls_get(k); RuiJ *ch = o ? rui_jprop(o, "x") : NULL;
		b->t = atab(); b->t->white = 1;
		for (guint i = 0; i < sch->len; i++) { const char *s = g_ptr_array_index(sch, i); GPtrArray *l = g_hash_table_lookup(by, s); const char *v[24], *lb[24]; char *own[24]; int n = 0, no = 0;
			const char *c = set_jstr(ch, s, ""); const char *cur = !strcmp(c, "none") ? "none" : NULL;
			for (guint q = 0; q < l->len && n < 22; q++) { Lx *x = g_ptr_array_index(l, q); v[n] = x->desktop; own[no++] = g_strdup_printf("%s%s", x->name, x->deb ? " (Debian)" : ""); lb[n++] = own[no - 1]; if (!cur && !strcmp(c, x->desktop)) cur = x->desktop; }
			if (!cur) cur = ((Lx *)g_ptr_array_index(l, 0))->desktop; v[n] = "none"; lb[n++] = "Don’t open them";
			GtkWidget *sl = s_select(v, lb, n, cur, on_link, g_strdup(s)); s_select_width(sl, 0); char *lab = g_strdup_printf("%s: links", s), nm[80]; snprintf(nm, sizeof nm, "lk_%s", s);
			arow(b->t, lab, NULL, sl); p_label_named(nm, sl); g_free(lab); for (int q = 0; q < no; q++) g_free(own[q]); }
		rui_jfree(o); }
	p_custom(lk_lay, b, 0, 0); p_custom_find("Links", 1);
	g_hash_table_unref(by); g_ptr_array_unref(sch); g_array_free(ix, TRUE);
}

/* ================================================================== Installed apps (the App Store's: bd95rapps), rappRemove */
typedef struct { GtkWidget *a, *b[8]; char *t[8]; int n; } IaBox;
static double ia_lay(void *d, double x, double y, double w)
{
	IaBox *b = d; if (!b->n) { p_place(b->a, x, y, w, 15); return 15; }
	double cw = w - 110 - 90 - 16; for (int i = 0; i < b->n; i++) p_place(b->b[i], x + cw + 8, y + i * 32 + 4, 110, 0); p_place(b->a, x, y, w, b->n * 32); return b->n * 32;
}
static gboolean ia_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	IaBox *b = d; double W = gtk_widget_get_allocated_width(w) / s_z(); s_col(cr, "fg", 1);
	if (!b->n) { s_text(cr, "No apps from the App Store yet.", 0, 0, 13, 400, 0, 15); return FALSE; }
	for (int i = 0; i < b->n; i++) { s_col(cr, "acr", 1); cairo_rectangle(cr, 0, i * 32 + 31, W, 1); cairo_fill(cr); s_col(cr, "fg", 1); s_text(cr, b->t[i], 0, i * 32 + 4 + 4, 13, 400, W - 216, 15); }
	return FALSE;
}
static char *RM_ID;
static void rm_done(int ok, void *d)
{
	(void)d; if (!ok || !RM_ID) return; RuiJ *o = set_ls_get("bd95rapps"); RuiJ *m = o ? rui_jprop(o, "x") : NULL; RuiJ *it = m ? rui_jprop(m, RM_ID) : NULL; char *name = g_strdup(set_jstr(it, "name", RM_ID)); rui_jfree(o);
	ls_prop("bd95rapps", RM_ID, NULL); set_notify("Removed", name, 0); g_free(name); redraw();
}
static void on_uninst(GtkButton *b, gpointer d)
{
	(void)b; g_free(RM_ID); RM_ID = g_strdup(d); RuiJ *o = set_ls_get("bd95rapps"); RuiJ *m = o ? rui_jprop(o, "x") : NULL; char *r = g_strdup_printf("Uninstall %s", set_jstr(m ? rui_jprop(m, d) : NULL, "name", d)); rui_jfree(o);
	set_note("elevate %s", r); rui_elevate(SW, r, rm_done, NULL); g_free(r);
}
static void on_store(GtkButton *b, gpointer d) { (void)b; (void)d; ask("open store"); }
static void installed(void)
{
	p_h4("Installed apps"); IaBox *b = g_new0(IaBox, 1); b->a = gtk_drawing_area_new(); g_signal_connect(b->a, "draw", G_CALLBACK(ia_draw), b); p_put(b->a);
	RuiJ *o = set_ls_get("bd95rapps"); RuiJ *m = o ? rui_jprop(o, "x") : NULL;
	for (int i = 0; m && i < rui_jlen(m) && b->n < 8; i++) { const char *id = rui_jkey(m, i); RuiJ *a = rui_jprop(m, id), *pm = rui_jprop(a, "permissions"); GString *p = g_string_new(NULL);
		for (int k = 0; pm && k < rui_jlen(pm); k++) { if (k) g_string_append(p, ", "); g_string_append(p, rui_jsval(rui_jat(pm, k)) ? rui_jsval(rui_jat(pm, k)) : ""); }
		b->t[b->n] = g_strdup_printf("%s %s %s — %s", set_jstr(a, "icon", ""), set_jstr(a, "name", ""), set_jstr(a, "version", ""), p->len ? p->str : "no permissions"); g_string_free(p, TRUE);
		GtkWidget *x = rui_button("Uninstall", 0); g_object_set_data_full(G_OBJECT(x), "id", g_strdup(id), g_free); g_signal_connect(x, "clicked", G_CALLBACK(on_uninst), g_object_get_data(G_OBJECT(x), "id")); p_put(x);
		char nm[24]; snprintf(nm, sizeof nm, "iax%d", b->n); p_label_named(nm, x); b->b[b->n++] = x; }
	rui_jfree(o); p_custom(ia_lay, b, 0, 0);
	BRow *r = brow(); bbtn(r, "Open the App Store", 0, G_CALLBACK(on_store), NULL, "ast"); brow_done(r, 0);
}

/* ================================================================== Linux (Debian 13): term26.js's, with wineold62.js's row */
static char *DMSG, *BMSG, *BKM, *BDM;   /* (the sections' .msg lines, kept while the page is drawn again) */
static RuiJ *DCK;                       /* (the last Check's answer, its lines shown under the buttons) */
static void say(char **m, const char *t) { g_free(*m); *m = g_strdup(t ? t : ""); set_note("msg %s", *m); redraw(); }
/* (lxJobWait: rudolfd's job followed every 0.9 s until it ends; each new line of its log given) */
typedef void (*JobDone)(RuiJ *j);
typedef struct { JobDone done; void (*line)(const char *l); int seen; } JobW;
static gboolean job_tick(gpointer d)
{
	JobW *w = d; RuiJ *j = rui_get("/api/job"); if (!j) return G_SOURCE_CONTINUE;
	if (w->line) { char **ls = g_strsplit(set_jstr(j, "log", ""), "\n", -1); int n = g_strv_length(ls); for (; w->seen < n - 1; w->seen++) w->line(ls[w->seen]); g_strfreev(ls); }
	if (!jb(j, "running") && jnum(j, "status") >= 0 && rui_jprop(j, "status")) { w->done(j); rui_jfree(j); g_free(w); return G_SOURCE_REMOVE; }
	rui_jfree(j); return G_SOURCE_CONTINUE;
}
static void job_wait(JobDone done, void (*line)(const char *l)) { JobW *w = g_new0(JobW, 1); w->done = done; w->line = line; g_timeout_add(900, job_tick, w); }
static char *last_line(RuiJ *j) { char *l = g_strstrip(g_strdup(set_jstr(j, "log", ""))); char *p = strrchr(l, '\n'); char *r = g_strdup(p ? p + 1 : l); g_free(l); return r; }
/* (jobCrashText) */
static char *crash_text(RuiJ *j)
{
	RuiJ *cs = rui_jprop(j, "crashes"), *c = cs && rui_jlen(cs) ? rui_jat(cs, 0) : NULL; const char *w = set_jstr(c, "where", "");
	return g_strdup_printf("%s stopped unexpectedly%s%s%s. That is a bug in Rudolf OS, not a problem with this computer or its connection. Task Manager › System › Report a problem… sends the details.",
		*set_jstr(c, "program", "") ? set_jstr(c, "program", "") : "A part of Rudolf OS", *w ? " (in " : "", w, *w ? ")" : "");
}
static void on_setup(GtkButton *b, gpointer d) { (void)b; (void)d; ask("open terminal"); }
static void on_check(GtkButton *b, gpointer d)
{
	(void)b; (void)d; char er[300]; RuiJ *j = api("debian/check", NULL, er, sizeof er); rui_jfree(DCK);
	if (!j) { char *e = g_strdup_printf("{\"error\":\"%s\"}", er); char *x = set_jesc(er); g_free(e); e = g_strdup_printf("{\"error\":\"%s\"}", x); g_free(x); DCK = rui_jparse(e); g_free(e); } else DCK = j;
	redraw();
}
static const char *const LXCHK[][2] = {   /* (term26.js LXCHK) */
	{ "persist", "Data partition" }, { "persist_fs", "File system" }, { "persist_kind", "Kind" }, { "persist_features", "Features" }, { "persist_inode_size", "Inode size" }, { "persist_opts", "Mounted with" },
	{ "tmpfile_persist", "Temporary files on the data partition" }, { "tmpfile_flatpak", "…in Flatpak's folder" }, { "tmpfile_flatpak_repo", "…in Flatpak's repository" }, { "tmpfile_var_tmp", "…in /var/tmp" },
	{ "tmpfile_tmp", "…in /tmp" }, { "tmpfile_debian", "…in Debian" }, { "flatpak_repo", "Flatpak repository" }, { "debian_owner", "Debian folder owner" }, { "userns", "User namespaces" },
	{ "proc_mount", "A new /proc for sandboxes" }, { "max_user_namespaces", "User namespaces allowed" }, { "debian", "Debian sandbox" } };
static char *chk_val(RuiJ *r, const char *k) { RuiJ *v = rui_jprop(r, k); if (!v || rui_jtype(v) <= 0) return NULL; if (rui_jtype(v) == 3) return *rui_jsval(v) ? g_strdup(rui_jsval(v)) : NULL;
	GString *b = g_string_new(NULL); set_jw(b, v); return g_string_free(b, FALSE); }
static int lx_bad(const char *v) { return v && g_regex_match_simple("^(failed|missing|not mounted|tmp folder missing|refused)|did not start|did not finish|not available|expected 200000", v, 0, 0); }
static void check_show(void)
{
	if (!DCK) return; const char *e = set_jstr(DCK, "error", NULL);
	if (e) { char *t = g_strdup_printf("The check could not run: %s", e); p_msg(t, 2); g_free(t); return; }
	GString *bad = g_string_new(NULL); int nb = 0;
	for (guint i = 0; i < G_N_ELEMENTS(LXCHK); i++) { char *v = chk_val(DCK, LXCHK[i][0]); if (lx_bad(v)) { const char *l = LXCHK[i][1]; char *ll = !strncmp(l, "…", strlen("…")) ? g_strdup_printf("temporary files %s", l + strlen("…")) : g_strdup(l); g_string_append_printf(bad, "%s%s", nb++ ? ", " : "", ll); g_free(ll); } g_free(v); }
	char *t = nb ? g_strdup_printf("%d problem(s): %s. Copy the system report (Task Manager › System) to report it.", nb, bad->str) : g_strdup("Everything Linux apps and the terminal need is working.");
	p_msg_m(t, 2, 10, 13); g_free(t); g_string_free(bad, TRUE);
	ATab *tb = atab(); for (guint i = 0; i < G_N_ELEMENTS(LXCHK); i++) { char *v = chk_val(DCK, LXCHK[i][0]); if (!v) continue; char *s = g_strdup_printf("%s%s", lx_bad(v) ? "⚠️ " : "", v); arow(tb, LXCHK[i][1], s, NULL); g_free(s); g_free(v); }
	p_custom(atab_lay, tb, 0, 0);
}
static void on_auto(int on, void *d) { (void)d; svj_bool("bd95debauto", on); }
static void upd_done(RuiJ *j)
{
	char *t = jnum(j, "status") == 0 ? g_strdup("Debian is up to date.") : jb(j, "crashed") ? crash_text(j) : NULL;
	if (!t) { char *l = last_line(j); t = g_strdup_printf("The update did not finish: %s", l); g_free(l); } say(&DMSG, t); g_free(t);
}
static void upd_go(int ok, void *d) { (void)d; if (!ok) return; char er[300]; RuiJ *j = api("debian/update", NULL, er, sizeof er); if (!j) { say(&DMSG, er); return; } rui_jfree(j); say(&DMSG, "Updating Debian…"); job_wait(upd_done, NULL); }
static void on_update(GtkButton *b, gpointer d) { (void)b; (void)d; if (set_admin()) upd_go(1, NULL); else { set_note("elevate Update Debian"); rui_elevate(SW, "Update Debian", upd_go, NULL); } }
static void on_passwd(GtkButton *b, gpointer d) { (void)b; (void)d; ask("open do-debpasswd"); }
static void rm_deb_done(RuiJ *j) { int ok = jnum(j, "status") == 0; set_ls_set("bd95debapps", "[]"); set_note("svj bd95debapps []"); g_free(DMSG); DMSG = g_strdup(ok ? "Debian was removed." : "Removing failed."); set_note("msg %s", DMSG); g_timeout_add(800, redraw_later, NULL); }
static void rm_deb_go(int ok, void *d) { (void)d; if (!ok) return; char er[300]; RuiJ *j = api("debian/remove", NULL, er, sizeof er); if (!j) { say(&DMSG, er); return; } rui_jfree(j); say(&DMSG, "Removing…"); job_wait(rm_deb_done, NULL); }
static void on_rmdeb(GtkButton *b, gpointer d) { (void)b; (void)d; set_note("elevate Remove Debian"); rui_elevate(SW, "Remove Debian and every program installed in it (your Documents, Pictures and Downloads stay)", rm_deb_go, NULL); }
static int WINE_N;
static void wine_line(const char *l) { if (g_regex_match_simple("^(Get|Unpacking|Setting up):?", l, 0, 0)) { char t[80]; snprintf(t, sizeof t, "Installing Wine… (%d steps)", ++WINE_N); say(&DMSG, t); } }
static void wine_done(RuiJ *j)
{
	if (jnum(j, "status") == 0) { say(&DMSG, "Wine is installed. Windows programs (.exe, .msi) now open with it."); set_notify("Wine is installed", "Windows programs (.exe, .msi) now open with it. Installed programs appear in the Start menu.", 0); return; }
	char *t; if (jb(j, "crashed")) t = crash_text(j); else { char *l = last_line(j); t = g_strdup_printf("Wine could not be installed: %s", l); g_free(l); } say(&DMSG, t); g_free(t);
}
static void wine_go(int ok, void *d) { (void)d; if (!ok) return; char er[300]; RuiJ *j = api("debian/wine", NULL, er, sizeof er); if (!j) { say(&DMSG, er); return; } rui_jfree(j); WINE_N = 0; say(&DMSG, "Installing Wine: about 1 GB to download, a few minutes…"); job_wait(wine_done, wine_line); }
static void on_wine(GtkButton *b, gpointer d) { (void)b; (void)d; if (set_admin()) wine_go(1, NULL); else { set_note("elevate Install Wine"); rui_elevate(SW, "Install Wine to run Windows programs (a download of about 1 GB)", wine_go, NULL); } }
static void on_wineold(GtkButton *b, gpointer d) { (void)b; (void)d; ask("open do-wineold"); }
static char *wo_progs(RuiJ *wo) { GString *b = g_string_new(NULL); RuiJ *p = rui_jprop(wo, "programs"); for (int i = 0; p && i < rui_jlen(p); i++) g_string_append_printf(b, "%s%s", i ? ", " : "", rui_jsval(rui_jat(p, i)) ? rui_jsval(rui_jat(p, i)) : ""); return g_string_free(b, FALSE); }   /* (woWhat, not several) */
static void linux_sec(void)
{
	p_h4("Linux (Debian 13)"); char er[300]; RuiJ *st = api("debian/status?size=1", NULL, er, sizeof er);
	if (!st) { p_msg("This Rudolf OS cannot run Debian.", 2); return; }
	if (!jb(st, "installed")) { p_msg("Debian is not set up. It adds a Linux terminal with APT.", 2); BRow *r = brow(); bbtn(r, "Set up Debian…", 0, G_CALLBACK(on_setup), NULL, "dsu"); bbtn(r, "Check", 0, G_CALLBACK(on_check), NULL, "dck"); brow_done(r, 14); check_show(); rui_jfree(st); return; }
	ATab *t = atab(); char *v = g_strdup_printf("Debian %s", set_jstr(st, "version", "")), *sz = fmt_b(jnum(st, "size")), *fr = fmt_b(jnum(st, "free")), na[24];
	int n = 0; RuiJ *da = DEBAPPS ? (rui_jprop(DEBAPPS, "apps") ? rui_jprop(DEBAPPS, "apps") : rui_jprop(DEBAPPS, "x")) : NULL; for (int i = 0; da && i < rui_jlen(da); i++) { RuiJ *m = rui_jprop(rui_jat(da, i), "menu"); if (!(m && rui_jtype(m) == 1 && !rui_jval(m))) n++; }
	snprintf(na, sizeof na, "%d", n); arow(t, "Version", v, NULL); arow(t, "Space used", sz, NULL); arow(t, "Free on the data partition", fr, NULL); arow(t, "Apps with a window", na, NULL); g_free(v); g_free(sz); g_free(fr);
	if (jb(st, "wine")) { ARow *r = arow(t, "Windows programs", "Wine is installed: .exe and .msi files open with it, and the programs you install appear in the Start menu.", NULL);
		RuiJ *wo = rui_get("/api/wine/old");
		if (wo && jb(wo, "on") && (jb(wo, "offer") || jb(wo, "running"))) {
			char *what; double c = jnum(wo, "count"); if (c) what = jb(wo, "several") ? g_strdup_printf("%.0f program%s", c, c == 1 ? "" : "s") : wo_progs(wo); else what = g_strdup("their Start menu entries");
			r->x = jb(wo, "running") ? g_strdup("The Windows programs from before Rudolf OS 1.5.9 are being brought into your account.") : g_strdup_printf("The Windows programs from before Rudolf OS 1.5.9 (%s) are not in your account yet.", what); g_free(what);
			r->xb = rui_button(jb(wo, "running") ? "Show progress…" : "Bring them in…", 0); g_signal_connect(r->xb, "clicked", G_CALLBACK(on_wineold), NULL); p_put(r->xb); p_label_named("wo62", r->xb); }
		rui_jfree(wo); }
	else { ARow *r = arow(t, "Windows programs", "", NULL); r->x = g_strdup("Wine is not installed."); r->xb = rui_button("Install Wine…", 0); g_signal_connect(r->xb, "clicked", G_CALLBACK(on_wine), NULL); p_put(r->xb); p_label_named("dwi", r->xb); }
	p_custom(atab_lay, t, 0, 0);
	CRow *c = g_new0(CRow, 1); ccheck(c, "Install Debian's updates automatically once a day", ld_bool("bd95debauto", 1), on_auto, NULL, "dau"); crow_done(c, 0);
	BRow *r = brow(); bbtn(r, "Open Terminal", 0, G_CALLBACK(on_setup), NULL, "dop"); bbtn(r, "Update now", 0, G_CALLBACK(on_update), NULL, "dup"); bbtn(r, "Change the Linux password…", 0, G_CALLBACK(on_passwd), NULL, "dpw");
	bbtn(r, "Remove Debian…", 0, G_CALLBACK(on_rmdeb), NULL, "drm"); bbtn(r, "Check", 0, G_CALLBACK(on_check), NULL, "dck"); brow_done(r, 14);
	p_label_named("dmsg", p_msg(DMSG ? DMSG : "", 2)); check_show(); rui_jfree(st);
}

/* ================================================================== Blitzen (assistant): blitzen26.js's */
static void on_keep(int on, void *d) { (void)d; svj_bool("bd95blzkeep", on); }
static GtkWidget *BOF;
static void bof_go(int ok, void *d) { int on = GPOINTER_TO_INT(d); if (!ok) { rui_check_set(BOF, !on); return; } svj_bool("bd95blzoff", on); }
static void on_off(int on, void *d)
{
	(void)d; if (set_admin()) { bof_go(1, GINT_TO_POINTER(on)); return; } set_note("elevate Turn Blitzen on or off"); rui_elevate(SW, "Turn Blitzen on or off", bof_go, GINT_TO_POINTER(on));
}
static void on_blz(GtkButton *b, gpointer d) { (void)b; (void)d; ask("open blitzen"); }
static void on_unload(GtkButton *b, gpointer d) { (void)b; (void)d; char er[300]; RuiJ *j = api("ai/unload", NULL, er, sizeof er); if (!j) { say(&BMSG, er); return; } rui_jfree(j); redraw(); }
static gboolean blz_later(gpointer d) { (void)d; redraw(); return G_SOURCE_REMOVE; }
static void brm_go(int ok, void *d) { (void)d; if (!ok) return; char er[300]; RuiJ *j = api("ai/remove", NULL, er, sizeof er); if (!j) { say(&BMSG, er); return; } rui_jfree(j); g_free(BMSG); BMSG = g_strdup("Removed."); set_note("msg Removed."); redraw(); g_timeout_add(600, blz_later, NULL); }
static void on_brm(GtkButton *b, gpointer d) { (void)b; (void)d; set_note("elevate Remove Blitzen's model"); rui_elevate(SW, "Remove Blitzen's model (your saved chats stay)", brm_go, NULL); }
static void blitzen_sec(void)
{
	p_h4("Blitzen (assistant)"); char er[300]; RuiJ *st = api("ai/status", NULL, er, sizeof er);
	if (!st) { p_msg("This Rudolf OS has no Blitzen.", 2); return; }
	int inst = jb(st, "installed"), off = ld_bool("bd95blzoff", 0), keep = ld_bool("bd95blzkeep", 1); RuiJ *sp = set_ls_get("bd95blzspeed"); RuiJ *spx = sp ? rui_jprop(sp, "x") : NULL;
	ATab *t = atab(); arow(t, "Model", inst ? set_jstr(st, "model", "Blitzen") : "not downloaded", NULL);
	if (inst) { char *s = fmt_b(jnum(st, "size")); arow(t, "Size", s, NULL); g_free(s); arow(t, "Licence", set_jstr(st, "licence", ""), NULL); }
	arow(t, "Running now", jb(st, "loaded") ? "yes" : "no (starts when you ask)", NULL);
	{ char *a = fmt_b(jnum(st, "mem")), *b = fmt_b(jnum(st, "need")), *m = g_strdup_printf("%s (needs %s)", a, b); arow(t, "Free memory", m, NULL); g_free(a); g_free(b); g_free(m); }
	{ double tps = spx ? jnum(spx, "tps") : 0; char *m = tps ? g_strdup_printf("%.1f tokens per second", tps) : g_strdup("–"); arow(t, "Last answer", m, NULL); g_free(m); }
	arow(t, "Python in Debian", jb(st, "python") ? "installed" : jb(st, "debian") ? "not installed" : "Debian not set up", NULL);
	arow(t, "C++ compiler in Debian", jb(st, "cpp") ? "installed" : jb(st, "debian") ? "not installed" : "Debian not set up", NULL);
	p_custom(atab_lay, t, 0, 0); rui_jfree(sp);
	CRow *c = g_new0(CRow, 1); ccheck(c, "Keep chats in Documents › Blitzen", keep, on_keep, NULL, "bkp"); BOF = ccheck(c, "Turn Blitzen off for all accounts", off, on_off, NULL, "bof"); crow_done(c, 0);
	BRow *r = brow(); bbtn(r, inst ? "Open Blitzen" : "Download…", 0, G_CALLBACK(on_blz), NULL, "bop"); if (jb(st, "loaded")) bbtn(r, "Unload now", 0, G_CALLBACK(on_unload), NULL, "bun");
	if (inst) bbtn(r, "Remove the model…", 0, G_CALLBACK(on_brm), NULL, "brm"); brow_done(r, 14);
	p_label_named("bmsg", p_msg(BMSG ? BMSG : "", 2)); rui_jfree(st);
}

/* ================================================================== For developers: tools56.js's (a <label>, not label.chk; .msg 13 px) */
static void on_dvt(int on, void *d) { (void)d; svj_bool("bd95devtools", on); }
static void devtools_sec(void)
{
	p_h4("For developers"); CRow *c = g_new0(CRow, 1); ccheck(c, "Show developer tools in the Start menu (Developer Console)", ld_bool("bd95devtools", 0), on_dvt, NULL, "dvt"); crow_done(c, 0);
	p_msg_m("The Developer Console shows the desktop's log, its windows and storage, and the app API. It is always in Task Manager and in search.", 0, 10, 13);
}

/* ================================================================== Blitzen: what it knows (blitzknow60.js's) */
static RuiJ *NOTES; static int NOTES_OK;
static int notes_set(GPtrArray *a)
{
	GString *b = g_string_new(NULL); for (guint i = 0; i < a->len && i < 50; i++) { char *x = g_strdup(g_ptr_array_index(a, i)); for (char *p = x; *p; p++) if (*p == '\r' || *p == '\n') *p = ' ';
		if (g_utf8_strlen(x, -1) > 300) *g_utf8_offset_to_pointer(x, 300) = 0; g_string_append_printf(b, "%s\n", x); g_free(x); }
	char er[300]; RuiJ *j = api("ai/know/notes?set=1", b->str, er, sizeof er); g_string_free(b, TRUE); if (!j) { say(&BKM, er); return 0; } rui_jfree(j); return 1;
}
static GPtrArray *notes_now(void) { GPtrArray *a = g_ptr_array_new_with_free_func(g_free); RuiJ *it = NOTES ? rui_jprop(NOTES, "items") : NULL; for (int i = 0; it && i < rui_jlen(it); i++) if (rui_jsval(rui_jat(it, i))) g_ptr_array_add(a, g_strdup(rui_jsval(rui_jat(it, i)))); return a; }
static void on_forget_note(GtkButton *b, gpointer d) { (void)b; GPtrArray *a = notes_now(); guint i = GPOINTER_TO_UINT(d); if (i < a->len) g_ptr_array_remove_index(a, i); if (notes_set(a)) redraw(); g_ptr_array_unref(a); }
static GtkWidget *BKNEW;
static void on_add_note(GtkButton *b, gpointer d)
{
	(void)b; (void)d; char *v = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(BKNEW)))); if (!*v) { g_free(v); return; }
	if (!NOTES_OK) { say(&BKM, "Notes are kept only in your account’s own folder on this computer’s disk, which is not open here."); g_free(v); return; }
	GPtrArray *a = notes_now(); int have = 0; for (guint i = 0; i < a->len; i++) if (!strcmp(g_ptr_array_index(a, i), v)) have = 1; if (!have) g_ptr_array_add(a, v); else g_free(v);
	if (notes_set(a)) redraw(); g_ptr_array_unref(a);
}
static void on_see(GtkButton *b, gpointer d) { (void)b; (void)d; ask("open do-blzknows"); }
static GtkWidget *BKL;
static void forget_ans(int i, void *d)
{
	(void)d; if (i == 0) { char er[300]; RuiJ *j = api("ai/know/forget?all=1", NULL, er, sizeof er); rui_jfree(j); }
	say(&BKM, "Blitzen no longer learns from what you do.");
}
static void on_learn(int on, void *d)
{
	(void)d; char p[40], er[300]; snprintf(p, sizeof p, "ai/know/learn?on=%d", on ? 1 : 0); RuiJ *j = api(p, NULL, er, sizeof er);
	if (!j) { rui_check_set(BKL, !on); say(&BKM, er); return; } rui_jfree(j);
	if (on) { say(&BKM, "Blitzen will learn from what you do from now on."); return; }
	static const char *const B[2] = { "Forget", "Cancel" };
	rui_confirm(SW, "⚡ Forget what Blitzen kept too?", "Learning is off. What it already kept of what you did can be forgotten now, or later in See what Blitzen knows.", B, 2, 0, forget_ans, NULL);
}
/* (a note's line: .blzsee1, 12 px, flex: 📝, its words, ✕) */
typedef struct { GtkWidget *a, *x[50]; char *t[50]; int n; } NoteBox;
static double notes_lay(void *d, double x, double y, double w)
{
	NoteBox *b = d; for (int i = 0; i < b->n; i++) p_place(b->x[i], x + w - p_nat_w(b->x[i]), y + i * 26 + 2, 0, 22); p_place(b->a, x, y, w, MAX(1, b->n * 26)); return b->n * 26;
}
static gboolean notes_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	NoteBox *b = d; double W = gtk_widget_get_allocated_width(w) / s_z(); rui_color(cr, "mut", 1);
	for (int i = 0; i < b->n; i++) { s_text(cr, "📝", 0, i * 26 + 6, 12, 400, 0, 14); s_text(cr, b->t[i], 23, i * 26 + 6, 12, 400, W - 23 - 8 - p_nat_w(b->x[i]) - 8, 14); }
	return FALSE;
}
static void knows_sec(void)
{
	p_h4("Blitzen: what it knows");
	p_msg("Before answering, Blitzen looks in Rudolf OS's own source and documents and the system's logs (with passwords, tokens and keys left out), and in what you ask it to remember. It does not learn by changing its model: what it uses can always be seen and forgotten here. It is a small model: good at “where in the system is …” and “what does this log line mean”, less so at weighing several web pages against each other; an answer that reads the web takes about a minute.", 2);
	if (ACCT[0]) {
		char er[300]; RuiJ *l = api("ai/know/learn", NULL, er, sizeof er); int on = jb(l, "learn"); rui_jfree(l);
		rui_jfree(NOTES); NOTES = api("ai/know/notes", NULL, er, sizeof er); NOTES_OK = NOTES != NULL;
		CRow *c = g_new0(CRow, 1); BKL = ccheck(c, "Learn from what I do on this computer", on, on_learn, NULL, "bklearn"); crow_done(c, 0);
		p_msg("The apps and files you open (names and folders, not what is in them), the settings you change and the errors you meet. Kept on this computer, in your account’s own encrypted folder only (as your notes are); never sent anywhere, never seen by another account.", 2);
		p_bold("Memory"); GPtrArray *a = notes_now();
		if (!a->len) p_msg("No notes. Say “Remember that …” to Blitzen, or add one here.", 2);
		else { NoteBox *b = g_new0(NoteBox, 1); b->a = gtk_drawing_area_new(); g_signal_connect(b->a, "draw", G_CALLBACK(notes_draw), b); p_put(b->a);
			for (guint i = 0; i < a->len && i < 50; i++) { b->t[b->n] = g_strdup(g_ptr_array_index(a, i)); GtkWidget *x = rui_button("✕", 0); gtk_widget_set_tooltip_text(x, "Forget this note"); g_signal_connect(x, "clicked", G_CALLBACK(on_forget_note), GUINT_TO_POINTER(i)); p_put(x);
				char nm[16]; snprintf(nm, sizeof nm, "bkx%u", i); p_label_named(nm, x); b->x[b->n++] = x; }
			p_custom(notes_lay, b, 0, 0); }
		g_ptr_array_unref(a);
		BRow *r = brow(); BKNEW = rui_entry("Something Blitzen should know about you"); gtk_entry_set_max_length(GTK_ENTRY(BKNEW), 300); p_put(BKNEW); p_label_named("bknew", BKNEW); r->e = BKNEW; r->eh = 23;
		g_signal_connect(BKNEW, "activate", G_CALLBACK(on_add_note), NULL); bbtn(r, "Add", 0, G_CALLBACK(on_add_note), NULL, "bkadd"); brow_done(r, 14);
	} else p_msg("Sign in to an account to have Blitzen learn what you do and keep notes.", 2);
	BRow *r = brow(); bbtn(r, "See what Blitzen knows…", 0, G_CALLBACK(on_see), NULL, "bksee"); brow_done(r, 14);
	p_label_named("bkm", p_msg(BKM ? BKM : "", 2));
}

/* ================================================================== Blitzen › Developer (blzdev60.js's): the tokens typed go to rudolfd
   (ai/dev/token, kept in the account's keyring) and are not kept here */
static GtkWidget *BDR, *BDB, *BDT, *BDL, *BDLT;
static void on_bdsave(GtkButton *b, gpointer d)
{
	(void)b; (void)d; char *r = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(BDR)))), *ba = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(BDB)))), *dl = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(BDL))));
	char *er_ = g_uri_escape_string(r, NULL, FALSE), *eb = g_uri_escape_string(*ba ? ba : "main", NULL, FALSE), *ed = g_uri_escape_string(dl, NULL, FALSE), *p = g_strdup_printf("ai/dev/settings?repo=%s&base=%s&devlink=%s", er_, eb, ed), er[300];
	RuiJ *j = api(p, NULL, er, sizeof er); int ok = j != NULL; rui_jfree(j); g_free(p); g_free(er_); g_free(eb); g_free(ed); g_free(r); g_free(ba); g_free(dl);
	struct { const char *k; GtkWidget *e; } T[2] = { { "github", BDT }, { "devlink", BDLT } };
	for (int i = 0; i < 2 && ok; i++) { char *v = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(T[i].e)))); if (*v) { char q[60]; snprintf(q, sizeof q, "ai/dev/token?key=%s", T[i].k); j = api(q, v, er, sizeof er); ok = j != NULL; rui_jfree(j); gtk_entry_set_text(GTK_ENTRY(T[i].e), ""); }
		memset(v, 0, strlen(v)); g_free(v); }
	say(&BDM, ok ? "Saved." : er);
}
static void on_bdx(GtkButton *b, gpointer d) { (void)b; char q[60], er[300]; snprintf(q, sizeof q, "ai/dev/token?key=%s&clear=1", (const char *)d); RuiJ *j = api(q, NULL, er, sizeof er); say(&BDM, j ? "Removed." : er); rui_jfree(j); }
static void on_bdopen(GtkButton *b, gpointer d) { (void)b; (void)d; ask("open do-blzdev"); }
static GtkWidget *tok(const char *ph, const char *name) { GtkWidget *e = rui_entry(ph); gtk_entry_set_visibility(GTK_ENTRY(e), FALSE); gtk_entry_set_input_purpose(GTK_ENTRY(e), GTK_INPUT_PURPOSE_PASSWORD); p_label_named(name, e); return e; }
static void blzdev_sec(void)
{
	char er[300]; RuiJ *S = api("ai/dev/settings", NULL, er, sizeof er); if (!S) return;
	p_h4("Blitzen › Developer");
	char *m = g_strdup_printf("Blitzen can work on your own Rudolf OS repository (a fork: docs/custom-builds.md) and drive a test computer through your development link (docs/DEVLINK.md). It reads freely; it changes nothing without your Allow, each time: a new branch and a pull request (never your default branch, never merged), a workflow started, a command sent. Tokens are kept in this account's keyring%s.", jb(S, "keyring") ? "" : " (this computer has no account keyring: for the system alone)");
	p_msg(m, 2); g_free(m);
	Cell *c = p_r2("Your repository"); BDR = rui_entry("you/RudolfOS"); gtk_entry_set_text(GTK_ENTRY(BDR), set_jstr(S, "repo", "")); c_add(c, BDR, 0, 0, 1); p_label_named("bdr", BDR); p_r2_more(c);
	c = p_r2("Its main branch"); BDB = rui_entry(NULL); gtk_entry_set_text(GTK_ENTRY(BDB), set_jstr(S, "base", "")); c_add(c, BDB, 0, 0, 1); p_label_named("bdb", BDB); p_r2_more(c);
	c = p_r2("GitHub token"); BDT = tok(jb(S, "github") ? "kept (type a new one to replace it)" : "github_pat_…", "bdt"); gtk_widget_set_size_request(BDT, (int)lround(177 * s_z()), (int)lround(19 * s_z())); c_add(c, BDT, 0, 2, 0);
	if (jb(S, "github")) { GtkWidget *x = rui_button("Remove", 0); g_signal_connect(x, "clicked", G_CALLBACK(on_bdx), (gpointer)"github"); c_add(c, x, sp_w(), 0, 0); p_label_named("bdtx", x); } p_r2_more(c);
	c = p_r2("Development link"); BDL = rui_entry("you/rudolfos-devlink (private)"); gtk_entry_set_text(GTK_ENTRY(BDL), set_jstr(S, "devlink", "")); c_add(c, BDL, 0, 0, 1); p_label_named("bdl", BDL); p_r2_more(c);
	c = p_r2("Its token"); BDLT = tok(jb(S, "devlinkToken") ? "kept (type a new one to replace it)" : "github_pat_…", "bdlt"); gtk_widget_set_size_request(BDLT, (int)lround(177 * s_z()), (int)lround(19 * s_z())); c_add(c, BDLT, 0, 0, 0);
	if (jb(S, "devlinkToken")) { GtkWidget *x = rui_button("Remove", 0); g_signal_connect(x, "clicked", G_CALLBACK(on_bdx), (gpointer)"devlink"); c_add(c, x, sp_w(), 0, 0); p_label_named("bdltx", x); }
	if (jb(S, "official")) p_msg("\x03The GitHub token: a fine-grained token for your repository alone, with Contents, Pull requests and Actions: read and write. <b>This is the official repository: Blitzen can only read it, and suggest changes as pull requests from your own fork.</b>", 2);
	else p_msg("The GitHub token: a fine-grained token for your repository alone, with Contents, Pull requests and Actions: read and write. ", 2);
	BRow *r = brow(); bbtn(r, "Save", 1, G_CALLBACK(on_bdsave), NULL, "bdsave"); bbtn(r, "Open Blitzen Developer", 0, G_CALLBACK(on_bdopen), NULL, "bdopen"); brow_done(r, 14);
	p_label_named("bdm", p_msg(BDM ? BDM : "", 2)); rui_jfree(S);
}

/* ================================================================== Calls to phones (phonecalls59.js's) */
typedef struct { GtkWidget *a, *b[8]; int n; double h[8]; } PhBox;
static double ph_rowh(int i, double w) { return fmax(fmax(25, 23), 15 + 10 + s_lines(PHONE59[i][3], 13, 400, w - 27.5 - 10 - 10 - 119.1, NULL) * 15) + 12 + 1; }
static double ph_lay(void *d, double x, double y, double w)
{
	PhBox *b = d; double cy = 0; for (int i = 0; i < b->n; i++) { b->h[i] = ph_rowh(i, w); p_place(b->b[i], x + w - p_nat_w(b->b[i]), y + cy + (b->h[i] - 1 - 23) / 2, 0, 0); cy += b->h[i]; }
	p_place(b->a, x, y, w, cy); return cy;
}
static gboolean ph_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	PhBox *b = d; double W = gtk_widget_get_allocated_width(w) / s_z(), y = 0;
	for (int i = 0; i < b->n; i++) { double h = ph_rowh(i, W), tw_ = W - 27.5 - 10 - 10 - p_nat_w(b->b[i]); int n = s_lines(PHONE59[i][3], 13, 400, tw_, NULL); double ch = 15 + 10 + n * 15, cy = y + (h - 1 - ch) / 2;
		s_col(cr, "acr", 1); cairo_rectangle(cr, 0, y + h - 1, W, 1); cairo_fill(cr); s_col(cr, "fg", 1);
		s_text(cr, PHONE59[i][1], 0, y + (h - 1 - 25) / 2, 22, 400, 0, 25); s_text(cr, PHONE59[i][0], 37.5, cy, 13, 700, 0, 15); s_text(cr, PHONE59[i][3], 37.5, cy + 25, 13, 400, tw_, 15); y += h; }
	return FALSE;
}
static void on_ph(GtkButton *b, gpointer d) { (void)b; char a[300]; snprintf(a, sizeof a, "browser %s", PHONE59[GPOINTER_TO_INT(d)][2]); ask(a); }
static void on_telask(GtkButton *b, gpointer d) { (void)b; (void)d; static const char *const K[1] = { "telApp" }, *const V[1] = { NULL }; set_store(K, V, 1); set_note("keep telApp (deleted)"); redraw(); }
static void phones_sec(void)
{
	p_h4("Calls to phones");
	p_msg_m("\x03Video calls work in Oracle (Google Meet, Zoom, Microsoft Teams) and with Zoom’s app. Calling a <b>phone number</b> needs a calling plan from one of these services; Rudolf OS does not sign you up for anything.", 0, 10, 13);
	PhBox *b = g_new0(PhBox, 1); b->a = gtk_drawing_area_new(); g_signal_connect(b->a, "draw", G_CALLBACK(ph_draw), b); p_put(b->a);
	for (guint i = 0; i < G_N_ELEMENTS(PHONE59) && i < 8; i++) { GtkWidget *x = rui_button("Open in Oracle", 0); g_signal_connect(x, "clicked", G_CALLBACK(on_ph), GINT_TO_POINTER(i)); p_put(x); char nm[12]; snprintf(nm, sizeof nm, "ph%u", i); p_label_named(nm, x); b->b[b->n++] = x; }
	p_custom(ph_lay, b, 0, 0);
	RuiJ *st = set_stored(); const char *tel = set_jstr(st, "telApp", NULL);
	if (tel && *tel) { const char *n = !strcmp(tel, "phone") ? "Phone screen" : !strcmp(tel, "voice") ? "Google Voice" : !strcmp(tel, "teams") ? "Microsoft Teams" : tel;
		char *e = g_markup_escape_text(n, -1), *t = g_strdup_printf("\x03Phone numbers on web pages open with <b>%s</b>. ", e); p_msg_m(t, 0, 10, 13); g_free(t); g_free(e);
		BRow *r = brow(); bbtn(r, "Ask each time", 0, G_CALLBACK(on_telask), NULL, "telask"); brow_done(r, 0); }
	rui_jfree(st);
}

void page_apps(void)
{
	lx_load(); p_h3("Apps"); defapps(); links(); installed(); linux_sec(); blitzen_sec(); devtools_sec(); knows_sec(); blzdev_sec(); phones_sec();
}
