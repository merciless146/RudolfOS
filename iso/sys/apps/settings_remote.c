/*
 * rudolf-settings: Remote access (remote50.js remotePanel). 1.6.3, docs/plan-1.6.3.md Part 3 (PR 6). As the page: rudolfd's GET remote
 * drawn as remotePanel draws it: the switch (an administrator's, asked with the administrator's prompt as the page's elevate: the
 * accounts handed to rudolf-remote first, remote/accounts, then remote?on=, "Remote access on|off" in the security log); How to
 * connect (the addresses, the pairing code, the certificate's fingerprint, the browsers remembered); Connected now (End the
 * session: remote/end); Unattended access (each account's ru in bd95user, asked with the administrator's prompt, the accounts
 * handed to rudolf-remote again and a note in System health, as the page's). The prompt that allows a connection and the marker
 * while someone is connected are the desktop's (rmPoll), not Settings'. GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings.h"

static RuiJ *RS;   /* the last GET remote (NULL: none) */
static int post(const char *path, const char *body, char *err, size_t n)
{
	char *out = NULL; int c = rui_http("POST", path, body, body ? strlen(body) : 0, &out, NULL); RuiJ *j = out ? rui_jparse(out) : NULL;
	if (err) snprintf(err, n, "%s", set_jstr(j, "error", c ? "" : "rudolfd did not answer")); set_note("POST %s %d", path, c); rui_jfree(j); g_free(out); return c;
}
static char *since_str(double t)
{
	/* (rmFmtSince) */
	long m = lround(fmax(0, (set_now() - t) / 60)); return m < 1 ? g_strdup("just now") : m == 1 ? g_strdup("1 minute") : g_strdup_printf("%ld minutes", m);
}

/* ---- the accounts (the page's ACCTS, bd95user's accts) and rmAccounts: name TAB hash TAB role TAB unattended, a line each */
static RuiJ *users(void) { return set_ls_get("bd95user"); }
static RuiJ *accts(RuiJ *u) { return rui_jprop(rui_jprop(u, "x"), "accts"); }
static char *rm_accounts(void)
{
	RuiJ *u = users(), *a = accts(u); GString *b = g_string_new(NULL);
	for (int i = 0; i < rui_jlen(a); i++) { RuiJ *x = rui_jat(a, i); const char *n = set_jstr(x, "n", ""), *h = set_jstr(x, "h", ""), *r = set_jstr(x, "r", "");
		if (!strcmp(r, "guest") || !*n || !*h) continue; char *nn = g_strdup(n); for (char *p = nn; *p; p++) if (*p == '\t' || *p == '\n') *p = ' ';
		RuiJ *ru = rui_jprop(x, "ru"); int on = ru && (rui_jtype(ru) == 1 || rui_jtype(ru) == 2) ? rui_jval(ru) != 0 : rui_jtype(ru) == 3 && *rui_jsval(ru);
		if (b->len) g_string_append_c(b, '\n'); g_string_append_printf(b, "%s\t%s\t%s\t%d", nn, h, r, on); g_free(nn); }
	g_string_append_c(b, '\n'); rui_jfree(u); return g_string_free(b, FALSE);
}
static void rm_sync(void) { char *b = rm_accounts(); post("/api/remote/accounts", b, NULL, 0); explicit_bzero(b, strlen(b)); g_free(b); }
/* (saveUser: the accounts written again, the one account's ru set, everything else as it was) */
static void set_ru(int idx, int on)
{
	RuiJ *u = users(), *v = rui_jprop(u, "x"), *a = accts(u); if (rui_jtype(v) != 5) { rui_jfree(u); return; }
	GString *b = g_string_new("{"); int n = 0;
	for (int i = 0; rui_jkey(v, i); i++) { const char *k = rui_jkey(v, i); char *e = set_jesc(k); g_string_append_printf(b, "%s\"%s\":", n++ ? "," : "", e); g_free(e);
		if (strcmp(k, "accts")) { set_jw(b, rui_jprop(v, k)); continue; }
		g_string_append_c(b, '[');
		for (int j = 0; j < rui_jlen(a); j++) { RuiJ *x = rui_jat(a, j); if (j) g_string_append_c(b, ','); if (j != idx) { set_jw(b, x); continue; }
			g_string_append_c(b, '{'); int m = 0, had = 0;
			for (int q = 0; rui_jkey(x, q); q++) { const char *kk = rui_jkey(x, q); char *e2 = set_jesc(kk); g_string_append_printf(b, "%s\"%s\":", m++ ? "," : "", e2); g_free(e2);
				if (!strcmp(kk, "ru")) { had = 1; g_string_append(b, on ? "true" : "false"); } else set_jw(b, rui_jprop(x, kk)); }
			if (!had) g_string_append_printf(b, "%s\"ru\":%s", m ? "," : "", on ? "true" : "false");
			g_string_append_c(b, '}'); }
		g_string_append_c(b, ']'); }
	g_string_append_c(b, '}'); set_ls_set("bd95user", b->str); explicit_bzero(b->str, b->len); g_string_free(b, TRUE); rui_jfree(u);
}

