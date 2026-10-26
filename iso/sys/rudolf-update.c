/* rudolf-update - automatic system updates for Rudolf OS. GPL-2.0-or-later.
 *
 * The system is five files: vmlinuz (kernel with its start-up initramfs) and the layers base.sfs,
 * chromium.sfs, desktop.sfs and modules.sfs. An update is assembled in the spare one of two slots on
 * RUDOLFDATA (/persist/os/system/a and b) while the running system stays untouched; the disc's start-up
 * script then starts it with kexec, and gives up on it after two failed starts (see initramfs/init).
 *
 * A release is a signed manifest (manifest.txt + manifest.sig, Ed25519, the same keys as rpk) listing each
 * file's size and SHA-256. Files are fetched as deltas: every file has a .rzs control file holding a weak
 * rolling checksum and a strong checksum (first 8 bytes of SHA-256) per 4 KB block - the zsync method. The
 * device finds the blocks it already has anywhere in its current files and downloads only the rest with
 * HTTP range requests (libcurl, loaded when needed; BusyBox wget is the fallback for whole files).
 *
 *   rudolf-update status            JSON for the desktop
 *   rudolf-update check             fetch and verify the newest manifest
 *   rudolf-update download          assemble the new release in the spare slot, ready for the next start
 *   rudolf-update boot-ok           the desktop came up: keep the slot that is being tried
 *   rudolf-update rollback          go back to the previous system at the next start
 *   rudolf-update set url URL | set auto 0|1
 *   rudolf-update rzs-make FILE OUT.rzs     (build side) write a delta control file */
#define _GNU_SOURCE
#include <ctype.h>
#include <dlfcn.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <openssl/evp.h>

#define BS 4096
#define NL 5
static const char *LAYERS[NL] = {"vmlinuz", "base", "chromium", "desktop", "modules"};
static char SYSD[256] = "/persist/os/system", MEDIA[256] = "/live/media", CONFP[256] = "/persist/os/update.conf";
static char KEYF[256] = "/etc/rudolf/rpk-keys", SLOTF[256] = "/etc/rudolf/slot";

static void die(const char *f, ...) { va_list a; va_start(a, f); fputs("rudolf-update: ", stderr); vfprintf(stderr, f, a); fputc('\n', stderr); va_end(a); exit(1); }
static void say(const char *f, ...) { va_list a; va_start(a, f); vprintf(f, a); putchar('\n'); fflush(stdout); va_end(a); }
static char *slurp(const char *path, size_t *len)
{
	int f = open(path, O_RDONLY | O_CLOEXEC); if (f < 0) return NULL;
	size_t cap = 65536, n = 0; char *b = malloc(cap); ssize_t r;
	while ((r = read(f, b + n, cap - n - 1)) > 0) { n += r; if (n + 1 >= cap) { cap *= 2; b = realloc(b, cap); } }
	close(f); b[n] = 0; if (len) *len = n; return b;
}
static int write_file(const char *path, const char *text)
{
	char t[600]; snprintf(t, sizeof t, "%s.new", path);
	FILE *o = fopen(t, "w"); if (!o) return -1; fputs(text, o); fflush(o); fsync(fileno(o)); fclose(o);
	return rename(t, path);
}
static void jstr(const char *s) { putchar('"'); for (; s && *s; s++) { unsigned char c = *s; if (c == '"' || c == '\\') printf("\\%c", c); else if (c < 0x20) printf("\\u%04x", c); else putchar(c); } putchar('"'); }
static int spawn(char *const argv[]) { pid_t p = fork(); if (!p) { execvp(argv[0], argv); _exit(127); } int st; waitpid(p, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : -1; }
static void mkdirs(const char *d) { char t[512]; snprintf(t, sizeof t, "%s", d); for (char *p = t + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(t, 0755); *p = '/'; } mkdir(t, 0755); }

/* ---------------- key = value files (state, config, RELEASE) */
typedef struct { char k[16][24]; char v[16][300]; int n; } kv_t;
static void kv_read(const char *path, kv_t *m, char sep)
{
	m->n = 0; char *t = slurp(path, NULL); if (!t) return;
	for (char *l = strtok(t, "\n"); l && m->n < 16; l = strtok(NULL, "\n")) {
		char *e = strchr(l, sep); if (!e) continue; *e = 0;
		snprintf(m->k[m->n], 24, "%s", l); snprintf(m->v[m->n], 300, "%s", e + 1); m->n++;
	}
	free(t);
}
static const char *kv(kv_t *m, const char *k) { for (int i = 0; i < m->n; i++) if (!strcmp(m->k[i], k)) return m->v[i]; return ""; }
static void kv_set(kv_t *m, const char *k, const char *v)
{
	for (int i = 0; i < m->n; i++) if (!strcmp(m->k[i], k)) { snprintf(m->v[i], 300, "%s", v); return; }
	if (m->n < 16) { snprintf(m->k[m->n], 24, "%s", k); snprintf(m->v[m->n], 300, "%s", v); m->n++; }
}
static int kv_write(const char *path, kv_t *m, char sep)
{
	char buf[6000]; size_t o = 0; for (int i = 0; i < m->n && o < sizeof buf - 400; i++) o += snprintf(buf + o, sizeof buf - o, "%s%c%s\n", m->k[i], sep, m->v[i]);
	return write_file(path, buf);
}

/* ---------------- crypto */
static unsigned char KEYS[8][32]; static int NKEYS;
static void load_keys(void)
{
	char *t = slurp(KEYF, NULL); NKEYS = 0; if (!t) return;
	for (char *l = strtok(t, "\n"); l && NKEYS < 8; l = strtok(NULL, "\n")) { while (isspace((unsigned char)*l)) l++; if (*l == '#') continue; char *e = l; while (*e && !isspace((unsigned char)*e)) e++; *e = 0; unsigned char raw[64]; if (strlen(l) == 44 && EVP_DecodeBlock(raw, (unsigned char *)l, 44) >= 32) memcpy(KEYS[NKEYS++], raw, 32); }
	free(t);
}
static int verify(const unsigned char *msg, size_t len, const unsigned char *sig)
{
	for (int i = 0; i < NKEYS; i++) {
		EVP_PKEY *k = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, KEYS[i], 32); if (!k) continue;
		EVP_MD_CTX *c = EVP_MD_CTX_new(); int ok = EVP_DigestVerifyInit(c, NULL, NULL, NULL, k) == 1 && EVP_DigestVerify(c, sig, 64, msg, len) == 1;
		EVP_MD_CTX_free(c); EVP_PKEY_free(k); if (ok) return 1;
	}
	return 0;
}
static int sha256_file(const char *path, char hex[65])
{
	int f = open(path, O_RDONLY | O_CLOEXEC); if (f < 0) return -1;
	EVP_MD_CTX *c = EVP_MD_CTX_new(); EVP_DigestInit_ex(c, EVP_sha256(), NULL);
	static unsigned char buf[1 << 20]; ssize_t n; while ((n = read(f, buf, sizeof buf)) > 0) EVP_DigestUpdate(c, buf, n);
	close(f); unsigned char md[32]; unsigned ml; EVP_DigestFinal_ex(c, md, &ml); EVP_MD_CTX_free(c);
	for (int i = 0; i < 32; i++) sprintf(hex + 2 * i, "%02x", md[i]); return 0;
}
static void strong(const unsigned char *p, size_t n, unsigned char out[8])
{
	unsigned char md[32], pad[BS]; unsigned ml;
	if (n < BS) { memcpy(pad, p, n); memset(pad + n, 0, BS - n); p = pad; }
	EVP_Digest(p, BS, md, &ml, EVP_sha256(), NULL); memcpy(out, md, 8);
}
/* rsync's rolling checksum over one block */
static uint32_t weak(const unsigned char *p, size_t n)
{
	uint32_t a = 0, b = 0; for (size_t i = 0; i < BS; i++) { unsigned x = i < n ? p[i] : 0; a += x; b += (BS - i) * x; }
	return (a & 0xffff) | (b & 0xffff) << 16;
}

