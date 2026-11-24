/*
 * rudolf-settings: Keyboard (settings25.js kbdPanel; a11y25.js's layouts, LAYOUTS and kblList; shell25.js's shortcuts, SHORT, kbMap,
 * kbOf and comboOf). 1.6.2, docs/plan-1.6.2.md Part 39. As the page: the layouts are the account's list (bd95kbl:ACCT, the first the
 * one used), the input method, the Compose key and Clipboard history are saved at once (persistS with SC: bd95set:ACCT), and each
 * shortcut changed is the account's map (bd95kb:ACCT): Change, then the keys (the native shell leaves every key to Settings
 * meanwhile: "kbcap 1"), a combination another shortcut had taken from it, Default, Reset all shortcuts. GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
#include "settings-gen.h"
#pragma GCC diagnostic pop

static char *kkey(const char *what) { return g_strdup_printf("%s:%s", what, ACCT); }
static const char *lay_name(const char *k) { for (guint i = 0; i < G_N_ELEMENTS(LAYK); i++) if (!strcmp(LAYK[i][0], k)) return LAYK[i][1]; return k; }
/* (kblList: the account's layouts, ['us'] when none) */
static GPtrArray *kbl(void)
{
	char *k = kkey("bd95kbl"); RuiJ *o = set_ls_get(k), *l = rui_jprop(o, "x"); g_free(k); GPtrArray *a = g_ptr_array_new_with_free_func(g_free);
	if (rui_jtype(l) == 4) for (int i = 0; i < rui_jlen(l); i++) { const char *s = rui_jsval(rui_jat(l, i)); if (s) g_ptr_array_add(a, g_strdup(s)); }
	else g_ptr_array_add(a, g_strdup("us"));
	rui_jfree(o); return a;
}
static void kbl_save(GPtrArray *a)
{
	GString *b = g_string_new("["); for (guint i = 0; i < a->len; i++) { char *e = set_jesc(g_ptr_array_index(a, i)); g_string_append_printf(b, "%s\"%s\"", i ? "," : "", e); g_free(e); } g_string_append_c(b, ']');
	char *k = kkey("bd95kbl"); set_ls_set(k, b->str); set_note("kbl %s", b->str); g_free(k); g_string_free(b, TRUE);
}
/* (kbMap: the shortcuts changed, {id: combination}) */
static RuiJ *KBM;
static void kbm_load(void) { rui_jfree(KBM); char *k = kkey("bd95kb"); KBM = set_ls_get(k); g_free(k); }
static const char *kbm_get(const char *id) { RuiJ *m = rui_jprop(KBM, "x"); return rui_jtype(rui_jprop(m, id)) == 3 ? rui_jsval(rui_jprop(m, id)) : NULL; }
static int kbm_has(const char *id) { const char *v = kbm_get(id); return v && *v; }   /* (the page's kbMap()[s.id]: a truthy one) */
static const char *kb_of(int i) { const char *v = kbm_get(SHORTS[i][0]); return v && *v ? v : SHORTS[i][2]; }
/* the map written again: each key as it was, `id` set to `v` (NULL: deleted), `id2` to "" (the shortcut it took the keys from) */
static void kbm_save(const char *id, const char *v, const char *id2, int clear)
{
	RuiJ *m = rui_jprop(KBM, "x"); GString *b = g_string_new("{"); int n = 0, sid = 0, sid2 = 0;
	for (int i = 0; !clear && i < rui_jlen(m); i++) { const char *k = rui_jkey(m, i); const char *x = rui_jsval(rui_jprop(m, k));
		if (id && !strcmp(k, id)) { sid = 1; if (!v) continue; x = v; } else if (id2 && !strcmp(k, id2)) { sid2 = 1; x = ""; }
		char *ek = set_jesc(k), *ex = set_jesc(x ? x : ""); g_string_append_printf(b, "%s\"%s\":\"%s\"", n++ ? "," : "", ek, ex); g_free(ek); g_free(ex); }
	if (!clear && id && v && !sid) { char *ek = set_jesc(id), *ex = set_jesc(v); g_string_append_printf(b, "%s\"%s\":\"%s\"", n++ ? "," : "", ek, ex); g_free(ek); g_free(ex); }
	if (!clear && id2 && !sid2) { char *ek = set_jesc(id2); g_string_append_printf(b, "%s\"%s\":\"\"", n++ ? "," : "", ek); g_free(ek); }
	g_string_append_c(b, '}'); char *k = kkey("bd95kb"); set_ls_set(k, b->str); set_note("kbmap %s", b->str); g_free(k); g_string_free(b, TRUE); kbm_load();
}

