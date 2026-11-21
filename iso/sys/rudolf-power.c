/*
 * power - the root helper for power and the clock (Rudolf OS 1.6.1, docs/plan-1.6.1.md Part 11):
 * /usr/lib/rudolf/root/power, run by rudolf-rootd for rudolfd (or by rudolfd itself while it is root and was not
 * started by rudolf-rootd).
 *
 *   power off|reboot [T]      /usr/lib/rudolf/shutdown off|reboot T (its output in /run/rudolf/shutdown.out), then as
 *                             before 1.4.5 if it could not finish: Chromium stopped, sync, every file system read-only,
 *                             the computer turned off or restarted. T: the desktop's own steps' times (a-z 0-9 = ,)
 *   power wakealarm N         the real-time clock wakes the computer after N seconds (1-60: the hardware check)
 *   power sleep               the computer sleeps (mem into /sys/power/state); the kernel's refusal said on standard
 *                             output (status 1)
 *   power status              /usr/lib/rudolf/power status   (System Settings › Power and battery)
 *   power set KEY=VALUE...    /usr/lib/rudolf/power set ... (the script checks each key and value against its choices;
 *                             here: lower-case letters, digits, "_" and "=" only, at most 100 each, at most 30)
 *   power apply               /usr/lib/rudolf/power apply (at rudolfd's start)
 *   power brightness-step up|down   /usr/lib/rudolf/hotkey brightness up|down, as the keys
 *   power backlight VALUE     the screen's backlight (/sys/class/backlight/NAME/brightness) set to VALUE, 0 to its
 *                             max_brightness (rudolfd works VALUE out from the percentage, as before)
 *   power dropcache           clean cached files dropped (/proc/sys/vm/drop_caches 1)
 *   power priority on|off     /usr/lib/rudolf/desktop-priority on|off (Task Manager)
 *   power clock-set MS        the clock set to MS milliseconds since 1970, then the hardware clock (hwclock -w -u)
 *   power clock-sync          the clock from the internet (BusyBox ntpd: pool.ntp.org, else time.google.com), then
 *                             the hardware clock; "ok EPOCH" or "fail EPOCH" in /run/rudolf/time-sync, as before
 *
 * Everything is checked again here, whatever rudolfd checked. Programs are run with their arguments as they are, never
 * a command line for a shell, with no environment but PATH. A refusal is said in the health log (Security).
 *
 * Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include "rudolf-root.h"
#include <ctype.h>
#include <dirent.h>
#include <sys/reboot.h>
#include <sys/time.h>
#include <sys/wait.h>

#define SYSFS  rr_env("RR_TEST_SYS", "/sys")
#define PROCFS rr_env("RR_TEST_PROC", "/proc")
#define LIB    rr_env("RR_TEST_LIB", "/usr/lib/rudolf")
#define RUNDIR rr_env("RR_TEST_RUN", "/run/rudolf")
/* where the system's commands this helper runs are looked for (never the caller's PATH) */
#ifdef RR_TEST
/* (host tests: only in RR_TEST_BIN, where stand-ins answer for them: a host test must never run the real umount -a -r) */
#define CMD_DIRS rr_env("RR_TEST_BIN", "/nonexistent")
#else
#define CMD_DIRS "/usr/sbin:/usr/bin:/sbin:/bin"
#endif
static const char *path_env(void) { return RR_PATH; }
static int find_cmd(const char *name, char *out, size_t on)
{
	char dirs[PATH_MAX]; snprintf(dirs, sizeof dirs, "%s", CMD_DIRS); char *sv = NULL;
	for (char *d = strtok_r(dirs, ":", &sv); d; d = strtok_r(NULL, ":", &sv)) { snprintf(out, on, "%s/%s", d, name); if (!access(out, X_OK)) return 0; }
	return -1;
}

static int refuse(const char *what, const char *arg)
{
	char a[80]; size_t k = 0; for (const char *c = arg ? arg : ""; *c && k < sizeof a - 1; c++) a[k++] = isprint((unsigned char)*c) ? *c : '?';
	a[k] = 0;
	rr_health("security", "the power helper refused %s (%s)", what, a);
	fprintf(stderr, "refused: %s\n", what); return 2;
}
/* a program run and waited for: its status (127: not started). argv[0]: a full path, or a command looked for in
   CMD_DIRS. out/err: -1 to keep this helper's own, -2 for /dev/null */
