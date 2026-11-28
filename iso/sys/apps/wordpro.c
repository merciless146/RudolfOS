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
 * Tests' hooks: RUDOLF_WP_STATE=FILE (what it shows: the toolbar's choices, the counts, the text and its tags, where the
 * undo stands, and the page's objects -- a table's rows, columns, heading row and borders, a picture's size, description,
 * width and alignment). RUDOLF_WP_DIALOG=table|picture opens one of the page's dialogs as its menu opens it,
 * RUDOLF_WP_PICTOOLS=1 opens a picture's tools as a click on it does, and RUDOLF_WP_ANSWER=FILE names the answer the test
 * would press -- a dialog's button, and the fields it would fill ("0=/tmp/pic.png", "2=Deer") before it.
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
static char *DOCPATH;   /* (the document's file, once it has been opened or saved: Save writes it, Save As asks) */
static int DIRTY, ASK;   /* (the page's unsaved changes: 1 while the writing is not in its file, and whether its question is up) */
static int ASK_LAST = -1;   /* (the answer the question was given, for the state the tests read) */
static char ON[40];   /* (the toggles the cursor's place has, as the page's updSel writes them) */

/* ------------------------------------------------------------------ the menu bar: the page's own (base_v24.html's MENUS,
   generated into wordpro-gen.h by mkwordpro.py at build time -- its titles, labels, keys and separators, and an id per
   item). What the app can do for an item is said here, by the item's id: an item with no op is drawn greyed and refuses,
   so the bar cannot promise a command the app has not got, and an item the page drops stops the build (its id is gone)
   instead of leaving a dead entry behind. */
enum { M_CLOSE, M_NEW, M_OPEN, M_SAVE, M_SAVEAS, M_UNDO, M_REDO, M_CUT, M_COPY, M_PASTE, M_SELALL, M_FIND, M_FINDREP,
	M_ZOOM, M_TOGGLE, M_LIST, M_QUOTE, M_CASE, M_WHEN, M_CLEAR, M_TABLE, M_PICTURE, M_LINK, M_CHARS, M_STATS, M_KEYS, M_ABOUT };
typedef struct { int id, op; const char *arg; } WpOp;
static const WpOp OPS[] = {
	{ WPI_FILE_NEW, M_NEW, NULL }, { WPI_FILE_OPEN, M_OPEN, NULL }, { WPI_FILE_CLOSE, M_CLOSE, NULL },
	{ WPI_FILE_SAVE, M_SAVE, NULL }, { WPI_FILE_SAVE_AS, M_SAVEAS, NULL },
	{ WPI_FILE_DOCUMENT_STATISTICS, M_STATS, NULL }, { WPI_TOOLS_DOCUMENT_STATISTICS, M_STATS, NULL },
	{ WPI_EDIT_CUT, M_CUT, NULL }, { WPI_EDIT_COPY, M_COPY, NULL }, { WPI_EDIT_PASTE, M_PASTE, NULL },
	{ WPI_EDIT_PASTE_AS_PLAIN_TEXT, M_PASTE, "plain" },
	{ WPI_INSERT_TABLE, M_TABLE, NULL }, { WPI_TABLE_INSERT_TABLE, M_TABLE, NULL },
	{ WPI_INSERT_PICTURE, M_PICTURE, NULL }, { WPI_INSERT_HYPERLINK, M_LINK, NULL },
	{ WPI_INSERT_DATE_LONG, M_WHEN, "long" }, { WPI_INSERT_DATE_SHORT, M_WHEN, "short" },
	{ WPI_INSERT_TIME, M_WHEN, "time" }, { WPI_INSERT_SPECIAL_CHARACTERS, M_CHARS, NULL },
	{ WPI_FORMAT_UPPERCASE, M_CASE, "up" }, { WPI_FORMAT_LOWERCASE, M_CASE, "low" },
	{ WPI_FORMAT_TITLE_CASE, M_CASE, "title" }, { WPI_FORMAT_SENTENCE_CASE, M_CASE, "sen" },
	{ WPI_FORMAT_BLOCK_QUOTE, M_QUOTE, NULL },
	{ WPI_EDIT_UNDO, M_UNDO, NULL }, { WPI_EDIT_REDO, M_REDO, NULL }, { WPI_EDIT_SELECT_ALL, M_SELALL, NULL },
	{ WPI_EDIT_FIND, M_FIND, NULL }, { WPI_EDIT_FIND_AND_REPLACE, M_FINDREP, NULL },
	{ WPI_VIEW_ZOOM_50, M_ZOOM, "50" }, { WPI_VIEW_ZOOM_75, M_ZOOM, "75" }, { WPI_VIEW_ZOOM_100, M_ZOOM, "100" },
	{ WPI_VIEW_ZOOM_125, M_ZOOM, "125" }, { WPI_VIEW_ZOOM_150, M_ZOOM, "150" }, { WPI_VIEW_ZOOM_200, M_ZOOM, "200" },
	{ WPI_FORMAT_BOLD, M_TOGGLE, "b" }, { WPI_FORMAT_ITALIC, M_TOGGLE, "i" }, { WPI_FORMAT_UNDERLINE, M_TOGGLE, "u" },
	{ WPI_FORMAT_STRIKETHROUGH, M_TOGGLE, "s" }, { WPI_FORMAT_SUBSCRIPT, M_TOGGLE, "sub" },
	{ WPI_FORMAT_SUPERSCRIPT, M_TOGGLE, "sup" }, { WPI_FORMAT_CLEAR_FORMATTING, M_CLEAR, NULL },
	{ WPI_FORMAT_BULLETED_LIST, M_LIST, "bul" }, { WPI_FORMAT_NUMBERED_LIST, M_LIST, "num" },
	{ WPI_FORMAT_INCREASE_INDENT, M_LIST, "ind" }, { WPI_FORMAT_DECREASE_INDENT, M_LIST, "out" },
	{ WPI_HELP_KEYBOARD_SHORTCUTS, M_KEYS, NULL }, { WPI_HELP_ABOUT_WORDPRO, M_ABOUT, NULL } };
static const WpOp *op_of(int id) { for (guint i = 0; i < G_N_ELEMENTS(OPS); i++) if (OPS[i].id == id) return &OPS[i]; return NULL; }
#define NMENUS WPMENUS_N
static int MENU_OPEN_I = -1;   /* (the menu open now: the tests read it, and a second press on its button closes it) */

/* ------------------------------------------------------------------ tags, and the page's "select some text first" */
static void state_soon(void);
static gboolean menu_hook(gpointer d);
static void menu_do(int id);
static void hist_push(void); static void hist_undo(void); static void hist_redo(void);
static void counts(void);
static void upd_sel(void);
static void obj_counts(int *w, int *c);
static void state_objs(GString *s);
static void state_steps(GString *s);
static void insert_picture_data(guchar *data, gsize len, const char *alt, int wpct, int walign);   /* (the bytes are the
	caller's: they are the model's when it takes them, and freed here when they could not be read) */
static void open_doc(const char *path);
static void hist_push(void);
static int save_now(void);
/* (a link: the stretch of the writing carrying an address -- the page's <a href>, kept as the run's own tag "a:<address>") */
static GtkTextTag *link_at(GtkTextIter *a, GtkTextIter *b);
static char *link_line(void);
/* (the page's title: the file's name, or Untitled, and a * while the writing is not in its file) */
static void set_title(void)
{
	if (!W || !W->win) return;
	char *base = DOCPATH ? g_path_get_basename(DOCPATH) : NULL;
	char t[220];
	snprintf(t, sizeof t, "%s%s - WordPro", base ? base : rui_tr("Untitled"), DIRTY ? " *" : "");
	gtk_window_set_title(GTK_WINDOW(W->win), t);
	g_free(base); state_soon();
}
static void touch(void) { if (!DIRTY) { DIRTY = 1; set_title(); } }
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
					".wp-mb{background:#f3f4f6;border-bottom:1px solid #d1d5db}"
		"button.wp-mb-b{border:1px solid transparent;background:transparent;border-radius:4px;font-size:13px;color:#111;padding:0 9px;min-height:0;min-width:0}"
		"button.wp-mb-b:hover{background:#dbe6ff}"
					".wp-table{border:1px solid #d1d5db;background:#fff}"
		"entry.wp-cell{border:1px solid #d1d5db;border-radius:0;background:#fff;font-size:12px;color:#111;padding:0 4px;min-height:0}"
		".wp-table-nb, .wp-table-nb entry.wp-cell{border-color:transparent}"
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
	if (t) return t;
	/* (a link looks like one: the page's .wp-ed a, underlined and in its own blue) */
	if (!strncmp(name, "a:", 2))
		return gtk_text_buffer_create_tag(BUF, name, "underline", PANGO_UNDERLINE_SINGLE, "foreground", "#1d4ed8", NULL);
	return gtk_text_buffer_create_tag(BUF, name, NULL);
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
	GtkTextTag *t = tag_new(name); g_object_set(G_OBJECT(t), prop, v, NULL); gtk_text_buffer_apply_tag(BUF, t, &s, &e); counts(); hist_push(); state_soon();
}
static void apply_dbl(const char *name, const char *prop, double v)
{
	GtkTextIter s, e; if (!need_sel() || !sel(&s, &e)) return;
	GtkTextTag *t = tag_new(name); g_object_set(G_OBJECT(t), prop, v, NULL); gtk_text_buffer_apply_tag(BUF, t, &s, &e); counts(); hist_push(); state_soon();
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
	gtk_text_buffer_apply_tag(BUF, t, &s, &e); counts(); hist_push(); state_soon();
}
static void on_align(GtkWidget *b, gpointer d)
{
	(void)b; GtkTextIter s, e; if (!need_sel()) return;
	const char *k = (const char *)d;
	char n[24]; snprintf(n, sizeof n, "al:%s", k);
	GtkTextTag *t = tag_new(n);
	g_object_set(G_OBJECT(t), "justification", !strcmp(k, "center") ? GTK_JUSTIFY_CENTER : !strcmp(k, "right") ? GTK_JUSTIFY_RIGHT :
		!strcmp(k, "fill") ? GTK_JUSTIFY_FILL : GTK_JUSTIFY_LEFT, NULL);
	gtk_text_buffer_apply_tag(BUF, t, &s, &e); counts(); hist_push(); state_soon();
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
	gtk_text_buffer_apply_tag(BUF, t, &s, &e); counts(); hist_push(); state_soon();
}
static void on_clear(GtkWidget *b, gpointer d)
{
	(void)b; (void)d; GtkTextIter s, e; if (!need_sel()) return;
	gtk_text_buffer_remove_all_tags(BUF, &s, &e); counts(); hist_push(); state_soon();
}
/* undo and redo: GTK 3's text buffer keeps no history of its own, so the writing is kept step by step -- and what stands at
   each of its places with it (the page's undo and redo, bounded to the last 120 changes). What an entry is, and how it is
   put back, is said at the entry itself, below the objects. */
