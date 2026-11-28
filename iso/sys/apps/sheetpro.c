/* ===== 1.6.3 (sheetpro): SpreadsheetPro as a native program (/usr/bin/rudolf-sheetpro) -- docs/plan-1.6.3.md Part 1,
   step 1: the page's window, its toolbar, its eight menus, its formula bar and its grid, with values, formulas, undo and
   the page's keys. The page is desktop/spro45.js (the window and its commands), desktop/sprof45.js (the book: a sheet's
   cells, and what a typed text means) and desktop/spreng45.js (the engine: values, references, ranges, functions); its look
   is desktop/styles25.css's .sp-* and .wp-*, as the native WordPro's is the page's.
   Step 2 begins the files: a .RSHEET opens and saves exactly as sprof45.js writes it (the page's v2 JSON -- its cells,
   their formulas and their style -- so a file the page saved opens here and one saved here opens in the page and in
   Oracle), the window's title is the file's name with the page's * while there are unsaved changes, and the program takes
   a path on its command line, as Files launches it with one. CSV and .xlsx, printing and PDFs are the rest of step 2.
   What the steps still leave out is drawn as the page draws an item it cannot choose: CSV and .xlsx, charts,
   conditional formatting, data validation, filters, sorting, the number formats, the picture of a cell's style beyond
   bold, italic, underline, strikethrough, the alignment and the two colours, the assistant, and printing. Each is named in
   the check as something the page has and the native app does not yet.
   GPL-2.0-or-later. */
#define _GNU_SOURCE
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "rudolf-ui.h"
#include "sheetpro-gen.h"

#define MAXC 702        /* (SPE.MAXC: A to ZZ, the page's own last column) */
#define MAXR 100000     /* (SPE.MAXR) */
#define HW 46.0         /* (the head's width: the row numbers' gutter) */
#define HH 22.0         /* (the head's height: the column letters' strip) */
#define DW 80.0         /* (a column's width, and a row's height, at 100 %) */
#define DH 20.0
#define CELLPT 11.0     /* (the page's cell font: 11 pt, drawn at 11*4/3 px) */

typedef struct { char *t; unsigned b, i, u, s; int ha; char *fc, *bg, *font; int sz; } Cell;   /* (a cell: what was typed, and its style) */

static RuiWin *W;
static GtkWidget *TB, *MBAR, *MBTNS[8], *AREA, *ENTRY, *NB, *FXE, *STATUS, *AGG, *ZTXT, *ZOOMSL, *AREAOV;
static GtkAdjustment *ADJV, *ADJH;
static GtkWidget *BTN_B, *BTN_I, *BTN_U, *BTN_S, *ALN[3], *FONTW, *SIZEW;
static char SAY[200] = "Ready";
static char AGGS[160];   /* (what the choice adds up to, as numbers the tests read: sum|average|min|max|count) */
static int CUR_R, CUR_C, SEL_R0, SEL_C0, SEL_R1, SEL_C1, SEL_AR, SEL_AC;
static int ZOOM = 100, SCX, SCY, SHOWF, GRIDON = 1, FBAR = 1, TABFROM = -1;
static int EDITING, UPDATING, ALLOCATED, READY;   /* (READY: the window is made, so the toolbar's own changes are not edits) */
static GHashTable *CELLS, *VISIT;
static const char *MENU_OPEN; static int MENU_OPEN_N;
static void state_soon(void); static void status_update(void); static void redraw(void); static void layout_ranges(void);
static void upd_tb(void); static void hist_push(void); static void hist_undo(void); static void hist_redo(void);
static void place_entry(void); static void on_style_btn(GtkWidget *b, gpointer d); static void fx_update(void);
static void zoom_to(int z); static void new_book(void); static void show_keys(void); static void show_stats(void);
static gint cmp_key(gconstpointer a, gconstpointer b);
static int a1_parse(const char *t, int *r0, int *c0, int *r1, int *c1);
static void nb_update(void); static void dlg_close(int i, void *d);
static void title_update(void); static void mark_dirty(void); static void do_open(const char *path);
static void do_save(int as); static void file_open_dlg(void);
static void check_errors(void); static int menus_n(void);
static void on_clear_btn(GtkWidget *b, gpointer d);
static void redraw(void) { if (AREA) gtk_widget_queue_draw(AREA); }

/* ------------------------------------------------------------------ the model: a sheet's cells (sprof45.js's map)
   A key is the page's spK(r,c) = r*702+c, held as the pointer's value (as g_direct_hash's key), and a cell is what was
   typed -- its text, never a parsed value -- with its style beside it, so what the page keeps of a cell is kept here. */
static guint ckey(int r, int c) { return (guint)((guint64)r * MAXC + (guint)c + 1u); }
static Cell *cell_get(int r, int c) { return g_hash_table_lookup(CELLS, GUINT_TO_POINTER(ckey(r, c))); }
static void cell_free(gpointer p)
{
	Cell *c = p; if (!c) return;
	g_free(c->t); g_free(c->fc); g_free(c->bg); g_free(c->font); g_free(c);
}
static Cell *cell_copy(const Cell *o)
{
	Cell *c = g_new0(Cell, 1);
	c->t = g_strdup(o->t); c->b = o->b; c->i = o->i; c->u = o->u; c->s = o->s; c->ha = o->ha;
	c->fc = g_strdup(o->fc); c->bg = g_strdup(o->bg); c->font = g_strdup(o->font); c->sz = o->sz;
	return c;
}
/* (a cell is set by its text: NULL or nothing in it takes the cell away, as the page's setCell does -- the style goes
   with it, so a cleared cell is empty again) */
static void cell_set(int r, int c, const char *text)
{
	gpointer k = GUINT_TO_POINTER(ckey(r, c));
	if (!text || !*text) { g_hash_table_remove(CELLS, k); return; }
	Cell *cl = g_hash_table_lookup(CELLS, k);
	if (!cl) { cl = g_new0(Cell, 1); g_hash_table_insert(CELLS, k, cl); }
	g_free(cl->t); cl->t = g_strdup(text);
}
static const char *cell_raw(int r, int c) { Cell *cl = cell_get(r, c); return cl && cl->t ? cl->t : ""; }
/* (the page's styleSel: what the toolbar changes of the cells of a choice) */
typedef void (*StyleFn)(Cell *cl, void *d);
static void style_sel(StyleFn fn, void *d)
{
	for (int r = SEL_R0; r <= SEL_R1; r++) for (int c = SEL_C0; c <= SEL_C1; c++) {
		Cell *cl = cell_get(r, c);
		if (!cl) { if (!fn) continue; cl = g_new0(Cell, 1); g_hash_table_insert(CELLS, GUINT_TO_POINTER(ckey(r, c)), cl); }
		if (fn) fn(cl, d);
	}
	hist_push(); upd_tb(); layout_ranges(); status_update(); fx_update(); redraw();
}
static int used_r(void) { int m = -1; GHashTableIter it; gpointer k, v; g_hash_table_iter_init(&it, CELLS);
	while (g_hash_table_iter_next(&it, &k, &v)) { int r = ((guint)GPOINTER_TO_UINT(k) - 1u) / MAXC; if (r > m) m = r; } return m; }
static int used_c(void) { int m = -1; GHashTableIter it; gpointer k, v; g_hash_table_iter_init(&it, CELLS);
	while (g_hash_table_iter_next(&it, &k, &v)) { int c = ((guint)GPOINTER_TO_UINT(k) - 1u) % MAXC; if (c > m) m = c; } return m; }
static int cell_n(void) { return (int)g_hash_table_size(CELLS); }

/* ------------------------------------------------------------------ values and formulas (the page's engine, the part
   step 1 needs: numbers, text, TRUE and FALSE, references, ranges, the arithmetic and comparison operators, and the five
   functions every sheet starts with -- SUM, AVERAGE, MIN, MAX and COUNT) */
