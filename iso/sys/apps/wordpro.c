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
/* the toolbar's choices, kept so the toolbar can show what the writing is (the page's updSel): the toggles, the four
   alignments, the style and font boxes and the size */
static GtkWidget *BTN_B, *BTN_I, *BTN_U, *BTN_S, *BTN_SUB, *BTN_SUP, *ALN[4], *STYLEW, *FONTW, *SIZEW;
static int UPDATING;
static int ZOOM = 100; static char SAY[200]; static const char *FAMILY = "Times New Roman"; static double PT = 12;
static char ON[40];   /* (the toggles the cursor's place has, as the page's updSel writes them) */

/* ------------------------------------------------------------------ the menu bar, as the page's .wp-mb has it (base_v24.html's
   MENUS): the eight menus, their items in the page's order, each with the key the page gives it. An item whose step is
   later is drawn greyed and refuses, as the page's own are until they are built (SpreadsheetPro's way). The tables are
   data only; what an item does is menu_do, below, where the commands are. */
enum { M_NONE, M_CLOSE, M_UNDO, M_REDO, M_SELALL, M_FIND, M_FINDREP, M_ZOOM, M_TOGGLE, M_ALIGN, M_LIST, M_CLEAR, M_KEYS, M_ABOUT };
typedef struct { const char *label, *key; int off, op; const char *arg; } WpMi;
#define WPMI_END { NULL, NULL, 0, M_NONE, NULL }
static const WpMi FILE_I[] = {
	{ "New", "Ctrl+N", 1, M_NONE, NULL }, { "Open…", "Ctrl+O", 1, M_NONE, NULL },
	{ "Save", "Ctrl+S", 1, M_NONE, NULL }, { "Save As…", "", 1, M_NONE, NULL }, WPMI_END,
	{ "Import text or HTML file…", "", 1, M_NONE, NULL },
	{ "Export as text (.TXT)…", "", 1, M_NONE, NULL }, { "Export as web page (.HTM)…", "", 1, M_NONE, NULL }, WPMI_END,
	{ "Page setup…", "", 1, M_NONE, NULL }, { "Print…", "Ctrl+P", 1, M_NONE, NULL }, WPMI_END,
	{ "Document statistics…", "", 1, M_NONE, NULL }, { "Close", "", 0, M_CLOSE, NULL } };
static const WpMi EDIT_I[] = {
	{ "Undo", "Ctrl+Z", 0, M_UNDO, NULL }, { "Redo", "Ctrl+Y", 0, M_REDO, NULL }, WPMI_END,
	{ "Cut", "Ctrl+X", 1, M_NONE, NULL }, { "Copy", "Ctrl+C", 1, M_NONE, NULL },
	{ "Paste", "Ctrl+V", 1, M_NONE, NULL }, { "Paste as plain text", "Ctrl+Shift+V", 1, M_NONE, NULL }, WPMI_END,
	{ "Select all", "Ctrl+A", 0, M_SELALL, NULL }, WPMI_END,
	{ "Find…", "Ctrl+F", 0, M_FIND, NULL }, { "Find and replace…", "Ctrl+H", 0, M_FINDREP, NULL } };
static const WpMi VIEW_I[] = {
	{ "Print layout", "", 1, M_NONE, NULL }, { "Web layout", "", 1, M_NONE, NULL }, WPMI_END,
	{ "Zoom 50%", "", 0, M_ZOOM, "50" }, { "Zoom 75%", "", 0, M_ZOOM, "75" }, { "Zoom 100%", "", 0, M_ZOOM, "100" },
	{ "Zoom 125%", "", 0, M_ZOOM, "125" }, { "Zoom 150%", "", 0, M_ZOOM, "150" }, { "Zoom 200%", "", 0, M_ZOOM, "200" }, WPMI_END,
	{ "Outline pane", "", 1, M_NONE, NULL }, { "Show formatting marks", "", 1, M_NONE, NULL },
	{ "Spell check as you type", "", 1, M_NONE, NULL }, WPMI_END,
	{ "Show Notey, the assistant", "", 1, M_NONE, NULL } };
