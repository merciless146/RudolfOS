/*
 * rudolf-taskmgr - Task Manager, native (1.6.2, docs/plan-1.6.2.md Part 39 step 2; docs/plan-2.0.md wave 1), with librudolf-ui.
 * The web Task Manager (desktop/tm26.js, tmtop57.js, apps57.js's tmApps57, tools56.js's Developer Console) as a program of its own:
 * every tab (Apps, Processes, Performance, System, Hardware, Startup), column, sort, search box, action and footer button, from the
 * same rudolfd requests (stats, processes, net, system, memory/breakdown, apps/running, proc/end, service/restart, memory/dropcache,
 * apps/end, apps/kill...), drawn in the page's look. Started by rudolf-native for Task Manager on the native desktop (the start menu,
 * the pins, the taskbar's right-click, Ctrl+Shift+Esc and Ctrl+Alt+Del); Chromium and Lite keep their web Task Manager.
 *   rudolf-taskmgr [--urgent]       (--urgent: Ctrl+Alt+Del's, above every window, the desktop's processes first while it is open)
 * Built with librudolf-ui (iso/sys/ui/build.sh WORK OUT taskmgr). Its test: iso/qt/native/taskmgr_t.sh.
 * Tests' hooks: RUDOLF_TM_STATE=FILE (what it shows, for the checklist), RUDOLF_TM_HIST=N (the graphs' history filled with the first
 * reading N times, as the page's after N samples), RUDOLF_TM_TAB=N (the tab it opens on).
 * GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "rudolf-ui.h"

enum { T_APPS, T_PROCS, T_PERF, T_SYS, T_HW, T_START, NT };
static const char *const TABN[NT] = { "Apps", "Processes", "Performance", "System", "Hardware", "Startup" };
static RuiWin *W; static GtkWidget *TABS, *SCR, *BOX, *ST, *FOOT; static int TAB = T_APPS, URGENT, BUSY;
static RuiJ *PROCS, *SYSJ, *APPSRUN, *MEMB, *CAT, *WINSJ, *SBS, *UPS; static double LCPU = -1, LMEM = -1, LMEMT = 0, LTEMP = NAN, LNET = 0; static RuiJ *LBAT;
static double NETB = -1; static gint64 NETT; static double HC[60], HM[60], HN[60]; static int NH;
static long SEL = -1; static char SORTK[8] = "rss"; static int SORTUP; static char FILT[200];
static char CCMSG[200]; static gint64 CCT;   /* (the last Clear cache's result, kept across redraws for 15 s) */
static RuiTable *PT; static GtkWidget *PBAR, *PFIND, *PEND, *PTREE, *PNOW, *PSEL;   /* (the Processes tab's own, kept while it refreshes) */
static int OPEN_DET[8];
static RuiTable *SVT, *STT;   /* (the System tab's services, for the tests' state) */   /* (which <details> are open: the System tab's and the Hardware tab's) */

/* ------------------------------------------------------------------ the page's helpers, in C */
static char *fmtB(double b)   /* (core25.js fmtB) */
{
	static char r[8][32]; static int i; char *o = r[i++ & 7];
	if (b < 0) o[0] = 0; else if (b < 1024) snprintf(o, 32, "%.0f B", b); else if (b < 1048576) snprintf(o, 32, "%.1f KB", b / 1024);
	else if (b < 1073741824) snprintf(o, 32, "%.2f MB", b / 1048576); else snprintf(o, 32, "%.1f GB", b / 1073741824);
	return o;
}
static const char *TMK[][2] = { {"desktop", "Desktop"}, {"web", "Web pages (desktop and Oracle tabs)"}, {"gpu", "Graphics"}, {"network", "Network"}, {"audio", "Sound"},
	{"chromium", "Browser helpers"}, {"webwindow", "Browser window for sign-in sites"}, {"app", "Apps"}, {"system", "System services"}, {"kernel", "Kernel"} };
static void dur(long s, char *o, size_t n) { if (s < 0) s = 0; long d = s / 86400, h = s % 86400 / 3600, m = s % 3600 / 60; char a[24] = "", b[24] = "";
	if (d) snprintf(a, sizeof a, "%ld d ", d); if (d || h) snprintf(b, sizeof b, "%ld h ", h); snprintf(o, n, "%s%s%ld min", a, b, m); }
static GPtrArray *lines(const char *t) { GPtrArray *a = g_ptr_array_new_with_free_func(g_free); if (!t) return a; char **v = g_strsplit(t, "\n", -1);
	for (int i = 0; v[i]; i++) { char *x = g_strstrip(v[i]); if (*x) g_ptr_array_add(a, g_strdup(x)); } g_strfreev(v); return a; }
static const char *js(RuiJ *o, const char *k) { const char *s = rui_jstr(o, k); return s ? s : ""; }
static char *disp(const char *d) { GString *o = g_string_new(NULL); char **v = g_strsplit(d ? d : "", "|", -1);   /* (tmDisp) */
	for (int i = 0; v[i]; i++) { char *x = g_strstrip(v[i]); if (*x) g_string_append_printf(o, "%s%s", o->len ? " · " : "", x); } g_strfreev(v);
	if (!o->len) g_string_assign(o, "not reported"); return g_string_free(o, FALSE); }
static const char *GPUV[][2] = { {"0x8086", "Intel"}, {"0x1002", "AMD"}, {"0x10de", "NVIDIA"}, {"0x1234", "QEMU standard VGA (virtual)"}, {"0x1af4", "virtio (virtual)"}, {"0x1b36", "QEMU (virtual)"},
	{"0x15ad", "VMware (virtual)"}, {"0x80ee", "VirtualBox (virtual)"}, {"0x1414", "Hyper-V (virtual)"} };
static char *gpu(const char *l)   /* (tmGpu: "card0 0x8086:0x46a6 i915") */
{
	char card[64] = "", id[64] = "", drv[64] = ""; sscanf(l, "%63s %63s %63s", card, id, drv); char v[16]; snprintf(v, sizeof v, "%.*s", (int)strcspn(id, ":"), id);
	const char *n = NULL; for (unsigned i = 0; i < G_N_ELEMENTS(GPUV); i++) if (!strcmp(GPUV[i][0], v)) n = GPUV[i][1];
	return g_strdup_printf("%s%s graphics (%s)%s%s", n ? n : "vendor ", n ? "" : v, id, drv[0] ? ", driver " : ", no driver", drv);
}
static int gige(const char *d) { static const char *const G[] = {"r8169", "e1000e", "igb", "igc", "tg3", "alx", "atl1c", "bnx2", "r8125", "atlantic", "ixgbe", "bnxt_en", "cdc_ncm", "r8152", "ax88179_178a", NULL};
	for (int i = 0; G[i]; i++) if (d && !strcmp(d, G[i])) return 1; return 0; }
static int slow(const char *kind, const char *state, const char *sp, const char *drv) { return !strcmp(kind, "wired") && !strcmp(state, "up") && atof(sp) > 0 && atof(sp) < 1000 && gige(drv); }
static const char *drawn(RuiJ *s)   /* (tmDrawn) */
{
	static char o[300]; if (!s) return "—"; const char *r = rui_jstr(s, "renderer"); if (r && *r) { snprintf(o, sizeof o, "the graphics card (%s)", r); return o; }
	return !strncmp(js(s, "display"), "gpu", 3) ? "the processor: Xorg runs without acceleration" : "the processor (software display)";
}
static char *sb_summary(RuiJ *s)   /* (sbSummary) */
{
	if (!s) return g_strdup("not reported"); if (!rui_jbool(s, "efi")) return g_strdup("not available (started in BIOS mode)");
	const char *lk = js(s, "lockdown"), *a = strchr(lk, '['), *b = a ? strchr(a, ']') : NULL; char l[32] = ""; if (a && b) snprintf(l, sizeof l, "%.*s", (int)(b - a - 1), a + 1);
	int on = rui_jbool(s, "on");
	return g_strdup_printf("%s%s%s%s%s%s%s", on ? "on" : "off", on && l[0] && strcmp(l, "none") ? ", kernel locked down (" : "", on && l[0] && strcmp(l, "none") ? l : "", on && l[0] && strcmp(l, "none") ? ")" : "",
		rui_jbool(s, "enrolled") ? ", Rudolf OS key enrolled" : "", rui_jbool(s, "pending") ? ", key waiting to be enrolled" : "", rui_jbool(s, "unverified") ? ", some layers unchecked" : "");
}
static char *rel(RuiJ *up)   /* (tmRel) */
{
	RuiJ *c = rui_jprop(up, "current"); if (!c) return g_strdup(""); GString *o = g_string_new(js(c, "version"));
	if (*js(c, "serial")) g_string_append_printf(o, " · serial %s", js(c, "serial"));
	const char *u = js(up, "url"); size_t n = strlen(u); while (n && u[n - 1] == '/') n--; const char *ch = n >= 6 && !strncmp(u + n - 6, "stable", 6) ? "stable" : n >= 4 && !strncmp(u + n - 4, "beta", 4) ? "beta" : NULL;
	if (ch) g_string_append_printf(o, " · %s channel", ch);
	const char *bt = js(up, "booted"); if (!strcmp(bt, "disc")) g_string_append(o, " · from the disc"); else if (*bt) g_string_append_printf(o, " · slot %s%s", bt, rui_jbool(up, "trying") ? " (trial start)" : "");
	return g_string_free(o, FALSE);
}
/* the app's name and icon (the page's catalogue, desktop/apps, as the native shell reads it) */
static void app_name(const char *k, char *icon, size_t ni, char *name, size_t nn)
{
	if (icon) icon[0] = 0; if (name) name[0] = 0; RuiJ *l = rui_jprop(CAT, "apps");
	for (int i = 0; i < rui_jlen(l); i++) { RuiJ *a = rui_jat(l, i); if (strcmp(js(a, "k"), k)) continue; if (icon) snprintf(icon, ni, "%s", js(a, "ic")[0] ? js(a, "ic") : js(a, "icon")); if (name) snprintf(name, nn, "%s", js(a, "name")); return; }
}

/* ------------------------------------------------------------------ state for the tests (RUDOLF_TM_STATE) */
static GString *ACTS;   /* (what was done, the last 20) */
static void act_note(const char *fmt, ...) { if (!ACTS) ACTS = g_string_new(NULL); va_list a; va_start(a, fmt); char b[300]; vsnprintf(b, sizeof b, fmt, a); va_end(a); g_string_append_printf(ACTS, "act %s\n", b); }
static void rect_line(GString *s, const char *n, GtkWidget *w)
{
	if (!w || !gtk_widget_get_mapped(w)) return; int x, y; GdkWindow *g = gtk_widget_get_window(W->win); int ox, oy; gdk_window_get_origin(g, &ox, &oy);
	gtk_widget_translate_coordinates(w, W->win, 0, 0, &x, &y); g_string_append_printf(s, "rect %s %d %d %d %d\n", n, ox + x, oy + y, gtk_widget_get_allocated_width(w), gtk_widget_get_allocated_height(w));
}
static RuiJ *wins_list(void);
static GHashTable *NAMED, *FIXED;   /* (the tab's parts, the window's own: the footer's buttons) */
static void named(const char *n, GtkWidget *w) { if (!NAMED) NAMED = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL); g_hash_table_replace(NAMED, g_strdup(n), w); }
static gboolean state_write(gpointer d)
{
	(void)d; const char *f = getenv("RUDOLF_TM_STATE"); if (!f || !W) return G_SOURCE_REMOVE;
	GString *s = g_string_new(NULL); g_string_append_printf(s, "tab %d\nsel %ld\nsort %s %d\nfilter %s\nstatus %s\nprocs %d\nbusy %d\n", TAB, SEL, SORTK, SORTUP, FILT, gtk_label_get_text(GTK_LABEL(ST)), rui_jlen(rui_jprop(PROCS, "items")), BUSY);
	if (WINSJ) g_string_append_printf(s, "wins %d\n", rui_jlen(wins_list()));
	rect_line(s, "body", W->body); rect_line(s, "box", BOX); if (PSEL && TAB == T_PROCS) g_string_append_printf(s, "selmsg %s\n", gtk_label_get_text(GTK_LABEL(PSEL)));
	for (int i = 0; i < NT; i++) { char n[16]; snprintf(n, sizeof n, "tab%d", i); rect_line(s, n, rui_tabs_button(TABS, i)); }
	for (GHashTable *h = NAMED; h; h = h == NAMED ? FIXED : NULL) { GHashTableIter it; gpointer k, v; g_hash_table_iter_init(&it, h); while (g_hash_table_iter_next(&it, &k, &v)) rect_line(s, k, v); }
	if (PT && TAB == T_PROCS) { g_string_append(s, "cols"); for (int c = 0; c <= 5; c++) g_string_append_printf(s, " %.0f", rui_table_col_x(PT, c)); g_string_append_c(s, '\n'); }
	if (PT && TAB == T_PROCS) { RuiJ *it = rui_jprop(PROCS, "items"); for (int i = 0; i < rui_jlen(it); i++) { long pid = (long)rui_jnum(rui_jat(it, i), "pid", 0); GdkRectangle r; rui_table_rect(PT, pid, &r);
			if (r.height) { int ox, oy; gdk_window_get_origin(gtk_widget_get_window(W->win), &ox, &oy); g_string_append_printf(s, "row %ld %d %d %d %d\n", pid, ox + r.x, oy + r.y, r.width, r.height); } } }
	if (SVT && TAB == T_SYS) { GdkRectangle r; int ox, oy; gdk_window_get_origin(gtk_widget_get_window(W->win), &ox, &oy); if (rui_table_button_rect(SVT, 1, 2, 0, &r)) g_string_append_printf(s, "rect restart-display %d %d %d %d\n", ox + r.x, oy + r.y, r.width, r.height); }
	if (STT && TAB == T_START) { GdkRectangle r; int ox, oy; gdk_window_get_origin(gtk_widget_get_window(W->win), &ox, &oy); if (rui_table_button_rect(STT, 0, 1, 0, &r)) g_string_append_printf(s, "rect remove0 %d %d %d %d\n", ox + r.x, oy + r.y, r.width, r.height); }
	GtkWidget *m = rui_menu_window(); if (m && gtk_widget_get_mapped(m)) { int x, y; gdk_window_get_origin(gtk_widget_get_window(m), &x, &y); g_string_append_printf(s, "menu %d %d %d %d\n", x, y, gtk_widget_get_allocated_width(m), gtk_widget_get_allocated_height(m)); }
	GtkWidget *dl = rui_dialog_box(); if (dl) rect_line(s, "dialog", dl);
	if (ACTS) g_string_append(s, ACTS->str);
	char *t = g_strdup_printf("%s.t", f); g_file_set_contents(t, s->str, -1, NULL); rename(t, f); g_free(t); g_string_free(s, TRUE); return G_SOURCE_REMOVE;
}
static void state_soon(void) { if (getenv("RUDOLF_TM_STATE")) g_timeout_add(150, state_write, NULL); }

