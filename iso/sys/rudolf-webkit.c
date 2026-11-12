/*
 * rudolf-webkit - the Rudolf OS desktop in WebKit (WebKitGTK 4.1) instead of Chromium (1.5.7, plan Part 4).
 *
 * System Settings › Desktop engine › Lite: the session starts this instead of Chromium's kiosk, as the desktop user,
 * in the desktop's memory group:   rudolf-webkit --rudolf-kiosk --size=WxH [--draw=gpu|mixed|soft] http://127.0.0.1:8080/
 * One window without a frame, class RudolfDesktop (rudolf-display keeps it exactly full screen, as Chromium's), the
 * page's own data in ~/.rudolf-webkit (at start the desktop loads rudolfd's mirror of its databases, as the shell
 * does). A website the page would load as the whole desktop is refused. A page that stopped (its process ended) is
 * loaded again. The camera, the microphone and notifications are allowed for the desktop: it asks the person itself.
 *
 * 1.5.8 (plan Part 9): Oracle's tabs are real WebKit pages, as the shell (iso/shell/shell.js) makes them real Chromium
 * pages with the Standard engine, and with the same messages, so the desktop's side (tabs26.js) is the same:
 *   the page -> here   window.webkit.messageHandlers.rudolfShell.postMessage({rudolfShell: 'tab', op, tid, ...})
 *   here -> the page   window.__rwkIn({rudolfShell: 'hello' | 'tabev' | 'log' | 'cleared' | 'pong', ...})
 * Each tab is a web view laid over the desktop's (a GtkOverlay) exactly where Oracle draws the page, with the parts
 * the desktop covers (menus, other windows) cut out of it (the shape of its own window). Each Rudolf OS account has
 * its own cookies and site storage (~/.rudolf-webkit/web/<part>). A page's new window becomes a tab with its opener
 * kept (sign-in pop-ups). Alerts, confirms and prompts, and the camera, microphone and location, are asked by the
 * desktop. Downloads go where Chromium's go (/run/rudolf/dl/.incoming, filed by the desktop), as NAME.crdownload
 * until they are complete. Fullscreen video fills the screen. The firewall applies to every page and frame a tab
 * loads. Flash content gets Ruffle. Widevine sites stay Chromium's (the desktop sends them to its window).
 *
 * WebKitGTK and what it needs (its helper processes, GStreamer's basic plug-ins, GLib's TLS module) are in the base
 * layer (systools/addwebkit.sh). Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *url = "http://127.0.0.1:8080/";
static GtkWidget *win, *overlay, *desk;
static int desk_ready;
/* 1.5.8: how the desktop draws, from Settings › Display › Graphics card (the session passes --draw=):
     gpu    the graphics card draws everything ("Use it for games and the desktop")
     mixed  the processor draws the contents, on several cores, and the graphics card puts the layers together and
            moves them ("Use it for games and Linux apps", the default). On the Vivobook, gpu took some 400 MB more at
            rest (graphics memory lent from RAM, still growing after 15 minutes); software alone stuttered.
     soft   the processor does both, with lighter effects (no shadows or blurs) to keep it smooth (no graphics card) */
enum { DRAW_GPU, DRAW_MIXED, DRAW_SOFT };
static int DRAW = DRAW_GPU;
static WebKitHardwareAccelerationPolicy HWPOL = WEBKIT_HARDWARE_ACCELERATION_POLICY_ON_DEMAND;
static const char *LIGHT_CSS = "*,*::before,*::after{box-shadow:none!important;text-shadow:none!important;"
	"backdrop-filter:none!important;-webkit-backdrop-filter:none!important}";
static guint hello_timer;

/* ------------------------------------------------------------------ small helpers */
static void json_str(GString *s, const char *v)
{
	g_string_append_c(s, '"');
	for (const unsigned char *c = (const unsigned char *)(v ? v : ""); *c; c++) {
		if (*c == '"' || *c == '\\') { g_string_append_c(s, '\\'); g_string_append_c(s, *c); }
		else if (*c == '\n') g_string_append(s, "\\n"); else if (*c == '\r') g_string_append(s, "\\r"); else if (*c == '\t') g_string_append(s, "\\t");
		else if (*c < 0x20) g_string_append_printf(s, "\\u%04x", *c);
		else if (*c == 0xe2 && c[1] == 0x80 && (c[2] == 0xa8 || c[2] == 0xa9)) { g_string_append_printf(s, "\\u%04x", c[2] == 0xa8 ? 0x2028 : 0x2029); c += 2; }
		else g_string_append_c(s, *c);
	}
	g_string_append_c(s, '"');
}
/* a message to the desktop page: window.__rwkIn(JSON) */
static void to_desk(GString *m)
{
	if (!desk_ready) { g_string_free(m, TRUE); return; }
	char *js = g_strdup_printf("window.__rwkIn&&window.__rwkIn(%s)", m->str);
	webkit_web_view_run_javascript(WEBKIT_WEB_VIEW(desk), js, NULL, NULL, NULL);
	g_free(js); g_string_free(m, TRUE);
}
static void note(const char *fmt, ...)
{
	va_list a; va_start(a, fmt); char *t = g_strdup_vprintf(fmt, a); va_end(a);
	fprintf(stderr, "rudolf-webkit: %s\n", t);
	GString *m = g_string_new("{\"rudolfShell\":\"log\",\"m\":"); json_str(m, t); g_string_append_c(m, '}'); to_desk(m); g_free(t);
}
static char *jstr(JSCValue *o, const char *k)
{
	JSCValue *v = jsc_value_object_get_property(o, k); char *r = NULL;
	if (v && jsc_value_is_string(v)) r = jsc_value_to_string(v);
	if (v) g_object_unref(v); return r;
}
static double jnum(JSCValue *o, const char *k, double d)
{
	JSCValue *v = jsc_value_object_get_property(o, k); double r = d;
	if (v && jsc_value_is_number(v)) r = jsc_value_to_double(v);
	if (v) g_object_unref(v); return r;
}
static int jbool(JSCValue *o, const char *k)
{
	JSCValue *v = jsc_value_object_get_property(o, k); int r = v && jsc_value_to_boolean(v);
	if (v) g_object_unref(v); return r;
}
static int http_url(const char *u) { return u && (!strncmp(u, "http://", 7) || !strncmp(u, "https://", 8)); }

