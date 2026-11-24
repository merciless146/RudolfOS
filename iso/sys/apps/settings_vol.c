/*
 * rudolf-settings: Volume Controls (base_v24.html volPanelS, with the modules that add to it: osd31.js sysVolPanel, the system volume;
 * sndrs60.js, the nine more categories, their styles and "Use one style everywhere"; settings25.js, App volume, Output and the
 * microphones; mictest59.js, the microphone and its test; call59.js, Calls' echo cancelling). 1.6.3, docs/plan-1.6.3.md Part 3.
 * As the page: the desktop's sounds are a working copy (S.snd) until Apply (secFooter: Apply, Revert, Reset to defaults; SC.snd saved
 * as persistS saves it, bd95set:ACCT's "snd"); the system volume, the sound card, the microphone and the echo canceller go to rudolfd
 * at once (volume?set=|muted=, audio?card=, audio/mic?name=, sound/echo?on=); App volume is saved at once (mixSet: bd95mix:ACCT).
 * The ▶ test sounds, a style chosen and a volume let go play the sound as the page's sfx() would, with this page's volume and style:
 * the native shell plays it ("sfx K V P": the page's own tones and Rudolf styles, which rudolf-native already plays for the desktop's
 * sounds, rendered as the page renders them). The microphone's test listens through PulseAudio's simple API (the default microphone,
 * which choosing one makes it) and draws what it hears as mictest59.js's analyser does (512 samples, Blackman window, smoothing .6,
 * -100 to -30 dB; 24 bars from 80 Hz to 8 kHz; the level). GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
#pragma GCC diagnostic ignored "-Wunused-variable"
#include "settings-gen.h"
#pragma GCC diagnostic pop

/* ================================================================== PulseAudio's simple API (libpulse-simple, as rudolf-native opens it) */
typedef struct { int format; uint32_t rate; uint8_t channels; } PaSpec;   /* (pa_sample_spec: PA_SAMPLE_S16LE is 3) */
typedef void *(*PaNew)(const char *, const char *, int, const char *, const char *, const PaSpec *, const void *, const void *, int *);
static struct { void *lib; int tried; PaNew pnew; int (*wr)(void *, const void *, size_t, int *); int (*rd)(void *, void *, size_t, int *); int (*drain)(void *, int *); void (*fr)(void *); } PA;
static int pa_open(void)
{
	if (!PA.tried) { PA.tried = 1; PA.lib = dlopen("libpulse-simple.so.0", RTLD_NOW | RTLD_LOCAL);
		if (PA.lib) { PA.pnew = (PaNew)dlsym(PA.lib, "pa_simple_new"); *(void **)&PA.wr = dlsym(PA.lib, "pa_simple_write"); *(void **)&PA.rd = dlsym(PA.lib, "pa_simple_read");
			*(void **)&PA.drain = dlsym(PA.lib, "pa_simple_drain"); *(void **)&PA.fr = dlsym(PA.lib, "pa_simple_free"); }
		else fprintf(stderr, "rudolf-settings: sound: no libpulse-simple (%s)\n", dlerror()); }
	return PA.pnew && PA.wr && PA.rd && PA.drain && PA.fr;
}
typedef struct { short *b; size_t n; int rate; } Pcm;
static gpointer play_thread(gpointer d)
{
	Pcm *q = d; PaSpec ss = { 3, (uint32_t)q->rate, 1 }; int er = 0; void *p = PA.pnew(NULL, "Rudolf OS Settings", 1, NULL, "Settings", &ss, NULL, NULL, &er);
	if (p) { PA.wr(p, q->b, q->n * 2, &er); PA.drain(p, &er); PA.fr(p); } else fprintf(stderr, "rudolf-settings: sound: PulseAudio refused (%d)\n", er);
	g_free(q->b); g_free(q); return NULL;
}
void set_play_pcm(const short *pcm, size_t n, int rate)
{
	set_note("play %zu samples at %d", n, rate); if (getenv("RUDOLF_SET_NOSOUND") || !n || !pa_open()) return;
	Pcm *q = g_new0(Pcm, 1); q->b = g_memdup2(pcm, n * 2); q->n = n; q->rate = rate; g_thread_unref(g_thread_new("sound", play_thread, q));
}

/* ================================================================== the values: S.snd (the working copy) and SC.snd (the saved one) */
typedef struct { int on, vol, p; } Snd;
static Snd W_[NSNDCAT + 1], C_[NSNDCAT + 1];   /* ([0]: the master volume, "All desktop sounds") */
static int KEEP;   /* (the working copy kept across a rebuild of the page) */
static void snd_read(RuiJ *o, Snd *A, int fill_missing_from_work)
{
	RuiJ *m = rui_jprop(o, "master"), *v; A[0].on = (v = rui_jprop(m, "on")) && rui_jtype(v) == 1 ? (int)rui_jval(v) : 1; A[0].vol = (int)rui_jnum(m, "vol", 70); A[0].p = 0;
	for (int i = 0; i < NSNDCAT; i++) { RuiJ *e = rui_jprop(o, SNDCAT[i].k);
		if (!e && fill_missing_from_work) { A[i + 1] = W_[i + 1]; continue; }
		A[i + 1].on = (v = rui_jprop(e, "on")) && rui_jtype(v) == 1 ? (int)rui_jval(v) : 1; A[i + 1].vol = (int)rui_jnum(e, "vol", SNDCAT[i].vol);
		int p = (int)rui_jnum(e, "p", 0); A[i + 1].p = p >= 0 && p < SNDCAT[i].ns ? p : 0; }
}
/* (sndFill: the account's sounds with every category and the master volume filled in; the saved copy given the categories it lacks) */
static void load(void)
{
	RuiJ *st = set_stored(), *s = rui_jprop(st, "snd"); snd_read(s, W_, 0); snd_read(s, C_, 1); rui_jfree(st);
}
static int dirty(void) { return memcmp(W_, C_, sizeof W_) != 0; }
static char *snd_json(const Snd *A)
{
	GString *b = g_string_new(NULL); g_string_append_printf(b, "{\"master\":{\"on\":%s,\"vol\":%d}", A[0].on ? "true" : "false", A[0].vol);
	for (int i = 0; i < NSNDCAT; i++) g_string_append_printf(b, ",\"%s\":{\"on\":%s,\"vol\":%d,\"p\":%d}", SNDCAT[i].k, A[i + 1].on ? "true" : "false", A[i + 1].vol, A[i + 1].p);
	g_string_append_c(b, '}'); return g_string_free(b, FALSE);
}
static void commit(void) { memcpy(C_, W_, sizeof W_); char *j = snd_json(C_); const char *K[1] = { "snd" }, *V[1] = { j }; set_store(K, V, 1); set_note("persistS snd %s", j); g_free(j); }
/* the page's sfx(k, force) for this page's working copy: the native shell plays it */
static void sfx(int i, int force)
{
	Snd *m = &W_[0], *s = &W_[i + 1]; if (!force && (!m->on || !s->on)) return;
	double v = m->vol / 100.0 * s->vol / 100.0; if (v <= 0) return;
	char q[96]; snprintf(q, sizeof q, "sfx %s %.4f %d", SNDCAT[i].k, v, s->p); rui_shell(SW, q); set_note("%s", q);
}
static int cat_i(const char *k) { for (int i = 0; i < NSNDCAT; i++) if (!strcmp(SNDCAT[i].k, k)) return i; return -1; }

