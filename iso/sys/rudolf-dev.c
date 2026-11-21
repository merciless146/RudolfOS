/* rudolf-dev - GitHub for Blitzen as a developer's agent, and the devlink command (1.6.0, docs/plan-1.6.0.md Part 19
 * item 4; docs/DEVLINK.md). GPL-2.0-or-later.
 *
 * As rudolf-dev (run by rudolfd for the desktop, as nobody; the token is the first line of standard input, never an
 * argument). R is OWNER/NAME:
 *   repo R                          the repository (GitHub's JSON)
 *   runs R                          its 15 newest workflow runs        jobs R RUN    a run's jobs
 *   joblog R JOB                    a job's log (its last 64 KB)       workflows R   its workflows
 *   file R PATH [REF]               a file (at most 1 MB)
 *   dispatch R WORKFLOW REF         starts a workflow (one with workflow_dispatch)
 *   push R BASE BRANCH MESSAGE      a NEW branch BRANCH from BASE with the files given after the token, one a line:
 *                                   PATH<tab>BASE64 (never the default branch, never a branch already there)
 *   pr R BRANCH BASE TITLE          opens a pull request (its body: the rest of standard input)
 *   dl-send R                       a devlink command (its JSON, the second line of standard input): prints its id
 *   dl-wait R ID SECS               its result (GitHub's res/ID.json), or {"waiting":true} after SECS seconds
 *   dl-file R ID NAME               a file sent with a result (raw)
 * Nothing here merges, releases, deletes or changes settings or secrets: there is no command for it.
 * 1.6.1 (docs/plan-1.6.1.md Part 26): and nothing here gives a secret away, whatever it is asked (rudolf-secrets.h; rudolfd
 * checks the same before it runs this): a key file is neither read (file), pushed (push) nor asked of the test computer
 * (dl-send's get, a script naming one); a push, a pull request or a devlink command that carries a private key, a token
 * or the token it was given is refused (exit 5); what it reads (a file, a job's log) is printed redacted.
 *
 * As devlink (/usr/bin/devlink, a link to this): the same command line as iso/systools/devlink.py (run, send, wait,
 * last, next), for driving another computer's development link from Rudolf OS. The token: DEVLINK_TOKEN or
 * ~/.config/rudolf-devlink/token; the repository: --repo or DEVLINK_REPO. DEVLINK_API (or RUDOLF_DEV_API): another API. */
#define _GNU_SOURCE
#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <openssl/evp.h>
#include "rudolf-secrets.h"   /* 1.6.1 (plan-1.6.1 Part 26): the secret detector */

static char API[300] = "https://api.github.com", TOK[512];
__attribute__((noreturn, format(printf, 2, 3))) static void die(int c, const char *f, ...) { va_list a; va_start(a, f); fputs("rudolf-dev: ", stderr); vfprintf(stderr, f, a); fputc('\n', stderr); va_end(a); exit(c); }

/* ---------------- libcurl, as rudolf-ai and rudolf-devlink load it */
typedef void CURL; struct curl_slist;
static struct { void *h; CURL *(*init)(void); int (*setopt)(CURL *, int, ...); int (*perform)(CURL *); int (*getinfo)(CURL *, int, ...); void (*cleanup)(CURL *);
	struct curl_slist *(*sl_append)(struct curl_slist *, const char *); void (*sl_free)(struct curl_slist *); } C;
