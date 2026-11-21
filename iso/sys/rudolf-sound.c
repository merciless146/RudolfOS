/*
 * sound - the root helper for sound (Rudolf OS 1.6.1, docs/plan-1.6.1.md Part 11): /usr/lib/rudolf/root/sound, run by
 * rudolf-rootd for rudolfd (or by rudolfd itself while it is root and was not started by rudolf-rootd). The sound
 * server is the desktop user's: /usr/lib/rudolf/hotkey and /usr/lib/rudolf/sound reach it as that user (su).
 *
 *   sound volume get|up|down|mute     /usr/lib/rudolf/hotkey volume ACTION
 *   sound volume set N                N: up to three digits (hotkey volume set N keeps it to 0-100)
 *   sound volume muted 0|1
 *   sound playing                     hotkey playing (how many streams play)
 *   sound mic users|get               hotkey mic users|get (the apps recording; muted or on)
 *   sound mic muted 0|1               hotkey mic muted 0|1
 *   sound echo on|off|status          /usr/lib/rudolf/sound echo ... (the echo canceller)
 *   sound mics                        /usr/lib/rudolf/sound mics (the microphones)
 *   sound mic-default NAME            /usr/lib/rudolf/sound mic NAME (letters, digits and . _ : -, at most 200)
 *   sound default N                   /usr/lib/rudolf/sound default N (a card's number)
 *   sound restart                     /usr/lib/rudolf/sound restart
 *   sound card N                      /etc/asound.conf for card N (and /persist/etc/asound.conf on an installed one)
 *
 * echo on|off, mic-default and restart write what the script says into /run/rudolf/sound.log, as rudolfd did.
 * Everything is checked again here; the scripts are run with their arguments as they are, with no environment but
 * PATH. A refusal is said in the health log (Security).
 *
 * Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include "rudolf-root.h"
#include <ctype.h>

#define LIB     rr_env("RR_TEST_LIB", "/usr/lib/rudolf")
#define RUNDIR  rr_env("RR_TEST_RUN", "/run/rudolf")
#define ETC     rr_env("RR_TEST_ETC", "/etc")
#define PERSIST rr_env("RR_TEST_PERSIST", "/persist")
static int refuse(const char *what, const char *arg)
{
	char a[80]; size_t k = 0; for (const char *c = arg ? arg : ""; *c && k < sizeof a - 1; c++) a[k++] = isprint((unsigned char)*c) ? *c : '?';
	a[k] = 0;
	rr_health("security", "the sound helper refused %s (%s)", what, a);
	fprintf(stderr, "refused: %s\n", what); return 2;
}
/* a script of Rudolf OS's own in place of this helper; tolog: its output appended to RUN/sound.log */
static int exec_lib(const char *prog, char *const args[], int tolog)
{
	char path[PATH_MAX]; snprintf(path, sizeof path, "%s/%s", LIB, prog);
	char *argv[6]; int k = 0; argv[k++] = path; for (int i = 0; args[i] && k < 5; i++) argv[k++] = args[i]; argv[k] = NULL;
	if (tolog) { char lp[PATH_MAX]; snprintf(lp, sizeof lp, "%s/sound.log", RUNDIR); int f = open(lp, O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW, 0644); if (f >= 0) { dup2(f, 1); dup2(f, 2); if (f > 2) close(f); } }
	char *env[] = { RR_PATH, NULL };
	execve(path, argv, env); fprintf(stderr, "%s could not be started: %s\n", prog, strerror(errno)); return 127;
}
static int digits(const char *s, size_t max) { size_t n = s ? strlen(s) : 0; return n && n <= max && strspn(s, "0123456789") == n; }
static int is01(const char *s) { return s && (!strcmp(s, "0") || !strcmp(s, "1")); }
static int write_file(const char *dir, const char *name, const char *v)
{
	char p[PATH_MAX]; snprintf(p, sizeof p, "%s/%s", dir, name);
	int f = open(p, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0644); if (f < 0) return -1;
	ssize_t n = write(f, v, strlen(v)); close(f); return n == (ssize_t)strlen(v) ? 0 : -1;
}

