/*
 * rudolf-settings: User Settings (base_v24.html usrPanelS as build19.py and settings25.js make it, with acct50.js's Files and
 * administrators, devkey59.js's publisher key, fido61.js's security keys, fprint63.js's fingerprints, otp60.js's authenticator app,
 * power31.js's and saver31.js's links). 1.6.3, docs/plan-1.6.3.md Part 3 (PR 8). As the page:
 *   - the accounts (bd95user: names, roles, and whether each has a key, codes, controls; never a hash or a key read here) and the
 *     session's own (desktop/session);
 *   - Sign-in options: the old page-only key, Guest account and Lock (bd95sys as the page's sysSave), the links to Display Properties
 *     and System health, parental controls (each account's pc in bd95user), the authenticator app, the security keys (rudolfd's
 *     desktop/keys...), the fingerprints (rudolfd's fp/...);
 *   - Files and administrators (aa, ak), the publisher key (pubkey:NAME).
 * What the page does with its own cryptography and its own stores (a new password, a new account, an account removed, codes set up,
 * another account's password reset, a publisher key made or read) is the page's: asked of a page of one action ("storgo W"), whose
 * dialogs are the page's own, so no password, key or secret of the page passes through this program. GPL-2.0-or-later.
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
static void redraw(void) { set_show_keep("usr"); }
static void ask(const char *a) { rui_shell(SW, a); set_note("%s", a); }
static int jb(RuiJ *o, const char *k) { RuiJ *v = o ? rui_jprop(o, k) : NULL; return v && ((rui_jtype(v) == 1 && rui_jval(v)) || (rui_jtype(v) == 2 && rui_jval(v)) || (rui_jtype(v) == 3 && *rui_jsval(v))); }
static double jnum(RuiJ *o, const char *k, double d) { RuiJ *v = o ? rui_jprop(o, k) : NULL; return v && rui_jtype(v) == 2 ? rui_jval(v) : d; }
static RuiJ *X(RuiJ *o) { return o ? rui_jprop(o, "x") : NULL; }   /* (set_ls_get's {"x": value}) */
static char *jstr_esc(const char *s) { char *e = set_jesc(s ? s : ""), *r = g_strdup_printf("\"%s\"", e); g_free(e); return r; }
G_GNUC_UNUSED static char *cred_body(const char *n, const char *p, const char *more) { char *a = jstr_esc(n), *b = jstr_esc(p), *r = g_strdup_printf("{\"n\":%s,\"p\":%s%s}", a, b, more ? more : ""); explicit_bzero(b, strlen(b)); g_free(a); g_free(b); return r; }
G_GNUC_UNUSED static void wipe(char *s) { if (s) { explicit_bzero(s, strlen(s)); g_free(s); } }
/* a request to rudolfd with a JSON body (fido61.js fidoReq): the answer, its status in *st */
static RuiJ *req(const char *path, const char *body, int *st)
{
	char p[400], *out = NULL; snprintf(p, sizeof p, "/api/%s", path); int c = rui_http(body ? "POST" : "GET", p, body, body ? strlen(body) : 0, &out, NULL); RuiJ *j = out ? rui_jparse(out) : NULL; g_free(out);
	set_note("%s %s %d", body ? "POST" : "GET", path, c); if (st) *st = c; return j;
}

/* ================================================================== the accounts (bd95user's accts, as the page's ACCTS) */
static RuiJ *UO; static RuiJ *ACC; static int NACC, MEI = -1;
static RuiJ *ac_at(int i) { return ACC && i >= 0 && i < NACC ? rui_jat(ACC, i) : NULL; }
static const char *an(int i) { return set_jstr(ac_at(i), "n", ""); }
static int is_admin_i(int i) { return !strcmp(set_jstr(ac_at(i), "r", ""), "admin"); }
static int allowed(int i) { RuiJ *v = rui_jprop(ac_at(i), "aa"); return !(v && rui_jtype(v) == 1 && !rui_jval(v)); }   /* (acAllowed: a.aa !== false) */
static void accts_load(void)
{
	rui_jfree(UO); UO = set_ls_get("bd95user"); RuiJ *v = X(UO); ACC = v ? rui_jprop(v, "accts") : NULL; NACC = ACC && rui_jtype(ACC) == 4 ? rui_jlen(ACC) : 0; MEI = -1;
	for (int i = 0; i < NACC; i++) if (!strcmp(set_jstr(ac_at(i), "id", ""), ACCT)) MEI = i;
	if (MEI < 0) for (int i = 0; i < NACC; i++) if (!strcmp(an(i), ME)) MEI = i;
}
/* (the page's saveUser for a change of an account's own fields: the list written again with that account's field set or taken away) */
static void acct_set(int who, const char *k, const char *json)
{
	RuiJ *o = set_ls_get("bd95user"), *v = X(o), *l = v ? rui_jprop(v, "accts") : NULL; if (!l || rui_jtype(l) != 4) { rui_jfree(o); return; }
	const char *id = set_jstr(ac_at(who), "id", ""), *nm = an(who); GString *b = g_string_new("{");
	for (int i = 0; rui_jkey(v, i); i++) { const char *key = rui_jkey(v, i); char *e = set_jesc(key); g_string_append_printf(b, "%s\"%s\":", i ? "," : "", e); g_free(e);
		if (strcmp(key, "accts")) { set_jw(b, rui_jprop(v, key)); continue; }
		g_string_append_c(b, '[');
		for (int a = 0; a < rui_jlen(l); a++) { RuiJ *x = rui_jat(l, a); if (a) g_string_append_c(b, ',');
			int me = *id ? !strcmp(set_jstr(x, "id", ""), id) : !strcmp(set_jstr(x, "n", ""), nm); if (!me) { set_jw(b, x); continue; }
			g_string_append_c(b, '{'); int f = 1, done = 0;
			for (int q = 0; rui_jkey(x, q); q++) { const char *xk = rui_jkey(x, q); if (!strcmp(xk, k)) { if (json) { char *e2 = set_jesc(xk); g_string_append_printf(b, "%s\"%s\":%s", f ? "" : ",", e2, json); g_free(e2); f = 0; } done = 1; continue; }
				char *e2 = set_jesc(xk); g_string_append_printf(b, "%s\"%s\":", f ? "" : ",", e2); g_free(e2); set_jw(b, rui_jprop(x, xk)); f = 0; }
			if (!done && json) { char *e2 = set_jesc(k); g_string_append_printf(b, "%s\"%s\":%s", f ? "" : ",", e2, json); g_free(e2); }
			g_string_append_c(b, '}'); }
		g_string_append_c(b, ']'); }
	g_string_append_c(b, '}'); set_ls_set("bd95user", b->str); set_note("svj bd95user %s %s", k, json ? json : "(removed)"); g_string_free(b, TRUE); rui_jfree(o); accts_load();
}
/* (the page's sysSave: one key of bd95sys) */
static void sys_set(const char *k, const char *json)
{
	RuiJ *o = set_ls_get("bd95sys"), *v = X(o); GString *b = g_string_new("{"); int first = 1, done = 0;
	for (int i = 0; v && rui_jtype(v) == 5 && rui_jkey(v, i); i++) { const char *key = rui_jkey(v, i); char *e = set_jesc(key); g_string_append_printf(b, "%s\"%s\":", first ? "" : ",", e); g_free(e); first = 0;
		if (!strcmp(key, k)) { g_string_append(b, json); done = 1; } else set_jw(b, rui_jprop(v, key)); }
	if (!done) { char *e = set_jesc(k); g_string_append_printf(b, "%s\"%s\":%s", first ? "" : ",", e, json); g_free(e); }
	g_string_append_c(b, '}'); set_ls_set("bd95sys", b->str); set_note("svj bd95sys %s", b->str); g_string_free(b, TRUE); rui_jfree(o);
}

