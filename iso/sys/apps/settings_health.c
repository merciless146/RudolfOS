/*
 * rudolf-settings: System health (sec50.js healthPanel, with power31.js's Power and battery, devlink40.js's and sec50.js's rows, apps57.js's
 * Memory, gamemode61.js's and gamesleep59.js's Game mode). 1.6.3, docs/plan-1.6.3.md Part 3 (PR 8). As the page: rudolfd's health log
 * (its summary of the last 7 days and the events, by kind), power/status and power/set (each change at once; administrators only),
 * devlink, accounts/users, memory/settings (the administrator's prompt first), game/set; S.gamemode and S.gmsleep kept at once. The
 * windows the page opens (Report a problem…, the self-test, Last starts, Blitzen's "Why?") open in a page of one action ("storgo W").
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
static void redraw(void) { set_show_keep("health"); }
static void ask(const char *a) { rui_shell(SW, a); set_note("%s", a); }
static void go(const char *w, const char *arg) { char a[160]; snprintf(a, sizeof a, "storgo %s%s%s", w, arg ? " " : "", arg ? arg : ""); ask(a); }
static int jb(RuiJ *o, const char *k) { RuiJ *v = o ? rui_jprop(o, k) : NULL; return v && ((rui_jtype(v) == 1 && rui_jval(v)) || (rui_jtype(v) == 2 && rui_jval(v)) || (rui_jtype(v) == 3 && *rui_jsval(v))); }
static double jnum(RuiJ *o, const char *k) { RuiJ *v = o ? rui_jprop(o, k) : NULL; return v && rui_jtype(v) == 2 ? rui_jval(v) : NAN; }
static RuiJ *get(const char *path) { char p[300]; snprintf(p, sizeof p, "/api/%s", path); return rui_get(p); }
static RuiJ *post(const char *path, char *err, size_t en)
{
	char p[600], *out = NULL; snprintf(p, sizeof p, "/api/%s", path); int c = rui_http("POST", p, NULL, 0, &out, NULL); RuiJ *j = out ? rui_jparse(out) : NULL; g_free(out); set_note("POST %s %d", path, c);
	if (c != 200 || !j) { if (err) snprintf(err, en, "%s", j ? set_jstr(j, "error", "Request failed") : "rudolfd did not answer"); rui_jfree(j); return NULL; } return j;
}
static char *fmt_b(double b) { if (!(b >= 0)) b = 0; if (b < 1024) return g_strdup_printf("%.0f B", b); if (b < 1048576) return g_strdup_printf("%.1f KB", b / 1024); if (b < 1073741824) return g_strdup_printf("%.2f MB", b / 1048576); return g_strdup_printf("%.1f GB", b / 1073741824); }
/* (hlWhen: the settings' date and the time, "11/14/2026 8:46 AM") */
static char *when(double t) { char *d = set_date_str(t); time_t tt = (time_t)t; struct tm lt; localtime_r(&tt, &lt); int h = lt.tm_hour % 12 ? lt.tm_hour % 12 : 12; char *r = g_strdup_printf("%s %d:%02d %s", d, h, lt.tm_min, lt.tm_hour < 12 ? "AM" : "PM"); g_free(d); return r; }

/* buttons in a .rmrow (flex, wrapping) */
typedef struct { GtkWidget *b[8]; int n; double gap; } BRow;
static double brow_lay(void *d, double x, double y, double w)
{
	BRow *r = d; double cx = 0, cy = 0, lh = 0, g = r->gap ? r->gap : 8;
	for (int k = 0; k < r->n; k++) { double bw = p_nat_w(r->b[k]), bh = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(r->b[k]), "h25")) ? 25 : p_nat_h(r->b[k]); if (cx > 0 && cx + bw > w + .5) { cx = 0; cy += lh + g; lh = 0; } p_place(r->b[k], x + cx, y + cy, 0, bh); cx += bw + g; lh = fmax(lh, bh); }
	return cy + lh;
}

static const char *const KIND[][3] = {   /* (HL_KIND) */
	{ "start", "🔌", "Start-up" }, { "recover", "♻️", "Kept in Recovered" }, { "ready", "✅", "Ready" }, { "fallback", "↩️", "Update fallback" }, { "memory", "🧠", "Memory" }, { "crash", "💥", "Crash" },
	{ "browser", "🔄", "Desktop restart" }, { "signin", "🔑", "Sign-in" }, { "disk", "💾", "Storage" }, { "heat", "🌡️", "Heat" }, { "firewall", "🛡️", "Firewall" }, { "remote", "🖥️", "Remote access" },
	{ "vault", "🔐", "Encryption" }, { "update", "⬆️", "Update" }, { "account", "👤", "Accounts" }, { "bluetooth", "🔵", "Bluetooth" }, { "hardware", "🧩", "Hardware" }, { "library", "📚", "Rudolf OS Library" },
	{ "security", "🔒", "Security" }, { "tor", "🧅", "Tor" }, { "modem", "📶", "Mobile network" }, { "extension", "🧩", "System extensions" }, { "boot", "🥾", "Start-up files and firmware" } };
