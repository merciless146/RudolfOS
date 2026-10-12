/* rudolf-print: driverless printing for Rudolf OS, without CUPS.
 *
 * Network printers that speak IPP Everywhere / AirPrint (most printers sold since about 2013) accept
 * documents directly: PDF, or PWG / Apple raster that the desktop renders from the PDF. This program
 * finds them on the local network (multicast DNS), asks what they can do and sends jobs.
 *
 *   rudolf-print find                     printers on the network (JSON)
 *   rudolf-print info URI                 what the printer supports (JSON)
 *   rudolf-print send URI FILE FORMAT [copies=N] [sides=KW] [color=KW] [media=KW] [pages=A-B]
 *                     [title-file=PATH]   sends one job; prints {"ok":true,"job":ID,...}
 * URI is ipp://host[:port]/path or ipps://... (printers use self-made certificates on the local network,
 * so ipps is encrypted but not checked against a certificate authority).
 * RP_MDNS=ADDR:PORT sends the discovery query there instead of the multicast group (tests).
 *
 * Copyright (C) 2026 the Rudolf OS authors. SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static void die(const char *fmt, ...)
{
	char m[400]; va_list a; va_start(a, fmt); vsnprintf(m, sizeof m, fmt, a); va_end(a);
	printf("{\"ok\":false,\"error\":\""); for (char *p = m; *p; p++) { if (*p == '"' || *p == '\\') putchar('\\'); putchar(*p); } printf("\"}\n"); exit(1);
}
/* ---------------- JSON output */
static void js(const char *s)
{
	putchar('"');
	for (const unsigned char *p = (const unsigned char *)s; p && *p; p++) {
		if (*p == '"' || *p == '\\') printf("\\%c", *p);
		else if (*p < 0x20) printf("\\u%04x", *p);
		else putchar(*p);
	}
	putchar('"');
}

/* ---------------- growable buffer */
typedef struct { unsigned char *b; size_t n, cap; } buf;
static void put(buf *x, const void *p, size_t n)
{
	if (x->n + n > x->cap) { x->cap = (x->n + n) * 2 + 4096; x->b = realloc(x->b, x->cap); if (!x->b) die("out of memory"); }
	memcpy(x->b + x->n, p, n); x->n += n;
}
static void put8(buf *x, int v) { unsigned char c = v; put(x, &c, 1); }
static void put16(buf *x, int v) { unsigned char c[2] = {v >> 8, v}; put(x, c, 2); }
static void put32(buf *x, uint32_t v) { unsigned char c[4] = {v >> 24, v >> 16, v >> 8, v}; put(x, c, 4); }

/* ---------------- IPP messages */
enum { T_OP = 1, T_JOB = 2, T_END = 3, T_PRINTER = 4, T_INT = 0x21, T_BOOL = 0x22, T_ENUM = 0x23, T_RES = 0x32, T_RANGE = 0x33,
       T_TEXT = 0x41, T_NAME = 0x42, T_KEYWORD = 0x44, T_URI = 0x45, T_CHARSET = 0x47, T_LANG = 0x48, T_MIME = 0x49 };
static void attr(buf *x, int tag, const char *name, const void *v, int vn)
{
	put8(x, tag); put16(x, name ? (int)strlen(name) : 0); if (name) put(x, name, strlen(name)); put16(x, vn); put(x, v, vn);
}
static void attr_s(buf *x, int tag, const char *name, const char *v) { attr(x, tag, name, v, strlen(v)); }
static void attr_i(buf *x, int tag, const char *name, int v) { unsigned char c[4] = {v >> 24, v >> 16, v >> 8, v}; attr(x, tag, name, c, 4); }
static int REQID = 1;
static void ipp_head(buf *x, int op, const char *uri)
{
	put16(x, 0x0200); put16(x, op); put32(x, REQID++); put8(x, T_OP);
	attr_s(x, T_CHARSET, "attributes-charset", "utf-8"); attr_s(x, T_LANG, "attributes-natural-language", "en");
	attr_s(x, T_URI, "printer-uri", uri);
	attr_s(x, T_NAME, "requesting-user-name", "rudolf");
}

