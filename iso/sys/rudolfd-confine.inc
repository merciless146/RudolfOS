/* (included by rudolfd.c, not built on its own: hence .inc) ---------- 1.6.1 (docs/plan-1.6.1.md Part 20): rudolfd
 * confined.
 * rudolfd runs as root and parses every request of the desktop's page and API: a parsing bug in it is a bug with root.
 * Part 11 moves its root work into rudolf-rootd's helpers; this puts what is left in the kernel's own confinement
 * (rudolf-confine.h: seccomp, Landlock, capabilities), behind a switch while it is new:
 *
 *   off  (the default): nothing changes.
 *   log  everything below is decided as with "on", but a system call the narrow list does not have is only written to
 *        the kernel's audit log (rudolfd puts it in the health log: Security), Landlock and the capabilities' cut are
 *        left out (they cannot only log), and a request a window does not make is answered and noted. For a beta's
 *        first run: what "on" would refuse, without refusing it.
 *   on   refused (EPERM), and said in the health log.
 * Chosen by, the last one given winning: the build (RUDOLFD_CONFINE, "off"); the file /persist/etc/rudolf-confine
 * (one word; root's, so only an administrator's change from outside rudolfd or the owner's own beta sets it); the
 * start-up option rudolf.confine=WORD; RUDOLFD_CONFINE=WORD in the environment (the host tests).
 *
 * Two layers, because everything the kernel's confinement does is inherited by every program the process starts,
 * and rudolfd still starts many (the shell's commands, the account's apps, Chromium's windows, the Debian container):
 *   1. All of rudolfd, from its start (confine_boot): only what nothing it starts can need. The capabilities no program
 *      of Rudolf OS uses are gone from the bounding set (CONFINE_TREE_DROP: the audit system's, MAC policies', process
 *      accounting, BPF, performance counters, checkpoint/restore), and a deny list of system calls (CONFINE_TREE_DENY:
 *      kexec, BPF, performance counters, file handles opened by number, raw I/O ports, process accounting, quotas,
 *      writing another process's memory, the obsolete uselib and nfsservctl). Programs started through rudolf-rootd
 *      are rudolf-rootd's children, not rudolfd's: they have their own, per helper (rudolf-rootd.c).
 *   2. Each request that starts nothing (confine_request): every connection is its own process (main's fork), so once
 *      the request's path is read (before its body, its JSON or anything it asks for), a request that runs no program,
 *      mounts nothing and signals no other process (rudolfd-confine-pure.h, read from the source by
 *      iso/systools/confinepure.py) is put in the narrow confinement for the rest of its life:
 *        - seccomp: only the calls rudolfd itself makes (CONFINE_NARROW, traced from the host tests and the QEMU
 *          checks: docs/plan-1.6.1.md Part 20); never exec, mount, ptrace, kill, setns, unshare, chroot, reboot;
 *        - Landlock (when the kernel has it): read anywhere, as root reads the system today; write, make and remove only
 *          in rudolfd's own folders (CONFINE_WRITE); run nothing; no TCP connection or listening socket of its own;
 *        - capabilities cut to what such a request needs (CONFINE_NARROW_KEEP: owners and modes of the shared files,
 *          reading the accounts' folders, the peer check's /proc/PID/fd, the dropped file's child becoming the account's
 *          user); no_new_privs.
 *      The page and its files (/, /index.html, /dlf/, /pkg/, everything under www/) are always narrow.
 *      Requests that reach root through rudolf-rootd's helpers only (CONFINE_ROOTD) are narrow while rudolf-rootd is
 *      there. As Part 11 moves an area, its requests stop starting programs and join the narrow list (the host test
 *      says so); once rudolfd runs as uid 1000 (Part 11's drop), the same layers apply unchanged, with no capabilities.
 * And the page's API is checked against what each window asks for (confine_window): the process holding the
 * connection (the peer check's, never anything the request says) is the native shell (rudolf-native), or the page
 * (Standard's Chromium; the native desktop's app windows and Lite, both WebKit's network process); a path that window's
 * code never asks for (rudolfd-winapi.h, iso/systools/winapi.py) is refused (403) and said in the health log. Root
 * (rudolfd's own helpers, the session's scripts) is never checked: it could do it all anyway.
 * What this does not cover: the requests that still start programs (most of the API until Part 11 moves them) are
 * only in layer 1; a request that is narrow can still ask rudolf-rootd's helpers for what they do (their own checks
 * hold); reading is not narrowed (rudolfd as root reads the accounts' folders and every process's information).
 */
