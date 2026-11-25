/*
 * rudolf-settings: Bluetooth (bt51.js btPanel). 1.6.3, docs/plan-1.6.3.md Part 3 (PR 6). As the page: rudolfd's GET bt (the adapter,
 * the devices paired or found) drawn as btPanel draws it, asked again every 8 s while nothing is being done; the On switch
 * (bt/power?on=), Search for devices (bt/scan?s=15, the list asked again every 3 s while it searches), Let this computer be found
 * (bt/visible), and each device's Pair, Connect, Disconnect and Forget (bt/pair|connect|disconnect|remove?mac=), every button off
 * while one is at work, with the page's words for what happened. While the page is open and Bluetooth is on, the pairing question
 * (bt/ask, answered with bt/answer?ok=) is asked as the page's btAskBox asks it. GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings.h"

static RuiJ *BT;                 /* the last status (NULL: none, as the page's btStatus) */
static char *BTJ;                /* (its words: drawn again only when they change) */
static int BUSY, SEARCHING;
static char *SAY; static int SAY_OK;   /* .btmsg */
static GtkWidget *ALIVE;         /* (the page still open: a part of it, weakly held) */
static guint TICK, ASKT, SCANT;
static int KEEPST;               /* (drawn again with the status kept: not asked for again) */
static char *ASKED;              /* the question being asked (its t) */

static int btshow(void) { return ALIVE != NULL; }
static RuiJ *bt_get(void) { return rui_get("/api/bt"); }
static char *jtext(RuiJ *j)
{
	/* (the status's words, to see whether anything changed) */
	GString *b = g_string_new(NULL); if (!j) return g_string_free(b, FALSE);
	g_string_append_printf(b, "%d%d%d%d%s|%s|", rui_jtype(rui_jprop(j, "adapter")) == 3, rui_jbool(j, "powered"), rui_jbool(j, "discovering"), rui_jbool(j, "running"), rui_jbool(j, "visible") ? "v" : "", set_jstr(j, "name", ""));
	RuiJ *d = rui_jprop(j, "devices");
	for (int i = 0; i < rui_jlen(d); i++) { RuiJ *x = rui_jat(d, i); g_string_append_printf(b, "%s,%s,%s,%d%d%d,%g,%g;", set_jstr(x, "mac", ""), set_jstr(x, "name", ""), set_jstr(x, "icon", ""),
		rui_jbool(x, "paired"), rui_jbool(x, "connected"), rui_jbool(x, "audio"), rui_jnum(x, "battery", -1), rui_jnum(x, "rssi", -999)); }
	return g_string_free(b, FALSE);
}
static void redraw(void)
{
	/* (the page's draw(): the page built again where it is) */
	if (!btshow()) return; GtkAdjustment *a = NULL; GtkWidget *p = gtk_widget_get_parent(ALIVE);
	while (p && !GTK_IS_SCROLLED_WINDOW(p)) p = gtk_widget_get_parent(p);
	double v = 0; if (p) { a = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(p)); v = gtk_adjustment_get_value(a); }
	KEEPST = 1; set_show("bt"); KEEPST = 0;
	if (ALIVE && a) { p = gtk_widget_get_parent(ALIVE); while (p && !GTK_IS_SCROLLED_WINDOW(p)) p = gtk_widget_get_parent(p); if (p) gtk_adjustment_set_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(p)), v); }
}
static void take(RuiJ *s)
{
	/* (a status from rudolfd: kept, the page drawn again when it changed) */
	char *t = jtext(s); int same = BTJ && !strcmp(t, BTJ) && (BT != NULL) == (s != NULL);
	rui_jfree(BT); BT = s; g_free(BTJ); BTJ = t; if (!same) redraw(); else set_state_soon();
}
static void say(const char *t, int ok) { g_free(SAY); SAY = g_strdup(t); SAY_OK = ok; set_note("btmsg %s", t); }

