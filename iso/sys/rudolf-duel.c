/* rudolf-duel: the LAN game relay for two-player games from the Rudolf OS Library (Ember Reach's duels), docs/plan-1.6.2.md
 * Part 30. A browser page cannot take connections, so while a player hosts, this computer runs a small WebSocket relay
 * on the local network and announces it there by multicast DNS; the other player's Rudolf OS finds it and the two
 * games meet through it. The protocol is the Library's duel relay's (rudolfos-library tools/duel-relay):
 *   a game says {t:"host",v,proto} (the relay answers {t:"room",code}) or {t:"join",code,v,proto}; paired, both get
 *   {t:"paired",role}, and every message one sends goes to the other as it is; one leaving, the other gets {t:"gone"}.
 *   Refusals: {t:"error",why,text} (version, proto, room, full, bad, size, rate, busy, idle).
 *
 *   rudolf-duel serve --port P --code CODE --v VERSION --game PKG --name NAME --addr A.B.C.D [--wait S]
 *       the relay, for one game of one version, with its room code (rudolfd chooses it): announced as
 *       NAME._rudolf-duel._tcp.local (TXT game=, v=, code=). It stops by itself when the hosting game leaves, when no
 *       game hosts within --wait seconds (60), or when nobody joins in 30 minutes; and on SIGTERM (rudolfd's duel/stop).
 *   rudolf-duel find [--game PKG] [--ms 2500]
 *       the games hosted on the network: {"ok":true,"games":[{"name","url","code","v","game"}]}
 *
 * Started by rudolfd (rudolfd-duel.inc) as the signed-in account's own Linux user, never root (it refuses to run as root),
 * with no new privileges. It takes connections only from loopback and private addresses (the local network); the
 * firewall's duel chain lets in its port from private addresses only, never on a cellular interface, while it runs.
 * Limits: 4 connections, 8 KB a message, 120 a second (240 at once), a minute silent. Text that is not UTF-8 is
 * refused (a browser would drop the other's connection over it). Addresses are never passed on.
 * mDNS: port 5353 shared (SO_REUSEADDR: Find printers may ask at the same time), joined on each interface that is up
 * (not loopback, point-to-point or cellular: /run/rudolf/firewall-cellular, RD_CELLULAR for tests); questions answered
 * by multicast (or to the asker when it asked from another port); the name read with every pointer and length checked
 * (fuzzed by qt/host/duel162_t.sh).
 *
 * Copyright (C) 2026 the Rudolf OS authors. SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define MAXMSG 8192
#define MAXC 4
#define RATE 120.0
#define BURST 240.0
#define IDLE 60.0
#define ROOMLIFE 1800.0
#define SVC "_rudolf-duel._tcp.local"
static const char ALPHA[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }

/* ---------------------------------------------------------------- SHA-1 and base64 (the WebSocket handshake) */
typedef struct { uint32_t h[5]; uint64_t n; unsigned char b[64]; size_t k; } sha1;
#define ROL(x, c) (((x) << (c)) | ((x) >> (32 - (c))))
static void sha1_block(sha1 *s, const unsigned char *p)
{
	uint32_t w[80], a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3], e = s->h[4];
	for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
	for (int i = 16; i < 80; i++) w[i] = ROL(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
	for (int i = 0; i < 80; i++) {
		uint32_t f, k;
		if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; } else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
		else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; } else { f = b ^ c ^ d; k = 0xCA62C1D6; }
		uint32_t t = ROL(a, 5) + f + e + k + w[i]; e = d; d = c; c = ROL(b, 30); b = a; a = t;
	}
	s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e;
}
static void sha1_add(sha1 *s, const void *v, size_t n) { const unsigned char *p = v; s->n += n; while (n--) { s->b[s->k++] = *p++; if (s->k == 64) { sha1_block(s, s->b); s->k = 0; } } }
static void sha1_of(const void *v, size_t n, unsigned char out[20])
{
	sha1 s = {{0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0}, 0, {0}, 0}; uint64_t bits = (uint64_t)n * 8;
	sha1_add(&s, v, n); unsigned char one = 0x80, z = 0; sha1_add(&s, &one, 1); while (s.k != 56) sha1_add(&s, &z, 1);
	unsigned char L[8]; for (int i = 0; i < 8; i++) L[i] = bits >> (56 - 8 * i); sha1_add(&s, L, 8);
	for (int i = 0; i < 5; i++) { out[4 * i] = s.h[i] >> 24; out[4 * i + 1] = s.h[i] >> 16; out[4 * i + 2] = s.h[i] >> 8; out[4 * i + 3] = s.h[i]; }
}
static void b64(const unsigned char *in, size_t n, char *out)
{
	static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; size_t o = 0;
	for (size_t i = 0; i < n; i += 3) {
		uint32_t v = in[i] << 16 | (i + 1 < n ? in[i + 1] << 8 : 0) | (i + 2 < n ? in[i + 2] : 0);
		out[o++] = T[v >> 18 & 63]; out[o++] = T[v >> 12 & 63]; out[o++] = i + 1 < n ? T[v >> 6 & 63] : '='; out[o++] = i + 2 < n ? T[v & 63] : '=';
	}
	out[o] = 0;
}

/* ---------------------------------------------------------------- the few JSON fields the relay reads */
/* a top-level field of a JSON object: 's' a string (unescaped into out, \u escapes as '?'), 'n' a number (as text), 0 none
   or not an object. Nested values are skipped, strings with their escapes */
