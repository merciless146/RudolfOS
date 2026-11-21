/* rudolf-printq: Rudolf OS's print queue (1.6.1, docs/plan-1.6.1.md Part 9).
 *
 * Until 1.6.1 a job went straight from rudolfd to the printer (rudolf-print send), and only the desktop could print:
 * Linux apps (Debian's, Flatpak's) print through CUPS's client library, which needs a CUPS server, and Rudolf OS has
 * none. This is the queue both use, without CUPS and without a network listener:
 *   - one socket in the file system, /run/cups/cups.sock (where CUPS's own clients look; Debian's /run/cups and
 *     Flatpak's cups socket are this folder), speaking the part of IPP over HTTP that CUPS's clients use: the printers
 *     (CUPS-Get-Printers, CUPS-Get-Default, Get-Printer-Attributes), jobs (Print-Job, Create-Job and Send-Document,
 *     Validate-Job), the queue (Get-Jobs, Get-Job-Attributes) and cancelling (Cancel-Job, Cancel-My-Jobs, Purge-Jobs);
 *     so lp, lpstat, cancel, GTK's and Qt's print dialogs and LibreOffice print as on any Linux;
 *   - and rudolfd's own requests (/rudolf/...: the desktop's jobs, the queue for Settings › Devices, cancelling).
 * Who is who: the kernel says which user is at the other end of each connection (SO_PEERCRED; a Debian program's user
 * is the account's own Linux user, as Debian's user namespace maps it). Every job belongs to the user that sent it;
 * only that user may cancel it, and the queue shows another user's jobs without their names ("another account's
 * document"). Root (rudolfd) says for whom it asks (X-Rudolf-As: the signed-in account's user, or rudolf), and is held
 * to that user's jobs the same way; root without it is the system and may cancel any job.
 * The printers: the ones rudolf-print finds (the network's IPP Everywhere and AirPrint printers, the USB printers of
 * /run/rudolf/usb-printers), and the ones the desktop printed to by address (kept in the queue's folder). A job is sent
 * to its printer by rudolf-print send, one at a time per printer: a PDF as it is when the printer takes PDF (or it is
 * a USB printer: rudolf-print knows what each kind needs), otherwise as PWG or Apple raster made here from the PDF's
 * pages by Poppler's pdftocairo (the USB printers extension's, or the system's).
 * Started by rudolf-init as root: it makes the socket (0666: any program on this computer may print, as with CUPS),
 * then becomes the user "print" (and group lp, for /dev/usb/lpN) with no_new_privs, before it reads anything.
 *
 *   rudolf-printq serve                          the queue
 *   rudolf-printq jobs [--as USER]               the queue as JSON (rudolfd: GET print/jobs)
 *   rudolf-printq submit URI FILE FORMAT [copies=N] [sides=KW] [color=KW] [media=KW] [pages=A-B] [title-file=PATH]
 *                [name-file=PATH] [--as USER]   a job (rudolfd: POST print/job); prints {"ok":true,"job":N,...}
 *   rudolf-printq cancel ID [--as USER]          (rudolfd: POST print/cancel)
 *   rudolf-printq printers                       the printers the queue offers (JSON)
 * Elsewhere for the host tests: RPQ_SOCK (the socket), RPQ_SPOOL (its folder), RPQ_PRINT (rudolf-print), RPQ_USER (the
 * user it becomes; "-": stays), RPQ_POPPLER (the folder with pdftocairo), RPQ_KNOWN (the printers by address).
 *
 * Copyright (C) 2026 the Rudolf OS authors. SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>

#define ENVOR(e, d) (getenv(e) && *getenv(e) ? getenv(e) : (d))
static const char *SOCK, *SPOOL, *RPRINT, *POPPLER, *KNOWN;
#define MAXJ 200
#define MAXP 48
#define MAXDOC (256L << 20)

/* ---------------- small things */
typedef struct { char *b; size_t n, cap; } buf;
static void put(buf *x, const void *p, size_t n)
{
	if (x->n + n + 1 > x->cap) { x->cap = (x->n + n) * 2 + 4096; x->b = realloc(x->b, x->cap); if (!x->b) { fputs("out of memory\n", stderr); exit(1); } }
	memcpy(x->b + x->n, p, n); x->n += n; x->b[x->n] = 0;
}
static void putf(buf *x, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void putf(buf *x, const char *fmt, ...)
{
	char t[4096]; va_list a; va_start(a, fmt); int k = vsnprintf(t, sizeof t, fmt, a); va_end(a);
	if (k > 0) put(x, t, (size_t)k < sizeof t ? (size_t)k : sizeof t - 1);
}
static void pjs(buf *x, const char *s)
{
	put(x, "\"", 1);
	for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
		if (*p == '"' || *p == '\\') { char e[2] = {'\\', *p}; put(x, e, 2); }
		else if (*p < 0x20) putf(x, "\\u%04x", *p);
		else put(x, p, 1);
	}
	put(x, "\"", 1);
}
static void logm(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void logm(const char *fmt, ...)
{
	char t[600]; va_list a; va_start(a, fmt); vsnprintf(t, sizeof t, fmt, a); va_end(a);
	time_t now = time(NULL); struct tm tm; gmtime_r(&now, &tm); char ts[32]; strftime(ts, sizeof ts, "%Y-%m-%dT%H:%M:%SZ", &tm);
	fprintf(stderr, "%s %s\n", ts, t); fflush(stderr);
}
/* text from outside, made safe to keep and show: no control characters, at most n-1 bytes, UTF-8 not cut in half */
static void clean(char *o, size_t n, const char *s, size_t sl)
{
	size_t k = 0; for (size_t i = 0; i < sl && s[i] && k + 1 < n; i++) o[k++] = (unsigned char)s[i] < 32 || s[i] == 127 ? ' ' : s[i];
	while (k && ((unsigned char)o[k - 1] & 0xC0) == 0x80) k--; if (k && (unsigned char)o[k - 1] >= 0xC0) k--;
	o[k] = 0;
}
static int uri_ok(const char *u)
{
	if (!u || strlen(u) > 290 || (strncmp(u, "ipp://", 6) && strncmp(u, "ipps://", 7) && strncmp(u, "usb://", 6))) return 0;
	if (!strncmp(u, "usb://", 6) && (strstr(u, "..") || strchr(u + 6, ':'))) return 0;
	for (; *u; u++) if (!isalnum((unsigned char)*u) && !strchr(":/._-~%[]", *u)) return 0;
	return 1;
}
static int word_ok(const char *s, size_t max) { if (!s || !*s || strlen(s) > max) return 0; for (; *s; s++) if (!isalnum((unsigned char)*s) && !strchr("-_.", *s)) return 0; return 1; }

/* ---------------- printers */
typedef struct { char q[48], name[160], uri[300], model[160], loc[160]; int usb; time_t seen; } prn;
static prn P[MAXP]; static int NP;
static time_t FIND_T; static pid_t FIND_PID; static int FIND_FD = -1; static buf FIND_OUT;
static char DEFAULT_URI[300]; static time_t START;
static prn *p_by_q(const char *q) { for (int i = 0; i < NP; i++) if (!strcmp(P[i].q, q)) return &P[i]; return NULL; }
static prn *p_by_uri(const char *u) { for (int i = 0; i < NP; i++) if (!strcmp(P[i].uri, u)) return &P[i]; return NULL; }
/* a queue's name, as CUPS's: letters, digits, "-" and "_", from the printer's own name; unique */
static void qname(const char *name, char *o, size_t n)
{
	size_t k = 0; int us = 0;
	for (const char *c = name; *c && k + 4 < n && k < 40; c++) {
		if (isalnum((unsigned char)*c) || *c == '-') { o[k++] = *c; us = 0; }
		else if (!us && k) { o[k++] = '_'; us = 1; }
	}
	while (k && o[k - 1] == '_') k--; if (!k) { snprintf(o, n, "Printer"); k = 7; } o[k] = 0;
	char base[48]; snprintf(base, sizeof base, "%s", o);
	for (int i = 2; p_by_q(o) && i < 99; i++) snprintf(o, n, "%.40s_%d", base, i);
}
static prn *p_add(const char *name, const char *uri, const char *model, const char *loc)
{
	if (!uri_ok(uri)) return NULL;
	prn *p = p_by_uri(uri);
	if (!p) { if (NP >= MAXP) return NULL; p = &P[NP]; memset(p, 0, sizeof *p); char nm[160]; clean(nm, sizeof nm, name && *name ? name : "Printer", 400); qname(nm, p->q, sizeof p->q); NP++; snprintf(p->uri, sizeof p->uri, "%s", uri); }
	if (name && *name) clean(p->name, sizeof p->name, name, 400);
	if (!*p->name) snprintf(p->name, sizeof p->name, "%s", p->q);
	if (model) clean(p->model, sizeof p->model, model, 400);
	if (loc) clean(p->loc, sizeof p->loc, loc, 400);
	p->usb = !strncmp(uri, "usb://", 6) || !strncmp(p->name, "USB: ", 5);
	p->seen = time(NULL); return p;
}
/* the printers the desktop printed to by address: "URI<TAB>NAME" lines, kept in the queue's folder */
static void known_load(void)
{
	FILE *f = fopen(KNOWN, "r"); if (!f) return; char l[600];
	while (fgets(l, sizeof l, f)) { l[strcspn(l, "\r\n")] = 0; char *t = strchr(l, '\t'); if (!t) continue; *t = 0; p_add(t + 1, l, NULL, NULL); }
	fclose(f);
}
static void known_add(const char *uri, const char *name)
{
	if (!strncmp(uri, "usb://", 6) || strstr(uri, "127.0.0.1")) return;   /* (USB printers come and go with their plug) */
	FILE *f = fopen(KNOWN, "r"); char l[600]; int n = 0, had = 0; char *keep[64];
	if (f) { while (fgets(l, sizeof l, f) && n < 63) { char u[300]; snprintf(u, sizeof u, "%.*s", (int)strcspn(l, "\t"), l); if (!strcmp(u, uri)) { had = 1; continue; } keep[n++] = strdup(l); } fclose(f); }
	(void)had; char tmp[400]; snprintf(tmp, sizeof tmp, "%s.new", KNOWN); f = fopen(tmp, "w");
	if (f) { char nm[160]; clean(nm, sizeof nm, name, 400); fprintf(f, "%s\t%s\n", uri, nm); for (int i = 0; i < n && i < 31; i++) fputs(keep[i], f); fclose(f); rename(tmp, KNOWN); }
	for (int i = 0; i < n; i++) free(keep[i]);
}
/* a string value of key in a flat JSON object starting at p (rudolf-print's answers: their own escaping) */
static int jget(const char *p, const char *end, const char *key, char *o, size_t n)
{
	char k[64]; snprintf(k, sizeof k, "\"%s\":\"", key); o[0] = 0;
	const char *s = memmem(p, end - p, k, strlen(k)); if (!s) return 0; s += strlen(k);
	size_t j = 0; for (; s < end && *s != '"'; s++) {
		if (*s == '\\' && s + 1 < end) { s++; if (*s == 'u' && s + 4 < end) { s += 4; if (j + 1 < n) o[j++] = '?'; continue; } }
		if (j + 1 < n) o[j++] = *s;
	}
	o[j] = 0; return 1;
}
static void find_parse(const char *t, size_t n)
{
	const char *end = t + n, *a = memchr(t, '[', n); if (!a) return;
	for (const char *o = a; (o = memchr(o, '{', end - o)); ) {
		const char *c = memchr(o, '}', end - o); if (!c) break;
		char name[200], uri[300], model[200], loc[200];
		if (jget(o, c, "uri", uri, sizeof uri)) { jget(o, c, "name", name, sizeof name); jget(o, c, "model", model, sizeof model); jget(o, c, "location", loc, sizeof loc); p_add(name, uri, model, loc); }
		o = c + 1;
	}
}
/* rudolf-print find, in the background (its multicast questions take about 3 s); at start and when asked after a minute */
static void find_start(void)
{
	if (FIND_PID) return;
	int pf[2]; if (pipe2(pf, O_CLOEXEC)) return;
	pid_t pid = fork(); if (pid < 0) { close(pf[0]); close(pf[1]); return; }
	if (!pid) { dup2(pf[1], 1); int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 0); dup2(nul, 2); } execl(RPRINT, "rudolf-print", "find", (char *)NULL); _exit(127); }
	close(pf[1]); FIND_FD = pf[0]; fcntl(FIND_FD, F_SETFL, O_NONBLOCK); FIND_PID = pid; FIND_OUT.n = 0; FIND_T = time(NULL);
}
static void find_read(int done)
{
	if (FIND_FD < 0) return;
	char b[8192]; ssize_t k; while ((k = read(FIND_FD, b, sizeof b)) > 0) put(&FIND_OUT, b, k);
	if (k == 0 || done) {
		close(FIND_FD); FIND_FD = -1;
		if (FIND_OUT.n) {
			/* the USB printers gone since are dropped (a network printer stays: it may be asleep) */
			int w = 0; for (int i = 0; i < NP; i++) { if (P[i].usb && P[i].seen < FIND_T) continue; if (w != i) P[w] = P[i]; w++; } NP = w;
			find_parse(FIND_OUT.b, FIND_OUT.n);
		}
	}
}
/* wait for a search under way (a client's first question: lpstat -p, a print dialog), at most 5 s */
static void find_wait(void)
{
	if (!FIND_PID && NP == 0 && time(NULL) - FIND_T > 5) find_start();
	for (int i = 0; i < 50 && FIND_PID; i++) {
		struct pollfd pf = {FIND_FD, POLLIN, 0}; if (FIND_FD >= 0) poll(&pf, 1, 100); else usleep(100000);
		find_read(0); int st; if (waitpid(FIND_PID, &st, WNOHANG) == FIND_PID) { FIND_PID = 0; find_read(1); }
	}
}
static void find_maybe(void) { if (time(NULL) - FIND_T > 60) find_start(); }

