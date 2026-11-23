/* rudolf-shares: the network side of File Explorer's network shares (Rudolf OS 1.6.2, docs/plan-1.6.2.md Part 35). A client
 * only: it listens for nothing but the answers to what it asks, for a few seconds; it mounts nothing (the drives helper does,
 * rudolf-drives.c share-mount, through the kernel's own SMB client). rudolfd runs it as nobody (find, resolve, probe) or as
 * the signed-in account's own Linux user (list), with no privilege.
 *
 *   rudolf-shares resolve NAME         the server's IPv4 address: NAME itself when it is one, else the names' file and DNS,
 *                                      else multicast DNS (NAME.local: Windows 10 and 11, NAS boxes, Macs and Linux answer)
 *   rudolf-shares probe IP [2.1]       which SMB the server speaks (JSON): {"ok":true,"dialect":"3.1.1","encryption":true,
 *                                      "signing":"required"} or {"ok":false,"why":"smb1"|"smb20"|"smb21"|"noanswer"|"proto"}
 *   rudolf-shares list SERVER IP [2.1] the server's shared folders (JSON), signed in with the name, password and domain on
 *                                      standard input (three lines; an empty name: as a guest): SMB 3 (3.0, 3.0.2, 3.1.1),
 *                                      2.1 only when asked; NTLMv2 inside SPNEGO; the session signed (HMAC-SHA256 for 2.1,
 *                                      AES-CMAC for 3); srvsvc's NetrShareEnum over the IPC$ pipe. Answers {"ok":true,
 *                                      "shares":[{"name":..,"remark":..}],"dialect":..} or {"ok":false,"why":"password"|
 *                                      "denied"|"smb1"|"smb20"|"smb21"|"noanswer"|"proto"}
 *   rudolf-shares find                 the computers and NAS boxes that offer shares on the home network (JSON): Windows by
 *                                      WS-Discovery (a Probe to 239.255.255.250:3702 from port 3702, the answers' computer
 *                                      names by WS-Transfer Get from their own address), NAS boxes, Macs and Linux by
 *                                      multicast DNS (_smb._tcp, from port 5353 as rudolf-print asks); never on a cellular
 *                                      modem's interface (/run/rudolf/firewall-cellular), answers only from private addresses
 * The password never leaves this process but in NTLMv2's proofs; it is never printed or written anywhere.
 * Tests: RS_WSD=ADDR:PORT and RS_MDNS=ADDR:PORT send find's and resolve's questions there; RS_PORT=N asks port N of the
 * server instead of 445.
 *
 * Copyright (C) 2026 the Rudolf OS authors. SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/core_names.h>

/* ---------------- output */
static void js(const char *s)
{
	putchar('"');
	for (const unsigned char *c = (const unsigned char *)s; *c; c++) {
		if (*c == '"' || *c == '\\') printf("\\%c", *c); else if (*c < 0x20) printf("\\u%04x", *c); else putchar(*c);
	}
	putchar('"');
}
static void bjs(void *x, const char *s);
static int fail(const char *why) { printf("{\"ok\":false,\"why\":\"%s\"}\n", why); return 1; }

typedef struct { unsigned char *b; size_t n, cap; } buf;
static void put(buf *x, const void *p, size_t n)
{
	if (x->n + n > x->cap) { x->cap = (x->n + n) * 2 + 256; x->b = realloc(x->b, x->cap); if (!x->b) exit(3); }
	memcpy(x->b + x->n, p, n); x->n += n;
}
static void p8(buf *x, unsigned v) { unsigned char c = v; put(x, &c, 1); }
static void p16(buf *x, unsigned v) { unsigned char c[2] = { v, v >> 8 }; put(x, c, 2); }
static void p32(buf *x, uint32_t v) { unsigned char c[4] = { v, v >> 8, v >> 16, v >> 24 }; put(x, c, 4); }
static void p64(buf *x, uint64_t v) { p32(x, (uint32_t)v); p32(x, (uint32_t)(v >> 32)); }
static void pz(buf *x, size_t n) { while (n--) p8(x, 0); }
static void palign(buf *x, size_t a, size_t base) { while ((x->n - base) % a) p8(x, 0); }
static void bjs(void *xv, const char *s)
{
	buf *x = xv; put(x, "\"", 1);
	for (const unsigned char *c = (const unsigned char *)s; *c; c++) {
		char e[8]; if (*c == '"' || *c == '\\') { e[0] = '\\'; e[1] = *c; put(x, e, 2); } else if (*c < 0x20) { snprintf(e, sizeof e, "\\u%04x", *c); put(x, e, 6); } else put(x, c, 1);
	}
	put(x, "\"", 1);
}
static void s16(unsigned char *p, unsigned v) { p[0] = v; p[1] = v >> 8; }
static void s32(unsigned char *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static unsigned g16(const unsigned char *p) { return p[0] | p[1] << 8; }
static uint32_t g32(const unsigned char *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint64_t g64(const unsigned char *p) { return g32(p) | (uint64_t)g32(p + 4) << 32; }
/* UTF-8 to UTF-16LE (characters outside the basic plane as surrogate pairs); upper: ASCII letters upper-cased (NTLMv2's user) */
static void putu16(buf *x, const char *s, int upper)
{
	const unsigned char *c = (const unsigned char *)s;
	while (*c) {
		uint32_t u = *c++;
		if (u >= 0xC0 && u < 0xE0 && (*c & 0xC0) == 0x80) u = (u & 0x1F) << 6 | (*c++ & 0x3F);
		else if (u >= 0xE0 && u < 0xF0 && (c[0] & 0xC0) == 0x80 && (c[1] & 0xC0) == 0x80) { u = (u & 0x0F) << 12 | (c[0] & 0x3F) << 6 | (c[1] & 0x3F); c += 2; }
		else if (u >= 0xF0 && (c[0] & 0xC0) == 0x80 && (c[1] & 0xC0) == 0x80 && (c[2] & 0xC0) == 0x80) { u = (u & 0x07) << 18 | (c[0] & 0x3F) << 12 | (c[1] & 0x3F) << 6 | (c[2] & 0x3F); c += 3; }
		else if (u >= 0x80) u = 0xFFFD;
		if (upper && u >= 'a' && u <= 'z') u -= 32;
		if (u >= 0x10000) { u -= 0x10000; p16(x, 0xD800 | u >> 10); p16(x, 0xDC00 | (u & 0x3FF)); } else p16(x, u);
	}
}
/* UTF-16LE (n code units) to UTF-8 */
static void u16to8(const unsigned char *p, size_t n, char *o, size_t on)
{
	size_t k = 0;
	for (size_t i = 0; i < n && k + 5 < on; i++) {
		uint32_t u = g16(p + 2 * i);
		if (u >= 0xD800 && u < 0xDC00 && i + 1 < n) { uint32_t l = g16(p + 2 * i + 2); if (l >= 0xDC00 && l < 0xE000) { u = 0x10000 + ((u - 0xD800) << 10) + (l - 0xDC00); i++; } }
		if (!u) break;
		if (u < 0x80) o[k++] = u;
		else if (u < 0x800) { o[k++] = 0xC0 | u >> 6; o[k++] = 0x80 | (u & 0x3F); }
		else if (u < 0x10000) { o[k++] = 0xE0 | u >> 12; o[k++] = 0x80 | (u >> 6 & 0x3F); o[k++] = 0x80 | (u & 0x3F); }
		else { o[k++] = 0xF0 | u >> 18; o[k++] = 0x80 | (u >> 12 & 0x3F); o[k++] = 0x80 | (u >> 6 & 0x3F); o[k++] = 0x80 | (u & 0x3F); }
	}
	o[k] = 0;
}

/* ---------------- crypto: MD4 (NTLM's hash of the password; OpenSSL 3 keeps it in its legacy provider, which Rudolf OS
   does not ship) and RC4 (NTLM's key exchange, the same) written out here; the rest from OpenSSL's default provider */
#define MD4F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define MD4G(x, y, z) (((x) & (y)) | ((x) & (z)) | ((y) & (z)))
#define MD4H(x, y, z) ((x) ^ (y) ^ (z))
#define ROL(v, s) ((v) << (s) | (v) >> (32 - (s)))
static void md4(const unsigned char *m, size_t n, unsigned char out[16])
{
	uint32_t h[4] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };
	size_t tot = ((n + 8) / 64 + 1) * 64; unsigned char *p = calloc(1, tot); if (!p) exit(3);
	memcpy(p, m, n); p[n] = 0x80; uint64_t bits = (uint64_t)n * 8; for (int i = 0; i < 8; i++) p[tot - 8 + i] = bits >> (8 * i);
	for (size_t o = 0; o < tot; o += 64) {
		uint32_t X[16]; for (int i = 0; i < 16; i++) X[i] = g32(p + o + 4 * i);
		uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
		static const int r1[4] = { 3, 7, 11, 19 }, r2[4] = { 3, 5, 9, 13 }, r3[4] = { 3, 9, 11, 15 };
		static const int k3[16] = { 0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15 };
		for (int i = 0; i < 16; i++) { uint32_t t = a + MD4F(b, c, d) + X[i]; t = ROL(t, r1[i % 4]); a = d; d = c; c = b; b = t; }
		for (int i = 0; i < 16; i++) { int k = (i % 4) * 4 + i / 4; uint32_t t = a + MD4G(b, c, d) + X[k] + 0x5a827999; t = ROL(t, r2[i % 4]); a = d; d = c; c = b; b = t; }
		for (int i = 0; i < 16; i++) { uint32_t t = a + MD4H(b, c, d) + X[k3[i]] + 0x6ed9eba1; t = ROL(t, r3[i % 4]); a = d; d = c; c = b; b = t; }
		h[0] += a; h[1] += b; h[2] += c; h[3] += d;
	}
	for (int i = 0; i < 4; i++) s32(out + 4 * i, h[i]);
	explicit_bzero(p, tot); free(p);
}
static void rc4(const unsigned char *key, size_t kn, const unsigned char *in, unsigned char *out, size_t n)
{
	unsigned char S[256]; for (int i = 0; i < 256; i++) S[i] = i;
	for (int i = 0, j = 0; i < 256; i++) { j = (j + S[i] + key[i % kn]) & 255; unsigned char t = S[i]; S[i] = S[j]; S[j] = t; }
	for (size_t k = 0, i = 0, j = 0; k < n; k++) { i = (i + 1) & 255; j = (j + S[i]) & 255; unsigned char t = S[i]; S[i] = S[j]; S[j] = t; out[k] = in[k] ^ S[(S[i] + S[j]) & 255]; }
	explicit_bzero(S, sizeof S);
}
static void hmac_md5(const unsigned char *k, size_t kn, const unsigned char *m, size_t n, unsigned char out[16])
{
	unsigned int ol = 16; HMAC(EVP_md5(), k, (int)kn, m, n, out, &ol);
}
static void hmac_sha256(const unsigned char *k, size_t kn, const unsigned char *m, size_t n, unsigned char out[32])
{
	unsigned int ol = 32; HMAC(EVP_sha256(), k, (int)kn, m, n, out, &ol);
}
static int aes_cmac(const unsigned char key[16], const unsigned char *m, size_t n, unsigned char out[16])
{
	EVP_MAC *mac = EVP_MAC_fetch(NULL, "CMAC", NULL); if (!mac) return -1;
	EVP_MAC_CTX *c = EVP_MAC_CTX_new(mac); char cip[] = "AES-128-CBC";
	OSSL_PARAM pr[] = { OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_CIPHER, cip, 0), OSSL_PARAM_construct_end() };
	size_t ol = 0; int ok = c && EVP_MAC_init(c, key, 16, pr) && EVP_MAC_update(c, m, n) && EVP_MAC_final(c, out, &ol, 16) && ol == 16;
	EVP_MAC_CTX_free(c); EVP_MAC_free(mac); return ok ? 0 : -1;
}
/* SP800-108 in counter mode with HMAC-SHA256, one round, 128 bits (MS-SMB2 3.1.4.2); label and context with their NUL */
static void smb_kdf(const unsigned char ki[16], const unsigned char *label, size_t ln, const unsigned char *ctx, size_t cn, unsigned char out[16])
{
	buf m = {0}; unsigned char one[4] = { 0, 0, 0, 1 }, L[4] = { 0, 0, 0, 128 }; put(&m, one, 4); put(&m, label, ln); p8(&m, 0); put(&m, ctx, cn); put(&m, L, 4);
	unsigned char h[32]; hmac_sha256(ki, 16, m.b, m.n, h); memcpy(out, h, 16); explicit_bzero(h, sizeof h); free(m.b);
}

