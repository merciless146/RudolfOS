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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "rudolf-ui.h"
#include "wordpro-gen.h"

static RuiWin *W; static GtkWidget *ED, *TB, *FIND, *FI, *REPL, *STAT_PG, *STAT_W, *STAT_C, *STAT_MSG, *ZOOMW;
static GtkTextBuffer *BUF;
/* the toolbar's choices, kept so the toolbar can show what the writing is (the page's updSel): the toggles, the four
   alignments, the style and font boxes and the size */
static GtkWidget *BTN_B, *BTN_I, *BTN_U, *BTN_S, *BTN_SUB, *BTN_SUP, *ALN[4], *STYLEW, *FONTW, *SIZEW;
static int UPDATING;
static int ZOOM = 100; static char SAY[200]; static const char *FAMILY = "Times New Roman"; static double PT = 12;
static char ON[40];   /* (the toggles the cursor's place has, as the page's updSel writes them) */
static const char *MENU_OPEN; static int MENU_OPEN_N;   /* (the menu open now, for the state the tests read) */

/* ------------------------------------------------------------------ tags, and the page's "select some text first" */
static void state_soon(void);
static void counts(void);
static void upd_sel(void);
static int menus_n(void);
static void obj_clear(void);
static void obj_counts(int *w, int *c);
static void state_objs(GString *s);
/* the page's own values for the parts of WordPro GTK does not draw as it (base_v24.html's .wp-tb, .wp-area, .wp-paper, .wp-sb) */
static void css(void)
{
	static const char *T =
		".wp-tb{background:#f9fafb;border-bottom:1px solid #d1d5db}"
		"button.wp-tb{min-width:26px;padding:0 5px;border:1px solid transparent;background:transparent;border-radius:4px;font-size:13px;color:#111}"
		"button.wp-tb:hover{background:#e0e7ff;border-color:#c7d2fe}"
		"button.wp-on{background:#c7d2fe;border-color:#818cf8}"   /* (the page's .wp-tb button.on) */
		"combobox.wp-tb,combobox.wp-tb button{border:1px solid #d1d5db;border-radius:4px;background:#fff;font-size:12px;color:#111}"
		"entry.wp-tb,spinbutton.wp-tb{border:1px solid #d1d5db;border-radius:4px;background:#fff;font-size:12px;color:#111}"
					".wp-mb{background:#f3f4f6;border-bottom:1px solid #d1d5db}"
		"button.wp-mb-b{border:1px solid transparent;background:transparent;border-radius:4px;font-size:13px;color:#111;padding:0 9px;min-height:0;min-width:0}"
		"button.wp-mb-b:hover{background:#dbe6ff}"
					".wp-table{border:1px solid #d1d5db;background:#fff}"
		"entry.wp-cell{border:1px solid #d1d5db;border-radius:0;background:#fff;font-size:12px;color:#111;padding:0 4px;min-height:0}"
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
static int has_tag_at(GtkTextIter *i, const char *name)
{
	GSList *l = gtk_text_iter_get_tags(i); int has = 0;
	for (GSList *k = l; k && !has; k = k->next) { char *nm = NULL; g_object_get(G_OBJECT(k->data), "name", &nm, NULL); if (nm && !strcmp(nm, name)) has = 1; g_free(nm); }
	g_slist_free(l); return has;
}
/* (the page's textDecide: every character of the choice has it, or with no choice the cursor's place) */
static int all_have(GtkTextIter *s, GtkTextIter *e, const char *name)
{
	if (gtk_text_iter_equal(s, e)) return has_tag_at(s, name);
	for (GtkTextIter i = *s; gtk_text_iter_compare(&i, e) < 0; gtk_text_iter_forward_char(&i)) if (!has_tag_at(&i, name)) return 0;
	return 1;
}
static void say(const char *m) { snprintf(SAY, sizeof SAY, "%s", m ? m : ""); if (STAT_MSG) gtk_label_set_text(GTK_LABEL(STAT_MSG), SAY); }
static int sel(GtkTextIter *s, GtkTextIter *e) { return gtk_text_buffer_get_selection_bounds(BUF, s, e); }
static int need_sel(void) { GtkTextIter s, e; if (sel(&s, &e)) return 1; say("Select some text first."); state_soon(); return 0; }
/* (the selection itself, not only whether there is one: the tag is applied to these iterators) */
static void apply_name(const char *name)
{
	GtkTextIter s, e; if (!need_sel() || !sel(&s, &e)) return;
	GtkTextTag *t = tag_new(name);
	if (all_have(&s, &e, name)) gtk_text_buffer_remove_tag(BUF, t, &s, &e);   /* (a second press takes it off, as the page's) */
	else gtk_text_buffer_apply_tag(BUF, t, &s, &e);
	counts(); upd_sel(); state_soon();
}
static void apply_str(const char *name, const char *prop, const char *v)
{
	GtkTextIter s, e; if (!need_sel() || !sel(&s, &e)) return;
	GtkTextTag *t = tag_new(name); g_object_set(G_OBJECT(t), prop, v, NULL); gtk_text_buffer_apply_tag(BUF, t, &s, &e); counts(); state_soon();
}
static void apply_dbl(const char *name, const char *prop, double v)
{
	GtkTextIter s, e; if (!need_sel() || !sel(&s, &e)) return;
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
	(void)d; if (UPDATING) return; int i = gtk_combo_box_get_active(c); if (i < 0 || i >= NWPFONTS) return;
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
	(void)d; if (UPDATING) return; int i = gtk_combo_box_get_active(c); if (i < 0 || i >= NWPSTYLES) return;
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
	GtkTextIter s, e; if (!need_sel() || !sel(&s, &e)) return;   /* (the choice itself: its end was read unfilled) */
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
static void hist_apply(const char *t) { SILENT = 1; gtk_text_buffer_set_text(BUF, t, -1); SILENT = 0; obj_clear(); counts(); state_soon(); }
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
/* (a picture or a table's place is the page's placeholder character, not a word and not a character of the writing) */
static int words_of(const char *t) { int n = 0, in = 0; for (const char *p = t; *p; p++) {
	if (!strncmp(p, "\xef\xbf\xbc", 3)) { p += 2; in = 0; continue; }
	if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') in = 0; else if (!in) { in = 1; n++; } } return n; }
static int chars_of(const char *t) { int n = 0; for (const char *p = t; *p; p++) {
	if (!strncmp(p, "\xef\xbf\xbc", 3)) { p += 2; continue; }
	if (*p != '\n' && *p != '\r') n++; } return n; }
/* ------------------------------------------------------------------ what the writing is, shown in the toolbar (updSel) */
static void on_class(GtkWidget *b, int on)
{
	GtkStyleContext *c = gtk_widget_get_style_context(b);
	if (on) gtk_style_context_add_class(c, "wp-on"); else gtk_style_context_remove_class(c, "wp-on");
}
static void upd_sel(void)
{
	if (!BUF || !BTN_B) return;
	GtkTextIter s, e, at; int sel_ = gtk_text_buffer_get_selection_bounds(BUF, &s, &e);
	gtk_text_buffer_get_iter_at_mark(BUF, &at, gtk_text_buffer_get_insert(BUF));
	/* (a choice is on when every character of the choice has it, as the page asks textDecide: no choice, the cursor's) */
	struct { GtkWidget *b; const char *t; } T[] = { { BTN_B, "b" }, { BTN_I, "i" }, { BTN_U, "u" }, { BTN_S, "s" }, { BTN_SUB, "sub" }, { BTN_SUP, "sup" } };
	char on[40] = "";
	for (unsigned i = 0; i < G_N_ELEMENTS(T); i++) {
		int v = has_tag_at(&at, T[i].t); on_class(T[i].b, v);
		if (v) { size_t n = strlen(on); snprintf(on + n, sizeof on - n, "%s%s", n ? "," : "", T[i].t); }
	}
	for (int i = 0; i < 4; i++) if (ALN[i]) { char n[16]; snprintf(n, sizeof n, "al:%s", i == 0 ? "left" : i == 1 ? "center" : i == 2 ? "right" : "fill"); on_class(ALN[i], has_tag_at(&at, n)); }
	/* the size and the style the cursor is in, as the page writes them back into its boxes */
	double pt = PT; char sty[32] = "p";
	{ GSList *l = gtk_text_iter_get_tags(&at);
	  for (GSList *k = l; k; k = k->next) { char *nm = NULL; g_object_get(G_OBJECT(k->data), "name", &nm, NULL);
		if (nm && !strncmp(nm, "sz:", 3)) pt = atof(nm + 3);
		if (nm && !strncmp(nm, "st:", 3)) snprintf(sty, sizeof sty, "%s", nm + 3);
		if (nm && !strncmp(nm, "ff:", 3) && FONTW) for (int i = 0; i < NWPFONTS; i++) if (!strcmp(WP_FONTS[i], nm + 3) && !gtk_widget_has_focus(FONTW)) { UPDATING = 1; gtk_combo_box_set_active(GTK_COMBO_BOX(FONTW), i); UPDATING = 0; }
		g_free(nm); }
	  g_slist_free(l); }
	if (SIZEW && !gtk_widget_has_focus(SIZEW)) { char t[16]; snprintf(t, sizeof t, "%.0f", pt); if (strcmp(gtk_entry_get_text(GTK_ENTRY(SIZEW)), t)) gtk_entry_set_text(GTK_ENTRY(SIZEW), t); }
	if (STYLEW && !UPDATING && !gtk_widget_has_focus(STYLEW)) for (int i = 0; i < NWPSTYLES; i++)
		if (!strcmp(WP_STYLES[i].k, sty) && gtk_combo_box_get_active(GTK_COMBO_BOX(STYLEW)) != i) { UPDATING = 1; gtk_combo_box_set_active(GTK_COMBO_BOX(STYLEW), i); UPDATING = 0; }
	(void)sel_;   /* (the cursor's place decides what the boxes show, as the page's updSel) */
	snprintf(ON, sizeof ON, "%s", on);
	state_soon();
}
static void on_cursor(GtkTextBuffer *b, GParamSpec *p, gpointer d) { (void)b; (void)p; (void)d; upd_sel(); }
static void counts(void)
{
	char *t = doc_text(); char b[96]; int w = words_of(t), c = chars_of(t);
	obj_counts(&w, &c);   /* (a table's cells are part of the writing: their words are the page's words) */
	snprintf(b, sizeof b, "%d %s", w, rui_tr("words")); gtk_label_set_text(GTK_LABEL(STAT_W), b);
	snprintf(b, sizeof b, "%d %s", c, rui_tr("characters")); gtk_label_set_text(GTK_LABEL(STAT_C), b);
	g_free(t);
}
static void state_write(void)
{
	const char *f = getenv("RUDOLF_WP_STATE"); if (!f || !BUF) return;
	GString *s = g_string_new(NULL); char *t = doc_text();
	GtkTextIter a, b; int has = sel(&a, &b);
	g_string_append_printf(s, "font %s\nsize %.1f\nzoom %d\nwords %d\nchars %d\nsel %d\nsay %s\nstyles %d\nfonts %d\n",
		FAMILY, PT, ZOOM, words_of(t), chars_of(t), has, SAY, NWPSTYLES, NWPFONTS);
	g_string_append_printf(s, "menus %d\n", menus_n());
	if (MENU_OPEN) g_string_append_printf(s, "menu %s %d\n", MENU_OPEN, MENU_OPEN_N);
	state_objs(s);
	if (FIND && FI && REPL) g_string_append_printf(s, "findbar %d\nfind %s\nrepl %s\n", gtk_widget_get_visible(FIND) ? 1 : 0,
		gtk_entry_get_text(GTK_ENTRY(FI)), gtk_entry_get_text(GTK_ENTRY(REPL)));
	g_string_append_printf(s, "on %s\n", ON);
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
static void on_mark(GtkTextBuffer *b, GtkTextIter *i, GtkTextMark *m, gpointer d) { (void)b; (void)i; (void)m; (void)d; counts(); upd_sel(); state_soon(); }

/* ------------------------------------------------------------------ the toolbar, drawn as the page's .wp-tb */
static GtkWidget *tb_button(const char *label, const char *tip, GCallback cb, const char *d)
{
	GtkWidget *b = gtk_button_new(); GtkWidget *l = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(l), label); gtk_container_add(GTK_CONTAINER(b), l);
	gtk_style_context_add_class(gtk_widget_get_style_context(b), "rui");
	gtk_widget_set_tooltip_text(b, rui_tr(tip)); gtk_widget_set_can_focus(b, FALSE);
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
	gtk_widget_set_size_request(st, (int)lround(rui_px(104)), (int)lround(rui_px(26))); STYLEW = st;
	gtk_combo_box_set_active(GTK_COMBO_BOX(st), 0); g_signal_connect(st, "changed", G_CALLBACK(on_style), NULL);
	gtk_style_context_add_class(gtk_widget_get_style_context(st), "wp-tb"); tb_add(TB, st);

	GtkWidget *ff = gtk_combo_box_text_new();
	for (int i = 0; i < NWPFONTS; i++) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ff), WP_FONTS[i]);
	gtk_widget_set_tooltip_text(ff, rui_tr("Font"));
	gtk_widget_set_size_request(ff, (int)lround(rui_px(128)), (int)lround(rui_px(26))); FONTW = ff;
	gtk_combo_box_set_active(GTK_COMBO_BOX(ff), 0); g_signal_connect(ff, "changed", G_CALLBACK(on_family), NULL);
	gtk_style_context_add_class(gtk_widget_get_style_context(ff), "wp-tb"); tb_add(TB, ff);

	tb_add(TB, tb_button("−", "Smaller", G_CALLBACK(inc_size), "down"));
	{ GtkWidget *sz = gtk_entry_new(); gtk_entry_set_text(GTK_ENTRY(sz), "12"); gtk_entry_set_width_chars(GTK_ENTRY(sz), 3);
	  gtk_entry_set_max_length(GTK_ENTRY(sz), 3); gtk_entry_set_alignment(GTK_ENTRY(sz), 0.5);
	  gtk_widget_set_tooltip_text(sz, rui_tr("Font size (pt)")); gtk_widget_set_size_request(sz, (int)lround(rui_px(34)), (int)lround(rui_px(26)));
	  g_signal_connect(sz, "activate", G_CALLBACK(on_size), NULL); tb_add(TB, sz); SIZEW = sz; g_object_set_data(G_OBJECT(TB), "size", sz); }
	tb_add(TB, tb_button("+", "Larger", G_CALLBACK(inc_size), NULL));
	tb_sep(TB);
	BTN_B = tb_button("<b>B</b>", "Bold (Ctrl+B)", G_CALLBACK(toggle), "b"); tb_add(TB, BTN_B);
	BTN_I = tb_button("<i>I</i>", "Italic (Ctrl+I)", G_CALLBACK(toggle), "i"); tb_add(TB, BTN_I);
	BTN_U = tb_button("<u>U</u>", "Underline (Ctrl+U)", G_CALLBACK(toggle), "u"); tb_add(TB, BTN_U);
	BTN_S = tb_button("<s>S</s>", "Strikethrough", G_CALLBACK(toggle), "s"); tb_add(TB, BTN_S);
	BTN_SUB = tb_button("x₂", "Subscript", G_CALLBACK(toggle), "sub"); tb_add(TB, BTN_SUB);
	BTN_SUP = tb_button("x²", "Superscript", G_CALLBACK(toggle), "sup"); tb_add(TB, BTN_SUP);
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
	ALN[0] = tb_button("⬅", "Align left", G_CALLBACK(on_align), "left"); tb_add(TB, ALN[0]);
	ALN[1] = tb_button("↔", "Center", G_CALLBACK(on_align), "center"); tb_add(TB, ALN[1]);
	ALN[2] = tb_button("➡", "Align right", G_CALLBACK(on_align), "right"); tb_add(TB, ALN[2]);
	ALN[3] = tb_button("☰", "Justify", G_CALLBACK(on_align), "fill"); tb_add(TB, ALN[3]);
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
/* ------------------------------------------------------------------ the menu bar, the page's .wp-mb and its eight menus
   Each menu is the page's own MENUS: its items in its order, with its shortcuts. An item the editor cannot do yet is drawn
   as the page draws one that cannot be chosen (dimmed), so the bar reads as the page's while what is there works. The
   dropdowns are rui_menu, the desktop's menus in the page's own look. An item names one operation: the tables say what an
   item does, and one dispatch does it, so no item needs a function of its own for a single call. */
enum { OP_NONE, OP_NEW, OP_CLOSE, OP_UNDO, OP_REDO, OP_CUT, OP_COPY, OP_PASTE, OP_SELALL, OP_FIND, OP_REPLACE, OP_ZOOM,
	OP_TAG, OP_LIST, OP_QUOTE, OP_CLEAR, OP_CASE, OP_WHEN, OP_STATS, OP_KEYS, OP_ABOUT, OP_PICTURE, OP_TABLE };
typedef struct { const char *label, *key; int off; const char *tag; int z; int op; const char *arg; } Mi;
#define WPS { NULL, NULL, 0, NULL, 0, OP_NONE, NULL }
static void noop(int i, void *d) { (void)i; (void)d; }
/* New is a step of the writing's own history, as every other change is: undo brings the page back, redo empties it again
   (a New that only emptied the buffer would leave undo and redo looking at a page that is no longer there) */
static void new_do(int i, void *d)
{
	(void)d; if (i != 1) return;
	SILENT = 1; gtk_text_buffer_set_text(BUF, "", -1); SILENT = 0;
	obj_clear(); hist_push(); counts(); upd_sel(); state_soon(); gtk_widget_grab_focus(ED);
}
static void new_page(void)
{
	char *t = doc_text(); int has = t && strspn(t, " \t\r\n") != strlen(t); g_free(t);
	if (!has) { gtk_widget_grab_focus(ED); return; }   /* (an empty page has nothing to discard, and nothing to undo) */
	static const char *B[] = { "Cancel", "Discard" };
	rui_confirm(W, "WordPro", rui_tr("Discard what is written on this page?"), B, 2, 0, new_do, NULL);
}
static void quote_style(void)
{
	for (int i = 0; i < NWPSTYLES; i++) if (!strcmp(WP_STYLES[i].k, "quote")) { if (STYLEW) gtk_combo_box_set_active(GTK_COMBO_BOX(STYLEW), i); return; }
}
static void zoom_to(int z)
{
	static const int Z[] = { 50, 60, 70, 80, 90, 100, 110, 125, 150, 175, 200 };
	ZOOM = z; set_editor_font();
	for (unsigned i = 0; ZOOMW && i < G_N_ELEMENTS(Z); i++) if (Z[i] == z) { UPDATING = 1; gtk_combo_box_set_active(GTK_COMBO_BOX(ZOOMW), (int)i); UPDATING = 0; break; }
	state_soon();
}
/* (UPPERCASE, lowercase, Title Case and Sentence case: the page's caseChange over the choice) */
static char *case_of(const char *t, int kind)
{
	GString *o = g_string_new(NULL); int start = 1;
	for (const char *p = t; *p; p = g_utf8_next_char(p)) {
		gunichar c = g_utf8_get_char(p); int space = g_unichar_isspace(c);
		int up = kind == 0 ? 1 : kind == 1 ? 0 : (start && !space);
		g_string_append_unichar(o, up ? g_unichar_toupper(c) : g_unichar_tolower(c));
		if (kind == 3) { if (c == '.' || c == '!' || c == '?') start = 1; else if (!space) start = 0; }
		else start = space;
	}
	return g_string_free(o, FALSE);
}
static void case_do(int kind)
{
	GtkTextIter s, e; if (!need_sel() || !sel(&s, &e)) return;
	char *t = gtk_text_buffer_get_text(BUF, &s, &e, FALSE), *n = case_of(t, kind);
	gtk_text_buffer_delete(BUF, &s, &e); gtk_text_buffer_insert(BUF, &s, n, -1);
	g_free(t); g_free(n); counts(); upd_sel(); state_soon();
}
static void insert_here(const char *t)
{
	GtkTextIter it; gtk_text_buffer_get_iter_at_mark(BUF, &it, gtk_text_buffer_get_insert(BUF));
	gtk_text_buffer_insert(BUF, &it, t, -1); counts(); state_soon();
}
static void insert_when(const char *k)
{
	char b[128]; time_t n = time(NULL); struct tm t; localtime_r(&n, &t);
	strftime(b, sizeof b, !strcmp(k, "long") ? "%A, %B %-d, %Y" : !strcmp(k, "short") ? "%m/%d/%Y" : "%I:%M %p", &t);
	insert_here(b);
}
static void show_stats(void)
{
	char *t = doc_text(); int lines = 1;
	for (const char *p = t; *p; p++) if (*p == '\n') lines++;
	char m[256];
	snprintf(m, sizeof m, "%d %s\n%d %s\n%d %s", words_of(t), rui_tr("words"), chars_of(t), rui_tr("characters"), lines, rui_tr("lines"));
	g_free(t);
	static const char *B[] = { "OK" };
	rui_confirm(W, rui_tr("Document statistics"), m, B, 1, 1, noop, NULL);
}
static void show_keys(void)
{
	static const char *B[] = { "OK" };
	rui_confirm(W, rui_tr("Keyboard shortcuts"),
		"Ctrl+B / I / U - bold, italic, underline\nCtrl+Z / Y - undo, redo\nCtrl+X / C / V - cut, copy, paste\n"
		"Ctrl+A - select all\nCtrl+F - find\nCtrl+K - insert a link\nCtrl+P - print", B, 1, 1, noop, NULL);
}
/* ------------------------------------------------------------------ the document's objects: a picture and a table
   The model, decided before either was written (docs/plan-1.6.3.md, "The document model"): a picture or a table is a
   place in the writing -- a child anchor, which the page itself holds as its placeholder character -- and its own data
   lives beside it here, so the cursor, the counts and the undo still see one writing in order, and a save can write what
   the page writes: an <img> or a <table>. A picture's bytes and size are kept; a table's cells are entries of their own,
   whose words are part of the page's word count. */
enum { OBJ_PIC = 1, OBJ_TABLE };
typedef struct { int kind, wpx, hpx, rows, cols; GtkWidget *w; char *path; } WpObj;
#define MAXOBJ 32
static WpObj OBJ[MAXOBJ]; static int NOBJ;
static void obj_clear(void)
{
	for (int i = 0; i < NOBJ; i++) g_free(OBJ[i].path);
	NOBJ = 0;
}
static void obj_counts(int *w, int *c)
{
	for (int i = 0; i < NOBJ; i++) if (OBJ[i].kind == OBJ_TABLE && OBJ[i].w) {
		GList *ch = gtk_container_get_children(GTK_CONTAINER(OBJ[i].w));
		for (GList *k = ch; k; k = k->next) if (GTK_IS_ENTRY(k->data)) {
			const char *t = gtk_entry_get_text(GTK_ENTRY(k->data)); *w += words_of(t); *c += chars_of(t);
		}
		g_list_free(ch);
	}
}
static void state_objs(GString *s)
{
	g_string_append_printf(s, "objs %d\n", NOBJ);
	for (int i = 0; i < NOBJ; i++) {
		if (OBJ[i].kind == OBJ_PIC) g_string_append_printf(s, "obj picture %dx%d %s\n", OBJ[i].wpx, OBJ[i].hpx, OBJ[i].path ? OBJ[i].path : "");
		else g_string_append_printf(s, "obj table %dx%d\n", OBJ[i].rows, OBJ[i].cols);
	}
}
static void obj_add(GtkWidget *w, int kind, const char *path, int wpx, int hpx, int rows, int cols)
{
	if (NOBJ >= MAXOBJ) { say(rui_tr("This page holds as many pictures and tables as it can.")); return; }
	GtkTextIter it; gtk_text_buffer_get_iter_at_mark(BUF, &it, gtk_text_buffer_get_insert(BUF));
	GtkTextChildAnchor *a = gtk_text_buffer_create_child_anchor(BUF, &it);
	gtk_text_view_add_child_at_anchor(GTK_TEXT_VIEW(ED), w, a);
	OBJ[NOBJ++] = (WpObj){ kind, wpx, hpx, rows, cols, w, g_strdup(path) };
	counts(); upd_sel(); state_soon(); gtk_widget_grab_focus(ED);
}
static void insert_picture(const char *path)
{
	GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, NULL);
	if (!pb) { say(rui_tr("That picture could not be read.")); return; }
	GtkWidget *img = gtk_image_new_from_pixbuf(pb);
	obj_add(img, OBJ_PIC, path, gdk_pixbuf_get_width(pb), gdk_pixbuf_get_height(pb), 0, 0);
	g_object_unref(pb);
}
static void insert_table(int rows, int cols)
{
	GtkWidget *g = gtk_grid_new(); gtk_style_context_add_class(gtk_widget_get_style_context(g), "wp-table");
	for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++) {
		GtkWidget *e = gtk_entry_new(); gtk_entry_set_width_chars(GTK_ENTRY(e), 8);
		gtk_widget_set_size_request(e, (int)lround(rui_px(96)), (int)lround(rui_px(26)));
		gtk_style_context_add_class(gtk_widget_get_style_context(e), "wp-cell");
		gtk_grid_attach(GTK_GRID(g), e, c, r, 1, 1);
	}
	obj_add(g, OBJ_TABLE, NULL, 0, 0, rows, cols);
}
static void choose_picture(void)
{
	GtkWidget *f = gtk_file_chooser_dialog_new(rui_tr("Insert picture"), GTK_WINDOW(W->win), GTK_FILE_CHOOSER_ACTION_OPEN,
		rui_tr("Cancel"), GTK_RESPONSE_CANCEL, rui_tr("Insert"), GTK_RESPONSE_ACCEPT, NULL);
	GtkFileFilter *fl = gtk_file_filter_new(); gtk_file_filter_set_name(fl, rui_tr("Pictures")); gtk_file_filter_add_pixbuf_formats(fl);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(f), fl);
	if (gtk_dialog_run(GTK_DIALOG(f)) == GTK_RESPONSE_ACCEPT) {
		char *p = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(f));
		if (p) insert_picture(p);
		g_free(p);
	}
	gtk_widget_destroy(f);
}
/* the page's Paste as plain text: the clipboard's text, and none of what came with it */
static void paste_plain(void)
{
	char *t = gtk_clipboard_wait_for_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
	if (!t) return;
	insert_here(t);
	g_free(t);
}
static gboolean insert_hook(gpointer d)
{
	const char *v = d;
	if (!strncmp(v, "picture:", 8)) insert_picture(v + 8);
	else if (!strcmp(v, "table")) insert_table(3, 3);
	return G_SOURCE_REMOVE;
}
static void menu_do(const Mi *s)
{
	GtkTextIter a, b;
	switch (s->op) {
	case OP_NEW: new_page(); break;
	case OP_CLOSE: gtk_widget_destroy(W->win); break;
	case OP_UNDO: hist_undo(); break;
	case OP_REDO: hist_redo(); break;
	case OP_CUT: gtk_text_buffer_cut_clipboard(BUF, gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), TRUE); counts(); state_soon(); break;
	case OP_COPY: gtk_text_buffer_copy_clipboard(BUF, gtk_clipboard_get(GDK_SELECTION_CLIPBOARD)); break;
	case OP_PASTE:
		if (s->arg) { paste_plain(); break; }
		gtk_text_buffer_get_iter_at_mark(BUF, &a, gtk_text_buffer_get_insert(BUF));
		gtk_text_buffer_paste_clipboard(BUF, gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), &a, TRUE);
		counts(); state_soon(); break;
	case OP_SELALL:
		gtk_text_buffer_get_bounds(BUF, &a, &b); gtk_text_buffer_select_range(BUF, &b, &a);
		gtk_widget_grab_focus(ED); state_soon(); break;
	case OP_FIND: on_find_open(NULL, NULL); break;
	case OP_REPLACE: gtk_widget_show_all(FIND); gtk_widget_grab_focus(REPL); state_soon(); break;
	case OP_ZOOM: zoom_to(s->z); break;
	case OP_TAG: apply_name(s->tag); break;
	case OP_LIST: line_prefix(s->arg); break;
	case OP_QUOTE: quote_style(); break;
	case OP_CLEAR: on_clear(NULL, NULL); break;
	case OP_CASE: case_do(!strcmp(s->arg, "up") ? 0 : !strcmp(s->arg, "low") ? 1 : !strcmp(s->arg, "title") ? 2 : 3); break;
	case OP_WHEN: insert_when(s->arg); break;
	case OP_PICTURE: choose_picture(); break;
	case OP_TABLE: insert_table(3, 3); break;   /* (the page's own default table, until its dialog comes) */
	case OP_STATS: show_stats(); break;
	case OP_KEYS: show_keys(); break;
	case OP_ABOUT:
		rui_about(W, "WordPro 1.0", rui_tr("A word processor for Rudolf OS with styles, the four alignments, bullets, numbering, "
			"find and replace, and the page's own lists. Documents are saved as .DOC files that WordLite and Oracle can also open."));
		break;
	default: break;
	}
}
static const Mi FILE_I[] = {
	{ "New", "Ctrl+N", 0, NULL, 0, OP_NEW, NULL }, { "Open...", "Ctrl+O", 1, NULL, 0, OP_NONE, NULL },
	{ "Save", "Ctrl+S", 1, NULL, 0, OP_NONE, NULL }, { "Save As...", "", 1, NULL, 0, OP_NONE, NULL }, WPS,
	{ "Import text or HTML file...", "", 1, NULL, 0, OP_NONE, NULL }, { "Export as text (.TXT)...", "", 1, NULL, 0, OP_NONE, NULL },
	{ "Export as web page (.HTM)...", "", 1, NULL, 0, OP_NONE, NULL }, WPS,
	{ "Page setup...", "", 1, NULL, 0, OP_NONE, NULL }, { "Print...", "Ctrl+P", 1, NULL, 0, OP_NONE, NULL }, WPS,
	{ "Document statistics...", "", 0, NULL, 0, OP_STATS, NULL }, { "Close", "", 0, NULL, 0, OP_CLOSE, NULL } };