typedef struct { char *b; size_t n, cap, max; } buf;
static void badd(buf *b, const void *p, size_t n)
{
	if (b->n + n + 1 > b->cap) { b->cap = (b->n + n + 1) * 2; char *nb = realloc(b->b, b->cap); if (!nb) die(1, "out of memory"); b->b = nb; }
	memcpy(b->b + b->n, p, n); b->n += n; b->b[b->n] = 0;
}
static void bprintf(buf *b, const char *f, ...) { va_list a; va_start(a, f); char *s = NULL; int n = vasprintf(&s, f, a); va_end(a); if (n < 0) die(1, "out of memory"); badd(b, s, n); free(s); }
static size_t wcb(char *p, size_t s, size_t n, void *u) { buf *b = u; if (b->max && b->n + s * n > b->max) return 0; badd(b, p, s * n); return s * n; }
/* (a redirect, as a job's log is, is followed here without the token: it goes to another address) */
static int http_url(const char *method, const char *url0, const char *body, buf *out, int raw, int auth);
static int http(const char *method, const char *path, const char *body, buf *out, int raw)
{
	char url[2048]; snprintf(url, sizeof url, "%s%s", API, path); return http_url(method, url, body, out, raw, 1);
}
static int http_url(const char *method, const char *url0, const char *body, buf *out, int raw, int auth)
{
	if (!C.h) { if (!(C.h = dlopen("libcurl.so.4", RTLD_NOW))) die(1, "libcurl.so.4 is missing");
		int (*gi)(long) = dlsym(C.h, "curl_global_init"); C.init = dlsym(C.h, "curl_easy_init"); C.setopt = dlsym(C.h, "curl_easy_setopt"); C.perform = dlsym(C.h, "curl_easy_perform");
		C.getinfo = dlsym(C.h, "curl_easy_getinfo"); C.cleanup = dlsym(C.h, "curl_easy_cleanup"); C.sl_append = dlsym(C.h, "curl_slist_append"); C.sl_free = dlsym(C.h, "curl_slist_free_all"); if (gi) gi(3); }
	char url[2048]; snprintf(url, sizeof url, "%s", url0);
	for (int attempt = 0; attempt < 4; attempt++) {
		CURL *h = C.init(); if (!h) die(1, "libcurl did not start");
		struct curl_slist *hd = NULL; char a[600]; snprintf(a, sizeof a, "Authorization: Bearer %s", TOK);
		if (auth) hd = C.sl_append(hd, a);
		hd = C.sl_append(hd, raw ? "Accept: application/vnd.github.raw" : "Accept: application/vnd.github+json");
		hd = C.sl_append(hd, "X-GitHub-Api-Version: 2022-11-28"); hd = C.sl_append(hd, "User-Agent: rudolf-dev");
		if (body) hd = C.sl_append(hd, "Content-Type: application/json");
		out->n = 0; if (out->b) out->b[0] = 0;
		C.setopt(h, 10002, url); C.setopt(h, 10023, hd); C.setopt(h, 10036, method); C.setopt(h, 20011, wcb); C.setopt(h, 10001, out);
		C.setopt(h, 52, 0L); C.setopt(h, 13, 120L); C.setopt(h, 78, 20L);
		if (body) C.setopt(h, 10015, body);
		if (!access("/etc/ssl/certs/ca-certificates.crt", R_OK)) C.setopt(h, 10065, "/etc/ssl/certs/ca-certificates.crt");
		int r = C.perform(h); long code = 0; C.getinfo(h, 0x200002, &code); char *loc = NULL, where[2048] = "";
		if (code >= 301 && code <= 308 && code != 304 && !C.getinfo(h, 0x100000 + 31, &loc) && loc) snprintf(where, sizeof where, "%s", loc);
		C.cleanup(h); C.sl_free(hd);
		if (*where) { if (!strncmp(where, "https://", 8) || (!strncmp(API, "http://127.0.0.1", 16) && !strncmp(where, "http://127.0.0.1", 16))) return http_url("GET", where, NULL, out, raw, 0); return (int)code; }
		if (!out->b) badd(out, "", 0);
		if (r && !code) { if (attempt < 3) { sleep(1 << attempt); continue; } die(1, "GitHub could not be reached"); }
		if ((code >= 500 || code == 429) && attempt < 3) { sleep(1 << attempt); continue; }
		return (int)code;
	}
	return 0;
}
/* ---------------- a little JSON */
static void jstr(buf *b, const char *s)
{
	badd(b, "\"", 1);
	for (; *s; s++) { unsigned char c = *s; if (c == '"' || c == '\\') { char e[2] = { '\\', c }; badd(b, e, 2); } else if (c == '\n') badd(b, "\\n", 2); else if (c == '\t') badd(b, "\\t", 2);
		else if (c == '\r') badd(b, "\\r", 2); else if (c < 0x20) bprintf(b, "\\u%04x", c); else badd(b, &c, 1); }
	badd(b, "\"", 1);
}
/* the first "KEY": value (a string, unescaped simply, or a number/true/false) in J, or "" */
static const char *jget(const char *j, const char *key)
{
	static char v[4][1024]; static int k; char *o = v[k++ & 3]; *o = 0; if (!j) return o;
	char pat[100]; snprintf(pat, sizeof pat, "\"%s\":", key); const char *p = strstr(j, pat);
	if (!p) { snprintf(pat, sizeof pat, "\"%s\": ", key); p = strstr(j, pat); } if (!p) return o;
	p += strlen(pat); while (*p == ' ') p++; size_t n = 0;
	if (*p == '"') { p++; while (*p && *p != '"' && n < 1023) { if (*p == '\\' && p[1]) { p++; o[n++] = *p == 'n' ? '\n' : *p; p++; continue; } o[n++] = *p++; } }
	else while (*p && !strchr(",}] \n", *p) && n < 1023) o[n++] = *p++;
	o[n] = 0; return o;
}
static int repo_ok(const char *r)
{
	if (!r || !*r || strlen(r) > 140 || strstr(r, "..")) return 0; const char *s = strchr(r, '/'); if (!s || s == r || !s[1] || strchr(s + 1, '/')) return 0;
	for (const char *c = r; *c; c++) if (!isalnum((unsigned char)*c) && !strchr("/._-", *c)) return 0; return 1;
}
static int ref_ok(const char *b) { if (!b || !*b || strlen(b) > 200 || strstr(b, "..") || b[0] == '/' || b[0] == '-') return 0; for (const char *c = b; *c; c++) if (!isalnum((unsigned char)*c) && !strchr("/._-", *c)) return 0; return 1; }
static int path_ok(const char *p) { if (!p || !*p || strlen(p) > 400 || p[0] == '/' || strstr(p, "..")) return 0; for (const char *c = p; *c; c++) if ((unsigned char)*c < 0x20 || strchr("?#\\", *c)) return 0; return 1; }
static void urlenc(buf *b, const char *s) { for (; *s; s++) { unsigned char c = *s; if (isalnum(c) || strchr("/-_.~", c)) badd(b, &c, 1); else bprintf(b, "%%%02X", c); } }
static char *b64(const unsigned char *d, size_t n) { char *o = malloc(4 * ((n + 2) / 3) + 1); if (!o) die(1, "out of memory"); EVP_EncodeBlock((unsigned char *)o, d, (int)n); return o; }
static void put(const buf *b) { fwrite(b->b, 1, b->n, stdout); if (!b->n || b->b[b->n - 1] != '\n') fputc('\n', stdout); }
/* 1.6.1 (Part 26): what goes out carries no secret (the token given included); what is printed is redacted */
static void guard_out(const char *s, size_t n, const char *what)
{
	int k = sd_kinds(s, n, SD_SECRET); if (k) die(5, "refused: %s would carry %s. A private key or a token is never sent.", what, sd_label(k));
}
static void put_redacted(const char *s, size_t n) { int k = 0; size_t on; char *r = sd_redact_n(s, n, SD_SHOWN, &k, &on); fwrite(r, 1, on, stdout); free(r); }
static int fail(int code, const buf *b, const char *what)
{
	const char *m = jget(b->b, "message");
	fprintf(stderr, "rudolf-dev: %s: GitHub answered %d%s%s\n", what, code, *m ? ": " : "", m);
	return code == 401 ? 3 : code == 403 || code == 404 ? 4 : 1;
}

