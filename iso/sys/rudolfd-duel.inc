/* (included by rudolfd.c, not built on its own: hence .inc) ---------- 1.6.2 (docs/plan-1.6.2.md Part 30): the LAN duel
 * service, two-player games from the Rudolf OS Library on a home network (Ember Reach's duels). A game page cannot take
 * connections, so while a player hosts, this computer runs the LAN game relay (/usr/bin/rudolf-duel serve) and the
 * other's Rudolf OS finds it by multicast DNS. The game asks through the game kit (RG.lan), the desktop asks here
 * (desktop/duel62.js), so only the desktop reaches these (rudolfd's token and the peer check, as every request):
 *   POST duel/host?ver=VERSION&game=GAME   the relay started for that game (its game kit id: "ember-reach") and version
 *                                          (one running is stopped first): {ok, url ("ws://192.168.1.20:47731/"), code,
 *                                          name, port}
 *   POST duel/stop                         stopped
 *   GET  duel/find?game=GAME               that game's duels hosted on the network: {ok, games: [{name, url, code, ver}]}
 *   GET  duel/status                       {ok, running, port, code}
 * The relay speaks the Library relay's protocol (rudolfos-library docs/duel-protocol.md) with one room.
 * The relay runs as the signed-in account's own Linux user (the desktop's user with nobody signed in), never root, with
 * no new privileges, its own session; it answers only the local network, and the firewall's duel chain lets its port in
 * from private addresses, never on a cellular interface, only while it runs: opened before it starts, closed by its
 * watcher (a process of rudolfd's that waits for it) as soon as it ends, whether stopped here or by itself (it stops when
 * its duel ends, when no game hosts within a minute, or when nobody joins in 10 minutes). /run is a tmpfs:
 * nothing of it survives a restart. Finding runs rudolf-duel find as the same user. */
#include <grp.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <pwd.h>
#include <sys/prctl.h>
#ifndef DUEL_BIN
#define DUEL_BIN "/usr/bin/rudolf-duel"
#endif
#ifndef DUEL_PORT
#define DUEL_PORT 47731
#endif
#define DUELD RUN "/duel"
static int duel_word(const char *s, size_t max, const char *more)
{
	size_t n = s ? strlen(s) : 0; if (!n || n > max) return 0;
	for (; *s; s++) if (!isalnum((unsigned char)*s) && !strchr(more, *s)) return 0;
	return 1;
}
/* the firewall's duel chain: through the network helper (1.6.1 Part 11), or rudolfd itself when it does not start */
static void duel_fw(int on)
{
	char ps[8]; snprintf(ps, sizeof ps, "%d", DUEL_PORT);
	const char *a1[] = { "network", "firewall", "duel", "on", ps, NULL }, *a0[] = { "network", "firewall", "duel", "off", NULL };
	if ((root_have("network") ? root_net(on ? a1 : a0, 1) : -2) == -2) {
		char c[120]; snprintf(c, sizeof c, "/usr/lib/rudolf/firewall duel %s%s >/dev/null 2>&1", on ? "on " : "off", on ? ps : ""); run(c);
	}
}
/* the relay's process while it runs: the pid in DUELD/pid, still rudolf-duel */
static int duel_pid(void)
{
	char *p = slurp(DUELD "/pid", NULL); int v = p ? atoi(p) : 0; free(p);
	if (v <= 1 || kill(v, 0)) return 0;
	char f[64]; snprintf(f, sizeof f, "/proc/%d/comm", v); char *c = slurp(f, NULL); if (c) trim(c);
	int ok = c && !strcmp(c, "rudolf-duel"); free(c);
	return ok ? v : 0;
}
static void duel_stop(void)
{
	int p = duel_pid();
	if (p) { kill(p, SIGTERM); for (int i = 0; i < 20 && !kill(p, 0); i++) usleep(50000); if (!kill(p, 0)) kill(p, SIGKILL); }
	unlink(DUELD "/pid"); unlink(DUELD "/state"); duel_fw(0);
}
/* the local network's address: the first private IPv4 address of an interface that is up (not loopback, point-to-point
   or a cellular modem's) */
