/* rudolf-debian: Debian 13 beside Rudolf OS, in its own user, mount, process, IPC and host-name namespaces.
 * Run by rudolfd as root. GPL-2.0-or-later.
 *
 *   rudolf-debian status                    installed=, version=, running=, size= (key=value lines)
 *   rudolf-debian setup PASSWORD-FILE       download, check, unpack /persist/debian; set the Linux password
 *   rudolf-debian start | stop              the namespaces' first process (started by any command on demand)
 *   rudolf-debian run [--root] [--cwd D] [--log FILE] [--bg] [--nonet] [--only D] -- CMD ARGS...   (--nonet: no network;
 *                                           1.6.1 --only D: nothing of the person's but the folder D, for Blitzen's programs)
 *   rudolf-debian pty ID [--root]           terminal server on /run/rudolf/term/ID.sock (see below)
 *   rudolf-debian apps                      launchers of installed graphical apps (JSON)
 *   rudolf-debian launch APP-ID             start one of them as the user
 *   rudolf-debian update                    apt-get update + upgrade (as Debian's root)
 *   rudolf-debian wine                      install Wine (Windows programs), as Debian's root
 *   rudolf-debian remove                    stop and delete Debian
 *   rudolf-debian rebind                    (1.5.6) the signed-in account's folders, as Rudolf OS has them now
 *   rudolf-debian debinfo FILE.deb          (1.5.6) the package's name, version... (dpkg-deb -f, as the user)
 *   rudolf-debian deb FILE.deb              (1.5.6) install a package from a file, with what it needs (apt-get)
 *   rudolf-debian runfile appimage|bin FILE (1.5.7) run a Linux program that needs no installing, as the user
 *   rudolf-debian keepfile FILE NAME        (1.5.7) ...and put it in the Start menu (a launcher of the person's)
 *   rudolf-debian devices                   (1.5.7) cameras and controllers plugged in since Debian started
 *
 * Who is who: Debian's root and system users (0-999) are uids 200000-200999 on Rudolf OS, with no rights
 * outside; Debian's "rudolf" (1000) is Rudolf OS's rudolf (1000), so the home folder is theirs on both
 * sides; 1001-65535 are 201001-265535. Debian sees the home folder, the display (/tmp/.X11-unix), the
 * session folder with the sound and D-Bus sockets (/run/user/1000), the GPU (/dev/dri) and the network; since 1.5.7
 * also cameras (/dev/video*, media*, v4l-subdev*) and game controllers (/dev/input, /dev/uinput, hidraw*), which
 * Rudolf OS gives to group 1000 (mdev.conf): Debian's rudolf's own group, so its programs can open them.
 *
 * 1.5.9 (plan 15.3, second step): while an account is signed in with its files on the disk, Debian is that account's:
 * Debian's "rudolf" (1000) is the account's own Linux user (acctUID, rudolfd's /run/rudolf/acct-session.env) instead
 * of the desktop's, and its home folder is the account's folder (so Wine's prefix, ~/.wine, and the launchers Wine
 * makes are the account's too); its runtime folder has the account's session D-Bus and the accounts' sound socket.
 * Debian's own user, password and files stay as they were. The map is made when Debian starts, so Debian is stopped
 * when the account changes (rudolfd's rebind at sign-in and sign-out) and started again for the new one on demand.
 * Nobody signed in (or an account without its own user): the desktop's user, as before.
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
#include "origin.h"   /* 1.6.0: where this build comes from (plan Part 19) */

#define ROOT "/persist/debian"
#define DL "/persist/debian.download"
#define RUND "/run/rudolf/debian"
#define INITPID RUND "/init.pid"
#define TERMD "/run/rudolf/term"
#define CONF "/persist/os/debian.conf"
#define KEYF "/etc/rudolf/rpk-keys"
#define DEFAULT_URL "https://github.com/merciless146/RudolfOS/releases/download/debian-13"
#define SHIFT 200000
#ifndef OPEN_BIN
#define OPEN_BIN "/usr/lib/rudolf/rudolf-open"   /* 1.6.0: xdg-open and the browser commands, inside (rudolf-open.c) */
#endif
#define OPEN_SHARE "/run/rudolf-os/share"         /* (inside: its launcher for GIO, first in XDG_DATA_DIRS) */

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

/* ------------------------------------------------------------------ whose Debian it is (1.5.9, plan 15.3) */
#ifndef ACCT_ENV
#define ACCT_ENV "/run/rudolf/acct-session.env"
#endif
#define INITUID RUND "/init.uid"
static unsigned d_uid = 1000;                 /* who Debian's "rudolf" is on Rudolf OS */
static char d_home[300] = "", d_rt[300] = ""; /* the account's home and runtime folder (account mode only) */
static char d_desk[40] = "Rudolf";            /* the desktop's name for its programs: the account's session's (1.6.0 Part 14) */
static int acct_mode(void) { return d_uid != 1000; }
static void who(void)
{
	d_uid = 1000; d_home[0] = d_rt[0] = 0; snprintf(d_desk, sizeof d_desk, "Rudolf");
	char *t = slurp(ACCT_ENV, NULL); if (!t) return;
	char user[40] = "", home[300] = "", rt[300] = "", desk[40] = "";
	for (char *l = strtok(t, "\n"); l; l = strtok(NULL, "\n")) {
		if (!strncmp(l, "XDG_CURRENT_DESKTOP=", 20)) snprintf(desk, sizeof desk, "%s", l + 20);
		else if (!strncmp(l, "USER=", 5)) snprintf(user, sizeof user, "%s", l + 5);
		else if (!strncmp(l, "HOME=", 5)) snprintf(home, sizeof home, "%s", l + 5);
		else if (!strncmp(l, "XDG_RUNTIME_DIR=", 16)) snprintf(rt, sizeof rt, "%s", l + 16);
	}
	free(t);
	/* rudolfd's account users are acctUID (uid 1001 and up), at HOME .../acctUID */
	char *e = NULL; unsigned long u = strncmp(user, "acct", 4) ? 0 : strtoul(user + 4, &e, 10);
	const char *hb = strrchr(home, '/');
	if (!u || !e || *e || u < 1001 || u > 59999 || home[0] != '/' || strstr(home, "..") || !hb || strcmp(hb + 1, user) || rt[0] != '/' || strstr(rt, "..")) return;
	struct stat st; if (stat(home, &st) || !S_ISDIR(st.st_mode) || st.st_uid != u) return;
	d_uid = (unsigned)u; snprintf(d_home, sizeof d_home, "%s", home); snprintf(d_rt, sizeof d_rt, "%s", rt);
	if (!strcmp(desk, "Rudolf:GNOME")) snprintf(d_desk, sizeof d_desk, "%s", desk);   /* (only the two names rudolfd writes) */
}
/* the person's own folder, as Rudolf OS (outside) sees it */
static const char *out_home(void) { return acct_mode() ? d_home : "/home/rudolf"; }