/* ---- the kind of a device (BT_KIND) and its name (btName) */
static void kind(RuiJ *d, const char **ic, const char **k)
{
	const char *i = set_jstr(d, "icon", "");
	if (strstr(i, "input-gaming")) { *ic = "🎮"; *k = "controller"; }
	else if (strstr(i, "input-keyboard")) { *ic = "⌨️"; *k = "keyboard"; }
	else if (strstr(i, "input-mouse") || strstr(i, "input-tablet")) { *ic = "🖱️"; *k = "mouse"; }
	else if (strstr(i, "audio-head") || strstr(i, "audio-card") || strstr(i, "audio-speaker") || rui_jbool(d, "audio")) { *ic = "🎧"; *k = "headphones or speaker"; }
	else if (strstr(i, "phone")) { *ic = "📱"; *k = "phone"; }
	else if (strstr(i, "computer")) { *ic = "💻"; *k = "computer"; }
	else { *ic = "🔵"; *k = "device"; }
}
static char *bt_name(RuiJ *d)
{
	const char *n = set_jstr(d, "name", ""); int mac = strlen(n) == 17;
	for (int i = 0; mac && i < 17; i++) mac = (i % 3 == 2) ? (n[i] == '-' || n[i] == ':') : g_ascii_isxdigit(n[i]) && !g_ascii_islower(n[i]);
	return g_strdup(*n && !mac ? n : rui_tr("Unnamed device"));
}

/* ---- what a device's button does (the page's act): rudolfd asked in a thread of its own, every button off meanwhile */
typedef struct { char what[16], mac[24], ok[200], nm[120]; int code; char err[300]; } Act;
static gboolean act_done(gpointer p)
{
	Act *a = p; BUSY = 0;
	if (a->code == 200) say(a->ok, 1);
	else { char b[600]; snprintf(b, sizeof b, "%s: %s. Make sure it is on, near the computer, and (to pair) in pairing mode.", a->nm, *a->err ? a->err : "it did not work"); say(b, 0); }
	/* (as the page: its draw() asks for the status again) */
	{ RuiJ *s = bt_get(); rui_jfree(BT); BT = s; g_free(BTJ); BTJ = jtext(s); }
	redraw(); g_free(a); return G_SOURCE_REMOVE;
}
static gpointer act_thread(gpointer p)
{
	Act *a = p; char path[96], *out = NULL;
	/* (each path written out: the confinement lists exactly these) */
	if (!strcmp(a->what, "pair")) snprintf(path, sizeof path, "/api/bt/pair?mac=%s", a->mac);
	else if (!strcmp(a->what, "connect")) snprintf(path, sizeof path, "/api/bt/connect?mac=%s", a->mac);
	else if (!strcmp(a->what, "disconnect")) snprintf(path, sizeof path, "/api/bt/disconnect?mac=%s", a->mac);
	else snprintf(path, sizeof path, "/api/bt/remove?mac=%s", a->mac);
	a->code = rui_http("POST", path, NULL, 0, &out, NULL); RuiJ *j = out ? rui_jparse(out) : NULL;
	if (a->code != 200) snprintf(a->err, sizeof a->err, "%s", set_jstr(j, "error", a->code ? "" : "rudolfd did not answer"));
	rui_jfree(j); g_free(out); g_idle_add(act_done, a); return NULL;
}
static gboolean ask_now(gpointer d);
static gboolean ask_soon(gpointer d) { (void)d; ask_now(NULL); return G_SOURCE_REMOVE; }
static void act(RuiJ *d, const char *what, const char *fmt)
{
	if (BUSY) return; BUSY = 1; Act *a = g_new0(Act, 1); snprintf(a->what, sizeof a->what, "%s", what); snprintf(a->mac, sizeof a->mac, "%s", set_jstr(d, "mac", ""));
	char *n = bt_name(d); snprintf(a->nm, sizeof a->nm, "%s", n); snprintf(a->ok, sizeof a->ok, fmt, n);
	if (!strcmp(what, "pair")) { char b[300]; snprintf(b, sizeof b, "Pairing with %s… keep it in pairing mode. A phone shows a code: check it matches the one shown here.", n); say(b, 1); g_timeout_add(1500, ask_soon, NULL); }
	else say("Working…", 1);
	g_free(n); set_note("bt %s %s", what, a->mac); redraw(); g_thread_unref(g_thread_new("bt", act_thread, a));
}
typedef struct { char mac[24]; int what; } Btn;   /* what: 0 pair 1 connect 2 disconnect 3 forget */
static RuiJ *dev_of(const char *mac) { RuiJ *d = rui_jprop(BT, "devices"); for (int i = 0; i < rui_jlen(d); i++) if (!strcmp(set_jstr(rui_jat(d, i), "mac", ""), mac)) return rui_jat(d, i); return NULL; }
static void on_btn(GtkButton *b, gpointer p)
{
	(void)b; Btn *x = p; RuiJ *d = dev_of(x->mac); if (!d) return;
	static const char *W[] = { "pair", "connect", "disconnect", "remove" };
	static const char *OK[] = { "%s is paired and connected.", "%s is connected.", "%s is disconnected.", "%s is forgotten. Pair it again to use it." };
	act(d, W[x->what], OK[x->what]);
}

