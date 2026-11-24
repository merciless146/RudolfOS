/*
 * rudolf-settings: Multimedia Settings (base_v24.html mmPanelS: the plugins, the folders and players, Flash (Ruffle), Game Boy / Game
 * Boy Advance, ZIP archives). 1.6.3, docs/plan-1.6.3.md Part 3. As the page: a working copy (MMW) of the multimedia settings (MMC) until
 * Apply, which checks the runtime address (https:// or empty), keeps the plugins only an administrator may change, saves bd95mm as
 * mmSave does and writes "installed plugin"/"removed plugin" to the security log as mmApplied does; Revert; Reset to defaults puts back
 * the preferences (MMD.set) to be applied. The folders open in File Explorer and the players open through the native shell ("open
 * explorer PATH", "open gameboy", "open flash"); Check Ruffle download asks the runtime's address itself (the page loads it in a frame).
 * GPL-2.0-or-later.
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

#define NP ((int)G_N_ELEMENTS(MMPLUG))
#define NS ((int)G_N_ELEMENTS(MMSET))
typedef struct { int inst[4], en[4]; char *set[32]; } Mm;
static Mm W_, C_; static int KEEP;
static int si(const char *k) { for (int i = 0; i < NS; i++) if (!strcmp(MMSET[i][0], k)) return i; return -1; }
static char *jv(RuiJ *v)
{
	int t = rui_jtype(v); char b[64];
	if (t == 1) return g_strdup(rui_jval(v) ? "true" : "false");
	if (t == 2) { g_ascii_formatd(b, sizeof b, "%.15g", rui_jval(v)); return g_strdup(b); }
	if (t == 3) { char *e = set_jesc(rui_jsval(v)), *r = g_strdup_printf("\"%s\"", e); g_free(e); return r; }
	return NULL;
}
static int jtype_of(const char *j) { return *j == '"' ? 3 : (!strcmp(j, "true") || !strcmp(j, "false")) ? 1 : 2; }
static void mm_free(Mm *m) { for (int i = 0; i < NS; i++) { g_free(m->set[i]); m->set[i] = NULL; } }
static void mm_copy(Mm *d, const Mm *s) { mm_free(d); memcpy(d->inst, s->inst, sizeof d->inst); memcpy(d->en, s->en, sizeof d->en); for (int i = 0; i < NS; i++) d->set[i] = g_strdup(s->set[i]); }
/* (mmLoadCfg: the defaults, the saved plugins and each saved preference of the default's type) */
static void load(void)
{
	RuiJ *o = set_ls_get("bd95mm"), *v = rui_jprop(o, "x"), *pl = rui_jprop(v, "plug"), *st = rui_jprop(v, "set"); mm_free(&C_);
	for (int i = 0; i < NP; i++) { RuiJ *p = rui_jprop(pl, MMPLUG[i][0]); C_.inst[i] = p ? rui_jbool(p, "inst") : 0; C_.en[i] = p ? C_.inst[i] && rui_jbool(p, "en") : 0; }
	for (int i = 0; i < NS; i++) { RuiJ *x = rui_jprop(st, MMSET[i][0]); char *j = x ? jv(x) : NULL; if (j && rui_jtype(x) == jtype_of(MMSET[i][1])) C_.set[i] = j; else { g_free(j); C_.set[i] = g_strdup(MMSET[i][1]); } }
	rui_jfree(o); mm_copy(&W_, &C_);
}
static char *mm_json(const Mm *m)
{
	GString *b = g_string_new("{\"plug\":{"); for (int i = 0; i < NP; i++) g_string_append_printf(b, "%s\"%s\":{\"inst\":%s,\"en\":%s}", i ? "," : "", MMPLUG[i][0], m->inst[i] ? "true" : "false", m->en[i] ? "true" : "false");
	g_string_append(b, "},\"set\":{"); for (int i = 0; i < NS; i++) g_string_append_printf(b, "%s\"%s\":%s", i ? "," : "", MMSET[i][0], m->set[i]); g_string_append(b, "}}"); return g_string_free(b, FALSE);
}
static int dirty(void) { char *a = mm_json(&W_), *b = mm_json(&C_); int d = strcmp(a, b) != 0; g_free(a); g_free(b); return d; }
static char *sget(const char *k) { int i = si(k); const char *j = i < 0 ? "\"\"" : W_.set[i]; if (*j != '"') return g_strdup(j); char *w = g_strdup_printf("{\"x\":%s}", j); RuiJ *o = rui_jparse(w); g_free(w); char *r = g_strdup(set_jstr(o, "x", "")); rui_jfree(o); return r; }
static void sput(const char *k, const char *v)
{
	int i = si(k); if (i < 0) return; g_free(W_.set[i]); int t = jtype_of(MMSET[i][1]);
	if (t == 3) { char *e = set_jesc(v); W_.set[i] = g_strdup_printf("\"%s\"", e); g_free(e); } else if (t == 2) { char b[40]; g_ascii_formatd(b, sizeof b, "%.15g", g_ascii_strtod(v, NULL)); W_.set[i] = g_strdup(b); } else W_.set[i] = g_strdup(v);
	set_note("mm %s %s", k, W_.set[i]); p_footer_update(); set_state_soon();
}