static const Mi EDIT_I[] = {
	{ "Undo", "Ctrl+Z", 0, NULL, 0, OP_UNDO, NULL }, { "Redo", "Ctrl+Y", 0, NULL, 0, OP_REDO, NULL }, WPS,
	{ "Cut", "Ctrl+X", 0, NULL, 0, OP_CUT, NULL }, { "Copy", "Ctrl+C", 0, NULL, 0, OP_COPY, NULL },
	{ "Paste", "Ctrl+V", 0, NULL, 0, OP_PASTE, NULL }, { "Paste as plain text", "Ctrl+Shift+V", 0, NULL, 0, OP_PASTE, "plain" }, WPS,
	{ "Select all", "Ctrl+A", 0, NULL, 0, OP_SELALL, NULL }, WPS,
	{ "Find...", "Ctrl+F", 0, NULL, 0, OP_FIND, NULL }, { "Find and replace...", "Ctrl+H", 0, NULL, 0, OP_REPLACE, NULL } };
static const Mi VIEW_I[] = {
	{ "Print layout", "", 1, NULL, 0, OP_NONE, NULL }, { "Web layout", "", 1, NULL, 0, OP_NONE, NULL }, WPS,
	{ "Zoom 50%", "", 0, NULL, 50, OP_ZOOM, NULL }, { "Zoom 75%", "", 0, NULL, 75, OP_ZOOM, NULL },
	{ "Zoom 100%", "", 0, NULL, 100, OP_ZOOM, NULL }, { "Zoom 125%", "", 0, NULL, 125, OP_ZOOM, NULL },
	{ "Zoom 150%", "", 0, NULL, 150, OP_ZOOM, NULL }, { "Zoom 200%", "", 0, NULL, 200, OP_ZOOM, NULL }, WPS,
	{ "Outline pane", "", 1, NULL, 0, OP_NONE, NULL }, { "Show formatting marks", "", 1, NULL, 0, OP_NONE, NULL },
	{ "Spell check as you type", "", 1, NULL, 0, OP_NONE, NULL }, WPS,
	{ "Show Notey, the assistant", "", 1, NULL, 0, OP_NONE, NULL } };
