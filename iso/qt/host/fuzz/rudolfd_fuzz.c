/* 1.5.9 (plan 15.5): a fuzz test of rudolfd's HTTP request parsing and JSON handling, compiled from rudolfd.c itself
 * (main renamed). The first byte of an input chooses what it drives:
 *   0  a request's head as it arrives on 127.0.0.1:8080 (req_head, then qparam and urldecode on what it gave)
 *   1  JSON as the API's bodies and the page's storage hold it (dj_end, dj_get, dj_at, dj_str, dj_num, dj_raw)
 *   2  the page's storage (the lsync copy) behind desktop/accounts and desktop/settings, the native shell's requests
 *   3  sjson then dj_str: what rudolfd writes it must read back unchanged
 * With clang's libFuzzer (-fsanitize=fuzzer,address) LLVMFuzzerTestOneInput is the target. Without it (gcc), this file
 * is its own fuzzer: -DRF_DRIVER gives a main() that runs the corpus, then mutates it for a bounded time, keeping the
 * inputs that reach code not reached before (-fsanitize-coverage=trace-pc), under AddressSanitizer and UBSan; an input
 * that crashes is written to the crash folder.
 *   rudolfd_fuzz CORPUS-DIR SECONDS CRASH-DIR [SEED]      (the driver)    See fuzz59_t.sh. */
#define main rudolfd_main
#include "../../../sys/rudolfd.c"
#undef main
#include <stdint.h>

static char fz_dir[256];
static int fz_fd = -1;
static void fz_setup(void)
{
	if (fz_dir[0]) return;
	snprintf(fz_dir, sizeof fz_dir, "/tmp/rudolfd-fuzz.%d", (int)getpid()); mkdir(fz_dir, 0700);
	snprintf(desk_ls, sizeof desk_ls, "%s/ls.json", fz_dir); snprintf(desk_dir, sizeof desk_dir, "%s", fz_dir); snprintf(desk_run, sizeof desk_run, "%s", fz_dir);
	desk_uid = getuid();
	fz_fd = open("/dev/null", O_WRONLY);
}
static void fz_json(const char *t, int depth)
{
	if (depth > 6 || !t) return;
	const char *e = dj_end(t, 0); (void)e;
	static const char *K[] = { "n", "p", "r", "h", "id", "ks", "fk", "iv", "ct", "pk", "accts", "data", "gen", "guest", "last", "disk", "token", NULL };
	for (int i = 0; K[i]; i++) { const char *v = dj_get(t, K[i]); if (v) { char *s = dj_str(v); free(s); dj_num(v, 0); dj_true(v); dj_truthy(v); fz_json(v, depth + 1); } }
	int n = dj_count(t);
	for (int i = 0; i < n && i < 64; i++) { const char *a = dj_at(t, i); char *s = dj_str(a); free(s); fz_json(a, depth + 1); }
	str o = {0}; dj_raw(&o, t, "null"); free(o.b);
}
int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n)
{
	fz_setup();
	if (!n || n > 1 << 16) return 0;
	int kind = d[0] & 3; d++; n--;
	char *t = malloc(n + 1); memcpy(t, d, n); t[n] = 0;
	if (kind == 0) {
		char hdr[16384]; size_t m = n < sizeof hdr - 1 ? n : sizeof hdr - 1; memcpy(hdr, d, m); hdr[m] = 0;
		char *end = strstr(hdr, "\r\n\r\n"); if (end) *end = 0;
		struct req r; memset(&r, 0, sizeof r); r.fd = fz_fd; size_t clen = 0;
		if (!req_head(hdr, &r, &clen)) {
			static const char *Q[] = { "repo", "hours", "ask", "id", "a", "acct", "img", "k", "v", "path", "n", NULL };
			for (int i = 0; Q[i]; i++) { char *v = qparam(r.query, Q[i]); if (v) (void)strlen(v); }
			char fp[2400]; snprintf(fp, sizeof fp, "/www%s", r.path); urldecode(fp);
			if (clen > MAXBODY) clen = 0;
		}
	} else if (kind == 1) {
		fz_json(t, 0);
		struct req r; memset(&r, 0, sizeof r); r.body = t; r.blen = n;
		char *a = dbody_field(&r, "n"), *b = dbody_field(&r, "p"); free(a); free(b);
	} else if (kind == 2) {
		int f = open(desk_ls, O_WRONLY | O_CREAT | O_TRUNC, 0600); if (f >= 0) { if (write(f, d, n) < 0) {} close(f); }
		struct req r; memset(&r, 0, sizeof r); r.fd = fz_fd;
		api_desktop_accounts(&r);
		api_desktop_settings(&r);
		dls l; if (dls_open(&l)) { char *list = dls_accounts(&l); if (list) { for (int i = 0; i < 64; i++) { const char *a = dj_at(list, i); if (!a) break; char id[80]; dacct_id(a, id, sizeof id); }
			dacct_find(list, "Ann"); free(list); } dls_close(&l); }
	} else {
		/* (bytes without a NUL: what a C string can hold) */
		for (size_t i = 0; i < n; i++) if (!t[i]) t[i] = 'x';
		str s = {0}; sjson(&s, t); char *back = dj_str(s.b);
		if (!back || strcmp(back, t)) { fprintf(stderr, "sjson/dj_str round trip differs\n"); abort(); }
		free(back); free(s.b);
	}
	free(t);
	return 0;
}

