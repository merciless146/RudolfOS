/* (included by rudolfd.c, not built on its own: hence .inc) ---------- 1.6.1 (docs/plan-1.6.1.md Part 19): game mode,
 * through /usr/lib/rudolf/gamemode (root), which raises the processor's energy preference (the power settings) and lowers
 * the desktop's own processor time while a game is full screen, and puts both back when it is over:
 *   POST game/on?src=page|native&kind=page|linux|oracle[&pid=N]   a full screen the desktop sees, said again every 2 s
 *   POST game/off?src=page|native                                 it is over
 *   GET  game/status                                              on, why (the sources, the games that asked through
 *                                                                 GameMode's D-Bus API), the desktop slowed, the setting
 *   POST game/set?enabled=0|1                                     Settings › System health › Memory › Game mode
 * Only the desktop reaches these (the peer check); a game reaches game mode only through rudolf-portal's GameMode API. */
#ifndef GM_SH
#define GM_SH "/usr/lib/rudolf/gamemode"
#endif
static int gm_word(const char *s, const char *const *ok) { if (!s) return 0; for (; *ok; ok++) if (!strcmp(s, *ok)) return 1; return 0; }
static void api_game(struct req *r, const char *what, int post)
{
	if (access(GM_SH, X_OK)) { reply_err(r->fd, 404, "this Rudolf OS has no game mode"); return; }
	static const char *const SRC[] = { "page", "native", NULL }, *const KIND[] = { "page", "linux", "oracle", NULL };
	char cmd[200]; char *src = qparam(r->query, "src");
	if (!strcmp(what, "status")) snprintf(cmd, sizeof cmd, GM_SH " status 2>/dev/null");
	else if (!post) { reply_err(r->fd, 405, "use POST"); return; }
	else if (!strcmp(what, "on")) {
		char *kind = qparam(r->query, "kind"), *pid = qparam(r->query, "pid"); long p = pid && *pid && strspn(pid, "0123456789") == strlen(pid) && strlen(pid) < 9 ? atol(pid) : 0;
		if (!gm_word(src, SRC) || !gm_word(kind, KIND)) { reply_err(r->fd, 400, "src=page|native and kind=page|linux|oracle"); return; }
		if (p > 1) snprintf(cmd, sizeof cmd, GM_SH " on %s %s %ld 2>/dev/null", src, kind, p); else snprintf(cmd, sizeof cmd, GM_SH " on %s %s 2>/dev/null", src, kind);
	} else if (!strcmp(what, "off")) { if (!gm_word(src, SRC)) { reply_err(r->fd, 400, "src=page|native"); return; } snprintf(cmd, sizeof cmd, GM_SH " off %s 2>/dev/null", src); }
	else if (!strcmp(what, "set")) { char *e = qparam(r->query, "enabled"); if (!e || (strcmp(e, "0") && strcmp(e, "1"))) { reply_err(r->fd, 400, "enabled=0|1"); return; }
		snprintf(cmd, sizeof cmd, GM_SH " set enabled=%s 2>/dev/null", e); }
	else { reply_err(r->fd, 404, "unknown game mode action"); return; }
	char *o = run_out(cmd); if (!o || o[0] != '{') { free(o); reply_err(r->fd, 500, "gamemode failed"); return; }
	reply(r->fd, 200, "application/json", o, strlen(o), NULL); free(o);
}
