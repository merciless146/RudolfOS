/* (included by rudolfd.c, not built on its own: hence .inc) ---------- 1.6.2 (docs/plan-1.6.2.md Part 34, step 2): the
 * Scan app's requests (desktop/scan62.js). rudolf-scan (iso/sys/rudolf-scan.c) does the work, never as root:
 *   GET  scan/find                     the scanners: the firewall's WSD window opened (the network helper's "firewall
 *                                      wsd open", ten seconds), then rudolf-scan find as the desktop's user (rudolf, 1000:
 *                                      ipp-usb's guard lets it reach the USB scanners' eSCL) {ok, scanners:[{id, name,
 *                                      model, via, proto}]}
 *   GET  scan/caps?id=ID               what it can do, as the account's own Linux user
 *   POST scan/start?id=ID&session=S[&source=flatbed|adf][&mode=color|grey|bw][&res=DPI][&area=L,T,W,H][&first=N][&preview=1]
 *                                      scans into the session's folder (~/.cache/rudolf-scan/S of the account's user, made
 *                                      by that user, 0700), as the account's own Linux user (the desktop's with nobody
 *                                      signed in), with no new privileges; answers when it is done (up to 15 minutes).
 *                                      A USB scanner (ipp-usb's address) is reached by that user only while it scans
 *                                      (usbprint's guard: the network helper's "usbscan allow|done UID"). One scan at a time.
 *   POST scan/cancel                   the running scan stopped (it keeps nothing of the page it was on)
 *   GET  scan/page?session=S&name=N    a page of the session (page-N.png, preview.png), read as that user
 *   POST scan/save?session=S&format=pdf|png|jpeg&pages=N,N,...[&to=REL]
 *                                      the pages into one file (rudolf-scan save, as that user): with to= into the account's
 *                                      own folder (REL: Documents/Scan 1.pdf, below one of its folders; never over a file;
 *                                      only while its files are on the disk, its session running) and {ok, path}; without,
 *                                      the file itself (the desktop keeps it where the person chose)
 *   POST scan/drop?session=S           the session's folder removed (by that user)
 * Every argument is checked here and again by rudolf-scan. The scan's process gets no descriptor or environment of
 * rudolfd's. */
#include <grp.h>
#include <pwd.h>
#include <sys/prctl.h>
#ifndef SCAN_BIN
#define SCAN_BIN "/usr/bin/rudolf-scan"
#endif
#define SCAND RUN "/scan"
static int scan_word(const char *s, size_t max, const char *more)
{
	size_t n = s ? strlen(s) : 0; if (!n || n > max) return 0;
	for (; *s; s++) if (!isalnum((unsigned char)*s) && !strchr(more, *s)) return 0;
	return 1;
}
/* a scanner's id as find gives it: escl:http(s)://ADDR:PORT/PATH, wsd:http://..., sane:BACKEND:DEVICE */
static int scan_id_ok(const char *id)
{
	if (!id || strlen(id) > 340 || strstr(id, "..")) return 0;
	if (!strncmp(id, "escl:http://", 12) || !strncmp(id, "escl:https://", 13) || !strncmp(id, "wsd:http://", 11)) return scan_word(strchr(id, ':') + 1, 320, ":/._-~%[]");
	if (!strncmp(id, "sane:", 5)) return scan_word(id + 5, 200, ":._-/");
	return 0;
}
/* who scans: the account's own Linux user (au_app_user: the desktop's with nobody signed in), never root */
static int scan_user(uid_t *u, gid_t *g, char *home, size_t hn)
{
	char au[32], ae[160]; au_app_user(au, sizeof au, ae, sizeof ae);
	struct passwd *pw = getpwnam(au);
	*u = pw ? pw->pw_uid : 1000; *g = pw ? pw->pw_gid : 1000; snprintf(home, hn, "%s", pw && pw->pw_dir ? pw->pw_dir : "/home/rudolf");
	return *u != 0 && *g != 0;
}
/* in a child: that user, no new privileges, nothing of rudolfd's; stdout to OUT */
static void scan_become(uid_t u, gid_t g, const char *home, int out)
{
	setsid();
	int nul = open("/dev/null", O_RDWR | O_CLOEXEC); dup2(nul >= 0 ? nul : 0, 0); dup2(out >= 0 ? out : nul, 1); dup2(nul >= 0 ? nul : 2, 2);
	for (int fd = 3; fd < 1024; fd++) close(fd);
	if (setgroups(0, NULL) || setgid(g) || setuid(u) || !getuid() || !geteuid()) _exit(126);
	prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	if (chdir(home)) { if (chdir("/tmp")) {} }
	char rt[64]; snprintf(rt, sizeof rt, "/run/user/%u", (unsigned)u); struct stat st;
	int has_rt = !stat(rt, &st) && S_ISDIR(st.st_mode) && st.st_uid == u;
	clearenv(); setenv("HOME", home, 1); setenv("LANG", "C.UTF-8", 1); setenv("PATH", "/usr/bin:/bin", 1); if (has_rt) setenv("XDG_RUNTIME_DIR", rt, 1);
	umask(022);
}
/* the session's folder, made (as the user, in the child) when MK */
static int scan_sess_dir(const char *home, const char *s, int mk, char *out, size_t on)
{
	char a[400]; snprintf(a, sizeof a, "%s/.cache", home); if (mk) mkdir(a, 0700);
	snprintf(a, sizeof a, "%s/.cache/rudolf-scan", home); if (mk) mkdir(a, 0700);
	snprintf(out, on, "%s/.cache/rudolf-scan/%s", home, s); if (mk && mkdir(out, 0700) && errno != EEXIST) return -1;
	return 0;
}
/* runs ARGV as the scanning user (in its session's folder when SESS), its output (at most MAX bytes) for MS
   milliseconds; PID_FILE: where its pid is kept while it runs (scan/cancel) */
