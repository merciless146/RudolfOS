/* rudolf-portal: the desktop portal of Rudolf OS, on the desktop user's session bus.
 *
 * Linux apps (Flatpak, sandboxed or not) ask "the desktop" for a few things over D-Bus. This service
 * answers as org.freedesktop.portal.Desktop with Rudolf's own dialogs:
 *   FileChooser   OpenFile, SaveFile, SaveFiles  -> the Rudolf file dialog
 *   OpenURI       OpenURI, OpenFile, OpenDirectory -> Oracle / the browser window / Files
 *   Notification  AddNotification, RemoveNotification -> Rudolf notifications
 *   Settings      ReadAll, Read, ReadOne         -> colour scheme
 *   ProxyResolver Lookup                         -> the proxy set in Rudolf OS, or none (1.2.8)
 *   NetworkMonitor GetStatus, GetAvailable, ...   -> online (1.2.8)
 * and as org.freedesktop.Notifications for apps that notify directly (libnotify).
 * Questions go to the desktop through rudolfd: a JSON file per request in /run/rudolf/portal/req,
 * the answer comes back in /run/rudolf/portal/resp. Files are handed over by path, so an app can use
 * a chosen file only where its sandbox lets it read and write (most desktop apps may use the home folder).
 * Copyright (C) 2026 the Rudolf OS authors. SPDX-License-Identifier: GPL-2.0-or-later */
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DIR_ "/run/rudolf/portal"
static GDBusConnection *BUS;
static guint SEQ;

typedef struct { char *id, *sender, *path; guint reg; } pending;
static GList *PENDING;

static void jstr(GString *s, const char *v)
{
	g_string_append_c(s, '"');
	for (const unsigned char *p = (const unsigned char *)(v ? v : ""); *p; p++) {
		if (*p == '"' || *p == '\\') { g_string_append_c(s, '\\'); g_string_append_c(s, *p); }
		else if (*p < 0x20) g_string_append_printf(s, "\\u%04x", *p);
		else g_string_append_c(s, *p);
	}
	g_string_append_c(s, '"');
}
/* the app behind a D-Bus caller: the Flatpak app ID from its sandbox's /.flatpak-info */
static char *caller_app(const char *sender)
{
	GVariant *r = g_dbus_connection_call_sync(BUS, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetConnectionUnixProcessID",
		g_variant_new("(s)", sender), G_VARIANT_TYPE("(u)"), 0, 2000, NULL, NULL);
	if (!r) return g_strdup("");
	guint32 pid; g_variant_get(r, "(u)", &pid); g_variant_unref(r);
	char path[64], *text = NULL; snprintf(path, sizeof path, "/proc/%u/root/.flatpak-info", pid);
	char *app = NULL;
	if (g_file_get_contents(path, &text, NULL, NULL)) {
		GKeyFile *k = g_key_file_new(); if (g_key_file_load_from_data(k, text, -1, 0, NULL)) app = g_key_file_get_string(k, "Application", "name", NULL); g_key_file_free(k); g_free(text);
	}
	return app ? app : g_strdup("");
}
static void write_request(const char *id, GString *json)
{
	char tmp[256], f[256]; snprintf(f, sizeof f, DIR_ "/req/%s.json", id); snprintf(tmp, sizeof tmp, "%s.tmp", f);
	if (g_file_set_contents(tmp, json->str, json->len, NULL)) rename(tmp, f);
}
static const char *opt_s(GVariant *o, const char *k) { const char *v = NULL; if (o) g_variant_lookup(o, k, "&s", &v); return v; }
static int opt_b(GVariant *o, const char *k) { gboolean v = FALSE; if (o) g_variant_lookup(o, k, "b", &v); return v; }
static char *opt_ay(GVariant *o, const char *k) { GVariant *v = o ? g_variant_lookup_value(o, k, G_VARIANT_TYPE_BYTESTRING) : NULL; char *s = v ? g_strdup(g_variant_get_bytestring(v)) : NULL; if (v) g_variant_unref(v); return s; }

