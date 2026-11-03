/*
 * rudolf-remote - remote access for Rudolf OS 1.5.0 (docs/plan-1.5.0.md, part 5): the desktop in any web browser.
 *
 * HTTPS on port 8443 with the computer's own certificate (EC P-256, made on first use in /persist/etc/remote/).
 * The viewer page (/usr/share/rudolf/remote/) signs in, then opens a WebSocket that carries the screen one way and
 * keyboard and mouse the other way:
 *   server -> viewer, binary: 'S' w:u16 h:u16                      the screen's size (then every tile follows)
 *                             'T' n:u16, n x (x y w h:u16, len:u32, zlib(BGRX))   changed 64x64 tiles
 *   server -> viewer, text:   {"t":"wait"|"go"|"bye","m":...}       waiting for approval, started, ended
 *   viewer -> server, text:   "a" (frame drawn), "m X Y", "b BUTTON 1|0", "k KEYSYM 1|0"
 * Signing in (POST /login, form fields code, user, pass):
 *   - the pairing code (6 digits, /run/rudolf/remote/code, new each time remote access is turned on), once per browser:
 *     the browser then keeps a device cookie (its SHA-256 in /persist/etc/remote-devices; turned off, they are deleted);
 *   - a Rudolf OS account and its password, checked against /persist/etc/remote-accounts (the desktop writes it:
 *     name, PBKDF2 hash "p2$salt$hash" as on the sign-in screen, role, unattended 0|1);
 *   - five wrong tries lock signing in for a minute.
 * Approval: unless the account may connect unattended, the person at the computer is asked (rudolfd /api/remote shows
 * /run/rudolf/remote/pending to the desktop, which writes answer-ID). One session at a time; the desktop can end it
 * (/run/rudolf/remote/end). Sessions go into the health log (rudolfd --health remote ...).
 *
 * Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XShm.h>
/* XTest prototypes (as rudolf-display: the header is not needed at run time) */
extern int XTestFakeKeyEvent(Display *, unsigned int, Bool, unsigned long);
extern int XTestFakeButtonEvent(Display *, unsigned int, Bool, unsigned long);
extern int XTestFakeMotionEvent(Display *, int, int, int, unsigned long);
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <zlib.h>

#define RDIR   "/run/rudolf/remote"
#define EDIR   "/persist/etc/remote"
#define ACCTF  "/persist/etc/remote-accounts"
#define DEVF   "/persist/etc/remote-devices"
#define WWWR   "/usr/share/rudolf/remote"
#define PORT   8443
#define TILE   64

