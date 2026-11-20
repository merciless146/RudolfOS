/*
 * litepoint.h - the window transitions' scenes, "Splitting-Merging LitePoint" (Rudolf OS 1.6.0, docs/plan-1.6.0.md Part 17).
 * The same as desktop/litepoint60.js's lpScene (the page's), for the native desktop (rudolf-native.c): what is where at a moment
 * of a transition: the lines, the points of light, the part of the window shown, the taskbar button lit.
 * iso/qt/shell2/litepoint60_t.js compares the two (it builds iso/qt/host/litepoint_dump.c with this file).
 * Copyright (C) 2026 Rudolf OS contributors.  GPL-2.0-or-later.
 */
#ifndef RUDOLF_LITEPOINT_H
#define RUDOLF_LITEPOINT_H
#include <math.h>
#include <string.h>

/* the windows' six (Part 17); the start menu's opening and closing, the taskbar's pop-ups, right-click menus and a window's
   taskbar button (Part 17b, 2026-11-18: moving away from the taskbar's side, the menus where they are) */
enum { LP_C2C, LP_S2S, LP_MAX, LP_MIN, LP_RES, LP_CLS, LP_SMO, LP_SMC, LP_POP, LP_CTX, LP_TBB, LP_KINDS };
static const char *const LP_NAME[LP_KINDS] = { "c2c", "s2s", "max", "min", "res", "cls", "smo", "smc", "pop", "ctx", "tbb" };
static const int LP_MS[LP_KINDS] = { 160, 220, 160, 160, 160, 160, 120, 120, 120, 80, 80 };   /* (2026-11-19, the owner: as litepoint60.js LP_MS) */
/* each one's steps, as shares of its time (the demo's, which the owner chose) */
static const double LP_W[LP_KINDS][5] = { { .07, .29, .29, .35 }, { .06, .2, .22, .2, .32 }, { .07, .3, .3, .33 }, { .3, .22, .18, .3 }, { .3, .36, .34 }, { .32, .3, .38 },
	{ .15, .22, .24, .14, .25 }, { .3, .22, .18, .3 }, { .26, .28, .16, .3 }, { .07, .29, .29, .35 }, { .3, .3, .4 } };
static const int LP_NW[LP_KINDS] = { 4, 5, 4, 4, 3, 3, 5, 4, 4, 4, 3 };

typedef struct { double x, y, w, h; } LPRect;
/* the window or menu (for max: as it was; for res: as it will be), maximized, the button it comes from or goes to; the taskbar's
   side (0 bottom, 1 top, 2 left, 3 right: the light moves away from it); a right-click menu's corner at the pointer (0 top-left,
   1 top-right, 2 bottom-right, 3 bottom-left) */
typedef struct { LPRect f, s; double bx, by; int dir, corner; } LPGeo;
#define LP_NL 24
#define LP_NP 12
typedef struct {
	double L[LP_NL][5]; int nl;   /* lines: x1 y1 x2 y2 alpha */
	double P[LP_NP][4]; int np;   /* points: x y size red */
	int geo_to;                   /* the window's geometry: 1 as after, 0 as before */
	int clip; LPRect cr;          /* clip 1: only cr of it shown (0 wide: none) */
	double lit;
} LPScene;