static RuiJ *HL; static char *KSEL;

/* ================================================================== the summary (hlSummary: the last 7 days) */
static void summary(void)
{
	RuiJ *ev = rui_jprop(HL, "events"); double now = jnum(HL, "now"); int n = ev ? rui_jlen(ev) : 0, cnt[6] = { 0 }; RuiJ *rd1 = NULL;
	for (int i = 0; i < n; i++) { RuiJ *e = rui_jat(ev, i); const char *k = set_jstr(e, "k", ""); if (!rd1 && !strcmp(k, "ready")) rd1 = e; if (now - jnum(e, "t") >= 7 * 86400) continue;
		if (!strcmp(k, "start")) cnt[0]++; if (!strcmp(k, "crash") || !strcmp(k, "browser")) cnt[1]++; if (!strcmp(k, "memory")) cnt[2]++; if (!strcmp(k, "fallback")) cnt[3]++; if (!strcmp(k, "signin")) cnt[4]++; }
	char *last; if (rd1) { GMatchInfo *mi; char *sec = NULL; if (g_regex_match_simple("(\\d+(?:\\.\\d+)?) s", set_jstr(rd1, "m", ""), 0, 0)) { GRegex *re = g_regex_new("(\\d+(?:\\.\\d+)?) s", 0, 0, NULL); g_regex_match(re, set_jstr(rd1, "m", ""), 0, &mi); sec = g_match_info_fetch(mi, 1); g_match_info_free(mi); g_regex_unref(re); }
		char *w = when(jnum(rd1, "t")); last = g_strdup_printf("%s%s · %s", sec ? sec : "", sec ? " s to the desktop" : "", w); g_free(w); g_free(sec); } else last = g_strdup("—");
	const char *L[6] = { "Last start-up", "Start-ups this week", "Crashes this week", "Pages closed to free memory", "Update fallbacks", "Failed sign-ins this week" };
	for (int i = 0; i < 6; i++) { char v[32]; snprintf(v, sizeof v, "%d", cnt[i == 0 ? 0 : i - 1]); Cell *c = p_r2(L[i]); c_text(c, i == 0 ? last : v, 400); if (i < 5) p_r2_more(c); }
	g_free(last);
}

