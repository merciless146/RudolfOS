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
 *   Secret        RetrieveSecret                 -> the app's own secret, for libsecret in a Flatpak sandbox (1.6.0)
 * and as org.gnome.keyring.SystemPrompter (gcr's prompter, 1.6.0): gnome-keyring-daemon's questions (a new keyring's
 * password, unlocking one) drawn by the desktop, the password handed back through gcr's secret exchange,
 * and as org.freedesktop.Notifications for apps that notify directly (libnotify).
 * Questions go to the desktop through rudolfd: a JSON file per request in /run/rudolf/portal/req,
 * the answer comes back in /run/rudolf/portal/resp. Files are handed over by path, so an app can use
 * a chosen file only where its sandbox lets it read and write (most desktop apps may use the home folder).
 * 1.6.0 (docs/plan-1.6.0.md Part 14): also, run by rudolfd as the signed-in account's user for Settings › Privacy ›
 * Saved passwords for apps:  rudolf-portal --keyring-list  (JSON: what the account's keyring and its Flatpak apps'
 * secrets hold, names only, never a secret) and  rudolf-portal --keyring-forget ID  (one of them removed).
 * Copyright (C) 2026 the Rudolf OS authors. SPDX-License-Identifier: GPL-2.0-or-later */
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>

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
static void kp_check(void);
static gboolean check_answers(gpointer u)
{
	(void)u;
	kp_check();
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

/* ---------------- 1.6.0: the Secret portal. libsecret inside a Flatpak sandbox keeps the app's secrets in a file of its
   own (~/.var/app/ID/data/keyrings), encrypted with a secret the portal gives it; that secret is the app's own, made
   once, in the account's folder (encrypted with the account's key, its user's only, 0700/0600), as gnome-keyring's own
   portal keeps it in the keyring. Only for a sandboxed app (its Flatpak ID): other programs use the Secret Service. */
static int app_id_ok(const char *a)
{
	size_t n = a ? strlen(a) : 0; if (!n || n > 255 || a[0] == '.' || strstr(a, "..")) return 0;
	for (const char *c = a; *c; c++) if (!g_ascii_isalnum(*c) && *c != '.' && *c != '_' && *c != '-') return 0;
	return 1;
}
static char *secrets_dir(void)
{
	const char *h = g_get_home_dir(); if (!h || !*h || !strcmp(h, "/")) return NULL;
	return g_build_filename(h, ".local", "share", "rudolf", "app-secrets", NULL);
}
/* the app's secret (64 bytes), made the first time; NULL if it cannot be kept */
static GBytes *app_secret(const char *app)
{
	char *d = secrets_dir(); if (!d) return NULL;
	if (g_mkdir_with_parents(d, 0700) || chmod(d, 0700)) { g_free(d); return NULL; }
	char *f = g_build_filename(d, app, NULL); g_free(d);
	gchar *b = NULL; gsize n = 0;
	if (g_file_get_contents(f, &b, &n, NULL) && n == 64) { g_free(f); return g_bytes_new_take(b, n); }
	g_free(b);
	guchar k[64]; if (getrandom(k, sizeof k, 0) != (ssize_t)sizeof k) { g_free(f); return NULL; }
	char *t = g_strdup_printf("%s.tmp", f); int fd = open(t, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0600); int ok = 0;
	if (fd >= 0) { ok = write(fd, k, sizeof k) == (ssize_t)sizeof k; if (close(fd)) ok = 0; if (ok) ok = !rename(t, f); if (!ok) unlink(t); }
	g_free(t); g_free(f);
	GBytes *r = ok ? g_bytes_new(k, sizeof k) : NULL; explicit_bzero(k, sizeof k); return r;
}
typedef struct { char *sender, *path; guint reg; } secret_req;
static gboolean secret_respond(gpointer u)
{
	secret_req *q = u; GVariantBuilder res; g_variant_builder_init(&res, G_VARIANT_TYPE("a{sv}"));
	g_dbus_connection_emit_signal(BUS, q->sender, q->path, "org.freedesktop.portal.Request", "Response", g_variant_new("(ua{sv})", 0, &res), NULL);
	g_dbus_connection_unregister_object(BUS, q->reg); g_free(q->sender); g_free(q->path); g_free(q); return FALSE;
}
static void retrieve_secret(GDBusMethodInvocation *inv, const char *sender, const char *app, GVariant *p)
{
	gint32 h; GVariant *o; g_variant_get(p, "(h@a{sv})", &h, &o);
	GUnixFDList *fds = g_dbus_message_get_unix_fd_list(g_dbus_method_invocation_get_message(inv));
	int fd = fds ? g_unix_fd_list_get(fds, h, NULL) : -1;
	if (fd < 0) { g_variant_unref(o); g_dbus_method_invocation_return_dbus_error(inv, "org.freedesktop.portal.Error.InvalidArgument", "no file descriptor"); return; }
	if (!app_id_ok(app)) { close(fd); g_variant_unref(o); g_dbus_method_invocation_return_dbus_error(inv, "org.freedesktop.portal.Error.NotAllowed", "only an app in a sandbox (Flatpak) is given a secret; other programs use the Secret Service"); return; }
	GBytes *s = app_secret(app);
	if (!s) { close(fd); g_variant_unref(o); g_dbus_method_invocation_return_dbus_error(inv, "org.freedesktop.portal.Error.Failed", "the app's secret could not be kept (no account signed in?)"); return; }
	gsize n; const guchar *k = g_bytes_get_data(s, &n); size_t w = 0;
	while (w < n) { ssize_t x = write(fd, k + w, n - w); if (x < 0 && errno == EINTR) continue; if (x <= 0) break; w += (size_t)x; }
	close(fd); g_bytes_unref(s);
	if (w != n) { g_variant_unref(o); g_dbus_method_invocation_return_dbus_error(inv, "org.freedesktop.portal.Error.Failed", "the secret could not be written"); return; }
	secret_req *q = g_new0(secret_req, 1); q->sender = g_strdup(sender); q->path = request_path(sender, o); g_variant_unref(o);
	q->reg = g_dbus_connection_register_object(BUS, q->path, REQ_INFO->interfaces[0], &REQ_VT, NULL, NULL, NULL);
	g_dbus_method_invocation_return_value(inv, g_variant_new("(o)", q->path));
	g_idle_add(secret_respond, q);
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
	} else if (!strcmp(iface, "org.freedesktop.portal.Secret")) {
		retrieve_secret(inv, sender, app, p);
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
	"<interface name='org.freedesktop.portal.Secret'>"
	"<method name='RetrieveSecret'><arg type='h' direction='in'/><arg type='a{sv}' direction='in'/><arg type='o' direction='out'/></method>"
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

/* ---------------- 1.6.0: Settings › Privacy › Saved passwords for apps (as the account's user, from rudolfd) */
#define SS "org.freedesktop.secrets"
static GVariant *ss_prop(GDBusConnection *b, const char *path, const char *iface, const char *prop)
{
	GVariant *r = g_dbus_connection_call_sync(b, SS, path, "org.freedesktop.DBus.Properties", "Get", g_variant_new("(ss)", iface, prop),
		G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NO_AUTO_START, 5000, NULL, NULL);
	if (!r) return NULL;
	GVariant *v; g_variant_get(r, "(v)", &v); g_variant_unref(r); return v;
}
static int keyring_list(void)
{
	GDBusConnection *b = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	GString *j = g_string_new("{\"ok\":true,\"keyring\":"); int n = 0;
	GVariant *cols = b ? ss_prop(b, "/org/freedesktop/secrets", "org.freedesktop.Secret.Service", "Collections") : NULL;
	g_string_append(j, cols ? "true" : "false"); g_string_append(j, ",\"items\":[");
	if (cols) { GVariantIter ci; const char *cp; g_variant_iter_init(&ci, cols);
		while (g_variant_iter_next(&ci, "&o", &cp)) {
			GVariant *its = ss_prop(b, cp, "org.freedesktop.Secret.Collection", "Items"); if (!its) continue;
			GVariant *cl = ss_prop(b, cp, "org.freedesktop.Secret.Collection", "Label");
			GVariantIter ii; const char *ip; g_variant_iter_init(&ii, its);
			while (g_variant_iter_next(&ii, "&o", &ip) && n < 500) {
				GVariant *lb = ss_prop(b, ip, "org.freedesktop.Secret.Item", "Label"), *at = ss_prop(b, ip, "org.freedesktop.Secret.Item", "Attributes");
				const char *app = NULL, *schema = NULL; if (at) { g_variant_lookup(at, "application", "&s", &app); if (!app) g_variant_lookup(at, "app_id", "&s", &app); g_variant_lookup(at, "xdg:schema", "&s", &schema); }
				if (n++) g_string_append_c(j, ',');
				g_string_append(j, "{\"id\":"); jstr(j, ip); g_string_append(j, ",\"label\":"); jstr(j, lb ? g_variant_get_string(lb, NULL) : "");
				g_string_append(j, ",\"app\":"); jstr(j, app); g_string_append(j, ",\"schema\":"); jstr(j, schema);
				g_string_append(j, ",\"keyring\":"); jstr(j, cl ? g_variant_get_string(cl, NULL) : ""); g_string_append(j, ",\"kind\":\"item\"}");
				if (lb) g_variant_unref(lb);
				if (at) g_variant_unref(at);
			}
			g_variant_unref(its); if (cl) g_variant_unref(cl);
		}
		g_variant_unref(cols); }
	/* the Flatpak apps' own secrets (the Secret portal) */
	char *d = secrets_dir(); GDir *dir = d ? g_dir_open(d, 0, NULL) : NULL; const char *nm;
	while (dir && (nm = g_dir_read_name(dir)) && n < 600) { if (!app_id_ok(nm) || g_str_has_suffix(nm, ".tmp")) continue;
		if (n++) g_string_append_c(j, ',');
		g_string_append(j, "{\"id\":"); char *id = g_strdup_printf("app:%s", nm); jstr(j, id); g_free(id);
		g_string_append(j, ",\"label\":"); jstr(j, nm); g_string_append(j, ",\"app\":"); jstr(j, nm); g_string_append(j, ",\"kind\":\"flatpak\"}"); }
	if (dir) g_dir_close(dir); g_free(d);
	g_string_append(j, "]}\n"); fputs(j->str, stdout); g_string_free(j, TRUE); return 0;
}
static int keyring_forget(const char *id)
{
	if (!id) return 2;
	if (!strncmp(id, "app:", 4)) {
		if (!app_id_ok(id + 4)) { fprintf(stderr, "not an app\n"); return 2; }
		char *d = secrets_dir(); if (!d) return 1; char *f = g_build_filename(d, id + 4, NULL); g_free(d);
		int r = unlink(f); g_free(f); if (r && errno != ENOENT) { perror("unlink"); return 1; } puts("{\"ok\":true}"); return 0;
	}
	/* an item of the account's keyring: /org/freedesktop/secrets/collection/NAME/ITEM */
	const char *pre = "/org/freedesktop/secrets/collection/";
	if (strncmp(id, pre, strlen(pre)) || !g_variant_is_object_path(id) || strstr(id, "/../")) { fprintf(stderr, "not an item\n"); return 2; }
	const char *rest = id + strlen(pre), *sl = strchr(rest, '/'); if (!sl || !sl[1] || strchr(sl + 1, '/')) { fprintf(stderr, "not an item\n"); return 2; }
	GDBusConnection *b = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL); if (!b) return 1;
	GError *e = NULL; GVariant *r = g_dbus_connection_call_sync(b, SS, id, "org.freedesktop.Secret.Item", "Delete", NULL, G_VARIANT_TYPE("(o)"), G_DBUS_CALL_FLAGS_NO_AUTO_START, 5000, NULL, &e);
	if (!r) { fprintf(stderr, "%s\n", e ? e->message : "failed"); return 1; }
	const char *pr; g_variant_get(r, "(&o)", &pr); int needs = strcmp(pr, "/"); g_variant_unref(r);
	if (needs) { fprintf(stderr, "the keyring asks for a prompt to remove it\n"); return 1; }
	puts("{\"ok\":true}"); return 0;
}

/* ---------------- 1.6.0 (docs/plan-1.6.0.md Part 14, third step): the keyring's prompts. gnome-keyring-daemon asks
   gcr's prompter (org.gnome.keyring.SystemPrompter on the session bus) when an app creates a keyring or needs one
   unlocked; Rudolf OS has no gcr-prompter (a GNOME window), so this portal answers in its place, with gcr's protocol
   (org.gnome.keyring.internal.Prompter, the daemon's org.gnome.keyring.internal.Prompter.Callback) and its secret
   exchange, and the desktop draws the question (a request file, as the file dialogs; the answer through rudolfd). */

/* gcr's secret exchange "sx-aes-1" (gcr-secret-exchange.c): Diffie-Hellman in IKE's 1536-bit group (RFC 3526, base 2),
   the shared value (unsigned, big-endian, no leading zeros) through HKDF-SHA256 (no salt: 32 zero bytes; no info) to a
   16-byte key, the secret in AES-128-CBC with PKCS#7 padding and a random 16-byte IV; each side's message a key file
   "[sx-aes-1]" with public= (and secret=, iv=) in base64. */
#define SX_P "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7EDEE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3DC2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F83655D23DCA3AD961C62F356208552BB9ED529077096966D670C354E4ABC9804F1746C08CA237327FFFFFFFFFFFFFFFF"
#define SX_G "sx-aes-1"
typedef struct { BIGNUM *priv; unsigned char pub[192]; int npub, generated, derived; unsigned char key[16]; } sx;
static BIGNUM *sx_prime(void) { static BIGNUM *p; if (!p) BN_hex2bn(&p, SX_P); return p; }
static void sx_clear(sx *x) { BN_clear_free(x->priv); x->priv = NULL; x->npub = x->generated = x->derived = 0; OPENSSL_cleanse(x->key, sizeof x->key); }
static int sx_generate(sx *x)
{
	BIGNUM *p = sx_prime(), *g = BN_new(), *pub = BN_new(); BN_CTX *c = BN_CTX_new(); int ok = 0;
	BN_clear_free(x->priv); x->priv = BN_secure_new(); x->derived = 0;
	if (!p || !g || !pub || !c || !x->priv || !BN_set_word(g, 2)) goto out;
	/* as egg_dh_gen_pair: a random number of the prime's bits, not zero, below 2^(bits-1) */
	do { if (!BN_priv_rand(x->priv, 1536, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY)) goto out; BN_clear_bit(x->priv, 1535); } while (BN_is_zero(x->priv));
	BN_set_flags(x->priv, BN_FLG_CONSTTIME);
	if (!BN_mod_exp(pub, g, x->priv, p, c)) goto out;
	x->npub = BN_bn2bin(pub, x->pub); x->generated = ok = x->npub > 0;
out:	BN_free(g); BN_free(pub); BN_CTX_free(c); return ok;
}
static int sx_derive(sx *x, const unsigned char *peer, size_t n)
{
	BIGNUM *p = sx_prime(), *q = BN_bin2bn(peer, (int)n, NULL), *k = BN_secure_new(), *lim = BN_dup(p); BN_CTX *c = BN_CTX_new(); int ok = 0;
	unsigned char ikm[192], zero[32] = {0}; int ni = 0; size_t kl = sizeof x->key; EVP_PKEY_CTX *h = NULL;
	if (!q || !k || !lim || !c || !x->priv || !BN_sub_word(lim, 1)) goto out;
	if (BN_cmp(q, BN_value_one()) <= 0 || BN_cmp(q, lim) >= 0) goto out;          /* 1 < peer < p-1 */
	if (!BN_mod_exp(k, q, x->priv, p, c) || (ni = BN_bn2bin(k, ikm)) <= 0) goto out;
	h = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);
	ok = h && EVP_PKEY_derive_init(h) > 0 && EVP_PKEY_CTX_set_hkdf_md(h, EVP_sha256()) > 0 && EVP_PKEY_CTX_set1_hkdf_salt(h, zero, sizeof zero) > 0 &&
		EVP_PKEY_CTX_set1_hkdf_key(h, ikm, ni) > 0 && EVP_PKEY_derive(h, x->key, &kl) > 0 && kl == sizeof x->key;
	x->derived = ok;
out:	OPENSSL_cleanse(ikm, sizeof ikm); EVP_PKEY_CTX_free(h); BN_free(q); BN_clear_free(k); BN_free(lim); BN_CTX_free(c); return ok;
}
static char *sx_out(GKeyFile *k) { char *r = g_key_file_to_data(k, NULL, NULL); g_strchug(r); g_key_file_free(k); return r; }
static void sx_set64(GKeyFile *k, const char *f, const unsigned char *d, size_t n) { char *b = g_base64_encode(d, n); g_key_file_set_value(k, SX_G, f, b); g_free(b); }
static unsigned char *sx_get64(GKeyFile *k, const char *f, gsize *n) { char *v = g_key_file_get_value(k, SX_G, f, NULL); unsigned char *r = v ? g_base64_decode(v, n) : NULL; g_free(v); return r; }
/* gcr_secret_exchange_begin: a new key pair, our public key */
static char *sx_begin(sx *x)
{
	sx_clear(x); if (!sx_generate(x)) return NULL;
	GKeyFile *k = g_key_file_new(); sx_set64(k, "public", x->pub, x->npub); return sx_out(k);
}
/* gcr_secret_exchange_receive: the other side's public key (the transport key derived once), and its secret if it sent one
   (*secret, NUL-terminated, to be wiped by the caller) */
static int sx_receive(sx *x, const char *text, char **secret, gsize *ns)
{
	if (secret) *secret = NULL;
	GKeyFile *k = g_key_file_new(); int ok = 0; unsigned char *peer = NULL, *iv = NULL, *ct = NULL; gsize np = 0, niv = 0, nct = 0;
	if (!text || !g_key_file_load_from_data(k, text, strlen(text), G_KEY_FILE_NONE, NULL)) goto out;
	if (!x->generated && !sx_generate(x)) goto out;
	if (!x->derived) { if (!(peer = sx_get64(k, "public", &np)) || !np || np > 192 || !sx_derive(x, peer, np)) goto out; }
	ok = 1;
	if (g_key_file_has_key(k, SX_G, "secret", NULL)) {
		ok = 0; iv = sx_get64(k, "iv", &niv); ct = sx_get64(k, "secret", &nct);
		if (!iv || niv != 16 || !ct || !nct || nct % 16) goto out;
		EVP_CIPHER_CTX *c = EVP_CIPHER_CTX_new(); char *pl = g_malloc0(nct + 16); int a = 0, b = 0;
		if (c && EVP_DecryptInit_ex(c, EVP_aes_128_cbc(), NULL, x->key, iv) > 0 && EVP_DecryptUpdate(c, (unsigned char *)pl, &a, ct, (int)nct) > 0 && EVP_DecryptFinal_ex(c, (unsigned char *)pl + a, &b) > 0) {
			pl[a + b] = 0; if (secret) { *secret = pl; pl = NULL; } if (ns) *ns = (gsize)(a + b); ok = 1;
		}
		if (pl) { OPENSSL_cleanse(pl, nct + 16); g_free(pl); }
		EVP_CIPHER_CTX_free(c);
	}
out:	g_free(peer); g_free(iv); if (ct) { OPENSSL_cleanse(ct, nct); g_free(ct); } g_key_file_free(k); return ok;
}
/* gcr_secret_exchange_send: our public key, and the secret encrypted (none: only the key, as for a confirmation) */
static char *sx_send(sx *x, const char *secret, size_t n)
{
	if (!x->derived) return NULL;
	GKeyFile *k = g_key_file_new(); sx_set64(k, "public", x->pub, x->npub);
	if (secret) {
		unsigned char iv[16], *ct = g_malloc(n + 16); int a = 0, b = 0, ok = 0; EVP_CIPHER_CTX *c = EVP_CIPHER_CTX_new();
		if (c && RAND_bytes(iv, sizeof iv) == 1 && EVP_EncryptInit_ex(c, EVP_aes_128_cbc(), NULL, x->key, iv) > 0 &&
		    EVP_EncryptUpdate(c, ct, &a, (const unsigned char *)secret, (int)n) > 0 && EVP_EncryptFinal_ex(c, ct + a, &b) > 0) ok = 1;
		EVP_CIPHER_CTX_free(c);
		if (ok) { sx_set64(k, "secret", ct, (size_t)(a + b)); sx_set64(k, "iv", iv, sizeof iv); }
		g_free(ct); if (!ok) { g_key_file_free(k); return NULL; }
	}
	return sx_out(k);
}

/* who asked: an app calling a Secret Service prompt's Prompt() (libsecret does, after CreateCollection or Unlock), seen
   by a second connection that only watches (the bus's monitoring, the same user's own bus); the daemon's
   BeginPrompting follows it. The app's Flatpak ID, else its program's name. */
static char *LAST_APP, *LAST_PROC; static gint64 LAST_T;
static void app_of_pid(guint32 pid, char **app, char **proc)
{
	char path[64], *text = NULL; *app = NULL; *proc = NULL;
	snprintf(path, sizeof path, "/proc/%u/root/.flatpak-info", pid);
	if (g_file_get_contents(path, &text, NULL, NULL)) { GKeyFile *k = g_key_file_new(); if (g_key_file_load_from_data(k, text, -1, 0, NULL)) *app = g_key_file_get_string(k, "Application", "name", NULL); g_key_file_free(k); g_free(text); }
	snprintf(path, sizeof path, "/proc/%u/comm", pid);
	if (g_file_get_contents(path, &text, NULL, NULL)) { g_strstrip(text); *proc = text; }
}
static gboolean mon_seen(gpointer u)
{
	char *sender = u;
	GVariant *r = g_dbus_connection_call_sync(BUS, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetConnectionUnixProcessID",
		g_variant_new("(s)", sender), G_VARIANT_TYPE("(u)"), 0, 2000, NULL, NULL);
	if (r) { guint32 pid; g_variant_get(r, "(u)", &pid); g_variant_unref(r); g_free(LAST_APP); g_free(LAST_PROC); app_of_pid(pid, &LAST_APP, &LAST_PROC); LAST_T = g_get_monotonic_time(); }
	g_free(sender); return FALSE;
}
static GDBusMessage *mon_filter(GDBusConnection *c, GDBusMessage *m, gboolean incoming, gpointer u)
{
	(void)c; (void)u;
	if (!incoming || g_dbus_message_get_message_type(m) != G_DBUS_MESSAGE_TYPE_METHOD_CALL) return m;
	if (!g_strcmp0(g_dbus_message_get_interface(m), "org.freedesktop.Secret.Prompt") && !g_strcmp0(g_dbus_message_get_member(m), "Prompt") && g_dbus_message_get_sender(m))
		g_idle_add(mon_seen, g_strdup(g_dbus_message_get_sender(m)));
	g_object_unref(m); return NULL;   /* (a monitor never answers what it sees) */
}
static void mon_start(void)
{
	char *a = g_dbus_address_get_for_bus_sync(G_BUS_TYPE_SESSION, NULL, NULL); if (!a) return;
	GDBusConnection *m = g_dbus_connection_new_for_address_sync(a, G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION, NULL, NULL, NULL); g_free(a);
	if (!m) return;
	g_dbus_connection_add_filter(m, mon_filter, NULL, NULL);
	const char *rules[] = {"type='method_call',interface='org.freedesktop.Secret.Prompt',member='Prompt'", NULL};
	GError *e = NULL; GVariant *r = g_dbus_connection_call_sync(m, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus.Monitoring", "BecomeMonitor",
		g_variant_new("(^asu)", rules, 0), NULL, 0, 3000, NULL, &e);
	if (r) g_variant_unref(r); else { fprintf(stderr, "rudolf-portal: the keyring's prompts will not name the app (%s)\n", e ? e->message : "?"); g_clear_error(&e); g_object_unref(m); }
}

/* the prompter: one prompt shown at a time (as gcr-prompter), the others wait their turn */
#define KP_IFACE "org.gnome.keyring.internal.Prompter"
#define KP_CB "org.gnome.keyring.internal.Prompter.Callback"
typedef struct { char *name, *path, *rid, *app, *proc, *type; guint watch; int refs, dead, ready, received; GVariantDict *props; sx x; } kprompt;
static GList *KP_ALL, *KP_WAIT; static kprompt *KP_ACTIVE;
static void kp_unref(kprompt *k)
{
	if (--k->refs) return;
	g_free(k->name); g_free(k->path); g_free(k->rid); g_free(k->app); g_free(k->proc); g_free(k->type);
	if (k->props) g_variant_dict_unref(k->props);
	sx_clear(&k->x); g_free(k);
}
static kprompt *kp_find(const char *name, const char *path)
{
	for (GList *l = KP_ALL; l; l = l->next) { kprompt *k = l->data; if (!strcmp(k->name, name) && !strcmp(k->path, path)) return k; }
	return NULL;
}
static void kp_next(void);
static int kp_b(kprompt *k, const char *key);
static void kp_unask(kprompt *k)
{
	if (!k->rid) return;
	char f[256]; snprintf(f, sizeof f, DIR_ "/req/%s.json", k->rid); unlink(f); snprintf(f, sizeof f, DIR_ "/resp/%s.json", k->rid); unlink(f);
	g_free(k->rid); k->rid = NULL;
}
static void kp_stop(kprompt *k, int done)
{
	if (k->dead) return;
	k->dead = 1; KP_ALL = g_list_remove(KP_ALL, k); KP_WAIT = g_list_remove(KP_WAIT, k); if (KP_ACTIVE == k) KP_ACTIVE = NULL;
	kp_unask(k);   /* (the desktop closes the question when its request is gone) */
	if (k->watch) g_bus_unwatch_name(k->watch);
	if (done) g_dbus_connection_call(BUS, k->name, k->path, KP_CB, "PromptDone", g_variant_new("()"), G_VARIANT_TYPE("()"), G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL, NULL, NULL);
	kp_unref(k);
}
static void kp_ready_done(GObject *s, GAsyncResult *res, gpointer u)
{
	kprompt *k = u; GError *e = NULL; GVariant *r = g_dbus_connection_call_finish(G_DBUS_CONNECTION(s), res, &e);
	if (r) g_variant_unref(r);
	if (!k->dead) { k->ready = 1; if (e) { fprintf(stderr, "rudolf-portal: the keyring's prompt: %s\n", e->message); kp_stop(k, 0); kp_next(); } }
	g_clear_error(&e); kp_unref(k);
}
/* PromptReady(reply, properties, exchange): "" when first ready, then "yes" or "no" with the password (sent encrypted) */
static void kp_send_ready(kprompt *k, const char *reply, const char *secret)
{
	char *sent = k->received ? sx_send(&k->x, secret, secret ? strlen(secret) : 0) : sx_begin(&k->x);
	if (!sent) { fprintf(stderr, "rudolf-portal: the keyring's prompt: the secret exchange failed\n"); kp_stop(k, 1); kp_next(); return; }
	/* (the properties sent back each as a variant in the variant, as gcr's prompter sends them: its callback unwraps them) */
	GVariantBuilder b; g_variant_builder_init(&b, G_VARIANT_TYPE("a{sv}"));
	const char *cl = NULL; gboolean ch = FALSE;
	if (k->props && g_variant_dict_lookup(k->props, "choice-label", "&s", &cl) && cl && *cl && g_variant_dict_lookup(k->props, "choice-chosen", "b", &ch))
		g_variant_builder_add(&b, "{sv}", "choice-chosen", g_variant_new_variant(g_variant_new_boolean(ch)));
	/* (a new password's strength, as gcr's dialog sets it: the daemon asks "Store passwords unencrypted?" only at 0, empty) */
	if (secret && kp_b(k, "password-new")) g_variant_builder_add(&b, "{sv}", "password-strength", g_variant_new_variant(g_variant_new_int32(*secret ? 1 : 0)));
	k->ready = 0; k->refs++;
	g_dbus_connection_call(BUS, k->name, k->path, KP_CB, "PromptReady", g_variant_new("(sa{sv}s)", reply, &b, sent), G_VARIANT_TYPE("()"),
		G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL, kp_ready_done, k);
	g_free(sent);
}
static void kp_next(void)
{
	if (KP_ACTIVE || !KP_WAIT) return;
	KP_ACTIVE = KP_WAIT->data; KP_WAIT = g_list_delete_link(KP_WAIT, KP_WAIT);
	kp_send_ready(KP_ACTIVE, "", NULL);
}
static void kp_vanished(GDBusConnection *c, const char *name, gpointer u)
{
	(void)c; (void)u;
	for (GList *l = KP_ALL; l;) { kprompt *k = l->data; l = l->next; if (!strcmp(k->name, name)) kp_stop(k, 0); }
	kp_next();
}
static const char *kp_s(kprompt *k, const char *key) { const char *v = NULL; if (k->props) g_variant_dict_lookup(k->props, key, "&s", &v); return v ? v : ""; }
static int kp_b(kprompt *k, const char *key) { gboolean v = FALSE; if (k->props) g_variant_dict_lookup(k->props, key, "b", &v); return v; }
/* the question for the desktop: what the daemon says (title, message, description, a warning such as a wrong password),
   whether a new password is chosen (asked twice), the choice (unlock it whenever I sign in), and who asked */
static void kp_ask(kprompt *k)
{
	g_free(k->rid); k->rid = g_strdup_printf("k%ld-%u", (long)getpid(), ++SEQ);
	GString *j = g_string_new("{\"app\":"); jstr(j, k->app); g_string_append(j, ",\"proc\":"); jstr(j, k->proc);
	g_string_append(j, ",\"kind\":\"keyring\",\"type\":"); jstr(j, k->type);
	const char *f[][2] = {{"title", "title"}, {"message", "message"}, {"description", "description"}, {"warning", "warning"}, {"choice-label", "choice"}, {"continue-label", "cont"}, {"cancel-label", "cancel"}};
	for (size_t i = 0; i < G_N_ELEMENTS(f); i++) { g_string_append_printf(j, ",\"%s\":", f[i][1]); jstr(j, kp_s(k, f[i][0])); }
	g_string_append_printf(j, ",\"newpass\":%s,\"chosen\":%s}", kp_b(k, "password-new") ? "true" : "false", kp_b(k, "choice-chosen") ? "true" : "false");
	write_request(k->rid, j); g_string_free(j, TRUE);
}
/* a JSON string field of the desktop's answer ({"response":0,"password":"…","chosen":true}), \u escapes with surrogate pairs */
static char *json_field(const char *t, const char *key)
{
	char pat[40]; snprintf(pat, sizeof pat, "\"%s\"", key); const char *p = strstr(t, pat); if (!p) return NULL;
	p += strlen(pat); while (*p == ' ' || *p == ':') p++; if (*p != '"') return NULL;
	GString *v = g_string_new(NULL); p++;
	while (*p && *p != '"') {
		if (*p == '\\' && p[1]) { p++;
			if (*p == 'u' && g_ascii_isxdigit(p[1]) && g_ascii_isxdigit(p[2]) && g_ascii_isxdigit(p[3]) && g_ascii_isxdigit(p[4])) {
				unsigned c = (unsigned)strtoul((char[]){p[1], p[2], p[3], p[4], 0}, NULL, 16); p += 4;
				if (c >= 0xD800 && c < 0xDC00 && p[1] == '\\' && p[2] == 'u') { unsigned d = (unsigned)strtoul((char[]){p[3], p[4], p[5], p[6], 0}, NULL, 16); if (d >= 0xDC00 && d < 0xE000) { c = 0x10000 + ((c - 0xD800) << 10) + (d - 0xDC00); p += 6; } }
				g_string_append_unichar(v, c);
			} else g_string_append_c(v, *p == 'n' ? '\n' : *p == 't' ? '\t' : *p);
		} else g_string_append_c(v, *p);
		p++;
	}
	char *r = g_strndup(v->str, v->len); OPENSSL_cleanse(v->str, v->allocated_len); g_string_free(v, TRUE); return r;
}
/* the desktop answered (rudolfd writes it, 0600, as the prompt's user): yes with the password, or no */
static void kp_check(void)
{
	kprompt *k = KP_ACTIVE; if (!k || !k->rid || k->ready) return;
	char f[256], *t = NULL; gsize n = 0; snprintf(f, sizeof f, DIR_ "/resp/%s.json", k->rid);
	if (!g_file_get_contents(f, &t, &n, NULL)) return;
	unlink(f); g_free(k->rid); k->rid = NULL;
	guint32 code = 2; const char *r = strstr(t, "\"response\""); if (r && (r = strchr(r, ':'))) code = (guint32)strtoul(r + 1, NULL, 10);
	int yes = code == 0, pw = !strcmp(k->type, "password");
	char *secret = yes && pw ? json_field(t, "password") : NULL; if (yes && pw && !secret) secret = g_strdup("");
	const char *ch = strstr(t, "\"chosen\""); if (ch && (ch = strchr(ch, ':'))) { while (*++ch == ' ') {} g_variant_dict_insert(k->props, "choice-chosen", "b", !strncmp(ch, "true", 4)); }
	OPENSSL_cleanse(t, n); g_free(t);
	kp_send_ready(k, yes ? "yes" : "no", secret);
	if (secret) { OPENSSL_cleanse(secret, strlen(secret)); g_free(secret); }
}
static void kp_method(GDBusConnection *c, const char *sender, const char *path, const char *iface, const char *method, GVariant *p, GDBusMethodInvocation *inv, gpointer u)
{
	(void)c; (void)path; (void)iface; (void)u;
	if (!strcmp(method, "BeginPrompting")) {
		const char *cp; g_variant_get(p, "(&o)", &cp);
		if (kp_find(sender, cp)) { g_dbus_method_invocation_return_dbus_error(inv, "org.freedesktop.DBus.Error.Failed", "Already begun prompting for this prompt callback"); return; }
		kprompt *k = g_new0(kprompt, 1); k->refs = 1; k->name = g_strdup(sender); k->path = g_strdup(cp); k->props = g_variant_dict_new(NULL);
		k->watch = g_bus_watch_name_on_connection(BUS, sender, G_BUS_NAME_WATCHER_FLAGS_NONE, NULL, kp_vanished, NULL, NULL);
		KP_ALL = g_list_append(KP_ALL, k); KP_WAIT = g_list_append(KP_WAIT, k);
		g_dbus_method_invocation_return_value(inv, g_variant_new("()"));
		kp_next();
	} else if (!strcmp(method, "PerformPrompt")) {
		const char *cp, *type, *ex; GVariantIter *it; g_variant_get(p, "(&o&sa{sv}&s)", &cp, &type, &it, &ex);
		kprompt *k = kp_find(sender, cp); const char *err = !k || k != KP_ACTIVE ? "Not begun prompting for this prompt callback" : !k->ready ? "Already performing a prompt for this prompt callback" : NULL;
		if (err) { g_variant_iter_free(it); g_dbus_method_invocation_return_dbus_error(inv, "org.freedesktop.DBus.Error.Failed", err); return; }
		const char *key; GVariant *v; while (g_variant_iter_loop(it, "{&sv}", &key, &v)) g_variant_dict_insert_value(k->props, key, v);
		g_variant_iter_free(it);
		if (!sx_receive(&k->x, ex, NULL, NULL)) { g_dbus_method_invocation_return_dbus_error(inv, "org.freedesktop.DBus.Error.InvalidArgs", "Invalid secret exchange received"); return; }
		k->received = 1;
		if (strcmp(type, "password") && strcmp(type, "confirm")) { g_dbus_method_invocation_return_dbus_error(inv, "org.freedesktop.DBus.Error.InvalidArgs", "Invalid type argument"); return; }
		g_free(k->type); k->type = g_strdup(type);
		if (!k->app && !k->proc && LAST_T && g_get_monotonic_time() - LAST_T < 30 * G_USEC_PER_SEC) { k->app = g_strdup(LAST_APP); k->proc = g_strdup(LAST_PROC); }
		k->ready = 0; kp_ask(k);
		g_dbus_method_invocation_return_value(inv, g_variant_new("()"));
	} else if (!strcmp(method, "StopPrompting")) {
		const char *cp; g_variant_get(p, "(&o)", &cp);
		kprompt *k = kp_find(sender, cp); if (k) kp_stop(k, 1);
		g_dbus_method_invocation_return_value(inv, g_variant_new("()"));
		kp_next();
	} else g_dbus_method_invocation_return_dbus_error(inv, "org.freedesktop.DBus.Error.UnknownMethod", "not supported");
}
static const GDBusInterfaceVTable KP_VT = {kp_method, NULL, NULL, {0}};
static const char KP_XML[] = "<node><interface name='" KP_IFACE "'>"
	"<method name='BeginPrompting'><arg type='o' direction='in'/></method>"
	"<method name='PerformPrompt'><arg type='o' direction='in'/><arg type='s' direction='in'/><arg type='a{sv}' direction='in'/><arg type='s' direction='in'/></method>"
	"<method name='StopPrompting'><arg type='o' direction='in'/></method>"
	"</interface></node>";
/* (a check of the secret exchange for the tests, against another implementation: our key, the peer's message received
   (its secret printed in base64), a secret sent; lines with '|' for the key files' line ends) */
static int sx_test(void)
{
	sx x = {0}; char line[8192], *s = sx_begin(&x); if (!s) return 1;
	for (char *c = s; *c; c++) if (*c == '\n') *c = '|';
	printf("%s\n", s); fflush(stdout); g_free(s);
	while (fgets(line, sizeof line, stdin)) {
		g_strchomp(line); char *sp = strchr(line, ' '); if (sp) *sp++ = 0; else sp = line + strlen(line);
		for (char *c = sp; *c; c++) if (*c == '|') *c = '\n';
		if (!strcmp(line, "recv")) { char *sec = NULL; gsize n = 0; if (!sx_receive(&x, sp, &sec, &n)) puts("invalid"); else if (!sec) puts("nosecret"); else { char *b = g_base64_encode((guchar *)sec, n); printf("secret %s\n", b); g_free(b); g_free(sec); } }
		else if (!strcmp(line, "send")) { gsize n = 0; guchar *d = g_base64_decode(sp, &n); char *z = g_strndup((char *)d, n); char *o = sx_send(&x, *sp == '-' ? NULL : z, n); g_free(d); g_free(z);
			if (!o) puts("invalid"); else { for (char *c = o; *c; c++) if (*c == '\n') *c = '|'; printf("%s\n", o); g_free(o); } }
		fflush(stdout);
	}
	sx_clear(&x); return 0;
}

int main(int argc, char **argv)
{
	if (argc > 1 && !strcmp(argv[1], "--keyring-list")) return keyring_list();
	if (argc > 2 && !strcmp(argv[1], "--keyring-forget")) return keyring_forget(argv[2]);
	if (argc > 1 && !strcmp(argv[1], "--sx-test")) return sx_test();
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
	/* 1.6.0: the keyring's prompter, in gcr-prompter's place */
	GDBusNodeInfo *kinfo = g_dbus_node_info_new_for_xml(KP_XML, NULL);
	g_dbus_connection_register_object(BUS, "/org/gnome/keyring/Prompter", kinfo->interfaces[0], &KP_VT, NULL, NULL, NULL);
	g_bus_own_name_on_connection(BUS, "org.gnome.keyring.SystemPrompter", G_BUS_NAME_OWNER_FLAGS_REPLACE, NULL, NULL, NULL, NULL);
	mon_start();
	g_timeout_add(200, check_answers, NULL);
	fprintf(stderr, "rudolf-portal: ready\n");
	g_main_loop_run(g_main_loop_new(NULL, FALSE));
	return 0;
}
