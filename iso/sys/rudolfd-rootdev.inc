/* (included by rudolfd-root.inc, not built on its own: hence .inc) ---------- 1.6.1 (docs/plan-1.6.1.md Part 11, devices):
 * Bluetooth and Thunderbolt through the devices helper (rudolf-devices.c: /usr/lib/rudolf/root/devices)
 * GET bt and every POST bt/... (/usr/lib/rudolf/bluetooth), GET tb, POST tb/allow|deny|forget and System health's
 * Thunderbolt line (/usr/lib/rudolf/thunderbolt). Each call site keeps its old command for when the helper does not
 * start (it is not there, or refused by its confinement) and rudolfd is root (root_net_out of rudolfd-rootnet.inc:
 * the same way of asking). rudolfd's API is unchanged. */
/* the helper's output (with err its errors too), else the old command's (cmd, run as before) */
static char *root_dev_out(const char *const argv[], int err, const char *cmd)
{
	if (root_have("devices")) { int st; char *o = root_net_out(argv, NULL, 0, err, &st); if (st != -2) return o; free(o); }
	return run_out(cmd);
}
/* the helper's status with its output nowhere, else the old command's (run(cmd)) */
static int root_dev_run(const char *const argv[], const char *cmd)
{
	int st = root_have("devices") ? root_net(argv, 1) : -2;
	return st == -2 ? run(cmd) : st;
}