#include "rudolf-confine.h"
#include "rudolfd-confine-pure.h"
#include "rudolfd-winapi.h"
#ifndef RUDOLFD_CONFINE
#define RUDOLFD_CONFINE "off"
#endif
#ifndef CONFINE_FILE
#define CONFINE_FILE PERSIST "/etc/rudolf-confine"
#endif
static int cf_mode = CF_OFF, cf_ll_abi = 0;
static int cf_narrow_on = 0;   /* (this connection's process is in the narrow confinement) */
static void confine_note(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#include "rudolfd-confine-tables.h"   /* what each layer allows: the lists */
#include "rudolfd-confine-narrow.h"   /* CONFINE_NARROW: the narrow layer's system calls (traced; the list and where each was seen) */

/* the mode for this start (see the top) */
static int confine_mode_read(void) { return cf_mode_read(RUDOLFD_CONFINE, CONFINE_FILE); }

/* at rudolfd's start, before it starts anything: layer 1, and said once in the health log */
static void confine_boot(void)
{
	cf_mode = confine_mode_read();
	if (cf_mode == CF_OFF) return;
	cf_ll_abi = cf_landlock_abi();
	int nd = 0, nd32 = 0; while (CONFINE_TREE_DENY[nd] >= 0) nd++; while (CONFINE_TREE_DENY32[nd32] >= 0) nd32++;
	const char *capw = "", *scw = "";
	if (cf_mode == CF_ON) {
		uint64_t keep = ~CONFINE_TREE_DROP;
		if (cf_caps_bound(keep)) capw = " (the capabilities could not all be dropped)";
	}
	if (cf_seccomp(CONFINE_TREE_DENY, nd, CONFINE_TREE_DENY32, nd32, 0, cf_mode)) scw = " (the system-call filter could not be set)";
	fprintf(stderr, "rudolfd: confinement %s (Landlock ABI %d)%s%s\n", cf_mode_name(cf_mode), cf_ll_abi, capw, scw);
	confine_note("rudolfd %d: confinement %s, Landlock ABI %d%s%s", (int)getpid(), cf_mode_name(cf_mode), cf_ll_abi, capw, scw);
	health_add("security", "rudolfd's confinement is %s: %s%s%s; the requests that start nothing %s%s", cf_mode_name(cf_mode),
		cf_mode == CF_ON ? "system calls nothing needs refused, unused capabilities dropped" : "system calls nothing needs only logged",
		capw, scw, cf_mode == CF_ON ? "narrowly confined" : "logged against the narrow list",
		cf_ll_abi ? "" : " (this kernel has no Landlock: the files they may write are not limited)");
}

/* is this request (the path after /api/, or a page file when api is 0) narrow? */
static int cf_match(const char *const *L, const char *p)
{
	for (int i = 0; L[i]; i++) {
		size_t l = strlen(L[i]);
		if (l && L[i][l - 1] == '*') { if (!strncmp(p, L[i], l - 1)) return 1; }
		else if (!strcmp(p, L[i])) return 1;
	}
	return 0;
}
static int confine_is_narrow(const char *path)
{
	if (strncmp(path, "/api/", 5)) return CONFINE_STATIC;
	const char *p = path + 5;
	if (cf_match(CONFINE_START, p)) return 0;
	if (cf_match(CONFINE_PURE, p)) return 1;
	return root_fd >= 0 && cf_match(CONFINE_ROOTD, p);
}
/* in a connection's process, as soon as its path is read. 0 go on; -1 the confinement failed with "on" (the
   request is refused: nothing unconfined is done for one that should be) */
static int cf_kind = -1;   /* (the window holding this connection, found before the confinement: confine_window_kind) */
static int confine_window_kind(void);
static int confine_request(int fd, const char *path)
{
	if (cf_mode == CF_OFF || cf_narrow_on || !confine_is_narrow(path)) return 0;
	/* the peer check reads the desktop's processes' /proc/PID/fd and /proc/PID/exe, which a Landlock domain may not
	   (as ptrace): done now, its answer kept for this connection (peer_desktop keeps it), and the window found */
	if (!strncmp(path, "/api/", 5) || !strcmp(path, "/") || !strcmp(path, "/index.html")) { peer_desktop(fd); cf_kind = confine_window_kind(); }
	cf_narrow_on = 1;
	int n = 0; while (CONFINE_NARROW[n] >= 0) n++;
	if (cf_mode == CF_LOG) { if (cf_seccomp(CONFINE_NARROW, n, NULL, 0, 1, CF_LOG)) {} return 0; }
	if (!prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0) && prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) return -1;
	if (cf_ll_abi > 0) {
		const char *pre = getenv("RUDOLFD_CONFINE_ROOT");   /* (the host tests: the folders under a test folder) */
		if (cf_landlock(CONFINE_WRITE, (int)(sizeof CONFINE_WRITE / sizeof *CONFINE_WRITE), pre ? pre : "", CF_SCOPE_SIGNAL | CF_SCOPE_ABSTRACT, 1, NULL) < 0) return -1;
	}
	if (cf_caps(CONFINE_NARROW_KEEP) && geteuid() == 0) return -1;
	return cf_seccomp(CONFINE_NARROW, n, NULL, 0, 1, CF_ON) ? -1 : 0;
}

