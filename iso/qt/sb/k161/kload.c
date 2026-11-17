/*
 * kload - the loads and the measurements of the 1.6.1 kernel tuning (docs/plan-1.6.1.md Part 12), run inside the QEMU
 * guest by k161_t.py. Tests only: never part of a release. Built static on the test machine. GPL-2.0-or-later.
 *
 *   kload wlat SECONDS [PERIOD_US]   wake-up latency of an ordinary (SCHED_OTHER, nice 0) thread, as the desktop's own
 *                                    threads are: it sleeps PERIOD_US (1000) and notes how late it woke; prints
 *                                    "wlat n=N p50=.. p99=.. p999=.. max=.. us"
 *   kload burn                       a busy loop (until killed); one per processor makes the "CPU load"
 *   kload call SECONDS               a call's audio stand-in: every 20 ms a little work (64 KiB touched), as an audio
 *                                    callback; prints how late each period was ("call n=.. late10=.. p99=.. max=.. ms")
 *   kload game MB SECONDS            a game's stand-in: MB of memory (assets, half compressible), a frame loop at 60
 *                                    fps that touches a quarter of it each second; prints the frame times
 *                                    ("game frames=.. slow33=.. slow100=.. p99=.. max=.. ms")
 *   kload tabs MB                    browser tabs' stand-in: MB of memory that compresses about 3:1 (as web pages' heaps),
 *                                    touched lightly (2 % a second) until killed
 */
#define _GNU_SOURCE
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

static double now_us(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e6 + t.tv_nsec / 1e3; }
static int cmpd(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
static double pct(double *v, size_t n, double p) { if (!n) return 0; size_t i = (size_t)(p * (n - 1) + 0.5); return v[i]; }
static uint64_t rs = 88172645463325252ull;
static uint64_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return rs; }
/* a page's worth of data: `rand_part` of its 4096 bytes random, the rest a repeated pattern */
static void fill(unsigned char *p, size_t len, unsigned rand_part)
{
	for (size_t o = 0; o < len; o += 4096) {
		size_t n = len - o < 4096 ? len - o : 4096;
		for (size_t i = 0; i < n; i += 8) {
			uint64_t v = i < rand_part ? rnd() : 0x0101010101010101ull * ((o >> 12) & 0xff);
			memcpy(p + o + i, &v, n - i < 8 ? n - i : 8);
		}
	}
}
static unsigned char *big(size_t mb)
{
	unsigned char *p = mmap(NULL, mb << 20, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED) { perror("mmap"); exit(1); }
	return p;
}

static int wlat(double secs, long period)
{
	size_t cap = (size_t)(secs * 1e6 / period) + 16, n = 0; double *v = malloc(cap * sizeof *v), end = now_us() + secs * 1e6;
	struct timespec d = {0, period * 1000};
	while (n < cap && now_us() < end) {
		double t0 = now_us(); nanosleep(&d, NULL); double late = now_us() - t0 - period;
		v[n++] = late < 0 ? 0 : late;
	}
	qsort(v, n, sizeof *v, cmpd);
	printf("wlat n=%zu p50=%.0f p99=%.0f p999=%.0f max=%.0f us\n", n, pct(v, n, .5), pct(v, n, .99), pct(v, n, .999), n ? v[n - 1] : 0);
	return 0;
}
static int call(double secs)
{
	size_t cap = (size_t)(secs * 50) + 16, n = 0, late10 = 0; double *v = malloc(cap * sizeof *v);
	unsigned char *buf = malloc(64 << 10); double t = now_us(), end = t + secs * 1e6;
	while (n < cap && t < end) {
		t += 20000; double w = t - now_us();
		if (w > 0) { struct timespec d = {(time_t)(w / 1e6), (long)((w - (time_t)(w / 1e6) * 1e6) * 1000)}; nanosleep(&d, NULL); }
		double late = (now_us() - t) / 1000; if (late < 0) late = 0;
		memset(buf, (int)n, 64 << 10);   /* the work of one period */
		v[n++] = late; if (late > 10) late10++;
		if (late > 20) t = now_us();   /* (a missed period is not made up, as an audio buffer is not) */
	}
	qsort(v, n, sizeof *v, cmpd);
	printf("call n=%zu late10=%zu p99=%.1f max=%.1f ms\n", n, late10, pct(v, n, .99), n ? v[n - 1] : 0);
	return 0;
}
static int game(size_t mb, double secs)
{
	unsigned char *p = big(mb); fill(p, mb << 20, 2048);
	size_t len = mb << 20, per = len / 4 / 60, off = 0, cap = (size_t)(secs * 60) + 16, n = 0, s33 = 0, s100 = 0;
	double *v = malloc(cap * sizeof *v), t = now_us(), end = t + secs * 1e6; unsigned long sum = 0;
	while (n < cap && now_us() < end) {
		double f0 = now_us();
		for (size_t i = 0; i < per; i += 4096) { sum += p[(off + i) % len]; p[(off + i) % len] = (unsigned char)sum; }
		off = (off + per) % len;
		t += 16667; double w = t - now_us();
		if (w > 0) { struct timespec d = {0, (long)(w * 1000)}; nanosleep(&d, NULL); } else t = now_us();
		double ft = (now_us() - f0) / 1000; v[n++] = ft; if (ft > 33.4) s33++; if (ft > 100) s100++;
	}
	qsort(v, n, sizeof *v, cmpd);
	printf("game frames=%zu slow33=%zu slow100=%zu p99=%.1f max=%.1f ms (%lu)\n", n, s33, s100, pct(v, n, .99), n ? v[n - 1] : 0, sum & 1);
	return 0;
}
static void tabs(size_t mb)
{
	unsigned char *p = big(mb); fill(p, mb << 20, 1024); size_t len = mb << 20, off = 0, per = len / 50;
	printf("tabs %zu MB\n", mb); fflush(stdout);
	for (;;) { for (size_t i = 0; i < per; i += 4096) p[(off + i) % len]++; off = (off + per) % len; sleep(1); }
}
int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IOLBF, 0);
	if (argc >= 3 && !strcmp(argv[1], "wlat")) return wlat(atof(argv[2]), argc > 3 ? atol(argv[3]) : 1000);
	if (argc >= 2 && !strcmp(argv[1], "burn")) { volatile unsigned long x = 0; for (;;) x++; }
	if (argc >= 3 && !strcmp(argv[1], "call")) return call(atof(argv[2]));
	if (argc >= 4 && !strcmp(argv[1], "game")) return game(strtoul(argv[2], NULL, 10), atof(argv[3]));
	if (argc >= 3 && !strcmp(argv[1], "tabs")) tabs(strtoul(argv[2], NULL, 10));
	fprintf(stderr, "usage: kload wlat S [US] | burn | call S | game MB S | tabs MB\n"); return 2;
}
