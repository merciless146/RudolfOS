/* rudolf-open (1.6.0): xdg-open, xdg-email, x-www-browser, sensible-browser and www-browser for Linux programs that
 * run beside Rudolf OS's desktop (Debian's, and AppImages run from it): a link or a file goes to the desktop, which opens
 * it as Standard does (http and https in Oracle, mailto: in Mail, a file in its app or Files, another app's own link
 * scheme in that app after asking), the way Flatpak apps already ask: through the account's session bus, to the Rudolf
 * portal's org.freedesktop.portal.OpenURI (rudolf-portal), which asks the desktop. Nothing else is reached: no system
 * API, no command is run here, the link is handed over as one string.
 *
 * Without it an Electron app's shell.openExternal (which runs xdg-open), GTK's gtk_show_uri and Qt's QDesktopServices
 * found no browser in Debian (no desktop that xdg-utils knows, no browser installed): "Continue with Google" in an app's
 * sign-in flashed and nothing opened (Claude's desktop app on the Vivobook, 2026-11-16).
 *
 * rudolf-debian binds this one program (static: Debian's C library is not Rudolf OS's) read-only over
 * /usr/local/bin/{xdg-open,xdg-email,x-www-browser,sensible-browser,www-browser}; what it does follows the name:
 *   xdg-open URL|FILE                       one link or file
 *   xdg-email [--cc A] [--bcc A] [--subject S] [--body B] [--attach F] [--utf8] ADDRESS...|MAILTO-URL
 *   x-www-browser / sensible-browser / www-browser / rudolf-open  URL|FILE...   each of them
 * Exit status as xdg-utils': 0 done, 1 bad arguments, 2 no such file, 3 the desktop cannot be reached, 4 refused.
 * When the desktop cannot be reached (no session bus: Debian's root, nobody signed in) xdg-open and xdg-email run
 * Debian's own (/usr/bin, or RUDOLF_OPEN_REAL) if it is installed.
 * Copyright (C) 2026 the Rudolf OS authors. SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define MAXURI 8192
static const char *ME = "rudolf-open";
static char ERRBUF[600];

/* ------------------------------------------------------------------ a growing buffer (a D-Bus message) */
typedef struct { unsigned char *b; size_t n, cap; } buf;
static void bput(buf *m, const void *p, size_t n)
{
	if (m->n + n > m->cap) { m->cap = (m->n + n) * 2 + 256; m->b = realloc(m->b, m->cap); if (!m->b) { perror(ME); exit(3); } }
	memcpy(m->b + m->n, p, n); m->n += n;
}
static void bpad(buf *m, size_t a) { static const unsigned char z[8]; while (m->n % a) bput(m, z, 1); }
static void bu8(buf *m, uint8_t v) { bput(m, &v, 1); }
static void bu32(buf *m, uint32_t v) { bpad(m, 4); unsigned char x[4] = {v, v >> 8, v >> 16, v >> 24}; bput(m, x, 4); }
static void bstr(buf *m, const char *s) { bu32(m, (uint32_t)strlen(s)); bput(m, s, strlen(s) + 1); }
static void bsig(buf *m, const char *s) { bu8(m, (uint8_t)strlen(s)); bput(m, s, strlen(s) + 1); }
/* a header field: (code, variant of type t) */
static void bfield(buf *m, uint8_t code, char t, const char *v)
{
	bpad(m, 8); bu8(m, code); char sg[2] = {t, 0}; bsig(m, sg);
	if (t == 'g') bsig(m, v); else bstr(m, v);
}

/* a method call; body already marshalled (offsets from 0, which is 8-aligned after the header) */
static buf call_msg(uint32_t serial, const char *dest, const char *path, const char *iface, const char *member, const char *sig, const buf *body)
{
	/* (NO_AUTO_START: the portal is Rudolf's, started by rudolfd; no other program is started by the bus for it) */
	buf h = {0}, f = {0};
	bfield(&f, 1, 'o', path); if (iface) bfield(&f, 2, 's', iface); bfield(&f, 3, 's', member); bfield(&f, 6, 's', dest);
	if (sig && *sig) bfield(&f, 8, 'g', sig);
	bu8(&h, 'l'); bu8(&h, 1); bu8(&h, 2); bu8(&h, 1); bu32(&h, body ? (uint32_t)body->n : 0); bu32(&h, serial);
	bu32(&h, (uint32_t)f.n); bput(&h, f.b, f.n); bpad(&h, 8);
	if (body && body->n) bput(&h, body->b, body->n);
	free(f.b); return h;
}