#define HIST 120
typedef struct Hist Hist;   /* (the text, and what stands at each of its places: made below, after the objects are) */
static Hist *HIST_T[HIST]; static int HIST_N, HIST_I, SILENT;
static void hist_push(void); static void hist_apply(Hist *h); static void obj_clear(void);
static Hist *hist_now(void); static int hist_same(Hist *a, Hist *b); static void hist_free(Hist *h);
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
/* (a picture or a table's place is the page's placeholder character, not a word and not a character of the writing) */
static int words_of(const char *t) { int n = 0, in = 0; for (const char *p = t; *p; p++) {
	if (!strncmp(p, "\xef\xbf\xbc", 3)) { p += 2; in = 0; continue; }
	if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') in = 0; else if (!in) { in = 1; n++; } } return n; }
/* (a character is a character, not its bytes: the page counts its document's own -- © is one -- and only the newlines
   and the places of pictures and tables (U+FFFC) are not counted) */
static int chars_of(const char *t) { int n = 0; for (const char *p = t; *p; ) {
	gunichar c = g_utf8_get_char(p); p = g_utf8_next_char(p);
	if (c == 0xfffc || c == '\n' || c == '\r') continue; n++; } return n; }
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
	GString *s = g_string_new(NULL); char *t = doc_text(); int w = words_of(t), c = chars_of(t);
	obj_counts(&w, &c);   /* (what the window shows: a table has words in its cells, as counts() counts them) */
	GtkTextIter a, b; int has = sel(&a, &b);
	g_string_append_printf(s, "font %s\nsize %.1f\nzoom %d\nwords %d\nchars %d\nsel %d\nsay %s\nstyles %d\nfonts %d\n",
		FAMILY, PT, ZOOM, w, c, has, SAY, NWPSTYLES, NWPFONTS);
	if (MENU_OPEN_I >= 0) g_string_append_printf(s, "menu %s %d\n", WP_MENUS[MENU_OPEN_I].title, WP_MENUS[MENU_OPEN_I].n);
	state_objs(s);
	g_string_append_printf(s, "hist %d %d\n", HIST_I, HIST_N);   /* (where the undo stands: the tests read it) */
	state_steps(s);
	if (FIND && FI && REPL) g_string_append_printf(s, "findbar %d\nfind %s\nrepl %s\n", gtk_widget_get_visible(FIND) ? 1 : 0,
		gtk_entry_get_text(GTK_ENTRY(FI)), gtk_entry_get_text(GTK_ENTRY(REPL)));
	g_string_append_printf(s, "dirty %d\nask %d\nanswer %d\ndialog %d\n", DIRTY, ASK, ASK_LAST, rui_dialog_box() ? 1 : 0);   /* (the tests read the guard: what is not saved, its question, how it was answered, and a dialog standing open) */
	g_string_append_printf(s, "on %s\n", ON);
	{ char *lk = link_line(); g_string_append_printf(s, "link %s\n", lk); g_free(lk); }   /* (the address the cursor stands in, if any) */
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
	for (int i = 0; i < NMENUS; i++) g_string_append_printf(s, "%s%s", i ? "," : " ", WP_MENUS[i].title);
	g_string_append_c(s, '\n');
	/* (what the page has and the app has no command for, menu by menu: the tests read these, so a command that
	   joins the page's bar cannot arrive here quietly, greyed, with nobody noticing) */
	for (int i = 0; i < NMENUS; i++) {
		int later = 0;
		for (int k = 0; k < WP_MENUS[i].n; k++) { const WpItem *m = &WP_MENUS[i].items[k]; if (m->label && !op_of(m->id)) later++; }
		g_string_append_printf(s, "later %s %d\n", WP_MENUS[i].title, later);
	}
	if (MENU_OPEN_I >= 0) {
		g_string_append_printf(s, "menuopen %s %d\n", WP_MENUS[MENU_OPEN_I].title, WP_MENUS[MENU_OPEN_I].n);
		g_string_append(s, "grey");
		for (int i = 0; i < WP_MENUS[MENU_OPEN_I].n; i++) { const WpItem *m = &WP_MENUS[MENU_OPEN_I].items[i]; if (m->label && !op_of(m->id)) g_string_append_printf(s, " %s", m->label); }
		g_string_append_c(s, '\n');
	}
	g_string_append_printf(s, "text %s\n", t); g_free(t);
	char *tmp = g_strdup_printf("%s.t", f); g_file_set_contents(tmp, s->str, -1, NULL); rename(tmp, f); g_free(tmp); g_string_free(s, TRUE);
}
static gboolean state_tick(gpointer d) { (void)d; state_write(); return G_SOURCE_REMOVE; }
static void state_soon(void) { if (getenv("RUDOLF_WP_STATE")) g_timeout_add(120, state_tick, NULL); }
static void hist_push(void)
{
	if (SILENT) return;
	Hist *h = hist_now();
	if (HIST_N && hist_same(HIST_T[HIST_N - 1], h)) { hist_free(h); return; }
	if (HIST_I < HIST_N) { for (int i = HIST_I; i < HIST_N; i++) { hist_free(HIST_T[i]); HIST_T[i] = NULL; } HIST_N = HIST_I; }
	if (HIST_N == HIST) { hist_free(HIST_T[0]); memmove(HIST_T, HIST_T + 1, sizeof(HIST_T[0]) * (HIST - 1)); HIST_N--; }
	HIST_T[HIST_N++] = h; HIST_I = HIST_N;
	touch();   /* (the writing is not its file's any more: the title says so, as the page's) */
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
static void noop(int i, void *d) { (void)i; (void)d; }
/* the page's question before the writing is thrown away (base_v24.html's confirmDiscard): Save, Don't save, or Cancel.
   It stands before New, Open and Close, as on the page, and nothing is thrown away until it is answered. */
enum { ASK_NEW = 1, ASK_OPEN, ASK_CLOSE };
static void do_what(int what, const char *path);
static int ASK_WHAT; static char *ASK_PATH;
static void ask_done(int i, void *d)
{
	(void)d; ASK = 0; ASK_LAST = i;   /* (what was answered, for the state the tests read) */
	if (ED) gtk_widget_grab_focus(ED);   /* (the writing has the keys again once the question is gone, as the page's) */
	if (i < 0) { state_soon(); return; }   /* (Cancel, or Escape's own answer: the page stays as it is) */
	if (i == 2 && !save_now()) { state_soon(); return; }   /* (a save that was cancelled: nothing is thrown away) */
	do_what(ASK_WHAT, ASK_PATH);
}
/* New is a step of the writing's own history, as every other change is: undo brings the page back, redo empties it again
   (a New that only emptied the buffer would leave undo and redo looking at a page that is no longer there) */
static void do_what(int what, const char *path)
{
	if (what == ASK_NEW) {
		g_free(DOCPATH); DOCPATH = NULL;
		SILENT = 1; obj_clear(); gtk_text_buffer_set_text(BUF, "", -1); SILENT = 0;
		hist_push(); DIRTY = 0;
		counts(); upd_sel(); set_title(); state_soon(); gtk_widget_grab_focus(ED);
	} else if (what == ASK_OPEN) { if (path) open_doc(path); }
	else if (what == ASK_CLOSE) gtk_widget_destroy(W->win);
}
static void confirm_discard(int what, const char *path)
{
	if (!DIRTY) { do_what(what, path); return; }   /* (nothing written since the file: no question, as the page's) */
	ASK_WHAT = what; g_free(ASK_PATH); ASK_PATH = path ? g_strdup(path) : NULL;
	char *base = DOCPATH ? g_path_get_basename(DOCPATH) : NULL;
	char t[220]; snprintf(t, sizeof t, "Save changes to %s?", base ? base : rui_tr("Untitled"));
	g_free(base);
	static const char *B[] = { "Cancel", "Don't save", "Save" };
	rui_confirm(W, rui_tr("WordPro"), rui_tr(t), B, 3, 1, ask_done, NULL);
	ASK = 1;
	state_soon();
}
static void new_page(void) { confirm_discard(ASK_NEW, NULL); }
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
/* ------------------------------------------------------------------ the document's objects: a picture and a table
   The model, decided before either was written (docs/plan-1.6.3.md, "The document model"): a picture or a table is a
   place in the writing -- a child anchor, which the page itself keeps as its placeholder character -- and its own data
   lives beside it here, so the cursor, the counts and the undo still see one writing in order, and a save writes what the
   page writes: an <img> or a <table>. A picture keeps its bytes (not a path: the page's document carries the picture);
   a table's cells are entries of their own, whose words are part of the page's word count. There is no cap on how many a
   page holds, and an object leaves the list when its widget goes -- which is what undo, New and a Backspace do to it. */
enum { OBJ_PIC = 1, OBJ_TABLE };
enum { WAL_INLINE = 0, WAL_LEFT, WAL_RIGHT, WAL_CENTER };   /* (the page's Align list in its picture tools, imgTools) */
#define PAGE_W 816.0   /* (the sheet's width in CSS px, as the page's .wp-ed is 816 px wide: a picture's width is its share) */
typedef struct { guchar *d; gsize n; int refs; } Img;   /* (a picture's bytes, shared: the writing and every history entry
															that has it point at one copy, so keeping the history costs no
															new memory for a picture that has not changed) */
static Img *img_new(guchar *data, gsize len) { Img *i = g_new0(Img, 1); i->d = data; i->n = len; i->refs = 1; return i; }
/* (a picture as the page draws it: its own bytes at its own size, or as wide as the width tool says -- a share of the
   sheet's 816 px, as imgTools' width is a share of the page -- placed in line, left, right or centred by its own box) */
static GdkPixbuf *pic_decode(Img *im)
{
	if (!im || !im->n) return NULL;
	GdkPixbufLoader *l = gdk_pixbuf_loader_new(); GdkPixbuf *pb = NULL;
	if (gdk_pixbuf_loader_write(l, im->d, im->n, NULL) && gdk_pixbuf_loader_close(l, NULL)) pb = gdk_pixbuf_loader_get_pixbuf(l);
	if (pb) g_object_ref(pb);
	g_object_unref(l);
	return pb;
}
static GtkWidget *pic_image(GdkPixbuf *pb, int wpct)
{
	GdkPixbuf *s = NULL;
	if (wpct > 0 && gdk_pixbuf_get_width(pb) > 0) {
		double want = PAGE_W * wpct / 100.0;
		int nw = MAX(1, (int)lround(want)), nh = MAX(1, (int)lround(gdk_pixbuf_get_height(pb) * want / gdk_pixbuf_get_width(pb)));
		s = gdk_pixbuf_scale_simple(pb, nw, nh, GDK_INTERP_BILINEAR);
	}
	GtkWidget *im = gtk_image_new_from_pixbuf(s ? s : pb);
	if (s) g_object_unref(s);
	return im;
}
static GtkAlign pic_align(int wa) { return wa == WAL_RIGHT ? GTK_ALIGN_END : wa == WAL_CENTER ? GTK_ALIGN_CENTER : GTK_ALIGN_START; }
static GtkWidget *pic_widget(GdkPixbuf *pb, int wpct, int walign)
{
	GtkWidget *e = gtk_event_box_new();
	gtk_widget_set_halign(e, pic_align(walign));
	gtk_container_add(GTK_CONTAINER(e), pic_image(pb, wpct));
	return e;
}
static Img *img_ref(Img *i) { if (i) i->refs++; return i; }
static void img_unref(Img *i) { if (i && --i->refs <= 0) { g_free(i->d); g_free(i); } }
typedef struct { int kind, wpx, hpx, rows, cols, hdr, border, wpct, walign; char *alt; GtkWidget *w; GtkTextChildAnchor *at; Img *im; } WpObj;
	/* (a table's hdr: its first row is its heading; border: 0 is the page's .nb, the table without borders. A picture's
	   alt: its description, the page's alt text; wpct: how wide as a share of the sheet, 0 for its own size; walign: its
	   place in the line, WAL_INLINE to WAL_CENTER) */
static void pic_tools(WpObj *o);   /* (the page's Picture dialog: width, alignment and description) */
static gboolean pic_click(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)w; if (e->button != 1 || e->type != GDK_BUTTON_RELEASE) return FALSE;
	pic_tools(d); return TRUE;
}
static GPtrArray *OBJ;
static void obj_free(gpointer p) { WpObj *o = p; img_unref(o->im); g_free(o->alt); g_free(o); }
static void obj_gone(GtkWidget *w, gpointer d)
{
	(void)d; if (!OBJ) return;
	for (guint i = 0; i < OBJ->len; i++) if (((WpObj *)g_ptr_array_index(OBJ, i))->w == w) { g_ptr_array_remove_index(OBJ, i); break; }
	counts(); state_soon();
}
static void obj_counts(int *w, int *c)
{
	if (!OBJ) return;
	for (guint i = 0; i < OBJ->len; i++) { WpObj *o = g_ptr_array_index(OBJ, i);
		if (o->kind != OBJ_TABLE || !o->w) continue;
		GList *ch = gtk_container_get_children(GTK_CONTAINER(o->w));
		for (GList *k = ch; k; k = k->next) if (GTK_IS_ENTRY(k->data)) {
			const char *t = gtk_entry_get_text(GTK_ENTRY(k->data)); *w += words_of(t); *c += chars_of(t);
		}
		g_list_free(ch);
	}
}
static const char *cell_text(WpObj *o, int r, int c);   /* (the cell's own text: what a table's entry carries) */
static void state_objs(GString *s)
{
	g_string_append_printf(s, "objs %d\n", OBJ ? (int)OBJ->len : 0);
	if (!OBJ) return;
	for (guint i = 0; i < OBJ->len; i++) { WpObj *o = g_ptr_array_index(OBJ, i);
		if (o->kind == OBJ_PIC) g_string_append_printf(s, "obj picture %dx%d %u\npic alt '%s' w %d align %d\n",
			o->wpx, o->hpx, (unsigned)(o->im ? o->im->n : 0), o->alt ? o->alt : "", o->wpct, o->walign);
		else {   /* (the cells' text, which is what an undo or a redo of a table has to bring back, and the two things the dialog chose) */
			g_string_append_printf(s, "obj table %dx%d\ntbl hdr %d nb %d\ncells", o->rows, o->cols, o->hdr, o->border ? 0 : 1);
			for (int r = 0; r < o->rows; r++) for (int c = 0; c < o->cols; c++) { g_string_append_c(s, '|'); g_string_append(s, cell_text(o, r, c)); }
			g_string_append_c(s, '\n');
		}
	}
}
/* (the object's widget goes into the writing at the anchor it was made for: the menu and the parser create the anchor,
   the history makes it again where an entry had one, and all three come through here) */
static void obj_place(GtkTextChildAnchor *a, WpObj *o)
{
	if (!OBJ) OBJ = g_ptr_array_new_with_free_func(obj_free);
	o->at = a;
	g_signal_connect(o->w, "destroy", G_CALLBACK(obj_gone), NULL);
	g_ptr_array_add(OBJ, o);
	gtk_widget_show_all(o->w);   /* (an object made after the window is shown is not shown with it, and an unrealized table is
									  not on the page: its grid and cells were never drawn, and a cell never took a key) */
	gtk_text_view_add_child_at_anchor(GTK_TEXT_VIEW(ED), o->w, a);
	if (o->kind == OBJ_PIC) {   /* (a click on a picture is the page's click on it: it opens the picture's tools) */
		gtk_widget_add_events(o->w, GDK_BUTTON_RELEASE_MASK);
		g_signal_connect(o->w, "button-release-event", G_CALLBACK(pic_click), o);
	}
}
/* (the objects go before the text that holds their places is replaced: a buffer change takes the place of a picture away,
   and the widget it destroys while GTK is in the middle of that change is what used to stop the window -- an undo, a New
   or an opened document all rewrote the writing under its own objects) */
static void obj_clear(void)
{
	if (!OBJ) return;
	for (guint i = 0; i < OBJ->len; i++) {
		WpObj *o = g_ptr_array_index(OBJ, i); GtkWidget *w = o->w;
		o->w = NULL;   /* (the list is emptied below in one step, so the destroy cannot take an entry of its own) */
		if (w) gtk_widget_destroy(w);
	}
	g_ptr_array_set_size(OBJ, 0);
}
typedef struct { int kind, wpx, hpx, rows, cols, hdr, border, wpct, walign; const char *alt; } ObjSpec;   /* (what the caller
	decided: a table's cells, its heading row and borders; a picture's size, its description and how it is placed) */
static void obj_add(GtkWidget *w, ObjSpec s, guchar *img, gsize img_len)
{
	GtkTextIter it; gtk_text_buffer_get_iter_at_mark(BUF, &it, gtk_text_buffer_get_insert(BUF));
	SILENT = 1;   /* (the anchor is not a step of the history of its own: the object standing at it is, below) */
	GtkTextChildAnchor *a = gtk_text_buffer_create_child_anchor(BUF, &it);
	SILENT = 0;
	WpObj *o = g_new0(WpObj, 1);
	o->kind = s.kind; o->wpx = s.wpx; o->hpx = s.hpx; o->rows = s.rows; o->cols = s.cols;
	o->hdr = s.hdr; o->border = s.border; o->wpct = s.wpct; o->walign = s.walign; o->alt = g_strdup(s.alt); o->w = w;
	o->im = img ? img_new(img, img_len) : NULL;
	obj_place(a, o);
	counts(); upd_sel(); state_soon(); gtk_widget_grab_focus(ED);
	hist_push();
}
/* (a table's cell: its writing is part of the page, so typing in it is a step of the history as typing in the page is --
   the entry keeps the cells, and the step is pushed as the cell's text changes. The page's undo and redo are read in the
   cell too, while the cursor stands in it, as a person's would: the writing is one, and so is its history) */
static void on_cell(GtkEditable *e, gpointer d)
{
	(void)e; (void)d; if (SILENT) return;
	hist_push(); counts(); state_soon();
}
static gboolean cell_key(GtkWidget *w, GdkEventKey *e, gpointer d)
{
	(void)w; (void)d; if (!(e->state & GDK_CONTROL_MASK)) return FALSE;
	if (e->keyval == GDK_KEY_z) { hist_undo(); return TRUE; }
	if (e->keyval == GDK_KEY_y) { hist_redo(); return TRUE; }
	return FALSE;
}
static GtkWidget *cell_new(int r, int c, const char *t)
{
	GtkWidget *e = gtk_entry_new(); gtk_entry_set_width_chars(GTK_ENTRY(e), 8);
	gtk_widget_set_size_request(e, (int)lround(rui_px(96)), (int)lround(rui_px(26)));
	gtk_style_context_add_class(gtk_widget_get_style_context(e), "wp-cell");
	g_object_set_data(G_OBJECT(e), "wp-cell", GINT_TO_POINTER(r * 100 + c + 1));   /* (+1: a cell at 0 would be a NULL pointer) */
	if (t && *t) gtk_entry_set_text(GTK_ENTRY(e), t);
	/* (the signals go on once the text is in: a cell being filled is not a step, what the typist changes is) */
	g_signal_connect(e, "changed", G_CALLBACK(on_cell), NULL);
	g_signal_connect(e, "key-press-event", G_CALLBACK(cell_key), NULL);
	return e;
}
static void insert_picture(const char *path, const char *alt)
{
	gchar *data = NULL; gsize len = 0;
	if (!g_file_get_contents(path, &data, &len, NULL)) { say(rui_tr("That picture could not be read.")); return; }
	insert_picture_data((guchar *)data, len, alt, 0, WAL_INLINE);   /* (the bytes, which is what the page's document carries) */
}
/* (a table's grid, with the page's wp-t or its wp-t nb: the .nb class is a table whose borders are not drawn) */
static GtkWidget *table_grid(int border)
{
	GtkWidget *g = gtk_grid_new();
	gtk_style_context_add_class(gtk_widget_get_style_context(g), "wp-table");
	if (!border) gtk_style_context_add_class(gtk_widget_get_style_context(g), "wp-table-nb");
	return g;
}
static void insert_table(int rows, int cols, int hdr, int border);
/* (the page's Insert table dialog -- base_v24.html's tableDlg: Rows and Columns, the first row a heading, borders drawn.
   Its limits are the page's own: 1 to 50 rows, 1 to 20 columns, and an empty or nonsense count is its 3) */
static void table_dlg_done(int i, const RuiField *f, void *d)
{
	(void)d; state_soon();   /* (the dialog is gone: what the page shows is written again, the table or not) */
	if (i != 1) return;   /* (Insert: the last of its buttons, which the page makes its primary) */
	int r = atoi(f[0].value ? f[0].value : ""), c = atoi(f[1].value ? f[1].value : "");
	if (!r) r = 3; if (r < 1) r = 1; if (r > 50) r = 50;
	if (!c) c = 3; if (c < 1) c = 1; if (c > 20) c = 20;
	insert_table(r, c, f[2].value && f[2].value[0] == '1', f[3].value && f[3].value[0] == '1');
}
static void table_dlg(void)
{
	RuiField f[4] = { { "Rows", RUI_F_NUM, "3" }, { "Columns", RUI_F_NUM, "3" },
		{ "Header row", RUI_F_CHECK, "1" }, { "Borders", RUI_F_CHECK, "1" } };
	static const char *B[] = { "Cancel", "Insert" };
	rui_form(W, rui_tr("Insert table"), NULL, f, 4, B, 2, 1, table_dlg_done, NULL);
	state_soon();   /* (the page says a dialog stands open: a check reads it here) */
}
static void insert_table(int rows, int cols, int hdr, int border)
{
	GtkWidget *g = table_grid(border);
	for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++) gtk_grid_attach(GTK_GRID(g), cell_new(r, c, NULL), c, r, 1, 1);
	ObjSpec s = { .kind = OBJ_TABLE, .rows = rows, .cols = cols, .hdr = hdr, .border = border };
	obj_add(g, s, NULL, 0);
}
/* (the page's Insert picture dialog -- base_v24.html's imgDialog: a file from this computer, a web address, and the
   description that becomes the picture's alt text. A picture fetched over a network address is not here yet: the page
   asks the firewall first and reads the picture the browser's way, and both are a step of their own, so it says so.) */
