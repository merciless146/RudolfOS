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

enum { LP_C2C, LP_S2S, LP_MAX, LP_MIN, LP_RES, LP_CLS, LP_KINDS };
static const char *const LP_NAME[LP_KINDS] = { "c2c", "s2s", "max", "min", "res", "cls" };
static const int LP_MS[LP_KINDS] = { 200, 220, 180, 200, 180, 200 };
/* each one's steps, as shares of its time (the demo's, which the owner chose) */
static const double LP_W[LP_KINDS][5] = { { .07, .29, .29, .35 }, { .06, .2, .22, .2, .32 }, { .07, .3, .3, .33 }, { .3, .22, .18, .3 }, { .3, .36, .34 }, { .32, .3, .38 } };
static const int LP_NW[LP_KINDS] = { 4, 5, 4, 4, 3, 3 };

typedef struct { double x, y, w, h; } LPRect;
typedef struct { LPRect f, s; double bx, by; } LPGeo;   /* the window (as it was for max, as it will be for res), maximized, the taskbar button */
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

static void lp_scene(int kind, double t, const LPGeo *G, LPScene *o)
{
	memset(o, 0, sizeof *o); o->geo_to = 1;
	if (kind <= LP_MAX) {
		const LPFr *d = &LP_FR[kind]; LPRect F = kind == LP_MAX ? G->s : G->f; int n = LP_NW[kind], i = 0; double p = 0; lp_at(kind, t, &i, &p); int last = i == n - 1;
#define PX(a) (F.x + (a) * F.w)
#define PY(b) (F.y + (b) * F.h)
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
	if (kind == LP_MIN) { LPRect f = G->f; double BMx = f.x + f.w / 2, BMy = f.y + f.h, p1 = lp_e(Q(0)), p2 = lp_e(Q(1)), p3 = lp_e(Q(2)), p4 = lp_e(Q(3));
		if (t <= 0) return; o->clip = 1; o->cr = lp_z(f); if (t >= 1) return;
		if (t < .3) { o->cr.x = lp_l(f.x, BMx, p1); o->cr.y = lp_l(f.y, BMy, p1); o->cr.w = lp_l(f.w, 0, p1); o->cr.h = lp_l(f.h, 0, p1); }
		if (t >= .7) o->lit = lp_c((Q(3) - .75) / .25) * (1 - lp_c((t - .985) / .015));
		double BLx = f.x, BRx = f.x + f.w, Ty = f.y;
		if (t < .3) lp_rect(o, f, lp_c(t / .08));
		else if (t < .52) { double ay = lp_l(Ty, BMy, p2); lp_line(o, BLx, ay, BLx, BMy, 1); lp_line(o, BRx, ay, BRx, BMy, 1); lp_line(o, BLx, BMy, BRx, BMy, 1); lp_light(o, BLx, ay, 1, 0); lp_light(o, BRx, ay, 1, 0); }
		else if (t < .7) { double ax = lp_l(BLx, BMx, p3), bx = lp_l(BRx, BMx, p3); lp_line(o, ax, BMy, bx, BMy, 1); lp_light(o, ax, BMy, 1, 0); lp_light(o, bx, BMy, 1, 0);
			if (Q(2) > .8) lp_light(o, BMx, BMy, 1 + (Q(2) - .8) * 4, (Q(2) - .8) / .2); }
		else { double px = lp_l(BMx, G->bx, p4), py = lp_l(BMy, G->by, p4); lp_line(o, BMx, BMy, px, py, 1 - p4); lp_light(o, px, py, lp_l(1.4, .8, p4), 1 - lp_c(p4 / .4)); }
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
