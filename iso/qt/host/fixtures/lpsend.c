/* 1.6.2 (docs/plan-1.6.2.md Part 25b): a stand-in Linux app saying its progress as Firefox, Chromium and Electron apps do: the
   Unity LauncherEntry signal (com.canonical.Unity.LauncherEntry.Update, "application://ID.desktop" and its properties), broadcast
   on the session bus, from a connection that stays (as an app's). Lines on standard input:
     u URI PROGRESS VISIBLE   PROGRESS a number (or "nan", "inf"), VISIBLE 0 or 1; "-" leaves either out
     ps URI TEXT              "progress" as a string (malformed)
     vs URI TEXT              "progress-visible" as a string (malformed)
     one URI                  a signal of the wrong signature: (s) only
     q                        leaves the bus
   Prints "ready" once connected, "sent" after each signal.   gcc -o lpsend lpsend.c $(pkg-config --cflags --libs gio-2.0) */
#include <gio/gio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void)
{
	GError *e = NULL; GDBusConnection *b = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &e);
	if (!b) { fprintf(stderr, "lpsend: %s\n", e->message); return 1; }
	printf("ready\n"); fflush(stdout);
	char l[1024];
	while (fgets(l, sizeof l, stdin)) {
		l[strcspn(l, "\n")] = 0; char op[8] = "", uri[600] = "", a[64] = "", c[64] = ""; int n = sscanf(l, "%7s %599s %63s %63s", op, uri, a, c);
		if (!strcmp(op, "q")) break;
		GVariant *p = NULL; GVariantBuilder d; g_variant_builder_init(&d, G_VARIANT_TYPE("a{sv}"));
		if (!strcmp(op, "u") && n == 4) {
			if (strcmp(a, "-")) g_variant_builder_add(&d, "{sv}", "progress", g_variant_new_double(strtod(a, NULL)));
			if (strcmp(c, "-")) g_variant_builder_add(&d, "{sv}", "progress-visible", g_variant_new_boolean(atoi(c) != 0));
			p = g_variant_new("(sa{sv})", uri, &d);
		} else if (!strcmp(op, "ps") && n >= 3) { g_variant_builder_add(&d, "{sv}", "progress", g_variant_new_string(a)); g_variant_builder_add(&d, "{sv}", "progress-visible", g_variant_new_boolean(1)); p = g_variant_new("(sa{sv})", uri, &d); }
		else if (!strcmp(op, "vs") && n >= 3) { g_variant_builder_add(&d, "{sv}", "progress", g_variant_new_double(.5)); g_variant_builder_add(&d, "{sv}", "progress-visible", g_variant_new_string(a)); p = g_variant_new("(sa{sv})", uri, &d); }
		else if (!strcmp(op, "one") && n >= 2) { g_variant_builder_clear(&d); p = g_variant_new("(s)", uri); }
		else { g_variant_builder_clear(&d); continue; }
		g_dbus_connection_emit_signal(b, NULL, "/com/canonical/unity/launcherentry/1", "com.canonical.Unity.LauncherEntry", "Update", p, NULL);
		g_dbus_connection_flush_sync(b, NULL, NULL);
		printf("sent\n"); fflush(stdout);
	}
	g_object_unref(b); return 0;
}
