/* rudolf-scan: scanning for Rudolf OS, without a scanning daemon (docs/plan-1.6.2.md Part 34).
 *
 * Finds scanners and scans with SANE (libsane.so.1, opened with dlopen) and its backends: sane-airscan for driverless
 * scanners (eSCL, also called AirScan, and WSD: most scanners and printer-scanners since about 2015) and, from Part 34
 * step 3, a few backends for older USB scanners. Discovery is this program's own, not sane-airscan's (which needs Avahi,
 * which Rudolf OS does not have, and listens for WS-Discovery's announcements): each scan gets a SANE configuration of
 * its own (SANE_CONFIG_DIR) naming just the scanner asked for, with sane-airscan's discovery off.
 *
 *   rudolf-scan find                    the scanners (JSON): eSCL found by multicast DNS (_uscan._tcp, _uscans._tcp:
 *                                       asked as a full querier from port 5353, as rudolf-print find asks, so that the
 *                                       answers come by multicast, which the firewall's mdns chain lets in); WSD found
 *                                       by WS-Discovery (a Probe from port 3702, whose ProbeMatches the firewall lets in
 *                                       for ten seconds after "firewall wsd open"; only those that answer this Probe's own
 *                                       random MessageID, from the address their scan service is at); USB scanners that
 *                                       speak eSCL through ipp-usb (the IPP over USB printers of /run/rudolf/usb-printers
 *                                       whose ipp-usb port answers /eSCL/ScannerCapabilities)
 *   rudolf-scan caps ID                 what it can do (JSON): sources, colour modes, resolutions, the largest area
 *   rudolf-scan scan ID DIR [source=flatbed|adf] [mode=color|grey|bw] [res=DPI] [area=L,T,W,H] [first=N] [preview=1]
 *                                       scans into DIR as page-N.png (N from first=, 1 by default): one page from the
 *                                       glass, every page in the feeder (source=adf); preview=1: one quick page,
 *                                       preview.png (the lowest resolution from 75 dpi, in colour). The area is in
 *                                       millimetres from the top-left corner. Black and white is a grey scan made
 *                                       black or white here where the scanner has no line-art mode.
 *   rudolf-scan save pdf OUT PAGE...    the pages into one PDF (colour and grey pages as JPEG, black and white ones as
 *                                       1-bit pages), at their resolutions' paper size
 *   rudolf-scan save png|jpeg OUT PAGE  one page as PNG or JPEG
 *                                       (OUT is never overwritten: a file there already is an error, "exists")
 * ID: escl:URL (http://ADDR:PORT/PATH or https://...) or wsd:URL (the scan service's address) for sane-airscan, or
 * sane:DEVICE for a USB scanner of the backends in /usr/share/rudolf/scan-backends (Part 34 step 3).
 * Every answer is one line of JSON: {"ok":true,...}, or {"ok":false,"code":CODE,"error":TEXT} with CODE one of
 * noscanner, busy, jam, open, nodocs, io, denied, cancelled, nosane, args, exists, write, memory, unsupported.
 * Pages are kept as PNG (lossless) with their resolution (pHYs); this program reads only the PNG it writes.
 * For the tests: RS_MDNS=ADDR:PORT (the mDNS question there instead of the group), RS_WSD=ADDR:PORT (the Probe there),
 * RS_USB (the USB printers' list), RS_CELLULAR (the cellular interfaces' list), RS_SANE (libsane's path),
 * RS_BACKENDS (the backends' list), RS_WAIT (milliseconds each discovery listens, 1500 by default).
 * The JPEG encoder is this program's own (baseline, the tables of ITU-T T.81 Annex K); PNG through zlib.
 *
 * Copyright (C) 2026 the Rudolf OS authors. SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <math.h>
#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>

#define MAX_PIXELS (120u * 1000 * 1000)   /* (600 dpi, 8.5 x 14 inches is 43 million) */

/* ---------------------------------------------------------------- output */
static void js(const char *s)
{
	putchar('"');
	for (; s && *s; s++) {
		unsigned char c = *s;
		if (c == '"' || c == '\\') printf("\\%c", c);
		else if (c < 0x20 || c == 0x7f) printf("\\u%04x", c);
		else putchar(c);
	}
	putchar('"');
}
static int fail(const char *code, const char *fmt, ...)
{
	char m[600]; va_list ap; va_start(ap, fmt); vsnprintf(m, sizeof m, fmt, ap); va_end(ap);
	printf("{\"ok\":false,\"code\":"); js(code); printf(",\"error\":"); js(m); printf("}\n"); fflush(stdout); return 1;
}
static long peak_kb(void) { struct rusage u; getrusage(RUSAGE_SELF, &u); return u.ru_maxrss; }
typedef struct { unsigned char *b; size_t n, cap; } buf;
static int put(buf *x, const void *p, size_t n)
{
	if (!n) return 0;
	if (x->n + n > x->cap) { size_t c = x->cap ? x->cap : 4096; while (c < x->n + n) c *= 2; unsigned char *nb = realloc(x->b, c); if (!nb) return -1; x->b = nb; x->cap = c; }
	memcpy(x->b + x->n, p, n); x->n += n; return 0;
}
static void put8(buf *x, int v) { unsigned char c = v; put(x, &c, 1); }
static void put16(buf *x, int v) { unsigned char c[2] = {v >> 8, v}; put(x, c, 2); }
static void putstr(buf *x, const char *s) { put(x, s, strlen(s)); }
static void putf(buf *x, const char *fmt, ...)
{
	char t[512]; va_list ap; va_start(ap, fmt); int n = vsnprintf(t, sizeof t, fmt, ap); va_end(ap);
	if (n > 0) put(x, t, (size_t)n < sizeof t ? (size_t)n : sizeof t - 1);
}
static int now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (int)(t.tv_sec * 1000 + t.tv_nsec / 1000000); }

/* ---------------------------------------------------------------- an image: 8 bits a channel, 1 (grey) or 3 (RGB) */
typedef struct { int w, h, ch, dpi, bw; unsigned char *px; } img;
static int img_alloc(img *m, int w, int h, int ch)
{
	if (w <= 0 || h <= 0 || (uint64_t)w * h > MAX_PIXELS) return -1;
	m->w = w; m->h = h; m->ch = ch; m->px = malloc((size_t)w * h * ch); return m->px ? 0 : -1;
}