/* ================================================================== Layouts */
typedef struct { GtkWidget *area, *up, *rm; char *text; } Acr;
static double acr_lay(void *d, double x, double y, double w)
{
	/* (.acr: a grid of 1fr 110px 90px, 8px apart, 4px above and below, its line under it) */
	Acr *a = d; double bh = p_nat_h(a->up), h = 4 + bh + 4 + 1;
	p_place(a->area, x, y, w, h); p_place(a->up, x + w - 90 - 8 - 110, y + 4, 110, 0); p_place(a->rm, x + w - 90, y + 4, 90, 0); return h;
}
static gboolean acr_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Acr *a = d; double z = s_z(), W = gtk_widget_get_allocated_width(w) / z, H = gtk_widget_get_allocated_height(w) / z, lh = s_lh(13, 400);
	s_col(cr, "fg", 1); s_text(cr, a->text, 0, (H - 1 - lh) / 2, 13, 400, W - 216, lh);
	cairo_save(cr); cairo_scale(cr, z, z); s_col(cr, "acr", 1); cairo_rectangle(cr, 0, H - 1, W, 1); cairo_fill(cr); cairo_restore(cr); return TRUE;
}
static void acr_free(gpointer p) { Acr *a = p; g_free(a->text); g_free(a); }
static void kbl_up(GtkButton *b, gpointer d) { (void)b; int i = GPOINTER_TO_INT(d); if (!i) return; GPtrArray *a = kbl(); if (i < (int)a->len) { gpointer t = a->pdata[i - 1]; a->pdata[i - 1] = a->pdata[i]; a->pdata[i] = t; kbl_save(a); } g_ptr_array_unref(a); set_show("kbd"); }
static void kbl_rm(GtkButton *b, gpointer d)
{
	(void)b; int i = GPOINTER_TO_INT(d); GPtrArray *a = kbl(); if (i < (int)a->len) { char *k = g_strdup(g_ptr_array_index(a, i)); for (guint j = a->len; j-- > 0;) if (!strcmp(g_ptr_array_index(a, j), k)) g_ptr_array_remove_index(a, j); g_free(k);
		if (a->len) kbl_save(a); } g_ptr_array_unref(a); set_show("kbd");
}
static GtkWidget *LASEL;
static void kbl_add(GtkButton *b, gpointer d)
{
	(void)b; (void)d; const char *v = s_select_get(LASEL); GPtrArray *a = kbl(); int have = 0; for (guint i = 0; i < a->len; i++) if (!strcmp(g_ptr_array_index(a, i), v)) have = 1;
	if (!have) { g_ptr_array_add(a, g_strdup(v)); kbl_save(a); } g_ptr_array_unref(a); set_show("kbd");
}

/* ================================================================== Input methods */
static void sv(const char *k, const char *json) { const char *K[1] = { k }, *V[1] = { json }; set_store(K, V, 1); set_note("persistS %s %s", k, json); }
static void on_ime(const char *v, void *d) { (void)d; char *j = g_strdup_printf("\"%s\"", v); sv("ime", j); g_free(j); }
static void on_cmp(const char *v, void *d) { (void)d; char *j = g_strdup_printf("\"%s\"", v); sv("compose", j); g_free(j); }
static void on_ch(int on, void *d) { (void)d; sv("cliphist", on ? "true" : "false"); }

