/*
 * rudolf-settings: Mobile network (mobile60.js mobPanelS, mobModem, mobSettings, mobConn, mobSignal; modem59.js mdmLine). 1.6.3,
 * docs/plan-1.6.3.md Part 3 (PR 6). As the page: rudolfd's mobile/airplane and mobile/status; for each cellular modem its line, its
 * SIM (missing, blocked for good, locked with its PIN or its PUK: unlocked with mobile/pin or mobile/puk, the administrator's name
 * and password in the body as the page's elevate({cred:true}) hands them on), and, with a SIM that works, the connection (Connect,
 * Disconnect: mobile/connect), the signal's details (mobile/signal, every 5 s), and the SIM's settings (mobile/settings: the APN,
 * found by itself or typed in, Connect automatically, this month's data and its limit, roaming, the network type), each kept as
 * the page keeps it (mobile/settings?id=&k=&v=). GPL-2.0-or-later.
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

static RuiJ *ST, *AIR;               /* mobile/status, mobile/airplane */
static GHashTable *SETS;              /* id -> mobile/settings (asked once a page is built) */
static GHashTable *CUSTOM;            /* id -> "Typed in" chosen (the page's .mobcf shown) */
static GtkWidget *ALIVE;              /* (the page still open) */
static guint SIGT;
static char *esc(const char *s) { return g_markup_escape_text(s ? s : "", -1); }
static int v_(const char *x) { return x && *x && strcmp(x, "--"); }
static int KEEP;   /* (drawn again: the choices not applied yet kept) */
static void redraw(void) { KEEP = 1; set_show_keep("mob"); KEEP = 0; }
static int post(const char *path, const char *body, char *msg, size_t n, int ok_message)
{
	char *out = NULL; int c = rui_http("POST", path, body, body ? strlen(body) : 0, &out, NULL); RuiJ *j = out ? rui_jparse(out) : NULL;
	if (msg) snprintf(msg, n, "%s", set_jstr(j, c == 200 && ok_message ? "message" : "error", c ? "" : "rudolfd did not answer"));
	set_note("POST %s %d", path, c); rui_jfree(j); g_free(out); return c;
}

/* ---- the modem's line (mdmLine) */
static const char *mdm_state(const char *s) { for (guint i = 0; i < G_N_ELEMENTS(MDMSTATE); i++) if (!strcmp(MDMSTATE[i][0], s)) return MDMSTATE[i][1]; return *s ? s : "—"; }
static char *mdm_line(RuiJ *m)
{
	GString *b = g_string_new(rui_tr(mdm_state(set_jstr(m, "state", ""))));
	const char *lk = set_jstr(m, "locked", ""), *op = set_jstr(m, "operator", ""), *te = set_jstr(m, "tech", ""), *sg = set_jstr(m, "signal", "");
	if (!rui_jbool(m, "sim") || !strcmp(set_jstr(m, "failed", ""), "sim-missing")) g_string_append_printf(b, " · %s", rui_tr("no SIM card"));
	else if (v_(lk) && strcmp(lk, "none")) g_string_append_printf(b, " · the SIM needs its %s (%s)", !strcmp(lk, "sim-puk") ? "PUK" : "PIN", lk);
	if (v_(op)) g_string_append_printf(b, " · %s", op);
	if (v_(te)) { char *u = g_ascii_strup(te, -1); GString *t = g_string_new(NULL); for (char *p = u; *p; p++) { if (*p == ',') g_string_append(t, ", "); else g_string_append_c(t, *p); } g_string_append_printf(b, " · %s", t->str); g_string_free(t, TRUE); g_free(u); }
	if (v_(sg) && strcmp(sg, "0") && strcmp(set_jstr(m, "state", ""), "locked") && rui_jbool(m, "sim")) g_string_append_printf(b, " · signal %s%%", sg);
	return g_string_free(b, FALSE);
}

/* ---- a check box or a radio button inline in its own line (the page's <label><input> words</label>: not label.ck) */
typedef struct { GtkWidget *a; char *t; int radio, on, sens; double ml; void (*cb)(int on, void *d); void *d; } Ick;
static double ick_lay(void *d, double x, double y, double w) { Ick *k = d; double tw = 0; s_lines(k->t, 13, 400, 0, &tw); double sp, a1; s_lines("a a", 13, 400, 0, &sp); s_lines("aa", 13, 400, 0, &a1);
	p_place(k->a, x, y, MIN(w, k->ml + 13 + 3 + (sp - a1) + tw), 19); return 19; }
