/*
 * rudolf-settings: System Settings (base_v24.html sysPanelS; settings31.js's Date, time and language: the language, Share my language
 * with apps, Clock from the internet; games58.js's shareLang; engine57.js's Desktop engine; rest61.js's Idle windows). 1.6.2,
 * docs/plan-1.6.2.md Part 39. As the page: the clock, the time zone, the date and its format, the language and sharing it are kept in
 * the page's working copy until Apply (secFooter: Apply, Revert, Reset to defaults, each confirmed; the page's SKEYS saved as persistS
 * saves them, bd95set:ACCT); the desktop engine is changed at once with the administrator's permission (desktop/engine?engine=);
 * Idle windows are saved at once (persistS with SC). GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
#include "settings-gen.h"
#pragma GCC diagnostic pop

/* the page's S and SC for this page's keys (SKEYS, settings31.js's lang, games58.js's shareLang), as JSON values */
enum { K_CK, K_H24, K_TZ, K_DOFF, K_DF, K_LANG, K_SHARE, NK };
static const char *const KN[NK] = { "ck", "h24", "tz", "doff", "df", "lang", "shareLang" };
static char *S_[NK], *SC_[NK];
static const char *dfl(int k) { static const char *const D[NK] = { "\"digital\"", "false", NULL, "0", "\"ymd\"", "\"en\"", "true" }; return D[k]; }
static char *jval(RuiJ *o, const char *k)
{
	RuiJ *v = rui_jprop(o, k); int t = rui_jtype(v); if (t < 0) return NULL;
	if (t == 1) return g_strdup(rui_jval(v) ? "true" : "false");
	if (t == 2) { char b[40]; double x = rui_jval(v); if (x == floor(x) && fabs(x) < 1e15) snprintf(b, sizeof b, "%.0f", x); else g_ascii_formatd(b, sizeof b, "%.17g", x); return g_strdup(b); }
	if (t == 3) { GString *b = g_string_new("\""); for (const char *s = rui_jsval(v); *s; s++) { if (*s == '"' || *s == '\\') g_string_append_c(b, '\\'); g_string_append_c(b, *s); } g_string_append_c(b, '"'); return g_string_free(b, FALSE); }
	return t == 0 ? g_strdup("null") : NULL;
}
static char *jstrv(const char *s) { GString *b = g_string_new("\""); for (; *s; s++) { if (*s == '"' || *s == '\\') g_string_append_c(b, '\\'); g_string_append_c(b, *s); } g_string_append_c(b, '"'); return g_string_free(b, FALSE); }
static char *unq(const char *j) { if (!j) return g_strdup(""); RuiJ *o = NULL; char *w = g_strdup_printf("{\"x\":%s}", j); o = rui_jparse(w); g_free(w); char *r = g_strdup(set_jstr(o, "x", "")); rui_jfree(o); return r; }
static int jtrue(const char *j) { return j && !strcmp(j, "true"); }
static double jnum(const char *j) { return j ? g_ascii_strtod(j, NULL) : 0; }
static const char *eff(char **A, int k) { return A[k] ? A[k] : dfl(k); }
/* (the local time zone: the page's Intl.DateTimeFormat().resolvedOptions().timeZone; TZ as the tests give it) */
static char *local_tz(void) { const char *t = getenv("TZ"); if (t && *t && !strchr(t, '/') && strcmp(t, "UTC")) t = NULL; if (t && *t) return g_strdup(*t == ':' ? t + 1 : t);
	GTimeZone *z = g_time_zone_new_local(); char *r = g_strdup(g_time_zone_get_identifier(z)); g_time_zone_unref(z); return r; }
static void load(void)
{
	RuiJ *o = set_stored();
	for (int k = 0; k < NK; k++) { g_free(SC_[k]); SC_[k] = jval(o, KN[k]); g_free(S_[k]); S_[k] = SC_[k] ? g_strdup(SC_[k]) : NULL; }
	rui_jfree(o);
	if (!SC_[K_TZ]) { char *l = local_tz(); SC_[K_TZ] = jstrv(l); S_[K_TZ] = g_strdup(SC_[K_TZ]); g_free(l); }
}
static int dirty(void) { for (int k = 0; k < NK; k++) if (strcmp(eff(S_, k) ? eff(S_, k) : "", eff(SC_, k) ? eff(SC_, k) : "")) return 1; return 0; }
static void setk(int k, char *v) { g_free(S_[k]); S_[k] = v; p_footer_update(); set_state_soon(); }