/* parsed response: a flat list of (group, name, tag, value) */
typedef struct { int group, tag; char name[64]; char val[256]; int ival, ival2, ival3; } ipa;
typedef struct { int status; ipa *a; int n; } ipr;
static int parse_ipp(const unsigned char *p, size_t n, ipr *r)
{
	memset(r, 0, sizeof *r); if (n < 9) return -1;
	r->status = p[2] << 8 | p[3]; size_t i = 8; int group = 0; char last[64] = "";
	while (i < n) {
		int tag = p[i++];
		if (tag == T_END) break;
		if (tag < 0x10) { group = tag; continue; }
		if (i + 2 > n) return -1; int nl = p[i] << 8 | p[i + 1]; i += 2; if (i + nl + 2 > n) return -1;
		if (nl) { snprintf(last, sizeof last, "%.*s", nl < 63 ? nl : 63, (const char *)p + i); } i += nl;
		int vl = p[i] << 8 | p[i + 1]; i += 2; if (i + vl > n) return -1;
		if (tag == 0x34 || tag == 0x37 || tag == 0x4a) { i += vl; continue; }   /* collections are skipped */
		if (r->n % 64 == 0) r->a = realloc(r->a, (r->n + 64) * sizeof(ipa));
		ipa *a = &r->a[r->n++]; memset(a, 0, sizeof *a); a->group = group; a->tag = tag; snprintf(a->name, sizeof a->name, "%s", last);
		const unsigned char *v = p + i;
		if ((tag == T_INT || tag == T_ENUM) && vl == 4) a->ival = (int)((uint32_t)v[0] << 24 | v[1] << 16 | v[2] << 8 | v[3]);
		else if (tag == T_BOOL && vl == 1) a->ival = v[0];
		else if (tag == T_RANGE && vl == 8) { a->ival = (int)((uint32_t)v[0] << 24 | v[1] << 16 | v[2] << 8 | v[3]); a->ival2 = (int)((uint32_t)v[4] << 24 | v[5] << 16 | v[6] << 8 | v[7]); }
		else if (tag == T_RES && vl == 9) { a->ival = (int)((uint32_t)v[0] << 24 | v[1] << 16 | v[2] << 8 | v[3]); a->ival2 = (int)((uint32_t)v[4] << 24 | v[5] << 16 | v[6] << 8 | v[7]); a->ival3 = v[8]; }
		else if (tag >= 0x40) snprintf(a->val, sizeof a->val, "%.*s", vl < 255 ? vl : 255, (const char *)v);
		i += vl;
	}
	return 0;
}

/* ---------------- HTTP POST through libcurl (dlopen, like rudolf-update) */
typedef void CURL; struct curl_slist;
static struct { void *h; CURL *(*init)(void); int (*setopt)(CURL *, int, ...); int (*perform)(CURL *); int (*getinfo)(CURL *, int, ...);
	struct curl_slist *(*slist)(struct curl_slist *, const char *); const char *(*strerror)(int); int (*global_init)(long); } C;