/* ---- the page drawn again (the page's draw), where it is */
static gboolean redraw_idle(gpointer d) { (void)d; set_show("remote"); return G_SOURCE_REMOVE; }

/* ---- the switch (an administrator's) */
static void on_go(int ok, void *d)
{
	int on = GPOINTER_TO_INT(d); if (!ok) { set_note("remote %d refused", on); set_show("remote"); return; }
	char e[300] = ""; int c = 200;
	if (on) { char *b = rm_accounts(); c = post("/api/remote/accounts", b, e, sizeof e); explicit_bzero(b, strlen(b)); g_free(b); }
	if (c == 200) c = post(on ? "/api/remote?on=1" : "/api/remote?on=0", NULL, e, sizeof e);
	if (c == 200) set_log_evt("sys", on ? "Remote access on" : "Remote access off");
	else set_notify("Remote access", *e ? e : "It could not be changed.", 1);
	set_show("remote");
}
static void on_switch(int on, void *d) { (void)d; rui_elevate(SW, on ? "Turn on remote access: devices on the local network can then ask to see and control this computer." : "Turn off remote access", on_go, GINT_TO_POINTER(on)); set_note("ask remote %d", on); }
static void on_end(GtkButton *b, gpointer d) { (void)b; (void)d; post("/api/remote/end", NULL, NULL, 0); g_timeout_add(1200, redraw_idle, NULL); }
/* ---- an account's unattended access */
typedef struct { int idx, on; char *n; } Ru;
static void ru_go(int ok, void *d)
{
	Ru *r = d; if (ok) { set_ru(r->idx, r->on); rm_sync();
		char *m = g_strdup_printf("{\"k\":\"remote\",\"m\":\"unattended access turned %s for an account\"}", r->on ? "on" : "off"); post("/api/health", m, NULL, 0); g_free(m); }
	set_note("remote ru %s %d %s", r->n, r->on, ok ? "ok" : "refused"); g_free(r->n); g_free(r); set_show("remote");
}
static void on_ru(int on, void *d)
{
	int idx = GPOINTER_TO_INT(d); RuiJ *u = users(); Ru *r = g_new0(Ru, 1); r->idx = idx; r->on = on; r->n = g_strdup(set_jstr(rui_jat(accts(u), idx), "n", "")); rui_jfree(u);
	char *why = on ? g_strdup_printf("Let %s connect remotely without anyone allowing it", r->n) : g_strdup_printf("Stop %s connecting remotely without anyone allowing it", r->n);
	rui_elevate(SW, why, ru_go, r); set_note("ask remote ru %s %d", r->n, on); g_free(why);
}

