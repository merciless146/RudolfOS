/*
 * rudolf-settings: Security Settings (base_v24.html secPanelS, with sec50.js's Firewall, vault50.js's Data encryption, sb28.js's Secure
 * Boot, devmode59.js's Developer mode, devlink40.js's Development link, sc60.js's Smart cards and otp60.js's Two-step sign-in).
 * 1.6.3, docs/plan-1.6.3.md Part 3 (PR 8). As the page:
 * - Website blocking, Antivirus, System protection: a working copy of bd95sec, applied with the administrator's own password typed in
 *   Confirm (checked by rudolfd, desktop/admin, as the page checks it) from the footer; the activity log (bd95seclog) and quarantine
 *   (bd95quar) from the page's storage; Scan now and a quarantined file's Restore done by the hidden page ("stor avscan", "stor qrestore").
 * - The firewall (firewall?on=), the camera-like switches: the administrator's prompt (rui_elevate) first.
 * - What the page does in its own dialogs (encrypting the data, the recovery key, Secure Boot's preparation and its one-time password,
 *   Developer mode, the development link's log, installing the DoD's certificates) opens in a page of one action ("storgo W", as Storage and backup's).
 * - The development link's Allow and Switch on…, the DoD certificates' Remove: the administrator's name and password sent with the
 *   request (rui_elevate_cred), as the page's elevate({cred}). The token typed is sent once and cleared here.
 * GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "settings.h"

static double tw(const char *t, double px, int wt) { double w = 0; s_lines(t ? t : "", px, wt, 0, &w); return w; }
static void redraw(void) { set_show_keep("sec"); }
static void ask(const char *a) { rui_shell(SW, a); set_note("%s", a); }
static void go(const char *w) { char a[80]; snprintf(a, sizeof a, "storgo %s", w); ask(a); }
static int jb(RuiJ *o, const char *k) { RuiJ *v = o ? rui_jprop(o, k) : NULL; return v && ((rui_jtype(v) == 1 && rui_jval(v)) || (rui_jtype(v) == 2 && rui_jval(v)) || (rui_jtype(v) == 3 && *rui_jsval(v))); }
static double jnum(RuiJ *o, const char *k) { RuiJ *v = o ? rui_jprop(o, k) : NULL; return v && rui_jtype(v) == 2 ? rui_jval(v) : NAN; }
static RuiJ *get(const char *path) { char p[300]; snprintf(p, sizeof p, "/api/%s", path); return rui_get(p); }
static RuiJ *post_body(const char *path, const char *body, char *err, size_t en)
{
	char p[600], *out = NULL; snprintf(p, sizeof p, "/api/%s", path); int c = rui_http("POST", p, body, body ? strlen(body) : 0, &out, NULL); RuiJ *j = out ? rui_jparse(out) : NULL; g_free(out); set_note("POST %s %d", path, c);
	if (c != 200 || !j) { if (err) snprintf(err, en, "%s", j ? set_jstr(j, "error", "Request failed") : "rudolfd did not answer"); rui_jfree(j); return NULL; } return j;
}
static RuiJ *X(RuiJ *o) { return o ? rui_jprop(o, "x") : NULL; }
static char *jstr_esc(const char *s) { char *e = set_jesc(s ? s : ""), *r = g_strdup_printf("\"%s\"", e); g_free(e); return r; }
static char *cred_body(const char *n, const char *p, const char *more) { char *a = jstr_esc(n), *b = jstr_esc(p), *r = g_strdup_printf("{\"n\":%s,\"p\":%s%s}", a, b, more ? more : ""); explicit_bzero(b, strlen(b)); g_free(a); g_free(b); return r; }
static void wipe(char *s) { if (s) { explicit_bzero(s, strlen(s)); g_free(s); } }
/* (the page's fmtT: sysDateStr and the time in the settings' zone, "9:41 AM") */
static char *fmt_t(double ms) { char *d = set_date_str(ms / 1000); time_t t = (time_t)(ms / 1000); struct tm lt; localtime_r(&t, &lt); int h = lt.tm_hour % 12 ? lt.tm_hour % 12 : 12; char *r = g_strdup_printf("%s %d:%02d %s", d, h, lt.tm_min, lt.tm_hour < 12 ? "AM" : "PM"); g_free(d); return r; }

/* buttons in a .rmrow (flex, wrapping, gap 8 px, or 6 as Data encryption's) */
typedef struct { GtkWidget *b[8]; int n; double gap; } BRow;
static double brow_lay(void *d, double x, double y, double w)
{
	BRow *r = d; double cx = 0, cy = 0, lh = 0, g = r->gap ? r->gap : 8;
	for (int k = 0; k < r->n; k++) { double bw = p_nat_w(r->b[k]), bh = p_nat_h(r->b[k]); if (cx > 0 && cx + bw > w + .5) { cx = 0; cy += lh + g; lh = 0; } p_place(r->b[k], x + cx, y + cy, 0, 0); cx += bw + g; lh = fmax(lh, bh); }
	return cy + lh;
}
static GtkWidget *bbtn(BRow *r, const char *l, int pri, GCallback cb, void *d, const char *name, int need_admin)
{
	GtkWidget *b = rui_button(l, pri); if (cb) g_signal_connect(b, "clicked", cb, d); if (need_admin && !set_admin()) gtk_widget_set_sensitive(b, FALSE); p_put(b); if (name) p_label_named(name, b); r->b[r->n++] = b; return b;
}
static BRow *brow(void) { return g_new0(BRow, 1); }
static void on_go(GtkButton *b, gpointer d) { (void)b; go(d); }
/* a plain line of the page's (a div: 13 px, 15 px a line, no margins; grey: #6b7280) */
typedef struct { GtkWidget *a; char *t; int grey, bold; } Line;
static double line_lay(void *d, double x, double y, double w) { Line *l = d; double h = s_lines(l->t, 13, l->bold ? 600 : 400, w, NULL) * 15; p_place(l->a, x, y, w, h); return h; }
static gboolean line_draw(GtkWidget *w, cairo_t *cr, gpointer d) { Line *l = d; if (l->grey) cairo_set_source_rgb(cr, 107 / 255.0, 114 / 255.0, 128 / 255.0); else s_col(cr, "fg", 1); s_text(cr, l->t, 0, 0, 13, l->bold ? 600 : 400, gtk_widget_get_allocated_width(w) / s_z(), 15); return FALSE; }
static GtkWidget *p_line(const char *t, int grey, int bold, double mt, double mb) { Line *l = g_new0(Line, 1); l->t = g_strdup(t); l->grey = grey; l->bold = bold; l->a = gtk_drawing_area_new(); g_signal_connect(l->a, "draw", G_CALLBACK(line_draw), l); p_put(l->a); p_custom(line_lay, l, mt, mb); return l->a; }

