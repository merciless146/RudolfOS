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
 * Trusted public keys: /etc/rudolf/rpk-keys and <root>/keys (base64, one per line).
 * Sources: the disc's own repository (/live/media/rudolf/pkg) plus lines in <root>/sources.
 * <root> is /persist/pkg on an installed system and /run/rudolf/pkg on a live one (lost at shutdown). */
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

#define MAGIC "RPKSIG01"
#define TRAILER 72
#define DISC_REPO "/live/media/rudolf/pkg"
#define SYS_KEYS "/etc/rudolf/rpk-keys"
#define MAXP 512

static char ROOT[256], MNT[256];


typedef struct { char *name, *version, *kind, *file, *sha, *title, *summary, *icon, *license; long long size; int src; } pkg_t;
typedef struct { char loc[512], type[8]; long long serial; int ok, count; char err[160]; } src_t;
static pkg_t AV[MAXP]; static int NAV;
static src_t SRC[16]; static int NSRC;
static unsigned char KEYS[16][32]; static int NKEYS;

static void die(const char *f, ...) { va_list a; va_start(a, f); fputs("rpk: ", stderr); vfprintf(stderr, f, a); fputc('\n', stderr); va_end(a); exit(1); }
static void say(const char *f, ...) { va_list a; va_start(a, f); vprintf(f, a); putchar('\n'); fflush(stdout); va_end(a); }
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
static void load_keys_from(const char *path)
{
	char *t = slurp(path, NULL); if (!t) return;
	for (char *l = strtok(t, "\n"); l && NKEYS < 16; l = strtok(NULL, "\n")) {
		while (isspace((unsigned char)*l)) l++;
		if (!*l || *l == '#') continue;
		char *e = l; while (*e && !isspace((unsigned char)*e)) e++; *e = 0;
		unsigned char raw[64]; int n = EVP_DecodeBlock(raw, (unsigned char *)l, strlen(l));
		if (strlen(l) == 44 && n >= 32) memcpy(KEYS[NKEYS++], raw, 32);
	}
	free(t);
}
static void load_keys(void) { char p[300]; NKEYS = 0; load_keys_from(getenv("RPK_KEYS") ? getenv("RPK_KEYS") : SYS_KEYS); snprintf(p, sizeof p, "%s/keys", ROOT); load_keys_from(p); }
static int verify(const unsigned char *msg, size_t len, const unsigned char *sig)
{
	for (int i = 0; i < NKEYS; i++) {
		EVP_PKEY *k = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, KEYS[i], 32); if (!k) continue;
		EVP_MD_CTX *c = EVP_MD_CTX_new(); int ok = EVP_DigestVerifyInit(c, NULL, NULL, NULL, k) == 1 && EVP_DigestVerify(c, sig, 64, msg, len) == 1;
		EVP_MD_CTX_free(c); EVP_PKEY_free(k);
		if (ok) return 1;
	}
	return 0;
}
/* checks the trailer; returns the squashfs length, or -1 with a reason */
static long long verify_rpk(const char *path, const char **why)
{
	struct stat st; if (stat(path, &st) || st.st_size < 4096 + TRAILER) { *why = "not a package file"; return -1; }
	int f = open(path, O_RDONLY | O_CLOEXEC); if (f < 0) { *why = "cannot read it"; return -1; }
	unsigned char t[TRAILER]; int ok = pread(f, t, TRAILER, st.st_size - TRAILER) == TRAILER; close(f);
	if (!ok || memcmp(t + 64, MAGIC, 8)) { *why = "not a signed Rudolf package"; return -1; }
	char hex[65], msg[80]; long long body = st.st_size - TRAILER;
	if (sha256_file(path, body, hex)) { *why = "cannot read it"; return -1; }
	snprintf(msg, sizeof msg, "rpk1 %s", hex);
	if (!verify((unsigned char *)msg, strlen(msg), t)) { *why = "the signature is not from a trusted key"; return -1; }
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
	char p[300]; snprintf(p, sizeof p, "%s/sources", ROOT); char *t = slurp(p, NULL);
	if (t) { for (char *l = strtok(t, "\n"); l; l = strtok(NULL, "\n")) if (*l == '/' || !strncmp(l, "http", 4)) add_src(l); free(t); }
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
	if (!disc && (sl != 64 || !verify((unsigned char *)t, il, (unsigned char *)g))) { snprintf(s->err, sizeof s->err, "the package list is not signed by a trusted key"); free(t); free(g); return -1; }
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
	NAV = 0; char idx[400], sig[400];
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
}

