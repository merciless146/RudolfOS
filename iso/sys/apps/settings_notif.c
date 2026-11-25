/*
 * rudolf-settings: Notifications (settings25.js notifPanel). 1.6.3, docs/plan-1.6.3.md Part 3 (PR 7). As the page: Do not disturb (dnd),
 * Sounds (nsnd), Show pop-ups for (ntime), each kept at once as persistS keeps them; Send a test notification (the page's notify,
 * through rudolfd as every native app's); Clear history (the account's list, bd95notif:ACCT, emptied as the page's svj does).
 * GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings.h"

static void keep(const char *k, const char *v) { const char *K[1] = { k }, *V[1] = { v }; set_store(K, V, 1); set_note("keep %s %s", k, v); }
static void on_dnd(int on, void *d) { (void)d; keep("dnd", on ? "true" : "false"); }
static void on_snd(int on, void *d) { (void)d; keep("nsnd", on ? "true" : "false"); }
static void on_time(const char *v, void *d) { (void)d; keep("ntime", v); }
static void on_test(GtkButton *b, gpointer d) { (void)b; (void)d; set_notify("This is a test", "Notifications look like this.", 0); }
static void on_clear(GtkButton *b, gpointer d) { (void)b; (void)d; char k[120]; snprintf(k, sizeof k, "bd95notif:%s", ACCT); set_ls_set(k, "[]"); set_note("notif cleared"); }

/* (two inline buttons, a space apart: the page's "<button> <button>") */
static double two_lay(void *d, double x, double y, double w) { (void)w; GtkWidget *b2 = g_object_get_data(G_OBJECT(d), "b2"); double sp, a1; s_lines("a a", 13, 400, 0, &sp); s_lines("aa", 13, 400, 0, &a1); p_place(d, x, y, 0, 0); p_place(b2, x + p_nat_w(d) + (sp - a1), y, 0, 0); return MAX(p_nat_h(d), p_nat_h(b2)); }

void page_notif(void)
{
	RuiJ *st = set_stored(); RuiJ *dnd = rui_jprop(st, "dnd"), *snd = rui_jprop(st, "nsnd"), *tm = rui_jprop(st, "ntime");
	int t = tm && rui_jtype(tm) == 2 ? (int)rui_jval(tm) : 5; char cur[8]; snprintf(cur, sizeof cur, "%d", t ? t : 5);
	p_h3("Notifications");
	Cell *c = p_r2("Do not disturb"); GtkWidget *a = c_check(c, "Hide pop-ups and sounds (errors still show)", dnd && rui_jtype(dnd) == 1 && rui_jval(dnd), on_dnd, NULL); p_label_named("ndn", a); p_r2_more(c);
	c = p_r2("Sounds"); GtkWidget *b = c_check(c, "Play a sound for new notifications", !(snd && rui_jtype(snd) == 1 && !rui_jval(snd)), on_snd, NULL); p_label_named("nsd", b); p_r2_more(c);
	const char *v[4] = { "3", "5", "8", "12" }, *l[4] = { "3 seconds", "5 seconds", "8 seconds", "12 seconds" };
	c = p_r2("Show pop-ups for"); GtkWidget *s = s_select(v, l, 4, cur, on_time, NULL); c_add(c, s, 0, 0, 1); p_label_named("ntm", s);
	{ GtkWidget *b1 = rui_button("Send a test notification", 0), *b2 = rui_button("Clear history", 0); g_signal_connect(b1, "clicked", G_CALLBACK(on_test), NULL); g_signal_connect(b2, "clicked", G_CALLBACK(on_clear), NULL);
		p_put(b1); p_put(b2); p_label_named("nt", b1); p_label_named("nc", b2); g_object_set_data(G_OBJECT(b1), "b2", b2); p_custom(two_lay, b1, 0, 0); }
	rui_jfree(st);
}
