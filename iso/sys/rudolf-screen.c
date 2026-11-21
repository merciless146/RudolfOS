/*
 * screen - the root helper for the display's settings (Rudolf OS 1.6.1, docs/plan-1.6.1.md Part 11):
 * /usr/lib/rudolf/root/screen, run by rudolf-rootd for rudolfd (or by rudolfd itself while it is root and was not
 * started by rudolf-rootd). (Named screen: sys/rudolf-display.c is the window manager.)
 *
 *   screen mode soft|games|full     the graphics path (/etc/rudolf-display; on an installed Rudolf OS also
 *                                   /persist/etc/rudolf-display), used from the next start
 *   screen shell on|off             the desktop's shell (/etc/rudolf-shell, and /persist/etc on an installed one)
 *   screen ctl status|keep|revert   /usr/lib/rudolf/screenctl ACTION (Settings › Display)
 *   screen ctl set RES FPS          /usr/lib/rudolf/screenctl set RES FPS (letters, digits and - _ . :, at most 12 and 4)
 *   screen keyboard LAYOUT [VARIANT]  setxkbmap -layout LAYOUT [-variant VARIANT] on display :0 (at most 16 and 24)
 *   screen restart-display          rudolf-display stopped (the session starts it again: System health's restart)
 *   screen restart-browser          the desktop's own browser stopped (either engine: it starts again)
 *
 * "Installed" is this helper's own reading (/run/rudolf/persistent), never rudolfd's word. Everything is checked
 * again here. Programs are run with their arguments as they are, never a command line for a shell, with no
 * environment but PATH (and DISPLAY=:0 for setxkbmap). A refusal is said in the health log (Security).
 *
 * Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include "rudolf-root.h"
#include <ctype.h>
#include <sys/wait.h>

#define LIB     rr_env("RR_TEST_LIB", "/usr/lib/rudolf")
#define RUNDIR  rr_env("RR_TEST_RUN", "/run/rudolf")
#define ETC     rr_env("RR_TEST_ETC", "/etc")
#define PERSIST rr_env("RR_TEST_PERSIST", "/persist")
#ifdef RR_TEST
#define CMD_DIRS rr_env("RR_TEST_BIN", "/nonexistent")
#else
#define CMD_DIRS "/usr/sbin:/usr/bin:/sbin:/bin"
#endif
static int refuse(const char *what, const char *arg)
{
	char a[80]; size_t k = 0; for (const char *c = arg ? arg : ""; *c && k < sizeof a - 1; c++) a[k++] = isprint((unsigned char)*c) ? *c : '?';
	a[k] = 0;
	rr_health("security", "the screen helper refused %s (%s)", what, a);
	fprintf(stderr, "refused: %s\n", what); return 2;
}
static int find_cmd(const char *name, char *out, size_t on)
{
	char dirs[PATH_MAX]; snprintf(dirs, sizeof dirs, "%s", CMD_DIRS); char *sv = NULL;
	for (char *d = strtok_r(dirs, ":", &sv); d; d = strtok_r(NULL, ":", &sv)) { snprintf(out, on, "%s/%s", d, name); if (!access(out, X_OK)) return 0; }
	return -1;
}
/* a system command in place of this helper (env: its environment) */
static int exec_cmd(const char *name, char *const argv[], char *const env[])
{
	char exe[PATH_MAX]; if (find_cmd(name, exe, sizeof exe)) { fprintf(stderr, "%s is not there\n", name); return 127; }
	execve(exe, argv, env); fprintf(stderr, "%s could not be started: %s\n", name, strerror(errno)); return 127;
}
static int safe_word(const char *s, size_t max)
{
	if (!s || !*s || strlen(s) > max) return 0;
	for (; *s; s++) if (!isalnum((unsigned char)*s) && !strchr("-_.:", *s)) return 0;
	return 1;
}
static int write_file(const char *dir, const char *name, const char *v)
{
	char p[PATH_MAX]; snprintf(p, sizeof p, "%s/%s", dir, name);
	int f = open(p, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0644); if (f < 0) return -1;
	ssize_t n = write(f, v, strlen(v)); close(f); return n == (ssize_t)strlen(v) ? 0 : -1;
}
/* a setting into /etc, and on an installed Rudolf OS into /persist/etc too (as rudolfd did: errors not said) */
static int setting(const char *name, const char *v)
{
	if (write_file(ETC, name, v)) {}
	char pf[PATH_MAX]; snprintf(pf, sizeof pf, "%s/persistent", RUNDIR);
	if (!access(pf, F_OK)) { char d[PATH_MAX]; snprintf(d, sizeof d, "%s/etc", PERSIST); mkdir(d, 0755); if (write_file(d, name, v)) {} }
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 2) { fprintf(stderr, "usage: screen mode|shell|ctl|keyboard|restart-display|restart-browser ...\n"); return 2; }
	const char *op = argv[1]; int n = argc - 2; char **a = argv + 2;
	char *penv[] = { RR_PATH, NULL };
	if (!strcmp(op, "mode")) {
		if (n != 1 || (strcmp(a[0], "soft") && strcmp(a[0], "games") && strcmp(a[0], "full"))) return refuse("a graphics path that is not soft, games or full", n ? a[0] : "");
		char v[16]; snprintf(v, sizeof v, "%s\n", a[0]); return setting("rudolf-display", v);
	}
	if (!strcmp(op, "shell")) {
		if (n != 1 || (strcmp(a[0], "on") && strcmp(a[0], "off"))) return refuse("a shell setting that is not on or off", n ? a[0] : "");
		return setting("rudolf-shell", strcmp(a[0], "on") ? "off\n" : "on\n");
	}
	if (!strcmp(op, "ctl")) {
		char sc[PATH_MAX]; snprintf(sc, sizeof sc, "%s/screenctl", LIB);
		if (n == 1 && (!strcmp(a[0], "status") || !strcmp(a[0], "keep") || !strcmp(a[0], "revert"))) { char *s[] = { sc, a[0], NULL }; execve(sc, s, penv); return 127; }
		if (n == 3 && !strcmp(a[0], "set") && safe_word(a[1], 12) && safe_word(a[2], 4)) { char *s[] = { sc, "set", a[1], a[2], NULL }; execve(sc, s, penv); return 127; }
		return refuse("a screen setting that is not one", n ? a[0] : "");
	}
	if (!strcmp(op, "keyboard")) {
		if (n < 1 || n > 2 || !safe_word(a[0], 16) || (n == 2 && !safe_word(a[1], 24))) return refuse("a keyboard layout that is not one", n ? a[0] : "");
		char *env[] = { RR_PATH, "DISPLAY=:0", NULL };
		char *s1[] = { "setxkbmap", "-layout", a[0], NULL }, *s2[] = { "setxkbmap", "-layout", a[0], "-variant", n == 2 ? a[1] : "", NULL };
		return exec_cmd("setxkbmap", n == 2 ? s2 : s1, env);
	}
	if (!strcmp(op, "restart-display")) {
		if (n) return refuse("arguments for restart-display", a[0]);
		char *s[] = { "pkill", "-x", "rudolf-display", NULL };
		pid_t p = fork(); if (p == 0) { int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 1); dup2(nul, 2); } exec_cmd("pkill", s, penv); _exit(127); }
		if (p > 0) while (waitpid(p, NULL, 0) < 0 && errno == EINTR) {}
		return 0;   /* (as before: "; true") */
	}
	if (!strcmp(op, "restart-browser")) {
		if (n) return refuse("arguments for restart-browser", a[0]);
		char *s[] = { "pkill", "-f", "[-]-rudolf-kiosk", NULL }; return exec_cmd("pkill", s, penv);
	}
	return refuse("an unknown action", op);
}