/* ---------------- Request objects: the app waits for their Response signal */
static void req_method(GDBusConnection *c, const char *sender, const char *path, const char *iface, const char *method, GVariant *p, GDBusMethodInvocation *inv, gpointer u)
{
	(void)c; (void)sender; (void)path; (void)iface; (void)method; (void)p; (void)u;
	g_dbus_method_invocation_return_value(inv, NULL);   /* Close: the dialog simply stays until answered */
}
static const GDBusInterfaceVTable REQ_VT = {req_method, NULL, NULL, {0}};
static GDBusNodeInfo *REQ_INFO;
static char *request_path(const char *sender, GVariant *opts)
{
	char *s = g_strdup(sender + 1); for (char *c = s; *c; c++) if (*c == '.') *c = '_';
	const char *tok = opt_s(opts, "handle_token"); char gen[32]; if (!tok) { snprintf(gen, sizeof gen, "rudolf%u", ++SEQ); tok = gen; }
	char *p = g_strdup_printf("/org/freedesktop/portal/desktop/request/%s/%s", s, tok); g_free(s); return p;
}
/* starts a request: registers the Request object, writes the question for the desktop */
static void ask_desktop(GDBusMethodInvocation *inv, const char *sender, GVariant *opts, GString *json)
{
	pending *q = g_new0(pending, 1);
	q->id = g_strdup_printf("%ld-%u", (long)getpid(), ++SEQ); q->sender = g_strdup(sender); q->path = request_path(sender, opts);
	q->reg = g_dbus_connection_register_object(BUS, q->path, REQ_INFO->interfaces[0], &REQ_VT, NULL, NULL, NULL);
	write_request(q->id, json);
	PENDING = g_list_append(PENDING, q);
	g_dbus_method_invocation_return_value(inv, g_variant_new("(o)", q->path));
}
/* reads {"response":N,"uris":["…",…]} written by the desktop (through rudolfd) */
static void parse_answer(const char *t, guint32 *code, GPtrArray *uris)
{
	const char *r = strstr(t, "\"response\""); *code = 2;
	if (r && (r = strchr(r, ':'))) *code = (guint32)strtoul(r + 1, NULL, 10);
	const char *a = strstr(t, "\"uris\""); if (!a || !(a = strchr(a, '['))) return;
	for (const char *p = a + 1; *p && *p != ']';) {
		if (*p != '"') { p++; continue; }
		GString *v = g_string_new(NULL); p++;
		while (*p && *p != '"') {
			if (*p == '\\' && p[1]) { p++; if (*p == 'u') { unsigned c = (unsigned)strtoul((char[]){p[1], p[2], p[3], p[4], 0}, NULL, 16); g_string_append_unichar(v, c); p += 4; } else g_string_append_c(v, *p == 'n' ? '\n' : *p); }
			else g_string_append_c(v, *p);
			p++;
		}
		if (*p) p++;
		g_ptr_array_add(uris, g_string_free(v, FALSE));
	}
}
static gboolean check_answers(gpointer u)
{
	(void)u;
	for (GList *l = PENDING; l;) {
		pending *q = l->data; GList *next = l->next;
		char f[256], *text = NULL; snprintf(f, sizeof f, DIR_ "/resp/%s.json", q->id);
		if (g_file_get_contents(f, &text, NULL, NULL)) {
			unlink(f);
			guint32 code; GPtrArray *uris = g_ptr_array_new_with_free_func(g_free); parse_answer(text, &code, uris); g_free(text);
			GVariantBuilder res; g_variant_builder_init(&res, G_VARIANT_TYPE("a{sv}"));
			if (uris->len) { g_ptr_array_add(uris, NULL); g_variant_builder_add(&res, "{sv}", "uris", g_variant_new_strv((const char *const *)uris->pdata, -1)); }
			g_dbus_connection_emit_signal(BUS, q->sender, q->path, "org.freedesktop.portal.Request", "Response", g_variant_new("(ua{sv})", code, &res), NULL);
			g_dbus_connection_unregister_object(BUS, q->reg);
			g_ptr_array_free(uris, TRUE); g_free(q->id); g_free(q->sender); g_free(q->path); g_free(q);
			PENDING = g_list_delete_link(PENDING, l);
		}
		l = next;
	}
	return TRUE;
}