/* ---------------- GitHub for Blitzen */
static int get_to(const char *path, int raw, size_t max, const char *what)
{
	buf b = { .max = max }; int c = http("GET", path, NULL, &b, raw); if (c != 200) return fail(c, &b, what);
	put(&b); return 0;
}
static int cmd_push(const char *R, const char *base, const char *br, const char *msg)
{
	if (!ref_ok(base) || !ref_ok(br)) die(2, "a branch is letters, digits and / . _ -");
	if (!strcmp(base, br)) die(2, "the new branch cannot be the one it starts from");
	buf b = {0}, p = {0}; int c;
	bprintf(&p, "/repos/%s", R); c = http("GET", p.b, NULL, &b, 0); if (c != 200) return fail(c, &b, "the repository");
	if (!strcmp(jget(b.b, "default_branch"), br)) die(5, "never to the default branch (%s): a new branch only", br);
	p.n = 0; bprintf(&p, "/repos/%s/git/ref/heads/", R); urlenc(&p, br); c = http("GET", p.b, NULL, &b, 0);
	if (c == 200) die(5, "the branch %s is already there: a new branch only", br);
	p.n = 0; bprintf(&p, "/repos/%s/git/ref/heads/", R); urlenc(&p, base); c = http("GET", p.b, NULL, &b, 0); if (c != 200) return fail(c, &b, "the branch it starts from");
	char sha[64]; snprintf(sha, sizeof sha, "%s", jget(strstr(b.b, "\"object\"") ? strstr(b.b, "\"object\"") : b.b, "sha"));
	/* the files first (all read before anything is written) */
	char line[1 << 16]; int nf = 0; char **paths = NULL, **conts = NULL;
	while (fgets(line, sizeof line, stdin)) { line[strcspn(line, "\r\n")] = 0; if (!*line) continue; char *t = strchr(line, '\t'); if (!t) die(2, "PATH<tab>BASE64 a line");
		*t = 0; if (!path_ok(line)) die(2, "a path inside the repository: %s", line);
		if (sd_keypath(line)) die(5, "refused: %s is a key file; key files are never pushed", line);
		{ size_t L = strlen(t + 1); unsigned char *d = malloc(L + 4); long dn = d ? sd_b64dec(t + 1, L, d, L + 4) : -1; if (dn > 0) guard_out((char *)d, dn, line); if (d) { memset(d, 0, L + 4); free(d); } }
		if (strstr(line, ".github/workflows/") == line) die(5, "not the repository's workflows (%s): a person changes those", line);
		paths = realloc(paths, (nf + 1) * sizeof *paths); conts = realloc(conts, (nf + 1) * sizeof *conts); paths[nf] = strdup(line); conts[nf] = strdup(t + 1); nf++; if (nf > 20) die(2, "at most 20 files"); }
	if (!nf) die(2, "no files");
	guard_out(msg, strlen(msg), "the commit's message");
	buf body = {0}; bprintf(&body, "{\"ref\":\"refs/heads/%s\",\"sha\":\"%s\"}", br, sha);
	p.n = 0; bprintf(&p, "/repos/%s/git/refs", R); c = http("POST", p.b, body.b, &b, 0); if (c != 201) return fail(c, &b, "the new branch");
	for (int i = 0; i < nf; i++) {
		p.n = 0; bprintf(&p, "/repos/%s/contents/", R); urlenc(&p, paths[i]); bprintf(&p, "?ref="); urlenc(&p, br);
		c = http("GET", p.b, NULL, &b, 0); char fsha[64] = ""; if (c == 200) snprintf(fsha, sizeof fsha, "%s", jget(b.b, "sha"));
		body.n = 0; bprintf(&body, "{\"message\":"); jstr(&body, msg); bprintf(&body, ",\"branch\":"); jstr(&body, br);
		bprintf(&body, ",\"content\":\"%s\"%s%s%s}", conts[i], *fsha ? ",\"sha\":\"" : "", fsha, *fsha ? "\"" : "");
		p.n = 0; bprintf(&p, "/repos/%s/contents/", R); urlenc(&p, paths[i]);
		c = http("PUT", p.b, body.b, &b, 0); if (c != 200 && c != 201) return fail(c, &b, paths[i]);
	}
	printf("{\"ok\":true,\"branch\":"); buf o = {0}; jstr(&o, br); put(&o); fflush(stdout);
	return 0;
}
static int cmd_pr(const char *R, const char *br, const char *base, const char *title)
{
	if (!ref_ok(base) || !ref_ok(br)) die(2, "a branch is letters, digits and / . _ -");
	buf in = {0}; char tmp[4096]; size_t k; while ((k = fread(tmp, 1, sizeof tmp, stdin)) > 0) { badd(&in, tmp, k); if (in.n > 60000) break; }
	buf body = {0}, b = {0}, p = {0};
	guard_out(title, strlen(title), "the pull request's title"); if (in.b) guard_out(in.b, in.n, "the pull request's text");
	bprintf(&body, "{\"title\":"); jstr(&body, title); bprintf(&body, ",\"head\":"); jstr(&body, br); bprintf(&body, ",\"base\":"); jstr(&body, base);
	bprintf(&body, ",\"body\":"); jstr(&body, in.b ? in.b : ""); bprintf(&body, "}");
	bprintf(&p, "/repos/%s/pulls", R); int c = http("POST", p.b, body.b, &b, 0); if (c != 201) return fail(c, &b, "the pull request");
	buf o = {0}; bprintf(&o, "{\"ok\":true,\"number\":%s,\"url\":", jget(b.b, "number")); jstr(&o, jget(b.b, "html_url")); bprintf(&o, "}"); put(&o); return 0;
}

