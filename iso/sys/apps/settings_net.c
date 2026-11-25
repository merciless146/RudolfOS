/*
 * rudolf-settings: Network Settings (base_v24.html netPanelS and what the modules add to it: settings25.js's real network (Wi-Fi,
 * Network hardware, Manual address, Proxy), wifi26.js wifiPanel, settings31.js (Web search and Downloads moved to Oracle's page),
 * mobile58.js's Metered connection, modem59.js's Cellular modems, hotspot59.js's Share this connection, vpn62.js's VPN). 1.6.3,
 * docs/plan-1.6.3.md Part 3 (PR 6). The same rows from the same rudolfd requests, the same actions (wifi/..., net/static, net/dhcp,
 * proxy, net/metered, net/hotspot, vpn/...), with the administrator's prompt where the page asks for it.
 * The installed system's page as the page shows it: what settings25.js hides (the browser version's connections) is not drawn;
 * what settings31.js hides but the page's CSS still shows (the Web search and Downloads rows, the footer: .r2 and .sfoot keep their
 * display over [hidden]) is drawn, as it appears. GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gstdio.h>
#include "settings.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
#pragma GCC diagnostic ignored "-Wunused-variable"
#include "settings-gen.h"
#pragma GCC diagnostic pop

static RuiJ *NET;                      /* net */
static GtkWidget *ALIVE;
static int KEEP;
static void redraw(void) { KEEP = 1; set_show_keep("net"); KEEP = 0; }
static int post(const char *path, const char *body, char *msg, size_t n)
{
	char *out = NULL; int c = rui_http("POST", path, body, body ? strlen(body) : 0, &out, NULL); RuiJ *j = out ? rui_jparse(out) : NULL;
	if (msg) snprintf(msg, n, "%s", set_jstr(j, c == 200 ? "message" : "error", c ? "" : "rudolfd did not answer"));
	set_note("POST %s %d", path, c); rui_jfree(j); g_free(out); return c;
}
static void post_note(const char *title, const char *path, const char *body) { char m[400]; int c = post(path, body, m, sizeof m); set_notify(title, m, c != 200); }
static char *fmt_b(double b)
{
	/* (the page's fmtB) */
	if (b < 0) return g_strdup(""); if (b < 1024) return g_strdup_printf("%.0f B", b); if (b < 1048576) return g_strdup_printf("%.1f KB", b / 1024);
	if (b < 1073741824) return g_strdup_printf("%.2f MB", b / 1048576); return g_strdup_printf("%.1f GB", b / 1073741824);
}

/* ================================================================== wpa_cli's words (wfDecode, wfStatus) */
static char *wf_decode(const char *s)
{
	GString *b = g_string_new(NULL);
	for (size_t i = 0; s[i]; i++) { char c = s[i]; if (c == '\\' && s[i + 1]) { char n = s[++i]; if (n == 'x' && g_ascii_isxdigit(s[i + 1]) && g_ascii_isxdigit(s[i + 2])) { g_string_append_c(b, (char)(g_ascii_xdigit_value(s[i + 1]) * 16 + g_ascii_xdigit_value(s[i + 2]))); i += 2; }
			else g_string_append_c(b, n == 'n' ? 10 : n == 'r' ? 13 : n == 't' ? 9 : n == 'e' ? 27 : n); } else g_string_append_c(b, c); }
	if (!g_utf8_validate(b->str, -1, NULL)) { char *l = g_convert(b->str, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL); g_string_free(b, TRUE); return l ? l : g_strdup(""); }
	return g_string_free(b, FALSE);
}
typedef struct { char *ssid, *flags; } WSaved;
typedef struct { char *state, *ssid, *ip, *rssi, *speed, *country, *adapter; GPtrArray *saved; int nwlan; } WStat;
static WStat W;
static void wst_clear(void) { g_free(W.state); g_free(W.ssid); g_free(W.ip); g_free(W.rssi); g_free(W.speed); g_free(W.country); g_free(W.adapter); if (W.saved) g_ptr_array_unref(W.saved); memset(&W, 0, sizeof W); }
static void ws_free(gpointer p) { WSaved *s = p; g_free(s->ssid); g_free(s->flags); g_free(s); }
static void wst_parse(const char *raw)
{
	wst_clear(); W.saved = g_ptr_array_new_with_free_func(ws_free); char **L = g_strsplit(raw ? raw : "", "\n", -1);
	for (int i = 0; L[i]; i++) { char *e = strchr(L[i], '='); if (!e) continue; *e = 0; const char *k = L[i], *v = e + 1;
		if (!strcmp(k, "saved")) { char **p = g_strsplit(v, "\t", -1); WSaved *s = g_new0(WSaved, 1); s->ssid = wf_decode(p[0] && p[1] ? p[1] : ""); s->flags = g_strdup(p[0] && p[1] && p[2] && p[3] ? p[3] : ""); g_ptr_array_add(W.saved, s); g_strfreev(p); }
		else if (!strcmp(k, "wlan")) W.nwlan++;
		else if (!strcmp(k, "wpa_state")) { g_free(W.state); W.state = g_strdup(v); } else if (!strcmp(k, "ssid")) { g_free(W.ssid); W.ssid = wf_decode(v); }
		else if (!strcmp(k, "ip_address")) { g_free(W.ip); W.ip = g_strdup(v); } else if (!strcmp(k, "RSSI")) { g_free(W.rssi); W.rssi = g_strdup(v); }
		else if (!strcmp(k, "LINKSPEED")) { g_free(W.speed); W.speed = g_strdup(v); } else if (!strcmp(k, "country")) { g_free(W.country); W.country = g_strdup(v); }
		else if (!strcmp(k, "adapter")) { g_free(W.adapter); W.adapter = g_strdup(v); } }
	g_strfreev(L);
}
static int w_on(void) { return W.state && !strcmp(W.state, "COMPLETED"); }
static int saved_has(const char *n) { for (guint i = 0; W.saved && i < W.saved->len; i++) if (!strcmp(((WSaved *)g_ptr_array_index(W.saved, i))->ssid, n)) return 1; return 0; }
typedef struct { char *n, *f; int s, fr; } WNet;
static GPtrArray *NETS;
static void wn_free(gpointer p) { WNet *x = p; g_free(x->n); g_free(x->f); g_free(x); }
static int wn_cmp(gconstpointer a, gconstpointer b)
{
	const WNet *x = *(WNet **)a, *y = *(WNet **)b; int cx = W.ssid && w_on() && !strcmp(x->n, W.ssid), cy = W.ssid && w_on() && !strcmp(y->n, W.ssid);
	if (cx != cy) return cy - cx; int sx = saved_has(x->n), sy = saved_has(y->n); if (sx != sy) return sy - sx; return y->s - x->s;
}
static void scan_parse(const char *raw)
{
	if (NETS) g_ptr_array_unref(NETS); NETS = g_ptr_array_new_with_free_func(wn_free); char **L = g_strsplit(raw ? raw : "", "\n", -1);
	for (int i = 1; L[0] && L[i]; i++) { char **p = g_strsplit(L[i], "\t", -1); if (g_strv_length(p) >= 5 && *p[4]) { char *n = wf_decode(p[4]); int s = atoi(p[2]); WNet *hit = NULL;
			for (guint j = 0; j < NETS->len && !hit; j++) if (!strcmp(((WNet *)g_ptr_array_index(NETS, j))->n, n)) hit = g_ptr_array_index(NETS, j);
			if (!hit) { hit = g_new0(WNet, 1); hit->n = n; hit->s = -1000; g_ptr_array_add(NETS, hit); } else g_free(n);
			if (s > hit->s) { hit->s = s; g_free(hit->f); hit->f = g_strdup(p[3]); hit->fr = atoi(p[1]); } } g_strfreev(p); }
	g_strfreev(L); g_ptr_array_sort(NETS, wn_cmp);
}
static const char *wf_kind(const char *f) { if (strstr(f, "EAP")) return "eap"; if (strstr(f, "OWE") && !strstr(f, "PSK") && !strstr(f, "SAE")) return "owe"; if (strstr(f, "PSK") || strstr(f, "SAE")) return "psk"; if (strstr(f, "WEP")) return "wep"; return "open"; }

/* ================================================================== drawing (the page's boxes) */
/* (one line, cut with … at w px: white-space nowrap, text-overflow ellipsis) */
static void ell(cairo_t *cr, const char *t, double x, double y, double px, int wt, double w)
{
	PangoLayout *l = s_layout(t, px, wt, 0); pango_layout_set_width(l, (int)(w * s_z() * PANGO_SCALE)); pango_layout_set_ellipsize(l, PANGO_ELLIPSIZE_END);
	cairo_move_to(cr, x * s_z(), y * s_z()); pango_cairo_show_layout(cr, l); g_object_unref(l);
}
static RuiJ *pjson(const char *path) { char *out = NULL; int c = rui_http("POST", path, NULL, 0, &out, NULL); RuiJ *j = c == 200 && out ? rui_jparse(out) : NULL; set_note("POST %s %d", path, c); g_free(out); return j; }
static void bars(cairo_t *cr, double x, double y, double dbm)
{
	/* (wfBars: four bars 4px wide, 2px apart, 6 to 18 px tall, from the bottom; on: #2563eb, else --bd) */
	int n = dbm >= -55 ? 4 : dbm >= -67 ? 3 : dbm >= -75 ? 2 : 1; double z = s_z(); cairo_save(cr); cairo_scale(cr, z, z);
	for (int i = 1; i <= 4; i++) { double h = i * 4 + 2; if (i <= n) cairo_set_source_rgb(cr, 0x25 / 255.0, 0x63 / 255.0, 0xeb / 255.0); else { cairo_restore(cr); s_col(cr, "bd", 1); cairo_save(cr); cairo_scale(cr, z, z); }
		s_rrect(cr, x + (i - 1) * 6, y + 18 - h, 4, h, 1); cairo_fill(cr); }
	cairo_restore(cr);
}
/* a box with a 1px border and a 4px one at the left (.wfcur, .sysucard) */
static void card(cairo_t *cr, double W_, double H, double r, double lr, double lg, double lb)
{
	double z = s_z(); cairo_save(cr); cairo_scale(cr, z, z);
	cairo_save(cr); s_rrect(cr, 0, 0, W_, H, r); cairo_clip(cr); cairo_set_source_rgb(cr, lr, lg, lb); cairo_rectangle(cr, 0, 0, 4, H); cairo_fill(cr); cairo_restore(cr);
	cairo_restore(cr); s_col(cr, "bd", 1); cairo_save(cr); cairo_scale(cr, z, z); cairo_set_line_width(cr, 1);
	cairo_new_path(cr); cairo_arc(cr, r, .5 + r - .5, r - .5, G_PI, 1.5 * G_PI); cairo_line_to(cr, W_ - r, .5); cairo_arc(cr, W_ - r, r, r - .5, 1.5 * G_PI, 2 * G_PI);
	cairo_line_to(cr, W_ - .5, H - r); cairo_arc(cr, W_ - r, H - r, r - .5, 0, .5 * G_PI); cairo_line_to(cr, r, H - .5); cairo_stroke(cr); cairo_restore(cr);
}
/* a button of the page's Wi-Fi parts (36 px tall, 0 14px) */
static GtkWidget *big(const char *l, int pri) { GtkWidget *b = rui_button(l, pri); p_put(b); return b; }
static double big_w(GtkWidget *b) { return p_nat_w(b) - 20 + 28; }
static double btn_lay(void *d, double x, double y, double w) { (void)w; p_place(d, x, y, 0, 0); return p_nat_h(d); }

/* ================================================================== Wi-Fi: the network in use (.wfcur) */
typedef struct { GtkWidget *a, *b; } Cur;
static double cur_lay(void *d, double x, double y, double w)
{
	Cur *c = d; double h = 1 + 10 + 46 + 10 + 1; p_place(c->a, x, y, w, h);
	if (c->b) { double bw = big_w(c->b); p_place(c->b, x + w - 1 - 12 - bw, y + 1 + 10 + 5, bw, 36); } return h;
}
static gboolean cur_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)d; double z = s_z(), Wd = gtk_widget_get_allocated_width(w) / z, H = gtk_widget_get_allocated_height(w) / z; int on = w_on();
	if (on) card(cr, Wd, H, 10, 0x16 / 255.0, 0xa3 / 255.0, 0x4a / 255.0); else card(cr, Wd, H, 10, 0x94 / 255.0, 0xa3 / 255.0, 0xb8 / 255.0);
	double x = 4 + 12, y = 1 + 10;
	if (on) bars(cr, x, y + 14, W.rssi ? atof(W.rssi) : -70); else { s_col(cr, "fg", 1); s_text(cr, "📶", x, y + 10, 22, 400, 0, 26); }
	double tx = x + (on ? 22 : 29) + 12; s_col(cr, "fg", 1);
	s_text(cr, on ? (W.ssid ? W.ssid : "") : rui_tr("Not connected"), tx, y, 15, 700, 0, s_lh(15, 700));
	char *m = on ? (W.ip && *W.ip ? g_strdup_printf("Connected · %s%s%s%s", W.ip, W.speed ? " · " : "", W.speed ? W.speed : "", W.speed ? " Mbit/s" : "") : g_strdup_printf("Connected · getting an address…%s%s%s", W.speed ? " · " : "", W.speed ? W.speed : "", W.speed ? " Mbit/s" : ""))
		: g_strdup(W.saved && W.saved->len ? "Saved networks are joined automatically when they are in range." : "Choose a network below.");
	s_col(cr, "mut", 1); s_text(cr, rui_tr(m), tx, y + 28, 12, 400, Wd - tx - 140, s_lh(12, 400)); g_free(m); return TRUE;
}
static void on_wdis(GtkButton *b, gpointer d) { (void)b; (void)d; char m[300]; if (post("/api/wifi/disconnect", NULL, m, sizeof m) != 200) set_notify("Wi-Fi", m, 1); redraw(); }
static gboolean later_redraw(gpointer d) { (void)d; if (ALIVE) redraw(); return G_SOURCE_REMOVE; }
static void on_wrc(GtkButton *b, gpointer d) { (void)b; (void)d; char m[300]; if (post("/api/wifi/reconnect", NULL, m, sizeof m) != 200) set_notify("Wi-Fi", m, 1); else g_timeout_add(3000, later_redraw, NULL); }

/* ---- Networks in range (.wfhd, .wfl: each an .acr grid of 1fr 110px 90px, 8px apart: the bars, the name, its kind, then the button
   alone in the second line, as the page's grid puts the four of them) */
typedef struct { GtkWidget *a, *b; WNet *n; char *kind; int cur; double h1; } WRow;
static double wrow_lay(void *d, double x, double y, double w)
{
	WRow *r = d; double c1 = w - 8 - 110 - 8 - 90; int nl = s_lines(r->kind, 12, 400, 90, NULL); double mh = 10 + MAX(18, nl * s_lh(12, 400));
	r->h1 = MAX(MAX(18, 15), mh); double h = 4 + r->h1 + 8 + 36 + 4 + 1; p_place(r->a, x, y, w, h); p_place(r->b, x, y + 4 + r->h1 + 8, c1, 36); return h;
}
static gboolean wrow_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	WRow *r = d; double z = s_z(), Wd = gtk_widget_get_allocated_width(w) / z, H = gtk_widget_get_allocated_height(w) / z, c1 = Wd - 8 - 110 - 8 - 90;
	if (r->cur) { cairo_save(cr); cairo_scale(cr, z, z); s_rrect(cr, 0, 0, Wd, H, 8); cairo_set_source_rgba(cr, 22 / 255.0, 163 / 255.0, 74 / 255.0, .08); cairo_fill(cr); cairo_restore(cr); }
	bars(cr, 0, 4 + (r->h1 - 18) / 2, r->n->s);
	s_col(cr, "fg", 1); ell(cr, r->n->n, c1 + 8, 4 + (r->h1 - 15) / 2, 13, 600, 110);
	s_col(cr, "mut", 1); s_text(cr, r->kind, c1 + 8 + 110 + 8, 4 + 10, 12, 400, 90, s_lh(12, 400));
	cairo_save(cr); cairo_scale(cr, z, z); s_col(cr, "acr", 1); cairo_rectangle(cr, 0, H - 1, Wd, 1); cairo_fill(cr); cairo_restore(cr); return TRUE;
}
static void wrow_free(gpointer p) { WRow *r = p; g_free(r->kind); g_free(r); }