static void pic_dlg_done(int i, const RuiField *f, void *d)
{
	(void)d; state_soon();
	if (i != 1) return;   /* (Insert: the last of its buttons, the page's primary) */
	const char *path = f[0].value ? f[0].value : "", *url = f[1].value ? f[1].value : "", *alt = f[2].value ? f[2].value : "";
	if (!*path && !*url) { say(rui_tr("Choose a file or enter a web address.")); return; }
	if (!*path) { say(rui_tr("A web address is not read here yet: choose a file.")); return; }
	insert_picture(path, alt);
}
static void pic_dlg(void)
{
	RuiField f[3] = { { "From computer", RUI_F_FILE, "" }, { "Web address", RUI_F_TEXT, "" }, { "Description", RUI_F_TEXT, "" } };
	static const char *B[] = { "Cancel", "Insert" };
	rui_form(W, rui_tr("Insert picture"), NULL, f, 3, B, 2, 1, pic_dlg_done, NULL);
	state_soon();   /* (the page says a dialog stands open: a check reads it here) */
}
/* (the page's Picture dialog -- imgTools: the width as a share of the sheet, the alignment, the description, and Delete
   picture / Cancel / OK. It opens as the page's opens: on the picture a click chose) */
static const char *const PIC_W[] = { "Original", "25%", "50%", "75%", "100%" };
static const char *const PIC_A[] = { "In line", "Left (wrap text)", "Right (wrap text)", "Centered" };
static WpObj *PIC_EDIT;
static void pic_del(WpObj *o)
{
	if (!o || !o->at) return;
	GtkTextIter a, b; gtk_text_buffer_get_iter_at_child_anchor(BUF, &a, o->at); b = a; gtk_text_iter_forward_char(&b);
	SILENT = 1; gtk_text_buffer_delete(BUF, &a, &b); SILENT = 0;   /* (the picture's widget goes with its place) */
	counts(); state_soon(); hist_push();
}
static void pic_tools_done(int i, const RuiField *f, void *d)
{
	(void)d; state_soon();
	WpObj *o = PIC_EDIT; PIC_EDIT = NULL;
	if (!o || i < 0) return;
	if (i == 0) { pic_del(o); return; }   /* (Delete picture) */
	if (i != 2) return;   /* (Cancel) */
	const char *wv = f[0].value ? f[0].value : "Original";
	int wpct = strcmp(wv, "Original") ? atoi(wv) : 0;
	int walign = WAL_INLINE; for (int k = 0; k < 4; k++) if (f[1].value && !strcmp(f[1].value, PIC_A[k])) walign = k;
	const char *alt = f[2].value ? f[2].value : "";
	if (wpct == o->wpct && walign == o->walign && !g_strcmp0(alt, o->alt ? o->alt : "")) return;   /* (nothing changed: no step) */
	o->wpct = wpct; o->walign = walign; g_free(o->alt); o->alt = g_strdup(alt);
	GdkPixbuf *pb = pic_decode(o->im);   /* (the picture drawn again as its tools say: its bytes are the model's, and what
											 changed is how wide it stands and where) */
	if (pb) {
		GtkWidget *im = pic_image(pb, o->wpct); g_object_unref(pb);
		GList *ch = gtk_container_get_children(GTK_CONTAINER(o->w));
		for (GList *k = ch; k; k = k->next) gtk_widget_destroy(k->data);
		g_list_free(ch);
		gtk_container_add(GTK_CONTAINER(o->w), im);
		gtk_widget_set_halign(o->w, pic_align(o->walign));
		gtk_widget_show_all(o->w);
	}
	counts(); state_soon(); hist_push();
}
static void pic_tools(WpObj *o)
{
	if (!o) return;
	PIC_EDIT = o;
	char wv[16] = "Original";
	if (o->wpct > 0) snprintf(wv, sizeof wv, "%d%%", o->wpct);
	RuiField f[3] = { { "Width", RUI_F_SEL, wv, PIC_W, 5 }, { "Align", RUI_F_SEL, PIC_A[o->walign], PIC_A, 4 },
		{ "Description", RUI_F_TEXT, o->alt ? o->alt : "" } };
	static const char *B[] = { "Delete picture", "Cancel", "OK" };
	rui_form(W, rui_tr("Picture"), NULL, f, 3, B, 3, 1, pic_tools_done, NULL);
	state_soon();
}
/* ------------------------------------------------------------------ the page's Hyperlink dialog (linkDialog) and links
   A link is a stretch of the writing carrying an address: the page's <a href> is kept as the run's own tag, "a:<address>",
   which is what the history carries with the words (a run's tags go back with its text), what the save writes as
   <a href="..."> and what the state says (link <address>). */