/* ================================================================== Power and battery (power31.js pwrPanel): power/set at once */
static RuiJ *PW;
static void on_pset(const char *v, void *d) { char q[120], er[300]; snprintf(q, sizeof q, "power/set?%s=%s", (const char *)d, v); RuiJ *j = post(q, er, sizeof er); if (!j) set_notify("Power and battery", er, 1); rui_jfree(j); redraw(); }
static void on_saver(int on, void *d) { (void)d; on_pset(on ? "1" : "0", "saver_now"); }
static GtkWidget *psel(const char *k, const char *const *v, const char *const *l, int n, int dis, const char *why)
{
	RuiJ *s = rui_jprop(PW, "set"), *x = rui_jprop(s, k); char cur[32] = ""; if (x && rui_jtype(x) == 3) snprintf(cur, sizeof cur, "%s", rui_jsval(x)); else if (x && rui_jtype(x) == 2) snprintf(cur, sizeof cur, "%.0f", rui_jval(x)); else if (x && rui_jtype(x) == 1) snprintf(cur, sizeof cur, "%s", rui_jval(x) ? "true" : "false");
	GtkWidget *w = s_select(v, l, n, cur, on_pset, (void *)g_intern_string(k)); s_select_width(w, 0); if (!set_admin() || dis) gtk_widget_set_sensitive(w, FALSE); if (dis && why) gtk_widget_set_tooltip_text(w, why); char nm[32]; snprintf(nm, sizeof nm, "pw_%s", k); p_label_named(nm, w); return w;
}
/* (pair: "On battery" and "Plugged in" over each select: label flex column, 11 px, gap 8) */
typedef struct { GtkWidget *a, *s[2]; } Pair;
static gboolean pair_draw(GtkWidget *w, cairo_t *cr, gpointer d) { Pair *p = d; (void)w; s_col(cr, "fg", 1); s_text(cr, "On battery", 0, 0, 11, 400, 0, 13); s_text(cr, "Plugged in", p_nat_w(p->s[0]) + 8, 0, 11, 400, 0, 13); return FALSE; }
static double pair_w(GtkWidget *s) { return fmax(p_nat_w(s), tw("On battery", 11, 400)); }
static void pair(Cell *c, const char *k, const char *const *v, const char *const *l, int n, int dis, const char *why)
{
	Pair *p = g_new0(Pair, 1); char kb[32], ka[32]; snprintf(kb, sizeof kb, "%s_bat", k); snprintf(ka, sizeof ka, "%s_ac", k); p->s[0] = psel(kb, v, l, n, dis, why); p->s[1] = psel(ka, v, l, n, dis, why);
	/* (the two selects as wide as the wider: the page's labels are flex items with the same widest option) */
	p->a = gtk_drawing_area_new(); g_signal_connect(p->a, "draw", G_CALLBACK(pair_draw), p); double w0 = pair_w(p->s[0]), w1 = pair_w(p->s[1]);
	g_object_set_data(G_OBJECT(p->s[0]), "set-label", (gpointer)"On battery"); g_object_set_data(G_OBJECT(p->s[1]), "set-label", (gpointer)"Plugged in");   /* (found by Search, as the page's labels) */
	gtk_widget_set_size_request(p->a, (int)lround((w0 + 8 + w1) * s_z()), (int)lround(13 * s_z())); { char nm[32]; snprintf(nm, sizeof nm, "cap_%s", k); p_label_named(nm, p->a); } c_add(c, p->a, 0, 0, 0); c_line(c, 0); c_add(c, p->s[0], 0, 0, 0); c_add(c, p->s[1], 8, 0, 0);
}
static char *bat_time(RuiJ *b)
{
	double n = jnum(b, "energy_now"), f = jnum(b, "energy_full"), r = jnum(b, "power_now"); if (n != n) { n = jnum(b, "charge_now"); f = jnum(b, "charge_full"); r = jnum(b, "current_now"); }
	if (!(r > 0)) return g_strdup(""); const char *st = set_jstr(b, "status", ""); double h = !strcmp(st, "Charging") ? (f - n) / r : !strcmp(st, "Discharging") ? n / r : 0; if (!(h > 0) || h > 48) return g_strdup("");
	int m = (int)lround(h * 60); return m >= 60 ? g_strdup_printf("%d h %d min%s", m / 60, m % 60, !strcmp(st, "Charging") ? " to full" : " left") : g_strdup_printf("%d min%s", m, !strcmp(st, "Charging") ? " to full" : " left");
}
static const char *bat_state(RuiJ *b) { const char *s = set_jstr(b, "status", ""); return !strcmp(s, "Charging") ? "charging" : !strcmp(s, "Discharging") ? "on battery" : !strcmp(s, "Full") ? "full" : !strcmp(s, "Not charging") ? "plugged in, not charging" : s; }
static void power_sec(void)
{
	p_h4_px("Power and battery", 15, 18); rui_jfree(PW); PW = get("power/status");
	if (!PW || !rui_jprop(PW, "set")) { p_msg("The power settings could not be read.", 0); return; }
	RuiJ *s = rui_jprop(PW, "set"), *cn = rui_jprop(PW, "can"), *b = rui_jprop(PW, "battery"), *now = rui_jprop(PW, "now"); int adm = set_admin();
	p_h4("Battery");
	if (b && rui_jtype(b) == 5) { double f = jnum(b, "energy_full"), dz = jnum(b, "energy_full_design"); if (f != f) { f = jnum(b, "charge_full"); dz = jnum(b, "charge_full_design"); }
		char hl[80] = ""; if (f > 0 && dz > 0) snprintf(hl, sizeof hl, " · health %.0f %%", round(f / dz * 100)); double cc = jnum(b, "cycle_count"); if (cc > 0) { size_t l = strlen(hl); snprintf(hl + l, sizeof hl - l, ", %.0f charge cycles", cc); }
		char *bt = bat_time(b), *st = g_markup_escape_text(bat_state(b), -1), *t = g_strdup_printf("\x03%.0f %%, %s%s%s%s%s", jnum(b, "capacity"), st, *bt ? " · " : "", bt, hl, jb(now, "saver") ? " · <b>battery saver is on</b>" : "");
		p_label_named("pwb", p_msg(t, 0)); g_free(t); g_free(bt); g_free(st); }
	else p_msg("This computer has no battery.", 0);
	int noProf = !jb(cn, "profile") && !jb(cn, "epp"); const char *why = noProf ? "This computer offers no power modes to Rudolf OS (no platform profile, no processor energy preference)." : NULL;
	static const char *const MV[3] = { "saver", "balanced", "performance" }, *const ML[3] = { "Power saver", "Balanced", "Performance" };
	Cell *c = p_r2("Power mode"); pair(c, "mode", MV, ML, 3, noProf, why); p_r2_more(c);
	if (noProf) { c = p_r2(""); c_msg(c, why, 0); p_r2_more(c); }
	RuiJ *cc = rui_jprop(cn, "cpu"); int cpuT = jb(cc, "tunable"); const char *drv = set_jstr(cc, "driver", "");
	char *cpuWhy = *drv ? g_strdup_printf("This processor's frequency driver is %s: only AMD's amd_pstate in active mode (amd-pstate-epp) takes these; the power mode applies as before.", drv) : g_strdup("This computer has no processor frequency driver (a virtual machine): the power mode applies as before.");
	static const char *const CV[3] = { "std", "tuned", "passive" }, *const CL[3] = { "As in 1.6.0", "Tuned: saves power on battery", "The kernel's own (amd_pstate passive)" };
	c = p_r2("Processor energy preference"); c_add(c, psel("cpu", CV, CL, 3, !cpuT, cpuWhy), 0, 0, 0);
	c_msg(c, cpuT ? "On AMD processors (amd_pstate, active): with Balanced on battery the processor is asked for balance_power instead of balance_performance. (Game mode asks for performance with any of these but the last: Settings › System health › Memory.) Being measured on laptops (docs/plan-1.6.1.md Part 12)." : cpuWhy, 0); g_free(cpuWhy); p_r2_more(c);
	c = p_r2("Battery saver");
	if (b && rui_jtype(b) == 5) { GtkWidget *k = c_check(c, "On now (until the charger is plugged in)", jb(s, "saver_now"), on_saver, NULL); p_label_named("pwsn", k); if (!adm) gtk_widget_set_sensitive(k, FALSE);
		static const char *const SV[4] = { "0", "10", "20", "30" }, *const SL[4] = { "never", "10 %", "20 %", "30 %" }; c_line(c, 4); c_text(c, "On by itself at ", 400); c_add(c, psel("saver_at", SV, SL, 4, 0, NULL), 0, 0, 0);
		char *m = g_strdup_printf("Power saver mode, the screen dimmer, fewer animations, the screen saver slower, no update checks%s.", jb(cn, "wifiSave") ? ", Wi-Fi power saving" : ""); c_msg(c, m, 0); g_free(m); }
	else c_msg(c, "Needs a battery.", 0);
	p_r2_more(c);
	static const char *const SCV[7] = { "0", "1", "2", "5", "10", "15", "30" }, *const SCL[7] = { "Never", "1 minute", "2 minutes", "5 minutes", "10 minutes", "15 minutes", "30 minutes" };
	static const char *const SLV[6] = { "0", "5", "10", "15", "30", "60" }, *const SLL[6] = { "Never", "5 minutes", "10 minutes", "15 minutes", "30 minutes", "60 minutes" };
	c = p_r2("Turn the screen off after"); pair(c, "screen", SCV, SCL, 7, 0, NULL); p_r2_more(c);
	c = p_r2("Sleep after"); pair(c, "sleep", SLV, SLL, 6, 0, NULL); p_r2_more(c);
	static const char *const LV[3] = { "sleep", "screen", "nothing" }, *const LL[3] = { "Sleep", "Turn the screen off", "Do nothing" };
	c = p_r2("When the lid is closed"); pair(c, "lid", LV, LL, 3, !jb(cn, "lid"), "This computer has no lid switch."); p_r2_more(c);
	static const char *const BV[3] = { "ask", "sleep", "shutdown" }, *const BL[3] = { "Ask what to do", "Sleep", "Shut down" };
	c = p_r2("When the power button is pressed"); { GtkWidget *w = psel("button", BV, BL, 3, 0, NULL); c_add(c, w, 0, 0, 1); } p_r2_more(c);
	static const char *const AV[4] = { "70", "80", "90", "100" }; const char *AL0[4] = { "Stop charging at 70 %", "Stop charging at 80 % (for a laptop that is mostly plugged in)", "Stop charging at 90 %", "Charge to 100 %" }; char *AL[4];
	RuiJ *kept = rui_jprop(PW, "careKept"); for (int i = 0; i < 4; i++) { double kv = jnum(kept, AV[i]); AL[i] = kv == kv && kv != atof(AV[i]) ? g_strdup_printf("%s (this battery keeps %.0f %% instead)", AL0[i], kv) : g_strdup(AL0[i]); }
	c = p_r2("Battery care"); { GtkWidget *w = psel("care", AV, (const char *const *)AL, 4, !jb(cn, "care"), "This battery does not let Rudolf OS limit its charge."); s_select_width(w, 445); c_add(c, w, 0, 0, 0); }   /* (select: max-width 100%) */ for (int i = 0; i < 4; i++) g_free(AL[i]);
	if (!jb(cn, "care")) c_msg(c, "This battery does not let Rudolf OS limit its charge.", 0);
	else { double nc = jnum(now, "care"), sc = jnum(s, "care"); if (nc == nc && nc != sc) { char *m = g_strdup_printf("You chose %.0f %%, but this battery keeps %.0f %%: it takes only some limits and stops where its own firmware chooses.%s", sc, nc, kept && rui_jlen(kept) ? " Choices it does not keep are marked." : ""); c_msg(c, m, 0); g_free(m); } }
	GString *m = g_string_new(adm ? "Changes apply at once and hold for everyone, also before anyone signs in." : "Only administrators can change these.");
	const char *pr = set_jstr(now, "profile", ""), *ep = set_jstr(now, "epp", ""); if (*pr || *ep) { g_string_append(m, " In force: "); if (*pr) g_string_append_printf(m, "platform profile %s", pr); if (*pr && *ep) g_string_append(m, ", "); if (*ep) g_string_append_printf(m, "processor preference %s", ep); g_string_append_c(m, '.'); }
	p_label_named("pwm", p_msg(m->str, 0)); g_string_free(m, TRUE);
}