/* ---------------- delta control files (.rzs): "RZS1", u32 block size, u64 file size, then per block u32 weak + 8 bytes strong */
static int rzs_make(const char *file, const char *out)
{
	int f = open(file, O_RDONLY); if (f < 0) die("cannot read %s", file);
	struct stat st; fstat(f, &st); size_t n = st.st_size, nb = (n + BS - 1) / BS;
	unsigned char *m = n ? mmap(NULL, n, PROT_READ, MAP_PRIVATE, f, 0) : NULL;
	FILE *o = fopen(out, "wb"); if (!o) die("cannot write %s", out);
	uint32_t bs = BS; uint64_t sz = n; fwrite("RZS1", 1, 4, o); fwrite(&bs, 4, 1, o); fwrite(&sz, 8, 1, o);
	for (size_t i = 0; i < nb; i++) { size_t l = n - i * BS < BS ? n - i * BS : BS; uint32_t w = weak(m + i * BS, l); unsigned char s[8]; strong(m + i * BS, l, s); fwrite(&w, 4, 1, o); fwrite(s, 8, 1, o); }
	fclose(o); if (m) munmap(m, n); close(f); return 0;
}

/* ---------------- HTTP (libcurl via dlopen; BusyBox wget for whole files when libcurl is missing) */
typedef void CURL;
static struct { void *h; int (*global_init)(long); CURL *(*init)(void); int (*setopt)(CURL *, int, ...); int (*perform)(CURL *); int (*getinfo)(CURL *, int, ...); void (*cleanup)(CURL *); const char *(*strerror)(int); } C;
static CURL *CH;
static int curl_up(void)
{
	if (CH) return 1;
	if (!(C.h = dlopen("libcurl.so.4", RTLD_NOW))) return 0;
	C.global_init = dlsym(C.h, "curl_global_init"); C.init = dlsym(C.h, "curl_easy_init"); C.setopt = dlsym(C.h, "curl_easy_setopt");
	C.perform = dlsym(C.h, "curl_easy_perform"); C.getinfo = dlsym(C.h, "curl_easy_getinfo"); C.cleanup = dlsym(C.h, "curl_easy_cleanup"); C.strerror = dlsym(C.h, "curl_easy_strerror");
	if (!C.global_init || !C.init || !C.setopt || !C.perform || !C.getinfo) return 0;
	C.global_init(3); CH = C.init(); return CH != NULL;
}
typedef struct { FILE *f; off_t at, limit; long long got; } sink_t;
static size_t wcb(char *p, size_t s, size_t n, void *u) { sink_t *k = u; size_t len = s * n; if (k->limit >= 0 && k->got + (long long)len > k->limit) return 0; if (fwrite(p, 1, len, k->f) != len) return 0; k->got += len; return len; }
/* GET url (or a byte range of it) into f at its current position; returns 0 when complete */
static int http_get(const char *url, FILE *f, long long from, long long to)
{
	if (curl_up()) {
		sink_t k = {f, 0, to >= 0 ? to - from + 1 : -1, 0}; char range[64];
		C.setopt(CH, 10002, url); C.setopt(CH, 20011, wcb); C.setopt(CH, 10001, &k); C.setopt(CH, 52, 1L); C.setopt(CH, 45, 1L);
		C.setopt(CH, 78, 30L); C.setopt(CH, 19, 1L); C.setopt(CH, 20, 60L); C.setopt(CH, 10018, "rudolf-update/1");
		if (!access("/etc/ssl/certs/ca-certificates.crt", R_OK)) C.setopt(CH, 10065, "/etc/ssl/certs/ca-certificates.crt");
		if (to >= 0) { snprintf(range, sizeof range, "%lld-%lld", from, to); C.setopt(CH, 10007, range); } else C.setopt(CH, 10007, (char *)NULL);
		int r = C.perform(CH); long code = 0; C.getinfo(CH, 0x200002, &code);
		if (r) { say("  download error: %s", C.strerror ? C.strerror(r) : "failed"); return -1; }
		if (to >= 0 && code != 206) { say("  the server does not send parts of files (HTTP %ld)", code); return -2; }
		if (to >= 0 && k.got != to - from + 1) return -1;
		return 0;
	}
	if (to >= 0) return -2;
	char tmp[] = "/tmp/rudolf-update-XXXXXX"; int fd = mkstemp(tmp); if (fd < 0) return -1; close(fd);
	char *a[] = {"wget", "-q", "-T", "60", "-O", tmp, (char *)url, NULL}; int r = spawn(a);
	if (!r) { FILE *i = fopen(tmp, "rb"); char b[65536]; size_t n; while (i && (n = fread(b, 1, sizeof b, i)) > 0) fwrite(b, 1, n, f); if (i) fclose(i); }
	unlink(tmp); return r ? -1 : 0;
}
static int http_to_file(const char *url, const char *path)
{
	FILE *f = fopen(path, "wb"); if (!f) return -1; int r = http_get(url, f, 0, -1); if (fclose(f)) r = -1; if (r) unlink(path); return r;
}

