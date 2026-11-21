/*
 * rudolf-rootd - runs the root helpers for rudolfd, and only for it (Rudolf OS 1.6.1, docs/plan-1.6.1.md Part 11).
 *
 *   rudolf-rootd /usr/bin/rudolfd [ARG...]
 *
 * Started by rudolf-init as root. It makes a socket pair, starts the program it is given (rudolfd) with one end
 * (RUDOLF_ROOTD_FD, the descriptor's number) and keeps the other: nothing else on the computer can reach it, since
 * it listens on nothing (no socket in the file system, no port). For each request rudolfd makes a socket pair of its
 * own and passes one end over that first one; rudolf-rootd checks who made it (SO_PEERCRED: the user is root or
 * rudolf, uid 1000, and the process runs the program it started: /proc/PID/exe), reads one request on it ("RRQ1",
 * the helper's name, its arguments, and three descriptors for its standard input, output and error: rudolf-root.h),
 * runs /usr/lib/rudolf/root/NAME with those arguments (never through a shell), with nothing of rudolfd's
 * environment, and answers with its exit status. Unknown helpers and malformed requests are refused (254) and said
 * in the health log (Security). rudolf-rootd ends, with rudolfd's exit status, when rudolfd and every process of it
 * that holds its end have ended.
 *
 * Why not a socket in the file system checked by SO_PEERCRED and /proc/PID/exe: once rudolfd runs as uid 1000
 * (Part 11's last step), any program of that user could start its own copy of /usr/bin/rudolfd with LD_PRELOAD, or
 * trace one (it is the user's own, not set-user-ID), and that copy's /proc/PID/exe is rudolfd too. Here the right to
 * ask is the descriptor rudolfd was given at its start, before anything else ran in it; it is close-on-exec, so the
 * programs rudolfd starts never have it. Why not set-user-ID helpers: rudolfd could not then set no_new_privs, and a
 * set-user-ID program starts with whatever its caller gives it (environment, descriptors, limits, the working folder)
 * and can be run by every process of its group, the desktop's browser among them.
 *
 * Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include "rudolf-root.h"
#include "rudolf-confine.h"
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/time.h>

/* the helpers there are (each its own program in RR_DIR; Part 11's areas as they move) */
static const char *const HELPERS[] = { "drives", "updates", "accounts", "power", NULL };
static char prog_exe[PATH_MAX];
static pid_t prog_pid;

/* 1.6.1 (docs/plan-1.6.1.md Part 20): each helper narrower than rudolfd, behind the same switch as rudolfd's
   confinement (rudolfd-confine.inc: the build's RUDOLFD_CONFINE, /persist/etc/rudolf-confine, rudolf.confine=; read
   once at rudolf-rootd's start). Put on in the helper's own process just before it starts, so it holds for the helper
   and every script it runs, and for nothing else:
     - the capabilities it needs and no others (bounding, effective, permitted, inheritable: the scripts it runs as
       root get no more);
     - a deny list of system calls, rudolfd's own (CF_HELPER_DENY: kexec, BPF, performance counters, file handles by
       number, raw I/O ports, process accounting, quotas, another process's memory) and, for every helper, what no
       helper does: tracing another process, loading or removing kernel modules, restarting, setting the clock,
       swap.
   No Landlock: both helpers mount (the drives; rudolf-update a release's layer, to read its keys), which a Landlock
   domain refuses. In "log" mode the calls are logged, not refused, and the capabilities are kept. */
