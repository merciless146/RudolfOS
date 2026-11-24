/*
 * rudolf-settings: Mouse and touchpad (devices26.js mousePanel, ptrGet, pointerApply; settings31.js's additions: the note under the
 * pointer speed, Buttons and clicks, msensOf and ptrKeep). 1.6.2, docs/plan-1.6.2.md Part 39. As the page: each change is saved at once
 * (persistS with SC: ptr and msens, mhand, mdbl, mscroll, msingle in bd95set:ACCT) and the pointer's own settings go to rudolfd
 * (pointer?speed=&accel=&tap=&natural=&typing=); Restore defaults puts back PTR_DEF and Buttons and clicks' defaults. The page's
 * grid here is its own (a 200px label column, 10px apart, at most 560px wide). GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings.h"

/* ---- the values (ptrGet: PTR_DEF and S.ptr; DEFS.acc's mhand, mdbl, mscroll, msingle) */
static double SPEED = 1, ACCEL = 1, DBL = 450, SCROLL = 5; static int TAP = 1, NAT = 0, TYP = 1, SINGLE = 0; static char HAND[8] = "right";
static void load(void)
{
	RuiJ *st = set_stored(), *p = rui_jprop(st, "ptr");
	SPEED = rui_jnum(p, "speed", 1); ACCEL = rui_jnum(p, "accel", 1);
	RuiJ *v; TAP = (v = rui_jprop(p, "tap")) && rui_jtype(v) == 1 ? (int)rui_jval(v) : 1; NAT = (v = rui_jprop(p, "natural")) && rui_jtype(v) == 1 ? (int)rui_jval(v) : 0; TYP = (v = rui_jprop(p, "typing")) && rui_jtype(v) == 1 ? (int)rui_jval(v) : 1;
	snprintf(HAND, sizeof HAND, "%s", set_jstr(st, "mhand", "right")); DBL = rui_jnum(st, "mdbl", 450); SCROLL = rui_jnum(st, "mscroll", 5); SINGLE = rui_jbool(st, "msingle");
	rui_jfree(st);
}
static char *num(double v) { char b[40]; g_ascii_formatd(b, sizeof b, "%.10g", v); return g_strdup(b); }
static int msens_of(double sp) { return (int)fmax(1, fmin(10, round(5 + 2.5 * log2(fmax(0.1, sp ? sp : 1))))); }
/* (the page's save, then ptrKeep: S.ptr and S.msens saved, then pointerApply) */
static void ptr_save(void)
{
	char *s = num(SPEED), *a = num(ACCEL), *j = g_strdup_printf("{\"speed\":%s,\"accel\":%s,\"tap\":%s,\"natural\":%s,\"typing\":%s}", s, a, TAP ? "true" : "false", NAT ? "true" : "false", TYP ? "true" : "false"), ms[8];
	snprintf(ms, sizeof ms, "%d", msens_of(SPEED)); const char *K[2] = { "ptr", "msens" }, *V[2] = { j, ms }; set_store(K, V, 2); set_note("persistS ptr %s msens %s", j, ms);
	char p[300]; snprintf(p, sizeof p, "/api/pointer?speed=%s&accel=%s&tap=%d&natural=%d&typing=%d", s, a, TAP, NAT, TYP); int c = rui_post(p, NULL, NULL, 0); set_note("POST %s %d", p, c);
	g_free(s); g_free(a); g_free(j);
}
static void sv(const char *k, const char *json) { const char *K[1] = { k }, *V[1] = { json }; set_store(K, V, 1); set_note("persistS %s %s", k, json); }