static int jget(const char *s, size_t n, const char *key, char *out, size_t on)
{
	size_t i = 0; int depth = 0, want = 0; out[0] = 0;
	while (i < n && isspace((unsigned char)s[i])) i++;
	if (i >= n || s[i] != '{') return 0;
	int expect_key = 0;
	for (; i < n; i++) {
		char c = s[i];
		if (c == '"') {
			size_t o = 0; char tmp[256]; int tl = 0; ++i;
			for (; i < n && s[i] != '"'; i++) {
				char ch = s[i];
				if (ch == '\\') { if (++i >= n) return 0; ch = s[i]; if (ch == 'u') { if (i + 4 >= n) return 0; i += 4; ch = '?'; } else if (ch == 'n') ch = '\n'; else if (ch == 't') ch = '\t'; else if (ch == 'r') ch = '\r'; else if (ch == 'b' || ch == 'f') ch = ' '; }
				if (tl < (int)sizeof tmp - 1) tmp[tl++] = ch;
				if (want && o < on - 1) out[o++] = ch;
			}
			if (i >= n) return 0;
			tmp[tl] = 0;
			if (want) { out[o] = 0; return 's'; }
			if (depth == 1 && expect_key) {
				size_t j = i + 1; while (j < n && isspace((unsigned char)s[j])) j++;
				if (j < n && s[j] == ':') { expect_key = 0; if (!strcmp(tmp, key)) { want = 1; i = j; while (i + 1 < n && isspace((unsigned char)s[i + 1])) i++;
					if (i + 1 < n && s[i + 1] != '"') { size_t a = i + 1, b = a; while (b < n && (isdigit((unsigned char)s[b]) || strchr("+-.eE", s[b]))) b++;
						if (b == a) return 0;
						snprintf(out, on, "%.*s", (int)(b - a), s + a); return 'n'; } } }
			}
			continue;
		}
		if (c == '{' || c == '[') { depth++; if (depth == 1) expect_key = 1; }
		else if (c == '}' || c == ']') { if (--depth < 0) return 0; }
		else if (c == ',' && depth == 1) expect_key = 1;
	}
	return 0;
}
/* the text is UTF-8 (well formed: no overlong forms, no surrogates, nothing past U+10FFFF) */
static int utf8_ok(const unsigned char *s, size_t n)
{
	for (size_t i = 0; i < n;) {
		unsigned c = s[i], cp; int k;
		if (c < 0x80) { i++; continue; }
		if (c >= 0xC2 && c <= 0xDF) { k = 1; cp = c & 0x1F; } else if (c >= 0xE0 && c <= 0xEF) { k = 2; cp = c & 0x0F; } else if (c >= 0xF0 && c <= 0xF4) { k = 3; cp = c & 0x07; } else return 0;
		if (i + (size_t)k >= n) return 0;
		for (int j = 1; j <= k; j++) { if ((s[i + j] & 0xC0) != 0x80) return 0; cp = cp << 6 | (s[i + j] & 0x3F); }
		if ((k == 2 && cp < 0x800) || (k == 3 && (cp < 0x10000 || cp > 0x10FFFF)) || (cp >= 0xD800 && cp <= 0xDFFF)) return 0;
		i += k + 1;
	}
	return 1;
}

/* ---------------------------------------------------------------- addresses */
static int private4(uint32_t a)   /* host order */
{
	return (a >> 24) == 127 || (a >> 24) == 10 || (a >> 20) == (172 << 4 | 1) || (a >> 16) == (192 << 8 | 168) || (a >> 16) == (169 << 8 | 254);
}
static int is_cellular(const char *ifn)
{
	const char *p = getenv("RD_CELLULAR"); FILE *f = fopen(p ? p : "/run/rudolf/firewall-cellular", "r"); char l[64]; int r = 0;
	if (!f) return 0;
	while (!r && fgets(l, sizeof l, f)) { l[strcspn(l, "\r\n")] = 0; if (!strcmp(l, ifn)) r = 1; }
	fclose(f); return r;
}

/* ---------------------------------------------------------------- multicast DNS */
typedef struct { unsigned char *b; size_t n, cap; } buf;
static void put(buf *x, const void *v, size_t n) { if (x->n + n > x->cap) { x->cap = (x->n + n) * 2 + 64; x->b = realloc(x->b, x->cap); if (!x->b) exit(1); } memcpy(x->b + x->n, v, n); x->n += n; }
static void put8(buf *x, int v) { unsigned char c = v; put(x, &c, 1); }
static void put16(buf *x, int v) { put8(x, v >> 8); put8(x, v); }
static void put32(buf *x, uint32_t v) { put16(x, v >> 16); put16(x, v & 0xFFFF); }
/* a name: the instance's first label may hold anything but is given whole (first), the rest split at dots */
static void pname(buf *x, const char *first, const char *rest)
{
	if (first) { size_t l = strlen(first); if (l > 63) l = 63; put8(x, l); put(x, first, l); }
	for (const char *p = rest; *p;) { const char *d = strchr(p, '.'); size_t l = d ? (size_t)(d - p) : strlen(p); put8(x, l); put(x, p, l); p += l; if (*p == '.') p++; }
	put8(x, 0);
}
/* a name read from a message: labels joined by dots (a dot inside a label kept as \x1f), every pointer and length
   checked, at most 20 jumps; -1 when it runs past the end or loops */