/* ---------------- delta download */
typedef struct { uint32_t w; unsigned char s[8]; } blk_t;
static int delta_fetch(const char *url, const char *rzs_url, char seeds[][300], int nseeds, const char *out, long long size, long long *dl)
{
	char cp[400]; snprintf(cp, sizeof cp, "%s.rzs", out);
	if (http_to_file(rzs_url, cp)) return -1;
	size_t cl; unsigned char *c = (unsigned char *)slurp(cp, &cl); unlink(cp);
	if (!c || cl < 16 || memcmp(c, "RZS1", 4) || *(uint32_t *)(c + 4) != BS || (long long)*(uint64_t *)(c + 8) != size) { free(c); return -1; }
	size_t nb = (size + BS - 1) / BS; if (cl != 16 + nb * 12) { free(c); return -1; }
	blk_t *B = malloc(nb * sizeof *B); for (size_t i = 0; i < nb; i++) { memcpy(&B[i].w, c + 16 + i * 12, 4); memcpy(B[i].s, c + 20 + i * 12, 8); }
	free(c);
	/* hash table weak -> first block, chained */
	size_t hs = 1; while (hs < nb * 2) hs <<= 1;
	int64_t *head = malloc(hs * sizeof *head), *nxt = malloc(nb * sizeof *nxt); for (size_t i = 0; i < hs; i++) head[i] = -1;
	for (size_t i = 0; i < nb; i++) { size_t h = (B[i].w * 2654435761u) & (hs - 1); nxt[i] = head[h]; head[h] = i; }
	unsigned char *have = calloc(nb, 1); size_t got = 0;
	int of = open(out, O_RDWR | O_CREAT | O_TRUNC, 0644); if (of < 0 || ftruncate(of, size)) { free(B); return -1; }
	for (int s = 0; s < nseeds && got < nb; s++) {
		int sf = open(seeds[s], O_RDONLY); if (sf < 0) continue;
		struct stat st; fstat(sf, &st); size_t n = st.st_size; if (n < BS) { close(sf); continue; }
		unsigned char *m = mmap(NULL, n, PROT_READ, MAP_PRIVATE, sf, 0); close(sf); if (m == MAP_FAILED) continue;
		madvise(m, n, MADV_SEQUENTIAL);
		uint32_t a = 0, b = 0; for (size_t i = 0; i < BS; i++) { a += m[i]; b += (BS - i) * m[i]; }
		size_t i = 0;
		for (;;) {
			uint32_t w = (a & 0xffff) | (b & 0xffff) << 16; int hit = 0;
			for (int64_t j = head[(w * 2654435761u) & (hs - 1)]; j >= 0; j = nxt[j]) {
				if (B[j].w != w) continue;
				unsigned char sv[8]; strong(m + i, BS, sv);
				if (memcmp(sv, B[j].s, 8)) continue;
				/* every wanted block with this content */
				for (int64_t k = j; k >= 0; k = nxt[k]) if (!have[k] && B[k].w == w && !memcmp(B[k].s, sv, 8)) {
					size_t l = (size_t)size - k * BS < BS ? (size_t)size - k * BS : BS;
					if (pwrite(of, m + i, l, (off_t)k * BS) == (ssize_t)l) { have[k] = 1; got++; }
				}
				hit = 1; break;
			}
			if (hit) { i += BS; if (i + BS > n) break; a = b = 0; for (size_t q = 0; q < BS; q++) { a += m[i + q]; b += (BS - q) * m[i + q]; } continue; }
			if (i + BS >= n) break;
			unsigned xo = m[i], xn = m[i + BS]; a = a - xo + xn; b = b - BS * xo + a; i++;
		}
		munmap(m, n);
	}
	say("  %zu of %zu blocks (%lld%%) already here", got, nb, nb ? (long long)got * 100 / nb : 100);
	/* fetch the rest as ranges, joining near ones */
	FILE *f = fdopen(of, "r+b"); int r = 0; *dl = 0;
	for (size_t i = 0; i < nb && !r;) {
		if (have[i]) { i++; continue; }
		/* one request per run of missing blocks; runs closer than 8 blocks are joined (up to 16 MB a request) */
		size_t j = i, k = i + 1;
		while (k < nb && k - i < 4096) {
			if (!have[k]) { j = k++; continue; }
			size_t g = k; while (g < nb && have[g] && g - k <= 8) g++;
			if (g < nb && !have[g] && g - k <= 8) { j = g; k = g + 1; } else break;
		}
		long long from = (long long)i * BS, to = (long long)(j + 1) * BS - 1; if (to >= size) to = size - 1;
		fseeko(f, from, SEEK_SET);
		r = http_get(url, f, from, to); if (!r) *dl += to - from + 1;
		for (size_t q = i; q <= j; q++) have[q] = 1;
		i = j + 1;
	}
	if (fclose(f)) r = -1;
	free(B); free(head); free(nxt); free(have);
	return r;
}

