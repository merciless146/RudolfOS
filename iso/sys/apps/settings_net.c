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
#include "settings.h"

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
		g_object_set_data(G_OBJECT(t), "b", b); g_signal_connect(t, "draw", G_CALLBACK(hd_draw), NULL); p_put(t); p_custom(hd_lay, t, 6, 6); p_custom_find("Networks in range", 0); }
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
	if (!KEEP) { rui_jfree(NET); NET = rui_get("/api/net"); RuiJ *s = rui_get("/api/wifi/status"); wst_parse(set_jstr(s, "raw", "")); rui_jfree(s); wscan(); }
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
	{ GtkWidget *w = gtk_drawing_area_new(); gtk_widget_set_no_show_all(w, TRUE); p_put(w); if (ALIVE) g_object_remove_weak_pointer(G_OBJECT(ALIVE), (gpointer *)&ALIVE); ALIVE = w; g_object_add_weak_pointer(G_OBJECT(w), (gpointer *)&ALIVE); }
	set_note("net %d %u", nwl, NETS ? NETS->len : 0);
}