/* ================================================================== the development link's row, the accounts' Linux users */
static void rows_sec(void)
{
	RuiJ *s = get("devlink"); if (s && jb(s, "available")) { Cell *c; if (!jb(s, "on")) { c = p_r2("Development link"); p_r2_mt(c, 6); c_text(c, "Off", 400); }
		else { char *t = g_strdup_printf("On, %s, %s", set_jstr(s, "repo", ""), jb(s, "change") ? "can change this computer" : "read-only"); c = p_r2("Development link"); p_r2_mt(c, 6); c_text(c, t, 400); g_free(t); } }
	rui_jfree(s);
	RuiJ *u = get("accounts/users"); RuiJ *us = rui_jprop(u, "users");
	if (u && jb(u, "on") && us && rui_jtype(us) == 4) { const char *bad = set_jstr(u, "moveFailed", ""); char *t;
		if (*bad) t = g_strdup_printf("The files of %s could not all be given to its own Linux user: they stay readable to the account, and it is tried again at the next start (see the events).", bad);
		else t = g_strdup_printf("Each account's Linux apps run as its own Linux user (%d account%s), so one account's apps cannot read another's files.", rui_jlen(us), rui_jlen(us) == 1 ? "" : "s");
		Cell *c = p_r2("Accounts' Linux users"); p_r2_mt(c, 6); { GtkWidget *m = c_msg_m(c, t, 0, 0, 0); if (*bad) g_object_set_data(G_OBJECT(m), "set-color", "#b91c1c"); } g_free(t); }   /* (a div: its words wrap) */
	rui_jfree(u);
}

