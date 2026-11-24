/*
 * rudolf-settings - Settings, native (1.6.2, docs/plan-1.6.2.md Part 39; docs/plan-2.0.md wave 3), with librudolf-ui.
 * The web Settings (desktop/settings25.js's window, setsearch59.js's Search in Settings, and each page with the modules that add to
 * it) as a program of its own: the same window (the search box above the sidebar of the 22 pages and the page), each page's rows,
 * controls, buttons, dialogs and administrator's prompts, the values saved as the page saves them (its settings in the page's
 * storage, bd95set:ACCT, through rudolfd's copy: lsync) and the same rudolfd requests, drawn in the page's look.
 *   rudolf-settings [PAGE]          (PAGE: disp, vol, ... about: the page it opens on, as the page's settings(PAGE))
 * Built with librudolf-ui (iso/sys/ui/build.sh WORK OUT settings: settings.c, settings_PAGE.c, mksettings.py's header from the page).
 * Pages done so far (one pull request each group): System (Date, time and language; Desktop engine; Idle windows), About. Until every
 * page is done the native desktop keeps opening the web Settings (docs/plan-1.6.2.md Part 39: never a window half native, half web);
 * a page not done yet says so here. Its test: iso/qt/native/settings_t.sh.
 * Tests' hooks: RUDOLF_SET_STATE=FILE (what it shows, for the checklist), RUDOLF_SET_NOW=SECONDS (the page's clock, held: the test's
 * Chromium clock), RUDOLF_SET_GPU=TEXT (About's Graphics as the page's WebGL names it: the two engines name a GPU differently).
 * GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <dlfcn.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "settings.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
#include "settings-gen.h"
#pragma GCC diagnostic pop

RuiWin *SW; char ACCT[80], ME[64], ROLE[16];
static const SetLook *L_(void) { return &LOOK[RUI.dark ? 1 : 0]; }
#define LK (L_())
static double Z(void) { return RUI.z; }
static int R(double v) { return (int)lround(v * RUI.z); }

/* ------------------------------------------------------------------ the account, the clock, the engine */
int set_admin(void) { return !strcmp(ROLE, "admin"); }
double set_now(void) { const char *t = getenv("RUDOLF_SET_NOW"); if (t && *t) { static double t0 = -1, base; if (t0 < 0) { t0 = g_get_monotonic_time() / 1e6; base = atof(t); } (void)t0; return base; } return g_get_real_time() / 1e6; }
static char ENGINE[16];
const char *set_engine_now(void)
{
	if (!ENGINE[0]) { RuiJ *e = rui_get("/api/desktop/engine"); const char *n = rui_jstr(e, "now"); snprintf(ENGINE, sizeof ENGINE, "%s", n ? n : "native"); rui_jfree(e); }
	return ENGINE;
}

/* ------------------------------------------------------------------ the page's storage (rudolfd's copy, lsync) */
const char *set_jstr(RuiJ *o, const char *k, const char *d) { const char *v = rui_jstr(o, k); return v ? v : d; }
static RuiJ *ls_get(const char *k)
{
	char *e = g_uri_escape_string(k, NULL, TRUE), p[300]; snprintf(p, sizeof p, "/api/lsync?key=%s", e); g_free(e);
	RuiJ *r = rui_get(p); const char *v = rui_jstr(r, "v"); RuiJ *o = v ? rui_jparse(v) : NULL; rui_jfree(r); return o;
}
RuiJ *set_stored(void)
{
	char k[120]; snprintf(k, sizeof k, "bd95set:%s", ACCT); RuiJ *o = ACCT[0] ? ls_get(k) : NULL; if (!o) o = ls_get("bd95set");
	return o ? o : rui_jparse("{}");
}
static void jesc(GString *b, const char *s) { for (; *s; s++) { unsigned char c = *s; if (c == '"' || c == '\\') g_string_append_printf(b, "\\%c", c); else if (c < 32) g_string_append_printf(b, "\\u%04x", c); else g_string_append_c(b, c); } }
/* a value of the stored object written back as JSON (rudolf-native's reader keeps numbers, strings, booleans, arrays, objects) */
static void jwrite(GString *b, RuiJ *v)
{
	int t = rui_jtype(v);
	if (t == 0 || t < 0) g_string_append(b, "null");
	else if (t == 1) g_string_append(b, rui_jval(v) ? "true" : "false");
	else if (t == 2) { char n[40]; double x = rui_jval(v); if (x == floor(x) && fabs(x) < 1e15) snprintf(n, sizeof n, "%.0f", x); else g_ascii_formatd(n, sizeof n, "%.17g", x); g_string_append(b, n); }
	else if (t == 3) { g_string_append_c(b, '"'); jesc(b, rui_jsval(v)); g_string_append_c(b, '"'); }
	else if (t == 4) { g_string_append_c(b, '['); for (int i = 0; i < rui_jlen(v); i++) { if (i) g_string_append_c(b, ','); jwrite(b, rui_jat(v, i)); } g_string_append_c(b, ']'); }
	else { g_string_append_c(b, '{'); for (int i = 0; i < rui_jlen(v); i++) { const char *k = rui_jkey(v, i); if (i) g_string_append_c(b, ','); g_string_append_c(b, '"'); jesc(b, k); g_string_append(b, "\":"); jwrite(b, rui_jprop(v, k)); } g_string_append_c(b, '}'); }
}
int set_store(const char *const *keys, const char *const *vals, int n)
{
	/* (persistS: the whole settings object, the keys changed, under bd95set and bd95set:ACCT, as one part of rudolfd's copy) */
	RuiJ *o = set_stored(); GString *s = g_string_new("{");
	int first = 1;
	for (int i = 0; i < rui_jlen(o); i++) { const char *k = rui_jkey(o, i); int ours = 0; for (int j = 0; j < n; j++) if (!strcmp(keys[j], k)) ours = 1; if (ours) continue;
		if (!first) g_string_append_c(s, ','); first = 0; g_string_append_c(s, '"'); jesc(s, k); g_string_append(s, "\":"); jwrite(s, rui_jprop(o, k)); }
	for (int j = 0; j < n; j++) { if (!vals[j]) continue; if (!first) g_string_append_c(s, ','); first = 0; g_string_append_c(s, '"'); jesc(s, keys[j]); g_string_append(s, "\":"); g_string_append(s, vals[j]); }
	g_string_append_c(s, '}'); rui_jfree(o);
	GString *b = g_string_new("{\"part\":1,\"set\":{\"bd95set\":\""); jesc(b, s->str); g_string_append(b, "\"");
	if (ACCT[0]) { g_string_append(b, ",\"bd95set:"); jesc(b, ACCT); g_string_append(b, "\":\""); jesc(b, s->str); g_string_append(b, "\""); }
	g_string_append(b, "}}"); int c = rui_post("/api/lsync", b->str, NULL, 0); set_note("store %d %s", c, s->str); g_string_free(b, TRUE); g_string_free(s, TRUE); return c;
}

/* ------------------------------------------------------------------ drawing in the page's look */
static void setc(cairo_t *cr, SetC c, double a)
{
	if (c.a < 0) { rui_color(cr, "ac", (c.a < -1.0001 ? -c.a - 1 : 1) * a); return; }   /* (the accent colour, at a share when a < -1) */
	cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a * a);
}
static void fgc(cairo_t *cr, double a) { if (RUI.dark) setc(cr, LK->sg_fg, a); else rui_color(cr, "fg", a); }   /* (the page's writing: Display Properties' colour; html.dark .sg's) */
static void rrect(cairo_t *cr, double x, double y, double w, double h, double r)
{
	r = fmin(r, fmin(w, h) / 2); cairo_new_sub_path(cr);
	cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0); cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
	cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI); cairo_arc(cr, x + r, y + r, r, M_PI, 1.5 * M_PI); cairo_close_path(cr);
}
/* the page's line box for a font size: CSS's "normal" line height as Chromium takes it from the font (ascent and descent, each rounded) */
static double asc_of(double px, int wt, double *desc)
{
	static PangoContext *c; if (!c) c = pango_font_map_create_context(pango_cairo_font_map_get_default());
	{ const cairo_font_options_t *so = gdk_screen_get_font_options(gdk_screen_get_default()); if (so) pango_cairo_context_set_font_options(c, so); }
	PangoFontDescription *f = pango_font_description_from_string(RUI.family); pango_font_description_set_absolute_size(f, px * RUI.z * PANGO_SCALE); pango_font_description_set_weight(f, wt);
	PangoFontMetrics *m = pango_context_get_metrics(c, f, NULL); double a = round(pango_font_metrics_get_ascent(m) / (double)PANGO_SCALE), d = round(pango_font_metrics_get_descent(m) / (double)PANGO_SCALE);
	pango_font_metrics_unref(m); pango_font_description_free(f); if (desc) *desc = d / RUI.z; return a / RUI.z;
}
static double lh_of(double px, int wt) { double d, a = asc_of(px, wt, &d); return a + d; }
/* a text laid out as the page lays it out: its font, wrapped at w CSS px (0: one line), letter-spacing ls (em) */
static PangoLayout *lay(PangoContext *pc, const char *t, double px, int wt, double w, double ls)
{
	PangoLayout *l = pango_layout_new(pc); PangoFontDescription *f = pango_font_description_from_string(RUI.family);
	pango_font_description_set_absolute_size(f, px * RUI.z * PANGO_SCALE); pango_font_description_set_weight(f, wt); pango_layout_set_font_description(l, f); pango_font_description_free(f);
	/* (<kbd>: the text between \x01 and \x02, a box of its own in the line: 11px DejaVu Sans Mono, padding 0 4px, a 1px border, 2px
	   at the bottom, as the page's kbd) */
	if (t && strchr(t, '\x01')) { GString *o = g_string_new(NULL); PangoAttrList *al = pango_attr_list_new();
		for (const char *p = t; *p;) { if (*p == '\x01') { const char *e = strchr(p, '\x02'); if (!e) e = p + strlen(p); guint st = o->len; g_string_append_len(o, p + 1, e - p - 1);
				PangoAttribute *f = pango_attr_family_new("DejaVu Sans Mono"); f->start_index = st; f->end_index = o->len; pango_attr_list_insert(al, f);
				PangoAttribute *z = pango_attr_size_new_absolute((int)(11 * RUI.z * PANGO_SCALE)); z->start_index = st; z->end_index = o->len; pango_attr_list_insert(al, z);
				PangoAttribute *mk = pango_attr_background_alpha_new(1); mk->start_index = st; mk->end_index = o->len; pango_attr_list_insert(al, mk);   /* (the run marked: its box drawn by text_draw) */
				/* (the box's padding and border: room at both ends, as letter spacing on the spaces around it) */
				if (st > 0 && o->str[st - 1] == ' ') { PangoAttribute *ls1 = pango_attr_letter_spacing_new((int)(4 * RUI.z * PANGO_SCALE)); ls1->start_index = st - 1; ls1->end_index = st; pango_attr_list_insert(al, ls1); }
				if (e[0] && e[1] == ' ') { PangoAttribute *ls2 = pango_attr_letter_spacing_new((int)(4 * RUI.z * PANGO_SCALE)); ls2->start_index = o->len; ls2->end_index = o->len + 1; pango_attr_list_insert(al, ls2); }
				p = *e ? e + 1 : e; continue; }
			g_string_append_c(o, *p++); }
		pango_layout_set_text(l, o->str, -1); pango_layout_set_attributes(l, al); pango_attr_list_unref(al); g_string_free(o, TRUE); }
	else if (t && t[0] == '\x03') pango_layout_set_markup(l, t + 1, -1);   /* (\x03: the rest is Pango's markup: the page's <b>, <i>) */
	else pango_layout_set_text(l, t ? t : "", -1);
	if (ls) { PangoAttrList *al = pango_attr_list_new(); pango_attr_list_insert(al, pango_attr_letter_spacing_new((int)lround(ls * px * RUI.z * PANGO_SCALE))); pango_layout_set_attributes(l, al); pango_attr_list_unref(al); }
	if (w > 0) { pango_layout_set_width(l, (int)(w * RUI.z * PANGO_SCALE)); pango_layout_set_wrap(l, PANGO_WRAP_WORD_CHAR); }
	return l;
}
static PangoContext *PC;
static PangoContext *pctx(void)
{
	if (!PC) { PC = pango_font_map_create_context(pango_cairo_font_map_get_default()); }
	const cairo_font_options_t *so = gdk_screen_get_font_options(gdk_screen_get_default()); if (so) pango_cairo_context_set_font_options(PC, so);
	pango_context_set_round_glyph_positions(PC, FALSE); return PC;
}
/* lines of a text and its width (CSS px), as wrapped at w */
static int text_lines(const char *t, double px, int wt, double w, double ls, double *tw)
{
	PangoLayout *l = lay(pctx(), t, px, wt, w, ls); int n = pango_layout_get_line_count(l); int pw, ph; pango_layout_get_size(l, &pw, &ph); if (tw) *tw = pw / (double)PANGO_SCALE / RUI.z; g_object_unref(l); return n;
}
/* writing drawn as Chromium draws a block of it: each line in a line box of lh, its baseline at the font's (rounded) ascent */
static void text_draw(cairo_t *cr, const char *t, double x, double y, double px, int wt, double w, double ls, double lh)
{
	PangoLayout *l = lay(pctx(), t, px, wt, w, ls);
	double d, a = asc_of(px, wt, &d); double half = (lh - (a + d)) / 2;
	PangoLayoutIter *it = pango_layout_get_iter(l); int i = 0;
	do { PangoLayoutLine *ln = pango_layout_iter_get_line_readonly(it);
		cairo_move_to(cr, round(x * RUI.z), round((y + i * lh + half + a) * RUI.z)); pango_cairo_show_layout_line(cr, ln); i++; } while (pango_layout_iter_next_line(it));
	pango_layout_iter_free(it);
	/* (each <kbd>'s box, around its run: drawn under its writing, so the run drawn again over it) */
	if (t && strchr(t, '\x01')) { PangoAttrIterator *ai = pango_attr_list_get_iterator(pango_layout_get_attributes(l)); cairo_pattern_t *ink = cairo_get_source(cr); cairo_pattern_reference(ink);
		do { PangoAttribute *mk = pango_attr_iterator_get(ai, PANGO_ATTR_BACKGROUND_ALPHA); if (!mk) continue; PangoRectangle p0, p1; pango_layout_index_to_pos(l, mk->start_index, &p0); pango_layout_index_to_pos(l, g_utf8_prev_char(pango_layout_get_text(l) + mk->end_index) - pango_layout_get_text(l), &p1);
			int line; pango_layout_index_to_line_x(l, mk->start_index, 0, &line, NULL);
			PangoFontDescription *kf = pango_font_description_from_string("DejaVu Sans Mono"); pango_font_description_set_absolute_size(kf, 11 * RUI.z * PANGO_SCALE);
			PangoFontMetrics *m = pango_context_get_metrics(pctx(), kf, NULL); double ka = round(pango_font_metrics_get_ascent(m) / (double)PANGO_SCALE) / RUI.z, kd = round(pango_font_metrics_get_descent(m) / (double)PANGO_SCALE) / RUI.z; pango_font_metrics_unref(m);
			double base = y + line * lh + half + a, bx = x + p0.x / (double)PANGO_SCALE / RUI.z - 5, bw = (p1.x + p1.width - p0.x) / (double)PANGO_SCALE / RUI.z + 10, by = base - ka - 1, bh = ka + kd + 3;
			char *run = g_strndup(pango_layout_get_text(l) + mk->start_index, mk->end_index - mk->start_index);
			cairo_save(cr); cairo_scale(cr, RUI.z, RUI.z); rui_color(cr, "bg2", 1); rrect(cr, bx, by, bw, bh, 4); cairo_fill(cr); rui_color(cr, "bd", 1); cairo_set_line_width(cr, 1); rrect(cr, bx + .5, by + .5, bw - 1, bh - 1, 3.5); cairo_stroke(cr); cairo_rectangle(cr, bx + 3, by + bh - 2, bw - 6, 1); cairo_fill(cr); cairo_restore(cr);
			PangoLayout *kl = pango_layout_new(pctx()); pango_layout_set_font_description(kl, kf); pango_layout_set_text(kl, run, -1); cairo_set_source(cr, ink); cairo_move_to(cr, round((bx + 5) * RUI.z), round(base * RUI.z)); pango_cairo_show_layout_line(cr, pango_layout_get_line_readonly(kl, 0));
			g_object_unref(kl); pango_font_description_free(kf); g_free(run); break;
		} while (pango_attr_iterator_next(ai));
		pango_attr_iterator_destroy(ai); cairo_pattern_destroy(ink); }
	g_object_unref(l);
}