/* ================================================================== small blocks: a line of writing, a .msg with a link */
/* "Signed in as NAME [Admin]" (color --mut, the name bold, .rolebadge) */
typedef struct { GtkWidget *a; } Who;
static double who_lay(void *d, double x, double y, double w) { Who *k = d; p_place(k->a, x, y, w, 16); return 16; }
static gboolean who_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)w; (void)d; const char *t = "Signed in as "; rui_color(cr, "mut", 1); s_text(cr, t, 0, 0, 13, 400, 0, 15); double x = tw(t, 13, 400); s_text(cr, ME, x, 0, 13, 700, 0, 15); x += tw(ME, 13, 700) + tw(" ", 13, 400) + 6;
	const char *r = !strcmp(ROLE, "admin") ? "Admin" : "User"; double bw = tw(r, 12, 400) + 16; cairo_set_source_rgb(cr, 0xe0 / 255.0, 0xe7 / 255.0, 0xff / 255.0); s_rrect(cr, x, 0, bw, 16, 8); cairo_fill(cr);
	cairo_set_source_rgb(cr, 0x37 / 255.0, 0x30 / 255.0, 0xa3 / 255.0); s_text(cr, r, x + 8, 1, 12, 400, 0, 14); return FALSE;
}
/* a .msg's link ("In <a>Display Properties › Screen saver</a>."): its words before the link, the link, after it */
typedef struct { char *pre, *link, *go; } MLink;
static gboolean mlink_click(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	MLink *k = d; double x = e->x / s_z(), a = tw(k->pre, 12, 400), b = a + tw(k->link, 12, 400); if (x >= a && x <= b && e->y / s_z() < 15) { set_note("link %s", k->go); set_show(k->go); return TRUE; } (void)w; return FALSE;
}
static GtkWidget *c_mlink(Cell *c, const char *pre, const char *link, const char *post, const char *go)
{
	MLink *k = g_new0(MLink, 1); k->pre = g_strdup(pre); k->link = g_strdup(link); k->go = g_strdup(go);
	char *a = g_markup_escape_text(pre, -1), *b = g_markup_escape_text(link, -1), *z = g_markup_escape_text(post, -1);
	char *m = g_strdup_printf("\x03%s<span foreground=\"%s\" underline=\"single\">%s</span>%s", a, RUI.dark ? "#93c5fd" : "#0000ee", b, z); g_free(a); g_free(b); g_free(z);
	GtkWidget *w = c_msg(c, m, 2); g_free(m); gtk_widget_add_events(w, GDK_BUTTON_PRESS_MASK); g_signal_connect(w, "button-press-event", G_CALLBACK(mlink_click), k); return w;
}

/* ================================================================== Your account, All accounts, Create new account (usrPanelS) */
static GtkWidget *OP, *NN, *NP1, *NP2, *CN, *CP1, *CP2, *CR; static char *PR[16]; static int PDEL[16]; static char *UM; static int UM_OK;
static GtkWidget *entry(const char *ph, int masked, int on, const char *name) { GtkWidget *e = rui_entry(ph); if (masked) gtk_entry_set_visibility(GTK_ENTRY(e), FALSE); if (!on) gtk_widget_set_sensitive(e, FALSE); p_label_named(name, e); return e; }
static const char *etext(GtkWidget *e) { return e ? gtk_entry_get_text(GTK_ENTRY(e)) : ""; }
static int npr(void) { int n = 0; for (int i = 0; i < 16; i++) if (PR[i]) n++; return n; }
static int npdel(void) { int n = 0; for (int i = 0; i < 16; i++) n += PDEL[i]; return n; }
static int usr_dirty(void) { return *etext(NN) || *etext(OP) || *etext(NP1) || *etext(NP2) || *etext(CN) || *etext(CP1) || *etext(CP2) || npr() || npdel(); }
static void on_ed(GtkEditable *e, gpointer d) { (void)e; (void)d; p_footer_update(); }
static void watch(GtkWidget *e) { g_signal_connect(e, "changed", G_CALLBACK(on_ed), NULL); }
static void clear_all(void)
{
	GtkWidget *E[7] = { OP, NN, NP1, NP2, CN, CP1, CP2 }; for (int i = 0; i < 7; i++) if (E[i]) gtk_entry_set_text(GTK_ENTRY(E[i]), "");
	if (CR) s_select_set(CR, "user"); for (int i = 0; i < 16; i++) { g_free(PR[i]); PR[i] = NULL; PDEL[i] = 0; } g_free(UM); UM = NULL;
}
/* (#acl's row: .acr, grid 1fr 110px 90px, gap 8, 4px 0, the line under it) */
typedef struct { GtkWidget *a, *sel, *btn; int i; } AclRow;
static double acl_lay(void *d, double x, double y, double w) { AclRow *r = d; double fr = w - 110 - 90 - 16; p_place(r->sel, x + fr + 8, y + 4, 110, 25); if (r->btn) p_place(r->btn, x + fr + 8 + 110 + 8, y + 5, 90, 23); p_place(r->a, x, y, w, 34); return 34; }
static gboolean acl_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	AclRow *r = d; double W = gtk_widget_get_allocated_width(w) / s_z(); cairo_set_source_rgb(cr, 0xee / 255.0, 0xf0 / 255.0, 0xf3 / 255.0); cairo_rectangle(cr, 0, 33, W, 1); cairo_fill(cr);
	char *t = g_strdup_printf("👤 %s%s", an(r->i), r->i == MEI ? " (you)" : ""); if (PDEL[r->i]) cairo_push_group(cr); s_col(cr, "fg", 1); s_text(cr, t, 0, 9, 13, 400, W - 216, 15);
	if (PDEL[r->i]) { double lw = fmin(tw(t, 13, 400), W - 216); cairo_rectangle(cr, 0, 9 + 8, lw, 1); cairo_fill(cr); cairo_pop_group_to_source(cr); cairo_paint_with_alpha(cr, .55); } g_free(t); return FALSE;
}
static void on_role(const char *v, void *d)
{
	int i = GPOINTER_TO_INT(d); if (!set_admin()) return; g_free(PR[i]); PR[i] = strcmp(v, set_jstr(ac_at(i), "r", "user")) ? g_strdup(v) : NULL; set_note("role %s %s", an(i), v); p_footer_update();
}
static void on_del(GtkButton *b, gpointer d) { (void)b; int i = GPOINTER_TO_INT(d); if (!set_admin()) return; PDEL[i] = !PDEL[i]; set_note("remove %s %d", an(i), PDEL[i]); p_footer_update(); redraw(); }
static void say(const char *t, int ok) { g_free(UM); UM = g_strdup(t); UM_OK = ok; set_note("say %s", t); }
/* Apply: the page's doApply (its checks first, as the page makes them, before anything); the change itself, which needs the page's
   own cryptography and stores, is the page's (a page of one action, "storgo usrapply") */