static SSL *S;
static char peer[64];
static void say(const char *kind, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void say(const char *kind, const char *fmt, ...)
{
	char m[300]; va_list ap; va_start(ap, fmt); vsnprintf(m, sizeof m, fmt, ap); va_end(ap);
	pid_t p = fork(); if (p == 0) { execl("/usr/bin/rudolfd", "rudolfd", "--health", kind, m, (char *)NULL); _exit(1); }
	if (p > 0) waitpid(p, NULL, 0);
}
static char *slurp(const char *p, size_t *n)
{
	FILE *f = fopen(p, "rb"); if (!f) return NULL;
	char *b = NULL; size_t c = 0, l = 0; for (;;) { if (l + 4097 > c) { c = c ? c * 2 : 8192; b = realloc(b, c); } size_t r = fread(b + l, 1, 4096, f); l += r; if (r < 4096) break; }
	fclose(f); b[l] = 0; if (n) *n = l; return b;
}
static void wfile(const char *p, const char *s, mode_t m) { char t[300]; snprintf(t, sizeof t, "%s.tmp", p); int f = open(t, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, m); if (f < 0) return; if (write(f, s, strlen(s)) < 0) {} close(f); rename(t, p); }
static void hex(const unsigned char *d, size_t n, char *o) { for (size_t i = 0; i < n; i++) sprintf(o + 2 * i, "%02x", d[i]); }
static void sha256hex(const char *s, char o[65]) { unsigned char h[32]; SHA256((const unsigned char *)s, strlen(s), h); hex(h, 32, o); }
static void randhex(char *o, int bytes) { unsigned char b[64]; RAND_bytes(b, bytes); hex(b, bytes, o); }

/* ---------------------------------------------------------------- the certificate */
static int make_cert(void)
{
	mkdir("/persist/etc", 0755); mkdir(EDIR, 0700);
	EVP_PKEY *k = EVP_EC_gen("P-256"); if (!k) return -1;
	X509 *x = X509_new(); X509_set_version(x, 2);
	unsigned char sn[16]; RAND_bytes(sn, sizeof sn); sn[0] &= 0x7f; BIGNUM *bn = BN_bin2bn(sn, sizeof sn, NULL); BN_to_ASN1_INTEGER(bn, X509_get_serialNumber(x)); BN_free(bn);
	X509_gmtime_adj(X509_getm_notBefore(x), -86400); X509_gmtime_adj(X509_getm_notAfter(x), 10L * 365 * 86400);
	X509_set_pubkey(x, k);
	X509_NAME *nm = X509_get_subject_name(x); X509_NAME_add_entry_by_txt(nm, "CN", MBSTRING_ASC, (const unsigned char *)"Rudolf OS remote access", -1, -1, 0);
	X509_set_issuer_name(x, nm);
	if (!X509_sign(x, k, EVP_sha256())) return -1;
	FILE *f = fopen(EDIR "/key.pem.tmp", "w"); if (!f) return -1; fchmod(fileno(f), 0600); PEM_write_PrivateKey(f, k, NULL, NULL, 0, NULL, NULL); fclose(f);
	f = fopen(EDIR "/cert.pem.tmp", "w"); if (!f) return -1; PEM_write_X509(f, x); fclose(f);
	rename(EDIR "/key.pem.tmp", EDIR "/key.pem"); rename(EDIR "/cert.pem.tmp", EDIR "/cert.pem");
	/* the fingerprint the Remote access page shows, to compare with the browser's */
	unsigned char md[32]; unsigned int ml = 0; X509_digest(x, EVP_sha256(), md, &ml); char fp[100] = ""; for (unsigned i = 0; i < ml; i++) sprintf(fp + strlen(fp), "%02X%s", md[i], i + 1 < ml ? ":" : "");
	strcat(fp, "\n"); wfile(EDIR "/fingerprint", fp, 0644);
	X509_free(x); EVP_PKEY_free(k); return 0;
}

/* ---------------------------------------------------------------- HTTP over TLS */
static int tls_write(const void *b, size_t n) { const char *p = b; while (n) { int w = SSL_write(S, p, n > 1 << 20 ? 1 << 20 : (int)n); if (w <= 0) return -1; p += w; n -= w; } return 0; }
static void reply(int code, const char *ct, const char *body, size_t n, const char *extra)
{
	char h[1500]; int l = snprintf(h, sizeof h, "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nCache-Control: no-store\r\nX-Frame-Options: DENY\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\nStrict-Transport-Security: max-age=31536000\r\n"
		"Content-Security-Policy: default-src 'self'; connect-src 'self' wss:; img-src 'self' data: blob:; style-src 'self' 'unsafe-inline'; frame-ancestors 'none'\r\n%sConnection: close\r\n\r\n",
		code, code == 200 ? "OK" : code == 101 ? "Switching Protocols" : code == 403 ? "Forbidden" : code == 404 ? "Not Found" : code == 429 ? "Too Many Requests" : "Error", ct, n, extra ? extra : "");
	tls_write(h, l); if (n) tls_write(body, n);
}
static void jreply(int code, const char *json, const char *extra) { reply(code, "application/json", json, strlen(json), extra); }
static const char *hdr(const char *h, const char *name, char *o, size_t on)
{
	size_t nl = strlen(name); o[0] = 0;
	for (const char *l = strstr(h, "\r\n"); l; l = strstr(l + 2, "\r\n")) if (!strncasecmp(l + 2, name, nl) && l[2 + nl] == ':') { const char *v = l + 3 + nl; while (*v == ' ') v++; snprintf(o, on, "%.*s", (int)strcspn(v, "\r\n"), v); return o; }
	return NULL;
}
static int cookie(const char *h, const char *name, char *o, size_t on)
{
	char c[2048]; if (!hdr(h, "Cookie", c, sizeof c)) return 0;
	size_t nl = strlen(name);
	for (char *p = c; *p; ) { while (*p == ' ' || *p == ';') p++; if (!strncmp(p, name, nl) && p[nl] == '=') { p += nl + 1; size_t k = strcspn(p, ";"); if (k >= on) return 0; memcpy(o, p, k); o[k] = 0; return 1; } p += strcspn(p, ";"); }
	return 0;
}
static void form(const char *b, const char *name, char *o, size_t on)
{
	o[0] = 0; size_t nl = strlen(name);
	for (const char *p = b; p && *p; p = strchr(p, '&') ? strchr(p, '&') + 1 : NULL) {
		if (strncmp(p, name, nl) || p[nl] != '=') continue;
		p += nl + 1; size_t i = 0;
		while (*p && *p != '&' && i + 1 < on) { if (*p == '+') o[i++] = ' '; else if (*p == '%' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2])) { char x[3] = {p[1], p[2], 0}; o[i++] = (char)strtol(x, NULL, 16); p += 2; } else o[i++] = *p; p++; }
		o[i] = 0; return;
	}
}
static int hexok(const char *s, size_t n) { if (strlen(s) != n) return 0; for (size_t i = 0; i < n; i++) if (!isxdigit((unsigned char)s[i])) return 0; return 1; }