/* ---------------- installed packages */
static int is_mounted(const char *dir)
{
	char *m = slurp("/proc/self/mounts", NULL); if (!m) return 0;
	char pat[300]; snprintf(pat, sizeof pat, " %s ", dir); int r = strstr(m, pat) != NULL; free(m); return r;
}
static int mount_pkg(const char *name)
{
	char file[400], dir[300]; snprintf(file, sizeof file, "%s/store/%s.rpk", ROOT, name); snprintf(dir, sizeof dir, "%s/%s", MNT, name);
	if (is_mounted(dir)) return 0;
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
/* verifies a downloaded or local package file and installs it in place of any older copy */
static int install_file(const char *path, const char *want_name, const char *want_sha, int from_disc)
{
	const char *why = NULL;
	if (want_sha) { char hex[65]; if (sha256_file(path, -1, hex) || strcmp(hex, want_sha)) { say("  refused: the download does not match the package list"); return -1; } }
	if (from_disc && want_sha) { if (verify_rpk(path, &why) < 0 && strcmp(why, "the signature is not from a trusted key")) { say("  refused: %s", why); return -1; } }
	else if (verify_rpk(path, &why) < 0) { say("  refused: %s", why); return -1; }
	char tmpd[300]; snprintf(tmpd, sizeof tmpd, "%s/tmp/look", ROOT); mkdirs(tmpd);
	char *a[] = {"mount", "-t", "squashfs", "-o", "loop,ro", (char *)path, tmpd, NULL};
	if (spawn(a, 1)) { say("  refused: the package image cannot be opened"); return -1; }
	info_t in; char ip[400]; snprintf(ip, sizeof ip, "%s/PKGINFO", tmpd);
	int bad = read_info(ip, &in) || !valid_name(iget(&in, "name")) || !*iget(&in, "version") || (want_name && strcmp(iget(&in, "name"), want_name));
	const char *kd = iget(&in, "kind"); if (strcmp(kd, "cli") && strcmp(kd, "app") && strcmp(kd, "webapp") && strcmp(kd, "addon")) bad = 1;
	char name[48]; snprintf(name, sizeof name, "%s", iget(&in, "name"));
	char dbf[400], dbt[420]; snprintf(dbf, sizeof dbf, "%s/db/%s.info", ROOT, name); snprintf(dbt, sizeof dbt, "%s.new", dbf);
	if (!bad) { char cmd[900]; snprintf(cmd, sizeof cmd, "%s/db", ROOT); mkdirs(cmd); char *c[] = {"cp", ip, dbt, NULL}; bad = spawn(c, 1) != 0; }
	{ char *u[] = {"umount", "-d", tmpd, NULL}; spawn(u, 1); }
	if (bad) { unlink(dbt); say("  refused: the package information is missing or invalid"); return -1; }
	char store[400], st2[420]; snprintf(store, sizeof store, "%s/store", ROOT); mkdirs(store);
	snprintf(store, sizeof store, "%s/store/%s.rpk", ROOT, name); snprintf(st2, sizeof st2, "%s.new", store);
	char *c[] = {"cp", (char *)path, st2, NULL}; if (spawn(c, 1)) { unlink(dbt); say("  could not store the package (is the disk full?)"); return -1; }
	umount_pkg(name);
	rename(st2, store); rename(dbt, dbf);
	if (mount_pkg(name)) { say("  installed %s %s, but it could not be mounted", name, iget(&in, "version")); return -1; }
	say("Installed %s %s", name, iget(&in, "version"));
	return 0;
}
static int install_name(const char *name)
{
	pkg_t *k = best(name); if (!k) { say("No package called \"%s\". Try: rpk search", name); return -1; }
	info_t cur; if (!installed_info(name, &cur) && strverscmp(iget(&cur, "version"), k->version) >= 0) { say("%s %s is already installed", name, iget(&cur, "version")); return 0; }
	say("Getting %s %s (%lld KB) from %s", name, k->version, (k->size + 1023) / 1024, SRC[k->src].loc);
	char dl[400]; snprintf(dl, sizeof dl, "%s/tmp", ROOT); mkdirs(dl); snprintf(dl, sizeof dl, "%s/tmp/%s.download", ROOT, name);
	if (fetch(&SRC[k->src], k->file, dl)) { say("  download failed"); unlink(dl); return -1; }
	int r = install_file(dl, name, k->sha, !strcmp(SRC[k->src].type, "disc")); unlink(dl); return r;
}
static int remove_pkg(const char *name)
{
	info_t in; if (installed_info(name, &in)) { say("%s is not installed", name); return -1; }
	umount_pkg(name);
	char p[400]; snprintf(p, sizeof p, "%s/store/%s.rpk", ROOT, name); unlink(p); snprintf(p, sizeof p, "%s/db/%s.info", ROOT, name); unlink(p);
	say("Removed %s", name); return 0;
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
	for (int i = 0; i < NSRC; i++) { printf("%s{\"loc\":", i ? "," : ""); jstr(SRC[i].loc); printf(",\"type\":"); jstr(SRC[i].type); printf(",\"ok\":%s,\"count\":%d,\"serial\":%lld,\"error\":", SRC[i].ok ? "true" : "false", SRC[i].count, SRC[i].serial); jstr(SRC[i].ok ? "" : SRC[i].err); putchar('}'); }
	printf("],\"installed\":[");
	for (int i = 0; i < ni; i++) {
		info_t in; if (installed_info(names[i], &in)) continue;
		char dir[300]; snprintf(dir, sizeof dir, "%s/%s", MNT, names[i]); pkg_t *b = best(names[i]);
		printf("%s{", i ? "," : "");
		for (int j = 0; j < in.n; j++) { jstr(in.k[j]); putchar(':'); jstr(in.v[j]); putchar(','); }
		printf("\"mounted\":%s,\"update\":", is_mounted(dir) ? "true" : "false");
		if (b && strverscmp(b->version, iget(&in, "version")) > 0) jstr(b->version); else printf("null");
		putchar('}');
	}
	printf("],\"available\":[");
	int first = 1;
	for (int i = 0; i < NAV; i++) {
		if (best(AV[i].name) != &AV[i]) continue;
		pkg_t *k = &AV[i]; info_t in; int inst = !installed_info(k->name, &in);
		printf("%s{\"name\":", first ? "" : ","); first = 0; jstr(k->name); printf(",\"version\":"); jstr(k->version); printf(",\"kind\":"); jstr(k->kind);
		printf(",\"size\":%lld,\"title\":", k->size); jstr(k->title); printf(",\"summary\":"); jstr(k->summary); printf(",\"icon\":"); jstr(k->icon);
		printf(",\"license\":"); jstr(k->license); printf(",\"source\":"); jstr(SRC[k->src].loc); printf(",\"installed\":"); if (inst) jstr(iget(&in, "version")); else printf("null");
		putchar('}');
	}
	printf("]}\n");
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
	     "  rpk key add BASE64       trust another signing key (administrators)");
}