/* ------------------------------------------------------------------ the page's pieces */
static void clear(GtkWidget *b) { GList *c = gtk_container_get_children(GTK_CONTAINER(b)); for (GList *l = c; l; l = l->next) gtk_widget_destroy(l->data); g_list_free(c); }
static void add(GtkWidget *w) { gtk_box_pack_start(GTK_BOX(BOX), w, FALSE, FALSE, 0); }
static GtkWidget *h4(const char *t) { GtkWidget *h = rui_heading(t); add(h); return h; }
static GtkWidget *msg(const char *t, int mt) { GtkWidget *m = rui_msg(t); gtk_widget_set_margin_top(m, (int)lround(rui_px(mt))); add(m); return m; }
/* a table of names and values (the page's kv: .tmt, the name 34 % wide and grey) */
typedef struct { const char *k; char *v; } KV;
static RuiTable *kv(KV *r, int n)
{
	RuiTable *t = rui_table_new(2); rui_table_nohead(t, 1); rui_table_wrap(t, 1); rui_table_col(t, 0, "", RUI_LEFT, .34); rui_table_col(t, 1, "", RUI_LEFT, 0);
	for (int i = 0; i < n; i++) { if (!r[i].k) continue; const char *c[2] = { rui_tr(r[i].k), r[i].v && *r[i].v ? r[i].v : "—" }; int at = rui_table_add(t, c, i); rui_table_cell_style(t, at, 0, 1, 0); }
	for (int i = 0; i < n; i++) g_free(r[i].v);
	rui_table_done(t); add(rui_table_widget(t)); return t;
}
/* <details>: a summary that opens a block of monospace lines (max 260 px high, scrolled) */
typedef struct { int i; GtkWidget *pre, *sum; char *title; } Det;
static void det_toggle(GtkWidget *w, GdkEventButton *e, gpointer d) { (void)w; (void)e; Det *t = d; OPEN_DET[t->i] = !OPEN_DET[t->i]; gtk_widget_set_visible(t->pre, OPEN_DET[t->i]);
	char *s = g_strdup_printf("%s %s", OPEN_DET[t->i] ? "▼" : "▶", t->title); gtk_label_set_text(GTK_LABEL(t->sum), s); g_free(s); }
static void det_free(gpointer d) { Det *t = d; g_free(t->title); g_free(t); }
static void details(int i, const char *sum, GPtrArray *ls, const char *empty)
{
	Det *t = g_new0(Det, 1); t->i = i; t->title = g_strdup(sum);
	GtkWidget *eb = gtk_event_box_new(), *s = rui_label(""); t->sum = s; gtk_container_add(GTK_CONTAINER(eb), s); add(eb);
	char *st = g_strdup_printf("%s %s", OPEN_DET[i] ? "▼" : "▶", sum); gtk_label_set_text(GTK_LABEL(s), st); g_free(st);
	GString *b = g_string_new(NULL); for (guint k = 0; ls && k < ls->len; k++) g_string_append_printf(b, "%s%s", k ? "\n" : "", (char *)g_ptr_array_index(ls, k));
	GtkWidget *pre = gtk_label_new(b->len ? b->str : empty ? empty : "(none)"); g_string_free(b, TRUE); gtk_label_set_xalign(GTK_LABEL(pre), 0); gtk_label_set_line_wrap(GTK_LABEL(pre), TRUE); gtk_label_set_line_wrap_mode(GTK_LABEL(pre), PANGO_WRAP_WORD_CHAR); gtk_label_set_selectable(GTK_LABEL(pre), TRUE);
	PangoAttrList *al = pango_attr_list_new(); pango_attr_list_insert(al, pango_attr_family_new("monospace")); pango_attr_list_insert(al, pango_attr_size_new_absolute((int)(rui_px(12) * PANGO_SCALE))); gtk_label_set_attributes(GTK_LABEL(pre), al); pango_attr_list_unref(al);
	GtkWidget *sc = gtk_scrolled_window_new(NULL, NULL); gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC); gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(sc), TRUE);
	gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(sc), (int)lround(rui_px(260))); gtk_container_add(GTK_CONTAINER(sc), pre);
	gtk_widget_set_margin_top(sc, (int)lround(rui_px(6))); gtk_widget_set_margin_bottom(sc, (int)lround(rui_px(6))); t->pre = sc; add(sc);
	gtk_widget_show_all(sc); gtk_widget_set_no_show_all(sc, TRUE); gtk_widget_set_visible(sc, OPEN_DET[i]);
	gtk_widget_add_events(eb, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK); g_signal_connect_data(eb, "button-press-event", G_CALLBACK(det_toggle), t, (GClosureNotify)(void (*)(void))det_free, 0);
}
static GtkWidget *row_end(void) { GtkWidget *r = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (int)lround(rui_px(8))); gtk_widget_set_margin_top(r, (int)lround(rui_px(14))); return r; }

/* ------------------------------------------------------------------ sampling (every 2 s, as the page's): off the window's thread */
typedef struct { int tab; char *stats, *procs, *apps, *net, *memb; } Sample;
static void sample_free(Sample *s) { g_free(s->stats); g_free(s->procs); g_free(s->apps); g_free(s->net); g_free(s->memb); g_free(s); }
static void sample_work(GTask *t, gpointer src, gpointer data, GCancellable *c)
{
	(void)src; (void)c; Sample *s = data;
	rui_http("GET", "/api/stats", NULL, 0, &s->stats, NULL);
	if (s->tab == T_APPS || s->tab == T_PROCS) rui_http("GET", "/api/processes", NULL, 0, &s->procs, NULL);
	if (s->tab == T_APPS) rui_http("POST", "/api/apps/running", NULL, 0, &s->apps, NULL);
	if (s->tab == T_PERF) { rui_http("GET", "/api/net", NULL, 0, &s->net, NULL); rui_http("GET", "/api/memory/breakdown", NULL, 0, &s->memb, NULL); }
	g_task_return_boolean(t, TRUE);
}
static void draw(int full);
static void hist_push(void)
{
	double m = LMEMT > 0 ? LMEM / LMEMT * 100 : 0;
	if (NH == 60) { memmove(HC, HC + 1, 59 * sizeof *HC); memmove(HM, HM + 1, 59 * sizeof *HM); memmove(HN, HN + 1, 59 * sizeof *HN); NH = 59; }
	HC[NH] = LCPU < 0 ? 0 : LCPU; HM[NH] = m; HN[NH] = LNET; NH++;
}
static void sample_done(GObject *src, GAsyncResult *res, gpointer d)
{
	(void)src; (void)res; Sample *s = d; BUSY = 0;
	RuiJ *st = s->stats ? rui_jparse(s->stats) : NULL;
	if (st) { LCPU = rui_jnum(st, "cpu", 0); LMEMT = rui_jnum(st, "memTotal", 0); LMEM = LMEMT - rui_jnum(st, "memAvailable", 0);
		RuiJ *t = rui_jprop(st, "tempC"); LTEMP = rui_jtype(t) == 2 ? rui_jval(t) : NAN; if (LBAT) rui_jfree(LBAT); LBAT = NULL;
		if (rui_jprop(st, "battery")) { char *b = NULL; (void)b; LBAT = rui_jparse(s->stats); } rui_jfree(st); }
	if (s->procs) { RuiJ *p = rui_jparse(s->procs); if (p) { rui_jfree(PROCS); PROCS = p; } }
	if (s->apps) { RuiJ *a = rui_jparse(s->apps); rui_jfree(APPSRUN); APPSRUN = a; }
	if (s->memb) { RuiJ *m = rui_jparse(s->memb); rui_jfree(MEMB); MEMB = m; }
	if (s->net) { RuiJ *n = rui_jparse(s->net); RuiJ *l = rui_jprop(n, "interfaces"); double tot = 0; gint64 now = g_get_monotonic_time();
		for (int i = 0; i < rui_jlen(l); i++) { RuiJ *x = rui_jat(l, i); if (!strcmp(js(x, "name"), "lo") || rui_jbool(x, "virtual")) continue; tot += rui_jnum(x, "rx", 0) + rui_jnum(x, "tx", 0); }
		LNET = NETB >= 0 && now > NETT ? fmax(0, (tot - NETB) / ((now - NETT) / 1e6)) : 0; NETB = tot; NETT = now; rui_jfree(n); }
	hist_push();
	static int first = 1; if (first) { first = 0; const char *h = getenv("RUDOLF_TM_HIST"); int n = h ? atoi(h) : 0; while (NH < n && NH < 60) hist_push(); }
	if (TAB == T_APPS || TAB == T_PROCS || TAB == T_PERF) draw(s->tab != TAB ? 1 : 0);
	sample_free(s);
}
static void wins_got(const char *json, void *d);
static void sample(void)
{
	if (BUSY) return; BUSY = 1; Sample *s = g_new0(Sample, 1); s->tab = TAB;
	if (TAB == T_APPS) rui_shell_wins(W, wins_got, NULL);
	GTask *t = g_task_new(NULL, NULL, sample_done, s); g_task_set_task_data(t, s, NULL); g_task_run_in_thread(t, sample_work); g_object_unref(t);
}
static gboolean tick(gpointer d) { (void)d; GdkWindow *g = gtk_widget_get_window(W->win); if (g && (gdk_window_get_state(g) & GDK_WINDOW_STATE_ICONIFIED)) return G_SOURCE_CONTINUE; sample(); return G_SOURCE_CONTINUE; }
static void wins_got(const char *json, void *d) { (void)d; RuiJ *j = rui_jparse(json ? json : "[]"); if (!j) { char *w = g_strdup_printf("{\"l\":%s}", json); RuiJ *o = rui_jparse(w); g_free(w); j = o; }
	rui_jfree(WINSJ); WINSJ = j; if (TAB == T_APPS) draw(0); state_soon(); }
static RuiJ *wins_list(void) { RuiJ *l = rui_jprop(WINSJ, "l"); return l ? l : WINSJ; }

/* ------------------------------------------------------------------ ending: an administrator approves, except for the person's own apps */
typedef struct { long pid; int force, tree; char name[64], what[600], title[40]; } End;
static void end_go(End *e)
{
	char p[96], err[200]; snprintf(p, sizeof p, "/api/proc/end?pid=%ld%s%s", e->pid, e->force ? "&force=1" : "", e->tree ? "&tree=1" : "");
	char *out = NULL; int c = rui_http("POST", p, NULL, 0, &out, NULL); RuiJ *j = out ? rui_jparse(out) : NULL; g_free(out);
	act_note("end %ld force=%d tree=%d status=%d", e->pid, e->force, e->tree, c);
	if (c == 200) { char b[120]; double n = rui_jnum(j, "ended", -1); if (e->tree && n >= 0) snprintf(b, sizeof b, "%s: %.0f process%s", e->name, n, n == 1 ? "" : "es"); else snprintf(b, sizeof b, "%s", e->name);
		rui_notify(e->tree ? "Process tree ended" : "Process ended", b, 0); SEL = -1; }
	else { snprintf(err, sizeof err, "%s", rui_jstr(j, "error") ? rui_jstr(j, "error") : c ? "rudolfd answered an error" : "rudolfd did not answer"); char t[120]; snprintf(t, sizeof t, "Could not end %s", e->name); rui_notify(t, err, 1); }
	rui_jfree(j); g_free(e); g_timeout_add(500, (GSourceFunc)(void (*)(void))sample, NULL);
}
static void end_ok(int i, void *d) { End *e = d; if (i == 1) end_go(e); else { act_note("end %ld cancelled", e->pid); g_free(e); } }
static void end_elev(int ok, void *d) { End *e = d; if (ok == 1) end_go(e); else { act_note("end %ld refused (administrator)", e->pid); g_free(e); } }
static RuiJ *proc_of(long pid) { RuiJ *it = rui_jprop(PROCS, "items"); for (int i = 0; i < rui_jlen(it); i++) { RuiJ *p = rui_jat(it, i); if ((long)rui_jnum(p, "pid", 0) == pid) return p; } return NULL; }
static void end_proc(long pid, int force, int tree)
{
	RuiJ *p = proc_of(pid); if (!p || rui_jbool(p, "prot")) return;
	RuiJ *it = rui_jprop(PROCS, "items"); int nk = 0, kept = 0, own = !strcmp(js(p, "tag"), "app") && !strcmp(js(p, "user"), "rudolf");
	if (tree) {   /* (tmTree: breadth first, a protected one counted and not gone into) */
		GArray *q = g_array_new(FALSE, FALSE, sizeof(long)); g_array_append_val(q, pid);
		for (guint k = 0; k < q->len; k++) { long x = g_array_index(q, long, k);
			for (int i = 0; i < rui_jlen(it); i++) { RuiJ *c = rui_jat(it, i); long cp = (long)rui_jnum(c, "pid", 0); if ((long)rui_jnum(c, "ppid", -1) != x || cp == pid) continue;
				nk++; if (rui_jbool(c, "prot")) kept++; else g_array_append_val(q, cp); if (strcmp(js(c, "tag"), "app") || strcmp(js(c, "user"), "rudolf")) own = 0; } }
		g_array_free(q, TRUE); }
	End *e = g_new0(End, 1); e->pid = pid; e->force = force; e->tree = tree; snprintf(e->name, sizeof e->name, "%s", js(p, "name"));
	snprintf(e->title, sizeof e->title, "%s", tree ? "End process tree" : "End process");
	int n = nk - kept; char tw[400];
	if (tree) snprintf(tw, sizeof tw, "End %s (process %ld) and the %d process%s it started%s? Anything they were doing is lost.%s", e->name, pid, n, n == 1 ? "" : "es", force ? ", at once" : "", "");
	else snprintf(tw, sizeof tw, "End %s (process %ld)%s?", e->name, pid, force ? " at once" : "");
	if (tree && kept) { size_t l = strlen(tw); snprintf(tw + l, sizeof tw - l, " (%d that Rudolf OS needs stay running.)", kept); }
	snprintf(e->what, sizeof e->what, "%s%s", !strcmp(js(p, "tag"), "web") ? "This process shows web pages; ending it closes them (if it shows the desktop, the desktop starts again). " : "", tw);
	act_note("ask end %ld %s", pid, own ? "confirm" : "administrator");
	if (own) { const char *B[2] = { "Cancel", e->title }; rui_confirm(W, e->title, e->what, B, 2, 1, end_ok, e); }
	else rui_elevate(W, e->what, end_elev, e);
	state_soon();
}

