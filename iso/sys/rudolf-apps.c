/* rudolf-apps: the Linux app catalogue and the list of installed Linux apps for Rudolf OS.
 *
 *   rudolf-apps catalog [OUT.json]   desktop apps in Flathub's AppStream catalogue (the file Flatpak
 *                                    downloads with `flatpak update --appstream`), as compact JSON
 *   rudolf-apps info ID              one app in full: description, screenshots, release, links
 *   rudolf-apps installed            installed apps with their names, sizes, launchers and file types
 *
 * RA_APPSTREAM overrides the catalogue file, RA_EXPORTS the exported launchers folder (tests).
 * Copyright (C) 2026 the Rudolf OS authors. SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>

#define APPSTREAM "/var/lib/flatpak/appstream/flathub/x86_64/active/appstream.xml.gz"
#define EXPORTS "/var/lib/flatpak/exports/share"

typedef struct { char *b; size_t n, cap; } str;
static void sput(str *s, const char *p, size_t n)
{
	if (s->n + n + 1 > s->cap) { s->cap = (s->n + n + 1) * 2 + 256; s->b = realloc(s->b, s->cap); }
	memcpy(s->b + s->n, p, n); s->n += n; s->b[s->n] = 0;
}
static void sadd(str *s, const char *p) { sput(s, p, strlen(p)); }
static void sjson(str *s, const char *v)
{
	sadd(s, "\"");
	for (const unsigned char *p = (const unsigned char *)(v ? v : ""); *p; p++) {
		char e[8];
		if (*p == '"' || *p == '\\') { e[0] = '\\'; e[1] = *p; sput(s, e, 2); }
		else if (*p == '\n') sadd(s, "\\n");
		else if (*p < 0x20) { snprintf(e, sizeof e, "\\u%04x", *p); sadd(s, e); }
		else sput(s, (const char *)p, 1);
	}
	sadd(s, "\"");
}

/* ---------------- reading the (gzipped) catalogue */
static char *read_gz(const char *path, size_t *len)
{
	gzFile g = gzopen(path, "rb"); if (!g) return NULL;
	str s = {0}; char b[65536]; int n;
	while ((n = gzread(g, b, sizeof b)) > 0) sput(&s, b, n);
	gzclose(g); if (len) *len = s.n; return s.b;
}

