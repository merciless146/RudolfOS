/*
 * rudolf-wordpro - WordPro, native (1.6.3, docs/plan-1.6.3.md Part 1), with librudolf-ui.
 * The web WordPro (base_v24.html's wordpro and wp25.js) as a program of its own, one step at a time:
 *   1. the editor and its toolbar: the paragraph styles, the fonts, the size, bold, italic, underline, strikethrough,
 *      sub- and superscript, the text colour and the highlight, the four alignments, bullets, numbering, the indent,
 *      the line spacing, clear formatting, undo and redo, find and replace, and the status line's page, words,
 *      characters and zoom. The toolbar's lists (the styles and the fonts) are read from the page at build time
 *      (mkwordpro.py), so they never drift from it.
 *   2. styles, tables and pictures (with the page's insert dialogs).
 *   3. print, open and save (the page's .DOC: its HTML with the page's own stylesheet), import and export.
 * Until every step is done the native desktop keeps opening the web WordPro (docs/plan-1.6.2.md Part 39: never a window
 * half native, half web); Chromium and Lite keep the web one always.
 *   rudolf-wordpro
 * Built with librudolf-ui (iso/sys/ui/build.sh WORK OUT wordpro). Its test: iso/qt/native/wordpro_t.sh.
 * Tests' hooks: RUDOLF_WP_STATE=FILE (what it shows: the toolbar's choices, the counts, the text and its tags).
 * GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rudolf-ui.h"
#include "wordpro-gen.h"

static RuiWin *W; static GtkWidget *ED, *TB, *FIND, *FI, *REPL, *STAT_PG, *STAT_W, *STAT_C, *STAT_MSG, *ZOOMW;
static GtkTextBuffer *BUF;
static int ZOOM = 100; static char SAY[200]; static const char *FAMILY = "Times New Roman"; static double PT = 12;

/* ------------------------------------------------------------------ tags, and the page's "select some text first" */
static void state_soon(void);
static void counts(void);
/* the page's own values for the parts of WordPro GTK does not draw as it (base_v24.html's .wp-tb, .wp-area, .wp-paper, .wp-sb) */
static void css(void)
{
	static const char *T =
		".wp-tb{background:#f9fafb;border-bottom:1px solid #d1d5db}"
		"button.wp-tb{min-width:26px;padding:0 5px;border:1px solid transparent;background:transparent;border-radius:4px;font-size:13px;color:#111}"
		"button.wp-tb:hover{background:#e0e7ff;border-color:#c7d2fe}"
		"combobox.wp-tb,combobox.wp-tb button{border:1px solid #d1d5db;border-radius:4px;background:#fff;font-size:12px;color:#111}"
		"entry.wp-tb,spinbutton.wp-tb{border:1px solid #d1d5db;border-radius:4px;background:#fff;font-size:12px;color:#111}"
		".wp-sep{background:#d1d5db}"
		".wp-area{background:#e5e7eb}"
		".wp-paper{background:#ffffff;box-shadow:0 1px 3px rgba(0,0,0,0.2)}"
		".wp-sb{background:#f9fafb;border-top:1px solid #d1d5db;font-size:12px;color:#4b5563}";
	GtkCssProvider *p = gtk_css_provider_new();
	gtk_css_provider_load_from_data(p, T, -1, NULL);
	gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(p), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref(p);
}
static GtkTextTag *tag_new(const char *name)
{
	GtkTextTagTable *tt = gtk_text_buffer_get_tag_table(BUF); GtkTextTag *t = gtk_text_tag_table_lookup(tt, name);
	return t ? t : gtk_text_buffer_create_tag(BUF, name, NULL);
}
static void say(const char *m) { snprintf(SAY, sizeof SAY, "%s", m ? m : ""); if (STAT_MSG) gtk_label_set_text(GTK_LABEL(STAT_MSG), SAY); }
static int sel(GtkTextIter *s, GtkTextIter *e) { return gtk_text_buffer_get_selection_bounds(BUF, s, e); }
static int need_sel(void) { GtkTextIter s, e; if (sel(&s, &e)) return 1; say("Select some text first."); state_soon(); return 0; }
static void apply_name(const char *name)
{
	GtkTextIter s, e; if (!need_sel()) return;
	gtk_text_buffer_apply_tag(BUF, tag_new(name), &s, &e); counts(); state_soon();
}
static void apply_str(const char *name, const char *prop, const char *v)
{
	GtkTextIter s, e; if (!need_sel()) return;
	GtkTextTag *t = tag_new(name); g_object_set(G_OBJECT(t), prop, v, NULL); gtk_text_buffer_apply_tag(BUF, t, &s, &e); counts(); state_soon();
}
static void apply_dbl(const char *name, const char *prop, double v)
{
	GtkTextIter s, e; if (!need_sel()) return;
	GtkTextTag *t = tag_new(name); g_object_set(G_OBJECT(t), prop, v, NULL); gtk_text_buffer_apply_tag(BUF, t, &s, &e); counts(); state_soon();
}
static void toggle(GtkWidget *b, gpointer d) { (void)b; apply_name((const char *)d); }