enum { V_EMPTY = 0, V_NUM, V_TEXT, V_BOOL, V_ERR, V_RANGE };
typedef struct { int k; double n; const char *s; int b; const char *err; int r0, c0, r1, c1; } Val;
static Val vnum(double n) { Val v = { V_NUM, n, NULL, 0, NULL, 0, 0, 0, 0 }; return v; }
static Val vtext(const char *s) { Val v = { V_TEXT, 0, s, 0, NULL, 0, 0, 0, 0 }; return v; }
static Val vbool(int b) { Val v = { V_BOOL, 0, NULL, b, NULL, 0, 0, 0, 0 }; return v; }
static Val verr(const char *e) { Val v = { V_ERR, 0, NULL, 0, e, 0, 0, 0, 0 }; return v; }
static Val vrange(int r0, int c0, int r1, int c1) { Val v = { V_RANGE, 0, NULL, 0, NULL, r0, c0, r1, c1 }; return v; }
static const char *E_DIV = "#DIV/0!", *E_VAL = "#VALUE!", *E_NAME = "#NAME?", *E_CIRC = "#CIRCULAR!";
/* (a typed text as a number, by the page's spParse: $ and commas allowed, a trailing % a hundredth of it) */
static int as_num(const char *t, double *out)
{
	if (!t || !*t) return 0;
	char *s = g_strdup(t); g_strstrip(s);
	char *p = s; int neg = 0, pct = 0, ok = 0;
	if (*p == '+' || *p == '-') { neg = *p == '-'; p++; }
	size_t n = strlen(p); if (n && p[n - 1] == '%') { pct = 1; p[n - 1] = 0; }
	if (*p == '$') p++;
	GString *g = g_string_new(NULL);
	for (char *x = p; *x; x++) if (*x != ',' && *x != ' ') g_string_append_c(g, *x);
	if (g->len) { char *e = NULL; double v = g_ascii_strtod(g->str, &e); if (e && !*e) { ok = 1; if (pct) v /= 100; if (neg) v = -v; *out = v; } }
	g_string_free(g, TRUE); g_free(s);
	return ok;
}
static Val eval_cell(int r, int c);
typedef struct { const char *s; int i, bad; } P;
static void skip(P *p) { while (p->s[p->i] == ' ' || p->s[p->i] == '\t' || p->s[p->i] == '\n') p->i++; }
static Val p_cmp(P *p);
static int peek(P *p) { skip(p); return (unsigned char)p->s[p->i]; }
static Val p_atom(P *p)
{
	int ch = peek(p);
	if (!ch) { p->bad = 1; return verr(E_NAME); }
	if (ch == '(') { p->i++; Val v = p_cmp(p); skip(p); if (p->s[p->i] == ')') p->i++; else p->bad = 1; return v; }
	if (ch == '"') {   /* (a string: its quotes doubled, as the page's tokenizer has them) */
		p->i++; GString *g = g_string_new(NULL);
		while (p->s[p->i]) { if (p->s[p->i] == '"') { if (p->s[p->i + 1] == '"') { g_string_append_c(g, '"'); p->i += 2; continue; } p->i++; break; } g_string_append_c(g, p->s[p->i++]); }
		Val v = vtext(g_string_free(g, FALSE)); return v;
	}
	if (ch == '#' || (ch >= '0' && ch <= '9') || ch == '.') {
		if (ch == '#') {   /* (an error written out, as the page's tokenizer reads one) */
			static const char *E[] = { "#DIV/0!", "#VALUE!", "#REF!", "#NAME?", "#N/A", "#NUM!", "#NULL!", "#CIRCULAR!" };
			for (unsigned k = 0; k < G_N_ELEMENTS(E); k++) { size_t l = strlen(E[k]);
				if (!g_ascii_strncasecmp(p->s + p->i, E[k], l)) { p->i += (int)l; return verr(E[k]); } }
			p->bad = 1; return verr(E_NAME);
		}
		char *e = NULL; double v = g_ascii_strtod(p->s + p->i, &e);
		if (!e || e == p->s + p->i) { p->bad = 1; return verr(E_VAL); }
		p->i = (int)(e - p->s); return vnum(v);
	}
	if (g_ascii_isalpha(ch) || ch == '$' || ch == '_') {
		int st = p->i;
		while (p->s[p->i] == '$') p->i++;
		while (g_ascii_isalnum(p->s[p->i]) || p->s[p->i] == '.' || p->s[p->i] == '_') p->i++;
		int en = p->i;
		if (p->s[p->i] == '(') {   /* (a function: its arguments, each an expression or a range) */
			char *name = g_strndup(p->s + st, (size_t)(en - st)); char up[32] = "";
			for (int k = 0; name[k] && k < 31; k++) up[k] = (char)g_ascii_toupper(name[k]);
			p->i++;
			GPtrArray *args = g_ptr_array_new();
			if (peek(p) != ')') for (;;) {
				Val a = p_cmp(p); Val *z = g_new(Val, 1); *z = a; g_ptr_array_add(args, z);
				skip(p); if (p->s[p->i] == ',') { p->i++; continue; } break;
			}
			skip(p); if (p->s[p->i] == ')') p->i++; else p->bad = 1;
			Val out = verr(E_VAL); int known = 1;
			double sum = 0, mn = 0, mx = 0; int cnt = 0, nonEmpty = 0;
			for (guint k = 0; k < args->len; k++) {
				Val *a = g_ptr_array_index(args, k);
				if (a->k == V_RANGE) {
					int last = a->r0 + 20000; if (last > a->r1) last = a->r1;
					for (int r = a->r0; r <= last; r++) for (int c = a->c0; c <= a->c1; c++) {
						Val v = eval_cell(r, c);
						if (v.k == V_NUM) { sum += v.n; if (!cnt || v.n < mn) mn = v.n; if (!cnt || v.n > mx) mx = v.n; cnt++; nonEmpty++; }
						else if (v.k == V_TEXT || v.k == V_BOOL) nonEmpty++;
						else if (v.k == V_ERR) { out = v; goto done; }
					}
				} else if (a->k == V_NUM) { sum += a->n; if (!cnt || a->n < mn) mn = a->n; if (!cnt || a->n > mx) mx = a->n; cnt++; nonEmpty++; }
				else if (a->k == V_TEXT) nonEmpty++;
				else if (a->k == V_BOOL) nonEmpty++;
				else if (a->k == V_ERR) { out = *a; goto done; }
			}
			if (!strcmp(up, "SUM")) out = vnum(sum);
			else if (!strcmp(up, "AVERAGE")) out = cnt ? vnum(sum / cnt) : verr(E_DIV);
			else if (!strcmp(up, "MIN")) out = cnt ? vnum(mn) : vnum(0);
			else if (!strcmp(up, "MAX")) out = cnt ? vnum(mx) : vnum(0);
			else if (!strcmp(up, "COUNT")) out = vnum(cnt);
			else known = 0;
			done:
			for (guint k = 0; k < args->len; k++) g_free(g_ptr_array_index(args, k));
			g_ptr_array_free(args, TRUE); g_free(name);
			if (!known) return verr(E_NAME);
			return out;
		}
		{   /* (a reference or a range: A1, $A$1, A1:B9 -- anything else is a name this step does not know) */
			char *name = g_strndup(p->s + st, (size_t)(en - st)); char *q = name; char *w = name;
			for (; *q; q++) if (*q != '$') *w++ = *q; *w = 0;
			if (p->s[p->i] == ':') {
				p->i++; int st2 = p->i;
				if (p->s[p->i] == '$') p->i++;
				while (g_ascii_isalnum(p->s[p->i])) p->i++;
				char *n2 = g_strndup(p->s + st2, (size_t)(p->i - st2)); char *q2 = n2, *w2 = n2;
				for (; *q2; q2++) if (*q2 != '$') *w2++ = *q2; *w2 = 0;
				GRegex *re = g_regex_new("^([A-Za-z]{1,3})(\\d{1,7})$", 0, 0, NULL);
				GMatchInfo *m1 = NULL, *m2 = NULL; Val out; int ok = 0;
				if (g_regex_match(re, name, 0, &m1) && g_regex_match(re, n2, 0, &m2)) {
					char *l1 = g_match_info_fetch(m1, 1), *l2 = g_match_info_fetch(m2, 1);
					int r1 = atoi(g_match_info_fetch(m1, 2)) - 1, r2 = atoi(g_match_info_fetch(m2, 2)) - 1;
					int c1 = 0, c2 = 0;
					for (char *x = l1; *x; x++) c1 = c1 * 26 + (g_ascii_toupper(*x) - 'A' + 1);
					for (char *x = l2; *x; x++) c2 = c2 * 26 + (g_ascii_toupper(*x) - 'A' + 1);
					c1--; c2--;
					ok = c1 >= 0 && c1 < MAXC && c2 >= 0 && c2 < MAXC && r1 >= 0 && r1 < MAXR && r2 >= 0 && r2 < MAXR;
					if (ok) out = vrange(MIN(r1, r2), MIN(c1, c2), MAX(r1, r2), MAX(c1, c2));
					g_free(l1); g_free(l2);
				}
				g_match_info_free(m1); g_match_info_free(m2); g_regex_unref(re);
				g_free(name); g_free(n2);
				if (!ok) { p->bad = 1; return verr(E_NAME); }
				return out;
			}
			GRegex *re = g_regex_new("^([A-Za-z]{1,3})(\\d{1,7})$", 0, 0, NULL);
			GMatchInfo *mi = NULL; int ok = g_regex_match(re, name, 0, &mi);
			Val out = verr(E_NAME);
			if (ok) {
				char *l = g_match_info_fetch(mi, 1); int r = atoi(g_match_info_fetch(mi, 2)) - 1, c = 0;
				for (char *x = l; *x; x++) c = c * 26 + (g_ascii_toupper(*x) - 'A' + 1);
				c--;
				if (c >= 0 && c < MAXC && r >= 0 && r < MAXR) { Val v = eval_cell(r, c); out = v.k == V_RANGE ? verr(E_VAL) : v; }
				g_free(l);
			} else if (!g_ascii_strcasecmp(name, "TRUE")) out = vbool(1);
			else if (!g_ascii_strcasecmp(name, "FALSE")) out = vbool(0);
			g_match_info_free(mi); g_regex_unref(re); g_free(name);
			return out;
		}
	}
	p->bad = 1; return verr(E_NAME);
}
static Val p_post(P *p)   /* (a trailing % is a hundredth of the number, as the page's) */
{
	Val v = p_atom(p);
	while (peek(p) == '%') { p->i++; if (v.k == V_NUM) v.n /= 100; else if (v.k != V_ERR) return verr(E_VAL); }
	return v;
}
static Val p_pow(P *p)
{
	Val a = p_post(p);
	while (peek(p) == '^') { p->i++; Val b = p_post(p); if (a.k != V_NUM || b.k != V_NUM) return verr(E_VAL); a = vnum(pow(a.n, b.n)); }
	return a;
}
static int as_dbl(Val v, double *out)
{
	if (v.k == V_NUM) { *out = v.n; return 1; }
	if (v.k == V_BOOL) { *out = v.b ? 1 : 0; return 1; }
	if (v.k == V_TEXT && as_num(v.s, out)) return 1;
	if (v.k == V_EMPTY) { *out = 0; return 1; }
	return 0;
}
static Val p_mul(P *p)
{
	Val a = p_pow(p);
	for (;;) {
		int ch = peek(p); if (ch != '*' && ch != '/') break; p->i++;
		Val b = p_pow(p); double x, y;
		if (a.k == V_ERR) continue; if (b.k == V_ERR) { a = b; continue; }
		if (!as_dbl(a, &x) || !as_dbl(b, &y)) { a = verr(E_VAL); continue; }
		if (ch == '/') { if (y == 0) a = verr(E_DIV); else a = vnum(x / y); } else a = vnum(x * y);
	}
	return a;
}
static Val p_add(P *p)
{
	Val a = p_mul(p);
	for (;;) {
		int ch = peek(p); if (ch != '+' && ch != '-') break; p->i++;
		Val b = p_mul(p); double x, y;
		if (a.k == V_ERR) continue; if (b.k == V_ERR) { a = b; continue; }
		if (!as_dbl(a, &x) || !as_dbl(b, &y)) { a = verr(E_VAL); continue; }
		a = vnum(ch == '+' ? x + y : x - y);
	}
	return a;
}
static int cmp_val(Val *a, Val *b)   /* (1 when a is the greater, 0 when they are the same, -1 below) */
{
	double x, y;
	if (as_dbl(*a, &x) && as_dbl(*b, &y)) return x > y ? 1 : x < y ? -1 : 0;
	const char *s = a->k == V_TEXT ? a->s : a->k == V_BOOL ? (a->b ? "TRUE" : "FALSE") : "";
	const char *t = b->k == V_TEXT ? b->s : b->k == V_BOOL ? (b->b ? "TRUE" : "FALSE") : "";
	int c = g_ascii_strcasecmp(s, t); return c > 0 ? 1 : c < 0 ? -1 : 0;
}
static Val p_cmp(P *p)
{
	Val a = p_add(p);
	for (;;) {
		int ch = peek(p), op = 0;
		if (ch == '=') op = 1; else if (ch == '<' && p->s[p->i + 1] == '>') op = 2;
		else if (ch == '<' && p->s[p->i + 1] == '=') op = 3; else if (ch == '>' && p->s[p->i + 1] == '=') op = 4;
		else if (ch == '<') op = 5; else if (ch == '>') op = 6;
		if (!op) break;
		p->i += (op == 2 || op == 3 || op == 4) ? 2 : 1;
		Val b = p_add(p); if (a.k == V_ERR) continue; if (b.k == V_ERR) { a = b; continue; }
		int c = cmp_val(&a, &b);
		switch (op) {
		case 1: a = vbool(c == 0); break; case 2: a = vbool(c != 0); break; case 3: a = vbool(c <= 0); break;
		case 4: a = vbool(c >= 0); break; case 5: a = vbool(c < 0); break; default: a = vbool(c > 0); break;
		}
	}
	return a;
}
static Val eval_formula(const char *src, int r, int c)
{
	gpointer k = GUINT_TO_POINTER(ckey(r, c));
	if (g_hash_table_contains(VISIT, k)) return verr(E_CIRC);
	g_hash_table_add(VISIT, k);
	P p; p.s = src; p.i = 0; p.bad = 0;
	Val v = p_cmp(&p);
	skip(&p);
	if (!p.bad && p.s[p.i]) v = verr(E_NAME);
	/* (a range on its own is not a value: only a function reads one) */
	if (!p.bad && v.k == V_RANGE) v = verr(E_VAL);
	g_hash_table_remove(VISIT, k);
	return v;
}
static Val eval_cell(int r, int c)
{
	Cell *cl = cell_get(r, c);
	if (!cl || !cl->t || !*cl->t) { Val v = { V_EMPTY, 0, NULL, 0, NULL, 0, 0, 0, 0 }; return v; }
	if (cl->t[0] == '=') return eval_formula(cl->t + 1, r, c);
	if (cl->t[0] == '\'') return vtext(cl->t + 1);
	double n;
	if (as_num(cl->t, &n)) return vnum(n);
	if (!g_ascii_strcasecmp(cl->t, "TRUE")) return vbool(1);
	if (!g_ascii_strcasecmp(cl->t, "FALSE")) return vbool(0);
	return vtext(cl->t);
}
/* (a number as the page's General format shows it: a whole number as it is, anything else to ten figures) */
static void num_text(char *b, size_t n, double v)
{
	if (v == 0) { snprintf(b, n, "0"); return; }
	if (fabs(v) < 1e15 && v == floor(v)) { snprintf(b, n, "%.0f", v); return; }
	snprintf(b, n, "%.10g", v);
}
/* (a cell as the grid shows it: what was typed, or a formula's value, as the page's spText has it) */
static char *cell_show(int r, int c)
{
	Cell *cl = cell_get(r, c);
	if (!cl || !cl->t || !*cl->t) return g_strdup("");
	if (SHOWF) return g_strdup(cl->t);
	Val v = eval_cell(r, c);
	switch (v.k) {
	case V_NUM: case V_EMPTY: { char b[40]; num_text(b, sizeof b, v.n); return g_strdup(b); }
	case V_TEXT: return g_strdup(v.s ? v.s : "");
	case V_BOOL: return g_strdup(v.b ? "TRUE" : "FALSE");
	case V_ERR: return g_strdup(v.err ? v.err : E_VAL);
	default: return g_strdup("");
	}
}

/* ------------------------------------------------------------------ undo and redo: the page's own history of changes
   An entry is every cell as it stood after a change (a book of a few thousand cells costs little, and a step of the
   history is exactly the page's "the cells as they were and as they are"). */
#define HISTP 100
typedef struct { GHashTable *h; } Step;
static Step *UNDO[HISTP]; static int UNDO_N, UNDO_I;
static void step_free(gpointer p) { Step *s = p; if (!s) return; g_hash_table_destroy(s->h); g_free(s); }
static GHashTable *cells_copy_of(GHashTable *src)
{
	GHashTable *h = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, cell_free);
	GHashTableIter it; gpointer k, v; g_hash_table_iter_init(&it, src);
	while (g_hash_table_iter_next(&it, &k, &v)) g_hash_table_insert(h, k, cell_copy(v));
	return h;
}
static GHashTable *cells_copy(void) { return cells_copy_of(CELLS); }
static void hist_push(void)
{
	if (UPDATING) return;
	if (UNDO_I < UNDO_N) { for (int i = UNDO_I; i < UNDO_N; i++) { step_free(UNDO[i]); UNDO[i] = NULL; } UNDO_N = UNDO_I; }
	if (UNDO_N == HISTP) {
		step_free(UNDO[0]); memmove(UNDO, UNDO + 1, sizeof(UNDO[0]) * (HISTP - 1)); UNDO_N--; UNDO_I--;
	}
	Step *s = g_new0(Step, 1); s->h = cells_copy();
	UNDO[UNDO_N++] = s; UNDO_I = UNDO_N;
	mark_dirty();
	state_soon();
}
static void hist_apply(void)
{
	GHashTable *old = CELLS; CELLS = cells_copy_of(UNDO[UNDO_I - 1]->h); g_hash_table_destroy(old);
	layout_ranges(); status_update(); upd_tb(); fx_update(); redraw(); state_soon();
}
static void hist_undo(void)
{
	if (EDITING) return;
	if (UNDO_I <= 1) { snprintf(SAY, sizeof SAY, "%s", rui_tr("Nothing to undo")); status_update(); return; }
	UNDO_I--; hist_apply();
}
static void hist_redo(void)
{
	if (EDITING) return;
	if (UNDO_I >= UNDO_N) { snprintf(SAY, sizeof SAY, "%s", rui_tr("Nothing to redo")); status_update(); return; }
	UNDO_I++; hist_apply();
}

/* ------------------------------------------------------------------ the file: a .RSHEET, as the page writes it
   sprof45.js's own v2 JSON (its spToJSON): {v:2, app, active, names, styles:[a style], sheets:[{name, color, cells:
   [[spK(r,c), value, formula, style]], ...}]}, each different style once, a cell's value what the page parses (its
   spParse: a number, TRUE or FALSE, or text) and the style index into the book's list. A file saved here opens in the
   page and in Oracle, and one they saved opens here.
   What the app does not model yet is not carried through a save -- the number formats, the sizes and hiding of rows and
   columns, the merges, the frozen panes, the charts, the other sheets -- and the step that models each takes it on. */