/* ---------------- a small XML reader: elements, attributes, text, entities */
typedef struct node { char *tag; char *attrs; char *text; struct node *kid, *next; } node;
static void unescape(char *s)
{
	char *o = s;
	while (*s) {
		if (*s == '&') {
			if (!strncmp(s, "&amp;", 5)) { *o++ = '&'; s += 5; continue; }
			if (!strncmp(s, "&lt;", 4)) { *o++ = '<'; s += 4; continue; }
			if (!strncmp(s, "&gt;", 4)) { *o++ = '>'; s += 4; continue; }
			if (!strncmp(s, "&quot;", 6)) { *o++ = '"'; s += 6; continue; }
			if (!strncmp(s, "&apos;", 6)) { *o++ = '\''; s += 6; continue; }
			if (s[1] == '#') {
				char *e; unsigned long c = s[2] == 'x' ? strtoul(s + 3, &e, 16) : strtoul(s + 2, &e, 10);
				if (*e == ';') {
					if (c < 0x80) *o++ = c;
					else if (c < 0x800) { *o++ = 0xC0 | c >> 6; *o++ = 0x80 | (c & 0x3F); }
					else if (c < 0x10000) { *o++ = 0xE0 | c >> 12; *o++ = 0x80 | (c >> 6 & 0x3F); *o++ = 0x80 | (c & 0x3F); }
					else { *o++ = 0xF0 | c >> 18; *o++ = 0x80 | (c >> 12 & 0x3F); *o++ = 0x80 | (c >> 6 & 0x3F); *o++ = 0x80 | (c & 0x3F); }
					s = e + 1; continue;
				}
			}
		}
		*o++ = *s++;
	}
	*o = 0;
}
/* parses the element starting at *p ('<' already checked); returns NULL at a closing tag or the end */
static node *parse_el(char **pp)
{
	char *p = *pp;
	for (;;) {
		while (*p && *p != '<') p++;
		if (!*p) { *pp = p; return NULL; }
		if (!strncmp(p, "<?", 2)) { char *e = strstr(p, "?>"); p = e ? e + 2 : p + strlen(p); continue; }
		if (!strncmp(p, "<!--", 4)) { char *e = strstr(p, "-->"); p = e ? e + 3 : p + strlen(p); continue; }
		if (!strncmp(p, "<!", 2)) { char *e = strchr(p, '>'); p = e ? e + 1 : p + strlen(p); continue; }
		break;
	}
	if (p[1] == '/') { char *e = strchr(p, '>'); *pp = e ? e + 1 : p + strlen(p); return NULL; }
	node *n = calloc(1, sizeof *n);
	char *t = p + 1, *te = t; while (*te && !isspace((unsigned char)*te) && *te != '>' && *te != '/') te++;
	n->tag = strndup(t, te - t);
	char *gt = te; char q = 0;
	while (*gt && (q || *gt != '>')) { if (q) { if (*gt == q) q = 0; } else if (*gt == '"' || *gt == '\'') q = *gt; gt++; }
	int self = gt > te && gt[-1] == '/';
	n->attrs = strndup(te, gt - te - (self ? 1 : 0));
	p = *gt ? gt + 1 : gt;
	if (self) { *pp = p; return n; }
	str text = {0}; node **last = &n->kid;
	for (;;) {
		char *lt = strchr(p, '<');
		if (!lt) { p += strlen(p); break; }
		if (!strncmp(lt, "<![CDATA[", 9)) { sput(&text, p, lt - p); char *e = strstr(lt, "]]>"); if (!e) { p = lt + strlen(lt); break; } sput(&text, lt + 9, e - lt - 9); p = e + 3; continue; }
		sput(&text, p, lt - p); p = lt;
		if (p[1] == '/') { char *e = strchr(p, '>'); p = e ? e + 1 : p + strlen(p); break; }
		node *k = parse_el(&p);
		if (k) { *last = k; last = &k->next;
			static const char *inl[] = {"em", "code", "strong", "b", "i", "a", "span", NULL};
			for (int i = 0; inl[i]; i++) if (!strcmp(k->tag, inl[i])) { sput(&text, k->text, strlen(k->text)); break; } }
		/* keep the words of inline markup (descriptions use <p>, <li>, <em>, <code>) in order */
	}
	if (text.b) { unescape(text.b); n->text = text.b; } else n->text = strdup("");
	*pp = p; return n;
}
static void free_node(node *n) { while (n) { node *nx = n->next; free_node(n->kid); free(n->tag); free(n->attrs); free(n->text); free(n); n = nx; } }
static int attr(const node *n, const char *name, char *out, size_t on)
{
	const char *a = n->attrs; size_t l = strlen(name);
	while (a && (a = strstr(a, name))) {
		if ((a == n->attrs || isspace((unsigned char)a[-1])) && a[l] == '=' && (a[l + 1] == '"' || a[l + 1] == '\'')) {
			char q = a[l + 1]; const char *v = a + l + 2, *e = strchr(v, q); if (!e) return 0;
			snprintf(out, on, "%.*s", (int)(e - v), v); unescape(out); return 1;
		}
		a += l;
	}
	return 0;
}
static int localized(const node *n) { char b[16]; return attr(n, "xml:lang", b, sizeof b); }
static const node *kid(const node *n, const char *tag) { for (const node *k = n ? n->kid : NULL; k; k = k->next) if (!strcmp(k->tag, tag) && !localized(k)) return k; return NULL; }
static const char *ktext(const node *n, const char *tag) { const node *k = kid(n, tag); return k ? k->text : ""; }
static void trim(char *s) { char *a = s; while (isspace((unsigned char)*a)) a++; memmove(s, a, strlen(a) + 1); size_t l = strlen(s); while (l && isspace((unsigned char)s[l - 1])) s[--l] = 0; }
static int is_app(const node *c)
{
	char t[40] = ""; attr(c, "type", t, sizeof t);
	return !strcmp(t, "desktop-application") || !strcmp(t, "desktop");
}
static int verified(const node *c)
{
	const node *cu = kid(c, "custom"); if (!cu) cu = kid(c, "metadata");
	for (const node *v = cu ? cu->kid : NULL; v; v = v->next) { char k[80]; if (attr(v, "key", k, sizeof k) && !strcmp(k, "flathub::verification::verified")) return !strcmp(v->text, "true"); }
	return 0;
}
static const char *developer(const node *c)
{
	const node *d = kid(c, "developer"); if (d) { const char *n = ktext(d, "name"); if (*n) return n; }
	return ktext(c, "developer_name");
}
static node *load(void)
{
	const char *f = getenv("RA_APPSTREAM") ? getenv("RA_APPSTREAM") : APPSTREAM;
	size_t len; char *x = read_gz(f, &len); if (!x) return NULL;
	char *p = x; node *root = NULL;
	while (*p && !root) { root = parse_el(&p); if (root && strcmp(root->tag, "components")) { free_node(root); root = NULL; } }
	free(x); return root;
}