static const WpMi INSERT_I[] = {
	{ "Table…", "", 1, M_NONE, NULL }, { "Picture…", "", 1, M_NONE, NULL },
	{ "Hyperlink…", "Ctrl+K", 1, M_NONE, NULL }, { "Bookmark…", "", 1, M_NONE, NULL },
	{ "Links in this document…", "", 1, M_NONE, NULL }, { "Horizontal line", "", 1, M_NONE, NULL },
	{ "Page break", "Ctrl+Enter", 1, M_NONE, NULL }, WPMI_END,
	{ "Date (long)", "", 1, M_NONE, NULL }, { "Date (short)", "", 1, M_NONE, NULL }, { "Time", "", 1, M_NONE, NULL },
	{ "Special characters…", "", 1, M_NONE, NULL }, WPMI_END,
	{ "Footnote…", "", 1, M_NONE, NULL }, { "Table of contents / update", "", 1, M_NONE, NULL }, WPMI_END,
	{ "Header and footer…", "", 1, M_NONE, NULL }, { "Page numbers", "", 1, M_NONE, NULL } };
static const WpMi FORMAT_I[] = {
	{ "Bold", "Ctrl+B", 0, M_TOGGLE, "b" }, { "Italic", "Ctrl+I", 0, M_TOGGLE, "i" },
	{ "Underline", "Ctrl+U", 0, M_TOGGLE, "u" }, { "Strikethrough", "", 0, M_TOGGLE, "s" },
	{ "Subscript", "", 0, M_TOGGLE, "sub" }, { "Superscript", "", 0, M_TOGGLE, "sup" }, WPMI_END,
	{ "UPPERCASE", "", 1, M_NONE, NULL }, { "lowercase", "", 1, M_NONE, NULL },
	{ "Title Case", "", 1, M_NONE, NULL }, { "Sentence case", "", 1, M_NONE, NULL },
	{ "Clear formatting", "", 0, M_CLEAR, NULL }, WPMI_END,
	{ "Paragraph…", "", 1, M_NONE, NULL }, { "Bulleted list", "", 0, M_LIST, "bul" },
	{ "Numbered list", "", 0, M_LIST, "num" }, { "Increase indent", "", 0, M_LIST, "ind" },
	{ "Decrease indent", "", 0, M_LIST, "out" }, { "Block quote", "", 1, M_NONE, NULL }, WPMI_END,
	{ "One column", "", 1, M_NONE, NULL }, { "Two columns", "", 1, M_NONE, NULL },
	{ "Three columns", "", 1, M_NONE, NULL }, { "Drop cap", "", 1, M_NONE, NULL } };
static const WpMi TABLE_I[] = {
	{ "Insert table…", "", 1, M_NONE, NULL }, WPMI_END,
	{ "Insert row above", "", 1, M_NONE, NULL }, { "Insert row below", "", 1, M_NONE, NULL },
	{ "Insert column left", "", 1, M_NONE, NULL }, { "Insert column right", "", 1, M_NONE, NULL }, WPMI_END,
	{ "Delete row", "", 1, M_NONE, NULL }, { "Delete column", "", 1, M_NONE, NULL },
	{ "Delete table", "", 1, M_NONE, NULL }, WPMI_END,
	{ "Merge with cell to the right", "", 1, M_NONE, NULL }, { "Shade cell…", "", 1, M_NONE, NULL },
	{ "Toggle borders", "", 1, M_NONE, NULL }, { "Toggle header row", "", 1, M_NONE, NULL } };
static const WpMi TOOLS_I[] = {
	{ "Check my writing (Notey)", "", 1, M_NONE, NULL }, { "Document statistics…", "", 1, M_NONE, NULL },
	{ "Smart quotes while typing", "", 1, M_NONE, NULL }, WPMI_END,
	{ "Change page colour…", "", 1, M_NONE, NULL } };
static const WpMi HELP_I[] = {
	{ "Ask Notey…", "", 1, M_NONE, NULL }, { "Keyboard shortcuts", "", 0, M_KEYS, NULL },
	{ "About WordPro", "", 0, M_ABOUT, NULL } };