/* ================================================================== the working copy of bd95sec (SECW) against what is applied (SECC) */
static const char *const TRUST0[] = { "wikipedia.org", "wikimedia.org", "wikidata.org", "wiktionary.org", "openstreetmap.org", "archive.org" };
typedef struct { int fwon; char mode[16]; GPtrArray *block, *trust; int avon; char act[16]; int unk, scripts; } Sec;
static Sec SECC, SECW; static int SEC_LOADED;
static void sec_free(Sec *s) { if (s->block) g_ptr_array_unref(s->block); if (s->trust) g_ptr_array_unref(s->trust); memset(s, 0, sizeof *s); }
static GPtrArray *strs(RuiJ *l, const char *const *dflt, int nd) { GPtrArray *a = g_ptr_array_new_with_free_func(g_free); if (l && rui_jtype(l) == 4) { for (int i = 0; i < rui_jlen(l); i++) if (rui_jsval(rui_jat(l, i))) g_ptr_array_add(a, g_strdup(rui_jsval(rui_jat(l, i)))); } else for (int i = 0; i < nd; i++) g_ptr_array_add(a, g_strdup(dflt[i])); return a; }
static void sec_default(Sec *s) { sec_free(s); s->fwon = 1; strcpy(s->mode, "open"); s->block = strs(NULL, NULL, 0); s->trust = strs(NULL, TRUST0, 6); s->avon = 1; strcpy(s->act, "quarantine"); }
static int jbool(RuiJ *o, const char *k, int d) { RuiJ *v = o ? rui_jprop(o, k) : NULL; return v && rui_jtype(v) == 1 ? (int)rui_jval(v) : d; }
static void sec_load(Sec *s)
{
	RuiJ *o = set_ls_get("bd95sec"); RuiJ *v = X(o), *fw = rui_jprop(v, "fw"), *av = rui_jprop(v, "av"), *pol = rui_jprop(v, "pol"); sec_default(s);
	s->fwon = jbool(fw, "on", 1); snprintf(s->mode, sizeof s->mode, "%s", set_jstr(fw, "mode", "open"));
	if (fw && rui_jprop(fw, "block")) { g_ptr_array_unref(s->block); s->block = strs(rui_jprop(fw, "block"), NULL, 0); }
	if (fw && rui_jprop(fw, "trust")) { g_ptr_array_unref(s->trust); s->trust = strs(rui_jprop(fw, "trust"), NULL, 0); }
	s->avon = jbool(av, "on", 1); snprintf(s->act, sizeof s->act, "%s", set_jstr(av, "act", "quarantine")); s->unk = jbool(pol, "unk", 0); s->scripts = jbool(pol, "scripts", 0); rui_jfree(o);
}
static void sec_copy(Sec *d, const Sec *s) { sec_free(d); *d = *s; d->block = g_ptr_array_new_with_free_func(g_free); d->trust = g_ptr_array_new_with_free_func(g_free); for (guint i = 0; i < s->block->len; i++) g_ptr_array_add(d->block, g_strdup(g_ptr_array_index(s->block, i))); for (guint i = 0; i < s->trust->len; i++) g_ptr_array_add(d->trust, g_strdup(g_ptr_array_index(s->trust, i))); }
static void arr_json(GString *b, GPtrArray *a) { g_string_append_c(b, '['); for (guint i = 0; i < a->len; i++) { char *e = jstr_esc(g_ptr_array_index(a, i)); g_string_append_printf(b, "%s%s", i ? "," : "", e); g_free(e); } g_string_append_c(b, ']'); }
static char *sec_json(const Sec *s)
{
	GString *b = g_string_new(NULL); g_string_printf(b, "{\"fw\":{\"on\":%s,\"mode\":\"%s\",\"block\":", s->fwon ? "true" : "false", s->mode); arr_json(b, s->block); g_string_append(b, ",\"trust\":"); arr_json(b, s->trust);
	g_string_append_printf(b, "},\"av\":{\"on\":%s,\"act\":\"%s\"},\"pol\":{\"unk\":%s,\"scripts\":%s}}", s->avon ? "true" : "false", s->act, s->unk ? "true" : "false", s->scripts ? "true" : "false"); return g_string_free(b, FALSE);
}
static int sec_dirty(void) { if (!set_admin()) return 0; char *a = sec_json(&SECC), *b = sec_json(&SECW); int d = strcmp(a, b) != 0; g_free(a); g_free(b); return d; }
static char *SM; static int SM_OK;   /* (#sm: the footer's words, green or red) */
static void say(const char *t, int ok) { g_free(SM); SM = g_strdup(t); SM_OK = ok; set_note("say %s", t); }
static GtkWidget *SP;   /* (Confirm: the administrator's own password) */
static int pw_right(void)
{
	const char *p = SP ? gtk_entry_get_text(GTK_ENTRY(SP)) : ""; if (!*p) return 0; char *b = cred_body(ME, p, ",\"why\":\"Apply security settings\""); int c = rui_post("/api/desktop/admin", b, NULL, 0); wipe(b); set_note("POST desktop/admin %d", c); return c == 200;
}
static void foot(int what)
{
	if (what == 1) { sec_copy(&SECW, &SECC); if (SP) gtk_entry_set_text(GTK_ENTRY(SP), ""); g_free(SM); SM = NULL; redraw(); return; }
	if (!set_admin()) { say("Only administrators can change security settings.", 0); redraw(); return; }
	if (!pw_right()) { say(what == 2 ? "Enter your admin password to reset these settings." : "Enter your admin password to apply these changes.", 0); redraw(); return; }
	if (what == 2) { sec_default(&SECW); sec_default(&SECC); } else sec_copy(&SECC, &SECW);
	char *j = sec_json(&SECC); set_ls_set("bd95sec", j); set_note("svj bd95sec %s", j); g_free(j); gtk_entry_set_text(GTK_ENTRY(SP), "");
	char *l = g_strdup_printf("%s %s security settings", ME, what == 2 ? "reset" : "changed"); set_log_evt("sys", l); g_free(l);
	say(what == 2 ? "Security settings were reset to their defaults." : "Security settings applied.", 1); redraw();
}

/* ================================================================== the status cards (.stat) and the lock bar */
typedef struct { GtkWidget *a; char *b[3], *t[3]; int warn[3]; } Stat;
static double stat_ch(Stat *s, double cw) { double h = 0; for (int i = 0; i < 3; i++) h = fmax(h, 15 + s_lines(s->t[i], 12, 400, cw - 22, NULL) * 14); return h + 18; }
static double stat_lay(void *d, double x, double y, double w) { Stat *s = d; double cw = (w - 18 - 16) / 3, h = stat_ch(s, cw) + 8; p_place(s->a, x, y, w, h); return h; }
static gboolean stat_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Stat *s = d; double W = gtk_widget_get_allocated_width(w) / s_z(), H = gtk_widget_get_allocated_height(w) / s_z(), cw = (W - 18 - 16) / 3, ch = stat_ch(s, cw);
	if (RUI.dark) cairo_set_source_rgb(cr, 0x0b / 255.0, 0x12 / 255.0, 0x20 / 255.0); else cairo_set_source_rgb(cr, .98, .98, .98); cairo_rectangle(cr, 0, 0, W, H); cairo_fill(cr); rui_color(cr, "bd", 1); cairo_rectangle(cr, 0, 0, W, 1); cairo_fill(cr);
	for (int i = 0; i < 3; i++) { double x = 9 + i * (cw + 8), y = 4; s_rrect(cr, x + .5, y + .5, cw - 1, ch - 1, 7.5);
		if (s->warn[i]) cairo_set_source_rgb(cr, 0xfe / 255.0, 0xf2 / 255.0, 0xf2 / 255.0); else cairo_set_source_rgb(cr, 0xf0 / 255.0, 0xfd / 255.0, 0xf4 / 255.0); cairo_fill_preserve(cr);
		if (s->warn[i]) cairo_set_source_rgb(cr, 0xfe / 255.0, 0xca / 255.0, 0xca / 255.0); else cairo_set_source_rgb(cr, 0xbb / 255.0, 0xf7 / 255.0, 0xd0 / 255.0); cairo_set_line_width(cr, 1); cairo_stroke(cr);
		cairo_set_source_rgb(cr, 0, 0, 0); s_text(cr, s->b[i], x + 11, y + 9, 13, 700, cw - 22, 15); s_text(cr, s->t[i], x + 11, y + 24, 12, 400, cw - 22, 14); }
	return FALSE;
}
static double lock_lay(void *d, double x, double y, double w) { int n = s_lines(g_object_get_data(G_OBJECT(d), "t"), 13, 400, w - 22, NULL); double h = n * 15 + 18; p_place(d, x, y, w, h); return h; }
static gboolean lock_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)d; double W = gtk_widget_get_allocated_width(w) / s_z(), H = gtk_widget_get_allocated_height(w) / s_z(); s_rrect(cr, .5, .5, W - 1, H - 1, 7.5); cairo_set_source_rgb(cr, 1, 0xfb / 255.0, 0xeb / 255.0); cairo_fill_preserve(cr); cairo_set_source_rgb(cr, 0xfd / 255.0, 0xe6 / 255.0, 0x8a / 255.0); cairo_stroke(cr);
	cairo_set_source_rgb(cr, 0x92 / 255.0, 0x40 / 255.0, 0x0e / 255.0); s_text(cr, g_object_get_data(G_OBJECT(w), "t"), 11, 9, 13, 400, W - 22, 15); return FALSE;
}
static int QN; static RuiJ *SLOGO;
static void top_sec(void)
{
	if (!set_admin()) { GtkWidget *a = gtk_drawing_area_new(); g_object_set_data(G_OBJECT(a), "t", (gpointer)"🔒 Only administrators can change security settings. You can see the current status below."); g_signal_connect(a, "draw", G_CALLBACK(lock_draw), NULL); p_put(a); p_custom(lock_lay, a, 0, 10); }
	Stat *s = g_new0(Stat, 1); s->a = gtk_drawing_area_new(); g_signal_connect(s->a, "draw", G_CALLBACK(stat_draw), s); p_put(s->a); p_label_named("stat", s->a);
	s->b[0] = "Website blocking"; s->t[0] = g_strdup(SECC.fwon ? (!strcmp(SECC.mode, "lock") ? "On — internet blocked" : !strcmp(SECC.mode, "trusted") ? "On — trusted sites only" : "On") : "Off ⚠"); s->warn[0] = !SECC.fwon;
	RuiJ *scan = rui_jprop(X(SLOGO), "scan"); char *lt = scan && rui_jtype(scan) == 5 ? fmt_t(jnum(scan, "t")) : NULL;
	s->b[1] = "Antivirus"; s->t[1] = SECC.avon ? g_strdup_printf("On — definitions 2026.10.10%s%s", lt ? " · last scan " : "", lt ? lt : "") : g_strdup("Off ⚠"); s->warn[1] = !SECC.avon; g_free(lt);
	s->b[2] = "Quarantine"; s->t[2] = g_strdup_printf("%d item%s", QN, QN == 1 ? "" : "s");
	p_custom(stat_lay, s, 0, 6);
}

