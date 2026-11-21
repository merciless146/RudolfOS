/*
 * procs - the root helper that ends processes for Task Manager (Rudolf OS 1.6.1, docs/plan-1.6.1.md Part 11):
 * /usr/lib/rudolf/root/procs, run by rudolf-rootd for rudolfd (or by rudolfd itself while it is root and was not
 * started by rudolf-rootd). Root is needed for the processes of the accounts' own Linux users (and the system's).
 *
 *   procs end term|kill        one process (its number on standard input) sent SIGTERM or SIGKILL
 *   procs tree term|kill       End process tree: the processes on standard input, one a line, each after its parent
 *                              (rudolfd's order); all stopped first, then ended from the last to the first, each let
 *                              go again (SIGCONT), as rudolfd did
 *
 * Standard output: one line for each process, 0 when it was sent the signal, else the error's number. The processes
 * Rudolf OS needs to keep running are checked again here, as rudolfd's Task Manager marks them (pid 1 and the kernel's,
 * rudolfd, rudolf-display, X, the session, rudolf-launch, the desktop's own browser), and this helper and the program
 * that started it: a list with one of them is refused as a whole and said in the health log (Security). At most 4096
 * processes; a number that is not one is refused.
 *
 * Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include "rudolf-root.h"
#include <ctype.h>
#include <signal.h>

#define PROCFS rr_env("RR_TEST_PROCS", "/proc")
#define MAXN 4096
static int refuse(const char *what, const char *arg)
{
	char a[80]; size_t k = 0; for (const char *c = arg ? arg : ""; *c && k < sizeof a - 1; c++) a[k++] = isprint((unsigned char)*c) ? *c : '?';
	a[k] = 0;
	rr_health("security", "the procs helper refused %s (%s)", what, a);
	fprintf(stderr, "refused: %s\n", what); return 2;
}
/* as rudolfd's read_proc and tag_procs: 1 when Rudolf OS needs this process (or it is gone: nothing to end) */
static int needed(int pid)
{
	char p[96], buf[1024], name[32] = "", cmd[400] = ""; int ppid = 0;
	if (pid <= 1 || pid == getpid() || pid == getppid()) return 1;
	snprintf(p, sizeof p, "%s/%d/stat", PROCFS, pid);
	int f = open(p, O_RDONLY | O_CLOEXEC); if (f < 0) return 0;   /* (gone: rudolfd's kill then said why) */
	ssize_t n = read(f, buf, sizeof buf - 1); close(f); if (n <= 0) return 0; buf[n] = 0;
	char *a = strchr(buf, '('), *b = strrchr(buf, ')'); if (!a || !b || b < a) return 1;
	size_t nl = (size_t)(b - a - 1); if (nl >= sizeof name) nl = sizeof name - 1; memcpy(name, a + 1, nl); name[nl] = 0;
	if (sscanf(b + 2, "%*c %d", &ppid) != 1) return 1;
	snprintf(p, sizeof p, "%s/%d/cmdline", PROCFS, pid);
	f = open(p, O_RDONLY | O_CLOEXEC);
	if (f >= 0) { n = read(f, cmd, sizeof cmd - 1); close(f); if (n > 0) { for (ssize_t i = 0; i < n; i++) if (!cmd[i]) cmd[i] = ' '; cmd[n] = 0; } }
	size_t L = strlen(cmd); while (L && isspace((unsigned char)cmd[L - 1])) cmd[--L] = 0;
	if (!*cmd || pid == 2 || ppid == 2) return 1;   /* the kernel's */
	if (!strcmp(name, "rudolfd") || !strcmp(name, "rudolf-display") || !strcmp(name, "Xorg") || !strcmp(name, "Xvfb") || strstr(cmd, "/usr/lib/rudolf/session")) return 1;
	if (!strcmp(name, "rudolf-launch") || !strcmp(name, "rudolf-rootd")) return 1;
	int chrome = strstr(cmd, "/chrome") != NULL || !strcmp(name, "chrome");
	if (chrome && strstr(cmd, "--rudolf-kiosk") && !strstr(cmd, "--type=")) return 1;
	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 3 || (strcmp(argv[1], "end") && strcmp(argv[1], "tree")) || (strcmp(argv[2], "term") && strcmp(argv[2], "kill"))) return refuse("an action that is not end or tree with term or kill", argc > 1 ? argv[1] : "");
	int tree = !strcmp(argv[1], "tree"), sig = strcmp(argv[2], "kill") ? SIGTERM : SIGKILL;
	static int pid[MAXN]; int n = 0; char l[32];
	FILE *in = fdopen(0, "r");
	while (in && fgets(l, sizeof l, in)) {
		size_t k = strlen(l); if (!k || l[k - 1] != '\n') return refuse("a process's number that is not one", l);
		l[--k] = 0; if (!k || k > 9 || strspn(l, "0123456789") != k || l[0] == '0') return refuse("a process's number that is not one", l);
		if (n >= MAXN) return refuse("more than 4096 processes", "");
		pid[n++] = atoi(l);
	}
	if (!n || (!tree && n != 1)) return refuse("a list of processes that is empty, or more than one to end", "");
	for (int i = 0; i < n; i++) if (needed(pid[i])) { char b[16]; snprintf(b, sizeof b, "%d", pid[i]); return refuse("ending a process Rudolf OS needs", b); }
	static int err[MAXN];
	if (!tree) { err[0] = kill(pid[0], sig) ? errno : 0; printf("%d\n", err[0]); return 0; }
	for (int i = 0; i < n; i++) kill(pid[i], SIGSTOP);
	for (int i = n; i-- > 0;) { err[i] = kill(pid[i], sig) ? errno : 0; kill(pid[i], SIGCONT); }
	for (int i = 0; i < n; i++) printf("%d\n", err[i]);
	return 0;
}