static size_t wcb(char *p, size_t s, size_t n, void *u) { put((buf *)u, p, s * n); return s * n; }
static void http_uri(const char *uri, char *out, size_t n)
{
	/* ipp://h/p -> http://h:631/p ; ipps://h/p -> https://h:443?.. no: IPP over TLS uses port 631 too */
	const char *rest = strstr(uri, "://"); rest = rest ? rest + 3 : uri;
	int tls = !strncmp(uri, "ipps:", 5) || !strncmp(uri, "https:", 6);
	const char *slash = strchr(rest, '/'); size_t hl = slash ? (size_t)(slash - rest) : strlen(rest);
	char host[300]; snprintf(host, sizeof host, "%.*s", (int)hl, rest);
	int has_port = host[0] == '[' ? strstr(host, "]:") != NULL : strchr(host, ':') != NULL;
	snprintf(out, n, "%s://%s%s%s", tls ? "https" : "http", host, has_port ? "" : ":631", slash ? slash : "/ipp/print");
}
static int ipp_post(const char *uri, buf *req, const char *docpath, buf *resp, int timeout)
{
	if (!C.h) {
		if (!(C.h = dlopen("libcurl.so.4", RTLD_NOW))) die("libcurl is missing");
		C.init = dlsym(C.h, "curl_easy_init"); C.setopt = dlsym(C.h, "curl_easy_setopt"); C.perform = dlsym(C.h, "curl_easy_perform");
		C.getinfo = dlsym(C.h, "curl_easy_getinfo"); C.slist = dlsym(C.h, "curl_slist_append"); C.strerror = dlsym(C.h, "curl_easy_strerror");
		C.global_init = dlsym(C.h, "curl_global_init"); if (!C.init || !C.setopt || !C.perform || !C.slist) die("libcurl is unusable"); C.global_init(3);
	}
	buf body = {0}; put(&body, req->b, req->n);
	if (docpath) { FILE *f = fopen(docpath, "rb"); if (!f) die("cannot read the document"); char b[65536]; size_t k; while ((k = fread(b, 1, sizeof b, f)) > 0) put(&body, b, k); fclose(f); }
	char url[600]; http_uri(uri, url, sizeof url);
	CURL *h = C.init(); struct curl_slist *hd = C.slist(NULL, "Content-Type: application/ipp"); hd = C.slist(hd, "Expect:");
	C.setopt(h, 10002, url); C.setopt(h, 47, 1L); C.setopt(h, 10015, body.b); C.setopt(h, 30120, (long long)body.n);
	C.setopt(h, 10023, hd); C.setopt(h, 20011, wcb); C.setopt(h, 10001, resp); C.setopt(h, 78, 8L); C.setopt(h, 13, (long)timeout);
	C.setopt(h, 64, 0L); C.setopt(h, 81, 0L); C.setopt(h, 10018, "rudolf-print/1"); C.setopt(h, 99, 1L);
	int r = C.perform(h); long code = 0; C.getinfo(h, 0x200002, &code); free(body.b);
	if (r) { fprintf(stderr, "%s\n", C.strerror ? C.strerror(r) : "failed"); return -1; }
	if (code != 200) { fprintf(stderr, "HTTP %ld\n", code); return -2; }
	return 0;
}

/* ---------------- printer attributes */
static const char *ASK[] = {"printer-name", "printer-info", "printer-make-and-model", "printer-location", "printer-state", "printer-state-reasons",
	"printer-state-message", "document-format-supported", "media-supported", "media-default", "media-ready", "sides-supported", "sides-default",
	"print-color-mode-supported", "print-color-mode-default", "copies-supported", "page-ranges-supported",
	"pwg-raster-document-resolution-supported", "pwg-raster-document-type-supported", "pwg-raster-document-sheet-back", "urf-supported",
	"printer-resolution-supported", "color-supported", "printer-uri-supported", "uri-security-supported", NULL};