/* ---- blocks of the page's own: a line of monospace (the addresses, the pairing code, the fingerprint: word-break break-all) */
typedef struct { char *t; double px, lh, dy, ls; int wt; GtkWidget *a; } Mono;
static PangoLayout *mono_lay(GtkWidget *w, Mono *m, const char *t)
{
	PangoLayout *l = gtk_widget_create_pango_layout(w, t); PangoFontDescription *f = pango_font_description_from_string("DejaVu Sans Mono");
	pango_font_description_set_absolute_size(f, m->px * s_z() * PANGO_SCALE); pango_font_description_set_weight(f, m->wt); pango_layout_set_font_description(l, f); pango_font_description_free(f);
	if (m->ls) { PangoAttrList *al = pango_attr_list_new(); pango_attr_list_insert(al, pango_attr_letter_spacing_new((int)(m->ls * s_z() * PANGO_SCALE))); pango_layout_set_attributes(l, al); pango_attr_list_unref(al); }
	return l;
}
/* (word-break: break-all: as many characters on a line as fit, the page's monospace advance each) */
static GPtrArray *mono_lines(Mono *m, double width)
{
	GPtrArray *a = g_ptr_array_new_with_free_func(g_free); PangoLayout *l = mono_lay(m->a, m, "0"); int pw, ph; pango_layout_get_size(l, &pw, &ph); g_object_unref(l);
	double cw = pw / (double)PANGO_SCALE / s_z() + m->ls; long n = width > 0 && cw > 0 ? (long)floor((width + .01) / cw) : 0; if (n < 1) n = 1;
	const char *p = m->t; while (*p) { const char *e = p; long k = 0; while (*e && k < n) { e = g_utf8_next_char(e); k++; } g_ptr_array_add(a, g_strndup(p, e - p)); p = e; }
	if (!a->len) g_ptr_array_add(a, g_strdup("")); return a;
}
static double mono_h(Mono *m, double w) { GPtrArray *a = mono_lines(m, w); double h = a->len * m->lh; g_ptr_array_unref(a); return h; }
static double mono_place(void *d, double x, double y, double w) { Mono *m = d; double h = mono_h(m, w); p_place(m->a, x, y, w, h); return h; }
static gboolean mono_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Mono *m = d; double z = s_z(), W = gtk_widget_get_allocated_width(w) / z; GPtrArray *a = mono_lines(m, W); s_col(cr, m->px < 12 ? "grey" : "fg", 1);
	for (guint i = 0; i < a->len; i++) { PangoLayout *l = mono_lay(w, m, g_ptr_array_index(a, i)); PangoRectangle lr; pango_layout_get_extents(l, NULL, &lr); double hh = lr.height / (double)PANGO_SCALE / z;
		cairo_move_to(cr, 0, (i * m->lh + (m->lh - hh) / 2 + m->dy) * z); pango_cairo_show_layout(cr, l); g_object_unref(l); }
	g_ptr_array_unref(a); return TRUE;
}
static void mono_free(gpointer p) { Mono *m = p; g_free(m->t); g_free(m); }
static void mono(const char *t, double px, int wt, double lh, double dy, double mt, double mb, double ls)
{
	Mono *m = g_new0(Mono, 1); m->t = g_strdup(t); m->px = px; m->wt = wt; m->lh = lh; m->dy = dy; m->ls = ls; m->a = gtk_drawing_area_new();
	g_object_set_data_full(G_OBJECT(m->a), "mono", m, mono_free); g_signal_connect(m->a, "draw", G_CALLBACK(mono_draw), m); p_put(m->a); p_custom(mono_place, m, mt, mb);
}
/* (Connected now: the session's words, then its End button in the same line) */
typedef struct { GtkWidget *a, *b; char *t; } Sess;
static double sess_lay(void *d, double x, double y, double w)
{
	Sess *s = d; double tw = 0, sp = 0, a1 = 0; s_lines(s->t, 13, 400, 0, &tw); s_lines("a a", 13, 400, 0, &sp); s_lines("aa", 13, 400, 0, &a1);
	double bh = p_nat_h(s->b); p_place(s->a, x, y, MIN(w, tw), bh); p_place(s->b, x + tw + (sp - a1), y, 0, 0); return bh;
}
static gboolean sess_draw(GtkWidget *w, cairo_t *cr, gpointer d) { Sess *s = d; s_col(cr, "fg", 1); s_text(cr, s->t, 0, 4, 13, 400, 0, s_lh(13, 400)); (void)w; return TRUE; }
static void sess_free(gpointer p) { Sess *s = p; g_free(s->t); g_free(s); }
static double lock_lay(void *d, double x, double y, double w) { const char *t = g_object_get_data(G_OBJECT(d), "t"); double h = 18 + 2 + s_lines(t, 13, 400, w - 22, NULL) * s_lh(13, 400); p_place(d, x, y, w, h); return h; }
static gboolean lock_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)d; const char *t = g_object_get_data(G_OBJECT(w), "t"); double z = s_z(), W = gtk_widget_get_allocated_width(w) / z, H = gtk_widget_get_allocated_height(w) / z; cairo_save(cr); cairo_scale(cr, z, z);
	cairo_set_source_rgb(cr, 1, .984, .922); s_rrect(cr, 0, 0, W, H, 8); cairo_fill(cr); cairo_set_source_rgb(cr, .992, .902, .541); cairo_set_line_width(cr, 1); s_rrect(cr, .5, .5, W - 1, H - 1, 7.5); cairo_stroke(cr); cairo_restore(cr);
	cairo_set_source_rgb(cr, .573, .251, .055); s_text(cr, t, 11, 9, 13, 400, W - 22, 0); return TRUE;
}

/* (an account's label.ck, display:block: its line 20.5 px, the box and the words as an inline line puts them) */
static double ck_lay(void *d, double x, double y, double w) { (void)w; p_place(d, x, y + .75, 0, 0); return 20.5; }