static gboolean ick_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Ick *k = d; double sp, a1; s_lines("a a", 13, 400, 0, &sp); s_lines("aa", 13, 400, 0, &a1); (void)w;
	if (k->radio) s_radio(cr, k->ml, 3, k->on, k->sens); else s_ckbox(cr, k->ml, 3, k->on, k->sens);
	s_col(cr, "fg", k->sens ? 1 : .5); s_text(cr, k->t, k->ml + 13 + 3 + (sp - a1), 4, 13, 400, 0, s_lh(13, 400));
	if (gtk_widget_has_focus(w)) { cairo_save(cr); cairo_scale(cr, s_z(), s_z()); s_col(cr, "fg", .6); cairo_set_line_width(cr, 1); cairo_rectangle(cr, k->ml - 1.5, 1.5, 16, 16); cairo_stroke(cr); cairo_restore(cr); }
	return TRUE;
}
static void ick_hit(Ick *k) { if (!k->sens) return; if (k->radio && k->on) return; k->on = k->radio ? 1 : !k->on; gtk_widget_queue_draw(k->a); set_state_soon(); if (k->cb) k->cb(k->on, k->d); }
static gboolean ick_press(GtkWidget *w, GdkEventButton *e, gpointer d) { if (e->button != 1 || e->type != GDK_BUTTON_PRESS) return FALSE; gtk_widget_grab_focus(w); ick_hit(d); return TRUE; }
static gboolean ick_key(GtkWidget *w, GdkEventKey *e, gpointer d) { (void)w; if (e->keyval == GDK_KEY_space) { ick_hit(d); return TRUE; } return FALSE; }
static void ick_free(gpointer p) { Ick *k = p; g_free(k->t); g_free(k); }
static Ick *ick(const char *t, int radio, int on, int sens, void (*cb)(int, void *), void *d, double mt, const char *name)
{
	Ick *k = g_new0(Ick, 1); k->t = g_strdup(rui_tr(t)); k->radio = radio; k->on = on; k->sens = sens; k->ml = radio ? 5 : 4; k->cb = cb; k->d = d; k->a = gtk_drawing_area_new();
	gtk_widget_set_can_focus(k->a, TRUE); gtk_widget_add_events(k->a, GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK);
	g_object_set_data_full(G_OBJECT(k->a), "ick", k, ick_free); g_signal_connect(k->a, "draw", G_CALLBACK(ick_draw), k); g_signal_connect(k->a, "button-press-event", G_CALLBACK(ick_press), k); g_signal_connect(k->a, "key-press-event", G_CALLBACK(ick_key), k);
	g_signal_connect(k->a, "focus-in-event", G_CALLBACK(gtk_widget_queue_draw), NULL); g_signal_connect(k->a, "focus-out-event", G_CALLBACK(gtk_widget_queue_draw), NULL);
	p_put(k->a); if (name) p_label_named(name, k->a); p_custom(ick_lay, k, mt, 0); p_custom_find(k->t, 0); return k;
}
/* (a button alone in its line, as an inline-block after a block: its line as tall as it) */
static double btn_lay(void *d, double x, double y, double w) { (void)w; p_place(d, x, y, 0, 0); return p_nat_h(d); }
static GtkWidget *btn(const char *l, GCallback cb, void *d, const char *name) { GtkWidget *b = rui_button(l, 0); g_signal_connect(b, "clicked", cb, d); p_put(b); if (name) p_label_named(name, b); p_custom(btn_lay, b, 0, 0); return b; }
/* (Data this month's bar: 8 px, rounded, green, amber from 80 %, red at 100 %) */
static double bar_lay(void *d, double x, double y, double w) { p_place(d, x, y, w, 8); return 8; }
static gboolean bar_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	double pct = GPOINTER_TO_INT(d), z = s_z(), W = gtk_widget_get_allocated_width(w) / z; cairo_save(cr); cairo_scale(cr, z, z); s_rrect(cr, 0, 0, W, 8, 4); cairo_clip(cr);
	cairo_set_source_rgb(cr, 0xe5 / 255.0, 0xe7 / 255.0, 0xeb / 255.0); cairo_paint(cr);
	if (pct >= 100) cairo_set_source_rgb(cr, 0xdc / 255.0, 0x26 / 255.0, 0x26 / 255.0); else if (pct >= 80) cairo_set_source_rgb(cr, 0xd9 / 255.0, 0x77 / 255.0, 0x06 / 255.0); else cairo_set_source_rgb(cr, 0x16 / 255.0, 0xa3 / 255.0, 0x4a / 255.0);
	cairo_rectangle(cr, 0, 0, W * MIN(100, pct) / 100, 8); cairo_fill(cr); cairo_restore(cr); return TRUE;
}

