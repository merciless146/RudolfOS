/*
 * librudolf-ui's sample window (1.6.2, docs/plan-1.6.2.md Part 39 step 1): the page's common controls as iso/qt/native/ui_sample.js
 * puts them into a window of the desktop page, drawn by librudolf-ui, for iso/qt/native/ui_t.sh's comparison. Not shipped.
 *   ui-sample --at=X,Y --size=WxH [--show=none|menu|dlg]      RUI_BASE: rudolfd's address; RUI_SAMPLE_RECTS: where to write the
 *   controls' rects ({"body":[x,y,w,h],"rects":{NAME:[x,y,w,h]}}, the body's on the screen, the controls' from the body's corner;
 *   FILE.menu or FILE.dlg for those).
 * GPL-2.0-or-later.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rudolf-ui.h"

static RuiWin *W; static GHashTable *NAMED; static int X = 100, Y = 100, WW = 640, WH = 480; static const char *SHOW = "none";
static void name(const char *n, GtkWidget *w) { g_hash_table_insert(NAMED, (gpointer)n, w); }
static void rect_of(GtkWidget *w, GString *s, const char *n, int *first)
{
	int x = 0, y = 0; gtk_widget_translate_coordinates(w, W->body, 0, 0, &x, &y);
	g_string_append_printf(s, "%s\"%s\":[%d,%d,%d,%d]", *first ? "" : ",", n, x, y, gtk_widget_get_allocated_width(w), gtk_widget_get_allocated_height(w)); *first = 0;
}
static void write_rects(const char *suffix, GString *rects)
{
	const char *f = getenv("RUI_SAMPLE_RECTS"); if (!f) return;
	int bx, by; gdk_window_get_origin(gtk_widget_get_window(W->win), &bx, &by);
	int ox = 0, oy = 0; gtk_widget_translate_coordinates(W->body, W->win, 0, 0, &ox, &oy);
	char *p = g_strdup_printf("%s%s", f, suffix), *t = g_strdup_printf("%s.t", p);
	FILE *o = fopen(t, "w"); if (o) { fprintf(o, "{\"body\":[%d,%d,%d,%d],\"rects\":{%s}}\n", bx + ox, by + oy, gtk_widget_get_allocated_width(W->body), gtk_widget_get_allocated_height(W->body), rects->str); fclose(o); rename(t, p); }
	g_free(p); g_free(t);
}
static gboolean report(gpointer d)
{
	(void)d; GString *s = g_string_new(NULL); int first = 1;
	if (!strcmp(SHOW, "menu")) { GtkWidget *m = rui_menu_window(); if (!m) return G_SOURCE_CONTINUE; int mx, my, bx, by; gdk_window_get_origin(gtk_widget_get_window(m), &mx, &my);
		gdk_window_get_origin(gtk_widget_get_window(W->body), &bx, &by); int ox, oy; gtk_widget_translate_coordinates(W->body, W->win, 0, 0, &ox, &oy);
		g_string_append_printf(s, "\"menu\":[%d,%d,%d,%d]", mx - bx - ox, my - by - oy, gtk_widget_get_allocated_width(m), gtk_widget_get_allocated_height(m)); write_rects(".menu", s); }
	else if (!strcmp(SHOW, "dlg")) { GtkWidget *b = rui_dialog_box(); if (!b) return G_SOURCE_CONTINUE; rect_of(b, s, "dlg", &first); write_rects(".dlg", s); }
	else { GHashTableIter it; gpointer k, v; g_hash_table_iter_init(&it, NAMED); while (g_hash_table_iter_next(&it, &k, &v)) rect_of(v, s, k, &first); write_rects("", s); }
	g_string_free(s, TRUE); return G_SOURCE_REMOVE;
}
static gboolean shown(gpointer d)
{
	(void)d;
	if (!strcmp(SHOW, "menu")) { static const RuiMenuItem M[] = { {"End process", NULL, NULL, 0, 0}, {"End process tree", NULL, NULL, 0, 0}, {NULL, NULL, NULL, 0, 1}, {"End now", NULL, NULL, 1, 0} };
		int bx, by, ox, oy; gdk_window_get_origin(gtk_widget_get_window(W->win), &bx, &by); gtk_widget_translate_coordinates(W->body, W->win, 0, 0, &ox, &oy);
		rui_menu(W->win, bx + ox + 300, by + oy + 200, "chrome (1234)", M, 4, NULL, NULL); }
	if (!strcmp(SHOW, "dlg")) { static const char *const B[] = { "Cancel", "End process" }; rui_confirm(W, "End process", "End chrome (process 1234)?", B, 2, 1, NULL, NULL); }
	g_timeout_add(600, report, NULL); return G_SOURCE_REMOVE;
}
static GtkWidget *row(void) { GtkWidget *r = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (int)rui_px(8)); gtk_widget_set_margin_bottom(r, (int)rui_px(6)); return r; }
int main(int argc, char **argv)
{
	for (int i = 1; i < argc; i++) {
		if (!strncmp(argv[i], "--at=", 5)) sscanf(argv[i] + 5, "%d,%d", &X, &Y);
		else if (!strncmp(argv[i], "--size=", 7)) sscanf(argv[i] + 7, "%dx%d", &WW, &WH);
		else if (!strncmp(argv[i], "--show=", 7)) SHOW = argv[i] + 7;
	}
	rui_init(&argc, &argv, "uisample"); NAMED = g_hash_table_new(g_str_hash, g_str_equal);
	W = rui_window("uisample", "Controls", 100, 100); gtk_window_set_decorated(GTK_WINDOW(W->win), FALSE);
	gtk_window_set_default_size(GTK_WINDOW(W->win), WW, WH); gtk_widget_set_size_request(W->win, WW, WH); gtk_window_move(GTK_WINDOW(W->win), X, Y);
	static const char *const TABS[] = { "Apps", "Processes", "Performance" };
	GtkWidget *tabs = rui_tabs(TABS, 3, NULL, NULL); name("tabs", tabs); gtk_box_pack_start(GTK_BOX(W->body), tabs, FALSE, FALSE, 0);
	GtkWidget *in = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); int pad = (int)rui_px(8); gtk_widget_set_margin_start(in, pad); gtk_widget_set_margin_end(in, pad); gtk_widget_set_margin_top(in, pad);
	gtk_box_pack_start(GTK_BOX(W->body), in, TRUE, TRUE, 0);
	GtkWidget *r1 = row(), *f = rui_entry("Find a process"); gtk_widget_set_size_request(f, (int)rui_px(234), -1); name("field", f);
	GtkWidget *b = rui_button("End process", 0), *bd = rui_button("End now", 0), *p = rui_button("Report a problem…", 1); gtk_widget_set_sensitive(bd, FALSE);
	name("btn", b); name("btnd", bd); name("pri", p);
	gtk_box_pack_start(GTK_BOX(r1), f, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(r1), b, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(r1), bd, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(r1), p, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(in), r1, FALSE, FALSE, 0);
	GtkWidget *r2 = row(), *fv = rui_entry(NULL); gtk_entry_set_text(GTK_ENTRY(fv), "chrome"); gtk_widget_set_size_request(fv, (int)rui_px(234), -1); name("fieldv", fv);
	gtk_box_pack_start(GTK_BOX(r2), fv, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(in), r2, FALSE, FALSE, 0);
	GtkWidget *ck = rui_check("Reopen my apps when I sign in", 1, NULL, NULL); name("check", ck); gtk_box_pack_start(GTK_BOX(in), ck, FALSE, FALSE, 0);
	GtkWidget *h = rui_heading("Apps reopened at sign-in"); name("h4", h); gtk_box_pack_start(GTK_BOX(in), h, FALSE, FALSE, 0);
	RuiTable *t = rui_table_new(3); rui_table_col(t, 0, "Name", RUI_LEFT, 0); rui_table_col(t, 1, "PID", RUI_RIGHT, 0); rui_table_col(t, 2, "Memory", RUI_RIGHT, 0);
	static const char *const R[3][3] = { {"chrome", "1234", "182.4 MB"}, {"rudolfd", "412", "9.8 MB"}, {"Xorg", "388", "61.0 MB"} };
	for (int i = 0; i < 3; i++) rui_table_add(t, R[i], i); rui_table_select(t, 1); rui_table_done(t);
	name("table", rui_table_widget(t)); gtk_box_pack_start(GTK_BOX(in), rui_table_widget(t), FALSE, FALSE, 0);
	GtkWidget *m = rui_msg("3 window(s) · 212 processes · Rudolf OS on this computer"); gtk_widget_set_margin_top(m, (int)rui_px(10)); name("msg", m); gtk_box_pack_start(GTK_BOX(in), m, FALSE, FALSE, 0);
	GtkWidget *lines = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	for (int i = 0; i < 12; i++) { char s[40]; snprintf(s, sizeof s, "Line %d of the list", i + 1); GtkWidget *l = rui_label(s);
		gtk_widget_set_margin_top(l, (int)rui_px(2)); gtk_widget_set_margin_bottom(l, (int)rui_px(2)); gtk_widget_set_margin_start(l, (int)rui_px(6)); gtk_box_pack_start(GTK_BOX(lines), l, FALSE, FALSE, 0); }
	GtkWidget *sc = rui_scroller(lines), *fr = gtk_event_box_new(); gtk_container_add(GTK_CONTAINER(fr), sc); gtk_style_context_add_class(gtk_widget_get_style_context(fr), "rui-frame");
	gtk_widget_set_size_request(fr, -1, (int)rui_px(72)); gtk_widget_set_margin_top(fr, (int)rui_px(8)); name("scroll", fr); gtk_box_pack_start(GTK_BOX(in), fr, FALSE, FALSE, 0);
	gtk_widget_show_all(W->win); gtk_window_move(GTK_WINDOW(W->win), X, Y); gtk_window_set_focus(GTK_WINDOW(W->win), NULL);   /* (nothing focused, as the page's window: the placeholder shown) */
	g_timeout_add(800, shown, NULL);
	gtk_main(); return 0;
}