static int rdname(const unsigned char *m, size_t n, size_t *i, char *out, size_t on)
{
	size_t o = 0, at = *i; int jumped = 0, hops = 0; out[0] = 0;
	for (;;) {
		if (at >= n) return -1;
		int l = m[at];
		if (l == 0) { at++; break; }
		if ((l & 0xC0) == 0xC0) { if (at + 1 >= n || ++hops > 20) return -1; size_t to = (size_t)(l & 0x3F) << 8 | m[at + 1]; if (!jumped) *i = at + 2; jumped = 1; at = to; continue; }
		if (l & 0xC0) return -1;
		at++; if (at + l > n) return -1;
		if (o && o < on - 1) out[o++] = '.';
		for (int k = 0; k < l && o < on - 1; k++) out[o++] = m[at + k] == '.' ? '\x1f' : m[at + k];
		out[o] = 0; at += l;
	}
	if (!jumped) *i = at;
	return 0;
}
/* the service this relay announces */
static struct { char name[64], code[8], v[24], game[48], host[64], inst[160]; struct in_addr addr; int port; } ME;
static int MIF[16], NMIF;
static int mif_has(int ix) { for (int i = 0; i < NMIF; i++) if (MIF[i] == ix) return 1; return 0; }
static void mdns_join(int fd, int drop)
{
	struct ifaddrs *ifs = NULL;
	if (drop) { for (int i = 0; i < NMIF; i++) { struct ip_mreqn mr = {0}; inet_pton(AF_INET, "224.0.0.251", &mr.imr_multiaddr); mr.imr_ifindex = MIF[i]; setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mr, sizeof mr); } NMIF = 0; return; }
	if (getifaddrs(&ifs)) return;
	for (struct ifaddrs *a = ifs; a; a = a->ifa_next) {
		if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET) continue;
		unsigned fl = a->ifa_flags; if (!(fl & IFF_UP) || !(fl & IFF_MULTICAST) || (fl & (IFF_LOOPBACK | IFF_POINTOPOINT))) continue;
		int ix = if_nametoindex(a->ifa_name); if (!ix || mif_has(ix) || NMIF >= 16 || is_cellular(a->ifa_name)) continue;
		struct ip_mreqn mr = {0}; inet_pton(AF_INET, "224.0.0.251", &mr.imr_multiaddr); mr.imr_ifindex = ix;
		if (!setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mr, sizeof mr)) MIF[NMIF++] = ix;
	}
	freeifaddrs(ifs);
}
static int mdns_socket(void)
{
	int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0); if (fd < 0) return -1;
	int one = 1, zero = 0; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
	setsockopt(fd, IPPROTO_IP, IP_MULTICAST_ALL, &zero, sizeof zero); setsockopt(fd, IPPROTO_IP, IP_PKTINFO, &one, sizeof one);
	unsigned char lp = 0, ttl = 255; setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &lp, 1); setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, 1);
	struct sockaddr_in me = {0}; me.sin_family = AF_INET; me.sin_port = htons(5353); me.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(fd, (struct sockaddr *)&me, sizeof me)) { close(fd); return -1; }
	mdns_join(fd, 0);
	return fd;
}
/* the answer: PTR (the service to this instance), SRV (its port and host), TXT (game, version, code), A (the address);
   ttl 0 says goodbye. id and the question for a one-shot asker (another port than 5353) */
static void mdns_answer(buf *x, int ttl, int id, const unsigned char *q, size_t ql)
{
	put16(x, id); put16(x, 0x8400); put16(x, q ? 1 : 0); put16(x, 4); put16(x, 0); put16(x, 0);
	if (q) put(x, q, ql);
	pname(x, NULL, SVC); put16(x, 12); put16(x, 1); put32(x, ttl);
	buf d = {0}; pname(&d, ME.name, SVC); put16(x, d.n); put(x, d.b, d.n); free(d.b);
	pname(x, ME.name, SVC); put16(x, 33); put16(x, 0x8001); put32(x, ttl);
	d = (buf){0}; put16(&d, 0); put16(&d, 0); put16(&d, ME.port); pname(&d, NULL, ME.host); put16(x, d.n); put(x, d.b, d.n); free(d.b);
	pname(x, ME.name, SVC); put16(x, 16); put16(x, 0x8001); put32(x, ttl);
	d = (buf){0}; char kv[96];
	snprintf(kv, sizeof kv, "game=%s", ME.game); put8(&d, strlen(kv)); put(&d, kv, strlen(kv));
	snprintf(kv, sizeof kv, "v=%s", ME.v); put8(&d, strlen(kv)); put(&d, kv, strlen(kv));
	snprintf(kv, sizeof kv, "code=%s", ME.code); put8(&d, strlen(kv)); put(&d, kv, strlen(kv));
	put16(x, d.n); put(x, d.b, d.n); free(d.b);
	pname(x, NULL, ME.host); put16(x, 1); put16(x, 0x8001); put32(x, ttl); put16(x, 4); put(x, &ME.addr, 4);
}
static void mdns_send_all(int fd, int ttl)
{
	buf x = {0}; mdns_answer(&x, ttl, 0, NULL, 0);
	struct sockaddr_in to = {0}; to.sin_family = AF_INET; to.sin_port = htons(5353); inet_pton(AF_INET, "224.0.0.251", &to.sin_addr);
	for (int i = 0; i < NMIF; i++) { struct ip_mreqn mr = {0}; mr.imr_ifindex = MIF[i]; setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &mr, sizeof mr); if (sendto(fd, x.b, x.n, 0, (struct sockaddr *)&to, sizeof to) < 0) {} }
	free(x.b);
}
/* a question for this service, this instance or its host: answered */
static int mdns_wants(const unsigned char *m, size_t n, size_t *qend)
{
	if (n < 12 || (m[2] & 0x80)) return 0;   /* (questions only) */
	int qd = m[4] << 8 | m[5], hit = 0; size_t i = 12; char nm[300];
	if (qd > 16) return 0;
	for (int k = 0; k < qd; k++) {
		if (rdname(m, n, &i, nm, sizeof nm) || i + 4 > n) return 0;
		int ty = m[i] << 8 | m[i + 1]; i += 4;
		if (!strcasecmp(nm, SVC) && (ty == 12 || ty == 255)) hit = 1;
		else if (!strcasecmp(nm, ME.inst) && (ty == 33 || ty == 16 || ty == 255)) hit = 1;
		else if (!strcasecmp(nm, ME.host) && (ty == 1 || ty == 255)) hit = 1;
	}
	*qend = i;
	return hit;
}
static void mdns_reply(int fd)
{
	for (int k = 0; k < 16; k++) {
		unsigned char m[9000]; struct sockaddr_in from = {0}; char cb[256];
		struct iovec io = {m, sizeof m}; struct msghdr mh = {&from, sizeof from, &io, 1, cb, sizeof cb, 0};
		ssize_t n = recvmsg(fd, &mh, 0); if (n < 0) return;
		int ix = 0; for (struct cmsghdr *c = CMSG_FIRSTHDR(&mh); c; c = CMSG_NXTHDR(&mh, c)) if (c->cmsg_level == IPPROTO_IP && c->cmsg_type == IP_PKTINFO) ix = ((struct in_pktinfo *)CMSG_DATA(c))->ipi_ifindex;
		if (!mif_has(ix) || !private4(ntohl(from.sin_addr.s_addr))) continue;
		size_t qe = 0; if (!mdns_wants(m, n, &qe)) continue;
		if (ntohs(from.sin_port) != 5353) {   /* a one-shot asker: answered to it, with its question (RFC 6762 6.7) */
			buf x = {0}; mdns_answer(&x, 10, m[0] << 8 | m[1], m + 12, qe - 12);
			if (sendto(fd, x.b, x.n, 0, (struct sockaddr *)&from, sizeof from) < 0) {} free(x.b);
		} else { struct ip_mreqn mr = {0}; mr.imr_ifindex = ix; setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &mr, sizeof mr);
			buf x = {0}; mdns_answer(&x, 120, 0, NULL, 0); struct sockaddr_in to = {0}; to.sin_family = AF_INET; to.sin_port = htons(5353); inet_pton(AF_INET, "224.0.0.251", &to.sin_addr);
			if (sendto(fd, x.b, x.n, 0, (struct sockaddr *)&to, sizeof to) < 0) {} free(x.b); }
	}
}

