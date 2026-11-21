/* ------------------------------------------------------------- ZIP files kept on the disk (1.6.1)
 * The owner, 2026-11-20: a download larger than 1.5 MB stays in the account's folder on the disk (DLACCTS/<acct>/NAME) and
 * Downloads holds a reference to it, so a large ZIP could not be opened or extracted (Archive Manager and Extract here read
 * the archive into the page). Here, with dlzip.sh:
 *   GET  dl/zip?name=&acct=       what is inside: {"ok", "items":[{"name","size","dir"}], "more", "total", "bytes"}, at most
 *                                 20,000 entries ("more": there are others); read as the desktop user, never as root
 *   POST dl/unzip?name=&acct=     Extract here: a job (dl-unzip) that unpacks it as the desktop user and moves each file
 *                                 flat into the account's folder (dlzip.sh says how); POST dl/unzip?stop=1: Cancel
 *   GET  dl/unzipped              the last extraction's result, all of it (the job's log the page reads is only its end):
 *                                 {"ok","running","acct","dirs":[...],"files":[{"path","disk","size"}],"end"}
 *   POST dl/unzipped              the page has taken it (its references are saved): forgotten
 * While an extraction is running, or its result has not been taken (for at most 15 minutes), dl/home says "unzip" for that
 * account, so the page does not take the files it moves there in as downloads of their own (dl27.js dlAdopt).
 * The ZIP's name and the folder go to the script as arguments and in a root-only file, never in a command line. */
static char DLZIP_SH[200] = "/usr/lib/rudolf/dlzip.sh", DLZIP_OUT[200] = RUN "/dlzip.files", DLZIP_STOP[200] = RUN "/dlzip.stop",
	DLZIP_LIST[200] = RUN "/dlzip.list", DLZIP_ACCT[200] = RUN "/dlzip.acct";
/* the account's folder (hd) and the ZIP in it (zp) for the request's name and acct: an account's own folder only, a plain
   file there (not a link), the folders on the way none either; 0 after a reply */
