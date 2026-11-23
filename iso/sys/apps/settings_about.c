/*
 * rudolf-settings: About (settings25.js aboutPanel, devPageUpdate, devShutdownLog; devmode59.js's additions). 1.6.2, docs/plan-1.6.2.md
 * Part 39. The same rows from the same rudolfd requests (info, sys/status), Licences and Help (the shell opens Help, at its Licences),
 * Copy details, and For developers: the desktop page from a file (the administrator's permission, rudolfd's update), its roll-back
 * (rollback, then restart-browser), Restart the desktop, and the last shutdowns' steps (shutdown/log). GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings.h"

static GString *ROWS;   /* (Copy details: "name: value" lines, as the page's rows) */
static GtkWidget *TAB;
static void add(const char *a, const char *b) { s_table2_add(TAB, rui_tr(a), b); g_string_append_printf(ROWS, "%s%s: %s", ROWS->len ? "\n" : "", a, b); }
/* the page's fmtB */
static char *fmtb(double b)
{
	if (b < 0) return g_strdup(""); if (b < 1024) return g_strdup_printf("%.0f B", b);
	if (b < 1048576) return g_strdup_printf("%.1f KB", b / 1024); if (b < 1073741824) return g_strdup_printf("%.2f MB", b / 1048576); return g_strdup_printf("%.1f GB", b / 1073741824);
}
/* the graphics as the system's GL names its renderer (the page asks its WebGL: its engine's name for it; RUDOLF_SET_GPU for the tests) */
static char *gpu_name(void)
{
	const char *t = getenv("RUDOLF_SET_GPU"); if (t && *t) return g_strdup(t);
	/* (on a window of its own, never shown, gone after: the Settings window keeps drawing as before) */
	static char *known; if (known) return g_strdup(known);
	GdkWindowAttr at = { 0 }; at.window_type = GDK_WINDOW_TOPLEVEL; at.wclass = GDK_INPUT_OUTPUT; at.width = 1; at.height = 1; at.override_redirect = TRUE;
	char *out = NULL; GdkWindow *w = gdk_window_new(NULL, &at, GDK_WA_NOREDIR); GError *e = NULL; GdkGLContext *c = w ? gdk_window_create_gl_context(w, &e) : NULL;
	if (c && gdk_gl_context_realize(c, &e)) { gdk_gl_context_make_current(c);
		/* (GTK's own GL loader, libepoxy, already in the program: its glGetString) */
		typedef const unsigned char *(*GS)(unsigned int); GS *gs = dlsym(RTLD_DEFAULT, "epoxy_glGetString");
		if (gs && *gs) { const unsigned char *r = (*gs)(0x1F01); if (r && *r) out = g_strdup((const char *)r); }
		gdk_gl_context_clear_current(); }
	g_clear_error(&e); int had = c != NULL; if (c) g_object_unref(c); if (w) gdk_window_destroy(w); known = out ? out : g_strdup(had ? "GL available" : "none"); return g_strdup(known);
}
static void rows(void)
{
	RuiJ *i = rui_get("/api/info");
	if (i) { const char *v = set_jstr(i, "vendor", ""), *p = set_jstr(i, "product", ""); char *cp = g_strstrip(g_strdup_printf("%s %s", v, p)); add("Computer", *cp ? cp : "—"); g_free(cp);
		char *pr = g_strdup_printf("%s (%g threads)", set_jstr(i, "cpu", "undefined"), rui_jnum(i, "threads", NAN)); add("Processor", pr); g_free(pr);
		char *m = fmtb(rui_jnum(i, "memTotal", NAN)); add("Memory", m); g_free(m); add("Linux kernel", set_jstr(i, "kernel", ""));
		const char *b = rui_jstr(i, "boot"); if (b) { char *u = g_ascii_strup(b, -1), *s = g_strdup_printf("%s%s", u, rui_jbool(i, "live") ? " · live" : " · installed"); add("Start-up", s); g_free(u); g_free(s); } }
	rui_jfree(i);
	/* (1.5.9, plan 15.6: the release's Chromium and its date) */
	RuiJ *u = rui_get("/api/sys/status"), *cu = rui_jprop(u, "current"); const char *ch = rui_jstr(cu, "chromium"), *rel = rui_jstr(cu, "released");
	if (ch) add("Chromium", ch);
	if (rel) { int y, mo, d; if (sscanf(rel, "%d-%d-%d", &y, &mo, &d) == 3) { GDateTime *t = g_date_time_new_utc(y, mo, d, 12, 0, 0); char *s = set_date_str(g_date_time_to_unix(t)); add("Released", s); g_free(s); g_date_time_unref(t); } else add("Released", rel); }
	rui_jfree(u);
	char *g = gpu_name(); add("Graphics", g); g_free(g);
	{ GdkDisplay *dp = gdk_display_get_default(); GdkMonitor *mo = gdk_display_get_primary_monitor(dp); if (!mo) mo = gdk_display_get_monitor(dp, 0); GdkRectangle r = { 0, 0, 0, 0 }; int sf = 1;
	  if (mo) { gdk_monitor_get_geometry(mo, &r); sf = gdk_monitor_get_scale_factor(mo); } char *s = g_strdup_printf("%d×%d @ %dx", r.width * sf, r.height * sf, sf); add("Screen", s); g_free(s); }
	/* (the page's browser storage has no counterpart for a program: the disk the account's files are on, as Downloads tells it) */
	{ RuiJ *sp = rui_get("/api/dl/space"); double tot = rui_jnum(sp, "total", 0), fr = rui_jnum(sp, "free", 0); char *a = fmtb(tot > 0 ? tot - fr : 0), *b = tot > 0 ? fmtb(tot) : NULL;
	  char *s = b ? g_strdup_printf("%s of %s", a, b) : g_strdup(a); add("Storage used", s); g_free(s); g_free(a); g_free(b); rui_jfree(sp); }
	{ char *s = ME[0] ? g_strdup_printf("%s (%s)", ME, !strcmp(ROLE, "admin") ? "administrator" : !strcmp(ROLE, "guest") ? "guest" : "standard user") : g_strdup("—"); add("Signed in as", s); g_free(s); }
	{ extern const char *set_lang(void); add("Language", set_lang()); }
}
static void lic(GtkButton *b, gpointer d) { (void)b; (void)d; rui_shell(SW, "open help licences"); set_note("open help licences"); }
static void help(GtkButton *b, gpointer d) { (void)b; (void)d; rui_shell(SW, "open help"); set_note("open help"); }
static void copy(GtkButton *b, gpointer d)
{
	(void)b; (void)d; char *t = g_strdup_printf("Rudolf OS %s\n%s", rui_version(), ROWS->str); gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), t, -1); gtk_clipboard_store(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
	const char *f = getenv("RUDOLF_SET_STATE"); if (f) { char *x = g_strdup_printf("%s.copy", f); g_file_set_contents(x, t, -1, NULL); g_free(x); }
	g_free(t); set_notify("Copied", "", 0);
}
/* ---- For developers: the desktop page from a file */
static char *PICKED;
static void upd_go(int ok, void *d)
{
	(void)d; if (ok != 1) { set_note("update refused (administrator)"); return; }
	gchar *buf = NULL; gsize n = 0; if (!g_file_get_contents(PICKED, &buf, &n, NULL)) { set_notify("Update failed", "The file could not be read.", 1); return; }
	char *out = NULL; int c = rui_http("POST", "/api/update", buf, n, &out, NULL); g_free(buf);
	RuiJ *j = out ? rui_jparse(out) : NULL; const char *m = rui_jstr(j, c == 200 ? "message" : "error");
	if (c == 200) set_notify("Update installed", m ? m : "", 0); else set_notify("Update failed", m ? m : "rudolfd did not answer", 1);
	set_note("update %d", c); rui_jfree(j); g_free(out);
}
static void upd_file(GtkButton *b, gpointer d)
{
	(void)b; (void)d;
	GtkWidget *fc = gtk_file_chooser_dialog_new(rui_tr("Choose the new Rudolf OS page (.HTML)"), GTK_WINDOW(SW->win), GTK_FILE_CHOOSER_ACTION_OPEN, rui_tr("Cancel"), GTK_RESPONSE_CANCEL, rui_tr("Open"), GTK_RESPONSE_ACCEPT, NULL);
	GtkFileFilter *f = gtk_file_filter_new(); gtk_file_filter_add_pattern(f, "*.html"); gtk_file_filter_add_pattern(f, "*.HTML"); gtk_file_filter_add_pattern(f, "*.htm"); gtk_file_filter_add_pattern(f, "*.HTM"); gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), f);
	const char *pre = getenv("RUDOLF_SET_PICK");   /* (the tests: the file chosen) */
	char *p = NULL; if (pre && *pre) p = g_strdup(pre); else if (gtk_dialog_run(GTK_DIALOG(fc)) == GTK_RESPONSE_ACCEPT) p = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));
	gtk_widget_destroy(fc); if (!p) return; g_free(PICKED); PICKED = p;
	char *why = g_strdup_printf("Install a Rudolf OS update from %s", p); rui_elevate(SW, why, upd_go, NULL); g_free(why); set_note("ask update %s", p);
}
static gboolean restart_late(gpointer d) { (void)d; rui_post("/api/restart-browser", NULL, NULL, 0); set_note("restart-browser"); return G_SOURCE_REMOVE; }
static void rb_go(int ok, void *d)
{
	(void)d; if (ok != 1) { set_note("rollback refused (administrator)"); return; }
	char err[200]; char *out = NULL; int c = rui_http("POST", "/api/rollback", NULL, 0, &out, NULL); RuiJ *j = out ? rui_jparse(out) : NULL; (void)err;
	if (c == 200) { char *m = g_strdup_printf("%s Restarting…", set_jstr(j, "message", "")); set_notify("Rolled back", m, 0); g_free(m); g_timeout_add(1500, restart_late, NULL); }
	else set_notify("Roll back failed", set_jstr(j, "error", "rudolfd did not answer"), 1);
	set_note("rollback %d", c); rui_jfree(j); g_free(out);
}
static void rollback(GtkButton *b, gpointer d) { (void)b; (void)d; rui_elevate(SW, "Roll back Rudolf OS to the previous version", rb_go, NULL); set_note("ask rollback"); }
static void restart(GtkButton *b, gpointer d) { (void)b; (void)d; restart_late(NULL); }
void page_about(void)
{
	if (ROWS) g_string_free(ROWS, TRUE); ROWS = g_string_new(NULL);
	p_h3("About");
	char *t = g_strdup_printf("Rudolf OS %s", rui_version()), *s = g_strdup_printf("%s · GPL-2.0-or-later", rui_version_date());
	/* (devmode59.js: Official or Custom, under the version, from sys/status's signer) */
	char *mark = NULL; int warn = 0; { RuiJ *st = rui_get("/api/sys/status"); const char *sg = rui_jstr(st, "signer");
	  if (sg && !strcmp(sg, "official")) mark = g_strdup_printf("✅ %s", rui_tr("Official Rudolf OS"));
	  else if (sg && !strcmp(sg, "custom")) { const char *k = set_jstr(st, "signerKey", ""); size_t n = strlen(k); char fp[40] = ""; if (n) snprintf(fp, sizeof fp, "%.8s…%.*s", k, n >= 6 ? 5 : 0, n >= 6 ? k + n - 6 : "");
		RuiJ *o = rui_get("/api/origin"); const char *repo = rui_jstr(o, "repo"); int off = o && rui_jprop(o, "official") && !rui_jbool(o, "official");
		mark = g_strdup_printf("⚠️ Custom build (signed by key %s)%s%s", fp, off && repo ? ", from github.com/" : "", off && repo ? repo : ""); warn = 1; rui_jfree(o); }
	  rui_jfree(st); }
	p_block(s_logo_block(t, s, mark, warn), 0, 0); g_free(t); g_free(s); g_free(mark);
	TAB = s_table2(); p_block(TAB, 0, 0); rows();
	{ static const char *const L[3] = { "Licences", "Help", "Copy details" }; GCallback C[3] = { G_CALLBACK(lic), G_CALLBACK(help), G_CALLBACK(copy) }; GtkWidget *b = p_buttons(L, 3, C, NULL); p_label_named("licences", b); }
	p_h4("For developers");
	{ Cell *c = p_r2("Desktop page"); c_msg_m(c, "Install a newer Rudolf OS desktop page (index.html) from a file, for testing. The previous page is kept so you can roll back to it. This is not a system update.", 0, 0, 13);
	  static const char *const L[3] = { "Update the page from a file…", "Roll back to the previous page", "Restart the desktop" }; GCallback C[3] = { G_CALLBACK(upd_file), G_CALLBACK(rollback), G_CALLBACK(restart) };
	  GtkWidget *b = c_buttons(c, L, 3, C, NULL); p_label_named("updfile", b); }
	{ Cell *c = p_r2("Shutting down"); RuiJ *j = rui_get("/api/shutdown/log"); char *lg = j ? g_strstrip(g_strdup(set_jstr(j, "log", ""))) : NULL;
	  c_pre(c, lg && *lg ? lg : j ? "Nothing yet: this list fills in after the next shutdown or restart." : "rudolfd did not answer"); g_free(lg); rui_jfree(j);
	  c_msg(c, "Each shutdown or restart: the desktop's steps and the system's, in milliseconds from the button; \"start\" lines are when Rudolf OS started again.", 0); }
}
