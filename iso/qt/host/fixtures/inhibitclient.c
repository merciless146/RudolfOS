/* qt/host/inhibit60_t.sh: an app asking to keep the screen on or the computer awake, and holding it while it runs:
   inhibitclient ss|pm|portal [FLAGS]   prints "HELD cookie-or-handle", then waits for a line on stdin: "release" lets
   it go (UnInhibit, or Close on the portal's Request), "exit" leaves the bus without letting go. */
#include <gio/gio.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv)
{
	if (argc < 2) return 2;
	GError *e = NULL; GDBusConnection *b = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &e); if (!b) { printf("ERROR %s\n", e->message); return 1; }
	GVariant *r; guint32 ck = 0; const char *h = NULL; int portal = !strcmp(argv[1], "portal"), pm = !strcmp(argv[1], "pm");
	if (portal) {
		GVariantBuilder o; g_variant_builder_init(&o, G_VARIANT_TYPE("a{sv}")); g_variant_builder_add(&o, "{sv}", "reason", g_variant_new_string("playing a video"));
		r = g_dbus_connection_call_sync(b, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.Inhibit", "Inhibit",
			g_variant_new("(su@a{sv})", "", argc > 2 ? (guint32)atoi(argv[2]) : 8, g_variant_builder_end(&o)), G_VARIANT_TYPE("(o)"), 0, 5000, NULL, &e);
		if (!r) { printf("ERROR %s\n", e->message); return 1; } g_variant_get(r, "(&o)", &h); printf("HELD %s\n", h);
	} else {
		r = g_dbus_connection_call_sync(b, pm ? "org.freedesktop.PowerManagement" : "org.freedesktop.ScreenSaver", pm ? "/org/freedesktop/PowerManagement/Inhibit" : "/org/freedesktop/ScreenSaver",
			pm ? "org.freedesktop.PowerManagement.Inhibit" : "org.freedesktop.ScreenSaver", "Inhibit", g_variant_new("(ss)", "Movie Player", "a film"), G_VARIANT_TYPE("(u)"), 0, 5000, NULL, &e);
		if (!r) { printf("ERROR %s\n", e->message); return 1; } g_variant_get(r, "(u)", &ck); printf("HELD %u\n", ck);
	}
	fflush(stdout);
	char l[64]; if (!fgets(l, sizeof l, stdin) || strncmp(l, "release", 7)) return 0;
	if (portal) g_dbus_connection_call_sync(b, "org.freedesktop.portal.Desktop", h, "org.freedesktop.portal.Request", "Close", NULL, NULL, 0, 5000, NULL, NULL);
	else g_dbus_connection_call_sync(b, pm ? "org.freedesktop.PowerManagement" : "org.freedesktop.ScreenSaver", pm ? "/org/freedesktop/PowerManagement/Inhibit" : "/org/freedesktop/ScreenSaver",
		pm ? "org.freedesktop.PowerManagement.Inhibit" : "org.freedesktop.ScreenSaver", "UnInhibit", g_variant_new("(u)", ck), NULL, 0, 5000, NULL, NULL);
	printf("RELEASED\n"); fflush(stdout);
	if (fgets(l, sizeof l, stdin)) {}
	return 0;
}