/* ---- the windows' lists ---- */
/* which window holds this connection: 0 root or unknown (not checked), 1 the page, 2 the native shell */
static int confine_window_kind(void)
{
	if (peer_uid_seen == 0 || peer_pid_seen <= 0) return 0;
	char p[64], e[PATH_MAX]; snprintf(p, sizeof p, "/proc/%d/exe", peer_pid_seen);
	ssize_t k = readlink(p, e, sizeof e - 1); if (k <= 0) return 0;
	e[k] = 0; const char *b = strrchr(e, '/'); b = b ? b + 1 : e;
	if (!strcmp(b, "rudolf-native")) return 2;
	return 1;   /* (Chromium's network process on Standard, WebKit's for the native desktop's windows and Lite) */
}
/* 1 answer it; 0 refused (answered here) */
static int confine_window(struct req *r)
{
	if (cf_mode == CF_OFF) return 1;
	int k = cf_kind >= 0 ? cf_kind : confine_window_kind(); if (!k) return 1;
	const char *p = r->path + 5;
	if (cf_match(k == 2 ? WINAPI_SHELL : WINAPI_PAGE, p)) return 1;
	static const char T[] = RUN "/winapi-refused"; struct stat st; time_t now = time(NULL);
	if (stat(T, &st) || now - st.st_mtime >= 10) {   /* (said at most every 10 s) */
		int w = open(T, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600); if (w >= 0) close(w);
		char safe[80]; size_t i = 0; for (; p[i] && i < sizeof safe - 1; i++) safe[i] = isalnum((unsigned char)p[i]) || strchr("/_.-", p[i]) ? p[i] : '?';
		safe[i] = 0;
		health_add("security", "%s asked for %s, which it never asks for: %s", k == 2 ? "the native shell" : "the desktop's page", safe, cf_mode == CF_ON ? "refused" : "answered (confinement: log)");
	}
	{ char safe[80]; size_t i = 0; for (; p[i] && i < sizeof safe - 1; i++) safe[i] = isalnum((unsigned char)p[i]) || strchr("/_.-", p[i]) ? p[i] : '?'; safe[i] = 0;
	  confine_note("%s asked for %s: %s", k == 2 ? "the native shell" : "the page", safe, cf_mode == CF_ON ? "refused" : "answered (log)"); }
	if (cf_mode == CF_LOG) return 1;
	reply_err(r->fd, 403, "this window does not use that part of the API");
	return 0;
}