/* ---------------- the SMB2 connection */
typedef struct {
	int fd; uint64_t mid, sid; uint32_t tid; unsigned dialect, secmode; uint32_t caps; int sign, cmac;
	unsigned char skey[16], sigkey[16], preauth[64]; int p311;
} smb;
static int tcp_to(const char *ip, int port, int ms)
{
	struct sockaddr_storage ss = {0}; socklen_t sl;
	struct sockaddr_in *a4 = (struct sockaddr_in *)&ss; struct sockaddr_in6 *a6 = (struct sockaddr_in6 *)&ss;
	if (inet_pton(AF_INET, ip, &a4->sin_addr) == 1) { a4->sin_family = AF_INET; a4->sin_port = htons(port); sl = sizeof *a4; }
	else if (inet_pton(AF_INET6, ip, &a6->sin6_addr) == 1) { a6->sin6_family = AF_INET6; a6->sin6_port = htons(port); sl = sizeof *a6; }
	else return -1;
	int fd = socket(ss.ss_family, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0); if (fd < 0) return -1;
	if (connect(fd, (struct sockaddr *)&ss, sl) && errno != EINPROGRESS) { close(fd); return -1; }
	struct pollfd p = { fd, POLLOUT, 0 }; int e = 0; socklen_t el = sizeof e;
	if (poll(&p, 1, ms) != 1 || getsockopt(fd, SOL_SOCKET, SO_ERROR, &e, &el) || e) { close(fd); return -1; }
	fcntl(fd, F_SETFL, 0);
	struct timeval tv = { 8, 0 }; setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv); setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
	int one = 1; setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
	return fd;
}
static int full(int fd, void *b, size_t n, int wr)
{
	size_t d = 0; while (d < n) { ssize_t r = wr ? send(fd, (char *)b + d, n - d, MSG_NOSIGNAL) : recv(fd, (char *)b + d, n - d, 0); if (r <= 0) { if (r < 0 && errno == EINTR) continue; return -1; } d += r; }
	return 0;
}
static void preauth_add(smb *c, const unsigned char *m, size_t n)
{
	if (!c->p311) return;
	EVP_MD_CTX *x = EVP_MD_CTX_new(); unsigned int ol = 64;
	EVP_DigestInit_ex(x, EVP_sha512(), NULL); EVP_DigestUpdate(x, c->preauth, 64); EVP_DigestUpdate(x, m, n); EVP_DigestFinal_ex(x, c->preauth, &ol); EVP_MD_CTX_free(x);
}
/* a request: the header made here (body: what follows it); signed when the session is */
static int smb_send(smb *c, unsigned cmd, const buf *body, int hash)
{
	buf m = {0}; unsigned char h[64] = { 0xFE, 'S', 'M', 'B', 64, 0 };
	s16(h + 6, 1); s16(h + 12, cmd); s16(h + 14, 31);
	uint32_t fl = c->sign && cmd != 0 && cmd != 1 ? 8 : 0; s32(h + 16, fl);
	s32(h + 24, (uint32_t)c->mid); s32(h + 28, (uint32_t)(c->mid >> 32)); c->mid++;
	s32(h + 32, 0xFEFF); s32(h + 36, c->tid); s32(h + 40, (uint32_t)c->sid); s32(h + 44, (uint32_t)(c->sid >> 32));
	put(&m, h, 64); put(&m, body->b, body->n);
	if (fl) {
		unsigned char sg[32];
		if (c->cmac) { if (aes_cmac(c->sigkey, m.b, m.n, sg)) { free(m.b); return -1; } }
		else hmac_sha256(c->skey, 16, m.b, m.n, sg);
		memcpy(m.b + 48, sg, 16);
	}
	if (hash) preauth_add(c, m.b, m.n);
	unsigned char t[4] = { 0, (unsigned char)(m.n >> 16), (unsigned char)(m.n >> 8), (unsigned char)m.n };
	int r = full(c->fd, t, 4, 1) || full(c->fd, m.b, m.n, 1) ? -1 : 0; free(m.b); return r;
}
/* the answer (its status; *out the whole message, header first), passing over interim answers (STATUS_PENDING) */
static int64_t smb_recv(smb *c, buf *out, int *smb1)
{
	for (;;) {
		unsigned char t[4]; if (full(c->fd, t, 4, 0)) return -1;
		size_t n = (size_t)t[1] << 16 | t[2] << 8 | t[3]; if (t[0] || n < 4 || n > (1 << 24)) return -1;
		out->n = 0; if (out->cap < n) { out->cap = n; out->b = realloc(out->b, n); if (!out->b) exit(3); }
		if (full(c->fd, out->b, n, 0)) return -1; out->n = n;
		if (!memcmp(out->b, "\xffSMB", 4)) { if (smb1) *smb1 = 1; return -2; }
		if (n < 64 || memcmp(out->b, "\xfeSMB", 4)) return -1;
		uint32_t st = g32(out->b + 8);
		if (st == 0x00000103 && (g32(out->b + 16) & 2)) continue;   /* STATUS_PENDING, asynchronous: the real answer follows */
		c->sid = c->sid ? c->sid : g64(out->b + 40);
		return st;
	}
}
#define ST_MORE 0xC0000016u
#define ST_OVERFLOW 0x80000005u
/* NEGOTIATE: 3.0, 3.0.2 and 3.1.1, or 2.1 alone when asked. 0, or why not */
static const char *smb_negotiate(smb *c, int v21)
{
	buf b = {0}; unsigned ds[3] = { 0x0300, 0x0302, 0x0311 }; int nd = v21 ? 1 : 3; if (v21) ds[0] = 0x0210;
	p16(&b, 36); p16(&b, nd); p16(&b, 1); p16(&b, 0); p32(&b, v21 ? 0 : 0x40 /* GLOBAL_CAP_ENCRYPTION */);
	unsigned char guid[16]; RAND_bytes(guid, 16); put(&b, guid, 16);
	size_t ctxoff = b.n; p32(&b, 0); p16(&b, 0); p16(&b, 0);
	for (int i = 0; i < nd; i++) p16(&b, ds[i]);
	if (!v21) {
		c->p311 = 1; memset(c->preauth, 0, 64);
		while ((64 + b.n) % 8) p8(&b, 0);
		s32(b.b + ctxoff, 64 + (uint32_t)b.n); s16(b.b + ctxoff + 4, 2);
		/* PREAUTH_INTEGRITY: SHA-512, a salt */
		p16(&b, 1); p16(&b, 38); p32(&b, 0); p16(&b, 1); p16(&b, 32); p16(&b, 1); unsigned char salt[32]; RAND_bytes(salt, 32); put(&b, salt, 32);
		while ((64 + b.n) % 8) p8(&b, 0);
		/* ENCRYPTION: AES-128-GCM, AES-128-CCM (so the server says whether it encrypts) */
		p16(&b, 2); p16(&b, 6); p32(&b, 0); p16(&b, 2); p16(&b, 2); p16(&b, 1);
	}
	int r = smb_send(c, 0, &b, 1); free(b.b); if (r) return "noanswer";
	buf a = {0}; int s1 = 0; int64_t st = smb_recv(c, &a, &s1);
	if (st == -2) { free(a.b); return "smb1"; }
	if (st < 0 || st != 0 || a.n < 64 + 64) { free(a.b); return st < 0 ? "noanswer" : "proto"; }
	const unsigned char *p = a.b + 64;
	c->secmode = g16(p + 2); c->dialect = g16(p + 4); c->caps = g32(p + 24);
	if (c->dialect != 0x0311) c->p311 = 0; else preauth_add(c, a.b, a.n);
	if (c->dialect == 0x0311) {   /* 3.1.1: encryption is said by a context with a cipher */
		uint32_t off = g32(p + 60); unsigned cnt = g16(p + 6); size_t o = off;
		for (unsigned i = 0; i < cnt && o + 8 <= a.n; i++) {
			unsigned ty = g16(a.b + o), ln = g16(a.b + o + 2);
			if (ty == 2 && ln >= 4 && o + 8 + 4 <= a.n && g16(a.b + o + 10)) c->caps |= 0x40;
			o += 8 + ln; o = (o + 7) & ~(size_t)7;
		}
	}
	free(a.b);
	if (c->dialect != 0x0210 && c->dialect != 0x0300 && c->dialect != 0x0302 && c->dialect != 0x0311) return "proto";
	return NULL;
}
static const char *dialect_name(unsigned d) { return d == 0x0311 ? "3.1.1" : d == 0x0302 ? "3.0.2" : d == 0x0300 ? "3.0" : d == 0x0210 ? "2.1" : d == 0x0202 ? "2.0.2" : "?"; }
/* what an older server speaks, when SMB 3 was not taken: a NEGOTIATE that offers 2.0.2 and 2.1 only to see */
static const char *smb_older(const char *ip, int port, int v21)
{
	smb c = {0}; c.fd = tcp_to(ip, port, 4000); if (c.fd < 0) return "noanswer";
	buf b = {0}; p16(&b, 36); p16(&b, 2); p16(&b, 1); p16(&b, 0); p32(&b, 0); unsigned char g[16]; RAND_bytes(g, 16); put(&b, g, 16); p64(&b, 0); p16(&b, 0x0202); p16(&b, 0x0210);
	const char *why = "proto"; buf a = {0}; int s1 = 0;
	if (!smb_send(&c, 0, &b, 0)) { int64_t st = smb_recv(&c, &a, &s1);
		if (st == -2) why = "smb1"; else if (st == 0 && a.n >= 70) { unsigned d = g16(a.b + 68); why = d == 0x0202 ? "smb20" : d == 0x0210 ? (v21 ? "proto" : "smb21") : "proto"; }
		else if (st < 0) why = "smb1"; }   /* (an SMB 1 server that hangs up on SMB 2's NEGOTIATE) */
	free(a.b); free(b.b); close(c.fd); return why;
}