/* ================================================================== a row of .vr2: its label (a check box and its words, inline), a style,
   a slider, its value, ▶; the page's grid: minmax(120px,1.1fr) minmax(90px,1fr) minmax(80px,1.2fr) 40px auto, 8 px apart */
typedef struct { int i; int sys, bold; char *label; GtkWidget *area, *sel, *rng, *btn; double c[5], h, lh; int on, sens; char out[24]; } Vr;
static GPtrArray *ROWS;
static double vr_cols(Vr *r, double W)
{
	double aw = r->btn ? p_nat_w(r->btn) : 0, fr = (W - 40 - aw - 4 * 8) / 3.3;
	r->c[0] = 1.1 * fr; r->c[1] = fr; r->c[2] = 1.2 * fr; r->c[3] = 40; r->c[4] = aw; return fr;
}
/* the label's words after the check box: its first line from 20 px and a space (the box: 13 px, margins 3 3 3 4), wrapped in the column */
static PangoLayout *vr_lay(Vr *r)
{
	char *t = g_strdup_printf(" %s", r->label); PangoLayout *l = s_layout(t, 13, r->bold ? 700 : 400, r->c[0]); g_free(t);
	pango_layout_set_indent(l, (int)(20 * s_z() * PANGO_SCALE)); return l;
}
static double vr_label_h(Vr *r) { PangoLayout *l = vr_lay(r); int n = pango_layout_get_line_count(l); g_object_unref(l); return 19 + (n - 1) * s_lh(13, 400); }
static double vr_lay_fn(void *d, double x, double y, double W)
{
	Vr *r = d; vr_cols(r, W); r->lh = vr_label_h(r);
	double h = fmax(r->lh, fmax(r->sel ? 25 : 0, fmax(20, fmax(15, r->btn ? p_nat_h(r->btn) : 0)))); r->h = h;
	double x2 = x + r->c[0] + 8, x3 = x2 + r->c[1] + 8, x5 = x3 + r->c[2] + 8 + 40 + 8;
	p_place(r->area, x, y, W, h + (r->i < 0 && !r->sys ? 9 : 0));
	if (r->sel) p_place(r->sel, x2, y + (h - 25) / 2, r->c[1], 25);
	p_place(r->rng, x3 + 2, y + (h - 20) / 2 + 2, r->c[2], 16);
	if (r->btn) p_place(r->btn, x5, y + (h - p_nat_h(r->btn)) / 2, 0, 0);
	return h + (r->i < 0 && !r->sys ? 9 : 0);   /* (.vm: 8 px under it and its line) */
}
static gboolean vr_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Vr *r = d; double z = s_z(), ly = (r->h - r->lh) / 2;
	s_ckbox(cr, 4, ly + 3, r->on, r->sens);
	PangoLayout *l = vr_lay(r); double de, a = s_asc(13, 400, &de); PangoLayoutIter *it = pango_layout_get_iter(l); int n = 0;
	s_col(cr, "fg", 1);
	do { double top = n ? ly + 19 + (n - 1) * (a + de) : ly + 16 - a; PangoRectangle lg; pango_layout_iter_get_line_extents(it, NULL, &lg);
		cairo_move_to(cr, round(lg.x / (double)PANGO_SCALE), round((top + a) * z)); pango_cairo_show_layout_line(cr, pango_layout_iter_get_line_readonly(it)); n++; } while (pango_layout_iter_next_line(it));
	pango_layout_iter_free(it); g_object_unref(l);
	double x4 = r->c[0] + 8 + r->c[1] + 8 + r->c[2] + 8;
	s_text(cr, r->out, x4, (r->h - 15) / 2, 13, 400, 0, 0);
	if (r->i < 0 && !r->sys) { s_col(cr, "bd", 1); cairo_rectangle(cr, 0, round((r->h + 8) * z), gtk_widget_get_allocated_width(w), round(z)); cairo_fill(cr); }
	if (gtk_widget_has_visible_focus(w)) { cairo_save(cr); cairo_scale(cr, z, z); s_col(cr, "ac", 1); cairo_set_line_width(cr, 2); s_rrect(cr, 3, ly + 2, 15, 15, 3); cairo_stroke(cr); cairo_restore(cr); }
	return TRUE;
}
static void vr_free(gpointer p) { Vr *r = p; g_free(r->label); g_free(r); }
static void vr_toggle(Vr *r);
static gboolean vr_press(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	Vr *r = d; if (e->button != 1) return FALSE; double x = e->x / s_z(), y = e->y / s_z(); if (x > r->c[0] || y > r->h) return TRUE;   /* (the label only, as the page's <label>) */
	if (e->type == GDK_BUTTON_PRESS) { gtk_widget_grab_focus(w); return TRUE; }
	if (e->type == GDK_BUTTON_RELEASE && r->sens) vr_toggle(r); return TRUE;
}
static gboolean vr_key(GtkWidget *w, GdkEventKey *e, gpointer d) { (void)w; Vr *r = d; if (e->keyval == GDK_KEY_space && r->sens) { vr_toggle(r); return TRUE; } return FALSE; }
static Vr *vr_new(int i, int sys, const char *label, int bold)
{
	Vr *r = g_new0(Vr, 1); r->i = i; r->sys = sys; r->bold = bold; r->label = g_strdup(rui_tr(label)); r->sens = 1;
	r->area = gtk_drawing_area_new(); gtk_widget_set_can_focus(r->area, TRUE); gtk_widget_add_events(r->area, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK);
	g_object_set_data_full(G_OBJECT(r->area), "vr", r, vr_free); g_signal_connect(r->area, "draw", G_CALLBACK(vr_draw), r);
	g_signal_connect(r->area, "button-press-event", G_CALLBACK(vr_press), r); g_signal_connect(r->area, "button-release-event", G_CALLBACK(vr_press), r); g_signal_connect(r->area, "key-press-event", G_CALLBACK(vr_key), r);
	p_put(r->area); g_ptr_array_add(ROWS, r); return r;
}
static void vr_redraw(Vr *r) { gtk_widget_queue_draw(r->area); set_state_soon(); }

