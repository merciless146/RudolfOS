/*
 * rudolf-settings: Bring in files from the old computer (bring62.js). 1.6.2, docs/plan-1.6.2.md Part 39.
 *   - Update and recovery's section: Look on the drives (bring/found), each copy found with its people, Bring them in: rudolfd's
 *     bring/import, its job followed by its PROGRESS lines, then what came in checked with the page's own antivirus rules (brScan:
 *     avScan by the name, a program's header, EICAR, the page's SIGS over the first and last parts of each file; the rules read from
 *     the page at build time by mksettings.py), what is caught deleted (home/delete) and written to the security log (logEvt 'av');
 *     the set marked as brought in (bd95bring:ACCT), as the page marks it;
 *   - before an installation erases a disk (bringStep): the disk looked at read-only (bring/scan, bring/result), the people and
 *     folders found, BitLocker's recovery key, keep them (copied onto a drive and checked: bring/copy, its job; a full drive and
 *     another one; files that could not be read) or erase them (bring/decide), then the installation goes on.
 * GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
#include "settings-gen.h"
#pragma GCC diagnostic pop

#define BR_TOP "Rudolf OS - Bring your files"
static char *brb(double b) { if (b < 1024) return g_strdup_printf("%.0f B", b); if (b < 1048576) return g_strdup_printf("%.1f KB", b / 1024); if (b < 1073741824) return g_strdup_printf("%.2f MB", b / 1048576); return g_strdup_printf("%.1f GB", b / 1073741824); }
static char *jesc(const char *s) { GString *b = g_string_new(NULL); for (; s && *s; s++) { unsigned char c = *s; if (c == '"' || c == '\\') g_string_append_printf(b, "\\%c", c); else if (c < 32) g_string_append_printf(b, "\\u%04x", c); else g_string_append_c(b, c); } return g_string_free(b, FALSE); }
static char *qe(const char *s) { return g_uri_escape_string(s ? s : "", NULL, TRUE); }
static int post(const char *path, const char *body, char *msg, size_t n)
{
	char *out = NULL; int c = rui_http("POST", path, body, body ? strlen(body) : 0, &out, NULL); RuiJ *j = out ? rui_jparse(out) : NULL;
	if (msg) snprintf(msg, n, "%s", set_jstr(j, c == 200 ? "message" : "error", c ? "" : "rudolfd did not answer")); set_note("POST %s %d", path, c); rui_jfree(j); g_free(out); return c;
}
/* a job's progress (brProg): "PROGRESS files total bytes totalbytes" */
static int prog(const char *log, double v[4])
{
	const char *last = NULL; for (const char *p = log; p && *p;) { const char *e = strchr(p, '\n'); if (!strncmp(p, "PROGRESS ", 9)) last = p; if (!e) break; p = e + 1; }
	if (!last) return 0; return sscanf(last + 9, "%lf %lf %lf %lf", &v[0], &v[1], &v[2], &v[3]) == 4;
}
static char *brtext(const char *log)
{
	GString *o = g_string_new(NULL); char **l = g_strsplit(log ? log : "", "\n", -1);
	for (int i = 0; l[i]; i++) { if (!strncmp(l[i], "PROGRESS ", 9) || !strncmp(l[i], "RESULT ", 7)) continue; if (o->len || i) g_string_append_c(o, '\n'); g_string_append(o, l[i]); } g_strfreev(l);
	if (o->len && o->str[0] == '\n') g_string_erase(o, 0, 1); return g_string_free(o, FALSE);
}
/* the page's own storage, a key at a time */
#define ls_get set_ls_get
#define ls_set set_ls_set
static void jw(GString *b, RuiJ *v);
static void jw(GString *b, RuiJ *v)
{
	int t = rui_jtype(v);
	if (t == 1) g_string_append(b, rui_jval(v) ? "true" : "false"); else if (t == 2) g_string_append_printf(b, "%.17g", rui_jval(v));
	else if (t == 3) { char *e = jesc(rui_jsval(v)); g_string_append_printf(b, "\"%s\"", e); g_free(e); }
	else if (t == 4) { g_string_append_c(b, '['); for (int i = 0; i < rui_jlen(v); i++) { if (i) g_string_append_c(b, ','); jw(b, rui_jat(v, i)); } g_string_append_c(b, ']'); }
	else if (t == 5) { g_string_append_c(b, '{'); for (int i = 0; i < rui_jlen(v); i++) { const char *k = rui_jkey(v, i); char *e = jesc(k); if (i) g_string_append_c(b, ','); g_string_append_printf(b, "\"%s\":", e); g_free(e); jw(b, rui_jprop(v, k)); } g_string_append_c(b, '}'); }
	else g_string_append(b, "null");
}
/* (logEvt: the security log's list, the last 100) */
static void log_evt(const char *k, const char *m)
{
	RuiJ *o = ls_get("bd95seclog"), *L = rui_jprop(o, "x"); GString *b = g_string_new("{"); int n = 0, found = 0;
	for (int i = 0; i < rui_jlen(L); i++) { const char *key = rui_jkey(L, i); char *e = jesc(key); if (n++) g_string_append_c(b, ','); g_string_append_printf(b, "\"%s\":", e); g_free(e);
		if (!strcmp(key, k)) { found = 1; RuiJ *a = rui_jprop(L, key); int len = rui_jlen(a), from = len + 1 > 100 ? len + 1 - 100 : 0; g_string_append_c(b, '['); int w = 0;
			for (int q = from; q < len; q++) { if (w++) g_string_append_c(b, ','); jw(b, rui_jat(a, q)); } char *me = jesc(m); g_string_append_printf(b, "%s{\"t\":%.0f,\"m\":\"%s\"}]", w ? "," : "", set_now() * 1000, me); g_free(me); }
		else jw(b, rui_jprop(L, key)); }
	if (!found) { char *me = jesc(m), *ke = jesc(k); g_string_append_printf(b, "%s\"%s\":[{\"t\":%.0f,\"m\":\"%s\"}]", n ? "," : "", ke, set_now() * 1000, me); g_free(me); g_free(ke); }
	g_string_append_c(b, '}'); ls_set("bd95seclog", b->str); set_note("logEvt %s %s", k, m); g_string_free(b, TRUE); rui_jfree(o);
}
/* the account's files on the disk, open (the page's brReady: hdOn) */
static int br_ready(void)
{
	RuiJ *u = ls_get("bd95user"), *a = rui_jprop(rui_jprop(u, "x"), "accts"); int disk = 0;
	for (int i = 0; i < rui_jlen(a); i++) { RuiJ *x = rui_jat(a, i); if (!g_strcmp0(rui_jstr(x, "id"), ACCT)) disk = rui_jbool(x, "disk"); }
	rui_jfree(u); return disk;
}

