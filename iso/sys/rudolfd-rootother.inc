/* (included by rudolfd-root.inc, not built on its own: hence .inc) ---------- 1.6.1 (docs/plan-1.6.1.md Part 11, other):
 * Task Manager's End process and End process tree through the procs helper (rudolf-procs.c), and the kernel's messages
 * through the logs helper (rudolf-logs.c: dmesg, dmesg -r). rudolfd still lists the processes and decides what to end
 * (the helper checks again what must keep running), and still picks the kernel's lines it wants with the same
 * commands as before, reading them from the helper instead of running dmesg. Each keeps its old way for when the
 * helper does not start (root_net_out of rudolfd-rootnet.inc: the same way of asking). The API is unchanged. */
/* the processes ended by the procs helper (tree: stopped first, then ended from the last; each one's errno, 0 when
   sent, into err): 0, or -2 when it did not start (rudolfd then does it itself), -1 when it refused */
static int root_procs_end(const int *pid, int n, int tree, int force, int *err)
{
	if (!root_have("procs") || n <= 0) return -2;
	str in = {0}; sadd(&in, "%s", ""); for (int i = 0; i < n; i++) sadd(&in, "%d\n", pid[i]);
	const char *av[] = { "procs", tree ? "tree" : "end", force ? "kill" : "term", NULL };
	int st = -1; char *o = NULL;
	if (in.n <= 4096) o = root_net_out(av, in.b, in.n, 0, &st);
	else {   /* (a long tree: its list from a file of rudolfd's own, unlinked at once, as the standard input) */
		char t[] = RUN "/procs.XXXXXX"; int f = mkostemp(t, O_CLOEXEC);
		if (f >= 0) { unlink(t); if (write(f, in.b, in.n) == (ssize_t)in.n && !lseek(f, 0, SEEK_SET)) {
			int pf[2]; if (!pipe2(pf, O_CLOEXEC)) { pid_t p = fork();
				if (p == 0) { close(pf[0]); int r = root_call(av, f, pf[1], 2); _exit(r < 0 ? 255 : r & 255); }
				close(pf[1]); str s = {0}; sadd(&s, "%s", ""); char b[4096]; ssize_t r;
				if (p > 0) while ((r = read(pf[0], b, sizeof b - 1)) > 0 || (r < 0 && errno == EINTR)) if (r > 0) { b[r] = 0; sadd(&s, "%s", b); }
				close(pf[0]); int ws = 0; if (p > 0) while (waitpid(p, &ws, 0) < 0 && errno == EINTR) {}
				int c = p > 0 && WIFEXITED(ws) ? WEXITSTATUS(ws) : 255; st = (c == 255 || c == RR_NOSTART) && geteuid() == 0 ? -2 : c; o = s.b; } }
			close(f); }
	}
	free(in.b);
	if (st == -2) { free(o); fprintf(stderr, "rudolfd: the procs helper did not start: rudolfd does it itself\n"); return -2; }
	if (st != 0 || !o) { free(o); return -1; }
	char *c = o; for (int i = 0; i < n; i++) { err[i] = c && *c ? atoi(c) : EPERM; c = c ? strchr(c, '\n') : NULL; if (c) c++; }
	free(o); return 0;
}
/* the shell pipeline "dmesg [-r] 2>/dev/null | REST" with the kernel's messages from the logs helper on REST's
   standard input; its output (never NULL). old: the pipeline as it was, run when the helper did not start */
static char *root_dmesg(int raw, const char *rest, const char *old)
{
	if (!root_have("logs")) return run_out(old);
	const char *av[] = { "logs", raw ? "dmesg-raw" : "dmesg", NULL }; int st;
	char *m = root_net_out(av, NULL, 0, 0, &st);
	if (st == -2) { free(m); return run_out(old); }
	int pi[2], po[2]; if (pipe2(pi, O_CLOEXEC)) { free(m); return strdup(""); }
	if (pipe2(po, O_CLOEXEC)) { close(pi[0]); close(pi[1]); free(m); return strdup(""); }
	pid_t w = fork();   /* (the messages written by a process of their own, while this one reads: neither waits on the other) */
	if (w == 0) { close(pi[0]); close(po[0]); close(po[1]); size_t n = strlen(m), k = 0; ssize_t r; while (k < n && ((r = write(pi[1], m + k, n - k)) > 0 || (r < 0 && errno == EINTR))) if (r > 0) k += (size_t)r; _exit(0); }
	pid_t p = fork();
	if (p == 0) { dup2(pi[0], 0); dup2(po[1], 1); int nul = open("/dev/null", O_WRONLY); if (nul >= 0) dup2(nul, 2); execl("/bin/sh", "sh", "-c", rest, (char *)NULL); _exit(127); }
	close(pi[0]); close(pi[1]); close(po[1]); free(m);
	str s = {0}; sadd(&s, "%s", ""); char b[4096]; ssize_t r;
	while ((r = read(po[0], b, sizeof b - 1)) > 0 || (r < 0 && errno == EINTR)) if (r > 0) { b[r] = 0; sadd(&s, "%s", b); }
	close(po[0]);
	if (w > 0) while (waitpid(w, NULL, 0) < 0 && errno == EINTR) {}
	if (p > 0) while (waitpid(p, NULL, 0) < 0 && errno == EINTR) {}
	return s.b;
}
/* a job's start: the kernel's last crash line now (RUN/job.kmark), so a crash during the job can be told apart */
#define KMARK_GREP "grep -E ': segfault at |: general protection|traps: '"
static void root_kmark(void)
{
	char *o = root_dmesg(0, KMARK_GREP " | tail -1 > " RUN "/job.kmark", "dmesg 2>/dev/null | " KMARK_GREP " | tail -1 > " RUN "/job.kmark");
	free(o);
}