/* ---- the SIM's settings (mobSettings' set) */
typedef struct { int id; char k[16]; } Key;
static int set_k(int id, const char *k, const char *v)
{
	char *e = g_uri_escape_string(v, NULL, FALSE), *p = g_strdup_printf("/api/mobile/settings?id=%d&k=%s&v=%s", id, k, e), m[300]; int c = post(p, NULL, m, sizeof m, 0);
	if (c != 200) set_notify("📶 Mobile network", *m ? m : "It could not be changed.", 1); g_free(e); g_free(p); return c == 200;
}
static Key *key(int id, const char *k) { Key *x = g_new0(Key, 1); x->id = id; snprintf(x->k, sizeof x->k, "%s", k); return x; }
static void on_flag(int on, void *d) { Key *x = d; if (!set_k(x->id, x->k, on ? "1" : "0")) redraw(); }
static void on_sel(const char *v, void *d) { Key *x = d; if (!set_k(x->id, x->k, v)) redraw(); }
static void on_auto(int on, void *d)
{
	int id = GPOINTER_TO_INT(d); (void)on; g_hash_table_remove(CUSTOM, GINT_TO_POINTER(id)); RuiJ *s = g_hash_table_lookup(SETS, GINT_TO_POINTER(id)); RuiJ *a = rui_jprop(s, "auto");
	if (set_k(id, "apn", "")) { char *t = g_strdup_printf("The operator's settings are found by themselves again%s%s.", a && rui_jtype(a) == 5 ? ": " : "", a && rui_jtype(a) == 5 ? set_jstr(a, "apn", "") : ""); set_notify("📶 Mobile network", t, 0); g_free(t); }
	redraw();
}
static void on_cust(int on, void *d) { (void)on; g_hash_table_insert(CUSTOM, d, GINT_TO_POINTER(1)); redraw(); }
typedef struct { int id; GtkWidget *apn, *user, *pass; } Apn;
static void on_save(GtkButton *b, gpointer d)
{
	(void)b; Apn *x = d; char *a = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(x->apn)))); int ok = *a && g_ascii_isalnum(*a) && strlen(a) <= 100;
	for (char *p = a; ok && *p; p++) ok = g_ascii_isalnum(*p) || *p == '.' || *p == '_' || *p == '-';
	if (!ok) { set_notify("📶 Mobile network", "An APN has letters, digits, dots and hyphens only.", 1); g_free(a); return; }
	if (set_k(x->id, "apn", a)) { set_k(x->id, "apnuser", gtk_entry_get_text(GTK_ENTRY(x->user))); const char *pw = gtk_entry_get_text(GTK_ENTRY(x->pass)); if (*pw) set_k(x->id, "apnpass", pw);
		gtk_entry_set_text(GTK_ENTRY(x->pass), ""); char *t = g_strdup_printf("Saved: the APN is %s.", a); set_notify("📶 Mobile network", t, 0); g_free(t); redraw(); }
	g_free(a);
}
static void lim_go(GtkWidget *e, Key *x)
{
	if (gtk_widget_in_destruction(e) || !gtk_widget_get_mapped(e) || KEEP) return;   /* (the page built again: its focus leaving is not a change) */
	const char *t = gtk_entry_get_text(GTK_ENTRY(e)), *was = g_object_get_data(G_OBJECT(e), "was"); if (was && !strcmp(t, was)) return;
	double v = g_ascii_strtod(t, NULL); if (!isfinite(v) || v < 0) v = 0; char b[24]; snprintf(b, sizeof b, "%.0f", round(v));
	if (set_k(x->id, "limit", b)) { g_object_set_data_full(G_OBJECT(e), "was", g_strdup(t), g_free); redraw(); } else gtk_entry_set_text(GTK_ENTRY(e), was ? was : "0");
}
static void lim_act(GtkEntry *e, gpointer d) { lim_go(GTK_WIDGET(e), d); }
static gboolean lim_out(GtkWidget *e, GdkEvent *ev, gpointer d) { (void)ev; lim_go(e, d); return FALSE; }

