/*
 * rudolf-settings: Devices (devices26.js devPanel, with tb54.js's Thunderbolt and USB4, usbprint60.js's printers, printq61.js's print
 * queue and scan62.js's scanners). 1.6.3, docs/plan-1.6.3.md Part 3 (PR 7). As the page: rudolfd's devices, devices/unsupported, tb,
 * print/usb and print/jobs drawn as the page's tables (.dvt: CSS's automatic table layout, 1px cells, the icon, the name in bold and
 * what it is under it, the buttons at the right); Open (the drive in File Explorer, as devOpenDrive), ⏏ Eject (eject?dev=), Settings
 * (Mouse and touchpad), Thunderbolt's Allow and Forget (tb/allow, tb/forget, the administrator's prompt), a document's Cancel
 * (print/cancel), System extensions…, 📠 Open Scan, Refresh. DEV_ICON, DEV_BUS and PQ_STATE read from the page at build time.
 * GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "settings.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
#pragma GCC diagnostic ignored "-Wunused-variable"
#include "settings-gen.h"
#pragma GCC diagnostic pop

static char *fmt_b(double b)
{
	if (b < 0) return g_strdup(""); if (b < 1024) return g_strdup_printf("%.0f B", b); if (b < 1048576) return g_strdup_printf("%.1f KB", b / 1024);
	if (b < 1073741824) return g_strdup_printf("%.2f MB", b / 1048576); return g_strdup_printf("%.1f GB", b / 1073741824);
}
static const char *dev_icon(const char *k, const char *dflt) { for (guint i = 0; i < G_N_ELEMENTS(DEVICON); i++) if (!strcmp(DEVICON[i][0], k)) return DEVICON[i][1]; return dflt; }
static int post(const char *path, char *msg, size_t n) { char *out = NULL; int c = rui_http("POST", path, NULL, 0, &out, NULL); RuiJ *j = out ? rui_jparse(out) : NULL; if (msg) snprintf(msg, n, "%s", set_jstr(j, c == 200 ? "message" : "error", c ? "" : "rudolfd did not answer")); set_note("POST %s %d", path, c); rui_jfree(j); g_free(out); return c; }

/* ================================================================== a .dvt table */
typedef struct { char *icon, *name, *small, *warn; int warn_all; GtkWidget *b[2]; int nb; } DRow;
typedef struct { GtkWidget *a; GPtrArray *rows; int fixed; double cw[3]; } DTab;
static void drow_free(gpointer p) { DRow *r = p; g_free(r->icon); g_free(r->name); g_free(r->small); g_free(r->warn); g_free(r); }
static double tw(const char *t, double px, int wt) { double w = 0; s_lines(t ? t : "", px, wt, 0, &w); return w; }
static double sp_w(void) { double sp, a1; s_lines("a a", 13, 400, 0, &sp); s_lines("aa", 13, 400, 0, &a1); return sp - a1; }
static void dtab_cols(DTab *t, double W)
{
	/* (the icon's column its own width (28px when the page says so); the extra room shared by the others as their widest contents) */
	double m0 = t->fixed ? 30 : 0, m1 = 2, m2 = 2;
	for (guint i = 0; i < t->rows->len; i++) { DRow *r = g_ptr_array_index(t->rows, i); if (!t->fixed) m0 = MAX(m0, tw(r->icon, 13, 400) + 2);
		char *s = g_strdup_printf("%s%s", r->small, r->warn ? r->warn : ""); m1 = MAX(m1, MAX(tw(r->name, 13, 700), tw(s, 10.8333, 400)) + 2); g_free(s);
		double bw = 0; for (int k = 0; k < r->nb; k++) bw += (k ? sp_w() : 0) + p_nat_w(r->b[k]); m2 = MAX(m2, bw + 2); }
	double ex = W - m0 - m1 - m2; t->cw[0] = m0; t->cw[1] = m1 + (ex > 0 ? ex * m1 / (m1 + m2) : 0); t->cw[2] = m2 + (ex > 0 ? ex * m2 / (m1 + m2) : 0);
}
static double dtab_lay(void *d, double x, double y, double w)
{
	DTab *t = d; dtab_cols(t, w); double h = t->rows->len * 32;
	for (guint i = 0; i < t->rows->len; i++) { DRow *r = g_ptr_array_index(t->rows, i); double bx = x + w - 1;
		for (int k = r->nb; k-- > 0;) { double bw = p_nat_w(r->b[k]); bx -= bw; p_place(r->b[k], bx, y + i * 32 + (32 - p_nat_h(r->b[k])) / 2, 0, 0); bx -= sp_w(); } }
	p_place(t->a, x, y, w, MAX(1, h)); return h;
}
static gboolean dtab_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	DTab *t = d; double z = s_z(), W = gtk_widget_get_allocated_width(w) / z; dtab_cols(t, W);
	for (guint i = 0; i < t->rows->len; i++) { DRow *r = g_ptr_array_index(t->rows, i); double y = i * 32;
		s_col(cr, "fg", 1); s_text(cr, r->icon, 1, y + (32 - 15) / 2, 13, 400, 0, 15);
		double x1 = t->cw[0] + 1; s_text(cr, r->name, x1, y + 1, 13, 700, 0, 15);
		double sy = y + 1 + 15 + (15 - s_lh(10.8333, 400)) / 2 + .5;
		if (r->warn_all) cairo_set_source_rgb(cr, 0xb4 / 255.0, 0x53 / 255.0, 0x09 / 255.0); else s_col(cr, "fg", 1);
		s_text(cr, r->small, x1, sy, 10.8333, 400, 0, s_lh(10.8333, 400));
		if (r->warn) { cairo_set_source_rgb(cr, 0xb4 / 255.0, 0x53 / 255.0, 0x09 / 255.0); s_text(cr, r->warn, x1 + tw(r->small, 10.8333, 400), sy, 10.8333, 400, 0, s_lh(10.8333, 400)); } }
	return TRUE;
}
static DTab *dtab(int fixed)
{
	DTab *t = g_new0(DTab, 1); t->fixed = fixed; t->rows = g_ptr_array_new_with_free_func(drow_free); t->a = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(t->a), "dtab", t, NULL);
	g_signal_connect(t->a, "draw", G_CALLBACK(dtab_draw), t); p_put(t->a); return t;
}
static DRow *drow(DTab *t, const char *icon, const char *name, const char *small) { DRow *r = g_new0(DRow, 1); r->icon = g_strdup(icon); r->name = g_strdup(name); r->small = g_strdup(small); g_ptr_array_add(t->rows, r); return r; }
static GtkWidget *dbtn(DRow *r, const char *l, GCallback cb, const char *data, const char *name)
{
	GtkWidget *b = rui_button(l, 0); g_object_set_data_full(G_OBJECT(b), "d", g_strdup(data ? data : ""), g_free); g_signal_connect(b, "clicked", cb, g_object_get_data(G_OBJECT(b), "d")); p_put(b); if (name) p_label_named(name, b); r->b[r->nb++] = b; return b;
}
static void dtab_done(DTab *t) { p_custom(dtab_lay, t, 0, 0); }
/* (<h4 style='margin:12px 0 4px'>: 13px bold, not .sh4's capitals) */
static double h4_lay(void *d, double x, double y, double w) { p_place(d, x, y, w, 15); return 15; }
static gboolean h4_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)d; const char *t = g_object_get_data(G_OBJECT(w), "t"); s_col(cr, "fg", 1); s_text(cr, t, 0, 0, 13, 700, 0, 15); return TRUE; }
static void h4(const char *t) { GtkWidget *a = gtk_drawing_area_new(); g_object_set_data_full(G_OBJECT(a), "t", g_strdup(rui_tr(t)), g_free); g_signal_connect(a, "draw", G_CALLBACK(h4_draw), NULL); p_put(a); p_custom(h4_lay, a, 12, 4); p_custom_find(rui_tr(t), 1); }