/* ---------------- the devlink (docs/DEVLINK.md) */
static int dl_max(const char *R, const char *folder)
{
	buf b = {0}, p = {0}; bprintf(&p, "/repos/%s/contents/%s?ref=link", R, folder); int c = http("GET", p.b, NULL, &b, 0), m = 0;
	if (c == 404) return 0; if (c == 401) die(3, "GitHub refused the token: it is wrong, expired or revoked"); if (c != 200) die(1, "could not list %s/ (GitHub answered %d)", folder, c);
	for (const char *s = b.b; (s = strstr(s, "\"name\":")); s += 7) { const char *v = s + 7; while (*v == ' ' || *v == '"') v++;
		if (strlen(v) > 11 && isdigit((unsigned char)v[0]) && strspn(v, "0123456789") == 6 && !strncmp(v + 6, ".json\"", 6)) { int n = atoi(v); if (n > m) m = n; } }
	return m;
}
static int dl_next(const char *R) { int a = dl_max(R, "cmd"), b = dl_max(R, "res"); return (a > b ? a : b) + 1; }
static int dl_put_file(const char *R, const char *path, const unsigned char *d, size_t n, const char *msg)
{
	buf b = {0}, p = {0}, body = {0}; bprintf(&p, "/repos/%s/contents/%s?ref=link", R, path); int c = http("GET", p.b, NULL, &b, 0);
	char sha[64] = ""; if (c == 200) snprintf(sha, sizeof sha, "%s", jget(b.b, "sha"));
	char *e = b64(d, n); bprintf(&body, "{\"message\":"); jstr(&body, msg); bprintf(&body, ",\"branch\":\"link\",\"content\":\"%s\"", e); free(e);
	if (*sha) bprintf(&body, ",\"sha\":\"%s\"", sha); bprintf(&body, "}");
	p.n = 0; bprintf(&p, "/repos/%s/contents/%s", R, path); return http("PUT", p.b, body.b, &b, 0);
}
/* a command (its JSON, checked): its id */
static int dl_send(const char *R, const char *json, char id[12])
{
	const char *k = jget(json, "kind"); static const char *K[] = { "sh", "screenshot", "get", "put", "restart", "end", NULL }; int ok = 0;
	for (int i = 0; K[i]; i++) if (!strcmp(k, K[i])) ok = 1;
	if (!ok || json[0] != '{') die(2, "a command is a JSON object with kind: sh, screenshot, get, put, restart or end");
	{ const char *pp = jget(json, "path"); if (*pp && sd_keypath(pp)) die(5, "refused: %s is a key file; it is never fetched", pp); }
	guard_out(json, strlen(json), "the command");
	size_t n = strlen(json); char *d = malloc(n + 2); memcpy(d, json, n); d[n] = '\n'; d[n + 1] = 0;
	for (int attempt = 0; attempt < 8; attempt++) {
		int nx = dl_next(R); if (nx > 999999) die(1, "the ids are used up: start a new repository");
		snprintf(id, 12, "%06d", nx); char path[40], msg[60]; snprintf(path, sizeof path, "cmd/%s.json", id); snprintf(msg, sizeof msg, "command %s (%s)", id, k);
		buf b = {0}, p = {0}, body = {0}; char *e = b64((unsigned char *)d, n + 1);
		bprintf(&body, "{\"message\":\"%s\",\"branch\":\"link\",\"content\":\"%s\"}", msg, e); free(e);
		bprintf(&p, "/repos/%s/contents/%s", R, path); int c = http("PUT", p.b, body.b, &b, 0);
		if (c == 200 || c == 201) { free(d); return 0; }
		if (c == 409 || c == 422) { sleep(1 + attempt); continue; }
		if (c == 404) die(4, "the repository or its branch \"link\" was not found, or the token cannot see it");
		return fail(c, &b, "the command");
	}
	die(1, "another writer kept taking the next id");
}
static int dl_result(const char *R, const char *id, buf *out)
{
	buf p = {0}; bprintf(&p, "/repos/%s/contents/res/%s.json?ref=link", R, id); int c = http("GET", p.b, NULL, out, 1);
	if (c == 404) return 0; if (c != 200) die(1, "res/%s.json could not be read (GitHub answered %d)", id, c); return 1;
}
static int dl_wait(const char *R, const char *id, int secs, buf *out)
{
	time_t end = time(NULL) + secs;
	for (;;) { if (dl_result(R, id, out)) return 1; if (time(NULL) >= end) return 0; sleep(secs > 30 ? 5 : 2); }
}
static int id_ok(const char *s) { return s && strlen(s) == 6 && strspn(s, "0123456789") == 6; }

