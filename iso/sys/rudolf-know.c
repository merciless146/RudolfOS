/* rudolf-know - what Blitzen looks up before it answers (1.6.0, docs/plan-1.6.0.md Part 18). GPL-2.0-or-later.
 *
 * The owner, 2026-11-18: "Can Blitzen be given the ability to train itself based on the system's source code, system logs
 * and user activity, and research sources thru Oracle for the user's questions or requests?" Not by training its model: by
 * looking things up. Run by rudolfd as root (its API: ai/know/...); reading the web it runs as nobody.
 *
 *   rudolf-know status                       what is kept: {"serial","source","logs","built",...} (JSON)
 *   rudolf-know build [--source]             the system's logs again (scrubbed); with --source, Rudolf OS's own source and
 *                                            docs for the running release (its signed list's S row: downloaded once,
 *                                            checked against its SHA-256, taken apart into chunks)
 *   rudolf-know search [--home DIR] [--n N] Q   the chunks that fit Q best (BM25 by words), from the source, the logs and
 *                                            the activity kept in that account's folder: JSON [{"src","where","kind","text","score"}]
 *   rudolf-know show SRC LINE                about 60 lines of a source file around LINE, from the chunks (plain text)
 *   rudolf-know activity --home DIR          one line on standard input ("KIND<tab>TEXT") kept in that account's activity
 *   rudolf-know list --home DIR              that account's activity, newest first (JSON)
 *   rudolf-know forget --home DIR [--acct ID] --all | --day YYYY-MM-DD | --item N   (N: as list numbers them)
 *   rudolf-know learn --home DIR [on|off]    "Learn from what I do" for that account: on or off (said back)
 *   rudolf-know notes --home DIR [--set]     the account's notes for Blitzen (JSON); --set: new ones, a line each, on standard input
 *   rudolf-know migrate --acct ID --home DIR  1.6.0's copy outside the folder (KD/acct/ID.log, .on) moved into it, and removed
 *   rudolf-know gone --acct ID               an account removed: anything of it outside its folder removed
 *   rudolf-know web [--tor] Q                a web search for Q (DuckDuckGo's plain page) and the text of its first 5 results
 *                                            (JSON [{"title","url","text"}]); as nobody, through Oracle's proxy or Tor
 *
 * Files: KD = /persist/os/know (RUDOLF_KNOW_DIR for the tests): source.chunks + source.serial, logs.chunks, status: the
 * system's, the same for every account. What is the person's (1.6.1, docs/plan-1.6.1.md Part 27 item 4) is in the account's own
 * encrypted home folder DIR (rudolfd gives it only while it is open: the account signed in), DIR/.rudolf/blitzen/ (0700, the
 * account's Linux user's: the folder's owner): activity.log (lines "EPOCH<tab>KIND<tab>TEXT", at most 2 MB, the oldest dropped
 * first; 0600), notes.txt (a note a line; 0600) and learn (there: "Learn from what I do" is on). 1.6.0 kept the activity in
 * KD/acct/ID.log, outside the folder: migrate moves it in.  A chunk is "\x1e" SRC "\t" WHERE "\n" then its text.
 * The source archive: every system release (r SERIAL) carries rudolf-os-SERIAL-source.tar.xz, the repository's desktop, iso, libs
 * and docs at that release, and its signed list a row "S<tab>source<tab>SIZE<tab>SHA-256<tab>ADDRESS" (iso/systools/make_release.py
 * --source); older rudolf-update reads past rows it does not know. */
#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <openssl/evp.h>

static char KD[256] = "/persist/os/know";
static const char *KEYF = "/etc/rudolf/rpk-keys", *SYSD = "/persist/os/system";

static void die(const char *f, ...) { va_list a; va_start(a, f); fprintf(stderr, "rudolf-know: "); vfprintf(stderr, f, a); fprintf(stderr, "\n"); va_end(a); exit(1); }
static char *slurp(const char *p, size_t *n)
{
	FILE *f = fopen(p, "rb"); if (!f) return NULL;
	size_t cap = 65536, len = 0; char *b = malloc(cap); size_t k;
	while (b && (k = fread(b + len, 1, cap - len - 1, f)) > 0) { len += k; if (cap - len < 2) { cap *= 2; char *nb = realloc(b, cap); if (!nb) { free(b); b = NULL; } else b = nb; } }
	fclose(f); if (!b) return NULL; b[len] = 0; if (n) *n = len; return b;
}
static int write_file(const char *p, const char *s, size_t n)
{
	char t[420]; snprintf(t, sizeof t, "%s.tmp", p);
	FILE *f = fopen(t, "wb"); if (!f) return -1;
	int ok = fwrite(s, 1, n, f) == n; ok = !fclose(f) && ok;
	return ok ? rename(t, p) : (unlink(t), -1);
}
static void mkdirs(const char *p) { char t[400]; snprintf(t, sizeof t, "%s", p); for (char *s = t + 1; *s; s++) if (*s == '/') { *s = 0; mkdir(t, 0700); *s = '/'; } mkdir(t, 0700); }
/* JSON string out */
static void js(FILE *o, const char *s, size_t n)
{
	fputc('"', o);
	for (size_t i = 0; i < n && s[i]; i++) { unsigned char c = s[i];
		if (c == '"' || c == '\\') { fputc('\\', o); fputc(c, o); }
		else if (c == '\n') fputs("\\n", o); else if (c == '\t') fputs("\\t", o); else if (c == '\r') ;
		else if (c < 0x20) fprintf(o, "\\u%04x", c); else fputc(c, o); }
	fputc('"', o);
}
static int acct_ok(const char *a) { if (!a || !*a || strlen(a) > 64) return 0; for (const char *p = a; *p; p++) if (!isalnum((unsigned char)*p) && *p != '-' && *p != '_' && *p != '.') return 0; return a[0] != '.'; }

