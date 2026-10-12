/* A tiny X11 app for testing Rudolf OS's Linux app support (window manager, launching with a file,
 * desktop portal). Prints what happens to stdout.
 *   testapp [--class C] [--title T] [--portal-open] [--portal-save NAME] [--notify] [--open-url URL] [--settings] [FILE]
 * GPL-2.0-or-later. */
#include <gio/gio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GMainLoop *loop; static GVariant *answer; static guint32 code = 99;
static void on_response(GDBusConnection *c, const char *s, const char *p, const char *i, const char *sig, GVariant *params, gpointer u)
{ (void)c; (void)s; (void)p; (void)i; (void)sig; (void)u; g_variant_get(params, "(u@a{sv})", &code, &answer); g_main_loop_quit(loop); }
static gboolean on_timeout(gpointer u) { (void)u; g_main_loop_quit(loop); return FALSE; }
/* calls a portal method that answers through a Request object, waits for the Response */
static int portal_request(GDBusConnection *bus, const char *iface, const char *method, GVariant *args_without_options, GVariantBuilder *opts, const char *token)
{
	char *sender = g_strdup(g_dbus_connection_get_unique_name(bus) + 1); for (char *c = sender; *c; c++) if (*c == '.') *c = '_';
	char *path = g_strdup_printf("/org/freedesktop/portal/desktop/request/%s/%s", sender, token);
	g_variant_builder_add(opts, "{sv}", "handle_token", g_variant_new_string(token));
	guint sub = g_dbus_connection_signal_subscribe(bus, "org.freedesktop.portal.Desktop", "org.freedesktop.portal.Request", "Response", path, NULL, G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE, on_response, NULL, NULL);
	GVariantBuilder all; g_variant_builder_init(&all, G_VARIANT_TYPE_TUPLE);
	GVariantIter it; GVariant *ch; g_variant_iter_init(&it, args_without_options); while ((ch = g_variant_iter_next_value(&it))) g_variant_builder_add_value(&all, ch);
	g_variant_builder_add_value(&all, g_variant_builder_end(opts));
	GError *e = NULL;
	GVariant *r = g_dbus_connection_call_sync(bus, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", iface, method, g_variant_builder_end(&all), G_VARIANT_TYPE("(o)"), 0, 10000, NULL, &e);
	if (!r) { printf("portal %s failed: %s\n", method, e->message); fflush(stdout); return -1; }
	const char *got; g_variant_get(r, "(&o)", &got); printf("portal %s request %s\n", method, strcmp(got, path) ? "path differs" : "path ok"); fflush(stdout);
	loop = g_main_loop_new(NULL, FALSE); g_timeout_add_seconds(180, on_timeout, NULL); g_main_loop_run(loop);
	g_dbus_connection_signal_unsubscribe(bus, sub);
	printf("portal %s response %u\n", method, code);
	if (answer) { GVariant *u = g_variant_lookup_value(answer, "uris", G_VARIANT_TYPE("as")); if (u) { const char **l = g_variant_get_strv(u, NULL); for (int i = 0; l && l[i]; i++) printf("portal uri %s\n", l[i]); g_free(l); } }
	fflush(stdout); return code;
}

int main(int argc, char **argv)
{
	const char *cls = "RudolfTest", *title = "Rudolf test app", *save = NULL, *url = NULL, *file = NULL; int popen_ = 0, notify = 0, settings = 0;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--class") && i + 1 < argc) cls = argv[++i];
		else if (!strcmp(argv[i], "--title") && i + 1 < argc) title = argv[++i];
		else if (!strcmp(argv[i], "--portal-open")) popen_ = 1;
		else if (!strcmp(argv[i], "--portal-save") && i + 1 < argc) save = argv[++i];
		else if (!strcmp(argv[i], "--notify")) notify = 1;
		else if (!strcmp(argv[i], "--settings")) settings = 1;
		else if (!strcmp(argv[i], "--open-url") && i + 1 < argc) url = argv[++i];
		else file = argv[i];
	}
	setvbuf(stdout, NULL, _IOLBF, 0);
	if (file) {
		FILE *f = fopen(file, "r+"); if (!f) { printf("file %s cannot be opened\n", file); }
		else { fseek(f, 0, SEEK_END); long n = ftell(f); printf("file %s %ld bytes\n", file, n); fputs("edited by the test app\n", f); fclose(f); }
	}
	Display *d = XOpenDisplay(NULL); if (!d) { printf("no display\n"); return 1; }
	Window w = XCreateSimpleWindow(d, DefaultRootWindow(d), 10, 10, 400, 300, 0, 0, 0x3366cc);
	XClassHint ch = {(char *)cls, (char *)cls}; XSetClassHint(d, w, &ch); XStoreName(d, w, title);
	Atom del = XInternAtom(d, "WM_DELETE_WINDOW", False); XSetWMProtocols(d, w, &del, 1);
	XSelectInput(d, w, StructureNotifyMask); XMapWindow(d, w); XFlush(d);
	printf("window %lx up\n", (unsigned long)w);
	if (popen_ || save || notify || url || settings) {
		GError *e = NULL; GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &e);
		if (!bus) printf("no session bus: %s\n", e->message);
		else {
			if (settings) {
				GVariant *r = g_dbus_connection_call_sync(bus, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.Settings", "ReadOne",
					g_variant_new("(ss)", "org.freedesktop.appearance", "color-scheme"), NULL, 0, 5000, NULL, &e);
				printf("settings %s\n", r ? "ok" : e->message); if (r) g_variant_unref(r); e = NULL;
			}
			if (popen_) { GVariantBuilder o; g_variant_builder_init(&o, G_VARIANT_TYPE("a{sv}")); portal_request(bus, "org.freedesktop.portal.FileChooser", "OpenFile", g_variant_new("(ss)", "", "Open a test file"), &o, "t1"); }
			if (save) {
				GVariantBuilder o; g_variant_builder_init(&o, G_VARIANT_TYPE("a{sv}")); g_variant_builder_add(&o, "{sv}", "current_name", g_variant_new_string(save));
				if (portal_request(bus, "org.freedesktop.portal.FileChooser", "SaveFile", g_variant_new("(ss)", "", "Save a test file"), &o, "t2") == 0 && answer) {
					GVariant *u = g_variant_lookup_value(answer, "uris", G_VARIANT_TYPE("as"));
					if (u) { const char **l = g_variant_get_strv(u, NULL); if (l && l[0]) { char *p = g_filename_from_uri(l[0], NULL, NULL); FILE *f = p ? fopen(p, "w") : NULL; if (f) { fputs("saved by the test app\n", f); fclose(f); printf("saved %s\n", p); } else printf("cannot save %s\n", p ? p : l[0]); g_free(p); } g_free(l); }
				}
			}
			if (notify) {
				GVariantBuilder n; g_variant_builder_init(&n, G_VARIANT_TYPE("a{sv}")); g_variant_builder_add(&n, "{sv}", "title", g_variant_new_string("Test app")); g_variant_builder_add(&n, "{sv}", "body", g_variant_new_string("Hello from a Linux app"));
				GVariant *r = g_dbus_connection_call_sync(bus, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.Notification", "AddNotification",
					g_variant_new("(sa{sv})", "n1", &n), NULL, 0, 5000, NULL, &e);
				printf("notify %s\n", r ? "sent" : e->message); e = NULL;
			}
			if (url) { GVariantBuilder o; g_variant_builder_init(&o, G_VARIANT_TYPE("a{sv}")); portal_request(bus, "org.freedesktop.portal.OpenURI", "OpenURI", g_variant_new("(ss)", "", url), &o, "t3"); }
		}
	}
	for (;;) {
		XEvent e; XNextEvent(d, &e);
		if (e.type == ConfigureNotify) printf("geometry %dx%d+%d+%d\n", e.xconfigure.width, e.xconfigure.height, e.xconfigure.x, e.xconfigure.y);
		if (e.type == ClientMessage && (Atom)e.xclient.data.l[0] == del) { printf("asked to close: closing\n"); break; }
	}
	XCloseDisplay(d); return 0;
}