/* ------------------------------------------------------------------ the firewall (the desktop sends its rules) */
static struct { int on; char mode[16]; char **block, **trust; } FW;
static char *host_of(const char *u)
{
	const char *p = strstr(u ? u : "", "://"); if (!p) return g_strdup(""); p += 3;
	const char *e = p + strcspn(p, "/?#"); const char *at = memchr(p, '@', e - p); if (at) p = at + 1;
	const char *c = p; if (*c == '[') { const char *b = memchr(c, ']', e - c); if (b) e = b + 1; } else { const char *col = memchr(c, ':', e - c); if (col) e = col; }
	return g_ascii_strdown(p, e - p);
}
static int host_in(const char *h, char **l)
{
	for (int i = 0; l && l[i]; i++) { size_t n = strlen(h), m = strlen(l[i]); if (!strcmp(h, l[i]) || (n > m && h[n - m - 1] == '.' && !strcmp(h + n - m, l[i]))) return 1; }
	return 0;
}
static const char *fw_why(const char *u)
{
	if (!FW.on || !u || (strncmp(u, "http", 4) && strncmp(u, "ws", 2))) return NULL;
	if (!strcmp(FW.mode, "lock")) return "All internet access is blocked by the firewall.";
	char *h = host_of(u); const char *r = NULL;
	if (host_in(h, FW.block)) r = "This site is on the blocked list.";
	else if (!strcmp(FW.mode, "trusted") && !host_in(h, FW.trust)) r = "This site is not on the trusted list.";
	g_free(h); return r;
}
static char **str_list(JSCValue *o, const char *k)
{
	JSCValue *a = jsc_value_object_get_property(o, k); GPtrArray *p = g_ptr_array_new();
	if (a && jsc_value_is_array(a)) for (guint i = 0; i < 2000; i++) {
		JSCValue *v = jsc_value_object_get_property_at_index(a, i); if (!v || jsc_value_is_undefined(v)) { if (v) g_object_unref(v); break; }
		char *s = jsc_value_to_string(v); g_ptr_array_add(p, g_ascii_strdown(s, -1)); g_free(s); g_object_unref(v);
	}
	if (a) g_object_unref(a); g_ptr_array_add(p, NULL); return (char **)g_ptr_array_free(p, FALSE);
}

/* ------------------------------------------------------------------ tabs */
typedef struct tab {
	char tid[64], part[48];
	GtkWidget *box, *view;           /* the view, in an event box with its own window (cut to the visible parts) */
	int x, y, w, h, z, shown, fs, still, stillUp;   /* still: covered, shown as a picture by the desktop (stillUp: the picture is there) */
	cairo_region_t *vis;             /* NULL: nothing covers it */
} tab;
static GHashTable *TABS;           /* tid -> tab */
static GHashTable *CTX;            /* part -> WebKitWebContext */
static GHashTable *DLG, *PERM;     /* id -> WebKitScriptDialog / WebKitPermissionRequest */
static int NEWN, DLGN, PERMN, DARK;
static char RUFFLE_JS[16384];
static const char *PAGE_JS =
	"(()=>{const say=o=>{try{window.webkit.messageHandlers.rudolfTab.postMessage(o)}catch(e){}};"
	"const SUB={F20:'w',F21:'t',F22:'n',F23:'q'};"
	"addEventListener('keydown',e=>{const c=e.ctrlKey||e.metaKey,k=SUB[e.code]&&e.ctrlKey?SUB[e.code]:e.key;let a='';"
	" if(c&&!e.altKey&&!e.shiftKey&&/^[twlrn]$/i.test(k))a='c'+k.toLowerCase();else if(c&&k=='q')a='none';else if(k=='F5')a='f5';"
	" else if(e.altKey&&!c&&(k=='ArrowLeft'||k=='ArrowRight'))a=k=='ArrowLeft'?'back':'fwd';else if(e.altKey&&!c&&(k=='F4'||e.code=='F20'))a='af4';"
	" else if(c&&!e.altKey&&(k=='+'||k=='='||k=='-'||k=='0'))a='z'+(k=='='?'+':k);else if(c&&k=='Tab')a=e.shiftKey?'ptab':'ntab';"
	" if(a){e.preventDefault();e.stopImmediatePropagation();if(a!='none')say({k:a})}},true);"
	"const S='embed[src*=\".swf\" i],embed[type*=\"shockwave\" i],object[data*=\".swf\" i],object[type*=\"shockwave\" i],object[classid*=\"d27cdb6e\" i],param[name=\"movie\" i][value*=\".swf\" i]';"
	"let done=false,mo=null;const go=()=>{if(done)return;done=true;if(mo)mo.disconnect();say({flash:location.href})};"
	"const watch=()=>{if(document.querySelector(S))return go();mo=new MutationObserver(()=>{if(document.querySelector(S))go()});"
	" mo.observe(document.documentElement,{childList:true,subtree:true,attributes:true,attributeFilter:['src','data','type','value']});setTimeout(()=>mo&&mo.disconnect(),60000)};"
	"if(document.documentElement)watch();else addEventListener('DOMContentLoaded',watch)})()";
static const char *DARK_ON = "(()=>{const id='rudolf-dark';let s=document.getElementById(id);if(!s){s=document.createElement('style');s.id=id;"
	"s.textContent='html{filter:invert(.9) hue-rotate(180deg)!important;background:#fff!important}img,video,picture,canvas,iframe,embed,object,svg image,[style*=\"background-image\"]{filter:invert(1) hue-rotate(180deg)!important}';"
	"(document.head||document.documentElement).appendChild(s)}try{const m=/rgba?\\(([^)]+)\\)/.exec(getComputedStyle(document.body||document.documentElement).backgroundColor||'');"
	"if(m){const v=m[1].split(',').map(Number);if(!(v.length>3&&v[3]<.5)&&(.2126*v[0]+.7152*v[1]+.0722*v[2])/255<.4)s.remove()}}catch(e){}})()";
static const char *DARK_OFF = "(()=>{const s=document.getElementById('rudolf-dark');if(s)s.remove()})()";