static int cmd_catalog(const char *out)
{
	node *root = load(); if (!root) { printf("{\"ok\":false,\"error\":\"the Flathub catalogue has not been downloaded yet\"}\n"); return 1; }
	str s = {0}; sadd(&s, "{\"ok\":true,\"apps\":["); int first = 1, n = 0;
	for (const node *c = root->kid; c; c = c->next) {
		if (strcmp(c->tag, "component") || !is_app(c)) continue;
		char id[256]; snprintf(id, sizeof id, "%s", ktext(c, "id")); trim(id); if (!*id) continue;
		size_t l = strlen(id); if (l > 8 && !strcmp(id + l - 8, ".desktop")) id[l - 8] = 0;
		char nm[256], su[400]; snprintf(nm, sizeof nm, "%s", ktext(c, "name")); trim(nm); snprintf(su, sizeof su, "%s", ktext(c, "summary")); trim(su);
		if (!first) sadd(&s, ","); first = 0; n++;
		sadd(&s, "{\"id\":"); sjson(&s, id); sadd(&s, ",\"name\":"); sjson(&s, *nm ? nm : id); sadd(&s, ",\"summary\":"); sjson(&s, su);
		sadd(&s, ",\"cats\":["); const node *cs = kid(c, "categories"); int f2 = 1;
		for (const node *k = cs ? cs->kid : NULL; k; k = k->next) { if (!f2) sadd(&s, ","); f2 = 0; char t[80]; snprintf(t, sizeof t, "%s", k->text); trim(t); sjson(&s, t); }
		sadd(&s, "],\"verified\":"); sadd(&s, verified(c) ? "true" : "false");
		sadd(&s, ",\"dev\":"); { char d[200]; snprintf(d, sizeof d, "%s", developer(c)); trim(d); sjson(&s, d); }
		sadd(&s, "}");
	}
	sadd(&s, "]}\n"); free_node(root);
	if (out) {
		char tmp[600]; snprintf(tmp, sizeof tmp, "%s.tmp", out);
		FILE *f = fopen(tmp, "w"); if (!f || fwrite(s.b, 1, s.n, f) != s.n || fclose(f) || rename(tmp, out)) { printf("{\"ok\":false,\"error\":\"cannot write the catalogue\"}\n"); return 1; }
		printf("{\"ok\":true,\"apps\":%d}\n", n);
	} else fwrite(s.b, 1, s.n, stdout);
	return 0;
}

