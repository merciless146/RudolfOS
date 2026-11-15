/* rpk - the Rudolf OS package manager. GPL-2.0-or-later.
 *
 * A package is a read-only squashfs image followed by a signature trailer:
 *     [squashfs][64-byte Ed25519 signature][8 bytes "RPKSIG01"]
 * The signature covers the text "rpk1 " + the SHA-256 (hex) of the squashfs part. Installed packages are
 * mounted read-only at /pkg/<name>, so installing never changes the system image and removing one deletes
 * a single file. Each image holds a PKGINFO file (key = value lines) and, by kind:
 *     cli     bin/<command>, lib/ (its own libraries)        rpk run <name> <command> [args]
 *     app     a desktop program started full screen          rpk run <name>
 *     webapp  www/ served by rudolfd at /pkg/<name>/         opened in a desktop window
 *     addon   files the desktop uses (for example a copy of Ruffle)
 * Repositories hold index.txt (tab-separated, one package per line) and index.sig (Ed25519 over index.txt).
 * Trusted public keys: /etc/rudolf/rpk-keys and <root>/keys (base64, one per line), and (1.5.2) the Rudolf OS
 * Library's keys in /etc/rudolf/library-keys. A package signed only by a Library key may hold files for the desktop
 * (kind addon or webapp), never programs, and may not replace a package signed by a system key. rudolf-update
 * never reads library-keys, so a Library key cannot sign a system update.
 * 1.5.9 (docs/plan-1.5.9.md 8.2): a package from an outside developer carries its developer's certificate:
 *     [squashfs][certificate text][64-byte certificate signature][4-byte length, little-endian][b"RPKCERT1"]
 *     [64-byte package signature][b"RPKSIG02"]
 * The certificate ("rpk-cert 1", name, github, key, prefix, expires) is signed by a Library key; the package by the
 * developer's key in it. Such a package is treated as the Library's (files for the desktop only), its name must
 * start with the certificate's prefix, and the certificate must not have expired. Revocation: the signed package
 * list's "R" lines (R key KEY, R pkg NAME VERSION|*): refused to install, and an installed one is disabled at the
 * next refresh (unmounted, with the reason in status).
 * Sources: the disc's own repository (/live/media/rudolf/pkg), the system's sources (/etc/rudolf/rpk-sources: the
 * Library) and lines in <root>/sources.
 * <root> is /persist/pkg on an installed system and /run/rudolf/pkg on a live one (lost at shutdown).
 * 1.6.0 (docs/plan-1.6.0.md Part 13): kind  sysext  is a signed system extension: files for the system (usr/, lib/firmware,
 * lib/modules/KVER/extra, at most one start script usr/lib/rudolf/ext.d/NAME.start) that /usr/lib/rudolf/sysext lays over
 * the unchanged base at the next start, read-only. Only a key in /etc/rudolf/rpk-keys itself (the release key) may sign
 * one: never a Library or developer key, nor a key added with "rpk key add". PKGINFO's  base = ABI  must be this base's
 * (/usr/share/rudolf/base-abi) and  kernel = KVER , when given, the running kernel's. An extension is never mounted at
 * /pkg/<name> and is not shown with the apps (status leaves it out: sysext-status lists them for Settings).
 *     rpk sysext-check NAME    the installed extension's signature (system key), base, kernel, revocation: "ok" or why
 *     rpk sysext-status        JSON: installed and available extensions, and what the last start did with them */
#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <openssl/evp.h>
/* (1.6.0) names are checked to 40 characters and paths fit their buffers: GCC's guesses about longer ones are noise */
#pragma GCC diagnostic ignored "-Wformat-truncation"

#define MAGIC "RPKSIG01"
#define TRAILER 72
#define DISC_REPO "/live/media/rudolf/pkg"
#define SYS_KEYS "/etc/rudolf/rpk-keys"
#define LIB_KEYS "/etc/rudolf/library-keys"
#define SYS_SOURCES "/etc/rudolf/rpk-sources"
#define MAXP 512

static char ROOT[256], MNT[256];


typedef struct { char *name, *version, *kind, *file, *sha, *title, *summary, *icon, *license; long long size; int src; } pkg_t;
typedef struct { char loc[512], type[8]; long long serial; int ok, count, lib; char err[160]; } src_t;
static pkg_t AV[MAXP]; static int NAV;
static src_t SRC[16]; static int NSRC;
static unsigned char KEYS[16][32]; static int NKEYS, KLIB[16], KSYS[16];   /* KSYS: (1.6.0) the key is in /etc/rudolf/rpk-keys itself */
static int VSYSFILE;
static int SXOK;   /* (1.6.0) "--system" given (rudolfd, after an administrator's password): a system extension may be installed or removed */   /* (1.6.0) a key from /etc/rudolf/rpk-keys itself made the last signature verify() accepted */
/* 1.5.9: the revocations the signed package lists carry, and the certificate verify_rpk last accepted */
typedef struct { char what[4]; char a[64]; char b[64]; } rev_t;
static rev_t REV[256]; static int NREV;
static struct { char name[64], github[64], key[48], prefix[48], expires[16]; } CERT;

static void die(const char *f, ...) { va_list a; va_start(a, f); fputs("rpk: ", stderr); vfprintf(stderr, f, a); fputc('\n', stderr); va_end(a); exit(1); }
static void say(const char *f, ...) { va_list a; va_start(a, f); vprintf(f, a); putchar('\n'); fflush(stdout); va_end(a); }
/* 1.5.4: a health-log line (Settings > Security > Health log) for each Library install, update, removal and refusal:
   rudolfd --health library MESSAGE; RPK_HEALTH replaces the program (tests), "" turns it off */
static int spawn(char *const argv[], int quiet);
static const char *HKIND = "library";
static void lib_health(const char *f, ...)
{
	const char *prog = getenv("RPK_HEALTH") ? getenv("RPK_HEALTH") : "/usr/bin/rudolfd";
	if (!*prog || access(prog, X_OK)) return;
	char m[300]; va_list a; va_start(a, f); vsnprintf(m, sizeof m, f, a); va_end(a);
	char *argv[] = {(char *)prog, "--health", (char *)HKIND, m, NULL}; spawn(argv, 1);
}
/* (1.6.0) a health-log line about a system extension (kind "extension") */
#define sx_health(...) do { HKIND = "extension"; lib_health(__VA_ARGS__); HKIND = "library"; } while (0)
static int valid_name(const char *n)
{
	if (!n || !*n || strlen(n) > 40 || !(islower((unsigned char)*n) || isdigit((unsigned char)*n))) return 0;
	for (const char *p = n; *p; p++) if (!(islower((unsigned char)*p) || isdigit((unsigned char)*p) || *p == '-' || *p == '_')) return 0;
	return 1;
}
static char *slurp(const char *path, size_t *len)
{
	int f = open(path, O_RDONLY | O_CLOEXEC); if (f < 0) return NULL;
	struct stat st; if (fstat(f, &st) || st.st_size > 64 << 20) { close(f); return NULL; }
	/* files in /proc report size 0, so read until the end */
	size_t cap = st.st_size ? st.st_size + 1 : 65536, n = 0; char *b = malloc(cap); ssize_t r;
	while ((r = read(f, b + n, cap - n - 1)) > 0) { n += r; if (n + 1 >= cap) { if (cap > (64u << 20)) break; cap *= 2; b = realloc(b, cap); } }
	close(f); b[n] = 0; if (len) *len = n; return b;
}
static void jstr(const char *s)
{
	putchar('"');
	for (; s && *s; s++) {
		unsigned char c = *s;
		if (c == '"' || c == '\\') printf("\\%c", c); else if (c == '\n') fputs("\\n", stdout); else if (c == '\t') fputs("\\t", stdout);
		else if (c < 0x20) printf("\\u%04x", c); else putchar(c);
	}
	putchar('"');
}
/* run a program without a shell; returns its exit status */
static int spawn(char *const argv[], int quiet)
{
	pid_t p = fork(); if (p < 0) return -1;
	if (!p) { if (quiet) { int n = open("/dev/null", O_WRONLY); dup2(n, 1); dup2(n, 2); } execvp(argv[0], argv); _exit(127); }
	int st; while (waitpid(p, &st, 0) < 0 && errno == EINTR); return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}
static void mkdirs(const char *d) { char t[512]; snprintf(t, sizeof t, "%s", d); for (char *p = t + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(t, 0755); *p = '/'; } mkdir(t, 0755); }