static void tabev(tab *t, const char *ev, const char *extra)
{
	GString *m = g_string_new("{\"rudolfShell\":\"tabev\",\"tid\":"); json_str(m, t->tid);
	g_string_append(m, ",\"ev\":"); json_str(m, ev);
	if (extra && *extra) { g_string_append_c(m, ','); g_string_append(m, extra); }
	g_string_append_c(m, '}'); to_desk(m);
}
static void nav_state(tab *t)
{
	WebKitWebView *v = WEBKIT_WEB_VIEW(t->view); GString *e = g_string_new("\"url\":");
	json_str(e, webkit_web_view_get_uri(v)); g_string_append_printf(e, ",\"back\":%s,\"fwd\":%s", webkit_web_view_can_go_back(v) ? "true" : "false", webkit_web_view_can_go_forward(v) ? "true" : "false");
	tabev(t, "nav", e->str); g_string_free(e, TRUE);
}
static void on_download(WebKitWebContext *c, WebKitDownload *dl, gpointer d);
static WebKitWebContext *ctx_for(const char *part)
{
	WebKitWebContext *c = g_hash_table_lookup(CTX, part); if (c) return c;
	char *base = g_build_filename(g_get_home_dir(), ".rudolf-webkit", "web", part, NULL), *data = g_build_filename(base, "data", NULL), *cache = g_build_filename(base, "cache", NULL);
	WebKitWebsiteDataManager *m = webkit_website_data_manager_new("base-data-directory", data, "base-cache-directory", cache, NULL);
	c = webkit_web_context_new_with_website_data_manager(m);
	webkit_web_context_set_cache_model(c, WEBKIT_CACHE_MODEL_WEB_BROWSER);
	webkit_cookie_manager_set_persistent_storage(webkit_web_context_get_cookie_manager(c), g_build_filename(data, "cookies.sqlite", NULL), WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE);
	g_signal_connect(c, "download-started", G_CALLBACK(on_download), NULL);
	g_hash_table_insert(CTX, g_strdup(part), c);
	g_free(base); g_free(data); g_free(cache); return c;
}
static void tab_free(gpointer p)
{
	tab *t = p; if (t->vis) cairo_region_destroy(t->vis);
	if (t->box) gtk_widget_destroy(t->box); g_free(t);
}
static void tabs_order(void)
{
	/* the overlay draws its children in order: lower z first */
	GList *l = g_hash_table_get_values(TABS); int n = g_list_length(l); tab **a = g_new(tab *, n); int i = 0;
	for (GList *x = l; x; x = x->next) a[i++] = x->data;
#define ZK(t) ((t)->fs ? 0x7fffffff : (t)->z)
	for (int p = 0; p < n; p++) for (int q = p + 1; q < n; q++) if (ZK(a[q]) < ZK(a[p])) { tab *s = a[p]; a[p] = a[q]; a[q] = s; }
	for (i = 0; i < n; i++) gtk_overlay_reorder_overlay(GTK_OVERLAY(overlay), a[i]->box, i);
	g_free(a); g_list_free(l);
}
/* what covers a tab (a menu, another window). WebKit's page cannot be cut on the screen in GTK 3: shaping its window,
   clipping its drawing, a native window, none of them showed the desktop through (the Lite engine check, in all three
   drawing modes), so the desktop's menus and windows went behind pages (the Vivobook, 2026-11-12). So, 1.5.8: while
   anything covers part of a tab, the tab is hidden and the desktop shows a still picture of it (a JPEG, sent with the
   'still' event) under its menus and windows; uncovered, the live tab comes back (cover_step). The clip and the input
   shape stay for the moment before the picture is up. */
static void shape(tab *t)
{
	GdkWindow *gw = gtk_widget_get_window(t->box); if (!gw || !gtk_widget_get_has_window(t->box)) return;
	gdk_window_input_shape_combine_region(gw, t->fs ? NULL : t->vis, 0, 0);
	gtk_widget_queue_draw(overlay);
}
static gboolean on_box_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)w; tab *t = d;
	if (t->vis && !t->fs) { gdk_cairo_region(cr, t->vis); cairo_clip(cr); }
	return FALSE;
}
/* out of the way: hidden, shrunk to a pixel in the corner and laid out again (a hide alone left the page's pixels on
   the screen: what was under it was not drawn again) */
static void step_aside(tab *t)
{
	gtk_widget_set_margin_start(t->box, 0); gtk_widget_set_margin_top(t->box, 0); gtk_widget_set_size_request(t->box, 1, 1);
	gtk_widget_hide(t->box); gtk_widget_queue_resize(overlay); gtk_widget_queue_draw(desk);
}
static void still_ready(GObject *o, GAsyncResult *res, gpointer d)
{
	char *tid = d; tab *t = g_hash_table_lookup(TABS, tid); g_free(tid);
	GError *err = NULL; cairo_surface_t *sf = webkit_web_view_get_snapshot_finish(WEBKIT_WEB_VIEW(o), res, &err);
	if (err) g_error_free(err);
	if (!t || !t->still) { if (sf) cairo_surface_destroy(sf); return; }
	GString *x = g_string_new("\"img\":");
	GdkPixbuf *pb = sf ? gdk_pixbuf_get_from_surface(sf, 0, 0, cairo_image_surface_get_width(sf), cairo_image_surface_get_height(sf)) : NULL;
	gchar *buf = NULL; gsize len = 0;
	if (pb && gdk_pixbuf_save_to_buffer(pb, &buf, &len, "jpeg", NULL, "quality", "82", NULL)) { gchar *b = g_base64_encode((guchar *)buf, len); g_string_append(x, "\"data:image/jpeg;base64,"); g_string_append(x, b); g_string_append_c(x, '"'); g_free(b); }
	else g_string_append(x, "null");
	g_free(buf); if (pb) g_object_unref(pb); if (sf) cairo_surface_destroy(sf);
	tabev(t, "still", x->str); g_string_free(x, TRUE);
	t->stillUp = 1; step_aside(t);   /* the picture is on its way: the live page steps aside */
	if (g_getenv("RUDOLF_WK_DEBUG")) g_printerr("still %s up, box visible=%d mapped=%d\n", t->tid, gtk_widget_get_visible(t->box), gtk_widget_get_mapped(t->box));
}
/* covered or not, after each place */
static void cover_step(tab *t)
{
	const int covered = t->shown && t->vis && !t->fs;
	if (covered && !t->still) { t->still = 1; t->stillUp = 0; webkit_web_view_get_snapshot(WEBKIT_WEB_VIEW(t->view), WEBKIT_SNAPSHOT_REGION_VISIBLE, WEBKIT_SNAPSHOT_OPTIONS_NONE, NULL, still_ready, g_strdup(t->tid)); }
	else if (!covered && t->still) { t->still = 0; t->stillUp = 0; tabev(t, "still", "\"img\":null"); }
}
static void apply_place(tab *t)
{
	cover_step(t);
	if (g_getenv("RUDOLF_WK_DEBUG")) g_printerr("apply %s shown=%d still=%d up=%d\n", t->tid, t->shown, t->still, t->stillUp);
	if (t->fs) {
		int W = gtk_widget_get_allocated_width(overlay), H = gtk_widget_get_allocated_height(overlay);
		gtk_widget_set_margin_start(t->box, 0); gtk_widget_set_margin_top(t->box, 0); gtk_widget_set_size_request(t->box, W, H);
		gtk_widget_show(t->box); shape(t); tabs_order(); return;
	}
	if (!t->shown) { gtk_widget_hide(t->box); return; }
	if (t->still && t->stillUp) { step_aside(t); return; }
	gtk_widget_set_margin_start(t->box, t->x); gtk_widget_set_margin_top(t->box, t->y);
	gtk_widget_set_size_request(t->box, t->w, t->h);
	gtk_widget_show(t->box); shape(t); tabs_order();
}
static void on_box_realize(GtkWidget *w, gpointer d) { (void)w; shape(d); }
static void on_box_size(GtkWidget *w, GdkRectangle *r, gpointer d) { (void)w; (void)r; shape(d); }