static int duel_addr(char *out, size_t on)
{
	struct ifaddrs *ia = NULL; int got = 0; char *cell = slurp(RUN "/firewall-cellular", NULL);
	if (getifaddrs(&ia)) { free(cell); return 0; }
	for (struct ifaddrs *i = ia; i && !got; i = i->ifa_next) {
		if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET || !(i->ifa_flags & IFF_UP) || (i->ifa_flags & (IFF_LOOPBACK | IFF_POINTOPOINT))) continue;
		if (cell) { int c = 0; for (char *l = cell; *l; ) { size_t n = strcspn(l, "\n"); if (n == strlen(i->ifa_name) && !strncmp(l, i->ifa_name, n)) c = 1; l += n; if (*l) l++; } if (c) continue; }
		uint32_t a = ntohl(((struct sockaddr_in *)i->ifa_addr)->sin_addr.s_addr);
		if (!((a >> 24) == 10 || (a >> 20) == (172 << 4 | 1) || (a >> 16) == (192 << 8 | 168) || (a >> 16) == (169 << 8 | 254))) continue;
		inet_ntop(AF_INET, &((struct sockaddr_in *)i->ifa_addr)->sin_addr, out, on); got = 1;
	}
	freeifaddrs(ia); free(cell);
	return got;
}
/* the account's own Linux user (au_app_user), never root */
static int duel_user(uid_t *u, gid_t *g, char *home, size_t hn)
{
	char au[32], ae[160]; au_app_user(au, sizeof au, ae, sizeof ae);
	struct passwd *pw = getpwnam(au);
	*u = pw ? pw->pw_uid : 1000; *g = pw ? pw->pw_gid : 1000; snprintf(home, hn, "%s", pw && pw->pw_dir ? pw->pw_dir : "/tmp");
	return *u != 0 && *g != 0;
}
/* in a child: the account's user, no new privileges, nothing of rudolfd's (its descriptors, its environment) */
static void duel_become(uid_t u, gid_t g, const char *home, int out)
{
	setsid();
	int nul = open("/dev/null", O_RDWR | O_CLOEXEC); dup2(nul >= 0 ? nul : 0, 0); dup2(out >= 0 ? out : nul, 1); dup2(nul >= 0 ? nul : 2, 2);
	for (int fd = 3; fd < 1024; fd++) close(fd);
	if (setgroups(0, NULL) || setgid(g) || setuid(u) || !getuid() || !geteuid()) _exit(126);
	prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	if (chdir(home)) { if (chdir("/tmp")) {} }
	clearenv(); setenv("HOME", home, 1); setenv("LANG", "C.UTF-8", 1); setenv("PATH", "/usr/bin:/bin", 1);
}
static void api_duel(struct req *r, const char *what, int post)
{
	if (access(DUEL_BIN, X_OK)) { reply_err(r->fd, 404, "this Rudolf OS has no LAN game relay"); return; }
	if (!strcmp(what, "status")) {
		int p = duel_pid(); char *st = p ? slurp(DUELD "/state", NULL) : NULL; if (st) trim(st);
		str s = {0}; sadd(&s, "{\"ok\":true,\"running\":%s,\"port\":%d,\"code\":", p ? "true" : "false", DUEL_PORT); sjson(&s, st ? st : ""); sadd(&s, "}"); free(st);
		reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "find")) {
		char *gm = qparam(r->query, "game"); if (!duel_word(gm, 40, "-")) { reply_err(r->fd, 400, "game=GAME"); return; }
		char game[48]; snprintf(game, sizeof game, "%s", gm);
		uid_t u; gid_t g; char home[300]; if (!duel_user(&u, &g, home, sizeof home)) { reply_err(r->fd, 403, "not as root"); return; }
		int pf[2]; if (pipe2(pf, O_CLOEXEC)) { reply_err(r->fd, 500, "cannot look"); return; }
		signal(SIGCHLD, SIG_DFL);
		pid_t p = fork();
		if (p == 0) { duel_become(u, g, home, pf[1]); char *av[] = { DUEL_BIN, "find", "--game", game, "--ms", "2500", NULL }; execv(av[0], av); _exit(127); }
		close(pf[1]);
		str o = {0}; sadd(&o, "%s", ""); char b[4096]; time_t until = time(NULL) + 6;
		while (p > 0 && o.n < 65536) { struct pollfd q = { pf[0], POLLIN, 0 }; if (time(NULL) > until || poll(&q, 1, 1000) < 0) break; if (!q.revents) continue; ssize_t n = read(pf[0], b, sizeof b - 1); if (n <= 0) break; b[n] = 0; sadd(&o, "%s", b); }
		close(pf[0]); if (p > 0) { kill(p, SIGKILL); waitpid(p, NULL, 0); }
		if (!o.b || strncmp(o.b, "{\"ok\":", 6)) { free(o.b); reply_err(r->fd, 500, "Rudolf OS couldn't look on the network."); return; }
		reply(r->fd, 200, "application/json", o.b, o.n, NULL); free(o.b); return;
	}
	if (!post) { reply_err(r->fd, 405, "use POST"); return; }
	if (!strcmp(what, "stop")) { duel_stop(); reply_ok(r->fd, "stopped"); return; }
	if (strcmp(what, "host")) { reply_err(r->fd, 404, "unknown duel action"); return; }
	char *vq = qparam(r->query, "ver"), *gq = qparam(r->query, "game");
	if (!duel_word(vq, 20, ".+-") || !duel_word(gq, 40, "-")) { reply_err(r->fd, 400, "ver=VERSION and game=GAME"); return; }
	char v[24], game[48], addr[64], name[48], code[8];
	snprintf(v, sizeof v, "%s", vq); snprintf(game, sizeof game, "%s", gq);
	uid_t u; gid_t g; char home[300]; if (!duel_user(&u, &g, home, sizeof home)) { reply_err(r->fd, 403, "not as root"); return; }
	duel_stop();
	if (!duel_addr(addr, sizeof addr)) { reply_err(r->fd, 409, "This computer isn't on a home network (Wi-Fi or a cable): connect to the same network as the other player."); return; }
	/* the room code: six of the relay's letters and digits (protocol.mjs's: no 0/O, 1/I/L), from the kernel's random numbers */
	static const char AL[] = "ABCDEFGHJKMNPQRSTUVWXYZ23456789"; int rf = open("/dev/urandom", O_RDONLY | O_CLOEXEC), k6 = 0;
	while (rf >= 0 && k6 < 6) { unsigned char b; if (read(rf, &b, 1) != 1) break; if (b < 248) code[k6++] = AL[b % 31]; }   /* (248 = 31 * 8: none favoured) */
	if (rf >= 0) close(rf);
	if (k6 < 6) { reply_err(r->fd, 500, "no random numbers"); return; }
	code[6] = 0;
	/* the computer's name, as the other player sees it */
	char hn[64] = ""; if (gethostname(hn, sizeof hn - 1)) hn[0] = 0; hn[sizeof hn - 1] = 0;
	size_t k = 0; for (char *c = hn; *c && k < sizeof name - 1; c++) if (isalnum((unsigned char)*c) || *c == '-' || *c == ' ') name[k++] = *c; name[k] = 0; if (!k) snprintf(name, sizeof name, "Rudolf OS");
	mkdir(DUELD, 0755);
	{ char st[64]; snprintf(st, sizeof st, "%s\n", code); write_str(DUELD "/state", st); }
	duel_fw(1);
	/* the watcher: a process of its own (rudolfd's request ends), that starts the relay as the account's user and closes
	   the firewall's port the moment it ends */
	signal(SIGCHLD, SIG_DFL);
	pid_t w = fork();
	if (w == 0) {
		setsid(); for (int fd = 3; fd < 1024; fd++) close(fd);
		pid_t w2 = fork(); if (w2) _exit(w2 > 0 ? 0 : 1);
		signal(SIGCHLD, SIG_DFL);
		int lf = open(DUELD "/log", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
		char ps[8]; snprintf(ps, sizeof ps, "%d", DUEL_PORT);
		pid_t c = fork();
		if (c == 0) { duel_become(u, g, home, -1); if (lf >= 0) { dup2(lf, 1); dup2(lf, 2); }
			char *av[] = { DUEL_BIN, "serve", "--port", ps, "--code", code, "--v", v, "--game", game, "--name", name, "--addr", addr, "--wait", "60", NULL }; execv(av[0], av); _exit(127); }
		if (lf >= 0) close(lf);
		if (c > 0) { char pp[24]; snprintf(pp, sizeof pp, "%d\n", (int)c); write_str(DUELD "/pid", pp); while (waitpid(c, NULL, 0) < 0 && errno == EINTR) {} }
		/* it ended: the port closed, unless another relay has been started since */
		char *now_p = slurp(DUELD "/pid", NULL); int same = now_p && atoi(now_p) == (int)c; free(now_p);
		if (same || c <= 0) { unlink(DUELD "/pid"); unlink(DUELD "/state"); duel_fw(0); }
		_exit(0);
	}
	if (w > 0) waitpid(w, NULL, 0);
	/* up when its port answers (a second at most) */
	int up = 0;
	for (int i = 0; i < 40 && !up; i++) {
		int s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0); struct sockaddr_in a = {0}; a.sin_family = AF_INET; a.sin_port = htons(DUEL_PORT); a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		if (s >= 0 && !connect(s, (struct sockaddr *)&a, sizeof a)) up = 1;
		if (s >= 0) close(s);
		if (!up) usleep(50000);
	}
	if (!up || !duel_pid()) { duel_stop(); reply_err(r->fd, 500, "Rudolf OS couldn't start the LAN game relay."); return; }
	str s = {0}; sadd(&s, "{\"ok\":true,\"url\":\"ws://%s:%d/\",\"port\":%d,\"code\":\"%s\",\"name\":", addr, DUEL_PORT, DUEL_PORT, code); sjson(&s, name); sadd(&s, "}");
	reply_json(r->fd, &s);
}