/* ---------------- crypto */
static int sha256_file(const char *path, long long limit, char hex[65])
{
	int f = open(path, O_RDONLY | O_CLOEXEC); if (f < 0) return -1;
	EVP_MD_CTX *c = EVP_MD_CTX_new(); EVP_DigestInit_ex(c, EVP_sha256(), NULL);
	unsigned char buf[65536]; long long done = 0; ssize_t n;
	while ((limit < 0 || done < limit) && (n = read(f, buf, limit < 0 || limit - done > (long long)sizeof buf ? (long long)sizeof buf : limit - done)) > 0) { EVP_DigestUpdate(c, buf, n); done += n; }
	close(f);
	unsigned char md[32]; unsigned int ml; EVP_DigestFinal_ex(c, md, &ml); EVP_MD_CTX_free(c);
	if (limit >= 0 && done != limit) return -1;
	for (int i = 0; i < 32; i++) sprintf(hex + 2 * i, "%02x", md[i]);
	return 0;
}
static void load_keys_from(const char *path, int lib, int sysfile)
{
	char *t = slurp(path, NULL); if (!t) return;
	for (char *l = strtok(t, "\n"); l && NKEYS < 16; l = strtok(NULL, "\n")) {
		while (isspace((unsigned char)*l)) l++;
		if (!*l || *l == '#') continue;
		char *e = l; while (*e && !isspace((unsigned char)*e)) e++; *e = 0;
		unsigned char raw[64]; int n = EVP_DecodeBlock(raw, (unsigned char *)l, strlen(l));
		if (strlen(l) == 44 && n >= 32) { KLIB[NKEYS] = lib; KSYS[NKEYS] = sysfile; memcpy(KEYS[NKEYS++], raw, 32); }
	}
	free(t);
}
static void load_keys(void)
{
	char p[300]; NKEYS = 0;
	load_keys_from(getenv("RPK_KEYS") ? getenv("RPK_KEYS") : SYS_KEYS, 0, 1);
	load_keys_from(getenv("RPK_LIBRARY_KEYS") ? getenv("RPK_LIBRARY_KEYS") : LIB_KEYS, 1, 0);
	snprintf(p, sizeof p, "%s/keys", ROOT); load_keys_from(p, 0, 0);
}
/* 0: no trusted key signed it; 1: a system key did; 2: only a Library key did */
static int verify(const unsigned char *msg, size_t len, const unsigned char *sig)
{
	int lib = 0, sys = 0; VSYSFILE = 0;
	for (int i = 0; i < NKEYS; i++) {
		EVP_PKEY *k = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, KEYS[i], 32); if (!k) continue;
		EVP_MD_CTX *c = EVP_MD_CTX_new(); int ok = EVP_DigestVerifyInit(c, NULL, NULL, NULL, k) == 1 && EVP_DigestVerify(c, sig, 64, msg, len) == 1;
		EVP_MD_CTX_free(c); EVP_PKEY_free(k);
		if (ok && KSYS[i]) { VSYSFILE = 1; return 1; }
		if (ok && !KLIB[i]) sys = 1;
		else if (ok) lib = 1;
	}
	return sys ? 1 : lib ? 2 : 0;
}
static int VCLASS;   /* what verify_rpk found: 1 a system key, 2 only a Library key, 3 a developer's certificate */
static int key_b64(const char *b, unsigned char out[32]) { unsigned char raw[64]; return strlen(b) == 44 && EVP_DecodeBlock(raw, (const unsigned char *)b, 44) >= 32 ? (memcpy(out, raw, 32), 0) : -1; }
static int verify_with(const unsigned char k32[32], const unsigned char *msg, size_t len, const unsigned char *sig)
{
	EVP_PKEY *k = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, k32, 32); if (!k) return 0;
	EVP_MD_CTX *c = EVP_MD_CTX_new(); int ok = EVP_DigestVerifyInit(c, NULL, NULL, NULL, k) == 1 && EVP_DigestVerify(c, sig, 64, msg, len) == 1;
	EVP_MD_CTX_free(c); EVP_PKEY_free(k); return ok;
}
static int revoked(const char *what, const char *a, const char *b)
{
	for (int i = 0; i < NREV; i++) if (!strcmp(REV[i].what, what) && !strcmp(REV[i].a, a) && (!b || !strcmp(REV[i].b, "*") || !strcmp(REV[i].b, b))) return 1;
	return 0;
}
static void today(char *d) { time_t now = time(NULL); strftime(d, 16, "%Y-%m-%d", gmtime(&now)); }
/* a developer's certificate (text + signature): signed by a trusted key; fills CERT. 0 or a reason */
static const char *check_cert(const char *t, size_t n, const unsigned char *sig)
{
	if (!verify((const unsigned char *)t, n, sig)) return "its developer certificate is not signed by the Rudolf OS Library";
	if (strncmp(t, "rpk-cert 1\n", 11)) return "its developer certificate is not one";
	memset(&CERT, 0, sizeof CERT); char *c = strndup(t, n);
	for (char *l = strtok(c, "\n"); l; l = strtok(NULL, "\n")) {
		char *v = strchr(l, ' '); if (!v) continue; *v++ = 0;
		if (!strcmp(l, "name")) snprintf(CERT.name, sizeof CERT.name, "%s", v); else if (!strcmp(l, "github")) snprintf(CERT.github, sizeof CERT.github, "%s", v);
		else if (!strcmp(l, "key")) snprintf(CERT.key, sizeof CERT.key, "%s", v); else if (!strcmp(l, "prefix")) snprintf(CERT.prefix, sizeof CERT.prefix, "%s", v);
		else if (!strcmp(l, "expires")) snprintf(CERT.expires, sizeof CERT.expires, "%s", v);
	}
	free(c);
	char d[16]; today(d);
	if (!*CERT.key || !*CERT.prefix || strlen(CERT.prefix) < 3 || !*CERT.expires) return "its developer certificate is incomplete";
	if (strcmp(CERT.expires, d) < 0) return "its developer certificate has expired";
	if (revoked("key", CERT.key, NULL)) return "its developer's key was revoked by the Rudolf OS Library";
	return NULL;
}
/* checks the trailer; returns the squashfs length, or -1 with a reason */
static long long verify_rpk(const char *path, const char **why)
{
	struct stat st; if (stat(path, &st) || st.st_size < 4096 + TRAILER) { *why = "not a package file"; return -1; }
	int f = open(path, O_RDONLY | O_CLOEXEC); if (f < 0) { *why = "cannot read it"; return -1; }
	unsigned char t[TRAILER]; int ok = pread(f, t, TRAILER, st.st_size - TRAILER) == TRAILER;
	if (!ok || (memcmp(t + 64, MAGIC, 8) && memcmp(t + 64, "RPKSIG02", 8))) { close(f); *why = "not a signed Rudolf package"; return -1; }
	long long body = st.st_size - TRAILER;
	int dev = !memcmp(t + 64, "RPKSIG02", 8); unsigned char ck[32];
	if (dev) {
		unsigned char h[12]; if (body < 12 || pread(f, h, 12, body - 12) != 12 || memcmp(h + 4, "RPKCERT1", 8)) { close(f); *why = "its developer certificate is missing"; return -1; }
		long long cl = h[0] | h[1] << 8 | h[2] << 16 | (long long)h[3] << 24;
		if (cl < 20 || cl > 4000 || body < 12 + 64 + cl + 4096) { close(f); *why = "its developer certificate is damaged"; return -1; }
		char *ct = malloc(cl + 1); unsigned char cs[64];
		ok = pread(f, ct, cl, body - 12 - 64 - cl) == cl && pread(f, cs, 64, body - 12 - 64) == 64; ct[ok ? cl : 0] = 0;
		const char *w = ok ? check_cert(ct, cl, cs) : "its developer certificate cannot be read"; free(ct);
		if (w) { close(f); *why = w; return -1; }
		if (key_b64(CERT.key, ck)) { close(f); *why = "its developer certificate is damaged"; return -1; }
		body -= 12 + 64 + cl;
	}
	close(f);
	char hex[65], msg[80];
	if (sha256_file(path, body, hex)) { *why = "cannot read it"; return -1; }
	snprintf(msg, sizeof msg, "rpk1 %s", hex);
	if (dev) { if (!verify_with(ck, (unsigned char *)msg, strlen(msg), t)) { *why = "the signature is not from the developer its certificate names"; return -1; } VCLASS = 3; return body; }
	if (!(VCLASS = verify((unsigned char *)msg, strlen(msg), t))) { *why = "the signature is not from a trusted key"; return -1; }
	return body;
}

