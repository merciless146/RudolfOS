/*
 * rudolf-launch - start Chromium with the Rudolf OS shell (an Isolated Web App) as the desktop.
 *
 *   rudolf-launch SHELL-ID CHROME [ARGS...]
 *
 * Chromium only opens an Isolated Web App when the browser itself asks for it: not from the command line, a
 * start-up page, a web page or an extension. rudolf-launch starts Chromium with --remote-debugging-pipe (a
 * private pipe on file descriptors 3 and 4 that only this process holds; no network port), then over that pipe:
 *   - opens isolated-app://SHELL-ID/ as a window and makes it fullscreen,
 *   - closes the starting tab,
 *   - opens the shell again whenever its window is closed (Ctrl+W and the like).
 * It exits with Chromium. If the shell never brings the desktop up, the session's watchdog notices (no
 * heartbeat) and starts the desktop page without the shell.
 *
 * Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
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

static int to_chrome = -1, from_chrome = -1, next_id = 1;
static char shell_url[128], shell_target[64];
static char buf[1 << 20]; static size_t blen;
static int debug;   /* RUDOLF_LAUNCH_DEBUG=1: log the conversation with Chromium on stderr */

static void send_cmd(const char *method, const char *params)
{
	char m[1024];
	int n = snprintf(m, sizeof m, "{\"id\":%d,\"method\":\"%s\",\"params\":%s}", next_id++, method, params ? params : "{}");
	if (n <= 0 || n >= (int)sizeof m) return;
	if (debug) fprintf(stderr, "launch >> %s\n", m);
	if (write(to_chrome, m, n + 1) < 0) {}   /* messages end with a NUL byte */
}
/* the string value of "key":"..." in msg (first one), or "" */
static void field(const char *msg, const char *key, char *out, size_t n)
{
	char k[64]; snprintf(k, sizeof k, "\"%s\":\"", key);
	const char *p = strstr(msg, k); out[0] = 0;
	if (!p) return;
	p += strlen(k);
	size_t i = 0; while (*p && *p != '"' && i + 1 < n) out[i++] = *p++;
	out[i] = 0;
}
static long num_field(const char *msg, const char *key)
{
	char k[64]; snprintf(k, sizeof k, "\"%s\":", key);
	const char *p = strstr(msg, k);
	return p ? strtol(p + strlen(k), NULL, 10) : -1;
}
static int open_id, tries, ready;
static long long retry_at;
static long long now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000LL + t.tv_nsec / 1000000; }
static void open_shell(void)
{
	char p[256]; snprintf(p, sizeof p, "{\"url\":\"%s\"}", shell_url);
	open_id = next_id; send_cmd("Target.createTarget", p);
	shell_target[0] = 0; tries++;
}

static void handle(const char *msg)
{
	char v[256], t[64];
	if (debug) fprintf(stderr, "launch << %.300s\n", msg);
	/* Chromium refused (not ready yet): try again in a second, for up to a minute */
	if (open_id && num_field(msg, "id") == open_id && strstr(msg, "\"error\"")) {
		if (tries < 60) retry_at = now_ms() + 1000; else fprintf(stderr, "rudolf-launch: the shell could not be opened\n");
		return;
	}
	/* the shell's window: make it fullscreen and close the starting tab */
	if (strstr(msg, "\"targetId\"") && strstr(msg, "\"result\"") && !strstr(msg, "\"targetInfo\"") && !strstr(msg, "\"targetInfos\"")) {
		field(msg, "targetId", t, sizeof t);
		if (t[0] && !shell_target[0]) {
			snprintf(shell_target, sizeof shell_target, "%s", t);
			char p[128]; snprintf(p, sizeof p, "{\"targetId\":\"%s\"}", shell_target);
			send_cmd("Browser.getWindowForTarget", p);
		}
		return;
	}
	/* the list of targets (asked for once the shell exists): close the ordinary tabs */
	if (strstr(msg, "\"targetInfos\"")) {
		for (const char *q = strstr(msg, "{\"targetId\""); q; q = strstr(q + 1, "{\"targetId\"")) {
			const char *e = strchr(q, '}'); if (!e) break;
			char one[1024]; size_t n = (size_t)(e - q + 1) < sizeof one - 1 ? (size_t)(e - q + 1) : sizeof one - 1;
			memcpy(one, q, n); one[n] = 0;
			char ty[32], url[512]; field(one, "type", ty, sizeof ty); field(one, "url", url, sizeof url); field(one, "targetId", t, sizeof t);
			if (!strcmp(ty, "page") && strncmp(url, "isolated-app://", 15) && strncmp(url, "devtools://", 11) && t[0]) {
				char p[128]; snprintf(p, sizeof p, "{\"targetId\":\"%s\"}", t); send_cmd("Target.closeTarget", p);
			}
		}
		return;
	}
	long wid = num_field(msg, "windowId");
	if (wid > 0 && strstr(msg, "\"result\"") && strstr(msg, "\"bounds\"")) {
		char p[160]; snprintf(p, sizeof p, "{\"windowId\":%ld,\"bounds\":{\"windowState\":\"fullscreen\"}}", wid);
		send_cmd("Browser.setWindowBounds", p);
		send_cmd("Target.getTargets", NULL);
		return;
	}
	/* targets appearing: close any ordinary tab (the starting about:blank, a new-tab page) */
	if (strstr(msg, "\"method\":\"Target.targetCreated\"") || strstr(msg, "\"method\":\"Target.targetInfoChanged\"")) {
		field(msg, "type", v, sizeof v);
		char url[512]; field(msg, "url", url, sizeof url);
		field(msg, "targetId", t, sizeof t);
		if (!ready && !strcmp(v, "page") && strstr(msg, "Target.targetInfoChanged")) { ready = 1; open_shell(); return; }
		if (!strcmp(v, "page") && t[0] && strncmp(url, "isolated-app://", 15) && strncmp(url, "devtools://", 11) && shell_target[0]) {
			char p[128]; snprintf(p, sizeof p, "{\"targetId\":\"%s\"}", t);
			send_cmd("Target.closeTarget", p);
		}
		return;
	}
	/* the shell's window was closed: open it again */
	if (strstr(msg, "\"method\":\"Target.targetDestroyed\"")) {
		field(msg, "targetId", t, sizeof t);
		if (shell_target[0] && !strcmp(t, shell_target)) { fprintf(stderr, "rudolf-launch: the shell was closed, opening it again\n"); open_shell(); }
	}
}

