/* portalstand: a stand-in for a Linux app that asks the desktop for things, as Sober (the Roblox client) and other Flatpak
 * apps do, for Rudolf OS's tests (iso/qt/sb/p160_t.py, iso/qt/host/portal60_t.sh). Static and without a toolkit or
 * libdbus (the D-Bus protocol by hand), so it runs in a stand-in Flatpak runtime, in Debian's stand-in, or anywhere.
 *   portalstand keyring LABEL SECRET   the Secret Service (gnome-keyring-daemon): a new keyring (CreateCollection, whose
 *                                      prompt asks for its password), then SECRET kept in it (CreateItem): as libsecret
 *                                      does for an app that keeps its sign-in in a keyring of its own
 *   portalstand open                   the portal's FileChooser.OpenFile (a file dialog): the files chosen
 *   portalstand link URL               the portal's OpenURI.OpenURI (a link for the desktop to open)
 *   portalstand app                    as an app started from its launcher: "started PID" in its log, then each line of
 *                                      DIR/portalstand.cmd (one of the three above, as words) done in turn, then it waits
 *                                      (a window-less app left running, until it is ended)
 * Its log: DIR/portalstand.log, DIR $PORTALSTAND_DIR, else $XDG_DATA_HOME (a Flatpak app's own folder), else $HOME.
 * Each result is one line: "keyring created COLLECTION item ITEM", "keyring dismissed", "open response N URIS",
 * "link response N", or "... error WHAT". GPL-2.0-or-later. */
#define _GNU_SOURCE
#include <errno.h>
#include <poll.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stddef.h>
#include <time.h>
#include <unistd.h>

static char LOGF[700];
static void say(const char *fmt, ...)
{
	char m[2000]; va_list a; va_start(a, fmt); vsnprintf(m, sizeof m, fmt, a); va_end(a);
	struct timespec t; clock_gettime(CLOCK_REALTIME, &t);
	FILE *o = fopen(LOGF, "a"); if (o) { fprintf(o, "%ld.%03ld %s\n", (long)t.tv_sec, t.tv_nsec / 1000000, m); fclose(o); }
	fprintf(stderr, "portalstand: %s\n", m);
}

/* ------------------------------------------------------------------ writing a message (little-endian) */
typedef struct { unsigned char *b; size_t n, cap; } buf;
static void bput(buf *m, const void *p, size_t n)
{
	if (m->n + n > m->cap) { m->cap = (m->n + n) * 2 + 256; m->b = realloc(m->b, m->cap); if (!m->b) exit(3); }
	memcpy(m->b + m->n, p, n); m->n += n;
}
static void bpad(buf *m, size_t a) { static const unsigned char z[8]; while (m->n % a) bput(m, z, 1); }
static void bu8(buf *m, uint8_t v) { bput(m, &v, 1); }
static void bu32(buf *m, uint32_t v) { bpad(m, 4); unsigned char x[4] = {v, v >> 8, v >> 16, v >> 24}; bput(m, x, 4); }
static void bstr(buf *m, const char *s) { bu32(m, (uint32_t)strlen(s)); bput(m, s, strlen(s) + 1); }
static void bsig(buf *m, const char *s) { bu8(m, (uint8_t)strlen(s)); bput(m, s, strlen(s) + 1); }
/* an array: its length is filled in by aend; elements start aligned to al */
static size_t abeg(buf *m, size_t al) { bu32(m, 0); size_t at = m->n - 4; bpad(m, al); return at; }
static void aend(buf *m, size_t at, size_t al)
{
	size_t start = at + 4; while (start % al) start++;
	uint32_t n = (uint32_t)(m->n - start); unsigned char x[4] = {n, n >> 8, n >> 16, n >> 24}; memcpy(m->b + at, x, 4);
}
/* {sv} entries: a string variant, an a{ss} variant */
static void dict_s(buf *m, const char *k, const char *v) { bpad(m, 8); bstr(m, k); bsig(m, "s"); bstr(m, v); }
static void dict_ass(buf *m, const char *k, const char *k1, const char *v1)
{
	bpad(m, 8); bstr(m, k); bsig(m, "a{ss}"); size_t a = abeg(m, 8); bpad(m, 8); bstr(m, k1); bstr(m, v1); aend(m, a, 8);
}
static void bfield(buf *m, uint8_t code, char t, const char *v)
{
	bpad(m, 8); bu8(m, code); char sg[2] = {t, 0}; bsig(m, sg);
	if (t == 'g') bsig(m, v); else bstr(m, v);
}
static uint32_t SERIAL;
static buf call_msg(const char *dest, const char *path, const char *iface, const char *member, const char *sig, const buf *body)
{
	buf h = {0}, f = {0};
	bfield(&f, 1, 'o', path); if (iface) bfield(&f, 2, 's', iface); bfield(&f, 3, 's', member); if (dest) bfield(&f, 6, 's', dest);
	if (sig && *sig) bfield(&f, 8, 'g', sig);
	bu8(&h, 'l'); bu8(&h, 1); bu8(&h, 0); bu8(&h, 1); bu32(&h, body ? (uint32_t)body->n : 0); bu32(&h, ++SERIAL);
	bu32(&h, (uint32_t)f.n); bput(&h, f.b, f.n); bpad(&h, 8);
	if (body && body->n) bput(&h, body->b, body->n);
	free(f.b); return h;
}