/* the password form under a network (pwForm), and connecting (connect: wifi/connect, then rudolfd's job followed) */
typedef struct { char ssid[160], kind[16]; int hidden; GtkWidget *name, *pw, *sel, *err; } PwF;
static char OPEN[160];      /* the network whose form is open ("" none; "\x01": a hidden network's) */
static int BUSY;
static guint JOBT;
static char JOBSSID[160];
static gboolean job_tick(gpointer d)
{
	(void)d; static int n; RuiJ *j = rui_get("/api/job"); if (!j) { if (++n > 100) goto done; return G_SOURCE_CONTINUE; }
	if (rui_jbool(j, "running") || strcmp(set_jstr(j, "name", ""), "wifi")) { rui_jfree(j); if (++n > 100) goto done; return G_SOURCE_CONTINUE; }
	{ char *lg = g_strstrip(g_strdup(set_jstr(j, "log", ""))); char *last = strrchr(lg, '\n'); const char *m = last ? last + 1 : lg; int ok = (int)rui_jnum(j, "status", 1) == 0;
		if (ok) { char *t = *m ? g_strdup(m) : g_strdup_printf("Connected to %s", JOBSSID); set_notify("Wi-Fi", t, 0); g_free(t); } else set_notify("Wi-Fi", *m ? m : "Could not connect", 1); g_free(lg); }
	rui_jfree(j);
done:
	n = 0; JOBT = 0; BUSY = 0; redraw(); return G_SOURCE_REMOVE;
}
static void connect_go(const char *ssid, const char *pw, const char *kind, int hidden)
{
	BUSY = 1; OPEN[0] = 0; const char *sec = !strcmp(kind, "saved") ? "saved" : NULL; char sb[24];
	if (!sec) { snprintf(sb, sizeof sb, "%s%s", hidden ? "hidden-" : "", !strcmp(kind, "psk") ? "psk" : !strcmp(kind, "owe") ? "owe" : "open"); sec = sb; }
	char *body = g_strdup_printf("%s\n%s\n%s", ssid, pw ? pw : "", sec); char m[300]; int c = post("/api/wifi/connect", body, m, sizeof m); explicit_bzero(body, strlen(body)); g_free(body);
	if (c != 200) { set_notify("Wi-Fi", m, 1); BUSY = 0; redraw(); return; }
	char *t = g_strdup_printf("Connecting to %s…", ssid); set_notify("Wi-Fi", t, 0); g_free(t); snprintf(JOBSSID, sizeof JOBSSID, "%s", ssid);
	if (!JOBT) JOBT = g_timeout_add(1000, job_tick, NULL); redraw();
}
static void pw_go(GtkWidget *w, gpointer d)
{
	(void)w; PwF *f = d; const char *s = f->hidden ? g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(f->name)))) : g_strdup(f->ssid), *kk = f->sel ? s_select_get(f->sel) : f->kind, *p = gtk_entry_get_text(GTK_ENTRY(f->pw));
	const char *e = !*s ? "Type the network name." : !strcmp(kk, "psk") && (g_utf8_strlen(p, -1) < 8 || g_utf8_strlen(p, -1) > 63) ? "A Wi-Fi password has 8 to 63 characters." : strpbrk(s, "\r\n") || strpbrk(p, "\r\n") ? "Line breaks cannot be used." : NULL;
	if (e) { p_text_set(f->err, rui_tr(e)); set_note("wfe %s", e); set_state_soon(); g_free((char *)s); return; }
	char *ss = g_strdup(s), *pp = g_strdup(!strcmp(kk, "psk") ? p : ""), *k2 = g_strdup(kk); int h = f->hidden; g_free((char *)s);
	connect_go(ss, pp, k2, h); explicit_bzero(pp, strlen(pp)); g_free(ss); g_free(pp); g_free(k2);
}
static void pw_cancel(GtkButton *b, gpointer d) { (void)b; (void)d; OPEN[0] = 0; redraw(); }
static void pw_show(int on, void *d) { gtk_entry_set_visibility(GTK_ENTRY(((PwF *)d)->pw), on); }
static void pw_kind(const char *v, void *d) { PwF *f = d; (void)v; (void)f; redraw(); }
static double pwf_lay(void *d, double x, double y, double w)
{
	/* (.wfpw: 32px in, a 1px border, 10px inside; the name 36 px; the password and Show; the error; Connect and Cancel) */
	PwF *f = d; GtkWidget *box = g_object_get_data(G_OBJECT(f->pw), "box"), *sh = g_object_get_data(G_OBJECT(f->pw), "show"), *go = g_object_get_data(G_OBJECT(f->pw), "go"), *cx = g_object_get_data(G_OBJECT(f->pw), "cancel");
	double ix = x + 32 + 11, iw = w - 32 - 22, cy = y + 4 + 11;
	if (f->name) { p_place(f->name, ix, cy, iw, 36); cy += 36 + 6; }
	double sw = p_nat_w(sh), selw = f->sel ? p_nat_w(f->sel) : 0, pw = iw - 10 - sw - (f->sel ? 10 + selw : 0);
	p_place(f->pw, ix, cy, MAX(180, pw), 36); p_place(sh, ix + MAX(180, pw) + 10, cy + (36 - p_nat_h(sh)) / 2, 0, 0); if (f->sel) p_place(f->sel, ix + MAX(180, pw) + 10 + sw + 10, cy + (36 - p_nat_h(f->sel)) / 2, 0, 0);
	cy += 36 + 10; p_place(f->err, ix, cy, iw, 18); cy += 18 + 14;
	double gw = p_nat_w(go); p_place(go, ix, cy, 0, 36); p_place(cx, ix + gw + 8, cy, 0, 36); cy += 36 + 11;
	p_place(box, x + 32, y + 4, w - 32, cy - y - 4); return cy - y + 10;
}
static gboolean pwf_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)d; double z = s_z(); s_col(cr, "bd", 1); cairo_save(cr); cairo_scale(cr, z, z); cairo_set_line_width(cr, 1); s_rrect(cr, .5, .5, gtk_widget_get_allocated_width(w) / z - 1, gtk_widget_get_allocated_height(w) / z - 1, 7.5); cairo_stroke(cr); cairo_restore(cr); return TRUE; }
static void pw_form(const char *ssid, const char *kind, int hidden)
{
	PwF *f = g_new0(PwF, 1); snprintf(f->ssid, sizeof f->ssid, "%s", ssid); snprintf(f->kind, sizeof f->kind, "%s", kind); f->hidden = hidden;
	GtkWidget *box = gtk_drawing_area_new(); g_signal_connect(box, "draw", G_CALLBACK(pwf_draw), NULL); p_put(box); g_object_set_data_full(G_OBJECT(box), "pwf", f, g_free);
	if (hidden) { f->name = rui_entry("Network name"); p_put(f->name); p_label_named("wfn", f->name); g_signal_connect(f->name, "activate", G_CALLBACK(pw_go), f); }
	f->pw = rui_entry("Password"); gtk_entry_set_visibility(GTK_ENTRY(f->pw), FALSE); p_put(f->pw); p_label_named("wfp", f->pw); g_signal_connect(f->pw, "activate", G_CALLBACK(pw_go), f);
	GtkWidget *sh = rui_check("Show", 0, pw_show, f); p_put(sh);
	if (hidden) { const char *v[2] = { "psk", "open" }, *l[2] = { "WPA2/WPA3 password", "No password" }; f->sel = s_select(v, l, 2, "psk", pw_kind, f); p_put(f->sel); p_label_named("wfk", f->sel); }
	if (!strcmp(kind, "open") || !strcmp(kind, "owe")) { gtk_widget_set_no_show_all(f->pw, TRUE); gtk_widget_set_no_show_all(sh, TRUE); }
	f->err = p_msg_m("", 0, 0, 0); gtk_widget_set_no_show_all(f->err, FALSE);
	GtkWidget *go = rui_button("Connect", 0), *cx = rui_button("Cancel", 0); g_signal_connect(go, "clicked", G_CALLBACK(pw_go), f); g_signal_connect(cx, "clicked", G_CALLBACK(pw_cancel), f); p_put(go); p_put(cx); p_label_named("wfgo", go);
	g_object_set_data(G_OBJECT(f->pw), "box", box); g_object_set_data(G_OBJECT(f->pw), "show", sh); g_object_set_data(G_OBJECT(f->pw), "go", go); g_object_set_data(G_OBJECT(f->pw), "cancel", cx);
	p_custom(pwf_lay, f, 0, 0);
}
static void on_wconn(GtkButton *b, gpointer d)
{
	(void)b; WRow *r = d; const char *k = wf_kind(r->n->f);
	if (!strcmp(k, "eap")) { char *t = g_strdup_printf("%s needs a school or company sign-in (802.1X), which Rudolf OS does not support yet.", r->n->n); set_notify("Wi-Fi", t, 1); g_free(t); return; }
	if (!strcmp(k, "wep")) { char *t = g_strdup_printf("%s uses WEP, an old encryption that is no longer safe and is not supported. Switch the router to WPA2 or WPA3.", r->n->n); set_notify("Wi-Fi", t, 1); g_free(t); return; }
	if (!strcmp(k, "psk") && !saved_has(r->n->n)) { snprintf(OPEN, sizeof OPEN, "%s", r->n->n); redraw(); return; }
	char *n = g_strdup(r->n->n); connect_go(n, "", saved_has(n) ? "saved" : k, 0); g_free(n);
}
static void on_whidden(GtkButton *b, gpointer d) { (void)b; (void)d; snprintf(OPEN, sizeof OPEN, "\x01"); redraw(); }
static void wscan(void) { RuiJ *r = pjson("/api/wifi/scan"); scan_parse(set_jstr(r, "raw", "")); rui_jfree(r); }
static void on_wrefresh(GtkButton *b, gpointer d) { (void)b; (void)d; wscan(); redraw(); }

/* ---- <details> (.sp25 details: a 1px border, 8px round, 6px 10px inside; its summary 600 with the disclosure triangle) */
typedef struct Det Det;
struct Det { GtkWidget *a; char *sum; int open; double (*inner)(Det *d, double x, double y, double w); void (*draw)(Det *d, cairo_t *cr, double w); void (*toggled)(Det *d); void *data; GtkWidget *kids[12]; int nk; char *text; };
static double det_lay(void *p, double x, double y, double w)
{
	Det *d = p; double h = 1 + 6 + s_lh(13, 600) + 6 + 1; if (d->open && d->inner) h = 1 + 6 + s_lh(13, 600) + d->inner(d, x + 11, y + 1 + 6 + s_lh(13, 600), w - 22) + 6 + 1;
	for (int i = 0; i < d->nk; i++) gtk_widget_set_child_visible(d->kids[i], d->open); p_place(d->a, x, y, w, h); return h;
}
static gboolean det_draw(GtkWidget *w, cairo_t *cr, gpointer p)
{
	Det *d = p; double z = s_z(), Wd = gtk_widget_get_allocated_width(w) / z, H = gtk_widget_get_allocated_height(w) / z;
	s_col(cr, "bd", 1); cairo_save(cr); cairo_scale(cr, z, z); cairo_set_line_width(cr, 1); s_rrect(cr, .5, .5, Wd - 1, H - 1, 7.5); cairo_stroke(cr); cairo_restore(cr);
	s_col(cr, "fg", 1); { double tx = 11, ty = 7 + s_lh(13, 600) / 2; cairo_save(cr); cairo_scale(cr, z, z); cairo_new_path(cr);
		if (d->open) { cairo_move_to(cr, tx, ty - 2.5); cairo_line_to(cr, tx + 8, ty - 2.5); cairo_line_to(cr, tx + 4, ty + 4); } else { cairo_move_to(cr, tx + .5, ty - 4); cairo_line_to(cr, tx + 7, ty); cairo_line_to(cr, tx + .5, ty + 4); }
		cairo_close_path(cr); cairo_fill(cr); cairo_restore(cr); s_text(cr, rui_tr(d->sum), tx + 14, 7, 13, 600, 0, s_lh(13, 600)); }
	if (d->open && d->draw) d->draw(d, cr, Wd - 22); return TRUE;
}
static gboolean det_press(GtkWidget *w, GdkEventButton *e, gpointer p)
{
	Det *d = p; if (e->button != 1 || e->type != GDK_BUTTON_PRESS || e->y / s_z() > 1 + 6 + s_lh(13, 600) + 3) return FALSE; gtk_widget_grab_focus(w);
	d->open = !d->open; set_note("details %s %d", d->sum, d->open); if (d->toggled) d->toggled(d); p_relayout(); gtk_widget_queue_draw(w); set_state_soon(); return TRUE;
}
static void det_free(gpointer p) { Det *d = p; g_free(d->sum); g_free(d->text); g_free(d); }
static Det *det(const char *sum, double mt, double mb, const char *name)
{
	Det *d = g_new0(Det, 1); d->sum = g_strdup(sum); d->a = gtk_drawing_area_new(); gtk_widget_set_can_focus(d->a, TRUE); gtk_widget_add_events(d->a, GDK_BUTTON_PRESS_MASK);
	g_object_set_data_full(G_OBJECT(d->a), "det", d, det_free); g_signal_connect(d->a, "draw", G_CALLBACK(det_draw), d); g_signal_connect(d->a, "button-press-event", G_CALLBACK(det_press), d);
	p_put(d->a); if (name) p_label_named(name, d->a); p_custom(det_lay, d, mt, mb); return d;
}
static void det_kid(Det *d, GtkWidget *w) { if (d->nk < 12) { d->kids[d->nk++] = w; p_put(w); } }

/* Radios of this network (wifi/radios, when opened) */
static double rad_inner(Det *d, double x, double y, double w) { (void)x; (void)y; int n = s_lines(d->text ? d->text : "", 12, 400, w, NULL); return 10 + MAX(18, n * s_lh(12, 400)); }
static void rad_draw(Det *d, cairo_t *cr, double w) { s_col(cr, "mut", 1); s_text(cr, d->text ? d->text : "", 11, 1 + 6 + s_lh(13, 600) + 10, 12, 400, w, s_lh(12, 400)); }
static void rad_load(Det *d)
{
	if (!d->open) return; RuiJ *r = rui_get("/api/wifi/radios"); GString *b = g_string_new(NULL); char *ssid = NULL, *bands = NULL, *boff = NULL; GPtrArray *rl = g_ptr_array_new_with_free_func(g_free);
	char **L = g_strsplit(set_jstr(r, "raw", ""), "\n", -1);
	for (int i = 0; L[i]; i++) { char *e = strchr(L[i], '='); if (!e) continue; *e = 0; if (!strcmp(L[i], "radio")) g_ptr_array_add(rl, g_strdup(e + 1)); else if (!strcmp(L[i], "ssid")) ssid = g_strstrip(g_strdup(e + 1)); else if (!strcmp(L[i], "bands")) bands = g_strstrip(g_strdup(e + 1)); else if (!strcmp(L[i], "bandsoff")) boff = g_strstrip(g_strdup(e + 1)); }
	g_strfreev(L);
	{ GString *bs = g_string_new(NULL); char **v = g_strsplit(bands ? bands : "", " ", -1); for (int i = 0; v[i]; i++) if (*v[i]) g_string_append_printf(bs, "%s%s GHz", bs->len ? ", " : "", v[i]); g_strfreev(v);
		GString *os = g_string_new(NULL); v = g_strsplit(boff ? boff : "", " ", -1); for (int i = 0; v[i]; i++) if (*v[i]) g_string_append_printf(os, "%s%s GHz", os->len ? ", " : "", v[i]); g_strfreev(v);
		g_string_append_printf(b, "This computer's Wi-Fi: %s%s%s%s.", bs->len ? bs->str : "its bands are not known", os->len ? " (" : "", os->len ? os->str : "", os->len ? " turned off by an administrator)" : ""); g_string_free(bs, TRUE); g_string_free(os, TRUE); }
	if (!r) { g_string_assign(b, "rudolfd did not answer"); }
	else if (!ssid || !*ssid) g_string_append(b, "\nConnect to a network to see its radios.");
	else if (!rl->len) g_string_append(b, "\nNo radios found yet: press Refresh above to look again.");
	else for (guint i = 0; i < rl->len; i++) { char **p = g_strsplit(g_ptr_array_index(rl, i), "\t", -1); int f = p[0] && p[1] ? atoi(p[1]) : 0; const char *st = p[0] && p[1] && p[2] && p[3] ? p[3] : "";
		const char *band = f >= 5925 ? "6 GHz" : f >= 4900 ? "5 GHz" : "2.4 GHz"; int ch = f >= 5925 ? (int)lround((f - 5950) / 5.0) : f >= 4900 ? (int)lround((f - 5000) / 5.0) : f == 2484 ? 14 : (int)lround((f - 2407) / 5.0);
		g_string_append_printf(b, "\n%s · channel %d · %s · %s", band, ch, !strcmp(st, "inuse") ? "In use" : !strcmp(st, "off") ? "Turned off" : !strcmp(st, "bandoff") ? "This computer's radio is off" : g_str_has_prefix(st, "failed:") ? "Didn't answer" : "Available", p[0] ? p[0] : ""); g_strfreev(p); }
	g_free(d->text); d->text = g_string_free(b, FALSE); g_free(ssid); g_free(bands); g_free(boff); g_ptr_array_unref(rl); rui_jfree(r);
}
/* Country (wifi/country) */
static double cty_inner(Det *d, double x, double y, double w)
{
	const char *t = rui_tr("Which channels and transmit power your Wi-Fi may use depend on the country. Set yours if some networks do not show up.");
	double mh = MAX(18, s_lines(t, 12, 400, w, NULL) * s_lh(12, 400)); double ry = y + 10 + mh; p_place(d->kids[0], x + 140, ry, 80, 0); p_place(d->kids[1], x, ry + 23 + 10, 130, 0); return 10 + mh + 23 + 10 + 23 + 10;
}
static void cty_draw(Det *d, cairo_t *cr, double w)
{
	(void)d; const char *t = rui_tr("Which channels and transmit power your Wi-Fi may use depend on the country. Set yours if some networks do not show up."); double y0 = 1 + 6 + s_lh(13, 600);
	s_col(cr, "mut", 1); s_text(cr, t, 11, y0 + 10, 12, 400, w, s_lh(12, 400)); double mh = MAX(18, s_lines(t, 12, 400, w, NULL) * s_lh(12, 400));
	s_col(cr, "fg", 1); s_text(cr, rui_tr("Country code"), 11, y0 + 10 + mh + 4, 13, 400, 130, s_lh(13, 400));
}
static void on_cty(GtkButton *b, gpointer d)
{
	(void)b; GtkWidget *e = d; char *c = g_ascii_strup(g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(e)))), -1);
	if (strlen(c) != 2 || !g_ascii_isalpha(c[0]) || !g_ascii_isalpha(c[1])) set_notify("Wi-Fi", "A country code is two letters, for example US, GB or DE.", 1);
	else { char p[64]; snprintf(p, sizeof p, "/api/wifi/country?cc=%s", c); post_note("Wi-Fi", p, NULL); }
	g_free(c);
}

