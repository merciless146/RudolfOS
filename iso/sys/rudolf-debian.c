/* rudolf-debian: Debian 13 beside Rudolf OS, in its own user, mount, process, IPC and host-name namespaces.
 * Run by rudolfd as root. GPL-2.0-or-later.
 *
 *   rudolf-debian status                    installed=, version=, running=, size= (key=value lines)
 *   rudolf-debian setup PASSWORD-FILE       download, check, unpack /persist/debian; set the Linux password
 *   rudolf-debian start | stop              the namespaces' first process (started by any command on demand)
 *   rudolf-debian run [--root] [--cwd D] [--log FILE] [--bg] [--nonet] -- CMD ARGS...   (--nonet: no network)
 *   rudolf-debian pty ID [--root]           terminal server on /run/rudolf/term/ID.sock (see below)
 *   rudolf-debian apps                      launchers of installed graphical apps (JSON)
 *   rudolf-debian launch APP-ID             start one of them as the user
 *   rudolf-debian update                    apt-get update + upgrade (as Debian's root)
 *   rudolf-debian remove                    stop and delete Debian
 *
 * Who is who: Debian's root and system users (0-999) are uids 200000-200999 on Rudolf OS, with no rights
 * outside; Debian's "rudolf" (1000) is Rudolf OS's rudolf (1000), so the home folder is theirs on both
 * sides; 1001-65535 are 201001-265535. Debian sees the home folder, the display (/tmp/.X11-unix), the
 * session folder with the sound and D-Bus sockets (/run/user/1000), the GPU (/dev/dri) and the network.
 *
 * Terminal protocol (one request per connection to ID.sock):
 *   "R <offset> [<s>]\n"    -> "<new offset>\n" + output bytes after <offset> (waits up to <s>, default 20 s);
 *                              "X <exit code>\n" once the shell has ended and everything was read
 *   "W <n>\n" + n bytes     -> keyboard input            "S <rows> <cols>\n" -> window size
 *   "K\n"                   -> hang up the shell
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <poll.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <openssl/evp.h>

#define ROOT "/persist/debian"
#define DL "/persist/debian.download"
#define RUND "/run/rudolf/debian"
#define INITPID RUND "/init.pid"
#define TERMD "/run/rudolf/term"
#define CONF "/persist/os/debian.conf"
#define KEYF "/etc/rudolf/rpk-keys"
#define DEFAULT_URL "https://github.com/merciless146/RudolfOS/releases/download/debian-13"
#define SHIFT 200000

static void die(const char *f, ...) { va_list a; va_start(a, f); fputs("rudolf-debian: ", stderr); vfprintf(stderr, f, a); fputc('\n', stderr); va_end(a); exit(1); }
static void say(const char *f, ...) { va_list a; va_start(a, f); vprintf(f, a); putchar('\n'); fflush(stdout); va_end(a); }
static char *slurp(const char *p, size_t *len)
{
	FILE *f = fopen(p, "rb"); if (!f) return NULL;
	size_t cap = 4096, n = 0; char *b = malloc(cap + 1); size_t r;
	while ((r = fread(b + n, 1, cap - n, f)) > 0) { n += r; if (n == cap) { cap *= 2; b = realloc(b, cap + 1); } }
	fclose(f); b[n] = 0; if (len) *len = n; return b;
}
static int write_str(const char *p, const char *s) { int f = open(p, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644); if (f < 0) return -1; ssize_t w = write(f, s, strlen(s)); close(f); return w == (ssize_t)strlen(s) ? 0 : -1; }
static void mkdirs(const char *d, mode_t m) { char t[512]; snprintf(t, sizeof t, "%s", d); for (char *p = t + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(t, m); *p = '/'; } mkdir(t, m); }
static int installed(void) { return !access(ROOT "/etc/rudolf-debian", F_OK) && !access(ROOT "/.rudolf-ready", F_OK); }

/* ------------------------------------------------------------------ namespaces */
static void write_maps(pid_t pid)
{
	char p[64], m[128];
	snprintf(m, sizeof m, "0 %d 1000\n1000 1000 1\n1001 %d 64535\n", SHIFT, SHIFT + 1001);
	snprintf(p, sizeof p, "/proc/%d/uid_map", pid); if (write_str(p, m)) die("cannot set the user map (%s)", strerror(errno));
	snprintf(p, sizeof p, "/proc/%d/gid_map", pid); if (write_str(p, m)) die("cannot set the group map (%s)", strerror(errno));
}
/* fork a child in a new user namespace (mapped as above) that is root inside; returns like fork() */
static pid_t fork_userns(void)
{
	int a[2], b[2]; if (pipe(a) || pipe(b)) die("pipe");
	pid_t p = fork(); if (p < 0) die("fork");
	if (!p) {
		close(a[0]); close(b[1]);
		if (unshare(CLONE_NEWUSER)) { fprintf(stderr, "rudolf-debian: user namespaces are not available (%s)\n", strerror(errno)); _exit(1); }
		char c = 1; if (write(a[1], &c, 1) != 1) _exit(1); close(a[1]);
		if (read(b[0], &c, 1) != 1) _exit(1); close(b[0]);
		if (setresgid(0, 0, 0) || setresuid(0, 0, 0)) _exit(1);
		setgroups(0, NULL);
		return 0;
	}
	close(a[1]); close(b[0]); char c;
	if (read(a[0], &c, 1) != 1) { waitpid(p, NULL, 0); die("could not start the sandbox"); }
	write_maps(p); if (write(b[1], &c, 1) != 1) die("pipe"); close(a[0]); close(b[1]);
	return p;
}
static int bindmnt(const char *src, const char *dst, int dir)
{
	struct stat st; if (stat(src, &st)) return -1;
	if (dir) mkdirs(dst, 0755); else if (access(dst, F_OK)) { int f = open(dst, O_WRONLY | O_CREAT, 0644); if (f >= 0) close(f); }
	return mount(src, dst, NULL, MS_BIND | MS_REC, NULL);
}
static pid_t keeper_pid(void)
{
	char *t = slurp(INITPID, NULL); if (!t) return 0; pid_t p = atoi(t); free(t);
	char x[64], buf[64] = {0}; snprintf(x, sizeof x, "/proc/%d/comm", p);
	int f = open(x, O_RDONLY); if (f < 0) return 0; ssize_t n = read(f, buf, sizeof buf - 1); close(f);
	return n > 0 && !strncmp(buf, "debian-init", 11) ? p : 0;
}
/* 1.2.7: a failing step tells the starter which one and why (errno), through the "ready" pipe: 2, step, errno.
   Before, every failure gave the same "the Debian sandbox did not start". */