/* ================================================================== Firewall (sec50.js fwSection): incoming connections */
static GtkWidget *FWN;
static void fw_go(int ok, void *d)
{
	int on = GPOINTER_TO_INT(d); if (!ok) return; char p[40], er[300]; snprintf(p, sizeof p, "firewall?on=%d", on); RuiJ *j = post_body(p, NULL, er, sizeof er);
	if (j) { set_notify("Firewall", on ? "The firewall is on." : "The firewall is off.", !on); set_log_evt("fw", on ? "Firewall (incoming connections) on" : "Firewall (incoming connections) off"); } else set_notify("Firewall", *er ? er : "It could not be changed.", 1);
	rui_jfree(j); redraw();
}
static void on_fw(int on, void *d) { (void)d; rui_check_set(FWN, !on); const char *r = on ? "Turn on the firewall" : "Turn off the firewall: other computers on the network can then reach programs that listen for connections."; set_note("elevate %s", r); rui_elevate(SW, r, fw_go, GINT_TO_POINTER(on)); }
static void fw_sec(void)
{
	p_h4("Firewall"); p_msg_m("Blocks connections that other computers start to this one. Programs on this computer can still reach the internet, and replies to them come in as usual.", 0, 2, 8);
	RuiJ *s = get("firewall"); Cell *c = p_r2("Firewall"); FWN = c_check(c, "Block incoming connections", jb(s, "on"), on_fw, NULL); p_label_named("fwn", FWN); if (!set_admin() || !s) gtk_widget_set_sensitive(FWN, FALSE);
	char *t; if (!s) t = g_strdup("not available"); else if (jb(s, "on")) { double n = jnum(s, "blocked"); t = g_strdup_printf("On · %.0f incoming connection%s blocked since start-up%s", n == n ? n : 0, n == 1 ? "" : "s", jb(s, "remote") ? " · remote access allowed from the local network" : ""); }
	else t = g_strdup("Off ⚠ other computers can reach programs that listen for connections");
	c_line(c, 0); c_msg_m(c, t, 4, 0, 0); g_free(t); rui_jfree(s);
}

/* ================================================================== Data encryption (vault50.js vSection) */
static const char *const VPARTS[][2] = { { "debian", "Linux (Debian) and Windows programs" }, { "flatpak", "Linux apps (Flatpak)" }, { "downloads", "Files kept in Downloads" } };
static void on_vmove(GtkButton *b, gpointer d) { (void)b; (void)d; char er[300]; RuiJ *j = post_body("vault/move", NULL, er, sizeof er); if (!j) set_notify("Encryption", er, 1); else go("vmove"); rui_jfree(j); }
static void vault_sec(void)
{
	RuiJ *s = get("vault"); p_h4("Data encryption"); const char *st = set_jstr(s, "state", ""); const char *t; int parts = 0; BRow *r = brow(); r->gap = 6;
	if (!s) t = "Not available.";
	else if (!strcmp(st, "unavailable")) t = "Encryption needs Rudolf OS installed, or a USB drive that keeps your files.";
	else if (!strcmp(st, "off")) { t = "Off. Debian, Linux apps and files kept in Downloads are stored as they are: anyone with this computer's drive can read them."; bbtn(r, "Encrypt my data…", 1, G_CALLBACK(on_go), "vsetup", "vsetup", 1); }
	else if (!strcmp(st, "restart")) { t = "The drive was prepared for encryption. Restart Rudolf OS to continue; after you sign in, your data moves into the encrypted folder."; bbtn(r, "Restart now", 1, G_CALLBACK(on_go), "restart", "vrestart", 0); }
	else if (!strcmp(st, "ready")) { t = "Set up, waiting to start: it starts when an account that can unlock it signs in with its password."; parts = 1; }
	else if (!strcmp(st, "locked")) { t = "On, locked: sign in to an account with its password to unlock it. Linux programs cannot start until then."; parts = 1; bbtn(r, "Unlock with the recovery key…", 0, G_CALLBACK(on_go), "vrecover", "vrecover", 0); }
	else if (!strcmp(st, "moving")) { t = "On, unlocked. Moving your data into the encrypted folder has not finished."; parts = 1; bbtn(r, "Show progress", 0, G_CALLBACK(on_go), "vmove", "vprog", 0); bbtn(r, "Move again", 0, G_CALLBACK(on_vmove), NULL, "vagain", 0); }
	else { t = "On, unlocked. Turned off, this computer's drive shows only scrambled names and contents for these:"; parts = 1; }
	p_label_named("v50t", p_msg_m(t, 0, 2, 6));
	if (parts) { RuiJ *mv = rui_jprop(s, "moved"); for (guint i = 0; i < G_N_ELEMENTS(VPARTS); i++) { int m = jb(mv, VPARTS[i][0]); char *l = g_strdup_printf("%s%s%s", m ? "🔒 " : "○ ", VPARTS[i][1], m ? ": encrypted" : ": not moved yet"); p_line(l, 0, 0, 0, 0); g_free(l); }
		/* (the accounts whose password unlocks it: bd95user's accounts named in vault's data.accts) */
		GString *who = g_string_new(NULL); RuiJ *ac = rui_jprop(rui_jprop(s, "data"), "accts"); RuiJ *uo = set_ls_get("bd95user"), *u = X(uo), *up = u && rui_jtype(u) == 3 ? rui_jparse(rui_jsval(u)) : NULL, *al = rui_jprop(up ? up : u, "accts");
		for (int i = 0; al && i < rui_jlen(al); i++) { RuiJ *a = rui_jat(al, i); const char *id = set_jstr(a, "id", ""); if (*id && jb(ac, id)) g_string_append_printf(who, "%s%s", who->len ? ", " : "", set_jstr(a, "n", "")); } rui_jfree(up); rui_jfree(uo);
		char *l = g_strdup_printf("Accounts whose password unlocks it: %s. Not encrypted: the system itself (it is signed and checked instead), settings, logs and the browser's own data (the desktop encrypts each account's home folder itself). Large downloads are in each account's own folder on the disk; Rudolf OS shows each account only its own, but programs running on this computer, such as Linux apps, could reach any of them.", who->len ? who->str : "none yet");
		p_line(l, 1, 0, 0, 6); g_free(l); g_string_free(who, TRUE); }
	if (r->n) p_custom(brow_lay, r, 14, 0); else p_custom(brow_lay, r, 0, 0);
	if (s && !strcmp(st, "off") && !set_admin()) p_msg(" An administrator can turn this on.", 0);
	rui_jfree(s);
}

