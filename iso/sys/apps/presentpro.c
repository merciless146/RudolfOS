/* ===== 1.6.3 (presentpro): PresentPro as a native program (/usr/bin/rudolf-presentpro) -- docs/plan-1.6.3.md Part 1,
   step 1: the page's window, its toolbar, its eight menus, its list of slides, its stage and its notes, with the page's
   eight layouts, the text typed into a placeholder, bold, italic, underline, strikethrough, the three alignments, bullets
   and numbering, undo and redo, and the page's keys. The page is desktop/ppro45.js (the window and its commands),
   desktop/pprof45.js (the deck: a slide's layout, its placeholders and the themes) and desktop/styles25.css's .pp-*, as
   the native WordPro's and SpreadsheetPro's look is the page's.
   What this step leaves out is drawn as the page draws an item it cannot choose: the files (.RDECK, .PPTX, the PDFs), the
   text boxes, shapes, pictures, tables, charts and icons, the transitions and animations, the presenter view, the slide
   sorter, the themes, the backgrounds and the master (numbers, dates, footer), grouping and the arrangement, the
   clipboard, the assistant and printing. They are later steps of the app, and each is named in the check as something the
   page has and the native app does not yet.
   GPL-2.0-or-later. */
#define _GNU_SOURCE
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rudolf-ui.h"
#include "presentpro-gen.h"

#define SLW 960.0       /* (pprof45.js's ppSize: a slide is 960 by 540, 16:9) */
#define SLH 540.0
#define HSTP 60         /* (the page's undo: 60 steps) */
#define LISTW 184.0     /* (the page's .pp-list: its padding, the numbers' 16px and a 150px slide) */
#define THW 150.0
#define THH 84.0        /* (150 * 540 / 960, as a slide's thumbnail is) */
#define THGAP 10.0
#define PAD 8.0
#define NOTES_H 92.0    /* (the page's .pp-nbar) */

typedef PpTheme Th;

/* (a layout's text places, as pprof45.js's ppLayout makes them: its role, its box in the slide's own 960x540, its size in
   the page's CSS pixels, its alignment, its valign, its weight, whether it is a list, and whether it takes the theme's
   title font and title ink instead of the body's) */
enum { PH_TITLE, PH_SUB, PH_BODY, PH_BODY2, PH_H1, PH_H2, PH_N };
static const char *PH_KEY[PH_N] = { "title", "sub", "body", "body2", "h1", "h2" };
enum { HA_LEFT, HA_CENTRE, HA_RIGHT };
enum { VA_TOP, VA_MIDDLE, VA_BOTTOM };
typedef struct { int ph; double x, y, w, h; int fs, ha, va, bold, bullets, tf; } PhBox;

/* (the m of pprof45.js's ppLayout: a tenth of the slide's side, its margin) */
static const PhBox L_TITLE[] = {
	{ PH_TITLE, 57.6, 151.2, 844.8, 129.6, 60, HA_CENTRE, VA_BOTTOM, 0, 0, 1 },
	{ PH_SUB, 115.2, 297.0, 729.6, 86.4, 24, HA_CENTRE, VA_TOP, 0, 0, 0 },
};
static const PhBox L_CONTENT[] = {
	{ PH_TITLE, 57.6, 32.4, 844.8, 86.4, 37, HA_LEFT, VA_MIDDLE, 0, 0, 1 },
	{ PH_BODY, 57.6, 140.4, 844.8, 356.4, 27, HA_LEFT, VA_TOP, 0, 1, 0 },
};
static const PhBox L_TWO[] = {
	{ PH_TITLE, 57.6, 32.4, 844.8, 86.4, 37, HA_LEFT, VA_MIDDLE, 0, 0, 1 },
	{ PH_BODY, 57.6, 140.4, 393.6, 345.6, 24, HA_LEFT, VA_TOP, 0, 1, 0 },
	{ PH_BODY2, 508.8, 140.4, 393.6, 345.6, 24, HA_LEFT, VA_TOP, 0, 1, 0 },
};
static const PhBox L_COMPARE[] = {
	{ PH_TITLE, 57.6, 32.4, 844.8, 75.6, 37, HA_LEFT, VA_MIDDLE, 0, 0, 1 },
	{ PH_H1, 57.6, 118.8, 393.6, 54.0, 27, HA_LEFT, VA_TOP, 1, 0, 0 },
	{ PH_H2, 508.8, 118.8, 393.6, 54.0, 27, HA_LEFT, VA_TOP, 1, 0, 0 },
	{ PH_BODY, 57.6, 183.6, 393.6, 302.4, 23, HA_LEFT, VA_TOP, 0, 1, 0 },
	{ PH_BODY2, 508.8, 183.6, 393.6, 302.4, 23, HA_LEFT, VA_TOP, 0, 1, 0 },
};
static const PhBox L_SECTION[] = {
	{ PH_TITLE, 57.6, 162.0, 844.8, 118.8, 48, HA_LEFT, VA_BOTTOM, 0, 0, 1 },
	{ PH_SUB, 57.6, 291.6, 844.8, 75.6, 22, HA_LEFT, VA_TOP, 0, 0, 0 },
};
static const PhBox L_TITLEONLY[] = {
	{ PH_TITLE, 57.6, 32.4, 844.8, 86.4, 37, HA_LEFT, VA_MIDDLE, 0, 0, 1 },
};
static const PhBox L_PICTURE[] = {
	{ PH_TITLE, 57.6, 388.8, 384.0, 64.8, 30, HA_LEFT, VA_BOTTOM, 0, 0, 1 },
	{ PH_SUB, 57.6, 453.6, 384.0, 54.0, 18, HA_LEFT, VA_TOP, 0, 0, 0 },
};
#define NBOX(a) (int)(sizeof (a) / sizeof (PhBox))
static const struct { const char *key; const PhBox *b; int n; } LAYOUT_T[] = {
	{ "title", L_TITLE, NBOX(L_TITLE) }, { "content", L_CONTENT, NBOX(L_CONTENT) },
	{ "two", L_TWO, NBOX(L_TWO) }, { "compare", L_COMPARE, NBOX(L_COMPARE) },
	{ "section", L_SECTION, NBOX(L_SECTION) }, { "titleonly", L_TITLEONLY, NBOX(L_TITLEONLY) },
	{ "blank", NULL, 0 }, { "picture", L_PICTURE, NBOX(L_PICTURE) },
};
static const PhBox *lay_boxes(const char *k, int *n)
{
	for (size_t i = 0; i < sizeof LAYOUT_T / sizeof LAYOUT_T[0]; i++)
		if (!strcmp(LAYOUT_T[i].key, k)) { *n = LAYOUT_T[i].n; return LAYOUT_T[i].b; }
	*n = NBOX(L_CONTENT);
	return L_CONTENT;
}
static int lay_has(const char *k, int ph)
{
	int n; const PhBox *b = lay_boxes(k, &n);
	for (int i = 0; i < n; i++) if (b[i].ph == ph) return 1;
	return 0;
}

/* ------------------------------------------------------------------ the model: a deck's slides (pprof45.js's ppDeckNew)
   A slide holds its layout's name and, for each of the page's text places, the words and what the toolbar set on them
   (the page's object: bold, italic, underline, strikethrough, its alignment, its bullets, its size, its font, its colour),
   with -1 meaning "the layout's own", as the page's placeholder takes its look from its layout. */
typedef struct { char *t; unsigned b, i, u, s; int ha, bl, fs; char *fc, *font; } Ph;
typedef struct { char layout[16]; Ph ph[PH_N]; char *notes; int hidden; } Slide;
typedef struct { Slide *sl; int n, cur; } Snap;

typedef struct { const char *label, *key; int off, op; } Mi;   /* (a menu's row: its words, its key, and what it does) */
typedef struct { const char *title; const Mi *items; int n; } Menu;

static RuiWin *W;
static GtkWidget *TB, *MBAR, *MBTNS[8], *LIST, *STAGE, *OVL, *EDF, *EDITV, *STATUS, *ZTXT, *ZOOMSL;
static GtkWidget *BTN_B, *BTN_I, *BTN_U, *BTN_S, *ALN[3], *BLB[2], *SIZEW, *LAYW;
static GtkTextBuffer *BUF, *NBUF;
static Slide *SL; static int SLN, SLC, CUR, PHCUR, TH, READY, UPDATING, EDIT_ON, EDIT_PH;
static double STK = 1, STX, STY;   /* (the stage's scale and where the slide's corner is: for the mouse and the editor) */
static int ZOOMF = 1; static int ZOOM = 100;
static Snap *HS; static int HSN, HSC, HSI = -1;   /* (HSI: the step on the screen; -1 while the deck has no step) */
static const char *MENU_OPEN; static int MENU_OPEN_N;
static void state_soon(void); static void status_update(void); static void redraw(void);
static void hist_push(void); static void hist_undo(void); static void hist_redo(void);
static void goto_slide(int i); static void new_slide(void); static void dup_slide(void); static void del_slide(void);
static void edit_begin(int ph); static void edit_commit(void); static void upd_tb(void); static void notes_sync(void);
static void menu_do(const Mi *it); static int menus_n(void);
static void redraw(void) { if (STAGE) { gtk_widget_queue_draw(STAGE); } if (LIST) gtk_widget_queue_draw(LIST); }

static Slide slide_new(void)
{
	Slide s;
	memset(&s, 0, sizeof s);
	g_strlcpy(s.layout, "title", sizeof s.layout);
	for (int i = 0; i < PH_N; i++) { s.ph[i].ha = -1; s.ph[i].bl = -1; }
	return s;
}
static void slide_free(Slide *s)
{
	if (!s) return;
	for (int i = 0; i < PH_N; i++) { g_free(s->ph[i].t); g_free(s->ph[i].fc); g_free(s->ph[i].font); }
	g_free(s->notes);
}
static void slide_copy(Slide *d, const Slide *s)
{
	memset(d, 0, sizeof *d);
	g_strlcpy(d->layout, s->layout, sizeof d->layout);
	for (int i = 0; i < PH_N; i++) {
		d->ph[i] = s->ph[i];
		d->ph[i].t = g_strdup(s->ph[i].t); d->ph[i].fc = g_strdup(s->ph[i].fc); d->ph[i].font = g_strdup(s->ph[i].font);
	}
	d->notes = g_strdup(s->notes); d->hidden = s->hidden;
}
/* (a slide is kept once it holds something, as the page's placeholders start empty; the layout always goes with it) */
static void layout_set(Slide *s, const char *k)
{
	g_strlcpy(s->layout, k, sizeof s->layout);
	for (int i = 0; i < PH_N; i++)
		if (!lay_has(k, i)) g_clear_pointer(&s->ph[i].t, g_free);
}