static int g_ready = -1;
#define KFAIL(n) do { int e_ = errno; unsigned char b_[3] = {2, (unsigned char)(n), (unsigned char)(e_ > 255 ? 255 : e_)}; if (g_ready >= 0 && write(g_ready, b_, 3)) {} _exit(n); } while (0)
static const char *kstep(int n)
{
	switch (n) {
	case 2: return "new namespaces (unshare)"; case 3: return "fork"; case 4: return "passing the process id";
	case 10: return "making mounts private"; case 11: return "binding the Debian folder"; case 12: return "mounting /proc";
	case 13: return "mounting /dev"; case 14: return "mounting /dev/pts"; case 15: return "entering the Debian folder";
	case 16: return "pivot_root"; case 17: return "entering the new root"; case 18: return "the ready signal";
	default: return "an unknown step";
	}
}
static void keeper_main(void)   /* pid 1 inside: set up the file system, then reap */
{
	prctl(PR_SET_NAME, "debian-init");
	if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL)) KFAIL(10);
	if (mount(ROOT, ROOT, NULL, MS_BIND | MS_REC, NULL)) KFAIL(11);
	if (mount("proc", ROOT "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL)) KFAIL(12);
	bindmnt("/sys", ROOT "/sys", 1);
	/* /dev: a small one, as in Flatpak's sandbox */
	mkdirs(ROOT "/dev", 0755);
	if (mount("tmpfs", ROOT "/dev", "tmpfs", MS_NOSUID | MS_NOEXEC, "mode=755,size=1m")) KFAIL(13);
	const char *nodes[] = {"null", "zero", "full", "random", "urandom", "tty", NULL};
	char a[256], b[256];
	for (int i = 0; nodes[i]; i++) { snprintf(a, sizeof a, "/dev/%s", nodes[i]); snprintf(b, sizeof b, ROOT "/dev/%s", nodes[i]); bindmnt(a, b, 0); }
	mkdirs(ROOT "/dev/pts", 0755);
	if (mount("devpts", ROOT "/dev/pts", "devpts", MS_NOSUID | MS_NOEXEC, "newinstance,ptmxmode=0666,mode=620")) KFAIL(14);
	symlink("pts/ptmx", ROOT "/dev/ptmx");
	mkdirs(ROOT "/dev/shm", 01777); mount("tmpfs", ROOT "/dev/shm", "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777");
	symlink("/proc/self/fd", ROOT "/dev/fd"); symlink("/proc/self/fd/0", ROOT "/dev/stdin");
	symlink("/proc/self/fd/1", ROOT "/dev/stdout"); symlink("/proc/self/fd/2", ROOT "/dev/stderr");
	if (!access("/dev/dri", F_OK)) bindmnt("/dev/dri", ROOT "/dev/dri", 1);
	if (!access("/dev/snd", F_OK)) bindmnt("/dev/snd", ROOT "/dev/snd", 1);
	/* /tmp and /run of its own, with the display and session sockets placed in them */
	mount("tmpfs", ROOT "/tmp", "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777");
	mount("tmpfs", ROOT "/run", "tmpfs", MS_NOSUID | MS_NODEV, "mode=755");
	mkdirs(ROOT "/run/lock", 01777); chmod(ROOT "/run/lock", 01777);
	if (!access("/tmp/.X11-unix", F_OK)) bindmnt("/tmp/.X11-unix", ROOT "/tmp/.X11-unix", 1);
	if (!access("/run/user/1000", F_OK)) bindmnt("/run/user/1000", ROOT "/run/user/1000", 1);
	bindmnt("/home/rudolf", ROOT "/home/rudolf", 1);
	if (access("/etc/resolv.conf", F_OK)) write_str("/etc/resolv.conf", "");
	bindmnt("/etc/resolv.conf", ROOT "/etc/resolv.conf", 0);
	sethostname("rudolf", 6);
	if (chdir(ROOT)) KFAIL(15);
	mkdir(".oldroot", 0700);
	if (syscall(SYS_pivot_root, ".", ".oldroot")) KFAIL(16);
	if (chdir("/")) KFAIL(17);
	umount2("/.oldroot", MNT_DETACH); rmdir("/.oldroot");
	/* ready: tell the starter, then reap whatever ends inside */
	int fd = 3; char c = 1; if (write(fd, &c, 1) != 1) KFAIL(18); close(fd);
	for (int i = 0; i < 1024; i++) close(i);
	open("/dev/null", O_RDWR); dup2(0, 1); dup2(0, 2);
	signal(SIGTERM, SIG_DFL); signal(SIGINT, SIG_IGN); signal(SIGHUP, SIG_IGN);
	for (;;) { if (wait(NULL) < 0 && errno == ECHILD) pause(); }
}
static pid_t start(void)
{
	pid_t k = keeper_pid(); if (k) return k;
	if (!installed()) die("Debian is not set up");
	mkdirs(RUND, 0755);
	int r[2], q2[2]; if (pipe(r) || pipe(q2)) die("pipe");
	pid_t p = fork_userns();
	if (!p) {
		/* inside the user namespace: new mount, PID, IPC and host-name namespaces; the next child is PID 1 */
		close(r[0]); close(q2[0]); g_ready = r[1];
		if (unshare(CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWUTS)) KFAIL(2);
		pid_t q = fork(); if (q < 0) KFAIL(3);
		if (!q) { close(q2[1]); dup2(r[1], 3); if (r[1] != 3) close(r[1]); g_ready = 3; setsid(); keeper_main(); }
		/* this process is still in the outer PID namespace, so q is the keeper's PID as Rudolf OS sees it */
		if (write(q2[1], &q, sizeof q) != sizeof q) KFAIL(4);
		_exit(0);
	}
	close(r[1]); close(q2[1]);
	pid_t q = 0; if (read(q2[0], &q, sizeof q) != sizeof q) q = 0; close(q2[0]);
	int st; waitpid(p, &st, 0);
	unsigned char c[3] = {0}; ssize_t n = read(r[0], c, 3); close(r[0]);
	if (n >= 3 && c[0] == 2) die("the Debian sandbox did not start: %s failed (%s)", kstep(c[1]), strerror(c[2]));
	if (n < 1 || c[0] != 1 || q <= 0) die("the Debian sandbox did not start (%s)", q <= 0 ? "no process" : "no ready signal");
	char s[32]; snprintf(s, sizeof s, "%d\n", q); write_str(INITPID, s);
	k = keeper_pid(); if (!k) die("the Debian sandbox did not start");
	return k;
}
static void stop(void)
{
	pid_t k = keeper_pid(); if (!k) return;
	kill(k, SIGKILL);
	for (int i = 0; i < 50 && keeper_pid(); i++) usleep(100000);
	unlink(INITPID);
}
static void env_for(int root, const char *home)
{
	clearenv();
	setenv("PATH", root ? "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" : "/usr/local/bin:/usr/bin:/bin:/usr/local/games:/usr/games", 1);
	setenv("HOME", home, 1); setenv("USER", root ? "root" : "rudolf", 1); setenv("LOGNAME", root ? "root" : "rudolf", 1);
	setenv("SHELL", "/bin/bash", 1); setenv("LANG", "C.UTF-8", 1); setenv("TERM", "xterm-256color", 1);
	setenv("DISPLAY", ":0", 1); setenv("XDG_RUNTIME_DIR", root ? "/run/user/0" : "/run/user/1000", 1);
	setenv("XDG_CURRENT_DESKTOP", "Rudolf", 1); setenv("XDG_SESSION_TYPE", "x11", 1);
	if (!access("/run/user/1000/pulse/native", F_OK)) setenv("PULSE_SERVER", "unix:/run/user/1000/pulse/native", 1);
	if (!root && !access("/run/user/1000/bus", F_OK)) setenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/run/user/1000/bus", 1);
	if (root) setenv("DEBIAN_FRONTEND", "noninteractive", 0);
}
/* join the running sandbox; returns in a child process that is inside (PID namespace included) */
static int nonet;   /* run: a network namespace of its own (no network) for what runs */
static pid_t enter(int root, const char *cwd)
{
	pid_t k = start();
	const char *ns[] = {"user", "mnt", "uts", "ipc", "pid"};
	int fds[5]; char p[64];
	for (int i = 0; i < 5; i++) { snprintf(p, sizeof p, "/proc/%d/ns/%s", k, ns[i]); if ((fds[i] = open(p, O_RDONLY | O_CLOEXEC)) < 0) die("cannot reach the sandbox"); }
	pid_t c = fork(); if (c < 0) die("fork");
	if (c) { for (int i = 0; i < 5; i++) close(fds[i]); return c; }
	for (int i = 0; i < 5; i++) if (setns(fds[i], 0)) { fprintf(stderr, "rudolf-debian: cannot join the sandbox (%s: %s)\n", ns[i], strerror(errno)); _exit(1); }
	for (int i = 0; i < 5; i++) close(fds[i]);
	pid_t g = fork(); if (g < 0) _exit(1);     /* the PID namespace applies to children */
	if (g) { int st; for (int i = 3; i < 1024; i++) close(i);   /* only waits: keep no pipe ends open */
		while (waitpid(g, &st, 0) < 0 && errno == EINTR) {} _exit(WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st)); }
	if (chdir("/")) _exit(1);
	if (setresgid(0, 0, 0) || setresuid(0, 0, 0)) _exit(1);
	if (nonet && unshare(CLONE_NEWNET)) { fprintf(stderr, "rudolf-debian: cannot cut the network (%s)\n", strerror(errno)); _exit(1); }
	if (!root) {
		gid_t gs[] = {27, 29, 44, 46, 100};
		if (setgroups(5, gs) || setresgid(1000, 1000, 1000) || setresuid(1000, 1000, 1000)) { fprintf(stderr, "rudolf-debian: cannot become the user\n"); _exit(1); }
	} else setgroups(0, NULL);
	env_for(root, root ? "/root" : "/home/rudolf");
	if (chdir(cwd && *cwd ? cwd : (root ? "/root" : "/home/rudolf"))) if (chdir("/")) {}
	return 0;
}