/* ------------------------------------------------------------------ the tests' state (RUDOLF_SET_STATE) */
static GString *NOTES; static GHashTable *NAMED;
void set_note(const char *fmt, ...) { if (!NOTES) NOTES = g_string_new(NULL); va_list a; va_start(a, fmt); char b[600]; vsnprintf(b, sizeof b, fmt, a); va_end(a); g_string_append_printf(NOTES, "act %s\n", b); if (NOTES->len > 20000) g_string_erase(NOTES, 0, NOTES->len - 15000); set_state_soon(); }
/* (a name forgotten when its widget goes, so the state never reads a widget destroyed: a dialog's, say) */
static void named_gone(GtkWidget *w, gpointer n) { if (NAMED && g_hash_table_lookup(NAMED, n) == w) g_hash_table_remove(NAMED, n); }
void p_label_named(const char *n, GtkWidget *w)
{
	if (!NAMED) NAMED = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL); g_hash_table_replace(NAMED, g_strdup(n), w);
	if (w) g_signal_connect_data(w, "destroy", G_CALLBACK(named_gone), g_strdup(n), (GClosureNotify)(void (*)(void))g_free, 0);
}
static void rect_line(GString *s, const char *n, GtkWidget *w)
{
	if (!w || !gtk_widget_get_mapped(w)) return; int x, y, ox, oy; gdk_window_get_origin(gtk_widget_get_window(SW->win), &ox, &oy);
	gtk_widget_translate_coordinates(w, SW->win, 0, 0, &x, &y); g_string_append_printf(s, "rect %s %d %d %d %d\n", n, ox + x, oy + y, gtk_widget_get_allocated_width(w), gtk_widget_get_allocated_height(w));
}
static void state_more(GString *s);
static gboolean state_write(gpointer d)
{
	(void)d; const char *f = getenv("RUDOLF_SET_STATE"); if (!f || !SW) return G_SOURCE_REMOVE;
	GString *s = g_string_new(NULL); state_more(s);
	if (NAMED) { GHashTableIter it; gpointer k, v; g_hash_table_iter_init(&it, NAMED); while (g_hash_table_iter_next(&it, &k, &v)) { rect_line(s, k, v); if (g_object_get_data(G_OBJECT(v), "set-sel")) g_string_append_printf(s, "sel %s %s\n", (char *)k, s_select_get(v)); } }
	GtkWidget *m = rui_menu_window(); if (m && gtk_widget_get_mapped(m)) { int x, y; gdk_window_get_origin(gtk_widget_get_window(m), &x, &y); g_string_append_printf(s, "menu %d %d %d %d\n", x, y, gtk_widget_get_allocated_width(m), gtk_widget_get_allocated_height(m)); }
	GtkWidget *dl = rui_dialog_box(); if (dl) rect_line(s, "dialog", dl);
	if (NOTES) g_string_append(s, NOTES->str);
	char *t = g_strdup_printf("%s.t", f); g_file_set_contents(t, s->str, -1, NULL); rename(t, f); g_free(t); g_string_free(s, TRUE); return G_SOURCE_REMOVE;
}
static guint STATE_ID;
static gboolean state_go(gpointer d) { STATE_ID = 0; return state_write(d); }
void set_state_soon(void) { if (getenv("RUDOLF_SET_STATE") && !STATE_ID) STATE_ID = g_timeout_add(150, state_go, NULL); }
void set_notify(const char *title, const char *body, int error) { rui_notify(title, body, error); set_note("notify %s | %s", title, body ? body : ""); }

/* ================================================================== the page: blocks laid out as the page's CSS lays them out */
enum { I_H3, I_H4, I_SGRP, I_MSG, I_R2, I_BTNS, I_BLOCK, I_SPACE, I_CUSTOM, I_CHECK };
typedef struct { int k; GtkWidget *w; double gap, dy, mt, mb; int full; char *text; int grey, weight; double x, y, wd, h; GtkWidget *bt[8]; int nb; } CItem;   /* k: 0 a control, 1 a line break, 2 .msg, 3 writing, 4 label.ck, 5 .rmrow, 6 pre */
struct Cell { char *label; GPtrArray *items; int more; double lx, ly, lw, lh, y, h; };
typedef struct { int k; char *text; int grey; double mt, mb, x, y, w, h; GtkWidget *wg; Cell *cell; GtkWidget *btn[8]; int nb; double minh; double (*lay)(void *d, double x, double y, double w); void *ld; } Item;
static GPtrArray *ITEMS; static GtkWidget *FX, *SCR, *FOOT, *FOOTST, *FOOTB[3], *AREA, *OUTER, *SIDE, *SBAR, *SQ, *DD; static int (*FDIRTY)(void); static SetFoot FACT; static double FX_W = 587;
static char CUR[16];
static void item_free(gpointer p) { Item *i = p; g_free(i->text); if (i->cell) { g_free(i->cell->label); for (guint k = 0; k < i->cell->items->len; k++) g_free(((CItem *)g_ptr_array_index(i->cell->items, k))->text); g_ptr_array_unref(i->cell->items); g_free(i->cell); } g_free(i); }
static Item *add_item(int k, const char *t, double mt, double mb) { Item *i = g_new0(Item, 1); i->k = k; i->text = g_strdup(t ? rui_tr(t) : NULL); i->mt = mt; i->mb = mb; g_ptr_array_add(ITEMS, i); return i; }
void p_h3(const char *t) { add_item(I_H3, t, LK->h3_m[0], LK->h3_m[2]); }
void p_h4(const char *t) { add_item(I_H4, t, 16, 8); }   /* (its words as the page has them; drawn in capitals: text-transform) */
void p_sgrp(const char *t) { add_item(I_SGRP, t, SGRP_MT, 8); }
static GtkWidget *placeholder(void) { GtkWidget *w = gtk_drawing_area_new(); gtk_widget_set_size_request(w, 1, 1); return w; }
GtkWidget *p_msg(const char *t, int grey) { Item *i = add_item(I_MSG, t, LK->msg_mt[0], 0); i->grey = grey; i->wg = placeholder(); gtk_fixed_put(GTK_FIXED(FX), i->wg, 0, 0); g_object_set_data(G_OBJECT(i->wg), "set-item", i); return i->wg; }
Cell *p_r2(const char *label) { Item *i = add_item(I_R2, NULL, 0, LK->r2_mb[0]); i->cell = g_new0(Cell, 1); i->cell->label = g_strdup(label ? rui_tr(label) : ""); i->cell->items = g_ptr_array_new(); return i->cell; }
void p_r2_more(Cell *c) { c->more = 1; }
static CItem *citem(Cell *c, int k) { CItem *x = g_new0(CItem, 1); x->k = k; g_ptr_array_add(c->items, x); return x; }
void c_add(Cell *c, GtkWidget *w, double gap, double dy, int full) { CItem *x = citem(c, 0); x->w = w; x->gap = gap; x->dy = dy; x->full = full; gtk_fixed_put(GTK_FIXED(FX), w, 0, 0); }
void c_line(Cell *c, double mt) { CItem *x = citem(c, 1); x->mt = mt; }
GtkWidget *c_msg(Cell *c, const char *t, int grey) { CItem *x = citem(c, 2); x->text = g_strdup(rui_tr(t)); x->grey = grey; x->mt = LK->msg_mt[0]; x->w = placeholder(); gtk_fixed_put(GTK_FIXED(FX), x->w, 0, 0); g_object_set_data(G_OBJECT(x->w), "set-citem", x); return x->w; }
GtkWidget *c_text(Cell *c, const char *t, int wt) { CItem *x = citem(c, 3); x->text = g_strdup(t); x->weight = wt; x->w = placeholder(); gtk_fixed_put(GTK_FIXED(FX), x->w, 0, 0); g_object_set_data(G_OBJECT(x->w), "set-citem", x); return x->w; }
GtkWidget *c_msg_m(Cell *c, const char *t, int grey, double mt, double mb) { GtkWidget *w = c_msg(c, t, grey); CItem *x = g_object_get_data(G_OBJECT(w), "set-citem"); x->mt = mt; x->mb = mb; return w; }
GtkWidget *c_pre(Cell *c, const char *t) { CItem *x = citem(c, 6); x->text = g_strdup(t); x->w = placeholder(); gtk_fixed_put(GTK_FIXED(FX), x->w, 0, 0); g_object_set_data(G_OBJECT(x->w), "set-citem", x); return x->w; }
GtkWidget *c_buttons(Cell *c, const char *const *labels, int n, GCallback *cbs, void *d)
{
	CItem *x = citem(c, 5); x->mt = LK->rmrow_mt[0]; x->nb = MIN(n, 8);
	for (int k = 0; k < x->nb; k++) { x->bt[k] = rui_button(labels[k], 0); if (cbs && cbs[k]) g_signal_connect(x->bt[k], "clicked", cbs[k], d); gtk_fixed_put(GTK_FIXED(FX), x->bt[k], 0, 0); }
	return x->bt[0];
}
GtkWidget *c_check(Cell *c, const char *t, int on, void (*cb)(int, void *), void *d) { CItem *x = citem(c, 4); x->w = rui_check(t, on, cb, d); x->text = g_strdup(rui_tr(t)); gtk_fixed_put(GTK_FIXED(FX), x->w, 0, 0); return x->w; }
/* (a .msg's or a writing's words changed: drawn again, laid out again) */
static void text_set(GtkWidget *w, const char *t)
{
	Item *i = g_object_get_data(G_OBJECT(w), "set-item"); CItem *x = g_object_get_data(G_OBJECT(w), "set-citem");
	if (i) { g_free(i->text); i->text = g_strdup(t); } if (x) { g_free(x->text); x->text = g_strdup(t); } p_relayout();
}
void p_text_set(GtkWidget *w, const char *t);
void p_text_set(GtkWidget *w, const char *t) { text_set(w, t); }
GtkWidget *p_buttons(const char *const *labels, int n, GCallback *cbs, void *d)
{
	Item *i = add_item(I_BTNS, NULL, LK->rmrow_mt[0], 0); i->nb = MIN(n, 8);
	for (int k = 0; k < i->nb; k++) { i->btn[k] = rui_button(labels[k], 0); if (cbs && cbs[k]) g_signal_connect(i->btn[k], "clicked", cbs[k], d); gtk_fixed_put(GTK_FIXED(FX), i->btn[k], 0, 0); }
	return i->btn[0];
}
void p_block(GtkWidget *w, double mt, double mb) { Item *i = add_item(I_BLOCK, NULL, mt, mb); i->wg = w; gtk_fixed_put(GTK_FIXED(FX), w, 0, 0); }
void p_space(double mt) { add_item(I_SPACE, NULL, mt, 0); }
GtkWidget *p_msg_m(const char *t, int grey, double mt, double mb) { GtkWidget *w = p_msg(t, grey); Item *i = g_object_get_data(G_OBJECT(w), "set-item"); i->mt = mt; i->mb = mb; return w; }
GtkWidget *p_check(const char *t, int on, void (*cb)(int, void *), void *d) { Item *i = add_item(I_CHECK, t, 0, 0); i->wg = rui_check(t, on, cb, d); gtk_fixed_put(GTK_FIXED(FX), i->wg, 0, 0); return i->wg; }
GtkWidget *p_buttons_h(const char *const *labels, int n, GCallback *cbs, void *d, double minh) { GtkWidget *b = p_buttons(labels, n, cbs, d); Item *i = g_ptr_array_index(ITEMS, ITEMS->len - 1); i->minh = minh; return b; }
void p_custom(double (*lay)(void *d, double x, double y, double w), void *d, double mt, double mb) { Item *i = add_item(I_CUSTOM, NULL, mt, mb); i->lay = lay; i->ld = d; }
void p_put(GtkWidget *w) { gtk_fixed_put(GTK_FIXED(FX), w, 0, 0); gtk_widget_show_all(w); }
void p_place(GtkWidget *w, double x, double y, double wd, double h) { if (wd > 0 || h > 0) gtk_widget_set_size_request(w, wd > 0 ? R(wd) : -1, h > 0 ? R(h) : -1); gtk_fixed_move(GTK_FIXED(FX), w, R(x), R(y)); }
double p_nat_w(GtkWidget *w);
double p_nat_h(GtkWidget *w);
static double nat_w(GtkWidget *w) { int m, n; gtk_widget_get_preferred_width(w, &m, &n); return n / Z(); }
double p_nat_w(GtkWidget *w) { int m, n; gtk_widget_get_preferred_width(w, &m, &n); return n / Z(); }
double p_nat_h(GtkWidget *w) { int m, n; gtk_widget_get_preferred_height(w, &m, &n); return n / Z(); }
static double nat_h(GtkWidget *w, double wd) { int m, n; if (wd > 0) gtk_widget_get_preferred_height_for_width(w, R(wd), &m, &n); else gtk_widget_get_preferred_height(w, &m, &n); return n / Z(); }
static void place(GtkWidget *w, double x, double y, double wd, double h)
{
	if (wd > 0) { int ow, oh; gtk_widget_get_size_request(w, &ow, &oh); gtk_widget_set_size_request(w, R(wd), h > 0 ? R(h) : oh); }
	gtk_fixed_move(GTK_FIXED(FX), w, R(x), R(y));
}
static double ck_lines(const char *t, double w, double *lh) { double l = RUI.fs * LK->ck_lh[0]; if (lh) *lh = l; return text_lines(t, RUI.fs, 400, w - (4 + 13 + 3 + LK->ck_gap[0]), 0, NULL) * l; }
/* a cell's controls: each line of them, the blocks (.msg, label.ck, .rmrow, pre) with their margins collapsing, placed from (x, y) in w;
   its height */