/* ------------------------------------------------------------------ Apps */
static void win_btn(long id, int which, void *d)
{
	(void)d; RuiJ *l = wins_list(); RuiJ *o = rui_jat(l, (int)id); if (!o) return; int b = which % 16;
	char q[80]; snprintf(q, sizeof q, "winop %s %s", js(o, "id"), b == 0 ? "focus" : "end");
	if (b == 1 && !strcmp(js(o, "app"), "taskmgr")) return;   /* (End task on Task Manager itself: nothing, as the page's) */
	rui_shell(W, q); act_note("%s", q);
	if (b == 1) { rui_notify("Task ended", js(o, "title"), 0); g_timeout_add(400, (GSourceFunc)(void (*)(void))sample, NULL); }
}
static const char *KIND57(const char *k) { return !strcmp(k, "flatpak") ? "Linux app" : !strcmp(k, "wine") ? "Windows program" : !strcmp(k, "debian") ? "Linux program" : k; }
static char *name57(RuiJ *x) { if (!strcmp(js(x, "kind"), "flatpak")) { char k[160], n[160]; snprintf(k, sizeof k, "lx:%s", js(x, "app")); app_name(k, NULL, 0, n, sizeof n); if (n[0]) return g_strdup(n); }
	char *s = g_strdup(js(x, "name")); size_t l = strlen(s); if (l > 4 && !g_ascii_strcasecmp(s + l - 4, ".exe")) s[l - 4] = 0; return s; }
static void app57_btn(long id, int which, void *d)
{
	(void)d; RuiJ *l = rui_jprop(APPSRUN, "apps"); RuiJ *x = rui_jat(l, (int)id); if (!x) return; char *n = name57(x), p[300], *e = g_uri_escape_string(js(x, "id"), NULL, TRUE);
	if (which % 16 == 0) { snprintf(p, sizeof p, "/api/apps/end?id=%s", e); char *out = NULL; int c = rui_http("POST", p, NULL, 0, &out, NULL); RuiJ *j = out ? rui_jparse(out) : NULL;
		char t[200]; if (c == 200) { snprintf(t, sizeof t, "%s was ended", n); rui_notify(t, rui_jbool(j, "forced") ? "It did not quit when asked, so it was stopped." : "", 0); } else { snprintf(t, sizeof t, "%s was not ended", n); rui_notify(t, js(j, "error"), 1); }
		rui_jfree(j); g_free(out); act_note("apps/end %s %d", js(x, "id"), c); }
	else { snprintf(p, sizeof p, "/api/apps/kill?id=%s", e); char *out = NULL; int c = rui_http("POST", p, NULL, 0, &out, NULL); char t[200];
		snprintf(t, sizeof t, c == 200 ? "%s was force quit" : "%s could not be force quit", n); rui_notify(t, "", c != 200); g_free(out); act_note("apps/kill %s %d", js(x, "id"), c); }
	g_free(e); g_free(n); g_timeout_add(300, (GSourceFunc)(void (*)(void))sample, NULL);
}
static void draw_apps(void)
{
	RuiTable *t = rui_table_new(6); static const char *const H[6] = { "App", "Window", "Status", "Workspace", "Memory", "" };
	for (int c = 0; c < 6; c++) rui_table_col(t, c, H[c], c == 4 ? RUI_RIGHT : RUI_LEFT, 0);
	RuiJ *l = wins_list(); int n = rui_jlen(l);
	if (!WINSJ) {   /* (no shell to ask: only this window, as the page alone) */
		const char *c[6] = { "📊 Task Manager", "Task Manager", "Active", "1", "—", "" }; int r = rui_table_add(t, c, -1); rui_table_set_button(t, r, 5, "Switch to\nEnd task", NULL, NULL); }
	for (int i = 0; i < n; i++) { RuiJ *o = rui_jat(l, i); char ic[48], nm[120], app[200], mb[32]; const char *k = js(o, "app"); app_name(k, ic, sizeof ic, nm, sizeof nm);
		snprintf(app, sizeof app, "%s %s", js(o, "icon")[0] ? js(o, "icon") : ic, nm[0] ? nm : "—");
		RuiJ *me = rui_jprop(o, "me"); RuiJ *m = rui_jprop(me, "mb");
		if (!strcmp(k, "taskmgr") && !rui_jtype(me)) { /* (this window: its own memory, measured) */ }
		if (rui_jtype(m) == 2) snprintf(mb, sizeof mb, "%.1f MB", rui_jval(m)); else snprintf(mb, sizeof mb, "—");
		char ws[16]; snprintf(ws, sizeof ws, "%.0f", rui_jnum(o, "ws", 1) ? rui_jnum(o, "ws", 1) : 1);
		const char *stt = rui_jnum(o, "rest", 0) ? "Resting (no memory used)" : rui_jnum(o, "min", 0) ? "Minimized" : rui_jnum(o, "act", 0) ? "Active" : "Running";
		const char *c[6] = { app, js(o, "title"), stt, ws, mb, "" }; int r = rui_table_add(t, c, i); rui_table_set_button(t, r, 5, "Switch to\nEnd task", win_btn, NULL); }
	rui_table_done(t); add(rui_table_widget(t)); named("apps-table", rui_table_widget(t));
	/* (apps57.js tmApps57) the apps that run, below the windows */
	RuiJ *al = rui_jprop(APPSRUN, "apps");
	if (APPSRUN) { h4("Apps and programs running");
		if (!rui_jlen(al)) msg("No Linux apps or Windows programs are running.", 10);
		else { RuiTable *a = rui_table_new(5); static const char *const H2[5] = { "App", "Kind", "Window", "Memory", "" }; for (int c = 0; c < 5; c++) rui_table_col(a, c, H2[c], c == 3 ? RUI_RIGHT : RUI_LEFT, 0);
			int m = rui_jlen(al), *ord = g_new(int, m); for (int i = 0; i < m; i++) ord[i] = i;
			for (int i = 0; i < m; i++) for (int j = i + 1; j < m; j++) if (rui_jnum(rui_jat(al, ord[j]), "rss", 0) > rui_jnum(rui_jat(al, ord[i]), "rss", 0)) { int x = ord[i]; ord[i] = ord[j]; ord[j] = x; }
			for (int i = 0; i < m; i++) { RuiJ *x = rui_jat(al, ord[i]); char *nm = name57(x); const char *c[5] = { nm, KIND57(js(x, "kind")), rui_jbool(x, "window") ? "Open" : "None (in the background)", fmtB(rui_jnum(x, "rss", 0)), "" };
				int r = rui_table_add(a, c, ord[i]); rui_table_set_button(a, r, 4, "End\nForce quit", app57_btn, NULL); g_free(nm); }
			g_free(ord); rui_table_done(a); add(rui_table_widget(a)); } }
	msg("Window memory is estimated from each window's contents; a real tab shows the JavaScript memory of its page. If the whole desktop stops responding for a minute, Rudolf OS restarts it and reopens your apps.", 8);
}

/* ------------------------------------------------------------------ Processes */
static void pick(long pid)
{
	SEL = pid; RuiJ *p = pid >= 0 ? proc_of(pid) : NULL; if (!p) SEL = -1; int dis = !p || rui_jbool(p, "prot");
	gtk_widget_set_sensitive(PEND, !dis); gtk_widget_set_sensitive(PTREE, !dis); gtk_widget_set_sensitive(PNOW, !dis);
	char b[300] = ""; if (p) { if (rui_jbool(p, "prot")) snprintf(b, sizeof b, "%s: Rudolf OS needs it, so it cannot be ended", js(p, "name"));
		else { const char *c = js(p, "cmd"); char cut[96]; g_utf8_strncpy(cut, c, 90); snprintf(b, sizeof b, "%s (%ld)%s%s", js(p, "name"), pid, *c ? " — " : "", *c ? cut : ""); } }
	gtk_label_set_text(GTK_LABEL(PSEL), b); if (PT) rui_table_select(PT, SEL); state_soon();
}
static const char *SORTC[5] = { "name", "pid", "user", "cpu", "rss" };
typedef struct { RuiJ *p; } PR;
static int pcmp(const void *a, const void *b)
{
	RuiJ *x = ((PR *)a)->p, *y = ((PR *)b)->p; int up = SORTUP ? 1 : -1;
	if (!strcmp(SORTK, "name") || !strcmp(SORTK, "user")) { const char *s = js(x, SORTK), *t = js(y, SORTK); return g_utf8_collate(s, t) * up; }
	RuiJ *u = rui_jprop(x, SORTK), *v = rui_jprop(y, SORTK); double a1 = rui_jtype(u) == 2 ? rui_jval(u) : -1, b1 = rui_jtype(v) == 2 ? rui_jval(v) : -1;
	return (a1 > b1) - (a1 < b1) ? ((a1 > b1) - (a1 < b1)) * up : 0;
}
static void procs_menu_do(int i, void *d) { long pid = (long)(intptr_t)d; if (i == 0) end_proc(pid, 0, 0); else if (i == 1) end_proc(pid, 0, 1); else if (i == 2) end_proc(pid, 1, 0); }
static void procs_row(long pid, int how, double x, double y, void *d)
{
	(void)d; RuiJ *p = proc_of(pid); if (!p) return;
	if (how == 0 || how == 2 || how == 5) { pick(pid); return; }
	if (how == 1) { pick(pid); int off = rui_jbool(p, "prot"); RuiMenuItem M[3] = { {"End process", NULL, NULL, off, 0}, {"End process tree", NULL, NULL, off, 0}, {"End now", NULL, NULL, off, 0} };
		char t[120]; snprintf(t, sizeof t, "%s (%ld)", js(p, "name"), pid); rui_menu(W->win, x, y, t, M, 3, procs_menu_do, (void *)(intptr_t)pid); act_note("menu %ld", pid); state_soon(); return; }
	if ((how == 3 || how == 4) && !rui_jbool(p, "prot")) end_proc(pid, 0, how == 4);
}
static void procs_table(void);
static void procs_head(int c, void *d) { (void)d; if (!strcmp(SORTK, SORTC[c])) SORTUP = !SORTUP; else { snprintf(SORTK, sizeof SORTK, "%s", SORTC[c]); SORTUP = c == 0 || c == 1; } act_note("sort %s %d", SORTK, SORTUP); procs_table(); }
static void procs_table(void)
{
	if (!PT) return; RuiJ *it = rui_jprop(PROCS, "items"); int n = rui_jlen(it);
	static const char *const HN[5] = { "Name", "PID", "User", "CPU", "Memory" };
	for (int c = 0; c < 5; c++) { char h[64]; snprintf(h, sizeof h, "%s%s", rui_tr(HN[c]), !strcmp(SORTK, SORTC[c]) ? (SORTUP ? " ▲" : " ▼") : ""); rui_table_heading(PT, c, h); }
	rui_table_clear(PT);
	char *q = g_utf8_strdown(g_strstrip(g_strdup(FILT)), -1);
	for (unsigned g = 0; g < G_N_ELEMENTS(TMK); g++) {
		PR *v = g_new(PR, n); int m = 0; double mem = 0, cpu = 0;
		for (int i = 0; i < n; i++) { RuiJ *p = rui_jat(it, i); if (strcmp(js(p, "tag"), TMK[g][0])) continue;
			if (*q) { char *h = g_strdup_printf("%s %s %.0f", js(p, "name"), js(p, "cmd"), rui_jnum(p, "pid", 0)), *hl = g_utf8_strdown(h, -1); int ok = strstr(hl, q) != NULL; g_free(h); g_free(hl); if (!ok) continue; }
			v[m++].p = p; mem += rui_jnum(p, "rss", 0); cpu += rui_jnum(p, "cpu", 0); }
		if (!m) { g_free(v); continue; }
		qsort(v, m, sizeof *v, pcmp);
		char rest[120]; snprintf(rest, sizeof rest, "%d · %.1f%% CPU · %s", m, cpu, fmtB(mem)); rui_table_group(PT, rui_tr(TMK[g][1]), rest);
		for (int i = 0; i < m; i++) { RuiJ *p = v[i].p; char nm[200], pid[24], cp[24]; const char *cmd = js(p, "cmd"), *ty = strstr(cmd, "--type=");
			char tyb[64] = ""; if (!strcmp(js(p, "name"), "chrome") && ty) { snprintf(tyb, sizeof tyb, "%.*s", (int)strcspn(ty + 7, " \t"), ty + 7); char *x = strstr(tyb, "-process"); if (x) memmove(x, x + 8, strlen(x + 8) + 1); }
			snprintf(nm, sizeof nm, "%s%s%s%s", rui_jbool(p, "prot") ? "🔒 " : "", js(p, "name"), tyb[0] ? " · " : !strcmp(TMK[g][0], "desktop") && !strcmp(js(p, "name"), "chrome") ? " · browser" : "", tyb);
			snprintf(pid, sizeof pid, "%.0f", rui_jnum(p, "pid", 0)); RuiJ *c = rui_jprop(p, "cpu"); if (rui_jtype(c) == 2) snprintf(cp, sizeof cp, "%.1f%%", rui_jval(c)); else snprintf(cp, sizeof cp, "…");
			double rss = rui_jnum(p, "rss", 0); const char *cells[5] = { nm, pid, js(p, "user"), cp, rss ? fmtB(rss) : "—" };
			rui_table_add(PT, cells, (long)rui_jnum(p, "pid", 0)); }
		g_free(v); }
	g_free(q); rui_table_select(PT, SEL); rui_table_done(PT); pick(proc_of(SEL) ? SEL : -1);
}
static void find_changed(GtkEditable *e, gpointer d) { (void)d; snprintf(FILT, sizeof FILT, "%s", gtk_entry_get_text(GTK_ENTRY(e))); procs_table(); state_soon(); }
static void pbtn(GtkButton *b, gpointer d) { (void)b; if (SEL >= 0) end_proc(SEL, GPOINTER_TO_INT(d) == 2, GPOINTER_TO_INT(d) == 1); }
static void draw_procs(int full)
{
	if (!PROCS) { add(rui_label("Loading…")); return; }
	if (!PBAR || full) {
		PBAR = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (int)lround(rui_px(8))); gtk_widget_set_margin_top(PBAR, (int)lround(rui_px(14))); gtk_widget_set_margin_bottom(PBAR, (int)lround(rui_px(6)));
		PFIND = rui_entry("Find a process"); gtk_widget_set_size_request(PFIND, (int)lround(rui_px(234)), -1); gtk_entry_set_text(GTK_ENTRY(PFIND), FILT); g_signal_connect(PFIND, "changed", G_CALLBACK(find_changed), NULL);
		PEND = rui_button("End process", 0); PTREE = rui_button("End process tree", 0); PNOW = rui_button("End now", 0);
		gtk_widget_set_tooltip_text(PTREE, rui_tr("Ends it and every process it started (as in Windows XP)")); gtk_widget_set_tooltip_text(PNOW, rui_tr("Ends it at once, without letting it save or tidy up"));
		g_signal_connect(PEND, "clicked", G_CALLBACK(pbtn), GINT_TO_POINTER(0)); g_signal_connect(PTREE, "clicked", G_CALLBACK(pbtn), GINT_TO_POINTER(1)); g_signal_connect(PNOW, "clicked", G_CALLBACK(pbtn), GINT_TO_POINTER(2));
		PSEL = rui_msg(""); gtk_label_set_line_wrap(GTK_LABEL(PSEL), FALSE); gtk_label_set_ellipsize(GTK_LABEL(PSEL), PANGO_ELLIPSIZE_END); gtk_widget_set_margin_top(PSEL, (int)lround(rui_px(10))); gtk_widget_set_hexpand(PSEL, TRUE);
		gtk_box_pack_start(GTK_BOX(PBAR), PFIND, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(PBAR), PEND, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(PBAR), PTREE, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(PBAR), PNOW, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(PBAR), PSEL, TRUE, TRUE, 0);
		add(PBAR); PT = rui_table_new(5); for (int c = 0; c < 5; c++) rui_table_col(PT, c, "", c == 0 || c == 2 ? RUI_LEFT : RUI_RIGHT, 0);
		rui_table_on(PT, procs_row, procs_head, NULL); add(rui_table_widget(PT)); named("find", PFIND); named("endp", PEND); named("endt", PTREE); named("endn", PNOW); named("ptable", rui_table_widget(PT));
		gtk_widget_show_all(PBAR); gtk_widget_show(rui_table_widget(PT));
	}
	procs_table();
}

