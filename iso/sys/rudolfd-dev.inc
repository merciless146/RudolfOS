/* 1.6.0 (docs/plan-1.6.0.md Part 19 item 4): Blitzen as a developer's agent. rudolfd's ai/dev/... for the desktop,
 * only while Developer mode is on (Settings › Security › Developer mode; dl_devmode). Each change (a push, a pull request, a workflow
 * started, a devlink command) is asked for by the page only after the person's Allow for that one (blzdev60.js).
 *   ai/dev/settings [?repo=OWNER/NAME&base=BRANCH&devlink=OWNER/NAME]   the account's repositories (names, not secrets)
 *   ai/dev/token?key=github|devlink   (POST, the token as the body; with clear=1: removed)   kept in the account's keyring
 *        (rudolf-portal --keyring-store, as its user; Settings › Privacy › Saved passwords for apps lists it), or, with
 *        no account keyring (no accounts, the guest), for root alone in /persist/os/dev
 *   ai/dev/repo | runs | jobs?run=N | joblog?job=N | workflows | file?path=P[&ref=B]     what the repository has
 *   ai/dev/dispatch?wf=W&ref=B     ai/dev/push?branch=B&message=M (body: PATH<tab>BASE64 a line)    ai/dev/pr?branch=B&title=T (body)
 *   ai/dev/dl-send (body: a command's JSON)   ai/dev/dl-wait?id=N&secs=S   ai/dev/dl-file?id=N&name=F   the devlink
 * The token is read from the keyring for each request and given to rudolf-dev (run as nobody) on its standard input:
 * never in an argument, a log or a reply. */
