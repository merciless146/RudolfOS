/* (included by rudolfd.c, not built on its own: hence .inc) ---------- 1.6.1 (docs/plan-1.6.1.md Part 11): root work
 * through the root helpers
 * What needs root is done by small helpers in /usr/lib/rudolf/root (rudolf-root.h), which check their arguments again
 * themselves. Started by rudolf-rootd (rudolf-init does), rudolfd has a descriptor to it (RUDOLF_ROOTD_FD, made
 * close-on-exec here so that nothing rudolfd starts has it) and asks it to run them: root_call. Started without it
 * (an older rudolf-init, the QEMU tests that start rudolfd again by hand) and running as root, rudolfd runs the helper
 * itself, the same way (no shell, nothing of its environment). Neither: the helper cannot run (-1), as a failed
 * command was before.
 *   root_call(argv, in, out, err)   argv[0] the helper's name; its exit status (as run() gave a command's), -1
 *   root_run(argv)                  its output into rudolfd's log, as run() of a command without redirections
 *   root_out(argv, err)             its output (and its errors with err=1) as a string, as run_out(); never NULL
 *   root_job(name, argv)            as start_job(): job.name, job.log, job.status, in the background */
#include "rudolf-root.h"
#ifndef RD_ROOT_DIR
#define RD_ROOT_DIR RR_DIR   /* (host tests build rudolfd with another) */
#endif
static int root_fd = -1;
static void root_init(void)
{
	const char *v = getenv("RUDOLF_ROOTD_FD");
	if (v && *v) { int f = atoi(v); if (f > 2 && fcntl(f, F_SETFD, FD_CLOEXEC) == 0) root_fd = f; }
	unsetenv("RUDOLF_ROOTD_FD");
	fprintf(stderr, "rudolfd: root helpers %s\n", root_fd >= 0 ? "through rudolf-rootd" : geteuid() ? "unavailable (not root, no rudolf-rootd)" : "run by rudolfd itself (no rudolf-rootd)");
}
static int root_call(const char *const argv[], int in, int out, int err)
{
	char msg[RR_MAXMSG]; size_t n = 5; memcpy(msg, RR_MAGIC, 5);
	int ac = 0;
	for (; argv[ac]; ac++) { size_t L = strlen(argv[ac]) + 1; if (ac >= RR_MAXARGS || L > RR_MAXARG || n + L > sizeof msg) return -1; memcpy(msg + n, argv[ac], L); n += L; }
	if (!ac || !rr_name_ok(argv[0])) return -1;
	if (root_fd >= 0) {
		int sp[2]; if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sp)) return -1;
		int ok = !rr_send(root_fd, "c", 1, &sp[1], 1); close(sp[1]);
		int fds[3] = { in, out, err };
		if (ok) ok = !rr_send(sp[0], msg, n, fds, 3);
		int32_t st = -1; ssize_t r = -1;
		if (ok) do r = recv(sp[0], &st, sizeof st, 0); while (r < 0 && errno == EINTR);
		close(sp[0]);
		if (r != (ssize_t)sizeof st) { fprintf(stderr, "rudolfd: rudolf-rootd did not answer for %s\n", argv[0]); return -1; }
		if (st == RR_REFUSED) fprintf(stderr, "rudolfd: rudolf-rootd refused to run %s\n", argv[0]);
		return st;
	}
	if (geteuid() != 0) return -1;
	char path[PATH_MAX]; snprintf(path, sizeof path, "%s/%s", RD_ROOT_DIR, argv[0]);
	pid_t p = fork();
	if (p < 0) return -1;
	if (p == 0) {
		int fd[3] = { in, out, err };
		for (int i = 0; i < 3; i++) { int d = fcntl(fd[i], F_DUPFD_CLOEXEC, 10); if (d < 0) _exit(RR_NOSTART); fd[i] = d; }
		for (int i = 0; i < 3; i++) if (dup2(fd[i], i) < 0) _exit(RR_NOSTART);
		for (int f = 3; f < 1024; f++) close(f);
		sigset_t none; sigemptyset(&none); sigprocmask(SIG_SETMASK, &none, NULL);
		for (int s = 1; s < 32; s++) if (s != SIGKILL && s != SIGSTOP) signal(s, SIG_DFL);
		umask(022); if (chdir("/")) _exit(RR_NOSTART);
		char *env[] = { RR_PATH, "RUDOLF_ROOT_HELPER=1", NULL };
		execve(path, (char *const *)argv, env); _exit(RR_NOSTART);
	}
	int st = 0; while (waitpid(p, &st, 0) < 0) if (errno != EINTR) return -1;
	return WIFEXITED(st) ? WEXITSTATUS(st) : WIFSIGNALED(st) ? 128 + WTERMSIG(st) : -1;
}
static int root_null(void) { return open("/dev/null", O_RDWR | O_CLOEXEC); }
static int root_run(const char *const argv[])
{
	int nul = root_null(); int r = root_call(argv, nul >= 0 ? nul : 0, 1, 2); if (nul >= 0) close(nul); return r;
}
/* (in a process of its own, which hands the status on, while this one reads: a long output never fills the pipe) */
static char *root_out(const char *const argv[], int err)
{
	int pf[2]; if (pipe2(pf, O_CLOEXEC)) return strdup("");
	pid_t p = fork();
	if (p == 0) {
		close(pf[0]); int nul = root_null();
		int r = root_call(argv, nul >= 0 ? nul : 0, pf[1], err ? pf[1] : nul >= 0 ? nul : 2);
		_exit(r < 0 ? 255 : r & 255);
	}
	close(pf[1]);
	str s = {0}; sadd(&s, "%s", ""); char b[4096]; ssize_t r;
	if (p > 0) while ((r = read(pf[0], b, sizeof b - 1)) > 0 || (r < 0 && errno == EINTR)) if (r > 0) { b[r] = 0; sadd(&s, "%s", b); }
	close(pf[0]);
	if (p > 0) while (waitpid(p, NULL, 0) < 0 && errno == EINTR) {}
	return s.b;
}
static void root_job(const char *name, const char *const argv[])
{
	unlink(RUN "/job.status"); unlink(RUN "/job.log");
	write_str(RUN "/job.name", name);
	run("dmesg 2>/dev/null | grep -E ': segfault at |: general protection|traps: ' | tail -1 > " RUN "/job.kmark");
	pid_t p = fork();
	if (p == 0) {
		setsid();
		if (fork() != 0) _exit(0);   /* (the job's own process, not rudolfd's child: nothing waits for it) */
		int nul = root_null(), lg = open(RUN "/job.log", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
		int r = root_call(argv, nul >= 0 ? nul : 0, lg >= 0 ? lg : 2, lg >= 0 ? lg : 2);
		char v[16]; snprintf(v, sizeof v, "%d\n", r < 0 ? 127 : r); write_str(RUN "/job.status", v);
		_exit(0);
	}
	if (p > 0) while (waitpid(p, NULL, 0) < 0 && errno == EINTR) {}
}
#include "rudolfd-roothome.inc"   /* (drives part 2: the accounts' folders and sessions) */
#include "rudolfd-rootpower.inc"  /* (power and the clock) */
#include "rudolfd-rootnet.inc"    /* (the network) */