/* ------------------------------------------------------------------ Performance */
typedef struct { const double *v; double top; const char *color; } Graph;
static gboolean graph_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Graph *g = d; int W0 = gtk_widget_get_allocated_width(w), H0 = gtk_widget_get_allocated_height(w); double z = RUI.z;
	/* the page's canvas: 640 x 100, drawn, then shown at the box's width (its border, its corners) */
	cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 640, 100); cairo_t *c = cairo_create(s);
	cairo_set_source_rgb(c, 11 / 255.0, 18 / 255.0, 32 / 255.0); cairo_paint(c);
	cairo_set_source_rgb(c, 30 / 255.0, 41 / 255.0, 59 / 255.0); cairo_set_line_width(c, 1);
	for (int y = 0; y <= 4; y++) { cairo_move_to(c, 0, y * 25); cairo_line_to(c, 640, y * 25); cairo_stroke(c); }
	double top = g->top; if (top <= 0) { double m = 1; for (int i = 0; i < NH; i++) if (g->v[i] > m) m = g->v[i]; top = m * 1.2; }
	unsigned col = 0x38bdf8; sscanf(g->color + 1, "%x", &col); cairo_set_source_rgb(c, (col >> 16 & 255) / 255.0, (col >> 8 & 255) / 255.0, (col & 255) / 255.0); cairo_set_line_width(c, 2);
	for (int i = 0; i < NH; i++) { double x = i * (640.0 / 59), y = 98 - g->v[i] / top * 96; if (i) cairo_line_to(c, x, y); else cairo_move_to(c, x, y); }
	cairo_stroke(c); cairo_destroy(c);
	double bw = z, r = 6 * z;
	cairo_save(cr); cairo_new_sub_path(cr); { double x = 0, y = 0, ww = W0, hh = H0; cairo_arc(cr, x + ww - r, y + r, r, -M_PI / 2, 0); cairo_arc(cr, x + ww - r, y + hh - r, r, 0, M_PI / 2); cairo_arc(cr, x + r, y + hh - r, r, M_PI / 2, M_PI); cairo_arc(cr, x + r, y + r, r, M_PI, 1.5 * M_PI); cairo_close_path(cr); }
	cairo_clip(cr); rui_color(cr, "bd", 1); cairo_paint(cr);
	cairo_translate(cr, bw, bw); cairo_scale(cr, (W0 - 2 * bw) / 640.0, (H0 - 2 * bw) / 100.0); cairo_set_source_surface(cr, s, 0, 0); cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR); cairo_paint(cr);
	cairo_restore(cr); cairo_surface_destroy(s); return TRUE;
}
static void graph(const double *v, double top, const char *color)
{
	Graph *g = g_new0(Graph, 1); g->v = v; g->top = top; g->color = color; GtkWidget *a = gtk_drawing_area_new(); gtk_widget_set_size_request(a, -1, (int)lround(rui_px(102)));
	g_signal_connect_data(a, "draw", G_CALLBACK(graph_draw), g, (GClosureNotify)(void (*)(void))g_free, 0); add(a);
}
/* the cards (.tmcards: as many 150 px columns as fit, the width shared out) */
typedef struct { char t[64], v[96]; } Card;
/* the cards' places: as many 150 px columns as fit (gap 8), each card 1 px border, 8 px padding, its name in <small> (the font's smaller
   size, 13 px line), its value in 18 px bold, wrapping; a row as tall as its tallest card */
static double SMALLPX(void) { return 13 / 1.2; }
static int card_lines(cairo_t *cr, const char *v, double w) { PangoLayout *l = rui_layout(cr, v, 18, 700); pango_layout_set_width(l, (int)(w * RUI.z * PANGO_SCALE)); pango_layout_set_wrap(l, PANGO_WRAP_WORD_CHAR); int n = pango_layout_get_line_count(l); g_object_unref(l); return n; }
static double cards_layout(GArray *a, cairo_t *cr, double Wd, double *cw, int *cols, double *rowh, int maxr)
{
	*cols = MAX(1, (int)floor((Wd + 8) / (150 + 8))); *cw = (Wd - (*cols - 1) * 8) / *cols; int rows = ((int)a->len + *cols - 1) / *cols; double H = 0;
	for (int r = 0; r < rows && r < maxr; r++) { double h = 0; for (int k = 0; k < *cols; k++) { guint i = r * *cols + k; if (i >= a->len) break; Card *c = &g_array_index(a, Card, i);
			double ch = 1 + 8 + 13 + 21 * card_lines(cr, c->v, *cw - 18) + 8 + 1; if (ch > h) h = ch; } rowh[r] = h; H += h + (r ? 8 : 0); }
	return H;
}
static gboolean cards_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	GArray *a = d; double z = RUI.z, Wd = gtk_widget_get_allocated_width(w) / z, cw, rowh[16]; int cols; cards_layout(a, cr, Wd, &cw, &cols, rowh, 16);
	GtkStyleContext *sc = gtk_widget_get_style_context(w); GdkRGBA fg; gtk_style_context_get_color(sc, gtk_style_context_get_state(sc), &fg);
	double y = 0;
	for (guint i = 0; i < a->len; i++) { Card *c = &g_array_index(a, Card, i); int r = i / cols, k = i % cols; if (k == 0 && r) y += rowh[r - 1] + 8; double x = k * (cw + 8), HH = rowh[r];
		rui_color(cr, "bd", 1); cairo_set_line_width(cr, z); cairo_new_sub_path(cr); double rr = 10 * z, X = (x + .5) * z, Y = (y + .5) * z, WW = (cw - 1) * z, H1 = (HH - 1) * z;
		cairo_arc(cr, X + WW - rr, Y + rr, rr, -M_PI / 2, 0); cairo_arc(cr, X + WW - rr, Y + H1 - rr, rr, 0, M_PI / 2); cairo_arc(cr, X + rr, Y + H1 - rr, rr, M_PI / 2, M_PI); cairo_arc(cr, X + rr, Y + rr, rr, M_PI, 1.5 * M_PI); cairo_close_path(cr); cairo_stroke(cr);
		rui_color(cr, "mut", 1); PangoLayout *t = rui_layout(cr, c->t, SMALLPX(), 400); PangoRectangle lg; pango_layout_get_pixel_extents(t, NULL, &lg); cairo_move_to(cr, round((x + 9) * z), round((y + 9) * z + (13 * z - lg.height) / 2)); pango_cairo_show_layout(cr, t); g_object_unref(t);
		gdk_cairo_set_source_rgba(cr, &fg); PangoLayout *v = rui_layout(cr, c->v, 18, 700); pango_layout_set_width(v, (int)((cw - 18) * z * PANGO_SCALE)); pango_layout_set_wrap(v, PANGO_WRAP_WORD_CHAR);
		int n = pango_layout_get_line_count(v); for (int li = 0; li < n; li++) { PangoLayoutLine *ln = pango_layout_get_line_readonly(v, li); PangoRectangle e; pango_layout_line_get_pixel_extents(ln, NULL, &e);
			cairo_move_to(cr, round((x + 9) * z), round((y + 22 + 21 * li) * z + (21 * z - e.height) / 2) - e.y); pango_cairo_show_layout_line(cr, ln); }
		g_object_unref(v); }
	return TRUE;
}
static gboolean cards_h(gpointer d) { GtkWidget *w = d; gtk_widget_set_size_request(w, -1, GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), "h"))); g_object_unref(w); return G_SOURCE_REMOVE; }   /* (not while allocating) */
static void cards_alloc(GtkWidget *w, GdkRectangle *r, gpointer d)
{
	GArray *a = d; cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_A8, 1, 1); cairo_t *cr = cairo_create(s); double cw, rowh[16]; int cols;
	double H = cards_layout(a, cr, r->width / RUI.z, &cw, &cols, rowh, 16); cairo_destroy(cr); cairo_surface_destroy(s);
	int h = (int)ceil(rui_px(H)); if (h != gtk_widget_get_allocated_height(w)) { g_object_set_data(G_OBJECT(w), "h", GINT_TO_POINTER(h)); g_idle_add(cards_h, g_object_ref(w)); }
}
static void cards(GArray *a)
{
	GtkWidget *w = gtk_drawing_area_new(); g_signal_connect_data(w, "draw", G_CALLBACK(cards_draw), a, (GClosureNotify)(void (*)(void))g_array_unref, 0);
	add(w); g_signal_connect(w, "size-allocate", G_CALLBACK(cards_alloc), a);   /* (its 10 px under it fall within the heading's 16 above: CSS margins collapse) */
	gtk_widget_set_size_request(w, -1, (int)lround(rui_px(52))); named("cards", w);
}
/* (.tmmrow i: 10 px, rgba(127,127,127,.18), 5 px corners; its b the share, #a78bfa) */
static gboolean membar_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	double pc = *(double *)d, W0 = gtk_widget_get_allocated_width(w), H0 = gtk_widget_get_allocated_height(w), r = H0 / 2;
	cairo_new_sub_path(cr); cairo_arc(cr, W0 - r, r, r, -M_PI / 2, M_PI / 2); cairo_arc(cr, r, r, r, M_PI / 2, 1.5 * M_PI); cairo_close_path(cr); cairo_clip(cr);
	cairo_set_source_rgba(cr, 127 / 255.0, 127 / 255.0, 127 / 255.0, .18); cairo_paint(cr);
	cairo_set_source_rgb(cr, 0xa7 / 255.0, 0x8b / 255.0, 0xfa / 255.0); cairo_rectangle(cr, 0, 0, W0 * fmin(100, pc) / 100, H0); cairo_fill(cr); return TRUE;
}
static void cc_click(GtkButton *b, gpointer d)
{
	GtkWidget *sp = d; gtk_widget_set_sensitive(GTK_WIDGET(b), FALSE); char *out = NULL; int c = rui_http("POST", "/api/memory/dropcache", NULL, 0, &out, NULL); RuiJ *j = out ? rui_jparse(out) : NULL;
	if (c == 200) { snprintf(CCMSG, sizeof CCMSG, "Cleared: %s more is free now.", fmtB(rui_jnum(j, "freed", 0))); CCT = g_get_monotonic_time(); gtk_label_set_text(GTK_LABEL(sp), CCMSG); }
	else { gtk_label_set_text(GTK_LABEL(sp), js(j, "error")); gtk_widget_set_sensitive(GTK_WIDGET(b), TRUE); }
	act_note("dropcache %d", c); rui_jfree(j); g_free(out); state_soon();
}
static void draw_perf(void)
{
	GArray *a = g_array_new(FALSE, TRUE, sizeof(Card)); Card c;
#define CARD(T, ...) do { memset(&c, 0, sizeof c); snprintf(c.t, sizeof c.t, "%s", rui_tr(T)); snprintf(c.v, sizeof c.v, __VA_ARGS__); g_array_append_val(a, c); } while (0)
	if (LCPU < 0) CARD("CPU", "—"); else CARD("CPU", "%.0f%%", LCPU);
	if (LMEM < 0) CARD("Memory", "—"); else if (LMEMT) CARD("Memory", "%s of %s", fmtB(LMEM), fmtB(LMEMT)); else CARD("Memory", "%s", fmtB(LMEM));
	CARD("Network", "%s/s", fmtB(LNET));
	if (!isnan(LTEMP)) CARD("Temperature", "%.0f °C", LTEMP);
	RuiJ *bl = rui_jprop(LBAT, "battery"); for (int i = 0; i < rui_jlen(bl); i++) { RuiJ *b = rui_jat(bl, i); const char *s = js(b, "status"); CARD("Battery", "%.0f%%%s%s", rui_jnum(b, "capacity", 0), *s ? " · " : "", s); }
	CARD("Windows", "%d", WINSJ ? rui_jlen(wins_list()) : 1); CARD("CPU threads", "%d", g_get_num_processors());
#undef CARD
	cards(a);
	h4("CPU (last minute)"); graph(HC, 100, "#38bdf8");
	h4("Memory in use"); graph(HM, 100, "#a78bfa");
	h4("Where the memory goes");
	if (!MEMB) msg("Not available.", 0);
	else { double tot = rui_jnum(MEMB, "total", 0), used = tot - rui_jnum(MEMB, "free", 0); if (used <= 0) used = 1;
		static const char *const R[][3] = { {"Programs and web pages", "programs", "The desktop, Oracle's tabs and every app, in memory now."}, {"Files in memory (cache)", "cache", ""}, {"Kernel", "kernel", "Linux itself: its tables, stacks and modules."},
			{"Graphics", "graphics", "Memory the graphics chip uses for the screen and for pages and games."}, {"SSD buffer", "ssd", "An SSD without memory of its own borrows some of the computer's; it makes the drive faster."},
			{"Compressed memory", "compressed", "What compressed memory itself takes to hold what was moved into it."}, {"Other drivers and kernel", "other", "Not itemised: mostly the graphics driver's spare pages (at most 64 MB), network buffers and what Linux takes while starting. Rudolf OS cannot measure these separately while the system is protected (Secure Boot)."} };
		for (unsigned i = 0; i < G_N_ELEMENTS(R); i++) { double v = rui_jnum(MEMB, R[i][1], 0); if (!v && strcmp(R[i][1], "other")) continue;
			GtkWidget *row = gtk_grid_new(); gtk_grid_set_column_spacing(GTK_GRID(row), (int)lround(rui_px(8))); gtk_widget_set_margin_top(row, (int)lround(rui_px(3))); gtk_widget_set_margin_bottom(row, (int)lround(rui_px(3)));
			GtkWidget *n = rui_label(R[i][0]); gtk_widget_set_size_request(n, (int)lround(rui_px(200)), -1);
			char why[400]; if (!strcmp(R[i][1], "cache")) { double sh = rui_jnum(MEMB, "shared", 0); snprintf(why, sizeof why, "Recently used files and the system's own; given back as soon as anything needs the memory%s%s%s.", sh ? " (" : "", sh ? fmtB(sh) : "", sh ? " of it is shared memory, which is not)" : ""); }
			else snprintf(why, sizeof why, "%s", R[i][2]); gtk_widget_set_tooltip_text(row, why);
			GtkWidget *bar = gtk_drawing_area_new(); gtk_widget_set_hexpand(bar, TRUE); gtk_widget_set_size_request(bar, -1, (int)lround(rui_px(10))); gtk_widget_set_valign(bar, GTK_ALIGN_CENTER);
			double *pc = g_new(double, 1); *pc = fmax(1, round(v * 100 / used));
			g_signal_connect_data(bar, "draw", G_CALLBACK(membar_draw), pc, (GClosureNotify)(void (*)(void))g_free, 0);
			GtkWidget *e = rui_label(fmtB(v)); gtk_label_set_xalign(GTK_LABEL(e), 1); gtk_widget_set_size_request(e, (int)lround(rui_px(80)), -1);
			gtk_grid_attach(GTK_GRID(row), n, 0, 0, 1, 1); gtk_grid_attach(GTK_GRID(row), bar, 1, 0, 1, 1); gtk_grid_attach(GTK_GRID(row), e, 2, 0, 1, 1); add(row); }
		char f[300]; snprintf(f, sizeof f, "%s of %s is used; %s is available (free, and the file cache that can be given back). Point at a row for what it is.", fmtB(used), fmtB(tot), fmtB(rui_jnum(MEMB, "available", 0))); msg(f, 10);
		GtkWidget *cc = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (int)lround(rui_px(8))); gtk_widget_set_margin_top(cc, (int)lround(rui_px(8)));
		GtkWidget *b = rui_button("Clear cache", 0), *sp = rui_msg(g_get_monotonic_time() - CCT < 15000000 && CCT ? CCMSG : "Frees the memory holding recently used files at once. Nothing is lost; files are read again when next used, so things may be a little slower for a moment.");
		gtk_widget_set_margin_top(sp, (int)lround(rui_px(10))); gtk_widget_set_hexpand(sp, TRUE); g_signal_connect(b, "clicked", G_CALLBACK(cc_click), sp); gtk_box_pack_start(GTK_BOX(cc), b, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(cc), sp, TRUE, TRUE, 0); add(cc); named("clearcache", b); }
	h4("Network (received and sent, per second)"); graph(HN, 0, "#34d399");
}

