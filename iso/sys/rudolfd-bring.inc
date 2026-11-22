/* (included by rudolfd.c, not built on its own: hence .inc) ---------- 1.6.2 (docs/plan-1.6.2.md Part 27): Bring your files
 * The installer's step that keeps the old system's personal files: everything is done by the drives helper
 * (rudolf-drives.c bring-*, bringfiles.sh), which checks the arguments again; nothing here reads the old disk.
 *   POST bring/scan?disk=D            a job (bring-scan): the old system's people and folders found and sized
 *   GET  bring/result?disk=D          what the last scan of D found (bringfiles.sh's JSON)
 *   POST bring/unlock?part=P          body: BitLocker's recovery key or password (passed on as the helper's standard
 *                                     input, never kept, never an argument): 403 when it does not open it
 *   POST bring/copy                   body: "D continue=0|1", the folder on the drive, "PERSON<TAB>FOLDER" lines: a job
 *   POST bring/cancel                 the copy (or bringing in) stops after its file
 *   POST bring/decide?disk=D&what=erase|partial
 *   POST bring/close?disk=D
 *   GET  bring/found                  the copies on the plugged-in drives
 *   POST bring/import?acct=A&dir=DIR&person=P   a job: one person's files brought into the signed-in account's folders,
 *                                     as the account's own Linux user */