static void foot(int what)
{
	if (what == 1) { clear_all(); redraw(); return; }
	const char *nn = etext(NN), *p1 = etext(NP1), *p2 = etext(NP2), *cn = etext(CN), *c1 = etext(CP1), *c2 = etext(CP2); int wc = *cn || *c1 || *c2;
	if (!*etext(OP)) { say("The current password is incorrect. Nothing was changed.", 0); redraw(); return; }
	if (!*nn && !*p1 && !wc && !npr() && !npdel()) { say("There are no changes to apply.", 0); redraw(); return; }
	if ((npr() || npdel() || wc) && !set_admin()) { say("Only administrators can create accounts, change permissions or remove accounts.", 0); redraw(); return; }
	for (int i = 0; *nn && i < NACC; i++) if (i != MEI && !g_ascii_strcasecmp(an(i), nn)) { say("Another account already uses that name.", 0); redraw(); return; }
	if (*p1 && strcmp(p1, p2)) { say("The new password and its confirmation do not match.", 0); redraw(); return; }
	if (wc) { if (!*cn) { say("Enter a name for the new account.", 0); redraw(); return; } if (!*c1) { say("Enter a password for the new account.", 0); redraw(); return; }
		if (strcmp(c1, c2)) { say("The new account password and its confirmation do not match.", 0); redraw(); return; }
		int dup = *nn && !g_ascii_strcasecmp(nn, cn); for (int i = 0; i < NACC; i++) if (!g_ascii_strcasecmp(an(i), cn)) dup = 1; if (dup) { say("Another account already uses that name.", 0); redraw(); return; } }
	int adm = 0; for (int i = 0; i < NACC; i++) if (!PDEL[i] && !strcmp(PR[i] ? PR[i] : set_jstr(ac_at(i), "r", "user"), "admin")) adm = 1; if (wc && !strcmp(s_select_get(CR), "admin")) adm = 1;
	if (!adm) { say("At least one administrator account must remain.", 0); redraw(); return; }
	ask("storgo usrapply"); clear_all(); redraw();
}
static void account_secs(int adm)
{
	p_h4("Your account");
	Cell *c = p_r2("Current password"); OP = entry(NULL, 1, 1, "op"); c_add(c, OP, 0, 0, 1); p_r2_more(c);
	c = p_r2("New user name"); NN = entry("leave blank to keep", 0, 1, "nn"); gtk_entry_set_max_length(GTK_ENTRY(NN), 24); c_add(c, NN, 0, 0, 1); p_r2_more(c);
	c = p_r2("New password"); NP1 = entry("leave blank to keep", 1, 1, "np1"); c_add(c, NP1, 0, 0, 1); p_r2_more(c);
	c = p_r2("Confirm new password"); NP2 = entry(NULL, 1, 1, "np2"); c_add(c, NP2, 0, 0, 1);
	p_h4("All accounts");
	static const char *const RV[2] = { "user", "admin" }, *const RL[2] = { "User", "Admin" };
	for (int i = 0; i < NACC && i < 16; i++) {
		AclRow *r = g_new0(AclRow, 1); r->i = i; r->a = gtk_drawing_area_new(); g_signal_connect(r->a, "draw", G_CALLBACK(acl_draw), r); p_put(r->a);
		r->sel = s_select(RV, RL, 2, PR[i] ? PR[i] : (is_admin_i(i) ? "admin" : "user"), on_role, GINT_TO_POINTER(i)); s_select_width(r->sel, 110); if (!adm || PDEL[i]) gtk_widget_set_sensitive(r->sel, FALSE); p_put(r->sel);
		char nm[16]; snprintf(nm, sizeof nm, "acs%d", i); p_label_named(nm, r->sel);
		if (adm && i != MEI) { r->btn = rui_button(PDEL[i] ? "Undo" : "Remove", 0); g_signal_connect(r->btn, "clicked", G_CALLBACK(on_del), GINT_TO_POINTER(i)); p_put(r->btn); snprintf(nm, sizeof nm, "acd%d", i); p_label_named(nm, r->btn); }
		p_custom(acl_lay, r, 0, 0); }
	p_h4("Create new account");
	c = p_r2("Account name"); CN = entry(NULL, 0, adm, "cn"); gtk_entry_set_max_length(GTK_ENTRY(CN), 24); c_add(c, CN, 0, 0, 1); p_r2_more(c);
	c = p_r2("Password"); CP1 = entry(NULL, 1, adm, "cp1"); c_add(c, CP1, 0, 0, 1); p_r2_more(c);
	c = p_r2("Confirm password"); CP2 = entry(NULL, 1, adm, "cp2"); c_add(c, CP2, 0, 0, 1); p_r2_more(c);
	c = p_r2("Permissions"); CR = s_select(RV, RL, 2, "user", NULL, NULL); if (!adm) gtk_widget_set_sensitive(CR, FALSE); c_add(c, CR, 0, 0, 1); p_label_named("cr", CR);
	GtkWidget *E[7] = { OP, NN, NP1, NP2, CN, CP1, CP2 }; for (int i = 0; i < 7; i++) watch(E[i]);
	GtkWidget *m = p_msg(UM ? UM : "", 0); p_label_named("um", m); if (UM) g_object_set_data(G_OBJECT(m), "set-color", UM_OK ? "#15803d" : "#b91c1c");
	p_label_named("uh", p_msg(adm ? "Your current password is required to apply any change. “Admin” accounts can change security settings, the system date and other accounts; “User” accounts cannot."
		: "You are a standard user. You can change your own name and password; only administrators can create accounts, change permissions or remove accounts.", 1));
}