/* This computer's Wi-Fi radios (wifi/bands, a menu; wifi/band with the administrator's prompt) */
typedef struct { char b[8]; int on; } Band;
static Band BANDS[4]; static int NBANDS; static char BCUR[8], BNAME[160];
static void bands_load(void)
{
	NBANDS = 0; BCUR[0] = BNAME[0] = 0; RuiJ *r = rui_get("/api/wifi/bands"); if (!r) return; char **L = g_strsplit(set_jstr(r, "raw", ""), "\n", -1);
	for (int i = 0; L[i]; i++) { char *e = strchr(L[i], '='); if (!e) continue; *e = 0; char **p = g_strsplit(e + 1, "\t", -1);
		if (!strcmp(L[i], "band") && NBANDS < 4 && p[0]) { snprintf(BANDS[NBANDS].b, 8, "%s", p[0]); BANDS[NBANDS++].on = !(p[1] && !strcmp(p[1], "off")); }
		else if (!strcmp(L[i], "adapter") && p[0]) { const char *bus = p[1] && p[2] ? p[2] : "", *nm = p[1] && p[2] && p[3] ? p[3] : ""; if (!strcmp(bus, "usb")) snprintf(BNAME, sizeof BNAME, "USB: %s", nm); else snprintf(BNAME, sizeof BNAME, "Built-in%s%s%s", *nm ? " (" : "", nm, *nm ? ")" : ""); }
		else if (!strcmp(L[i], "freq")) { int f = atoi(e + 1); snprintf(BCUR, sizeof BCUR, "%s", f >= 5925 ? "6" : f >= 4900 ? "5" : f ? "2.4" : ""); }
		g_strfreev(p); }
	g_strfreev(L); rui_jfree(r);
}
static char *bands_note(void)
{
	if (!NBANDS) return g_strdup(""); GString *o = g_string_new(NULL); int n = 0; for (int i = 0; i < NBANDS; i++) if (!BANDS[i].on) { g_string_append_printf(o, "%s%s GHz", n++ ? ", " : "", BANDS[i].b); }
	char *t = g_strdup_printf("%s%s%s%s", *BNAME ? BNAME : "", *BNAME ? ": " : "", n ? o->str : "every radio is on.", n ? (n > 1 ? " turned off by an administrator; Rudolf OS does not turn them back on by itself." : " turned off by an administrator; Rudolf OS does not turn it back on by itself.") : "");
	char *r = set_admin() ? t : g_strdup_printf("%s Only an administrator can change this.", t); if (r != t) g_free(t); g_string_free(o, TRUE); return r;
}
static int BANDI;
static void band_go(int ok, void *d)
{
	(void)d; if (!ok) return; Band *x = &BANDS[BANDI]; int on = !x->on; char p[96], m[300]; snprintf(p, sizeof p, "/api/wifi/band?band=%s&on=%s", x->b, on ? "on" : "off");
	int c = post(p, NULL, m, sizeof m); set_notify("Wi-Fi", m, c != 200); if (c == 200) { char *l = g_strdup_printf("%s: %s", ME, m); set_log_evt("sys", l); g_free(l); } bands_load(); redraw();
}
static void band_pick(int i, void *d)
{
	(void)d; if (i < 0 || i >= NBANDS) return; if (!set_admin()) { set_notify("Wi-Fi", "Only an administrator can change which Wi-Fi radios this computer uses.", 1); return; }
	int on = !BANDS[i].on, n = 0; for (int j = 0; j < NBANDS; j++) n += BANDS[j].on;
	if (!on && n <= 1) { set_notify("Wi-Fi", "One of this computer's Wi-Fi radios has to stay on. To stop using Wi-Fi, turn Wi-Fi off.", 1); return; }
	BANDI = i; char *why = on ? g_strdup_printf("Turn on this computer's %s GHz Wi-Fi radio", BANDS[i].b) : g_strdup_printf("Turn off this computer's %s GHz Wi-Fi radio: this computer will not use %s GHz networks or radios until an administrator turns it on again", BANDS[i].b, BANDS[i].b);
	rui_elevate(SW, why, band_go, NULL); set_note("ask band %s %d", BANDS[i].b, on); g_free(why);
}
static void on_bands(GtkButton *b, gpointer d)
{
	(void)d; bands_load(); RuiMenuItem it[4]; char lb[4][96]; int n = NBANDS;
	if (!n) { set_notify("Wi-Fi", "This computer's Wi-Fi radios are not known yet.", 0); return; }
	for (int i = 0; i < n; i++) { snprintf(lb[i], sizeof lb[i], "%s  %s GHz radio%s", BANDS[i].on ? "✓" : "   ", BANDS[i].b, !strcmp(BCUR, BANDS[i].b) ? "   (in use)" : ""); it[i] = (RuiMenuItem){ lb[i], NULL, NULL, !set_admin(), 0 }; }
	GtkAllocation a; gtk_widget_get_allocation(GTK_WIDGET(b), &a); rui_menu(GTK_WIDGET(b), 0, a.height + 4, NULL, it, n, band_pick, NULL); set_note("menu bands %d", n); redraw();
}
static void on_why(GtkButton *b, gpointer d) { (void)b; (void)d; rui_shell(SW, "open blitzen"); set_note("blitzen why wifi"); }

/* ---- Saved networks (.acr.wfrow: the name, Metered (12px label.ck), Forget: tapped twice) */
typedef struct { GtkWidget *a, *ck, *b; char *ssid, *t; int sure; guint t2; } SRow;
static double srow_lay(void *d, double x, double y, double w)
{
	SRow *r = d; double h = 4 + 44 + 4 + 1, c1 = w - 8 - 110 - 8 - 90; p_place(r->a, x, y, w, h); p_place(r->ck, x + c1 + 8, y + 4 + (44 - 19) / 2, 104, 19); p_place(r->b, x + c1 + 8 + 110 + 8, y + 4 + 4, 90, 36); return h;   /* (.wfrow: min-height 44px) */
}
static gboolean srow_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	SRow *r = d; double z = s_z(), Wd = gtk_widget_get_allocated_width(w) / z, H = gtk_widget_get_allocated_height(w) / z;
	s_col(cr, "fg", 1); s_text(cr, r->t, 0, 4 + (44 - 15) / 2, 13, 400, Wd - 216, s_lh(13, 400));
	cairo_save(cr); cairo_scale(cr, z, z); s_col(cr, "acr", 1); cairo_rectangle(cr, 0, H - 1, Wd, 1); cairo_fill(cr); cairo_restore(cr); return TRUE;
}
static void srow_free(gpointer p) { SRow *r = p; if (r->t2) g_source_remove(r->t2); g_free(r->ssid); g_free(r->t); g_free(r); }
/* (appMetered: the Wi-Fi networks marked metered, the page's bd95metered list) */
static int metered_has(const char *ssid) { RuiJ *m = set_ls_get("bd95metered"); RuiJ *a = rui_jprop(m, "x"); int r = 0; for (int i = 0; i < rui_jlen(a) && !r; i++) r = !g_strcmp0(rui_jsval(rui_jat(a, i)), ssid); rui_jfree(m); return r; }
static void metered_set(const char *ssid, int on)
{
	RuiJ *m = set_ls_get("bd95metered"); RuiJ *a = rui_jprop(m, "x"); GString *b = g_string_new("["); int n = 0;
	for (int i = 0; i < rui_jlen(a); i++) { const char *v = rui_jsval(rui_jat(a, i)); if (!v || !strcmp(v, ssid)) continue; char *e = set_jesc(v); g_string_append_printf(b, "%s\"%s\"", n++ ? "," : "", e); g_free(e); }
	if (on) { char *e = set_jesc(ssid); g_string_append_printf(b, "%s\"%s\"", n++ ? "," : "", e); g_free(e); }
	g_string_append_c(b, ']'); set_ls_set("bd95metered", b->str); set_note("metered %s %d", ssid, on); g_string_free(b, TRUE); rui_jfree(m);
}
static void on_mt(int on, void *d) { metered_set(((SRow *)d)->ssid, on); }
/* (label.ck.wfmt: the check box 4px in, 3px down, its word in 12px after it, 19px tall) */
typedef struct { int on; void (*cb)(int, void *); void *d; char *t; } Mck;
static gboolean mck_draw(GtkWidget *w, cairo_t *cr, gpointer p) { Mck *m = p; s_ckbox(cr, 4, 3, m->on, gtk_widget_is_sensitive(w)); s_col(cr, "fg", 1); s_text(cr, m->t, 4 + 13 + 3, (19 - s_lh(12, 400)) / 2, 12, 400, 0, s_lh(12, 400)); return TRUE; }
static gboolean mck_press(GtkWidget *w, GdkEventButton *e, gpointer p) { Mck *m = p; if (e->button != 1 || e->type != GDK_BUTTON_PRESS) return FALSE; gtk_widget_grab_focus(w); m->on = !m->on; gtk_widget_queue_draw(w); set_state_soon(); if (m->cb) m->cb(m->on, m->d); return TRUE; }
static void mck_free(gpointer p) { Mck *m = p; g_free(m->t); g_free(m); }
static GtkWidget *mck(const char *t, int on, void (*cb)(int, void *), void *d)
{
	Mck *m = g_new0(Mck, 1); m->t = g_strdup(rui_tr(t)); m->on = on; m->cb = cb; m->d = d; GtkWidget *w = gtk_drawing_area_new(); gtk_widget_set_can_focus(w, TRUE); gtk_widget_add_events(w, GDK_BUTTON_PRESS_MASK);
	g_object_set_data_full(G_OBJECT(w), "mck", m, mck_free); g_signal_connect(w, "draw", G_CALLBACK(mck_draw), m); g_signal_connect(w, "button-press-event", G_CALLBACK(mck_press), m); return w;
}
static gboolean unsure(gpointer d) { SRow *r = d; r->t2 = 0; r->sure = 0; gtk_button_set_label(GTK_BUTTON(r->b), rui_tr("Forget")); gtk_style_context_remove_class(gtk_widget_get_style_context(r->b), "pri"); return G_SOURCE_REMOVE; }
static void on_forget(GtkButton *b, gpointer d)
{
	SRow *r = d; if (!r->sure) { r->sure = 1; gtk_button_set_label(b, rui_tr("Tap again to forget")); gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(b)), "pri"); r->t2 = g_timeout_add(4000, unsure, r); set_note("forget sure %s", r->ssid); return; }
	post_note("Wi-Fi", "/api/wifi/forget", r->ssid); redraw();
}

/* ================================================================== Network hardware (.tmt.nh: four columns, auto layout) */
typedef struct { GtkWidget *a; char *c[8][4]; int n; } Tab;
static void tab_cols(Tab *t, double w, double *cw)
{
	double mc[4] = { 0 }, s = 0; for (int r = 0; r < t->n; r++) for (int c = 0; c < 4; c++) { double tw = 0; s_lines(t->c[r][c], 13, 400, 0, &tw); mc[c] = MAX(mc[c], tw + 12); }
	for (int c = 0; c < 4; c++) s += mc[c]; for (int c = 0; c < 4; c++) cw[c] = s > 0 ? mc[c] + (w - s) * mc[c] / s : w / 4;
}
static double tab_lay(void *d, double x, double y, double w) { Tab *t = d; double h = t->n ? t->n * 23.5 + .5 * (t->n > 1) : 0; p_place(t->a, x, y, w, MAX(1, h)); return h; }
static gboolean tab_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Tab *t = d; double z = s_z(), Wd = gtk_widget_get_allocated_width(w) / z, cw[4]; tab_cols(t, Wd, cw);
	for (int r = 0; r < t->n; r++) { double y = r * 23.75, x = 0; s_col(cr, "fg", 1); for (int c = 0; c < 4; c++) { s_text(cr, t->c[r][c], x + 6, y + 4, 13, 400, 0, s_lh(13, 400)); x += cw[c]; }
		cairo_save(cr); cairo_scale(cr, z, z); cairo_set_source_rgba(cr, 0, 0, 0, .05); if (RUI.dark) cairo_set_source_rgba(cr, 0, 0, 0, .05); cairo_rectangle(cr, 0, y + 22.75, Wd, 1); cairo_fill(cr); cairo_restore(cr); }
	return TRUE;
}
static void tab_free(gpointer p) { Tab *t = p; for (int r = 0; r < t->n; r++) for (int c = 0; c < 4; c++) g_free(t->c[r][c]); g_free(t); }

/* ================================================================== Manual address, Proxy */
static GtkWidget *NIF, *NAD, *NGW, *NDN, *NPX;
static void on_static(GtkButton *b, gpointer d)
{
	(void)b; (void)d; char *a = g_uri_escape_string(gtk_entry_get_text(GTK_ENTRY(NAD)), NULL, FALSE), *g = g_uri_escape_string(gtk_entry_get_text(GTK_ENTRY(NGW)), NULL, FALSE), *n = g_uri_escape_string(gtk_entry_get_text(GTK_ENTRY(NDN)), NULL, FALSE);
	char *p = g_strdup_printf("/api/net/static?if=%s&addr=%s&gw=%s&dns=%s", s_select_get(NIF), a, g, n); post_note("Network", p, NULL); g_free(p); g_free(a); g_free(g); g_free(n); redraw();
}
static void on_dhcp(GtkButton *b, gpointer d) { (void)b; (void)d; char *p = g_strdup_printf("/api/net/dhcp?if=%s", s_select_get(NIF)); post_note("Network", p, NULL); g_free(p); redraw(); }
static void on_proxy(GtkButton *b, gpointer d) { (void)b; (void)d; char *t = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(NPX)))), *e = g_uri_escape_string(t, NULL, FALSE), *p = g_strdup_printf("/api/proxy?server=%s", e); post_note("Proxy", p, NULL); g_free(t); g_free(e); g_free(p); }