static void deck_ensure(int n)
{
	if (n <= SLC) return;
	int old = SLC;
	SLC = SLC ? SLC * 2 : 8;
	while (SLC < n) SLC *= 2;
	SL = g_renew(Slide, SL, SLC);
	memset(SL + old, 0, sizeof(Slide) * (size_t)(SLC - old));
}
static void deck_insert(int at, const Slide *proto)
{
	deck_ensure(SLN + 1);
	if (at < SLN) memmove(SL + at + 1, SL + at, sizeof(Slide) * (size_t)(SLN - at));
	if (proto) slide_copy(&SL[at], proto); else SL[at] = slide_new();
	SLN++;
}
static void deck_remove(int at)
{
	slide_free(&SL[at]);
	if (at + 1 < SLN) memmove(SL + at, SL + at + 1, sizeof(Slide) * (size_t)(SLN - at - 1));
	SLN--; memset(&SL[SLN], 0, sizeof(Slide));
}

/* ------------------------------------------------------------------ the deck's history (the page's 60 steps, pushed
   after a change, as WordPro and SpreadsheetPro push theirs: the first undo goes back from what is on the screen) */
static void snap_free(Snap *s) { for (int i = 0; i < s->n; i++) slide_free(&s->sl[i]); g_free(s->sl); memset(s, 0, sizeof *s); }
static Slide *deck_copy(const Slide *src, int n)
{
	Slide *c = g_new0(Slide, n > 0 ? n : 1);
	for (int i = 0; i < n; i++) slide_copy(&c[i], &src[i]);
	return c;
}
static void deck_take(Slide *src, int n)
{
	for (int i = 0; i < SLN; i++) slide_free(&SL[i]);
	g_free(SL); SL = src; SLN = n; SLC = n > 0 ? n : 1;
	if (CUR >= SLN) CUR = SLN - 1;
	if (CUR < 0) CUR = 0;
}
static void hist_push(void)
{
	for (int i = HSI + 1; i < HSN; i++) snap_free(&HS[i]);
	HSN = HSI + 1;
	if (HSN == HSC) { HSC = HSC ? HSC * 2 : 16; HS = g_renew(Snap, HS, HSC); memset(HS + HSN, 0, sizeof(Snap) * (size_t)(HSC - HSN)); }
	HS[HSN].sl = deck_copy(SL, SLN); HS[HSN].n = SLN; HS[HSN].cur = CUR;
	HSI = HSN; HSN++;
	if (HSN > HSTP) { snap_free(&HS[0]); memmove(HS, HS + 1, sizeof(Snap) * (size_t)(HSN - 1)); HSN--; HSI--; }
}
static void hist_apply(int i)
{
	Slide *c = deck_copy(HS[i].sl, HS[i].n);
	deck_take(c, HS[i].n);
	CUR = HS[i].cur < SLN ? HS[i].cur : SLN - 1;
	PHCUR = 0;
}
static void hist_undo(void)
{
	if (HSI <= 0) return;
	HSI--; hist_apply(HSI); goto_slide(CUR);
}
static void hist_redo(void)
{
	if (HSI + 1 >= HSN) return;
	HSI++; hist_apply(HSI); goto_slide(CUR);
}

/* ------------------------------------------------------------------ the look of a slide (the page's .pp-slide: the
   theme's background, its title's and its body's inks and fonts, the layout's boxes) */
static int hex_rgb(const char *h, GdkRGBA *c)
{
	if (!h || h[0] != '#' || (int)strlen(h) < 7) return 0;
	unsigned r = 0, g = 0, b = 0;
	if (sscanf(h + 1, "%2x%2x%2x", &r, &g, &b) != 3) return 0;
	c->red = r / 255.0; c->green = g / 255.0; c->blue = b / 255.0; c->alpha = 1.0;
	return 1;
}
/* (a list's words as the page's <ul>/<ol> shows them: a bullet or a number in front of each line that has words) */
static char *ph_disp(const Slide *s, const PhBox *b)
{
	const Ph *p = &s->ph[b->ph];
	if (!p->t || !*p->t) return NULL;
	int bl = p->bl >= 0 ? p->bl : b->bullets;
	if (!bl) return g_strdup(p->t);
	GString *g = g_string_new(NULL);
	char **lines = g_strsplit(p->t, "\n", -1);
	int num = 0;
	for (int i = 0; lines[i]; i++) {
		if (*lines[i]) {
			if (bl == 1) g_string_append(g, "\xe2\x80\xa2  ");
			else g_string_append_printf(g, "%d.  ", ++num);
		}
		g_string_append(g, lines[i]);
		if (lines[i + 1]) g_string_append_c(g, '\n');
	}
	g_strfreev(lines);
	return g_string_free(g, FALSE);
}
static PangoLayout *text_layout(cairo_t *cr, const char *t, const char *font, double px, int bold, int ital, int und,
                                int str, int ha, double w)
{
	PangoLayout *l = pango_cairo_create_layout(cr);
	PangoFontDescription *fd = pango_font_description_new();
	pango_font_description_set_family(fd, font && *font ? font : "Calibri");
	pango_font_description_set_absolute_size(fd, px * PANGO_SCALE);
	pango_font_description_set_weight(fd, bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
	pango_font_description_set_style(fd, ital ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
	pango_layout_set_font_description(l, fd);
	pango_font_description_free(fd);
	if (und || str) {
		PangoAttrList *al = pango_attr_list_new();
		if (und) pango_attr_list_insert(al, pango_attr_underline_new(PANGO_UNDERLINE_SINGLE));
		if (str) pango_attr_list_insert(al, pango_attr_strikethrough_new(TRUE));
		pango_layout_set_attributes(l, al);
		pango_attr_list_unref(al);
	}
	pango_layout_set_text(l, t, -1);
	pango_layout_set_width(l, (int)(w * PANGO_SCALE));
	pango_layout_set_wrap(l, PANGO_WRAP_WORD_CHAR);
	pango_layout_set_alignment(l, ha == HA_CENTRE ? PANGO_ALIGN_CENTER : ha == HA_RIGHT ? PANGO_ALIGN_RIGHT : PANGO_ALIGN_LEFT);
	return l;
}
static void draw_slot(cairo_t *cr, double ox, double oy, double k, const Slide *s, const PhBox *b, const Th *th, int thumb)
{
	const Ph *p = &s->ph[b->ph];
	double x = ox + b->x * k, y = oy + b->y * k, w = b->w * k, h = b->h * k;
	char *t = ph_disp(s, b);
	if (!t) {
		/* (an empty place is a box to click, drawn as the page draws a picture's place: dashed, and never in a thumbnail) */
		if (!thumb) {
			GdkRGBA g = { 0.72, 0.72, 0.72, 1 };
			const double dash[2] = { 5, 4 };
			cairo_set_dash(cr, dash, 2, 0);
			cairo_set_line_width(cr, 1);
			gdk_cairo_set_source_rgba(cr, &g);
			cairo_rectangle(cr, x + 0.5, y + 0.5, w - 1, h - 1);
			cairo_stroke(cr);
			cairo_set_dash(cr, NULL, 0, 0);
		}
		return;
	}
	int fs = p->fs > 0 ? p->fs : b->fs;
	int ha = p->ha > 0 ? p->ha - 1 : b->ha;
	GdkRGBA col;
	if (!hex_rgb(p->fc, &col) && !hex_rgb(b->tf ? th->title : th->fg, &col)) { col = (GdkRGBA){ 0, 0, 0, 1 }; }
	PangoLayout *l = text_layout(cr, t, p->font, fs * k, b->bold || p->b, p->i, p->u, p->s, ha, w);
	int lh; pango_layout_get_pixel_size(l, NULL, &lh);
	double dy = b->va == VA_MIDDLE ? (h - lh) / 2 : b->va == VA_BOTTOM ? h - lh : 0;
	if (dy < 0) dy = 0;
	cairo_move_to(cr, x, y + dy);
	gdk_cairo_set_source_rgba(cr, &col);
	pango_cairo_show_layout(cr, l);
	g_object_unref(l);
	g_free(t);
}
static void draw_picture_place(cairo_t *cr, double ox, double oy, double k, int thumb)
{
	if (thumb) return;
	double x = ox + 480 * k, y = oy + 43.2 * k, w = 422.4 * k, h = 453.6 * k;
	GdkRGBA g = { 0.66, 0.66, 0.66, 1 };
	const double dash[2] = { 6, 5 };
	cairo_set_dash(cr, dash, 2, 0); cairo_set_line_width(cr, 2 * (k < 0.5 ? k * 2 : 1));
	gdk_cairo_set_source_rgba(cr, &g);
	cairo_rectangle(cr, x, y, w, h);
	cairo_stroke(cr);
	cairo_set_dash(cr, NULL, 0, 0);
	PangoLayout *l = text_layout(cr, rui_tr("Picture"), "Calibri", 18 * k, 0, 0, 0, 0, HA_CENTRE, w);
	cairo_move_to(cr, x, y + h / 2 - 10 * k);
	gdk_cairo_set_source_rgba(cr, &g);
	pango_cairo_show_layout(cr, l);
	g_object_unref(l);
}
static void draw_slide(cairo_t *cr, double ox, double oy, double k, const Slide *s, int thumb)
{
	const Th *th = &PP_THEMES[TH];
	GdkRGBA bg;
	cairo_save(cr);
	cairo_rectangle(cr, ox, oy, SLW * k, SLH * k);
	cairo_clip(cr);
	/* (the page's theme: a plain ink, or a gradient the Design step makes; a gradient slides are later steps' work) */
	if (!hex_rgb(th->bg, &bg)) bg = (GdkRGBA){ 1, 1, 1, 1 };
	gdk_cairo_set_source_rgba(cr, &bg);
	cairo_paint(cr);
	int n; const PhBox *b = lay_boxes(s->layout, &n);
	for (int i = 0; i < n; i++) draw_slot(cr, ox, oy, k, s, &b[i], th, thumb);
	if (!strcmp(s->layout, "picture")) draw_picture_place(cr, ox, oy, k, thumb);
	cairo_restore(cr);
}

/* ------------------------------------------------------------------ the stage and the list of slides (the page's
   .pp-wrap .pp-stagebox .pp-stage, and .pp-list .pp-th .n) */
static gboolean stage_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)d;
	int ww = gtk_widget_get_allocated_width(w), wh = gtk_widget_get_allocated_height(w);
	double k = ZOOMF ? fmin((ww - 2 * 20) / SLW, (wh - 2 * 20) / SLH) : ZOOM / 100.0;
	if (!(k > 0.05)) k = 0.05;
	STK = k; STX = (ww - SLW * k) / 2; STY = (wh - SLH * k) / 2;
	if (STX < 0) STX = 0;
	if (STY < 0) STY = 0;
	GdkRGBA grey = { 0.898, 0.906, 0.918, 1 };   /* (#e5e7eb, the page's .pp-stagebox) */
	gdk_cairo_set_source_rgba(cr, &grey);
	cairo_paint(cr);
	{ /* (the page's slide shadow: a soft box under the white slide) */
		GdkRGBA sh = { 0, 0, 0, 0.18 };
		gdk_cairo_set_source_rgba(cr, &sh);
		cairo_rectangle(cr, STX + 2, STY + 4, SLW * k, SLH * k);
		cairo_fill(cr);
	}
	draw_slide(cr, STX, STY, k, &SL[CUR], 0);
	return TRUE;
}
static double list_item_h(void) { return THH + THGAP; }
static int list_height(void) { return (int)ceil(PAD + SLN * list_item_h() + 2); }
static gboolean list_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)d;
	int wh = gtk_widget_get_allocated_height(w);
	GdkRGBA bg = { 0.949, 0.957, 0.965, 1 };   /* (#f3f4f6, the page's .pp-list) */
	gdk_cairo_set_source_rgba(cr, &bg);
	cairo_paint(cr);
	for (int i = 0; i < SLN; i++) {
		double y = PAD + i * list_item_h();
		if (y > wh) break;
		double k = THW / SLW;
		double tx = 28, ty = y, tw = THW, th2 = THH;
		/* (the page's .pp-th: white, a thin border, and the current slide in the app's blue) */
		GdkRGBA wb = { 1, 1, 1, 1 };
		gdk_cairo_set_source_rgba(cr, &wb);
		cairo_rectangle(cr, tx, ty, tw, th2);
		cairo_fill(cr);
		cairo_save(cr);
		cairo_rectangle(cr, tx, ty, tw, th2);
		cairo_clip(cr);
		double alpha = SL[i].hidden ? 0.35 : 1.0;
		cairo_push_group(cr);
		draw_slide(cr, tx, ty, k, &SL[i], 1);
		cairo_pop_group_to_source(cr);
		cairo_paint_with_alpha(cr, alpha);
		cairo_restore(cr);
		GdkRGBA bd = i == CUR ? (GdkRGBA){ 0.145, 0.388, 0.922, 1 } : (GdkRGBA){ 0.80, 0.82, 0.85, 1 };
		gdk_cairo_set_source_rgba(cr, &bd);
		cairo_set_line_width(cr, i == CUR ? 2 : 1);
		cairo_rectangle(cr, tx + 0.5, ty + 0.5, tw - 1, th2 - 1);
		cairo_stroke(cr);
		/* (the slide's number at the left, as the page's .n has it) */
		char nb[16]; snprintf(nb, sizeof nb, "%d", i + 1);
		PangoLayout *l = text_layout(cr, nb, "Calibri", 11, i == CUR, 0, 0, SL[i].hidden, HA_RIGHT, 16);
		gdk_cairo_set_source_rgba(cr, &bd);
		cairo_move_to(cr, 6, y + 6);
		pango_cairo_show_layout(cr, l);
		g_object_unref(l);
		if (SL[i].hidden) {
			GdkRGBA h = { 0.35, 0.35, 0.35, 1 };
			gdk_cairo_set_source_rgba(cr, &h);
			cairo_set_line_width(cr, 1);
			cairo_move_to(cr, 6, y + 7); cairo_line_to(cr, 22, y + 7);
			cairo_stroke(cr);
		}
	}
	return TRUE;
}
static void list_size(void) { if (LIST) gtk_widget_set_size_request(LIST, (int)LISTW, list_height()); }
static void zoom_label(void)
{
	if (!ZTXT) return;
	char z[16];
	if (ZOOMF) snprintf(z, sizeof z, "%s", rui_tr("Fit"));
	else snprintf(z, sizeof z, "%d%%", ZOOM);
	gtk_label_set_text(GTK_LABEL(ZTXT), z);
	if (ZOOMSL) {
		int u = UPDATING; UPDATING = 1;
		gtk_range_set_value(GTK_RANGE(ZOOMSL), ZOOMF ? (int)lround(fmin(200, fmax(25, STK * 100))) : ZOOM);
		UPDATING = u;
	}
}
static void status_update(void)
{
	if (!STATUS) return;
	char s[160];
	snprintf(s, sizeof s, "%s %d %s %d%s", rui_tr("Slide"), CUR + 1, rui_tr("of"), SLN, SL[CUR].hidden ? rui_tr(" \xc2\xb7 hidden") : "");
	gtk_label_set_text(GTK_LABEL(STATUS), s);
	zoom_label();
	state_soon();
}