/* ------------------------------------------------------------------ the toolbar's controls */
static void set_editor_font(void)
{
	char f[80]; snprintf(f, sizeof f, "%s %.1f", FAMILY, PT * ZOOM / 100.0);
	PangoFontDescription *d = pango_font_description_from_string(f);
	gtk_widget_override_font(ED, d); pango_font_description_free(d);
}
static void on_colour(GtkColorButton *b, gpointer d)
{
	GdkRGBA c; gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(b), &c);
	char hex[8], n[32]; snprintf(hex, sizeof hex, "#%02x%02x%02x", (int)lround(c.red * 255), (int)lround(c.green * 255), (int)lround(c.blue * 255));
	snprintf(n, sizeof n, "%s:%s", (const char *)d, hex);
	apply_str(n, !strcmp((const char *)d, "bg") ? "background" : "foreground", hex);
}
static void on_family(GtkComboBox *c, gpointer d)
{
	(void)d; int i = gtk_combo_box_get_active(c); if (i < 0 || i >= NWPFONTS) return;
	char n[96]; snprintf(n, sizeof n, "ff:%s", WP_FONTS[i]); apply_str(n, "family", WP_FONTS[i]);
}
static void on_size(GtkWidget *w, gpointer d)
{
	(void)d; double pt = atof(gtk_entry_get_text(GTK_ENTRY(w)));
	if (pt < 1) pt = 1; if (pt > 400) pt = 400; PT = pt;
	char n[40]; snprintf(n, sizeof n, "sz:%.1f", pt); apply_dbl(n, "size-points", pt);
}
static void inc_size(GtkWidget *b, gpointer d)
{
	(void)b; double s = PT >= 72 ? 8 : PT >= 28 ? 4 : PT >= 12 ? 2 : 1; PT += d ? -s : s;
	if (PT < 1) PT = 1; if (PT > 400) PT = 400;
	GtkWidget *e = g_object_get_data(G_OBJECT(TB), "size"); char t[16]; snprintf(t, sizeof t, "%.0f", PT);
	gtk_entry_set_text(GTK_ENTRY(e), t);
	char n[40]; snprintf(n, sizeof n, "sz:%.1f", PT); apply_dbl(n, "size-points", PT);
}
static void on_style(GtkComboBox *c, gpointer d)
{
	(void)d; int i = gtk_combo_box_get_active(c); if (i < 0 || i >= NWPSTYLES) return;
	GtkTextIter s, e; if (!need_sel()) return;
	static const struct { double pt; int bold, italic; int just; } ST[] = {
		{ 12, 0, 0, GTK_JUSTIFY_LEFT }, { 20, 1, 0, GTK_JUSTIFY_LEFT }, { 16, 1, 0, GTK_JUSTIFY_LEFT }, { 14, 1, 0, GTK_JUSTIFY_LEFT },
		{ 12, 1, 0, GTK_JUSTIFY_LEFT }, { 26, 1, 0, GTK_JUSTIFY_CENTER }, { 14, 0, 1, GTK_JUSTIFY_CENTER }, { 12, 0, 1, GTK_JUSTIFY_LEFT },
		{ 11, 0, 0, GTK_JUSTIFY_LEFT } };
	int n = (int)(sizeof ST / sizeof ST[0]); if (i >= n) i = n - 1;
	char nm[48]; snprintf(nm, sizeof nm, "st:%s", WP_STYLES[i].k); GtkTextTag *t = tag_new(nm);
	g_object_set(G_OBJECT(t), "size-points", ST[i].pt, "weight", ST[i].bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
		"style", ST[i].italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL, "justification", ST[i].just, NULL);
	if (!strcmp(WP_STYLES[i].k, "code")) g_object_set(G_OBJECT(t), "family", "monospace", NULL);
	gtk_text_buffer_apply_tag(BUF, t, &s, &e); counts(); state_soon();
}
static void on_align(GtkWidget *b, gpointer d)
{
	(void)b; GtkTextIter s, e; if (!need_sel()) return;
	const char *k = (const char *)d;
	char n[24]; snprintf(n, sizeof n, "al:%s", k);
	GtkTextTag *t = tag_new(n);
	g_object_set(G_OBJECT(t), "justification", !strcmp(k, "center") ? GTK_JUSTIFY_CENTER : !strcmp(k, "right") ? GTK_JUSTIFY_RIGHT :
		!strcmp(k, "fill") ? GTK_JUSTIFY_FILL : GTK_JUSTIFY_LEFT, NULL);
	gtk_text_buffer_apply_tag(BUF, t, &s, &e); counts(); state_soon();
}
/* bullets, numbering and the indent: the page's block commands, on each line of the choice */
static void line_prefix(const char *kind)
{
	GtkTextIter s, e; if (!need_sel()) return;
	int end = gtk_text_iter_get_offset(&e); gtk_text_iter_set_line_offset(&s, 0); int n = 0;
	for (;;) {
		GtkTextIter line = s;
		if (!strcmp(kind, "ind") || !strcmp(kind, "out")) {
			gtk_text_iter_set_line_offset(&line, 0);
			if (!strcmp(kind, "ind")) gtk_text_buffer_insert(BUF, &line, "    ", -1);
			else {
				GtkTextIter t = line; int k = 0;
				while (k < 4 && gtk_text_iter_get_char(&t) == ' ') { gtk_text_iter_forward_char(&t); k++; }
				if (k) gtk_text_buffer_delete(BUF, &line, &t);
			}
		} else {
			char m[16];
			if (!strcmp(kind, "num")) snprintf(m, sizeof m, "%d. ", ++n); else snprintf(m, sizeof m, "• ");
			gtk_text_buffer_insert(BUF, &line, m, -1);
		}
		if (!gtk_text_iter_forward_line(&s) || gtk_text_iter_get_offset(&s) > end) break;
	}
	counts(); state_soon();
}
static void on_list(GtkWidget *b, gpointer d) { (void)b; line_prefix((const char *)d); }
static void on_spacing(GtkComboBox *c, gpointer d)
{
	(void)d; int i = gtk_combo_box_get_active(c); if (i <= 0) return;
	static const double V[] = { 0, 1.0, 1.15, 1.5, 2.0, 2.5 }; GtkTextIter s, e; if (!need_sel()) return;
	char n[32]; snprintf(n, sizeof n, "ls:%.2f", V[i]); GtkTextTag *t = tag_new(n);
	int pad = (int)lround((V[i] - 1) * PT * 0.66);
	g_object_set(G_OBJECT(t), "pixels-above-lines", pad, "pixels-below-lines", pad, NULL);
	gtk_text_buffer_apply_tag(BUF, t, &s, &e); counts(); state_soon();
}
static void on_clear(GtkWidget *b, gpointer d)
{
	(void)b; (void)d; GtkTextIter s, e; if (!need_sel()) return;
	gtk_text_buffer_remove_all_tags(BUF, &s, &e); counts(); state_soon();
}
/* undo and redo: GTK 3's text buffer keeps no history of its own, so the writing is kept step by step (the page's undo and
   redo, bounded to the last 120 changes) */