/* ------------------------------------------------------------------ the connection */
static int FD = -1, DEAD;
static int writeall(const void *p, size_t n)
{
	const char *c = p;
	while (n) { ssize_t w = send(FD, c, n, MSG_NOSIGNAL); if (w < 0 && errno == EINTR) continue; if (w <= 0) return -1; c += w; n -= (size_t)w; }
	return 0;
}
static int readn(void *p, size_t n, int ms)
{
	char *c = p;
	while (n) {
		struct pollfd pf = {FD, POLLIN, 0}; int r = poll(&pf, 1, ms); if (r < 0 && errno == EINTR) continue; if (r <= 0) return -1;
		ssize_t g = recv(FD, c, n, 0); if (g < 0 && errno == EINTR) continue; if (g <= 0) { DEAD = 1; return -1; } c += g; n -= (size_t)g;
	}
	return 0;
}
static int bus_open(void)
{
	const char *a = getenv("DBUS_SESSION_BUS_ADDRESS"); char path[300] = ""; int abstract = 0;
	if (!a) { say("error no session bus"); return -1; }
	const char *p = strstr(a, "unix:path="); if (p) { p += 10; size_t n = strcspn(p, ",;"); snprintf(path, sizeof path, "%.*s", (int)n, p); }
	else if ((p = strstr(a, "unix:abstract="))) { p += 14; size_t n = strcspn(p, ",;"); snprintf(path, sizeof path, "%.*s", (int)n, p); abstract = 1; }
	if (!path[0]) { say("error bus address %s", a); return -1; }
	FD = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	struct sockaddr_un u = {.sun_family = AF_UNIX}; socklen_t ul;
	if (abstract) { u.sun_path[0] = 0; memcpy(u.sun_path + 1, path, strlen(path)); ul = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + 1 + strlen(path)); }
	else { snprintf(u.sun_path, sizeof u.sun_path, "%s", path); ul = sizeof u; }
	if (connect(FD, (struct sockaddr *)&u, ul)) { say("error connect %s: %s", path, strerror(errno)); return -1; }
	char uid[16], hex[40] = "", line[300]; snprintf(uid, sizeof uid, "%u", (unsigned)getuid());
	for (char *c = uid; *c; c++) sprintf(hex + strlen(hex), "%02x", (unsigned char)*c);
	char au[100]; int an = snprintf(au, sizeof au, "AUTH EXTERNAL %s\r\n", hex);
	if (writeall("", 1) || writeall(au, (size_t)an)) return -1;
	size_t k = 0; while (k < sizeof line - 1) { if (readn(line + k, 1, 5000)) return -1; if (line[k++] == '\n') break; } line[k] = 0;
	if (strncmp(line, "OK ", 3)) { say("error auth: %s", line); return -1; }
	if (writeall("BEGIN\r\n", 7)) return -1;
	return 0;
}