/* ================================================================== Website blocking (the page's old "Firewall": URL rules for Oracle) */
/* (.chips: flex, wrapping, gap 4, 6 px under; each .chip 12 px, 1px 8px, round, its ✕ #6b7280 4 px after; none: <i>none</i>) */
typedef struct { GtkWidget *a; GPtrArray *l; double W, h; int which; } Chips;
static double chip_w(const char *t) { return tw(t, 12, 400) + tw(" ", 12, 400) + 4 + tw("✕", 12, 400) + 16 + 2; }
static double chips_h(Chips *c, double W) { if (!c->l->len) return 15; double x = 0, y = 0; for (guint i = 0; i < c->l->len; i++) { double w = chip_w(g_ptr_array_index(c->l, i)); if (x > 0 && x + w > W + .5) { x = 0; y += 22; } x += w + 4; } return y + 18; }
static gboolean chips_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Chips *c = d; double W = gtk_widget_get_allocated_width(w) / s_z();
	if (!c->l->len) { cairo_set_source_rgb(cr, 107 / 255.0, 114 / 255.0, 128 / 255.0); PangoLayout *l = s_layout("none", 13, 400, 0); PangoFontDescription *f = pango_font_description_copy(pango_layout_get_font_description(l) ? pango_layout_get_font_description(l) : pango_context_get_font_description(pango_layout_get_context(l)));
		pango_font_description_set_style(f, PANGO_STYLE_ITALIC); pango_layout_set_font_description(l, f); pango_font_description_free(f); double z = s_z(); cairo_save(cr); cairo_scale(cr, 1 / z, 1 / z); cairo_move_to(cr, 0, (15 - s_lh(13, 400)) / 2 * z); pango_cairo_show_layout(cr, l); cairo_restore(cr); g_object_unref(l); return FALSE; }
	double x = 0, y = 0;
	for (guint i = 0; i < c->l->len; i++) { const char *t = g_ptr_array_index(c->l, i); double cw = chip_w(t); if (x > 0 && x + cw > W + .5) { x = 0; y += 22; }
		s_rrect(cr, x + .5, y + .5, cw - 1, 17, 8.5); if (RUI.dark) cairo_set_source_rgb(cr, 0x1e / 255.0, 0x29 / 255.0, 0x3b / 255.0); else cairo_set_source_rgb(cr, 0xee / 255.0, 0xf2 / 255.0, 1); cairo_fill_preserve(cr);
		if (RUI.dark) cairo_set_source_rgb(cr, 0x33 / 255.0, 0x41 / 255.0, 0x55 / 255.0); else cairo_set_source_rgb(cr, 0xc7 / 255.0, 0xd2 / 255.0, 0xfe / 255.0); cairo_set_line_width(cr, 1); cairo_stroke(cr);
		if (RUI.dark) cairo_set_source_rgb(cr, 0xe2 / 255.0, 0xe8 / 255.0, 0xf0 / 255.0); else cairo_set_source_rgb(cr, 0, 0, 0); s_text(cr, t, x + 9, y + 2, 12, 400, 0, 14);
		cairo_set_source_rgb(cr, 107 / 255.0, 114 / 255.0, 128 / 255.0); s_text(cr, "✕", x + 9 + tw(t, 12, 400) + tw(" ", 12, 400) + 4, y + 2, 12, 400, 0, 14); x += cw + 4; }
	return FALSE;
}
static void chips_alloc(GtkWidget *w, GtkAllocation *a, gpointer d) { Chips *c = d; double W = a->width / s_z(); if (fabs(W - c->W) < .5) return; c->W = W; double h = chips_h(c, W); if (fabs(h - c->h) > .1) { c->h = h; gtk_widget_set_size_request(w, -1, (int)lround(h * s_z())); p_relayout(); } (void)w; }
static gboolean chips_click(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	Chips *c = d; if (!set_admin()) return FALSE; double W = gtk_widget_get_allocated_width(w) / s_z(), ex = e->x / s_z(), ey = e->y / s_z(), x = 0, y = 0;
	for (guint i = 0; i < c->l->len; i++) { const char *t = g_ptr_array_index(c->l, i); double cw = chip_w(t); if (x > 0 && x + cw > W + .5) { x = 0; y += 22; }
		double ux = x + 9 + tw(t, 12, 400) + tw(" ", 12, 400) + 4; if (ey >= y && ey <= y + 18 && ex >= ux && ex <= ux + tw("✕", 12, 400)) { set_note("chip remove %s", t); g_ptr_array_remove_index(c->l, i); p_footer_update(); redraw(); return TRUE; } x += cw + 4; }
	return FALSE;
}
static GtkWidget *chips(GPtrArray *l, const char *name)
{
	Chips *c = g_new0(Chips, 1); c->l = l; c->a = gtk_drawing_area_new(); gtk_widget_add_events(c->a, GDK_BUTTON_PRESS_MASK); g_signal_connect(c->a, "draw", G_CALLBACK(chips_draw), c); g_signal_connect(c->a, "size-allocate", G_CALLBACK(chips_alloc), c);
	g_signal_connect(c->a, "button-press-event", G_CALLBACK(chips_click), c); c->h = chips_h(c, 447); gtk_widget_set_size_request(c->a, -1, (int)lround(c->h * s_z())); p_label_named(name, c->a); return c->a;
}
static GtkWidget *FWBI, *FWTI;
static char *norm_host(const char *v)   /* (normHost: the site's name, lower case, no scheme, path or www.) */
{
	char *s = g_ascii_strdown(g_strstrip(g_strdup(v)), -1); char *p = strstr(s, "://"); char *h = g_strdup(p ? p + 3 : s); g_free(s); h[strcspn(h, "/?#:")] = 0; if (!strncmp(h, "www.", 4)) memmove(h, h + 4, strlen(h + 4) + 1); return h;
}
static void add_host(GtkWidget *e, GPtrArray *a)
{
	if (!set_admin()) return; char *h = norm_host(gtk_entry_get_text(GTK_ENTRY(e)));
	if (!g_regex_match_simple("^([a-z0-9-]+\\.)+[a-z]{2,}$|^\\d{1,3}(\\.\\d{1,3}){3}$", h, 0, 0)) { say("Enter a valid site name such as example.com.", 0); g_free(h); redraw(); return; }
	int have = 0; for (guint i = 0; i < a->len; i++) if (!strcmp(g_ptr_array_index(a, i), h)) have = 1; if (!have) g_ptr_array_add(a, h); else g_free(h); g_free(SM); SM = NULL; redraw();
}
static void on_addb(GtkWidget *w, gpointer d) { (void)w; (void)d; add_host(FWBI, SECW.block); }
static void on_addt(GtkWidget *w, gpointer d) { (void)w; (void)d; add_host(FWTI, SECW.trust); }
static void on_chk(int on, void *d) { int *v = d; *v = on; p_footer_update(); }
static void on_mode(const char *v, void *d) { (void)d; snprintf(SECW.mode, sizeof SECW.mode, "%s", v); p_footer_update(); }
static void on_act(const char *v, void *d) { (void)d; snprintf(SECW.act, sizeof SECW.act, "%s", v); p_footer_update(); }
static void hosts_row(const char *label, GPtrArray *a, GtkWidget **in, const char *ph, GCallback add, const char *cn, const char *in_n, const char *add_n)
{
	Cell *c = p_r2(label); c_add(c, chips(a, cn), 0, 0, 1); c_line(c, 6); *in = rui_entry(ph); gtk_widget_set_size_request(*in, (int)lround(185 * s_z()), -1); c_add(c, *in, 0, 0, 0); p_label_named(in_n, *in); g_signal_connect(*in, "activate", add, NULL);
	GtkWidget *b = rui_button("Add", 0); g_signal_connect(b, "clicked", add, NULL); c_add(c, b, 4, 0, 0); p_label_named(add_n, b); if (!set_admin()) { gtk_widget_set_sensitive(*in, FALSE); gtk_widget_set_sensitive(b, FALSE); } p_r2_more(c);
}
static void web_sec(void)
{
	int adm = set_admin(); p_h4("Website blocking");
	Cell *c = p_r2("Website blocking"); GtkWidget *w = c_check(c, "Turn on website blocking (in Oracle)", SECW.fwon, on_chk, &SECW.fwon); p_label_named("fwo", w); if (!adm) gtk_widget_set_sensitive(w, FALSE); p_r2_more(c);
	static const char *const MV[3] = { "open", "trusted", "lock" }, *const ML[3] = { "Allow all sites except blocked ones", "Trusted sites only", "Block all internet access" };
	c = p_r2("Web access"); w = s_select(MV, ML, 3, SECW.mode, on_mode, NULL); c_add(c, w, 0, 0, 1); p_label_named("fwm", w); if (!adm) gtk_widget_set_sensitive(w, FALSE); p_r2_more(c);
	hosts_row("Blocked sites", SECW.block, &FWBI, "example.com", G_CALLBACK(on_addb), "fwb", "fwbi", "fwba");
	hosts_row("Trusted sites", SECW.trust, &FWTI, "example.org", G_CALLBACK(on_addt), "fwt", "fwti", "fwta");
}

/* ================================================================== Antivirus, System protection, Confirm */
static void on_scan(GtkButton *b, gpointer d) { (void)b; (void)d; if (set_admin()) ask("stor avscan"); }
static void av_sec(void)
{
	int adm = set_admin(); p_h4("Antivirus");
	Cell *c = p_r2("Real-time protection"); GtkWidget *w = c_check(c, "Scan files when they are saved, downloaded, opened or run", SECW.avon, on_chk, &SECW.avon); p_label_named("avo", w); if (!adm) gtk_widget_set_sensitive(w, FALSE); p_r2_more(c);
	static const char *const AV[2] = { "quarantine", "block" }, *const AL[2] = { "Move it to quarantine", "Block and discard it" };
	c = p_r2("If a threat is found"); w = s_select(AV, AL, 2, SECW.act, on_act, NULL); c_add(c, w, 0, 0, 1); p_label_named("ava", w); if (!adm) gtk_widget_set_sensitive(w, FALSE); p_r2_more(c);
	c = p_r2("Virus scan"); GtkWidget *b = rui_button("Scan now", 0); g_signal_connect(b, "clicked", G_CALLBACK(on_scan), NULL); if (!adm) gtk_widget_set_sensitive(b, FALSE); c_add(c, b, 0, 0, 0); p_label_named("scn", b); c_msg_m(c, "Definitions 2026.10.10", 4, 0, 0);
	/* (the last scan's result, as the page's #scr: the hidden page's count, kept in bd95avscan:ACCT) */
	char k[120]; snprintf(k, sizeof k, "bd95avscan:%s", ACCT); RuiJ *o = set_ls_get(k), *r = X(o);
	if (r && rui_jtype(r) == 5) { double n = jnum(r, "files"); RuiJ *f = rui_jprop(r, "found"); int nf = f ? rui_jlen(f) : 0; c_line(c, 6);
		char *t = g_strdup_printf("Scanned %.0f files — %s.", n == n ? n : 0, nf ? "" : "no threats found"); if (nf) { g_free(t); t = g_strdup_printf("Scanned %.0f files — %d threat%s found.", n, nf, nf == 1 ? "" : "s"); } c_text(c, t, 600); g_free(t);
		if (nf) { c_line(c, 0); GtkWidget *q = rui_button("Quarantine all", 0); g_signal_connect_swapped(q, "clicked", G_CALLBACK(ask), (gpointer)"stor avquar"); c_add(c, q, 0, 0, 0); p_label_named("avquar", q);
			for (int i = 0; i < nf && i < 30; i++) { RuiJ *x = rui_jat(f, i); c_line(c, 0); char *l = g_strdup_printf("☣️ %s — %s", set_jstr(x, "p", ""), set_jstr(x, "th", "")); c_msg_m(c, l, 4, 0, 0); g_free(l); } } }
	rui_jfree(o);
	p_h4("System protection");
	c = p_r2("File types"); w = c_check(c, "Allow saving file types Rudolf OS does not recognize (programs and installers such as .EXE stay blocked)", SECW.unk, on_chk, &SECW.unk); p_label_named("pu", w); if (!adm) gtk_widget_set_sensitive(w, FALSE); p_r2_more(c);
	c = p_r2("Scripts"); w = c_check(c, "Let standard users run Lua scripts and BlockLua programs", SECW.scripts, on_chk, &SECW.scripts); p_label_named("ps", w); if (!adm) gtk_widget_set_sensitive(w, FALSE);
	if (adm) { p_h4("Confirm"); c = p_r2("Your admin password"); SP = rui_entry("required to apply changes"); gtk_entry_set_visibility(GTK_ENTRY(SP), FALSE); c_add(c, SP, 0, 0, 1); p_label_named("sp", SP); } else SP = NULL;
	GtkWidget *m = p_msg(SM ? SM : "", 0); p_label_named("sm", m); if (SM) g_object_set_data(G_OBJECT(m), "set-color", SM_OK ? "#15803d" : "#b91c1c");
}

