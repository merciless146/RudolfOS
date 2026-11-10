/*
 * rudolf-memguard - when memory runs out, end the largest web page instead of letting the whole desktop freeze (1.4.7).
 *
 * Rudolf OS 1.4.6 had no swap. A 1.7 GB game tab on a 3.4 GB laptop used up the memory; the kernel then kept dropping
 * and rereading the system's own files, the graphics process and the desktop stalled in the kernel for a minute, and
 * the session's watchdog restarted the browser (every window and tab gone). Now /usr/lib/rudolf/memory gives the
 * system compressed swap in memory (zram), and this guard watches, once a second and at once when the kernel reports
 * a memory stall (a PSI trigger):
 *   - critical: little memory left (MemAvailable under 5% of the memory, or under 10% for 2 s while the system stalls
 *     on memory), and the swap (if any) nearly full; or the system stalled on memory (PSI "full") over 30% of the
 *     last 10 s for 3 s in a row with under 15% left; or (1.4.8) the kernel reported stalls twice within 2.5 s
 *     with under 8% left, swap or not
 *   - then it ends (SIGKILL) the renderer process of the largest web page: a tab in Oracle (a Controlled Frame), or a
 *     page in the browser window for sign-in sites, or one no browser holds any more. Never the desktop page itself
 *     (it says which process it is: /run/rudolf/desktop-renderer), nor Chromium's other processes, the graphics
 *     process or the system. Pages under 200 MiB are left alone too.
 *     Oracle then shows "This page was closed because it used too much memory" with a Reload button.
 *   - after ending one it waits 8 s (the memory takes a moment to come back) before it looks again
 *   - 1.5.6: when there is no such page, an app with no window open (Discord in the tray, found on the Vivobook: a
 *     game froze and the desktop was restarted while the guard had "no web page to end"). The largest Flatpak app
 *     (all its sandboxes, from /run/user/N/.flatpak) of over 200 MiB whose windows are none of those listed in
 *     /run/rudolf/windows: a window of class "steam_app_N" (a Steam game) counts as Steam's. It is asked to quit
 *     (SIGTERM), and ended 3 s later if it has not. The app in front, and every app with a window open, are left alone.
 * It also tells the kernel which processes to end last if the kernel has to step in itself (oom_score_adj): the
 * desktop's browser, its graphics and network processes, Xorg, rudolf-display, rudolfd, rudolf-launch and this guard.
 *
 * Files (in /run/rudolf):
 *   memguard.log       what it did (also copied to the session log)
 *   memguard.events    one line per page ended: "TIME PID RSS-BYTES", or per app: "TIME PID RSS-BYTES APP-NAME"
 *                      (rudolfd's /api/memory and the health log read it)
 *   memguard.critical  touched every second while memory is critical (the session's watchdog then waits longer)
 * Options: rudolf.nomemguard=1 on the start-up options (it then only sets oom_score_adj); --once prints what it
 * would do and exits (testing).
 *   1.5.7 (plan stage 7): --apps prints the apps that run, as JSON, for Task Manager (rudolfd's apps/running): Flatpak
 *   apps (all their sandboxes as one), Windows programs in Wine and Linux programs in Debian, each with its memory and
 *   whether a window of it is open; --end ID asks one to quit (SIGTERM) and ends what is left of it 5 s later
 *   (SIGKILL), its own processes only. When this guard quits an app it also says so to the desktop at once (a line
 *   "memory closed NAME MB" in /run/rudolf/events), which shows a notification.
 *
 * Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define RUN "/run/rudolf"
#define MAXP 4096
#define MIN_RSS (200LL << 20)   /* pages smaller than this are left alone */
#define COOLDOWN 8

struct proc { int pid, ppid; unsigned long long start; long long rss; char name[32];   /* rss: in memory and in the swap */ char kind; /* r renderer, b browser (kiosk), w browser (web window), g gpu, n network, o other chrome, s system, 0 */ char ext; };
static struct proc ps[MAXP];
static int np, once, off;
static long hz, pagesz;
static char buf[8192];

