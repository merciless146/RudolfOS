/*
 * rudolf-settings: Update and recovery (settings25.js updPanel; sysupd25.js's Security updates; sysext60.js's System extensions;
 * startup60.js's Start-up of this drive; bring62.js's Bring in files from the old computer; boot30.js's Boot menu). 1.6.2,
 * docs/plan-1.6.2.md Part 39. The same rows from the same rudolfd requests (sys/status, sysext/status, info, check, install/images,
 * disks, bootmenu, bring/found), the same actions (sys/check, sys/download, sys/rollback, sys/stable and sys/tostable, sys/set,
 * sys/beta, sys/urlcheck, sysext/..., pkg/refresh, install, factory-reset, sys/startup-check, sys/startup-files, bootmenu/set,
 * bootmenu/windows, power?action=reboot, rudolfd's job followed as the page follows it), with the administrator's permission where
 * the page asks for it: checked by rudolfd (desktop/admin), and where the page hands the name and password on ({cred:true}) they
 * go in the request's body for rudolfd to check again, then are forgotten.
 * Not native yet (docs/plan-1.6.2.md Part 39, PR 2b): Bring them in (bring/import and the page's antivirus check of what came in)
 * and an installation's offer to bring in the old computer's files first (bring62.js bringStep). GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings.h"

static RuiJ *ST, *SX, *INFO, *DISKS, *BOOT, *IMAGES; static int BUSY, ADM, ADV_OPEN;
static GtkWidget *UURL, *JL;
static GString *JLOG;
static char *esc(const char *s) { return g_markup_escape_text(s ? s : "", -1); }
static char *fmtb(double b) { if (b < 1024) return g_strdup_printf("%.0f B", b); if (b < 1048576) return g_strdup_printf("%.1f KB", b / 1024); if (b < 1073741824) return g_strdup_printf("%.2f MB", b / 1048576); return g_strdup_printf("%.1f GB", b / 1073741824); }
static char *upper(const char *s) { return g_ascii_strup(s ? s : "", -1); }
static void redraw(void) { set_show("upd"); }
static gboolean redraw_idle(gpointer d) { (void)d; redraw(); return G_SOURCE_REMOVE; }
static char *jesc(const char *s) { GString *b = g_string_new(NULL); for (; s && *s; s++) { unsigned char c = *s; if (c == '"' || c == '\\') g_string_append_printf(b, "\\%c", c); else if (c < 32) g_string_append_printf(b, "\\u%04x", c); else g_string_append_c(b, c); } return g_string_free(b, FALSE); }
static char *cred_body(const char *n, const char *p) { char *a = jesc(n), *b = jesc(p), *r = g_strdup_printf("{\"n\":\"%s\",\"p\":\"%s\"}", a, b); explicit_bzero(b, strlen(b)); g_free(a); g_free(b); return r; }
static int post(const char *path, const char *body, char *msg, size_t n)
{
	char *out = NULL; int c = rui_http("POST", path, body, body ? strlen(body) : 0, &out, NULL); RuiJ *j = out ? rui_jparse(out) : NULL;
	if (msg) snprintf(msg, n, "%s", set_jstr(j, c == 200 ? "message" : "error", c ? "" : "rudolfd did not answer"));
	set_note("POST %s %d", path, c); rui_jfree(j); g_free(out); return c;
}
static void dlg(const char *title, const char *text, const char *const *b, int n, void (*cb)(int, void *), void *d) { rui_confirm(SW, title, text, b, n, 1, cb, d); set_note("ask %s", title); }

/* ---- rudolfd's job followed (sysuJob, sxJobWait, the installer's): its lines into the page's log, then done(status) */
typedef struct { void (*done)(int st, void *d); void *d; guint id; size_t seen; } Job;
static gboolean job_tick(gpointer p)
{
	Job *j = p; RuiJ *r = rui_get("/api/job"); if (!r) return G_SOURCE_CONTINUE; const char *log = set_jstr(r, "log", "");
	if (!JLOG) JLOG = g_string_new(NULL); g_string_assign(JLOG, log); if (JL) { p_text_set(JL, JLOG->str); }
	int run = rui_jbool(r, "running"), st = (int)rui_jnum(r, "status", -1); rui_jfree(r);
	if (run) return G_SOURCE_CONTINUE;
	BUSY = 0; set_note("job done %d", st); if (j->done) j->done(st, j->d); g_free(j); return G_SOURCE_REMOVE;
}
static void job_follow(void (*done)(int, void *), void *d) { Job *j = g_new0(Job, 1); j->done = done; j->d = d; BUSY = 1; if (JLOG) g_string_truncate(JLOG, 0); j->id = g_timeout_add(700, job_tick, j); }
static void job_redraw(int st, void *d) { (void)st; (void)d; g_idle_add(redraw_idle, NULL); }
static void sys_job(const char *what, const char *body)
{
	char p[80]; snprintf(p, sizeof p, "/api/sys/%s", what); char m[300]; int c = post(p, body, m, sizeof m);
	if (c != 200) { set_notify("Updates", m, 1); redraw(); return; } job_follow(job_redraw, NULL); redraw();
}

/* ================================================================== the version line (its What's new) */
typedef struct { double lx, lw; } Ver;
static gboolean ver_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Ver *v = d; char *a = g_strdup_printf("Rudolf OS %s", rui_version()), *b = g_strdup_printf(" (%s) · ", rui_version_date()); double wa, wb, wl;
	s_lines(a, RUI.fs, 700, 0, &wa); s_lines(b, RUI.fs, 400, 0, &wb); s_lines(rui_tr("What's new"), RUI.fs, 400, 0, &wl);
	s_col(cr, "fg", 1); s_text(cr, a, 0, 0, RUI.fs, 700, 0, 0); s_text(cr, b, wa, 0, RUI.fs, 400, 0, 0);
	cairo_set_source_rgb(cr, 0, 0, RUI.dark ? 1 : 238 / 255.0); if (RUI.dark) cairo_set_source_rgb(cr, .62, .7, 1);
	s_text(cr, rui_tr("What's new"), wa + wb, 0, RUI.fs, 400, 0, 0); double lh = s_lh(RUI.fs, 400);
	cairo_rectangle(cr, (wa + wb) * s_z(), round((lh - 2) * s_z()), wl * s_z(), s_z()); cairo_fill(cr);
	v->lx = wa + wb; v->lw = wl; g_free(a); g_free(b); (void)w; return TRUE;
}
static gboolean ver_click(GtkWidget *w, GdkEventButton *e, gpointer d) { Ver *v = d; double x = e->x / s_z(); (void)w; if (x >= v->lx && x <= v->lx + v->lw) { rui_shell(SW, "open help new"); set_note("open help new"); } return TRUE; }
static double ver_lay(void *d, double x, double y, double w) { GtkWidget *a = d; p_place(a, x, y, w, s_lh(RUI.fs, 400)); return s_lh(RUI.fs, 400); }

/* ================================================================== Security updates (sysupd25.js) */
typedef struct { char *line, *msg, *warn; int cls; GtkWidget *w; } Card;   /* cls: 0, 1 ok, 2 warn */
static double card_h(Card *c, double w) { double iw = w - 2 - 4 - 20, lh = s_lh(RUI.fs, 400); double h = 8 + s_lines(c->line, RUI.fs, 400, iw, NULL) * lh + 4 + fmax(18, s_lines(c->msg, 13, 400, iw, NULL) * lh) + 8 + 2;
	if (c->warn) h += 4 + fmax(18, s_lines(c->warn, 13, 400, iw, NULL) * lh); return h; }
