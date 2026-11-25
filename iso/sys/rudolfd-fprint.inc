/* (included by rudolfd.c, not built on its own: hence .inc) ---------- 1.6.3 (docs/plan-1.6.3.md Part 2): fingerprint
 * readers. The reader is the fprint root helper's (rudolf-fprint.c, with libfprint); this is who may ask it what, and when.
 *
 * A finger stands in for the password at the lock screen and in administrator prompts, never more:
 *   - never the first sign-in after a restart, nor a sign-in after signing out: the account's prints are encrypted under
 *     its file key, which only the password (or the password and a security key) unwraps (rudolf-fprint.c). rudolfd
 *     also keeps, in memory (RUN, gone at a restart), which accounts signed in with their password since the start
 *     (fp_signed_in: the page's home/unlock?session=1, Native's desktop/login), and takes a finger for no other;
 *     signing out (home_lock) ends it;
 *   - five fingers in a row not recognised: the password, until the password is used (fp/password, a sign-in, a
 *     match does not count);
 *   - never instead of two-step sign-in's second step: the page and Native ask the code or the key after a finger as
 *     after a password;
 *   - Guest has none.
 *
 *   GET  fp/status[?acct=ID]      {"helper","readers":[{"name","driver","scan","stages",...}],"unsupported":[{"usb","name"}],
 *                                 with acct: "disk","open","fingers":[FINGER],"signed","fails","ready","why"}
 *   POST fp/enroll                {"acct","finger","n","p"}: the account's own password, or an administrator's (another
 *                                 account's); the account's files must be open. Waits for the scans: {"ok","finger"}
 *   GET  fp/progress              the scans so far: {"stage","of","msg"}
 *   POST fp/remove                {"acct","finger"|"all":true,"n","p"} (as enroll)
 *   POST fp/verify                {"acct","for":"unlock"|"admin"}: waits up to 30 s for a finger. 200 {"ok":true,"acct",
 *                                 "finger","n"}; 403 {"error","left"} not recognised; 423 {"need":"password","why":"restart"|
 *                                 "tries"|"guest"} the password is needed; 404 no reader or no fingers; 408 no finger in time
 *   POST fp/cancel                a waiting enroll or verify ends
 *   POST fp/password              {"n","p"}: the password was used (the page's lock screen and prompts): the account's
 *                                 count of fingers not recognised starts again (checked here; wrong ones counted as
 *                                 the lock screen's)
 * An account removed: its prints too (fp_acct_gone, from au_sync_ids). */