static int run_prog(char *const argv[], int out, int err)
{
	char exe[PATH_MAX]; if (argv[0][0] == '/') snprintf(exe, sizeof exe, "%s", argv[0]); else if (find_cmd(argv[0], exe, sizeof exe)) return 127;
	pid_t p = fork(); if (p < 0) return 127;
	if (p == 0) {
		int nul = open("/dev/null", O_RDWR | O_CLOEXEC);
		if (out == -2) out = nul;
		if (err == -2) err = nul;
		if (out >= 0 && dup2(out, 1) < 0) _exit(127);
		if (err >= 0 && dup2(err, 2) < 0) _exit(127);
		if (nul >= 0) dup2(nul, 0);
		char *env[] = { (char *)path_env(), NULL };
		execve(exe, argv, env); _exit(127);
	}
	int st = 0; while (waitpid(p, &st, 0) < 0) if (errno != EINTR) return 127;
	return WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
}
/* a program of Rudolf OS's own (in LIB) in place of this helper: its output and status are the caller's */
static int exec_lib(const char *prog, char *const args[])
{
	char path[PATH_MAX]; snprintf(path, sizeof path, "%s/%s", LIB, prog);
	char *argv[40]; int a = 0; argv[a++] = path; for (int i = 0; args[i] && a < 39; i++) argv[a++] = args[i]; argv[a] = NULL;
	char *env[] = { (char *)path_env(), NULL };
	execve(path, argv, env); fprintf(stderr, "%s could not be started: %s\n", prog, strerror(errno)); return 127;
}
static int write_file(const char *p, const char *v)
{
	int f = open(p, O_WRONLY | O_CLOEXEC); if (f < 0) return -1;
	ssize_t n = write(f, v, strlen(v)); int e = errno; close(f); errno = e; return n == (ssize_t)strlen(v) ? 0 : -1;
}
static int digits(const char *s, size_t max) { size_t n = s ? strlen(s) : 0; return n && n <= max && strspn(s, "0123456789") == n; }
static int word_ok(const char *s, size_t max, const char *chars) { size_t n = s ? strlen(s) : 0; return n <= max && strspn(s, chars) == n; }
/* the screen's backlight, as rudolfd finds it (the first one; an intel or amdgpu one before others) */
static int backlight_dir(char *out, size_t n)
{
	char base[PATH_MAX]; snprintf(base, sizeof base, "%s/class/backlight", SYSFS);
	DIR *d = opendir(base); struct dirent *e; int ok = 0;
	while (d && (e = readdir(d))) {
		if (e->d_name[0] == '.') continue;
		snprintf(out, n, "%s/%s", base, e->d_name); ok = 1;
		if (strstr(e->d_name, "intel") || strstr(e->d_name, "amdgpu")) break;
	}
	if (d) closedir(d);
	return ok;
}
static void time_note(int ok)
{
	char p[PATH_MAX], v[64]; snprintf(p, sizeof p, "%s/time-sync", RUNDIR); snprintf(v, sizeof v, "%s %lld\n", ok ? "ok" : "fail", (long long)time(NULL));
	int f = open(p, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0644); if (f >= 0) { if (write(f, v, strlen(v)) < 0) {} close(f); }
}