/* ---------------------------------------------------------------- finding games (rudolf-duel find) */
typedef struct { char inst[300], host[300], v[24], code[8], game[48]; int port, srv, txt, seen; } found_t;
static found_t FOUND[32]; static int NF;
typedef struct { char name[300]; char ip[20]; } arec_t; static arec_t AR[32]; static int NAR;
static found_t *found_get(const char *inst)
{
	for (int i = 0; i < NF; i++) if (!strcasecmp(FOUND[i].inst, inst)) return &FOUND[i];
	if (NF >= 32) return NULL;
	found_t *f = &FOUND[NF++]; memset(f, 0, sizeof *f); snprintf(f->inst, sizeof f->inst, "%s", inst); return f;
}
static int endswith_svc(const char *nm) { size_t a = strlen(nm), b = strlen("." SVC); return a > b && !strcasecmp(nm + a - b, "." SVC); }
static int word_ok(const char *s, size_t max, const char *more) { size_t n = strlen(s); if (!n || n > max) return 0; for (; *s; s++) if (!isalnum((unsigned char)*s) && !strchr(more, *s)) return 0; return 1; }
static int code_ok(const char *c) { if (strlen(c) != 6) return 0; for (; *c; c++) if (!strchr(ALPHA, *c)) return 0; return 1; }
/* a response: its records read (PTR, SRV, TXT and A of this service only), bounded */
void duel_parse(const unsigned char *m, size_t n)
{
	if (n < 12 || !(m[2] & 0x80)) return;
	int qd = m[4] << 8 | m[5], rr = (m[6] << 8 | m[7]) + (m[8] << 8 | m[9]) + (m[10] << 8 | m[11]); size_t i = 12; char nm[300], t[300];
	if (qd > 16 || rr > 64) return;
	for (int k = 0; k < qd; k++) { if (rdname(m, n, &i, nm, sizeof nm) || i + 4 > n) return; i += 4; }
	for (int k = 0; k < rr; k++) {
		if (rdname(m, n, &i, nm, sizeof nm) || i + 10 > n) return;
		int type = m[i] << 8 | m[i + 1]; size_t len = (size_t)m[i + 8] << 8 | m[i + 9]; i += 10; if (i + len > n) return; size_t d = i;
		if (type == 12 && !strcasecmp(nm, SVC)) { size_t j = d; if (!rdname(m, n, &j, t, sizeof t) && endswith_svc(t)) { found_t *f = found_get(t); if (f) f->seen = 1; } }
		else if (type == 33 && len > 6 && endswith_svc(nm)) { found_t *f = found_get(nm); size_t j = d + 6; if (f && !rdname(m, n, &j, t, sizeof t)) { f->port = m[d + 4] << 8 | m[d + 5]; snprintf(f->host, sizeof f->host, "%s", t); f->srv = 1; } }
		else if (type == 16 && endswith_svc(nm)) {
			found_t *f = found_get(nm); if (f) { f->txt = 1; size_t j = d;
				while (j < d + len) { int l = m[j++]; if (j + l > d + len) break; char kv[300]; snprintf(kv, sizeof kv, "%.*s", l, (const char *)m + j); j += l;
					char *eq = strchr(kv, '='); if (!eq) continue; *eq++ = 0;
					if (!strcasecmp(kv, "v")) snprintf(f->v, sizeof f->v, "%s", eq); else if (!strcasecmp(kv, "code")) snprintf(f->code, sizeof f->code, "%s", eq); else if (!strcasecmp(kv, "game")) snprintf(f->game, sizeof f->game, "%s", eq); } }
		}
		else if (type == 1 && len == 4 && NAR < 32) { snprintf(AR[NAR].name, sizeof AR[NAR].name, "%s", nm); inet_ntop(AF_INET, m + d, AR[NAR].ip, sizeof AR[NAR].ip); NAR++; }
		i += len;
	}
}
static void jstr(const char *s) { putchar('"'); for (; *s; s++) { unsigned char c = *s; if (c == '"' || c == '\\') printf("\\%c", c); else if (c < 0x20) printf("\\u%04x", c); else putchar(c); } putchar('"'); }
#ifndef RD_NO_MAIN
static int cmd_find(const char *game, int ms)
{
	int fd = mdns_socket(); if (fd < 0) { printf("{\"ok\":false,\"error\":\"This computer can't look on the network (multicast DNS).\"}\n"); return 1; }
	struct sockaddr_in to = {0}; to.sin_family = AF_INET; to.sin_port = htons(5353); inet_pton(AF_INET, "224.0.0.251", &to.sin_addr);
	buf q = {0}; put16(&q, 0); put16(&q, 0); put16(&q, 1); put16(&q, 0); put16(&q, 0); put16(&q, 0); pname(&q, NULL, SVC); put16(&q, 12); put16(&q, 1);
	double t0 = now(), next = t0;
	while (now() - t0 < ms / 1000.0) {
		if (now() >= next) { for (int i = 0; i < NMIF; i++) { struct ip_mreqn mr = {0}; mr.imr_ifindex = MIF[i]; setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &mr, sizeof mr); if (sendto(fd, q.b, q.n, 0, (struct sockaddr *)&to, sizeof to) < 0) {} } next += 0.9; }
		struct pollfd p = {fd, POLLIN, 0}; int left = (int)((next - now()) * 1000); if (left < 10) left = 10; if (poll(&p, 1, left) <= 0) continue;
		for (int k = 0; k < 32; k++) {
			unsigned char m[9000]; struct sockaddr_in from = {0}; char cb[256];
			struct iovec io = {m, sizeof m}; struct msghdr mh = {&from, sizeof from, &io, 1, cb, sizeof cb, 0};
			ssize_t n = recvmsg(fd, &mh, 0); if (n < 0) break;
			int ix = 0; for (struct cmsghdr *c = CMSG_FIRSTHDR(&mh); c; c = CMSG_NXTHDR(&mh, c)) if (c->cmsg_level == IPPROTO_IP && c->cmsg_type == IP_PKTINFO) ix = ((struct in_pktinfo *)CMSG_DATA(c))->ipi_ifindex;
			if (ntohs(from.sin_port) != 5353 || !mif_has(ix) || !private4(ntohl(from.sin_addr.s_addr))) continue;
			duel_parse(m, n);
		}
	}
	free(q.b); mdns_join(fd, 1); close(fd);
	printf("{\"ok\":true,\"games\":["); int first = 1;
	for (int i = 0; i < NF; i++) {
		found_t *f = &FOUND[i]; if (!f->srv || !f->txt || !f->port || !code_ok(f->code) || !word_ok(f->v, 20, ".+-") || (game && *game && strcmp(f->game, game))) continue;
		const char *ip = NULL; for (int j = 0; j < NAR; j++) if (!strcasecmp(AR[j].name, f->host)) ip = AR[j].ip;
		struct in_addr ia; if (!ip || inet_pton(AF_INET, ip, &ia) != 1 || !private4(ntohl(ia.s_addr))) continue;
		char name[80]; size_t L = strlen(f->inst) - strlen("." SVC); snprintf(name, sizeof name, "%.*s", (int)(L < 60 ? L : 60), f->inst);
		for (char *c = name; *c; c++) if (*c == '\x1f') *c = '.'; else if ((unsigned char)*c < 0x20) *c = ' ';
		char url[64]; snprintf(url, sizeof url, "ws://%s:%d/", ip, f->port);
		printf("%s{\"name\":", first ? "" : ","); first = 0; jstr(name); printf(",\"url\":"); jstr(url); printf(",\"code\":"); jstr(f->code); printf(",\"v\":"); jstr(f->v); printf(",\"game\":"); jstr(f->game); printf("}");
	}
	printf("]}\n");
	return 0;
}
#endif

