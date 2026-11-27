/*
 * rudolf-settings: Privacy (settings25.js privPanel, with keyring60.js's Saved passwords for apps and orablock61.js's Ads and trackers in
 * Oracle). 1.6.3, docs/plan-1.6.3.md Part 3 (PR 8). As the page: the camera's own switch (camera?on=, the administrator's prompt), the
 * keyring's list (keyring/list, keyring/forget: names only, never a secret; each Remove asked first), the apps' permissions
 * (bd95perm:ACCT) and their log (bd95permlog:ACCT), Clear recent files (bd95recent:ACCT), notification history (bd95notif:ACCT) and the
 * clipboard history (the hidden page's, "stor clipclear"), Oracle's blocking (S.oblock, kept at once as persistS). GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "settings.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
#pragma GCC diagnostic ignored "-Wunused-variable"
#include "settings-gen.h"
#pragma GCC diagnostic pop

static double tw(const char *t, double px, int wt) { double w = 0; s_lines(t ? t : "", px, wt, 0, &w); return w; }
static double sp_w(void) { double sp, a1; s_lines("a a", 13, 400, 0, &sp); s_lines("aa", 13, 400, 0, &a1); return sp - a1; }
static double word_max(const char *t, double px, int wt) { double m = 0; char **w = g_regex_split_simple("\\s+", t ? t : "", 0, 0); for (int i = 0; w[i]; i++) if (*w[i]) m = fmax(m, tw(w[i], px, wt)); g_strfreev(w); return m; }
static void redraw(void) { set_show_keep("priv"); }
static void ask(const char *a) { rui_shell(SW, a); set_note("%s", a); }
static int jb(RuiJ *o, const char *k) { RuiJ *v = o ? rui_jprop(o, k) : NULL; return v && ((rui_jtype(v) == 1 && rui_jval(v)) || (rui_jtype(v) == 2 && rui_jval(v)) || (rui_jtype(v) == 3 && *rui_jsval(v))); }
static double jnum(RuiJ *o, const char *k) { RuiJ *v = o ? rui_jprop(o, k) : NULL; return v && rui_jtype(v) == 2 ? rui_jval(v) : NAN; }
static RuiJ *post(const char *path, char *err, size_t en)
{
	char p[600], *out = NULL; snprintf(p, sizeof p, "/api/%s", path); int c = rui_http("POST", p, NULL, 0, &out, NULL); RuiJ *j = out ? rui_jparse(out) : NULL; g_free(out); set_note("POST %s %d", path, c);
	if (c != 200 || !j) { if (err) snprintf(err, en, "%s", j ? set_jstr(j, "error", "Request failed") : "rudolfd did not answer"); rui_jfree(j); return NULL; } return j;
}
static RuiJ *get(const char *path) { char p[300]; snprintf(p, sizeof p, "/api/%s", path); return rui_get(p); }
static RuiJ *X(RuiJ *o) { return o ? rui_jprop(o, "x") : NULL; }   /* (set_ls_get's {"x": value}) */
static void svj(const char *k, const char *json) { set_ls_set(k, json); set_note("svj %s %s", k, json); }
static void ctext(void *c, const char *s);

/* ================================================================== a .tmt of the page's, up to four columns (CSS's automatic table
   layout: each column between its narrowest and widest contents), each cell writing, an icon and a name in bold over small writing
   (storage56.js's lines), a bar (.stbar, 80px), or a button; a cell's writing at the right when asked */