/* ------------------------------------------------------------------ what a change does */
static void slide_changed(void)
{
	hist_push();
	list_size();
	redraw();
	status_update();
}
static void goto_slide(int i)
{
	if (SLN <= 0) return;
	if (i < 0) i = 0;
	if (i >= SLN) i = SLN - 1;
	CUR = i;
	int n; const PhBox *b = lay_boxes(SL[CUR].layout, &n);
	PHCUR = n > 0 ? b[0].ph : 0;
	notes_sync();
	upd_tb(); redraw(); status_update();
}
static void new_slide(void)
{
	/* (the page's addSlide: the slide before's layout, and a title slide is followed by a content one) */
	const char *lay = strcmp(SL[CUR].layout, "title") ? SL[CUR].layout : "content";
	Slide s = slide_new();
	g_strlcpy(s.layout, lay, sizeof s.layout);
	deck_insert(CUR + 1, &s);
	slide_free(&s);
	goto_slide(CUR + 1);
	slide_changed();
}
static void dup_slide(void)
{
	Slide tmp; slide_copy(&tmp, &SL[CUR]);
	deck_insert(CUR + 1, &tmp);
	slide_free(&tmp);
	goto_slide(CUR + 1);
	slide_changed();
}
static void del_slide(void)
{
	if (SLN <= 1) return;
	deck_remove(CUR);
	if (CUR >= SLN) CUR = SLN - 1;
	goto_slide(CUR);
	slide_changed();
}
static void set_layout(const char *key)
{
	layout_set(&SL[CUR], key);
	goto_slide(CUR);
	slide_changed();
}
static void toggle_style(int k)
{
	Ph *p = &SL[CUR].ph[PHCUR];
	if (!lay_has(SL[CUR].layout, PHCUR)) return;
	unsigned *f = k == 0 ? &p->b : k == 1 ? &p->i : k == 2 ? &p->u : &p->s;
	*f = !*f;
	upd_tb(); slide_changed();
}
static void set_align(int ha)
{
	Ph *p = &SL[CUR].ph[PHCUR];
	if (!lay_has(SL[CUR].layout, PHCUR)) return;
	p->ha = ha;
	upd_tb(); slide_changed();
}
static void set_list(int mode)
{
	Ph *p = &SL[CUR].ph[PHCUR];
	if (!lay_has(SL[CUR].layout, PHCUR)) return;
	int n; const PhBox *b = lay_boxes(SL[CUR].layout, &n);
	int cur = p->bl >= 0 ? p->bl : 0;
	for (int i = 0; i < n; i++) if (b[i].ph == PHCUR) cur = p->bl >= 0 ? p->bl : b[i].bullets;
	p->bl = cur == mode ? 0 : mode;   /* (the page's act.ul/act.ol: the item again takes the list away) */
	upd_tb(); slide_changed();
}
static void set_text(int ph, const char *t)
{
	Ph *p = &SL[CUR].ph[ph];
	g_free(p->t);
	p->t = t && *t ? g_strdup(t) : NULL;
	slide_changed();
}
static void upd_tb(void)
{
	if (!READY) return;
	Ph *p = &SL[CUR].ph[PHCUR];
	int has = lay_has(SL[CUR].layout, PHCUR);
	int bl = p->bl;
	int ha = p->ha;
	GtkWidget *b[4] = { BTN_B, BTN_I, BTN_U, BTN_S };
	unsigned on[4] = { p->b, p->i, p->u, p->s };
	for (int i = 0; i < 4; i++) {
		if (!b[i]) continue;
		GtkStyleContext *c = gtk_widget_get_style_context(b[i]);
		if (has && on[i]) gtk_style_context_add_class(c, "wp-on"); else gtk_style_context_remove_class(c, "wp-on");
	}
	for (int i = 0; i < 3; i++) {
		if (!ALN[i]) continue;
		GtkStyleContext *c = gtk_widget_get_style_context(ALN[i]);
		if (has && ha == i + 1) gtk_style_context_add_class(c, "wp-on"); else gtk_style_context_remove_class(c, "wp-on");
	}
	for (int i = 0; i < 2; i++) {
		if (!BLB[i]) continue;
		GtkStyleContext *c = gtk_widget_get_style_context(BLB[i]);
		if (has && bl == i + 1) gtk_style_context_add_class(c, "wp-on"); else gtk_style_context_remove_class(c, "wp-on");
	}
	if (LAYW) {
		int u = UPDATING; UPDATING = 1;
		for (int i = 0; i < NPPLAYOUTS; i++) if (!strcmp(PP_LAYOUTS[i].key, SL[CUR].layout)) gtk_combo_box_set_active(GTK_COMBO_BOX(LAYW), i);
		UPDATING = u;
	}
	if (SIZEW) {
		int u = UPDATING; UPDATING = 1;
		int fs = p->fs;
		if (fs <= 0) { int n; const PhBox *bx = lay_boxes(SL[CUR].layout, &n); fs = 18; for (int i = 0; i < n; i++) if (bx[i].ph == PHCUR) fs = bx[i].fs; }
		int at = -1;
		for (int i = 0; i < NPPSIZES; i++) if (PP_SIZES[i] <= fs) at = i;
		gtk_combo_box_set_active(GTK_COMBO_BOX(SIZEW), at);
		UPDATING = u;
	}
}