/* ---------------- package info (PKGINFO: key = value) */
typedef struct { char k[24][24]; char v[24][400]; int n; } info_t;
static int read_info(const char *path, info_t *in)
{
	in->n = 0; char *t = slurp(path, NULL); if (!t) return -1;
	for (char *l = strtok(t, "\n"); l && in->n < 24; l = strtok(NULL, "\n")) {
		char *eq = strchr(l, '='); if (!eq || *l == '#') continue;
		char *k = l, *v = eq + 1; *eq = 0;
		while (*k && isspace((unsigned char)*k)) k++; for (char *e = eq - 1; e >= k && isspace((unsigned char)*e); e--) *e = 0;
		while (*v && isspace((unsigned char)*v)) v++; for (char *e = v + strlen(v) - 1; e >= v && isspace((unsigned char)*e); e--) *e = 0;
		snprintf(in->k[in->n], 24, "%s", k); snprintf(in->v[in->n], 400, "%s", v); in->n++;
	}
	free(t); return 0;
}
static const char *iget(info_t *in, const char *k) { for (int i = 0; i < in->n; i++) if (!strcmp(in->k[i], k)) return in->v[i]; return ""; }

/* ---------------- sources and indexes */
static void add_src(const char *loc)
{
	if (NSRC >= 16) return;
	src_t *s = &SRC[NSRC++]; memset(s, 0, sizeof *s); snprintf(s->loc, sizeof s->loc, "%s", loc);
	snprintf(s->type, sizeof s->type, "%s", !strncmp(loc, "http://", 7) || !strncmp(loc, "https://", 8) ? "web" : "folder");
}
static void load_sources(void)
{
	NSRC = 0; const char *d = getenv("RPK_DISC") ? getenv("RPK_DISC") : DISC_REPO;
	struct stat st; if (!stat(d, &st) && S_ISDIR(st.st_mode)) { add_src(d); strcpy(SRC[0].type, "disc"); }
	char p[300]; snprintf(p, sizeof p, "%s/sources", ROOT);
	/* (1.6.0) and the system extensions' list for this base (written into the base by ci_release.sh) */
	const char *files[3] = { getenv("RPK_SOURCES") ? getenv("RPK_SOURCES") : SYS_SOURCES, p, getenv("RPK_SYSEXT_SOURCE") ? getenv("RPK_SYSEXT_SOURCE") : "/usr/share/rudolf/sysext-source" };
	for (int f = 0; f < 3; f++) {
		char *t = slurp(files[f], NULL); if (!t) continue;
		for (char *l = strtok(t, "\n"); l; l = strtok(NULL, "\n")) {
			if (*l != '/' && strncmp(l, "http", 4)) continue;
			int dup = 0; for (int i = 0; i < NSRC; i++) if (!strcmp(SRC[i].loc, l)) dup = 1;
			if (!dup) add_src(l);
		}
		free(t);
	}
}
static int fetch(src_t *s, const char *file, const char *dest)
{
	char from[800]; snprintf(from, sizeof from, "%s/%s", s->loc, file);
	unlink(dest);
	if (!strcmp(s->type, "web")) { char *a[] = {"wget", "-q", "-T", "30", "-O", (char *)dest, from, NULL}; return spawn(a, 1) || access(dest, R_OK) ? -1 : 0; }
	char *a[] = {"cp", from, (char *)dest, NULL}; return spawn(a, 1);
}
static char *field(char **p) { char *s = *p; if (!s) return ""; char *t = strchr(s, '\t'); if (t) { *t = 0; *p = t + 1; } else *p = NULL; return s; }
/* parses a verified index into AV; returns the package count or -1 */
static int parse_index(char *t, int src, long long *serial, char *expires)
{
	int n = 0; *serial = 0; expires[0] = 0;
	if (strncmp(t, "rpk-index 1\n", 12)) return -1;
	for (char *l = strtok(t, "\n"); l; l = strtok(NULL, "\n")) {
		if (!strncmp(l, "serial ", 7)) *serial = atoll(l + 7);
		else if (!strncmp(l, "expires ", 8)) snprintf(expires, 16, "%s", l + 8);
		else if (!strncmp(l, "R\t", 2) && NREV < 256) {   /* (1.5.9) a revocation */
			char *p = l + 2; rev_t *r = &REV[NREV]; snprintf(r->what, sizeof r->what, "%s", field(&p)); snprintf(r->a, sizeof r->a, "%s", field(&p)); snprintf(r->b, sizeof r->b, "%s", field(&p));
			if ((!strcmp(r->what, "key") || !strcmp(r->what, "pkg")) && *r->a) NREV++;
		}
		else if (!strncmp(l, "P\t", 2) && NAV < MAXP) {
			char *p = l + 2; pkg_t *k = &AV[NAV];
			k->name = strdup(field(&p)); k->version = strdup(field(&p)); k->kind = strdup(field(&p)); k->size = atoll(field(&p));
			k->sha = strdup(field(&p)); k->file = strdup(field(&p)); k->title = strdup(field(&p)); k->summary = strdup(field(&p));
			k->icon = strdup(field(&p)); k->license = strdup(field(&p)); k->src = src;
			if (valid_name(k->name) && strlen(k->sha) == 64 && !strchr(k->file, '/') && k->file[0] != '.') { NAV++; n++; }
		}
	}
	return n;
}
static int load_index(int i, const char *idx, const char *sig, int check_expiry)
{
	src_t *s = &SRC[i]; size_t il, sl; char *t = slurp(idx, &il), *g = slurp(sig, &sl);
	/* the disc's own read-only repository is trusted like the system on the same disc: its list is not
	   signature-checked, but every package from it must still match the SHA-256 in that list */
	int disc = !strcmp(s->type, "disc");
	if (!t || (!g && !disc)) { snprintf(s->err, sizeof s->err, "no package list yet (run rpk refresh)"); free(t); free(g); return -1; }
	int v = disc ? 1 : sl == 64 ? verify((unsigned char *)t, il, (unsigned char *)g) : 0;
	if (!v) { snprintf(s->err, sizeof s->err, "the package list is not signed by a trusted key"); free(t); free(g); return -1; }
	s->lib = v == 2;
	char exp[16]; int n = parse_index(t, i, &s->serial, exp);
	free(g);
	if (n < 0) { snprintf(s->err, sizeof s->err, "the package list is damaged"); return -1; }
	if (check_expiry && exp[0]) { char today[16]; time_t now = time(NULL); strftime(today, sizeof today, "%Y-%m-%d", gmtime(&now)); if (strcmp(exp, today) < 0) { snprintf(s->err, sizeof s->err, "the package list expired on %s", exp); NAV -= n; return -1; } }
	s->ok = 1; s->count = n; return n;
}
static void idx_paths(int i, char *idx, char *sig, size_t n)
{
	/* cache file named after the source, so reordering sources keeps each one's cache */
	unsigned long h = 5381; for (const char *p = SRC[i].loc; *p; p++) h = h * 33 + (unsigned char)*p;
	snprintf(idx, n, "%s/index/%lx.txt", ROOT, h); snprintf(sig, n, "%s/index/%lx.sig", ROOT, h);
}
static void load_available(void)
{
	NAV = 0; NREV = 0; char idx[400], sig[400];
	for (int i = 0; i < NSRC; i++) {
		if (!strcmp(SRC[i].type, "web")) { idx_paths(i, idx, sig, sizeof idx); load_index(i, idx, sig, 1); }
		else { snprintf(idx, sizeof idx, "%s/index.txt", SRC[i].loc); snprintf(sig, sizeof sig, "%s/index.sig", SRC[i].loc); load_index(i, idx, sig, 0); }
	}
}
static pkg_t *best(const char *name)
{
	pkg_t *b = NULL;
	for (int i = 0; i < NAV; i++) if (!strcmp(AV[i].name, name) && (!b || strverscmp(AV[i].version, b->version) > 0)) b = &AV[i];
	return b;
}
static int list_installed(char names[][48], int max);
static void umount_pkg(const char *name);
static int installed_info(const char *name, info_t *in);
/* 1.5.9: installed packages the Library revoked (the package, or its developer's key) are disabled: unmounted, kept
   with the reason (status shows it; removing them is the person's choice) */