#define FP_MAXFAIL 5
static char fp_state[256] = RUN "/fpstate", fp_hrun[256] = RUN "/fprint";   /* (rudolfd's own; the helper's; host tests change them) */
static int fp_have(void) { char p[PATH_MAX]; snprintf(p, sizeof p, "%s/fprint", RD_ROOT_DIR); return !access(p, F_OK); }
static int fp_acct_ok(const char *a) { return home_acct_ok(a) && strcmp(a, "guest") && strcmp(a, "shared"); }
static void fp_file(char *o, size_t n, const char *kind, const char *acct) { snprintf(o, n, "%s/%s-%s", fp_state, kind, acct); }
static int fp_fails(const char *acct) { char p[400]; fp_file(p, sizeof p, "fails", acct); char *t = slurp(p, NULL); int n = t ? atoi(t) : 0; free(t); return n; }
static void fp_fails_set(const char *acct, int n)
{
	char p[400]; fp_file(p, sizeof p, "fails", acct);
	if (!n) { unlink(p); return; }
	mkdir(fp_state, 0700); char b[16]; snprintf(b, sizeof b, "%d\n", n); write_str(p, b);
}
static int fp_signed(const char *acct) { char p[400]; fp_file(p, sizeof p, "signed", acct); return !access(p, F_OK); }
/* the account signed in with its password (rudolfd-home.inc's home/unlock with session=1; Native's desktop/login) */
static void fp_signed_in(const char *acct)
{
	if (!fp_acct_ok(acct)) return;
	mkdir(fp_state, 0700); char p[400]; fp_file(p, sizeof p, "signed", acct); write_str(p, "1\n"); fp_fails_set(acct, 0);
}
/* signed out (home_lock): its files are locked again, and a finger is not taken until the password has been */
static void fp_signed_out(const char *acct) { if (!fp_acct_ok(acct)) return; char p[400]; fp_file(p, sizeof p, "signed", acct); unlink(p); }
/* the account is gone (au_sync_ids): its prints and the reader's own copies of them */
static void fp_acct_gone(const char *acct)
{
	if (!fp_acct_ok(acct)) return;
	fp_signed_out(acct); fp_fails_set(acct, 0);
	if (!fp_have()) return;
	const char *argv[] = { "fprint", "purge", acct, NULL };
	if (root_run(argv)) health_add("account", "the fingerprints of the removed account %.80s could not all be removed", acct);
	else health_add("account", "the fingerprints of the removed account %.80s were removed", acct);
}
/* the helper's answer, with the HTTP status its "code" stands for */
static void fp_reply(struct req *r, char *o)
{
	trim(o);
	if (!o[0] || o[0] != '{') { reply_err(r->fd, 500, "the fingerprint helper did not answer"); return; }
	const char *cq = dj_get(o, "ok"); int ok = cq && dj_true(dj_ws(cq));
	int st = 200;
	if (!ok) { char *c = dj_str(dj_get(o, "code")); const char *k = c ? c : "";
		st = !strcmp(k, "noreader") || !strcmp(k, "nofingers") ? 404 : !strcmp(k, "timeout") ? 408 : !strcmp(k, "locked") ? 423 : !strcmp(k, "failed") ? 500 : 409;
		free(c); }
	reply(r->fd, st, "application/json", o, strlen(o), NULL);
}
/* who may change ACCT's fingers: its own password, or an administrator's. 0, or the status sent */
static int fp_cred(struct req *r, const char *acct, char *who, size_t wn)
{
	int wt = dfail_wait(); if (wt) { char m[120]; snprintf(m, sizeof m, "Too many failed attempts. Please wait %d seconds and try again.", wt); reply_err(r->fd, 429, m); return 429; }
	char *n = dbody_field(r, "n"), *pw = dbody_field(r, "p"); int okk = 0, found = 0;
	dls l; dls_open(&l); char *list = dls_accounts(&l); const char *a = list && n ? dacct_find(list, n) : NULL;
	if (a && pw) { char id[80], *role = dj_str(dj_get(a, "r")), *h = dj_str(dj_get(a, "h")), *an = dj_str(dj_get(a, "n")); dacct_id(a, id, sizeof id);
		found = !strcmp(id, acct) || (role && !strcmp(role, "admin"));
		okk = found && dpw_check(h, pw) == 0; snprintf(who, wn, "%s", an ? an : id); dfree_secret(h); free(role); free(an); }
	dfree_secret(pw); free(n); free(list); dls_close(&l);
	if (!okk) { dfail_note(0); health_add("signin", "a wrong password changing an account's fingerprints%s", found ? "" : " (not that account's or an administrator's)");
		reply_err(r->fd, 403, found ? "Wrong password." : "Only the account itself or an administrator can change its fingerprints."); return 403; }
	dfail_note(1); return 0;
}
static char *fp_body_acct(struct req *r) { char *a = dbody_field(r, "acct"); if (a && !fp_acct_ok(a)) { free(a); a = NULL; } return a; }
static void fp_cancel(void) { if (!fp_have()) return; const char *argv[] = { "fprint", "cancel", NULL }; char *o = root_out(argv, 0); free(o); }
/* the account's name and role, as the page keeps them */
static int fp_acct_info(const char *acct, char *name, size_t nn, char *role, size_t rn)
{
	dls l; dls_open(&l); char *list = dls_accounts(&l); int found = 0; name[0] = role[0] = 0;
	for (int i = 0; list; i++) { const char *a = dj_at(list, i); if (!a) break; char id[80]; dacct_id(a, id, sizeof id);
		if (!strcmp(id, acct)) { char *n = dj_str(dj_get(a, "n")), *ro = dj_str(dj_get(a, "r")); snprintf(name, nn, "%s", n ? n : acct); snprintf(role, rn, "%s", ro ? ro : ""); free(n); free(ro); found = 1; break; } }
	free(list); dls_close(&l); return found;
}
static void api_fp(struct req *r, const char *w, int post)
{
	if (!strcmp(w, "status")) {
		const char *qa = qparam(r->query, "acct"); char acct[80] = ""; if (qa && fp_acct_ok(qa)) snprintf(acct, sizeof acct, "%s", qa);
		str s = {0}; sadd(&s, "{\"ok\":true,\"helper\":%s", fp_have() ? "true" : "false");
		int readers = 0, nf = 0;
		if (fp_have()) {
			const char *dv[] = { "fprint", "devices", NULL }; char *o = root_out(dv, 0); trim(o);
			const char *rq = o && o[0] == '{' ? dj_get(o, "readers") : NULL, *uq = o && o[0] == '{' ? dj_get(o, "unsupported") : NULL;
			const char *re = rq ? dj_end(rq, 0) : NULL, *ue = uq ? dj_end(uq, 0) : NULL;
			if (re && *dj_ws(rq) == '[') { rq = dj_ws(rq); sadd(&s, ",\"readers\":%.*s", (int)(re - rq), rq); readers = dj_at(rq, 0) != NULL; } else sadd(&s, ",\"readers\":[]");
			if (ue && *dj_ws(uq) == '[') { uq = dj_ws(uq); sadd(&s, ",\"unsupported\":%.*s", (int)(ue - uq), uq); } else sadd(&s, ",\"unsupported\":[]");
			free(o);
			if (acct[0]) {
				const char *lv[] = { "fprint", "list", acct, NULL }; char *l = root_out(lv, 0); trim(l);
				const char *fq = l && l[0] == '{' ? dj_get(l, "fingers") : NULL, *fe = fq ? dj_end(fq, 0) : NULL;
				const char *dq = l && l[0] == '{' ? dj_get(l, "disk") : NULL, *oq = l && l[0] == '{' ? dj_get(l, "open") : NULL;
				sadd(&s, ",\"disk\":%s,\"open\":%s", dq && dj_true(dj_ws(dq)) ? "true" : "false", oq && dj_true(dj_ws(oq)) ? "true" : "false");
				if (fe && *dj_ws(fq) == '[') { fq = dj_ws(fq); sadd(&s, ",\"fingers\":%.*s", (int)(fe - fq), fq); for (int i = 0; dj_at(fq, i); i++) nf++; } else sadd(&s, ",\"fingers\":[]");
				free(l);
			}
		} else sadd(&s, ",\"readers\":[],\"unsupported\":[]");
		if (acct[0]) {
			int sg = fp_signed(acct), fl = fp_fails(acct);
			const char *why = !fp_have() ? "helper" : !readers ? "noreader" : !nf ? "nofingers" : !sg ? "restart" : fl >= FP_MAXFAIL ? "tries" : "";
			sadd(&s, ",\"signed\":%s,\"fails\":%d,\"ready\":%s,\"why\":\"%s\"", sg ? "true" : "false", fl, why[0] ? "false" : "true", why);
		}
		sadd(&s, "}"); reply_json(r->fd, &s); return;
	}
	if (!strcmp(w, "progress")) {
		char p[PATH_MAX]; snprintf(p, sizeof p, "%s/progress", fp_hrun); char *t = slurp(p, NULL);
		if (!t) { static const char E[] = "{\"stage\":0,\"of\":0,\"msg\":\"\"}"; reply(r->fd, 200, "application/json", E, sizeof E - 1, NULL); return; }
		trim(t); reply(r->fd, 200, "application/json", t, strlen(t), NULL); free(t); return;
	}
	if (!post) { reply_err(r->fd, 405, "POST"); return; }
	if (!fp_have()) { reply_err(r->fd, 404, "This Rudolf OS has no fingerprint support."); return; }
	if (!strcmp(w, "cancel")) { fp_cancel(); reply_ok(r->fd, "cancelled"); return; }
	if (!strcmp(w, "password")) {
		int wt = dfail_wait(); if (wt) { char m[120]; snprintf(m, sizeof m, "Too many failed attempts. Please wait %d seconds and try again.", wt); reply_err(r->fd, 429, m); return; }
		char *n = dbody_field(r, "n"), *pw = dbody_field(r, "p"); int okk = 0; char id[80] = "";
		dls l; dls_open(&l); char *list = dls_accounts(&l); const char *a = list && n ? dacct_find(list, n) : NULL;
		if (a && pw) { char *h = dj_str(dj_get(a, "h")); dacct_id(a, id, sizeof id); okk = dpw_check(h, pw) == 0; dfree_secret(h); }
		dfree_secret(pw); free(n); free(list); dls_close(&l);
		if (!okk) { dfail_note(0); reply_err(r->fd, 403, "Wrong password."); return; }
		dfail_note(1); if (fp_acct_ok(id)) fp_fails_set(id, 0); reply_ok(r->fd, "ok"); return;
	}
	char *acct = fp_body_acct(r);
	if (!acct) { char *a = dbody_field(r, "acct"); int g = a && !strcmp(a, "guest"); free(a);
		static const char G[] = "{\"ok\":false,\"need\":\"password\",\"why\":\"guest\",\"error\":\"Guest has no fingerprints.\"}";
		if (g) reply(r->fd, 423, "application/json", G, sizeof G - 1, NULL);
		else reply_err(r->fd, 400, "acct?");
		return; }
	if (!strcmp(w, "enroll") || !strcmp(w, "remove")) {
		char who[256] = "", name[256], role[16]; if (fp_cred(r, acct, who, sizeof who)) { free(acct); return; }
		fp_acct_info(acct, name, sizeof name, role, sizeof role);
		char *f = dbody_field(r, "finger"); const char *aq = dj_get(r->body, "all"); int all = !strcmp(w, "remove") && aq && dj_true(dj_ws(aq));
		const char *fn = all ? "all" : f ? f : "";
		if (!all && (strlen(fn) > 20 || strspn(fn, "abcdefghijklmnopqrstuvwxyz-") != strlen(fn) || !fn[0])) { free(f); free(acct); reply_err(r->fd, 400, "finger?"); return; }
		if (!strcmp(w, "enroll")) fp_cancel();   /* (a lock screen's finger waiting meanwhile gives the reader up) */
		const char *argv[] = { "fprint", !strcmp(w, "enroll") ? "enroll" : "delete", acct, fn, NULL };
		char *o = root_out(argv, 0); trim(o);
		const char *okq = o[0] == '{' ? dj_get(o, "ok") : NULL;
		if (okq && dj_true(dj_ws(okq))) {
			if (!strcmp(w, "enroll")) health_add("security", "a fingerprint (%s) was added to the account %.100s by %.100s", fn, name[0] ? name : acct, who);
			else health_add("security", "%s%s%s removed from the account %.100s by %.100s", all ? "every fingerprint was" : "a fingerprint (", all ? "" : fn, all ? "" : ") was", name[0] ? name : acct, who); }
		fp_reply(r, o); free(o); free(f); free(acct); return;
	}
	if (!strcmp(w, "verify")) {
		char name[256], role[16]; const char *fq = dj_get(r->body, "for"); int adm = fq && !strncmp(dj_ws(fq), "\"admin\"", 7);
		if (!fp_acct_info(acct, name, sizeof name, role, sizeof role)) { free(acct); reply_err(r->fd, 404, "no such account"); return; }
		if (adm && strcmp(role, "admin")) { free(acct); reply_err(r->fd, 403, "only an administrator's finger can allow this"); return; }
		const char *why = !fp_signed(acct) ? "restart" : fp_fails(acct) >= FP_MAXFAIL ? "tries" : NULL;
		if (why) { str e = {0}; sadd(&e, "{\"ok\":false,\"need\":\"password\",\"why\":\"%s\",\"error\":", why);
			sjson(&e, !strcmp(why, "restart") ? "Use your password: a finger is taken only after this account has signed in with its password since the computer started." : "Too many fingers were not recognised: use your password.");
			sadd(&e, "}"); reply(r->fd, 423, "application/json", e.b, e.n, NULL); free(e.b); free(acct); return; }
		fp_cancel(); { struct timespec ts = { 0, 300000000 }; nanosleep(&ts, NULL); }   /* (another one waiting gives the reader up) */
		const char *argv[] = { "fprint", "verify", acct, "30", NULL };
		char *o = root_out(argv, 0); trim(o);
		const char *mq = o[0] == '{' ? dj_get(o, "match") : NULL, *okq = o[0] == '{' ? dj_get(o, "ok") : NULL;
		if (okq && dj_true(dj_ws(okq)) && mq && dj_true(dj_ws(mq))) {
			fp_fails_set(acct, 0); char *f = dj_str(dj_get(o, "finger"));
			str s = {0}; sadd(&s, "{\"ok\":true,\"acct\":"); sjson(&s, acct); sadd(&s, ",\"n\":"); sjson(&s, name); sadd(&s, ",\"finger\":"); sjson(&s, f ? f : ""); sadd(&s, "}");
			free(f); free(o); free(acct); reply_json(r->fd, &s); return;
		}
		if (okq && dj_true(dj_ws(okq))) {
			int n = fp_fails(acct) + 1; fp_fails_set(acct, n);
			health_add("signin", "a finger not recognised for the account %.100s (%d of %d)", name, n, FP_MAXFAIL);
			str e = {0};
			if (n >= FP_MAXFAIL) { sadd(&e, "{\"ok\":false,\"need\":\"password\",\"why\":\"tries\",\"error\":\"Too many fingers were not recognised: use your password.\"}"); reply(r->fd, 423, "application/json", e.b, e.n, NULL); }
			else { sadd(&e, "{\"ok\":false,\"left\":%d,\"error\":\"That finger was not recognised.\"}", FP_MAXFAIL - n); reply(r->fd, 403, "application/json", e.b, e.n, NULL); }
			free(e.b); free(o); free(acct); return;
		}
		fp_reply(r, o); free(o); free(acct); return;
	}
	free(acct); reply_err(r->fd, 404, "unknown fingerprint request");
}