/* ---------------------------------------------------------------- devices, sessions, accounts */
static int device_ok(const char *h)
{
	char t[80], d[65]; if (!cookie(h, "rrd", t, sizeof t) || !hexok(t, 64)) return 0;
	sha256hex(t, d); char *all = slurp(DEVF, NULL); int ok = all && strstr(all, d); free(all); return ok;
}
static void device_add(char *tok)
{
	randhex(tok, 32); char d[65]; sha256hex(tok, d);
	char *all = slurp(DEVF, NULL); size_t l = all ? strlen(all) : 0; char *n = malloc(l + 80); snprintf(n, l + 80, "%s%s\n", all ? all : "", d); wfile(DEVF, n, 0600); free(n); free(all);
}
/* "user\tunattended\texpires" in RDIR/sess/<token> */
static int session_get(const char *h, char *user, size_t un, int *unatt)
{
	char t[80], p[200]; if (!cookie(h, "rrs", t, sizeof t) || !hexok(t, 64)) return 0;
	snprintf(p, sizeof p, RDIR "/sess/%s", t); char *s = slurp(p, NULL); if (!s) return 0;
	char u[100] = ""; int ua = 0; long long ex = 0; int ok = sscanf(s, "%99[^\t]\t%d\t%lld", u, &ua, &ex) == 3 && ex > time(NULL); free(s);
	if (!ok) { unlink(p); return 0; }
	snprintf(user, un, "%s", u); *unatt = ua; return 1;
}
/* failed sign-ins: five within a minute lock it for a minute */
static int fails(int add)
{
	int f = open(RDIR "/fails", O_RDWR | O_CREAT | O_CLOEXEC, 0600); if (f < 0) return 0; flock(f, LOCK_EX);
	char b[64] = ""; ssize_t r = read(f, b, sizeof b - 1); if (r > 0) b[r] = 0;
	int n = 0; long long t0 = 0; sscanf(b, "%d %lld", &n, &t0); long long now = time(NULL);
	if (now - t0 > 60) { n = 0; t0 = now; }
	if (add > 0) n++; else if (add < 0) n = 0;
	char o[64]; int l = snprintf(o, sizeof o, "%d %lld\n", n, t0); lseek(f, 0, SEEK_SET); if (ftruncate(f, 0) || write(f, o, l) < 0) {} close(f);
	return n;
}
/* the account's PBKDF2 hash "p2$<salt b64>$<hash b64>" (210 000 rounds, SHA-256, 32 bytes: the desktop's pwMake) */
static int b64dec(const char *in, unsigned char *out, int max) { int n = EVP_DecodeBlock(out, (const unsigned char *)in, strlen(in)); if (n < 0) return -1; size_t l = strlen(in); while (l && in[l - 1] == '=') { n--; l--; } return n > max ? -1 : n; }
static int account_check(const char *user, const char *pass, int *unatt, char *role, size_t rn)
{
	char *all = slurp(ACCTF, NULL); if (!all) return 0;
	int ok = 0;
	for (char *l = strtok(all, "\n"); l; l = strtok(NULL, "\n")) {
		char n[100] = "", h[300] = "", r[16] = ""; int ua = 0;
		if (sscanf(l, "%99[^\t]\t%299[^\t]\t%15[^\t]\t%d", n, h, r, &ua) != 4 || strcasecmp(n, user)) continue;
		if (strncmp(h, "p2$", 3)) break;   /* an older hash: sign in on the computer once, which upgrades it */
		char *s1 = h + 3, *s2 = strchr(s1, '$'); if (!s2) break; *s2++ = 0;
		unsigned char salt[64], want[64], got[32];
		int sl = b64dec(s1, salt, sizeof salt), wl = b64dec(s2, want, sizeof want);
		if (sl <= 0 || wl != 32) break;
		/* the desktop derives from the salt's base64 decoded bytes (b64d(salt)) */
		if (PKCS5_PBKDF2_HMAC(pass, strlen(pass), salt, sl, 210000, EVP_sha256(), 32, got) && CRYPTO_memcmp(got, want, 32) == 0) { ok = 1; *unatt = ua; snprintf(role, rn, "%s", r); }
		break;
	}
	free(all); return ok;
}