static const struct { const char *title, *key; const WpMi *items; int n; } MENUS[] = {
	{ "File", "Alt+F", FILE_I, G_N_ELEMENTS(FILE_I) }, { "Edit", "Alt+E", EDIT_I, G_N_ELEMENTS(EDIT_I) },
	{ "View", "Alt+V", VIEW_I, G_N_ELEMENTS(VIEW_I) }, { "Insert", "Alt+I", INSERT_I, G_N_ELEMENTS(INSERT_I) },
	{ "Format", "Alt+O", FORMAT_I, G_N_ELEMENTS(FORMAT_I) }, { "Table", "Alt+A", TABLE_I, G_N_ELEMENTS(TABLE_I) },
	{ "Tools", "Alt+T", TOOLS_I, G_N_ELEMENTS(TOOLS_I) }, { "Help", "Alt+H", HELP_I, G_N_ELEMENTS(HELP_I) } };
#define NMENUS ((int)G_N_ELEMENTS(MENUS))
static int MENU_OPEN_I = -1;   /* (the menu open now: the tests read it, and a second press on its button closes it) */

/* ------------------------------------------------------------------ tags, and the page's "select some text first" */
static void state_soon(void);
static gboolean menu_hook(gpointer d);
static void hist_push(void); static void hist_undo(void); static void hist_redo(void);
static void counts(void);
static void upd_sel(void);
/* the page's own values for the parts of WordPro GTK does not draw as it (base_v24.html's .wp-tb, .wp-area, .wp-paper, .wp-sb) */
static void css(void)
{
	static const char *T =
		".wp-tb{background:#f9fafb;border-bottom:1px solid #d1d5db}"
		".wp-mb{background:#f3f4f6;border-bottom:1px solid #d1d5db}"
		"button.wp-mb-b{border:0;background:transparent;padding:3px 9px;border-radius:4px;font-size:13px;color:#111}"
		"button.wp-mb-b:hover{background:#dbe6ff}"
		"button.wp-tb{min-width:26px;padding:0 5px;border:1px solid transparent;background:transparent;border-radius:4px;font-size:13px;color:#111}"
		"button.wp-tb:hover{background:#e0e7ff;border-color:#c7d2fe}"
		"button.wp-on{background:#c7d2fe;border-color:#818cf8}"   /* (the page's .wp-tb button.on) */
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
	else {
		const char *other = !strcmp(name, "sub") ? "sup" : !strcmp(name, "sup") ? "sub" : NULL;
		if (other) gtk_text_buffer_remove_tag(BUF, tag_new(other), &s, &e);   /* (a character is one or the other, never both) */
		gtk_text_buffer_apply_tag(BUF, t, &s, &e);
	}
	hist_push(); counts(); upd_sel(); state_soon();
}
static void apply_str(const char *name, const char *prop, const char *v)
{
	GtkTextIter s, e; if (!need_sel() || !sel(&s, &e)) return;
	GtkTextTag *t = tag_new(name); g_object_set(G_OBJECT(t), prop, v, NULL); gtk_text_buffer_apply_tag(BUF, t, &s, &e); hist_push(); counts(); state_soon();
}
static void apply_dbl(const char *name, const char *prop, double v)
{
	GtkTextIter s, e; if (!need_sel() || !sel(&s, &e)) return;
	GtkTextTag *t = tag_new(name); g_object_set(G_OBJECT(t), prop, v, NULL); gtk_text_buffer_apply_tag(BUF, t, &s, &e); hist_push(); counts(); state_soon();
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
	/* (a paragraph is one style: the one it had goes, as the page's own does) */
	for (int j = 0; j < NWPSTYLES; j++) {
		char on[48]; snprintf(on, sizeof on, "st:%s", WP_STYLES[j].k);
		GtkTextTag *o = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(BUF), on);
		if (o) gtk_text_buffer_remove_tag(BUF, o, &s, &e);
	}
	char nm[48]; snprintf(nm, sizeof nm, "st:%s", WP_STYLES[i].k); GtkTextTag *t = tag_new(nm);
	g_object_set(G_OBJECT(t), "size-points", ST[i].pt, "weight", ST[i].bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
		"style", ST[i].italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL, "justification", ST[i].just, NULL);
	if (!strcmp(WP_STYLES[i].k, "code")) g_object_set(G_OBJECT(t), "family", "monospace", NULL);
	gtk_text_buffer_apply_tag(BUF, t, &s, &e); hist_push(); counts(); state_soon();
}
static void on_align(GtkWidget *b, gpointer d)
{
	(void)b; GtkTextIter s, e; if (!need_sel()) return;
	const char *k = (const char *)d;
	char n[24]; snprintf(n, sizeof n, "al:%s", k);
	GtkTextTag *t = tag_new(n);
	g_object_set(G_OBJECT(t), "justification", !strcmp(k, "center") ? GTK_JUSTIFY_CENTER : !strcmp(k, "right") ? GTK_JUSTIFY_RIGHT :
		!strcmp(k, "fill") ? GTK_JUSTIFY_FILL : GTK_JUSTIFY_LEFT, NULL);
	gtk_text_buffer_apply_tag(BUF, t, &s, &e); hist_push(); counts(); state_soon();
}
/* bullets, numbering and the indent: the page's block commands, on each line of the choice */
/* (a line's first characters, for the two block commands that can be taken off again) */
static char *line_head(GtkTextIter at)
{
	GtkTextIter s = at, e;
	gtk_text_iter_set_line_offset(&s, 0); e = s; gtk_text_iter_forward_chars(&e, 10);
	return gtk_text_iter_get_slice(&s, &e);
}
static int has_prefix(const char *h, const char *kind)
{
	if (!strcmp(kind, "bul"))   /* (a bullet's three UTF-8 bytes and the space after it) */
		return (unsigned char)h[0] == 0xe2 && (unsigned char)h[1] == 0x80 && (unsigned char)h[2] == 0xa2 && h[3] == ' ';
	if (!strcmp(kind, "num")) {
		const char *p = h;
		if (!(*p >= '0' && *p <= '9')) return 0;
		while (*p >= '0' && *p <= '9') p++;
		return p[0] == '.' && p[1] == ' ';
	}
	return 0;
}
/* (the page's block commands, on each line of the choice. A second press takes the bullet or the number off, as the page's
   own does, and the lines are taken by their number: an inserted prefix does not move the ones still to come.) */