/* description markup -> plain text: paragraphs separated by blank lines, list items with a bullet */
static void desc_text(const node *d, str *o)
{
	for (const node *k = d ? d->kid : NULL; k; k = k->next) {
		if (localized(k)) continue;
		if (!strcmp(k->tag, "p")) { char *t = strdup(k->text); trim(t); if (*t) { if (o->n) sadd(o, "\n\n"); sadd(o, t); } free(t); }
		else if (!strcmp(k->tag, "ul") || !strcmp(k->tag, "ol")) {
			for (const node *li = k->kid; li; li = li->next) { if (localized(li)) continue; char *t = strdup(li->text); trim(t); if (*t) { sadd(o, o->n ? "\n" : ""); sadd(o, "\xe2\x80\xa2 "); sadd(o, t); } free(t); }
		}
	}
}
static void remote_extra(const char *id, str *s);
static int cmd_info(const char *want, int remote)
{
	node *root = load(); if (!root) { printf("{\"ok\":false,\"error\":\"the Flathub catalogue has not been downloaded yet\"}\n"); return 1; }
	for (const node *c = root->kid; c; c = c->next) {
		if (strcmp(c->tag, "component")) continue;
		char id[256]; snprintf(id, sizeof id, "%s", ktext(c, "id")); trim(id);
		size_t l = strlen(id); if (l > 8 && !strcmp(id + l - 8, ".desktop")) id[l - 8] = 0;
		if (strcmp(id, want)) continue;
		str s = {0}; sadd(&s, "{\"ok\":true,\"id\":"); sjson(&s, id);
		char nm[256]; snprintf(nm, sizeof nm, "%s", ktext(c, "name")); trim(nm); sadd(&s, ",\"name\":"); sjson(&s, nm);
		char su[400]; snprintf(su, sizeof su, "%s", ktext(c, "summary")); trim(su); sadd(&s, ",\"summary\":"); sjson(&s, su);
		str d = {0}; desc_text(kid(c, "description"), &d); sadd(&s, ",\"description\":"); sjson(&s, d.b ? d.b : ""); free(d.b);
		char lic[120]; snprintf(lic, sizeof lic, "%s", ktext(c, "project_license")); trim(lic); sadd(&s, ",\"license\":"); sjson(&s, lic);
		{ char dv[200]; snprintf(dv, sizeof dv, "%s", developer(c)); trim(dv); sadd(&s, ",\"dev\":"); sjson(&s, dv); }
		sadd(&s, ",\"verified\":"); sadd(&s, verified(c) ? "true" : "false");
		sadd(&s, ",\"homepage\":"); { const char *h = ""; for (const node *k = c->kid; k; k = k->next) { char t[20]; if (!strcmp(k->tag, "url") && attr(k, "type", t, sizeof t) && !strcmp(t, "homepage")) { h = k->text; break; } } char hh[400]; snprintf(hh, sizeof hh, "%s", h); trim(hh); sjson(&s, hh); }
		sadd(&s, ",\"version\":"); { const node *rs = kid(c, "releases"); char v[80] = "", ts[40] = "", dt[40] = ""; if (rs && rs->kid) { attr(rs->kid, "version", v, sizeof v); attr(rs->kid, "timestamp", ts, sizeof ts); attr(rs->kid, "date", dt, sizeof dt); } sjson(&s, v); sadd(&s, ",\"released\":"); sjson(&s, *dt ? dt : ts); }
		sadd(&s, ",\"screenshots\":["); { const node *ss = kid(c, "screenshots"); int f = 1, n = 0;
			for (const node *sh = ss ? ss->kid : NULL; sh && n < 5; sh = sh->next) {
				/* the largest thumbnail at most ~1300 px wide, else the source image */
				const char *best = NULL; int bw = 0;
				for (const node *im = sh->kid; im; im = im->next) {
					if (strcmp(im->tag, "image")) continue; char t[20] = "", w[20] = ""; attr(im, "type", t, sizeof t); attr(im, "width", w, sizeof w);
					int wi = atoi(w); if (!strcmp(t, "source") && !best) best = im->text; if (!strcmp(t, "thumbnail") && wi <= 1300 && wi > bw) { bw = wi; best = im->text; }
				}
				if (best) { char u[600]; snprintf(u, sizeof u, "%s", best); trim(u); if (!strncmp(u, "https://", 8)) { if (!f) sadd(&s, ","); f = 0; sjson(&s, u); n++; } }
			} }
		sadd(&s, "],\"cats\":["); { const node *cs = kid(c, "categories"); int f2 = 1; for (const node *k = cs ? cs->kid : NULL; k; k = k->next) { if (!f2) sadd(&s, ","); f2 = 0; char t[80]; snprintf(t, sizeof t, "%s", k->text); trim(t); sjson(&s, t); } }
		sadd(&s, "]"); if (remote) remote_extra(id, &s); sadd(&s, "}\n"); fwrite(s.b, 1, s.n, stdout); free_node(root); return 0;
	}
	free_node(root); printf("{\"ok\":false,\"error\":\"no such app in the catalogue\"}\n"); return 1;
}