/* ================================================================== Sign-in options (settings25.js, power31.js, saver31.js) */
static void pk_go(int ok, void *d) { (void)d; if (!ok) return; acct_set(MEI, "pk", NULL); redraw(); }
static void on_pkr(GtkButton *b, gpointer d) { (void)b; (void)d; char *r = g_strdup_printf("Remove the security key from %s", ME); set_note("elevate %s", r); rui_elevate(SW, r, pk_go, NULL); g_free(r); }
static void on_guest(int on, void *d) { (void)d; sys_set("guest", on ? "true" : "false"); }
static void on_lock(const char *v, void *d) { (void)d; sys_set("lockmin", v); }
/* (the page's optH: the option whose value is the setting's, else the first) */
static const char *sel_cur(RuiJ *sys, const char *k, const char *const *v, int n) { double x = jnum(sys, k, NAN); for (int i = 0; i < n; i++) if (!isnan(x) && atof(v[i]) == x) return v[i]; return v[0]; }
static void signin_sec(int adm)
{
	p_h4("Sign-in options"); RuiJ *so = set_ls_get("bd95sys"), *sys = X(so); RuiJ *me = ac_at(MEI); int old = me && rui_jprop(me, "pk");
	Cell *c = p_r2("Security key"); c_text(c, old ? "A key is set up." : "No key.", 400);
	if (old) { GtkWidget *b = rui_button("Remove", 0); g_signal_connect(b, "clicked", G_CALLBACK(on_pkr), NULL); c_add(c, b, 4, -4, 0); p_label_named("pkr", b); }
	c_line(c, 0); c_msg(c, old ? "When a key is added, signing in needs your password and the key." : "Security keys are under Security keys below.", 2);
	if (adm) { p_r2_more(c);
		c = p_r2("Guest account"); GtkWidget *g = c_check(c, "Show a Guest button on the sign-in screen", jb(sys, "guest"), on_guest, NULL); p_label_named("ugs", g); p_r2_more(c);
		c = p_r2("Screen saver"); c_mlink(c, "In ", "Display Properties › Screen saver", ".", "disp"); p_r2_more(c);
		static const char *const LV[6] = { "0", "5", "10", "15", "30", "60" }, *const LL[6] = { "No idle lock", "Lock after 5 minutes idle", "Lock after 10 minutes idle", "Lock after 15 minutes idle", "Lock after 30 minutes idle", "Lock after 60 minutes idle" };
		c = p_r2("Lock"); GtkWidget *s = s_select(LV, LL, 6, sel_cur(sys, "lockmin", LV, 6), on_lock, NULL); c_add(c, s, 0, 0, 0); p_label_named("ulm", s); p_r2_more(c);
		c = p_r2("Sleep"); c_mlink(c, "In ", "System health › Power and battery", ".", "health"); }
	rui_jfree(so);
}

/* ================================================================== Family and parental controls (each standard account's pc) */
typedef struct { GtkWidget *a, *sel, *ck[24], *save; int i, open, n; } Det;
static int DETOPEN[16];
static double det_lay(void *d, double x, double y, double w)
{
	Det *t = d; double h = 29;
	if (t->open) { double cx = x + 11, cw = w - 22, top = y + 22; p_place(t->sel, cx + 140, top, cw - 140, 25); double cy = top + 35, ck = cy;
		for (int k = 0; k < t->n; k++) { p_place(t->ck[k], cx + 140, ck, cw - 140, 19); ck += 19; }
		double r2h = 25 + 10 + (ck - cy); p_place(t->save, cx, top + r2h + 10, 0, 0); h = 22 + r2h + 10 + 23 + 7; }
	gtk_widget_set_visible(t->sel, t->open); for (int k = 0; k < t->n; k++) gtk_widget_set_visible(t->ck[k], t->open); gtk_widget_set_visible(t->save, t->open);
	p_place(t->a, x, y, w, h); return h;
}
static gboolean det_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Det *t = d; double W = gtk_widget_get_allocated_width(w) / s_z(), H = gtk_widget_get_allocated_height(w) / s_z();
	if (RUI.dark) cairo_set_source_rgb(cr, 0x33 / 255.0, 0x41 / 255.0, 0x55 / 255.0); else cairo_set_source_rgb(cr, 0xd1 / 255.0, 0xd5 / 255.0, 0xdb / 255.0);
	s_rrect(cr, .5, .5, W - 1, H - 1, 7.5); cairo_set_line_width(cr, 1); cairo_stroke(cr);
	RuiJ *a = ac_at(t->i), *pc = rui_jprop(a, "pc"); int on = pc && (jnum(pc, "mins", 0) || (rui_jprop(pc, "block") && rui_jlen(rui_jprop(pc, "block"))));
	char *s = g_strdup_printf("👤 %s%s", an(t->i), on ? " — controls on" : ""); s_col(cr, "fg", 1);
	/* (the summary's marker: Chromium's disclosure triangle, then the words) */
	double mx = 11, my = 7 + 4; if (t->open) { cairo_move_to(cr, mx, my + 1); cairo_line_to(cr, mx + 7, my + 1); cairo_line_to(cr, mx + 3.5, my + 6.5); } else { cairo_move_to(cr, mx + 1, my); cairo_line_to(cr, mx + 6.5, my + 3.5); cairo_line_to(cr, mx + 1, my + 7); }
	cairo_close_path(cr); cairo_fill(cr); s_text(cr, s, 11 + tw("▶ ", 13, 600), 7, 13, 600, 0, 15); g_free(s);
	if (t->open) { s_text(cr, "Daily time limit", 11, 22 + 5, 13, 400, 130, 15); double ah = t->n * 19; s_text(cr, "Allowed apps", 11, 22 + 35 + (ah - 15) / 2, 13, 400, 130, 15); }
	return FALSE;
}
static gboolean det_click(GtkWidget *w, GdkEventButton *e, gpointer d) { Det *t = d; (void)w; if (e->y / s_z() > 22) return FALSE; t->open = !t->open; DETOPEN[t->i] = t->open; set_note("details %s %d", an(t->i), t->open); p_relayout(); gtk_widget_queue_draw(t->a); return TRUE; }
static void on_pcsave(GtkButton *b, gpointer d)
{
	(void)b; Det *t = d; const char *m = s_select_get(t->sel); GString *bl = g_string_new("["); int nb = 0;
	for (int k = 0; k < t->n; k++) if (!rui_check_get(t->ck[k])) { char *e = jstr_esc(APPCAT_K[k]); g_string_append_printf(bl, "%s%s", nb++ ? "," : "", e); g_free(e); }
	g_string_append_c(bl, ']'); char *pc = atoi(m) || nb ? g_strdup_printf("{\"mins\":%s,\"block\":%s}", m, bl->str) : NULL; acct_set(t->i, "pc", pc);
	char *l = g_strdup_printf("%s changed parental controls for %s", ME, an(t->i)); set_log_evt("sys", l); g_free(l); { char *nb = g_strdup_printf("Parental controls for %s", an(t->i)); set_notify("Saved", nb, 0); g_free(nb); } g_free(pc); g_string_free(bl, TRUE); redraw();
}
static void family_sec(void)
{
	p_h4("Family and parental controls"); int any = 0;
	static const char *const MV[7] = { "0", "30", "60", "90", "120", "180", "240" }, *const ML[7] = { "No limit", "30 minutes", "60 minutes", "90 minutes", "120 minutes", "180 minutes", "240 minutes" };
	for (int i = 0; i < NACC && i < 16; i++) { if (is_admin_i(i)) continue; any = 1;
		RuiJ *pc = rui_jprop(ac_at(i), "pc"), *bl = rui_jprop(pc, "block"); Det *t = g_new0(Det, 1); t->i = i; t->open = DETOPEN[i]; t->a = gtk_drawing_area_new(); gtk_widget_add_events(t->a, GDK_BUTTON_PRESS_MASK);
		g_signal_connect(t->a, "draw", G_CALLBACK(det_draw), t); g_signal_connect(t->a, "button-press-event", G_CALLBACK(det_click), t); p_put(t->a);
		char cur[16]; snprintf(cur, sizeof cur, "%.0f", jnum(pc, "mins", 0)); int okv = 0; for (int k = 0; k < 7; k++) if (!strcmp(MV[k], cur)) okv = 1;
		t->sel = s_select(MV, ML, 7, okv ? cur : "0", NULL, NULL); p_put(t->sel); char nm[16]; snprintf(nm, sizeof nm, "pm%d", i); p_label_named(nm, t->sel);
		for (int k = 0; k < APPCAT_N && k < 24; k++) { int blk = 0; for (int q = 0; bl && q < rui_jlen(bl); q++) if (rui_jsval(rui_jat(bl, q)) && !strcmp(rui_jsval(rui_jat(bl, q)), APPCAT_K[k])) blk = 1;
			t->ck[k] = rui_check(APPCAT_N_[k], !blk, NULL, NULL); p_put(t->ck[k]); snprintf(nm, sizeof nm, "pa%d_%d", i, k); p_label_named(nm, t->ck[k]); t->n++; }
		char *sl = g_strdup_printf("Save for %s", an(i)); t->save = rui_button(sl, 0); g_free(sl); g_signal_connect(t->save, "clicked", G_CALLBACK(on_pcsave), t); p_put(t->save); snprintf(nm, sizeof nm, "ps%d", i); p_label_named(nm, t->save);
		snprintf(nm, sizeof nm, "pcl%d", i); p_label_named(nm, t->a);
		p_custom(det_lay, t, 0, 6); p_custom_more("Daily time limit"); p_custom_more("Allowed apps"); for (int k = 0; k < APPCAT_N; k++) p_custom_more(APPCAT_N_[k]); }
	if (!any) p_msg("Create a standard (User) account to set parental controls.", 2);
}