/* ---------------- devlink (the command line of iso/systools/devlink.py) */
static int dl_print(const char *R, const char *id, const buf *r, const char *save)
{
	char st[32]; snprintf(st, sizeof st, "%s", jget(r->b, "status")); const char *out = jget(r->b, "stdout"), *err = jget(r->b, "stderr");
	(void)R;
	if (*out) { fputs(out, stdout); if (out[strlen(out) - 1] != '\n') fputc('\n', stdout); }
	if (*err) { fputs(err, stderr); if (err[strlen(err) - 1] != '\n') fputc('\n', stderr); }
	fprintf(stderr, "[%s %s: status %s", id, jget(r->b, "kind"), *st ? st : "?");
	const char *f[] = { "statusText", "error", "approved", NULL };
	for (int i = 0; f[i]; i++) { const char *v = jget(r->b, f[i]); if (*v) fprintf(stderr, "; %s %s", f[i], v); }
	/* the files sent with it, saved */
	const char *fl = strstr(r->b, "\"files\""); if (fl && save) { const char *e = strchr(fl, ']'); mkdir(save, 0755);
		for (const char *q = strchr(fl, '['); q && e && q < e; ) { q = strchr(q + 1, '"'); if (!q || q > e) break; const char *z = strchr(q + 1, '"'); if (!z) break;
			char nm[200]; snprintf(nm, sizeof nm, "%.*s", (int)(z - q - 1), q + 1); q = z;
			if (strchr(nm, '/') || nm[0] == '.') continue;
			buf b = {0}, p = {0}; bprintf(&p, "/repos/%s/contents/res/%s/%s?ref=link", R, id, nm);
			if (http("GET", p.b, NULL, &b, 1) == 200) { char fn[400]; snprintf(fn, sizeof fn, "%s/%s-%s", save, id, nm); FILE *o = fopen(fn, "wb"); if (o) { fwrite(b.b, 1, b.n, o); fclose(o); fprintf(stderr, "; saved %s", fn); } } } }
	fprintf(stderr, "]\n");
	int s = atoi(st); return *st && s >= 0 && s < 256 ? s : 1;
}
static int devlink_main(int argc, char **argv)
{
	const char *R = getenv("DEVLINK_REPO"), *tf = NULL; int i = 1;
	for (; i < argc && argv[i][0] == '-' && argv[i][1] == '-'; i++) { if (!strcmp(argv[i], "--repo") && i + 1 < argc) R = argv[++i]; else if (!strcmp(argv[i], "--token-file") && i + 1 < argc) tf = argv[++i];
		else if (!strcmp(argv[i], "--help")) { puts("devlink [--repo OWNER/NAME] run|send KIND [ARG] [--timeout S] [--file F|--url U|--src P] [--wait S] [--save DIR] | wait ID [--wait S] [--save DIR] | last | next   (docs/DEVLINK.md)"); return 0; }
		else break; }
	if (i >= argc) die(2, "run, send, wait, last or next (devlink --help)");
	const char *op = argv[i++];
	if (!repo_ok(R)) die(2, "the repository is written as OWNER/NAME (--repo or DEVLINK_REPO)");
	const char *t = getenv("DEVLINK_TOKEN"); if (t && *t) snprintf(TOK, sizeof TOK, "%s", t);
	else { char p[600]; if (!tf) { snprintf(p, sizeof p, "%s/.config/rudolf-devlink/token", getenv("HOME") ? getenv("HOME") : ""); tf = p; }
		FILE *f = fopen(tf, "r"); if (!f || !fgets(TOK, sizeof TOK, f)) die(2, "no token (DEVLINK_TOKEN or --token-file)"); fclose(f); TOK[strcspn(TOK, "\r\n")] = 0;
		struct stat st; if (!stat(tf, &st) && (st.st_mode & 077)) fprintf(stderr, "devlink: %s can be read by others: chmod 600 it\n", tf); }
	if (!strcmp(op, "next")) { printf("%06d\n", dl_next(R)); return 0; }
	if (!strcmp(op, "last")) { int c = dl_max(R, "cmd"), r = dl_max(R, "res"); printf("{\"command\":"); c ? printf("\"%06d\"", c) : printf("null"); printf(",\"result\":"); r ? printf("\"%06d\"", r) : printf("null"); puts("}"); return 0; }
	int wait = 900; const char *save = NULL, *kind = NULL, *arg = NULL, *file = NULL, *url = NULL, *src = NULL; int to = 0;
	for (; i < argc; i++) {
		if (!strcmp(argv[i], "--wait") && i + 1 < argc) wait = atoi(argv[++i]); else if (!strcmp(argv[i], "--save") && i + 1 < argc) save = argv[++i];
		else if (!strcmp(argv[i], "--timeout") && i + 1 < argc) to = atoi(argv[++i]); else if (!strcmp(argv[i], "--file") && i + 1 < argc) file = argv[++i];
		else if (!strcmp(argv[i], "--url") && i + 1 < argc) url = argv[++i]; else if (!strcmp(argv[i], "--src") && i + 1 < argc) src = argv[++i];
		else if (!kind) kind = argv[i]; else if (!arg) arg = argv[i]; else die(2, "too many arguments"); }
	if (!strcmp(op, "wait")) {
		if (!id_ok(kind)) die(2, "an id is six digits"); buf r = {0};
		if (!dl_wait(R, kind, wait, &r)) { fprintf(stderr, "devlink: no result within %d s\n", wait); return 124; } return dl_print(R, kind, &r, save); }
	if (strcmp(op, "run") && strcmp(op, "send")) die(2, "run, send, wait, last or next");
	if (!kind) die(2, "which kind? sh, screenshot, get, put, restart, end");
	buf j = {0}; bprintf(&j, "{\"kind\":"); jstr(&j, kind);
	if (!strcmp(kind, "sh")) { buf s = {0}; if (!arg || !strcmp(arg, "-")) { char tmp[4096]; size_t k; while ((k = fread(tmp, 1, sizeof tmp, stdin)) > 0) badd(&s, tmp, k); } else badd(&s, arg, strlen(arg));
		if (!s.n) die(2, "sh needs a script"); bprintf(&j, ",\"script\":"); jstr(&j, s.b); if (to > 0) bprintf(&j, ",\"timeout\":%d", to); }
	else if (!strcmp(kind, "get")) { if (!arg || arg[0] != '/') die(2, "get needs a full path"); bprintf(&j, ",\"path\":"); jstr(&j, arg); }
	else if (!strcmp(kind, "put")) { if (!arg) die(2, "put needs a name"); bprintf(&j, ",\"name\":"); jstr(&j, arg);
		if (file) { FILE *f = fopen(file, "rb"); if (!f) die(2, "%s: %s", file, strerror(errno)); buf d = {0}; char tmp[65536]; size_t k; while ((k = fread(tmp, 1, sizeof tmp, f)) > 0) { badd(&d, tmp, k); if (d.n > (95u << 20)) die(2, "at most about 95 MB this way: attach it to a release (--url)"); } fclose(f);
			char path[300]; snprintf(path, sizeof path, "files/%s", arg); int c = dl_put_file(R, path, (unsigned char *)d.b, d.n, "file"); if (c != 200 && c != 201) die(1, "the file could not be uploaded (GitHub answered %d)", c); src = path;
			bprintf(&j, ",\"src\":"); jstr(&j, src); }
		else if (src) { bprintf(&j, ",\"src\":"); jstr(&j, src); } else if (url) { bprintf(&j, ",\"url\":"); jstr(&j, url); } else die(2, "put needs --file, --src or --url"); }
	bprintf(&j, "}");
	char id[12]; dl_send(R, j.b, id);
	if (!strcmp(op, "send")) { puts(id); return 0; }
	buf r = {0}; if (!dl_wait(R, id, wait > to + 120 ? wait : to + 120, &r)) { fprintf(stderr, "devlink: no result within %d s (is the link on, and Ask first answered?)\n", wait); return 124; }
	return dl_print(R, id, &r, save);
}