static double pre_lh(void) { return 13; }
static double cell_lay(Cell *c, double x, double y, double w)
{
	double lt = y, lx = 0, lhh = 0, cy = y, pmb = 0; int inl = 0, first = 1;
	for (guint k = 0; k < c->items->len; k++) { CItem *it = g_ptr_array_index(c->items, k);
		if (it->k == 0 || it->k == 3) {
			if (!inl) { cy += pmb; pmb = 0; lt = cy; lx = 0; lhh = 0; inl = 1; }
			double ww, hh;
			if (it->k == 3) { double tw; text_lines(it->text, RUI.fs, it->weight == 700 ? 700 : 400, 0, 0, &tw); ww = tw; hh = lh_of(RUI.fs, 400); it->wd = ww; it->h = hh; gtk_widget_set_size_request(it->w, R(ww), R(hh)); }
			else { ww = it->full ? w : nat_w(it->w); hh = nat_h(it->w, ww); }
			it->x = x + lx + it->gap; it->y = lt + it->dy; place(it->w, it->x, it->y, it->full ? ww : 0, 0); lx += it->gap + ww; lhh = fmax(lhh, it->dy + hh); cy = lt + lhh;
			first = 0; continue; }
		if (it->k == 1) { if (inl) cy = lt + lhh; inl = 0; cy += pmb + it->mt; pmb = 0; continue; }
		/* a block */
		if (inl) { cy = lt + lhh; inl = 0; }
		cy += first ? it->mt : fmax(pmb, it->mt); first = 0; it->x = x; it->y = cy; it->wd = w;
		if (it->k == 2) { double lh = lh_of(LK->msg_fs[0], 400); int n = text_lines(it->text, LK->msg_fs[0], 400, w, 0, NULL); it->h = fmax(LK->msg_mh[0], n * lh); place(it->w, x, cy, w, it->h); }
		else if (it->k == 4) { double lh, h = ck_lines(it->text, w, &lh); it->h = fmax(19, h); rui_check_wrap(it->w, w, it->h); place(it->w, x, cy, 0, 0); }
		else if (it->k == 6) { PangoLayout *l = lay(pctx(), it->text, 11, 400, w, 0); PangoFontDescription *f = pango_font_description_from_string("monospace"); pango_font_description_set_absolute_size(f, 11 * Z() * PANGO_SCALE); pango_layout_set_font_description(l, f); pango_font_description_free(f);
			int n = pango_layout_get_line_count(l); g_object_unref(l); it->h = fmin(160, n * pre_lh()); place(it->w, x, cy, w, it->h); }
		else if (it->k == 5) { double bx = 0, by = 0, rh = 0;
			for (int b = 0; b < it->nb; b++) { double bw = nat_w(it->bt[b]), bh = nat_h(it->bt[b], bw); if (bx > 0 && bx + bw > w + .5) { bx = 0; by += rh + LK->rmrow_gap[0]; rh = 0; }
				place(it->bt[b], x + bx, cy + by, 0, 0); bx += bw + LK->rmrow_gap[0]; rh = fmax(rh, bh); }
			it->h = by + rh; }
		cy += it->h; pmb = it->mb;
	}
	if (inl) cy = lt + lhh;
	return cy - y;
}
static void lay_all(void)
{
	if (!ITEMS || !FX) return;
	double W = FX_W, x0 = LK->sc_pad[3], y = LK->sc_pad[0], pmb = 0; int first = 1;
	for (guint k = 0; k < ITEMS->len; k++) { Item *i = g_ptr_array_index(ITEMS, k);
		y += first ? i->mt : fmax(pmb, i->mt); first = 0; i->x = x0; i->y = y; i->w = W;
		switch (i->k) {
		case I_H3: i->h = lh_of(LK->h3_fs[0], 700); break;
		case I_H4: i->h = lh_of(13, 700); break;
		case I_SGRP: i->h = lh_of(SGRP_FS, 700); break;
		case I_MSG: { int n = text_lines(i->text, LK->msg_fs[0], 400, W, 0, NULL); i->h = fmax(LK->msg_mh[0], n * lh_of(LK->msg_fs[0], 400)); place(i->wg, x0, y, W, i->h); break; }
		case I_R2: { Cell *c = i->cell; double gap = LK->r2_gap[0], lw = 130; int n = text_lines(c->label, RUI.fs, 400, lw, 0, NULL); c->lh = n * lh_of(RUI.fs, 400);
			double ch = cell_lay(c, x0 + lw + gap, y, W - lw - gap); double h = fmax(c->lh, ch);
			if (ch < h) { /* (align-items: center: the cell in the middle of a taller label) */ double dy = (h - ch) / 2; for (guint q = 0; q < c->items->len; q++) { CItem *it = g_ptr_array_index(c->items, q); if (it->w && it->k != 1) { it->y += dy; gtk_fixed_move(GTK_FIXED(FX), it->w, R(it->x), R(it->y)); } } }
			c->lx = x0; c->ly = y + (h - c->lh) / 2; c->lw = lw; c->y = y; c->h = h; i->h = h; i->mb = c->more ? gap : LK->r2_mb[0]; break; }
		case I_BTNS: { double bx = x0, bh = 0; for (int b = 0; b < i->nb; b++) { double bw = nat_w(i->btn[b]), h = fmax(i->minh, nat_h(i->btn[b], bw)); if (i->minh > 0) { int ow, oh; gtk_widget_get_size_request(i->btn[b], &ow, &oh); gtk_widget_set_size_request(i->btn[b], ow, R(h)); } place(i->btn[b], bx, y, 0, 0); bx += bw + LK->rmrow_gap[0]; bh = fmax(bh, h); } i->h = bh; break; }
		case I_BLOCK: { double h = nat_h(i->wg, W); place(i->wg, x0, y, W, 0); i->h = h; break; }
		case I_CUSTOM: i->h = i->lay(i->ld, x0, y, W); break;
		case I_CHECK: { double lh, h = ck_lines(i->text, W, &lh); i->h = fmax(19, h); rui_check_wrap(i->wg, W, i->h); place(i->wg, x0, y, 0, 0); break; }
		default: i->h = 0;
		}
		y += i->h; pmb = i->mb;
	}
	double total;
	if (FOOT) { y += fmax(pmb, LK->sfoot_m[0]); total = y + nat_h(FOOT, -1) + LK->sfoot_m[2] + LK->sc_pad[2]; }
	else total = y + pmb + LK->sc_pad[2];
	gtk_widget_set_size_request(FX, -1, R(total)); gtk_widget_queue_draw(FX); set_state_soon();
}
static guint RELAY;
static gboolean relay_go(gpointer d) { (void)d; RELAY = 0; lay_all(); return G_SOURCE_REMOVE; }
void p_relayout(void) { if (!RELAY) RELAY = g_idle_add(relay_go, NULL); }
/* (the search's hit, lit for a moment: the page's .sshit) */
static Item *HIT; static CItem *HITC; static gint64 HIT_T; static guint HIT_ID;
static gboolean fx_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)d; if (!ITEMS) return FALSE;
	setc(cr, LK->sg_bg, 1); cairo_paint(cr);
	if (HIT && (HIT_T > 0)) { double t = (g_get_monotonic_time() - HIT_T) / 1e6, a = t < 2.4 * .6 ? 1 : fmax(0, 1 - (t - 2.4 * .6) / (2.4 * .4));
		if (a > 0) { rui_color(cr, "ac", .28 * a); double x, y, wd, h;
			if (HIT->k == I_R2 && HIT->cell) { Cell *c = HIT->cell;
				if (HITC) { x = HITC->x; y = HITC->y; wd = HITC->wd ? HITC->wd : nat_w(HITC->w); h = HITC->h ? HITC->h : nat_h(HITC->w, -1); }
				else { /* (the label and the control beside it) */ rrect(cr, c->lx * Z(), c->ly * Z(), c->lw * Z(), c->lh * Z(), 4 * Z()); cairo_fill(cr); x = c->lx + c->lw + LK->r2_gap[0]; y = c->y; wd = HIT->w - c->lw - LK->r2_gap[0]; h = c->h; }
			} else { x = HIT->x; y = HIT->y; wd = HIT->w; h = HIT->h; }
			rrect(cr, x * Z(), y * Z(), wd * Z(), h * Z(), 4 * Z()); cairo_fill(cr); } }
	for (guint k = 0; k < ITEMS->len; k++) { Item *i = g_ptr_array_index(ITEMS, k);
		switch (i->k) {
		case I_H3: fgc(cr, 1); text_draw(cr, i->text, i->x, i->y, LK->h3_fs[0], 700, 0, 0, i->h); break;
		case I_H4: if (RUI.dark) rui_color(cr, "mut", 1); else cairo_set_source_rgb(cr, 107 / 255.0, 114 / 255.0, 128 / 255.0);
			if (RUI.dark) setc(cr, LK->msg_dark, 1); { char *u = g_utf8_strup(i->text, -1); text_draw(cr, u, i->x, i->y, 13, 700, 0, .04, i->h); g_free(u); } break;
		case I_SGRP: fgc(cr, 1); { char *u = g_utf8_strup(i->text, -1); text_draw(cr, u, i->x, i->y, SGRP_FS, 700, 0, .04, i->h); g_free(u); } break;
		case I_MSG: if (RUI.dark) setc(cr, LK->msg_dark, 1); else if (i->grey) cairo_set_source_rgb(cr, 107 / 255.0, 114 / 255.0, 128 / 255.0); else fgc(cr, 1);
			text_draw(cr, i->text, i->x, i->y, LK->msg_fs[0], 400, i->w, 0, lh_of(LK->msg_fs[0], 400)); break;
		case I_R2: { Cell *c = i->cell; fgc(cr, 1); text_draw(cr, c->label, c->lx, c->ly, RUI.fs, 400, c->lw, 0, lh_of(RUI.fs, 400));
			for (guint q = 0; q < c->items->len; q++) { CItem *it = g_ptr_array_index(c->items, q);
				if (it->k == 2) { if (RUI.dark) setc(cr, LK->msg_dark, 1); else if (it->grey) cairo_set_source_rgb(cr, 107 / 255.0, 114 / 255.0, 128 / 255.0); else fgc(cr, 1);
					text_draw(cr, it->text, it->x, it->y, LK->msg_fs[0], 400, it->wd, 0, lh_of(LK->msg_fs[0], 400)); }
				if (it->k == 3) { fgc(cr, 1); text_draw(cr, it->text, it->x, it->y, RUI.fs, it->weight == 700 ? 700 : 400, 0, 0, it->h); }
				if (it->k == 6) { fgc(cr, 1); PangoLayout *l = lay(pctx(), it->text, 11, 400, it->wd, 0); PangoFontDescription *f = pango_font_description_from_string("monospace"); pango_font_description_set_absolute_size(f, 11 * Z() * PANGO_SCALE); pango_layout_set_font_description(l, f); pango_font_description_free(f);
					PangoFontMetrics *m = pango_context_get_metrics(pctx(), pango_layout_get_font_description(l), NULL); double a = round(pango_font_metrics_get_ascent(m) / (double)PANGO_SCALE); pango_font_metrics_unref(m);
					cairo_save(cr); cairo_rectangle(cr, R(it->x), R(it->y), R(it->wd), R(it->h)); cairo_clip(cr); PangoLayoutIter *li = pango_layout_get_iter(l); int n = 0;
					do { cairo_move_to(cr, R(it->x), round(R(it->y + n * pre_lh()) + a)); pango_cairo_show_layout_line(cr, pango_layout_iter_get_line_readonly(li)); n++; } while (pango_layout_iter_next_line(li));
					pango_layout_iter_free(li); g_object_unref(l); cairo_restore(cr); } }
			break; }
		}
	}
	return FALSE;
}
static gboolean hit_tick(gpointer d) { (void)d; if (FX) gtk_widget_queue_draw(FX); if (g_get_monotonic_time() - HIT_T > 2600000) { HIT = NULL; HITC = NULL; HIT_ID = 0; return G_SOURCE_REMOVE; } return G_SOURCE_CONTINUE; }

/* ------------------------------------------------------------------ the footer (.sfoot: secFooter) */
static void foot_click(GtkButton *b, gpointer d);
void p_footer(int (*dirty)(void), SetFoot act)
{
	FDIRTY = dirty; FACT = act;
	FOOT = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, R(LK->sfoot_gap[0])); gtk_style_context_add_class(gtk_widget_get_style_context(FOOT), "set-foot"); gtk_widget_set_valign(FOOT, GTK_ALIGN_END);
	FOOTST = gtk_label_new(""); gtk_style_context_add_class(gtk_widget_get_style_context(FOOTST), "set-sst"); gtk_label_set_xalign(GTK_LABEL(FOOTST), 0);
	gtk_box_pack_start(GTK_BOX(FOOT), FOOTST, TRUE, TRUE, 0);
	static const char *const N[3] = { "Apply", "Revert", "Reset to defaults" };
	for (int i = 0; i < 3; i++) { FOOTB[i] = rui_button(N[i], 0); g_object_set_data(G_OBJECT(FOOTB[i]), "set-i", GINT_TO_POINTER(i)); g_signal_connect(FOOTB[i], "clicked", G_CALLBACK(foot_click), NULL); gtk_box_pack_start(GTK_BOX(FOOT), FOOTB[i], FALSE, FALSE, 0); }
	p_label_named("apply", FOOTB[0]); p_label_named("revert", FOOTB[1]); p_label_named("reset", FOOTB[2]);
	gtk_overlay_add_overlay(GTK_OVERLAY(AREA), FOOT); gtk_widget_show_all(FOOT); p_footer_update();
}
static guint FNOTE;
static gboolean fnote_end(gpointer d) { (void)d; FNOTE = 0; p_footer_update(); return G_SOURCE_REMOVE; }
void p_footer_update(void)
{
	if (!FOOT || FNOTE) return; int d = FDIRTY && FDIRTY();
	char t[120]; snprintf(t, sizeof t, d ? "● %s" : "%s", d ? rui_tr("Unapplied changes") : ""); gtk_label_set_text(GTK_LABEL(FOOTST), t);
	GtkStyleContext *sc = gtk_widget_get_style_context(FOOTST); gtk_style_context_remove_class(sc, "ok");
	gtk_widget_set_sensitive(FOOTB[0], d); gtk_widget_set_sensitive(FOOTB[1], d);
	GtkStyleContext *a = gtk_widget_get_style_context(FOOTB[0]); if (d) gtk_style_context_add_class(a, "pri"); else gtk_style_context_remove_class(a, "pri");
	set_state_soon();
}
void p_footer_note(const char *t)
{
	if (!FOOT) return; char b[160]; snprintf(b, sizeof b, "✓ %s", rui_tr(t)); gtk_label_set_text(GTK_LABEL(FOOTST), b); gtk_style_context_add_class(gtk_widget_get_style_context(FOOTST), "ok");
	if (FNOTE) g_source_remove(FNOTE); FNOTE = g_timeout_add(2500, fnote_end, NULL); set_note("footer %s", t);
}
static void foot_do(int i, void *d) { int what = GPOINTER_TO_INT(d); if (i == 1 && FACT) FACT(what); else set_note("footer %d cancelled", what); p_footer_update(); }
static void foot_click(GtkButton *b, gpointer d)
{
	(void)d; int i = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "set-i"));
	static const char *const T[3] = { "Apply", "Revert", "Reset to defaults" }, *const M[3] = { "Apply these changes?", "Discard your unapplied changes and go back to the saved settings?", "Reset these settings to their defaults?" };
	if (i < 2 && !(FDIRTY && FDIRTY())) return;
	const char *const B[2] = { "Cancel", "Confirm" }; rui_confirm(SW, T[i], M[i], B, 2, 1, foot_do, GINT_TO_POINTER(i)); set_note("ask footer %d", i); set_state_soon();
}