/* ---- the page's grid: a row per label, its controls in the second column */
enum { K_RANGE, K_DBL, K_SEL, K_CK };
typedef struct { int kind; char *label, *msg; GtkWidget *area, *ctl, *dbl; char *(*out)(void); double lw; } Row;
static GPtrArray *ROWS;
static double label_h(Row *r) { return s_lines(rui_tr(r->label), 13, 400, 200, NULL) * s_lh(13, 400); }
static double msg_h(Row *r, double w) { return r->msg ? 2 + fmax(18, s_lines(rui_tr(r->msg), 13, 400, w, NULL) * s_lh(13, 400)) : 0; }
static double ctl_h(Row *r, double w)
{
	switch (r->kind) { case K_RANGE: return 21 + msg_h(r, w); case K_DBL: return 21 + 6 + 25; case K_SEL: return 25; default: return 19; }
}
static double row_lay(void *d, double x, double y, double W)
{
	Row *r = d; double w = fmin(W, 560), cw = w - 210, cx = x + 210, lh = label_h(r), ch = ctl_h(r, cw), h = fmax(lh, ch), cy = y + (h - ch) / 2;
	p_place(r->area, x, y, w, h);
	if (r->kind == K_RANGE || r->kind == K_DBL) { p_place(r->ctl, cx + 2, cy + 2, 220, 16); if (r->dbl) { double sp, a1; s_lines("a a", 13, 400, 0, &sp); s_lines("aa", 13, 400, 0, &a1);
		double tw; s_lines("📁", 13, 400, 0, &tw); double t2; s_lines(gtk_widget_get_name(r->dbl), 13, 400, 0, &t2); p_place(r->dbl, cx, cy + 27, 16 + tw + (sp - a1) + t2 + 2, 25); } }
	else if (r->kind == K_SEL) p_place(r->ctl, cx, cy, cw, 25);
	else { rui_check_wrap(r->ctl, cw, 19); p_place(r->ctl, cx, cy, cw, 19); }
	return h;
}
static gboolean row_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Row *r = d; double z = s_z(), W = gtk_widget_get_allocated_width(w) / z, H = gtk_widget_get_allocated_height(w) / z, lh = s_lh(13, 400), cw = W - 210, ch = ctl_h(r, cw), cy = (H - ch) / 2;
	s_col(cr, "fg", 1); s_text(cr, rui_tr(r->label), 0, (H - label_h(r)) / 2, 13, 400, 200, lh);
	if (r->out) { char *o = r->out(); double sp, a1; s_lines("a a", 13, 400, 0, &sp); s_lines("aa", 13, 400, 0, &a1); s_text(cr, o, 210 + 2 + 220 + 2 + (sp - a1), cy + 6, 13, 400, 0, lh); g_free(o); }
	if (r->msg) { s_col(cr, "msg", 1); s_text(cr, rui_tr(r->msg), 210, cy + 21 + 2, 13, 400, cw, lh); }
	return TRUE;
}
static void row_free(gpointer p) { Row *r = p; g_free(r->label); g_free(r->msg); g_free(r); }
static Row *row(int kind, const char *label, GtkWidget *ctl, const char *msg, char *(*out)(void))
{
	Row *r = g_new0(Row, 1); r->kind = kind; r->label = g_strdup(label); r->msg = msg ? g_strdup(msg) : NULL; r->ctl = ctl; r->out = out;
	r->area = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(r->area), "row", r, row_free); g_signal_connect(r->area, "draw", G_CALLBACK(row_draw), r); p_put(r->area); p_put(ctl);
	g_ptr_array_add(ROWS, r); return r;
}
static void grid(Row **rs, int n, double mt, double mb)
{
	for (int i = 0; i < n; i++) { p_custom(row_lay, rs[i], i ? 0 : mt, i < n - 1 ? 10 : mb); p_custom_find(rs[i]->label, 0); }
}
/* (a plain <h4>: 13px bold, 16px above, 6px under; found by Search as a heading) */
static double h4_lay(void *d, double x, double y, double w) { p_place(d, x, y, w, s_lh(13, 700)); return s_lh(13, 700); }
static gboolean h4_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)w; s_col(cr, "fg", 1); s_text(cr, d, 0, 0, 13, 700, 0, 0); return TRUE; }
static void h4(const char *t) { GtkWidget *a = gtk_drawing_area_new(); char *tt = g_strdup(rui_tr(t)); g_object_set_data_full(G_OBJECT(a), "t", tt, g_free); g_signal_connect(a, "draw", G_CALLBACK(h4_draw), tt); p_put(a); p_custom(h4_lay, a, 16, 6); p_custom_find(t, 1); }