/* ---------------------------------------------------------------- PNG (written with zlib; read: only what this program writes, and any PNG of those kinds) */
static void png_chunk(buf *o, const char *t, const unsigned char *d, size_t n)
{
	unsigned char l[4] = {n >> 24, n >> 16, n >> 8, n}; put(o, l, 4); put(o, t, 4); if (n) put(o, d, n);
	uLong c = crc32(0, (const Bytef *)t, 4); if (n) c = crc32(c, d, n); unsigned char cc[4] = {c >> 24, c >> 16, c >> 8, c}; put(o, cc, 4);
}
static int png_encode(const img *m, buf *o)
{
	static const unsigned char SIG[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
	put(o, SIG, 8);
	int bits = m->bw ? 1 : 8, ct = m->ch == 3 ? 2 : 0;
	unsigned char h[13] = {m->w >> 24, m->w >> 16, m->w >> 8, m->w, m->h >> 24, m->h >> 16, m->h >> 8, m->h, bits, ct, 0, 0, 0};
	png_chunk(o, "IHDR", h, 13);
	if (m->dpi > 0) { uint32_t ppm = (uint32_t)(m->dpi / 0.0254 + 0.5); unsigned char p[9] = {ppm >> 24, ppm >> 16, ppm >> 8, ppm, ppm >> 24, ppm >> 16, ppm >> 8, ppm, 1}; png_chunk(o, "pHYs", p, 9); }
	size_t row = m->bw ? (size_t)(m->w + 7) / 8 : (size_t)m->w * m->ch;
	unsigned char *cur = calloc(1, row + 1), *prev = calloc(1, row), *line = malloc(row), *ob = malloc(1 << 17);
	if (!cur || !prev || !line || !ob) { free(cur); free(prev); free(line); free(ob); return -1; }
	z_stream z = {0}; deflateInit(&z, 6); buf idat = {0}; int r = 0;
	for (int y = 0; y < m->h && !r; y++) {
		const unsigned char *s = m->px + (size_t)y * m->w * m->ch;
		if (m->bw) { memset(line, 0, row); for (int x = 0; x < m->w; x++) if (s[x] >= 128) line[x >> 3] |= 0x80 >> (x & 7); }
		else memcpy(line, s, row);
		cur[0] = 2; for (size_t i = 0; i < row; i++) cur[i + 1] = line[i] - prev[i];   /* (filter Up: good for scans) */
		memcpy(prev, line, row);
		z.next_in = cur; z.avail_in = row + 1;
		do { z.next_out = ob; z.avail_out = 1 << 17; if (deflate(&z, y == m->h - 1 ? Z_FINISH : Z_NO_FLUSH) == Z_STREAM_ERROR) r = -1; if (put(&idat, ob, (1 << 17) - z.avail_out)) r = -1; } while (!r && (z.avail_out == 0 || (y == m->h - 1 && z.avail_in)));
	}
	deflateEnd(&z); free(cur); free(prev); free(line); free(ob);
	if (!r) { png_chunk(o, "IDAT", idat.b, idat.n); png_chunk(o, "IEND", NULL, 0); }
	free(idat.b); return r;
}
static uint32_t be32(const unsigned char *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
static int png_decode(const unsigned char *d, size_t n, img *m)
{
	memset(m, 0, sizeof *m);
	if (n < 8 || memcmp(d, "\x89PNG\r\n\x1a\n", 8)) return -1;
	size_t i = 8; int w = 0, h = 0, bits = 0, ct = -1, il = 0; buf idat = {0};
	while (i + 12 <= n) {
		uint32_t l = be32(d + i); const unsigned char *t = d + i + 4, *c = d + i + 8;
		if (l > n - i - 12) break;
		if (!memcmp(t, "IHDR", 4) && l >= 13) { w = be32(c); h = be32(c + 4); bits = c[8]; ct = c[9]; il = c[12]; }
		else if (!memcmp(t, "pHYs", 4) && l >= 9 && c[8] == 1) m->dpi = (int)(be32(c) * 0.0254 + 0.5);
		else if (!memcmp(t, "IDAT", 4)) { if (put(&idat, c, l)) { free(idat.b); return -1; } }
		else if (!memcmp(t, "IEND", 4)) break;
		i += 12 + l;
	}
	if (w <= 0 || h <= 0 || il || !((ct == 0 && (bits == 8 || bits == 1)) || (ct == 2 && bits == 8)) || img_alloc(m, w, h, ct == 2 ? 3 : 1)) { free(idat.b); free(m->px); m->px = NULL; return -1; }
	m->bw = bits == 1;
	size_t row = bits == 1 ? (size_t)(w + 7) / 8 : (size_t)w * m->ch; int bpp = bits == 1 ? 1 : m->ch;
	unsigned char *raw = malloc(row + 1), *prev = calloc(1, row), *cur = malloc(row);
	z_stream z = {0}; inflateInit(&z); z.next_in = idat.b; z.avail_in = idat.n; int r = raw && prev && cur ? 0 : -1;
	for (int y = 0; y < h && !r; y++) {
		z.next_out = raw; z.avail_out = row + 1;
		while (z.avail_out && !r) { int e = inflate(&z, Z_SYNC_FLUSH); if (e != Z_OK && e != Z_STREAM_END) r = -1; else if (e == Z_STREAM_END && z.avail_out) r = -1; }
		if (r) break;
		int f = raw[0]; unsigned char *s = raw + 1;
		for (size_t x = 0; x < row; x++) {
			int a = x >= (size_t)bpp ? cur[x - bpp] : 0, b = prev[x], c = x >= (size_t)bpp ? prev[x - bpp] : 0, v = s[x];
			switch (f) { case 0: break; case 1: v += a; break; case 2: v += b; break; case 3: v += (a + b) / 2; break;
				case 4: { int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c); v += pa <= pb && pa <= pc ? a : pb <= pc ? b : c; } break;
				default: r = -1; }
			cur[x] = v;
		}
		unsigned char *o = m->px + (size_t)y * w * m->ch;
		if (bits == 1) for (int x = 0; x < w; x++) o[x] = cur[x >> 3] & (0x80 >> (x & 7)) ? 255 : 0; else memcpy(o, cur, row);
		memcpy(prev, cur, row);
	}
	inflateEnd(&z); free(raw); free(prev); free(cur); free(idat.b);
	if (r) { free(m->px); m->px = NULL; } return r;
}

/* ---------------------------------------------------------------- JPEG (baseline, 4:4:4; ITU-T T.81 Annex K's tables) */
static const unsigned char ZZ[64] = {0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};
static const unsigned char QL[64] = {16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55, 14, 13, 16, 24, 40, 57, 69, 56, 14, 17, 22, 29, 51, 87, 80, 62,
	18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92, 49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99};
static const unsigned char QC[64] = {17, 18, 24, 47, 99, 99, 99, 99, 18, 21, 26, 66, 99, 99, 99, 99, 24, 26, 56, 99, 99, 99, 99, 99, 47, 66, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99};
static const unsigned char DCL_B[16] = {0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0}, DCC_B[16] = {0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0};
static const unsigned char DC_V[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
static const unsigned char ACL_B[16] = {0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d}, ACC_B[16] = {0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77};
static const unsigned char ACL_V[162] = {0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
	0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75,
	0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9,
	0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
	0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa};
static const unsigned char ACC_V[162] = {0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
	0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x35, 0x36,
	0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74,
	0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
	0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa};
typedef struct { unsigned short code[256]; unsigned char len[256]; } huff;
static void huff_make(huff *t, const unsigned char *bits, const unsigned char *vals)
{
	memset(t, 0, sizeof *t); int code = 0, k = 0;
	for (int l = 1; l <= 16; l++) { for (int i = 0; i < bits[l - 1]; i++) { t->code[vals[k]] = code++; t->len[vals[k]] = l; k++; } code <<= 1; }
}
typedef struct { buf *o; uint32_t acc; int nb; } bitw;
static void bits_put(bitw *b, uint32_t v, int n)
{
	b->acc = b->acc << n | (v & ((1u << n) - 1)); b->nb += n;
	while (b->nb >= 8) { int c = b->acc >> (b->nb - 8) & 0xff; put8(b->o, c); if (c == 0xff) put8(b->o, 0); b->nb -= 8; }
}
static void bits_flush(bitw *b) { if (b->nb) bits_put(b, 0x7f, 8 - b->nb); }
static int nbits(int v) { v = v < 0 ? -v : v; int n = 0; while (v) { n++; v >>= 1; } return n; }
static double COS8[8][8];
static void dct8(const double *in, double *out, int step)
{
	for (int u = 0; u < 8; u++) { double s = 0; for (int x = 0; x < 8; x++) s += in[x * step] * COS8[u][x]; out[u * step] = s * (u ? 0.5 : 0.5 / M_SQRT2); }
}
static void jpeg_block(bitw *b, const double *blk, const int *q, int *dcprev, const huff *dc, const huff *ac)
{
	double t[64], f[64];
	for (int y = 0; y < 8; y++) dct8(blk + y * 8, t + y * 8, 1);
	for (int x = 0; x < 8; x++) dct8(t + x, f + x, 8);
	int z[64]; for (int i = 0; i < 64; i++) { double v = f[ZZ[i]] / q[i]; z[i] = (int)(v < 0 ? v - 0.5 : v + 0.5); }
	int d = z[0] - *dcprev; *dcprev = z[0]; int n = nbits(d);
	bits_put(b, dc->code[n], dc->len[n]); if (n) bits_put(b, d < 0 ? d - 1 : d, n);
	int run = 0;
	for (int i = 1; i < 64; i++) {
		if (!z[i]) { run++; continue; }
		while (run > 15) { bits_put(b, ac->code[0xf0], ac->len[0xf0]); run -= 16; }
		int v = z[i], s = nbits(v), sym = run << 4 | s; bits_put(b, ac->code[sym], ac->len[sym]); bits_put(b, v < 0 ? v - 1 : v, s); run = 0;
	}
	if (run) bits_put(b, ac->code[0], ac->len[0]);
}
static int jpeg_encode(const img *m, int quality, buf *o)
{
	for (int u = 0; u < 8; u++) for (int x = 0; x < 8; x++) COS8[u][x] = cos((2 * x + 1) * u * M_PI / 16);
	quality = quality < 1 ? 1 : quality > 100 ? 100 : quality; int sc = quality < 50 ? 5000 / quality : 200 - quality * 2;
	unsigned char ql[64], qc[64]; int qlz[64], qcz[64];
	for (int i = 0; i < 64; i++) {
		int a = (QL[ZZ[i]] * sc + 50) / 100, c = (QC[ZZ[i]] * sc + 50) / 100;
		ql[i] = a < 1 ? 1 : a > 255 ? 255 : a; qc[i] = c < 1 ? 1 : c > 255 ? 255 : c; qlz[i] = ql[i]; qcz[i] = qc[i];
	}
	huff dcl, acl, dcc, acc; huff_make(&dcl, DCL_B, DC_V); huff_make(&acl, ACL_B, ACL_V); huff_make(&dcc, DCC_B, DC_V); huff_make(&acc, ACC_B, ACC_V);
	int nc = m->ch;
	put16(o, 0xffd8);
	put16(o, 0xffe0); put16(o, 16); putstr(o, "JFIF"); put8(o, 0); put8(o, 1); put8(o, 1); put8(o, 1); put16(o, m->dpi > 0 ? m->dpi : 72); put16(o, m->dpi > 0 ? m->dpi : 72); put8(o, 0); put8(o, 0);
	put16(o, 0xffdb); put16(o, nc == 3 ? 132 : 67); put8(o, 0); put(o, ql, 64); if (nc == 3) { put8(o, 1); put(o, qc, 64); }
	put16(o, 0xffc0); put16(o, 8 + 3 * nc); put8(o, 8); put16(o, m->h); put16(o, m->w); put8(o, nc);
	for (int c = 0; c < nc; c++) { put8(o, c + 1); put8(o, 0x11); put8(o, c ? 1 : 0); }
	const unsigned char *tb[4] = {DCL_B, ACL_B, DCC_B, ACC_B}, *tv[4] = {DC_V, ACL_V, DC_V, ACC_V}; int tc[4] = {0x00, 0x10, 0x01, 0x11}, tn[4] = {12, 162, 12, 162};
	for (int t = 0; t < (nc == 3 ? 4 : 2); t++) { put16(o, 0xffc4); put16(o, 3 + 16 + tn[t]); put8(o, tc[t]); put(o, tb[t], 16); put(o, tv[t], tn[t]); }
	put16(o, 0xffda); put16(o, 6 + 2 * nc); put8(o, nc); for (int c = 0; c < nc; c++) { put8(o, c + 1); put8(o, c ? 0x11 : 0x00); } put8(o, 0); put8(o, 63); put8(o, 0);
	bitw b = {o, 0, 0}; int dcp[3] = {0, 0, 0}; double blk[3][64];
	for (int by = 0; by < m->h; by += 8) for (int bx = 0; bx < m->w; bx += 8) {
		for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) {
			int sx = bx + x < m->w ? bx + x : m->w - 1, sy = by + y < m->h ? by + y : m->h - 1;
			const unsigned char *p = m->px + ((size_t)sy * m->w + sx) * nc;
			if (nc == 1) blk[0][y * 8 + x] = p[0] - 128.0;
			else {
				double r = p[0], g = p[1], bl = p[2];
				blk[0][y * 8 + x] = 0.299 * r + 0.587 * g + 0.114 * bl - 128;
				blk[1][y * 8 + x] = -0.168736 * r - 0.331264 * g + 0.5 * bl;
				blk[2][y * 8 + x] = 0.5 * r - 0.418688 * g - 0.081312 * bl;
			}
		}
		for (int c = 0; c < nc; c++) jpeg_block(&b, blk[c], c ? qcz : qlz, &dcp[c], c ? &dcc : &dcl, c ? &acc : &acl);
	}
	bits_flush(&b); put16(o, 0xffd9); return o->b ? 0 : -1;
}

/* ---------------------------------------------------------------- PDF: one image a page */
static int pdf_write(img *pages, int n, buf *o)
{
	size_t off[4 + 3 * 512]; int no = 0; if (n > 500) return -1;
	putstr(o, "%PDF-1.4\n%\xe2\xe3\xcf\xd3\n");
	off[++no] = o->n; putstr(o, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
	off[++no] = o->n; putstr(o, "2 0 obj\n<< /Type /Pages /Kids ["); for (int i = 0; i < n; i++) putf(o, " %d 0 R", 3 + 3 * i); putf(o, " ] /Count %d >>\nendobj\n", n);
	for (int i = 0; i < n; i++) {
		img *m = &pages[i]; int dpi = m->dpi > 0 ? m->dpi : 300, p = 3 + 3 * i; double pw = m->w * 72.0 / dpi, ph = m->h * 72.0 / dpi;
		off[++no] = o->n; putf(o, "%d 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %.2f %.2f] /Resources << /XObject << /Im0 %d 0 R >> >> /Contents %d 0 R >>\nendobj\n", p, pw, ph, p + 2, p + 1);
		char c[128]; int cl = snprintf(c, sizeof c, "q %.2f 0 0 %.2f 0 0 cm /Im0 Do Q\n", pw, ph);
		off[++no] = o->n; putf(o, "%d 0 obj\n<< /Length %d >>\nstream\n", p + 1, cl); put(o, c, cl); putstr(o, "endstream\nendobj\n");
		buf d = {0}; const char *filter, *cs = m->ch == 3 ? "/DeviceRGB" : "/DeviceGray"; int bpc = 8;
		if (m->bw) {   /* 1 bit a pixel, Flate (PDF's DeviceGray: 0 black, 1 white, as PNG's) */
			size_t row = (m->w + 7) / 8; unsigned char *raw = calloc(row, m->h); if (!raw) return -1;
			for (int y = 0; y < m->h; y++) for (int x = 0; x < m->w; x++) if (m->px[(size_t)y * m->w + x] >= 128) raw[y * row + (x >> 3)] |= 0x80 >> (x & 7);
			uLongf zl = compressBound(row * m->h); d.b = malloc(zl); if (!d.b || compress2(d.b, &zl, raw, row * m->h, 9) != Z_OK) { free(raw); free(d.b); return -1; }
			d.n = zl; free(raw); filter = "/FlateDecode"; bpc = 1;
		} else { if (jpeg_encode(m, 85, &d)) { free(d.b); return -1; } filter = "/DCTDecode"; }
		off[++no] = o->n; putf(o, "%d 0 obj\n<< /Type /XObject /Subtype /Image /Width %d /Height %d /ColorSpace %s /BitsPerComponent %d /Filter %s /Length %zu >>\nstream\n", p + 2, m->w, m->h, cs, bpc, filter, d.n);
		put(o, d.b, d.n); free(d.b); putstr(o, "\nendstream\nendobj\n");
	}
	size_t x = o->n; putf(o, "xref\n0 %d\n0000000000 65535 f \n", no + 1);
	for (int i = 1; i <= no; i++) putf(o, "%010zu 00000 n \n", off[i]);
	putf(o, "trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%zu\n%%%%EOF\n", no + 1, x);
	return o->b ? 0 : -1;
}

/* ---------------------------------------------------------------- files: written whole, never over another */
static int write_new(const char *path, const buf *b)
{
	char tmp[4200]; snprintf(tmp, sizeof tmp, "%s.rscan-%d", path, (int)getpid());
	int f = open(tmp, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0644); if (f < 0) return -1;
	size_t w = 0; while (w < b->n) { ssize_t k = write(f, b->b + w, b->n - w); if (k <= 0) break; w += k; }
	int ok = w == b->n && !fsync(f); close(f);
	if (!ok) { unlink(tmp); return -1; }
	if (link(tmp, path)) { int e = errno; unlink(tmp); errno = e; return -1; }
	unlink(tmp); return 0;
}
static unsigned char *slurp(const char *p, size_t *n, size_t max)
{
	int f = open(p, O_RDONLY | O_NOFOLLOW | O_CLOEXEC); if (f < 0) return NULL;
	struct stat st; if (fstat(f, &st) || !S_ISREG(st.st_mode) || (size_t)st.st_size > max) { close(f); return NULL; }
	unsigned char *d = malloc(st.st_size + 1); size_t r = 0;
	while (d && r < (size_t)st.st_size) { ssize_t k = read(f, d + r, st.st_size - r); if (k <= 0) break; r += k; }
	close(f); if (!d || r != (size_t)st.st_size) { free(d); return NULL; } d[r] = 0; if (n) *n = r; return d;
}

/* ---------------------------------------------------------------- HTTP (plain, IPv4: ipp-usb's eSCL, WSD's metadata) */
static int http_req(const char *ip, int port, const char *method, const char *path, const char *ctype, const char *body, buf *out, int ms)
{
	struct sockaddr_in a = {0}; a.sin_family = AF_INET; a.sin_port = htons(port); if (inet_pton(AF_INET, ip, &a.sin_addr) != 1) return -1;
	int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0); if (fd < 0) return -1;
	int end = now_ms() + ms;
	if (connect(fd, (struct sockaddr *)&a, sizeof a) && errno != EINPROGRESS) { close(fd); return -1; }
	struct pollfd p = {fd, POLLOUT, 0}; int e = 0; socklen_t el = sizeof e;
	if (poll(&p, 1, ms) != 1 || getsockopt(fd, SOL_SOCKET, SO_ERROR, &e, &el) || e) { close(fd); return -1; }
	buf rq = {0}; putf(&rq, "%s %s HTTP/1.1\r\nHost: %s:%d\r\nUser-Agent: rudolf-scan\r\nConnection: close\r\n", method, path, ip, port);
	if (body) { putf(&rq, "Content-Type: %s\r\nContent-Length: %zu\r\n", ctype, strlen(body)); }
	putstr(&rq, "\r\n"); if (body) putstr(&rq, body);
	size_t w = 0;
	while (w < rq.n) { p.events = POLLOUT; if (poll(&p, 1, end - now_ms()) != 1) break; ssize_t k = send(fd, rq.b + w, rq.n - w, MSG_NOSIGNAL); if (k <= 0) break; w += k; }
	int ok = w == rq.n; free(rq.b); buf r = {0};
	while (ok) {
		int left = end - now_ms(); if (left <= 0) break; p.events = POLLIN; if (poll(&p, 1, left) != 1) break;
		char t[8192]; ssize_t k = recv(fd, t, sizeof t, 0); if (k < 0 && errno == EAGAIN) continue; if (k <= 0) break;
		if (r.n + k > (512u << 10) || put(&r, t, k)) break;
	}
	close(fd);
	put8(&r, 0); r.n--;
	int code = 0; char *hdr_end = r.b ? strstr((char *)r.b, "\r\n\r\n") : NULL;
	if (!hdr_end || sscanf((char *)r.b, "HTTP/%*s %d", &code) != 1) { free(r.b); return -1; }
	char *bd = hdr_end + 4; size_t bl = r.n - (bd - (char *)r.b); *hdr_end = 0;
	if (strcasestr((char *)r.b, "transfer-encoding: chunked")) {   /* (chunks joined) */
		buf c = {0}; char *q = bd, *qe = bd + bl;
		while (q < qe) { char *nl = strstr(q, "\r\n"); if (!nl) break; long l = strtol(q, NULL, 16); q = nl + 2; if (l <= 0 || q + l > qe) break; put(&c, q, l); q += l + 2; }
		put8(&c, 0); c.n--; *out = c;
	} else { buf c = {0}; put(&c, bd, bl); put8(&c, 0); c.n--; *out = c; }
	free(r.b); return code;
}
/* an element's text (any namespace prefix), from S up to E: the first <x:NAME ...>TEXT</ */
static int xml_text(const char *s, const char *e, const char *name, char *out, size_t on, const char **after)
{
	size_t nl = strlen(name); out[0] = 0;
	for (const char *p = s; p && p < e; ) {
		p = memchr(p, '<', e - p); if (!p) break; p++;
		const char *q = p; while (q < e && *q != '>' && *q != ' ' && *q != '/' && *q != '\t' && *q != '\r' && *q != '\n') q++;
		const char *c = memchr(p, ':', q - p); const char *nm = c ? c + 1 : p;
		if ((size_t)(q - nm) == nl && !memcmp(nm, name, nl) && *p != '/') {
			const char *gt = memchr(q, '>', e - q); if (!gt) return -1; if (gt[-1] == '/') { if (after) *after = gt + 1; return 0; }
			const char *lt = memchr(gt + 1, '<', e - gt - 1); if (!lt) return -1;
			size_t l = lt - gt - 1; if (l >= on) l = on - 1; memcpy(out, gt + 1, l); out[l] = 0;
			char *a = out; while (isspace((unsigned char)*a)) a++; memmove(out, a, strlen(a) + 1); size_t z = strlen(out); while (z && isspace((unsigned char)out[z - 1])) out[--z] = 0;
			if (after) *after = lt; return 1;
		}
		p = q;
	}
	return 0;
}
/* a block <x:NAME ...> ... </x:NAME> (not nested in itself): its inside, from S up to E */
static int xml_block(const char *s, const char *e, const char *name, const char **bs, const char **be)
{
	char t[64]; snprintf(t, sizeof t, ":%s", name); size_t tl = strlen(t);
	for (const char *p = s; p < e; ) {
		p = memchr(p, '<', e - p); if (!p) return 0; p++;
		const char *q = p; while (q < e && *q != '>' && *q != ' ' && *q != '\t' && *q != '\r' && *q != '\n') q++;
		if (*p != '/' && (((size_t)(q - p) >= tl && !memcmp(q - tl, t, tl)) || ((size_t)(q - p) == tl - 1 && !memcmp(p, t + 1, tl - 1)))) {
			const char *gt = memchr(q, '>', e - q); if (!gt) return 0; *bs = gt + 1;
			for (const char *r = gt; r < e; ) {
				r = memchr(r, '<', e - r); if (!r) return 0; r++;
				if (*r == '/') { const char *z = r + 1; while (z < e && *z != '>') z++; if ((size_t)(z - r - 1) >= tl - 1 && (!memcmp(z - tl, t, tl) || ((size_t)(z - r - 1) == tl - 1 && !memcmp(r + 1, t + 1, tl - 1)))) { *be = r - 1; return 1; } }
			}
			return 0;
		}
		p = q;
	}
	return 0;
}
static int url_ok(const char *u)
{
	if (!u || strlen(u) > 300 || (strncmp(u, "http://", 7) && strncmp(u, "https://", 8))) return 0;
	for (; *u; u++) if (!isalnum((unsigned char)*u) && !strchr(":/._-~%[]", *u)) return 0;
	return 1;
}
/* the host and port of http://HOST:PORT/... (IPv4 only) */
static int url_host(const char *u, char *ip, size_t in, int *port)
{
	const char *h = strstr(u, "://"); if (!h) return -1; h += 3; const char *e = h + strcspn(h, ":/"); if ((size_t)(e - h) >= in) return -1;
	memcpy(ip, h, e - h); ip[e - h] = 0; *port = !strncmp(u, "https", 5) ? 443 : 80; if (*e == ':') *port = atoi(e + 1);
	struct in_addr a; return inet_pton(AF_INET, ip, &a) == 1 && *port > 0 && *port < 65536 ? 0 : -1;
}

/* ---------------------------------------------------------------- discovery: the list */
typedef struct { char id[340], name[160], model[160], via[16], proto[8], ip[64], uuid[80]; } scanner;
static scanner SC[64]; static int NSC;
static void add_scanner(const char *proto, const char *url, const char *name, const char *model, const char *via, const char *ip, const char *uuid)
{
	if (!url_ok(url) || NSC >= 64) return;
	char id[340]; snprintf(id, sizeof id, "%s:%s", proto, url);
	for (int i = 0; i < NSC; i++) {
		if (!strcmp(SC[i].id, id)) return;
		/* one entry a scanner: eSCL before WSD (the same address), plain http before https (the same UUID) */
		if (!strcmp(via, "network") && !strcmp(SC[i].via, "network") && *ip && !strcmp(SC[i].ip, ip)) {
			if (!strcmp(proto, "wsd")) return;
			if (!strcmp(SC[i].proto, "wsd") || (!strncmp(url, "http:", 5) && strstr(SC[i].id, "https:"))) { NSC--; memmove(&SC[i], &SC[i + 1], (NSC - i) * sizeof *SC); i--; continue; }
			if (strstr(id, "https:")) return;
		}
	}
	scanner *s = &SC[NSC++]; memset(s, 0, sizeof *s);
	snprintf(s->id, sizeof s->id, "%s", id); snprintf(s->name, sizeof s->name, "%s", *name ? name : *model ? model : ip);
	snprintf(s->model, sizeof s->model, "%s", model); snprintf(s->via, sizeof s->via, "%s", via); snprintf(s->proto, sizeof s->proto, "%s", proto);
	snprintf(s->ip, sizeof s->ip, "%s", ip); snprintf(s->uuid, sizeof s->uuid, "%s", uuid);
	for (char *c = s->name; *c; c++) if ((unsigned char)*c < 32) *c = ' ';
}

/* ---------------------------------------------------------------- multicast DNS (as rudolf-print find: RFC 6762/6763) */
typedef struct { char inst[256], host[256], rs[128], ty[160], uuid[80], ip[64]; int port, tls, srv, txt; } svc;
static svc S[64]; static int NS;
typedef struct { char name[256], ip[64]; } arec; static arec A[64]; static int NA;
static int rdname(const unsigned char *m, size_t n, size_t *i, char *out, size_t on)
{
	size_t o = 0, at = *i; int jumped = 0, hops = 0; out[0] = 0;
	while (at < n) {
		int l = m[at];
		if (l == 0) { at++; break; }
		if ((l & 0xC0) == 0xC0) { if (at + 1 >= n || ++hops > 20) return -1; size_t to = (l & 0x3F) << 8 | m[at + 1]; if (!jumped) *i = at + 2; jumped = 1; at = to; continue; }
		if (l & 0xC0) return -1;
		at++; if (at + l > n) return -1;
		if (o && o < on - 1) out[o++] = '.';
		for (int k = 0; k < l && o < on - 1; k++) out[o++] = m[at + k] == '.' ? '\x1f' : m[at + k];
		out[o] = 0; at += l;
	}
	if (!jumped) *i = at;
	return 0;
}
static void qname(buf *x, const char *name)
{
	const char *p = name;
	while (*p) { const char *d = strchr(p, '.'); size_t l = d ? (size_t)(d - p) : strlen(p); if (l > 63) l = 63; put8(x, l); for (size_t k = 0; k < l; k++) put8(x, p[k] == '\x1f' ? '.' : p[k]); p += l; while (*p && *p != '.') p++; if (*p == '.') p++; }
	put8(x, 0);
}
static int is_scan_svc(const char *nm) { return strstr(nm, "._uscan._tcp.") || strstr(nm, "._uscans._tcp."); }
static svc *svc_get(const char *inst)
{
	if (!is_scan_svc(inst)) return NULL;
	for (int i = 0; i < NS; i++) if (!strcasecmp(S[i].inst, inst)) return &S[i];
	if (NS >= 64) return NULL; svc *s = &S[NS++]; memset(s, 0, sizeof *s); snprintf(s->inst, sizeof s->inst, "%s", inst);
	s->tls = strstr(inst, "._uscans._tcp.") != NULL; return s;
}
static void parse_mdns(const unsigned char *m, size_t n)
{
	if (n < 12) return;
	int qd = m[4] << 8 | m[5], rr = (m[6] << 8 | m[7]) + (m[8] << 8 | m[9]) + (m[10] << 8 | m[11]); size_t i = 12; char nm[256], t[256];
	for (int k = 0; k < qd; k++) { if (rdname(m, n, &i, nm, sizeof nm)) return; i += 4; }
	for (int k = 0; k < rr; k++) {
		if (rdname(m, n, &i, nm, sizeof nm) || i + 10 > n) return;
		int type = m[i] << 8 | m[i + 1]; size_t len = m[i + 8] << 8 | m[i + 9]; i += 10; if (i + len > n) return; size_t d = i;
		if (type == 12 && (!strcasecmp(nm, "_uscan._tcp.local") || !strcasecmp(nm, "_uscans._tcp.local"))) { size_t j = d; if (!rdname(m, n, &j, t, sizeof t)) svc_get(t); }
		else if (type == 33 && len > 6) { svc *s = svc_get(nm); size_t j = d + 6; if (s && !rdname(m, n, &j, t, sizeof t)) { s->port = m[d + 4] << 8 | m[d + 5]; snprintf(s->host, sizeof s->host, "%s", t); s->srv = 1; } }
		else if (type == 16) {
			svc *s = svc_get(nm); if (s) { s->txt = 1; size_t j = d;
				while (j < d + len) { int l = m[j++]; if (j + l > d + len) break; char kv[300]; snprintf(kv, sizeof kv, "%.*s", l, (const char *)m + j); j += l;
					char *eq = strchr(kv, '='); if (!eq) continue; *eq++ = 0;
					if (!strcasecmp(kv, "rs")) snprintf(s->rs, sizeof s->rs, "%s", eq); else if (!strcasecmp(kv, "ty")) snprintf(s->ty, sizeof s->ty, "%s", eq);
					else if (!strcasecmp(kv, "UUID")) snprintf(s->uuid, sizeof s->uuid, "%s", eq); } }
		}
		else if (type == 1 && len == 4 && NA < 64) { snprintf(A[NA].name, sizeof A[NA].name, "%s", nm); inet_ntop(AF_INET, m + d, A[NA].ip, sizeof A[NA].ip); NA++; }
		i += len;
	}
}
/* the interfaces asked on (not loopback, point-to-point or a cellular modem's) and this computer's own addresses */
static int MIF[16], NMIF; static struct in_addr OWN[32]; static int NOWN;
static int mif_has(int ix) { for (int i = 0; i < NMIF; i++) if (MIF[i] == ix) return 1; return 0; }
static int is_cellular(const char *ifn)
{
	const char *p = getenv("RS_CELLULAR"); FILE *f = fopen(p ? p : "/run/rudolf/firewall-cellular", "r"); char l[64]; int r = 0;
	if (!f) return 0;
	while (!r && fgets(l, sizeof l, f)) { l[strcspn(l, "\r\n")] = 0; if (!strcmp(l, ifn)) r = 1; }
	fclose(f); return r;
}
static void find_ifs(void)
{
	struct ifaddrs *ifs = NULL; NMIF = NOWN = 0; if (getifaddrs(&ifs)) return;
	for (struct ifaddrs *a = ifs; a; a = a->ifa_next) {
		if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET) continue;
		if (NOWN < 32) OWN[NOWN++] = ((struct sockaddr_in *)a->ifa_addr)->sin_addr;
		unsigned fl = a->ifa_flags; if (!(fl & IFF_UP) || !(fl & IFF_MULTICAST) || (fl & (IFF_LOOPBACK | IFF_POINTOPOINT))) continue;
		int ix = if_nametoindex(a->ifa_name); if (!ix || mif_has(ix) || NMIF >= 16 || is_cellular(a->ifa_name)) continue;
		MIF[NMIF++] = ix;
	}
	freeifaddrs(ifs);
}
static int own_addr(struct in_addr a) { for (int i = 0; i < NOWN; i++) if (OWN[i].s_addr == a.s_addr) return 1; return 0; }
static int recv_from_link(int fd, unsigned char *m, size_t mn, struct sockaddr_in *from, int strict)
{
	char cb[256]; struct iovec io = {m, mn}; struct msghdr mh = {from, sizeof *from, &io, 1, cb, sizeof cb, 0};
	ssize_t n = recvmsg(fd, &mh, 0); if (n <= 0) return -1;
	if (strict) {
		int ix = 0; for (struct cmsghdr *c = CMSG_FIRSTHDR(&mh); c; c = CMSG_NXTHDR(&mh, c)) if (c->cmsg_level == IPPROTO_IP && c->cmsg_type == IP_PKTINFO) ix = ((struct in_pktinfo *)CMSG_DATA(c))->ipi_ifindex;
		if (!mif_has(ix) || own_addr(from->sin_addr)) return -1;
	}
	return (int)n;
}
static void mdns_send(int fd, struct sockaddr_in *to, const char **names, const int *types, int k, int mcast)
{
	buf q = {0}; put16(&q, 0); put16(&q, 0); put16(&q, k); put16(&q, 0); put16(&q, 0); put16(&q, 0);
	for (int i = 0; i < k; i++) { qname(&q, names[i]); put16(&q, types[i]); put16(&q, 1); }
	if (mcast) for (int i = 0; i < NMIF; i++) { struct ip_mreqn mr = {0}; mr.imr_ifindex = MIF[i]; setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &mr, sizeof mr); if (sendto(fd, q.b, q.n, 0, (struct sockaddr *)to, sizeof *to) < 0) {} }
	else if (sendto(fd, q.b, q.n, 0, (struct sockaddr *)to, sizeof *to) < 0) {}
	free(q.b);
}
static void mdns_listen(int fd, int ms, int mcast)
{
	int end = now_ms() + ms;
	for (;;) {
		int left = end - now_ms(); if (left <= 0) break;
		struct pollfd p = {fd, POLLIN, 0}; if (poll(&p, 1, left) <= 0) break;
		unsigned char m[9000]; struct sockaddr_in from = {0}; int n = recv_from_link(fd, m, sizeof m, &from, mcast);
		if (n < 12 || !(m[2] & 0x80)) continue;   /* (responses only) */
		if (mcast && ntohs(from.sin_port) != 5353) continue;   /* (RFC 6762 6: from port 5353) */
		parse_mdns(m, n);
	}
}
static int wait_ms(void) { const char *w = getenv("RS_WAIT"); int v = w ? atoi(w) : 1500; return v < 200 ? 200 : v > 10000 ? 10000 : v; }
static void find_mdns(void)
{
	int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0); if (fd < 0) return;
	struct sockaddr_in to = {0}; to.sin_family = AF_INET; to.sin_port = htons(5353); inet_pton(AF_INET, "224.0.0.251", &to.sin_addr);
	const char *ov = getenv("RS_MDNS"); int mcast = 0;
	if (ov) { char a[64]; snprintf(a, sizeof a, "%s", ov); char *c = strrchr(a, ':'); if (c) { *c = 0; to.sin_port = htons(atoi(c + 1)); } inet_pton(AF_INET, a, &to.sin_addr); }
	else {
		int one = 1, zero = 0; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
		setsockopt(fd, IPPROTO_IP, IP_MULTICAST_ALL, &zero, sizeof zero); setsockopt(fd, IPPROTO_IP, IP_PKTINFO, &one, sizeof one);
		unsigned char lp = 0; setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &lp, 1);
		struct sockaddr_in me = {0}; me.sin_family = AF_INET; me.sin_port = htons(5353); me.sin_addr.s_addr = htonl(INADDR_ANY);
		if (!bind(fd, (struct sockaddr *)&me, sizeof me)) {
			mcast = 1;
			for (int i = 0; i < NMIF; i++) { struct ip_mreqn mr = {0}; inet_pton(AF_INET, "224.0.0.251", &mr.imr_multiaddr); mr.imr_ifindex = MIF[i]; setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mr, sizeof mr); }
		}
	}
	unsigned char ttl = 255; setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, 1);
	const char *n1[] = {"_uscan._tcp.local", "_uscans._tcp.local"}; const int t1[] = {12, 12}; int w = wait_ms();
	mdns_send(fd, &to, n1, t1, 2, mcast); mdns_listen(fd, w * 2 / 3, mcast); mdns_send(fd, &to, n1, t1, 2, mcast); mdns_listen(fd, w / 2, mcast);
	for (int round = 0; round < 2; round++) {   /* (what the first answers left out) */
		const char *nn[32]; int tt[32], k = 0;
		for (int i = 0; i < NS && k < 30; i++) { if (!S[i].srv) { nn[k] = S[i].inst; tt[k++] = 33; } if (!S[i].txt) { nn[k] = S[i].inst; tt[k++] = 16; } }
		for (int i = 0; i < NS && k < 31; i++) if (S[i].srv) { int have = 0; for (int j = 0; j < NA; j++) if (!strcasecmp(A[j].name, S[i].host)) have = 1; if (!have) { nn[k] = S[i].host; tt[k++] = 1; } }
		if (!k) break; mdns_send(fd, &to, nn, tt, k, mcast); mdns_listen(fd, w / 2, mcast);
	}
	close(fd);   /* (the group left, the port given back) */
	for (int i = 0; i < NS; i++) {
		svc *s = &S[i]; if (!s->srv || !s->port) continue;
		for (int j = 0; j < NA; j++) if (!strcasecmp(A[j].name, s->host)) snprintf(s->ip, sizeof s->ip, "%s", A[j].ip);
		if (!*s->ip) continue;
		char label[256]; snprintf(label, sizeof label, "%s", s->inst); char *d = strchr(label, '.'); if (d) *d = 0; for (char *c = label; *c; c++) if (*c == '\x1f') *c = '.';
		char rs[130]; snprintf(rs, sizeof rs, "%s", *s->rs ? s->rs : "eSCL"); while (rs[0] == '/') memmove(rs, rs + 1, strlen(rs));
		char url[400]; snprintf(url, sizeof url, "%s://%s:%d/%s", s->tls ? "https" : "http", s->ip, s->port, rs);
		add_scanner("escl", url, label, s->ty, "network", s->ip, s->uuid);
	}
}