/* ---------------------------------------------------------------- WebSocket */
static int ws_send(int op, const void *d, size_t n)
{
	unsigned char h[10]; int hl = 2; h[0] = 0x80 | op;
	if (n < 126) h[1] = n; else if (n < 65536) { h[1] = 126; h[2] = n >> 8; h[3] = n; hl = 4; } else { h[1] = 127; for (int i = 0; i < 8; i++) h[2 + i] = (uint64_t)n >> (56 - 8 * i); hl = 10; }
	return tls_write(h, hl) || tls_write(d, n) ? -1 : 0;
}
static int ws_text(const char *s) { return ws_send(1, s, strlen(s)); }
static int rd_exact(unsigned char *b, size_t n, int fd, int ms)
{
	size_t g = 0;
	while (g < n) {
		if (!SSL_pending(S)) { struct pollfd p = { fd, POLLIN, 0 }; int r = poll(&p, 1, ms); if (r <= 0) return -1; }
		int r = SSL_read(S, b + g, n - g); if (r <= 0) return -1; g += r;
	}
	return 0;
}
/* one message from the viewer into m (text, up to mn-1 bytes); 0: none waiting; -1: closed */
static int ws_recv(int fd, char *m, size_t mn, int wait_ms)
{
	if (!SSL_pending(S)) { struct pollfd p = { fd, POLLIN, 0 }; int r = poll(&p, 1, wait_ms); if (r == 0) return 0; if (r < 0 || (p.revents & (POLLHUP | POLLERR))) return -1; }
	unsigned char h[14];
	if (rd_exact(h, 2, fd, 5000)) return -1;
	int op = h[0] & 15; uint64_t n = h[1] & 127; int masked = h[1] & 128;
	if (n == 126) { if (rd_exact(h, 2, fd, 5000)) return -1; n = h[0] << 8 | h[1]; } else if (n == 127) { if (rd_exact(h, 8, fd, 5000)) return -1; n = 0; for (int i = 0; i < 8; i++) n = n << 8 | h[i]; }
	unsigned char mk[4] = {0}; if (masked && rd_exact(mk, 4, fd, 5000)) return -1;
	if (n > 65536) return -1;
	unsigned char *b = malloc(n + 1); if (n && rd_exact(b, n, fd, 5000)) { free(b); return -1; }
	for (uint64_t i = 0; i < n; i++) b[i] ^= mk[i & 3];
	if (op == 8) { free(b); return -1; }
	if (op == 9) { ws_send(10, b, n); free(b); return 0; }
	if (op != 1) { free(b); return 0; }
	size_t k = n < mn - 1 ? n : mn - 1; memcpy(m, b, k); m[k] = 0; free(b); return 1;
}