/* ------------------------------------------------------------------ the page's own storage (rudolfd's copy, lsync), a key at a time */
static char ACCT[80];
static RuiJ *ls_get(const char *k) { char *e = g_uri_escape_string(k, NULL, TRUE), p[300]; snprintf(p, sizeof p, "/api/lsync?key=%s", e); g_free(e);
	RuiJ *r = rui_get(p); const char *v = rui_jstr(r, "v"); RuiJ *o = v ? rui_jparse(v) : NULL; if (!o && v) { char *w = g_strdup_printf("{\"x\":%s}", v); RuiJ *x = rui_jparse(w); g_free(w); o = x; } rui_jfree(r); return o; }
static int ls_set(const char *k, const char *json)
{
	GString *b = g_string_new("{\"part\":1,\"set\":{\""); g_string_append(b, k); g_string_append(b, "\":\"");
	for (const char *c = json; *c; c++) { if (*c == '"' || *c == '\\') g_string_append_c(b, '\\'); g_string_append_c(b, *c); }
	g_string_append(b, "\"}}"); int c = rui_post("/api/lsync", b->str, NULL, 0); g_string_free(b, TRUE); return c;
}
static RuiJ *ls_list(RuiJ *o) { RuiJ *x = rui_jprop(o, "x"); return x ? x : o; }   /* (an array is given wrapped: rudolf-native's reader takes objects) */

/* ------------------------------------------------------------------ System */
static void svc_elev(int ok, void *d) { char *n = d; if (ok == 1) { char p[96], err[200]; snprintf(p, sizeof p, "/api/service/restart?name=%s", n); int c = rui_post(p, NULL, err, sizeof err);
	char *lab = g_object_get_data(G_OBJECT(W->win), n); if (c == 200) rui_notify(lab ? lab : n, "Started again.", 0); else { char t[120]; snprintf(t, sizeof t, "Could not restart %s", lab ? lab : n); rui_notify(t, err, 1); }
	act_note("service/restart %s %d", n, c); } else act_note("service/restart %s refused (administrator)", n); g_free(n); state_soon(); }
static void desk_elev(int ok, void *d) { (void)d; if (ok == 1) { rui_post("/api/shell/switch", NULL, NULL, 0); act_note("shell/switch"); } }
static void svc_btn(long id, int which, void *d)
{
	(void)which; (void)d; static const char *const RS[] = { NULL, "display", "sound", "wifi" };
	if (id == 100) { rui_elevate(W, rui_tr("Restart the desktop? Open apps close; your files are saved."), desk_elev, NULL); return; }
	if (id < 1 || id > 3) return; char *lab = g_object_get_data(G_OBJECT(W->win), RS[id]); char r[160]; snprintf(r, sizeof r, "%s %s?", rui_tr("Restart"), lab ? lab : RS[id]);
	rui_elevate(W, r, svc_elev, g_strdup(RS[id]));
}
static void draw_system(void);
static void refresh_sys(GtkButton *b, gpointer d) { (void)b; (void)d; rui_jfree(SYSJ); SYSJ = NULL; draw(1); }
static void sys_get(void) { if (!SYSJ) SYSJ = rui_get("/api/system"); if (!SBS) SBS = rui_get("/api/secureboot/status"); rui_jfree(UPS); UPS = rui_get("/api/sys/status"); }
static void draw_system(void)
{
	sys_get(); RuiJ *s = SYSJ, *k = rui_jprop(s, "kernel"), *sv = rui_jprop(s, "services");
	GtkWidget *rb = row_end(); gtk_widget_set_halign(rb, GTK_ALIGN_END); GtkWidget *rf = rui_button("Refresh", 0); g_signal_connect(rf, "clicked", G_CALLBACK(refresh_sys), NULL); gtk_box_pack_start(GTK_BOX(rb), rf, FALSE, FALSE, 0); add(rb); named("refresh", rf);
	h4("Rudolf OS");
	{ char st[300] = "—"; if (k) { const char *m = js(k, "media"); const char *mn = !strcmp(m, "cd") ? "disc" : !strcmp(m, "usb") ? "USB drive" : !strcmp(m, "sd") ? "SD card" : !strcmp(m, "disk") ? "internal disk or USB drive" : m; char *bu = g_ascii_strup(js(k, "boot"), -1);
			snprintf(st, sizeof st, "%s, %s%s%s", bu, mn, rui_jbool(k, "live") ? " (live: changes are not kept)" : "", rui_jbool(k, "recovery") ? ", recovery mode" : ""); g_free(bu); }
	  char up[64] = "", ld[96] = ""; if (k) { dur((long)rui_jnum(k, "uptime", 0), up, sizeof up); RuiJ *l = rui_jprop(k, "load"); GString *g = g_string_new(NULL); for (int i = 0; i < rui_jlen(l); i++) g_string_append_printf(g, "%s%g", i ? ", " : "", rui_jval(rui_jat(l, i))); snprintf(ld, sizeof ld, "%s", g->str); g_string_free(g, TRUE); }
	  char *rl = UPS ? rel(UPS) : NULL;
	  KV r[] = { {"Version", g_strdup_printf("%s (%s)", rui_version(), rui_version_date())}, {UPS ? "Installed release" : NULL, rl && *rl ? g_strdup(rl) : g_strdup("—")}, {"Started from", g_strdup(st)}, {"Secure Boot", sb_summary(SBS)},
		{k ? "Running for" : NULL, g_strdup(up)}, {k ? "Load (1, 5, 15 min)" : NULL, g_strdup(ld)} }; g_free(rl); kv(r, G_N_ELEMENTS(r)); }
	h4("The desktop");
	{ GdkRectangle g = {0}; GdkMonitor *m = gdk_display_get_primary_monitor(gdk_display_get_default()); if (!m) m = gdk_display_get_monitor(gdk_display_get_default(), 0); if (m) gdk_monitor_get_geometry(m, &g);
	  const char *rn = js(s, "renderer"); KV r[] = { {"Runs in", g_strdup(rui_jbool(sv, "shellFailed") ? "the plain page: the shell did not start (fallback)" : "the native desktop (each app a window of its own)")}, {"Display", s ? disp(js(s, "display")) : g_strdup("—")},
		{"Drawn by", g_strdup(drawn(s))}, {"Pages drawn with", g_strdup_printf("%s%s%s", *rn ? "ANGLE (" : "WebKit", rn, *rn ? ")" : "")}, {*js(s, "gsp") ? "NVIDIA GSP" : NULL, g_strdup(js(s, "gsp"))},
		{"Desktop size", g_strdup_printf("%d×%d · scale %g", (int)lround(g.width / RUI.z), (int)lround(g.height / RUI.z), RUI.z)} }; kv(r, G_N_ELEMENTS(r)); }
	if (sv) { h4("Services"); RuiTable *t = rui_table_new(3); rui_table_nohead(t, 1); for (int c = 0; c < 3; c++) rui_table_col(t, c, "", RUI_LEFT, 0);
		static const char *const SV[][3] = { {"rudolfd (system service)", "rudolfd", NULL}, {"Display (rudolf-display)", "display", "display"}, {"Sound server", "sound", "sound"}, {"Wi-Fi", "wifi", "wifi"}, {"X display server", "x", NULL},
			{"Shell (real tabs)", "shell", NULL}, {"Browser window for sign-in sites", "webWindow", NULL}, {"Linux apps", "linuxApps", NULL} };
		for (unsigned i = 0; i < G_N_ELEMENTS(SV); i++) { const char *c[3] = { rui_tr(SV[i][0]), rui_tr(rui_jbool(sv, SV[i][1]) ? "Running" : "Not running"), "" };
			long id = !SV[i][2] ? 0 : !strcmp(SV[i][2], "display") ? 1 : !strcmp(SV[i][2], "sound") ? 2 : 3; int r = rui_table_add(t, c, id);
			if (SV[i][2]) { rui_table_set_button(t, r, 2, rui_tr("Restart"), svc_btn, NULL); g_object_set_data_full(G_OBJECT(W->win), SV[i][2], g_strdup(rui_tr(SV[i][0])), g_free); } }
		const char *c[3] = { rui_tr("Desktop (Chromium)"), rui_tr("Running"), "" }; int r = rui_table_add(t, c, 100); rui_table_set_button(t, r, 2, rui_tr("Restart the desktop"), svc_btn, NULL);
		rui_table_done(t); add(rui_table_widget(t)); named("services", rui_table_widget(t)); SVT = t; }
	h4("Protection and storage");
	{ RuiJ *fw = rui_get("/api/firewall"), *sec = ls_get("bd95sec"), *se = rui_get("/api/desktop/session"); RuiJ *sf = rui_jprop(sec, "fw"), *sa = rui_jprop(sec, "av");
	  int fwon = sf && rui_jprop(sf, "on") ? rui_jbool(sf, "on") : 1, avon = sa && rui_jprop(sa, "on") ? rui_jbool(sa, "on") : 1; const char *mode = sf && rui_jstr(sf, "mode") ? rui_jstr(sf, "mode") : "open";
	  RuiJ *set = rui_get("/api/desktop/settings"), *ss = rui_jprop(set, "s");
	  KV r[] = { {"File storage", g_strdup("Running (IndexedDB)")}, {"Home folder encryption", g_strdup(*js(se, "fk") ? "On (AES-256-GCM)" : !strcmp(js(se, "r"), "guest") ? "Guest session" : "Off")},
		{"Firewall", fw ? (rui_jbool(fw, "on") ? g_strdup_printf("On, %.0f blocked", rui_jnum(fw, "blocked", 0)) : g_strdup("Off")) : g_strdup("—")},
		{"Website blocking", fwon ? g_strdup_printf("On (%s)", mode) : g_strdup("Off")}, {"Antivirus", g_strdup(avon ? "On" : "Off")},
		{"Screen reader", g_strdup(rui_jbool(ss, "sr") ? "On" : "Off")}, {"Notifications", g_strdup(rui_jbool(ss, "dnd") ? "Do not disturb" : "On")}, {"Workspaces", g_strdup("3 (current 1)")} };
	  kv(r, G_N_ELEMENTS(r)); rui_jfree(fw); rui_jfree(sec); rui_jfree(se); rui_jfree(set); }
	if (k) { RuiJ *m = rui_jprop(s, "memory"); h4("Kernel");
		double tot = rui_jnum(m, "total", 0), av = rui_jnum(m, "available", 0), st = rui_jnum(m, "swapTotal", 0), sfr = rui_jnum(m, "swapFree", 0);
		char mem[200], sw[200]; snprintf(mem, sizeof mem, "%s in use of %s · cached %s", fmtB(tot - av), fmtB(tot), fmtB(rui_jnum(m, "cached", 0) + rui_jnum(m, "buffers", 0)));
		if (st > 0) { snprintf(sw, sizeof sw, "%s of %s", fmtB(st - sfr), fmtB(st)); if (rui_jnum(m, "zramStored", 0)) { size_t l = strlen(sw); snprintf(sw + l, sizeof sw - l, " · compressed in memory: %s stored in %s", fmtB(rui_jnum(m, "zramStored", 0)), fmtB(rui_jnum(m, "zramUsed", 0))); } } else snprintf(sw, sizeof sw, "none");
		KV r[] = { {"Linux", g_strdup_printf("%s (%s)", js(k, "release"), js(k, "machine"))}, {"Built", g_strdup(js(k, "version"))}, {"Start-up options", g_strdup(js(k, "cmdline"))},
			{"Processes and threads", g_strdup_printf("%g", rui_jnum(k, "procs", 0))}, {"Memory", g_strdup(mem)}, {"Swap", g_strdup(sw)} };
		RuiJ *pr = rui_jprop(k, "procs"); if (rui_jtype(pr) == 3) { g_free(r[3].v); r[3].v = g_strdup(rui_jsval(pr)); }
		kv(r, G_N_ELEMENTS(r));
		GPtrArray *mo = lines(js(k, "modules")), *ms = lines(js(k, "messages")); char t[96];
		GPtrArray *one = g_ptr_array_new_with_free_func(g_free); if (mo->len) { GString *j = g_string_new(NULL); for (guint i = 0; i < mo->len; i++) g_string_append_printf(j, "%s%s", i ? "  " : "", (char *)g_ptr_array_index(mo, i)); g_ptr_array_add(one, g_string_free(j, FALSE)); }
		snprintf(t, sizeof t, "%s (%u)", rui_tr("Drivers loaded"), mo->len); details(0, t, one, "none: built into the kernel");
		snprintf(t, sizeof t, "%s (%u)", rui_tr("Kernel warnings and errors"), ms->len); details(1, t, ms, "none");
		g_ptr_array_unref(mo); g_ptr_array_unref(ms); g_ptr_array_unref(one); }
}