/* ------------------------------------------------------------------ namespaces */
static void write_maps(pid_t pid)
{
	char p[64], m[128], g[128];
	/* Debian's rudolf (1000) is d_uid (the desktop's user, or the signed-in account's); its group stays 1000, the
	   devices' group (cameras, controllers), which the accounts' users are in too */
	snprintf(m, sizeof m, "0 %d 1000\n1000 %u 1\n1001 %d 64535\n", SHIFT, d_uid, SHIFT + 1001);
	snprintf(g, sizeof g, "0 %d 1000\n1000 1000 1\n1001 %d 64535\n", SHIFT, SHIFT + 1001);
	snprintf(p, sizeof p, "/proc/%d/uid_map", pid); if (write_str(p, m)) die("cannot set the user map (%s)", strerror(errno));
	snprintf(p, sizeof p, "/proc/%d/gid_map", pid); if (write_str(p, g)) die("cannot set the group map (%s)", strerror(errno));
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
/* 1.6.2 (docs/plan-1.6.2.md Part 34, step 4): Linux apps (Simple Scan) use the network scanners Rudolf OS found: its list
   (/persist/etc/rudolf/scanners.conf, sane-airscan's settings that rudolfd writes after each Find scanners; their own
   discovery off) bound read only over Debian's /etc/sane.d/airscan.conf, when Debian has SANE (its /etc/sane.d). They
   reach the scanners as any program of the account reaches the network; nothing listens, and no USB device is given. */
#ifndef SCAN_SHARE
#define SCAN_SHARE "/persist/etc/rudolf/scanners.conf"
#endif
static void scanners_share(void)
{
	struct stat st; if (lstat(SCAN_SHARE, &st) || !S_ISREG(st.st_mode) || stat(ROOT "/etc/sane.d", &st) || !S_ISDIR(st.st_mode)) return;
	if (!bindmnt(SCAN_SHARE, ROOT "/etc/sane.d/airscan.conf", 0)) mount(NULL, ROOT "/etc/sane.d/airscan.conf", NULL, MS_REMOUNT | MS_BIND | MS_RDONLY, NULL);
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
/* 1.5.7: the device nodes Debian's programs get one by one: cameras, Steam Input's uinput, hidraw (controllers) */
static int dev_wanted(const char *n)
{
	static const char *const P[] = {"video", "media", "v4l-subdev", "hidraw", NULL};
	if (!strcmp(n, "uinput")) return 1;
	for (int i = 0; P[i]; i++) { size_t l = strlen(P[i]); if (!strncmp(n, P[i], l) && isdigit((unsigned char)n[l])) { for (const char *c = n + l; *c; c++) if (!isdigit((unsigned char)*c)) return 0; return 1; } }
	return 0;
}
/* 1.6.1 (docs/plan-1.6.1.md Part 9): Rudolf OS's print queue (rudolf-printq), at the place CUPS's clients look for a CUPS
   server: Debian's lp, GTK's and Qt's print dialogs and LibreOffice print through it, each job its sender's (the
   account's own Linux user, as the queue sees it through this user namespace), PDF only */
static void print_bind(void)
{
	if (access("/run/cups/cups.sock", F_OK)) return;
	mkdirs(ROOT "/run/cups", 0755); bindmnt("/run/cups", ROOT "/run/cups", 1);
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
	/* 1.5.8 (plan 7.2): USB devices for adb and scrcpy (a phone with USB debugging on, which devplug gives to the
	   desktop user); the folder itself, so devices plugged in later appear in it */
	bindmnt("/dev/bus/usb", ROOT "/dev/bus/usb", 1);
	mkdirs(ROOT "/dev/pts", 0755);
	if (mount("devpts", ROOT "/dev/pts", "devpts", MS_NOSUID | MS_NOEXEC, "newinstance,ptmxmode=0666,mode=620")) KFAIL(14);
	symlink("pts/ptmx", ROOT "/dev/ptmx");
	mkdirs(ROOT "/dev/shm", 01777); mount("tmpfs", ROOT "/dev/shm", "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777");
	symlink("/proc/self/fd", ROOT "/dev/fd"); symlink("/proc/self/fd/0", ROOT "/dev/stdin");
	symlink("/proc/self/fd/1", ROOT "/dev/stdout"); symlink("/proc/self/fd/2", ROOT "/dev/stderr");
	if (!access("/dev/dri", F_OK)) bindmnt("/dev/dri", ROOT "/dev/dri", 1);
	if (!access("/dev/snd", F_OK)) bindmnt("/dev/snd", ROOT "/dev/snd", 1);
	/* 1.5.7: game controllers (the folder, so ones plugged in later are there too) and the single device nodes:
	   cameras, Steam Input's uinput, hidraw (later ones: rudolf-debian devices, from mdev) */
	if (!access("/dev/input", F_OK)) bindmnt("/dev/input", ROOT "/dev/input", 1);
	{ DIR *dd = opendir("/dev"); struct dirent *de; while (dd && (de = readdir(dd))) if (dev_wanted(de->d_name)) {
		snprintf(a, sizeof a, "/dev/%s", de->d_name); snprintf(b, sizeof b, ROOT "/dev/%s", de->d_name); bindmnt(a, b, 0); }
	  if (dd) closedir(dd); }
	/* /tmp and /run of its own, with the display and session sockets placed in them */
	mount("tmpfs", ROOT "/tmp", "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777");
	mount("tmpfs", ROOT "/run", "tmpfs", MS_NOSUID | MS_NODEV, "mode=755");
	mkdirs(ROOT "/run/lock", 01777); chmod(ROOT "/run/lock", 01777);
	if (!access("/tmp/.X11-unix", F_OK)) bindmnt("/tmp/.X11-unix", ROOT "/tmp/.X11-unix", 1);
	/* 1.6.0 (plan Part 20): the smart card service's socket folder (made by the session, so a reader plugged in later is
	   seen too): Debian's own OpenSC, Firefox or a VPN client can use the card */
	if (!access("/run/pcscd", F_OK)) { mkdirs(ROOT "/run/pcscd", 0755); bindmnt("/run/pcscd", ROOT "/run/pcscd", 1); }
	print_bind();   /* 1.6.1 (plan Part 9): the print queue */
	/* 1.6.0 (plan Part 9): udev's device database, read only, for libudev in Debian's programs (Steam and SDL find
	   controllers by it, as on any other Linux; with mdev there is none) */
	if (!access("/run/udev/data", F_OK) && !bindmnt("/run/udev", ROOT "/run/udev", 1))
		mount(NULL, ROOT "/run/udev", NULL, MS_REMOUNT | MS_BIND | MS_RDONLY, NULL);
	if (acct_mode()) {
		/* 1.5.9 (15.3): the account's: a runtime folder of Debian's own, its user's (Debian's 1000), with the mount
		   points for the account's session bus and the sound socket, which acct_binds() places from outside (the
		   keeper itself, Debian's root, may not go into them); the home folder likewise */
		mkdirs(ROOT "/run/user", 0755); mkdir(ROOT "/run/user/1000", 0700); mount("tmpfs", ROOT "/run/user/1000", "tmpfs", MS_NOSUID | MS_NODEV, "mode=700,uid=1000,gid=1000,size=1m");
		mkdirs(ROOT "/run/user/1000/pulse", 0755);
		{ int f = open(ROOT "/run/user/1000/bus", O_WRONLY | O_CREAT | O_CLOEXEC, 0600); if (f >= 0) close(f);
		  f = open(ROOT "/run/user/1000/pulse/native", O_WRONLY | O_CREAT | O_CLOEXEC, 0600); if (f >= 0) close(f); }
		mkdirs(ROOT "/home/rudolf", 0755);
		goto homed;
	}
	if (!access("/run/user/1000", F_OK)) bindmnt("/run/user/1000", ROOT "/run/user/1000", 1);
	/* 1.5.6: /home/rudolf without what is mounted on it. The signed-in account's folders bound there by rudolfd would
	   come in as part of this copy, which in Debian's user namespace locks them in place: rebind could never take
	   them off, so after sign-out the account's folder stayed in use and its folders' names readable (QEMU t156).
	   The folder underneath (rudolf-init binds /persist/home/rudolf there) has nothing mounted on it; rebind puts
	   the account's folders on top. */
	{ struct stat h, p; mkdirs(ROOT "/home/rudolf", 0755);
	  if (stat("/home/rudolf", &h) || stat("/persist/home/rudolf", &p) || h.st_dev != p.st_dev || h.st_ino != p.st_ino ||
	      mount("/persist/home/rudolf", ROOT "/home/rudolf", NULL, MS_BIND, NULL))
		bindmnt("/home/rudolf", ROOT "/home/rudolf", 1); }
	/* 1.5.6: with "Encrypt my data", Downloads is a link into the encrypted folder (/persist/vault/downloads), which is
	   outside Debian: the folder is placed at the same path inside, so the link works there too. The downloads store's
	   own bind (/run/rudolf/dl) is taken when there is one, so that this is the store and not an account's folder
	   bound over it; the account's comes on top (rebind). */
	{ char t[300]; ssize_t n = readlink("/home/rudolf/Downloads", t, sizeof t - 1);
	  if (n > 0) { t[n] = 0; char d[400]; snprintf(d, sizeof d, ROOT "%s", t);
	    if (t[0] == '/' && !strstr(t, "..") && !strncmp(t, "/persist/", 9)) bindmnt(!access("/run/rudolf/dl/.", F_OK) ? "/run/rudolf/dl" : t, d, 1); } }
homed:
	if (access("/etc/resolv.conf", F_OK)) write_str("/etc/resolv.conf", "");
	bindmnt("/etc/resolv.conf", ROOT "/etc/resolv.conf", 0);
	scanners_share();   /* 1.6.2 (Part 34, step 4) */
	/* 1.6.0: rudolf-fetch in the Terminal (Rudolf's logo and this computer's details), read only: Rudolf's own files
	   (the logo, VERSION) in /usr/share/rudolf-os, the command as /usr/local/bin/rudolf-fetch, and the facts Debian
	   cannot see (the desktop engine, the update channel) in /run/rudolf-os/facts. Nothing else in Debian changes. */
	if (!access("/usr/lib/rudolf/rudolf-fetch", X_OK)) {
		mkdirs(ROOT "/usr/share/rudolf-os", 0755);
		if (!bindmnt("/usr/share/rudolf", ROOT "/usr/share/rudolf-os", 1)) mount(NULL, ROOT "/usr/share/rudolf-os", NULL, MS_REMOUNT | MS_BIND | MS_RDONLY, NULL);
		mkdirs(ROOT "/usr/local/bin", 0755);
		if (!bindmnt("/usr/lib/rudolf/rudolf-fetch", ROOT "/usr/local/bin/rudolf-fetch", 0)) mount(NULL, ROOT "/usr/local/bin/rudolf-fetch", NULL, MS_REMOUNT | MS_BIND | MS_RDONLY, NULL);
		char f[400] = "", e[32] = "", u[300] = ""; char *t = slurp("/run/rudolf/engine-choice", NULL);
		if (t) { snprintf(e, sizeof e, "%s", t); e[strcspn(e, "\n")] = 0; free(t); }
		t = slurp("/persist/os/update.conf", NULL);
		if (t) { char *q = strstr(t, "url="); if (q && (q == t || q[-1] == '\n')) { snprintf(u, sizeof u, "%s", q + 4); u[strcspn(u, "\n")] = 0; } free(t); }
		size_t ul = strlen(u); const char *ch = !ul || (ul >= 7 && !strcmp(u + ul - 7, "/stable")) ? "stable" : ul >= 5 && !strcmp(u + ul - 5, "/beta") ? "beta" : "custom";
		snprintf(f, sizeof f, "engine=%s\nchannel=%s\n", e, ch);
		mkdirs(ROOT "/run/rudolf-os", 0755); write_str(ROOT "/run/rudolf-os/facts", f);
	}
	/* 1.6.0: links and files Debian's programs open go to Rudolf OS's desktop (rudolf-open, through the account's session
	   bus to the Rudolf portal, as Flatpak apps' do): Electron's shell.openExternal and Qt run xdg-open (and xdg-email),
	   other programs x-www-browser, sensible-browser or $BROWSER; GIO (GTK's gtk_show_uri, gio open) finds the launcher
	   below as the default for web and mail links (XDG_DATA_DIRS, env_for). Read only, as rudolf-fetch; nothing else in
	   Debian changes, and Debian's own xdg-open is used when the desktop cannot be reached. */
	if (!access(OPEN_BIN, X_OK)) {
		static const char *const NAMES[] = {"xdg-open", "xdg-email", "x-www-browser", "sensible-browser", "www-browser", "gnome-www-browser", NULL};
		mkdirs(ROOT "/usr/local/bin", 0755);
		for (int i = 0; NAMES[i]; i++) { snprintf(b, sizeof b, ROOT "/usr/local/bin/%s", NAMES[i]);
			if (!bindmnt(OPEN_BIN, b, 0)) mount(NULL, b, NULL, MS_REMOUNT | MS_BIND | MS_RDONLY, NULL); }
		mkdirs(ROOT OPEN_SHARE "/applications", 0755);
		write_str(ROOT OPEN_SHARE "/applications/rudolf-open.desktop", "[Desktop Entry]\nType=Application\nName=Rudolf OS\n"
			"Comment=Opens links in Oracle, mail in Mail and files in their app on Rudolf OS's desktop\nExec=/usr/local/bin/xdg-open %u\n"
			"NoDisplay=true\nTerminal=false\nMimeType=x-scheme-handler/http;x-scheme-handler/https;x-scheme-handler/mailto;text/html;application/xhtml+xml;\n");
		write_str(ROOT OPEN_SHARE "/applications/mimeapps.list", "[Default Applications]\nx-scheme-handler/http=rudolf-open.desktop\n"
			"x-scheme-handler/https=rudolf-open.desktop\nx-scheme-handler/mailto=rudolf-open.desktop\ntext/html=rudolf-open.desktop\napplication/xhtml+xml=rudolf-open.desktop\n");
	}
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
static int cmd_rebind(void);
static void stop(void);
static int acct_binds(pid_t k);
static unsigned keeper_uid(void) { char *t = slurp(INITUID, NULL); unsigned u = t ? (unsigned)strtoul(t, NULL, 10) : 1000; free(t); return u ? u : 1000; }
static pid_t start(void)
{
	who();
	pid_t k = keeper_pid();
	/* 1.5.9 (15.3): Debian started for another user (an account signed in or out since): stopped, started again */
	if (k && keeper_uid() != d_uid) { stop(); k = 0; }
	if (k) return k;
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
	snprintf(s, sizeof s, "%u\n", d_uid); write_str(INITUID, s);
	k = keeper_pid(); if (!k) die("the Debian sandbox did not start");
	if (acct_mode()) { if (acct_binds(k)) { stop(); die("the account's folder could not be given to Debian"); } }
	else cmd_rebind();   /* 1.5.6: the signed-in account's folders */
	return k;
}
static void stop(void)
{
	pid_t k = keeper_pid(); if (!k) return;
	kill(k, SIGKILL);
	for (int i = 0; i < 50 && keeper_pid(); i++) usleep(100000);
	unlink(INITPID); unlink(INITUID);
}
static void env_for(int root, const char *home)
{
	clearenv();
	setenv("PATH", root ? "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" : "/usr/local/bin:/usr/bin:/bin:/usr/local/games:/usr/games", 1);
	setenv("HOME", home, 1); setenv("USER", root ? "root" : "rudolf", 1); setenv("LOGNAME", root ? "root" : "rudolf", 1);
	setenv("SHELL", "/bin/bash", 1); setenv("LANG", "C.UTF-8", 1); setenv("TERM", "xterm-256color", 1);
	setenv("DISPLAY", ":0", 1); setenv("XDG_RUNTIME_DIR", root ? "/run/user/0" : "/run/user/1000", 1);
	/* (1.6.0 Part 14: the account's keyring on its session bus: named GNOME's too, so Electron apps keep sign-ins in it) */
	setenv("XDG_CURRENT_DESKTOP", !root && acct_mode() ? d_desk : "Rudolf", 1); setenv("XDG_SESSION_TYPE", "x11", 1);
	if (!access("/run/user/1000/pulse/native", F_OK)) setenv("PULSE_SERVER", "unix:/run/user/1000/pulse/native", 1);
	if (!root && !access("/run/user/1000/bus", F_OK)) setenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/run/user/1000/bus", 1);
	if (root) setenv("DEBIAN_FRONTEND", "noninteractive", 0);
	/* 1.6.0: links to Rudolf OS's desktop (rudolf-open, bound by the keeper): the browser of programs that ask $BROWSER
	   (Python's webbrowser, git, many command-line tools), and GIO's default for web and mail links */
	if (!root && !access("/usr/local/bin/x-www-browser", X_OK) && !access(OPEN_SHARE "/applications/rudolf-open.desktop", R_OK)) {
		setenv("BROWSER", "/usr/local/bin/x-www-browser", 1);
		setenv("XDG_DATA_DIRS", OPEN_SHARE ":/usr/local/share:/usr/share", 1);
	}
}
/* join the running sandbox; returns in a child process that is inside (PID namespace included) */
static int nonet;   /* run: a network namespace of its own (no network) for what runs */
static const char *only_dir;   /* run --only: the one folder of the person's that what runs may see */
/* 1.6.1 (docs/plan-1.6.1.md Part 26): a program Blitzen runs (rudolfd's ai/run) sees nothing of the person's but its own
 * folder: in a mount namespace of its own (made inside Debian's, as Debian's root), the home folders, /root, /tmp (the
 * display's socket), the session's folder (D-Bus, the sound), the smart cards', the printers', USB drives, and the
 * devices that see or hear (input, sound, cameras, USB, hidraw) are covered by empty ones; then its folder alone is put
 * back. The keyring, ~/.ssh, ~/.gnupg, other accounts' folders, the Documents and rudolfd's token are not there (rudolfd's
 * folders, /run/rudolf and /persist, were never in Debian). With --nonet (always for ai/run) it has no network either. */
static int run_only(const char *dir)
{
	if (!dir || dir[0] != '/' || strstr(dir, "..")) return -1;
	if (unshare(CLONE_NEWNS) || mount(NULL, "/", NULL, MS_REC | MS_SLAVE, NULL)) return -1;
	/* (held open in the new namespace, before what is above it is covered: a bind needs a source of its own namespace) */
	int keep = open(dir, O_PATH | O_DIRECTORY | O_CLOEXEC); if (keep < 0) return -1;
	static const char *const hide[] = { "/home", "/root", "/tmp", "/var/tmp", "/run/user", "/run/pcscd", "/run/cups", "/run/udev", "/media", "/mnt", "/dev/shm", NULL };
	for (int i = 0; hide[i]; i++) { struct stat st; if (lstat(hide[i], &st) || !S_ISDIR(st.st_mode)) continue;
		if (mount("tmpfs", hide[i], "tmpfs", MS_NOSUID | MS_NODEV, !strcmp(hide[i], "/tmp") || !strcmp(hide[i], "/var/tmp") || !strcmp(hide[i], "/dev/shm") ? "size=64m,mode=1777" : "size=1m,mode=755")) { close(keep); return -1; } }
	static const char *const devd[] = { "/dev/input", "/dev/snd", "/dev/bus/usb", "/dev/dri", NULL };
	for (int i = 0; devd[i]; i++) if (!access(devd[i], F_OK) && mount("tmpfs", devd[i], "tmpfs", MS_NOSUID | MS_NOEXEC, "size=16k,mode=755")) { close(keep); return -1; }
	DIR *d = opendir("/dev"); struct dirent *e;
	while (d && (e = readdir(d))) if (!strncmp(e->d_name, "video", 5) || !strncmp(e->d_name, "media", 5) || !strncmp(e->d_name, "hidraw", 6) || !strcmp(e->d_name, "uinput") || !strncmp(e->d_name, "v4l-subdev", 10)) {
		char p[300]; snprintf(p, sizeof p, "/dev/%s", e->d_name); if (mount("/dev/null", p, NULL, MS_BIND, NULL)) { closedir(d); close(keep); return -1; } }
	if (d) closedir(d);
	/* its own folder back, where it was */
	char t[512]; snprintf(t, sizeof t, "%s", dir); for (char *q = t + 1; *q; q++) if (*q == '/') { *q = 0; mkdir(t, 0755); *q = '/'; } mkdir(t, 0755);
	char fp[64]; snprintf(fp, sizeof fp, "/proc/self/fd/%d", keep);
	int r = mount(fp, dir, NULL, MS_BIND, NULL); close(keep); return r ? -1 : 0;
}
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
	if (only_dir && run_only(only_dir)) { fprintf(stderr, "rudolf-debian: cannot keep the program to its folder (%s)\n", strerror(errno)); _exit(1); }
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
	origin_release(u, n, "debian-13");   /* (1.6.0: this build's own releases, origin.h; DEFAULT_URL is the official one) */
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
		else if (!strcmp(argv[i], "--only") && i + 1 < argc) only_dir = argv[++i];
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
typedef struct { char name[160], exec[600], icon[200], cls[120], mime[600], cats[200], path[400]; int hidden, term, nodisp; } desk;
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
		else if (dkey(l, "Categories", d->cats, sizeof d->cats)) {} else if (dkey(l, "Path", d->path, sizeof d->path)) {}
		else if (dkey(l, "NoDisplay", v, sizeof v) && !strcmp(v, "true")) d->nodisp = 1;
		else if (dkey(l, "Hidden", v, sizeof v) && !strcmp(v, "true")) d->hidden = 1;
		else if (dkey(l, "Terminal", v, sizeof v) && !strcmp(v, "true")) d->term = 1;
		else if (dkey(l, "Type", v, sizeof v) && strcmp(v, "Application")) d->hidden = 1;
	}
	fclose(f);
	/* (1.4.9) Exec= is first unescaped as a string value (\\ \s \t \n \r), as the desktop entry rules say; the
	   words are split after that. Wine writes Windows paths as C:\\\\users\\\\..., which must reach it as C:\users\... */
	{ char *r = d->exec, *w = d->exec; while (*r) { if (r[0] == '\\' && r[1]) { char c = r[1]; *w++ = c == 's' ? ' ' : c == 't' ? '\t' : c == 'n' ? '\n' : c == 'r' ? '\r' : c == '\\' ? '\\' : (*w++ = '\\', c); r += 2; } else *w++ = *r++; } *w = 0; }
	return d->name[0] && d->exec[0] ? 0 : -1;
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
	/* 1.4.9: the person's own icons (Wine puts each installed Windows program's icon here) */
	for (int i = 0; sizes[i]; i++) { snprintf(out, n, "%s/.local/share/icons/hicolor/%s/apps/%s.png", out_home(), sizes[i], icon); if (!access(out, R_OK)) return 1; }
	snprintf(out, n, ROOT "/usr/share/pixmaps/%s.png", icon); if (!access(out, R_OK)) return 1;
	snprintf(out, n, ROOT "/usr/share/pixmaps/%s.svg", icon); if (!access(out, R_OK)) return 1;
	snprintf(out, n, ROOT "/usr/share/pixmaps/%s.xpm", icon); return !access(out, R_OK);
}
/* the folders launchers are read from: Debian's menu, and (1.4.8) Wine's own folder, where Debian keeps the "Wine
   Windows Program Loader" (wine.desktop: opens .exe, .msi and .bat files; kept out of menus) */
static const char *const APPDIRS[] = { "/usr/share/applications", "/usr/share/wine/applications", NULL };
/* 1.4.9: launchers in the person's own folder (/home/rudolf/.local/share/applications, any depth): Wine makes one there
   for each Windows program it installs (Starport GE: wine/Programs/StarportGE/Starport GE.desktop). Their IDs are
   "u-" and the launcher's path inside that folder in hex, so any file name works. Only launchers shown in menus are
   listed (Wine's per-file-type ones, wine-extension-*, are not), and none with the name of one of Debian's own. */
#define UAPPS "/home/rudolf/.local/share/applications"   /* (inside Debian) */
/* the same folder as Rudolf OS sees it: the desktop's /home/rudolf, or the signed-in account's home (1.5.9) */
static const char *uapps_out(void) { static char b[400]; snprintf(b, sizeof b, "%s/.local/share/applications", out_home()); return b; }
static void hexid(const char *rel, char *out, size_t n) { size_t o = snprintf(out, n, "u-"); for (const char *c = rel; *c && o + 3 < n; c++) o += snprintf(out + o, n - o, "%02x", (unsigned char)*c); }
static int unhexid(const char *id, char *rel, size_t n)
{
	if (strncmp(id, "u-", 2)) return -1;
	id += 2; size_t l = strlen(id), o = 0; if (l % 2 || l / 2 >= n) return -1;
	for (size_t i = 0; i < l; i += 2) { unsigned v; if (sscanf(id + i, "%2x", &v) != 1 || !v) return -1; rel[o++] = (char)v; }
	rel[o] = 0; return strstr(rel, "..") || rel[0] == '/' ? -1 : 0;
}
static int app_out(const char *id, desk *k, int first)
{
	char ic[600]; int hi = find_icon(k->icon, ic, sizeof ic);
	printf("%s{\"id\":", first ? "" : ","); jstr(stdout, id);
	printf(",\"name\":"); jstr(stdout, k->name); printf(",\"wmclass\":"); jstr(stdout, k->cls);
	printf(",\"mime\":"); jstr(stdout, k->mime); printf(",\"categories\":"); jstr(stdout, k->cats);
	printf(",\"icon\":"); jstr(stdout, hi ? ic : ""); printf(",\"menu\":%s}", k->nodisp ? "false" : "true");
	return 0;
}
static char sysnames[256][160]; static int nsys;
static void user_apps(const char *dir, const char *rel, int depth, int *first)
{
	DIR *d = depth < 6 ? opendir(dir) : NULL; struct dirent *e;
	while (d && (e = readdir(d))) {
		if (e->d_name[0] == '.') continue;
		char p[900], r[700]; snprintf(p, sizeof p, "%s/%s", dir, e->d_name); snprintf(r, sizeof r, "%s%s%s", rel, *rel ? "/" : "", e->d_name);
		struct stat st; if (stat(p, &st)) continue;
		if (S_ISDIR(st.st_mode)) { user_apps(p, r, depth + 1, first); continue; }
		size_t l = strlen(r); if (l < 9 || strcmp(r + l - 8, ".desktop") || !strncmp(e->d_name, "wine-extension-", 15)) continue;
		desk k; if (read_desk(p, &k) || k.hidden || k.term || k.nodisp) continue;
		int dup = 0; for (int i = 0; i < nsys; i++) if (!strcmp(sysnames[i], k.name)) dup = 1; if (dup) continue;
		r[l - 8] = 0; char id[700]; hexid(r, id, sizeof id); if (strlen(id) > 200) continue;
		app_out(id, &k, *first); *first = 0;
	}
	if (d) closedir(d);
}
static int cmd_apps(void)
{
	who(); printf("{\"ok\":true,\"apps\":["); int first = 1;
	for (int di = 0; APPDIRS[di] && installed(); di++) {
	char dp[256]; snprintf(dp, sizeof dp, ROOT "%s", APPDIRS[di]);
	DIR *d = opendir(dp); struct dirent *e;
	while (d && (e = readdir(d))) {
		size_t l = strlen(e->d_name); if (l < 9 || strcmp(e->d_name + l - 8, ".desktop")) continue;
		char p[512]; snprintf(p, sizeof p, "%s/%s", dp, e->d_name);
		if (di) { char q[512]; snprintf(q, sizeof q, ROOT "/usr/share/applications/%s", e->d_name); if (!access(q, F_OK)) continue; }   /* the menu's own wins */
		desk k; if (read_desk(p, &k) || k.hidden || k.term) continue;
		/* 1.4.8: a launcher kept out of menus (NoDisplay) still opens its file types: Wine's "Windows Program
		   Loader" opens .exe and .msi files. It is listed with "menu":false (Open with only, not the Start menu). */
		if (k.nodisp && !k.mime[0]) continue;
		char id[256]; snprintf(id, sizeof id, "%.*s", (int)(l - 8), e->d_name);
		app_out(id, &k, first); first = 0;
		if (!k.nodisp && nsys < 256) snprintf(sysnames[nsys++], sizeof sysnames[0], "%s", k.name);
	}
	if (d) closedir(d);
	}
	if (installed()) user_apps(uapps_out(), "", 0, &first);
	printf("]}\n"); return 0;
}
/* (1.6.1, docs/plan-1.6.1.md Part 15, item 1) a Debian app that can use the system's title bar is asked to once, as Flatpak's are:
   rudolf-apps does it (its list, /usr/share/rudolf/titlebars, deb:LAUNCHER), as the person's own Linux user, in the person's own
   folder as Rudolf OS sees it (Debian's ~/.config is that folder's) */
static void titlebar_ask(const char *id)
{
	char key[200]; snprintf(key, sizeof key, "deb:%s", id);
	if (strlen(id) > 180 || access("/usr/bin/rudolf-apps", X_OK)) return;
	who(); const char *home = out_home(); if (!home || home[0] != '/') return;
	pid_t c = fork(); if (c < 0) return;
	if (!c) {
		if (setgroups(0, NULL) || setgid(1000) || setuid(d_uid) || !getuid()) _exit(1);
		int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 0); dup2(nul, 1); }
		setenv("HOME", home, 1);
		execl("/usr/bin/rudolf-apps", "rudolf-apps", "titlebar", key, home, (char *)NULL); _exit(127);
	}
	int st; waitpid(c, &st, 0);
}
/* 1.6.3 (the owner's Asus desktop, 2026-11-28: Starport: GE and Dead Frontier started through Wine both ended at once,
   exit 3, and nothing said what the container had to run them with): what a Windows program runs with, read when one ends
   at once, kept beside its log (RUND/ID.diag.txt) and said with the box once it is shown: Wine's version, its 64- and 32-bit
   GL and Vulkan packages, the renderer, Vulkan's device, /dev/dri, the room left, and this account's newest logs (never
   another account's). A game that ends at once is nearly always one of these. (LR_DIAG_CMD is a test's.) */