/* ---------------------------------------------------------------- the screen */
static Display *D; static XImage *IM; static XShmSegmentInfo SHM; static int W, H, useshm;
static uint32_t *prev;
static int screen_open(void)
{
	for (int i = 0; i < 20 && !(D = XOpenDisplay(":0")); i++) usleep(200000);
	if (!D) return -1;
	Window r = DefaultRootWindow(D); XWindowAttributes a; XGetWindowAttributes(D, r, &a); W = a.width; H = a.height;
	useshm = XShmQueryExtension(D);
	if (useshm) {
		IM = XShmCreateImage(D, DefaultVisual(D, DefaultScreen(D)), DefaultDepth(D, DefaultScreen(D)), ZPixmap, NULL, &SHM, W, H);
		SHM.shmid = IM ? shmget(IPC_PRIVATE, IM->bytes_per_line * IM->height, IPC_CREAT | 0600) : -1;
		if (SHM.shmid < 0) { useshm = 0; if (IM) XDestroyImage(IM); IM = NULL; }
		else { SHM.shmaddr = IM->data = shmat(SHM.shmid, NULL, 0); SHM.readOnly = False; XShmAttach(D, &SHM); XSync(D, False); shmctl(SHM.shmid, IPC_RMID, NULL); }
	}
	free(prev); prev = calloc((size_t)W * H, 4);
	return 0;
}
static void screen_close(void)
{
	if (IM) { if (useshm) { XShmDetach(D, &SHM); shmdt(SHM.shmaddr); IM->data = NULL; } XDestroyImage(IM); IM = NULL; }
	if (D) XCloseDisplay(D); D = NULL;
}
static uint32_t *grab(void)
{
	Window r = DefaultRootWindow(D);
	if (useshm) { if (!XShmGetImage(D, r, IM, 0, 0, AllPlanes)) return NULL; return (uint32_t *)IM->data; }
	if (IM) XDestroyImage(IM); IM = XGetImage(D, r, 0, 0, W, H, AllPlanes, ZPixmap); return IM ? (uint32_t *)IM->data : NULL;
}
/* the tiles that changed since the last frame (all of them when full), in one message */
static int send_frame(int full)
{
	uint32_t *px = grab(); if (!px) return -1;
	int stride = IM->bytes_per_line / 4;
	size_t cap = 1 << 20, n = 3; unsigned char *m = malloc(cap); m[0] = 'T'; int cnt = 0;
	static unsigned char raw[TILE * TILE * 4]; static unsigned char z[TILE * TILE * 4 + 1024];
	for (int ty = 0; ty < H; ty += TILE) for (int tx = 0; tx < W; tx += TILE) {
		int tw = W - tx < TILE ? W - tx : TILE, th = H - ty < TILE ? H - ty : TILE, ch = full;
		for (int y = 0; y < th && !ch; y++) if (memcmp(px + (size_t)(ty + y) * stride + tx, prev + (size_t)(ty + y) * W + tx, tw * 4)) ch = 1;
		if (!ch) continue;
		for (int y = 0; y < th; y++) { memcpy(raw + (size_t)y * tw * 4, px + (size_t)(ty + y) * stride + tx, tw * 4); memcpy(prev + (size_t)(ty + y) * W + tx, px + (size_t)(ty + y) * stride + tx, tw * 4); }
		uLongf zl = sizeof z; if (compress2(z, &zl, raw, (uLong)tw * th * 4, 1) != Z_OK) continue;
		if (n + 12 + zl > cap) { cap = (n + 12 + zl) * 2; m = realloc(m, cap); }
		unsigned char *q = m + n; q[0] = tx >> 8; q[1] = tx; q[2] = ty >> 8; q[3] = ty; q[4] = tw >> 8; q[5] = tw; q[6] = th >> 8; q[7] = th;
		q[8] = zl >> 24; q[9] = zl >> 16; q[10] = zl >> 8; q[11] = zl; memcpy(q + 12, z, zl); n += 12 + zl; cnt++;
	}
	m[1] = cnt >> 8; m[2] = cnt; int r = cnt ? ws_send(2, m, n) : 0; free(m); return r < 0 ? -1 : cnt;
}
static void input(char *m)
{
	int a = 0, b = 0, c = 0;
	if (m[0] == 'm' && sscanf(m + 1, "%d %d", &a, &b) == 2) { if (a >= 0 && b >= 0 && a < W && b < H) XTestFakeMotionEvent(D, -1, a, b, CurrentTime); }
	else if (m[0] == 'b' && sscanf(m + 1, "%d %d", &a, &b) == 2) { if (a >= 1 && a <= 7) XTestFakeButtonEvent(D, a, b ? True : False, CurrentTime); }
	else if (m[0] == 'k' && sscanf(m + 1, "%d %d", &a, &b) == 2) {
		/* Shift as the viewer holds it; a character that needs Shift (a capital, "!") gets it when the viewer did not
		   send Shift itself (phones' keyboards, pasted text) */
		static int shift; if (a == 0xffe1 || a == 0xffe2) shift = b;
		KeyCode kc = XKeysymToKeycode(D, (KeySym)a);
		if (kc) {
			int need = !shift && XkbKeycodeToKeysym(D, kc, 0, 0) != (KeySym)a && XkbKeycodeToKeysym(D, kc, 0, 1) == (KeySym)a;
			KeyCode sk = need ? XKeysymToKeycode(D, 0xffe1) : 0;
			if (sk && b) XTestFakeKeyEvent(D, sk, True, CurrentTime);
			XTestFakeKeyEvent(D, kc, b ? True : False, CurrentTime);
			if (sk && b) XTestFakeKeyEvent(D, sk, False, CurrentTime);
		}
	}
	else if (m[0] == 'r') { (void)c; }
	XFlush(D);
}

