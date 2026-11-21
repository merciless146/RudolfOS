/* 1.6.0 (docs/plan-1.6.0.md Part 18): Blitzen knows the system. rudolfd's ai/know/... runs rudolf-know (as root; it
 * reads the web as nobody) for the desktop:
 *   GET  ai/know/status                    what is kept, and whether this account's learning is on: {source, serial, logs, learn, ...}
 *   GET  ai/know/search?q=Q[&n=N]          the pieces that fit Q (the source, the logs, and this account's activity when its
 *                                          learning is on); the logs are read again first when older than a minute
 *   GET  ai/know/show?src=FILE&line=N      a source file around a line (a source Blitzen named)
 *   ai/know/learn                          POST ai/know/learn?on=1|0   "Learn from what I do on this computer" (per account, off at first)
 *   POST ai/know/activity                  body "KIND<tab>TEXT": one thing done, kept only while that account's learning is on
 *   GET  ai/know/list                      POST ai/know/forget?all=1 | day=YYYY-MM-DD | item=N
 *   GET  ai/know/web?q=Q[&tor=1]           a web search's first pages, as text (only when the person asked for it)
 *   ai/know/notes                          POST ai/know/notes?set=1 (body: the notes, a line each)   the account's notes for Blitzen
 * The account is the one signed in (rudolfd's own record, HOME_SESSION), never one the page names; without one (the
 * guest, or a computer without accounts on its disk) nothing is learned and only the source and logs are searched.
 * 1.6.1 (docs/plan-1.6.1.md Part 27 item 4): what is the person's (the activity, the notes, the setting itself) is kept only in
 * the account's own encrypted home folder (.rudolf/blitzen, its Linux user's, 0700), and only while it is open (the account
 * signed in): never in /persist/os/know, which is the system's (the source, the logs). An account whose files are not on the disk
 * (or locked) has none of it. 1.6.0's copy outside the folder is moved in the first time the folder is open; an account removed
 * (rudolfd-users.inc) has anything left of it outside its folder removed.
 * know_tick: the release's source is fetched and taken apart once per release, only while nobody has touched the
 * computer for 10 minutes (so never during a game), on mains power or a battery at 50 % or more, and not on a metered
 * connection. */
#ifndef KNOW_BIN
#define KNOW_BIN "/usr/bin/rudolf-know"
#endif
#ifndef KNOW_DIR
#define KNOW_DIR PERSIST "/os/know"
#endif
static int know_acct(char *a, size_t n)
{
	*a = 0; char *cur = slurp(HOME_SESSION, NULL); if (!cur) return 0; trim(cur);
	int ok = home_acct_ok(cur) && strcmp(cur, "guest") && strcmp(cur, "shared"); if (ok) snprintf(a, n, "%s", cur);
	free(cur); return ok;
}
/* the account's own folder, while it is open (its key added: signed in); 0 when it has none here */
static int know_home(const char *acct, char *h, size_t n)
{
	*h = 0; if (!*acct || !home_unlocked(acct)) return 0;
	snprintf(h, n, "%s/%s", home_root, acct); return 1;
}
static int know_learning(const char *acct) { char h[400], p[460]; if (!know_home(acct, h, sizeof h)) return 0; snprintf(p, sizeof p, "%s/.rudolf/blitzen/learn", h); return !access(p, F_OK); }
/* KNOW_BIN with these arguments (no shell: what the person typed stays one argument), IN on its standard input; its
 * standard output (at most 8 MB), and its exit status in *st (-1 when it was stopped after SECS seconds) */