/* ------------------------------------------------------------------ the connection */
static int writeall(int fd, const void *p, size_t n)
{
	const char *c = p;
	while (n) { ssize_t w = send(fd, c, n, MSG_NOSIGNAL); if (w < 0 && errno == EINTR) continue; if (w <= 0) return -1; c += w; n -= (size_t)w; }
	return 0;
}
static int readsome(int fd, void *p, size_t n, int ms)
{
	struct pollfd pf = {fd, POLLIN, 0};
	for (;;) { int r = poll(&pf, 1, ms); if (r < 0 && errno == EINTR) continue; if (r <= 0) return -1; break; }
	ssize_t g = recv(fd, p, n, 0); return g > 0 ? (int)g : -1;
}
static int readall(int fd, void *p, size_t n, int ms)
{
	char *c = p;
	while (n) { int g = readsome(fd, c, n, ms); if (g < 0) return -1; c += g; n -= (size_t)g; }
	return 0;
}
/* one line of the authentication exchange */
static int authline(int fd, char *out, size_t n)
{
	size_t o = 0;
	while (o + 1 < n) { char c; if (readall(fd, &c, 1, 5000)) return -1; if (c == '\n') break; if (c != '\r') out[o++] = c; }
	out[o] = 0; return 0;
}
/* the session bus: DBUS_SESSION_BUS_ADDRESS (unix:path= or unix:abstract=), else $XDG_RUNTIME_DIR/bus */
static int bus_connect(void)
{
	const char *a = getenv("DBUS_SESSION_BUS_ADDRESS"); char path[sizeof(((struct sockaddr_un *)0)->sun_path)] = ""; int abstract = 0;
	if (a && *a) {
		/* the first unix: address of a list (a;b) */
		char t[1024]; snprintf(t, sizeof t, "%s", a);
		for (char *e = strtok(t, ";"); e && !*path; e = strtok(NULL, ";")) {
			if (strncmp(e, "unix:", 5)) continue;
			for (char *kv = strtok(e + 5, ","); kv; kv = strtok(NULL, ",")) {
				if (!strncmp(kv, "path=", 5)) { snprintf(path, sizeof path, "%s", kv + 5); abstract = 0; }
				else if (!strncmp(kv, "abstract=", 9)) { snprintf(path, sizeof path, "%s", kv + 9); abstract = 1; }
			}
		}
		/* (a %xx in the path: unescaped) */
		char *w = path; for (char *r = path; *r; r++) { unsigned v; if (*r == '%' && sscanf(r + 1, "%2x", &v) == 1) { *w++ = (char)v; r += 2; } else *w++ = *r; } *w = 0;
	}
	if (!*path) { const char *rt = getenv("XDG_RUNTIME_DIR"); if (rt && *rt) snprintf(path, sizeof path, "%s/bus", rt); }
	if (!*path) { snprintf(ERRBUF, sizeof ERRBUF, "no session bus (nobody signed in to Rudolf OS?)"); return -1; }
	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0); if (fd < 0) { snprintf(ERRBUF, sizeof ERRBUF, "socket: %s", strerror(errno)); return -1; }
	struct sockaddr_un sa = {0}; sa.sun_family = AF_UNIX; socklen_t l;
	if (abstract) { memcpy(sa.sun_path + 1, path, strnlen(path, sizeof sa.sun_path - 2)); l = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + 1 + strnlen(path, sizeof sa.sun_path - 2)); }
	else { snprintf(sa.sun_path, sizeof sa.sun_path, "%s", path); l = sizeof sa; }
	if (connect(fd, (struct sockaddr *)&sa, l)) { snprintf(ERRBUF, sizeof ERRBUF, "the session bus (%s): %s", path, strerror(errno)); close(fd); return -1; }
	/* EXTERNAL without an identity first: the bus takes the caller's own (inside Debian's user namespace the user is 1000,
	   which the bus outside knows by another number); then with this side's number, as older buses want */
	char line[256], hex[64] = ""; { char u[24]; snprintf(u, sizeof u, "%u", (unsigned)getuid()); for (char *c = u; *c; c++) snprintf(hex + strlen(hex), sizeof hex - strlen(hex), "%02x", (unsigned char)*c); }
	if (writeall(fd, "", 1) || writeall(fd, "AUTH EXTERNAL\r\n", 15) || authline(fd, line, sizeof line)) goto authfail;
	if (!strncmp(line, "DATA", 4)) { if (writeall(fd, "DATA\r\n", 6) || authline(fd, line, sizeof line)) goto authfail; }
	if (strncmp(line, "OK ", 3)) {
		char m[100]; snprintf(m, sizeof m, "AUTH EXTERNAL %s\r\n", hex);
		if (writeall(fd, m, strlen(m)) || authline(fd, line, sizeof line) || strncmp(line, "OK ", 3)) goto authfail;
	}
	if (writeall(fd, "BEGIN\r\n", 7)) goto authfail;
	return fd;