/* ---- the outputs */
static char *o_speed(void) { return g_strdup_printf("%.1f×", SPEED); }
static char *o_accel(void) { return ACCEL == 1 ? g_strdup(rui_tr("off")) : g_strdup_printf("%.2f×", ACCEL); }
static char *o_dbl(void) { return g_strdup_printf("%.0f ms%s", DBL, DBL == 450 ? rui_tr(" (default)") : ""); }
static char *o_scroll(void) { return SCROLL == 5 ? g_strdup(rui_tr("Default")) : g_strdup_printf("×%.2f", pow(2, ((SCROLL ? SCROLL : 5) - 5) / 2.5)); }
static void redraw_rows(void) { for (guint i = 0; ROWS && i < ROWS->len; i++) gtk_widget_queue_draw(((Row *)g_ptr_array_index(ROWS, i))->area); set_state_soon(); }

/* ---- the changes */
static void on_speed(double v, void *d) { (void)d; SPEED = v; ptr_save(); redraw_rows(); }
static void on_accel(double v, void *d) { (void)d; ACCEL = v; ptr_save(); redraw_rows(); }
static void on_tap(int on, void *d) { (void)d; TAP = on; ptr_save(); }
static void on_nat(int on, void *d) { (void)d; NAT = on; ptr_save(); }
static void on_typ(int on, void *d) { (void)d; TYP = on; ptr_save(); }
static void on_hand(const char *v, void *d) { (void)d; snprintf(HAND, sizeof HAND, "%s", v); char *j = g_strdup_printf("\"%s\"", v); sv("mhand", j); g_free(j); }
static void on_dbl(double v, void *d) { (void)d; DBL = v; char *j = num(v); sv("mdbl", j); g_free(j); redraw_rows(); }
static void on_scroll(double v, void *d) { (void)d; SCROLL = v; char *j = num(v); sv("mscroll", j); g_free(j); redraw_rows(); }
static void on_single(int on, void *d) { (void)d; SINGLE = on; sv("msingle", on ? "true" : "false"); }
static void on_defaults(GtkButton *b, gpointer d)
{
	(void)b; (void)d; SPEED = 1; ACCEL = 1; TAP = 1; NAT = 0; TYP = 1; ptr_save();
	const char *K[4] = { "mhand", "mdbl", "mscroll", "msingle" }, *V[4] = { "\"right\"", "450", "5", "false" }; set_store(K, V, 4); set_note("persistS mouse defaults"); set_show("mouse");
}
/* (the page's .dbltest: a folder to double-click, its words as it opens and closes) */
static int OPENED;
static gboolean dbl_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)d; double z = s_z(), W = gtk_widget_get_allocated_width(w) / z, H = gtk_widget_get_allocated_height(w) / z;
	cairo_save(cr); cairo_scale(cr, z, z); s_col(cr, "bd", 1); cairo_set_line_width(cr, 1); s_rrect(cr, .5, .5, W - 1, H - 1, 5.5); cairo_stroke(cr); cairo_restore(cr);
	s_col(cr, "fg", 1); char *t = g_strdup_printf("%s %s", OPENED ? "📂" : "📁", gtk_widget_get_name(w)); s_text(cr, t, 9, 5, 13, 400, 0, 0); g_free(t); return TRUE;
}
static void dbl_toggle(GtkWidget *w) { OPENED = !OPENED; gtk_widget_set_name(w, rui_tr(OPENED ? "Opened! Double-click again to close" : "Double-click this folder to test")); set_note("dbltest %d", OPENED); p_relayout(); gtk_widget_queue_draw(w); }
static gboolean dbl_press(GtkWidget *w, GdkEventButton *e, gpointer d) { (void)d; gtk_widget_grab_focus(w); if (e->type == GDK_2BUTTON_PRESS && e->button == 1) dbl_toggle(w); return TRUE; }
static gboolean dbl_key(GtkWidget *w, GdkEventKey *e, gpointer d) { (void)d; if (e->keyval == GDK_KEY_Return || e->keyval == GDK_KEY_KP_Enter) { dbl_toggle(w); return TRUE; } return FALSE; }
static double zero_lay(void *d, double x, double y, double w) { (void)d; (void)x; (void)y; (void)w; return 0; }
/* (label.chk: a check box and its words in a line of the page's writing, one under another with <br>: 19px lines, the box 3px down) */
static double chk_lay(void *d, double x, double y, double w) { (void)w; p_place(d, x, y + 3, 0, 0); return 19; }
static void chk(const char *name, const char *t, int on, void (*cb)(int, void *)) { GtkWidget *c = rui_check(t, on, cb, NULL); p_put(c); p_label_named(name, c); p_custom(chk_lay, c, 0, 0); p_custom_find(t, 0); }
static double btn_lay(void *d, double x, double y, double w) { (void)w; p_place(d, x, y, 0, 0); return p_nat_h(d); }