enum { C_TEXT, C_LINE, C_BAR, C_BTN };
typedef struct { int k, right; char *t, *icon, *b, *s; double frac; GtkWidget *w; } TCell;
typedef struct { TCell c[4]; int span; } TRow;   /* (span: one cell across the columns, at .6 opacity: colspan) */
typedef struct { GtkWidget *a; GArray *rows; int nc; double cw[4], W; const char *th[4]; int hdr; } NTab;
static void cell_mm(TCell *c, double *mn, double *mx)
{
	switch (c->k) {
	case C_TEXT: *mx = tw(c->t, 13, 400); *mn = word_max(c->t, 13, 400); break;
	case C_LINE: *mx = fmax(tw(c->icon, 13, 400) + sp_w() + tw(c->b, 13, 700), tw(c->s, 10.8333, 400)); *mn = fmax(fmax(tw(c->icon, 13, 400), word_max(c->b, 13, 700)), word_max(c->s, 10.8333, 400)); break;
	case C_BAR: *mx = *mn = 82; break;
	default: *mx = c->w ? p_nat_w(c->w) + GPOINTER_TO_INT(g_object_get_data(G_OBJECT(c->w), "bw")) : 0; *mn = c->w ? fmin(*mx, word_max(gtk_button_get_label(GTK_BUTTON(c->w)), 13, 400) + 22) : 0;
		if (c->w && g_object_get_data(G_OBJECT(c->w), "w2")) { double w2 = p_nat_w(g_object_get_data(G_OBJECT(c->w), "w2")); *mx += sp_w() + w2; *mn = fmax(*mn, w2); } break; }
	*mx += 12; *mn += 12;
}
static void ntab_cols(NTab *t, double W)
{
	double mn[4] = { 0 }, mx[4] = { 0 }, smn = 0, smx = 0;
	for (guint i = 0; i < t->rows->len; i++) { TRow *r = &g_array_index(t->rows, TRow, i); if (r->span) continue; for (int c = 0; c < t->nc; c++) { double a, b; cell_mm(&r->c[c], &a, &b); mn[c] = fmax(mn[c], a); mx[c] = fmax(mx[c], b); } }
	if (t->hdr) for (int c = 0; c < t->nc; c++) { double w = tw(t->th[c], 13, 700) + 12; mx[c] = fmax(mx[c], w); mn[c] = fmax(mn[c], word_max(t->th[c], 13, 700) + 12); }
	for (int c = 0; c < t->nc; c++) { smn += mn[c]; smx += mx[c]; }
	for (int c = 0; c < t->nc; c++) t->cw[c] = smx <= W ? mx[c] + (W - smx) * mx[c] / fmax(1, smx) : mn[c] + fmax(0, W - smn) * (smx - smn > 0 ? (mx[c] - mn[c]) / (smx - smn) : 1.0 / t->nc);
	t->W = W;
}
static double btn_h(TCell *c, double w) { if (!c->w) return 0; if (g_object_get_data(G_OBJECT(c->w), "bh")) return GPOINTER_TO_INT(g_object_get_data(G_OBJECT(c->w), "bh")); double nw = p_nat_w(c->w); return nw <= w + .5 ? p_nat_h(c->w) : p_nat_h(c->w) + 15 * (s_lines(gtk_button_get_label(GTK_BUTTON(c->w)), 13, 400, w - 22, NULL) - 1); }
static double cell_h(TCell *c, double w)
{
	switch (c->k) { case C_TEXT: return s_lines(c->t, 13, 400, w, NULL) * 15; case C_LINE: return 15 + (c->s && *c->s ? s_lines(c->s, 10.8333, 400, w, NULL) : 1) * 15; case C_BAR: return 10; default: return btn_h(c, w); }
}
static double row_h(NTab *t, TRow *r) { if (r->span) return s_lines(r->c[0].t, 13, 400, t->W - 12, NULL) * 15 + 9; double h = 0; for (int c = 0; c < t->nc; c++) h = fmax(h, cell_h(&r->c[c], t->cw[c] - 12)); return h + 9; }
static double ntab_lay(void *d, double x, double y, double w)
{
	NTab *t = d; ntab_cols(t, w); double h = t->hdr ? 24 : 0;
	for (guint i = 0; i < t->rows->len; i++) { TRow *r = &g_array_index(t->rows, TRow, i); double rh = row_h(t, r), cx = 0;
		for (int c = 0; c < t->nc; c++) { TCell *e = &r->c[c]; if (e->k == C_BTN && e->w) { double bw = fmin(p_nat_w(e->w) + GPOINTER_TO_INT(g_object_get_data(G_OBJECT(e->w), "bw")), t->cw[c] - 12), bh = btn_h(e, t->cw[c] - 12);
				p_place(e->w, x + cx + 6, y + h + (rh - 1 - bh) / 2, bw, bh); GtkWidget *w2 = g_object_get_data(G_OBJECT(e->w), "w2"); if (w2) p_place(w2, x + cx + 6 + bw + sp_w(), y + h + (rh - 1 - p_nat_h(w2)) / 2, 0, 0); if (bw < p_nat_w(e->w) - .5) { GtkWidget *l = gtk_bin_get_child(GTK_BIN(e->w)); gtk_label_set_line_wrap(GTK_LABEL(l), TRUE); gtk_label_set_justify(GTK_LABEL(l), GTK_JUSTIFY_CENTER); } }
			cx += t->cw[c]; }
		h += rh; }
	p_place(t->a, x, y, w, MAX(1, h)); return h;
}
static gboolean ntab_draw(GtkWidget *wg, cairo_t *cr, gpointer d)
{
	NTab *t = d; double W = gtk_widget_get_allocated_width(wg) / s_z(), y = 0; if (fabs(W - t->W) > .1) ntab_cols(t, W);
	if (t->hdr) { s_col(cr, "th", 1); cairo_rectangle(cr, 0, 0, W, 23); cairo_fill(cr); rui_color(cr, "bd", 1); cairo_rectangle(cr, 0, 23, W, 1); cairo_fill(cr); double cx = 0; s_col(cr, "fg", 1);
		for (int c = 0; c < t->nc; c++) { s_text(cr, t->th[c], cx + 6, 4, 13, 700, 0, 15); cx += t->cw[c]; } y = 24; }
	for (guint i = 0; i < t->rows->len; i++) { TRow *r = &g_array_index(t->rows, TRow, i); double rh = row_h(t, r), cx = 0;
		s_col(cr, "td", 1); cairo_rectangle(cr, 0, y + rh - 1, W, 1); cairo_fill(cr);
		if (r->span) { cairo_push_group(cr); s_col(cr, "fg", 1); s_text(cr, r->c[0].t, 6, y + 4, 13, 400, W - 12, 15); cairo_pop_group_to_source(cr); cairo_paint_with_alpha(cr, .6); y += rh; continue; }
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
static void ctext(void *cv, const char *s) { TCell *c = cv; c->k = C_TEXT; c->t = g_strdup(s ? s : ""); }


/* buttons in a .rmrow (flex, wrapping, gap 8 px), or one inline button (no row: margin 0) */
typedef struct { GtkWidget *b[8]; int n; } BRow;
static double brow_lay(void *d, double x, double y, double w)
{
	BRow *r = d; double cx = 0, cy = 0, lh = 0;
	for (int k = 0; k < r->n; k++) { double bw = p_nat_w(r->b[k]), bh = p_nat_h(r->b[k]); if (cx > 0 && cx + bw > w + .5) { cx = 0; cy += lh + 8; lh = 0; } p_place(r->b[k], x + cx, y + cy, 0, 0); cx += bw + 8; lh = fmax(lh, bh); }
	return cy + lh;
}
static GtkWidget *bbtn(BRow *r, const char *l, GCallback cb, void *d, const char *name) { GtkWidget *b = rui_button(l, 0); if (cb) g_signal_connect(b, "clicked", cb, d); p_put(b); if (name) p_label_named(name, b); r->b[r->n++] = b; return b; }
static BRow *brow(void) { return g_new0(BRow, 1); }

/* ================================================================== the page's words with its link (Read the privacy statement: Help) */
typedef struct { GtkWidget *a; char *t, *l; } Link;
static char *link_markup(Link *k) { char *e = g_markup_escape_text(k->t, -1), *f = g_markup_escape_text(k->l, -1), *m = g_strdup_printf("%s<span foreground=\"%s\" underline=\"single\">%s</span>", e, RUI.dark ? "#9e9eff" : "#0000ee", f); g_free(e); g_free(f); return m; }
static PangoLayout *link_layout(Link *k, double w) { char *m = link_markup(k); PangoLayout *l = s_layout("", 13, 400, w); pango_layout_set_markup(l, m, -1); g_free(m); return l; }
static double link_lay(void *d, double x, double y, double w) { Link *k = d; PangoLayout *l = link_layout(k, w); double h = pango_layout_get_line_count(l) * 15; g_object_unref(l); p_place(k->a, x, y, w, h); return h; }
static gboolean link_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Link *k = d; double z = s_z(); PangoLayout *l = link_layout(k, gtk_widget_get_allocated_width(w) / z); s_col(cr, "fg", 1); int n = pango_layout_get_line_count(l);
	for (int i = 0; i < n; i++) { PangoLayoutIter *it = pango_layout_get_iter(l); for (int q = 0; q < i; q++) pango_layout_iter_next_line(it); int bl = pango_layout_iter_get_baseline(it); pango_layout_iter_free(it);
		cairo_save(cr); cairo_scale(cr, 1 / z, 1 / z); double asc = s_asc(13, 400, NULL); cairo_move_to(cr, 0, (i * 15 + (15 - s_lh(13, 400)) / 2 + asc) * z); (void)bl; pango_cairo_show_layout_line(cr, pango_layout_get_line_readonly(l, i)); cairo_restore(cr); }
	g_object_unref(l); return FALSE;
}
static gboolean link_click(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	Link *k = d; double z = s_z(); PangoLayout *l = link_layout(k, gtk_widget_get_allocated_width(w) / z); int idx, tr; int line = (int)(e->y / z / 15); PangoLayoutLine *ln = pango_layout_get_line_readonly(l, MIN(line, pango_layout_get_line_count(l) - 1));
	gboolean in = pango_layout_line_x_to_index(ln, (int)(e->x * PANGO_SCALE), &idx, &tr); int start = (int)strlen(k->t); g_object_unref(l);
	if (in && idx >= start) { ask("open help privacy"); return TRUE; } return FALSE;
}
static void p_link(const char *t, const char *link, double mt, double mb, const char *name)
{
	Link *k = g_new0(Link, 1); k->t = g_strdup(t); k->l = g_strdup(link); k->a = gtk_drawing_area_new(); gtk_widget_add_events(k->a, GDK_BUTTON_PRESS_MASK); g_signal_connect(k->a, "draw", G_CALLBACK(link_draw), k);
	g_signal_connect(k->a, "button-press-event", G_CALLBACK(link_click), k); p_put(k->a); p_label_named(name, k->a); p_custom(link_lay, k, mt, mb);
}

/* ================================================================== Camera (rudolfd's camera, camera?on=) */
static GtkWidget *CAMCB; static char *CAMMSG;
static void cam_go(int ok, void *d)
{
	int on = GPOINTER_TO_INT(d); if (!ok) { rui_check_set(CAMCB, !on); return; } char p[40], er[300]; snprintf(p, sizeof p, "camera?on=%d", on); RuiJ *j = post(p, er, sizeof er); g_free(CAMMSG); CAMMSG = j ? NULL : g_strdup(er); rui_jfree(j); redraw();
}
static void on_cam(int on, void *d) { (void)d; char *r = g_strdup_printf("Switch the camera %s", on ? "on" : "off"); set_note("elevate %s", r); rui_elevate(SW, r, cam_go, GINT_TO_POINTER(on)); g_free(r); }
static void camera_sec(void)
{
	RuiJ *r = get("camera"); if (!r) return; p_h4("Camera"); int on = jb(r, "on"); double n = jnum(r, "devices");
	CAMCB = p_check("Camera on", on, on_cam, NULL); p_label_named("pcs", CAMCB);
	char *t = CAMMSG ? g_strdup(CAMMSG) : on ? (n > 0 ? g_strdup_printf("%.0f camera device%s. Apps and websites must still ask before using it.", n, n > 1 ? "s" : "") : g_strdup("No camera is connected now.")) : g_strdup("Off: no app, website or Linux app can use the camera, and its driver is not loaded. It stays off after a restart.");
	p_label_named("pcm", p_msg_m(t, 0, 10, 13)); g_free(t); rui_jfree(r);
}

/* ================================================================== Saved passwords for apps (keyring60.js krSection) */
static char *KRSAID; static RuiJ *KRL;
static const char *kr_who(RuiJ *it, char *b, size_t n)
{
	const char *app = set_jstr(it, "app", ""), *sch = set_jstr(it, "schema", "");
	if (!strcmp(set_jstr(it, "kind", ""), "flatpak")) { snprintf(b, n, "%s (Flatpak app)", app); return b; } if (*app) return app;
	if (!strcmp(sch, "chrome_libsecret_os_crypt_password_v2") || !strcmp(sch, "chrome_libsecret_os_crypt_password_v1")) return "Chromium-based app (its saved logins)";
	if (!strcmp(sch, "org.freedesktop.Secret.Generic")) return "An app"; if (!strcmp(sch, "org.gnome.keyring.Note")) return "A note"; if (!strcmp(sch, "org.gnome.keyring.NetworkPassword")) return "A network password";
	return *sch ? sch : "An app";
}
static int KR_I;
static void kr_one(int i, void *d)
{
	(void)d; if (i != 0) return; RuiJ *it = rui_jat(rui_jprop(KRL, "items"), KR_I); char *e = g_uri_escape_string(set_jstr(it, "id", ""), NULL, FALSE), p[300], er[300]; snprintf(p, sizeof p, "keyring/forget?id=%s", e); g_free(e);
	RuiJ *j = post(p, er, sizeof er); g_free(KRSAID); KRSAID = j ? g_strdup("Removed.") : g_strdup_printf("Not removed: %s", er); rui_jfree(j); redraw();
}
static void on_kr(GtkButton *b, gpointer d)
{
	(void)b; KR_I = GPOINTER_TO_INT(d); RuiJ *it = rui_jat(rui_jprop(KRL, "items"), KR_I); char w[300]; const char *who = kr_who(it, w, sizeof w), *lb = set_jstr(it, "label", "");
	char *q = g_strdup_printf("Remove \"%s\"? %s will be signed out of what it kept here.", *lb ? lb : who, who); static const char *const B[2] = { "Remove", "Cancel" }; set_note("ask %s", q);
	rui_confirm(SW, NULL, q, B, 2, 0, kr_one, NULL); g_free(q);
}
static void kr_all(int i, void *d)
{
	(void)d; if (i != 0) return; RuiJ *items = rui_jprop(KRL, "items"); int bad = 0;
	for (int k = 0; items && k < rui_jlen(items); k++) { char *e = g_uri_escape_string(set_jstr(rui_jat(items, k), "id", ""), NULL, FALSE), p[300], er[300]; snprintf(p, sizeof p, "keyring/forget?id=%s", e); g_free(e); RuiJ *j = post(p, er, sizeof er); if (!j) bad++; rui_jfree(j); }
	g_free(KRSAID); KRSAID = bad ? g_strdup_printf("%d could not be removed.", bad) : g_strdup("All removed."); redraw();
}
static void on_krall(GtkButton *b, gpointer d) { (void)b; (void)d; int n = rui_jlen(rui_jprop(KRL, "items")); char *q = g_strdup_printf("Remove all %d saved passwords for apps? Each app will be signed out of what it kept here.", n); static const char *const B[2] = { "Remove all", "Cancel" }; set_note("ask %s", q); rui_confirm(SW, NULL, q, B, 2, 0, kr_all, NULL); g_free(q); }
static void keyring_sec(void)
{
	p_h4("Saved passwords for apps");
	p_msg_m("Linux apps keep their sign-ins and tokens in this account's keyring, unlocked when you sign in. Only this account's apps can read it. Removing one signs that app out of it.", 0, 10, 13);
	rui_jfree(KRL); KRL = get("keyring/list"); NTab *t = ntab(3); t->hdr = 1; t->th[0] = "Saved by"; t->th[1] = "What"; t->th[2] = ""; RuiJ *items = rui_jprop(KRL, "items"); int n = items ? rui_jlen(items) : 0;
	const char *one = !KRL ? "The saved passwords could not be read: rudolfd did not answer" : !jb(KRL, "signedIn") ? "Sign in to an account to see its saved passwords." : !n ? (jb(KRL, "keyring") ? "No app has saved a password yet." : "The keyring is not running now (it starts when you sign in).") : NULL;
	if (one) { TRow *r = nrow(t); r->span = 1; ctext(&r->c[0], one); }
	else for (int i = 0; i < n && i < 60; i++) { RuiJ *it = rui_jat(items, i); TRow *r = nrow(t); char w[300]; ctext(&r->c[0], kr_who(it, w, sizeof w)); ctext(&r->c[1], *set_jstr(it, "label", "") ? set_jstr(it, "label", "") : "(no name)");
		r->c[2].k = C_BTN; r->c[2].w = rui_button("Remove", 0); gtk_widget_set_tooltip_text(r->c[2].w, "Remove it: the app is signed out of what it kept"); g_object_set_data(G_OBJECT(r->c[2].w), "bh", GINT_TO_POINTER(27)); g_object_set_data(G_OBJECT(r->c[2].w), "bw", GINT_TO_POINTER(-4));   /* (.pkr: padding 5px 8px) */ g_signal_connect(r->c[2].w, "clicked", G_CALLBACK(on_kr), GINT_TO_POINTER(i)); p_put(r->c[2].w);
		char nm[16]; snprintf(nm, sizeof nm, "pkr%d", i); p_label_named(nm, r->c[2].w); }
	p_custom(ntab_lay, t, 0, 0);
	BRow *b = brow(); GtkWidget *x = bbtn(b, "Remove all", G_CALLBACK(on_krall), NULL, "pka"); gtk_widget_set_sensitive(x, !one); p_custom(brow_lay, b, 14, 0);
	p_label_named("pkm", p_msg_m(KRSAID ? KRSAID : "", 0, 10, 13));
}

/* ================================================================== App permissions, Recent activity, Other data */
static void perm_key(char *k, size_t n, const char *w) { snprintf(k, n, "bd95%s:%s", w, ACCT); }
static void on_perm(const char *v, void *d)
{
	char k[120]; perm_key(k, sizeof k, "perm"); RuiJ *ao = set_ls_get(k), *a = X(ao); GString *b = g_string_new("{"); int first = 1;
	for (int i = 0; a && rui_jtype(a) == 5 && i < rui_jlen(a); i++) { const char *q = rui_jkey(a, i); if (!strcmp(q, d)) continue; if (!first) g_string_append_c(b, ','); first = 0; char *e = set_jesc(q); g_string_append_printf(b, "\"%s\":", e); g_free(e); set_jw(b, rui_jprop(a, q)); }
	if (strcmp(v, "ask")) { if (!first) g_string_append_c(b, ','); char *e = set_jesc(d); g_string_append_printf(b, "\"%s\":\"%s\"", e, v); g_free(e); }
	g_string_append_c(b, '}'); svj(k, b->str); g_string_free(b, TRUE); rui_jfree(ao); redraw();
}
static const char *perm_name(const char *p) { for (guint i = 0; i < G_N_ELEMENTS(PERMNAMES); i++) if (!strcmp(PERMNAMES[i][0], p)) return PERMNAMES[i][1]; return p; }
static void on_forget(GtkButton *b, gpointer d) { (void)b; char k[120]; perm_key(k, sizeof k, d); svj(k, !strcmp(d, "perm") ? "{}" : "[]"); redraw(); }
static void on_recent(GtkButton *b, gpointer d) { (void)b; (void)d; char k[120]; perm_key(k, sizeof k, "recent"); svj(k, "[]"); set_notify("Recent files cleared", "", 0); }
static void on_notif(GtkButton *b, gpointer d) { (void)b; (void)d; char k[120]; perm_key(k, sizeof k, "notif"); svj(k, "[]"); }
static void on_clip(GtkButton *b, gpointer d) { (void)b; (void)d; ask("stor clipclear"); }
typedef struct { GtkWidget *a; GPtrArray *l; int none; } Lines;
static double lines_lay(void *d, double x, double y, double w) { Lines *L = d; double h = 0; for (guint i = 0; i < L->l->len; i++) h += s_lines(g_ptr_array_index(L->l, i), 13, 400, w, NULL) * 15; p_place(L->a, x, y, w, MAX(1, h)); return h; }
static gboolean lines_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Lines *L = d; double W = gtk_widget_get_allocated_width(w) / s_z(), y = 0; if (L->none) cairo_push_group(cr); s_col(cr, "fg", 1);
	for (guint i = 0; i < L->l->len; i++) { const char *t = g_ptr_array_index(L->l, i); s_text(cr, t, 0, y, 13, 400, W, 15); y += s_lines(t, 13, 400, W, NULL) * 15; }
	if (L->none) { cairo_pop_group_to_source(cr); cairo_paint_with_alpha(cr, .6); } return FALSE;
}
static void perms_sec(void)
{
	p_h4("App permissions"); char k[120]; perm_key(k, sizeof k, "perm"); RuiJ *ao = set_ls_get(k), *a = X(ao); NTab *t = ntab(3); t->hdr = 1; t->th[0] = "App"; t->th[1] = "Permission"; t->th[2] = "Choice";
	int n = a && rui_jtype(a) == 5 ? rui_jlen(a) : 0;
	if (!n) { TRow *r = nrow(t); r->span = 1; ctext(&r->c[0], "No app has asked yet."); }
	for (int i = 0; i < n && i < 80; i++) { const char *key = rui_jkey(a, i); char *app = g_strdup(key), *perm = strchr(app, '|'); if (perm) *perm++ = 0; else perm = "";
		TRow *r = nrow(t); ctext(&r->c[0], app); ctext(&r->c[1], perm_name(perm)); const char *cur = set_jstr(a, key, "");
		static const char *const V[3] = { "allow", "deny", "ask" }, *const L[3] = { "Allow", "Don’t allow", "Ask every time" };
		r->c[2].k = C_BTN; r->c[2].w = s_select(V, L, 3, !strcmp(cur, "allow") || !strcmp(cur, "deny") ? cur : "allow", on_perm, g_strdup(key)); s_select_width(r->c[2].w, 0); p_put(r->c[2].w); char nm[16]; snprintf(nm, sizeof nm, "ppm%d", i); p_label_named(nm, r->c[2].w); g_free(app); }
	p_custom(ntab_lay, t, 0, 0); rui_jfree(ao);
	BRow *b = brow(); bbtn(b, "Forget all choices", G_CALLBACK(on_forget), "perm", "pr"); p_custom(brow_lay, b, 0, 0);
	p_h4("Recent activity"); perm_key(k, sizeof k, "permlog"); RuiJ *lo = set_ls_get(k), *l = X(lo); Lines *L = g_new0(Lines, 1); L->l = g_ptr_array_new_with_free_func(g_free); L->a = gtk_drawing_area_new(); g_signal_connect(L->a, "draw", G_CALLBACK(lines_draw), L); p_put(L->a);
	int m = l && rui_jtype(l) == 4 ? MIN(rui_jlen(l), 40) : 0;
	for (int i = 0; i < m; i++) { RuiJ *x = rui_jat(l, i); time_t at = (time_t)(jnum(x, "at") / 1000); struct tm lt; localtime_r(&at, &lt); char d[64], h[16]; strftime(d, sizeof d, "%-m/%-d/%Y", &lt); int hr = lt.tm_hour % 12 ? lt.tm_hour % 12 : 12; snprintf(h, sizeof h, "%d:%02d:%02d %s", hr, lt.tm_min, lt.tm_sec, lt.tm_hour < 12 ? "AM" : "PM");
		g_ptr_array_add(L->l, g_strdup_printf("%s, %s — %s: %s (%s)", d, h, set_jstr(x, "app", ""), perm_name(set_jstr(x, "perm", "")), jb(x, "ok") ? "allowed" : "denied")); }
	if (!m) { L->none = 1; g_ptr_array_add(L->l, g_strdup("Nothing yet.")); } rui_jfree(lo); p_custom(lines_lay, L, 0, 0);
	b = brow(); bbtn(b, "Clear activity", G_CALLBACK(on_forget), "permlog", "pclr"); p_custom(brow_lay, b, 0, 0);
	p_h4("Other data"); b = brow(); bbtn(b, "Clear recent files", G_CALLBACK(on_recent), NULL, "rc"); bbtn(b, "Clear notification history", G_CALLBACK(on_notif), NULL, "cn"); bbtn(b, "Clear clipboard history", G_CALLBACK(on_clip), NULL, "cc"); p_custom(brow_lay, b, 14, 0);
}