/* ================================================================== the actions */
static void redraw(void) { set_show_keep("devs"); }
static RuiJ *DEV;
static void on_open(GtkButton *b, gpointer d)
{
	(void)b; RuiJ *dr = rui_jprop(DEV, "drives"); for (int i = 0; i < rui_jlen(dr); i++) { RuiJ *v = rui_jat(dr, i); if (strcmp(set_jstr(v, "dev", ""), d)) continue;
		char *e = g_uri_escape_string(set_jstr(v, "path", ""), NULL, FALSE), *a = g_strdup_printf("open explorer %s", set_jstr(v, "path", "")); rui_shell(SW, a); set_note("open drive %s", (char *)d); g_free(e); g_free(a); }
}
static gboolean later(gpointer d) { (void)d; redraw(); return G_SOURCE_REMOVE; }
static void on_eject(GtkButton *b, gpointer d) { gtk_widget_set_sensitive(GTK_WIDGET(b), FALSE); char p[64], m[300]; snprintf(p, sizeof p, "/api/eject?dev=%s", (char *)d); if (post(p, m, sizeof m) == 200) g_timeout_add(800, later, NULL); else { gtk_widget_set_sensitive(GTK_WIDGET(b), TRUE); set_notify("Eject", m, 1); } }
static void on_mouse(GtkButton *b, gpointer d) { (void)b; (void)d; set_show("mouse"); }
static void on_ext(GtkButton *b, gpointer d) { (void)b; (void)d; set_show("upd"); }
static void on_scan(GtkButton *b, gpointer d) { (void)b; (void)d; rui_shell(SW, "open scan"); set_note("open scan"); }
static void on_refresh(GtkButton *b, gpointer d) { (void)b; (void)d; redraw(); }
static void on_cancel(GtkButton *b, gpointer d)
{
	gtk_widget_set_sensitive(GTK_WIDGET(b), FALSE); char *e = g_uri_escape_string(d, NULL, FALSE), *p = g_strdup_printf("/api/print/cancel?id=%s", e), m[300];
	if (post(p, m, sizeof m) != 200) set_notify("Print queue", m, 0); g_free(e); g_free(p); redraw();
}
/* (Thunderbolt: Allow, the administrator's; Forget) */
static void tb_allow_go(int ok, void *d) { if (ok) { char *e = g_uri_escape_string(d, NULL, FALSE), *p = g_strdup_printf("/api/tb/allow?id=%s", e), m[300]; if (post(p, m, sizeof m) != 200) set_notify("Thunderbolt", m, 1); g_free(e); g_free(p); } g_free(d); redraw(); }
static void on_tballow(GtkButton *b, gpointer d) { const char *nm = g_object_get_data(G_OBJECT(b), "nm"); char *w = g_strdup_printf("Allow the Thunderbolt device %s", nm ? nm : ""); rui_elevate(SW, w, tb_allow_go, g_strdup(d)); g_free(w); set_note("ask tb allow"); }
static void on_tbforget(GtkButton *b, gpointer d) { (void)b; char *e = g_uri_escape_string(d, NULL, FALSE), *p = g_strdup_printf("/api/tb/forget?uid=%s", e), m[300]; if (post(p, m, sizeof m) != 200) set_notify("Thunderbolt", m, 1); g_free(e); g_free(p); redraw(); }