#define HIST 120
static char *HIST_T[HIST]; static int HIST_N, HIST_I, SILENT;
static void hist_apply(const char *t) { SILENT = 1; gtk_text_buffer_set_text(BUF, t, -1); SILENT = 0; counts(); state_soon(); }
static void hist_undo(void) { if (HIST_I > 1) { HIST_I--; hist_apply(HIST_T[HIST_I - 1]); } }
static void hist_redo(void) { if (HIST_I < HIST_N) { HIST_I++; hist_apply(HIST_T[HIST_I - 1]); } }
static void on_undo(GtkWidget *b, gpointer d) { (void)b; (void)d; hist_undo(); }
static void on_redo(GtkWidget *b, gpointer d) { (void)b; (void)d; hist_redo(); }
static void on_zoom(GtkComboBox *c, gpointer d)
{
	(void)d; static const int Z[] = { 50, 60, 70, 80, 90, 100, 110, 125, 150, 175, 200 };
	int i = gtk_combo_box_get_active(c); if (i < 0) return; ZOOM = Z[i]; set_editor_font(); state_soon();
}

/* ------------------------------------------------------------------ find and replace (the page's Find bar) */
static void reveal(GtkTextIter *a, GtkTextIter *b)
{
	gtk_text_buffer_select_range(BUF, a, b);
	GtkTextMark *m = gtk_text_buffer_create_mark(BUF, NULL, a, 0);
	gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(ED), m); gtk_text_buffer_delete_mark(BUF, m);
}
static void find_next(void)
{
	const char *n = gtk_entry_get_text(GTK_ENTRY(FI)); if (!*n) { say(""); return; }
	GtkTextIter at, a, b; GtkTextIter s, e;
	if (sel(&s, &e)) at = e; else gtk_text_buffer_get_start_iter(BUF, &at);
	if (!gtk_text_iter_forward_search(&at, n, 0, &a, &b, NULL)) {
		gtk_text_buffer_get_start_iter(BUF, &at);
		if (!gtk_text_iter_forward_search(&at, n, 0, &a, &b, NULL)) { say("Not found."); state_soon(); return; }
	}
	reveal(&a, &b); state_soon();
}
static void find_prev(void)
{
	const char *n = gtk_entry_get_text(GTK_ENTRY(FI)); if (!*n) return;
	GtkTextIter s, e, at, a, b; int have = 0; GtkTextIter ha = { 0 }, hb = { 0 };
	gtk_text_buffer_get_start_iter(BUF, &at);
	if (sel(&s, &e)) { GtkTextIter stop = s; gtk_text_buffer_get_start_iter(BUF, &at);
		while (gtk_text_iter_forward_search(&at, n, 0, &a, &b, NULL) && gtk_text_iter_compare(&a, &stop) < 0) { ha = a; hb = b; have = 1; at = b; } }
	else while (gtk_text_iter_forward_search(&at, n, 0, &a, &b, NULL)) { ha = a; hb = b; have = 1; at = b; }
	if (!have) { say("Not found."); state_soon(); return; }
	reveal(&ha, &hb); state_soon();
}
static void replace_all(void)
{
	const char *n = gtk_entry_get_text(GTK_ENTRY(FI)); const char *r = gtk_entry_get_text(GTK_ENTRY(REPL));
	if (!*n) return;
	GtkTextIter s, a, b; gtk_text_buffer_get_start_iter(BUF, &s); int k = 0;
	while (gtk_text_iter_forward_search(&s, n, 0, &a, &b, NULL)) {
		gtk_text_buffer_delete(BUF, &a, &b); gtk_text_buffer_insert(BUF, &a, r, -1); s = a; k++;
	}
	char m[64]; snprintf(m, sizeof m, "%d replaced.", k); say(m); counts(); state_soon();
}
static void on_find_open(GtkWidget *b, gpointer d) { (void)b; (void)d; gtk_widget_show_all(FIND); gtk_widget_grab_focus(FI); state_soon(); }
static void on_find_close(GtkWidget *b, gpointer d) { (void)b; (void)d; gtk_widget_hide(FIND); gtk_widget_grab_focus(ED); state_soon(); }