/* ================================================================== Web search and Downloads (NETW/NETC: bd95net's search and dl; settings31.js
   hides their headings and words, but the page's .r2 rows and footer keep their display over [hidden]: drawn as they appear) */
typedef struct { char prov[16], url[400], key[200], relay[8], relayUrl[400]; int n; char exe[8]; int max; } NetCfg;
static NetCfg NC, NW;   /* applied (NETC), working copy (NETW) */
static const NetCfg ND = { "wikipedia", "", "", "auto", "https://api.allorigins.win/raw?url={urlenc}", 10, "ask", 4096 };
static void cpy(char *d, size_t n, const char *s) { snprintf(d, n, "%s", s ? s : ""); }
static void nc_load(void)
{
	memset(&NC, 0, sizeof NC); cpy(NC.prov, sizeof NC.prov, ND.prov); cpy(NC.relay, sizeof NC.relay, ND.relay); cpy(NC.relayUrl, sizeof NC.relayUrl, ND.relayUrl); NC.n = ND.n; cpy(NC.exe, sizeof NC.exe, ND.exe); NC.max = ND.max;
	RuiJ *v = set_ls_get("bd95net"); RuiJ *x = rui_jprop(v, "x"), *se = rui_jprop(x, "search"), *dl = rui_jprop(x, "dl");
	if (rui_jtype(se) == 5) { const char *t; RuiJ *n;
		if ((t = rui_jstr(se, "prov"))) cpy(NC.prov, sizeof NC.prov, t); if ((t = rui_jstr(se, "url"))) cpy(NC.url, sizeof NC.url, t); if ((t = rui_jstr(se, "key"))) cpy(NC.key, sizeof NC.key, t);
		if ((t = rui_jstr(se, "relay"))) cpy(NC.relay, sizeof NC.relay, t); if ((t = rui_jstr(se, "relayUrl"))) cpy(NC.relayUrl, sizeof NC.relayUrl, t);
		if ((n = rui_jprop(se, "n")) && rui_jtype(n) == 2) NC.n = (int)rui_jval(n); }
	if (rui_jtype(dl) == 5) { const char *e = rui_jstr(dl, "exe"); if (e && (!strcmp(e, "ask") || !strcmp(e, "block"))) cpy(NC.exe, sizeof NC.exe, e);
		int m = (int)rui_jnum(dl, "max", 0); static const int OK[] = { 100, 500, 1024, 2048, 4096, 8192, 16384 }; for (guint k = 0; k < G_N_ELEMENTS(OK); k++) if (m == OK[k]) NC.max = m; }
	rui_jfree(v);
}
static int NWSET;
static int nw_dirty(void) { return memcmp(&NC, &NW, sizeof NC) != 0; }
static GtkWidget *SV[8];   /* prov url key n relay relayUrl exe dlmax */
static GtkWidget *NSM;
static int NOSAVE;         /* (the rows being filled in: not the person's change) */
static void nsm_say(const char *t, int ok)
{
	char *e = g_markup_escape_text(rui_tr(t), -1), *m = g_strdup_printf("\x03<span foreground=\"%s\">%s</span>", ok ? "#15803d" : "#b91c1c", e); if (NSM) p_text_set(NSM, *t ? m : "");
	g_free(e); g_free(m); set_note("nsm %s", t); set_state_soon();
}
static void nw_sel(const char *v, void *d)
{
	int k = GPOINTER_TO_INT(d); if (!set_admin() || NOSAVE) return;
	if (k == 0) cpy(NW.prov, sizeof NW.prov, v); else if (k == 3) NW.n = atoi(v); else if (k == 4) cpy(NW.relay, sizeof NW.relay, v); else if (k == 6) cpy(NW.exe, sizeof NW.exe, v); else if (k == 7) NW.max = atoi(v);
	p_footer_update();
}
static void nw_ent(GtkEditable *e, gpointer d)
{
	int k = GPOINTER_TO_INT(d); const char *t = gtk_entry_get_text(GTK_ENTRY(e)); if (!set_admin() || NOSAVE) return;
	if (k == 1) cpy(NW.url, sizeof NW.url, t); else if (k == 2) cpy(NW.key, sizeof NW.key, t); else if (k == 5) cpy(NW.relayUrl, sizeof NW.relayUrl, t); p_footer_update();
}
static void nw_show(void)
{
	char n[8], mx[8]; snprintf(n, sizeof n, "%d", NW.n); snprintf(mx, sizeof mx, "%d", NW.max); if (!SV[0]) return; NOSAVE = 1;
	s_select_set(SV[0], NW.prov); gtk_entry_set_text(GTK_ENTRY(SV[1]), NW.url); gtk_entry_set_text(GTK_ENTRY(SV[2]), NW.key); s_select_set(SV[3], n); s_select_set(SV[4], NW.relay); gtk_entry_set_text(GTK_ENTRY(SV[5]), NW.relayUrl); s_select_set(SV[6], NW.exe); s_select_set(SV[7], mx);
	NOSAVE = 0;
}
static int https_q(const char *u) { return !g_ascii_strncasecmp(u, "https://", 8) && u[8] && !strpbrk(u + 8, " \t\n"); }
static char *nc_json(void)
{
	/* (NETC written as netSave writes it: the connections and the active one as they are kept, search and dl as applied) */
	RuiJ *v = set_ls_get("bd95net"); RuiJ *x = rui_jprop(v, "x"); GString *b = g_string_new("{\"conns\":");
	if (rui_jtype(rui_jprop(x, "conns")) == 4) set_jw(b, rui_jprop(x, "conns")); else g_string_append(b, "[{\"id\":\"eth0\",\"name\":\"Ethernet\",\"type\":\"wired\",\"ssid\":\"\",\"sec\":\"open\",\"pw\":\"\",\"auto\":true,\"ipm\":\"dhcp\",\"ip\":\"\",\"mask\":\"\",\"gw\":\"\",\"dns\":\"\"}]");
	g_string_append(b, ",\"active\":"); RuiJ *a = rui_jprop(x, "active"); if (a) set_jw(b, a); else g_string_append(b, "\"eth0\"");
	char *e[5] = { set_jesc(NC.prov), set_jesc(NC.url), set_jesc(NC.key), set_jesc(NC.relay), set_jesc(NC.relayUrl) };
	g_string_append_printf(b, ",\"search\":{\"prov\":\"%s\",\"url\":\"%s\",\"key\":\"%s\",\"relay\":\"%s\",\"relayUrl\":\"%s\",\"n\":%d},\"dl\":{\"exe\":\"%s\",\"max\":%d}}", e[0], e[1], e[2], e[3], e[4], NC.n, NC.exe, NC.max);
	for (int i = 0; i < 5; i++) g_free(e[i]); rui_jfree(v); return g_string_free(b, FALSE);
}
void net_foot(int what)
{
	if (what == 1) { NW = NC; nw_show(); nsm_say("", 1); p_footer_update(); return; }
	if (what == 2) { NetCfg k = NW; cpy(k.prov, sizeof k.prov, ND.prov); cpy(k.url, sizeof k.url, ND.url); cpy(k.key, sizeof k.key, ND.key); cpy(k.relay, sizeof k.relay, ND.relay); cpy(k.relayUrl, sizeof k.relayUrl, ND.relayUrl);
		k.n = ND.n; cpy(k.exe, sizeof k.exe, ND.exe); k.max = ND.max; NW = k; nw_show(); p_footer_note("Search and download options reset. Apply to keep them."); return; }
	if (!strcmp(NW.prov, "custom") && !(https_q(NW.url) && strstr(NW.url, "{q}"))) { nsm_say("The custom search address must start with https:// and contain {q}.", 0); return; }
	if (*NW.relayUrl && !(https_q(NW.relayUrl) && (strstr(NW.relayUrl, "{urlenc}") || strstr(NW.relayUrl, "{url}")))) { nsm_say("The relay address must start with https:// and contain {urlenc}.", 0); return; }
	NC = NW; char *j = nc_json(); set_ls_set("bd95net", j); g_free(j); char *l = g_strdup_printf("%s changed network settings", ME); set_log_evt("sys", l); g_free(l);
	p_footer_note("Network settings applied."); nsm_say("", 1);
}
int net_dirty(void) { return set_admin() && nw_dirty(); }
/* Test search (netSearch, asked by this program itself: a relay is a browser's workaround, not needed here) */
typedef struct { NetCfg c; int ok, n; char msg[300]; } Ts;
static gboolean ts_done(gpointer p)
{
	Ts *t = p; char b[300]; if (t->ok) snprintf(b, sizeof b, "%d result(s) from %s (direct).", t->n, !strcmp(t->c.prov, "ddg") ? "DuckDuckGo" : !strcmp(t->c.prov, "custom") ? "the search service" : "Wikipedia");
	nsm_say(t->ok ? b : t->msg, t->ok); g_free(t); return G_SOURCE_REMOVE;
}
static gpointer ts_thread(gpointer p)
{
	Ts *t = p; char *u = NULL;
	if (!strcmp(t->c.prov, "wikipedia")) u = g_strdup_printf("https://en.wikipedia.org/w/api.php?action=query&list=search&srsearch=Rudolf&srlimit=%d&format=json&origin=*", t->c.n);
	else if (!strcmp(t->c.prov, "ddg")) u = g_strdup("https://api.duckduckgo.com/?q=Rudolf&format=json&no_html=1&skip_disambig=1");
	else { GString *g = g_string_new(t->c.url); char *k = g_uri_escape_string(t->c.key, NULL, FALSE), *n = g_strdup_printf("%d", t->c.n); const char *R[][2] = { { "{q}", "Rudolf" }, { "{key}", k }, { "{n}", n } };
		for (int i = 0; i < 3; i++) { char *f; while ((f = strstr(g->str, R[i][0]))) { gssize o = f - g->str; g_string_erase(g, o, strlen(R[i][0])); g_string_insert(g, o, R[i][1]); } } u = g_string_free(g, FALSE); g_free(k); g_free(n); }
	char *body = NULL; int st = set_https_get(u, &body, t->msg, sizeof t->msg);
	if (st == 200 && body) { RuiJ *j = rui_jparse(body); RuiJ *a = rui_jprop(rui_jprop(j, "query"), "search"); if (!a) a = rui_jprop(j, "RelatedTopics"); if (!a) a = rui_jprop(j, "results"); if (!a) a = rui_jprop(j, "items");
		t->n = rui_jlen(a); t->ok = j != NULL; if (!j) snprintf(t->msg, sizeof t->msg, "The search service did not answer with JSON."); rui_jfree(j); }
	else if (st && !*t->msg) snprintf(t->msg, sizeof t->msg, "The search service answered %d.", st);
	else if (!*t->msg) snprintf(t->msg, sizeof t->msg, "The search service could not be reached.");
	g_free(body); g_free(u); g_idle_add(ts_done, t); return NULL;
}
static void on_test(GtkButton *b, gpointer d) { (void)b; (void)d; Ts *t = g_new0(Ts, 1); t->c = NW; nsm_say("Searching…", 1); g_thread_unref(g_thread_new("search", ts_thread, t)); }
/* (input type=password, no rule of the page's: Chromium's own field, 1px 2px, 21px tall) */
static void plain_entry(GtkWidget *e)
{
	/* (light: a 1px #767676 border; dark: the page's dark input, a 2px inset border: black at the top and left, #6581a9 at the bottom and right) */
	GtkCssProvider *c = gtk_css_provider_new(); double z = s_z(); char *css = RUI.dark
		? g_strdup_printf("entry { padding: 0 %.2fpx; min-height: 0; border-style: solid; border-width: %.2fpx; border-color: #000 #6581a9 #6581a9 #000; border-radius: 0; background: #0b1220; box-shadow: none; }", 1 * z, 2 * z)
		: g_strdup_printf("entry { padding: %.2fpx %.2fpx; min-height: 0; border: %.2fpx solid #767676; border-radius: %.2fpx; background: #ffffff; box-shadow: none; }", 1 * z, 2 * z, 1 * z, 2 * z);
	gtk_css_provider_load_from_data(c, css, -1, NULL); gtk_style_context_add_provider(gtk_widget_get_style_context(e), GTK_STYLE_PROVIDER(c), GTK_STYLE_PROVIDER_PRIORITY_USER + 10); g_object_unref(c); g_free(css);
}
void net_search_rows(const char *pfx)
{
	if (!NWSET) { nc_load(); NW = NC; NWSET = 1; }
	int adm = set_admin(); char nm[24], n[8], mx[8]; snprintf(n, sizeof n, "%d", NW.n); snprintf(mx, sizeof mx, "%d", NW.max);
	const char *pv[3] = { "wikipedia", "ddg", "custom" }, *pl[3] = { "Wikipedia", "DuckDuckGo Instant Answers", "Custom JSON search API" };
	const char *nv[3] = { "5", "10", "20" }, *rv[3] = { "auto", "always", "off" }, *rl[3] = { "Automatic (use if the direct request fails)", "Always use the relay", "Never use a relay" };
	const char *ev[2] = { "ask", "block" }, *el[2] = { "Ask for permission (administrator approval)", "Always block" };
	const char *mv[7] = { "100", "500", "1024", "2048", "4096", "8192", "16384" }, *ml[7] = { "100 MB", "500 MB", "1 GB", "2 GB", "4 GB", "8 GB", "16 GB" };
	NOSAVE = 1;
	Cell *c = p_r2("Search service"); SV[0] = s_select(pv, pl, 3, NW.prov, nw_sel, GINT_TO_POINTER(0)); c_add(c, SV[0], 0, 0, 1); p_r2_more(c);
	c = p_r2("Custom address"); SV[1] = rui_entry("https://search.example/search?q={q}&format=json"); gtk_entry_set_text(GTK_ENTRY(SV[1]), NW.url); c_add(c, SV[1], 0, 0, 1); p_r2_more(c);
	c = p_r2("API key"); SV[2] = rui_entry("Used for {key} in the address"); gtk_entry_set_visibility(GTK_ENTRY(SV[2]), FALSE); plain_entry(SV[2]); gtk_entry_set_text(GTK_ENTRY(SV[2]), NW.key); c_add(c, SV[2], 0, 0, 1); p_r2_more(c);
	c = p_r2("Results"); SV[3] = s_select(nv, nv, 3, n, nw_sel, GINT_TO_POINTER(3)); c_add(c, SV[3], 0, 0, 1); p_r2_more(c);
	c = p_r2("CORS relay"); SV[4] = s_select(rv, rl, 3, NW.relay, nw_sel, GINT_TO_POINTER(4)); c_add(c, SV[4], 0, 0, 1); p_r2_more(c);
	c = p_r2("Relay address"); SV[5] = rui_entry("https://relay.example/?url={urlenc}"); gtk_entry_set_text(GTK_ENTRY(SV[5]), NW.relayUrl); c_add(c, SV[5], 0, 0, 1); p_r2_more(c);
	c = p_r2("Test"); GtkWidget *t = rui_button("Test search", 0); g_signal_connect(t, "clicked", G_CALLBACK(on_test), NULL); c_add(c, t, 0, 0, 0); snprintf(nm, sizeof nm, "%snts", pfx); p_label_named(nm, t);
	c = p_r2("Program files"); SV[6] = s_select(ev, el, 2, NW.exe, nw_sel, GINT_TO_POINTER(6)); c_add(c, SV[6], 0, 0, 1); p_r2_more(c);
	c = p_r2("Largest download"); SV[7] = s_select(mv, ml, 7, mx, nw_sel, GINT_TO_POINTER(7)); c_add(c, SV[7], 0, 0, 1);
	static const char *N[8] = { "prov", "url", "key", "n", "relay", "relayUrl", "exe", "dlmax" };
	for (int i = 0; i < 8; i++) { if (!adm) gtk_widget_set_sensitive(SV[i], FALSE); if (i == 1 || i == 2 || i == 5) g_signal_connect(SV[i], "changed", G_CALLBACK(nw_ent), GINT_TO_POINTER(i));
		snprintf(nm, sizeof nm, "%s%s", pfx, N[i]); p_label_named(nm, SV[i]); }
	NOSAVE = 0;
}
void net_search_msg(void) { NSM = p_msg("", 0); p_label_named("nsm", NSM); }
void net_search_forget(void) { NWSET = 0; }

