/* (included by rudolfd-root.inc, not built on its own: hence .inc) ---------- 1.6.1 (docs/plan-1.6.1.md Part 11, network):
 * the network through the network helper (rudolf-network.c: /usr/lib/rudolf/root/network)
 * Wi-Fi (wifi.sh: every action, the request to join or forget a network on the helper's standard input, never in a file
 * of rudolfd's), the connected network's name and signal (wpa_cli), an address by DHCP or a fixed one and the name
 * server, the firewall (on, off, its count, remote access's port), the hotspot (its request on standard input too) and
 * metered connections (netplug). Each call site in rudolfd.c keeps its old lines as they were for when the helper does
 * not start (-2: not there, or refused by its confinement) and rudolfd is root; otherwise the helper's answer is used
 * the same way the old command's was. rudolfd's API is unchanged. */
/* the helper's output (its standard output; with err 1 its errors too; err >= 2: that descriptor for its errors) and
   its status in *st, -2 when it did not start and rudolfd is root (rudolfd then does it itself). in: its standard
   input (at most 4096 bytes, NULL for none) */
static char *root_net_out(const char *const argv[], const char *in, size_t inl, int err, int *st)
{
	*st = -1; int pf[2], pi[2] = { -1, -1 };
	if (inl > 4096 || pipe2(pf, O_CLOEXEC)) return strdup("");
	if (pipe2(pi, O_CLOEXEC)) { close(pf[0]); close(pf[1]); return strdup(""); }
	int okin = !inl || write(pi[1], in, inl) == (ssize_t)inl; close(pi[1]);
	pid_t p = okin ? fork() : -1;
	if (p == 0) {
		close(pf[0]); int nul = root_null();
		int r = root_call(argv, pi[0], pf[1], err == 1 ? pf[1] : err >= 2 ? err : nul >= 0 ? nul : 2);
		_exit(r < 0 ? 255 : r & 255);
	}
	close(pi[0]); close(pf[1]);
	str s = {0}; sadd(&s, "%s", ""); char b[4096]; ssize_t r;
	if (p > 0) while ((r = read(pf[0], b, sizeof b - 1)) > 0 || (r < 0 && errno == EINTR)) if (r > 0) { b[r] = 0; sadd(&s, "%s", b); }
	close(pf[0]);
	int ws = 0; if (p > 0) while (waitpid(p, &ws, 0) < 0 && errno == EINTR) {}
	int c = p > 0 && WIFEXITED(ws) ? WEXITSTATUS(ws) : 255;
	*st = (c == 255 || c == RR_NOSTART) && geteuid() == 0 ? -2 : c;
	if (*st == -2) fprintf(stderr, "rudolfd: the %s helper did not start (%s): rudolfd does it itself\n", argv[0], argv[1]);
	return s.b;
}
/* as run() of the old command: the helper's status (-2 as above); its output into rudolfd's log, or with quiet nowhere */
static int root_net(const char *const argv[], int quiet)
{
	int nul = root_null(), st = root_call(argv, nul >= 0 ? nul : 0, quiet && nul >= 0 ? nul : 1, quiet && nul >= 0 ? nul : 2);
	if (nul >= 0) close(nul);
	if ((st < 0 || st == RR_NOSTART) && geteuid() == 0) { fprintf(stderr, "rudolfd: the %s helper did not start (%s): rudolfd does it itself\n", argv[0], argv[1]); return -2; }
	return st;
}
/* the output of a request the helper may do (NULL when it did not start, or there is no helper: the caller then runs
   its old command); err as root_net_out */
static char *root_net_try(const char *const argv[], int err)
{
	if (!root_have("network")) return NULL;
	int st; char *o = root_net_out(argv, NULL, 0, err, &st);
	if (st == -2) { free(o); return NULL; }
	return o;
}
/* Wi-Fi's join as a job (as start_job: job.name, job.log, job.status), the request on the helper's standard input.
   Should the helper not start, the old line: the request into fallback (rudolfd's root-only file) and cmd run. */
static void root_net_job(const char *name, const char *const argv[], const char *in, size_t inl, const char *fallback, const char *cmd)
{
	unlink(RUN "/job.status"); unlink(RUN "/job.log");
	write_str(RUN "/job.name", name);
	root_kmark();
	pid_t p = fork();
	if (p == 0) {
		setsid();
		if (fork() != 0) _exit(0);   /* (the job's own process, not rudolfd's child: nothing waits for it) */
		int nul = root_null(), lg = open(RUN "/job.log", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644), pi[2];
		int r = -1;
		if (!pipe2(pi, O_CLOEXEC)) {
			if (write(pi[1], in, inl) != (ssize_t)inl) {} close(pi[1]);
			r = root_call(argv, pi[0], lg >= 0 ? lg : 2, lg >= 0 ? lg : 2); close(pi[0]);
		}
		if ((r < 0 || r == RR_NOSTART) && geteuid() == 0) {
			if (lg >= 0) { if (ftruncate(lg, 0)) {} }
			int fd = (unlink(fallback), open(fallback, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600));
			if (fd >= 0) { if (write(fd, in, inl) != (ssize_t)inl) {} close(fd); }
			if (lg >= 0) { dup2(lg, 1); dup2(lg, 2); }
			int ws = system(cmd); r = WIFEXITED(ws) ? WEXITSTATUS(ws) : 127;
		}
		(void)nul;
		char v[16]; snprintf(v, sizeof v, "%d\n", r < 0 ? 127 : r); write_str(RUN "/job.status", v);
		_exit(0);
	}
	if (p > 0) while (waitpid(p, NULL, 0) < 0 && errno == EINTR) {}
}
/* GET net's Wi-Fi line: the connected network's name and signal into ss and rs (as trimmed before); 0 when done here */
static int root_net_link(const char *ifn, char **ss, char **rs)
{
	const char *av[] = { "network", "wifi-link", ifn, NULL };
	char *o = root_net_try(av, 0); if (!o) return -1;
	char *nl = strchr(o, '\n'); if (nl) *nl = 0;
	*ss = strdup(o); *rs = strdup(nl ? nl + 1 : ""); free(o);
	trim(*ss); trim(*rs); return 0;
}
/* System health's wifiNow: wifi.sh radios, then wifi.sh status's lines that say the state (as the old grep -E) */
static char *root_net_wifinow(void)
{
	const char *a1[] = { "network", "wifi", "radios", "", NULL }, *a2[] = { "network", "wifi", "status", "", NULL };
	char *r1 = root_net_try(a1, 0); if (!r1) return NULL;
	char *r2 = root_net_try(a2, 0); if (!r2) { free(r1); return NULL; }
	static const char *K[] = { "adapter=", "wlan=", "wpa_state=", "ssid=", "bssid=", "freq=", "RSSI=", "LINKSPEED=", "country=", NULL };
	str s = {0}; sadd(&s, "%s", r1);
	for (char *l = r2, *e; l && *l; l = e ? e + 1 : NULL) {
		e = strchr(l, '\n'); if (e) *e = 0;
		for (int k = 0; K[k]; k++) if (!strncmp(l, K[k], strlen(K[k]))) { sadd(&s, "%s\n", l); break; }
	}
	free(r1); free(r2); return s.b;
}