/* ---------------------------------------------------------------- WS-Discovery (WSD scanners) */
static void uuid4(char *o)
{
	unsigned char r[16]; int f = open("/dev/urandom", O_RDONLY | O_CLOEXEC); if (f < 0 || read(f, r, 16) != 16) { struct timespec t; clock_gettime(CLOCK_REALTIME, &t); for (int i = 0; i < 16; i++) r[i] = (t.tv_nsec >> (i % 4 * 8)) ^ (getpid() * 31 + i); }
	if (f >= 0) close(f);
	r[6] = (r[6] & 0x0f) | 0x40; r[8] = (r[8] & 0x3f) | 0x80;
	sprintf(o, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8], r[9], r[10], r[11], r[12], r[13], r[14], r[15]);
}
static const char SOAP_NS[] = "xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\"";
/* the scan service of a WSD device at URL (its metadata, WS-Transfer Get): the address sane-airscan scans at */
static void wsd_metadata(const char *xaddr, const char *from_ip, const char *epr)
{
	char ip[64]; int port; if (!url_ok(xaddr) || strncmp(xaddr, "http://", 7) || url_host(xaddr, ip, sizeof ip, &port) || strcmp(ip, from_ip)) return;
	const char *path = strchr(xaddr + 7, '/'); if (!path) path = "/";
	char mid[40], body[1400]; uuid4(mid);
	snprintf(body, sizeof body, "<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope %s><soap:Header><wsa:To>%s</wsa:To>"
		"<wsa:Action>http://schemas.xmlsoap.org/ws/2004/09/transfer/Get</wsa:Action><wsa:MessageID>urn:uuid:%s</wsa:MessageID>"
		"<wsa:ReplyTo><wsa:Address>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</wsa:Address></wsa:ReplyTo></soap:Header><soap:Body/></soap:Envelope>", SOAP_NS, epr, mid);
	buf r = {0}; int code = http_req(ip, port, "POST", path, "application/soap+xml", body, &r, 3000);
	if (code != 200 || !r.b) { free(r.b); return; }
	const char *s = (char *)r.b, *e = s + r.n; char name[160] = "", model[160] = "", maker[100] = "";
	xml_text(s, e, "FriendlyName", name, sizeof name, NULL); xml_text(s, e, "ModelName", model, sizeof model, NULL); xml_text(s, e, "Manufacturer", maker, sizeof maker, NULL);
	if (*maker && *model && strncasecmp(model, maker, strlen(maker))) { char t[160]; snprintf(t, sizeof t, "%s %s", maker, model); snprintf(model, sizeof model, "%s", t); }
	/* every Hosted service whose Types name ScannerServiceType: its EndpointReference's Address */
	for (const char *p = s; p < e; ) {
		const char *hs, *he; if (xml_block(p, e, "Hosted", &hs, &he) != 1) break;
		char types[400], addr[400]; xml_text(hs, he, "Types", types, sizeof types, NULL);
		if (strstr(types, "ScannerServiceType") && xml_text(hs, he, "Address", addr, sizeof addr, NULL) == 1) {
			char aip[64]; int ap;
			if (url_ok(addr) && !strncmp(addr, "http://", 7) && !url_host(addr, aip, sizeof aip, &ap) && !strcmp(aip, from_ip))
				add_scanner("wsd", addr, name, model, "network", from_ip, epr);
		}
		p = he + 1;
	}
	free(r.b);
}
static void find_wsd(void)
{
	int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0); if (fd < 0) return;
	int one = 1; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one); setsockopt(fd, IPPROTO_IP, IP_PKTINFO, &one, sizeof one);
	struct sockaddr_in to = {0}; to.sin_family = AF_INET; to.sin_port = htons(3702); inet_pton(AF_INET, "239.255.255.250", &to.sin_addr);
	const char *ov = getenv("RS_WSD"); int strict = !ov;
	if (ov) { char a[64]; snprintf(a, sizeof a, "%s", ov); char *c = strrchr(a, ':'); if (c) { *c = 0; to.sin_port = htons(atoi(c + 1)); } inet_pton(AF_INET, a, &to.sin_addr); }
	else {   /* (from port 3702: the firewall lets answers to it in for ten seconds after "firewall wsd open") */
		struct sockaddr_in me = {0}; me.sin_family = AF_INET; me.sin_port = htons(3702); me.sin_addr.s_addr = htonl(INADDR_ANY);
		if (bind(fd, (struct sockaddr *)&me, sizeof me)) { close(fd); return; }
	}
	unsigned char ttl = 1, lp = 0; setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, 1); setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &lp, 1);
	char mid[40], probe[1200]; uuid4(mid);
	int pl = snprintf(probe, sizeof probe, "<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope %s xmlns:wsd=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" xmlns:wscn=\"http://schemas.microsoft.com/windows/2006/08/wdp/scan\">"
		"<soap:Header><wsa:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</wsa:To><wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</wsa:Action>"
		"<wsa:MessageID>urn:uuid:%s</wsa:MessageID></soap:Header><soap:Body><wsd:Probe><wsd:Types>wscn:ScanDeviceType</wsd:Types></wsd:Probe></soap:Body></soap:Envelope>", SOAP_NS, mid);
	for (int round = 0; round < 2; round++) {
		if (strict) for (int i = 0; i < NMIF; i++) { struct ip_mreqn mr = {0}; mr.imr_ifindex = MIF[i]; setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &mr, sizeof mr); if (sendto(fd, probe, pl, 0, (struct sockaddr *)&to, sizeof to) < 0) {} }
		else if (sendto(fd, probe, pl, 0, (struct sockaddr *)&to, sizeof to) < 0) {}
		int end = now_ms() + (round ? wait_ms() / 2 : wait_ms() * 2 / 3);
		for (;;) {
			int left = end - now_ms(); if (left <= 0) break;
			struct pollfd p = {fd, POLLIN, 0}; if (poll(&p, 1, left) <= 0) break;
			unsigned char m[16384]; struct sockaddr_in from = {0}; int n = recv_from_link(fd, m, sizeof m - 1, &from, strict); if (n <= 0) continue; m[n] = 0;
			const char *s = (char *)m, *e = s + n; char rel[100], tmp[60];
			/* only a ProbeMatches for this Probe (its random MessageID: answers no one else could have read off the network
			   before they arrive are trusted no further than that) and a scanner's */
			snprintf(tmp, sizeof tmp, "urn:uuid:%s", mid);
			if (xml_text(s, e, "RelatesTo", rel, sizeof rel, NULL) != 1 || strcmp(rel, tmp)) continue;
			const char *ms, *me;
			for (const char *q = s; xml_block(q, e, "ProbeMatch", &ms, &me) == 1; q = me + 1) {
				char types[400], xa[1200], epr[200]; xml_text(ms, me, "Types", types, sizeof types, NULL);
				if (!strstr(types, "ScanDeviceType")) continue;
				if (xml_text(ms, me, "XAddrs", xa, sizeof xa, NULL) != 1) continue;
				if (xml_text(ms, me, "Address", epr, sizeof epr, NULL) != 1 || strncmp(epr, "urn:uuid:", 9)) continue;
				for (char *c = epr; *c; c++) if (!isalnum((unsigned char)*c) && !strchr(":-", *c)) { *epr = 0; break; }
				if (!*epr) continue;
				char fip[64]; inet_ntop(AF_INET, &from.sin_addr, fip, sizeof fip);
				for (char *t = strtok(xa, " \t\r\n"); t; t = strtok(NULL, " \t\r\n")) if (!strncmp(t, "http://", 7)) { wsd_metadata(t, fip, epr); break; }
			}
		}
	}
	close(fd);
}