/* ================================================================== the plugins (.mmplug: its name and blurb, its state, Installed and Enabled) */
typedef struct { int i; GtkWidget *a, *ci, *ce; double c1, sw, cw, h; } Plug;
static Plug PL[4]; static GtkWidget *MMM; static int ADM;
static const char *pstat(int i) { return !C_.inst[i] ? "Not installed" : C_.en[i] ? "Installed — enabled" : "Installed — disabled"; }
static double plug_lay(void *d, double x, double y, double w)
{
	Plug *p = d; double sw; s_lines(rui_tr(pstat(p->i)), 12, 400, 0, &sw); double cw = fmax(p_nat_w(p->ci), p_nat_w(p->ce)); p->sw = sw; p->cw = cw;
	p->c1 = fmax(0, w - 22 - sw - cw - 20); double th = s_lh(13, 700) + 2 + s_lines(rui_tr(MMPLUG[p->i][2]), 12, 400, p->c1, NULL) * s_lh(12, 400);
	double ih = fmax(th, fmax(s_lh(12, 400), 53)); p->h = ih + 18; p_place(p->a, x, y, w, p->h);
	double cx = x + 11 + p->c1 + 10 + sw + 10, cy = y + 9 + (ih - 53) / 2; p_place(p->ci, cx, cy, 0, 0); p_place(p->ce, cx, cy + 34, 0, 0); return p->h;
}
static gboolean plug_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Plug *p = d; double z = s_z(), W = gtk_widget_get_allocated_width(w) / z, ih = p->h - 18;
	cairo_save(cr); cairo_scale(cr, z, z); s_col(cr, "bg2", 1); s_rrect(cr, 0, 0, W, p->h, 8); cairo_fill(cr); s_col(cr, "bd", 1); cairo_set_line_width(cr, 1); s_rrect(cr, .5, .5, W - 1, p->h - 1, 7.5); cairo_stroke(cr); cairo_restore(cr);
	double th = s_lh(13, 700) + 2 + s_lines(rui_tr(MMPLUG[p->i][2]), 12, 400, p->c1, NULL) * s_lh(12, 400), ty = 9 + (ih - th) / 2;
	s_col(cr, "fg", 1); s_text(cr, rui_tr(MMPLUG[p->i][1]), 11, ty, 13, 700, 0, 0);
	cairo_set_source_rgb(cr, 107 / 255.0, 114 / 255.0, 128 / 255.0); s_text(cr, rui_tr(MMPLUG[p->i][2]), 11, ty + s_lh(13, 700) + 2, 12, 400, p->c1, 0);
	if (RUI.dark) s_col(cr, "mut", 1); else cairo_set_source_rgb(cr, 0x37 / 255.0, 0x41 / 255.0, 0x51 / 255.0); s_text(cr, rui_tr(pstat(p->i)), 11 + p->c1 + 10, 9 + (ih - s_lh(12, 400)) / 2, 12, 400, 0, 0);
	return TRUE;
}
static void plug_sync(void) { for (int i = 0; i < NP; i++) { rui_check_set(PL[i].ci, W_.inst[i]); rui_check_set(PL[i].ce, W_.en[i] && W_.inst[i]); gtk_widget_set_sensitive(PL[i].ci, ADM); gtk_widget_set_sensitive(PL[i].ce, ADM && W_.inst[i]); gtk_widget_queue_draw(PL[i].a); } }
static void on_inst(int on, void *d) { int i = GPOINTER_TO_INT(d); if (!ADM) return; W_.inst[i] = on; W_.en[i] = on; set_note("mm plug %s inst %d", MMPLUG[i][0], on); plug_sync(); p_footer_update(); }
static void on_en(int on, void *d) { int i = GPOINTER_TO_INT(d); if (!ADM) return; W_.en[i] = on && W_.inst[i]; set_note("mm plug %s en %d", MMPLUG[i][0], W_.en[i]); plug_sync(); p_footer_update(); }
static double zero_lay(void *d, double x, double y, double w) { (void)d; (void)x; (void)y; (void)w; return 0; }