/* ---- a device's row (the page's .acr, made a flex row: its icon 20px, its name in bold and what it is under it, its buttons) */
typedef struct { GtkWidget *area, *b[2]; int nb; char *ic, *nm, *sub; } Row;
static double row_lay(void *d, double x, double y, double w)
{
	Row *r = d; double bh = r->nb ? p_nat_h(r->b[0]) : 0, ch = 30, h = MAX(MAX(ch, 24), bh) + 8 + 1, bx = x + w;
	for (int i = r->nb; i-- > 0;) { double bw = p_nat_w(r->b[i]); bx -= bw; p_place(r->b[i], bx, y + 4 + (h - 9 - bh) / 2, 0, 0); bx -= 8; }
	p_place(r->area, x, y, w, h); return h;
}
static gboolean row_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Row *r = d; double z = s_z(), W = gtk_widget_get_allocated_width(w) / z, H = gtk_widget_get_allocated_height(w) / z, in = H - 9;
	s_col(cr, "fg", 1); s_text(cr, r->ic, 0, 4 + (in - 24) / 2 + .5, 20, 400, 0, 24);
	double ty = 4 + (in - 30) / 2; s_text(cr, r->nm, 33, ty, 13, 700, W - 33, s_lh(13, 700));
	s_col(cr, "grey", 1); s_text(cr, r->sub, 33, ty + 17, 10.8333, 400, W - 33, s_lh(10.8333, 400));
	cairo_save(cr); cairo_scale(cr, z, z); s_col(cr, "acr", 1); cairo_rectangle(cr, 0, H - 1, W, 1); cairo_fill(cr); cairo_restore(cr); return TRUE;
}
static void row_free(gpointer p) { Row *r = p; g_free(r->ic); g_free(r->nm); g_free(r->sub); g_free(r); }
static void dev_row(RuiJ *d, int paired, int n)
{
	Row *r = g_new0(Row, 1); const char *ic, *k; kind(d, &ic, &k); r->ic = g_strdup(ic); r->nm = bt_name(d);
	GString *s = g_string_new(rui_tr(k)); int con = rui_jbool(d, "connected");
	if (con || rui_jbool(d, "paired")) g_string_append_printf(s, " · %s", rui_tr(con ? "connected" : "not connected"));
	if (rui_jtype(rui_jprop(d, "battery")) == 2) g_string_append_printf(s, " · %s %g%%", rui_tr("battery"), rui_jnum(d, "battery", 0));
	r->sub = g_string_free(s, FALSE);
	r->area = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(r->area), "row", r, row_free); g_signal_connect(r->area, "draw", G_CALLBACK(row_draw), r); p_put(r->area);
	struct { const char *l; int what, pri; } B[2]; int nb = 0;
	if (!paired) B[nb++] = (typeof(B[0])){ "Pair", 0, 1 };
	else { if (con) B[nb++] = (typeof(B[0])){ "Disconnect", 2, 0 }; else B[nb++] = (typeof(B[0])){ "Connect", 1, 1 }; B[nb++] = (typeof(B[0])){ "Forget", 3, 0 }; }
	for (int i = 0; i < nb; i++) { Btn *x = g_new0(Btn, 1); snprintf(x->mac, sizeof x->mac, "%s", set_jstr(d, "mac", "")); x->what = B[i].what;
		GtkWidget *b = rui_button(B[i].l, B[i].pri); g_object_set_data_full(G_OBJECT(b), "btn", x, g_free); g_signal_connect(b, "clicked", G_CALLBACK(on_btn), x);
		if (BUSY) gtk_widget_set_sensitive(b, FALSE); p_put(b); r->b[r->nb++] = b;
		char nm[32]; snprintf(nm, sizeof nm, "bt%s%d.%d", paired ? "m" : "n", n, i); p_label_named(nm, b); }
	p_custom(row_lay, r, 0, 0);
}

