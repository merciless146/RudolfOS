/*
 * rudolf-settings: Oracle (signin26.js signinPanel, as settings31.js puts it with Oracle's Web search and Downloads; oraenc60.js's
 * Website data; orasleep63.js's Memory). 1.6.3, docs/plan-1.6.3.md Part 3 (PR 6). As the page: Real browser tabs (rudolfd's shell,
 * shell?on=, shell/switch), When a page asks for a password (siAsk), Your sign-in sites (webSites, siAdd and siRemove), the built-in
 * sites (web25.js's WEB_SITES, SI_BANKS), the Web search and Downloads rows and their footer (settings_net.c's, one working copy with
 * Network Settings), Keep my websites encrypted with my account (desktop/owenc, its password asked to turn it off), and Free tabs you
 * have not looked at for (oraFree). All of them kept as the page keeps them (persistS, bd95net, rudolfd). GPL-2.0-or-later.
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

static RuiJ *SH;           /* rudolfd's shell */
static GtkWidget *SHMW, *SIM, *SIN;
static int SAVED_SHELL;
static gboolean sim_draw(GtkWidget *w, cairo_t *cr, gpointer d);
static int open_list;      /* the built-in list's <details> open */

/* ---- the account's settings (SC): webSites, siAsk, oraFree, kept as persistS keeps them */
static RuiJ *sites(RuiJ *st) { RuiJ *a = rui_jprop(st, "webSites"); return rui_jtype(a) == 4 ? a : NULL; }
static void keep(const char *k, const char *json) { const char *K[1] = { k }, *V[1] = { json }; set_store(K, V, 1); set_note("keep %s %s", k, json); }
static void sites_save(GPtrArray *l)
{
	GString *b = g_string_new("["); for (guint i = 0; i < l->len; i++) { char *e = set_jesc(g_ptr_array_index(l, i)); g_string_append_printf(b, "%s\"%s\"", i ? "," : "", e); g_free(e); } g_string_append_c(b, ']');
	keep("webSites", b->str); g_string_free(b, TRUE);
}
static GPtrArray *sites_list(void)
{
	GPtrArray *l = g_ptr_array_new_with_free_func(g_free); RuiJ *st = set_stored(), *a = sites(st);
	for (int i = 0; i < rui_jlen(a); i++) if (rui_jtype(rui_jat(a, i)) == 3) g_ptr_array_add(l, g_strdup(rui_jsval(rui_jat(a, i)))); rui_jfree(st); return l;
}
/* (siDomain: the site a host name belongs to, mail.example.co.uk → example.co.uk) */
static char *si_domain(const char *h)
{
	char *l = g_ascii_strdown(h ? h : "", -1), *s = l; if (g_str_has_prefix(s, "www.")) s += 4; char **p = g_strsplit(s, ".", -1); GPtrArray *a = g_ptr_array_new();
	for (int i = 0; p[i]; i++) if (*p[i]) g_ptr_array_add(a, p[i]); int n = a->len; char *r;
	if (n < 3) { GString *g = g_string_new(NULL); for (int i = 0; i < n; i++) g_string_append_printf(g, "%s%s", i ? "." : "", (char *)a->pdata[i]); r = g_string_free(g, FALSE); }
	else { const char *sld = a->pdata[n - 2]; static const char *SLD[] = { "co", "com", "net", "org", "gov", "ac", "edu", "ne", "or", "go" }; int is = 0; for (guint k = 0; k < G_N_ELEMENTS(SLD); k++) is |= !strcmp(sld, SLD[k]);
		int take = is && strlen(a->pdata[n - 1]) == 2 ? 3 : 2; GString *g = g_string_new(NULL); for (int i = n - take; i < n; i++) g_string_append_printf(g, "%s%s", i > n - take ? "." : "", (char *)a->pdata[i]); r = g_string_free(g, FALSE); }
	g_ptr_array_unref(a); g_strfreev(p); g_free(l); return r;
}
static int dom_ok(const char *d)
{
	/* (/^[a-z0-9.-]+\.[a-z0-9-]+$/) */
	const char *dot = strrchr(d, '.'); if (!dot || dot == d || !dot[1]) return 0;
	for (const char *p = d; p < dot; p++) if (!(g_ascii_islower(*p) || g_ascii_isdigit(*p) || *p == '.' || *p == '-')) return 0;
	for (const char *p = dot + 1; *p; p++) if (!(g_ascii_islower(*p) || g_ascii_isdigit(*p) || *p == '-')) return 0; return 1;
}
static void on_add(GtkWidget *w, gpointer d)
{
	(void)w; (void)d; char *v = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(SIN))));
	if (!g_ascii_strncasecmp(v, "http://", 7) || !g_ascii_strncasecmp(v, "https://", 8)) { const char *s = strstr(v, "://") + 3; size_t n = strcspn(s, "/?#:"); char *x = g_strndup(s, n); g_free(v); v = x; }
	char *low = g_ascii_strdown(v, -1), *dm = si_domain(low); g_free(v); g_free(low);
	if (!*dm || !dom_ok(dm)) { g_object_set_data_full(G_OBJECT(SIM), "t", g_strdup(rui_tr("That is not a web address such as example.com.")), g_free); gtk_widget_queue_draw(SIM); set_note("sim bad"); set_state_soon(); g_free(dm); return; }
	GPtrArray *l = sites_list(); int has = 0; for (guint i = 0; i < l->len; i++) has |= !strcmp(g_ptr_array_index(l, i), dm);
	for (guint i = 0; i < G_N_ELEMENTS(WEBSITES); i++) { size_t a = strlen(dm), b = strlen(WEBSITES[i]); has |= !strcmp(dm, WEBSITES[i]) || (a > b && dm[a - b - 1] == '.' && !strcmp(dm + a - b, WEBSITES[i])); }
	if (!has) { g_ptr_array_add(l, g_strdup(dm)); sites_save(l); }
	g_ptr_array_unref(l); g_free(dm); set_show_keep("web");
}
static void on_rm(GtkButton *b, gpointer d)
{
	(void)b; GPtrArray *l = sites_list(), *o = g_ptr_array_new_with_free_func(g_free); for (guint i = 0; i < l->len; i++) if (strcmp(g_ptr_array_index(l, i), d)) g_ptr_array_add(o, g_strdup(g_ptr_array_index(l, i)));
	sites_save(o); g_ptr_array_unref(l); g_ptr_array_unref(o); set_show_keep("web");
}
static void on_ask(int on, void *d) { (void)d; keep("siAsk", on ? "true" : "false"); }