static GtkTextTag *link_tag_in(GtkTextIter *i)
{
	GtkTextTag *t = NULL; GSList *l = gtk_text_iter_get_tags(i);
	for (GSList *k = l; k && !t; k = k->next) {
		char *nm = NULL; g_object_get(G_OBJECT(k->data), "name", &nm, NULL);
		if (nm && !strncmp(nm, "a:", 2)) t = k->data;
		g_free(nm);
	}
	g_slist_free(l);
	return t;
}
static GtkTextTag *link_at(GtkTextIter *a, GtkTextIter *b)
{
	GtkTextIter it; gtk_text_buffer_get_iter_at_mark(BUF, &it, gtk_text_buffer_get_insert(BUF));
	GtkTextTag *t = link_tag_in(&it);
	/* (a caret just after a link's words is in the link, as a browser's caret at an anchor's end is -- the character before
	   it is what says so, because a tag that ends at a place is not one of the tags at that place) */
	if (!t && gtk_text_iter_get_offset(&it) > 0) { GtkTextIter p = it; gtk_text_iter_backward_char(&p); t = link_tag_in(&p); }
	if (!t) return NULL;
	*a = it; *b = it;
	if (!gtk_text_iter_starts_tag(&it, t)) gtk_text_iter_backward_to_tag_toggle(a, t);
	if (!gtk_text_iter_ends_tag(&it, t)) gtk_text_iter_forward_to_tag_toggle(b, t);
	return t;
}
static char *link_line(void)
{
	GtkTextIter a, b; GtkTextTag *t = link_at(&a, &b); char *nm = NULL;
	if (!t) return g_strdup("");
	g_object_get(G_OBJECT(t), "name", &nm, NULL);
	char *r = g_strdup(nm ? nm + 2 : "");
	g_free(nm);
	return r;
}
/* (the page's own rule for an address: an http, https, mailto, file or #-address as it is typed, anything else a web one) */
static int link_absolute(const char *u)
{
	return !g_ascii_strncasecmp(u, "http:", 5) || !g_ascii_strncasecmp(u, "https:", 6) ||
		!g_ascii_strncasecmp(u, "mailto:", 7) || !g_ascii_strncasecmp(u, "file:", 5) || u[0] == '#';
}
static void link_remove(void)
{
	GtkTextIter a, b; GtkTextTag *t = link_at(&a, &b);
	if (!t) return;
	gtk_text_buffer_remove_tag(BUF, t, &a, &b);   /* (the words stay; only the address goes, as the page's unlink does) */
	counts(); upd_sel(); hist_push(); state_soon();
}
/* (the dialog's OK: the words carry the address. The words replace what is chosen, as the page's insertHTML does, and when
   a link stands at the cursor they are that link's own words replaced) */
static void link_apply(const char *addr, const char *words)
{
	/* (the places are kept as offsets: a change in the buffer moves or ends an iterator, and the range is read back from
	   the offsets after it -- the same way put_at_end reads an opened document's range) */
	char *tn = g_strconcat("a:", addr, NULL); GtkTextTag *nt = tag_new(tn); g_free(tn);
	GtkTextIter la, lb; GtkTextTag *old = link_at(&la, &lb);
	int oa = old ? gtk_text_iter_get_offset(&la) : 0, ob = old ? gtk_text_iter_get_offset(&lb) : 0;
	GtkTextIter s, e; int has = sel(&s, &e), off, eo = 0;
	if (has) { off = gtk_text_iter_get_offset(&s); eo = gtk_text_iter_get_offset(&e); }
	else { GtkTextIter c; gtk_text_buffer_get_iter_at_mark(BUF, &c, gtk_text_buffer_get_insert(BUF)); off = gtk_text_iter_get_offset(&c); }
	SILENT = 1;   /* (one step of the history: the link is put in, and its words are not a step of their own) */
	if (old) {
		GtkTextIter a2, b2; gtk_text_buffer_get_iter_at_offset(BUF, &a2, oa); gtk_text_buffer_get_iter_at_offset(BUF, &b2, ob);
		gtk_text_buffer_remove_tag(BUF, old, &a2, &b2);
	}
	if (has) {
		GtkTextIter a2, b2; gtk_text_buffer_get_iter_at_offset(BUF, &a2, off); gtk_text_buffer_get_iter_at_offset(BUF, &b2, eo);
		gtk_text_buffer_delete(BUF, &a2, &b2);
	}
	{ int n = (int)g_utf8_strlen(words, -1); GtkTextIter i, a2, b2;
	  gtk_text_buffer_get_iter_at_offset(BUF, &i, off); gtk_text_buffer_insert(BUF, &i, words, -1);
	  gtk_text_buffer_get_iter_at_offset(BUF, &a2, off); gtk_text_buffer_get_iter_at_offset(BUF, &b2, off + n);
	  gtk_text_buffer_apply_tag(BUF, nt, &a2, &b2); }
	SILENT = 0;
	counts(); upd_sel(); hist_push(); state_soon();
}
static int LINK_NB;   /* (the buttons the dialog opened with: OK is its last, Remove link its middle when one stands there) */
static void link_dlg_done(int i, const RuiField *f, void *d);
static void link_dlg_open(const char *words, const char *addr)
{
	static const char *B2[] = { "Cancel", "OK" };
	static const char *B3[] = { "Cancel", "Remove link", "OK" };
	GtkTextIter la, lb; int in = link_at(&la, &lb) != NULL;
	RuiField f[2] = { { "Text", RUI_F_TEXT, words ? words : "" }, { "Address", RUI_F_TEXT, addr ? addr : "https://" } };
	LINK_NB = in ? 3 : 2;
	rui_form(W, rui_tr("Hyperlink"), NULL, f, 2, in ? B3 : B2, LINK_NB, 1, link_dlg_done, NULL);
	state_soon();
}
static void link_dlg_done(int i, const RuiField *f, void *d)
{
	(void)d; state_soon();
	if (i < 0) return;   /* (Escape is Cancel: nothing written) */
	if (i == LINK_NB - 1) {   /* (OK: the page's own rule, and its own words when none is typed) */
		char *a = g_strstrip(g_strdup(f[1].value ? f[1].value : ""));
		char *t = g_strstrip(g_strdup(f[0].value ? f[0].value : ""));
		if (!*a || !strcmp(a, "https://")) {   /* (the page keeps its dialog standing: this one says so, and opens again) */
			say(rui_tr("Enter an address.")); link_dlg_open(t, a); g_free(a); g_free(t); return; }
		GString *n = g_string_new(NULL);
		if (!link_absolute(a)) g_string_append(n, "https://");
		g_string_append(n, a);
		link_apply(n->str, *t ? t : n->str);
		g_string_free(n, TRUE); g_free(a); g_free(t);
		return;
	}
	if (LINK_NB == 3 && i == 1) link_remove();   /* (Remove link) */
}
static void link_dlg(void)
{
	GtkTextIter la, lb; GtkTextTag *t = link_at(&la, &lb); GtkTextIter s, e;
	char *w = NULL, *a = NULL;
	if (t) {   /* (the page's own: the link's words and its address) */
		char *nm = NULL; g_object_get(G_OBJECT(t), "name", &nm, NULL);
		w = gtk_text_buffer_get_text(BUF, &la, &lb, FALSE); a = g_strdup(nm ? nm + 2 : ""); g_free(nm);
	} else if (sel(&s, &e)) w = gtk_text_buffer_get_text(BUF, &s, &e, FALSE);   /* (or what is chosen) */
	link_dlg_open(w ? w : "", a ? a : "https://");
	g_free(w); g_free(a);
}
/* ------------------------------------------------------------------ the page's Special characters dialog (specialDlg)
   The page's own WP_CHARS in its own order, a grid of buttons: a press puts the character in at the cursor and leaves the
   dialog standing, and Close ends it -- which is what its dialog() with a click on its body does */
static void chars_pick(int i, const char *cell, void *d)
{
	(void)i; (void)d;
	if (!cell) { state_soon(); return; }   /* (a button of its own: Close) */
	insert_here(cell);   /* (a character, put in as typing puts it in -- the page's insertText -- and a step of the history) */
}
static void chars_dlg(void)
{
	static const char *B[] = { "Close" };
	rui_pick(W, rui_tr("Special characters"), WP_CHARS, NWPCHARS, 12, B, 1, 1, chars_pick, NULL);
	state_soon();
}
/* the page's Paste as plain text: the clipboard's text, and none of what came with it */
static void paste_plain(void)
{
	char *t = gtk_clipboard_wait_for_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
	if (!t) return;
	insert_here(t);
	g_free(t);
}
/* (the tests: the cursor in a table's first cell, where a click on it puts it -- the display a check runs on has no
   window manager, so where a page's cell is on the screen is not something the check can know) */
static gboolean focus_cell_hook(gpointer d)
{
	(void)d; if (!OBJ) return G_SOURCE_REMOVE;
	for (guint i = 0; i < OBJ->len; i++) {
		WpObj *o = g_ptr_array_index(OBJ, i);
		if (o->kind != OBJ_TABLE || !o->w) continue;
		GList *ch = gtk_container_get_children(GTK_CONTAINER(o->w));
		for (GList *k = ch; k; k = k->next) if (GTK_IS_ENTRY(k->data) && GPOINTER_TO_INT(g_object_get_data(G_OBJECT(k->data), "wp-cell")) == 1) {
			gtk_widget_grab_focus(k->data);
			/* (a click in a cell puts the cursor where it stands -- the focus alone chooses all of the cell's text, and the
			   first letter typed would then be two changes: the choice taken away, and the letter in) */
			gtk_editable_set_position(GTK_EDITABLE(k->data), -1);
			break; }
		g_list_free(ch);
		break;
	}
	return G_SOURCE_REMOVE;
}
static gboolean insert_hook(gpointer d)
{
	const char *v = d;
	if (!strncmp(v, "picture:", 8)) insert_picture(v + 8, NULL);
	else if (!strcmp(v, "table")) insert_table(3, 3, 1, 1);
	else if (!strncmp(v, "table:", 6)) {   /* (the dialog's own choices, without a display to press its buttons on) */
		int r = 3, c = 3, h = 1, b = 1;
		if (sscanf(v + 6, "%dx%d:%d:%d", &r, &c, &h, &b) < 2) { r = 3; c = 3; h = 1; b = 1; }
		if (!r) r = 3; if (r < 1) r = 1; if (r > 50) r = 50;
		if (!c) c = 3; if (c < 1) c = 1; if (c > 20) c = 20;
		insert_table(r, c, h, b);
	}
	return G_SOURCE_REMOVE;
}
static gboolean dialog_hook(gpointer d)
{
	const char *v = d;   /* (the tests: the page's dialog opened as its menu opens it) */
	if (!strcmp(v, "table")) table_dlg(); else if (!strcmp(v, "picture")) pic_dlg();
	else if (!strcmp(v, "link")) link_dlg(); else if (!strcmp(v, "chars")) chars_dlg();
	return G_SOURCE_REMOVE;
}
static gboolean pic_tools_hook(gpointer d)
{
	(void)d;   /* (the tests: the picture tools opened as a click on the picture opens them) */
	if (OBJ) for (guint i = 0; i < OBJ->len; i++) { WpObj *o = g_ptr_array_index(OBJ, i); if (o->kind == OBJ_PIC) { pic_tools(o); break; } }
	return G_SOURCE_REMOVE;
}
/* ------------------------------------------------------------------ saving and opening the page's .DOC
   docs/plan-1.6.3.md, "The document model": the file is exactly the one the web WordPro writes -- its meta comment, its
   stylesheet in <style data-wp>, then the blocks as its HTML: paragraphs as <p>/<h1>-<h4>/<blockquote>/<pre>, the inline
   tags as <b>, <i>, <u>, <s>, <sub>, <sup> and the font, size and colour as the page's own spans, a table as <table>,
   a picture as <img src="data:...base64,...">. What it cannot read it says out loud. */