/* signals of a tab's view */
static gboolean on_tab_load_failed(WebKitWebView *v, WebKitLoadEvent e, gchar *u, GError *err, gpointer d)
{
	(void)v; (void)e; tab *t = d;
	if (g_error_matches(err, WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_CANCELLED) || g_error_matches(err, WEBKIT_POLICY_ERROR, WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE)) return FALSE;
	GString *x = g_string_new("\"loading\":false,\"error\":"); json_str(x, err ? err->message : ""); g_string_append(x, ",\"url\":"); json_str(x, u);
	tabev(t, "load", x->str); g_string_free(x, TRUE); return FALSE;
}
static void on_tab_load(WebKitWebView *v, WebKitLoadEvent e, gpointer d)
{
	tab *t = d;
	if (e == WEBKIT_LOAD_STARTED) tabev(t, "load", "\"loading\":true");
	else if (e == WEBKIT_LOAD_COMMITTED) { nav_state(t); if (DARK) webkit_web_view_run_javascript(v, DARK_ON, NULL, NULL, NULL); }
	else if (e == WEBKIT_LOAD_FINISHED) { tabev(t, "load", "\"loading\":false"); nav_state(t); }
}
static void on_tab_uri(GObject *o, GParamSpec *p, gpointer d) { (void)o; (void)p; nav_state(d); }
static void on_tab_title(GObject *o, GParamSpec *p, gpointer d)
{
	(void)p; GString *x = g_string_new("\"title\":"); const char *ti = webkit_web_view_get_title(WEBKIT_WEB_VIEW(o));
	char *c = g_utf8_substring(ti ? ti : "", 0, 300); json_str(x, c); g_free(c); tabev(d, "title", x->str); g_string_free(x, TRUE);
}
static void on_tab_close(WebKitWebView *v, gpointer d) { (void)v; tabev(d, "close", NULL); }
static void on_tab_gone(WebKitWebView *v, WebKitWebProcessTerminationReason r, gpointer d)
{
	(void)v; tabev(d, "gone", r == WEBKIT_WEB_PROCESS_EXCEEDED_MEMORY_LIMIT ? "\"why\":\"oom\"" : "\"why\":\"crashed\"");
}
static gboolean on_tab_focus(GtkWidget *w, GdkEvent *e, gpointer d) { (void)w; (void)e; tabev(d, "focus", NULL); return FALSE; }
static gboolean on_tab_dialog(WebKitWebView *v, WebKitScriptDialog *dl, gpointer d)
{
	(void)v; int did = ++DLGN; webkit_script_dialog_ref(dl); g_hash_table_insert(DLG, GINT_TO_POINTER(did), dl);
	WebKitScriptDialogType ty = webkit_script_dialog_get_dialog_type(dl);
	const char *kind = ty == WEBKIT_SCRIPT_DIALOG_ALERT ? "alert" : ty == WEBKIT_SCRIPT_DIALOG_PROMPT ? "prompt" : "confirm";
	GString *x = g_string_new(""); g_string_append_printf(x, "\"did\":%d,\"kind\":", did); json_str(x, kind);
	char *msg = g_utf8_substring(webkit_script_dialog_get_message(dl) ? webkit_script_dialog_get_message(dl) : "", 0, 2000);
	g_string_append(x, ",\"text\":"); json_str(x, ty == WEBKIT_SCRIPT_DIALOG_BEFORE_UNLOAD_CONFIRM ? "Leave this page? Changes you made may not be saved." : msg); g_free(msg);
	g_string_append(x, ",\"def\":"); json_str(x, ty == WEBKIT_SCRIPT_DIALOG_PROMPT ? webkit_script_dialog_prompt_get_default_text(dl) : "");
	tabev(d, "dialog", x->str); g_string_free(x, TRUE);
	return TRUE;   /* answered later (op 'dialog') */
}
static gboolean on_tab_perm(WebKitWebView *v, WebKitPermissionRequest *req, gpointer d)
{
	const char *p = WEBKIT_IS_USER_MEDIA_PERMISSION_REQUEST(req) ? "media" : WEBKIT_IS_GEOLOCATION_PERMISSION_REQUEST(req) ? "geolocation" : NULL;
#ifdef WEBKIT_IS_POINTER_LOCK_PERMISSION_REQUEST
	if (WEBKIT_IS_POINTER_LOCK_PERMISSION_REQUEST(req)) { webkit_permission_request_allow(req); return TRUE; }
#endif
	if (!p) { webkit_permission_request_deny(req); return TRUE; }
	int pid = ++PERMN; g_object_ref(req); g_hash_table_insert(PERM, GINT_TO_POINTER(pid), req);
	GString *x = g_string_new(""); g_string_append_printf(x, "\"pid\":%d,\"perm\":", pid); json_str(x, p); g_string_append(x, ",\"url\":"); json_str(x, webkit_web_view_get_uri(v));
	tabev(d, "perm", x->str); g_string_free(x, TRUE); return TRUE;
}
static gboolean on_tab_fs(WebKitWebView *v, gpointer d)
{
	(void)v; tab *t = d; t->fs = 1; tabev(t, "fs", "\"on\":true"); apply_place(t); gtk_widget_grab_focus(t->view); return FALSE;
}
static gboolean on_tab_unfs(WebKitWebView *v, gpointer d)
{
	(void)v; tab *t = d; t->fs = 0; tabev(t, "fs", "\"on\":false"); apply_place(t); return FALSE;
}
static gboolean on_tab_decide(WebKitWebView *v, WebKitPolicyDecision *dec, WebKitPolicyDecisionType type, gpointer d)
{
	(void)v; tab *t = d;
	if (type == WEBKIT_POLICY_DECISION_TYPE_RESPONSE) {
		WebKitResponsePolicyDecision *r = WEBKIT_RESPONSE_POLICY_DECISION(dec);
		if (!webkit_response_policy_decision_is_mime_type_supported(r)) { webkit_policy_decision_download(dec); return TRUE; }
		return FALSE;
	}
	WebKitNavigationAction *a = webkit_navigation_policy_decision_get_navigation_action(WEBKIT_NAVIGATION_POLICY_DECISION(dec));
	const char *u = webkit_uri_request_get_uri(webkit_navigation_action_get_request(a)); const char *why = fw_why(u);
	if (!why) return FALSE;
	webkit_policy_decision_ignore(dec);
	GString *x = g_string_new("\"url\":"); json_str(x, u); g_string_append(x, ",\"why\":"); json_str(x, why);
	g_string_append_printf(x, ",\"main\":%s", type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION ? "true" : "false");
	tabev(t, "blocked", x->str); g_string_free(x, TRUE); return TRUE;
}
static tab *tab_new(const char *tid, const char *part, WebKitWebView *related);
static void on_tab_ready(WebKitWebView *v, gpointer d)
{
	tab *nt = d; WebKitWindowProperties *wp = webkit_web_view_get_window_properties(v);
	int popup = wp && !webkit_window_properties_get_toolbar_visible(wp);
	tab *opener = g_object_get_data(G_OBJECT(v), "rudolf-opener");
	GString *x = g_string_new("\"url\":"); json_str(x, webkit_web_view_get_uri(v) ? webkit_web_view_get_uri(v) : "about:blank");
	g_string_append(x, ",\"ntid\":"); json_str(x, nt->tid); g_string_append_printf(x, ",\"popup\":%s", popup ? "true" : "false");
	if (opener) tabev(opener, "newwin", x->str); g_string_free(x, TRUE);
}
static GtkWidget *on_tab_create(WebKitWebView *v, WebKitNavigationAction *a, gpointer d)
{
	tab *t = d; const char *u = webkit_uri_request_get_uri(webkit_navigation_action_get_request(a));
	if ((u && *u && !http_url(u) && strcmp(u, "about:blank")) || g_hash_table_size(TABS) >= 60) return NULL;
	char ntid[64]; snprintf(ntid, sizeof ntid, "n%d-%.40s", ++NEWN, t->tid);
	tab *nt = tab_new(ntid, t->part, v);
	g_object_set_data(G_OBJECT(nt->view), "rudolf-opener", t);
	g_signal_connect(nt->view, "ready-to-show", G_CALLBACK(on_tab_ready), nt);
	return nt->view;
}
/* the page's own scripts (an isolated world): shortcuts, Flash content */
static void on_tab_msg(WebKitUserContentManager *m, WebKitJavascriptResult *r, gpointer d)
{
	(void)m; tab *t = d; JSCValue *o = webkit_javascript_result_get_js_value(r); if (!jsc_value_is_object(o)) return;
	char *k = jstr(o, "k"), *fl = jstr(o, "flash");
	if (k) { GString *x = g_string_new("\"k\":"); k[strcspn(k, "\n")] = 0; if (strlen(k) > 8) k[8] = 0; json_str(x, k); tabev(t, "keys", x->str); g_string_free(x, TRUE); }
	else if (fl && RUFFLE_JS[0]) { webkit_web_view_run_javascript(WEBKIT_WEB_VIEW(t->view), RUFFLE_JS, NULL, NULL, NULL); tabev(t, "flash", NULL); }
	g_free(k); g_free(fl);
}
static tab *tab_new(const char *tid, const char *part, WebKitWebView *related)
{
	tab *t = g_new0(tab, 1); snprintf(t->tid, sizeof t->tid, "%s", tid); snprintf(t->part, sizeof t->part, "%s", part);
	WebKitUserContentManager *ucm = webkit_user_content_manager_new();
	webkit_user_content_manager_register_script_message_handler_in_world(ucm, "rudolfTab", "rudolf");
	g_signal_connect(ucm, "script-message-received::rudolfTab", G_CALLBACK(on_tab_msg), t);
	webkit_user_content_manager_add_script(ucm, webkit_user_script_new_for_world(PAGE_JS, WEBKIT_USER_CONTENT_INJECT_TOP_FRAME, WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START, "rudolf", NULL, NULL));
	if (related) t->view = GTK_WIDGET(g_object_new(WEBKIT_TYPE_WEB_VIEW, "related-view", related, "user-content-manager", ucm, NULL));
	else t->view = GTK_WIDGET(g_object_new(WEBKIT_TYPE_WEB_VIEW, "web-context", ctx_for(part), "user-content-manager", ucm, NULL));
	WebKitSettings *s = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(t->view));
	g_object_set(s, "enable-javascript", TRUE, "enable-webaudio", TRUE, "enable-media-stream", TRUE, "enable-webgl", TRUE, "enable-fullscreen", TRUE,
		"enable-developer-extras", FALSE, "javascript-can-open-windows-automatically", FALSE, "enable-back-forward-navigation-gestures", TRUE, NULL);
	webkit_settings_set_hardware_acceleration_policy(s, HWPOL);
	t->box = gtk_event_box_new(); gtk_event_box_set_visible_window(GTK_EVENT_BOX(t->box), TRUE);
	gtk_widget_set_halign(t->box, GTK_ALIGN_START); gtk_widget_set_valign(t->box, GTK_ALIGN_START);
	gtk_container_add(GTK_CONTAINER(t->box), t->view); gtk_widget_show(t->view);
	g_signal_connect(t->view, "draw", G_CALLBACK(on_box_draw), t);   /* (the view draws itself: the clip must be on its own drawing) */
	gtk_overlay_add_overlay(GTK_OVERLAY(overlay), t->box);
	g_signal_connect(t->box, "realize", G_CALLBACK(on_box_realize), t);
	g_signal_connect(t->box, "draw", G_CALLBACK(on_box_draw), t);
	g_signal_connect(t->box, "size-allocate", G_CALLBACK(on_box_size), t);
	g_signal_connect(t->view, "load-changed", G_CALLBACK(on_tab_load), t);
	g_signal_connect(t->view, "load-failed", G_CALLBACK(on_tab_load_failed), t);
	g_signal_connect(t->view, "notify::uri", G_CALLBACK(on_tab_uri), t);
	g_signal_connect(t->view, "notify::title", G_CALLBACK(on_tab_title), t);
	g_signal_connect(t->view, "close", G_CALLBACK(on_tab_close), t);
	g_signal_connect(t->view, "web-process-terminated", G_CALLBACK(on_tab_gone), t);
	g_signal_connect(t->view, "focus-in-event", G_CALLBACK(on_tab_focus), t);
	g_signal_connect(t->view, "script-dialog", G_CALLBACK(on_tab_dialog), t);
	g_signal_connect(t->view, "permission-request", G_CALLBACK(on_tab_perm), t);
	g_signal_connect(t->view, "enter-fullscreen", G_CALLBACK(on_tab_fs), t);
	g_signal_connect(t->view, "leave-fullscreen", G_CALLBACK(on_tab_unfs), t);
	g_signal_connect(t->view, "decide-policy", G_CALLBACK(on_tab_decide), t);
	g_signal_connect(t->view, "create", G_CALLBACK(on_tab_create), t);
	g_hash_table_insert(TABS, g_strdup(t->tid), t);
	return t;
}
static void tabs_reset(void)
{
	g_hash_table_remove_all(TABS);
	GHashTableIter i; gpointer k, v;
	g_hash_table_iter_init(&i, DLG); while (g_hash_table_iter_next(&i, &k, &v)) { webkit_script_dialog_close(v); webkit_script_dialog_unref(v); } g_hash_table_remove_all(DLG);
	g_hash_table_iter_init(&i, PERM); while (g_hash_table_iter_next(&i, &k, &v)) { webkit_permission_request_deny(v); g_object_unref(v); } g_hash_table_remove_all(PERM);
}
/* downloads: where Chromium's go, as NAME.crdownload until complete (the desktop files them, dl27.js) */
static gboolean on_dl_dest(WebKitDownload *dl, gchar *sug, gpointer d)
{
	(void)d; char *base = g_path_get_basename(sug && *sug ? sug : "download");
	for (char *c = base; *c; c++) if (*c == '/' || (unsigned char)*c < 0x20) *c = '_';
	g_mkdir_with_parents("/run/rudolf/dl/.incoming", 0700);
	char *fin = g_build_filename("/run/rudolf/dl/.incoming", base, NULL);
	for (int n = 1; n < 1000 && g_file_test(fin, G_FILE_TEST_EXISTS); n++) { g_free(fin); char *nb = g_strdup_printf("%d-%s", n, base); fin = g_build_filename("/run/rudolf/dl/.incoming", nb, NULL); g_free(nb); }
	char *part = g_strconcat(fin, ".crdownload", NULL), *uri = g_filename_to_uri(part, NULL, NULL);
	webkit_download_set_destination(dl, uri);
	g_object_set_data_full(G_OBJECT(dl), "rudolf-final", fin, g_free); g_object_set_data_full(G_OBJECT(dl), "rudolf-part", part, g_free);
	g_free(uri); g_free(base); return TRUE;
}
static void on_dl_done(WebKitDownload *dl, gpointer d)
{
	(void)d; const char *p = g_object_get_data(G_OBJECT(dl), "rudolf-part"), *f = g_object_get_data(G_OBJECT(dl), "rudolf-final");
	if (p && f && g_object_get_data(G_OBJECT(dl), "rudolf-failed") == NULL) { if (rename(p, f)) note("download: could not finish %s", f); }
}
static void on_dl_fail(WebKitDownload *dl, GError *e, gpointer d)
{
	(void)e; (void)d; g_object_set_data(G_OBJECT(dl), "rudolf-failed", GINT_TO_POINTER(1));
	const char *p = g_object_get_data(G_OBJECT(dl), "rudolf-part"); if (p) unlink(p);
}
static void on_download(WebKitWebContext *c, WebKitDownload *dl, gpointer d)
{
	(void)c; (void)d; WebKitWebView *v = webkit_download_get_web_view(dl); if (!v || GTK_WIDGET(v) == desk) return;
	g_signal_connect(dl, "decide-destination", G_CALLBACK(on_dl_dest), NULL);
	g_signal_connect(dl, "finished", G_CALLBACK(on_dl_done), NULL);
	g_signal_connect(dl, "failed", G_CALLBACK(on_dl_fail), NULL);
	GHashTableIter i; gpointer k, val; g_hash_table_iter_init(&i, TABS);
	while (g_hash_table_iter_next(&i, &k, &val)) if (((tab *)val)->view == GTK_WIDGET(v)) {
		WebKitURIRequest *rq = webkit_download_get_request(dl); GString *x = g_string_new("\"url\":"); json_str(x, rq ? webkit_uri_request_get_uri(rq) : "");
		g_string_append(x, ",\"started\":true"); tabev(val, "download", x->str); g_string_free(x, TRUE); break; }
}
static void cleared(GObject *o, GAsyncResult *r, gpointer d)
{
	GError *e = NULL; int ok = webkit_website_data_manager_clear_finish(WEBKIT_WEBSITE_DATA_MANAGER(o), r, &e); if (e) g_error_free(e);
	GString *m = g_string_new("{\"rudolfShell\":\"cleared\",\"id\":"); g_string_append_printf(m, "%d,\"ok\":%s}", GPOINTER_TO_INT(d), ok ? "true" : "false"); to_desk(m);
}
static int rect_of(JSCValue *a, int *x)
{
	if (!a || !jsc_value_is_array(a)) return 0;
	for (int i = 0; i < 4; i++) { JSCValue *v = jsc_value_object_get_property_at_index(a, i); if (!v || !jsc_value_is_number(v)) { if (v) g_object_unref(v); return 0; } x[i] = (int)jsc_value_to_double(v); g_object_unref(v); }
	return 1;
}
static void on_tab_op(JSCValue *m)
{
	char *op = jstr(m, "op"), *tid = jstr(m, "tid"); if (!op) { g_free(tid); return; }
	tab *t = tid ? g_hash_table_lookup(TABS, tid) : NULL;
	if (!strcmp(op, "fw")) {
		JSCValue *r = jsc_value_object_get_property(m, "rules");
		if (r && jsc_value_is_object(r)) { FW.on = jbool(r, "on"); char *md = jstr(r, "mode"); snprintf(FW.mode, sizeof FW.mode, "%s", md ? md : "open"); g_free(md);
			g_strfreev(FW.block); g_strfreev(FW.trust); FW.block = str_list(r, "block"); FW.trust = str_list(r, "trust"); }
		if (r) g_object_unref(r);
	} else if (!strcmp(op, "dialog")) {
		int did = (int)jnum(m, "did", 0); WebKitScriptDialog *dl = g_hash_table_lookup(DLG, GINT_TO_POINTER(did));
		if (dl) { int ok = jbool(m, "ok"); WebKitScriptDialogType ty = webkit_script_dialog_get_dialog_type(dl);
			if (ty == WEBKIT_SCRIPT_DIALOG_CONFIRM || ty == WEBKIT_SCRIPT_DIALOG_BEFORE_UNLOAD_CONFIRM) webkit_script_dialog_confirm_set_confirmed(dl, ok);
			else if (ty == WEBKIT_SCRIPT_DIALOG_PROMPT && ok) { char *tx = jstr(m, "text"); webkit_script_dialog_prompt_set_text(dl, tx ? tx : ""); g_free(tx); }
			webkit_script_dialog_close(dl); g_hash_table_remove(DLG, GINT_TO_POINTER(did)); webkit_script_dialog_unref(dl); }
	} else if (!strcmp(op, "perm")) {
		int pid = (int)jnum(m, "pid", 0); WebKitPermissionRequest *p = g_hash_table_lookup(PERM, GINT_TO_POINTER(pid));
		if (p) { if (jbool(m, "allow")) webkit_permission_request_allow(p); else webkit_permission_request_deny(p); g_hash_table_remove(PERM, GINT_TO_POINTER(pid)); g_object_unref(p); }
	} else if (!strcmp(op, "blur")) gtk_widget_grab_focus(desk);
	else if (!strcmp(op, "dark")) {
		DARK = jbool(m, "on"); GHashTableIter i; gpointer k, v; g_hash_table_iter_init(&i, TABS);
		while (g_hash_table_iter_next(&i, &k, &v)) webkit_web_view_run_javascript(WEBKIT_WEB_VIEW(((tab *)v)->view), DARK ? DARK_ON : DARK_OFF, NULL, NULL, NULL);
	} else if (!strcmp(op, "ruffle")) {
		/* Ruffle's own copy and settings, as the desktop's Flash Player (the desktop sends them) */
		JSCValue *c = jsc_value_object_get_property(m, "cfg"); RUFFLE_JS[0] = 0;
		if (c && jsc_value_is_object(c) && jbool(c, "on")) {
			char **u = str_list(c, "urls"); JSCValue *cf = jsc_value_object_get_property(c, "config"); char *cfj = cf ? jsc_value_to_json(cf, 0) : NULL;
			GString *j = g_string_new("(()=>{if(document.documentElement.dataset.rudolfRuffle)return;document.documentElement.dataset.rudolfRuffle='1';"
				"window.RufflePlayer=window.RufflePlayer||{};window.RufflePlayer.config=Object.assign({unmuteOverlay:'visible',warnOnUnsupportedContent:false},");
			g_string_append(j, cfj && *cfj == '{' ? cfj : "{}"); g_string_append(j, ",window.RufflePlayer.config||{});const U=[");
			for (int i = 0, n = 0; u && u[i] && n < 4; i++) if (http_url(u[i])) { if (n++) g_string_append_c(j, ','); json_str(j, u[i]); }
			g_string_append(j, "];let i=0;const next=()=>{if(i>=U.length)return;const s=document.createElement('script');s.src=U[i++];s.onerror=()=>{s.remove();next()};(document.head||document.documentElement).appendChild(s)};next()})()");
			if (j->len < sizeof RUFFLE_JS) memcpy(RUFFLE_JS, j->str, j->len + 1);
			g_string_free(j, TRUE); g_strfreev(u); g_free(cfj); if (cf) g_object_unref(cf);
		}
		if (c) g_object_unref(c);
	} else if (!strcmp(op, "clear")) {
		char *part = jstr(m, "part"); int id = (int)jnum(m, "id", 0);
		if (part) { char safe[48]; int n = 0; for (char *c = part; *c && n < 40; c++) if (g_ascii_isalnum(*c) || *c == '_' || *c == '-') safe[n++] = *c; safe[n] = 0;
			webkit_website_data_manager_clear(webkit_web_context_get_website_data_manager(ctx_for(*safe ? safe : "x")), WEBKIT_WEBSITE_DATA_ALL, 0, NULL, cleared, GINT_TO_POINTER(id)); }
		g_free(part);
	} else if (!strcmp(op, "open")) {
		char *adopt = jstr(m, "adopt"), *u = jstr(m, "url"), *part = jstr(m, "part");
		tab *a = adopt ? g_hash_table_lookup(TABS, adopt) : NULL;
		if (a) { gpointer ok = NULL; g_hash_table_steal_extended(TABS, adopt, &ok, NULL); g_free(ok); snprintf(a->tid, sizeof a->tid, "%s", tid ? tid : "x"); g_hash_table_insert(TABS, g_strdup(a->tid), a); }
		else if (tid && !t && g_hash_table_size(TABS) < 60) {
			char safe[48]; int n = 0; for (char *c = part ? part : "x"; *c && n < 40; c++) if (g_ascii_isalnum(*c) || *c == '_' || *c == '-') safe[n++] = *c; safe[n] = 0;
			tab *nt = tab_new(tid, *safe ? safe : "x", NULL);
			if (http_url(u)) webkit_web_view_load_uri(WEBKIT_WEB_VIEW(nt->view), u);
		}
		g_free(adopt); g_free(u); g_free(part);
	} else if (t) {
		WebKitWebView *v = WEBKIT_WEB_VIEW(t->view);
		if (!strcmp(op, "go")) { char *u = jstr(m, "url"); if (http_url(u)) webkit_web_view_load_uri(v, u); g_free(u); }
		else if (!strcmp(op, "back")) webkit_web_view_go_back(v);
		else if (!strcmp(op, "forward")) webkit_web_view_go_forward(v);
		else if (!strcmp(op, "reload")) webkit_web_view_reload(v);
		else if (!strcmp(op, "stop")) webkit_web_view_stop_loading(v);
		else if (!strcmp(op, "zoom")) { double z = jnum(m, "z", 1); webkit_web_view_set_zoom_level(v, z < .25 ? .25 : z > 5 ? 5 : z); }
		else if (!strcmp(op, "focus")) gtk_widget_grab_focus(t->view);
		else if (!strcmp(op, "close")) g_hash_table_remove(TABS, t->tid);
		else if (!strcmp(op, "place")) {
			JSCValue *r = jsc_value_object_get_property(m, "r"), *vis = jsc_value_object_get_property(m, "vis"); int x[4];
			if (rect_of(r, x) && x[2] > 0 && x[3] > 0) {
				t->x = x[0]; t->y = x[1]; t->w = x[2]; t->h = x[3]; t->z = (int)jnum(m, "z", 0); t->shown = 1;
				if (t->vis) cairo_region_destroy(t->vis); t->vis = NULL;
				if (vis && jsc_value_is_array(vis)) {
					t->vis = cairo_region_create();
					for (guint i = 0; i < 200; i++) { JSCValue *e = jsc_value_object_get_property_at_index(vis, i); int y[4];
						if (!e || jsc_value_is_undefined(e)) { if (e) g_object_unref(e); break; }
						if (rect_of(e, y)) { cairo_rectangle_int_t rc = {y[0], y[1], y[2], y[3]}; cairo_region_union_rectangle(t->vis, &rc); }
						g_object_unref(e); }
					if (cairo_region_is_empty(t->vis)) t->shown = 0;
				}
			} else t->shown = 0;
			if (r) g_object_unref(r); if (vis) g_object_unref(vis);
			if (g_getenv("RUDOLF_WK_DEBUG")) g_printerr("place %s shown=%d at %d,%d %dx%d cut=%d\n", t->tid, t->shown, t->x, t->y, t->w, t->h, t->vis ? cairo_region_num_rectangles(t->vis) : -1);
			apply_place(t);
		}
	}
	g_free(op); g_free(tid);
}