/* ------------------------------------------------------------------ the text of a placeholder, edited where it is
   (the page's contenteditable: the words go in, Escape or a click away puts them on the slide) */
static void edit_begin(int ph)
{
	int n; const PhBox *b = lay_boxes(SL[CUR].layout, &n); const PhBox *box = NULL;
	for (int i = 0; i < n; i++) if (b[i].ph == ph) box = &b[i];
	if (!box || !EDF) return;
	PHCUR = ph; EDIT_PH = ph;
	gtk_widget_set_margin_start(EDF, (int)lround(STX + box->x * STK));
	gtk_widget_set_margin_top(EDF, (int)lround(STY + box->y * STK));
	gtk_widget_set_size_request(EDF, (int)lround(box->w * STK), (int)lround(box->h * STK));
	gtk_widget_set_halign(EDF, GTK_ALIGN_START);
	gtk_widget_set_valign(EDF, GTK_ALIGN_START);
	{ /* (the page's editing object: its own size in the slide's words) */
		const Th *th = &PP_THEMES[TH];
		Ph *p = &SL[CUR].ph[ph];
		int fs = p->fs > 0 ? p->fs : box->fs;
		GtkCssProvider *pr = gtk_css_provider_new();
		char *c = g_strdup_printf("textview{font-size:%.1fpx;font-family:'%s';}", fs * STK,
		                          p->font ? p->font : (box->tf ? th->tf : th->bf));
		gtk_css_provider_load_from_data(pr, c, -1, NULL);
		gtk_style_context_add_provider(gtk_widget_get_style_context(EDITV), GTK_STYLE_PROVIDER(pr),
		                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		g_object_unref(pr);
		g_free(c);
	}
	gtk_text_buffer_set_text(BUF, SL[CUR].ph[ph].t ? SL[CUR].ph[ph].t : "", -1);
	EDIT_ON = 1;
	gtk_widget_show(EDF);
	gtk_widget_grab_focus(EDITV);
}
static void edit_commit(void)
{
	if (!EDIT_ON) return;
	EDIT_ON = 0;
	GtkTextIter a, b;
	gtk_text_buffer_get_bounds(BUF, &a, &b);
	char *t = gtk_text_buffer_get_text(BUF, &a, &b, FALSE);
	int changed = g_strcmp0(t, SL[CUR].ph[EDIT_PH].t) != 0;
	gtk_widget_hide(EDF);
	gtk_widget_grab_focus(STAGE);
	if (changed) set_text(EDIT_PH, t);
	g_free(t);
	redraw();
	status_update();
}
static gboolean on_edit_key(GtkWidget *w, GdkEventKey *e, gpointer d)
{
	(void)w; (void)d;
	if (e->keyval == GDK_KEY_Escape) { edit_commit(); return TRUE; }
	return FALSE;
}
static gboolean on_edit_focus_out(GtkWidget *w, GdkEventFocus *e, gpointer d)
{
	(void)w; (void)e; (void)d;
	if (EDIT_ON) edit_commit();
	return FALSE;
}

/* ------------------------------------------------------------------ the window's own hands: the stage's mouse, the
   list's mouse, and the page's keys */
static const PhBox *ph_at(double sx, double sy)
{
	int n; const PhBox *b = lay_boxes(SL[CUR].layout, &n);
	for (int i = 0; i < n; i++)
		if (sx >= b[i].x && sx <= b[i].x + b[i].w && sy >= b[i].y && sy <= b[i].y + b[i].h) return &b[i];
	return n > 0 ? &b[0] : NULL;
}
static gboolean on_stage_click(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)w; (void)d;
	if (e->button != 1) return FALSE;
	const PhBox *b = ph_at((e->x - STX) / STK, (e->y - STY) / STK);
	if (b) {
		PHCUR = b->ph;
		upd_tb(); redraw();
		if (e->type == GDK_2BUTTON_PRESS) edit_begin(b->ph);
	}
	gtk_widget_grab_focus(STAGE);
	return TRUE;
}
static gboolean on_list_click(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)w; (void)d;
	if (e->button != 1) return FALSE;
	int i = (int)floor((e->y - PAD) / list_item_h());
	gtk_widget_grab_focus(LIST);
	if (i >= 0 && i < SLN) goto_slide(i);
	return TRUE;
}
static void next_ph(int step)
{
	int n; const PhBox *b = lay_boxes(SL[CUR].layout, &n);
	if (n <= 0) return;
	int at = 0;
	for (int i = 0; i < n; i++) if (b[i].ph == PHCUR) at = i;
	at = (at + step + n) % n;
	PHCUR = b[at].ph;
	upd_tb(); redraw();
}
static gboolean on_key(GtkWidget *w, GdkEventKey *e, gpointer d)
{
	(void)w; (void)d;
	gboolean ctrl = (e->state & GDK_CONTROL_MASK) != 0;
	gboolean shift = (e->state & GDK_SHIFT_MASK) != 0;
	gboolean listf = LIST && gtk_widget_has_focus(LIST);
	switch (e->keyval) {
	case GDK_KEY_z: if (ctrl) { if (shift) hist_redo(); else hist_undo(); return TRUE; } break;
	case GDK_KEY_y: if (ctrl) { hist_redo(); return TRUE; } break;
	case GDK_KEY_m: if (ctrl) { new_slide(); return TRUE; } break;
	case GDK_KEY_d: if (ctrl) { dup_slide(); return TRUE; } break;
	case GDK_KEY_b: if (ctrl) { toggle_style(0); return TRUE; } break;
	case GDK_KEY_i: if (ctrl) { toggle_style(1); return TRUE; } break;
	case GDK_KEY_u: if (ctrl) { toggle_style(2); return TRUE; } break;
	case GDK_KEY_Page_Down: goto_slide(CUR + 1); return TRUE;
	case GDK_KEY_Page_Up: goto_slide(CUR - 1); return TRUE;
	case GDK_KEY_Down: if (listf) { goto_slide(CUR + 1); return TRUE; } break;
	case GDK_KEY_Up: if (listf) { goto_slide(CUR - 1); return TRUE; } break;
	case GDK_KEY_Delete: if (listf) { del_slide(); return TRUE; } break;
	case GDK_KEY_Return: case GDK_KEY_KP_Enter: case GDK_KEY_F2: edit_begin(PHCUR); return TRUE;
	case GDK_KEY_Tab: next_ph(shift ? -1 : 1); return TRUE;
	case GDK_KEY_ISO_Left_Tab: next_ph(-1); return TRUE;
	case GDK_KEY_Escape: if (EDIT_ON) { edit_commit(); return TRUE; } break;
	default: break;
	}
	return FALSE;
}
static void notes_sync(void)
{
	if (!NBUF) return;
	int u = UPDATING; UPDATING = 1;
	gtk_text_buffer_set_text(NBUF, SL[CUR].notes ? SL[CUR].notes : "", -1);
	UPDATING = u;
}
static void on_notes_changed(GtkTextBuffer *b, gpointer d)
{
	(void)b; (void)d;
	if (UPDATING || !READY) return;
	GtkTextIter a, z;
	gtk_text_buffer_get_bounds(NBUF, &a, &z);
	char *t = gtk_text_buffer_get_text(NBUF, &a, &z, FALSE);
	g_free(SL[CUR].notes);
	SL[CUR].notes = *t ? g_strdup(t) : NULL;
	g_free(t);
	/* (the notes are part of the slide, but a keystroke is not a step of the page's undo: it is put on the slide as it
	   is typed, as the page's contenteditable leaves it) */
	state_soon();
}

/* ------------------------------------------------------------------ the toolbar's and the menus' callbacks */
static void on_new_btn(GtkWidget *b, gpointer d) { (void)b; (void)d; new_slide(); }
static void on_dup_btn(GtkWidget *b, gpointer d) { (void)b; (void)d; dup_slide(); }
static void on_del_btn(GtkWidget *b, gpointer d) { (void)b; (void)d; del_slide(); }
static void on_undo_btn(GtkWidget *b, gpointer d) { (void)b; (void)d; hist_undo(); }
static void on_redo_btn(GtkWidget *b, gpointer d) { (void)b; (void)d; hist_redo(); }
static void on_fit_btn(GtkWidget *b, gpointer d) { (void)b; (void)d; ZOOMF = 1; redraw(); status_update(); }
static void on_style_btn(GtkWidget *b, gpointer d) { (void)b; toggle_style((int)(intptr_t)d); }
static void on_align_btn(GtkWidget *b, gpointer d) { (void)b; set_align((int)(intptr_t)d); }
static void on_bullets_btn(GtkWidget *b, gpointer d) { (void)b; set_list(1); }
static void on_number_btn(GtkWidget *b, gpointer d) { (void)b; set_list(2); }
static void on_layout_combo(GtkComboBox *c, gpointer d)
{
	(void)d;
	if (!READY || UPDATING) return;
	int i = gtk_combo_box_get_active(c);
	if (i >= 0 && i < NPPLAYOUTS && strcmp(PP_LAYOUTS[i].key, SL[CUR].layout)) set_layout(PP_LAYOUTS[i].key);
}
static void on_zoom(GtkRange *r, gpointer d)
{
	(void)d;
	if (!READY || UPDATING) return;
	ZOOM = (int)lround(gtk_range_get_value(r));
	ZOOMF = 0;
	redraw();
	zoom_label();
	state_soon();
}
static void on_font(GtkComboBox *c, gpointer d)
{
	(void)d;
	if (!READY || UPDATING) return;
	int i = gtk_combo_box_get_active(c);
	if (i < 0 || i >= NPPFONTS || !lay_has(SL[CUR].layout, PHCUR)) return;
	g_free(SL[CUR].ph[PHCUR].font);
	SL[CUR].ph[PHCUR].font = g_strdup(PP_FONTS[i]);
	slide_changed();
}
static void on_fsize(GtkComboBox *c, gpointer d)
{
	(void)d;
	if (!READY || UPDATING) return;
	int i = gtk_combo_box_get_active(c);
	if (i < 0 || i >= NPPSIZES || !lay_has(SL[CUR].layout, PHCUR)) return;
	SL[CUR].ph[PHCUR].fs = PP_SIZES[i];
	slide_changed();
}
static void on_colour(GtkColorButton *cb, gpointer d)
{
	(void)d;
	if (!READY || UPDATING) return;
	GdkRGBA c;
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(cb), &c);
	char *hex = g_strdup_printf("#%02x%02x%02x", (int)lround(c.red * 255), (int)lround(c.green * 255), (int)lround(c.blue * 255));
	g_free(SL[CUR].ph[PHCUR].fc);
	SL[CUR].ph[PHCUR].fc = hex;
	slide_changed();
}