static void sim_settings(int id)
{
	RuiJ *s = g_hash_table_lookup(SETS, GINT_TO_POINTER(id));
	if (!s) { char p[64]; snprintf(p, sizeof p, "/api/mobile/settings?id=%d", id); s = rui_get(p); if (s) g_hash_table_insert(SETS, GINT_TO_POINTER(id), s); }
	if (!s) { p_msg("The SIM's settings could not be read: it did not answer", 2); return; }
	RuiJ *a = rui_jprop(s, "auto"); int has = a && rui_jtype(a) == 5, custom = *set_jstr(s, "apn", "") || !has || g_hash_table_contains(CUSTOM, GINT_TO_POINTER(id));
	char nm[32];
	p_h4("The operator's settings (APN)");
	{ char *t = has ? g_strdup_printf("Automatic: %s (%s, %s)", set_jstr(a, "apn", ""), set_jstr(a, "name", ""), !strcmp(set_jstr(a, "source", ""), "rudolf") ? "corrected by Rudolf OS" : "from the operators' database")
		: g_strdup_printf("Automatic: none is known for this SIM's operator%s%s%s: type it in", *set_jstr(s, "simOp", "") ? " (" : "", set_jstr(s, "simOp", ""), *set_jstr(s, "simOp", "") ? ")" : "");
		snprintf(nm, sizeof nm, "mobauto%d", id); ick(t, 1, !custom, has, on_auto, GINT_TO_POINTER(id), 0, nm); g_free(t); }
	snprintf(nm, sizeof nm, "mobcust%d", id); ick("Typed in", 1, custom, 1, on_cust, GINT_TO_POINTER(id), 0, nm);
	if (custom) { Apn *x = g_new0(Apn, 1); x->id = id;
		Cell *c = p_r2("APN"); x->apn = rui_entry(""); gtk_entry_set_text(GTK_ENTRY(x->apn), set_jstr(s, "apn", "")); c_add(c, x->apn, 0, 0, 1); snprintf(nm, sizeof nm, "mobapn%d", id); p_label_named(nm, x->apn); p_r2_more(c);
		c = p_r2("User name"); x->user = rui_entry(""); gtk_entry_set_text(GTK_ENTRY(x->user), set_jstr(s, "apnuser", "")); c_add(c, x->user, 0, 0, 1); p_r2_more(c);
		c = p_r2("Password"); x->pass = rui_entry(rui_jbool(s, "apnpass") ? "(kept: type to change)" : ""); gtk_entry_set_visibility(GTK_ENTRY(x->pass), FALSE); c_add(c, x->pass, 0, 0, 1);
		p_msg("Type the APN exactly as your operator writes it: some networks refuse it in other capitals. The user name and password are usually empty.", 2);
		GtkWidget *b = btn("Save", G_CALLBACK(on_save), x, NULL); g_object_set_data_full(G_OBJECT(b), "apn", x, g_free); snprintf(nm, sizeof nm, "mobsave%d", id); p_label_named(nm, b); }
	p_h4("Connect automatically");
	{ RuiJ *ac = rui_jprop(s, "autoconnect"); snprintf(nm, sizeof nm, "mobautoc%d", id); Ick *k = ick("Connect to mobile data by itself", 0, !(ac && rui_jtype(ac) == 1 && !rui_jval(ac)), 1, on_flag, key(id, "autoconnect"), 0, nm); g_object_set_data_full(G_OBJECT(k->a), "key", k->d, g_free); }
	p_msg("On the SIM's home network; abroad only when roaming is allowed below. Wi-Fi and cables come first: mobile data stays in reserve and takes over within seconds when they drop.", 2);
	p_h4("Data this month");
	{ RuiJ *u = rui_jprop(s, "usage"); double used = rui_jnum(u, "used", 0), lim = rui_jnum(u, "limit", 0), pct = rui_jnum(u, "pct", 0); char ub[40];
		if (used >= 1e9) snprintf(ub, sizeof ub, "%.2f GB", used / 1e9); else snprintf(ub, sizeof ub, "%.0f MB", round(used / 1e6));
		GString *t = g_string_new(ub); if (lim) g_string_append_printf(t, " of %g MB (%g %%)", lim, pct); g_string_append_printf(t, " since %s", *set_jstr(u, "start", "") ? set_jstr(u, "start", "") : "the 1st");
		if (*set_jstr(u, "next", "")) g_string_append_printf(t, ", until %s", set_jstr(u, "next", "")); if (rui_jbool(u, "off")) g_string_append(t, " · mobile data is off: the limit is reached (Connect to go on: it may cost)");
		p_msg(t->str, 2); g_string_free(t, TRUE);
		if (lim) { GtkWidget *bar = gtk_drawing_area_new(); g_signal_connect(bar, "draw", G_CALLBACK(bar_draw), GINT_TO_POINTER((int)lround(pct))); p_put(bar); p_custom(bar_lay, bar, 4, 4); }
		Cell *c = p_r2("Monthly limit (MB)"); GtkWidget *e = rui_entry(""); char lb[24]; snprintf(lb, sizeof lb, "%g", lim); gtk_entry_set_text(GTK_ENTRY(e), lb); g_object_set_data_full(G_OBJECT(e), "was", g_strdup(lb), g_free);
		gtk_widget_set_size_request(e, (int)lround(110 * s_z()), -1); Key *lk = key(id, "limit"); g_object_set_data_full(G_OBJECT(e), "key", lk, g_free);
		g_signal_connect(e, "activate", G_CALLBACK(lim_act), lk); g_signal_connect(e, "focus-out-event", G_CALLBACK(lim_out), lk); c_add(c, e, 0, 0, 0); snprintf(nm, sizeof nm, "moblim%d", id); p_label_named(nm, e); p_r2_more(c);
		c = p_r2("The month starts on day"); const char *vv[28]; char vb[28][4]; for (int i = 0; i < 28; i++) { snprintf(vb[i], 4, "%d", i + 1); vv[i] = vb[i]; }
		char cur[8]; snprintf(cur, sizeof cur, "%d", (int)rui_jnum(u, "startday", 1)); Key *sk = key(id, "startday"); GtkWidget *sd = s_select(vv, vv, 28, cur, on_sel, sk); g_object_set_data_full(G_OBJECT(sd), "key", sk, g_free);
		c_add(c, sd, 0, 0, 1); snprintf(nm, sizeof nm, "mobsd%d", id); p_label_named(nm, sd);
		snprintf(nm, sizeof nm, "mobdoff%d", id); Ick *k = ick("Turn mobile data off at the limit", 0, rui_jbool(u, "dataoff"), 1, on_flag, key(id, "dataoff"), 0, nm); g_object_set_data_full(G_OBJECT(k->a), "key", k->d, g_free); }
	p_msg("A notice at 80 %. Counted on this computer from what the modem sends and receives: your operator's count can differ a little.", 2);
	p_h4("Roaming");
	{ snprintf(nm, sizeof nm, "mobroam%d", id); Ick *k = ick("Use mobile data when roaming", 0, rui_jbool(s, "roaming"), 1, on_flag, key(id, "roaming"), 0, nm); g_object_set_data_full(G_OBJECT(k->a), "key", k->d, g_free); }
	p_msg("Off by default: abroad, or on another operator's network, data can cost much more than at home.", 2);
	p_h4("Network type");
	{ RuiJ *md = rui_jprop(s, "modes"); int n = rui_jlen(md); const char *vv[8], *ll[8]; int k = 0;
		if (!n) { vv[k] = "auto"; ll[k++] = MOBMODE[0][1]; }
		for (int i = 0; i < n && k < 8; i++) { vv[k] = rui_jsval(rui_jat(md, i)); ll[k] = vv[k]; for (guint j = 0; j < G_N_ELEMENTS(MOBMODE); j++) if (!strcmp(MOBMODE[j][0], vv[k])) ll[k] = MOBMODE[j][1]; k++; }
		Cell *c = p_r2("Use"); Key *mk = key(id, "mode"); GtkWidget *sel = s_select(vv, ll, k, *set_jstr(s, "mode", "") ? set_jstr(s, "mode", "") : "auto", on_sel, mk); g_object_set_data_full(G_OBJECT(sel), "key", mk, g_free);
		c_add(c, sel, 0, 0, 1); snprintf(nm, sizeof nm, "mobmode%d", id); p_label_named(nm, sel);
		p_msg(n < 2 ? "This modem does not let the network type be chosen." : "", 2); }
}