static char *FILE_PATH;   /* the file the book came from or went to; NULL for a book that has none */
static int DIRTY, SUSPEND;
static const char *path_base(const char *p)
{
	if (!p) return NULL;
	const char *s = strrchr(p, '/');
	return s ? s + 1 : p;
}
static void title_update(void)
{
	if (!W || !W->win) return;
	const char *b = path_base(FILE_PATH);
	char t[600];
	snprintf(t, sizeof t, "%s%s - SpreadsheetPro", b && *b ? b : "Book1", DIRTY ? " *" : "");
	gtk_window_set_title(GTK_WINDOW(W->win), t);
}
/* (a change the person made: the page's dirty flag, and its * in the title) */
static void mark_dirty(void)
{
	if (!READY || SUSPEND || DIRTY) return;
	DIRTY = 1; title_update();
}
/* ---- writing the page's JSON ---- */
static void j_str(GString *o, const char *s)
{
	g_string_append_c(o, '"');
	for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
		switch (*p) {
		case '"': g_string_append(o, "\\\""); break;
		case '\\': g_string_append(o, "\\\\"); break;
		case '\n': g_string_append(o, "\\n"); break;
		case '\r': g_string_append(o, "\\r"); break;
		case '\t': g_string_append(o, "\\t"); break;
		default:
			if (*p < 0x20) g_string_append_printf(o, "\\u%04x", (unsigned)*p);
			else g_string_append_c(o, (char)*p);
		}
	}
	g_string_append_c(o, '"');
}
static void j_num(GString *o, double v)
{
	if (v == (double)(long long)v && fabs(v) < 1e15) g_string_append_printf(o, "%lld", (long long)v);
	else { char b[48]; snprintf(b, sizeof b, "%.15g", v); g_string_append(o, b); }
}
static const char *ha_name(int ha) { return ha == 2 ? "center" : ha == 3 ? "right" : ha == 1 ? "left" : NULL; }
static int ha_num(const char *s) { return !s ? 0 : !g_ascii_strcasecmp(s, "left") ? 1 : !g_ascii_strcasecmp(s, "center") ? 2 : !g_ascii_strcasecmp(s, "right") ? 3 : 0; }
static int cell_styled(const Cell *c)
{
	return c && (c->b || c->i || c->u || c->s || c->ha || c->fc || c->bg || c->font || c->sz);
}
/* (a cell's style as the page keeps it: b, i, u, s as 1, ha as its own word, fc and bg as colours, font and sz) */
static void style_json(GString *o, const Cell *c)
{
	int first = 1;
	g_string_append_c(o, '{');
#define STFIELD(fmt, ...) do { if (!first) g_string_append_c(o, ','); g_string_append_printf(o, fmt, __VA_ARGS__); first = 0; } while (0)
	if (c->b) { if (!first) g_string_append_c(o, ','); g_string_append(o, "\"b\":1"); first = 0; }
	if (c->i) { if (!first) g_string_append_c(o, ','); g_string_append(o, "\"i\":1"); first = 0; }
	if (c->u) { if (!first) g_string_append_c(o, ','); g_string_append(o, "\"u\":1"); first = 0; }
	if (c->s) { if (!first) g_string_append_c(o, ','); g_string_append(o, "\"s\":1"); first = 0; }
	if (c->ha) { if (!first) g_string_append_c(o, ','); g_string_append_printf(o, "\"ha\":\"%s\"", ha_name(c->ha)); first = 0; }
	if (c->fc) { if (!first) g_string_append_c(o, ','); g_string_append(o, "\"fc\":"); j_str(o, c->fc); first = 0; }
	if (c->bg) { if (!first) g_string_append_c(o, ','); g_string_append(o, "\"bg\":"); j_str(o, c->bg); first = 0; }
	if (c->font) { if (!first) g_string_append_c(o, ','); g_string_append(o, "\"font\":"); j_str(o, c->font); first = 0; }
	if (c->sz) STFIELD("\"sz\":%d", c->sz);
#undef STFIELD
	g_string_append_c(o, '}');
}
/* (what the page keeps as a cell's value: a number, TRUE or FALSE, or the text -- its own words after the page's ') */
static void j_value(GString *o, int r, int c, const Cell *cl)
{
	const char *t = cl->t ? cl->t : "";
	if (t[0] == '\'') { j_str(o, t + 1); return; }
	Val v = eval_cell(r, c);
	if (v.k == V_NUM) j_num(o, v.n);
	else if (v.k == V_BOOL) g_string_append(o, v.b ? "true" : "false");
	else j_str(o, t);
}
static GArray *sorted_keys(void)
{
	GArray *ks = g_array_new(FALSE, FALSE, sizeof(guint));
	GHashTableIter it; gpointer k, v; g_hash_table_iter_init(&it, CELLS);
	while (g_hash_table_iter_next(&it, &k, &v)) { guint key = GPOINTER_TO_UINT(k); g_array_append_val(ks, key); }
	g_array_sort(ks, cmp_key);
	return ks;
}
static int sheet_save(const char *path, char **err)
{
	if (!path || !*path) { if (err) *err = g_strdup("no file"); return 0; }
	GArray *ks = sorted_keys();
	GPtrArray *sty = g_ptr_array_new_with_free_func(g_free);
	GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	for (guint i = 0; i < ks->len; i++) {
		guint key = g_array_index(ks, guint, i) - 1u; int r = (int)(key / MAXC), c = (int)(key % MAXC);
		Cell *cl = cell_get(r, c);
		if (!cell_styled(cl)) continue;
		GString *b = g_string_new(NULL); style_json(b, cl);
		if (!g_hash_table_contains(seen, b->str)) {
			g_hash_table_insert(seen, g_strdup(b->str), GINT_TO_POINTER(sty->len + 1));
			g_ptr_array_add(sty, g_strdup(b->str));
		}
		g_string_free(b, TRUE);
	}
	GString *o = g_string_new("{\"v\":2,\"app\":\"SpreadsheetPro\",\"active\":0,\"names\":{},\"styles\":[");
	for (guint i = 0; i < sty->len; i++) { if (i) g_string_append_c(o, ','); g_string_append(o, g_ptr_array_index(sty, i)); }
	g_string_append(o, "],\"sheets\":[{\"name\":\"Sheet1\",\"color\":\"\",\"cells\":[");
	int first = 1;
	for (guint i = 0; i < ks->len; i++) {
		guint key = g_array_index(ks, guint, i) - 1u; int r = (int)(key / MAXC), c = (int)(key % MAXC);
		Cell *cl = cell_get(r, c);
		if (!cl || !cl->t || !*cl->t) continue;
		int si = 0;
		if (cell_styled(cl)) {
			GString *b = g_string_new(NULL); style_json(b, cl);
			gpointer p = g_hash_table_lookup(seen, b->str); si = p ? GPOINTER_TO_INT(p) - 1 : 0;
			g_string_free(b, TRUE);
		}
		if (!first) g_string_append_c(o, ','); first = 0;
		g_string_append_printf(o, "[%u,", key);
		if (cl->t[0] == '=') { g_string_append(o, "null,"); j_str(o, cl->t); if (si || cell_styled(cl)) g_string_append_printf(o, ",%d", si); }
		else { j_value(o, r, c, cl); if (cell_styled(cl)) g_string_append_printf(o, ",null,%d", si); }
		g_string_append_c(o, ']');
	}
	g_string_append_printf(o, "],\"cw\":{},\"rh\":{},\"hid\":{},\"hc\":{},\"freeze\":{\"r\":0,\"c\":0},\"merges\":[],\"cf\":[],"
		"\"dv\":[],\"cm\":{},\"charts\":[],\"filter\":null,\"grid\":%s,\"print\":{}}]}\n", GRIDON ? "true" : "false");
	GError *ge = NULL;
	int ok = g_file_set_contents(path, o->str, (gssize)o->len, &ge);
	if (!ok && err) *err = g_strdup(ge ? ge->message : "could not be written");
	if (ge) g_error_free(ge);
	g_string_free(o, TRUE);
	g_ptr_array_free(sty, TRUE);
	g_hash_table_destroy(seen);
	g_array_free(ks, TRUE);
	if (!ok) return 0;
	g_free(FILE_PATH); FILE_PATH = g_strdup(path); DIRTY = 0; title_update();
	return 1;
}
/* ---- reading it back (the page's spFromJSON) ---- */
/* (a string read as a value must not be read back as a number or TRUE/FALSE: the page's own ' in front of it) */
static char *typed_text(const char *s)
{
	if (!s) return NULL;
	if (!*s) return g_strdup("");
	if (s[0] == '=' || s[0] == '\'') return g_strdup_printf("'%s", s);
	char *end = NULL;
	double d = g_ascii_strtod(s, &end); (void)d;
	if (end && end != s && !*end) return g_strdup_printf("'%s", s);
	if (!g_ascii_strcasecmp(s, "TRUE") || !g_ascii_strcasecmp(s, "FALSE")) return g_strdup_printf("'%s", s);
	int digits = 0, other = 0;
	for (const char *p = s; *p; p++) { if (g_ascii_isdigit(*p)) digits = 1; else if (!strchr("+-.,eE$%:/ ", *p)) other = 1; }
	if (digits && !other) return g_strdup_printf("'%s", s);   /* ($5, 5%, 1,200, a date, a time) */
	return g_strdup(s);
}
static void style_read(Cell *cl, RuiJ *so)
{
	cl->b = rui_jbool(so, "b"); cl->i = rui_jbool(so, "i"); cl->u = rui_jbool(so, "u"); cl->s = rui_jbool(so, "s");
	cl->ha = ha_num(rui_jstr(so, "ha"));
	const char *fc = rui_jstr(so, "fc"), *bg = rui_jstr(so, "bg"), *fo = rui_jstr(so, "font");
	if (fc && *fc) cl->fc = g_strdup(fc);
	if (bg && *bg) cl->bg = g_strdup(bg);
	if (fo && *fo) cl->font = g_strdup(fo);
	cl->sz = (int)rui_jnum(so, "sz", 0);
}
static int sheet_load(const char *path, char **err)
{
	char *text = NULL;
	if (!path || !*path) { if (err) *err = g_strdup("no file"); return 0; }
	if (!g_file_get_contents(path, &text, NULL, NULL)) { if (err) *err = g_strdup_printf("%s could not be read", path_base(path)); return 0; }
	RuiJ *o = rui_jparse(text); g_free(text);
	if (!o) { if (err) *err = g_strdup("not a spreadsheet"); return 0; }
	int ver = (int)rui_jnum(o, "v", 1);
	GHashTable *h = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, cell_free);
	RuiJ *sheets = rui_jprop(o, "sheets");
	if (sheets && rui_jlen(sheets)) {
		RuiJ *sh = rui_jat(sheets, 0), *cells = rui_jprop(sh, "cells"), *sty = rui_jprop(o, "styles");
		for (int i = 0; cells && i < rui_jlen(cells); i++) {
			RuiJ *a = rui_jat(cells, i);
			if (rui_jtype(a) != 4 || rui_jlen(a) < 2) continue;
			double kd = rui_jval(rui_jat(a, 0));
			if (kd < 0 || kd >= (double)MAXC * (double)MAXR) continue;
			guint key = (guint)kd; int r = (int)(key / MAXC), c = (int)(key % MAXC);
			RuiJ *fv = rui_jlen(a) > 2 ? rui_jat(a, 2) : NULL;
			const char *f = fv && rui_jtype(fv) == 3 ? rui_jsval(fv) : NULL;
			char *t = NULL;
			if (f && *f) t = g_strdup(f);
			else {
				RuiJ *vv = rui_jat(a, 1); int vt = rui_jtype(vv);
				if (vt == 2) { char b[48]; snprintf(b, sizeof b, "%.15g", rui_jval(vv)); t = g_strdup(b); }
				else if (vt == 1) t = g_strdup(rui_jval(vv) ? "TRUE" : "FALSE");
				else if (vt == 3) t = typed_text(rui_jsval(vv));
			}
			if (!t || !*t) { g_free(t); continue; }
			Cell *cl = g_new0(Cell, 1); cl->t = t;
			RuiJ *sv = rui_jlen(a) > 3 ? rui_jat(a, 3) : NULL;
			if (sv && rui_jtype(sv) == 2 && sty && rui_jlen(sty)) {
				int si = (int)rui_jval(sv);
				if (si >= 0 && si < rui_jlen(sty)) style_read(cl, rui_jat(sty, si));
			}
			g_hash_table_insert(h, GUINT_TO_POINTER(key + 1u), cl);
		}
	} else if (ver == 1) {   /* (the old Spreadsheet: {v:1, cells:{A1:{v, b}}}) */
		RuiJ *cells = rui_jprop(o, "cells");
		for (int i = 0; cells && i < rui_jlen(cells); i++) {
			const char *name = rui_jkey(cells, i);
			RuiJ *c = name ? rui_jprop(cells, name) : NULL;
			int r = 0, c0 = 0, r1 = 0, c1 = 0;
			if (!name || !c || !a1_parse(name, &r, &c0, &r1, &c1)) continue;
			const char *v = rui_jstr(c, "v");
			if (!v || !*v) continue;
			Cell *cl = g_new0(Cell, 1); cl->t = g_strdup(v);
			if (rui_jbool(c, "b")) cl->b = 1;
			g_hash_table_insert(h, GUINT_TO_POINTER(ckey(r, c0)), cl);
		}
	}
	rui_jfree(o);
	if (!g_hash_table_size(h)) { /* (an empty book is still a book: a new one, from the file's name) */ }
	SUSPEND = 1;
	{ GHashTable *old = CELLS; CELLS = h; g_hash_table_destroy(old); }
	UNDO_N = UNDO_I = 0; hist_push();
	CUR_R = CUR_C = SEL_R0 = SEL_C0 = SEL_R1 = SEL_C1 = SEL_AR = SEL_AC = 0;
	SCX = SCY = 0; TABFROM = -1; SHOWF = 0;
	g_free(FILE_PATH); FILE_PATH = g_strdup(path); DIRTY = 0;
	SUSPEND = 0;
	title_update();
	nb_update(); fx_update(); upd_tb(); layout_ranges(); status_update(); redraw();
	snprintf(SAY, sizeof SAY, "%s %s", rui_tr("Opened"), path_base(path));
	status_update();
	return 1;
}
/* ---- what File's rows and the page's keys do ---- */
static void file_note(const char *title, const char *msg)
{
	const char *b[] = { rui_tr("Close") };
	rui_confirm(W, rui_tr(title), msg ? msg : "", b, 1, 1, dlg_close, NULL);
}
static void do_open(const char *path)
{
	char *err = NULL;
	if (!sheet_load(path, &err)) { file_note("Open", err); g_free(err); return; }
	state_soon();
}
static void open_done(GtkNativeDialog *d, gint resp, gpointer u)
{
	(void)u;
	if (resp == GTK_RESPONSE_ACCEPT) {
		char *f = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));
		if (f) { do_open(f); g_free(f); }
	}
	g_object_unref(d);
}
static void file_open_dlg(void)
{
	GtkFileChooserNative *ch = gtk_file_chooser_native_new(rui_tr("Open a spreadsheet"), GTK_WINDOW(W->win),
		GTK_FILE_CHOOSER_ACTION_OPEN, rui_tr("Open"), rui_tr("Cancel"));
	GtkFileFilter *ff = gtk_file_filter_new();
	gtk_file_filter_add_pattern(ff, "*.RSHEET"); gtk_file_filter_add_pattern(ff, "*.rsheet");
	gtk_file_filter_set_name(ff, "SpreadsheetPro (.RSHEET)");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(ch), ff);
	g_signal_connect(ch, "response", G_CALLBACK(open_done), NULL);
	gtk_native_dialog_show(GTK_NATIVE_DIALOG(ch));
}
static void save_to(const char *path)
{
	char *err = NULL;
	if (!sheet_save(path, &err)) { file_note("Save", err); g_free(err); return; }
	snprintf(SAY, sizeof SAY, "%s %s", rui_tr("Saved"), path_base(path));
	status_update(); state_soon();
}
static void save_done(GtkNativeDialog *d, gint resp, gpointer u)
{
	(void)u;
	if (resp == GTK_RESPONSE_ACCEPT) {
		char *f = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));
		if (f) {
			const char *e = strrchr(f, '.');
			if (!e || (g_ascii_strcasecmp(e, ".RSHEET"))) { char *n = g_strconcat(f, ".RSHEET", NULL); save_to(n); g_free(n); }
			else save_to(f);
			g_free(f);
		}
	}
	g_object_unref(d);
}
static void file_save_as(void)
{
	GtkFileChooserNative *ch = gtk_file_chooser_native_new(rui_tr("Save as"), GTK_WINDOW(W->win),
		GTK_FILE_CHOOSER_ACTION_SAVE, rui_tr("Save"), rui_tr("Cancel"));
	const char *b = path_base(FILE_PATH);
	char *def = b && *b ? g_strdup(b) : g_strdup("Book1.RSHEET");
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(ch), def);
	g_free(def);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(ch), TRUE);
	g_signal_connect(ch, "response", G_CALLBACK(save_done), NULL);
	gtk_native_dialog_show(GTK_NATIVE_DIALOG(ch));
}
static void do_save(int as)
{
	if (!as && FILE_PATH && *FILE_PATH) { save_to(FILE_PATH); return; }
	file_save_as();
}

/* ------------------------------------------------------------------ the page's grid, drawn as its canvas is
   The head is 46 x 22 px (the row numbers and the column letters), a column 80 px and a row 20 px at 100 %, the grid lines
   #e2e3e3, the head #f3f4f6 with #444 letters, the choice #1a73e8 at 12 % with its 2 px border -- the page's own values
   (spro45.js's paint and drawHeaders, styles25.css's .sp-*). */