/* ---- dates and times as the page makes them (ymd, dateFmt, sysDateStr, clockParts) in a time zone */
static GDateTime *in_tz(double t, const char *tz) { GTimeZone *z = tz && *tz ? g_time_zone_new_identifier(tz) : NULL; if (!z) z = g_time_zone_new_utc(); GDateTime *d = g_date_time_new_from_unix_utc((gint64)floor(t)); GDateTime *r = g_date_time_to_timezone(d, z); g_date_time_unref(d); g_time_zone_unref(z); return r; }
static char *date_fmt(GDateTime *d, const char *f)
{
	int y = g_date_time_get_year(d), m = g_date_time_get_month(d), dd = g_date_time_get_day_of_month(d);
	if (!strcmp(f, "mdy")) return g_strdup_printf("%02d/%02d/%d", m, dd, y); if (!strcmp(f, "dmy")) return g_strdup_printf("%02d/%02d/%d", dd, m, y); return g_strdup_printf("%d/%02d/%02d", y, m, dd);
}
static char *date_str_with(double t, char **A) { char *tz = unq(eff(A, K_TZ)), *df = unq(eff(A, K_DF)); GDateTime *d = in_tz(t, tz); char *r = date_fmt(d, *df ? df : "ymd"); g_date_time_unref(d); g_free(tz); g_free(df); return r; }
static double sys_now(char **A) { return set_now() + jnum(eff(A, K_DOFF)) / 1000; }
char *set_date_str(double t)
{
	/* (About's Released: the page's sysDateStr with the settings saved) */
	RuiJ *o = set_stored(); char *A[NK] = { 0 }; for (int k = 0; k < NK; k++) A[k] = jval(o, KN[k]); rui_jfree(o); if (!A[K_TZ]) { char *l = local_tz(); A[K_TZ] = jstrv(l); g_free(l); }
	char *r = date_str_with(t, A); for (int k = 0; k < NK; k++) g_free(A[k]); return r;
}
const char *set_lang(void);
const char *set_lang(void) { static char l[16]; RuiJ *o = set_stored(); snprintf(l, sizeof l, "%s", set_jstr(o, "lang", "en")); rui_jfree(o); return l; }
static char *time_str(double t, char **A) { char *tz = unq(eff(A, K_TZ)); GDateTime *d = in_tz(t, tz); g_free(tz); int h = g_date_time_get_hour(d), m = g_date_time_get_minute(d); g_date_time_unref(d);
	return jtrue(eff(A, K_H24)) ? g_strdup_printf("%02d:%02d", h, m) : g_strdup_printf("%d:%02d %s", h % 12 ? h % 12 : 12, m, h < 12 ? "AM" : "PM"); }

/* ---- the time zones (tzList: for each whole hour from GMT-12 to GMT+14, the first city of POOL there now) */
typedef struct { int t; char z[48], l[120]; } Tz;
static Tz TZL[27];
static double off_of(const char *z, double now) { GTimeZone *g = g_time_zone_new_identifier(z); if (!g) return NAN; int i = g_time_zone_find_interval(g, G_TIME_TYPE_UNIVERSAL, (gint64)now); double o = i >= 0 ? g_time_zone_get_offset(g, i) / 3600.0 : NAN; g_time_zone_unref(g); return o; }
static void tz_list(void)
{
	double now = set_now();
	for (int t = -12; t <= 14; t++) { Tz *x = &TZL[t + 12]; x->t = t; int f = -1;
		for (guint p = 0; p < G_N_ELEMENTS(TZPOOL) && f < 0; p++) if (off_of(TZPOOL[p][1], now) == t) f = p;
		if (f >= 0) { snprintf(x->z, sizeof x->z, "%s", TZPOOL[f][1]); snprintf(x->l, sizeof x->l, "%s, %s, %s (GMT%c%d)", TZPOOL[f][0], TZPOOL[f][2], TZPOOL[f][3], t < 0 ? '-' : '+', abs(t)); }
		else { snprintf(x->z, sizeof x->z, "Etc/GMT%c%d", t > 0 ? '-' : '+', abs(t)); snprintf(x->l, sizeof x->l, "Open ocean (GMT%c%d)", t < 0 ? '-' : '+', abs(t)); } }
}