static void revoke_installed(void)
{
	NAV = 0; NREV = 0; load_available();
	char n[MAXP][48]; int k = list_installed(n, MAXP);
	for (int i = 0; i < k; i++) {
		info_t in; if (installed_info(n[i], &in)) continue;
		const char *why = revoked("pkg", n[i], iget(&in, "version")) ? "the Rudolf OS Library revoked this package"
			: !strcmp(iget(&in, "signed"), "developer") && revoked("key", iget(&in, "certkey"), NULL) ? "the Rudolf OS Library revoked its developer's key" : NULL;
		char dis[400]; snprintf(dis, sizeof dis, "%s/db/%s.disabled", ROOT, n[i]);
		if (!why || !access(dis, F_OK)) continue;
		umount_pkg(n[i]); FILE *o = fopen(dis, "w"); if (o) { fprintf(o, "%s\n", why); fclose(o); }
		say("Disabled %s: %s", n[i], why); lib_health("disabled %s %s: %s", n[i], iget(&in, "version"), why);
	}
}
static void refresh(void)
{
	char idx[400], sig[400], ti[420], ts[420];
	for (int i = 0; i < NSRC; i++) {
		if (strcmp(SRC[i].type, "web")) { say("%s: %s", SRC[i].type, SRC[i].loc); continue; }
		say("Checking %s", SRC[i].loc);
		idx_paths(i, idx, sig, sizeof idx); snprintf(ti, sizeof ti, "%s.new", idx); snprintf(ts, sizeof ts, "%s.new", sig);
		if (fetch(&SRC[i], "index.txt", ti) || fetch(&SRC[i], "index.sig", ts)) { say("  could not download the package list"); continue; }
		/* load the old serial, then check the new list before replacing it */
		long long old = 0; int save = NAV;
		if (load_index(i, idx, sig, 0) >= 0) old = SRC[i].serial;
		NAV = save; SRC[i].ok = 0;
		if (load_index(i, ti, ts, 1) < 0) { say("  refused: %s", SRC[i].err); unlink(ti); unlink(ts); NAV = save; continue; }
		NAV = save;
		if (SRC[i].serial < old) { say("  refused: this list is older than the one already here"); unlink(ti); unlink(ts); continue; }
		rename(ti, idx); rename(ts, sig); say("  %d packages", SRC[i].count);
	}
	revoke_installed();
}

/* ---------------- installed packages */
static int is_mounted(const char *dir)
{
	char *m = slurp("/proc/self/mounts", NULL); if (!m) return 0;
	char pat[300]; snprintf(pat, sizeof pat, " %s ", dir); int r = strstr(m, pat) != NULL; free(m); return r;
}
static int installed_info(const char *name, info_t *in);
static int mount_pkg(const char *name)
{
	{ info_t in; if (!installed_info(name, &in) && !strcmp(iget(&in, "kind"), "sysext")) return 0; }   /* (1.6.0: laid over the system at start, not here) */
	char file[400], dir[300]; snprintf(file, sizeof file, "%s/store/%s.rpk", ROOT, name); snprintf(dir, sizeof dir, "%s/%s", MNT, name);
	if (is_mounted(dir)) return 0;
	{ char dis[400]; snprintf(dis, sizeof dis, "%s/db/%s.disabled", ROOT, name); if (!access(dis, F_OK)) return 0; }   /* (1.5.9: revoked) */
	mkdirs(dir);
	char *a[] = {"mount", "-t", "squashfs", "-o", "loop,ro", file, dir, NULL}; return spawn(a, 1);
}
static void umount_pkg(const char *name)
{
	char dir[300]; snprintf(dir, sizeof dir, "%s/%s", MNT, name);
	if (is_mounted(dir)) { char *a[] = {"umount", "-d", dir, NULL}; if (spawn(a, 1)) { char *b[] = {"umount", "-l", dir, NULL}; spawn(b, 1); } }
	rmdir(dir);
}
static int installed_info(const char *name, info_t *in) { char p[400]; snprintf(p, sizeof p, "%s/db/%s.info", ROOT, name); return read_info(p, in); }
static int list_installed(char names[][48], int max)
{
	char d[300]; snprintf(d, sizeof d, "%s/db", ROOT); DIR *dd = opendir(d); int n = 0; if (!dd) return 0;
	struct dirent *e;
	while ((e = readdir(dd)) && n < max) { size_t l = strlen(e->d_name); if (l > 5 && !strcmp(e->d_name + l - 5, ".info") && l - 5 < 48) { memcpy(names[n], e->d_name, l - 5); names[n][l - 5] = 0; if (valid_name(names[n])) n++; } }
	closedir(dd);
	for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) if (strcmp(names[i], names[j]) > 0) { char t[48]; memcpy(t, names[i], 48); memcpy(names[i], names[j], 48); memcpy(names[j], t, 48); }
	return n;
}
/* ---------------- (1.6.0, plan Part 13) system extensions */
#include <sys/utsname.h>
#define BASE_ABI "/usr/share/rudolf/base-abi"
/* this base's ABI (a hash of its libraries, written by ci_release.sh), or "" */
static void base_abi(char out[80])
{
	out[0] = 0; char *t = slurp(getenv("RPK_BASE_ABI") ? getenv("RPK_BASE_ABI") : BASE_ABI, NULL); if (!t) return;
	size_t n = strcspn(t, " \t\r\n"); if (n < 80) { memcpy(out, t, n); out[n] = 0; } free(t);
}
static void kernel_rel(char out[80])
{
	if (getenv("RPK_KERNEL")) { snprintf(out, 80, "%s", getenv("RPK_KERNEL")); return; }
	struct utsname u; if (uname(&u)) out[0] = 0; else snprintf(out, 80, "%s", u.release);
}
/* why an extension's PKGINFO does not fit this system (NULL: it fits) */
static const char *sysext_fits(info_t *in)
{
	static char why[200]; char abi[80], kr[80]; base_abi(abi); kernel_rel(kr);
	const char *b = iget(in, "base"), *k = iget(in, "kernel");
	if (!*abi) return "this Rudolf OS has no base ABI (/usr/share/rudolf/base-abi): it takes no system extensions";
	if (!*b) return "it does not say which Rudolf OS base it was built for";
	if (strcmp(b, abi)) { snprintf(why, sizeof why, "it was built for another Rudolf OS base (%.16s, this one is %.16s)", b, abi); return why; }
	if (*k && strcmp(k, kr)) { snprintf(why, sizeof why, "it was built for Linux %.40s, this computer runs %.40s", k, kr); return why; }
	return NULL;
}
/* what an extension may hold: PKGINFO, licenses/, and under usr/, lib/firmware/ and lib/modules/KVER/extra/ only (KVER
   its kernel line); at most one start script, usr/lib/rudolf/ext.d/NAME.start; no device files. NULL: it may */