/* ================================================================== the antivirus over what came in (brScan) */
static int av_on(void) { RuiJ *o = ls_get("bd95sec"), *x = rui_jprop(rui_jprop(o, "x"), "av"); RuiJ *on = rui_jprop(x, "on"); int r = !(rui_jtype(on) == 1 && !rui_jval(on)); rui_jfree(o); return r; }
static char *latin1(const char *b, size_t n) { GString *o = g_string_sized_new(n + 16); for (size_t i = 0; i < n; i++) g_string_append_unichar(o, (unsigned char)b[i]); return g_string_free(o, FALSE); }
static char *read_part(const char *rel, long off, long len, size_t *n)
{
	char *a = qe(ACCT), *p = qe(rel), path[1400], rg[80]; snprintf(path, sizeof path, "/api/home/read?acct=%s&path=%s", a, p); g_free(a); g_free(p);
	if (off < 0) snprintf(rg, sizeof rg, "Range: bytes=%ld\r\n", off); else snprintf(rg, sizeof rg, "Range: bytes=%ld-%ld\r\n", off, off + len - 1);
	char *out = NULL; int c = rui_http_h("GET", path, rg, NULL, 0, &out, n); if (c != 200 && c != 206) { g_free(out); return NULL; } return out;
}
/* (the parts are latin1 made UTF-8, so a pattern's \xNN is the byte as JavaScript's latin1 string has it; a name is matched as its bytes) */
static int rx(const char *re, int ci, const char *s) { GRegex *r = g_regex_new(re, (ci ? G_REGEX_CASELESS : 0) | G_REGEX_DOLLAR_ENDONLY | (g_utf8_validate(s, -1, NULL) ? 0 : G_REGEX_RAW), 0, NULL); if (!r) return 0; int m = g_regex_match(r, s, 0, NULL); g_regex_unref(r); return m; }
static int av_scan(const char *rel)
{
	const char *base = strrchr(rel, '/'); char *nm = g_ascii_strup(base ? base + 1 : rel, -1); GPtrArray *hits = g_ptr_array_new();
	size_t n0 = 0; char *h = read_part(rel, 0, AV_PART * 2, &n0); if (!h) { g_free(nm); g_ptr_array_free(hits, TRUE); return 0; }
	GPtrArray *parts = g_ptr_array_new_with_free_func(g_free); g_ptr_array_add(parts, latin1(h, n0));
	if ((long)n0 >= AV_PART * 2) { size_t n1 = 0; char *t = read_part(rel, -AV_PART, AV_PART, &n1); if (t) { g_ptr_array_add(parts, latin1(t, n1)); g_free(t); } }
	if (rx(AV_DOUBLE, 0, nm)) g_ptr_array_add(hits, "Trojan.DoubleExtension");
	if (n0 >= 2 && (!memcmp(h, "MZ", 2) || (n0 >= 4 && !memcmp(h, "\x7f" "ELF", 4)))) g_ptr_array_add(hits, "Win.Executable.Header");   /* (AV_EXEC, on the bytes) */
	for (guint i = 0; i < parts->len; i++) { const char *s = g_ptr_array_index(parts, i); if (rx(AV_EICAR, 0, s)) g_ptr_array_add(hits, "EICAR-Test-File");
		for (guint k = 0; k < G_N_ELEMENTS(AVSIGS); k++) if (rx(AVSIGS[k].re, AVSIGS[k].ci, s)) g_ptr_array_add(hits, (gpointer)AVSIGS[k].name); }
	g_free(h); g_ptr_array_unref(parts); int bad = 0;
	if (hits->len) { GString *u = g_string_new(NULL); for (guint i = 0; i < hits->len; i++) { const char *x = g_ptr_array_index(hits, i); int dup = 0; for (guint k = 0; k < i; k++) if (!strcmp(g_ptr_array_index(hits, k), x)) dup = 1; if (!dup) g_string_append_printf(u, "%s%s", u->len ? ", " : "", x); }
		char *a = qe(ACCT), *p = qe(rel), path[1400]; snprintf(path, sizeof path, "/api/home/delete?acct=%s&path=%s", a, p); g_free(a); g_free(p);
		if (post(path, NULL, NULL, 0) == 200) { char *m = g_strdup_printf("Blocked %s (brought in from the old computer): %s", nm, u->str); log_evt("av", m); g_free(m); bad = 1; } g_string_free(u, TRUE); }
	g_ptr_array_free(hits, TRUE); g_free(nm); return bad;
}