/* ---------------- NTLMv2 inside SPNEGO */
#define NF_UNICODE 0x00000001u
#define NF_REQTARGET 0x00000004u
#define NF_SIGN 0x00000010u
#define NF_NTLM 0x00000200u
#define NF_ANON 0x00000800u
#define NF_ALWAYS 0x00008000u
#define NF_ESS 0x00080000u
#define NF_TINFO 0x00800000u
#define NF_VERSION 0x02000000u
#define NF_128 0x20000000u
#define NF_KEYX 0x40000000u
#define NF_56 0x80000000u
static void der_len(buf *x, size_t n) { if (n < 128) p8(x, n); else if (n < 256) { p8(x, 0x81); p8(x, n); } else { p8(x, 0x82); p8(x, n >> 8); p8(x, n & 255); } }
static void der(buf *x, unsigned tag, const buf *in) { p8(x, tag); der_len(x, in->n); put(x, in->b, in->n); }
static const unsigned char OID_SPNEGO[] = { 0x06, 0x06, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x02 };
static const unsigned char OID_NTLM[] = { 0x06, 0x0a, 0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x02, 0x02, 0x0a };
static void spnego_init(buf *out, const buf *ntlm)
{
	buf t = {0}, a = {0}, b = {0}, c = {0}, d = {0}, e = {0};
	put(&t, OID_NTLM, sizeof OID_NTLM); der(&a, 0x30, &t);            /* MechTypeList */
	der(&b, 0xa0, &a);                                                 /* [0] mechTypes */
	buf mt = {0}; der(&mt, 0x04, ntlm); der(&b, 0xa2, &mt);            /* [2] mechToken */
	der(&c, 0x30, &b); der(&d, 0xa0, &c);                              /* NegTokenInit */
	put(&e, OID_SPNEGO, sizeof OID_SPNEGO); put(&e, d.b, d.n); der(out, 0x60, &e);
	free(t.b); free(a.b); free(b.b); free(c.b); free(d.b); free(e.b); free(mt.b);
}
static void spnego_resp(buf *out, const buf *ntlm)
{
	buf a = {0}, b = {0}, c = {0}; der(&a, 0x04, ntlm); der(&b, 0xa2, &a); der(&c, 0x30, &b); der(out, 0xa1, &c);
	free(a.b); free(b.b); free(c.b);
}
static void sec_field(buf *m, size_t at, size_t len, size_t off) { s16(m->b + at, len); s16(m->b + at + 2, len); s32(m->b + at + 4, off); }
static int session_setup(smb *c, const buf *tok, buf *ans, unsigned *sflags)
{
	buf b = {0}; p16(&b, 25); p8(&b, 0); p8(&b, 1); p32(&b, 0); p32(&b, 0); p16(&b, 64 + 24); p16(&b, tok->n); p64(&b, 0); put(&b, tok->b, tok->n);
	int r = smb_send(c, 1, &b, 1); free(b.b); if (r) return -1;
	int64_t st = smb_recv(c, ans, NULL); if (st < 0) return -1;
	if (st == ST_MORE) preauth_add(c, ans->b, ans->n);
	if (ans->n >= 64 + 8) *sflags = g16(ans->b + 64 + 2);
	return (int)(uint32_t)st;
}
/* signed in: 0, else why */
static const char *smb_login(smb *c, const char *user, const char *pass, const char *dom)
{
	int anon = !*user;
	uint32_t fl = NF_UNICODE | NF_REQTARGET | NF_SIGN | NF_NTLM | NF_ALWAYS | NF_ESS | NF_TINFO | NF_VERSION | NF_128 | NF_KEYX | NF_56 | (anon ? NF_ANON : 0);
	buf n1 = {0}; put(&n1, "NTLMSSP\0", 8); p32(&n1, 1); p32(&n1, fl); pz(&n1, 16); p8(&n1, 10); p8(&n1, 0); p16(&n1, 19045); pz(&n1, 3); p8(&n1, 15);
	buf t1 = {0}; spnego_init(&t1, &n1); free(n1.b);
	buf a = {0}; unsigned sf = 0; int st = session_setup(c, &t1, &a, &sf); free(t1.b);
	if (st == -1) { free(a.b); return "noanswer"; }
	if ((uint32_t)st != ST_MORE || a.n < 64 + 8) { free(a.b); return st == (int)0xC000006D ? "password" : "proto"; }
	size_t so = g16(a.b + 64 + 4), sl = g16(a.b + 64 + 6); if (so + sl > a.n) { free(a.b); return "proto"; }
	const unsigned char *sb = a.b + so, *ch = memmem(sb, sl, "NTLMSSP\0", 8); if (!ch) { free(a.b); return "proto"; }
	size_t chn = sl - (size_t)(ch - sb); if (chn < 48 || g32(ch + 8) != 2) { free(a.b); return "proto"; }
	uint32_t sfl = g32(ch + 20); unsigned char schal[8]; memcpy(schal, ch + 24, 8);
	size_t tinl = g16(ch + 40), tino = g32(ch + 44); if (tino + tinl > chn) tinl = 0;
	const unsigned char *tin = ch + tino;
	char tname[128] = ""; { size_t tl = g16(ch + 12), to = g32(ch + 16); if (to + tl <= chn) u16to8(ch + to, tl / 2, tname, sizeof tname); }
	/* the AUTHENTICATE message */
	buf lm = {0}, nt = {0}, ud = {0}, uu = {0}, ws = {0}, ek = {0}; unsigned char ekey[16];
	const char *domain = *dom ? dom : tname;
	if (anon) { p8(&lm, 0); }
	else {
		buf pw = {0}; putu16(&pw, pass, 0); unsigned char nth[16]; md4(pw.b ? pw.b : (unsigned char *)"", pw.n, nth); if (pw.b) explicit_bzero(pw.b, pw.n); free(pw.b);
		buf idn = {0}; putu16(&idn, user, 1); putu16(&idn, domain, 0); unsigned char owf[16]; hmac_md5(nth, 16, idn.b, idn.n, owf); free(idn.b); explicit_bzero(nth, 16);
		uint64_t ts = 0; int havets = 0;
		for (size_t o = 0; o + 4 <= tinl; ) { unsigned id = g16(tin + o), ln = g16(tin + o + 2); if (!id || o + 4 + ln > tinl) break; if (id == 7 && ln == 8) { ts = g64(tin + o + 4); havets = 1; } o += 4 + ln; }
		if (!havets) { struct timeval tv; gettimeofday(&tv, NULL); ts = ((uint64_t)tv.tv_sec + 11644473600ULL) * 10000000ULL + (uint64_t)tv.tv_usec * 10; }
		unsigned char cc[8]; RAND_bytes(cc, 8);
		buf tmp = {0}; p8(&tmp, 1); p8(&tmp, 1); pz(&tmp, 6); p64(&tmp, ts); put(&tmp, cc, 8); pz(&tmp, 4); if (tinl) put(&tmp, tin, tinl); pz(&tmp, 4);
		buf pr = {0}; put(&pr, schal, 8); put(&pr, tmp.b, tmp.n); unsigned char proof[16]; hmac_md5(owf, 16, pr.b, pr.n, proof); free(pr.b);
		put(&nt, proof, 16); put(&nt, tmp.b, tmp.n); free(tmp.b);
		if (havets) pz(&lm, 24);
		else { buf l2 = {0}; put(&l2, schal, 8); put(&l2, cc, 8); unsigned char lr[16]; hmac_md5(owf, 16, l2.b, l2.n, lr); put(&lm, lr, 16); put(&lm, cc, 8); free(l2.b); }
		unsigned char base[16]; hmac_md5(owf, 16, proof, 16, base); explicit_bzero(owf, 16);
		if (sfl & NF_KEYX) { RAND_bytes(ekey, 16); unsigned char enc[16]; rc4(base, 16, ekey, enc, 16); put(&ek, enc, 16); }
		else memcpy(ekey, base, 16);
		explicit_bzero(base, 16);
		putu16(&ud, domain, 0); putu16(&uu, user, 0);
	}
	putu16(&ws, "RUDOLF", 0);
	uint32_t afl = (fl & sfl) | NF_UNICODE | (anon ? NF_ANON : 0); if (anon) afl &= ~NF_KEYX;
	buf m = {0}; put(&m, "NTLMSSP\0", 8); p32(&m, 3); pz(&m, 48); p32(&m, afl); p8(&m, 10); p8(&m, 0); p16(&m, 19045); pz(&m, 3); p8(&m, 15);
	size_t off = m.n;
#define ADD(at, bb) do { sec_field(&m, at, (bb).n, off); off += (bb).n; } while (0)
	ADD(12, lm); ADD(20, nt); ADD(28, ud); ADD(36, uu); ADD(44, ws); ADD(52, ek);
#undef ADD
	put(&m, lm.b, lm.n); put(&m, nt.b, nt.n); put(&m, ud.b, ud.n); put(&m, uu.b, uu.n); put(&m, ws.b, ws.n); put(&m, ek.b, ek.n);
	free(lm.b); free(nt.b); free(ud.b); free(uu.b); free(ws.b); free(ek.b);
	buf t3 = {0}; spnego_resp(&t3, &m); explicit_bzero(m.b, m.n); free(m.b);
	st = session_setup(c, &t3, &a, &sf); free(t3.b); free(a.b);
	if (st == -1) { explicit_bzero(ekey, 16); return "noanswer"; }
	if (st) { explicit_bzero(ekey, 16); return (uint32_t)st == 0xC000006Du || (uint32_t)st == 0xC0000022u || (uint32_t)st == 0xC000015Bu ? "password" : (uint32_t)st == 0xC0000072u || (uint32_t)st == 0xC000006Eu || (uint32_t)st == 0xC0000234u || (uint32_t)st == 0xC0000071u || (uint32_t)st == 0xC0000193u ? "denied" : "proto"; }
	/* a guest's or anonymous session has no key the server knows: never signed (a server that wants signing refuses it) */
	if (!anon && !(sf & 3)) {
		memcpy(c->skey, ekey, 16); c->sign = 1;
		if (c->dialect == 0x0311) { c->cmac = 1; static const unsigned char L[] = "SMBSigningKey"; smb_kdf(c->skey, L, sizeof L, c->preauth, 64, c->sigkey); }
		else if (c->dialect >= 0x0300) { c->cmac = 1; static const unsigned char L[] = "SMB2AESCMAC", C[] = "SmbSign"; smb_kdf(c->skey, L, sizeof L, C, sizeof C, c->sigkey); }
	}
	explicit_bzero(ekey, 16); return NULL;
}