/* ================================================================== Memory (apps57.js memPanel57): memory/settings, the administrator's prompt */
static char *MQ;
static void mem_go(int ok, void *d) { (void)d; if (ok && MQ) { char q[120], er[300]; snprintf(q, sizeof q, "memory/settings?%s", MQ); RuiJ *j = post(q, er, sizeof er); if (j) set_notify("Memory settings saved", !strncmp(MQ, "zram", 4) ? "The new size applies at the next restart." : "", 0); else set_notify("Memory settings", er, 1); rui_jfree(j); } redraw(); }
static void mem_save(const char *q) { g_free(MQ); MQ = g_strdup(q); set_note("elevate Change the memory settings"); rui_elevate(SW, "Change the memory settings", mem_go, NULL); }
static void on_mzs(const char *v, void *d) { (void)d; char q[40]; snprintf(q, sizeof q, "zram=%s", v); mem_save(q); }
static void on_mck(int on, void *d) { char q[40]; snprintf(q, sizeof q, "%s=%d", (const char *)d, on ? 1 : 0); mem_save(q); }
static void mem_sec(void)
{
	RuiJ *m = get("memory/settings"); if (!m) return; int ed = set_admin() && jb(m, "persistent"); RuiJ *cz = rui_jprop(m, "compressed"); double tot = jnum(m, "total"), used = tot - jnum(m, "available");
	p_h4_px("Memory", 15, 18);
	char *a = fmt_b(used), *b = fmt_b(tot), *t = g_strdup_printf("%s of %s (%.0f %%)", a, b, round(used * 100 / tot)); Cell *c = p_r2("In use now"); c_text(c, t, 400); g_free(a); g_free(b); g_free(t); p_r2_more(c);
	if (jb(cz, "on")) { char *x = fmt_b(jnum(cz, "size")), *y = fmt_b(jnum(cz, "stored")), *z = fmt_b(jnum(cz, "used")); double sz = jnum(cz, "size"); char pc[32] = ""; if (sz > 0) snprintf(pc, sizeof pc, " (%.0f %% full)", round(jnum(cz, "stored") * 100 / sz));
		t = g_strdup_printf("%s, holding %s in %s%s", x, y, z, pc); g_free(x); g_free(y); g_free(z); } else t = g_strdup("Off");
	c = p_r2("Compressed memory"); c_text(c, t, 400); g_free(t); p_r2_more(c);
	static const char *const ZV[3] = { "auto", "small", "off" }, *const ZL[3] = { "Automatic (recommended)", "Smaller", "Off" };
	c = p_r2("Size of compressed memory"); GtkWidget *z = s_select(ZV, ZL, 3, set_jstr(m, "zram", "auto"), on_mzs, NULL); s_select_width(z, 0); if (!ed) gtk_widget_set_sensitive(z, FALSE); c_add(c, z, 0, 0, 0); p_label_named("mzs", z);
	t = g_strdup_printf("Automatic: as large as the memory (at most 8 GB). Too small runs out sooner; too large spends the computer's time compressing. A change applies at the next restart.%s", jb(m, "persistent") ? "" : " (Kept only on an installed Rudolf OS.)"); c_msg(c, t, 0); g_free(t); p_r2_more(c);
	c = p_r2("When memory runs out"); GtkWidget *k = c_check(c, "Close apps with no window open (such as Discord in the tray)", jb(m, "closeApps"), on_mck, "closeApps"); if (!ed) gtk_widget_set_sensitive(k, FALSE); p_label_named("mca", k); p_r2_more(c);
	/* (the last 7 days' "memory" events: a list) */
	RuiJ *ev = rui_jprop(HL, "events"); double now = jnum(HL, "now"); GString *ls = g_string_new(NULL); int nl = 0;
	for (int i = 0; ev && i < rui_jlen(ev) && nl < 20; i++) { RuiJ *e = rui_jat(ev, i); if (strcmp(set_jstr(e, "k", ""), "memory") || now - jnum(e, "t") >= 7 * 86400) continue; char *w = when(jnum(e, "t")); const char *msg = set_jstr(e, "m", ""); if (!strncmp(msg, "memory ran out: ", 16)) msg += 16;
		g_string_append_printf(ls, "%s•\t%s: %s", nl ? "\n" : "", w, msg); g_free(w); nl++; }
	c = p_r2("Closed to free memory"); if (nl) { c_text(c, ls->str, 400); GtkWidget *x = NULL; (void)x; } else c_text(c, "Nothing in the last 7 days.", 400); g_string_free(ls, TRUE); p_r2_more(c);
	c = p_r2("Video problems"); k = c_check(c, "Log video problems (the Lite and Native desktops)", jb(m, "videoLog"), on_mck, "videoLog"); if (!ed) gtk_widget_set_sensitive(k, FALSE); p_label_named("mvl", k);
	c_msg(c, "Writes the video and sound system's warnings to a log that the system report includes (on Native, Oracle's video and sound messages too). Starts at the next restart; leave it off unless you are looking into a problem.", 0);
	rui_jfree(m);
}