/* ================================================================== the drop-down (the page's select, closed as Chromium draws it) */
typedef struct { char **v, **l; int n, cur, *off; void (*cb)(const char *, void *); void *d; double w; } Sel;
#define SEL_FS (40.0 / 3)   /* (a select's own font size: Chromium's 13.333px, the page setting only its family) */
static double sel_inner(void) { return lh_of(SEL_FS, 400) + 2; }
static void sel_size(GtkWidget *w)
{
	Sel *s = g_object_get_data(G_OBJECT(w), "set-sel"); double tw = 0;
	for (int i = 0; i < s->n; i++) { double x; text_lines(s->l[i], SEL_FS, 400, 0, 0, &x); tw = fmax(tw, x); }
	double h = 2 * LK->sel_bd_w[0] + LK->sel_pad[0] + LK->sel_pad[2] + sel_inner();
	double ww = s->w > 0 ? s->w : ceil(2 * LK->sel_bd_w[0] + LK->sel_pad[1] + LK->sel_pad[3] + tw + 21);   /* (Chromium's arrow's room: 21 px) */
	gtk_widget_set_size_request(w, R(ww), R(h));
}
static gboolean sel_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Sel *s = d; double W = gtk_widget_get_allocated_width(w) / Z(), H = gtk_widget_get_allocated_height(w) / Z(), b = LK->sel_bd_w[0]; int on = gtk_widget_is_sensitive(w);
	cairo_save(cr); cairo_scale(cr, Z(), Z());
	setc(cr, LK->sel_bg, 1); rrect(cr, b / 2, b / 2, W - b, H - b, LK->sel_r[0]); cairo_fill_preserve(cr); setc(cr, LK->sel_bd, on ? 1 : .5); cairo_set_line_width(cr, b); cairo_stroke(cr);
	double a = on ? 1 : .5; setc(cr, LK->sel_fg, a);
	/* the arrow: Chromium's chevron, 9 px wide, at 5 px from the right edge */
	double cx = W - 9.5, cy = H / 2; cairo_set_line_width(cr, 1.6); cairo_move_to(cr, cx - 4, cy - 2); cairo_line_to(cr, cx, cy + 2); cairo_line_to(cr, cx + 4, cy - 2); cairo_stroke(cr);
	cairo_restore(cr);
	if (gtk_widget_has_visible_focus(w)) { cairo_save(cr); cairo_scale(cr, Z(), Z()); rui_color(cr, "ac", 1); cairo_set_line_width(cr, 2); rrect(cr, -1, -1, W + 2, H + 2, LK->sel_r[0] + 1); cairo_stroke(cr); cairo_restore(cr); }
	setc(cr, LK->sel_fg, a);
	cairo_save(cr); cairo_rectangle(cr, 0, 0, R(W - 21), R(H)); cairo_clip(cr);
	text_draw(cr, s->cur >= 0 && s->cur < s->n ? s->l[s->cur] : "", b + LK->sel_pad[3], b + LK->sel_pad[0] + 1, SEL_FS, 400, 0, 0, sel_inner() - 2);
	cairo_restore(cr);
	return TRUE;
}
static void sel_pick(int i, void *d)
{
	GtkWidget *w = d; Sel *s = g_object_get_data(G_OBJECT(w), "set-sel"); if (i < 0 || i >= s->n || (s->off && s->off[i]) || i == s->cur) return;
	s->cur = i; gtk_widget_queue_draw(w); if (s->cb) s->cb(s->v[i], s->d);
}
static void sel_open(GtkWidget *w)
{
	Sel *s = g_object_get_data(G_OBJECT(w), "set-sel"); RuiMenuItem *m = g_new0(RuiMenuItem, s->n);
	for (int i = 0; i < s->n; i++) { m[i].label = s->l[i]; m[i].icon = i == s->cur ? "✓" : ""; m[i].off = s->off ? s->off[i] : 0; }
	int x, y, ox, oy; gdk_window_get_origin(gtk_widget_get_window(SW->win), &ox, &oy); gtk_widget_translate_coordinates(w, SW->win, 0, gtk_widget_get_allocated_height(w), &x, &y);
	rui_menu(w, ox + x, oy + y, NULL, m, s->n, sel_pick, w); g_free(m); set_note("select open %s", gtk_widget_get_name(w)); set_state_soon();
}
static gboolean sel_press(GtkWidget *w, GdkEventButton *e, gpointer d) { (void)d; if (e->button != 1 || e->type != GDK_BUTTON_PRESS) return FALSE; gtk_widget_grab_focus(w); sel_open(w); return TRUE; }
static gboolean sel_key(GtkWidget *w, GdkEventKey *e, gpointer d)
{
	Sel *s = d; int k = e->keyval, i = s->cur;
	if (k == GDK_KEY_space || k == GDK_KEY_Return || k == GDK_KEY_KP_Enter || (k == GDK_KEY_Down && (e->state & GDK_MOD1_MASK))) { sel_open(w); return TRUE; }
	if (k == GDK_KEY_Down || k == GDK_KEY_Up) { int st = k == GDK_KEY_Down ? 1 : -1; for (int j = i + st; j >= 0 && j < s->n; j += st) if (!(s->off && s->off[j])) { sel_pick(j, w); break; } return TRUE; }
	return FALSE;
}
static void sel_free(gpointer p) { Sel *s = p; g_strfreev(s->v); g_strfreev(s->l); g_free(s->off); g_free(s); }
static void sel_re(void *d) { sel_size(d); gtk_widget_queue_draw(d); }
GtkWidget *s_select(const char *const *v, const char *const *l, int n, const char *cur, void (*cb)(const char *, void *), void *d)
{
	Sel *s = g_new0(Sel, 1); s->n = n; s->v = g_new0(char *, n + 1); s->l = g_new0(char *, n + 1); s->cb = cb; s->d = d; s->cur = n ? 0 : -1;
	for (int i = 0; i < n; i++) { s->v[i] = g_strdup(v[i]); s->l[i] = g_strdup(rui_tr(l[i])); if (cur && !strcmp(cur, v[i])) s->cur = i; }
	GtkWidget *w = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(w), "set-sel", s, sel_free); gtk_widget_set_can_focus(w, TRUE);
	gtk_widget_add_events(w, GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK);
	g_signal_connect(w, "draw", G_CALLBACK(sel_draw), s); g_signal_connect(w, "button-press-event", G_CALLBACK(sel_press), s); g_signal_connect(w, "key-press-event", G_CALLBACK(sel_key), s);
	sel_size(w); rui_on_settings(sel_re, w); return w;
}
const char *s_select_get(GtkWidget *w) { Sel *s = g_object_get_data(G_OBJECT(w), "set-sel"); return s && s->cur >= 0 ? s->v[s->cur] : ""; }
void s_select_set(GtkWidget *w, const char *v) { Sel *s = g_object_get_data(G_OBJECT(w), "set-sel"); if (!s) return; for (int i = 0; i < s->n; i++) if (!strcmp(s->v[i], v)) s->cur = i; gtk_widget_queue_draw(w); }
void s_select_disable(GtkWidget *w, int i, int off) { Sel *s = g_object_get_data(G_OBJECT(w), "set-sel"); if (!s || i < 0 || i >= s->n) return; if (!s->off) s->off = g_new0(int, s->n); s->off[i] = off; }
void s_select_label(GtkWidget *w, int i, const char *t) { Sel *s = g_object_get_data(G_OBJECT(w), "set-sel"); if (!s || i < 0 || i >= s->n) return; g_free(s->l[i]); s->l[i] = g_strdup(t); sel_size(w); gtk_widget_queue_draw(w); }
void s_select_width(GtkWidget *w, double cw) { Sel *s = g_object_get_data(G_OBJECT(w), "set-sel"); if (!s) return; s->w = cw; sel_size(w); }

/* ================================================================== the date field (input type=date as Chromium draws it: mm/dd/yyyy) */
typedef struct { int y, m, d, seg, typed; void (*cb)(int, int, int, void *); void *dd; } Dt;
static int mdays(int y, int m) { static const int D[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }; return m == 2 && ((y % 4 == 0 && y % 100) || y % 400 == 0) ? 29 : D[(m + 11) % 12]; }
static gboolean dt_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Dt *t = d; double W = gtk_widget_get_allocated_width(w) / Z(), H = gtk_widget_get_allocated_height(w) / Z(); int on = gtk_widget_is_sensitive(w);
	cairo_save(cr); cairo_scale(cr, Z(), Z());
	if (RUI.dark) cairo_set_source_rgb(cr, 11 / 255.0, 18 / 255.0, 32 / 255.0); else cairo_set_source_rgb(cr, 1, 1, 1);
	rrect(cr, .5, .5, W - 1, H - 1, 2); cairo_fill_preserve(cr);
	if (RUI.dark) cairo_set_source_rgb(cr, 51 / 255.0, 65 / 255.0, 85 / 255.0); else cairo_set_source_rgba(cr, 118 / 255.0, 118 / 255.0, 118 / 255.0, on ? 1 : .5);
	cairo_set_line_width(cr, 1); cairo_stroke(cr);
	if (gtk_widget_has_focus(w) && !t->seg) { rui_color(cr, "ac", 1); cairo_set_line_width(cr, 2); rrect(cr, 0, 0, W, H, 3); cairo_stroke(cr); }
	/* the calendar's picture, at 10 px from the right edge (Chromium's) */
	double ix = W - 16, iy = H / 2 - 5; if (RUI.dark) cairo_set_source_rgb(cr, .89, .91, .94); else cairo_set_source_rgba(cr, 0, 0, 0, on ? 1 : .4);
	cairo_rectangle(cr, ix, iy, 11, 2); cairo_fill(cr); cairo_set_line_width(cr, 1); cairo_rectangle(cr, ix + .5, iy + .5, 10, 10); cairo_stroke(cr);
	cairo_restore(cr);
	char seg[3][8]; snprintf(seg[0], 8, "%02d", t->m); snprintf(seg[1], 8, "%02d", t->d); snprintf(seg[2], 8, "%04d", t->y);
	double x = 5, lh = lh_of(SEL_FS, 400), ty = (H - lh) / 2;
	for (int i = 0; i < 3; i++) { double tw; text_lines(seg[i], SEL_FS, 400, 0, 0, &tw);
		if (t->seg == i + 1 && gtk_widget_has_focus(w)) { cairo_save(cr); cairo_scale(cr, Z(), Z()); cairo_set_source_rgb(cr, 0x26 / 255.0, 0x5d / 255.0, 0xd8 / 255.0); cairo_rectangle(cr, x, ty, tw, lh); cairo_fill(cr); cairo_restore(cr); cairo_set_source_rgb(cr, 1, 1, 1); }
		else if (RUI.dark) cairo_set_source_rgb(cr, .89, .91, .94); else cairo_set_source_rgba(cr, 0, 0, 0, on ? 1 : .5);
		text_draw(cr, seg[i], x, ty, SEL_FS, 400, 0, 0, lh); x += tw;
		if (i < 2) { if (RUI.dark) cairo_set_source_rgb(cr, .89, .91, .94); else cairo_set_source_rgba(cr, 0, 0, 0, on ? 1 : .5); double sw; text_lines("/", SEL_FS, 400, 0, 0, &sw); text_draw(cr, "/", x, ty, SEL_FS, 400, 0, 0, lh); x += sw; } }
	return TRUE;
}
static void dt_changed(GtkWidget *w, Dt *t) { if (t->d > mdays(t->y, t->m)) t->d = mdays(t->y, t->m); gtk_widget_queue_draw(w); if (t->cb) t->cb(t->y, t->m, t->d, t->dd); }
static gboolean dt_press(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	Dt *t = d; if (e->button != 1) return FALSE; gtk_widget_grab_focus(w); double x = e->x / Z(), seg[3]; text_lines("00/", SEL_FS, 400, 0, 0, &seg[0]); seg[1] = seg[0];
	t->seg = x < 5 + seg[0] ? 1 : x < 5 + seg[0] + seg[1] ? 2 : 3; t->typed = 0; gtk_widget_queue_draw(w); return TRUE;
}
static gboolean dt_key(GtkWidget *w, GdkEventKey *e, gpointer d)
{
	Dt *t = d; int k = e->keyval; if (!t->seg) t->seg = 1;
	if (k == GDK_KEY_Left || (k == GDK_KEY_ISO_Left_Tab && t->seg > 1)) { if (t->seg > 1) { t->seg--; t->typed = 0; gtk_widget_queue_draw(w); return TRUE; } return FALSE; }
	if (k == GDK_KEY_Right || (k == GDK_KEY_Tab && t->seg < 3)) { if (t->seg < 3) { t->seg++; t->typed = 0; gtk_widget_queue_draw(w); return TRUE; } return FALSE; }
	if (k == GDK_KEY_Up || k == GDK_KEY_Down) { int st = k == GDK_KEY_Up ? 1 : -1;
		if (t->seg == 1) t->m = (t->m - 1 + st + 12) % 12 + 1; else if (t->seg == 2) { int n = mdays(t->y, t->m); t->d = (t->d - 1 + st + n) % n + 1; } else t->y = MAX(1, MIN(275760, t->y + st));
		dt_changed(w, t); return TRUE; }
	if (k >= GDK_KEY_0 && k <= GDK_KEY_9) { int v = k - GDK_KEY_0;
		if (t->seg == 1) { t->m = t->typed ? MIN(12, t->m * 10 + v) : MAX(1, v); if (t->typed || v > 1) { t->seg = 2; t->typed = 0; } else t->typed = 1; }
		else if (t->seg == 2) { t->d = t->typed ? MIN(31, t->d * 10 + v) : MAX(1, v); if (t->typed || v > 3) { t->seg = 3; t->typed = 0; } else t->typed = 1; }
		else { t->y = t->typed ? (t->y * 10 + v) % 100000 : v; t->typed++; if (t->typed >= 4) t->typed = 0; }
		if (t->y >= 1000) dt_changed(w, t); else gtk_widget_queue_draw(w); return TRUE; }
	return FALSE;
}
static gboolean dt_focus_out(GtkWidget *w, GdkEvent *e, gpointer d) { (void)e; ((Dt *)d)->seg = 0; gtk_widget_queue_draw(w); return FALSE; }
GtkWidget *s_date(int y, int m, int d, void (*cb)(int, int, int, void *), void *dd)
{
	Dt *t = g_new0(Dt, 1); t->y = y; t->m = m; t->d = d; t->cb = cb; t->dd = dd;
	GtkWidget *w = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(w), "set-dt", t, g_free); gtk_widget_set_can_focus(w, TRUE);
	gtk_widget_add_events(w, GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK | GDK_FOCUS_CHANGE_MASK);
	g_signal_connect(w, "draw", G_CALLBACK(dt_draw), t); g_signal_connect(w, "button-press-event", G_CALLBACK(dt_press), t); g_signal_connect(w, "key-press-event", G_CALLBACK(dt_key), t);
	g_signal_connect(w, "focus-out-event", G_CALLBACK(dt_focus_out), t);
	/* (Chromium's: 131 x 21 at 13.333px, its numbers, its calendar's picture) */
	gtk_widget_set_size_request(w, R(131), R(21)); return w;
}
void s_date_set(GtkWidget *w, int y, int m, int d) { Dt *t = g_object_get_data(G_OBJECT(w), "set-dt"); if (!t) return; t->y = y; t->m = m; t->d = d; gtk_widget_queue_draw(w); }