int main(int argc, char **argv)
{
	if (argc < 2) { fprintf(stderr, "usage: power off|reboot|wakealarm|sleep|status|set|apply|brightness-step|backlight|dropcache|priority|clock-set|clock-sync ...\n"); return 2; }
	const char *op = argv[1]; int n = argc - 2; char **a = argv + 2;
	if (!strcmp(op, "off") || !strcmp(op, "reboot")) {
		if (n > 1 || (n == 1 && !word_ok(a[0], 200, "abcdefghijklmnopqrstuvwxyz0123456789=,"))) return refuse("a shutdown's times that are not ones", n ? a[0] : "");
		int rb = !strcmp(op, "reboot"); char *what = rb ? "reboot" : "poweroff";
		/* (as rudolfd did: the shutdown script, its output in RUN/shutdown.out; then, if it could not finish, as before 1.4.5) */
		char sh[PATH_MAX], out[PATH_MAX]; snprintf(sh, sizeof sh, "%s/shutdown", LIB); snprintf(out, sizeof out, "%s/shutdown.out", RUNDIR);
		if (!access(sh, X_OK)) { int f = open(out, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644); char *s[] = { sh, what, n ? a[0] : "", NULL }; run_prog(s, f >= 0 ? f : -2, f >= 0 ? f : -2); if (f >= 0) close(f); }
		{ char pw[PATH_MAX]; snprintf(pw, sizeof pw, "%s/power", RUNDIR); int f = open(pw, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644); if (f >= 0) { if (write(f, what, strlen(what)) < 0) {} close(f); } }
		{ char *s[] = { "pkill", "-KILL", "-f", "/opt/chromium/chrome", NULL }; run_prog(s, -2, -2); }
		sync();
		{ char *s[] = { "umount", "-a", "-r", NULL }; run_prog(s, -1, -2); }
#ifdef RR_TEST
		{ char p[PATH_MAX]; snprintf(p, sizeof p, "%s/rebooted", RUNDIR); int f = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (f >= 0) { if (write(f, what, strlen(what)) < 0) {} close(f); } return 0; }
#else
		reboot(rb ? RB_AUTOBOOT : RB_POWER_OFF);
		return 1;
#endif
	}
	if (!strcmp(op, "wakealarm")) {
		if (n != 1 || !digits(a[0], 2) || atoi(a[0]) < 1 || atoi(a[0]) > 60) return refuse("a wake alarm that is not 1 to 60 seconds", n ? a[0] : "");
		char p[PATH_MAX], v[16]; snprintf(p, sizeof p, "%s/class/rtc/rtc0/wakealarm", SYSFS); snprintf(v, sizeof v, "+%d", atoi(a[0]));
		if (write_file(p, "0")) {}
		return write_file(p, v) ? 1 : 0;
	}
	if (!strcmp(op, "sleep")) {
		if (n) return refuse("arguments for sleep", a[0]);
		char p[PATH_MAX]; snprintf(p, sizeof p, "%s/power/state", SYSFS);
		if (write_file(p, "mem")) { printf("%s\n", strerror(errno)); return 1; }
		return 0;
	}
	if (!strcmp(op, "status") || !strcmp(op, "apply")) {
		if (n) return refuse("arguments for the power settings", a[0]);
		char *s[] = { (char *)op, NULL }; return exec_lib("power", s);
	}
	if (!strcmp(op, "set")) {
		if (n < 1 || n > 30) return refuse("power settings that are not one to 30 of KEY=VALUE", n ? a[0] : "");
		for (int i = 0; i < n; i++) {
			if (!*a[i] || !word_ok(a[i], 100, "abcdefghijklmnopqrstuvwxyz0123456789_=")) return refuse("a power setting with other characters", a[i]);
		}
		char *s[33]; s[0] = "set"; for (int i = 0; i < n; i++) s[1 + i] = a[i]; s[1 + n] = NULL; return exec_lib("power", s);
	}
	if (!strcmp(op, "brightness-step")) {
		if (n != 1 || (strcmp(a[0], "up") && strcmp(a[0], "down"))) return refuse("a brightness step that is not up or down", n ? a[0] : "");
		char *s[] = { "brightness", a[0], NULL }; return exec_lib("hotkey", s);
	}
	if (!strcmp(op, "backlight")) {
		if (n != 1 || !digits(a[0], 9)) return refuse("a brightness that is not a number", n ? a[0] : "");
		char bl[PATH_MAX - 32], f[PATH_MAX]; if (!backlight_dir(bl, sizeof bl)) return 1;
		snprintf(f, sizeof f, "%s/max_brightness", bl); FILE *m = fopen(f, "r"); long long mx = -1; if (m) { if (fscanf(m, "%lld", &mx) != 1) mx = -1; fclose(m); }
		long long v = atoll(a[0]); if (mx < 0 || v > mx) return refuse("a brightness over the screen's most", a[0]);
		snprintf(f, sizeof f, "%s/brightness", bl); return write_file(f, a[0]) ? 1 : 0;
	}
	if (!strcmp(op, "dropcache")) {
		if (n) return refuse("arguments for dropcache", a[0]);
		char p[PATH_MAX]; snprintf(p, sizeof p, "%s/sys/vm/drop_caches", PROCFS); return write_file(p, "1") ? 1 : 0;
	}
	if (!strcmp(op, "priority")) {
		if (n != 1 || (strcmp(a[0], "on") && strcmp(a[0], "off"))) return refuse("a desktop priority that is not on or off", n ? a[0] : "");
		char *s[] = { a[0], NULL }; return exec_lib("desktop-priority", s);
	}
	if (!strcmp(op, "clock-set")) {
		if (n != 1 || !digits(a[0], 15)) return refuse("a time that is not milliseconds since 1970", n ? a[0] : "");
		long long ms = atoll(a[0]); struct timeval tv = { (time_t)(ms / 1000), (suseconds_t)((ms % 1000) * 1000) };
#ifdef RR_TEST
		{ char p[PATH_MAX], v[40]; snprintf(p, sizeof p, "%s/clock", RUNDIR); snprintf(v, sizeof v, "%lld.%06ld", (long long)tv.tv_sec, (long)tv.tv_usec); int f = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (f >= 0) { if (write(f, v, strlen(v)) < 0) {} close(f); } }
#else
		if (settimeofday(&tv, NULL)) return 1;
#endif
		char *s[] = { "hwclock", "-w", "-u", NULL }; run_prog(s, -2, -2); return 0;
	}
	if (!strcmp(op, "clock-sync")) {
		if (n) return refuse("arguments for clock-sync", a[0]);
		char *s1[] = { "ntpd", "-n", "-q", "-p", "pool.ntp.org", NULL }, *s2[] = { "ntpd", "-n", "-q", "-p", "time.google.com", NULL };
		if (run_prog(s1, -2, -2) && run_prog(s2, -2, -2)) { time_note(0); return 1; }
		char *h[] = { "hwclock", "-w", "-u", NULL }; run_prog(h, -2, -2); time_note(1); return 0;
	}
	return refuse("an unknown action", op);
}