void page_mouse(void)
{
	load(); if (ROWS) g_ptr_array_unref(ROWS); ROWS = g_ptr_array_new(); OPENED = 0;
	p_h3("Mouse and touchpad");
	{ GtkWidget *sp = s_range(0.3, 3, 0.1, SPEED, on_speed, NULL), *ac = s_range(1, 3, 0.25, ACCEL, on_accel, NULL); p_label_named("ptsp", sp); p_label_named("ptac", ac);
		Row *rs[2] = { row(K_RANGE, "Pointer speed", sp, "Also used by games that capture the mouse (Quake, 3D programs).", o_speed), row(K_RANGE, "Acceleration (faster when you move quickly)", ac, NULL, o_accel) };
		grid(rs, 2, 0, 10); }
	h4("Touchpad");
	{ RuiJ *dv = rui_get("/api/devices"), *in = rui_jprop(dv, "inputs"); GString *t = g_string_new(NULL); int n = 0;
		if (rui_jtype(in) == 4) { for (int i = 0; i < rui_jlen(in); i++) { RuiJ *x = rui_jat(in, i); if (!g_strcmp0(rui_jstr(x, "kind"), "touchpad")) g_string_append_printf(t, "%s%s", n++ ? ", " : "Found: ", set_jstr(x, "name", "")); }
			if (!n) g_string_assign(t, "No touchpad found on this computer. These settings apply when one is connected."); }
		p_msg(t->str, 0); g_string_free(t, TRUE); rui_jfree(dv); }
	chk("pttap", "Tap to click (two fingers: right click, three: middle click)", TAP, on_tap); chk("ptnat", "Natural scrolling (content follows your fingers)", NAT, on_nat); chk("pttyp", "Ignore the touchpad while typing", TYP, on_typ);
	p_msg_m("Two fingers scroll up, down and sideways. On a clickpad, press with two fingers or in the bottom-right corner for a right click.", 0, 10, 13);
	h4("Buttons and clicks");
	{ const char *hv[2] = { "right", "left" }, *hn[2] = { "Left button (right-handed)", "Right button (left-handed)" };
		GtkWidget *hs = s_select(hv, hn, 2, HAND, on_hand, NULL), *dr = s_range(200, 900, 50, DBL, on_dbl, NULL), *sr = s_range(1, 10, 1, SCROLL, on_scroll, NULL), *ck = rui_check("Open files and folders with a single click", SINGLE, on_single, NULL);
		p_label_named("mpb", hs); p_label_named("mpd", dr); p_label_named("mps", sr); p_label_named("mpo", ck);
		Row *rd = row(K_DBL, "Double-click speed", dr, NULL, o_dbl);
		GtkWidget *dt = gtk_drawing_area_new(); gtk_widget_set_name(dt, rui_tr("Double-click this folder to test")); gtk_widget_set_can_focus(dt, TRUE); gtk_widget_add_events(dt, GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK);
		g_signal_connect(dt, "draw", G_CALLBACK(dbl_draw), NULL); g_signal_connect(dt, "button-press-event", G_CALLBACK(dbl_press), NULL); g_signal_connect(dt, "key-press-event", G_CALLBACK(dbl_key), NULL); p_put(dt); rd->dbl = dt; p_label_named("dbltest", dt);
		Row *rs[4] = { row(K_SEL, "Primary button", hs, NULL, NULL), rd, row(K_RANGE, "Scroll speed", sr, NULL, o_scroll), row(K_CK, "Opening items", ck, NULL, NULL) };
		grid(rs, 4, 0, 0); p_custom(zero_lay, NULL, 0, 10); p_custom_find("Open files and folders with a single click", 0); }
	p_msg_m("Left-handed mode swaps the buttons on the desktop, windows, menus, lists and icons. Inside text boxes, sliders and games the browser keeps the physical buttons. Shift + right-click shows the browser's own menu.", 0, 10, 13);
	GtkWidget *df = rui_button("Restore defaults", 0); g_signal_connect(df, "clicked", G_CALLBACK(on_defaults), NULL); p_put(df); p_label_named("ptdef", df); p_custom(btn_lay, df, 13, 13);
}