static void line_prefix(const char *kind)
{
	GtkTextIter s, e; if (!need_sel() || !sel(&s, &e)) return;   /* (need_sel fills its own iterators, not these) */
	int l0 = gtk_text_iter_get_line(&s), l1 = gtk_text_iter_get_line(&e), block = !strcmp(kind, "ind") || !strcmp(kind, "out");
	if (l1 > l0 && gtk_text_iter_get_line_offset(&e) == 0) l1--;   /* (a choice ending at a line's start does not take that line) */
	int all = 1, n = 0;
	for (int L = l0; L <= l1; L++) {
		GtkTextIter i; gtk_text_buffer_get_iter_at_line(BUF, &i, L);
		char *h = line_head(i);
		if (!block && !has_prefix(h, kind)) all = 0;
		g_free(h); n++;
	}
	all = all && n > 0;
	for (int L = l0, num = 0; L <= l1; L++) {
		GtkTextIter i; gtk_text_buffer_get_iter_at_line(BUF, &i, L);
		GtkTextIter t = i; gtk_text_iter_set_line_offset(&t, 0);
		if (!strcmp(kind, "ind")) { gtk_text_buffer_insert(BUF, &t, "    ", -1); continue; }
		if (!strcmp(kind, "out")) {
			GtkTextIter u = t; int k = 0;
			while (k < 4 && gtk_text_iter_get_char(&u) == ' ') { gtk_text_iter_forward_char(&u); k++; }
			if (k) gtk_text_buffer_delete(BUF, &t, &u);
			continue;
		}
		if (all) {
			GtkTextIter u = t;
			if (!strcmp(kind, "num")) {
				while (gtk_text_iter_get_char(&u) >= '0' && gtk_text_iter_get_char(&u) <= '9') gtk_text_iter_forward_char(&u);
				gtk_text_iter_forward_char(&u);                              /* (the dot) */
				if (gtk_text_iter_get_char(&u) == ' ') gtk_text_iter_forward_char(&u);
			} else gtk_text_iter_forward_chars(&u, 2);                       /* (the bullet and its space) */
			gtk_text_buffer_delete(BUF, &t, &u);
		} else {
			char m[16];
			if (!strcmp(kind, "num")) snprintf(m, sizeof m, "%d. ", ++num); else snprintf(m, sizeof m, "• ");
			gtk_text_buffer_insert(BUF, &t, m, -1);
		}
	}
	hist_push(); counts(); state_soon();
}
static void on_list(GtkWidget *b, gpointer d) { (void)b; line_prefix((const char *)d); }
static void on_spacing(GtkComboBox *c, gpointer d)
{
	(void)d; int i = gtk_combo_box_get_active(c); if (i <= 0) return;
	static const double V[] = { 0, 1.0, 1.15, 1.5, 2.0, 2.5 }; GtkTextIter s, e; if (!need_sel()) return;
	char n[32]; snprintf(n, sizeof n, "ls:%.2f", V[i]); GtkTextTag *t = tag_new(n);
	int pad = (int)lround((V[i] - 1) * PT * 0.66);
	g_object_set(G_OBJECT(t), "pixels-above-lines", pad, "pixels-below-lines", pad, NULL);
	gtk_text_buffer_apply_tag(BUF, t, &s, &e); hist_push(); counts(); state_soon();
}
static void on_clear(GtkWidget *b, gpointer d)
{
	(void)b; (void)d; GtkTextIter s, e; if (!need_sel()) return;
	gtk_text_buffer_remove_all_tags(BUF, &s, &e); hist_push(); counts(); state_soon();
}
/* undo and redo: GTK 3's text buffer keeps no history of its own, so the writing is kept step by step (the page's undo and
   redo, bounded to the last 120 changes). A step is the writing *with its marks* -- GTK's own tag-set serialization --
   so undoing a letter no longer takes the bold, the colour, the size and the lists with it; and the cursor and the
   choice are kept with it, so an undo leaves a person where the change was, not at the top of the page. */