static void esc_into(GString *o, const char *t, int n)
{
	for (int i = 0; i < n; i++) {
		if (t[i] == '&') g_string_append(o, "&amp;");
		else if (t[i] == '<') g_string_append(o, "&lt;");
		else if (t[i] == '>') g_string_append(o, "&gt;");
		else if (t[i] == '"') g_string_append(o, "&quot;");
		else g_string_append_c(o, t[i]);
	}
}
static WpObj *obj_at(GtkTextChildAnchor *a)
{
	if (!OBJ || !a) return NULL;
	for (guint i = 0; i < OBJ->len; i++) if (((WpObj *)g_ptr_array_index(OBJ, i))->at == a) return g_ptr_array_index(OBJ, i);
	return NULL;
}
static const char *block_el(GtkTextIter *i)
{
	GSList *l = gtk_text_iter_get_tags(i); char *last = NULL; const char *el = "p";
	for (GSList *k = l; k; k = k->next) { char *nm = NULL; g_object_get(G_OBJECT(k->data), "name", &nm, NULL);
		if (nm && !strncmp(nm, "st:", 3)) { g_free(last); last = g_strdup(nm + 3); }
		g_free(nm); }
	g_slist_free(l);
	if (last) { if (!strcmp(last, "h1")) el = "h1"; else if (!strcmp(last, "h2")) el = "h2"; else if (!strcmp(last, "h3")) el = "h3";
		else if (!strcmp(last, "h4")) el = "h4"; else if (!strcmp(last, "quote")) el = "blockquote"; else if (!strcmp(last, "code")) el = "pre"; }
	g_free(last); return el;
}
/* (a run of the writing: the same six inline tags, and the font, size and colour the page writes as spans) */
typedef struct { int b, i, u, s, sub, sup; char ff[80], col[16], href[400]; double sz; } Run;   /* (href: a link's address) */
static void run_at(GtkTextIter *it, Run *r)
{
	memset(r, 0, sizeof *r);
	r->b = has_tag_at(it, "b"); r->i = has_tag_at(it, "i"); r->u = has_tag_at(it, "u");
	r->s = has_tag_at(it, "s"); r->sub = has_tag_at(it, "sub"); r->sup = has_tag_at(it, "sup");
	GSList *l = gtk_text_iter_get_tags(it);
	for (GSList *k = l; k; k = k->next) { char *nm = NULL; g_object_get(G_OBJECT(k->data), "name", &nm, NULL);
		if (!nm) continue;
		if (!strncmp(nm, "ff:", 3)) snprintf(r->ff, sizeof r->ff, "%s", nm + 3);
		else if (!strncmp(nm, "fg:", 3)) snprintf(r->col, sizeof r->col, "%s", nm + 3);
		else if (!strncmp(nm, "sz:", 3)) r->sz = atof(nm + 3);
		else if (!strncmp(nm, "a:", 2)) g_strlcpy(r->href, nm + 2, sizeof r->href);
		g_free(nm); }
	g_slist_free(l);
}
static int run_same(Run *a, Run *b) { return memcmp(a, b, sizeof *a) == 0; }
static void run_open(GString *o, Run *r)
{
	if (*r->href) { g_string_append(o, "<a href=\""); esc_into(o, r->href, (int)strlen(r->href)); g_string_append(o, "\">"); }
	if (r->b) g_string_append(o, "<b>");
	if (r->i) g_string_append(o, "<i>");
	if (r->u) g_string_append(o, "<u>");
	if (r->s) g_string_append(o, "<s>");
	if (r->sub) g_string_append(o, "<sub>");
	if (r->sup) g_string_append(o, "<sup>");
	if (*r->ff || r->sz > 0) {
		g_string_append(o, "<span style=\"");
		if (*r->ff) g_string_append_printf(o, "font-family:%s", r->ff);
		if (r->sz > 0) g_string_append_printf(o, "%sfont-size:%gpt", *r->ff ? ";" : "", r->sz);
		g_string_append(o, "\">");
	}
	if (*r->col) g_string_append_printf(o, "<span style=\"color:%s\">", r->col);
}
static void run_close(GString *o, Run *r)
{
	if (*r->col) g_string_append(o, "</span>");
	if (*r->ff || r->sz > 0) g_string_append(o, "</span>");
	if (r->sup) g_string_append(o, "</sup>");
	if (r->sub) g_string_append(o, "</sub>");
	if (r->s) g_string_append(o, "</s>");
	if (r->u) g_string_append(o, "</u>");
	if (r->i) g_string_append(o, "</i>");
	if (r->b) g_string_append(o, "</b>");
	if (*r->href) g_string_append(o, "</a>");
}
static const char *cell_text(WpObj *o, int r, int c)
{
	const char *t = "";
	if (!o->w) return t;
	GList *ch = gtk_container_get_children(GTK_CONTAINER(o->w));
	for (GList *k = ch; k; k = k->next) { gpointer x = g_object_get_data(G_OBJECT(k->data), "wp-cell");
		if (x && GPOINTER_TO_INT(x) == r * 100 + c + 1) { t = gtk_entry_get_text(GTK_ENTRY(k->data)); break; } }
	g_list_free(ch);
	return t;
}
static void obj_html(GString *o, WpObj *ob)
{
	if (!ob) return;
	if (ob->kind == OBJ_PIC) {
		/* (as the page's own <img> stands in its document: the bytes, its description, its size, and a style carrying the
		   width and the place imgTools gave it -- imgTools writes width, float and display there) */
		char *b = g_base64_encode(ob->im->d, ob->im->n);
		g_string_append_printf(o, "<img src=\"data:image/png;base64,%s\" alt=\"", b);
		g_free(b);
		esc_into(o, ob->alt ? ob->alt : "", (int)strlen(ob->alt ? ob->alt : ""));
		g_string_append_printf(o, "\" width=\"%d\" height=\"%d\" style=\"max-width:100%%", ob->wpx, ob->hpx);
		if (ob->wpct > 0) g_string_append_printf(o, ";width:%d%%", ob->wpct);
		if (ob->walign == WAL_LEFT) g_string_append(o, ";float:left;margin:0 10px 6px 0");
		else if (ob->walign == WAL_RIGHT) g_string_append(o, ";float:right;margin:0 0 6px 10px");
		else if (ob->walign == WAL_CENTER) g_string_append(o, ";display:block;margin:6pt auto");
		g_string_append(o, "\">");
		return;
	}
	g_string_append(o, ob->border ? "<table class=\"wp-t\">" : "<table class=\"wp-t nb\">");
	for (int r = 0; r < ob->rows; r++) {
		g_string_append(o, "<tr>");
		for (int c = 0; c < ob->cols; c++) {
			const char *tag = ob->hdr && r == 0 ? "th" : "td", *txt = cell_text(ob, r, c);
			g_string_append_printf(o, "<%s>", tag); esc_into(o, txt, (int)strlen(txt)); g_string_append_printf(o, "</%s>", tag);
		}
		g_string_append(o, "</tr>");
	}
	g_string_append(o, "</table>");
}
/* ------------------------------------------------------------------ an entry of the writing's history
   The text with its places, and what stands at each: a picture's bytes and size (shared, not copied), a table's cells.
   Putting an entry back writes its text, turns each placeholder character into a place again, and makes the object that
   stood at it -- so undo and redo move the picture or the table itself, and a place with nothing behind it cannot be left
   (an entry whose place has lost its object drops the place). */
typedef struct { int kind, wpx, hpx, rows, cols, hdr, border, wpct, walign; Img *im; char *alt; GPtrArray *cells; } Snap;
struct Hist { char *t; GPtrArray *objs; GPtrArray *runs; };
typedef struct { int a, b; char *t; } HstRun;   /* (a stretch of the writing with the same tags: an entry keeps them, so undo brings a page's formatting back with its text) */
static void hst_run_free(gpointer p) { HstRun *r = p; g_free(r->t); g_free(r); }
static void snap_free(gpointer p)
{
	Snap *s = p; img_unref(s->im); g_free(s->alt); if (s->cells) g_ptr_array_free(s->cells, TRUE); g_free(s);
}
static Snap *snap_of(WpObj *o)
{
	Snap *s = g_new0(Snap, 1);
	if (!o) return s;   /* (kind 0: a place the list has already lost; it takes its turn in the order, and no place back) */
	s->kind = o->kind; s->wpx = o->wpx; s->hpx = o->hpx; s->rows = o->rows; s->cols = o->cols; s->hdr = o->hdr; s->border = o->border;
	s->wpct = o->wpct; s->walign = o->walign; s->alt = g_strdup(o->alt);
	s->im = img_ref(o->im);
	if (o->kind == OBJ_TABLE) {
		s->cells = g_ptr_array_new_with_free_func(g_free);
		for (int r = 0; r < o->rows; r++) for (int c = 0; c < o->cols; c++) g_ptr_array_add(s->cells, g_strdup(cell_text(o, r, c)));
	}
	return s;
}
static gint hst_cmp(gconstpointer a, gconstpointer b) { return strcmp(*(const char *const *)a, *(const char *const *)b); }
/* (the tags on one character, as one name: the page's run, in the order that makes two of them the same one) */
static char *tags_at(GtkTextIter *i)
{
	GPtrArray *v = g_ptr_array_new_with_free_func(g_free);
	GSList *l = gtk_text_iter_get_tags(i);
	for (GSList *k = l; k; k = k->next) {
		char *nm = NULL; g_object_get(G_OBJECT(k->data), "name", &nm, NULL);
		if (nm && strcmp(nm, "find")) g_ptr_array_add(v, nm); else g_free(nm);   /* (the find bar's highlight is not the writing) */
	}
	g_slist_free(l);
	g_ptr_array_sort(v, hst_cmp);
	GString *o = g_string_new(NULL);
	/* (a run's tags are held apart by the ASCII unit separator, which no address holds, so a link's own address -- one of
	   them -- comes back whole) */
	for (guint n = 0; n < v->len; n++) { if (n) g_string_append_c(o, '\x1f'); g_string_append(o, g_ptr_array_index(v, n)); }
	g_ptr_array_free(v, TRUE);
	return g_string_free(o, FALSE);
}
/* (the writing's runs of tags, in text offsets: every place where the tags change starts one) */
static void hist_runs(GPtrArray *runs)
{
	GtkTextIter t; gtk_text_buffer_get_start_iter(BUF, &t);
	char *cur = tags_at(&t); int start = 0;
	while (gtk_text_iter_forward_to_tag_toggle(&t, NULL)) {
		int off = gtk_text_iter_get_offset(&t);
		char *n = tags_at(&t);
		if (off <= start) { if (strcmp(n, cur)) { g_free(cur); cur = n; } else g_free(n); continue; }   /* (two toggles at one place) */
		if (!strcmp(n, cur)) { g_free(n); continue; }
		HstRun *r = g_new(HstRun, 1); r->a = start; r->b = off; r->t = cur; g_ptr_array_add(runs, r);
		cur = n; start = off;
	}
	int end = gtk_text_buffer_get_char_count(BUF);
	if (end > start) { HstRun *r = g_new(HstRun, 1); r->a = start; r->b = end; r->t = cur; g_ptr_array_add(runs, r); }
	else g_free(cur);
}
static void hist_runs_apply(GPtrArray *runs)
{
	for (guint k = 0; runs && k < runs->len; k++) {
		HstRun *r = g_ptr_array_index(runs, k);
		if (!*r->t || r->b <= r->a) continue;
		GtkTextIter a, b; gtk_text_buffer_get_iter_at_offset(BUF, &a, r->a); gtk_text_buffer_get_iter_at_offset(BUF, &b, r->b);
		char **names = g_strsplit(r->t, "\x1f", -1);
		for (int i = 0; names[i]; i++) if (*names[i]) gtk_text_buffer_apply_tag(BUF, tag_new(names[i]), &a, &b);
		g_strfreev(names);
	}
}
/* (what the writing is for the history: the same text, but with the place of every picture and table in it. get_text
   leaves the placeholders out, which is right for the page's words and for a save -- and would lose where an object
   stands in an entry, so an entry keeps them itself) */