/* ---------------- jobs */
enum { J_PENDING = 3, J_HELD = 4, J_PROCESSING = 5, J_CANCELED = 7, J_ABORTED = 8, J_COMPLETED = 9 };
typedef struct {
	int id, state; uid_t uid; char user[64], title[160], uri[300], pname[160], fmt[48], opts[400], msg[300];
	time_t made, started, done, retry; pid_t pid; int fd; buf out; long size; int from_desktop, tries;
} job;
static job J[MAXJ]; static int NJ, NEXTID = 1;
static job *j_by_id(int id) { for (int i = 0; i < NJ; i++) if (J[i].id == id) return &J[i]; return NULL; }
static void docpath(const job *j, char *o, size_t n) { snprintf(o, n, "%s/%d.doc", SPOOL, j->id); }
static int j_active(int s) { return s == J_PENDING || s == J_HELD || s == J_PROCESSING; }
static void j_tidy(void)
{
	/* finished jobs are kept an hour (and at most 50 of them), so the queue can say what became of them */
	time_t now = time(NULL); int fin = 0; for (int i = 0; i < NJ; i++) if (!j_active(J[i].state)) fin++;
	int w = 0;
	for (int i = 0; i < NJ; i++) {
		job *j = &J[i];
		if (j->state == J_HELD && now - j->made > 300) { j->state = J_ABORTED; j->done = now; snprintf(j->msg, sizeof j->msg, "Its document never came."); char d[400]; docpath(j, d, sizeof d); unlink(d); }
		if (!j_active(j->state) && (now - j->done > 3600 || fin > 50)) { fin--; free(j->out.b); continue; }
		if (w != i) J[w] = J[i]; w++;
	}
	NJ = w;
}
static job *j_new(uid_t uid, const char *user, const char *title, const char *uri, const char *pname)
{
	j_tidy(); if (NJ >= MAXJ) return NULL;
	job *j = &J[NJ++]; memset(j, 0, sizeof *j); j->id = NEXTID++; j->state = J_HELD; j->uid = uid; j->fd = -1; j->made = time(NULL);
	clean(j->user, sizeof j->user, user, 200); clean(j->title, sizeof j->title, title && *title ? title : "Document", 600);
	snprintf(j->uri, sizeof j->uri, "%s", uri); clean(j->pname, sizeof j->pname, pname, 400); snprintf(j->fmt, sizeof j->fmt, "application/pdf");
	return j;
}
static const char *j_word(int s) { return s == J_PENDING || s == J_HELD ? "waiting" : s == J_PROCESSING ? "printing" : s == J_CANCELED ? "canceled" : s == J_ABORTED ? "failed" : "sent"; }

/* ---- the job's own process: what the printer takes, the pages made into raster when it takes no PDF, rudolf-print send */
static int run_cap(char *const argv[], buf *o, int secs)
{
	int pf[2]; if (pipe2(pf, O_CLOEXEC)) return -1;
	pid_t pid = fork(); if (pid < 0) return -1;
	if (!pid) { dup2(pf[1], 1); int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 0); dup2(nul, 2); } alarm(secs); execv(argv[0], argv); _exit(127); }
	close(pf[1]); char b[8192]; ssize_t k; while ((k = read(pf[0], b, sizeof b)) > 0) put(o, b, k); close(pf[0]);
	int st = 0; waitpid(pid, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}
static void child_fail(const char *m) { buf o = {0}; put(&o, "{\"ok\":false,\"error\":", 20); pjs(&o, m); put(&o, "}\n", 2); if (write(1, o.b, o.n)) {} _exit(1); }
/* PackBits-like runs of one line (PWG 5102.4 and Apple raster use the same coding) */
static void rle_line(buf *o, const unsigned char *px, int w, int bpp)
{
	int x = 0;
	while (x < w) {
		const unsigned char *p = px + x * bpp; int n = 1;
		while (x + n < w && n < 128 && !memcmp(p, px + (x + n) * bpp, bpp)) n++;
		if (n > 1) { unsigned char c = n - 1; put(o, &c, 1); put(o, p, bpp); x += n; continue; }
		int m = 1; while (x + m < w && m < 128 && !(x + m + 1 < w && !memcmp(px + (x + m) * bpp, px + (x + m + 1) * bpp, bpp))) m++;
		if (m == 1) { unsigned char c = 0; put(o, &c, 1); put(o, p, bpp); } else { unsigned char c = 257 - m; put(o, &c, 1); put(o, p, m * bpp); }
		x += m;
	}
}
static void be32(unsigned char *h, int off, uint32_t v) { h[off] = v >> 24; h[off + 1] = v >> 16; h[off + 2] = v >> 8; h[off + 3] = v; }
/* one page (a PNG from pdftocairo: 8 bits, grey, RGB or RGBA, not interlaced) read into rows of grey or RGB */
static unsigned char *png_read(const char *path, int *W, int *H, int *BPP)
{
	FILE *f = fopen(path, "rb"); if (!f) return NULL;
	buf raw = {0}; char b[65536]; size_t k; while ((k = fread(b, 1, sizeof b, f)) > 0) put(&raw, b, k); fclose(f);
	const unsigned char *p = (const unsigned char *)raw.b; size_t n = raw.n, i = 8; buf z = {0}; int w = 0, h = 0, ct = -1, depth = 0, il = 0;
	if (n < 8 || memcmp(p, "\x89PNG\r\n\x1a\n", 8)) { free(raw.b); return NULL; }
	while (i + 12 <= n) {
		uint32_t len = (uint32_t)p[i] << 24 | p[i + 1] << 16 | p[i + 2] << 8 | p[i + 3]; const unsigned char *t = p + i + 4, *d = p + i + 8;
		if (len > n - i - 12) break;
		if (!memcmp(t, "IHDR", 4) && len >= 13) { w = (int)((uint32_t)d[0] << 24 | d[1] << 16 | d[2] << 8 | d[3]); h = (int)((uint32_t)d[4] << 24 | d[5] << 16 | d[6] << 8 | d[7]); depth = d[8]; ct = d[9]; il = d[12]; }
		else if (!memcmp(t, "IDAT", 4)) put(&z, d, len);
		else if (!memcmp(t, "IEND", 4)) break;
		i += 12 + len;
	}
	free(raw.b);
	int ch = ct == 0 ? 1 : ct == 2 ? 3 : ct == 6 ? 4 : ct == 4 ? 2 : 0;
	if (w <= 0 || h <= 0 || w > 20000 || h > 30000 || depth != 8 || !ch || il || !z.n) { free(z.b); return NULL; }
	size_t stride = (size_t)w * ch, want = (stride + 1) * h; unsigned char *u = malloc(want); if (!u) { free(z.b); return NULL; }
	z_stream zs = {0}; zs.next_in = (unsigned char *)z.b; zs.avail_in = z.n; zs.next_out = u; zs.avail_out = want;
	int zr = inflateInit(&zs) == Z_OK ? inflate(&zs, Z_FINISH) : Z_DATA_ERROR; inflateEnd(&zs); free(z.b);
	if ((zr != Z_STREAM_END && zr != Z_OK) || zs.total_out != want) { free(u); return NULL; }
	int bpp = ch <= 2 ? 1 : 3; unsigned char *out = malloc((size_t)w * h * bpp), *prev = calloc(stride, 1), *cur = malloc(stride);
	if (!out || !prev || !cur) { free(u); free(out); free(prev); free(cur); return NULL; }
	for (int y = 0; y < h; y++) {
		const unsigned char *r = u + y * (stride + 1); int ft = r[0]; memcpy(cur, r + 1, stride);
		for (size_t x = 0; x < stride; x++) {
			int a = x >= (size_t)ch ? cur[x - ch] : 0, up = prev[x], c = x >= (size_t)ch ? prev[x - ch] : 0;
			if (ft == 1) cur[x] += a; else if (ft == 2) cur[x] += up; else if (ft == 3) cur[x] += (a + up) / 2;
			else if (ft == 4) { int pp = a + up - c, pa = abs(pp - a), pb = abs(pp - up), pc = abs(pp - c); cur[x] += pa <= pb && pa <= pc ? a : pb <= pc ? up : c; }
		}
		unsigned char *o = out + (size_t)y * w * bpp;
		for (int x = 0; x < w; x++) {
			const unsigned char *s = cur + (size_t)x * ch;
			if (bpp == 1) o[x] = s[0];
			else if (ch == 4) { int al = s[3]; for (int q = 0; q < 3; q++) o[x * 3 + q] = (unsigned char)((s[q] * al + 255 * (255 - al)) / 255); }
			else memcpy(o + x * 3, s, 3);
		}
		unsigned char *t = prev; prev = cur; cur = t;
	}
	free(u); free(prev); free(cur); *W = w; *H = h; *BPP = bpp; return out;
}
/* one page appended to the raster stream (PWG 5102.4 or Apple raster: a header, then each line run-length coded,
   with how many times it repeats) */
