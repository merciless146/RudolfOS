/*
 * 1.6.3 (docs/plan-1.6.3.md Part 2's last step): librudolf-ui's administrator prompt takes a finger, as the page's prompt
 * does (rudolf-ui.c's fp_*; core25.js elevate's __elevOpened, fprint63.js's fpLoop). Run with a stand-in rudolfd
 * (fixtures/fpui_rd.py), on a virtual screen: the prompt offers the reader ("Or Ann puts a finger on the reader."), a
 * finger not recognised is said with the tries left and the reader asked again, and a match answers the prompt with the
 * finger's one-time answer ("fp:" and its "t") where the password goes -- which rudolfd takes once, in that account's name
 * only; the password still answers the prompt while a finger waits. The shell test checks that the reader is given up
 * (fp/cancel) when the dialog goes.
 *   fpuiprompt63_t finger|password          (RUI_BASE: the stand-in rudolfd's address; Xvfb)
 * Not shipped. GPL-2.0-or-later.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rudolf-ui.h"

static RuiWin *W; static const char *PHASE = "finger"; static int N = 0, F = 0;
static void ok(const char *what, int c, const char *d) { N++; if (c) printf("PASS %s\n", what); else { printf("FAIL %s - %s\n", what, d ? d : ""); F++; } }
static void report(const char *name, const char *fail) { printf("FAIL %s - %s\n", name, fail); F++; N++; }

/* the dialog's own line (the finger's), as it is on the screen now */
static const char *fline(void)
{
	GtkWidget *b = rui_dialog_box(); static char out[400]; out[0] = 0; if (!b) return out;
	GList *ch = gtk_container_get_children(GTK_CONTAINER(b));
	for (GList *l = ch; l; l = l->next) if (GTK_IS_LABEL(l->data)) { const char *t = gtk_label_get_text(GTK_LABEL(l->data)); if (strstr(t, "finger")) { snprintf(out, sizeof out, "%s", t); break; } }
	g_list_free(ch); return out;
}
static GtkWidget *find_entry(GtkWidget *w)
{
	if (GTK_IS_ENTRY(w)) return w;
	if (GTK_IS_CONTAINER(w)) { GList *ch = gtk_container_get_children(GTK_CONTAINER(w)); for (GList *l = ch; l; l = l->next) { GtkWidget *r = find_entry(l->data); if (r) { g_list_free(ch); return r; } } g_list_free(ch); }
	return NULL;
}
static int admin_post(const char *n, const char *p)
{
	GString *b = g_string_new("{\"n\":\"");
	for (const char *s = n ? n : ""; *s; s++) { unsigned char c = *s; if (c == '"' || c == '\\') g_string_append_printf(b, "\\%c", c); else if (c >= 32) g_string_append_c(b, c); }
	g_string_append(b, "\",\"p\":\"");
	for (const char *s = p ? p : ""; *s; s++) { unsigned char c = *s; if (c == '"' || c == '\\') g_string_append_printf(b, "\\%c", c); else if (c >= 32) g_string_append_c(b, c); }
	g_string_append(b, "\",\"why\":\"the test\"}");
	int c = rui_post("/api/desktop/admin", b->str, NULL, 0); g_string_free(b, TRUE); return c;
}
static void cred(const char *n, const char *p, void *d)
{
	(void)d;
	if (!strcmp(PHASE, "finger")) {
		char want[80]; snprintf(want, sizeof want, "fp:%s", getenv("FPUI_TICKET") ? getenv("FPUI_TICKET") : "");
		ok("the finger answered the prompt: a finger's one-time answer where the password goes", p && !strcmp(p, want), p ? p : "(none)");
		ok("...and in the account signed in's own name", n && !strcmp(n, "Ann"), n ? n : "(none)");
		if (n && p) { int c1 = admin_post(n, p), c2 = admin_post(n, p);
			ok("rudolfd takes it: the request it answers is allowed", c1 == 200, "");
			ok("...once: the same answer again is refused", c2 == 403, ""); }
	} else {
		ok("the password still answers the prompt while a finger waits", p && !strcmp(p, "abcd"), p ? p : "(none)");
		if (n && p) ok("...and rudolfd allows it", admin_post(n, p) == 200, "");
	}
	gtk_main_quit();
}
static gboolean show_prompt(gpointer d) { (void)d; rui_elevate_cred(W, "Change another account's password", cred, NULL); return G_SOURCE_REMOVE; }
static gboolean check_offer(gpointer d) { (void)d; ok("the prompt offers the reader", strstr(fline(), "Or Ann puts a finger on the reader.") != NULL, fline()); return G_SOURCE_REMOVE; }
static gboolean check_left(gpointer d) { (void)d; ok("a finger not recognised: said with the tries left, the reader asked again", strstr(fline(), "not recognised: try again (4 tries left)") != NULL, fline()); return G_SOURCE_REMOVE; }
static gboolean type_password(gpointer d)
{
	(void)d; GtkWidget *b = rui_dialog_box(), *e = b ? find_entry(b) : NULL;
	if (!e) { report("the password answers the prompt", "no field in the prompt"); gtk_main_quit(); return G_SOURCE_REMOVE; }
	gtk_entry_set_text(GTK_ENTRY(e), "abcd"); gtk_widget_activate(e); return G_SOURCE_REMOVE;
}
static gboolean too_long(gpointer d) { (void)d; if (!N) report("the prompt answered", "nothing in 20 s"); else report("the prompt answered", "the answer did not come in 20 s"); gtk_main_quit(); return G_SOURCE_REMOVE; }
int main(int argc, char **argv)
{
	if (argc > 1) PHASE = argv[1];
	if (!rui_init(&argc, &argv, "fpuiprompt")) { printf("FAIL librudolf-ui: no token\n"); return 1; }
	W = rui_window("fpuiprompt", "Administrator permission", 640, 420);
	gtk_widget_show_all(W->win);
	g_timeout_add(300, show_prompt, NULL);
	if (!strcmp(PHASE, "finger")) { g_timeout_add(700, check_offer, NULL); g_timeout_add(1800, check_left, NULL); }
	else if (!strcmp(PHASE, "password")) g_timeout_add(900, type_password, NULL);
	else { printf("usage: fpuiprompt63_t finger|password\n"); return 2; }
	g_timeout_add(20000, too_long, NULL);
	gtk_main();
	printf("%d/%d passed\n", N - F, N);
	return F ? 1 : 0;
}