#define HIST 120
static GdkAtom HIST_ATOM;
typedef struct { guint8 *d; gsize n; int cur, a, b, has; } Step;
static Step HS[HIST]; static int HN, HI, SILENT;
static void step_free(Step *s) { g_free(s->d); s->d = NULL; s->n = 0; }
static void step_take(Step *s)
{
	step_free(s);
	GtkTextIter a, b, i;
	gtk_text_buffer_get_bounds(BUF, &a, &b);
	s->d = gtk_text_buffer_serialize(BUF, BUF, HIST_ATOM, &a, &b, &s->n);
	gtk_text_buffer_get_iter_at_mark(BUF, &i, gtk_text_buffer_get_insert(BUF));
	s->cur = gtk_text_iter_get_offset(&i);
	s->has = gtk_text_buffer_get_selection_bounds(BUF, &a, &b);
	if (s->has) { s->a = gtk_text_iter_get_offset(&a); s->b = gtk_text_iter_get_offset(&b); }
}
static void step_apply(Step *s)
{
	SILENT = 1;
	gtk_text_buffer_set_text(BUF, "", -1);   /* (the text and its marks go together; the step puts both back) */
	if (s->d && s->n) {
		GtkTextIter at; gtk_text_buffer_get_start_iter(BUF, &at);
		gtk_text_buffer_deserialize(BUF, BUF, HIST_ATOM, &at, s->d, s->n, NULL);
	}
	int len = gtk_text_buffer_get_char_count(BUF), cur = s->cur > len ? len : s->cur;
	if (s->has) {
		int x = s->a > len ? len : s->a, y = s->b > len ? len : s->b;
		GtkTextIter a, b;
		gtk_text_buffer_get_iter_at_offset(BUF, &a, x); gtk_text_buffer_get_iter_at_offset(BUF, &b, y);
		gtk_text_buffer_select_range(BUF, &a, &b);
	} else {
		GtkTextIter at; gtk_text_buffer_get_iter_at_offset(BUF, &at, cur);
		gtk_text_buffer_place_cursor(BUF, &at);
	}
	SILENT = 0; counts(); upd_sel(); state_soon();
}
static void hist_undo(void) { if (HI > 1) { HI--; step_apply(&HS[HI - 1]); } }
static void hist_redo(void) { if (HI < HN) { HI++; step_apply(&HS[HI - 1]); } }
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
/* (the match is marked, as the page marks it: the tag exists for this and nothing else) */
static void mark_find(GtkTextIter *a, GtkTextIter *b)
{
	GtkTextIter s, e; gtk_text_buffer_get_bounds(BUF, &s, &e);
	gtk_text_buffer_remove_tag(BUF, tag_new("find"), &s, &e);
	gtk_text_buffer_apply_tag(BUF, tag_new("find"), a, b);
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
	mark_find(&a, &b); reveal(&a, &b); state_soon();
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
	mark_find(&ha, &hb); reveal(&ha, &hb); state_soon();
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
	gtk_text_buffer_get_start_iter(BUF, &a); gtk_text_buffer_get_end_iter(BUF, &b);
	g_string_append_printf(s, "mb %d", NMENUS);
	for (int i = 0; i < NMENUS; i++) g_string_append_printf(s, "%s%s", i ? "," : " ", MENUS[i].title);
	g_string_append_c(s, '\n');
	if (MENU_OPEN_I >= 0) {
		g_string_append_printf(s, "menuopen %s %d\n", MENUS[MENU_OPEN_I].title, MENUS[MENU_OPEN_I].n);
		g_string_append(s, "grey");
		for (int i = 0; i < MENUS[MENU_OPEN_I].n; i++) { const WpMi *m = &MENUS[MENU_OPEN_I].items[i]; if (m->off && m->label) g_string_append_printf(s, " %s", m->label); }
		g_string_append_c(s, '\n');
	}
	g_string_append_printf(s, "text %s\n", t); g_free(t);
	char *tmp = g_strdup_printf("%s.t", f); g_file_set_contents(tmp, s->str, -1, NULL); rename(tmp, f); g_free(tmp); g_string_free(s, TRUE);
}
static gboolean state_tick(gpointer d) { (void)d; state_write(); return G_SOURCE_REMOVE; }
static void state_soon(void) { if (getenv("RUDOLF_WP_STATE")) g_timeout_add(120, state_tick, NULL); }
static void hist_push(void)
{
	if (SILENT || !BUF) return;
	Step s = { 0 };
	step_take(&s);
	if (HN && HS[HN - 1].d && s.n == HS[HN - 1].n && !memcmp(HS[HN - 1].d, s.d, s.n)) { step_free(&s); return; }
	if (HI < HN) { for (int i = HI; i < HN; i++) step_free(&HS[i]); HN = HI; }
	if (HN == HIST) { step_free(&HS[0]); memmove(HS, HS + 1, sizeof(HS[0]) * (HIST - 1)); HN--; }
	HS[HN++] = s; HI = HN;
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
	gtk_text_buffer_create_tag(BUF, "b", "weight", PANGO_WEIGHT_BOLD, NULL);
	gtk_text_buffer_create_tag(BUF, "i", "style", PANGO_STYLE_ITALIC, NULL);
	gtk_text_buffer_create_tag(BUF, "u", "underline", PANGO_UNDERLINE_SINGLE, NULL);
	gtk_text_buffer_create_tag(BUF, "s", "strikethrough", TRUE, NULL);
	gtk_text_buffer_create_tag(BUF, "sub", "rise", -4000, "size-points", 9.0, NULL);
	gtk_text_buffer_create_tag(BUF, "sup", "rise", 6000, "size-points", 9.0, NULL);
	gtk_text_buffer_create_tag(BUF, "find", "background", "#fde68a", NULL);
	HIST_ATOM = gtk_text_buffer_register_serialize_tagset(BUF, "application/x-rudolf-wordpro");
	gtk_text_buffer_register_deserialize_tagset(BUF, "application/x-rudolf-wordpro");
	gtk_text_buffer_deserialize_set_can_create_tags(BUF, HIST_ATOM, TRUE);
	hist_push();   /* (the empty page is the step undo comes back to) */
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
	case GDK_KEY_z: if (e->state & GDK_SHIFT_MASK) hist_redo(); else hist_undo(); return TRUE;
	case GDK_KEY_y: hist_redo(); return TRUE;
	case GDK_KEY_f: gtk_widget_show_all(FIND); gtk_widget_grab_focus(FI); return TRUE;
	default: return FALSE;
	}
}
/* (the check's hand on the toolbar's own commands: RUDOLF_WP_DO=bul,bul applies each to the whole writing, once there is
   some -- a click on a button is the person's, and the check cannot aim one) */