/* ================================================================== Metered connection (mobile58.js: net/metered) */
static void on_mtr(const char *v, void *d)
{
	(void)d; RuiJ *j = rui_get("/api/net/metered"); char *i = g_uri_escape_string(set_jstr(j, "if", ""), NULL, FALSE), *p = g_strdup_printf("/api/net/metered?if=%s&v=%s", i, v), m[300];
	if (post(p, NULL, m, sizeof m) != 200) set_notify("Network", m, 1); g_free(i); g_free(p); rui_jfree(j); redraw();
}
static char *mtr_fmt(double b) { return b >= 1e9 ? g_strdup_printf("%.2f GB", b / 1e9) : b >= 1e6 ? g_strdup_printf("%.1f MB", b / 1e6) : g_strdup_printf("%.0f KB", round(b / 1e3)); }
static void metered_part(void)
{
	RuiJ *j = rui_get("/api/net/metered"); p_h4("Metered connection"); char *t; int con = j && rui_jbool(j, "connected");
	if (!con) t = g_strdup(j ? "Not connected." : "Looking…");
	else { const char *kd = set_jstr(j, "kind", ""), *ic = "🌐", *kn = "Network"; for (guint i = 0; i < G_N_ELEMENTS(MTRKIND); i++) if (!strcmp(MTRKIND[i][0], kd)) { ic = MTRKIND[i][1]; kn = MTRKIND[i][2]; }
		const char *key = set_jstr(j, "key", ""); char *nm = !strcmp(kd, "wifi") ? g_strdup(g_str_has_prefix(key, "wifi:") ? key + 5 : key) : g_strdup(*set_jstr(j, "name", "") ? set_jstr(j, "name", "") : set_jstr(j, "if", ""));
		int me = rui_jbool(j, "metered"), man = rui_jbool(j, "manual"); const char *au = set_jstr(j, "auto", "");
		const char *why = man ? "set by you for this network" : !strcmp(au, "modem") ? "a cellular modem" : !strcmp(au, "phone") ? "a phone's tethering" : !strcmp(au, "dhcp") ? "the network says it is metered" : "";
		char *ub = mtr_fmt(rui_jnum(j, "used", 0));
		t = me ? g_strdup_printf("%s %s: %s — metered (%s) · %s used this month", ic, kn, nm, why, ub) : g_strdup_printf("%s %s: %s — not metered%s", ic, kn, nm, man ? " (set by you for this network)" : "");
		g_free(nm); g_free(ub); }
	p_msg(t, 2); g_free(t);
	{ const char *v[3] = { "auto", "on", "off" }, *l[3] = { "Automatic", "Metered", "Not metered" }; const char *cur = con && rui_jbool(j, "manual") ? (rui_jbool(j, "metered") ? "on" : "off") : "auto";
		Cell *c = p_r2("This network"); GtkWidget *s = s_select(v, l, 3, cur, on_mtr, NULL); if (!con) gtk_widget_set_sensitive(s, FALSE); c_add(c, s, 0, 0, 1); p_label_named("mtrsel", s); p_r2_mt(c, 6); }
	p_msg_m("On a metered connection (cellular data with a monthly cap), Rudolf OS downloads nothing large by itself: system updates are found but wait for you, and Linux apps and Debian are not updated in the background. Websites and apps work as usual. Cellular modems and phones' USB tethering are metered automatically, and so are networks that say so (Android hotspots).", 2, 6, 0);
	rui_jfree(j);
}

/* ================================================================== Cellular modems (modem59.js: mobile/status; the FCC unlock, mobile/fcc) */
typedef struct { GtkWidget *a, *b; char *nm, *line, *fcc; double th; } Mdm;
static double mdm_text_h(Mdm *m, double w)
{
	double h = s_lines(m->nm, 13, 700, w, NULL) * s_lh(13, 700) + 10 + MAX(18, s_lines(m->line, 13, 400, w, NULL) * s_lh(13, 400));
	if (m->fcc) h += 10 + MAX(18, s_lines(m->fcc, 13, 400, w, NULL) * s_lh(13, 400)) + (m->b ? p_nat_h(m->b) : 0); return h;
}
static double mdm_lay(void *d, double x, double y, double w)
{
	Mdm *m = d; double c1 = w - 8 - 110 - 8 - 90; m->th = mdm_text_h(m, 110); double h = 4 + MAX(MAX(m->th, 25), 44) + 4 + 1; p_place(m->a, x, y, w, h);
	if (m->b) p_place(m->b, x + c1 + 8, y + 4 + m->th - p_nat_h(m->b), 0, 0); return h;
}
static gboolean mdm_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	Mdm *m = d; double z = s_z(), Wd = gtk_widget_get_allocated_width(w) / z, H = gtk_widget_get_allocated_height(w) / z, c1 = Wd - 8 - 110 - 8 - 90, in = H - 9, tx = c1 + 8;
	s_col(cr, "fg", 1); s_text(cr, "📶", 0, 4 + (in - 25) / 2, 22, 400, 0, 25);
	double ty = 4 + (in - m->th) / 2; s_text(cr, m->nm, tx, ty, 13, 700, 110, s_lh(13, 700)); ty += s_lines(m->nm, 13, 700, 110, NULL) * s_lh(13, 700) + 10;
	s_col(cr, "msg", 1); s_text(cr, m->line, tx, ty, 13, 400, 110, s_lh(13, 400)); ty += MAX(18, s_lines(m->line, 13, 400, 110, NULL) * s_lh(13, 400));
	if (m->fcc) s_text(cr, m->fcc, tx, ty + 10, 13, 400, 110, s_lh(13, 400));
	cairo_save(cr); cairo_scale(cr, z, z); s_col(cr, "acr", 1); cairo_rectangle(cr, 0, H - 1, Wd, 1); cairo_fill(cr); cairo_restore(cr); return TRUE;
}
static void mdm_free(gpointer p) { Mdm *m = p; g_free(m->nm); g_free(m->line); g_free(m->fcc); g_free(m); }
typedef struct { char id[16]; int on; } Fcc;
static void fcc_cred(const char *n, const char *pw, void *d)
{
	Fcc *f = d; if (!n) { g_free(f); return; }
	char *en = set_jesc(n), *ep = set_jesc(pw), *b = g_strdup_printf("{\"n\":\"%s\",\"p\":\"%s\"}", en, ep), *i = g_uri_escape_string(f->id, NULL, FALSE), *path = g_strdup_printf("/api/mobile/fcc?id=%s&on=%d", i, f->on), m[300];
	int c = post(path, b, m, sizeof m); set_notify("📶 Cellular module", c == 200 ? (*m ? m : "Done.") : m, c != 200); explicit_bzero(b, strlen(b)); explicit_bzero(ep, strlen(ep)); g_free(b); g_free(en); g_free(ep); g_free(i); g_free(path); g_free(f); redraw();
}
static void on_fcc(GtkButton *b, gpointer d)
{
	(void)b; Fcc *f = g_memdup2(d, sizeof(Fcc)); char *what = f->on ? g_strdup_printf("Turn on this laptop's cellular module (%s): Rudolf OS will run ModemManager's unlock for it each time it starts", f->id) : g_strdup_printf("Turn off the FCC unlock of the cellular module (%s)", f->id);
	rui_elevate_cred(SW, what, fcc_cred, f); set_note("ask fcc %s %d", f->id, f->on); g_free(what);
}
static char *mdm_line(RuiJ *m)
{
	/* (mdmLine, then its firmware and driver) */
	const char *st = set_jstr(m, "state", ""), *s2 = st; for (guint i = 0; i < G_N_ELEMENTS(MDMSTATE); i++) if (!strcmp(MDMSTATE[i][0], st)) s2 = MDMSTATE[i][1]; if (!*s2) s2 = "—";
	GString *b = g_string_new(rui_tr(s2)); const char *lk = set_jstr(m, "locked", ""), *op = set_jstr(m, "operator", ""), *te = set_jstr(m, "tech", ""), *sg = set_jstr(m, "signal", ""), *fw = set_jstr(m, "firmware", ""), *dr = set_jstr(m, "driver", "");
#define V_(x) ((x) && *(x) && strcmp((x), "--"))
	if (!rui_jbool(m, "sim") || !strcmp(set_jstr(m, "failed", ""), "sim-missing")) g_string_append(b, " · no SIM card");
	else if (V_(lk) && strcmp(lk, "none")) g_string_append_printf(b, " · the SIM needs its %s (%s)", !strcmp(lk, "sim-puk") ? "PUK" : "PIN", lk);
	if (V_(op)) g_string_append_printf(b, " · %s", op);
	if (V_(te)) { char *u = g_ascii_strup(te, -1); GString *t = g_string_new(NULL); for (char *p = u; *p; p++) { if (*p == ',') g_string_append(t, ", "); else g_string_append_c(t, *p); } g_string_append_printf(b, " · %s", t->str); g_string_free(t, TRUE); g_free(u); }
	if (V_(sg) && strcmp(sg, "0") && strcmp(st, "locked") && rui_jbool(m, "sim")) g_string_append_printf(b, " · signal %s%%", sg);
	if (V_(fw)) g_string_append_printf(b, " · firmware %s", fw); if (*dr) g_string_append_printf(b, " · %s", dr);
#undef V_
	return g_string_free(b, FALSE);
}
static void modems_part(void)
{
	RuiJ *s = rui_get("/api/mobile/status"), *ms = rui_jprop(s, "modems"), *ns = rui_jprop(s, "notes"); if (!rui_jlen(ms) && !rui_jlen(ns)) { rui_jfree(s); return; }
	if (rui_jlen(ms)) p_h4("Cellular modems");
	for (int i = 0; i < rui_jlen(ms); i++) { RuiJ *m = rui_jat(ms, i); Mdm *x = g_new0(Mdm, 1); const char *mk = set_jstr(m, "maker", ""), *mo = set_jstr(m, "model", "");
		GString *n = g_string_new(NULL); if (*mk && strcmp(mk, "--")) g_string_append(n, mk); if (*mo && strcmp(mo, "--")) g_string_append_printf(n, "%s%s", n->len ? " " : "", mo); if (!n->len) g_string_append(n, rui_tr("Cellular modem")); x->nm = g_string_free(n, FALSE);
		x->line = mdm_line(m); const char *fc = set_jstr(m, "fcc", ""), *fid = set_jstr(m, "fccId", "");
		x->a = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(x->a), "mdm", x, mdm_free); g_signal_connect(x->a, "draw", G_CALLBACK(mdm_draw), x); p_put(x->a);
		if (!strcmp(fc, "off") || !strcmp(fc, "on")) { int off = !strcmp(fc, "off");
			x->fcc = off ? g_strdup_printf("If its radio stays off: laptop makers lock some cellular modules until the computer unlocks them (the “FCC lock”). Rudolf OS can run ModemManager's unlock for this module (%s) each time it starts, telling it this computer may use its radio. Turn it on only for a module that came with this laptop. An administrator's password is needed.", fid)
				: g_strdup_printf("The radio is unlocked each time the module starts (FCC unlock for %s).", fid);
			Fcc *f = g_new0(Fcc, 1); cpy(f->id, sizeof f->id, fid); f->on = off; x->b = rui_button(off ? "Turn on this laptop's cellular module" : "Turn off the FCC unlock", 0); g_object_set_data_full(G_OBJECT(x->b), "fcc", f, g_free); g_signal_connect(x->b, "clicked", G_CALLBACK(on_fcc), f); p_put(x->b); p_label_named("mdmfcc", x->b); }
		p_custom(mdm_lay, x, 0, 0); }
	if (rui_jlen(ns)) { p_h4("Cellular modules Linux supports only partly");
		for (int i = 0; i < rui_jlen(ns); i++) { RuiJ *n = rui_jat(ns, i); Mdm *x = g_new0(Mdm, 1); x->nm = g_strdup_printf("%s (%s)", set_jstr(n, "name", ""), set_jstr(n, "id", "")); x->line = g_strdup(set_jstr(n, "text", ""));
			x->a = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(x->a), "mdm", x, mdm_free); g_signal_connect(x->a, "draw", G_CALLBACK(mdm_draw), x); p_put(x->a); p_custom(mdm_lay, x, 0, 0); } }
	rui_jfree(s);
}

/* ================================================================== a card (.sysucard: 1px border, 4px at the left, 8px round, 8px 10px inside, 4px above, 8px under) */
typedef struct { GtkWidget *a; int ok; double (*inner)(void *d, double x, double y, double w); void (*draw)(void *d, cairo_t *cr, double w); void *d; } Card;
static double card_lay(void *p, double x, double y, double w) { Card *c = p; double h = 1 + 8 + c->inner(c->d, x + 4 + 10, y + 1 + 8, w - 4 - 10 - 10 - 1) + 8 + 1; p_place(c->a, x, y, w, h); return h; }
static gboolean card_draw(GtkWidget *w, cairo_t *cr, gpointer p)
{
	Card *c = p; double z = s_z(), Wd = gtk_widget_get_allocated_width(w) / z, H = gtk_widget_get_allocated_height(w) / z;
	if (c->ok) card(cr, Wd, H, 8, 0x16 / 255.0, 0xa3 / 255.0, 0x4a / 255.0); else { double r, g, b; if (RUI.dark) { r = 0x33 / 255.0; g = 0x41 / 255.0; b = 0x55 / 255.0; } else { r = 0xd1 / 255.0; g = 0xd5 / 255.0; b = 0xdb / 255.0; } card(cr, Wd, H, 8, r, g, b); }
	cairo_save(cr); cairo_translate(cr, (4 + 10) * z, (1 + 8) * z); if (c->draw) c->draw(c->d, cr, Wd - 4 - 10 - 10 - 1); cairo_restore(cr); return TRUE;
}
static Card *card_add(int ok, double (*inner)(void *, double, double, double), void (*draw)(void *, cairo_t *, double), void *d, const char *name)
{
	Card *c = g_new0(Card, 1); c->ok = ok; c->inner = inner; c->draw = draw; c->d = d; c->a = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(c->a), "card", c, g_free);
	g_signal_connect(c->a, "draw", G_CALLBACK(card_draw), c); p_put(c->a); if (name) p_label_named(name, c->a); p_custom(card_lay, c, 4, 8); return c;
}
/* a line of text asked for (askText), as a small dialog of this window */
static char *ask_text(const char *title, const char *init, int secret)
{
	GtkWidget *dl = gtk_dialog_new_with_buttons(rui_tr(title), GTK_WINDOW(SW->win), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, rui_tr("Cancel"), GTK_RESPONSE_CANCEL, rui_tr("OK"), GTK_RESPONSE_OK, NULL);
	GtkWidget *e = rui_entry(NULL); if (init) gtk_entry_set_text(GTK_ENTRY(e), init); gtk_entry_set_visibility(GTK_ENTRY(e), !secret); gtk_entry_set_activates_default(GTK_ENTRY(e), TRUE); gtk_dialog_set_default_response(GTK_DIALOG(dl), GTK_RESPONSE_OK);
	GtkWidget *ca = gtk_dialog_get_content_area(GTK_DIALOG(dl)); gtk_container_set_border_width(GTK_CONTAINER(ca), 12); gtk_box_pack_start(GTK_BOX(ca), e, FALSE, FALSE, 0); gtk_widget_set_size_request(e, (int)lround(320 * s_z()), -1);
	gtk_widget_show_all(dl); p_label_named("asktext", e); set_note("ask text %s", title); set_state_soon();
	char *r = gtk_dialog_run(GTK_DIALOG(dl)) == GTK_RESPONSE_OK ? g_strdup(gtk_entry_get_text(GTK_ENTRY(e))) : NULL; gtk_widget_destroy(dl); return r;
}