/* ---------------- what may never be indexed: lines with a password, token or key (the same rules as rudolfd's ai/facts) */
static int secret_line(const char *l, size_t n)
{
	static const char *const W[] = { "password", "passwd", "passphrase", "psk", "token", "secret", "private", "bearer",
		"authorization", "cookie", "key=", "key:", "-----begin", NULL };
	char low[1024]; size_t i = 0; for (; i < n && i < sizeof low - 1; i++) low[i] = tolower((unsigned char)l[i]); low[i] = 0;
	for (int k = 0; W[k]; k++) if (strstr(low, W[k])) return 1;
	return 0;
}
/* a new text: those lines left out, 32 or more characters of hex or base64 with a digit in them (a key, a hash) [hidden] */
static char *scrub(const char *s, size_t n)
{
	char *o = malloc(14 * n + 128), *w = o;   /* (a left-out line of 4 bytes says so in 53) */ if (!o) die("out of memory");
	for (const char *l = s; l < s + n; ) {
		const char *e = memchr(l, '\n', n - (l - s)); size_t L = e ? (size_t)(e - l) : (size_t)(s + n - l);
		if (secret_line(l, L)) { w += sprintf(w, "(a line with a password, token or key was left out)\n"); l += L + 1; continue; }
		for (const char *p = l; p < l + L; ) {
			const char *q = p; while (q < l + L && (isalnum((unsigned char)*q) || *q == '+' || *q == '=' || *q == '_' || *q == '-')) q++;
			size_t k = q - p; int dig = 0; for (const char *c = p; c < q; c++) if (isdigit((unsigned char)*c)) dig = 1;
			if (k >= 32 && dig) { memcpy(w, "[hidden]", 8); w += 8; p = q; continue; }
			if (k) { memcpy(w, p, k); w += k; p = q; continue; }
			*w++ = *p++;
		}
		*w++ = '\n'; l += L + 1;
	}
	*w = 0; return o;
}

/* ---------------- chunks */
typedef struct { const char *src, *where, *text; size_t sl, wl, tl; char kind; } chunk;
typedef struct { chunk *c; size_t n, cap; char *buf[8]; int nb; } store;
static void st_add(store *s, char kind, const char *src, size_t sl, const char *where, size_t wl, const char *text, size_t tl)
{
	if (s->n == s->cap) { s->cap = s->cap ? s->cap * 2 : 4096; s->c = realloc(s->c, s->cap * sizeof *s->c); if (!s->c) die("out of memory"); }
	s->c[s->n++] = (chunk){ src, where, text, sl, wl, tl, kind };
}
static void st_load(store *s, const char *path, char kind)
{
	size_t n; char *b = slurp(path, &n); if (!b) return; if (s->nb < 8) s->buf[s->nb++] = b;
	for (char *p = b; p && (p = memchr(p, 0x1e, n - (p - b))); ) {
		char *src = p + 1, *tab = memchr(src, '\t', n - (src - b)), *nl = tab ? memchr(tab, '\n', n - (tab - b)) : NULL; if (!nl) break;
		char *end = memchr(nl + 1, 0x1e, n - (nl + 1 - b)); if (!end) end = b + n;
		st_add(s, kind, src, tab - src, tab + 1, nl - tab - 1, nl + 1, end - nl - 1); p = end;
	}
}
static void act_path(char *p, size_t n, const char *home);
static void st_activity(store *s, const char *home)
{
	char p[600]; act_path(p, sizeof p, home); size_t n; char *b = slurp(p, &n); if (!b) return; if (s->nb < 8) s->buf[s->nb++] = b;
	for (char *l = b; l < b + n; ) { char *e = memchr(l, '\n', n - (l - b)); if (!e) e = b + n;
		char *t1 = memchr(l, '\t', e - l), *t2 = t1 ? memchr(t1 + 1, '\t', e - t1 - 1) : NULL;
		if (t2) st_add(s, 'a', "activity", 8, l, t1 - l, t2 + 1, e - t2 - 1);   /* (where: its time; the kind is in the text) */
		l = e + 1; }
}
/* chunk writer: a file's text in pieces of about 300 words, each with the line it starts at */
static void chunk_text(FILE *o, const char *src, const char *t, size_t n, int scrubbed)
{
	size_t i = 0; int line = 1;
	while (i < n) {
		size_t st = i; int sl = line, words = 0, inw = 0;
		while (i < n && (words < 300 || t[i] != '\n')) { if (t[i] == '\n') line++; int w = !isspace((unsigned char)t[i]); if (w && !inw) words++; inw = w; i++; if (i - st > 6000 && t[i - 1] == '\n') break; }
		if (i < n) { i++; line++; }
		size_t len = i - st; int any = 0; for (size_t k = st; k < i; k++) if (!isspace((unsigned char)t[k])) { any = 1; break; }
		if (!any) continue;
		(void)scrubbed; fprintf(o, "\x1e%s\tline %d\n", src, sl); fwrite(t + st, 1, len, o); if (t[i - 1] != '\n') fputc('\n', o);
	}
}