/* ---------------- the portal interfaces */
static void portal_method(GDBusConnection *c, const char *sender, const char *path, const char *iface, const char *method, GVariant *p, GDBusMethodInvocation *inv, gpointer u)
{
	(void)c; (void)path; (void)u;
	char *app = caller_app(sender);
	GString *j = g_string_new("{\"app\":"); jstr(j, app);
	if (!strcmp(iface, "org.freedesktop.portal.FileChooser")) {
		const char *parent, *title; GVariant *o; g_variant_get(p, "(&s&s@a{sv})", &parent, &title, &o);
		g_string_append(j, ",\"kind\":"); jstr(j, !strcmp(method, "OpenFile") ? "open" : "save");
		g_string_append(j, ",\"title\":"); jstr(j, title);
		g_string_append_printf(j, ",\"multiple\":%s,\"directory\":%s", opt_b(o, "multiple") ? "true" : "false", opt_b(o, "directory") ? "true" : "false");
		g_string_append(j, ",\"name\":"); jstr(j, opt_s(o, "current_name"));
		char *folder = opt_ay(o, "current_folder"); g_string_append(j, ",\"folder\":"); jstr(j, folder); g_free(folder);
		char *file = opt_ay(o, "current_file"); g_string_append(j, ",\"file\":"); jstr(j, file); g_free(file);
		g_string_append(j, ",\"accept\":"); jstr(j, opt_s(o, "accept_label"));
		/* file type filters: [(name, [(kind, pattern)])] -> patterns only */
		g_string_append(j, ",\"patterns\":[");
		GVariant *fl = g_variant_lookup_value(o, "filters", G_VARIANT_TYPE("a(sa(us))")); int first = 1;
		if (fl) { GVariantIter it; const char *nm; GVariant *pats; g_variant_iter_init(&it, fl);
			while (g_variant_iter_next(&it, "(&s@a(us))", &nm, &pats)) { GVariantIter i2; guint32 kd; const char *pt; g_variant_iter_init(&i2, pats);
				while (g_variant_iter_next(&i2, "(u&s)", &kd, &pt)) if (kd == 0) { if (!first) g_string_append_c(j, ','); first = 0; jstr(j, pt); }
				g_variant_unref(pats); }
			g_variant_unref(fl); }
		g_string_append(j, "]}");
		ask_desktop(inv, sender, o, j); g_variant_unref(o);
	} else if (!strcmp(iface, "org.freedesktop.portal.OpenURI")) {
		GVariant *o = NULL;
		if (!strcmp(method, "OpenURI")) {
			const char *parent, *uri; g_variant_get(p, "(&s&s@a{sv})", &parent, &uri, &o);
			g_string_append(j, ",\"kind\":\"url\",\"uri\":"); jstr(j, uri);
		} else {
			/* OpenFile / OpenDirectory hand over an open file: find its path */
			const char *parent; gint32 h; g_variant_get(p, "(&sh@a{sv})", &parent, &h, &o);
			GUnixFDList *fds = g_dbus_message_get_unix_fd_list(g_dbus_method_invocation_get_message(inv)); char *pth = NULL;
			int fd = fds ? g_unix_fd_list_get(fds, h, NULL) : -1;
			if (fd >= 0) { char l[64]; snprintf(l, sizeof l, "/proc/self/fd/%d", fd); pth = g_file_read_link(l, NULL); close(fd); }
			g_string_append(j, ",\"kind\":"); jstr(j, !strcmp(method, "OpenDirectory") ? "folder" : "openfile"); g_string_append(j, ",\"path\":"); jstr(j, pth); g_free(pth);
		}
		g_string_append(j, "}"); ask_desktop(inv, sender, o, j); if (o) g_variant_unref(o);
	} else if (!strcmp(iface, "org.freedesktop.portal.Notification")) {
		if (!strcmp(method, "AddNotification")) {
			const char *id; GVariant *n; g_variant_get(p, "(&s@a{sv})", &id, &n);
			g_string_append(j, ",\"kind\":\"notify\",\"title\":"); jstr(j, opt_s(n, "title")); g_string_append(j, ",\"body\":"); jstr(j, opt_s(n, "body")); g_string_append(j, "}");
			char rid[48]; snprintf(rid, sizeof rid, "n%ld-%u", (long)getpid(), ++SEQ); write_request(rid, j); g_variant_unref(n);
		}
		g_dbus_method_invocation_return_value(inv, NULL);
	} else if (!strcmp(iface, "org.freedesktop.portal.Settings")) {
		/* colour scheme: 0 no preference, 1 dark, 2 light (the desktop writes /run/rudolf/portal/scheme) */
		char *t = NULL; guint32 scheme = 0; if (g_file_get_contents(DIR_ "/scheme", &t, NULL, NULL)) { scheme = (guint32)atoi(t); g_free(t); }
		GVariant *val = g_variant_new_uint32(scheme);
		if (!strcmp(method, "ReadAll")) {
			GVariantBuilder all, ns; g_variant_builder_init(&all, G_VARIANT_TYPE("a{sa{sv}}")); g_variant_builder_init(&ns, G_VARIANT_TYPE("a{sv}"));
			g_variant_builder_add(&ns, "{sv}", "color-scheme", val); g_variant_builder_add(&all, "{s@a{sv}}", "org.freedesktop.appearance", g_variant_builder_end(&ns));
			g_dbus_method_invocation_return_value(inv, g_variant_new("(@a{sa{sv}})", g_variant_builder_end(&all)));
		} else {
			const char *nsp, *key; g_variant_get(p, "(&s&s)", &nsp, &key);
			if (strcmp(nsp, "org.freedesktop.appearance") || strcmp(key, "color-scheme")) { g_variant_unref(g_variant_ref_sink(val)); g_dbus_method_invocation_return_dbus_error(inv, "org.freedesktop.portal.Error.NotFound", "Requested setting not found"); }
			else if (!strcmp(method, "ReadOne")) g_dbus_method_invocation_return_value(inv, g_variant_new("(v)", val));
			else g_dbus_method_invocation_return_value(inv, g_variant_new("(v)", g_variant_new_variant(val)));
		}
	} else if (!strcmp(iface, "org.freedesktop.portal.ProxyResolver")) {
		/* 1.2.8: GLib inside a Flatpak sandbox asks the portal for the proxy of every address (GProxyResolverPortal);
		   without this interface each request fails ("No such interface ...ProxyResolver"), so Sober's sign-in page
		   (WebKitGTK) stayed blank and signing in to Roblox failed. The answer: Rudolf OS's proxy (Settings > Network,
		   /run/rudolf/proxy, as Chromium's --proxy-server takes it), or a direct connection. */
		char *t = NULL; const char *px = "direct://"; char buf[512];
		if (g_file_get_contents("/run/rudolf/proxy", &t, NULL, NULL)) { g_strstrip(t);
			if (*t) { if (strstr(t, "://")) snprintf(buf, sizeof buf, "%s", t); else snprintf(buf, sizeof buf, "http://%s", t); px = buf; } }
		const char *list[] = {px, NULL};
		g_dbus_method_invocation_return_value(inv, g_variant_new("(^as)", list)); g_free(t);
	} else if (!strcmp(iface, "org.freedesktop.portal.NetworkMonitor")) {
		/* 1.2.8: online, not metered, full connectivity (GNetworkMonitorPortal); Rudolf OS shows the real state itself */
		if (!strcmp(method, "GetAvailable") || !strcmp(method, "CanReach")) g_dbus_method_invocation_return_value(inv, g_variant_new("(b)", TRUE));
		else if (!strcmp(method, "GetMetered")) g_dbus_method_invocation_return_value(inv, g_variant_new("(b)", FALSE));
		else if (!strcmp(method, "GetConnectivity")) g_dbus_method_invocation_return_value(inv, g_variant_new("(u)", 4));
		else { GVariantBuilder b; g_variant_builder_init(&b, G_VARIANT_TYPE("a{sv}"));
			g_variant_builder_add(&b, "{sv}", "available", g_variant_new_boolean(TRUE)); g_variant_builder_add(&b, "{sv}", "metered", g_variant_new_boolean(FALSE));
			g_variant_builder_add(&b, "{sv}", "connectivity", g_variant_new_uint32(4));
			g_dbus_method_invocation_return_value(inv, g_variant_new("(@a{sv})", g_variant_builder_end(&b))); }
	} else g_dbus_method_invocation_return_dbus_error(inv, "org.freedesktop.DBus.Error.UnknownMethod", "not supported");
	g_string_free(j, TRUE); g_free(app);
}
static GVariant *portal_prop(GDBusConnection *c, const char *sender, const char *path, const char *iface, const char *prop, GError **e, gpointer u)
{
	(void)c; (void)sender; (void)path; (void)e; (void)u;
	if (!strcmp(prop, "version")) return g_variant_new_uint32(!strcmp(iface, "org.freedesktop.portal.OpenURI") ? 4 : !strcmp(iface, "org.freedesktop.portal.FileChooser") ? 3 : !strcmp(iface, "org.freedesktop.portal.Settings") ? 2 : !strcmp(iface, "org.freedesktop.portal.NetworkMonitor") ? 3 : 1);
	if (!strcmp(prop, "SupportedOptions")) return g_variant_new("a{sv}", NULL);
	return NULL;
}
static const GDBusInterfaceVTable PORTAL_VT = {portal_method, portal_prop, NULL, {0}};