/* ================================================================== Quarantine (bd95quar) and the activity log (bd95seclog) */
static RuiJ *QUARO;
static int QDEL;
static void q_del(int i, void *d)
{
	(void)d; if (i != 1) return; RuiJ *q = X(QUARO); GString *b = g_string_new("["); int first = 1; const char *nm = "";
	for (int k = 0; q && k < rui_jlen(q); k++) { if (k == QDEL) { nm = set_jstr(rui_jat(q, k), "n", ""); continue; } if (!first) g_string_append_c(b, ','); first = 0; set_jw(b, rui_jat(q, k)); } g_string_append_c(b, ']');
	set_ls_set("bd95quar", b->str); set_note("svj bd95quar (%d left)", QN - 1); char *l = g_strdup_printf("Deleted %s from quarantine", nm); set_log_evt("av", l); g_free(l); g_string_free(b, TRUE); redraw();
}
static void on_qdel(GtkButton *b, gpointer d) { (void)b; QDEL = GPOINTER_TO_INT(d); RuiJ *it = rui_jat(X(QUARO), QDEL); char *t = g_strdup_printf("Permanently delete %s?", set_jstr(it, "n", "")); static const char *const B[2] = { "Cancel", "Confirm" }; rui_confirm(SW, "Delete", t, B, 2, 1, q_del, NULL); g_free(t); }
/* (.acr.qr: the file's line, its Restore and Delete at the right; 4px above and below, a line under) */
typedef struct { GtkWidget *a, *b[2]; char *t; } QRow;
static double qrow_lay(void *d, double x, double y, double w) { QRow *r = d; double bx = x + w; for (int k = 1; k >= 0; k--) if (r->b[k]) { bx -= p_nat_w(r->b[k]); p_place(r->b[k], bx, y + 4, 0, 0); bx -= 8; } p_place(r->a, x, y, w, 32); return 32; }
static gboolean qrow_draw(GtkWidget *w, cairo_t *cr, gpointer d) { QRow *r = d; double W = gtk_widget_get_allocated_width(w) / s_z(); s_col(cr, "acr", 1); cairo_rectangle(cr, 0, 31, W, 1); cairo_fill(cr); s_col(cr, "fg", 1); s_text(cr, r->t, 0, 8, 13, 400, W - 160, 15); return FALSE; }
static int QRES;
static void q_res(int i, void *d) { (void)d; if (i != 1) return; char a[40]; snprintf(a, sizeof a, "stor qrestore %d", QRES); ask(a); }
static void on_qres(GtkButton *b, gpointer d) { (void)b; QRES = GPOINTER_TO_INT(d); RuiJ *it = rui_jat(X(QUARO), QRES); GString *th = g_string_new(NULL); RuiJ *t = rui_jprop(it, "th"); for (int k = 0; t && k < rui_jlen(t); k++) g_string_append_printf(th, "%s%s", k ? ", " : "", rui_jsval(rui_jat(t, k)) ? rui_jsval(rui_jat(t, k)) : "");
	char *m = g_strdup_printf("Restore %s? It was flagged as %s.", set_jstr(it, "n", ""), th->str); static const char *const B[2] = { "Cancel", "Confirm" }; rui_confirm(SW, "Restore", m, B, 2, 1, q_res, NULL); g_free(m); g_string_free(th, TRUE); }
/* (.lgr: 20px 150px 1fr, gap 6, 12 px #374151, 2px 0) */
typedef struct { GtkWidget *a; GPtrArray *k, *t, *m; } Log;
static double log_lay(void *d, double x, double y, double w) { Log *l = d; double h = 0; for (guint i = 0; i < l->m->len; i++) h += fmax(14, s_lines(g_ptr_array_index(l->m, i), 12, 400, w - 182, NULL) * 14) + 4; p_place(l->a, x, y, w, MAX(1, h)); return h; }
static gboolean log_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Log *l = d; double W = gtk_widget_get_allocated_width(w) / s_z(), y = 0;
	for (guint i = 0; i < l->m->len; i++) { const char *m = g_ptr_array_index(l->m, i); double h = fmax(14, s_lines(m, 12, 400, W - 182, NULL) * 14);
		if (RUI.dark) rui_color(cr, "mut", 1); else cairo_set_source_rgb(cr, 0x37 / 255.0, 0x41 / 255.0, 0x51 / 255.0);
		s_text(cr, g_ptr_array_index(l->k, i), 0, y + 2, 12, 400, 20, 14); s_text(cr, g_ptr_array_index(l->t, i), 26, y + 2, 12, 400, 150, 14); s_text(cr, m, 182, y + 2, 12, 400, W - 182, 14); y += h + 4; }
	return FALSE;
}
static void ql_clear(int i, void *d) { (void)d; if (i != 1) return; RuiJ *s = X(SLOGO); GString *b = g_string_new("{\"fw\":[],\"av\":[],\"sys\":[],\"scan\":"); RuiJ *sc = rui_jprop(s, "scan"); if (sc) set_jw(b, sc); else g_string_append(b, "null"); g_string_append_c(b, '}'); set_ls_set("bd95seclog", b->str); set_note("svj bd95seclog cleared"); g_string_free(b, TRUE); redraw(); }
static void on_lgc(GtkButton *b, gpointer d) { (void)b; (void)d; static const char *const B[2] = { "Cancel", "Confirm" }; rui_confirm(SW, "Clear log", "Clear the activity log?", B, 2, 1, ql_clear, NULL); }
static gint by_t(gconstpointer a, gconstpointer b) { double x = jnum(*(RuiJ *const *)a, "t"), y = jnum(*(RuiJ *const *)b, "t"); return x < y ? 1 : x > y ? -1 : 0; }
static void quar_log_sec(void)
{
	int adm = set_admin(); p_h4("Quarantine"); RuiJ *q = X(QUARO);
	if (!QN) p_line("No quarantined files.", 1, 0, 0, 0);
	else for (int i = 0; i < QN && i < 40; i++) { RuiJ *it = rui_jat(q, i); GString *th = g_string_new(NULL); RuiJ *t = rui_jprop(it, "th"); for (int k = 0; t && k < rui_jlen(t); k++) g_string_append_printf(th, "%s%s", k ? ", " : "", rui_jsval(rui_jat(t, k)) ? rui_jsval(rui_jat(t, k)) : "");
		QRow *r = g_new0(QRow, 1); r->t = g_strdup_printf("☣️ %s — %s", set_jstr(it, "n", ""), th->str); g_string_free(th, TRUE); r->a = gtk_drawing_area_new(); g_signal_connect(r->a, "draw", G_CALLBACK(qrow_draw), r); p_put(r->a);
		if (adm) { r->b[0] = rui_button("Restore", 0); r->b[1] = rui_button("Delete", 0); g_signal_connect(r->b[0], "clicked", G_CALLBACK(on_qres), GINT_TO_POINTER(i)); g_signal_connect(r->b[1], "clicked", G_CALLBACK(on_qdel), GINT_TO_POINTER(i)); p_put(r->b[0]); p_put(r->b[1]);
			char nm[16]; snprintf(nm, sizeof nm, "qr%d", i); p_label_named(nm, r->b[0]); snprintf(nm, sizeof nm, "qx%d", i); p_label_named(nm, r->b[1]); }
		p_custom(qrow_lay, r, 0, 0); }
	p_h4("Activity log"); RuiJ *s = X(SLOGO); GPtrArray *all = g_ptr_array_new(); const char *ks[3] = { "fw", "av", "sys" }, *ic[3] = { "🛡️", "☣️", "⚙️" };
	for (int k = 0; k < 3; k++) { RuiJ *a = rui_jprop(s, ks[k]); for (int i = 0; a && rui_jtype(a) == 4 && i < rui_jlen(a); i++) { RuiJ *e = rui_jat(a, i); g_ptr_array_add(all, e); g_ptr_array_add(all, (gpointer)ic[k]); } }
	/* (pairs: an event and its icon; sorted newest first, the first 12) */
	GPtrArray *ev = g_ptr_array_new(); for (guint i = 0; i < all->len; i += 2) g_ptr_array_add(ev, g_ptr_array_index(all, i)); g_ptr_array_sort(ev, by_t);
	if (!ev->len) p_line("Nothing has been logged yet.", 1, 0, 0, 0);
	else { Log *l = g_new0(Log, 1); l->k = g_ptr_array_new(); l->t = g_ptr_array_new_with_free_func(g_free); l->m = g_ptr_array_new_with_free_func(g_free); l->a = gtk_drawing_area_new(); g_signal_connect(l->a, "draw", G_CALLBACK(log_draw), l); p_put(l->a);
		for (guint i = 0; i < ev->len && i < 12; i++) { RuiJ *e = g_ptr_array_index(ev, i); const char *icon = "⚙️"; for (guint q2 = 0; q2 < all->len; q2 += 2) if (g_ptr_array_index(all, q2) == e) icon = g_ptr_array_index(all, q2 + 1);
			g_ptr_array_add(l->k, (gpointer)icon); g_ptr_array_add(l->t, fmt_t(jnum(e, "t"))); g_ptr_array_add(l->m, g_strdup(set_jstr(e, "m", ""))); }
		p_custom(log_lay, l, 0, 0); if (adm) { BRow *r = brow(); bbtn(r, "Clear log", 0, G_CALLBACK(on_lgc), NULL, "lgc", 1); p_custom(brow_lay, r, 6, 0); } }
	g_ptr_array_free(ev, TRUE); g_ptr_array_free(all, TRUE);
}

