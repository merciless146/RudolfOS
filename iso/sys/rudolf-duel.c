/*
 * rudolf-duel - the duel service: a relay for two players' games on the local network (Rudolf OS 1.6.1; Ember Reach's
 * duels, rudolfos-library docs/duel-protocol.md). A page can't take connections, so on a LAN one computer relays: the
 * host's game asks for this through the game kit (RG.lan.host -> the desktop -> rudolfd's POST duel/host), rudolfd
 * starts it for that one match, as the signed-in account's own Linux user (never root), and opens the firewall for its
 * port to the local network only while it runs.
 *
 *   rudolf-duel serve --port P --code CODE --ver VER --name NAME
 *       A WebSocket relay holding one room (CODE), the same protocol as the Library's relay (tools/duel-relay:
 *       protocol.mjs in single-room mode): the host's game (from this computer only: loopback) takes the room, the
 *       other joins it with the code; the versions must match; then the game's messages pass between the two as they
 *       are. Neither is told the other's address. Limits as the online relay's: 8 KB a message, 90 a second (a burst
 *       of 180), 10 s to say hello, 60 s of silence, 8 connections; connections only from this computer and private
 *       (local network) addresses. Announced by multicast DNS as "Ember Reach duel on NAME._emberduel._tcp.local"
 *       (its port; TXT ver, code, name), on every interface that is up except loopback and a cellular modem's, and
 *       answering questions for it. It ends, with an mDNS goodbye, when the match ends (a paired room closes), when
 *       the host's game goes, when no host came in 60 s, after 3 hours, or on SIGTERM.
 *   rudolf-duel find
 *       The duels hosted on the local network: an mDNS question for _emberduel._tcp.local (from port 5353, so the
 *       answers come by multicast, which the firewall lets in), answers read for 2 s. Prints
 *       {"ok":true,"games":[{"name","addr","port","url","code","ver"}]}: only answers from private addresses, on an
 *       interface it asked on, from port 5353.
 *
 * Tests: RDUEL_CELLULAR (the file of cellular interfaces, else /run/rudolf/firewall-cellular).
 * Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
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
#include <sys/random.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define MSG_MAX 8192
#define RATE 90.0
#define BURST 180.0
#define HELLO_S 10
#define QUIET_S 60
#define NOHOST_S 60
#define LIFE_S (3 * 3600)
#define CONNS 8
#define SVC "_emberduel._tcp.local"
#define CODECH "ABCDEFGHJKMNPQRSTUVWXYZ23456789"

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
static volatile sig_atomic_t STOP = 0;
static void on_sig(int s) { (void)s; STOP = 1; }

/* ------------------------------------------------------------------ SHA-1 and base64 (the WebSocket handshake) */
static void sha1(const unsigned char *d, size_t n, unsigned char out[20])
{
	uint32_t h[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
	size_t tot = ((n + 8) / 64 + 1) * 64; unsigned char *m = calloc(1, tot); if (!m) { memset(out, 0, 20); return; }
	memcpy(m, d, n); m[n] = 0x80; uint64_t bits = (uint64_t)n * 8; for (int i = 0; i < 8; i++) m[tot - 1 - i] = bits >> (8 * i);
	for (size_t o = 0; o < tot; o += 64) {
		uint32_t w[80];
		for (int i = 0; i < 16; i++) w[i] = (uint32_t)m[o + 4 * i] << 24 | (uint32_t)m[o + 4 * i + 1] << 16 | (uint32_t)m[o + 4 * i + 2] << 8 | m[o + 4 * i + 3];
		for (int i = 16; i < 80; i++) { uint32_t x = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]; w[i] = x << 1 | x >> 31; }
		uint32_t a = h[0], b = h[1], c = h[2], e4 = h[3], e = h[4];
		for (int i = 0; i < 80; i++) {
			uint32_t f, k;
			if (i < 20) { f = (b & c) | (~b & e4); k = 0x5A827999; } else if (i < 40) { f = b ^ c ^ e4; k = 0x6ED9EBA1; }
			else if (i < 60) { f = (b & c) | (b & e4) | (c & e4); k = 0x8F1BBCDC; } else { f = b ^ c ^ e4; k = 0xCA62C1D6; }
			uint32_t t = (a << 5 | a >> 27) + f + e + k + w[i]; e = e4; e4 = c; c = b << 30 | b >> 2; b = a; a = t;
		}
		h[0] += a; h[1] += b; h[2] += c; h[3] += e4; h[4] += e;
	}
	free(m);
	for (int i = 0; i < 5; i++) for (int j = 0; j < 4; j++) out[4 * i + j] = h[i] >> (24 - 8 * j);
}
static void b64(const unsigned char *d, size_t n, char *o)
{
	static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t j = 0;
	for (size_t i = 0; i < n; i += 3) {
		uint32_t v = d[i] << 16 | (i + 1 < n ? d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
		o[j++] = T[v >> 18 & 63]; o[j++] = T[v >> 12 & 63]; o[j++] = i + 1 < n ? T[v >> 6 & 63] : '='; o[j++] = i + 2 < n ? T[v & 63] : '=';
	}
	o[j] = 0;
}

/* ------------------------------------------------------------------ the room's settings */
static char CODE[8], VER[24], NAME[64], INST[96];
static int PORT;
static int code_ok(const char *c) { if (strlen(c) != 6) return 0; for (; *c; c++) if (!strchr(CODECH, *c)) return 0; return 1; }
static int ver_ok(const char *v) { size_t n = strlen(v); if (!n || n > 20) return 0; for (; *v; v++) if (!((*v >= '0' && *v <= '9') || (*v >= 'A' && *v <= 'Z') || (*v >= 'a' && *v <= 'z') || strchr(".+-", *v))) return 0; return 1; }

/* ------------------------------------------------------------------ the players' connections */
typedef struct {
	int fd, ws, said, role, gone, loop;   /* ws: handshake done; role: 0 none, 1 host, 2 guest; loop: from this computer */
	unsigned char in[MSG_MAX * 2 + 64]; size_t inn;
	unsigned char part[MSG_MAX + 1]; size_t partn; int parting;
	double at, heard, tokens, filled;
} conn;
static conn C[CONNS];
static int HOST = -1, GUEST = -1, PAIRED_ONCE = 0, HOSTED_ONCE = 0;

static void wr(int fd, const void *b, size_t n) { const char *p = b; while (n) { ssize_t w = send(fd, p, n, MSG_NOSIGNAL); if (w < 0 && errno == EINTR) continue; if (w <= 0) return; p += w; n -= w; } }
static void frame(conn *c, int op, const char *d, size_t n)
{
	if (c->fd < 0 || n > 65535) return;
	unsigned char h[4]; size_t hl;
	h[0] = 0x80 | op; if (n < 126) { h[1] = n; hl = 2; } else { h[1] = 126; h[2] = n >> 8; h[3] = n & 255; hl = 4; }
	wr(c->fd, h, hl); if (n) wr(c->fd, d, n);
}
static void say(conn *c, const char *t) { frame(c, 1, t, strlen(t)); }
static void conn_close(int i, int code, const char *why);
static void sayerr(conn *c, const char *why)
{
	static const struct { const char *k, *t; } T[] = { { "bad", "That is not a duel the relay knows." }, { "version", "The other player runs another version of the game." },
		{ "room", "There is no duel with that code." }, { "full", "That duel already has two players." }, { "busy", "The relay is busy." },
		{ "limit", "Too much, too fast." }, { "time", "Nothing heard in time." } };
	const char *t = ""; for (size_t i = 0; i < sizeof T / sizeof *T; i++) if (!strcmp(T[i].k, why)) t = T[i].t;
	char b[256];
	if (!strcmp(why, "version")) snprintf(b, sizeof b, "{\"t\":\"error\",\"why\":\"version\",\"text\":\"%s\",\"ver\":\"%s\"}", t, VER);
	else snprintf(b, sizeof b, "{\"t\":\"error\",\"why\":\"%s\",\"text\":\"%s\"}", why, t);
	say(c, b);
}
static void drop(int i, const char *why) { sayerr(&C[i], why); conn_close(i, !strcmp(why, "limit") ? 1008 : 1000, why); }
/* a connection gone: the room's other player is told and let go; once a paired room closes, the match is over */
static void conn_close(int i, int code, const char *why)
{
	conn *c = &C[i]; if (c->fd < 0) return;
	if (c->ws) { unsigned char b[64]; b[0] = code >> 8; b[1] = code & 255; size_t n = snprintf((char *)b + 2, 60, "%s", why ? why : ""); frame(c, 8, (char *)b, 2 + n); }
	shutdown(c->fd, SHUT_RDWR); close(c->fd); c->fd = -1;
	int other = i == HOST ? GUEST : i == GUEST ? HOST : -1, paired = HOST >= 0 && GUEST >= 0;
	if (i == HOST) HOST = -1;
	if (i == GUEST) GUEST = -1;
	if (other >= 0 && C[other].fd >= 0) { say(&C[other], "{\"t\":\"left\"}"); conn_close(other, 1000, "left"); }
	if (paired || (c->role == 1)) STOP = 1;   /* (the match is over, or the host's game went: the service's work is done) */
}

/* the hello's fields: the top level of a small, flat JSON object (strings and numbers); 0 if it isn't one */
static int jfield(const char *s, const char *key, char *out, size_t on)
{
	const char *p = s; while (*p == ' ') p++; if (*p++ != '{') return 0;
	for (;;) {
		while (*p == ' ' || *p == ',') p++;
		if (*p == '}' || !*p) return 0;
		if (*p++ != '"') return 0;
		const char *k = p; while (*p && *p != '"') { if (*p == '\\') return 0; p++; } if (!*p) return 0;
		size_t kl = p - k; p++; while (*p == ' ') p++; if (*p++ != ':') return 0; while (*p == ' ') p++;
		char v[64]; size_t vn = 0;
		if (*p == '"') { p++; while (*p && *p != '"') { if (*p == '\\' || vn + 1 >= sizeof v) return 0; v[vn++] = *p++; } if (!*p) return 0; p++; }
		else { while (*p && *p != ',' && *p != '}') { if (vn + 1 >= sizeof v || *p == '{' || *p == '[') return 0; v[vn++] = *p++; } while (vn && v[vn - 1] == ' ') vn--; }
		v[vn] = 0;
		if (kl == strlen(key) && !strncmp(k, key, kl)) { snprintf(out, on, "%s", v); return 1; }
	}
}
/* one text message from a player (protocol.mjs's Lobby.message, with one room) */
static void message(int i, char *t, size_t n)
{
	conn *c = &C[i]; double tm = now();
	c->tokens += (tm - c->filled) * RATE; if (c->tokens > BURST) c->tokens = BURST; c->filled = tm;
	if (--c->tokens < 0) return drop(i, "limit");
	c->heard = tm;
	if (c->role && HOST >= 0 && GUEST >= 0 && n >= 8 && !strncmp(t, "{\"t\":\"g\"", 8)) { int o = i == HOST ? GUEST : HOST; frame(&C[o], 1, t, n); return; }
	if (memchr(t, 0, n)) return drop(i, "bad");
	char ty[16], proto[8], game[24], ver[24], want[8], code[16] = "";
	if (!jfield(t, "t", ty, sizeof ty)) return drop(i, "bad");
	if (!strcmp(ty, "g")) return;
	if (strcmp(ty, "hello") || c->said) return drop(i, "bad");
	c->said = 1;
	if (!jfield(t, "proto", proto, sizeof proto) || strcmp(proto, "1") || !jfield(t, "game", game, sizeof game) || strcmp(game, "ember-reach") ||
	    !jfield(t, "ver", ver, sizeof ver) || !ver_ok(ver) || !jfield(t, "want", want, sizeof want)) return drop(i, "bad");
	jfield(t, "code", code, sizeof code);
	if (!strcmp(want, "host")) {
		/* (only the game on this computer hosts: someone else on the network can't take the room first) */
		if (!c->loop || HOST >= 0 || HOSTED_ONCE || strcmp(ver, VER)) return drop(i, "busy");
		HOST = i; c->role = 1; HOSTED_ONCE = 1;
		char b[64]; snprintf(b, sizeof b, "{\"t\":\"room\",\"code\":\"%s\"}", CODE); say(c, b); return;
	}
	if (!strcmp(want, "join") || !strcmp(want, "quick")) {
		if (!strcmp(want, "join") && strcmp(code, CODE)) return drop(i, "room");
		if (HOST < 0) return drop(i, "room");
		if (GUEST >= 0) return drop(i, "full");
		if (strcmp(ver, VER)) return drop(i, "version");
		GUEST = i; c->role = 2; PAIRED_ONCE = 1;
		say(&C[HOST], "{\"t\":\"paired\",\"role\":\"host\"}"); say(c, "{\"t\":\"paired\",\"role\":\"guest\"}"); return;
	}
	drop(i, "bad");
}
/* the HTTP request before the WebSocket: Upgrade, the key, version 13 (any path: the hello's routing in its query is
   for relays that route; here there is one room) */
static int handshake(int i)
{
	conn *c = &C[i]; c->in[c->inn] = 0;
	char *e = strstr((char *)c->in, "\r\n\r\n"); if (!e) return c->inn > 4096 ? -1 : 0;
	size_t used = e + 4 - (char *)c->in; *e = 0;
	char *key = NULL, *up = NULL, *ver = NULL;
	for (char *l = strstr((char *)c->in, "\r\n"); l; ) {
		l += 2; char *n = strstr(l, "\r\n"); if (n) *n = 0;
		char *col = strchr(l, ':'); if (col) { *col = 0; char *v = col + 1; while (*v == ' ') v++;
			if (!strcasecmp(l, "Sec-WebSocket-Key")) key = v; else if (!strcasecmp(l, "Upgrade")) up = v; else if (!strcasecmp(l, "Sec-WebSocket-Version")) ver = v; }
		l = n;   /* (NULL after the last line) */
	}
	if (strncmp((char *)c->in, "GET ", 4) || !key || !up || strcasecmp(up, "websocket") || !ver || strcmp(ver, "13") || strlen(key) != 24) {
		const char *no = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n"; wr(c->fd, no, strlen(no)); return -1; }
	char cat[64 + 40]; snprintf(cat, sizeof cat, "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);
	unsigned char h[20]; sha1((unsigned char *)cat, strlen(cat), h); char acc[32]; b64(h, 20, acc);
	char rsp[200]; int rn = snprintf(rsp, sizeof rsp, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", acc);
	wr(c->fd, rsp, rn); c->ws = 1;
	memmove(c->in, c->in + used, c->inn - used); c->inn -= used;
	return 1;
}
/* the WebSocket frames read so far (masked, text only, fragments joined, at most 8 KB a message) */
static void frames(int i)
{
	conn *c = &C[i];
	for (;;) {
		if (c->fd < 0 || c->inn < 2) return;
		unsigned char *b = c->in; int fin = b[0] & 0x80, op = b[0] & 15; size_t n = b[1] & 127, o = 2;
		if (!(b[1] & 0x80) || (b[0] & 0x70)) return conn_close(i, 1002, "protocol");
		if (n == 126) { if (c->inn < 4) return; n = b[2] << 8 | b[3]; o = 4; } else if (n == 127) return conn_close(i, 1009, "too big");
		if (n > MSG_MAX) return conn_close(i, 1009, "too big");
		if (c->inn < o + 4 + n) return;
		unsigned char *mk = b + o, *d = b + o + 4; for (size_t k = 0; k < n; k++) d[k] ^= mk[k & 3];
		unsigned char msg[MSG_MAX + 1]; memcpy(msg, d, n);
		size_t used = o + 4 + n; memmove(c->in, c->in + used, c->inn - used); c->inn -= used;
		if (op == 8) return conn_close(i, 1000, "");
		if (op == 9) { frame(c, 10, (char *)msg, n); continue; }
		if (op == 10) continue;
		if (op == 2) return conn_close(i, 1003, "text only");
		if (op != 0 && op != 1) return conn_close(i, 1002, "protocol");
		if ((op == 1 && c->parting) || (op == 0 && !c->parting)) return conn_close(i, 1002, "protocol");
		if (c->partn + n > MSG_MAX) return drop(i, "limit");
		memcpy(c->part + c->partn, msg, n); c->partn += n; c->parting = !fin;
		if (!fin) continue;
		c->part[c->partn] = 0; size_t tn = c->partn; c->partn = 0;
		message(i, (char *)c->part, tn);
	}
}
/* who may connect: this computer, and private (local network) addresses */
static int addr_ok(struct in_addr a, int *loop)
{
	uint32_t x = ntohl(a.s_addr); *loop = (x >> 24) == 127;
	return *loop || (x >> 24) == 10 || (x >> 20) == (172 << 4 | 1) || (x >> 16) == (192 << 8 | 168) || (x >> 16) == (169 << 8 | 254);
}

/* ------------------------------------------------------------------ multicast DNS */
typedef struct { unsigned char b[1500]; size_t n; int bad; } pkt;
static void p8(pkt *p, int v) { if (p->n < sizeof p->b) p->b[p->n++] = v; else p->bad = 1; }
static void p16(pkt *p, int v) { p8(p, v >> 8 & 255); p8(p, v & 255); }
static void p32(pkt *p, uint32_t v) { p16(p, v >> 16); p16(p, v & 65535); }
static void pname(pkt *p, const char *first, const char *rest)
{
	if (first) { size_t l = strlen(first); if (l > 63) l = 63; p8(p, l); for (size_t k = 0; k < l; k++) p8(p, first[k]); }
	for (const char *s = rest; *s; ) { const char *d = strchr(s, '.'); size_t l = d ? (size_t)(d - s) : strlen(s); p8(p, l); for (size_t k = 0; k < l; k++) p8(p, s[k]); s += l; if (*s == '.') s++; }
	p8(p, 0);
}
/* a name at m+*i (pointers followed, at most 20 of them, bounds checked), labels joined by '.', '.' inside a label as \x1f */
static int rdname(const unsigned char *m, size_t n, size_t *i, char *out, size_t on)
{
	size_t j = *i, o = 0; int jumps = 0, moved = 0;
	for (;;) {
		if (j >= n) return -1;
		unsigned l = m[j];
		if ((l & 0xC0) == 0xC0) { if (j + 1 >= n || ++jumps > 20) return -1; if (!moved) *i = j + 2; moved = 1; j = (l & 0x3F) << 8 | m[j + 1]; continue; }
		if (l & 0xC0) return -1;
		j++;
		if (!l) break;
		if (j + l > n || o + l + 2 >= on) return -1;
		if (o) out[o++] = '.';
		for (unsigned k = 0; k < l; k++) out[o++] = m[j + k] == '.' ? '\x1f' : m[j + k];
		j += l;
	}
	out[o] = 0; if (!moved) *i = j;
	return 0;
}
static int is_cellular(const char *ifn)
{
	const char *p = getenv("RDUEL_CELLULAR"); FILE *f = fopen(p ? p : "/run/rudolf/firewall-cellular", "r"); char l[64]; int r = 0;
	if (!f) return 0;
	while (!r && fgets(l, sizeof l, f)) { l[strcspn(l, "\r\n")] = 0; if (!strcmp(l, ifn)) r = 1; }
	fclose(f); return r;
}
static struct { int ix; struct in_addr a; } MIF[16]; static int NMIF;
static int mif_at(int ix) { for (int i = 0; i < NMIF; i++) if (MIF[i].ix == ix) return i; return -1; }
static int mdns_open(void)
{
	int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0), one = 1, ttl = 255; if (fd < 0) return -1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one); setsockopt(fd, IPPROTO_IP, IP_PKTINFO, &one, sizeof one);
	setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof ttl); setsockopt(fd, IPPROTO_IP, IP_TTL, &ttl, sizeof ttl);
	unsigned char lp = 0; setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &lp, sizeof lp);
	struct sockaddr_in me = {0}; me.sin_family = AF_INET; me.sin_port = htons(5353); me.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(fd, (struct sockaddr *)&me, sizeof me)) { close(fd); return -1; }
	struct ifaddrs *ifs = NULL; if (getifaddrs(&ifs)) return fd;
	for (struct ifaddrs *a = ifs; a; a = a->ifa_next) {
		if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET) continue;
		unsigned fl = a->ifa_flags; if (!(fl & IFF_UP) || !(fl & IFF_MULTICAST) || (fl & (IFF_LOOPBACK | IFF_POINTOPOINT))) continue;
		int ix = if_nametoindex(a->ifa_name); if (!ix || mif_at(ix) >= 0 || NMIF >= 16 || is_cellular(a->ifa_name)) continue;
		struct ip_mreqn mr = {0}; inet_pton(AF_INET, "224.0.0.251", &mr.imr_multiaddr); mr.imr_ifindex = ix;
		if (!setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mr, sizeof mr)) { MIF[NMIF].ix = ix; MIF[NMIF].a = ((struct sockaddr_in *)a->ifa_addr)->sin_addr; NMIF++; }
	}
	freeifaddrs(ifs);
	return fd;
}
static void mdns_send(int fd, pkt *p, int k)
{
	struct sockaddr_in to = {0}; to.sin_family = AF_INET; to.sin_port = htons(5353); inet_pton(AF_INET, "224.0.0.251", &to.sin_addr);
	struct ip_mreqn mr = {0}; mr.imr_ifindex = MIF[k].ix; setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &mr, sizeof mr);
	if (!p->bad && sendto(fd, p->b, p->n, 0, (struct sockaddr *)&to, sizeof to) < 0) {}
}
/* the answer (or announcement, or with ttl 0 the goodbye) for interface k: PTR, SRV, TXT and the A record */
static void mdns_answer(int fd, int k, uint32_t ttl)
{
	char host[80]; snprintf(host, sizeof host, "rudolf-duel-%d.local", PORT);
	pkt p = {0}; p16(&p, 0); p16(&p, 0x8400); p16(&p, 0); p16(&p, 4); p16(&p, 0); p16(&p, 0);
	pname(&p, NULL, SVC); p16(&p, 12); p16(&p, 1); p32(&p, ttl);
	{ pkt q = {0}; pname(&q, INST, SVC); p16(&p, q.n); for (size_t j = 0; j < q.n; j++) p8(&p, q.b[j]); }
	pname(&p, INST, SVC); p16(&p, 33); p16(&p, 0x8001); p32(&p, ttl);
	{ pkt q = {0}; p16(&q, 0); p16(&q, 0); p16(&q, PORT); pname(&q, NULL, host); p16(&p, q.n); for (size_t j = 0; j < q.n; j++) p8(&p, q.b[j]); }
	pname(&p, INST, SVC); p16(&p, 16); p16(&p, 0x8001); p32(&p, ttl);
	{ pkt q = {0}; char kv[3][100]; snprintf(kv[0], 100, "ver=%s", VER); snprintf(kv[1], 100, "code=%s", CODE); snprintf(kv[2], 100, "name=%s", NAME);
	  for (int j = 0; j < 3; j++) { size_t l = strlen(kv[j]); p8(&q, l); for (size_t x = 0; x < l; x++) p8(&q, kv[j][x]); }
	  p16(&p, q.n); for (size_t j = 0; j < q.n; j++) p8(&p, q.b[j]); }
	pname(&p, NULL, host); p16(&p, 1); p16(&p, 0x8001); p32(&p, ttl); p16(&p, 4); { uint32_t a = MIF[k].a.s_addr; const unsigned char *b = (const unsigned char *)&a; for (int j = 0; j < 4; j++) p8(&p, b[j]); }
	mdns_send(fd, &p, k);
}
/* a question that asks for the duel: the service (PTR or ANY) or its instance */
static void mdns_question(int fd, const unsigned char *m, size_t n, int ix)
{
	if (n < 12 || (m[2] & 0x80)) return;
	int k = mif_at(ix); if (k < 0) return;
	int qd = m[4] << 8 | m[5]; size_t i = 12; char nm[256], inst[200];
	snprintf(inst, sizeof inst, "%s.%s", INST, SVC); for (char *x = inst; *x && x < inst + strlen(INST); x++) if (*x == '.') *x = '\x1f';
	for (int q = 0; q < qd && q < 16; q++) {
		if (rdname(m, n, &i, nm, sizeof nm) || i + 4 > n) return;
		int type = m[i] << 8 | m[i + 1]; i += 4;
		if ((!strcasecmp(nm, SVC) && (type == 12 || type == 255)) || !strcasecmp(nm, inst)) { mdns_answer(fd, k, 120); return; }
	}
}