static char *scan_run(const char *const argv[], const char *sess, size_t max, int ms, const char *pid_file, size_t *outn)
{
	uid_t u; gid_t g; char home[300]; if (!scan_user(&u, &g, home, sizeof home)) return NULL;
	int pf[2]; if (pipe2(pf, O_CLOEXEC)) return NULL;
	signal(SIGCHLD, SIG_DFL);
	pid_t p = fork();
	if (p == 0) {
		scan_become(u, g, home, pf[1]);
		char d[700]; if (sess && scan_sess_dir(home, sess, 1, d, sizeof d)) _exit(125);
		execv(argv[0], (char *const *)argv); _exit(127);
	}
	close(pf[1]); if (p < 0) { close(pf[0]); return NULL; }
	if (pid_file) { char b[24]; snprintf(b, sizeof b, "%d\n", (int)p); mkdir(SCAND, 0755); write_str(pid_file, b); }
	size_t cap = 65536 < max ? 65536 : max; char *bin = malloc(cap + 1); size_t bn = 0; time_t until = time(NULL) + ms / 1000;
	while (bin) {
		if (bn == cap) { if (cap >= max) break; size_t nc = cap * 2 < max ? cap * 2 : max; char *nb = realloc(bin, nc + 1); if (!nb) break; bin = nb; cap = nc; }
		struct pollfd q = { pf[0], POLLIN, 0 }; if (time(NULL) > until) break; int e = poll(&q, 1, 1000); if (e < 0 && errno != EINTR) break; if (e <= 0) continue;
		ssize_t n = read(pf[0], bin + bn, cap - bn); if (n <= 0) break; bn += n;
	}
	close(pf[0]); kill(p, SIGTERM); for (int i = 0; i < 20; i++) { if (waitpid(p, NULL, WNOHANG) == p) { p = 0; break; } usleep(50000); }
	if (p > 0) { kill(p, SIGKILL); waitpid(p, NULL, 0); }
	if (pid_file) unlink(pid_file);
	if (bin) bin[bn] = 0; if (outn) *outn = bn; return bin;
}
/* the network helper (or rudolfd itself while it is root and the helper does not start) */
static void scan_helper(const char *const a[], const char *cmd)
{
	if ((root_have("network") ? root_net(a, 1) : -2) == -2) run(cmd);
}
static int scan_usb_id(const char *id) { return !strncmp(id, "escl:http://127.0.0.1:600", 25); }
static void api_scan(struct req *r, const char *what, int post)
{
	if (access(SCAN_BIN, X_OK)) { reply_err(r->fd, 404, "this Rudolf OS cannot scan"); return; }
	if (!strcmp(what, "find")) {
		const char *fw[] = { "network", "firewall", "wsd", "open", NULL }; scan_helper(fw, "/usr/lib/rudolf/firewall wsd open >/dev/null 2>&1");
		/* (as the desktop's user: ipp-usb's guard lets it reach the USB scanners; never root) */
		struct passwd *pw = getpwnam("rudolf"); uid_t u = pw ? pw->pw_uid : 1000; gid_t g = pw ? pw->pw_gid : 1000;
		int pf[2]; if (pipe2(pf, O_CLOEXEC)) { reply_err(r->fd, 500, "cannot look"); return; }
		signal(SIGCHLD, SIG_DFL);
		pid_t p = fork();
		if (p == 0) { scan_become(u, g, "/tmp", pf[1]); char *av[] = { SCAN_BIN, "find", NULL }; execv(av[0], av); _exit(127); }
		close(pf[1]);
		str o = {0}; sadd(&o, "%s", ""); char b[4096]; time_t until = time(NULL) + 12;
		while (p > 0 && o.n < 65536) { struct pollfd q = { pf[0], POLLIN, 0 }; if (time(NULL) > until || poll(&q, 1, 1000) < 0) break; if (!q.revents) continue; ssize_t n = read(pf[0], b, sizeof b - 1); if (n <= 0) break; b[n] = 0; sadd(&o, "%s", b); }
		close(pf[0]); if (p > 0) { kill(p, SIGKILL); waitpid(p, NULL, 0); }
		if (!o.b || strncmp(o.b, "{\"ok\":", 6)) { free(o.b); reply_err(r->fd, 500, "Rudolf OS couldn't look for scanners."); return; }
		reply(r->fd, 200, "application/json", o.b, o.n, NULL); free(o.b); return;
	}
	if (!strcmp(what, "caps")) {
		const char *id = qparam(r->query, "id"); if (!scan_id_ok(id)) { reply_err(r->fd, 400, "id: one that find gave"); return; }
		char ib[350]; snprintf(ib, sizeof ib, "%s", id);
		uid_t u; gid_t g; char home[300]; scan_user(&u, &g, home, sizeof home); char us[16]; snprintf(us, sizeof us, "%u", (unsigned)u);
		int usb = scan_usb_id(ib) && u >= 1001;
		if (usb) { const char *a[] = { "network", "usbscan", "allow", us, NULL }; char c[120]; snprintf(c, sizeof c, "/usr/lib/rudolf/usbprint scanner allow %s >/dev/null 2>&1", us); scan_helper(a, c); }
		const char *av[] = { SCAN_BIN, "caps", ib, NULL }; size_t n = 0; char *o = scan_run(av, NULL, 65536, 60000, NULL, &n);
		if (usb) { const char *a[] = { "network", "usbscan", "done", us, NULL }; char c[120]; snprintf(c, sizeof c, "/usr/lib/rudolf/usbprint scanner done %s >/dev/null 2>&1", us); scan_helper(a, c); }
		if (!o || strncmp(o, "{\"ok\":", 6)) { free(o); reply_err(r->fd, 500, "The scanner could not be asked."); return; }
		reply(r->fd, 200, "application/json", o, n, NULL); free(o); return;
	}
	if (!post) { reply_err(r->fd, 405, "use POST"); return; }
	if (!strcmp(what, "cancel")) {
		char *p = slurp(SCAND "/pid", NULL); int v = p ? atoi(p) : 0; free(p);
		if (v > 1) { char f[64]; snprintf(f, sizeof f, "/proc/%d/comm", v); char *c = slurp(f, NULL); if (c) trim(c); if (c && !strcmp(c, "rudolf-scan")) kill(v, SIGTERM); free(c); }
		reply_ok(r->fd, "cancelled"); return;
	}
	const char *sq = qparam(r->query, "session"); if (!scan_word(sq, 16, "")) { reply_err(r->fd, 400, "session=S (letters and digits)"); return; }
	char sess[20]; snprintf(sess, sizeof sess, "%s", sq);
	uid_t u; gid_t g; char home[300]; if (!scan_user(&u, &g, home, sizeof home)) { reply_err(r->fd, 403, "not as root"); return; }
	char dir[700]; scan_sess_dir(home, sess, 0, dir, sizeof dir);
	if (!strcmp(what, "start")) {
		const char *id = qparam(r->query, "id"); if (!scan_id_ok(id)) { reply_err(r->fd, 400, "id: one that find gave"); return; }
		char ib[350]; snprintf(ib, sizeof ib, "%s", id);
		char *p = slurp(SCAND "/pid", NULL); int v = p ? atoi(p) : 0; free(p);
		if (v > 1 && !kill(v, 0)) { reply_err(r->fd, 409, "Another scan is running: wait for it to finish, or cancel it."); return; }
		static const char *const K[] = { "source", "mode", "res", "area", "first", "preview", NULL };
		const char *av[16]; char opt[6][64]; int n = 0; av[n++] = SCAN_BIN; av[n++] = "scan"; av[n++] = ib; av[n++] = dir;
		for (int i = 0; K[i]; i++) { const char *q = qparam(r->query, K[i]); if (!q || !*q) continue;
			if (!scan_word(q, 40, ".,")) { reply_err(r->fd, 400, "a setting that is not one"); return; }
			snprintf(opt[i], sizeof opt[i], "%s=%s", K[i], q); av[n++] = opt[i]; }
		av[n] = NULL;
		char us[16]; snprintf(us, sizeof us, "%u", (unsigned)u); int usb = scan_usb_id(ib) && u >= 1001;
		if (usb) { const char *a[] = { "network", "usbscan", "allow", us, NULL }; char c[120]; snprintf(c, sizeof c, "/usr/lib/rudolf/usbprint scanner allow %s >/dev/null 2>&1", us); scan_helper(a, c); }
		size_t on = 0; char *o = scan_run(av, sess, 65536, 15 * 60 * 1000, SCAND "/pid", &on);
		if (usb) { const char *a[] = { "network", "usbscan", "done", us, NULL }; char c[120]; snprintf(c, sizeof c, "/usr/lib/rudolf/usbprint scanner done %s >/dev/null 2>&1", us); scan_helper(a, c); }
		if (!o || strncmp(o, "{\"ok\":", 6)) { free(o); reply_err(r->fd, 500, "The scan stopped: Rudolf OS's scanning did not answer."); return; }
		reply(r->fd, 200, "application/json", o, on, NULL); free(o); return;   /* (an error of the scanner's is {"ok":false,...}: the page says it) */
	}
	if (!strcmp(what, "page")) {
		const char *nm = qparam(r->query, "name"); if (!scan_word(nm, 40, "-.") || strstr(nm, "..") || !strstr(nm, ".png")) { reply_err(r->fd, 400, "name: a page's"); return; }
		char f[800]; snprintf(f, sizeof f, "%s/%s", dir, nm);
		/* (read by that user: a link it made in its own folder leads nowhere it could not read itself) */
		const char *av[] = { "/bin/cat", f, NULL }; size_t n = 0; char *o = scan_run(av, NULL, 256u << 20, 60000, NULL, &n);
		if (!o || n < 8 || memcmp(o, "\x89PNG", 4)) { free(o); reply_err(r->fd, 404, "no such page"); return; }
		reply(r->fd, 200, "image/png", o, n, NULL); free(o); return;
	}
	if (!strcmp(what, "drop")) {
		const char *av[] = { "/bin/rm", "-rf", "--", dir, NULL }; size_t n; free(scan_run(av, NULL, 1024, 30000, NULL, &n)); reply_ok(r->fd, "dropped"); return;
	}
	if (!strcmp(what, "save")) {
		const char *fm = qparam(r->query, "format"); char fmt[8];
		if (!fm || (strcmp(fm, "pdf") && strcmp(fm, "png") && strcmp(fm, "jpeg"))) { reply_err(r->fd, 400, "format=pdf, png or jpeg"); return; }
		snprintf(fmt, sizeof fmt, "%s", fm);
		const char *pq = qparam(r->query, "pages"); if (!scan_word(pq, 1500, ",") || strspn(pq, "0123456789,") != strlen(pq)) { reply_err(r->fd, 400, "pages=N,N,..."); return; }
		char pages[1600]; snprintf(pages, sizeof pages, "%s", pq);
		const char *av[512]; static char names[500][720]; int n = 0; av[n++] = SCAN_BIN; av[n++] = "save"; av[n++] = fmt;
		const char *to = qparam(r->query, "to"); char out[1200];
		if (to && *to) {
			/* into the account's own folder: only below one of its folders, only while its session runs (its files on the disk) */
			char au[32], ae[160]; au_app_user(au, sizeof au, ae, sizeof ae);
			if (!strcmp(au, "rudolf")) { reply_err(r->fd, 409, "no account's files are on the disk"); return; }
			static const char *const TOP[] = { "Documents/", "Pictures/", "Desktop/", "Downloads/", "Music/", "Videos/", NULL }; int top = 0;
			for (int i = 0; TOP[i]; i++) if (!strncmp(to, TOP[i], strlen(TOP[i]))) top = 1;
			if (!top || strlen(to) > 400 || strstr(to, "..") || strstr(to, "//") || strchr(to, '\\') || to[strlen(to) - 1] == '/') { reply_err(r->fd, 400, "to: a file below the account's Documents, Pictures, Desktop or Downloads"); return; }
			for (const char *c = to; *c; c++) if ((unsigned char)*c < 32 || *c == 127) { reply_err(r->fd, 400, "to: a file's name"); return; }
			{ const char *b = strrchr(to, '/') + 1; if (*b == '.' || *b == '-') { reply_err(r->fd, 400, "to: a file's name"); return; } }
			snprintf(out, sizeof out, "%s/%s", home, to);
		} else snprintf(out, sizeof out, "%s/out.%s", dir, fmt);
		av[n++] = out;
		for (char *t = pages, *e; t && *t && n < 505; t = e) { e = strchr(t, ','); if (e) *e++ = 0; if (!*t) continue; int k = atoi(t); if (k < 1 || k > 999) { reply_err(r->fd, 400, "pages: 1 to 999"); return; }
			snprintf(names[n], sizeof names[n], "%s/page-%d.png", dir, k); av[n] = names[n]; n++; }
		av[n] = NULL; if (n < 5) { reply_err(r->fd, 400, "which pages"); return; }
		if (!to || !*to) { const char *rm[] = { "/bin/rm", "-f", "--", out, NULL }; size_t z; free(scan_run(rm, NULL, 256, 10000, NULL, &z)); }
		size_t on = 0; char *o = scan_run(av, NULL, 65536, 5 * 60 * 1000, NULL, &on);
		if (!o || strncmp(o, "{\"ok\":", 6)) { free(o); reply_err(r->fd, 500, "The file could not be made."); return; }
		if (strncmp(o, "{\"ok\":true", 10) || (to && *to)) {
			if (!strncmp(o, "{\"ok\":true", 10)) { str s = {0}; sadd(&s, "{\"ok\":true,\"path\":"); sjson(&s, to); sadd(&s, ",\"result\":%s}", o); free(o); reply_json(r->fd, &s); return; }
			reply(r->fd, 200, "application/json", o, on, NULL); free(o); return;
		}
		free(o);
		const char *cv[] = { "/bin/cat", out, NULL }; size_t fn = 0; char *f = scan_run(cv, NULL, 512u << 20, 120000, NULL, &fn);
		const char *rm[] = { "/bin/rm", "-f", "--", out, NULL }; size_t z; free(scan_run(rm, NULL, 256, 10000, NULL, &z));
		if (!f || !fn) { free(f); reply_err(r->fd, 500, "The file could not be read back."); return; }
		reply(r->fd, 200, !strcmp(fmt, "pdf") ? "application/pdf" : !strcmp(fmt, "png") ? "image/png" : "image/jpeg", f, fn, NULL); free(f); return;
	}
	reply_err(r->fd, 404, "unknown scan action");
}