/* ---- the connection (mobConn) */
static void on_conn(GtkButton *b, gpointer d)
{
	int id = GPOINTER_TO_INT(d) >> 1, on = GPOINTER_TO_INT(d) & 1; gtk_widget_set_sensitive(GTK_WIDGET(b), FALSE); char p[64], m[300]; snprintf(p, sizeof p, "/api/mobile/connect?id=%d&on=%d", id, on);
	if (post(p, NULL, m, sizeof m, 0) == 200) set_notify("📶 Mobile data", on ? "Connected." : "Disconnected: it stays off until you connect again.", 0); else set_notify("📶 Mobile data", m, 1);
	rui_jfree(ST); ST = rui_get("/api/mobile/status"); redraw();
}
/* ---- the signal's details (mobSignal: every 5 s while the page is open) */
static const char *q_(double v, double g, double f, double p) { return v >= g ? "excellent" : v >= f ? "good" : v >= p ? "fair" : "poor"; }
static void u_(GString *b, const char *pre, RuiJ *o, const char *k, const char *unit) { RuiJ *v = rui_jprop(o, k); if (!v || rui_jtype(v) != 2) return; char n[24]; g_ascii_formatd(n, sizeof n, "%g", round(rui_jval(v) * 10) / 10); g_string_append_printf(b, "%s%s %s", pre, n, unit); }
static int has_(RuiJ *o, const char *k) { RuiJ *v = rui_jprop(o, k); return v && rui_jtype(v) == 2; }
static char *sig_text(RuiJ *g)
{
	if (!g) return g_strdup("The modem gives no signal details."); GString *b = g_string_new(NULL); RuiJ *l = rui_jprop(g, "lte"), *n = rui_jprop(g, "nr5g"), *u = rui_jprop(g, "umts");
	if (has_(l, "rsrp")) { if (b->len) g_string_append_c(b, '\n'); u_(b, "LTE: RSRP ", l, "rsrp", "dBm"); g_string_append_printf(b, " (%s)", q_(rui_jnum(l, "rsrp", 0), -80, -90, -100));
		if (has_(l, "rsrq")) { u_(b, " · RSRQ ", l, "rsrq", "dB"); g_string_append_printf(b, " (%s)", q_(rui_jnum(l, "rsrq", 0), -10, -15, -20)); }
		if (has_(l, "sinr")) { u_(b, " · SINR ", l, "sinr", "dB"); g_string_append_printf(b, " (%s)", q_(rui_jnum(l, "sinr", 0), 20, 13, 0)); }
		u_(b, " · RSSI ", l, "rssi", "dBm"); }
	if (has_(n, "rsrp")) { if (b->len) g_string_append_c(b, '\n'); u_(b, "5G: RSRP ", n, "rsrp", "dBm"); g_string_append_printf(b, " (%s)", q_(rui_jnum(n, "rsrp", 0), -80, -90, -100)); u_(b, " · RSRQ ", n, "rsrq", "dB"); u_(b, " · SINR ", n, "sinr", "dB"); }
	if (has_(u, "rscp")) { if (b->len) g_string_append_c(b, '\n'); u_(b, "3G: RSCP ", u, "rscp", "dBm"); u_(b, " · Ec/Io ", u, "ecio", "dB"); u_(b, " · RSSI ", u, "rssi", "dBm"); }
	if (!b->len) g_string_append(b, "The modem gives no signal details yet.");
	return g_string_free(b, FALSE);
}
static GPtrArray *SIGW;   /* the signal's lines on the page (their modem's id in "id") */
static gboolean sig_tick(gpointer d)
{
	(void)d; if (!ALIVE) { SIGT = 0; return G_SOURCE_REMOVE; }
	for (guint i = 0; SIGW && i < SIGW->len; i++) { GtkWidget *w = g_ptr_array_index(SIGW, i); char p[64]; snprintf(p, sizeof p, "/api/mobile/signal?id=%d", GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), "id")));
		RuiJ *g = rui_get(p); char *t = sig_text(g); p_text_set(w, t); g_free(t); rui_jfree(g); }
	return G_SOURCE_CONTINUE;
}