/* ================================================================== the pages, the sidebar (.sn) */
typedef void (*PageFn)(void);
static const struct { const char *k; PageFn fn; } NATIVE_PAGES[] = { { "sys", page_sys }, { "upd", page_upd }, { "about", page_about } };
static PageFn page_fn(const char *k) { for (guint i = 0; i < G_N_ELEMENTS(NATIVE_PAGES); i++) if (!strcmp(NATIVE_PAGES[i].k, k)) return NATIVE_PAGES[i].fn; return NULL; }
static int page_i(const char *k) { for (int i = 0; i < NPAGES; i++) if (!strcmp(PAGES[i].k, k)) return i; return -1; }
static int SIDE_HOV = -1, SIDE_FOC = -1;
static double side_w(void) { return LK->sn_pad[3] + LK->sn_flex[0] + LK->sn_pad[1] + LK->sn_bd_w[0]; }
static double side_ih(void) { return LK->sni_pad[0] + lh_of(RUI.fs, 400) + LK->sni_pad[2]; }
static double side_h(void) { return LK->sn_pad[0] + NPAGES * (side_ih() + LK->sni_mb[0]) + LK->sn_pad[2]; }
static void side_size(void) { if (SIDE) gtk_widget_set_size_request(SIDE, R(side_w()), R(side_h())); }
static char *side_text(int i) { return g_strdup_printf("%s  %s", PAGES[i].icon, rui_tr(PAGES[i].name)); }
static gboolean side_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)d; double W = side_w(), H = gtk_widget_get_allocated_height(w) / Z(), ih = side_ih();
	cairo_save(cr); cairo_scale(cr, Z(), Z()); setc(cr, LK->sn_bg, 1); cairo_paint(cr);
	setc(cr, LK->sn_bd, 1); cairo_rectangle(cr, W - LK->sn_bd_w[0], 0, LK->sn_bd_w[0], H); cairo_fill(cr); cairo_restore(cr);
	int cur = page_i(CUR);
	for (int i = 0; i < NPAGES; i++) { double x = LK->sn_pad[3], y = LK->sn_pad[0] + i * (ih + LK->sni_mb[0]), iw = LK->sn_flex[0];
		cairo_save(cr); cairo_scale(cr, Z(), Z());
		if (i == cur) { rui_color(cr, "ac", 1); rrect(cr, x, y, iw, ih, LK->sni_r[0]); cairo_fill(cr); }
		else if (i == SIDE_HOV) { setc(cr, LK->selhov, 1); rrect(cr, x, y, iw, ih, LK->sni_r[0]); cairo_fill(cr); }
		if (i == SIDE_FOC && gtk_widget_has_visible_focus(w)) { rui_color(cr, "ac", 1); cairo_set_line_width(cr, LK->focus_w[0]); double o = LK->focus_off[0] + LK->focus_w[0] / 2; rrect(cr, x - o, y - o, iw + 2 * o, ih + 2 * o, LK->sni_r[0] + o); cairo_stroke(cr); }
		cairo_restore(cr);
		if (i == cur) rui_color(cr, "acfg", 1); else fgc(cr, 1);
		char *t = side_text(i); cairo_save(cr); cairo_rectangle(cr, R(x), R(y), R(iw), R(ih)); cairo_clip(cr); text_draw(cr, t, x + LK->sni_pad[3], y + LK->sni_pad[0], RUI.fs, 400, 0, 0, lh_of(RUI.fs, 400)); cairo_restore(cr); g_free(t); }
	return TRUE;
}
static int side_at(double y) { y = y / Z() - LK->sn_pad[0]; if (y < 0) return -1; double ih = side_ih() + LK->sni_mb[0]; int i = (int)(y / ih); return i < NPAGES && y - i * ih < side_ih() ? i : -1; }
static gboolean side_motion(GtkWidget *w, GdkEventMotion *e, gpointer d) { (void)d; int i = side_at(e->y); if (i != SIDE_HOV) { SIDE_HOV = i; gtk_widget_queue_draw(w); } return FALSE; }
static gboolean side_leave(GtkWidget *w, GdkEventCrossing *e, gpointer d) { (void)e; (void)d; if (SIDE_HOV != -1) { SIDE_HOV = -1; gtk_widget_queue_draw(w); } return FALSE; }
static void show_guarded(const char *k);
static gboolean side_go(gpointer d) { show_guarded(d); return G_SOURCE_REMOVE; }   /* (the page changed after the event: not while GTK still handles it) */
static gboolean side_press(GtkWidget *w, GdkEventButton *e, gpointer d) { (void)d; if (e->button != 1 || e->type != GDK_BUTTON_PRESS) return FALSE; int i = side_at(e->y); if (i < 0) return TRUE; SIDE_FOC = i; gtk_widget_grab_focus(w); g_idle_add(side_go, (gpointer)PAGES[i].k); return TRUE; }
static gboolean side_key(GtkWidget *w, GdkEventKey *e, gpointer d)
{
	(void)d; int k = e->keyval; if (SIDE_FOC < 0) SIDE_FOC = MAX(0, page_i(CUR));
	if (k == GDK_KEY_Return || k == GDK_KEY_KP_Enter || k == GDK_KEY_space) { g_idle_add(side_go, (gpointer)PAGES[SIDE_FOC].k); return TRUE; }
	if (k == GDK_KEY_Down && SIDE_FOC < NPAGES - 1) { SIDE_FOC++; gtk_widget_queue_draw(w); set_state_soon(); return TRUE; }
	if (k == GDK_KEY_Up && SIDE_FOC > 0) { SIDE_FOC--; gtk_widget_queue_draw(w); set_state_soon(); return TRUE; }
	return k == GDK_KEY_Down || k == GDK_KEY_Up;
}