static double zf(void) { return ZOOM / 100.0; }
static char *col_name(int c)
{
	static char b[4][8]; static int i;
	char *o = b[i]; i = (i + 1) & 3;
	int n = c + 1, k = 0; char t[8];
	while (n > 0) { int m = (n - 1) % 26; t[k++] = (char)('A' + m); n = (n - 1) / 26; }
	for (int j = 0; j < k; j++) o[j] = t[k - 1 - j];
	o[k] = 0;
	return o;
}
static void cell_box(int r, int c, double *x, double *y, double *w, double *h)
{
	double z = zf();
	*x = HW + c * DW * z - SCX; *y = HH + r * DH * z - SCY; *w = DW * z; *h = DH * z;
}
/* (a text in the head: its column letter or its row number, centred) */
static void head_text(cairo_t *cr, PangoLayout *lay, const char *t, double cx, double cy, double px, const char *hex)
{
	PangoFontDescription *fd = pango_font_description_new();
	pango_font_description_set_family(fd, "Segoe UI, Arial, sans-serif");
	pango_font_description_set_absolute_size(fd, px * PANGO_SCALE);
	pango_layout_set_font_description(lay, fd); pango_layout_set_text(lay, t, -1);
	int tw = 0, th = 0; pango_layout_get_pixel_size(lay, &tw, &th);
	GdkRGBA rgb; if (gdk_rgba_parse(&rgb, hex)) cairo_set_source_rgb(cr, rgb.red, rgb.green, rgb.blue);
	cairo_move_to(cr, cx - tw / 2.0, cy - th / 2.0); pango_cairo_show_layout(cr, lay);
	pango_font_description_free(fd);
}
static void draw_cell_text(cairo_t *cr, PangoLayout *lay, int r, int c, double x, double y, double w, double h)
{
	Cell *cl = cell_get(r, c);
	char *t = cell_show(r, c);
	if (!t || !*t) { g_free(t); return; }
	double z = zf();
	PangoFontDescription *fd = pango_font_description_new();
	pango_font_description_set_family(fd, cl && cl->font && *cl->font ? cl->font : "Calibri, Carlito, Arial, sans-serif");
	pango_font_description_set_absolute_size(fd, (cl && cl->sz > 0 ? cl->sz : CELLPT) * 4 / 3 * z * PANGO_SCALE);
	pango_font_description_set_weight(fd, cl && cl->b ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
	pango_font_description_set_style(fd, cl && cl->i ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
	pango_layout_set_font_description(lay, fd);
	pango_layout_set_width(lay, (int)((w - 6) * PANGO_SCALE));
	pango_layout_set_ellipsize(lay, PANGO_ELLIPSIZE_END);
	pango_layout_set_text(lay, t, -1);
	int tw = 0, th = 0; pango_layout_get_pixel_size(lay, &tw, &th);
	Val v = eval_cell(r, c);
	gboolean have = FALSE; GdkRGBA rgb;
	if (cl && cl->fc) have = gdk_rgba_parse(&rgb, cl->fc);
	if (have) cairo_set_source_rgb(cr, rgb.red, rgb.green, rgb.blue);
	else if (v.k == V_ERR) cairo_set_source_rgb(cr, 0.75, 0, 0);
	else cairo_set_source_rgb(cr, 0, 0, 0);
	double tx = x + 3;
	if (cl && cl->ha == 2) tx = x + (w - tw) / 2;
	else if (cl && cl->ha == 3) tx = x + w - tw - 3;
	else if (!cl || !cl->ha) { if (v.k == V_NUM) tx = x + w - tw - 3; else if (v.k == V_BOOL || v.k == V_ERR) tx = x + (w - tw) / 2; }
	cairo_move_to(cr, tx, y + (h - th) / 2); pango_cairo_show_layout(cr, lay);
	if (cl && (cl->u || cl->s)) {
		cairo_set_line_width(cr, 1.4);
		double ly = cl->u ? y + h - 3.0 : y + h / 2.0, len = tw > w - 8 ? w - 8 : tw;
		cairo_move_to(cr, tx, ly); cairo_line_to(cr, tx + len, ly); cairo_stroke(cr);
	}
	pango_font_description_free(fd);
	g_free(t);
}
static void draw(GtkWidget *widget, cairo_t *cr, gpointer d)
{
	(void)d;
	int Wd = gtk_widget_get_allocated_width(widget), Ht = gtk_widget_get_allocated_height(widget);
	if (Wd < 4 || Ht < 4) return;
	double z = zf(), cw = DW * z, rh = DH * z;
	cairo_set_source_rgb(cr, 1, 1, 1); cairo_paint(cr);
	int c0 = (int)(SCX / cw), r0 = (int)(SCY / rh);
	if (c0 < 0) c0 = 0; if (c0 > MAXC - 1) c0 = MAXC - 1;
	if (r0 < 0) r0 = 0; if (r0 > MAXR - 1) r0 = MAXR - 1;
	int c1 = c0 + (int)((Wd - HW) / cw) + 1, r1 = r0 + (int)((Ht - HH) / rh) + 1;
	if (c1 > MAXC - 1) c1 = MAXC - 1; if (r1 > MAXR - 1) r1 = MAXR - 1;
	PangoLayout *lay = gtk_widget_create_pango_layout(widget, NULL);
	cairo_save(cr); cairo_rectangle(cr, HW, HH, Wd - HW, Ht - HH); cairo_clip(cr);
	for (int r = r0; r <= r1; r++) for (int c = c0; c <= c1; c++) {
		Cell *cl = cell_get(r, c);
		if (!cl || !cl->bg) continue;
		GdkRGBA rgb; if (!gdk_rgba_parse(&rgb, cl->bg)) continue;
		double x, y, w, h; cell_box(r, c, &x, &y, &w, &h);
		cairo_set_source_rgb(cr, rgb.red, rgb.green, rgb.blue); cairo_rectangle(cr, x, y, w, h); cairo_fill(cr);
	}
	if (GRIDON) {
		cairo_set_source_rgb(cr, 0.886, 0.890, 0.890); cairo_set_line_width(cr, 1);
		for (int c = c0; c <= c1 + 1 && c <= MAXC; c++) { double x = (int)(HW + c * cw - SCX) + 0.5; cairo_move_to(cr, x, HH); cairo_line_to(cr, x, Ht); }
		for (int r = r0; r <= r1 + 1 && r <= MAXR; r++) { double y = (int)(HH + r * rh - SCY) + 0.5; cairo_move_to(cr, HW, y); cairo_line_to(cr, Wd, y); }
		cairo_stroke(cr);
	}
	for (int r = r0; r <= r1; r++) for (int c = c0; c <= c1; c++) { double x, y, w, h; cell_box(r, c, &x, &y, &w, &h); draw_cell_text(cr, lay, r, c, x, y, w, h); }
	cairo_set_source_rgba(cr, 26 / 255.0, 115 / 255.0, 232 / 255.0, 0.12);
	for (int r = SEL_R0; r <= SEL_R1 && r <= MAXR - 1; r++) for (int c = SEL_C0; c <= SEL_C1 && c <= MAXC - 1; c++) {
		double x, y, w, h; cell_box(r, c, &x, &y, &w, &h); cairo_rectangle(cr, x, y, w, h); }
	cairo_fill(cr);
	{ double x, y, w, h; cell_box(CUR_R, CUR_C, &x, &y, &w, &h);
	  cairo_set_source_rgb(cr, 26 / 255.0, 115 / 255.0, 232 / 255.0); cairo_set_line_width(cr, 2);
	  cairo_rectangle(cr, x + 1, y + 1, w - 2, h - 2); cairo_stroke(cr); }
	cairo_restore(cr);
	/* the head, over the cells as the page draws it */
	cairo_set_source_rgb(cr, 0.953, 0.957, 0.965);
	cairo_rectangle(cr, 0, 0, Wd, HH); cairo_fill(cr);
	cairo_rectangle(cr, 0, 0, HW, Ht); cairo_fill(cr);
	for (int c = c0; c <= c1; c++) {
		double x = HW + c * cw - SCX;
		if (x + cw < HW || x > Wd) continue;
		int on = c >= SEL_C0 && c <= SEL_C1;
		if (on) { cairo_set_source_rgb(cr, 0.827, 0.890, 0.992); cairo_rectangle(cr, MAX(x, HW), 0, cw, HH); cairo_fill(cr); }
		head_text(cr, lay, col_name(c), x + cw / 2, HH / 2, 11 * z < 9 ? 9 : 11 * z, on ? "#0b57d0" : "#444444");
		cairo_set_source_rgb(cr, 0.820, 0.835, 0.859); cairo_rectangle(cr, x + cw - 1, 0, 1, HH); cairo_fill(cr);
	}
	for (int r = r0; r <= r1; r++) {
		double y = HH + r * rh - SCY;
		if (y + rh < HH || y > Ht) continue;
		int on = r >= SEL_R0 && r <= SEL_R1;
		if (on) { cairo_set_source_rgb(cr, 0.827, 0.890, 0.992); cairo_rectangle(cr, 0, y, HW, rh); cairo_fill(cr); }
		char n[16]; snprintf(n, sizeof n, "%d", r + 1);
		head_text(cr, lay, n, HW / 2, y + rh / 2, 11 * z < 9 ? 9 : 11 * z, on ? "#0b57d0" : "#444444");
		cairo_set_source_rgb(cr, 0.820, 0.835, 0.859); cairo_rectangle(cr, 0, y + rh - 1, HW, 1); cairo_fill(cr);
	}
	cairo_set_source_rgb(cr, 0.820, 0.835, 0.859);
	cairo_rectangle(cr, HW - 1, 0, 1, Ht); cairo_fill(cr);
	cairo_rectangle(cr, 0, HH - 1, Wd, 1); cairo_fill(cr);
	cairo_set_source_rgb(cr, 0.898, 0.906, 0.922); cairo_rectangle(cr, 0, 0, HW - 1, HH - 1); cairo_fill(cr);
	/* (the corner's own mark: the page draws a small triangle there) */
	cairo_set_source_rgb(cr, 0.612, 0.639, 0.686);
	cairo_move_to(cr, HW - 5, HH - 14); cairo_line_to(cr, HW - 5, HH - 5); cairo_line_to(cr, HW - 14, HH - 5); cairo_fill(cr);
	g_object_unref(lay);
}
/* (the grid's scroll bars: its own ranges, as the page's spacer gives the scrolling area) */
static void layout_ranges(void)
{
	if (!ADJV || !ADJH) return;
	double z = zf();
	int rows = used_r() + 25; if (rows < 60) rows = 60;
	int cols = used_c() + 10; if (cols < 26) cols = 26;
	int vw = ALLOCATED ? gtk_widget_get_allocated_width(AREA) : 960;
	int vh = ALLOCATED ? gtk_widget_get_allocated_height(AREA) : 500;
	gtk_adjustment_configure(ADJV, SCY, 0, HH + rows * DH * z, MAX(1, vh - 60), vh * 0.25, vh);
	gtk_adjustment_configure(ADJH, SCX, 0, HW + cols * DW * z, MAX(1, vw - 60), vw * 0.25, vw);
}
static void on_scroll(GtkAdjustment *a, gpointer d)
{
	if (a == ADJV) SCY = (int)gtk_adjustment_get_value(a); else SCX = (int)gtk_adjustment_get_value(a);
	(void)d; place_entry(); redraw();
}
static void ensure_visible(int r, int c)
{
	if (!ALLOCATED) return;
	double z = zf(), cw = DW * z, rh = DH * z;
	int vw = gtk_widget_get_allocated_width(AREA), vh = gtk_widget_get_allocated_height(AREA);
	int x0 = (int)(c * cw), x1 = (int)((c + 1) * cw), y0 = (int)(r * rh), y1 = (int)((r + 1) * rh);
	if (x0 < SCX) SCX = x0; else if (x1 > SCX + vw - (int)HW) SCX = x1 - vw + (int)HW;
	if (y0 < SCY) SCY = y0; else if (y1 > SCY + vh - (int)HH) SCY = y1 - vh + (int)HH;
	if (SCX < 0) SCX = 0; if (SCY < 0) SCY = 0;
	if (ADJH) gtk_adjustment_set_value(ADJH, SCX);
	if (ADJV) gtk_adjustment_set_value(ADJV, SCY);
}

/* ------------------------------------------------------------------ the selection, the name box and the formula bar */
static void nb_update(void)
{
	if (!NB) return;
	char t[40];
	if (SEL_R0 == SEL_R1 && SEL_C0 == SEL_C1) snprintf(t, sizeof t, "%s%d", col_name(CUR_C), CUR_R + 1);
	else snprintf(t, sizeof t, "%s%d:%s%d", col_name(SEL_C0), SEL_R0 + 1, col_name(SEL_C1), SEL_R1 + 1);
	gtk_entry_set_text(GTK_ENTRY(NB), t);
}
static void fx_update(void) { if (FXE) gtk_entry_set_text(GTK_ENTRY(FXE), cell_raw(CUR_R, CUR_C)); }
static void sel_set(int r, int c, int extend)
{
	if (r < 0) r = 0; if (r > MAXR - 1) r = MAXR - 1;
	if (c < 0) c = 0; if (c > MAXC - 1) c = MAXC - 1;
	CUR_R = r; CUR_C = c;
	if (extend) {
		SEL_R0 = MIN(SEL_AR, r); SEL_R1 = MAX(SEL_AR, r); SEL_C0 = MIN(SEL_AC, c); SEL_C1 = MAX(SEL_AC, c);
	} else {
		SEL_AR = r; SEL_AC = c; SEL_R0 = SEL_R1 = r; SEL_C0 = SEL_C1 = c;
	}
	nb_update(); fx_update(); upd_tb(); ensure_visible(r, c); status_update(); redraw();
}
static void sel_range(int r0, int c0, int r1, int c1)
{
	SEL_R0 = MIN(r0, r1); SEL_R1 = MAX(r0, r1); SEL_C0 = MIN(c0, c1); SEL_C1 = MAX(c0, c1);
	SEL_AR = SEL_R0; SEL_AC = SEL_C0; CUR_R = SEL_R0; CUR_C = SEL_C0;
	nb_update(); fx_update(); upd_tb(); status_update(); redraw();
}

/* ------------------------------------------------------------------ editing a cell (the page's .sp-ed over the cell)
   Typing starts it with what was typed, F2 with what the cell holds; Enter puts it in and goes down (to the column the row
   started in, when the row was typed with Tab), Tab puts it in and goes right, Escape is the page's cancel. */
static void place_entry(void)
{
	if (!ENTRY || !gtk_widget_get_visible(ENTRY)) return;
	double z = zf(), cw = DW * z, rh = DH * z;
	int x = (int)(HW + CUR_C * cw - SCX), y = (int)(HH + CUR_R * rh - SCY);
	gtk_widget_set_margin_start(ENTRY, MAX((int)HW, x));
	gtk_widget_set_margin_top(ENTRY, MAX((int)HH, y));
	gtk_widget_set_size_request(ENTRY, MAX((int)cw - 2, 60), MAX((int)rh, 18));
}
static void edit_begin(const char *init)
{
	if (EDITING) return;
	EDITING = 1;
	gtk_entry_set_text(GTK_ENTRY(ENTRY), init && *init ? init : (init ? "" : cell_raw(CUR_R, CUR_C)));
	place_entry(); gtk_widget_show(ENTRY); gtk_widget_grab_focus(ENTRY);
	gtk_editable_set_position(GTK_EDITABLE(ENTRY), -1);
	snprintf(SAY, sizeof SAY, "%s", rui_tr("Enter")); status_update(); redraw();
}
static void edit_end(void)
{
	if (!EDITING) return;
	EDITING = 0; gtk_widget_hide(ENTRY);
	snprintf(SAY, sizeof SAY, "%s", rui_tr("Ready"));
	status_update(); redraw();
}
static void edit_commit(int dr, int dc)
{
	if (!EDITING) return;
	char *t = g_strdup(gtk_entry_get_text(GTK_ENTRY(ENTRY)));
	int r = CUR_R, c = CUR_C;
	edit_end();
	if (strcmp(t, cell_raw(r, c))) { cell_set(r, c, t); hist_push(); }
	g_free(t);
	if (dc && !dr) { if (TABFROM < 0) TABFROM = c; }
	if (dr && !dc && TABFROM >= 0) { int f = TABFROM; TABFROM = -1; layout_ranges(); sel_set(r + 1, f, 0); return; }
	if (dr) TABFROM = -1;
	layout_ranges(); sel_set(r + dr, c + dc, 0);
}
static void edit_cancel(void) { if (!EDITING) return; edit_end(); fx_update(); }
static gboolean area_key(GtkWidget *w, GdkEventKey *e, gpointer d);
static void on_entry_activate(GtkWidget *w, gpointer d) { (void)w; (void)d; edit_commit(1, 0); gtk_widget_grab_focus(AREA); }
static gboolean on_entry_key(GtkWidget *w, GdkEventKey *e, gpointer d)
{
	(void)w; (void)d;
	if (e->keyval == GDK_KEY_Tab) { edit_commit(0, (e->state & GDK_SHIFT_MASK) ? -1 : 1); gtk_widget_grab_focus(AREA); return TRUE; }
	if (e->keyval == GDK_KEY_Escape) { edit_cancel(); gtk_widget_grab_focus(AREA); return TRUE; }
	return FALSE;
}

/* ------------------------------------------------------------------ the grid's keys (spro45.js's sc keydown) */
static int has_cell(int r, int c) { const char *t = cell_raw(r, c); return t && *t; }
static void jump_move(int dr, int dc, int shift)
{
	int r = CUR_R, c = CUR_C;
	if (has_cell(r, c) && has_cell(r + dr, c + dc)) {
		while (r + dr >= 0 && r + dr < MAXR && c + dc >= 0 && c + dc < MAXC && has_cell(r + dr, c + dc)) { r += dr; c += dc; }
		sel_set(r, c, shift); return;
	}
	int fr = -1, fc = -1;
	GHashTableIter it; gpointer k, v; g_hash_table_iter_init(&it, CELLS);
	while (g_hash_table_iter_next(&it, &k, &v)) {
		guint key = GPOINTER_TO_UINT(k) - 1u; int rr = (int)(key / MAXC), cc = (int)(key % MAXC);
		if (dr) { if (cc != c) continue; if (dr > 0 ? rr > r : rr < r) { if (fr < 0 || (dr > 0 ? rr < fr : rr > fr)) fr = rr; } }
		else { if (rr != r) continue; if (dc > 0 ? cc > c : cc < c) { if (fc < 0 || (dc > 0 ? cc < fc : cc > fc)) fc = cc; } }
	}
	if (dr) { if (fr < 0) fr = dr > 0 ? MAXR - 1 : 0; sel_set(fr, c, shift); }
	else { if (fc < 0) fc = dc > 0 ? MAXC - 1 : 0; sel_set(r, fc, shift); }
}
/* (Clear contents: the page's Delete) */
static void clear_cells(void)
{
	long area = (long)(SEL_R1 - SEL_R0 + 1) * (SEL_C1 - SEL_C0 + 1);
	if (area > 20000) return;
	for (int r = SEL_R0; r <= SEL_R1; r++) for (int c = SEL_C0; c <= SEL_C1; c++) cell_set(r, c, NULL);
	hist_push();
	layout_ranges(); status_update(); fx_update(); redraw();
}
static void clear_formats(void)
{
	for (int r = SEL_R0; r <= SEL_R1; r++) for (int c = SEL_C0; c <= SEL_C1; c++) {
		Cell *cl = cell_get(r, c); if (!cl) continue;
		cl->b = cl->i = cl->u = cl->s = 0; cl->ha = 0; cl->sz = 0;
		g_free(cl->fc); cl->fc = NULL; g_free(cl->bg); cl->bg = NULL; g_free(cl->font); cl->font = NULL;
	}
	hist_push();
	upd_tb(); redraw();
}
static void autosum(void)
{
	int r = CUR_R, c = CUR_C, top = -1, bot = -1;
	for (int rr = r - 1; rr >= 0; rr--) {
		Val v = eval_cell(rr, c);
		if (v.k == V_NUM) { if (bot < 0) bot = rr; top = rr; continue; }
		if (v.k == V_EMPTY && bot >= 0) break;
		break;
	}
	if (bot < 0) { snprintf(SAY, sizeof SAY, "%s", rui_tr("There are no numbers above to add up.")); status_update(); return; }
	char ca[8], cb[8]; snprintf(ca, sizeof ca, "%s", col_name(c)); snprintf(cb, sizeof cb, "%s", col_name(c));
	char t[64]; snprintf(t, sizeof t, "=SUM(%s%d:%s%d)", ca, top + 1, cb, bot + 1);
	cell_set(r, c, t); hist_push();
	fx_update(); status_update(); redraw();
}
static void copy_sel(int cut)
{
	GString *o = g_string_new(NULL);
	for (int r = SEL_R0; r <= SEL_R1; r++) {
		for (int c = SEL_C0; c <= SEL_C1; c++) { if (c > SEL_C0) g_string_append_c(o, '\t'); g_string_append(o, cell_raw(r, c)); }
		if (r < SEL_R1) g_string_append_c(o, '\n');
	}
	gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), o->str, (int)o->len);
	g_string_free(o, TRUE);
	snprintf(SAY, sizeof SAY, "%s", rui_tr(cut ? "Cut: choose where to paste" : "Copied"));
	if (cut) clear_cells(); else status_update();
}
static void paste_sel(void)
{
	char *t = gtk_clipboard_wait_for_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
	if (!t) return;
	char **rows = g_strsplit(t, "\n", -1);
	for (int i = 0; rows[i]; i++) {
		size_t n = strlen(rows[i]); while (n && (rows[i][n - 1] == '\r')) rows[i][--n] = 0;
		char **cols = g_strsplit(rows[i], "\t", -1);
		for (int j = 0; cols[j]; j++) { if (CUR_R + i >= MAXR || CUR_C + j >= MAXC) continue; cell_set(CUR_R + i, CUR_C + j, cols[j]); }
		g_strfreev(cols);
	}
	g_strfreev(rows); g_free(t);
	hist_push();
	layout_ranges(); fx_update(); status_update(); redraw();
}
static gboolean area_key(GtkWidget *w, GdkEventKey *e, gpointer d)
{
	(void)w; (void)d;
	int ctrl = (e->state & GDK_CONTROL_MASK) != 0, shift = (e->state & GDK_SHIFT_MASK) != 0;
	if (EDITING) return FALSE;
	switch (e->keyval) {
	case GDK_KEY_Left: case GDK_KEY_Right: case GDK_KEY_Up: case GDK_KEY_Down: {
		int dr = e->keyval == GDK_KEY_Up ? -1 : e->keyval == GDK_KEY_Down ? 1 : 0;
		int dc = e->keyval == GDK_KEY_Left ? -1 : e->keyval == GDK_KEY_Right ? 1 : 0;
		if (ctrl) { jump_move(dr, dc, shift); return TRUE; }
		if (shift) { sel_set(CUR_R + dr, CUR_C + dc, 1); return TRUE; }
		TABFROM = -1; sel_set(CUR_R + dr, CUR_C + dc, 0); return TRUE; }
	case GDK_KEY_Tab: TABFROM = TABFROM < 0 && !shift ? CUR_C : TABFROM; sel_set(CUR_R, CUR_C + (shift ? -1 : 1), 0); return TRUE;
	case GDK_KEY_Return: case GDK_KEY_KP_Enter:
		if (shift) { sel_set(CUR_R - 1, CUR_C, 0); return TRUE; }
		if (TABFROM >= 0) { int f = TABFROM; TABFROM = -1; sel_set(CUR_R + 1, f, 0); return TRUE; }
		sel_set(CUR_R + 1, CUR_C, 0); return TRUE;
	case GDK_KEY_Home:
		if (ctrl) { SCX = SCY = 0; sel_set(0, 0, shift); } else sel_set(CUR_R, 0, shift);
		return TRUE;
	case GDK_KEY_End:
		if (ctrl) sel_set(MAX(0, used_r()), MAX(0, used_c()), shift); else sel_set(CUR_R, MAX(0, used_c()), shift);
		return TRUE;
	case GDK_KEY_F2: edit_begin(NULL); return TRUE;
	case GDK_KEY_F9: layout_ranges(); status_update(); redraw(); return TRUE;
	case GDK_KEY_Delete: case GDK_KEY_BackSpace:
		if (e->keyval == GDK_KEY_BackSpace) { edit_begin(""); return TRUE; }
		clear_cells(); return TRUE;
	case GDK_KEY_Escape: TABFROM = -1; return TRUE;
	default: break;
	}
	if (ctrl) {
		switch (e->keyval) {
		case GDK_KEY_n: new_book(); return TRUE;
		case GDK_KEY_o: file_open_dlg(); return TRUE;
		case GDK_KEY_s: do_save((e->state & GDK_SHIFT_MASK) ? 1 : 0); return TRUE;
		case GDK_KEY_z: hist_undo(); return TRUE;
		case GDK_KEY_y: hist_redo(); return TRUE;
		case GDK_KEY_a: sel_range(0, 0, MAX(0, used_r()), MAX(0, used_c())); return TRUE;
		case GDK_KEY_c: copy_sel(0); return TRUE;
		case GDK_KEY_x: copy_sel(1); return TRUE;
		case GDK_KEY_v: paste_sel(); return TRUE;
		case GDK_KEY_b: on_style_btn(NULL, GINT_TO_POINTER(0)); return TRUE;
		case GDK_KEY_i: on_style_btn(NULL, GINT_TO_POINTER(1)); return TRUE;
		case GDK_KEY_u: on_style_btn(NULL, GINT_TO_POINTER(2)); return TRUE;
		case GDK_KEY_5: on_style_btn(NULL, GINT_TO_POINTER(3)); return TRUE;
		case GDK_KEY_grave: case GDK_KEY_backslash: SHOWF = !SHOWF; redraw(); status_update(); return TRUE;
		case GDK_KEY_g: if (NB) gtk_widget_grab_focus(NB); return TRUE;
		default: return FALSE;
		}
	}
	if ((e->state & GDK_MOD1_MASK) && (e->keyval == GDK_KEY_equal || e->keyval == GDK_KEY_plus)) { autosum(); return TRUE; }
	gunichar c = gdk_keyval_to_unicode(e->keyval);
	if (c && !g_unichar_iscntrl(c) && !(e->state & (GDK_MOD1_MASK | GDK_CONTROL_MASK))) {
		char b[8]; int n = g_unichar_to_utf8(c, b); b[n] = 0;
		edit_begin(b); return TRUE;
	}
	return FALSE;
}
static gboolean area_click(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)d;
	gtk_widget_grab_focus(w);
	if (e->button != 1) return TRUE;
	double z = zf(), cw = DW * z, rh = DH * z;
	int c = (int)((e->x - HW + SCX) / cw), r = (int)((e->y - HH + SCY) / rh);
	if (e->x < HW || e->y < HH) return TRUE;   /* (a press on the head: choosing a whole row or column is a later step) */
	TABFROM = -1;
	sel_set(r, c, (e->state & GDK_SHIFT_MASK) != 0);
	return TRUE;
}
static gboolean area_scroll(GtkWidget *w, GdkEventScroll *e, gpointer d)
{
	(void)w; (void)d;
	int step = 3;
	if (e->direction == GDK_SCROLL_UP || e->direction == GDK_SCROLL_DOWN)
		SCY = MAX(0, SCY + (e->direction == GDK_SCROLL_UP ? -1 : 1) * (int)(DH * zf()) * step);
	else SCX = MAX(0, SCX + (e->direction == GDK_SCROLL_LEFT ? -1 : 1) * (int)(DW * zf()) * step);
	if (ADJV) gtk_adjustment_set_value(ADJV, SCY);
	if (ADJH) gtk_adjustment_set_value(ADJH, SCX);
	place_entry(); redraw();
	return TRUE;
}
static gboolean on_area_size(GtkWidget *w, GdkRectangle *a, gpointer d)
{
	(void)w; (void)a; (void)d;
	ALLOCATED = 1; layout_ranges(); place_entry();
	return FALSE;
}

