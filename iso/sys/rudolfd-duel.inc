/* (included by rudolfd.c, not built on its own: hence .inc) ---------- 1.6.1: duels on the local network for the
 * Library's games (Ember Reach 0.5.2's LAN duels; rudolfos-library docs/duel-protocol.md).
 * A page can't take connections, so the host's computer relays: the game asks through the game kit, the desktop
 * (duel61.js, after asking the person) asks here, with the token and through the peer check as every request.
 *
 *   POST duel/host?ver=VER   the duel service (rudolf-duel serve) started for one match: as the signed-in account's own
 *                            Linux user (au_app_user; the desktop's user without accounts), never root; on the first
 *                            free port of 47860-47869, with a new room code; the firewall opened for that port to the
 *                            local network only (private addresses, never a cellular interface: firewall duel PORT,
 *                            through the network helper) while it runs. A process of rudolfd's own (the keeper)
 *                            waits for the service and closes the firewall again when it ends, whatever ends it (the
 *                            match over, the host's game gone, duel/stop, sign-out). One at a time: hosting again
 *                            stops the last. -> {url (ws://127.0.0.1:PORT/: the host's game joins on this computer),
 *                            code, port}
 *   POST duel/stop           the service stopped (its keeper closes the firewall); nothing hosted: nothing to do
 *   GET duel/find            the duels hosted on the local network (rudolf-duel find, as the account's user: mDNS)
 *                            -> {games: [{name, addr, port, url, code, ver}]}
 *   GET duel                 {hosting, port, code}
 */
#include <grp.h>
#include <pwd.h>
#include <sys/random.h>
#ifndef DUEL_BIN
#define DUEL_BIN "/usr/bin/rudolf-duel"
#endif
#ifndef DUEL_FW
#define DUEL_FW "/usr/lib/rudolf/firewall"
#endif
#define DUEL_STATE RUN "/duel"   /* "PID PORT CODE KEEPER": the service, while it runs */
#define DUEL_PORT0 47860
#define DUEL_PORTS 10

/* the firewall's hole for the service's port (port 0: closed) */
static void duel_fw(int port)
{
	char ps[16]; snprintf(ps, sizeof ps, "%d", port);
	const char *av[] = { "network", "firewall", "duel", port ? ps : "off", NULL };
	if ((root_have("network") ? root_net(av, 1) : -2) == -2) { char cmd[200]; snprintf(cmd, sizeof cmd, DUEL_FW " duel %s >/dev/null 2>&1", port ? ps : "off"); run(cmd); }
}
/* the service now (0: none): its pid, port and code, and its keeper's pid, if it is still the duel service */
static int duel_now(int *port, char *code, size_t cn, int *keeper)
{
	char *s = slurp(DUEL_STATE, NULL); if (!s) return 0;
	int pid = 0, pt = 0, kp = 0; char c[16] = "";
	if (sscanf(s, "%d %d %15s %d", &pid, &pt, c, &kp) != 4) pid = 0;
	free(s);
	if (pid > 1) { char p[64]; snprintf(p, sizeof p, "/proc/%d/comm", pid); char *cm = slurp(p, NULL); if (!cm || strncmp(cm, "rudolf-duel", 11)) pid = 0; free(cm); }
	if (pid <= 1) return 0;
	if (port) *port = pt; if (code) snprintf(code, cn, "%s", c); if (keeper) *keeper = kp;
	return pid;
}
/* stopped: the service told to end (its keeper closes the firewall); waits for it a little, and tidies up if the
   keeper is gone too */