/* ------------------------------------------------------------------ the desktop page */
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
	desk_ready = 0; tabs_reset(); g_timeout_add_seconds(1, reload_later, v);
}
static GtkWidget *on_create(WebKitWebView *v, WebKitNavigationAction *a, gpointer d)
{
	/* a new window asked for by the desktop page itself: a website goes where the desktop sends websites */
	(void)v; (void)d; const char *u = webkit_uri_request_get_uri(webkit_navigation_action_get_request(a));
	if (http_url(u) && !local_or_not_web(u)) { GString *m = g_string_new("{\"rudolfShell\":\"newwindow\",\"url\":"); json_str(m, u); g_string_append_c(m, '}'); to_desk(m); }
	return NULL;
}
static gboolean hello(gpointer d)
{
	(void)d; if (desk_ready < 0) { hello_timer = 0; return G_SOURCE_REMOVE; }
	desk_ready = 1; to_desk(g_string_new("{\"rudolfShell\":\"hello\",\"version\":\"webkit-1.5.8\",\"caps\":[\"tabs\",\"dark\"]}"));
	return G_SOURCE_CONTINUE;
}
static void on_desk_load(WebKitWebView *v, WebKitLoadEvent e, gpointer d)
{
	(void)v; (void)d;
	if (e == WEBKIT_LOAD_STARTED || e == WEBKIT_LOAD_COMMITTED) { if (hello_timer) g_source_remove(hello_timer); hello_timer = 0; desk_ready = 0; tabs_reset(); }
	if (e == WEBKIT_LOAD_FINISHED) { hello(NULL); hello_timer = g_timeout_add_seconds(1, hello, NULL); }
}
static void on_desk_msg(WebKitUserContentManager *u, WebKitJavascriptResult *r, gpointer d)
{
	(void)u; (void)d; JSCValue *m = webkit_javascript_result_get_js_value(r); if (!jsc_value_is_object(m)) return;
	char *k = jstr(m, "rudolfShell"); if (!k) return;
	if (!strcmp(k, "ready")) { if (hello_timer) g_source_remove(hello_timer); hello_timer = 0; desk_ready = 1; }
	else if (!strcmp(k, "ping")) { GString *p = g_string_new(""); g_string_append_printf(p, "{\"rudolfShell\":\"pong\",\"id\":%d}", (int)jnum(m, "id", 0)); to_desk(p); }
	else if (!strcmp(k, "tab")) on_tab_op(m);
	g_free(k);
}