static gboolean card_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Card *c = d; double z = s_z(), W = gtk_widget_get_allocated_width(w) / z, H = gtk_widget_get_allocated_height(w) / z, iw = W - 2 - 4 - 20, lh = s_lh(RUI.fs, 400);
	cairo_save(cr); cairo_scale(cr, z, z);
	/* (.sysucard: a 1px border, the left one 4px: green when ok, amber when a warning, else the border's colour) */
	s_rrect(cr, 0, 0, W, H, 8); s_col(cr, "bd", 1); cairo_fill(cr);
	if (c->cls == 1) { cairo_set_source_rgb(cr, 0x16 / 255.0, 0xa3 / 255.0, 0x4a / 255.0); } else if (c->cls == 2) cairo_set_source_rgb(cr, 0xd9 / 255.0, 0x77 / 255.0, 0x06 / 255.0);
	if (c->cls) { cairo_save(cr); cairo_rectangle(cr, 0, 0, 4, H); cairo_clip(cr); s_rrect(cr, 0, 0, W, H, 8); cairo_fill(cr); cairo_restore(cr); }
	cairo_set_source_rgba(cr, 0, 0, 0, 0); cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE); cairo_restore(cr);
	cairo_save(cr); cairo_scale(cr, z, z); { GdkRGBA bg; (void)bg; } cairo_restore(cr);
	/* the inside, the page's own background */
	cairo_save(cr); cairo_scale(cr, z, z); cairo_new_path(cr);
	{ double bx = 4, by = 1, bw = W - 5, bh = H - 2; cairo_move_to(cr, bx, by); cairo_line_to(cr, bx + bw - 7, by); cairo_arc(cr, bx + bw - 7, by + 7, 7, -M_PI / 2, 0); cairo_line_to(cr, bx + bw, by + bh - 7); cairo_arc(cr, bx + bw - 7, by + bh - 7, 7, 0, M_PI / 2); cairo_line_to(cr, bx, by + bh); cairo_close_path(cr); }
	if (RUI.dark) cairo_set_source_rgb(cr, 0x0f / 255.0, 0x17 / 255.0, 0x2a / 255.0); else cairo_set_source_rgb(cr, 1, 1, 1); cairo_fill(cr); cairo_restore(cr);
	double y = 1 + 8; s_col(cr, "fg", 1); s_text(cr, c->line, 4 + 10, y, RUI.fs, 400, iw, lh); y += s_lines(c->line, RUI.fs, 400, iw, NULL) * lh + 4;
	s_col(cr, "msg", 1); s_text(cr, c->msg, 4 + 10, y, 13, 400, iw, lh); y += fmax(18, s_lines(c->msg, 13, 400, iw, NULL) * lh);
	if (c->warn) { y += 4; if (RUI.dark) cairo_set_source_rgb(cr, 0xfb / 255.0, 0xbf / 255.0, 0x24 / 255.0); else cairo_set_source_rgb(cr, 0xb4 / 255.0, 0x53 / 255.0, 0x09 / 255.0); s_text(cr, c->warn, 4 + 10, y, 13, 400, iw, lh); }
	return TRUE;
}
static double card_lay(void *d, double x, double y, double w) { Card *c = d; double h = card_h(c, w); p_place(c->w, x, y, w, h); return h; }
static void card_free(gpointer p) { Card *c = p; g_free(c->line); g_free(c->msg); g_free(c->warn); g_free(c); }
/* ("checked today at 8:30 AM": sysuWhen, in the desktop's time zone) */
static char *when(double t)
{
	if (!t) return g_strdup("never"); RuiJ *o = set_stored(); const char *tz = set_jstr(o, "tz", ""); GTimeZone *z = *tz ? g_time_zone_new_identifier(tz) : NULL; if (!z) z = g_time_zone_new_utc(); rui_jfree(o);
	GDateTime *u = g_date_time_new_from_unix_utc((gint64)t), *d = g_date_time_to_timezone(u, z), *nu = g_date_time_new_from_unix_utc((gint64)set_now()), *n = g_date_time_to_timezone(nu, z);
	int same = g_date_time_get_year(d) == g_date_time_get_year(n) && g_date_time_get_day_of_year(d) == g_date_time_get_day_of_year(n); int h = g_date_time_get_hour(d);
	char *r = same ? g_strdup_printf("today at %d:%02d %s", h % 12 ? h % 12 : 12, g_date_time_get_minute(d), h < 12 ? "AM" : "PM")
		: g_strdup_printf("%d/%d/%d at %d:%02d %s", g_date_time_get_month(d), g_date_time_get_day_of_month(d), g_date_time_get_year(d), h % 12 ? h % 12 : 12, g_date_time_get_minute(d), h < 12 ? "AM" : "PM");
	g_date_time_unref(u); g_date_time_unref(d); g_date_time_unref(nu); g_date_time_unref(n); g_time_zone_unref(z); return r;
}
static char *dl_label(RuiJ *av)
{
	double sz = rui_jnum(av, "size", 0), wh = rui_jnum(av, "whole", 0); long mb = sz ? MAX(1, lround(sz / 1e6)) : 0, w = wh > 0 ? MAX(1, lround(wh / 1e6)) : 0;
	/* (on a metered connection the page says so: the native one asks rudolfd's net/metered as the page's netMetered) */
	RuiJ *m = rui_get("/api/net/metered"); int met = rui_jbool(m, "metered"), cell = met && !g_strcmp0(rui_jstr(m, "auto"), "modem"); rui_jfree(m);
	char sz2[48] = ""; if (w) snprintf(sz2, sizeof sz2, ": about %ld MB", w); else if (mb) snprintf(sz2, sizeof sz2, ": up to %ld MB", mb);
	if (cell) return g_strdup_printf("Download over mobile data%s", sz2); if (met) return g_strdup_printf("Download on this metered connection%s", sz2);
	return w ? g_strdup_printf("Download (about %ld MB)", w) : g_strdup("Download (only the changes)");
}
static void on_check(GtkButton *b, gpointer d) { (void)b; (void)d; sys_job("check", NULL); }
static void on_download(GtkButton *b, gpointer d) { (void)b; (void)d; sys_job("download", NULL); }
static void reboot_do(int i, void *d) { (void)d; if (i == 1) { char m[200]; if (post("/api/power?action=reboot", NULL, m, sizeof m) != 200) set_notify("Restart", m, 1); } }
static void on_restart(GtkButton *b, gpointer d) { (void)b; (void)d; const char *B[2] = { "Later", "Restart now" }; dlg("Restart to finish the update", "Rudolf OS restarts now and starts the updated system. Save your work first.", B, 2, reboot_do, NULL); }
static void rb_go(int ok, void *d) { (void)d; if (ok != 1) return; char m[300]; int c = post("/api/sys/rollback", NULL, m, sizeof m); g_strstrip(m); set_notify("Roll back", c == 200 ? (*m ? m : "Done.") : m, c != 200); redraw(); }
static void on_rollback(GtkButton *b, gpointer d) { (void)b; (void)d; rui_elevate(SW, "Go back to the previous version of Rudolf OS", rb_go, NULL); set_note("ask rollback system"); }
/* ---- Roll back to last stable version (sysuToStable) */
static RuiJ *STABLE;
static void ts_restart(int i, void *d) { reboot_do(i, d); }
static void ts_done(int st, void *d)
{
	const char *how = d; const char *log = JLOG ? JLOG->str : ""; char *t = g_strstrip(g_strdup(log)); const char *last = strrchr(t, '\n'); last = last ? last + 1 : t;
	if (st != 0) set_notify("Roll back to last stable version", *last ? last : "It did not work.", 1);
	else if (!strcmp(how, "running")) set_notify("Roll back to last stable version", last, 0);
	else { RuiJ *s = rui_jprop(STABLE, "stable"); char *v = g_strdup_printf("Rudolf OS %s (stable, released %s) starts when the computer restarts. Save your work first.", set_jstr(s, "version", "?"), set_jstr(s, "released", "?"));
		const char *B[2] = { "Later", "Restart now" }; dlg("Restart to roll back", v, B, 2, ts_restart, NULL); g_free(v); }
	g_free(t); g_idle_add(redraw_idle, NULL);
}
static void ts_cred(const char *n, const char *p, void *d)
{
	(void)d; if (!n) return; char *b = cred_body(n, p), m[300]; int c = post("/api/sys/tostable", b, m, sizeof m); explicit_bzero(b, strlen(b)); g_free(b);
	if (c != 200) { set_notify("Roll back to last stable version", m, 1); return; } job_follow(ts_done, (void *)set_jstr(STABLE, "how", "")); redraw();
}
static void ts_ok(int i, void *d) { (void)d; if (i != 1) return; RuiJ *s = rui_jprop(STABLE, "stable"); char *r = g_strdup_printf("Roll back to %s, the last stable version of Rudolf OS", set_jstr(s, "version", "?")); rui_elevate_cred(SW, r, ts_cred, NULL); g_free(r); }
static void ts_checked(int st, void *d)
{
	(void)st; (void)d; const char *T = "Roll back to last stable version"; rui_jfree(STABLE); STABLE = rui_get("/api/sys/stable"); RuiJ *s = rui_jprop(STABLE, "stable");
	const char *B1[1] = { "OK" };
	if (!STABLE || !s) { dlg(T, "Rudolf OS could not find out which release is the last stable one. Check the connection, then try again.", B1, 1, NULL, NULL); g_idle_add(redraw_idle, NULL); return; }
	const char *how = set_jstr(STABLE, "how", ""); if (!strcmp(how, "official")) { set_notify(T, "This computer runs a custom build: use Return to official Rudolf OS in Settings › Security.", 1); g_idle_add(redraw_idle, NULL); return; }
	char *v = g_strdup_printf("Rudolf OS %s (stable, released %s)", set_jstr(s, "version", "?"), set_jstr(s, "released", "?"));
	if (!rui_jbool(STABLE, "compatible")) { char *t = g_strdup_printf("Cannot roll back to %s", set_jstr(s, "version", "?")), *x = g_strdup_printf("%s\n%s\nYour system stays as it is. Updates keep coming as before.", v, set_jstr(STABLE, "why", "")); dlg(t, x, B1, 1, NULL, NULL); g_free(t); g_free(x); g_free(v); g_idle_add(redraw_idle, NULL); return; }
	GString *t = g_string_new(NULL);
	if (!strcmp(how, "running")) g_string_append_printf(t, "This computer already runs %s. Get beta versions is turned off, so it stays on the stable releases.", v);
	else g_string_append_printf(t, "%s starts the next time the computer restarts%s. Get beta versions is turned off, so the next update does not bring the beta back. Your accounts, files and apps are kept.", v, !strcmp(how, "slot") ? ": it is still on this computer, so nothing is downloaded" : ": it is downloaded first, as updates are (only the changes)");
	RuiJ *nt = rui_jprop(STABLE, "notes"); if (rui_jlen(nt)) { g_string_append_printf(t, "\nWhile %s runs:", set_jstr(s, "version", "?")); for (int i = 0; i < rui_jlen(nt); i++) { const char *n = rui_jsval(rui_jat(nt, i)); if (!n) continue; char *c = g_strdup(n); c[0] = g_ascii_toupper(c[0]); g_string_append_printf(t, "\n• %s.", c); g_free(c); } g_string_append(t, "\nThey are kept for when a newer version comes back."); }
	const char *B[2] = { "Cancel", !strcmp(how, "running") ? "Turn off beta versions" : "Roll back" }; dlg(T, t->str, B, 2, ts_ok, NULL); g_string_free(t, TRUE); g_free(v); g_idle_add(redraw_idle, NULL);
}
static void on_tostable(GtkButton *b, gpointer d)
{
	(void)b; (void)d; const char *T = "Roll back to last stable version"; if (!set_admin()) { set_notify(T, "Only an administrator can do this.", 1); return; }
	if (rui_jbool(ST, "developer") || !g_strcmp0(rui_jstr(ST, "signer"), "custom")) { set_notify(T, "This computer runs a custom build: use Return to official Rudolf OS in Settings › Security.", 1); return; }
	char m[300]; if (post("/api/sys/check", NULL, m, sizeof m) != 200) { set_notify(T, m, 1); return; } job_follow(ts_checked, NULL); redraw();
}
static void auto_go(int ok, void *d) { int v = GPOINTER_TO_INT(d); if (ok == 1) { char p[64], m[200]; snprintf(p, sizeof p, "/api/sys/set?k=auto&v=%d", v); if (post(p, NULL, m, sizeof m) != 200) set_notify("Updates", m, 1); } redraw(); }
static void on_auto(int on, void *d) { (void)d; rui_elevate(SW, on ? "Turn on automatic security updates" : "Turn off automatic security updates", auto_go, GINT_TO_POINTER(on)); set_note("ask auto %d", on); }
static void beta_on(const char *n, const char *p, void *d)
{
	(void)d; if (!n) { redraw(); return; } char *b = cred_body(n, p), m[300]; int c = post("/api/sys/beta?on=1", b, m, sizeof m); explicit_bzero(b, strlen(b)); g_free(b);
	if (c != 200) { set_notify("Updates", m, 1); redraw(); return; } set_notify("Updates", "Beta versions are on. Checking for updates…", 0); sys_job("check", NULL);
}
static void beta_off(int ok, void *d) { (void)d; if (ok != 1) { redraw(); return; } char m[300]; if (post("/api/sys/beta?on=0", NULL, m, sizeof m) != 200) { set_notify("Updates", m, 1); redraw(); return; }
	set_notify("Updates", "Beta versions are off: the next update brings the latest stable release. Checking for updates…", 0); sys_job("check", NULL); }
