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
 *   rudolf-update recovery          (1.5.9) refresh the official recovery system (slot r) from the running slot
 *   rudolf-update developer on KEY URL | off    (1.5.9, plan 8.3) Developer mode: one more trusted key (a developer's
 *                                   publisher key) and their update address; off: back to the official ones only
 *   rudolf-update return-official keep|erase    (1.5.9, plan 8.5) back to the official Rudolf OS at the next start
 *   rudolf-update stable-info       (1.6.0) JSON: the last stable release, whether this system is one, and whether
 *                                   going back to it is possible here (the data's layout); from what check saw
 *   rudolf-update to-stable         (1.6.0) Roll back to the last stable version: that release at the next start (from the
 *                                   other slot when it holds it, else downloaded), beta versions off
 *   rudolf-update set url URL | set auto 0|1
 *   rudolf-update rzs-make FILE OUT.rzs     (build side) write a delta control file
 *   rudolf-update fetch URL OUT             (1.5.7, Make a stick) a release's boot image from github.com */
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
static unsigned char KEYS[8][32]; static int NKEYS, NOFF;
/* 1.5.9 (plan 8.3): Developer mode's extra key, kept on RUDOLFDATA (never in the system image); the official keys
   (KEYF, the system image's list) come first, and only they count for the recovery system and "Official" */
static char EXTRAF[256] = "/persist/etc/rudolf/extra-keys";
static int key_line(char *l, unsigned char out[32]) { while (isspace((unsigned char)*l)) l++; if (*l == '#') return 0; char *e = l; while (*e && !isspace((unsigned char)*e)) e++; *e = 0; unsigned char raw[64]; if (strlen(l) == 44 && EVP_DecodeBlock(raw, (unsigned char *)l, 44) >= 32) { memcpy(out, raw, 32); return 1; } return 0; }
static void load_file_keys(const char *path) { char *t = slurp(path, NULL); if (!t) return; for (char *l = strtok(t, "\n"); l && NKEYS < 8; l = strtok(NULL, "\n")) if (key_line(l, KEYS[NKEYS])) NKEYS++; free(t); }
static void load_keys(void)
{
	NKEYS = 0; load_file_keys(KEYF); NOFF = NKEYS;
	if (!access(EXTRAF, F_OK)) load_file_keys(EXTRAF);
}
/* which key signed: 1..NKEYS, 0 for none (the official ones are 1..NOFF) */
static int verify(const unsigned char *msg, size_t len, const unsigned char *sig)
{
	for (int i = 0; i < NKEYS; i++) {
		EVP_PKEY *k = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, KEYS[i], 32); if (!k) continue;
		EVP_MD_CTX *c = EVP_MD_CTX_new(); int ok = EVP_DigestVerifyInit(c, NULL, NULL, NULL, k) == 1 && EVP_DigestVerify(c, sig, 64, msg, len) == 1;
		EVP_MD_CTX_free(c); EVP_PKEY_free(k); if (ok) return i + 1;
	}
	return 0;
}
static int official(int v) { return v > 0 && v <= NOFF; }
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
typedef struct { long long serial; char version[64], released[16], expires[16], notes[300], chromium[64], kernel[64]; int urgent; lay_t L[NL]; int nl;
	int datafmt, rmin, rmax; } man_t;   /* (1.6.0: the data layout the release keeps, and the oldest and newest it reads; 0: no record) */
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
		else if (!strncmp(l, "datafmt ", 8)) m->datafmt = atoi(l + 8);
		else if (!strncmp(l, "readsfmt ", 9)) { int a = 0, b = 0; if (sscanf(l + 9, "%d-%d", &a, &b) == 2 && a > 0 && b >= a) { m->rmin = a; m->rmax = b; } }
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
/* 1.5.8: r is a stick's rescue system (Make a stick keeps it; updates never write it, they only ever choose a or b) */
static void booted(char *s) { char *t = slurp(SLOTF, NULL); snprintf(s, 8, "%s", t && (t[0] == 'a' || t[0] == 'b' || t[0] == 'r') ? (t[1] = 0, t) : "disc"); free(t); }
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

/* 1.5.7: what this start is. A disc layer only counts as part of an update when the disc is on the same drive as
   RUDOLFDATA (an installed computer, or a USB drive with its own RUDOLFDATA): a USB stick or a disc used to start a
   computer goes away. On the Vivobook, 1.5.6 running from a rescue stick put an update in the spare slot that took
   its Chromium layer from the stick, and scheduled it: without the stick that system could not start.
   RU_CMDLINE, RU_MOUNTS and RU_SYSBLOCK move these for the host test. */
static char CMDF[256] = "/etc/rudolf/cmdline", MOUNTSF[256] = "/proc/mounts", SYSBLK[256] = "/sys/block";
static int recovery_start(void)
{
	char *c = slurp(CMDF, NULL); if (!c) c = slurp("/proc/cmdline", NULL);
	int r = c && strstr(c, "rudolf.recovery=1") != NULL; free(c); return r;
}
/* the device mounted on DIR (from /proc/mounts), or "" */
static void mount_dev(const char *dir, char *out, size_t n)
{
	out[0] = 0; char *t = slurp(MOUNTSF, NULL); if (!t) return;
	for (char *l = strtok(t, "\n"); l; l = strtok(NULL, "\n")) {
		char d[300], m[300]; if (sscanf(l, "%299s %299s", d, m) == 2 && !strcmp(m, dir)) { snprintf(out, n, "%s", d); break; }
	}
	free(t);
}
/* the whole disk under a device: /dev/nvme0n1p3 -> nvme0n1, /dev/sda1 -> sda, /dev/loop3 -> its backing disk */
static void disk_of(const char *dev, char *out, size_t n)
{
	const char *b = strrchr(dev, '/'); b = b ? b + 1 : dev; char name[64]; snprintf(name, sizeof name, "%s", b);
	if (!strncmp(name, "loop", 4)) {
		char p[400]; snprintf(p, sizeof p, "%s/%s/loop/backing_file", SYSBLK, name); char *f = slurp(p, NULL);
		if (f) { char *nl = strchr(f, '\n'); if (nl) *nl = 0; if (!strncmp(f, "/dev/", 5)) { disk_of(f, out, n); free(f); return; } free(f); }
		snprintf(out, n, "%s", name); return;
	}
	size_t l = strlen(name);
	if (!strncmp(name, "nvme", 4) || !strncmp(name, "mmcblk", 6)) { char *pp = strrchr(name, 'p'); if (pp && pp > name + 4 && isdigit((unsigned char)pp[1])) *pp = 0; }
	else { while (l && isdigit((unsigned char)name[l - 1])) name[--l] = 0; }
	snprintf(out, n, "%s", name);
}
static int media_permanent(void)
{
	char md[300], pd[300], a[64], b[64]; mount_dev(MEDIA, md, sizeof md); mount_dev("/persist", pd, sizeof pd);
	if (getenv("RU_PERSIST_DEV")) snprintf(pd, sizeof pd, "%s", getenv("RU_PERSIST_DEV"));
	if (!*md || !*pd) return 0;
	disk_of(md, a, sizeof a); disk_of(pd, b, sizeof b); return !strcmp(a, b);
}
/* updates change the installed system: not from a recovery start, and not from a system running off a stick or a
   disc that is not this computer's own drive */
static int may_change_system(int quiet)
{
	char bs[8]; snprintf(bs, sizeof bs, "%s", ""); { char *t = slurp(SLOTF, NULL); snprintf(bs, sizeof bs, "%s", t && (t[0] == 'a' || t[0] == 'b') ? "slot" : "disc"); free(t); }
	if (recovery_start()) { if (!quiet) say("Not in recovery mode: restart normally to update Rudolf OS."); return 0; }
	if (!strcmp(bs, "disc") && !media_permanent()) { if (!quiet) say("Not while Rudolf OS runs from a USB drive or disc: start it from this computer's own drive to update it."); return 0; }
	return 1;
}

/* (1.6.0: taken once per run: Return to official and Roll back to the last stable version download with the lock
   already held, and a second flock on a new descriptor of the same file is refused: "another update is running") */
static int LOCKED_;
static int lockf_(void) { if (LOCKED_) return 0; char p[300]; snprintf(p, sizeof p, "%s/.lock", SYSD); mkdirs(SYSD); int f = open(p, O_CREAT | O_RDWR | O_CLOEXEC, 0600); if (f >= 0 && !flock(f, LOCK_EX | LOCK_NB)) { LOCKED_ = 1; return 0; } return -1; }

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
/* 1.6.0 (owner, 2026-11-15): beta versions are opt-in. Settings › Update and recovery's "Get beta versions" (an
   administrator's name and password to turn on, checked by rudolfd) sets the official beta address; turned off, it
   sets the official stable address and "tostable": the next update brings the latest stable release even when its
   number is lower than the beta's that runs (the beta's numbers run ahead of stable's). Set by "set channel". */