static int get_attrs(const char *uri, ipr *r)
{
	buf q = {0}; ipp_head(&q, 0x000B, uri);
	const char *ra = "requested-attributes"; for (int i = 0; ASK[i]; i++) attr_s(&q, T_KEYWORD, i ? NULL : ra, ASK[i]);
	put8(&q, T_END); buf rs = {0}; int e = ipp_post(uri, &q, NULL, &rs, 15); free(q.b);
	if (e) return e; if (parse_ipp(rs.b, rs.n, r)) return -3; free(rs.b); return 0;
}
static void list_json(ipr *r, const char *name, const char *key)
{
	printf(",\"%s\":[", key); int first = 1;
	for (int i = 0; i < r->n; i++) if (!strcmp(r->a[i].name, name)) {
		if (!first) putchar(','); first = 0; ipa *a = &r->a[i];
		if (a->tag == T_RES) printf("%d", a->ival);
		else if (a->tag == T_INT || a->tag == T_ENUM) printf("%d", a->ival);
		else if (a->tag == T_RANGE) printf("[%d,%d]", a->ival, a->ival2);
		else if (a->tag == T_BOOL) printf(a->ival ? "true" : "false");
		else js(a->val);
	}
	putchar(']');
}
static const ipa *one(ipr *r, const char *name) { for (int i = 0; i < r->n; i++) if (!strcmp(r->a[i].name, name)) return &r->a[i]; return NULL; }
static int cmd_info(const char *uri)
{
	ipr r; int e = get_attrs(uri, &r);
	if (e == -1) die("The printer does not answer. Check that it is switched on and on the same network.");
	if (e == -2) die("This device does not accept IPP print jobs at that address.");
	if (e) die("The printer's answer could not be read.");
	if (r.status >= 0x100) die("The printer refused the request (IPP status 0x%04x).", r.status);
	const ipa *nm = one(&r, "printer-info"), *mm = one(&r, "printer-make-and-model"), *st = one(&r, "printer-state"), *lc = one(&r, "printer-location"), *cs = one(&r, "color-supported");
	printf("{\"ok\":true,\"uri\":"); js(uri); printf(",\"name\":"); js(nm && *nm->val ? nm->val : one(&r, "printer-name") ? one(&r, "printer-name")->val : "Printer");
	printf(",\"model\":"); js(mm ? mm->val : ""); printf(",\"location\":"); js(lc ? lc->val : "");
	printf(",\"state\":%d,\"color\":%s", st ? st->ival : 3, cs && cs->ival ? "true" : "false");
	const ipa *sm = one(&r, "printer-state-message"); printf(",\"message\":"); js(sm ? sm->val : "");
	list_json(&r, "printer-state-reasons", "reasons"); list_json(&r, "document-format-supported", "formats");
	list_json(&r, "media-supported", "media"); list_json(&r, "media-ready", "mediaReady"); list_json(&r, "media-default", "mediaDefault");
	list_json(&r, "sides-supported", "sides"); list_json(&r, "print-color-mode-supported", "colorModes");
	list_json(&r, "copies-supported", "copies"); list_json(&r, "page-ranges-supported", "pageRanges");
	list_json(&r, "pwg-raster-document-resolution-supported", "pwgResolutions"); list_json(&r, "pwg-raster-document-type-supported", "pwgTypes");
	list_json(&r, "pwg-raster-document-sheet-back", "sheetBack"); list_json(&r, "urf-supported", "urf");
	printf("}\n"); return 0;
}