#ifndef LR_DIAG_CMD
#define LR_DIAG_CMD \
	"echo '# Wine'; wine --version 2>&1 | head -1; " \
	"echo '# Wine, GL and Vulkan, 64- and 32-bit'; " \
	"dpkg-query -W -f='${Package} ${Version} ${Architecture}\\n' 'wine*' 'libgl1*' 'libglx*' 'libegl*' 'mesa-vulkan-drivers*' 'libvulkan1*' 2>/dev/null | sort | head -20; " \
	"echo '# not installed cleanly'; dpkg --audit 2>&1 | head -5; " \
	"dpkg-query -W -f='${Package}: ${Status}\\n' 'wine32*' 'wine64*' 'libgl1-mesa-dri:i386' 'libglx-mesa0:i386' 'mesa-vulkan-drivers:i386' 2>/dev/null | grep -v 'install ok installed' | head -5; " \
	"echo '# the renderer'; if command -v glxinfo >/dev/null; then timeout 3 glxinfo -B 2>&1 | grep -Ei 'vendor string|renderer string|OpenGL version|OpenGL core profile version|Device:|Video memory' | head -8; else echo '(glxinfo is not installed: apt install mesa-utils)'; fi; " \
	"echo '# Vulkan'; if command -v vulkaninfo >/dev/null; then timeout 3 vulkaninfo --summary 2>&1 | grep -Ei 'apiVersion|driverName|deviceName|deviceType' | head -8; else echo '(vulkaninfo is not installed: apt install vulkan-tools)'; fi; " \
	"echo '# /dev/dri'; ls -l /dev/dri 2>&1 | head -6; " \
	"echo '# room'; df -h /home /tmp 2>&1 | head -4; " \
	"echo '# the newest logs'; find " RUND " -maxdepth 1 -name '*.log' -uid $(id -u) -printf '%T@ %p\\n' 2>/dev/null | sort -rn | head -3 | cut -d' ' -f2- | while read -r f; do echo \"--- $f\"; tail -12 \"$f\"; done"