/* ---------------- words and BM25 */
static const char *STOP[] = { "the", "a", "an", "and", "or", "of", "to", "in", "is", "it", "on", "for", "with", "as", "at", "by", "be", "this", "that", "my", "me", "i", "do", "does", "how", "what", "why", "where", "when", "can", "you", "are", "was", "not", "no", "if", "from", "its", "into", "there", "which", "who", NULL };
static int stopw(const char *w) { for (int i = 0; STOP[i]; i++) if (!strcmp(STOP[i], w)) return 1; return 0; }
/* a word's simple stem, so that "failed", "fails" and "failing" find "fail" (English endings only) */
static void stem(char *w)
{
	size_t n = strlen(w);
	if (n > 5 && !strcmp(w + n - 3, "ing")) w[n - 3] = 0;
	else if (n > 4 && !strcmp(w + n - 2, "ed")) w[n - 2] = 0;
	else if (n > 4 && !strcmp(w + n - 2, "es")) w[n - 2] = 0;
	else if (n > 3 && w[n - 1] == 's' && w[n - 2] != 's') w[n - 1] = 0;
}
#define QN 16
typedef struct { char w[48]; int df; } qterm;
/* a word matches a query word: the same, or one of its parts split at _ (wm_minimise: minimise) */
static int count_tf(const char *t, size_t n, qterm *q, int nq, int *tf)
{
	int len = 0; memset(tf, 0, sizeof(int) * nq);
	for (size_t i = 0; i < n; ) {
		while (i < n && !isalnum((unsigned char)t[i])) i++;
		size_t s = i; while (i < n && (isalnum((unsigned char)t[i]) || t[i] == '_')) i++;
		if (i == s) continue;
		len++;
		size_t wl = i - s; if (wl >= 48) continue;
		char w[48]; for (size_t k = 0; k < wl; k++) w[k] = tolower((unsigned char)t[s + k]); w[wl] = 0;
		char ws[48]; snprintf(ws, sizeof ws, "%s", w); stem(ws);
		for (int j = 0; j < nq; j++) {
			if (!strcmp(ws, q[j].w)) { tf[j]++; continue; }
			if (strchr(w, '_')) { char *sv, *p, cp[48]; snprintf(cp, sizeof cp, "%s", w); for (p = strtok_r(cp, "_", &sv); p; p = strtok_r(NULL, "_", &sv)) { stem(p); if (!strcmp(p, q[j].w)) { tf[j]++; break; } } }
		}
	}
	return len;
}
static int cmp_score(const void *a, const void *b) { double x = ((const double *)a)[0], y = ((const double *)b)[0]; return x < y ? 1 : x > y ? -1 : 0; }
static int search(const char *home, int nmax, const char *query)
{
	qterm q[QN]; int nq = 0; char qc[1024]; snprintf(qc, sizeof qc, "%s", query);
	for (char *p = qc; *p; p++) *p = isalnum((unsigned char)*p) || *p == '_' ? tolower((unsigned char)*p) : ' ';
	char *sv; for (char *w = strtok_r(qc, " ", &sv); w && nq < QN; w = strtok_r(NULL, " ", &sv)) {
		if (strlen(w) < 2 || strlen(w) >= 48 || stopw(w)) continue;
		stem(w);
		int dup = 0; for (int j = 0; j < nq; j++) if (!strcmp(q[j].w, w)) dup = 1;
		if (!dup) { snprintf(q[nq].w, 48, "%s", w); q[nq].df = 0; nq++; } }
	store s = {0}; char p[400];
	snprintf(p, sizeof p, "%s/source.chunks", KD); st_load(&s, p, 's');
	snprintf(p, sizeof p, "%s/logs.chunks", KD); st_load(&s, p, 'l');
	if (home) st_activity(&s, home);
	if (!nq || !s.n) { puts("[]"); return 0; }
	int (*tf)[QN] = calloc(s.n, sizeof *tf); int *dl = calloc(s.n, sizeof *dl); if (!tf || !dl) die("out of memory");
	double avg = 0;
	for (size_t i = 0; i < s.n; i++) { dl[i] = count_tf(s.c[i].text, s.c[i].tl, q, nq, tf[i]); avg += dl[i]; for (int j = 0; j < nq; j++) if (tf[i][j]) q[j].df++; }
	avg = avg / s.n + 1;
	double (*sc)[2] = calloc(s.n, sizeof *sc); size_t ns = 0;
	for (size_t i = 0; i < s.n; i++) {
		double v = 0; int hit = 0;
		for (int j = 0; j < nq; j++) if (tf[i][j]) { hit++; double idf = log1p((s.n - q[j].df + .5) / (q[j].df + .5)), f = tf[i][j];
			v += idf * f * 2.2 / (f + 1.2 * (.25 + .75 * dl[i] / avg)); }
		if (!hit) continue;
		v *= 1 + .15 * (hit - 1);   /* (more of the question's words together) */
		if (s.c[i].kind == 'a') v *= 1.15; else if (s.c[i].kind == 'l') v *= 1.1;   /* (what happened here, before what is written anywhere) */
		sc[ns][0] = v; sc[ns][1] = (double)i; ns++;
	}
	qsort(sc, ns, sizeof *sc, cmp_score);
	printf("["); for (size_t k = 0; k < ns && (int)k < nmax; k++) { chunk *c = &s.c[(size_t)sc[k][1]];
		printf("%s{\"kind\":\"%s\",\"src\":", k ? "," : "", c->kind == 's' ? "source" : c->kind == 'l' ? "log" : "activity"); js(stdout, c->src, c->sl);
		printf(",\"where\":"); js(stdout, c->where, c->wl); printf(",\"score\":%.3f,\"text\":", sc[k][0]); js(stdout, c->text, c->tl > 2400 ? 2400 : c->tl); printf("}"); }
	printf("]\n");
	return 0;
}
static int show(const char *src, int line)
{
	store s = {0}; char p[400]; snprintf(p, sizeof p, "%s/source.chunks", KD); st_load(&s, p, 's');
	int printed = 0;
	for (size_t i = 0; i < s.n; i++) { chunk *c = &s.c[i]; if (c->sl != strlen(src) || strncmp(c->src, src, c->sl)) continue;
		int at = atoi(c->where + 5), ln = at;   /* ("line N") */
		for (size_t k = 0; k < c->tl; ) { const char *e = memchr(c->text + k, '\n', c->tl - k); size_t L = e ? (size_t)(e - c->text - k) : c->tl - k;
			if (ln >= line - 25 && ln <= line + 35) { printf("%6d  %.*s\n", ln, (int)L, c->text + k); printed++; }
			ln++; k += L + 1; } }
	if (!printed) { fprintf(stderr, "rudolf-know: %s is not among what Blitzen keeps\n", src); return 1; }
	return 0;
}

/* ---------------- the logs (again at each build), scrubbed */
static const char *LOGS[][2] = {
	{ "/persist/var/log/health.log", "the health log" }, { "/run/rudolf/session.log", "the session log" }, { "/run/rudolf/page.log", "the desktop's page log" },
	{ "/run/rudolf/rudolfd.log", "the system service's log" }, { "/run/rudolf/native-desktop.log", "the native desktop's log" }, { "/run/rudolf/update.log", "the updates' log" },
	{ "/persist/var/log/starts.log", "the start-ups' log" }, { "/run/rudolf/portal.log", "the portal's log" }, { NULL, NULL } };