static void on_beta(int on, void *d) { (void)d; if (on) rui_elevate_cred(SW, "Get beta versions of Rudolf OS", beta_on, NULL); else rui_elevate(SW, "Stop getting beta versions of Rudolf OS", beta_off, NULL); set_note("ask beta %d", on); }
/* ---- the update address (details.uadv) */
static GtkWidget *UCK; static RuiJ *UCKR;
static void uck(const char *v)
{
	char *e = g_uri_escape_string(v, NULL, TRUE), p[1200]; snprintf(p, sizeof p, "/api/sys/urlcheck?url=%s", e); g_free(e);
	char *out = NULL; int c = rui_http("POST", p, NULL, 0, &out, NULL); rui_jfree(UCKR); UCKR = c == 200 && out ? rui_jparse(out) : NULL; GString *t = g_string_new(NULL);
	if (UCKR) { RuiJ *l = rui_jprop(UCKR, "lines"); for (int i = 0; i < rui_jlen(l); i++) { RuiJ *x = rui_jat(l, i); const char *lv = rui_jsval(rui_jat(x, 0)), *s = rui_jsval(rui_jat(x, 1)); g_string_append_printf(t, "%s%s%s", t->len ? "\n" : "", !g_strcmp0(lv, "ok") ? "✅ " : !g_strcmp0(lv, "warn") ? "⚠️ " : "❌ ", s ? s : ""); }
		const char *f = rui_jstr(UCKR, "fpr"), *k = rui_jstr(UCKR, "key"); if (f) g_string_append_printf(t, "%sKey: %s%s", t->len ? "\n" : "", f, !g_strcmp0(k, "official") ? " (the official Rudolf OS key)" : !g_strcmp0(k, "developer") ? " (Developer mode)" : " (another build)"); }
	else { RuiJ *j = out ? rui_jparse(out) : NULL; g_string_append_printf(t, "❌ %s", set_jstr(j, "error", "rudolfd did not answer")); rui_jfree(j); }
	if (UCK) p_text_set(UCK, t->str); set_note("urlcheck %d", c); g_string_free(t, TRUE); g_free(out);
}
static void on_uchk(GtkButton *b, gpointer d) { (void)b; (void)d; const char *v = gtk_entry_get_text(GTK_ENTRY(UURL)); char *t = g_strstrip(g_strdup(v)); if (!g_str_has_prefix(t, "https://")) set_notify("Updates", "The address starts with https://", 1); else uck(t); g_free(t); }
static char *SAVEURL;
static void save_cred(const char *n, const char *p, void *d)
{
	(void)d; if (!n) return; char *e = g_uri_escape_string(SAVEURL, NULL, TRUE), *path = g_strdup_printf("/api/sys/set?k=url&v=%s", e); g_free(e);
	char *b = cred_body(n, p), m[300]; int c = post(path, b, m, sizeof m); explicit_bzero(b, strlen(b)); g_free(b); g_free(path);
	if (c != 200) { set_notify("Updates", m, 1); return; } set_notify("Updates", "Saved. Checking for updates…", 0); sys_job("check", NULL);
}
static void save_ask(int i, void *d) { (void)d; if (i == 1) rui_elevate_cred(SW, "Change where Rudolf OS gets updates", save_cred, NULL); }
static void on_usave(GtkButton *bt, gpointer d)
{
	(void)bt; (void)d; g_free(SAVEURL); SAVEURL = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(UURL))));
	if (*SAVEURL && !g_str_has_prefix(SAVEURL, "https://")) { set_notify("Updates", "The address starts with https://", 1); return; }
	if (*SAVEURL) { uck(SAVEURL); if (!UCKR) return; const char *k = set_jstr(UCKR, "key", ""); const char *B[2] = { "Cancel", "Save it" };
		if (!strcmp(k, "other")) { char *t = g_strdup_printf("This address is another build of Rudolf OS (key %s). Its updates do not install on this computer, so updates will stop here until the address is changed back. You can still make a USB stick from it. Save it anyway?", set_jstr(UCKR, "fpr", "")); dlg("Update address", t, B, 2, save_ask, NULL); g_free(t); return; }
		if (!strcmp(k, "none")) { dlg("Update address", "This address is not set up for this computer (see the lines above). Save it anyway?", B, 2, save_ask, NULL); return; } }
	save_ask(1, NULL);
}
typedef struct { GtkWidget *w; } Sum;
static gboolean sum_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	/* (summary: Chromium's disclosure triangle, then the words) */
	(void)d; double z = s_z(), lh = s_lh(RUI.fs, 400); s_col(cr, "fg", 1); cairo_save(cr); cairo_scale(cr, z, z);
	if (ADV_OPEN) { cairo_move_to(cr, 0.5, lh / 2 - 3); cairo_line_to(cr, 8.5, lh / 2 - 3); cairo_line_to(cr, 4.5, lh / 2 + 3); }
	else { cairo_move_to(cr, 1.5, lh / 2 - 4); cairo_line_to(cr, 7.5, lh / 2); cairo_line_to(cr, 1.5, lh / 2 + 4); }
	cairo_close_path(cr); cairo_fill(cr); cairo_restore(cr); s_text(cr, rui_tr("Update address"), 14, 0, RUI.fs, 400, 0, lh); (void)w; return TRUE;
}
static gboolean sum_click(GtkWidget *w, GdkEventButton *e, gpointer d) { (void)w; (void)e; (void)d; ADV_OPEN = !ADV_OPEN; set_note("details %d", ADV_OPEN); g_idle_add(redraw_idle, NULL); return TRUE; }
static double sum_lay(void *d, double x, double y, double w) { p_place(d, x, y, w, s_lh(RUI.fs, 400)); return s_lh(RUI.fs, 400); }
/* (the address row: the field as wide as is left, its two buttons) */
typedef struct { GtkWidget *e, *b1, *b2; } URow;
static double urow_lay(void *d, double x, double y, double w) { URow *u = d; double w1 = p_nat_w(u->b1), w2 = p_nat_w(u->b2), h = p_nat_h(u->b1); p_place(u->e, x, y, w - w1 - w2 - 16, h); p_place(u->b1, x + w - w1 - w2 - 8, y, 0, 0); p_place(u->b2, x + w - w2, y, 0, 0); return h; }
/* the job's log (pre.jl): the page's own, shown once something ran */
static gboolean jl_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)d; const char *t = JLOG ? JLOG->str : ""; double W = gtk_widget_get_allocated_width(w) / s_z(), H = gtk_widget_get_allocated_height(w) / s_z();
	cairo_save(cr); cairo_scale(cr, s_z(), s_z()); cairo_set_source_rgb(cr, 0x0f / 255.0, 0x17 / 255.0, 0x2a / 255.0); s_rrect(cr, 0, 0, W, H, 6); cairo_fill(cr); cairo_restore(cr);
	cairo_set_source_rgb(cr, 0xe2 / 255.0, 0xe8 / 255.0, 0xf0 / 255.0); int n = s_lines(t, 11, 400, W - 16, NULL); double lh = s_lh(11, 400), off = fmax(0, n * lh - (H - 16));
	cairo_save(cr); cairo_rectangle(cr, 8 * s_z(), 8 * s_z(), (W - 16) * s_z(), (H - 16) * s_z()); cairo_clip(cr); s_text(cr, t, 8, 8 - off, 11, 400, W - 16, lh); cairo_restore(cr); return TRUE; }
static double jl_lay(void *d, double x, double y, double w) { if (!JLOG || !JLOG->len) { p_place(d, x, y, 1, 1); gtk_widget_set_visible(d, FALSE); return 0; } gtk_widget_set_visible(d, TRUE); double h = fmin(120, 16 + s_lines(JLOG->str, 11, 400, w - 16, NULL) * s_lh(11, 400)); p_place(d, x, y, w, h); return h; }
static void p_jl(void) { GtkWidget *a = gtk_drawing_area_new(); g_signal_connect(a, "draw", G_CALLBACK(jl_draw), NULL); p_put(a); JL = a; p_custom(jl_lay, a, 0, 0); }