/* ------------------------------------------------------------------ the eight menus, with the page's own rows */
enum {
	OP_NONE, OP_NEW, OP_TEMPLATE, OP_OPEN, OP_SAVE, OP_SAVEAS, OP_EXPORT, OP_PRINT, OP_CLOSE,
	OP_UNDO, OP_REDO, OP_CUT, OP_COPY, OP_PASTE, OP_DUPOBJ, OP_SELALL,
	OP_NEWSLIDE, OP_DUPSLIDE, OP_DELSLIDE, OP_HIDESLIDE, OP_LAYOUT, OP_BOLD, OP_ITAL, OP_UNDER, OP_BULLETS, OP_NUMBER,
	OP_INSERT, OP_MASTER, OP_ENDSHOW, OP_ARRANGE, OP_OBJECT, OP_HOME, OP_NORMAL, OP_SORTER, OP_NOTES, OP_NOTEY,
	OP_KEYS, OP_ABOUT,
};
#define NMI(a) (int)(sizeof (a) / sizeof (Mi))
#define SEP { NULL, NULL, 1, OP_NONE }

static const Mi M_FILE[] = {
	{ "New", "Ctrl+N", 0, OP_NEW },
	{ "New from template\xe2\x80\xa6", "", 1, OP_TEMPLATE },
	{ "Open\xe2\x80\xa6 (.RDECK, .PPTX)", "Ctrl+O", 1, OP_OPEN },
	{ "Save", "Ctrl+S", 1, OP_SAVE },
	{ "Save as\xe2\x80\xa6 (.RDECK, .PPTX)", "", 1, OP_SAVEAS },
	SEP,
	{ "Export as PowerPoint (.pptx)\xe2\x80\xa6", "", 1, OP_EXPORT },
	{ "Export as PDF: slides", "", 1, OP_EXPORT },
	{ "Export as PDF: handouts, 1 a page", "", 1, OP_EXPORT },
	{ "Export as PDF: handouts, 2 a page", "", 1, OP_EXPORT },
	{ "Export as PDF: handouts, 3 a page with lines", "", 1, OP_EXPORT },
	{ "Export as PDF: handouts, 6 a page", "", 1, OP_EXPORT },
	{ "Export as PDF: notes pages", "", 1, OP_EXPORT },
	{ "Export the slides as pictures (PNG)", "", 1, OP_EXPORT },
	{ "Send to WordPro (a handout)", "", 1, OP_EXPORT },
	SEP,
	{ "Print\xe2\x80\xa6", "Ctrl+P", 1, OP_PRINT },
	{ "Close", "", 0, OP_CLOSE },
};
static const Mi M_HOME[] = {
	{ "Undo", "Ctrl+Z", 0, OP_UNDO },
	{ "Redo", "Ctrl+Y", 0, OP_REDO },
	SEP,
	{ "Cut", "Ctrl+X", 1, OP_CUT },
	{ "Copy", "Ctrl+C", 1, OP_COPY },
	{ "Paste", "Ctrl+V", 1, OP_PASTE },
	{ "Duplicate", "Ctrl+D", 1, OP_DUPOBJ },
	{ "Select all", "Ctrl+A", 1, OP_SELALL },
	SEP,
	{ "New slide", "Ctrl+M", 0, OP_NEWSLIDE },
	{ "Duplicate slide", "", 0, OP_DUPSLIDE },
	{ "Delete slide", "", 0, OP_DELSLIDE },
	{ "Hide slide", "", 0, OP_HIDESLIDE },
	{ "Layout", "", 0, OP_LAYOUT },
	SEP,
	{ "Bold", "Ctrl+B", 0, OP_BOLD },
	{ "Italic", "Ctrl+I", 0, OP_ITAL },
	{ "Underline", "Ctrl+U", 0, OP_UNDER },
	{ "Bullets", "", 0, OP_BULLETS },
	{ "Numbering", "", 0, OP_NUMBER },
};
static const Mi M_INSERT[] = {
	{ "Text box", "", 1, OP_INSERT },
	{ "Rectangle", "", 1, OP_INSERT },
	{ "Rounded rectangle", "", 1, OP_INSERT },
	{ "Ellipse", "", 1, OP_INSERT },
	{ "Triangle", "", 1, OP_INSERT },
	{ "More shapes\xe2\x80\xa6", "", 1, OP_INSERT },
	{ "Connector (between two selected objects)", "", 1, OP_INSERT },
	SEP,
	{ "Picture\xe2\x80\xa6", "", 1, OP_INSERT },
	{ "Table\xe2\x80\xa6", "", 1, OP_INSERT },
	{ "Chart\xe2\x80\xa6", "", 1, OP_INSERT },
	{ "Icon\xe2\x80\xa6", "", 1, OP_INSERT },
	SEP,
	{ "Slide number on every slide", "", 1, OP_MASTER },
	{ "Date on every slide", "", 1, OP_MASTER },
	{ "Footer\xe2\x80\xa6", "", 1, OP_MASTER },
};
static const Mi M_DESIGN[] = {
	{ "Themes\xe2\x80\xa6", "", 1, OP_MASTER },
	{ "Background\xe2\x80\xa6", "", 1, OP_MASTER },
	{ "Slide size\xe2\x80\xa6", "", 1, OP_MASTER },
	{ "Master: footer, date, numbers, logo\xe2\x80\xa6", "", 1, OP_MASTER },
};
static const Mi M_ARRANGE[] = {
	{ "Bring to front", "", 1, OP_ARRANGE },
	{ "Bring forward", "", 1, OP_ARRANGE },
	{ "Send backward", "", 1, OP_ARRANGE },
	{ "Send to back", "", 1, OP_ARRANGE },
	SEP,
	{ "Align left", "", 1, OP_ARRANGE },
	{ "Align centre", "", 1, OP_ARRANGE },
	{ "Align right", "", 1, OP_ARRANGE },
	{ "Align top", "", 1, OP_ARRANGE },
	{ "Align middle", "", 1, OP_ARRANGE },
	{ "Align bottom", "", 1, OP_ARRANGE },
	{ "Distribute horizontally", "", 1, OP_ARRANGE },
	{ "Distribute vertically", "", 1, OP_ARRANGE },
	SEP,
	{ "Group", "Ctrl+G", 1, OP_ARRANGE },
	{ "Ungroup", "Ctrl+Shift+G", 1, OP_ARRANGE },
	SEP,
	{ "Format shape\xe2\x80\xa6", "", 1, OP_OBJECT },
	{ "Format picture\xe2\x80\xa6", "", 1, OP_OBJECT },
	{ "Rotate right 90\xc2\xb0", "", 1, OP_OBJECT },
	{ "Flip to level", "", 1, OP_OBJECT },
};
static const Mi M_SHOW[] = {
	{ "From the beginning", "F5", 1, OP_ENDSHOW },
	{ "From this slide", "Shift+F5", 1, OP_ENDSHOW },
	{ "Rehearse timings", "", 1, OP_ENDSHOW },
	{ "Set up the show\xe2\x80\xa6", "", 1, OP_ENDSHOW },
	SEP,
	{ "Transition\xe2\x80\xa6", "", 1, OP_ENDSHOW },
	{ "Animation\xe2\x80\xa6", "", 1, OP_ENDSHOW },
};
static const Mi M_VIEW[] = {
	{ "Normal", "", 0, OP_NORMAL },
	{ "Slide sorter", "", 1, OP_SORTER },
	SEP,
	{ "Fit the slide to the window", "", 0, OP_HOME },
	{ "Notes", "", 0, OP_NOTES },
	SEP,
	{ "Show Notey, the assistant", "", 1, OP_NOTEY },
};
static const Mi M_HELP[] = {
	{ "Ask Notey\xe2\x80\xa6", "", 1, OP_NOTEY },
	{ "Keyboard shortcuts", "", 0, OP_KEYS },
	{ "About PresentPro", "", 0, OP_ABOUT },
};
static const Menu MENUS[] = {
	{ "File", M_FILE, NMI(M_FILE) }, { "Home", M_HOME, NMI(M_HOME) }, { "Insert", M_INSERT, NMI(M_INSERT) },
	{ "Design", M_DESIGN, NMI(M_DESIGN) }, { "Arrange", M_ARRANGE, NMI(M_ARRANGE) }, { "Slide Show", M_SHOW, NMI(M_SHOW) },
	{ "View", M_VIEW, NMI(M_VIEW) }, { "Help", M_HELP, NMI(M_HELP) },
};
#define NMENUS (int)(sizeof MENUS / sizeof MENUS[0])
static int menus_n(void) { return NMENUS; }