/* ================================================================== the system volume (osd31.js: rudolfd's volume, as the keys set it) */
static int SV = -1, SVM; static Vr *SYS; static guint SV_POLL;
static void sys_show(void)
{
	if (!SYS) return; Vr *r = SYS; r->on = SVM; if (SV >= 0 && !gtk_widget_has_focus(r->rng)) s_range_set(r->rng, SV);
	if (SVM) snprintf(r->out, sizeof r->out, "%s", rui_tr("Muted")); else snprintf(r->out, sizeof r->out, "%d%%", SV); vr_redraw(r);
}
static int sys_take(RuiJ *j) { if (!j || rui_jtype(rui_jprop(j, "volume")) != 2) return 0; SV = (int)rui_jnum(j, "volume", 0); SVM = rui_jbool(j, "muted"); return 1; }
static void sys_post(const char *q)
{
	char p[64], *out = NULL; snprintf(p, sizeof p, "/api/volume?%s", q); int c = rui_http("POST", p, "", 0, &out, NULL); set_note("POST %s %d", p + 5, c);
	if (c == 200 && out) { RuiJ *j = rui_jparse(out); sys_take(j); rui_jfree(j); } free(out); sys_show();
}
static void sys_rng(double v, void *d) { (void)d; SV = (int)v; SVM = 0; char q[24]; snprintf(q, sizeof q, "set=%d", (int)v); sys_post(q); }
static gboolean sys_poll(gpointer d) { (void)d; if (!SYS) { SV_POLL = 0; return G_SOURCE_REMOVE; } RuiJ *j = rui_get("/api/volume"); if (sys_take(j)) sys_show(); rui_jfree(j); return G_SOURCE_CONTINUE; }

/* ================================================================== the changes (the page's oninput/onchange on .vr2's controls) */
static void vr_toggle(Vr *r)
{
	r->on = !r->on; vr_redraw(r);
	if (r->sys) { char q[16]; snprintf(q, sizeof q, "muted=%d", r->on); sys_post(q); return; }
	Snd *s = &W_[r->i + 1]; s->on = r->on; gtk_widget_set_sensitive(r->rng, r->on); set_note("snd %s on %d", r->i < 0 ? "master" : SNDCAT[r->i].k, r->on); p_footer_update();
}
static void on_vol(double v, void *d) { Vr *r = d; W_[r->i + 1].vol = (int)v; snprintf(r->out, sizeof r->out, "%d%%", (int)v); vr_redraw(r); p_footer_update(); }
static void on_vol_change(double v, void *d) { (void)v; Vr *r = d; sfx(r->i < 0 ? cat_i("notify") : r->i, 0); }
static void on_style(const char *v, void *d) { Vr *r = d; W_[r->i + 1].p = atoi(v); set_note("snd %s p %s", SNDCAT[r->i].k, v); p_footer_update(); sfx(r->i, 1); }
static void on_test(GtkButton *b, gpointer d) { (void)b; Vr *r = d; Snd *s = &W_[r->i + 1]; sfx(r->i, !(s->on && W_[0].on)); }
/* "Use one style everywhere" (sndrs60.js): each category's nearest style, its select set; the choice put back to "Choose…" */
static GtkWidget *RSMSG, *RSSEL;
static void on_scheme(const char *v, void *d)
{
	(void)d; GtkWidget *sel = RSSEL; int si = -1; for (guint k = 0; k < G_N_ELEMENTS(SNDSCH); k++) if (!strcmp(SNDSCH[k].name, v)) si = (int)k; if (si < 0) return;
	int n = 0;
	for (int i = 0; i < NSNDCAT; i++) { const char *want = !strcmp(v, "Classic") ? NULL : *SNDSCH[si].map[i] ? SNDSCH[si].map[i] : v; int p = want ? -1 : 0;
		if (want) for (int j = 0; j < SNDCAT[i].ns; j++) if (!strcmp(SNDCAT[i].styles[j], want)) { p = j; break; }
		if (p < 0) continue; W_[i + 1].p = p; n++;
		for (guint k = 0; k < ROWS->len; k++) { Vr *r = g_ptr_array_index(ROWS, k); if (r->i == i && r->sel) { char pv[16]; snprintf(pv, sizeof pv, "%d", p); s_select_set(r->sel, pv); } } }
	char *t = g_strdup_printf("%s for %d of %d categories. Press Apply to keep it.", v, n, NSNDCAT); p_text_set(RSMSG, t); g_free(t);
	s_select_set(sel, ""); set_note("snd scheme %s %d", v, n); p_footer_update(); sfx(cat_i("startup"), 1);
}