static void sec_updates(void)
{
	p_h4("Security updates");
	if (!ST || (!rui_jprop(ST, "current") && !rui_jprop(ST, "persistent"))) { p_msg_m("This Rudolf OS has no automatic updates. Use a newer disc image.", 0, 10, 13); return; }
	RuiJ *cur = rui_jprop(ST, "current"), *st = rui_jprop(ST, "state"), *av = rui_jprop(ST, "available"), *slots = rui_jprop(ST, "slots"); const char *nk = rui_jstr(st, "next"); RuiJ *nx = nk ? rui_jprop(slots, nk) : NULL; if (nx && !rui_jbool(nx, "complete")) nx = NULL;
	if (rui_jtype(av) != 5) av = NULL;
	const char *ver = set_jstr(cur, "version", rui_version()), *booted = set_jstr(ST, "booted", "disc"), *good = rui_jstr(st, "good");
	Card *c = g_new0(Card, 1); char *ev = esc(ver);
	if (!rui_jbool(ST, "persistent")) { c->line = g_strdup("Security updates need Rudolf OS installed on this computer, or a USB drive with a RUDOLFDATA partition (see Install below)."); c->cls = 2; }
	else if (rui_jprop(ST, "canUpdate") && !rui_jbool(ST, "canUpdate")) { c->line = g_strdup(rui_jbool(ST, "recovery") ? "Recovery mode: updates are paused. Restart normally to update Rudolf OS." : "Rudolf OS is running from a USB drive or disc: start it from this computer's own drive to update it."); c->cls = 2; }
	else if (!strcmp(booted, "r") && !nx) { c->line = g_strdup_printf("\x03<b>Rescue system:</b> no updated Rudolf OS on this drive could start, so its rescue system (%s, kept by Make a stick) started. Install the latest update to put a working system back.", ev); c->cls = 2; }
	else if (rui_jbool(ST, "trying") && !g_strcmp0(good, booted)) { c->line = g_strdup_printf("\x03<b>Kept:</b> Rudolf OS %s started successfully and is now the system this computer starts.", ev); c->cls = 1; }
	else if (rui_jbool(ST, "trying")) c->line = g_strdup("Trying the new version: it is kept as soon as the desktop has started.");
	else if (nx) { char *v = esc(rui_jstr(nx, "version")); c->line = g_strdup_printf("\x03<b>Ready:</b> Rudolf OS %s starts when the computer restarts.", v); g_free(v); c->cls = 1; }
	else if (av) { char *v = esc(rui_jstr(av, "version")), *n = esc(rui_jstr(av, "notes")); c->line = g_strdup_printf("\x03<b>Update available:</b> Rudolf OS %s%s%s%s", v, *n ? " · " : "", n, rui_jbool(av, "urgent") ? " · <b>urgent security fix</b>" : ""); g_free(v); g_free(n); c->cls = rui_jbool(av, "urgent") ? 2 : 0;
		double wh = rui_jnum(av, "whole", 0); if (wh > 0) { const char *wl = set_jstr(av, "wholeLayers", ""); int k = 0; char **p = g_strsplit(wl, ",", -1); for (int i = 0; p[i]; i++) if (*p[i]) k++; g_strfreev(p);
			char *pk = esc(rui_jstr(av, "packed")); c->warn = g_strdup_printf("This update packs the system's images in a new way%s%s%s, so it downloads %s whole this once: about %ld MB. Later updates download only the changes again.", *pk ? " (" : "", pk, *pk ? ")" : "", k > 1 ? "them" : "it", MAX(1, lround(wh / 1e6))); g_free(pk); } }
	else if (!rui_jstr(ST, "url") || !*rui_jstr(ST, "url")) c->line = g_strdup("No update address is set yet.");
	else c->line = g_strdup("Rudolf OS is up to date.");
	char *w = when(rui_jnum(ST, "checked", 0)), *bu = upper(booted);
	c->msg = g_strdup_printf("Rudolf OS %s%s%s%s%s · checked %s%s%s", ver, rui_jstr(cur, "chromium") ? " · Chromium " : "", set_jstr(cur, "chromium", ""), rui_jstr(cur, "kernel") ? " · Linux " : "", set_jstr(cur, "kernel", ""), w,
		!strcmp(booted, "r") ? " · the rescue system" : strcmp(booted, "disc") ? " · system slot " : "", !strcmp(booted, "r") || !strcmp(booted, "disc") ? "" : bu);
	if (rui_jstr(st, "failed")) { char *f = upper(rui_jstr(st, "failed")); char *x = g_strdup_printf("The update in slot %s did not start, so Rudolf OS went back to the previous version. It will try the next release.", f); g_free(f); if (c->warn) { char *y = g_strdup_printf("%s\n%s", c->warn, x); g_free(c->warn); g_free(x); c->warn = y; } else c->warn = x; }
	g_free(w); g_free(bu); g_free(ev);
	c->w = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(c->w), "set-card", c, card_free); g_signal_connect(c->w, "draw", G_CALLBACK(card_draw), c); p_put(c->w); p_custom(card_lay, c, 4, 8);
	/* the buttons, as the page shows them */
	const char *L[5]; GCallback C[5]; int n = 0; char *dl = NULL; int pers = rui_jbool(ST, "persistent"), can = !(rui_jprop(ST, "canUpdate") && !rui_jbool(ST, "canUpdate"));
	L[n] = "Check now"; C[n++] = G_CALLBACK(on_check);
	if (av && !nx && pers && can) { dl = dl_label(av); L[n] = dl; C[n++] = G_CALLBACK(on_download); }
	if (nx) { L[n] = "Restart now"; C[n++] = G_CALLBACK(on_restart); }
	if ((good && strcmp(good, "disc")) || rui_jstr(st, "prev")) { L[n] = "Roll back system…"; C[n++] = G_CALLBACK(on_rollback); }
	int stable = pers && can && rui_jprop(ST, "runningStable") && (rui_jbool(ST, "beta") || (rui_jtype(rui_jprop(ST, "runningStable")) == 1 && !rui_jbool(ST, "runningStable")));
	if (stable) { L[n] = "Roll back to last stable version…"; C[n++] = G_CALLBACK(on_tostable); }
	GtkWidget *b0 = p_buttons(L, n, C, NULL); p_label_named("ucheck", b0);
	{ GList *ch = NULL; (void)ch; }
	if (BUSY) { /* (busy: every button off, as the page's) */ }
	g_free(dl);
	GtkWidget *ua = p_check("Download security updates automatically", rui_jbool(ST, "auto"), on_auto, NULL); gtk_widget_set_sensitive(ua, ADM && !BUSY); p_label_named("uauto", ua);
	GtkWidget *ub = p_check("Get beta versions", rui_jbool(ST, "beta"), on_beta, NULL); gtk_widget_set_sensitive(ub, ADM && !rui_jbool(ST, "developer") && !BUSY); p_label_named("ubeta", ub);
	p_msg_m(rui_jbool(ST, "beta") ? "This computer gets beta versions: early releases for testing, which can have problems. Turn this off to go back to the latest stable release at the next update."
		: rui_jbool(ST, "toStable") ? "Beta versions are off: the next update brings the latest stable release." : "Beta versions are early releases for testing. Turning them on needs an administrator's name and password.", 0, 10, 13);
	if (rui_jstr(ST, "recoveryVersion")) { char d2[40] = ""; double ra = rui_jnum(ST, "recoveryAt", 0); if (ra) { GDateTime *t = g_date_time_new_from_unix_utc((gint64)ra); snprintf(d2, sizeof d2, " (%d/%d/%d)", g_date_time_get_month(t), g_date_time_get_day_of_month(t), g_date_time_get_year(t)); g_date_time_unref(t); }
		char *t = g_strdup_printf("Recovery mode (in the start-up menu) starts the official Rudolf OS %s, the newest official stable release when it was last renewed%s. It is kept up to date from the official address, whatever the update address.", rui_jstr(ST, "recoveryVersion"), d2); p_msg_m(t, 0, 10, 13); g_free(t); }
	GtkWidget *sm = gtk_drawing_area_new(); gtk_widget_add_events(sm, GDK_BUTTON_PRESS_MASK); g_signal_connect(sm, "draw", G_CALLBACK(sum_draw), NULL); g_signal_connect(sm, "button-press-event", G_CALLBACK(sum_click), NULL); p_put(sm); p_custom(sum_lay, sm, 6, 0); p_label_named("uadv", sm);
	if (ADV_OPEN) { URow *u = g_new0(URow, 1); u->e = rui_entry("https://…/stable"); gtk_entry_set_text(GTK_ENTRY(u->e), set_jstr(ST, "url", "")); gtk_widget_set_sensitive(u->e, ADM); UURL = u->e;
		u->b1 = rui_button("Check this address", 0); u->b2 = rui_button("Save", 0); gtk_widget_set_sensitive(u->b2, ADM); g_signal_connect(u->b1, "clicked", G_CALLBACK(on_uchk), NULL); g_signal_connect(u->b2, "clicked", G_CALLBACK(on_usave), NULL);
		p_put(u->e); p_put(u->b1); p_put(u->b2); p_custom(urow_lay, u, 14, 0); p_label_named("uurl", u->e); p_label_named("uchk", u->b1); p_label_named("usave", u->b2);
		UCK = p_msg_m("", 0, 10, 0); p_msg_m("Releases must be signed by a key this computer trusts; others are refused.", 0, 10, 13); }
	p_jl();
}