/* ------------------------------------------------------------------ setup (download, check, unpack) */
typedef void CURL;
static struct { void *h; CURL *(*init)(void); int (*setopt)(CURL *, int, ...); int (*perform)(CURL *); int (*getinfo)(CURL *, int, ...); const char *(*strerror)(int); } C;
static int lastpct = -1;
static int prog(void *u, long long dt, long long dn, long long ut, long long un)
{
	(void)u; (void)ut; (void)un;
	if (dt > 1000000) { int pc = (int)(dn * 100 / dt); if (pc != lastpct && pc % 5 == 0) { lastpct = pc; say("progress %d", pc); } }
	return 0;
}
static size_t wcb(char *p, size_t s, size_t n, void *f) { return fwrite(p, s, n, f); }
static int download(const char *url, const char *path, int showprog)
{
	if (!C.h) {
		if (!(C.h = dlopen("libcurl.so.4", RTLD_NOW))) {   /* BusyBox wget as a fallback */
			char cmd[1200]; snprintf(cmd, sizeof cmd, "wget -q -T 60 -O '%s' '%s'", path, url); return system(cmd) ? -1 : 0;
		}
		int (*gi)(long) = dlsym(C.h, "curl_global_init"); C.init = dlsym(C.h, "curl_easy_init"); C.setopt = dlsym(C.h, "curl_easy_setopt");
		C.perform = dlsym(C.h, "curl_easy_perform"); C.getinfo = dlsym(C.h, "curl_easy_getinfo"); C.strerror = dlsym(C.h, "curl_easy_strerror");
		if (gi) gi(3);
	}
	FILE *f = fopen(path, "wb"); if (!f) return -1;
	CURL *h = C.init();
	C.setopt(h, 10002, url); C.setopt(h, 20011, wcb); C.setopt(h, 10001, f); C.setopt(h, 52, 1L); C.setopt(h, 45, 1L);
	C.setopt(h, 78, 30L); C.setopt(h, 19, 1L); C.setopt(h, 20, 60L); C.setopt(h, 10018, "rudolf-debian/1");
	if (!access("/etc/ssl/certs/ca-certificates.crt", R_OK)) C.setopt(h, 10065, "/etc/ssl/certs/ca-certificates.crt");
	char *px = slurp("/run/rudolf/proxy", NULL); if (px) { for (char *e = px; *e; e++) if (*e == '\n') *e = 0; if (*px) C.setopt(h, 10004, px); }
	if (showprog) { lastpct = -1; C.setopt(h, 43, 0L); C.setopt(h, 20219, prog); }
	int r = C.perform(h); long code = 0; C.getinfo(h, 0x200002, &code);
	if (fclose(f)) r = -1;
	if (r) { say("download error: %s", C.strerror ? C.strerror(r) : "failed"); unlink(path); return -1; }
	if (code >= 400) { say("download error: HTTP %ld", code); unlink(path); return -1; }
	return 0;
}
static int verify_sig(const char *msg, size_t ml, const unsigned char *sig, size_t sl)
{
	if (sl != 64) return 0;
	char *t = slurp(KEYF, NULL); if (!t) return 0; int ok = 0;
	for (char *l = strtok(t, "\n"); l && !ok; l = strtok(NULL, "\n")) {
		while (isspace((unsigned char)*l)) l++; if (*l == '#') continue;
		char *e = l; while (*e && !isspace((unsigned char)*e)) e++; *e = 0;
		unsigned char raw[64]; if (strlen(l) != 44 || EVP_DecodeBlock(raw, (unsigned char *)l, 44) < 32) continue;
		EVP_PKEY *k = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, raw, 32); if (!k) continue;
		EVP_MD_CTX *c = EVP_MD_CTX_new();
		ok = EVP_DigestVerifyInit(c, NULL, NULL, NULL, k) == 1 && EVP_DigestVerify(c, sig, 64, (const unsigned char *)msg, ml) == 1;
		EVP_MD_CTX_free(c); EVP_PKEY_free(k);
	}
	free(t); return ok;
}
static int sha256_file(const char *path, char hex[65])
{
	int f = open(path, O_RDONLY | O_CLOEXEC); if (f < 0) return -1;
	EVP_MD_CTX *c = EVP_MD_CTX_new(); EVP_DigestInit_ex(c, EVP_sha256(), NULL);
	static unsigned char buf[1 << 20]; ssize_t n; while ((n = read(f, buf, sizeof buf)) > 0) EVP_DigestUpdate(c, buf, n);
	close(f); unsigned char md[32]; unsigned l; EVP_DigestFinal_ex(c, md, &l); EVP_MD_CTX_free(c);
	for (int i = 0; i < 32; i++) sprintf(hex + 2 * i, "%02x", md[i]); return 0;
}
static const char *field(char *t, const char *k, char *out, size_t n)
{
	size_t kl = strlen(k); out[0] = 0;
	for (char *l = t; l && *l; ) { char *e = strchr(l, '\n'); if (!strncmp(l, k, kl) && l[kl] == ' ') { size_t m = e ? (size_t)(e - l - kl - 1) : strlen(l + kl + 1); if (m >= n) m = n - 1; memcpy(out, l + kl + 1, m); out[m] = 0; break; } l = e ? e + 1 : NULL; }
	return out;
}
static void base_url(char *u, size_t n)
{
	snprintf(u, n, "%s", DEFAULT_URL);
	char *t = slurp(CONF, NULL); if (!t) return;
	char *l = strstr(t, "url="); if (l && (l == t || l[-1] == '\n')) { l += 4; char *e = strchr(l, '\n'); if (e) *e = 0; if (!strncmp(l, "http", 4)) snprintf(u, n, "%s", l); }
	free(t);
}
static int run_in(int root, const char *in, char *const argv[]);
static int cmd_setup(const char *pwfile)
{
	if (getuid()) die("run as root");
	if (access("/persist", W_OK) || system("mountpoint -q /persist")) die("Debian needs an installed Rudolf OS (a data partition); it cannot be kept on a live disc");
	struct statvfs v; if (!statvfs("/persist", &v) && (unsigned long long)v.f_bavail * v.f_frsize < 600ull << 20) die("not enough free space: Debian needs at least 600 MB");
	char *pw = slurp(pwfile, NULL); unlink(pwfile);
	if (!pw) die("no password given");
	char *nl = strchr(pw, '\n'); if (nl) *nl = 0;
	if (strlen(pw) < 4 || strchr(pw, ':')) die("the password needs at least 4 characters and no ':'");
	char url[400], u2[460], hex[65], want[80], file[120], ver[40], sz[40];
	base_url(url, sizeof url);
	stop();
	mkdirs(DL, 0755);
	say("step Checking the download");
	snprintf(u2, sizeof u2, "%s/debian.txt", url); if (download(u2, DL "/debian.txt", 0)) die("could not download the Debian list from %s", url);
	snprintf(u2, sizeof u2, "%s/debian.txt.sig", url); if (download(u2, DL "/debian.txt.sig", 0)) die("could not download the signature");
	size_t ml, sl; char *m = slurp(DL "/debian.txt", &ml), *s = slurp(DL "/debian.txt.sig", &sl);
	if (!m || !s || !verify_sig(m, ml, (unsigned char *)s, sl)) die("refused: the Debian download is not signed by a trusted key");
	field(m, "file", file, sizeof file); field(m, "sha256", want, sizeof want); field(m, "debian", ver, sizeof ver); field(m, "size", sz, sizeof sz);
	if (!*file || strchr(file, '/') || strlen(want) != 64) die("the Debian list is not valid");
	say("step Downloading Debian %s (%lld MB)", ver, atoll(sz) >> 20);
	snprintf(u2, sizeof u2, "%s/%s", url, file);
	char tar[200]; snprintf(tar, sizeof tar, DL "/%s", file);
	if (download(u2, tar, 1)) die("the download failed; check the internet connection and try again");
	if (sha256_file(tar, hex) || strcmp(hex, want)) { unlink(tar); die("refused: the download is damaged (checksum does not match)"); }
	chmod(tar, 0644);
	say("step Unpacking");
	char cmd[300]; snprintf(cmd, sizeof cmd, "rm -rf " ROOT); if (system(cmd)) {}
	mkdir(ROOT, 0755); if (chown(ROOT, SHIFT, SHIFT)) {}
	pid_t p = fork_userns();
	if (!p) {   /* root inside the namespace, so files get Debian's owners shifted as above */
		if (chdir(ROOT)) _exit(1);
		execl("/bin/busybox", "tar", "--numeric-owner", "-xJf", tar, NULL); _exit(127);
	}
	int st; waitpid(p, &st, 0);
	if (!WIFEXITED(st) || WEXITSTATUS(st)) { if (system(cmd)) {} die("unpacking failed"); }
	unlink(tar);
	write_str(ROOT "/.rudolf-ready", ver);
	say("step Setting up your account");
	/* the Linux password, for root and rudolf; the home folder's shell settings when it has none */
	char in[300]; snprintf(in, sizeof in, "root:%s\nrudolf:%s\n", pw, pw); memset(pw, 0, strlen(pw)); free(pw);
	char *a1[] = {"/usr/sbin/chpasswd", NULL};
	int r = run_in(1, in, a1); memset(in, 0, sizeof in);
	if (r) die("could not set the password");
	char *a2[] = {"/bin/sh", "-c", "for f in .bashrc .profile .bash_logout; do [ -e \"$HOME/$f\" ] || cp /etc/skel/$f \"$HOME/\"; done", NULL};
	run_in(0, NULL, a2);
	write_str(RUND "/apps-changed", "1");
	say("done Debian %s is ready", ver);
	return 0;
}