/* ================================================================== Authenticator app (otp60.js otpRow) */
static void otp_off_go(int ok, void *d) { (void)d; if (!ok) return; acct_set(MEI, "otp", NULL); char *l = g_strdup_printf("%s turned off authenticator codes", ME); set_log_evt("sys", l); g_free(l); redraw(); }
static int otp_required(void) { RuiJ *o = set_ls_get("bd95sys"); const char *m = set_jstr(X(o), "mfa", "off"); int r = !strcmp(m, "all") || (!strcmp(m, "admin") && !strcmp(ROLE, "admin")); rui_jfree(o); return r; }
static void on_otpr(GtkButton *b, gpointer d)
{
	(void)b; (void)d; if (otp_required()) { set_notify("Two-step sign-in", "An administrator requires two-step sign-in for this account, so it cannot be turned off.", 1); return; }
	char *r = g_strdup_printf("Turn off authenticator codes for %s", ME); set_note("elevate %s", r); rui_elevate(SW, r, otp_off_go, NULL); g_free(r);
}
static void on_otpa(GtkButton *b, gpointer d) { (void)b; (void)d; ask("storgo otpsetup"); }   /* (the page's otpSetup: the secret made, shown and sealed there) */
static void otp_row(void)
{
	int on = ac_at(MEI) && rui_jprop(ac_at(MEI), "otp"); Cell *c = p_r2("Authenticator app"); GtkWidget *t = c_text(c, on ? "On." : "Off.", 400); c_dy(t, 4); p_label_named("otps", t);
	GtkWidget *a = rui_button(on ? "Set up again…" : "Set up…", 0); g_signal_connect(a, "clicked", G_CALLBACK(on_otpa), NULL); c_add(c, a, tw(" ", 13, 400), 0, 0); p_label_named("otpa", a);
	if (on) { GtkWidget *r = rui_button("Turn off", 0); g_signal_connect(r, "clicked", G_CALLBACK(on_otpr), NULL); c_add(c, r, 0, 0, 0); p_label_named("otpr", r); }
	c_line(c, 0); c_msg(c, "Codes from an app on your phone, asked for after your password when you sign in or unlock. They guard the sign-in; your files stay protected by your password (security keys that also protect the files are coming).", 2);
}