/* ================================================================== System extensions (sysext60.js) */
static char *sxsize(double b) { return b >= 1048576 ? g_strdup_printf("%.1f MB", b / 1048576) : g_strdup_printf("%ld KB", MAX(1, lround(b / 1024))); }
typedef struct { GtkWidget *area; GPtrArray *rows; GtkWidget *btn[16][2]; int nb[16]; char *empty; double cw[5], rh[16], hh; } SxT;   /* rows: of 5 strings, [0] "title\nsummary" */
static const char *const SXH[5] = { "Extension", "Version", "Size", "State", "" };
static double cell_min(const char *t, int wt) { double m = 0; char **w = g_regex_split_simple("\\s+", t, 0, 0); for (int i = 0; w[i]; i++) if (*w[i]) { double x; s_lines(w[i], 13, wt, 0, &x); m = fmax(m, x); } g_strfreev(w); return m; }
static double sx_lay(void *d, double x, double y, double W)
{
	SxT *t = d; double mn[5], mx[5], lh = s_lh(13, 400), pad = 12;
	for (int c = 0; c < 5; c++) { double hw; s_lines(rui_tr(SXH[c]), 13, 700, 0, &hw); mx[c] = hw + pad; mn[c] = cell_min(rui_tr(SXH[c]), 700) + pad; }
	for (guint r = 0; r < t->rows->len; r++) { char **row = g_ptr_array_index(t->rows, r);
		for (int c = 0; c < 4; c++) { char **parts = g_strsplit(row[c], "\n", 2); for (int k = 0; parts[k]; k++) { double tw; s_lines(parts[k], 13, c == 0 && k == 0 ? 700 : 400, 0, &tw); mx[c] = fmax(mx[c], tw + pad); mn[c] = fmax(mn[c], cell_min(parts[k], c == 0 && k == 0 ? 700 : 400) + pad); } g_strfreev(parts); }
		double bw = 0, bmx = 0; for (int b = 0; b < t->nb[r]; b++) { double w = p_nat_w(t->btn[r][b]); bw += w; bmx = fmax(bmx, w); } mx[4] = fmax(mx[4], bw + pad); mn[4] = fmax(mn[4], bmx + pad); }
	double smx = 0, smn = 0; for (int c = 0; c < 5; c++) { smx += mx[c]; smn += mn[c]; }
	if (t->rows->len == 0) { for (int c = 0; c < 5; c++) t->cw[c] = mx[c] + (W - smx) * mx[c] / smx; }
	else if (smx <= W) for (int c = 0; c < 5; c++) t->cw[c] = mx[c] + (W - smx) * mx[c] / smx;
	else { double sp = smx - smn; for (int c = 0; c < 5; c++) t->cw[c] = mn[c] + (W - smn) * (sp > 0 ? (mx[c] - mn[c]) / sp : .2); }
	t->hh = 4 + s_lh(13, 700) + 4 + .5; double yy = t->hh;   /* (border-collapse: the heading's line shared with the first row's, half each) */
	if (!t->rows->len) yy += 8 + s_lines(t->empty, 13, 400, W - pad, NULL) * lh + 1;
	for (guint r = 0; r < t->rows->len; r++) { char **row = g_ptr_array_index(t->rows, r); double h = 0;
		for (int c = 0; c < 4; c++) { double ch; if (c == 0) { char **p = g_strsplit(row[0], "\n", 2); ch = s_lines(p[0], 13, 700, t->cw[0] - pad, NULL) * lh + (p[1] ? 10 + fmax(18, s_lines(p[1], 13, 400, t->cw[0] - pad, NULL) * lh) : 0); g_strfreev(p); } else ch = s_lines(row[c], 13, 400, t->cw[c] - pad, NULL) * lh; h = fmax(h, ch); }
		double bh = t->nb[r] ? p_nat_h(t->btn[r][0]) : 0; h = fmax(h, bh); t->rh[r] = h + 8 + 1;
		double bx = x; for (int c = 0; c < 4; c++) bx += t->cw[c]; bx += 6; for (int b = 0; b < t->nb[r]; b++) { p_place(t->btn[r][b], bx, y + yy + (t->rh[r] - 1 - bh) / 2, 0, 0); bx += p_nat_w(t->btn[r][b]); }
		yy += t->rh[r]; }
	yy += .5; p_place(t->area, x, y, W, yy); return yy;
}
static gboolean sx_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	SxT *t = d; double z = s_z(), W = gtk_widget_get_allocated_width(w) / z, lh = s_lh(13, 400), x = 0;
	cairo_save(cr); cairo_scale(cr, z, z); s_col(cr, "th", 1); cairo_rectangle(cr, 0, 0, W, t->hh); cairo_fill(cr); s_col(cr, "bd", 1); cairo_rectangle(cr, 0, t->hh - .5, W, 1); cairo_fill(cr); cairo_restore(cr);
	s_col(cr, "fg", 1); for (int c = 0; c < 5; c++) { s_text(cr, rui_tr(SXH[c]), x + 6, 4, 13, 700, 0, 0); x += t->cw[c]; }
	double y = t->hh;
	if (!t->rows->len) { s_col(cr, "fg", .6); s_text(cr, t->empty, 6, y + 4, 13, 400, W - 12, lh); cairo_save(cr); cairo_scale(cr, z, z); cairo_set_source_rgba(cr, 0, 0, 0, .05); double h = 8 + s_lines(t->empty, 13, 400, W - 12, NULL) * lh + 1; cairo_rectangle(cr, 0, y + h - 1, W, 1); cairo_fill(cr); cairo_restore(cr); }
	for (guint r = 0; r < t->rows->len; r++) { char **row = g_ptr_array_index(t->rows, r); double h = t->rh[r]; x = 0;
		cairo_save(cr); cairo_scale(cr, z, z); cairo_set_source_rgba(cr, 0, 0, 0, .05); cairo_rectangle(cr, 0, y + h - 1, W, 1); cairo_fill(cr); cairo_restore(cr);
		for (int c = 0; c < 4; c++) { double cw = t->cw[c] - 12;
			if (c == 0) { char **p = g_strsplit(row[0], "\n", 2); double th = s_lines(p[0], 13, 700, cw, NULL) * lh, mh = p[1] ? 10 + fmax(18, s_lines(p[1], 13, 400, cw, NULL) * lh) : 0, ty = y + (h - 1 - th - mh) / 2;
				s_col(cr, "fg", 1); s_text(cr, p[0], 6, ty, 13, 700, cw, lh); if (p[1]) { s_col(cr, "msg", 1); s_text(cr, p[1], 6, ty + th + 10, 13, 400, cw, lh); } g_strfreev(p); }
			else { double th = s_lines(row[c], 13, 400, cw, NULL) * lh; if (c == 3 && row[5] && *row[5]) { if (RUI.dark) cairo_set_source_rgb(cr, .98, .75, .14); else cairo_set_source_rgb(cr, .71, .33, .04); } else s_col(cr, "fg", 1); s_text(cr, row[c], x + 6, y + (h - 1 - th) / 2, 13, 400, cw, lh); }
			x += t->cw[c]; }
		y += h; }
	return TRUE;
}
static void sx_free(gpointer p) { SxT *t = p; g_ptr_array_unref(t->rows); g_free(t->empty); g_free(t); }
static char *SXSAID;
typedef struct { char what[16], name[128]; } SxJob;
static void sx_done(int st, void *d)
{
	SxJob *j = d; g_free(SXSAID); SXSAID = g_strdup(st == 0 ? (!strcmp(j->what, "install") ? "Installed: it is in use after a restart." : "Removed: it is gone after a restart.") : "It did not work: see the lines below.");
	if (st == 0) g_object_set_data(G_OBJECT(SW->win), "sx-changed", GINT_TO_POINTER(1)); g_free(j); rui_jfree(SX); SX = rui_get("/api/sysext/status"); g_idle_add(redraw_idle, NULL);
}
static void sx_cred(const char *n, const char *p, void *d)
{
	SxJob *j = d; if (!n) { g_free(j); return; } char *e = g_uri_escape_string(j->name, NULL, TRUE), path[300]; snprintf(path, sizeof path, "/api/sysext/%s%s%s", j->what, *j->name ? "?name=" : "", *j->name ? e : ""); g_free(e);
	char *b = cred_body(n, p), m[300]; int c = post(path, b, m, sizeof m); explicit_bzero(b, strlen(b)); g_free(b);
	if (c != 200) { g_free(SXSAID); SXSAID = g_strdup(m); g_free(j); redraw(); return; }
	if (!strcmp(j->what, "skip-once")) { g_free(SXSAID); SXSAID = g_strdup("The next start is without system extensions; the one after uses them again."); g_free(j); rui_jfree(SX); SX = rui_get("/api/sysext/status"); redraw(); return; }
	if (!strcmp(j->what, "retry")) { g_free(SXSAID); SXSAID = g_strdup("It is tried again at the next start."); g_free(j); rui_jfree(SX); SX = rui_get("/api/sysext/status"); redraw(); return; }
	job_follow(sx_done, j); redraw();
}
static void sx_admin(const char *what, const char *name, const char *reason) { SxJob *j = g_new0(SxJob, 1); snprintf(j->what, sizeof j->what, "%s", what); snprintf(j->name, sizeof j->name, "%s", name ? name : ""); rui_elevate_cred(SW, reason, sx_cred, j); set_note("ask sysext %s %s", what, j->name); }
static void sx_rm_ok(int i, void *d) { char *n = d; if (i == 1) { char *r = g_strdup_printf("Remove the system extension %s", n); sx_admin("remove", g_object_get_data(G_OBJECT(SW->win), "sx-rm"), r); g_free(r); } g_free(n); }
static void sx_remove(GtkButton *b, gpointer d)
{
	(void)d; const char *name = g_object_get_data(G_OBJECT(b), "sx-name"), *title = g_object_get_data(G_OBJECT(b), "sx-title");
	g_object_set_data_full(G_OBJECT(SW->win), "sx-rm", g_strdup(name), g_free); char *t = g_strdup_printf("Remove the system extension \"%s\"? What it adds is gone after the next restart.", title); const char *B[2] = { "Cancel", "Remove" }; dlg("Remove", t, B, 2, sx_rm_ok, g_strdup(title)); g_free(t);
}
static void sx_retry(GtkButton *b, gpointer d) { (void)d; char *r = g_strdup_printf("Try the system extension %s again at the next start", (char *)g_object_get_data(G_OBJECT(b), "sx-title")); sx_admin("retry", g_object_get_data(G_OBJECT(b), "sx-name"), r); g_free(r); }
static void sx_skip(GtkButton *b, gpointer d) { (void)b; (void)d; sx_admin("skip-once", "", "Start Rudolf OS once without system extensions"); }
static void sx_checked(int st, void *d) { (void)st; (void)d; g_free(SXSAID); SXSAID = g_strdup("Checked."); rui_jfree(SX); SX = rui_get("/api/sysext/status"); g_idle_add(redraw_idle, NULL); }
static void sx_check(GtkButton *b, gpointer d) { (void)b; (void)d; g_free(SXSAID); SXSAID = g_strdup("Checking for extensions…"); char m[300]; if (post("/api/pkg/refresh", NULL, m, sizeof m) != 200) { g_free(SXSAID); SXSAID = g_strdup(m); redraw(); return; } job_follow(sx_checked, NULL); redraw(); }
static void sx_restart_ok(int i, void *d) { reboot_do(i, d); }
static void sx_restart(GtkButton *b, gpointer d) { (void)b; (void)d; const char *B[2] = { "Later", "Restart now" }; dlg("Restart now", "Restart Rudolf OS now? The system extensions change then. Save your work first.", B, 2, sx_restart_ok, NULL); }
static RuiJ *SXAV;
static void sx_pick(int i, void *d) { (void)d; RuiJ *a = rui_jat(SXAV, i); if (i < 0 || !a) return; char *r = g_strdup_printf("Install the system extension %s", set_jstr(a, "title", set_jstr(a, "name", ""))); sx_admin("install", rui_jstr(a, "name"), r); g_free(r); }
static void sx_install(GtkButton *b, gpointer d)
{
	/* (the page's list of what can be installed: here as a menu of them, each with what it is) */
	(void)d; RuiJ *av = rui_jprop(SX, "available"); GString *js = g_string_new("[");
	int n = 0; for (int i = 0; i < rui_jlen(av); i++) { RuiJ *a = rui_jat(av, i); const char *in = rui_jstr(a, "installed"); if (in && !g_strcmp0(in, rui_jstr(a, "version"))) continue;
		if (n++) g_string_append_c(js, ','); char *nm = jesc(set_jstr(a, "name", "")), *tt = jesc(set_jstr(a, "title", set_jstr(a, "name", ""))), *v = jesc(set_jstr(a, "version", "")); g_string_append_printf(js, "{\"name\":\"%s\",\"title\":\"%s\",\"version\":\"%s\"}", nm, tt, v); g_free(nm); g_free(tt); g_free(v); }
	g_string_append_c(js, ']'); char *w = g_strdup_printf("{\"x\":%s}", js->str); rui_jfree(SXAV); SXAV = rui_jparse(w); SXAV = SXAV; g_free(w); g_string_free(js, TRUE);
	RuiJ *list = rui_jprop(SXAV, "x"); if (!rui_jlen(list)) { const char *B[1] = { "Cancel" }; dlg("Install a system extension", rui_jlen(av) ? "Every extension is installed." : "No extensions are listed for this version of Rudolf OS yet. Try Check for extensions.", B, 1, NULL, NULL); return; }
	RuiMenuItem *m = g_new0(RuiMenuItem, rui_jlen(list)); char **lab = g_new0(char *, rui_jlen(list) + 1);
	for (int i = 0; i < rui_jlen(list); i++) { RuiJ *a = rui_jat(list, i); lab[i] = g_strdup_printf("%s %s", rui_jstr(a, "title"), rui_jstr(a, "version")); m[i].label = lab[i]; }
	RuiJ *keep = SXAV; SXAV = list; (void)keep;
	int x, y, ox, oy; gdk_window_get_origin(gtk_widget_get_window(SW->win), &ox, &oy); gtk_widget_translate_coordinates(GTK_WIDGET(b), SW->win, 0, gtk_widget_get_allocated_height(GTK_WIDGET(b)), &x, &y);
	rui_menu(GTK_WIDGET(b), ox + x, oy + y, rui_tr("Install a system extension"), m, rui_jlen(list), sx_pick, NULL); g_strfreev(lab); g_free(m); set_note("sysext install list %d", rui_jlen(list)); set_state_soon();
}
static void sec_sysext(void)
{
	p_h4("System extensions");
	p_msg_m("Signed parts added to Rudolf OS without a new release (printer support, a driver, firmware). They are laid over the system at each start and never change it; one that would replace a part of Rudolf OS, or keeps Rudolf OS from starting, is left out.", 0, 10, 13);
	const char *started = rui_jstr(SX, "started"); char *sxs = started && strcmp(started, "with extensions") && strcmp(started, "none installed") && strcmp(started, "not yet") ? g_strdup_printf("This start: %s.", started) : g_strdup(rui_jbool(SX, "skipOnce") ? "The next start is without system extensions." : "");
	p_msg_m(sxs, 0, 10, 13); g_free(sxs);
	SxT *t = g_new0(SxT, 1); t->rows = g_ptr_array_new_with_free_func((GDestroyNotify)g_strfreev); t->area = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(t->area), "sx", t, sx_free); g_signal_connect(t->area, "draw", G_CALLBACK(sx_draw), t); p_put(t->area);
	int adm = set_admin(), off = 0, pend = 0; RuiJ *inst = rui_jprop(SX, "installed");
	if (!SX) { t->empty = g_strdup("The system extensions could not be read."); off = 1; }
	else if (!rui_jbool(SX, "persistent")) { t->empty = g_strdup("System extensions need Rudolf OS installed on this computer (or a RUDOLFDATA partition): they are kept there."); off = 1; }
	else if (!rui_jlen(inst)) t->empty = g_strdup("No system extensions are installed.");
	for (int i = 0; SX && rui_jbool(SX, "persistent") && i < rui_jlen(inst) && i < 16; i++) { RuiJ *e = rui_jat(inst, i); const char *title = set_jstr(e, "title", set_jstr(e, "name", "")), *sum = rui_jstr(e, "summary");
		char **row = g_new0(char *, 7); row[0] = sum && *sum ? g_strdup_printf("%s\n%s", title, sum) : g_strdup(title); row[1] = g_strdup(set_jstr(e, "version", "")); row[2] = rui_jnum(e, "size", 0) ? sxsize(rui_jnum(e, "size", 0)) : g_strdup("");
		/* (sxState) */
		const char *o = rui_jstr(e, "off"), *s = set_jstr(e, "state", ""), *pr = rui_jstr(e, "problem"); int warn = 1;
		if (o) row[3] = g_strdup_printf("Left out: %s", o); else if (!strcmp(s, "in-use")) { row[3] = pr ? g_strdup_printf("In use now; left out at the next start: %s", pr) : g_strdup("In use"); warn = pr != NULL; }
		else if (!strcmp(s, "left-out")) row[3] = g_strdup_printf("Left out: %s", set_jstr(e, "why", "see System health")); else if (pr) row[3] = g_strdup_printf("Cannot be used: %s", pr); else { row[3] = g_strdup("In use after a restart"); warn = 0; }
		row[4] = g_strdup(""); row[5] = g_strdup(warn ? "warn" : ""); if (!strcmp(s, "next")) pend = 1; g_ptr_array_add(t->rows, row);
		int k = 0; if (o) { GtkWidget *a = rui_button("Try again", 0); g_object_set_data_full(G_OBJECT(a), "sx-name", g_strdup(set_jstr(e, "name", "")), g_free); g_object_set_data_full(G_OBJECT(a), "sx-title", g_strdup(title), g_free); g_signal_connect(a, "clicked", G_CALLBACK(sx_retry), NULL); gtk_widget_set_sensitive(a, adm && !BUSY); p_put(a); t->btn[i][k++] = a; }
		GtkWidget *rm = rui_button("Remove", 0); g_object_set_data_full(G_OBJECT(rm), "sx-name", g_strdup(set_jstr(e, "name", "")), g_free); g_object_set_data_full(G_OBJECT(rm), "sx-title", g_strdup(title), g_free); g_signal_connect(rm, "clicked", G_CALLBACK(sx_remove), NULL); gtk_widget_set_sensitive(rm, adm && !BUSY); p_put(rm); t->btn[i][k++] = rm; t->nb[i] = k;
		if (i == 0) p_label_named("sxremove0", rm); }
	p_custom(sx_lay, t, 0, 0);
	const char *L[4] = { "Install…", "Check for extensions", "Start once without extensions", "Restart now" }; GCallback C[4] = { G_CALLBACK(sx_install), G_CALLBACK(sx_check), G_CALLBACK(sx_skip), G_CALLBACK(sx_restart) };
	int rs = pend || g_object_get_data(G_OBJECT(SW->win), "sx-changed"); GtkWidget *b0 = p_buttons(L, rs ? 4 : 3, C, NULL); p_label_named("sxi", b0);
	{ GtkWidget *p = gtk_widget_get_parent(b0); GList *ch = gtk_container_get_children(GTK_CONTAINER(p)); int idx = 0; for (GList *l = ch; l; l = l->next) { GtkWidget *w = l->data; if (!GTK_IS_BUTTON(w)) continue;
		const char *lb = gtk_button_get_label(GTK_BUTTON(w)); if (lb && (!strcmp(lb, rui_tr(L[0])) || !strcmp(lb, rui_tr(L[2])))) gtk_widget_set_sensitive(w, adm && !off && !BUSY); else if (lb && !strcmp(lb, rui_tr(L[1]))) { gtk_widget_set_sensitive(w, !off && !BUSY); p_label_named("sxc", w); } (void)idx; } g_list_free(ch); }
	p_msg_m(SXSAID ? SXSAID : "", 0, 10, 13);
}