/* ================================================================== Update and recovery's section */
static RuiJ *BRF; static int BRLOOKED, BRRUN; static GtkWidget *BRSAY; static char *BRSAID; static double BRBAR = -1; static int BRSEL[16];
static void say(const char *t) { g_free(BRSAID); BRSAID = g_strdup(t); if (BRSAY) p_text_set(BRSAY, t); set_note("bring say %s", t); }
typedef struct { RuiJ *set, *person; guint id; } Imp;
static char *set_key(RuiJ *s) { const char *dir = set_jstr(s, "dir", ""); char **p = g_strsplit(dir, "/", -1); GString *k = g_string_new(NULL); for (int i = 3; p[i]; i++) g_string_append_printf(k, "%s%s", i > 3 ? "/" : "", p[i]); g_strfreev(p); g_string_append_printf(k, "|%s", set_jstr(s, "made", "")); return g_string_free(k, FALSE); }
static int done_has(const char *k) { char key[120]; snprintf(key, sizeof key, "bd95bring:%s", ACCT); RuiJ *o = ls_get(key), *l = rui_jprop(o, "x"); int f = 0; for (int i = 0; i < rui_jlen(l); i++) if (!g_strcmp0(rui_jsval(rui_jat(l, i)), k)) f = 1; rui_jfree(o); return f; }
static void done_mark(const char *k)
{
	char key[120]; snprintf(key, sizeof key, "bd95bring:%s", ACCT); RuiJ *o = ls_get(key), *l = rui_jprop(o, "x"); GString *b = g_string_new("["); int n = rui_jlen(l), from = n + 1 > 50 ? n + 1 - 50 : 0, w = 0, have = 0;
	for (int i = 0; i < n; i++) if (!g_strcmp0(rui_jsval(rui_jat(l, i)), k)) have = 1;
	if (have) { rui_jfree(o); g_string_free(b, TRUE); return; }
	for (int i = from; i < n; i++) { char *e = jesc(rui_jsval(rui_jat(l, i))); g_string_append_printf(b, "%s\"%s\"", w++ ? "," : "", e); g_free(e); }
	char *e = jesc(k); g_string_append_printf(b, "%s\"%s\"]", w ? "," : "", e); g_free(e); ls_set(key, b->str); g_string_free(b, TRUE); rui_jfree(o);
}
static gboolean imp_tick(gpointer d)
{
	Imp *m = d; RuiJ *j = rui_get("/api/job"); if (!j) return G_SOURCE_CONTINUE; const char *nmj = rui_jstr(j, "name"); if (nmj && strcmp(nmj, "bring-import")) { rui_jfree(j); return G_SOURCE_CONTINUE; }
	const char *log = set_jstr(j, "log", ""); double v[4]; if (prog(log, v)) { BRBAR = v[3] ? v[2] / v[3] * 900 : v[0] / fmax(1, v[1]) * 900; char *a = brb(v[2]), *b = brb(v[3]), *t = g_strdup_printf("Bringing in %.0f of %.0f files · %s of %s…", v[0], v[1], a, b); say(t); g_free(a); g_free(b); g_free(t); }
	int run = rui_jbool(j, "running"); double st = rui_jnum(j, "status", -1); if (run || st < 0) { rui_jfree(j); return G_SOURCE_CONTINUE; }
	char *tail = brtext(log); g_strstrip(tail); rui_jfree(j);
	if (st != 0 && st != 3) { const char *l = strrchr(tail, '\n'); char *t = g_strdup_printf("Not brought in: %s", *tail ? (l ? l + 1 : tail) : "it stopped."); say(t); g_free(t); g_free(tail); BRRUN = 0; g_free(m); return G_SOURCE_REMOVE; }
	/* what came in, then the antivirus over it */
	char *a = qe(ACCT), path[400]; snprintf(path, sizeof path, "/api/home/read?acct=%s&path=.rudolf%%2Fbring-imported.txt", a); g_free(a); char *out = NULL; size_t on = 0; int c = rui_http_h("GET", path, NULL, NULL, 0, &out, &on);
	char **rels = c == 200 && out ? g_strsplit(out, "\n", -1) : g_new0(char *, 1); g_free(out); int nr = 0; for (int i = 0; rels[i]; i++) if (*rels[i]) nr++;
	int avon = av_on(); say(avon ? "Checking what came in with the antivirus…" : "Putting them in place…"); GPtrArray *bad = g_ptr_array_new();
	int k = 0; for (int i = 0; avon && rels[i]; i++) { if (!*rels[i]) continue; k++; BRBAR = 900 + k / fmax(1, nr) * 100; if (k % 10 == 1) { char *t = g_strdup_printf("Checking with the antivirus: %d of %d files…", k, nr); say(t); g_free(t); } if (av_scan(rels[i])) g_ptr_array_add(bad, rels[i]); }
	BRBAR = 1000; char *sk = set_key(m->set), *mk = g_strdup_printf("%s|%s", sk, set_jstr(m->person, "dir", "")); done_mark(mk); g_free(sk); g_free(mk);
	GString *lines = g_string_new(NULL); { char **L = g_strsplit(tail, "\n", -1); for (int i = 0; L[i]; i++) if (g_str_has_prefix(L[i], "Brought in") || g_regex_match_simple("^\\d+ files were not", L[i], 0, 0) || g_str_has_prefix(L[i], "  ")) g_string_append_printf(lines, "%s%s", lines->len ? "\n" : "", L[i]); g_strfreev(L); }
	GString *av = g_string_new(NULL); if (bad->len) { g_string_append_printf(av, " The antivirus blocked %u (deleted here, still on the drive): ", bad->len); for (guint i = 0; i < bad->len && i < 3; i++) { const char *r = g_ptr_array_index(bad, i), *s = strrchr(r, '/'); g_string_append_printf(av, "%s%s", i ? ", " : "", s ? s + 1 : r); } g_string_append_printf(av, "%s.", bad->len > 3 ? "…" : ""); }
	char *t = g_strdup_printf("%s%s Your old Documents are in Documents, Pictures in Pictures and so on; the rest in Documents\\From the old computer. The copy on the drive is left as it is: delete it when you no longer need it.", lines->len ? lines->str : "Done.", av->str); say(t); g_free(t);
	char *nt = g_strdup_printf("%d files brought into your folders.%s", nr, av->str); set_notify("Your files from the old computer are here", nt, bad->len || st == 3); g_free(nt);
	g_string_free(lines, TRUE); g_string_free(av, TRUE); g_ptr_array_free(bad, TRUE); g_strfreev(rels); g_free(tail); BRRUN = 0; g_free(m); return G_SOURCE_REMOVE;
}
static void br_in(GtkButton *b, gpointer d)
{
	int i = GPOINTER_TO_INT(d); if (BRRUN) return; RuiJ *s = rui_jat(rui_jprop(BRF, "sets"), i), *p = rui_jat(rui_jprop(s, "people"), BRSEL[i & 15]); if (!s || !p) return;
	BRRUN = 1; gtk_widget_set_sensitive(GTK_WIDGET(b), FALSE); BRBAR = 0; char *a = qe(ACCT), *dd = qe(rui_jstr(s, "dir")), *pp = qe(rui_jstr(p, "dir")), path[1600], m[300];
	snprintf(path, sizeof path, "/api/bring/import?acct=%s&dir=%s&person=%s", a, dd, pp); g_free(a); g_free(dd); g_free(pp);
	if (post(path, NULL, m, sizeof m) != 200) { char *t = g_strdup_printf("Not brought in: %s", m); say(t); g_free(t); BRRUN = 0; return; }
	Imp *x = g_new0(Imp, 1); x->set = s; x->person = p; x->id = g_timeout_add(700, imp_tick, x);
}
static void br_pick(int on, void *d) { int v = GPOINTER_TO_INT(d), set = v >> 8, who = v & 255; if (on) BRSEL[set & 15] = who; set_show("upd"); }
static void br_look(GtkButton *b, gpointer d) { (void)b; (void)d; rui_jfree(BRF); BRF = rui_get("/api/bring/found"); BRLOOKED = 1; BRBAR = -1; g_free(BRSAID); BRSAID = NULL; memset(BRSEL, 0xff, sizeof BRSEL); set_note("bring look %d", BRF != NULL); set_show("upd"); }
static gboolean bar_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
	/* (a progress bar as Chromium draws one: a grey track, the accent colour's fill) */
	(void)d; double z = s_z(), W = gtk_widget_get_allocated_width(w) / z, H = gtk_widget_get_allocated_height(w) / z; cairo_save(cr); cairo_scale(cr, z, z);
	cairo_set_source_rgba(cr, .5, .5, .5, .25); s_rrect(cr, 0, 0, W, H, H / 2); cairo_fill(cr); s_col(cr, "ac", 1); s_rrect(cr, 0, 0, W * fmax(0, fmin(1, BRBAR / 1000)), H, H / 2); cairo_fill(cr); cairo_restore(cr); return TRUE;
}
static double bar_lay(void *d, double x, double y, double w) { if (BRBAR < 0) { p_place(d, x, y, 1, 1); gtk_widget_set_visible(d, FALSE); return 0; } gtk_widget_set_visible(d, TRUE); p_place(d, x, y, w, 16); return 16; }
static gboolean bar_tick(gpointer d) { if (!GTK_IS_WIDGET(d)) return G_SOURCE_REMOVE; gtk_widget_queue_draw(d); return BRRUN ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE; }
void sec_bring(void)
{
	p_h4("Bring in files from the old computer");
	p_msg_m("The files the installer kept on a USB drive (" BR_TOP ") go into your folders: the old Documents into Documents, Pictures into Pictures and so on. Plug the drive in, then look.", 0, 10, 13);
	if (BRLOOKED) {
		if (!br_ready()) p_msg_m("Sign in to an account that keeps its files on this computer's disk (an installed Rudolf OS) to bring them in. Otherwise copy them from the drive in File Explorer.", 0, 10, 13);
		else if (!BRF) p_msg_m("rudolfd did not answer.", 0, 10, 13);
		else { RuiJ *sets = rui_jprop(BRF, "sets"); if (!rui_jlen(sets)) p_msg_m("No copy of an old computer's files on the drives plugged in.", 0, 10, 13);
			for (int i = 0; i < rui_jlen(sets) && i < 16; i++) { RuiJ *s = rui_jat(sets, i); const char *sy = set_jstr(s, "system", ""); char made[16]; snprintf(made, sizeof made, "%.10s", set_jstr(s, "made", "")); int dr = (int)rui_jnum(s, "drive", 1), last = rui_jbool(s, "last");
				char *t = dr > 1 || !last ? g_strdup_printf("%s · copied %s · drive %d%s", !strcmp(sy, "windows") ? "Windows" : !strcmp(sy, "linux") ? "Linux" : sy, made, dr, last ? " (the last)" : " (more on another drive)") : g_strdup_printf("%s · copied %s", !strcmp(sy, "windows") ? "Windows" : !strcmp(sy, "linux") ? "Linux" : sy, made);
				p_h4(t); g_free(t); RuiJ *pp = rui_jprop(s, "people"); char *sk = set_key(s);
				if (BRSEL[i] < 0 || BRSEL[i] >= rui_jlen(pp)) { BRSEL[i] = 0; for (int k = 0; k < rui_jlen(pp); k++) if (!g_ascii_strcasecmp(set_jstr(rui_jat(pp, k), "name", ""), ME)) BRSEL[i] = k; }
				for (int k = 0; k < rui_jlen(pp); k++) { RuiJ *p = rui_jat(pp, k); char *b = brb(rui_jnum(p, "bytes", 0)), *mk = g_strdup_printf("%s|%s", sk, set_jstr(p, "dir", "")); char *x = g_strdup_printf("%s's files — %s (%.0f files)%s", set_jstr(p, "name", ""), b, rui_jnum(p, "files", 0), done_has(mk) ? " · brought in already" : "");
					GtkWidget *c = p_check(x, BRSEL[i] == k, br_pick, GINT_TO_POINTER((i << 8) | k)); char nm[24]; snprintf(nm, sizeof nm, "brp%d-%d", i, k); p_label_named(nm, c); g_free(b); g_free(mk); g_free(x); }
				g_free(sk);
				const char *L[1] = { "Bring them in" }; GCallback C[1] = { G_CALLBACK(br_in) }; GtkWidget *bi = p_buttons(L, 1, C, GINT_TO_POINTER(i)); gtk_widget_set_sensitive(bi, !BRRUN); char nm[16]; snprintf(nm, sizeof nm, "brin%d", i); p_label_named(nm, bi);
				GtkWidget *bar = gtk_drawing_area_new(); g_signal_connect(bar, "draw", G_CALLBACK(bar_draw), NULL); p_put(bar); p_custom(bar_lay, bar, 6, 0); if (BRRUN) g_timeout_add(300, bar_tick, bar);
				BRSAY = p_msg_m(BRSAID ? BRSAID : "", 0, 10, 0); } } }
	const char *L[1] = { "Look on the drives" }; GCallback C[1] = { G_CALLBACK(br_look) }; p_label_named("brlook", p_buttons(L, 1, C, NULL));
}