static void logf_(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void logf_(const char *fmt, ...)
{
	char m[512], t[64]; va_list a; va_start(a, fmt); vsnprintf(m, sizeof m, fmt, a); va_end(a);
	time_t now = time(NULL); struct tm tm; localtime_r(&now, &tm); strftime(t, sizeof t, "%a %b %e %H:%M:%S %Z %Y", &tm);
	if (once) { printf("%s\n", m); return; }
	char l[640]; int n = snprintf(l, sizeof l, "%s memory guard: %s\n", t, m);
	if (n <= 0) return;
	if (n >= (int)sizeof l) n = sizeof l - 1;
	int f = open(RUN "/memguard.log", O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644); if (f >= 0) { if (write(f, l, n) < 0) {} close(f); }
	f = open(RUN "/session.log", O_WRONLY | O_APPEND | O_CLOEXEC); if (f >= 0) { if (write(f, l, n) < 0) {} close(f); }
}
static ssize_t readf(const char *path, char *b, size_t n)
{
	int f = open(path, O_RDONLY | O_CLOEXEC); if (f < 0) return -1;
	ssize_t r = read(f, b, n - 1); close(f); if (r < 0) r = 0; b[r] = 0; return r;
}
static void writef(const char *path, const char *s) { int f = open(path, O_WRONLY | O_CLOEXEC); if (f >= 0) { if (write(f, s, strlen(s)) < 0) {} close(f); } }
static long long meminfo(const char *b, const char *key)
{
	const char *p = strstr(b, key); return p ? atoll(p + strlen(key)) * 1024 : 0;
}
static double psi_full10(void)
{
	if (readf("/proc/pressure/memory", buf, sizeof buf) <= 0) return -1;
	const char *p = strstr(buf, "full avg10="); return p ? atof(p + 11) : -1;
}

/* every process: kind, parent, start time, memory */
static void scan(void)
{
	DIR *d = opendir("/proc"); struct dirent *e; np = 0;
	char path[64];
	while (d && (e = readdir(d)) && np < MAXP) {
		int pid = atoi(e->d_name); if (pid <= 0) continue;
		snprintf(path, sizeof path, "/proc/%d/stat", pid);
		if (readf(path, buf, sizeof buf) <= 0) continue;
		char *a = strchr(buf, '('), *b = strrchr(buf, ')'); if (!a || !b) continue;
		struct proc *p = &ps[np]; memset(p, 0, sizeof *p); p->pid = pid;
		char name[32]; size_t nl = b - a - 1; if (nl >= sizeof name) nl = sizeof name - 1; memcpy(name, a + 1, nl); name[nl] = 0;
		snprintf(p->name, sizeof p->name, "%s", name);
		/* after ')': state ppid pgrp session tty tpgid flags minflt cminflt majflt cmajflt utime stime cutime cstime
		   priority nice threads itrealvalue starttime */
		char st; if (sscanf(b + 2, "%c %d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %*d %*d %*d %*d %*d %*d %llu", &st, &p->ppid, &p->start) < 3) continue;
		if (!strcmp(name, "chrome")) {
			snprintf(path, sizeof path, "/proc/%d/cmdline", pid);
			ssize_t n = readf(path, buf, sizeof buf);
			for (ssize_t i = 0; i < n; i++) if (!buf[i]) buf[i] = ' ';
			if (strstr(buf, "--type=renderer")) { p->kind = 'r'; p->ext = strstr(buf, "--extension-process") != NULL; }
			else if (strstr(buf, "--type=gpu-process")) p->kind = 'g';
			else if (strstr(buf, "--type=utility") && strstr(buf, "NetworkService")) p->kind = 'n';
			else if (strstr(buf, "--type=")) p->kind = 'o';
			else if (strstr(buf, "--rudolf-kiosk")) p->kind = 'b';
			else if (strstr(buf, "--class=RudolfWeb")) p->kind = 'w';
			else p->kind = 'o';
			/* its memory: what is in memory and what is in the swap. Under memory pressure most of a large page's
			   memory is in the (compressed) swap, so the memory in use alone makes it look small (1.4.7's first
			   test: the page was not ended because it seemed to be under 200 MiB) */
			snprintf(path, sizeof path, "/proc/%d/status", pid);
			if (readf(path, buf, sizeof buf) > 0) {
				const char *r = strstr(buf, "\nVmRSS:"), *w = strstr(buf, "\nVmSwap:");
				p->rss = ((r ? atoll(r + 8) : 0) + (w ? atoll(w + 9) : 0)) * 1024;
			}
		} else if (!strcmp(name, "Xorg") || !strcmp(name, "Xvfb") || !strcmp(name, "rudolf-display") || !strcmp(name, "rudolfd") || !strcmp(name, "rudolf-launch") || !strcmp(name, "rudolf-memguard")) p->kind = 's';
		np++;
	}
	if (d) closedir(d);
}
static struct proc *find(int pid) { for (int i = 0; i < np; i++) if (ps[i].pid == pid) return &ps[i]; return NULL; }
/* the browser a Chromium process belongs to (its main process: kind b or w), or NULL */
static struct proc *browser_of(struct proc *p)
{
	for (int k = 0; k < 6 && p; k++) { if (p->kind == 'b' || p->kind == 'w') return p; p = find(p->ppid); }
	return NULL;
}
/* the desktop's own page process: the desktop page says which it is each time it loads (rudolfd /api/memory/iam: it
   keeps its processor busy for half a second, and the page process that used that time is the desktop's). Chromium
   does not say which process holds which page, and a start-time rule failed: Chromium starts a spare page process
   early and gives it to the next tab (1.4.7's QEMU test: the test page was taken for the desktop's own). The shell
   (the Isolated Web App) and the spare process are small, so as the largest they are never ended either. */
static int desktop_pid(void)
{
	static char b[32]; if (readf(RUN "/desktop-renderer", b, sizeof b) <= 0) return 0;
	return atoi(b);
}
static int desktop_own(struct proc *p, struct proc *b) { (void)b; int d = desktop_pid(); return d > 0 && p->pid == d; }
static void set_adj(int pid, int adj)
{
	char path[64], cur[16], v[16]; snprintf(path, sizeof path, "/proc/%d/oom_score_adj", pid);
	if (readf(path, cur, sizeof cur) > 0 && atoi(cur) == adj) return;
	snprintf(v, sizeof v, "%d", adj); writef(path, v);
}
/* which processes the kernel ends last: the desktop and what draws it */
static void protect(void)
{
	for (int i = 0; i < np; i++) {
		struct proc *p = &ps[i], *b;
		switch (p->kind) {
		case 's': set_adj(p->pid, -900); break;
		case 'b': set_adj(p->pid, -800); break;
		case 'g': case 'n': if ((b = browser_of(p)) && b->kind == 'b') set_adj(p->pid, -700); break;
		case 'r': if (desktop_own(p, NULL)) set_adj(p->pid, 0); break;
		}
	}
}
/* the largest web page that may be ended, or NULL: any page process but the desktop's own, also one that no browser
   holds any more (left behind when the browser was restarted: it only holds memory) */
static struct proc *victim(void)
{
	struct proc *best = NULL;
	for (int i = 0; i < np; i++) {
		struct proc *p = &ps[i];
		if (p->kind != 'r' || p->ext || p->rss < MIN_RSS || desktop_own(p, NULL)) continue;
		if (!best || p->rss > best->rss) best = p;
	}
	return best;
}

/* 1.5.6: apps. Each Flatpak sandbox has a folder in /run/user/UID/.flatpak with "pid" (its bwrap, the root of its
   processes) and "info" ([Application] name=com.discordapp.Discord). Steam starts its interface and its games in
   further sandboxes of its own (through the Flatpak portal): the same name, so they count as Steam's. */
#define MAXAPP 32
struct app { char id[128]; int roots[16], nroots; long long rss; };
static struct app apps[MAXAPP];
static int napps;
static int under(struct proc *p, int root) { for (int k = 0; k < 64 && p; k++) { if (p->pid == root) return 1; p = find(p->ppid); } return 0; }
static long long proc_mem(int pid)
{
	char path[64]; snprintf(path, sizeof path, "/proc/%d/status", pid);
	if (readf(path, buf, sizeof buf) <= 0) return 0;
	const char *r = strstr(buf, "\nVmRSS:"), *w = strstr(buf, "\nVmSwap:");
	return ((r ? atoll(r + 8) : 0) + (w ? atoll(w + 9) : 0)) * 1024;
}
static void scan_apps(const char *base)
{
	napps = 0;
	DIR *u = opendir(base); struct dirent *e;
	while (u && (e = readdir(u))) {
		if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
		char dir[600]; snprintf(dir, sizeof dir, "%s/%s/.flatpak", base, e->d_name);
		DIR *d = opendir(dir); struct dirent *i;
		while (d && (i = readdir(d))) {
			if (i->d_name[0] == '.') continue;
			char path[900], info[2048];
			snprintf(path, sizeof path, "%s/%s/pid", dir, i->d_name);
			if (readf(path, info, sizeof info) <= 0) continue;
			int root = atoi(info); if (root <= 1 || !find(root)) continue;
			snprintf(path, sizeof path, "%s/%s/info", dir, i->d_name);
			if (readf(path, info, sizeof info) <= 0) continue;
			char *n = strstr(info, "\nname="); if (!n && !strncmp(info, "name=", 5)) n = info - 1;
			if (!n) continue;
			n += 6; size_t l = strcspn(n, "\n"); if (!l || l >= sizeof apps[0].id) continue;
			struct app *a = NULL;
			for (int k = 0; k < napps; k++) if (strlen(apps[k].id) == l && !strncmp(apps[k].id, n, l)) a = &apps[k];
			if (!a) { if (napps >= MAXAPP) continue; a = &apps[napps++]; memset(a, 0, sizeof *a); memcpy(a->id, n, l); a->id[l] = 0; }
			if (a->nroots < 16) a->roots[a->nroots++] = root;
		}
		if (d) closedir(d);
	}
	if (u) closedir(u);
	for (int k = 0; k < napps; k++)
		for (int i = 0; i < np; i++) for (int r = 0; r < apps[k].nroots; r++) if (under(&ps[i], apps[k].roots[r])) { apps[k].rss += proc_mem(ps[i].pid); break; }
}
/* the app's short name: com.discordapp.Discord -> Discord */
static const char *app_name(const struct app *a) { const char *d = strrchr(a->id, '.'); return d ? d + 1 : a->id; }
/* true when one of its windows is open: a window class (or instance) that is its short name, or starts with it and
   "_" (steam_app_945360 for a Steam game), case aside. /run/rudolf/windows: id, web, class, instance, full screen, title */
static int has_window(const struct app *a, const char *wins)
{
	const char *nm = app_name(a); size_t nl = strlen(nm);
	for (const char *l = wins; l && *l; ) {
		const char *end = strchr(l, '\n'); if (!end) end = l + strlen(l);
		const char *f = l; int col = 0;
		while (f < end) {
			const char *t = memchr(f, '\t', end - f); if (!t) t = end;
			if ((col == 2 || col == 3) && (size_t)(t - f) >= nl && !strncasecmp(f, nm, nl) && ((size_t)(t - f) == nl || f[nl] == '_')) return 1;
			f = t + 1; col++;
		}
		l = *end ? end + 1 : end;
	}
	return 0;
}
/* the largest app with no window open, over MIN_RSS, or NULL */
static struct app *app_victim(void)
{
	static char wins[16384];
	/* (--once, for the host test: other places, and a smaller size) */
	const char *wf = once && getenv("MG_WINDOWS") ? getenv("MG_WINDOWS") : RUN "/windows", *ru = once && getenv("MG_RUNUSER") ? getenv("MG_RUNUSER") : "/run/user";
	long long min = once && getenv("MG_MIN_MIB") ? atoll(getenv("MG_MIN_MIB")) << 20 : MIN_RSS;
	if (readf(wf, wins, sizeof wins) < 0) wins[0] = 0;
	scan_apps(ru);
	struct app *best = NULL;
	for (int k = 0; k < napps; k++) {
		if (apps[k].rss < min || has_window(&apps[k], wins)) continue;
		if (!best || apps[k].rss > best->rss) best = &apps[k];
	}
	return best;
}
/* 1.5.7: Settings › System health › Memory › "Close apps with no window open when memory runs out" (on unless
   /persist/etc/rudolf-memory says close_apps=0) */
static int close_apps_on(void)
{
	char b[200]; const char *f = getenv("MG_TEST") ? getenv("MG_MEMCONF") : "/persist/etc/rudolf-memory";
	if (!f || readf(f, b, sizeof b) <= 0) return 1;
	return strstr(b, "close_apps=0") == NULL;
}
static void signal_app(struct app *a, int sig)
{
	scan();
	for (int i = 0; i < np; i++) for (int r = 0; r < a->nroots; r++) if (under(&ps[i], a->roots[r])) { kill(ps[i].pid, sig); break; }
}

/* ---------- 1.5.7 (plan stage 7): the apps that run, for Task Manager, and ending one ---------- */
/* Debian's processes are under its keeper (debian-init, /run/rudolf/debian/init.pid). A Windows program is a process
   named *.exe that is not one of Wine's own; a Linux program is a process whose parent is the keeper or a shell, that
   is not a shell or a helper. Each counts with everything under it. */
static const char *const DEB_HELPERS[] = { "debian-init", "sh", "bash", "dash", "sleep", "wineserver", "services.exe", "winedevice.exe",
	"explorer.exe", "plugplay.exe", "svchost.exe", "rpcss.exe", "conhost.exe", "start.exe", "winemenubuilder", "wineboot.exe",
	"tabtip.exe", "rundll32.exe", "dbus-daemon", "dbus-launch", "pulseaudio", "xdg-desktop-por", "xdg-document-po", "xdg-permission-", "at-spi-bus-laun", "at-spi2-registr", "gvfsd", NULL };
static int deb_helper(const char *n) { for (int i = 0; DEB_HELPERS[i]; i++) if (!strcmp(n, DEB_HELPERS[i])) return 1; return 0; }
static int is_shell(const char *n) { return !strcmp(n, "sh") || !strcmp(n, "bash") || !strcmp(n, "dash"); }
/* the program's own name: its command line's first word, after the last / or \\ (Wine gives C:\\...\\Game.exe; the
   kernel's name is cut at 15 letters, so a long .exe name loses its ".exe"); the kernel's name otherwise */
static void long_name(struct proc *p, char *o, size_t on)
{
	char path[64], cl[1024]; snprintf(path, sizeof path, "/proc/%d/cmdline", p->pid); ssize_t n = readf(path, cl, sizeof cl);
	if (n > 0) { cl[sizeof cl - 1] = 0; const char *b = cl, *c; for (c = cl; *c; c++) if (*c == '/' || *c == '\\') b = c + 1;
		if (*b && (size_t)(c - b) < on && strncmp(b, p->name, strlen(p->name) > 15 ? 15 : strlen(p->name)) == 0) { snprintf(o, on, "%s", b); return; } }
	snprintf(o, on, "%s", p->name);
}
static int ends_exe(const char *n) { size_t l = strlen(n); return l > 4 && !strcasecmp(n + l - 4, ".exe"); }
struct dapp { int root; char name[64]; char kind; long long rss; };
static struct dapp dapps[64]; static int ndapps;
static void scan_debian(void)
{
	ndapps = 0;
	char path[64]; const char *kp = once && getenv("MG_KEEPER") ? NULL : RUN "/debian/init.pid"; int keeper = 0;
	if (kp) { if (readf(kp, path, sizeof path) > 0) keeper = atoi(path); } else keeper = atoi(getenv("MG_KEEPER"));
	struct proc *k = keeper > 1 ? find(keeper) : NULL;
	if (!k || (strcmp(k->name, "debian-init") && !(once && getenv("MG_KEEPER")))) return;
	for (int i = 0; i < np && ndapps < 64; i++) {
		struct proc *p = &ps[i]; if (p->pid == keeper || !under(p, keeper)) continue;
		char ln[64]; long_name(p, ln, sizeof ln); if (deb_helper(ln) || deb_helper(p->name)) continue;
		struct proc *par = find(p->ppid); if (!par) continue;
		int exe = ends_exe(ln), top;
		if (exe) { /* the topmost .exe that is not Wine's own: a program started by another counts with it */
			top = 1; for (struct proc *q = par; q && q->pid != keeper; q = find(q->ppid)) { char qn[64]; long_name(q, qn, sizeof qn); if (ends_exe(qn) && !deb_helper(qn)) { top = 0; break; } } }
		else { top = par->pid == keeper || is_shell(par->name);
			/* under a Windows program, or under another Linux program, it counts with that one */
			for (struct proc *q = par; top && q && q->pid != keeper; q = find(q->ppid)) { char qn[64]; long_name(q, qn, sizeof qn); if (!deb_helper(qn) && !is_shell(qn)) top = 0; } }
		if (!top) continue;
		struct dapp *d = &dapps[ndapps++]; d->root = p->pid; snprintf(d->name, sizeof d->name, "%s", ln); d->kind = exe ? 'w' : 'd'; d->rss = 0;
	}
	for (int a = 0; a < ndapps; a++) for (int i = 0; i < np; i++) if (under(&ps[i], dapps[a].root)) dapps[a].rss += proc_mem(ps[i].pid);
}
/* a Windows or Linux program's window: Wine's class is the .exe's name; a Linux program's is usually its name */
static int dapp_window(const struct dapp *d, const char *wins)
{
	char nm[64]; snprintf(nm, sizeof nm, "%s", d->name); size_t nl = strlen(nm);
	char st[64]; snprintf(st, sizeof st, "%s", nm); if (d->kind == 'w' && nl > 4) st[nl - 4] = 0; size_t sl = strlen(st);
	for (const char *l = wins; l && *l; ) {
		const char *end = strchr(l, '\n'); if (!end) end = l + strlen(l);
		const char *f = l; int col = 0;
		while (f < end) {
			const char *t = memchr(f, '\t', end - f); if (!t) t = end; size_t fl = t - f;
			if ((col == 2 || col == 3) && ((fl == nl && !strncasecmp(f, nm, nl)) || (fl == sl && !strncasecmp(f, st, sl)))) return 1;
			f = t + 1; col++;
		}
		l = *end ? end + 1 : end;
	}
	return 0;
}
static void jstr(const char *v) { putchar('"'); for (const unsigned char *c = (const unsigned char *)v; *c; c++) { if (*c == '"' || *c == '\\') printf("\\%c", *c); else if (*c < 32) printf("\\u%04x", *c); else putchar(*c); } putchar('"'); }
static void list_apps(void)
{
	static char wins[16384];
	const char *wf = getenv("MG_WINDOWS") && once ? getenv("MG_WINDOWS") : RUN "/windows", *ru = getenv("MG_RUNUSER") && once ? getenv("MG_RUNUSER") : "/run/user";
	if (readf(wf, wins, sizeof wins) < 0) wins[0] = 0;
	scan(); scan_apps(ru); scan_debian();
	printf("{\"ok\":true,\"apps\":["); int n = 0;
	for (int k = 0; k < napps; k++) { printf("%s{\"id\":\"flatpak:", n++ ? "," : ""); printf("%s\",\"app\":", apps[k].id); jstr(apps[k].id);
		printf(",\"name\":"); jstr(app_name(&apps[k])); printf(",\"kind\":\"flatpak\",\"rss\":%lld,\"window\":%s,\"processes\":%d}", apps[k].rss, has_window(&apps[k], wins) ? "true" : "false", apps[k].nroots); }
	for (int k = 0; k < ndapps; k++) { printf("%s{\"id\":\"%s:%d\",\"name\":", n++ ? "," : "", dapps[k].kind == 'w' ? "wine" : "debian", dapps[k].root); jstr(dapps[k].name);
		printf(",\"kind\":\"%s\",\"rss\":%lld,\"window\":%s}", dapps[k].kind == 'w' ? "wine" : "debian", dapps[k].rss, dapp_window(&dapps[k], wins) ? "true" : "false"); }
	printf("]}\n");
}
/* the processes of one listed app: flatpak:ID, wine:PID or debian:PID (a PID that is one of the listed programs) */
static int app_pids(const char *id, int *out, int max)
{
	scan(); const char *ru = getenv("MG_RUNUSER") && once ? getenv("MG_RUNUSER") : "/run/user"; int n = 0;
	if (!strncmp(id, "flatpak:", 8)) { scan_apps(ru);
		for (int k = 0; k < napps; k++) if (!strcmp(apps[k].id, id + 8))
			for (int i = 0; i < np && n < max; i++) for (int r = 0; r < apps[k].nroots; r++) if (under(&ps[i], apps[k].roots[r])) { out[n++] = ps[i].pid; break; }
		return n; }
	int pid = 0; if (!strncmp(id, "wine:", 5)) pid = atoi(id + 5); else if (!strncmp(id, "debian:", 7)) pid = atoi(id + 7); else return 0;
	scan_debian(); int listed = 0; for (int k = 0; k < ndapps; k++) if (dapps[k].root == pid && (dapps[k].kind == 'w') == !strncmp(id, "wine:", 5)) listed = 1;
	if (!listed) return 0;
	for (int i = 0; i < np && n < max; i++) if (under(&ps[i], pid)) out[n++] = ps[i].pid;
	return n;
}
static int end_app(const char *id)
{
	static int pids[MAXP]; int n = app_pids(id, pids, MAXP);
	if (!n) { printf("{\"ok\":false,\"error\":\"that app is not running any more\"}\n"); return 1; }
	for (int i = 0; i < n; i++) kill(pids[i], SIGTERM);
	int left = n, waited = 0;
	for (; waited < 50 && left; waited++) { usleep(100000); left = 0; for (int i = 0; i < n; i++) if (!kill(pids[i], 0)) { char p[64]; snprintf(p, sizeof p, "/proc/%d/stat", pids[i]); if (readf(p, buf, sizeof buf) > 0) { char *b = strrchr(buf, ')'); if (b && b[2] != 'Z') left++; } } }
	int forced = 0; if (left) { int now[MAXP]; int m = app_pids(id, now, MAXP); for (int i = 0; i < m; i++) { kill(now[i], SIGKILL); forced++; } }
	printf("{\"ok\":true,\"processes\":%d,\"forced\":%s}\n", n, forced ? "true" : "false");
	return 0;
}

int main(int argc, char **argv)
{
	once = argc > 1 && !strcmp(argv[1], "--once");
	/* 1.5.7: (the host test sets MG_TEST to move the places, as --once does) */
	if (argc > 1 && (!strcmp(argv[1], "--apps") || !strcmp(argv[1], "--end"))) {
		once = getenv("MG_TEST") != NULL; hz = sysconf(_SC_CLK_TCK); pagesz = sysconf(_SC_PAGESIZE);
		if (!strcmp(argv[1], "--apps")) { list_apps(); return 0; }
		if (argc < 3 || strlen(argv[2]) > 200) { printf("{\"ok\":false,\"error\":\"which app?\"}\n"); return 2; }
		return end_app(argv[2]);
	}
	hz = sysconf(_SC_CLK_TCK); pagesz = sysconf(_SC_PAGESIZE);
	if (readf("/etc/rudolf/cmdline", buf, sizeof buf) <= 0) readf("/proc/cmdline", buf, sizeof buf);
	off = strstr(buf, "rudolf.nomemguard=1") != NULL;
	if (once) {
		scan();
		long long avail = 0, total = 0; if (readf("/proc/meminfo", buf, sizeof buf) > 0) { avail = meminfo(buf, "MemAvailable:"); total = meminfo(buf, "MemTotal:"); }
		printf("memory: %lld MiB available of %lld MiB, stall %.1f%%\n", avail >> 20, total >> 20, psi_full10());
		for (int i = 0; i < np; i++) { struct proc *p = &ps[i], *b; if (p->kind != 'r') continue; b = browser_of(p);
			printf("renderer %d: %lld MiB%s%s%s\n", p->pid, p->rss >> 20, p->ext ? " (extension)" : "", !b ? " (no browser)" : b->kind == 'w' ? " (web window)" : "",
				desktop_own(p, NULL) ? " (the desktop's own: kept)" : ""); }
		struct proc *v = victim(); if (v) printf("would end: %d (%lld MiB)\n", v->pid, v->rss >> 20); else printf("would end: none\n");
		struct app *a = app_victim();
		for (int k = 0; k < napps; k++) printf("app %s: %lld MiB, %d sandbox(es)%s\n", apps[k].id, apps[k].rss >> 20, apps[k].nroots, &apps[k] == a ? " (no window open: would quit it)" : "");
		if (!v && a) { if (close_apps_on()) printf("would quit app: %s (%lld MiB)\n", app_name(a), a->rss >> 20); else printf("would quit app: none (closing apps is off)\n"); }
		return 0;
	}
	/* never swapped out or stalled itself; ended last; ahead of the pages */
	if (mlockall(MCL_CURRENT | MCL_FUTURE)) {}
	set_adj(getpid(), -1000);
	setpriority(PRIO_PROCESS, 0, -10);
	signal(SIGPIPE, SIG_IGN);
	/* the kernel wakes us as soon as tasks stall on memory for 150 ms within a second */
	int pf = open("/proc/pressure/memory", O_RDWR | O_NONBLOCK | O_CLOEXEC);
	if (pf >= 0 && write(pf, "full 150000 1000000", 20) < 0) { close(pf); pf = -1; }
	logf_("started%s%s", pf < 0 ? " (no memory pressure trigger: once a second)" : "", off ? " (ending pages is off: rudolf.nomemguard=1)" : "");
	int low = 0, stall = 0; time_t last_kill = 0, last_protect = 0, was_crit = 0; struct timespec fire0 = {0, 0};
	for (;;) {
		struct pollfd p = { pf, POLLPRI, 0 };
		int fired = 0;
		if (pf >= 0) { int r = poll(&p, 1, 1000); if ((r < 0 && errno != EINTR) || (r > 0 && (p.revents & (POLLERR | POLLNVAL)))) { close(pf); pf = -1; sleep(1); } else if (r > 0 && (p.revents & POLLPRI)) fired = 1; } else sleep(1);
		time_t now = time(NULL);
		if (readf("/proc/meminfo", buf, sizeof buf) <= 0) continue;
		long long total = meminfo(buf, "MemTotal:"), avail = meminfo(buf, "MemAvailable:"), swt = meminfo(buf, "SwapTotal:"), swf = meminfo(buf, "SwapFree:");
		double full = psi_full10();
		int swap_low = swt == 0 || swf < swt / 10;
		int hard = avail < total / 20 && swap_low;
		low = avail < total / 10 && swap_low && full >= 10 ? low + 1 : 0;
		stall = full >= 30 && avail < total * 15 / 100 ? stall + 1 : 0;
		/* 1.4.8: thrashing while the swap still has room. Memory can fill faster than pages are moved to the
		   compressed swap (QEMU: 175 MiB left with 954 of 987 MiB of swap free, and the desktop stalled 30 s
		   until the 10-second stall average reached 30%). So the kernel's own stall report counts at once: two
		   reports (tasks stalled on memory 150 ms within a second) within 2.5 s, with under 8% of the memory left. */
		int thrash = 0;
		if (fired) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); double gap = (t.tv_sec - fire0.tv_sec) + (t.tv_nsec - fire0.tv_nsec) / 1e9;
			thrash = fire0.tv_sec && gap < 2.5 && avail < total * 8 / 100; fire0 = t; }
		int crit = hard || low >= 2 || stall >= 3 || thrash;
		if (crit) { int f = open(RUN "/memguard.critical", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644); if (f >= 0) close(f);
			if (!was_crit) logf_("memory is low: %lld MiB available of %lld MiB, swap %lld of %lld MiB free, stall %.0f%%%s", avail >> 20, total >> 20, swf >> 20, swt >> 20, full, thrash && !hard && low < 2 && stall < 3 ? " (thrashing: the kernel reports stalls)" : "");
			was_crit = now; }
		else if (was_crit && now - was_crit > 10) { logf_("memory is back: %lld MiB available", avail >> 20); was_crit = 0; }
		if (now - last_protect >= 5 || crit) { scan(); protect(); last_protect = now; }
		if (!crit || off || now - last_kill < COOLDOWN) continue;
		struct proc *v = victim();
		struct app *a = v || !close_apps_on() ? NULL : app_victim();
		if (!v && a) {
			char nm[128]; snprintf(nm, sizeof nm, "%s", app_name(a)); long long rss = a->rss; int root = a->roots[0];
			signal_app(a, SIGTERM); sleep(3);
			if ((a = app_victim()) && !strcmp(app_name(a), nm)) signal_app(a, SIGKILL);
			logf_("quit %s (no window open, %lld MiB) to free memory", nm, rss >> 20);
			char l[200]; int n = snprintf(l, sizeof l, "%lld %d %lld %s\n", (long long)now, root, rss, nm);
			int f = open(RUN "/memguard.events", O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644); if (f >= 0) { if (write(f, l, n) < 0) {} close(f); }
			/* 1.5.7: the desktop says so at once ("Discord was closed to free memory") */
			n = snprintf(l, sizeof l, "memory closed %s %lld\n", nm, rss >> 20);
			f = open(RUN "/events", O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644); if (f >= 0) { if (write(f, l, n) < 0) {} close(f); }
			last_kill = time(NULL); low = stall = 0; fire0.tv_sec = 0;
			continue;
		}
		if (!v) {
			if (now - last_kill >= 30) {
				/* say what holds the memory: the largest page renderer, and why it was left alone */
				struct proc *big = NULL;
				for (int i = 0; i < np; i++) if (ps[i].kind == 'r' && (!big || ps[i].rss > big->rss)) big = &ps[i];
				if (big) logf_("no web page to end: the largest page renderer (%d, %lld MiB) is %s", big->pid, big->rss >> 20,
					big->ext ? "an extension" : desktop_own(big, NULL) ? "the desktop's own" : "under 200 MiB");
				else logf_("no web page to end (no page renderers: a program holds the memory)");
				logf_("no app without a window open to quit either");
				last_kill = now;
			}
			continue;
		}
		if (kill(v->pid, SIGKILL) == 0) {
			logf_("ended the largest web page (renderer %d, %lld MiB) to free memory", v->pid, v->rss >> 20);
			char l[96]; int n = snprintf(l, sizeof l, "%lld %d %lld\n", (long long)now, v->pid, v->rss);
			int f = open(RUN "/memguard.events", O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644); if (f >= 0) { if (write(f, l, n) < 0) {} close(f); }
		}
		last_kill = now; low = stall = 0; fire0.tv_sec = 0;
	}
}
