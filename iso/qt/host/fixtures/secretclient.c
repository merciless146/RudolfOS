/* qt/host/keyportal60_t.sh: an app asking the Secret portal for its secret, as libsecret in a Flatpak sandbox does:
   RetrieveSecret(fd, {handle_token}) on org.freedesktop.portal.Desktop, then the Request's Response; prints
   "RESPONSE N HEX" (the secret read from the pipe, in hex) or "ERROR message". */
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <stdio.h>
#include <unistd.h>
static guint32 CODE = 99; static GMainLoop *L;
static void on_resp(GDBusConnection *c, const char *s, const char *p, const char *i, const char *n, GVariant *v, gpointer u)
{ (void)c; (void)s; (void)p; (void)i; (void)n; (void)u; GVariant *r; g_variant_get(v, "(u@a{sv})", &CODE, &r); g_variant_unref(r); g_main_loop_quit(L); }
static gboolean tmo(gpointer u) { (void)u; g_main_loop_quit(L); return FALSE; }
int main(void)
{
	GError *e = NULL; GDBusConnection *b = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &e); if (!b) { printf("ERROR %s\n", e->message); return 1; }
	char *me = g_strdup(g_dbus_connection_get_unique_name(b) + 1); for (char *c = me; *c; c++) if (*c == '.') *c = '_';
	char *path = g_strdup_printf("/org/freedesktop/portal/desktop/request/%s/t1", me);
	L = g_main_loop_new(NULL, FALSE);
	g_dbus_connection_signal_subscribe(b, NULL, "org.freedesktop.portal.Request", "Response", path, NULL, 0, on_resp, NULL, NULL);
	int pp[2]; if (pipe(pp)) return 1;
	GUnixFDList *fl = g_unix_fd_list_new(); int h = g_unix_fd_list_append(fl, pp[1], NULL); close(pp[1]);
	GVariantBuilder o; g_variant_builder_init(&o, G_VARIANT_TYPE("a{sv}")); g_variant_builder_add(&o, "{sv}", "handle_token", g_variant_new_string("t1"));
	GVariant *r = g_dbus_connection_call_with_unix_fd_list_sync(b, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.Secret",
		"RetrieveSecret", g_variant_new("(h@a{sv})", h, g_variant_builder_end(&o)), G_VARIANT_TYPE("(o)"), 0, 5000, fl, NULL, NULL, &e);
	if (!r) { printf("ERROR %s\n", e->message); return 1; }
	const char *got; g_variant_get(r, "(&o)", &got); if (strcmp(got, path)) { printf("ERROR handle %s, not %s\n", got, path); return 1; }
	g_timeout_add(5000, tmo, NULL); g_main_loop_run(L);
	unsigned char buf[256]; ssize_t n = read(pp[0], buf, sizeof buf); printf("RESPONSE %u ", CODE);
	for (ssize_t i = 0; i < n; i++) printf("%02x", buf[i]); printf("\n"); return 0;
}