/* ================================================================== the folders and players (.mmrow), the messages (#mmm) */
static GtkWidget *FB[4];
static double row_lay(void *d, double x, double y, double w) { (void)d; double bx = 0, by = 0, rh = 0; for (int i = 0; i < 4; i++) { double bw = p_nat_w(FB[i]), bh = p_nat_h(FB[i]); if (bx > 0 && bx + bw > w) { bx = 0; by += rh + 6; rh = 0; } p_place(FB[i], x + bx, y + by, 0, 0); bx += bw + 6; rh = fmax(rh, bh); } return by + rh; }
static void open_ask(GtkButton *b, gpointer d) { (void)b; rui_shell(SW, d); set_note("%s", (const char *)d); }
static void say(const char *t, int ok) { char *e = g_markup_escape_text(rui_tr(t), -1), *m = g_strdup_printf("\x03<span foreground=\"%s\">%s</span>", ok ? "#15803d" : "#b91c1c", e); p_text_set(MMM, *t ? m : ""); g_free(e); g_free(m); set_note("mmm %s", t); set_state_soon(); }
/* Check Ruffle download: the runtime's address asked (the page's rufLoad opens it in a frame; RUFURLS when no address is set) */
typedef struct { char *u; int ok; char err[200]; } Chk;
static gboolean chk_done(gpointer d) { Chk *c = d; if (MMM) { if (c->ok) say("Ruffle downloaded and ready.", 1); else say(*c->err ? c->err : "Could not download Ruffle.", 0); } g_free(c->u); g_free(c); return G_SOURCE_REMOVE; }
static int https_get(const char *url, char *err, size_t en)
{
	if (getenv("RUDOLF_SET_RUFFLE")) { snprintf(err, en, "%s", getenv("RUDOLF_SET_RUFFLE")); return !strcmp(err, "ok"); }   /* (the tests: no internet) */
	char *host = NULL, *path = NULL; const char *h = url + 8, *sl = strchr(h, '/'); host = sl ? g_strndup(h, sl - h) : g_strdup(h); path = g_strdup(sl ? sl : "/");
	GSocketClient *sc = g_socket_client_new(); g_socket_client_set_tls(sc, TRUE); g_socket_client_set_timeout(sc, 20); GError *e = NULL; int ok = 0;
	GSocketConnection *cn = g_socket_client_connect_to_host(sc, host, 443, NULL, &e);
	if (cn) { char *rq = g_strdup_printf("GET %s/ruffle.js HTTP/1.1\r\nHost: %s\r\nUser-Agent: Rudolf OS Settings\r\nConnection: close\r\n\r\n", strcmp(path, "/") ? path : "", host);
		GOutputStream *os = g_io_stream_get_output_stream(G_IO_STREAM(cn)); GInputStream *is = g_io_stream_get_input_stream(G_IO_STREAM(cn)); char b[64] = ""; gssize n = 0;
		if (g_output_stream_write_all(os, rq, strlen(rq), NULL, NULL, &e) && (n = g_input_stream_read(is, b, sizeof b - 1, NULL, &e)) > 12) { b[n] = 0; int st = atoi(b + 9); ok = st >= 200 && st < 400; if (!ok) snprintf(err, en, "The Ruffle download answered %d.", st); }
		g_free(rq); g_object_unref(cn); }
	if (e) { snprintf(err, en, "%s", e->message); g_error_free(e); }
	g_object_unref(sc); g_free(host); g_free(path); return ok;
}
static gpointer chk_thread(gpointer d)
{
	Chk *c = d;
	if (*c->u) c->ok = https_get(c->u, c->err, sizeof c->err);
	else { static const char *const U[2] = { "https://unpkg.com/@ruffle-rs/ruffle", "https://cdn.jsdelivr.net/npm/@ruffle-rs/ruffle" }; for (int i = 0; i < 2 && !c->ok; i++) c->ok = https_get(U[i], c->err, sizeof c->err); }
	g_idle_add(chk_done, c); return NULL;
}
static void on_chk(GtkButton *b, gpointer d) { (void)b; (void)d; say("Contacting the Ruffle download…", 1); Chk *c = g_new0(Chk, 1); char *u = sget("rufUrl"); c->u = g_strdup(g_strstrip(u)); g_free(u); g_thread_unref(g_thread_new("ruffle", chk_thread, c)); }