static void show_keys(void)
{
	const char *b[1];
	b[0] = rui_tr("Close");
	rui_confirm(W, rui_tr("Keyboard shortcuts"),
	            rui_tr("Ctrl+M  New slide\nCtrl+D  Duplicate slide\nCtrl+Z, Ctrl+Y  Undo, redo\nEnter, F2  Edit the text\n"
	                   "Tab  The next place\nCtrl+B, Ctrl+I, Ctrl+U  Bold, italic, underline\nPageUp, PageDown  The slide before, after\n"
	                   "Double-click a place to type in it"), b, 1, 0, NULL, NULL);
}
static void layout_pick_cb(int i, void *d)
{
	(void)d;
	if (i >= 0 && i < NPPLAYOUTS) set_layout(PP_LAYOUTS[i].key);
}
static void menu_do(const Mi *it)
{
	switch (it->op) {
	case OP_NEW: {
		Slide s = slide_new();
		for (int i = 0; i < SLN; i++) slide_free(&SL[i]);
		g_free(SL); SL = NULL; SLN = 0; SLC = 0;
		deck_insert(0, &s); slide_free(&s);
		CUR = 0;
		goto_slide(0); slide_changed();
		break; }
	case OP_CLOSE: gtk_window_close(GTK_WINDOW(W->win)); break;
	case OP_UNDO: hist_undo(); break;
	case OP_REDO: hist_redo(); break;
	case OP_NEWSLIDE: new_slide(); break;
	case OP_DUPSLIDE: dup_slide(); break;
	case OP_DELSLIDE: del_slide(); break;
	case OP_HIDESLIDE:
		SL[CUR].hidden = !SL[CUR].hidden;
		slide_changed();
		break;
	case OP_LAYOUT: {
		/* (the page's Layout submenu: the eight layouts, in the page's order) */
		const char *b[NPPLAYOUTS];
		for (int i = 0; i < NPPLAYOUTS; i++) b[i] = rui_tr(PP_LAYOUTS[i].name);
		rui_confirm(W, rui_tr("Layout"), rui_tr("The slide's layout"), b, NPPLAYOUTS, 0, layout_pick_cb, NULL);
		break; }
	case OP_BOLD: toggle_style(0); break;
	case OP_ITAL: toggle_style(1); break;
	case OP_UNDER: toggle_style(2); break;
	case OP_BULLETS: set_list(1); break;
	case OP_NUMBER: set_list(2); break;
	case OP_HOME: ZOOMF = 1; redraw(); status_update(); break;
	case OP_NOTES: {
		GtkWidget *bar = g_object_get_data(G_OBJECT(W->win), "pp-notesbar");
		if (bar) gtk_widget_set_visible(bar, !gtk_widget_get_visible(bar));
		break; }
	case OP_KEYS: show_keys(); break;
	case OP_ABOUT:
		rui_about(W, "PresentPro", rui_tr("The presentation app of Rudolf OS, a companion to WordPro and SpreadsheetPro: "
		                                  "themes, transitions and animations, a presenter view, and PowerPoint files."));
		break;
	default: break;   /* (a row the step cannot choose is drawn greyed, and never gets here) */
	}
}
static void menu_pick(int i, void *d)
{
	int m = (int)(intptr_t)d;
	MENU_OPEN = NULL;
	if (i >= 0 && i < MENUS[m].n && !MENUS[m].items[i].off) menu_do(&MENUS[m].items[i]);
	state_soon();
}
static void open_menu(GtkWidget *b, int m)
{
	RuiMenuItem *it = g_new0(RuiMenuItem, MENUS[m].n);
	for (int k = 0; k < MENUS[m].n; k++) {
		const Mi *s = &MENUS[m].items[k];
		it[k].label = s->label; it[k].key = s->key; it[k].off = s->off; it[k].sep = s->label == NULL;
	}
	GtkAllocation a; gtk_widget_get_allocation(b, &a);
	int ox = 0, oy = 0;
	if (gtk_widget_get_window(b)) gdk_window_get_origin(gtk_widget_get_window(b), &ox, &oy);
	MENU_OPEN = MENUS[m].title; MENU_OPEN_N = MENUS[m].n;
	rui_menu(b, ox, oy + a.height, NULL, it, MENUS[m].n, menu_pick, (void *)(intptr_t)m);
	g_free(it);
	state_soon();
}
static void mb_click(GtkWidget *b, gpointer d) { open_menu(b, (int)(intptr_t)d); }
static gboolean menu_hook(gpointer d) { int m = (int)(intptr_t)d; open_menu(MBTNS[m], m); return G_SOURCE_REMOVE; }

/* ------------------------------------------------------------------ the window's parts */
static GtkWidget *tb_button(const char *label, const char *tip, GCallback cb, gpointer d)
{
	GtkWidget *b = gtk_button_new();
	GtkWidget *l = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(l), label);
	gtk_container_add(GTK_CONTAINER(b), l);
	gtk_style_context_add_class(gtk_widget_get_style_context(b), "rui");
	gtk_style_context_add_class(gtk_widget_get_style_context(b), "wp-tb");
	gtk_widget_set_tooltip_text(b, rui_tr(tip));
	gtk_widget_set_can_focus(b, FALSE);
	if (cb) g_signal_connect(b, "clicked", cb, d);
	gtk_widget_set_size_request(b, (int)lround(rui_px(26)), (int)lround(rui_px(26)));
	return b;
}
static GtkWidget *tb_wide(const char *label, const char *tip, GCallback cb)
{
	GtkWidget *b = tb_button(label, tip, cb, NULL);
	gtk_widget_set_size_request(b, (int)lround(rui_px(64)), (int)lround(rui_px(26)));
	return b;
}
static void tb_add(GtkWidget *fb, GtkWidget *w) { gtk_flow_box_insert(GTK_FLOW_BOX(fb), w, -1); }
static void tb_sep(GtkWidget *fb)
{
	GtkWidget *s = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_size_request(s, (int)lround(rui_px(1)), (int)lround(rui_px(20)));
	gtk_widget_set_margin_start(s, (int)lround(rui_px(3)));
	gtk_widget_set_margin_end(s, (int)lround(rui_px(3)));
	gtk_style_context_add_class(gtk_widget_get_style_context(s), "wp-sep");
	gtk_flow_box_insert(GTK_FLOW_BOX(fb), s, -1);
}
static GtkWidget *tb_off(GtkWidget *b) { gtk_widget_set_sensitive(b, FALSE); return b; }