/* ------------------------------------------------------------------ reading a message: its header, its body as text */
typedef struct { int type; uint32_t serial, reply; char member[100], iface[100], path[300], sig[100], err[200]; unsigned char *body; uint32_t blen; } msg;
static uint32_t rd32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static int msg_read(msg *m, int ms)
{
	memset(m, 0, sizeof *m); unsigned char h[16];
	if (readn(h, 16, ms)) return -1;
	if (h[0] != 'l') { say("error big-endian message"); return -1; }
	m->type = h[1]; m->blen = rd32(h + 4); m->serial = rd32(h + 8); uint32_t fl = rd32(h + 12);
	size_t fpad = (fl + 7) & ~7u; unsigned char *f = malloc(fpad + 1); if (!f || readn(f, fpad, ms)) return -1;
	for (size_t o = 0; o < fl;) {
		o = (o + 7) & ~7u; if (o >= fl) break;
		uint8_t code = f[o]; uint8_t sl = f[o + 1]; char t = (char)f[o + 2]; o += 3 + sl;   /* (the variant's signature: one type) */
		if (t == 's' || t == 'o') { o = (o + 3) & ~3u; uint32_t n = rd32(f + o); o += 4; const char *s = (char *)f + o;
			char *dst = code == 1 ? m->path : code == 2 ? m->iface : code == 3 ? m->member : code == 4 ? m->err : NULL;
			size_t cap = code == 1 ? sizeof m->path : code == 4 ? sizeof m->err : 100;
			if (dst) snprintf(dst, cap, "%.*s", (int)n, s);
			o += n + 1; }
		else if (t == 'u') { o = (o + 3) & ~3u; if (code == 5) m->reply = rd32(f + o); o += 4; }
		else if (t == 'g') { uint8_t n = f[o]; if (code == 8) snprintf(m->sig, sizeof m->sig, "%.*s", n, (char *)f + o + 1); o += 1 + n + 1; }
		else break;
	}
	free(f);
	m->body = malloc(m->blen + 1); if (!m->body || (m->blen && readn(m->body, m->blen, ms))) return -1;
	return 0;
}
/* one value of the signature at *s from the body at *o, written as text into t (strings quoted with '') */
static void val(const msg *m, const char **s, size_t *o, char *t, size_t tn);
static size_t align_of(char c) { switch (c) { case 'y': case 'g': case 'v': return 1; case 'n': case 'q': return 2; case 'x': case 't': case 'd': case '(': case '{': return 8; default: return 4; } }
static const char *skip1(const char *s)
{
	if (*s == 'a') return skip1(s + 1);
	if (*s == '(' || *s == '{') { char e = *s == '(' ? ')' : '}'; s++; while (*s && *s != e) s = skip1(s); return *s ? s + 1 : s; }
	return s + 1;
}
static void cat(char *t, size_t tn, const char *x) { size_t l = strlen(t); if (l + 1 < tn) snprintf(t + l, tn - l, "%s", x); }
static void val(const msg *m, const char **s, size_t *o, char *t, size_t tn)
{
	char c = **s; size_t al = align_of(c); *o = (*o + al - 1) / al * al; char x[64];
	if (*o > m->blen) { (*s)++; return; }
	switch (c) {
	case 'y': snprintf(x, sizeof x, "%u", m->body[*o]); *o += 1; cat(t, tn, x); (*s)++; return;
	case 'b': case 'u': case 'i': snprintf(x, sizeof x, c == 'b' ? (rd32(m->body + *o) ? "true" : "false") : "%u", rd32(m->body + *o)); *o += 4; cat(t, tn, x); (*s)++; return;
	case 'n': case 'q': snprintf(x, sizeof x, "%u", m->body[*o] | m->body[*o + 1] << 8); *o += 2; cat(t, tn, x); (*s)++; return;
	case 'x': case 't': case 'd': snprintf(x, sizeof x, "%u", rd32(m->body + *o)); *o += 8; cat(t, tn, x); (*s)++; return;
	case 's': case 'o': { uint32_t n = rd32(m->body + *o); *o += 4; cat(t, tn, "'"); char *v = strndup((char *)m->body + *o, n); cat(t, tn, v); free(v); cat(t, tn, "'"); *o += n + 1; (*s)++; return; }
	case 'g': { uint8_t n = m->body[*o]; char *v = strndup((char *)m->body + *o + 1, n); cat(t, tn, v); free(v); *o += 2u + n; (*s)++; return; }
	case 'v': { uint8_t n = m->body[*o]; char sg[64]; snprintf(sg, sizeof sg, "%.*s", n, (char *)m->body + *o + 1); *o += 2u + n; const char *ss = sg; cat(t, tn, "<"); val(m, &ss, o, t, tn); cat(t, tn, ">"); (*s)++; return; }
	case 'a': { uint32_t n = rd32(m->body + *o); *o += 4; const char *el = *s + 1, *after = skip1(el); size_t eal = align_of(*el); *o = (*o + eal - 1) / eal * eal; size_t end = *o + n;
		cat(t, tn, "["); int first = 1; while (*o < end && *o < m->blen) { if (!first) cat(t, tn, ", "); first = 0; const char *ee = el; val(m, &ee, o, t, tn); } cat(t, tn, "]"); *s = after; return; }
	case '(': case '{': { char e = c == '(' ? ')' : '}'; (*s)++; cat(t, tn, c == '(' ? "(" : "{"); int first = 1;
		while (**s && **s != e) { if (!first) cat(t, tn, c == '{' ? ": " : ", "); first = 0; val(m, s, o, t, tn); }
		if (**s) (*s)++;
		cat(t, tn, c == '(' ? ")" : "}"); return; }
	default: (*s)++; return;
	}
}
static void body_text(const msg *m, char *t, size_t tn)
{
	t[0] = 0; const char *s = m->sig; size_t o = 0; int first = 1;
	while (*s) { if (!first) cat(t, tn, ", "); first = 0; val(m, &s, &o, t, tn); }
}
/* the n-th quoted string of a text ('' marks), or "" */
static void nth_str(const char *t, int n, char *out, size_t on)
{
	out[0] = 0; const char *p = t;
	for (int i = 0; ; i++) { const char *a = strchr(p, '\''); if (!a) return; const char *b = strchr(a + 1, '\''); if (!b) return;
		if (i == n) { snprintf(out, on, "%.*s", (int)(b - a - 1), a + 1); return; } p = b + 1; }
}