/* ================================================================== the preferences ([data-k]) */
static GtkWidget *ENTRY, *SEL[16], *CK[16], *RNG; static int NSEL, NCK;
static void on_sel(const char *v, void *d) { sput(d, v); }
static void on_ck(int on, void *d) { sput(d, on ? "true" : "false"); }
static void on_rng(double v, void *d) { char b[16]; snprintf(b, sizeof b, "%g", v); sput(d, b); }
static void on_entry(GtkEditable *e, gpointer d) { (void)d; sput("rufUrl", gtk_entry_get_text(GTK_ENTRY(e))); }
static GtkWidget *sel(Cell *c, const char *k, const char *const *v, const char *const *l, int n)
{
	char *cur = sget(k); GtkWidget *s = s_select(v, l, n, cur, on_sel, (gpointer)k); g_free(cur); c_add(c, s, 0, 0, 1); p_label_named(k, s); g_object_set_data(G_OBJECT(s), "k", (gpointer)k); if (NSEL < 16) SEL[NSEL++] = s; return s;
}
static GtkWidget *ck(Cell *c, const char *k, const char *t)
{
	char *v = sget(k); GtkWidget *w = c_check(c, t, !strcmp(v, "true"), on_ck, (gpointer)k); g_free(v); p_label_named(k, w); g_object_set_data(G_OBJECT(w), "k", (gpointer)k); if (NCK < 16) CK[NCK++] = w; return w;
}
/* (refill: each control shows the working copy again) */
static void refill(void)
{
	plug_sync(); for (int i = 0; i < NSEL; i++) { char *v = sget(g_object_get_data(G_OBJECT(SEL[i]), "k")); s_select_set(SEL[i], v); g_free(v); }
	for (int i = 0; i < NCK; i++) { char *v = sget(g_object_get_data(G_OBJECT(CK[i]), "k")); rui_check_set(CK[i], !strcmp(v, "true")); g_free(v); }
	{ char *v = sget("gbVol"); s_range_set(RNG, atof(v)); g_free(v); } { char *v = sget("rufUrl"); g_signal_handlers_block_by_func(ENTRY, on_entry, NULL); gtk_entry_set_text(GTK_ENTRY(ENTRY), v); g_signal_handlers_unblock_by_func(ENTRY, on_entry, NULL); g_free(v); }
	say("", 1); p_footer_update();
}

