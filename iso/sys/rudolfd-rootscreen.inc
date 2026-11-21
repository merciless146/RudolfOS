/* (included by rudolfd-root.inc, not built on its own: hence .inc) ---------- 1.6.1 (docs/plan-1.6.1.md Part 11, display
 * and sound): the display's settings through the screen helper (rudolf-screen.c: the graphics path, the shell,
 * screenctl, the keyboard's layout, rudolf-display's and the browser's restarts) and sound through the sound helper
 * (rudolf-sound.c: hotkey and the sound script as the desktop's user, /etc/asound.conf). Each call site keeps its old
 * command for when the helper does not start and rudolfd is root (root_net_out of rudolfd-rootnet.inc: the same way
 * of asking; av[0] names the helper). The API is unchanged. */
/* the helper's output (with err its errors too), else the old command's (cmd) */
static char *root_ui_out(const char *const av[], int err, const char *cmd)
{
	if (root_have(av[0])) { int st; char *o = root_net_out(av, NULL, 0, err, &st); if (st != -2) return o; free(o); }
	return run_out(cmd);
}
/* the helper's status (its output nowhere, or with log into rudolfd's log as run() of cmd had it), else run(cmd);
   cmd NULL: -2 back to the caller, which then does the old work itself */
static int root_ui_run(const char *const av[], int log, const char *cmd)
{
	int st = root_have(av[0]) ? root_net(av, !log) : -2;
	return st == -2 && cmd ? run(cmd) : st;
}