/* ------------------------------------------------------------------ serve */
static int serve(void)
{
	signal(SIGPIPE, SIG_IGN); signal(SIGTERM, on_sig); signal(SIGINT, on_sig); signal(SIGHUP, on_sig);
	int ls = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0), one = 1; if (ls < 0) return 1;
	setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
	struct sockaddr_in me = {0}; me.sin_family = AF_INET; me.sin_port = htons(PORT); me.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(ls, (struct sockaddr *)&me, sizeof me) || listen(ls, 8)) { fprintf(stderr, "rudolf-duel: port %d: %s\n", PORT, strerror(errno)); return 1; }
	for (int i = 0; i < CONNS; i++) C[i].fd = -1;
	int md = mdns_open();
	if (md < 0) fprintf(stderr, "rudolf-duel: multicast DNS: port 5353 not available (%s): not announced\n", strerror(errno));
	printf("rudolf-duel: hosting %s on port %d (%d interface%s announced)\n", CODE, PORT, NMIF, NMIF == 1 ? "" : "s"); fflush(stdout);
	double t0 = now(), next_ann = t0, ann = 0;
	while (!STOP) {
		double t = now();
		if (md >= 0 && ann < 3 && t >= next_ann) { for (int k = 0; k < NMIF; k++) mdns_answer(md, k, 120); ann++; next_ann = t + (ann == 1 ? 1 : 2); }
		if (t - t0 > LIFE_S || (!HOSTED_ONCE && t - t0 > NOHOST_S)) break;
		for (int i = 0; i < CONNS; i++) {
			if (C[i].fd < 0) continue;
			if (!C[i].said && t - C[i].at > HELLO_S) drop(i, "time");
			else if (C[i].role && HOST >= 0 && GUEST >= 0 && t - C[i].heard > QUIET_S) drop(i, "time");
		}
		if (STOP) break;
		struct pollfd pf[CONNS + 2]; int k = 0, who[CONNS + 2];
		pf[k].fd = ls; pf[k].events = POLLIN; who[k++] = -1;
		if (md >= 0) { pf[k].fd = md; pf[k].events = POLLIN; who[k++] = -2; }
		for (int i = 0; i < CONNS; i++) if (C[i].fd >= 0) { pf[k].fd = C[i].fd; pf[k].events = POLLIN; who[k++] = i; }
		if (poll(pf, k, 500) < 0 && errno != EINTR) break;
		for (int j = 0; j < k; j++) {
			if (!pf[j].revents) continue;
			if (who[j] == -1) {
				struct sockaddr_in a = {0}; socklen_t al = sizeof a; int fd = accept4(ls, (struct sockaddr *)&a, &al, SOCK_CLOEXEC); if (fd < 0) continue;
				int loop = 0, slot = -1; for (int i = 0; i < CONNS; i++) if (C[i].fd < 0) { slot = i; break; }
				if (!addr_ok(a.sin_addr, &loop) || slot < 0) { close(fd); continue; }
				setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
				conn *c = &C[slot]; memset(c, 0, sizeof *c); c->fd = fd; c->loop = loop; c->at = c->heard = c->filled = now(); c->tokens = BURST;
			} else if (who[j] == -2) {
				unsigned char m[1500]; struct sockaddr_in from = {0}; char cb[256];
				struct iovec io = { m, sizeof m }; struct msghdr mh = { &from, sizeof from, &io, 1, cb, sizeof cb, 0 };
				ssize_t n = recvmsg(md, &mh, MSG_DONTWAIT); if (n < 12) continue;
				int ix = 0; for (struct cmsghdr *c = CMSG_FIRSTHDR(&mh); c; c = CMSG_NXTHDR(&mh, c)) if (c->cmsg_level == IPPROTO_IP && c->cmsg_type == IP_PKTINFO) ix = ((struct in_pktinfo *)CMSG_DATA(c))->ipi_ifindex;
				mdns_question(md, m, n, ix);
			} else {
				int i = who[j]; conn *c = &C[i]; if (c->fd < 0) continue;
				ssize_t n = recv(c->fd, c->in + c->inn, sizeof c->in - c->inn - 1, MSG_DONTWAIT);
				if (n <= 0) { if (n < 0 && (errno == EAGAIN || errno == EINTR)) continue; conn_close(i, 1000, ""); continue; }
				c->inn += n;
				if (!c->ws) { int h = handshake(i); if (h < 0) { close(c->fd); c->fd = -1; continue; } if (!h) continue; }
				frames(i);
				if (c->fd >= 0 && c->inn >= sizeof c->in - 1) drop(i, "limit");
			}
		}
	}
	for (int i = 0; i < CONNS; i++) if (C[i].fd >= 0) { if (C[i].role && (HOST >= 0 && GUEST >= 0)) say(&C[i], "{\"t\":\"left\"}"); sayerr(&C[i], "time"); shutdown(C[i].fd, SHUT_RDWR); close(C[i].fd); C[i].fd = -1; }
	if (md >= 0) for (int k = 0; k < NMIF; k++) mdns_answer(md, k, 0);   /* (the goodbye: TTL 0) */
	close(ls);
	printf("rudolf-duel: ended (%s)\n", PAIRED_ONCE ? "the match is over" : HOSTED_ONCE ? "the host left" : "nobody hosted"); fflush(stdout);
	return 0;
}