static void build_menubar(void)
{
	MBAR = gtk_flow_box_new();
	gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(MBAR), GTK_SELECTION_NONE);
	gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(MBAR), (int)lround(rui_px(1)));
	gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(MBAR), 12);
	gtk_widget_set_margin_start(MBAR, (int)lround(rui_px(4)));
	gtk_widget_set_margin_top(MBAR, (int)lround(rui_px(2)));
	gtk_widget_set_margin_bottom(MBAR, (int)lround(rui_px(2)));
	gtk_style_context_add_class(gtk_widget_get_style_context(MBAR), "wp-mb");
	for (int i = 0; i < NMENUS; i++) {
		GtkWidget *b = gtk_button_new_with_label(MENUS[i].title);
		gtk_style_context_add_class(gtk_widget_get_style_context(b), "rui");
		gtk_style_context_add_class(gtk_widget_get_style_context(b), "wp-mb-b");
		gtk_widget_set_can_focus(b, FALSE);
		gtk_widget_set_valign(b, GTK_ALIGN_CENTER);
		g_signal_connect(b, "clicked", G_CALLBACK(mb_click), (gpointer)(intptr_t)i);
		gtk_flow_box_insert(GTK_FLOW_BOX(MBAR), b, -1);
		MBTNS[i] = b;
	}
	gtk_box_pack_start(GTK_BOX(W->body), MBAR, FALSE, FALSE, 0);
}
static void build_toolbar(void)
{
	TB = gtk_flow_box_new();
	gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(TB), GTK_SELECTION_NONE);
	gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(TB), (int)lround(rui_px(3)));
	gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(TB), (int)lround(rui_px(3)));
	gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(TB), 40);
	gtk_widget_set_margin_start(TB, (int)lround(rui_px(6)));
	gtk_widget_set_margin_end(TB, (int)lround(rui_px(6)));
	gtk_widget_set_margin_top(TB, (int)lround(rui_px(4)));
	gtk_widget_set_margin_bottom(TB, (int)lround(rui_px(4)));
	gtk_style_context_add_class(gtk_widget_get_style_context(TB), "wp-tb");
	tb_add(TB, tb_wide("<b>+</b> " , "New slide (Ctrl+M)", G_CALLBACK(on_new_btn)));
	{ /* (the page's Layout list) */
		GtkWidget *lw = gtk_combo_box_text_new();
		for (int i = 0; i < NPPLAYOUTS; i++) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(lw), rui_tr(PP_LAYOUTS[i].name));
		gtk_widget_set_tooltip_text(lw, rui_tr("Layout"));
		gtk_widget_set_size_request(lw, (int)lround(rui_px(132)), (int)lround(rui_px(26)));
		gtk_style_context_add_class(gtk_widget_get_style_context(lw), "wp-tb");
		g_signal_connect(lw, "changed", G_CALLBACK(on_layout_combo), NULL);
		LAYW = lw; tb_add(TB, lw); }
	tb_sep(TB);
	{
		GtkWidget *ff = gtk_combo_box_text_new();
		for (int i = 0; i < NPPFONTS; i++) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ff), PP_FONTS[i]);
		gtk_widget_set_tooltip_text(ff, rui_tr("Font"));
		gtk_widget_set_size_request(ff, (int)lround(rui_px(124)), (int)lround(rui_px(26)));
		gtk_style_context_add_class(gtk_widget_get_style_context(ff), "wp-tb");
		gtk_combo_box_set_active(GTK_COMBO_BOX(ff), 0);
		g_signal_connect(ff, "changed", G_CALLBACK(on_font), NULL);
		tb_add(TB, ff);
	}
	{
		GtkWidget *sz = gtk_combo_box_text_new();
		for (int i = 0; i < NPPSIZES; i++) {
			char b[8]; snprintf(b, sizeof b, "%d", PP_SIZES[i]);
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sz), b);
		}
		gtk_widget_set_tooltip_text(sz, rui_tr("Font size"));
		gtk_widget_set_size_request(sz, (int)lround(rui_px(54)), (int)lround(rui_px(26)));
		gtk_style_context_add_class(gtk_widget_get_style_context(sz), "wp-tb");
		g_signal_connect(sz, "changed", G_CALLBACK(on_fsize), NULL);
		SIZEW = sz; tb_add(TB, sz);
	}
	tb_sep(TB);
	BTN_B = tb_button("<b>B</b>", "Bold (Ctrl+B)", G_CALLBACK(on_style_btn), GINT_TO_POINTER(0)); tb_add(TB, BTN_B);
	BTN_I = tb_button("<i>I</i>", "Italic (Ctrl+I)", G_CALLBACK(on_style_btn), GINT_TO_POINTER(1)); tb_add(TB, BTN_I);
	BTN_U = tb_button("<u>U</u>", "Underline (Ctrl+U)", G_CALLBACK(on_style_btn), GINT_TO_POINTER(2)); tb_add(TB, BTN_U);
	BTN_S = tb_button("<s>S</s>", "Strikethrough", G_CALLBACK(on_style_btn), GINT_TO_POINTER(3)); tb_add(TB, BTN_S);
	{
		GdkRGBA c0 = { 0.06, 0.06, 0.06, 1.0 };
		GtkWidget *fc = gtk_color_button_new_with_rgba(&c0);
		gtk_widget_set_tooltip_text(fc, rui_tr("Text colour"));
		gtk_widget_set_size_request(fc, (int)lround(rui_px(26)), (int)lround(rui_px(26)));
		gtk_style_context_add_class(gtk_widget_get_style_context(fc), "wp-tb");
		g_signal_connect(fc, "color-set", G_CALLBACK(on_colour), NULL);
		tb_add(TB, fc);
	}
	tb_sep(TB);
	ALN[0] = tb_button("\xe2\x87\xa4", "Align left", G_CALLBACK(on_align_btn), GINT_TO_POINTER(1)); tb_add(TB, ALN[0]);
	ALN[1] = tb_button("\xe2\x86\x94", "Align centre", G_CALLBACK(on_align_btn), GINT_TO_POINTER(2)); tb_add(TB, ALN[1]);
	ALN[2] = tb_button("\xe2\x87\xa5", "Align right", G_CALLBACK(on_align_btn), GINT_TO_POINTER(3)); tb_add(TB, ALN[2]);
	BLB[0] = tb_button("\xe2\x80\xa2", "Bullets", G_CALLBACK(on_bullets_btn), NULL); tb_add(TB, BLB[0]);
	BLB[1] = tb_button("1.", "Numbering", G_CALLBACK(on_number_btn), NULL); tb_add(TB, BLB[1]);
	tb_sep(TB);
	tb_add(TB, tb_button("\xe2\x86\xb6", "Undo (Ctrl+Z)", G_CALLBACK(on_undo_btn), NULL));
	tb_add(TB, tb_button("\xe2\x86\xb7", "Redo (Ctrl+Y)", G_CALLBACK(on_redo_btn), NULL));
	tb_add(TB, tb_button("\xe2\x9c\x95", "Delete the slide", G_CALLBACK(on_del_btn), NULL));
	tb_add(TB, tb_button("\xe2\x9e\x95", "Duplicate the slide (Ctrl+D)", G_CALLBACK(on_dup_btn), NULL));
	tb_sep(TB);
	tb_add(TB, tb_off(tb_button("T", "Text box (a later step)", NULL, NULL)));
	{ /* (the page's first four shapes, then the rest) */
		for (int i = 0; i < 4 && i < NPPSHAPES; i++)
			tb_add(TB, tb_off(tb_button("\xe2\x97\xa6", PP_SHAPES[i].name, NULL, NULL)));
	}
	tb_add(TB, tb_off(tb_button("\xe2\x96\xa3", "Picture (a later step)", NULL, NULL)));
	tb_add(TB, tb_off(tb_button("T\xc3\xa4", "Table (a later step)", NULL, NULL)));
	tb_add(TB, tb_off(tb_button("\xe2\x96\xa4", "Chart (a later step)", NULL, NULL)));
	tb_add(TB, tb_off(tb_button("\xe2\x98\x85", "Icons (a later step)", NULL, NULL)));
	tb_sep(TB);
	tb_add(TB, tb_off(tb_button("\xe2\x86\x91", "Bring to front (a later step)", NULL, NULL)));
	tb_add(TB, tb_off(tb_button("\xe2\x86\x93", "Send to back (a later step)", NULL, NULL)));
	tb_sep(TB);
	tb_add(TB, tb_off(tb_button("\xe2\x96\xb6 " "\xe2\x80\x8bPresent", "Present (a later step)", NULL, NULL)));
	gtk_box_pack_start(GTK_BOX(W->body), TB, FALSE, FALSE, 0);
}
static void build_panes(void)
{
	GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request(sw, (int)lround(rui_px(LISTW)), -1);
	gtk_style_context_add_class(gtk_widget_get_style_context(sw), "pp-listbox");
	LIST = gtk_drawing_area_new();
	gtk_widget_set_size_request(LIST, (int)lround(rui_px(LISTW)), list_height());
	gtk_widget_add_events(LIST, GDK_BUTTON_PRESS_MASK);
	gtk_widget_set_can_focus(LIST, TRUE);
	g_signal_connect(LIST, "draw", G_CALLBACK(list_draw), NULL);
	g_signal_connect(LIST, "button-press-event", G_CALLBACK(on_list_click), NULL);
	gtk_container_add(GTK_CONTAINER(sw), LIST);
	gtk_box_pack_start(GTK_BOX(h), sw, FALSE, FALSE, 0);
	GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	OVL = gtk_overlay_new();
	STAGE = gtk_drawing_area_new();
	gtk_widget_set_hexpand(STAGE, TRUE);
	gtk_widget_set_vexpand(STAGE, TRUE);
	gtk_widget_add_events(STAGE, GDK_BUTTON_PRESS_MASK);
	gtk_widget_set_can_focus(STAGE, TRUE);
	g_signal_connect(STAGE, "draw", G_CALLBACK(stage_draw), NULL);
	g_signal_connect(STAGE, "button-press-event", G_CALLBACK(on_stage_click), NULL);
	gtk_container_add(GTK_CONTAINER(OVL), STAGE);
	{
		EDF = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(EDF), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		EDITV = gtk_text_view_new();
		BUF = gtk_text_view_get_buffer(GTK_TEXT_VIEW(EDITV));
		gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(EDITV), GTK_WRAP_WORD_CHAR);
		gtk_text_view_set_left_margin(GTK_TEXT_VIEW(EDITV), (int)lround(rui_px(2)));
		gtk_text_view_set_right_margin(GTK_TEXT_VIEW(EDITV), (int)lround(rui_px(2)));
		gtk_style_context_add_class(gtk_widget_get_style_context(EDITV), "pp-ed");
		g_signal_connect(EDITV, "key-press-event", G_CALLBACK(on_edit_key), NULL);
		g_signal_connect(EDITV, "focus-out-event", G_CALLBACK(on_edit_focus_out), NULL);
		gtk_container_add(GTK_CONTAINER(EDF), EDITV);
		gtk_overlay_add_overlay(GTK_OVERLAY(OVL), EDF);
		gtk_widget_set_halign(EDF, GTK_ALIGN_START);
		gtk_widget_set_valign(EDF, GTK_ALIGN_START);
		g_object_set_data(G_OBJECT(OVL), "pp-ed-sc", EDF);
	}
	gtk_box_pack_start(GTK_BOX(v), OVL, TRUE, TRUE, 0);
	{ /* (the page's .pp-nbar: the speaker's words, under the stage) */
		GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
		gtk_widget_set_size_request(bar, -1, (int)lround(rui_px(NOTES_H)));
		gtk_style_context_add_class(gtk_widget_get_style_context(bar), "pp-nbar");
		GtkWidget *sw2 = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw2), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		GtkWidget *NOTESV = gtk_text_view_new();
		NBUF = gtk_text_view_get_buffer(GTK_TEXT_VIEW(NOTESV));
		gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(NOTESV), GTK_WRAP_WORD_CHAR);
		gtk_text_view_set_left_margin(GTK_TEXT_VIEW(NOTESV), (int)lround(rui_px(10)));
		gtk_text_view_set_top_margin(GTK_TEXT_VIEW(NOTESV), (int)lround(rui_px(4)));
		g_signal_connect(NBUF, "changed", G_CALLBACK(on_notes_changed), NULL);
		gtk_container_add(GTK_CONTAINER(sw2), NOTESV);
		gtk_box_pack_start(GTK_BOX(bar), sw2, TRUE, TRUE, 0);
		g_object_set_data(G_OBJECT(W->win), "pp-notesbar", bar);
		gtk_box_pack_start(GTK_BOX(v), bar, FALSE, FALSE, 0);
	}
	gtk_box_pack_start(GTK_BOX(h), v, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(W->body), h, TRUE, TRUE, 0);
}
static void build_status(void)
{
	GtkWidget *sb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (int)lround(rui_px(10)));
	gtk_style_context_add_class(gtk_widget_get_style_context(sb), "wp-sb");
	gtk_widget_set_margin_start(sb, (int)lround(rui_px(8)));
	gtk_widget_set_margin_end(sb, (int)lround(rui_px(8)));
	gtk_widget_set_margin_top(sb, (int)lround(rui_px(4)));
	gtk_widget_set_margin_bottom(sb, (int)lround(rui_px(4)));
	STATUS = gtk_label_new("Slide 1 of 1");
	ZTXT = gtk_label_new(rui_tr("Fit"));
	ZOOMSL = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 25, 200, 25);
	gtk_scale_set_draw_value(GTK_SCALE(ZOOMSL), FALSE);
	gtk_widget_set_size_request(ZOOMSL, (int)lround(rui_px(110)), -1);
	gtk_widget_set_can_focus(ZOOMSL, FALSE);
	gtk_widget_set_tooltip_text(ZOOMSL, rui_tr("Zoom"));
	g_signal_connect(ZOOMSL, "value-changed", G_CALLBACK(on_zoom), NULL);
	GtkWidget *sp = gtk_label_new("");
	gtk_widget_set_hexpand(sp, TRUE);
	gtk_box_pack_start(GTK_BOX(sb), STATUS, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(sb), sp, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(sb), ZTXT, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(sb), ZOOMSL, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(sb), tb_button("\xe2\x86\x94", "Fit the slide to the window", G_CALLBACK(on_fit_btn), NULL), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(W->body), sb, FALSE, FALSE, 0);
}