static const char *sysext_walk(const char *top, const char *rel, const char *name, const char *kver, int *starts)
{
	char d[1200]; snprintf(d, sizeof d, "%s%s%s", top, *rel ? "/" : "", rel);
	DIR *dd = opendir(d); if (!dd) return "its files cannot be read";
	struct dirent *e; const char *bad = NULL;
	while (!bad && (e = readdir(dd))) {
		if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
		char r[1200]; snprintf(r, sizeof r, "%s%s%s", rel, *rel ? "/" : "", e->d_name);
		char f[1300]; snprintf(f, sizeof f, "%s/%s", top, r); struct stat st; if (lstat(f, &st)) { bad = "its files cannot be read"; break; }
		int dir = S_ISDIR(st.st_mode);
		if (!*rel) { if (!strcmp(r, "PKGINFO") && S_ISREG(st.st_mode)) continue; if (dir && (!strcmp(r, "licenses") || !strcmp(r, "usr") || !strcmp(r, "lib"))) { if (strcmp(r, "licenses")) bad = sysext_walk(top, r, name, kver, starts); continue; }
			bad = "it holds files outside usr/, lib/firmware and lib/modules"; break; }
		if (!strcmp(rel, "lib")) { if (dir && (!strcmp(e->d_name, "firmware") || !strcmp(e->d_name, "modules"))) { bad = sysext_walk(top, r, name, kver, starts); continue; } bad = "it holds files in lib/ other than firmware and modules"; break; }
		if (!strcmp(rel, "lib/modules")) { if (dir && *kver && !strcmp(e->d_name, kver)) { bad = sysext_walk(top, r, name, kver, starts); continue; } bad = "its kernel modules are not for the kernel its PKGINFO names"; break; }
		if (!strncmp(rel, "lib/modules/", 12) && !strchr(rel + 12, '/')) { if (dir && !strcmp(e->d_name, "extra")) { bad = sysext_walk(top, r, name, kver, starts); continue; } bad = "its kernel modules must be in lib/modules/KVER/extra"; break; }
		if (!S_ISREG(st.st_mode) && !dir && !S_ISLNK(st.st_mode)) { bad = "it holds a device, pipe or socket"; break; }
		if (!strcmp(rel, "usr/lib/rudolf/ext.d")) { char want[100]; snprintf(want, sizeof want, "%s.start", name);
			if (strcmp(e->d_name, want) || !S_ISREG(st.st_mode)) { bad = "its only start script may be usr/lib/rudolf/ext.d/NAME.start"; break; } (*starts)++; continue; }
		if (dir) bad = sysext_walk(top, r, name, kver, starts);
	}
	closedir(dd); return bad;
}
/* verifies a downloaded or local package file and installs it in place of any older copy */
static int MISMATCH;   /* (1.6.0) the last install_file was refused because the download did not match the list */
static int install_file(const char *path, const char *want_name, const char *want_sha, int from_disc)
{
	const char *why = NULL; VCLASS = 0; MISMATCH = 0;
	if (want_sha) { char hex[65]; if (sha256_file(path, -1, hex) || strcmp(hex, want_sha)) { MISMATCH = 1; say("  refused: the download does not match the package list"); return -1; } }
	if (from_disc && want_sha) { if (verify_rpk(path, &why) < 0 && strcmp(why, "the signature is not from a trusted key")) { say("  refused: %s", why); return -1; } }
	else if (verify_rpk(path, &why) < 0) { say("  refused: %s", why); return -1; }
	int cls = VCLASS;   /* 0 only for an unsigned package from the disc's own list, which the system trusts */
	char tmpd[300]; snprintf(tmpd, sizeof tmpd, "%s/tmp/look", ROOT); mkdirs(tmpd);
	char *a[] = {"mount", "-t", "squashfs", "-o", "loop,ro", (char *)path, tmpd, NULL};
	if (spawn(a, 1)) { say("  refused: the package image cannot be opened"); return -1; }
	info_t in; char ip[400]; snprintf(ip, sizeof ip, "%s/PKGINFO", tmpd);
	int bad = read_info(ip, &in) || !valid_name(iget(&in, "name")) || !*iget(&in, "version") || (want_name && strcmp(iget(&in, "name"), want_name));
	const char *kd = iget(&in, "kind"); if (strcmp(kd, "cli") && strcmp(kd, "app") && strcmp(kd, "webapp") && strcmp(kd, "addon") && strcmp(kd, "sysext")) bad = 1;
	int sx = !strcmp(kd, "sysext");
	if (*iget(&in, "signed")) bad = 1;   /* rpk writes that line itself */
	char name[48]; snprintf(name, sizeof name, "%s", iget(&in, "name"));
	char dbf[400], dbt[420]; snprintf(dbf, sizeof dbf, "%s/db/%s.info", ROOT, name); snprintf(dbt, sizeof dbt, "%s.new", dbf);
	/* a package only a Library key signed: files for the desktop, never programs, and never in place of a package a
	   system key signed (so a stolen Library key could not change what the system's own packages do) */
	int lib = cls == 2 || cls == 3;
	const char *refuse = NULL;
	if (!bad && revoked("pkg", iget(&in, "name"), iget(&in, "version"))) refuse = "this package was revoked by the Rudolf OS Library";
	/* (1.5.9) an outside developer's package: only names under the prefix its certificate gives */
	if (!bad && !refuse && cls == 3 && strncmp(iget(&in, "name"), CERT.prefix, strlen(CERT.prefix))) refuse = "its name is not one its developer certificate allows";
	if (!bad && lib && !sx && strcmp(kd, "addon") && strcmp(kd, "webapp")) refuse = "a Rudolf OS Library package can only hold files for the desktop";
	/* (1.6.0) a system extension: signed by the release key (a key in /etc/rudolf/rpk-keys itself, never one added with
	   "rpk key add"), for this base and kernel, holding only what an extension may; nor may it replace an app's package */
	if (!bad && !refuse && sx) {
		int starts = 0;
		if (!SXOK) refuse = "a system extension is installed by an administrator, in Settings › Update and recovery › System extensions (rpk install --system)";
		else if (cls != 1 || !VSYSFILE) refuse = "a system extension must be signed by Rudolf OS's release key";
		else if ((refuse = sysext_fits(&in))) {}
		else if ((refuse = sysext_walk(tmpd, "", iget(&in, "name"), iget(&in, "kernel"), &starts))) {}
		else if (starts > 1) refuse = "it has more than one start script";
	}
	if (!bad && !refuse) { info_t cur; if (!installed_info(name, &cur) && (!strcmp(iget(&cur, "kind"), "sysext")) != sx) refuse = sx ? "an app's package of that name is installed" : "a system extension of that name is installed"; }
	/* (1.5.9: an outside developer's package replaces only its own developer's, never the Library's or the system's) */
	if (!bad && lib && !refuse) { info_t cur; if (!installed_info(name, &cur)) {
		if (cls == 3) { if (strcmp(iget(&cur, "signed"), "developer") || strcmp(iget(&cur, "certkey"), CERT.key)) refuse = "it would replace a package from someone else"; }
		else if (strcmp(iget(&cur, "signed"), "library")) refuse = "it would replace a package that came with Rudolf OS"; } }
	if (!bad && !refuse) { char cmd[900]; snprintf(cmd, sizeof cmd, "%s/db", ROOT); mkdirs(cmd); char *c[] = {"cp", ip, dbt, NULL}; bad = spawn(c, 1) != 0;
		if (!bad) { FILE *o = fopen(dbt, "a"); if (o) { fprintf(o, "\nsigned = %s\n", cls == 3 ? "developer" : lib ? "library" : "system");
			if (cls == 3) fprintf(o, "certkey = %s\ndeveloper = %s\ngithub = %s\n", CERT.key, CERT.name, CERT.github); fclose(o); } else bad = 1; } }
	{ char *u[] = {"umount", "-d", tmpd, NULL}; spawn(u, 1); }
	if (refuse) { unlink(dbt); say("  refused: %s", refuse); if (sx) sx_health("refused the system extension %s %s: %s", name, iget(&in, "version"), refuse); else if (lib) lib_health("refused %s %s, signed by the Rudolf OS Library: %s", name, iget(&in, "version"), refuse); return -1; }
	if (bad) { unlink(dbt); say("  refused: the package information is missing or invalid"); return -1; }
	char store[400], st2[420]; snprintf(store, sizeof store, "%s/store", ROOT); mkdirs(store);
	snprintf(store, sizeof store, "%s/store/%s.rpk", ROOT, name); snprintf(st2, sizeof st2, "%s.new", store);
	char *c[] = {"cp", (char *)path, st2, NULL}; if (spawn(c, 1)) { unlink(dbt); say("  could not store the package (is the disk full?)"); return -1; }
	char was[64] = ""; { info_t cur; if (!installed_info(name, &cur)) snprintf(was, sizeof was, "%s", iget(&cur, "version")); }
	umount_pkg(name);
	rename(st2, store); rename(dbt, dbf);
	if (sx) {   /* (1.6.0) laid over the system at the next start, by /usr/lib/rudolf/sysext; tried once (see there) */
		char p[400]; snprintf(p, sizeof p, "%s/db/%s.disabled", ROOT, name); unlink(p); snprintf(p, sizeof p, "%s/sysext/off/%s", ROOT, name); unlink(p);
		say("Installed the system extension %s %s: it is in use after the next restart", name, iget(&in, "version"));
		if (*was) sx_health("updated the system extension %s from %s to %s (in use after a restart)", name, was, iget(&in, "version"));
		else sx_health("installed the system extension %s %s (in use after a restart)", name, iget(&in, "version"));
		return 0;
	}
	if (mount_pkg(name)) { say("  installed %s %s, but it could not be mounted", name, iget(&in, "version")); return -1; }
	{ char dis[400]; snprintf(dis, sizeof dis, "%s/db/%s.disabled", ROOT, name); unlink(dis); }   /* (a newer version that is not revoked) */
	say("Installed %s %s", name, iget(&in, "version"));
	if (lib) { if (*was) lib_health("updated %s from %s to %s, signed by the Rudolf OS Library", name, was, iget(&in, "version"));
		else lib_health("installed %s %s, signed by the Rudolf OS Library", name, iget(&in, "version")); }
	return 0;
}
static int install_name(const char *name)
{
	pkg_t *k = best(name);
	if (!k) { static int again; if (!again) { again = 1; refresh(); load_available(); k = best(name); } }   /* a new source (the Library) not fetched yet */
	if (!k) { say("No package called \"%s\". Try: rpk search", name); return -1; }
	info_t cur; if (!installed_info(name, &cur) && strverscmp(iget(&cur, "version"), k->version) >= 0) { say("%s %s is already installed", name, iget(&cur, "version")); return 0; }
	say("Getting %s %s (%lld KB) from %s", name, k->version, (k->size + 1023) / 1024, SRC[k->src].loc);
	char dl[400]; snprintf(dl, sizeof dl, "%s/tmp", ROOT); mkdirs(dl); snprintf(dl, sizeof dl, "%s/tmp/%s.download", ROOT, name);
	int net = strcmp(SRC[k->src].type, "disc");
	int r = fetch(&SRC[k->src], k->file, dl) ? (say("  download failed"), -2) : install_file(dl, name, k->sha, !net); unlink(dl);
	/* 1.6.0: the package list kept on this computer was older than the package: the Library publishes a new version
	   under the same file name (Ember Reach 0.4.0 on 2026-11-15, minutes after it was published, while the App Store
	   had the list with 0.3.11), so the download did not match the list, or was gone. The list is fetched again
	   (checked with its key as always) and the package got once more. */
	static int retried;
	if (r && net && (MISMATCH || r == -2) && !retried) {
		retried = 1; say("  the package list on this computer is older than the Library's: checking the list again");
		refresh(); load_available(); r = install_name(name); retried = 0; return r;
	}
	return r ? -1 : 0;
}
static int remove_pkg(const char *name)
{
	info_t in; if (installed_info(name, &in)) { say("%s is not installed", name); return -1; }
	if (!strcmp(iget(&in, "kind"), "sysext") && !SXOK) { say("%s is a system extension: an administrator removes it in Settings › Update and recovery (rpk remove --system)", name); return -1; }
	umount_pkg(name);
	char p[400]; snprintf(p, sizeof p, "%s/store/%s.rpk", ROOT, name); unlink(p); snprintf(p, sizeof p, "%s/db/%s.info", ROOT, name); unlink(p);
	snprintf(p, sizeof p, "%s/db/%s.disabled", ROOT, name); unlink(p);
	if (!strcmp(iget(&in, "kind"), "sysext")) {   /* (1.6.0) nothing of it is left once the computer restarts */
		snprintf(p, sizeof p, "%s/sysext/off/%s", ROOT, name); unlink(p);
		say("Removed the system extension %s: its files are gone after the next restart", name);
		sx_health("removed the system extension %s %s (gone after a restart)", name, iget(&in, "version")); return 0;
	}
	say("Removed %s", name);
	if (!strcmp(iget(&in, "signed"), "library")) lib_health("removed %s %s, from the Rudolf OS Library", name, iget(&in, "version"));
	return 0;
}