/* ---- Real browser tabs (rudolfd's shell) */
static const char *shell_text(void)
{
	if (SAVED_SHELL) return "Saved: this takes effect when the desktop starts again.";
	RuiJ *s = SH; if (!s) return "Checking…"; if (!*set_jstr(s, "id", "")) return "Checking…";
	RuiJ *on = rui_jprop(s, "on"); if (on && rui_jtype(on) == 1 && !rui_jval(on)) return "Off: websites in Oracle are shown the 1.1 way, and sites that need a sign-in open in the browser window.";
	if (rui_jbool(s, "active")) return "On: Oracle opens websites as real tabs."; if (rui_jbool(s, "failed")) return "The real tabs did not start, so the desktop runs without them until Rudolf OS starts again.";
	if (!rui_jbool(s, "installed")) return "Being installed in the background…"; if (!rui_jbool(s, "ready")) return "Getting your files ready for the move…"; return "Ready: starts with the desktop next time.";
}
static void on_shell(int on, void *d)
{
	(void)d; char m[300]; char *out = NULL; int c = rui_http("POST", on ? "/api/shell?on=1" : "/api/shell?on=0", NULL, 0, &out, NULL); RuiJ *j = out ? rui_jparse(out) : NULL; set_note("POST /api/shell?on=%d %d", on, c);
	snprintf(m, sizeof m, "%s", set_jstr(j, "error", "rudolfd did not answer")); rui_jfree(j); g_free(out);
	if (c == 200) { SAVED_SHELL = 1; p_text_set(SHMW, rui_tr(shell_text())); } else p_text_set(SHMW, m); set_state_soon();
}
static gboolean switch_now(gpointer d) { (void)d; char *out = NULL; rui_http("POST", "/api/shell/switch", NULL, 0, &out, NULL); g_free(out); set_note("POST /api/shell/switch"); return G_SOURCE_REMOVE; }
static void on_restart(GtkButton *b, gpointer d) { (void)b; (void)d; g_timeout_add(500, switch_now, NULL); }