void page_remote(void)
{
	rui_jfree(RS); RS = rui_get("/api/remote"); RuiJ *s = RS; int adm = set_admin();
	p_h3("Remote access");
	p_msg_m("See and use this computer from a phone, tablet or another computer on the same network, in any web browser. Nothing to install there. Someone at this computer is asked to allow each connection, unless the account may connect unattended.", 0, 0, 10);
	int on = s && rui_jbool(s, "on");
	{ Cell *c = p_r2("Remote access"); GtkWidget *cb = c_check(c, "Allow remote access to this computer", on, on_switch, NULL); p_label_named("rmo", cb); if (!adm || !s) gtk_widget_set_sensitive(cb, FALSE);
		const char *st = !s ? "Remote access needs the installed Rudolf OS (bootable disc or USB)." : rui_jbool(s, "live") ? "needs Rudolf OS installed, or a USB drive that keeps files" : on ? (rui_jbool(s, "running") ? "On" : "Starting…") : "Off";
		c_msg_m(c, st, 4, 1, 1); }
	if (on) {
		p_h4("How to connect");
		p_msg_m("On the other device, open this address in a web browser:", 0, 0, 6);
		RuiJ *ad = rui_jprop(s, "addresses"); int port = (int)rui_jnum(s, "port", 8443);
		for (int i = 0; i < rui_jlen(ad); i++) { char *a = g_strdup_printf("https://%s:%d", rui_jsval(rui_jat(ad, i)), port); mono(a, 16, 600, 19, 0, 2, 2, 0); g_free(a); }
		if (!rui_jlen(ad)) p_msg_m("\x03<i>This computer has no network address yet. Connect it to a network first.</i>", 0, 0, 0);
		p_msg_m("Pairing code (asked once in each browser):", 0, 8, 4);
		{ const char *code = set_jstr(s, "code", ""); mono(*code ? code : "—", 28, 700, 33.6, 0, 0, 0, 4);   /* (letter-spacing: 4px) */ }
		p_msg_m("The browser warns that the connection's certificate is not known: it is this computer's own. To be sure it is this computer, compare the certificate's SHA-256 fingerprint in the browser with this one:", 2, 8, 0);
		{ const char *fp = set_jstr(s, "fingerprint", ""); mono(*fp ? fp : "(made when remote access first starts)", 11, 400, 14, 0, 0, 12, 0); }
		{ int nd = (int)rui_jnum(s, "devices", 0); char *t = g_strdup_printf("Only devices on the local network can connect (the firewall allows private addresses only). %d browser%s remembered; turning remote access off forgets them.", nd, nd == 1 ? "" : "s"); p_msg_m(t, 2, 10, 12); g_free(t); }
		p_h4("Connected now");
		RuiJ *ss = rui_jprop(s, "session");
		if (ss && rui_jtype(ss) == 5) { Sess *x = g_new0(Sess, 1); char *f = since_str(rui_jnum(ss, "since", 0)); x->t = g_strdup_printf("%s from %s, for %s.", set_jstr(ss, "user", ""), set_jstr(ss, "from", ""), f); g_free(f);
			x->a = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(x->a), "sess", x, sess_free); g_signal_connect(x->a, "draw", G_CALLBACK(sess_draw), x); p_put(x->a);
			x->b = rui_button("End the session", 0); g_signal_connect(x->b, "clicked", G_CALLBACK(on_end), NULL); p_put(x->b); p_label_named("rme", x->b); p_custom(sess_lay, x, 0, 0); }
		else p_msg_m("\x03<i>Nobody.</i>", 1, 0, 0);
		p_h4("Unattended access");
		p_msg_m("\x03" "These accounts can connect <b>without</b> anyone at the computer allowing it, so the computer can be reached when nobody is there. Every connection is in System health.", 0, 0, 6);
		RuiJ *u = users(), *a = accts(u);
		for (int i = 0; i < rui_jlen(a); i++) { RuiJ *x = rui_jat(a, i); const char *r = set_jstr(x, "r", ""); if (!strcmp(r, "guest")) continue;
			char *t = g_strdup_printf("%s%s", set_jstr(x, "n", ""), !strcmp(r, "admin") ? rui_tr(" (administrator)") : ""); RuiJ *ru = rui_jprop(x, "ru");
			GtkWidget *ck = rui_check(t, ru && rui_jtype(ru) == 1 && rui_jval(ru), on_ru, GINT_TO_POINTER(i)); if (!adm) gtk_widget_set_sensitive(ck, FALSE);
			p_put(ck); p_custom(ck_lay, ck, 0, 0); p_custom_find(t, 0);
			char nm[16]; snprintf(nm, sizeof nm, "rmu%d", i); p_label_named(nm, ck); g_free(t); }
		rui_jfree(u);
	}
	if (!adm) { GtkWidget *a = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(a), "t", g_strdup(rui_tr("🔒 Only administrators can change remote access.")), g_free); g_signal_connect(a, "draw", G_CALLBACK(lock_draw), NULL); p_put(a); p_label_named("lockbar", a); p_custom(lock_lay, a, 0, 10); }
	set_note("remote %s", on ? "on" : s ? "off" : "none");
}
