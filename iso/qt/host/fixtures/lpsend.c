/* 1.6.2 (docs/plan-1.6.2.md Part 25b): a stand-in Linux app saying its progress as Firefox, Chromium and Electron apps do: the
   Unity LauncherEntry signal (com.canonical.Unity.LauncherEntry.Update, "application://ID.desktop" and its properties), broadcast
   on the session bus, from a connection that stays (as an app's). Lines on standard input:
     u URI PROGRESS VISIBLE   PROGRESS a number (or "nan", "inf"), VISIBLE 0 or 1; "-" leaves either out
     ps URI TEXT              "progress" as a string (malformed)
     vs URI TEXT              "progress-visible" as a string (malformed)
     one URI                  a signal of the wrong signature: (s) only
     q                        leaves the bus
   Prints "ready" once connected, "sent" after each signal.   gcc -o lpsend lpsend.c $(pkg-config --cflags --libs gio-2.0) -ldl
   (Part 25b's left part) LPSEND_AS says whose rule it follows before sending a progress at all, as their sources say (checked on
   2026-11-22), printing "believes 1" or "believes 0 REASON" once connected, and "skipped" in place of "sent" when it believes not:
     chromium   Chromium's DownloadStatusUpdater (chrome/browser/download/download_status_updater_linux.cc): the desktop, by
                base::nix::GetDesktopEnvironment (XDG_CURRENT_DESKTOP's names in order: Unity, unless DESKTOP_SESSION is
                gnome-fallback; GNOME, KDE, ...), must be Unity or KDE; then libunity (libunity.so.4, .6 or .9) must load, and its
                inspector must see Unity running: the name com.canonical.Unity owned on the session bus
     electron43 Electron up to 43 (shell/browser/linux/unity_service.cc): libunity and the name, whatever the desktop
     electron   Electron's main branch of 2026 (shell/browser/linux/launcher_entry.cc): the signal straight on the bus, always
   (no LPSEND_AS: always, as before) */
#include <gio/gio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
/* base::nix::GetDesktopEnvironment's answer from XDG_CURRENT_DESKTOP (the names it knows, the first one it knows wins) */
static const char *lp_desktop(void)
{
	const char *x = getenv("XDG_CURRENT_DESKTOP"), *ds = getenv("DESKTOP_SESSION"); static char r[32]; snprintf(r, sizeof r, "OTHER");
	if (!x) return r;
	char **v = g_strsplit(x, ":", -1);
	for (char **p = v; *p; p++) {
		char *t = g_strstrip(*p); if (!*t) continue;
		if (!strcmp(t, "Unity")) { snprintf(r, sizeof r, "%s", ds && strstr(ds, "gnome-fallback") ? "GNOME" : "UNITY"); break; }
		if (!strcmp(t, "GNOME") || !strcmp(t, "KDE") || !strcmp(t, "Deepin") || !strcmp(t, "X-Cinnamon") || !strcmp(t, "Pantheon") || !strcmp(t, "XFCE") || !strcmp(t, "UKUI") || !strcmp(t, "LXQt") || !strcmp(t, "COSMIC")) { snprintf(r, sizeof r, "%s", t); break; }
	}
	g_strfreev(v); return r;
}
static int lp_libunity(void) { const char *n[] = {"libunity.so.4", "libunity.so.6", "libunity.so.9"}; for (int i = 0; i < 3; i++) { void *h = dlopen(n[i], RTLD_LAZY); if (h) { dlclose(h); return 1; } } return 0; }
static int lp_unity_running(GDBusConnection *b)
{
	GVariant *r = g_dbus_connection_call_sync(b, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameHasOwner", g_variant_new("(s)", "com.canonical.Unity"), G_VARIANT_TYPE("(b)"), 0, 2000, NULL, NULL);
	gboolean o = FALSE; if (r) { g_variant_get(r, "(b)", &o); g_variant_unref(r); } return o;
}
/* whether it would send at all, and why not */
static int lp_believes(GDBusConnection *b, char *why, size_t n)
{
	const char *as = getenv("LPSEND_AS"); *why = 0;
	if (!as || !*as || !strcmp(as, "electron")) return 1;
	if (!strcmp(as, "chromium")) { const char *d = lp_desktop(); if (strcmp(d, "UNITY") && strcmp(d, "KDE")) { snprintf(why, n, "the desktop is %s, not Unity or KDE", d); return 0; } }
	else if (strcmp(as, "electron43")) { snprintf(why, n, "unknown LPSEND_AS"); return 0; }
	if (!lp_libunity()) { snprintf(why, n, "no libunity"); return 0; }
	if (!lp_unity_running(b)) { snprintf(why, n, "com.canonical.Unity has no owner"); return 0; }
	return 1;
}
int main(void)
{
	GError *e = NULL; GDBusConnection *b = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &e);
	if (!b) { fprintf(stderr, "lpsend: %s\n", e->message); return 1; }
	char why[120]; int yes = lp_believes(b, why, sizeof why);
	printf("ready\n"); if (getenv("LPSEND_AS")) printf("believes %d%s%s\n", yes, *why ? " " : "", why); fflush(stdout);
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
		if (!yes) { g_variant_unref(g_variant_ref_sink(p)); printf("skipped\n"); fflush(stdout); continue; }
		g_dbus_connection_emit_signal(b, NULL, "/com/canonical/unity/launcherentry/1", "com.canonical.Unity.LauncherEntry", "Update", p, NULL);
		g_dbus_connection_flush_sync(b, NULL, NULL);
		printf("sent\n"); fflush(stdout);
	}
	g_object_unref(b); return 0;
}