/* ================================================================== Share this connection (hotspot59.js: net/hotspot, an administrator's) */
static RuiJ *HS;
static GtkWidget *HSN, *HSP, *HSB;
static char HSPW[16];
static void hs_rand(char *o) { const char *a = "abcdefghjkmnpqrstuvwxyz23456789"; guint32 r[12]; for (int i = 0; i < 12; i++) { r[i] = g_random_int(); o[i] = a[r[i] % strlen(a)]; } o[12] = 0; }
static double hsl_inner(void *d, double x, double y, double w) { (void)d; (void)x; (void)y; const char *t = g_object_get_data(G_OBJECT(HSN ? HSN : SW->win), "hsl"); return MAX(15, s_lines(t ? t : "", 13, 400, w, NULL) * s_lh(13, 400)); }
static char *HSL;
static double hsl_in(void *d, double x, double y, double w) { (void)d; (void)x; (void)y; return MAX(15, s_lines(HSL, 13, 400, w, NULL) * s_lh(13, 400)); }
static void hsl_draw(void *d, cairo_t *cr, double w) { (void)d; s_col(cr, "fg", 1); s_text(cr, HSL, 0, 0, 13, 400, w, s_lh(13, 400)); }
static void on_dice(GtkButton *b, gpointer d) { (void)b; (void)d; hs_rand(HSPW); gtk_entry_set_text(GTK_ENTRY(HSP), HSPW); }
typedef struct { int on; char n[64], p[80], b[8]; } Hs;
static void hs_go(int ok, void *d)
{
	Hs *h = d; if (ok) { char m[400]; int c; if (!h->on) c = post("/api/net/hotspot?on=0", NULL, m, sizeof m); else { char *body = g_strdup_printf("%s\n%s\n%s", h->n, h->p, h->b); c = post("/api/net/hotspot", body, m, sizeof m); explicit_bzero(body, strlen(body)); g_free(body); }
		set_notify("Share this connection", m, c != 200); }
	explicit_bzero(h, sizeof *h); g_free(h); redraw();
}
static void on_hst(GtkButton *b, gpointer d)
{
	(void)b; (void)d; int on = !rui_jbool(HS, "on"); Hs *h = g_new0(Hs, 1); h->on = on;
	if (!on) { rui_elevate(SW, "Stop sharing this computer’s connection", hs_go, h); set_note("ask hotspot off"); return; }
	char *n = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(HSN)))); const char *p = gtk_entry_get_text(GTK_ENTRY(HSP)); int pl = g_utf8_strlen(p, -1);
	const char *e = !*n || g_utf8_strlen(n, -1) > 32 ? "The network name has 1 to 32 characters." : pl && (pl < 8 || pl > 63) ? "The password has 8 to 63 characters." : !pl && !rui_jbool(HS, "saved") ? "Choose a password (8 to 63 characters)." : NULL;
	if (e) { set_notify("Share this connection", e, 1); g_free(n); g_free(h); return; }
	cpy(h->n, sizeof h->n, n); cpy(h->p, sizeof h->p, p); cpy(h->b, sizeof h->b, s_select_get(HSB));
	char *why = g_strdup_printf("Share this computer’s connection as the Wi-Fi network %s", n); rui_elevate(SW, why, hs_go, h); set_note("ask hotspot on %s", n); g_free(why); g_free(n);
}
static void hotspot_part(void)
{
	rui_jfree(HS); HS = rui_get("/api/net/hotspot"); if (!HS) return; int on = rui_jbool(HS, "on"), adm = set_admin();
	p_h4("Share this connection");
	g_free(HSL); if (on) { int dv = (int)rui_jnum(HS, "devices", 0); HSL = g_strdup_printf("\x03<b>On</b> · %s · %d device%s connected · sharing %s%s%s%s", set_jstr(HS, "name", ""), dv, dv == 1 ? "" : "s", set_jstr(HS, "uplink", ""), *set_jstr(HS, "band", "") ? " · " : "", set_jstr(HS, "band", ""), *set_jstr(HS, "band", "") ? " GHz" : ""); }
	else if (*set_jstr(HS, "why", "")) { char *e = g_markup_escape_text(set_jstr(HS, "why", ""), -1); HSL = g_strdup_printf("\x03<b>Off</b> · %s", e); g_free(e); }
	else HSL = g_strdup("\x03<b>Off</b> · other devices can use this computer’s internet connection through its Wi-Fi.");
	card_add(on, hsl_in, hsl_draw, NULL, "hsl");
	if (!on) {
		Cell *c = p_r2("Network name"); HSN = rui_entry(NULL); gtk_entry_set_max_length(GTK_ENTRY(HSN), 32); { char r[16]; if (*set_jstr(HS, "name", "")) gtk_entry_set_text(GTK_ENTRY(HSN), set_jstr(HS, "name", "")); else { hs_rand(r); r[4] = 0; char *t = g_strdup_printf("Rudolf-%u", g_random_int_range(1000, 9999)); gtk_entry_set_text(GTK_ENTRY(HSN), t); g_free(t); } }
		c_add(c, HSN, 0, 0, 1); p_label_named("hsn", HSN); p_r2_more(c);
		c = p_r2("Password"); HSP = rui_entry(rui_jbool(HS, "saved") ? "(the saved password)" : "8 to 63 characters"); gtk_entry_set_max_length(GTK_ENTRY(HSP), 63); if (!rui_jbool(HS, "saved")) { hs_rand(HSPW); gtk_entry_set_text(GTK_ENTRY(HSP), HSPW); }
		GtkWidget *dice = rui_button("🎲", 0); g_signal_connect(dice, "clicked", G_CALLBACK(on_dice), NULL); p_label_named("hsg", dice); c_add(c, HSP, 0, 0, 2); c_add(c, dice, 6, 0, 0); p_label_named("hsp", HSP); p_r2_more(c);
		const char *bv[2] = { "2.4", "5" }, *bl[2] = { "2.4 GHz (reaches further, every device)", "5 GHz (faster)" }; c = p_r2("Band"); HSB = s_select(bv, bl, 2, !strcmp(set_jstr(HS, "band", ""), "5") ? "5" : "2.4", NULL, NULL); c_add(c, HSB, 0, 0, 1); p_label_named("hsb", HSB); }
	else HSN = HSP = HSB = NULL;
	{ GtkWidget *t = rui_button(on ? "Turn off" : "Turn on", 1); g_signal_connect(t, "clicked", G_CALLBACK(on_hst), NULL); gtk_widget_set_sensitive(t, adm && (on || !*set_jstr(HS, "why", ""))); p_put(t); p_label_named("hst", t); p_custom(btn_lay, t, 14, 0); }
	if (!adm) p_msg("Only an administrator can share this computer's connection.", 0);
	(void)hsl_inner;
}

