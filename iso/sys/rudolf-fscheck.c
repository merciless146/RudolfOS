/* rudolf-fscheck: what Linux apps and the Debian terminal need from this computer (Rudolf OS 1.2.7). GPL-2.0-or-later.
 *   rudolf-fscheck         key=value lines (rudolfd: GET debian/check; Settings > Apps > Linux > Check; system report)
 * Written for a laptop installed from an older disc and updated, where Flatpak failed with
 * "open(O_TMPFILE): No such file or directory" and the Debian sandbox would not start, with no further detail.
 * Checks: the data partition (mounted, file system, ext features, inode size), unnamed temporary files (O_TMPFILE) in
 * the folders Flatpak and Debian use, user namespaces, Flatpak's repository folders, and the Debian sandbox itself. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static void kv(const char *k, const char *v) { printf("%s=", k); for (const char *c = v; *c; c++) putchar(*c == '\n' ? ' ' : *c); putchar('\n'); }

/* the device, file system and options /persist is mounted with */
static int persist_mount(char *dev, size_t dl, char *fs, size_t fl, char *opts, size_t ol)
{
	FILE *f = fopen("/proc/mounts", "r"); if (!f) return 0;
	char d[256], m[256], t[64], o[512]; int found = 0;
	while (fscanf(f, "%255s %255s %63s %511s %*d %*d", d, m, t, o) == 4)
		if (!strcmp(m, "/persist")) { snprintf(dev, dl, "%s", d); snprintf(fs, fl, "%s", t); snprintf(opts, ol, "%s", o); found = 1; }
	fclose(f); return found;
}
/* ext2/3/4 superblock: features that matter (the names e2fsprogs uses) and the inode size */
static void ext_features(const char *dev)
{
	int f = open(dev, O_RDONLY); unsigned char sb[1024];
	if (f < 0 || pread(f, sb, sizeof sb, 1024) != (ssize_t)sizeof sb) { if (f >= 0) close(f); kv("persist_features", f < 0 ? strerror(errno) : "unreadable"); return; }
	close(f);
	if (sb[56] != 0x53 || sb[57] != 0xEF) { kv("persist_features", "not an ext file system"); return; }
	uint32_t compat = *(uint32_t *)(sb + 0x5C), incompat = *(uint32_t *)(sb + 0x60), ro = *(uint32_t *)(sb + 0x64), rev = *(uint32_t *)(sb + 0x4C);
	uint16_t isz = rev ? *(uint16_t *)(sb + 0x58) : 128;
	char out[512] = ""; size_t n = 0;
#define F(cond, name) if (cond) n += snprintf(out + n, sizeof out - n, "%s%s", n ? " " : "", name)
	F(compat & 0x4, "has_journal"); F(compat & 0x8, "ext_attr"); F(compat & 0x20, "dir_index");
	F(incompat & 0x2, "filetype"); F(incompat & 0x40, "extent"); F(incompat & 0x80, "64bit"); F(incompat & 0x200, "flex_bg");
	F(ro & 0x1, "sparse_super"); F(ro & 0x2, "large_file"); F(ro & 0x8, "huge_file"); F(ro & 0x20, "dir_nlink");
	F(ro & 0x40, "extra_isize"); F(ro & 0x400, "metadata_csum");
#undef F
	kv("persist_features", n ? out : "(none)");
	char b[16]; snprintf(b, sizeof b, "%u", isz); kv("persist_inode_size", b);
	kv("persist_kind", incompat & 0x40 ? "ext4" : compat & 0x4 ? "ext3 (no extents)" : "ext2 (no journal, no extents)");
}
/* an unnamed temporary file in DIR, as Flatpak's OSTree makes them */
static void tmpfile_in(const char *key, const char *dir)
{
	struct stat st;
	if (stat(dir, &st)) { char b[300]; snprintf(b, sizeof b, "missing (%s)", strerror(errno)); kv(key, b); return; }
	int f = open(dir, O_TMPFILE | O_RDWR, 0600);
	if (f < 0) { char b[300]; snprintf(b, sizeof b, "failed: %s", strerror(errno)); kv(key, b); return; }
	close(f); kv(key, "ok");
}
static void userns(void)
{
	pid_t p = fork();
	if (!p) _exit(unshare(CLONE_NEWUSER | CLONE_NEWNS) ? (errno & 0x7f) + 1 : 0);
	int st = 0; waitpid(p, &st, 0); int c = WIFEXITED(st) ? WEXITSTATUS(st) : 127;
	if (!c) kv("userns", "ok"); else { char b[200]; snprintf(b, sizeof b, "failed: %s", strerror(c - 1)); kv("userns", b); }
	FILE *f = fopen("/proc/sys/user/max_user_namespaces", "r"); char v[32] = "?";
	if (f) { if (!fgets(v, sizeof v, f)) strcpy(v, "?"); fclose(f); v[strcspn(v, "\n")] = 0; }
	kv("max_user_namespaces", v);
}
/* the Debian sandbox: started (or already running), or why not (rudolf-debian names the failing step since 1.2.7) */
static void debian(void)
{
	if (access("/persist/debian/etc/rudolf-debian", F_OK)) { kv("debian", "not set up"); return; }
	struct stat st; char o[64];
	if (!stat("/persist/debian", &st)) { snprintf(o, sizeof o, "%u:%u%s", st.st_uid, st.st_gid, st.st_uid == 200000 ? "" : " (expected 200000:200000)"); kv("debian_owner", o); }
	if (access("/persist/debian/.rudolf-ready", F_OK)) { kv("debian", "set up, but the setup did not finish (Settings > Apps > Linux: remove it and set it up again)"); return; }
	FILE *p = popen("/usr/bin/rudolf-debian start 2>&1", "r"); char out[1024] = ""; size_t n = 0;
	if (p) { n = fread(out, 1, sizeof out - 1, p); out[n] = 0; int st = pclose(p);
		if (WIFEXITED(st) && WEXITSTATUS(st) == 0) { kv("debian", "started"); return; } }
	char *e = strstr(out, "rudolf-debian: "); kv("debian", e ? e + 15 : n ? out : "failed (no message)");
}
int main(void)
{
	char dev[256] = "", fs[64] = "", opts[512] = "";
	if (persist_mount(dev, sizeof dev, fs, sizeof fs, opts, sizeof opts)) {
		kv("persist", "mounted"); kv("persist_dev", dev); kv("persist_fs", fs); kv("persist_opts", opts); ext_features(dev);
	} else kv("persist", "not mounted (running live: Linux apps need a computer that keeps files)");
	tmpfile_in("tmpfile_persist", "/persist");
	tmpfile_in("tmpfile_flatpak", "/var/lib/flatpak");
	tmpfile_in("tmpfile_flatpak_repo", "/var/lib/flatpak/repo/tmp");
	tmpfile_in("tmpfile_var_tmp", "/var/tmp");
	tmpfile_in("tmpfile_tmp", "/tmp");
	tmpfile_in("tmpfile_debian", "/persist/debian/tmp");
	struct stat st; kv("flatpak_repo", stat("/var/lib/flatpak/repo", &st) ? "not made yet" : stat("/var/lib/flatpak/repo/tmp", &st) ? "tmp folder missing" : "ok");
	userns();
	debian();
	return 0;
}