/* ---- a .rmrow: a button and the words after it (a span of 12px, #6b7280) */
typedef struct { GtkWidget *b, *t; } Rm;
static double rm_lay(void *d, double x, double y, double w) { Rm *r = d; double bw = p_nat_w(r->b), bh = p_nat_h(r->b); p_place(r->b, x, y, 0, 0); p_place(r->t, x + bw + 8, y, MAX(1, w - bw - 8), bh); return bh; }
static gboolean rm_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)d; const char *t = g_object_get_data(G_OBJECT(w), "t"); if (!t || !*t) return TRUE; s_col(cr, "grey", 1); s_text(cr, t, 0, 0, 12, 400, gtk_widget_get_allocated_width(w) / s_z(), s_lh(12, 400)); return TRUE; }
static GtkWidget *rmrow(const char *label, GCallback cb, int on, const char *words, const char *name)
{
	Rm *r = g_new0(Rm, 1); r->b = rui_button(label, 0); g_signal_connect(r->b, "clicked", cb, NULL); gtk_widget_set_sensitive(r->b, on && !BUSY); p_put(r->b); p_label_named(name, r->b);
	r->t = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(r->t), "t", g_strdup(words ? words : ""), g_free); g_object_set_data_full(G_OBJECT(r->t), "rm", r, g_free); g_signal_connect(r->t, "draw", G_CALLBACK(rm_draw), NULL); p_put(r->t);
	p_custom(rm_lay, r, 14, 0); return r->b;
}

/* ---- the switch, Search for devices, Let this computer be found */
typedef struct { int code; char err[300]; RuiJ *st; int what; } Bg;   /* what: 0 power 1 scan 2 visible */
static gboolean bg_done(gpointer p)
{
	Bg *g = p; if (g->what == 1) { SEARCHING = 0; BUSY = 0; if (SCANT) { g_source_remove(SCANT); SCANT = 0; } }
	if (g->code != 200) say(*g->err ? g->err : g->what == 0 ? "Bluetooth could not be changed." : g->what == 1 ? "The search did not work." : "It did not work.", 0);
	RuiJ *s = g->code == 200 && g->st ? g->st : bt_get(); if (s != g->st) rui_jfree(g->st);
	rui_jfree(BT); BT = s; g_free(BTJ); BTJ = jtext(s); redraw(); g_free(g); return G_SOURCE_REMOVE;
}
static gpointer bg_thread(gpointer p)
{
	Bg *g = p; char *out = NULL; const char *path = g->what == 1 ? "/api/bt/scan?s=15" : g->what == 2 ? "/api/bt/visible" : NULL; char pw[40];
	if (!path) { snprintf(pw, sizeof pw, "/api/bt/power?on=%d", g->code); path = pw; }
	g->code = rui_http("POST", path, NULL, 0, &out, NULL); RuiJ *j = out ? rui_jparse(out) : NULL;
	if (g->code == 200) g->st = j; else { snprintf(g->err, sizeof g->err, "%s", set_jstr(j, "error", "")); rui_jfree(j); }
	g_free(out); g_idle_add(bg_done, g); return NULL;
}
static void on_power(int on, void *d) { (void)d; if (!BT) return; Bg *g = g_new0(Bg, 1); g->what = 0; g->code = on; set_note("bt power %d", on); g_thread_unref(g_thread_new("bt", bg_thread, g)); }
static void on_visible(GtkButton *b, gpointer d) { (void)d; if (BUSY) return; gtk_widget_set_sensitive(GTK_WIDGET(b), FALSE); Bg *g = g_new0(Bg, 1); g->what = 2; set_note("bt visible"); g_thread_unref(g_thread_new("bt", bg_thread, g)); }
static gboolean scan_tick(gpointer d) { (void)d; if (!SEARCHING) return G_SOURCE_REMOVE; RuiJ *s = bt_get(); if (s && btshow()) { /* (as the page: drawn, but busy) */ take(s); } else rui_jfree(s); return G_SOURCE_CONTINUE; }
static void on_search(GtkButton *b, gpointer d)
{
	(void)b; (void)d; if (BUSY) return; BUSY = 1; SEARCHING = 1; set_note("bt scan");
	Bg *g = g_new0(Bg, 1); g->what = 1; SCANT = g_timeout_add(3000, scan_tick, NULL); redraw(); g_thread_unref(g_thread_new("bt", bg_thread, g));
}