static void bring_reply_out(struct req *r, char *o, int rc)
{
	trim(o);
	if (rc == 0) reply_ok(r->fd, *o ? o : "done");
	else reply_err(r->fd, rc == 2 ? 400 : rc == 3 ? 403 : 500, *o ? o : "it could not be done");
	free(o);
}
static void api_bring(struct req *r, const char *what, int post)
{
	if (!strcmp(what, "result") || !strcmp(what, "found")) {
		if (post) { reply_err(r->fd, 400, "use GET"); return; }
		if (!strcmp(what, "found")) {
			const char *av[] = { "drives", "bring-found", NULL }; char *o = root_out(av, 0); trim(o);
			if (o[0] != '{') { free(o); reply_err(r->fd, 500, "the drives could not be looked at"); return; }
			str s = {0}; sadd(&s, "%s", o); free(o); reply_json(r->fd, &s); return;
		}
		char *d = qparam(r->query, "disk"); if (!safe_word(d, 16) || strchr(d, '.')) { reply_err(r->fd, 400, "which disk?"); return; }
		char p[200]; snprintf(p, sizeof p, RUN "/bring/%s.json", d);
		char *c = slurp(p, NULL); if (!c) { reply_err(r->fd, 404, "not looked at yet"); return; }
		trim(c); if (c[0] != '{') { free(c); reply_err(r->fd, 500, "bad result"); return; }
		str s = {0}; sadd(&s, "%s", c); free(c); reply_json(r->fd, &s); return;
	}
	if (!post) { reply_err(r->fd, 400, "use POST"); return; }
	if (!strcmp(what, "scan") || !strcmp(what, "close") || !strcmp(what, "decide")) {
		char *d = qparam(r->query, "disk"); if (!safe_word(d, 16) || strchr(d, '.')) { reply_err(r->fd, 400, "which disk?"); return; }
		if (!strcmp(what, "scan")) {
			if (job_running()) { reply_err(r->fd, 409, "busy: another job is running, try again when it ends"); return; }
			const char *av[] = { "drives", "bring-scan", d, NULL }; root_job("bring-scan", av); reply_ok(r->fd, "looking"); return;
		}
		if (!strcmp(what, "close")) { const char *av[] = { "drives", "bring-close", d, NULL }; int rc = root_call(av, 0, 2, 2); reply_ok(r->fd, rc == 0 ? "closed" : "not open"); return; }
		char *w = qparam(r->query, "what"); if (!w || (strcmp(w, "erase") && strcmp(w, "partial"))) { reply_err(r->fd, 400, "erase or partial?"); return; }
		const char *av[] = { "drives", "bring-decide", d, w, NULL };
		int pf[2]; if (pipe2(pf, O_CLOEXEC)) { reply_err(r->fd, 500, "could not start"); return; }
		int nul = root_null(); int rc = root_call(av, nul >= 0 ? nul : 0, pf[1], pf[1]); close(pf[1]); if (nul >= 0) close(nul);
		char b[600]; ssize_t n = read(pf[0], b, sizeof b - 1); close(pf[0]); b[n > 0 ? n : 0] = 0;
		bring_reply_out(r, strdup(b), rc); return;
	}
	if (!strcmp(what, "unlock")) {
		char *pt = qparam(r->query, "part"); if (!safe_word(pt, 32) || strchr(pt, '.')) { reply_err(r->fd, 400, "which partition?"); return; }
		if (!r->body || !r->blen || r->blen > 256 || memchr(r->body, '\n', r->blen)) { if (r->body) explicit_bzero(r->body, r->blen); reply_err(r->fd, 400, "the key, on one line"); return; }
		int kp[2], op[2]; if (pipe2(kp, O_CLOEXEC)) { explicit_bzero(r->body, r->blen); reply_err(r->fd, 500, "could not start"); return; }
		if (pipe2(op, O_CLOEXEC)) { close(kp[0]); close(kp[1]); explicit_bzero(r->body, r->blen); reply_err(r->fd, 500, "could not start"); return; }
		ssize_t w = write(kp[1], r->body, r->blen); if (write(kp[1], "\n", 1) < 0) w = -1; close(kp[1]); explicit_bzero(r->body, r->blen);
		int rc = w < 0 ? -1 : root_call((const char *[]){ "drives", "bring-unlock", pt, NULL }, kp[0], op[1], op[1]);
		close(kp[0]); close(op[1]);
		char b[600]; ssize_t n = read(op[0], b, sizeof b - 1); close(op[0]); b[n > 0 ? n : 0] = 0;
		health_add("disk", "Bring your files: BitLocker on %.32s %s", pt, rc == 0 ? "opened read-only" : "not opened");
		bring_reply_out(r, strdup(b), rc); return;
	}
	if (!strcmp(what, "cancel")) { const char *av[] = { "drives", "bring-cancel", NULL }; root_call(av, 0, 2, 2); reply_ok(r->fd, "stopping"); return; }
	if (!strcmp(what, "copy")) {
		if (!r->body || r->blen > 65536) { reply_err(r->fd, 400, "what to copy?"); return; }
		if (job_running()) { reply_err(r->fd, 409, "busy: another job is running, try again when it ends"); return; }
		/* (the helper's script checks every line against its own scan; here only its shape) */
		char *b = strndup(r->body, r->blen); int bad = !b || memchr(r->body, 0, r->blen) != NULL, ln = 0;
		for (char *l = b, *e; !bad && l && *l; l = e ? e + 1 : NULL, ln++) {
			e = strchr(l, '\n'); size_t L = e ? (size_t)(e - l) : strlen(l);
			if (ln == 0 && (L < 3 || L > 40)) bad = 1;
			if (ln == 1 && (strncmp(l, "/media/", 7) || L > 1000)) bad = 1;
		}
		if (bad || ln < 3) { free(b); reply_err(r->fd, 400, "what to copy?"); return; }
		FILE *f = fopen(RUN "/bring-copy.list", "w"); if (!f) { free(b); reply_err(r->fd, 500, "could not start"); return; }
		fputs(b, f); fclose(f); free(b);
		health_add("files", "Bring your files: the old system's personal files are being copied onto a drive");
		const char *av[] = { "drives", "bring-copy", NULL }; root_job("bring-copy", av); reply_ok(r->fd, "copying"); return;
	}
	if (!strcmp(what, "import")) {
		char acct[80]; { const char *a = qparam(r->query, "acct"); snprintf(acct, sizeof acct, "%s", a ? a : ""); }
		char *dir = qparam(r->query, "dir"), *pe = qparam(r->query, "person");
		if (!home_acct_ok(acct) || !dir || strncmp(dir, "/media/", 7) || !pe || !*pe || strchr(pe, '/')) { reply_err(r->fd, 400, "which copy, and whose files?"); return; }
		if (job_running()) { reply_err(r->fd, 409, "busy: another job is running, try again when it ends"); return; }
		const char *av[] = { "drives", "bring-import", acct, dir, pe, NULL }; root_job("bring-import", av); reply_ok(r->fd, "bringing in"); return;
	}
	reply_err(r->fd, 404, "unknown Bring your files action");
}