/* ================================================================== Secure Boot (sb28.js sbSection) */
static void sb_cancel_go(int ok, void *d) { (void)d; if (!ok) return; char er[200]; RuiJ *j = post_body("secureboot/cancel", NULL, er, sizeof er); rui_jfree(j); redraw(); }
static void on_sbcancel(GtkButton *b, gpointer d) { (void)b; (void)d; set_note("elevate Cancel the Secure Boot request."); rui_elevate(SW, "Cancel the Secure Boot request.", sb_cancel_go, NULL); }
static void on_stor(GtkButton *b, gpointer d) { (void)b; (void)d; set_show("stor"); }
static void sb_sec(void)
{
	RuiJ *s = get("secureboot/status"), *inf = get("info"); p_h4("Secure Boot"); BRow *r = brow(); const char *t; char *own = NULL;
	int old = inf && rui_jprop(inf, "bootSecure") && jnum(inf, "bootSecure") == 0;
	const char *OLD = "This computer started from a disc made before Rudolf OS supported Secure Boot (1.2.2). Its boot files are not signed, so the computer refuses them with Secure Boot on, and the blue key screen never appears. Get new boot files replaces them and keeps your files (1.2.8; it needs the boot image from the Rudolf OS releases). Or make a Rudolf OS USB stick from a downloaded image of 1.2.2 or newer, then prepare Secure Boot from there.";
	const char *lk = set_jstr(s, "lockdown", ""); char lock[32] = ""; { const char *a = strchr(lk, '['), *b = a ? strchr(a, ']') : NULL; if (a && b && b - a - 1 < (int)sizeof lock) { memcpy(lock, a + 1, b - a - 1); lock[b - a - 1] = 0; } }
	if (!s) t = "The state of Secure Boot could not be read.";
	else if (!jb(s, "efi")) t = "This computer started in BIOS (legacy) mode, which has no Secure Boot. Start it in UEFI mode to use Secure Boot.";
	else if (jb(s, "on")) { const char *u = set_jstr(s, "unverified", ""); char *uu = g_strdup(u); for (char *p = uu; *p; p++) if (*p == '\n') *p = ';';
		own = g_strdup_printf("On. The computer started only software signed for Rudolf OS, the kernel is locked down%s%s%s", *lock && strcmp(lock, "none") ? "" : " (lockdown is not reported)", *u ? ". Some system layers could not be checked: " : ", and every system layer is checked as it is read.", *u ? uu : ""); g_free(uu); t = own; }
	else if (jb(s, "enrolled")) t = "Off, but ready: the Rudolf OS key is enrolled. Turn Secure Boot on in the computer's firmware settings (often F2, F10, Del or Esc while it starts).";
	else if (jb(s, "pending")) { own = old ? g_strdup_printf("Off. A request to enroll the Rudolf OS key is waiting, but it cannot be acted on: %s", OLD) : g_strdup("Off. The Rudolf OS key will be offered for enrolment at the next start (the blue MokManager screen); you need the one-time password that was shown."); t = own;
		if (!old) bbtn(r, "Restart now", 1, G_CALLBACK(on_go), "restart", "sbrs", 1); bbtn(r, "Cancel the request", 0, G_CALLBACK(on_sbcancel), NULL, "sbcan", 1); }
	else if (old && jb(rui_jprop(inf, "bootFiles"), "pending")) { t = "Off. New boot files are ready: they are written the next time the computer starts. Then prepare Secure Boot here."; bbtn(r, "Restart now", 1, G_CALLBACK(on_go), "restart", "sbrs", 1); }
	else if (old) { own = g_strdup_printf("Off. %s", OLD); t = own; bbtn(r, "Get new boot files…", 1, G_CALLBACK(on_go), "bootfiles", "sbbf", 1); bbtn(r, "Make a USB stick…", 0, G_CALLBACK(on_stor), NULL, "sbstk", 1); }
	else { t = "Off. With Secure Boot on, the computer starts only signed software, so nothing can be slipped in before Rudolf OS starts. Rudolf OS needs its key enrolled once: prepare it here, then confirm it at the next start. This works on every computer, also where the blue screen's “Enroll key from disk” does not accept rudolf.cer."; bbtn(r, "Prepare Secure Boot…", 1, G_CALLBACK(on_go), "sbprepare", "sbprep", 1); }
	p_label_named("sbt", p_msg_m(t, 0, 10, 13)); p_custom(brow_lay, r, 14, 0); g_free(own); rui_jfree(s); rui_jfree(inf);
}

/* ================================================================== Developer mode (devmode59.js's): its card (.sysucard) */
typedef struct { GtkWidget *a; char *b, *m; int warn; } Card;
static double card_lay(void *d, double x, double y, double w) { Card *c = d; double h = 15 + 4 + s_lines(c->m, 13, 400, w - 4 - 1 - 20, NULL) * 15 + 18; p_place(c->a, x, y, w, h); return h; }
static gboolean card_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Card *c = d; double W = gtk_widget_get_allocated_width(w) / s_z(), H = gtk_widget_get_allocated_height(w) / s_z();
	s_rrect(cr, .5, .5, W - 1, H - 1, 7.5); rui_color(cr, "bd", 1); cairo_set_line_width(cr, 1); cairo_stroke(cr);
	cairo_save(cr); s_rrect(cr, 0, 0, W, H, 8); cairo_clip(cr); if (c->warn) cairo_set_source_rgb(cr, 0xd9 / 255.0, 0x77 / 255.0, 0x06 / 255.0); else rui_color(cr, "bd", 1); cairo_rectangle(cr, 0, 0, 4, H); cairo_fill(cr); cairo_restore(cr);
	s_col(cr, "fg", 1); PangoLayout *l = s_layout("", 13, 400, W - 25); pango_layout_set_markup(l, c->b, -1); double z = s_z(); cairo_save(cr); cairo_scale(cr, 1 / z, 1 / z); cairo_move_to(cr, 14 * z, (9 + (15 - s_lh(13, 400)) / 2) * z); pango_cairo_show_layout(cr, l); cairo_restore(cr); g_object_unref(l);
	s_text(cr, c->m, 14, 9 + 15 + 4, 13, 400, W - 25, 15); return FALSE;
}
static void devm_sec(void)
{
	RuiJ *s = get("sys/status"); if (!s || !rui_jprop(s, "developer")) { rui_jfree(s); return; } int dev = jb(s, "developer"), adm = set_admin(); const char *sg = set_jstr(s, "signer", "");
	p_h4("Developer mode"); Card *c = g_new0(Card, 1); c->warn = dev; char *bl = !strcmp(sg, "official") ? g_strdup("Official Rudolf OS") : !strcmp(sg, "custom") ? g_strdup_printf("Custom build (signed by key %.16s)", set_jstr(s, "signerKey", "")) : g_strdup("");
	char *be = g_markup_escape_text(bl, -1); c->b = g_strdup_printf("<b>%s</b>%s%s", dev ? "On" : "Off", *bl ? " · this system: " : "", be); g_free(be); g_free(bl);
	c->m = dev ? g_strdup_printf("Updates signed by a developer's key are installed too, from %s. Recovery mode still starts the official Rudolf OS%s.", set_jstr(s, "url", ""), jb(s, "recoveryOfficial") ? "" : " (once an official release has started on this computer)") : g_strdup("Only releases signed by Rudolf OS are installed. Developers can install their own builds here.");
	c->a = gtk_drawing_area_new(); g_signal_connect(c->a, "draw", G_CALLBACK(card_draw), c); p_put(c->a); p_custom(card_lay, c, 4, 8);
	BRow *r = brow(); bbtn(r, dev ? "Turn off Developer mode…" : "Turn on Developer mode…", 0, G_CALLBACK(on_go), dev ? "devmoff" : "devmon", "dvb", 1);
	if (dev || !strcmp(sg, "custom")) bbtn(r, "Return to official Rudolf OS…", 0, G_CALLBACK(on_go), "devmreturn", "dvr", 1); p_custom(brow_lay, r, 14, 0);
	if (!adm) p_msg("Only an administrator can change this.", 0); rui_jfree(s);
}

/* ================================================================== Development link (devlink40.js devlSection): the administrator's
   name and password with each request that allows or switches it on (rui_elevate_cred), the token sent once and cleared here */
