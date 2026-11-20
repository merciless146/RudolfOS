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
 *   - lets the shell capture the pointer (games) without a prompt,
 *   - opens the shell again whenever its window is closed.
 * It exits with Chromium. If the shell never brings the desktop up, the session's watchdog notices (no
 * heartbeat) and starts the desktop page without the shell.
 *
 *   rudolf-launch --oracle CONTROL SHELL-ID CHROME [ARGS...]      (1.5.9, docs/plan-1.5.9.md 12.3c)
 *
 * Oracle on the native desktop: one Chromium for every Oracle window, each window the shell with Oracle alone in it
 * (isolated-app://SHELL-ID/#app=browser&...: the shell passes its #... on to the desktop page, which opens Oracle in
 * one-app mode with the shell's real tabs). CONTROL is a FIFO rudolf-native writes to, one line per window:
 *   open W H X Y HASH       a window of W x H at X,Y (0 0 0 0: Chromium's choice) for isolated-app://SHELL-ID/#HASH
 * Chromium starts with no window; when the last of the shell's windows has closed, Chromium is closed and this exits.
 * (12.3c step 5) The desktop page in each window (http://127.0.0.1:8080/#app=...) gets rudolfNativeOracle(JSON), a
 * DevTools binding on this private pipe (no website tab has it): what the page tells the native desktop, as its
 * rudolfNative message handler does in WebKit, is written to standard output, one line each: "msg SESSION JSON" (SESSION:
 * the page's, for an answer). On CONTROL, "act SESSION ID" runs a notification's own action in that page (a toast's click).
 * The page's own full screen ({"op":"fs","on":1|0}, shell26.js's stand-in) is done here: its window in Chromium's full screen.
 * "mirror ACCOUNT" has every desktop page take the files an app's window saved from rudolfd's mirror (__oaMirrorIn); (1.6.0) "set
 * ACCOUNT" its settings (__oaSetIn); "close X Y W H"
 * asks the page of the window at that place to close, as its own ✕ (the frame's ✕ on the native desktop). A page closing its window
 * says so ({"op":"closing"}): the last one while a download is arriving is taken off the screen ("hide X Y W H" on standard output)
 * and its page closes once Chromium's downloads have finished and it has filed them (Chromium never asks "Download is in progress").
 * (1.6.0) Each desktop page's title (the window's title as the page has it: {"op":"title","t":...}, sent as it changes and asked for
 * once the binding is there) is written as "title X Y W H JSON-STRING", X Y W H its window's place: Rudolf's Chromium 158 left an
 * Oracle window's own title at the shell's name, which is all the native desktop saw.
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
#include <sys/stat.h>
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

/* ---- the memory each page uses, for Task Manager (Chromium does not say which process belongs to which tab).
   While rudolfd asks (it touches WANT when Task Manager looks), every 2 seconds: list the targets, attach to each
   page (the desktop and Oracle's tabs), ask for its JavaScript memory, and write {url, used} to OUT. */
#define WANT "/run/user/1000/rudolf-tabmem.want"
#define OUTF "/run/user/1000/rudolf-tabmem.json"
#define MAXT 64
static struct { char target[64], session[64], url[512]; long long used; int attach_id, heap_id, seen; } tm[MAXT];
static int tm_list_id;
static long long tm_last;
static int tm_slot(const char *target)
{
	int free_i = -1;
	for (int i = 0; i < MAXT; i++) { if (!strcmp(tm[i].target, target)) return i; if (free_i < 0 && !tm[i].target[0]) free_i = i; }
	if (free_i >= 0) { memset(&tm[free_i], 0, sizeof tm[free_i]); snprintf(tm[free_i].target, sizeof tm[free_i].target, "%s", target); tm[free_i].used = -1; }
	return free_i;
}
static void tm_write(void)
{
	FILE *f = fopen(OUTF ".tmp", "w"); if (!f) return;
	fprintf(f, "{\"time\":%lld,\"pages\":[", (long long)time(NULL));
	int first = 1;
	for (int i = 0; i < MAXT; i++) if (tm[i].target[0] && tm[i].used >= 0) { fprintf(f, "%s{\"url\":\"%s\",\"used\":%lld}", first ? "" : ",", tm[i].url, tm[i].used); first = 0; }
	fprintf(f, "]}\n"); fclose(f); rename(OUTF ".tmp", OUTF);
}
static void send_session(const char *session, const char *method, const char *params, int *id_out)
{
	char m[512];
	*id_out = next_id;
	int n = snprintf(m, sizeof m, "{\"id\":%d,\"sessionId\":\"%s\",\"method\":\"%s\",\"params\":%s}", next_id++, session, method, params ? params : "{}");
	if (n > 0 && n < (int)sizeof m && write(to_chrome, m, n + 1) < 0) {}
}
static void tm_tick(long long now)
{
	struct stat st;
	if (stat(WANT, &st) || time(NULL) - st.st_mtime > 15 || now - tm_last < 2000 || !shell_target[0]) return;
	tm_last = now;
	tm_write();
	for (int i = 0; i < MAXT; i++) tm[i].seen = 0;
	tm_list_id = next_id; send_cmd("Target.getTargets", NULL);
}
/* a reply to the memory probe: 1 when handled */
static int tm_handle(const char *msg, long id)
{
	if (id <= 0) return 0;
	if (id == tm_list_id) {
		for (const char *q = strstr(msg, "{\"targetId\""); q; q = strstr(q + 1, "{\"targetId\"")) {
			const char *e = strchr(q, '}'); if (!e) break;
			char one[1024], t[64], ty[32], url[512]; size_t n = (size_t)(e - q + 1) < sizeof one - 1 ? (size_t)(e - q + 1) : sizeof one - 1;
			memcpy(one, q, n); one[n] = 0;
			field(one, "type", ty, sizeof ty); field(one, "url", url, sizeof url); field(one, "targetId", t, sizeof t);
			if ((strcmp(ty, "webview") && strcmp(ty, "page")) || !strncmp(url, "isolated-app://", 15) || !t[0]) continue;
			int k = tm_slot(t); if (k < 0) continue;
			tm[k].seen = 1; snprintf(tm[k].url, sizeof tm[k].url, "%s", url);
			{ size_t ul = strlen(tm[k].url); while (ul && tm[k].url[ul - 1] == '\\') tm[k].url[--ul] = 0; }   /* a cut-off escape */
			if (!tm[k].session[0]) { char p[128]; snprintf(p, sizeof p, "{\"targetId\":\"%s\",\"flatten\":true}", t); tm[k].attach_id = next_id; send_cmd("Target.attachToTarget", p); }
			else send_session(tm[k].session, "Runtime.getHeapUsage", NULL, &tm[k].heap_id);
		}
		for (int i = 0; i < MAXT; i++) if (tm[i].target[0] && !tm[i].seen) tm[i].target[0] = 0;   /* gone */
		return 1;
	}
	for (int i = 0; i < MAXT; i++) {
		if (!tm[i].target[0]) continue;
		if (id == tm[i].attach_id) {
			tm[i].attach_id = 0;
			field(msg, "sessionId", tm[i].session, sizeof tm[i].session);
			if (tm[i].session[0]) send_session(tm[i].session, "Runtime.getHeapUsage", NULL, &tm[i].heap_id);
			return 1;
		}
		if (id == tm[i].heap_id) { tm[i].heap_id = 0; long long u = num_field(msg, "usedSize"); if (u >= 0) tm[i].used = u; return 1; }
	}
	return 0;
}
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
	if (tm_handle(msg, strstr(msg, "\"method\"") ? 0 : num_field(msg, "id"))) return;
	if (strstr(msg, "\"sessionId\"")) return;   /* events from the pages attached for the memory probe */
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

/* ---- Oracle's windows (--oracle) */
static int oracle, o_had, o_n; static char o_id[64];
static struct { char target[64]; } o_win[64];
static struct { char params[1400]; int id, tries; long long at; } o_q[32]; static int o_nq; static long long o_close_at;   /* (1.6.0 Part 21: when Chromium closes after "logout") */
/* the desktop pages attached for their messages (12.3c step 5) */
static struct { char target[64], parent[64], session[80]; int attach_id; char title[600]; } o_pg[64]; static int o_npg;
/* (1.6.0) the titles waiting for their window's place */
static struct { int id; char title[600]; } o_tq[16];
/* a JSON string's literal at key, quotes and escapes as they are ("" when none) */
static int raw_str(const char *msg, const char *key, char *out, size_t n)
{
	char k[64]; snprintf(k, sizeof k, "\"%s\":\"", key); const char *p = strstr(msg, k); if (!p) return -1; p += strlen(k) - 1;
	size_t o = 0; out[o++] = *p++;
	for (; *p && o + 3 < n; p++) { if (*p == '\\' && p[1]) { out[o++] = *p++; out[o++] = *p; continue; } out[o++] = *p; if (*p == '"') break; }
	if (out[o - 1] != '"' || o < 2) { out[0] = 0; return -1; }
	out[o] = 0; return 0;
}
static void o_title(int k)
{
	if (!o_pg[k].parent[0]) return;
	for (int i = 0; i < 16; i++) if (!o_tq[i].id) { char p[128]; snprintf(p, sizeof p, "{\"targetId\":\"%.63s\"}", o_pg[k].parent);
		o_tq[i].id = next_id; snprintf(o_tq[i].title, sizeof o_tq[i].title, "%s", o_pg[k].title); send_cmd("Browser.getWindowForTarget", p); return; }
}
/* (12.3c) downloads in progress: Chromium is not closed under them when Oracle's last window closes (on Standard Chromium never
   closes): that window goes from the screen, and its page closes once they have finished and it has filed them */
static struct { char guid[64]; } o_dl[32]; static int o_ndl, o_hide_id; static char o_defer[80];
/* (a window's frame ✕ on the native desktop: its page asked to close, found by the window's place: "close X Y W H") */
static struct { int x, y, w, h; } o_cl; static struct { char target[64]; int id; } o_clq[64]; static int o_ncl;
static void o_send(int i) { o_q[i].id = next_id; o_q[i].tries++; send_cmd("Target.createTarget", o_q[i].params); }
static void o_line(char *l)
{
	/* (a toast's click: the notification's own action in the page that sent it) */
	char sid[80], nid[48];
	if (sscanf(l, "act %79s %47s", sid, nid) == 2) {
		if (strspn(sid, "0123456789ABCDEFabcdef") != strlen(sid) || strspn(nid, "0123456789.") != strlen(nid)) return;
		char p[200]; snprintf(p, sizeof p, "{\"expression\":\"window.__oaToastAct&&__oaToastAct('%s')\",\"userGesture\":true}", nid);
		int k; send_session(sid, "Runtime.evaluate", p, &k); return;
	}
	/* (the files an app's window saved, in rudolfd's mirror: every desktop page here takes them, 12.3c) */
	char mid[64];
	if (sscanf(l, "mirror %63s", mid) == 1) {
		if (strspn(mid, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != strlen(mid)) return;
		char p[200]; snprintf(p, sizeof p, "{\"expression\":\"window.__oaMirrorIn&&__oaMirrorIn('%s')\"}", mid);
		for (int i = 0; i < o_npg; i++) if (o_pg[i].session[0]) { int k; send_session(o_pg[i].session, "Runtime.evaluate", p, &k); }
		return;
	}
	/* (1.6.0 Part 21) "webclear": an account deleted on the native desktop: every desktop page clears the websites listed in rudolfd's
	   copy (__oaWebClearIn, tabs26.js); "logout": the account signed out: each page saves its tabs and clears Guest's websites
	   (__oaLogout), then Chromium closes, as every window does when signing out on Standard */
	if (!strcmp(l, "webclear") || !strcmp(l, "logout")) {
		const char *p = !strcmp(l, "logout") ? "{\"expression\":\"window.__oaLogout&&__oaLogout()\"}" : "{\"expression\":\"window.__oaWebClearIn&&__oaWebClearIn()\"}";
		for (int i = 0; i < o_npg; i++) if (o_pg[i].session[0]) { int k; send_session(o_pg[i].session, "Runtime.evaluate", p, &k); }
		if (!strcmp(l, "logout")) o_close_at = now_ms() + 2500;
		return;
	}
	/* (1.6.0, the owner's report 3: an app window saved its settings; every desktop page here takes them from rudolfd's copy) */
	if (sscanf(l, "set %63s", mid) == 1) {
		if (strspn(mid, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != strlen(mid)) return;
		char p[200]; snprintf(p, sizeof p, "{\"expression\":\"window.__oaSetIn&&__oaSetIn('%s')\"}", mid);
		for (int i = 0; i < o_npg; i++) if (o_pg[i].session[0]) { int k; send_session(o_pg[i].session, "Runtime.evaluate", p, &k); }
		return;
	}
	int cx, cy, cw, ch;
	if (sscanf(l, "close %d %d %d %d", &cx, &cy, &cw, &ch) == 4) {
		o_cl.x = cx; o_cl.y = cy; o_cl.w = cw; o_cl.h = ch; o_ncl = 0;
		for (int i = 0; i < o_n && o_ncl < 64; i++) { char p[128]; snprintf(p, sizeof p, "{\"targetId\":\"%.63s\"}", o_win[i].target);
			memcpy(o_clq[o_ncl].target, o_win[i].target, sizeof o_clq[0].target); o_clq[o_ncl++].id = next_id; send_cmd("Browser.getWindowForTarget", p); }
		return;
	}
	unsigned w, h; int x, y; char hash[1100] = "";
	if (sscanf(l, "open %u %u %d %d %1099s", &w, &h, &x, &y, hash) < 4) return;
	/* (the hash as rudolf-native escapes it: letters, digits and the URL's own punctuation only) */
	for (const char *c = hash; *c; c++) if (!strchr("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789%&=._-~+", *c)) return;
	if (o_nq >= 32) return;
	char g[160] = ""; if (w >= 200 && h >= 150 && w < 20000 && h < 20000) snprintf(g, sizeof g, ",\"left\":%d,\"top\":%d,\"width\":%u,\"height\":%u", x, y, w, h);
	o_defer[0] = 0;   /* (a new window: the one going may close as any) */
	snprintf(o_q[o_nq].params, sizeof o_q[0].params, "{\"url\":\"isolated-app://%s/%s%s\",\"newWindow\":true%s}", o_id, hash[0] ? "#" : "", hash, g);
	o_q[o_nq].tries = 0; o_q[o_nq].at = 0; o_q[o_nq].id = 0;
	if (ready) o_send(o_nq);
	o_nq++;
}
/* (12.3c step 5) the desktop page's own full screen (shell26.js's stand-in fills the page, which is the window): its window, the
   shell's (the page's parent target), in Chromium's full screen, so the window manager gives it the screen as on Standard */
static struct { int id, on; } o_fs[8];
static void o_fullscreen(const char *sid, int on)
{
	for (int i = 0; i < o_npg; i++) if (!strcmp(o_pg[i].session, sid) && o_pg[i].parent[0]) {
		for (int k = 0; k < 8; k++) if (!o_fs[k].id) { char p[128]; snprintf(p, sizeof p, "{\"targetId\":\"%s\"}", o_pg[i].parent); o_fs[k].id = next_id; o_fs[k].on = on; send_cmd("Browser.getWindowForTarget", p); return; }
		return;
	}
}
/* a JSON string's value at key (its escapes undone): the binding's payload */
static int json_str(const char *msg, const char *key, char *out, size_t n)
{
	char k[64]; snprintf(k, sizeof k, "\"%s\":\"", key); const char *p = strstr(msg, k); if (!p) return -1; p += strlen(k);
	size_t o = 0;
	for (; *p && *p != '"' && o + 4 < n; p++) {
		if (*p != '\\') { out[o++] = *p; continue; }
		p++; if (!*p) break;
		switch (*p) { case 'n': out[o++] = ' '; break; case 't': out[o++] = ' '; break; case 'r': break; case 'b': case 'f': break;
		case 'u': { unsigned c = 0; for (int i = 1; i <= 4; i++) { char h = p[i]; c = c * 16 + (h >= '0' && h <= '9' ? h - '0' : h >= 'a' && h <= 'f' ? h - 'a' + 10 : h >= 'A' && h <= 'F' ? h - 'A' + 10 : 0); if (!h) return -1; } p += 4;
			/* (kept as the escape it was: the payload is JSON itself, read again by rudolf-native) */
			o += snprintf(out + o, n - o, "\\u%04x", c); break; }
		default: out[o++] = *p; }
	}
	out[o] = 0; return 0;
}
static int o_win_slot(const char *t, int add)
{
	for (int i = 0; i < o_n; i++) if (!strcmp(o_win[i].target, t)) return i;
	if (!add || o_n >= 64) return -1;
	snprintf(o_win[o_n].target, sizeof o_win[0].target, "%s", t); return o_n++;
}
static int o_handle(const char *msg)
{
	char t[64], ty[32], url[512];
	if (debug) fprintf(stderr, "launch << %.300s\n", msg);
	long id = strstr(msg, "\"method\"") ? 0 : num_field(msg, "id");
	/* (12.3c step 5) a desktop page attached: its binding; what it calls goes to standard output */
	for (int i = 0; id > 0 && i < o_npg; i++) if (o_pg[i].attach_id == id) {
		char sid[80]; field(msg, "sessionId", sid, sizeof sid); o_pg[i].attach_id = 0; snprintf(o_pg[i].session, sizeof o_pg[i].session, "%s", sid);
		if (sid[0]) { int k; send_session(sid, "Runtime.addBinding", "{\"name\":\"rudolfNativeOracle\"}", &k);
			/* (1.6.0: its title, said before the binding was there) */
			send_session(sid, "Runtime.evaluate", "{\"expression\":\"window.__oaTitleAgain&&__oaTitleAgain()\"}", &k); }
		return 1;
	}
	for (int k = 0; id > 0 && k < 8; k++) if (o_fs[k].id == id) {
		long wid = num_field(msg, "windowId"); o_fs[k].id = 0;
		if (wid > 0) { char p[160]; snprintf(p, sizeof p, "{\"windowId\":%ld,\"bounds\":{\"windowState\":\"%s\"}}", wid, o_fs[k].on ? "fullscreen" : "normal"); send_cmd("Browser.setWindowBounds", p); }
		return 1;
	}
	for (int i = 0; id > 0 && i < 16; i++) if (o_tq[i].id == id) {   /* (1.6.0: a page's title, with its window's place) */
		o_tq[i].id = 0; const char *b = strstr(msg, "\"bounds\"");
		if (b) { printf("title %ld %ld %ld %ld %s\n", num_field(b, "left"), num_field(b, "top"), num_field(b, "width"), num_field(b, "height"), o_tq[i].title); fflush(stdout); }
		return 1;
	}
	if (id > 0 && o_hide_id && id == o_hide_id) {   /* (the window that goes from the screen while Chromium finishes its downloads) */
		o_hide_id = 0; const char *b = strstr(msg, "\"bounds\"");
		if (b) { printf("hide %ld %ld %ld %ld\n", num_field(b, "left"), num_field(b, "top"), num_field(b, "width"), num_field(b, "height")); fflush(stdout); }
		return 1;
	}
	for (int i = 0; id > 0 && i < o_ncl; i++) if (o_clq[i].id == id) {   /* (the window asked to close: is it this one?) */
		o_clq[i].id = 0; const char *b = strstr(msg, "\"bounds\"");
		if (b && labs(num_field(b, "left") - o_cl.x) <= 2 && labs(num_field(b, "top") - o_cl.y) <= 2 && labs(num_field(b, "width") - o_cl.w) <= 2 && labs(num_field(b, "height") - o_cl.h) <= 2) {
			int done = 0;
			for (int k = 0; k < o_npg; k++) if (!strcmp(o_pg[k].parent, o_clq[i].target) && o_pg[k].session[0]) { int q;
				send_session(o_pg[k].session, "Runtime.evaluate", "{\"expression\":\"window.__oneAppClose?(window.__oneAppClose(),1):window.close()\",\"userGesture\":true}", &q); done = 1; }
			if (!done) { char p[128]; snprintf(p, sizeof p, "{\"targetId\":\"%.63s\"}", o_clq[i].target); send_cmd("Target.closeTarget", p); }
			o_ncl = 0; }
		return 1;
	}
	if (strstr(msg, "\"method\":\"Runtime.bindingCalled\"") && strstr(msg, "\"name\":\"rudolfNativeOracle\"") && strstr(msg, "\\\"op\\\":\\\"closing\\\"")) {
		/* (a page closing its window: the last one while a download is arriving goes from the screen instead, and its page stays until
		   the downloads are in, as on Standard, where Chromium never closes; otherwise it closes now) */
		char sid[80]; field(msg, "sessionId", sid, sizeof sid); int k = -1; for (int i = 0; i < o_npg; i++) if (!strcmp(o_pg[i].session, sid)) k = i;
		if (k >= 0 && o_ndl > 0 && o_n <= 1 && o_pg[k].parent[0]) {
			fprintf(stderr, "rudolf-launch: Oracle's last window closed with %d download(s) in progress: Chromium waits for them\n", o_ndl);
			snprintf(o_defer, sizeof o_defer, "%s", sid); int q; send_session(sid, "Runtime.evaluate", "{\"expression\":\"window.__oaCloseLater&&__oaCloseLater()\"}", &q);
			char p[128]; snprintf(p, sizeof p, "{\"targetId\":\"%.63s\"}", o_pg[k].parent); o_hide_id = next_id; send_cmd("Browser.getWindowForTarget", p);
		} else if (sid[0]) { int q; send_session(sid, "Runtime.evaluate", "{\"expression\":\"window.__oaCloseNow&&__oaCloseNow()\"}", &q); }
		return 1;
	}
	if (strstr(msg, "\"method\":\"Runtime.bindingCalled\"") && strstr(msg, "\"name\":\"rudolfNativeOracle\"")) {
		static char pl[16384]; char sid[80]; field(msg, "sessionId", sid, sizeof sid);
		if (sid[0] && !json_str(msg, "payload", pl, sizeof pl) && pl[0] == '{') {
			if (strstr(pl, "\"op\":\"fs\"")) o_fullscreen(sid, strstr(pl, "\"on\":1") != NULL);   /* (the page's own full screen: here) */
			else if (strstr(pl, "\"op\":\"title\"")) {   /* (1.6.0: its title, with its window's place) */
				char ti[600]; for (int k = 0; k < o_npg; k++) if (!strcmp(o_pg[k].session, sid)) { if (!raw_str(pl, "t", ti, sizeof ti)) { snprintf(o_pg[k].title, sizeof o_pg[k].title, "%s", ti); o_title(k); } break; } }
			else { printf("msg %s %s\n", sid, pl); fflush(stdout); } }
		return 1;
	}
	if (strstr(msg, "\"sessionId\"") && !strstr(msg, "\"method\":\"Target.")) return 1;   /* (the attached pages' own events) */
	/* (downloads: begun, then finished or cancelled) */
	if (strstr(msg, "\"method\":\"Browser.downloadWillBegin\"")) { char g[64]; field(msg, "guid", g, sizeof g); if (g[0] && o_ndl < 32) snprintf(o_dl[o_ndl++].guid, sizeof o_dl[0].guid, "%s", g); return 1; }
	if (strstr(msg, "\"method\":\"Browser.downloadProgress\"")) { char g[64], stt[24]; field(msg, "guid", g, sizeof g); field(msg, "state", stt, sizeof stt);
		if (strcmp(stt, "inProgress")) for (int i = 0; i < o_ndl; i++) if (!strcmp(o_dl[i].guid, g)) { o_dl[i] = o_dl[--o_ndl]; break; }
		if (!o_ndl && o_defer[0]) { int q; fprintf(stderr, "rudolf-launch: the downloads have finished: Oracle's page files them and closes\n");
			send_session(o_defer, "Runtime.evaluate", "{\"expression\":\"window.__oaCloseWhenDone&&__oaCloseWhenDone()\"}", &q); o_defer[0] = 0; }
		return 1; }
	for (int i = 0; id > 0 && i < o_nq; i++) if (o_q[i].id == id) {
		if (strstr(msg, "\"error\"") && o_q[i].tries < 60) { o_q[i].at = now_ms() + 1000; return 1; }
		memmove(&o_q[i], &o_q[i + 1], (o_nq - i - 1) * sizeof o_q[0]); o_nq--; return 1;
	}
	if (strstr(msg, "\"method\":\"Target.targetCreated\"") || strstr(msg, "\"method\":\"Target.targetInfoChanged\"")) {
		field(msg, "type", ty, sizeof ty); field(msg, "url", url, sizeof url); field(msg, "targetId", t, sizeof t);
		if (!t[0]) return 1;
		if (!strncmp(url, "isolated-app://", 15) && (!strcmp(ty, "page") || !strcmp(ty, "app"))) { if (o_win_slot(t, 1) >= 0) o_had = 1; }
		else if (!strcmp(ty, "webview") && !strncmp(url, "http://127.0.0.1:8080/#app=", 27)) {   /* (a window's desktop page: attached once) */
			int k = -1; for (int i = 0; i < o_npg; i++) if (!strcmp(o_pg[i].target, t)) k = i;
			if (k < 0 && o_npg < 64) { k = o_npg++; snprintf(o_pg[k].target, sizeof o_pg[0].target, "%s", t); o_pg[k].session[0] = 0; o_pg[k].title[0] = 0; field(msg, "parentId", o_pg[k].parent, sizeof o_pg[k].parent);
				char p[128]; snprintf(p, sizeof p, "{\"targetId\":\"%s\",\"flatten\":true}", t); o_pg[k].attach_id = next_id; send_cmd("Target.attachToTarget", p); }
			}
		else if (!strcmp(ty, "page") && strncmp(url, "devtools://", 11)) { char p[128]; snprintf(p, sizeof p, "{\"targetId\":\"%s\"}", t); send_cmd("Target.closeTarget", p); }
		return 1;
	}
	if (strstr(msg, "\"method\":\"Target.targetDestroyed\"")) {
		field(msg, "targetId", t, sizeof t); int k = o_win_slot(t, 0);
		for (int i = 0; i < o_npg; i++) if (!strcmp(o_pg[i].target, t)) { o_pg[i] = o_pg[--o_npg]; break; }
		if (k >= 0) { o_win[k] = o_win[--o_n];
			/* the last of Oracle's windows: Chromium goes with it (the native desktop's memory is the point) */
			if (!o_n && o_had && !o_nq) send_cmd("Browser.close", NULL); }
		return 1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	const char *ctl = NULL;
	if (argc > 1 && !strcmp(argv[1], "--oracle")) { if (argc < 5) { fprintf(stderr, "usage: rudolf-launch --oracle CONTROL SHELL-ID CHROME [ARGS...]\n"); return 2; }
		oracle = 1; ctl = argv[2]; argv += 2; argc -= 2; }
	if (argc < 3) { fprintf(stderr, "usage: rudolf-launch SHELL-ID CHROME [ARGS...]\n"); return 2; }
	const char *id = argv[1];
	if (strlen(id) != 56 || strspn(id, "abcdefghijklmnopqrstuvwxyz234567") != 56) { fprintf(stderr, "rudolf-launch: bad shell ID\n"); return 2; }
	snprintf(shell_url, sizeof shell_url, "isolated-app://%s/", id); snprintf(o_id, sizeof o_id, "%s", id);
	int cfd = -1, cfw = -1; (void)cfw;
	if (oracle) { cfd = open(ctl, O_RDONLY | O_NONBLOCK | O_CLOEXEC); cfw = open(ctl, O_WRONLY | O_NONBLOCK | O_CLOEXEC);   /* (kept open: no end of file between writers) */
		struct stat st; if (cfd < 0 || fstat(cfd, &st) || !S_ISFIFO(st.st_mode)) { fprintf(stderr, "rudolf-launch: %s is not a FIFO\n", ctl); return 2; } }
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
		char **args = calloc(argc + 5, sizeof *args); int n = 0;
		for (int i = 2; i < argc; i++) args[n++] = argv[i];
		args[n++] = "--remote-debugging-pipe";
		/* 1.4.2: the pipe makes Chromium report itself as controlled by automation (navigator.webdriver true), and
		   Google's sign-in refuses such a browser ("This browser or app may not be secure"). Oracle's tabs are
		   the person's own browsing: the pipe only opens the shell, so pages are not told. */
		args[n++] = "--disable-blink-features=AutomationControlled";
		if (oracle) args[n++] = "--no-startup-window"; else args[n++] = "about:blank";
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
	/* the shell captures the pointer for games without Chromium asking the person first */
	{
		char p[256];
		snprintf(p, sizeof p, "{\"permission\":{\"name\":\"pointer-lock\"},\"setting\":\"granted\",\"origin\":\"isolated-app://%s\"}", id);
		send_cmd("Browser.setPermission", p);
	}
	if (oracle) { ready = 1;   /* (no starting page to wait for: Chromium answers the first window when it can, retried) */
		send_cmd("Browser.setDownloadBehavior", "{\"behavior\":\"default\",\"eventsEnabled\":true}"); }   /* (where the policy says; told of each one) */
	static char cbuf[4096]; size_t cl = 0;
	for (;;) {
		struct pollfd pfs[2] = {{from_chrome, POLLIN, 0}, {cfd, POLLIN, 0}};
		int r = poll(pfs, oracle ? 2 : 1, 1000); struct pollfd pf = pfs[0];
		if (r < 0 && errno == EINTR) continue;
		if (oracle && r > 0 && (pfs[1].revents & POLLIN)) {
			ssize_t g = read(cfd, cbuf + cl, sizeof cbuf - 1 - cl);
			if (g > 0) { cl += g; cbuf[cl] = 0; char *e; while ((e = memchr(cbuf, '\n', cl))) { *e = 0; o_line(cbuf); cl -= (size_t)(e + 1 - cbuf); memmove(cbuf, e + 1, cl); } if (cl >= sizeof cbuf - 1) cl = 0; }
		}
		if (r > 0 && (pf.revents & (POLLIN | POLLHUP))) {
			ssize_t got = read(from_chrome, buf + blen, sizeof buf - blen - 1);
			if (got <= 0) break;                     /* Chromium has gone */
			blen += got;
			size_t start = 0;
			for (size_t i = 0; i < blen; i++) if (buf[i] == 0) { if (!oracle || !o_handle(buf + start)) { if (!oracle) handle(buf + start); } start = i + 1; }
			memmove(buf, buf + start, blen - start); blen -= start;
			if (blen >= sizeof buf - 1) blen = 0;        /* a message too large to matter (none expected) */
		}
		if (oracle && o_close_at && now_ms() >= o_close_at) { o_close_at = 0; o_nq = 0; send_cmd("Browser.close", NULL); }   /* (signed out: "logout") */
		if (oracle) { for (int i = 0; i < o_nq; i++) if (!o_q[i].id || (o_q[i].at && now_ms() >= o_q[i].at)) { o_q[i].at = 0; o_send(i); } }
		else {
		if (retry_at && now_ms() >= retry_at) { retry_at = 0; open_shell(); }
		tm_tick(now_ms());
		if (!ready && now_ms() - t0 > 20000 && !open_id) { ready = 1; open_shell(); }   /* no page event: try anyway */
		}
		int st;
		if (waitpid(pid, &st, WNOHANG) == pid) return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
	}
	int st = 0; waitpid(pid, &st, 0);
	return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
}