/* ================================================================== Shortcuts (a .tmt of Action, Keys and its buttons) */
#define NSH ((int)G_N_ELEMENTS(SHORTS))
typedef struct { GtkWidget *area, *ch[NSH], *df[NSH]; double cw[3], rh[NSH], hh; char *act[NSH], *keys[NSH]; } KbT;
static KbT *KT; static int CAP = -1;
static const char *const KTH[3] = { "Action", "Keys", "" };
static double wmin(const char *t, int wt) { double m = 0; char **w = g_regex_split_simple("\\s+", t, 0, 0); for (int i = 0; w[i]; i++) if (*w[i]) { double x; s_lines(w[i], 13, wt, 0, &x); m = fmax(m, x); } g_strfreev(w); return m; }
static double kbd_w(const char *k) { char *t = g_strdup_printf("\x01%s\x02", k); double w; s_lines(t, 13, 400, 0, &w); g_free(t); return w; }
static double btns_w(int r) { double w = p_nat_w(KT->ch[r]); if (KT->df[r]) { double sp; s_lines("a a", 13, 400, 0, &sp); double a1; s_lines("aa", 13, 400, 0, &a1); w += sp - a1 + p_nat_w(KT->df[r]); } return w; }
static double kt_lay(void *d, double x, double y, double W)
{
	/* (the table's automatic layout, as Chromium's: each column its widest, the room left shared by those widths; narrower than that,
	   each between its narrowest and its widest) */
	KbT *t = d; double mn[3], mx[3], pad = 12, lh = s_lh(13, 400);
	for (int c = 0; c < 3; c++) { double hw; s_lines(rui_tr(KTH[c]), 13, 700, 0, &hw); mx[c] = hw + pad; mn[c] = wmin(rui_tr(KTH[c]), 700) + pad; }
	for (int r = 0; r < NSH; r++) { double tw; s_lines(t->act[r], 13, 400, 0, &tw); mx[0] = fmax(mx[0], tw + pad); mn[0] = fmax(mn[0], wmin(t->act[r], 400) + pad);
		double kw = kbd_w(t->keys[r]); mx[1] = fmax(mx[1], kw + pad); mn[1] = fmax(mn[1], kw + pad);
		double bw = btns_w(r); mx[2] = fmax(mx[2], bw + pad); mn[2] = fmax(mn[2], (t->df[r] ? fmax(p_nat_w(t->ch[r]), p_nat_w(t->df[r])) : bw) + pad); }
	double smx = mx[0] + mx[1] + mx[2], smn = mn[0] + mn[1] + mn[2];
	if (smx <= W) for (int c = 0; c < 3; c++) t->cw[c] = mx[c] + (W - smx) * mx[c] / smx;
	else { double sp = smx - smn; for (int c = 0; c < 3; c++) t->cw[c] = mn[c] + (W - smn) * (sp > 0 ? (mx[c] - mn[c]) / sp : 1 / 3.0); }
	t->hh = 4 + s_lh(13, 700) + 4 + .5; double yy = t->hh;
	for (int r = 0; r < NSH; r++) { double bh = p_nat_h(t->ch[r]), bx = x + t->cw[0] + t->cw[1] + 6, two = t->df[r] && btns_w(r) > t->cw[2] - pad;
		double h = fmax(s_lines(t->act[r], 13, 400, t->cw[0] - pad, NULL) * lh, two ? 2 * bh : bh); t->rh[r] = h + 8 + 1;
		double by = y + yy + (t->rh[r] - 1 - (two ? 2 * bh : bh)) / 2; p_place(t->ch[r], bx, by, 0, 0);
		if (t->df[r]) { if (two) p_place(t->df[r], bx, by + bh, 0, 0); else p_place(t->df[r], bx + btns_w(r) - p_nat_w(t->df[r]), by, 0, 0); }
		yy += t->rh[r]; }
	yy += .5; p_place(t->area, x, y, W, yy); return yy;
}
static gboolean kt_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	KbT *t = d; double z = s_z(), W = gtk_widget_get_allocated_width(w) / z, lh = s_lh(13, 400), x = 0;
	cairo_save(cr); cairo_scale(cr, z, z); s_col(cr, "th", 1); cairo_rectangle(cr, 0, 0, W, t->hh); cairo_fill(cr); s_col(cr, "bd", 1); cairo_rectangle(cr, 0, t->hh - .5, W, 1); cairo_fill(cr); cairo_restore(cr);
	s_col(cr, "fg", 1); for (int c = 0; c < 3; c++) { s_text(cr, rui_tr(KTH[c]), x + 6, 4, 13, 700, 0, 0); x += t->cw[c]; }
	double y = t->hh;
	for (int r = 0; r < NSH; r++) { double h = t->rh[r];
		cairo_save(cr); cairo_scale(cr, z, z); cairo_set_source_rgba(cr, 0, 0, 0, .05); cairo_rectangle(cr, 0, y + h - 1, W, 1); cairo_fill(cr); cairo_restore(cr);
		double th = s_lines(t->act[r], 13, 400, t->cw[0] - 12, NULL) * lh; s_col(cr, "fg", 1); s_text(cr, t->act[r], 6, y + (h - 1 - th) / 2, 13, 400, t->cw[0] - 12, lh);
		char *k = g_strdup_printf("\x01%s\x02", t->keys[r]); s_text(cr, k, t->cw[0] + 6, y + (h - 1 - lh) / 2, 13, 400, 0, lh); g_free(k);
		y += h; }
	return TRUE;
}
static void kt_free(gpointer p) { KbT *t = p; for (int r = 0; r < NSH; r++) { g_free(t->act[r]); g_free(t->keys[r]); } if (KT == t) KT = NULL; g_free(t); }
static char *super(const char *k) { GString *s = g_string_new(NULL); for (const char *p = k; *p;) { if (!strncmp(p, "Meta", 4)) { g_string_append(s, "Super"); p += 4; } else g_string_append_c(s, *p++); } return g_string_free(s, FALSE); }