/* ---------------------------------------------------------------- the relay */
typedef struct cl {
	int fd, ws, gone, hosted, role; double tokens, at, heard; char v[40];
	unsigned char in[MAXMSG + 4096 + 16]; size_t inl;
	unsigned char *frag; size_t fragl;
	unsigned char *out; size_t outl;
	struct cl *peer;
} cl;
static cl *C[MAXC];
static int ROOM_OPEN;   /* a game hosts the room, waiting for the other */
static double ROOM_AT;
static volatile sig_atomic_t STOP;
static void on_term(int s) { (void)s; STOP = 1; }
static void cl_write(cl *c, const void *v, size_t n)
{
	if (c->gone) return;
	if (!c->outl) { ssize_t w = send(c->fd, v, n, MSG_NOSIGNAL); if (w == (ssize_t)n) return; if (w < 0) { if (errno != EAGAIN && errno != EWOULDBLOCK) { c->gone = 1; return; } w = 0; } v = (const char *)v + w; n -= w; }
	if (c->outl + n > 256 * 1024) { c->gone = 1; return; }
	c->out = realloc(c->out, c->outl + n); if (!c->out) { c->gone = 1; return; } memcpy(c->out + c->outl, v, n); c->outl += n;
}
static void ws_frame(cl *c, int op, const void *d, size_t n)
{
	unsigned char h[4]; size_t hl;
	if (n < 126) { h[0] = 0x80 | op; h[1] = n; hl = 2; } else { h[0] = 0x80 | op; h[1] = 126; h[2] = n >> 8; h[3] = n; hl = 4; }
	cl_write(c, h, hl); cl_write(c, d, n);
}
static void ws_text(cl *c, const char *t) { ws_frame(c, 1, t, strlen(t)); }
static void refuse(cl *c, const char *why, const char *text)
{
	char m[400]; snprintf(m, sizeof m, "{\"t\":\"error\",\"why\":\"%s\",\"text\":\"%s\"}", why, text);
	if (c->ws) { ws_text(c, m); unsigned char b[2] = {0x03, 0xE8}; ws_frame(c, 8, b, 2); }
	c->gone = 1;
}
static void relay_msg(cl *c, const char *t, size_t n)
{
	double tn = now(); c->tokens += (tn - c->at) * RATE; if (c->tokens > BURST) c->tokens = BURST; c->at = tn; c->heard = tn;
	if (--c->tokens < 0) { refuse(c, "rate", "Too many messages."); return; }
	if (!utf8_ok((const unsigned char *)t, n)) { refuse(c, "bad", "The relay did not understand the game."); return; }
	char ty[24], v[40], code[24], proto[16];
	if (jget(t, n, "t", ty, sizeof ty) != 's') { refuse(c, "bad", "The relay did not understand the game."); return; }
	if (c->peer) {
		if (!strcmp(ty, "room") || !strcmp(ty, "queued") || !strcmp(ty, "paired") || !strcmp(ty, "error") || !strcmp(ty, "gone")) { refuse(c, "bad", "The relay did not understand the game."); return; }
		ws_frame(c->peer, 1, t, n); return;
	}
	if (!strcmp(ty, "ping") && (c->hosted || c->role)) return;
	if (c->hosted) { refuse(c, "bad", "The relay did not understand the game."); return; }
	if (jget(t, n, "proto", proto, sizeof proto) != 'n' || strcmp(proto, "1")) { refuse(c, "proto", "That relay speaks a different duel protocol: update the game."); return; }
	if (jget(t, n, "v", v, sizeof v) != 's' || !word_ok(v, 20, ".+-")) { refuse(c, "bad", "The relay did not understand the game."); return; }
	if (strcmp(v, ME.v)) { char tx[200]; snprintf(tx, sizeof tx, "That game runs version %s, and you have %s. Both need the same version to duel.", ME.v, v); refuse(c, "version", tx); return; }
	snprintf(c->v, sizeof c->v, "%s", v);
	if (!strcmp(ty, "host")) {
		for (int i = 0; i < MAXC; i++) if (C[i] && C[i] != c && C[i]->hosted && !C[i]->gone) { refuse(c, "busy", "A game is already hosted here."); return; }
		for (int i = 0; i < MAXC; i++) if (C[i] && C[i]->role == 1) { refuse(c, "busy", "A game is already hosted here."); return; }
		c->hosted = 1; ROOM_OPEN = 1; ROOM_AT = tn;
		char m[64]; snprintf(m, sizeof m, "{\"t\":\"room\",\"code\":\"%s\"}", ME.code); ws_text(c, m);
	} else if (!strcmp(ty, "join")) {
		jget(t, n, "code", code, sizeof code);
		char k[8]; size_t o = 0; for (char *p = code; *p && o < 6; p++) if (isalnum((unsigned char)*p)) k[o++] = toupper((unsigned char)*p); k[o] = 0;
		cl *h = NULL; for (int i = 0; i < MAXC; i++) if (C[i] && C[i]->hosted && !C[i]->gone && !C[i]->peer) h = C[i];
		if (!h || strcmp(k, ME.code)) { refuse(c, "room", "No game is waiting with that code. Check it with the other pilot."); return; }
		h->hosted = 0; h->role = 1; c->role = 2; h->peer = c; c->peer = h; ROOM_OPEN = 0;
		ws_text(h, "{\"t\":\"paired\",\"role\":\"host\"}"); ws_text(c, "{\"t\":\"paired\",\"role\":\"guest\"}");
	} else refuse(c, "bad", "Games on a home network are joined by their code.");
}
/* the WebSocket handshake: GET / or /duel, Upgrade: websocket, version 13, a key */
static int hdr(const char *h, const char *name, char *out, size_t on)
{
	size_t nl = strlen(name);
	for (const char *p = h; (p = strchr(p, '\n')); ) { p++; if (!strncasecmp(p, name, nl) && p[nl] == ':') { p += nl + 1; while (*p == ' ' || *p == '\t') p++; size_t l = strcspn(p, "\r\n"); if (l >= on) return 0; memcpy(out, p, l); out[l] = 0; return 1; } }
	return 0;
}
static void handshake(cl *c)
{
	char *e = memmem(c->in, c->inl, "\r\n\r\n", 4);
	if (!e) { if (c->inl >= 4096) c->gone = 1; return; }
	size_t hl = (unsigned char *)e - c->in + 4; char h[4200]; memcpy(h, c->in, hl); h[hl] = 0;
	char up[64], key[64], ver[8];
	int path = !strncmp(h, "GET / ", 6) || !strncmp(h, "GET /duel ", 10) || !strncmp(h, "GET /?", 6) || !strncmp(h, "GET /duel?", 10);
	if (!path || !hdr(h, "Upgrade", up, sizeof up) || strcasecmp(up, "websocket") || !hdr(h, "Sec-WebSocket-Key", key, sizeof key) || strlen(key) > 40 || !hdr(h, "Sec-WebSocket-Version", ver, sizeof ver) || strcmp(ver, "13")) {
		const char *r = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"; cl_write(c, r, strlen(r)); c->gone = 1; return;
	}
	char acc[128]; snprintf(acc, sizeof acc, "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key); unsigned char sh[20]; sha1_of(acc, strlen(acc), sh); char b[40]; b64(sh, 20, b);
	char r[300]; snprintf(r, sizeof r, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", b);
	cl_write(c, r, strlen(r)); c->ws = 1;
	memmove(c->in, c->in + hl, c->inl - hl); c->inl -= hl;
}
/* frames: masked (from a browser), text, continuation, ping, pong, close; anything else closes */
static void frames(cl *c)
{
	while (!c->gone && c->inl >= 2) {
		unsigned char *b = c->in; int fin = b[0] & 0x80, op = b[0] & 15, masked = b[1] & 0x80; size_t n = b[1] & 127, o = 2;
		if (b[0] & 0x70 || !masked) { c->gone = 1; return; }
		if (n == 126) { if (c->inl < 4) return; n = (size_t)b[2] << 8 | b[3]; o = 4; } else if (n == 127) { refuse(c, "size", "A message was too large."); return; }
		if (n > MAXMSG) { refuse(c, "size", "A message was too large."); return; }
		if (c->inl < o + 4 + n) return;
		unsigned char *mk = b + o, *d = b + o + 4; for (size_t i = 0; i < n; i++) d[i] ^= mk[i & 3];
		if (op == 8) { unsigned char z[2] = {0x03, 0xE8}; ws_frame(c, 8, z, 2); c->gone = 1; return; }
		else if (op == 9) { if (n <= 125) ws_frame(c, 10, d, n); }
		else if (op == 10) {}
		else if (op == 1 || op == 0) {
			if (op == 1 && c->fragl) { c->gone = 1; return; }
			if (op == 0 && !c->frag) { c->gone = 1; return; }
			if (c->fragl + n > MAXMSG) { refuse(c, "size", "A message was too large."); return; }
			c->frag = realloc(c->frag, c->fragl + n + 1); if (!c->frag) { c->gone = 1; return; } memcpy(c->frag + c->fragl, d, n); c->fragl += n;
			if (fin) { c->frag[c->fragl] = 0; size_t L = c->fragl; char *t = (char *)c->frag; c->frag = NULL; c->fragl = 0; relay_msg(c, t, L); free(t); }
		} else { c->gone = 1; return; }
		size_t used = o + 4 + n; memmove(c->in, c->in + used, c->inl - used); c->inl -= used;
	}
}
static void cl_drop(int i)
{
	cl *c = C[i]; C[i] = NULL;
	if (c->outl) { if (send(c->fd, c->out, c->outl, MSG_NOSIGNAL | MSG_DONTWAIT) < 0) {} }
	if (c->peer) { cl *p = c->peer; p->peer = NULL; ws_text(p, "{\"t\":\"gone\"}"); unsigned char z[2] = {0x03, 0xE8}; ws_frame(p, 8, z, 2); p->gone = 1; }
	if (c->hosted || c->role == 1) STOP = 2;   /* (the hosting game left: the relay's work is done) */
	close(c->fd); free(c->out); free(c->frag); free(c);
}
#ifndef RD_NO_MAIN
static int cmd_serve(int port, int wait)
{
	if (geteuid() == 0 && !getenv("RD_TEST_ROOT")) { fprintf(stderr, "rudolf-duel: never as root (rudolfd starts it as the account's own user)\n"); return 2; }
	prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	signal(SIGPIPE, SIG_IGN); signal(SIGTERM, on_term); signal(SIGINT, on_term); signal(SIGHUP, on_term);
	int ls = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0), one = 1; if (ls < 0) return 1;
	setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
	struct sockaddr_in a = {0}; a.sin_family = AF_INET; a.sin_port = htons(port); a.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(ls, (struct sockaddr *)&a, sizeof a) || listen(ls, 8)) { fprintf(stderr, "rudolf-duel: port %d: %s\n", port, strerror(errno)); return 1; }
	int md = mdns_socket();
	if (md < 0) fprintf(stderr, "rudolf-duel: multicast DNS not available: the game can still be joined by its address\n");
	else mdns_send_all(md, 120);
	printf("{\"ok\":true,\"port\":%d}\n", port); fflush(stdout);
	double start = now(), ann = start + 1, sweep = start + 1; int anns = 1, ever = 0;
	while (!STOP) {
		struct pollfd p[MAXC + 2]; int k = 0, ix[MAXC + 2];
		p[k].fd = ls; p[k].events = POLLIN; ix[k++] = -1;
		if (md >= 0) { p[k].fd = md; p[k].events = POLLIN; ix[k++] = -2; }
		for (int i = 0; i < MAXC; i++) if (C[i]) { p[k].fd = C[i]->fd; p[k].events = POLLIN | (C[i]->outl ? POLLOUT : 0); ix[k++] = i; }
		int r = poll(p, k, 500); if (r < 0 && errno != EINTR) break;
		for (int j = 0; j < k && r > 0; j++) {
			if (!p[j].revents) continue;
			if (ix[j] == -1) {
				for (;;) {
					struct sockaddr_in from; socklen_t fl = sizeof from; int fd = accept4(ls, (struct sockaddr *)&from, &fl, SOCK_CLOEXEC | SOCK_NONBLOCK); if (fd < 0) break;
					int slot = -1; for (int i = 0; i < MAXC; i++) if (!C[i]) { slot = i; break; }
					/* the local network only: loopback and private addresses (the firewall's rule says the same) */
					if (slot < 0 || !private4(ntohl(from.sin_addr.s_addr))) { close(fd); continue; }
					setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
					cl *c = calloc(1, sizeof *c); if (!c) { close(fd); continue; } c->fd = fd; c->tokens = BURST; c->at = c->heard = now(); C[slot] = c; ever = 1;
				}
			} else if (ix[j] == -2) mdns_reply(md);
			else {
				cl *c = C[ix[j]]; if (!c) continue;
				if (p[j].revents & POLLOUT && c->outl) { ssize_t w = send(c->fd, c->out, c->outl, MSG_NOSIGNAL); if (w > 0) { memmove(c->out, c->out + w, c->outl - w); c->outl -= w; } else if (w < 0 && errno != EAGAIN) c->gone = 1; }
				if (p[j].revents & (POLLIN | POLLHUP | POLLERR)) {
					ssize_t n = recv(c->fd, c->in + c->inl, sizeof c->in - c->inl, 0);
					if (n <= 0) { if (n == 0 || (errno != EAGAIN && errno != EINTR)) c->gone = 1; }
					else { c->inl += n; if (!c->ws) handshake(c); if (c->ws) frames(c); if (c->inl >= sizeof c->in) c->gone = 1; }
				}
			}
		}
		double t = now();
		for (int i = 0; i < MAXC; i++) if (C[i] && (C[i]->gone || t - C[i]->heard > IDLE)) { if (!C[i]->gone) refuse(C[i], "idle", "Nothing was heard for a minute."); cl_drop(i); }
		if (md >= 0 && anns < 3 && t >= ann) { mdns_send_all(md, 120); anns++; ann = t + 2; }
		if (t >= sweep) { sweep = t + 1;
			int hosting = 0; for (int i = 0; i < MAXC; i++) if (C[i] && (C[i]->hosted || C[i]->role)) hosting = 1;
			if (!hosting && t - start > wait) STOP = 3;   /* (nobody hosted) */
			if (ROOM_OPEN && t - ROOM_AT > ROOMLIFE) STOP = 4;   /* (nobody joined in half an hour) */
			(void)ever; }
	}
	for (int i = 0; i < MAXC; i++) if (C[i]) { if (C[i]->peer) C[i]->peer = NULL; ws_text(C[i], "{\"t\":\"gone\"}"); cl_drop(i); }
	if (md >= 0) { mdns_send_all(md, 0); mdns_join(md, 1); close(md); }   /* (goodbye: the announcement withdrawn) */
	close(ls);
	return 0;
}
int main(int argc, char **argv)
{
	if (argc < 2) { fprintf(stderr, "usage: rudolf-duel serve --port P --code CODE --v VERSION --game PKG --name NAME --addr A.B.C.D [--wait S] | find [--game PKG] [--ms MS]\n"); return 2; }
	const char *code = "", *v = "", *game = "", *name = "", *addr = ""; int port = 0, wait = 60, ms = 2500;
	for (int i = 2; i + 1 < argc; i += 2) {
		if (!strcmp(argv[i], "--port")) port = atoi(argv[i + 1]); else if (!strcmp(argv[i], "--code")) code = argv[i + 1]; else if (!strcmp(argv[i], "--v")) v = argv[i + 1];
		else if (!strcmp(argv[i], "--game")) game = argv[i + 1]; else if (!strcmp(argv[i], "--name")) name = argv[i + 1]; else if (!strcmp(argv[i], "--addr")) addr = argv[i + 1];
		else if (!strcmp(argv[i], "--wait")) wait = atoi(argv[i + 1]); else if (!strcmp(argv[i], "--ms")) ms = atoi(argv[i + 1]);
		else { fprintf(stderr, "rudolf-duel: unknown %s\n", argv[i]); return 2; }
	}
	if (!strcmp(argv[1], "find")) { if (*game && !word_ok(game, 40, "-")) return 2; return cmd_find(game, ms < 300 ? 300 : ms > 8000 ? 8000 : ms); }
	if (strcmp(argv[1], "serve")) { fprintf(stderr, "rudolf-duel: serve or find\n"); return 2; }
	if (port < 1024 || port > 65535 || !code_ok(code) || !word_ok(v, 20, ".+-") || !word_ok(game, 40, "-") || inet_pton(AF_INET, addr, &ME.addr) != 1 || !private4(ntohl(ME.addr.s_addr)) || wait < 1 || wait > 3600) { fprintf(stderr, "rudolf-duel: bad arguments\n"); return 2; }
	snprintf(ME.name, sizeof ME.name, "%s", *name ? name : "Rudolf OS"); for (char *c = ME.name; *c; c++) if (!isalnum((unsigned char)*c) && !strchr(" -_", *c)) *c = '-';
	snprintf(ME.code, sizeof ME.code, "%s", code); snprintf(ME.v, sizeof ME.v, "%s", v); snprintf(ME.game, sizeof ME.game, "%s", game); ME.port = port;
	snprintf(ME.host, sizeof ME.host, "rudolf-duel-%s.local", code); for (char *c = ME.host; *c; c++) *c = tolower((unsigned char)*c);
	snprintf(ME.inst, sizeof ME.inst, "%s." SVC, ME.name);
	return cmd_serve(port, wait);
}
#endif