/* ---------------- manifest */
typedef struct { char name[16], sha[65], file[400]; long long size; } lay_t;
typedef struct { long long serial; char version[64], released[16], expires[16], notes[300], chromium[64], kernel[64]; int urgent; lay_t L[NL]; int nl; } man_t;
static int parse_manifest(char *t, man_t *m)
{
	memset(m, 0, sizeof *m);
	if (strncmp(t, "rudolf-system 1\n", 16)) return -1;
	for (char *l = strtok(t, "\n"); l; l = strtok(NULL, "\n")) {
		if (!strncmp(l, "serial ", 7)) m->serial = atoll(l + 7);
		else if (!strncmp(l, "version ", 8)) snprintf(m->version, sizeof m->version, "%s", l + 8);
		else if (!strncmp(l, "released ", 9)) snprintf(m->released, sizeof m->released, "%s", l + 9);
		else if (!strncmp(l, "expires ", 8)) snprintf(m->expires, sizeof m->expires, "%s", l + 8);
		else if (!strncmp(l, "notes ", 6)) snprintf(m->notes, sizeof m->notes, "%s", l + 6);
		else if (!strncmp(l, "chromium ", 9)) snprintf(m->chromium, sizeof m->chromium, "%s", l + 9);
		else if (!strncmp(l, "kernel ", 7)) snprintf(m->kernel, sizeof m->kernel, "%s", l + 7);
		else if (!strncmp(l, "urgent ", 7)) m->urgent = atoi(l + 7);
		else if (!strncmp(l, "L\t", 2) && m->nl < NL) {
			lay_t *y = &m->L[m->nl]; char *p = l + 2, *f[4]; int k = 0;
			for (char *s = strtok_r(p, "\t", &p); s && k < 4; s = strtok_r(NULL, "\t", &p)) f[k++] = s;
			if (k < 4) continue;
			snprintf(y->name, sizeof y->name, "%s", f[0]); y->size = atoll(f[1]); snprintf(y->sha, sizeof y->sha, "%s", f[2]); snprintf(y->file, sizeof y->file, "%s", f[3]);
			int known = 0; for (int i = 0; i < NL; i++) if (!strcmp(y->name, LAYERS[i])) known = 1;
			const char *fp = y->file; while (!strncmp(fp, "../", 3)) fp += 3;
			if (known && strlen(y->sha) == 64 && !strstr(fp, "..")) m->nl++;
		}
	}
	return m->serial > 0 && m->nl == NL ? 0 : -1;
}
static lay_t *mlayer(man_t *m, const char *n) { for (int i = 0; i < m->nl; i++) if (!strcmp(m->L[i].name, n)) return &m->L[i]; return NULL; }

/* ---------------- where things are */
static void booted(char *s) { char *t = slurp(SLOTF, NULL); snprintf(s, 8, "%s", t && (t[0] == 'a' || t[0] == 'b') ? (t[1] = 0, t) : "disc"); free(t); }
static void cur_release(kv_t *r) { char s[8], p[400]; booted(s); if (strcmp(s, "disc")) snprintf(p, sizeof p, "%s/%s/RELEASE", SYSD, s); else snprintf(p, sizeof p, "%s/rudolf/release", MEDIA); kv_read(p, r, ' '); }
static void layer_path(const char *slot, const char *n, char *out, size_t sz)
{
	/* a slot holds the files that differ from the disc; the rest come from the disc */
	if (strcmp(slot, "disc")) { snprintf(out, sz, "%s/%s/%s%s", SYSD, slot, n, strcmp(n, "vmlinuz") ? ".sfs" : ""); if (!access(out, R_OK)) return; }
	if (!strcmp(n, "vmlinuz")) snprintf(out, sz, "%s/boot/vmlinuz", MEDIA); else snprintf(out, sz, "%s/rudolf/%s.sfs", MEDIA, n);
}
static void url_join(const char *base, const char *file, char *out, size_t n)
{
	if (strstr(file, "://")) { snprintf(out, n, "%s", file); return; }
	/* "../files/<serial>/x": the channel folder's parent holds the files of every release */
	char b[500]; snprintf(b, sizeof b, "%s", base); while (strlen(b) > 1 && b[strlen(b) - 1] == '/') b[strlen(b) - 1] = 0;
	while (!strncmp(file, "../", 3)) { char *sl = strrchr(b, '/'); if (sl && sl > strstr(b, "://") + 2) *sl = 0; file += 3; }
	snprintf(out, n, "%s/%s", b, file);
}