/* ---------------- sending a job */
static int okword(const char *s) { if (!*s || strlen(s) > 80) return 0; for (; *s; s++) if (!isalnum((unsigned char)*s) && !strchr("-_.", *s)) return 0; return 1; }
static int cmd_send(int argc, char **argv)
{
	const char *uri = argv[2], *file = argv[3], *fmt = argv[4];
	int copies = 1, p1 = 0, p2 = 0; const char *sides = NULL, *color = NULL, *media = NULL; char title[200] = "Document";
	for (int i = 5; i < argc; i++) {
		char *v = strchr(argv[i], '='); if (!v) continue; v++;
		if (!strncmp(argv[i], "copies=", 7)) copies = atoi(v);
		else if (!strncmp(argv[i], "sides=", 6) && okword(v)) sides = v;
		else if (!strncmp(argv[i], "color=", 6) && okword(v)) color = v;
		else if (!strncmp(argv[i], "media=", 6) && okword(v)) media = v;
		else if (!strncmp(argv[i], "pages=", 6)) sscanf(v, "%d-%d", &p1, &p2);
		else if (!strncmp(argv[i], "title-file=", 11)) { FILE *f = fopen(v, "r"); if (f) { if (!fgets(title, sizeof title, f)) {} fclose(f); title[strcspn(title, "\r\n")] = 0; } }
	}
	if (copies < 1 || copies > 99) copies = 1;
	if (strcmp(fmt, "application/pdf") && strcmp(fmt, "image/pwg-raster") && strcmp(fmt, "image/urf")) die("unknown document format");
	struct stat st; if (stat(file, &st) || !st.st_size) die("the document is empty");
	buf q = {0}; ipp_head(&q, 0x0002, uri);
	attr_s(&q, T_NAME, "job-name", *title ? title : "Document"); attr_s(&q, T_MIME, "document-format", fmt);
	put8(&q, T_JOB);
	if (copies > 1) attr_i(&q, T_INT, "copies", copies);
	if (sides) attr_s(&q, T_KEYWORD, "sides", sides);
	if (color) attr_s(&q, T_KEYWORD, "print-color-mode", color);
	if (media) attr_s(&q, T_KEYWORD, "media", media);
	if (p1 > 0 && p2 >= p1 && !strcmp(fmt, "application/pdf")) { unsigned char rg[8] = {p1 >> 24, p1 >> 16, p1 >> 8, p1, p2 >> 24, p2 >> 16, p2 >> 8, p2}; attr(&q, T_RANGE, "page-ranges", rg, 8); }
	put8(&q, T_END);
	buf rs = {0}; int e = ipp_post(uri, &q, file, &rs, 600); free(q.b);
	if (e == -1) die("The printer does not answer. Check that it is switched on and on the same network.");
	if (e) die("The printer did not accept the job.");
	ipr r; if (parse_ipp(rs.b, rs.n, &r)) die("The printer's answer could not be read.");
	if (r.status == 0x040A) die("The printer cannot take this kind of document.");
	if (r.status == 0x0507) die("The printer is busy or offline. Try again in a moment.");
	if (r.status >= 0x400 && r.status < 0x500) die("The printer refused the job (IPP status 0x%04x: a setting it does not support?).", r.status);
	if (r.status >= 0x500) die("The printer could not print the job (IPP status 0x%04x).", r.status);
	const ipa *id = one(&r, "job-id"), *js_ = one(&r, "job-state");
	printf("{\"ok\":true,\"job\":%d,\"state\":%d,\"status\":%d", id ? id->ival : 0, js_ ? js_->ival : 0, r.status);
	printf(",\"message\":"); js(r.status ? "Sent. The printer ignored some settings it does not support." : "Sent to the printer."); printf("}\n");
	return 0;
}