/* ---------------------------------------------------------------- a session */
static void session(int fd, const char *user, int unatt)
{
	char m[256], sp[100];
	/* one at a time */
	char *cur = slurp(RDIR "/session", NULL);
	if (cur) { int pid = atoi(cur); free(cur); if (pid > 0 && !kill(pid, 0)) { ws_text("{\"t\":\"bye\",\"m\":\"Someone is already connected to this computer.\"}"); return; } }
	if (!unatt) {
		char id[17]; randhex(id, 8); char pend[300]; snprintf(pend, sizeof pend, "%s\t%s\t%s\t%lld\n", id, user, peer, (long long)time(NULL));
		wfile(RDIR "/pending", pend, 0644);
		ws_text("{\"t\":\"wait\",\"m\":\"Waiting for someone at the computer to allow the connection…\"}");
		char ap[120]; snprintf(ap, sizeof ap, RDIR "/answer-%s", id); int ans = 0;
		for (int i = 0; i < 600 && !ans; i++) {
			char *a = slurp(ap, NULL); if (a) { ans = !strncmp(a, "allow", 5) ? 1 : -1; free(a); unlink(ap); break; }
			int r = ws_recv(fd, m, sizeof m, 100); if (r < 0) { unlink(RDIR "/pending"); return; }
		}
		unlink(RDIR "/pending");
		if (ans != 1) { ws_text(ans < 0 ? "{\"t\":\"bye\",\"m\":\"The person at the computer did not allow the connection.\"}" : "{\"t\":\"bye\",\"m\":\"Nobody at the computer answered.\"}"); say("remote", "a remote connection by %s from %s was %s", user, peer, ans < 0 ? "refused at the computer" : "not answered"); return; }
	}
	if (screen_open()) { ws_text("{\"t\":\"bye\",\"m\":\"The screen could not be opened.\"}"); return; }
	snprintf(sp, sizeof sp, "%d\t%s\t%s\t%lld\t%d\n", getpid(), user, peer, (long long)time(NULL), unatt); wfile(RDIR "/session", sp, 0644);
	unlink(RDIR "/end");
	say("remote", "%s connected from %s%s", user, peer, unatt ? " (unattended access)" : " (allowed at the computer)");
	time_t t0 = time(NULL);
	ws_text("{\"t\":\"go\"}");
	unsigned char sz[5] = {'S', W >> 8, W, H >> 8, H}; ws_send(2, sz, 5);
	int full = 1, inflight = 0; struct timespec last = {0};
	const char *why = "the viewer closed the connection";
	for (;;) {
		if (!access(RDIR "/end", F_OK)) { unlink(RDIR "/end"); ws_text("{\"t\":\"bye\",\"m\":\"The person at the computer ended the session.\"}"); why = "it was ended at the computer"; break; }
		if (access(RDIR "/on", F_OK)) { ws_text("{\"t\":\"bye\",\"m\":\"Remote access was turned off.\"}"); why = "remote access was turned off"; break; }
		/* the screen size changed (Display settings): start again with the new size */
		{ XWindowAttributes a; XGetWindowAttributes(D, DefaultRootWindow(D), &a); if (a.width != W || a.height != H) { screen_close(); if (screen_open()) break; unsigned char s2[5] = {'S', W >> 8, W, H >> 8, H}; ws_send(2, s2, 5); full = 1; inflight = 0; } }
		struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
		long el = (now.tv_sec - last.tv_sec) * 1000 + (now.tv_nsec - last.tv_nsec) / 1000000;
		if (!inflight && el >= 66) { int c = send_frame(full); if (c < 0) break; full = 0; last = now; if (c > 0) inflight = 1; }
		int r = ws_recv(fd, m, sizeof m, inflight ? 40 : 30);
		if (r < 0) break;
		if (r == 1) { if (m[0] == 'a') inflight = 0; else input(m); }
		/* a frame not acknowledged within 5 s: the viewer is gone or too slow; send the next one anyway */
		if (inflight && el > 5000) inflight = 0;
	}
	screen_close(); unlink(RDIR "/session");
	say("remote", "the remote session of %s from %s ended after %lld min (%s)", user, peer, (long long)(time(NULL) - t0 + 30) / 60, why);
}