static char *hist_text(void)
{
	GString *o = g_string_new(NULL);
	GtkTextIter i, e; gtk_text_buffer_get_start_iter(BUF, &i); gtk_text_buffer_get_end_iter(BUF, &e);
	for (; gtk_text_iter_compare(&i, &e) < 0; gtk_text_iter_forward_char(&i)) {
		gunichar c = gtk_text_iter_get_char(&i);
		if (c == 0xFFFC) g_string_append(o, "\xef\xbf\xbc"); else g_string_append_unichar(o, c);
	}
	return g_string_free(o, FALSE);
}
static Hist *hist_now(void)
{
	Hist *h = g_new0(Hist, 1);
	h->t = hist_text();
	h->objs = g_ptr_array_new_with_free_func(snap_free);
	GtkTextIter i, e; gtk_text_buffer_get_start_iter(BUF, &i); gtk_text_buffer_get_end_iter(BUF, &e);
	for (; gtk_text_iter_compare(&i, &e) < 0; gtk_text_iter_forward_char(&i))
		if (gtk_text_iter_get_char(&i) == 0xFFFC) g_ptr_array_add(h->objs, snap_of(obj_at(gtk_text_iter_get_child_anchor(&i))));
	h->runs = g_ptr_array_new_with_free_func(hst_run_free);
	hist_runs(h->runs);
	return h;
}
static int hist_same(Hist *a, Hist *b)
{
	if (!a || !b || strcmp(a->t, b->t) || a->objs->len != b->objs->len) return 0;
	for (guint i = 0; i < a->objs->len; i++) {
		Snap *x = g_ptr_array_index(a->objs, i), *y = g_ptr_array_index(b->objs, i);
		if (x->kind != y->kind || x->wpx != y->wpx || x->hpx != y->hpx || x->rows != y->rows || x->cols != y->cols ||
			x->hdr != y->hdr || x->border != y->border || x->wpct != y->wpct || x->walign != y->walign || g_strcmp0(x->alt, y->alt)) return 0;
		if (x->kind != OBJ_TABLE) continue;
		for (guint k = 0; k < x->cells->len; k++) if (strcmp(g_ptr_array_index(x->cells, k), g_ptr_array_index(y->cells, k))) return 0;
	}
	if (a->runs->len != b->runs->len) return 0;
	for (guint i = 0; i < a->runs->len; i++) {
		HstRun *x = g_ptr_array_index(a->runs, i), *y = g_ptr_array_index(b->runs, i);
		if (x->a != y->a || x->b != y->b || strcmp(x->t, y->t)) return 0;
	}
	return 1;
}
static void hist_free(Hist *h) { if (!h) return; g_free(h->t); g_ptr_array_free(h->objs, TRUE); if (h->runs) g_ptr_array_free(h->runs, TRUE); g_free(h); }
static void state_steps(GString *s)
{
	for (int k = 0; k < HIST_N; k++) {   /* (each step: its text's length, its places, and a table's cells -- what a check reads) */
		Hist *e = HIST_T[k];
		g_string_append_printf(s, "step %d %d %d", k + 1, (int)strlen(e->t), (int)e->objs->len);
		for (guint i = 0; i < e->objs->len; i++) { Snap *o = g_ptr_array_index(e->objs, i);
			if (!o->cells) continue;
			for (guint j = 0; j < o->cells->len; j++) g_string_append_printf(s, " %s", (char *)g_ptr_array_index(o->cells, j)); }
		g_string_append_c(s, '\n');
	}
}
/* (the page as the window opens: the empty step undo walks back to, and the first thing put on the page can be undone) */
static void hist_empty(void) { HIST_T[0] = hist_now(); HIST_N = HIST_I = 1; }
static void snap_place(GtkTextChildAnchor *a, Snap *s)
{
	WpObj *o = g_new0(WpObj, 1);
	o->kind = s->kind; o->wpx = s->wpx; o->hpx = s->hpx; o->rows = s->rows; o->cols = s->cols; o->hdr = s->hdr; o->border = s->border;
	o->wpct = s->wpct; o->walign = s->walign; o->alt = g_strdup(s->alt);
	o->im = img_ref(s->im);
	if (s->kind == OBJ_PIC) {
		GdkPixbuf *pb = pic_decode(s->im);
		if (pb) { o->w = pic_widget(pb, s->wpct, s->walign); g_object_unref(pb); }
	} else {
		GtkWidget *g = table_grid(s->border);
		for (int r = 0; r < s->rows; r++) for (int c = 0; c < s->cols; c++) {
			guint k = (guint)(r * s->cols + c);
			gtk_grid_attach(GTK_GRID(g), cell_new(r, c, s->cells && k < s->cells->len ? g_ptr_array_index(s->cells, k) : NULL), c, r, 1, 1);
		}
		o->w = g;
	}
	if (!o->w) { img_unref(o->im); g_free(o->alt); g_free(o); return; }   /* (a picture that cannot be read again is not put back empty) */
	obj_place(a, o);
}
static void hist_apply(Hist *h)
{
	if (!h) return;
	SILENT = 1;
	if (ED) gtk_widget_grab_focus(ED);   /* (the keys are the page's again before the cell that took them goes) */
	obj_clear();
	gtk_text_buffer_set_text(BUF, h->t, -1);
	int n = (int)h->objs->len, have = 0; int *off = g_new0(int, n > 0 ? (gsize)n : 1);
	GtkTextIter i, e; gtk_text_buffer_get_start_iter(BUF, &i); gtk_text_buffer_get_end_iter(BUF, &e);
	for (; gtk_text_iter_compare(&i, &e) < 0 && have < n; gtk_text_iter_forward_char(&i))
		if (gtk_text_iter_get_char(&i) == 0xFFFC) off[have++] = gtk_text_iter_get_offset(&i);
	/* (from the last place back, so that each one's offset still holds when the ones before it are made) */
	for (int k = n - 1; k >= 0; k--) {
		if (k >= have) continue;
		GtkTextIter a, b; gtk_text_buffer_get_iter_at_offset(BUF, &a, off[k]); b = a; gtk_text_iter_forward_char(&b);
		gtk_text_buffer_delete(BUF, &a, &b);
		Snap *s = g_ptr_array_index(h->objs, k);
		if (s->kind == 0) continue;
		GtkTextIter at; gtk_text_buffer_get_iter_at_offset(BUF, &at, off[k]);
		snap_place(gtk_text_buffer_create_child_anchor(BUF, &at), s);
	}
	g_free(off);
	hist_runs_apply(h->runs);   /* (the tags the entry had, put back where they were) */
	SILENT = 0;
	DIRTY = 1; set_title();   /* (the page is not what its file holds any more, as on the page) */
	counts(); upd_sel(); state_soon();
}
static char *doc_html(void)
{
	GString *o = g_string_new(NULL);
	g_string_append(o, "<!--wp:%7B%7D--><style data-wp>");
	g_string_append(o, WP_CSS_DOC);
	g_string_append(o, "</style>");
	GtkTextIter line; gtk_text_buffer_get_start_iter(BUF, &line);
	for (;;) {
		GtkTextIter end = line; if (!gtk_text_iter_ends_line(&end)) gtk_text_iter_forward_to_line_end(&end);
		const char *el = block_el(&line);
		Run cur; memset(&cur, 0, sizeof cur); int wrote = 0;
		g_string_append_printf(o, "<%s>", el);
		for (GtkTextIter i = line; gtk_text_iter_compare(&i, &end) < 0; gtk_text_iter_forward_char(&i)) {
			if (gtk_text_iter_get_char(&i) == 0xFFFC) {
				Run none; memset(&none, 0, sizeof none); run_close(o, &cur); cur = none;
				obj_html(o, obj_at(gtk_text_iter_get_child_anchor(&i)));
				wrote = 1; continue;
			}
			Run r; run_at(&i, &r);
			if (!run_same(&r, &cur)) { run_close(o, &cur); run_open(o, &r); cur = r; }
			char c[8]; int n = g_unichar_to_utf8(gtk_text_iter_get_char(&i), c); esc_into(o, c, n); wrote = 1;
		}
		run_close(o, &cur);
		if (!wrote) g_string_append(o, "<br>");
		g_string_append_printf(o, "</%s>\n", el);
		if (gtk_text_iter_is_end(&line) || !gtk_text_iter_forward_line(&line)) break;
		if (gtk_text_iter_is_end(&line)) break;
	}
	return g_string_free(o, FALSE);
}
static int save_now(void);
static int save_doc(const char *path)
{
	char *h = doc_html(); int ok = 0;
	if (!g_file_set_contents(path, h, -1, NULL)) say(rui_tr("That document could not be saved."));
	else { g_free(DOCPATH); DOCPATH = g_strdup(path); DIRTY = 0; set_title(); ok = 1; }   /* (the writing is in its file: no * any more) */
	g_free(h);
	state_soon();
	return ok;
}
static int choose_save(void)
{
	GtkWidget *f = gtk_file_chooser_dialog_new(rui_tr("Save as"), GTK_WINDOW(W->win), GTK_FILE_CHOOSER_ACTION_SAVE,
		rui_tr("Cancel"), GTK_RESPONSE_CANCEL, rui_tr("Save"), GTK_RESPONSE_ACCEPT, NULL);
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(f), "LETTER.DOC");
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(f), TRUE);
	int ok = 0;
	if (gtk_dialog_run(GTK_DIALOG(f)) == GTK_RESPONSE_ACCEPT) {
		char *p = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(f));
		if (p) ok = save_doc(p);
		g_free(p);
	}
	gtk_widget_destroy(f);
	return ok;
}
static int save_now(void) { return DOCPATH ? save_doc(DOCPATH) : choose_save(); }
static void choose_open(void)
{
	GtkWidget *f = gtk_file_chooser_dialog_new(rui_tr("Open document"), GTK_WINDOW(W->win), GTK_FILE_CHOOSER_ACTION_OPEN,
		rui_tr("Cancel"), GTK_RESPONSE_CANCEL, rui_tr("Open"), GTK_RESPONSE_ACCEPT, NULL);
	if (gtk_dialog_run(GTK_DIALOG(f)) == GTK_RESPONSE_ACCEPT) {
		char *p = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(f));
		if (p) confirm_discard(ASK_OPEN, p);   /* (the page asks before a page with unsaved changes is replaced) */
		g_free(p);
	}
	gtk_widget_destroy(f);
}
/* ---- opening: the same subset back into the model */
/* (an opened document's pictures and tables go where its own text has reached: at the end, not at the cursor left from
   someone else's page) */
static void cursor_end(void) { GtkTextIter e; gtk_text_buffer_get_end_iter(BUF, &e); gtk_text_buffer_place_cursor(BUF, &e); }
static void put_at_end(const char *t, int n, const char **stk, int sp)
{
	/* (the range is kept as an offset: an iterator sitting where the text goes moves past it, and a range of nothing
	   tags nothing -- which is how an opened document lost its bold until the round trip showed it) */
	GtkTextIter s, e; gtk_text_buffer_get_end_iter(BUF, &s);
	int off = gtk_text_iter_get_offset(&s);
	gtk_text_buffer_insert(BUF, &s, t, n);
	if (!sp) return;
	gtk_text_buffer_get_iter_at_offset(BUF, &s, off);
	gtk_text_buffer_get_end_iter(BUF, &e);
	for (int k = 0; k < sp; k++) gtk_text_buffer_apply_tag(BUF, tag_new(stk[k]), &s, &e);
}
static void put_unescaped(const char *t, int n, const char **stk, int sp, GString *cell)
{
	GString *o = g_string_new(NULL);
	for (int i = 0; i < n; i++) {
		if (t[i] == '&') {
			if (!strncmp(t + i, "&amp;", 5)) { g_string_append_c(o, '&'); i += 4; continue; }
			if (!strncmp(t + i, "&lt;", 4)) { g_string_append_c(o, '<'); i += 3; continue; }
			if (!strncmp(t + i, "&gt;", 4)) { g_string_append_c(o, '>'); i += 3; continue; }
			if (!strncmp(t + i, "&quot;", 6)) { g_string_append_c(o, '"'); i += 5; continue; }
			if (!strncmp(t + i, "&nbsp;", 6)) { g_string_append_c(o, ' '); i += 5; continue; }
		}
		g_string_append_c(o, t[i]);
	}
	if (cell) g_string_append_len(cell, o->str, (gssize)o->len);
	else put_at_end(o->str, (int)o->len, stk, sp);
	g_string_free(o, TRUE);
}
static void insert_picture_data(guchar *data, gsize len, const char *alt, int wpct, int walign)
{
	GdkPixbufLoader *l = gdk_pixbuf_loader_new(); GdkPixbuf *pb = NULL;
	if (gdk_pixbuf_loader_write(l, data, len, NULL) && gdk_pixbuf_loader_close(l, NULL)) pb = gdk_pixbuf_loader_get_pixbuf(l);
	if (!pb) { g_object_unref(l); g_free(data); say(rui_tr("That picture could not be read.")); return; }
	g_object_ref(pb);
	ObjSpec s = { .kind = OBJ_PIC, .wpx = gdk_pixbuf_get_width(pb), .hpx = gdk_pixbuf_get_height(pb), .alt = alt,
		.wpct = wpct, .walign = walign };
	obj_add(pic_widget(pb, wpct, walign), s, data, len);
	g_object_unref(pb); g_object_unref(l);
}
static void insert_table_text(GPtrArray *rows, int hdr, int border)
{
	int nr = (int)rows->len, nc = 0;
	for (int r = 0; r < nr; r++) { GPtrArray *row = g_ptr_array_index(rows, r); if ((int)row->len > nc) nc = (int)row->len; }
	if (nr <= 0 || nc <= 0) return;
	GtkWidget *g = table_grid(border);
	for (int r = 0; r < nr; r++) { GPtrArray *row = g_ptr_array_index(rows, r);
		for (int c = 0; c < nc; c++) {
			gtk_grid_attach(GTK_GRID(g), cell_new(r, c, c < (int)row->len ? g_ptr_array_index(row, c) : NULL), c, r, 1, 1);
		} }
	ObjSpec spec = { .kind = OBJ_TABLE, .rows = nr, .cols = nc, .hdr = hdr, .border = border };
	obj_add(g, spec, NULL, 0);
}
/* (an attribute's own words, unescaped -- what the page wrote in alt="..." or style="..."; only a whole attribute, never
   the middle of the picture's base64, which is why a name after a space or the tag's own < is required) */