int main(int argc, char **argv)
{
	if (argc < 3) { fprintf(stderr, "usage: rudolf-launch SHELL-ID CHROME [ARGS...]\n"); return 2; }
	const char *id = argv[1];
	if (strlen(id) != 56 || strspn(id, "abcdefghijklmnopqrstuvwxyz234567") != 56) { fprintf(stderr, "rudolf-launch: bad shell ID\n"); return 2; }
	snprintf(shell_url, sizeof shell_url, "isolated-app://%s/", id);
	debug = getenv("RUDOLF_LAUNCH_DEBUG") != NULL;
	int a[2], b[2];
	if (pipe(a) || pipe(b)) { perror("pipe"); return 1; }
	pid_t pid = fork();
	if (pid < 0) { perror("fork"); return 1; }
	if (pid == 0) {
		/* Chromium reads commands on fd 3 and writes replies and events on fd 4 */
		/* move both ends out of the way first: the pipes may already be numbered 3 and 4 */
		int r = fcntl(a[0], F_DUPFD, 10), w = fcntl(b[1], F_DUPFD, 10);
		if (r < 0 || w < 0) _exit(127);
		close(a[0]); close(a[1]); close(b[0]); close(b[1]);
		if (dup2(r, 3) < 0 || dup2(w, 4) < 0) _exit(127);
		close(r); close(w);
		char **args = calloc(argc + 4, sizeof *args); int n = 0;
		for (int i = 2; i < argc; i++) args[n++] = argv[i];
		args[n++] = "--remote-debugging-pipe";
		args[n++] = "about:blank";
		args[n] = NULL;
		execv(args[0], args);
		perror("rudolf-launch: exec"); _exit(127);
	}
	close(a[0]); close(b[1]);
	to_chrome = a[1]; from_chrome = b[0];
	signal(SIGPIPE, SIG_IGN);
	/* forward TERM and INT to Chromium so the session can stop it as before */
	struct sigaction sa = {0}; sa.sa_handler = SIG_IGN;
	sigaction(SIGHUP, &sa, NULL);
	long long t0 = now_ms();
	send_cmd("Target.setDiscoverTargets", "{\"discover\":true}");   /* the shell is opened once the first page has loaded */
	for (;;) {
		struct pollfd pf = {from_chrome, POLLIN, 0};
		int r = poll(&pf, 1, 1000);
		if (r < 0 && errno == EINTR) continue;
		if (r > 0) {
			ssize_t got = read(from_chrome, buf + blen, sizeof buf - blen - 1);
			if (got <= 0) break;                     /* Chromium has gone */
			blen += got;
			size_t start = 0;
			for (size_t i = 0; i < blen; i++) if (buf[i] == 0) { handle(buf + start); start = i + 1; }
			memmove(buf, buf + start, blen - start); blen -= start;
			if (blen >= sizeof buf - 1) blen = 0;        /* a message too large to matter (none expected) */
		}
		if (retry_at && now_ms() >= retry_at) { retry_at = 0; open_shell(); }
		if (!ready && now_ms() - t0 > 20000 && !open_id) { ready = 1; open_shell(); }   /* no page event: try anyway */
		int st;
		if (waitpid(pid, &st, WNOHANG) == pid) return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
	}
	int st = 0; waitpid(pid, &st, 0);
	return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
}