/* ================================================================== Game mode (gamemode61.js, gamesleep59.js): S.gamemode, S.gmsleep at once */
static void keep(const char *k, int on) { const char *K[1] = { k }, *V[1] = { on ? "true" : "false" }; set_store(K, V, 1); set_note("keep %s %s", k, V[0]); }
static void on_gm(int on, void *d) { (void)d; keep("gamemode", on); char q[40], er[200]; snprintf(q, sizeof q, "game/set?enabled=%d", on ? 1 : 0); RuiJ *j = post(q, er, sizeof er); rui_jfree(j); redraw(); }
static void on_gsl(int on, void *d) { (void)d; keep("gmsleep", on); }
static void game_sec(void)
{
	RuiJ *st = set_stored(); RuiJ *gm = rui_jprop(st, "gamemode"); int gmon = !(gm && rui_jtype(gm) == 1 && !rui_jval(gm)); int gs = jb(st, "gmsleep"); rui_jfree(st);
	Cell *c = p_r2("Game mode"); p_r2_mt(c, 6); GtkWidget *k = c_check(c, "Game mode", gmon, on_gm, NULL); p_label_named("gm61ck", k);
	c_msg(c, "While a game is full screen: the processor runs at full speed (not with battery saver), the desktop's own work waits behind a Linux game, and notifications are held and shown after. Everything goes back as it was when the game leaves full screen or ends. Games that ask for GameMode get the processor's speed only.", 0);
	RuiJ *g = get("game/status"); char *n = !g ? g_strdup("") : !jb(g, "enabled") ? g_strdup("Game mode is off.") : jb(g, "on") ? g_strdup_printf("On now%s.", jb(g, "throttled") ? "; the desktop waits behind the game" : "") : g_strdup(""); c_msg(c, n, 0); g_free(n); rui_jfree(g);
	c_line(c, 0); k = c_check(c, "In game mode, free memory for the game", gs, on_gsl, NULL); p_label_named("gsl59ck", k);
	c_msg(c, "Oracle's background tabs sleep (a tab that is not its window's current one closes its page and keeps its address; selecting it loads it again; tabs playing sound or holding typed text stay awake), and Blitzen's model is unloaded unless Blitzen is in front. Recently used files are let go from memory (the file cache), and updates wait until the game ends.", 0);
}