/* ---- the pairing question (btAskBox): asked every 2 s while Bluetooth is on and the page is open */
static void ask_answer(int i, void *d)
{
	(void)d; const char *k = ASKED ? ASKED : ""; (void)k; if (i < 0) return;
	char *out = NULL; rui_http("POST", i == 1 ? "/api/bt/answer?ok=1" : "/api/bt/answer?ok=0", NULL, 0, &out, NULL); g_free(out); set_note("bt answer %d", i == 1);
}
static gboolean ask_now(gpointer d)
{
	(void)d; if (!btshow()) { ASKT = 0; return G_SOURCE_REMOVE; } if (!BT || !rui_jbool(BT, "powered")) return G_SOURCE_CONTINUE;
	RuiJ *r = rui_get("/api/bt/ask"); RuiJ *a = rui_jprop(r, "ask");
	if (!a || rui_jtype(a) != 5) { g_free(ASKED); ASKED = NULL; rui_jfree(r); return G_SOURCE_CONTINUE; }
	char t[64]; snprintf(t, sizeof t, "%g", rui_jnum(a, "t", 0)); if (ASKED && !strcmp(ASKED, t)) { rui_jfree(r); return G_SOURCE_CONTINUE; }
	g_free(ASKED); ASKED = g_strdup(t);
	const char *kd = set_jstr(a, "kind", ""), *nm = set_jstr(a, "name", ""), *code = set_jstr(a, "code", ""); char b[400];
	if (!strcmp(kd, "confirm")) { snprintf(b, sizeof b, "Pair with %s? Choose Yes only if it shows the same code:\n\n%s", *nm ? nm : "this device", code); const char *const B[2] = { "No", "Yes, pair" }; rui_confirm(SW, "🔵 Bluetooth pairing", b, B, 2, 1, ask_answer, NULL); set_note("ask bt confirm %s", code); }
	else if (!strcmp(kd, "accept")) { snprintf(b, sizeof b, "%s wants to pair with this computer.", *nm ? nm : "A Bluetooth device"); const char *const B[2] = { "Don't pair", "Pair" }; rui_confirm(SW, "🔵 Bluetooth pairing", b, B, 2, 1, ask_answer, NULL); set_note("ask bt accept"); }
	else if (!strcmp(kd, "keyboard")) { snprintf(b, sizeof b, "Type this code on %s, then press Enter:\n\n%s", *nm ? nm : "the keyboard", code); const char *const B[1] = { "OK" }; rui_confirm(SW, "🔵 Bluetooth pairing", b, B, 1, 0, NULL, NULL); set_note("ask bt keyboard %s", code); }
	rui_jfree(r); return G_SOURCE_CONTINUE;
}
static gboolean tick(gpointer d)
{
	/* (the page's: drawn again every 8 s while nothing is being done) */
	(void)d; if (!btshow()) { TICK = 0; return G_SOURCE_REMOVE; } if (!BUSY) take(bt_get()); return G_SOURCE_CONTINUE;
}