/* ------------------------------------------------------------------ the counts, and the state the tests read */
static char *doc_text(void) { GtkTextIter s, e; gtk_text_buffer_get_start_iter(BUF, &s); gtk_text_buffer_get_end_iter(BUF, &e); return gtk_text_buffer_get_text(BUF, &s, &e, FALSE); }
static int words_of(const char *t) { int n = 0, in = 0; for (const char *p = t; *p; p++) { if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') in = 0; else if (!in) { in = 1; n++; } } return n; }
static int chars_of(const char *t) { int n = 0; for (const char *p = t; *p; p++) if (*p != '\n' && *p != '\r') n++; return n; }
static void counts(void)
{
	char *t = doc_text(); char b[96];
	snprintf(b, sizeof b, "%d %s", words_of(t), rui_tr("words")); gtk_label_set_text(GTK_LABEL(STAT_W), b);
	snprintf(b, sizeof b, "%d %s", chars_of(t), rui_tr("characters")); gtk_label_set_text(GTK_LABEL(STAT_C), b);
	g_free(t);
}
static void state_write(void)
{
	const char *f = getenv("RUDOLF_WP_STATE"); if (!f || !BUF) return;
	GString *s = g_string_new(NULL); char *t = doc_text();
	GtkTextIter a, b; int has = sel(&a, &b);
	g_string_append_printf(s, "font %s\nsize %.1f\nzoom %d\nwords %d\nchars %d\nsel %d\nsay %s\nstyles %d\nfonts %d\n",
		FAMILY, PT, ZOOM, words_of(t), chars_of(t), has, SAY, NWPSTYLES, NWPFONTS);
	gtk_text_buffer_get_start_iter(BUF, &a); gtk_text_buffer_get_end_iter(BUF, &b);
	g_string_append(s, "tags");
	char last[200] = "";
	for (GtkTextIter i = a; gtk_text_iter_compare(&i, &b) < 0; gtk_text_iter_forward_line(&i)) {
		GtkTextIter j = i; char line[200] = "";
		for (; !gtk_text_iter_is_end(&j) && gtk_text_iter_get_char(&j) != '\n'; gtk_text_iter_forward_char(&j)) {
			GSList *l = gtk_text_iter_get_tags(&j);
			for (GSList *k = l; k; k = k->next) {
				char *nm = NULL; g_object_get(G_OBJECT(k->data), "name", &nm, NULL);
				if (!nm || strstr(line, nm)) { g_free(nm); continue; }
				const char *n = nm;
				size_t at = strlen(line); snprintf(line + at, sizeof line - at, "%s%s", at ? "," : "", n);
				g_free(nm);
			}
			g_slist_free(l);
		}
		if (*line && strcmp(line, last)) { g_string_append_printf(s, " %s", line); snprintf(last, sizeof last, "%s", line); }
	}
	g_string_append_c(s, '\n');
	g_string_append_printf(s, "text %s\n", t); g_free(t);
	char *tmp = g_strdup_printf("%s.t", f); g_file_set_contents(tmp, s->str, -1, NULL); rename(tmp, f); g_free(tmp); g_string_free(s, TRUE);
}
static gboolean state_tick(gpointer d) { (void)d; state_write(); return G_SOURCE_REMOVE; }
static void state_soon(void) { if (getenv("RUDOLF_WP_STATE")) g_timeout_add(120, state_tick, NULL); }
static void hist_push(void)
{
	if (SILENT) return;
	char *t = doc_text();
	if (HIST_N && HIST_T[HIST_N - 1] && !strcmp(HIST_T[HIST_N - 1], t)) { g_free(t); return; }
	if (HIST_I < HIST_N) { for (int i = HIST_I; i < HIST_N; i++) { g_free(HIST_T[i]); HIST_T[i] = NULL; } HIST_N = HIST_I; }
	if (HIST_N == HIST) { g_free(HIST_T[0]); memmove(HIST_T, HIST_T + 1, sizeof(HIST_T[0]) * (HIST - 1)); HIST_N--; }
	HIST_T[HIST_N++] = t; HIST_I = HIST_N;
}
static void on_text(GtkTextBuffer *b, gpointer d) { (void)b; (void)d; hist_push(); counts(); state_soon(); }
static void on_mark(GtkTextBuffer *b, GtkTextIter *i, GtkTextMark *m, gpointer d) { (void)b; (void)i; (void)m; (void)d; counts(); state_soon(); }

/* ------------------------------------------------------------------ the toolbar, drawn as the page's .wp-tb */
static GtkWidget *tb_button(const char *label, const char *tip, GCallback cb, const char *d)
{
	GtkWidget *b = gtk_button_new(); GtkWidget *l = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(l), label); gtk_container_add(GTK_CONTAINER(b), l);
	gtk_style_context_add_class(gtk_widget_get_style_context(b), "rui");
	gtk_widget_set_tooltip_text(b, rui_tr(tip));
	if (cb) g_signal_connect(b, "clicked", cb, (gpointer)d);
	gtk_style_context_add_class(gtk_widget_get_style_context(b), "wp-tb");
	gtk_widget_set_size_request(b, (int)lround(rui_px(26)), (int)lround(rui_px(26)));
	return b;
}
static void tb_add(GtkWidget *fb, GtkWidget *w) { gtk_flow_box_insert(GTK_FLOW_BOX(fb), w, -1); }
static void tb_sep(GtkWidget *fb)
{
	GtkWidget *s = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_size_request(s, (int)lround(rui_px(1)), (int)lround(rui_px(20)));
	gtk_widget_set_margin_start(s, (int)lround(rui_px(3))); gtk_widget_set_margin_end(s, (int)lround(rui_px(3)));
	GtkStyleContext *c = gtk_widget_get_style_context(s); gtk_style_context_add_class(c, "wp-sep");
	gtk_flow_box_insert(GTK_FLOW_BOX(fb), s, -1);
}
static void build_toolbar(void)
{
	TB = gtk_flow_box_new();
	gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(TB), GTK_SELECTION_NONE);
	gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(TB), (int)lround(rui_px(3)));
	gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(TB), (int)lround(rui_px(3)));
	gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(TB), 40);
	gtk_widget_set_margin_start(TB, (int)lround(rui_px(6))); gtk_widget_set_margin_end(TB, (int)lround(rui_px(6)));
	gtk_widget_set_margin_top(TB, (int)lround(rui_px(4))); gtk_widget_set_margin_bottom(TB, (int)lround(rui_px(4)));
	gtk_style_context_add_class(gtk_widget_get_style_context(TB), "wp-tb");

	GtkWidget *st = gtk_combo_box_text_new();
	for (int i = 0; i < NWPSTYLES; i++) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st), rui_tr(WP_STYLES[i].name));
	gtk_widget_set_tooltip_text(st, rui_tr("Paragraph style"));
	gtk_widget_set_size_request(st, (int)lround(rui_px(104)), (int)lround(rui_px(26)));
	gtk_combo_box_set_active(GTK_COMBO_BOX(st), 0); g_signal_connect(st, "changed", G_CALLBACK(on_style), NULL);
	gtk_style_context_add_class(gtk_widget_get_style_context(st), "wp-tb"); tb_add(TB, st);

	GtkWidget *ff = gtk_combo_box_text_new();
	for (int i = 0; i < NWPFONTS; i++) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ff), WP_FONTS[i]);
	gtk_widget_set_tooltip_text(ff, rui_tr("Font"));
	gtk_widget_set_size_request(ff, (int)lround(rui_px(128)), (int)lround(rui_px(26)));
	gtk_combo_box_set_active(GTK_COMBO_BOX(ff), 0); g_signal_connect(ff, "changed", G_CALLBACK(on_family), NULL);
	gtk_style_context_add_class(gtk_widget_get_style_context(ff), "wp-tb"); tb_add(TB, ff);

	tb_add(TB, tb_button("−", "Smaller", G_CALLBACK(inc_size), "down"));
	{ GtkWidget *sz = gtk_entry_new(); gtk_entry_set_text(GTK_ENTRY(sz), "12"); gtk_entry_set_width_chars(GTK_ENTRY(sz), 3);
	  gtk_entry_set_max_length(GTK_ENTRY(sz), 3); gtk_entry_set_alignment(GTK_ENTRY(sz), 0.5);
	  gtk_widget_set_tooltip_text(sz, rui_tr("Font size (pt)")); gtk_widget_set_size_request(sz, (int)lround(rui_px(34)), (int)lround(rui_px(26)));
	  g_signal_connect(sz, "activate", G_CALLBACK(on_size), NULL); tb_add(TB, sz); g_object_set_data(G_OBJECT(TB), "size", sz); }
	tb_add(TB, tb_button("+", "Larger", G_CALLBACK(inc_size), NULL));
	tb_sep(TB);
	tb_add(TB, tb_button("<b>B</b>", "Bold (Ctrl+B)", G_CALLBACK(toggle), "b"));
	tb_add(TB, tb_button("<i>I</i>", "Italic (Ctrl+I)", G_CALLBACK(toggle), "i"));
	tb_add(TB, tb_button("<u>U</u>", "Underline (Ctrl+U)", G_CALLBACK(toggle), "u"));
	tb_add(TB, tb_button("<s>S</s>", "Strikethrough", G_CALLBACK(toggle), "s"));
	tb_add(TB, tb_button("x₂", "Subscript", G_CALLBACK(toggle), "sub"));
	tb_add(TB, tb_button("x²", "Superscript", G_CALLBACK(toggle), "sup"));
	tb_sep(TB);
	{ GtkWidget *cb = gtk_color_button_new_with_rgba(&(GdkRGBA){ 0.752941, 0, 0, 1 });
	  gtk_widget_set_tooltip_text(cb, rui_tr("Text color")); gtk_color_button_set_title(GTK_COLOR_BUTTON(cb), rui_tr("Text color"));
	  gtk_widget_set_size_request(cb, (int)lround(rui_px(26)), (int)lround(rui_px(26)));
	  g_signal_connect(cb, "color-set", G_CALLBACK(on_colour), (gpointer)"fg"); tb_add(TB, cb); }
	{ GtkWidget *cb = gtk_color_button_new_with_rgba(&(GdkRGBA){ 1, 1, 0, 1 });
	  gtk_widget_set_tooltip_text(cb, rui_tr("Highlight")); gtk_color_button_set_title(GTK_COLOR_BUTTON(cb), rui_tr("Highlight"));
	  gtk_widget_set_size_request(cb, (int)lround(rui_px(26)), (int)lround(rui_px(26)));
	  g_signal_connect(cb, "color-set", G_CALLBACK(on_colour), (gpointer)"bg"); tb_add(TB, cb); }
	tb_sep(TB);
	tb_add(TB, tb_button("⬅", "Align left", G_CALLBACK(on_align), "left"));
	tb_add(TB, tb_button("↔", "Center", G_CALLBACK(on_align), "center"));
	tb_add(TB, tb_button("➡", "Align right", G_CALLBACK(on_align), "right"));
	tb_add(TB, tb_button("☰", "Justify", G_CALLBACK(on_align), "fill"));
	tb_sep(TB);
	tb_add(TB, tb_button("• ≡", "Bullets", G_CALLBACK(on_list), "bul"));
	tb_add(TB, tb_button("1. ≡", "Numbering", G_CALLBACK(on_list), "num"));
	tb_add(TB, tb_button("⇤", "Decrease indent", G_CALLBACK(on_list), "out"));
	tb_add(TB, tb_button("⇥", "Increase indent", G_CALLBACK(on_list), "ind"));
	{ GtkWidget *ls = gtk_combo_box_text_new();
	  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ls), rui_tr("Spacing"));
	  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ls), "1"); gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ls), "1.15");
	  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ls), "1.5"); gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ls), "2");
	  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ls), "2.5");
	  gtk_widget_set_tooltip_text(ls, rui_tr("Line spacing"));
	  gtk_widget_set_size_request(ls, (int)lround(rui_px(76)), (int)lround(rui_px(26)));
	  gtk_combo_box_set_active(GTK_COMBO_BOX(ls), 0); g_signal_connect(ls, "changed", G_CALLBACK(on_spacing), NULL);
	  gtk_style_context_add_class(gtk_widget_get_style_context(ls), "wp-tb"); tb_add(TB, ls); }
	tb_sep(TB);
	tb_add(TB, tb_button("⌧", "Clear formatting", G_CALLBACK(on_clear), NULL));
	tb_add(TB, tb_button("↶", "Undo (Ctrl+Z)", G_CALLBACK(on_undo), NULL));
	tb_add(TB, tb_button("↷", "Redo (Ctrl+Y)", G_CALLBACK(on_redo), NULL));
	tb_add(TB, tb_button("🔍", "Find (Ctrl+F)", G_CALLBACK(on_find_open), NULL));
	gtk_box_pack_start(GTK_BOX(W->body), TB, FALSE, FALSE, 0);
}
static void build_find(void)
{
	FIND = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (int)lround(rui_px(4)));
	gtk_widget_set_margin_start(FIND, (int)lround(rui_px(6))); gtk_widget_set_margin_end(FIND, (int)lround(rui_px(6)));
	gtk_widget_set_margin_top(FIND, (int)lround(rui_px(4))); gtk_widget_set_margin_bottom(FIND, (int)lround(rui_px(4)));
	FI = rui_entry("Find"); gtk_widget_set_size_request(FI, (int)lround(rui_px(180)), -1); gtk_box_pack_start(GTK_BOX(FIND), FI, FALSE, FALSE, 0);
	REPL = rui_entry("Replace with"); gtk_widget_set_size_request(REPL, (int)lround(rui_px(180)), -1); gtk_box_pack_start(GTK_BOX(FIND), REPL, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(FIND), tb_button("▲", "Previous", G_CALLBACK(find_prev), NULL), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(FIND), tb_button("▼", "Next", G_CALLBACK(find_next), NULL), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(FIND), tb_button("Replace all", "Replace all", G_CALLBACK(replace_all), NULL), FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(FIND), tb_button("✕", "Close", G_CALLBACK(on_find_close), NULL), FALSE, FALSE, 0);
	g_signal_connect(FI, "activate", G_CALLBACK(find_next), NULL);
	gtk_box_pack_start(GTK_BOX(W->body), FIND, FALSE, FALSE, 0);
}
static void build_editor(void)
{
	GtkWidget *area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_style_context_add_class(gtk_widget_get_style_context(area), "wp-area");
	GtkWidget *paper = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_style_context_add_class(gtk_widget_get_style_context(paper), "wp-paper");
	gtk_widget_set_halign(paper, GTK_ALIGN_CENTER); gtk_widget_set_valign(paper, GTK_ALIGN_START);
	gtk_widget_set_size_request(paper, (int)lround(rui_px(816)), -1);
	gtk_widget_set_margin_top(paper, (int)lround(rui_px(16))); gtk_widget_set_margin_bottom(paper, (int)lround(rui_px(24)));
	gtk_widget_set_margin_start(paper, (int)lround(rui_px(24))); gtk_widget_set_margin_end(paper, (int)lround(rui_px(24)));
	ED = gtk_text_view_new(); BUF = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ED));
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(ED), GTK_WRAP_WORD_CHAR);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(ED), (int)lround(rui_px(96)));
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(ED), (int)lround(rui_px(96)));
	gtk_text_view_set_top_margin(GTK_TEXT_VIEW(ED), (int)lround(rui_px(96)));
	gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(ED), (int)lround(rui_px(96)));
	HIST_T[0] = g_strdup(""); HIST_N = 1; HIST_I = 1;   /* (the history undo and redo walk, from the empty page) */
	gtk_text_buffer_create_tag(BUF, "b", "weight", PANGO_WEIGHT_BOLD, NULL);
	gtk_text_buffer_create_tag(BUF, "i", "style", PANGO_STYLE_ITALIC, NULL);
	gtk_text_buffer_create_tag(BUF, "u", "underline", PANGO_UNDERLINE_SINGLE, NULL);
	gtk_text_buffer_create_tag(BUF, "s", "strikethrough", TRUE, NULL);
	gtk_text_buffer_create_tag(BUF, "sub", "rise", -4000, "size-points", 9.0, NULL);
	gtk_text_buffer_create_tag(BUF, "sup", "rise", 6000, "size-points", 9.0, NULL);
	gtk_text_buffer_create_tag(BUF, "find", "background", "#fde68a", NULL);
	set_editor_font();
	gtk_container_add(GTK_CONTAINER(paper), ED);
	gtk_box_pack_start(GTK_BOX(area), paper, TRUE, TRUE, 0);
	GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scroll), area);
	gtk_box_pack_start(GTK_BOX(W->body), scroll, TRUE, TRUE, 0);
	g_signal_connect(BUF, "changed", G_CALLBACK(on_text), NULL);
	g_signal_connect(BUF, "mark-set", G_CALLBACK(on_mark), NULL);
}
static void build_status(void)
{
	GtkWidget *sb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (int)lround(rui_px(14)));
	gtk_style_context_add_class(gtk_widget_get_style_context(sb), "wp-sb");
	gtk_widget_set_margin_start(sb, (int)lround(rui_px(8))); gtk_widget_set_margin_end(sb, (int)lround(rui_px(8)));
	gtk_widget_set_margin_top(sb, (int)lround(rui_px(4))); gtk_widget_set_margin_bottom(sb, (int)lround(rui_px(4)));
	STAT_PG = gtk_label_new(rui_tr("Page 1 of 1")); STAT_W = gtk_label_new("0"); STAT_C = gtk_label_new("0");
	STAT_MSG = gtk_label_new(""); gtk_label_set_ellipsize(GTK_LABEL(STAT_MSG), PANGO_ELLIPSIZE_END);
	gtk_widget_set_hexpand(STAT_MSG, TRUE); gtk_label_set_xalign(GTK_LABEL(STAT_MSG), 1);
	gtk_box_pack_start(GTK_BOX(sb), STAT_PG, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(sb), STAT_W, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(sb), STAT_C, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(sb), STAT_MSG, TRUE, TRUE, 0);
	ZOOMW = gtk_combo_box_text_new();
	{ static const char *V[] = { "50%", "60%", "70%", "80%", "90%", "100%", "110%", "125%", "150%", "175%", "200%" };
	  for (unsigned i = 0; i < G_N_ELEMENTS(V); i++) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ZOOMW), V[i]);
	  gtk_combo_box_set_active(GTK_COMBO_BOX(ZOOMW), 5); gtk_widget_set_tooltip_text(ZOOMW, rui_tr("Zoom"));
	  gtk_widget_set_size_request(ZOOMW, (int)lround(rui_px(72)), -1);
	  g_signal_connect(ZOOMW, "changed", G_CALLBACK(on_zoom), NULL); }
	gtk_box_pack_start(GTK_BOX(sb), ZOOMW, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(W->body), sb, FALSE, FALSE, 0);
}
/* the page's keys (Ctrl+B, I, U, Z, Y, F). An accelerator group is used, not a key handler on the window: the writing
   takes Ctrl+B and Ctrl+I itself (they move the cursor, as every GTK text view does), so a handler there never saw them. */