/* ---------------- installed apps: `flatpak list` + the launchers Flatpak exports */
static char *desktop_key(const char *text, const char *key)
{
	/* only the [Desktop Entry] group, only the unlocalized key */
	const char *g = strstr(text, "[Desktop Entry]"); if (!g) return NULL;
	size_t kl = strlen(key);
	for (const char *l = strchr(g, '\n'); l; l = strchr(l, '\n')) {
		l++; if (*l == '[') break;
		if (strncmp(l, key, kl)) continue;
		const char *q = l + kl; while (*q == ' ') q++; if (*q != '=') continue;
		q++; while (*q == ' ') q++; const char *e = strchr(q, '\n'); return strndup(q, e ? (size_t)(e - q) : strlen(q));
	}
	return NULL;
}
static char *slurp(const char *p)
{
	FILE *f = fopen(p, "r"); if (!f) return NULL; str s = {0}; char b[8192]; size_t n;
	while ((n = fread(b, 1, sizeof b, f)) > 0) sput(&s, b, n); fclose(f); return s.b ? s.b : strdup("");
}
static int cmd_installed(void)
{
	const char *ex = getenv("RA_EXPORTS") ? getenv("RA_EXPORTS") : EXPORTS;
	FILE *p = popen("LANG=C.UTF-8 flatpak list --system --app --columns=application,name,version,size,origin 2>/dev/null", "r");
	str s = {0}; sadd(&s, "{\"ok\":true,\"apps\":["); int first = 1;
	char line[1200];
	while (p && fgets(line, sizeof line, p)) {
		line[strcspn(line, "\n")] = 0; char *f[5] = {0}; int k = 0; char *sp;
		for (char *t = strtok_r(line, "\t", &sp); t && k < 5; t = strtok_r(NULL, "\t", &sp)) f[k++] = t;
		if (k < 2 || !f[0][0] || strchr(f[0], '/') || strchr(f[0], ' ')) continue;
		if (!first) sadd(&s, ","); first = 0;
		sadd(&s, "{\"id\":"); sjson(&s, f[0]); sadd(&s, ",\"name\":"); sjson(&s, f[1]); sadd(&s, ",\"version\":"); sjson(&s, k > 2 ? f[2] : "");
		if (k > 3) for (char *q = f[3]; *q; q++) if (*q == '?') *q = ' ';
		sadd(&s, ",\"size\":"); sjson(&s, k > 3 ? f[3] : ""); sadd(&s, ",\"origin\":"); sjson(&s, k > 4 ? f[4] : "");
		/* launchers: <id>.desktop and any <id>.*.desktop the app exports */
		sadd(&s, ",\"launchers\":["); char dir[600]; snprintf(dir, sizeof dir, "%s/applications", ex); DIR *d = opendir(dir); int fl = 1;
		struct dirent *e;
		while (d && (e = readdir(d))) {
			size_t il = strlen(f[0]), nl = strlen(e->d_name);
			if (nl < 9 || strcmp(e->d_name + nl - 8, ".desktop") || strncmp(e->d_name, f[0], il) || (e->d_name[il] != '.')) continue;
			char fp[1200]; snprintf(fp, sizeof fp, "%s/%s", dir, e->d_name); char *t = slurp(fp); if (!t) continue;
			char *nd = desktop_key(t, "NoDisplay"), *hid = desktop_key(t, "Hidden");
			if ((nd && !strcmp(nd, "true")) || (hid && !strcmp(hid, "true"))) { free(nd); free(hid); free(t); continue; }
			free(nd); free(hid);
			char *nm = desktop_key(t, "Name"), *ic = desktop_key(t, "Icon"), *mt = desktop_key(t, "MimeType"), *wc = desktop_key(t, "StartupWMClass"), *cm = desktop_key(t, "Comment"), *ca = desktop_key(t, "Categories");
			if (!fl) sadd(&s, ","); fl = 0;
			char lid[300]; snprintf(lid, sizeof lid, "%.*s", (int)(nl - 8), e->d_name);
			sadd(&s, "{\"desktop\":"); sjson(&s, lid); sadd(&s, ",\"name\":"); sjson(&s, nm ? nm : f[1]); sadd(&s, ",\"icon\":"); sjson(&s, ic ? ic : f[0]);
			sadd(&s, ",\"comment\":"); sjson(&s, cm ? cm : ""); sadd(&s, ",\"mime\":"); sjson(&s, mt ? mt : ""); sadd(&s, ",\"wmclass\":"); sjson(&s, wc ? wc : ""); sadd(&s, ",\"cats\":"); sjson(&s, ca ? ca : "");
			sadd(&s, "}"); free(nm); free(ic); free(mt); free(wc); free(cm); free(ca); free(t);
		}
		if (d) closedir(d);
		sadd(&s, "]}");
	}
	if (p) pclose(p);
	sadd(&s, "]}\n"); fwrite(s.b, 1, s.n, stdout); return 0;
}


