/* (included by rudolfd.c, not built on its own: hence .inc) ---------- 1.6.2 (docs/plan-1.6.2.md Part 33): a VPN for
 * the whole computer (Settings › Network › VPN, the taskbar's network pop-up). The VPN helper does the work as root
 * (rudolf-vpn.c: /usr/lib/rudolf/root/vpn, through rudolf-rootd); here is who may do what:
 *   GET  vpn                     the VPNs (never a key): {ok, wireguard, vpns: [{id, name, type, server, addresses, dns,
 *                                whole, on, state, since, by}], active, me: {name, admin, signedIn}}
 *   POST vpn/add                 an administrator's name and password, checked here; body {n, p, name, conf}: the file
 *                                (at most 64 KB) to the helper's standard input, never into a file of rudolfd's
 *   POST vpn/set                 {n, p, id, name}, the same
 *   POST vpn/remove              {n, p, id}, the same
 *   POST vpn/on?id=ID            any account (another VPN on is turned off first)
 *   POST vpn/off                 any account
 * The account is the one signed in (HOME_SESSION, as Blitzen's know.inc reads it), never one the request names; its
 * name is said with the VPN it turned on (by). While one is meant to be on, vpn tick every half minute sets it up again
 * should its interface be gone. */
typedef struct { char id[80], name[256]; int admin, signed_in; } vpn_who;
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
		char *n = dj_str(dj_get(a, "n")), *rl = dj_str(dj_get(a, "r"));
		snprintf(w->name, sizeof w->name, "%s", n ? n : ""); w->admin = rl && !strcmp(rl, "admin");
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
		str s = {0}; size_t L = strlen(o); if (L && o[L - 1] == '}') o[--L] = 0;
		sadd(&s, "%s,\"me\":{\"name\":", o); sjson(&s, me.name); sadd(&s, ",\"admin\":%s,\"signedIn\":%s}}", me.admin ? "true" : "false", me.signed_in ? "true" : "false");
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
		char *name = dbody_field(r, "name"); char nb[80];
		if (!name || !*name || strlen(name) > 64 || strchr(name, '\n')) { free(an); free(id); free(name); reply_err(r->fd, 400, "the name has 1 to 64 characters"); return; }
		snprintf(nb, sizeof nb, "%s\n", name);
		const char *av[] = { "vpn", "set", id, "name", NULL }; int st; char *o = vpn_helper(av, nb, strlen(nb), &st);
		if (st == 0) health_add("vpn", "a VPN was renamed by %.80s", an);
		free(an); free(id); free(name); vpn_reply(r, o, st); return;
	}
	if (!strcmp(w, "on") || !strcmp(w, "off")) {
		int st; char *o;
		if (!strcmp(w, "off")) { const char *av[] = { "vpn", "down", NULL }; o = vpn_helper(av, NULL, 0, &st); vpn_reply(r, o, st); return; }
		const char *id = qparam(r->query, "id");   /* (qparam's own buffer: never freed) */
		if (!vpn_id_ok(id)) { reply_err(r->fd, 400, "id: a VPN"); return; }
		const char *la[] = { "vpn", "list", NULL }; char *list = vpn_helper(la, NULL, 0, &st);
		if (st < 0 || !list || *list != '{') { free(list); reply_err(r->fd, 503, "this Rudolf OS cannot set up VPNs (its VPN helper did not start)"); return; }
		char key[24]; snprintf(key, sizeof key, "\"id\":\"%s\"", id); int there = strstr(list, key) != NULL; free(list);
		if (!there) { reply_err(r->fd, 404, "there is no such VPN"); return; }
		vpn_who me; vpn_whoami(&me);
		const char *av[] = { "vpn", "up", id, me.name[0] ? me.name : "-", NULL }; o = vpn_helper(av, NULL, 0, &st);
		vpn_reply(r, o, st); return;
	}
	reply_err(r->fd, 404, "unknown VPN request");
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