static int raster_page(FILE *out, const char *png, int urf, int dpi, int total, const char *media, int gray)
{
	int w, h, bpp; unsigned char *px = png_read(png, &w, &h, &bpp); if (!px) return -1;
	if (gray && bpp == 3) { for (size_t i = 0; i < (size_t)w * h; i++) px[i] = (unsigned char)((px[i * 3] * 77 + px[i * 3 + 1] * 150 + px[i * 3 + 2] * 29) >> 8); bpp = 1; }
	if (urf) { unsigned char hd[32] = {0}; hd[0] = bpp * 8; hd[1] = bpp == 3 ? 1 : 0; hd[2] = 1; hd[3] = 4; be32(hd, 12, w); be32(hd, 16, h); be32(hd, 20, dpi); fwrite(hd, 1, 32, out); }
	else {
		unsigned char hd[1796] = {0}; memcpy(hd, "PwgRaster", 9); be32(hd, 276, dpi); be32(hd, 280, dpi); be32(hd, 340, 1);
		be32(hd, 352, (uint32_t)(w * 72.0 / dpi + 0.5)); be32(hd, 356, (uint32_t)(h * 72.0 / dpi + 0.5));
		be32(hd, 372, w); be32(hd, 376, h); be32(hd, 384, 8); be32(hd, 388, bpp * 8); be32(hd, 392, w * bpp); be32(hd, 400, bpp == 3 ? 19 : 18); be32(hd, 420, bpp);
		be32(hd, 452, total); be32(hd, 456, 1); be32(hd, 460, 1); be32(hd, 472, w); be32(hd, 476, h); be32(hd, 480, 0xffffff); be32(hd, 484, 4);
		if (media) snprintf((char *)hd + 1732, 64, "%.63s", media);
		fwrite(hd, 1, sizeof hd, out);
	}
	size_t row = (size_t)w * bpp; buf o = {0};
	for (int y = 0; y < h; ) {
		int rep = 0; while (rep < 255 && y + rep + 1 < h && !memcmp(px + y * row, px + (y + rep + 1) * row, row)) rep++;
		unsigned char c = rep; o.n = 0; put(&o, &c, 1); rle_line(&o, px + y * row, w, bpp); fwrite(o.b, 1, o.n, out);
		y += rep + 1;
	}
	free(o.b); free(px); return 0;
}
static int has_fmt(const char *info, const char *f) { char k[64]; snprintf(k, sizeof k, "\"%s\"", f); const char *s = strstr(info, "\"formats\":["); if (!s) return 0; const char *e = strchr(s, ']'); const char *m = strstr(s, k); return m && e && m < e; }
static int list_has(const char *info, const char *list, const char *v) { char k[64], l[64]; snprintf(k, sizeof k, "\"%s\"", v); snprintf(l, sizeof l, "\"%s\":[", list); const char *s = strstr(info, l); if (!s) return 0; const char *e = strchr(s, ']'); const char *m = strstr(s, k); return m && e && m < e; }
static int list_int(const char *info, const char *list, int want)
{
	char l[64]; snprintf(l, sizeof l, "\"%s\":[", list); const char *s = strstr(info, l); if (!s) return 0; s += strlen(l); int best = 0;
	while (*s && *s != ']') { int v = atoi(s); if (v == want) return want; if (v >= 300 && (!best || v < best)) best = v; if (!best || (best < 300 && v > best)) best = v; while (*s && *s != ',' && *s != ']') s++; if (*s == ',') s++; }
	return best;
}
static void job_child(job *j)
{
	char doc[400], tf[400]; docpath(j, doc, sizeof doc); snprintf(tf, sizeof tf, "%s/%d.title", SPOOL, j->id);
	FILE *f = fopen(tf, "w"); if (f) { fprintf(f, "%s\n", j->title); fclose(f); }
	const char *fmt = j->fmt; char rast[400] = "";
	if (!strcmp(fmt, "application/pdf") && strncmp(j->uri, "usb://", 6)) {
		buf info = {0}; char *ia[] = {(char *)RPRINT, "info", j->uri, NULL}; run_cap(ia, &info, 30);
		if (!info.b || !strstr(info.b, "\"ok\":true")) { char m[300] = "The printer does not answer."; if (info.b) jget(info.b, info.b + info.n, "error", m, sizeof m); child_fail(m); }
		if (!has_fmt(info.b, "application/pdf")) {
			int urf = !has_fmt(info.b, "image/pwg-raster") && has_fmt(info.b, "image/urf");
			if (!urf && !has_fmt(info.b, "image/pwg-raster")) child_fail("This printer needs a driver: Rudolf OS prints to IPP Everywhere and AirPrint printers.");
			char pc[400]; snprintf(pc, sizeof pc, "%s/pdftocairo", POPPLER); if (access(pc, X_OK)) snprintf(pc, sizeof pc, "/usr/bin/pdftocairo");
			if (access(pc, X_OK)) child_fail("This printer takes pictures of the pages, not PDF: printing a Linux app's document to it needs Poppler, which comes with the USB printers extension (Settings › Update and recovery › System extensions).");
			int dpi = urf ? 300 : list_int(info.b, "pwgResolutions", 300); if (dpi <= 0 || dpi > 1200) dpi = 300;
			int color = strstr(j->opts, "color=monochrome") ? 0 : urf ? !!strstr(info.b, "SRGB24") : list_has(info.b, "pwgTypes", "srgb_8");
			if (!color && !urf && !list_has(info.b, "pwgTypes", "sgray_8")) color = 1;
			char pre[400], r[16]; snprintf(pre, sizeof pre, "%s/%d.page", SPOOL, j->id); snprintf(r, sizeof r, "%d", dpi);
			char *ca[] = {pc, "-png", "-r", r, doc, pre, NULL, NULL}; if (!color) { ca[1] = "-gray"; ca[2] = "-png"; ca[3] = "-r"; ca[4] = r; ca[5] = doc; ca[6] = pre; }
			buf ig = {0}; if (run_cap(ca, &ig, 300)) child_fail("The pages could not be prepared for the printer.");
			free(ig.b);
			/* the pages pdftocairo wrote: PRE-1.png ... (numbered with as many digits as the last page needs) */
			char **pg = NULL; int np = 0; DIR *d = opendir(SPOOL); struct dirent *e; char pfx[64]; snprintf(pfx, sizeof pfx, "%d.page-", j->id);
			while (d && (e = readdir(d))) if (!strncmp(e->d_name, pfx, strlen(pfx))) { pg = realloc(pg, (np + 1) * sizeof *pg); pg[np++] = strdup(e->d_name); }
			if (d) closedir(d);
			for (int a = 0; a < np; a++) for (int b = a + 1; b < np; b++) if (atoi(pg[b] + strlen(pfx)) < atoi(pg[a] + strlen(pfx))) { char *t = pg[a]; pg[a] = pg[b]; pg[b] = t; }
			if (!np) child_fail("The document has no pages.");
			snprintf(rast, sizeof rast, "%s/%d.raster", SPOOL, j->id); FILE *o = fopen(rast, "wb"); if (!o) child_fail("No room for the print job.");
			if (urf) { unsigned char h[12] = {'U', 'N', 'I', 'R', 'A', 'S', 'T', 0}; be32(h, 8, np); fwrite(h, 1, 12, o); } else fwrite("RaS2", 1, 4, o);
			const char *mo = strstr(j->opts, "media="); char media[64] = ""; if (mo) sscanf(mo + 6, "%63[A-Za-z0-9_.-]", media);
			for (int a = 0; a < np; a++) { char p[500]; snprintf(p, sizeof p, "%s/%s", SPOOL, pg[a]); if (raster_page(o, p, urf, dpi, np, *media ? media : NULL, !color)) { fclose(o); child_fail("A page could not be prepared for the printer."); } unlink(p); free(pg[a]); }
			free(pg); fclose(o); fmt = urf ? "image/urf" : "image/pwg-raster";
		}
		free(info.b);
	}
	/* rudolf-print send URI FILE FORMAT options... title-file= */
	char *av[16]; int n = 0; av[n++] = (char *)RPRINT; av[n++] = "send"; av[n++] = j->uri; av[n++] = *rast ? rast : doc; av[n++] = (char *)fmt;
	char ob[400]; snprintf(ob, sizeof ob, "%s", j->opts);
	for (char *s = strtok(ob, " "); s && n < 13; s = strtok(NULL, " ")) { if (*rast && !strncmp(s, "pages=", 6)) continue; av[n++] = s; }
	char tfa[420]; snprintf(tfa, sizeof tfa, "title-file=%s", tf); av[n++] = tfa; av[n] = NULL;
	int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 0); dup2(nul, 2); }
	execv(RPRINT, av); child_fail("rudolf-print could not be started.");
}
static void j_start(job *j)
{
	int pf[2]; if (pipe2(pf, O_CLOEXEC)) return;
	pid_t pid = fork(); if (pid < 0) { close(pf[0]); close(pf[1]); return; }
	if (!pid) { setpgid(0, 0); signal(SIGTERM, SIG_DFL); signal(SIGCHLD, SIG_DFL); dup2(pf[1], 1); job_child(j); _exit(1); }
	setpgid(pid, pid); close(pf[1]); j->fd = pf[0]; fcntl(j->fd, F_SETFL, O_NONBLOCK); j->pid = pid; j->state = J_PROCESSING; j->started = time(NULL); j->out.n = 0;
	logm("job %d (%s, uid %u) to %s: started", j->id, j->user, (unsigned)j->uid, j->uri);
}
static void j_files_gone(const job *j)
{
	char p[400]; docpath(j, p, sizeof p); unlink(p); snprintf(p, sizeof p, "%s/%d.title", SPOOL, j->id); unlink(p); snprintf(p, sizeof p, "%s/%d.raster", SPOOL, j->id); unlink(p);
	DIR *d = opendir(SPOOL); struct dirent *e; char pfx[64]; snprintf(pfx, sizeof pfx, "%d.page-", j->id);
	while (d && (e = readdir(d))) if (!strncmp(e->d_name, pfx, strlen(pfx))) { snprintf(p, sizeof p, "%s/%s", SPOOL, e->d_name); unlink(p); }
	if (d) closedir(d);
}
static void j_done(job *j, int status)
{
	if (j->fd >= 0) { char b[4096]; ssize_t k; while ((k = read(j->fd, b, sizeof b)) > 0) put(&j->out, b, k); close(j->fd); j->fd = -1; }
	j->pid = 0; j->done = time(NULL);
	if (j->state == J_CANCELED) { j_files_gone(j); return; }
	char m[300] = "";
	/* (a printer that is busy with another computer's job, or warming up: asked again every 5 s, for up to 10 minutes) */
	if (j->out.b && strstr(j->out.b, "busy or offline") && ++j->tries < 120) { j->state = J_PENDING; j->retry = time(NULL) + 5; j->done = 0; snprintf(j->msg, sizeof j->msg, "The printer is busy: trying again."); return; }
	if (j->out.b && strstr(j->out.b, "\"ok\":true")) { j->state = J_COMPLETED; jget(j->out.b, j->out.b + j->out.n, "message", m, sizeof m); if (!*m) snprintf(m, sizeof m, "Sent to the printer."); }
	else { j->state = J_ABORTED; if (j->out.b) jget(j->out.b, j->out.b + j->out.n, "error", m, sizeof m); if (!*m) snprintf(m, sizeof m, "Not printed (rudolf-print ended with %d).", status); }
	clean(j->msg, sizeof j->msg, m, sizeof m); j_files_gone(j);
	logm("job %d: %s (%s)", j->id, j_word(j->state), j->msg);
}
/* the next job for each printer that is not busy */
static void j_schedule(void)
{
	for (int i = 0; i < NJ; i++) {
		if (J[i].state != J_PENDING || J[i].retry > time(NULL)) continue; int busy = 0;
		for (int k = 0; k < NJ; k++) if (J[k].state == J_PROCESSING && !strcmp(J[k].uri, J[i].uri)) busy = 1;
		if (!busy) j_start(&J[i]);
	}
}
static int j_cancel(job *j)
{
	if (!j_active(j->state)) return 0;
	if (j->state == J_PROCESSING && j->pid > 0) kill(-j->pid, SIGTERM);
	int was = j->state; j->state = J_CANCELED; j->done = time(NULL); snprintf(j->msg, sizeof j->msg, "Canceled.");
	if (was != J_PROCESSING) j_files_gone(j);
	logm("job %d: canceled", j->id); return 1;
}

