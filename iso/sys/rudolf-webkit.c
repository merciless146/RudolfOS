/*
 * rudolf-webkit - the Rudolf OS desktop in WebKit (WebKitGTK 4.1) instead of Chromium (1.5.7, plan Part 4).
 *
 * System Settings › Desktop engine › Lite: the session starts this instead of Chromium's kiosk, as the desktop user,
 * in the desktop's memory group:   rudolf-webkit --rudolf-kiosk --size=WxH http://127.0.0.1:8080/
 * One window without a frame, class RudolfDesktop (rudolf-display keeps it exactly full screen, as Chromium's), the
 * page's own data in ~/.rudolf-webkit (at start the desktop loads rudolfd's mirror of its databases, as the shell
 * does). The page sees RUDOLFD.engine == "webkit" and sends websites to Chromium's own window; a website the page
 * would load as the whole desktop is refused here too. A page that stopped (its process ended) is loaded again.
 * The camera, the microphone and notifications are allowed: the desktop asks the person itself.
 * WebKitGTK and what it needs (its helper processes, GStreamer's basic plug-ins, GLib's TLS module) are in the base
 * layer (systools/addwebkit.sh). Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *url = "http://127.0.0.1:8080/";

static int local_or_not_web(const char *u)
{
	if (strncmp(u, "http:", 5) && strncmp(u, "https:", 6)) return 1;   /* data:, blob:, about:, the page's own frames */
	return !strncmp(u, "http://127.0.0.1:", 17) || !strncmp(u, "http://localhost:", 17);
}
static gboolean on_permission(WebKitWebView *v, WebKitPermissionRequest *req, gpointer d)
{
	(void)v; (void)d; webkit_permission_request_allow(req); return TRUE;
}
static gboolean on_decide(WebKitWebView *v, WebKitPolicyDecision *dec, WebKitPolicyDecisionType type, gpointer d)
{
	(void)v; (void)d;
	if (type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) return FALSE;   /* the page's frames: as they like */
	WebKitNavigationAction *a = webkit_navigation_policy_decision_get_navigation_action(WEBKIT_NAVIGATION_POLICY_DECISION(dec));
	const char *u = webkit_uri_request_get_uri(webkit_navigation_action_get_request(a));
	if (u && !local_or_not_web(u)) { fprintf(stderr, "rudolf-webkit: not leaving the desktop for %s\n", u); webkit_policy_decision_ignore(dec); return TRUE; }
	return FALSE;
}
static gboolean reload_later(gpointer v) { webkit_web_view_load_uri(WEBKIT_WEB_VIEW(v), url); return G_SOURCE_REMOVE; }
static void on_terminated(WebKitWebView *v, WebKitWebProcessTerminationReason r, gpointer d)
{
	(void)d; fprintf(stderr, "rudolf-webkit: the page stopped (%d), loading it again\n", (int)r);
	g_timeout_add_seconds(1, reload_later, v);
}
static GtkWidget *on_create(WebKitWebView *v, WebKitNavigationAction *a, gpointer d)
{
	(void)v; (void)a; (void)d; return NULL;   /* new windows: the page sends websites to Chromium's window */
}

int main(int argc, char **argv)
{
	int w = 0, h = 0;
	for (int i = 1; i < argc; i++) {
		if (!strncmp(argv[i], "--size=", 7)) { if (sscanf(argv[i] + 7, "%dx%d", &w, &h) != 2) w = h = 0; }
		else if (!strncmp(argv[i], "http://127.0.0.1:", 17)) url = argv[i];
	}
	g_set_prgname("RudolfDesktop"); gdk_set_program_class("RudolfDesktop");
	gtk_init(&argc, &argv);
	char *home = g_build_filename(g_get_home_dir(), ".rudolf-webkit", NULL);
	char *data = g_build_filename(home, "data", NULL), *cache = g_build_filename(home, "cache", NULL);
	WebKitWebsiteDataManager *mgr = webkit_website_data_manager_new("base-data-directory", data, "base-cache-directory", cache, NULL);
	WebKitWebContext *ctx = webkit_web_context_new_with_website_data_manager(mgr);
	webkit_web_context_set_cache_model(ctx, WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER);   /* the least for caches: the page is local */
	GtkWidget *view = webkit_web_view_new_with_context(ctx);
	WebKitSettings *s = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(view));
	g_object_set(s, "enable-javascript", TRUE, "javascript-can-access-clipboard", TRUE, "enable-webaudio", TRUE,
		"enable-media-stream", TRUE, "enable-webgl", TRUE, "enable-developer-extras", FALSE,
		"media-playback-requires-user-gesture", FALSE, "allow-file-access-from-file-urls", FALSE,
		"enable-write-console-messages-to-stdout", TRUE, NULL);
	webkit_settings_set_hardware_acceleration_policy(s, WEBKIT_HARDWARE_ACCELERATION_POLICY_ON_DEMAND);
	g_signal_connect(view, "permission-request", G_CALLBACK(on_permission), NULL);
	g_signal_connect(view, "decide-policy", G_CALLBACK(on_decide), NULL);
	g_signal_connect(view, "web-process-terminated", G_CALLBACK(on_terminated), NULL);
	g_signal_connect(view, "create", G_CALLBACK(on_create), NULL);
	GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_decorated(GTK_WINDOW(win), FALSE); gtk_window_set_title(GTK_WINDOW(win), "Rudolf OS");
	gtk_container_add(GTK_CONTAINER(win), view);
	if (w <= 0 || h <= 0) { GdkScreen *scr = gdk_screen_get_default(); w = gdk_screen_get_width(scr); h = gdk_screen_get_height(scr); }
	gtk_window_set_default_size(GTK_WINDOW(win), w, h); gtk_window_move(GTK_WINDOW(win), 0, 0); gtk_window_fullscreen(GTK_WINDOW(win));
	g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	webkit_web_view_load_uri(WEBKIT_WEB_VIEW(view), url);
	gtk_widget_show_all(win);
	gtk_main();
	return 0;
}