/* a call and its answer (signals and other messages meanwhile are kept for wait_signal); 0 with the answer's text */
#define KEEP 16
static msg KEPT[KEEP]; static int NKEPT;
static int call(const char *dest, const char *path, const char *iface, const char *member, const char *sig, const buf *body, char *t, size_t tn, int ms)
{
	buf h = call_msg(dest, path, iface, member, sig, body); uint32_t me = SERIAL;
	int w = writeall(h.b, h.n); free(h.b); if (w) { snprintf(t, tn, "write failed"); return -1; }
	for (;;) {
		msg m; if (msg_read(&m, ms)) { snprintf(t, tn, "no answer to %s", member); return -1; }
		if ((m.type == 2 || m.type == 3) && m.reply == me) {
			if (m.type == 3) { char b[600]; body_text(&m, b, sizeof b); snprintf(t, tn, "%s: %s", m.err, b); free(m.body); return -1; }
			body_text(&m, t, tn); free(m.body); return 0;
		}
		if (m.type == 4 && NKEPT < KEEP) KEPT[NKEPT++] = m; else free(m.body);
	}
}
/* a signal MEMBER from the object PATH: its text */
static int wait_signal(const char *path, const char *member, char *t, size_t tn, int secs)
{
	for (int i = 0; i < NKEPT; i++) if (!strcmp(KEPT[i].path, path) && !strcmp(KEPT[i].member, member)) {
		body_text(&KEPT[i], t, tn); free(KEPT[i].body); KEPT[i] = KEPT[--NKEPT]; return 0; }
	time_t end = time(NULL) + secs;
	while (time(NULL) < end) {
		msg m; if (msg_read(&m, 1000)) { if (DEAD) return -1; continue; }
		if (m.type == 4 && !strcmp(m.path, path) && !strcmp(m.member, member)) { body_text(&m, t, tn); free(m.body); return 0; }
		free(m.body);
	}
	return -1;
}
static int add_match(const char *rule)
{
	buf b = {0}; bstr(&b, rule); char t[300]; int r = call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "AddMatch", "s", &b, t, sizeof t, 5000); free(b.b); return r;
}
static char UNIQUE[100];
static int hello(void)
{
	char t[300]; if (call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "Hello", "", NULL, t, sizeof t, 5000)) { say("error hello: %s", t); return -1; }
	nth_str(t, 0, UNIQUE, sizeof UNIQUE); return 0;
}