/* ------------------------------------------------------------------ what the toolbar changes (the page's setSt and
   styleSel): bold, italic, underline, strikethrough, the alignment, the text colour, the fill colour, the font and its size */
typedef struct { int what, val; const char *str; } StArg;
static void st_apply(Cell *cl, void *d)
{
	StArg *a = d;
	switch (a->what) {
	case 0: cl->b = (unsigned)a->val; break;
	case 1: cl->i = (unsigned)a->val; break;
	case 2: cl->u = (unsigned)a->val; break;
	case 3: cl->s = (unsigned)a->val; break;
	case 4: cl->ha = a->val; break;
	case 5: g_free(cl->fc); cl->fc = a->str && *a->str ? g_strdup(a->str) : NULL; break;
	case 6: g_free(cl->bg); cl->bg = a->str && *a->str ? g_strdup(a->str) : NULL; break;
	case 7: g_free(cl->font); cl->font = a->str && *a->str ? g_strdup(a->str) : NULL; break;
	case 8: cl->sz = a->val; break;
	default: break;
	}
}
static int cur_is(int which)
{
	Cell *cl = cell_get(CUR_R, CUR_C); if (!cl) return 0;
	switch (which) { case 0: return (int)cl->b; case 1: return (int)cl->i; case 2: return (int)cl->u; case 3: return (int)cl->s; case 4: return cl->ha; default: return 0; }
}
static void on_class(GtkWidget *b, int on)
{
	GtkStyleContext *c = gtk_widget_get_style_context(b);
	if (on) gtk_style_context_add_class(c, "wp-on"); else gtk_style_context_remove_class(c, "wp-on");
}
static void on_style_btn(GtkWidget *b, gpointer d)
{
	(void)b; if (!READY) return; int what = GPOINTER_TO_INT(d);
	StArg a = { what, cur_is(what) ? 0 : 1, NULL };
	style_sel(st_apply, &a);
}
static void on_align_btn(GtkWidget *b, gpointer d)
{
	(void)b; if (!READY) return; int ha = GPOINTER_TO_INT(d);
	StArg a = { 4, cur_is(4) == ha ? 0 : ha, NULL };
	style_sel(st_apply, &a);
}
static void on_font(GtkComboBox *c, gpointer d)
{
	(void)d; if (UPDATING || !READY) return;
	int i = gtk_combo_box_get_active(c); if (i < 0 || i >= NSPFONTS) return;
	StArg a = { 7, 0, SP_FONTS[i] }; style_sel(st_apply, &a);
}
static void on_fsize(GtkComboBox *c, gpointer d)
{
	(void)d; if (UPDATING || !READY) return;
	int i = gtk_combo_box_get_active(c); if (i < 0 || i >= NSP_SIZES) return;
	StArg a = { 8, SP_SIZES[i], NULL }; style_sel(st_apply, &a);
}
static void on_colour(GtkColorButton *b, gpointer d)
{
	if (!READY) return;
	GdkRGBA c; gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(b), &c);
	char hex[8]; snprintf(hex, sizeof hex, "#%02x%02x%02x", (int)lround(c.red * 255), (int)lround(c.green * 255), (int)lround(c.blue * 255));
	StArg a = { GPOINTER_TO_INT(d) == 0 ? 5 : 6, 0, hex }; style_sel(st_apply, &a);
}
static void on_sum_btn(GtkWidget *b, gpointer d) { (void)b; (void)d; autosum(); }
static void on_undo_btn(GtkWidget *b, gpointer d) { (void)b; (void)d; hist_undo(); }
static void on_redo_btn(GtkWidget *b, gpointer d) { (void)b; (void)d; hist_redo(); }
/* (the bar's own marks: what the cell under the cursor has, as the page's updTb) */
static void upd_tb(void)
{
	if (!BTN_B) return;
	UPDATING = 1;
	Cell *cl = cell_get(CUR_R, CUR_C);
	on_class(BTN_B, cl && cl->b); on_class(BTN_I, cl && cl->i); on_class(BTN_U, cl && cl->u); on_class(BTN_S, cl && cl->s);
	int ha = cl ? cl->ha : 0;
	for (int k = 0; k < 3; k++) on_class(ALN[k], ha == k + 1);
	if (FONTW) {
		int hit = 0;
		for (int i = 0; i < NSPFONTS; i++) if (cl && cl->font && !strcmp(cl->font, SP_FONTS[i])) hit = i;
		gtk_combo_box_set_active(GTK_COMBO_BOX(FONTW), hit);
	}
	if (SIZEW) {
		int hit = 0, sz = cl && cl->sz > 0 ? cl->sz : 11;
		for (int i = 0; i < NSP_SIZES; i++) if (SP_SIZES[i] == sz) hit = i;
		gtk_combo_box_set_active(GTK_COMBO_BOX(SIZEW), hit);
	}
	UPDATING = 0;
}