/* ---- the built-in list (<details>, not a .sp25 one: no border; its summary 400; the list a .msg of line-height 1.6) */
static char *builtin_text(void)
{
	GString *g = g_string_new(NULL); for (guint i = 0; i < G_N_ELEMENTS(WEBSITES); i++) g_string_append_printf(g, "%s%s", g->len ? " · " : "", WEBSITES[i]);
	for (guint i = 0; i < G_N_ELEMENTS(SIBANKS); i++) g_string_append_printf(g, " · %s", SIBANKS[i]); g_string_append_printf(g, "\n%s", rui_tr("Also any site whose name ends in “bank” or is a credit union.")); return g_string_free(g, FALSE);
}
static double bl_lh(void) { return 20.8; }
static double bl_h(double w) { if (!open_list) return 15; char *t = builtin_text(); int n = s_lines(t, 13, 400, w, NULL); g_free(t); return 15 + 6 + n * bl_lh(); }
static double bl_lay(void *d, double x, double y, double w) { double h = bl_h(w); p_place(d, x, y, w, h); return h; }
static gboolean bl_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)d; double z = s_z(), Wd = gtk_widget_get_allocated_width(w) / z; char *s = g_strdup_printf(rui_tr("Built-in sign-in sites, banks and payment sites (%u)"), (unsigned)(G_N_ELEMENTS(WEBSITES) + G_N_ELEMENTS(SIBANKS)));
	s_col(cr, "fg", 1); double ty = s_lh(13, 400) / 2; cairo_save(cr); cairo_scale(cr, z, z); cairo_new_path(cr);
	if (open_list) { cairo_move_to(cr, 0, ty - 2.5); cairo_line_to(cr, 8, ty - 2.5); cairo_line_to(cr, 4, ty + 4); } else { cairo_move_to(cr, .5, ty - 4); cairo_line_to(cr, 7, ty); cairo_line_to(cr, .5, ty + 4); }
	cairo_close_path(cr); cairo_fill(cr); cairo_restore(cr); s_text(cr, s, 14, 0, 13, 400, 0, s_lh(13, 400)); g_free(s);
	if (open_list) { char *t = builtin_text(); s_col(cr, "msg", 1); s_text(cr, t, 0, 15 + 6, 13, 400, Wd, bl_lh()); g_free(t); }
	return TRUE;
}
static gboolean bl_press(GtkWidget *w, GdkEventButton *e, gpointer d) { (void)d; if (e->button != 1 || e->type != GDK_BUTTON_PRESS || e->y / s_z() > 15) return FALSE; open_list = !open_list; set_note("details builtin %d", open_list); p_relayout(); gtk_widget_queue_draw(w); set_state_soon(); return TRUE; }

/* ---- a site of yours (a flex row: its name, Remove; 3px above and below) */
static double row_lay(void *d, double x, double y, double w) { GtkWidget *b = g_object_get_data(G_OBJECT(d), "b"); double bh = p_nat_h(b), bw = p_nat_w(b); p_place(d, x, y, w - bw - 8, bh + 6); p_place(b, x + w - bw, y + 3, 0, 0); return bh + 6; }
static gboolean row_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)d; const char *t = g_object_get_data(G_OBJECT(w), "t"); double H = gtk_widget_get_allocated_height(w) / s_z(); s_col(cr, "fg", 1); s_text(cr, t, 0, (H - 15) / 2, 13, 400, 0, 15); return TRUE; }
/* ---- the add row (.rmrow: the field 240px + its padding, Add, the message 10px down) */
static double add_lay(void *d, double x, double y, double w) { (void)w; GtkWidget *b = g_object_get_data(G_OBJECT(d), "b"); double h = 28; p_place(d, x, y, 254, h); p_place(b, x + 254 + 8, y, 0, h); p_place(SIM, x + 254 + 8 + p_nat_w(b) + 8, y + 10, MAX(1, w - 254 - 16 - p_nat_w(b)), 18); return h; }