static const Mi INSERT_I[] = {
	{ "Table...", "", 0, NULL, 0, OP_TABLE, NULL }, { "Picture...", "", 0, NULL, 0, OP_PICTURE, NULL },
	{ "Hyperlink...", "Ctrl+K", 1, NULL, 0, OP_NONE, NULL }, { "Bookmark...", "", 1, NULL, 0, OP_NONE, NULL },
	{ "Links in this document...", "", 1, NULL, 0, OP_NONE, NULL }, { "Horizontal line", "", 1, NULL, 0, OP_NONE, NULL },
	{ "Page break", "Ctrl+Enter", 1, NULL, 0, OP_NONE, NULL }, WPS,
	{ "Date (long)", "", 0, NULL, 0, OP_WHEN, "long" }, { "Date (short)", "", 0, NULL, 0, OP_WHEN, "short" },
	{ "Time", "", 0, NULL, 0, OP_WHEN, "time" }, { "Special characters...", "", 1, NULL, 0, OP_NONE, NULL }, WPS,
	{ "Footnote...", "", 1, NULL, 0, OP_NONE, NULL }, { "Table of contents / update", "", 1, NULL, 0, OP_NONE, NULL }, WPS,
	{ "Header and footer...", "", 1, NULL, 0, OP_NONE, NULL }, { "Page numbers", "", 1, NULL, 0, OP_NONE, NULL } };
