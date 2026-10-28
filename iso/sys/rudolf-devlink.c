/* rudolf-devlink - the development link (1.4.0). GPL-2.0-or-later.
 *
 * Lets a developer the owner chooses (a Claude session, a collaborator) run tests on this computer through a
 * private GitHub repository the owner owns, used as a mailbox on its branch "link":
 *   cmd/NNNNNN.json      a command, written by the developer: {"kind": "sh"|"screenshot"|"get"|"put"|"restart"|"end", ...}
 *   res/NNNNNN.json      its result, written here: {"id","kind","started","ended","status","stdout","stderr","files",...}
 *   res/NNNNNN/NAME      files sent with a result (a screenshot, a file asked for with "get")
 * Commands run one at a time, in order, as root. The owner switches the link on in Settings > Security (rudolfd
 * starts this program), with an administrator's password, for 1 to 8 hours; a banner shows while it runs, and with
 * "Ask first" every command waits for Allow on the screen. See docs/plan-1.4.0.md.
 *
 *   rudolf-devlink run                      the link (started by rudolfd; RUDOLF_TOKEN: rudolfd's API token)
 *   rudolf-devlink check REPO TOKEN-FILE    can the token write to that private repository? ("ok" or why not)
 *
 * Files: DIR = /persist/devlink (kept files) or /run/rudolf/devlink-store (live): config, token (0600, never
 * printed), state (the last command done), log, files/ (files brought with "put"). RUN = /run/rudolf/devlink: pid,
 * status.json, pending.json and answer.NNNNNN (the Allow prompt), stop.
 *
 * GitHub's REST API through libcurl, loaded when needed as rudolf-update does. On a test disc (start-up option
 * rudolf.debug=1) /run/rudolf/devlink-api or DIR/test-api may name a stand-in server instead; the host test sets
 * RUDOLF_DEVLINK_TEST=1 and the RUDOLF_DEVLINK_* variables. */
#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <openssl/evp.h>

#define OUTMAX   (1u << 20)      /* stdout and stderr each, per command */
#define GETMAX   (25u << 20)     /* a file sent with "get" */
#define PUTMAX   (512ull << 20)  /* a file brought with "put" */
#define ASKWAIT  600             /* seconds a command waits for Allow */

static char DIR_[256] = "/persist/devlink", RUND[256] = "/run/rudolf/devlink", API[256] = "https://api.github.com";
static char RDURL[256] = "http://127.0.0.1:8080", REPO[128], TOKEN[256];
static int TESTMODE;
static volatile sig_atomic_t STOP;