int main(int argc, char **argv)
{
	const char *r = getenv("RPK_ROOT");
	snprintf(ROOT, sizeof ROOT, "%s", r ? r : !access("/run/rudolf/persistent", F_OK) ? "/persist/pkg" : "/run/rudolf/pkg");
	snprintf(MNT, sizeof MNT, "%s", getenv("RPK_MNT") ? getenv("RPK_MNT") : "/pkg");
	if (argc < 2 || !strcmp(argv[1], "help") || !strcmp(argv[1], "--help")) { usage(); return argc < 2; }
	const char *c = argv[1];
	if (!strcmp(c, "run")) { if (argc < 3) die("usage: rpk run NAME [COMMAND] [ARGS]"); if (!valid_name(argv[2])) die("bad package name"); return run_pkg(argv[2], argc > 3 ? argv[3] : NULL, argv + 4, argc > 4 ? argc - 4 : 0); }
	mkdirs(ROOT); mkdirs(MNT);
	{ char p[300]; snprintf(p, sizeof p, "%s/index", ROOT); mkdirs(p); }
	load_keys(); load_sources();
	if (!strcmp(c, "status")) { load_available(); print_status(); return 0; }
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
		say("Checking %s", argv[2]); return install_file(argv[2], NULL, NULL, 0) ? 1 : 0;
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
		for (int i = 0; i < k; i++) { info_t in; pkg_t *b = best(n[i]); if (installed_info(n[i], &in) || !b || strverscmp(b->version, iget(&in, "version")) <= 0) continue; done++; if (install_name(n[i])) bad++; }
		say(done ? "Updated %d package(s)." : "Everything is up to date.", done - bad);
		return bad != 0;
	}
	usage(); return 1;
}