#ifndef OFFICIAL_URL
#define OFFICIAL_URL "https://merciless146.github.io/RudolfOS/stable"
#endif
#include "origin.h"   /* 1.6.0 (plan Part 19): a fork's build switches between the fork's own channels */
static const char *off_url(int beta)
{
	static char b[2][300], o[300]; snprintf(o, sizeof o, "%s/stable", origin_get("pages"));
	const char *st = getenv("RU_OFFICIAL_URL") ? getenv("RU_OFFICIAL_URL") : o;
	if (!beta) { snprintf(b[0], sizeof b[0], "%s", st); return b[0]; }
	if (getenv("RU_OFFICIAL_BETA")) { snprintf(b[1], sizeof b[1], "%s", getenv("RU_OFFICIAL_BETA")); return b[1]; }
	size_t n = strlen(st); if (n > 7 && !strcmp(st + n - 7, "/stable")) snprintf(b[1], sizeof b[1], "%.*s/beta", (int)(n - 7), st); else snprintf(b[1], sizeof b[1], "%s", st);
	return b[1];
}
static void eff_url(kv_t *cf, char *o, size_t n)
{
	snprintf(o, n, "%s", kv(cf, "url"));
	if (!*o) { char *d = slurp("/etc/rudolf/update-url", NULL); if (d) { snprintf(o, n, "%s", d); char *nl = strchr(o, '\n'); if (nl) *nl = 0; free(d); } }
	size_t l = strlen(o); while (l && o[l - 1] == '/') o[--l] = 0;
}
static int on_beta(kv_t *cf) { char u[300]; eff_url(cf, u, sizeof u); return !strcmp(u, off_url(1)); }
/* whether a release (its serial) is the one to install: newer, or (leaving the betas) the stable one, whatever its number */
static int wanted(long long serial, long long cur)
{
	if (serial > cur) return 1;
	kv_t cf; kv_read(CONFP, &cf, '='); return !strcmp(kv(&cf, "tostable"), "1") && serial != cur && !on_beta(&cf);
}
/* 1.6.0 (the owner, 2026-11-16): Roll back to the last stable version, and what a release keeps of the person's data.
   Each release's list says the layout of the data it keeps ("datafmt N") and the layouts it can read ("readsfmt LO-HI");
   this system's own are below. Going back to an older release is refused, with what would break, when it cannot read
   the data on this computer. Releases published without the record are known by their versions (rel_fmt). Layouts:
     1  every account's files belong to the desktop's Linux user (rudolf, uid 1000): Rudolf OS before 1.5.9
     2  each account is its own Linux user (uid 1001 and up), its folder and files moved to it and its alone: 1.5.9
        (docs/plan-1.5.9.md 15.3), and 1.6.0, which only adds beside them (the keyring's files in the account's folder,
        signed system extensions in /persist/pkg, Messages, the month's mobile data): 1.5.9 leaves those alone.
   iso/datafmt (the build's record in each release list) must say the same: qt/host/tostable60_t.sh checks it. */
#define DATAFMT 2
#define READS_MIN 1
#define READS_MAX 2
static int ver_cmp(const char *v, int a, int b, int c) { int x = 0, y = 0, z = 0; sscanf(v ? v : "", "%d.%d.%d", &x, &y, &z); return x != a ? x - a : y != b ? y - b : z - c; }
static void rel_fmt(const man_t *m, int *f, int *lo, int *hi)
{
	if (m->datafmt > 0) { *f = m->datafmt; *lo = m->rmin > 0 ? m->rmin : m->datafmt; *hi = m->rmax > 0 ? m->rmax : m->datafmt; return; }
	/* (no record: 1.5.9 reads both layouts (it moves the first to the second at its first start) and keeps the second,
	   tested on the released 1.5.9 (qt/sb/t160r_t.py); anything older knows only the first) */
	if (ver_cmp(m->version, 1, 5, 9) >= 0) { *f = 2; *lo = 1; *hi = 2; } else { *f = 1; *lo = 1; *hi = 1; }
}
/* the layout of the data on this computer: the newest any release that started here wrote down (boot-ok), and at least
   this system's own (it is running on it now). /persist/os/datafmt, beside the system's slots */