/* ================================================================== .sp25: App volume, Output, the microphones (12 px .msg in --mut) */
typedef struct { char *t; GtkWidget *a; double mt, mb, h; } Msg12;
static double m12_lay(void *d, double x, double y, double w) { Msg12 *m = d; m->h = fmax(18, s_lines(m->t, 12, 400, w, NULL) * s_lh(12, 400)); p_place(m->a, x, y, w, m->h); return m->h; }
static gboolean m12_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)w; Msg12 *m = d; s_col(cr, "mut", 1); s_text(cr, m->t, 0, 0, 12, 400, gtk_widget_get_allocated_width(w) / s_z(), 0); return TRUE; }
static void m12_free(gpointer p) { Msg12 *m = p; g_free(m->t); g_free(m); }
static Msg12 *m12(const char *t, double mt, double mb)
{
	Msg12 *m = g_new0(Msg12, 1); m->t = g_strdup(rui_tr(t)); m->a = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(m->a), "m12", m, m12_free);
	g_signal_connect(m->a, "draw", G_CALLBACK(m12_draw), m); p_put(m->a); p_custom(m12_lay, m, mt, mb); return m;
}
static void m12_set(Msg12 *m, const char *t) { g_free(m->t); m->t = g_strdup(t); gtk_widget_queue_draw(m->a); p_relayout(); set_state_soon(); }
/* .acr: an app's name, its slider, its value (grid 1fr 110px 90px, 8 px apart, 4 px above and under, a line under) */
typedef struct { char *k, *name; GtkWidget *a, *rng; int v; } Acr;
static double acr_lay(void *d, double x, double y, double w) { Acr *r = d; p_place(r->a, x, y, w, 29); p_place(r->rng, x + w - 90 - 8 - 110 + 2, y + 4 + 2, 106, 16); return 29; }
static gboolean acr_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Acr *r = d; double z = s_z(), W = gtk_widget_get_allocated_width(w) / z; s_col(cr, "fg", 1); s_text(cr, r->name, 0, 4 + 2.5, 13, 400, 0, 0);
	char v[16]; snprintf(v, sizeof v, "%d%%", r->v); s_text(cr, v, W - 90, 4 + 2.5, 13, 400, 0, 0);
	s_col(cr, "acr", 1); cairo_rectangle(cr, 0, round(28 * z), W * z, round(z)); cairo_fill(cr); return TRUE;
}
static void acr_free(gpointer p) { Acr *r = p; g_free(r->k); g_free(r->name); g_free(r); }
static void acr_set(double v, void *d)
{
	Acr *r = d; r->v = (int)v; gtk_widget_queue_draw(r->a); char k[120]; snprintf(k, sizeof k, "bd95mix:%s", ACCT);
	RuiJ *m = set_ls_get(k), *o = rui_jprop(m, "x"); GString *b = g_string_new("{"); int n = 0;
	for (int i = 0; rui_jtype(o) == 5 && rui_jkey(o, i); i++) { const char *kk = rui_jkey(o, i); if (!strcmp(kk, r->k)) continue; RuiJ *x = rui_jprop(o, kk); if (rui_jtype(x) != 2) continue;
		char nb[40]; g_ascii_formatd(nb, sizeof nb, "%.10g", rui_jval(x)); char *e = set_jesc(kk); g_string_append_printf(b, "%s\"%s\":%s", n++ ? "," : "", e, nb); g_free(e); }
	char nb[40]; g_ascii_formatd(nb, sizeof nb, "%.10g", v / 100); char *e = set_jesc(r->k); g_string_append_printf(b, "%s\"%s\":%s}", n ? "," : "", e, nb); g_free(e); rui_jfree(m);
	set_ls_set(k, b->str); set_note("mix %s %s", k, b->str); g_string_free(b, TRUE);
}
static double mix_get(const char *app)
{
	char k[120]; snprintf(k, sizeof k, "bd95mix:%s", ACCT); RuiJ *m = set_ls_get(k), *x = rui_jprop(rui_jprop(m, "x"), app); double v = rui_jtype(x) == 2 ? rui_jval(x) : 1; rui_jfree(m); return v;
}
/* the sound cards and microphones (rudolfd's audio: /proc/asound/cards and pcm), as settings25.js reads them */
static GtkWidget *CARD; static Msg12 *MICS;
static int sof(const char *n) { return g_regex_match_simple("^sof[-_]", n, G_REGEX_CASELESS, 0); }
static void on_card(const char *v, void *d)
{
	(void)d; char p[64], er[200] = ""; snprintf(p, sizeof p, "/api/audio?card=%s", v); char *out = NULL; int c = rui_http("POST", p, "", 0, &out, NULL); set_note("POST %s %d", p + 5, c);
	RuiJ *j = out ? rui_jparse(out) : NULL; const char *m = rui_jstr(j, c == 200 ? "message" : "error"); snprintf(er, sizeof er, "%s", m ? m : c ? "" : "rudolfd did not answer");
	set_notify("Sound card", er, 0); rui_jfree(j); free(out);
}
static void audio_fill(GtkWidget **card_out, char **mics_out)
{
	RuiJ *r = rui_get("/api/audio"); const char *cards = rui_jstr(r, "cards"), *pcm = rui_jstr(r, "pcm");
	GPtrArray *num = g_ptr_array_new_with_free_func(g_free), *lab = g_ptr_array_new_with_free_func(g_free); GString *mics = g_string_new(NULL); int nm = 0;
	GHashTable *pl = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL), *cap = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	if (pcm) { char **L = g_strsplit(pcm, "\n", -1); for (char **l = L; *l; l++) { int a; if (sscanf(*l, "%d-", &a) != 1 || !strchr(*l, ':')) continue; char k[16]; snprintf(k, sizeof k, "%d", a);
			if (g_regex_match_simple("playback \\d", *l, 0, 0)) g_hash_table_add(pl, g_strdup(k)); if (g_regex_match_simple("capture \\d", *l, 0, 0)) g_hash_table_add(cap, g_strdup(k)); } g_strfreev(L); }
	if (cards) { GRegex *re = g_regex_new("^\\s*(\\d+)\\s+\\[([^\\]]+)\\]\\s*:\\s*(.*)$", 0, 0, NULL); char **L = g_strsplit(cards, "\n", -1);
		for (char **l = L; *l; l++) { GMatchInfo *mi; if (!g_regex_match(re, *l, 0, &mi)) { g_match_info_free(mi); continue; }
			char *n = g_match_info_fetch(mi, 1), *id = g_match_info_fetch(mi, 2), *ds = g_match_info_fetch(mi, 3); g_match_info_free(mi); char *idt = g_strstrip(g_strdup(id)), *dst = g_strstrip(g_strdup(ds));
			char kn[16]; snprintf(kn, sizeof kn, "%d", atoi(n)); int is_sof = sof(dst);
			if (!pcm || g_hash_table_contains(pl, kn)) { g_ptr_array_add(num, g_strdup(n)); g_ptr_array_add(lab, is_sof ? g_strdup_printf("Intel sound processor (SOF) — %s", ds) : g_strdup_printf("%s — %s", idt, ds)); }
			if (pcm && g_hash_table_contains(cap, kn)) { char *both = g_strdup_printf("%s %s", id, ds), *k = NULL;
				if (is_sof) k = g_strdup("Built-in microphones (Intel sound processor)");
				else if (g_regex_match_simple("acp6x|acp63|acp-pdm|acp_pdm|acppdm|rn-?pdm|renoir|acp_rpl|amd-?dmic|acp", both, G_REGEX_CASELESS, 0)) k = g_strdup("Built-in digital microphones (AMD audio processor)");
				else if (!g_regex_match_simple("HDMI|DisplayPort", ds, G_REGEX_CASELESS, 0)) k = g_strdup_printf("%s — %s", idt, ds);
				if (k) { g_string_append_printf(mics, "%s%s", nm++ ? " · " : "", k); g_free(k); } g_free(both); }
			g_free(n); g_free(id); g_free(ds); g_free(idt); g_free(dst); }
		g_strfreev(L); g_regex_unref(re); }
	if (!num->len) { g_ptr_array_add(num, g_strdup("")); g_ptr_array_add(lab, g_strdup("No sound card found")); }
	*card_out = s_select((const char *const *)num->pdata, (const char *const *)lab->pdata, (int)num->len, NULL, on_card, NULL);
	*mics_out = !r ? g_strdup("Looking…") : pcm ? g_strdup(nm ? mics->str : "No microphone found.") : g_strdup("—");
	g_ptr_array_unref(num); g_ptr_array_unref(lab); g_string_free(mics, TRUE); g_hash_table_unref(pl); g_hash_table_unref(cap); rui_jfree(r);
}