/* ---------------------------------------------------------------- USB scanners that speak eSCL through ipp-usb */
static void find_usb_escl(void)
{
	const char *p = getenv("RS_USB"); FILE *f = fopen(p ? p : "/run/rudolf/usb-printers", "r"); if (!f) return;
	char l[1600];
	while (fgets(l, sizeof l, f)) {
		l[strcspn(l, "\r\n")] = 0; char *fld[5] = {0}; int k = 0; for (char *t = l, *e; t && k < 5; t = e) { e = strchr(t, '\t'); if (e) *e++ = 0; fld[k++] = t; }
		if (k < 3 || strcmp(fld[0], "ipp") || strncmp(fld[2], "ipp://127.0.0.1:", 16)) continue;
		int port = atoi(fld[2] + 16); if (port <= 0 || port > 65535) continue;
		buf r = {0}; int code = http_req("127.0.0.1", port, "GET", "/eSCL/ScannerCapabilities", NULL, NULL, &r, 4000);
		if (code == 200 && r.b && strstr((char *)r.b, "ScannerCapabilities")) {
			char model[160] = "", uuid[80] = "", url[80], name[200];
			xml_text((char *)r.b, (char *)r.b + r.n, "MakeAndModel", model, sizeof model, NULL); xml_text((char *)r.b, (char *)r.b + r.n, "UUID", uuid, sizeof uuid, NULL);
			snprintf(url, sizeof url, "http://127.0.0.1:%d/eSCL", port); snprintf(name, sizeof name, "USB: %s", *fld[1] ? fld[1] : *model ? model : "scanner");
			add_scanner("escl", url, name, model, "usb", "127.0.0.1", uuid);
		}
		free(r.b);
	}
	fclose(f);
}