/* ------------------------------------------------------------------ Hardware */
static void hwc_go(GtkButton *b, gpointer d) { (void)b; (void)d; rui_shell(W, "open do-hwcheck"); act_note("open do-hwcheck"); }
static void draw_hw(void)
{
	sys_get(); RuiJ *s = SYSJ; if (!s || rui_jstr(s, "error")) { add(rui_label(s ? js(s, "error") : "—")); return; }
	{ GtkWidget *d = row_end(); GtkWidget *b = rui_button("Check this computer…", 1); g_signal_connect(b, "clicked", G_CALLBACK(hwc_go), NULL); named("hwcheck", b);
	  RuiJ *hc = ls_get("bd95hwcheck"); char t[400]; snprintf(t, sizeof t, " %s", rui_tr("Sound, microphone, network, USB, graphics and sleep, in about a minute."));
	  if (rui_jprop(hc, "results")) { snprintf(t, sizeof t, " Last check: %s", "see the hardware check"); } rui_jfree(hc);
	  GtkWidget *m = rui_msg(t); gtk_widget_set_margin_top(m, (int)lround(rui_px(10))); gtk_box_pack_start(GTK_BOX(d), b, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(d), m, TRUE, TRUE, 0); add(d); }
	RuiJ *c = rui_jprop(s, "cpu"), *k = rui_jprop(s, "kernel"), *m = rui_jprop(s, "memory");
	h4("Computer"); { char *bu = g_ascii_strup(js(k, "boot"), -1); char *mo = g_strstrip(g_strdup(js(s, "machine"))), *fi = g_strstrip(g_strdup(js(s, "firmware")));
		KV r[] = { {"Model", g_strdup(*mo ? mo : "not reported")}, {"Firmware", g_strdup(*fi ? fi : "not reported")}, {"Started with", bu} }; g_free(mo); g_free(fi); kv(r, 3); }
	h4("Processor"); { RuiJ *t = rui_jprop(c, "tempC"); KV r[] = { {"Model", g_strdup(js(c, "model"))}, {"Threads", g_strdup_printf("%g", rui_jnum(c, "threads", 0))}, {"Speed now", rui_jnum(c, "mhz", 0) ? g_strdup_printf("%g MHz", rui_jnum(c, "mhz", 0)) : NULL},
		{"Temperature", rui_jtype(t) == 2 ? g_strdup_printf("%g °C", rui_jval(t)) : g_strdup("not reported")}, {"Memory", g_strdup(fmtB(rui_jnum(m, "total", 0)))} }; kv(r, 5); }
	h4("Graphics and display"); { GPtrArray *g = lines(js(s, "gpu")); KV *r = g_new0(KV, g->len + 1); char **keys = g_new0(char *, g->len + 1);
		for (guint i = 0; i < g->len; i++) { const char *l = g_ptr_array_index(g, i); keys[i] = g_strndup(l, strcspn(l, " ")); r[i].k = keys[i]; r[i].v = gpu(l); }
		r[g->len].k = "Display"; r[g->len].v = disp(js(s, "display")); kv(r, g->len + 1); g_strfreev(keys); g_free(r); g_ptr_array_unref(g); }
	{ RuiJ *bl = rui_jprop(s, "battery"); int nb = 0; KV r[8]; for (int i = 0; i < rui_jlen(bl) && nb < 8; i++) { RuiJ *b = rui_jat(bl, i); double cap = rui_jnum(b, "capacity", -1);
			if (g_ascii_strcasecmp(js(b, "type"), "battery") || !strcmp(js(b, "scope"), "Device") || cap < 0 || cap > 100) continue;
			r[nb].k = "Battery"; r[nb++].v = g_strdup_printf("%g%%%s%s", cap, *js(b, "status") ? ", " : "", js(b, "status")); }
	  if (nb) { h4("Battery"); kv(r, nb); } }
	h4("Storage"); { GPtrArray *g = lines(js(s, "storage")); KV *r = g_new0(KV, g->len + 1); char **keys = g_new0(char *, g->len + 1);
		for (guint i = 0; i < g->len; i++) { char d[64] = "", at[200] = ""; double sz = 0, us = 0; sscanf(g_ptr_array_index(g, i), "%63s %lf %lf %199s", d, &sz, &us, at); keys[i] = g_strdup_printf("%s (%s)", d, at); r[i].k = keys[i]; r[i].v = g_strdup_printf("%s used of %s", fmtB(us), fmtB(sz)); }
		kv(r, g->len); g_strfreev(keys); g_free(r); g_ptr_array_unref(g); }
	h4("Network adapters"); { GPtrArray *g = lines(js(s, "net")); KV *r = g_new0(KV, g->len + 1); char **keys = g_new0(char *, g->len + 1);
		for (guint i = 0; i < g->len; i++) { char n[64] = "", st[32] = "", sp[32] = "", kind[16] = "", drv[64] = ""; double rx = 0, tx = 0; sscanf(g_ptr_array_index(g, i), "%63s %31s %31s %lf %lf %15s %63s", n, st, sp, &rx, &tx, kind, drv);
			keys[i] = g_strdup_printf("%s%s", n, !strcmp(kind, "wifi") ? " (Wi-Fi)" : " (wired)"); r[i].k = keys[i];
			r[i].v = g_strdup_printf("%s%s%s%s · received %s, sent %s%s", st, strcmp(sp, "-") && atof(sp) > 0 ? ", " : "", strcmp(sp, "-") && atof(sp) > 0 ? sp : "", strcmp(sp, "-") && atof(sp) > 0 ? " Mb/s" : "", fmtB(rx), fmtB(tx),
				slow(kind, st, sp, drv) ? " · slower than this chip can go: check the cable (use one for gigabit, Cat5e or better) or the router port" : ""); }
		kv(r, g->len); g_strfreev(keys); g_free(r); g_ptr_array_unref(g); }
	h4("Sound"); { GPtrArray *g = lines(js(s, "audio")); KV *r = g_new0(KV, g->len + 1); char **keys = g_new0(char *, g->len + 1); int n = 0;
		for (guint i = 0; i < g->len; i++) { const char *l = g_ptr_array_index(g, i); if (!isdigit((unsigned char)l[0])) continue; keys[n] = g_strdup_printf("Card %.*s", (int)strcspn(l, " "), l); r[n].k = keys[n];
			const char *x = l; while (isdigit((unsigned char)*x)) x++; while (*x == ' ' || *x == '\t') x++; r[n++].v = g_strdup(x); }
		kv(r, n); g_strfreev(keys); g_free(r); g_ptr_array_unref(g); }
	{ GPtrArray *in = lines(js(s, "input")); char t[96]; snprintf(t, sizeof t, "%s (%u)", rui_tr("Input devices"), in->len); details(2, t, in, NULL); g_ptr_array_unref(in); }
}

/* ------------------------------------------------------------------ Startup */
static gboolean redraw_idle(gpointer d) { (void)d; draw(1); return G_SOURCE_REMOVE; }
static RuiJ *SETJ;
static void restore_toggled(int on, void *d)
{
	/* (the account's settings as the page keeps them, with "restore" set: the page's persistS) */
	(void)d; char k[120], p[200]; snprintf(k, sizeof k, "bd95set:%s", ACCT); char *e = g_uri_escape_string(k, NULL, TRUE); snprintf(p, sizeof p, "/api/lsync?key=%s", e); g_free(e);
	RuiJ *r = rui_get(p); const char *raw = rui_jstr(r, "v"); GString *s = g_string_new(raw && *raw == '{' ? raw : "{}");
	char *at = strstr(s->str, "\"restore\":"); if (at) { size_t i = at - s->str + 10, x = i; while (x < s->len && s->str[x] != ',' && s->str[x] != '}') x++; g_string_erase(s, i, x - i); g_string_insert(s, i, on ? "true" : "false"); }
	else { g_string_truncate(s, s->len - 1); g_string_append_printf(s, "%s\"restore\":%s}", s->len > 1 ? "," : "", on ? "true" : "false"); }
	int c = ls_set(k, s->str); act_note("restore %d %d", on, c); g_string_free(s, TRUE); rui_jfree(r); state_soon();
}
static void start_remove(long id, int which, void *d)
{
	(void)which; (void)d; char k[120]; snprintf(k, sizeof k, "bd95sess:%s", ACCT); RuiJ *o = ls_get(k), *l = ls_list(o); GString *s = g_string_new("[");
	for (int i = 0, n = 0; i < rui_jlen(l); i++) { if (i == id) continue; const char *v = rui_jsval(rui_jat(l, i)); if (!v) continue; g_string_append_printf(s, "%s\"%s\"", n++ ? "," : "", v); }
	g_string_append(s, "]"); int c = ls_set(k, s->str); act_note("sess remove %ld %d", id, c); g_string_free(s, TRUE); rui_jfree(o); g_idle_add(redraw_idle, NULL);   /* (not inside the table's own click) */
}
static void draw_start(void)
{
	char k[120]; snprintf(k, sizeof k, "bd95set:%s", ACCT); rui_jfree(SETJ); SETJ = ls_get(k); RuiJ *rs = rui_jprop(SETJ, "restore");
	GtkWidget *ck = rui_check("Reopen my apps when I sign in", !(rs && rui_jtype(rs) == 1 && !rui_jval(rs)), restore_toggled, NULL); add(ck); named("restore", ck);
	h4("Apps reopened at sign-in");
	snprintf(k, sizeof k, "bd95sess:%s", ACCT); RuiJ *o = ls_get(k), *l = ls_list(o);
	if (!rui_jlen(l)) add(rui_label("None yet."));
	else { RuiTable *t = rui_table_new(3); rui_table_nohead(t, 1); rui_table_col(t, 0, "", RUI_LEFT, 0); rui_table_col(t, 1, "", RUI_LEFT, 0); rui_table_col(t, 2, "", RUI_LEFT, 0);
		for (int i = 0; i < rui_jlen(l); i++) { const char *a = rui_jsval(rui_jat(l, i)); if (!a) continue; char ic[48], nm[120], txt[200]; app_name(a, ic, sizeof ic, nm, sizeof nm);
			snprintf(txt, sizeof txt, "%s%s%s", nm[0] ? ic : "", nm[0] ? " " : "", nm[0] ? nm : a); const char *c[3] = { txt, "", "" }; int r = rui_table_add(t, c, i); rui_table_set_button(t, r, 1, rui_tr("Remove"), start_remove, NULL); }
		rui_table_done(t); add(rui_table_widget(t)); named("startlist", rui_table_widget(t)); STT = t; }
	rui_jfree(o);
}