/* ---------------- multicast DNS discovery (RFC 6762/6763) */
typedef struct { char inst[256], host[256], rp[128], ty[128], note[128], ip[64], pdl[256]; int port, tls, srv, txt; } svc;
static svc S[64]; static int NS;
typedef struct { char name[256], ip[64]; } arec; static arec A[64]; static int NA;
static int rdname(const unsigned char *m, size_t n, size_t *i, char *out, size_t on)
{
	size_t o = 0, at = *i; int jumped = 0, hops = 0; out[0] = 0;
	while (at < n) {
		int l = m[at];
		if (l == 0) { at++; break; }
		if ((l & 0xC0) == 0xC0) { if (at + 1 >= n || ++hops > 20) return -1; size_t to = (l & 0x3F) << 8 | m[at + 1]; if (!jumped) *i = at + 2; jumped = 1; at = to; continue; }
		at++; if (at + l > n) return -1;
		if (o && o < on - 1) out[o++] = '.';
		for (int k = 0; k < l && o < on - 1; k++) out[o++] = m[at + k] == '.' ? '\x1f' : m[at + k];   /* dots inside a label are kept apart */
		out[o] = 0; at += l;
	}
	if (!jumped) *i = at;
	return 0;
}
static void qname(buf *x, const char *name)
{
	const char *p = name;
	while (*p) { const char *d = strchr(p, '.'); size_t l = d ? (size_t)(d - p) : strlen(p); put8(x, l); for (size_t k = 0; k < l; k++) put8(x, p[k] == '\x1f' ? '.' : p[k]); p += l; if (*p == '.') p++; }
	put8(x, 0);
}
static svc *svc_get(const char *inst)
{
	for (int i = 0; i < NS; i++) if (!strcasecmp(S[i].inst, inst)) return &S[i];
	if (NS >= 64) return NULL; svc *s = &S[NS++]; memset(s, 0, sizeof *s); snprintf(s->inst, sizeof s->inst, "%s", inst);
	s->tls = strstr(inst, "._ipps._tcp.") != NULL; return s;
}
static void parse_mdns(const unsigned char *m, size_t n)
{
	if (n < 12) return;
	int qd = m[4] << 8 | m[5], rr = (m[6] << 8 | m[7]) + (m[8] << 8 | m[9]) + (m[10] << 8 | m[11]); size_t i = 12; char nm[256], t[256];
	for (int k = 0; k < qd; k++) { if (rdname(m, n, &i, nm, sizeof nm)) return; i += 4; }
	for (int k = 0; k < rr; k++) {
		if (rdname(m, n, &i, nm, sizeof nm) || i + 10 > n) return;
		int type = m[i] << 8 | m[i + 1]; size_t len = m[i + 8] << 8 | m[i + 9]; i += 10; if (i + len > n) return; size_t d = i;
		if (type == 12 && (strstr(nm, "_ipp._tcp.") || strstr(nm, "_ipps._tcp.")) && !strstr(nm, "._sub.")) { size_t j = d; if (!rdname(m, n, &j, t, sizeof t)) svc_get(t); }
		else if (type == 12 && strstr(nm, "._sub.")) { size_t j = d; if (!rdname(m, n, &j, t, sizeof t)) svc_get(t); }
		else if (type == 33 && len > 6) { svc *s = svc_get(nm); size_t j = d + 6; if (s && !rdname(m, n, &j, t, sizeof t)) { s->port = m[d + 4] << 8 | m[d + 5]; snprintf(s->host, sizeof s->host, "%s", t); s->srv = 1; } }
		else if (type == 16) {
			svc *s = svc_get(nm); if (s) { s->txt = 1; size_t j = d;
				while (j < d + len) { int l = m[j++]; if (j + l > d + len) break; char kv[300]; snprintf(kv, sizeof kv, "%.*s", l, (const char *)m + j); j += l;
					char *eq = strchr(kv, '='); if (!eq) continue; *eq++ = 0;
					if (!strcasecmp(kv, "rp")) snprintf(s->rp, sizeof s->rp, "%s", eq); else if (!strcasecmp(kv, "ty")) snprintf(s->ty, sizeof s->ty, "%s", eq);
					else if (!strcasecmp(kv, "note")) snprintf(s->note, sizeof s->note, "%s", eq); else if (!strcasecmp(kv, "pdl")) snprintf(s->pdl, sizeof s->pdl, "%s", eq); } }
		}
		else if (type == 1 && len == 4 && NA < 64) { snprintf(A[NA].name, sizeof A[NA].name, "%s", nm); inet_ntop(AF_INET, m + d, A[NA].ip, sizeof A[NA].ip); NA++; }
		i += len;
	}
}
static void mdns_query(int fd, struct sockaddr_in *to, const char **names, const int *types, int k)
{
	buf q = {0}; put16(&q, 0); put16(&q, 0); put16(&q, k); put16(&q, 0); put16(&q, 0); put16(&q, 0);
	for (int i = 0; i < k; i++) { qname(&q, names[i]); put16(&q, types[i]); put16(&q, 1); }
	if (sendto(fd, q.b, q.n, 0, (struct sockaddr *)to, sizeof *to) < 0) {} free(q.b);
}
static void mdns_listen(int fd, int ms)
{
	struct timeval t0; gettimeofday(&t0, NULL);
	for (;;) {
		struct timeval t; gettimeofday(&t, NULL); int left = ms - (int)((t.tv_sec - t0.tv_sec) * 1000 + (t.tv_usec - t0.tv_usec) / 1000); if (left <= 0) break;
		struct pollfd p = {fd, POLLIN, 0}; if (poll(&p, 1, left) <= 0) break;
		unsigned char m[9000]; ssize_t n = recv(fd, m, sizeof m, 0); if (n > 0) parse_mdns(m, n);
	}
}
static int cmd_find(void)
{
	int fd = socket(AF_INET, SOCK_DGRAM, 0); if (fd < 0) die("no network");
	struct sockaddr_in to = {0}; to.sin_family = AF_INET; to.sin_port = htons(5353); inet_pton(AF_INET, "224.0.0.251", &to.sin_addr);
	const char *ov = getenv("RP_MDNS");
	if (ov) { char a[64]; snprintf(a, sizeof a, "%s", ov); char *c = strrchr(a, ':'); if (c) { *c = 0; to.sin_port = htons(atoi(c + 1)); } inet_pton(AF_INET, a, &to.sin_addr); }
	unsigned char ttl = 255; setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, 1);
	const char *n1[] = {"_ipp._tcp.local", "_ipps._tcp.local", "_print._sub._ipp._tcp.local"}; const int t1[] = {12, 12, 12};
	mdns_query(fd, &to, n1, t1, 3); mdns_listen(fd, 1200); mdns_query(fd, &to, n1, t1, 3); mdns_listen(fd, 900);
	/* ask again for whatever the first answers left out */
	for (int round = 0; round < 2; round++) {
		const char *nn[32]; int tt[32], k = 0;
		for (int i = 0; i < NS && k < 30; i++) { if (!S[i].srv) { nn[k] = S[i].inst; tt[k++] = 33; } if (!S[i].txt) { nn[k] = S[i].inst; tt[k++] = 16; } }
		for (int i = 0; i < NS && k < 31; i++) if (S[i].srv) { int have = 0; for (int j = 0; j < NA; j++) if (!strcasecmp(A[j].name, S[i].host)) have = 1; if (!have) { nn[k] = S[i].host; tt[k++] = 1; } }
		if (!k) break; mdns_query(fd, &to, nn, tt, k); mdns_listen(fd, 800);
	}
	close(fd);
	printf("{\"ok\":true,\"printers\":["); int first = 1;
	/* one entry per printer: prefer plain ipp (some printers' TLS is slow), keep ipps when it is the only one */
	for (int i = 0; i < NS; i++) {
		svc *s = &S[i]; if (!s->srv || !s->port) continue;
		for (int j = 0; j < NA; j++) if (!strcasecmp(A[j].name, s->host)) snprintf(s->ip, sizeof s->ip, "%s", A[j].ip);
		if (!*s->ip) continue;
		char label[256]; snprintf(label, sizeof label, "%s", s->inst); char *d = strchr(label, '.'); if (d) *d = 0; for (char *c = label; *c; c++) if (*c == '\x1f') *c = '.';
		int dup = 0; for (int j = 0; j < NS; j++) if (j != i && S[j].srv && !strncasecmp(S[j].inst, s->inst, strlen(label)) && S[j].inst[strlen(label)] == '.' && s->tls && !S[j].tls) dup = 1;
		if (dup) continue;
		char uri[600]; snprintf(uri, sizeof uri, "%s://%s:%d/%s", s->tls ? "ipps" : "ipp", s->ip, s->port, *s->rp ? s->rp : "ipp/print");
		if (!first) putchar(','); first = 0;
		printf("{\"name\":"); js(label); printf(",\"uri\":"); js(uri); printf(",\"model\":"); js(s->ty); printf(",\"location\":"); js(s->note); printf("}");
	}
	printf("]}\n"); return 0;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IOFBF, 1 << 16);
	if (argc >= 2 && !strcmp(argv[1], "find")) return cmd_find();
	if (argc >= 3 && !strcmp(argv[1], "info")) return cmd_info(argv[2]);
	if (argc >= 5 && !strcmp(argv[1], "send")) return cmd_send(argc, argv);
	fprintf(stderr, "usage: rudolf-print find | info URI | send URI FILE FORMAT [copies=N] [sides=KW] [color=KW] [media=KW] [pages=A-B] [title-file=PATH]\n");
	return 2;
}