#ifndef DEV_BIN
#define DEV_BIN "/usr/bin/rudolf-dev"
#endif
#ifndef DEV_DIR
#define DEV_DIR PERSIST "/os/dev"
#endif
static int dev_name_ok(const char *v, int repo)
{
	if (!v || !*v || strlen(v) > 140 || strstr(v, "..") || v[0] == '/' || v[0] == '-') return 0;
	for (const char *c = v; *c; c++) if (!isalnum((unsigned char)*c) && !strchr("/._-", *c)) return 0;
	if (repo) { const char *s = strchr(v, '/'); if (!s || s == v || !s[1] || strchr(s + 1, '/')) return 0; }
	return 1;
}
/* ARGV run (as nobody when DROP), IN on its standard input: its standard output (at most 40 MB) and its last error line */
static size_t dev_len;   /* (the length of what dev_run last returned: it may hold zero bytes) */
static char *dev_run(const char *const *argv, const char *in, size_t inl, int secs, int drop, int *st, char *err, size_t en)
{
	int pi[2], po[2], pe[2]; *st = -1; if (err) *err = 0;
	if (pipe(pi)) return NULL; if (pipe(po)) { close(pi[0]); close(pi[1]); return NULL; } if (pipe(pe)) { close(pi[0]); close(pi[1]); close(po[0]); close(po[1]); return NULL; }
	signal(SIGCHLD, SIG_DFL);
	pid_t p = fork(); if (p < 0) { close(pi[0]); close(pi[1]); close(po[0]); close(po[1]); close(pe[0]); close(pe[1]); return NULL; }
	if (!p) { dup2(pi[0], 0); dup2(po[1], 1); dup2(pe[1], 2); for (int fd = 3; fd < 256; fd++) close(fd);
		setpgid(0, 0);
		if (drop && (setgroups(0, NULL) || setgid(65534) || setuid(65534))) _exit(126);
		execv(argv[0], (char *const *)argv); _exit(127); }
	close(pi[0]); close(po[1]); close(pe[1]);
	if (in && inl) { size_t off = 0; while (off < inl) { ssize_t w = write(pi[1], in + off, inl - off); if (w <= 0) break; off += w; } }
	close(pi[1]);
	str s = {0}; sadd(&s, "%s", ""); str e = {0}; sadd(&e, "%s", ""); char buf[16384]; time_t until = time(NULL) + secs; int stopped = 0, open_o = 1, open_e = 1;
	while (open_o || open_e) {
		struct pollfd pf[2] = { { open_o ? po[0] : -1, POLLIN, 0 }, { open_e ? pe[0] : -1, POLLIN, 0 } };
		if (time(NULL) > until) { kill(-p, SIGKILL); stopped = 1; break; }
		int k = poll(pf, 2, 1000); if (k < 0 && errno != EINTR) break; if (k <= 0) continue;
		for (int i = 0; i < 2; i++) if (pf[i].revents) { ssize_t g = read(i ? pe[0] : po[0], buf, sizeof buf - 1);
			if (g <= 0) { if (i) open_e = 0; else open_o = 0; continue; }
			str *t = i ? &e : &s;   /* (kept byte for byte: a screenshot has zero bytes in it) */
			if (t->n < (i ? 8000u : (40u << 20))) { if (t->n + g + 1 > t->cap) { t->cap = (t->n + g + 1) * 2; char *nb = realloc(t->b, t->cap); if (!nb) continue; t->b = nb; }
				memcpy(t->b + t->n, buf, g); t->n += g; t->b[t->n] = 0; } }
	}
	close(po[0]); close(pe[0]); int w = 0; waitpid(p, &w, 0);
	*st = stopped ? -1 : WIFEXITED(w) ? WEXITSTATUS(w) : -1;
	if (err) { char *t = e.b; size_t n = e.n; while (n && (t[n - 1] == '\n' || t[n - 1] == ' ')) t[--n] = 0; char *l = strrchr(t, '\n'); l = l ? l + 1 : t;
		if (!strncmp(l, "rudolf-dev: ", 12)) l += 12; snprintf(err, en, "%s", l); }
	free(e.b); dev_len = s.n; return s.b;
}
static int dev_conf(const char *acct, const char *key, char *out, size_t n)
{
	*out = 0; char p[300]; snprintf(p, sizeof p, DEV_DIR "/%s.conf", acct); FILE *f = fopen(p, "r"); if (!f) return 0; char l[400];
	while (fgets(l, sizeof l, f)) { l[strcspn(l, "\n")] = 0; size_t k = strlen(key); if (!strncmp(l, key, k) && l[k] == '=') snprintf(out, n, "%s", l + k + 1); }
	fclose(f); return *out != 0;
}
/* the token KEY (github, devlink) of the signed-in account: its keyring, or root's file without one */
static int dev_keyring(char *au, size_t an, char *ae, size_t en) { au_app_user(au, an, ae, en); return strcmp(au, "rudolf") != 0; }
static int dev_token(const char *acct, const char *key, char *tok, size_t n)
{
	*tok = 0; char au[32], ae[160]; int st;
	if (dev_keyring(au, sizeof au, ae, sizeof ae)) {
		char cmd[300]; snprintf(cmd, sizeof cmd, "set -a; . \"$0\"; set +a; exec " AU_PORTAL " --keyring-lookup %s", key);
		const char *a[] = { "/bin/su", "-s", "/bin/sh", au, "-c", cmd, ae, NULL };
		char *o = dev_run(a, NULL, 0, 20, 0, &st, NULL, 0); if (!st && o) { o[strcspn(o, "\r\n")] = 0; snprintf(tok, n, "%s", o); }
		if (o) { explicit_bzero(o, strlen(o)); free(o); }
		return *tok != 0;
	}
	char p[300]; snprintf(p, sizeof p, DEV_DIR "/%s.%s.token", acct, key); char *t = slurp(p, NULL); if (!t) return 0; t[strcspn(t, "\r\n")] = 0; snprintf(tok, n, "%s", t); explicit_bzero(t, strlen(t)); free(t);
	return *tok != 0;
}
static void dev_reply(struct req *r, char *o, int st, const char *err, int kind)
{
	/* kind 0: rudolf-dev's JSON as "data"; 1: text as "text"; 2: bytes as "base64" */
	if (st == 0 && o) {
		str s = {0};
		if (kind == 0) { sadd(&s, "{\"ok\":true,\"data\":"); sadd(&s, "%s", *o ? o : "null"); sadd(&s, "}"); }
		else if (kind == 1) { sadd(&s, "{\"ok\":true,\"text\":"); sjson(&s, o); sadd(&s, "}"); }
		else { static const char B[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; const unsigned char *d = (const unsigned char *)o; size_t n = dev_len;
			sadd(&s, "{\"ok\":true,\"base64\":\""); char q[5] = "";
			for (size_t i = 0; i < n; i += 3) { unsigned v = d[i] << 16 | (i + 1 < n ? d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
				q[0] = B[v >> 18 & 63]; q[1] = B[v >> 12 & 63]; q[2] = i + 1 < n ? B[v >> 6 & 63] : '='; q[3] = i + 2 < n ? B[v & 63] : '='; sadd(&s, "%s", q); }
			sadd(&s, "\"}"); }
		reply_json(r->fd, &s);
	} else reply_err(r->fd, st == 3 ? 401 : st == 4 ? 404 : st == 5 ? 409 : st == 2 ? 400 : st == -1 ? 504 : 502, err && *err ? err : "GitHub could not be asked");
	free(o);
}
static void api_ai_dev(struct req *r, const char *what)
{
	if (!dl_devmode()) { reply_err(r->fd, 403, "Blitzen's developer part is there only while Developer mode is on (Settings › Security › Developer mode)"); return; }
	char acct[80]; if (!know_acct(acct, sizeof acct)) snprintf(acct, sizeof acct, "rudolf");
	mkdir(PERSIST "/os", 0755); mkdir(DEV_DIR, 0700);
	char repo[160], base[160], dlr[160]; dev_conf(acct, "repo", repo, sizeof repo); dev_conf(acct, "base", base, sizeof base); dev_conf(acct, "devlink", dlr, sizeof dlr);
	if (!*base) snprintf(base, sizeof base, "main");
	if (!strcmp(what, "settings")) {
		const char *nr = qparam(r->query, "repo"), *nb = qparam(r->query, "base"), *nd = qparam(r->query, "devlink");
		if (nr || nb || nd) {
			if ((nr && *nr && !dev_name_ok(nr, 1)) || (nd && *nd && !dev_name_ok(nd, 1)) || (nb && *nb && !dev_name_ok(nb, 0))) { reply_err(r->fd, 400, "a repository is OWNER/NAME; a branch is letters, digits and / . _ -"); return; }
			if (nr) snprintf(repo, sizeof repo, "%s", nr);
			if (nb && *nb) snprintf(base, sizeof base, "%s", nb); if (nd) snprintf(dlr, sizeof dlr, "%s", nd);
			if (*repo && *dlr && !strcmp(repo, dlr)) { reply_err(r->fd, 400, "the devlink repository is a private one of its own, not the Rudolf OS repository"); return; }
			char p[300], c[600]; snprintf(p, sizeof p, DEV_DIR "/%s.conf", acct); snprintf(c, sizeof c, "repo=%s\nbase=%s\ndevlink=%s\n", repo, base, dlr);
			if (write_str(p, c)) { reply_err(r->fd, 409, "settings are kept only on an installed Rudolf OS"); return; }
		}
		char t[512]; int hg = dev_token(acct, "github", t, sizeof t); explicit_bzero(t, sizeof t); int hd = dev_token(acct, "devlink", t, sizeof t); explicit_bzero(t, sizeof t);
		char au[32], ae[160]; int kr = dev_keyring(au, sizeof au, ae, sizeof ae);
		str s = {0}; sadd(&s, "{\"ok\":true,\"developer\":true,\"repo\":"); sjson(&s, repo); sadd(&s, ",\"base\":"); sjson(&s, base); sadd(&s, ",\"devlink\":"); sjson(&s, dlr);
		sadd(&s, ",\"github\":%s,\"devlinkToken\":%s,\"keyring\":%s,\"official\":%s}", hg ? "true" : "false", hd ? "true" : "false", kr ? "true" : "false", !strcasecmp(repo, OFFICIAL_REPO) ? "true" : "false");
		reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "token")) {
		const char *key = qparam(r->query, "key"), *clr = qparam(r->query, "clear");
		if (!key || (strcmp(key, "github") && strcmp(key, "devlink"))) { reply_err(r->fd, 400, "key=github or key=devlink"); return; }
		char au[32], ae[160]; int st; char err[300];
		int clear = clr && !strcmp(clr, "1");
		char tok[512] = ""; if (!clear) { size_t n = r->blen < sizeof tok - 1 ? r->blen : sizeof tok - 1; if (r->body) memcpy(tok, r->body, n); tok[n] = 0; tok[strcspn(tok, "\r\n")] = 0;
			if (strlen(tok) < 20 || strspn(tok, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != strlen(tok)) { explicit_bzero(tok, sizeof tok); reply_err(r->fd, 400, "that is not a GitHub token (github_pat_… or ghp_…)"); return; } }
		if (dev_keyring(au, sizeof au, ae, sizeof ae)) {
			char cmd[300], label[200]; snprintf(cmd, sizeof cmd, "set -a; . \"$0\"; set +a; exec " AU_PORTAL " --keyring-%s %s \"$1\"", clear ? "clear" : "store", key);
			snprintf(label, sizeof label, "Blitzen: %s token for %s", !strcmp(key, "github") ? "GitHub" : "development link", !strcmp(key, "github") ? (*repo ? repo : "your repository") : (*dlr ? dlr : "your devlink"));
			const char *a[] = { "/bin/su", "-s", "/bin/sh", au, "-c", cmd, ae, label, NULL };
			size_t tl = strlen(tok); tok[tl] = '\n'; char *o = dev_run(a, tok, clear ? 0 : tl + 1, 20, 0, &st, err, sizeof err); explicit_bzero(tok, sizeof tok); free(o);
			if (st) { reply_err(r->fd, 409, *err ? err : "the keyring did not keep it (is it unlocked?)"); return; }
		} else {
			char p[300]; snprintf(p, sizeof p, DEV_DIR "/%s.%s.token", acct, key);
			if (clear) unlink(p); else { strcat(tok, "\n"); int bad = write_str(p, tok); chmod(p, 0600); explicit_bzero(tok, sizeof tok); if (bad) { reply_err(r->fd, 409, "it could not be kept"); return; } }
		}
		health_add("account", clear ? "a token was removed from Blitzen's developer settings" : "a token was kept for Blitzen's developer settings");
		reply_ok(r->fd, clear ? "removed" : "kept"); return;
	}
	/* the rest asks GitHub, with the account's token */
	int dl = !strncmp(what, "dl-", 3); const char *R = dl ? dlr : repo;
	if (!*R) { reply_err(r->fd, 409, dl ? "no development link repository is set (Settings › Apps › Blitzen › Developer)" : "no repository is set (Settings › Apps › Blitzen › Developer)"); return; }
	char tok[512]; if (!dev_token(acct, dl ? "devlink" : "github", tok, sizeof tok)) { reply_err(r->fd, 409, dl ? "no token for the development link is kept" : "no GitHub token is kept"); return; }
	str in = {0}; sadd(&in, "%s\n", tok); explicit_bzero(tok, sizeof tok);
	const char *a[10] = { DEV_BIN, what, R, NULL }; int kind = 0, secs = 60, st; char err[400]; char n1[24];
	const char *q1;
	if (!strcmp(what, "repo") || !strcmp(what, "runs") || !strcmp(what, "workflows")) ;
	else if (!strcmp(what, "jobs") && (q1 = qparam(r->query, "run")) && strspn(q1, "0123456789") == strlen(q1) && *q1) a[3] = q1;
	else if (!strcmp(what, "joblog") && (q1 = qparam(r->query, "job")) && strspn(q1, "0123456789") == strlen(q1) && *q1) { a[3] = q1; kind = 1; }
	else if (!strcmp(what, "file") && (q1 = qparam(r->query, "path"))) { a[3] = q1; a[4] = qparam(r->query, "ref"); kind = 1; }
	else if (!strcmp(what, "dispatch") && (q1 = qparam(r->query, "wf"))) { a[3] = q1; a[4] = qparam(r->query, "ref") ? qparam(r->query, "ref") : base; }
	else if (!strcmp(what, "push") && (q1 = qparam(r->query, "branch")) && qparam(r->query, "message")) { a[3] = base; a[4] = q1; a[5] = qparam(r->query, "message"); secs = 300; }
	else if (!strcmp(what, "pr") && (q1 = qparam(r->query, "branch")) && qparam(r->query, "title")) { a[3] = q1; a[4] = base; a[5] = qparam(r->query, "title"); }
	else if (!strcmp(what, "dl-send") && r->body && r->blen && r->blen < 60000) ;
	else if (!strcmp(what, "dl-wait") && (q1 = qparam(r->query, "id"))) { a[3] = q1; snprintf(n1, sizeof n1, "%d", qparam(r->query, "secs") && atoi(qparam(r->query, "secs")) > 0 && atoi(qparam(r->query, "secs")) <= 50 ? atoi(qparam(r->query, "secs")) : 25); a[4] = n1; }
	else if (!strcmp(what, "dl-file") && (q1 = qparam(r->query, "id")) && qparam(r->query, "name")) { a[3] = q1; a[4] = qparam(r->query, "name"); kind = 2; }
	else { explicit_bzero(in.b, in.n); free(in.b); reply_err(r->fd, 400, "no such request, or something it needs is missing"); return; }
	if (r->body && r->blen && (!strcmp(what, "push") || !strcmp(what, "pr") || !strcmp(what, "dl-send"))) {
		size_t at = in.n; if (in.n + r->blen + 2 > in.cap) { in.cap = in.n + r->blen + 2; in.b = realloc(in.b, in.cap); }
		memcpy(in.b + at, r->body, r->blen); in.n = at + r->blen; in.b[in.n++] = '\n'; in.b[in.n] = 0;
		if (!strcmp(what, "dl-send")) for (size_t i = at; i + 1 < in.n; i++) if (in.b[i] == '\n' || in.b[i] == '\r') in.b[i] = ' ';
	}
	char *o = dev_run(a, in.b, in.n, secs, 1, &st, err, sizeof err); explicit_bzero(in.b, in.n); free(in.b);
	if (st == 0 && (!strcmp(what, "push") || !strcmp(what, "pr") || !strcmp(what, "dispatch") || !strcmp(what, "dl-send")))
		health_add("account", !strcmp(what, "push") ? "Blitzen pushed a new branch to your repository (you allowed it)" : !strcmp(what, "pr") ? "Blitzen opened a pull request on your repository (you allowed it)"
			: !strcmp(what, "dispatch") ? "Blitzen started a workflow of your repository (you allowed it)" : "Blitzen sent a command to your development link (you allowed it)");
	dev_reply(r, o, st, err, kind);
}
