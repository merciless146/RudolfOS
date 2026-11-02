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
 *     last 10 s for 3 s in a row with under 15% left
 *   - then it ends (SIGKILL) the renderer process of the largest web page: a tab in Oracle (a Controlled Frame), or a
 *     page in the browser window for sign-in sites, or one no browser holds any more. Never the desktop page itself
 *     (it says which process it is: /run/rudolf/desktop-renderer), nor Chromium's other processes, the graphics
 *     process or the system. Pages under 200 MiB are left alone too.
 *     Oracle then shows "This page was closed because it used too much memory" with a Reload button.
 *   - after ending one it waits 8 s (the memory takes a moment to come back) before it looks again
 * It also tells the kernel which processes to end last if the kernel has to step in itself (oom_score_adj): the
 * desktop's browser, its graphics and network processes, Xorg, rudolf-display, rudolfd, rudolf-launch and this guard.
 *
 * Files (in /run/rudolf):
 *   memguard.log       what it did (also copied to the session log)
 *   memguard.events    one line per page ended: "TIME PID RSS-BYTES" (rudolfd's /api/memory reads it)
 *   memguard.critical  touched every second while memory is critical (the session's watchdog then waits longer)
 * Options: rudolf.nomemguard=1 on the start-up options (it then only sets oom_score_adj); --once prints what it
 * would do and exits (testing).
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
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define RUN "/run/rudolf"
#define MAXP 4096
#define MIN_RSS (200LL << 20)   /* pages smaller than this are left alone */
#define COOLDOWN 8

struct proc { int pid, ppid; unsigned long long start; long long rss;   /* rss: in memory and in the swap */ char kind; /* r renderer, b browser (kiosk), w browser (web window), g gpu, n network, o other chrome, s system, 0 */ char ext; };
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

int main(int argc, char **argv)
{
	once = argc > 1 && !strcmp(argv[1], "--once");
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
	int low = 0, stall = 0; time_t last_kill = 0, last_protect = 0, was_crit = 0;
	for (;;) {
		struct pollfd p = { pf, POLLPRI, 0 };
		if (pf >= 0) { int r = poll(&p, 1, 1000); if ((r < 0 && errno != EINTR) || (r > 0 && (p.revents & (POLLERR | POLLNVAL)))) { close(pf); pf = -1; sleep(1); } } else sleep(1);
		time_t now = time(NULL);
		if (readf("/proc/meminfo", buf, sizeof buf) <= 0) continue;
		long long total = meminfo(buf, "MemTotal:"), avail = meminfo(buf, "MemAvailable:"), swt = meminfo(buf, "SwapTotal:"), swf = meminfo(buf, "SwapFree:");
		double full = psi_full10();
		int swap_low = swt == 0 || swf < swt / 10;
		int hard = avail < total / 20 && swap_low;
		low = avail < total / 10 && swap_low && full >= 10 ? low + 1 : 0;
		stall = full >= 30 && avail < total * 15 / 100 ? stall + 1 : 0;
		int crit = hard || low >= 2 || stall >= 3;
		if (crit) { int f = open(RUN "/memguard.critical", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644); if (f >= 0) close(f);
			if (!was_crit) logf_("memory is low: %lld MiB available of %lld MiB, swap %lld of %lld MiB free, stall %.0f%%", avail >> 20, total >> 20, swf >> 20, swt >> 20, full);
			was_crit = now; }
		else if (was_crit && now - was_crit > 10) { logf_("memory is back: %lld MiB available", avail >> 20); was_crit = 0; }
		if (now - last_protect >= 5 || crit) { scan(); protect(); last_protect = now; }
		if (!crit || off || now - last_kill < COOLDOWN) continue;
		struct proc *v = victim();
		if (!v) {
			if (now - last_kill >= 30) {
				/* say what holds the memory: the largest page renderer, and why it was left alone */
				struct proc *big = NULL;
				for (int i = 0; i < np; i++) if (ps[i].kind == 'r' && (!big || ps[i].rss > big->rss)) big = &ps[i];
				if (big) logf_("no web page to end: the largest page renderer (%d, %lld MiB) is %s", big->pid, big->rss >> 20,
					big->ext ? "an extension" : desktop_own(big, NULL) ? "the desktop's own" : "under 200 MiB");
				else logf_("no web page to end (no page renderers: a program holds the memory)");
				last_kill = now;
			}
			continue;
		}
		if (kill(v->pid, SIGKILL) == 0) {
			logf_("ended the largest web page (renderer %d, %lld MiB) to free memory", v->pid, v->rss >> 20);
			char l[96]; int n = snprintf(l, sizeof l, "%lld %d %lld\n", (long long)now, v->pid, v->rss);
			int f = open(RUN "/memguard.events", O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644); if (f >= 0) { if (write(f, l, n) < 0) {} close(f); }
		}
		last_kill = now; low = stall = 0;
	}
}