/* ---- the SIM's PIN and PUK (mobModem's codes: elevate({cred:true}), then mobile/pin|puk with the name and password) */
typedef struct { int id, puk; char pin[16], code[16]; GtkWidget *msg; } Code;
static void say(GtkWidget *m, const char *t, int bad) { char *e = esc(rui_tr(t)), *x = bad ? g_strdup_printf("\x03<span foreground=\"#dc2626\">%s</span>", e) : g_strdup(t); p_text_set(m, x); g_free(e); g_free(x); set_note("mobmsg %s", t); set_state_soon(); }
static gboolean redraw_idle(gpointer d) { (void)d; if (ALIVE) { rui_jfree(ST); ST = rui_get("/api/mobile/status"); redraw(); } return G_SOURCE_REMOVE; }
static void code_cred(const char *n, const char *p, void *d)
{
	Code *c = d; if (!n) { explicit_bzero(c, sizeof *c); g_free(c); return; }
	char *en = set_jesc(n), *ep = set_jesc(p), *b = c->puk ? g_strdup_printf("{\"n\":\"%s\",\"p\":\"%s\",\"puk\":\"%s\",\"pin\":\"%s\"}", en, ep, c->code, c->pin) : g_strdup_printf("{\"n\":\"%s\",\"p\":\"%s\",\"pin\":\"%s\"}", en, ep, c->pin);
	char path[64], m[300]; snprintf(path, sizeof path, c->puk ? "/api/mobile/puk?id=%d" : "/api/mobile/pin?id=%d", c->id); int r = post(path, b, m, sizeof m, 1);
	explicit_bzero(b, strlen(b)); explicit_bzero(ep, strlen(ep)); g_free(b); g_free(en); g_free(ep);
	if (r == 200) { set_notify("📶 SIM card", *m ? m : "Unlocked.", 0); g_idle_add(redraw_idle, NULL); }
	else { say(c->msg, m, 1); if (strstr(m, "blocked") || strstr(m, "tries left")) g_timeout_add(1500, redraw_idle, NULL); }
	explicit_bzero(c, sizeof *c); g_free(c);
}
static int digits(const char *s, int lo, int hi) { int n = strlen(s); if (n < lo || n > hi) return 0; for (int i = 0; i < n; i++) if (!g_ascii_isdigit(s[i])) return 0; return 1; }
static void on_pin(GtkButton *b, gpointer d)
{
	(void)b; GtkWidget *e = d; int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(e), "id")); GtkWidget *m = g_object_get_data(G_OBJECT(e), "msg"); const char *pin = gtk_entry_get_text(GTK_ENTRY(e));
	if (!digits(pin, 4, 8)) { say(m, "A PIN is 4 to 8 digits.", 1); return; }
	Code *c = g_new0(Code, 1); c->id = id; c->msg = m; snprintf(c->pin, sizeof c->pin, "%s", pin); rui_elevate_cred(SW, "Unlock the SIM card with its PIN", code_cred, c); set_note("ask mobile pin %d", id);
}
static void on_puk(GtkButton *b, gpointer d)
{
	(void)b; GtkWidget **e = d; int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(e[0]), "id")); GtkWidget *m = g_object_get_data(G_OBJECT(e[0]), "msg");
	const char *puk = gtk_entry_get_text(GTK_ENTRY(e[0])), *p1 = gtk_entry_get_text(GTK_ENTRY(e[1])), *p2 = gtk_entry_get_text(GTK_ENTRY(e[2]));
	if (!digits(puk, 8, 8)) { say(m, "A PUK is 8 digits.", 1); return; } if (!digits(p1, 4, 8)) { say(m, "The new PIN must be 4 to 8 digits.", 1); return; } if (strcmp(p1, p2)) { say(m, "The two new PINs are not the same.", 1); return; }
	Code *c = g_new0(Code, 1); c->id = id; c->puk = 1; c->msg = m; snprintf(c->code, sizeof c->code, "%s", puk); snprintf(c->pin, sizeof c->pin, "%s", p1); rui_elevate_cred(SW, "Unblock the SIM card with its PUK and a new PIN", code_cred, c); set_note("ask mobile puk %d", id);
}
static GtkWidget *code_entry(void) { GtkWidget *e = rui_entry(""); gtk_entry_set_visibility(GTK_ENTRY(e), FALSE); gtk_entry_set_max_length(GTK_ENTRY(e), 8); gtk_entry_set_input_purpose(GTK_ENTRY(e), GTK_INPUT_PURPOSE_PIN); return e; }