/* ================================================================== the microphone's test (mictest59.js) */
#define MT_N 512
typedef struct { GtkWidget *eq, *lvl, *go, *sel, *lab; Msg12 *st; float fr[MT_N / 2]; double sm[MT_N / 2]; unsigned char bins[MT_N / 2]; int level, peak, on; guint tick, timer; GThread *th; volatile int stop; int rate;
	GMutex mu; float ring[MT_N]; int fill; char label[200]; } MicT;
static MicT MT;
static gpointer mt_thread(gpointer d)
{
	(void)d; PaSpec ss = { 5, (uint32_t)MT.rate, 1 }; int er = 0;   /* (PA_SAMPLE_FLOAT32LE is 5) */
	void *p = PA.pnew(NULL, "Rudolf OS Settings", 2, NULL, "Microphone test", &ss, NULL, NULL, &er); if (!p) { MT.stop = 2; return NULL; }
	float buf[256];
	while (!MT.stop) { if (PA.rd(p, buf, sizeof buf, &er) < 0) { MT.stop = 2; break; }
		g_mutex_lock(&MT.mu); memmove(MT.ring, MT.ring + 256, (MT_N - 256) * sizeof(float)); memcpy(MT.ring + MT_N - 256, buf, sizeof buf); MT.fill = 1; g_mutex_unlock(&MT.mu); }
	PA.fr(p); return NULL;
}
/* (Web Audio's AnalyserNode: a Blackman window, the FFT's magnitudes over N, smoothed over time, in dB between -100 and -30, as bytes) */
static void mt_fft(const float *x)
{
	static double re[MT_N], im[MT_N]; int n = MT_N;
	for (int i = 0; i < n; i++) { double a = .16, w = (1 - a) / 2 - .5 * cos(2 * M_PI * i / n) + a / 2 * cos(4 * M_PI * i / n); re[i] = x[i] * w; im[i] = 0; }
	for (int i = 1, j = 0; i < n; i++) { int b = n >> 1; for (; j & b; b >>= 1) j ^= b; j ^= b; if (i < j) { double t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; } }
	for (int len = 2; len <= n; len <<= 1) { double ang = -2 * M_PI / len; for (int i = 0; i < n; i += len) for (int k = 0; k < len / 2; k++) {
		double wr = cos(ang * k), wi = sin(ang * k), ur = re[i + k], ui = im[i + k], vr = re[i + k + len / 2] * wr - im[i + k + len / 2] * wi, vi = re[i + k + len / 2] * wi + im[i + k + len / 2] * wr;
		re[i + k] = ur + vr; im[i + k] = ui + vi; re[i + k + len / 2] = ur - vr; im[i + k + len / 2] = ui - vi; } }
	for (int k = 0; k < n / 2; k++) { double mag = sqrt(re[k] * re[k] + im[k] * im[k]) / n; MT.sm[k] = .6 * MT.sm[k] + .4 * mag;
		double db = MT.sm[k] > 0 ? 20 * log10(MT.sm[k]) : -1000; double b = 255 * (db + 100) / 70; MT.bins[k] = (unsigned char)fmax(0, fmin(255, floor(b))); }
}
static void mt_status(const char *t) { m12_set(MT.st, t); set_note("mictest %s", t); }
static void mt_stop(int how);
static gboolean mt_tick(gpointer d)
{
	(void)d; if (MT.stop == 2) { MT.tick = 0; mt_stop(-1); mt_status("The microphone could not be opened: PulseAudio refused it"); return G_SOURCE_REMOVE; }
	float x[MT_N]; int ok; g_mutex_lock(&MT.mu); ok = MT.fill; memcpy(x, MT.ring, sizeof x); g_mutex_unlock(&MT.mu); if (!ok) return G_SOURCE_CONTINUE;
	mt_fft(x); double rms = 0; for (int i = 0; i < MT_N; i++) { double v = (round(fmax(0, fmin(255, 128 * (x[i] + 1)))) - 128) / 128; rms += v * v; } rms = sqrt(rms / MT_N);
	MT.level = (int)fmin(100, round(rms * 300)); MT.peak = MAX(MT.peak, MT.level); gtk_widget_queue_draw(MT.eq); gtk_widget_queue_draw(MT.lvl); set_state_soon(); return G_SOURCE_CONTINUE;
}
static gboolean mt_end(gpointer d) { (void)d; MT.timer = 0; mt_stop(-1); mt_status(MT.peak >= 3 ? "Test ended: the microphone was heard." : "Test ended: nothing was heard. Choose another microphone, or check that it is not muted."); return G_SOURCE_REMOVE; }
static void mt_stop(int how)
{
	MT.stop = MT.stop ? MT.stop : 1; if (MT.th) { g_thread_join(MT.th); MT.th = NULL; } if (MT.tick) { g_source_remove(MT.tick); MT.tick = 0; } if (MT.timer) { g_source_remove(MT.timer); MT.timer = 0; }
	MT.on = 0; if (MT.go) { gtk_button_set_label(GTK_BUTTON(MT.go), rui_tr("Test")); p_relayout(); }
	if (how == 1) mt_status(MT.peak >= 3 ? "Test stopped: the microphone was heard." : "Test stopped.");
}
static void mt_start(void)
{
	mt_stop(0); gtk_button_set_label(GTK_BUTTON(MT.go), rui_tr("Stop test")); p_relayout();
	if (getenv("RUDOLF_SET_NOSOUND") || !pa_open()) { mt_stop(0); mt_status("The microphone could not be opened: no sound server"); return; }
	memset(MT.sm, 0, sizeof MT.sm); memset(MT.bins, 0, sizeof MT.bins); MT.level = MT.peak = 0; MT.fill = 0; MT.stop = 0; MT.rate = 48000; MT.on = 1;
	MT.th = g_thread_new("mictest", mt_thread, NULL); MT.tick = g_timeout_add(33, mt_tick, NULL); MT.timer = g_timeout_add_seconds(120, mt_end, NULL);
	char *t = g_strdup_printf("Listening to %s. Speak or tap near it.", *MT.label ? MT.label : "the microphone"); mt_status(t); g_free(t);
}
static void mt_go(GtkButton *b, gpointer d) { (void)b; (void)d; if (MT.on) mt_stop(1); else mt_start(); }
static void mt_pick(const char *v, void *d)
{
	(void)d; int was = MT.on; mt_stop(0); char *e = g_uri_escape_string(v, "!'()*", FALSE), p[300], *out = NULL; snprintf(p, sizeof p, "/api/audio/mic?name=%s", e); g_free(e);
	int c = rui_http("POST", p, "", 0, &out, NULL); set_note("POST %s %d", p + 5, c);
	if (c != 200) { RuiJ *j = out ? rui_jparse(out) : NULL; const char *m = rui_jstr(j, "error"); mt_status(m && *m ? m : "That microphone could not be chosen."); rui_jfree(j); free(out); return; }
	free(out);
	const char *lab = g_object_get_data(G_OBJECT(MT.sel), v); snprintf(MT.label, sizeof MT.label, "%s", lab ? lab : v);
	char *t = g_strdup_printf("Chosen: %s (for every app).", MT.label); mt_status(t); g_free(t); if (was) mt_start();
}
static gboolean eq_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)d; double z = s_z(), W = gtk_widget_get_allocated_width(w) / z, H = 70; cairo_save(cr); cairo_scale(cr, z, z);
	cairo_set_source_rgb(cr, 0x0b / 255.0, 0x12 / 255.0, 0x20 / 255.0); s_rrect(cr, 0, 0, W, H, 4); cairo_fill(cr);
	if (MT.on || MT.peak) { double sx = W / 360; cairo_scale(cr, sx, 1); double ny = MT.rate / 2.0, bw = 360.0 / 24; int nb = MT_N / 2;
		for (int i = 0; i < 24; i++) { double f0 = 80 * pow(100, i / 24.0), f1 = 80 * pow(100, (i + 1) / 24.0); int b0 = (int)fmin(nb - 1, fmax(1, round(f0 / ny * nb))), b1 = (int)fmin(nb - 1, fmax(1, round(f1 / ny * nb))), m = 0;
			for (int k = b0; k <= b1; k++) m = MAX(m, MT.bins[k]); double h = fmax(2, m / 255.0 * (H - 4));
			if (m > 200) cairo_set_source_rgb(cr, 0xef / 255.0, 0x44 / 255.0, 0x44 / 255.0); else if (m > 140) cairo_set_source_rgb(cr, 0xf5 / 255.0, 0x9e / 255.0, 0x0b / 255.0); else cairo_set_source_rgb(cr, 0x22 / 255.0, 0xc5 / 255.0, 0x5e / 255.0);
			cairo_rectangle(cr, i * bw + 2, H - h, bw - 4, h); cairo_fill(cr); } }
	cairo_restore(cr); return TRUE;
}
static gboolean lvl_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)d; double z = s_z(), W = gtk_widget_get_allocated_width(w) / z; cairo_save(cr); cairo_scale(cr, z, z);
	cairo_set_source_rgb(cr, 0x1f / 255.0, 0x29 / 255.0, 0x37 / 255.0); s_rrect(cr, 0, 0, W, 6, 3); cairo_fill_preserve(cr); cairo_clip(cr);
	cairo_set_source_rgb(cr, 0x22 / 255.0, 0xc5 / 255.0, 0x5e / 255.0); cairo_rectangle(cr, 0, 0, W * (MT.on ? MT.level : 0) / 100.0, 6); cairo_fill(cr); cairo_restore(cr); return TRUE;
}
/* (.mtbox's .r2: "Microphone", then the select (flex: 1) and Test, 6 px apart) */
static gboolean mlab_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)d; s_col(cr, "fg", 1); s_text(cr, rui_tr("Microphone"), 0, (gtk_widget_get_allocated_height(w) / s_z() - s_lh(13, 400)) / 2, 13, 400, 130, 0); return TRUE; }
static double mrow_lay(void *d, double x, double y, double w)
{
	(void)d; double bw = p_nat_w(MT.go), bh = p_nat_h(MT.go), cx = x + 140, cw = w - 140; p_place(MT.lab, x, y, 130, 25); p_place(MT.sel, cx, y, cw - 6 - bw, 25); p_place(MT.go, cx + cw - bw, y + (25 - bh) / 2, 0, 0); return 25;
}
static double eq_lay(void *d, double x, double y, double w) { (void)d; double ww = fmin(360, w); p_place(MT.eq, x, y, ww, 70); return 70; }
static double lvl_lay(void *d, double x, double y, double w) { (void)d; p_place(MT.lvl, x, y, fmin(360, w), 6); return 6; }
static void mt_gone(GtkWidget *w, gpointer d) { (void)w; (void)d; mt_stop(0); MT.go = MT.eq = MT.lvl = MT.sel = NULL; }
static void mic_box(void)
{
	RuiJ *r = rui_get("/api/audio/mics"), *L = rui_jprop(r, "mics"); int n = rui_jtype(L) == 4 ? rui_jlen(L) : 0;
	const char **v = g_new0(const char *, n + 1), **l = g_new0(const char *, n + 1); const char *cur = NULL; MT.label[0] = 0;
	for (int i = 0; i < n; i++) { RuiJ *m = rui_jat(L, i); v[i] = set_jstr(m, "name", ""); const char *ds = rui_jstr(m, "desc"); l[i] = ds && *ds ? ds : v[i]; if (rui_jbool(m, "default")) { cur = v[i]; snprintf(MT.label, sizeof MT.label, "%s", l[i]); } }
	if (!cur && n) snprintf(MT.label, sizeof MT.label, "%s", l[0]);
	const char *nv[1] = { "" }, *nl[1] = { "No microphone found" };
	MT.sel = n ? s_select(v, l, n, cur, mt_pick, NULL) : s_select(nv, nl, 1, "", NULL, NULL);
	for (int i = 0; i < n; i++) g_object_set_data_full(G_OBJECT(MT.sel), v[i], g_strdup(l[i]), g_free);
	MT.go = rui_button("Test", 0); gtk_widget_set_tooltip_text(MT.go, rui_tr("See what the chosen microphone hears")); g_signal_connect(MT.go, "clicked", G_CALLBACK(mt_go), NULL);
	if (!n) { gtk_widget_set_sensitive(MT.sel, FALSE); gtk_widget_set_sensitive(MT.go, FALSE); }
	MT.lab = gtk_drawing_area_new(); g_signal_connect(MT.lab, "draw", G_CALLBACK(mlab_draw), NULL); p_put(MT.lab); p_put(MT.sel); p_put(MT.go);
	p_custom(mrow_lay, NULL, 6, 10); p_custom_find("Microphone", 0); p_label_named("mtsel", MT.sel); p_label_named("mtgo", MT.go);
	g_signal_connect(MT.go, "destroy", G_CALLBACK(mt_gone), NULL);
	MT.eq = gtk_drawing_area_new(); g_signal_connect(MT.eq, "draw", G_CALLBACK(eq_draw), NULL); p_put(MT.eq); p_custom(eq_lay, NULL, 6, 2); p_label_named("mteq", MT.eq);
	MT.lvl = gtk_drawing_area_new(); g_signal_connect(MT.lvl, "draw", G_CALLBACK(lvl_draw), NULL); p_put(MT.lvl); p_custom(lvl_lay, NULL, 0, 0);
	MT.st = m12("Press Test and speak: the bars move with what the microphone hears.", 4, 0); p_label_named("mtst", MT.st->a);
	g_free(v); g_free(l); g_object_set_data_full(G_OBJECT(MT.sel), "mics-json", r, (GDestroyNotify)rui_jfree);
}