/* ------------------------------------------------------------------ run */
static int run_in(int root, const char *in, char *const argv[])
{
	int pp[2]; if (in && pipe(pp)) die("pipe");
	pid_t c = enter(root, NULL);
	if (!c) {
		if (in) { dup2(pp[0], 0); close(pp[0]); close(pp[1]); }
		execvp(argv[0], argv); fprintf(stderr, "rudolf-debian: %s: %s\n", argv[0], strerror(errno)); _exit(127);
	}
	if (in) { close(pp[0]); if (write(pp[1], in, strlen(in)) < 0) {} close(pp[1]); }
	int st; waitpid(c, &st, 0);
	return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
}
static int cmd_run(int argc, char **argv)
{
	int root = 0, bg = 0; const char *cwd = NULL, *log = NULL; int i = 0;
	for (; i < argc; i++) {
		if (!strcmp(argv[i], "--root")) root = 1; else if (!strcmp(argv[i], "--bg")) bg = 1; else if (!strcmp(argv[i], "--nonet")) nonet = 1;
		else if (!strcmp(argv[i], "--cwd") && i + 1 < argc) cwd = argv[++i]; else if (!strcmp(argv[i], "--log") && i + 1 < argc) log = argv[++i];
		else if (!strcmp(argv[i], "--")) { i++; break; } else break;
	}
	if (i >= argc) die("nothing to run");
	if (bg) { pid_t d = fork(); if (d) return 0; setsid(); }
	int lf = -1; if (log) lf = open(log, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
	pid_t c = enter(root, cwd);
	if (!c) {
		if (bg) { int n = open("/dev/null", O_RDWR); dup2(n, 0); if (lf < 0) { dup2(n, 1); dup2(n, 2); } }
		if (lf >= 0) { dup2(lf, 1); dup2(lf, 2); }
		execvp(argv[i], argv + i); fprintf(stderr, "rudolf-debian: %s: %s\n", argv[i], strerror(errno)); _exit(127);
	}
	int st; waitpid(c, &st, 0);
	return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
}

/* ------------------------------------------------------------------ terminal server */
#define TBUF (512 * 1024)
static char tb[TBUF]; static long long tstart, tend;   /* absolute offsets of the kept output */
static void tadd(const char *p, ssize_t n)
{
	for (ssize_t i = 0; i < n; i++) tb[(tend + i) % TBUF] = p[i];
	tend += n; if (tend - tstart > TBUF) tstart = tend - TBUF;
}
typedef struct { int fd; long long off; time_t until; } waiter;
static void send_from(int fd, long long off, int done, int code)
{
	if (off < tstart) off = tstart; if (off > tend) off = tend;
	if (done && off >= tend) { dprintf(fd, "X %d\n", code); return; }
	dprintf(fd, "%lld\n", tend);
	for (long long o = off; o < tend; ) {
		long long s = o % TBUF, n = tend - o; if (s + n > TBUF) n = TBUF - s;
		ssize_t w = write(fd, tb + s, n); if (w <= 0) break; o += w;
	}
}
static int cmd_pty(const char *id, int root)
{
	for (const char *c = id; *c; c++) if (!isalnum((unsigned char)*c)) die("bad terminal id");
	if (strlen(id) > 32) die("bad terminal id");
	start();
	mkdirs(TERMD, 0700);
	char sp[160]; snprintf(sp, sizeof sp, TERMD "/%s.sock", id); unlink(sp);
	int ls = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0); struct sockaddr_un sa = {.sun_family = AF_UNIX};
	snprintf(sa.sun_path, sizeof sa.sun_path, "%s", sp);
	if (ls < 0 || bind(ls, (struct sockaddr *)&sa, sizeof sa) || listen(ls, 16)) die("cannot open the terminal socket");
	chmod(sp, 0600);
	/* daemon: detach from rudolfd's request */
	pid_t d = fork(); if (d < 0) die("fork"); if (d) { printf("%s\n", id); return 0; }
	setsid(); int nul = open("/dev/null", O_RDWR); dup2(nul, 0); dup2(nul, 1); dup2(nul, 2);
	/* the terminal itself is made inside the sandbox, so programs there see their own /dev/pts */
	int sv[2]; if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv)) _exit(1);
	pid_t sh = enter(root, NULL);
	if (!sh) {
		close(sv[0]);
		int m = posix_openpt(O_RDWR | O_NOCTTY); if (m < 0 || grantpt(m) || unlockpt(m)) _exit(1);
		char *pn = ptsname(m);
		/* hand the master to the server, then become the shell on the other end */
		struct msghdr mh = {0}; char c = 'm'; struct iovec iov = {&c, 1}; char cb[CMSG_SPACE(sizeof(int))] = {0};
		mh.msg_iov = &iov; mh.msg_iovlen = 1; mh.msg_control = cb; mh.msg_controllen = sizeof cb;
		struct cmsghdr *cm = CMSG_FIRSTHDR(&mh); cm->cmsg_level = SOL_SOCKET; cm->cmsg_type = SCM_RIGHTS; cm->cmsg_len = CMSG_LEN(sizeof(int)); memcpy(CMSG_DATA(cm), &m, sizeof(int));
		if (sendmsg(sv[1], &mh, 0) < 0) _exit(1); close(sv[1]);
		setsid();
		int s = open(pn, O_RDWR); if (s < 0) _exit(1);
		ioctl(s, TIOCSCTTY, 0); close(m);
		dup2(s, 0); dup2(s, 1); dup2(s, 2); if (s > 2) close(s);
		struct winsize ws = {30, 100, 0, 0}; ioctl(0, TIOCSWINSZ, &ws);
		if (!root && !access("/etc/motd", R_OK)) { char *t = slurp("/etc/motd", NULL); if (t) { if (write(1, t, strlen(t)) < 0) {} free(t); } }
		execl("/bin/bash", "-bash", NULL); _exit(127);
	}
	close(sv[1]);
	struct msghdr mh = {0}; char c; struct iovec iov = {&c, 1}; char cb[CMSG_SPACE(sizeof(int))];
	mh.msg_iov = &iov; mh.msg_iovlen = 1; mh.msg_control = cb; mh.msg_controllen = sizeof cb;
	int m = -1;
	if (recvmsg(sv[0], &mh, 0) == 1) { struct cmsghdr *cm = CMSG_FIRSTHDR(&mh); if (cm && cm->cmsg_type == SCM_RIGHTS) memcpy(&m, CMSG_DATA(cm), sizeof(int)); }
	close(sv[0]);
	if (m < 0) { unlink(sp); _exit(1); }
	waiter w[32]; int nw = 0, done = 0, code = 0; time_t ended = 0;
	signal(SIGPIPE, SIG_IGN);
	for (;;) {
		struct pollfd p[34]; int np = 0;
		p[np++] = (struct pollfd){ls, POLLIN, 0};
		if (!done) p[np++] = (struct pollfd){m, POLLIN, 0};
		poll(p, np, 1000);
		time_t now = time(NULL);
		if (!done && (p[1].revents & (POLLIN | POLLHUP | POLLERR))) {
			char b[16384]; ssize_t n = read(m, b, sizeof b);
			if (n > 0) tadd(b, n);
			else { int st; done = 1; ended = now; if (waitpid(sh, &st, WNOHANG) > 0) code = WIFEXITED(st) ? WEXITSTATUS(st) : 1; }
		}
		if (p[0].revents & POLLIN) {
			int cfd = accept4(ls, NULL, NULL, SOCK_CLOEXEC);
			if (cfd >= 0) {
				char line[64]; int ln = 0; char ch;
				struct timeval tv = {5, 0}; setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
				while (ln < 63 && read(cfd, &ch, 1) == 1 && ch != '\n') line[ln++] = ch;
				line[ln] = 0;
				if (line[0] == 'R') {
					long long off = 0; int wait = 20; sscanf(line + 2, "%lld %d", &off, &wait); if (wait < 0 || wait > 25) wait = 20;
					if (off < tend || done || nw >= 32 || !wait) { send_from(cfd, off, done, code); close(cfd); }
					else w[nw++] = (waiter){cfd, off, now + wait};
				} else if (line[0] == 'W' && !done) {
					long n = atol(line + 2); char b[8192];
					while (n > 0) { ssize_t r = read(cfd, b, n > (long)sizeof b ? (long)sizeof b : n); if (r <= 0) break; if (write(m, b, r) < 0) break; n -= r; }
					if (write(cfd, "ok\n", 3) < 0) {} close(cfd);
				} else if (line[0] == 'S' && !done) {
					int rows = 0, cols = 0; sscanf(line + 2, "%d %d", &rows, &cols);
					if (rows > 0 && cols > 0 && rows < 500 && cols < 1000) { struct winsize ws = {rows, cols, 0, 0}; ioctl(m, TIOCSWINSZ, &ws); }
					if (write(cfd, "ok\n", 3) < 0) {} close(cfd);
				} else if (line[0] == 'K') {
					if (!done) { pid_t pg = tcgetpgrp(m); if (pg > 0) kill(-pg, SIGHUP); kill(sh, SIGHUP); }
					if (write(cfd, "ok\n", 3) < 0) {} close(cfd);
				} else close(cfd);
			}
		}
		for (int i = 0; i < nw; ) {
			if (w[i].off < tend || done || now >= w[i].until) { send_from(w[i].fd, w[i].off, done, code); close(w[i].fd); w[i] = w[--nw]; }
			else i++;
		}
		if (done) { int st; if (waitpid(sh, &st, WNOHANG) > 0) code = WIFEXITED(st) ? WEXITSTATUS(st) : 1; if (now - ended > 30) break; }
	}
	unlink(sp); _exit(0);
}