static char *unesc_dup(const char *t, int n)
{
	GString *o = g_string_new(NULL);
	for (int i = 0; i < n; i++) {
		if (t[i] == '&') {
			if (n - i >= 5 && !strncmp(t + i, "&amp;", 5)) { g_string_append_c(o, '&'); i += 4; continue; }
			if (n - i >= 4 && !strncmp(t + i, "&lt;", 4)) { g_string_append_c(o, '<'); i += 3; continue; }
			if (n - i >= 4 && !strncmp(t + i, "&gt;", 4)) { g_string_append_c(o, '>'); i += 3; continue; }
			if (n - i >= 6 && !strncmp(t + i, "&quot;", 6)) { g_string_append_c(o, '"'); i += 5; continue; }
		}
		g_string_append_c(o, t[i]);
	}
	return g_string_free(o, FALSE);
}
static char *attr_dup(const char *p, const char *end, const char *name)
{
	size_t nl = strlen(name);
	for (const char *a = p; (a = strstr(a, name)) && a < end; a += nl) {
		if (a > p && a[-1] != ' ' && a[-1] != '\t' && a[-1] != '\n' && a[-1] != '<') continue;
		const char *q = a + nl; while (q < end && (*q == ' ' || *q == '=')) q++;
		if (q >= end || *q != '"') continue;
		q++; const char *e = memchr(q, '"', (size_t)(end - q));
		if (!e) return NULL;
		return unesc_dup(q, (int)(e - q));
	}
	return NULL;
}
static int par_close(const char *t)
{
	return !strncmp(t, "</p", 3) || !strncmp(t, "</h1", 4) || !strncmp(t, "</h2", 4) || !strncmp(t, "</h3", 4) ||
		!strncmp(t, "</h4", 4) || !strncmp(t, "</blockquote", 12) || !strncmp(t, "</pre", 5) || !strncmp(t, "</div", 5);
}
static void open_doc(const char *path)
{
	char *h = NULL; gsize n = 0;
	if (!g_file_get_contents(path, &h, &n, NULL)) { say(rui_tr("That document could not be opened.")); return; }
	SILENT = 1; obj_clear(); gtk_text_buffer_set_text(BUF, "", -1); SILENT = 0;
	char *p = h;
	if (!strncmp(p, "<!--wp:", 7)) { char *e = strstr(p, "-->"); if (e) p = e + 3; }
	{ char *s = strstr(p, "<style data-wp"); if (s) { char *e = strstr(s, "</style>"); if (e) p = e + 8; } }
	const char *stk[32]; int sp = 0;
	GPtrArray *rows = NULL, *cells = NULL; GString *cell = NULL; int in_table = 0, thdr = 0, tnb = 0;
	char pend[24] = "p";
	GtkTextIter at; gtk_text_buffer_get_start_iter(BUF, &at);
	GtkTextMark *pstart = gtk_text_buffer_create_mark(BUF, "wp-p", &at, TRUE);
	while (*p) {
		if (*p != '<') {
			char *e = strchr(p, '<'); size_t len = e ? (size_t)(e - p) : strlen(p);
			put_unescaped(p, (int)len, stk, sp, cell);
			p += len; continue;
		}
		/* (a picture's tag is as long as its picture, so it is never copied: it is read where it stands) */
		if (!strncmp(p, "<img", 4)) {
			char *gt = strstr(p, ">");
			if (!gt) break;
			char *s = strstr(p, "src=\"data:"), *cm = s && s < gt ? strchr(s, ',') : NULL, *q = cm ? strchr(cm + 1, '"') : NULL;
			/* (its description and how the page places it: the alt the dialog gave it, and the width, float and display of
			   the style imgTools wrote) */
			char *alt = attr_dup(p, gt, "alt"), *sty = attr_dup(p, gt, "style");
			int wpct = 0, walign = WAL_INLINE;
			if (sty) {
				/* (not max-width, which stands before it: the width tool's own is the one after a ; or at the start) */
				char *w = strstr(sty, "width:");
				while (w && w > sty && !strncmp(w - 4, "max-", 4)) w = strstr(w + 6, "width:");
				if (w) wpct = atoi(w + 6);
				if (strstr(sty, "float:left")) walign = WAL_LEFT;
				else if (strstr(sty, "float:right")) walign = WAL_RIGHT;
				else if (strstr(sty, "display:block")) walign = WAL_CENTER;
			}
			if (cm && q && q < gt) {
				char *b64 = g_strndup(cm + 1, (gsize)(q - cm - 1));
				gsize dl = 0; guchar *d = g_base64_decode(b64, &dl);
				g_free(b64);
				if (d) { cursor_end(); insert_picture_data(d, dl, alt, wpct, walign); }
			}
			g_free(alt); g_free(sty);
			p = gt + 1; continue;
		}
		char *e = strchr(p, '>'); if (!e) break;
		size_t tl = (size_t)(e - p + 1); char tag[512];
		if (tl >= sizeof tag) { p = e + 1; continue; }
		memcpy(tag, p, tl); tag[tl] = 0; p = e + 1;
		if (!strncmp(tag, "<table", 6)) {
			in_table = 1; rows = g_ptr_array_new(); cells = NULL; cell = NULL; thdr = 0; tnb = 0;
			const char *cl = strstr(tag, "class=\"");   /* (the page's .wp-t nb: a table with no borders) */
			if (cl) { cl += 7; const char *ce = strchr(cl, '"');
				if (ce && ce - cl < 60) { char cs[60]; memcpy(cs, cl, (gsize)(ce - cl)); cs[ce - cl] = 0; if (strstr(cs, "nb")) tnb = 1; } }
			continue;
		}
		if (!strncmp(tag, "</table", 7)) {
			if (rows && rows->len) { cursor_end(); insert_table_text(rows, thdr, !tnb); }
			if (rows) {
				for (guint r = 0; r < rows->len; r++) { GPtrArray *row = g_ptr_array_index(rows, r);
					for (guint c = 0; c < row->len; c++) g_free(g_ptr_array_index(row, c)); g_ptr_array_free(row, TRUE); }
				g_ptr_array_free(rows, TRUE);
			}
			rows = NULL; cells = NULL; in_table = 0; continue;
		}
		if (!strncmp(tag, "<tr", 3)) { cells = g_ptr_array_new(); continue; }
		if (!strncmp(tag, "</tr", 4)) {
			if (cell) { if (cells) g_ptr_array_add(cells, g_strdup(cell->str)); g_string_free(cell, TRUE); cell = NULL; }
			if (cells && rows) g_ptr_array_add(rows, cells);
			cells = NULL; continue;
		}
		if (!strncmp(tag, "<td", 3) || !strncmp(tag, "<th", 3)) {
			if (!strncmp(tag, "<th", 3) && rows && !rows->len) thdr = 1;   /* (its first row's <th>: the table's heading row) */
			if (!cells) cells = g_ptr_array_new(); if (cell) g_string_free(cell, TRUE); cell = g_string_new(NULL); continue; }
		if (!strncmp(tag, "</td", 4) || !strncmp(tag, "</th", 4)) {
			if (cells) g_ptr_array_add(cells, g_strdup(cell ? cell->str : ""));
			if (cell) { g_string_free(cell, TRUE); cell = NULL; }
			continue;
		}
		if (!strncmp(tag, "<br", 3)) { if (!in_table) put_at_end("\n", 1, NULL, 0); continue; }
		const char *el = NULL;
		if (!strncmp(tag, "<p", 2) && tag[1] != '/') el = "p";
		else if (!strncmp(tag, "<h1", 3)) el = "h1"; else if (!strncmp(tag, "<h2", 3)) el = "h2";
		else if (!strncmp(tag, "<h3", 3)) el = "h3"; else if (!strncmp(tag, "<h4", 3)) el = "h4";
		else if (!strncmp(tag, "<blockquote", 11)) el = "quote"; else if (!strncmp(tag, "<pre", 4)) el = "code";
		else if (!strncmp(tag, "<div", 4)) el = "p";
		if (el) {
			if (gtk_text_buffer_get_char_count(BUF)) put_at_end("\n", 1, NULL, 0);
			{ GtkTextIter e2; gtk_text_buffer_get_end_iter(BUF, &e2); gtk_text_buffer_move_mark(BUF, pstart, &e2); }
			snprintf(pend, sizeof pend, "%s", el);
			continue;
		}
		if (par_close(tag)) {
			if (strcmp(pend, "p")) {
				char nm[24]; snprintf(nm, sizeof nm, "st:%s", pend);
				GtkTextIter a2, b2; gtk_text_buffer_get_iter_at_mark(BUF, &a2, pstart); gtk_text_buffer_get_end_iter(BUF, &b2);
				if (gtk_text_iter_compare(&a2, &b2) < 0) gtk_text_buffer_apply_tag(BUF, tag_new(nm), &a2, &b2);
			}
			snprintf(pend, sizeof pend, "p");
			continue;
		}
		if (!strncmp(tag, "<a ", 3)) {   /* (a link: its address is the run's own tag, as the model keeps it) */
			char *h = strstr(tag, "href=\"");
			if (h && sp < 31) {
				h += 6; char v[512]; int k = 0; while (*h && *h != '"' && k < 511) v[k++] = *h++; v[k] = 0;
				char *u = unesc_dup(v, k);
				stk[sp++] = g_strdup_printf("a:%s", u);
				g_free(u);
			}
			continue;
		}
		if (!strncmp(tag, "</a", 3)) { if (sp > 0 && !strncmp(stk[sp - 1], "a:", 2)) g_free((gpointer)stk[--sp]); continue; }
		if (!strncmp(tag, "<span", 5)) {
			char *st = strstr(tag, "style=\"");
			if (st) {
				st += 7;
				char *c = strstr(st, "color:");
				if (c && sp < 31) { char v[16]; int k = 0; c += 6; while (*c && *c != ';' && *c != '"' && k < 15) v[k++] = *c++; v[k] = 0; stk[sp++] = g_strdup_printf("fg:%s", v); }
				char *f = strstr(st, "font-family:");
				if (f && sp < 31) { char v[80]; int k = 0; f += 12; while (*f && *f != ';' && *f != '"' && k < 79) v[k++] = *f++; v[k] = 0; stk[sp++] = g_strdup_printf("ff:%s", v); }
				char *z = strstr(st, "font-size:");
				if (z && sp < 31) { char v[24]; int k = 0; z += 10; while (*z && *z != 'p' && *z != ';' && *z != '"' && k < 23) v[k++] = *z++; v[k] = 0; stk[sp++] = g_strdup_printf("sz:%s", v); }
			}
			continue;
		}
		if (!strncmp(tag, "</span", 6)) { if (sp > 0) g_free((gpointer)stk[--sp]); continue; }
		{ struct { const char *open, *name; } IN[] = { { "<b", "b" }, { "<i", "i" }, { "<u", "u" }, { "<s", "s" }, { "<sub", "sub" }, { "<sup", "sup" } };
		  for (unsigned k = 0; k < G_N_ELEMENTS(IN); k++) {
			size_t ol = strlen(IN[k].open);
			if (!strncmp(tag, IN[k].open, ol) && tag[ol] == '>') { if (sp < 31) stk[sp++] = IN[k].name; break; }
			char cl[8]; snprintf(cl, sizeof cl, "</%s", IN[k].name);
			size_t ll = strlen(cl);
			if (!strncmp(tag, cl, ll) && tag[ll] == '>') { if (sp > 0 && !strcmp(stk[sp - 1], IN[k].name)) sp--; break; }
		  } }
	}
	for (int k = 0; k < sp; k++) {
		const char *nm = stk[k];
		if (!strcmp(nm, "b") || !strcmp(nm, "i") || !strcmp(nm, "u") || !strcmp(nm, "s") || !strcmp(nm, "sub") || !strcmp(nm, "sup")) continue;
		g_free((gpointer)nm);
	}
	gtk_text_buffer_delete_mark(BUF, pstart);
	g_free(h);
	counts(); upd_sel(); hist_push();
	g_free(DOCPATH); DOCPATH = g_strdup(path); DIRTY = 0; set_title();   /* (what was opened is its file's, with nothing unsaved) */
	state_soon();
}
static gboolean io_hook(gpointer d)
{
	(void)d;
	const char *op = getenv("RUDOLF_WP_OPEN"), *sv = getenv("RUDOLF_WP_SAVE");
	if (op && *op) confirm_discard(ASK_OPEN, op);
	if (sv && *sv) save_doc(sv);
	return G_SOURCE_REMOVE;
}
static void on_find_text(GtkEditable *e, gpointer d) { (void)e; (void)d; state_soon(); }
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
	/* (the bar's two boxes are part of what the page shows: typing in either moves the state on, as the writing does) */
	g_signal_connect(FI, "changed", G_CALLBACK(on_find_text), NULL);
	g_signal_connect(REPL, "changed", G_CALLBACK(on_find_text), NULL);
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
	hist_empty();   /* (the history undo and redo walk, from the empty page) */
	gtk_text_buffer_create_tag(BUF, "b", "weight", PANGO_WEIGHT_BOLD, NULL);
	gtk_text_buffer_create_tag(BUF, "i", "style", PANGO_STYLE_ITALIC, NULL);
	gtk_text_buffer_create_tag(BUF, "u", "underline", PANGO_UNDERLINE_SINGLE, NULL);
	gtk_text_buffer_create_tag(BUF, "s", "strikethrough", TRUE, NULL);
	gtk_text_buffer_create_tag(BUF, "sub", "rise", -4000, "size-points", 9.0, NULL);
	gtk_text_buffer_create_tag(BUF, "sup", "rise", 6000, "size-points", 9.0, NULL);
	gtk_text_buffer_create_tag(BUF, "find", "background", "#fde68a", NULL);
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
	case GDK_KEY_h: gtk_widget_show_all(FIND); gtk_widget_grab_focus(REPL); state_soon(); return TRUE;
	case GDK_KEY_k: link_dlg(); return TRUE;   /* (the page's Ctrl+K) */
	case GDK_KEY_n: new_page(); return TRUE;
	case GDK_KEY_s: save_now(); return TRUE;
	case GDK_KEY_o: choose_open(); return TRUE;
	case GDK_KEY_v: if (e->state & GDK_SHIFT_MASK) { paste_plain(); return TRUE; } return FALSE;
	default: return FALSE;
	}
}
/* (the window's own close, as the page's W.onclose: the question stands before the writing goes with it) */
static gboolean on_delete(GtkWidget *w, GdkEvent *e, gpointer d)
{
	(void)w; (void)e; (void)d;
	if (!DIRTY) return FALSE;   /* (nothing written since the file: the window closes) */
	confirm_discard(ASK_CLOSE, NULL);
	return TRUE;
}
static gboolean close_hook(gpointer d)   /* (the tests: the window's close, as File > Close does it) */
{
	(void)d; if (!ASK && DIRTY) confirm_discard(ASK_CLOSE, NULL);
	return G_SOURCE_CONTINUE;
}
/* (the tests: the question answered as a press on one of its buttons answers it. Nothing outside the app knows where a
   dialog's buttons stand on a display with no window manager, so the test names the one it would press, in a file the
   app reads while the question is up -- written when the test wants the answer, and empty while it does not) */
