/* ------------------------------------------------------------- ZIP files kept on the disk (1.6.1)
 * The owner, 2026-11-20: a download larger than 1.5 MB stays in the account's folder on the disk (DLACCTS/<acct>/NAME) and
 * Downloads holds a reference to it, so a large ZIP could not be opened or extracted (Archive Manager and Extract here read
 * the archive into the page). Here, with dlzip.sh:
 *   GET  dl/zip?name=&acct=       what is inside: {"ok", "items":[{"name","size","dir"}], "more", "total", "bytes"}, at most
 *                                 20,000 entries ("more": there are others); read as the desktop user, never as root
 *   POST dl/unzip?name=&acct=     Extract here: a job (dl-unzip) that unpacks it as the desktop user and moves each file
 *                                 flat into the account's folder (dlzip.sh says how); POST dl/unzip?stop=1: Cancel
 *   1.6.3 Part 10, item 1: either request takes path=/an/absolute/file.zip instead of name= and acct= -- any ZIP at a path
 *   the account may read (its home, a drive's, /tmp: dlzip_path_ok), and then the job unpacks it as the desktop user into a
 *   free folder beside it, named after it ("game", "game (2)"), with nothing moved anywhere else. The listing is the same
 *   answer; the result says where the folder is ("dest"). No account is marked as unzipping then, as nothing of a Downloads
 *   folder changes.
 *   GET  dl/unzipped              the last extraction's result, all of it (the job's log the page reads is only its end):
 *                                 {"ok","running","acct","dest","dirs":[...],"files":[{"path","disk","size"}],"end"}
 *   POST dl/unzipped              the page has taken it (its references are saved): forgotten
 * While an extraction is running, or its result has not been taken (for at most 15 minutes), dl/home says "unzip" for that
 * account, so the page does not take the files it moves there in as downloads of their own (dl27.js dlAdopt).
 * The ZIP's name and the folder go to the script as arguments and in a root-only file, never in a command line. */
static char DLZIP_SH[200] = "/usr/lib/rudolf/dlzip.sh", DLZIP_OUT[200] = RUN "/dlzip.files", DLZIP_STOP[200] = RUN "/dlzip.stop",
	DLZIP_LIST[200] = RUN "/dlzip.list", DLZIP_ACCT[200] = RUN "/dlzip.acct", DLZIP_DEST[200] = RUN "/dlzip.dest";
/* ---- whose files a request may name (1.6.3 Part 10, item 1: the account check)
   An account's Downloads is a folder of this computer that no key protects: it belongs to the desktop's user, and every
   account's is beside the others. So the account a request names is checked to be the one signed in -- never only to be a
   name of the right shape, which any page of the desktop may say. The guest's store and the shared one are nobody's (the
   guest is a shared sandbox by design). With no account signed in nothing is refused here and there is no desktop to ask:
   the sign-in screen's own housekeeping (and the checks, which run so) go on as before. */