static void dfmt_path(char *o, size_t n) { snprintf(o, n, "%s", SYSD); char *sl = strrchr(o, '/'); if (sl && sl != o) *sl = 0; strncat(o, "/datafmt", n - strlen(o) - 1); }
static int data_fmt(void) { char p[320]; dfmt_path(p, sizeof p); char *t = slurp(p, NULL); int f = t ? atoi(t) : 0; free(t); return f > DATAFMT ? f : DATAFMT; }
static void data_fmt_mark(void) { char p[320], v[16]; dfmt_path(p, sizeof p); int f = data_fmt(); char *t = slurp(p, NULL); int was = t ? atoi(t) : 0; free(t); if (was != f) { snprintf(v, sizeof v, "%d\n", f); write_file(p, v); } }
static const char *fmt_what(int f)
{
	return f == 1 ? "every account's files belong to one Linux user, the desktop's (as before Rudolf OS 1.5.9)"
	     : f == 2 ? "each account is its own Linux user, its folder and files belonging to it alone (since Rudolf OS 1.5.9)"
	     : "the accounts' files are kept in a newer way";
}
/* can release M read this computer's data? 1, or 0 with why (in plain words) */
static int compat(const man_t *m, char *why, size_t n)
{
	int f, lo, hi; rel_fmt(m, &f, &lo, &hi); int d = data_fmt();
	if (d >= lo && d <= hi) return 1;
	if (d > hi) snprintf(why, n, "Rudolf OS %s cannot read the accounts and files on this computer: here %s, and Rudolf OS %s only knows the way where %s. "
		"Going back to it would leave accounts whose folders and files it cannot open, so nothing was changed.", m->version, fmt_what(d), m->version, fmt_what(hi));
	else snprintf(why, n, "Rudolf OS %s no longer reads the accounts and files kept the way this computer keeps them (%s), so nothing was changed.", m->version, fmt_what(d));
	return 0;
}
/* what is new since an older release and not used by it while it runs (kept on the computer for when this one comes back) */
static const struct { int a, b, c; const char *what; } NOTUSED[] = {
	{1, 6, 0, "Linux apps' sign-ins saved in the keyring (new in 1.6.0) are not used: those apps may ask you to sign in again"},
	{1, 6, 0, "system extensions (such as USB printers) are not used"},
	{1, 6, 0, "Messages and the month's mobile data count are kept but not shown"},
};
static void notes_for(const man_t *m, void (*each)(const char *))
{
	for (size_t i = 0; i < sizeof NOTUSED / sizeof *NOTUSED; i++) if (ver_cmp(m->version, NOTUSED[i].a, NOTUSED[i].b, NOTUSED[i].c) < 0) each(NOTUSED[i].what);
}
static int COMPAT_GATE;   /* (to-stable: the release downloaded must read this computer's data) */
static void stable_fetch(void);
static int cmd_check(int quiet)
{
	kv_t cf; kv_read(CONFP, &cf, '='); const char *url = kv(&cf, "url");
	char def[300] = ""; if (!*url) { char *d = slurp("/etc/rudolf/update-url", NULL); if (d) { snprintf(def, sizeof def, "%s", d); char *nl = strchr(def, '\n'); if (nl) *nl = 0; free(d); } url = def; }
	if (!*url) { say("No update address is set (Settings > Update and recovery)."); return 2; }
	char mu[400], su[400], mp[300], sp[300]; snprintf(mu, sizeof mu, "%s/manifest.txt", url); snprintf(su, sizeof su, "%s/manifest.sig", url);
	snprintf(mp, sizeof mp, "%s/available.txt.dl", SYSD); snprintf(sp, sizeof sp, "%s/available.sig.dl", SYSD);
	if (!quiet) say("Checking %s", url);
	stable_fetch();   /* (1.6.0: the last stable release, for Roll back to the last stable version) */
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
	else if (wanted(m.serial, cur)) say("Update available: Rudolf OS %s, the latest stable release (beta versions were turned off)", m.version);
	else say("Rudolf OS is up to date (%s).", kv(&r, "version")[0] ? kv(&r, "version") : "this release");
	free(t); free(g);
	return 0;
}
static int FORCE_DL;   /* (1.5.9: Return to official: the official release even when the custom build's number is higher) */
static int cmd_download(void)
{
	if (!FORCE_DL && !may_change_system(0)) return 6;
	if (lockf_()) die("another update is running");
	int r = cmd_check(1); if (r) return r;
	char ap[300]; snprintf(ap, sizeof ap, "%s/available.txt", SYSD); char *t = slurp(ap, NULL); man_t m; if (!t || parse_manifest(t, &m)) die("no release list");
	char sig[300]; snprintf(sig, sizeof sig, "%s/available.sig", SYSD);
	{ size_t ml, sl; char *mt = slurp(ap, &ml), *sg = slurp(sig, &sl); int ok = mt && sg && sl == 64 && verify((unsigned char *)mt, ml, (unsigned char *)sg); free(mt); free(sg); if (!ok) die("the saved release list is not signed"); }
	kv_t cr; cur_release(&cr);
	if (!FORCE_DL && !wanted(m.serial, atoll(kv(&cr, "serial")))) { say("Already up to date."); return 0; }
	{ char why[900]; if (COMPAT_GATE && !compat(&m, why, sizeof why)) { say("%s", why); return 8; } }
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
	long long total = 0; int permanent = media_permanent();
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
			/* (1.5.7: only a disc on this computer's own drive; one that goes away is copied into the slot) */
			if (!strncmp(cand[c], MEDIA, strlen(MEDIA)) && strcmp(y->name, "vmlinuz") && permanent) { say("%s: unchanged (the disc's copy is used)", y->name); done = 1; break; }
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
	char rel[1200]; int rl = snprintf(rel, sizeof rel, "serial %lld\nversion %s\nreleased %s\nchromium %s\nkernel %s\nnotes %s\n", m.serial, m.version, m.released, m.chromium, m.kernel, m.notes);
	if (m.datafmt > 0 && rl > 0 && rl < (int)sizeof rel - 40) snprintf(rel + rl, sizeof rel - rl, "datafmt %d\nreadsfmt %d-%d\n", m.datafmt, m.rmin > 0 ? m.rmin : m.datafmt, m.rmax > 0 ? m.rmax : m.datafmt);
	char rp[400]; snprintf(rp, sizeof rp, "%s/RELEASE", tn); write_file(rp, rel);
	/* the signed release list goes with the slot: the start-up checks the layers against its hashes (1.2.2) */
	{ char d1[420], d2[420]; snprintf(d1, sizeof d1, "%s/manifest.txt", tn); snprintf(d2, sizeof d2, "%s/manifest.sig", tn);
	  char *a1[] = {"cp", ap, d1, NULL}, *a2[] = {"cp", sig, d2, NULL}; if (spawn(a1) || spawn(a2)) die("could not keep the release list with the slot"); }
	/* 1.5.7: complete before it is scheduled: every file in the slot at its listed size, or (a layer) on a disc that
	   is part of this computer's own drive */
	for (int i = 0; i < NL; i++) {
		lay_t *y = mlayer(&m, LAYERS[i]); char f[420]; struct stat s2;
		snprintf(f, sizeof f, "%s/%s%s", tn, y->name, strcmp(y->name, "vmlinuz") ? ".sfs" : "");
		if (!stat(f, &s2) && s2.st_size == y->size) continue;
		if (strcmp(y->name, "vmlinuz") && permanent) { snprintf(f, sizeof f, "%s/rudolf/%s.sfs", MEDIA, y->name); if (!stat(f, &s2) && s2.st_size == y->size) continue; }
		say("%s: missing from the new system; it is not used.", y->name);
		{ char *a[] = {"rm", "-rf", tn, NULL}; spawn(a); } return 7;
	}
	snprintf(rp, sizeof rp, "%s/complete", tn); write_file(rp, "1\n");
	sync();
	{ char *a[] = {"rm", "-rf", tp, NULL}; spawn(a); } if (rename(tn, tp)) die("could not finish the slot");
	{ kv_t c2; kv_read(CONFP, &c2, '='); if (*kv(&c2, "tostable")) { kv_set(&c2, "tostable", ""); kv_write(CONFP, &c2, '='); } }
	kv_read(stp, &st, '='); kv_set(&st, "next", T); kv_set(&st, "tries", "2"); kv_set(&st, "failed", "");
	if (!*kv(&st, "good")) kv_set(&st, "good", strcmp(bs, "disc") ? bs : "disc");
	kv_write(stp, &st, '='); sync();
	say("Ready: Rudolf OS %s starts the next time the computer restarts (%lld KB downloaded).", m.version, (total + 1023) / 1024);
	return 0;
}
/* 1.5.9 (docs/plan-1.5.9.md 8.4): the official recovery system, slot r: the newest official release that started
   successfully on this computer. Refreshed from the running slot after it proved itself (boot-ok), only when that
   slot's release list is signed by the official key (KEYF, the system image's own list: never a key added later),
   and only by a newer release. Hard links (no extra room while the slot keeps the same files); a layer the slot
   takes from the disc on this drive is linked or copied from there. Recovery mode starts slot r first (initramfs).
   Updates never write it (they go to a or b). */
static int official_slot_(const char *s);
static int recovery_refresh(const char *bs)
{
	if (strcmp(bs, "a") && strcmp(bs, "b")) return 0;
	char sp[300], mp[400], gp[400]; snprintf(sp, sizeof sp, "%s/%s", SYSD, bs);
	snprintf(mp, sizeof mp, "%s/manifest.txt", sp); snprintf(gp, sizeof gp, "%s/manifest.sig", sp);
	size_t ml, sl; char *mt = slurp(mp, &ml), *sg = slurp(gp, &sl);
	int off = mt && sg && sl == 64 && official(verify((unsigned char *)mt, ml, (unsigned char *)sg));
	man_t m; char *cp = mt ? strdup(mt) : NULL; if (off && parse_manifest(cp, &m)) off = 0;
	free(sg); if (!off) { free(mt); free(cp); say("Slot %s is not an official release: the recovery system is kept as it is.", bs); return 1; }
	char rp[300], rr[400]; snprintf(rp, sizeof rp, "%s/r", SYSD); snprintf(rr, sizeof rr, "%s/RELEASE", rp);
	kv_t r0; kv_read(rr, &r0, ' '); char ck[400]; snprintf(ck, sizeof ck, "%s/complete", rp);
	if (!access(ck, F_OK) && atoll(kv(&r0, "serial")) >= m.serial) { free(mt); free(cp); return 0; }
	char tn[310]; snprintf(tn, sizeof tn, "%s.new", rp); { char *a[] = {"rm", "-rf", tn, NULL}; spawn(a); } mkdirs(tn);
	int bad = 0;
	for (int i = 0; i < NL && !bad; i++) {
		lay_t *y = mlayer(&m, LAYERS[i]); if (!y) { bad = 1; break; }
		const char *ext = strcmp(y->name, "vmlinuz") ? ".sfs" : "";
		char from[420], to[420]; struct stat s2; snprintf(to, sizeof to, "%s/%s%s", tn, y->name, ext);
		snprintf(from, sizeof from, "%s/%s%s", sp, y->name, ext);
		if (stat(from, &s2) || s2.st_size != y->size) { snprintf(from, sizeof from, "%s/rudolf/%s%s", MEDIA, y->name, ext); if (stat(from, &s2) || s2.st_size != y->size) { bad = 1; break; } }
		if (link(from, to)) { char *a[] = {"cp", from, to, NULL}; if (spawn(a)) bad = 1; }
	}
	char f1[420], f2[420], f3[420];
	snprintf(f1, sizeof f1, "%s/RELEASE", sp); snprintf(f2, sizeof f2, "%s/RELEASE", tn);
	{ char *a[] = {"cp", f1, f2, NULL}; if (!bad && spawn(a)) bad = 1; }
	snprintf(f2, sizeof f2, "%s/manifest.txt", tn); snprintf(f3, sizeof f3, "%s/manifest.sig", tn);
	{ char *a1[] = {"cp", mp, f2, NULL}, *a2[] = {"cp", gp, f3, NULL}; if (!bad && (spawn(a1) || spawn(a2))) bad = 1; }
	free(mt); free(cp);
	if (bad) { char *a[] = {"rm", "-rf", tn, NULL}; spawn(a); say("The recovery system could not be refreshed (a file is missing); the previous one is kept."); return 1; }
	char note[300]; time_t now = time(NULL); char ts[40]; strftime(ts, sizeof ts, "%Y-%m-%d %H:%M", localtime(&now));
	snprintf(note, sizeof note, "official recovery system: release %lld, from slot %s, %s; only official releases update it\n", m.serial, bs, ts);
	snprintf(f2, sizeof f2, "%s/official", tn); write_file(f2, note);
	snprintf(f2, sizeof f2, "%s/complete", tn); write_file(f2, "1\n"); sync();
	{ char *a[] = {"rm", "-rf", rp, NULL}; spawn(a); } if (rename(tn, rp)) { say("could not finish the recovery system"); return 1; }
	sync(); say("Recovery system: Rudolf OS release %lld (official).", m.serial); return 0;
}
/* 1.6.0 (docs/plan-1.6.0.md Part 15; 1.5.9 8.4): the official recovery system checked at each start (boot-ok) and
   repaired. Its release list must be signed by the official key and every file there at its listed size; once a week
   (r/checked), every file's SHA-256 too. Anything wrong: it is made again from this system when this one is an
   official release, or (an official one is not running) left for the next official one, said either way. */
