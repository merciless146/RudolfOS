/* (included by rudolfd.c, not built on its own: hence .inc) ---------- 1.6.2 (docs/plan-1.6.2.md Part 33): a VPN for
 * the whole computer (Settings › Network › VPN, the taskbar's network pop-up). The VPN helper does the work as root
 * (rudolf-vpn.c: /usr/lib/rudolf/root/vpn, through rudolf-rootd); here is who may do what:
 *   GET  vpn                     the VPNs (never a key) and, for each, whether the signed-in account may turn it on and
 *                                off: {ok, wireguard, vpns: [{id, name, type, policy, kill, server, addresses, dns, on,
 *                                state, by, mayOn, mayOff, why}], active, me: {name, admin, child}}
 *   POST vpn/add                 an administrator's name and password, checked here; body {n, p, name, conf}: the file
 *                                (at most 64 KB) to the helper's standard input, never into a file of rudolfd's
 *   POST vpn/set                 {n, p, id, name | policy (everyone, admins, always) | kill (true, false)}, the same
 *   POST vpn/remove              {n, p, id}, the same
 *   POST vpn/on?id=ID            "Everyone": any account, no password; "Administrators only" and "Always on": a signed-in
 *                                administrator, or the body {n, p} of one; another VPN on is turned off first
 *   POST vpn/off                 "Always on": nobody; an account under parental controls: never; "Administrators only":
 *                                as on; "Everyone": any account
 * The account is the one signed in (HOME_SESSION, as Blitzen's know.inc reads it), never one the request names; its
 * role and parental controls are the page's accounts (the lsync copy, as desk_admin_check reads them). The VPN is the
 * whole computer's: turning off one another account turned on says so in the page ("Ann's VPN is on for everyone on
 * this computer"). At rudolfd's start an "Always on" VPN is turned on (vpn boot); while one is meant to be on, vpn tick
 * every half minute sets it up again should its interface be gone. */
typedef struct { char id[80], name[256]; int admin, child, signed_in; } vpn_who;
static void vpn_whoami(vpn_who *w)
{
	memset(w, 0, sizeof *w);
	char *cur = slurp(HOME_SESSION, NULL); if (cur) trim(cur);
	if (cur && *cur && home_acct_ok(cur)) { snprintf(w->id, sizeof w->id, "%s", cur); w->signed_in = 1; }
	free(cur);
	if (!w->signed_in) return;
	if (!strcmp(w->id, "guest")) { snprintf(w->name, sizeof w->name, "Guest"); return; }
	dls l; if (!dls_open(&l)) return;
	char *list = dls_accounts(&l);
	for (int i = 0; list; i++) {
		const char *a = dj_at(list, i); if (!a) break;
		char id[80]; dacct_id(a, id, sizeof id); if (strcmp(id, w->id)) continue;
		char *n = dj_str(dj_get(a, "n")), *rl = dj_str(dj_get(a, "r")); const char *pc = dj_get(a, "pc");
		snprintf(w->name, sizeof w->name, "%s", n ? n : ""); w->admin = rl && !strcmp(rl, "admin");
		/* (parental controls: the page's a.pc, set by an administrator for a standard account) */
		w->child = !w->admin && pc && *dj_ws(pc) == '{';
		free(n); free(rl); break;
	}
	free(list); dls_close(&l);
}
/* the helper: its standard output (its errors with err) and status in *st; in: its standard input (at most 70 KB,
   written by a process of its own so a long file never waits on a full pipe). -1 in *st: it could not be asked */