int main(int argc, char **argv)
{
	const char *api = getenv("RUDOLF_DEV_API"); if (!api || !*api) api = getenv("DEVLINK_API"); if (api && *api) snprintf(API, sizeof API, "%s", api);
	{ size_t n = strlen(API); while (n && API[n - 1] == '/') API[--n] = 0; }
	const char *b = strrchr(argv[0], '/'); b = b ? b + 1 : argv[0];
	if (!strcmp(b, "devlink")) return devlink_main(argc, argv);
	if (argc > 1 && !strcmp(argv[1], "devlink")) return devlink_main(argc - 1, argv + 1);
	if (argc < 3) die(2, "usage: repo|runs|jobs|joblog|workflows|file|dispatch|push|pr|dl-send|dl-wait|dl-file R ... (the token on standard input)");
	const char *cmd = argv[1], *R = argv[2];
	if (!repo_ok(R)) die(2, "the repository is written as OWNER/NAME");
	if (!fgets(TOK, sizeof TOK, stdin)) die(2, "the token: the first line of standard input"); TOK[strcspn(TOK, "\r\n")] = 0;
	if (strlen(TOK) < 10 || strspn(TOK, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != strlen(TOK)) die(2, "that is not a GitHub token");
	sd_known_init(0, 0); sd_known_add(TOK, strlen(TOK));   /* (1.6.1 Part 26: the token itself is never sent or printed) */
	buf p = {0};
	if (!strcmp(cmd, "repo")) { bprintf(&p, "/repos/%s", R); return get_to(p.b, 0, 1 << 20, "the repository"); }
	if (!strcmp(cmd, "runs")) { bprintf(&p, "/repos/%s/actions/runs?per_page=15", R); return get_to(p.b, 0, 4 << 20, "the runs"); }
	if (!strcmp(cmd, "workflows")) { bprintf(&p, "/repos/%s/actions/workflows?per_page=100", R); return get_to(p.b, 0, 2 << 20, "the workflows"); }
	if (!strcmp(cmd, "jobs") && argc > 3 && strspn(argv[3], "0123456789") == strlen(argv[3])) { bprintf(&p, "/repos/%s/actions/runs/%s/jobs", R, argv[3]); return get_to(p.b, 0, 4 << 20, "the jobs"); }
	if (!strcmp(cmd, "joblog") && argc > 3 && strspn(argv[3], "0123456789") == strlen(argv[3])) {
		bprintf(&p, "/repos/%s/actions/jobs/%s/logs", R, argv[3]); buf b = {0}; int c = http("GET", p.b, NULL, &b, 0); if (c != 200) return fail(c, &b, "the log");
		size_t from = b.n > 65536 ? b.n - 65536 : 0; while (from && from < b.n && b.b[from - 1] != '\n') from++;
		put_redacted(b.b + from, b.n - from); return 0; }
	if (!strcmp(cmd, "file") && argc > 3) { if (!path_ok(argv[3])) die(2, "a path inside the repository"); if (sd_keypath(argv[3])) die(5, "refused: %s is a key file; key files are never read for Blitzen", argv[3]);
		bprintf(&p, "/repos/%s/contents/", R); urlenc(&p, argv[3]);
		if (argc > 4) { if (!ref_ok(argv[4])) die(2, "a branch"); bprintf(&p, "?ref="); urlenc(&p, argv[4]); }
		buf b = { .max = 1 << 20 }; int c = http("GET", p.b, NULL, &b, 1); if (c != 200) return fail(c, &b, argv[3]); put_redacted(b.b, b.n); return 0; }
	if (!strcmp(cmd, "dispatch") && argc > 4) { if (!ref_ok(argv[3]) || !ref_ok(argv[4])) die(2, "a workflow (its file name or number) and a branch");
		buf body = {0}, b = {0}; bprintf(&body, "{\"ref\":"); jstr(&body, argv[4]); bprintf(&body, "}"); bprintf(&p, "/repos/%s/actions/workflows/", R); urlenc(&p, argv[3]); bprintf(&p, "/dispatches");
		int c = http("POST", p.b, body.b, &b, 0); if (c != 204) return fail(c, &b, "starting the workflow"); puts("{\"ok\":true}"); return 0; }
	if (!strcmp(cmd, "push") && argc > 5) return cmd_push(R, argv[3], argv[4], argv[5]);
	if (!strcmp(cmd, "pr") && argc > 5) return cmd_pr(R, argv[3], argv[4], argv[5]);
	if (!strcmp(cmd, "dl-send")) { char j[70000]; if (!fgets(j, sizeof j, stdin)) die(2, "the command's JSON: the second line"); j[strcspn(j, "\r\n")] = 0; char id[12]; dl_send(R, j, id); printf("{\"ok\":true,\"id\":\"%s\"}\n", id); return 0; }
	if (!strcmp(cmd, "dl-wait") && argc > 4 && id_ok(argv[3])) { buf r = {0}; int s = atoi(argv[4]); if (s < 0 || s > 600) s = 30;
		if (!dl_wait(R, argv[3], s, &r)) { puts("{\"ok\":true,\"waiting\":true}"); return 0; } printf("{\"ok\":true,\"result\":"); put(&r); fputs("}\n", stdout); return 0; }
	if (!strcmp(cmd, "dl-file") && argc > 4 && id_ok(argv[3])) { const char *n = argv[4]; if (strchr(n, '/') || n[0] == '.' || !*n) die(2, "a file's name");
		bprintf(&p, "/repos/%s/contents/res/%s/", R, argv[3]); urlenc(&p, n); bprintf(&p, "?ref=link"); buf b = { .max = 30 << 20 }; int c = http("GET", p.b, NULL, &b, 1);
		if (c != 200) return fail(c, &b, n); fwrite(b.b, 1, b.n, stdout); return 0; }
	die(2, "unknown command %s", cmd);
}