/* ================================================================== before an installation: Bring your files (bringStep) */
typedef struct { char disk[64]; void (*cb)(int ok, void *d); void *d; GtkWidget *dim, *box, *msg, *list, *go, *cancel, *bar, *pt, *log, *tgt; RuiJ *R, *T; int busy, cont, ended, mode; char name[80]; guint tick; GPtrArray *checks; double barv; } Bs;
static Bs *BS;
static GtkWidget *lab(const char *t, int grey) { GtkWidget *l = gtk_label_new(t); gtk_label_set_xalign(GTK_LABEL(l), 0); gtk_label_set_line_wrap(GTK_LABEL(l), TRUE); gtk_label_set_max_width_chars(GTK_LABEL(l), 70); if (grey) gtk_style_context_add_class(gtk_widget_get_style_context(l), "rui-msg"); return l; }
/* (the page's warning, BR_STATE: .msg in #b45309) */
static GtkWidget *warn(const char *t) { GtkWidget *l = lab("", 0); char *m = g_markup_printf_escaped("<span foreground=\"#b45309\">%s</span>", t); gtk_label_set_markup(GTK_LABEL(l), m); g_free(m); return l; }
static void bs_msg(const char *t) { if (BS) gtk_label_set_text(GTK_LABEL(BS->msg), t); set_note("bringstep %s", t); }
static void bs_end(int v)
{
	Bs *b = BS; if (!b || b->ended) return; b->ended = 1; BS = NULL; if (b->tick) g_source_remove(b->tick); gtk_widget_destroy(b->dim);
	if (!v) { char *e = qe(b->disk), p[200]; snprintf(p, sizeof p, "/api/bring/close?disk=%s", e); g_free(e); post(p, NULL, NULL, 0); }
	set_note("bringstep end %d", v); if (b->cb) b->cb(v, b->d); rui_jfree(b->R); rui_jfree(b->T); if (b->checks) g_ptr_array_unref(b->checks); g_free(b);
}
static void bs_cancel(GtkButton *x, gpointer d) { (void)x; (void)d; if (!BS) return; if (BS->busy) { post("/api/bring/cancel", NULL, NULL, 0); gtk_widget_set_sensitive(BS->cancel, FALSE); bs_msg("Stopping… nothing is erased."); return; } bs_end(0); }
static void bs_clear(GtkWidget *c) { GList *l = gtk_container_get_children(GTK_CONTAINER(c)); for (GList *x = l; x; x = x->next) gtk_widget_destroy(x->data); g_list_free(l); }
static void bs_look(void); static void bs_draw(int keep); static void bs_sum(void);
/* the drives a copy can go onto (drivecopy60.js dcTargets, not the disk being erased) */
static char *dcb(double b) { return b >= 1e9 ? g_strdup_printf("%.1f GB", b / 1e9) : b >= 1e6 ? g_strdup_printf("%.0f MB", b / 1e6) : g_strdup_printf("%.0f KB", b / 1e3); }
static RuiJ *targets(const char *disk)
{
	RuiJ *r = rui_get("/api/disks"), *dl = rui_jprop(r, "disks"); GString *o = g_string_new("{\"x\":["); int n = 0, pass;
	for (pass = 0; pass < 2; pass++) for (int i = 0; i < rui_jlen(dl); i++) { RuiJ *dk = rui_jat(dl, i), *pa = rui_jprop(dk, "partitions"); const char *dn = set_jstr(dk, "name", ""); int skip = g_str_has_prefix(dn, "dm-");
		if (rui_jbool(dk, "removable") != (pass == 0)) continue;   /* (removable ones first) */
		for (int k = 0; k < rui_jlen(pa); k++) { const char *mt = rui_jstr(rui_jat(pa, k), "mount"); if (mt && *mt && !g_str_has_prefix(mt, "/media/")) skip = 1; } if (skip) continue;
		for (int k = 0; k < rui_jlen(pa); k++) { RuiJ *pt = rui_jat(pa, k); const char *fs = set_jstr(pt, "fs", ""), *lb = set_jstr(pt, "label", ""), *nm = set_jstr(pt, "name", "");
			if (!*fs || g_regex_match_simple("^(swap|iso9660|crypto_LUKS|LVM2_member|linux_raid_member)$", fs, 0, 0) || !strcmp(lb, "RUDOLFDATA") || !strcmp(lb, "RUDOLF EFI") || g_str_has_prefix(lb, "RUDOLF_BOOT") || g_str_has_prefix(nm, disk)) continue;
			char *bb = dcb(rui_jnum(pt, "bytes", 0)), *l = g_strdup_printf("%s (%s%s%s)", *lb ? lb : nm, bb, rui_jstr(dk, "model") ? ", " : "", set_jstr(dk, "model", "")), *el = jesc(l), *en = jesc(nm), *ef = jesc(fs), *em = jesc(set_jstr(pt, "mount", ""));
			g_string_append_printf(o, "%s{\"dev\":\"%s\",\"mount\":\"%s\",\"fs\":\"%s\",\"removable\":%s,\"label\":\"%s\"}", n++ ? "," : "", en, em, ef, rui_jbool(dk, "removable") ? "true" : "false", el); g_free(bb); g_free(l); g_free(el); g_free(en); g_free(ef); g_free(em); } }
	g_string_append(o, "]}"); RuiJ *x = rui_jparse(o->str); g_string_free(o, TRUE); rui_jfree(r); return x;
}
static gboolean bs_job(gpointer d)
{
	/* (brJob: rudolfd's job of that name, until it ends) */
	Bs *b = BS; const char *want = d; if (!b) return G_SOURCE_REMOVE; RuiJ *j = rui_get("/api/job"); if (!j) return G_SOURCE_CONTINUE; const char *nm = rui_jstr(j, "name"); if (nm && strcmp(nm, want)) { rui_jfree(j); return G_SOURCE_CONTINUE; }
	const char *log = set_jstr(j, "log", "");
	if (!strcmp(want, "bring-copy")) { double v[4]; if (prog(log, v)) { b->barv = v[3] ? v[2] / v[3] * 1000 : v[0] / fmax(1, v[1]) * 1000; char *x = brb(v[2]), *y = brb(v[3]), *t = g_strdup_printf("%.0f of %.0f files · %s of %s", v[0], v[1], x, y); gtk_label_set_text(GTK_LABEL(b->pt), t); g_free(x); g_free(y); g_free(t); gtk_widget_queue_draw(b->bar); }
		char *tx = brtext(log); gtk_label_set_text(GTK_LABEL(b->log), tx); g_free(tx); }
	int run = rui_jbool(j, "running"); double st = rui_jnum(j, "status", -1); if (run || st < 0) { rui_jfree(j); return G_SOURCE_CONTINUE; }
	b->tick = 0; b->busy = 0; char *tx = brtext(log); rui_jfree(j);
	if (!strcmp(want, "bring-scan")) {
		if (st != 0) { g_strstrip(tx); const char *l = strrchr(tx, '\n'); char *m = g_strdup_printf("The disk could not be looked at: %s. Nothing was changed.", *tx ? (l ? l + 1 : tx) : "it could not be looked at"); bs_msg(m); g_free(m); gtk_button_set_label(GTK_BUTTON(b->go), rui_tr("Look again")); gtk_widget_set_sensitive(b->go, TRUE); g_object_set_data(G_OBJECT(b->go), "do", "look"); g_free(tx); return G_SOURCE_REMOVE; }
		char *e = qe(b->disk), p[200]; snprintf(p, sizeof p, "/api/bring/result?disk=%s", e); g_free(e); rui_jfree(b->R); b->R = rui_get(p);
		if (!b->R || !rui_jbool(b->R, "found")) { g_free(tx); bs_end(1); return G_SOURCE_REMOVE; } bs_draw(0); g_free(tx); return G_SOURCE_REMOVE; }
	/* the copy's end */
	gtk_widget_set_sensitive(b->cancel, TRUE); gtk_button_set_label(GTK_BUTTON(b->cancel), rui_tr("Cancel"));
	if (st == 0) { b->cont = 0; bs_msg("Copied and checked: every file was read back from the drive and matches. Keep the drive until your files are in Rudolf OS (it offers to bring them in after you sign in).");
		gtk_button_set_label(GTK_BUTTON(b->go), rui_tr("Continue to install")); gtk_widget_set_sensitive(b->go, TRUE); g_object_set_data(G_OBJECT(b->go), "do", "end"); }
	else if (st == 3) { bs_msg("Copied and checked, except files that could not be read (listed above). They are not kept."); gtk_button_set_label(GTK_BUTTON(b->go), rui_tr("Erase anyway (they are not kept)")); gtk_widget_set_sensitive(b->go, TRUE); g_object_set_data(G_OBJECT(b->go), "do", "partial"); }
	else if (st == 28) { b->cont = 1; bs_msg("The drive is full: what is on it is checked. Safely remove it, plug in another drive, choose it below and continue: it takes the rest. Nothing is erased until everything is copied."); bs_draw(1); }
	else { b->cont = 0; char *m = g_strdup_printf("%s Nothing was erased: the disk is as it was.", st == 130 ? "Stopped." : st == 27 ? "Not copied: files over 4 GB cannot go on a FAT32 drive (listed above). Use an exFAT or NTFS drive, or untick their folders." : st == 9 ? "The copy read back from the drive does not match: the drive may be failing. Try another drive." : "The copy did not finish."); bs_msg(m); g_free(m); bs_draw(1); }
	g_free(tx); return G_SOURCE_REMOVE;
}
static void bs_look(void)
{
	Bs *b = BS; b->busy = 1; gtk_widget_set_sensitive(b->go, FALSE); bs_clear(b->list); char *m = g_strdup_printf("Looking at what is on %s (read-only: nothing on it is changed)…", b->disk); bs_msg(m); g_free(m);
	char *e = qe(b->disk), p[200], err[300]; snprintf(p, sizeof p, "/api/bring/scan?disk=%s", e); g_free(e);
	if (post(p, NULL, err, sizeof err) != 200) { b->busy = 0; char *x = g_strdup_printf("The disk could not be looked at: %s. Nothing was changed.", err); bs_msg(x); g_free(x); gtk_button_set_label(GTK_BUTTON(b->go), rui_tr("Look again")); gtk_widget_set_sensitive(b->go, TRUE); g_object_set_data(G_OBJECT(b->go), "do", "look"); return; }
	b->tick = g_timeout_add(700, bs_job, "bring-scan");
}
static void bs_check(int on, void *d) { (void)on; (void)d; bs_sum(); }
static void bs_mode(int on, void *d) { if (!BS) return; if (on) BS->mode = GPOINTER_TO_INT(d); bs_draw(1); }
static void bs_target(const char *v, void *d) { (void)v; (void)d; bs_sum(); }
static void bs_relook(GtkButton *x, gpointer d) { (void)x; (void)d; bs_draw(1); }
static void bs_again(GtkButton *x, gpointer d) { (void)x; (void)d; bs_look(); }
static void bs_unlock(GtkButton *x, gpointer d)
{
	GtkWidget *en = d; const char *part = g_object_get_data(G_OBJECT(x), "part"); const char *key = gtk_entry_get_text(GTK_ENTRY(en)); char *k = g_strstrip(g_strdup(key)); if (!*k) { g_free(k); return; }
	char *e = qe(part), p[200], m[300]; snprintf(p, sizeof p, "/api/bring/unlock?part=%s", e); g_free(e); int c = post(p, k, m, sizeof m); explicit_bzero(k, strlen(k)); g_free(k); gtk_entry_set_text(GTK_ENTRY(en), "");
	if (c == 200) bs_look(); else bs_msg(c == 403 || strstr(m, "does not open") ? "That key does not open it. Check the digits (the key ID shown by Windows helps find the right one)." : m);
}
static const char *locked_text(const char *k)
{
	if (!strcmp(k, "bitlocker")) return "Windows on this partition is locked with BitLocker (many Windows 11 laptops are, often without their owner knowing). Type its recovery key to read it: 48 digits, usually kept in the Microsoft account of whoever set the computer up (account.microsoft.com/devices/recoverykey, on a phone or another computer), on a printout, in a file saved on a USB drive, or with the company or school that gave the computer. It is used only to open the disk read-only, and never kept.";
	if (!strcmp(k, "apfs")) return "This disk has macOS on it (APFS). Rudolf OS cannot read it. Copy your files on the Mac first: start the Mac, plug in a USB drive, open Finder and drag your Documents, Pictures, Music, Movies and Desktop folders onto the drive. Then install Rudolf OS.";
	if (!strcmp(k, "hfs")) return "This disk has an older macOS on it (Mac OS Extended). Rudolf OS cannot read it. Copy your files on the Mac first (Finder: drag Documents, Pictures, Music, Movies and Desktop onto a USB drive), then install.";
	if (!strcmp(k, "luks")) return "This partition is an encrypted Linux one (LUKS). Rudolf OS cannot open it here. Copy your files from that Linux first, then install.";
	if (!strcmp(k, "ntfs-unreadable")) return "Windows on this partition could not be read. Windows may not have been shut down fully, or the disk needs checking: start Windows, hold Shift while choosing Shut down (or run chkdsk), then look again.";
	return "Rudolf OS cannot read this partition.";
}
static const char *fkey(const char *k) { static const char *const K[][2] = { { "Desktop", "Desktop" }, { "Documents", "Documents" }, { "Pictures", "Pictures" }, { "Music", "Music" }, { "Videos", "Videos" }, { "Downloads", "Downloads" }, { "Other", "Other files and folders" } }; for (guint i = 0; i < G_N_ELEMENTS(K); i++) if (!strcmp(K[i][0], k)) return K[i][1]; return k; }
static void bs_draw(int keep)
{
	Bs *b = BS; bs_clear(b->list); int people = 0; if (b->checks) g_ptr_array_unref(b->checks); b->checks = g_ptr_array_new();
	if (!keep) { char *m = g_strdup_printf("%s has another system on it. Its personal files can be kept: copied onto a USB drive and checked before anything is erased, then brought into your account after Rudolf OS starts.", b->disk); bs_msg(m); g_free(m); }
	RuiJ *sy = rui_jprop(b->R, "systems");
	for (int i = 0; i < rui_jlen(sy); i++) { RuiJ *s = rui_jat(sy, i); char *h = g_strdup_printf("%s (%s)", set_jstr(s, "name", ""), set_jstr(s, "part", "")); gtk_box_pack_start(GTK_BOX(b->list), rui_heading(h), FALSE, FALSE, 0); g_free(h);
		const char *stt = set_jstr(s, "state", ""); if (!strcmp(stt, "dirty")) gtk_box_pack_start(GTK_BOX(b->list), warn("Windows was not shut down cleanly. Its files are read as they are on the disk (read-only); changes made just before may be missing. To be sure, start Windows, hold Shift while choosing Shut down, then look again."), FALSE, FALSE, 0);
		if (!strcmp(stt, "hibernated")) gtk_box_pack_start(GTK_BOX(b->list), warn("Windows is hibernated, or uses Fast Startup. Its files are read as they are on the disk (read-only); files saved just before it went to sleep may be missing. To be sure, start Windows, hold Shift while choosing Shut down (or turn Fast Startup off in Power Options), then look again."), FALSE, FALSE, 0);
		RuiJ *pp = rui_jprop(s, "people"); for (int k = 0; k < rui_jlen(pp); k++) { RuiJ *p = rui_jat(pp, k), *fl = rui_jprop(p, "folders"); if (!rui_jlen(fl)) continue; people++;
			char *bb = brb(rui_jnum(p, "bytes", 0)), *t = g_strdup_printf("%s — %s", set_jstr(p, "name", ""), bb); GtkWidget *l = lab(t, 0); gtk_widget_set_margin_top(l, 6); gtk_box_pack_start(GTK_BOX(b->list), l, FALSE, FALSE, 0); g_free(bb); g_free(t);
			for (int f = 0; f < rui_jlen(fl); f++) { RuiJ *x = rui_jat(fl, f); char *fb = brb(rui_jnum(x, "bytes", 0)); double nf = rui_jnum(x, "files", 0); char *ft = g_strdup_printf("%s — %s (%.0f file%s)", fkey(set_jstr(x, "k", "")), fb, nf, nf == 1 ? "" : "s");
				GtkWidget *c = rui_check(ft, 1, bs_check, NULL); g_object_set_data_full(G_OBJECT(c), "id", g_strdup(set_jstr(p, "id", "")), g_free); g_object_set_data_full(G_OBJECT(c), "k", g_strdup(set_jstr(x, "k", "")), g_free); g_object_set_data(G_OBJECT(c), "b", GINT_TO_POINTER((int)(rui_jnum(x, "bytes", 0) / 1024)));
				gtk_widget_set_sensitive(c, !b->cont); gtk_box_pack_start(GTK_BOX(b->list), c, FALSE, FALSE, 0); g_ptr_array_add(b->checks, c); char nm[24]; snprintf(nm, sizeof nm, "bsf%u", b->checks->len - 1); p_label_named(nm, c); g_free(fb); g_free(ft); } } }
	RuiJ *lk = rui_jprop(b->R, "locked");
	for (int i = 0; i < rui_jlen(lk); i++) { RuiJ *L = rui_jat(lk, i); const char *k = set_jstr(L, "kind", ""); char *h = g_strdup_printf("%s (%s)", !strcmp(k, "bitlocker") ? "Locked with BitLocker" : !strcmp(k, "apfs") || !strcmp(k, "hfs") ? "macOS" : !strcmp(k, "luks") ? "Encrypted Linux" : "Windows", set_jstr(L, "part", ""));
		gtk_box_pack_start(GTK_BOX(b->list), rui_heading(h), FALSE, FALSE, 0); g_free(h); gtk_box_pack_start(GTK_BOX(b->list), lab(locked_text(k), 1), FALSE, FALSE, 0);
		if (!strcmp(k, "bitlocker")) { GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8), *en = rui_entry("123456-123456-123456-123456-123456-123456-123456-123456"), *bt = rui_button("Open it (read-only)", 0); gtk_entry_set_visibility(GTK_ENTRY(en), TRUE); gtk_widget_set_hexpand(en, TRUE);
			g_object_set_data_full(G_OBJECT(bt), "part", g_strdup(set_jstr(L, "part", "")), g_free); g_signal_connect(bt, "clicked", G_CALLBACK(bs_unlock), en); gtk_box_pack_start(GTK_BOX(row), en, TRUE, TRUE, 0); gtk_box_pack_start(GTK_BOX(row), bt, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(b->list), row, FALSE, FALSE, 0); p_label_named("bskey", en); } }
	double lost = rui_jnum(b->R, "total", 0); char *lb = brb(lost);
	/* keep or erase (the page's radio buttons) */
	if (!people) b->mode = 1;
	if (people) { GtkWidget *r = rui_check("Keep them: copy the folders ticked above onto a USB drive first (they are checked before anything is erased)", b->mode == 0, bs_mode, GINT_TO_POINTER(0)); gtk_widget_set_margin_top(r, 10); gtk_box_pack_start(GTK_BOX(b->list), r, FALSE, FALSE, 0); p_label_named("bskeep", r); }
	char *et = people ? g_strdup_printf("Erase them without keeping: %s of personal files on %s will be lost for good", lb, b->disk) : g_strdup_printf("Erase %s without keeping anything from it: what is on it will be lost for good", b->disk);
	GtkWidget *r2 = rui_check(et, b->mode == 1, bs_mode, GINT_TO_POINTER(1)); gtk_box_pack_start(GTK_BOX(b->list), r2, FALSE, FALSE, 0); p_label_named("bserase", r2); g_free(et); g_free(lb);
	if (b->mode == 0) { GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8); gtk_box_pack_start(GTK_BOX(row), rui_label("Copy onto"), FALSE, FALSE, 0);
		rui_jfree(b->T); b->T = targets(b->disk); RuiJ *tl = rui_jprop(b->T, "x"); const char *v[16], *l[16]; char vs[16][8]; int n = 0;
		if (!rui_jlen(tl)) { v[n] = ""; l[n] = "No USB drive: plug one in, then choose Look for drives"; n++; }
		for (int i = 0; i < rui_jlen(tl) && n < 16; i++) { snprintf(vs[n], 8, "%d", i); v[n] = vs[n]; l[n] = rui_jstr(rui_jat(tl, i), "label"); n++; }
		b->tgt = s_select(v, l, n, n ? v[0] : "", bs_target, NULL); s_select_width(b->tgt, 300); gtk_box_pack_start(GTK_BOX(row), b->tgt, FALSE, FALSE, 0); p_label_named("bsto", b->tgt);
		GtkWidget *rl = rui_button("Look for drives", 0); g_signal_connect(rl, "clicked", G_CALLBACK(bs_relook), NULL); gtk_box_pack_start(GTK_BOX(row), rl, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(b->list), row, FALSE, FALSE, 0); }
	else b->tgt = NULL;
	GtkWidget *nt = lab("", 1); g_object_set_data(G_OBJECT(b->list), "nt", nt); gtk_box_pack_start(GTK_BOX(b->list), nt, FALSE, FALSE, 0);
	GtkWidget *la = rui_button("Look again", 0); g_signal_connect(la, "clicked", G_CALLBACK(bs_again), NULL); gtk_widget_set_halign(la, GTK_ALIGN_START); gtk_box_pack_start(GTK_BOX(b->list), la, FALSE, FALSE, 0);
	if (!b->name[0]) { GDateTime *t = g_date_time_new_from_unix_utc((gint64)set_now()); char *f = g_date_time_format(t, "Old computer %Y-%m-%d %H.%M"); snprintf(b->name, sizeof b->name, "%s", f); g_free(f); g_date_time_unref(t); }
	gtk_widget_show_all(b->list); bs_sum();
}
static void bs_sum(void)
{
	Bs *b = BS; if (!b) return; GtkWidget *nt = g_object_get_data(G_OBJECT(b->list), "nt"); int n = 0; double tot = 0;
	for (guint i = 0; b->checks && i < b->checks->len; i++) { GtkWidget *c = g_ptr_array_index(b->checks, i); if (rui_check_get(c)) { n++; tot += GPOINTER_TO_INT(g_object_get_data(G_OBJECT(c), "b")) * 1024.0; } }
	RuiJ *t = b->tgt ? rui_jat(rui_jprop(b->T, "x"), atoi(s_select_get(b->tgt))) : NULL; if (b->tgt && !*s_select_get(b->tgt)) t = NULL;
	if (b->mode == 0) { char *tb = brb(tot), *m = n ? g_strdup_printf("%d folder(s), %s, are copied%s%s into “" BR_TOP "\\%s” and checked.%s", n, tb, t ? " onto " : "", t ? rui_jstr(t, "label") : "", b->name, t && !g_strcmp0(rui_jstr(t, "fs"), "vfat") ? " This drive is FAT32: a file over 4 GB cannot go on it (an exFAT or NTFS drive takes them)." : "") : g_strdup("Tick the folders to keep.");
		if (nt) gtk_label_set_text(GTK_LABEL(nt), m); g_free(m); g_free(tb); gtk_button_set_label(GTK_BUTTON(b->go), rui_tr(b->cont ? "Continue onto this drive" : "Copy and check")); gtk_widget_set_sensitive(b->go, n && t); g_object_set_data(G_OBJECT(b->go), "do", "copy"); }
	else { if (nt) gtk_label_set_text(GTK_LABEL(nt), ""); gtk_button_set_label(GTK_BUTTON(b->go), rui_tr("Erase without keeping")); gtk_widget_set_sensitive(b->go, TRUE); g_object_set_data(G_OBJECT(b->go), "do", "erase"); }
	set_state_soon();
}
static gboolean bar2_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)d; double z = s_z(), W = gtk_widget_get_allocated_width(w) / z, H = gtk_widget_get_allocated_height(w) / z; cairo_save(cr); cairo_scale(cr, z, z); cairo_set_source_rgba(cr, .5, .5, .5, .25); s_rrect(cr, 0, 0, W, H, H / 2); cairo_fill(cr); s_col(cr, "ac", 1); s_rrect(cr, 0, 0, W * (BS ? fmin(1, BS->barv / 1000) : 0), H, H / 2); cairo_fill(cr); cairo_restore(cr); return TRUE; }
static void bs_copy(void)
{
	Bs *b = BS; RuiJ *t = b->tgt ? rui_jat(rui_jprop(b->T, "x"), atoi(s_select_get(b->tgt))) : NULL; if (!t) return; GString *ids = g_string_new(NULL);
	for (guint i = 0; i < b->checks->len; i++) { GtkWidget *c = g_ptr_array_index(b->checks, i); if (rui_check_get(c)) g_string_append_printf(ids, "%s\t%s\n", (char *)g_object_get_data(G_OBJECT(c), "id"), (char *)g_object_get_data(G_OBJECT(c), "k")); }
	if (!ids->len) { g_string_free(ids, TRUE); return; }
	b->busy = 1; gtk_widget_set_sensitive(b->go, FALSE); gtk_widget_set_sensitive(b->list, FALSE); gtk_widget_set_visible(b->bar, TRUE); gtk_widget_set_visible(b->pt, TRUE); gtk_widget_set_visible(b->log, TRUE); gtk_button_set_label(GTK_BUTTON(b->cancel), rui_tr("Stop"));
	char m[300]; const char *mt = rui_jstr(t, "mount"); char *mount = NULL;
	if (!mt || !*mt) { char *e = qe(rui_jstr(t, "dev")), p[200]; snprintf(p, sizeof p, "/api/mount?dev=%s", e); g_free(e); if (post(p, NULL, m, sizeof m) != 200) { bs_msg(m); b->busy = 0; gtk_widget_set_sensitive(b->list, TRUE); bs_sum(); g_string_free(ids, TRUE); return; } mount = g_strdup_printf("/media/%s", rui_jstr(t, "dev")); }
	else mount = g_strdup(mt);
	bs_msg("Copying… leave the drive plugged in. Nothing is erased until every file is copied and checked.");
	char *body = g_strdup_printf("%s continue=%d\n%s/" BR_TOP "/%s\n%s", b->disk, b->cont, mount, b->name, ids->str); g_free(mount); g_string_free(ids, TRUE);
	if (post("/api/bring/copy", body, m, sizeof m) != 200) { g_free(body); bs_msg(m); b->busy = 0; gtk_widget_set_sensitive(b->list, TRUE); bs_draw(1); return; } g_free(body);
	b->tick = g_timeout_add(700, bs_job, "bring-copy");
}
static void bs_go(GtkButton *x, gpointer d)
{
	(void)x; (void)d; Bs *b = BS; if (!b) return; const char *w = g_object_get_data(G_OBJECT(b->go), "do"); if (!w) return; char m[300];
	if (!strcmp(w, "look")) bs_look(); else if (!strcmp(w, "end")) bs_end(1); else if (!strcmp(w, "copy")) bs_copy();
	else { char *e = qe(b->disk), p[200]; snprintf(p, sizeof p, "/api/bring/decide?disk=%s&what=%s", e, !strcmp(w, "erase") ? "erase" : "partial"); g_free(e); gtk_widget_set_sensitive(b->go, FALSE);
		if (!strcmp(w, "erase") || !strcmp(w, "partial")) { if (post(p, NULL, m, sizeof m) == 200) bs_end(1); else { bs_msg(m); gtk_widget_set_sensitive(b->go, TRUE); } } }
}
static gboolean dim_draw(GtkWidget *w, cairo_t *cr, gpointer d) { (void)d; GtkStyleContext *c = gtk_widget_get_style_context(w); gtk_render_background(c, cr, 0, 0, gtk_widget_get_allocated_width(w), gtk_widget_get_allocated_height(w)); return FALSE; }
static gboolean bs_key(GtkWidget *w, GdkEventKey *e, gpointer d) { (void)w; (void)d; if (e->keyval == GDK_KEY_Escape && BS && !BS->busy) { bs_end(0); return TRUE; } return FALSE; }
void bring_step(const char *disk, void (*cb)(int ok, void *d), void *d)
{
	if (BS) bs_end(0); Bs *b = g_new0(Bs, 1); BS = b; snprintf(b->disk, sizeof b->disk, "%s", disk); b->cb = cb; b->d = d;
	b->dim = gtk_event_box_new(); gtk_style_context_add_class(gtk_widget_get_style_context(b->dim), "rui-dim"); g_signal_connect(b->dim, "draw", G_CALLBACK(dim_draw), NULL);
	b->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6); gtk_style_context_add_class(gtk_widget_get_style_context(b->box), "rui-dlg"); gtk_widget_set_halign(b->box, GTK_ALIGN_CENTER); gtk_widget_set_valign(b->box, GTK_ALIGN_CENTER); gtk_widget_set_size_request(b->box, (int)lround(620 * s_z()), -1);
	GtkWidget *sc = gtk_scrolled_window_new(NULL, NULL); gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC); gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(sc), TRUE); gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(sc), (int)lround(520 * s_z()));
	gtk_container_add(GTK_CONTAINER(b->dim), sc); gtk_container_add(GTK_CONTAINER(sc), b->box);
	GtkWidget *t = gtk_label_new(rui_tr("Bring your files")); gtk_style_context_add_class(gtk_widget_get_style_context(t), "rui-t"); gtk_label_set_xalign(GTK_LABEL(t), 0); gtk_box_pack_start(GTK_BOX(b->box), t, FALSE, FALSE, 0);
	b->msg = lab("", 1); gtk_box_pack_start(GTK_BOX(b->box), b->msg, FALSE, FALSE, 0);
	b->list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4); gtk_box_pack_start(GTK_BOX(b->box), b->list, FALSE, FALSE, 0);
	b->bar = gtk_drawing_area_new(); gtk_widget_set_size_request(b->bar, -1, (int)lround(12 * s_z())); g_signal_connect(b->bar, "draw", G_CALLBACK(bar2_draw), NULL); gtk_box_pack_start(GTK_BOX(b->box), b->bar, FALSE, FALSE, 0);
	b->pt = lab("", 1); gtk_box_pack_start(GTK_BOX(b->box), b->pt, FALSE, FALSE, 0); b->log = lab("", 1); gtk_box_pack_start(GTK_BOX(b->box), b->log, FALSE, FALSE, 0);
	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8); gtk_widget_set_halign(row, GTK_ALIGN_END); b->cancel = rui_button("Cancel", 0); b->go = rui_button("Next", 1); gtk_widget_set_sensitive(b->go, FALSE);
	g_signal_connect(b->cancel, "clicked", G_CALLBACK(bs_cancel), NULL); g_signal_connect(b->go, "clicked", G_CALLBACK(bs_go), NULL); gtk_box_pack_start(GTK_BOX(row), b->cancel, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(row), b->go, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(b->box), row, FALSE, FALSE, 0);
	p_label_named("bsgo", b->go); p_label_named("bscancel", b->cancel); static int keyed; if (!keyed++) g_signal_connect(SW->win, "key-press-event", G_CALLBACK(bs_key), NULL);
	gtk_overlay_add_overlay(GTK_OVERLAY(SW->overlay), b->dim); gtk_widget_show_all(b->dim); gtk_widget_set_visible(b->bar, FALSE); gtk_widget_set_visible(b->pt, FALSE); gtk_widget_set_visible(b->log, FALSE);
	set_note("bringstep %s", disk); bs_look();
}