/* ---- the page's controls */
static GtkWidget *SDT, *SDP, *TZS, *DFS, *ENG, *ENGNOW, *NTSM, *IR, *IM;
static void sync_date(void)
{
	char *tz = unq(eff(S_, K_TZ)); GDateTime *d = in_tz(sys_now(S_), tz); g_free(tz);
	if (SDT) s_date_set(SDT, g_date_time_get_year(d), g_date_time_get_month(d), g_date_time_get_day_of_month(d)); g_date_time_unref(d);
	if (SDP) { char *s = date_str_with(sys_now(S_), S_); p_text_set(SDP, s); g_free(s); }
}
static void on_ck(const char *v, void *d) { (void)d; setk(K_CK, jstrv(v)); set_note("set ck %s", v); }
static void on_h24(int on, void *d) { (void)d; setk(K_H24, g_strdup(on ? "true" : "false")); set_note("set h24 %d", on); }
static void on_tz(const char *v, void *d) { (void)d; setk(K_TZ, jstrv(v)); sync_date(); set_note("set tz %s", v); }
static void on_df(const char *v, void *d) { (void)d; setk(K_DF, jstrv(v)); sync_date(); set_note("set df %s", v); }
static void on_date(int y, int m, int dd, void *d)
{
	/* (setSysDate: the desktop's date moved by whole days from today's, at most 100 years) */
	(void)d; if (!set_admin()) return; char *tz = unq(eff(S_, K_TZ)); GDateTime *t = in_tz(set_now(), tz); g_free(tz);
	GDateTime *a = g_date_time_new_utc(y, m, dd, 0, 0, 0), *b = g_date_time_new_utc(g_date_time_get_year(t), g_date_time_get_month(t), g_date_time_get_day_of_month(t), 0, 0, 0); g_date_time_unref(t);
	if (!a || !b) { if (a) g_date_time_unref(a); if (b) g_date_time_unref(b); return; }
	double r = round(g_date_time_difference(a, b) / 1e6 / 86400); g_date_time_unref(a); g_date_time_unref(b); if (fabs(r) > 36500) return;
	setk(K_DOFF, g_strdup_printf("%.0f", r * 864e5)); sync_date(); set_note("set doff %.0f", r * 864e5);
}
static void on_now(GtkButton *b, gpointer d) { (void)b; (void)d; if (!set_admin()) return; setk(K_DOFF, g_strdup("0")); sync_date(); set_note("set doff 0"); }
static void retranslate(void);
static void on_lang(const char *v, void *d) { (void)d; setk(K_LANG, jstrv(v)); set_note("set lang %s", v); retranslate(); }
static void on_share(int on, void *d) { (void)d; setk(K_SHARE, g_strdup(on ? "true" : "false")); set_note("set shareLang %d", on); }
/* (a11yApply's setLang, and W.rt: the window in the language chosen, before it is applied, as the page's) */
static gboolean rebuild_idle(gpointer d) { (void)d; set_show("sys"); return G_SOURCE_REMOVE; }
static int KEEP;   /* (the working copy kept across a rebuild of the page) */
static void retranslate(void) { char *l = unq(eff(S_, K_LANG)); if (strcmp(l, RUI.lang)) { snprintf(RUI.lang, sizeof RUI.lang, "%s", l); KEEP = 1; g_idle_add(rebuild_idle, NULL); } g_free(l); }
/* ---- Clock from the internet (settings31.js tsay) */
static void tsay(void)
{
	if (!NTSM) return; RuiJ *j = rui_get("/api/time/status"); if (!j) return; const char *r = rui_jstr(j, "result"); double at = rui_jnum(j, "at", 0); char *m;
	char *wt = at ? time_str(at, S_) : NULL, *wd = at ? date_str_with(at, S_) : NULL; char *tz = unq(eff(S_, K_TZ));
	GDateTime *a = at ? in_tz(at, tz) : NULL, *n = in_tz(sys_now(S_), tz); g_free(tz);
	int same = a && g_date_time_get_year(a) == g_date_time_get_year(n) && g_date_time_get_day_of_year(a) == g_date_time_get_day_of_year(n);
	if (r && !strcmp(r, "ok")) m = g_strdup_printf("Set from the internet at %s%s%s. Rudolf OS does this by itself at every start, when the network comes up and every 12 hours.", wt, same ? "" : " on ", same ? "" : wd);
	else if (r && !strcmp(r, "fail")) m = g_strdup_printf("The time servers did not answer at %s (offline?). Rudolf OS tries again when the network comes up.", wt);
	else m = g_strdup("Rudolf OS sets the clock from the internet by itself once the network is up.");
	if (rui_jbool(j, "floored")) { char *x = g_strdup_printf("%s At start the clock read an earlier date than this release, so it was set to the release date until then.", m); g_free(m); m = x; }
	p_text_set(NTSM, m); g_free(m); g_free(wt); g_free(wd); if (a) g_date_time_unref(a); g_date_time_unref(n); rui_jfree(j);
}
static void on_nts(GtkButton *b, gpointer d)
{
	(void)b; (void)d; char *out = NULL; int c = rui_http("POST", "/api/time/sync", NULL, 0, &out, NULL); RuiJ *j = out ? rui_jparse(out) : NULL;
	set_notify("Clock", set_jstr(j, c == 200 ? "message" : "error", c ? "" : "rudolfd did not answer"), c != 200); set_note("time/sync %d", c); rui_jfree(j); g_free(out); tsay();
}
/* ---- Desktop engine (engine57.js) */
static const char *const ENGV[3] = { "native", "chromium", "webkit" }, *const ENGL[3] = { "Native", "Chromium", "Lite (WebKit, test)" };
static const char *nm(const char *v) { return !strcmp(v, "webkit") ? "Lite (WebKit)" : !strcmp(v, "native") ? "Native" : "Chromium"; }
static void eng_draw(void);
static char ENGWANT[16];
static void eng_go(int ok, void *d)
{
	(void)d; if (ok == 1) { char p[80], err[200]; snprintf(p, sizeof p, "/api/desktop/engine?engine=%s", ENGWANT); int c = rui_post(p, NULL, err, sizeof err);
		if (c == 200) { char *m = g_strdup_printf("%s is used after the computer restarts.", nm(ENGWANT)); set_notify("Desktop engine", m, 0); g_free(m); } else set_notify("Desktop engine", err, 1); set_note("engine %s %d", ENGWANT, c); }
	else set_note("engine refused (administrator)");
	eng_draw();
}
static void on_eng(const char *v, void *d) { (void)d; snprintf(ENGWANT, sizeof ENGWANT, "%s", v); rui_elevate(SW, "Change the desktop engine", eng_go, NULL); set_note("ask engine %s", v); }
static void eng_draw(void)
{
	RuiJ *e = rui_get("/api/desktop/engine"); if (!e || !ENG) { rui_jfree(e); return; }
	const char *choice = set_jstr(e, "choice", ""), *now = set_jstr(e, "now", ""); int adm = set_admin(), pers = rui_jbool(e, "persistent");
	s_select_set(ENG, choice); gtk_widget_set_sensitive(ENG, adm && pers);
	char t[120];
	s_select_disable(ENG, 2, !rui_jbool(e, "available")); snprintf(t, sizeof t, "%s%s", rui_tr(ENGL[2]), rui_jbool(e, "available") ? "" : " — update Rudolf OS first"); s_select_label(ENG, 2, t);
	s_select_disable(ENG, 0, !rui_jbool(e, "nativeAvailable")); snprintf(t, sizeof t, "%s%s", rui_tr(ENGL[0]), rui_jbool(e, "nativeAvailable") ? "" : " — update Rudolf OS first"); s_select_label(ENG, 0, t);
	GString *m = g_string_new(NULL); g_string_append_printf(m, "In use now: %s%s%s%s.", nm(now), rui_jbool(e, "failed") ? " (" : "", rui_jbool(e, "failed") ? (!strcmp(choice, "native") ? "Native" : "Lite") : "", rui_jbool(e, "failed") ? " did not start this time, so Chromium was used)" : "");
	if (strcmp(choice, now)) g_string_append_printf(m, " %s is used after the computer restarts.", nm(choice));
	g_string_append(m, !adm ? " Only administrators can change it." : !pers ? " Kept only on an installed Rudolf OS." : "");
	const char *gu = rui_jstr(e, "gaveUp");
	if (gu) { char when[64] = "", why[300]; const char *sp = strchr(gu, ' '); snprintf(why, sizeof why, "%s", sp ? sp + 1 : gu); int nat = g_str_has_suffix(why, " (Native)"); if (nat) why[strlen(why) - 9] = 0;
		if (sp) { char iso[40]; snprintf(iso, sizeof iso, "%.*s", (int)MIN(sp - gu, 39), gu); GDateTime *d = g_date_time_new_from_iso8601(iso, NULL); if (d) { GDateTime *l = g_date_time_to_local(d); char *f = g_date_time_format(l, "%m/%d/%Y, %l:%M:%S %p"); snprintf(when, sizeof when, "%s", g_strstrip(f)); g_free(f); g_date_time_unref(l); g_date_time_unref(d); } }
		g_string_append_printf(m, " %s was turned off%s%s because %s. Choose it again to try once more.", nat ? "Native" : "Lite", when[0] ? " on " : "", when, why); }
	p_text_set(ENGNOW, m->str); g_string_free(m, TRUE); rui_jfree(e);
}
/* ---- Idle windows (rest61.js: saved at once) */
static void rest_save(void)
{
	const char *k[2] = { "idlerest", "idlemin" }; char *v[2] = { g_strdup(rui_check_get(IR) ? "true" : "false"), g_strdup(s_select_get(IM)) };
	set_store(k, (const char *const *)v, 2); gtk_widget_set_sensitive(IM, rui_check_get(IR)); set_note("idle %s %s", v[0], v[1]); g_free(v[0]); g_free(v[1]);
}
static void on_ir(int on, void *d) { (void)on; (void)d; rest_save(); }
static void on_im(const char *v, void *d) { (void)v; (void)d; rest_save(); }