/* ---------------- org.freedesktop.Notifications (apps that notify without the portal) */
static void notif_method(GDBusConnection *c, const char *sender, const char *path, const char *iface, const char *method, GVariant *p, GDBusMethodInvocation *inv, gpointer u)
{
	(void)c; (void)path; (void)iface; (void)u;
	if (!strcmp(method, "Notify")) {
		const char *appn, *icon, *sum, *body; guint32 rep; GVariant *acts, *hints; gint32 tmo;
		g_variant_get(p, "(&su&s&s&s@as@a{sv}i)", &appn, &rep, &icon, &sum, &body, &acts, &hints, &tmo);
		GString *j = g_string_new("{\"app\":"); char *app = caller_app(sender); jstr(j, *app ? app : appn); g_free(app);
		g_string_append(j, ",\"kind\":\"notify\",\"title\":"); jstr(j, sum); g_string_append(j, ",\"body\":"); jstr(j, body); g_string_append(j, "}");
		char rid[48]; guint32 nid = ++SEQ; snprintf(rid, sizeof rid, "n%ld-%u", (long)getpid(), nid); write_request(rid, j); g_string_free(j, TRUE);
		g_variant_unref(acts); g_variant_unref(hints);
		g_dbus_method_invocation_return_value(inv, g_variant_new("(u)", rep ? rep : nid));
	} else if (!strcmp(method, "GetCapabilities")) { const char *caps[] = {"body", NULL}; g_dbus_method_invocation_return_value(inv, g_variant_new("(^as)", caps)); }
	else if (!strcmp(method, "GetServerInformation")) g_dbus_method_invocation_return_value(inv, g_variant_new("(ssss)", "Rudolf OS", "Rudolf OS", "1.1", "1.2"));
	else g_dbus_method_invocation_return_value(inv, NULL);   /* CloseNotification */
}
static const GDBusInterfaceVTable NOTIF_VT = {notif_method, NULL, NULL, {0}};