static RuiJ *DL; static char *DLERR, *DLALERR, *DLCERR;
static GtkWidget *DLR, *DLK, *DLH, *DLA, *DLCH, *DLB, *DLHOME, *DLAL;
static void dl_err(char **e, const char *t) { g_free(*e); *e = t ? g_strdup(t) : NULL; if (t) set_note("dlerr %s", t); }
static void al_cred(const char *n, const char *p, void *d)
{
	(void)d; if (!n) { redraw(); return; } char *b = cred_body(n, p, NULL), er[300]; RuiJ *j = post_body("devlink/allow?on=1", b, er, sizeof er); wipe(b); dl_err(&DLALERR, j ? NULL : (*er ? er : "It could not be allowed.")); rui_jfree(j); redraw();
}
static void on_dlal(int on, void *d)
{
	(void)d; dl_err(&DLALERR, NULL);
	if (!on) { char er[300]; RuiJ *j = post_body("devlink/allow?on=0", NULL, er, sizeof er); if (j) { char *l = g_strdup_printf("%s disallowed the development link", ME); set_log_evt("sys", l); g_free(l); } else dl_err(&DLALERR, er); rui_jfree(j); redraw(); return; }
	rui_check_set(DLAL, 0); set_note("elevate cred Allow the development link"); rui_elevate_cred(SW, "Allow the development link on this computer: an administrator can then let commands from a GitHub repository look at (or, if allowed, change) this computer.", al_cred, NULL);
}
static void allow_row(void)
{
	DLAL = p_check("Allow the development link on this computer", jb(DL, "allowed"), on_dlal, NULL); p_label_named("dlal", DLAL); g_object_set_data(G_OBJECT(DLAL), "set-weight", GINT_TO_POINTER(600));
	if (!set_admin() && !jb(DL, "allowed")) gtk_widget_set_sensitive(DLAL, FALSE);
	p_msg_m("Unticked, it cannot be switched on at all; unticking it ends it at once. Ticking it needs an administrator's password.", 0, 10, 13);
	GtkWidget *e = p_msg_m(DLALERR ? DLALERR : "", 0, 10, 13); g_object_set_data(G_OBJECT(e), "set-color", "#ee6666"); p_label_named("dlalerr", e);
}
static char *ON_REPO, *ON_Q; static int ON_CH;
static void on_cred(const char *n, const char *p, void *d)
{
	(void)d; if (!n) return; const char *tok = gtk_entry_get_text(GTK_ENTRY(DLK)); char *t = jstr_esc(tok), *more = g_strdup_printf(",\"token\":%s", t); wipe(t);
	char *b = cred_body(n, p, more), er[300]; wipe(more); dl_err(&DLERR, "Checking the token with GitHub…"); RuiJ *j = post_body(ON_Q, b, er, sizeof er); wipe(b); gtk_entry_set_text(GTK_ENTRY(DLK), "");
	dl_err(&DLERR, j ? NULL : (*er ? er : "It could not be switched on.")); rui_jfree(j); redraw();
}
static void on_dlon(GtkButton *bt, gpointer d)
{
	(void)bt; (void)d; dl_err(&DLERR, NULL); char *repo = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(DLR)))); const char *tok = gtk_entry_get_text(GTK_ENTRY(DLK));
	if (!g_regex_match_simple("^[\\w.-]+/[\\w.-]+$", repo, 0, 0)) { dl_err(&DLERR, "Write the repository as owner/name."); g_free(repo); redraw(); return; }
	if (!*tok && !jb(DL, "hasToken")) { dl_err(&DLERR, "Paste the token from GitHub."); g_free(repo); redraw(); return; }
	if (!jb(DL, "allowed") && rui_jprop(DL, "allowed")) { dl_err(&DLERR, "Tick \"Allow the development link on this computer\" first."); g_free(repo); redraw(); return; }
	const char *h = s_select_get(DLH); ON_CH = rui_check_get(DLCH); char *er_ = g_uri_escape_string(repo, NULL, FALSE); g_free(ON_Q);
	ON_Q = g_strdup_printf("devlink/on?repo=%s&hours=%s&ask=%d&restart=%d&home=%d&change=%d", er_, h, rui_check_get(DLA), rui_check_get(DLB), rui_check_get(DLHOME), ON_CH); g_free(er_);
	char *r = g_strdup_printf("Switch the development link on: commands from the GitHub repository %s can %s this computer for %s hour(s).", repo, ON_CH ? "change" : "look at", h); g_free(ON_REPO); ON_REPO = repo;
	set_note("elevate cred %s", r); rui_elevate_cred(SW, r, on_cred, NULL); g_free(r);
}
static void fg_go(int ok, void *d) { (void)d; if (!ok) return; char er[200]; RuiJ *j = post_body("devlink/forget", NULL, er, sizeof er); rui_jfree(j); redraw(); }
static void on_dlforget(GtkButton *b, gpointer d) { (void)b; (void)d; set_note("elevate Delete the development link's token"); rui_elevate(SW, "Delete the development link's token from this computer.", fg_go, NULL); }
static void on_dloff(GtkButton *b, gpointer d) { (void)b; (void)d; char er[200]; RuiJ *j = post_body("devlink/off", NULL, er, sizeof er); rui_jfree(j); redraw(); }
static void ch_cred(const char *n, const char *p, void *d) { (void)d; if (!n) { redraw(); return; } char *b = cred_body(n, p, NULL), er[300]; RuiJ *j = post_body("devlink/change?on=1", b, er, sizeof er); wipe(b); dl_err(&DLCERR, j ? NULL : (*er ? er : "It could not be allowed.")); rui_jfree(j); redraw(); }
static void on_dlc(int on, void *d)
{
	(void)d; dl_err(&DLCERR, NULL); if (!on) { char er[300]; RuiJ *j = post_body("devlink/change?on=0", NULL, er, sizeof er); if (!j) dl_err(&DLCERR, er); rui_jfree(j); redraw(); return; }
	char *r = g_strdup_printf("Allow the development link to change this computer: commands from %s can then install, change settings and restart it.", set_jstr(DL, "repo", "")); set_note("elevate cred %s", r); rui_elevate_cred(SW, r, ch_cred, NULL); g_free(r);
}
static char *left(double s) { int m = (int)fmax(0, round((s == s ? s : 0) / 60)); return m >= 60 ? g_strdup_printf("%d h %d min", m / 60, m % 60) : g_strdup_printf("%d min", m); }
static void devl_sec(void)
{
	rui_jfree(DL); DL = get("devlink"); p_h4("Development link"); int adm = set_admin();
	if (!DL || !jb(DL, "available")) { p_msg_m("This Rudolf OS has no development link.", 0, 10, 13); return; }
	if (jb(DL, "on")) { RuiJ *st = rui_jprop(DL, "status"); char un[16] = ""; double u = jnum(DL, "until"); if (u == u) { time_t t = (time_t)u; struct tm lt; localtime_r(&t, &lt); strftime(un, sizeof un, "%I:%M %p", &lt); } char *lf = left(jnum(DL, "secondsLeft"));
		const char *ss = set_jstr(st, "state", ""), *se = set_jstr(st, "error", "");
		char *t = g_strdup_printf("On until %s (%s left): commands from the GitHub repository %s %s%s%s. It turns itself off after 4 hours without a command.%s%s%s%s%s", un, lf, set_jstr(DL, "repo", ""), jb(DL, "change") ? "can change this computer" : "can look at this computer (read-only)",
			jb(DL, "ask") ? "; each one waits for you to allow it" : ", without asking", jb(DL, "restart") ? ", also after a restart (with Ask first on again)" : ", until the computer restarts", *ss && strcmp(ss, "on") ? " Now: " : "", *ss && strcmp(ss, "on") ? ss : "", *ss && strcmp(ss, "on") ? "." : "", *se ? " " : "", *se ? se : "");
		p_label_named("dls", p_msg_m(t, 0, 10, 13)); g_free(t); g_free(lf); allow_row();
		GtkWidget *c = p_check("Allow changes to this computer", jb(DL, "change"), on_dlc, NULL); p_label_named("dlc", c); if (!jb(DL, "change") && (!adm || (rui_jprop(DL, "allowed") && !jb(DL, "allowed")))) gtk_widget_set_sensitive(c, FALSE);
		p_msg_m("Unticked, its commands can only look: read files and logs, take screenshots, measure. Ticked, they can also install, change settings and restart. Ticking it needs an administrator's password; it is unticked again at every start.", 0, 10, 13);
		GtkWidget *e = p_msg_m(DLCERR ? DLCERR : "", 0, 10, 13); g_object_set_data(G_OBJECT(e), "set-color", "#b91c1c");
		BRow *r = brow(); bbtn(r, "End now", 1, G_CALLBACK(on_dloff), NULL, "dloff", 0); bbtn(r, "Log…", 0, G_CALLBACK(on_go), "devllog", "dllog", 0); p_custom(brow_lay, r, 14, 0); return; }
	p_label_named("dls", p_msg_m("Off. When it is on, a developer you choose (for example a Claude session) can run tests on this computer through a private GitHub repository you own: run commands (read-only unless you allow changes), take screenshots and send files; with changes allowed, also bring files and restart it. Use it on a test computer only, never on one with personal files. A red banner shows while it is on.", 0, 10, 13));
	allow_row();
	Cell *c = p_r2("Repository"); DLR = rui_entry("owner/name (private)"); gtk_entry_set_text(GTK_ENTRY(DLR), set_jstr(DL, "repo", "")); c_add(c, DLR, 0, 0, 1); p_label_named("dlr", DLR); p_r2_more(c);
	c = p_r2("Token"); DLK = rui_entry(jb(DL, "hasToken") ? "kept from last time (paste a new one to replace it)" : NULL); gtk_entry_set_visibility(GTK_ENTRY(DLK), FALSE); c_add(c, DLK, 0, 0, 1); p_label_named("dlk", DLK); p_r2_more(c);
	static const char *const HV[4] = { "1", "2", "4", "8" }, *const HL[4] = { "1 hour", "2 hours", "4 hours", "8 hours" };
	c = p_r2("For"); DLH = s_select(HV, HL, 4, "2", NULL, NULL); c_add(c, DLH, 0, 0, 1); p_label_named("dlh", DLH);
	DLA = p_check("Ask first: each command waits for me to allow it (on again after every restart)", 1, NULL, NULL); p_label_named("dla", DLA);
	DLCH = p_check("Allow changes to this computer (unticked: its commands can only look; unticked again at every start)", 0, NULL, NULL); p_label_named("dlch", DLCH);
	DLB = p_check("Keep on after restarting (until its time is up)", 0, NULL, NULL); p_label_named("dlb", DLB); if (!jb(DL, "persistent")) { gtk_widget_set_sensitive(DLB, FALSE); gtk_widget_set_tooltip_text(DLB, "Needs the files kept on this drive (RUDOLFDATA)"); }
	DLHOME = p_check("Let it read files in home folders", jb(DL, "home"), NULL, NULL); p_label_named("dlhome", DLHOME);
	p_msg_m("\x03It turns itself off after 4 hours without a command, and when the computer restarts unless you keep it on. On github.com, make a private repository, then a fine-grained personal access token for that repository only, with the permission <b>Contents: Read and write</b>. Paste it here; it is checked with GitHub, kept where only the system can read it, and never shown again. Revoking it on GitHub ends all access.", 0, 10, 13);
	GtkWidget *e = p_msg_m(DLERR ? DLERR : "", 0, 10, 13); g_object_set_data(G_OBJECT(e), "set-color", "#ee6666"); p_label_named("dlerr", e);
	BRow *r = brow(); GtkWidget *on = bbtn(r, "Switch on…", 1, G_CALLBACK(on_dlon), NULL, "dlon", 1); if (rui_jprop(DL, "allowed") && !jb(DL, "allowed")) gtk_widget_set_sensitive(on, FALSE);
	if (jb(DL, "hasToken")) bbtn(r, "Forget the token", 0, G_CALLBACK(on_dlforget), NULL, "dlfg", 1); bbtn(r, "Log…", 0, G_CALLBACK(on_go), "devllog", "dllog", 0); p_custom(brow_lay, r, 14, 0);
}