/* ================================================================== Calls (call59.js): the echo canceller for Linux apps */
static GtkWidget *EC, *ECM; static const char ECB[] = "For calls in Linux apps that have no echo cancelling of their own, with the speakers on. It uses some processor time. Oracle's calls cancel echo themselves; a Bluetooth headset does too.";
static int EC_BUSY;
static void ec_show(RuiJ *s)
{
	int on = rui_jbool(s, "on"), act = rui_jbool(s, "active"); rui_check_set(EC, on);
	char *t = g_strdup_printf("%s%s", rui_tr(ECB), on && !act ? rui_tr(" (Not active now: there is no microphone or speaker it can use.)") : ""); p_text_set(ECM, t); g_free(t);
}
static void on_ec(int on, void *d)
{
	(void)d; if (EC_BUSY) return; EC_BUSY = 1; gtk_widget_set_sensitive(EC, FALSE); char p[48], *out = NULL; snprintf(p, sizeof p, "/api/sound/echo?on=%d", on);
	int c = rui_http("POST", p, "", 0, &out, NULL); set_note("POST %s %d", p + 5, c); RuiJ *j = out ? rui_jparse(out) : NULL;
	if (c == 200 && j) { ec_show(j); set_notify("Echo cancelling", rui_jbool(j, "on") ? "On" : "Off", 0); }
	else { rui_check_set(EC, !on); const char *m = rui_jstr(j, "error"); set_notify("Echo cancelling", m ? m : "rudolfd did not answer", 1); }
	rui_jfree(j); free(out); gtk_widget_set_sensitive(EC, TRUE); EC_BUSY = 0;
}