#endif
#define LR_DIAG_MAX 100   /* (the box says them all; the file, every line) */
static char lr_diag[LR_DIAG_MAX][300]; static int lr_diag_n;
static void lr_read_diag(const char *keep_at)
{
	lr_diag_n = 0;
	FILE *p = popen(LR_DIAG_CMD, "r"); if (!p) return;
	FILE *k = keep_at && *keep_at ? fopen(keep_at, "w") : NULL;   /* (best effort: a check that cannot be kept is still said) */
	char l[400];
	while (fgets(l, sizeof l, p)) {
		size_t n = strlen(l); while (n && (l[n - 1] == '\n' || l[n - 1] == '\r')) l[--n] = 0;
		if (!*l) continue;
		if (k) fprintf(k, "%s\n", l);
		if (lr_diag_n < LR_DIAG_MAX) snprintf(lr_diag[lr_diag_n++], sizeof lr_diag[0], "%s", l);
	}
	if (k) fclose(k);
	pclose(p);
}
/* 1.6.2 (docs/plan-1.6.2.md Part 40, the owner's Vivobook on 2026-11-23: Starport GE's launcher could not find its folder, which
   was in the Wine prefix from before 1.5.9, and nothing at all was shown): a program that ends within LR_QUICK seconds of starting
   with an error (an exit status other than 0, or a signal) is said, as a request of the desktop portal's that waits for no
   answer ("lxfail-...", kind "lxfail": rudolfd's portal/poll hands it to the page, Chromium's and Lite's desktop page or the
   native desktop's hidden page, which shows the message, linux26.js and wineold62.js): the program's name, how it ended, the
   first real error in its log and the last lines (RUND/ID.log; Wine's "fixme:" lines left out, and its driver-service setup
   -- winebth and wineusb, and the scmdatabase line after them -- which fails on every launch of every Windows program and
   once filled this message alone, hiding the cause above it), whether it is a Windows program, what it runs with (Wine's
   version, its 64- and 32-bit GL and Vulkan packages, the renderer, /dev/dri, the room left, this account's newest logs; read
   after the request is written, so the box never waits for them, kept in RUND/ID.diag.txt, and sent again for the box to fill
   in), and a folder of the Wine prefix it names (its launcher's Path=, or the first /home/rudolf/.wine/... in those lines),
   which rudolfd checks against the prefix from before 1.5.9 (rudolfd-wineold.inc). One that runs longer, or ends with 0, is
   not said. */