static const Mi FORMAT_I[] = {
	{ "Bold", "Ctrl+B", 0, "b", 0, OP_TAG, NULL }, { "Italic", "Ctrl+I", 0, "i", 0, OP_TAG, NULL },
	{ "Underline", "Ctrl+U", 0, "u", 0, OP_TAG, NULL }, { "Strikethrough", "", 0, "s", 0, OP_TAG, NULL },
	{ "Subscript", "", 0, "sub", 0, OP_TAG, NULL }, { "Superscript", "", 0, "sup", 0, OP_TAG, NULL }, WPS,
	{ "UPPERCASE", "", 0, NULL, 0, OP_CASE, "up" }, { "lowercase", "", 0, NULL, 0, OP_CASE, "low" },
	{ "Title Case", "", 0, NULL, 0, OP_CASE, "title" }, { "Sentence case", "", 0, NULL, 0, OP_CASE, "sen" },
	{ "Clear formatting", "", 0, NULL, 0, OP_CLEAR, NULL }, WPS,
	{ "Paragraph...", "", 1, NULL, 0, OP_NONE, NULL }, { "Bulleted list", "", 0, NULL, 0, OP_LIST, "bul" },
	{ "Numbered list", "", 0, NULL, 0, OP_LIST, "num" }, { "Increase indent", "", 0, NULL, 0, OP_LIST, "ind" },
	{ "Decrease indent", "", 0, NULL, 0, OP_LIST, "out" }, { "Block quote", "", 0, NULL, 0, OP_QUOTE, NULL }, WPS,
	{ "One column", "", 1, NULL, 0, OP_NONE, NULL }, { "Two columns", "", 1, NULL, 0, OP_NONE, NULL },
	{ "Three columns", "", 1, NULL, 0, OP_NONE, NULL }, { "Drop cap", "", 1, NULL, 0, OP_NONE, NULL } };
