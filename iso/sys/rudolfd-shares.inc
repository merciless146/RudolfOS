/* (included by rudolfd.c, not built on its own: hence .inc) ---------- 1.6.2 (docs/plan-1.6.2.md Part 35): network shares
 * File Explorer's Network: shared folders on Windows computers and NAS boxes (SMB), opened as a folder of the signed-in
 * account. rudolfd only checks and hands on: the network's side is rudolf-shares (run as nobody: it finds, resolves, asks
 * which SMB a server speaks and lists its shares), the mount is the drives helper's (rudolf-drives.c share-mount: the kernel's
 * own SMB client, for the account's own Linux user, under /run/rudolf/shares/UID/ID, which only that user may enter).
 *   GET  shares/found                    the computers and NAS boxes on the home network (rudolf-shares find)
 *   POST shares/list?server=S[&ip=A][&ver=2.1][&guest=1]   a server's shared folders; the body: the name, the password and
 *                                        the domain, a line each (an empty password: the one kept in the keyring, if any)
 *   POST shares/open?server=S&share=X[&ip=A][&ver=2.1][&guest=1][&keep=1]   opened (the body as for list); keep=1: the
 *                                        password kept in the account's keyring (rudolf-portal, as its user), keep=0 with a
 *                                        password typed: any kept one forgotten. {"ok":true,"id","path","encrypted",...}
 *   POST shares/close?id=ID              closed ("Disconnect")
 *   GET  shares/open                     the account's open shares
 *   GET  shares/kept?server=S&share=X&user=U[&ver=2.1]    whether a password is kept for it (never the password)
 *   POST shares/forget?server=S&share=X&user=U[&ver=2.1]  the kept password forgotten
 * Errors: {"ok":false,"error":TEXT,"why":WHY}, WHY one of password, denied, noshare, noanswer, smb1, smb20, smb21, name,
 * nomodule, busy. A share is the account's alone: the API answers about the signed-in account's (its own Linux user's, or
 * the desktop's with nobody signed in), fs/ reaches only its folders (shares_path_ok), and signing out closes them all
 * (shares_stop_uid, from au_session_stop). The password goes from the request's body to the helper's standard input and
 * the keyring only; it is never in a file, a log, an argument or an answer. */
#define SHARES_DIR RUN "/shares"
#define SHARES_META RUN "/shares-meta"
#define SHARES_BIN "/usr/bin/rudolf-shares"
/* the signed-in account (its id, "" for nobody) and the Linux user its shares are for */
static unsigned shares_cur(char *acct, size_t an)
{
	acct[0] = 0; char *cur = slurp(HOME_SESSION, NULL);
	if (cur) { trim(cur); if (home_acct_ok(cur) && strcmp(cur, "guest")) snprintf(acct, an, "%s", cur); free(cur); }
	unsigned u = *acct ? au_uid(acct) : 0; return u ? u : 1000;
}
/* the shares' own folder of the signed-in account: SHARES_DIR/UID/ID with ID 16 hex digits; p is it or inside it, with no
   hidden name on the way, and (made canonical: links followed) still inside it */