/* ---- the keys pressed (comboOf: Ctrl+, Alt+, Shift+, Meta+, then the key as a page names it) */
static char *key_name(guint kv)
{
	static const struct { guint v; const char *n; } K[] = { { GDK_KEY_Escape, "Escape" }, { GDK_KEY_Tab, "Tab" }, { GDK_KEY_ISO_Left_Tab, "Tab" }, { GDK_KEY_Return, "Enter" }, { GDK_KEY_KP_Enter, "Enter" },
		{ GDK_KEY_BackSpace, "Backspace" }, { GDK_KEY_Delete, "Delete" }, { GDK_KEY_KP_Delete, "Delete" }, { GDK_KEY_Insert, "Insert" }, { GDK_KEY_Home, "Home" }, { GDK_KEY_End, "End" },
		{ GDK_KEY_Page_Up, "PageUp" }, { GDK_KEY_Page_Down, "PageDown" }, { GDK_KEY_Left, "ArrowLeft" }, { GDK_KEY_Right, "ArrowRight" }, { GDK_KEY_Up, "ArrowUp" }, { GDK_KEY_Down, "ArrowDown" },
		{ GDK_KEY_Print, "PrintScreen" }, { GDK_KEY_Menu, "ContextMenu" }, { GDK_KEY_Pause, "Pause" }, { GDK_KEY_Caps_Lock, "CapsLock" }, { GDK_KEY_Num_Lock, "NumLock" }, { GDK_KEY_Scroll_Lock, "ScrollLock" },
		{ GDK_KEY_ISO_Level3_Shift, "AltGraph" }, { GDK_KEY_space, "Space" } };
	static const guint MOD[] = { GDK_KEY_Control_L, GDK_KEY_Control_R, GDK_KEY_Alt_L, GDK_KEY_Alt_R, GDK_KEY_Shift_L, GDK_KEY_Shift_R, GDK_KEY_Super_L, GDK_KEY_Super_R, GDK_KEY_Meta_L, GDK_KEY_Meta_R, GDK_KEY_Hyper_L, GDK_KEY_Hyper_R };
	for (guint i = 0; i < G_N_ELEMENTS(MOD); i++) if (kv == MOD[i]) return g_strdup("");
	for (guint i = 0; i < G_N_ELEMENTS(K); i++) if (kv == K[i].v) return g_strdup(K[i].n);
	if (kv >= GDK_KEY_F1 && kv <= GDK_KEY_F35) return g_strdup_printf("F%u", kv - GDK_KEY_F1 + 1);
	gunichar u = gdk_keyval_to_unicode(kv); if (u >= 0x20 && u != 0x7f) { char b[8] = ""; g_unichar_to_utf8(g_unichar_toupper(u), b); return g_strdup(b); }
	const char *n = gdk_keyval_name(kv); return g_strdup(n ? n : "Unidentified");
}
static guint CAPSIG;
static void cap_done(void)
{
	if (CAPSIG) { g_signal_handler_disconnect(SW->win, CAPSIG); CAPSIG = 0; } CAP = -1; rui_shell(SW, "kbcap 0"); set_note("kbcap 0"); set_show("kbd");
}
static gboolean cap_key(GtkWidget *w, GdkEvent *ev, gpointer d)
{
	(void)w; (void)d; if (ev->type != GDK_KEY_PRESS || CAP < 0) return FALSE; GdkEventKey *e = &ev->key; guint m = e->state;
	int ctrl = m & GDK_CONTROL_MASK, alt = m & GDK_MOD1_MASK, shift = m & GDK_SHIFT_MASK, meta = m & (GDK_SUPER_MASK | GDK_MOD4_MASK | GDK_META_MASK);
	if (e->keyval == GDK_KEY_Escape && !ctrl && !alt && !meta && !shift) { cap_done(); return TRUE; }
	char *k = key_name(e->keyval); if (!*k) { g_free(k); return TRUE; }
	char *cmb = g_strdup_printf("%s%s%s%s%s", ctrl ? "Ctrl+" : "", alt ? "Alt+" : "", shift ? "Shift+" : "", meta ? "Meta+" : "", k);
	int fkey = k[0] == 'F' && k[1] && strspn(k + 1, "0123456789") == strlen(k + 1);
	if (!ctrl && !alt && !meta && !fkey && strcmp(k, "PrintScreen") && strcmp(k, "Escape")) { if (KT) { gtk_button_set_label(GTK_BUTTON(KT->ch[CAP]), rui_tr("Use Ctrl, Alt or Super")); p_relayout(); } set_note("kbcap refused %s", cmb); g_free(k); g_free(cmb); return TRUE; }
	int clash = -1; for (int i = 0; i < NSH; i++) if (i != CAP && !strcmp(kb_of(i), cmb)) clash = i;
	set_note("kbset %s %s", SHORTS[CAP][0], cmb); kbm_save(SHORTS[CAP][0], cmb, clash >= 0 ? SHORTS[clash][0] : NULL, 0);
	cap_done(); if (clash >= 0) { char *t = g_strdup_printf("“%s” no longer has a shortcut.", SHORTS[clash][1]); set_notify("Shortcut moved", t, 0); g_free(t); }
	g_free(k); g_free(cmb); return TRUE;
}
static void kb_change(GtkButton *b, gpointer d)
{
	if (CAP >= 0) return; CAP = GPOINTER_TO_INT(d); gtk_button_set_label(b, rui_tr("Press keys…")); p_relayout();
	CAPSIG = g_signal_connect(SW->win, "event", G_CALLBACK(cap_key), NULL); rui_shell(SW, "kbcap 1"); set_note("kbcap 1 %s", SHORTS[CAP][0]);
}
static void kb_default(GtkButton *b, gpointer d) { (void)b; kbm_save(SHORTS[GPOINTER_TO_INT(d)][0], NULL, NULL, 0); set_show("kbd"); }
static void kb_reset(GtkButton *b, gpointer d) { (void)b; (void)d; kbm_save(NULL, NULL, NULL, 1); set_show("kbd"); }
static double kbr_lay(void *d, double x, double y, double w) { (void)w; p_place(d, x, y, 0, 0); return p_nat_h(d); }