/* ================================================================== Security keys (fido61.js fidoRow: rudolfd's desktop/keys...) */
static RuiJ *FST;
void usr_fido_add(void); void usr_fido_remove(int i); void usr_fido_require(int on);
static void on_fka(GtkButton *b, gpointer d) { (void)b; (void)d; usr_fido_add(); }
static void on_fkr(GtkButton *b, gpointer d) { (void)b; usr_fido_remove(GPOINTER_TO_INT(d)); }
static void on_fkq(int on, void *d) { (void)d; usr_fido_require(on); }
static void fido_row(void)
{
	char p[200]; char *e = g_uri_escape_string(ACCT, NULL, FALSE); snprintf(p, sizeof p, "desktop/keys?acct=%s", e); g_free(e); rui_jfree(FST); int st = 0; FST = req(p, NULL, &st);
	RuiJ *keys = rui_jprop(FST, "keys"); int ok = FST && st == 200 && jb(FST, "ok") && keys && rui_jtype(keys) == 4, n = ok ? rui_jlen(keys) : 0, req_ = ok && jb(FST, "req");
	char s[200]; if (!ok) snprintf(s, sizeof s, "Needs the installed Rudolf OS."); else if (jb(FST, "bad")) snprintf(s, sizeof s, "The keys’ record is damaged: see System health."); else if (!jb(FST, "lib")) snprintf(s, sizeof s, "Not available: libfido2 is missing from this Rudolf OS.");
	else if (n) { if (req_) snprintf(s, sizeof s, "%d keys; your files need one of them.", n); else if (n == 1) snprintf(s, sizeof s, "1 key; asked for after your password."); else snprintf(s, sizeof s, "%d keys; asked for after your password.", n); } else snprintf(s, sizeof s, "None.");
	Cell *c = p_r2("Security keys"); p_label_named("fks", c_text(c, s, 400));
	for (int i = 0; i < n; i++) { RuiJ *k = rui_jat(keys, i); double last = jnum(k, "last", 0); char *lt = last ? set_date_str(last) : NULL;   /* (toLocaleDateString: the page's date) */
		char *t = g_strdup_printf("🔑 %s%s%s", set_jstr(k, "name", ""), lt ? " · last used " : "", lt ? lt : ""); g_free(lt); c_line(c, 0); GtkWidget *x = c_text(c, t, 400); c_dy(x, 4); g_free(t);
		GtkWidget *b = rui_button("Remove", 0); g_signal_connect(b, "clicked", G_CALLBACK(on_fkr), GINT_TO_POINTER((int)jnum(k, "i", i))); c_add(c, b, tw(" ", 13, 400), 0, 0); char nm[16]; snprintf(nm, sizeof nm, "fkr%d", i); p_label_named(nm, b); }
	c_line(c, 0); GtkWidget *a = rui_button("Add a security key…", 0); g_signal_connect(a, "clicked", G_CALLBACK(on_fka), NULL); gtk_widget_set_sensitive(a, ok && jb(FST, "lib") && n < 5); c_add(c, a, 0, 0, 0); p_label_named("fka", a);
	c_line(c, 0); GtkWidget *q = c_check(c, "Require a security key (protects your files)", req_, on_fkq, NULL); gtk_widget_set_sensitive(q, ok && !(n < 2 && !req_)); p_label_named("fkq", q);
	RuiJ *me = ac_at(MEI); const char *w = n < 2 ? "Add two keys (one as a spare, kept somewhere safe) to be able to require one: then a copy of your disk opens nothing without one of your keys and your password. A code from an authenticator app guards the sign-in only."
		: req_ ? (allowed(MEI) ? "Administrators can still open your files with their own copy (if you lose all your keys, that is the way back: docs/security-keys.md); with that turned off for your account, nobody can open them without one of your keys." : "If you lose all your keys, nobody can open your files.")
		: "Your files are still opened by your password alone until you require a key.";
	p_label_named("fkw", c_msg(c, w, 2));
	if (me && rui_jprop(me, "pk")) c_msg(c, "Your old security key is checked by the sign-in screen only, so it does not protect your files and Native and Lite cannot use it. Add your keys here again: the old one is then removed.", 2);
}

/* ================================================================== Fingerprints (fprint63.js fpRow: rudolfd's fp/...) */
static const char *const FPF[10][2] = { { "right-index", "Right index finger" }, { "left-index", "Left index finger" }, { "right-thumb", "Right thumb" }, { "left-thumb", "Left thumb" },
	{ "right-middle", "Right middle finger" }, { "left-middle", "Left middle finger" }, { "right-ring", "Right ring finger" }, { "left-ring", "Left ring finger" }, { "right-little", "Right little finger" }, { "left-little", "Left little finger" } };
static const char *fp_name(const char *f) { for (int i = 0; i < 10; i++) if (!strcmp(FPF[i][0], f)) return FPF[i][1]; return f; }
static RuiJ *fp_status(const char *id, int *st) { char p[200]; char *e = g_uri_escape_string(id ? id : "", NULL, FALSE); snprintf(p, sizeof p, "fp/status?acct=%s", e); g_free(e); return req(p, NULL, st); }
/* (fpSays: one plain sentence) */
static char *fp_says(RuiJ *s, int st, int self)
{
	if (!s || st != 200) return g_strdup("Needs the installed Rudolf OS."); if (!jb(s, "helper")) return g_strdup("Fingerprint readers are not in this Rudolf OS.");
	RuiJ *rds = rui_jprop(s, "readers"), *un = rui_jprop(s, "unsupported"), *rd = rds && rui_jlen(rds) ? rui_jat(rds, 0) : NULL, *u = un && rui_jlen(un) ? rui_jat(un, 0) : NULL;
	if (!rd && u) return g_strdup_printf("The fingerprint reader “%s” (USB %s) cannot be used: libfprint, which Rudolf OS reads fingerprints with, does not support it.", *set_jstr(u, "name", "") ? set_jstr(u, "name", "") : "Fingerprint reader", set_jstr(u, "usb", ""));
	if (!rd) return g_strdup("No fingerprint reader is connected. A touchpad or a touchscreen cannot read a fingerprint: a fingerprint reader is needed (in the power button, beside the keyboard, or one plugged into USB).");
	if (!jb(s, "disk")) return g_strdup_printf("%s not on this computer’s disk, so fingerprints cannot be kept with them.", self ? "Your files are" : "This account’s files are");
	RuiJ *fs = rui_jprop(s, "fingers"); int n = fs ? rui_jlen(fs) : 0; const char *why = set_jstr(s, "why", "");
	if (!n) return g_strdup_printf("%s. No fingerprints yet.", set_jstr(rd, "name", ""));
	return g_strdup_printf("%s. %d fingerprint%s%s", set_jstr(rd, "name", ""), n, n == 1 ? "" : "s", !strcmp(why, "tries") ? ". Five fingers were not recognised: the password is needed once." : !strcmp(why, "restart") ? ". After a restart or signing out, the password is asked first." : ". A finger unlocks the lock screen and answers administrator prompts; the password always works too.");
}
void usr_fp_add(int who); void usr_fp_remove(int who, const char *finger); void usr_fp_other(int who);
static void on_fpa(GtkButton *b, gpointer d) { (void)b; usr_fp_add(GPOINTER_TO_INT(d)); }
static void on_fpr(GtkButton *b, gpointer d) { (void)b; usr_fp_remove(MEI, d); }
static void on_fpra(GtkButton *b, gpointer d) { (void)b; usr_fp_remove(GPOINTER_TO_INT(d), NULL); }
static void on_fpo(GtkButton *b, gpointer d) { (void)b; usr_fp_other(GPOINTER_TO_INT(d)); }
static void fp_row(int adm)
{
	int st = 0; RuiJ *s = fp_status(ACCT, &st); char *t = fp_says(s, st, 1); Cell *c = p_r2("Fingerprints"); p_label_named("fps", c_msg_m(c, t, 5, 0, 0)); g_free(t);
	RuiJ *fs = rui_jprop(s, "fingers"); int n = s && st == 200 && fs ? rui_jlen(fs) : 0;
	for (int i = 0; i < n; i++) { const char *f = rui_jsval(rui_jat(fs, i)) ? rui_jsval(rui_jat(fs, i)) : ""; char *l = g_strdup_printf("☝️ %s", fp_name(f)); c_line(c, 0); GtkWidget *x = c_text(c, l, 400); c_dy(x, 4); g_free(l);
		GtkWidget *b = rui_button("Remove", 0); g_signal_connect(b, "clicked", G_CALLBACK(on_fpr), g_strdup(f)); c_add(c, b, tw(" ", 13, 400), 0, 0); char nm[16]; snprintf(nm, sizeof nm, "fpr%d", i); p_label_named(nm, b); }
	int can = s && st == 200 && jb(s, "helper") && rui_jprop(s, "readers") && rui_jlen(rui_jprop(s, "readers")) && jb(s, "disk");
	c_line(c, 0); GtkWidget *a = rui_button("Add a fingerprint…", 0); g_signal_connect(a, "clicked", G_CALLBACK(on_fpa), GINT_TO_POINTER(MEI)); gtk_widget_set_sensitive(a, can && jb(s, "open")); c_add(c, a, 0, 0, 0); p_label_named("fpa", a);
	if (n > 1) { GtkWidget *r = rui_button("Remove all", 0); g_signal_connect(r, "clicked", G_CALLBACK(on_fpra), GINT_TO_POINTER(MEI)); c_add(c, r, tw(" ", 13, 400), 0, 0); p_label_named("fpra", r); }
	p_label_named("fpw", c_msg(c, "", 2)); rui_jfree(s);
	if (adm) { int any = 0; for (int i = 0; i < NACC; i++) if (i != MEI && strcmp(set_jstr(ac_at(i), "r", ""), "guest")) any = 1;
		if (any) { p_r2_more(c); c = p_r2("Other accounts’ fingerprints");
			for (int i = 0, k = 0; i < NACC; i++) { if (i == MEI || !strcmp(set_jstr(ac_at(i), "r", ""), "guest")) continue; char *l = g_strdup_printf("%s…", an(i)); GtkWidget *b = rui_button(l, 0); g_free(l);
				g_signal_connect(b, "clicked", G_CALLBACK(on_fpo), GINT_TO_POINTER(i)); c_add(c, b, k++ ? tw(" ", 13, 400) : 0, 0, 0); char nm[16]; snprintf(nm, sizeof nm, "fpo%d", i); p_label_named(nm, b); } } }
}