static char *know_run(const char *const *args, const char *in, size_t inl, int secs, int *st)
{
	const char *argv[24]; int n = 0; argv[n++] = KNOW_BIN; for (int i = 0; args[i] && n < 23; i++) argv[n++] = args[i]; argv[n] = NULL;
	int pi[2], po[2]; *st = -1; if (pipe(pi)) return NULL; if (pipe(po)) { close(pi[0]); close(pi[1]); return NULL; }
	signal(SIGCHLD, SIG_DFL);
	pid_t p = fork(); if (p < 0) { close(pi[0]); close(pi[1]); close(po[0]); close(po[1]); return NULL; }
	if (!p) { dup2(pi[0], 0); dup2(po[1], 1); int nul = open("/dev/null", O_WRONLY); if (nul >= 0) dup2(nul, 2);
		for (int fd = 3; fd < 256; fd++) close(fd);
		setpgid(0, 0); execv(argv[0], (char *const *)argv); _exit(127); }
	close(pi[0]); close(po[1]);
	if (in && inl) { if (write(pi[1], in, inl) < 0) {} }
	close(pi[1]);
	str s = {0}; sadd(&s, "%s", ""); char buf[16384]; time_t until = time(NULL) + secs; int stopped = 0;
	fcntl(po[0], F_SETFL, O_NONBLOCK);
	for (;;) {
		struct pollfd pf = { po[0], POLLIN, 0 }; if (time(NULL) > until) { kill(-p, SIGKILL); stopped = 1; break; }
		int k = poll(&pf, 1, 1000); if (k < 0 && errno != EINTR) break; if (k <= 0) continue;
		ssize_t g = read(po[0], buf, sizeof buf - 1); if (g == 0) break; if (g < 0) { if (errno == EAGAIN || errno == EINTR) continue; break; }
		buf[g] = 0; if (s.n < (8u << 20)) sadd(&s, "%s", buf);
	}
	close(po[0]); int w = 0; waitpid(p, &w, 0);
	*st = stopped ? -1 : WIFEXITED(w) ? WEXITSTATUS(w) : -1;
	return s.b;
}
static void know_reply(struct req *r, char *o, int st, const char *fail)
{
	/* (as every reply of rudolfd's: an object with "ok"; a list as its "items") */
	if (st == 0 && o && *o == '[') { str s = {0}; sadd(&s, "{\"ok\":true,\"items\":%s}", o); reply_json(r->fd, &s); }
	else reply_err(r->fd, st == -1 ? 504 : 409, fail);
	free(o);
}
/* the logs read again when the kept copy is older than a minute (what failed a moment ago is in it) */
static void know_logs_fresh(void)
{
	struct stat st; if (!stat(KNOW_DIR "/logs.chunks", &st) && time(NULL) - st.st_mtime < 60) return;
	const char *a[] = { "build", NULL }; int x; free(know_run(a, NULL, 0, 30, &x));
}
static void api_ai_know(struct req *r, const char *what)
{
	char acct[80]; int has = know_acct(acct, sizeof acct), x;
	int post = !strcmp(r->method, "POST");
	char home[400]; int hh = has && know_home(acct, home, sizeof home);
	if (hh) { char l1[300], l2[300]; snprintf(l1, sizeof l1, KNOW_DIR "/acct/%s.log", acct); snprintf(l2, sizeof l2, KNOW_DIR "/acct/%s.on", acct);
		if (!access(l1, F_OK) || !access(l2, F_OK)) { const char *a[] = { "migrate", "--acct", acct, "--home", home, NULL }; free(know_run(a, NULL, 0, 30, &x)); } }   /* (1.6.0's copy, outside the folder) */
	if (!strcmp(what, "status")) {
		const char *a[] = { "status", NULL }; char *o = know_run(a, NULL, 0, 10, &x);
		if (x || !o || *o != '{') { know_reply(r, o, x, "Blitzen's knowledge is not set up on this computer"); return; }
		str s = {0}; o[strcspn(o, "\n")] = 0; size_t L = strlen(o); if (L && o[L - 1] == '}') o[L - 1] = 0;
		sadd(&s, "{\"ok\":true,%s,\"account\":%s,\"learn\":%s}", o + 1, has ? "true" : "false", know_learning(acct) ? "true" : "false"); free(o); reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "search")) {
		const char *q = qparam(r->query, "q"), *n = qparam(r->query, "n"); if (!q || !*q || strlen(q) > 1000) { reply_err(r->fd, 400, "search for what?"); return; }
		char nn[8]; snprintf(nn, sizeof nn, "%d", n && atoi(n) > 0 && atoi(n) <= 20 ? atoi(n) : 6);
		know_logs_fresh();
		const char *a1[] = { "search", "--home", home, "--n", nn, "--", q, NULL }, *a2[] = { "search", "--n", nn, "--", q, NULL };
		char *o = know_run(know_learning(acct) ? a1 : a2, NULL, 0, 20, &x); know_reply(r, o, x, "Blitzen could not look that up"); return;
	}
	if (!strcmp(what, "show")) {
		const char *f = qparam(r->query, "src"), *l = qparam(r->query, "line");
		if (!f || !*f || strlen(f) > 300 || strstr(f, "..") || !l || atoi(l) < 1) { reply_err(r->fd, 400, "src=FILE&line=N"); return; }
		char ln[16]; snprintf(ln, sizeof ln, "%d", atoi(l)); const char *a[] = { "show", "--", f, ln, NULL };
		char *o = know_run(a, NULL, 0, 15, &x); if (x) { reply_err(r->fd, 404, "that file is not among what Blitzen keeps"); free(o); return; }
		str s = {0}; sadd(&s, "{\"ok\":true,\"text\":"); sjson(&s, o ? o : ""); sadd(&s, "}"); free(o); reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "web")) {
		const char *q = qparam(r->query, "q"), *t = qparam(r->query, "tor"); if (!q || !*q || strlen(q) > 400) { reply_err(r->fd, 400, "search for what?"); return; }
		int tor = t && !strcmp(t, "1");
		const char *a[] = { "web", tor ? "--tor" : "--", tor ? "--" : q, tor ? q : NULL, NULL };
		char *o = know_run(a, NULL, 0, 90, &x); know_reply(r, o, x, tor ? "the search did not answer through Tor (is a Tor window open in Oracle?)" : "the search did not answer"); return;
	}
	/* what follows is about the signed-in account's own activity */
	if (!has) { reply_err(r->fd, 409, "Blitzen learns only for an account that is signed in (not the guest)"); return; }
	if (!hh && strcmp(what, "learn")) { reply_err(r->fd, 409, "Blitzen keeps what it learns only in your account's own folder on this computer's disk, and it is not open"); return; }
	if (!strcmp(what, "learn")) {
		const char *on = qparam(r->query, "on");   /* (without on: what it is now; the desktop asks everything by POST) */
		if (on) {
			if (!post || (strcmp(on, "1") && strcmp(on, "0"))) { reply_err(r->fd, 400, "on=1 or on=0"); return; }
			if (!hh) { if (*on == '1') { reply_err(r->fd, 409, "Blitzen keeps what it learns only in your account's own folder on this computer's disk, and it is not open"); return; } }
			else { const char *a[] = { "learn", "--home", home, "--", *on == '1' ? "on" : "off", NULL }; free(know_run(a, NULL, 0, 10, &x));
				if (x) { reply_err(r->fd, 409, "the setting could not be kept in your folder"); return; } }
		}
		str s = {0}; sadd(&s, "{\"ok\":true,\"learn\":%s}", know_learning(acct) ? "true" : "false"); reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "activity")) {
		if (!post || !r->body || !r->blen || r->blen > 2000) { reply_err(r->fd, 400, "KIND<tab>TEXT"); return; }
		if (!know_learning(acct)) { reply_ok(r->fd, "not kept: learning is off"); return; }
		char in[2100]; size_t L = r->blen; memcpy(in, r->body, L); in[L] = 0; for (size_t i = 0; i < L; i++) if (in[i] == '\n' || in[i] == '\r') in[i] = ' ';
		in[L] = '\n';
		const char *a[] = { "activity", "--home", home, NULL }; char *o = know_run(a, in, L + 1, 10, &x);
		if (x) { free(o); reply_err(r->fd, 400, "not kept"); return; } if (o) trim(o); reply_ok(r->fd, o && *o ? o : "kept"); free(o); return;
	}
	if (!strcmp(what, "notes")) {
		const char *set = qparam(r->query, "set");   /* (the desktop asks everything by POST: set=1 says the body is the notes) */
		if (post && set && !strcmp(set, "1")) { const char *a[] = { "notes", "--home", home, "--set", NULL }; char *o = know_run(a, r->body ? r->body : "", r->body ? r->blen : 0, 10, &x); if (o) trim(o);
			if (x || r->blen > 20000) reply_err(r->fd, x ? 409 : 400, "the notes could not be kept"); else reply_ok(r->fd, o && *o ? o : "kept"); free(o); return; }
		const char *a[] = { "notes", "--home", home, NULL }; char *o = know_run(a, NULL, 0, 10, &x); know_reply(r, o, x, "could not read them"); return;
	}
	if (!strcmp(what, "list")) { const char *a[] = { "list", "--home", home, NULL }; char *o = know_run(a, NULL, 0, 15, &x); know_reply(r, o, x, "could not read it"); return; }
	if (!strcmp(what, "forget") && post) {
		const char *all = qparam(r->query, "all"), *day = qparam(r->query, "day"), *item = qparam(r->query, "item"); char v[16];
		const char *a[] = { "forget", "--home", home, "--acct", acct, NULL, NULL, NULL };
		if (all && !strcmp(all, "1")) a[5] = "--all";
		else if (day && strlen(day) == 10 && day[4] == '-' && day[7] == '-' && strspn(day, "0123456789-") == 10) { a[5] = "--day"; snprintf(v, sizeof v, "%s", day); a[6] = v; }
		else if (item && atoi(item) > 0) { a[5] = "--item"; snprintf(v, sizeof v, "%d", atoi(item)); a[6] = v; }
		else { reply_err(r->fd, 400, "all=1, day=YYYY-MM-DD or item=N"); return; }
		char *o = know_run(a, NULL, 0, 15, &x); if (o) trim(o); if (x) reply_err(r->fd, 409, "could not forget it"); else reply_ok(r->fd, o && *o ? o : "forgotten"); free(o); return;
	}
	reply_err(r->fd, 404, "no such request");
}
/* (1.6.1 Part 27) an account removed (rudolfd-users.inc au_sync_ids): anything of it outside its folder (1.6.0's copy) removed */
static void know_acct_gone(const char *acct)
{
	if (!home_acct_ok(acct) || access(KNOW_BIN, X_OK)) return; int x; const char *a[] = { "gone", "--acct", acct, NULL }; free(know_run(a, NULL, 0, 10, &x));
}
/* once a minute: the release's source fetched and taken apart (once per release) while the computer is idle */
static int know_power_ok(void)
{
	DIR *d = opendir("/sys/class/power_supply"); if (!d) return 1;   /* (no battery and no adapter known: a desktop) */
	struct dirent *e; int mains = 0, bat = 0, cap = 100; char p[400];
	while ((e = readdir(d))) { if (e->d_name[0] == '.') continue;
		snprintf(p, sizeof p, "/sys/class/power_supply/%s/type", e->d_name); char *t = slurp(p, NULL); if (!t) continue; trim(t);
		if (!strcmp(t, "Mains")) { snprintf(p, sizeof p, "/sys/class/power_supply/%s/online", e->d_name); if (read_ll(p, 0) == 1) mains = 1; }
		else if (!strcmp(t, "Battery")) { snprintf(p, sizeof p, "/sys/class/power_supply/%s/scope", e->d_name); char *sc = slurp(p, NULL);
			if (!sc || strncmp(sc, "Device", 6)) { bat = 1; snprintf(p, sizeof p, "/sys/class/power_supply/%s/capacity", e->d_name); long long c = read_ll(p, 100); if (c < cap) cap = (int)c; }
			free(sc); }
		free(t); }
	closedir(d);
	return mains || !bat || cap >= 50;
}
static pid_t know_pid;
static void know_tick(void)
{
	static time_t last; time_t now = time(NULL); if (now - last < 60) return; last = now;
	if (know_pid > 0) { int w; pid_t g = waitpid(know_pid, &w, WNOHANG); if (g == know_pid || (g < 0 && errno == ECHILD)) { know_pid = 0; write_str(RUN "/know.tried", "1\n"); } else return; }
	if (access(KNOW_BIN, X_OK) || access(AI_BIN, X_OK) || access(KNOW_DIR, F_OK)) return;   /* (only where Blitzen is, once its knowledge has been opened or asked) */
	struct stat st; if (!stat(RUN "/know.tried", &st) && now - st.st_mtime < 6 * 3600) return;
	long long in = read_ll(RUN "/input-ms", -1); struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
	if (in < 0 || t.tv_sec * 1000LL + t.tv_nsec / 1000000 - in < 600000) return;
	if (!know_power_ok()) return;
	const char *mav[] = { "network", "metered-state", NULL }; char *m = root_net_try(mav, 0); if (!m) m = run_out("/usr/lib/rudolf/netplug state 2>/dev/null");   /* (1.6.1 Part 11) */
	int metered = m && strstr(m, "\"metered\":true"); free(m); if (metered) return;
	pid_t p = fork(); if (p < 0) return;
	if (!p) { int nul = open("/dev/null", O_RDWR); dup2(nul, 0); dup2(nul, 1); dup2(nul, 2); for (int fd = 3; fd < 256; fd++) close(fd);
		setsid(); if (nice(19) == -1) {} syscall(SYS_ioprio_set, 1, 0, 3 << 13);   /* (the idle class: it never slows anything else) */
		execl(KNOW_BIN, KNOW_BIN, "build", "--source", (char *)NULL); _exit(127); }
	know_pid = p;
}