/* ------------------------------------------------------------------ what it asks */
#define SS "org.freedesktop.secrets"
static int do_keyring(const char *label, const char *secret)
{
	char t[4000], coll[300] = "", prompt[300] = "";
	if (add_match("type='signal',interface='org.freedesktop.Secret.Prompt',member='Completed'")) { say("keyring error match"); return 1; }
	buf b = {0}; size_t a = abeg(&b, 8); dict_s(&b, "org.freedesktop.Secret.Collection.Label", label); aend(&b, a, 8); bstr(&b, "");
	int r = call(SS, "/org/freedesktop/secrets", "org.freedesktop.Secret.Service", "CreateCollection", "a{sv}s", &b, t, sizeof t, 30000); free(b.b);
	if (r) { say("keyring error CreateCollection %s", t); return 1; }
	nth_str(t, 0, coll, sizeof coll); nth_str(t, 1, prompt, sizeof prompt);
	say("keyring asked CreateCollection: %s", t);
	if (strcmp(prompt, "/")) {
		b = (buf){0}; bstr(&b, "");
		r = call(SS, prompt, "org.freedesktop.Secret.Prompt", "Prompt", "s", &b, t, sizeof t, 30000); free(b.b);
		if (r) { say("keyring error Prompt %s", t); return 1; }
		say("keyring prompting %s", prompt);
		if (wait_signal(prompt, "Completed", t, sizeof t, 600)) { say("keyring error no answer to the prompt"); return 1; }
		say("keyring completed %s", t);
		if (!strncmp(t, "true", 4)) { say("keyring dismissed"); return 2; }
		nth_str(t, 0, coll, sizeof coll);
	}
	if (!coll[0] || !strcmp(coll, "/")) { say("keyring error no collection"); return 1; }
	/* the secret, kept in it (a plain session: the bus is the account's own) */
	b = (buf){0}; bstr(&b, "plain"); bsig(&b, "s"); bstr(&b, "");
	r = call(SS, "/org/freedesktop/secrets", "org.freedesktop.Secret.Service", "OpenSession", "sv", &b, t, sizeof t, 30000); free(b.b);
	if (r) { say("keyring error OpenSession %s", t); return 1; }
	char sess[300]; nth_str(t, 1, sess, sizeof sess);   /* (<''>, the session's path) */
	b = (buf){0}; a = abeg(&b, 8); dict_s(&b, "org.freedesktop.Secret.Item.Label", "PortalStand sign-in"); dict_ass(&b, "org.freedesktop.Secret.Item.Attributes", "app", "portalstand"); aend(&b, a, 8);
	bpad(&b, 8); bstr(&b, sess); size_t p = abeg(&b, 1); aend(&b, p, 1); p = abeg(&b, 1); bput(&b, secret, strlen(secret)); aend(&b, p, 1); bstr(&b, "text/plain");
	bu32(&b, 1);
	char cpath[300]; snprintf(cpath, sizeof cpath, "%s", coll);
	r = call(SS, cpath, "org.freedesktop.Secret.Collection", "CreateItem", "a{sv}(oayays)b", &b, t, sizeof t, 30000); free(b.b);
	if (r) { say("keyring error CreateItem %s", t); return 1; }
	char item[300]; nth_str(t, 0, item, sizeof item);
	say("keyring created %s item %s", coll, item);
	return 0;
}
static const char *SAVENAME;   /* (save NAME: FileChooser.SaveFile with current_name NAME) */
static int portal_req(const char *what, const char *iface, const char *member, const char *arg)
{
	char t[4000], req[300], tok[40];
	snprintf(tok, sizeof tok, "ps%ld", (long)getpid() % 100000 + (long)time(NULL) % 1000);
	/* (the request's path is known before the call, so its Response is never missed) */
	char u[100]; snprintf(u, sizeof u, "%s", UNIQUE + 1); for (char *c = u; *c; c++) if (*c == '.') *c = '_';
	snprintf(req, sizeof req, "/org/freedesktop/portal/desktop/request/%s/%s", u, tok);
	char rule[500]; snprintf(rule, sizeof rule, "type='signal',interface='org.freedesktop.portal.Request',member='Response',path='%s'", req);
	if (add_match(rule)) { say("%s error match", what); return 1; }
	buf b = {0}; bstr(&b, ""); bstr(&b, arg); size_t a = abeg(&b, 8); dict_s(&b, "handle_token", tok); if (SAVENAME) dict_s(&b, "current_name", SAVENAME); aend(&b, a, 8);
	int r = call("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", iface, member, "ssa{sv}", &b, t, sizeof t, 30000); free(b.b);
	if (r) { say("%s error %s %s", what, member, t); return 1; }
	char got[300]; nth_str(t, 0, got, sizeof got);
	say("%s asked %s: %s", what, member, got);
	if (wait_signal(got[0] ? got : req, "Response", t, sizeof t, 600)) { say("%s error no response", what); return 1; }
	say("%s response %s", what, t);
	return 0;
}
static int do_one(int argc, char **argv)
{
	if (argc >= 3 && !strcmp(argv[0], "keyring")) return do_keyring(argv[1], argv[2]);
	if (argc >= 1 && !strcmp(argv[0], "open")) return portal_req("open", "org.freedesktop.portal.FileChooser", "OpenFile", "PortalStand: open a file");
	if (argc >= 2 && !strcmp(argv[0], "save")) { SAVENAME = argv[1]; return portal_req("save", "org.freedesktop.portal.FileChooser", "SaveFile", "PortalStand: save a file"); }
	if (argc >= 2 && !strcmp(argv[0], "link")) return portal_req("link", "org.freedesktop.portal.OpenURI", "OpenURI", argv[1]);
	say("error what? %s", argc ? argv[0] : "");
	return 1;
}