int main(int argc, char **argv)
{
	int w = 0, h = 0;
	for (int i = 1; i < argc; i++) {
		if (!strncmp(argv[i], "--size=", 7)) { if (sscanf(argv[i] + 7, "%dx%d", &w, &h) != 2) w = h = 0; }
		else if (!strcmp(argv[i], "--draw=soft") || !strcmp(argv[i], "--software")) DRAW = DRAW_SOFT;
		else if (!strcmp(argv[i], "--draw=mixed")) DRAW = DRAW_MIXED;
		else if (!strcmp(argv[i], "--draw=gpu")) DRAW = DRAW_GPU;
		else if (!strncmp(argv[i], "http://127.0.0.1:", 17)) url = argv[i];
	}
	if (DRAW == DRAW_SOFT) HWPOL = WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER;
	if (DRAW == DRAW_MIXED) {   /* (WebKitGTK 2.52: read by the web processes, which inherit the environment) */
		char n[8]; int c = (int)g_get_num_processors() - 1; snprintf(n, sizeof n, "%d", c < 1 ? 1 : c > 8 ? 8 : c);
		g_setenv("WEBKIT_SKIA_ENABLE_CPU_RENDERING", "1", TRUE); g_setenv("WEBKIT_SKIA_CPU_PAINTING_THREADS", n, TRUE);
	}
	g_set_prgname("RudolfDesktop"); gdk_set_program_class("RudolfDesktop");
	gtk_init(&argc, &argv);
	TABS = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, tab_free);
	CTX = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
	DLG = g_hash_table_new(g_direct_hash, g_direct_equal); PERM = g_hash_table_new(g_direct_hash, g_direct_equal);
	snprintf(FW.mode, sizeof FW.mode, "open");
	char *home = g_build_filename(g_get_home_dir(), ".rudolf-webkit", NULL);
	char *data = g_build_filename(home, "data", NULL), *cache = g_build_filename(home, "cache", NULL);
	WebKitWebsiteDataManager *mgr = webkit_website_data_manager_new("base-data-directory", data, "base-cache-directory", cache, NULL);
	/* 1.5.8: the desktop's web process frees what it can spare (decoded images, fonts, JavaScript's leftovers) from about
	   250 MB (conservative, 1/3 of the limit) and more from about 380 MB (strict, 1/2), checked every 15 s; WebKit's own
	   limit follows the computer's memory, so on a 4 GB laptop it began only past 1 GB. It is never ended for its size. */
	WebKitMemoryPressureSettings *mps = webkit_memory_pressure_settings_new();
	webkit_memory_pressure_settings_set_memory_limit(mps, 768);
	webkit_memory_pressure_settings_set_conservative_threshold(mps, 0.33);
	webkit_memory_pressure_settings_set_strict_threshold(mps, 0.5);
	webkit_memory_pressure_settings_set_kill_threshold(mps, 0);
	webkit_memory_pressure_settings_set_poll_interval(mps, 15);
	WebKitWebContext *ctx = WEBKIT_WEB_CONTEXT(g_object_new(WEBKIT_TYPE_WEB_CONTEXT, "website-data-manager", mgr, "memory-pressure-settings", mps, NULL));
	webkit_memory_pressure_settings_free(mps);
	webkit_web_context_set_cache_model(ctx, WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER);   /* the least for caches: the page is local */
	WebKitUserContentManager *ucm = webkit_user_content_manager_new();
	webkit_user_content_manager_register_script_message_handler(ucm, "rudolfShell");
	g_signal_connect(ucm, "script-message-received::rudolfShell", G_CALLBACK(on_desk_msg), NULL);
	if (DRAW == DRAW_SOFT) webkit_user_content_manager_add_style_sheet(ucm, webkit_user_style_sheet_new(LIGHT_CSS,
		WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES, WEBKIT_USER_STYLE_LEVEL_USER, NULL, NULL));
	desk = GTK_WIDGET(g_object_new(WEBKIT_TYPE_WEB_VIEW, "web-context", ctx, "user-content-manager", ucm, NULL));
	WebKitSettings *s = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(desk));
	g_object_set(s, "enable-javascript", TRUE, "javascript-can-access-clipboard", TRUE, "enable-webaudio", TRUE,
		"enable-media-stream", TRUE, "enable-webgl", TRUE, "enable-developer-extras", FALSE,
		"media-playback-requires-user-gesture", FALSE, "allow-file-access-from-file-urls", FALSE,
		"enable-page-cache", FALSE,   /* (1.5.8: the desktop never goes back to an earlier page: no back/forward cache) */
		"enable-write-console-messages-to-stdout", TRUE, NULL);
	webkit_settings_set_hardware_acceleration_policy(s, HWPOL);
	g_signal_connect(desk, "permission-request", G_CALLBACK(on_permission), NULL);
	g_signal_connect(desk, "decide-policy", G_CALLBACK(on_decide), NULL);
	g_signal_connect(desk, "web-process-terminated", G_CALLBACK(on_terminated), NULL);
	g_signal_connect(desk, "create", G_CALLBACK(on_create), NULL);
	g_signal_connect(desk, "load-changed", G_CALLBACK(on_desk_load), NULL);
	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_decorated(GTK_WINDOW(win), FALSE); gtk_window_set_title(GTK_WINDOW(win), "Rudolf OS");
	overlay = gtk_overlay_new(); gtk_container_add(GTK_CONTAINER(win), overlay); gtk_container_add(GTK_CONTAINER(overlay), desk);
	if (w <= 0 || h <= 0) { GdkScreen *scr = gdk_screen_get_default(); w = gdk_screen_get_width(scr); h = gdk_screen_get_height(scr); }
	gtk_window_set_default_size(GTK_WINDOW(win), w, h); gtk_window_move(GTK_WINDOW(win), 0, 0); gtk_window_fullscreen(GTK_WINDOW(win));
	g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	webkit_web_view_load_uri(WEBKIT_WEB_VIEW(desk), url);
	gtk_widget_show_all(win);
	gtk_main();
	return 0;
}