/* ---------------------------------------------------------------- one connection */
static void serve(int fd)
{
	char h[8192]; size_t n = 0; char *end = NULL;
	while (n < sizeof h - 1) { struct pollfd p = { fd, POLLIN, 0 }; if (!SSL_pending(S) && poll(&p, 1, 15000) <= 0) return; int r = SSL_read(S, h + n, sizeof h - 1 - n); if (r <= 0) return; n += r; h[n] = 0; if ((end = strstr(h, "\r\n\r\n"))) break; }
	if (!end) return;
	char meth[8] = "", path[512] = ""; sscanf(h, "%7s %511s", meth, path);
	char *qm = strchr(path, '?'); if (qm) *qm = 0;
	if (access(RDIR "/on", F_OK)) { jreply(403, "{\"ok\":false,\"error\":\"Remote access is off.\"}", NULL); return; }
	if (!strcmp(meth, "GET") && (!strcmp(path, "/") || !strcmp(path, "/remote.js") || !strcmp(path, "/remote.css"))) {
		char fp[200]; snprintf(fp, sizeof fp, WWWR "%s", !strcmp(path, "/") ? "/index.html" : path); size_t l; char *b = slurp(fp, &l);
		if (!b) { jreply(404, "{\"ok\":false}", NULL); return; }
		reply(200, !strcmp(path, "/") ? "text/html; charset=utf-8" : path[8] == 'j' ? "text/javascript" : "text/css", b, l, NULL); free(b); return;
	}
	if (!strcmp(meth, "GET") && !strcmp(path, "/status")) {
		char u[100]; int ua; int s = session_get(h, u, sizeof u, &ua);
		char j[300]; snprintf(j, sizeof j, "{\"ok\":true,\"paired\":%s,\"signedIn\":%s}", device_ok(h) ? "true" : "false", s ? "true" : "false"); jreply(200, j, NULL); return;
	}
	if (!strcmp(meth, "POST") && !strcmp(path, "/login")) {
		char cl[32]; size_t want = hdr(h, "Content-Length", cl, sizeof cl) ? strtoul(cl, NULL, 10) : 0; if (want > 4096) return;
		char body[4100]; size_t have = n - (end + 4 - h); if (have > want) have = want; memcpy(body, end + 4, have);
		while (have < want) { int r = SSL_read(S, body + have, want - have); if (r <= 0) return; have += r; } body[have] = 0;
		if (fails(0) >= 5) { jreply(429, "{\"ok\":false,\"error\":\"Too many wrong tries. Wait a minute and try again.\"}", NULL); return; }
		char code[16], user[100], pass[300]; form(body, "code", code, sizeof code); form(body, "user", user, sizeof user); form(body, "pass", pass, sizeof pass);
		explicit_bzero(body, sizeof body);
		int paired = device_ok(h); char setdev[200] = "";
		if (!paired) {
			char *want_code = slurp(RDIR "/code", NULL); int good = want_code && strlen(code) == 6 && !strncmp(want_code, code, 6); free(want_code);
			if (!good) { fails(1); say("signin", "remote access: a wrong pairing code from %s", peer); jreply(403, "{\"ok\":false,\"needCode\":true,\"error\":\"That pairing code is not right. It is shown on the computer in Settings › Remote access.\"}", NULL); return; }
			char tok[70]; device_add(tok); snprintf(setdev, sizeof setdev, "Set-Cookie: rrd=%s; Max-Age=31536000; Path=/; Secure; HttpOnly; SameSite=Strict\r\n", tok);
		}
		int ua = 0; char role[16] = "";
		if (!account_check(user, pass, &ua, role, sizeof role)) {
			explicit_bzero(pass, sizeof pass); int f = fails(1); say("signin", "remote access: a wrong password from %s", peer);
			char j[300]; snprintf(j, sizeof j, "{\"ok\":false,\"error\":\"The account name or password is not right.%s\"}", f >= 5 ? " Too many wrong tries: wait a minute." : "");
			jreply(403, j, setdev[0] ? setdev : NULL); return;
		}
		explicit_bzero(pass, sizeof pass); fails(-1);
		char tok[70], sp[200], sv[200]; randhex(tok, 32); mkdir(RDIR "/sess", 0700); snprintf(sp, sizeof sp, RDIR "/sess/%s", tok);
		snprintf(sv, sizeof sv, "%s\t%d\t%lld\n", user, ua, (long long)time(NULL) + 12 * 3600); wfile(sp, sv, 0600);
		char ex[500]; snprintf(ex, sizeof ex, "%sSet-Cookie: rrs=%s; Path=/; Secure; HttpOnly; SameSite=Strict\r\n", setdev, tok);
		jreply(200, "{\"ok\":true}", ex); return;
	}
	if (!strcmp(meth, "GET") && !strcmp(path, "/ws")) {
		char u[100]; int ua = 0;
		char k[100], host[200], org[300]; hdr(h, "Sec-WebSocket-Key", k, sizeof k); hdr(h, "Host", host, sizeof host); hdr(h, "Origin", org, sizeof org);
		/* the page itself only: the Origin must be this computer's address */
		char want[260]; snprintf(want, sizeof want, "https://%s", host);
		if (!k[0] || strcmp(org, want) || !session_get(h, u, sizeof u, &ua) || !device_ok(h)) { jreply(403, "{\"ok\":false,\"error\":\"Sign in first.\"}", NULL); return; }
		char acc[200]; snprintf(acc, sizeof acc, "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", k); unsigned char sh[20]; SHA1((unsigned char *)acc, strlen(acc), sh);
		char b64[40]; EVP_EncodeBlock((unsigned char *)b64, sh, 20);
		char hh[300]; int l = snprintf(hh, sizeof hh, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", b64);
		if (tls_write(hh, l)) return;
		session(fd, u, ua); return;
	}
	jreply(404, "{\"ok\":false,\"error\":\"not found\"}", NULL);
}

int main(int argc, char **argv)
{
	(void)argc; (void)argv;
	signal(SIGPIPE, SIG_IGN); signal(SIGCHLD, SIG_DFL);
	mkdir("/run/rudolf", 0755); mkdir(RDIR, 0755); mkdir(RDIR "/sess", 0700);
	if (access(EDIR "/cert.pem", R_OK) || access(EDIR "/key.pem", R_OK)) if (make_cert()) { fprintf(stderr, "rudolf-remote: the certificate could not be made\n"); return 1; }
	SSL_CTX *c = SSL_CTX_new(TLS_server_method()); SSL_CTX_set_min_proto_version(c, TLS1_2_VERSION);
	if (SSL_CTX_use_certificate_file(c, EDIR "/cert.pem", SSL_FILETYPE_PEM) != 1 || SSL_CTX_use_PrivateKey_file(c, EDIR "/key.pem", SSL_FILETYPE_PEM) != 1) { fprintf(stderr, "rudolf-remote: bad certificate\n"); return 1; }
	/* IPv6 and IPv4 on one socket; IPv4 alone where the kernel has no IPv6 */
	int one = 1, zero = 0, s = socket(AF_INET6, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (s >= 0) {
		setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one); setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &zero, sizeof zero);
		struct sockaddr_in6 a = { .sin6_family = AF_INET6, .sin6_port = htons(PORT), .sin6_addr = in6addr_any };
		if (bind(s, (struct sockaddr *)&a, sizeof a)) { close(s); s = -1; }
	}
	if (s < 0) {
		s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0); setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
		struct sockaddr_in a4 = { .sin_family = AF_INET, .sin_port = htons(PORT), .sin_addr.s_addr = htonl(INADDR_ANY) };
		if (s < 0 || bind(s, (struct sockaddr *)&a4, sizeof a4)) { perror("rudolf-remote: port 8443"); return 1; }
	}
	if (listen(s, 16)) { perror("rudolf-remote: listen"); return 1; }
	char pid[32]; snprintf(pid, sizeof pid, "%d\n", getpid()); wfile(RDIR "/pid", pid, 0644);
	fprintf(stderr, "rudolf-remote: serving https on port %d\n", PORT);
	for (;;) {
		while (waitpid(-1, NULL, WNOHANG) > 0) {}
		struct sockaddr_storage pa; socklen_t pl = sizeof pa;
		int fd = accept4(s, (struct sockaddr *)&pa, &pl, SOCK_CLOEXEC); if (fd < 0) continue;
		pid_t p = fork();
		if (p == 0) {
			close(s); setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
			if (pa.ss_family == AF_INET) inet_ntop(AF_INET, &((struct sockaddr_in *)&pa)->sin_addr, peer, sizeof peer);
			else { struct sockaddr_in6 *p6 = (struct sockaddr_in6 *)&pa; if (IN6_IS_ADDR_V4MAPPED(&p6->sin6_addr)) inet_ntop(AF_INET, &p6->sin6_addr.s6_addr[12], peer, sizeof peer); else inet_ntop(AF_INET6, &p6->sin6_addr, peer, sizeof peer); }
			S = SSL_new(c); SSL_set_fd(S, fd);
			if (SSL_accept(S) == 1) serve(fd);
			SSL_shutdown(S); SSL_free(S); close(fd); _exit(0);
		}
		close(fd);
	}
}