static void modem(RuiJ *m)
{
	int id = (int)rui_jnum(m, "id", 0); char nm[32]; const char *mk = set_jstr(m, "maker", ""), *mo = set_jstr(m, "model", ""), *lk = set_jstr(m, "locked", ""), *fl = set_jstr(m, "failed", ""), *st = set_jstr(m, "state", "");
	{ GString *n = g_string_new("📶 "); if (v_(mk)) g_string_append(n, mk); if (v_(mo)) g_string_append_printf(n, "%s%s", v_(mk) ? " " : "", mo); if (!v_(mk) && !v_(mo)) g_string_append(n, rui_tr("Cellular modem")); p_h4(n->str); g_string_free(n, TRUE); }
	{ char *l = mdm_line(m); p_msg(l, 2); g_free(l); }
	int sim = rui_jbool(m, "sim");
	if (!sim || !strcmp(fl, "sim-missing")) p_msg("No SIM card. Insert one (turn the computer off first if its slot is inside, under a cover), then restart.", 2);
	else if (!strcmp(fl, "sim-error")) p_msg("\x03<span foreground=\"#dc2626\"><b>The SIM card is blocked for good</b> (its PUK was given wrongly too many times). Only your operator can replace it: ask them for a new SIM.</span>", 2);
	else if (!strcmp(lk, "sim-pin")) {
		RuiJ *r = rui_jprop(rui_jprop(m, "retries"), "pin"); char *t = g_strdup_printf("\x03%s <b>%d tries left.</b>", rui_tr("The SIM card is locked with its PIN."), r && rui_jtype(r) == 2 ? (int)rui_jval(r) : 3); p_msg(t, 2); g_free(t);
		Cell *c = p_r2("PIN"); GtkWidget *e = code_entry(); c_add(c, e, 0, 0, 1); snprintf(nm, sizeof nm, "mobpin%d", id); p_label_named(nm, e); g_object_set_data(G_OBJECT(e), "id", GINT_TO_POINTER(id));
		GtkWidget *b = btn("Unlock the SIM", G_CALLBACK(on_pin), e, NULL); snprintf(nm, sizeof nm, "mobpinb%d", id); p_label_named(nm, b);
		GtkWidget *mg = p_msg("", 2); g_object_set_data(G_OBJECT(e), "msg", mg); snprintf(nm, sizeof nm, "mobmsg%d", id); p_label_named(nm, mg);
		p_msg("An administrator's password is needed. After three wrong PINs the SIM asks for its PUK.", 2); }
	else if (!strcmp(lk, "sim-puk")) {
		RuiJ *r = rui_jprop(rui_jprop(m, "retries"), "puk");
		char *t = g_strdup_printf("\x03<span foreground=\"#b45309\">⚠️ <b>The SIM card is blocked</b> after three wrong PINs. Type its PUK (8 digits: on the card the SIM came on, or from your operator) and a new PIN. <b>Careful: ten wrong PUKs block the SIM for good</b>, and only your operator can replace it. <b>%d tries left.</b></span>", r && rui_jtype(r) == 2 ? (int)rui_jval(r) : 10);
		p_msg(t, 2); g_free(t);
		GtkWidget **e = g_new0(GtkWidget *, 3); const char *L[3] = { "PUK", "New PIN", "New PIN again" }, *N[3] = { "mobpuk", "mobnp1", "mobnp2" };
		for (int i = 0; i < 3; i++) { Cell *c = p_r2(L[i]); e[i] = code_entry(); c_add(c, e[i], 0, 0, 1); snprintf(nm, sizeof nm, "%s%d", N[i], id); p_label_named(nm, e[i]); if (i < 2) p_r2_more(c); }
		g_object_set_data(G_OBJECT(e[0]), "id", GINT_TO_POINTER(id));
		GtkWidget *b = btn("Unblock the SIM", G_CALLBACK(on_puk), e, NULL); g_object_set_data_full(G_OBJECT(b), "e", e, g_free); snprintf(nm, sizeof nm, "mobpukb%d", id); p_label_named(nm, b);
		GtkWidget *mg = p_msg("", 2); g_object_set_data(G_OBJECT(e[0]), "msg", mg); snprintf(nm, sizeof nm, "mobmsg%d", id); p_label_named(nm, mg); }
	else { GString *t = g_string_new(rui_tr("SIM card: ")); g_string_append(t, *set_jstr(m, "simName", "") ? set_jstr(m, "simName", "") : rui_tr("unnamed operator"));
		if (*set_jstr(m, "simOp", "")) g_string_append_printf(t, " (%s)", set_jstr(m, "simOp", ""));
		if (!strcmp(set_jstr(m, "reg", ""), "roaming")) { g_string_append(t, rui_tr(" · roaming now")); if (*set_jstr(m, "operator", "")) g_string_append_printf(t, " on %s", set_jstr(m, "operator", "")); }
		p_msg(t->str, 2); g_string_free(t, TRUE); }
	if (sim && !*lk && strcmp(fl, "sim-error") && strcmp(fl, "sim-missing")) {
		int on = !strcmp(st, "connected"); p_h4("Connection");
		{ const char *rt = set_jstr(ST, "route", ""); char *t;
			if (on) t = !*set_jstr(m, "iface", "") ? g_strdup("Connected") : rui_jbool(ST, "cellular") ? g_strdup("Connected and in use") : g_strdup_printf("Connected: kept in reserve while %s carries the traffic", g_str_has_prefix(rt, "wl") ? "Wi-Fi" : *rt ? "a cable" : "another connection");
			else t = !strcmp(st, "registered") ? g_strdup("Not connected") : mdm_line(m);
			p_msg(t, 2); g_free(t); }
		GtkWidget *b = btn(on ? "Disconnect" : "Connect", G_CALLBACK(on_conn), GINT_TO_POINTER(id << 1 | !on), NULL); snprintf(nm, sizeof nm, "mobcb%d", id); p_label_named(nm, b);
		if (!strcmp(st, "registered") || on) { p_h4("Signal"); char p[64]; snprintf(p, sizeof p, "/api/mobile/signal?id=%d", id); RuiJ *g = rui_get(p); char *t = sig_text(g); rui_jfree(g);
			GtkWidget *w = p_msg(t, 2); g_free(t); g_object_set_data(G_OBJECT(w), "id", GINT_TO_POINTER(id)); if (!SIGW) SIGW = g_ptr_array_new(); g_ptr_array_add(SIGW, w); snprintf(nm, sizeof nm, "mobsig%d", id); p_label_named(nm, w);
			p_msg("Move the computer (or an outside antenna) while watching these: they change every few seconds.", 2); }
		sim_settings(id);
	}
}