static char *vpn_helper(const char *const argv[], const char *in, size_t inl, int *st)
{
	*st = -1; if (inl > 70000) return strdup("");
	int pf[2], pi[2]; if (pipe2(pf, O_CLOEXEC)) return strdup("");
	if (pipe2(pi, O_CLOEXEC)) { close(pf[0]); close(pf[1]); return strdup(""); }
	pid_t wr = inl ? fork() : 0;
	if (wr == 0 && inl) { close(pf[0]); close(pf[1]); close(pi[0]); size_t o = 0; while (o < inl) { ssize_t w = write(pi[1], in + o, inl - o); if (w <= 0) { if (w < 0 && errno == EINTR) continue; break; } o += (size_t)w; } _exit(0); }
	close(pi[1]);
	pid_t p = fork();
	if (p == 0) { close(pf[0]); int nul = root_null(); int r = root_call(argv, pi[0], pf[1], nul >= 0 ? nul : 2); _exit(r < 0 ? 255 : r & 255); }
	close(pi[0]); close(pf[1]);
	str s = {0}; sadd(&s, "%s", ""); char b[4096]; ssize_t r;
	if (p > 0) while ((r = read(pf[0], b, sizeof b - 1)) > 0 || (r < 0 && errno == EINTR)) if (r > 0) { b[r] = 0; sadd(&s, "%s", b); }
	close(pf[0]);
	int ws = 0; if (p > 0) while (waitpid(p, &ws, 0) < 0 && errno == EINTR) {}
	if (wr > 0) while (waitpid(wr, NULL, 0) < 0 && errno == EINTR) {}
	int c = p > 0 && WIFEXITED(ws) ? WEXITSTATUS(ws) : 255;
	*st = c == 255 || c == RR_NOSTART || c == RR_REFUSED ? -1 : c;
	return s.b;
}
static int vpn_id_ok(const char *s) { if (!s || strlen(s) != 9 || s[0] != 'v') return 0; for (int i = 1; i < 9; i++) if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return 0; return 1; }
/* the helper's answer as the reply: its JSON as it is (200, or 409 when it says ok:false), 503 when it could not run */
static void vpn_reply(struct req *r, char *o, int st)
{
	if (st < 0 || !o || *o != '{') { free(o); reply_err(r->fd, 503, "this Rudolf OS cannot set up VPNs (its VPN helper did not start)"); return; }
	trim(o); int ok = strstr(o, "\"ok\":true") == o + 1;
	reply(r->fd, ok ? 200 : 409, "application/json", o, strlen(o), NULL); free(o);
}
/* one VPN's policy in the helper's list (NULL: no such VPN) */
static int vpn_policy(const char *list, const char *id, char *pol, size_t pn, char *by, size_t bn, int *on)
{
	const char *vs = list ? dj_get(list, "vpns") : NULL; *pol = 0; if (by) *by = 0; *on = 0;
	for (int i = 0; vs; i++) {
		const char *v = dj_at(vs, i); if (!v) break;
		char *x = dj_str(dj_get(v, "id")); int hit = x && !strcmp(x, id); free(x); if (!hit) continue;
		char *p = dj_str(dj_get(v, "policy")); snprintf(pol, pn, "%s", p ? p : "everyone"); free(p);
		if (by) { char *b = dj_str(dj_get(v, "by")); snprintf(by, bn, "%s", b ? b : ""); free(b); }
		*on = dj_true(dj_get(v, "on")); return 1;
	}
	return 0;
}
/* may w turn a VPN of policy pol on (off = 0) or off (off = 1)? 0 yes, else why; adm: an administrator's password came */
static const char *vpn_may(const vpn_who *w, const char *pol, int off, int adm)
{
	if (off && !strcmp(pol, "always")) return "This VPN is always on: nobody can turn it off. An administrator can change that in Settings › Network › VPN.";
	if (off && w->child) return "Your account has parental controls: it cannot turn a VPN off.";
	if (!strcmp(pol, "everyone")) return NULL;
	if (w->admin || adm) return NULL;
	return off ? "Only an administrator can turn this VPN off." : "Only an administrator can turn this VPN on.";
}
static int vpn_admin_body(struct req *r, char *why, size_t wn, char **who)
{
	char *an = NULL, *ap = NULL; *why = 0; if (who) *who = NULL;
	if (r->body && r->blen && r->body[0] == '{') { an = dbody_field(r, "n"); ap = dbody_field(r, "p"); }
	int adm = an && ap ? desk_admin_check(an, ap, why, wn) : -1; dfree_secret(ap);
	if (!adm && who) *who = an; else free(an);
	return adm;
}
static void api_vpn(struct req *r, const char *w, int post)
{
	if (!*w && !post) {
		const char *av[] = { "vpn", "list", NULL }; int st; char *o = vpn_helper(av, NULL, 0, &st);
		if (st < 0 || !o || *o != '{') { free(o); reply_err(r->fd, 503, "this Rudolf OS cannot set up VPNs (its VPN helper did not start)"); return; }
		vpn_who me; vpn_whoami(&me); trim(o);
		/* each VPN with mayOn, mayOff and why for the account signed in now */
		str s = {0}; const char *vs = dj_get(o, "vpns"); const char *ae = vs ? dj_end(vs, 0) : NULL;
		sadd(&s, "%.*s[", vs ? (int)(vs - o) : 0, o);
		for (int i = 0; vs; i++) {
			const char *v = dj_at(vs, i); if (!v) break; const char *ve = dj_end(v, 0); if (!ve) break;
			char *p = dj_str(dj_get(v, "policy")); const char *won = vpn_may(&me, p ? p : "everyone", 0, 0), *woff = vpn_may(&me, p ? p : "everyone", 1, 0); free(p);
			sadd(&s, "%s%.*s,\"mayOn\":%s,\"mayOff\":%s,\"why\":", i ? "," : "", (int)(ve - v - 1), v, won ? "false" : "true", woff ? "false" : "true");
			sjson(&s, woff ? woff : won ? won : ""); sadd(&s, "}");
		}
		sadd(&s, "]%s", ae ? ae : "}"); trim(s.b); s.n = strlen(s.b); if (s.n && s.b[s.n - 1] == '}') { s.n--; s.b[s.n] = 0; }
		sadd(&s, ",\"me\":{\"name\":"); sjson(&s, me.name); sadd(&s, ",\"admin\":%s,\"child\":%s,\"signedIn\":%s}}", me.admin ? "true" : "false", me.child ? "true" : "false", me.signed_in ? "true" : "false");
		if (!vs) { free(s.b); s = (str){0}; sadd(&s, "%s", o); }
		free(o); reply_json(r->fd, &s); return;
	}
	if (!post) { reply_err(r->fd, 405, "POST"); return; }
	if (!strcmp(w, "add") || !strcmp(w, "set") || !strcmp(w, "remove")) {
		char why[200]; char *an = NULL; int adm = vpn_admin_body(r, why, sizeof why, &an);
		if (adm) { reply_err(r->fd, adm == 429 ? 429 : 403, why[0] ? why : "an administrator's name and password are needed"); return; }
		if (!strcmp(w, "add")) {
			char *name = dbody_field(r, "name"), *conf = dbody_field(r, "conf"), *type = dbody_field(r, "type");
			size_t cl = conf ? strlen(conf) : 0;
			if (!name || !*name || strlen(name) > 64 || strchr(name, '\n') || !conf || !cl || cl > 65536 || (type && strcmp(type, "wireguard"))) {
				free(an); free(name); free(type); dfree_secret(conf); reply_err(r->fd, 400, "a name (1 to 64 characters) and a WireGuard file (at most 64 KB)"); return; }
			str in = {0}; sadd(&in, "%s\n", name); size_t nl = in.n; in.b = realloc(in.b, nl + cl + 1); memcpy(in.b + nl, conf, cl); in.n = nl + cl; in.b[in.n] = 0;
			dfree_secret(conf);
			const char *av[] = { "vpn", "add", "wireguard", NULL }; int st; char *o = vpn_helper(av, in.b, in.n, &st);
			explicit_bzero(in.b, in.n); free(in.b);
			if (st == 0) health_add("vpn", "a VPN was added by %.80s: %.80s", an, name);
			free(an); free(name); free(type); vpn_reply(r, o, st); return;
		}
		char *id = dbody_field(r, "id");
		if (!vpn_id_ok(id)) { free(an); free(id); reply_err(r->fd, 400, "id: a VPN"); return; }
		if (!strcmp(w, "remove")) {
			const char *av[] = { "vpn", "remove", id, NULL }; int st; char *o = vpn_helper(av, NULL, 0, &st);
			if (st == 0) health_add("vpn", "a VPN was removed by %.80s", an);
			free(an); free(id); vpn_reply(r, o, st); return;
		}
		/* set: one setting a request */
		char *name = dbody_field(r, "name"), *pol = dbody_field(r, "policy"); const char *kq = r->body ? dj_get(r->body, "kill") : NULL;
		const char *av[6] = { "vpn", "set", id, NULL, NULL, NULL }; const char *in = NULL; size_t inl = 0; char nb[80];
		if (name) { if (!*name || strlen(name) > 64 || strchr(name, '\n')) { free(an); free(id); free(name); free(pol); reply_err(r->fd, 400, "the name has 1 to 64 characters"); return; }
			snprintf(nb, sizeof nb, "%s\n", name); in = nb; inl = strlen(nb); av[3] = "name"; }
		else if (pol) { if (strcmp(pol, "everyone") && strcmp(pol, "admins") && strcmp(pol, "always")) { free(an); free(id); free(pol); reply_err(r->fd, 400, "policy: everyone, admins or always"); return; }
			av[3] = "policy"; av[4] = pol; }
		else if (kq) { av[3] = "kill"; av[4] = dj_true(kq) ? "on" : "off"; }
		else { free(an); free(id); reply_err(r->fd, 400, "name, policy or kill"); return; }
		int st; char *o = vpn_helper(av, in, inl, &st);
		if (st == 0) health_add("vpn", "a VPN's %s was changed by %.80s%s%s", av[3], an, av[4] ? ": " : "", av[4] ? av[4] : "");
		free(an); free(id); free(name); free(pol); vpn_reply(r, o, st); return;
	}
	if (!strcmp(w, "on") || !strcmp(w, "off")) {
		int off = !strcmp(w, "off");
		const char *la[] = { "vpn", "list", NULL }; int st; char *list = vpn_helper(la, NULL, 0, &st);
		if (st < 0 || !list || *list != '{') { free(list); reply_err(r->fd, 503, "this Rudolf OS cannot set up VPNs (its VPN helper did not start)"); return; }
		char *id = off ? NULL : qparam(r->query, "id"), act[16] = "";
		if (off) { const char *ac = dj_get(list, "active"); char *x = ac ? dj_str(ac) : NULL; if (x) snprintf(act, sizeof act, "%s", x); free(x);
			if (!*act) { free(list); reply_ok(r->fd, "No VPN is on."); return; } }
		const char *vid = off ? act : id;
		if (!vpn_id_ok(vid)) { free(list); reply_err(r->fd, 400, "id: a VPN"); return; }
		char pol[16], by[80]; int on;
		if (!vpn_policy(list, vid, pol, sizeof pol, by, sizeof by, &on)) { free(list); reply_err(r->fd, 404, "there is no such VPN"); return; }
		/* (turning one on while another is on turns that one off: allowed only when that one may be turned off too) */
		char curpol[16] = ""; { const char *ac = dj_get(list, "active"); char *x = ac ? dj_str(ac) : NULL; char cb[80]; int con;
			if (!off && x && strcmp(x, vid)) vpn_policy(list, x, curpol, sizeof curpol, cb, sizeof cb, &con); free(x); }
		free(list);
		vpn_who me; vpn_whoami(&me);
		char why[200] = ""; int adm = 0;
		const char *no = vpn_may(&me, pol, off, 0), *no2 = *curpol ? vpn_may(&me, curpol, 1, 0) : NULL;
		if ((no || no2) && r->body && r->blen && r->body[0] == '{') {
			/* (an administrator's password lets a standard account through "Administrators only", never "Always on" or a child's off) */
			char *who = NULL; int a2 = vpn_admin_body(r, why, sizeof why, &who); free(who);
			if (a2) { reply_err(r->fd, a2 == 429 ? 429 : 403, why[0] ? why : "an administrator's name and password are needed"); return; }
			adm = 1; no = vpn_may(&me, pol, off, 1); no2 = *curpol ? vpn_may(&me, curpol, 1, 1) : NULL;
		}
		if (no || no2) { str j = {0}; sadd(&j, "{\"ok\":false,\"needAdmin\":%s,\"error\":", (no ? strstr(no, "Only an administrator") != NULL : strstr(no2, "Only an administrator") != NULL) ? "true" : "false"); sjson(&j, no ? no : no2); sadd(&j, "}");
			reply(r->fd, 403, "application/json", j.b, j.n, NULL); free(j.b); free(id); return; }
		(void)adm;
		int st2; char *o;
		if (off) { const char *av[] = { "vpn", "down", NULL }; o = vpn_helper(av, NULL, 0, &st2); }
		else { const char *av[] = { "vpn", "up", vid, me.name[0] ? me.name : "-", NULL }; o = vpn_helper(av, NULL, 0, &st2); }
		free(id); vpn_reply(r, o, st2); return;
	}
	reply_err(r->fd, 404, "unknown VPN request");
}
/* at rudolfd's start: an "Always on" VPN on (in the background: its server may take a while to answer) */
static void vpn_boot(void)
{
	if (!root_have("vpn")) return;
	pid_t p = fork();
	if (p == 0) { if (fork() != 0) _exit(0); const char *av[] = { "vpn", "boot", NULL }; int nul = root_null(); root_call(av, nul, nul, nul); _exit(0); }
	if (p > 0) while (waitpid(p, NULL, 0) < 0 && errno == EINTR) {}
}
/* every half minute while a VPN is meant to be on (RUN/vpn/on: the helper's, root's) */
static void vpn_tick(void)
{
	static time_t last; time_t now = time(NULL); if (now - last < 30) return; last = now;
	if (access(RUN "/vpn/on", F_OK) || !root_have("vpn")) return;
	pid_t p = fork();
	if (p == 0) { if (fork() != 0) _exit(0); const char *av[] = { "vpn", "tick", NULL }; int nul = root_null(); root_call(av, nul, nul, nul); _exit(0); }
	if (p > 0) while (waitpid(p, NULL, 0) < 0 && errno == EINTR) {}
}