static const Mi TABLE_I[] = {
	{ "Insert table...", "", 0, NULL, 0, OP_TABLE, NULL }, WPS,
	{ "Insert row above", "", 1, NULL, 0, OP_NONE, NULL }, { "Insert row below", "", 1, NULL, 0, OP_NONE, NULL },
	{ "Insert column left", "", 1, NULL, 0, OP_NONE, NULL }, { "Insert column right", "", 1, NULL, 0, OP_NONE, NULL }, WPS,
	{ "Delete row", "", 1, NULL, 0, OP_NONE, NULL }, { "Delete column", "", 1, NULL, 0, OP_NONE, NULL },
	{ "Delete table", "", 1, NULL, 0, OP_NONE, NULL }, WPS,
	{ "Merge with cell to the right", "", 1, NULL, 0, OP_NONE, NULL }, { "Shade cell...", "", 1, NULL, 0, OP_NONE, NULL },
	{ "Toggle borders", "", 1, NULL, 0, OP_NONE, NULL }, { "Toggle header row", "", 1, NULL, 0, OP_NONE, NULL } };
static const Mi TOOLS_I[] = {
	{ "Check my writing (Notey)", "", 1, NULL, 0, OP_NONE, NULL }, { "Document statistics...", "", 0, NULL, 0, OP_STATS, NULL },
	{ "Smart quotes while typing", "", 1, NULL, 0, OP_NONE, NULL }, WPS,
	{ "Change page colour...", "", 1, NULL, 0, OP_NONE, NULL } };