static int build_logs(void)
{
	char p[400]; snprintf(p, sizeof p, "%s/logs.chunks.new", KD); FILE *o = fopen(p, "wb"); if (!o) return -1; int n = 0;
	const char *alt = getenv("RUDOLF_KNOW_LOGS");   /* (the tests: a folder whose *.log files stand in for the logs) */
	if (alt && *alt) { DIR *D = opendir(alt); struct dirent *e;
		while (D && (e = readdir(D))) { size_t l = strlen(e->d_name); if (l < 5 || strcmp(e->d_name + l - 4, ".log")) continue;
			char f[600]; snprintf(f, sizeof f, "%s/%s", alt, e->d_name); size_t m; char *t = slurp(f, &m); if (!t) continue;
			if (m > 400000) { memmove(t, t + m - 400000, 400001); m = 400000; }
			{ char *c = scrub(t, m); chunk_text(o, e->d_name, c, strlen(c), 1); free(c); } free(t); n++; }
		if (D) closedir(D); }
	else for (int i = 0; LOGS[i][0]; i++) { size_t m; char *t = slurp(LOGS[i][0], &m); if (!t) continue;
		if (m > 400000) { memmove(t, t + m - 400000, 400001); m = 400000; }   /* (the newest 400 KB of each) */
		{ char *c = scrub(t, m); chunk_text(o, LOGS[i][1], c, strlen(c), 1); free(c); } free(t); n++; }
	if (!alt || !*alt) { FILE *d = popen("dmesg 2>/dev/null | tail -300", "r"); if (d) { char *t = malloc(1 << 20); size_t m = t ? fread(t, 1, (1 << 20) - 1, d) : 0; pclose(d);
		if (t && m) { t[m] = 0; char *c = scrub(t, m); chunk_text(o, "the kernel's messages", c, strlen(c), 1); free(c); n++; } free(t); } }
	if (fclose(o)) return -1;
	char f[400]; snprintf(f, sizeof f, "%s/logs.chunks", KD); return rename(p, f) ? -1 : n;
}