/* ================================================================== Ads and trackers in Oracle (orablock61.js oraBlockPanel; S.oblock) */
static RuiJ *OB;
static void ob_save(int on, GPtrArray *off, GPtrArray *mine)
{
	GString *b = g_string_new(NULL); g_string_printf(b, "{\"on\":%s,\"off\":[", on ? "true" : "false");
	for (guint i = 0; i < off->len; i++) { char *e = set_jesc(g_ptr_array_index(off, i)); g_string_append_printf(b, "%s\"%s\"", i ? "," : "", e); g_free(e); } g_string_append(b, "],\"mine\":[");
	for (guint i = 0; i < mine->len; i++) { char *e = set_jesc(g_ptr_array_index(mine, i)); g_string_append_printf(b, "%s\"%s\"", i ? "," : "", e); g_free(e); } g_string_append(b, "]}");
	static const char *const K[1] = { "oblock" }; const char *V[1] = { b->str }; set_store(K, V, 1); set_note("keep oblock %s", b->str); g_string_free(b, TRUE);
}
static int dom_ok(const char *v) { return g_regex_match_simple("^[a-z0-9]([a-z0-9-]*[a-z0-9])?(\\.[a-z0-9]([a-z0-9-]*[a-z0-9])?)+$", v, 0, 0); }
static GPtrArray *ob_list(const char *k, int max) { GPtrArray *a = g_ptr_array_new_with_free_func(g_free); RuiJ *l = rui_jprop(OB, k); for (int i = 0; l && rui_jtype(l) == 4 && i < rui_jlen(l) && (int)a->len < max; i++) { const char *v = rui_jsval(rui_jat(l, i)); if (!v || !dom_ok(v)) continue; int dup = 0; for (guint q = 0; q < a->len; q++) if (!strcmp(g_ptr_array_index(a, q), v)) dup = 1; if (!dup) g_ptr_array_add(a, g_strdup(v)); } return a; }
static int ob_on(void) { RuiJ *v = rui_jprop(OB, "on"); return !(v && rui_jtype(v) == 1 && !rui_jval(v)); }
static void on_ob(int on, void *d) { (void)d; GPtrArray *a = ob_list("off", 500), *b = ob_list("mine", 2000); ob_save(on, a, b); g_ptr_array_unref(a); g_ptr_array_unref(b); redraw(); }
static void on_obx(GtkButton *bt, gpointer d)
{
	(void)bt; const char *w = d; int mine = w[0] == 'm'; const char *site = w + 2; GPtrArray *a = ob_list("off", 500), *b = ob_list("mine", 2000), *t = mine ? b : a;
	for (guint i = 0; i < t->len; i++) if (!strcmp(g_ptr_array_index(t, i), site)) { g_ptr_array_remove_index(t, i); break; } ob_save(ob_on(), a, b); g_ptr_array_unref(a); g_ptr_array_unref(b); redraw();
}
static GtkWidget *OBIN; static char *OBMSG;
static void on_obadd(GtkWidget *w, gpointer d)
{
	(void)w; (void)d; char *v = g_ascii_strdown(g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(OBIN)))), -1); GRegex *r1 = g_regex_new("^[a-z]+://", 0, 0, NULL), *r2 = g_regex_new("[/:?#].*$", 0, 0, NULL), *r3 = g_regex_new("^\\*\\.", 0, 0, NULL);
	char *a = g_regex_replace_literal(r1, v, -1, 0, "", 0, NULL), *b = g_regex_replace_literal(r2, a, -1, 0, "", 0, NULL), *c = g_regex_replace_literal(r3, b, -1, 0, "", 0, NULL); g_regex_unref(r1); g_regex_unref(r2); g_regex_unref(r3);
	if (!dom_ok(c)) { g_free(OBMSG); OBMSG = g_strdup("That is not a site's name."); redraw(); }
	else { GPtrArray *x = ob_list("off", 500), *y = ob_list("mine", 2000); int have = 0; for (guint i = 0; i < y->len; i++) if (!strcmp(g_ptr_array_index(y, i), c)) have = 1; if (!have) g_ptr_array_add(y, g_strdup(c)); ob_save(ob_on(), x, y); g_ptr_array_unref(x); g_ptr_array_unref(y); g_free(OBMSG); OBMSG = NULL; redraw(); }
	g_free(v); g_free(a); g_free(b); g_free(c);
}
/* (.oblkt: 600 weight, 8 px above; a site's line: its name and ✕, gap 8) */
typedef struct { GtkWidget *a; char *t; } Bold;
static double bold_lay(void *d, double x, double y, double w) { Bold *b = d; p_place(b->a, x, y, w, 15); return 15; }
static gboolean bold_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)w; Bold *b = d; s_col(cr, "fg", 1); s_text(cr, b->t, 0, 0, 13, 600, 0, 15); return FALSE; }
static void p_bold(const char *t, double mt) { Bold *b = g_new0(Bold, 1); b->t = g_strdup(t); b->a = gtk_drawing_area_new(); g_signal_connect(b->a, "draw", G_CALLBACK(bold_draw), b); p_put(b->a); p_custom(bold_lay, b, mt, 0); }
typedef struct { GtkWidget *a, *x[64]; char *t[64]; int n; } Sites;
static double sites_lay(void *d, double x, double y, double w) { Sites *s = d; double h = 0; for (int i = 0; i < s->n; i++) { double bh = p_nat_h(s->x[i]); p_place(s->x[i], x + tw(s->t[i], 13, 400) + 8, y + h, 0, 0); h += fmax(15, bh); } p_place(s->a, x, y, w, MAX(1, h)); return h; }
static gboolean sites_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)w; Sites *s = d; double h = 0; s_col(cr, "fg", 1); for (int i = 0; i < s->n; i++) { double bh = fmax(15, p_nat_h(s->x[i])); s_text(cr, s->t[i], 0, h + (bh - 15) / 2, 13, 400, 0, 15); h += bh; } return FALSE; }
static void sites(const char *k, const char *pfx, int max)
{
	GPtrArray *l = ob_list(k, max); if (!l->len) { p_msg("None", 0); g_ptr_array_unref(l); return; }
	Sites *s = g_new0(Sites, 1); s->a = gtk_drawing_area_new(); g_signal_connect(s->a, "draw", G_CALLBACK(sites_draw), s); p_put(s->a);
	for (guint i = 0; i < l->len && i < 64; i++) { s->t[s->n] = g_strdup(g_ptr_array_index(l, i)); GtkWidget *x = rui_button("✕", 0); char *tt = g_strdup_printf("Remove %s", s->t[s->n]); gtk_widget_set_tooltip_text(x, tt); g_free(tt);
		gtk_style_context_add_class(gtk_widget_get_style_context(x), "set-oblkx"); g_signal_connect_data(x, "clicked", G_CALLBACK(on_obx), g_strdup_printf("%s %s", pfx, s->t[s->n]), (GClosureNotify)(void (*)(void))g_free, 0); p_put(x); char nm[24]; snprintf(nm, sizeof nm, "obx%s%d", pfx, s->n); p_label_named(nm, x); s->x[s->n++] = x; }
	p_custom(sites_lay, s, 0, 0); g_ptr_array_unref(l);
}
typedef struct { GtkWidget *e, *b; } Add;
static double add_lay(void *d, double x, double y, double w) { (void)w; Add *a = d; p_place(a->e, x, y + 2.5, 185, 23); p_place(a->b, x + 191, y + 2.5, 0, 0); if (OBMSG) { /* (its words after the button) */ } return 28; }
static void oblock_sec(void)
{
	RuiJ *st = set_stored(); rui_jfree(OB); OB = NULL; RuiJ *o = rui_jprop(st, "oblock"); if (o && rui_jtype(o) == 5) { GString *b = g_string_new(NULL); set_jw(b, o); OB = rui_jparse(b->str); g_string_free(b, TRUE); } rui_jfree(st);
	p_h4("Ads and trackers in Oracle"); p_label_named("oblkon", p_check("Block ads and trackers on websites", ob_on(), on_ob, NULL));
	GString *src = g_string_new(NULL); int ns = 0; for (int i = 0; ORABLOCK_SRC[i]; i++) ns++; if (ns > 1) { g_string_append(src, " ("); for (int i = 0; ORABLOCK_SRC[i]; i++) g_string_append_printf(src, "%s%s", i ? ", " : "", ORABLOCK_SRC[i]); g_string_append_c(src, ')'); }
	char *t = g_strdup_printf("Rudolf OS's own list blocks %d ad and tracker services%s. Everything is checked on this computer: nothing about the websites you visit is sent anywhere. Private and Tor windows always block. Pages loaded again use the change.", ORABLOCK_N, src->str);
	p_msg_m(t, 0, 10, 13); g_free(t); g_string_free(src, TRUE);
	p_bold("Sites where blocking is off (from the shield)", 8); sites("off", "o", 500); p_bold("Sites you block too", 8); sites("mine", "m", 2000);
	Add *a = g_new0(Add, 1); a->e = rui_entry("a site, such as ads.example.com"); a->b = rui_button("Block", 0); g_signal_connect(a->e, "activate", G_CALLBACK(on_obadd), NULL); g_signal_connect(a->b, "clicked", G_CALLBACK(on_obadd), NULL);
	p_put(a->e); p_put(a->b); OBIN = a->e; p_label_named("obin", a->e); p_label_named("obadd", a->b); p_custom(add_lay, a, 6, 0); if (OBMSG) p_msg(OBMSG, 0);
}

void page_priv(void)
{
	p_h3("Privacy"); p_link("Rudolf OS keeps your data on this device. Apps must ask before using the camera, microphone, location, clipboard or your files. ", "Read the privacy statement", 10, 13, "pl");
	camera_sec(); if (ACCT[0]) keyring_sec(); perms_sec(); oblock_sec();
}