static int recovery_ok(int full)
{
	char rp[300], mp[400], gp[400], ck[400], om[400]; snprintf(rp, sizeof rp, "%s/r", SYSD);
	snprintf(ck, sizeof ck, "%s/complete", rp); snprintf(om, sizeof om, "%s/official", rp);
	if (access(ck, F_OK) || access(om, F_OK)) return 0;
	snprintf(mp, sizeof mp, "%s/manifest.txt", rp); snprintf(gp, sizeof gp, "%s/manifest.sig", rp);
	size_t ml, sl; char *mt = slurp(mp, &ml), *sg = slurp(gp, &sl);
	int ok = mt && sg && sl == 64 && official(verify((unsigned char *)mt, ml, (unsigned char *)sg)); free(sg);
	man_t m; char *cp = mt ? strdup(mt) : NULL; if (ok && parse_manifest(cp, &m)) ok = 0;
	for (int i = 0; ok && i < NL; i++) {
		lay_t *y = mlayer(&m, LAYERS[i]); if (!y) { ok = 0; break; }
		char f[420], hex[65]; struct stat s2; snprintf(f, sizeof f, "%s/%s%s", rp, y->name, strcmp(y->name, "vmlinuz") ? ".sfs" : "");
		if (stat(f, &s2) || s2.st_size != y->size) { say("The official recovery system's %s is missing or changed.", y->name); ok = 0; break; }
		if (full && (sha256_file(f, hex) || strcmp(hex, y->sha))) { say("The official recovery system's %s does not match its signed release list.", y->name); ok = 0; }
	}
	free(mt); free(cp); return ok;
}
static void recovery_check(const char *bs)
{
	char rp[300], ck[400], ts[420]; snprintf(rp, sizeof rp, "%s/r", SYSD); snprintf(ck, sizeof ck, "%s/complete", rp); snprintf(ts, sizeof ts, "%s/checked", rp);
	if (access(rp, F_OK)) return;   /* (none yet: recovery_refresh makes it from the first official system that starts) */
	/* (only one that says it is the official recovery system: a 1.5.8 stick's rescue system has no mark, and is left) */
	{ char om[420]; snprintf(om, sizeof om, "%s/official", rp); if (access(om, F_OK) || access(ck, F_OK)) return; }
	char *t = slurp(ts, NULL); long long last = t ? atoll(t) : 0; free(t); time_t now = time(NULL);
	int full = now - last > 7 * 86400;
	if (recovery_ok(full)) { if (full) { char n[32]; snprintf(n, sizeof n, "%lld\n", (long long)now); write_file(ts, n); } return; }
	char tn[320]; snprintf(tn, sizeof tn, "%s.bad", rp); { char *a[] = {"rm", "-rf", tn, NULL}; spawn(a); } rename(rp, tn); { char *a[] = {"rm", "-rf", tn, NULL}; spawn(a); }
	if (official_slot_(bs)) { say("The official recovery system was not as it should be: it is made again from this official system.");
		if (!recovery_refresh(bs)) { char n[32]; snprintf(n, sizeof n, "%lld\n", (long long)now); write_file(ts, n); } }
	else say("The official recovery system was not as it should be and was removed: the next official release that starts makes it again (or Developer mode downloads it).");
}
/* 1.6.0 (1.5.9 8.4): the official recovery system downloaded, for a computer that never ran an official release (a
   developer's own disc, Developer mode on): the newest release at the official address (OFFICIAL_URL, not the
   developer's), its list checked with the official key only, its files with their SHA-256; files already on this
   computer with the same SHA-256 are linked instead. Nothing is started from it except by Recovery mode. */
#ifndef OFFICIAL_URL
#define OFFICIAL_URL "https://merciless146.github.io/RudolfOS/stable"
#endif
static int cmd_recovery_fetch(void)
{
	if (recovery_start()) { say("Not while in recovery mode."); return 1; }
	if (recovery_ok(0)) { say("The official recovery system is already on this computer."); return 0; }
	if (lockf_()) die("another update is running");
	const char *url = getenv("RU_OFFICIAL_URL") ? getenv("RU_OFFICIAL_URL") : OFFICIAL_URL;
	char rp[300], tn[310], mu[400], su[400], mp[420], sp[420]; snprintf(rp, sizeof rp, "%s/r", SYSD); snprintf(tn, sizeof tn, "%s.new", rp);
	{ char *a[] = {"rm", "-rf", tn, NULL}; spawn(a); } mkdirs(tn);
	snprintf(mu, sizeof mu, "%s/manifest.txt", url); snprintf(su, sizeof su, "%s/manifest.sig", url);
	snprintf(mp, sizeof mp, "%s/manifest.txt", tn); snprintf(sp, sizeof sp, "%s/manifest.sig", tn);
	say("Downloading the official recovery system from %s", url);
	if (http_to_file(mu, mp) || http_to_file(su, sp)) { say("Could not reach the official update address."); return 3; }
	size_t ml, sl; char *mt = slurp(mp, &ml), *sg = slurp(sp, &sl);
	int ok = mt && sg && sl == 64 && official(verify((unsigned char *)mt, ml, (unsigned char *)sg)); free(sg);
	man_t m; char *cp = mt ? strdup(mt) : NULL; if (ok && parse_manifest(cp, &m)) ok = 0;
	if (!ok) { free(mt); free(cp); { char *a[] = {"rm", "-rf", tn, NULL}; spawn(a); } say("Refused: the release list there is not signed by the official Rudolf OS key."); return 4; }
	long long need = 0; for (int i = 0; i < m.nl; i++) need += m.L[i].size;
	struct statvfs v; if (!statvfs(SYSD, &v) && (long long)v.f_bavail * v.f_frsize < need) { free(mt); free(cp); { char *a[] = {"rm", "-rf", tn, NULL}; spawn(a); } die("not enough free space on RUDOLFDATA (about %lld MB needed)", need / 1048576); }
	for (int i = 0; i < NL; i++) {
		lay_t *y = mlayer(&m, LAYERS[i]); char out[420], hex[65], fu[500], c3[3][300]; int done = 0;
		snprintf(out, sizeof out, "%s/%s%s", tn, y->name, strcmp(y->name, "vmlinuz") ? ".sfs" : "");
		layer_path("a", y->name, c3[0], 300); layer_path("b", y->name, c3[1], 300); layer_path("disc", y->name, c3[2], 300);
		for (int c = 0; c < 3 && !done; c++) { struct stat s; if (stat(c3[c], &s) || s.st_size != y->size || sha256_file(c3[c], hex) || strcmp(hex, y->sha)) continue;
			if (!link(c3[c], out)) done = 1; else { char *a[] = {"cp", c3[c], out, NULL}; done = !spawn(a); } }
		if (done) { say("%s: already on this computer", y->name); continue; }
		url_join(url, y->file, fu, sizeof fu); say("%s: downloading (%lld MB)", y->name, (y->size + 1048575) / 1048576);
		if (http_to_file(fu, out) || sha256_file(out, hex) || strcmp(hex, y->sha)) { free(mt); free(cp); { char *a[] = {"rm", "-rf", tn, NULL}; spawn(a); } say("%s: the download failed or does not match the official release list; try again later.", y->name); return 5; }
	}
	char rel[1200], f[420]; snprintf(rel, sizeof rel, "serial %lld\nversion %s\nreleased %s\nchromium %s\nkernel %s\nnotes %s\n", m.serial, m.version, m.released, m.chromium, m.kernel, m.notes);
	snprintf(f, sizeof f, "%s/RELEASE", tn); write_file(f, rel);
	char note[300], ts[40]; time_t now = time(NULL); strftime(ts, sizeof ts, "%Y-%m-%d %H:%M", localtime(&now));
	snprintf(note, sizeof note, "official recovery system: release %lld, downloaded from the official address, %s; only official releases update it\n", m.serial, ts);
	snprintf(f, sizeof f, "%s/official", tn); write_file(f, note); snprintf(f, sizeof f, "%s/complete", tn); write_file(f, "1\n"); sync();
	{ char *a[] = {"rm", "-rf", rp, NULL}; spawn(a); } if (rename(tn, rp)) die("could not finish the recovery system");
	free(mt); free(cp); sync(); say("Recovery system: Rudolf OS %s (official), downloaded.", m.version); return 0;
}
/* 1.6.0: started on its own (a new session; its output to LOG), so the caller does not wait for it */
static void detach(char *const argv[], const char *log)
{
	pid_t p = fork(); if (p) { if (p > 0) waitpid(p, NULL, 0); return; }
	if (fork()) _exit(0);
	setsid(); int f = open(log, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600), z = open("/dev/null", O_RDONLY);
	if (z >= 0) dup2(z, 0); if (f >= 0) { dup2(f, 1); dup2(f, 2); }
	execv(argv[0], argv); _exit(127);
}
/* 1.5.9 (docs/plan-1.5.9.md 8.5): Return to official Rudolf OS, from Settings or from Recovery mode. Developer mode
   off (the extra key and the developer's address gone); the official recovery system (slot r) put into the spare slot,
   or, without one, the newest official release downloaded there (checked with the official key only); it starts at the
   next restart (ret=1: once it has started, boot-ok clears every slot that is not official). erase: a factory reset at
   that start too (accounts, files and apps; the system slots stay). From a USB stick only when it is this computer's
   own drive. */