/* ================================================================== Smart cards (sc60.js scSection) */
static void on_scl(int on, void *d) { (void)d; static const char *const K[1] = { "sclock" }; const char *V[1] = { on ? "true" : "false" }; set_store(K, V, 1); set_note("keep sclock %s", V[0]); }
static void dodrm_cred(const char *n, const char *p, void *d) { (void)d; if (!n) return; char *b = cred_body(n, p, NULL), er[300]; RuiJ *j = post_body("sc/dod/remove", b, er, sizeof er); wipe(b); set_notify("DoD certificates", j ? "Removed." : er, !j); rui_jfree(j); redraw(); }
static void on_dodrm(GtkButton *b, gpointer d) { (void)b; (void)d; set_note("elevate cred Remove the DoD's certificates"); rui_elevate_cred(SW, "Remove the DoD's certificates from Oracle", dodrm_cred, NULL); }
static void sc_sec(void)
{
	p_h4("Smart cards"); RuiJ *s = get("sc/status");
	if (!s) p_msg("The smart card service is not available.", 0);
	else { RuiJ *rs = rui_jprop(s, "readers"); int n = rs ? rui_jlen(rs) : 0;
		if (!n) p_label_named("scr", p_msg(jb(s, "pcscd") ? "No card reader is plugged in." : "No card reader is plugged in. Plug one in to use a smart card.", 0));
		/* (a reader without a card; a card's certificates are read by the page (scCert): their names and dates show there) */
		for (int i = 0; i < n; i++) { RuiJ *r = rui_jat(rs, i); char *t = jb(r, "card") ? g_strdup_printf("💳 %s: %s%s%s%s. ", set_jstr(r, "name", ""), *set_jstr(r, "label", "") ? set_jstr(r, "label", "") : "a card", *set_jstr(r, "maker", "") ? " (" : "", set_jstr(r, "maker", ""), *set_jstr(r, "maker", "") ? ")" : "")
				: g_strdup_printf("💳 %s: no card in it.", set_jstr(r, "name", "")); p_msg(t, 0); g_free(t); } }
	p_msg("Sign in to websites in Oracle with a smart card, such as a DoD Common Access Card (CAC) or a PIV card: when a site asks, Oracle shows the card's certificates, then asks for the card's PIN. Rudolf OS never sees the PIN. A card locks itself after 3 wrong PINs, and only its issuer can unlock it. Not used in Tor windows. With the Lite desktop, open the site in Chromium (Oracle's ☰ menu).", 0);
	RuiJ *dl = get("sc/dod/list"); int nc = dl && rui_jprop(dl, "certs") ? rui_jlen(rui_jprop(dl, "certs")) : 0; int adm = set_admin();
	Cell *c = p_r2("DoD certificates"); char *t = !dl ? g_strdup("Not available.") : nc ? g_strdup_printf("Installed (%d certificates).", nc) : g_strdup("Not installed."); c_text(c, t, 400); g_free(t);
	GtkWidget *bi = rui_button("Install the DoD's certificates", 1); g_signal_connect(bi, "clicked", G_CALLBACK(on_go), "dodinstall"); if (!adm) gtk_widget_set_sensitive(bi, FALSE); c_line(c, 14); c_add(c, bi, 0, 0, 0); p_label_named("scdi", bi);
	if (nc) { GtkWidget *br = rui_button("Remove them", 0); g_signal_connect(br, "clicked", G_CALLBACK(on_dodrm), NULL); if (!adm) gtk_widget_set_sensitive(br, FALSE); c_add(c, br, 8, 0, 0); p_label_named("scdr", br); }
	c_msg(c, "Sites such as iPERMS and IPPS-A use the DoD's own certificates, which Oracle does not trust by itself. They are downloaded from the DoD's PKI site and installed only if they come from the DoD's root certificates; trusted for websites only, for every account on this computer. The DoD does not support Linux, so a site may still refuse or behave differently.", 0); p_r2_more(c);
	RuiJ *st = set_stored(); c = p_r2("Card taken out"); GtkWidget *l = c_check(c, "Lock the screen when my card is taken out", jb(st, "sclock"), on_scl, NULL); p_label_named("scl", l); rui_jfree(st); rui_jfree(s); rui_jfree(dl);
}

/* ================================================================== Two-step sign-in (otp60.js otpPolicy): bd95sys's mfa */
static char *OTPV;
static void otp_go(int ok, void *d)
{
	(void)d; if (!ok) { redraw(); return; } RuiJ *o = set_ls_get("bd95sys"), *v = X(o); GString *b = g_string_new("{"); int first = 1;
	for (int i = 0; v && rui_jtype(v) == 5 && i < rui_jlen(v); i++) { const char *k = rui_jkey(v, i); if (!strcmp(k, "mfa") || (!strcmp(k, "guest") && !strcmp(OTPV, "all"))) continue; if (!first) g_string_append_c(b, ','); first = 0; char *e = set_jesc(k); g_string_append_printf(b, "\"%s\":", e); g_free(e); set_jw(b, rui_jprop(v, k)); }
	g_string_append_printf(b, "%s\"mfa\":\"%s\"%s}", first ? "" : ",", OTPV, !strcmp(OTPV, "all") ? ",\"guest\":false" : ""); set_ls_set("bd95sys", b->str); set_note("svj bd95sys %s", b->str); g_string_free(b, TRUE); rui_jfree(o);
	char *l = g_strdup_printf("%s set two-step sign-in: %s", ME, OTPV); set_log_evt("sys", l); g_free(l); redraw();
}
static void on_otp(const char *v, void *d) { (void)d; g_free(OTPV); OTPV = g_strdup(v); set_note("elevate Change who must use two-step sign-in"); rui_elevate(SW, "Change who must use two-step sign-in", otp_go, NULL); }
static void otp_sec(void)
{
	p_h4("Two-step sign-in"); RuiJ *o = set_ls_get("bd95sys"); const char *cur = set_jstr(X(o), "mfa", "off");
	static const char *const V[3] = { "off", "admin", "all" }, *const L[3] = { "Not required", "Required for administrators", "Required for every account" };
	Cell *c = p_r2("Required"); GtkWidget *s = s_select(V, L, 3, cur, on_otp, NULL); c_add(c, s, 0, 0, 1); p_label_named("otpq", s); if (!set_admin()) gtk_widget_set_sensitive(s, FALSE); rui_jfree(o);
	p_msg("An account without it is asked to set up an authenticator app before its desktop opens. The Guest account cannot use it, so it is turned off while every account must.", 0);
}

void page_sec(void)
{
	if (!SEC_LOADED || !sec_dirty()) { sec_load(&SECC); if (!SEC_LOADED || !sec_dirty()) sec_copy(&SECW, &SECC); SEC_LOADED = 1; }
	rui_jfree(QUARO); QUARO = set_ls_get("bd95quar"); QN = X(QUARO) && rui_jtype(X(QUARO)) == 4 ? rui_jlen(X(QUARO)) : 0; rui_jfree(SLOGO); SLOGO = set_ls_get("bd95seclog");
	p_h3("Security Settings"); top_sec(); fw_sec(); vault_sec(); web_sec(); av_sec(); quar_log_sec(); sb_sec(); devm_sec(); devl_sec(); sc_sec(); otp_sec();
	if (set_admin()) p_footer(sec_dirty, foot);
}