/* ---- the footer's Apply, Revert, Reset to defaults (secCommit, secRevert, secReset) */
static void commit(void)
{
	const char *k[NK]; const char *v[NK]; for (int i = 0; i < NK; i++) { k[i] = KN[i]; v[i] = S_[i]; g_free(SC_[i]); SC_[i] = S_[i] ? g_strdup(S_[i]) : NULL; }
	set_store(k, v, NK);
}
static void act(int what)
{
	if (what == 0) { commit(); p_footer_note("Settings applied."); }
	else if (what == 1) { for (int i = 0; i < NK; i++) { g_free(S_[i]); S_[i] = SC_[i] ? g_strdup(SC_[i]) : NULL; } KEEP = 1; char *l = unq(eff(S_, K_LANG)); snprintf(RUI.lang, sizeof RUI.lang, "%s", l); g_free(l); set_show("sys"); p_footer_note("Changes reverted."); }
	else { /* (DEFS.sys and games58.js's shareLang; the time zone this computer's; an administrator's own date offset kept for others) */
		static const int R[5] = { K_CK, K_H24, K_DOFF, K_DF, K_SHARE }; char *keepd = set_admin() ? NULL : (S_[K_DOFF] ? g_strdup(S_[K_DOFF]) : NULL);
		for (int i = 0; i < 5; i++) { g_free(S_[R[i]]); S_[R[i]] = g_strdup(dfl(R[i])); }
		char *l = local_tz(); g_free(S_[K_TZ]); S_[K_TZ] = jstrv(l); g_free(l); if (!set_admin()) { g_free(S_[K_DOFF]); S_[K_DOFF] = keepd; }
		commit(); KEEP = 1; set_show("sys"); p_footer_note("Defaults restored."); }
	set_state_soon();
}

