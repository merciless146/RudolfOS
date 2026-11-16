/* teststand: a stand-in for a Linux app that signs in through a web page (as Claude's desktop app does with "Continue
 * with Google", Discord, Steam, VS Code), for Rudolf OS's tests (iso/qt/sb/l160_t.py). No Electron, no toolkit: static,
 * so it runs in Debian or in a stand-in for it.
 *   teststand [start]           serves the sign-in page on 127.0.0.1:PORT (TESTSTAND_PORT, default 8600) for 10
 *                               minutes, then opens it the way Electron's shell.openExternal does: it runs xdg-open with
 *                               the page's address and writes "xdg-open exit N" to $HOME/teststand.log. The page sends
 *                               the browser on (its script) to /done, which answers with a redirect (HTTP 302) to
 *                               teststand://done?code=CODE (CODE: $HOME/teststand.code, else X160).
 *   teststand teststand://...   what its launcher (MimeType x-scheme-handler/teststand) is started with after the
 *                               sign-in: the link is added to $HOME/teststand.out
 * GPL-2.0-or-later. */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static char *home(const char *f) { static char p[600]; const char *h = getenv("HOME"); snprintf(p, sizeof p, "%s/%s", h && *h ? h : "/tmp", f); return p; }
static void say(const char *f, const char *m) { FILE *o = fopen(home(f), "a"); if (o) { fprintf(o, "%s\n", m); fclose(o); } }
static void reply(int c, const char *status, const char *extra, const char *body)
{
	char h[1200]; int n = snprintf(h, sizeof h, "HTTP/1.1 %s\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %zu\r\nCache-Control: no-store\r\nConnection: close\r\n%s\r\n", status, strlen(body), extra);
	if (write(c, h, n) < 0 || write(c, body, strlen(body)) < 0) {}
}
static void serve(int port)
{
	int s = socket(AF_INET, SOCK_STREAM, 0), one = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
	struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons(port)}; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bind(s, (struct sockaddr *)&a, sizeof a) || listen(s, 16)) { say("teststand.log", "the page could not be served"); _exit(1); }
	char code[200] = "X160"; FILE *f = fopen(home("teststand.code"), "r");
	if (f) { if (fgets(code, sizeof code, f)) code[strcspn(code, "\r\n")] = 0; fclose(f); }
	alarm(600);
	for (;;) {
		int c = accept(s, NULL, NULL); if (c < 0) continue;
		char rq[2048] = ""; ssize_t n = read(c, rq, sizeof rq - 1); if (n > 0) rq[n] = 0;
		char loc[400];
		if (!strncmp(rq, "GET /signin", 11)) {
			reply(c, "200 OK", "", "<!doctype html><title>Test sign-in</title><body style='font:24px sans-serif;background:#dcfce7'>Signed in as test. Going back to the app...<script>setTimeout(()=>{location.href='/done'},1500)</script>");
			say("teststand.log", "page asked");
		} else if (!strncmp(rq, "GET /done", 9)) {
			snprintf(loc, sizeof loc, "Location: teststand://done?code=%s&state=t160\r\n", code);
			reply(c, "302 Found", loc, ""); say("teststand.log", "redirect sent");
		} else reply(c, "404 Not Found", "", "no");
		close(c);
	}
}
int main(int argc, char **argv)
{
	if (argc > 1 && !strncmp(argv[1], "teststand:", 10)) { say("teststand.out", argv[1]); return 0; }
	const char *pe = getenv("TESTSTAND_PORT"); int port = pe ? atoi(pe) : 8600;
	signal(SIGCHLD, SIG_IGN);
	pid_t p = fork();
	if (!p) { setsid(); serve(port); _exit(0); }
	struct timespec ts = {0, 500000000}; nanosleep(&ts, NULL);
	char url[100]; snprintf(url, sizeof url, "http://127.0.0.1:%d/signin", port);
	signal(SIGCHLD, SIG_DFL);
	pid_t x = fork();
	if (!x) { execlp("xdg-open", "xdg-open", url, (char *)NULL); _exit(127); }
	int st = 0; waitpid(x, &st, 0);
	char m[100]; snprintf(m, sizeof m, "xdg-open exit %d", WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st)); say("teststand.log", m);
	return 0;
}