static gboolean sim_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)d; const char *t = g_object_get_data(G_OBJECT(w), "t"); if (!t) return TRUE; s_col(cr, "msg", 1); s_text(cr, t, 0, 0, 13, 400, 0, s_lh(13, 400)); return TRUE; }
/* ---- Website data (oraenc60.js: desktop/owenc) */
static GtkWidget *OWEC;
static char *owe_eng(void) { const char *e = set_engine_now(); return g_strdup(!strcmp(e, "native") ? "native" : !strcmp(e, "webkit") ? "lite" : "std"); }
typedef struct { int want; } Owe;
static void owe_post(int want, const char *pw)
{
	char *a = set_jesc(ACCT), *p = pw ? set_jesc(pw) : NULL, *b = p ? g_strdup_printf("{\"acct\":\"%s\",\"on\":%s,\"p\":\"%s\"}", a, want ? "true" : "false", p) : g_strdup_printf("{\"acct\":\"%s\",\"on\":%s}", a, want ? "true" : "false");
	char *out = NULL; int c = rui_http("POST", "/api/desktop/owenc", b, strlen(b), &out, NULL); RuiJ *j = out ? rui_jparse(out) : NULL; set_note("POST /api/desktop/owenc %d", c);
	if (c == 200) { char *l = g_strdup_printf("%s %s keeping websites encrypted", ME, want ? "turned on" : "turned off"); set_log_evt("sys", l); g_free(l);
		set_notify("Oracle", want ? "Your websites will be kept encrypted with your account from the next time Oracle opens." : "Your websites will be kept the shared way from the next time Oracle opens.", 0); }
	else set_notify("Oracle", set_jstr(j, "error", "That did not work."), 1);
	explicit_bzero(b, strlen(b)); if (p) { explicit_bzero(p, strlen(p)); g_free(p); } g_free(a); g_free(b); rui_jfree(j); g_free(out); set_show_keep("web");
}
static void on_owe(int on, void *d)
{
	(void)d; if (on) { owe_post(1, NULL); return; }
	/* (askText: the account's own password, rudolfd checks it) */
	GtkWidget *dl = gtk_dialog_new_with_buttons(rui_tr("Turn off encrypted websites: your password"), GTK_WINDOW(SW->win), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, rui_tr("Cancel"), GTK_RESPONSE_CANCEL, rui_tr("OK"), GTK_RESPONSE_OK, NULL);
	GtkWidget *e = rui_entry(NULL); gtk_entry_set_visibility(GTK_ENTRY(e), FALSE); gtk_entry_set_activates_default(GTK_ENTRY(e), TRUE); gtk_dialog_set_default_response(GTK_DIALOG(dl), GTK_RESPONSE_OK);
	GtkWidget *ca = gtk_dialog_get_content_area(GTK_DIALOG(dl)); gtk_container_set_border_width(GTK_CONTAINER(ca), 12); gtk_box_pack_start(GTK_BOX(ca), e, FALSE, FALSE, 0); gtk_widget_set_size_request(e, (int)lround(320 * s_z()), -1);
	gtk_widget_show_all(dl); p_label_named("asktext", e); set_note("ask text owenc"); set_state_soon();
	if (gtk_dialog_run(GTK_DIALOG(dl)) == GTK_RESPONSE_OK) { char *p = g_strdup(gtk_entry_get_text(GTK_ENTRY(e))); gtk_widget_destroy(dl); owe_post(0, p); explicit_bzero(p, strlen(p)); g_free(p); }
	else { gtk_widget_destroy(dl); set_show_keep("web"); }
}
static char *owe_off_text(const char *eng, RuiJ *wk)
{
	const char *where = !strcmp(eng, "std") ? "profile" : "data"; const char *st = set_jstr(wk, "state", ""), *why = set_jstr(wk, "why", "");
	char *w = g_strdup_printf("Off: Oracle's websites are kept in this computer's shared browser %s", where), *r;
	if (!strcmp(eng, "lite") || !*st || !strcmp(st, "off")) r = g_strdup_printf("%s, and they are not encrypted: someone with this computer's drive could read them.", w);
	else if (!strcmp(st, "tpm")) { char *why2 = g_strdup(why); char *p = strstr(why2, " ("); if (p) *p = 0;
		r = g_strdup_printf("%s, encrypted with a key kept in this computer's security chip (TPM) and tied to its Secure Boot: this computer's drive alone cannot read them, nor can another system started on it. Anyone who can sign in on this computer can still use them.%s%s%s", w,
			*why && !g_str_has_prefix(why, "the first time") ? " A new key was made at this start (" : "", *why && !g_str_has_prefix(why, "the first time") ? why2 : "", *why && !g_str_has_prefix(why, "the first time") ? "): what was kept under the previous one could not be read, so websites asked you to sign in again." : ""); g_free(why2); }
	else if (!strcmp(st, "none")) r = g_strdup_printf("%s, with the browser's built-in key only: this computer has no TPM security chip (or it is turned off in its firmware's settings), so someone with this computer's drive could read them.", w);
	else r = g_strdup_printf("%s, with the browser's built-in key at this start: the TPM security chip did not give its key%s%s%s, so someone with this computer's drive could read what is kept now.", w, *why ? " (" : "", why, *why ? ")" : "");
	g_free(w); return r;
}
static int acct_disk(void)
{
	RuiJ *u = set_ls_get("bd95user"), *a = rui_jprop(rui_jprop(u, "x"), "accts"); int r = 0; for (int i = 0; i < rui_jlen(a); i++) if (!strcmp(set_jstr(rui_jat(a, i), "id", ""), ACCT)) r = rui_jbool(rui_jat(a, i), "disk"); rui_jfree(u); return r;
}
static void owe_part(void)
{
	if (!*ACCT || !strcmp(ROLE, "guest")) return; char *eng = owe_eng(), p[160]; char *ea = g_uri_escape_string(ACCT, NULL, FALSE); snprintf(p, sizeof p, "/api/desktop/owenc?acct=%s", ea); g_free(ea);
	RuiJ *st = rui_get(p), *wk = rui_get("/api/webkey"); int ok = st && rui_jbool(st, "ok") && rui_jbool(st, "open"); RuiJ *o = rui_jprop(st, "on");
	int on = o && rui_jtype(o) == 1 ? (int)rui_jval(o) : strcmp(eng, "std") != 0;
	p_h4("Website data");
	Cell *c = p_r2("Encryption"); OWEC = c_check(c, "Keep my websites encrypted with my account", ok && on, on_owe, NULL); p_label_named("owec", OWEC); if (!ok) gtk_widget_set_sensitive(OWEC, FALSE);
	char *m = !ok ? g_strdup(acct_disk() ? "Your files are not open on this computer's disk right now, so this cannot be changed." : "Only for accounts whose files are kept on this computer's disk.")
		: on ? g_strdup_printf("On: Oracle's websites (your sign-ins, site data, cache and saved site passwords) are encrypted with your account's key and locked when you sign out.%s", !strcmp(eng, "std") ? " On this desktop Oracle then opens in a window of its own, over the desktop, and uses a little more memory while it is open." : "") : owe_off_text(eng, wk);
	c_msg(c, m, 0); c_msg(c, ok ? "A change applies the next time Oracle opens, and websites ask you to sign in again. It protects your websites while you are signed out; anyone using your account while it is signed in can still use them." : "", 0);
	g_free(m); g_free(eng); rui_jfree(st); rui_jfree(wk);
}
/* ---- Memory (orasleep63.js: oraFree, kept at once) */
static void on_free(const char *v, void *d)
{
	(void)d; int n = atoi(v); keep("oraFree", v); static const char *L[][2] = { { "15", "15 minutes" }, { "30", "30 minutes" }, { "60", "1 hour" }, { "120", "2 hours" }, { "0", "never" } };
	const char *t = v; for (guint i = 0; i < G_N_ELEMENTS(L); i++) if (!strcmp(L[i][0], v)) t = L[i][1]; char *l = g_strdup_printf("%s set Oracle to free tabs not looked at for %s", *ME ? ME : "Someone", t); set_log_evt("sys", l); g_free(l); (void)n;
}