static int lockf_(void) { char p[300]; snprintf(p, sizeof p, "%s/.lock", SYSD); mkdirs(SYSD); int f = open(p, O_CREAT | O_RDWR | O_CLOEXEC, 0600); return f >= 0 && !flock(f, LOCK_EX | LOCK_NB) ? 0 : -1; }

/* speedtest (1.2.5, Task Manager's hardware check): how fast the update server answers here. Downloads the newest
   release's kernel (about 10 MB) into nothing; prints bytes and ms */
static int cmd_speedtest(void)
{
	kv_t cf; kv_read(CONFP, &cf, '='); const char *url = kv(&cf, "url");
	char def[300] = ""; if (!*url) { char *d = slurp("/etc/rudolf/update-url", NULL); if (d) { snprintf(def, sizeof def, "%s", d); char *nl = strchr(def, '\n'); if (nl) *nl = 0; free(d); } url = def; }
	if (!*url) { say("No update address is set."); return 2; }
	char mu[400], tmp[] = "/tmp/rudolf-speed-XXXXXX"; int fd = mkstemp(tmp); if (fd < 0) return 1; close(fd);
	snprintf(mu, sizeof mu, "%s/manifest.txt", url);
	if (http_to_file(mu, tmp)) { unlink(tmp); say("Could not reach the update server."); return 3; }
	size_t ml; char *t = slurp(tmp, &ml); unlink(tmp); man_t m;
	if (!t || parse_manifest(t, &m)) { free(t); say("The update server did not send a release list."); return 3; }
	lay_t *y = mlayer(&m, "vmlinuz"); if (!y) { free(t); return 3; }
	char u[600]; url_join(url, y->file, u, sizeof u); free(t);
	FILE *f = fopen("/dev/null", "wb"); if (!f) return 1;
	long long want = y->size;   /* the whole kernel, about 10 MB: any server, with or without libcurl */
	struct timespec a, b; clock_gettime(CLOCK_MONOTONIC, &a);
	int r = http_get(u, f, 0, -1);
	clock_gettime(CLOCK_MONOTONIC, &b); fclose(f);
	if (r) { say("The download from the update server failed."); return 3; }
	printf("bytes %lld\nms %lld\n", want, (long long)((b.tv_sec - a.tv_sec) * 1000 + (b.tv_nsec - a.tv_nsec) / 1000000));
	return 0;
}
static int cmd_check(int quiet)
{
	kv_t cf; kv_read(CONFP, &cf, '='); const char *url = kv(&cf, "url");
	char def[300] = ""; if (!*url) { char *d = slurp("/etc/rudolf/update-url", NULL); if (d) { snprintf(def, sizeof def, "%s", d); char *nl = strchr(def, '\n'); if (nl) *nl = 0; free(d); } url = def; }
	if (!*url) { say("No update address is set (Settings > Update and recovery)."); return 2; }
	char mu[400], su[400], mp[300], sp[300]; snprintf(mu, sizeof mu, "%s/manifest.txt", url); snprintf(su, sizeof su, "%s/manifest.sig", url);
	snprintf(mp, sizeof mp, "%s/available.txt.dl", SYSD); snprintf(sp, sizeof sp, "%s/available.sig.dl", SYSD);
	if (!quiet) say("Checking %s", url);
	if (http_to_file(mu, mp) || http_to_file(su, sp)) { say("Could not reach the update server."); return 3; }
	size_t ml, sl; char *t = slurp(mp, &ml), *g = slurp(sp, &sl);
	if (!t || !g || sl != 64 || !verify((unsigned char *)t, ml, (unsigned char *)g)) { say("Refused: the release list is not signed by a trusted key."); free(t); free(g); return 4; }
	char *copy = strdup(t); man_t m; int bad = parse_manifest(copy, &m); free(copy);
	if (bad) { say("Refused: the release list is damaged."); return 4; }
	char today[16]; time_t now = time(NULL); strftime(today, sizeof today, "%Y-%m-%d", gmtime(&now));
	if (*m.expires && strcmp(m.expires, today) < 0) { say("Refused: the release list expired on %s, so it may be hiding newer fixes.", m.expires); return 4; }
	kv_t r; cur_release(&r); long long cur = atoll(kv(&r, "serial"));
	/* never accept a list older than one already seen (someone replaying an old release) */
	char ap[300]; snprintf(ap, sizeof ap, "%s/available.txt", SYSD); size_t ol; char *old = slurp(ap, &ol); long long seen = 0;
	if (old) { man_t om; char *oc = strdup(old); if (!parse_manifest(oc, &om)) seen = om.serial; free(oc); free(old); }
	if (m.serial < seen) { say("Refused: this release list is older than one already seen."); return 4; }
	char sp2[300]; snprintf(sp2, sizeof sp2, "%s/available.sig", SYSD);
	rename(mp, ap); rename(sp, sp2);
	kv_t cf2; kv_read(CONFP, &cf2, '='); char ts[32]; snprintf(ts, sizeof ts, "%ld", (long)now); kv_set(&cf2, "checked", ts); kv_write(CONFP, &cf2, '=');
	if (m.serial > cur) say("Update available: Rudolf OS %s (%s)%s", m.version, m.notes, m.urgent ? " - urgent security fix" : "");
	else say("Rudolf OS is up to date (%s).", kv(&r, "version")[0] ? kv(&r, "version") : "this release");
	free(t); free(g);
	return 0;
}
static int cmd_download(void)
{
	if (lockf_()) die("another update is running");
	int r = cmd_check(1); if (r) return r;
	char ap[300]; snprintf(ap, sizeof ap, "%s/available.txt", SYSD); char *t = slurp(ap, NULL); man_t m; if (!t || parse_manifest(t, &m)) die("no release list");
	char sig[300]; snprintf(sig, sizeof sig, "%s/available.sig", SYSD);
	{ size_t ml, sl; char *mt = slurp(ap, &ml), *sg = slurp(sig, &sl); int ok = mt && sg && sl == 64 && verify((unsigned char *)mt, ml, (unsigned char *)sg); free(mt); free(sg); if (!ok) die("the saved release list is not signed"); }
	kv_t cr; cur_release(&cr);
	if (m.serial <= atoll(kv(&cr, "serial"))) { say("Already up to date."); return 0; }
	char bs[8]; booted(bs); kv_t st; char stp[300]; snprintf(stp, sizeof stp, "%s/state", SYSD); kv_read(stp, &st, '=');
	/* the spare slot: never the one running, never the last good one */
	const char *T = strcmp(bs, "a") && strcmp(kv(&st, "good"), "a") ? "a" : strcmp(bs, "b") && strcmp(kv(&st, "good"), "b") ? "b" : NULL;
	if (!T) die("no spare slot");
	char tp[300], tn[310]; snprintf(tp, sizeof tp, "%s/%s", SYSD, T); snprintf(tn, sizeof tn, "%s.new", tp);
	{ char *a[] = {"rm", "-rf", tn, NULL}; spawn(a); } mkdirs(tn);
	/* if a waiting update is being replaced, it is no longer waiting */
	if (!strcmp(kv(&st, "next"), T)) { kv_set(&st, "next", ""); kv_set(&st, "tries", "0"); kv_write(stp, &st, '='); }
	long long need = 0; for (int i = 0; i < m.nl; i++) need += m.L[i].size;
	struct statvfs v; if (!statvfs(SYSD, &v) && (long long)v.f_bavail * v.f_frsize < need / 2) die("not enough free space on RUDOLFDATA (about %lld MB needed)", need / 2 / 1048576);
	char base[300]; kv_t cf; kv_read(CONFP, &cf, '='); snprintf(base, sizeof base, "%s", kv(&cf, "url")); if (!*base) { char *d = slurp("/etc/rudolf/update-url", NULL); if (d) { snprintf(base, sizeof base, "%s", d); char *nl = strchr(base, '\n'); if (nl) *nl = 0; free(d); } }
	long long total = 0;
	for (int i = 0; i < NL; i++) {
		lay_t *y = mlayer(&m, LAYERS[i]); char out[400], hex[65];
		snprintf(out, sizeof out, "%s/%s%s", tn, y->name, strcmp(y->name, "vmlinuz") ? ".sfs" : "");
		/* the same file already on this computer: reuse it (hard link) or, for an unchanged disc layer, use the disc's */
		char cand[4][300]; int nc = 0;
		layer_path(bs, y->name, cand[nc], 300); nc++;
		layer_path(strcmp(bs, "a") ? "a" : "b", y->name, cand[nc], 300); nc++;
		layer_path("disc", y->name, cand[nc], 300); nc++;
		int done = 0;
		for (int c = 0; c < nc && !done; c++) {
			struct stat s; if (stat(cand[c], &s) || s.st_size != y->size || sha256_file(cand[c], hex) || strcmp(hex, y->sha)) continue;
			if (!strncmp(cand[c], MEDIA, strlen(MEDIA)) && strcmp(y->name, "vmlinuz")) { say("%s: unchanged (the disc's copy is used)", y->name); done = 1; break; }
			if (!link(cand[c], out)) { say("%s: unchanged", y->name); done = 1; break; }
			char *a[] = {"cp", cand[c], out, NULL}; if (!spawn(a)) { say("%s: unchanged (copied)", y->name); done = 1; }
		}
		if (done) continue;
		char fu[500], ru[510]; url_join(base, y->file, fu, sizeof fu); snprintf(ru, sizeof ru, "%s.rzs", fu);
		say("%s: downloading the changes (%lld MB file)", y->name, (y->size + 1048575) / 1048576);
		long long got = 0; int dr = delta_fetch(fu, ru, cand, nc, out, y->size, &got);
		if (dr || sha256_file(out, hex) || strcmp(hex, y->sha)) {
			say("  the delta did not work out; downloading the whole file");
			if (http_to_file(fu, out) || sha256_file(out, hex) || strcmp(hex, y->sha)) { say("%s: download failed or does not match the release list; try again later.", y->name); return 5; }
			got = y->size;
		}
		say("  downloaded %lld KB", (got + 1023) / 1024); total += got;
	}
	char rel[1200]; snprintf(rel, sizeof rel, "serial %lld\nversion %s\nreleased %s\nchromium %s\nkernel %s\nnotes %s\n", m.serial, m.version, m.released, m.chromium, m.kernel, m.notes);
	char rp[400]; snprintf(rp, sizeof rp, "%s/RELEASE", tn); write_file(rp, rel);
	/* the signed release list goes with the slot: the start-up checks the layers against its hashes (1.2.2) */
	{ char d1[420], d2[420]; snprintf(d1, sizeof d1, "%s/manifest.txt", tn); snprintf(d2, sizeof d2, "%s/manifest.sig", tn);
	  char *a1[] = {"cp", ap, d1, NULL}, *a2[] = {"cp", sig, d2, NULL}; if (spawn(a1) || spawn(a2)) die("could not keep the release list with the slot"); }
	snprintf(rp, sizeof rp, "%s/complete", tn); write_file(rp, "1\n");
	sync();
	{ char *a[] = {"rm", "-rf", tp, NULL}; spawn(a); } if (rename(tn, tp)) die("could not finish the slot");
	kv_read(stp, &st, '='); kv_set(&st, "next", T); kv_set(&st, "tries", "2"); kv_set(&st, "failed", "");
	if (!*kv(&st, "good")) kv_set(&st, "good", strcmp(bs, "disc") ? bs : "disc");
	kv_write(stp, &st, '='); sync();
	say("Ready: Rudolf OS %s starts the next time the computer restarts (%lld KB downloaded).", m.version, (total + 1023) / 1024);
	return 0;
}
static int cmd_bootok(void)
{
	char bs[8], stp[300]; booted(bs); snprintf(stp, sizeof stp, "%s/state", SYSD); kv_t st; kv_read(stp, &st, '=');
	if (!strcmp(bs, "disc") || strcmp(kv(&st, "next"), bs)) return 0;
	char g[300]; snprintf(g, sizeof g, "%s", kv(&st, "good"));
	kv_set(&st, "prev", g); kv_set(&st, "good", bs); kv_set(&st, "next", ""); kv_set(&st, "tries", "0"); kv_set(&st, "failed", "");
	kv_write(stp, &st, '='); sync();
	/* the new system brings its own desktop: a desktop installed earlier from a file no longer overrides it */
	if (!access("/persist/os/current/index.html", F_OK)) write_file("/persist/os/disabled", "system update\n");
	say("Keeping the updated system (slot %s).", bs); return 0;
}
static int cmd_rollback(void)
{
	char stp[300]; snprintf(stp, sizeof stp, "%s/state", SYSD); kv_t st; kv_read(stp, &st, '=');
	char g[300], p[300]; snprintf(g, sizeof g, "%s", kv(&st, "good")); snprintf(p, sizeof p, "%s", kv(&st, "prev"));
	if (!*g || !strcmp(g, "disc")) { say("Already using the system on the disc."); return 1; }
	if (*p && strcmp(p, "disc")) { char c[700]; snprintf(c, sizeof c, "%s/%s/complete", SYSD, p); if (access(c, F_OK)) snprintf(p, sizeof p, "disc"); }
	kv_set(&st, "good", *p ? p : "disc"); kv_set(&st, "prev", g); kv_set(&st, "next", ""); kv_set(&st, "tries", "0");
	kv_write(stp, &st, '='); sync(); say("The previous system starts the next time the computer restarts."); return 0;
}
static void slot_json(const char *s)
{
	char p[400]; snprintf(p, sizeof p, "%s/%s/RELEASE", SYSD, s); kv_t r; kv_read(p, &r, ' ');
	snprintf(p, sizeof p, "%s/%s/complete", SYSD, s);
	printf("{\"complete\":%s,\"serial\":%lld,\"version\":", access(p, F_OK) ? "false" : "true", atoll(kv(&r, "serial"))); jstr(kv(&r, "version"));
	printf(",\"released\":"); jstr(kv(&r, "released")); printf("}");
}
static int cmd_status(void)
{
	char bs[8], stp[300]; booted(bs); snprintf(stp, sizeof stp, "%s/state", SYSD); kv_t st; kv_read(stp, &st, '=');
	kv_t r; cur_release(&r); kv_t cf; kv_read(CONFP, &cf, '=');
	char *d = slurp("/etc/rudolf/update-url", NULL); if (d) { char *nl = strchr(d, '\n'); if (nl) *nl = 0; }
	printf("{\"ok\":true,\"persistent\":%s,\"booted\":", access("/run/rudolf/persistent", F_OK) ? "false" : "true"); jstr(bs);
	printf(",\"trying\":%s", strstr((access("/etc/rudolf/cmdline", F_OK) ? slurp("/proc/cmdline", NULL) : slurp("/etc/rudolf/cmdline", NULL)) ?: "", "rudolf.trying=1") ? "true" : "false");
	printf(",\"current\":{\"serial\":%lld,\"version\":", atoll(kv(&r, "serial"))); jstr(kv(&r, "version"));
	printf(",\"chromium\":"); jstr(kv(&r, "chromium")); printf(",\"kernel\":"); jstr(kv(&r, "kernel")); printf(",\"released\":"); jstr(kv(&r, "released")); printf(",\"notes\":"); jstr(kv(&r, "notes")); printf("}");
	printf(",\"state\":{\"good\":"); jstr(kv(&st, "good")); printf(",\"prev\":"); jstr(kv(&st, "prev")); printf(",\"next\":"); jstr(kv(&st, "next"));
	printf(",\"tries\":%d,\"failed\":", atoi(kv(&st, "tries"))); jstr(kv(&st, "failed")); printf("}");
	printf(",\"slots\":{\"a\":"); slot_json("a"); printf(",\"b\":"); slot_json("b"); printf("}");
	printf(",\"url\":"); jstr(*kv(&cf, "url") ? kv(&cf, "url") : d ? d : ""); printf(",\"auto\":%s", strcmp(kv(&cf, "auto"), "0") ? "true" : "false");
	printf(",\"checked\":%lld,\"available\":", atoll(kv(&cf, "checked")));
	char ap[300], sp[300]; snprintf(ap, sizeof ap, "%s/available.txt", SYSD); snprintf(sp, sizeof sp, "%s/available.sig", SYSD);
	size_t ml, sl; char *mt = slurp(ap, &ml), *sg = slurp(sp, &sl); man_t m; int okm = 0;
	if (mt && sg && sl == 64 && verify((unsigned char *)mt, ml, (unsigned char *)sg)) { char *c = strdup(mt); okm = !parse_manifest(c, &m); free(c); }
	if (okm && m.serial > atoll(kv(&r, "serial"))) {
		long long sz = 0; for (int i = 0; i < m.nl; i++) sz += m.L[i].size;
		printf("{\"serial\":%lld,\"version\":", m.serial); jstr(m.version); printf(",\"notes\":"); jstr(m.notes); printf(",\"released\":"); jstr(m.released);
		printf(",\"chromium\":"); jstr(m.chromium); printf(",\"kernel\":"); jstr(m.kernel); printf(",\"urgent\":%s,\"size\":%lld}", m.urgent ? "true" : "false", sz);
	} else printf("null");
	printf("}\n"); free(mt); free(sg); free(d);
	return 0;
}