int main(int argc, char **argv)
{
	const char *d = getenv("PORTALSTAND_DIR"); if (!d || !*d) d = getenv("XDG_DATA_HOME"); if (!d || !*d) d = getenv("HOME"); if (!d || !*d) d = "/tmp";
	snprintf(LOGF, sizeof LOGF, "%s/portalstand.log", d);
	if (argc < 2) { fprintf(stderr, "usage: portalstand keyring LABEL SECRET | open | save NAME | link URL | app\n"); return 1; }
	setvbuf(stderr, NULL, _IONBF, 0);
	int app = !strcmp(argv[1], "app");
	if (app) say("started %ld", (long)getpid());
	if (bus_open() || hello()) { say("error the session bus"); if (!app) return 3; }
	if (!app) return do_one(argc - 1, argv + 1);
	char cf[700]; snprintf(cf, sizeof cf, "%s/portalstand.cmd", d);
	FILE *c = fopen(cf, "r"); char line[2000];
	while (c && FD >= 0 && fgets(line, sizeof line, c)) {
		line[strcspn(line, "\r\n")] = 0; if (!line[0]) continue;
		char *w[8]; int n = 0; for (char *s = strtok(line, " "); s && n < 8; s = strtok(NULL, " ")) w[n++] = s;
		say("doing %s", w[0]); do_one(n, w);
	}
	if (c) fclose(c);
	say("waiting");
	for (;;) pause();
}