/* ------------------------------------------------------------------ the footer: Copy system report (tm26.js sysReport, from rudolfd), Report a problem…
   (the page's own dialog, opened by the shell), Developer Console */
static char *scrub(const char *t, GPtrArray *nets)   /* (srScrub: network and hardware addresses, e-mail addresses, the accounts' and networks' names) */
{
	GRegex *ip = g_regex_new("\\b\\d{1,3}(\\.\\d{1,3}){3}\\b", 0, 0, NULL), *mac = g_regex_new("\\b([0-9a-f]{2}:){5}[0-9a-f]{2}\\b", G_REGEX_CASELESS, 0, NULL), *em = g_regex_new("[\\w.+-]+@[\\w-]+(\\.[\\w-]+)+", 0, 0, NULL);
	char *a = g_regex_replace_literal(ip, t, -1, 0, "x.x.x.x", 0, NULL), *b = g_regex_replace_literal(mac, a, -1, 0, "xx:xx:xx:xx:xx:xx", 0, NULL), *c = g_regex_replace_literal(em, b, -1, 0, "(e-mail)", 0, NULL);
	g_free(a); g_free(b); g_regex_unref(ip); g_regex_unref(mac); g_regex_unref(em);
	RuiJ *acc = rui_get("/api/desktop/accounts"), *l = rui_jprop(acc, "accounts");
	for (int i = 0; i < rui_jlen(l); i++) { const char *n = js(rui_jat(l, i), "n"); if (strlen(n) > 2) { char **v = g_strsplit(c, n, -1); g_free(c); c = g_strjoinv("(user)", v); g_strfreev(v); } }
	rui_jfree(acc);
	for (guint i = 0; nets && i < nets->len; i++) { const char *w = g_ptr_array_index(nets, i); if (strlen(w) < 2) continue; char *e = g_regex_escape_string(w, -1), *pat = g_strdup_printf("(^|[^\\w])%s(?![\\w])", e);
		GRegex *r = g_regex_new(pat, 0, 0, NULL); if (r) { char *x = g_regex_replace(r, c, -1, 0, "\\1(network name)", 0, NULL); if (x) { g_free(c); c = x; } g_regex_unref(r); } g_free(e); g_free(pat); }
	return c;
}
static char *sys_report(void)
{
	RuiJ *sys = rui_get("/api/system"), *st = rui_get("/api/stats"), *info = rui_get("/api/info"), *pr = rui_get("/api/processes"), *up = rui_get("/api/sys/status"), *wf = rui_get("/api/wifi/status");
	RuiJ *k = rui_jprop(sys, "kernel"), *m = rui_jprop(sys, "memory"), *c = rui_jprop(sys, "cpu"), *sv = rui_jprop(sys, "services"), *sbs = rui_get("/api/secureboot/status");
	GPtrArray *nets = g_ptr_array_new_with_free_func(g_free); { const char *raw = js(wf, "raw"); char **v = g_strsplit(raw, "\n", -1);
		for (int i = 0; v[i]; i++) { char *x = g_strstrip(v[i]); if (!g_ascii_strncasecmp(x, "ssid=", 5) || !g_ascii_strncasecmp(x, "ssid:", 5)) g_ptr_array_add(nets, g_strdup(g_strstrip(x + 5)));
			if (!strncmp(x, "saved=", 6)) { char *tb = strchr(x, '\t'); if (tb) g_ptr_array_add(nets, g_strndup(tb + 1, strcspn(tb + 1, "\t"))); } } g_strfreev(v);
		const char *ss = rui_jstr(wf, "ssid"); if (ss) g_ptr_array_add(nets, g_strdup(ss)); }
	GString *o = g_string_new("Rudolf OS system report\n\n== Status\n");
	char *rl = up ? rel(up) : g_strdup(""); g_string_append_printf(o, "Rudolf OS: %s (%s)%s%s\n", rui_version(), rui_version_date(), *rl ? " · release " : "", rl); g_free(rl);
	{ time_t t = time(NULL); struct tm g; gmtime_r(&t, &g); char b[32]; strftime(b, sizeof b, "%Y-%m-%dT%H:%M:%SZ", &g); g_string_append_printf(o, "Created: %s\n", b); }
	{ char *bu = g_ascii_strup(*js(k, "boot") ? js(k, "boot") : *js(info, "boot") ? js(info, "boot") : "?", -1); int live = rui_jprop(k, "live") ? rui_jbool(k, "live") : rui_jbool(info, "live");
	  g_string_append_printf(o, "Started from: %s, %s%s%s\n", bu, *js(k, "media") ? js(k, "media") : *js(info, "media") ? js(info, "media") : "?", live ? ", live (changes are not kept)" : ", changes kept (RUDOLFDATA)", rui_jbool(k, "recovery") ? ", recovery" : ""); g_free(bu); }
	{ char *sb = sb_summary(sbs); g_string_append_printf(o, "Secure Boot: %s\n", sb); g_free(sb); }
	{ RuiJ *v = rui_get("/api/vault"); GString *e = g_string_new(v ? js(v, "state") : "not reported"); RuiJ *mv = rui_jprop(v, "moved"); if (mv) { GString *l = g_string_new(NULL); for (int i = 0; i < rui_jlen(mv); i++) if (rui_jval(rui_jat(mv, i))) g_string_append_printf(l, "%s%s", l->len ? ", " : "", rui_jkey(mv, i)); g_string_append_printf(e, " (%s encrypted)", l->str); g_string_free(l, TRUE); }
	  g_string_append_printf(o, "Data encryption: %s\n", e->str); g_string_free(e, TRUE); rui_jfree(v); }
	{ RuiJ *f = rui_get("/api/firewall"); if (f) g_string_append_printf(o, "Firewall: %s\n", rui_jbool(f, "on") ? g_strdup_printf("on, %.0f incoming connections blocked%s", rui_jnum(f, "blocked", 0), rui_jbool(f, "remote") ? ", remote access open to the local network" : "") : "off"); else g_string_append(o, "Firewall: not reported\n"); rui_jfree(f); }
	{ RuiJ *d = rui_get("/api/devlink"); g_string_append_printf(o, "Development link: %s\n", !d || !rui_jbool(d, "available") ? "not available" : !rui_jbool(d, "on") ? "off" : "on"); rui_jfree(d); }
	g_string_append(o, "Desktop: native (rudolf-native, each app a window of its own)\n");
	{ char *dp = sys ? disp(js(sys, "display")) : g_strdup("—"); g_string_append_printf(o, "Display: %s\nDrawn by: %s\n", dp, drawn(sys)); g_free(dp); }
	g_string_append_printf(o, "Pages drawn with: %s%s%s\n", *js(sys, "renderer") ? "ANGLE (" : "WebKit", js(sys, "renderer"), *js(sys, "renderer") ? ")" : "");
	if (*js(sys, "gsp")) { char *g = g_strdup(js(sys, "gsp")); for (char *x = g; *x; x++) if (*x == '\n') *x = '/'; g_string_append_printf(o, "NVIDIA GSP: %s\n", g); g_free(g); }
	{ GdkRectangle g = {0}; GdkMonitor *mo = gdk_display_get_monitor(gdk_display_get_default(), 0); if (mo) gdk_monitor_get_geometry(mo, &g); g_string_append_printf(o, "Desktop size: %dx%d @%g\n", g.width, g.height, 1.0); }
	{ char u[64]; if (rui_jprop(k, "uptime")) dur((long)rui_jnum(k, "uptime", 0), u, sizeof u); else snprintf(u, sizeof u, "—"); g_string_append_printf(o, "Uptime: %s\n", u); }
	if (st) { GString *l = g_string_new(NULL); RuiJ *ld = rui_jprop(k, "load"); for (int i = 0; i < rui_jlen(ld); i++) g_string_append_printf(l, "%s%g", i ? " " : "", rui_jval(rui_jat(ld, i))); if (!l->len) g_string_append_printf(l, "%g", rui_jnum(st, "load", 0));
		g_string_append_printf(o, "CPU: %.0f%% now, load %s\n", rui_jnum(st, "cpu", 0), l->str); g_string_free(l, TRUE);
		double mt = rui_jnum(st, "memTotal", 0); g_string_append_printf(o, "Memory: %s used of %s", fmtB(mt - rui_jnum(st, "memAvailable", 0)), fmtB(mt)); if (rui_jnum(m, "swapTotal", 0)) g_string_append_printf(o, ", swap %s used", fmtB(rui_jnum(m, "swapTotal", 0) - rui_jnum(m, "swapFree", 0))); g_string_append_c(o, '\n');
		RuiJ *t = rui_jprop(st, "tempC"); if (rui_jtype(t) == 2) g_string_append_printf(o, "Temperature: %g C\n", rui_jval(t)); else g_string_append(o, "Temperature: not reported\n");
		RuiJ *bl = rui_jprop(st, "battery"); GString *b = g_string_new(NULL); for (int i = 0; i < rui_jlen(bl); i++) { RuiJ *x = rui_jat(bl, i); double cap = rui_jnum(x, "capacity", -1); if (g_ascii_strcasecmp(js(x, "type"), "battery") || !strcmp(js(x, "scope"), "Device") || cap < 0 || cap > 100) continue; g_string_append_printf(b, "%s%g%% %s", b->len ? ", " : "", cap, js(x, "status")); }
		g_string_append_printf(o, "Battery: %s\n", b->len ? b->str : "none"); g_string_free(b, TRUE); }
	else g_string_append(o, "CPU: —\nMemory: —\nTemperature: not reported\nBattery: none\n");
	g_string_append_printf(o, "Storage: %s\n", rui_jnum(info, "persistTotal", 0) > 0 ? g_strdup_printf("%s free of %s", fmtB(rui_jnum(info, "persistFree", 0)), fmtB(rui_jnum(info, "persistTotal", 0))) : "live (not installed)");
	{ RuiJ *n = rui_get("/api/net"), *l = rui_jprop(n, "interfaces"); int upn = 0; for (int i = 0; i < rui_jlen(l); i++) { RuiJ *x = rui_jat(l, i); if (!strcmp(js(x, "state"), "up") && strcmp(js(x, "name"), "lo") && !rui_jbool(x, "virtual")) upn = 1; } g_string_append_printf(o, "Network: %s\n", upn ? "connected" : "not connected"); rui_jfree(n); }
	if (sys) { const char *y[] = { "not running", "running" }; g_string_append_printf(o, "Services: display %s, sound %s, wifi %s, X %s, linux apps %s\n", y[rui_jbool(sv, "display")], y[rui_jbool(sv, "sound")], y[rui_jbool(sv, "wifi")], y[rui_jbool(sv, "x")], y[rui_jbool(sv, "linuxApps")]); }
	else g_string_append(o, "Services: —\n");
	g_string_append_printf(o, "Windows: %d\n", WINSJ ? rui_jlen(wins_list()) : 1);
	{ GPtrArray *ms = lines(js(k, "messages")); if (sys) g_string_append_printf(o, "Kernel warnings: %u\n", ms->len); else g_string_append(o, "Kernel warnings: —\n"); g_ptr_array_unref(ms); }
	g_string_append(o, "Hardware check: not run\n\n");
	if (sys) {
		char *mo = g_strstrip(g_strdup(js(sys, "machine"))), *fi = g_strstrip(g_strdup(js(sys, "firmware"))); g_string_append_printf(o, "== Computer\nMachine: %s\nFirmware: %s\nProcessor: %s, %g threads%s\n", *mo ? mo : "?", *fi ? fi : "?", js(c, "model"), rui_jnum(c, "threads", 0), rui_jnum(c, "mhz", 0) ? g_strdup_printf(", %g MHz", rui_jnum(c, "mhz", 0)) : ""); g_free(mo); g_free(fi);
		GPtrArray *g = lines(js(sys, "gpu")); for (guint i = 0; i < g->len; i++) { char *x = gpu(g_ptr_array_index(g, i)); g_string_append_printf(o, "Graphics: %s\n", x); g_free(x); } g_ptr_array_unref(g);
		g = lines(js(sys, "audio")); for (guint i = 0; i < g->len; i++) { const char *l = g_ptr_array_index(g, i); if (!isdigit((unsigned char)*l)) continue; while (isdigit((unsigned char)*l)) l++; while (*l == ' ' || *l == '\t') l++; g_string_append_printf(o, "Sound: %s\n", l); } g_ptr_array_unref(g);
		g = lines(js(sys, "storage")); for (guint i = 0; i < g->len; i++) { char d[64] = "", at[200] = ""; double sz = 0, us = 0; sscanf(g_ptr_array_index(g, i), "%63s %lf %lf %199s", d, &sz, &us, at); g_string_append_printf(o, "Storage: %s %s %s used of %s\n", d, at, fmtB(us), fmtB(sz)); } g_ptr_array_unref(g);
		g = lines(js(sys, "net")); for (guint i = 0; i < g->len; i++) { char n[64] = "", s1[32] = "", sp[32] = "", kind[16] = "", drv[64] = ""; double rx, tx; sscanf(g_ptr_array_index(g, i), "%63s %31s %31s %lf %lf %15s %63s", n, s1, sp, &rx, &tx, kind, drv);
			g_string_append_printf(o, "Network: %s %s %s%s%s%s%s%s%s\n", n, kind, s1, strcmp(sp, "-") && atof(sp) > 0 ? " " : "", strcmp(sp, "-") && atof(sp) > 0 ? sp : "", strcmp(sp, "-") && atof(sp) > 0 ? " Mb/s" : "", drv[0] ? " (" : "", drv, drv[0] ? ")" : ""); } g_ptr_array_unref(g);
		g = lines(js(sys, "input")); GString *in = g_string_new(NULL); for (guint i = 0; i < g->len; i++) g_string_append_printf(in, "%s%s", i ? "; " : "", (char *)g_ptr_array_index(g, i)); g_string_append_printf(o, "Input: %s\n\n", in->str); g_string_free(in, TRUE); g_ptr_array_unref(g);
		GPtrArray *mo2 = lines(js(k, "modules")); g_string_append_printf(o, "== Kernel\nLinux: %s %s\nOptions: %s\nDrivers: %u\nMemory: total %s, available %s, cached %s%s\n\n", js(k, "release"), js(k, "machine"), js(k, "cmdline"), mo2->len, fmtB(rui_jnum(m, "total", 0)), fmtB(rui_jnum(m, "available", 0)), fmtB(rui_jnum(m, "cached", 0)),
			rui_jnum(m, "zramStored", 0) ? g_strdup_printf(", compressed swap %s in %s", fmtB(rui_jnum(m, "zramStored", 0)), fmtB(rui_jnum(m, "zramUsed", 0))) : ""); g_ptr_array_unref(mo2); }
	RuiJ *it = rui_jprop(pr, "items");
	if (it) { int n = rui_jlen(it); g_string_append_printf(o, "== Processes (%d), most memory first\n", n); int *ord = g_new(int, n); for (int i = 0; i < n; i++) ord[i] = i;
		for (int i = 0; i < n && i < 10; i++) for (int j = i + 1; j < n; j++) if (rui_jnum(rui_jat(it, ord[j]), "rss", 0) > rui_jnum(rui_jat(it, ord[i]), "rss", 0)) { int x = ord[i]; ord[i] = ord[j]; ord[j] = x; }
		for (int i = 0; i < n && i < 10; i++) { RuiJ *p = rui_jat(it, ord[i]); RuiJ *cp = rui_jprop(p, "cpu"); g_string_append_printf(o, "%s [%s] %s%s\n", js(p, "name"), js(p, "tag"), fmtB(rui_jnum(p, "rss", 0)), rui_jtype(cp) == 2 ? g_strdup_printf(" %.1f%%", rui_jval(cp)) : ""); }
		GString *busy = g_string_new(NULL); for (int i = 0; i < n; i++) ord[i] = i; for (int i = 0; i < n && i < 5; i++) for (int j = i + 1; j < n; j++) if (rui_jnum(rui_jat(it, ord[j]), "cpu", 0) > rui_jnum(rui_jat(it, ord[i]), "cpu", 0)) { int x = ord[i]; ord[i] = ord[j]; ord[j] = x; }
		for (int i = 0; i < n && i < 5; i++) { RuiJ *p = rui_jat(it, ord[i]); if (rui_jnum(p, "cpu", 0) > 5) g_string_append_printf(busy, "%s%s %.0f%%", busy->len ? ", " : "", js(p, "name"), rui_jnum(p, "cpu", 0)); }
		if (busy->len) g_string_append_printf(o, "Busiest: %s\n", busy->str); g_string_free(busy, TRUE); g_free(ord); g_string_append_c(o, '\n'); }
	{ RuiJ *cr = rui_get("/api/crashes"), *l = rui_jprop(cr, "crashes"); if (l) { g_string_append(o, "== Crashes (Rudolf OS programs since start-up)\n"); if (!rui_jlen(l)) g_string_append(o, "none\n");
			for (int i = 0; i < rui_jlen(l); i++) { RuiJ *x = rui_jat(l, i); g_string_append_printf(o, "%s: %s in %s (%.0f min after start-up)\n", js(x, "program"), js(x, "kind"), *js(x, "where") ? js(x, "where") : "?", round(rui_jnum(x, "at", 0) / 60)); } g_string_append_c(o, '\n'); } rui_jfree(cr); }
	{ GPtrArray *ms = lines(js(k, "messages")); if (sys && ms->len) { g_string_append(o, "== Kernel warnings and errors (latest)\n"); for (guint i = ms->len > 20 ? ms->len - 20 : 0; i < ms->len; i++) g_string_append_printf(o, "%s\n", (char *)g_ptr_array_index(ms, i)); g_string_append_c(o, '\n'); } g_ptr_array_unref(ms); }
	{ GPtrArray *ss = lines(js(sys, "session")); if (sys && ss->len) { g_string_append(o, "== Session log (latest)\n"); for (guint i = ss->len > 15 ? ss->len - 15 : 0; i < ss->len; i++) g_string_append_printf(o, "%s\n", (char *)g_ptr_array_index(ss, i)); g_string_append_c(o, '\n'); } g_ptr_array_unref(ss); }
	{ RuiJ *cl = rui_get("/api/clientlog"); GPtrArray *ls = lines(js(cl, "log")); if (ls->len) { g_string_append(o, "== Desktop errors (latest)\n"); for (guint i = ls->len > 15 ? ls->len - 15 : 0; i < ls->len; i++) g_string_append_printf(o, "%s\n", (char *)g_ptr_array_index(ls, i)); g_string_append_c(o, '\n'); } g_ptr_array_unref(ls); rui_jfree(cl); }
	char *r = scrub(o->str, nets); g_string_free(o, TRUE); g_ptr_array_unref(nets);
	rui_jfree(sys); rui_jfree(st); rui_jfree(info); rui_jfree(pr); rui_jfree(up); rui_jfree(wf); rui_jfree(sbs); return r;
}
static void copy_report(GtkButton *b, gpointer d)
{
	(void)b; (void)d; char *t = sys_report(); GtkClipboard *c = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD); gtk_clipboard_set_text(c, t, -1);
	rui_notify("System report copied", "Paste it wherever you need it.", 0); act_note("copied %zu", strlen(t));
	const char *f = getenv("RUDOLF_TM_STATE"); if (f) { char *p = g_strdup_printf("%s.report", f); g_file_set_contents(p, t, -1, NULL); g_free(p); }
	g_free(t); state_soon();
}
static void report_problem(GtkButton *b, gpointer d) { (void)b; (void)d; rui_shell(W, "open report"); act_note("open report"); state_soon(); }
static void dev_console(GtkButton *b, gpointer d) { (void)b; (void)d; rui_shell(W, "open devcon"); act_note("open devcon"); state_soon(); }