int main(int argc, char **argv)
{
	if (getenv("RU_SYSD")) snprintf(SYSD, sizeof SYSD, "%s", getenv("RU_SYSD"));
	if (getenv("RU_MEDIA")) snprintf(MEDIA, sizeof MEDIA, "%s", getenv("RU_MEDIA"));
	if (getenv("RU_CONF")) snprintf(CONFP, sizeof CONFP, "%s", getenv("RU_CONF"));
	if (getenv("RU_KEYS")) snprintf(KEYF, sizeof KEYF, "%s", getenv("RU_KEYS"));
	if (getenv("RU_SLOT")) snprintf(SLOTF, sizeof SLOTF, "%s", getenv("RU_SLOT"));
	if (argc < 2) { fputs("usage: rudolf-update status|check|download|boot-ok|rollback|set url URL|set auto 0|1|rzs-make FILE OUT\n", stderr); return 2; }
	const char *c = argv[1];
	if (!strcmp(c, "rzs-make")) { if (argc < 4) die("usage: rzs-make FILE OUT.rzs"); return rzs_make(argv[2], argv[3]); }
	if (!strcmp(c, "delta") && argc >= 5) {
		/* delta URL OUT SIZE SEED... (testing: fetch one file using URL.rzs and the seed files) */
		char sd[4][300]; int n = 0; for (int i = 5; i < argc && n < 4; i++) snprintf(sd[n++], 300, "%s", argv[i]);
		char ru[600]; snprintf(ru, sizeof ru, "%s.rzs", argv[2]); long long got = 0;
		int r = delta_fetch(argv[2], ru, sd, n, argv[3], atoll(argv[4]), &got); say("result %d, downloaded %lld bytes", r, got); return r != 0;
	}
	if (!strcmp(c, "speedtest")) return cmd_speedtest();
	load_keys();
	if (!strcmp(c, "status")) return cmd_status();
	/* verify-manifest LIST SIG (1.2.4, install.sh): is this a release list signed with a trusted key? */
	if (!strcmp(c, "verify-manifest") && argc == 4) {
		size_t ml, sl; char *mt = slurp(argv[2], &ml), *sg = slurp(argv[3], &sl);
		if (!mt || !sg || sl != 64 || !verify((unsigned char *)mt, ml, (unsigned char *)sg)) { say("not signed by Rudolf OS"); return 1; }
		man_t m; char *cp = strdup(mt); if (parse_manifest(cp, &m)) { say("not a Rudolf OS release list"); return 1; }
		printf("serial %lld\nversion %s\n", m.serial, m.version); return 0;
	}
	if (access("/run/rudolf/persistent", F_OK) && !getenv("RU_SYSD")) { say("Updates need Rudolf OS installed (or a USB drive with a RUDOLFDATA partition)."); return 2; }
	mkdirs(SYSD); { char d[300]; snprintf(d, sizeof d, "%s", CONFP); char *s = strrchr(d, '/'); if (s) { *s = 0; mkdirs(d); } }
	if (!strcmp(c, "check")) return cmd_check(0);
	if (!strcmp(c, "download")) return cmd_download();
	if (!strcmp(c, "boot-ok")) return cmd_bootok();
	if (!strcmp(c, "rollback")) return cmd_rollback();
	if (!strcmp(c, "set") && argc >= 4) {
		kv_t cf; kv_read(CONFP, &cf, '=');
		if (!strcmp(argv[2], "url")) { const char *u = argv[3]; if (*u && strncmp(u, "https://", 8) && strncmp(u, "http://", 7)) die("the address starts with https://"); for (const char *q = u; *q; q++) if (!isalnum((unsigned char)*q) && !strchr("/:._-~%+", *q)) die("bad character in the address"); kv_set(&cf, "url", u); }
		else if (!strcmp(argv[2], "auto")) kv_set(&cf, "auto", strcmp(argv[3], "0") ? "1" : "0");
		else die("set url|auto");
		return kv_write(CONFP, &cf, '=') ? 1 : 0;
	}
	die("unknown command %s", c); return 2;
}