#ifndef LR_DIR
#define LR_DIR "/run/rudolf/portal/req"
#endif
#define LR_QUICK 10
/* the request as rudolfd lists it (whole: only *.json): written first without what it runs with -- the box is shown at
   once -- and then a second one with it, once the readings are in (glxinfo and vulkaninfo can each take seconds). The second
   has an id of its own (the page never takes an id twice, lxPortalPoll's LX.seen) and says which request's box it fills
   ("of"); wineold62.js fills that box in rather than showing another. */
static void lr_request(FILE *f, const char *id, const desk *k, int status, double secs, int wine, const char *dir,
	const char *log, int nl, char (*lines)[300], const char *diaglog, const char *of)
{
	fprintf(f, "{\"kind\":\"lxfail\",\"app\":"); jstr(f, id); fprintf(f, ",\"name\":"); jstr(f, k->name);
	fprintf(f, ",\"status\":%d,\"secs\":%.1f,\"wine\":%s,\"dir\":", status, secs, wine ? "true" : "false"); jstr(f, dir);
	fprintf(f, ",\"log\":"); jstr(f, log); fprintf(f, ",\"lines\":[");
	for (int i = 0; i < nl; i++) { jstr(f, lines[i]); if (i + 1 < nl) fputc(',', f); }
	fputc(']', f);
	if (diaglog && *diaglog && lr_diag_n) {
		fprintf(f, ",\"diag\":[");
		for (int i = 0; i < lr_diag_n; i++) { jstr(f, lr_diag[i]); if (i + 1 < lr_diag_n) fputc(',', f); }
		fprintf(f, "],\"diaglog\":"); jstr(f, diaglog);
	}
	if (of && *of) { fprintf(f, ",\"fill\":true,\"of\":"); jstr(f, of); }
	fprintf(f, "}\n");
}
static int lr_send(const char *p, const char *tp, const char *id, const desk *k, int status, double secs, int wine,
	const char *dir, const char *log, int nl, char (*lines)[300], const char *diaglog, const char *of)
{
	FILE *f = fopen(tp, "w"); if (!f) return 1;
	lr_request(f, id, k, status, secs, wine, dir, log, nl, lines, diaglog, of);
	int bad = ferror(f); if (fclose(f) || bad) { unlink(tp); return 1; }
	if (rename(tp, p)) { unlink(tp); return 1; }
	return 0;
}
static void launch_report(const char *id, const desk *k, int status, double secs, const char *log)
{
	if (!status || secs > LR_QUICK) return;
	char *t = slurp(log, NULL); char lines[4][300]; int nl = 0; const char *wdir = NULL; char dir[400] = "";
	if (t) {
		/* the lines that say something: no "fixme:" (Wine's), and nothing of Wine's driver-service setup, which is the
		   same on every launch and says nothing about this program */
		static const char *const NOISE[] = { "winebth", "wineusb", "scmdatabase_autostart_services" };
		char *keep[3]; int nk = 0; char *first_err = NULL; int noise_seen = 0;
		for (char *p = t; *p;) {
			char *fe = strchr(p, '\n'); if (fe) *fe = 0;
			size_t n = strlen(p); while (n && (p[n - 1] == '\r' || p[n - 1] == ' ' || p[n - 1] == '\t')) p[--n] = 0;
			int noise = 0; for (unsigned q = 0; q < sizeof NOISE / sizeof *NOISE; q++) if (strstr(p, NOISE[q])) noise = 1;
			if (noise) noise_seen = 1;
			if (*p && strncmp(p, "fixme:", 6) && !strstr(p, ":fixme:") && !noise) {
				if (!first_err && (strstr(p, ":err:") || !strncmp(p, "err:", 4))) first_err = p;   /* (the cause, ahead of the noise) */
				keep[nk % 3] = p; nk++;
			}
			if (!fe) break;
			p = fe + 1;
		}
		/* the earliest real error first, then the last lines: three at most */
		int have = nk < 3 ? nk : 3;
		if (first_err) snprintf(lines[nl++], sizeof lines[0], "%s", first_err);
		for (int i = 0; i < have && nl < 3; i++) {
			char *s2 = keep[(nk - have + i) % 3];
			if (first_err && s2 == first_err) continue;
			snprintf(lines[nl++], sizeof lines[0], "%s", s2);
		}
		/* (a log of nothing but that setup: said as much; an empty log stays empty, for the page's own words) */
		if (!nl && noise_seen) snprintf(lines[nl++], sizeof lines[0], "%s",
			"(-- nothing in the log but Wine's driver and Bluetooth setup, which fails on every launch and harms nothing --)");
	}
	int wine = strstr(k->exec, "wine") || strstr(k->exec, ".exe") || strstr(k->path, "/.wine/");
	for (int i = 0; i < nl && !wine; i++) if (strstr(lines[i], "/.wine/") || strstr(lines[i], "wine:")) wine = 1;
	if (!strncmp(k->path, "/home/rudolf/.wine/", 19)) wdir = k->path;
	for (int i = nl - 1; i >= 0 && !wdir; i--) { char *w = strstr(lines[i], "/home/rudolf/.wine/"); if (w) wdir = w; }
	if (wdir) { snprintf(dir, sizeof dir, "%s", wdir); dir[strcspn(dir, "\"':\n")] = 0; size_t l = strlen(dir); while (l && (dir[l - 1] == ' ' || dir[l - 1] == '/')) dir[--l] = 0; if (strstr(dir, "..")) dir[0] = 0; }
	mkdirs("/run/rudolf/portal", 0755); if (!mkdir(LR_DIR, 01733)) chmod(LR_DIR, 01733);   /* (as rudolfd makes it: anyone may add, only the owner changes) */
	char base[80], p[300], tp[320];
	snprintf(base, sizeof base, "lxfail-%ld-%d", (long)time(NULL), (int)getpid());
	snprintf(p, sizeof p, "%s/%s.json", LR_DIR, base); snprintf(tp, sizeof tp, "%s.tmp", p);
	/* the box first: what it ended with and where its log is, said now */
	if (lr_send(p, tp, id, k, status, secs, wine, dir, log, nl, lines, NULL, NULL)) { free(t); return; }
	/* then what it runs with (a Windows program): read, kept beside the log, and said in a second request of its own,
	   which the page fills into the box it already shows */
	if (wine) {
		char diaglog[420] = ""; size_t l = strlen(log);
		if (l > 4 && !strcmp(log + l - 4, ".log")) snprintf(diaglog, sizeof diaglog, "%.*s.diag.txt", (int)(l - 4), log);
		else snprintf(diaglog, sizeof diaglog, "%s.diag.txt", log);
		if (!strstr(diaglog, "..")) lr_read_diag(diaglog);
		if (lr_diag_n) {
			char base2[92], p2[312], tp2[332];
			snprintf(base2, sizeof base2, "%s-diag", base);
			snprintf(p2, sizeof p2, "%s/%s.json", LR_DIR, base2); snprintf(tp2, sizeof tp2, "%s.tmp", p2);
			lr_send(p2, tp2, id, k, status, secs, wine, dir, log, nl, lines, diaglog, base);
		}
	}
	free(t);
}
static int cmd_launch(const char *id, const char *file)
{
	for (const char *c = id; *c; c++) if (!isalnum((unsigned char)*c) && !strchr("._-+", *c)) die("bad app id");
	char p[900]; desk k; int found = 0;
	if (!strncmp(id, "u-", 2)) { char rel[700]; if (unhexid(id, rel, sizeof rel)) die("bad app id"); who(); snprintf(p, sizeof p, "%s/%s.desktop", uapps_out(), rel); found = !read_desk(p, &k); }
	for (int di = 0; APPDIRS[di] && !found; di++) { snprintf(p, sizeof p, ROOT "%s/%s.desktop", APPDIRS[di], id); found = !read_desk(p, &k); }
	if (!found) die("no such app");
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
		/* (1.6.0: as it is, not copied: a link of the app's own scheme can be longer than a word here) */
		if (o[0] == '%' && o[1] && !o[2]) { if (strchr("fFuU", o[1]) && file && *file) argv[n++] = (char *)file; continue; }
		argv[n++] = o;
	}
	argv[n] = NULL; if (!n) die("the app has no command");
	titlebar_ask(id);
	mkdirs(RUND, 0755);
	char log[300]; snprintf(log, sizeof log, RUND "/%s.log", id);
	int lf = open(log, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	pid_t d = fork(); if (d) { printf("started\n"); return 0; }
	setsid();
	/* 1.4.9: in the launcher's own folder (Path=), as Windows programs' launchers and updaters expect */
	struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
	pid_t c = enter(0, k.path[0] == '/' && !strstr(k.path, "..") ? k.path : "/home/rudolf");
	if (!c) {
		int nul = open("/dev/null", O_RDWR); dup2(nul, 0); if (lf >= 0) { dup2(lf, 1); dup2(lf, 2); }
		execvp(argv[0], argv); fprintf(stderr, "%s: %s\n", argv[0], strerror(errno)); _exit(127);
	}
	int st; waitpid(c, &st, 0); clock_gettime(CLOCK_MONOTONIC, &t1);
	if (lf >= 0) close(lf);
	launch_report(id, &k, WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st), (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9, log);   /* (1.6.2 Part 40) */
	_exit(0);
}

