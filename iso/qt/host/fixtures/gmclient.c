/* qt/host/gamemodeportal161_t.sh: a game asking for GameMode as libgamemode does (1.6.1, docs/plan-1.6.1.md Part 19):
   gmclient feral|portal|pidfd [PID]   registers PID (its own when not given) with com.feralinteractive.GameMode, the
   portal's GameMode (a pid) or the portal's GameMode by pidfd; prints "REG rc", then "Q rc" (QueryStatus); waits for a
   line on stdin: "unreg" unregisters ("UNREG rc", "Q rc"), "exit" leaves the bus without unregistering. */
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
static GDBusConnection *B; static int portal, pidfd;
static int call(const char *m, gint32 pid)
{
	GError *e = NULL; GVariant *r; char mm[64];
	if (pidfd) { int fd = (int)syscall(SYS_pidfd_open, pid, 0), me = (int)syscall(SYS_pidfd_open, getpid(), 0); GUnixFDList *fl = g_unix_fd_list_new(); gint32 a = g_unix_fd_list_append(fl, fd, NULL), b = g_unix_fd_list_append(fl, me, NULL);
		snprintf(mm, sizeof mm, "%sByPIDFd", m);
		r = g_dbus_connection_call_with_unix_fd_list_sync(B, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.GameMode", mm, g_variant_new("(hh)", a, b), G_VARIANT_TYPE("(i)"), 0, 5000, fl, NULL, NULL, &e);
		close(fd); close(me); g_object_unref(fl); }
	else r = g_dbus_connection_call_sync(B, portal ? "org.freedesktop.portal.Desktop" : "com.feralinteractive.GameMode", portal ? "/org/freedesktop/portal/desktop" : "/com/feralinteractive/GameMode",
		portal ? "org.freedesktop.portal.GameMode" : "com.feralinteractive.GameMode", m, g_variant_new("(i)", pid), G_VARIANT_TYPE("(i)"), 0, 5000, NULL, &e);
	if (!r) { printf("ERROR %s\n", e->message); fflush(stdout); return -9; }
	gint32 v; g_variant_get(r, "(i)", &v); g_variant_unref(r); return v;
}
int main(int argc, char **argv)
{
	if (argc < 2) return 2;
	portal = !strcmp(argv[1], "portal") || !strcmp(argv[1], "pidfd"); pidfd = !strcmp(argv[1], "pidfd");
	GError *e = NULL; B = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &e); if (!B) { printf("ERROR %s\n", e->message); return 1; }
	gint32 pid = argc > 2 ? atoi(argv[2]) : getpid();
	printf("REG %d\n", call("RegisterGame", pid)); printf("Q %d\n", call("QueryStatus", pid)); fflush(stdout);
	char l[64]; if (!fgets(l, sizeof l, stdin) || strncmp(l, "unreg", 5)) return 0;
	printf("UNREG %d\n", call("UnregisterGame", pid)); printf("Q %d\n", call("QueryStatus", pid)); fflush(stdout);
	if (fgets(l, sizeof l, stdin)) {}
	return 0;
}