/* ---------------- srvsvc's NetrShareEnum over \PIPE\srvsvc */
static int64_t req(smb *c, unsigned cmd, buf *b, buf *a) { int r = smb_send(c, cmd, b, 0); b->n = 0; return r ? -1 : smb_recv(c, a, NULL); }
static const char *share_enum(smb *c, const char *ip, buf *names)
{
	buf b = {0}, a = {0}; int64_t st;
	/* the IPC$ tree */
	buf path = {0}; char pth[200]; snprintf(pth, sizeof pth, "\\\\%s\\IPC$", ip); putu16(&path, pth, 0);
	p16(&b, 9); p16(&b, 0); p16(&b, 64 + 8); p16(&b, path.n); put(&b, path.b, path.n); free(path.b);
	st = req(c, 3, &b, &a); if (st) { free(a.b); free(b.b); return st == -1 ? "noanswer" : (uint32_t)st == 0xC0000022u ? "denied" : "proto"; }
	c->tid = g32(a.b + 36);
	/* the pipe */
	buf nm = {0}; putu16(&nm, "srvsvc", 0);
	p16(&b, 57); p8(&b, 0); p8(&b, 0); p32(&b, 2); p64(&b, 0); p64(&b, 0); p32(&b, 0x0012019F); p32(&b, 0); p32(&b, 7); p32(&b, 1); p32(&b, 0);
	p16(&b, 64 + 56); p16(&b, nm.n); p32(&b, 0); p32(&b, 0); put(&b, nm.b, nm.n); free(nm.b);
	st = req(c, 5, &b, &a); if (st || a.n < 64 + 80) { free(a.b); free(b.b); return st == -1 ? "noanswer" : (uint32_t)st == 0xC0000022u ? "denied" : "proto"; }
	unsigned char fid[16]; memcpy(fid, a.b + 64 + 64, 16);
	/* DCE/RPC: bind to srvsvc 3.0 with NDR, then NetrShareEnum (opnum 15), level 1 */
	static const unsigned char SRVSVC[16] = { 0xc8, 0x4f, 0x32, 0x4b, 0x70, 0x16, 0xd3, 0x01, 0x12, 0x78, 0x5a, 0x47, 0xbf, 0x6e, 0xe1, 0x88 };
	static const unsigned char NDR[16] = { 0x04, 0x5d, 0x88, 0x8a, 0xeb, 0x1c, 0xc9, 0x11, 0x9f, 0xe8, 0x08, 0x00, 0x2b, 0x10, 0x48, 0x60 };
	for (int step = 0; step < 2; step++) {
		buf pdu = {0};
		p8(&pdu, 5); p8(&pdu, 0); p8(&pdu, step ? 0 : 11); p8(&pdu, 3); p32(&pdu, 0x10); p16(&pdu, 0); p16(&pdu, 0); p32(&pdu, step + 1);
		if (!step) { p16(&pdu, 4280); p16(&pdu, 4280); p32(&pdu, 0); p8(&pdu, 1); pz(&pdu, 3); p16(&pdu, 0); p8(&pdu, 1); p8(&pdu, 0); put(&pdu, SRVSVC, 16); p16(&pdu, 3); p16(&pdu, 0); put(&pdu, NDR, 16); p32(&pdu, 2); }
		else {
			buf st2 = {0}; char srv[200]; snprintf(srv, sizeof srv, "\\\\%s", ip); buf w = {0}; putu16(&w, srv, 0); p16(&w, 0); uint32_t nch = (uint32_t)(w.n / 2);
			p32(&st2, 0x00020000); p32(&st2, nch); p32(&st2, 0); p32(&st2, nch); put(&st2, w.b, w.n); palign(&st2, 4, 0); free(w.b);
			p32(&st2, 1); p32(&st2, 1); p32(&st2, 0x00020004); p32(&st2, 0); p32(&st2, 0); p32(&st2, 0xFFFFFFFF); p32(&st2, 0x00020008); p32(&st2, 0);
			p32(&pdu, (uint32_t)st2.n); p16(&pdu, 0); p16(&pdu, 15); put(&pdu, st2.b, st2.n); free(st2.b);
		}
		s16(pdu.b + 8, pdu.n);
		/* WRITE */
		p16(&b, 49); p16(&b, 64 + 48); p32(&b, pdu.n); p64(&b, 0); put(&b, fid, 16); p32(&b, 0); p32(&b, 0); p16(&b, 0); p16(&b, 0); p32(&b, 0); put(&b, pdu.b, pdu.n); free(pdu.b);
		st = req(c, 9, &b, &a); if (st) { free(a.b); free(b.b); return st == -1 ? "noanswer" : "proto"; }
		/* READ, fragment by fragment, the stub data put together */
		buf stub = {0}; int last = 0, guard = 0; buf frag = {0};
		while (!last && guard++ < 256) {
			p16(&b, 49); p8(&b, 0x50); p8(&b, 0); p32(&b, 65536); p64(&b, 0); put(&b, fid, 16); p32(&b, 0); p32(&b, 0); p32(&b, 0); p16(&b, 0); p16(&b, 0); p8(&b, 0);
			st = req(c, 8, &b, &a);
			if (st && (uint32_t)st != ST_OVERFLOW) { free(a.b); free(b.b); free(stub.b); free(frag.b); return st == -1 ? "noanswer" : "proto"; }
			if (a.n < 64 + 16) { free(a.b); free(b.b); free(stub.b); free(frag.b); return "proto"; }
			size_t dofs = a.b[64 + 2]; uint32_t dl = g32(a.b + 64 + 4); if (dofs + dl > a.n) { free(a.b); free(b.b); free(stub.b); free(frag.b); return "proto"; }
			put(&frag, a.b + dofs, dl);
			/* whole fragments out of what came */
			while (frag.n >= 16) {
				size_t fl = g16(frag.b + 8); if (fl < 16 || frag.n < fl) break;
				unsigned ty = frag.b[2], pf = frag.b[3];
				if (ty == 13 || ty == 3) { free(a.b); free(b.b); free(stub.b); free(frag.b); return "denied"; }   /* bind_nak, fault */
				if (step == 0) { if (ty != 12) { free(a.b); free(b.b); free(stub.b); free(frag.b); return "proto"; } last = 1; }
				else { if (ty != 2 || fl < 24) { free(a.b); free(b.b); free(stub.b); free(frag.b); return "proto"; } put(&stub, frag.b + 24, fl - 24 - g16(frag.b + 10)); if (pf & 2) last = 1; }
				memmove(frag.b, frag.b + fl, frag.n - fl); frag.n -= fl;
			}
		}
		free(frag.b);
		if (step == 1) {
			/* level, switch, the container's pointer, EntriesRead, the array's pointer, its count, the entries, their strings */
			const unsigned char *s = stub.b; size_t n = stub.n, o = 0; const char *why = NULL;
#define NEED(k) do { if (o + (k) > n) { why = "proto"; goto done; } } while (0)
			NEED(20); uint32_t lvl = g32(s); o = 12; uint32_t cnt = g32(s + o); o += 4; uint32_t ap = g32(s + o); o += 4;
			if (lvl != 1) { why = "proto"; goto done; }
			if (ap && cnt) {
				NEED(4); uint32_t mc = g32(s + o); o += 4; if (mc < cnt || cnt > 4096) { why = "proto"; goto done; }
				uint32_t *ty = calloc(cnt, 4), *pn = calloc(cnt, 4), *pr = calloc(cnt, 4); if (!ty || !pn || !pr) exit(3);
				NEED((size_t)cnt * 12); for (uint32_t i = 0; i < cnt; i++) { pn[i] = g32(s + o); ty[i] = g32(s + o + 4); pr[i] = g32(s + o + 8); o += 12; }
				for (uint32_t i = 0; i < cnt; i++) {
					char nmv[300] = "", rmk[300] = "";
					for (int k = 0; k < 2; k++) {
						if (!(k ? pr[i] : pn[i])) continue;
						o = (o + 3) & ~(size_t)3; if (o + 12 > n) { why = "proto"; break; }
						uint32_t ac = g32(s + o + 8); o += 12; if (ac > 4096 || o + (size_t)ac * 2 > n) { why = "proto"; break; }
						u16to8(s + o, ac, k ? rmk : nmv, 300); o += (size_t)ac * 2;
					}
					if (why) break;
					/* folders only (STYPE_DISKTREE), not the administrative and hidden ones the server keeps (C$, ADMIN$, IPC$) */
					if ((ty[i] & 0x0FFFFFFF) == 0 && !(ty[i] & 0x80000000) && *nmv && nmv[strlen(nmv) - 1] != '$') {
						if (names->n) put(names, ",", 1);
						put(names, "{\"name\":", 8); bjs(names, nmv); put(names, ",\"remark\":", 10); bjs(names, rmk); put(names, "}", 1);
					}
				}
				free(ty); free(pn); free(pr);
				if (why) goto done;
			}
		done:
#undef NEED
			free(stub.b); free(a.b); free(b.b); return why;
		}
		free(stub.b);
	}
	free(a.b); free(b.b); return "proto";
}