authfail:
	snprintf(ERRBUF, sizeof ERRBUF, "the session bus (%s) did not accept this program", path); close(fd); return -1;
}
/* the next message: its type, reply serial and error name (and the error's text) */
typedef struct { int type; uint32_t reply; char err[200], text[300]; } msg;
static uint32_t rd32(const unsigned char *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t rd32e(const unsigned char *p, int be) { return be ? ((uint32_t)p[3] | (uint32_t)p[2] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[0] << 24) : rd32(p); }
static int read_msg(int fd, msg *out, int ms)
{
	unsigned char fx[16]; if (readall(fd, fx, 16, ms)) return -1;
	int be = fx[0] == 'B'; if (!be && fx[0] != 'l') return -1;
	uint32_t blen = rd32e(fx + 4, be), flen = rd32e(fx + 12, be);
	if (blen > (1u << 26) || flen > (1u << 26)) return -1;
	size_t hl = 16 + flen; hl = (hl + 7) & ~(size_t)7;
	size_t tot = hl + blen; unsigned char *m = malloc(tot); if (!m) return -1;
	memcpy(m, fx, 16); if (readall(fd, m + 16, tot - 16, ms)) { free(m); return -1; }
	memset(out, 0, sizeof *out); out->type = fx[1];
	char sig[64] = "";
	for (size_t o = 16; o < 16 + flen; ) {
		o = (o + 7) & ~(size_t)7; if (o + 4 > 16 + flen) break;
		uint8_t code = m[o]; uint8_t sl = m[o + 1]; if (o + 2 + sl + 1 > 16 + flen) break;
		char t = sl ? (char)m[o + 2] : 0; o += 3 + sl;
		if (t == 'u') { o = (o + 3) & ~(size_t)3; if (o + 4 > tot) break; if (code == 5) out->reply = rd32e(m + o, be); o += 4; }
		else if (t == 's' || t == 'o') { o = (o + 3) & ~(size_t)3; if (o + 4 > tot) break; uint32_t l = rd32e(m + o, be); o += 4; if (o + l + 1 > tot) break;
			if (code == 4) snprintf(out->err, sizeof out->err, "%.*s", (int)l, (char *)m + o);
			o += l + 1; }
		else if (t == 'g') { if (o + 1 > tot) break; uint8_t l = m[o]; if (o + 1 + l + 1 > tot) break; if (code == 8) snprintf(sig, sizeof sig, "%.*s", (int)l, (char *)m + o + 1); o += 1 + l + 1; }
		else break;
	}
	if (out->type == 3 && sig[0] == 's' && blen >= 4) { uint32_t l = rd32e(m + hl, be); if (hl + 4 + l <= tot) snprintf(out->text, sizeof out->text, "%.*s", (int)l, (char *)m + hl + 4); }
	free(m); return 0;
}
/* call and wait for its answer: 0 done, -1 not reached, -2 refused (ERRBUF says why) */
static int call(int fd, uint32_t serial, const char *dest, const char *path, const char *iface, const char *member, const char *sig, const buf *body)
{
	buf c = call_msg(serial, dest, path, iface, member, sig, body);
	int w = writeall(fd, c.b, c.n); free(c.b);
	if (w) { snprintf(ERRBUF, sizeof ERRBUF, "the session bus closed"); return -1; }
	time_t end = time(NULL) + 25; msg m;
	while (time(NULL) < end) {
		if (read_msg(fd, &m, 25000)) { snprintf(ERRBUF, sizeof ERRBUF, "no answer from the session bus"); return -1; }
		if ((m.type == 2 || m.type == 3) && m.reply == serial) {
			if (m.type == 2) return 0;
			snprintf(ERRBUF, sizeof ERRBUF, "%s%s%s", m.err, *m.text ? ": " : "", m.text);
			/* no portal on the bus: as not reached (Debian's own may still open it) */
			return !strcmp(m.err, "org.freedesktop.DBus.Error.ServiceUnknown") || !strcmp(m.err, "org.freedesktop.DBus.Error.NameHasNoOwner") ? -1 : -2;
		}
	}
	snprintf(ERRBUF, sizeof ERRBUF, "no answer from the desktop"); return -1;
}
/* hand one URI to the desktop: OpenURI("", uri, {}) */
static int open_uri(const char *uri)
{
	int fd = bus_connect(); if (fd < 0) return -1;
	int r = call(fd, 1, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "Hello", NULL, NULL);
	if (!r) {
		buf b = {0}; bstr(&b, ""); bstr(&b, uri); bu32(&b, 0); bpad(&b, 8);   /* (an empty a{sv}: its length, then its elements' alignment) */
		r = call(fd, 2, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.OpenURI", "OpenURI", "ssa{sv}", &b);
		free(b.b);
	}
	close(fd); return r;
}

/* ------------------------------------------------------------------ what is opened */
static int scheme_len(const char *s)
{
	if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z'))) return 0;
	int i = 1; while ((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= '0' && s[i] <= '9') || s[i] == '+' || s[i] == '-' || s[i] == '.') i++;
	return s[i] == ':' && i >= 2 ? i : 0;   /* (one letter: a drive, C:\..., not a scheme) */
}
static int clean(const char *s) { for (const unsigned char *c = (const unsigned char *)s; *c; c++) if (*c < 0x20 || *c == 0x7f) return 0; return 1; }
/* a link as it is, a file or folder as file:// with its absolute path */
static int to_uri(const char *a, char *out, size_t n)
{
	if (!*a || strlen(a) >= MAXURI || !clean(a)) return 1;
	if (scheme_len(a)) { snprintf(out, n, "%s", a); return 0; }
	char rp[PATH_MAX]; if (!realpath(a, rp)) return 2;
	size_t o = snprintf(out, n, "file://");
	for (const unsigned char *c = (const unsigned char *)rp; *c && o + 4 < n; c++) {
		if ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || strchr("/-._~!$&'()*+,;=:@", *c)) out[o++] = (char)*c;
		else o += snprintf(out + o, n - o, "%%%02X", *c);
	}
	out[o] = 0; return 0;
}
/* xdg-email's options as a mailto: link */
static void pct(char *out, size_t n, size_t *o, const char *s)
{
	for (const unsigned char *c = (const unsigned char *)s; *c && *o + 4 < n; c++) {
		if ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || strchr("-._~@", *c)) out[(*o)++] = (char)*c;
		else *o += snprintf(out + *o, n - *o, "%%%02X", *c);
	}
	out[*o] = 0;
}
static int email_uri(int argc, char **argv, char *out, size_t n)
{
	const char *cc[16], *bcc[16], *subj = NULL, *body = NULL, *to[32]; int ncc = 0, nbcc = 0, nto = 0; const char *mailto = NULL;
	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (!strcmp(a, "--utf8")) continue;
		if ((!strcmp(a, "--cc") || !strcmp(a, "--bcc") || !strcmp(a, "--subject") || !strcmp(a, "--body") || !strcmp(a, "--attach")) && i + 1 < argc) {
			const char *v = argv[++i];
			if (!strcmp(a, "--cc")) { if (ncc < 16) cc[ncc++] = v; } else if (!strcmp(a, "--bcc")) { if (nbcc < 16) bcc[nbcc++] = v; }
			else if (!strcmp(a, "--subject")) subj = v; else if (!strcmp(a, "--body")) body = v;   /* (--attach: Mail is asked without the file) */
			continue;
		}
		if (a[0] == '-') return 1;
		if (!strncasecmp(a, "mailto:", 7)) mailto = a; else if (nto < 32) to[nto++] = a;
	}
	if (mailto) { if (strlen(mailto) >= n || !clean(mailto)) return 1; snprintf(out, n, "%s", mailto); return 0; }
	size_t o = snprintf(out, n, "mailto:");
	for (int i = 0; i < nto; i++) { if (i) o += snprintf(out + o, n - o, ","); pct(out, n, &o, to[i]); }
	char sep = '?';
	for (int i = 0; i < ncc; i++) { o += snprintf(out + o, n - o, "%ccc=", sep); sep = '&'; pct(out, n, &o, cc[i]); }
	for (int i = 0; i < nbcc; i++) { o += snprintf(out + o, n - o, "%cbcc=", sep); sep = '&'; pct(out, n, &o, bcc[i]); }
	if (subj) { o += snprintf(out + o, n - o, "%csubject=", sep); sep = '&'; pct(out, n, &o, subj); }
	if (body) { o += snprintf(out + o, n - o, "%cbody=", sep); sep = '&'; pct(out, n, &o, body); }
	return o >= n - 1 ? 1 : 0;
}
/* Debian's own program of that name, when the desktop cannot be reached */
static void fallback(const char *name, char **argv)
{
	const char *d = getenv("RUDOLF_OPEN_REAL"); char p[PATH_MAX]; snprintf(p, sizeof p, "%s/%s", d && *d ? d : "/usr/bin", name);
	char self[PATH_MAX], other[PATH_MAX];
	if (access(p, X_OK) || !realpath(p, other) || (realpath("/proc/self/exe", self) && !strcmp(self, other))) return;
	struct stat a, b; if (!stat("/proc/self/exe", &a) && !stat(p, &b) && a.st_dev == b.st_dev && a.st_ino == b.st_ino) return;
	argv[0] = p; execv(p, argv);
}
static void usage(const char *name)
{
	if (!strcmp(name, "xdg-email")) fprintf(stderr, "Usage: xdg-email [--utf8] [--cc address] [--bcc address] [--subject text] [--body text] [--attach file] { mailto-uri | address(es) }\n");
	else if (!strcmp(name, "xdg-open")) fprintf(stderr, "Usage: xdg-open { file | URL }\n");
	else fprintf(stderr, "Usage: %s URL...\n", name);
	fprintf(stderr, "(Rudolf OS: the link or file opens on Rudolf OS's desktop: web pages in Oracle, mail in Mail, files in their app.)\n");
}
int main(int argc, char **argv)
{
	const char *name = strrchr(argv[0], '/'); name = name ? name + 1 : argv[0];
	ME = name;
	if (argc >= 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "--manual") || !strcmp(argv[1], "-h"))) { usage(name); return 0; }
	if (argc >= 2 && !strcmp(argv[1], "--version")) { printf("%s 1.2.1 (Rudolf OS: rudolf-open 1.6.0)\n", name); return 0; }
	static char uri[MAXURI + 16];
	if (!strcmp(name, "xdg-email")) {
		if (argc < 2 || email_uri(argc, argv, uri, sizeof uri)) { usage(name); return 1; }
		int r = open_uri(uri);
		if (r == -1) fallback(name, argv);
		if (r) { fprintf(stderr, "%s: %s\n", name, ERRBUF); return r == -2 ? 4 : 3; }
		return 0;
	}
	int single = !strcmp(name, "xdg-open");
	if (argc < 2 || (single && argc != 2)) { usage(name); return 1; }
	int rc = 0;
	for (int i = 1; i < argc; i++) {
		int t = to_uri(argv[i], uri, sizeof uri);
		if (t == 1) { fprintf(stderr, "%s: not a link or file name: %.200s\n", name, argv[i]); rc = rc ? rc : 1; continue; }
		if (t == 2) { fprintf(stderr, "%s: file '%.200s' does not exist\n", name, argv[i]); rc = rc ? rc : 2; continue; }
		int r = open_uri(uri);
		if (r == -1 && single) fallback(name, argv);
		if (r) { fprintf(stderr, "%s: %s\n", name, ERRBUF); rc = rc ? rc : (r == -2 ? 4 : 3); }
	}
	return rc;
}