/* ================================================================== the page */
static char *usb_speed(double m) { return !m ? g_strdup("") : m < 12 ? g_strdup("USB 1.0, 1.5 Mbit/s") : m < 480 ? g_strdup("USB 1.1, 12 Mbit/s") : m < 5000 ? g_strdup("USB 2.0, 480 Mbit/s") : g_strdup_printf("USB 3, %g Gbit/s", m / 1000); }
static char *usb_port(double m) { return !m ? g_strdup("") : m >= 5000 ? g_strdup_printf("in a USB 3 port (%g Gbit/s)", m / 1000) : g_strdup("in a USB 2.0 port"); }
static char *disk_of(const char *d)
{
	/* (devDisk: mmcblk0p1 → mmcblk0, nvme0n1p2 → nvme0n1, sdb1 → sdb) */
	GRegex *a = g_regex_new("^(mmcblk\\d+|nvme\\d+n\\d+)", 0, 0, NULL), *b = g_regex_new("^(sd[a-z]+)", 0, 0, NULL); GMatchInfo *mi = NULL; char *r = NULL;
	if (g_regex_match(a, d, 0, &mi) || (g_match_info_free(mi), mi = NULL, g_regex_match(b, d, 0, &mi))) r = g_match_info_fetch(mi, 1); g_match_info_free(mi); g_regex_unref(a); g_regex_unref(b); return r ? r : g_strdup(d);
}
static char *when(double t)
{
	/* (pqWhen: the page's toLocaleTimeString, hour and minute, as en-US writes them) */
	if (!t) return g_strdup(""); GDateTime *g = g_date_time_new_from_unix_local((gint64)t); int h = g_date_time_get_hour(g), mi = g_date_time_get_minute(g); g_date_time_unref(g);
	return g_strdup_printf("%d:%02d %s", h % 12 ? h % 12 : 12, mi, h < 12 ? "AM" : "PM");
}
static void none_msg(void) { p_msg("None", 0); }
/* (a button alone, mt px under the block before it) */
static double btn1_lay(void *d, double x, double y, double w) { (void)w; p_place(d, x, y, 0, 0); return p_nat_h(d); }
void page_devs(void)
{
	rui_jfree(DEV); DEV = rui_get("/api/devices"); char nm[24];
	p_h3("Devices");
	p_msg_m("What is plugged into this computer. USB drives and SD cards open in File Explorer by themselves; use ⏏ Eject before unplugging one.", 0, 10, 13);
	if (!DEV) p_msg_m("could not list the devices", 0, 0, 0);
	else {
		RuiJ *dr = rui_jprop(DEV, "drives"), *us = rui_jprop(DEV, "usb"), *in = rui_jprop(DEV, "inputs");
		h4("Drives and SD cards");
		if (!rui_jlen(dr)) none_msg(); else { DTab *t = dtab(1);
			for (int i = 0; i < rui_jlen(dr); i++) { RuiJ *v = rui_jat(dr, i); const char *dev = set_jstr(v, "dev", ""), *lb = set_jstr(v, "label", ""), *mo = set_jstr(v, "model", ""); char *mt = g_strstrip(g_strdup(mo));
				const char *name = *lb ? lb : *mt ? mt : g_str_has_prefix(dev, "mmcblk") ? "SD card" : "USB drive"; char *sz = fmt_b(rui_jnum(v, "bytes", 0));
				char *sm = g_strdup_printf("%s · %s%s%s%s", sz, set_jstr(v, "fs", ""), rui_jbool(v, "readonly") ? " · read only" : "", *mo ? " · " : "", mo);
				DRow *r = drow(t, g_str_has_prefix(dev, "mmcblk") ? "💾" : "🔌", name, sm); snprintf(nm, sizeof nm, "dvopen%d", i); dbtn(r, "Open", G_CALLBACK(on_open), dev, nm);
				char *dk = disk_of(dev); snprintf(nm, sizeof nm, "dveject%d", i); dbtn(r, "⏏ Eject", G_CALLBACK(on_eject), dk, nm); g_free(dk); g_free(sz); g_free(sm); g_free(mt); }
			dtab_done(t); }
		h4("USB devices");
		{ DTab *t = dtab(0); int n = 0;
			for (int i = 0; i < rui_jlen(us); i++) { RuiJ *v = rui_jat(us, i); const char *k = set_jstr(v, "kind", ""); if (!strcmp(k, "USB drive") && rui_jlen(dr)) continue;
				char *sp = usb_speed(rui_jnum(v, "speed", 0)), *po = usb_port(rui_jnum(v, "port", 0)); GString *s = g_string_new(NULL); g_string_printf(s, "%s · %s", k, set_jstr(v, "id", ""));
				if (*sp) g_string_append_printf(s, " · %s%s%s", sp, *po ? ", " : "", po); DRow *r = drow(t, dev_icon(k, "🔌"), set_jstr(v, "name", ""), s->str);
				if (!rui_jnum(v, "driver", 0)) r->warn = g_strdup(" · no driver in Rudolf OS"); g_string_free(s, TRUE); g_free(sp); g_free(po); n++; }
			if (!n) { none_msg(); gtk_widget_destroy(t->a); } else dtab_done(t); }
		{ RuiJ *tb = rui_get("/api/tb"); if (tb && rui_jbool(tb, "present")) { h4("Thunderbolt and USB4");
				p_msg_m(rui_jbool(tb, "dma") ? "This computer protects its memory from devices on the cable (DMA protection), so devices are allowed by themselves." : "This computer does not report DMA protection, so each new device is allowed once by an administrator.", 0, 0, 4);
				RuiJ *ds = rui_jprop(tb, "devices"), *kp = rui_jprop(tb, "kept"); DTab *t = dtab(1);
				for (int i = 0; i < rui_jlen(ds); i++) { RuiJ *v = rui_jat(ds, i); int ap = rui_jbool(v, "approved"), ke = rui_jbool(v, "kept"); char *s = g_strdup_printf("%s%s", ap ? "Connected" : "Waiting to be allowed", ke ? " · allowed by itself when plugged in" : "");
					DRow *r = drow(t, "⚡", set_jstr(v, "name", ""), s); if (!ap) { GtkWidget *b = dbtn(r, "Allow", G_CALLBACK(on_tballow), set_jstr(v, "id", ""), NULL); g_object_set_data_full(G_OBJECT(b), "nm", g_strdup(set_jstr(v, "name", "")), g_free); }
					if (ke) dbtn(r, "Forget", G_CALLBACK(on_tbforget), set_jstr(v, "uid", ""), NULL); g_free(s); }
				for (int i = 0; i < rui_jlen(kp); i++) { RuiJ *k = rui_jat(kp, i); int on = 0; for (int j = 0; j < rui_jlen(ds); j++) on |= !strcmp(set_jstr(rui_jat(ds, j), "uid", ""), set_jstr(k, "uid", ""));
					if (on) continue; DRow *r = drow(t, "⚡", set_jstr(k, "name", ""), "Not plugged in · allowed by itself when plugged in"); dbtn(r, "Forget", G_CALLBACK(on_tbforget), set_jstr(k, "uid", ""), NULL); }
				if (!t->rows->len) { p_msg("None plugged in.", 0); gtk_widget_destroy(t->a); } else dtab_done(t); }
			rui_jfree(tb); }
		h4("Keyboards, mice and touchpads");
		if (!rui_jlen(in)) none_msg(); else { DTab *t = dtab(0);
			for (int i = 0; i < rui_jlen(in); i++) { RuiJ *v = rui_jat(in, i); const char *k = set_jstr(v, "kind", "");
				const char *ic = !strcmp(k, "keyboard") ? "⌨️" : !strcmp(k, "mouse") ? "🖱️" : !strcmp(k, "touchpad") ? "👆" : !strcmp(k, "touchscreen") ? "📱" : !strcmp(k, "controller") ? "🎮" : "🖱️";
				DRow *r = drow(t, ic, set_jstr(v, "name", ""), k); if (!strcmp(k, "touchpad") || !strcmp(k, "mouse")) { snprintf(nm, sizeof nm, "dvmouse%d", i); dbtn(r, "Settings", G_CALLBACK(on_mouse), NULL, nm); } }
			dtab_done(t); }
		{ RuiJ *un = rui_get("/api/devices/unsupported"), *ud = rui_jprop(un, "devices");
			if (un) { h4("Not supported yet"); p_msg_m("Parts of this computer that Rudolf OS has no driver for yet. Report a problem (Settings › Security) includes this list, so it can be added.", 0, 0, 4);
				if (!rui_jlen(ud)) p_msg("Every device in this computer has a driver.", 0);
				else { DTab *t = dtab(1); for (int i = 0; i < rui_jlen(ud); i++) { RuiJ *v = rui_jat(ud, i); const char *b = set_jstr(v, "bus", ""), *bn = b;
						for (guint k = 0; k < G_N_ELEMENTS(DEVBUS); k++) if (!strcmp(DEVBUS[k][0], b)) bn = DEVBUS[k][1];
						char *s = g_strdup_printf("%s · %s %s", set_jstr(v, "kind", ""), bn, set_jstr(v, "id", "")); DRow *r = drow(t, dev_icon(set_jstr(v, "kind", ""), "🧩"), set_jstr(v, "name", ""), s); r->warn = g_strdup(" · no driver yet"); g_free(s); }
					dtab_done(t); } }
			rui_jfree(un); }
	}
	/* (usbprint60.js: the printers plugged in by USB) */
	{ RuiJ *L = rui_get("/api/print/usb"); if (L) { RuiJ *ps = rui_jprop(L, "printers"); h4("Printers plugged in by USB");
			if (!rui_jlen(ps)) p_msg("No USB printer is plugged in. Network printers are found by the print dialog.", 0);
			else { DTab *t = dtab(1); for (int i = 0; i < rui_jlen(ps); i++) { RuiJ *p = rui_jat(ps, i); const char *u = set_jstr(p, "usb", ""); int rdy = rui_jbool(p, "ready"), need = 0; const char *line;
					if (!strcmp(u, "ipp")) { line = rdy ? "IPP over USB · ready to print" : "IPP over USB · needs the USB printers extension to print"; need = !rdy; }
					else if (!strcmp(u, "pdf")) line = "Reads PDF · ready to print";
					else if (!strcmp(u, "postscript")) { line = rdy ? "Reads PostScript · ready to print" : "Reads PostScript · needs the USB printers extension to print"; need = !rdy; }
					else line = "Needs a driver Rudolf OS does not have (PCL or a maker's own language): it cannot print yet";
					const char *n = set_jstr(p, "name", ""), *mo = set_jstr(p, "model", ""); char *s = g_strdup_printf("%s%s%s", line, *mo && strcmp(mo, n) ? " · " : "", *mo && strcmp(mo, n) ? mo : "");
					DRow *r = drow(t, "🖨️", *n ? n : "USB printer", s); r->warn_all = !strcmp(u, "driver"); if (need) dbtn(r, "System extensions…", G_CALLBACK(on_ext), NULL, "upx"); g_free(s); }
				dtab_done(t); } }
		rui_jfree(L); }
	/* (printq61.js: the print queue) */
	{ char *out = NULL; int c = rui_http("POST", "/api/print/jobs", NULL, 0, &out, NULL); RuiJ *L = c == 200 && out ? rui_jparse(out) : NULL; g_free(out);
		if (L) { h4("Print queue"); RuiJ *js = rui_jprop(L, "jobs"), *q = rui_jprop(L, "queue"); int off = q && rui_jtype(q) == 1 && !rui_jval(q);
			if (off || !rui_jlen(js)) p_msg(off ? "The print queue is not running on this computer: documents go straight to the printer." : "Nothing is printing. Documents printed from this desktop and from Linux apps are listed here for an hour.", 0);
			else { DTab *t = dtab(1); for (int i = 0; i < rui_jlen(js); i++) { RuiJ *j = rui_jat(js, i); const char *st = set_jstr(j, "state", ""), *ic = "🖨️", *sw = st;
					for (guint k = 0; k < G_N_ELEMENTS(PQSTATE); k++) if (!strcmp(PQSTATE[k][0], st)) { ic = PQSTATE[k][1]; sw = PQSTATE[k][2]; }
					const char *n = set_jstr(j, "name", ""); char *w = when(rui_jnum(j, "done", 0) ? rui_jnum(j, "done", 0) : rui_jnum(j, "made", 0));
					GString *s = g_string_new(*set_jstr(j, "printer", "") ? set_jstr(j, "printer", "") : "Printer"); g_string_append_printf(s, " · %s", sw);
					if (!strcmp(st, "failed") && *set_jstr(j, "message", "")) g_string_append_printf(s, ": %s", set_jstr(j, "message", "")); if (*w) g_string_append_printf(s, " · %s", w);
					DRow *r = drow(t, ic, rui_jbool(j, "mine") ? (*n ? n : "Document") : (*n ? n : "Another account's document"), s->str); r->warn_all = !strcmp(st, "failed");
					if (rui_jbool(j, "cancel")) { snprintf(nm, sizeof nm, "pqx%d", i); dbtn(r, "Cancel", G_CALLBACK(on_cancel), set_jstr(j, "id", ""), nm); } g_string_free(s, TRUE); g_free(w); }
				dtab_done(t); } }
		rui_jfree(L); }
	/* (scan62.js) */
	h4("Scanners");
	p_msg("Scanners and printer-scanners on the network (eSCL, also called AirScan, and WSD) and on USB work without a driver.", 0);
	{ GtkWidget *b = rui_button("📠 Open Scan", 0); g_signal_connect(b, "clicked", G_CALLBACK(on_scan), NULL); p_put(b); p_label_named("dvscan", b); p_custom(btn1_lay, b, 4, 0); }
	{ GtkWidget *b = rui_button("Refresh", 0); g_signal_connect(b, "clicked", G_CALLBACK(on_refresh), NULL); p_put(b); p_label_named("dvr", b); p_custom(btn1_lay, b, 13, 13); }
}