/* ================================================================== Search in Settings (setsearch59.js) */
typedef struct { const char *k; char *t, *s, *p; const char *ic, *kw; int h, page; } SE;
typedef struct { char *t, *s; int h; } Harv;
static GPtrArray *LIVE[64];   /* (SSLIVE: what each native page showed, harvested as it was shown) */
/* the words of a text: lower case, without accents; split at what is not a-z or 0-9, the parts joined too ("wi-fi": wi, fi, wifi) */
static void words(const char *s, GPtrArray *o)
{
	char *lo = g_utf8_strdown(s, -1), *nf = g_utf8_normalize(lo, -1, G_NORMALIZE_NFD); g_free(lo); GString *b = g_string_new(NULL);
	for (const char *p = nf; p && *p; p = g_utf8_next_char(p)) { gunichar c = g_utf8_get_char(p); if (c >= 0x300 && c <= 0x36f) continue; g_string_append_unichar(b, c); }
	g_free(nf); char **ws = g_regex_split_simple("\\s+", b->str, 0, 0); g_string_free(b, TRUE);
	for (int i = 0; ws[i]; i++) { if (!*ws[i]) continue; GPtrArray *parts = g_ptr_array_new(); GString *cur = g_string_new(NULL);
		for (const char *p = ws[i];; p++) { int ok = *p && ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9')); if (ok) g_string_append_c(cur, *p); else { if (cur->len) g_ptr_array_add(parts, g_strdup(cur->str)); g_string_truncate(cur, 0); } if (!*p) break; }
		g_string_free(cur, TRUE); GString *j = g_string_new(NULL); for (guint k = 0; k < parts->len; k++) { g_ptr_array_add(o, g_strdup(g_ptr_array_index(parts, k))); g_string_append(j, g_ptr_array_index(parts, k)); }
		if (parts->len > 1) g_ptr_array_add(o, g_strdup(j->str)); g_string_free(j, TRUE); g_ptr_array_free(parts, TRUE); }
	g_strfreev(ws);
}
static GPtrArray *wlist(const char *s) { GPtrArray *o = g_ptr_array_new_with_free_func(g_free); words(s ? s : "", o); return o; }
static int any_starts(GPtrArray *a, const char *w) { for (guint i = 0; i < a->len; i++) if (g_str_has_prefix(g_ptr_array_index(a, i), w)) return 1; return 0; }
static void se_free(gpointer p) { SE *e = p; g_free(e->t); g_free(e->s); g_free(e->p); g_free(e); }
static GPtrArray *ss_all(void)
{
	GPtrArray *L = g_ptr_array_new_with_free_func(se_free);
	for (int i = 0; i < NPAGES; i++) { const char *k = PAGES[i].k, *pg = PAGES[i].name;
		SE *e = g_new0(SE, 1); e->k = k; e->t = g_strdup(pg); e->ic = PAGES[i].icon; e->p = g_strdup("Settings"); e->kw = PAGES[i].kw; e->page = 1; g_ptr_array_add(L, e);
		if (LIVE[i]) { for (guint q = 0; q < LIVE[i]->len; q++) { Harv *h = g_ptr_array_index(LIVE[i], q); if (!strcmp(h->t, pg)) continue;
				SE *x = g_new0(SE, 1); x->k = k; x->t = g_strdup(h->t); x->s = g_strdup(h->s); x->h = h->h; x->kw = ""; x->p = g_strdup_printf("Settings › %s%s%s", pg, h->s[0] && !h->h ? " › " : "", h->s[0] && !h->h ? h->s : ""); g_ptr_array_add(L, x); } }
		else for (int q = 0; SETIDX[q].k; q++) { if (strcmp(SETIDX[q].k, k) || !strcmp(SETIDX[q].t, pg)) continue;
				SE *x = g_new0(SE, 1); x->k = k; x->t = g_strdup(SETIDX[q].t); x->s = g_strdup(SETIDX[q].s); x->h = SETIDX[q].h; x->kw = ""; x->p = g_strdup_printf("Settings › %s%s%s", pg, x->s[0] && !x->h ? " › " : "", x->s[0] && !x->h ? x->s : ""); g_ptr_array_add(L, x); }
	}
	return L;
}
typedef struct { double sc; long len; int i; SE *e; } Scored;
static long u16len(const char *s) { long n = 0; for (const char *p = s; *p; p = g_utf8_next_char(p)) n += g_utf8_get_char(p) > 0xffff ? 2 : 1; return n; }
static int sc_cmp(const void *a, const void *b) { const Scored *x = a, *y = b; if (x->sc != y->sc) return x->sc < y->sc ? -1 : 1; if (x->len != y->len) return x->len < y->len ? -1 : 1; return x->i - y->i; }
static GPtrArray *ALL, *RES;   /* (RES: SE* of ALL) */
static void ss_match(const char *q)
{
	if (RES) g_ptr_array_unref(RES); RES = g_ptr_array_new(); if (ALL) g_ptr_array_unref(ALL); ALL = ss_all();
	GPtrArray *qw = wlist(q); if (!qw->len) { g_ptr_array_unref(qw); return; }
	char *ql = g_utf8_strdown(q, -1); g_strstrip(ql); Scored *S = g_new0(Scored, ALL->len); int n = 0;
	for (guint i = 0; i < ALL->len; i++) { SE *e = g_ptr_array_index(ALL, i); GPtrArray *tw = wlist(e->t), *all = wlist(e->t), *pw = wlist(e->p), *kw = wlist(e->kw);
		for (guint k = 0; k < pw->len; k++) g_ptr_array_add(all, g_strdup(g_ptr_array_index(pw, k))); for (guint k = 0; k < kw->len; k++) g_ptr_array_add(all, g_strdup(g_ptr_array_index(kw, k)));
		int ok = 1; for (guint k = 0; ok && k < qw->len; k++) if (!any_starts(all, g_ptr_array_index(qw, k))) ok = 0;
		if (ok) { char *tl = g_utf8_strdown(e->t, -1); double sc; int alltw = 1; for (guint k = 0; k < qw->len; k++) if (!any_starts(tw, g_ptr_array_index(qw, k))) alltw = 0;
			sc = g_str_has_prefix(tl, ql) ? 0 : alltw ? 1 : 2; g_free(tl);
			if (e->page) { GPtrArray *tk = wlist(e->t); for (guint k = 0; k < kw->len; k++) g_ptr_array_add(tk, g_strdup(g_ptr_array_index(kw, k))); int a = 1; for (guint k = 0; k < qw->len; k++) if (!any_starts(tk, g_ptr_array_index(qw, k))) a = 0; g_ptr_array_unref(tk); sc = a ? -1 : sc - .5; }
			else if (e->h) sc -= .2;
			S[n].sc = sc; S[n].len = u16len(e->t); S[n].i = i; S[n].e = e; n++; }
		g_ptr_array_unref(tw); g_ptr_array_unref(all); g_ptr_array_unref(pw); g_ptr_array_unref(kw); }
	qsort(S, n, sizeof *S, sc_cmp); for (int i = 0; i < n && i < 60; i++) g_ptr_array_add(RES, S[i].e);
	g_free(S); g_free(ql); g_ptr_array_unref(qw);
}
/* the typed words in bold in a result (ssMark), as Pango's markup */
static char *ss_mark(const char *t, const char *q)
{
	GPtrArray *qw = wlist(q); GString *o = g_string_new(NULL); GRegex *re = g_regex_new("(\\s+)", 0, 0, NULL); char **parts = g_regex_split(re, t, 0); g_regex_unref(re);
	for (int i = 0; parts[i]; i++) { const char *w = parts[i]; char *lo = g_utf8_strdown(w, -1), *nf = g_utf8_normalize(lo, -1, G_NORMALIZE_NFD); GString *b = g_string_new(NULL);
		for (const char *p = nf; p && *p; p = g_utf8_next_char(p)) { gunichar c = g_utf8_get_char(p); if (c >= 0x300 && c <= 0x36f) continue; g_string_append_unichar(b, c); }
		const char *st = b->str; while (*st && !((*st >= 'a' && *st <= 'z') || (*st >= '0' && *st <= '9'))) st++;
		const char *m = NULL; for (guint k = 0; k < qw->len; k++) { const char *x = g_ptr_array_index(qw, k); if (g_str_has_prefix(st, x) && (!m || strlen(x) > strlen(m))) m = x; }
		if (!m) { char *e = g_markup_escape_text(w, -1); g_string_append(o, e); g_free(e); }
		else { const char *p = w; long pre = 0; while (*p) { gunichar c = g_utf8_get_char(p); if ((c < 128 && isalnum((int)c)) || (c >= 0xc0 && c <= 0x24f)) break; p = g_utf8_next_char(p); pre++; }
			long ml = (long)g_utf8_strlen(m, -1); const char *a = g_utf8_offset_to_pointer(w, pre), *z = g_utf8_offset_to_pointer(w, MIN(pre + ml, g_utf8_strlen(w, -1)));
			char *e1 = g_markup_escape_text(w, a - w), *e2 = g_markup_escape_text(a, z - a), *e3 = g_markup_escape_text(z, -1); g_string_append_printf(o, "%s<b>%s</b>%s", e1, e2, e3); g_free(e1); g_free(e2); g_free(e3); }
		g_free(lo); g_free(nf); g_string_free(b, TRUE); }
	g_strfreev(parts); g_ptr_array_unref(qw); return g_string_free(o, FALSE);
}
/* the drop-down of results (.ssdd) */
static int RCUR = -1; static GtkWidget *DDA, *DDS;
static double row_h(void) { return LK->ssr_pad[0] + LK->sst_fs[0] * LK->sst_lh[0] + LK->ssp_mt[0] + lh_of(LK->ssp_fs[0], 400) + LK->ssr_pad[2]; }
static double none_h(void) { return LK->ssnone_pad[0] + lh_of(LK->ssnone_fs[0], 400) + LK->ssnone_pad[2]; }
static void dd_close(void) { if (DD) gtk_widget_hide(DD); RCUR = -1; set_state_soon(); }
static gboolean dd_open(void) { return DD && gtk_widget_get_visible(DD); }
static void dd_size(void)
{
	double ih = RES && RES->len ? RES->len * row_h() : none_h(), b = LK->ssdd_bd_w[0], pad = LK->ssdd_pad[0] + LK->ssdd_pad[2], h = fmin(LK->ssdd_maxh[0], ih + pad + 2 * b);
	double W = gtk_widget_get_allocated_width(SW->body) / Z() - 2 * LK->ssdd_lr[0];
	gtk_widget_set_size_request(DDA, R(W - 2 * b - LK->ssdd_pad[1] - LK->ssdd_pad[3]), R(ih)); gtk_widget_set_size_request(DD, R(W), R(h));
	gtk_widget_set_margin_start(DD, R(LK->ssdd_lr[0])); gtk_widget_set_margin_top(DD, R(gtk_widget_get_allocated_height(SBAR) / Z() - LK->ssdd_top[0]));
}
static gboolean dd_frame(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)d; double W = gtk_widget_get_allocated_width(w) / Z(), H = gtk_widget_get_allocated_height(w) / Z(), b = LK->ssdd_bd_w[0];
	cairo_save(cr); cairo_scale(cr, Z(), Z()); setc(cr, LK->ssdd_bg, 1); rrect(cr, b / 2, b / 2, W - b, H - b, LK->ssdd_r[0]); cairo_fill_preserve(cr); setc(cr, LK->ssdd_bd, 1); cairo_set_line_width(cr, b); cairo_stroke(cr); cairo_restore(cr);
	return FALSE;
}
static gboolean dd_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)d; double W = gtk_widget_get_allocated_width(w) / Z();
	const char *q = gtk_entry_get_text(GTK_ENTRY(SQ));
	if (!RES || !RES->len) { setc(cr, LK->ssnone_fg, 1); char *t = g_strdup_printf("No settings match “%s”", q); char *s = g_strstrip(g_strdup(t)); (void)s; g_free(s);
		char *tq = g_strstrip(g_strdup(q)); char *tt = g_strdup_printf("No settings match “%s”", tq); text_draw(cr, tt, LK->ssnone_pad[3], LK->ssnone_pad[0], LK->ssnone_fs[0], 400, W - LK->ssnone_pad[1] - LK->ssnone_pad[3], 0, lh_of(LK->ssnone_fs[0], 400)); g_free(tt); g_free(tq); g_free(t); return TRUE; }
	double rh = row_h();
	for (guint i = 0; i < RES->len; i++) { SE *e = g_ptr_array_index(RES, i); double y = i * rh;
		if ((int)i == RCUR) { cairo_save(cr); cairo_scale(cr, Z(), Z()); rui_color(cr, "ac", 1); rrect(cr, 0, y, W, rh, LK->ssr_r[0]); cairo_fill(cr); cairo_restore(cr); }
		char *mk = ss_mark(e->t, q), *full = e->page ? g_strdup_printf("%s %s", e->ic, mk) : g_strdup(mk); g_free(mk);
		PangoLayout *l = lay(pctx(), "", LK->sst_fs[0], 400, 0, 0); pango_layout_set_markup(l, full, -1); g_free(full);
		double a, dd; a = asc_of(LK->sst_fs[0], 400, &dd); double lh = LK->sst_fs[0] * LK->sst_lh[0];
		if ((int)i == RCUR) rui_color(cr, "acfg", 1); else fgc(cr, 1);
		cairo_move_to(cr, R(LK->ssr_pad[3]), R(y + LK->ssr_pad[0] + (lh - a - dd) / 2 + a)); pango_cairo_show_layout_line(cr, pango_layout_get_line_readonly(l, 0)); g_object_unref(l);
		if ((int)i != RCUR) setc(cr, LK->ssp_fg, 1);
		PangoLayout *p = lay(pctx(), e->p, LK->ssp_fs[0], 400, W - LK->ssr_pad[1] - LK->ssr_pad[3], 0); pango_layout_set_ellipsize(p, PANGO_ELLIPSIZE_END); pango_layout_set_wrap(p, PANGO_WRAP_CHAR); pango_layout_set_height(p, 0);
		double pa, pd; pa = asc_of(LK->ssp_fs[0], 400, &pd); cairo_move_to(cr, R(LK->ssr_pad[3]), R(y + LK->ssr_pad[0] + lh + LK->ssp_mt[0] + pa)); pango_cairo_show_layout_line(cr, pango_layout_get_line_readonly(p, 0)); g_object_unref(p); }
	return TRUE;
}
static void dd_hi(int i)
{
	RCUR = i; gtk_widget_queue_draw(DDA);
	if (DDS && i >= 0) { GtkAdjustment *a = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(DDS)); double y = R(i * row_h()), h = R(row_h()), v = gtk_adjustment_get_value(a), pg = gtk_adjustment_get_page_size(a);
		if (y < v) gtk_adjustment_set_value(a, y); else if (y + h > v + pg) gtk_adjustment_set_value(a, y + h - pg); }
	set_state_soon();
}
static void ss_open(SE *e);
static void dd_pick(int i)
{
	if (!RES || i < 0 || i >= (int)RES->len) return; SE *e = g_ptr_array_index(RES, i); SE c = *e; c.t = g_strdup(e->t); c.s = g_strdup(e->s); c.p = NULL;
	dd_close(); gtk_window_set_focus(GTK_WINDOW(SW->win), NULL); set_note("search open %s %s", c.k, c.t); ss_open(&c); g_free(c.t); g_free(c.s);
}
static void ss_draw_list(void)
{
	const char *v = gtk_entry_get_text(GTK_ENTRY(SQ)); char *t = g_strstrip(g_strdup(v)); int empty = !*t; g_free(t);
	if (empty) { if (RES) g_ptr_array_set_size(RES, 0); dd_close(); return; }
	ss_match(v); gtk_widget_show(DD); gtk_widget_show_all(DDS); dd_size(); RCUR = -1; if (RES->len) dd_hi(0); gtk_widget_queue_draw(DDA); set_state_soon();
}
static gboolean dd_press(GtkWidget *w, GdkEventButton *e, gpointer d) { (void)w; (void)d; if (e->button != 1) return FALSE; dd_pick((int)(e->y / Z() / row_h())); return TRUE; }
static gboolean dd_motion(GtkWidget *w, GdkEventMotion *e, gpointer d) { (void)w; (void)d; int i = (int)(e->y / Z() / row_h()); if (RES && i < (int)RES->len && i != RCUR) dd_hi(i); return FALSE; }
static void sq_changed(GtkEditable *e, gpointer d) { (void)e; (void)d; ss_draw_list(); }
static gboolean sq_key(GtkWidget *w, GdkEventKey *e, gpointer d)
{
	(void)d; int k = e->keyval;
	if (k == GDK_KEY_Down || k == GDK_KEY_Up) { if (!dd_open()) ss_draw_list(); if (!RES || !RES->len) return TRUE; int n = RES->len; dd_hi(((RCUR < 0 ? (k == GDK_KEY_Down ? -1 : 0) : RCUR) + (k == GDK_KEY_Down ? 1 : -1) + n) % n); return TRUE; }
	if (k == GDK_KEY_Return || k == GDK_KEY_KP_Enter) { if (!dd_open()) ss_draw_list(); dd_pick(RCUR < 0 ? 0 : RCUR); return TRUE; }
	if (k == GDK_KEY_Escape) { if (dd_open()) { dd_close(); return TRUE; } if (*gtk_entry_get_text(GTK_ENTRY(w))) { gtk_entry_set_text(GTK_ENTRY(w), ""); return TRUE; } }
	return FALSE;
}
static gboolean sq_press(GtkWidget *w, GdkEventButton *e, gpointer d) { (void)e; (void)d; gtk_window_set_focus(GTK_WINDOW(SW->win), w); return FALSE; }
static gboolean sq_focus_in(GtkWidget *w, GdkEvent *e, gpointer d) { (void)w; (void)e; (void)d; char *t = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(SQ)))); if (*t) ss_draw_list(); g_free(t); return FALSE; }
static gboolean sq_blur_late(gpointer d) { (void)d; if (!gtk_widget_has_focus(SQ)) dd_close(); return G_SOURCE_REMOVE; }
static gboolean sq_focus_out(GtkWidget *w, GdkEvent *e, gpointer d) { (void)w; (void)e; (void)d; g_timeout_add(150, sq_blur_late, NULL); return FALSE; }
static gboolean ssi_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	/* the magnifier (.ssbar .ssi: the page's own picture, a circle and a line, in the writing's colour at its opacity) */
	(void)d; const double *m = LK->ssi_mask; double s = LK->ssi_w[0] / m[0], H = gtk_widget_get_allocated_height(w) / Z();
	cairo_save(cr); cairo_scale(cr, Z(), Z()); cairo_translate(cr, LK->ssi_left[0] - LK->ssbar_pad[3], H / 2 - LK->ssi_w[0] / 2);   /* (its place in the bar, from the field's edge) */ cairo_scale(cr, s, s);
	fgc(cr, LK->ssi_op[0]); cairo_set_line_width(cr, m[1]);
	cairo_new_sub_path(cr); cairo_arc(cr, m[2], m[3], m[4], 0, 2 * M_PI); cairo_stroke(cr); cairo_move_to(cr, m[5], m[6]); cairo_line_to(cr, m[7], m[8]); cairo_stroke(cr); cairo_restore(cr);
	return FALSE;
}