void page_sys(void)
{
	if (!KEEP) load(); KEEP = 0; tz_list(); int adm = set_admin();
	p_h3("System Settings"); p_sgrp(SGRP_T);
	/* Clock */
	p_h4("Clock");
	{ Cell *c = p_r2("Clock"); p_r2_more(c); const char *v[2] = { "digital", "analog" }, *l[2] = { "Digital", "Analog" }; char *ck = unq(eff(S_, K_CK));
	  GtkWidget *s = s_select(v, l, 2, strcmp(ck, "analog") ? "digital" : "analog", on_ck, NULL); gtk_widget_set_name(s, "ck"); c_add(c, s, 0, 0, 0); p_label_named("ck", s); g_free(ck);
	  GtkWidget *h = rui_check("24-hour", jtrue(eff(S_, K_H24)), on_h24, NULL); g_object_set_data_full(G_OBJECT(h), "set-label", g_strdup(rui_tr("24-hour")), g_free);   /* (label.ck inline: the search finds it) */ c_add(c, h, 4.1, 1, 0); p_label_named("h24", h); }
	{ Cell *c = p_r2("Time zone"); char *tz = unq(eff(S_, K_TZ)); int at = -1; for (int i = 0; i < 27; i++) if (!strcmp(TZL[i].z, tz)) at = i;
	  if (at < 0) { double o = off_of(tz, set_now()); for (int i = 0; i < 27 && at < 0; i++) if (TZL[i].t == o) at = i; }
	  const char *v[28], *l[28]; int n = 0; if (at < 0) { v[n] = tz; l[n] = tz; n++; } for (int i = 0; i < 27; i++) { v[n] = TZL[i].z; l[n] = TZL[i].l; n++; }
	  TZS = s_select(v, l, n, at < 0 ? tz : TZL[at].z, on_tz, NULL); gtk_widget_set_name(TZS, "tz"); c_add(c, TZS, 0, 0, 1); p_label_named("tz", TZS); g_free(tz); }
	/* Date */
	p_h4("Date");
	{ Cell *c = p_r2("Date"); p_r2_more(c); SDT = s_date(2026, 1, 1, on_date, NULL); gtk_widget_set_name(SDT, "sdt"); gtk_widget_set_sensitive(SDT, adm); c_add(c, SDT, 0, 1, 0); p_label_named("sdt", SDT);
	  GtkWidget *b = rui_button("Use current date", 0); gtk_widget_set_sensitive(b, adm); g_signal_connect(b, "clicked", G_CALLBACK(on_now), NULL); c_add(c, b, 4.4, 0, 0); p_label_named("sdn", b); }
	{ Cell *c = p_r2("Date format"); p_r2_more(c); const char *v[3] = { "ymd", "mdy", "dmy" }, *l[3] = { "YYYY/MM/DD", "MM/DD/YYYY", "DD/MM/YYYY" }; char *df = unq(eff(S_, K_DF));
	  DFS = s_select(v, l, 3, *df ? df : "ymd", on_df, NULL); gtk_widget_set_name(DFS, "sdf"); c_add(c, DFS, 0, 0, 1); p_label_named("sdf", DFS); g_free(df); }
	{ Cell *c = p_r2("Preview"); SDP = c_text(c, "", 700); }
	sync_date();
	p_msg(adm ? "The date you choose is used by the clock, the terminal and the whole system." : "Only administrators can change the system date.", 1);
	/* Language (settings31.js) */
	p_h4("Language");
	{ Cell *c = p_r2("Language"); const char *v[16], *l[16]; int n = 0; for (guint i = 0; i < G_N_ELEMENTS(SLANGS) && n < 16; i++) { v[n] = SLANGS[i][0]; l[n] = SLANGS[i][1]; n++; } char *lg = unq(eff(S_, K_LANG));
	  GtkWidget *s = s_select(v, l, n, *lg ? lg : "en", on_lang, NULL); gtk_widget_set_name(s, "slg"); c_add(c, s, 0, 0, 1); p_label_named("slg", s); g_free(lg); }
	{ Cell *c = p_r2("Apps"); GtkWidget *k = c_check(c, "Share my language with apps", strcmp(eff(S_, K_SHARE), "false") != 0, on_share, NULL); p_label_named("slgs", k);
	  c_msg(c, "Apps from the Library, such as games, can then show their own menus in this language. Off, they are not told which language you use.", 1); }
	p_h4("Clock from the internet");
	{ Cell *c = p_r2("Internet time"); GtkWidget *b = rui_button("Set the clock from the internet", 0); g_signal_connect(b, "clicked", G_CALLBACK(on_nts), NULL); c_add(c, b, 0, 0, 0); p_label_named("nts", b);
	  NTSM = c_msg(c, "", 0); tsay(); }
	/* Desktop engine (engine57.js: shown when rudolfd answers) */
	{ RuiJ *e = rui_get("/api/desktop/engine"); if (e) {
		p_h4("Desktop engine");
		Cell *c = p_r2("Desktop engine"); ENG = s_select(ENGV, ENGL, 3, set_jstr(e, "choice", "chromium"), on_eng, NULL); gtk_widget_set_name(ENG, "eng57"); c_add(c, ENG, 0, 0, 0); p_label_named("eng57", ENG); ENGNOW = c_msg(c, "", 0);
		p_msg("Native draws the desktop itself, without a web engine (the taskbar, the start menu, the windows' frames, the desktop and the sign-in screen), and opens each app in a window of its own: it looks and works as Chromium, uses less memory and starts faster. A new installation starts with it. If it does not start, Chromium is used.", 0);
		p_msg("Chromium runs the desktop page in Rudolf's Chromium, as Rudolf OS always has (before 1.6.2 it was called Standard).", 0);
		p_msg("Lite runs the desktop and its apps in WebKit, a lighter web engine: it takes much less memory, which leaves more for apps and games. Websites then open in Chromium's own window. It is a test in 1.5.7: some things may look or work differently (videos in H.264 or AAC play only with Chromium for now). The engine changes when the computer restarts. If the desktop shows nothing or keeps restarting, hold \x01" "Ctrl+Alt+Backspace\x02" " for a second: it starts again with Chromium.", 0);
		rui_jfree(e); eng_draw(); } else ENG = ENGNOW = NULL; }
	/* Idle windows (rest61.js) */
	{ int small = 1; RuiJ *st = rui_get("/api/stats"); double mt = rui_jnum(st, "memTotal", 0); if (mt) small = mt < 6.0 * 1073741824; rui_jfree(st);
	  RuiJ *o = set_stored(); RuiJ *ir = rui_jprop(o, "idlerest"); int on = rui_jtype(ir) == 1 ? rui_jval(ir) != 0 : small; int mn = (int)rui_jnum(o, "idlemin", 10); if (mn != 5 && mn != 10 && mn != 20 && mn != 30 && mn != 60) mn = 10; rui_jfree(o);
	  int nat = !strcmp(set_engine_now(), "native");
	  p_h4("Idle windows"); Cell *c = p_r2("Idle windows"); IR = c_check(c, "Give an app's memory back when its window has been minimized or covered for a while", on, on_ir, NULL); p_label_named("ir61", IR);
	  c_line(c, 6); c_label(c, "After", 5); const char *v[5] = { "5", "10", "20", "30", "60" }, *l[5] = { "5 minutes", "10 minutes", "20 minutes", "30 minutes", "60 minutes" }; char cur[8]; snprintf(cur, sizeof cur, "%d", mn);
	  IM = s_select(v, l, 5, cur, on_im, NULL); gtk_widget_set_name(IM, "im61"); gtk_widget_set_sensitive(IM, on); c_add(c, IM, 4.2, 0, 0); p_label_named("im61", IM);
	  char *m = g_strdup_printf("%sNotepad and File Explorer only: the app's page is unloaded and loads again where it was when you show the window (its files, folder and scroll). Never with unsaved changes, a dialog, sound or video playing, a call, a download or a transfer. Oracle, Linux, Debian, Flatpak and Wine apps are never touched. %s",
		nat ? "" : "On the native desktop (Settings › System health › Desktop engine: Native). ", small ? "On by default on this computer (under 6 GB of memory)." : "Off by default on this computer (6 GB of memory or more).");
	  c_msg(c, m, 0); g_free(m); }
	p_footer(dirty, act);
}