static GtkWidget *find_btn(GtkWidget *w, const char *label)
{
	if (GTK_IS_BUTTON(w)) { const char *l = gtk_button_get_label(GTK_BUTTON(w)); return l && !strcmp(l, label) ? w : NULL; }
	if (!GTK_IS_CONTAINER(w)) return NULL;
	GList *ch = gtk_container_get_children(GTK_CONTAINER(w)); GtkWidget *hit = NULL;
	for (GList *k = ch; k && !hit; k = k->next) hit = find_btn(k->data, label);
	g_list_free(ch);
	return hit;
}
static gboolean answer_hook(gpointer d)
{
	const char *f = d;
	if (!rui_dialog_box()) return G_SOURCE_CONTINUE;   /* (a form's buttons are pressed as a question's are) */
	static char *LAST;   /* (what it has already done: a dialog of small buttons stands after a press on one of them, and a press
							 pressed again on every turn of this hook would put the character in as many times) */
	char *lab = NULL; if (!g_file_get_contents(f, &lab, NULL, NULL)) return G_SOURCE_CONTINUE;
	{ char *raw = g_strdup(lab);   /* (as it was read: the scan below ends the lines where they were) */
	  if (LAST && !strcmp(LAST, raw)) { g_free(raw); g_free(lab); return G_SOURCE_CONTINUE; }
	  g_free(LAST); LAST = raw; }
	/* (its lines: "N=value" fills the dialog's field N as its own kind fills it -- a text, a path, a choice's own words, a
	   check's 1 or 0 -- and the last line that is not one names the button the test would press) */
	GtkWidget *box = rui_dialog_box(); char *btn = NULL;
	for (char *line = lab; line && *line; ) {
		char *nl = strchr(line, '\n'); if (nl) *nl = 0;
		g_strstrip(line);
		char *eq = strchr(line, '=');
		if (!strncmp(line, "c=", 2)) rui_pick_click(box, line + 2);   /* (a dialog of small buttons: the one this character says) */
		else if (*line && eq && strspn(line, "0123456789") == (size_t)(eq - line)) {
			*eq = 0; GtkWidget *w = rui_form_field(box, atoi(line)); if (w) rui_form_value(w, eq + 1);
		} else if (*line) { g_free(btn); btn = g_strdup(line); }
		line = nl ? nl + 1 : NULL;
	}
	if (btn) { GtkWidget *b = find_btn(box, rui_tr(btn)); if (b) gtk_button_clicked(GTK_BUTTON(b)); }
	g_free(btn); g_free(lab);   /* (LAST keeps its own copy of what it read) */
	return G_SOURCE_CONTINUE;
}
/* (the check's hand on the toolbar's own commands: RUDOLF_WP_DO=bul,bul applies each to the whole writing, once there is
   some -- a click on a button is the person's, and the check cannot aim one) */
static int DO_TICKS;
static gboolean do_hook(gpointer d)
{
	static int LASTN;   /* (what the writing held on the last turn: the commands act on the whole of it, so a turn that
						   sees the letters still coming waits for the next one -- a check types just after the app opens) */
	int n = (int)gtk_text_buffer_get_char_count(BUF);
	if ((!n && ++DO_TICKS < 50) || n != LASTN) { LASTN = n; return G_SOURCE_CONTINUE; }
	char *copy = g_strdup((const char *)d), *sv = NULL;
	for (char *tok = strtok_r(copy, ",", &sv); tok; tok = strtok_r(NULL, ",", &sv)) {
		GtkTextIter a, b;
		gtk_text_buffer_get_bounds(BUF, &a, &b);
		gtk_text_buffer_select_range(BUF, &a, &b);   /* (the page's commands act on the choice, so the whole writing is one) */
		if (!strncmp(tok, "menu:", 5)) { int m = atoi(tok + 5); if (m >= 0 && m < NMENUS) g_idle_add(menu_hook, (gpointer)(intptr_t)m); }
		else if (!strncmp(tok, "mi:", 3)) { int m = atoi(tok + 3), k = (strchr(tok + 3, ':')) ? atoi(strchr(tok + 3, ':') + 1) : -1;
			if (m >= 0 && m < NMENUS && k >= 0 && k < WP_MENUS[m].n) menu_do(WP_MENUS[m].items[k].id); }
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
static GtkWidget *MBAR, *MBTNS[WPMENUS_N];
static void menu_do(int id)
{
	const WpOp *o = op_of(id); if (!o) return;   /* (an item the app has no command for: greyed, and it refuses) */
	switch (o->op) {
	case M_CLOSE: gtk_widget_destroy(W->win); break;
	case M_UNDO: hist_undo(); break;
	case M_REDO: hist_redo(); break;
	case M_SELALL: { GtkTextIter a, b; gtk_text_buffer_get_bounds(BUF, &a, &b); gtk_text_buffer_select_range(BUF, &a, &b); state_soon(); break; }
	case M_FIND: on_find_open(NULL, NULL); break;
	case M_FINDREP: on_find_open(NULL, NULL); if (REPL) gtk_widget_grab_focus(REPL); break;
	case M_ZOOM: zoom_to(atoi(o->arg)); break;
	case M_TOGGLE: toggle(NULL, (gpointer)o->arg); break;
	case M_LIST: on_list(NULL, (gpointer)o->arg); break;
	case M_CLEAR: on_clear(NULL, NULL); break;
	case M_NEW: new_page(); break;
	case M_OPEN: confirm_discard(ASK_OPEN, NULL); break;
	case M_SAVE: save_now(); break;
	case M_SAVEAS: choose_save(); break;
	case M_STATS: show_stats(); break;
	case M_CUT: gtk_text_buffer_cut_clipboard(BUF, gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), TRUE);
		counts(); state_soon(); break;
	case M_COPY: gtk_text_buffer_copy_clipboard(BUF, gtk_clipboard_get(GDK_SELECTION_CLIPBOARD)); break;
	case M_PASTE:
		if (o->arg && !strcmp(o->arg, "plain")) paste_plain();
		else gtk_text_buffer_paste_clipboard(BUF, gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), NULL, TRUE);
		counts(); state_soon(); break;
	case M_QUOTE: quote_style(); break;
	case M_CASE: case_do(!strcmp(o->arg, "up") ? 0 : !strcmp(o->arg, "low") ? 1 : !strcmp(o->arg, "title") ? 2 : 3); break;
	case M_WHEN: insert_when(o->arg); break;
	case M_TABLE: table_dlg(); break;
	case M_PICTURE: pic_dlg(); break;
	case M_LINK: link_dlg(); break;
	case M_CHARS: chars_dlg(); break;
	case M_KEYS:
		rui_about(W, rui_tr("Keyboard shortcuts"), rui_tr("Ctrl+B / I / U  Bold, italic, underline.\n"
			"Ctrl+Z / Y  Undo, redo.\nCtrl+F / H  Find, find and replace.\nCtrl+K  Insert a link."
			"\nCtrl+N / O / S  New, open, save.\nCtrl+Click a link  Open it in Oracle.\nTab (in a table)  The next cell."));
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
	if (i >= 0 && i < WP_MENUS[m].n) menu_do(WP_MENUS[m].items[i].id);
	state_soon();
}
static void open_menu(GtkWidget *b, int m)
{
	RuiMenuItem *it = g_new0(RuiMenuItem, WP_MENUS[m].n);
	for (int k = 0; k < WP_MENUS[m].n; k++) {
		const WpItem *s = &WP_MENUS[m].items[k];
		it[k].label = s->label; it[k].key = s->key; it[k].sep = s->label == NULL;
		it[k].off = s->label && !op_of(s->id);
	}
	GtkAllocation a; gtk_widget_get_allocation(b, &a); int ox = 0, oy = 0;
	if (gtk_widget_get_window(b)) gdk_window_get_origin(gtk_widget_get_window(b), &ox, &oy);
	MENU_OPEN_I = m;
	rui_menu(b, ox, oy + a.height, NULL, it, WP_MENUS[m].n, menu_pick, (void *)(intptr_t)m);
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
		GtkWidget *b = gtk_button_new_with_label(rui_tr(WP_MENUS[i].title));
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
	g_signal_connect(W->win, "delete-event", G_CALLBACK(on_delete), NULL);
	gtk_widget_show_all(W->win); gtk_widget_hide(FIND);
	{ const char *mo = getenv("RUDOLF_WP_MENU");   /* (the tests: the menu a click on the bar opens) */
	  if (mo && *mo) for (int i = 0; i < NMENUS; i++) if (!strcmp(WP_MENUS[i].title, mo)) { g_timeout_add(400, menu_hook, (gpointer)(intptr_t)i); break; } }
	if (getenv("RUDOLF_WP_OPEN") || getenv("RUDOLF_WP_SAVE")) g_timeout_add(400, io_hook, NULL);
	{ const char *ins = getenv("RUDOLF_WP_INSERT");   /* (the tests: a picture or a table put in as the menu puts it) */
	  if (ins && *ins) g_timeout_add(400, insert_hook, (gpointer)ins); }
	if (getenv("RUDOLF_WP_FOCUS")) g_timeout_add(2500, focus_cell_hook, NULL);   /* (the tests: the cursor in a table's first cell,
															   after the window has the display's focus) */
	gtk_widget_grab_focus(ED);   /* (the page's window opens with its writing ready: typing goes into it at once) */
	set_title();   /* (Untitled - WordPro, as the page opens its window) */
	if (getenv("RUDOLF_WP_CLOSE")) g_timeout_add(2000, close_hook, NULL);   /* (the tests: the window's close, while anything is unsaved) */
	if (getenv("RUDOLF_WP_ANSWER")) g_timeout_add(150, answer_hook, (gpointer)getenv("RUDOLF_WP_ANSWER"));   /* (the tests: the question's own buttons) */
	{ const char *dl = getenv("RUDOLF_WP_DIALOG");   /* (the tests: a dialog of the page's opened as its menu opens it) */
	  if (dl && *dl) g_timeout_add(400, dialog_hook, (gpointer)dl); }
	if (getenv("RUDOLF_WP_PICTOOLS")) g_timeout_add(1200, pic_tools_hook, NULL);   /* (the tests: the picture tools, as a
																			  click on the picture opens them) */
	counts(); state_soon();
	{ const char *w = getenv("RUDOLF_WP_DO"); if (w && *w) g_timeout_add(200, do_hook, (gpointer)w); }
	gtk_main(); return 0;
}