static int port_of(void) { const char *p = getenv("RS_PORT"); int v = p ? atoi(p) : 445; return v > 0 && v < 65536 ? v : 445; }
static int ip_ok(const char *s) { unsigned char b[16]; return s && (inet_pton(AF_INET, s, b) == 1 || inet_pton(AF_INET6, s, b) == 1); }
static int cmd_probe(const char *ip, int v21)
{
	if (!ip_ok(ip)) return fail("address");
	smb c = {0}; c.fd = tcp_to(ip, port_of(), 4000); if (c.fd < 0) return fail("noanswer");
	const char *why = smb_negotiate(&c, v21); close(c.fd);
	if (why) { if (!strcmp(why, "proto") || !strcmp(why, "noanswer")) why = smb_older(ip, port_of(), v21); return fail(why); }
	printf("{\"ok\":true,\"dialect\":\"%s\",\"encryption\":%s,\"signing\":\"%s\"}\n", dialect_name(c.dialect), c.caps & 0x40 ? "true" : "false", c.secmode & 2 ? "required" : "enabled");
	return 0;
}
static void chomp(char *s) { s[strcspn(s, "\r\n")] = 0; }
static int cmd_list(const char *server, const char *ip, int v21)
{
	(void)server;
	if (!ip_ok(ip)) return fail("address");
	char user[260] = "", pass[520] = "", dom[260] = "";
	if (!fgets(user, sizeof user, stdin)) user[0] = 0; if (!fgets(pass, sizeof pass, stdin)) pass[0] = 0; if (!fgets(dom, sizeof dom, stdin)) dom[0] = 0;
	chomp(user); chomp(pass); chomp(dom);
	const char *why = NULL; buf names = {0}; smb c;
	/* a guest: an anonymous session first, then the user "guest" without a password (servers that map unknown users to guest) */
	for (int round = 0; round < (*user ? 1 : 2); round++) {
		memset(&c, 0, sizeof c); c.fd = tcp_to(ip, port_of(), 4000); if (c.fd < 0) { why = "noanswer"; break; }
		why = smb_negotiate(&c, v21);
		if (why) { close(c.fd); if (!strcmp(why, "proto") || !strcmp(why, "noanswer")) why = smb_older(ip, port_of(), v21); break; }
		why = smb_login(&c, round ? "guest" : user, round ? "" : pass, dom);
		if (!why) why = share_enum(&c, ip, &names);
		close(c.fd);
		if (!why || (strcmp(why, "password") && strcmp(why, "denied"))) break;
		names.n = 0;
	}
	explicit_bzero(pass, sizeof pass);
	if (why) { free(names.b); return fail(why); }
	printf("{\"ok\":true,\"dialect\":\"%s\",\"shares\":[", dialect_name(c.dialect)); fwrite(names.b ? (char *)names.b : "", 1, names.n, stdout); printf("]}\n");
	free(names.b); return 0;
}