static int DO_TICKS;
static gboolean do_hook(gpointer d)
{
	if (!gtk_text_buffer_get_char_count(BUF) && ++DO_TICKS < 50) return G_SOURCE_CONTINUE;
	char *copy = g_strdup((const char *)d), *sv = NULL;
	for (char *tok = strtok_r(copy, ",", &sv); tok; tok = strtok_r(NULL, ",", &sv)) {
		GtkTextIter a, b;
		gtk_text_buffer_get_bounds(BUF, &a, &b);
		gtk_text_buffer_select_range(BUF, &a, &b);   /* (the page's commands act on the choice, so the whole writing is one) */
		if (!strncmp(tok, "menu:", 5)) { int m = atoi(tok + 5); if (m >= 0 && m < NMENUS) g_idle_add(menu_hook, (gpointer)(intptr_t)m); }
		else if (!strncmp(tok, "mi:", 3)) { int m = atoi(tok + 3), k = (strchr(tok + 3, ':')) ? atoi(strchr(tok + 3, ':') + 1) : -1;
			if (m >= 0 && m < NMENUS && k >= 0 && k < MENUS[m].n) menu_do(&MENUS[m].items[k]); }
		if (!strcmp(tok, "bul") || !strcmp(tok, "num") || !strcmp(tok, "ind") || !strcmp(tok, "out")) line_prefix(tok);
		else if (!strncmp(tok, "style:", 6)) {
			int k = atoi(tok + 6);
			if (k >= 0 && k < NWPSTYLES && STYLEW) {
				UPDATING = 1; gtk_combo_box_set_active(GTK_COMBO_BOX(STYLEW), k); UPDATING = 0;
				on_style(GTK_COMBO_BOX(STYLEW), NULL);
			}
		}
	}
	g_free(copy);
	state_soon();
	return G_SOURCE_REMOVE;
}
/* ------------------------------------------------------------------ the menus themselves: the bar, its drop-downs and what
   each item does (the page's own commands; an item whose step is later has no op and refuses) */