#ifdef RF_DRIVER
/* ---------------------------------------------------------------- the driver (no libFuzzer) */
#include <setjmp.h>
#define MAPN (1 << 16)
static uint8_t cov[MAPN]; static size_t cov_new;
__attribute__((no_sanitize_address, no_sanitize_coverage, no_sanitize_undefined)) void __sanitizer_cov_trace_pc(void)
{
	static uintptr_t prev;
	uintptr_t pc = (uintptr_t)__builtin_return_address(0);
	size_t i = ((pc >> 1) ^ prev) & (MAPN - 1); prev = pc >> 2;
	if (!cov[i]) { cov[i] = 1; cov_new++; }
}
typedef struct { uint8_t *d; size_t n; } inp;
static inp *C; static size_t CN, CCAP;
static const uint8_t *cur; static size_t curn; static char crashdir[512];
extern void __sanitizer_set_death_callback(void (*)(void));
static void save_cur(const char *why)
{
	char p[700]; static int k; snprintf(p, sizeof p, "%s/crash-%d-%d", crashdir, (int)getpid(), k++);
	int f = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (f >= 0) { if (write(f, cur, curn) < 0) {} close(f); }
	fprintf(stderr, "rudolfd_fuzz: %s: the input is in %s\n", why, p);
}
static void on_death(void) { save_cur("crashed"); }
static void on_alarm(int s) { (void)s; save_cur("took more than 10 s"); _exit(3); }
static void add(const uint8_t *d, size_t n)
{
	if (CN == CCAP) { CCAP = CCAP ? CCAP * 2 : 256; C = realloc(C, CCAP * sizeof *C); }
	C[CN].d = malloc(n ? n : 1); memcpy(C[CN].d, d, n); C[CN].n = n; CN++;
}
static uint64_t rs;
static uint64_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return rs; }
static size_t mutate(uint8_t *b, size_t n, size_t cap)
{
	static const char *D[] = { "\r\n", "\r\n\r\n", "\"", "\\", "\\u", "\\ud800", "\\udc00", "{", "}", "[", "]", ":", ",", "%", "%2e%2e", "..", "?", "&", "=",
		"Content-Length:", "Content-Length: -1", "Content-Length: 99999999999999999999", "X-Rudolf-Token: ", "Range: bytes=", "GET /", "POST /api/",
		"{\"data\":{\"bd95user\":\"", "\\\"", "\\ud800\\u", "\\u12", "\\", "\"accts\":[", "null", "true", "1e999", "-", "0", "\xff", "\xc3\xbc", NULL };
	int ops = 1 + rnd() % 6;
	for (int o = 0; o < ops; o++) {
		size_t p = n ? rnd() % (n + 1) : 0;
		switch (rnd() % 8) {
		case 0: if (n) b[rnd() % n] ^= 1u << (rnd() % 8); break;
		case 1: if (n) b[rnd() % n] = (uint8_t)rnd(); break;
		case 2: if (p < n) { size_t l = 1 + rnd() % (n - p); if (l > 16) l = 16; memmove(b + p, b + p + l, n - p - l); n -= l; } break;
		case 3: { int k = 0; while (D[k]) k++; const char *w = D[rnd() % k]; size_t l = strlen(w); if (n + l <= cap) { memmove(b + p + l, b + p, n - p); memcpy(b + p, w, l); n += l; } } break;
		case 4: if (n) { size_t s = rnd() % n, l = 1 + rnd() % (n - s); if (l > 256) l = 256; if (n + l <= cap) { uint8_t tmp[256]; memcpy(tmp, b + s, l); memmove(b + p + l, b + p, n - p); memcpy(b + p, tmp, l); n += l; } } break;
		case 5: if (CN) { inp *x = &C[rnd() % CN]; if (x->n > 1) { size_t s = 1 + rnd() % (x->n - 1), l = 1 + rnd() % (x->n - s); if (l > 512) l = 512; if (p + l <= cap) { if (p + l > n) n = p + l; memcpy(b + p, x->d + s, l); } } } break;
		case 6: if (n + 1 <= cap) { memmove(b + p + 1, b + p, n - p); b[p] = "{[\"\\:,0a\r\n%"[rnd() % 11]; n++; } break;
		case 7: if (n > 1 && rnd() % 4 == 0) n = 1 + rnd() % n; break;
		}
	}
	return n;
}
int main(int argc, char **argv)
{
	if (argc < 4) { fprintf(stderr, "usage: rudolfd_fuzz CORPUS-DIR SECONDS CRASH-DIR [SEED]\n"); return 2; }
	snprintf(crashdir, sizeof crashdir, "%s", argv[3]); mkdir(crashdir, 0755);
	rs = argc > 4 ? strtoull(argv[4], NULL, 10) : (uint64_t)time(NULL) * 2654435761u + getpid(); if (!rs) rs = 1;
	__sanitizer_set_death_callback(on_death); signal(SIGALRM, on_alarm);
	DIR *D = opendir(argv[1]); struct dirent *e; size_t seeds = 0;
	while (D && (e = readdir(D))) {
		if (e->d_name[0] == '.') continue;
		char p[700]; snprintf(p, sizeof p, "%s/%s", argv[1], e->d_name); size_t n; char *b = slurp(p, &n); if (!b) continue;
		cur = (uint8_t *)b; curn = n; alarm(10); LLVMFuzzerTestOneInput((uint8_t *)b, n); alarm(0);
		add((uint8_t *)b, n); free(b); seeds++;
	}
	if (D) closedir(D);
	if (!CN) { fprintf(stderr, "rudolfd_fuzz: no corpus in %s\n", argv[1]); return 2; }
	fprintf(stderr, "rudolfd_fuzz: %zu corpus inputs ran, %zu places reached; mutating for %s s\n", seeds, cov_new, argv[2]);
	time_t until = time(NULL) + atoi(argv[2]); size_t runs = 0, kept = 0; static uint8_t buf[1 << 16];
	while (time(NULL) < until) {
		inp *x = &C[rnd() % CN]; size_t n = x->n > sizeof buf ? sizeof buf : x->n; memcpy(buf, x->d, n);
		uint8_t k0 = n ? buf[0] : 0;
		n = mutate(buf, n, sizeof buf); if (!n) continue;
		if (rnd() % 8) buf[0] = k0;   /* (mostly the same target as the input it came from) */
		size_t before = cov_new; cur = buf; curn = n;
		alarm(10); LLVMFuzzerTestOneInput(buf, n); alarm(0); runs++;
		if (cov_new > before && CN < 20000) { add(buf, n); kept++; }
	}
	fprintf(stderr, "rudolfd_fuzz: %zu runs, %zu new inputs kept, %zu places reached, no crash\n", runs, kept, cov_new);
	char rm[300]; snprintf(rm, sizeof rm, "rm -rf '%s'", fz_dir); if (system(rm)) {}
	return 0;
}
#endif