/* ---------------------------------------------------------------- SANE (opened with dlopen: the base layer has libsane from 1.6.2) */
typedef int SANE_Int, SANE_Word, SANE_Bool, SANE_Status; typedef void *SANE_Handle; typedef const char *SANE_String_Const;
enum { ST_GOOD = 0, ST_UNSUPPORTED, ST_CANCELLED, ST_BUSY, ST_INVAL, ST_EOF, ST_JAMMED, ST_NO_DOCS, ST_COVER_OPEN, ST_IO_ERROR, ST_NO_MEM, ST_DENIED };
enum { T_BOOL = 0, T_INT, T_FIXED, T_STRING, T_BUTTON, T_GROUP };
enum { C_NONE = 0, C_RANGE, C_WORD_LIST, C_STRING_LIST };
enum { F_GRAY = 0, F_RGB, F_RED, F_GREEN, F_BLUE };
#define CAP_INACTIVE 32
#define CAP_SOFT_SELECT 1
#define FIX(v) ((SANE_Word)((v) * (1 << 16)))
#define UNFIX(v) ((double)(v) / (1 << 16))
typedef struct { SANE_String_Const name, vendor, model, type; } SANE_Device;
typedef struct { SANE_Word min, max, quant; } SANE_Range;
typedef struct { SANE_String_Const name, title, desc; int type, unit; SANE_Int size, cap; int ctype; union { const SANE_String_Const *sl; const SANE_Word *wl; const SANE_Range *r; } c; } SANE_Option_Descriptor;
typedef struct { int format; SANE_Bool last_frame; SANE_Int bytes_per_line, pixels_per_line, lines, depth; } SANE_Parameters;
static struct {
	SANE_Status (*init)(SANE_Int *, void *); void (*exit)(void); SANE_Status (*get_devices)(const SANE_Device ***, SANE_Bool);
	SANE_Status (*open)(SANE_String_Const, SANE_Handle *); void (*close)(SANE_Handle);
	const SANE_Option_Descriptor *(*opt)(SANE_Handle, SANE_Int); SANE_Status (*control)(SANE_Handle, SANE_Int, int, void *, SANE_Int *);
	SANE_Status (*params)(SANE_Handle, SANE_Parameters *); SANE_Status (*start)(SANE_Handle); SANE_Status (*read)(SANE_Handle, unsigned char *, SANE_Int, SANE_Int *);
	void (*cancel)(SANE_Handle);
} SN;
static char CFG[64];
static volatile sig_atomic_t STOP;
static void on_stop(int s) { (void)s; STOP = 1; }
static void cfg_cleanup(void)
{
	if (!*CFG) return; const char *f[] = {"dll.conf", "airscan.conf", NULL}; char p[128];
	for (int i = 0; f[i]; i++) { snprintf(p, sizeof p, "%s/%s", CFG, f[i]); unlink(p); }
	rmdir(CFG); *CFG = 0;
}
static int backend_allowed(const char *be)
{
	const char *p = getenv("RS_BACKENDS"); FILE *f = fopen(p ? p : "/usr/share/rudolf/scan-backends", "r"); char l[64]; int ok = 0;
	if (!f) return 0;
	while (!ok && fgets(l, sizeof l, f)) { l[strcspn(l, " \t\r\n#")] = 0; if (*l && !strcmp(l, be)) ok = 1; }
	fclose(f); return ok;
}
static int sane_dev_ok(const char *d)
{
	size_t n = strlen(d); if (!n || n > 200 || strstr(d, "..")) return 0;
	for (; *d; d++) if (!isalnum((unsigned char)*d) && !strchr(":._-/", *d)) return 0;
	return 1;
}
/* SANE set up for ID: its own configuration (just this scanner), libsane opened; DEV gets SANE's device name */
static int sane_setup(const char *id, char *dev, size_t dn)
{
	const char *lib = getenv("RS_SANE"); void *h = dlopen(lib ? lib : "libsane.so.1", RTLD_NOW | RTLD_LOCAL);
	if (!h) return fail("nosane", "This Rudolf OS has no scanning library (libsane).");
#define L(f, n) if (!(*(void **)&SN.f = dlsym(h, n))) return fail("nosane", "The scanning library has no %s.", n);
	L(init, "sane_init") L(exit, "sane_exit") L(get_devices, "sane_get_devices") L(open, "sane_open") L(close, "sane_close") L(opt, "sane_get_option_descriptor")
	L(control, "sane_control_option") L(params, "sane_get_parameters") L(start, "sane_start") L(read, "sane_read") L(cancel, "sane_cancel")
#undef L
	const char *tmp = getenv("XDG_RUNTIME_DIR"); struct stat st;
	if (!tmp || stat(tmp, &st) || !S_ISDIR(st.st_mode) || st.st_uid != getuid() || strlen(tmp) > 40) tmp = "/tmp";
	snprintf(CFG, sizeof CFG, "%s/rscan-XXXXXX", tmp); if (!mkdtemp(CFG)) { *CFG = 0; return fail("write", "No room for the scanner's settings."); }
	atexit(cfg_cleanup);
	char p[128]; FILE *f; char be[40] = "airscan";
	if (!strncmp(id, "sane:", 5)) {
		const char *d = id + 5; size_t bl = strcspn(d, ":"); if (!sane_dev_ok(d) || !bl || bl >= sizeof be) return fail("args", "Not a scanner's name.");
		memcpy(be, d, bl); be[bl] = 0; if (!strcmp(be, "airscan") || !strcmp(be, "net") || !backend_allowed(be)) return fail("args", "This Rudolf OS has no driver called %s.", be);
		snprintf(dev, dn, "%s", d);
	}
	snprintf(p, sizeof p, "%s/dll.conf", CFG); if (!(f = fopen(p, "w"))) return fail("write", "No room for the scanner's settings."); fprintf(f, "%s\n", be); fclose(f);
	snprintf(p, sizeof p, "%s/airscan.conf", CFG); if (!(f = fopen(p, "w"))) return fail("write", "No room for the scanner's settings.");
	fprintf(f, "[options]\ndiscovery = disable\nmodel = network\n[devices]\n");
	if (!strncmp(id, "escl:", 5) || !strncmp(id, "wsd:", 4)) {
		const char *u = strchr(id, ':') + 1; if (!url_ok(u)) { fclose(f); return fail("args", "Not a scanner's address."); }
		fprintf(f, "\"Rudolf scanner\" = %s, %s\n", u, !strncmp(id, "wsd:", 4) ? "wsd" : "escl");
	} else if (strncmp(id, "sane:", 5)) { fclose(f); return fail("args", "Not a scanner's name."); }
	fclose(f);
	setenv("SANE_CONFIG_DIR", CFG, 1); unsetenv("SANE_DEBUG_DLL");
	SANE_Int v = 0; if (SN.init(&v, NULL) != ST_GOOD) return fail("nosane", "The scanning library did not start.");
	if (strncmp(id, "sane:", 5)) {   /* (sane-airscan's name for it: airscan:e0:Rudolf scanner, airscan:w0:...) */
		const SANE_Device **dl = NULL; *dev = 0;
		if (SN.get_devices(&dl, 0) == ST_GOOD && dl) for (int i = 0; dl[i] && !*dev; i++) if (dl[i]->name && !strncmp(dl[i]->name, "airscan:", 8)) snprintf(dev, dn, "%s", dl[i]->name);
		if (!*dev) return fail("noscanner", "The scanner could not be set up.");
	}
	return 0;
}
static const char *sane_code(SANE_Status s)
{
	switch (s) { case ST_BUSY: return "busy"; case ST_JAMMED: return "jam"; case ST_NO_DOCS: return "nodocs"; case ST_COVER_OPEN: return "open";
		case ST_DENIED: return "denied"; case ST_CANCELLED: return "cancelled"; case ST_NO_MEM: return "memory"; case ST_UNSUPPORTED: return "unsupported";
		case ST_INVAL: return "noscanner"; default: return "io"; }
}
static const char *sane_text(SANE_Status s)
{
	switch (s) {
	case ST_BUSY: return "The scanner is busy: another computer may be scanning, or it is printing or copying. Try again in a moment.";
	case ST_JAMMED: return "Paper is jammed in the scanner's document feeder. Clear it, then scan again.";
	case ST_NO_DOCS: return "There is no paper in the document feeder. Put the pages in, then scan again.";
	case ST_COVER_OPEN: return "The scanner's cover or document feeder is open. Close it, then scan again.";
	case ST_DENIED: return "This account may not use this scanner.";
	case ST_CANCELLED: return "The scan was cancelled.";
	case ST_NO_MEM: return "There is not enough memory for this scan. Try a lower resolution.";
	case ST_UNSUPPORTED: return "The scanner cannot do that.";
	case ST_INVAL: return "The scanner is not there any more. Is it switched on and on the same network?";
	default: return "The scanner stopped answering in the middle of the scan. Check that it is switched on and connected, then try again.";
	}
}
static int sane_fail(SANE_Status s) { return fail(sane_code(s), "%s", sane_text(s)); }
static SANE_Handle H;
static int opt_find(const char *name)
{
	for (int i = 1; i < 200; i++) { const SANE_Option_Descriptor *d = SN.opt(H, i); if (!d) break; if (d->name && !strcmp(d->name, name) && !(d->cap & CAP_INACTIVE)) return i; }
	return -1;
}
static const SANE_Option_Descriptor *optd(const char *name, int *ix) { *ix = opt_find(name); return *ix > 0 ? SN.opt(H, *ix) : NULL; }
static int opt_set_str(const char *name, const char *v)
{
	int i; const SANE_Option_Descriptor *d = optd(name, &i); if (!d || d->type != T_STRING || !(d->cap & CAP_SOFT_SELECT)) return -1;
	char b[256]; snprintf(b, sizeof b, "%s", v); SANE_Int info = 0; return SN.control(H, i, 1, b, &info) == ST_GOOD ? 0 : -1;
}
static int opt_set_num(const char *name, double v)
{
	int i; const SANE_Option_Descriptor *d = optd(name, &i); if (!d || (d->type != T_INT && d->type != T_FIXED) || !(d->cap & CAP_SOFT_SELECT)) return -1;
	SANE_Word w = d->type == T_FIXED ? FIX(v) : (SANE_Word)lround(v); SANE_Int info = 0; return SN.control(H, i, 1, &w, &info) == ST_GOOD ? 0 : -1;
}
static double opt_get_num(const char *name, double dflt)
{
	int i; const SANE_Option_Descriptor *d = optd(name, &i); if (!d || (d->type != T_INT && d->type != T_FIXED) || d->size != sizeof(SANE_Word)) return dflt;
	SANE_Word w = 0; if (SN.control(H, i, 0, &w, NULL) != ST_GOOD) return dflt; return d->type == T_FIXED ? UNFIX(w) : w;
}
/* a choice of a string option by what it means (its list's own word for it) */
static const char *pick(const char *name, const char *const *words)
{
	int i; const SANE_Option_Descriptor *d = optd(name, &i); if (!d || d->type != T_STRING || d->ctype != C_STRING_LIST) return NULL;
	for (int w = 0; words[w]; w++) for (int k = 0; d->c.sl[k]; k++) if (strcasestr(d->c.sl[k], words[w])) {
		if (strcasestr(d->c.sl[k], "duplex") && !strcasestr(words[w], "duplex")) continue;
		return d->c.sl[k];
	}
	return NULL;
}
static const char *const SRC_FLAT[] = {"flatbed", "platen", "normal", "document table", NULL}, *const SRC_ADF[] = {"adf", "feeder", "automatic document", NULL};
static const char *const MODE_COLOR[] = {"color", "colour", NULL}, *const MODE_GREY[] = {"gray", "grey", NULL}, *const MODE_BW[] = {"lineart", "binary", "black", NULL};
/* the resolutions it offers (up to 600 dpi), N of them into R */
static int resolutions(int *r, int max)
{
	int i, n = 0; const SANE_Option_Descriptor *d = optd("resolution", &i); if (!d) return 0;
	double f = d->type == T_FIXED ? 1.0 / (1 << 16) : 1;
	if (d->ctype == C_WORD_LIST) { for (int k = 1; k <= d->c.wl[0] && n < max; k++) { int v = (int)lround(d->c.wl[k] * f); if (v >= 50 && v <= 600) r[n++] = v; } }
	else if (d->ctype == C_RANGE) {
		static const int STD[] = {75, 100, 150, 200, 300, 400, 600}; double lo = d->c.r->min * f, hi = d->c.r->max * f;
		for (unsigned k = 0; k < sizeof STD / sizeof *STD && n < max; k++) if (STD[k] >= lo && STD[k] <= hi) r[n++] = STD[k];
		if (!n && hi >= 50) r[n++] = (int)lo;
	}
	return n;
}
static int range_mm(const char *name, double *hi)
{
	int i; const SANE_Option_Descriptor *d = optd(name, &i); if (!d || d->ctype != C_RANGE) return -1;
	*hi = d->type == T_FIXED ? UNFIX(d->c.r->max) : d->c.r->max; return 0;
}
static int open_scanner(const char *id)
{
	char dev[256] = ""; if (sane_setup(id, dev, sizeof dev)) return -1;
	SANE_Status s = SN.open(dev, &H); if (s != ST_GOOD) { if (s == ST_INVAL || s == ST_IO_ERROR) fail("noscanner", "%s", sane_text(ST_INVAL)); else sane_fail(s); return -1; }
	return 0;
}
static int cmd_caps(const char *id)
{
	if (open_scanner(id)) return 1;
	printf("{\"ok\":true,\"sources\":[");
	int first = 1; struct { const char *key; const char *const *w; } srcs[] = {{"flatbed", SRC_FLAT}, {"adf", SRC_ADF}};
	int nsrc = 0; for (int k = 0; k < 2; k++) if (pick("source", srcs[k].w)) nsrc++;
	for (int k = 0; k < 2; k++) {
		const char *v = pick("source", srcs[k].w); if (!v && (k || nsrc)) continue;   /* (no source option: one, the glass) */
		if (v) opt_set_str("source", v);
		double w = 0, h = 0; range_mm("br-x", &w); range_mm("br-y", &h); int r[16], nr = resolutions(r, 16);
		printf("%s{\"source\":\"%s\",\"width\":%.1f,\"height\":%.1f,\"resolutions\":[", first ? "" : ",", srcs[k].key, w, h); first = 0;
		for (int i = 0; i < nr; i++) printf("%s%d", i ? "," : "", r[i]);
		printf("]}");
	}
	printf("],\"modes\":[\"color\"");
	if (pick("mode", MODE_GREY)) printf(",\"grey\""); printf(",\"bw\"]}\n");   /* (black and white is made here when it has no line-art mode) */
	SN.close(H); SN.exit(); return 0;
}
/* one page from SANE into M (grey or RGB, 8 bits) */
static SANE_Status read_page(img *m, int want_grey)
{
	SANE_Parameters p; SANE_Status s; unsigned char *rgb = NULL; int W = 0, Hh = 0, ch = 0, passes = 0;
	for (;;) {
		if ((s = SN.params(H, &p)) != ST_GOOD) return s;
		if (p.depth != 1 && p.depth != 8 && p.depth != 16) return ST_UNSUPPORTED;
		int fch = p.format == F_RGB ? 3 : 1; int pw = p.pixels_per_line, bpl = p.bytes_per_line;
		if (pw <= 0 || bpl <= 0 || (int64_t)pw * fch * (p.depth == 16 ? 2 : 1) > (int64_t)bpl * 8 / (p.depth == 1 ? 1 : 8)) return ST_IO_ERROR;
		if (!passes) { W = pw; ch = p.format == F_GRAY ? 1 : 3; }
		else if (pw != W) return ST_IO_ERROR;
		/* the frame's bytes (lines -1: not known before the end) */
		buf raw = {0}; unsigned char t[65536]; SANE_Int got;
		while (!STOP) {
			s = SN.read(H, t, sizeof t, &got); if (s == ST_EOF) break; if (s != ST_GOOD) { free(raw.b); return s; }
			if (raw.n + got > (size_t)MAX_PIXELS * 3 || put(&raw, t, got)) { free(raw.b); SN.cancel(H); return ST_NO_MEM; }
		}
		if (STOP) { free(raw.b); SN.cancel(H); return ST_CANCELLED; }
		int lines = (int)(raw.n / bpl); if (!passes) Hh = lines;
		if (lines <= 0) { free(raw.b); return ST_IO_ERROR; }
		if (!rgb) { if ((uint64_t)W * Hh > MAX_PIXELS || !(rgb = calloc((size_t)W * Hh, ch))) { free(raw.b); return ST_NO_MEM; } }
		int c0 = p.format == F_GREEN ? 1 : p.format == F_BLUE ? 2 : 0;
		for (int y = 0; y < Hh && y < lines; y++) {
			const unsigned char *src = raw.b + (size_t)y * bpl; unsigned char *dst = rgb + (size_t)y * W * ch;
			for (int x = 0; x < W; x++) for (int c = 0; c < fch; c++) {
				int v;
				if (p.depth == 1) v = src[x >> 3] & (0x80 >> (x & 7)) ? 0 : 255;   /* (SANE's line art: 1 is black) */
				else if (p.depth == 16) { uint16_t q; memcpy(&q, src + ((size_t)x * fch + c) * 2, 2); v = q >> 8; }
				else v = src[(size_t)x * fch + c];
				if (fch == 3) dst[x * 3 + c] = v; else if (ch == 1) dst[x] = v; else if (p.format == F_GRAY) { dst[x * 3] = dst[x * 3 + 1] = dst[x * 3 + 2] = v; } else dst[x * 3 + c0] = v;
			}
		}
		free(raw.b); passes++;
		if (p.last_frame || passes >= 3) break;
		if ((s = SN.start(H)) != ST_GOOD) { free(rgb); return s; }
	}
	m->w = W; m->h = Hh; m->ch = ch; m->px = rgb;
	if (want_grey && ch == 3) {   /* (a scanner without a grey mode) */
		for (size_t i = 0; i < (size_t)W * Hh; i++) rgb[i] = (rgb[i * 3] * 299 + rgb[i * 3 + 1] * 587 + rgb[i * 3 + 2] * 114 + 500) / 1000;
		m->ch = 1;
	}
	return ST_GOOD;
}
static int cmd_scan(int argc, char **argv)
{
	const char *id = argv[2], *dir = argv[3]; char source[16] = "flatbed", mode[8] = "color"; int res = 0, first = 1, preview = 0; double area[4] = {-1, -1, -1, -1};
	for (int i = 4; i < argc; i++) {
		const char *a = argv[i];
		if (!strncmp(a, "source=", 7) && (!strcmp(a + 7, "flatbed") || !strcmp(a + 7, "adf"))) snprintf(source, sizeof source, "%s", a + 7);
		else if (!strncmp(a, "mode=", 5) && (!strcmp(a + 5, "color") || !strcmp(a + 5, "grey") || !strcmp(a + 5, "bw"))) snprintf(mode, sizeof mode, "%s", a + 5);
		else if (!strncmp(a, "res=", 4) && atoi(a + 4) >= 50 && atoi(a + 4) <= 600) res = atoi(a + 4);
		else if (!strncmp(a, "first=", 6) && atoi(a + 6) >= 1 && atoi(a + 6) <= 999) first = atoi(a + 6);
		else if (!strcmp(a, "preview=1")) preview = 1;
		else if (!strncmp(a, "area=", 5) && sscanf(a + 5, "%lf,%lf,%lf,%lf", &area[0], &area[1], &area[2], &area[3]) == 4 && area[0] >= 0 && area[1] >= 0 && area[2] > 1 && area[3] > 1 && area[2] < 2000 && area[3] < 2000) {}
		else return fail("args", "Unknown setting: %s", a);
	}
	struct stat st; if (stat(dir, &st) || !S_ISDIR(st.st_mode) || access(dir, W_OK)) return fail("write", "The scans' folder cannot be written.");
	if (open_scanner(id)) return 1;
	struct sigaction sa = {0}; sa.sa_handler = on_stop; sigaction(SIGTERM, &sa, NULL); sigaction(SIGINT, &sa, NULL); sigaction(SIGHUP, &sa, NULL);
	if (preview) { snprintf(source, sizeof source, "flatbed"); snprintf(mode, sizeof mode, "color"); area[0] = -1; }
	const char *v = pick("source", !strcmp(source, "adf") ? SRC_ADF : SRC_FLAT);
	if (!strcmp(source, "adf") && !v) { SN.close(H); SN.exit(); return fail("unsupported", "This scanner has no document feeder."); }
	if (v) opt_set_str("source", v);
	int bw_here = 0, grey_here = 0;
	if (!strcmp(mode, "bw")) { const char *m = pick("mode", MODE_BW); if (m) opt_set_str("mode", m); else { const char *g = pick("mode", MODE_GREY); if (g) opt_set_str("mode", g); else grey_here = 1; bw_here = 1; } }
	else if (!strcmp(mode, "grey")) { const char *m = pick("mode", MODE_GREY); if (m) opt_set_str("mode", m); else grey_here = 1; }
	else { const char *m = pick("mode", MODE_COLOR); if (m) opt_set_str("mode", m); }
	int r[16], nr = resolutions(r, 16), want = preview ? 75 : res ? res : 300, best = 0;
	for (int i = 0; i < nr; i++) if (!best || abs(r[i] - want) < abs(best - want) || (preview && r[i] >= 75 && best < 75)) best = r[i];
	if (preview) { for (int i = 0; i < nr; i++) if (r[i] >= 75 && (best < 75 || r[i] < best)) best = r[i]; }
	if (best) opt_set_num("resolution", best);
	double mw = 0, mh = 0; range_mm("br-x", &mw); range_mm("br-y", &mh);
	if (area[0] >= 0 && mw > 0 && mh > 0) {
		double l = fmin(area[0], mw - 1), t = fmin(area[1], mh - 1), rr = fmin(area[0] + area[2], mw), b = fmin(area[1] + area[3], mh);
		opt_set_num("tl-x", 0); opt_set_num("tl-y", 0); opt_set_num("br-x", rr); opt_set_num("br-y", b); opt_set_num("tl-x", l); opt_set_num("tl-y", t);
	} else if (mw > 0 && mh > 0) { opt_set_num("tl-x", 0); opt_set_num("tl-y", 0); opt_set_num("br-x", mw); opt_set_num("br-y", mh); }
	int dpi = (int)lround(opt_get_num("resolution", best ? best : 300));
	int adf = !strcmp(source, "adf") && !preview, n = 0; char names[64][32]; SANE_Status s = ST_GOOD; int ws = 0, hs = 0;
	for (;;) {
		s = SN.start(H);
		if (s != ST_GOOD) { if (n && s == ST_NO_DOCS) s = ST_GOOD; break; }   /* (the feeder emptied: done) */
		img m = {0}; s = read_page(&m, grey_here || bw_here);
		if (s != ST_GOOD) { free(m.px); break; }
		m.dpi = dpi; if (bw_here) { for (size_t i = 0; i < (size_t)m.w * m.h; i++) m.px[i] = m.px[i] >= 128 ? 255 : 0; }
		m.bw = !strcmp(mode, "bw");
		if (m.bw && m.ch == 3) { for (size_t i = 0; i < (size_t)m.w * m.h; i++) m.px[i] = m.px[i * 3]; m.ch = 1; }
		buf o = {0}; char path[4200];
		if (preview) snprintf(names[n], sizeof names[n], "preview.png"); else snprintf(names[n], sizeof names[n], "page-%d.png", first + n);
		snprintf(path, sizeof path, "%s/%s", dir, names[n]); if (preview) unlink(path);
		int e = png_encode(&m, &o) || write_new(path, &o); ws = m.w; hs = m.h; free(o.b); free(m.px);
		if (e) { SN.cancel(H); SN.close(H); SN.exit(); return fail(errno == EEXIST ? "exists" : "write", "The scanned page could not be kept (%s).", errno == EEXIST ? "a page of that number is there already" : strerror(errno)); }
		n++;
		if (!adf || n >= 60 || STOP) break;
	}
	SN.cancel(H); SN.close(H); SN.exit();
	if (STOP && s == ST_GOOD) s = ST_CANCELLED;
	if (!n) return sane_fail(s == ST_GOOD ? ST_IO_ERROR : s);
	printf("{\"ok\":true,\"pages\":["); for (int i = 0; i < n; i++) { printf("%s", i ? "," : ""); js(names[i]); }
	printf("],\"width\":%d,\"height\":%d,\"dpi\":%d,\"mode\":\"%s\",\"peakKB\":%ld", ws, hs, dpi, mode, peak_kb());
	if (s != ST_GOOD) { printf(",\"stopped\":"); js(sane_code(s)); printf(",\"error\":"); js(sane_text(s)); }   /* (pages kept before a jam) */
	printf("}\n"); return 0;
}