static void dlz_sess(char *o, size_t n)
{
	/* rudolfd-home.inc's HOME_SESSION, written by home_bind and removed at sign-out (this include comes before it) */
	o[0] = 0; char *c = slurp(RUN "/home-session", NULL); if (!c) return; trim(c); snprintf(o, n, "%s", c); free(c);
}
static int dlz_acct_mine(const char *acct)
{
	if (!acct || !*acct) return 1;
	char s[80]; dlz_sess(s, sizeof s);
	if (!*s || !strcmp(acct, "guest") || !strcmp(acct, "shared")) return 1;
	/* the page sends the account's id in lower case (dlAcct); the session holds it as it was made */
	for (const char *a = acct, *b = s;; a++, b++) {
		if (!*a && !*b) return 1;
		if (!*a || !*b || tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
	}
}
/* the account's folder (hd) and the ZIP in it (zp) for the request's name and acct: an account's own folder only, a plain
   file there (not a link), the folders on the way none either; 0 after a reply */
static int dlzip_file(struct req *r, char *zp, size_t zn, char *hd, size_t hn, char *acct, size_t an)
{
	const char *name = qparam(r->query, "name"), *a = qparam(r->query, "acct");
	if (!a || !*a || !dl_acct_ok(a)) { reply_err(r->fd, 400, "which account?"); return 0; }
	if (!dlz_acct_mine(a)) { reply_err(r->fd, 403, "another account's Downloads is not yours"); return 0; }
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
/* (1.6.3 Part 10, item 1) the path a request may name: where the desktop user's own files are, where drives are mounted, and
   /tmp -- and whatever DLZIP_ROOTS names (a colon list, for the checks). The real path is what is tested, so a link cannot
   lead out of them; what the file's own permissions then allow is the desktop user's business, as the reading is done as
   that user. */
static int dlzip_under(const char *p, const char *r, size_t rl)
{
	if (strncmp(p, r, rl)) return 0;
	return rl && r[rl - 1] == '/' ? 1 : (p[rl] == 0 || p[rl] == '/');
}
static int dlzip_path_ok(const char *real)
{
	/* the signed-in account's own places, not /home as a whole: each account has a Linux home of its own under /home, and
	   it is that account's. Nor any hidden part of a path: the accounts' unencrypted downloads store is
	   /home/rudolf/Downloads/.accounts/<id>, the shared quarantine is another, and both are other accounts' files. */
	for (const char *c = real; *c; c++) if (*c == '/' && c[1] == '.') return 0;
	static const char *const R[] = { "/home/rudolf", "/media", "/run/media", "/mnt", "/tmp", "/var/tmp", "/srv", NULL };
	for (int i = 0; R[i]; i++) if (dlzip_under(real, R[i], strlen(R[i]))) return 1;
	const char *e = getenv("DLZIP_ROOTS"); if (!e || !*e) return 0;
	char *c = strdup(e); if (!c) return 0;
	int ok = 0;
	for (char *sv, *t = strtok_r(c, ":", &sv); t && !ok; t = strtok_r(NULL, ":", &sv)) if (*t) ok = dlzip_under(real, t, strlen(t));
	free(c); return ok;
}
/* a file at an absolute path the account may read: its real path (no link, a plain file) and how big it is; 0 after a reply */
static int dlzip_real(struct req *r, const char *p, char *zp, size_t zn, struct stat *stp)
{
	if (!p || *p != '/') { reply_err(r->fd, 400, "an absolute path, please"); return 0; }
	if (strlen(p) >= PATH_MAX) { reply_err(r->fd, 400, "that path is too long"); return 0; }
	for (const unsigned char *c = (const unsigned char *)p; *c; c++) if (*c < 0x20 || *c == 0x7f) { reply_err(r->fd, 400, "a path, please"); return 0; }
	char rz[PATH_MAX];
	if (!realpath(p, rz)) { reply_err(r->fd, 404, "the file is not there any more"); return 0; }
	if (!dlzip_path_ok(rz)) { reply_err(r->fd, 403, "only a file in your home, on a drive, or in /tmp"); return 0; }
	int f = open(rz, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC); struct stat st;
	if (f < 0 || fstat(f, &st) || !S_ISREG(st.st_mode)) { if (f >= 0) close(f); reply_err(r->fd, 404, "the file is not there any more"); return 0; }
	close(f);
	if ((size_t)snprintf(zp, zn, "%s", rz) >= zn) { reply_err(r->fd, 400, "that path is too long"); return 0; }
	if (stp) *stp = st;
	return 1;
}
/* a ZIP at an absolute path the account may read: its real path and the folder it is in */
static int dlzip_at_path(struct req *r, const char *p, char *zp, size_t zn, char *hd, size_t hn)
{
	if (!dlzip_real(r, p, zp, zn, NULL)) return 0;
	char *sl = strrchr(zp, '/'); size_t dl = sl == zp ? 1 : (size_t)(sl - zp);
	if (dl >= hn) { reply_err(r->fd, 400, "that path is too long"); return 0; }
	memcpy(hd, zp, dl); hd[dl] = 0;
	return 1;
}
/* 1.6.3 Part 10, item 1: a file at a path, not only one in Downloads -- the bytes the page's antivirus reads of a file a ZIP
   was unpacked into beside it (GET dl/read?path=), and moving such a file into the quarantine or deleting it when it is found
   bad (POST dl/file?path=&to=quar|del). The same places a ZIP at a path may be in, the same off and len, and the same
   quarantine and deletion the page already asks for a Downloads file, so a folder beside a ZIP is checked exactly as one
   unpacked into Downloads. */
/* the folder an extraction just made beside a ZIP ("dest" in its result, kept when dl/unzipped says where it is), while the
   page still has its result (15 minutes, as dlzip_pending counts). A path may only name a file inside it: the antivirus
   reads there, and its quarantine or deletion moves and removes there -- never another file of the computer, another
   account's included, which is what naming any path the account may read would have allowed. */
static int dlz_in_dest(const char *real)
{
	struct stat st;
	if (stat(DLZIP_DEST, &st) || time(NULL) - st.st_mtime >= 900) return 0;
	char *d = slurp(DLZIP_DEST, NULL); if (!d) return 0;
	/* line 1: the account whose extraction it was (empty with nobody signed in), line 2: the folder. A folder another
	   account unpacked is not this account's to read, quarantine or delete: the record goes with the session. */
	char *nl = strchr(d, '\n'); if (!nl) { free(d); return 0; }
	*nl = 0;
	char s[80]; dlz_sess(s, sizeof s); size_t i = 0;
	for (; d[i] && s[i]; i++) if (tolower((unsigned char)d[i]) != tolower((unsigned char)s[i])) break;
	int ok = !d[i] && !s[i];
	if (ok) {
		const char *path = nl + 1; char want[PATH_MAX + 2]; snprintf(want, sizeof want, "%s/", path);
		size_t l = strlen(path) ? strlen(want) : 0;
		ok = l && *path == '/' && !strncmp(real, want, l) && real[l];
	}
	free(d); return ok;
}
static void api_dl_path(struct req *r, const char *what, int post)
{
	char fp[PATH_MAX]; struct stat st;
	if (!dlzip_real(r, qparam(r->query, "path"), fp, sizeof fp, &st)) return;
	if (!dlz_in_dest(fp)) { reply_err(r->fd, 403, "only a file inside the folder the extraction made"); return; }
	if (!strcmp(what, "read")) {
		const char *os = qparam(r->query, "off"), *ls = qparam(r->query, "len");
		long long off = os ? atoll(os) : 0, len = ls ? atoll(ls) : 8 << 20;
		int f = open(fp, O_RDONLY | O_NOFOLLOW);
		if (f < 0) { reply_err(r->fd, 404, "the file is not there any more"); return; }
		if (len < 0 || len > 8 << 20) len = 8 << 20;
		if (off < 0) off = st.st_size + off < 0 ? 0 : st.st_size + off;
		if (off > st.st_size) off = st.st_size;
		if (len > st.st_size - off) len = st.st_size - off;
		char *b = malloc(len + 1); ssize_t got = 0;
		if (b && lseek(f, off, SEEK_SET) == off) { while (got < len) { ssize_t k = read(f, b + got, len - got); if (k <= 0) break; got += k; } }
		close(f);
		if (!b) { reply_err(r->fd, 500, "no memory"); return; }
		reply(r->fd, 200, "application/octet-stream", b, got, NULL); free(b); return;
	}
	if (!post) { reply_err(r->fd, 400, "use POST"); return; }
	const char *to = qparam(r->query, "to");
	if (!to || (strcmp(to, "quar") && strcmp(to, "del"))) { reply_err(r->fd, 400, "to=quar or del"); return; }
	if (!strcmp(to, "del")) { if (unlink(fp)) { reply_err(r->fd, 500, strerror(errno)); return; } reply_ok(r->fd, "deleted"); return; }
	char b[300]; snprintf(b, sizeof b, "%s", strrchr(fp, '/') + 1);
	mkdir(QUARANTINE, 0700); if (chown(QUARANTINE, 1000, 1000)) {}
	char dst[700]; dl_unique(QUARANTINE, b, dst, sizeof dst);
	if (rename(fp, dst)) { reply_err(r->fd, 500, strerror(errno)); return; }
	if (chown(dst, 1000, 1000)) {}
	char ob[760]; str s = {0}; sadd(&s, "{\"ok\":true,\"path\":"); sjson(&s, dl_out(dst, ob, sizeof ob));
	sadd(&s, ",\"name\":"); sjson(&s, strrchr(dst, '/') + 1); sadd(&s, ",\"size\":%lld}", (long long)st.st_size);
	reply_json(r->fd, &s); return;
}
/* a path inside the folder an extraction made: relative, nothing of it above that folder, no control characters */
static int dlzip_rel_ok(const char *p)
{
	if (!p || !*p || *p == '/' || strlen(p) >= PATH_MAX) return 0;
	size_t n = strlen(p);
	if (!strcmp(p, "..") || !strncmp(p, "../", 3) || strstr(p, "/../") || (n > 3 && !strcmp(p + n - 3, "/.."))) return 0;
	for (const unsigned char *c = (const unsigned char *)p; *c; c++) if (*c < 0x20 || *c == 0x7f) return 0;
	return 1;
}
/* (the request's ZIP and the folder the job works in, and which way it ends: FLAT into the account's folder, or BESIDE into a
   free folder named after the ZIP) */
enum { DLZIP_FLAT = 0, DLZIP_BESIDE = 1 };
static int dlzip_target(struct req *r, char *zp, size_t zn, char *hd, size_t hn, char *acct, size_t an, int *mode)
{
	*mode = DLZIP_FLAT; acct[0] = 0;
	const char *p = qparam(r->query, "path");
	if (p && *p) { *mode = DLZIP_BESIDE; return dlzip_at_path(r, p, zp, zn, hd, hn); }
	return dlzip_file(r, zp, zn, hd, hn, acct, an);
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
	char zp[600], hd[300], acct[80]; int mode = DLZIP_FLAT;
	if (!strcmp(what, "zip")) {
		if (!dlzip_target(r, zp, sizeof zp, hd, sizeof hd, acct, sizeof acct, &mode)) return;
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
		if (!dlzip_target(r, zp, sizeof zp, hd, sizeof hd, acct, sizeof acct, &mode)) return;
		unlink(DLZIP_STOP); unlink(DLZIP_OUT); unlink(DLZIP_LIST); unlink(DLZIP_DEST);   /* (a new extraction: no older folder is the antivirus's) */
		int f = open(DLZIP_LIST, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
		str l = {0}; sadd(&l, "%s\n%s\n%s\n", zp, hd, mode == DLZIP_BESIDE ? "beside" : "flat");
		if (f < 0 || write(f, l.b, l.n) != (ssize_t)l.n) { if (f >= 0) close(f); free(l.b); reply_err(r->fd, 500, "could not write the request"); return; }
		close(f); free(l.b);
		/* (only an account's Downloads folder changes in the flat way: dl/home's "unzip" is for that one -- a ZIP beside itself
		   leaves whatever result the page has not taken yet alone) */
		if (mode == DLZIP_FLAT) write_str(DLZIP_ACCT, acct);
		char cmd[700]; snprintf(cmd, sizeof cmd, "/bin/sh %s extract %s", DLZIP_SH, DLZIP_LIST);   /* (fixed paths, no names) */
		start_job("dl-unzip", cmd); reply_ok(r->fd, "started"); return;
	}
	if (!strcmp(what, "unzipped")) {
		if (post) { unlink(DLZIP_OUT); unlink(DLZIP_ACCT); unlink(DLZIP_DEST); reply_ok(r->fd, "taken"); return; }
		char *o = slurp(DLZIP_OUT, NULL), *a = slurp(DLZIP_ACCT, NULL); if (a) trim(a);
		str s = {0}; sadd(&s, "{\"ok\":true,\"running\":%s,\"acct\":", dlzip_job_running() ? "true" : "false"); sjson(&s, a ? a : "");
		str dirs = {0}, files = {0}; sadd(&dirs, "%s", ""); sadd(&files, "%s", ""); int nd = 0, nf = 0; char *end = NULL, *dest = NULL;
		for (char *sv, *l = o ? strtok_r(o, "\n", &sv) : NULL; l; l = strtok_r(NULL, "\n", &sv)) {
			if (!strncmp(l, "dir ", 4)) { sadd(&dirs, "%s", nd++ ? "," : ""); sjson(&dirs, l + 4); }
			else if (!strncmp(l, "file ", 5)) {
				char *t1 = strchr(l + 5, '\t'), *t2 = t1 ? strchr(t1 + 1, '\t') : NULL; if (!t2) continue;
				*t1 = 0; *t2 = 0; if (!dlzip_rel_ok(t1 + 1)) continue;
				sadd(&files, "%s{\"path\":", nf++ ? "," : ""); sjson(&files, l + 5); sadd(&files, ",\"disk\":"); sjson(&files, t1 + 1); sadd(&files, ",\"size\":%lld}", atoll(t2 + 1));
			} else if (!strncmp(l, "dest ", 5)) {
				dest = l + 5;
				/* where the page will read, quarantine and delete its files by path: kept for its antivirus */
				if (*dest == '/') { char ss[80], b[PATH_MAX + 100]; dlz_sess(ss, sizeof ss); snprintf(b, sizeof b, "%s\n%s\n", ss, dest); write_str(DLZIP_DEST, b); chmod(DLZIP_DEST, 0600); }
			}
			else if (!strncmp(l, "done ", 5) || !strncmp(l, "stopped ", 8) || !strncmp(l, "error ", 6)) end = l;
		}
		sadd(&s, ",\"dest\":"); sjson(&s, dest ? dest : "");
		sadd(&s, ",\"dirs\":[%s],\"files\":[%s],\"end\":", dirs.b, files.b); sjson(&s, end ? end : ""); sadd(&s, "}");
		free(dirs.b); free(files.b); free(o); free(a); reply_json(r->fd, &s); return;
	}
	reply_err(r->fd, 404, "unknown download action");
}