/* ================================================================== Install, Recovery (settings25.js updPanel) */
typedef struct { char name[64], label[300]; int keep; GtkWidget *span, *b1, *b2; double bytes, ty, sw; char model[128]; } Disk;
static double disk_lay(void *d, double x, double y, double W)
{
	Disk *k = d; double c1 = 110, c2 = 90, gap = 8, sw = W - c1 - c2 - 2 * gap, lh = s_lh(RUI.fs, 400); double th = s_lines(k->label, RUI.fs, 400, sw, NULL) * lh, bh = p_nat_h(k->b2); double h = fmax(th, bh) + 8 + 1;
	k->ty = 4 + (h - 9 - th) / 2; k->sw = sw; p_place(k->span, x, y, W, h);
	if (k->b1) { p_place(k->b1, x + sw + gap, y + 4 + (h - 9 - bh) / 2, c1, 0); p_place(k->b2, x + sw + gap + c1 + gap, y + 4 + (h - 9 - bh) / 2, c2, 0); }
	else p_place(k->b2, x + sw + gap, y + 4 + (h - 9 - bh) / 2, c1, 0);
	return h;
}
static gboolean disk_draw(GtkWidget *w, cairo_t *cr, gpointer d) { Disk *k = d; double W = gtk_widget_get_allocated_width(w) / s_z(), H = gtk_widget_get_allocated_height(w) / s_z(); s_col(cr, "fg", 1); s_text(cr, k->label, 0, k->ty, RUI.fs, 400, k->sw, 0);
	cairo_save(cr); cairo_scale(cr, s_z(), s_z()); s_col(cr, "acr", 1); cairo_rectangle(cr, 0, H - 1, W, 1); cairo_fill(cr); cairo_restore(cr); return TRUE; }   /* (.acr: its line under the row) */