void page_mob(void)
{
	if (!SETS) { SETS = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)rui_jfree); CUSTOM = g_hash_table_new(g_direct_hash, g_direct_equal); }
	g_hash_table_remove_all(SETS); if (!KEEP) g_hash_table_remove_all(CUSTOM);   /* (asked again each time, as the page's draw; "Typed in" kept while the page is open) */
	if (SIGW) g_ptr_array_set_size(SIGW, 0);
	rui_jfree(ST); ST = rui_get("/api/mobile/status"); rui_jfree(AIR); AIR = rui_get("/api/mobile/airplane");
	GtkWidget *first = NULL;
	if (rui_jbool(AIR, "on")) first = p_msg("✈️ Airplane mode is on: the cellular modem, Wi-Fi and Bluetooth are off. Turn it off in quick settings.", 2);
	RuiJ *ms = rui_jprop(ST, "modems");
	if (!rui_jlen(ms)) { p_h4("Mobile network"); GtkWidget *w = p_msg("No cellular modem is connected. A USB cellular stick, or a laptop's built-in cellular module, shows here with its SIM card. A hotspot or a phone sharing its connection works through Wi-Fi or USB (Network Settings).", 2); if (!first) first = w; }
	for (int i = 0; i < rui_jlen(ms); i++) modem(rui_jat(ms, i));
	{ GtkWidget *w = gtk_drawing_area_new(); gtk_widget_set_no_show_all(w, TRUE); p_put(w); if (ALIVE) g_object_remove_weak_pointer(G_OBJECT(ALIVE), (gpointer *)&ALIVE); ALIVE = w; g_object_add_weak_pointer(G_OBJECT(w), (gpointer *)&ALIVE); }
	if (!SIGT && SIGW && SIGW->len) SIGT = g_timeout_add(5000, sig_tick, NULL);
	set_note("mob %d", rui_jlen(ms));
}