/* every line also in a log of its own, never trimmed (the health log keeps only its newest): PERSIST/var/log/confine.log
   (RUN/confine.log in a live start), up to 256 KB: what a beta's "log" run found, for the owner and the QEMU check */
static void confine_note(const char *fmt, ...)
{
	char p[200]; snprintf(p, sizeof p, "%s", access(RUN "/persistent", F_OK) ? RUN "/confine.log" : PERSIST "/var/log/confine.log");
	struct stat st; if (!stat(p, &st) && st.st_size > (256 << 10)) return;
	int f = open(p, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC | O_NOFOLLOW, 0600); if (f < 0) return;
	char b[600]; time_t t = time(NULL); struct tm tm; gmtime_r(&t, &tm); size_t n = strftime(b, sizeof b, "%Y-%m-%dT%H:%M:%SZ ", &tm);
	va_list a; va_start(a, fmt); int k = vsnprintf(b + n, sizeof b - n - 1, fmt, a); va_end(a);
	if (k > 0) { n += (size_t)k < sizeof b - n - 1 ? (size_t)k : sizeof b - n - 2; b[n++] = '\n'; if (write(f, b, n) < 0) {} }
	close(f);
}
/* the kernel's audit lines for the system calls refused or logged (type=1326) of rudolfd and the programs it starts,
   into the health log: each program and call once per start (up to 40; then a count), so a beta's first run in "log"
   mode lists what "on" would refuse */
static void confine_tick(void)
{
	static int kf = -2, said, more; static char seen[40][48];
	if (cf_mode == CF_OFF) return;
	if (kf == -2) { kf = open("/dev/kmsg", O_RDONLY | O_NONBLOCK | O_CLOEXEC); if (kf >= 0) lseek(kf, 0, SEEK_END); }
	if (kf < 0) return;
	char b[1024]; ssize_t n;
	for (int guard = 0; guard < 500 && (n = read(kf, b, sizeof b - 1)) != 0; guard++) {
		if (n < 0) { if (errno == EPIPE) continue; break; }   /* (EPIPE: lines lost to the ring's turning over) */
		b[n] = 0;
		if (!strstr(b, "type=1326")) continue;
		const char *c = strstr(b, " comm=\""), *x = strstr(b, " syscall="), *a = strstr(b, " arch=");
		if (!c || !x) continue;
		char comm[24] = "", key[48]; if (sscanf(c + 7, "%23[^\"]", comm) != 1) continue;
		for (char *q = comm; *q; q++) if (!isalnum((unsigned char)*q) && !strchr("-_.", *q)) *q = '?';
		int nr = atoi(x + 9); unsigned arch = a ? (unsigned)strtoul(a + 6, NULL, 16) : 0;
		snprintf(key, sizeof key, "%s %d%s", comm, nr, arch == 0x40000003u ? " (32-bit)" : "");
		int k = 0; for (; k < said && strcmp(seen[k], key); k++) {}
		if (k < said) continue;
		if (said < 40) {
			snprintf(seen[said++], sizeof seen[0], "%s", key);
			confine_note("%s system call %d%s of %s (%s)", cf_mode == CF_ON ? "refused" : "logged", nr, arch == 0x40000003u ? " (32-bit)" : "", comm, b);
			health_add("security", "the confinement %s system call %d%s of %s", cf_mode == CF_ON ? "refused" : "logged (it would refuse)", nr, arch == 0x40000003u ? " (32-bit)" : "", comm);
		} else if (!more++) health_add("security", "the confinement %s more system calls than it lists (see the kernel's log)", cf_mode == CF_ON ? "refused" : "logged");
	}
}