void page_web(void)
{
	rui_jfree(SH); SH = rui_get("/api/shell"); RuiJ *st = set_stored();
	p_h3("Oracle");
	p_h4("Real browser tabs");
	{ Cell *c = p_r2("Oracle's tabs"); RuiJ *on = rui_jprop(SH, "on"); GtkWidget *ck = c_check(c, "Open websites as real browser tabs (signing in works inside Oracle)", SH ? (on && rui_jtype(on) == 1 && rui_jval(on)) : 1, on_shell, NULL); p_label_named("sishell", ck);
		SHMW = c_msg(c, shell_text(), 0); p_label_named("sishm", SHMW);
		const char *L[1] = { "Restart the desktop now" }; GCallback C[1] = { G_CALLBACK(on_restart) }; c_buttons(c, L, 1, C, NULL); }
	p_msg_m("Websites in Oracle are shown inside the desktop, where they cannot keep you signed in. Sites that need a sign-in open in a real browser window instead (🌐 in the taskbar switches between it and the desktop).", 0, 10, 13);
	{ Cell *c = p_r2("When a page asks for a password"); RuiJ *a = rui_jprop(st, "siAsk"); GtkWidget *ck = c_check(c, "Ask first (otherwise the site moves to the browser window by itself)", !(a && rui_jtype(a) == 1 && !rui_jval(a)), on_ask, NULL); p_label_named("siask", ck);
		c_msg(c, "Sending a sign-in, or a “Sign in with Google / Microsoft / Apple…” button, always moves the site.", 0); }
	p_h4("Your sign-in sites");
	{ RuiJ *a = sites(st); int n = 0;
		for (int i = 0; i < rui_jlen(a); i++) { if (rui_jtype(rui_jat(a, i)) != 3) continue; const char *d = rui_jsval(rui_jat(a, i)); GtkWidget *t = gtk_drawing_area_new(), *b = rui_button("Remove", 0);
			g_object_set_data_full(G_OBJECT(t), "t", g_strdup(d), g_free); g_object_set_data(G_OBJECT(t), "b", b); g_signal_connect(t, "draw", G_CALLBACK(row_draw), NULL); g_object_set_data_full(G_OBJECT(b), "d", g_strdup(d), g_free);
			g_signal_connect(b, "clicked", G_CALLBACK(on_rm), g_object_get_data(G_OBJECT(b), "d")); p_put(t); p_put(b); char nm[16]; snprintf(nm, sizeof nm, "sirm%d", n++); p_label_named(nm, b); p_custom(row_lay, t, 0, 0); }
		if (!n) p_msg("None yet. Choose “Always for this site” when Oracle asks, or add one here.", 0); }
	{ SIN = rui_entry("example.com"); GtkWidget *b = rui_button("Add", 0); SIM = gtk_drawing_area_new(); g_object_set_data(G_OBJECT(SIN), "b", b); g_signal_connect(b, "clicked", G_CALLBACK(on_add), NULL); g_signal_connect(SIN, "activate", G_CALLBACK(on_add), NULL);
		g_signal_connect(SIM, "draw", G_CALLBACK(sim_draw), NULL); p_put(SIN); p_put(b); p_put(SIM); p_label_named("sin", SIN); p_label_named("sia", b); p_custom(add_lay, SIN, 14, 0); }
	{ GtkWidget *d = gtk_drawing_area_new(); gtk_widget_add_events(d, GDK_BUTTON_PRESS_MASK); g_signal_connect(d, "draw", G_CALLBACK(bl_draw), NULL); g_signal_connect(d, "button-press-event", G_CALLBACK(bl_press), NULL); p_put(d); p_label_named("sibuilt", d); p_custom(bl_lay, d, 10, 0); }
	/* (settings31.js: Network Settings' Web search and Downloads, with their headings and words, #nsm and the footer) */
	p_footer_box();
	p_h4("Web search (Oracle)");
	net_search_rows("o", 1);
	p_msg_m("Oracle asks the search service directly over HTTPS. Many services do not allow browser requests; the relay forwards the request through another server as a workaround, so it can see what you search for. Requests obey the firewall.", 1, 0, 0);
	p_h4("Downloads");
	net_search_rows("o", 2);
	net_search_msg();
	if (set_admin()) { p_footer(net_dirty, net_foot); p_footer_here(); }
	owe_part();
	p_h4("Memory");
	{ RuiJ *f = rui_jprop(st, "oraFree"); int v = f && rui_jtype(f) == 2 ? (int)rui_jval(f) : 30; if (v != 15 && v != 30 && v != 60 && v != 120 && v != 0) v = 30; char cur[8]; snprintf(cur, sizeof cur, "%d", v);
		const char *vv[5] = { "15", "30", "60", "120", "0" }, *ll[5] = { "15 minutes", "30 minutes", "1 hour", "2 hours", "Never" };
		Cell *c = p_r2("Free tabs you have not looked at for"); GtkWidget *s = s_select(vv, ll, 5, cur, on_free, NULL); c_add(c, s, 0, 0, 0); p_label_named("ofreesel", s);
		c_msg(c, "A tab that is not the one shown in its window closes its page after that time and keeps its address (💤 on the tab); choosing it loads it again where it was. Tabs playing sound or video, or holding text you have typed and not sent, are never freed. When memory runs low, the tabs you are not looking at are freed at once (unless this is Never).", 0); }
	rui_jfree(st); set_note("web %s", SH ? set_jstr(SH, "id", "") : "none");
}