int main(int argc, char **argv)
{
	if (argc < 2) { fprintf(stderr, "usage: sound volume|playing|mic|echo|mics|mic-default|default|restart|card ...\n"); return 2; }
	const char *op = argv[1]; int n = argc - 2; char **a = argv + 2;
	if (!strcmp(op, "volume")) {
		if (n == 1 && (!strcmp(a[0], "get") || !strcmp(a[0], "up") || !strcmp(a[0], "down") || !strcmp(a[0], "mute"))) { char *s[] = { "volume", a[0], NULL }; return exec_lib("hotkey", s, 0); }
		if (n == 2 && !strcmp(a[0], "set") && digits(a[1], 3)) { char *s[] = { "volume", "set", a[1], NULL }; return exec_lib("hotkey", s, 0); }
		if (n == 2 && !strcmp(a[0], "muted") && is01(a[1])) { char *s[] = { "volume", "muted", a[1], NULL }; return exec_lib("hotkey", s, 0); }
		return refuse("a volume that is not one", n ? a[0] : "");
	}
	if (!strcmp(op, "playing")) { if (n) return refuse("arguments for playing", a[0]); char *s[] = { "playing", NULL }; return exec_lib("hotkey", s, 0); }
	if (!strcmp(op, "mic")) {
		if (n == 1 && (!strcmp(a[0], "users") || !strcmp(a[0], "get"))) { char *s[] = { "mic", a[0], NULL }; return exec_lib("hotkey", s, 0); }
		if (n == 2 && !strcmp(a[0], "muted") && is01(a[1])) { char *s[] = { "mic", "muted", a[1], NULL }; return exec_lib("hotkey", s, 0); }
		return refuse("a microphone request that is not one", n ? a[0] : "");
	}
	if (!strcmp(op, "echo")) {
		if (n != 1 || (strcmp(a[0], "on") && strcmp(a[0], "off") && strcmp(a[0], "status"))) return refuse("an echo canceller setting that is not one", n ? a[0] : "");
		char *s[] = { "echo", a[0], NULL }; return exec_lib("sound", s, strcmp(a[0], "status") != 0);
	}
	if (!strcmp(op, "mics")) { if (n) return refuse("arguments for mics", a[0]); char *s[] = { "mics", NULL }; return exec_lib("sound", s, 0); }
	if (!strcmp(op, "mic-default")) {
		if (n != 1 || !*a[0] || strlen(a[0]) > 200 || strspn(a[0], "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._:-") != strlen(a[0])) return refuse("a microphone that is not one", n ? a[0] : "");
		char *s[] = { "mic", a[0], NULL }; return exec_lib("sound", s, 1);
	}
	if (!strcmp(op, "default")) {
		if (n != 1 || !digits(a[0], 9)) return refuse("a sound card that is not one", n ? a[0] : "");
		char *s[] = { "default", a[0], NULL }; return exec_lib("sound", s, 0);
	}
	if (!strcmp(op, "restart")) { if (n) return refuse("arguments for restart", a[0]); char *s[] = { "restart", NULL }; return exec_lib("sound", s, 1); }
	if (!strcmp(op, "card")) {
		if (n != 1 || !digits(a[0], 9)) return refuse("a sound card that is not one", n ? a[0] : "");
		char v[128]; snprintf(v, sizeof v, "defaults.pcm.card %s\ndefaults.ctl.card %s\n", a[0], a[0]);
		if (write_file(ETC, "asound.conf", v)) {}
		char d[PATH_MAX], pf[PATH_MAX]; snprintf(d, sizeof d, "%s/etc", PERSIST); mkdir(d, 0755);
		snprintf(pf, sizeof pf, "%s/persistent", RUNDIR); if (!access(pf, F_OK) && write_file(d, "asound.conf", v)) {}
		return 0;
	}
	return refuse("an unknown action", op);
}