/* ---------------- download size and permissions, from Flathub (needs the network) */
static int id_ok(const char *id) { if (!id || !*id || strlen(id) > 200) return 0; for (const char *c = id; *c; c++) if (!isalnum((unsigned char)*c) && !strchr("._-", *c)) return 0; return 1; }
static void remote_extra(const char *id, str *s)
{
	if (!id_ok(id)) return;
	char cmd[400], line[1000]; snprintf(cmd, sizeof cmd, "LANG=C.UTF-8 timeout 25 flatpak remote-info --system flathub %s 2>/dev/null", id);
	char dl[64] = "", inst[64] = "", rt[200] = "";
	FILE *p = popen(cmd, "r");
	while (p && fgets(line, sizeof line, p)) {
		char *v = strchr(line, ':'); if (!v) continue; *v++ = 0; char *k = line; trim(k); trim(v);
		for (char *q = v; *q; q++) if (*q == '?') *q = ' ';   /* sizes use a no-break space, printed as ? without a UTF-8 locale */
		if (!strcmp(k, "Download") || !strcmp(k, "Download Size")) snprintf(dl, sizeof dl, "%s", v);
		else if (!strcmp(k, "Installed") || !strcmp(k, "Installed Size")) snprintf(inst, sizeof inst, "%s", v);
		else if (!strcmp(k, "Runtime")) snprintf(rt, sizeof rt, "%s", v);
	}
	if (p) pclose(p);
	sadd(s, ",\"download\":"); sjson(s, dl); sadd(s, ",\"installedSize\":"); sjson(s, inst); sadd(s, ",\"runtime\":"); sjson(s, rt);
	/* is the runtime already here? */
	int have = 0; if (*rt) { char c2[400]; snprintf(c2, sizeof c2, "flatpak info --system %.200s >/dev/null 2>&1", rt); for (char *q = c2; *q; q++) if (*q == '\'' || *q == ';' || *q == '`' || *q == '$') *q = ' '; have = system(c2) == 0; }
	sadd(s, ",\"runtimeInstalled\":"); sadd(s, have ? "true" : "false");
	snprintf(cmd, sizeof cmd, "LANG=C.UTF-8 timeout 25 flatpak remote-info --system --show-metadata flathub %s 2>/dev/null", id);
	p = popen(cmd, "r"); int ctx = 0, first = 1;
	sadd(s, ",\"permissions\":{");
	while (p && fgets(line, sizeof line, p)) {
		line[strcspn(line, "\n")] = 0;
		if (line[0] == '[') { ctx = !strcmp(line, "[Context]"); continue; }
		if (!ctx) continue; char *v = strchr(line, '='); if (!v) continue; *v++ = 0;
		if (!first) sadd(s, ","); first = 0; sjson(s, line); sadd(s, ":"); sjson(s, v);
	}
	if (p) pclose(p);
	sadd(s, "}");
}