/* ------------------------------------------------------------------ the status line (the page's: what the choice adds
   up to -- Sum, Average, Min, Max and Count of more than one cell -- and the zoom) */
static void status_update(void)
{
	char agg[240] = "";
	long area = (long)(SEL_R1 - SEL_R0 + 1) * (long)(SEL_C1 - SEL_C0 + 1);
	int nums = 0, cnt = 0; double sum = 0, mn = 0, mx = 0;
	if (area > 1 && area <= 20000)
		for (int r = SEL_R0; r <= SEL_R1; r++) for (int c = SEL_C0; c <= SEL_C1; c++) {
			Val v = eval_cell(r, c);
			if (v.k == V_NUM) { if (!nums) { mn = mx = v.n; } else { if (v.n < mn) mn = v.n; if (v.n > mx) mx = v.n; } nums++; sum += v.n; cnt++; }
			else if (v.k == V_TEXT || v.k == V_BOOL || v.k == V_ERR) cnt++;
		}
	/* (the figures once: as the page's status line writes them, and as plain numbers for the state the tests read) */
	if (nums) {
		char a[40], b[40], m1[40], m2[40];
		num_text(a, sizeof a, sum); num_text(b, sizeof b, sum / nums);
		num_text(m1, sizeof m1, mn); num_text(m2, sizeof m2, mx);
		snprintf(agg, sizeof agg, "%s: %s   %s: %s   %s: %s   %s: %s   %s: %d", rui_tr("Sum"), a, rui_tr("Average"), b,
			rui_tr("Min"), m1, rui_tr("Max"), m2, rui_tr("Count"), cnt);
		snprintf(AGGS, sizeof AGGS, "%.10g|%.10g|%.10g|%.10g|%d", sum, sum / nums, mn, mx, cnt);
	} else {
		if (cnt) snprintf(agg, sizeof agg, "%s: %d", rui_tr("Count"), cnt);
		snprintf(AGGS, sizeof AGGS, "-|-|-|-|%d", cnt);
	}
	if (STATUS) gtk_label_set_text(GTK_LABEL(STATUS), rui_tr(SAY));
	if (AGG) gtk_label_set_text(GTK_LABEL(AGG), agg);
	if (ZTXT) { char z[16]; snprintf(z, sizeof z, "%d%%", ZOOM); gtk_label_set_text(GTK_LABEL(ZTXT), z); }
	if (ZOOMSL) { int u = UPDATING; UPDATING = 1; gtk_range_set_value(GTK_RANGE(ZOOMSL), ZOOM); UPDATING = u; }
	state_soon();
}

/* ------------------------------------------------------------------ what the tests read (RUDOLF_SP_STATE) */
static gint cmp_key(gconstpointer a, gconstpointer b)
{
	guint x = *(const guint *)a, y = *(const guint *)b;
	return x < y ? -1 : x > y ? 1 : 0;
}
static void state_write(void)
{
	const char *f = getenv("RUDOLF_SP_STATE"); if (!f) return;
	GString *s = g_string_new(NULL);
	char title[96] = "";
	if (W && W->win) { const char *t = gtk_window_get_title(GTK_WINDOW(W->win)); if (t) snprintf(title, sizeof title, "%s", t); }
	g_string_append_printf(s, "title %s\nmenus %d\n", title, menus_n());
	g_string_append_printf(s, "file %s\ndirty %d\n", FILE_PATH && *FILE_PATH ? path_base(FILE_PATH) : "-", DIRTY);
	if (MENU_OPEN) g_string_append_printf(s, "menu %s %d\n", MENU_OPEN, MENU_OPEN_N);
	g_string_append_printf(s, "sel %d %d %d %d %d %d\n", CUR_R, CUR_C, SEL_R0, SEL_C0, SEL_R1, SEL_C1);
	{ char t[48];
	  if (SEL_R0 == SEL_R1 && SEL_C0 == SEL_C1) snprintf(t, sizeof t, "%s%d", col_name(CUR_C), CUR_R + 1);
	  else snprintf(t, sizeof t, "%s%d:%s%d", col_name(SEL_C0), SEL_R0 + 1, col_name(SEL_C1), SEL_R1 + 1);
	  g_string_append_printf(s, "selname %s\n", t); }
	g_string_append_printf(s, "fx %s\ncells %d\n", cell_raw(CUR_R, CUR_C), cell_n());
	GArray *ks = g_array_new(FALSE, FALSE, sizeof(guint));
	GHashTableIter it; gpointer k, v; g_hash_table_iter_init(&it, CELLS);
	while (g_hash_table_iter_next(&it, &k, &v)) { guint key = GPOINTER_TO_UINT(k); g_array_append_val(ks, key); }
	g_array_sort(ks, cmp_key);
	for (guint i = 0; i < ks->len; i++) {
		guint key = g_array_index(ks, guint, i) - 1u; int r = (int)(key / MAXC), c = (int)(key % MAXC);
		char a1[8]; snprintf(a1, sizeof a1, "%s", col_name(c));
		char *show = cell_show(r, c);
		g_string_append_printf(s, "cell %s%d %s\nshow %s%d %s\n", a1, r + 1, cell_raw(r, c), a1, r + 1, show ? show : "");
		g_free(show);
		Cell *cl = cell_get(r, c);
		if (cl && (cl->b || cl->i || cl->u || cl->s || cl->ha || cl->fc || cl->bg || cl->font || cl->sz))
			g_string_append_printf(s, "fmt %s%d b%u i%u u%u s%u ha%d fc%s bg%s\n", a1, r + 1, cl->b, cl->i, cl->u, cl->s,
				cl->ha, cl->fc ? cl->fc : "-", cl->bg ? cl->bg : "-");
	}
	g_array_free(ks, TRUE);
	g_string_append_printf(s, "zoom %d\nhist %d %d\nagg %s\nsay %s\n", ZOOM, UNDO_I, UNDO_N, AGGS, SAY);
	char *tmp = g_strdup_printf("%s.t", f); g_file_set_contents(tmp, s->str, -1, NULL); rename(tmp, f); g_free(tmp);
	g_string_free(s, TRUE);
}
static gboolean state_tick(gpointer d) { (void)d; state_write(); return G_SOURCE_REMOVE; }
static void state_soon(void) { if (getenv("RUDOLF_SP_STATE")) g_timeout_add(120, state_tick, NULL); }

/* ------------------------------------------------------------------ the look: the page's own values for what GTK does
   not draw as it (base_v24.html's .wp-tb .wp-mb .wp-sb, styles25.css's .sp-fb .sp-nb .sp-fx .sp-tabs .sp-tab .sp-ed) */
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
		".sp-fb{background:#f9fafb;border-bottom:1px solid #d1d5db}"
		"entry.sp-nb{border:1px solid #d1d5db;border-radius:4px;background:#fff;font-size:12px;color:#111;padding:0 4px;min-height:0}"
		"label.sp-fxl{color:#5f6368;font-style:italic;font-size:13px}"
		"entry.sp-fx{border:1px solid #d1d5db;border-radius:4px;background:#fff;font-size:12px;color:#111;padding:0 4px;min-height:0}"
		".sp-tabs{background:#f9fafb;border-top:1px solid #d1d5db}"
		"button.sp-tab{border:1px solid #d1d5db;border-bottom-color:transparent;border-radius:4px 4px 0 0;background:#f3f4f6;font-size:12px;color:#444;padding:0 12px;min-height:22px;min-width:0}"
		"button.sp-tab.on{background:#fff;color:#0b57d0;font-weight:600}"
		"button.sp-tab:disabled{color:#9ca3af}"
		"entry.sp-ed{border:2px solid #1a73e8;border-radius:0;background:#fff;font-size:12px;color:#111;padding:0 2px;min-height:0}";
	GtkCssProvider *p = gtk_css_provider_new();
	gtk_css_provider_load_from_data(p, T, -1, NULL);
	gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(p), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref(p);
}
static void dlg_close(int i, void *d) { (void)i; (void)d; }

/* ------------------------------------------------------------------ the eight menus: the page's own items, in its own
   order, each of them either working here or drawn as the page draws one it cannot choose (greyed out) */
enum { OP_NONE = 0, OP_NEW, OP_CLOSE, OP_FOPEN, OP_FSAVE, OP_FSAVEAS, OP_UNDO, OP_REDO, OP_CUT, OP_COPY, OP_PASTE, OP_SELALL, OP_CLEAR, OP_CLEARF, OP_CLEARA,
	OP_ZOOM, OP_FBAR, OP_GRID, OP_SHOWF, OP_BOLD, OP_ITAL, OP_UNDER, OP_STRIKE, OP_AL, OP_RECALC, OP_ABOUT, OP_KEYS, OP_STATS,
	OP_ERRORS, OP_GOTO, OP_WHEN };
typedef struct { const char *label, *key; int off; int op; const char *arg; } Mi;
#define WPS { NULL, NULL, 0, OP_NONE, NULL }
static const Mi FILE_I[] = {
	{ "New", "Ctrl+N", 0, OP_NEW, NULL }, { "New from template...", "", 1, OP_NONE, NULL },
	{ "Open...", "Ctrl+O", 0, OP_FOPEN, NULL }, { "Save", "Ctrl+S", 0, OP_FSAVE, NULL },
	{ "Save as... (.RSHEET, .XLSX, .CSV)", "", 0, OP_FSAVEAS, NULL }, WPS,
	{ "Import CSV into a new sheet...", "", 1, OP_NONE, NULL }, { "Export this sheet as CSV...", "", 1, OP_NONE, NULL },
	{ "Export as Excel workbook (.xlsx)...", "", 1, OP_NONE, NULL }, WPS,
	{ "Page setup...", "", 1, OP_NONE, NULL }, { "Print...", "Ctrl+P", 1, OP_NONE, NULL },
	{ "Save as PDF...", "", 1, OP_NONE, NULL }, WPS, { "Close", "", 0, OP_CLOSE, NULL } };
static const Mi EDIT_I[] = {
	{ "Undo", "Ctrl+Z", 0, OP_UNDO, NULL }, { "Redo", "Ctrl+Y", 0, OP_REDO, NULL }, WPS,
	{ "Cut", "Ctrl+X", 0, OP_CUT, NULL }, { "Copy", "Ctrl+C", 0, OP_COPY, NULL },
	{ "Paste", "Ctrl+V", 0, OP_PASTE, NULL }, { "Paste values only", "", 1, OP_NONE, NULL },
	{ "Paste formats only", "", 1, OP_NONE, NULL }, { "Paste formulas only", "", 1, OP_NONE, NULL },
	{ "Paste transposed", "", 1, OP_NONE, NULL }, WPS,
	{ "Fill down", "Ctrl+D", 1, OP_NONE, NULL }, { "Fill right", "Ctrl+R", 1, OP_NONE, NULL },
	{ "Clear contents", "Del", 0, OP_CLEAR, NULL }, { "Clear formats", "", 0, OP_CLEARF, NULL },
	{ "Clear all", "", 0, OP_CLEARA, NULL }, { "Delete cells...", "", 1, OP_NONE, NULL }, WPS,
	{ "Select all", "Ctrl+A", 0, OP_SELALL, NULL }, { "Find...", "Ctrl+F", 1, OP_NONE, NULL },
	{ "Replace...", "", 1, OP_NONE, NULL }, { "Go to...", "Ctrl+G", 0, OP_GOTO, NULL } };
static const Mi VIEW_I[] = {
	{ "Formula bar", "", 0, OP_FBAR, NULL }, { "Grid lines", "", 0, OP_GRID, NULL },
	{ "Show formulas", "Ctrl+`", 0, OP_SHOWF, NULL }, WPS,
	{ "Freeze panes (above and left of the selected cell)", "", 1, OP_NONE, NULL },
	{ "Freeze top row", "", 1, OP_NONE, NULL }, { "Freeze first column", "", 1, OP_NONE, NULL },
	{ "Unfreeze panes", "", 1, OP_NONE, NULL }, WPS,
	{ "Zoom 50%", "", 0, OP_ZOOM, "50" }, { "Zoom 75%", "", 0, OP_ZOOM, "75" },
	{ "Zoom 100%", "", 0, OP_ZOOM, "100" }, { "Zoom 125%", "", 0, OP_ZOOM, "125" },
	{ "Zoom 150%", "", 0, OP_ZOOM, "150" }, { "Zoom 200%", "", 0, OP_ZOOM, "200" },
	{ "Full screen", "", 1, OP_NONE, NULL }, { "Show Notey, the assistant", "", 1, OP_NONE, NULL } };
static const Mi INSERT_I[] = {
	{ "Cells...", "", 1, OP_NONE, NULL }, { "Rows", "", 1, OP_NONE, NULL },
	{ "Columns", "", 1, OP_NONE, NULL }, { "Sheet", "", 1, OP_NONE, NULL }, WPS,
	{ "Chart...", "", 1, OP_NONE, NULL }, { "Function...", "", 1, OP_NONE, NULL },
	{ "Comment...", "", 1, OP_NONE, NULL }, { "Name...", "", 1, OP_NONE, NULL }, WPS,
	{ "Today's date", "Ctrl+;", 0, OP_WHEN, "date" }, { "The time now", "Ctrl+Shift+;", 0, OP_WHEN, "time" } };
static const Mi FORMAT_I[] = {
	{ "Bold", "Ctrl+B", 0, OP_BOLD, NULL }, { "Italic", "Ctrl+I", 0, OP_ITAL, NULL },
	{ "Underline", "Ctrl+U", 0, OP_UNDER, NULL }, { "Strikethrough", "Ctrl+5", 0, OP_STRIKE, NULL }, WPS,
	{ "Align left", "", 0, OP_AL, "l" }, { "Align centre", "", 0, OP_AL, "c" }, { "Align right", "", 0, OP_AL, "r" }, WPS,
	{ "Number: General", "", 1, OP_NONE, NULL }, { "Number: 0.00", "", 1, OP_NONE, NULL },
	{ "Currency", "", 1, OP_NONE, NULL }, { "Percent", "", 1, OP_NONE, NULL },
	{ "Date", "", 1, OP_NONE, NULL }, { "Time", "", 1, OP_NONE, NULL }, WPS,
	{ "More decimal places", "", 1, OP_NONE, NULL }, { "Fewer decimal places", "", 1, OP_NONE, NULL }, WPS,
	{ "Row height...", "", 1, OP_NONE, NULL }, { "Column width...", "", 1, OP_NONE, NULL },
	{ "Auto-fit column width", "", 1, OP_NONE, NULL }, { "Auto-fit row height", "", 1, OP_NONE, NULL }, WPS,
	{ "Hide rows", "", 1, OP_NONE, NULL }, { "Unhide rows", "", 1, OP_NONE, NULL },
	{ "Hide columns", "", 1, OP_NONE, NULL }, { "Unhide columns", "", 1, OP_NONE, NULL }, WPS,
	{ "Conditional formatting...", "", 1, OP_NONE, NULL }, { "Data validation...", "", 1, OP_NONE, NULL },
	{ "Clear formatting", "", 0, OP_CLEARF, NULL }, WPS, { "Paint format", "", 1, OP_NONE, NULL } };