/* ---------------------------------------------------------------- save */
static int cmd_save(int argc, char **argv)
{
	const char *fmt = argv[2], *out = argv[3]; int np = argc - 4;
	if (strcmp(fmt, "pdf") && strcmp(fmt, "png") && strcmp(fmt, "jpeg")) return fail("args", "Saved as pdf, png or jpeg.");
	if (np < 1 || np > 500 || (strcmp(fmt, "pdf") && np != 1)) return fail("args", "Which pages.");
	if (!access(out, F_OK)) return fail("exists", "There is a file of that name already.");
	img *pg = calloc(np, sizeof *pg); buf o = {0}; int e = 0;
	if (!pg) return fail("memory", "Not enough memory.");
	for (int i = 0; i < np && !e; i++) {
		size_t n; unsigned char *d = slurp(argv[4 + i], &n, 512u << 20);
		if (!d || png_decode(d, n, &pg[i])) e = 1; free(d);
		if (e) { for (int k = 0; k <= i; k++) free(pg[k].px); free(pg); return fail("args", "Page %d is not a scanned page.", i + 1); }
	}
	if (!strcmp(fmt, "pdf")) {
		/* (one page in memory at a time would be smaller; pages are kept as PNG, so a page is at most 120 million pixels) */
		e = pdf_write(pg, np, &o);
	} else if (!strcmp(fmt, "png")) e = png_encode(&pg[0], &o);
	else e = jpeg_encode(&pg[0], 90, &o);
	for (int i = 0; i < np; i++) free(pg[i].px); free(pg);
	if (e) { free(o.b); return fail("memory", "The file could not be made (not enough memory?)."); }
	umask(022);
	if (write_new(out, &o)) { int er = errno; free(o.b); return fail(er == EEXIST ? "exists" : "write", "%s", er == EEXIST ? "There is a file of that name already." : er == ENOSPC ? "The drive is full." : "The file could not be written here."); }
	printf("{\"ok\":true,\"bytes\":%zu,\"pages\":%d,\"peakKB\":%ld}\n", o.n, np, peak_kb()); free(o.b); return 0;
}