static const char XML[] =
	"<node>"
	"<interface name='org.freedesktop.portal.FileChooser'>"
	"<method name='OpenFile'><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='a{sv}' direction='in'/><arg type='o' direction='out'/></method>"
	"<method name='SaveFile'><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='a{sv}' direction='in'/><arg type='o' direction='out'/></method>"
	"<method name='SaveFiles'><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='a{sv}' direction='in'/><arg type='o' direction='out'/></method>"
	"<property name='version' type='u' access='read'/></interface>"
	"<interface name='org.freedesktop.portal.OpenURI'>"
	"<method name='OpenURI'><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='a{sv}' direction='in'/><arg type='o' direction='out'/></method>"
	"<method name='OpenFile'><arg type='s' direction='in'/><arg type='h' direction='in'/><arg type='a{sv}' direction='in'/><arg type='o' direction='out'/></method>"
	"<method name='OpenDirectory'><arg type='s' direction='in'/><arg type='h' direction='in'/><arg type='a{sv}' direction='in'/><arg type='o' direction='out'/></method>"
	"<property name='version' type='u' access='read'/></interface>"
	"<interface name='org.freedesktop.portal.Notification'>"
	"<method name='AddNotification'><arg type='s' direction='in'/><arg type='a{sv}' direction='in'/></method>"
	"<method name='RemoveNotification'><arg type='s' direction='in'/></method>"
	"<property name='version' type='u' access='read'/><property name='SupportedOptions' type='a{sv}' access='read'/></interface>"
	"<interface name='org.freedesktop.portal.Settings'>"
	"<method name='ReadAll'><arg type='as' direction='in'/><arg type='a{sa{sv}}' direction='out'/></method>"
	"<method name='Read'><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='v' direction='out'/></method>"
	"<method name='ReadOne'><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='v' direction='out'/></method>"
	"<signal name='SettingChanged'><arg type='s'/><arg type='s'/><arg type='v'/></signal>"
	"<property name='version' type='u' access='read'/></interface>"
	"<interface name='org.freedesktop.portal.ProxyResolver'>"
	"<method name='Lookup'><arg type='s' direction='in'/><arg type='as' direction='out'/></method>"
	"<property name='version' type='u' access='read'/></interface>"
	"<interface name='org.freedesktop.portal.NetworkMonitor'>"
	"<method name='GetAvailable'><arg type='b' direction='out'/></method>"
	"<method name='GetMetered'><arg type='b' direction='out'/></method>"
	"<method name='GetConnectivity'><arg type='u' direction='out'/></method>"
	"<method name='GetStatus'><arg type='a{sv}' direction='out'/></method>"
	"<method name='CanReach'><arg type='s' direction='in'/><arg type='u' direction='in'/><arg type='b' direction='out'/></method>"
	"<signal name='changed'/>"
	"<property name='version' type='u' access='read'/></interface>"
	"</node>";