static const Mi DATA_I[] = {
	{ "Sort ascending", "", 1, OP_NONE, NULL }, { "Sort descending", "", 1, OP_NONE, NULL },
	{ "Sort...", "", 1, OP_NONE, NULL }, { "Filter", "", 1, OP_NONE, NULL }, WPS,
	{ "Recalculate now", "F9", 0, OP_RECALC, NULL }, { "Import data from a file...", "", 1, OP_NONE, NULL },
	{ "Split text to columns...", "", 1, OP_NONE, NULL }, { "Data validation...", "", 1, OP_NONE, NULL }, WPS,
	{ "Named ranges...", "", 1, OP_NONE, NULL }, { "Pivot table...", "", 1, OP_NONE, NULL } };
static const Mi TOOLS_I[] = {
	{ "Check for errors", "", 0, OP_ERRORS, NULL }, { "Statistics", "", 0, OP_STATS, NULL },
	{ "Protect the sheet...", "", 1, OP_NONE, NULL }, WPS, { "Macros...", "", 1, OP_NONE, NULL } };
static const Mi HELP_I[] = {
	{ "Keyboard shortcuts", "", 0, OP_KEYS, NULL }, { "About SpreadsheetPro", "", 0, OP_ABOUT, NULL },
	WPS, { "Documentation for SpreadsheetPro", "", 1, OP_NONE, NULL } };
static const struct { const char *title; const Mi *items; int n; } MENUS[] = {
	{ "File", FILE_I, G_N_ELEMENTS(FILE_I) }, { "Edit", EDIT_I, G_N_ELEMENTS(EDIT_I) },
	{ "View", VIEW_I, G_N_ELEMENTS(VIEW_I) }, { "Insert", INSERT_I, G_N_ELEMENTS(INSERT_I) },
	{ "Format", FORMAT_I, G_N_ELEMENTS(FORMAT_I) }, { "Data", DATA_I, G_N_ELEMENTS(DATA_I) },
	{ "Tools", TOOLS_I, G_N_ELEMENTS(TOOLS_I) }, { "Help", HELP_I, G_N_ELEMENTS(HELP_I) } };
#define NMENUS ((int)G_N_ELEMENTS(MENUS))
static int menus_n(void) { return NMENUS; }
static GtkWidget *FBARB;

static void zoom_to(int z)
{
	if (z < 50) z = 50; if (z > 200) z = 200;
	ZOOM = z;
	layout_ranges(); status_update(); redraw();
}
static void new_book(void)
{
	GHashTable *old = CELLS;
	CELLS = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, cell_free);
	g_hash_table_destroy(old);
	CUR_R = CUR_C = SEL_R0 = SEL_C0 = SEL_R1 = SEL_C1 = SEL_AR = SEL_AC = 0;
	SCX = SCY = 0; TABFROM = -1; SHOWF = 0;
	SUSPEND = 1; hist_push(); SUSPEND = 0;
	g_free(FILE_PATH); FILE_PATH = NULL; DIRTY = 0; title_update();
	layout_ranges(); nb_update(); fx_update(); upd_tb(); status_update(); redraw();
}
static void show_keys(void)
{
	const char *t = rui_tr("Arrows: move\nTab, Shift+Tab: a cell right, left\nEnter: the cell below (and the row's first column,\n"
		"when the row was typed with Tab)\nShift+Enter: the cell above\nCtrl+arrows: to the edge of the data\n"
		"Shift+arrows: choose more cells\nF2: change the cell\nDelete: clear the cells\nCtrl+Z, Ctrl+Y: undo, redo\n"
		"Ctrl+B, Ctrl+I, Ctrl+U, Ctrl+5: bold, italic, underline, strikethrough\n"
		"Ctrl+C, Ctrl+X, Ctrl+V: copy, cut, paste\nCtrl+A: choose every cell with something in it\n"
		"Ctrl+`: show the formulas\nAlt+=: add up the numbers above\nCtrl+Home: A1");
	const char *b[] = { rui_tr("Close") };
	rui_confirm(W, rui_tr("Keyboard shortcuts"), t, b, 1, 1, dlg_close, NULL);
}
static void show_stats(void)
{
	int formulas = 0, errs = 0, nums = 0; double sum = 0;
	GHashTableIter it; gpointer k, v; g_hash_table_iter_init(&it, CELLS);
	while (g_hash_table_iter_next(&it, &k, &v)) {
		Cell *cl = v;
		if (cl->t && cl->t[0] == '=') formulas++;
		guint key = GPOINTER_TO_UINT(k) - 1u; int r = (int)(key / MAXC), c = (int)(key % MAXC);
		Val val = eval_cell(r, c);
		if (val.k == V_ERR) errs++; else if (val.k == V_NUM) { nums++; sum += val.n; }
	}
	char a[40], t[400]; num_text(a, sizeof a, sum);
	snprintf(t, sizeof t, "%s: %d\n%s: %d\n%s: %d\n%s: %d\n%s: %s", rui_tr("Cells with something in them"), cell_n(),
		rui_tr("Formulas"), formulas, rui_tr("Numbers"), nums, rui_tr("Errors"), errs, rui_tr("Sum of the numbers"), a);
	const char *b[] = { rui_tr("Close") };
	rui_confirm(W, rui_tr("Statistics"), t, b, 1, 1, dlg_close, NULL);
}
static void check_errors(void)
{
	GString *t = g_string_new(NULL); int errs = 0;
	GHashTableIter it; gpointer k, v; g_hash_table_iter_init(&it, CELLS);
	while (g_hash_table_iter_next(&it, &k, &v)) {
		guint key = GPOINTER_TO_UINT(k) - 1u; int r = (int)(key / MAXC), c = (int)(key % MAXC);
		Val val = eval_cell(r, c);
		if (val.k != V_ERR) continue;
		errs++;
		if (errs <= 10) { char a1[8]; snprintf(a1, sizeof a1, "%s", col_name(c));
			g_string_append_printf(t, "%s%d: %s\n", a1, r + 1, val.err ? val.err : "#VALUE!"); }
	}
	if (!errs) g_string_append(t, rui_tr("No cell of this sheet is an error."));
	const char *b[] = { rui_tr("Close") };
	rui_confirm(W, rui_tr("Check for errors"), t->str, b, 1, 1, dlg_close, NULL);
	g_string_free(t, TRUE);
}
static void menu_do(const Mi *s)
{
	switch (s->op) {
	case OP_NEW: new_book(); break;
	case OP_CLOSE: gtk_widget_destroy(W->win); break;
	case OP_FOPEN: file_open_dlg(); break;
	case OP_FSAVE: do_save(0); break;
	case OP_FSAVEAS: do_save(1); break;
	case OP_UNDO: hist_undo(); break;
	case OP_REDO: hist_redo(); break;
	case OP_CUT: copy_sel(1); break;
	case OP_COPY: copy_sel(0); break;
	case OP_PASTE: paste_sel(); break;
	case OP_SELALL: sel_range(0, 0, MAX(0, used_r()), MAX(0, used_c())); break;
	case OP_CLEAR: clear_cells(); break;
	case OP_CLEARF: clear_formats(); break;
	case OP_CLEARA: clear_cells(); break;
	case OP_ZOOM: zoom_to(s->arg ? atoi(s->arg) : 100); break;
	case OP_FBAR:
		FBAR = !FBAR;
		if (FBARB) { if (FBAR) gtk_widget_show(FBARB); else gtk_widget_hide(FBARB); }
		status_update();
		break;
	case OP_GRID: GRIDON = !GRIDON; redraw(); status_update(); break;
	case OP_SHOWF: SHOWF = !SHOWF; redraw(); status_update(); break;
	case OP_BOLD: on_style_btn(NULL, GINT_TO_POINTER(0)); break;
	case OP_ITAL: on_style_btn(NULL, GINT_TO_POINTER(1)); break;
	case OP_UNDER: on_style_btn(NULL, GINT_TO_POINTER(2)); break;
	case OP_STRIKE: on_style_btn(NULL, GINT_TO_POINTER(3)); break;
	case OP_AL: on_align_btn(NULL, GINT_TO_POINTER(!strcmp(s->arg, "l") ? 1 : !strcmp(s->arg, "c") ? 2 : 3)); break;
	case OP_WHEN: {
		time_t now = time(NULL); struct tm tmv; struct tm *tm = localtime_r(&now, &tmv);
		if (!tm) break;
		char b[64];
		if (!strcmp(s->arg, "date")) strftime(b, sizeof b, "%d %B %Y", tm); else strftime(b, sizeof b, "%H:%M", tm);
		cell_set(CUR_R, CUR_C, b); hist_push();
		fx_update(); status_update(); redraw();
		break; }
	case OP_RECALC: layout_ranges(); fx_update(); upd_tb(); status_update(); redraw(); break;
	case OP_GOTO: if (NB) gtk_widget_grab_focus(NB); break;
	case OP_KEYS: show_keys(); break;
	case OP_STATS: show_stats(); break;
	case OP_ERRORS: check_errors(); break;
	case OP_ABOUT:
		rui_about(W, "SpreadsheetPro 1.0", rui_tr("A spreadsheet for Rudolf OS with formulas, references and ranges, "
			"undo and redo, the fonts and the colours, and the page's own keys. Sheets are saved as .RSHEET files that the page "
			"and Oracle can also open."));
		break;
	default: break;
	}
}
static void menu_pick(int i, void *d)
{
	int m = (int)(intptr_t)d; MENU_OPEN = NULL;
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
	GtkAllocation a; gtk_widget_get_allocation(b, &a); int ox = 0, oy = 0;
	if (gtk_widget_get_window(b)) gdk_window_get_origin(gtk_widget_get_window(b), &ox, &oy);
	MENU_OPEN = MENUS[m].title; MENU_OPEN_N = MENUS[m].n;
	rui_menu(b, ox, oy + a.height, NULL, it, MENUS[m].n, menu_pick, (void *)(intptr_t)m);
	g_free(it); state_soon();
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

/* ------------------------------------------------------------------ the toolbar, as the page's .sp-tb has it: the font and
   its size, bold, italic, underline, strikethrough, the two colours, the three alignments, the sum, undo, redo, the clear,
   and the items of a later step (the number formats, wrap, merge, borders, sorting, filtering, charts) drawn greyed */
static GtkWidget *tb_button(const char *label, const char *tip, GCallback cb, gpointer d)
{
	GtkWidget *b = gtk_button_new(); GtkWidget *l = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(l), label); gtk_container_add(GTK_CONTAINER(b), l);
	gtk_style_context_add_class(gtk_widget_get_style_context(b), "rui");
	gtk_style_context_add_class(gtk_widget_get_style_context(b), "wp-tb");
	gtk_widget_set_tooltip_text(b, rui_tr(tip)); gtk_widget_set_can_focus(b, FALSE);
	if (cb) g_signal_connect(b, "clicked", cb, d);
	gtk_widget_set_size_request(b, (int)lround(rui_px(26)), (int)lround(rui_px(26)));
	return b;
}
static void tb_add(GtkWidget *fb, GtkWidget *w) { gtk_flow_box_insert(GTK_FLOW_BOX(fb), w, -1); }
static void tb_sep(GtkWidget *fb)
{
	GtkWidget *s = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_size_request(s, (int)lround(rui_px(1)), (int)lround(rui_px(20)));
	gtk_widget_set_margin_start(s, (int)lround(rui_px(3))); gtk_widget_set_margin_end(s, (int)lround(rui_px(3)));
	gtk_style_context_add_class(gtk_widget_get_style_context(s), "wp-sep");
	gtk_flow_box_insert(GTK_FLOW_BOX(fb), s, -1);
}
static GtkWidget *tb_off(GtkWidget *b) { gtk_widget_set_sensitive(b, FALSE); return b; }
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
	GtkWidget *ff = gtk_combo_box_text_new();
	for (int i = 0; i < NSPFONTS; i++) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ff), SP_FONTS[i]);
	gtk_widget_set_tooltip_text(ff, rui_tr("Font"));
	gtk_widget_set_size_request(ff, (int)lround(rui_px(124)), (int)lround(rui_px(26)));
	gtk_style_context_add_class(gtk_widget_get_style_context(ff), "wp-tb");
	gtk_combo_box_set_active(GTK_COMBO_BOX(ff), 0);
	g_signal_connect(ff, "changed", G_CALLBACK(on_font), NULL);
	FONTW = ff; tb_add(TB, ff);
	GtkWidget *sz = gtk_combo_box_text_new();
	for (int i = 0; i < NSP_SIZES; i++) { char b[8]; snprintf(b, sizeof b, "%d", SP_SIZES[i]); gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sz), b); }
	gtk_widget_set_tooltip_text(sz, rui_tr("Font size"));
	gtk_widget_set_size_request(sz, (int)lround(rui_px(54)), (int)lround(rui_px(26)));
	gtk_style_context_add_class(gtk_widget_get_style_context(sz), "wp-tb");
	SIZEW = sz; tb_add(TB, sz);
	g_signal_connect(sz, "changed", G_CALLBACK(on_fsize), NULL);
	tb_sep(TB);
	BTN_B = tb_button("<b>B</b>", "Bold (Ctrl+B)", G_CALLBACK(on_style_btn), GINT_TO_POINTER(0)); tb_add(TB, BTN_B);
	BTN_I = tb_button("<i>I</i>", "Italic (Ctrl+I)", G_CALLBACK(on_style_btn), GINT_TO_POINTER(1)); tb_add(TB, BTN_I);
	BTN_U = tb_button("<u>U</u>", "Underline (Ctrl+U)", G_CALLBACK(on_style_btn), GINT_TO_POINTER(2)); tb_add(TB, BTN_U);
	BTN_S = tb_button("<s>S</s>", "Strikethrough (Ctrl+5)", G_CALLBACK(on_style_btn), GINT_TO_POINTER(3)); tb_add(TB, BTN_S);
	tb_sep(TB);
	{ GdkRGBA c0 = { 0.06, 0.06, 0.06, 1.0 };
	  GtkWidget *fc = gtk_color_button_new_with_rgba(&c0);
	  gtk_widget_set_tooltip_text(fc, rui_tr("Text colour"));
	  gtk_widget_set_size_request(fc, (int)lround(rui_px(26)), (int)lround(rui_px(26)));
	  gtk_style_context_add_class(gtk_widget_get_style_context(fc), "wp-tb");
	  g_signal_connect(fc, "color-set", G_CALLBACK(on_colour), GINT_TO_POINTER(0)); tb_add(TB, fc); }
	{ GdkRGBA c1 = { 1.0, 0.95, 0.6, 1.0 };
	  GtkWidget *bg = gtk_color_button_new_with_rgba(&c1);
	  gtk_widget_set_tooltip_text(bg, rui_tr("Fill colour"));
	  gtk_widget_set_size_request(bg, (int)lround(rui_px(26)), (int)lround(rui_px(26)));
	  gtk_style_context_add_class(gtk_widget_get_style_context(bg), "wp-tb");
	  g_signal_connect(bg, "color-set", G_CALLBACK(on_colour), GINT_TO_POINTER(1)); tb_add(TB, bg); }
	tb_sep(TB);
	ALN[0] = tb_button("\xe2\x87\xa4", "Align left", G_CALLBACK(on_align_btn), GINT_TO_POINTER(1)); tb_add(TB, ALN[0]);
	ALN[1] = tb_button("\xe2\x86\x94", "Align centre", G_CALLBACK(on_align_btn), GINT_TO_POINTER(2)); tb_add(TB, ALN[1]);
	ALN[2] = tb_button("\xe2\x87\xa5", "Align right", G_CALLBACK(on_align_btn), GINT_TO_POINTER(3)); tb_add(TB, ALN[2]);
	tb_sep(TB);
	tb_add(TB, tb_button("\xce\xa3", "Add up the numbers above (Alt+=)", G_CALLBACK(on_sum_btn), NULL));
	tb_sep(TB);
	tb_add(TB, tb_button("\xe2\x86\xb6", "Undo (Ctrl+Z)", G_CALLBACK(on_undo_btn), NULL));
	tb_add(TB, tb_button("\xe2\x86\xb7", "Redo (Ctrl+Y)", G_CALLBACK(on_redo_btn), NULL));
	tb_add(TB, tb_button("\xe2\x8c\xab", "Clear the cells (Delete)", G_CALLBACK(on_clear_btn), NULL));
	tb_sep(TB);
	{ GtkWidget *nf = gtk_combo_box_text_new();
	  for (int i = 0; i < NSPNF; i++) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(nf), rui_tr(SP_NF[i].name));
	  gtk_combo_box_set_active(GTK_COMBO_BOX(nf), 0);
	  gtk_widget_set_tooltip_text(nf, rui_tr("Number format (a later step)"));
	  gtk_widget_set_size_request(nf, (int)lround(rui_px(104)), (int)lround(rui_px(26)));
	  gtk_style_context_add_class(gtk_widget_get_style_context(nf), "wp-tb"); tb_add(TB, tb_off(nf)); }
	tb_add(TB, tb_off(tb_button("\xe2\x86\xb5", "Wrap text (a later step)", NULL, NULL)));
	tb_add(TB, tb_off(tb_button("\xe2\x87\x94", "Merge and centre (a later step)", NULL, NULL)));
	tb_add(TB, tb_off(tb_button("\xe2\x96\xa6", "Borders (a later step)", NULL, NULL)));
	tb_sep(TB);
	tb_add(TB, tb_off(tb_button("A\xe2\x86\x93" "Z", "Sort ascending (a later step)", NULL, NULL)));
	tb_add(TB, tb_off(tb_button("Z\xe2\x86\x93" "A", "Sort descending (a later step)", NULL, NULL)));
	tb_add(TB, tb_off(tb_button("\xe2\x96\xbd", "Filter (a later step)", NULL, NULL)));
	tb_add(TB, tb_off(tb_button("\xe2\x97\x8a", "Chart (a later step)", NULL, NULL)));
	gtk_box_pack_start(GTK_BOX(W->body), TB, FALSE, FALSE, 0);
}