static int official_slot(const char *s)
{
	char mp[400], gp[400]; snprintf(mp, sizeof mp, "%s/%s/manifest.txt", SYSD, s); snprintf(gp, sizeof gp, "%s/%s/manifest.sig", SYSD, s);
	size_t ml, sl; char *mt = slurp(mp, &ml), *sg = slurp(gp, &sl); int v = mt && sg && sl == 64 ? verify((unsigned char *)mt, ml, (unsigned char *)sg) : 0;
	free(mt); free(sg); return official(v);
}
static int official_slot_(const char *s) { return official_slot(s); }
static int cmd_return(int erase)
{
	char bs[8]; booted(bs);
	if (!strcmp(bs, "disc") && !media_permanent()) { say("Not while Rudolf OS runs from a USB drive or disc: start this computer's own Rudolf OS (or its Recovery mode)."); return 6; }
	if (lockf_()) die("another update is running");
	unlink(EXTRAF); { kv_t cf; kv_read(CONFP, &cf, '='); kv_set(&cf, "url", ""); kv_set(&cf, "devurl", ""); kv_write(CONFP, &cf, '='); }
	load_keys();   /* the official keys only from here on */
	char stp[300]; snprintf(stp, sizeof stp, "%s/state", SYSD); kv_t st; kv_read(stp, &st, '=');
	char rc[400]; snprintf(rc, sizeof rc, "%s/r/complete", SYSD); char ro[400]; snprintf(ro, sizeof ro, "%s/r/official", SYSD);
	const char *T = NULL;
	if (!access(rc, F_OK) && !access(ro, F_OK) && official_slot("r")) {
		/* the spare slot: not the one running, not the last good one (the custom build) */
		T = strcmp(bs, "a") && strcmp(kv(&st, "good"), "a") ? "a" : strcmp(bs, "b") && strcmp(kv(&st, "good"), "b") ? "b" : NULL;
		if (!T) T = strcmp(kv(&st, "good"), "a") ? "a" : "b";
		char tp[300], tn[310]; snprintf(tp, sizeof tp, "%s/%s", SYSD, T); snprintf(tn, sizeof tn, "%s.new", tp);
		{ char *a[] = {"rm", "-rf", tn, NULL}; spawn(a); } mkdirs(tn);
		char rd_[300]; snprintf(rd_, sizeof rd_, "%s/r", SYSD); DIR *d = opendir(rd_);
		struct dirent *e; int bad = !d;
		while (d && (e = readdir(d))) { if (e->d_name[0] == '.' || !strcmp(e->d_name, "official")) continue;
			char f1[600], f2[600]; snprintf(f1, sizeof f1, "%s/%s", rd_, e->d_name); snprintf(f2, sizeof f2, "%s/%s", tn, e->d_name);
			if (link(f1, f2)) { char *a[] = {"cp", f1, f2, NULL}; if (spawn(a)) bad = 1; } }
		if (d) closedir(d);
		if (bad) { char *a[] = {"rm", "-rf", tn, NULL}; spawn(a); die("could not copy the official recovery system"); }
		sync(); { char *a[] = {"rm", "-rf", tp, NULL}; spawn(a); } if (rename(tn, tp)) die("could not finish the slot");
		kv_read(stp, &st, '='); kv_set(&st, "next", T); kv_set(&st, "tries", "2"); kv_set(&st, "failed", "");
	} else {
		say("No official recovery system on this computer yet: the newest official Rudolf OS is downloaded.");
		FORCE_DL = 1; int r = cmd_download(); FORCE_DL = 0; if (r) return r;
		kv_read(stp, &st, '='); T = kv(&st, "next");
		if (!*T || !official_slot(T)) die("the official release could not be prepared");
	}
	kv_set(&st, "ret", "1"); kv_write(stp, &st, '='); sync();
	if (erase) { char fr[400]; snprintf(fr, sizeof fr, "%s/../../.factory-reset", SYSD); write_file(fr, "1\n"); sync(); }
	char rp[400]; snprintf(rp, sizeof rp, "%s/%s/RELEASE", SYSD, T); kv_t rl; kv_read(rp, &rl, ' ');
	say("Returning to the official Rudolf OS %s: it starts the next time the computer restarts%s.", kv(&rl, "version"), erase ? ", with everything erased (a factory reset)" : ", with your accounts, files and apps");
	/* 1.6.0 (8.5 step 3): the official start-up files (rudolf.efi) put back from that release's boot image: staged now
	   (espcheck downloads and checks it), or at the official system's first start (boot-ok) if that fails */
	if (!getenv("RU_NOESP")) { const char *e = getenv("RU_ESPCHECK") ? getenv("RU_ESPCHECK") : "/usr/lib/rudolf/espcheck";
		if (!access(e, X_OK)) { char *a[] = {(char *)e, "repair", (char *)T, NULL}; detach(a, "/run/rudolf/espcheck.log"); } }
	return 0;
}
/* (after Return to official: the official system started; the custom build's slots are cleared) */
static void return_done(const char *bs, kv_t *st, const char *stp)
{
	if (strcmp(kv(st, "ret"), "1") || !official_slot(bs)) return;
	const char *sl[2] = {"a", "b"};
	for (int i = 0; i < 2; i++) { if (!strcmp(sl[i], bs) || official_slot(sl[i])) continue;
		char p[300]; snprintf(p, sizeof p, "%s/%s", SYSD, sl[i]); { char *a[] = {"rm", "-rf", p, NULL}; spawn(a); }
		if (!strcmp(kv(st, "prev"), sl[i])) kv_set(st, "prev", ""); say("Cleared the custom build in slot %s.", sl[i]); }
	kv_set(st, "ret", ""); kv_write(stp, st, '='); sync(); say("Back to the official Rudolf OS.");
}
/* 1.6.0 (plan Part 15; 1.5.9 8.4, 8.5): the start-up files on the drive (rudolf.efi, recovery.efi) checked by the
   official system at each start, and the official ones put back from its release's boot image when they are not
   (/usr/lib/rudolf/espcheck, in the background: it may download) */