/* ---------------- who is asking */
typedef struct { uid_t uid; int root, as; char user[64]; } who;
static void who_name(uid_t u, char *o, size_t n) { struct passwd *pw = getpwuid(u); if (pw) snprintf(o, n, "%s", pw->pw_name); else snprintf(o, n, "%u", (unsigned)u); }
/* may this one cancel that job: its own user's (root as the user it said; root alone: any) */
static int may(const who *w, const job *j) { return w->root && !w->as ? 1 : w->uid == j->uid; }
static int mine(const who *w, const job *j) { return w->root && !w->as ? 0 : w->uid == j->uid; }

/* ---------------- IPP (the part CUPS's clients use) */
enum { T_OP = 1, T_JOB = 2, T_END = 3, T_PRINTER = 4, T_UNSUP = 5, T_INT = 0x21, T_BOOL = 0x22, T_ENUM = 0x23, T_DATE = 0x31, T_RANGE = 0x33,
       T_TEXT = 0x41, T_NAME = 0x42, T_KEYWORD = 0x44, T_URI = 0x45, T_CHARSET = 0x47, T_LANG = 0x48, T_MIME = 0x49 };
typedef struct { int group, tag; char name[64]; char val[400]; int ival, vlen; } ia;
typedef struct { int ver, op, reqid; ia *a; int n; size_t end; } ireq;
static int ipp_parse(const unsigned char *p, size_t n, ireq *r)
{
	memset(r, 0, sizeof *r); if (n < 9) return -1;
	r->ver = p[0] << 8 | p[1]; r->op = p[2] << 8 | p[3]; r->reqid = (int)((uint32_t)p[4] << 24 | p[5] << 16 | p[6] << 8 | p[7]);
	size_t i = 8; int group = 0; char last[64] = "";
	while (i < n) {
		int tag = p[i++];
		if (tag == T_END) { r->end = i; return 0; }
		if (tag < 0x10) { group = tag; continue; }
		if (i + 2 > n) return -1; int nl = p[i] << 8 | p[i + 1]; i += 2; if (i + nl + 2 > n) return -1;
		if (nl) snprintf(last, sizeof last, "%.*s", nl < 63 ? nl : 63, (const char *)p + i); i += nl;
		int vl = p[i] << 8 | p[i + 1]; i += 2; if (i + vl > n) return -1;
		if (r->n >= 400) { i += vl; continue; }
		if (r->n % 32 == 0) r->a = realloc(r->a, (r->n + 32) * sizeof(ia));
		ia *a = &r->a[r->n++]; memset(a, 0, sizeof *a); a->group = group; a->tag = tag; a->vlen = vl; snprintf(a->name, sizeof a->name, "%s", last);
		const unsigned char *v = p + i;
		if ((tag == T_INT || tag == T_ENUM) && vl == 4) a->ival = (int)((uint32_t)v[0] << 24 | v[1] << 16 | v[2] << 8 | v[3]);
		else if (tag == T_BOOL && vl == 1) a->ival = v[0];
		else if (tag == T_RANGE && vl == 8) a->ival = (int)((uint32_t)v[0] << 24 | v[1] << 16 | v[2] << 8 | v[3]);
		else if (tag >= 0x40) { int k = vl < (int)sizeof a->val - 1 ? vl : (int)sizeof a->val - 1; memcpy(a->val, v, k); a->val[k] = 0; }
		if (tag == T_RANGE && vl == 8) snprintf(a->val, sizeof a->val, "%d-%d", a->ival, (int)((uint32_t)v[4] << 24 | v[5] << 16 | v[6] << 8 | v[7]));
		i += vl;
	}
	return -1;
}
static const ia *iget(const ireq *r, int group, const char *name) { for (int i = 0; i < r->n; i++) if ((!group || r->a[i].group == group) && !strcmp(r->a[i].name, name)) return &r->a[i]; return NULL; }
/* requested-attributes: none asked (all), "all", or the names asked; a group ("printer-description", "job-template") counts as all */
static int asked(const ireq *r, const char *name)
{
	int any = 0;
	for (int i = 0; i < r->n; i++) {
		if (strcmp(r->a[i].name, "requested-attributes") || r->a[i].group != T_OP) continue; any = 1;
		const char *v = r->a[i].val; if (!strcmp(v, "all") || !strcmp(v, name) || !strcmp(v, "printer-description") || !strcmp(v, "job-description") || !strcmp(v, "job-template") || !strcmp(v, "job-status")) return 1;
	}
	return !any;
}
static void o16(buf *o, int v) { unsigned char c[2] = {v >> 8, v}; put(o, c, 2); }
static void o8(buf *o, int v) { unsigned char c = v; put(o, &c, 1); }
static void oattr(buf *o, int tag, const char *name, const void *v, int vn) { o8(o, tag); o16(o, name ? (int)strlen(name) : 0); if (name) put(o, name, strlen(name)); o16(o, vn); put(o, v, vn); }
static void os(buf *o, int tag, const char *name, const char *v) { int n = strlen(v); if (n > 1023) n = 1023; oattr(o, tag, name, v, n); }
static void oi(buf *o, int tag, const char *name, int v) { unsigned char c[4] = {v >> 24, v >> 16, v >> 8, v}; oattr(o, tag, name, c, 4); }
static void ob(buf *o, const char *name, int v) { unsigned char c = !!v; oattr(o, T_BOOL, name, &c, 1); }
static void orange(buf *o, const char *name, int a, int b) { unsigned char c[8] = {a >> 24, a >> 16, a >> 8, a, b >> 24, b >> 16, b >> 8, b}; oattr(o, T_RANGE, name, c, 8); }
static void oslist(buf *o, int tag, const char *name, const char *const *v) { for (int i = 0; v[i]; i++) os(o, tag, i ? NULL : name, v[i]); }
static void ohead(buf *o, int status, int reqid)
{
	o16(o, 0x0200); o16(o, status); unsigned char c[4] = {reqid >> 24, reqid >> 16, reqid >> 8, reqid}; put(o, c, 4); o8(o, T_OP);
	os(o, T_CHARSET, "attributes-charset", "utf-8"); os(o, T_LANG, "attributes-natural-language", "en");
}
static void omsg(buf *o, const char *m) { os(o, T_TEXT, "status-message", m); }
static const int OPS[] = {0x0002, 0x0004, 0x0005, 0x0006, 0x0008, 0x0009, 0x000A, 0x000B, 0x0012, 0x0038, 0x0039, 0x4001, 0x4002, 0x4005, 0};
static int queued(const prn *p) { int n = 0; for (int i = 0; i < NJ; i++) if (j_active(J[i].state) && !strcmp(J[i].uri, p->uri)) n++; return n; }
static void printer_attrs(buf *o, const ireq *r, const prn *p)
{
	char u[200]; snprintf(u, sizeof u, "ipp://localhost/printers/%s", p->q);
	static const char *const FMT[] = {"application/pdf", "application/octet-stream", NULL}, *const SIDES[] = {"one-sided", "two-sided-long-edge", "two-sided-short-edge", NULL},
		*const MEDIA[] = {"na_letter_8.5x11in", "iso_a4_210x297mm", "na_legal_8.5x14in", "iso_a5_148x210mm", "na_executive_7.25x10.5in", NULL},
		*const COLOR[] = {"color", "monochrome", NULL}, *const VER[] = {"1.0", "1.1", "2.0", NULL}, *const NONE[] = {"none", NULL}, *const NONE2[] = {"none", "none", NULL};
	int busy = 0; for (int i = 0; i < NJ; i++) if (J[i].state == J_PROCESSING && !strcmp(J[i].uri, p->uri)) busy = 1;
#define A(n) if (asked(r, n))
	A("printer-name") os(o, T_NAME, "printer-name", p->q);
	A("printer-uri-supported") os(o, T_URI, "printer-uri-supported", u);
	A("uri-authentication-supported") os(o, T_KEYWORD, "uri-authentication-supported", "none");
	A("uri-security-supported") os(o, T_KEYWORD, "uri-security-supported", "none");
	A("printer-info") os(o, T_TEXT, "printer-info", p->name);
	A("printer-location") os(o, T_TEXT, "printer-location", p->loc);
	A("printer-make-and-model") os(o, T_TEXT, "printer-make-and-model", *p->model ? p->model : p->usb ? "USB printer" : "IPP Everywhere printer");
	A("printer-state") oi(o, T_ENUM, "printer-state", busy ? 4 : 3);
	A("printer-state-reasons") os(o, T_KEYWORD, "printer-state-reasons", "none");
	A("printer-state-change-time") oi(o, T_INT, "printer-state-change-time", (int)START);
	A("printer-state-message") os(o, T_TEXT, "printer-state-message", busy ? "Printing" : "");
	A("printer-is-accepting-jobs") ob(o, "printer-is-accepting-jobs", 1);
	A("printer-is-shared") ob(o, "printer-is-shared", 0);
	A("printer-is-temporary") ob(o, "printer-is-temporary", 0);
	A("printer-type") oi(o, T_ENUM, "printer-type", 0x4 | 0x10 | 0x40 | 0x80 | 0x200 | (strcmp(DEFAULT_URI, p->uri) ? 0 : 0x20000));
	A("device-uri") os(o, T_URI, "device-uri", p->uri);
	A("printer-up-time") oi(o, T_INT, "printer-up-time", (int)time(NULL));
	A("queued-job-count") oi(o, T_INT, "queued-job-count", queued(p));
	A("document-format-supported") oslist(o, T_MIME, "document-format-supported", FMT);
	A("document-format-default") os(o, T_MIME, "document-format-default", "application/pdf");
	A("copies-supported") orange(o, "copies-supported", 1, 99);
	A("copies-default") oi(o, T_INT, "copies-default", 1);
	A("sides-supported") oslist(o, T_KEYWORD, "sides-supported", SIDES);
	A("sides-default") os(o, T_KEYWORD, "sides-default", "one-sided");
	A("media-supported") oslist(o, T_KEYWORD, "media-supported", MEDIA);
	A("media-default") os(o, T_KEYWORD, "media-default", "na_letter_8.5x11in");
	A("print-color-mode-supported") oslist(o, T_KEYWORD, "print-color-mode-supported", COLOR);
	A("print-color-mode-default") os(o, T_KEYWORD, "print-color-mode-default", "color");
	A("color-supported") ob(o, "color-supported", 1);
	A("page-ranges-supported") ob(o, "page-ranges-supported", 1);
	A("operations-supported") for (int i = 0; OPS[i]; i++) oi(o, T_ENUM, i ? NULL : "operations-supported", OPS[i]);
	A("charset-configured") os(o, T_CHARSET, "charset-configured", "utf-8");
	A("charset-supported") os(o, T_CHARSET, "charset-supported", "utf-8");
	A("natural-language-configured") os(o, T_LANG, "natural-language-configured", "en");
	A("generated-natural-language-supported") os(o, T_LANG, "generated-natural-language-supported", "en");
	A("ipp-versions-supported") oslist(o, T_KEYWORD, "ipp-versions-supported", VER);
	A("pdl-override-supported") os(o, T_KEYWORD, "pdl-override-supported", "not-attempted");
	A("multiple-document-jobs-supported") ob(o, "multiple-document-jobs-supported", 0);
	A("job-sheets-supported") oslist(o, T_NAME, "job-sheets-supported", NONE);
	A("job-sheets-default") oslist(o, T_NAME, "job-sheets-default", NONE2);
	A("compression-supported") oslist(o, T_KEYWORD, "compression-supported", NONE);
	A("job-hold-until-supported") os(o, T_KEYWORD, "job-hold-until-supported", "no-hold");
	A("job-hold-until-default") os(o, T_KEYWORD, "job-hold-until-default", "no-hold");
	A("printer-more-info") os(o, T_URI, "printer-more-info", u);
#undef A
}
static void job_attrs(buf *o, const ireq *r, const job *j, const who *w)
{
	char u[100], pu[200]; snprintf(u, sizeof u, "ipp://localhost/jobs/%d", j->id);
	const prn *p = p_by_uri(j->uri); snprintf(pu, sizeof pu, "ipp://localhost/printers/%s", p ? p->q : "gone");
	int own = mine(w, j) || (w->root && !w->as);
	int st = j->state == J_HELD ? J_PENDING : j->state;
#define A(n) if (asked(r, n))
	A("job-id") oi(o, T_INT, "job-id", j->id);
	A("job-uri") os(o, T_URI, "job-uri", u);
	A("job-printer-uri") os(o, T_URI, "job-printer-uri", pu);
	A("job-name") os(o, T_NAME, "job-name", own ? j->title : "Another account's document");
	A("job-originating-user-name") os(o, T_NAME, "job-originating-user-name", own ? j->user : "another account");
	A("job-state") oi(o, T_ENUM, "job-state", st);
	A("job-state-reasons") os(o, T_KEYWORD, "job-state-reasons", st == J_PENDING ? "none" : st == J_PROCESSING ? "job-printing" : st == J_CANCELED ? "job-canceled-by-user" : st == J_ABORTED ? "aborted-by-system" : "job-completed-successfully");
	A("job-state-message") os(o, T_TEXT, "job-state-message", own ? j->msg : "");
	A("job-k-octets") oi(o, T_INT, "job-k-octets", (int)((j->size + 1023) / 1024));
	A("job-priority") oi(o, T_INT, "job-priority", 50);
	A("time-at-creation") oi(o, T_INT, "time-at-creation", (int)j->made);
	A("time-at-processing") oi(o, T_INT, "time-at-processing", (int)j->started);
	A("time-at-completed") oi(o, T_INT, "time-at-completed", (int)j->done);
	A("job-printer-up-time") oi(o, T_INT, "job-printer-up-time", (int)time(NULL));
	A("document-format") os(o, T_MIME, "document-format", j->fmt);
#undef A
}
/* which printer a request means: printer-uri ipp://.../printers/NAME (or the default for "/"), or the HTTP path */
static prn *req_printer(const ireq *r, const char *path)
{
	const ia *u = iget(r, T_OP, "printer-uri"); const char *s = u ? strstr(u->val, "/printers/") : NULL;
	if (!s && path) s = strstr(path, "/printers/");
	if (s) { char q[64]; snprintf(q, sizeof q, "%.*s", (int)strcspn(s + 10, "/?"), s + 10); return p_by_q(q); }
	find_wait(); prn *d = DEFAULT_URI[0] ? p_by_uri(DEFAULT_URI) : NULL; return d ? d : NP ? &P[0] : NULL;
}
static int req_jobid(const ireq *r)
{
	const ia *a = iget(r, T_OP, "job-id"); if (a) return a->ival;
	a = iget(r, T_OP, "job-uri"); const char *s = a ? strstr(a->val, "/jobs/") : NULL; return s ? atoi(s + 6) : 0;
}
/* the options of a job that rudolf-print sends on (copies, sides, color, media, page range) */
static void job_opts(job *j, const ireq *r)
{
	char o[400] = ""; const ia *a;
	if ((a = iget(r, T_JOB, "copies")) && a->ival >= 2 && a->ival <= 99) snprintf(o + strlen(o), sizeof o - strlen(o), " copies=%d", a->ival);
	if ((a = iget(r, T_JOB, "sides")) && word_ok(a->val, 40)) snprintf(o + strlen(o), sizeof o - strlen(o), " sides=%s", a->val);
	if ((a = iget(r, T_JOB, "print-color-mode")) && word_ok(a->val, 40)) snprintf(o + strlen(o), sizeof o - strlen(o), " color=%s", a->val);
	if ((a = iget(r, T_JOB, "media")) && a->tag == T_KEYWORD && word_ok(a->val, 60)) snprintf(o + strlen(o), sizeof o - strlen(o), " media=%s", a->val);
	if ((a = iget(r, T_JOB, "page-ranges")) && a->tag == T_RANGE) { int p1 = 0, p2 = 0; if (sscanf(a->val, "%d-%d", &p1, &p2) == 2 && p1 > 0 && p2 >= p1) snprintf(o + strlen(o), sizeof o - strlen(o), " pages=%d-%d", p1, p2); }
	snprintf(j->opts, sizeof j->opts, "%s", o + (*o == ' '));
}
/* the document that came with a request (from the body's offset) into the job's spool file; its kind checked */
static int job_doc(job *j, int bodyfd, size_t off, size_t total, const char *fmt, char *err, size_t en)
{
	char p[400]; docpath(j, p, sizeof p); int o = open(p, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
	if (o < 0) { snprintf(err, en, "No room for the print job."); return -1; }
	if (lseek(bodyfd, off, SEEK_SET) < 0) { close(o); return -1; }
	char b[65536]; size_t left = total > off ? total - off : 0, got = 0; unsigned char head[8] = {0};
	while (left) { ssize_t k = read(bodyfd, b, left < sizeof b ? left : sizeof b); if (k <= 0) break; if (got < 8) memcpy(head + got, b, (size_t)k < 8 - got ? (size_t)k : 8 - got); if (write(o, b, k) != k) { close(o); snprintf(err, en, "No room for the print job."); return -1; } left -= k; got += k; }
	close(o); j->size = got;
	if (!got) { snprintf(err, en, "The document is empty."); unlink(p); return -1; }
	/* (PDF only: what CUPS's clients send by themselves when a printer's formats are PDF; PostScript would need Ghostscript) */
	if (memcmp(head, "%PDF", 4) || (fmt && *fmt && strcmp(fmt, "application/pdf") && strcmp(fmt, "application/octet-stream"))) {
		snprintf(err, en, "Rudolf OS's printers take PDF documents%s.", !memcmp(head, "%!", 2) ? " (this one is PostScript)" : ""); unlink(p); return -2; }
	snprintf(j->fmt, sizeof j->fmt, "application/pdf");
	return 0;
}
static void ipp_handle(int bodyfd, size_t total, const unsigned char *hb, size_t hn, const char *path, const who *w, buf *o)
{
	ireq r; ohead(o, 0, 0); o->n = 0;
	if (ipp_parse(hb, hn, &r)) { ohead(o, 0x0400, 0); omsg(o, "The request could not be read."); o8(o, T_END); return; }
	int id = r.reqid; char err[300] = "";
	if ((r.ver >> 8) < 1 || (r.ver >> 8) > 2) { ohead(o, 0x0503, id); o8(o, T_END); free(r.a); return; }
	switch (r.op) {
	case 0x4002: /* CUPS-Get-Printers */
	case 0x4001: /* CUPS-Get-Default */ {
		find_wait(); find_maybe(); ohead(o, NP ? 0 : 0x0406, id); if (!NP) omsg(o, "No printer was found.");
		for (int i = 0; i < NP; i++) {
			if (r.op == 0x4001 && !(DEFAULT_URI[0] ? !strcmp(P[i].uri, DEFAULT_URI) || !p_by_uri(DEFAULT_URI) : 1)) continue;
			o8(o, T_PRINTER); printer_attrs(o, &r, &P[i]); if (r.op == 0x4001) break;
		}
		break; }
	case 0x4005: /* CUPS-Get-Classes: none */ ohead(o, 0x0406, id); break;
	case 0x000B: /* Get-Printer-Attributes */ {
		prn *p = req_printer(&r, path); if (!p) { ohead(o, 0x0406, id); omsg(o, "No such printer."); break; }
		ohead(o, 0, id); o8(o, T_PRINTER); printer_attrs(o, &r, p); break; }
	case 0x0004: /* Validate-Job */ { prn *p = req_printer(&r, path); ohead(o, p ? 0 : 0x0406, id); break; }
	case 0x0002: /* Print-Job */
	case 0x0005: /* Create-Job */ {
		prn *p = req_printer(&r, path); if (!p) { ohead(o, 0x0406, id); omsg(o, "No such printer."); break; }
		const ia *jn = iget(&r, T_OP, "job-name"); job *j = j_new(w->uid, w->user, jn ? jn->val : "Document", p->uri, p->name);
		if (!j) { ohead(o, 0x0509, id); omsg(o, "The queue is full."); break; }
		job_opts(j, &r);
		if (r.op == 0x0002) {
			const ia *df = iget(&r, T_OP, "document-format"); int e = job_doc(j, bodyfd, r.end, total, df ? df->val : "", err, sizeof err);
			if (e) { j->state = J_ABORTED; j->done = time(NULL); snprintf(j->msg, sizeof j->msg, "%s", err); ohead(o, e == -2 ? 0x040A : 0x0500, id); omsg(o, err); break; }
			j->state = J_PENDING;
		}
		if (!DEFAULT_URI[0]) snprintf(DEFAULT_URI, sizeof DEFAULT_URI, "%s", p->uri);
		logm("job %d from %s (uid %u) for %s: %s", j->id, w->user, (unsigned)w->uid, p->q, r.op == 2 ? "queued" : "made, waiting for its document");
		ohead(o, 0, id); o8(o, T_JOB); { ireq all = {0}; job_attrs(o, &all, j, w); } j_schedule(); break; }
	case 0x0006: /* Send-Document */ {
		job *j = j_by_id(req_jobid(&r)); if (!j) { ohead(o, 0x0406, id); omsg(o, "No such job."); break; }
		if (!mine(w, j) && !(w->root && !w->as)) { ohead(o, 0x0401, id); omsg(o, "That is another account's job."); break; }
		if (j->state != J_HELD) { ohead(o, 0x0400, id); omsg(o, "The job has its document already."); break; }
		const ia *df = iget(&r, T_OP, "document-format"); const ia *last = iget(&r, T_OP, "last-document");
		size_t got = total > r.end ? total - r.end : 0;
		if (!got && last && last->ival) { /* (an empty last document: the job ends with what it has) */ j->state = J_ABORTED; j->done = time(NULL); snprintf(j->msg, sizeof j->msg, "The document is empty."); ohead(o, 0, id); break; }
		int e = job_doc(j, bodyfd, r.end, total, df ? df->val : "", err, sizeof err);
		if (e) { j->state = J_ABORTED; j->done = time(NULL); snprintf(j->msg, sizeof j->msg, "%s", err); ohead(o, e == -2 ? 0x040A : 0x0500, id); omsg(o, err); break; }
		j->state = J_PENDING; ohead(o, 0, id); o8(o, T_JOB); { ireq all = {0}; job_attrs(o, &all, j, w); } j_schedule(); break; }
	case 0x000A: /* Get-Jobs */ {
		const ia *wj = iget(&r, T_OP, "which-jobs"), *my = iget(&r, T_OP, "my-jobs"), *lim = iget(&r, T_OP, "limit");
		int comp = wj && (!strcmp(wj->val, "completed") || !strcmp(wj->val, "all")), act = !wj || strcmp(wj->val, "completed"), n = 0;
		prn *p = NULL; { const ia *u = iget(&r, T_OP, "printer-uri"); if (u && strstr(u->val, "/printers/")) p = req_printer(&r, NULL); }
		ohead(o, 0, id);
		for (int i = 0; i < NJ; i++) {
			job *j = &J[i]; int a = j_active(j->state);
			if ((a && !act) || (!a && !comp) || (my && my->ival && !mine(w, j)) || (p && strcmp(j->uri, p->uri))) continue;
			if (lim && lim->ival > 0 && n >= lim->ival) break;
			o8(o, T_JOB); job_attrs(o, &r, j, w); n++;
		}
		break; }
	case 0x0009: /* Get-Job-Attributes */ {
		job *j = j_by_id(req_jobid(&r)); if (!j) { ohead(o, 0x0406, id); omsg(o, "No such job."); break; }
		ohead(o, 0, id); o8(o, T_JOB); job_attrs(o, &r, j, w); break; }
	case 0x0008: /* Cancel-Job */ {
		job *j = j_by_id(req_jobid(&r)); if (!j) { ohead(o, 0x0406, id); omsg(o, "No such job."); break; }
		if (!may(w, j)) { ohead(o, 0x0401, id); omsg(o, "That is another account's job: only its own account can cancel it."); logm("job %d: %s (uid %u) may not cancel it", j->id, w->user, (unsigned)w->uid); break; }
		if (!j_active(j->state)) { ohead(o, 0x0407, id); omsg(o, "The job has finished already."); break; }
		j_cancel(j); ohead(o, 0, id); break; }
	case 0x0012: /* Purge-Jobs */
	case 0x0038: /* Cancel-Jobs */
	case 0x0039: /* Cancel-My-Jobs: each one's own only */ {
		prn *p = NULL; { const ia *u = iget(&r, T_OP, "printer-uri"); if (u && strstr(u->val, "/printers/")) p = req_printer(&r, NULL); }
		for (int i = 0; i < NJ; i++) if (may(w, &J[i]) && (!p || !strcmp(J[i].uri, p->uri))) j_cancel(&J[i]);
		ohead(o, 0, id); break; }
	default: ohead(o, 0x0501, id); omsg(o, "Rudolf OS's print queue does not do that."); break;
	}
	o8(o, T_END); free(r.a);
}

/* ---------------- rudolfd's own requests (JSON) */
static int qp(const char *q, const char *k, char *o, size_t n)
{
	size_t kl = strlen(k); o[0] = 0;
	for (const char *s = q; s && *s; ) {
		if (!strncmp(s, k, kl) && s[kl] == '=') {
			s += kl + 1; size_t j = 0;
			for (; *s && *s != '&' && j + 1 < n; s++) {
				if (*s == '%' && isxdigit((unsigned char)s[1]) && isxdigit((unsigned char)s[2])) { char h[3] = {s[1], s[2], 0}; o[j++] = (char)strtol(h, NULL, 16); s += 2; }
				else o[j++] = *s == '+' ? ' ' : *s;
			}
			o[j] = 0; return 1;
		}
		s = strchr(s, '&'); if (s) s++;
	}
	return 0;
}
static void rudolf_handle(const char *path, const char *query, int bodyfd, size_t total, const who *w, buf *o, int *code)
{
	*code = 200;
	if (!strcmp(path, "/rudolf/jobs") || !strcmp(path, "/rudolf/printers")) {
		int jobs = path[8] == 'j'; if (!jobs) find_wait();
		put(o, "{\"ok\":true,\"printers\":[", 23);
		for (int i = 0; i < NP; i++) { if (i) put(o, ",", 1); put(o, "{\"q\":", 5); pjs(o, P[i].q); put(o, ",\"name\":", 8); pjs(o, P[i].name); put(o, ",\"uri\":", 7); pjs(o, P[i].uri); putf(o, ",\"queued\":%d,\"usb\":%s}", queued(&P[i]), P[i].usb ? "true" : "false"); }
		put(o, "]", 1);
		if (jobs) {
			put(o, ",\"jobs\":[", 9); int first = 1;
			for (int i = NJ - 1; i >= 0; i--) {
				job *j = &J[i]; int own = mine(w, j), see = own || (w->root && !w->as);
				if (!first) put(o, ",", 1); first = 0;
				putf(o, "{\"id\":%d,\"state\":\"%s\",\"mine\":%s,\"name\":", j->id, j_word(j->state), own ? "true" : "false"); pjs(o, see ? j->title : "");
				put(o, ",\"printer\":", 11); pjs(o, j->pname); put(o, ",\"uri\":", 7); pjs(o, j->uri); put(o, ",\"user\":", 8); pjs(o, see ? j->user : "");
				put(o, ",\"message\":", 11); pjs(o, see ? j->msg : ""); putf(o, ",\"made\":%ld,\"done\":%ld,\"size\":%ld,\"cancel\":%s}", (long)j->made, (long)j->done, j->size, may(w, j) && j_active(j->state) ? "true" : "false");
			}
			put(o, "]", 1);
		}
		put(o, "}\n", 2); return;
	}
	if (!strcmp(path, "/rudolf/cancel")) {
		char v[16]; qp(query, "id", v, sizeof v); job *j = j_by_id(atoi(v));
		if (!j) { *code = 404; put(o, "{\"ok\":false,\"error\":\"No such job.\"}\n", 36); return; }
		if (!may(w, j)) { *code = 403; put(o, "{\"ok\":false,\"error\":\"That is another account's job: only its own account can cancel it.\"}\n", 90); logm("job %d: %s (uid %u) may not cancel it", j->id, w->user, (unsigned)w->uid); return; }
		if (!j_cancel(j)) { put(o, "{\"ok\":false,\"error\":\"The job has finished already.\"}\n", 53); *code = 409; return; }
		put(o, "{\"ok\":true,\"message\":\"Canceled.\"}\n", 34); return;
	}
	if (!strcmp(path, "/rudolf/submit")) {
		char uri[300], fmt[48], name[200], title[200], v[80], err[300] = "";
		qp(query, "uri", uri, sizeof uri); qp(query, "format", fmt, sizeof fmt); qp(query, "name", name, sizeof name); qp(query, "title", title, sizeof title);
		if (!uri_ok(uri)) { *code = 400; put(o, "{\"ok\":false,\"error\":\"a printer address looks like ipp://192.168.1.20/ipp/print\"}\n", 81); return; }
		if (strcmp(fmt, "application/pdf") && strcmp(fmt, "image/pwg-raster") && strcmp(fmt, "image/urf")) { *code = 400; put(o, "{\"ok\":false,\"error\":\"unknown document format\"}\n", 47); return; }
		prn *p = p_by_uri(uri); if (!p) { p = p_add(*name ? name : uri, uri, NULL, NULL); if (p && !strncmp(uri, "ipp", 3)) known_add(uri, p->name); }
		job *j = j_new(w->uid, w->user, title, uri, p ? p->name : *name ? name : uri); if (!j) { *code = 503; put(o, "{\"ok\":false,\"error\":\"The queue is full.\"}\n", 41); return; }
		j->from_desktop = 1; char op[400] = "";
		static const char *const K[] = {"copies", "sides", "color", "media", "pages", NULL};
		for (int i = 0; K[i]; i++) if (qp(query, K[i], v, sizeof v) && *v && (i == 4 ? strspn(v, "0123456789-") == strlen(v) : word_ok(v, 60))) snprintf(op + strlen(op), sizeof op - strlen(op), "%s%s=%s", *op ? " " : "", K[i], v);
		snprintf(j->opts, sizeof j->opts, "%s", op);
		/* the desktop's document: a PDF, or raster it made itself (the print dialog's pdf.js) */
		char pth[400]; docpath(j, pth, sizeof pth); int fo = open(pth, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600); long got = 0;
		if (fo >= 0 && lseek(bodyfd, 0, SEEK_SET) == 0) { char b[65536]; ssize_t k; while ((size_t)got < total && (k = read(bodyfd, b, sizeof b)) > 0) { if (write(fo, b, k) != k) { got = -1; break; } got += k; } }
		if (fo >= 0) close(fo);
		if (fo < 0 || got <= 0) { snprintf(err, sizeof err, "No room for the print job (or the document is empty)."); j->state = J_ABORTED; j->done = time(NULL); snprintf(j->msg, sizeof j->msg, "%s", err); unlink(pth); *code = 500; put(o, "{\"ok\":false,\"error\":", 20); pjs(o, err); put(o, "}\n", 2); return; }
		j->size = got; snprintf(j->fmt, sizeof j->fmt, "%s", fmt); j->state = J_PENDING;
		logm("job %d from the desktop (%s) for %s: queued", j->id, w->user, uri);
		j_schedule();
		putf(o, "{\"ok\":true,\"job\":%d,\"queued\":true,\"state\":\"%s\",\"message\":\"In the print queue (Settings › Devices).\"}\n", j->id, j_word(j->state)); return;
	}
	*code = 404; put(o, "{\"ok\":false,\"error\":\"unknown request\"}\n", 39);
}

/* ---------------- HTTP on the socket: one request a connection (Connection: close; CUPS's clients connect again) */
static int rd_line(int fd, char *b, size_t n)
{
	size_t k = 0; while (k + 1 < n) { char c; ssize_t r = read(fd, &c, 1); if (r <= 0) return -1; if (c == '\n') { if (k && b[k - 1] == '\r') k--; b[k] = 0; return (int)k; } b[k++] = c; }
	return -1;
}
static void http_reply(int fd, int code, const char *type, const char *b, size_t n)
{
	char h[300]; int k = snprintf(h, sizeof h, "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n", code,
		code == 200 ? "OK" : code == 400 ? "Bad Request" : code == 403 ? "Forbidden" : code == 404 ? "Not Found" : code == 409 ? "Conflict" : code == 413 ? "Request Entity Too Large" : "Error", type, n);
	if (write(fd, h, k) != k) return; size_t s = 0; while (s < n) { ssize_t w = write(fd, b + s, n - s); if (w <= 0) return; s += w; }
}
/* the request's body into a file of the spool (documents can be large), from Content-Length or chunks */
static long body_in(int fd, int chunked, long len, int out)
{
	char b[65536]; long tot = 0;
	if (!chunked) { while (tot < len) { ssize_t k = read(fd, b, (size_t)(len - tot) < sizeof b ? (size_t)(len - tot) : sizeof b); if (k <= 0) return -1; if (write(out, b, k) != k) return -1; tot += k; } return tot; }
	for (;;) {
		char l[64]; if (rd_line(fd, l, sizeof l) < 0) return -1; long n = strtol(l, NULL, 16); if (n < 0) return -1;
		if (!n) { while (rd_line(fd, l, sizeof l) > 0) {} return tot; }
		if (tot + n > MAXDOC) return -2;
		while (n) { ssize_t k = read(fd, b, (size_t)n < sizeof b ? (size_t)n : sizeof b); if (k <= 0) return -1; if (write(out, b, k) != k) return -1; n -= k; tot += k; }
		if (rd_line(fd, l, sizeof l) < 0) return -1;
	}
}
static void serve_one(int c)
{
	struct timeval tv = {30, 0}; setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv); setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
	struct ucred cr; socklen_t cl = sizeof cr; if (getsockopt(c, SOL_SOCKET, SO_PEERCRED, &cr, &cl)) return;
	who w = {0}; w.uid = cr.uid; w.root = cr.uid == 0; who_name(cr.uid, w.user, sizeof w.user);
	char l[2048], method[16] = "", target[1024] = "", as[64] = ""; long len = 0; int chunked = 0, expect = 0;
	if (rd_line(c, l, sizeof l) <= 0 || sscanf(l, "%15s %1023s", method, target) != 2) return;
	for (int i = 0; i < 60; i++) {
		int k = rd_line(c, l, sizeof l); if (k < 0) return; if (!k) break;
		if (!strncasecmp(l, "Content-Length:", 15)) len = atol(l + 15);
		else if (!strncasecmp(l, "Transfer-Encoding:", 18) && strcasestr(l + 18, "chunked")) chunked = 1;
		else if (!strncasecmp(l, "Expect:", 7) && strcasestr(l + 7, "100-continue")) expect = 1;
		else if (!strncasecmp(l, "X-Rudolf-As:", 12)) { const char *v = l + 12; while (*v == ' ') v++; snprintf(as, sizeof as, "%s", v); }
	}
	/* root says for whom it asks (rudolfd: the signed-in account's user); anyone else is who the kernel says */
	if (w.root && *as) { struct passwd *pw = getpwnam(as); if (!pw) { http_reply(c, 403, "application/json", "{\"ok\":false,\"error\":\"no such user\"}\n", 36); return; } w.uid = pw->pw_uid; w.as = 1; snprintf(w.user, sizeof w.user, "%s", pw->pw_name); }
	char path[1024], query[1024] = ""; snprintf(path, sizeof path, "%s", target); char *qm = strchr(path, '?'); if (qm) { snprintf(query, sizeof query, "%s", qm + 1); *qm = 0; }
	if (!strcmp(method, "GET") || !strcmp(method, "HEAD")) {
		if (!strncmp(path, "/rudolf/", 8)) { buf o = {0}; int code; rudolf_handle(path, query, -1, 0, &w, &o, &code); http_reply(c, code, "application/json", o.b, o.n); free(o.b); return; }
		/* (a printer's PPD: none, as for a driverless queue; CUPS's clients then ask the printer's attributes) */
		http_reply(c, 404, "text/plain", "Not Found\n", 10); return;
	}
	if (strcmp(method, "POST")) { http_reply(c, 400, "text/plain", "Bad Request\n", 12); return; }
	if (!chunked && (len < 0 || len > MAXDOC)) { http_reply(c, 413, "text/plain", "Too large\n", 10); return; }
	if (expect && write(c, "HTTP/1.1 100 Continue\r\n\r\n", 25) != 25) return;
	char bp[400]; snprintf(bp, sizeof bp, "%s/body-XXXXXX", SPOOL); int bf = mkostemp(bp, O_CLOEXEC); if (bf < 0) { http_reply(c, 500, "text/plain", "No room\n", 8); return; }
	unlink(bp); long total = body_in(c, chunked, len, bf);
	if (total < 0) { close(bf); if (total == -2) http_reply(c, 413, "text/plain", "Too large\n", 10); return; }
	buf o = {0}; int code = 200;
	if (!strncmp(path, "/rudolf/", 8)) { rudolf_handle(path, query, bf, total, &w, &o, &code); http_reply(c, code, "application/json", o.b, o.n); }
	else {
		unsigned char hb[65536]; ssize_t hn = pread(bf, hb, sizeof hb, 0);
		ipp_handle(bf, total, hb, hn > 0 ? hn : 0, path, &w, &o); http_reply(c, 200, "application/ipp", o.b, o.n);
	}
	free(o.b); close(bf);
}
static volatile sig_atomic_t QUIT;
static void on_term(int s) { (void)s; QUIT = 1; }
static int cmd_serve(void)
{
	signal(SIGPIPE, SIG_IGN); signal(SIGTERM, on_term); signal(SIGINT, on_term);
	/* the socket's folder and the socket, then the spool, as root; then the queue's own user */
	char dir[300]; snprintf(dir, sizeof dir, "%s", SOCK); char *sl = strrchr(dir, '/'); if (sl) *sl = 0;
	mkdir(dir, 0755); chmod(dir, 0755);
	int ls = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0); struct sockaddr_un sa = {0}; sa.sun_family = AF_UNIX; snprintf(sa.sun_path, sizeof sa.sun_path, "%s", SOCK);
	unlink(SOCK); mode_t um = umask(0); int b = bind(ls, (struct sockaddr *)&sa, sizeof sa); umask(um);
	if (ls < 0 || b || listen(ls, 32)) { logm("cannot open the socket %s: %s", SOCK, strerror(errno)); return 1; }
	chmod(SOCK, 0666);
	mkdir(SPOOL, 0700);
	const char *user = ENVOR("RPQ_USER", "print");
	if (strcmp(user, "-")) {
		struct passwd *pw = getpwnam(user); if (!pw) { logm("no user %s: the queue does not start as root", user); return 1; }
		struct group *lp = getgrnam("lp"); gid_t gs[2] = {pw->pw_gid, lp ? lp->gr_gid : pw->pw_gid};
		if (chown(SPOOL, pw->pw_uid, pw->pw_gid)) {} chmod(SPOOL, 0700); if (chown(dir, pw->pw_uid, pw->pw_gid)) {}   /* (its folder its own: the socket removed when it stops) */
		{ char kd[300]; snprintf(kd, sizeof kd, "%s", KNOWN); char *s = strrchr(kd, '/'); if (s) { *s = 0; mkdir(kd, 0755); } int f = open(KNOWN, O_WRONLY | O_CREAT | O_CLOEXEC, 0644); if (f >= 0) { if (fchown(f, pw->pw_uid, pw->pw_gid)) {} close(f); } char t[320]; snprintf(t, sizeof t, "%s.new", KNOWN); unlink(t); if (s) { if (chown(kd, pw->pw_uid, pw->pw_gid)) {} } }
		if (setgroups(lp ? 2 : 1, gs) || setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) || setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) || !setuid(0)) { logm("could not become %s", user); return 1; }
		prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	}
	{ DIR *d = opendir(SPOOL); struct dirent *e; while (d && (e = readdir(d))) if (e->d_name[0] != '.') { char p[600]; snprintf(p, sizeof p, "%s/%s", SPOOL, e->d_name); unlink(p); } if (d) closedir(d); }
	START = time(NULL); known_load(); find_start();
	logm("the print queue is open on %s (as %s)", SOCK, strcmp(user, "-") ? user : "the user that started it");
	while (!QUIT) {
		struct pollfd pf[2 + MAXJ]; int n = 0; pf[n++] = (struct pollfd){ls, POLLIN, 0}; if (FIND_FD >= 0) pf[n++] = (struct pollfd){FIND_FD, POLLIN, 0};
		poll(pf, n, 500);
		if (pf[0].revents & POLLIN) { int c = accept4(ls, NULL, NULL, SOCK_CLOEXEC); if (c >= 0) { serve_one(c); close(c); } }
		find_read(0);
		for (int i = 0; i < NJ; i++) if (J[i].fd >= 0) { char bb[4096]; ssize_t k; while ((k = read(J[i].fd, bb, sizeof bb)) > 0) put(&J[i].out, bb, k); }
		int st; pid_t p;
		while ((p = waitpid(-1, &st, WNOHANG)) > 0) {
			if (p == FIND_PID) { FIND_PID = 0; find_read(1); continue; }
			for (int i = 0; i < NJ; i++) if (J[i].pid == p) j_done(&J[i], WIFEXITED(st) ? WEXITSTATUS(st) : -1);
		}
		j_schedule();
	}
	for (int i = 0; i < NJ; i++) if (J[i].pid > 0) kill(-J[i].pid, SIGTERM);
	unlink(SOCK); logm("the print queue closed"); return 0;
}