static const Mi HELP_I[] = {
	{ "Keyboard shortcuts", "", 0, NULL, 0, OP_KEYS, NULL }, { "About WordPro", "", 0, NULL, 0, OP_ABOUT, NULL } };
static const struct { const char *title; const Mi *items; int n; } MENUS[] = {
	{ "File", FILE_I, G_N_ELEMENTS(FILE_I) }, { "Edit", EDIT_I, G_N_ELEMENTS(EDIT_I) },
	{ "View", VIEW_I, G_N_ELEMENTS(VIEW_I) }, { "Insert", INSERT_I, G_N_ELEMENTS(INSERT_I) },
	{ "Format", FORMAT_I, G_N_ELEMENTS(FORMAT_I) }, { "Table", TABLE_I, G_N_ELEMENTS(TABLE_I) },
	{ "Tools", TOOLS_I, G_N_ELEMENTS(TOOLS_I) }, { "Help", HELP_I, G_N_ELEMENTS(HELP_I) } };
#define NMENUS ((int)G_N_ELEMENTS(MENUS))
static GtkWidget *MBAR, *MBTNS[NMENUS];
static int menus_n(void) { return NMENUS; }
static void menu_pick(int i, void *d)
{
	int m = (int)(intptr_t)d; MENU_OPEN = NULL;
	if (i >= 0 && i < MENUS[m].n && !MENUS[m].items[i].off) menu_do(&MENUS[m].items[i]);
	upd_sel(); state_soon();
}
static void open_menu(GtkWidget *b, int m)
{
	RuiMenuItem *it = g_new0(RuiMenuItem, MENUS[m].n); char **lab = g_new0(char *, MENUS[m].n); int nlab = 0;
	GtkTextIter at; gtk_text_buffer_get_iter_at_mark(BUF, &at, gtk_text_buffer_get_insert(BUF));
	for (int k = 0; k < MENUS[m].n; k++) {
		const Mi *s = &MENUS[m].items[k];
		it[k].label = s->label; it[k].key = s->key; it[k].off = s->off; it[k].sep = s->label == NULL;
		if (s->label && ((s->tag && has_tag_at(&at, s->tag)) || (s->z && s->z == ZOOM))) it[k].label = lab[nlab++] = g_strconcat("\xe2\x9c\x93 ", s->label, NULL);
	}
	GtkAllocation a; gtk_widget_get_allocation(b, &a); int ox = 0, oy = 0;
	if (gtk_widget_get_window(b)) gdk_window_get_origin(gtk_widget_get_window(b), &ox, &oy);
	MENU_OPEN = MENUS[m].title; MENU_OPEN_N = MENUS[m].n;
	rui_menu(b, ox, oy + a.height, NULL, it, MENUS[m].n, menu_pick, (void *)(intptr_t)m);
	for (int k = 0; k < nlab; k++) g_free(lab[k]);
	g_free(lab); g_free(it); state_soon();
}
static void mb_click(GtkWidget *b, gpointer d) { open_menu(b, (int)(intptr_t)d); }
static void build_menubar(void)
{
	MBAR = gtk_flow_box_new();
	gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(MBAR), GTK_SELECTION_NONE);
	gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(MBAR), (int)lround(rui_px(1)));
	gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(MBAR), 12);
	gtk_widget_set_margin_start(MBAR, (int)lround(rui_px(4)));
	gtk_widget_set_margin_top(MBAR, (int)lround(rui_px(2))); gtk_widget_set_margin_bottom(MBAR, (int)lround(rui_px(2)));
	gtk_style_context_add_class(gtk_widget_get_style_context(MBAR), "wp-mb");
	for (int i = 0; i < NMENUS; i++) {
		GtkWidget *b = gtk_button_new_with_label(MENUS[i].title);
		gtk_style_context_add_class(gtk_widget_get_style_context(b), "rui");
		gtk_style_context_add_class(gtk_widget_get_style_context(b), "wp-mb-b");
		gtk_widget_set_can_focus(b, FALSE); gtk_widget_set_valign(b, GTK_ALIGN_CENTER);
		g_signal_connect(b, "clicked", G_CALLBACK(mb_click), (gpointer)(intptr_t)i);
		gtk_flow_box_insert(GTK_FLOW_BOX(MBAR), b, -1); MBTNS[i] = b;
	}
	gtk_box_pack_start(GTK_BOX(W->body), MBAR, FALSE, FALSE, 0);
}
static gboolean menu_hook(gpointer d) { int m = (int)(intptr_t)d; open_menu(MBTNS[m], m); return G_SOURCE_REMOVE; }
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
	g_signal_connect(BUF, "notify::cursor-position", G_CALLBACK(on_cursor), NULL);   /* (the toolbar follows the writing) */
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
/* the page's keys. The handler is on the writing itself, not on the window: a handler on the window runs after the text
   view has already taken Ctrl+B and Ctrl+I (it moves the cursor with them), and returning TRUE here stops that. */