/* ================================================================== the window: a page shown, laid out, found */
static char *css_c(SetC c) { if (c.a < 0) return g_strdup(RUI.accent); return g_strdup_printf("rgba(%d,%d,%d,%.3f)", (int)lround(c.r * 255), (int)lround(c.g * 255), (int)lround(c.b * 255), c.a); }
static GtkCssProvider *CSS;
static void own_css(void)
{
	/* Settings' own parts in GTK's CSS (the search bar, its field, the footer), from the page's values (mksettings.py), scaled */
	char *bb = css_c(LK->ssbar_bg), *bd = css_c(LK->ssbar_bd), *qb = css_c(LK->ssq_bg), *qd = css_c(LK->ssq_bd), *fb = css_c(LK->sfoot_bg), *fd = css_c(LK->sfoot_bd), *st = css_c(LK->sfoot_sst), *ok = css_c(LK->sfoot_sst_ok);
	const char *fg = RUI.dark ? "#e2e8f0" : RUI.tx; double z = Z(), lhd = 0;
	{ PangoLayout *l = lay(pctx(), "Ag", RUI.fs, 400, 0, 0); int w, h; pango_layout_get_pixel_size(l, &w, &h); g_object_unref(l); lhd = h / z - lh_of(RUI.fs, 400); if (lhd < 0) lhd = 0; }
	char *c = g_strdup_printf(
		".set-ssbar { background-color: %s; border-bottom: %.2fpx solid %s; padding: %.2fpx %.2fpx %.2fpx %.2fpx; }\n"
		"entry.set-ssq { background-color: %s; color: %s; border: %.2fpx solid %s; border-radius: %.2fpx; padding: %.2fpx %.2fpx %.2fpx %.2fpx; font-size: %.2fpx; caret-color: %s; min-height: 0; }\n"
		"entry.set-ssq:focus { outline: %.2fpx solid %s; outline-offset: %.2fpx; -gtk-outline-radius: %.2fpx; }\n"
		"entry.set-ssq selection { background-color: alpha(%s, 0.3); }\n"
		".set-foot { background-color: %s; border-top: %.2fpx solid %s; padding: %.2fpx %.2fpx %.2fpx %.2fpx; }\n"
		".set-sst { font-size: %.2fpx; color: %s; }\n.set-sst.ok { color: %s; }\n"
		".set-foot button.rui:disabled { opacity: %g; }\n",
		bb, LK->ssbar_bd_w[0] * z, bd, LK->ssbar_pad[0] * z, LK->ssbar_pad[1] * z, LK->ssbar_pad[2] * z, LK->ssbar_pad[3] * z,
		qb, fg, LK->ssq_bd_w[0] * z, qd, LK->ssq_r[0] * z, LK->ssq_pad[0] * z, LK->ssq_pad[1] * z, fmax(0, LK->ssq_pad[2] - lhd) * z, LK->ssq_pad[3] * z, RUI.fs * z, fg,
		LK->ssq_focus_w[0] * z, RUI.accent, LK->ssq_focus_off[0] * z, (LK->ssq_r[0] + LK->ssq_focus_off[0]) * z, RUI.accent,
		fb, LK->sfoot_bd_w[0] * z, fd, LK->sfoot_pad[0] * z, LK->sfoot_pad[1] * z, LK->sfoot_pad[2] * z, LK->sfoot_pad[3] * z, LK->sfoot_sst_fs[0] * z, st, ok, LK->sfoot_dis[0]);
	if (!CSS) { CSS = gtk_css_provider_new(); gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(CSS), GTK_STYLE_PROVIDER_PRIORITY_USER + 1); }
	GError *e = NULL; if (!gtk_css_provider_load_from_data(CSS, c, -1, &e)) { g_printerr("rudolf-settings: its CSS: %s\n", e ? e->message : "?"); g_clear_error(&e); }
	g_free(c); g_free(bb); g_free(bd); g_free(qb); g_free(qd); g_free(fb); g_free(fd); g_free(st); g_free(ok);
}
/* what a page shows that Search in Settings can find (ssHarvest): its sections, its rows' labels, its check boxes' and inline labels */
static int harv_ok(const char *t, const char *sec, int h4)
{
	long n = g_utf8_strlen(t, -1); if (n > 90 || n < 2) return 0;
	int dig = 1; for (const char *p = t; *p; p++) if (!strchr("0123456789 \t%×.,:+−-", *p) && !(p[0] == '\xc3' && p[1] == '\x97') && !(p[0] == '\xe2' && p[1] == '\x88' && p[2] == '\x92')) { dig = 0; break; }
	if (dig) return 0; if (!h4 && !strcmp(t, sec)) return 0; return 1;
}
static char *harv_clean(const char *t)
{
	char *s = g_strdup(t ? t : ""); GRegex *r = g_regex_new("\\s+", 0, 0, NULL); char *a = g_regex_replace_literal(r, s, -1, 0, " ", 0, NULL); g_regex_unref(r); g_free(s);
	GRegex *r2 = g_regex_new("^[\\s★•·]+|[\\s:：…]+$", 0, 0, NULL); char *b = g_regex_replace_literal(r2, a, -1, 0, "", 0, NULL); g_regex_unref(r2); g_free(a); return g_strstrip(b);
}
static void harv_free(gpointer p) { Harv *h = p; g_free(h->t); g_free(h->s); g_free(h); }
static void harv_add(GPtrArray *o, GHashTable *seen, const char *t, const char *sec, int h4)
{
	char *c = harv_clean(t); if (!harv_ok(c, sec, h4)) { g_free(c); return; } char *key = g_strdup_printf("%s\t%s", c, h4 ? "" : sec);
	if (g_hash_table_contains(seen, key)) { g_free(key); g_free(c); return; } g_hash_table_add(seen, key);
	Harv *h = g_new0(Harv, 1); h->t = c; h->s = g_strdup(h4 ? "" : sec); h->h = h4; g_ptr_array_add(o, h);
}
static void harvest(void)
{
	int pi = page_i(CUR); if (pi < 0 || !page_fn(CUR) || !ITEMS) return;
	GPtrArray *o = g_ptr_array_new_with_free_func(harv_free); GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL); char *sec = g_strdup("");
	for (guint k = 0; k < ITEMS->len; k++) { Item *i = g_ptr_array_index(ITEMS, k);
		if (i->k == I_H4 || i->k == I_SGRP) { const char *orig = g_object_get_data(G_OBJECT(FX), "x") ? NULL : NULL; (void)orig; char *o2 = harv_clean(i->text ? i->text : ""); g_free(sec); sec = o2; if (!*sec) continue; harv_add(o, seen, i->text, "", 1); }
		if (i->k == I_CHECK) harv_add(o, seen, i->text, sec, 0);
		if (i->k == I_R2) { harv_add(o, seen, i->cell->label, sec, 0); for (guint q = 0; q < i->cell->items->len; q++) { CItem *it = g_ptr_array_index(i->cell->items, q); if (it->k == 4 || (it->k == 3 && it->weight == 1)) harv_add(o, seen, it->text, sec, 0); else if (it->k == 0 && g_object_get_data(G_OBJECT(it->w), "set-label")) harv_add(o, seen, g_object_get_data(G_OBJECT(it->w), "set-label"), sec, 0); } } }
	g_free(sec); g_hash_table_unref(seen); if (LIVE[pi]) g_ptr_array_unref(LIVE[pi]); LIVE[pi] = o; set_state_soon();
}
/* (a label of the cell that Search in Settings finds, as the page's <label>: "After") */
GtkWidget *c_label(Cell *c, const char *t, double dy);
GtkWidget *c_label(Cell *c, const char *t, double dy) { GtkWidget *w = c_text(c, rui_tr(t), 1); CItem *x = g_object_get_data(G_OBJECT(w), "set-citem"); x->dy = dy; return w; }
static GtkAdjustment *fx_adj(void) { GList *c = gtk_container_get_children(GTK_CONTAINER(SCR)); GtkAdjustment *a = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(c->data)); g_list_free(c); return a; }
static void scroll_to(double y, double h)
{
	GtkAdjustment *a = fx_adj(); double pg = gtk_adjustment_get_page_size(a); gtk_adjustment_set_value(a, MAX(0, MIN(gtk_adjustment_get_upper(a) - pg, R(y + h / 2) - pg / 2)));
}
static void page_build(void);
static void show_now(const char *k)
{
	if (!k || page_i(k) < 0) k = "disp";
	snprintf(CUR, sizeof CUR, "%s", k); page_build(); if (SIDE) gtk_widget_queue_draw(SIDE); set_note("show %s", k);
}
void set_show(const char *k) { show_now(k); }
/* (the page's W.guard: unapplied changes asked about first) */
static char NEXT[16];
static void guard_do(int i, void *d)
{
	(void)d; if (i == 0 || i < 0) { set_note("guard keep"); return; }
	if (i == 1 && FACT) FACT(1); if (i == 2 && FACT) FACT(0); set_note("guard %s", i == 1 ? "revert" : "apply"); show_now(NEXT);
}
static void show_guarded(const char *k)
{
	if (!strcmp(k, CUR)) { show_now(k); return; }
	if (FDIRTY && FDIRTY()) { snprintf(NEXT, sizeof NEXT, "%s", k); const char *const B[3] = { "Keep editing", "Revert", "Apply" }; rui_confirm(SW, "Unapplied changes", "You have changes that are not applied yet.", B, 3, 1, guard_do, NULL); set_note("ask guard"); return; }
	show_now(k);
}
static void ss_open(SE *e)
{
	show_guarded(e->k); if (strcmp(CUR, e->k)) return;   /* (asked first: the page stays) */
	lay_all(); if (e->page) { gtk_adjustment_set_value(fx_adj(), 0); return; }
	char *sec = g_strdup(""); Item *hit = NULL; CItem *hc = NULL;
	for (guint k = 0; k < ITEMS->len && !hit; k++) { Item *i = g_ptr_array_index(ITEMS, k);
		if (i->k == I_H4 || i->k == I_SGRP) { g_free(sec); sec = harv_clean(i->text); char *t = harv_clean(i->text); if (e->h && !strcmp(t, e->t)) hit = i; g_free(t); continue; }
		if (i->k != I_R2 || e->h) continue; int secok = !e->s || !*e->s || !strcmp(sec, e->s);
		char *t = harv_clean(i->cell->label); if (secok && !strcmp(t, e->t)) hit = i; g_free(t);
		for (guint q = 0; !hit && q < i->cell->items->len; q++) { CItem *it = g_ptr_array_index(i->cell->items, q); const char *lt = it->k == 0 ? g_object_get_data(G_OBJECT(it->w), "set-label") : it->text; if (!(it->k == 4 || (it->k == 3 && it->weight == 1) || (it->k == 0 && lt))) continue; char *x = harv_clean(lt); if (secok && !strcmp(x, e->t)) { hit = i; hc = it; } g_free(x); } }
	g_free(sec); if (!hit) return;
	HIT = hit; HITC = hc; HIT_T = g_get_monotonic_time(); if (!HIT_ID) HIT_ID = g_timeout_add(50, hit_tick, NULL);
	if (hit->k == I_R2) scroll_to(hit->cell->y, hit->cell->h); else scroll_to(hit->y, hit->h);
	/* (its control focused, as the page's) */
	GtkWidget *ctl = hc ? hc->w : NULL; if (!ctl && hit->k == I_R2) for (guint q = 0; q < hit->cell->items->len && !ctl; q++) { CItem *it = g_ptr_array_index(hit->cell->items, q); if (it->k == 0 && gtk_widget_get_can_focus(it->w) && gtk_widget_is_sensitive(it->w)) ctl = it->w; }
	if (ctl) gtk_widget_grab_focus(ctl);
	set_note("search hit %s", e->t); set_state_soon();
}
static void page_todo(void)
{
	int i = page_i(CUR); p_h3(PAGES[i].name);
	p_msg("This page is not in the native Settings yet (docs/plan-1.6.2.md Part 39): the native desktop opens the web Settings until every page is.", 1);
}
static void page_build(void)
{
	HIT = NULL; HITC = NULL; if (FOOT) { gtk_widget_destroy(FOOT); FOOT = NULL; } FDIRTY = NULL; FACT = NULL; if (NAMED) g_hash_table_remove_all(NAMED);
	if (FX) { GList *c = gtk_container_get_children(GTK_CONTAINER(FX)); for (GList *l = c; l; l = l->next) gtk_widget_destroy(l->data); g_list_free(c); }
	if (ITEMS) g_ptr_array_unref(ITEMS); ITEMS = g_ptr_array_new_with_free_func(item_free);
	PageFn f = page_fn(CUR); if (f) f(); else page_todo();
	gtk_widget_show_all(FX); lay_all(); gtk_adjustment_set_value(fx_adj(), 0); harvest();
	if (FOOT) p_footer_update();
}
static void fx_alloc(GtkWidget *w, GdkRectangle *a, gpointer d) { (void)w; (void)d; double nw = a->width / Z() - LK->sc_pad[1] - LK->sc_pad[3]; if (fabs(nw - FX_W) > .2) { FX_W = nw; p_relayout(); } }
static void outer_alloc(GtkWidget *w, GdkRectangle *a, gpointer d)
{
	(void)d; (void)w; static int lh = -1, lw = -1; int h = a->height, wd = a->width; if (h == lh && wd == lw) return; lh = h; lw = wd;
	/* (.sc as tall as .sg's view: max-height 100%; the sidebar may be taller, and .sg scrolls it) */
	gtk_widget_set_size_request(AREA, -1, h);
}
static void restyle(void *d) { (void)d; own_css(); side_size(); if (SIDE) gtk_widget_queue_draw(SIDE); p_relayout(); if (DDA) gtk_widget_queue_draw(DDA); }
static gboolean on_delete(GtkWidget *w, GdkEvent *e, gpointer d);
static void close_do(int i, void *d) { (void)d; if (i == 1 && FACT) FACT(1); if (i == 2 && FACT) FACT(0); if (i == 1 || i == 2) { set_note("close %s", i == 1 ? "discard" : "apply"); gtk_main_quit(); } }
static gboolean on_delete(GtkWidget *w, GdkEvent *e, gpointer d)
{
	(void)w; (void)e; (void)d;
	if (FDIRTY && FDIRTY()) { const char *const B[3] = { "Keep editing", "Discard", "Apply" }; rui_confirm(SW, "Unapplied changes", "You have changes that are not applied yet.", B, 3, 1, close_do, NULL); set_note("ask close"); return TRUE; }
	gtk_main_quit(); return TRUE;
}
static gboolean win_key(GtkWidget *w, GdkEventKey *e, gpointer d)
{
	(void)w; (void)d; if ((e->state & GDK_CONTROL_MASK) && (e->keyval == GDK_KEY_f || e->keyval == GDK_KEY_F)) { gtk_widget_grab_focus(SQ); return TRUE; }
	return FALSE;
}
static void state_more(GString *s)
{
	g_string_append_printf(s, "page %s\nnative %d\ndirty %d\nscroll %.0f\n", CUR, page_fn(CUR) != NULL, FDIRTY && FDIRTY(), FX ? gtk_adjustment_get_value(fx_adj()) / Z() : 0);
	if (FOOTST) g_string_append_printf(s, "foot %s\n", gtk_label_get_text(GTK_LABEL(FOOTST)));
	if (FOOT) g_string_append_printf(s, "footen %d %d\n", gtk_widget_is_sensitive(FOOTB[0]), gtk_widget_is_sensitive(FOOTB[1]));
	rect_line(s, "body", SW->body); rect_line(s, "search", SQ); rect_line(s, "side", SIDE); rect_line(s, "page", FX); if (DD && gtk_widget_get_visible(DD)) rect_line(s, "dd", DD);
	g_string_append_printf(s, "q %s\nres %d\nrcur %d\n", gtk_entry_get_text(GTK_ENTRY(SQ)), RES && dd_open() ? (int)RES->len : -1, RCUR);
	if (RES && dd_open()) for (guint i = 0; i < RES->len; i++) { SE *e = g_ptr_array_index(RES, i); g_string_append_printf(s, "r %s|%s|%s\n", e->k, e->t, e->p); }
	{ int ox, oy, x, y; gdk_window_get_origin(gtk_widget_get_window(SW->win), &ox, &oy); gtk_widget_translate_coordinates(SIDE, SW->win, 0, 0, &x, &y);
	  for (int i = 0; i < NPAGES; i++) g_string_append_printf(s, "rect side-%s %d %d %d %d\n", PAGES[i].k, ox + x + R(LK->sn_pad[3]), oy + y + R(LK->sn_pad[0] + i * (side_ih() + LK->sni_mb[0])), R(LK->sn_flex[0]), R(side_ih()));
	  if (DDA && dd_open() && RES) { gtk_widget_translate_coordinates(DDA, SW->win, 0, 0, &x, &y); for (guint i = 0; i < RES->len && i < 12; i++) g_string_append_printf(s, "rect res%u %d %d %d %d\n", i, ox + x, oy + y + R(i * row_h()), gtk_widget_get_allocated_width(DDA), R(row_h())); } }
	for (int i = 0; i < NPAGES; i++) if (LIVE[i]) for (guint q = 0; q < LIVE[i]->len; q++) { Harv *h = g_ptr_array_index(LIVE[i], q); g_string_append_printf(s, "harv %s|%s|%s|%d\n", PAGES[i].k, h->t, h->s, h->h); }
	if (HIT) g_string_append_printf(s, "hit %s\n", HIT->k == I_R2 ? HIT->cell->label : HIT->text);
	if (ITEMS && FX && gtk_widget_get_mapped(FX)) { int ox, oy, x, y; gdk_window_get_origin(gtk_widget_get_window(SW->win), &ox, &oy); gtk_widget_translate_coordinates(FX, SW->win, 0, 0, &x, &y);
		for (guint k = 0; k < ITEMS->len; k++) { Item *i = g_ptr_array_index(ITEMS, k); if (i->k == I_R2) g_string_append_printf(s, "row %s|%d %d %d %d\n", i->cell->label, ox + x + R(i->x), oy + y + R(i->cell->y), R(i->w), R(i->cell->h)); } }
	{ GtkWidget *f = gtk_window_get_focus(GTK_WINDOW(SW->win)); g_string_append_printf(s, "focus %s\n", f ? (f == SQ ? "search" : f == SIDE ? "side" : gtk_widget_get_name(f)) : "-"); if (SIDE_FOC >= 0) g_string_append_printf(s, "sidefoc %d\n", SIDE_FOC); }
}
static gboolean state_tick(gpointer d) { (void)d; set_state_soon(); return G_SOURCE_CONTINUE; }
int main(int argc, char **argv)
{
	const char *start = argc > 1 && argv[1][0] != '-' ? argv[1] : "disp";
	rui_init(&argc, &argv, "settings");
	{ RuiJ *s = rui_get("/api/desktop/session"); snprintf(ACCT, sizeof ACCT, "%s", set_jstr(s, "id", "")); snprintf(ME, sizeof ME, "%s", set_jstr(s, "n", "")); snprintf(ROLE, sizeof ROLE, "%s", set_jstr(s, "r", "")); rui_jfree(s); }
	own_css();
	/* the page's window: 860 x 640 with its title bar (34 px): its body 860 x 606; the native frame adds the title bar and edge */
	SW = rui_window("settings", "Settings", 860, 606);
	/* the search bar (.ssbar) */
	SBAR = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); gtk_style_context_add_class(gtk_widget_get_style_context(SBAR), "set-ssbar");
	{ GtkWidget *ov = gtk_overlay_new(); SQ = gtk_entry_new(); gtk_style_context_add_class(gtk_widget_get_style_context(SQ), "set-ssq"); gtk_entry_set_placeholder_text(GTK_ENTRY(SQ), rui_tr("Search settings"));
	  pango_context_set_round_glyph_positions(gtk_widget_get_pango_context(SQ), FALSE); gtk_widget_set_name(SQ, "search");
	  gtk_container_add(GTK_CONTAINER(ov), SQ); g_signal_connect_after(SQ, "draw", G_CALLBACK(ssi_draw), NULL);   /* (the magnifier drawn over the field's own drawing: it takes no clicks) */
	  gtk_box_pack_start(GTK_BOX(SBAR), ov, FALSE, FALSE, 0);
	  g_signal_connect(SQ, "changed", G_CALLBACK(sq_changed), NULL); g_signal_connect(SQ, "key-press-event", G_CALLBACK(sq_key), NULL);
	  g_signal_connect(SQ, "focus-in-event", G_CALLBACK(sq_focus_in), NULL); g_signal_connect(SQ, "button-press-event", G_CALLBACK(sq_press), NULL); g_signal_connect(SQ, "focus-out-event", G_CALLBACK(sq_focus_out), NULL); }
	gtk_box_pack_start(GTK_BOX(SW->body), SBAR, FALSE, FALSE, 0);
	/* .sg: the sidebar and the page, scrolled together when the sidebar is taller than the window; the page scrolls on its own */
	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	SIDE = gtk_drawing_area_new(); gtk_widget_set_can_focus(SIDE, TRUE); gtk_widget_add_events(SIDE, GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK); side_size();
	g_signal_connect(SIDE, "draw", G_CALLBACK(side_draw), NULL); g_signal_connect(SIDE, "motion-notify-event", G_CALLBACK(side_motion), NULL); g_signal_connect(SIDE, "leave-notify-event", G_CALLBACK(side_leave), NULL);
	g_signal_connect(SIDE, "button-press-event", G_CALLBACK(side_press), NULL); g_signal_connect(SIDE, "key-press-event", G_CALLBACK(side_key), NULL); gtk_widget_set_valign(SIDE, GTK_ALIGN_START);
	FX = gtk_fixed_new(); gtk_widget_set_has_window(FX, FALSE); g_signal_connect(FX, "draw", G_CALLBACK(fx_draw), NULL); g_signal_connect(FX, "size-allocate", G_CALLBACK(fx_alloc), NULL);
	SCR = rui_scroller(FX); AREA = gtk_overlay_new(); gtk_container_add(GTK_CONTAINER(AREA), SCR); gtk_widget_set_valign(AREA, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(row), SIDE, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(row), AREA, TRUE, TRUE, 0);
	OUTER = rui_scroller(row); gtk_box_pack_start(GTK_BOX(SW->body), OUTER, TRUE, TRUE, 0);
	{ GList *c = gtk_container_get_children(GTK_CONTAINER(OUTER)); g_signal_connect(c->data, "size-allocate", G_CALLBACK(outer_alloc), NULL); gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(c->data), GTK_POLICY_EXTERNAL, GTK_POLICY_EXTERNAL); g_list_free(c); }
	/* (the page as wide as its view: what it holds laid out in that width, never making the window wider: CSS's block width) */
	{ GList *c = gtk_container_get_children(GTK_CONTAINER(SCR)); gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(c->data), GTK_POLICY_EXTERNAL, GTK_POLICY_EXTERNAL); g_list_free(c); }
	g_signal_connect_swapped(fx_adj(), "value-changed", G_CALLBACK(set_state_soon), NULL);
	/* the search's drop-down (.ssdd), over the window's body */
	DD = gtk_event_box_new(); gtk_widget_set_halign(DD, GTK_ALIGN_START); gtk_widget_set_valign(DD, GTK_ALIGN_START); gtk_widget_set_app_paintable(DD, TRUE); g_signal_connect(DD, "draw", G_CALLBACK(dd_frame), NULL);
	DDA = gtk_drawing_area_new(); gtk_widget_add_events(DDA, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK); g_signal_connect(DDA, "draw", G_CALLBACK(dd_draw), NULL);
	g_signal_connect(DDA, "button-press-event", G_CALLBACK(dd_press), NULL); g_signal_connect(DDA, "motion-notify-event", G_CALLBACK(dd_motion), NULL);
	DDS = gtk_scrolled_window_new(NULL, NULL); gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(DDS), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC); gtk_container_add(GTK_CONTAINER(DDS), DDA);
	{ double b = LK->ssdd_bd_w[0]; gtk_widget_set_margin_top(DDS, R(b + LK->ssdd_pad[0])); gtk_widget_set_margin_bottom(DDS, R(b + LK->ssdd_pad[2])); gtk_widget_set_margin_start(DDS, R(b + LK->ssdd_pad[3])); gtk_widget_set_margin_end(DDS, R(b + LK->ssdd_pad[1])); }
	gtk_container_add(GTK_CONTAINER(DD), DDS); gtk_overlay_add_overlay(GTK_OVERLAY(SW->overlay), DD); gtk_widget_set_no_show_all(DD, TRUE);
	g_signal_connect(SW->win, "delete-event", G_CALLBACK(on_delete), NULL); g_signal_connect(SW->win, "key-press-event", G_CALLBACK(win_key), NULL);
	rui_on_settings(restyle, NULL);
	snprintf(CUR, sizeof CUR, "%s", page_i(start) >= 0 ? start : "disp");
	gtk_widget_show_all(SW->win); gtk_window_set_focus(GTK_WINDOW(SW->win), NULL);
	page_build(); set_state_soon();
	if (getenv("RUDOLF_SET_STATE")) g_timeout_add(400, state_tick, NULL);   /* (the tests: what shows written again often, a menu closed included) */
	gtk_main(); return 0;
}