/* ------------------------------------------------------------------ graphical apps */
static void jstr(FILE *o, const char *s) { fputc('"', o); for (; s && *s; s++) { unsigned char c = *s; if (c == '"' || c == '\\') fprintf(o, "\\%c", c); else if (c < 0x20) fprintf(o, "\\u%04x", c); else fputc(c, o); } fputc('"', o); }
static int dkey(const char *line, const char *k, char *out, size_t n)
{
	size_t kl = strlen(k); if (strncmp(line, k, kl) || line[kl] != '=') return 0;
	snprintf(out, n, "%s", line + kl + 1); size_t l = strlen(out); while (l && (out[l - 1] == '\n' || out[l - 1] == '\r')) out[--l] = 0; return 1;
}
typedef struct { char name[160], exec[600], icon[200], cls[120], mime[600], cats[200]; int hidden, term; } desk;
static int read_desk(const char *path, desk *d)
{
	memset(d, 0, sizeof *d); FILE *f = fopen(path, "r"); if (!f) return -1;
	char l[1024]; int in = 0, gotname = 0;
	while (fgets(l, sizeof l, f)) {
		if (l[0] == '[') { in = !strncmp(l, "[Desktop Entry]", 15); continue; } if (!in) continue;
		char v[600];
		if (!gotname && dkey(l, "Name", d->name, sizeof d->name)) gotname = 1;
		else if (dkey(l, "Exec", d->exec, sizeof d->exec)) {} else if (dkey(l, "Icon", d->icon, sizeof d->icon)) {}
		else if (dkey(l, "StartupWMClass", d->cls, sizeof d->cls)) {} else if (dkey(l, "MimeType", d->mime, sizeof d->mime)) {}
		else if (dkey(l, "Categories", d->cats, sizeof d->cats)) {}
		else if (dkey(l, "NoDisplay", v, sizeof v) && !strcmp(v, "true")) d->hidden = 1;
		else if (dkey(l, "Hidden", v, sizeof v) && !strcmp(v, "true")) d->hidden = 1;
		else if (dkey(l, "Terminal", v, sizeof v) && !strcmp(v, "true")) d->term = 1;
		else if (dkey(l, "Type", v, sizeof v) && strcmp(v, "Application")) d->hidden = 1;
	}
	fclose(f); return d->name[0] && d->exec[0] ? 0 : -1;
}
/* an icon file inside Debian for an Icon= name (path as seen from Rudolf OS) */
static int find_icon(const char *icon, char *out, size_t n)
{
	if (!icon[0]) return 0;
	if (icon[0] == '/') { snprintf(out, n, ROOT "%s", icon); return !access(out, R_OK) && !strstr(icon, ".."); }
	if (strchr(icon, '/')) return 0;
	const char *sizes[] = {"128x128", "256x256", "96x96", "64x64", "48x48", "scalable", "512x512", "32x32", NULL};
	const char *themes[] = {"hicolor", "Adwaita", "breeze", NULL};
	for (int t = 0; themes[t]; t++) for (int i = 0; sizes[i]; i++) {
		snprintf(out, n, ROOT "/usr/share/icons/%s/%s/apps/%s.png", themes[t], sizes[i], icon); if (!access(out, R_OK)) return 1;
		snprintf(out, n, ROOT "/usr/share/icons/%s/%s/apps/%s.svg", themes[t], sizes[i], icon); if (!access(out, R_OK)) return 1;
	}
	snprintf(out, n, ROOT "/usr/share/pixmaps/%s.png", icon); if (!access(out, R_OK)) return 1;
	snprintf(out, n, ROOT "/usr/share/pixmaps/%s.svg", icon); if (!access(out, R_OK)) return 1;
	snprintf(out, n, ROOT "/usr/share/pixmaps/%s.xpm", icon); return !access(out, R_OK);
}
static int cmd_apps(void)
{
	printf("{\"ok\":true,\"apps\":[");
	DIR *d = installed() ? opendir(ROOT "/usr/share/applications") : NULL; struct dirent *e; int first = 1;
	while (d && (e = readdir(d))) {
		size_t l = strlen(e->d_name); if (l < 9 || strcmp(e->d_name + l - 8, ".desktop")) continue;
		char p[512]; snprintf(p, sizeof p, ROOT "/usr/share/applications/%s", e->d_name);
		desk k; if (read_desk(p, &k) || k.hidden || k.term) continue;
		char id[256]; snprintf(id, sizeof id, "%.*s", (int)(l - 8), e->d_name);
		char ic[600]; int hi = find_icon(k.icon, ic, sizeof ic);
		printf("%s{\"id\":", first ? "" : ","); jstr(stdout, id);
		printf(",\"name\":"); jstr(stdout, k.name); printf(",\"wmclass\":"); jstr(stdout, k.cls);
		printf(",\"mime\":"); jstr(stdout, k.mime); printf(",\"categories\":"); jstr(stdout, k.cats);
		printf(",\"icon\":"); jstr(stdout, hi ? ic : ""); printf("}");
		first = 0;
	}
	if (d) closedir(d);
	printf("]}\n"); return 0;
}
static int cmd_launch(const char *id, const char *file)
{
	for (const char *c = id; *c; c++) if (!isalnum((unsigned char)*c) && !strchr("._-+", *c)) die("bad app id");
	char p[512]; snprintf(p, sizeof p, ROOT "/usr/share/applications/%s.desktop", id);
	desk k; if (read_desk(p, &k)) die("no such app");
	/* Exec= split into words; %f %F %u %U become the file (if any), other field codes are dropped */
	char *argv[64]; int n = 0; char *s = k.exec;
	static char words[64][600];
	while (*s && n < 62) {
		while (*s == ' ') s++; if (!*s) break;
		char *o = words[n]; int q = 0, ol = 0;
		while (*s && (q || *s != ' ') && ol < 598) {
			if (*s == '"') { q = !q; s++; continue; }
			if (*s == '\\' && s[1]) { s++; o[ol++] = *s++; continue; }
			o[ol++] = *s++;
		}
		o[ol] = 0;
		if (o[0] == '%' && o[1] && !o[2]) { if (strchr("fFuU", o[1]) && file && *file) { snprintf(o, 600, "%s", file); argv[n++] = o; } continue; }
		argv[n++] = o;
	}
	argv[n] = NULL; if (!n) die("the app has no command");
	mkdirs(RUND, 0755);
	char log[300]; snprintf(log, sizeof log, RUND "/%s.log", id);
	int lf = open(log, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	pid_t d = fork(); if (d) { printf("started\n"); return 0; }
	setsid();
	pid_t c = enter(0, "/home/rudolf");
	if (!c) {
		int nul = open("/dev/null", O_RDWR); dup2(nul, 0); if (lf >= 0) { dup2(lf, 1); dup2(lf, 2); }
		execvp(argv[0], argv); fprintf(stderr, "%s: %s\n", argv[0], strerror(errno)); _exit(127);
	}
	int st; waitpid(c, &st, 0); _exit(0);
}

/* ------------------------------------------------------------------ status, update, remove */
static long long du(const char *path)
{
	char cmd[300]; snprintf(cmd, sizeof cmd, "du -sk %s 2>/dev/null", path);
	FILE *p = popen(cmd, "r"); long long k = 0; if (p) { if (fscanf(p, "%lld", &k) != 1) k = 0; pclose(p); } return k * 1024;
}
int main(int argc, char **argv)
{
	if (argc < 2) { fprintf(stderr, "usage: rudolf-debian status|setup|passwd|start|stop|run|pty|apps|launch|update|remove\n"); return 1; }
	const char *c = argv[1];
	if (getuid() != 0) die("run as root");
	umask(022);
	if (!strcmp(c, "status")) {
		char *v = slurp(ROOT "/etc/rudolf-debian", NULL); if (v) { char *e = strchr(v, '\n'); if (e) *e = 0; }
		struct statvfs s; long long fr = statvfs("/persist", &s) ? 0 : (long long)s.f_bavail * s.f_frsize;
		printf("installed=%d\nversion=%s\nrunning=%d\npersist=%d\nfree=%lld\n", installed(), v ? v : "", keeper_pid() > 0, !system("mountpoint -q /persist"), fr);
		if (argc > 2 && !strcmp(argv[2], "--size")) printf("size=%lld\n", installed() ? du(ROOT) : 0);
		char u[400]; base_url(u, sizeof u); printf("url=%s\n", u);
		return 0;
	}
	if (!strcmp(c, "setup")) { if (argc < 3) die("setup PASSWORD-FILE"); return cmd_setup(argv[2]); }
	if (!strcmp(c, "start")) { printf("%d\n", start()); return 0; }
	if (!strcmp(c, "stop")) { stop(); return 0; }
	if (!strcmp(c, "run")) return cmd_run(argc - 2, argv + 2);
	if (!strcmp(c, "pty")) { if (argc < 3) die("pty ID"); return cmd_pty(argv[2], argc > 3 && !strcmp(argv[3], "--root")); }
	if (!strcmp(c, "apps")) return cmd_apps();
	if (!strcmp(c, "launch")) { if (argc < 3) die("launch APP-ID [FILE]"); return cmd_launch(argv[2], argc > 3 ? argv[3] : NULL); }
	/* 1.3.0: a new Linux password (Settings > Apps > Linux > Change the Linux password), for root and rudolf, as at setup */
	if (!strcmp(c, "passwd")) {
		if (argc < 3) die("passwd PASSWORD-FILE");
		if (!installed()) { unlink(argv[2]); die("Debian is not set up"); }
		char *pw = slurp(argv[2], NULL); unlink(argv[2]); if (!pw) die("no password given");
		char *nl = strchr(pw, '\n'); if (nl) *nl = 0;
		if (strlen(pw) < 4 || strchr(pw, ':')) { memset(pw, 0, strlen(pw)); free(pw); die("the password needs at least 4 characters and no :"); }
		char in[300]; snprintf(in, sizeof in, "root:%s\nrudolf:%s\n", pw, pw); memset(pw, 0, strlen(pw)); free(pw);
		char *a1[] = {"/usr/sbin/chpasswd", NULL};
		int r = run_in(1, in, a1); memset(in, 0, sizeof in);
		if (r) die("could not set the password");
		say("done the Linux password is changed"); return 0;
	}
	if (!strcmp(c, "update")) {
		if (!installed()) die("Debian is not set up");
		char *a[] = {"/bin/sh", "-c", "apt-get update && apt-get -y -o Dpkg::Options::=--force-confold upgrade && apt-get clean", NULL};
		int r = run_in(1, NULL, a); write_str(RUND "/apps-changed", "1"); return r;
	}
	if (!strcmp(c, "remove")) {
		stop();
		/* the home folder is Rudolf OS's own and is never inside ROOT on disk (it is mounted over at start) */
		if (system("rm -rf " ROOT " " DL)) die("could not delete everything");
		say("Debian was removed"); return 0;
	}
	die("unknown command %s", c);
}