/* ---------------- multicast DNS (as rudolf-print asks: RFC 6762, from port 5353, the group joined on each interface) */
static int MIF[16], NMIF, MCAST; static struct in_addr OWN[32]; static int NOWN;
static int mif_has(int ix) { for (int i = 0; i < NMIF; i++) if (MIF[i] == ix) return 1; return 0; }
static int is_cellular(const char *ifn)
{
	const char *p = getenv("RS_CELLULAR"); FILE *f = fopen(p ? p : "/run/rudolf/firewall-cellular", "r"); char l[64]; int r = 0;
	if (!f) return 0;
	while (!r && fgets(l, sizeof l, f)) { chomp(l); if (!strcmp(l, ifn)) r = 1; }
	fclose(f); return r;
}
/* the interfaces to ask on (up, multicast, not loopback, point-to-point or cellular); with group: joined to it */
static void ifs_load(int fd, const char *group)
{
	struct ifaddrs *ifs = NULL; if (getifaddrs(&ifs)) return;
	for (struct ifaddrs *a = ifs; a; a = a->ifa_next) {
		if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET) continue;
		if (NOWN < 32) OWN[NOWN++] = ((struct sockaddr_in *)a->ifa_addr)->sin_addr;
		unsigned fl = a->ifa_flags; if (!(fl & IFF_UP) || !(fl & IFF_MULTICAST) || (fl & (IFF_LOOPBACK | IFF_POINTOPOINT))) continue;
		int ix = if_nametoindex(a->ifa_name); if (!ix || mif_has(ix) || NMIF >= 16 || is_cellular(a->ifa_name)) continue;
		if (group) { struct ip_mreqn mr = {0}; inet_pton(AF_INET, group, &mr.imr_multiaddr); mr.imr_ifindex = ix; if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mr, sizeof mr)) continue; }
		MIF[NMIF++] = ix;
	}
	freeifaddrs(ifs);
}
static int own(struct in_addr a) { for (int i = 0; i < NOWN; i++) if (OWN[i].s_addr == a.s_addr) return 1; return 0; }
static int private4(struct in_addr a)
{
	uint32_t v = ntohl(a.s_addr);
	return (v >> 24) == 10 || (v >> 20) == (172 << 4 | 1) || (v >> 16) == (192 << 8 | 168) || (v >> 16) == (169 << 8 | 254);
}
static void sendall(int fd, const void *m, size_t n, struct sockaddr_in *to)
{
	if (MCAST && NMIF) for (int i = 0; i < NMIF; i++) { struct ip_mreqn mr = {0}; mr.imr_ifindex = MIF[i]; setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &mr, sizeof mr); if (sendto(fd, m, n, 0, (struct sockaddr *)to, sizeof *to) < 0) {} }
	else if (sendto(fd, m, n, 0, (struct sockaddr *)to, sizeof *to) < 0) {}
}
static int udp_from(int port, const char *group, struct sockaddr_in *to, const char *env, const char *gaddr, int gport)
{
	int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0); if (fd < 0) return -1;
	to->sin_family = AF_INET; to->sin_port = htons(gport); inet_pton(AF_INET, gaddr, &to->sin_addr);
	const char *ov = getenv(env);
	if (ov) { char a[64]; snprintf(a, sizeof a, "%s", ov); char *c = strrchr(a, ':'); if (c) { *c = 0; to->sin_port = htons(atoi(c + 1)); } inet_pton(AF_INET, a, &to->sin_addr); return fd; }
	int one = 1, zero = 0; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
	setsockopt(fd, IPPROTO_IP, IP_MULTICAST_ALL, &zero, sizeof zero); setsockopt(fd, IPPROTO_IP, IP_PKTINFO, &one, sizeof one);
	unsigned char lp = 0, ttl = group ? 255 : 1; setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &lp, 1); setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, 1);
	struct sockaddr_in me = {0}; me.sin_family = AF_INET; me.sin_port = htons(port); me.sin_addr.s_addr = htonl(INADDR_ANY);
	if (!bind(fd, (struct sockaddr *)&me, sizeof me)) { MCAST = 1; ifs_load(fd, group); }
	return fd;
}
/* one datagram within ms (0: none), from a computer on an interface asked on, not this one */
static ssize_t udp_recv(int fd, unsigned char *m, size_t mn, struct sockaddr_in *from, int ms, int sport)
{
	struct pollfd p = { fd, POLLIN, 0 }; if (ms <= 0 || poll(&p, 1, ms) <= 0) return 0;
	char cb[256]; struct iovec io = { m, mn }; struct msghdr mh = { from, sizeof *from, &io, 1, cb, sizeof cb, 0 };
	ssize_t n = recvmsg(fd, &mh, 0); if (n <= 0) return -1;
	if (MCAST) {
		int ix = 0; for (struct cmsghdr *c = CMSG_FIRSTHDR(&mh); c; c = CMSG_NXTHDR(&mh, c)) if (c->cmsg_level == IPPROTO_IP && c->cmsg_type == IP_PKTINFO) ix = ((struct in_pktinfo *)CMSG_DATA(c))->ipi_ifindex;
		if (ntohs(from->sin_port) != sport || !mif_has(ix) || own(from->sin_addr) || !private4(from->sin_addr)) return -1;
	}
	return n;
}
static long ms_left(struct timeval *t0, int ms) { struct timeval t; gettimeofday(&t, NULL); return ms - ((t.tv_sec - t0->tv_sec) * 1000 + (t.tv_usec - t0->tv_usec) / 1000); }
static int rdname(const unsigned char *m, size_t n, size_t *i, char *out, size_t on)
{
	size_t o = 0, at = *i; int jumped = 0, hops = 0; out[0] = 0;
	while (at < n) {
		int l = m[at];
		if (l == 0) { at++; break; }
		if ((l & 0xC0) == 0xC0) { if (at + 1 >= n || ++hops > 20) return -1; size_t to = (l & 0x3F) << 8 | m[at + 1]; if (!jumped) *i = at + 2; jumped = 1; at = to; continue; }
		at++; if (at + l > n) return -1;
		if (o && o < on - 1) out[o++] = '.';
		for (int k = 0; k < l && o < on - 1; k++) out[o++] = m[at + k] == '.' ? '\x1f' : m[at + k];
		out[o] = 0; at += l;
	}
	if (!jumped) *i = at;
	return 0;
}
static void qname(buf *x, const char *name)
{
	const char *p = name;
	while (*p) { const char *d = strchr(p, '.'); size_t l = d ? (size_t)(d - p) : strlen(p); if (l > 63) l = 63; p8(x, l); for (size_t k = 0; k < l; k++) p8(x, p[k] == '\x1f' ? '.' : p[k]); p += d ? (size_t)(d - p) : strlen(p); if (*p == '.') p++; }
	p8(x, 0);
}
static void mdns_ask(int fd, struct sockaddr_in *to, const char **names, const int *types, int k)
{
	buf q = {0}; unsigned char h[12] = {0}; h[5] = k; put(&q, h, 12);
	for (int i = 0; i < k; i++) { qname(&q, names[i]); p8(&q, types[i] >> 8); p8(&q, types[i]); p8(&q, 0); p8(&q, 1); }
	sendall(fd, q.b, q.n, to); free(q.b);
}
typedef struct { char inst[256], host[256], ip[64], name[128], how[8], model[128]; int srv; } srvr;
static srvr SV[64]; static int NSV;
typedef struct { char name[256], ip[64]; } arec; static arec AR[64]; static int NAR;
static srvr *sv_inst(const char *inst) { for (int i = 0; i < NSV; i++) if (!strcasecmp(SV[i].inst, inst)) return &SV[i]; if (NSV >= 64) return NULL; srvr *s = &SV[NSV++]; memset(s, 0, sizeof *s); snprintf(s->inst, sizeof s->inst, "%s", inst); snprintf(s->how, sizeof s->how, "mdns"); return s; }
static void mdns_parse(const unsigned char *m, size_t n)
{
	if (n < 12 || !(m[2] & 0x80)) return;
	int qd = m[4] << 8 | m[5], rr = (m[6] << 8 | m[7]) + (m[8] << 8 | m[9]) + (m[10] << 8 | m[11]); size_t i = 12; char nm[256], t[256];
	for (int k = 0; k < qd; k++) { if (rdname(m, n, &i, nm, sizeof nm)) return; i += 4; }
	for (int k = 0; k < rr; k++) {
		if (rdname(m, n, &i, nm, sizeof nm) || i + 10 > n) return;
		int type = m[i] << 8 | m[i + 1]; size_t len = m[i + 8] << 8 | m[i + 9]; i += 10; if (i + len > n) return; size_t d = i;
		if (type == 12 && !strcasecmp(nm, "_smb._tcp.local")) { size_t j = d; if (!rdname(m, n, &j, t, sizeof t)) sv_inst(t); }
		else if (type == 33 && len > 6 && strcasestr(nm, "._smb._tcp.local")) { srvr *s = sv_inst(nm); size_t j = d + 6; if (s && !rdname(m, n, &j, t, sizeof t)) { snprintf(s->host, sizeof s->host, "%s", t); s->srv = 1; } }
		else if (type == 1 && len == 4 && NAR < 64) { snprintf(AR[NAR].name, sizeof AR[NAR].name, "%s", nm); inet_ntop(AF_INET, m + d, AR[NAR].ip, sizeof AR[NAR].ip); NAR++; }
		i += len;
	}
}
static void mdns_listen(int fd, int ms)
{
	struct timeval t0; gettimeofday(&t0, NULL); long l;
	while ((l = ms_left(&t0, ms)) > 0) { unsigned char m[9000]; struct sockaddr_in f = {0}; ssize_t n = udp_recv(fd, m, sizeof m, &f, (int)l, 5353); if (n == 0) break; if (n > 0) mdns_parse(m, n); }
}
/* NAME (or NAME.local) by multicast DNS: its IPv4 address into ip */
static int mdns_resolve(const char *name, char *ip, size_t in)
{
	char q[300]; size_t L = strlen(name);
	if (L > 6 && !strcasecmp(name + L - 6, ".local")) snprintf(q, sizeof q, "%s", name); else snprintf(q, sizeof q, "%s.local", name);
	struct sockaddr_in to = {0}; int fd = udp_from(5353, "224.0.0.251", &to, "RS_MDNS", "224.0.0.251", 5353); if (fd < 0) return -1;
	const char *nn[1] = { q }; int tt[1] = { 1 };
	for (int r = 0; r < 3; r++) {
		mdns_ask(fd, &to, nn, tt, 1); mdns_listen(fd, 700 + 300 * r);
		for (int i = 0; i < NAR; i++) if (!strcasecmp(AR[i].name, q)) { snprintf(ip, in, "%s", AR[i].ip); close(fd); return 0; }
	}
	close(fd); return -1;
}
static int name_ok(const char *s)
{
	size_t n = s ? strlen(s) : 0; if (!n || n > 253 || s[0] == '.' || s[0] == '-') return 0;
	for (; *s; s++) if (!isalnum((unsigned char)*s) && *s != '.' && *s != '-' && *s != '_') return 0;
	return 1;
}
static int cmd_resolve(const char *name)
{
	char ip[64] = "";
	if (ip_ok(name)) { puts(name); return 0; }
	if (!name_ok(name)) { fprintf(stderr, "not a computer's name\n"); return 2; }
	size_t L = strlen(name); int local = strchr(name, '.') == NULL || (L > 6 && !strcasecmp(name + L - 6, ".local"));
	if (!local || !getenv("RS_MDNS")) {
		struct addrinfo hint = {0}, *res = NULL; hint.ai_family = AF_INET; hint.ai_socktype = SOCK_STREAM;
		if (!getaddrinfo(name, NULL, &hint, &res) && res) { inet_ntop(AF_INET, &((struct sockaddr_in *)res->ai_addr)->sin_addr, ip, sizeof ip); freeaddrinfo(res); puts(ip); return 0; }
	}
	if (local && !mdns_resolve(name, ip, sizeof ip)) { puts(ip); return 0; }
	fprintf(stderr, "%s was not found on the network\n", name); return 1;
}