void page_kbd(void)
{
	RuiJ *st = set_stored(); kbm_load();
	p_h3("Keyboard"); p_h4("Layouts");
	GPtrArray *l = kbl();
	for (guint i = 0; i < l->len; i++) { Acr *a = g_new0(Acr, 1); const char *k = g_ptr_array_index(l, i); a->text = g_strdup_printf("%s%s", i ? "" : "★ ", lay_name(k));
		a->area = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(a->area), "acr", a, acr_free); g_signal_connect(a->area, "draw", G_CALLBACK(acr_draw), a); p_put(a->area);
		a->up = rui_button("▲", 0); g_signal_connect(a->up, "clicked", G_CALLBACK(kbl_up), GINT_TO_POINTER(i)); p_put(a->up); a->rm = rui_button("Remove", 0); g_signal_connect(a->rm, "clicked", G_CALLBACK(kbl_rm), GINT_TO_POINTER(i)); p_put(a->rm);
		char n[16]; snprintf(n, sizeof n, "kbup%u", i); p_label_named(n, a->up); snprintf(n, sizeof n, "kbrm%u", i); p_label_named(n, a->rm); p_custom(acr_lay, a, 0, 0); }
	g_ptr_array_unref(l);
	{ Cell *c = p_r2("Add a layout"); const char *v[G_N_ELEMENTS(LAYK)], *n[G_N_ELEMENTS(LAYK)]; for (guint i = 0; i < G_N_ELEMENTS(LAYK); i++) { v[i] = LAYK[i][0]; n[i] = LAYK[i][1]; }
		LASEL = s_select(v, n, G_N_ELEMENTS(LAYK), "us", NULL, NULL); c_add(c, LASEL, 0, 0, 0); p_label_named("kbla", LASEL);
		GtkWidget *ad = rui_button("Add", 0); g_signal_connect(ad, "clicked", G_CALLBACK(kbl_add), NULL); double sp, a1; s_lines("a a", 13, 400, 0, &sp); s_lines("aa", 13, 400, 0, &a1); c_add(c, ad, sp - a1, 1, 0); p_label_named("kbadd", ad); }
	p_msg("Switch with Super+Space or the indicator on the taskbar. The layout changes the real keyboard map.", 0);
	p_h4("Input methods");
	{ const char *iv[4] = { "off", "ja", "ko", "zh" }, *in[4] = { "None", "Japanese (romaji → kana)", "Korean (2-set Hangul)", "Chinese (pinyin)" };
		Cell *c = p_r2("Input method"); GtkWidget *s = s_select(iv, in, 4, set_jstr(st, "ime", "off"), on_ime, NULL); c_add(c, s, 0, 0, 1); p_label_named("kbime", s); p_r2_more(c);
		const char *cv[5] = { "off", "RightAlt", "Menu", "CapsLock", "ScrollLock" }, *cn[5] = { "Off", "Right Alt", "Menu key", "Caps Lock", "Scroll Lock" };
		Cell *c2 = p_r2("Compose key"); GtkWidget *s2 = s_select(cv, cn, 5, set_jstr(st, "compose", "off"), on_cmp, NULL); c_add(c2, s2, 0, 0, 1); p_label_named("kbcmp", s2); p_r2_more(c2);
		RuiJ *chv = rui_jprop(st, "cliphist"); int ch = !(rui_jtype(chv) == 1 && !rui_jval(chv));
		Cell *c3 = p_r2("Clipboard history"); p_label_named("kbch", c_check(c3, "Keep the last 25 things I copy (Super+V)", ch, on_ch, NULL)); }
	p_h4("Shortcuts");
	p_msg("Click “Change”, then press the new key combination. Esc cancels.", 0);
	KbT *t = g_new0(KbT, 1); KT = t; t->area = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(t->area), "kt", t, kt_free); g_signal_connect(t->area, "draw", G_CALLBACK(kt_draw), t); p_put(t->area);
	for (int r = 0; r < NSH; r++) { t->act[r] = g_strdup_printf("%s — %s", SHORTS[r][3], SHORTS[r][1]); t->keys[r] = super(kb_of(r));
		t->ch[r] = rui_button(CAP == r ? "Press keys…" : "Change", 0); g_signal_connect(t->ch[r], "clicked", G_CALLBACK(kb_change), GINT_TO_POINTER(r)); p_put(t->ch[r]);
		char n[16]; snprintf(n, sizeof n, "kbc%d", r); p_label_named(n, t->ch[r]);
		if (kbm_has(SHORTS[r][0])) { t->df[r] = rui_button("Default", 0); g_signal_connect(t->df[r], "clicked", G_CALLBACK(kb_default), GINT_TO_POINTER(r)); p_put(t->df[r]); snprintf(n, sizeof n, "kbd%d", r); p_label_named(n, t->df[r]); } }
	p_custom(kt_lay, t, 0, 0);
	GtkWidget *rs = rui_button("Reset all shortcuts", 0); g_signal_connect(rs, "clicked", G_CALLBACK(kb_reset), NULL); p_put(rs); p_label_named("kbr", rs); p_custom(kbr_lay, rs, 0, 0);
	rui_jfree(st);
}