/* ---------------- small helpers */
typedef struct { char *b; size_t n, cap; } buf;
static void badd(buf *s, const void *p, size_t n)
{
	if (s->n + n + 1 > s->cap) { size_t c = s->cap ? s->cap : 4096; while (c < s->n + n + 1) c *= 2; char *nb = realloc(s->b, c); if (!nb) return; s->b = nb; s->cap = c; }
	memcpy(s->b + s->n, p, n); s->n += n; s->b[s->n] = 0;
}
static void bfmt(buf *s, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void bfmt(buf *s, const char *fmt, ...)
{
	va_list a; va_start(a, fmt); char *t = NULL; int n = vasprintf(&t, fmt, a); va_end(a);
	if (n > 0) badd(s, t, n);
	free(t);
}
static void bjson(buf *s, const char *v, size_t n)
{
	badd(s, "\"", 1);
	for (size_t i = 0; i < n; i++) {
		unsigned char c = v[i];
		if (c == '"' || c == '\\') { char e[2] = {'\\', c}; badd(s, e, 2); }
		else if (c == '\n') badd(s, "\\n", 2); else if (c == '\r') badd(s, "\\r", 2); else if (c == '\t') badd(s, "\\t", 2);
		else if (c < 0x20 || c == 0x7f) bfmt(s, "\\u%04x", c);
		else badd(s, &v[i], 1);
	}
	badd(s, "\"", 1);
}
static void bjs(buf *s, const char *v) { bjson(s, v ? v : "", v ? strlen(v) : 0); }
static char *slurp(const char *p, size_t *n)
{
	FILE *f = fopen(p, "rb"); if (!f) return NULL;
	buf b = {0}; char t[65536]; size_t k; while ((k = fread(t, 1, sizeof t, f)) > 0) badd(&b, t, k); fclose(f);
	if (!b.b) badd(&b, "", 0);
	if (n) *n = b.n;
	return b.b;
}
static int write_file(const char *p, const void *d, size_t n, int mode)
{
	char t[512]; snprintf(t, sizeof t, "%s.new", p);
	int f = open(t, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode); if (f < 0) return -1;
	if (write(f, d, n) != (ssize_t)n) { close(f); unlink(t); return -1; }
	fsync(f); close(f); return rename(t, p);
}
static void trim(char *s) { size_t n = strlen(s); while (n && strchr("\r\n \t", s[n - 1])) s[--n] = 0; }
static void iso(char *o, size_t n, time_t t) { struct tm g; gmtime_r(&t, &g); strftime(o, n, "%Y-%m-%dT%H:%M:%SZ", &g); }
static void path2(char *o, size_t n, const char *d, const char *f) { snprintf(o, n, "%s/%s", d, f); }

/* a value from a flat JSON object (the commands are flat): string (unescaped) or number, NULL when missing */
static char *jget(const char *j, const char *key)
{
	char k[80]; snprintf(k, sizeof k, "\"%s\"", key);
	for (const char *p = j; (p = strstr(p, k)); p += strlen(k)) {
		const char *q = p + strlen(k); while (isspace((unsigned char)*q)) q++;
		if (*q != ':') continue;
		q++; while (isspace((unsigned char)*q)) q++;
		buf o = {0};
		if (*q == '"') {
			for (q++; *q && *q != '"'; q++) {
				if (*q != '\\') { badd(&o, q, 1); continue; }
				q++;
				switch (*q) {
				case 'n': badd(&o, "\n", 1); break; case 't': badd(&o, "\t", 1); break; case 'r': badd(&o, "\r", 1); break;
				case 'b': badd(&o, "\b", 1); break; case 'f': badd(&o, "\f", 1); break;
				case 'u': {
					unsigned v = 0; if (sscanf(q + 1, "%4x", &v) != 1) break; q += 4;
					/* a surrogate pair */
					if (v >= 0xd800 && v < 0xdc00 && q[1] == '\\' && q[2] == 'u') { unsigned w = 0; if (sscanf(q + 3, "%4x", &w) == 1 && w >= 0xdc00 && w < 0xe000) { v = 0x10000 + ((v - 0xd800) << 10) + (w - 0xdc00); q += 6; } }
					char u[4]; int l;
					if (v < 0x80) { u[0] = v; l = 1; } else if (v < 0x800) { u[0] = 0xc0 | v >> 6; u[1] = 0x80 | (v & 63); l = 2; }
					else if (v < 0x10000) { u[0] = 0xe0 | v >> 12; u[1] = 0x80 | (v >> 6 & 63); u[2] = 0x80 | (v & 63); l = 3; }
					else { u[0] = 0xf0 | v >> 18; u[1] = 0x80 | (v >> 12 & 63); u[2] = 0x80 | (v >> 6 & 63); u[3] = 0x80 | (v & 63); l = 4; }
					badd(&o, u, l); break; }
				case 0: q--; break;
				default: badd(&o, q, 1);
				}
			}
		} else { size_t l = strcspn(q, ",}] \t\r\n"); badd(&o, q, l); }
		if (!o.b) badd(&o, "", 0);
		return o.b;
	}
	return NULL;
}

/* ---------------- HTTP (libcurl via dlopen) */
typedef void CURL;
struct slist;
static struct {
	void *h; int (*global_init)(long); CURL *(*init)(void); int (*setopt)(CURL *, int, ...); int (*perform)(CURL *);
	int (*getinfo)(CURL *, int, ...); void (*cleanup)(CURL *); const char *(*strerror)(int);
	struct slist *(*sl_append)(struct slist *, const char *); void (*sl_free)(struct slist *);
} C;
static int curl_up(void)
{
	if (C.h) return 1;
	if (!(C.h = dlopen("libcurl.so.4", RTLD_NOW))) return 0;
	C.global_init = dlsym(C.h, "curl_global_init"); C.init = dlsym(C.h, "curl_easy_init"); C.setopt = dlsym(C.h, "curl_easy_setopt");
	C.perform = dlsym(C.h, "curl_easy_perform"); C.getinfo = dlsym(C.h, "curl_easy_getinfo"); C.cleanup = dlsym(C.h, "curl_easy_cleanup");
	C.strerror = dlsym(C.h, "curl_easy_strerror"); C.sl_append = dlsym(C.h, "curl_slist_append"); C.sl_free = dlsym(C.h, "curl_slist_free_all");
	if (!C.global_init || !C.init || !C.setopt || !C.perform || !C.getinfo || !C.cleanup || !C.sl_append || !C.sl_free) { C.h = NULL; return 0; }
	C.global_init(3); return 1;
}
/* libcurl's option numbers (curl.h): the base layer has the library, not its headers */
enum { O_URL = 10002, O_WRITEFUNCTION = 20011, O_WRITEDATA = 10001, O_HEADERFUNCTION = 20079, O_HEADERDATA = 10029,
	O_HTTPHEADER = 10023, O_CUSTOMREQUEST = 10036, O_POSTFIELDS = 10015, O_POSTFIELDSIZE_LARGE = 30120,
	O_FOLLOWLOCATION = 52, O_CONNECTTIMEOUT = 78, O_TIMEOUT = 13, O_LOW_SPEED_LIMIT = 19, O_LOW_SPEED_TIME = 20,
	O_USERAGENT = 10018, O_CAINFO = 10065, O_NOSIGNAL = 99, I_RESPONSE_CODE = 0x200002 };
typedef struct { buf *mem; FILE *f; unsigned long long got, max; int over; } sink;
static size_t wcb(char *p, size_t s, size_t n, void *u)
{
	sink *k = u; size_t l = s * n;
	if (k->max && k->got + l > k->max) { k->over = 1; return 0; }
	k->got += l;
	if (k->f) return fwrite(p, 1, l, k->f) == l ? l : 0;
	badd(k->mem, p, l); return l;
}
static size_t hcb(char *p, size_t s, size_t n, void *u)
{
	size_t l = s * n; char *etag = u;
	if (l > 5 && !strncasecmp(p, "ETag:", 5)) { const char *v = p + 5; while (*v == ' ') v++; snprintf(etag, 128, "%.*s", (int)strcspn(v, "\r\n"), v); }
	return l;
}
static char NETERR[256];
/* one request. auth: send the token (only to the API). Returns the HTTP status, 0 when the server was not reached
   (NETERR says why), -1 when the answer was larger than max. */
static long http(const char *method, const char *url, int auth, const char *accept, const char *body, size_t blen,
                 const char *etag_in, char *etag_out, buf *mem, FILE *f, unsigned long long max, long timeout)
{
	if (!curl_up()) { snprintf(NETERR, sizeof NETERR, "libcurl is missing"); return 0; }
	CURL *h = C.init(); if (!h) return 0;
	struct slist *hd = NULL; char t[400];
	snprintf(t, sizeof t, "Accept: %s", accept ? accept : "application/vnd.github+json"); hd = C.sl_append(hd, t);
	hd = C.sl_append(hd, "X-GitHub-Api-Version: 2022-11-28");
	if (auth) { snprintf(t, sizeof t, "Authorization: Bearer %s", TOKEN); hd = C.sl_append(hd, t); memset(t, 0, sizeof t); }
	if (etag_in && *etag_in) { snprintf(t, sizeof t, "If-None-Match: %s", etag_in); hd = C.sl_append(hd, t); }
	if (body) hd = C.sl_append(hd, "Content-Type: application/json");
	buf dummy = {0}; sink k = { mem ? mem : &dummy, f, 0, max, 0 };
	char etag[128] = "";
	C.setopt(h, O_URL, url); C.setopt(h, O_HTTPHEADER, hd); C.setopt(h, O_WRITEFUNCTION, wcb); C.setopt(h, O_WRITEDATA, &k);
	C.setopt(h, O_HEADERFUNCTION, hcb); C.setopt(h, O_HEADERDATA, etag); C.setopt(h, O_NOSIGNAL, 1L);
	C.setopt(h, O_CONNECTTIMEOUT, 20L); C.setopt(h, O_TIMEOUT, timeout); C.setopt(h, O_LOW_SPEED_LIMIT, 1L); C.setopt(h, O_LOW_SPEED_TIME, 60L);
	C.setopt(h, O_USERAGENT, "rudolf-devlink/1");
	if (!access("/etc/ssl/certs/ca-certificates.crt", R_OK)) C.setopt(h, O_CAINFO, "/etc/ssl/certs/ca-certificates.crt");
	/* release files are redirected to another host; the token is never sent there (auth 0 for those) */
	C.setopt(h, O_FOLLOWLOCATION, auth ? 0L : 1L);
	if (strcmp(method, "GET")) C.setopt(h, O_CUSTOMREQUEST, method);
	if (body) { C.setopt(h, O_POSTFIELDS, body); C.setopt(h, O_POSTFIELDSIZE_LARGE, (long long)blen); }
	int r = C.perform(h); long code = 0; C.getinfo(h, I_RESPONSE_CODE, &code);
	C.sl_free(hd); C.cleanup(h); free(dummy.b);
	if (etag_out) snprintf(etag_out, 128, "%s", etag);
	if (k.over) return -1;
	if (r) { snprintf(NETERR, sizeof NETERR, "%s", C.strerror ? C.strerror(r) : "the network failed"); return code ? code : 0; }
	return code;
}

/* ---------------- files and status */
static void status(const char *state, const char *last, const char *err)
{
	char p[300]; path2(p, sizeof p, RUND, "status.json");
	buf s = {0}; bfmt(&s, "{\"state\":"); bjs(&s, state); bfmt(&s, ",\"last\":"); bjs(&s, last); bfmt(&s, ",\"error\":"); bjs(&s, err); bfmt(&s, ",\"at\":%lld}\n", (long long)time(NULL));
	write_file(p, s.b, s.n, 0644); free(s.b);
}
static char *cfg(const char *key)
{
	char p[300]; path2(p, sizeof p, DIR_, "config"); char *c = slurp(p, NULL); if (!c) return NULL;
	char k[64]; snprintf(k, sizeof k, "%s=", key); char *v = NULL;
	for (char *l = strtok(c, "\n"); l; l = strtok(NULL, "\n")) if (!strncmp(l, k, strlen(k))) { v = strdup(l + strlen(k)); break; }
	free(c); return v;
}
static void cfg_set(const char *key, const char *val)
{
	char p[300]; path2(p, sizeof p, DIR_, "config"); char *c = slurp(p, NULL); buf o = {0}; char k[64]; snprintf(k, sizeof k, "%s=", key);
	for (char *l = c ? strtok(c, "\n") : NULL; l; l = strtok(NULL, "\n")) if (strncmp(l, k, strlen(k))) bfmt(&o, "%s\n", l);
	bfmt(&o, "%s%s\n", k, val); write_file(p, o.b, o.n, 0600); free(o.b); free(c);
}
static void logline(const char *id, const char *kind, const char *what, const char *result, const char *approved)
{
	char p[300], t[32]; path2(p, sizeof p, DIR_, "log"); iso(t, sizeof t, time(NULL));
	buf s = {0}; bfmt(&s, "{\"t\":\"%s\",\"id\":", t); bjs(&s, id); bfmt(&s, ",\"kind\":"); bjs(&s, kind);
	char w[301]; snprintf(w, sizeof w, "%s", what ? what : ""); bfmt(&s, ",\"what\":"); bjs(&s, w);
	bfmt(&s, ",\"result\":"); bjs(&s, result); bfmt(&s, ",\"approved\":"); bjs(&s, approved); bfmt(&s, "}\n");
	int f = open(p, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600); if (f >= 0) { if (write(f, s.b, s.n) < 0) {} close(f); } free(s.b);
}
/* the log keeps 30 days */
static void log_prune(void)
{
	char p[300]; path2(p, sizeof p, DIR_, "log"); char *c = slurp(p, NULL); if (!c) return;
	char lim[32]; iso(lim, sizeof lim, time(NULL) - 30 * 86400); buf o = {0};
	for (char *l = strtok(c, "\n"); l; l = strtok(NULL, "\n")) { const char *t = strstr(l, "\"t\":\""); if (t && strncmp(t + 5, lim, 20) < 0) continue; bfmt(&o, "%s\n", l); }
	write_file(p, o.b ? o.b : "", o.n, 0600); free(o.b); free(c);
}
static int id_ok(const char *s) { if (strlen(s) != 6) return 0; for (int i = 0; i < 6; i++) if (!isdigit((unsigned char)s[i])) return 0; return 1; }
static int repo_ok(const char *r)
{
	const char *sl = strchr(r, '/'); if (!sl || sl == r || !sl[1] || strchr(sl + 1, '/') || strlen(r) > 100) return 0;
	for (const char *c = r; *c; c++) if (!isalnum((unsigned char)*c) && !strchr("-_./", *c)) return 0;
	return !strstr(r, "..");
}
static int name_ok(const char *n) { if (!n || !*n || strlen(n) > 100 || *n == '.') return 0; for (const char *c = n; *c; c++) if (!isalnum((unsigned char)*c) && !strchr("-_.+", *c)) return 0; return 1; }

/* ---------------- GitHub */
static char *b64(const unsigned char *d, size_t n)
{
	char *o = malloc(4 * ((n + 2) / 3) + 1); if (!o) return NULL; EVP_EncodeBlock((unsigned char *)o, d, n); return o;
}
/* write a file to the repository's branch "link" (new, or replacing one there) */
static int gh_put(const char *path, const void *d, size_t n, const char *msg)
{
	char url[600]; snprintf(url, sizeof url, "%s/repos/%s/contents/%s", API, REPO, path);
	char *c = b64(d, n); if (!c) return -1;
	char sha[64] = "";
	for (int attempt = 0; attempt < 2; attempt++) {
		buf body = {0}; bfmt(&body, "{\"message\":"); bjs(&body, msg); bfmt(&body, ",\"branch\":\"link\",\"content\":\"%s\"", c);
		if (*sha) bfmt(&body, ",\"sha\":\"%s\"", sha);
		bfmt(&body, "}");
		buf r = {0}; long code = http("PUT", url, 1, NULL, body.b, body.n, NULL, NULL, &r, NULL, 4u << 20, 600);
		free(body.b);
		if (code == 200 || code == 201) { free(r.b); free(c); return 0; }
		free(r.b);
		if (code != 422 && code != 409) { free(c); return -1; }
		/* already there (a result written before a restart): replace it */
		buf m = {0}; char u2[700]; snprintf(u2, sizeof u2, "%s?ref=link", url);
		if (http("GET", u2, 1, NULL, NULL, 0, NULL, NULL, &m, NULL, 4u << 20, 60) != 200) { free(m.b); free(c); return -1; }
		char *s = jget(m.b, "sha"); snprintf(sha, sizeof sha, "%s", s ? s : ""); free(s); free(m.b);
		if (!*sha) { free(c); return -1; }
	}
	free(c); return -1;
}

/* ---------------- running a command */
typedef struct { char id[8], kind[16]; char *json; } cmd;
typedef struct { buf out, err; int status; char statustxt[64], files[4][120]; int nfiles; char error[300]; } result;

static void read_capped(const char *p, buf *b)
{
	FILE *f = fopen(p, "rb"); if (!f) return; char t[65536]; size_t k, tot = 0;
	while ((k = fread(t, 1, sizeof t, f)) > 0) { if (tot + k > OUTMAX) { badd(b, t, OUTMAX - tot); tot = OUTMAX; break; } badd(b, t, k); tot += k; }
	if (tot >= OUTMAX && fgetc(f) != EOF) bfmt(b, "\n[... cut: more than 1 MB of output]");
	fclose(f);
}
static void run_sh(cmd *c, result *r)
{
	char *script = jget(c->json, "script"); char *to = jget(c->json, "timeout");
	long lim = to ? atol(to) : 600; if (lim < 1) lim = 600; if (lim > 3600) lim = 3600; free(to);
	if (!script) { snprintf(r->error, sizeof r->error, "no script"); r->status = -1; return; }
	char sp[300], op[300], ep[300];
	snprintf(sp, sizeof sp, "%s/work/%s.sh", RUND, c->id); snprintf(op, sizeof op, "%s/work/%s.out", RUND, c->id); snprintf(ep, sizeof ep, "%s/work/%s.err", RUND, c->id);
	write_file(sp, script, strlen(script), 0700); free(script);
	pid_t p = fork();
	if (p == 0) {
		setsid();
		int o = open(op, O_WRONLY | O_CREAT | O_TRUNC, 0600), e = open(ep, O_WRONLY | O_CREAT | O_TRUNC, 0600), n = open("/dev/null", O_RDONLY);
		dup2(n, 0); dup2(o, 1); dup2(e, 2);
		if (chdir("/root")) { if (chdir("/")) {} }
		execl("/bin/sh", "sh", sp, (char *)NULL); _exit(127);
	}
	time_t end = time(NULL) + lim; int st = 0, done = 0;
	while (!done) {
		pid_t w = waitpid(p, &st, WNOHANG);
		if (w == p) { done = 1; break; }
		if (time(NULL) >= end || STOP) { kill(-p, SIGKILL); waitpid(p, &st, 0); snprintf(r->statustxt, sizeof r->statustxt, STOP ? "stopped (the link was ended)" : "timeout after %ld s", lim); r->status = -1; break; }
		usleep(200000);
	}
	if (done) { if (WIFEXITED(st)) r->status = WEXITSTATUS(st); else { r->status = 128 + WTERMSIG(st); snprintf(r->statustxt, sizeof r->statustxt, "ended by signal %d", WTERMSIG(st)); } }
	read_capped(op, &r->out); read_capped(ep, &r->err);
	unlink(sp); unlink(op); unlink(ep);
}
static void add_file(result *r, const char *name) { if (r->nfiles < 4) snprintf(r->files[r->nfiles++], 120, "%s", name); }
static void run_screenshot(cmd *c, result *r)
{
	const char *tok = getenv("RUDOLF_TOKEN"); char url[300]; snprintf(url, sizeof url, "%s/api/screenshot", RDURL);
	if (!curl_up()) { snprintf(r->error, sizeof r->error, "libcurl is missing"); r->status = -1; return; }
	CURL *h = C.init(); struct slist *hd = NULL; char t[200]; snprintf(t, sizeof t, "X-Rudolf-Token: %s", tok ? tok : ""); hd = C.sl_append(hd, t);
	buf png = {0}; sink k = { &png, NULL, 0, 64u << 20, 0 };
	C.setopt(h, O_URL, url); C.setopt(h, O_HTTPHEADER, hd); C.setopt(h, O_WRITEFUNCTION, wcb); C.setopt(h, O_WRITEDATA, &k); C.setopt(h, O_TIMEOUT, 60L); C.setopt(h, O_NOSIGNAL, 1L);
	int e = C.perform(h); long code = 0; C.getinfo(h, I_RESPONSE_CODE, &code); C.sl_free(hd); C.cleanup(h);
	if (e || code != 200 || png.n < 8 || memcmp(png.b, "\x89PNG", 4)) { snprintf(r->error, sizeof r->error, "the screenshot failed (%ld)", code); r->status = -1; free(png.b); return; }
	char gp[100]; snprintf(gp, sizeof gp, "res/%s/screen.png", c->id);
	if (gh_put(gp, png.b, png.n, "screenshot")) { snprintf(r->error, sizeof r->error, "the screenshot (%zu bytes) could not be sent: %s", png.n, NETERR); r->status = -1; }
	else { add_file(r, "screen.png"); r->status = 0; }
	free(png.b);
}
static void run_get(cmd *c, result *r)
{
	char *p = jget(c->json, "path"), *home = cfg("home"); struct stat st;
	int homeok = home && *home == '1'; free(home);
	if (!p || p[0] != '/' || strstr(p, "/../")) { snprintf(r->error, sizeof r->error, "give a full path"); r->status = -1; free(p); return; }
	char rp[4096]; if (!realpath(p, rp)) { snprintf(r->error, sizeof r->error, "%s: %s", p, strerror(errno)); r->status = -1; free(p); return; }
	if (!homeok && (!strncmp(rp, "/home/", 6) || !strncmp(rp, "/persist/home/", 14) || !strcmp(rp, "/home"))) { snprintf(r->error, sizeof r->error, "files in home folders are not sent (the owner did not allow it when switching the link on)"); r->status = -1; free(p); return; }
	char tk[300]; path2(tk, sizeof tk, DIR_, "token"); char tr[4096];
	if (realpath(tk, tr) && !strcmp(tr, rp)) { snprintf(r->error, sizeof r->error, "the link's token is never sent"); r->status = -1; free(p); return; }
	if (stat(rp, &st) || !S_ISREG(st.st_mode)) { snprintf(r->error, sizeof r->error, "%s is not a file", p); r->status = -1; free(p); return; }
	if ((unsigned long long)st.st_size > GETMAX) { snprintf(r->error, sizeof r->error, "%s is %lld bytes; at most 25 MB is sent", p, (long long)st.st_size); r->status = -1; free(p); return; }
	size_t n; char *d = slurp(rp, &n); const char *bn = strrchr(rp, '/') + 1; char name[120]; snprintf(name, sizeof name, "%s", name_ok(bn) ? bn : "file");
	char gp[200]; snprintf(gp, sizeof gp, "res/%s/%s", c->id, name);
	if (!d || gh_put(gp, d, n, "file")) { snprintf(r->error, sizeof r->error, "%s could not be sent: %s", p, NETERR); r->status = -1; }
	else { add_file(r, name); r->status = 0; bfmt(&r->out, "%s: %zu bytes\n", rp, n); }
	free(d); free(p);
}
static void run_put(cmd *c, result *r)
{
	char *src = jget(c->json, "src"), *url = jget(c->json, "url"), *name = jget(c->json, "name");
	char fd[300], fp[500]; path2(fd, sizeof fd, DIR_, "files"); mkdir(fd, 0700);
	if (!name_ok(name)) { snprintf(r->error, sizeof r->error, "name: letters, digits and - _ . + only"); r->status = -1; goto out; }
	snprintf(fp, sizeof fp, "%s/%s", fd, name);
	char u[800]; int auth;
	if (src && *src && !strstr(src, "..")) { snprintf(u, sizeof u, "%s/repos/%s/contents/%s?ref=link", API, REPO, src); auth = 1; }
	else if (url && !strncmp(url, "https://github.com/merciless146/RudolfOS/releases/download/", 58) && !strstr(url, "..")) { snprintf(u, sizeof u, "%s", url); auth = 0; }
	else { snprintf(r->error, sizeof r->error, "src (a file in this repository) or url (a file of a Rudolf OS release on GitHub)"); r->status = -1; goto out; }
	char tmp[520]; snprintf(tmp, sizeof tmp, "%s.part", fp); FILE *f = fopen(tmp, "wb");
	if (!f) { snprintf(r->error, sizeof r->error, "%s: %s", tmp, strerror(errno)); r->status = -1; goto out; }
	long code = http("GET", u, auth, auth ? "application/vnd.github.raw" : "*/*", NULL, 0, NULL, NULL, NULL, f, PUTMAX, 3600);
	fclose(f);
	if (code == -1) { unlink(tmp); snprintf(r->error, sizeof r->error, "larger than 512 MB"); r->status = -1; goto out; }
	if (code != 200) { unlink(tmp); snprintf(r->error, sizeof r->error, "the file could not be fetched (HTTP %ld%s%s)", code, *NETERR ? ", " : "", NETERR); r->status = -1; goto out; }
	rename(tmp, fp); struct stat st; stat(fp, &st);
	bfmt(&r->out, "%s: %lld bytes\n", fp, (long long)st.st_size); r->status = 0;
out:
	free(src); free(url); free(name);
}

/* ---------------- Ask first */
static int ask(cmd *c, const char *what)
{
	char pp[300], ap[300]; path2(pp, sizeof pp, RUND, "pending.json"); snprintf(ap, sizeof ap, "%s/answer.%s", RUND, c->id);
	unlink(ap);
	buf s = {0}; bfmt(&s, "{\"id\":\"%s\",\"kind\":", c->id); bjs(&s, c->kind); bfmt(&s, ",\"what\":"); bjs(&s, what); bfmt(&s, ",\"since\":%lld}\n", (long long)time(NULL));
	write_file(pp, s.b, s.n, 0644); free(s.b);
	status("waiting for approval", c->id, "");
	time_t end = time(NULL) + ASKWAIT; int a = 0;
	while (!STOP && time(NULL) < end) {
		char *v = slurp(ap, NULL);
		if (v) { trim(v); a = !strcmp(v, "allow") ? 1 : -1; free(v); break; }
		sleep(1);
	}
	unlink(pp); unlink(ap);
	return a;
}
static void describe(cmd *c, buf *w)
{
	if (!strcmp(c->kind, "sh")) { char *s = jget(c->json, "script"); bfmt(w, "%s", s ? s : ""); free(s); }
	else if (!strcmp(c->kind, "get")) { char *p = jget(c->json, "path"); bfmt(w, "send the file %s", p ? p : "?"); free(p); }
	else if (!strcmp(c->kind, "put")) { char *n = jget(c->json, "name"), *s = jget(c->json, "src"), *u = jget(c->json, "url"); bfmt(w, "bring %s to /persist/devlink/files/%s", s && *s ? s : u ? u : "?", n ? n : "?"); free(n); free(s); free(u); }
	else if (!strcmp(c->kind, "screenshot")) bfmt(w, "take a screenshot");
	else if (!strcmp(c->kind, "restart")) bfmt(w, "restart the computer");
	else if (!strcmp(c->kind, "end")) bfmt(w, "end the development link");
	else bfmt(w, "%s", c->kind);
}

/* the result, written to res/ID.json */
static int report(cmd *c, result *r, time_t t0, const char *approved)
{
	char a[32], b[32]; iso(a, sizeof a, t0); iso(b, sizeof b, time(NULL));
	buf s = {0}; bfmt(&s, "{\"id\":\"%s\",\"kind\":", c->id); bjs(&s, c->kind); bfmt(&s, ",\"started\":\"%s\",\"ended\":\"%s\",\"status\":%d", a, b, r->status);
	if (*r->statustxt) { bfmt(&s, ",\"statusText\":"); bjs(&s, r->statustxt); }
	bfmt(&s, ",\"stdout\":"); bjson(&s, r->out.b ? r->out.b : "", r->out.n); bfmt(&s, ",\"stderr\":"); bjson(&s, r->err.b ? r->err.b : "", r->err.n);
	bfmt(&s, ",\"files\":["); for (int i = 0; i < r->nfiles; i++) { if (i) bfmt(&s, ","); bjs(&s, r->files[i]); } bfmt(&s, "]");
	if (*r->error) { bfmt(&s, ",\"error\":"); bjs(&s, r->error); }
	bfmt(&s, ",\"approved\":"); bjs(&s, approved); bfmt(&s, "}\n");
	char gp[64]; snprintf(gp, sizeof gp, "res/%s.json", c->id);
	int rc = -1;
	for (int i = 0; i < 5 && rc && !STOP; i++) { rc = gh_put(gp, s.b, s.n, "result"); if (rc) sleep(5 << i); }
	free(s.b); return rc;
}

static void done_upto(const char *id) { char p[300]; path2(p, sizeof p, DIR_, "state"); char l[64]; snprintf(l, sizeof l, "last=%s\n", id); write_file(p, l, strlen(l), 0600); }
static void rd_post(const char *what)
{
	const char *tok = getenv("RUDOLF_TOKEN"); char url[300]; snprintf(url, sizeof url, "%s/api/%s", RDURL, what);
	if (!curl_up()) return;
	CURL *h = C.init(); struct slist *hd = NULL; char t[200]; snprintf(t, sizeof t, "X-Rudolf-Token: %s", tok ? tok : ""); hd = C.sl_append(hd, t);
	buf m = {0}; sink k = { &m, NULL, 0, 1 << 20, 0 };
	C.setopt(h, O_URL, url); C.setopt(h, O_HTTPHEADER, hd); C.setopt(h, O_CUSTOMREQUEST, "POST"); C.setopt(h, O_POSTFIELDS, ""); C.setopt(h, O_WRITEFUNCTION, wcb); C.setopt(h, O_WRITEDATA, &k); C.setopt(h, O_TIMEOUT, 30L); C.setopt(h, O_NOSIGNAL, 1L);
	C.perform(h); C.sl_free(hd); C.cleanup(h); free(m.b);
}

/* 1 = go on, 0 = the link ends here (end, restart) */
static int execute(cmd *c)
{
	result r; memset(&r, 0, sizeof r);
	buf w = {0}; describe(c, &w);
	char *ask_cfg = cfg("ask"); int askfirst = !ask_cfg || *ask_cfg != '0'; free(ask_cfg);
	const char *approved = "not asked (Ask first was off)";
	if (askfirst) {
		int a = ask(c, w.b ? w.b : "");
		if (a <= 0) {
			snprintf(r.error, sizeof r.error, a < 0 ? "refused on the computer" : STOP ? "the link was ended before it was allowed" : "not allowed within 10 minutes");
			r.status = -1; approved = a < 0 ? "refused" : "no answer";
			logline(c->id, c->kind, w.b, r.error, approved); report(c, &r, time(NULL), approved); done_upto(c->id); free(w.b); return 1;
		}
		approved = "allowed on the computer";
	}
	char running[64]; snprintf(running, sizeof running, "running %s", c->id); status(running, c->id, "");
	{ char p[300]; path2(p, sizeof p, DIR_, "state"); char l[80]; char *old = slurp(p, NULL); char last[16] = "000000"; if (old) { char *q = strstr(old, "last="); if (q) snprintf(last, sizeof last, "%.6s", q + 5); free(old); }
	  snprintf(l, sizeof l, "last=%s\nrunning=%s\n", last, c->id); write_file(p, l, strlen(l), 0600); }
	time_t t0 = time(NULL); int go = 1;
	if (!strcmp(c->kind, "sh")) run_sh(c, &r);
	else if (!strcmp(c->kind, "screenshot")) run_screenshot(c, &r);
	else if (!strcmp(c->kind, "get")) run_get(c, &r);
	else if (!strcmp(c->kind, "put")) run_put(c, &r);
	else if (!strcmp(c->kind, "restart")) { r.status = 0; go = 0; }
	else if (!strcmp(c->kind, "end")) { r.status = 0; go = 0; }
	else { snprintf(r.error, sizeof r.error, "unknown kind: sh, screenshot, get, put, restart or end"); r.status = -1; }
	char res[400]; snprintf(res, sizeof res, "%s%s%d%s%s", *r.error ? r.error : "", *r.error ? " (" : "status ", r.status, *r.statustxt ? ", " : "", *r.statustxt ? r.statustxt : "");
	if (*r.error) strcat(res, ")");
	logline(c->id, c->kind, w.b, res, approved); free(w.b);
	done_upto(c->id);
	if (report(c, &r, t0, approved)) status("on", c->id, "the result could not be sent to GitHub");
	free(r.out.b); free(r.err.b);
	if (!strcmp(c->kind, "end")) { cfg_set("on", "0"); status("ended (an end command)", c->id, ""); }
	if (!strcmp(c->kind, "restart")) { status("restarting", c->id, ""); sync(); rd_post("power?action=reboot"); sleep(30); }
	return go;
}

/* after a restart in the middle of a command: say so instead of running it again */
static void interrupted(void)
{
	char p[300]; path2(p, sizeof p, DIR_, "state"); char *s = slurp(p, NULL); if (!s) return;
	char *q = strstr(s, "running="); if (!q) { free(s); return; }
	cmd c; memset(&c, 0, sizeof c); snprintf(c.id, sizeof c.id, "%.6s", q + 8); snprintf(c.kind, sizeof c.kind, "?"); free(s);
	if (!id_ok(c.id)) return;
	result r; memset(&r, 0, sizeof r); r.status = -1; snprintf(r.error, sizeof r.error, "interrupted: the computer restarted or the link stopped while it ran; not run again");
	logline(c.id, c.kind, "", r.error, "-");
	if (!report(&c, &r, time(NULL), "-")) done_upto(c.id);
}

static int cmp(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
static void onsig(int s) { (void)s; STOP = 1; }

static int load(void)
{
	char *r = cfg("repo"); if (!r || !repo_ok(r)) { free(r); return -1; } snprintf(REPO, sizeof REPO, "%s", r); free(r);
	char p[300]; path2(p, sizeof p, DIR_, "token"); char *t = slurp(p, NULL); if (!t) return -1; trim(t); snprintf(TOKEN, sizeof TOKEN, "%s", t);
	memset(t, 0, strlen(t)); free(t); return *TOKEN ? 0 : -1;
}
static int expired(void) { char *u = cfg("until"); long long v = u ? atoll(u) : 0; free(u); return time(NULL) >= v; }
static int stop_asked(void) { char p[300]; path2(p, sizeof p, RUND, "stop"); return !access(p, F_OK); }

/* the ids that have a result in res/ (NNNNNN.json); -1 when the listing could not be read (then nothing is skipped) */
static int results_done(char (**out)[8])
{
	char url[400]; snprintf(url, sizeof url, "%s/repos/%s/contents/res?ref=link", API, REPO);
	buf m = {0}; long code = http("GET", url, 1, NULL, NULL, 0, NULL, NULL, &m, NULL, 4u << 20, 60);
	int n = 0, cap = 64; char (*d)[8] = malloc(cap * sizeof *d);
	if (code == 200 && d) for (char *q = m.b; q && (q = strstr(q, "\"name\"")); q += 6) {
		char *v = jget(q, "name");
		if (v && strlen(v) == 11 && !strcmp(v + 6, ".json")) { v[6] = 0; if (id_ok(v)) { if (n == cap) { cap *= 2; char (*nd)[8] = realloc(d, cap * sizeof *d); if (!nd) { free(v); break; } d = nd; } snprintf(d[n++], 8, "%s", v); } }
		free(v);
	}
	free(m.b); *out = d;
	return code == 200 || code == 404 ? n : -1;
}

static int link_run(void)
{
	signal(SIGTERM, onsig); signal(SIGINT, onsig); signal(SIGPIPE, SIG_IGN);
	mkdir(RUND, 0755); char wd[300]; snprintf(wd, sizeof wd, "%s/work", RUND); mkdir(wd, 0700);
	{ char p[300]; path2(p, sizeof p, RUND, "pid"); char b[32]; snprintf(b, sizeof b, "%d\n", getpid()); write_file(p, b, strlen(b), 0644); }
	char *on = cfg("on"); int ison = on && *on == '1'; free(on);
	if (!ison || load()) { status("off", "", "not switched on"); return 1; }
	log_prune(); interrupted();
	char etag[128] = "", url[400]; snprintf(url, sizeof url, "%s/repos/%s/contents/cmd?ref=link", API, REPO);
	int back = 5; status("on", "", "");
	while (!STOP) {
		if (stop_asked()) { status("off", "", ""); break; }
		if (expired()) { cfg_set("on", "0"); status("ended (its time was up)", "", ""); break; }
		buf m = {0}; char ne[128] = ""; NETERR[0] = 0;
		long code = http("GET", url, 1, NULL, NULL, 0, etag, ne, &m, NULL, 4u << 20, 60);
		if (code == 0 || code >= 500 || code == 429 || code == 403) {
			char e[300]; if (code) snprintf(e, sizeof e, "GitHub answered %ld", code); else snprintf(e, sizeof e, "%s", NETERR);
			status("waiting for the network", "", e); free(m.b);
			for (int i = 0; i < back && !STOP && !stop_asked(); i++) sleep(1);
			back = back < 120 ? back * 2 : 120; continue;
		}
		back = 5;
		if (code == 401) { status("stopped", "", "GitHub refused the token (revoked or expired): switch the link on again with a new one"); free(m.b); cfg_set("on", "0"); break; }
		if (code == 200) {
			snprintf(etag, sizeof etag, "%s", ne);
			char *names[4096]; int n = 0;
			for (char *q = m.b; q && n < 4096 && (q = strstr(q, "\"name\"")); q += 6) {
				char *v = jget(q, "name"); if (v && strlen(v) == 11 && !strcmp(v + 6, ".json")) { v[6] = 0; if (id_ok(v)) { names[n++] = v; continue; } }
				free(v);
			}
			qsort(names, n, sizeof *names, cmp);
			char sp[300]; path2(sp, sizeof sp, DIR_, "state"); char *st = slurp(sp, NULL); char last[8] = "000000"; if (st) { char *q = strstr(st, "last="); if (q) snprintf(last, sizeof last, "%.6s", q + 5); free(st); }
			int go = 1;
			/* 1.4.1: commands that already have a result in the repository are done, whatever this data partition
			   remembers (on another one, or after the record was lost, every command since 000001 ran again) */
			char (*done)[8] = NULL; int nd = -1;
			for (int i = 0; i < n && nd < 0; i++) if (strcmp(names[i], last) > 0) nd = results_done(&done);
			for (int i = 0; i < n; i++) {
				int had = 0; for (int k = 0; k < nd; k++) if (!strcmp(done[k], names[i])) had = 1;
				if (had && strcmp(names[i], last) > 0) { snprintf(last, sizeof last, "%s", names[i]); done_upto(last); }
				if (go && !STOP && strcmp(names[i], last) > 0 && !expired() && !stop_asked()) {
					char cu[500]; snprintf(cu, sizeof cu, "%s/repos/%s/contents/cmd/%s.json?ref=link", API, REPO, names[i]);
					buf j = {0}; long cc = http("GET", cu, 1, "application/vnd.github.raw", NULL, 0, NULL, NULL, &j, NULL, 4u << 20, 120);
					if (cc == 200 && j.b) {
						cmd c; memset(&c, 0, sizeof c); snprintf(c.id, sizeof c.id, "%s", names[i]); c.json = j.b;
						char *k = jget(j.b, "kind"); snprintf(c.kind, sizeof c.kind, "%s", k ? k : "?"); free(k);
						go = execute(&c); snprintf(last, sizeof last, "%s", names[i]);
					} else { free(j.b); status("waiting for the network", "", "a command could not be fetched"); free(names[i]); for (int k2 = i + 1; k2 < n; k2++) free(names[k2]); n = 0; break; }
					free(j.b);
				}
				free(names[i]);
			}
			free(done);
			if (!go) break;
			status("on", last, "");
		} else if (code != 304) {
			/* 404: no branch "link" or no cmd folder yet */
			status("on", "", code == 404 ? "no commands yet (branch link, folder cmd)" : "GitHub answered unexpectedly");
		}
		free(m.b);
		for (int i = 0; i < 5 && !STOP && !stop_asked(); i++) sleep(1);
	}
	char p[300]; path2(p, sizeof p, RUND, "pid"); unlink(p);
	memset(TOKEN, 0, sizeof TOKEN);
	return 0;
}

/* check REPO TOKEN-FILE: the repository must be private and the token able to write to it */
static int link_check(const char *repo, const char *tf)
{
	if (!repo_ok(repo)) { puts("the repository is written as owner/name"); return 2; }
	char *t = slurp(tf, NULL); if (!t) { puts("no token"); return 2; } trim(t);
	if (!*t || strlen(t) >= sizeof TOKEN || strspn(t, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != strlen(t)) { puts("that is not a GitHub token (letters, digits and _ only)"); free(t); return 2; }
	snprintf(TOKEN, sizeof TOKEN, "%s", t); memset(t, 0, strlen(t)); free(t); snprintf(REPO, sizeof REPO, "%s", repo);
	char url[400]; snprintf(url, sizeof url, "%s/repos/%s", API, REPO); buf m = {0};
	long code = http("GET", url, 1, NULL, NULL, 0, NULL, NULL, &m, NULL, 4u << 20, 60);
	int rc = 1;
	if (code == 0) printf("GitHub could not be reached (%s)\n", NETERR);
	else if (code == 401) puts("GitHub refused the token: it is wrong, expired or revoked");
	else if (code == 404 || code == 403) puts("the repository was not found, or the token cannot see it");
	else if (code != 200) printf("GitHub answered %ld\n", code);
	else {
		char *pv = jget(m.b, "private"), *push = NULL; char *perm = m.b ? strstr(m.b, "\"permissions\"") : NULL;
		if (perm) push = jget(perm, "push");
		if (!pv || strcmp(pv, "true")) puts("the repository is public: use a private one, so nobody else can read what is run here");
		else if (push && strcmp(push, "true")) puts("the token cannot write to it: give it Contents: Read and write");
		else { puts("ok"); rc = 0; }
		free(pv); free(push);
	}
	free(m.b); memset(TOKEN, 0, sizeof TOKEN); return rc;
}

static void setup(void)
{
	if (access("/run/rudolf/persistent", F_OK)) snprintf(DIR_, sizeof DIR_, "/run/rudolf/devlink-store");
	char *cl = slurp("/etc/rudolf/cmdline", NULL);
	int dbg = cl && strstr(cl, "rudolf.debug=1"); free(cl);
	if (getenv("RUDOLF_DEVLINK_TEST") && !strcmp(getenv("RUDOLF_DEVLINK_TEST"), "1")) TESTMODE = 1;
	if (TESTMODE) {
		const char *v;
		if ((v = getenv("RUDOLF_DEVLINK_API"))) snprintf(API, sizeof API, "%s", v);
		if ((v = getenv("RUDOLF_DEVLINK_DIR"))) snprintf(DIR_, sizeof DIR_, "%s", v);
		if ((v = getenv("RUDOLF_DEVLINK_RUN"))) snprintf(RUND, sizeof RUND, "%s", v);
		if ((v = getenv("RUDOLF_DEVLINK_RD"))) snprintf(RDURL, sizeof RDURL, "%s", v);
	}
	mkdir(DIR_, 0700); chmod(DIR_, 0700);
	if (!TESTMODE && dbg) {
		/* a test disc: a stand-in for GitHub, named by the test (DIR/test-api lasts across a restart) */
		char tp[300]; path2(tp, sizeof tp, DIR_, "test-api");
		char *a = slurp("/run/rudolf/devlink-api", NULL); if (!a) a = slurp(tp, NULL);
		if (a) { trim(a); if (!strncmp(a, "http", 4)) snprintf(API, sizeof API, "%s", a); free(a); }
	}
}

int main(int argc, char **argv)
{
	setup();
	if (argc == 2 && !strcmp(argv[1], "run")) return link_run();
	if (argc == 4 && !strcmp(argv[1], "check")) return link_check(argv[2], argv[3]);
	fprintf(stderr, "usage: rudolf-devlink run | check REPO TOKEN-FILE\n"); return 2;
}