/* ---------------- run */
static int run_pkg(const char *name, const char *cmd, char **args, int nargs)
{
	info_t in; if (installed_info(name, &in)) die("%s is not installed", name);
	char dir[300]; snprintf(dir, sizeof dir, "%s/%s", MNT, name);
	if (!is_mounted(dir) && mount_pkg(name)) die("%s could not be mounted", name);
	const char *kind = iget(&in, "kind"); char prog[600];
	if (!strcmp(kind, "cli")) {
		const char *cmds = iget(&in, "commands"); char one[64] = "";
		if (!cmd) { if (!*cmds || strchr(cmds, ' ')) die("say which command: %s", *cmds ? cmds : "(none)"); snprintf(one, sizeof one, "%s", cmds); cmd = one; }
		/* only the commands the package lists, so a name cannot reach outside bin/ */
		int ok = 0; char list[400]; snprintf(list, sizeof list, "%s", cmds);
		for (char *t = strtok(list, " "); t; t = strtok(NULL, " ")) if (!strcmp(t, cmd)) ok = 1;
		/* a package with one command: "rpk run jq -c ." means the command jq with those arguments */
		if (!ok && *cmds && !strchr(cmds, ' ')) { args--; nargs++; args[0] = (char *)cmd; snprintf(one, sizeof one, "%s", cmds); cmd = one; ok = 1; }
		if (!ok) die("%s has no command \"%s\" (it has: %s)", name, cmd, cmds);
		snprintf(prog, sizeof prog, "%s/bin/%s", dir, cmd);
	} else if (!strcmp(kind, "app")) {
		const char *e = iget(&in, "exec"); if (!*e || strstr(e, "..") || *e == '/') die("%s has no program to start", name);
		snprintf(prog, sizeof prog, "%s/%s", dir, e);
	} else die("%s is a %s package: open it from the desktop", name, kind);
	char lib[700], path[900]; const char *ol = getenv("LD_LIBRARY_PATH"), *op = getenv("PATH");
	snprintf(lib, sizeof lib, "%s/lib%s%s", dir, ol && *ol ? ":" : "", ol ? ol : ""); setenv("LD_LIBRARY_PATH", lib, 1);
	snprintf(path, sizeof path, "%s/bin:%s", dir, op ? op : "/usr/bin:/bin"); setenv("PATH", path, 1);
	setenv("RPK_PKG", dir, 1);
	/* ENV in the package: KEY=VALUE lines, $PKG = where it is mounted */
	char envp[400]; snprintf(envp, sizeof envp, "%s/ENV", dir); char *t = slurp(envp, NULL);
	if (t) { for (char *l = strtok(t, "\n"); l; l = strtok(NULL, "\n")) { char *eq = strchr(l, '='); if (!eq || *l == '#') continue; *eq = 0; char v[700] = ""; const char *s = eq + 1; size_t o = 0;
			while (*s && o < sizeof v - 1) { if (!strncmp(s, "$PKG", 4)) { o += snprintf(v + o, sizeof v - o, "%s", dir); s += 4; } else v[o++] = *s++; } v[o < sizeof v ? o : sizeof v - 1] = 0;
			if (!strncmp(l, "LD_", 3) || !strcmp(l, "PATH")) continue; setenv(l, v, 1); } free(t); }
	char **argv = calloc(nargs + 2, sizeof *argv); argv[0] = prog; for (int i = 0; i < nargs; i++) argv[i + 1] = args[i];
	execv(prog, argv); die("cannot start %s: %s", prog, strerror(errno)); return 1;
}

