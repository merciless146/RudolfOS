/* (included by rudolfd-root.inc, not built on its own: hence .inc) ---------- 1.6.1 (docs/plan-1.6.1.md Part 11, power):
 * power and the clock through the power helper (rudolf-power.c: /usr/lib/rudolf/root/power)
 * Restart and shut down (the shutdown script, then reboot(2)), sleep (/sys/power/state) and its wake alarm, the power
 * settings (/usr/lib/rudolf/power), brightness (the backlight's file, hotkey's steps), the file cache dropped, the
 * desktop's priority, the clock (set, from the internet, the hardware clock). Each call site in rudolfd.c keeps its
 * old lines as they were for when the helper does not start (-2: not there, or refused by its confinement, plan Part
 * 20's list without it) and rudolfd is root, so restart and shut down always work; otherwise the helper's answer is
 * used the same way the old command's was. rudolfd's API is unchanged. */
/* the helper's status, -2 when it did not start and rudolfd is root (rudolfd then does it itself) */
static int root_power(const char *const argv[], char *out, size_t on)
{
	char o[256]; if (!out) { out = o; on = sizeof o; }
	int st = root_home_ask(argv, NULL, 0, out, on);
	if ((st < 0 || st == RR_NOSTART) && geteuid() == 0) { fprintf(stderr, "rudolfd: the power helper did not start: rudolfd does it itself\n"); return -2; }
	return st;
}
/* as root_out (its output, read while it runs), with its status in *st (-2 as above) */
static char *root_power_out(const char *const argv[], int err, int *st)
{
	int pf[2]; *st = -1; if (pipe2(pf, O_CLOEXEC)) return strdup("");
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
	int ws = 0; if (p > 0) while (waitpid(p, &ws, 0) < 0 && errno == EINTR) {}
	int c = p > 0 && WIFEXITED(ws) ? WEXITSTATUS(ws) : 255;
	*st = (c == 255 || c == RR_NOSTART) && geteuid() == 0 ? -2 : c;
	if (*st == -2) fprintf(stderr, "rudolfd: the power helper did not start: rudolfd does it itself\n");
	return s.b;
}