/* ================================================================== VPN (vpn62.js: vpn, vpn/on, vpn/off, vpn/add, vpn/set, vpn/remove) */
static RuiJ *VS; static char VBUSY[64];
static const char *vpn_state(RuiJ *v) { if (*VBUSY && !strcmp(VBUSY, set_jstr(v, "id", ""))) return rui_jbool(v, "on") ? "Turning off…" : "Connecting…"; const char *s = set_jstr(v, "state", ""); return !rui_jbool(v, "on") ? "Off" : !strcmp(s, "connected") ? "Connected" : !strcmp(s, "noanswer") ? "No answer" : !strcmp(s, "down") ? "Down" : "Connecting…"; }
static RuiJ *vpn_by_id(const char *id) { RuiJ *a = rui_jprop(VS, "vpns"); for (int i = 0; i < rui_jlen(a); i++) if (!strcmp(set_jstr(rui_jat(a, i), "id", ""), id)) return rui_jat(a, i); return NULL; }
static int vpn_others(RuiJ *v) { const char *me = set_jstr(rui_jprop(VS, "me"), "name", ""), *by = set_jstr(v, "by", ""); return rui_jbool(v, "on") && *by && *me && strcmp(by, me); }
typedef struct { char id[64], what[16], val[200]; int on; } VAct;
static void vpn_send(VAct *a, const char *n, const char *pw)
{
	char m[400]; int c; RuiJ *v = vpn_by_id(a->id); char nm[200]; cpy(nm, sizeof nm, set_jstr(v, "name", ""));
	char *en = n ? set_jesc(n) : NULL, *ep = pw ? set_jesc(pw) : NULL, *body = NULL;
	if (!strcmp(a->what, "toggle")) { char path[160]; if (a->on) snprintf(path, sizeof path, "/api/vpn/off"); else { char *i = g_uri_escape_string(a->id, NULL, FALSE); snprintf(path, sizeof path, "/api/vpn/on?id=%s", i); g_free(i); }
		if (n) body = g_strdup_printf("{\"n\":\"%s\",\"p\":\"%s\"}", en, ep); cpy(VBUSY, sizeof VBUSY, a->id); c = post(path, body, m, sizeof m); *VBUSY = 0;
		if (c == 200) { char *t = a->on ? g_strdup_printf("%s is off.", nm) : *m ? g_strdup(m) : g_strdup_printf("%s is on.", nm); set_notify("VPN", t, 0); g_free(t); } else set_notify("VPN", m, 1); }
	else { char *ei = set_jesc(a->id);
		if (!strcmp(a->what, "remove")) body = g_strdup_printf("{\"n\":\"%s\",\"p\":\"%s\",\"id\":\"%s\"}", en, ep, ei);
		else if (!strcmp(a->what, "kill")) body = g_strdup_printf("{\"n\":\"%s\",\"p\":\"%s\",\"id\":\"%s\",\"kill\":%s}", en, ep, ei, a->on ? "true" : "false");
		else { char *ev = set_jesc(a->val); body = g_strdup_printf("{\"n\":\"%s\",\"p\":\"%s\",\"id\":\"%s\",\"%s\":\"%s\"}", en, ep, ei, a->what, ev); g_free(ev); }
		c = post(!strcmp(a->what, "remove") ? "/api/vpn/remove" : "/api/vpn/set", body, m, sizeof m); g_free(ei);
		if (c == 200) { char *t = !strcmp(a->what, "remove") ? g_strdup_printf("%s was removed.", nm) : g_strdup("Saved."); set_notify("VPN", t, 0); g_free(t); } else set_notify("VPN", m, 1); }
	if (body) { explicit_bzero(body, strlen(body)); g_free(body); } if (ep) { explicit_bzero(ep, strlen(ep)); g_free(ep); } g_free(en);
}
static void vpn_cred(const char *n, const char *p, void *d) { VAct *a = d; if (n) vpn_send(a, n, p); explicit_bzero(a, sizeof *a); g_free(a); redraw(); }
static void vpn_elev(VAct *a, const char *why) { rui_elevate_cred(SW, why, vpn_cred, a); set_note("ask vpn %s %s", a->what, a->id); }
static void vpn_toggle_2(VAct *a)
{
	RuiJ *v = vpn_by_id(a->id); if (!v) { g_free(a); return; } int on = rui_jbool(v, "on"); const char *act = set_jstr(VS, "active", ""); RuiJ *cur = !on && *act && strcmp(act, a->id) ? vpn_by_id(act) : NULL;
	if ((on ? !rui_jbool(v, "mayOff") : !rui_jbool(v, "mayOn")) || (cur && !rui_jbool(cur, "mayOff"))) { char *w = g_strdup_printf("%s%s", on ? "Turn off the VPN " : "Turn on the VPN ", set_jstr(v, "name", "")); vpn_elev(a, w); g_free(w); return; }
	vpn_send(a, NULL, NULL); g_free(a); redraw();
}
static void vpn_ask2(int i, void *d) { VAct *a = d; if (i != 1) { g_free(a); return; } vpn_toggle_2(a); }
static void vpn_ask1(int i, void *d)
{
	VAct *a = d; if (i != 1) { g_free(a); return; } RuiJ *v = vpn_by_id(a->id); const char *act = set_jstr(VS, "active", ""); RuiJ *cur = !rui_jbool(v, "on") && *act && strcmp(act, a->id) ? vpn_by_id(act) : NULL;
	if (cur && !rui_jbool(cur, "mayOff") && strcmp(set_jstr(cur, "policy", ""), "admins")) { char *t = g_strdup_printf("%s is on and cannot be turned off: %s", set_jstr(cur, "name", ""), set_jstr(cur, "why", "")); set_notify("VPN", t, 1); g_free(t); g_free(a); return; }
	if (cur && vpn_others(cur)) { char *ti = g_strdup_printf("Turn off %s?", set_jstr(cur, "name", "")), *tx = g_strdup_printf("%s's VPN is on for everyone on this computer. Turning on %s turns it off for %s too.", set_jstr(cur, "by", ""), set_jstr(v, "name", ""), set_jstr(cur, "by", "")), *y = g_strdup_printf("Turn on %s", set_jstr(v, "name", ""));
		const char *B[2] = { "Cancel", y }; rui_confirm(SW, ti, tx, B, 2, 1, vpn_ask2, a); set_note("ask vpn others"); g_free(ti); g_free(tx); g_free(y); return; }
	vpn_toggle_2(a);
}
static void on_vpnb(GtkButton *b, gpointer d)
{
	(void)b; RuiJ *v = vpn_by_id(d); if (!v || *VBUSY) return; VAct *a = g_new0(VAct, 1); cpy(a->id, sizeof a->id, d); cpy(a->what, sizeof a->what, "toggle"); a->on = rui_jbool(v, "on");
	if (a->on && !rui_jbool(v, "mayOff") && strcmp(set_jstr(v, "policy", ""), "admins")) { set_notify("VPN", *set_jstr(v, "why", "") ? set_jstr(v, "why", "") : "This VPN cannot be turned off.", 1); g_free(a); return; }
	if (a->on && vpn_others(v)) { char *ti = g_strdup_printf("Turn off %s?", set_jstr(v, "name", "")), *tx = g_strdup_printf("%s's VPN is on for everyone on this computer. Turning it off turns it off for %s too.", set_jstr(v, "by", ""), set_jstr(v, "by", ""));
		const char *B[2] = { "Cancel", "Turn off" }; rui_confirm(SW, ti, tx, B, 2, 1, vpn_ask1, a); set_note("ask vpn off others"); g_free(ti); g_free(tx); return; }
	vpn_ask1(1, a);
}
static void vpn_admin(const char *id, const char *what, const char *val, int on)
{
	RuiJ *v = vpn_by_id(id); if (!v) return; VAct *a = g_new0(VAct, 1); cpy(a->id, sizeof a->id, id); cpy(a->what, sizeof a->what, what); cpy(a->val, sizeof a->val, val); a->on = on; const char *nm = set_jstr(v, "name", "");
	char *why = !strcmp(what, "name") ? g_strdup_printf("Rename the VPN %s", nm) : !strcmp(what, "policy") ? g_strdup_printf("Change who may turn the VPN %s on and off", nm) : !strcmp(what, "kill") ? g_strdup_printf("%s the kill switch of the VPN %s", on ? "Turn on" : "Turn off", nm) : g_strdup_printf("Remove the VPN %s", nm);
	vpn_elev(a, why); g_free(why);
}
static void on_vpnp(const char *v, void *d) { RuiJ *x = vpn_by_id(d); if (x && strcmp(v, set_jstr(x, "policy", ""))) vpn_admin(d, "policy", v, 0); }
static void on_vpnk(int on, void *d) { vpn_admin(d, "kill", "", on); }
static void on_vpnr(GtkButton *b, gpointer d)
{
	(void)b; RuiJ *v = vpn_by_id(d); if (!v) return; char *t = g_strdup_printf("A new name for %s", set_jstr(v, "name", "")); char *n = ask_text(t, NULL, 0); g_free(t); if (!n) return; g_strstrip(n);
	if (!*n) { g_free(n); return; } if (g_utf8_strlen(n, -1) > 64) set_notify("VPN", "The name has 1 to 64 characters.", 1); else vpn_admin(d, "name", n, 0); g_free(n);
}
static void vpnx_ok(int i, void *d) { if (i == 1) vpn_admin(d, "remove", "", 0); g_free(d); }
static void on_vpnx(GtkButton *b, gpointer d)
{
	(void)b; RuiJ *v = vpn_by_id(d); if (!v) return; char *ti = g_strdup_printf("Remove %s?", set_jstr(v, "name", "")), *tx = g_strdup_printf("Its file and its key are removed from this computer%s. You can add it again from its file.", rui_jbool(v, "on") ? ", and it is turned off" : "");
	const char *B[2] = { "Cancel", "Remove" }; rui_confirm(SW, ti, tx, B, 2, 1, vpnx_ok, g_strdup(d)); set_note("ask vpn remove"); g_free(ti); g_free(tx);
}
/* Add a VPN… (vpnAdd: the file, its name, an OpenVPN file's login, then the administrator's name and password) */
typedef struct { char *conf, *name, *type, *user, *pass; } VAdd;
static void vadd_free(VAdd *a) { if (a->conf) explicit_bzero(a->conf, strlen(a->conf)); if (a->pass) explicit_bzero(a->pass, strlen(a->pass)); g_free(a->conf); g_free(a->name); g_free(a->type); g_free(a->user); g_free(a->pass); g_free(a); }
static void vadd_cred(const char *n, const char *p, void *d)
{
	VAdd *a = d; if (n) { char *en = set_jesc(n), *ep = set_jesc(p), *nm = set_jesc(a->name), *cf = set_jesc(a->conf), *ty = set_jesc(a->type);
		GString *b = g_string_new(NULL); g_string_printf(b, "{\"n\":\"%s\",\"p\":\"%s\",\"name\":\"%s\",\"conf\":\"%s\",\"type\":\"%s\"", en, ep, nm, cf, ty);
		if (a->user) { char *u = set_jesc(a->user), *pp = set_jesc(a->pass); g_string_append_printf(b, ",\"user\":\"%s\",\"pass\":\"%s\"", u, pp); explicit_bzero(pp, strlen(pp)); g_free(u); g_free(pp); } g_string_append_c(b, '}');
		char *out = NULL; int c = rui_http("POST", "/api/vpn/add", b->str, b->len, &out, NULL); RuiJ *j = out ? rui_jparse(out) : NULL; set_note("POST /api/vpn/add %d", c);
		if (c == 200) { GString *t = g_string_new(NULL); g_string_printf(t, "%s was added.", a->name); RuiJ *ns = rui_jprop(j, "notes"); for (int i = 0; i < rui_jlen(ns); i++) g_string_append_printf(t, " %s", rui_jsval(rui_jat(ns, i))); set_notify("VPN", t->str, 0); g_string_free(t, TRUE); }
		else set_notify("VPN", set_jstr(j, "error", "It could not be added."), 1);
		explicit_bzero(b->str, b->len); g_string_free(b, TRUE); explicit_bzero(ep, strlen(ep)); explicit_bzero(cf, strlen(cf)); g_free(en); g_free(ep); g_free(nm); g_free(cf); g_free(ty); rui_jfree(j); g_free(out); }
	vadd_free(a); redraw();
}
static const char *vpn_kind(const char *t)
{
	GRegex *wg = g_regex_new("^\\s*\\[Interface\\]", G_REGEX_CASELESS | G_REGEX_MULTILINE, 0, NULL), *cl = g_regex_new("^\\s*client\\s*$", G_REGEX_MULTILINE, 0, NULL), *re = g_regex_new("^\\s*remote\\s", G_REGEX_MULTILINE, 0, NULL);
	const char *k = g_regex_match(wg, t, 0, NULL) ? "wireguard" : g_regex_match(cl, t, 0, NULL) && g_regex_match(re, t, 0, NULL) ? "openvpn" : ""; g_regex_unref(wg); g_regex_unref(cl); g_regex_unref(re); return k;
}
static void on_vpna(GtkButton *b, gpointer d)
{
	(void)b; (void)d; char *f = NULL; const char *pick = getenv("RUDOLF_SET_PICK");
	if (pick) f = g_strdup(pick);
	else { GtkFileChooserNative *ch = gtk_file_chooser_native_new(rui_tr("Add a VPN"), GTK_WINDOW(SW->win), GTK_FILE_CHOOSER_ACTION_OPEN, rui_tr("Open"), rui_tr("Cancel")); GtkFileFilter *ff = gtk_file_filter_new(); gtk_file_filter_add_pattern(ff, "*.conf"); gtk_file_filter_add_pattern(ff, "*.ovpn"); gtk_file_filter_set_name(ff, "WireGuard or OpenVPN file"); gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(ch), ff);
		if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(ch)) == GTK_RESPONSE_ACCEPT) f = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(ch)); g_object_unref(ch); }
	if (!f) return; char *conf = NULL; gsize n = 0; GStatBuf sb;
	if (g_stat(f, &sb) || sb.st_size > 65536) { set_notify("VPN", "The file is over 64 KB: it is not a VPN file.", 1); g_free(f); return; }
	if (!g_file_get_contents(f, &conf, &n, NULL)) { set_notify("VPN", "The file could not be read.", 1); g_free(f); return; }
	const char *k = vpn_kind(conf); if (!*k) { set_notify("VPN", "This is not a WireGuard file ([Interface]) or an OpenVPN client's file (client, remote).", 1); explicit_bzero(conf, n); g_free(conf); g_free(f); return; }
	char *base = g_path_get_basename(f), *dot = strrchr(base, '.'); if (dot && (!g_ascii_strcasecmp(dot, ".conf") || !g_ascii_strcasecmp(dot, ".ovpn"))) *dot = 0; if (g_utf8_strlen(base, -1) > 64) *g_utf8_offset_to_pointer(base, 64) = 0;
	char *nm = getenv("RUDOLF_SET_PICK") ? g_strdup(base) : ask_text("Give it a name", base, 0); g_free(base); g_free(f); if (!nm || !*g_strstrip(nm)) { g_free(nm); explicit_bzero(conf, n); g_free(conf); return; }
	VAdd *a = g_new0(VAdd, 1); a->conf = conf; a->name = nm; a->type = g_strdup(k);
	GRegex *au = g_regex_new("^\\s*auth-user-pass\\s*$", G_REGEX_MULTILINE, 0, NULL); int login = !strcmp(k, "openvpn") && g_regex_match(au, conf, 0, NULL); g_regex_unref(au);
	if (login) { a->user = ask_text("Username", NULL, 0); a->pass = a->user ? ask_text("Password", NULL, 1) : NULL; if (!a->user || !a->pass || !*a->user || !*a->pass) { set_notify("VPN", "This VPN signs in with a username and password: type them.", 1); vadd_free(a); return; } }
	char *why = g_strdup_printf("Add the VPN %s", nm); rui_elevate_cred(SW, why, vadd_cred, a); set_note("ask vpn add %s", nm); g_free(why);
}
/* (a VPN's card: its name, state and button; what it is; who turned it on; who may, Rename, Remove; the kill switch) */
typedef struct { GtkWidget *b, *sel, *rn, *rm, *ck; char *nm, *st, *in, *by, *kt; int adm; } VCard;
static double kill_h(VCard *v, double w) { return MAX(17, s_lines(v->kt, 13, 400, w, NULL) * s_lh(13, 400)) + 4; }
static double vc_inner(void *d, double x, double y, double w)
{
	VCard *v = d; double bw = p_nat_w(v->b), bh = p_nat_h(v->b); p_place(v->b, x + w - bw, y, 0, 0); double cy = y + bh;
	cy += 4 + MAX(18, s_lines(v->in, 13, 400, w, NULL) * s_lh(13, 400)); cy += 4 + MAX(18, s_lines(v->by, 13, 400, w, NULL) * s_lh(13, 400));
	if (v->adm) { cy += 6; double lw = 0; s_lines(rui_tr("Who may turn it on and off"), 13, 400, 0, &lw); double sw = p_nat_w(v->sel), rw = p_nat_w(v->rn), mw = p_nat_w(v->rm), sh = p_nat_h(v->sel), lx = x + lw + 6;
		p_place(v->sel, lx, cy, 0, 0); double ax = lx + sw + 6, ly = cy, lh = sh;
		if (ax + rw <= x + w) { p_place(v->rn, ax, cy + (sh - p_nat_h(v->rn)) / 2, 0, 0); ax += rw + 6; } else { ly += lh + 6; ax = x; p_place(v->rn, ax, ly, 0, 0); ax += rw + 6; lh = p_nat_h(v->rn); }
		if (ax + mw <= x + w) p_place(v->rm, ax, ly + (lh - p_nat_h(v->rm)) / 2, 0, 0); else { ly += lh + 6; p_place(v->rm, x, ly, 0, 0); lh = p_nat_h(v->rm); }
		cy = ly + lh; cy += 6; p_place(v->ck, x, cy, w, kill_h(v, w)); cy += kill_h(v, w); }
	return cy - y;
}
static void vc_draw(void *d, cairo_t *cr, double w)
{
	VCard *v = d; double bw = p_nat_w(v->b), bh = p_nat_h(v->b), sw = 0; s_lines(v->st, 13, 400, 0, &sw);
	s_col(cr, "fg", 1); s_text(cr, v->nm, 0, (bh - 15) / 2, 13, 700, w - bw - 8 - sw - 8, 15); s_text(cr, v->st, w - bw - 8 - sw, (bh - 15) / 2, 13, 400, 0, 15);
	double cy = bh + 4; s_col(cr, "msg", 1); s_text(cr, v->in, 0, cy, 13, 400, w, s_lh(13, 400)); cy += MAX(18, s_lines(v->in, 13, 400, w, NULL) * s_lh(13, 400)) + 4; s_text(cr, v->by, 0, cy, 13, 400, w, s_lh(13, 400));
	cy += MAX(18, s_lines(v->by, 13, 400, w, NULL) * s_lh(13, 400));
	if (v->adm) { double sh = p_nat_h(v->sel); s_text(cr, rui_tr("Who may turn it on and off"), 0, cy + 6 + 4 + (sh - 4 - 18) / 2 + 1.5, 13, 400, 0, s_lh(13, 400)); }
}
static void vc_free(gpointer p) { VCard *v = p; g_free(v->nm); g_free(v->st); g_free(v->in); g_free(v->by); g_free(v->kt); g_free(v); }
/* (the kill switch: an inline check box and its words, wrapping under it) */
typedef struct { int on; void (*cb)(int, void *); void *d; char *t; } Kck;
static gboolean kck_draw(GtkWidget *w, cairo_t *cr, gpointer p)
{
	Kck *k = p; double z = s_z(), W_ = gtk_widget_get_allocated_width(w) / z; s_ckbox(cr, 4, 3, k->on, gtk_widget_is_sensitive(w)); double sp, a1; s_lines("a a", 13, 400, 0, &sp); s_lines("aa", 13, 400, 0, &a1);
	PangoLayout *l = s_layout(k->t, 13, 400, W_); pango_layout_set_indent(l, (int)((4 + 13 + 3 + sp - a1) * z * PANGO_SCALE)); s_col(cr, "fg", 1); cairo_move_to(cr, 0, 1 * z); pango_cairo_show_layout(cr, l); g_object_unref(l); return TRUE;
}
static gboolean kck_press(GtkWidget *w, GdkEventButton *e, gpointer p) { Kck *k = p; if (e->button != 1 || e->type != GDK_BUTTON_PRESS) return FALSE; gtk_widget_grab_focus(w); if (k->cb) k->cb(!k->on, k->d); return TRUE; }
static void kck_free(gpointer p) { Kck *k = p; g_free(k->t); g_free(k); }
static void vpn_part(void)
{
	rui_jfree(VS); VS = rui_get("/api/vpn"); RuiJ *vs = rui_jprop(VS, "vpns"); if (!VS || rui_jtype(vs) != 4) return; int adm = rui_jbool(rui_jprop(VS, "me"), "admin") || set_admin();
	p_h4("VPN");
	p_msg("A VPN for the whole computer: while one is on, every app's traffic and its name lookups go through it.", 0);
	p_msg("Oracle's Tor windows then run inside the VPN: Tor connects through it, and the VPN sees only that Tor is used.", 0);
	if (!rui_jlen(vs)) p_msg("No VPN yet. Add one from your VPN provider's or your home router's WireGuard file.", 0);
	for (int i = 0; i < rui_jlen(vs); i++) { RuiJ *v = rui_jat(vs, i); VCard *c = g_new0(VCard, 1); const char *id = set_jstr(v, "id", ""); char nm[32]; c->adm = adm;
		c->nm = g_strdup(set_jstr(v, "name", "")); c->st = g_strdup(rui_tr(vpn_state(v)));
		if (rui_jbool(v, "broken")) c->in = g_strdup("Its file could not be read: remove it and add it again.");
		else { GString *g = g_string_new(NULL); const char *sv = set_jstr(v, "server", ""); RuiJ *ad = rui_jprop(v, "addresses"), *dn = rui_jprop(v, "dns"); if (*sv) g_string_append_printf(g, "Server %s", sv);
			if (rui_jlen(ad)) { g_string_append_printf(g, "%saddress ", g->len ? " · " : ""); for (int k = 0; k < rui_jlen(ad); k++) g_string_append_printf(g, "%s%s", k ? ", " : "", rui_jsval(rui_jat(ad, k))); }
			if (rui_jlen(dn)) { g_string_append_printf(g, "%sname lookups through ", g->len ? " · " : ""); for (int k = 0; k < rui_jlen(dn); k++) g_string_append_printf(g, "%s%s", k ? ", " : "", rui_jsval(rui_jat(dn, k))); }
			else g_string_append_printf(g, "%sname lookups: the network's own (the file names no DNS server)", g->len ? " · " : "");
			RuiJ *wh = rui_jprop(v, "whole"); if (wh && rui_jtype(wh) == 1 && !rui_jval(wh)) g_string_append(g, " · only its own networks go through it");
			if (rui_jbool(v, "kill") && !adm) g_string_append(g, " · kill switch on"); c->in = g_string_free(g, FALSE); }
		{ GString *g = g_string_new(NULL); const char *why = set_jstr(v, "why", ""), *by = set_jstr(v, "by", "");
			if (rui_jbool(v, "on")) { g_string_append_printf(g, "On for everyone on this computer%s%s%s.", *by ? " (turned on by " : "", by, *by ? ")" : ""); if (!rui_jbool(v, "mayOff") && *why) g_string_append_printf(g, " %s", why); }
			else if (*why && !rui_jbool(v, "mayOn")) g_string_append(g, why);
			if (rui_jbool(v, "on") && rui_jbool(v, "kill") && strcmp(set_jstr(v, "state", ""), "connected")) g_string_append(g, " The kill switch lets nothing out but its server until it is back or turned off.");
			c->by = g_string_free(g, FALSE); }
		c->kt = g_strdup(rui_tr("Kill switch: while this VPN is meant to be on and its tunnel is down, nothing goes out but its server"));
		c->b = rui_button(rui_jbool(v, "on") ? "Disconnect" : "Connect", 1); g_object_set_data_full(G_OBJECT(c->b), "id", g_strdup(id), g_free); g_signal_connect(c->b, "clicked", G_CALLBACK(on_vpnb), g_object_get_data(G_OBJECT(c->b), "id"));
		gtk_widget_set_sensitive(c->b, !*VBUSY && !rui_jbool(v, "broken") && !(rui_jbool(v, "on") && !rui_jbool(v, "mayOff") && strcmp(set_jstr(v, "policy", ""), "admins"))); p_put(c->b); snprintf(nm, sizeof nm, "vpnb%d", i); p_label_named(nm, c->b);
		if (adm) { const char *pv[8], *pl[8]; int np = 0; for (guint k = 0; k < G_N_ELEMENTS(VPNPOL) && np < 8; k++) { pv[np] = VPNPOL[k][0]; pl[np++] = VPNPOL[k][1]; }
			char *idc = g_strdup(id); c->sel = s_select(pv, pl, np, set_jstr(v, "policy", ""), on_vpnp, idc); g_object_set_data_full(G_OBJECT(c->sel), "id", idc, g_free); p_put(c->sel); snprintf(nm, sizeof nm, "vpnp%d", i); p_label_named(nm, c->sel);
			c->rn = rui_button("Rename…", 0); c->rm = rui_button("Remove…", 0); g_object_set_data_full(G_OBJECT(c->rn), "id", g_strdup(id), g_free); g_object_set_data_full(G_OBJECT(c->rm), "id", g_strdup(id), g_free);
			g_signal_connect(c->rn, "clicked", G_CALLBACK(on_vpnr), g_object_get_data(G_OBJECT(c->rn), "id")); g_signal_connect(c->rm, "clicked", G_CALLBACK(on_vpnx), g_object_get_data(G_OBJECT(c->rm), "id")); p_put(c->rn); p_put(c->rm);
			snprintf(nm, sizeof nm, "vpnr%d", i); p_label_named(nm, c->rn); snprintf(nm, sizeof nm, "vpnx%d", i); p_label_named(nm, c->rm);
			Kck *k = g_new0(Kck, 1); k->on = rui_jbool(v, "kill"); k->cb = on_vpnk; k->d = g_strdup(id); k->t = g_strdup(c->kt); c->ck = gtk_drawing_area_new(); gtk_widget_add_events(c->ck, GDK_BUTTON_PRESS_MASK); gtk_widget_set_can_focus(c->ck, TRUE);
			g_object_set_data_full(G_OBJECT(c->ck), "kck", k, kck_free); g_object_set_data_full(G_OBJECT(c->ck), "id", k->d, g_free); g_signal_connect(c->ck, "draw", G_CALLBACK(kck_draw), k); g_signal_connect(c->ck, "button-press-event", G_CALLBACK(kck_press), k); p_put(c->ck); snprintf(nm, sizeof nm, "vpnk%d", i); p_label_named(nm, c->ck); }
		Card *cd = card_add(rui_jbool(v, "on") && !strcmp(set_jstr(v, "state", ""), "connected"), vc_inner, vc_draw, c, NULL); g_object_set_data_full(G_OBJECT(cd->a), "vc", c, vc_free);
		if (adm) { p_custom_find("Who may turn it on and off", 0); } }
	{ GtkWidget *a = rui_button("Add a VPN…", 0); g_signal_connect(a, "clicked", G_CALLBACK(on_vpna), NULL); gtk_widget_set_sensitive(a, adm && !(rui_jtype(rui_jprop(VS, "wireguard")) == 1 && !rui_jbool(VS, "wireguard"))); p_put(a); p_label_named("vpna", a); p_custom(btn_lay, a, 14, 0); }
	if (!adm) p_msg("Only an administrator can add, rename or remove a VPN.", 0);
	if (rui_jtype(rui_jprop(VS, "wireguard")) == 1 && !rui_jbool(VS, "wireguard")) p_msg("This system's kernel has no WireGuard: update Rudolf OS.", 0);
}