/* ================================================================== Files and administrators (acct50.js acSection) */
typedef struct { GtkWidget *a, *ck, *rp; int i; char *as; } AcRow;
/* (.acr: flex, wrapping, gap 8, 3px 0, align-items center; the name's min-width 140) */
static double acrow_lay(void *d, double x, double y, double w)
{
	AcRow *r = d; char *t = g_strdup_printf("👤 %s%s", an(r->i), r->i == MEI ? " (you)" : ""); double nw = fmax(140, tw(t, 13, 400)); g_free(t);
	double it[4] = { nw, p_nat_w(r->ck), r->as ? tw(r->as, 12, 400) : 0, r->rp ? p_nat_w(r->rp) : -1 }, ih[4] = { 15, 19, r->as && *r->as ? 14 : 0, r->rp ? 23 : 0 };
	int n = r->rp ? 4 : 3; double lx = 0, ly = 0, lh = 0, ix[4], iy[4]; int ln[4], line = 0; double lhs[4] = { 0 };
	for (int k = 0; k < n; k++) { if (lx > 0 && lx + it[k] > w + .5) { ly += lh + 8; lhs[line] = lh; line++; lx = 0; lh = 0; } ix[k] = lx; ln[k] = line; lx += it[k] + 8; lh = fmax(lh, ih[k]); } lhs[line] = lh;
	double tops[4] = { 0 }; for (int q = 1; q <= line; q++) tops[q] = tops[q - 1] + lhs[q - 1] + 8;
	for (int k = 0; k < n; k++) iy[k] = 3 + tops[ln[k]] + (lhs[ln[k]] - ih[k]) / 2;
	double *pos = g_object_get_data(G_OBJECT(r->a), "pos"); if (!pos) { pos = g_new0(double, 8); g_object_set_data_full(G_OBJECT(r->a), "pos", pos, g_free); }
	pos[0] = ix[0]; pos[1] = iy[0]; pos[2] = ix[2]; pos[3] = iy[2];
	p_place(r->ck, x + ix[1], y + iy[1], 0, 0); if (r->rp) p_place(r->rp, x + ix[3], y + iy[3], 0, 0);
	double h = 3 + tops[line] + lhs[line] + 3 + 1; p_place(r->a, x, y, w, h); return h;
}
static gboolean acrow_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	AcRow *r = d; double W = gtk_widget_get_allocated_width(w) / s_z(), H = gtk_widget_get_allocated_height(w) / s_z(), *pos = g_object_get_data(G_OBJECT(w), "pos"); if (!pos) return FALSE;
	cairo_set_source_rgb(cr, 0xee / 255.0, 0xf0 / 255.0, 0xf3 / 255.0); cairo_rectangle(cr, 0, H - 1, W, 1); cairo_fill(cr);
	char *t = g_strdup_printf("👤 %s%s", an(r->i), r->i == MEI ? " (you)" : ""); s_col(cr, "fg", 1); s_text(cr, t, pos[0], pos[1], 13, 400, 0, 15); g_free(t);
	if (r->as && *r->as) { cairo_set_source_rgb(cr, 107 / 255.0, 114 / 255.0, 128 / 255.0); s_text(cr, r->as, pos[2], pos[3], 12, 400, 0, 14); } return FALSE;
}
static int AAI, AAON;
static void aa_go(int ok, void *d)
{
	(void)d; if (!ok) { redraw(); return; } int i = AAI, on = AAON;
	if (on && i == MEI) { ask("storgo usraa"); redraw(); return; }   /* (its own copies for the administrators: made by the page, which holds the file key) */
	acct_set(i, "aa", on ? "true" : "false"); if (!on) acct_set(i, "ak", NULL);
	char *l = g_strdup_printf("%s turned %s administrator access to %s's files", ME, on ? "on" : "off", an(i)); set_log_evt("sys", l); g_free(l); redraw();
}
static void on_aa(int on, void *d)
{
	AAI = GPOINTER_TO_INT(d); AAON = on; char *r = g_strdup_printf("%s%s", on ? "Let administrators open the files of " : "Stop administrators from opening the files of ", an(AAI)); set_note("elevate %s", r); rui_elevate(SW, r, aa_go, NULL); g_free(r);
}
static void on_rp(GtkButton *b, gpointer d) { (void)b; char a[120]; snprintf(a, sizeof a, "storgo usrreset %s", set_jstr(ac_at(GPOINTER_TO_INT(d)), "id", "")); ask(a); }   /* (acResetDlg: the page's, with its keys) */
static void files_sec(int adm)
{
	p_h4("Files and administrators"); char *t = g_strdup_printf("Your files: %s%s", allowed(MEI) ? "you and the administrators of this computer can open them." : "only you can open them.", adm ? "" : " Only an administrator can change this.");
	p_label_named("acme", p_msg_m(t, 0, 2, 8)); g_free(t); if (!adm) return;
	for (int i = 0; i < NACC; i++) { AcRow *r = g_new0(AcRow, 1); r->i = i; RuiJ *a = ac_at(i), *ak = rui_jprop(a, "ak"); int self = i == MEI, has = ak && rui_jprop(ak, ACCT);
		r->as = g_strdup(self || !allowed(i) ? "" : has ? "" : "from their next sign-in");
		r->a = gtk_drawing_area_new(); g_signal_connect(r->a, "draw", G_CALLBACK(acrow_draw), r); p_put(r->a);
		r->ck = rui_check("Administrators can open this account's files", allowed(i), on_aa, GINT_TO_POINTER(i)); p_put(r->ck); char nm[16]; snprintf(nm, sizeof nm, "aa%d", i); p_label_named(nm, r->ck);
		if (!self) { r->rp = rui_button("Reset password…", 0); g_signal_connect(r->rp, "clicked", G_CALLBACK(on_rp), GINT_TO_POINTER(i)); p_put(r->rp); snprintf(nm, sizeof nm, "rp%d", i); p_label_named(nm, r->rp); }
		p_custom(acrow_lay, r, 0, 0); if (i == 0) p_custom_more("Administrators can open this account's files"); }
}