/* (.lockbar: why the plugins cannot be changed) */
static const char LOCKT[] = "🔒 Only administrators can install, remove, enable or disable plugins. You can still change the playback preferences below.";
static double lock_lay(void *d, double x, double y, double w) { double h = 18 + 2 + s_lines(rui_tr(LOCKT), 13, 400, w - 22, NULL) * s_lh(13, 400); p_place(d, x, y, w, h); return h; }
static gboolean lock_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)d; double z = s_z(), W = gtk_widget_get_allocated_width(w) / z, H = gtk_widget_get_allocated_height(w) / z; cairo_save(cr); cairo_scale(cr, z, z);
	cairo_set_source_rgb(cr, 1, .984, .922); s_rrect(cr, 0, 0, W, H, 8); cairo_fill(cr); cairo_set_source_rgb(cr, .992, .902, .541); cairo_set_line_width(cr, 1); s_rrect(cr, .5, .5, W - 1, H - 1, 7.5); cairo_stroke(cr); cairo_restore(cr);
	cairo_set_source_rgb(cr, .573, .251, .055); s_text(cr, rui_tr(LOCKT), 11, 9, 13, 400, W - 22, 0); return TRUE;
}
/* ================================================================== the footer */
static void act(int what)
{
	if (what == 0) {
		char *u = sget("rufUrl"); g_strstrip(u);
		if (*u && !g_regex_match_simple("^https://[^\\s]+$", u, G_REGEX_CASELESS, 0)) { say("The runtime address must start with https:// (or be left empty).", 0); g_free(u); return; }
		sput("rufUrl", u); g_free(u);
		Mm old = {0}; mm_copy(&old, &C_); if (!ADM) { memcpy(W_.inst, C_.inst, sizeof W_.inst); memcpy(W_.en, C_.en, sizeof W_.en); } for (int i = 0; i < NP; i++) if (!W_.inst[i]) W_.en[i] = 0;
		mm_copy(&C_, &W_); char *j = mm_json(&C_); set_ls_set("bd95mm", j); set_note("mmSave %s", j); g_free(j);
		/* (mmApplied: a plugin installed or removed is written to the security log) */
		for (int i = 0; i < NP; i++) { if (C_.inst[i] == old.inst[i]) continue; char *m = g_strdup_printf("%s %s plugin %s", ME, C_.inst[i] ? "installed" : "removed", MMPLUG[i][0]); set_log_evt("sys", m); g_free(m); }
		mm_free(&old); p_footer_note("Multimedia settings applied."); refill(); return;
	}
	if (what == 1) { mm_copy(&W_, &C_); refill(); p_footer_note("Changes reverted."); return; }
	for (int i = 0; i < NS; i++) { g_free(W_.set[i]); W_.set[i] = g_strdup(MMSET[i][1]); } refill(); p_footer_note("Preferences reset. Apply to keep them.");
}
static void gone(GtkWidget *w, gpointer d) { (void)w; (void)d; MMM = NULL; ENTRY = RNG = NULL; NSEL = NCK = 0; }