/* ================================================================== Events: the kinds, the buttons, the list (each with its "⚡ Why?") */
/* (<i style='color:#6b7280'>: a line in italics) */
static gboolean grey_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)d; PangoLayout *l = s_layout(g_object_get_data(G_OBJECT(w), "t"), 13, 400, 0); PangoFontDescription *f = pango_font_description_copy(pango_layout_get_font_description(l) ? pango_layout_get_font_description(l) : pango_context_get_font_description(pango_layout_get_context(l)));
	pango_font_description_set_style(f, PANGO_STYLE_ITALIC); pango_layout_set_font_description(l, f); pango_font_description_free(f); double z = s_z(); cairo_set_source_rgb(cr, 107 / 255.0, 114 / 255.0, 128 / 255.0); cairo_save(cr); cairo_scale(cr, 1 / z, 1 / z); cairo_move_to(cr, 0, (15 - s_lh(13, 400)) / 2 * z); pango_cairo_show_layout(cr, l); cairo_restore(cr); g_object_unref(l); return FALSE; }
static double grey_lay(void *d, double x, double y, double w) { p_place(d, x, y, w, 15); return 15; }
static void p_line_grey(const char *t) { GtkWidget *a = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(a), "t", g_strdup(t), g_free); g_signal_connect(a, "draw", G_CALLBACK(grey_draw), NULL); p_put(a); p_custom(grey_lay, a, 0, 0); }
static void on_kind(const char *v, void *d) { (void)d; g_free(KSEL); KSEL = g_strdup(v); redraw(); }
static void on_refresh(GtkButton *b, gpointer d) { (void)b; (void)d; redraw(); }
static void on_go(GtkButton *b, gpointer d) { (void)b; go(d, NULL); }
static void on_why(GtkButton *b, gpointer d) { (void)b; char a[24]; snprintf(a, sizeof a, "%d", GPOINTER_TO_INT(d)); go("hlwhy", a); }
typedef struct { GtkWidget *a; GPtrArray *rows; } Evs;
typedef struct { const char *icon; char *when, *m; GtkWidget *why; } Ev;
static double evs_lay(void *d, double x, double y, double w)
{
	Evs *E = d; double cy = 0; for (guint i = 0; i < E->rows->len; i++) { Ev *e = g_ptr_array_index(E->rows, i); double h = e->why ? 23 : 22; if (e->why) p_place(e->why, x + 184.2 + tw(e->m, 13, 400) + 6 + 8, y + cy + 4, 0, 15); cy += h; }
	p_place(E->a, x, y, w, MAX(1, cy)); return cy;
}
static gboolean evs_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Evs *E = d; double W = gtk_widget_get_allocated_width(w) / s_z(), cy = 0;
	for (guint i = 0; i < E->rows->len; i++) { Ev *e = g_ptr_array_index(E->rows, i); double h = e->why ? 23 : 22; cairo_set_source_rgba(cr, .5, .5, .5, .15); cairo_rectangle(cr, 0, cy + h - 1, W, 1); cairo_fill(cr);
		s_col(cr, "fg", 1); s_text(cr, e->icon, 0, cy + 3, 13, 400, 0, 15); cairo_set_source_rgb(cr, 107 / 255.0, 114 / 255.0, 128 / 255.0); s_text(cr, e->when, 26.2, cy + 4, 12, 400, 0, 14); s_col(cr, "fg", 1); s_text(cr, e->m, 184.2, cy + 3, 13, 400, W - 184.2, 15); cy += h; }
	return FALSE;
}
static void events_sec(void)
{
	p_h4("Events"); const char *v[32], *l[32]; char *own[32]; int n = 0; v[n] = ""; l[n++] = "All events";
	for (guint i = 0; i < G_N_ELEMENTS(KIND); i++) { v[n] = KIND[i][0]; own[i] = g_strdup_printf("%s %s", KIND[i][1], KIND[i][2]); l[n++] = own[i]; }
	BRow *r = g_new0(BRow, 1); r->gap = 6; GtkWidget *s = s_select(v, l, n, KSEL ? KSEL : "", on_kind, NULL); s_select_width(s, 234);   /* (its widest kind) */ p_put(s); p_label_named("hlk", s); r->b[r->n++] = s; for (guint i = 0; i < G_N_ELEMENTS(KIND); i++) g_free(own[i]);
	const char *B[5] = { "Refresh", "Report a problem…", "⚡ What's wrong with this computer?", "Run the self-test", "Last starts" }, *K[5] = { NULL, "report", "bzall", "selftest", "starts" }, *N[5] = { "hlr", "hlp", "hlw", "hst", "hss" };
	for (int i = 0; i < 5; i++) { GtkWidget *b = rui_button(B[i], 0); g_signal_connect(b, "clicked", i ? G_CALLBACK(on_go) : G_CALLBACK(on_refresh), (gpointer)K[i]); p_put(b); p_label_named(N[i], b); if (i < 2) g_object_set_data(G_OBJECT(b), "h25", GINT_TO_POINTER(1)); r->b[r->n++] = b; }
	p_custom(brow_lay, r, 14, 6);
	if (!HL) { p_line_grey("The health log is kept by the installed Rudolf OS (bootable disc or USB)."); return; }
	RuiJ *ev = rui_jprop(HL, "events"); Evs *E = g_new0(Evs, 1); E->rows = g_ptr_array_new(); E->a = gtk_drawing_area_new(); g_signal_connect(E->a, "draw", G_CALLBACK(evs_draw), E); p_put(E->a); p_label_named("evs", E->a);
	for (int i = 0; ev && i < rui_jlen(ev) && E->rows->len < 300; i++) { RuiJ *x = rui_jat(ev, i); const char *k = set_jstr(x, "k", ""); if (KSEL && *KSEL && strcmp(KSEL, k)) continue;
		Ev *e = g_new0(Ev, 1); e->icon = "•"; for (guint q = 0; q < G_N_ELEMENTS(KIND); q++) if (!strcmp(KIND[q][0], k)) e->icon = KIND[q][1]; e->when = when(jnum(x, "t")); e->m = g_strdup(set_jstr(x, "m", ""));
		/* (bzWhyBtn: the kinds that go wrong, or words that say something did; its kind "error" always shows it) */
		if (!strcmp(k, "crash") || !strcmp(k, "browser") || !strcmp(k, "memory") || !strcmp(k, "fallback") || !strcmp(k, "signin") || g_regex_match_simple("could ?n[o']t|cannot|can't|failed|failure|error|not found|refused|denied|crash|stopped|timed out|timeout|no answer|did not|went wrong|not respond|white screen|restarted", e->m, G_REGEX_CASELESS, 0)) {
			e->why = rui_button("⚡ Why?", 0); gtk_style_context_add_class(gtk_widget_get_style_context(e->why), "set-small"); g_signal_connect(e->why, "clicked", G_CALLBACK(on_why), GINT_TO_POINTER(i)); p_put(e->why); char nm[16]; snprintf(nm, sizeof nm, "why%d", i); p_label_named(nm, e->why); }
		g_ptr_array_add(E->rows, e); }
	if (!E->rows->len) { gtk_widget_destroy(E->a); p_line_grey("Nothing recorded."); return; }
	p_custom(evs_lay, E, 0, 0);
}