/* ------------------------------------------------------------------ the window */
static void status(void)
{
	char s[300]; int nw = WINSJ ? rui_jlen(wins_list()) : 1; RuiJ *it = rui_jprop(PROCS, "items");
	if (PROCS) snprintf(s, sizeof s, "%d %s · %d %s · %s", nw, rui_tr("window(s)"), rui_jlen(it), rui_tr("processes"), rui_tr("Rudolf OS on this computer"));
	else snprintf(s, sizeof s, "%d %s · %s", nw, rui_tr("window(s)"), rui_tr("Rudolf OS on this computer"));
	gtk_label_set_text(GTK_LABEL(ST), s);
}
static void draw(int full)
{
	rui_tabs_set(TABS, TAB);
	if (TAB == T_PROCS && !full && PT) { procs_table(); status(); state_soon(); return; }   /* (the list keeps its scroll and the search box its focus) */
	if ((TAB == T_SYS || TAB == T_HW) && !full) { status(); return; }
	GtkAdjustment *a = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(g_object_get_data(G_OBJECT(SCR), "rui-sw"))); double y = full ? 0 : gtk_adjustment_get_value(a);
	clear(BOX); PT = NULL; PBAR = NULL; SVT = NULL; STT = NULL; if (NAMED) g_hash_table_remove_all(NAMED);
	switch (TAB) { case T_APPS: draw_apps(); break; case T_PROCS: draw_procs(1); break; case T_PERF: draw_perf(); break; case T_SYS: draw_system(); break; case T_HW: draw_hw(); break; default: draw_start(); }
	gtk_widget_show_all(BOX); status(); if (!full) gtk_adjustment_set_value(a, y); state_soon();
}
static void tab_cb(int i, void *d) { (void)d; TAB = i; SEL = -1; act_note("tab %d", i); draw(1); if (i == T_APPS || i == T_PROCS || i == T_PERF) sample(); }
static gboolean on_delete(GtkWidget *w, GdkEvent *e, gpointer d)
{
	(void)w; (void)e; (void)d;
	/* (tmtop57.js: Task Manager's share of the processor given back when it closes) */
	RuiJ *p = rui_get("/api/desktop/priority"); if (rui_jbool(p, "on")) rui_post("/api/desktop/priority?on=0", NULL, NULL, 0); rui_jfree(p);
	gtk_main_quit(); return FALSE;
}
static void restyle(void *d) { (void)d; draw(1); }
static gboolean key(GtkWidget *w, GdkEventKey *e, gpointer d) { (void)w; (void)d; if (e->keyval == GDK_KEY_Escape && rui_menu_window()) return FALSE; return FALSE; }
int main(int argc, char **argv)
{
	for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "--urgent")) URGENT = 1;
	rui_init(&argc, &argv, "taskmgr");
	{ RuiJ *s = rui_get("/api/desktop/settings"); snprintf(ACCT, sizeof ACCT, "%s", js(s, "acct")); rui_jfree(s); }
	CAT = rui_get("/api/desktop/apps");
	const char *tab = getenv("RUDOLF_TM_TAB"); if (tab) TAB = atoi(tab) % NT;
	/* the page's window: 860 x 580 with its title bar (34 px): its body 860 x 546; the native frame adds the title bar and edge */
	W = rui_window("taskmgr", "Task Manager", 860, 546);
	TABS = rui_tabs(TABN, NT, tab_cb, NULL); gtk_box_pack_start(GTK_BOX(W->body), TABS, FALSE, FALSE, 0);
	BOX = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); int pad = (int)lround(rui_px(8)); gtk_widget_set_margin_start(BOX, pad); gtk_widget_set_margin_end(BOX, pad); gtk_widget_set_margin_top(BOX, pad); gtk_widget_set_margin_bottom(BOX, pad);
	SCR = rui_scroller(BOX); gtk_box_pack_start(GTK_BOX(W->body), SCR, TRUE, TRUE, 0);
	{ GList *c = gtk_container_get_children(GTK_CONTAINER(SCR)); g_object_set_data(G_OBJECT(SCR), "rui-sw", c->data);
	  g_signal_connect_swapped(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(c->data)), "value-changed", G_CALLBACK(state_soon), NULL); g_list_free(c); }   /* (the tests' places follow the scrolling) */
	FOOT = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (int)lround(rui_px(8))); gtk_style_context_add_class(gtk_widget_get_style_context(FOOT), "rui-foot");
	ST = rui_msg(""); gtk_label_set_line_wrap(GTK_LABEL(ST), FALSE); gtk_label_set_ellipsize(GTK_LABEL(ST), PANGO_ELLIPSIZE_END); gtk_widget_set_margin_top(ST, (int)lround(rui_px(10))); gtk_widget_set_hexpand(ST, TRUE);
	GtkWidget *rb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0), *cp = rui_button("Copy system report", 0), *rp = rui_button("Report a problem…", 1), *dc = rui_button("🛠️ Developer Console", 0);
	gtk_widget_set_margin_start(rp, (int)lround(rui_px(4)));   /* (the page's space between the first two; the third right after the second) */
	g_signal_connect(cp, "clicked", G_CALLBACK(copy_report), NULL); g_signal_connect(rp, "clicked", G_CALLBACK(report_problem), NULL); g_signal_connect(dc, "clicked", G_CALLBACK(dev_console), NULL);
	gtk_box_pack_start(GTK_BOX(rb), cp, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(rb), rp, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(rb), dc, FALSE, FALSE, 0); gtk_widget_set_valign(rb, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(FOOT), ST, TRUE, TRUE, 0); gtk_box_pack_end(GTK_BOX(FOOT), rb, FALSE, FALSE, 0); gtk_widget_set_margin_top(FOOT, (int)lround(rui_px(14))); gtk_box_pack_start(GTK_BOX(W->body), FOOT, FALSE, FALSE, 0);
	FIXED = g_hash_table_new(g_str_hash, g_str_equal); g_hash_table_insert(FIXED, "copy", cp); g_hash_table_insert(FIXED, "report", rp); g_hash_table_insert(FIXED, "devcon", dc);
	g_signal_connect(W->win, "delete-event", G_CALLBACK(on_delete), NULL); g_signal_connect(W->win, "key-press-event", G_CALLBACK(key), NULL);
	gtk_widget_show_all(W->win); gtk_window_set_focus(GTK_WINDOW(W->win), NULL);
	if (URGENT) { gtk_window_set_keep_above(GTK_WINDOW(W->win), TRUE); gtk_window_present(GTK_WINDOW(W->win)); rui_post("/api/desktop/priority?on=1", NULL, NULL, 0); }
	g_object_set_data(G_OBJECT(cp), "keep", NULL); g_object_set_data(G_OBJECT(rp), "keep", NULL); g_object_set_data(G_OBJECT(dc), "keep", NULL);
	rui_on_settings(restyle, NULL);
	draw(1); sample(); g_timeout_add_seconds(2, tick, NULL);
	gtk_main(); return 0;
}