static void esp_check(const char *bs)
{
	if (!official_slot_(bs) || getenv("RU_NOESP")) return;
	const char *e = getenv("RU_ESPCHECK") ? getenv("RU_ESPCHECK") : "/usr/lib/rudolf/espcheck";
	if (access(e, X_OK)) return;
	char *a[] = {(char *)e, "boot", (char *)bs, NULL}; detach(a, getenv("RU_ESPLOG") ? getenv("RU_ESPLOG") : "/run/rudolf/espcheck.log");
}
static int cmd_bootok(void)
{
	char bs[8], stp[300]; booted(bs); snprintf(stp, sizeof stp, "%s/state", SYSD); kv_t st; kv_read(stp, &st, '=');
	data_fmt_mark();   /* (1.6.0: the layout of the data this system keeps, for Roll back to the last stable version) */
	if (!strcmp(bs, "disc")) return 0;
	if (recovery_start()) return 0;   /* 1.5.7: a desktop in recovery mode does not prove the normal start works */
	/* (1.5.9: the last good system, started again: the recovery system is made if it is missing or older) */
	if (strcmp(kv(&st, "next"), bs)) { if (!strcmp(kv(&st, "good"), bs)) { recovery_check(bs); recovery_refresh(bs); esp_check(bs); } return 0; }
	char g[300]; snprintf(g, sizeof g, "%s", kv(&st, "good"));
	kv_set(&st, "prev", g); kv_set(&st, "good", bs); kv_set(&st, "next", ""); kv_set(&st, "tries", "0"); kv_set(&st, "failed", "");
	kv_write(stp, &st, '='); sync();
	/* the new system brings its own desktop: a desktop installed earlier from a file no longer overrides it */
	if (!access("/persist/os/current/index.html", F_OK)) write_file("/persist/os/disabled", "system update\n");
	say("Keeping the updated system (slot %s).", bs);
	return_done(bs, &st, stp);
	recovery_check(bs);
	recovery_refresh(bs);   /* (1.5.9: the official recovery system follows the newest official release that started) */
	esp_check(bs);
	return 0;
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
/* ---------------- 1.6.0 (the owner, 2026-11-16): Roll back to the last stable version.
   The official stable channel (OFFICIAL_URL, whatever this computer's update address is) says which release is the
   latest stable one: its signed list (manifest.txt), and the stable releases published before it: history.txt, written
   by the release workflow's promotion and signed with the release key ("rudolf-stable-history 1", then one line per
   release: "S SERIAL VERSION RELEASED"). Both are checked with the official keys only (never a developer's) and kept in
   SYSD (stable.txt/.sig, stable-history.txt/.sig) by check. A newer history replaces the kept one only when it still
   names every release the kept one names, so an older list sent again cannot take a release off it. This system is a
   stable release when it is official and its serial is the list's or in the history; Settings shows the button when it
   is not (a beta, a test or a developer's build), or when Get beta versions is on. */
static int file_official(const char *tp, const char *sp, char **text, size_t *len)
{
	size_t ml, sl; char *t = slurp(tp, &ml), *g = slurp(sp, &sl);
	int ok = t && g && sl == 64 && official(verify((unsigned char *)t, ml, (unsigned char *)g)); free(g);
	if (!ok) { free(t); return 0; }
	if (text) *text = t; else free(t);
	if (len) *len = ml;
	return 1;
}
static int stable_load(man_t *m)
{
	char a[320], b[320]; snprintf(a, sizeof a, "%s/stable.txt", SYSD); snprintf(b, sizeof b, "%s/stable.sig", SYSD);
	char *t = NULL; if (!file_official(a, b, &t, NULL)) return -1;
	int r = parse_manifest(t, m); free(t); return r;
}
static int hist_ok(const char *t) { return t && !strncmp(t, "rudolf-stable-history 1\n", 24); }
static int hist_has(const char *t, long long serial)
{
	if (!hist_ok(t)) return 0;
	for (const char *l = t; l && *l; l = strchr(l, '\n') ? strchr(l, '\n') + 1 : NULL) { long long s; if (sscanf(l, "S %lld", &s) == 1 && s == serial) return 1; }
	return 0;
}
static char *hist_load(void)
{
	char a[320], b[320]; snprintf(a, sizeof a, "%s/stable-history.txt", SYSD); snprintf(b, sizeof b, "%s/stable-history.sig", SYSD);
	char *t = NULL; if (!file_official(a, b, &t, NULL) || !hist_ok(t)) { free(t); return NULL; }
	return t;
}
static int STABLE_FETCHED;
static void stable_fetch(void)
{
	if (STABLE_FETCHED++) return;
	const char *u = off_url(0); char mu[400], su[400], mp[320], sp[320];
	snprintf(mu, sizeof mu, "%s/manifest.txt", u); snprintf(su, sizeof su, "%s/manifest.sig", u);
	snprintf(mp, sizeof mp, "%s/stable.txt.dl", SYSD); snprintf(sp, sizeof sp, "%s/stable.sig.dl", SYSD);
	if (!http_to_file(mu, mp) && !http_to_file(su, sp)) {
		char *t = NULL; man_t m, old; int good = file_official(mp, sp, &t, NULL) && !parse_manifest(t, &m); free(t);
		if (good && !stable_load(&old) && m.serial < old.serial) good = 0;   /* (never an older stable list than one seen) */
		if (good) { char a[320], b[320]; snprintf(a, sizeof a, "%s/stable.txt", SYSD); snprintf(b, sizeof b, "%s/stable.sig", SYSD); rename(mp, a); rename(sp, b); }
	}
	unlink(mp); unlink(sp);
	snprintf(mu, sizeof mu, "%s/history.txt", u); snprintf(su, sizeof su, "%s/history.sig", u);
	snprintf(mp, sizeof mp, "%s/stable-history.txt.dl", SYSD); snprintf(sp, sizeof sp, "%s/stable-history.sig.dl", SYSD);
	if (!http_to_file(mu, mp) && !http_to_file(su, sp)) {
		char *t = NULL; int good = file_official(mp, sp, &t, NULL) && hist_ok(t);
		char *old = good ? hist_load() : NULL;
		/* every release the kept list names must still be there */
		for (const char *l = old; good && l && *l; l = strchr(l, '\n') ? strchr(l, '\n') + 1 : NULL) { long long s; if (sscanf(l, "S %lld", &s) == 1 && !hist_has(t, s)) good = 0; }
		if (good) { char a[320], b[320]; snprintf(a, sizeof a, "%s/stable-history.txt", SYSD); snprintf(b, sizeof b, "%s/stable-history.sig", SYSD); rename(mp, a); rename(sp, b); }
		free(t); free(old);
	}
	unlink(mp); unlink(sp);
}
/* the running system's signer (verify(): 0 none, 1..NOFF official, more: a developer's key) */
static int run_signer(void)
{
	char bs[8], mp[400], gp[400]; booted(bs);
	if (strcmp(bs, "disc")) { snprintf(mp, sizeof mp, "%s/%s/manifest.txt", SYSD, bs); snprintf(gp, sizeof gp, "%s/%s/manifest.sig", SYSD, bs); }
	else { snprintf(mp, sizeof mp, "%s/rudolf/manifest.txt", MEDIA); snprintf(gp, sizeof gp, "%s/rudolf/manifest.sig", MEDIA); }
	size_t l1, l2; char *t1 = slurp(mp, &l1), *t2 = slurp(gp, &l2); int v = t1 && t2 && l2 == 64 ? verify((unsigned char *)t1, l1, (unsigned char *)t2) : 0;
	free(t1); free(t2); return v;
}
/* 1 a published stable release, 0 not (a beta, a test or a developer's build), -1 not known (no stable list seen yet) */
static int running_stable(long long cur, const man_t *st, int have)
{
	if (!official(run_signer())) return 0;
	if (have && st->serial == cur) return 1;
	char *h = hist_load(); int in = h && hist_has(h, cur); free(h);
	return in ? 1 : have ? 0 : -1;
}
/* the other system slot, when it holds exactly release SERIAL, complete and signed by the official key (and it is not
   the one the start-up last gave up on) */
static const char *slot_with(long long serial, const char *bs)
{
	static const char *S[2] = {"a", "b"};
	char stp[300]; snprintf(stp, sizeof stp, "%s/state", SYSD); kv_t st; kv_read(stp, &st, '=');
	for (int i = 0; i < 2; i++) {
		if (!strcmp(S[i], bs) || !strcmp(kv(&st, "failed"), S[i])) continue;
		char c[400], mp[400], gp[400]; snprintf(c, sizeof c, "%s/%s/complete", SYSD, S[i]);
		snprintf(mp, sizeof mp, "%s/%s/manifest.txt", SYSD, S[i]); snprintf(gp, sizeof gp, "%s/%s/manifest.sig", SYSD, S[i]);
		char *t = NULL; man_t m; if (access(c, F_OK) || !file_official(mp, gp, &t, NULL)) { free(t); continue; }
		int ok = !parse_manifest(t, &m) && m.serial == serial; free(t);
		/* every file there, at its listed size (the start-up checks their hashes) */
		for (int k = 0; ok && k < NL; k++) { lay_t *y = mlayer(&m, LAYERS[k]); char f[420]; struct stat s2; if (!y) { ok = 0; break; }
			snprintf(f, sizeof f, "%s/%s/%s%s", SYSD, S[i], y->name, strcmp(y->name, "vmlinuz") ? ".sfs" : "");
			if (stat(f, &s2) || s2.st_size != y->size) { snprintf(f, sizeof f, "%s/rudolf/%s.sfs", MEDIA, y->name); if (!strcmp(y->name, "vmlinuz") || !media_permanent() || stat(f, &s2) || s2.st_size != y->size) ok = 0; } }
		if (ok) return S[i];
	}
	return NULL;
}
static void set_stable_channel(void)
{
	kv_t cf; kv_read(CONFP, &cf, '='); kv_set(&cf, "url", off_url(0)); kv_set(&cf, "tostable", ""); kv_write(CONFP, &cf, '=');
	char ap[320]; snprintf(ap, sizeof ap, "%s/available.txt", SYSD); unlink(ap); snprintf(ap, sizeof ap, "%s/available.sig", SYSD); unlink(ap);
}
static int NOTE_N;
static void note_json(const char *w) { printf("%s", NOTE_N++ ? "," : ""); jstr(w); }
static void note_say(const char *w) { say("Note: in that release %s.", w); }
static int cmd_stable_info(void)
{
	man_t st; int have = !stable_load(&st); kv_t r; cur_release(&r); long long cur = atoll(kv(&r, "serial"));
	kv_t cf; kv_read(CONFP, &cf, '='); char bs[8]; booted(bs); int v = run_signer(), rs = running_stable(cur, &st, have);
	printf("{\"ok\":true,\"running\":{\"serial\":%lld,\"version\":", cur); jstr(kv(&r, "version")); printf("}");
	printf(",\"signer\":\"%s\",\"developer\":%s,\"beta\":%s,\"canUpdate\":%s", official(v) ? "official" : v ? "custom" : "unknown", access(EXTRAF, F_OK) ? "false" : "true",
	       on_beta(&cf) ? "true" : "false", may_change_system(1) ? "true" : "false");
	printf(",\"runningStable\":%s,\"dataFmt\":%d,\"reads\":[%d,%d]", rs > 0 ? "true" : rs == 0 ? "false" : "null", data_fmt(), READS_MIN, READS_MAX);
	if (!have) { printf(",\"stable\":null,\"how\":\"unknown\"}\n"); return 0; }
	int f, lo, hi; rel_fmt(&st, &f, &lo, &hi); char why[900] = ""; int ok = compat(&st, why, sizeof why);
	printf(",\"stable\":{\"serial\":%lld,\"version\":", st.serial); jstr(st.version); printf(",\"released\":"); jstr(st.released);
	printf(",\"datafmt\":%d,\"reads\":[%d,%d],\"recorded\":%s}", f, lo, hi, st.datafmt > 0 ? "true" : "false");
	const char *sl = slot_with(st.serial, bs);
	const char *how = !official(v) ? "official" : cur == st.serial ? "running" : sl ? "slot" : "download";
	printf(",\"how\":\"%s\",\"slot\":", how); jstr(sl ? sl : ""); printf(",\"compatible\":%s,\"why\":", ok ? "true" : "false"); jstr(why);
	printf(",\"notes\":["); NOTE_N = 0; notes_for(&st, note_json); printf("]}\n");
	return 0;
}
static int cmd_tostable(void)
{
	if (!may_change_system(0)) return 6;
	if (!access(EXTRAF, F_OK) || !official(run_signer())) { say("This computer runs a custom build: use Return to official Rudolf OS (Settings > Security) to go back to the official system."); return 9; }
	if (lockf_()) die("another update is running");
	say("Checking the last stable release at %s", off_url(0));
	stable_fetch();
	man_t st; if (stable_load(&st)) { say("Could not get the list of stable releases: check the connection and try again. Nothing was changed."); return 3; }
	kv_t r; cur_release(&r); long long cur = atoll(kv(&r, "serial")); char bs[8]; booted(bs);
	say("The last stable version: Rudolf OS %s (released %s).", st.version, st.released);
	char why[900]; if (!compat(&st, why, sizeof why)) { say("%s", why); return 8; }
	notes_for(&st, note_say);
	if (cur == st.serial) { set_stable_channel(); say("This computer already runs Rudolf OS %s, the latest stable release. Beta versions are off.", st.version); return 0; }
	const char *sl = slot_with(st.serial, bs);
	if (sl) {
		char stp[300]; snprintf(stp, sizeof stp, "%s/state", SYSD); kv_t s2; kv_read(stp, &s2, '=');
		if (!*kv(&s2, "good")) kv_set(&s2, "good", bs);
		kv_set(&s2, "next", sl); kv_set(&s2, "tries", "2"); kv_set(&s2, "failed", ""); kv_write(stp, &s2, '='); set_stable_channel(); sync();
		say("Rudolf OS %s is still on this computer (system slot %s): nothing to download. Beta versions are off.", st.version, sl);
		say("Ready: Rudolf OS %s starts the next time the computer restarts.", st.version); return 0;
	}
	/* downloaded as an update (only the changes), from the official stable address: beta versions off ("tostable": the
	   stable release even though its number is lower), and the list it downloads checked again for this computer's data */
	{ kv_t cf; kv_read(CONFP, &cf, '='); kv_set(&cf, "url", off_url(0)); kv_set(&cf, "tostable", "1"); kv_write(CONFP, &cf, '=');
	  char ap[320]; snprintf(ap, sizeof ap, "%s/available.txt", SYSD); unlink(ap); snprintf(ap, sizeof ap, "%s/available.sig", SYSD); unlink(ap); }
	say("Beta versions are off. Downloading Rudolf OS %s into the spare system slot (only the changes).", st.version);
	COMPAT_GATE = 1; int rc = cmd_download(); COMPAT_GATE = 0;
	if (rc) say("The stable release could not be installed now; the next update brings it (beta versions stay off).");
	return rc;
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
	printf(",\"canUpdate\":%s,\"recovery\":%s", may_change_system(1) ? "true" : "false", recovery_start() ? "true" : "false");
	printf(",\"slots\":{\"a\":"); slot_json("a"); printf(",\"b\":"); slot_json("b"); printf(",\"r\":"); slot_json("r"); printf("}");
	/* 1.5.9 (plan 8.3): who signed the running system (official, or a developer's key in Developer mode), Developer
	   mode itself, and whether the recovery system is the official one */
	{ char mp2[400], gp2[400]; if (strcmp(bs, "disc")) { snprintf(mp2, sizeof mp2, "%s/%s/manifest.txt", SYSD, bs); snprintf(gp2, sizeof gp2, "%s/%s/manifest.sig", SYSD, bs); }
	  else { snprintf(mp2, sizeof mp2, "%s/rudolf/manifest.txt", MEDIA); snprintf(gp2, sizeof gp2, "%s/rudolf/manifest.sig", MEDIA); }
	  size_t l1, l2; char *t1 = slurp(mp2, &l1), *t2 = slurp(gp2, &l2); int v = t1 && t2 && l2 == 64 ? verify((unsigned char *)t1, l1, (unsigned char *)t2) : 0;
	  printf(",\"signer\":\"%s\",\"signerKey\":", official(v) ? "official" : v ? "custom" : "unknown");
	  if (v) { unsigned char b64[64]; EVP_EncodeBlock(b64, KEYS[v - 1], 32); jstr((char *)b64); } else printf("null");
	  free(t1); free(t2);
	  char rk[400]; snprintf(rk, sizeof rk, "%s/r/official", SYSD);
	  printf(",\"developer\":%s,\"recoveryOfficial\":%s", access(EXTRAF, F_OK) ? "false" : "true", access(rk, F_OK) ? "false" : "true"); }
	printf(",\"url\":"); jstr(*kv(&cf, "url") ? kv(&cf, "url") : d ? d : ""); printf(",\"auto\":%s", strcmp(kv(&cf, "auto"), "0") ? "true" : "false");
	printf(",\"beta\":%s,\"toStable\":%s", on_beta(&cf) ? "true" : "false", !strcmp(kv(&cf, "tostable"), "1") ? "true" : "false");
	/* 1.6.0: Roll back to the last stable version: whether this system is a published stable release, and which is the last */
	{ man_t sm; int have = !stable_load(&sm), rs = running_stable(atoll(kv(&r, "serial")), &sm, have);
	  printf(",\"runningStable\":%s,\"stableVersion\":", rs > 0 ? "true" : rs == 0 ? "false" : "null"); jstr(have ? sm.version : ""); }
	printf(",\"checked\":%lld,\"available\":", atoll(kv(&cf, "checked")));
	char ap[300], sp[300]; snprintf(ap, sizeof ap, "%s/available.txt", SYSD); snprintf(sp, sizeof sp, "%s/available.sig", SYSD);
	size_t ml, sl; char *mt = slurp(ap, &ml), *sg = slurp(sp, &sl); man_t m; int okm = 0;
	if (mt && sg && sl == 64 && verify((unsigned char *)mt, ml, (unsigned char *)sg)) { char *c = strdup(mt); okm = !parse_manifest(c, &m); free(c); }
	if (okm && wanted(m.serial, atoll(kv(&r, "serial")))) {
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
	if (getenv("RU_CMDLINE")) snprintf(CMDF, sizeof CMDF, "%s", getenv("RU_CMDLINE"));
	if (getenv("RU_MOUNTS")) snprintf(MOUNTSF, sizeof MOUNTSF, "%s", getenv("RU_MOUNTS"));
	if (getenv("RU_SYSBLOCK")) snprintf(SYSBLK, sizeof SYSBLK, "%s", getenv("RU_SYSBLOCK"));
	if (getenv("RU_EXTRA")) snprintf(EXTRAF, sizeof EXTRAF, "%s", getenv("RU_EXTRA"));
	if (argc < 2) { fputs("usage: rudolf-update status|check|download|boot-ok|rollback|set url URL|set auto 0|1|set channel beta|stable|rzs-make FILE OUT\n", stderr); return 2; }
	const char *c = argv[1];
	if (!strcmp(c, "rzs-make")) { if (argc < 4) die("usage: rzs-make FILE OUT.rzs"); return rzs_make(argv[2], argv[3]); }
	if (!strcmp(c, "delta") && argc >= 5) {
		/* delta URL OUT SIZE SEED... (testing: fetch one file using URL.rzs and the seed files) */
		char sd[4][300]; int n = 0; for (int i = 5; i < argc && n < 4; i++) snprintf(sd[n++], 300, "%s", argv[i]);
		char ru[600]; snprintf(ru, sizeof ru, "%s.rzs", argv[2]); long long got = 0;
		int r = delta_fetch(argv[2], ru, sd, n, argv[3], atoll(argv[4]), &got); say("result %d, downloaded %lld bytes", r, got); return r != 0;
	}
	if (!strcmp(c, "speedtest")) return cmd_speedtest();
	/* 1.5.7: fetch URL OUT, for Make a stick: a release's boot image (boot.iso), from an https address on github.com
	   (the one beside the kernel in the installed system's signed release list); what is in it is checked after */
	if (!strcmp(c, "fetch") && argc == 4) {
		if (strncmp(argv[2], "https://github.com/", 19) || strstr(argv[2], "..") || strchr(argv[2], '\'') || strlen(argv[2]) > 400) { say("only a Rudolf OS release on github.com"); return 2; }
		char t[600]; snprintf(t, sizeof t, "%s.part", argv[3]);
		if (http_to_file(argv[2], t)) { unlink(t); say("the download did not finish"); return 3; }
		if (rename(t, argv[3])) { unlink(t); return 3; } return 0;
	}
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
	if (!strcmp(c, "stable-info")) return cmd_stable_info();   /* 1.6.0 */
	if (!strcmp(c, "to-stable")) return cmd_tostable();
	if (!strcmp(c, "recovery")) { char bs[8]; booted(bs); if (recovery_start()) { say("Not while in recovery mode."); return 1; } return recovery_refresh(bs); }
	/* 1.5.9 (plan 8.3): Developer mode (the desktop asks an administrator's password and a typed confirmation first) */
	if (!strcmp(c, "developer") && argc >= 3) {
		kv_t cf; kv_read(CONFP, &cf, '=');
		if (!strcmp(argv[2], "off")) {
			unlink(EXTRAF); kv_set(&cf, "url", ""); kv_set(&cf, "devurl", "");
			if (kv_write(CONFP, &cf, '=')) return 1;
			say("Developer mode is off: only releases signed by Rudolf OS are installed, from the official address."); return 0;
		}
		if (strcmp(argv[2], "on") || argc != 5) die("developer on KEY URL | developer off");
		unsigned char k[32]; char kl[100]; snprintf(kl, sizeof kl, "%s", argv[3]);
		if (!key_line(kl, k)) die("the key is a publisher key: 44 characters of base64 (an Ed25519 public key)");
		for (int i = 0; i < NOFF; i++) if (!memcmp(KEYS[i], k, 32)) die("that is the official Rudolf OS key");
		const char *u = argv[4]; if (strncmp(u, "https://", 8)) die("the update address starts with https://");
		for (const char *q = u; *q; q++) if (!isalnum((unsigned char)*q) && !strchr("/:._-~%+", *q)) die("bad character in the address");
		{ char d[300]; snprintf(d, sizeof d, "%s", EXTRAF); char *sl = strrchr(d, '/'); if (sl) { *sl = 0; mkdirs(d); } }
		char line[200]; snprintf(line, sizeof line, "# Developer mode (Settings > Updates): a developer's publisher key\n%s\n", argv[3]);
		if (write_file(EXTRAF, line)) die("could not keep the key");
		kv_set(&cf, "url", u); kv_set(&cf, "devurl", u); if (kv_write(CONFP, &cf, '=')) return 1;
		say("Developer mode is on: releases signed by this key are installed too, from %s. The official recovery system stays official.", u);
		/* 1.6.0 (1.5.9 8.4): a computer that never ran an official release has none: it is downloaded now */
		if (!recovery_ok(0)) { say("There is no official recovery system on this computer yet: it is being downloaded from the official address.");
			if (!getenv("RU_NOAUTOFETCH")) { char self[300]; ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1); if (n > 0) { self[n] = 0; char *a[] = {self, "recovery-fetch", NULL}; detach(a, "/run/rudolf/recovery-fetch.log"); } } }
		return 0;
	}
	if (!strcmp(c, "recovery-fetch")) return cmd_recovery_fetch();   /* 1.6.0 */
	if (!strcmp(c, "recovery-check")) { char bs[8]; booted(bs); if (recovery_start()) return 1; recovery_check(bs); return recovery_ok(0) ? 0 : 1; }
	if (!strcmp(c, "fetch") && argc == 4) { if (strncmp(argv[2], "https://", 8) && !getenv("RU_OFFICIAL_URL")) die("https only"); return http_to_file(argv[2], argv[3]) ? 3 : 0; }   /* (espcheck's download) */
	if (!strcmp(c, "return-official") && argc == 3 && (!strcmp(argv[2], "keep") || !strcmp(argv[2], "erase"))) return cmd_return(!strcmp(argv[2], "erase"));
	if (!strcmp(c, "set") && argc >= 4) {
		kv_t cf; kv_read(CONFP, &cf, '=');
		if (!strcmp(argv[2], "url")) { const char *u = argv[3]; if (*u && strncmp(u, "https://", 8) && strncmp(u, "http://", 7)) die("the address starts with https://"); for (const char *q = u; *q; q++) if (!isalnum((unsigned char)*q) && !strchr("/:._-~%+", *q)) die("bad character in the address"); kv_set(&cf, "url", u); }
		else if (!strcmp(argv[2], "auto")) kv_set(&cf, "auto", strcmp(argv[3], "0") ? "1" : "0");
		else if (!strcmp(argv[2], "channel") && (!strcmp(argv[3], "beta") || !strcmp(argv[3], "stable"))) {
			if (!access(EXTRAF, F_OK)) die("Developer mode is on: its own address is used (turn Developer mode off first)");
			int beta = !strcmp(argv[3], "beta"), was = on_beta(&cf);
			kv_set(&cf, "url", off_url(beta)); kv_set(&cf, "tostable", !beta && was ? "1" : "");
			/* the list already seen is the other channel's: forget it (the next one is checked with the official key as always) */
			if (beta != was) { char ap[300]; snprintf(ap, sizeof ap, "%s/available.txt", SYSD); unlink(ap); snprintf(ap, sizeof ap, "%s/available.sig", SYSD); unlink(ap); }
			if (kv_write(CONFP, &cf, '=')) return 1;
			say(beta ? "Beta versions are on: updates now come from %s." : was ? "Beta versions are off: the next update brings the latest stable release (%s)." : "Updates come from %s.", off_url(beta));
			return 0;
		}
		else die("set url|auto|channel");
		return kv_write(CONFP, &cf, '=') ? 1 : 0;
	}
	die("unknown command %s", c); return 2;
}