/* (a paragraph with the page's <b>: its words laid out with their markup, wrapping in the page's width) */
typedef struct { GtkWidget *a; char *m; } Rich;
static PangoLayout *rich_layout(Rich *r, double w) { PangoLayout *l = s_layout("", 13, 400, w); pango_layout_set_markup(l, r->m, -1); return l; }
static double rich_lay(void *d, double x, double y, double w) { Rich *r = d; PangoLayout *l = rich_layout(r, w); double h = pango_layout_get_line_count(l) * 15; g_object_unref(l); p_place(r->a, x, y, w, h); return h; }
static gboolean rich_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Rich *r = d; double z = s_z(); PangoLayout *l = rich_layout(r, gtk_widget_get_allocated_width(w) / z); s_col(cr, "fg", 1); int n = pango_layout_get_line_count(l); double asc = s_asc(13, 400, NULL);
	for (int i = 0; i < n; i++) { cairo_save(cr); cairo_scale(cr, 1 / z, 1 / z); cairo_move_to(cr, 0, (i * 15 + (15 - s_lh(13, 400)) / 2 + asc) * z); pango_cairo_show_layout_line(cr, pango_layout_get_line_readonly(l, i)); cairo_restore(cr); }
	g_object_unref(l); return FALSE;
}
static void p_rich(const char *m, double mt, double mb) { Rich *r = g_new0(Rich, 1); r->m = g_strdup(m); r->a = gtk_drawing_area_new(); g_signal_connect(r->a, "draw", G_CALLBACK(rich_draw), r); p_put(r->a); p_label_named("rich", r->a); p_custom(rich_lay, r, mt, mb); }
void page_health(void)
{
	rui_jfree(HL); HL = get("health?n=500"); if (HL && !(rui_jprop(HL, "events") && rui_jtype(rui_jprop(HL, "events")) == 4)) { rui_jfree(HL); HL = NULL; }
	p_h3("System health");
	p_rich("What happened on this computer: start-ups, crashes, memory, updates, sign-in failures, the firewall and remote access. It is kept on this computer only and is never sent anywhere. Power and battery is here too, below the summary. <b>Report a problem</b> includes the latest events, after you have read the report.", 0, 10);
	if (HL) summary(); power_sec(); rows_sec(); mem_sec(); game_sec(); events_sec();
}