/* ---------------- WS-Discovery (Windows computers): a Probe for devices, their ProbeMatches, then each one's Get */
static void uuid4(char *o)
{
	unsigned char u[16]; RAND_bytes(u, 16); u[6] = (u[6] & 0x0F) | 0x40; u[8] = (u[8] & 0x3F) | 0x80;
	sprintf(o, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7], u[8], u[9], u[10], u[11], u[12], u[13], u[14], u[15]);
}
/* the text of the first element whose name ends in :TAG (or is TAG), into o */
static int xml_text(const char *x, const char *tag, char *o, size_t on)
{
	o[0] = 0; const char *p = x; size_t tl = strlen(tag);
	while ((p = strchr(p, '<'))) {
		p++; if (*p == '/' || *p == '?' || *p == '!') continue;
		const char *e = p; while (*e && !isspace((unsigned char)*e) && *e != '>' && *e != '/') e++;
		const char *c = memchr(p, ':', e - p); const char *ln = c ? c + 1 : p;
		if ((size_t)(e - ln) == tl && !strncmp(ln, tag, tl)) {
			const char *gt = strchr(e, '>'); if (!gt || gt[-1] == '/') return 0;
			const char *end = strchr(gt + 1, '<'); if (!end) return 0;
			size_t n = end - gt - 1; if (n >= on) n = on - 1; memcpy(o, gt + 1, n); o[n] = 0;
			/* the few entities XML has */
			char *w = o; for (char *r = o; *r; ) { if (*r == '&') { if (!strncmp(r, "&amp;", 5)) { *w++ = '&'; r += 5; continue; } if (!strncmp(r, "&lt;", 4)) { *w++ = '<'; r += 4; continue; } if (!strncmp(r, "&gt;", 4)) { *w++ = '>'; r += 4; continue; } if (!strncmp(r, "&quot;", 6)) { *w++ = '"'; r += 6; continue; } if (!strncmp(r, "&apos;", 6)) { *w++ = '\''; r += 6; continue; } } *w++ = *r++; }
			*w = 0; while (w > o && isspace((unsigned char)w[-1])) *--w = 0;
			return 1;
		}
	}
	return 0;
}
static const char NS_SOAP[] = "xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\"";
/* WS-Transfer Get of the device at http://IP:PORT/PATH (IP: the one that answered the Probe): the computer's name */
static void wsd_get(const char *ip, int port, const char *path, const char *endpoint, char *name, size_t nn, char *wg, size_t wn)
{
	int fd = tcp_to(ip, port, 2000); if (fd < 0) return;
	struct timeval tv = { 3, 0 }; setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
	char id[40]; uuid4(id); char body[1600];
	int bl = snprintf(body, sizeof body, "<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope %s><soap:Header><wsa:To>%s</wsa:To>"
		"<wsa:Action>http://schemas.xmlsoap.org/ws/2004/09/transfer/Get</wsa:Action><wsa:MessageID>urn:uuid:%s</wsa:MessageID>"
		"<wsa:ReplyTo><wsa:Address>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</wsa:Address></wsa:ReplyTo></soap:Header><soap:Body/></soap:Envelope>", NS_SOAP, endpoint, id);
	char rq[2400]; int rl = snprintf(rq, sizeof rq, "POST /%s HTTP/1.1\r\nHost: %s:%d\r\nContent-Type: application/soap+xml\r\nUser-Agent: wsd\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s", path, ip, port, bl, body);
	if (rl > 0 && rl < (int)sizeof rq && !full(fd, rq, rl, 1)) {
		static char r[65536]; size_t n = 0; ssize_t k;
		while (n < sizeof r - 1 && (k = recv(fd, r + n, sizeof r - 1 - n, 0)) > 0) n += k;
		r[n] = 0; char *b = strstr(r, "\r\n\r\n");
		char comp[300];
		if (b && xml_text(b, "Computer", comp, sizeof comp)) {
			char *sl = strchr(comp, '/'); if (sl) { *sl++ = 0; char *c = strchr(sl, ':'); if (c) snprintf(wg, wn, "%s", c + 1); }
			snprintf(name, nn, "%s", comp);
		} else if (b) xml_text(b, "FriendlyName", name, nn);
	}
	close(fd);
}
static void wsd_find(int ms)
{
	struct sockaddr_in to = {0}; int fd = udp_from(3702, NULL, &to, "RS_WSD", "239.255.255.250", 3702); if (fd < 0) return;
	char id[40]; uuid4(id); char m[1400];
	int n = snprintf(m, sizeof m, "<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope %s xmlns:wsd=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" xmlns:wsdp=\"http://schemas.xmlsoap.org/ws/2006/02/devprof\">"
		"<soap:Header><wsa:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</wsa:To><wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</wsa:Action>"
		"<wsa:MessageID>urn:uuid:%s</wsa:MessageID></soap:Header><soap:Body><wsd:Probe><wsd:Types>wsdp:Device</wsd:Types></wsd:Probe></soap:Body></soap:Envelope>", NS_SOAP, id);
	sendall(fd, m, n, &to);
	struct timeval t0; gettimeofday(&t0, NULL); long l; int again = 0;
	struct { char ip[64], ep[200], path[200]; int port; } F[32]; int nf = 0;
	while ((l = ms_left(&t0, ms)) > 0) {
		if (!again && l < ms / 2) { sendall(fd, m, n, &to); again = 1; }
		static unsigned char r[65536]; struct sockaddr_in f = {0}; ssize_t k = udp_recv(fd, r, sizeof r - 1, &f, (int)(l > ms / 2 && !again ? l - ms / 2 : l), 3702);
		if (k == 0 && again) break; if (k <= 0) continue; r[k] = 0;
		char rel[100], types[400], xa[400], ep[200];
		/* an answer to this Probe, from a computer (pub:Computer), whose address is its own */
		if (!xml_text((char *)r, "RelatesTo", rel, sizeof rel) || !strstr(rel, id)) continue;
		if (!xml_text((char *)r, "Types", types, sizeof types) || !strstr(types, "Computer")) continue;
		xml_text((char *)r, "Address", ep, sizeof ep); xml_text((char *)r, "XAddrs", xa, sizeof xa);
		char ip[64]; inet_ntop(AF_INET, &f.sin_addr, ip, sizeof ip);
		int dup = 0; for (int i = 0; i < nf; i++) if (!strcmp(F[i].ip, ip)) dup = 1; if (dup || nf >= 32) continue;
		snprintf(F[nf].ip, sizeof F[nf].ip, "%s", ip); snprintf(F[nf].ep, sizeof F[nf].ep, "%s", ep); F[nf].port = 0; F[nf].path[0] = 0;
		/* the XAddrs on this computer's own address only (never another place it names) */
		for (char *x = strtok(xa, " "); x; x = strtok(NULL, " ")) {
			char h[100]; int pt = 80; char pa[200] = "";
			if (sscanf(x, "http://%99[^:/]:%d/%199s", h, &pt, pa) >= 2 && !strcmp(h, ip) && pt > 0 && pt < 65536) { F[nf].port = pt; snprintf(F[nf].path, sizeof F[nf].path, "%s", pa); break; }
		}
		nf++;
	}
	/* those that gave no address for the Get (Windows, wsdd): a Resolve of their endpoint, its ResolveMatch's XAddrs */
	for (int i = 0; i < nf; i++) {
		if (F[i].port || !*F[i].ep) continue;
		char rid[40]; uuid4(rid); char rm[1400];
		int rn = snprintf(rm, sizeof rm, "<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope %s xmlns:wsd=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\">"
			"<soap:Header><wsa:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</wsa:To><wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Resolve</wsa:Action>"
			"<wsa:MessageID>urn:uuid:%s</wsa:MessageID></soap:Header><soap:Body><wsd:Resolve><wsa:EndpointReference><wsa:Address>%s</wsa:Address></wsa:EndpointReference></wsd:Resolve></soap:Body></soap:Envelope>", NS_SOAP, rid, F[i].ep);
		if (rn <= 0 || rn >= (int)sizeof rm) continue;
		sendall(fd, rm, rn, &to); struct timeval t1; gettimeofday(&t1, NULL);
		while ((l = ms_left(&t1, 900)) > 0) {
			static unsigned char r[65536]; struct sockaddr_in f = {0}; ssize_t k = udp_recv(fd, r, sizeof r - 1, &f, (int)l, 3702);
			if (k == 0) break; if (k < 0) continue; r[k] = 0;
			char rel[100], xa[400], ip[64]; inet_ntop(AF_INET, &f.sin_addr, ip, sizeof ip);
			if (strcmp(ip, F[i].ip) || !xml_text((char *)r, "RelatesTo", rel, sizeof rel) || !strstr(rel, rid) || !xml_text((char *)r, "XAddrs", xa, sizeof xa)) continue;
			for (char *x = strtok(xa, " "); x; x = strtok(NULL, " ")) {
				char h[100]; int pt = 80; char pa[200] = "";
				if (sscanf(x, "http://%99[^:/]:%d/%199s", h, &pt, pa) >= 2 && !strcmp(h, ip) && pt > 0 && pt < 65536) { F[i].port = pt; snprintf(F[i].path, sizeof F[i].path, "%s", pa); break; }
			}
			if (F[i].port) break;
		}
	}
	close(fd);
	for (int i = 0; i < nf && NSV < 64; i++) {
		srvr *s = &SV[NSV++]; memset(s, 0, sizeof *s); snprintf(s->ip, sizeof s->ip, "%s", F[i].ip); snprintf(s->how, sizeof s->how, "wsd"); s->srv = 1;
		char wg[100] = "";
		if (F[i].port) wsd_get(F[i].ip, F[i].port, F[i].path, F[i].ep, s->name, sizeof s->name, wg, sizeof wg);
		for (char *c = s->name; *c; c++) if ((unsigned char)*c < 32) *c = ' ';
		if (*wg) snprintf(s->model, sizeof s->model, "%s", wg);
	}
}
static int cmd_find(void)
{
	/* multicast DNS first (its questions, then what the first answers left out), then WS-Discovery */
	struct sockaddr_in to = {0}; int fd = udp_from(5353, "224.0.0.251", &to, "RS_MDNS", "224.0.0.251", 5353);
	if (fd >= 0) {
		const char *n1[] = { "_smb._tcp.local" }; int t1[] = { 12 };
		mdns_ask(fd, &to, n1, t1, 1); mdns_listen(fd, 1000); mdns_ask(fd, &to, n1, t1, 1); mdns_listen(fd, 700);
		for (int round = 0; round < 2; round++) {
			const char *nn[32]; int tt[32], k = 0;
			for (int i = 0; i < NSV && k < 30; i++) if (!SV[i].srv) { nn[k] = SV[i].inst; tt[k++] = 33; }
			for (int i = 0; i < NSV && k < 31; i++) if (SV[i].srv) { int have = 0; for (int j = 0; j < NAR; j++) if (!strcasecmp(AR[j].name, SV[i].host)) have = 1; if (!have) { nn[k] = SV[i].host; tt[k++] = 1; } }
			if (!k) break; mdns_ask(fd, &to, nn, tt, k); mdns_listen(fd, 700);
		}
		close(fd);
		for (int i = 0; i < NSV; i++) {
			srvr *s = &SV[i]; for (int j = 0; j < NAR; j++) if (!strcasecmp(AR[j].name, s->host)) snprintf(s->ip, sizeof s->ip, "%s", AR[j].ip);
			snprintf(s->name, sizeof s->name, "%s", s->inst); char *d = strchr(s->name, '.'); if (d) *d = 0; for (char *c = s->name; *c; c++) if (*c == '\x1f') *c = '.';
		}
	}
	NMIF = 0; MCAST = 0; NOWN = 0;
	int nm = NSV; wsd_find(2200);
	printf("{\"ok\":true,\"servers\":["); int first = 1;
	for (int i = 0; i < NSV; i++) {
		srvr *s = &SV[i]; if (!*s->ip) continue;
		int dup = 0; for (int j = 0; j < i; j++) if (!strcmp(SV[j].ip, s->ip) && *SV[j].ip) dup = 1; if (dup) continue;   /* (mDNS's name first) */
		char host[256] = ""; if (i < nm) { snprintf(host, sizeof host, "%s", s->host); size_t L = strlen(host); if (L && host[L - 1] == '.') host[L - 1] = 0; }
		if (!*s->name) snprintf(s->name, sizeof s->name, "%s", s->ip);
		printf("%s{\"name\":", first ? "" : ","); js(s->name); printf(",\"ip\":"); js(s->ip); printf(",\"host\":"); js(*host ? host : s->name);
		printf(",\"how\":\"%s\",\"group\":", s->how); js(s->model); printf("}"); first = 0;
	}
	printf("]}\n"); return 0;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IOFBF, 1 << 16);
	if (argc >= 3 && !strcmp(argv[1], "resolve")) return cmd_resolve(argv[2]);
	if (argc >= 3 && !strcmp(argv[1], "probe")) return cmd_probe(argv[2], argc > 3 && !strcmp(argv[3], "2.1"));
	if (argc >= 4 && !strcmp(argv[1], "list")) { int r = cmd_list(argv[2], argv[3], argc > 4 && !strcmp(argv[4], "2.1")); fflush(stdout); return r; }
	if (argc >= 2 && !strcmp(argv[1], "find")) { int r = cmd_find(); fflush(stdout); return r; }
	fprintf(stderr, "usage: rudolf-shares resolve NAME | probe IP [2.1] | list SERVER IP [2.1] | find\n"); return 2;
}