/* ================================================================== blocks of About's: the logo and the version, the table of details */
typedef struct { char *t, *sub, *mark; int warn; GdkPixbuf *px; } Logo;
static double logo_tb(Logo *l) { return lh_of(18, 700) + LK->msg_mt[0] + fmax(LK->msg_mh[0], lh_of(LK->msg_fs[0], 400)) + (l->mark ? 2 + lh_of(12, l->warn ? 600 : 400) : 0); }
static gboolean logo_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	/* (flex, gap 14px, centred: the logo; the name in bold 18px, the .msg under it at its 10px, devmode59.js's Official or Custom line) */
	Logo *l = d; double z = Z(), tb = logo_tb(l), H = fmax(64, tb);
	if (l->px) { cairo_save(cr); cairo_scale(cr, z, z); gdk_cairo_set_source_pixbuf(cr, l->px, 0, (H - 64) / 2); cairo_paint(cr); cairo_restore(cr); }
	double th = lh_of(18, 700), mh = fmax(LK->msg_mh[0], lh_of(LK->msg_fs[0], 400)), top = (H - tb) / 2;
	fgc(cr, 1); text_draw(cr, l->t, 64 + 14, top, 18, 700, 0, 0, th);
	if (RUI.dark) setc(cr, LK->msg_dark, 1); else fgc(cr, 1);
	text_draw(cr, l->sub, 64 + 14, top + th + LK->msg_mt[0], LK->msg_fs[0], 400, 0, 0, lh_of(LK->msg_fs[0], 400));
	if (l->mark) { if (l->warn) cairo_set_source_rgb(cr, 0xb4 / 255.0, 0x53 / 255.0, 0x09 / 255.0); else fgc(cr, 1); text_draw(cr, l->mark, 64 + 14, top + th + LK->msg_mt[0] + mh + 2, 12, l->warn ? 600 : 400, 0, 0, lh_of(12, l->warn ? 600 : 400)); }
	(void)w; return TRUE;
}
static void logo_free(gpointer p) { Logo *l = p; g_free(l->t); g_free(l->sub); g_free(l->mark); if (l->px) g_object_unref(l->px); g_free(l); }
GtkWidget *s_logo_block(const char *title, const char *sub, const char *mark, int warn)
{
	Logo *l = g_new0(Logo, 1); l->t = g_strdup(title); l->sub = g_strdup(sub); l->mark = mark ? g_strdup(mark) : NULL; l->warn = warn;
	GInputStream *in = g_memory_input_stream_new_from_data(LOGO_PNG, sizeof LOGO_PNG, NULL); GdkPixbuf *p = gdk_pixbuf_new_from_stream(in, NULL, NULL); g_object_unref(in);
	if (p) { l->px = gdk_pixbuf_get_width(p) == 64 ? p : gdk_pixbuf_scale_simple(p, 64, 64, GDK_INTERP_BILINEAR); if (l->px != p) g_object_unref(p); }
	GtkWidget *w = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(w), "set-logo", l, logo_free); g_signal_connect(w, "draw", G_CALLBACK(logo_draw), l);
	gtk_widget_set_size_request(w, -1, R(fmax(64, logo_tb(l)))); return w;
}
/* the table of names and values (.tmt.ab: width 100%, its columns as CSS's automatic table layout makes them) */
typedef struct { GPtrArray *a, *b; double c0, W; } Tab2;
static double word_max(const char *t, double px)
{
	double m = 0; char **w = g_regex_split_simple("\\s+", t, 0, 0); for (int i = 0; w[i]; i++) if (*w[i]) { double x; text_lines(w[i], px, 400, 0, 0, &x); m = fmax(m, x); } g_strfreev(w); return m;
}
static void tab2_cols(Tab2 *t, double W)
{
	double pad = LK->td_pad[1] + LK->td_pad[3], mn[2] = { 0, 0 }, mx[2] = { 0, 0 }, fs = LK->tmt_fs[0];
	for (guint i = 0; i < t->a->len; i++) for (int c = 0; c < 2; c++) { const char *s = g_ptr_array_index(c ? t->b : t->a, i); double x; text_lines(s, fs, 400, 0, 0, &x); mx[c] = fmax(mx[c], x + pad); mn[c] = fmax(mn[c], word_max(s, fs) + pad); }
	if (mx[0] + mx[1] <= W) t->c0 = mx[0] + (W - mx[0] - mx[1]) * mx[0] / fmax(1, mx[0] + mx[1]);
	else { double sp = (mx[0] - mn[0]) + (mx[1] - mn[1]); t->c0 = mn[0] + (W - mn[0] - mn[1]) * (sp > 0 ? (mx[0] - mn[0]) / sp : .5); }
	t->W = W;
}
static double tab2_rowh(Tab2 *t, guint i, double W)
{
	double pad = LK->td_pad[1] + LK->td_pad[3], lh = lh_of(LK->tmt_fs[0], 400);
	int n0 = text_lines(g_ptr_array_index(t->a, i), LK->tmt_fs[0], 400, t->c0 - pad, 0, NULL), n1 = text_lines(g_ptr_array_index(t->b, i), LK->tmt_fs[0], 400, W - t->c0 - pad, 0, NULL);
	return MAX(n0, n1) * lh + LK->td_pad[0] + LK->td_pad[2] + LK->td_bd_w[0];
}
static gboolean tab2_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Tab2 *t = d; double W = gtk_widget_get_allocated_width(w) / Z(), y = 0, pad = LK->td_pad[1] + LK->td_pad[3], lh = lh_of(LK->tmt_fs[0], 400); if (fabs(W - t->W) > .1) tab2_cols(t, W);
	for (guint i = 0; i < t->a->len; i++) { double h = tab2_rowh(t, i, W);
		cairo_save(cr); cairo_scale(cr, Z(), Z()); setc(cr, LK->td_bd, 1); cairo_rectangle(cr, 0, y + h - LK->td_bd_w[0], W, LK->td_bd_w[0]); cairo_fill(cr); cairo_restore(cr);
		for (int c = 0; c < 2; c++) { const char *s = g_ptr_array_index(c ? t->b : t->a, i); double cw = c ? W - t->c0 - pad : t->c0 - pad; int n = text_lines(s, LK->tmt_fs[0], 400, cw, 0, NULL);
			double ty = y + (h - LK->td_bd_w[0] - n * lh) / 2; fgc(cr, 1); text_draw(cr, s, (c ? t->c0 : 0) + LK->td_pad[3], ty, LK->tmt_fs[0], 400, cw, 0, lh); }
		y += h; }
	return TRUE;
}
static void tab2_free(gpointer p) { Tab2 *t = p; g_ptr_array_unref(t->a); g_ptr_array_unref(t->b); g_free(t); }
static void tab2_measure(GtkWidget *w, GtkAllocation *a, gpointer d)
{
	Tab2 *t = d; double W = a->width / Z(); if (fabs(W - t->W) < .1) return; tab2_cols(t, W); double h = 0; for (guint i = 0; i < t->a->len; i++) h += tab2_rowh(t, i, W);
	gtk_widget_set_size_request(w, -1, R(h)); p_relayout();
}
GtkWidget *s_table2(void)
{
	Tab2 *t = g_new0(Tab2, 1); t->a = g_ptr_array_new_with_free_func(g_free); t->b = g_ptr_array_new_with_free_func(g_free);
	GtkWidget *w = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(w), "set-tab2", t, tab2_free); g_signal_connect(w, "draw", G_CALLBACK(tab2_draw), t); g_signal_connect(w, "size-allocate", G_CALLBACK(tab2_measure), t);
	return w;
}
void s_table2_add(GtkWidget *w, const char *a, const char *b)
{
	Tab2 *t = g_object_get_data(G_OBJECT(w), "set-tab2"); g_ptr_array_add(t->a, g_strdup(a)); g_ptr_array_add(t->b, g_strdup(b ? b : ""));
	double W = FX_W; tab2_cols(t, W); double h = 0; for (guint i = 0; i < t->a->len; i++) h += tab2_rowh(t, i, W); gtk_widget_set_size_request(w, -1, R(h));
}

/* ================================================================== drawing helpers for the pages' own blocks */
void s_text(cairo_t *cr, const char *t, double x, double y, double px, int wt, double w, double lh) { text_draw(cr, t, x, y, px, wt, w, 0, lh > 0 ? lh : lh_of(px, wt)); }
int s_lines(const char *t, double px, int wt, double w, double *tw) { return text_lines(t, px, wt, w, 0, tw); }
double s_lh(double px, int wt) { return lh_of(px, wt); }
void s_rrect(cairo_t *cr, double x, double y, double w, double h, double r) { rrect(cr, x, y, w, h, r); }
/* the page's colours by name: fg (the writing), msg (a .msg's: grey in dark), grey (#6b7280), bd, bg2, th (.tmt th's), acr (.acr's line) */
void s_col(cairo_t *cr, const char *w, double a)
{
	if (!strcmp(w, "fg")) fgc(cr, a); else if (!strcmp(w, "msg")) { if (RUI.dark) setc(cr, LK->msg_dark, a); else fgc(cr, a); }
	else if (!strcmp(w, "grey")) { if (RUI.dark) setc(cr, LK->msg_dark, a); else cairo_set_source_rgba(cr, 107 / 255.0, 114 / 255.0, 128 / 255.0, a); }
	else if (!strcmp(w, "acr")) { if (RUI.dark) rui_color(cr, "bd", a); else cairo_set_source_rgba(cr, 0xee / 255.0, 0xf0 / 255.0, 0xf3 / 255.0, a); }
	else if (!strcmp(w, "th")) { if (RUI.dark) cairo_set_source_rgba(cr, 0x1e / 255.0, 0x29 / 255.0, 0x3b / 255.0, a); else rui_color(cr, "bg2", a); }
	else rui_color(cr, w, a);
}
double s_z(void) { return RUI.z; }