/* ------------------------------------------------------------------ the name box and the formula bar (.sp-fb) */
static int a1_parse(const char *t, int *r0, int *c0, int *r1, int *c1)
{
	if (!t) return 0;
	GRegex *re = g_regex_new("^\\s*([A-Za-z]{1,3})([0-9]{1,7})(?:\\s*:\\s*([A-Za-z]{1,3})([0-9]{1,7}))?\\s*$", 0, 0, NULL);
	GMatchInfo *mi = NULL; int ok = 0;
	if (g_regex_match(re, t, 0, &mi)) {
		char *l1 = g_match_info_fetch(mi, 1), *d1 = g_match_info_fetch(mi, 2);
		char *l2 = g_match_info_fetch(mi, 3), *d2 = g_match_info_fetch(mi, 4);
		int ca = 0, cb = 0;
		for (char *x = l1; *x; x++) ca = ca * 26 + (g_ascii_toupper(*x) - 'A' + 1);
		ca--; int ra = atoi(d1) - 1;
		int cc = ca, rc = ra;
		if (l2 && *l2) { for (char *x = l2; *x; x++) cb = cb * 26 + (g_ascii_toupper(*x) - 'A' + 1); cc = cb - 1; rc = atoi(d2) - 1; }
		ok = ca >= 0 && ca < MAXC && ra >= 0 && ra < MAXR && cc >= 0 && cc < MAXC && rc >= 0 && rc < MAXR;
		if (ok) { *r0 = ra; *c0 = ca; *r1 = rc; *c1 = cc; }
		g_free(l1); g_free(d1); g_free(l2); g_free(d2);
	}
	g_match_info_free(mi); g_regex_unref(re);
	return ok;
}
static void on_nb(GtkWidget *w, gpointer d)
{
	(void)d; int r0, c0, r1, c1;
	if (a1_parse(gtk_entry_get_text(GTK_ENTRY(w)), &r0, &c0, &r1, &c1)) sel_range(r0, c0, r1, c1);
	else { snprintf(SAY, sizeof SAY, "%s", rui_tr("That is not a cell's name.")); status_update(); }
	gtk_widget_grab_focus(AREA);
}
static void on_fx(GtkWidget *w, gpointer d)
{
	(void)d;
	char *t = g_strdup(gtk_entry_get_text(GTK_ENTRY(w)));
	if (strcmp(t, cell_raw(CUR_R, CUR_C))) { cell_set(CUR_R, CUR_C, t); hist_push(); }
	g_free(t);
	layout_ranges(); fx_update(); upd_tb(); status_update(); redraw();
	gtk_widget_grab_focus(AREA);
}
static void build_formula(void)
{
	FBARB = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (int)lround(rui_px(6)));
	gtk_widget_set_margin_start(FBARB, (int)lround(rui_px(6))); gtk_widget_set_margin_end(FBARB, (int)lround(rui_px(6)));
	gtk_widget_set_margin_top(FBARB, (int)lround(rui_px(3))); gtk_widget_set_margin_bottom(FBARB, (int)lround(rui_px(3)));
	gtk_style_context_add_class(gtk_widget_get_style_context(FBARB), "sp-fb");
	NB = gtk_entry_new();
	gtk_widget_set_size_request(NB, (int)lround(rui_px(88)), (int)lround(rui_px(24)));
	gtk_entry_set_alignment(GTK_ENTRY(NB), 0.5);
	gtk_widget_set_tooltip_text(NB, rui_tr("The name box: type a cell's name (A1) or a range's (A1:D4)"));
	gtk_style_context_add_class(gtk_widget_get_style_context(NB), "sp-nb");
	g_signal_connect(NB, "activate", G_CALLBACK(on_nb), NULL);
	gtk_box_pack_start(GTK_BOX(FBARB), NB, FALSE, FALSE, 0);
	GtkWidget *fx = gtk_label_new("fx");
	gtk_style_context_add_class(gtk_widget_get_style_context(fx), "sp-fxl");
	gtk_box_pack_start(GTK_BOX(FBARB), fx, FALSE, FALSE, 0);
	FXE = gtk_entry_new();
	gtk_widget_set_hexpand(FXE, TRUE);
	gtk_widget_set_tooltip_text(FXE, rui_tr("What the cell holds: a value, or a formula starting with ="));
	gtk_style_context_add_class(gtk_widget_get_style_context(FXE), "sp-fx");
	g_signal_connect(FXE, "activate", G_CALLBACK(on_fx), NULL);
	gtk_box_pack_start(GTK_BOX(FBARB), FXE, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(W->body), FBARB, FALSE, FALSE, 0);
}
/* (the grid, its scroll bars and the cell being typed in over it) */
static void build_grid(void)
{
	AREAOV = gtk_overlay_new();
	AREA = gtk_drawing_area_new();
	gtk_widget_set_hexpand(AREA, TRUE); gtk_widget_set_vexpand(AREA, TRUE);
	gtk_widget_add_events(AREA, GDK_BUTTON_PRESS_MASK | GDK_SCROLL_MASK | GDK_KEY_PRESS_MASK);
	gtk_widget_set_can_focus(AREA, TRUE);
	g_signal_connect(AREA, "draw", G_CALLBACK(draw), NULL);
	g_signal_connect(AREA, "key-press-event", G_CALLBACK(area_key), NULL);
	g_signal_connect(AREA, "button-press-event", G_CALLBACK(area_click), NULL);
	g_signal_connect(AREA, "scroll-event", G_CALLBACK(area_scroll), NULL);
	g_signal_connect(AREA, "size-allocate", G_CALLBACK(on_area_size), NULL);
	gtk_container_add(GTK_CONTAINER(AREAOV), AREA);
	ENTRY = gtk_entry_new();
	gtk_widget_set_halign(ENTRY, GTK_ALIGN_START); gtk_widget_set_valign(ENTRY, GTK_ALIGN_START);
	gtk_style_context_add_class(gtk_widget_get_style_context(ENTRY), "sp-ed");
	g_signal_connect(ENTRY, "activate", G_CALLBACK(on_entry_activate), NULL);
	g_signal_connect(ENTRY, "key-press-event", G_CALLBACK(on_entry_key), NULL);
	gtk_overlay_add_overlay(GTK_OVERLAY(AREAOV), ENTRY);
	ADJV = gtk_adjustment_new(0, 0, 100, 1, 10, 10);
	ADJH = gtk_adjustment_new(0, 0, 100, 1, 10, 10);
	g_signal_connect(ADJV, "value-changed", G_CALLBACK(on_scroll), NULL);
	g_signal_connect(ADJH, "value-changed", G_CALLBACK(on_scroll), NULL);
	GtkWidget *vb = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, ADJV);
	GtkWidget *hb = gtk_scrollbar_new(GTK_ORIENTATION_HORIZONTAL, ADJH);
	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(row), AREAOV, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(row), vb, FALSE, FALSE, 0);
	GtkWidget *col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start(GTK_BOX(col), row, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(col), hb, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(W->body), col, TRUE, TRUE, 0);
}
/* (the sheet's tabs, as the page's .sp-tabs: one sheet, and the button that would add another) */
static void build_tabs(void)
{
	GtkWidget *t = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (int)lround(rui_px(2)));
	gtk_widget_set_margin_start(t, (int)lround(rui_px(6))); gtk_widget_set_margin_top(t, (int)lround(rui_px(2)));
	gtk_style_context_add_class(gtk_widget_get_style_context(t), "sp-tabs");
	GtkWidget *add = gtk_button_new_with_label("+");
	gtk_widget_set_size_request(add, (int)lround(rui_px(24)), (int)lround(rui_px(22)));
	gtk_widget_set_tooltip_text(add, rui_tr("Another sheet (a later step)"));
	gtk_widget_set_can_focus(add, FALSE); tb_off(add);
	gtk_box_pack_start(GTK_BOX(t), add, FALSE, FALSE, 0);
	GtkWidget *s1 = gtk_button_new_with_label(rui_tr("Sheet1"));
	gtk_widget_set_can_focus(s1, FALSE);
	gtk_style_context_add_class(gtk_widget_get_style_context(s1), "rui");
	gtk_style_context_add_class(gtk_widget_get_style_context(s1), "sp-tab");
	gtk_style_context_add_class(gtk_widget_get_style_context(s1), "on");
	gtk_box_pack_start(GTK_BOX(t), s1, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(W->body), t, FALSE, FALSE, 0);
}
static void on_zoom(GtkRange *r, gpointer d)
{
	(void)d; if (UPDATING) return;
	ZOOM = (int)gtk_range_get_value(r);
	layout_ranges(); redraw(); status_update();
}
static void build_status(void)
{
	GtkWidget *sb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (int)lround(rui_px(14)));
	gtk_style_context_add_class(gtk_widget_get_style_context(sb), "wp-sb");
	gtk_widget_set_margin_start(sb, (int)lround(rui_px(8))); gtk_widget_set_margin_end(sb, (int)lround(rui_px(8)));
	gtk_widget_set_margin_top(sb, (int)lround(rui_px(4))); gtk_widget_set_margin_bottom(sb, (int)lround(rui_px(4)));
	STATUS = gtk_label_new(rui_tr(SAY));
	AGG = gtk_label_new(""); gtk_widget_set_hexpand(AGG, TRUE); gtk_label_set_xalign(GTK_LABEL(AGG), 0);
	gtk_label_set_ellipsize(GTK_LABEL(AGG), PANGO_ELLIPSIZE_END);
	ZTXT = gtk_label_new("100%");
	ZOOMSL = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 50, 200, 25);
	gtk_scale_set_draw_value(GTK_SCALE(ZOOMSL), FALSE);
	gtk_widget_set_size_request(ZOOMSL, (int)lround(rui_px(110)), -1);
	gtk_widget_set_can_focus(ZOOMSL, FALSE);
	gtk_widget_set_tooltip_text(ZOOMSL, rui_tr("Zoom"));
	g_signal_connect(ZOOMSL, "value-changed", G_CALLBACK(on_zoom), NULL);
	gtk_box_pack_start(GTK_BOX(sb), STATUS, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(sb), AGG, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(sb), ZTXT, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(sb), ZOOMSL, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(W->body), sb, FALSE, FALSE, 0);
}
static void on_clear_btn(GtkWidget *b, gpointer d) { (void)b; (void)d; clear_cells(); }
static gboolean goto_hook(gpointer d)
{
	int r0, c0, r1, c1;
	if (a1_parse((const char *)d, &r0, &c0, &r1, &c1)) sel_range(r0, c0, r1, c1);
	gtk_widget_grab_focus(AREA);
	return G_SOURCE_REMOVE;
}
int main(int argc, char **argv)
{
	rui_init(&argc, &argv, "sheetpro");
	css();
	W = rui_window("sheetpro", "SpreadsheetPro", 1000, 660);
	CELLS = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, cell_free);
	VISIT = g_hash_table_new(g_direct_hash, g_direct_equal);
	build_menubar(); build_toolbar(); build_formula(); build_grid(); build_tabs(); build_status();
	gtk_window_set_title(GTK_WINDOW(W->win), "Book1 - SpreadsheetPro");
	hist_push();
	gtk_widget_show_all(W->win);
	gtk_widget_hide(ENTRY);
	READY = 1;
	nb_update(); fx_update(); upd_tb(); layout_ranges(); status_update();
	gtk_widget_grab_focus(AREA);
	{ const char *f = argc > 1 ? argv[1] : NULL;
	  if (f && *f && f[0] != '-') do_open(f); }
	{ const char *mo = getenv("RUDOLF_SP_MENU");
	  if (mo && *mo) for (int i = 0; i < NMENUS; i++) if (!strcmp(MENUS[i].title, mo)) { g_timeout_add(400, menu_hook, (gpointer)(intptr_t)i); break; } }
	{ const char *go = getenv("RUDOLF_SP_GOTO"); if (go && *go) g_timeout_add(900, goto_hook, (gpointer)go); }
	state_soon();
	gtk_main();
	return 0;
}