/* ------------------------------------------------------------------ find */
typedef struct { char inst[200], code[8], ver[24], name[64]; struct in_addr a; int port, srv, txt; } found;
static found F[32]; static int NF;
static found *f_get(const char *inst, struct in_addr a)
{
	for (int i = 0; i < NF; i++) if (!strcasecmp(F[i].inst, inst) && F[i].a.s_addr == a.s_addr) return &F[i];
	if (NF >= 32) return NULL;
	found *f = &F[NF++]; memset(f, 0, sizeof *f); snprintf(f->inst, sizeof f->inst, "%s", inst); f->a = a; return f;
}
static void parse_answer(const unsigned char *m, size_t n, struct in_addr from)
{
	if (n < 12 || !(m[2] & 0x80)) return;
	int qd = m[4] << 8 | m[5], rr = (m[6] << 8 | m[7]) + (m[8] << 8 | m[9]) + (m[10] << 8 | m[11]); size_t i = 12; char nm[256];
	for (int k = 0; k < qd; k++) { if (rdname(m, n, &i, nm, sizeof nm) || i + 4 > n) return; i += 4; }
	size_t sl = strlen("." SVC);
	for (int k = 0; k < rr && k < 64; k++) {
		if (rdname(m, n, &i, nm, sizeof nm) || i + 10 > n) return;
		int type = m[i] << 8 | m[i + 1]; size_t len = m[i + 8] << 8 | m[i + 9]; i += 10; if (i + len > n) return; size_t d = i; i += len;
		size_t nl = strlen(nm); if (nl <= sl || strcasecmp(nm + nl - sl, "." SVC)) continue;   /* (only the duel's instances) */
		found *f = f_get(nm, from); if (!f) continue;
		if (type == 33 && len >= 7) { f->port = m[d + 4] << 8 | m[d + 5]; f->srv = 1; }
		else if (type == 16) {
			f->txt = 1;
			for (size_t j = d; j < d + len; ) { size_t l = m[j++]; if (j + l > d + len) break; char kv[128]; snprintf(kv, sizeof kv, "%.*s", (int)l, (const char *)m + j); j += l;
				char *eq = strchr(kv, '='); if (!eq) continue; *eq++ = 0;
				if (!strcmp(kv, "code")) snprintf(f->code, sizeof f->code, "%s", strlen(eq) < sizeof f->code ? eq : "");
				else if (!strcmp(kv, "ver")) snprintf(f->ver, sizeof f->ver, "%s", strlen(eq) < sizeof f->ver ? eq : "");
				else if (!strcmp(kv, "name")) snprintf(f->name, sizeof f->name, "%s", eq); }
		}
	}
}
static void jstr(const char *s)
{
	putchar('"');
	for (const unsigned char *p = (const unsigned char *)s; *p; p++) { if (*p == '"' || *p == '\\') printf("\\%c", *p); else if (*p < 32 || *p == 127) printf("\\u%04x", *p); else putchar(*p); }
	putchar('"');
}
static int find(void)
{
	int fd = mdns_open();
	if (fd < 0) { printf("{\"ok\":false,\"error\":\"multicast DNS could not be used here\"}\n"); return 1; }
	pkt q = {0}; p16(&q, 0); p16(&q, 0); p16(&q, 1); p16(&q, 0); p16(&q, 0); p16(&q, 0); pname(&q, NULL, SVC); p16(&q, 12); p16(&q, 1);
	struct in_addr own[32]; int nown = 0; { struct ifaddrs *ifs = NULL; if (!getifaddrs(&ifs)) { for (struct ifaddrs *a = ifs; a; a = a->ifa_next) if (a->ifa_addr && a->ifa_addr->sa_family == AF_INET && nown < 32) own[nown++] = ((struct sockaddr_in *)a->ifa_addr)->sin_addr; freeifaddrs(ifs); } }
	for (int round = 0; round < 2; round++) {
		for (int k = 0; k < NMIF; k++) mdns_send(fd, &q, k);
		double end = now() + (round ? 0.9 : 1.1);
		for (;;) {
			int left = (int)((end - now()) * 1000); if (left <= 0) break;
			struct pollfd p = { fd, POLLIN, 0 }; if (poll(&p, 1, left) <= 0) break;
			unsigned char m[1500]; struct sockaddr_in from = {0}; char cb[256];
			struct iovec io = { m, sizeof m }; struct msghdr mh = { &from, sizeof from, &io, 1, cb, sizeof cb, 0 };
			ssize_t n = recvmsg(fd, &mh, MSG_DONTWAIT); if (n < 12) continue;
			int ix = 0; for (struct cmsghdr *c = CMSG_FIRSTHDR(&mh); c; c = CMSG_NXTHDR(&mh, c)) if (c->cmsg_level == IPPROTO_IP && c->cmsg_type == IP_PKTINFO) ix = ((struct in_pktinfo *)CMSG_DATA(c))->ipi_ifindex;
			int loop = 0;
			if (ntohs(from.sin_port) != 5353 || mif_at(ix) < 0 || !addr_ok(from.sin_addr, &loop) || loop) continue;   /* (RFC 6762 6; the local network only) */
			(void)own; (void)nown;
			parse_answer(m, n, from.sin_addr);
		}
	}
	printf("{\"ok\":true,\"games\":[");
	int first = 1;
	for (int i = 0; i < NF; i++) {
		found *f = &F[i]; if (!f->srv || !f->txt || !code_ok(f->code) || !ver_ok(f->ver) || f->port < 1024) continue;
		char nm[64]; snprintf(nm, sizeof nm, "%s", *f->name ? f->name : "a computer"); for (char *x = nm; *x; x++) if ((unsigned char)*x < 32) *x = ' ';
		char ad[20]; inet_ntop(AF_INET, &f->a, ad, sizeof ad);
		printf("%s{\"name\":", first ? "" : ","); jstr(nm); printf(",\"addr\":\"%s\",\"port\":%d,\"url\":\"ws://%s:%d/\",\"code\":\"%s\",\"ver\":", ad, f->port, ad, f->port, f->code); jstr(f->ver); printf("}");
		first = 0;
	}
	printf("]}\n");
	close(fd);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc >= 2 && !strcmp(argv[1], "find")) return find();
	if (argc >= 2 && !strcmp(argv[1], "serve")) {
		for (int i = 2; i + 1 < argc; i += 2) {
			if (!strcmp(argv[i], "--port")) PORT = atoi(argv[i + 1]);
			else if (!strcmp(argv[i], "--code")) snprintf(CODE, sizeof CODE, "%s", strlen(argv[i + 1]) < sizeof CODE ? argv[i + 1] : "");
			else if (!strcmp(argv[i], "--ver")) snprintf(VER, sizeof VER, "%s", strlen(argv[i + 1]) < sizeof VER ? argv[i + 1] : "");
			else if (!strcmp(argv[i], "--name")) snprintf(NAME, sizeof NAME, "%s", argv[i + 1]);
		}
		if (PORT < 1024 || PORT > 65535 || !code_ok(CODE) || !ver_ok(VER)) { fprintf(stderr, "usage: rudolf-duel serve --port P (1024-65535) --code CODE --ver VER [--name NAME]\n"); return 2; }
		if (!geteuid()) { fprintf(stderr, "rudolf-duel: not as root\n"); return 2; }
		for (char *x = NAME; *x; x++) if ((unsigned char)*x < 32 || *x == '"' || *x == '\\') *x = ' ';
		if (!*NAME) snprintf(NAME, sizeof NAME, "a Rudolf OS computer");
		snprintf(INST, sizeof INST, "Ember Reach duel on %.40s", NAME);
		return serve();
	}
	fprintf(stderr, "usage: rudolf-duel serve --port P --code CODE --ver VER [--name NAME] | rudolf-duel find\n");
	return 2;
}