static void accel(GtkAccelGroup *g, GObject *o, guint kv, GdkModifierType m, gpointer d)
{
	(void)g; (void)o; (void)kv; (void)m;
	switch (GPOINTER_TO_INT(d)) {
	case 'b': apply_name("b"); break;
	case 'i': apply_name("i"); break;
	case 'u': apply_name("u"); break;
	case 'z': hist_undo(); break;
	case 'y': hist_redo(); break;
	case 'f': gtk_widget_show_all(FIND); gtk_widget_grab_focus(FI); break;
	default: break;
	}
}
int main(int argc, char **argv)
{
	rui_init(&argc, &argv, "wordpro");
	css();
	W = rui_window("wordpro", "WordPro", 980, 646);
	build_find(); build_toolbar(); build_editor(); build_status();
	{ GtkAccelGroup *ag = gtk_accel_group_new();
	  static const struct { guint kv; int k; } K[] = { { GDK_KEY_b, 'b' }, { GDK_KEY_i, 'i' }, { GDK_KEY_u, 'u' }, { GDK_KEY_z, 'z' }, { GDK_KEY_y, 'y' }, { GDK_KEY_f, 'f' } };
	  for (unsigned i = 0; i < G_N_ELEMENTS(K); i++)
		  gtk_accel_group_connect(ag, K[i].kv, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE, g_cclosure_new(G_CALLBACK(accel), GINT_TO_POINTER(K[i].k), NULL));
	  gtk_window_add_accel_group(GTK_WINDOW(W->win), ag); }
	gtk_widget_show_all(W->win); gtk_widget_hide(FIND);
	gtk_widget_grab_focus(ED);   /* (the page's window opens with its writing ready: typing goes into it at once) */
	counts(); state_soon();
	gtk_main(); return 0;
}