/* ---------------- status (JSON for the desktop) */
static void print_status(void)
{
	char names[MAXP][48]; int ni = list_installed(names, MAXP);
	printf("{\"ok\":true,\"persistent\":%s,\"root\":", strcmp(ROOT, "/run/rudolf/pkg") ? "true" : "false"); jstr(ROOT);
	printf(",\"keys\":%d,\"sources\":[", NKEYS);
	/* (1.6.0; checked: when a web source's list was last downloaded, from its cached file's time; 0 if never: the App Store
	   says how old the Library's list is, beside its Refresh button) */
	for (int i = 0; i < NSRC; i++) { long long ck = 0; if (!strcmp(SRC[i].type, "web")) { char ci[400], cs[400]; struct stat cst; idx_paths(i, ci, cs, sizeof ci); if (!stat(ci, &cst)) ck = (long long)cst.st_mtime; }
		printf("%s{\"loc\":", i ? "," : ""); jstr(SRC[i].loc); printf(",\"type\":"); jstr(SRC[i].type); printf(",\"ok\":%s,\"library\":%s,\"count\":%d,\"serial\":%lld,\"checked\":%lld,\"error\":", SRC[i].ok ? "true" : "false", SRC[i].lib ? "true" : "false", SRC[i].count, SRC[i].serial, ck); jstr(SRC[i].ok ? "" : SRC[i].err); putchar('}'); }
	printf("],\"installed\":[");
	int fi = 1;
	for (int i = 0; i < ni; i++) {
		info_t in; if (installed_info(names[i], &in)) continue;
		if (!strcmp(iget(&in, "kind"), "sysext")) continue;   /* (1.6.0: system extensions are not apps: sysext-status) */
		char dir[300]; snprintf(dir, sizeof dir, "%s/%s", MNT, names[i]); pkg_t *b = best(names[i]);
		printf("%s{", fi ? "" : ","); fi = 0;
		for (int j = 0; j < in.n; j++) { jstr(in.k[j]); putchar(':'); jstr(in.v[j]); putchar(','); }
		{ char dis[400]; snprintf(dis, sizeof dis, "%s/db/%s.disabled", ROOT, names[i]); char *w = slurp(dis, NULL); if (w) { char *nl = strchr(w, '\n'); if (nl) *nl = 0; printf("\"disabled\":"); jstr(w); putchar(','); free(w); } }
		printf("\"mounted\":%s,\"update\":", is_mounted(dir) ? "true" : "false");
		if (b && strverscmp(b->version, iget(&in, "version")) > 0) jstr(b->version); else printf("null");
		putchar('}');
	}
	printf("],\"available\":[");
	int first = 1;
	for (int i = 0; i < NAV; i++) {
		if (best(AV[i].name) != &AV[i] || !strcmp(AV[i].kind, "sysext")) continue;
		pkg_t *k = &AV[i]; info_t in; int inst = !installed_info(k->name, &in);
		printf("%s{\"name\":", first ? "" : ","); first = 0; jstr(k->name); printf(",\"version\":"); jstr(k->version); printf(",\"kind\":"); jstr(k->kind);
		printf(",\"size\":%lld,\"title\":", k->size); jstr(k->title); printf(",\"summary\":"); jstr(k->summary); printf(",\"icon\":"); jstr(k->icon);
		printf(",\"license\":"); jstr(k->license); printf(",\"source\":"); jstr(SRC[k->src].loc); printf(",\"library\":%s,\"installed\":", SRC[k->src].lib ? "true" : "false"); if (inst) jstr(iget(&in, "version")); else printf("null");
		putchar('}');
	}
	printf("]}\n");
}

/* (1.6.0) why an installed extension cannot be used now (NULL: it can): its file's signature again (a system key), the
   base and kernel it was built for, a revocation */
static const char *sysext_check(const char *name)
{
	static char why[240]; info_t in;
	if (installed_info(name, &in) || strcmp(iget(&in, "kind"), "sysext")) return "it is not an installed system extension";
	char dis[400]; snprintf(dis, sizeof dis, "%s/db/%s.disabled", ROOT, name); char *w = slurp(dis, NULL);
	if (w) { w[strcspn(w, "\n")] = 0; snprintf(why, sizeof why, "%s", w); free(w); return why; }
	char f[400]; snprintf(f, sizeof f, "%s/store/%s.rpk", ROOT, name); const char *v = NULL; VCLASS = 0;
	if (verify_rpk(f, &v) < 0) { snprintf(why, sizeof why, "its file is no longer the signed one (%s)", v); return why; }
	if (VCLASS != 1 || !VSYSFILE) return "it is not signed by Rudolf OS's release key";
	return sysext_fits(&in);
}
static void sysext_status(void)
{
	char names[MAXP][48]; int ni = list_installed(names, MAXP); char abi[80], kr[80]; base_abi(abi); kernel_rel(kr);
	char sp[400]; snprintf(sp, sizeof sp, "%s", getenv("RPK_SYSEXT_STATE") ? getenv("RPK_SYSEXT_STATE") : "/run/rudolf/ext/state"); char *st = slurp(sp, NULL);
	printf("{\"ok\":true,\"persistent\":%s,\"base\":", strcmp(ROOT, "/run/rudolf/pkg") ? "true" : "false"); jstr(abi); printf(",\"kernel\":"); jstr(kr);
	{ char so[400]; snprintf(so, sizeof so, "%s/sysext/skip-once", ROOT); printf(",\"skipOnce\":%s", access(so, F_OK) ? "false" : "true"); }
	/* what the start did ("start" line: with extensions, or why none) */
	printf(",\"started\":"); { char *m = NULL; if (st) for (char *l = st; l && *l; ) { char *e = strchr(l, '\n'); if (!strncmp(l, "start\t", 6)) { m = strndup(l + 6, e ? (size_t)(e - l - 6) : strlen(l + 6)); break; } l = e ? e + 1 : NULL; }
		jstr(m ? m : "not yet"); free(m); }
	printf(",\"installed\":["); int first = 1;
	for (int i = 0; i < ni; i++) {
		info_t in; if (installed_info(names[i], &in) || strcmp(iget(&in, "kind"), "sysext")) continue;
		printf("%s{\"name\":", first ? "" : ","); first = 0; jstr(names[i]);
		const char *K[] = {"version", "title", "summary", "license", "base", "kernel", NULL};
		for (int k = 0; K[k]; k++) { printf(",\"%s\":", K[k]); jstr(iget(&in, K[k])); }
		char f[400]; struct stat fs; snprintf(f, sizeof f, "%s/store/%s.rpk", ROOT, names[i]); printf(",\"size\":%lld", stat(f, &fs) ? 0LL : (long long)fs.st_size);
		/* this start (lines NAME VERSION STATE WHY): in use or left out; "next" when installed (or updated) since */
		char state[24] = "next", why[300] = "";
		if (st) { char pat[64]; snprintf(pat, sizeof pat, "%s\t", names[i]);
			for (char *l = st; l && *l; ) { char *e = strchr(l, '\n'); size_t n = e ? (size_t)(e - l) : strlen(l);
				if (!strncmp(l, pat, strlen(pat))) { char ln[600]; snprintf(ln, sizeof ln, "%.*s", (int)(n < 599 ? n : 599), l); char *p = ln + strlen(pat), *t2 = strchr(p, '\t'); if (t2) *t2++ = 0; char *t3 = t2 ? strchr(t2, '\t') : NULL; if (t3) *t3++ = 0;
					if (!strcmp(p, iget(&in, "version"))) { snprintf(state, sizeof state, "%s", t2 ? t2 : ""); snprintf(why, sizeof why, "%s", t3 ? t3 : ""); } }
				l = e ? e + 1 : NULL; } }
		char off[400]; snprintf(off, sizeof off, "%s/sysext/off/%s", ROOT, names[i]); char *o = slurp(off, NULL);
		const char *now = sysext_check(names[i]);
		printf(",\"state\":"); jstr(state); printf(",\"why\":"); jstr(why);
		if (o) { o[strcspn(o, "\n")] = 0; printf(",\"off\":"); jstr(o); free(o); }
		if (now) { printf(",\"problem\":"); jstr(now); }
		printf("}");
	}
	printf("],\"available\":["); first = 1;
	for (int i = 0; i < NAV; i++) {
		pkg_t *k = &AV[i]; if (strcmp(k->kind, "sysext") || best(k->name) != k || SRC[k->src].lib) continue;
		info_t in; int inst = !installed_info(k->name, &in);
		printf("%s{\"name\":", first ? "" : ","); first = 0; jstr(k->name); printf(",\"version\":"); jstr(k->version);
		printf(",\"size\":%lld,\"title\":", k->size); jstr(k->title); printf(",\"summary\":"); jstr(k->summary); printf(",\"license\":"); jstr(k->license);
		printf(",\"installed\":"); if (inst) jstr(iget(&in, "version")); else printf("null"); printf("}");
	}
	printf("]}\n"); free(st);
}
static void lock(void)
{
	char p[300]; snprintf(p, sizeof p, "%s/.lock", ROOT); int f = open(p, O_CREAT | O_RDWR | O_CLOEXEC, 0600);
	if (f < 0 || flock(f, LOCK_EX | LOCK_NB)) die("another rpk is already running");
}
static void usage(void)
{
	puts("rpk - the Rudolf OS package manager\n"
	     "  rpk search [WORD]        packages you can install\n"
	     "  rpk list                 installed packages\n"
	     "  rpk info NAME            details of a package\n"
	     "  rpk install NAME...      install (or update) packages\n"
	     "  rpk install-file FILE    install a signed .rpk file\n"
	     "  rpk remove NAME...       remove packages\n"
	     "  rpk update               get the newest version of everything installed\n"
	     "  rpk refresh              download the package lists again\n"
	     "  rpk run NAME [COMMAND] [ARGS]  start a program from a package\n"
	     "  rpk source list|add|remove [ADDRESS]   where packages come from\n"
	     "  rpk key add BASE64       trust another signing key (administrators)\n"
	     "  rpk sysext-status        system extensions (JSON); rpk sysext-check NAME: can it be used?");
}

