/* rsynth.h - the Rudolf sounds' renderer (1.6.0; the owner's start-up chime and desktop sounds, 2026-11-19).
 * GPL-2.0-or-later. The same arithmetic as desktop/sndrs60.js rsRender, step for step, so the native desktop plays the
 * samples the page plays: events from the page (rsEvents), rendered here at 44100 Hz, mono. Kept in its own header so
 * the host test (iso/qt/host/rsynth60_t.sh) can compare the two renderers sample by sample.
 *   kinds: 'B' bell (t,f,dur,amp); 'P' pad (t,dur,amp,f1..f6); 'K' tick (t,f,amp); 'S' sweep (t,f0,f1,dur,amp);
 *          'N' noise (t,f,q,dur,amp,type); 'T' tone (t,f,dur,amp,wave,fEnd)
 *   G: bright, warmth, grain, spark, space (0..100) */
#ifndef RUDOLF_RSYNTH_H
#define RUDOLF_RSYNTH_H
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define RS_SR 44100
#define RS_MAXV 10
typedef struct { char k; int nv; double v[RS_MAXV]; } RsEv;   /* (v: the event's numbers after its kind, t first) */
typedef struct { double b0, b1, b2, a1, a2, x1, x2, y1, y2; } RsBq;

static double rs_env(double x, double t, double a, double pk, double d)
{
	if (x < t || pk <= 1e-4) return 0;
	if (x < t + a) return .0001 + (pk - .0001) * (x - t) / a;
	if (x < t + a + d) return pk * pow(.0001 / pk, (x - t - a) / d);
	return 0;
}
static void rs_coef(RsBq *q, int ty, double f, double Q)
{
	if (f > RS_SR * .45) f = RS_SR * .45;
	double w = 2 * M_PI * f / RS_SR, c = cos(w), al = sin(w) / (2 * Q), a0 = 1 + al;
	if (ty == 0) { q->b0 = (1 - c) / 2 / a0; q->b1 = (1 - c) / a0; q->b2 = (1 - c) / 2 / a0; }
	else if (ty == 1) { q->b0 = al / a0; q->b1 = 0; q->b2 = -al / a0; }
	else { q->b0 = (1 + c) / 2 / a0; q->b1 = -(1 + c) / a0; q->b2 = (1 + c) / 2 / a0; }
	q->a1 = -2 * c / a0; q->a2 = (1 - al) / a0;
}
static double rs_run(RsBq *q, double x) { double y = q->b0 * x + q->b1 * q->x1 + q->b2 * q->x2 - q->a1 * q->y1 - q->a2 * q->y2; q->x2 = q->x1; q->x1 = x; q->y2 = q->y1; q->y1 = y; return y; }
static double rs_wave(int w, double ph) { return w == 1 ? (ph < .5 ? 1 : -1) : w == 2 ? 1 - 4 * fabs(ph - .5) : w == 3 ? 2 * ph - 1 : sin(2 * M_PI * ph); }
static double rs_fl(double x) { return x - floor(x); }
/* how long the events last (s), with the room's tail (rsLength) */
static double rs_length(const RsEv *E, int n, const double G[5])
{
	double L = 0;
	for (int i = 0; i < n; i++) { const double *v = E[i].v; double d = 0;
		switch (E[i].k) { case 'B': d = v[2] + .1; break; case 'P': d = v[1]; break; case 'K': d = .06; break; case 'S': d = v[3] + .2; break; case 'N': d = v[3] + .05; break; case 'T': d = v[2] + .05; break; }
		if (v[0] + d > L) L = v[0] + d; }
	L += .15 + G[4] / 100 * 1.2; return L > 8 ? 8 : L;
}
/* the samples (as floats, -1..1, peak at most .95); *np their number; NULL when out of memory */
static float *rs_render(const RsEv *E, int n, const double G[5], size_t *np)
{
	const double sr = RS_SR, bright = G[0] / 100, warmth = G[1] / 100, grain = G[2] / 100, spark = G[3] / 100, space = G[4] / 100;
	size_t N = (size_t)ceil(rs_length(E, n, G) * sr); double *out = calloc(N ? N : 1, sizeof *out); if (!out) return NULL;
	long long seed = 12345;
#define RND() (seed = (seed * 16807) % 2147483647, (double)seed / 2147483647 * 2 - 1)
	for (int i = 0; i < n; i++) {
		const double *v = E[i].v, t = v[0]; size_t s0 = t > 0 ? (size_t)floor(t * sr) : 0;
		if (E[i].k == 'B') { double f = v[1], d = v[2], am = v[3], i0 = f * (.9 + bright * 2.6), i1 = f * .05, pc = 0, pm = 0, ps = 0; size_t end = (size_t)ceil((t + d + .1) * sr); if (end > N) end = N;
			for (size_t s = s0; s < end; s++) { double x = (double)s / sr, u = x - t, idx = u < d * .7 ? i0 * pow(i1 / i0, u / (d * .7)) : i1;
				pm += f * 3.5 / sr; pm = rs_fl(pm); double fi = f + sin(2 * M_PI * pm) * idx; pc += fi / sr; pc = rs_fl(pc); ps += f * 2.756 / sr; ps = rs_fl(ps);
				out[s] += sin(2 * M_PI * pc) * rs_env(x, t, .004, am, d) + sin(2 * M_PI * ps) * rs_env(x, t, .002, am * .18 * (.4 + bright), d * .35); } }
		else if (E[i].k == 'P') { double d = v[1], am = v[2]; int nf = E[i].nv - 3; if (warmth <= 0 || am * warmth <= 1e-4 || nf < 1) continue; if (nf > 6) nf = 6;
			RsBq q = {0}; double pk = am * warmth, att = fmin(.6, d * .3), ph[12] = {0}; size_t end = (size_t)ceil((t + d) * sr); if (end > N) end = N;
			for (size_t s = s0; s < end; s++) { double x = (double)s / sr, u = x - t;
				if ((s - s0) % 32 == 0) rs_coef(&q, 0, (380 + warmth * 900) + ((260 + warmth * 500) - (380 + warmth * 900)) * fmin(1, u / d), .4);
				double val = 0; for (int j = 0; j < nf; j++) for (int h = 0; h < 2; h++) { double fr = v[3 + j] * pow(2, (h ? 6 : -7) / 1200.0); int k = j * 2 + h; ph[k] += fr / sr; ph[k] = rs_fl(ph[k]); val += (2 * ph[k] - 1) * .22 / nf; }
				double gn = u < att ? .0001 + (pk - .0001) * u / att : u < d * .55 ? pk : pk * pow(.0001 / pk, (u - d * .55) / (d * .45)); out[s] += rs_run(&q, val) * gn; } }
		else if (E[i].k == 'K') { if (grain <= 0) continue; double f = v[1], am = v[2], ph = 0; RsBq q = {0}; rs_coef(&q, 1, f * 2, 3); size_t end = (size_t)ceil((t + .06) * sr); if (end > N) end = N;
			for (size_t s = s0; s < end; s++) { ph += f / sr; ph = rs_fl(ph); out[s] += rs_run(&q, ph < .5 ? 1 : -1) * rs_env((double)s / sr, t, .0015, am * grain, .035); } }
		else if (E[i].k == 'S') { double f0 = v[1], f1 = v[2], d = v[3], am = v[4]; if (spark <= 0 || am * spark * .55 <= 1e-4) continue; double pk = am * spark, ph = 0; RsBq q = {0};
			size_t end = (size_t)ceil((t + d + .2) * sr); if (end > N) end = N;
			for (size_t s = s0; s < end; s++) { double x = (double)s / sr, u = x - t, r = fmin(1, u / d), f = f0 * pow(f1 / f0, r); ph += f / sr; ph = rs_fl(ph);
				if ((s - s0) % 32 == 0) rs_coef(&q, 1, f * 2, 6);
				double nz = rs_run(&q, RND());
				double g1 = u < d * .6 ? .0001 + (pk * .55 - .0001) * u / (d * .6) : u < d + .08 ? pk * .55 * pow(.0001 / (pk * .55), (u - d * .6) / (d * .4 + .08)) : 0,
					g2 = u < d * .7 ? .0001 + (pk * .6 - .0001) * u / (d * .7) : u < d + .1 ? pk * .6 * pow(.0001 / (pk * .6), (u - d * .7) / (d * .3 + .1)) : 0;
				out[s] += sin(2 * M_PI * ph) * g1 + nz * g2; } }
		else if (E[i].k == 'N') { double f = v[1], Q = v[2], d = v[3], am = v[4]; int ty = (int)v[5]; RsBq q = {0}; rs_coef(&q, ty ? 2 : 1, f, Q); size_t end = (size_t)ceil((t + d + .05) * sr); if (end > N) end = N;
			for (size_t s = s0; s < end; s++) out[s] += rs_run(&q, RND()) * rs_env((double)s / sr, t, .001, am, d); }
		else if (E[i].k == 'T') { double f = v[1], d = v[2], am = v[3], fe = v[5], ph = 0; int w = (int)v[4]; RsBq q = {0}; rs_coef(&q, 0, 900 + bright * 100 * 40, .7071); size_t end = (size_t)ceil((t + d + .05) * sr); if (end > N) end = N;
			for (size_t s = s0; s < end; s++) { double x = (double)s / sr, fr = fe ? f * pow(fe / f, fmin(1, (x - t) / d)) : f; ph += fr / sr; ph = rs_fl(ph); out[s] += rs_run(&q, rs_wave(w, ph)) * rs_env(x, t, .008, am, d); } }
	}
#undef RND
	/* the room: four combs and two all-passes, mixed in by Space */
	if (space > 0) { static const int CL[4] = { 1116, 1188, 1277, 1356 }, AL[2] = { 556, 441 }; double fb = .7 + space * .25, wet = space * .55, *cb[4], lp[4] = {0}, *ab[2]; int ci[4] = {0}, ai[2] = {0};
		for (int c = 0; c < 4; c++) cb[c] = calloc(CL[c], sizeof(double));
		for (int a = 0; a < 2; a++) ab[a] = calloc(AL[a], sizeof(double));
		int ok = 1; for (int c = 0; c < 4; c++) if (!cb[c]) ok = 0; for (int a = 0; a < 2; a++) if (!ab[a]) ok = 0;
		for (size_t s = 0; ok && s < N; s++) { double x = out[s], y = 0;
			for (int c = 0; c < 4; c++) { double o = cb[c][ci[c]]; lp[c] = o * .8 + lp[c] * .2; cb[c][ci[c]] = x + lp[c] * fb; ci[c] = (ci[c] + 1) % CL[c]; y += o; }
			y *= .25; for (int a = 0; a < 2; a++) { double o = ab[a][ai[a]], vv = -y * .5 + o; ab[a][ai[a]] = y + o * .5; ai[a] = (ai[a] + 1) % AL[a]; y = vv; }
			out[s] = x + y * wet; }
		for (int c = 0; c < 4; c++) free(cb[c]);
		for (int a = 0; a < 2; a++) free(ab[a]); }
	double pk = 0; for (size_t s = 0; s < N; s++) if (fabs(out[s]) > pk) pk = fabs(out[s]);
	double sc = pk > .95 ? .95 / pk : 1; float *f32 = malloc((N ? N : 1) * sizeof(float)); if (f32) for (size_t s = 0; s < N; s++) f32[s] = (float)(out[s] * sc);
	free(out); *np = N; return f32;
}
#endif
