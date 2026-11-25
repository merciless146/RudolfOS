/* (included by rudolfd.c, after rudolfd-dev.inc: dev_run) ---------- 1.6.3 (docs/plan-1.6.3.md Part 7, step 1): Oracle's own password
 * manager. Its passwords are kept in the signed-in account's keyring (rudolf-portal --webpw, as the account's Linux user in its
 * session, as Blitzen's tokens are: rudolfd-dev.inc dev_token), unlocked by its sign-in and locked with it; never in the page's storage,
 * a file of rudolfd's or Oracle's profile.
 *   GET  oracle/pw/list   {"ok","signedIn","items":[{"origin","user","when"}]}: names only
 *   POST oracle/pw/get    body "ORIGIN\nUSER": {"ok","pw"}, for that exact origin and user only
 *   POST oracle/pw/set    body "ORIGIN\nUSER\nPASSWORD"
 *   POST oracle/pw/del    body "ORIGIN\nUSER"
 * ORIGIN is https://HOST[:PORT] exactly (a lower-case host, the tab's own top-level origin: the page asks only for the tab it fills,
 * after the person's click); nobody signed in, or Guest: nothing kept (409). Private and Tor windows never ask to save (the page).
 * The body goes to rudolf-portal on its standard input: nothing of it in a command line, a log or a file. */
static int webpw_ok_origin(const char *o, size_t n)
{
	if (n < 9 || n > 268 || strncmp(o, "https://", 8)) return 0;
	int colon = 0, digits = 0;
	for (size_t i = 8; i < n; i++) { char c = o[i];
		if (colon) { if (c < '0' || c > '9' || ++digits > 5) return 0; continue; }
		if (c == ':') { if (i == 8) return 0; colon = 1; continue; }
		if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-')) return 0; }
	return o[8] != '.' && o[8] != '-' && (!colon || digits);
}
/* the body's lines: origin, user and (want 3) the password; 0: not as said */
static int webpw_body(struct req *r, int want, const char **L, size_t *Ln)
{
	if (!r->body || !r->blen || r->blen > 1600 || memchr(r->body, 0, r->blen)) return 0;
	const char *p = r->body, *e = r->body + r->blen; int k = 0;
	while (k < want) { const char *nl = memchr(p, '\n', (size_t)(e - p)); const char *end = nl ? nl : e; L[k] = p; Ln[k] = (size_t)(end - p); k++; if (!nl) break; p = nl + 1; }
	if (k != want || (want == 2 && L[1] + Ln[1] != e) || (want == 3 && L[2] + Ln[2] != e)) return 0;
	if (!webpw_ok_origin(L[0], Ln[0]) || Ln[1] > 200) return 0;
	for (size_t i = 0; i < Ln[1]; i++) if ((unsigned char)L[1][i] < 0x20 || L[1][i] == 0x7f) return 0;
	if (want == 3) { if (!Ln[2] || Ln[2] > 1000) return 0; for (size_t i = 0; i < Ln[2]; i++) if (L[2][i] == '\r' || L[2][i] == '\n') return 0; }
	return 1;
}
static void api_webpw(struct req *r, const char *what, int post)
{
	char au[32], ae[160]; au_app_user(au, sizeof au, ae, sizeof ae);
	int list = !post && !strcmp(what, "list"), get = post && !strcmp(what, "get"), set = post && !strcmp(what, "set"), del = post && !strcmp(what, "del");
	if (!list && !get && !set && !del) { reply_err(r->fd, 404, "list, get, set or del"); return; }
	if (!strcmp(au, "rudolf")) { if (list) { str s = {0}; sadd(&s, "{\"ok\":true,\"signedIn\":false,\"items\":[]}"); reply_json(r->fd, &s); } else reply_err(r->fd, 409, "no account is signed in (Guest keeps no passwords)"); return; }
	const char *L[3] = {0}; size_t Ln[3] = {0};
	if (!list && !webpw_body(r, set ? 3 : 2, L, Ln)) { reply_err(r->fd, 400, "the site's origin (https://HOST[:PORT]) and the user name, one a line"); return; }
	char cmd[200]; snprintf(cmd, sizeof cmd, "set -a; . \"$0\"; set +a; exec " AU_PORTAL " --webpw %s", list ? "list" : get ? "get" : set ? "set" : "del");
	const char *a[] = { "/bin/su", "-s", "/bin/sh", au, "-c", cmd, ae, NULL };
	/* (its standard input: the body's lines, each ended) */
	char in[1700]; size_t inl = 0;
	for (int i = 0; !list && i < (set ? 3 : 2); i++) { memcpy(in + inl, L[i], Ln[i]); inl += Ln[i]; in[inl++] = '\n'; }
	int st; char err[300]; char *o = dev_run(a, list ? NULL : in, inl, 20, 0, &st, err, sizeof err);
	explicit_bzero(in, sizeof in); if (r->body) explicit_bzero(r->body, r->blen);
	if (o) trim(o);
	if (list) {
		static const char OK[] = "{\"ok\":true,";
		if (st || !o || strncmp(o, OK, sizeof OK - 1) || o[strlen(o) - 1] != '}') { free(o); reply_err(r->fd, 500, *err ? err : "the keyring could not be read"); return; }
		str s = {0}; sadd(&s, "{\"ok\":true,\"signedIn\":true,%s", o + sizeof OK - 1); free(o); reply_json(r->fd, &s); return;
	}
	if (get) {
		if (st || !o) { if (o) { explicit_bzero(o, strlen(o)); free(o); } reply_err(r->fd, st == 1 ? 404 : 500, st == 1 ? (strstr(err, "locked") ? "the keyring is locked" : "none kept for this site") : *err ? err : "the keyring could not be read"); return; }
		str s = {0}; sadd(&s, "{\"ok\":true,\"pw\":"); sjson(&s, o); sadd(&s, "}"); explicit_bzero(o, strlen(o)); free(o);
		reply(r->fd, 200, "application/json", s.b, s.n, NULL); if (s.b) { explicit_bzero(s.b, s.n); free(s.b); } return;   /* (the answer wiped once sent) */
	}
	int okd = !st && o && strstr(o, "\"ok\":true"); free(o);
	if (!okd) { reply_err(r->fd, st == 1 && del ? 404 : 500, *err ? err : set ? "it could not be kept" : "it could not be removed"); return; }
	if (set) health_add("account", "Oracle kept a website's password in the account's keyring"); else health_add("account", "Oracle removed a website's saved password");
	reply_ok(r->fd, set ? "kept" : "removed");
}