int main(int argc, char **argv)
{
	const char *r = getenv("RPK_ROOT");
	snprintf(ROOT, sizeof ROOT, "%s", r ? r : !access("/run/rudolf/persistent", F_OK) ? "/persist/pkg" : "/run/rudolf/pkg");
	snprintf(MNT, sizeof MNT, "%s", getenv("RPK_MNT") ? getenv("RPK_MNT") : "/pkg");
	if (argc < 2 || !strcmp(argv[1], "help") || !strcmp(argv[1], "--help")) { usage(); return argc < 2; }
	const char *c = argv[1];
	/* (1.6.0) --system after the command: system extensions too (rudolfd gives it once an administrator's password is checked) */
	if (argc > 2 && !strcmp(argv[2], "--system")) { SXOK = 1; for (int i = 2; i < argc - 1; i++) argv[i] = argv[i + 1]; argc--; argv[argc] = NULL; }
	if (!strcmp(c, "run")) { if (argc < 3) die("usage: rpk run NAME [COMMAND] [ARGS]"); if (!valid_name(argv[2])) die("bad package name"); return run_pkg(argv[2], argc > 3 ? argv[3] : NULL, argv + 4, argc > 4 ? argc - 4 : 0); }
	mkdirs(ROOT); mkdirs(MNT);
	{ char p[300]; snprintf(p, sizeof p, "%s/index", ROOT); mkdirs(p); }
	load_keys(); load_sources();
	if (!strcmp(c, "status")) { load_available(); print_status(); return 0; }
	if (!strcmp(c, "sysext-status")) { load_available(); sysext_status(); return 0; }
	if (!strcmp(c, "sysext-check")) { if (argc < 3 || !valid_name(argv[2])) die("usage: rpk sysext-check NAME"); load_available(); const char *w = sysext_check(argv[2]); say("%s", w ? w : "ok"); return w ? 1 : 0; }
	if (!strcmp(c, "mount-all")) { char n[MAXP][48]; int k = list_installed(n, MAXP), bad = 0; for (int i = 0; i < k; i++) if (mount_pkg(n[i])) { fprintf(stderr, "rpk: %s could not be mounted\n", n[i]); bad++; } return bad != 0; }
	if (!strcmp(c, "list")) {
		char n[MAXP][48]; int k = list_installed(n, MAXP); if (!k) say("No packages installed. See: rpk search");
		for (int i = 0; i < k; i++) { info_t in; if (!installed_info(n[i], &in)) say("%-16s %-14s %-7s %s", n[i], iget(&in, "version"), iget(&in, "kind"), iget(&in, "title")); }
		return 0;
	}
	if (!strcmp(c, "search")) {
		load_available(); const char *w = argc > 2 ? argv[2] : ""; int shown = 0;
		for (int i = 0; i < NAV; i++) {
			pkg_t *k = &AV[i]; if (best(k->name) != k) continue;
			if (*w && !strcasestr(k->name, w) && !strcasestr(k->title, w) && !strcasestr(k->summary, w)) continue;
			info_t in; int inst = !installed_info(k->name, &in);
			say("%-16s %-14s %-7s %s%s", k->name, k->version, k->kind, k->summary, inst ? "  [installed]" : ""); shown++;
		}
		if (!shown) { int any = 0; for (int i = 0; i < NSRC; i++) if (SRC[i].ok) any = 1; say(any ? "Nothing matches." : "No package lists. Add a source (rpk source add ADDRESS), then: rpk refresh"); }
		return 0;
	}
	if (!strcmp(c, "info")) {
		if (argc < 3 || !valid_name(argv[2])) die("usage: rpk info NAME");
		load_available(); info_t in; pkg_t *k = best(argv[2]);
		if (!installed_info(argv[2], &in)) { for (int i = 0; i < in.n; i++) say("%-10s %s", in.k[i], in.v[i]); if (k && strverscmp(k->version, iget(&in, "version")) > 0) say("%-10s %s", "update", k->version); }
		else if (k) say("name       %s\nversion    %s\nkind       %s\ntitle      %s\nsummary    %s\nlicense    %s\nsize       %lld KB\nsource     %s\n(not installed)", k->name, k->version, k->kind, k->title, k->summary, k->license, (k->size + 1023) / 1024, SRC[k->src].loc);
		else die("no package called \"%s\"", argv[2]);
		return 0;
	}
	if (!strcmp(c, "source")) {
		char p[300]; snprintf(p, sizeof p, "%s/sources", ROOT);
		if (argc < 3 || !strcmp(argv[2], "list")) { for (int i = 0; i < NSRC; i++) say("%-6s %s", SRC[i].type, SRC[i].loc); if (!NSRC) say("No sources."); return 0; }
		if (argc < 4) die("usage: rpk source add|remove ADDRESS");
		const char *a = argv[3]; size_t l = strlen(a);
		if (l > 400 || (strncmp(a, "https://", 8) && strncmp(a, "http://", 7) && *a != '/')) die("an address starts with https://, http:// or /");
		for (const char *q = a; *q; q++) if (!isalnum((unsigned char)*q) && !strchr("/:._-~%+", *q)) die("that address has characters rpk does not accept");
		char clean[420]; snprintf(clean, sizeof clean, "%s", a); while (strlen(clean) > 1 && clean[strlen(clean) - 1] == '/') clean[strlen(clean) - 1] = 0;
		lock();
		char *t = slurp(p, NULL);
		FILE *o = fopen(p, "w"); if (!o) die("cannot save the sources");
		int had = 0;
		if (t) for (char *ln = strtok(t, "\n"); ln; ln = strtok(NULL, "\n")) { if (!strcmp(ln, clean)) { had = 1; if (!strcmp(argv[2], "remove")) continue; } fprintf(o, "%s\n", ln); }
		if (!strcmp(argv[2], "add") && !had) fprintf(o, "%s\n", clean);
		fclose(o); free(t);
		say(!strcmp(argv[2], "add") ? (had ? "Already a source: %s" : "Added %s. Next: rpk refresh") : (had ? "Removed %s" : "Not a source: %s"), clean);
		return 0;
	}
	if (!strcmp(c, "key")) {
		if (argc < 4 || strcmp(argv[2], "add")) die("usage: rpk key add BASE64");
		unsigned char raw[64]; if (strlen(argv[3]) != 44 || EVP_DecodeBlock(raw, (unsigned char *)argv[3], 44) < 32) die("a key is 44 characters of base64 (32 bytes)");
		lock(); char p[300]; snprintf(p, sizeof p, "%s/keys", ROOT); FILE *o = fopen(p, "a"); if (!o) die("cannot save the key"); fprintf(o, "%s\n", argv[3]); fclose(o);
		say("Trusted key added."); return 0;
	}
	if (!strcmp(c, "refresh")) { lock(); refresh(); return 0; }
	if (!strcmp(c, "install-file")) {
		if (argc < 3) die("usage: rpk install-file FILE"); lock();
		say("Checking %s", argv[2]); load_available(); return install_file(argv[2], NULL, NULL, 0) ? 1 : 0;   /* (1.5.9: with the revocations) */
	}
	if (!strcmp(c, "install") || !strcmp(c, "remove")) {
		if (argc < 3) die("usage: rpk %s NAME...", c); lock();
		int inst = !strcmp(c, "install"), bad = 0;
		if (inst) { int have = 0; load_available(); for (int i = 0; i < NSRC; i++) if (SRC[i].ok) have = 1; if (!have) { refresh(); load_available(); } }
		for (int i = 2; i < argc; i++) { if (!valid_name(argv[i])) { say("Bad package name: %s", argv[i]); bad++; continue; } if (inst ? install_name(argv[i]) : remove_pkg(argv[i])) bad++; }
		return bad != 0;
	}
	if (!strcmp(c, "update")) {
		lock(); refresh(); load_available();
		char n[MAXP][48]; int k = list_installed(n, MAXP), done = 0, bad = 0;
		for (int i = 0; i < k; i++) { info_t in; pkg_t *b = best(n[i]); if (installed_info(n[i], &in) || !b || strverscmp(b->version, iget(&in, "version")) <= 0) continue; done++;
			SXOK = !strcmp(iget(&in, "kind"), "sysext");   /* (1.6.0: an extension an administrator installed gets its updates as the apps do) */
			if (install_name(n[i])) bad++; SXOK = 0; }
		say(done ? "Updated %d package(s)." : "Everything is up to date.", done - bad);
		return bad != 0;
	}
	usage(); return 1;
}