/* ---------------- the client side (rudolfd, the tests) */
static int client(const char *method, const char *target, const char *as, const char *file, buf *out)
{
	int s = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0); struct sockaddr_un sa = {0}; sa.sun_family = AF_UNIX; snprintf(sa.sun_path, sizeof sa.sun_path, "%s", SOCK);
	if (s < 0 || connect(s, (struct sockaddr *)&sa, sizeof sa)) { printf("{\"ok\":false,\"error\":\"The print queue is not running.\"}\n"); return 3; }
	struct stat st; long n = 0; int f = -1; if (file) { f = open(file, O_RDONLY | O_CLOEXEC); if (f < 0 || fstat(f, &st)) { printf("{\"ok\":false,\"error\":\"could not read the document\"}\n"); return 2; } n = st.st_size; }
	char h[1600]; int k = snprintf(h, sizeof h, "%s %s HTTP/1.1\r\nHost: localhost\r\nContent-Length: %ld\r\n%s%s%sConnection: close\r\n\r\n", method, target, n, as ? "X-Rudolf-As: " : "", as ? as : "", as ? "\r\n" : "");
	if (write(s, h, k) != k) return 3;
	if (f >= 0) { char b[65536]; ssize_t r; while ((r = read(f, b, sizeof b)) > 0) if (write(s, b, r) != r) break; close(f); }
	char b[8192]; ssize_t r; buf all = {0}; while ((r = read(s, b, sizeof b)) > 0) put(&all, b, r); close(s);
	char *body = all.b ? strstr(all.b, "\r\n\r\n") : NULL; if (!body) { printf("{\"ok\":false,\"error\":\"The print queue did not answer.\"}\n"); return 3; }
	put(out, body + 4, all.n - (body + 4 - all.b)); free(all.b); return 0;
}
static void enc(buf *o, const char *k, const char *v)
{
	putf(o, "%s%s=", o->n ? "&" : "", k);
	for (const unsigned char *c = (const unsigned char *)v; *c; c++) { if (isalnum(*c) || strchr("-_.~", *c)) put(o, c, 1); else putf(o, "%%%02X", *c); }
}
int main(int argc, char **argv)
{
	SOCK = ENVOR("RPQ_SOCK", "/run/cups/cups.sock"); SPOOL = ENVOR("RPQ_SPOOL", "/run/rudolf-printq/spool");
	RPRINT = ENVOR("RPQ_PRINT", "/usr/bin/rudolf-print"); POPPLER = ENVOR("RPQ_POPPLER", "/usr/lib/rudolf/print"); KNOWN = ENVOR("RPQ_KNOWN", "/run/rudolf-printq/known");
	const char *as = NULL; int n = 0; char *a[24];
	for (int i = 1; i < argc && n < 23; i++) { if (!strcmp(argv[i], "--as") && i + 1 < argc) { as = argv[++i]; continue; } a[n++] = argv[i]; }
	a[n] = NULL; if (as && !word_ok(as, 60)) { printf("{\"ok\":false,\"error\":\"bad user\"}\n"); return 2; }
	if (n >= 1 && !strcmp(a[0], "serve")) return cmd_serve();
	buf o = {0}; int r = -1;
	if (n >= 1 && !strcmp(a[0], "jobs")) r = client("GET", "/rudolf/jobs", as, NULL, &o);
	else if (n >= 1 && !strcmp(a[0], "printers")) r = client("GET", "/rudolf/printers", as, NULL, &o);
	else if (n >= 2 && !strcmp(a[0], "cancel")) { char t[64]; snprintf(t, sizeof t, "/rudolf/cancel?id=%d", atoi(a[1])); r = client("POST", t, as, NULL, &o); }
	else if (n >= 4 && !strcmp(a[0], "submit")) {
		buf q = {0}; enc(&q, "uri", a[1]); enc(&q, "format", a[3]);
		for (int i = 4; i < n; i++) {
			char *eq = strchr(a[i], '='); if (!eq) continue; *eq = 0; const char *v = eq + 1;
			if (!strcmp(a[i], "title-file") || !strcmp(a[i], "name-file")) { char t[200] = ""; FILE *f = fopen(v, "r"); if (f) { if (!fgets(t, sizeof t, f)) t[0] = 0; fclose(f); } t[strcspn(t, "\r\n")] = 0; enc(&q, a[i][0] == 't' ? "title" : "name", t); }
			else enc(&q, a[i], v);
		}
		char t[1500]; snprintf(t, sizeof t, "/rudolf/submit?%s", q.b ? q.b : ""); r = client("POST", t, as, a[2], &o); free(q.b);
	}
	else { fprintf(stderr, "usage: rudolf-printq serve | jobs | printers | cancel ID | submit URI FILE FORMAT [KEY=VALUE...] [--as USER]\n"); return 2; }
	if (o.b) fputs(o.b, stdout);
	return r ? r : o.b && strstr(o.b, "\"ok\":true") ? 0 : 1;
}