static int cmd_find(void)
{
	find_ifs();
	if (!getenv("RS_ONLY") || strstr(getenv("RS_ONLY"), "mdns")) find_mdns();
	if (!getenv("RS_ONLY") || strstr(getenv("RS_ONLY"), "wsd")) find_wsd();
	if (!getenv("RS_ONLY") || strstr(getenv("RS_ONLY"), "usb")) find_usb_escl();
	printf("{\"ok\":true,\"scanners\":[");
	for (int i = 0; i < NSC; i++) {
		printf("%s{\"id\":", i ? "," : ""); js(SC[i].id); printf(",\"name\":"); js(SC[i].name); printf(",\"model\":"); js(SC[i].model);
		printf(",\"via\":"); js(SC[i].via); printf(",\"proto\":\"%s\"}", !strcmp(SC[i].proto, "wsd") ? "WSD" : "eSCL");
	}
	printf("]}\n"); return 0;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IOFBF, 1 << 16); signal(SIGPIPE, SIG_IGN); umask(022);
	alarm(1800);   /* (nothing here takes half an hour: a scanner that hangs does not keep this running) */
	if (argc >= 2 && !strcmp(argv[1], "find")) return cmd_find();
	if (argc >= 3 && !strcmp(argv[1], "caps")) return cmd_caps(argv[2]);
	if (argc >= 4 && !strcmp(argv[1], "scan")) return cmd_scan(argc, argv);
	if (argc >= 5 && !strcmp(argv[1], "save")) return cmd_save(argc, argv);
	fprintf(stderr, "usage: rudolf-scan find | caps ID | scan ID DIR [source=flatbed|adf] [mode=color|grey|bw] [res=DPI] [area=L,T,W,H] [first=N] [preview=1]\n"
		"       | save pdf OUT PAGE... | save png|jpeg OUT PAGE\n");
	return 2;
}