/* ------------------------------------------------------------------ the state the check reads */
static void flat(char *dst, size_t n, const char *src)
{
	size_t j = 0;
	for (size_t i = 0; src && src[i] && j + 1 < n; i++) dst[j++] = src[i] == '\n' ? '|' : src[i];
	dst[j] = 0;
}
static void state_write(void)
{
	const char *f = getenv("RUDOLF_PP_STATE");
	if (!f) return;
	GString *s = g_string_new(NULL);
	char title[96] = "";
	if (W && W->win) {
		const char *t = gtk_window_get_title(GTK_WINDOW(W->win));
		if (t) snprintf(title, sizeof title, "%s", t);
	}
	g_string_append_printf(s, "title %s\nmenus %d\n", title, menus_n());
	if (MENU_OPEN) g_string_append_printf(s, "menu %s %d\n", MENU_OPEN, MENU_OPEN_N);
	g_string_append_printf(s, "lists fonts %d sizes %d layouts %d themes %d shapes %d\n", NPPFONTS, NPPSIZES, NPPLAYOUTS,
	                       NPPTHEMES, NPPSHAPES);
	g_string_append_printf(s, "slides %d\ncur %d\nlayout %s\nhidden %d\ntheme %s\nsize %dx%d\n", SLN, CUR, SL[CUR].layout,
	                       SL[CUR].hidden, PP_THEMES[TH].name, (int)SLW, (int)SLH);
	for (int i = 0; i < PH_N; i++) {
		Ph *p = &SL[CUR].ph[i];
		if (p->t && *p->t) {
			char t[2048]; flat(t, sizeof t, p->t);
			g_string_append_printf(s, "text %s %s\n", PH_KEY[i], t);
		}
		if (p->b || p->i || p->u || p->s || p->ha > 0 || p->bl >= 0 || p->fs > 0 || p->fc || p->font)
			g_string_append_printf(s, "fmt %s b%u i%u u%u s%u ha%d bl%d fs%d font%s colour%s\n", PH_KEY[i], p->b, p->i, p->u,
			                       p->s, p->ha, p->bl, p->fs, p->font ? p->font : "-", p->fc ? p->fc : "-");
	}
	{
		char t[2048]; flat(t, sizeof t, SL[CUR].notes ? SL[CUR].notes : "");
		g_string_append_printf(s, "notes %s\n", t);
	}
	for (int i = 0; i < SLN; i++) {
		char t[512]; flat(t, sizeof t, SL[i].ph[PH_TITLE].t ? SL[i].ph[PH_TITLE].t : "");
		g_string_append_printf(s, "slide %d %s %s%s\n", i, SL[i].layout, t, SL[i].hidden ? " hidden" : "");
	}
	{
		char st[160];
		snprintf(st, sizeof st, "%s %d %s %d", rui_tr("Slide"), CUR + 1, rui_tr("of"), SLN);
		g_string_append_printf(s, "status %s%s\n", st, SL[CUR].hidden ? " (hidden)" : "");
	}
	{
		char z[16];
		if (ZOOMF) snprintf(z, sizeof z, "Fit"); else snprintf(z, sizeof z, "%d%%", ZOOM);
		g_string_append_printf(s, "zoom %s\nhist %d %d\n", z, HSI, HSN);
	}
	char *tmp = g_strdup_printf("%s.t", f);
	g_file_set_contents(tmp, s->str, -1, NULL);
	rename(tmp, f);
	g_free(tmp);
	g_string_free(s, TRUE);
}
static gboolean state_tick(gpointer d) { (void)d; state_write(); return G_SOURCE_REMOVE; }
static void state_soon(void) { if (getenv("RUDOLF_PP_STATE")) g_timeout_add(120, state_tick, NULL); }

/* ------------------------------------------------------------------ the look: the page's own values for what GTK does
   not draw as it (base_v24.html's .wp-tb .wp-mb .wp-sep .wp-sb, styles25.css's .pp-stagebox .pp-list .pp-nbar) */
static void css(void)
{
	static const char *T =
		".wp-tb{background:#f9fafb;border-bottom:1px solid #d1d5db}"
		"button.wp-tb{min-width:26px;padding:0 5px;border:1px solid transparent;background:transparent;border-radius:4px;font-size:13px;color:#111}"
		"button.wp-tb:hover{background:#e0e7ff;border-color:#c7d2fe}"
		"button.wp-tb:disabled{color:#9ca3af}"
		"button.wp-on{background:#c7d2fe;border-color:#818cf8}"
		"combobox.wp-tb,combobox.wp-tb button{border:1px solid #d1d5db;border-radius:4px;background:#fff;font-size:12px;color:#111}"
		".wp-mb{background:#f3f4f6;border-bottom:1px solid #d1d5db}"
		"button.wp-mb-b{border:1px solid transparent;background:transparent;border-radius:4px;font-size:13px;color:#111;padding:0 9px;min-height:0;min-width:0}"
		"button.wp-mb-b:hover{background:#dbe6ff}"
		".wp-sep{background:#d1d5db}"
		".wp-sb{background:#f9fafb;border-top:1px solid #d1d5db;font-size:12px;color:#4b5563}"
		".pp-listbox{background:#f3f4f6;border-right:1px solid #e5e7eb}"
		".pp-nbar{background:#f9fafb;border-top:1px solid #d1d5db;font-size:13px;color:#374151}"
		"textview.pp-ed{background:#ffffff;border:2px solid #2563eb;color:#111}";
	GtkCssProvider *p = gtk_css_provider_new();
	gtk_css_provider_load_from_data(p, T, -1, NULL);
	gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(p),
	                                          GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref(p);
}

/* ------------------------------------------------------------------ the tests' own ways in */
static gboolean goto_hook(gpointer d)
{
	int n = atoi((const char *)d);
	goto_slide(n - 1);
	gtk_widget_grab_focus(STAGE);
	return G_SOURCE_REMOVE;
}
static gboolean text_hook(gpointer d)
{
	/* ("slot=words": what a person types, without the mouse's guesses about where a place is) */
	char *a = g_strdup((const char *)d);
	char *eq = strchr(a, '=');
	if (eq) {
		*eq = 0;
		for (int i = 0; i < PH_N; i++)
			if (!strcmp(a, PH_KEY[i])) { set_text(i, eq + 1); goto_slide(CUR); break; }
	}
	g_free(a);
	return G_SOURCE_REMOVE;
}
static void do_one(const char *c)
{
	if (!strcmp(c, "new")) new_slide();
	else if (!strcmp(c, "dup")) dup_slide();
	else if (!strcmp(c, "del")) del_slide();
	else if (!strcmp(c, "hide")) { SL[CUR].hidden = !SL[CUR].hidden; slide_changed(); }
	else if (!strcmp(c, "bold")) toggle_style(0);
	else if (!strcmp(c, "ital")) toggle_style(1);
	else if (!strcmp(c, "under")) toggle_style(2);
	else if (!strcmp(c, "bullets")) set_list(1);
	else if (!strcmp(c, "numbering")) set_list(2);
	else if (!strcmp(c, "undo")) hist_undo();
	else if (!strcmp(c, "redo")) hist_redo();
	else if (!strcmp(c, "fit")) { ZOOMF = 1; redraw(); status_update(); }
	else if (!strcmp(c, "tab")) next_ph(1);
	else if (!strncmp(c, "goto:", 5)) goto_slide(atoi(c + 5) - 1);
	else if (!strncmp(c, "layout:", 7)) set_layout(c + 7);
	else if (!strncmp(c, "align:", 6)) set_align(c[6] == 'l' ? 1 : c[6] == 'c' ? 2 : 3);
	else if (!strncmp(c, "text:", 5)) {
		char *a = g_strdup(c + 5), *eq = strchr(a, '=');
		if (eq) {
			*eq = 0;
			for (int i = 0; i < PH_N; i++) if (!strcmp(a, PH_KEY[i])) { set_text(i, eq + 1); break; }
		}
		g_free(a);
	} else if (!strncmp(c, "notes:", 6) || !strncmp(c, "notes=", 6)) {
		g_free(SL[CUR].notes);
		SL[CUR].notes = c[6] ? g_strdup(c + 6) : NULL;
		notes_sync();
		state_soon();
	}
}
static gboolean do_hook(gpointer d)
{
	char *a = g_strdup((const char *)d), *save = NULL;
	for (char *c = strtok_r(a, ";", &save); c; c = strtok_r(NULL, ";", &save)) do_one(c);
	g_free(a);
	goto_slide(CUR);
	return G_SOURCE_REMOVE;
}

int main(int argc, char **argv)
{
	rui_init(&argc, &argv, "presentpro");
	css();
	W = rui_window("presentpro", "PresentPro", 1060, 700);
	deck_ensure(1);
	SL[0] = slide_new();
	SLN = 1; CUR = 0; PHCUR = PH_TITLE;
	build_menubar(); build_toolbar(); build_panes(); build_status();
	gtk_window_set_title(GTK_WINDOW(W->win), "Untitled presentation - PresentPro");
	g_signal_connect(W->win, "key-press-event", G_CALLBACK(on_key), NULL);
	gtk_widget_show_all(W->win);
	gtk_widget_hide(EDF);
	READY = 1;
	goto_slide(0);
	hist_push();
	status_update();
	gtk_widget_grab_focus(STAGE);
	{
		const char *mo = getenv("RUDOLF_PP_MENU");
		if (mo && *mo) for (int i = 0; i < NMENUS; i++) if (!strcmp(MENUS[i].title, mo)) { g_timeout_add(400, menu_hook, (gpointer)(intptr_t)i); break; }
	}
	{ const char *go = getenv("RUDOLF_PP_GOTO"); if (go && *go) g_timeout_add(600, goto_hook, (gpointer)go); }
	{ const char *tx = getenv("RUDOLF_PP_TEXT"); if (tx && *tx) g_timeout_add(900, text_hook, (gpointer)tx); }
	{ const char *dd = getenv("RUDOLF_PP_DO"); if (dd && *dd) g_timeout_add(1200, do_hook, (gpointer)dd); }
	state_soon();
	gtk_main();
	return 0;
}