#ifndef RUDOLFD_CONFINE
#define RUDOLFD_CONFINE "off"
#endif
#include "rudolf-rootd-confine.h"   /* HELPER_CAPS, CF_HELPER_DENY */
static int confine_m = CF_OFF;
/* in the helper's process, before its exec: 0, or -1 when "on" could not be put on (the helper is then not started) */
static int helper_confine(const char *name)
{
	if (confine_m == CF_OFF) return 0;
	uint64_t keep = 0; int known = 0;
	for (int i = 0; HELPER_CAPS[i].name; i++) if (!strcmp(HELPER_CAPS[i].name, name)) { keep = HELPER_CAPS[i].keep; known = 1; }
	if (!known) return confine_m == CF_ON ? -1 : 0;   /* (a helper without its line here is never run unconfined with "on") */
	if (confine_m == CF_ON && cf_caps(keep)) return -1;
	int n = 0; while (CF_HELPER_DENY[n] >= 0) n++;
	return cf_seccomp(CF_HELPER_DENY, n, NULL, 0, 0, confine_m) && confine_m == CF_ON ? -1 : 0;
}
static int helper_known(const char *n) { for (int i = 0; HELPERS[i]; i++) if (!strcmp(n, HELPERS[i])) return 1; return 0; }
static void close_from(int lo)
{
#ifdef SYS_close_range
	if (!syscall(SYS_close_range, (unsigned)lo, ~0U, 0)) return;
#endif
	DIR *d = opendir("/proc/self/fd"); struct dirent *e; int me = d ? dirfd(d) : -1;
	while (d && (e = readdir(d))) { int f = atoi(e->d_name); if (f >= lo && f != me) close(f); }
	if (d) closedir(d); else for (int f = lo; f < 1024; f++) close(f);
}
/* who made this request's socket pair: root or the desktop's user, in a process running the program we started */
static int peer_ok(int ch, char *why, size_t wn)
{
	struct ucred uc; socklen_t l = sizeof uc;
	if (getsockopt(ch, SOL_SOCKET, SO_PEERCRED, &uc, &l) || l != sizeof uc) { snprintf(why, wn, "no credentials on the request"); return 0; }
	unsigned want = (unsigned)atoi(rr_env("RR_TEST_UID", "1000"));
	if (uc.uid != 0 && uc.uid != want) { snprintf(why, wn, "a request from uid %u", (unsigned)uc.uid); return 0; }
	char p[64], e[PATH_MAX]; snprintf(p, sizeof p, "/proc/%d/exe", (int)uc.pid);
	ssize_t k = readlink(p, e, sizeof e - 1); if (k < 0) { snprintf(why, wn, "a request from a process that is gone (%d)", (int)uc.pid); return 0; }
	e[k] = 0;
	if (strcmp(e, prog_exe)) { snprintf(why, wn, "a request from %.200s, not %.200s", e, prog_exe); return 0; }
	return 1;
}
static void answer(int ch, int st) { int32_t v = st; if (rr_send(ch, &v, sizeof v, NULL, 0)) {} }
/* one request: checked, run, answered (in a process of its own) */
static void serve(int ch)
{
	char why[480] = "";
	/* (the request read first, whoever sent it: refused before it was read, its sender could lose the answer, as a
	   socket closed with unread data resets the other end; at most 10 s to come) */
	struct timeval tv = { 10, 0 }; setsockopt(ch, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
	static char b[RR_MAXMSG + 1]; int fds[4], nfd = 0;
	ssize_t n = rr_recv(ch, b, RR_MAXMSG, fds, &nfd, 3);
	if (!peer_ok(ch, why, sizeof why)) { for (int i = 0; i < nfd; i++) close(fds[i]); rr_health("security", "rudolf-rootd refused a request: %s", why); answer(ch, RR_REFUSED); return; }
	char *argv[RR_MAXARGS + 2]; int ac = 0;
	if (n < 6 || nfd != 3 || memcmp(b, RR_MAGIC, 5) || b[n - 1]) { snprintf(why, sizeof why, "a malformed request (%zd bytes, %d descriptors)", n, nfd); goto refuse; }
	for (char *s = b + 5; s < b + n && ac <= RR_MAXARGS; s += strlen(s) + 1) {
		if (strlen(s) > RR_MAXARG) { snprintf(why, sizeof why, "an argument over %d bytes", RR_MAXARG); goto refuse; }
		argv[ac++] = s;
	}
	if (ac < 1 || ac > RR_MAXARGS) { snprintf(why, sizeof why, "%d arguments", ac); goto refuse; }
	argv[ac] = NULL;
	if (!rr_name_ok(argv[0]) || !helper_known(argv[0])) { snprintf(why, sizeof why, "an unknown helper (%.40s)", argv[0]); goto refuse; }
	char path[PATH_MAX]; snprintf(path, sizeof path, "%s/%s", rr_env("RR_TEST_DIR", RR_DIR), argv[0]);
	pid_t p = fork();
	if (p == 0) {
		for (int i = 0; i < 3; i++) if (dup2(fds[i], i) < 0) _exit(RR_NOSTART);
		close_from(3);
		sigset_t none; sigemptyset(&none); sigprocmask(SIG_SETMASK, &none, NULL);
		for (int s = 1; s < 32; s++) if (s != SIGKILL && s != SIGSTOP) signal(s, SIG_DFL);
		umask(022); if (chdir("/")) _exit(RR_NOSTART);
		char *env[] = { RR_PATH, "RUDOLF_ROOT_HELPER=1", NULL };
		if (helper_confine(argv[0])) _exit(RR_NOSTART);   /* (1.6.1 Part 20) */
		execve(path, argv, env); _exit(RR_NOSTART);
	}
	for (int i = 0; i < 3; i++) close(fds[i]);
	int st = 0; if (p < 0) { answer(ch, RR_NOSTART); return; }
	while (waitpid(p, &st, 0) < 0 && errno == EINTR) {}
	answer(ch, WIFEXITED(st) ? WEXITSTATUS(st) : WIFSIGNALED(st) ? 128 + WTERMSIG(st) : RR_NOSTART);
	return;
refuse:
	for (int i = 0; i < nfd; i++) close(fds[i]);
	rr_health("security", "rudolf-rootd refused a request: %s", why); answer(ch, RR_REFUSED);
}
static void pass_on(int s) { if (prog_pid > 0) kill(prog_pid, s); }

int main(int argc, char **argv)
{
	if (argc < 2 || argv[1][0] != '/') { fprintf(stderr, "usage: rudolf-rootd /PATH/TO/PROGRAM [ARG...]\n"); return 2; }
	if (!realpath(argv[1], prog_exe)) { fprintf(stderr, "rudolf-rootd: %s: %s\n", argv[1], strerror(errno)); return 127; }
	signal(SIGPIPE, SIG_IGN);
	confine_m = cf_mode_read(RUDOLFD_CONFINE, rr_env("RR_TEST_CONFINE_FILE", "/persist/etc/rudolf-confine"));   /* (1.6.1 Part 20) */
	int sv[2]; if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sv)) { perror("rudolf-rootd: socketpair"); return 1; }
	prog_pid = fork();
	if (prog_pid < 0) { perror("rudolf-rootd: fork"); return 1; }
	if (prog_pid == 0) {
		int f = fcntl(sv[1], F_DUPFD, 3); close(sv[0]); close(sv[1]);
		char v[16]; snprintf(v, sizeof v, "%d", f); setenv("RUDOLF_ROOTD_FD", v, 1);
		execv(argv[1], argv + 1); perror("rudolf-rootd: exec"); _exit(127);
	}
	close(sv[1]);
	struct sigaction sa = { .sa_handler = pass_on }; sigemptyset(&sa.sa_mask);
	sigaction(SIGTERM, &sa, NULL); sigaction(SIGINT, &sa, NULL); sigaction(SIGHUP, &sa, NULL);
	int prog_st = -1;
	for (;;) {
		pid_t w; int st;
		while ((w = waitpid(-1, &st, WNOHANG)) > 0) if (w == prog_pid) prog_st = st;
		struct pollfd pf = { sv[0], POLLIN, 0 };
		int r = poll(&pf, 1, 1000);
		if (r < 0 && errno != EINTR) break;
		if (r <= 0) continue;
		int fd[2], nfd = 0; char c[8];
		ssize_t n = rr_recv(sv[0], c, sizeof c, fd, &nfd, 1);
		if (n == 0 || (n < 0 && errno != EINTR && errno != EAGAIN)) break;   /* (rudolfd and all that held its end are gone) */
		if (nfd != 1) { for (int i = 0; i < nfd; i++) close(fd[i]); continue; }
		pid_t p = fork();
		if (p == 0) { close(sv[0]); serve(fd[0]); _exit(0); }
		close(fd[0]);
	}
	close(sv[0]);
	while (prog_st == -1) { int st; pid_t w = waitpid(-1, &st, 0); if (w < 0 && errno == ECHILD) break; if (w == prog_pid) prog_st = st; }
	while (waitpid(-1, NULL, 0) > 0) {}
	return prog_st == -1 ? 0 : WIFEXITED(prog_st) ? WEXITSTATUS(prog_st) : 128 + WTERMSIG(prog_st);
}