static int dlzip_file(struct req *r, char *zp, size_t zn, char *hd, size_t hn, char *acct, size_t an)
{
	const char *name = qparam(r->query, "name"), *a = qparam(r->query, "acct");
	if (!a || !*a || !dl_acct_ok(a)) { reply_err(r->fd, 400, "which account?"); return 0; }
	if (!dl_name_ok(name)) { reply_err(r->fd, 400, "which file?"); return 0; }
	for (const unsigned char *c = (const unsigned char *)name; *c; c++) if (*c < 0x20 || *c == 0x7f) { reply_err(r->fd, 400, "which file?"); return 0; }
	if (!dl_acct_dir(a, hd, hn)) { reply_err(r->fd, 400, "which account?"); return 0; }
	snprintf(acct, an, "%s", a);
	if ((size_t)snprintf(zp, zn, "%s/%s", hd, name) >= zn) { reply_err(r->fd, 400, "which file?"); return 0; }
	/* canonical: the real path is the account's folder in the real accounts' folder, and the ZIP in it, nothing else */
	char rr[PATH_MAX], rz[PATH_MAX], want[PATH_MAX + 320];
	if (!realpath(DLACCTS, rr) || !realpath(zp, rz)) { reply_err(r->fd, 404, "the file is not there any more"); return 0; }
	snprintf(want, sizeof want, "%s/%s/%s", rr, a, name);
	if (strcmp(rz, want)) { reply_err(r->fd, 403, "only files in the account's Downloads"); return 0; }
	struct stat hs; if (lstat(hd, &hs) || !S_ISDIR(hs.st_mode)) { reply_err(r->fd, 403, "only files in the account's Downloads"); return 0; }
	int f = open(zp, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC); struct stat st;
	if (f < 0 || fstat(f, &st) || !S_ISREG(st.st_mode)) { if (f >= 0) close(f); reply_err(r->fd, 404, "the file is not there any more"); return 0; }
	close(f); return 1;
}
/* dlzip.sh list ZIP, its output (at most 16 MB, 120 s) */
static char *dlzip_list_out(const char *zp, int *st)
{
	const char *argv[] = { "/bin/sh", DLZIP_SH, "list", zp, NULL };
	int po[2]; *st = -1; if (pipe(po)) return NULL;
	signal(SIGCHLD, SIG_DFL);
	pid_t p = fork(); if (p < 0) { close(po[0]); close(po[1]); return NULL; }
	if (!p) { int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 0); dup2(nul, 2); } dup2(po[1], 1);
		for (int fd = 3; fd < 256; fd++) close(fd);
		setpgid(0, 0); execv(argv[0], (char *const *)argv); _exit(127); }
	close(po[1]);
	size_t cap = 1 << 16, n = 0; char *b = malloc(cap); time_t until = time(NULL) + 120; int stopped = 0;
	for (;;) {
		if (time(NULL) > until) { kill(-p, SIGKILL); stopped = 1; break; }
		struct pollfd pf = { po[0], POLLIN, 0 }; int k = poll(&pf, 1, 1000); if (k < 0 && errno != EINTR) break; if (k <= 0) continue;
		if (n + 16384 + 1 > cap) { if (cap >= (16u << 20)) { kill(-p, SIGKILL); break; } cap *= 2; char *nb = realloc(b, cap); if (!nb) break; b = nb; }
		ssize_t g = read(po[0], b + n, 16384); if (g == 0) break; if (g < 0) { if (errno == EINTR || errno == EAGAIN) continue; break; } n += g;
	}
	close(po[0]); int w = 0; waitpid(p, &w, 0); b[n] = 0;
	*st = stopped ? -1 : WIFEXITED(w) ? WEXITSTATUS(w) : -1;
	return b;
}
static int dlzip_job_running(void)
{
	if (!job_running()) return 0;
	char *n = slurp(RUN "/job.name", NULL); int yes = n && !strcmp(n, "dl-unzip"); free(n); return yes;
}
/* an extraction for acct running, or its result not taken yet (made in the last 15 minutes) */
static int dlzip_pending(const char *acct)
{
	if (!acct || !*acct) return 0;
	char *a = slurp(DLZIP_ACCT, NULL); if (!a) return 0; trim(a); int same = !strcmp(a, acct); free(a);
	if (!same) return 0;
	if (dlzip_job_running()) return 1;
	struct stat st; return !stat(DLZIP_OUT, &st) && time(NULL) - st.st_mtime < 900;
}
static void api_dl_zip(struct req *r, const char *what, int post)
{
	char zp[600], hd[300], acct[80];
	if (!strcmp(what, "zip")) {
		if (!dlzip_file(r, zp, sizeof zp, hd, sizeof hd, acct, sizeof acct)) return;
		int st; char *o = dlzip_list_out(zp, &st);
		if (!o || st == -1) { free(o); reply_err(r->fd, 504, "reading the archive took too long"); return; }
		if (!strncmp(o, "error ", 6) || strstr(o, "\nerror ")) { free(o); reply_err(r->fd, 422, "This file is not a ZIP archive, or it is damaged."); return; }
		str s = {0}; sadd(&s, "{\"ok\":true,\"items\":["); int n = 0, more = 0; long long tot = 0, bytes = 0;
		for (char *sv, *l = strtok_r(o, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) {
			if (!strncmp(l, "entry ", 6)) {
				char *e; long long sz = strtoll(l + 6, &e, 10); if (e == l + 6 || *e != ' ' || n >= 20000) continue;
				const char *nm = e + 1; size_t L = strlen(nm); if (!L) continue;
				sadd(&s, "%s{\"name\":", n ? "," : ""); sjson(&s, nm); sadd(&s, ",\"size\":%lld,\"dir\":%s}", sz, nm[L - 1] == '/' ? "true" : "false"); n++;
			} else if (!strcmp(l, "more")) more = 1;
			else if (!strncmp(l, "total ", 6)) sscanf(l + 6, "%lld %lld", &tot, &bytes);
		}
		free(o);
		sadd(&s, "],\"more\":%s,\"total\":%lld,\"bytes\":%lld}", more ? "true" : "false", tot, bytes); reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "unzip")) {
		if (!post) { reply_err(r->fd, 400, "use POST"); return; }
		char *sp = qparam(r->query, "stop");
		if (sp && !strcmp(sp, "1")) {
			if (!dlzip_job_running()) { reply_err(r->fd, 409, "no extraction is running"); return; }
			write_str(DLZIP_STOP, "1"); chmod(DLZIP_STOP, 0644); reply_ok(r->fd, "stopping"); return;
		}
		if (job_running()) { reply_err(r->fd, 409, "another task is still running, try again in a moment"); return; }
		if (!dlzip_file(r, zp, sizeof zp, hd, sizeof hd, acct, sizeof acct)) return;
		unlink(DLZIP_STOP); unlink(DLZIP_OUT); unlink(DLZIP_LIST);
		int f = open(DLZIP_LIST, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
		str l = {0}; sadd(&l, "%s\n%s\n", zp, hd);
		if (f < 0 || write(f, l.b, l.n) != (ssize_t)l.n) { if (f >= 0) close(f); free(l.b); reply_err(r->fd, 500, "could not write the request"); return; }
		close(f); free(l.b); write_str(DLZIP_ACCT, acct);
		char cmd[700]; snprintf(cmd, sizeof cmd, "/bin/sh %s extract %s", DLZIP_SH, DLZIP_LIST);   /* (fixed paths, no names) */
		start_job("dl-unzip", cmd); reply_ok(r->fd, "started"); return;
	}
	if (!strcmp(what, "unzipped")) {
		if (post) { unlink(DLZIP_OUT); unlink(DLZIP_ACCT); reply_ok(r->fd, "taken"); return; }
		char *o = slurp(DLZIP_OUT, NULL), *a = slurp(DLZIP_ACCT, NULL); if (a) trim(a);
		str s = {0}; sadd(&s, "{\"ok\":true,\"running\":%s,\"acct\":", dlzip_job_running() ? "true" : "false"); sjson(&s, a ? a : "");
		str dirs = {0}, files = {0}; sadd(&dirs, "%s", ""); sadd(&files, "%s", ""); int nd = 0, nf = 0; char *end = NULL;
		for (char *sv, *l = o ? strtok_r(o, "\n", &sv) : NULL; l; l = strtok_r(NULL, "\n", &sv)) {
			if (!strncmp(l, "dir ", 4)) { sadd(&dirs, "%s", nd++ ? "," : ""); sjson(&dirs, l + 4); }
			else if (!strncmp(l, "file ", 5)) {
				char *t1 = strchr(l + 5, '\t'), *t2 = t1 ? strchr(t1 + 1, '\t') : NULL; if (!t2) continue;
				*t1 = 0; *t2 = 0; if (!dl_name_ok(t1 + 1)) continue;
				sadd(&files, "%s{\"path\":", nf++ ? "," : ""); sjson(&files, l + 5); sadd(&files, ",\"disk\":"); sjson(&files, t1 + 1); sadd(&files, ",\"size\":%lld}", atoll(t2 + 1));
			} else if (!strncmp(l, "done ", 5) || !strncmp(l, "stopped ", 8) || !strncmp(l, "error ", 6)) end = l;
		}
		sadd(&s, ",\"dirs\":[%s],\"files\":[%s],\"end\":", dirs.b, files.b); sjson(&s, end ? end : ""); sadd(&s, "}");
		free(dirs.b); free(files.b); free(o); free(a); reply_json(r->fd, &s); return;
	}
	reply_err(r->fd, 404, "unknown download action");
}