static const char REQ_XML[] = "<node><interface name='org.freedesktop.portal.Request'><method name='Close'/>"
	"<signal name='Response'><arg type='u'/><arg type='a{sv}'/></signal></interface></node>";
static const char NOTIF_XML[] = "<node><interface name='org.freedesktop.Notifications'>"
	"<method name='Notify'><arg type='s' direction='in'/><arg type='u' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='as' direction='in'/><arg type='a{sv}' direction='in'/><arg type='i' direction='in'/><arg type='u' direction='out'/></method>"
	"<method name='CloseNotification'><arg type='u' direction='in'/></method>"
	"<method name='GetCapabilities'><arg type='as' direction='out'/></method>"
	"<method name='GetServerInformation'><arg type='s' direction='out'/><arg type='s' direction='out'/><arg type='s' direction='out'/><arg type='s' direction='out'/></method>"
	"</interface></node>";

int main(void)
{
	GError *e = NULL;
	g_mkdir_with_parents(DIR_ "/req", 0755); g_mkdir_with_parents(DIR_ "/resp", 0755);
	BUS = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &e);
	if (!BUS) { fprintf(stderr, "rudolf-portal: no session bus: %s\n", e->message); return 1; }
	GDBusNodeInfo *info = g_dbus_node_info_new_for_xml(XML, &e); REQ_INFO = g_dbus_node_info_new_for_xml(REQ_XML, NULL);
	GDBusNodeInfo *ninfo = g_dbus_node_info_new_for_xml(NOTIF_XML, NULL);
	if (!info) { fprintf(stderr, "rudolf-portal: %s\n", e->message); return 1; }
	for (int i = 0; info->interfaces[i]; i++) g_dbus_connection_register_object(BUS, "/org/freedesktop/portal/desktop", info->interfaces[i], &PORTAL_VT, NULL, NULL, NULL);
	g_dbus_connection_register_object(BUS, "/org/freedesktop/Notifications", ninfo->interfaces[0], &NOTIF_VT, NULL, NULL, NULL);
	g_bus_own_name_on_connection(BUS, "org.freedesktop.portal.Desktop", G_BUS_NAME_OWNER_FLAGS_REPLACE, NULL, NULL, NULL, NULL);
	g_bus_own_name_on_connection(BUS, "org.freedesktop.Notifications", G_BUS_NAME_OWNER_FLAGS_REPLACE, NULL, NULL, NULL, NULL);
	g_timeout_add(200, check_answers, NULL);
	fprintf(stderr, "rudolf-portal: ready\n");
	g_main_loop_run(g_main_loop_new(NULL, FALSE));
	return 0;
}