void page_bt(void)
{
	if (!KEEPST) { rui_jfree(BT); BT = bt_get(); g_free(BTJ); BTJ = jtext(BT); }
	p_h3("Bluetooth");
	p_msg_m("\x03" "Controllers, keyboards, mice and headphones. To pair one, put it in pairing mode (usually by holding its pairing or Share/Sync button until its light flashes), then choose <b>Search for devices</b>.", 0, 0, 10);
	RuiJ *s = BT; int ad = s && rui_jtype(rui_jprop(s, "adapter")) == 3 && *set_jstr(s, "adapter", ""), pw = ad && rui_jbool(s, "powered");
	{ Cell *c = p_r2("Bluetooth"); GtkWidget *cb = c_check(c, "On", pw, on_power, NULL); p_label_named("bto", cb); if (!ad) gtk_widget_set_sensitive(cb, FALSE);
		const char *st = !s ? "Bluetooth needs the installed Rudolf OS." : !ad ? "This computer has no Bluetooth adapter Rudolf OS can use. A USB Bluetooth adapter works too." :
			pw ? (rui_jbool(s, "discovering") ? "On · searching…" : "On") : rui_jbool(s, "running") ? "Off" : "Starting…";
		c_msg_m(c, st, 4, 1, 1); }   /* (span.bts: on the line under label.ck, 12px in a 13px line box) */
	p_h4("My devices");
	RuiJ *dv = rui_jprop(s, "devices"); int np = 0, nn = 0;
	if (!ad) p_msg_m("\x03<i>None.</i>", 1, 0, 0);
	else { for (int i = 0; i < rui_jlen(dv); i++) if (rui_jbool(rui_jat(dv, i), "paired")) dev_row(rui_jat(dv, i), 1, np++);
		if (!np) p_msg_m("\x03<i>No devices paired yet.</i>", 1, 0, 0); }
	p_h4("Other devices");
	rmrow("Search for devices", G_CALLBACK(on_search), ad && pw, SEARCHING ? "Searching for 15 seconds…" : "", "btf");
	{ char vis[200] = ""; if (pw && rui_jbool(s, "visible")) snprintf(vis, sizeof vis, "Visible as “%s” for a few minutes.", set_jstr(s, "name", "Rudolf OS"));
		rmrow("Let this computer be found", G_CALLBACK(on_visible), ad && pw, vis, "btv"); }
	p_msg_m("To pair from a phone, choose this, then look for this computer in the phone's Bluetooth settings.", 2, 4, 8);
	if (ad) {
		/* (the others, the nearest first) */
		int n = rui_jlen(dv), *o = g_new0(int, n + 1), k = 0; for (int i = 0; i < n; i++) if (!rui_jbool(rui_jat(dv, i), "paired")) o[k++] = i;
		for (int i = 1; i < k; i++) for (int j = i; j > 0 && rui_jnum(rui_jat(dv, o[j]), "rssi", -999) > rui_jnum(rui_jat(dv, o[j - 1]), "rssi", -999); j--) { int t = o[j]; o[j] = o[j - 1]; o[j - 1] = t; }
		for (int i = 0; i < k; i++) dev_row(rui_jat(dv, o[i]), 0, nn++);
		g_free(o);
		if (!nn) p_msg_m(rui_jbool(s, "discovering") ? "\x03<i>Searching…</i>" : "\x03<i>Choose Search for devices to find one.</i>", 1, 0, 0);
	}
	{ char *e = g_markup_escape_text(SAY ? rui_tr(SAY) : "", -1), *m = g_strdup_printf("\x03<span foreground=\"%s\">%s</span>", SAY_OK ? "#15803d" : "#b91c1c", e);
		GtkWidget *w = p_msg_m(SAY && *SAY ? m : "", 0, 10, 0); p_label_named("btmsg", w); g_free(e); g_free(m);
		if (ALIVE) g_object_remove_weak_pointer(G_OBJECT(ALIVE), (gpointer *)&ALIVE); ALIVE = w; g_object_add_weak_pointer(G_OBJECT(w), (gpointer *)&ALIVE); }
	if (!TICK) TICK = g_timeout_add(8000, tick, NULL);
	if (!ASKT) ASKT = g_timeout_add(2000, ask_now, NULL);
	set_note("bt %s %d %d", ad ? "adapter" : "none", np, nn);
}