void page_mm(void)
{
	if (!KEEP) load(); KEEP = 0; ADM = set_admin(); NSEL = NCK = 0;
	p_h3("Multimedia Settings");
	if (!ADM) { GtkWidget *a = gtk_drawing_area_new(); g_signal_connect(a, "draw", G_CALLBACK(lock_draw), NULL); p_put(a); p_label_named("lockbar", a); p_custom(lock_lay, a, 0, 10); }
	p_h4("Plugins");
	for (int i = 0; i < NP && i < 4; i++) { Plug *p = &PL[i]; p->i = i; p->a = gtk_drawing_area_new(); g_signal_connect(p->a, "draw", G_CALLBACK(plug_draw), p); p_put(p->a);
		p->ci = rui_check("Installed", W_.inst[i], on_inst, GINT_TO_POINTER(i)); p->ce = rui_check("Enabled", W_.en[i] && W_.inst[i], on_en, GINT_TO_POINTER(i)); p_put(p->ci); p_put(p->ce);
		char n[32]; snprintf(n, sizeof n, "inst-%s", MMPLUG[i][0]); p_label_named(n, p->ci); snprintf(n, sizeof n, "en-%s", MMPLUG[i][0]); p_label_named(n, p->ce);
		p_custom(zero_lay, NULL, 0, 0); p_custom_find("Installed", 0); p_custom(zero_lay, NULL, 0, 0); p_custom_find("Enabled", 0); p_custom(plug_lay, p, 0, 8); }
	plug_sync();
	p_h4("Folders and players");
	{ static const char *const L[4] = { "📁 INSTALLERS folder", "📁 EMUZIPS folder", "🦤 Emu", "⚡ Flash Player" }, *const A[4] = { "open explorer C:\\PROGRAMS\\INSTALLERS", "open explorer C:\\PROGRAMS\\EMUZIPS", "open gameboy", "open flash" }, *const N[4] = { "mmf1", "mmf2", "mmp1", "mmp2" };
		for (int i = 0; i < 4; i++) { FB[i] = rui_button(L[i], 0); g_signal_connect(FB[i], "clicked", G_CALLBACK(open_ask), (gpointer)A[i]); p_put(FB[i]); p_label_named(N[i], FB[i]); } p_custom(row_lay, NULL, 0, 0); }
	p_msg_m("INSTALLERS holds plugin installers and launchers (C:\\PROGRAMS\\INSTALLERS). EMUZIPS is where the emulators look for .ZIP game archives (C:\\PROGRAMS\\EMUZIPS).", 1, 6, 0);
	p_h4("Flash (Ruffle)");
	{ Cell *c = p_r2("Runtime address"); ENTRY = rui_entry("Default: unpkg / jsDelivr (@ruffle-rs/ruffle)"); { char *v = sget("rufUrl"); gtk_entry_set_text(GTK_ENTRY(ENTRY), v); g_free(v); } g_signal_connect(ENTRY, "changed", G_CALLBACK(on_entry), NULL);
		g_signal_connect(ENTRY, "destroy", G_CALLBACK(gone), NULL); c_add(c, ENTRY, 0, 0, 1); p_label_named("rufUrl", ENTRY); p_r2_more(c); }
	{ static const char *const v[] = { "low", "medium", "high", "best" }, *const l[] = { "Low", "Medium", "High", "Best" }; Cell *c = p_r2("Quality"); sel(c, "rufQ", v, l, 4); p_r2_more(c); }
	{ static const char *const v[] = { "showAll", "noBorder", "exactFit", "noScale" }, *const l[] = { "Show all", "No border", "Exact fit", "No scaling" }; Cell *c = p_r2("Scaling"); sel(c, "rufScale", v, l, 4); p_r2_more(c); }
	{ Cell *c = p_r2("Options"); ck(c, "rufNet", "Let Flash content use the network"); c_line(c, 15); ck(c, "rufMenu", "Show the right-click menu"); p_r2_more(c); }
	{ Cell *c = p_r2("Runtime"); GtkWidget *b = rui_button("Check Ruffle download", 0); g_signal_connect(b, "clicked", G_CALLBACK(on_chk), NULL); c_add(c, b, 0, 0, 0); p_label_named("mmchk", b); }
	p_h4("Game Boy / Game Boy Advance");
	{ static const char *const v[] = { "0", "1", "2", "3", "4", "5" }, *const l[] = { "Fit window", "1×", "2×", "3×", "4×", "5×" }; Cell *c = p_r2("Screen size"); sel(c, "gbScale", v, l, 6); p_r2_more(c); }
	{ static const char *const v[] = { "green", "gray", "pocket", "amber" }, *const l[] = { "Classic green", "Gray", "Pocket", "Amber" }; Cell *c = p_r2("Palette"); sel(c, "gbPal", v, l, 4); p_r2_more(c); }
	{ static const char *const v[] = { "none", "scan", "lcd", "crt" }, *const l[] = { "None (sharp pixels)", "Scanlines (GPU)", "LCD grid (GPU)", "CRT (GPU)" }; Cell *c = p_r2("Screen filter"); sel(c, "gbFilter", v, l, 4); p_r2_more(c); }
	{ static const char *const v[] = { "0.5", "1", "2", "4" }, *const l[] = { "0.5×", "1× (normal)", "2×", "4×" }; Cell *c = p_r2("Speed"); sel(c, "gbSpeed", v, l, 4); p_r2_more(c); }
	{ Cell *c = p_r2("Volume"); char *v = sget("gbVol"); RNG = s_range(0, 100, 1, atof(v), on_rng, (gpointer)"gbVol"); g_free(v); c_add(c, RNG, 2, 2, 1); c_line(c, 2); p_label_named("gbVol", RNG); p_r2_more(c); }
	{ Cell *c = p_r2("Options"); ck(c, "gbSound", "Sound on"); c_line(c, 15); ck(c, "gbSmooth", "Smooth pixels"); c_line(c, 15); ck(c, "gbCgb", "Play Game Boy Color games in color"); c_line(c, 15); ck(c, "gbSave", "Keep battery saves automatically"); }
	p_msg_m("Game Boy Advance: A / S keys = L / R. GBA saves (SRAM, Flash, EEPROM) and the real-time clock work automatically. Game files over 1 MB added to the library are kept in browser storage.", 1, 6, 0);
	p_h4("ZIP archives");
	{ Cell *c = p_r2("Archives"); ck(c, "zipOn", "Let the emulators read .ZIP files"); p_r2_more(c); }
	{ static const char *const v[] = { "4", "16", "32", "64" }, *const l[] = { "4 MB", "16 MB", "32 MB", "64 MB" }; Cell *c = p_r2("Unpacked limit"); sel(c, "zipMax", v, l, 4); }
	MMM = p_msg("", 0); p_label_named("mmm", MMM);
	p_footer(dirty, act);
}