/* ================================================================== Developer: publisher key (devkey59.js; pubkey:NAME) */
/* (dkMakeUI, dkImportUI, dkShowPrivate: the page's, in a page of one action: the private half is never here) */
static void on_dkm(GtkButton *b, gpointer d) { (void)b; (void)d; ask("storgo dkmake"); }
static void on_dki(GtkButton *b, gpointer d) { (void)b; (void)d; ask("storgo dkuse"); }
static void on_dks(GtkButton *b, gpointer d) { (void)b; (void)d; ask("storgo dkshow"); }
static void on_dkc(GtkButton *b, gpointer d) { (void)d; gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), g_object_get_data(G_OBJECT(b), "pub"), -1); set_notify("Copied", "The public key", 0); set_note("copied the public key"); }
static char *uname_(void);
static void on_dkf(GtkButton *b, gpointer d) { (void)b; (void)d; char *u = uname_(), k[120]; snprintf(k, sizeof k, "pubkey:%s", u); g_free(u); set_ls_del(k); set_note("svj %s (removed)", k); redraw(); }
static char *uname_(void) { char *u = g_utf8_strup(ME, -1); for (char *p = u; *p; p++) if (strchr("\\/:*?\"<>|", *p)) *p = '_'; if (g_utf8_strlen(u, -1) > 40) *g_utf8_offset_to_pointer(u, 40) = 0; return u; }
static void dev_sec(void)
{
	p_h4("Developer: publisher key"); char *u = uname_(), k[120]; snprintf(k, sizeof k, "pubkey:%s", u); RuiJ *o = set_ls_get(k), *v = X(o); int has = v && rui_jtype(v) == 5;
	if (has) { Cell *c = p_r2("Public key"); GtkWidget *e = rui_entry(NULL); gtk_entry_set_text(GTK_ENTRY(e), set_jstr(v, "pub", "")); gtk_editable_set_editable(GTK_EDITABLE(e), FALSE); c_add(c, e, 0, 0, 1); p_label_named("dkp", e);
		char *m = g_strdup_printf("Fingerprint %s · made %s", set_jstr(v, "fp", ""), set_jstr(v, "made", "")); c_msg(c, m, 2); g_free(m);
		static const char *const L[3] = { "Copy the public key", "Show the private key…", "Forget it here" }; GCallback cb[3] = { G_CALLBACK(on_dkc), G_CALLBACK(on_dks), G_CALLBACK(on_dkf) }; GtkWidget *row = p_buttons(L, 3, cb, NULL); g_object_set_data_full(G_OBJECT(row), "pub", g_strdup(set_jstr(v, "pub", "")), g_free); p_label_named("dkc", row); }
	else { p_msg_m("A publisher key signs the games and Rudolf OS builds you make (see docs/custom-builds.md). Its public half goes in Developer mode or in a Library developer certificate.", 0, 10, 13);
		static const char *const L[2] = { "Make a publisher key…", "Use a backup…" }; GCallback cb[2] = { G_CALLBACK(on_dkm), G_CALLBACK(on_dki) }; GtkWidget *row = p_buttons(L, 2, cb, NULL); gtk_style_context_add_class(gtk_widget_get_style_context(row), "pri"); p_label_named("dkm", row); }
	rui_jfree(o); g_free(u);
}

static gboolean usr_focus(GtkWidget *w, GdkEventFocus *e, gpointer d)   /* (1.6.3 PR 8: a dialog's window closed: the page draws again) */
{ (void)w; (void)e; (void)d; if (!strcmp(set_page(), "usr")) redraw(); return FALSE; }
static void usr_watch_focus(void) { static int on; if (on || !SW) return; on = 1; g_signal_connect(SW->win, "focus-in-event", G_CALLBACK(usr_focus), NULL); }

void page_usr(void)
{
	accts_load(); usr_watch_focus(); p_h3("User Settings");
	if (!strcmp(ROLE, "guest")) { p_msg_m("\x03You are using a <b>guest session</b>. Nothing you do is kept after you sign out. Ask an administrator for an account to keep your files.", 5, 0, 0); return; }
	int adm = set_admin(); Who *w = g_new0(Who, 1); w->a = gtk_drawing_area_new(); g_signal_connect(w->a, "draw", G_CALLBACK(who_draw), w); p_put(w->a); p_custom(who_lay, w, 0, 6);
	account_secs(adm); signin_sec(adm); if (adm) family_sec(); otp_row(); fido_row(); fp_row(adm); files_sec(adm); dev_sec();
	p_footer(usr_dirty, foot); p_footer_noreset();
}

/* (the actions that ask rudolfd with a password: the page's own dialogs (fido61.js's fidoAsk, fprint63.js's fpEnroll and
   fpOther), run in a page of one action, #app=do-storgo&w=..., as Security's are: the page holds the file key, and its
   dialogs are the ones tested there. The page draws again when that window closes.) */
void usr_fido_add(void) { ask("storgo fidoadd"); }
void usr_fido_remove(int i) { char a[40]; snprintf(a, sizeof a, "storgo fidorm %d", i); ask(a); }
void usr_fido_require(int on) { ask(on ? "storgo fidoreq 1" : "storgo fidoreq 0"); }
void usr_fp_add(int who) { char a[160]; snprintf(a, sizeof a, "storgo fpadd %s", set_jstr(ac_at(who), "id", "")); ask(a); }
void usr_fp_remove(int who, const char *f) { (void)who; char a[64]; if (f && *f) snprintf(a, sizeof a, "storgo fprm %s", f); else snprintf(a, sizeof a, "storgo fprm"); ask(a); }
void usr_fp_other(int who) { char a[160]; snprintf(a, sizeof a, "storgo fpother %s", set_jstr(ac_at(who), "id", "")); ask(a); }