/* ------------------------------------------------------------------ the account's folders (1.5.6) */
/* rudolfd binds the signed-in account's Documents, Pictures, Music, Videos (and Downloads) over /home/rudolf's at
   sign-in. Debian's mount namespace is its own (private, from when it started), so it does not see that: rebind puts
   the same folders there, through the new mount API (a copy of the folder made outside, attached inside), after
   taking off whatever was put there before. Run by rudolfd after its binds change, and when Debian starts. */
#ifndef SYS_open_tree
#define SYS_open_tree 428
#endif
#ifndef SYS_move_mount
#define SYS_move_mount 429
#endif
#define RB_OPEN_TREE_CLONE 1
#define RB_MOVE_MOUNT_F_EMPTY_PATH 0x4
static const char *const RB_NAMES[] = {"Documents", "Pictures", "Music", "Videos", "Downloads", NULL};
/* mounts stacked at path (as the keeper sees it) */
static int rb_count(const char *mi, const char *path)
{
	int n = 0; size_t pl = strlen(path);
	for (const char *l = mi; l && *l; ) {
		const char *t = l; for (int i = 0; i < 4 && t; i++) { t = strchr(t, ' '); if (t) t++; }
		if (t && !strncmp(t, path, pl) && t[pl] == ' ') n++;
		l = strchr(l, '\n'); if (l) l++;
	}
	return n;
}
/* 1.5.9 (15.3): Debian as the account's user: its folder is Debian's /home/rudolf, its session bus and the accounts'
   sound socket in Debian's /run/user/1000 (clones made here, as Rudolf OS's root, attached inside, as rebind does) */