/* ---------------- the source: the running release's S row, downloaded once, checked, taken apart */
static int verify_sig(const char *msg, size_t ml, const unsigned char *sig, size_t sl)
{
	if (sl != 64) return 0;
	char *t = slurp(getenv("RUDOLF_KNOW_KEYS") ? getenv("RUDOLF_KNOW_KEYS") : KEYF, NULL); if (!t) return 0; int ok = 0;
	for (char *l = strtok(t, "\n"); l && !ok; l = strtok(NULL, "\n")) {
		while (isspace((unsigned char)*l)) l++;
		if (*l == '#') continue;
		char *e = l; while (*e && !isspace((unsigned char)*e)) e++; *e = 0;
		unsigned char raw[64]; if (strlen(l) != 44 || EVP_DecodeBlock(raw, (unsigned char *)l, 44) < 32) continue;
		EVP_PKEY *k = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, raw, 32); if (!k) continue;
		EVP_MD_CTX *c = EVP_MD_CTX_new();
		ok = EVP_DigestVerifyInit(c, NULL, NULL, NULL, k) == 1 && EVP_DigestVerify(c, sig, 64, (const unsigned char *)msg, ml) == 1;
		EVP_MD_CTX_free(c); EVP_PKEY_free(k);
	}
	free(t); return ok;
}
static int sha256_file(const char *p, char out[65])
{
	FILE *f = fopen(p, "rb"); if (!f) return -1; EVP_MD_CTX *c = EVP_MD_CTX_new(); EVP_DigestInit_ex(c, EVP_sha256(), NULL);
	unsigned char b[65536], h[32]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0) EVP_DigestUpdate(c, b, n); fclose(f);
	unsigned int hl = 0; EVP_DigestFinal_ex(c, h, &hl); EVP_MD_CTX_free(c); for (int i = 0; i < 32; i++) sprintf(out + 2 * i, "%02x", h[i]); out[64] = 0; return 0;
}
typedef void CURL;
static struct { void *h; CURL *(*init)(void); int (*setopt)(CURL *, int, ...); int (*perform)(CURL *); int (*getinfo)(CURL *, int, ...); void (*cleanup)(CURL *); } C;
typedef struct { char *b; size_t n, cap, max; FILE *f; } sink;
static size_t wcb(char *p, size_t s, size_t n, void *u)
{
	sink *k = u; size_t m = s * n; if (k->f) return fwrite(p, 1, m, k->f);
	if (k->n + m > k->max) return 0;   /* (too big: stopped) */
	if (k->n + m + 1 > k->cap) { k->cap = (k->n + m + 1) * 2; char *nb = realloc(k->b, k->cap); if (!nb) return 0; k->b = nb; }
	memcpy(k->b + k->n, p, m); k->n += m; k->b[k->n] = 0; return m;
}
static const char *PROXY;   /* (Oracle's proxy, or Tor's) */
static int fetch(const char *url, sink *k, long tmo)
{
	if (!C.h) { if (!(C.h = dlopen("libcurl.so.4", RTLD_NOW))) return -1;
		int (*gi)(long) = dlsym(C.h, "curl_global_init"); C.init = dlsym(C.h, "curl_easy_init"); C.setopt = dlsym(C.h, "curl_easy_setopt");
		C.perform = dlsym(C.h, "curl_easy_perform"); C.getinfo = dlsym(C.h, "curl_easy_getinfo"); C.cleanup = dlsym(C.h, "curl_easy_cleanup"); if (gi) gi(3); }
	CURL *h = C.init(); if (!h) return -1;
	C.setopt(h, 10002, url); C.setopt(h, 20011, wcb); C.setopt(h, 10001, k); C.setopt(h, 52, 1L); C.setopt(h, 68, 5L); C.setopt(h, 13, tmo); C.setopt(h, 78, 15L);
	C.setopt(h, 10018, "Mozilla/5.0 (X11; Linux x86_64) Rudolf-OS-Blitzen/1.6");
	C.setopt(h, 10102, "");   /* (CURLOPT_ACCEPT_ENCODING: what libcurl can unpack) */
	if (!access("/etc/ssl/certs/ca-certificates.crt", R_OK)) C.setopt(h, 10065, "/etc/ssl/certs/ca-certificates.crt");
	if (PROXY && *PROXY) C.setopt(h, 10004, PROXY);
	int r = C.perform(h); long code = 0; C.getinfo(h, 0x200002, &code); C.cleanup(h);
	return r || code >= 400 ? -1 : 0;
}
static int build_source(void)
{
	/* the running release: its slot's signed list */
	char *cl = slurp("/etc/rudolf/slot", NULL), slot[4] = ""; if (cl && (cl[0] == 'a' || cl[0] == 'b')) { slot[0] = cl[0]; slot[1] = 0; } free(cl);   /* (as rudolf-update knows it; a stick's r and the disc have no list) */
	const char *sd = getenv("RUDOLF_KNOW_SYSD") ? getenv("RUDOLF_KNOW_SYSD") : SYSD; if (getenv("RUDOLF_KNOW_SLOT")) snprintf(slot, sizeof slot, "%s", getenv("RUDOLF_KNOW_SLOT"));
	if (!slot[0]) { fprintf(stderr, "rudolf-know: not started from an installed system's slot: no source to fetch\n"); return 2; }
	char mp[400], gp[400]; snprintf(mp, sizeof mp, "%s/%s/manifest.txt", sd, slot); snprintf(gp, sizeof gp, "%s/%s/manifest.sig", sd, slot);
	size_t ml, gl; char *m = slurp(mp, &ml); unsigned char *g = (unsigned char *)slurp(gp, &gl);
	if (!m || !g || !verify_sig(m, ml, g, gl)) { free(m); free(g); fprintf(stderr, "rudolf-know: this release's list is missing or not signed by Rudolf OS\n"); return 2; }
	free(g);
	char serial[32] = "", sha[80] = "", url[600] = ""; long long size = 0;
	for (char *sv, *l = strtok_r(m, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) {
		if (!strncmp(l, "serial ", 7)) snprintf(serial, sizeof serial, "%s", l + 7);
		else if (!strncmp(l, "S\tsource\t", 9)) { char *f[3]; int k = 0; char *sv2; for (char *x = strtok_r(l + 9, "\t", &sv2); x && k < 3; x = strtok_r(NULL, "\t", &sv2)) f[k++] = x;
			if (k == 3) { size = atoll(f[0]); snprintf(sha, sizeof sha, "%s", f[1]); snprintf(url, sizeof url, "%s", f[2]); } } }
	free(m);
	if (!*sha || strlen(sha) != 64 || strncmp(url, "https://", 8)) { fprintf(stderr, "rudolf-know: release %s has no source archive in its list\n", serial); return 3; }
	char sp[400]; snprintf(sp, sizeof sp, "%s/source.serial", KD); char *have = slurp(sp, NULL);
	if (have && !strncmp(have, serial, strlen(serial)) && have[strlen(serial)] == '\n') { free(have); printf("source %s already kept\n", serial); return 0; }
	free(have);
	char tp[400]; snprintf(tp, sizeof tp, "%s/source.tar.xz", KD); sink k = {0}; k.f = fopen(tp, "wb"); if (!k.f) die("cannot write %s", tp);
	const char *te = getenv("RUDOLF_KNOW_SOURCE_FILE");   /* (the tests: the archive already here) */
	int r = te ? -1 : fetch(url, &k, 1800); fclose(k.f);
	if (te) { char cmd[900]; snprintf(cmd, sizeof cmd, "cp '%s' '%s'", te, tp); r = system(cmd) ? -1 : 0; }
	char got[65]; struct stat st;
	if (r || stat(tp, &st) || (size && st.st_size != size) || sha256_file(tp, got) || strcmp(got, sha)) { unlink(tp); fprintf(stderr, "rudolf-know: the source archive did not download whole or does not match its list\n"); return 4; }
	/* taken apart in a folder of its own; the text files only, the built page and data files left out */
	char xd[400], cmd[2000]; snprintf(xd, sizeof xd, "%s/source.x", KD); snprintf(cmd, sizeof cmd, "rm -rf '%s' && mkdir -p '%s' && tar -xJf '%s' -C '%s'", xd, xd, tp, xd);
	if (system(cmd)) { unlink(tp); fprintf(stderr, "rudolf-know: the source archive could not be unpacked\n"); return 4; }
	char np[400]; snprintf(np, sizeof np, "%s/source.chunks.new", KD); FILE *o = fopen(np, "wb"); if (!o) die("cannot write %s", np);
	snprintf(cmd, sizeof cmd, "cd '%s' && find . -type f -size -1000k \\( -name '*.c' -o -name '*.h' -o -name '*.inc' -o -name '*.js' -o -name '*.sh' -o -name '*.py' -o -name '*.md' -o -name '*.txt' -o -name '*.TXT' -o -name '*.css' -o -name '*.rules' -o -name '*.yml' -o -name 'session' -o -name 'rudolf-init' \\) ! -path '*/iso/www/*' | sort", xd);
	FILE *L = popen(cmd, "r"); int files = 0; char line[1000];
	while (L && fgets(line, sizeof line, L)) { line[strcspn(line, "\n")] = 0; char f[1500]; snprintf(f, sizeof f, "%s/%s", xd, line + 2); size_t n; char *t = slurp(f, &n); if (!t) continue;
		size_t bad = 0; for (size_t i = 0; i < n && i < 4096; i++) if (!t[i]) bad++;
		if (!bad) { const char *rel = strchr(line + 2, '/'); chunk_text(o, rel ? rel + 1 : line + 2, t, n, 0); files++; }   /* (without the archive's top folder) */
		free(t); }
	if (L) pclose(L);
	snprintf(cmd, sizeof cmd, "rm -rf '%s' '%s'", xd, tp); if (system(cmd)) {}
	if (fclose(o)) return 4;
	char fp[400]; snprintf(fp, sizeof fp, "%s/source.chunks", KD); rename(np, fp);
	char sv[64]; int k2 = snprintf(sv, sizeof sv, "%s\n", serial); write_file(sp, sv, k2);
	printf("source %s: %d files\n", serial, files); return 0;
}

/* ---------------- an account's activity and notes: in its own folder, its own user's */
static int home_ok(const char *h)
{
	struct stat st; if (!h || h[0] != '/' || strstr(h, "/../") || strlen(h) > 300 || lstat(h, &st) || !S_ISDIR(st.st_mode)) return 0;
	return 1;
}
static void act_dir(char *p, size_t n, const char *home) { snprintf(p, n, "%s/.rudolf/blitzen", home); }
static void act_path(char *p, size_t n, const char *home) { snprintf(p, n, "%s/.rudolf/blitzen/activity.log", home); }
/* the folder .rudolf/blitzen made (0700), and both it and .rudolf the home folder owner's (never a link: refused) */
static int act_mkdir(const char *home)
{
	struct stat hs; if (lstat(home, &hs) || !S_ISDIR(hs.st_mode)) return -1;
	char p[600]; snprintf(p, sizeof p, "%s/.rudolf", home); struct stat st;
	if (lstat(p, &st)) { if (mkdir(p, 0755)) return -1; if (lchown(p, hs.st_uid, hs.st_gid)) {} } else if (!S_ISDIR(st.st_mode)) return -1;
	act_dir(p, sizeof p, home);
	if (lstat(p, &st)) { if (mkdir(p, 0700)) return -1; } else if (!S_ISDIR(st.st_mode)) return -1;
	if (lchown(p, hs.st_uid, hs.st_gid)) {} chmod(p, 0700); return 0;
}
/* a file there written whole (0600, the folder owner's) */
static int act_write(const char *home, const char *name, const char *s, size_t n)
{
	if (act_mkdir(home)) return -1;
	struct stat hs; if (lstat(home, &hs)) return -1;
	char d[600], t[700], p[700]; act_dir(d, sizeof d, home); snprintf(p, sizeof p, "%s/%s", d, name); snprintf(t, sizeof t, "%s/.%s.new", d, name);
	unlink(t); int f = open(t, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600); if (f < 0) return -1;
	int ok = write(f, s, n) == (ssize_t)n; if (fchown(f, hs.st_uid, hs.st_gid)) {} fchmod(f, 0600); ok = !close(f) && ok;
	if (!ok || rename(t, p)) { unlink(t); return -1; } return 0;
}
static int act_append(const char *home, const char *line)
{
	if (act_mkdir(home)) return -1;
	struct stat hs; if (lstat(home, &hs)) return -1;
	char p[600]; act_path(p, sizeof p, home); struct stat st;
	if (!lstat(p, &st) && st.st_size > 2000000) {   /* (at most 2 MB: the oldest quarter dropped) */
		size_t n; char *b = slurp(p, &n); if (b) { char *cut = memchr(b + n / 4, '\n', n - n / 4); if (cut) act_write(home, "activity.log", cut + 1, n - (cut + 1 - b)); free(b); } }
	int f = open(p, O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW | O_CLOEXEC, 0600); if (f < 0) return -1;
	if (fchown(f, hs.st_uid, hs.st_gid)) {} fchmod(f, 0600); size_t L = strlen(line); int ok = write(f, line, L) == (ssize_t)L; close(f); return ok ? 0 : -1;
}
static void legacy_path(char *p, size_t n, const char *acct, const char *ext) { snprintf(p, n, "%s/acct/%s.%s", KD, acct, ext); }
static int activity(const char *home)
{
	char in[2100]; if (!fgets(in, sizeof in, stdin)) return 1; in[strcspn(in, "\n")] = 0;
	char *t = strchr(in, '\t'); if (!t || t == in || t - in > 20 || !t[1]) die("give KIND<tab>TEXT");
	for (char *p = in; p < t; p++) if (!isalpha((unsigned char)*p)) die("bad kind");
	for (char *p = t + 1; *p; p++) if ((unsigned char)*p < 0x20) *p = ' ';
	if (secret_line(t + 1, strlen(t + 1))) { puts("not kept: it carried a password, token or key"); return 0; }
	{ char *c = scrub(t + 1, strlen(t + 1)); c[strcspn(c, "\n")] = 0; snprintf(t + 1, sizeof in - (t + 1 - in), "%s", c); free(c); }
	char line[2200]; snprintf(line, sizeof line, "%ld\t%.*s\t%s\n", (long)time(NULL), (int)(t - in), in, t + 1);
	if (act_append(home, line)) die("cannot keep it");
	return 0;
}
static int list(const char *home)
{
	char p[600]; act_path(p, sizeof p, home); size_t n; char *b = slurp(p, &n); printf("[");
	if (b) { /* newest first: the line numbers stay those of the file (forget --item N) */
		int total = 0; for (size_t i = 0; i < n; i++) if (b[i] == '\n') total++;
		char **ls = calloc(total + 1, sizeof *ls); int k = 0; for (char *l = b; k < total && l < b + n; ) { ls[k++] = l; char *e = memchr(l, '\n', n - (l - b)); if (!e) break; *e = 0; l = e + 1; }
		int out = 0; for (int i = k - 1; i >= 0 && out < 1000; i--) { char *t1 = strchr(ls[i], '\t'), *t2 = t1 ? strchr(t1 + 1, '\t') : NULL; if (!t2) continue;
			printf("%s{\"item\":%d,\"t\":%ld,\"kind\":", out++ ? "," : "", i + 1, atol(ls[i])); js(stdout, t1 + 1, t2 - t1 - 1); printf(",\"text\":"); js(stdout, t2 + 1, strlen(t2 + 1)); printf("}"); }
		free(ls); free(b); }
	printf("]\n"); return 0;
}
static int forget(const char *home, const char *acct, const char *how, const char *arg)
{
	char p[600]; act_path(p, sizeof p, home);
	if (!strcmp(how, "--all")) { unlink(p); if (acct && acct_ok(acct)) { char l[400]; legacy_path(l, sizeof l, acct, "log"); unlink(l); } puts("forgotten"); return 0; }
	size_t n; char *b = slurp(p, &n); if (!b) { puts("forgotten"); return 0; }
	char *o = malloc(n + 1); size_t on = 0; int ln = 0, gone = 0;
	for (char *l = b; l < b + n; ) { char *e = memchr(l, '\n', n - (l - b)); if (!e) e = b + n; ln++; int drop = 0;
		if (!strcmp(how, "--item")) drop = ln == atoi(arg);
		else if (!strcmp(how, "--day")) { time_t t = atol(l); struct tm tm; char day[16]; localtime_r(&t, &tm); strftime(day, sizeof day, "%Y-%m-%d", &tm); drop = !strcmp(day, arg); }
		if (drop) gone++; else { memcpy(o + on, l, e - l); on += e - l; o[on++] = '\n'; }
		l = e + 1; }
	act_write(home, "activity.log", o, on); free(o); free(b); printf("forgotten %d\n", gone); return 0;
}
/* the notes: at most 50 of at most 300 characters; one with a password, token or key is not kept */
static int notes(const char *home, int set)
{
	char p[600]; act_dir(p, sizeof p, home); strcat(p, "/notes.txt");
	if (set) { char *o = malloc(50 * 302 + 1); size_t on = 0; int k = 0, left = 0; char l[2048];
		while (fgets(l, sizeof l, stdin)) { l[strcspn(l, "\r\n")] = 0; size_t L = strlen(l); if (!L) continue; if (k >= 50) break;
			if (L > 300) { L = 300; while (L && ((unsigned char)l[L] & 0xc0) == 0x80) L--; l[L] = 0; }
			for (char *c = l; *c; c++) if ((unsigned char)*c < 0x20) *c = ' ';
			if (secret_line(l, L)) { left++; continue; }
			char *c = scrub(l, L); c[strcspn(c, "\n")] = 0; size_t cl = strlen(c); if (cl > 300) cl = 300; memcpy(o + on, c, cl); on += cl; o[on++] = '\n'; free(c); k++; }
		if (!on) { unlink(p); } else if (act_write(home, "notes.txt", o, on)) die("cannot keep them");
		free(o); printf("kept %d%s\n", k, left ? " (a note with a password, token or key was not kept)" : ""); return 0; }
	size_t n; char *b = slurp(p, &n); printf("[");
	int k = 0; if (b) for (char *sv, *l = strtok_r(b, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) { printf("%s", k++ ? "," : ""); js(stdout, l, strlen(l)); }
	printf("]\n"); free(b); return 0;
}
/* 1.6.0's copy of an account's activity, outside its folder: into the folder (before what is there), then removed */
static int migrate(const char *acct, const char *home)
{
	char l[400], on[400]; legacy_path(l, sizeof l, acct, "log"); legacy_path(on, sizeof on, acct, "on");
	size_t n = 0, m = 0; char *old = slurp(l, &n); int moved = 0;
	if (old && n) { char p[600]; act_path(p, sizeof p, home); char *cur = slurp(p, &m);
		char *all = malloc(n + m + 2); memcpy(all, old, n); if (n && old[n - 1] != '\n') all[n++] = '\n'; if (cur) memcpy(all + n, cur, m);
		if (act_write(home, "activity.log", all, n + m)) die("cannot move it");
		free(all); free(cur); moved = 1; }
	free(old);
	if (!access(on, F_OK)) { if (act_write(home, "learn", "1\n", 2)) die("cannot move it"); moved = 1; }
	unlink(l); unlink(on); printf("%s\n", moved ? "moved" : "nothing"); return 0;
}
/* "Learn from what I do": the file learn in the account's folder (there: on) */
static int learn(const char *home, const char *v)
{
	char p[600]; act_dir(p, sizeof p, home); strcat(p, "/learn");
	if (v && !strcmp(v, "on")) { if (act_write(home, "learn", "1\n", 2)) die("cannot keep it"); }
	else if (v && !strcmp(v, "off")) unlink(p);
	else if (v && *v) die("on or off");
	puts(access(p, F_OK) ? "off" : "on"); return 0;
}
static int gone(const char *acct)
{
	char p[400]; legacy_path(p, sizeof p, acct, "log"); unlink(p); legacy_path(p, sizeof p, acct, "on"); unlink(p); puts("removed"); return 0;
}

/* ---------------- the web: a search's results and their pages' text (as nobody) */
static void html_text(const char *h, size_t n, char *o, size_t on, char *title, size_t tn)
{
	size_t k = 0; int skip = 0, sp = 0; if (title) *title = 0;
	for (size_t i = 0; i < n && k + 2 < on; ) {
		if (h[i] == '<') {
			const char *e = memchr(h + i, '>', n - i); if (!e) break; size_t tl = e - (h + i) + 1;
			char tag[16] = ""; size_t a = i + 1, j = 0; if (h[a] == '/') a++; while (a < n && j < sizeof tag - 1 && isalnum((unsigned char)h[a])) tag[j++] = tolower((unsigned char)h[a++]); tag[j] = 0;
			int close = h[i + 1] == '/';
			if (!strcmp(tag, "script") || !strcmp(tag, "style") || !strcmp(tag, "noscript") || !strcmp(tag, "svg") || !strcmp(tag, "nav") || !strcmp(tag, "footer") || !strcmp(tag, "header")) skip = close ? (skip ? skip - 1 : 0) : skip + 1;
			if (!strcmp(tag, "title") && !close && title && !*title) { const char *te = memmem(e + 1, n - (e + 1 - h), "</", 2); if (te) { size_t L = te - (e + 1); if (L >= tn) L = tn - 1; memcpy(title, e + 1, L); title[L] = 0; } }
			if (!strcmp(tag, "p") || !strcmp(tag, "br") || !strcmp(tag, "div") || !strcmp(tag, "li") || tag[0] == 'h') { if (k && o[k - 1] != '\n') o[k++] = '\n'; sp = 0; }
			i += tl; continue; }
		if (skip) { i++; continue; }
		char c = h[i++];
		if (c == '&') { const char *e = memchr(h + i, ';', n - i < 8 ? n - i : 8); if (e) { size_t L = e - (h + i); c = !strncmp(h + i, "amp", L) ? '&' : !strncmp(h + i, "lt", L) ? '<' : !strncmp(h + i, "gt", L) ? '>' : !strncmp(h + i, "quot", L) ? '"' : (!strncmp(h + i, "#39", L) || !strncmp(h + i, "apos", L)) ? '\'' : ' '; i += L + 1; } }
		if (isspace((unsigned char)c)) { if (!sp && k && o[k - 1] != '\n') { o[k++] = ' '; sp = 1; } continue; }
		sp = 0; o[k++] = c;
	}
	o[k] = 0;
}
static void urldec(const char *s, char *o, size_t on)
{
	size_t k = 0; for (; *s && k + 1 < on; s++) { if (*s == '%' && isxdigit((unsigned char)s[1]) && isxdigit((unsigned char)s[2])) { char h[3] = { s[1], s[2], 0 }; o[k++] = (char)strtol(h, NULL, 16); s += 2; } else if (*s == '+') o[k++] = ' '; else if (*s == '&') break; else o[k++] = *s; }
	o[k] = 0;
}
static void urlenc(const char *s, char *o, size_t on) { size_t k = 0; for (; *s && k + 4 < on; s++) { unsigned char c = *s; if (isalnum(c) || strchr("-_.~", c)) o[k++] = c; else if (c == ' ') o[k++] = '+'; else k += snprintf(o + k, on - k, "%%%02X", c); } o[k] = 0; }
static int web(const char *q, int tor)
{
	char *px = slurp("/run/rudolf/proxy", NULL); if (px) px[strcspn(px, "\n")] = 0;
	PROXY = tor ? "socks5h://127.0.0.1:9050" : px;
	if (getenv("RUDOLF_KNOW_PROXY")) PROXY = getenv("RUDOLF_KNOW_PROXY");
	/* (nobody from here on: what comes from the web is read by a user that can read and write nothing of anyone's) */
	if (!getuid()) { if (setgroups(0, NULL) || setgid(65534) || setuid(65534)) die("could not drop to nobody"); }
	const char *base = getenv("RUDOLF_KNOW_SEARCH") ? getenv("RUDOLF_KNOW_SEARCH") : "https://html.duckduckgo.com/html/?q=";
	char qe[700], u[1200]; urlenc(q, qe, sizeof qe); snprintf(u, sizeof u, "%s%s", base, qe);
	sink k = { .max = 3 << 20 }; if (fetch(u, &k, 20) || !k.b) { fprintf(stderr, "rudolf-know: the search did not answer%s\n", tor ? " (is a Tor window open in Oracle?)" : ""); return 5; }
	char *urls[5]; int nu = 0;
	for (char *p = k.b; nu < 5 && (p = strstr(p, "result__a")); p++) {
		char *hr = strstr(p, "href=\""); if (!hr) break; hr += 6; char *e = strchr(hr, '"'); if (!e) break; *e = 0;
		char dec[1200], *uddg = strstr(hr, "uddg="); if (uddg) urldec(uddg + 5, dec, sizeof dec); else snprintf(dec, sizeof dec, "%s", hr);
		for (char *a = dec; (a = strstr(a, "&amp;")); ) memmove(a + 1, a + 5, strlen(a + 5) + 1);
		if (strncmp(dec, "https://", 8) && strncmp(dec, "http://", 7)) { p = e; continue; }
		if (strstr(dec, "duckduckgo.com/y.js")) { p = e; continue; }   /* (adverts) */
		int dup = 0; for (int i = 0; i < nu; i++) if (!strcmp(urls[i], dec)) dup = 1;
		if (!dup) urls[nu++] = strdup(dec);
		p = e;
	}
	free(k.b);
	printf("[");
	int out = 0;
	for (int i = 0; i < nu; i++) {
		sink pg = { .max = 2 << 20 }; if (fetch(urls[i], &pg, 12) || !pg.b) { free(pg.b); continue; }
		size_t tn = 1 << 16; char *t = malloc(tn), title[300]; html_text(pg.b, pg.n, t, tn, title, sizeof title); free(pg.b);
		/* at most about 1,200 words */
		int w = 0, inw = 0; size_t cut = strlen(t); for (size_t j = 0; t[j]; j++) { int c = !isspace((unsigned char)t[j]); if (c && !inw && ++w > 1200) { cut = j; break; } inw = c; } t[cut] = 0;
		char ttl[300]; html_text(title, strlen(title), ttl, sizeof ttl, NULL, 0);
		printf("%s{\"url\":", out++ ? "," : ""); js(stdout, urls[i], strlen(urls[i])); printf(",\"title\":"); js(stdout, ttl, strlen(ttl)); printf(",\"text\":"); js(stdout, t, strlen(t)); printf("}");
		free(t); fflush(stdout);
	}
	printf("]\n");
	return 0;
}

static int status(void)
{
	char p[400]; struct stat a, b; snprintf(p, sizeof p, "%s/source.chunks", KD); int hs = !stat(p, &a);
	snprintf(p, sizeof p, "%s/logs.chunks", KD); int hl = !stat(p, &b);
	snprintf(p, sizeof p, "%s/source.serial", KD); char *sv = slurp(p, NULL); if (sv) sv[strcspn(sv, "\n")] = 0;
	printf("{\"source\":%s,\"serial\":", hs ? "true" : "false"); js(stdout, sv ? sv : "", sv ? strlen(sv) : 0);
	printf(",\"source_bytes\":%lld,\"logs\":%s,\"logs_bytes\":%lld,\"built\":%ld}\n", hs ? (long long)a.st_size : 0LL, hl ? "true" : "false", hl ? (long long)b.st_size : 0LL, hl ? (long)b.st_mtime : 0L);
	free(sv); return 0;
}

int main(int argc, char **argv)
{
	if (getenv("RUDOLF_KNOW_DIR") && *getenv("RUDOLF_KNOW_DIR")) snprintf(KD, sizeof KD, "%s", getenv("RUDOLF_KNOW_DIR"));
	if (argc < 2) die("usage: status | build [--source] | search [--home DIR] [--n N] Q | show SRC LINE | activity --home DIR | list --home DIR | forget --home DIR [--acct ID] --all|--day D|--item N | notes --home DIR [--set] | learn --home DIR [on|off] | migrate --acct ID --home DIR | gone --acct ID | web [--tor] Q");
	umask(077);
	const char *cmd = argv[1], *acct = NULL, *home = NULL; int n = 6, tor = 0, src = 0, set = 0; int i = 2;
	for (; i < argc; i++) { if (!strcmp(argv[i], "--acct") && i + 1 < argc) acct = argv[++i]; else if (!strcmp(argv[i], "--home") && i + 1 < argc) home = argv[++i];
		else if (!strcmp(argv[i], "--n") && i + 1 < argc) n = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--tor")) tor = 1; else if (!strcmp(argv[i], "--source")) src = 1; else if (!strcmp(argv[i], "--set")) set = 1;
		else if (!strcmp(argv[i], "--")) { i++; break; } else break; }   /* (after --: what was asked, even if it starts with --) */
	if (n < 1 || n > 20) n = 6;
	if (home && !home_ok(home)) die("that is not an account's folder");
	if (acct && !acct_ok(acct)) die("which account? (--acct ID)");
	if (!strcmp(cmd, "web")) { if (i >= argc || !*argv[i] || strlen(argv[i]) > 400) die("search for what?"); return web(argv[i], tor); }
	mkdirs(KD);
	if (!strcmp(cmd, "status")) return status();
	if (!strcmp(cmd, "build")) { int l = build_logs(); if (l < 0) die("could not write the logs' index"); printf("logs: %d\n", l); return src ? build_source() : 0; }
	if (!strcmp(cmd, "search")) { if (i >= argc) die("search for what?"); return search(home, n, argv[i]); }
	if (!strcmp(cmd, "show")) { if (i + 1 >= argc) die("show SRC LINE"); return show(argv[i], atoi(argv[i + 1])); }
	if (!strcmp(cmd, "gone")) { if (!acct) die("which account? (--acct ID)"); return gone(acct); }
	if (!home) die("which account's folder? (--home DIR)");
	if (!strcmp(cmd, "activity")) return activity(home);
	if (!strcmp(cmd, "list")) return list(home);
	if (!strcmp(cmd, "notes")) return notes(home, set);
	if (!strcmp(cmd, "learn")) return learn(home, i < argc ? argv[i] : NULL);
	if (!strcmp(cmd, "migrate")) { if (!acct) die("which account? (--acct ID)"); return migrate(acct, home); }
	if (!strcmp(cmd, "forget")) { if (i >= argc) die("forget what?"); return forget(home, acct, argv[i], i + 1 < argc ? argv[i + 1] : ""); }
	die("unknown command %s", cmd);
}