static double lp_e(double t) { return t < .5 ? 4 * t * t * t : 1 - pow(-2 * t + 2, 3) / 2; }
static double lp_c(double v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static double lp_l(double a, double b, double t) { return a + (b - a) * t; }
static LPRect lp_r(LPRect a, LPRect b, double t) { LPRect r = { lp_l(a.x, b.x, t), lp_l(a.y, b.y, t), lp_l(a.w, b.w, t), lp_l(a.h, b.h, t) }; return r; }
static LPRect lp_z(LPRect a) { LPRect r = { a.x, a.y, 0, 0 }; return r; }
static double lp_step(int k, double t, int i) { double a = 0; for (int j = 0; j < i; j++) a += LP_W[k][j]; return lp_c((t - a) / LP_W[k][i]); }
static void lp_at(int k, double t, int *i, double *p)
{
	double a = 0; for (int j = 0; j < LP_NW[k]; j++) { double b = a + LP_W[k][j]; if (t <= b || j == LP_NW[k] - 1) { *i = j; *p = lp_c((t - a) / LP_W[k][j]); return; } a = b; }
}
static void lp_line(LPScene *o, double ax, double ay, double bx, double by, double a)
{
	if (a <= 0 || o->nl >= LP_NL) return; double *l = o->L[o->nl++]; l[0] = ax; l[1] = ay; l[2] = bx; l[3] = by; l[4] = a > 1 ? 1 : a;
}
static void lp_light(LPScene *o, double x, double y, double k, double r) { if (o->np >= LP_NP) return; double *p = o->P[o->np++]; p[0] = x; p[1] = y; p[2] = k; p[3] = lp_c(r); }
static void lp_rect(LPScene *o, LPRect R, double a)
{
	lp_line(o, R.x, R.y, R.x + R.w, R.y, a); lp_line(o, R.x + R.w, R.y, R.x + R.w, R.y + R.h, a); lp_line(o, R.x + R.w, R.y + R.h, R.x, R.y + R.h, a); lp_line(o, R.x, R.y + R.h, R.x, R.y, a);
}
/* the three drawn on a frame (c2c, s2s, max): where the points start, where they meet, each step's lines in the frame's 0..1 */
typedef struct { int no; double o[2][2]; double m[2][2]; double from[2]; int ns[5]; double seg[5][4][4]; } LPFr;
static const LPFr LP_FR[3] = {
	{ 1, { { 0, 0 } }, { { 1, 1 } }, { 1, 1 }, { 0, 2, 2, 0 }, { {{0}}, { { 0, 0, 1, 0 }, { 0, 0, 0, 1 } }, { { 1, 0, 1, 1 }, { 0, 1, 1, 1 } } } },
	{ 1, { { 0, .5 } }, { { 1, .5 } }, { 1, .5 }, { 0, 2, 2, 2, 0 }, { {{0}}, { { 0, .5, 0, 0 }, { 0, .5, 0, 1 } }, { { 0, 0, 1, 0 }, { 0, 1, 1, 1 } }, { { 1, 0, 1, .5 }, { 1, 1, 1, .5 } } } },
	{ 2, { { .5, 0 }, { .5, 1 } }, { { 0, .5 }, { 1, .5 } }, { 0, 0 }, { 0, 4, 4, 0 },
		{ {{0}}, { { .5, 0, 0, 0 }, { .5, 0, 1, 0 }, { .5, 1, 0, 1 }, { .5, 1, 1, 1 } }, { { 0, 0, 0, .5 }, { 0, 1, 0, .5 }, { 1, 0, 1, .5 }, { 1, 1, 1, .5 } } } } };

/* (Part 17b) a place in a menu (or window) as the taskbar sees it: A along the taskbar's side, B away from it (0 at the side
   facing the taskbar, 1 at the far side) */
static void lp_np(const LPRect *R, int dir, double a, double b, double *x, double *y)
{
	if (dir == 1) { *x = R->x + a * R->w; *y = R->y + b * R->h; }
	else if (dir == 2) { *x = R->x + b * R->w; *y = R->y + a * R->h; }
	else if (dir == 3) { *x = R->x + (1 - b) * R->w; *y = R->y + a * R->h; }
	else { *x = R->x + a * R->w; *y = R->y + (1 - b) * R->h; }
}
static LPRect lp_nrect(const LPRect *R, int dir, double a0, double a1, double b0, double b1)
{
	double x0, y0, x1, y1; lp_np(R, dir, a0, b0, &x0, &y0); lp_np(R, dir, a1, b1, &x1, &y1);
	LPRect r = { fmin(x0, x1), fmin(y0, y1), fabs(x1 - x0), fabs(y1 - y0) }; return r;
}
static void lp_nline(LPScene *o, const LPRect *R, int dir, double a0, double b0, double a1, double b1, double al)
{
	double x0, y0, x1, y1; lp_np(R, dir, a0, b0, &x0, &y0); lp_np(R, dir, a1, b1, &x1, &y1); lp_line(o, x0, y0, x1, y1, al);
}
static void lp_nlight(LPScene *o, const LPRect *R, int dir, double a, double b, double k, double r) { double x, y; lp_np(R, dir, a, b, &x, &y); lp_light(o, x, y, k, r); }

static void lp_scene(int kind, double t, const LPGeo *G, LPScene *o)
{
	memset(o, 0, sizeof *o); o->geo_to = 1;
	if (kind <= LP_MAX || kind == LP_CTX) {
		const LPFr *d = &LP_FR[kind == LP_CTX ? LP_C2C : kind]; LPRect F = kind == LP_MAX ? G->s : G->f; int n = LP_NW[kind], i = 0; double p = 0; lp_at(kind, t, &i, &p); int last = i == n - 1;
		int fx = kind == LP_CTX && (G->corner == 1 || G->corner == 2), fy = kind == LP_CTX && (G->corner == 2 || G->corner == 3);   /* (from the corner at the pointer) */
#define PX(a) (F.x + (fx ? 1 - (a) : (a)) * F.w)
#define PY(b) (F.y + (fy ? 1 - (b) : (b)) * F.h)
		if (kind == LP_MAX) { o->geo_to = last; if (last && t < 1) { o->clip = 1; o->cr = lp_r(G->f, G->s, lp_e(p)); } }
		else if (t < 1) { o->clip = 1;
			if (last) { double ox = PX(d->from[0]), oy = PY(d->from[1]), e = lp_e(p), x0 = lp_l(ox, F.x, e), y0 = lp_l(oy, F.y, e), x1 = lp_l(ox, F.x + F.w, e), y1 = lp_l(oy, F.y + F.h, e);
				o->cr.x = fmin(x0, x1); o->cr.y = fmin(y0, y1); o->cr.w = fabs(x1 - x0); o->cr.h = fabs(y1 - y0); }
			else o->cr = lp_z(F); }
		if (t >= 1) return;
		double fade = last ? 1 - lp_c((p - .55) / .45) : 1;
		for (int k = 1; k <= i; k++) { double pp = k < i ? 1 : lp_e(p);
			for (int s = 0; s < d->ns[k]; s++) { const double *g = d->seg[k][s]; double ax = PX(g[0]), ay = PY(g[1]), bx = PX(g[2]), by = PY(g[3]); lp_line(o, ax, ay, lp_l(ax, bx, pp), lp_l(ay, by, pp), fade); } }
		if (i == 0) { for (int j = 0; j < d->no; j++) lp_light(o, PX(d->o[j][0]), PY(d->o[j][1]), lp_e(p) * 1.15, 0); }
		else if (!last) { double e = lp_e(p);
			for (int s = 0; s < d->ns[i]; s++) { const double *g = d->seg[i][s]; lp_light(o, lp_l(PX(g[0]), PX(g[2]), e), lp_l(PY(g[1]), PY(g[3]), e), 1, 0); }
			if (i == n - 2 && p > .85) for (int j = 0; j < d->no; j++) lp_light(o, PX(d->m[j][0]), PY(d->m[j][1]), 1 + (p - .85) * 6, (p - .85) / .15); }
		else { double k = 1 - lp_c(p / .5); if (k > 0) for (int j = 0; j < d->no; j++) lp_light(o, PX(d->m[j][0]), PY(d->m[j][1]), lp_l(.4, 1.9, k), k); }
#undef PX
#undef PY
		return;
	}
#define Q(i) lp_step(kind, t, i)
	if (kind == LP_MIN || kind == LP_SMC) { LPRect f = G->f; int dr = G->dir; double p1 = lp_e(Q(0)), p2 = lp_e(Q(1)), p3 = lp_e(Q(2)), p4 = lp_e(Q(3)), BMx, BMy; lp_np(&f, dr, .5, 0, &BMx, &BMy);
		if (t <= 0) return; o->clip = 1; o->cr = lp_z(f); if (t >= 1) return;
		if (t < .3) o->cr = lp_nrect(&f, dr, .5 * p1, 1 - .5 * p1, 0, 1 - p1);   /* (cleared into the middle of the side toward the taskbar) */
		if (t >= .7) o->lit = lp_c((Q(3) - .75) / .25) * (1 - lp_c((t - .985) / .015));
		if (t < .3) lp_rect(o, f, lp_c(t / .08));
		else if (t < .52) { double b = lp_l(1, 0, p2); lp_nline(o, &f, dr, 0, b, 0, 0, 1); lp_nline(o, &f, dr, 1, b, 1, 0, 1); lp_nline(o, &f, dr, 0, 0, 1, 0, 1); lp_nlight(o, &f, dr, 0, b, 1, 0); lp_nlight(o, &f, dr, 1, b, 1, 0); }
		else if (t < .7) { double a0 = lp_l(0, .5, p3), a1 = lp_l(1, .5, p3); lp_nline(o, &f, dr, a0, 0, a1, 0, 1); lp_nlight(o, &f, dr, a0, 0, 1, 0); lp_nlight(o, &f, dr, a1, 0, 1, 0);
			if (Q(2) > .8) lp_light(o, BMx, BMy, 1 + (Q(2) - .8) * 4, (Q(2) - .8) / .2); }
		else { double px = lp_l(BMx, G->bx, p4), py = lp_l(BMy, G->by, p4); lp_line(o, BMx, BMy, px, py, 1 - p4); lp_light(o, px, py, lp_l(1.4, .8, p4), 1 - lp_c(p4 / .4)); }
		return;
	}
	/* (Part 17b) the start menu and the taskbar's pop-ups opening, and a taskbar button's outline: from the side toward the taskbar
	   (the start menu: a point rising from the start button to its middle first; a pop-up: from above the button clicked), out to
	   its corners, along both sides away from the taskbar, meeting in the middle of the far side; then filled from the taskbar's
	   side (a button is there already: only the outline) */
	if (kind == LP_SMO || kind == LP_POP || kind == LP_TBB) { LPRect R = G->f; int dr = G->dir, k0 = kind == LP_SMO, tb = kind == LP_TBB;
		double st[6] = { 0 }; for (int j = 0; j < LP_NW[kind]; j++) st[j + 1] = st[j] + LP_W[kind][j];
		double aA = kind == LP_POP ? lp_c(dr < 2 ? (R.w > 0 ? (G->bx - R.x) / R.w : .5) : (R.h > 0 ? (G->by - R.y) / R.h : .5)) : .5, Ax, Ay; lp_np(&R, dr, aA, 0, &Ax, &Ay);
		double kk = tb ? .5 : 1;
		if (!tb) { o->clip = 1; o->cr = lp_z(R); }
		if (t <= 0) return; if (t >= 1) { o->clip = 0; return; }
		if (k0 && t < st[1]) { double p = lp_e(Q(0)), px = lp_l(G->bx, Ax, p), py = lp_l(G->by, Ay, p); lp_line(o, G->bx, G->by, px, py, 1 - p * .4); lp_light(o, px, py, lp_l(1.4, 1, p), 0); return; }
		double s1 = lp_e(Q(k0)), s2 = lp_e(Q(k0 + 1)), s3 = lp_e(Q(k0 + 2)), sf = tb ? 0 : Q(k0 + 3), fade = tb ? 1 - lp_c((Q(2) - .7) / .3) : 1 - lp_c((sf - .55) / .45);
		if (!tb && t >= st[k0 + 3]) o->cr = lp_nrect(&R, dr, 0, 1, 0, lp_e(sf));
		lp_nline(o, &R, dr, aA, 0, lp_l(aA, 0, s1), 0, fade); lp_nline(o, &R, dr, aA, 0, lp_l(aA, 1, s1), 0, fade);
		if (t >= st[k0 + 1]) { lp_nline(o, &R, dr, 0, 0, 0, s2, fade); lp_nline(o, &R, dr, 1, 0, 1, s2, fade); }
		if (t >= st[k0 + 2]) { lp_nline(o, &R, dr, 0, 1, lp_l(0, .5, s3), 1, fade); lp_nline(o, &R, dr, 1, 1, lp_l(1, .5, s3), 1, fade); }
		if (t < st[k0 + 1]) { lp_nlight(o, &R, dr, lp_l(aA, 0, s1), 0, kk, 0); lp_nlight(o, &R, dr, lp_l(aA, 1, s1), 0, kk, 0); }
		else if (t < st[k0 + 2]) { lp_nlight(o, &R, dr, 0, s2, kk, 0); lp_nlight(o, &R, dr, 1, s2, kk, 0); }
		else if (tb) { double m = Q(2); if (m < .6) { lp_nlight(o, &R, dr, lp_l(0, .5, s3), 1, kk, 0); lp_nlight(o, &R, dr, lp_l(1, .5, s3), 1, kk, 0); }
			else { double q = (m - .6) / .4; lp_nlight(o, &R, dr, .5, 1, kk * lp_l(1.4, .3, q), 1 - q * .3); } }
		else if (t < st[k0 + 3]) { lp_nlight(o, &R, dr, lp_l(0, .5, s3), 1, 1, 0); lp_nlight(o, &R, dr, lp_l(1, .5, s3), 1, 1, 0);
			if (Q(k0 + 2) > .8) lp_nlight(o, &R, dr, .5, 1, 1 + (Q(k0 + 2) - .8) * 4, (Q(k0 + 2) - .8) / .2); }
		else { double q = 1 - lp_c(sf / .5); if (q > 0) lp_nlight(o, &R, dr, .5, 1, lp_l(.4, 1.6, q), q); }
		return;
	}
	if (kind == LP_RES) { LPRect S = G->s, N = G->f; double p1 = lp_e(Q(0)), p2 = lp_e(Q(1)), p3 = lp_e(Q(2)); o->geo_to = t >= 1;
		if (t <= 0 || t >= 1) return;
		if (t >= .66) { o->clip = 1; o->cr = lp_r(S, N, p3); }
		double Sc[4][2] = { { S.x, S.y }, { S.x + S.w, S.y }, { S.x + S.w, S.y + S.h }, { S.x, S.y + S.h } }, Nc[4][2] = { { N.x, N.y }, { N.x + N.w, N.y }, { N.x + N.w, N.y + N.h }, { N.x, N.y + N.h } };
		double MLx = S.x, MRx = S.x + S.w, My = S.y + S.h / 2;
		if (t < .3) { if (Q(0) < .2) { lp_light(o, MLx, My, Q(0) * 5, 0); lp_light(o, MRx, My, Q(0) * 5, 0); }
			double P[4][2] = { { MLx, lp_l(My, Sc[0][1], p1) }, { MLx, lp_l(My, Sc[3][1], p1) }, { MRx, lp_l(My, Sc[1][1], p1) }, { MRx, lp_l(My, Sc[2][1], p1) } };
			lp_line(o, MLx, My, P[0][0], P[0][1], 1); lp_line(o, MLx, My, P[1][0], P[1][1], 1); lp_line(o, MRx, My, P[2][0], P[2][1], 1); lp_line(o, MRx, My, P[3][0], P[3][1], 1);
			for (int j = 0; j < 4; j++) lp_light(o, P[j][0], P[j][1], 1, 0);
			if (Q(0) > .9) { lp_line(o, Sc[0][0], Sc[0][1], Sc[1][0], Sc[1][1], (Q(0) - .9) * 10); lp_line(o, Sc[3][0], Sc[3][1], Sc[2][0], Sc[2][1], (Q(0) - .9) * 10); }
			return; }
		double g = t < .66 ? p2 : 1, Qp[4][2], fade = t < .66 ? 1 : 1 - lp_c((Q(2) - .5) / .5);
		for (int j = 0; j < 4; j++) { Qp[j][0] = lp_l(Sc[j][0], Nc[j][0], g); Qp[j][1] = lp_l(Sc[j][1], Nc[j][1], g); }
		for (int j = 0; j < 4; j++) lp_line(o, Qp[j][0], Qp[j][1], Qp[(j + 1) % 4][0], Qp[(j + 1) % 4][1], fade);
		if (t < .66) { for (int j = 0; j < 4; j++) lp_light(o, Qp[j][0], Qp[j][1], 1, 0); if (Q(1) > .85) for (int j = 0; j < 4; j++) lp_light(o, Qp[j][0], Qp[j][1], 1 + (Q(1) - .85) * 4, (Q(1) - .85) / .15); }
		else { double k = 1 - lp_c(Q(2) / .5); if (k > 0) for (int j = 0; j < 4; j++) lp_light(o, Nc[j][0], Nc[j][1], lp_l(.4, 1.5, k), k); }
		return;
	}
	if (kind == LP_CLS) { LPRect f = G->f; double my = f.y + f.h / 2, MLx = f.x, MRx = f.x + f.w, Cx = f.x + f.w / 2, p1 = lp_e(Q(0)), p2 = lp_e(Q(1)), p3 = lp_e(Q(2));
		if (t <= 0) return; o->clip = 1; o->cr = lp_z(f); if (t >= 1) return;
		if (t < .32) { o->cr.x = f.x; o->cr.y = lp_l(f.y, my, p1); o->cr.w = f.w; o->cr.h = lp_l(f.h, 0, p1); lp_rect(o, f, lp_c(t / .08));
			if (Q(0) > .8) { double k = (Q(0) - .8) * 5; lp_light(o, f.x, f.y, k, 0); lp_light(o, f.x + f.w, f.y, k, 0); lp_light(o, f.x, f.y + f.h, k, 0); lp_light(o, f.x + f.w, f.y + f.h, k, 0); }
			return; }
		if (t < .62) { double top = lp_l(f.y, my, p2), bot = lp_l(f.y + f.h, my, p2); LPRect R = { f.x, top, f.w, bot - top }; lp_rect(o, R, 1);
			lp_light(o, f.x, top, 1, 0); lp_light(o, f.x + f.w, top, 1, 0); lp_light(o, f.x, bot, 1, 0); lp_light(o, f.x + f.w, bot, 1, 0); return; }
		double m = fmin(1, p3 / .7), ax = lp_l(MLx, Cx, m), bx = lp_l(MRx, Cx, m);
		if (p3 < .7) { lp_line(o, ax, my, bx, my, 1); lp_light(o, ax, my, 1, 0); lp_light(o, bx, my, 1, 0); }
		else { double k = 1 - lp_c((Q(2) - .75) / .25); lp_light(o, Cx, my, lp_l(.3, 1.9, k), fmin(1, (p3 - .7) / .1)); }
	}
#undef Q
}
#endif