static void duel_stop(void)
{
	int port = 0, kp = 0, pid = duel_now(&port, NULL, 0, &kp);
	if (!pid) { if (!access(DUEL_STATE, F_OK)) { unlink(DUEL_STATE); duel_fw(0); } return; }
	kill(pid, SIGTERM);
	for (int i = 0; i < 40 && !access(DUEL_STATE, F_OK); i++) usleep(50000);
	if (!access(DUEL_STATE, F_OK)) { if (kill(pid, 0) == 0) kill(pid, SIGKILL); unlink(DUEL_STATE); duel_fw(0); }
}
static int duel_ver_ok(const char *v)
{
	size_t n = v ? strlen(v) : 0; if (!n || n > 20) return 0;
	for (; *v; v++) if (!isalnum((unsigned char)*v) && !strchr(".+-", *v)) return 0;
	return 1;
}
/* the account's own Linux user (never root): its uid, gid and home */
static int duel_user(uid_t *u, gid_t *g, char *home, size_t hn)
{
	char au[32], ae[160]; au_app_user(au, sizeof au, ae, sizeof ae);
	struct passwd *pw = getpwnam(au);
	*u = pw ? pw->pw_uid : 1000; *g = pw ? pw->pw_gid : 1000; snprintf(home, hn, "%s", pw ? pw->pw_dir : "/tmp");
	return *u != 0;
}
/* in a child: become that user, and run the service (no environment but PATH and HOME) */
static void duel_exec(uid_t u, gid_t g, const char *home, char *const argv[], int out)
{
	int nul = open("/dev/null", O_RDWR | O_CLOEXEC);
	if (nul >= 0) dup2(nul, 0); if (out >= 0) { dup2(out, 1); dup2(out, 2); }
	for (int fd = 3; fd < 1024; fd++) close(fd);
	if (setgroups(0, NULL) || setgid(g) || setuid(u) || !getuid() || !geteuid()) _exit(126);
	if (chdir(home)) { if (chdir("/tmp")) {} }
	clearenv(); setenv("PATH", "/usr/bin:/bin", 1); setenv("HOME", home, 1); setenv("LANG", "C.UTF-8", 1);
	execv(argv[0], argv); _exit(127);
}
static void api_duel(struct req *r, const char *what, int post)
{
	if (!strcmp(what, "")) {
		int port = 0; char code[16] = ""; int pid = duel_now(&port, code, sizeof code, NULL);
		str s = {0}; sadd(&s, "{\"ok\":true,\"hosting\":%s,\"port\":%d,\"code\":", pid ? "true" : "false", pid ? port : 0); sjson(&s, pid ? code : ""); sadd(&s, "}");
		reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "stop")) { if (!post) { reply_err(r->fd, 405, "POST"); return; } duel_stop(); reply_ok(r->fd, "stopped"); return; }
	if (!strcmp(what, "find")) {
		uid_t u; gid_t g; char home[300]; if (!duel_user(&u, &g, home, sizeof home)) { reply_err(r->fd, 403, "not as root"); return; }
		int pf[2]; if (pipe2(pf, O_CLOEXEC)) { reply_err(r->fd, 500, "cannot look"); return; }
		signal(SIGCHLD, SIG_DFL);
		pid_t p = fork();
		if (p == 0) { char *av[] = { DUEL_BIN, "find", NULL }; int o = fcntl(pf[1], F_DUPFD, 10); duel_exec(u, g, home, av, o); }
		close(pf[1]);
		str s = {0}; sadd(&s, "%s", ""); char b[4096]; ssize_t n; time_t until = time(NULL) + 6;
		while (p > 0 && time(NULL) < until) { struct pollfd q = { pf[0], POLLIN, 0 }; if (poll(&q, 1, 500) <= 0) continue; n = read(pf[0], b, sizeof b - 1); if (n <= 0) break; b[n] = 0; if (s.n < 65536) sadd(&s, "%s", b); }
		close(pf[0]); if (p > 0) { kill(p, SIGKILL); while (waitpid(p, NULL, 0) < 0 && errno == EINTR) {} }
		/* (its answer as it came, if it is one: one line of JSON beginning {"ok":true,"games":[) */
		if (s.b && !strncmp(s.b, "{\"ok\":true,\"games\":[", 20) && strlen(s.b) < 65536) { reply(r->fd, 200, "application/json", s.b, strcspn(s.b, "\n"), NULL); free(s.b); return; }
		free(s.b); reply_err(r->fd, 500, "the network could not be searched"); return;
	}
	if (!strcmp(what, "host")) {
		if (!post) { reply_err(r->fd, 405, "POST"); return; }
		char *ver = qparam(r->query, "ver"); if (!duel_ver_ok(ver)) { reply_err(r->fd, 400, "ver: the game's version"); return; }
		char v[24]; snprintf(v, sizeof v, "%s", ver);
		uid_t u; gid_t g; char home[300]; if (!duel_user(&u, &g, home, sizeof home)) { reply_err(r->fd, 403, "the duel service never runs as root"); return; }
		if (access(DUEL_BIN, X_OK)) { reply_err(r->fd, 404, "this Rudolf OS has no duel service"); return; }
		duel_stop();   /* (one at a time) */
		/* the first free port of the range, a new code, this computer's name */
		int port = 0;
		for (int i = 0; i < DUEL_PORTS && !port; i++) {
			int t = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0), one = 1; if (t < 0) break;
			setsockopt(t, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
			struct sockaddr_in a = {0}; a.sin_family = AF_INET; a.sin_port = htons(DUEL_PORT0 + i); a.sin_addr.s_addr = htonl(INADDR_ANY);
			if (!bind(t, (struct sockaddr *)&a, sizeof a)) port = DUEL_PORT0 + i;
			close(t);
		}
		if (!port) { reply_err(r->fd, 409, "no free port for the duel service (47860-47869)"); return; }
		static const char CH[] = "ABCDEFGHJKMNPQRSTUVWXYZ23456789"; unsigned char rb[6]; char code[8];
		if (getrandom(rb, 6, 0) != 6) { reply_err(r->fd, 500, "no randomness"); return; }
		for (int i = 0; i < 6; i++) code[i] = CH[rb[i] % (sizeof CH - 1)]; code[6] = 0;
		char name[64] = ""; if (gethostname(name, sizeof name - 1)) snprintf(name, sizeof name, "Rudolf OS");
		for (char *c = name; *c; c++) if (!isalnum((unsigned char)*c) && !strchr(" .-_", *c)) *c = '-';
		char ps[16]; snprintf(ps, sizeof ps, "%d", port);
		int lg = open(RUN "/duel.log", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
		/* the keeper: rudolfd's own process (a session of its own), which starts the service as the account's user,
		   opens the firewall's hole, waits for the service to end, and closes the hole again */
		signal(SIGCHLD, SIG_DFL);
		pid_t kp = fork();
		if (kp < 0) { if (lg >= 0) close(lg); reply_err(r->fd, 500, "cannot start"); return; }
		if (kp == 0) {
			setsid(); signal(SIGCHLD, SIG_DFL); signal(SIGTERM, SIG_DFL); signal(SIGPIPE, SIG_IGN);
			for (int fd = 0; fd < 1024; fd++) if (fd != lg) close(fd);
			pid_t c = fork();
			if (c == 0) { char *av[] = { DUEL_BIN, "serve", "--port", ps, "--code", code, "--ver", v, "--name", name, NULL }; duel_exec(u, g, home, av, lg); }
			if (c < 0) _exit(1);
			char st[96]; snprintf(st, sizeof st, "%d %d %s %d\n", (int)c, port, code, (int)getpid());
			write_str(DUEL_STATE ".new", st); rename(DUEL_STATE ".new", DUEL_STATE);
			duel_fw(port);
			health_add("firewall", "port %d opened to the local network for a game's duel (rudolf-duel, as uid %u)", port, (unsigned)u);
			int ws; while (waitpid(c, &ws, 0) < 0 && errno == EINTR) {}
			duel_fw(0);
			{ char *s = slurp(DUEL_STATE, NULL); int pid = s ? atoi(s) : 0; free(s); if (pid == c) unlink(DUEL_STATE); }
			health_add("firewall", "the duel's port %d closed again", port);
			_exit(0);
		}
		if (lg >= 0) close(lg);
		while (waitpid(kp, NULL, WNOHANG) < 0 && errno == EINTR) {}
		/* answered once the service listens (3 s at most) */
		int up = 0;
		for (int i = 0; i < 60 && !up; i++) {
			int t = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0); if (t < 0) break;
			struct sockaddr_in a = {0}; a.sin_family = AF_INET; a.sin_port = htons(port); a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			if (!connect(t, (struct sockaddr *)&a, sizeof a) && duel_now(NULL, NULL, 0, NULL)) up = 1;
			close(t); if (!up) usleep(50000);
		}
		if (!up) { duel_stop(); reply_err(r->fd, 500, "the duel service did not start"); return; }
		str s = {0}; sadd(&s, "{\"ok\":true,\"url\":\"ws://127.0.0.1:%d/\",\"code\":\"%s\",\"port\":%d}", port, code, port);
		reply_json(r->fd, &s); return;
	}
	reply_err(r->fd, 404, "unknown duel request");
}