/* (the row's line under it: .acr's border-bottom, drawn by the page's own block under the row) */
static GtkWidget *JLI; static GString *ILOG;
static gboolean inst_tick(gpointer d)
{
	Disk *k = d; RuiJ *j = rui_get("/api/job"); if (!j) return G_SOURCE_CONTINUE; if (!ILOG) ILOG = g_string_new(NULL); g_string_assign(ILOG, set_jstr(j, "log", "")); if (JLI) p_text_set(JLI, ILOG->str);
	int run = rui_jbool(j, "running"), st = (int)rui_jnum(j, "status", -1); rui_jfree(j); if (run || st < 0) return G_SOURCE_CONTINUE;
	set_notify(st == 0 ? "Rudolf OS is installed" : "Installation failed", st == 0 ? "Remove the disc or USB drive, then restart." : "See the log in Settings.", st != 0);
	if (st == 0) { char *t = g_strdup_printf("Remove the disc or USB drive, then restart the computer to start Rudolf OS from %s.", k->name); const char *B[2] = { "Later", "Restart now" }; dlg("Rudolf OS is installed", t, B, 2, reboot_do, NULL); g_free(t); }
	return G_SOURCE_REMOVE;
}
static Disk *IDISK; static int BOOTONLY;
static void inst_go(int i, void *d)
{
	(void)d; Disk *k = IDISK; if (i != 1 || !k) return; char p[300]; snprintf(p, sizeof p, "/api/install?disk=%s&confirm=%s%s", k->name, k->name, BOOTONLY ? "&bootonly=1" : ""); char m[300]; int c = post(p, NULL, m, sizeof m);
	if (c != 200) { set_notify(BOOTONLY ? "The boot files were not replaced" : "Installation failed", m, 1); return; }
	if (!ILOG) ILOG = g_string_new(NULL); g_string_assign(ILOG, " "); p_relayout(); g_timeout_add(1000, inst_tick, k);
}
static void inst_elev(int ok, void *d)
{
	(void)d; Disk *k = IDISK; if (ok != 1 || !k) return; char *bb = fmtb(k->bytes); const char *B[2] = { "Cancel", BOOTONLY ? "Replace the boot files" : "Erase and install" };
	/* (bring62.js's bringStep, an offer to bring in the old computer's files first: Part 39, PR 2b) */
	if (BOOTONLY) { char *t = g_strdup_printf("The area in front of %s's files is written with this Rudolf OS. Its files, settings and installed system are kept. Do not unplug it or switch the computer off while it is written.", k->name), *tt = g_strdup_printf("Replace the boot files on %s?", k->name); dlg(tt, t, B, 2, inst_go, NULL); g_free(t); g_free(tt); }
	else { char *t = g_strdup_printf("All partitions and files on %s (%s) will be destroyed and replaced by Rudolf OS.", k->name, bb), *tt = g_strdup_printf("Erase %s?", k->name); dlg(tt, t, B, 2, inst_go, NULL); g_free(t); g_free(tt); }
	g_free(bb);
}
static void on_install(GtkButton *b, gpointer d) { (void)b; Disk *k = d; IDISK = k; BOOTONLY = 0; char *bb = fmtb(k->bytes), *r = g_strdup_printf("Install Rudolf OS on %s (%s, %s). EVERYTHING on this disk will be erased.", k->name, k->model, bb); rui_elevate(SW, r, inst_elev, NULL); set_note("ask install %s", k->name); g_free(r); g_free(bb); }
static void on_bootonly(GtkButton *b, gpointer d) { (void)b; Disk *k = d; IDISK = k; BOOTONLY = 1; char *r = g_strdup_printf("Replace the boot files on %s (%s). Its files are kept.", k->name, k->model); rui_elevate(SW, r, inst_elev, NULL); set_note("ask bootonly %s", k->name); g_free(r); }
static void fr_go(int i, void *d) { (void)d; if (i != 1) return; char m[300]; int c = post("/api/factory-reset", NULL, m, sizeof m); set_notify(c == 200 ? "Factory reset scheduled" : "Factory reset failed", m, c != 200); }
static void fr_elev(int ok, void *d) { (void)d; if (ok != 1) return; const char *B[2] = { "Cancel", "Erase at restart" }; dlg("Factory reset", "All accounts, files, settings and updates on this computer will be erased at the next start.", B, 2, fr_go, NULL); }
static void on_factory(GtkButton *b, gpointer d) { (void)b; (void)d; rui_elevate(SW, "Erase all saved data on this computer (factory reset)", fr_elev, NULL); set_note("ask factory reset"); }
static void sec_install(void)
{
	/* (the line about how this system started: its status, read from info) */
	const char *media = set_jstr(INFO, "media", ""); char *bt = upper(set_jstr(INFO, "boot", ""));
	char *ui = rui_jbool(INFO, "live") ? g_strdup_printf("Running live from %s · started with %s%s%s", !strcmp(media, "cd") ? "a disc" : !strcmp(media, "usb") ? "a USB drive" : !strcmp(media, "sd") ? "an SD card" : "a USB drive or disk", bt, rui_jbool(INFO, "recovery") ? " · RECOVERY MODE" : "", rui_jbool(INFO, "updateInstalled") ? " · an update is installed" : "")
		: g_strdup_printf("Installed on this computer · started with %s%s%s", bt, rui_jbool(INFO, "recovery") ? " · RECOVERY MODE" : "", rui_jbool(INFO, "updateInstalled") ? " · an update is installed" : "");
	p_msg_m(INFO ? ui : "Loading…", 0, 10, 0); g_free(ui); g_free(bt);
	p_h4("Install Rudolf OS on this computer");
	p_msg_m("\x03" "Copies Rudolf OS to an internal disk and adds a RUDOLFDATA partition for your files. <b>Everything on the chosen disk is erased.</b>", 0, 10, 13);
	RuiJ *dl = rui_jprop(DISKS, "disks"); int any = 0;
	for (int i = 0; i < rui_jlen(dl); i++) { RuiJ *k = rui_jat(dl, i); if (rui_jnum(k, "bytes", 0) <= 4e9) continue; any = 1; RuiJ *pa = rui_jprop(k, "partitions"); int own = 0, keep = 0, mounted = 0;
		for (int p = 0; p < rui_jlen(pa); p++) { RuiJ *x = rui_jat(pa, p); const char *fs = set_jstr(x, "fs", ""), *lb = set_jstr(x, "label", ""), *mt = set_jstr(x, "mount", ""); if (!strcmp(fs, "iso9660") || (!strcmp(lb, "RUDOLFDATA") && rui_jprop(INFO, "live") && !rui_jbool(INFO, "live"))) own = 1;
			if (!strcmp(lb, "RUDOLFDATA")) keep = 1; if (!strcmp(mt, "/live/media") || !strcmp(mt, "/persist")) mounted = 1; }
		keep = keep && !mounted;
		Disk *d = g_new0(Disk, 1); snprintf(d->name, sizeof d->name, "%s", set_jstr(k, "name", "")); snprintf(d->model, sizeof d->model, "%s", set_jstr(k, "model", "")); d->bytes = rui_jnum(k, "bytes", 0); char *bb = fmtb(d->bytes);
		snprintf(d->label, sizeof d->label, "%s %s — %s — %s — %d partition(s)%s", rui_jbool(k, "removable") ? "🔌" : "💽", d->name, *d->model ? d->model : "disk", bb, rui_jlen(pa), own ? " (Rudolf OS)" : ""); g_free(bb);
		d->span = gtk_drawing_area_new(); g_signal_connect(d->span, "draw", G_CALLBACK(disk_draw), d); g_object_set_data_full(G_OBJECT(d->span), "disk", d, g_free); p_put(d->span);
		if (keep) { d->b1 = rui_button("Replace the boot files only…", 0); g_signal_connect(d->b1, "clicked", G_CALLBACK(on_bootonly), d); p_put(d->b1); }
		d->b2 = rui_button("Install here…", 0); g_signal_connect(d->b2, "clicked", G_CALLBACK(on_install), d); p_put(d->b2); if (!any || i == 0) p_label_named("install0", d->b2);
		p_custom(disk_lay, d, 0, 0); }
	if (!any) p_msg_m(DISKS ? "No suitable internal disk (at least 4 GB) was found." : "", 0, 0, 0);
	JLI = NULL; if (ILOG && ILOG->len) { JLI = p_msg_m(ILOG->str, 0, 0, 0); }
	p_h4("Recovery");
	p_msg_m("\x03" "Choose <i>Recovery mode</i> in the boot menu to start the installed system with safe settings (the Chromium desktop engine, no shell, safe graphics); updates wait until a normal start. Recovery mode needs boot files from 1.5.7 or later (Update and recovery › boot files): older ones start the drive's own system, which most installed computers do not have. <i>Check this disc</i> verifies every file.", 0, 10, 13);
	RuiJ *ck = rui_get("/api/check"); const char *res = rui_jstr(ck, "result"); char *t = res && *res ? g_strdup_printf("Last media check: %s", res) : g_strdup(""); p_msg_m(t, 0, 10, 0); g_free(t); rui_jfree(ck);
	const char *L[1] = { "Factory reset…" }; GCallback C[1] = { G_CALLBACK(on_factory) }; p_label_named("factory", p_buttons(L, 1, C, NULL));
}
/* ================================================================== Bring in files from the old computer (bring62.js brSection) */
static RuiJ *BRF; static int BRLOOKED;
static void br_look(GtkButton *b, gpointer d) { (void)b; (void)d; rui_jfree(BRF); BRF = rui_get("/api/bring/found"); BRLOOKED = 1; set_note("bring look %d", BRF != NULL); redraw(); }
static void br_in(GtkButton *b, gpointer d) { (void)b; (void)d; set_notify("Bring in files from the old computer", "Not in the native Settings yet (docs/plan-1.6.2.md Part 39): open Settings in Chromium mode to bring them in.", 1); }
static void sec_bring(void)
{
	p_h4("Bring in files from the old computer");
	p_msg_m("The files the installer kept on a USB drive (Rudolf OS - Bring your files) go into your folders: the old Documents into Documents, Pictures into Pictures and so on. Plug the drive in, then look.", 0, 10, 13);
	if (BRLOOKED) { RuiJ *sets = rui_jprop(BRF, "sets");
		if (!BRF) p_msg_m("rudolfd did not answer.", 0, 10, 13);
		else if (!rui_jlen(sets)) p_msg_m("No copy of an old computer's files on the drives plugged in.", 0, 10, 13);
		for (int i = 0; i < rui_jlen(sets); i++) { RuiJ *s = rui_jat(sets, i); const char *sy = set_jstr(s, "system", ""); char made[16]; snprintf(made, sizeof made, "%.10s", set_jstr(s, "made", ""));
			char *t = g_strdup_printf("%s · copied %s", !strcmp(sy, "windows") ? "Windows" : !strcmp(sy, "linux") ? "Linux" : sy, made); p_msg_m(t, 0, 10, 0); g_free(t);
			RuiJ *pp = rui_jprop(s, "people"); for (int k = 0; k < rui_jlen(pp); k++) { RuiJ *p = rui_jat(pp, k); char *b = fmtb(rui_jnum(p, "bytes", 0)), *x = g_strdup_printf("%s's files — %s (%.0f files)", set_jstr(p, "name", ""), b, rui_jnum(p, "files", 0)); p_msg_m(x, 0, 4, 0); g_free(b); g_free(x); }
			const char *L[1] = { "Bring them in" }; GCallback C[1] = { G_CALLBACK(br_in) }; p_buttons(L, 1, C, NULL); } }
	const char *L[1] = { "Look on the drives" }; GCallback C[1] = { G_CALLBACK(br_look) }; p_label_named("brlook", p_buttons(L, 1, C, NULL));
}
/* ================================================================== Start-up of this drive (startup60.js) */
static void sup_ck_cred(const char *n, const char *p, void *d)
{
	(void)d; if (!n) return; char *b = cred_body(n, p), *out = NULL; int c = rui_http("POST", "/api/sys/startup-check", b, strlen(b), &out, NULL); explicit_bzero(b, strlen(b)); g_free(b);
	RuiJ *j = out ? rui_jparse(out) : NULL; const char *rep = set_jstr(j, c == 200 ? "report" : "error", ""); if (!JLOG) JLOG = g_string_new(NULL); g_string_assign(JLOG, rep);
	{ GRegex *r = g_regex_new("\\nexit=\\d+\\s*$", 0, 0, NULL); char *x = g_regex_replace_literal(r, JLOG->str, -1, 0, "", 0, NULL); g_string_assign(JLOG, x); g_free(x); g_regex_unref(r); }
	int good = c == 200 && g_regex_match_simple("exit=0\\s*$", rep, 0, 0); set_note("startup-check %d %d", c, good);
	if (c == 200) set_notify("Start-up of this drive", good ? "All is right: the computer can start from this drive." : "Not all is right: see Settings › Update and recovery.", !good); else set_notify("Start-up of this drive", rep, 1);
	rui_jfree(j); g_free(out); redraw();
}
static void sup_ck(GtkButton *b, gpointer d) { (void)b; (void)d; rui_elevate_cred(SW, "Check this drive's start-up and put the firmware's entry for it right", sup_ck_cred, NULL); set_note("ask startup-check"); }
static void sup_fx_done(int st, void *d) { (void)d; set_notify("Start-up of this drive", st == 0 ? "The official start-up files are back, and the firmware's entry for the drive is right." : "Nothing was changed, or the old files were put back: see Settings › Update and recovery.", st != 0); g_idle_add(redraw_idle, NULL); }
static void sup_fx_cred(const char *n, const char *p, void *d) { (void)d; if (!n) return; char *b = cred_body(n, p), m[300]; int c = post("/api/sys/startup-files", b, m, sizeof m); explicit_bzero(b, strlen(b)); g_free(b); if (c != 200) { set_notify("Start-up of this drive", m, 1); return; } job_follow(sup_fx_done, NULL); redraw(); }
static void sup_fx_ok(int i, void *d) { (void)d; if (i == 1) rui_elevate_cred(SW, "Put back the official start-up files on this drive", sup_fx_cred, NULL); }
static void sup_fx(GtkButton *b, gpointer d)
{
	(void)b; (void)d; const char *B[2] = { "Cancel", "Put them back" };
	dlg("Put back the official start-up files", "Rudolf OS downloads this release's official start-up files, checks their signatures, and puts them into this drive's EFI system partition one file at a time: each new file is written beside the old one and checked before it replaces it. The old ones are kept until the computer has started with the new ones. Your files, settings and apps are not touched, nor is the drive's partition table.\nKeep the computer connected to power (or charged above 50 %) and do not switch it off until it says it is finished. If anything does not check, the old files are put back.", B, 2, sup_fx_ok, NULL);
}
static void sec_startup(void)
{
	p_h4("Start-up of this drive");
	p_msg_m("The files the computer starts Rudolf OS with (in the drive's EFI system partition) and the firmware's entry for the drive. Rudolf OS does not change them by itself.", 0, 10, 13);
	const char *L[2] = { "Check this drive's start-up…", "Put back the official start-up files…" }; GCallback C[2] = { G_CALLBACK(sup_ck), G_CALLBACK(sup_fx) }; GtkWidget *b = p_buttons(L, 2, C, NULL); p_label_named("supck", b);
	{ GList *ch = gtk_container_get_children(GTK_CONTAINER(gtk_widget_get_parent(b))); for (GList *l = ch; l; l = l->next) if (GTK_IS_BUTTON(l->data)) { const char *lb = gtk_button_get_label(l->data); if (lb && (!strcmp(lb, rui_tr(L[0])) || !strcmp(lb, rui_tr(L[1])))) gtk_widget_set_sensitive(l->data, ADM); } g_list_free(ch); }
	if (!ADM) p_msg("Only an administrator can do this.", 0);
}
/* ================================================================== Boot menu (boot30.js) */
static GtkWidget *BMD, *BMT, *BMM; static char *BMMT;
static gboolean bmm_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)w; (void)d; if (BMMT) { s_col(cr, "msg", 1); s_text(cr, BMMT, 0, 0, 13, 400, 0, 0); } return TRUE; }
static double save_lay(void *d, double x, double y, double w) { GtkWidget **pr = d; double bw = p_nat_w(pr[0]); p_place(pr[0], x, y, 0, 28); p_place(pr[1], x + bw + 8, y + 10, w - bw - 8, 18); return 28; }
static const char *boot_title(const char *id)
{
	static const char *const T[][2] = { { "rudolf-start.conf", "Start Rudolf OS" }, { "rudolf-recovery.conf", "Recovery mode" }, { "rudolf-check.conf", "Check this disc or USB drive" }, { "rudolf-ram.conf", "Start from memory" }, { "rudolf-debug.conf", "Debug start" }, { "auto-windows", "Windows Boot Manager" } };
	for (guint i = 0; i < G_N_ELEMENTS(T); i++) if (!strcmp(T[i][0], id)) return T[i][1];
	static char b[128]; snprintf(b, sizeof b, "%s", id); char *c = strstr(b, ".conf"); if (c && !c[5]) *c = 0; return b;
}
static void bm_save_go(int ok, void *d)
{
	(void)d; if (ok != 1) return; char *a = g_uri_escape_string(s_select_get(BMD), NULL, TRUE), *b = g_uri_escape_string(s_select_get(BMT), NULL, TRUE), p[300], m[300]; snprintf(p, sizeof p, "/api/bootmenu/set?default=%s&timeout=%s", a, b); g_free(a); g_free(b);
	int c = post(p, NULL, m, sizeof m); g_free(BMMT); BMMT = c == 200 ? g_strdup(" Saved. It applies from the next start.") : g_strdup_printf(" %s", m); if (BMM) gtk_widget_queue_draw(BMM);
}
static void bm_save(GtkButton *b, gpointer d) { (void)b; (void)d; rui_elevate(SW, "Change the boot menu: what starts by default and how long it waits.", bm_save_go, NULL); set_note("ask bootmenu"); }
static gboolean reboot_late(gpointer d) { (void)d; post("/api/power?action=reboot", NULL, NULL, 0); return G_SOURCE_REMOVE; }
static void win_go(int ok, void *d)
{
	RuiJ *w = d; if (ok != 1) return; char p[80], m[300]; snprintf(p, sizeof p, "/api/bootmenu/windows?n=%s", set_jstr(w, "n", "")); if (post(p, NULL, m, sizeof m) != 200) { set_notify("Restart into Windows", m, 1); return; }
	char *t = g_strdup_printf("The next start goes to %s.", set_jstr(w, "title", "Windows")); set_notify("Restarting into Windows", t, 0); g_free(t); g_timeout_add(1200, reboot_late, NULL);
}
static void on_win(GtkButton *b, gpointer d) { (void)b; RuiJ *w = d; char *r = g_strdup_printf("Restart into Windows: the computer starts %s once, then this menu again as before.", set_jstr(w, "title", "Windows")); rui_elevate(SW, r, win_go, w); g_free(r); }
static void sec_boot(void)
{
	if (!BOOT) return; p_h4("Boot menu");
	if (!rui_jbool(BOOT, "uefi")) { p_msg_m("\x03" "This computer started in BIOS (legacy) mode. To start Windows, choose <b>Boot from hard disk</b> in the Rudolf Boot menu.", 0, 10, 13); return; }
	if (!rui_jbool(BOOT, "loader")) { p_msg_m("Rudolf OS was not started from its own boot menu, so there is nothing to set here.", 0, 10, 13); return; }
	RuiJ *en = rui_jprop(BOOT, "entries"); const char *def = set_jstr(BOOT, "default", ""), *to = set_jstr(BOOT, "timeout", ""); const char *v[24], *l[24]; char lab[24][160]; int n = 0, has = 0;
	v[n] = ""; snprintf(lab[n], sizeof lab[n], "As on the disc (%s)", boot_title("rudolf-start.conf")); l[n] = lab[n]; n++;
	for (int i = 0; i < rui_jlen(en) && n < 24; i++) { const char *e = rui_jsval(rui_jat(en, i)); if (!e) continue; int ok = (g_str_has_prefix(e, "rudolf-") && g_str_has_suffix(e, ".conf") && strcmp(e, "rudolf-debug.conf") && strcmp(e, "rudolf-start.conf")) || !strcmp(e, "auto-windows");
		if (!ok) continue; v[n] = e; snprintf(lab[n], sizeof lab[n], "%s", boot_title(e)); l[n] = lab[n]; if (!strcmp(e, def)) has = 1; n++; }
	{ Cell *c = p_r2("Starts by default"); BMD = s_select(v, l, n, has ? def : "", NULL, NULL); gtk_widget_set_sensitive(BMD, ADM); c_add(c, BMD, 0, 0, 1); p_label_named("bmd", BMD); }
	{ Cell *c = p_r2("Wait before starting"); const char *tv[8] = { "", "2", "3", "4", "5", "6", "8", "10" }, *tl[8] = { "As on the disc (4 seconds)", "2 seconds", "3 seconds", "4 seconds", "5 seconds", "6 seconds", "8 seconds", "10 seconds" }; int ok = 0; for (int i = 1; i < 8; i++) if (!strcmp(to, tv[i])) ok = 1;
	  BMT = s_select(tv, tl, 8, ok ? to : "", NULL, NULL); gtk_widget_set_sensitive(BMT, ADM); c_add(c, BMT, 0, 0, 1); p_label_named("bmt", BMT); }
	{ /* (.rmrow: Save and the .msg beside it, whose 10px and 18px make the row 28 tall: the button stretched to it) */
	  GtkWidget *b = rui_button("Save", 0); g_signal_connect(b, "clicked", G_CALLBACK(bm_save), NULL); gtk_widget_set_sensitive(b, ADM); p_put(b); p_label_named("bms", b);
	  BMM = gtk_drawing_area_new(); g_signal_connect(BMM, "draw", G_CALLBACK(bmm_draw), NULL); p_put(BMM); GtkWidget **pr = g_new0(GtkWidget *, 2); pr[0] = b; pr[1] = BMM; p_custom(save_lay, pr, 14, 0); }
	char *t = g_strdup_printf("These choices are kept in this computer's firmware, not on the disc or stick. They apply to every Rudolf OS disc or stick started on this computer. %s%s%s", rui_jstr(BOOT, "selected") ? "This time Rudolf OS started with “" : "", rui_jstr(BOOT, "selected") ? boot_title(rui_jstr(BOOT, "selected")) : "", rui_jstr(BOOT, "selected") ? "”." : "");
	p_msg_m(t, 0, 10, 13); g_free(t);
	RuiJ *wl = rui_jprop(BOOT, "windows");
	if (rui_jlen(wl)) { p_h4("Windows"); p_msg_m("Windows is on this computer. Restart into it once; the next start after that comes back to this menu.", 0, 10, 13);
		const char *L[8]; GCallback C[8]; char lb[8][120]; int k = 0; for (int i = 0; i < rui_jlen(wl) && i < 8; i++) { RuiJ *w = rui_jat(wl, i); if (rui_jlen(wl) > 1) snprintf(lb[k], sizeof lb[k], "Restart into %s (%s)", set_jstr(w, "title", ""), set_jstr(w, "n", "")); else snprintf(lb[k], sizeof lb[k], "Restart into Windows"); L[k] = lb[k]; C[k] = G_CALLBACK(on_win); k++; }
		GtkWidget *b0 = p_buttons(L, k, C, NULL); GList *ch = gtk_container_get_children(GTK_CONTAINER(gtk_widget_get_parent(b0))); int i = 0; for (GList *x = ch; x; x = x->next) if (GTK_IS_BUTTON(x->data)) { const char *bl = gtk_button_get_label(x->data); for (int q = 0; q < k; q++) if (bl && !strcmp(bl, rui_tr(L[q]))) { g_signal_handlers_disconnect_by_func(x->data, G_CALLBACK(on_win), NULL); g_signal_connect(x->data, "clicked", G_CALLBACK(on_win), rui_jat(wl, q)); gtk_widget_set_sensitive(x->data, ADM); } i++; } g_list_free(ch); }
	else p_msg_m("No Windows was found in this computer's start-up list.", 0, 10, 13);
	if (!ADM) p_msg_m("An administrator can change this.", 0, 10, 13);
}

void page_upd(void)
{
	ADM = set_admin();
	if (!BUSY) { rui_jfree(ST); ST = rui_get("/api/sys/status"); if (!SX || !g_object_get_data(G_OBJECT(SW->win), "sx-keep")) { rui_jfree(SX); SX = rui_get("/api/sysext/status"); } }
	rui_jfree(INFO); INFO = rui_get("/api/info"); rui_jfree(DISKS); DISKS = rui_get("/api/disks"); rui_jfree(BOOT); BOOT = rui_get("/api/bootmenu"); rui_jfree(IMAGES); IMAGES = NULL;
	p_h3("Update and recovery");
	{ Ver *v = g_new0(Ver, 1); GtkWidget *a = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(a), "ver", v, g_free); gtk_widget_add_events(a, GDK_BUTTON_PRESS_MASK); g_signal_connect(a, "draw", G_CALLBACK(ver_draw), v); g_signal_connect(a, "button-press-event", G_CALLBACK(ver_click), v); p_put(a); p_custom(ver_lay, a, 13, 13); p_label_named("wnl", a); }
	sec_updates(); sec_sysext(); sec_install(); sec_bring(); sec_startup(); sec_boot();
}