static int acct_binds(pid_t k)
{
	char bus[340], snd[] = "/run/rudolf/pulse/native"; snprintf(bus, sizeof bus, "%s/bus", d_rt);
	const char *src[3] = {d_home, bus, snd}, *dst[3] = {"/home/rudolf", "/run/user/1000/bus", "/run/user/1000/pulse/native"};
	int fds[3];
	for (int i = 0; i < 3; i++) { struct stat st; fds[i] = lstat(src[i], &st) ? -1 : syscall(SYS_open_tree, AT_FDCWD, src[i], RB_OPEN_TREE_CLONE | O_CLOEXEC); }
	if (fds[0] < 0) { for (int i = 1; i < 3; i++) if (fds[i] >= 0) close(fds[i]); return -1; }
	char p[64]; snprintf(p, sizeof p, "/proc/%d/ns/mnt", k); int ns = open(p, O_RDONLY | O_CLOEXEC);
	if (ns < 0) { for (int i = 0; i < 3; i++) if (fds[i] >= 0) close(fds[i]); return -1; }
	pid_t c = fork(); if (c < 0) die("fork");
	if (!c) {
		if (setns(ns, CLONE_NEWNS)) _exit(1);
		for (int i = 0; i < 3; i++) if (fds[i] >= 0 && syscall(SYS_move_mount, fds[i], "", AT_FDCWD, dst[i], RB_MOVE_MOUNT_F_EMPTY_PATH)) {
			fprintf(stderr, "rudolf-debian: %s: %s\n", dst[i], strerror(errno)); if (!i) _exit(1); }
		_exit(0);
	}
	close(ns); for (int i = 0; i < 3; i++) if (fds[i] >= 0) close(fds[i]);
	int st; waitpid(c, &st, 0);
	if (!WIFEXITED(st) || WEXITSTATUS(st)) return -1;
	/* its shell's settings when it has none (as setup does for the desktop's) */
	char *a2[] = {"/bin/sh", "-c", "for f in .bashrc .profile .bash_logout; do [ -e \"$HOME/$f\" ] || [ ! -e /etc/skel/$f ] || cp /etc/skel/$f \"$HOME/\"; done", NULL};
	run_in(0, NULL, a2);
	return 0;
}
static int cmd_rebind(void)
{
	pid_t k = keeper_pid(); if (!k) return 0;
	/* 1.5.9 (15.3): signed in or out as another user: Debian stops (its next command starts it for the new one) */
	who(); if (keeper_uid() != d_uid) { stop(); return 0; }
	if (acct_mode()) return 0;   /* (the account's whole folder is Debian's home: nothing to place) */
	char *acct = slurp("/run/rudolf/home-session", NULL); if (acct) { char *e = strpbrk(acct, "\r\n"); if (e) *e = 0; }
	for (const char *c = acct; c && *c; c++) if (!isalnum((unsigned char)*c) && *c != '-' && *c != '_') { free(acct); acct = NULL; break; }
	/* Downloads is the account's own only while the downloads store has its bind (rudolfd: dl_store_bound) */
	char *hm = slurp("/proc/self/mountinfo", NULL); int dlb = hm && strstr(hm, " /run/rudolf/dl "); free(hm);
	char p[64]; snprintf(p, sizeof p, "/proc/%d/mountinfo", k); char *mi = slurp(p, NULL); if (!mi) die("cannot read Debian's mounts");
	int fds[8], base[8], cnt[8]; char tgt[8][320];
	for (int i = 0; RB_NAMES[i]; i++) {
		char h[200]; snprintf(h, sizeof h, "/home/rudolf/%s", RB_NAMES[i]);
		char l[300]; ssize_t n = readlink(h, l, sizeof l - 1);
		if (n > 0 && l[0] == '/') { l[n] = 0; snprintf(tgt[i], sizeof tgt[i], "%s", l); base[i] = 1; }   /* the link's folder, placed at start */
		else { snprintf(tgt[i], sizeof tgt[i], "%s", h); base[i] = 0; }
		cnt[i] = rb_count(mi, tgt[i]);
		fds[i] = -1;
		if (acct && *acct && (strcmp(RB_NAMES[i], "Downloads") || dlb)) {
			char src[300]; snprintf(src, sizeof src, "/persist/accounts/%s/%s", acct, RB_NAMES[i]);
			struct stat st; if (!lstat(src, &st) && S_ISDIR(st.st_mode)) fds[i] = syscall(SYS_open_tree, AT_FDCWD, src, RB_OPEN_TREE_CLONE | O_CLOEXEC);
		}
	}
	free(mi); free(acct);
	snprintf(p, sizeof p, "/proc/%d/ns/mnt", k); int ns = open(p, O_RDONLY | O_CLOEXEC); if (ns < 0) die("cannot reach the sandbox");
	pid_t c = fork(); if (c < 0) die("fork");
	if (!c) {
		if (setns(ns, CLONE_NEWNS)) { fprintf(stderr, "rudolf-debian: rebind: cannot join (%s)\n", strerror(errno)); _exit(1); }
		int bad = 0;
		for (int i = 0; RB_NAMES[i]; i++) {
			for (int j = cnt[i]; j > base[i]; j--) if (umount2(tgt[i], MNT_DETACH)) break;
			if (fds[i] >= 0 && syscall(SYS_move_mount, fds[i], "", AT_FDCWD, tgt[i], RB_MOVE_MOUNT_F_EMPTY_PATH)) { fprintf(stderr, "rudolf-debian: rebind %s: %s\n", RB_NAMES[i], strerror(errno)); bad = 1; }
		}
		_exit(bad);
	}
	close(ns); for (int i = 0; RB_NAMES[i]; i++) if (fds[i] >= 0) close(fds[i]);
	int st; waitpid(c, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
}

/* ------------------------------------------------------------------ devices plugged in later (1.5.7) */
/* a camera or controller plugged in after Debian started is bound into its /dev (the same way rebind places the
   account's folders); one that is gone is taken off. Run by mdev for each such device (mdev.conf). */
static int cmd_devices(void)
{
	pid_t k = keeper_pid(); if (!k) return 0;
	char host[64][64], in[64][64]; int nh = 0, ni = 0;
	DIR *d = opendir("/dev"); struct dirent *e;
	while (d && (e = readdir(d)) && nh < 64) { struct stat st; char p[300]; snprintf(p, sizeof p, "/dev/%s", e->d_name);
		if (dev_wanted(e->d_name) && !stat(p, &st) && S_ISCHR(st.st_mode)) snprintf(host[nh++], 64, "%s", e->d_name); }
	if (d) closedir(d);
	char kd[64]; snprintf(kd, sizeof kd, "/proc/%d/root/dev", k);
	d = opendir(kd); while (d && (e = readdir(d)) && ni < 64) if (dev_wanted(e->d_name)) snprintf(in[ni++], 64, "%s", e->d_name);
	if (d) closedir(d);
	int fds[64], add[64], na = 0, del[64], nd = 0;
	for (int i = 0; i < nh; i++) { int have = 0; for (int j = 0; j < ni; j++) if (!strcmp(host[i], in[j])) have = 1;
		if (have) continue; char p[300]; snprintf(p, sizeof p, "/dev/%s", host[i]);
		int fd = syscall(SYS_open_tree, AT_FDCWD, p, RB_OPEN_TREE_CLONE | O_CLOEXEC); if (fd >= 0) { fds[na] = fd; add[na++] = i; } }
	for (int j = 0; j < ni; j++) { int there = 0; for (int i = 0; i < nh; i++) if (!strcmp(host[i], in[j])) there = 1; if (!there) del[nd++] = j; }
	if (!na && !nd) return 0;
	char p[64]; snprintf(p, sizeof p, "/proc/%d/ns/mnt", k); int ns = open(p, O_RDONLY | O_CLOEXEC); if (ns < 0) die("cannot reach the sandbox");
	pid_t c = fork(); if (c < 0) die("fork");
	if (!c) {
		if (setns(ns, CLONE_NEWNS)) _exit(1);
		int bad = 0;
		for (int j = 0; j < nd; j++) { char t[100]; snprintf(t, sizeof t, "/dev/%s", in[del[j]]); umount2(t, MNT_DETACH); unlink(t); }
		for (int a = 0; a < na; a++) { char t[100]; snprintf(t, sizeof t, "/dev/%s", host[add[a]]);
			int f = open(t, O_WRONLY | O_CREAT | O_CLOEXEC, 0600); if (f >= 0) close(f);
			if (syscall(SYS_move_mount, fds[a], "", AT_FDCWD, t, RB_MOVE_MOUNT_F_EMPTY_PATH)) { fprintf(stderr, "rudolf-debian: devices %s: %s\n", t, strerror(errno)); bad = 1; } }
		_exit(bad);
	}
	close(ns); for (int a = 0; a < na; a++) close(fds[a]);
	int st; waitpid(c, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
}

/* ------------------------------------------------------------------ programs that need no installing (1.5.7) */
/* .AppImage (one file with the program and what it needs; run extracted, as Debian has no FUSE) and .bin or .run
   (a program, or a self-extracting installer: a shell script). The file is the person's, in their folders. */
static int file_ok(const char *f)
{
	return f && !strncmp(f, "/home/rudolf/", 13) && !strstr(f, "/../") && !strstr(f, "/./") && strlen(f) < 900 && !strpbrk(f, "\n\r");
}
static int cmd_runfile(const char *kind, const char *file)
{
	int appimage = !strcmp(kind, "appimage"); if (!appimage && strcmp(kind, "bin")) die("runfile appimage|bin FILE");
	if (!file_ok(file)) die("Linux cannot reach this file: move it into your Downloads or Documents");
	char dir[900]; snprintf(dir, sizeof dir, "%s", file); char *sl = strrchr(dir, '/'); if (sl) *sl = 0;
	mkdirs(RUND, 0755);
	int lf = open(RUND "/runfile.log", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	pid_t d = fork(); if (d) { printf("started\n"); return 0; }
	setsid();
	pid_t c = enter(0, dir);
	if (!c) {
		int nul = open("/dev/null", O_RDWR); dup2(nul, 0); if (lf >= 0) { dup2(lf, 1); dup2(lf, 2); }
		struct stat st; if (stat(file, &st) || !S_ISREG(st.st_mode)) { fprintf(stderr, "%s: not found\n", file); _exit(127); }
		if (!(st.st_mode & S_IXUSR) && chmod(file, st.st_mode | S_IXUSR)) fprintf(stderr, "%s: could not be made runnable (%s)\n", file, strerror(errno));
		if (appimage) { setenv("APPIMAGE_EXTRACT_AND_RUN", "1", 1); setenv("NO_CLEANUP", "1", 1); }
		/* a .bin that is neither a program (ELF) nor a script (#!) is a shell script without its first line */
		char h[4] = {0}; int f = open(file, O_RDONLY | O_CLOEXEC); if (f >= 0) { if (read(f, h, 4) < 0) h[0] = 0; close(f); }
		if (!appimage && memcmp(h, "\177ELF", 4) && memcmp(h, "#!", 2)) execlp("sh", "sh", file, (char *)NULL);
		else execl(file, file, (char *)NULL);
		fprintf(stderr, "%s: %s\n", file, strerror(errno)); _exit(127);
	}
	int st; waitpid(c, &st, 0); _exit(0);
}
static void exec_quote(const char *file, char *q, size_t n)
{
	size_t o = 0; q[o++] = '"'; for (const char *c = file; *c && o < n - 3; c++) { if (*c == '"' || *c == '\\' || *c == '`' || *c == '$') q[o++] = '\\'; q[o++] = *c; } q[o++] = '"'; q[o] = 0;
}
static int cmd_keepfile(const char *file, const char *name)
{
	if (!file_ok(file)) die("Linux cannot reach this file: move it into your Downloads or Documents");
	char nm[120]; size_t o = 0; for (const char *c = name ? name : ""; *c && o < sizeof nm - 1; c++) if ((unsigned char)*c >= 32 && *c != '\\') nm[o++] = *c; nm[o] = 0;
	if (!*nm) die("no name");
	const char *bn = strrchr(file, '/') + 1; size_t bl = strlen(bn); int appimage = bl > 9 && !strcasecmp(bn + bl - 9, ".AppImage");
	/* Exec: the path in double quotes, with " and \\ escaped (the launcher's own rules) */
	char q[2000]; exec_quote(file, q, sizeof q);
	char body[3000]; snprintf(body, sizeof body, "[Desktop Entry]\nType=Application\nName=%s\nExec=%s%s\nTerminal=false\nCategories=Utility;\nComment=Kept by Rudolf OS from %s\n", nm, appimage ? "env APPIMAGE_EXTRACT_AND_RUN=1 NO_CLEANUP=1 " : "", q, bn);
	pid_t c = enter(0, "/home/rudolf");
	if (!c) {
		struct stat st; if (!stat(file, &st) && !(st.st_mode & S_IXUSR)) if (chmod(file, st.st_mode | S_IXUSR)) {}
		mkdirs(UAPPS "/rudolf-kept", 0755);
		char lp[400]; unsigned h = 5381; for (const char *x = file; *x; x++) h = h * 33 + (unsigned char)*x;
		snprintf(lp, sizeof lp, UAPPS "/rudolf-kept/%08x.desktop", h);
		if (write_str(lp, body)) { fprintf(stderr, "rudolf-debian: the launcher could not be written\n"); _exit(1); }
		printf("kept\n"); _exit(0);
	}
	int st; waitpid(c, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
}

/* ------------------------------------------------------------------ status, update, remove */
static long long du(const char *path)
{
	char cmd[300]; snprintf(cmd, sizeof cmd, "du -sk %s 2>/dev/null", path);
	FILE *p = popen(cmd, "r"); long long k = 0; if (p) { if (fscanf(p, "%lld", &k) != 1) k = 0; pclose(p); } return k * 1024;
}
#ifndef RUDOLF_DEBIAN_NO_MAIN   /* (a test builds run_only alone: iso/qt/host/blzrun161_t.sh) */
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
		printf("wine=%d\n", installed() && !access(ROOT "/usr/bin/wine-stable", X_OK));   /* 1.4.9 */
		return 0;
	}
	if (!strcmp(c, "setup")) { if (argc < 3) die("setup PASSWORD-FILE"); return cmd_setup(argv[2]); }
	if (!strcmp(c, "start")) { printf("%d\n", start()); return 0; }
	if (!strcmp(c, "rebind")) return cmd_rebind();
	/* 1.5.6: installing a downloaded package (the installation wizard). The file is one of the person's own
	   (/home/rudolf/...), checked by rudolfd too; it is copied inside first, so that apt reads a file of its own. */
	if (!strcmp(c, "debinfo") || !strcmp(c, "deb")) {
		if (argc < 3) die("%s FILE.deb", c);
		const char *f = argv[2]; size_t l = strlen(f);
		if (strncmp(f, "/home/rudolf/", 13) || strstr(f, "/../") || l < 5 || strcasecmp(f + l - 4, ".deb")) die("not a package in your folders");
		if (!installed()) die("Debian is not set up");
		if (!strcmp(c, "debinfo")) {
			char *a[] = {"dpkg-deb", "-f", (char *)f, "Package", "Version", "Architecture", "Maintainer", "Installed-Size", "Description", NULL};
			char *ra[] = {"--", a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], NULL};
			return cmd_run(10, ra);
		}
		char *a[] = {"/bin/sh", "-c", "export DEBIAN_FRONTEND=noninteractive; f=/tmp/rudolf-install.deb; cp \"$1\" $f && chmod 644 $f || exit 3; "
			"p=$(dpkg-deb -f $f Package); echo 'step Getting the list of packages'; apt-get update; echo 'step Installing'; apt-get install -y $f; r=$?; rm -f $f; "
			/* 1.5.6: a package whose installation adds its maker's own package source (Steam's adds Valve's) recommends
			   packages from there, which could not be found until now: they are added in a second pass, which may fail
			   without failing the installation */
			"if [ $r = 0 ] && [ -n \"$p\" ]; then echo 'step Adding what it recommends'; apt-get update >/dev/null 2>&1; apt-get install -y --install-recommends \"$p\" || echo 'note: some recommended packages could not be added'; fi; "
			"apt-get clean; exit $r", "sh", (char *)f, NULL};
		int r = run_in(1, NULL, a); write_str(RUND "/apps-changed", "1");
		if (!r) say("done installed"); return r;
	}
	if (!strcmp(c, "stop")) { stop(); return 0; }
	if (!strcmp(c, "run")) return cmd_run(argc - 2, argv + 2);
	if (!strcmp(c, "pty")) { if (argc < 3) die("pty ID"); return cmd_pty(argv[2], argc > 3 && !strcmp(argv[3], "--root")); }
	if (!strcmp(c, "apps")) return cmd_apps();
	if (!strcmp(c, "launch")) { if (argc < 3) die("launch APP-ID [FILE]"); return cmd_launch(argv[2], argc > 3 ? argv[3] : NULL); }
	if (!strcmp(c, "runfile")) { if (argc < 4) die("runfile appimage|bin FILE"); return cmd_runfile(argv[2], argv[3]); }
	if (!strcmp(c, "keepfile")) { if (argc < 4) die("keepfile FILE NAME"); return cmd_keepfile(argv[2], argv[3]); }
	if (!strcmp(c, "devices")) return cmd_devices();
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
	/* 1.4.9: Wine, to run Windows programs (Settings > Apps > Linux > Install Wine): the 32- and 64-bit parts, as
	   Windows programs need both */
	if (!strcmp(c, "wine")) {
		if (!installed()) die("Debian is not set up");
		char *a[] = {"/bin/sh", "-c", "export DEBIAN_FRONTEND=noninteractive; dpkg --add-architecture i386 && apt-get update && apt-get install -y --install-recommends wine wine32:i386 wine64 && apt-get clean", NULL};
		int r = run_in(1, NULL, a); write_str(RUND "/apps-changed", "1");
		if (!r) say("done Wine is installed");
		return r;
	}
	if (!strcmp(c, "remove")) {
		stop();
		/* the home folder is Rudolf OS's own and is never inside ROOT on disk (it is mounted over at start) */
		if (system("rm -rf " ROOT " " DL)) die("could not delete everything");
		say("Debian was removed"); return 0;
	}
	die("unknown command %s", c);
}
#endif