/* ("Web search and the download limits are on the Oracle page.": its link, the browser's own (#0000ee, underlined), opens Oracle) */
static const char LNK_A[] = "Web search and the download limits are on the ", LNK_B[] = "Oracle", LNK_C[] = " page.";
static double lnk_lay(void *d, double x, double y, double w) { p_place(d, x, y, w, 18); return 18; }
static gboolean lnk_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	(void)w; (void)d; double a = 0, b = 0; s_lines(rui_tr(LNK_A), 13, 400, 0, &a); s_lines(rui_tr(LNK_B), 13, 400, 0, &b);
	s_col(cr, "grey", 1); s_text(cr, rui_tr(LNK_A), 0, 0, 13, 400, 0, s_lh(13, 400)); s_text(cr, rui_tr(LNK_C), a + b, 0, 13, 400, 0, s_lh(13, 400));
	if (RUI.dark) cairo_set_source_rgb(cr, .62, .62, 1); else cairo_set_source_rgb(cr, 0, 0, .933); s_text(cr, rui_tr(LNK_B), a, 0, 13, 400, 0, s_lh(13, 400));
	double z = s_z(), asc = s_asc(13, 400, NULL); cairo_rectangle(cr, a * z, (asc + 1.5) * z, b * z, MAX(1, round(z))); cairo_fill(cr); return TRUE;
}
static gboolean lnk_press(GtkWidget *w, GdkEventButton *e, gpointer d) { (void)w; (void)d; double a = 0, b = 0, x = e->x / s_z(); s_lines(rui_tr(LNK_A), 13, 400, 0, &a); s_lines(rui_tr(LNK_B), 13, 400, 0, &b); if (e->button == 1 && x >= a && x <= a + b) { set_note("link oracle"); set_show("web"); } return TRUE; }

/* ================================================================== the page */
/* (.wfhd: "Networks in range" in bold at the left, Refresh at the right, 6px above and below) */
static double hd_lay(void *d, double x, double y, double w) { GtkWidget *t = d, *b = g_object_get_data(G_OBJECT(t), "b"); double bh = p_nat_h(b), bw = p_nat_w(b); p_place(t, x, y, w - bw, bh); p_place(b, x + w - bw, y, 0, 0); return bh; }
static gboolean hd_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)d; double H = gtk_widget_get_allocated_height(w) / s_z(); s_col(cr, "fg", 1); s_text(cr, rui_tr("Networks in range"), 0, (H - 15) / 2, 13, 700, 0, 15); return TRUE; }
/* (a .rmrow of one button, 14px under) */
static GtkWidget *row1(const char *l, GCallback cb, const char *name) { GtkWidget *b = rui_button(l, 0); g_signal_connect(b, "clicked", cb, NULL); p_put(b); p_label_named(name, b); p_custom(btn_lay, b, 14, 0); return b; }
/* (.wfbd: the radios' button (36px), its note under it) */
typedef struct { GtkWidget *b, *n; } Bd;
static double bd_lay(void *d, double x, double y, double w) { Bd *b = d; double bw = big_w(b->b); p_place(b->b, x, y, bw, 36); const char *t = g_object_get_data(G_OBJECT(b->n), "t"); double nh = MAX(18, s_lines(t, 12, 400, w, NULL) * s_lh(12, 400)); p_place(b->n, x, y + 36 + 10, w, nh); return 36 + 10 + nh; }
static gboolean bdn_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)d; const char *t = g_object_get_data(G_OBJECT(w), "t"); s_col(cr, "mut", 1); s_text(cr, t, 0, 0, 12, 400, gtk_widget_get_allocated_width(w) / s_z(), s_lh(12, 400)); return TRUE; }

static void wifi_part(void)
{
	{ Cur *c = g_new0(Cur, 1); c->a = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(c->a), "cur", c, g_free); g_signal_connect(c->a, "draw", G_CALLBACK(cur_draw), NULL); p_put(c->a); p_label_named("wfcur", c->a);
		if (w_on()) { c->b = big("Disconnect", 0); g_signal_connect(c->b, "clicked", G_CALLBACK(on_wdis), NULL); p_label_named("wfdis", c->b); }
		else if (W.saved && W.saved->len) { c->b = big("Reconnect", 0); g_signal_connect(c->b, "clicked", G_CALLBACK(on_wrc), NULL); p_label_named("wfrc", c->b); }
		p_custom(cur_lay, c, 0, 10); }
	{ GtkWidget *b = rui_button("↻ Refresh", 0), *t = gtk_drawing_area_new(); g_signal_connect(b, "clicked", G_CALLBACK(on_wrefresh), NULL); p_put(b); p_label_named("wfre", b);
		g_object_set_data(G_OBJECT(t), "b", b); g_signal_connect(t, "draw", G_CALLBACK(hd_draw), NULL); p_put(t); p_custom(hd_lay, t, 6, 6); }
	if (!NETS || !NETS->len) p_msg(NETS ? "No networks found. Check that the router is on and nearby, then press Refresh." : "Looking for networks…", 2);
	for (guint i = 0; NETS && i < NETS->len; i++) {
		WNet *n = g_ptr_array_index(NETS, i); WRow *r = g_new0(WRow, 1); r->n = n; const char *k = wf_kind(n->f); r->cur = W.ssid && w_on() && !strcmp(n->n, W.ssid);
		const char *kt = !strcmp(k, "psk") ? (strstr(n->f, "SAE") ? (strstr(n->f, "PSK") ? "🔒 WPA2/\u2060WPA3" : "🔒 WPA3") : "🔒 WPA2") : !strcmp(k, "eap") ? "🔒 Enterprise" : !strcmp(k, "wep") ? "⚠️ WEP" : !strcmp(k, "owe") ? "Enhanced open" : "Open";
		r->kind = g_strdup_printf("%s%s%s", kt, n->fr >= 5900 ? " · 6 GHz" : n->fr >= 4900 ? " · 5 GHz" : " · 2.4 GHz", saved_has(n->n) && !r->cur ? " · Saved" : "");
		r->a = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(r->a), "wrow", r, wrow_free); g_signal_connect(r->a, "draw", G_CALLBACK(wrow_draw), r); p_put(r->a);
		r->b = rui_button(r->cur ? "Connected" : "Connect", 0); gtk_widget_set_sensitive(r->b, !r->cur && !BUSY); g_signal_connect(r->b, "clicked", G_CALLBACK(on_wconn), r); p_put(r->b);
		char nm[24]; snprintf(nm, sizeof nm, "wfc%u", i); p_label_named(nm, r->b); p_custom(wrow_lay, r, 0, 0);
		if (!strcmp(OPEN, n->n)) pw_form(n->n, k, 0); }
	row1("Join a hidden network…", G_CALLBACK(on_whidden), "wfhid");
	if (OPEN[0] == 1) pw_form("", "psk", 1);
	{ Det *d = det("Radios of this network", 6, 6, "wfrad"); d->inner = rad_inner; d->draw = rad_draw; d->toggled = rad_load; }
	{ Bd *b = g_new0(Bd, 1); b->b = big("📡 This computer's Wi-Fi radios ▾", 0); g_signal_connect(b->b, "clicked", G_CALLBACK(on_bands), NULL); p_label_named("wfbdb", b->b);
		b->n = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(b->n), "t", bands_note(), g_free); g_object_set_data_full(G_OBJECT(b->n), "bd", b, g_free); g_signal_connect(b->n, "draw", G_CALLBACK(bdn_draw), NULL); p_put(b->n); p_custom(bd_lay, b, 6, 6); }
	row1("⚡\uFE0E Why does the Wi-Fi drop?", G_CALLBACK(on_why), "wfwhy");
	p_h4("Saved networks");
	if (!W.saved || !W.saved->len) p_msg("No saved networks yet.", 2);
	for (guint i = 0; W.saved && i < W.saved->len; i++) { WSaved *x = g_ptr_array_index(W.saved, i); SRow *r = g_new0(SRow, 1); r->ssid = g_strdup(x->ssid); r->t = g_strdup_printf("%s%s", x->ssid, strstr(x->flags, "CURRENT") ? "  ✓ connected" : "");
		r->a = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(r->a), "srow", r, srow_free); g_signal_connect(r->a, "draw", G_CALLBACK(srow_draw), r); p_put(r->a);
		r->ck = mck("Metered", metered_has(x->ssid), on_mt, r); p_put(r->ck); p_custom_find("Metered", 0);
		r->b = big("Forget", 0); g_signal_connect(r->b, "clicked", G_CALLBACK(on_forget), r);
		char nm[24]; snprintf(nm, sizeof nm, "wfmt%u", i); p_label_named(nm, r->ck); snprintf(nm, sizeof nm, "wff%u", i); p_label_named(nm, r->b); p_custom(srow_lay, r, 0, 0); }
	{ Det *d = det("Country", 10, 6, "wfadv"); d->inner = cty_inner; d->draw = cty_draw; GtkWidget *e = rui_entry("e.g. US, GB, DE"); gtk_entry_set_max_length(GTK_ENTRY(e), 2); if (W.country) gtk_entry_set_text(GTK_ENTRY(e), W.country);
		det_kid(d, e); p_label_named("wfcc", e); GtkWidget *b = rui_button("Set", 0); g_signal_connect(b, "clicked", G_CALLBACK(on_cty), e); det_kid(d, b); p_label_named("wfccs", b); p_custom_find("Country code", 0); }
}

void page_net(void)
{
	if (!KEEP) { net_search_forget(); rui_jfree(NET); NET = rui_get("/api/net"); RuiJ *s = rui_get("/api/wifi/status"); wst_parse(set_jstr(s, "raw", "")); rui_jfree(s); wscan(); }
	p_h3("Network Settings");
	p_h4("Wi-Fi");
	RuiJ *ifs = rui_jprop(NET, "interfaces"); int nwl = 0; for (int i = 0; i < rui_jlen(ifs); i++) nwl += rui_jbool(rui_jat(ifs, i), "wireless");
	if (!NET) ;
	else if (!nwl) p_msg("No Wi-Fi adapter was found. If your computer has one, it may need a driver Rudolf OS does not include yet (a USB Wi-Fi adapter with a Realtek, MediaTek or Intel chip usually works). Or use a network cable.", 2);
	else if (!rui_jbool(NET, "wifiTools")) p_msg("A Wi-Fi adapter was found, but this system has no Wi-Fi tools. Update Rudolf OS, or use a network cable.", 2);
	else wifi_part();
	p_h4("Network hardware");
	{ Tab *t = g_new0(Tab, 1); for (int i = 0; i < rui_jlen(ifs) && t->n < 8; i++) { RuiJ *x = rui_jat(ifs, i); if (rui_jbool(x, "virtual")) continue; int r = t->n++; const char *ip = set_jstr(x, "ip", "");
			RuiJ *in = rui_jprop(x, "internet"); char *rx = fmt_b(rui_jnum(x, "rx", 0)), *tx = fmt_b(rui_jnum(x, "tx", 0));
			t->c[r][0] = g_strdup_printf("%s%s", rui_jbool(x, "wireless") ? "📶 " : "🔌 ", set_jstr(x, "name", "")); t->c[r][1] = g_strdup(set_jstr(x, "state", ""));
			t->c[r][2] = g_strdup_printf("%s%s", *ip ? ip : "no address", *ip && in && rui_jtype(in) == 1 && !rui_jval(in) ? " · no internet (not used)" : ""); t->c[r][3] = g_strdup_printf("↓%s ↑%s", rx, tx); g_free(rx); g_free(tx); }
		t->a = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(t->a), "tab", t, tab_free); g_signal_connect(t->a, "draw", G_CALLBACK(tab_draw), t); p_put(t->a); p_custom(tab_lay, t, 0, 0); }
	{ char *g = g_strdup_printf("Gateway %s · DNS %s", *set_jstr(NET, "gateway", "") ? set_jstr(NET, "gateway", "") : "—", *set_jstr(NET, "dns", "") ? set_jstr(NET, "dns", "") : "—"); p_msg(NET ? g : "rudolfd did not answer", 2); g_free(g); }
	p_h4("Manual address");
	{ const char *v[8]; int n = 0; for (int i = 0; i < rui_jlen(ifs) && n < 8; i++) if (!rui_jbool(rui_jat(ifs, i), "virtual")) v[n++] = set_jstr(rui_jat(ifs, i), "name", "");
		Cell *c = p_r2("Interface"); NIF = s_select(v, v, n, n ? v[0] : "", NULL, NULL); c_add(c, NIF, 0, 0, 1); p_label_named("nif", NIF); p_r2_more(c);
		c = p_r2("Address/prefix"); NAD = rui_entry("192.168.1.50/24"); c_add(c, NAD, 0, 0, 1); p_label_named("nad", NAD); p_r2_more(c);
		c = p_r2("Gateway"); NGW = rui_entry("192.168.1.1"); c_add(c, NGW, 0, 0, 1); p_label_named("ngw", NGW); p_r2_more(c);
		c = p_r2("DNS server"); NDN = rui_entry("1.1.1.1"); c_add(c, NDN, 0, 0, 1); p_label_named("ndn", NDN);
		const char *L[2] = { "Apply manual address", "Use automatic (DHCP)" }; GCallback C[2] = { G_CALLBACK(on_static), G_CALLBACK(on_dhcp) }; p_buttons(L, 2, C, NULL); }
	p_h4("Proxy");
	{ Cell *c = p_r2("Proxy server"); NPX = rui_entry("host:port (empty = none)"); c_add(c, NPX, 0, 0, 1); p_label_named("npx", NPX);
		GtkWidget *b = rui_button("Save proxy", 0); g_signal_connect(b, "clicked", G_CALLBACK(on_proxy), NULL); p_put(b); p_label_named("npa", b); p_custom(btn_lay, b, 0, 0); }
	p_msg("The saved \"Connections\" and \"Add a connection\" of the browser version are not shown here: on this computer, Wi-Fi and Manual address above set up the real network.", 2);
	/* (what settings31.js hides but the page's CSS still shows: the Web search and Downloads rows, without their headings and words) */
	net_search_rows("");
	net_search_msg();
	p_msg("Connections here are profiles that control whether Rudolf OS may use the network. Actual connectivity is provided by the device you are using; the OS cannot join real Wi‑Fi networks itself.", 1);
	{ GtkWidget *w = gtk_drawing_area_new(); gtk_widget_add_events(w, GDK_BUTTON_PRESS_MASK); g_signal_connect(w, "draw", G_CALLBACK(lnk_draw), NULL); g_signal_connect(w, "button-press-event", G_CALLBACK(lnk_press), NULL); p_put(w); p_label_named("nsmoved", w); p_custom(lnk_lay, w, 10, 0); }
	metered_part();
	modems_part();
	hotspot_part();
	vpn_part();
	if (set_admin()) p_footer(net_dirty, net_foot);
	{ GtkWidget *w = gtk_drawing_area_new(); gtk_widget_set_no_show_all(w, TRUE); p_put(w); if (ALIVE) g_object_remove_weak_pointer(G_OBJECT(ALIVE), (gpointer *)&ALIVE); ALIVE = w; g_object_add_weak_pointer(G_OBJECT(w), (gpointer *)&ALIVE); }
	set_note("net %d %u", nwl, NETS ? NETS->len : 0);
}