static int shares_path_ok(const char *p)
{
	if (!p || strncmp(p, SHARES_DIR "/", sizeof SHARES_DIR) || strlen(p) > 1000 || strstr(p, "/.") || strstr(p, "//")) return 0;
	char acct[80]; unsigned uid = shares_cur(acct, sizeof acct);
	char root[200]; int rl = snprintf(root, sizeof root, SHARES_DIR "/%u/", uid);
	if (strncmp(p, root, rl)) return 0;
	const char *id = p + rl; size_t k = 0; while (id[k] && id[k] != '/') { if (!isxdigit((unsigned char)id[k]) || isupper((unsigned char)id[k])) return 0; k++; }
	if (k != 16) return 0;
	char top[220]; snprintf(top, sizeof top, "%.*s", (int)(rl + 16), p);
	struct stat a, b; char par[220]; snprintf(par, sizeof par, SHARES_DIR "/%u", uid);
	if (lstat(top, &a) || !S_ISDIR(a.st_mode) || lstat(par, &b) || a.st_dev == b.st_dev) return 0;   /* (open: a mount of its own) */
	/* links followed: what exists of the path (a file about to be written may not), made canonical, must stay inside it */
	char t[1100], rp[PATH_MAX]; snprintf(t, sizeof t, "%s", p); size_t L = strlen(top);
	for (;;) {
		if (realpath(t, rp)) return !strncmp(rp, top, L) && (!rp[L] || rp[L] == '/');
		char *sl = strrchr(t, '/'); if (errno != ENOENT || !sl || (size_t)(sl - t) < L) return 0;
		*sl = 0;
	}
}
/* signing out (au_session_stop, which runs in drives-home or in rudolfd as root): every share of the account's user closed */
static void shares_stop_uid(unsigned uid)
{
	if (geteuid()) return;
	char d[200]; snprintf(d, sizeof d, SHARES_DIR "/%u", uid);
	DIR *dd = opendir(d); if (!dd) return; struct dirent *de; int n = 0;
	while ((de = readdir(dd))) {
		if (strlen(de->d_name) != 16) continue;
		char p[300]; snprintf(p, sizeof p, "%s/%s", d, de->d_name);
		if (!umount2(p, UMOUNT_NOFOLLOW) || !umount2(p, MNT_DETACH | UMOUNT_NOFOLLOW)) n++;
		rmdir(p);
		char m[300]; snprintf(m, sizeof m, SHARES_META "/%u-%s", uid, de->d_name); unlink(m);
	}
	closedir(dd);
	if (n) fprintf(stderr, "rudolfd: %d network share%s closed at sign-out\n", n, n == 1 ? "" : "s");
}
/* an id for (server, share, user, version): FNV-1a, 64 bits, as 16 hex digits (the folder's name and the keyring's key) */
static void shares_id(const char *srv, const char *sh, const char *user, const char *ver, char out[17])
{
	uint64_t h = 1469598103934665603ULL; const char *parts[4] = { srv, sh, user, ver };
	for (int i = 0; i < 4; i++) { for (const char *c = parts[i]; *c; c++) { h ^= (unsigned char)(i == 1 ? *c : tolower((unsigned char)*c)); h *= 1099511628211ULL; } h ^= 0xff; h *= 1099511628211ULL; }
	snprintf(out, 17, "%016llx", (unsigned long long)h);
}
static int shares_server_ok(const char *s)
{
	size_t n = s ? strlen(s) : 0; if (!n || n > 253 || s[0] == '.' || s[0] == '-') return 0;
	for (; *s; s++) if (!isalnum((unsigned char)*s) && !strchr(".-_:", *s)) return 0;
	return 1;
}
static int shares_name_ok(const char *s)
{
	size_t n = s ? strlen(s) : 0; if (!n || n > 80 || !strcmp(s, ".") || !strcmp(s, "..")) return 0;
	for (const unsigned char *c = (const unsigned char *)s; *c; c++) if (*c < 32 || *c == 127 || strchr("/\\:*?\"<>|,", *c)) return 0;
	return 1;
}
static int shares_ip_ok(const char *s) { unsigned char b[16]; return s && strlen(s) < 64 && (inet_pton(AF_INET, s, b) == 1 || inet_pton(AF_INET6, s, b) == 1); }
static void shares_err(struct req *r, int code, const char *why, const char *m)
{
	str s = {0}; sadd(&s, "{\"ok\":false,\"error\":"); sjson(&s, m); sadd(&s, ",\"why\":\"%s\"}", why);
	reply(r->fd, code, "application/json", s.b, s.n, NULL); free(s.b);
}
static void shares_why(struct req *r, const char *why, const char *srv, const char *sh)
{
	char m[400];
	if (!strcmp(why, "password")) shares_err(r, 401, why, "The name or password is not right, or this name may not open it.");
	else if (!strcmp(why, "denied")) shares_err(r, 403, why, "The server does not let this name in.");
	else if (!strcmp(why, "noshare")) { snprintf(m, sizeof m, "%s has no shared folder called \"%s\".", srv, sh ? sh : ""); shares_err(r, 404, why, m); }
	else if (!strcmp(why, "noanswer")) { snprintf(m, sizeof m, "%s does not answer. Is it on, and on this network?", srv); shares_err(r, 504, why, m); }
	else if (!strcmp(why, "name")) { snprintf(m, sizeof m, "No computer called %s was found on this network.", srv); shares_err(r, 404, why, m); }
	else if (!strcmp(why, "smb1")) { snprintf(m, sizeof m, "%s speaks only SMB 1, which is old and unsafe, so Rudolf OS never uses it. Turn on SMB 2 or 3 on it (on a NAS, in its file services settings).", srv); shares_err(r, 426, why, m); }
	else if (!strcmp(why, "smb20")) { snprintf(m, sizeof m, "%s speaks only SMB 2.0, which Rudolf OS does not use. Turn on SMB 3 on it.", srv); shares_err(r, 426, why, m); }
	else if (!strcmp(why, "smb21")) { snprintf(m, sizeof m, "%s speaks SMB 2.1 at most. To open it anyway, choose SMB 2.1 for it (More options).", srv); shares_err(r, 426, why, m); }
	else if (!strcmp(why, "nomodule")) shares_err(r, 501, why, "This version of Rudolf OS has no SMB client yet: it comes with an update.");
	else { snprintf(m, sizeof m, "%s could not be opened (%.160s).", srv, why); shares_err(r, 502, "proto", m); }
}
/* rudolf-shares, as nobody; its answer (NULL: it did not run); in: its standard input */
static char *shares_ask(const char *a1, const char *a2, const char *a3, const char *a4, const char *in, size_t inl, int secs, int *st)
{
	const char *av[] = { SHARES_BIN, a1, a2, a3, a4, NULL };
	char *o = dev_run(av, in, inl, secs, 1, st, NULL, 0); if (o) trim(o); return o;
}
static int shares_resolve(const char *srv, const char *ipq, char *ip, size_t in)
{
	if (ipq && *ipq) { if (!shares_ip_ok(ipq)) return -1; snprintf(ip, in, "%s", ipq); return 0; }
	if (shares_ip_ok(srv)) { snprintf(ip, in, "%s", srv); return 0; }
	int st; char *o = shares_ask("resolve", srv, NULL, NULL, NULL, 0, 10, &st);
	int ok = o && !st && shares_ip_ok(o); if (ok) snprintf(ip, in, "%s", o); free(o); return ok ? 0 : -1;
}
/* the body's three lines: name, password, domain (the password wiped by the caller) */
static int shares_body(struct req *r, char *user, size_t un, char *pass, size_t pn, char *dom, size_t dn)
{
	user[0] = pass[0] = dom[0] = 0;
	if (!r->body || !r->blen) return 0;
	if (r->blen > 1024 || memchr(r->body, 0, r->blen)) return -1;
	char *b = malloc(r->blen + 1); if (!b) return -1; memcpy(b, r->body, r->blen); b[r->blen] = 0; explicit_bzero(r->body, r->blen);
	char *l[3] = { b, NULL, NULL }; for (int i = 1; i < 3 && l[i - 1]; i++) { char *nl = strchr(l[i - 1], '\n'); if (nl) { *nl = 0; l[i] = nl + 1; } }
	for (int i = 0; i < 3; i++) if (l[i]) l[i][strcspn(l[i], "\r\n")] = 0;   /* (the third line's own newline too) */
	int bad = strlen(l[0]) >= un || (l[1] && strlen(l[1]) >= pn) || (l[2] && strlen(l[2]) >= dn);
	if (!bad) { snprintf(user, un, "%s", l[0]); if (l[1]) snprintf(pass, pn, "%s", l[1]); if (l[2]) snprintf(dom, dn, "%s", l[2]); }
	explicit_bzero(b, r->blen + 1); free(b); return bad ? -1 : 0;
}
static int shares_user_ok(const char *u) { for (const char *c = u; *c; c++) if ((unsigned char)*c < 32 || strchr("/\\[]:;|=,+*?<>\"", *c)) return 0; return strlen(u) < 105; }
static int shares_dom_ok(const char *d) { for (const char *c = d; *c; c++) if (!isalnum((unsigned char)*c) && !strchr(".-_", *c)) return 0; return strlen(d) < 65; }
/* the account's keyring (rudolf-portal as its user, in its session): 1 when there is one */
static int shares_keyring(char *au, size_t an, char *ae, size_t en) { au_app_user(au, an, ae, en); return strcmp(au, "rudolf") != 0; }
static int shares_kept(const char *id, char *pw, size_t pn)
{
	char au[32], ae[160]; pw[0] = 0; if (!shares_keyring(au, sizeof au, ae, sizeof ae)) return 0;
	char cmd[300]; snprintf(cmd, sizeof cmd, "set -a; . \"$0\"; set +a; exec " AU_PORTAL " --keyring-lookup share-%s", id);
	const char *a[] = { "/bin/su", "-s", "/bin/sh", au, "-c", cmd, ae, NULL }; int st;
	char *o = dev_run(a, NULL, 0, 20, 0, &st, NULL, 0);
	if (!st && o) { o[strcspn(o, "\r\n")] = 0; snprintf(pw, pn, "%s", o); }
	if (o) { explicit_bzero(o, strlen(o)); free(o); }
	return *pw != 0;
}
static int shares_keep(const char *id, const char *pw, const char *label, int clear)
{
	char au[32], ae[160]; if (!shares_keyring(au, sizeof au, ae, sizeof ae)) return -1;
	char cmd[300]; snprintf(cmd, sizeof cmd, "set -a; . \"$0\"; set +a; exec " AU_PORTAL " --keyring-%s share-%s \"$1\"", clear ? "clear" : "store", id);
	const char *a[] = { "/bin/su", "-s", "/bin/sh", au, "-c", cmd, ae, label, NULL }; int st;
	str in = {0}; if (!clear) sadd(&in, "%s\n", pw);
	char *o = dev_run(a, in.b, in.n, 20, 0, &st, NULL, 0); if (in.b) { explicit_bzero(in.b, in.n); free(in.b); } free(o);
	return st;
}
static void shares_api(struct req *r, const char *what, int post)
{
	char acct[80]; unsigned uid = shares_cur(acct, sizeof acct);
	if (!strcmp(what, "found")) {
		if (post) { reply_err(r->fd, 400, "use GET"); return; }
		/* (WS-Discovery's answers let in through the firewall for ten seconds, as Find scanners does: rudolfd-scan.inc) */
		{ const char *fw[] = { "network", "firewall", "wsd", "open", NULL }; scan_helper(fw, "/usr/lib/rudolf/firewall wsd open >/dev/null 2>&1"); }
		int st; char *o = shares_ask("find", NULL, NULL, NULL, NULL, 0, 15, &st);
		if (!o || st || strncmp(o, "{\"ok\":true", 10)) { free(o); shares_err(r, 502, "find", "The network could not be looked at."); return; }
		str s = {0}; sadd(&s, "%s", o); free(o); reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "open") && !post) {
		str s = {0}; sadd(&s, "{\"ok\":true,\"uid\":%u,\"shares\":[", uid); int n = 0;
		char d[200]; snprintf(d, sizeof d, SHARES_DIR "/%u", uid); DIR *dd = opendir(d); struct dirent *de; struct stat b;
		while (dd && (de = readdir(dd))) {
			if (strlen(de->d_name) != 16) continue;
			char p[300], m[300]; struct stat a; snprintf(p, sizeof p, "%s/%s", d, de->d_name);
			if (lstat(p, &a) || lstat(d, &b) || a.st_dev == b.st_dev) continue;
			snprintf(m, sizeof m, SHARES_META "/%u-%s", uid, de->d_name); char *t = slurp(m, NULL); if (!t) continue;
			char *f[7] = {0}; int k = 0; for (char *x = t, *sv = NULL, *y = strtok_r(x, "\t\n", &sv); y && k < 7; y = strtok_r(NULL, "\t\n", &sv)) f[k++] = y;
			if (k >= 6) { sadd(&s, "%s{\"id\":\"%s\",\"server\":", n++ ? "," : "", de->d_name); sjson(&s, f[0]); sadd(&s, ",\"share\":"); sjson(&s, f[1]); sadd(&s, ",\"user\":"); sjson(&s, f[2]);
				sadd(&s, ",\"ver\":"); sjson(&s, f[3]); sadd(&s, ",\"encrypted\":%s,\"guest\":%s,\"ip\":", !strcmp(f[4], "1") ? "true" : "false", !strcmp(f[5], "1") ? "true" : "false"); sjson(&s, k > 6 ? f[6] : ""); sadd(&s, ",\"path\":"); sjson(&s, p); sadd(&s, "}"); }
			free(t);
		}
		if (dd) closedir(dd);
		sadd(&s, "]}"); reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "close")) {
		if (!post) { reply_err(r->fd, 400, "use POST"); return; }
		char *id = qparam(r->query, "id"); char idc[20];
		if (!id || strlen(id) != 16 || strspn(id, "0123456789abcdef") != 16) { reply_err(r->fd, 400, "which share?"); return; }
		snprintf(idc, sizeof idc, "%s", id); char us[16]; snprintf(us, sizeof us, "%u", uid);
		const char *av[] = { "drives", "share-umount", us, idc, NULL }; int nul = root_null();
		int rc = root_call(av, nul >= 0 ? nul : 0, nul >= 0 ? nul : 1, 2); if (nul >= 0) close(nul);
		char m[300]; snprintf(m, sizeof m, SHARES_META "/%u-%s", uid, idc); unlink(m);
		if (rc) { reply_err(r->fd, 500, "it could not be closed"); return; }
		reply_ok(r->fd, "disconnected"); return;
	}
	/* the rest names a share or a server */
	char srv[260], sh[100], ipq[64], ver[8], ip[64]; const char *q;
	q = qparam(r->query, "server"); snprintf(srv, sizeof srv, "%s", q ? q : "");
	q = qparam(r->query, "share"); snprintf(sh, sizeof sh, "%s", q ? q : "");
	q = qparam(r->query, "ip"); snprintf(ipq, sizeof ipq, "%s", q ? q : "");
	q = qparam(r->query, "ver"); snprintf(ver, sizeof ver, "%s", q && !strcmp(q, "2.1") ? "2.1" : "3");
	if (q && *q && strcmp(q, "2.1") && strcmp(q, "3")) { shares_err(r, 400, "version", "Rudolf OS opens shares with SMB 3, or SMB 2.1 when asked; never SMB 1 or 2.0."); return; }
	q = qparam(r->query, "guest"); int guest = q && !strcmp(q, "1");
	q = qparam(r->query, "keep"); int keep = q && !strcmp(q, "1"), keep0 = q && !strcmp(q, "0");
	char quser[110]; q = qparam(r->query, "user"); snprintf(quser, sizeof quser, "%s", q ? q : "");
	if (!shares_server_ok(srv)) { reply_err(r->fd, 400, "the server: a computer's name or address"); return; }
	if (*ipq && !shares_ip_ok(ipq)) { reply_err(r->fd, 400, "the address is not one"); return; }
	if (!strcmp(what, "kept") || !strcmp(what, "forget")) {
		if (!shares_name_ok(sh) || !shares_user_ok(quser)) { reply_err(r->fd, 400, "which share?"); return; }
		char id[17]; shares_id(srv, sh, quser, ver, id);
		if (!strcmp(what, "forget")) { if (!post) { reply_err(r->fd, 400, "use POST"); return; } int st = shares_keep(id, "", "", 1); reply_ok(r->fd, st ? "nothing was kept" : "forgotten"); return; }
		char pw[520]; int k = shares_kept(id, pw, sizeof pw); explicit_bzero(pw, sizeof pw);
		char au[32], ae[160]; int kr = shares_keyring(au, sizeof au, ae, sizeof ae);
		str s = {0}; sadd(&s, "{\"ok\":true,\"kept\":%s,\"keyring\":%s}", k ? "true" : "false", kr ? "true" : "false"); reply_json(r->fd, &s); return;
	}
	if (!post || (strcmp(what, "list") && strcmp(what, "open"))) { reply_err(r->fd, post ? 404 : 400, post ? "unknown shares request" : "use POST"); return; }
	char user[110], pass[520], dom[70];
	if (shares_body(r, user, sizeof user, pass, sizeof pass, dom, sizeof dom)) { explicit_bzero(pass, sizeof pass); reply_err(r->fd, 400, "the name, password and domain: a line each"); return; }
	if (guest) { user[0] = 0; explicit_bzero(pass, sizeof pass); }
	if (!shares_user_ok(user) || !shares_dom_ok(dom) || (!guest && !*user)) { explicit_bzero(pass, sizeof pass); reply_err(r->fd, 400, guest ? "the domain is not one" : "a name to sign in with, or as a guest"); return; }
	if (!strcmp(what, "open") && !shares_name_ok(sh)) { explicit_bzero(pass, sizeof pass); reply_err(r->fd, 400, "the shared folder's name"); return; }
	char id[17]; shares_id(srv, *sh ? sh : "IPC$", guest ? "" : user, ver, id);
	int typed = *pass != 0, fromkr = 0;
	if (!guest && !typed && *sh) fromkr = shares_kept(id, pass, sizeof pass);
	if (shares_resolve(srv, ipq, ip, sizeof ip)) { explicit_bzero(pass, sizeof pass); shares_why(r, "name", srv, sh); return; }
	str in = {0}; sadd(&in, "%s\n%s\n%s\n", user, pass, dom);
	if (!strcmp(what, "list")) {
		int st; char *o = shares_ask("list", srv, ip, !strcmp(ver, "2.1") ? "2.1" : NULL, in.b, in.n, 25, &st); explicit_bzero(in.b, in.n); free(in.b); explicit_bzero(pass, sizeof pass);
		if (o && !strncmp(o, "{\"ok\":true", 10)) { str s = {0}; sadd(&s, "%s", o); free(o); reply_json(r->fd, &s); return; }
		char why[40] = "noanswer"; const char *w = o ? strstr(o, "\"why\":\"") : NULL; if (w) snprintf(why, sizeof why, "%.*s", (int)strcspn(w + 7, "\""), w + 7);
		free(o); shares_why(r, why, srv, NULL); return;
	}
	char us[16]; snprintf(us, sizeof us, "%u", uid);
	const char *av[] = { "drives", "share-mount", us, id, ip, srv, sh, ver, guest ? "1" : "0", NULL };
	char out[300]; int rc = root_home_ask(av, in.b, in.n, out, sizeof out); explicit_bzero(in.b, in.n); free(in.b);
	if (rc == 0) {
		char label[300]; snprintf(label, sizeof label, "Network share \\\\%s\\%s%s%s", srv, sh, *user ? " as " : "", user);
		int kept = 0;
		if (keep && typed) kept = !shares_keep(id, pass, label, 0);
		else if (keep0 && typed) shares_keep(id, "", "", 1);
		else kept = fromkr;
		explicit_bzero(pass, sizeof pass);
		int enc = strstr(out, "encrypted") != NULL;
		mkdir(SHARES_META, 0700); char m[300], md[600]; snprintf(m, sizeof m, SHARES_META "/%u-%s", uid, id);
		snprintf(md, sizeof md, "%s\t%s\t%s\t%s\t%d\t%d\t%s\n", srv, sh, *user ? user : "-", ver, enc, guest, ip); write_str(m, md); chmod(m, 0600);
		char p[200]; snprintf(p, sizeof p, SHARES_DIR "/%u/%s", uid, id);
		health_add("account", "a network share was opened: \\\\%.100s\\%.80s (SMB %s%s)", srv, sh, ver, enc ? ", encrypted" : "");
		str s = {0}; sadd(&s, "{\"ok\":true,\"id\":\"%s\",\"path\":", id); sjson(&s, p); sadd(&s, ",\"server\":"); sjson(&s, srv); sadd(&s, ",\"share\":"); sjson(&s, sh);
		sadd(&s, ",\"ip\":"); sjson(&s, ip); sadd(&s, ",\"ver\":\"%s\",\"encrypted\":%s,\"kept\":%s,\"already\":%s}", ver, enc ? "true" : "false", kept ? "true" : "false", strstr(out, "already") ? "true" : "false");
		reply_json(r->fd, &s); return;
	}
	explicit_bzero(pass, sizeof pass);
	if (rc == 3 && fromkr) { shares_err(r, 401, "password", "The password kept for it is not right any more: type it again."); return; }
	if (rc == 3) { shares_why(r, "password", srv, sh); return; }
	if (rc == 4) { shares_why(r, "noshare", srv, sh); return; }
	if (rc == 7) { shares_why(r, "nomodule", srv, sh); return; }
	if (rc == 5 || rc == 6) {   /* no answer, or the kernel could not agree with it (an SMB 1 server hangs up: "host is down"): which SMB does it speak? */
		int st; char *o = shares_ask("probe", ip, !strcmp(ver, "2.1") ? "2.1" : NULL, NULL, NULL, 0, 15, &st);
		char why[40] = ""; const char *w = o ? strstr(o, "\"why\":\"") : NULL; if (w) snprintf(why, sizeof why, "%.*s", (int)strcspn(w + 7, "\""), w + 7);
		free(o); shares_why(r, *why ? why : rc == 5 ? "noanswer" : out, srv, sh); return;
	}
	if (rc == 2) { reply_err(r->fd, 400, *out ? out : "refused"); return; }
	shares_err(r, 500, "proto", *out ? out : "the share could not be opened (the drives helper did not run)");
}