static GtkWidget *MBAR, *MBTNS[8];
static void menu_do(const WpMi *s)
{
	if (!s || s->off) return;
	switch (s->op) {
	case M_CLOSE: gtk_widget_destroy(W->win); break;
	case M_UNDO: hist_undo(); break;
	case M_REDO: hist_redo(); break;
	case M_SELALL: { GtkTextIter a, b; gtk_text_buffer_get_bounds(BUF, &a, &b); gtk_text_buffer_select_range(BUF, &a, &b); state_soon(); break; }
	case M_FIND: on_find_open(NULL, NULL); break;
	case M_FINDREP: on_find_open(NULL, NULL); if (REPL) gtk_widget_grab_focus(REPL); break;
	case M_ZOOM: ZOOM = atoi(s->arg); set_editor_font(); state_soon(); break;
	case M_TOGGLE: toggle(NULL, (gpointer)s->arg); break;
	case M_ALIGN: on_align(NULL, (gpointer)s->arg); break;
	case M_LIST: on_list(NULL, (gpointer)s->arg); break;
	case M_CLEAR: on_clear(NULL, NULL); break;
	case M_KEYS:
		rui_about(W, rui_tr("Keyboard shortcuts"), rui_tr("Ctrl+B / I / U  Bold, italic, underline.\n"
			"Ctrl+Z / Y  Undo, redo.\nCtrl+F / H  Find, find and replace.\nCtrl+Enter  Page break."
			"\nCtrl+Click a link  Open it in Oracle.\nTab (in a table)  The next cell."));
		break;
	case M_ABOUT:
		rui_about(W, "WordPro 1.0", rui_tr("A full word processor for Rudolf OS with styles, tables, pictures, footnotes, "
			"headers and footers, the page's layout, find and replace, and Notey, your sticky-note writing assistant. "
			"Documents are saved as .DOC files that WordLite and Oracle can also open."));
		break;
	default: break;
	}
}
static void menu_pick(int i, void *d)
{
	int m = (int)(intptr_t)d; MENU_OPEN_I = -1;
	if (i >= 0 && i < MENUS[m].n) menu_do(&MENUS[m].items[i]);
	state_soon();
}
static void open_menu(GtkWidget *b, int m)
{
	RuiMenuItem *it = g_new0(RuiMenuItem, MENUS[m].n);
	for (int k = 0; k < MENUS[m].n; k++) {
		const WpMi *s = &MENUS[m].items[k];
		it[k].label = s->label; it[k].key = s->key; it[k].off = s->off; it[k].sep = s->label == NULL;
	}
	GtkAllocation a; gtk_widget_get_allocation(b, &a); int ox = 0, oy = 0;
	if (gtk_widget_get_window(b)) gdk_window_get_origin(gtk_widget_get_window(b), &ox, &oy);
	MENU_OPEN_I = m;
	rui_menu(b, ox, oy + a.height, NULL, it, MENUS[m].n, menu_pick, (void *)(intptr_t)m);
	g_free(it); state_soon();
}
static void mb_click(GtkWidget *b, gpointer d)
{
	int m = (int)(intptr_t)d;
	if (MENU_OPEN_I == m) { MENU_OPEN_I = -1; state_soon(); return; }   /* (the page's second press closes it) */
	open_menu(b, m);
}
static void build_menubar(void)
{
	MBAR = gtk_flow_box_new();
	gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(MBAR), GTK_SELECTION_NONE);
	gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(MBAR), (int)lround(rui_px(1)));
	gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(MBAR), 12);
	gtk_widget_set_margin_start(MBAR, (int)lround(rui_px(4)));
	gtk_widget_set_margin_end(MBAR, (int)lround(rui_px(4)));
	gtk_widget_set_margin_top(MBAR, (int)lround(rui_px(2))); gtk_widget_set_margin_bottom(MBAR, (int)lround(rui_px(2)));
	gtk_style_context_add_class(gtk_widget_get_style_context(MBAR), "wp-mb");
	for (int i = 0; i < NMENUS; i++) {
		GtkWidget *b = gtk_button_new_with_label(rui_tr(MENUS[i].title));
		gtk_style_context_add_class(gtk_widget_get_style_context(b), "rui");
		gtk_style_context_add_class(gtk_widget_get_style_context(b), "wp-mb-b");
		gtk_widget_set_can_focus(b, FALSE);
		g_signal_connect(b, "clicked", G_CALLBACK(mb_click), (gpointer)(intptr_t)i);
		gtk_flow_box_insert(GTK_FLOW_BOX(MBAR), b, -1); MBTNS[i] = b;
	}
	gtk_box_pack_start(GTK_BOX(W->body), MBAR, FALSE, FALSE, 0);
}
static gboolean menu_hook(gpointer d)
{
	int m = (int)(intptr_t)d;
	if (m >= 0 && m < NMENUS) open_menu(MBTNS[m], m);
	return G_SOURCE_REMOVE;
}

int main(int argc, char **argv)
{
	rui_init(&argc, &argv, "wordpro");
	css();
	W = rui_window("wordpro", "WordPro", 980, 646);
	build_menubar(); build_find(); build_toolbar(); build_editor(); build_status();
	g_signal_connect(ED, "key-press-event", G_CALLBACK(key), NULL);
	gtk_widget_show_all(W->win); gtk_widget_hide(FIND);
	gtk_widget_grab_focus(ED);   /* (the page's window opens with its writing ready: typing goes into it at once) */
	counts(); state_soon();
	{ const char *w = getenv("RUDOLF_WP_DO"); if (w && *w) g_timeout_add(200, do_hook, (gpointer)w); }
	gtk_main(); return 0;
}