/* ================================================================== the footer: Apply, Revert, Reset to defaults (secCommit, secRevert, secReset 'vol') */
static void act(int what)
{
	if (what == 0) commit();
	else if (what == 1) { memcpy(W_, C_, sizeof W_); KEEP = 1; set_show("vol"); p_footer_note("Changes reverted."); set_note("revert snd"); }
	else { int mv = W_[0].vol; W_[0].on = 1; W_[0].vol = mv; for (int i = 0; i < NSNDCAT; i++) { W_[i + 1].on = 1; W_[i + 1].vol = SNDCAT[i].vol; W_[i + 1].p = 0; } commit(); KEEP = 1; set_show("vol"); }
}
static void page_gone(GtkWidget *w, gpointer d) { (void)w; (void)d; SYS = NULL; RSMSG = NULL; CARD = NULL; MICS = NULL; EC = ECM = NULL; if (ROWS) { g_ptr_array_unref(ROWS); ROWS = NULL; } }

void page_vol(void)
{
	if (!KEEP) load(); KEEP = 0; if (ROWS) g_ptr_array_unref(ROWS); ROWS = g_ptr_array_new();
	p_h3("Volume Controls");
	/* the system volume, when there is a sound server (osd31.js: shown once rudolfd answers), with "Use one style everywhere" in it */
	{ RuiJ *j = rui_get("/api/volume"); int ok = sys_take(j); rui_jfree(j);
		if (ok) { p_h4("System volume");
			Cell *c = p_r2("Use one style everywhere"); const char **sv = g_new0(const char *, G_N_ELEMENTS(SNDSCH) + 2); sv[0] = "";
			const char **sl = g_new0(const char *, G_N_ELEMENTS(SNDSCH) + 2); sl[0] = "Choose…"; for (guint k = 0; k < G_N_ELEMENTS(SNDSCH); k++) sv[k + 1] = sl[k + 1] = SNDSCH[k].name;
			GtkWidget *rs = RSSEL = s_select(sv, sl, (int)G_N_ELEMENTS(SNDSCH) + 1, "", on_scheme, NULL); g_free(sv); g_free(sl);
			double sp; s_lines("a a", 13, 400, 0, &sp); double a1; s_lines("aa", 13, 400, 0, &a1);
			c_add(c, rs, 0, 0, 0); p_label_named("rsall", rs); RSMSG = c_text(c, "", 400); c_gap(RSMSG, sp - a1); p_label_named("rsallm", RSMSG);
			SYS = vr_new(-1, 1, "Mute", 0); SYS->rng = s_range(0, 100, 1, SV, sys_rng, NULL); p_put(SYS->rng); p_label_named("sysmute", SYS->area); p_label_named("sysvol", SYS->rng);
			SYS->on = SVM; if (SVM) snprintf(SYS->out, sizeof SYS->out, "%s", rui_tr("Muted")); else snprintf(SYS->out, sizeof SYS->out, "%d%%", SV);
			p_custom(vr_lay_fn, SYS, 0, 6); p_custom_find("Mute", 0);
			p_msg_m("The computer's own volume, the same as the volume keys: every app, game and Linux app. The desktop's sounds below are mixed into it.", 1, 10, 0);
			p_h4("Desktop sounds");
			if (!SV_POLL) SV_POLL = g_timeout_add_seconds(5, sys_poll, NULL); } }
	/* the master volume and each category */
	{ Vr *m = vr_new(-1, 0, "All desktop sounds", 1); m->on = W_[0].on; m->rng = s_range(0, 100, 1, W_[0].vol, on_vol, m); s_range_on_change(m->rng, on_vol_change); gtk_widget_set_sensitive(m->rng, m->on);
		p_put(m->rng); snprintf(m->out, sizeof m->out, "%d%%", W_[0].vol); p_label_named("sndmaster", m->area); p_label_named("sndmastervol", m->rng);
		p_custom(vr_lay_fn, m, 0, 10); p_custom_find("All desktop sounds", 0); }
	for (int i = 0; i < NSNDCAT; i++) { Vr *r = vr_new(i, 0, SNDCAT[i].name, 0); r->on = W_[i + 1].on;
		char **v = g_new0(char *, SNDCAT[i].ns + 1); for (int k = 0; k < SNDCAT[i].ns; k++) v[k] = g_strdup_printf("%d", k); char cur[8]; snprintf(cur, sizeof cur, "%d", W_[i + 1].p);
		r->sel = s_select((const char *const *)v, SNDCAT[i].styles, SNDCAT[i].ns, cur, on_style, r); g_strfreev(v); gtk_widget_set_tooltip_text(r->sel, rui_tr("Sound"));
		r->rng = s_range(0, 100, 1, W_[i + 1].vol, on_vol, r); s_range_on_change(r->rng, on_vol_change); gtk_widget_set_sensitive(r->rng, r->on); snprintf(r->out, sizeof r->out, "%d%%", W_[i + 1].vol);
		r->btn = rui_button("▶", 0); gtk_widget_set_tooltip_text(r->btn, rui_tr("Play")); g_signal_connect(r->btn, "clicked", G_CALLBACK(on_test), r);
		p_put(r->sel); p_put(r->rng); p_put(r->btn);
		char n[40]; snprintf(n, sizeof n, "snd-%s", SNDCAT[i].k); p_label_named(n, r->area); snprintf(n, sizeof n, "sndp-%s", SNDCAT[i].k); p_label_named(n, r->sel);
		snprintf(n, sizeof n, "sndv-%s", SNDCAT[i].k); p_label_named(n, r->rng); snprintf(n, sizeof n, "sndt-%s", SNDCAT[i].k); p_label_named(n, r->btn);
		p_custom(vr_lay_fn, r, 0, 6); p_custom_find(SNDCAT[i].name, 0); }
	{ char *t = g_strdup_printf(rui_tr("Pick a sound style for each of the %d sound categories, turn it on or off, and set its volume from 0%% to 100%%. “All desktop sounds” scales all of them, inside the system volume. Changes are previewed immediately; press Apply to keep them."), NSNDCAT);
		p_msg(t, 1); g_free(t); }
	/* settings25.js: App volume, Output; the microphones */
	p_h4("App volume");
	{ Acr *a = g_new0(Acr, 1); a->k = g_strdup("system"); a->name = g_strdup("system"); a->v = (int)round(mix_get("system") * 100); a->a = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(a->a), "acr", a, acr_free);
		g_signal_connect(a->a, "draw", G_CALLBACK(acr_draw), a); a->rng = s_range(0, 100, 1, a->v, acr_set, a); p_put(a->a); p_put(a->rng); p_label_named("mix-system", a->rng); p_custom(acr_lay, a, 0, 0); }
	p_h4("Output");
	{ Cell *c = p_r2("Output device"); const char *v[1] = { "" }, *l[1] = { "System default" }; GtkWidget *s = s_select(v, l, 1, "", NULL, NULL); c_add(c, s, 0, 0, 1); p_label_named("sink", s); }
	{ char *mics = NULL; Cell *c = p_r2("Sound card"); audio_fill(&CARD, &mics); c_add(c, CARD, 0, 0, 1); p_label_named("card", CARD);
		p_h4("Microphones"); MICS = m12(mics, 10, 0); p_label_named("mics", MICS->a); g_free(mics); }
	mic_box();
	/* call59.js: Calls */
	p_h4("Calls");
	EC = p_check("Cancel echo for Linux apps", 0, on_ec, NULL); p_label_named("echo", EC);
	ECM = p_msg(ECB, 0);
	{ RuiJ *s = rui_get("/api/sound/echo"); if (s) ec_show(s); else gtk_widget_set_sensitive(EC, FALSE); rui_jfree(s); }
	p_footer(dirty, act);
	g_signal_connect(ROWS && ROWS->len ? ((Vr *)g_ptr_array_index(ROWS, 0))->area : EC, "destroy", G_CALLBACK(page_gone), NULL);
}