/* ---------------- starting an app through its launcher, as the desktop user */
static int cmd_launch(const char *desk, const char *argfile)
{
	if (!id_ok(desk)) return 2;
	char file[1100] = "";
	if (argfile && *argfile) { FILE *f = fopen(argfile, "r"); if (f) { if (!fgets(file, sizeof file, f)) file[0] = 0; fclose(f); } unlink(argfile); file[strcspn(file, "\n")] = 0; }
	const char *ex = getenv("RA_EXPORTS") ? getenv("RA_EXPORTS") : EXPORTS;
	char dp[600]; snprintf(dp, sizeof dp, "%s/applications/%s.desktop", ex, desk);
	char *t = slurp(dp), *exec = t ? desktop_key(t, "Exec") : NULL;
	char *argv[64]; int n = 0;
	if (exec) {
		/* split on spaces, keeping quoted words; %f/%F/%u/%U become the file, other field codes go */
		char *p = exec;
		while (*p && n < 60) {
			while (*p == ' ') p++; if (!*p) break;
			str w = {0}; char q = 0;
			for (; *p && (q || *p != ' '); p++) {
				if (q) { if (*p == q) { q = 0; continue; } if (*p == '\\' && p[1]) { p++; } sput(&w, p, 1); }
				else if (*p == '"' || *p == '\'') q = *p; else sput(&w, p, 1);
			}
			char *a = w.b ? w.b : strdup("");
			if (!strcmp(a, "--file-forwarding") || !strcmp(a, "@@") || !strcmp(a, "@@u")) { free(a); continue; }
			if (!strcmp(a, "%f") || !strcmp(a, "%F") || !strcmp(a, "%u") || !strcmp(a, "%U")) { free(a); if (*file) argv[n++] = file; continue; }
			if (a[0] == '%' && a[1] && !a[2]) { free(a); continue; }
			argv[n++] = a;
		}
	}
	if (!n) {   /* no launcher: the app's default command */
		argv[n++] = "flatpak"; argv[n++] = "run"; argv[n++] = (char *)desk; if (*file) argv[n++] = file;
	}
	argv[n] = NULL;
	fprintf(stderr, "rudolf-apps: starting %s%s%s\n", desk, *file ? " with " : "", file);
	execvp(argv[0], argv); perror(argv[0]); return 127;
}

int main(int argc, char **argv)
{
	if (argc >= 2 && !strcmp(argv[1], "catalog")) return cmd_catalog(argc >= 3 ? argv[2] : NULL);
	if (argc >= 3 && !strcmp(argv[1], "info")) return cmd_info(argv[2], argc >= 4 && !strcmp(argv[3], "--remote"));
	if (argc >= 3 && !strcmp(argv[1], "launch")) return cmd_launch(argv[2], argc >= 4 ? argv[3] : NULL);
	if (argc >= 2 && !strcmp(argv[1], "installed")) return cmd_installed();
	fprintf(stderr, "usage: rudolf-apps catalog [OUT] | info ID | installed\n"); return 2;
}