static gboolean key(GtkWidget *w, GdkEventKey *e, gpointer d)
{
	(void)w; (void)d; if (!(e->state & GDK_CONTROL_MASK)) return FALSE;
	switch (e->keyval) {
	case GDK_KEY_b: apply_name("b"); return TRUE;
	case GDK_KEY_i: apply_name("i"); return TRUE;
	case GDK_KEY_u: apply_name("u"); return TRUE;
	case GDK_KEY_z: hist_undo(); return TRUE;
	case GDK_KEY_y: hist_redo(); return TRUE;
	case GDK_KEY_f: gtk_widget_show_all(FIND); gtk_widget_grab_focus(FI); return TRUE;
	case GDK_KEY_h: gtk_widget_show_all(FIND); gtk_widget_grab_focus(REPL); state_soon(); return TRUE;
	case GDK_KEY_n: new_page(); return TRUE;
	case GDK_KEY_v: if (e->state & GDK_SHIFT_MASK) { paste_plain(); return TRUE; } return FALSE;
	default: return FALSE;
	}
}
int main(int argc, char **argv)
{
	rui_init(&argc, &argv, "wordpro");
	css();
	W = rui_window("wordpro", "WordPro", 980, 646);
	build_menubar(); build_find(); build_toolbar(); build_editor(); build_status();
	g_signal_connect(ED, "key-press-event", G_CALLBACK(key), NULL);
	gtk_widget_show_all(W->win); gtk_widget_hide(FIND);
	{ const char *mo = getenv("RUDOLF_WP_MENU");   /* (the tests: the menu a click on the bar opens) */
	  if (mo && *mo) for (int i = 0; i < NMENUS; i++) if (!strcmp(MENUS[i].title, mo)) { g_timeout_add(400, menu_hook, (gpointer)(intptr_t)i); break; } }
	{ const char *ins = getenv("RUDOLF_WP_INSERT");   /* (the tests: a picture or a table put in as the menu puts it) */
	  if (ins && *ins) g_timeout_add(400, insert_hook, (gpointer)ins); }
	gtk_widget_grab_focus(ED);   /* (the page's window opens with its writing ready: typing goes into it at once) */
	counts(); state_soon();
	gtk_main(); return 0;
}
