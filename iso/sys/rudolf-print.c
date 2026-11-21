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
 * 1.6.1 (docs/plan-1.6.1.md Part 9, found on the owner's Vivobook): find asks as a full mDNS querier (RFC 6762): from
 * port 5353 (SO_REUSEADDR: rudolfd's find and the print queue's may run at once; Rudolf OS has no Avahi), joined to
 * 224.0.0.251 on each interface that is up (not loopback, point-to-point or a cellular modem's: /run/rudolf/firewall-
 * cellular, RP_CELLULAR elsewhere for tests), so printers answer by multicast, which the firewall lets in (its mdns
 * chain). Until then it asked from an ephemeral port, and the firewall dropped every answer. Only responses (QR set)
 * from source port 5353 that arrive on a joined interface and are not from this computer's own addresses are read.
 * 1.6.0 (docs/plan-1.6.0.md Part 8): USB printers, from /run/rudolf/usb-printers (written by /usr/lib/rudolf/usbprint;
 * RP_USB elsewhere for tests), are listed by find as "USB: name": an IPP over USB printer at ipp-usb's local address
 * (printed to as a network printer), a plain one as usb://lpN (its job written to /dev/usb/lpN: a PDF as it is, for a
 * PostScript printer through the USB printers extension's pdftops; RP_USBDEV and RP_FILTERS elsewhere for tests), one that
 * needs a driver, or an IPP over USB one without the extension, as usb://... that says so.
 *   rudolf-print usb                      the USB printers alone (JSON, for Settings › Devices)
 *
 * Copyright (C) 2026 the Rudolf OS authors. SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
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
static int usb_info(const char *uri);
static int cmd_info(const char *uri)
{
	if (!strncmp(uri, "usb://", 6)) return usb_info(uri);   /* (1.6.0) */
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
static int usb_send(const char *uri, const char *file, const char *fmt, int copies, int p1, int p2);
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
	if (!strncmp(uri, "usb://", 6)) return usb_send(uri, file, fmt, copies, p1, p2);   /* (1.6.0: a plain USB printer) */
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

/* ---------------- (1.6.0) USB printers: /run/rudolf/usb-printers, one a line: KIND\tNAME\tURI\tDEVICE-ID\tSYSNAME
   KIND: ipp (IPP over USB; URI ipp-usb's address, or empty without the extension), pdf, postscript, driver */
typedef struct { char kind[16], name[160], uri[200], devid[600], sys[64], model[160]; } usbp;
static usbp U[16]; static int NU;
static void usb_load(void)
{
	NU = 0; FILE *f = fopen(getenv("RP_USB") ? getenv("RP_USB") : "/run/rudolf/usb-printers", "r"); if (!f) return;
	char l[1400];
	while (NU < 16 && fgets(l, sizeof l, f)) {
		l[strcspn(l, "\r\n")] = 0; char *c[5] = {0}; int n = 0; char *p = l;
		while (n < 5) { c[n++] = p; char *t = strchr(p, '\t'); if (!t) break; *t = 0; p = t + 1; }
		if (n < 5 || !*c[0]) continue;
		usbp *u = &U[NU]; memset(u, 0, sizeof *u);
		snprintf(u->kind, sizeof u->kind, "%s", c[0]); snprintf(u->name, sizeof u->name, "%s", *c[1] ? c[1] : "USB printer");
		snprintf(u->devid, sizeof u->devid, "%s", c[3]); snprintf(u->sys, sizeof u->sys, "%s", c[4]);
		if (!strcmp(u->kind, "ipp") && *c[2]) snprintf(u->uri, sizeof u->uri, "%s", c[2]);
		else if (!strncmp(c[2], "/dev/usb/lp", 11)) snprintf(u->uri, sizeof u->uri, "usb://%s", c[2] + 9);
		else snprintf(u->uri, sizeof u->uri, "usb://%s/%s", u->kind, u->sys);
		/* the model from the device ID (MDL:, else the name) */
		const char *m = strstr(u->devid, "MDL:"); if (!m) m = strstr(u->devid, "MODEL:");
		if (m) { m = strchr(m, ':') + 1; size_t k = strcspn(m, ";"); snprintf(u->model, sizeof u->model, "%.*s", (int)(k < 150 ? k : 150), m); } else snprintf(u->model, sizeof u->model, "%s", u->name);
		NU++;
	}
	fclose(f);
}
static const char *usb_kind_text(const usbp *u)
{
	if (!strcmp(u->kind, "ipp")) return strncmp(u->uri, "ipp", 3) ? "IPP over USB (needs the USB printers extension)" : "IPP over USB";
	if (!strcmp(u->kind, "pdf")) return "PDF"; if (!strcmp(u->kind, "postscript")) return "PostScript";
	return "needs a driver";
}
static char *usb_filter(const char *name, char *out, size_t n);
static void usb_json(const usbp *u, int full)
{
	printf("{\"name\":"); { char n[200]; snprintf(n, sizeof n, "USB: %s", u->name); js(full ? u->name : n); } printf(",\"uri\":"); js(u->uri);
	printf(",\"model\":"); js(u->model); printf(",\"location\":\"USB\",\"usb\":"); js(u->kind); printf(",\"kind\":"); js(usb_kind_text(u));
	if (full) { char f[300]; int ready = !strcmp(u->kind, "ipp") ? !strncmp(u->uri, "ipp", 3) : !strcmp(u->kind, "pdf") ? 1 : !strcmp(u->kind, "postscript") ? usb_filter("pdftops", f, sizeof f) != NULL : 0;
		printf(",\"ready\":%s,\"deviceId\":", ready ? "true" : "false"); js(u->devid); printf(",\"sys\":"); js(u->sys); }
	putchar('}');
}
static int cmd_usb(void)
{
	usb_load(); printf("{\"ok\":true,\"printers\":[");
	for (int i = 0; i < NU; i++) { if (i) putchar(','); usb_json(&U[i], 1); }
	printf("]}\n"); return 0;
}
static const usbp *usb_find(const char *uri) { usb_load(); for (int i = 0; i < NU; i++) if (!strcmp(U[i].uri, uri)) return &U[i]; return NULL; }
static char *usb_filter(const char *name, char *out, size_t n)
{
	snprintf(out, n, "%s/%s", getenv("RP_FILTERS") ? getenv("RP_FILTERS") : "/usr/lib/rudolf/print", name);
	return access(out, X_OK) ? NULL : out;
}
#define NEED_EXT "install the USB printers extension (Settings › Update and recovery › System extensions)"
static int usb_info(const char *uri)
{
	const usbp *u = usb_find(uri); char f[300];
	if (!u) die("The printer is not plugged in (or was unplugged).");
	if (!strcmp(u->kind, "ipp")) die("This printer prints over USB with IPP: " NEED_EXT " to print to it.");
	if (!strcmp(u->kind, "postscript") && !usb_filter("pdftops", f, sizeof f)) die("This printer reads PostScript: " NEED_EXT " to print to it.");
	int can = !strcmp(u->kind, "pdf") || !strcmp(u->kind, "postscript");
	printf("{\"ok\":true,\"uri\":"); js(uri); printf(",\"name\":"); js(u->name); printf(",\"model\":"); js(u->model);
	printf(",\"location\":\"USB\",\"state\":3,\"color\":true,\"message\":"); js(can ? "" : "It needs a driver Rudolf OS does not have.");
	printf(",\"usb\":"); js(u->kind); printf(",\"formats\":[%s],\"sides\":[\"one-sided\"],\"copies\":[[1,99]]", can ? "\"application/pdf\"" : "");
	printf(",\"pageRanges\":[%s]}\n", usb_filter(!strcmp(u->kind, "pdf") ? "pdftocairo" : "pdftops", f, sizeof f) ? "true" : "false"); return 0;
}
/* a plain printer's job: the PDF (the pages asked for, through pdftocairo, when the extension is there) or PostScript from
   pdftops, written to its /dev/usb/lpN, once per copy */
static int usb_send(const char *uri, const char *file, const char *fmt, int copies, int p1, int p2)
{
	const usbp *u = usb_find(uri); char flt[300], cmd[1400], tmp[64] = "";
	if (!u) die("The printer is not plugged in (or was unplugged).");
	if (strcmp(u->kind, "pdf") && strcmp(u->kind, "postscript")) die(!strcmp(u->kind, "ipp") ? "This printer prints over USB with IPP: " NEED_EXT " to print to it." : "This printer needs a driver Rudolf OS does not have.");
	if (strcmp(fmt, "application/pdf")) die("The printer cannot take this kind of document.");
	if (strncmp(uri, "usb://lp", 8) || strspn(uri + 8, "0123456789") != strlen(uri + 8) || !uri[8]) die("not a USB printer");
	char dev[160]; snprintf(dev, sizeof dev, "%s/%s", getenv("RP_USBDEV") ? getenv("RP_USBDEV") : "/dev/usb", uri + 6);
	int ranged = p1 > 0 && p2 >= p1, all = 0;
	const char *src = file;
	if (!strcmp(u->kind, "postscript")) {
		if (!usb_filter("pdftops", flt, sizeof flt)) die("This printer reads PostScript: " NEED_EXT " to print to it.");
		strcpy(tmp, "/tmp/rprint-usb-XXXXXX"); int t = mkstemp(tmp); if (t < 0) die("no room for the print job"); close(t);
		char rg[40] = ""; if (ranged) snprintf(rg, sizeof rg, "-f %d -l %d ", p1, p2);
		snprintf(cmd, sizeof cmd, "'%s' -q %s'%s' '%s'", flt, rg, file, tmp);
		if (system(cmd)) { unlink(tmp); die("The document could not be made into PostScript."); }
		src = tmp;
	} else if (ranged) {
		if (usb_filter("pdftocairo", flt, sizeof flt)) {
			strcpy(tmp, "/tmp/rprint-usb-XXXXXX"); int t = mkstemp(tmp); if (t < 0) die("no room for the print job"); close(t);
			snprintf(cmd, sizeof cmd, "'%s' -q -pdf -f %d -l %d '%s' '%s'", flt, p1, p2, file, tmp);
			if (system(cmd)) { unlink(tmp); die("The pages could not be picked out of the document."); }
			src = tmp;
		} else all = 1;   /* (without the extension a range cannot be picked out: all the pages are printed) */
	}
	FILE *in = NULL; size_t total = 0;
	for (int c = 0; c < copies; c++) {
		in = fopen(src, "rb"); if (!in) { if (*tmp) unlink(tmp); die("the document is empty"); }
		int o = open(dev, O_WRONLY | O_CLOEXEC); if (o < 0) { fclose(in); if (*tmp) unlink(tmp); die("The printer does not answer (%s: %s). Check that it is switched on and plugged in.", uri + 6, strerror(errno)); }
		char b[65536]; size_t n;
		while ((n = fread(b, 1, sizeof b, in)) > 0) { size_t w = 0; while (w < n) { ssize_t k = write(o, b + w, n - w); if (k < 0) { if (errno == EINTR) continue; close(o); fclose(in); if (*tmp) unlink(tmp); die("The printer stopped taking the job (%s).", strerror(errno)); } w += k; } total += n; }
		close(o); fclose(in);
	}
	if (*tmp) unlink(tmp);
	printf("{\"ok\":true,\"job\":0,\"state\":9,\"status\":0,\"bytes\":%zu,\"message\":", total);
	js(all ? "Sent to the printer: all the pages (picking pages needs the USB printers extension)." : "Sent to the printer."); printf("}\n");
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
/* the interfaces find asks on (joined to 224.0.0.251) and this computer's own addresses (whose packets are not answers) */
static int MIF[16], NMIF, MCAST;
static struct in_addr OWN[32]; static int NOWN;
static int mif_has(int ix) { for (int i = 0; i < NMIF; i++) if (MIF[i] == ix) return 1; return 0; }
static int is_cellular(const char *ifn)
{
	const char *p = getenv("RP_CELLULAR"); FILE *f = fopen(p ? p : "/run/rudolf/firewall-cellular", "r"); char l[64]; int r = 0;
	if (!f) return 0;
	while (!r && fgets(l, sizeof l, f)) { l[strcspn(l, "\r\n")] = 0; if (!strcmp(l, ifn)) r = 1; }
	fclose(f); return r;
}
static void mdns_join(int fd, int drop)
{
	if (drop) {
		for (int i = 0; i < NMIF; i++) { struct ip_mreqn mr = {0}; inet_pton(AF_INET, "224.0.0.251", &mr.imr_multiaddr); mr.imr_ifindex = MIF[i]; setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mr, sizeof mr); }
		NMIF = 0; return;
	}
	struct ifaddrs *ifs = NULL; if (getifaddrs(&ifs)) return;
	for (struct ifaddrs *a = ifs; a; a = a->ifa_next) {
		if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET) continue;
		if (NOWN < 32) OWN[NOWN++] = ((struct sockaddr_in *)a->ifa_addr)->sin_addr;
		unsigned fl = a->ifa_flags; if (!(fl & IFF_UP) || !(fl & IFF_MULTICAST) || (fl & (IFF_LOOPBACK | IFF_POINTOPOINT))) continue;
		int ix = if_nametoindex(a->ifa_name); if (!ix || mif_has(ix) || NMIF >= 16 || is_cellular(a->ifa_name)) continue;
		struct ip_mreqn mr = {0}; inet_pton(AF_INET, "224.0.0.251", &mr.imr_multiaddr); mr.imr_ifindex = ix;
		if (!setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mr, sizeof mr)) MIF[NMIF++] = ix;
	}
	freeifaddrs(ifs);
}
static void mdns_query(int fd, struct sockaddr_in *to, const char **names, const int *types, int k)
{
	buf q = {0}; put16(&q, 0); put16(&q, 0); put16(&q, k); put16(&q, 0); put16(&q, 0); put16(&q, 0);
	for (int i = 0; i < k; i++) { qname(&q, names[i]); put16(&q, types[i]); put16(&q, 1); }
	if (MCAST && NMIF) for (int i = 0; i < NMIF; i++) {   /* out of each joined interface */
		struct ip_mreqn mr = {0}; mr.imr_ifindex = MIF[i]; setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &mr, sizeof mr);
		if (sendto(fd, q.b, q.n, 0, (struct sockaddr *)to, sizeof *to) < 0) {}
	}
	else if (sendto(fd, q.b, q.n, 0, (struct sockaddr *)to, sizeof *to) < 0) {}
	free(q.b);
}
static void mdns_listen(int fd, int ms)
{
	struct timeval t0; gettimeofday(&t0, NULL);
	for (;;) {
		struct timeval t; gettimeofday(&t, NULL); int left = ms - (int)((t.tv_sec - t0.tv_sec) * 1000 + (t.tv_usec - t0.tv_usec) / 1000); if (left <= 0) break;
		struct pollfd p = {fd, POLLIN, 0}; if (poll(&p, 1, left) <= 0) break;
		unsigned char m[9000]; struct sockaddr_in from = {0}; char cb[256];
		struct iovec io = {m, sizeof m}; struct msghdr mh = {&from, sizeof from, &io, 1, cb, sizeof cb, 0};
		ssize_t n = recvmsg(fd, &mh, 0); if (n < 12 || !(m[2] & 0x80)) continue;   /* (responses only: QR set) */
		if (MCAST) {
			/* RFC 6762 6: a response's source port is 5353; and only what came in on an interface find asks on, from
			   another computer (not a program here pretending to be a printer) */
			int ix = 0; for (struct cmsghdr *c = CMSG_FIRSTHDR(&mh); c; c = CMSG_NXTHDR(&mh, c)) if (c->cmsg_level == IPPROTO_IP && c->cmsg_type == IP_PKTINFO) ix = ((struct in_pktinfo *)CMSG_DATA(c))->ipi_ifindex;
			if (ntohs(from.sin_port) != 5353 || !mif_has(ix)) continue;
			int own = 0; for (int i = 0; i < NOWN; i++) if (OWN[i].s_addr == from.sin_addr.s_addr) own = 1; if (own) continue;
		}
		parse_mdns(m, n);
	}
}
static int cmd_find(void)
{
	int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0); if (fd < 0) die("no network");
	struct sockaddr_in to = {0}; to.sin_family = AF_INET; to.sin_port = htons(5353); inet_pton(AF_INET, "224.0.0.251", &to.sin_addr);
	const char *ov = getenv("RP_MDNS");
	if (ov) { char a[64]; snprintf(a, sizeof a, "%s", ov); char *c = strrchr(a, ':'); if (c) { *c = 0; to.sin_port = htons(atoi(c + 1)); } inet_pton(AF_INET, a, &to.sin_addr); }
	else {
		/* a full querier: port 5353, the group joined, so that the answers come by multicast (RFC 6762 5.2, 6) */
		int one = 1, zero = 0; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
		setsockopt(fd, IPPROTO_IP, IP_MULTICAST_ALL, &zero, sizeof zero); setsockopt(fd, IPPROTO_IP, IP_PKTINFO, &one, sizeof one);
		unsigned char lp = 0; setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &lp, 1);
		struct sockaddr_in me = {0}; me.sin_family = AF_INET; me.sin_port = htons(5353); me.sin_addr.s_addr = htonl(INADDR_ANY);
		if (!bind(fd, (struct sockaddr *)&me, sizeof me)) { MCAST = 1; mdns_join(fd, 0); }
		/* (otherwise, port 5353 held by a program that does not share it: asked from an ephemeral port as before 1.6.1,
		   whose answers only arrive when the firewall is off) */
	}
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
	if (MCAST) mdns_join(fd, 1);   /* (the group left, the port given back) */
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
	/* (1.6.0) and the USB printers */
	usb_load(); for (int i = 0; i < NU; i++) { if (!first) putchar(','); first = 0; usb_json(&U[i], 0); }
	printf("]}\n"); return 0;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IOFBF, 1 << 16);
	if (argc >= 2 && !strcmp(argv[1], "find")) return cmd_find();
	if (argc >= 2 && !strcmp(argv[1], "usb")) return cmd_usb();
	if (argc >= 3 && !strcmp(argv[1], "info")) return cmd_info(argv[2]);
	if (argc >= 5 && !strcmp(argv[1], "send")) return cmd_send(argc, argv);
	fprintf(stderr, "usage: rudolf-print find | usb | info URI | send URI FILE FORMAT [copies=N] [sides=KW] [color=KW] [media=KW] [pages=A-B] [title-file=PATH]\n");
	return 2;
}
