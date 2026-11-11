/*
 * rudolf-shellfix - put the system's shell (Oracle's tabs, an Isolated Web App) into a Chromium profile that has an
 * older copy of it (1.5.8). GPL-2.0-or-later.
 *
 * Since Chromium 155 the session installs the shell in Isolated Web App developer mode from its file
 * (--install-isolated-web-app-from-file), and Chromium then never replaces an installed copy: "App is already
 * installed", whatever the version. The Vivobook ran the shell of 2026-11-01 for ten days of shell changes (YouTube's
 * full screen among them). So when the profile's copy differs from the system's file, the session runs this before the
 * desktop starts: it starts Chromium once with the profile (on the session's X display, out of sight), and through the
 * DevTools protocol over a pipe (--remote-debugging-pipe: no network port) uninstalls the old copy (PWA.uninstall) and
 * installs the file (PWA.install); Chromium 157 updates no installed copy in place. Chromium deletes the app's own
 * storage with it: the desktop's databases come back from rudolfd's mirror as at the first start of the shell, and
 * websites in Oracle's tabs ask to sign in again. It runs only when the shell changed (a release with changes to it).
 *
 *   rudolf-shellfix CHROME PROFILE BUNDLE ID [extra Chromium options...]
 * Exit 0 when the profile has the file's copy, 1 when Chromium refused (its message on stderr), 2 on a usage error,
 * 3 when Chromium did not answer within 90 s. It is run as the desktop user.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int to_chrome, from_chrome; static pid_t kid; static time_t deadline;
static char buf[1 << 20]; static size_t blen;

static void send_msg(const char *m)
{
	size_t n = strlen(m) + 1; const char *p = m;
	while (n) { ssize_t w = write(to_chrome, p, n); if (w < 0 && errno == EINTR) continue; if (w <= 0) { fprintf(stderr, "rudolf-shellfix: Chromium closed its pipe\n"); exit(3); } p += w; n -= w; }
}
/* the next message (NUL-terminated JSON) from Chromium, into out */
static int next_msg(char *out, size_t on)
{
	for (;;) {
		char *z = memchr(buf, 0, blen);
		if (z) { size_t l = z - buf; if (l >= on) l = on - 1; memcpy(out, buf, l); out[l] = 0; memmove(buf, z + 1, blen - (z - buf) - 1); blen -= z - buf + 1; return 1; }
		int left = (int)(deadline - time(NULL)); if (left <= 0) return 0;
		struct pollfd p = { from_chrome, POLLIN, 0 }; int r = poll(&p, 1, left * 1000); if (r < 0 && errno == EINTR) continue; if (r <= 0) return 0;
		if (blen >= sizeof buf - 1) blen = 0;   /* a message larger than the buffer: dropped */
		ssize_t g = read(from_chrome, buf + blen, sizeof buf - 1 - blen); if (g <= 0) return 0; blen += g;
	}
}
/* send a command and wait for its answer (other messages are skipped) */
static int call(int id, const char *session, const char *method, const char *params, char *out, size_t on)
{
	char m[4096];
	if (session) snprintf(m, sizeof m, "{\"id\":%d,\"sessionId\":\"%s\",\"method\":\"%s\",\"params\":%s}", id, session, method, params);
	else snprintf(m, sizeof m, "{\"id\":%d,\"method\":\"%s\",\"params\":%s}", id, method, params);
	send_msg(m);
	char want[32]; snprintf(want, sizeof want, "\"id\":%d,", id); char want2[32]; snprintf(want2, sizeof want2, "\"id\":%d}", id);
	while (next_msg(out, on)) if (strstr(out, want) || strstr(out, want2)) return !strstr(out, "\"error\":");
	fprintf(stderr, "rudolf-shellfix: no answer to %s\n", method); kill(kid, SIGKILL); exit(3);
}
static int jstr(const char *j, const char *key, char *out, size_t on)
{
	char k[64]; snprintf(k, sizeof k, "\"%s\":\"", key); const char *p = strstr(j, k); if (!p) return 0; p += strlen(k);
	size_t i = 0; while (*p && *p != '"' && i + 1 < on) out[i++] = *p++; out[i] = 0; return i > 0;
}
static int safe(const char *s, const char *extra)
{
	if (!*s) return 0; for (; *s; s++) if (!(*s >= 'a' && *s <= 'z') && !(*s >= 'A' && *s <= 'Z') && !(*s >= '0' && *s <= '9') && !strchr(extra, *s)) return 0; return 1;
}
int main(int argc, char **argv)
{
	if (argc < 5) { fprintf(stderr, "usage: rudolf-shellfix CHROME PROFILE BUNDLE ID [Chromium options...]\n"); return 2; }
	const char *chrome = argv[1], *profile = argv[2], *bundle = argv[3], *id = argv[4];
	if (!safe(id, "") || strlen(id) != 56 || !safe(bundle, "/._-") || bundle[0] != '/' || strstr(bundle, "..")) { fprintf(stderr, "rudolf-shellfix: bad ID or bundle path\n"); return 2; }
	int in[2], out[2]; if (pipe(in) || pipe(out)) return 3;
	signal(SIGPIPE, SIG_IGN);
	kid = fork(); if (kid < 0) return 3;
	if (!kid) {
		/* Chromium reads commands from fd 3 and writes answers to fd 4 */
		int a = fcntl(in[0], F_DUPFD, 10), b = fcntl(out[1], F_DUPFD, 10);
		close(in[0]); close(in[1]); close(out[0]); close(out[1]);
		dup2(a, 3); dup2(b, 4); close(a); close(b);
		char ud[1100]; snprintf(ud, sizeof ud, "--user-data-dir=%s", profile);
		char *av[80]; int n = 0;
		av[n++] = (char *)chrome; av[n++] = ud; av[n++] = "--remote-debugging-pipe"; av[n++] = "--no-first-run"; av[n++] = "--no-default-browser-check";
		av[n++] = "--enable-features=IsolatedWebApps,IsolatedWebAppDevMode"; av[n++] = "--window-position=-4000,-4000"; av[n++] = "--window-size=200,200";
		av[n++] = "--class=RudolfShellFix"; av[n++] = "--disable-session-crashed-bubble"; av[n++] = "--password-store=basic";
		/* as a test browser runs (Playwright's switches): no prompts, no throttling of a window kept out of sight; without
		   them the removal waited for an answer that never came (Chromium 157) */
		av[n++] = "--enable-automation"; av[n++] = "--disable-backgrounding-occluded-windows"; av[n++] = "--disable-renderer-backgrounding";
		av[n++] = "--disable-background-timer-throttling"; av[n++] = "--disable-extensions"; av[n++] = "--disable-sync"; av[n++] = "--disable-component-update";
		av[n++] = "--disable-background-networking"; av[n++] = "--disable-default-apps"; av[n++] = "--disable-hang-monitor"; av[n++] = "--no-service-autorun";
		for (int i = 5; i < argc && n < 75; i++) av[n++] = argv[i];
		av[n++] = "about:blank"; av[n] = NULL;
		int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 0); }
		execv(chrome, av); _exit(127);
	}
	close(in[0]); close(out[1]); to_chrome = in[1]; from_chrome = out[0];
	deadline = time(NULL) + 90;
	static char r[1 << 20]; char target[128] = "", session[128] = "", p[2048];
	int ok = 0;
	call(1, NULL, "Target.getTargets", "{}", r, sizeof r);
	{ const char *t = strstr(r, "\"type\":\"page\""); if (t) { const char *s = t; while (s > r && strncmp(s, "\"targetId\":\"", 12)) s--; if (!strncmp(s, "\"targetId\":\"", 12)) jstr(s, "targetId", target, sizeof target); } }
	if (!*target) { call(2, NULL, "Target.createTarget", "{\"url\":\"about:blank\"}", r, sizeof r); jstr(r, "targetId", target, sizeof target); }
	if (!*target) { fprintf(stderr, "rudolf-shellfix: no page in Chromium\n"); goto done; }
	snprintf(p, sizeof p, "{\"targetId\":\"%s\",\"flatten\":true}", target);
	call(3, NULL, "Target.attachToTarget", p, r, sizeof r);
	if (!jstr(r, "sessionId", session, sizeof session)) { fprintf(stderr, "rudolf-shellfix: could not attach: %.300s\n", r); goto done; }
	/* Chromium 157 updates no installed copy in place, however it was installed ("App is already installed"): the old
	   one is removed first, and its storage goes with it */
	char inst[2048]; snprintf(inst, sizeof inst, "{\"manifestId\":\"isolated-app://%s/\",\"installUrlOrBundleUrl\":\"file://%s\"}", id, bundle);
	snprintf(p, sizeof p, "{\"manifestId\":\"isolated-app://%s/\"}", id);
	if (!call(4, session, "PWA.uninstall", p, r, sizeof r)) fprintf(stderr, "rudolf-shellfix: uninstall: %.300s\n", r);   /* (none: fine) */
	ok = call(5, session, "PWA.install", inst, r, sizeof r);
	if (!ok) fprintf(stderr, "rudolf-shellfix: install: %.300s\n", r);
done:
	send_msg("{\"id\":9,\"method\":\"Browser.close\",\"params\":{}}");
	for (int i = 0; i < 100; i++) { if (waitpid(kid, NULL, WNOHANG) == kid) break; usleep(100000); if (i == 99) { kill(kid, SIGKILL); waitpid(kid, NULL, 0); } }
	return ok ? 0 : 1;
}
