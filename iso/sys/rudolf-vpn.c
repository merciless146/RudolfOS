/*
 * vpn - the root helper for VPNs (Rudolf OS 1.6.2, docs/plan-1.6.2.md Part 33): /usr/lib/rudolf/root/vpn, run by
 * rudolf-rootd for rudolfd (or by rudolfd itself while it is root and was not started by rudolf-rootd).
 *
 * A VPN here is the whole computer's: while one is on, every program's traffic and its name lookups go through it.
 * WireGuard is the kernel's own (built in); this helper sets it up through the kernel's netlink itself, as wg and
 * wg-quick would (no WireGuard tools are shipped):
 *   - the interface rvpn0 (one VPN on at a time), its private key, its peers (public key, preshared key, endpoint,
 *     allowed IPs, keepalive), its addresses and MTU;
 *   - the routes: a peer's AllowedIPs. 0.0.0.0/0 (or ::/0) takes the whole computer as wg-quick does: a routing table of
 *     its own (VPN_TABLE) with the default route into the tunnel, used by every packet that is not the tunnel's own
 *     (fwmark VPN_MARK: WireGuard marks what it sends to the server), so the server's own route stays outside the
 *     tunnel on whatever network the computer is on (another Wi-Fi included), and the local network's own routes kept
 *     (suppress_prefixlength 0). Without IPv6 policy routing in the kernel, ::/0 is the two halves ::/1 and 8000::/1.
 *     A tunnel that takes the whole of IPv4 but no IPv6 makes IPv6 unreachable while it is on (no way around it);
 *   - the file's DNS servers: /etc/resolv.conf (written in place: Debian's sandbox has it bound) and RUN/dns-vpn,
 *     which udhcpc.sh and the modem script use in place of a connection's own while it is there; the network's own
 *     put back at the end.
 * Off again: the interface, its rules and routes removed, src_valid_mark and the name servers as they were.
 *
 *   vpn list                     every VPN as JSON (never a key): id, name, type, policy, server, addresses,
 *                                DNS, and the one on: state (connecting, connected, noanswer), since, by, bytes
 *   vpn add wireguard            standard input: the VPN's name on the first line, then the .conf file (at most 64 KB).
 *                                Checked (a malformed file is refused with the line it fails on), kept in VPN_DIR
 *                                (root's 0700; each file root's 0600: the private key is read by nobody else).
 *                                Prints {"ok":true,"id":ID,"notes":[...]}
 *   vpn set ID name|policy VALUE the name (standard input, one line, at most 64 bytes); the policy
 *                                (everyone, admins, always)
 *   vpn remove ID                turned off first if it is on; its files removed
 *   vpn up ID BY                 turned on (another one on is turned off first); BY: who turned it on (the account's
 *                                name, at most 64 bytes, or "-") for "Ann's VPN is on for everyone on this computer"
 *   vpn down                     the VPN that is on turned off
 *   vpn boot                     at rudolfd's start: an "Always on" VPN turned on
 *   vpn tick                     every half minute while a VPN is meant to be on: set up again if its interface is gone
 *
 * Who may do which is rudolfd's (rudolfd-vpn.inc: an administrator's password for add, set and remove; the VPN's
 * policy and parental controls for up and down). Everything is checked again here, whatever rudolfd checked: an ID is
 * "v" and 8 hex digits, names are printable UTF-8. The private and preshared keys go from the file to the kernel and
 * nowhere else: never printed, never in the health log, never in a file another user can read. A refusal is said in
 * the health log (Security).
 *
 * Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include "rudolf-root.h"
#include <ctype.h>
#include <dirent.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/file.h>
#include <sys/random.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/genetlink.h>
#include <linux/fib_rules.h>
#include <linux/if_link.h>
#include <linux/wireguard.h>

#define VPN_DIR  rr_env("RR_TEST_VPNDIR", "/persist/etc/vpn")
#define RUNDIR   rr_env("RR_TEST_RUN", "/run/rudolf")
#define ETC      rr_env("RR_TEST_ETC", "/etc")
#define PROCNET  rr_env("RR_TEST_PROCNET", "/proc/sys/net")
#define VPN_IF   "rvpn0"
#define VPN_MARK  0x52564eu   /* ("RVN"; the fwmark WireGuard puts on what it sends to the server) */
#define VPN_TABLE 52566u      /* (the whole computer's routing table while a VPN is on) */
#define VPN_PREF  32760u      /* (the rules' priorities: VPN_PREF the local network's routes, VPN_PREF+1 the tunnel) */
#define MAXCONF  65536
#define MAXPEERS 16
#define MAXIPS   64
#define MAXADDR  8
#define MAXDNS   8

static int refuse(const char *what, const char *arg)
{
	char a[80]; size_t k = 0; for (const char *c = arg ? arg : ""; *c && k < sizeof a - 1; c++) a[k++] = isprint((unsigned char)*c) ? *c : '?';
	a[k] = 0;
	rr_health("security", "the VPN helper refused %s (%s)", what, a);
	fprintf(stderr, "refused: %s\n", what); return 2;
}
static void jstr(FILE *o, const char *s)
{
	fputc('"', o);
	for (const unsigned char *c = (const unsigned char *)(s ? s : ""); *c; c++) {
		if (*c == '"' || *c == '\\') fprintf(o, "\\%c", *c);
		else if (*c < 0x20) fprintf(o, "\\u%04x", *c);
		else if (*c == '<' || *c == '>' || *c == '&') fprintf(o, "\\u%04x", *c);   /* (never markup, wherever it lands) */
		else fputc(*c, o);
	}
	fputc('"', o);
}
/* an ID: "v" and 8 lower-case hex digits */
static int id_ok(const char *s) { if (!s || strlen(s) != 9 || s[0] != 'v') return 0; for (int i = 1; i < 9; i++) if (!isxdigit((unsigned char)s[i]) || isupper((unsigned char)s[i])) return 0; return 1; }
/* a name for people: 1 to max bytes of UTF-8, no control characters */
static int utf8_name_ok(const char *s, size_t max)
{
	size_t n = s ? strlen(s) : 0; if (!n || n > max) return 0;
	const unsigned char *p = (const unsigned char *)s;
	while (*p) {
		if (*p < 0x20 || *p == 0x7f) return 0;
		if (*p < 0x80) { p++; continue; }
		int k = (*p & 0xe0) == 0xc0 ? 1 : (*p & 0xf0) == 0xe0 ? 2 : (*p & 0xf8) == 0xf0 ? 3 : -1;
		if (k < 0 || (*p == 0xc0 || *p == 0xc1)) return 0;
		for (int i = 1; i <= k; i++) if ((p[i] & 0xc0) != 0x80) return 0;
		if (k == 1 && p[1] >= 0x80 && p[1] <= 0x9f && *p == 0xc2) return 0;   /* (C1 controls) */
		p += k + 1;
	}
	return 1;
}
static char *read_all(int fd, size_t max, size_t *len)
{
	char *b = malloc(max + 2); size_t n = 0; ssize_t r; if (!b) return NULL;
	while ((r = read(fd, b + n, max + 1 - n)) > 0 || (r < 0 && errno == EINTR)) if (r > 0) { n += (size_t)r; if (n > max) break; }
	if (n > max) { explicit_bzero(b, n); free(b); return NULL; }
	b[n] = 0; if (len) *len = n; return b;
}
static char *slurp_any(const char *path, size_t max, size_t *len, int follow)
{
	int f = open(path, O_RDONLY | O_CLOEXEC | (follow ? 0 : O_NOFOLLOW)); if (f < 0) return NULL;
	char *b = read_all(f, max, len); close(f); return b;
}
static char *slurp(const char *path, size_t max, size_t *len) { return slurp_any(path, max, len, 0); }
/* a file of root's (0600 unless mode), written whole and renamed into place */
static int put_file(const char *path, const char *data, size_t n, mode_t mode)
{
	char tp[PATH_MAX]; snprintf(tp, sizeof tp, "%s.new", path); unlink(tp);
	int f = open(tp, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, mode);
	if (f < 0) return -1;
	int ok = write(f, data, n) == (ssize_t)n; if (fchmod(f, mode)) ok = 0; if (fsync(f)) {} close(f);
	if (!ok || rename(tp, path)) { unlink(tp); return -1; }
	return 0;
}
/* written in place, its inode kept (Debian's sandbox binds /etc/resolv.conf: a new file would not reach it); a link
   there is followed, as udhcpc.sh's "cat >" follows it (/etc is root's) */
static int put_inplace(const char *path, const char *data)
{
	int f = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644); if (f < 0) return -1;
	if (fchmod(f, 0644)) {}
	size_t n = strlen(data); int ok = write(f, data, n) == (ssize_t)n; close(f); return ok ? 0 : -1;
}
static void vpn_event(void)
{
	char p[PATH_MAX]; snprintf(p, sizeof p, "%s/events", RUNDIR);
	int f = open(p, O_WRONLY | O_APPEND | O_CLOEXEC | O_NOFOLLOW); if (f >= 0) { if (write(f, "vpn changed\n", 12) < 0) {} close(f); }
}
static void mkdirs(void)
{
	char p[PATH_MAX];
	{ /* VPN_DIR's parent may be missing on a live system's /persist */
		snprintf(p, sizeof p, "%s", VPN_DIR); char *s = strrchr(p, '/'); if (s && s != p) { *s = 0; if (!mkdir(p, 0755) && chmod(p, 0755)) {} } }
	mkdir(VPN_DIR, 0700); if (chmod(VPN_DIR, 0700)) {}
	if (!mkdir(RUNDIR, 0755) && chmod(RUNDIR, 0755)) {}
	snprintf(p, sizeof p, "%s/vpn", RUNDIR); mkdir(p, 0700); if (chmod(p, 0700)) {}
}

/* ------------------------------------------------------------------ the .conf file */
typedef struct { int fam; uint8_t a[16]; int len; } Cidr;
typedef struct {
	uint8_t pub[32], psk[32]; int has_psk;
	char host[256]; int port; int keepalive; int has_keepalive;
	Cidr ips[MAXIPS]; int nips;
} Peer;
typedef struct {
	uint8_t key[32]; int listen; int mtu;
	Cidr addr[MAXADDR]; int naddr;
	char dns[MAXDNS][48]; int ndns; char search[256];
	Peer peer[MAXPEERS]; int npeer;
	char notes[4][160]; int nnotes;
} Conf;
static void conf_wipe(Conf *c) { explicit_bzero(c, sizeof *c); }
static int b64key(const char *s, uint8_t out[32])
{
	if (strlen(s) != 44 || s[43] != '=') return -1;
	static const char *A = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	uint32_t acc = 0; int bits = 0, n = 0;
	for (int i = 0; i < 43; i++) { const char *q = strchr(A, s[i]); if (!q || !s[i]) return -1; acc = acc << 6 | (uint32_t)(q - A); bits += 6;
		if (bits >= 8) { bits -= 8; if (n < 32) out[n++] = (uint8_t)(acc >> bits); acc &= (1u << bits) - 1; } }
	return n == 32 && acc == 0 ? 0 : -1;   /* (the last character's spare bits must be zero, as wg checks) */
}
static char *trimw(char *s) { while (*s == ' ' || *s == '\t') s++; size_t n = strlen(s); while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) s[--n] = 0; return s; }
static int parse_ip(const char *s, int *fam, uint8_t *a)
{
	if (inet_pton(AF_INET, s, a) == 1) { *fam = AF_INET; return 0; }
	if (inet_pton(AF_INET6, s, a) == 1) { *fam = AF_INET6; return 0; }
	return -1;
}
/* "10.0.0.2/24", "fd00::2/64", "1.2.3.4" (a host); for AllowedIPs the host bits are cleared */
static int parse_cidr(const char *s0, Cidr *c, int clear)
{
	char s[80]; if (strlen(s0) >= sizeof s) return -1; snprintf(s, sizeof s, "%s", s0);
	char *sl = strchr(s, '/'); int len = -1;
	if (sl) { *sl++ = 0; if (!*sl || strspn(sl, "0123456789") != strlen(sl) || strlen(sl) > 3) return -1; len = atoi(sl); }
	memset(c, 0, sizeof *c); if (parse_ip(s, &c->fam, c->a)) return -1;
	int max = c->fam == AF_INET ? 32 : 128; if (len < 0) len = max; if (len > max) return -1; c->len = len;
	if (clear) for (int b = len; b < max; b++) c->a[b / 8] &= (uint8_t)~(0x80 >> (b % 8));
	return 0;
}
static int split_list(char *v, char **out, int max)
{
	int n = 0; char *sv = NULL;
	for (char *t = strtok_r(v, ",", &sv); t; t = strtok_r(NULL, ",", &sv)) { t = trimw(t); if (!*t) continue; if (n >= max) return -1; out[n++] = t; }
	return n;
}
static int num_ok(const char *v, long lo, long hi, long *out)
{
	if (!*v || strlen(v) > 6 || strspn(v, "0123456789") != strlen(v)) return 0;
	long x = atol(v); if (x < lo || x > hi) return 0; *out = x; return 1;
}
/* the endpoint "host:port" or "[v6]:port": host letters, digits, - . (a name) or an address */
static int parse_endpoint(const char *v, char *host, size_t hn, int *port)
{
	const char *c; char h[256];
	if (*v == '[') { const char *e = strchr(v, ']'); if (!e || e[1] != ':' || (size_t)(e - v - 1) >= sizeof h) return -1; snprintf(h, sizeof h, "%.*s", (int)(e - v - 1), v + 1); c = e + 1;
		struct in6_addr t; if (inet_pton(AF_INET6, h, &t) != 1) return -1; }
	else { c = strrchr(v, ':'); if (!c || (size_t)(c - v) >= sizeof h || c == v) return -1; snprintf(h, sizeof h, "%.*s", (int)(c - v), v);
		if (strspn(h, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.") != strlen(h) || h[0] == '-' || h[0] == '.') return -1; }
	long p; if (!num_ok(c + 1, 1, 65535, &p)) return -1;
	if (strlen(h) >= hn) return -1; snprintf(host, hn, "%s", h); *port = (int)p; return 0;
}
static void note(Conf *c, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void note(Conf *c, const char *fmt, ...)
{
	if (c->nnotes >= 4) return; va_list ap; va_start(ap, fmt); vsnprintf(c->notes[c->nnotes++], sizeof c->notes[0], fmt, ap); va_end(ap);
}
/* the whole file, checked; 0, or -1 with why (the line it fails on) */
static int conf_parse(const char *text, size_t len, Conf *c, char *why, size_t wn)
{
	memset(c, 0, sizeof *c); c->ndns = 0;
	if (len > MAXCONF) { snprintf(why, wn, "the file is over 64 KB: it is not a WireGuard file"); return -1; }
	if (memchr(text, 0, len)) { snprintf(why, wn, "the file has a zero byte: it is not a text file"); return -1; }
	char *t = malloc(len + 1); if (!t) { snprintf(why, wn, "out of memory"); return -1; } memcpy(t, text, len); t[len] = 0;
	int sec = 0, ln = 0, haskey = 0, rc = -1, ign = 0; Peer *p = NULL; char *sv = NULL;
	if ((unsigned char)t[0] == 0xef && (unsigned char)t[1] == 0xbb && (unsigned char)t[2] == 0xbf) memset(t, ' ', 3);   /* (a BOM from Windows) */
	(void)sv;
	for (char *l = t, *nx; l; l = nx) {
		nx = strchr(l, '\n'); if (nx) *nx++ = 0;
		ln++;
		char *h = strchr(l, '#'); if (h) *h = 0;
		l = trimw(l); if (!*l) continue;
		if (strlen(l) > 4096) { snprintf(why, wn, "line %d is too long", ln); goto out; }
		if (*l == '[') {
			if (!strcasecmp(l, "[Interface]")) { if (sec == 1 || c->npeer) { snprintf(why, wn, "line %d: a second [Interface]", ln); goto out; } sec = 1; continue; }
			if (!strcasecmp(l, "[Peer]")) { if (c->npeer >= MAXPEERS) { snprintf(why, wn, "line %d: more than %d peers", ln, MAXPEERS); goto out; } p = &c->peer[c->npeer++]; memset(p, 0, sizeof *p); sec = 2; continue; }
			snprintf(why, wn, "line %d: %.40s is not a section of a WireGuard file ([Interface] or [Peer])", ln, l); goto out;
		}
		char *eq = strchr(l, '='); if (!eq) { snprintf(why, wn, "line %d is not \"Key = value\"", ln); goto out; }
		*eq = 0; char *k = trimw(l), *v = trimw(eq + 1);
		if (!sec) { snprintf(why, wn, "line %d: %.40s before [Interface]", ln, k); goto out; }
		for (char *q = v; *q; q++) if ((unsigned char)*q < 0x20 && *q != '\t') { snprintf(why, wn, "line %d has a control character", ln); goto out; }
		if (sec == 1) {
			if (!strcasecmp(k, "PrivateKey")) { if (b64key(v, c->key)) { snprintf(why, wn, "line %d: the private key is not a WireGuard key (44 characters of base64)", ln); goto out; } haskey = 1; }
			else if (!strcasecmp(k, "Address")) { char *it[MAXADDR]; int n = split_list(v, it, MAXADDR - c->naddr); if (n <= 0) { snprintf(why, wn, "line %d: no address, or more than %d", ln, MAXADDR); goto out; }
				for (int i = 0; i < n; i++) if (parse_cidr(it[i], &c->addr[c->naddr++], 0)) { snprintf(why, wn, "line %d: %.60s is not an address", ln, it[i]); goto out; } }
			else if (!strcasecmp(k, "DNS")) { char *it[MAXDNS + 4]; int n = split_list(v, it, MAXDNS + 4); if (n < 0) { snprintf(why, wn, "line %d: too many DNS servers", ln); goto out; }
				for (int i = 0; i < n; i++) { int fam; uint8_t a[16];
					if (!parse_ip(it[i], &fam, a)) { if (c->ndns >= MAXDNS) { snprintf(why, wn, "line %d: more than %d DNS servers", ln, MAXDNS); goto out; } inet_ntop(fam, a, c->dns[c->ndns++], 48); }
					else if (strlen(it[i]) < 64 && strspn(it[i], "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.") == strlen(it[i]) && it[i][0] != '-' && it[i][0] != '.' && strlen(c->search) + strlen(it[i]) + 2 < sizeof c->search) { if (*c->search) strcat(c->search, " "); strcat(c->search, it[i]); }
					else { snprintf(why, wn, "line %d: %.60s is not a DNS server or a domain", ln, it[i]); goto out; } } }
			else if (!strcasecmp(k, "ListenPort")) { long x; if (!num_ok(v, 1, 65535, &x)) { snprintf(why, wn, "line %d: ListenPort is not a port", ln); goto out; } c->listen = (int)x; }
			else if (!strcasecmp(k, "MTU")) { long x; if (!num_ok(v, 576, 9000, &x)) { snprintf(why, wn, "line %d: MTU is not 576 to 9000", ln); goto out; } c->mtu = (int)x; }
			else if (!strcasecmp(k, "PreUp") || !strcasecmp(k, "PostUp") || !strcasecmp(k, "PreDown") || !strcasecmp(k, "PostDown")) { if (!(ign & 1)) note(c, "Its commands (%s) are never run: Rudolf OS sets the VPN up itself.", k); ign |= 1; }
			else if (!strcasecmp(k, "Table") || !strcasecmp(k, "FwMark") || !strcasecmp(k, "SaveConfig")) { if (!(ign & 2)) note(c, "%s is not used: Rudolf OS chooses the routing itself.", k); ign |= 2; }
			else { snprintf(why, wn, "line %d: %.40s is not a setting of [Interface]", ln, k); goto out; }
		} else {
			if (!strcasecmp(k, "PublicKey")) { if (b64key(v, p->pub)) { snprintf(why, wn, "line %d: the public key is not a WireGuard key (44 characters of base64)", ln); goto out; } }
			else if (!strcasecmp(k, "PresharedKey")) { if (b64key(v, p->psk)) { snprintf(why, wn, "line %d: the preshared key is not a WireGuard key", ln); goto out; } p->has_psk = 1; }
			else if (!strcasecmp(k, "AllowedIPs")) { char *it[MAXIPS]; int n = split_list(v, it, MAXIPS - p->nips); if (n < 0) { snprintf(why, wn, "line %d: more than %d allowed IPs", ln, MAXIPS); goto out; }
				for (int i = 0; i < n; i++) if (parse_cidr(it[i], &p->ips[p->nips++], 1)) { snprintf(why, wn, "line %d: %.60s is not an address or network", ln, it[i]); goto out; } }
			else if (!strcasecmp(k, "Endpoint")) { if (parse_endpoint(v, p->host, sizeof p->host, &p->port)) { snprintf(why, wn, "line %d: the endpoint is not host:port", ln); goto out; } }
			else if (!strcasecmp(k, "PersistentKeepalive")) { long x; if (!strcasecmp(v, "off")) x = 0; else if (!num_ok(v, 0, 65535, &x)) { snprintf(why, wn, "line %d: PersistentKeepalive is not 0 to 65535", ln); goto out; } p->keepalive = (int)x; p->has_keepalive = 1; }
			else { snprintf(why, wn, "line %d: %.40s is not a setting of [Peer]", ln, k); goto out; }
		}
	}
	if (!sec) { snprintf(why, wn, "this is not a WireGuard file: it has no [Interface]"); goto out; }
	if (!haskey) { snprintf(why, wn, "the file has no PrivateKey in [Interface]"); goto out; }
	if (!c->naddr) { snprintf(why, wn, "the file has no Address in [Interface]"); goto out; }
	if (!c->npeer) { snprintf(why, wn, "the file has no [Peer] (the VPN's server)"); goto out; }
	for (int i = 0; i < c->npeer; i++) {
		static const uint8_t z[32];
		if (!memcmp(c->peer[i].pub, z, 32)) { snprintf(why, wn, "peer %d has no PublicKey", i + 1); goto out; }
		if (!c->peer[i].nips) { snprintf(why, wn, "peer %d has no AllowedIPs", i + 1); goto out; }
	}
	if (!c->peer[0].host[0]) { snprintf(why, wn, "the first [Peer] has no Endpoint (the VPN server's address): this computer would have nowhere to connect"); goto out; }
	rc = 0;
out:
	explicit_bzero(t, len); free(t);
	if (rc) conf_wipe(c);
	return rc;
}
static int full4(const Conf *c) { for (int i = 0; i < c->npeer; i++) for (int k = 0; k < c->peer[i].nips; k++) if (c->peer[i].ips[k].fam == AF_INET && !c->peer[i].ips[k].len) return 1; return 0; }
static int full6(const Conf *c) { for (int i = 0; i < c->npeer; i++) for (int k = 0; k < c->peer[i].nips; k++) if (c->peer[i].ips[k].fam == AF_INET6 && !c->peer[i].ips[k].len) return 1; return 0; }
static int any6(const Conf *c) { for (int i = 0; i < c->npeer; i++) for (int k = 0; k < c->peer[i].nips; k++) if (c->peer[i].ips[k].fam == AF_INET6) return 1; return 0; }

/* ------------------------------------------------------------------ what is kept: VPN_DIR/ID.conf (the file), ID.meta */
typedef struct { char id[16], name[80], type[16], policy[16]; long long added; } Meta;
static void meta_path(const char *id, const char *ext, char *o, size_t n) { snprintf(o, n, "%s/%s.%s", VPN_DIR, id, ext); }
static int meta_read(const char *id, Meta *m)
{
	memset(m, 0, sizeof *m); char p[PATH_MAX]; meta_path(id, "meta", p, sizeof p);
	char *t = slurp(p, 8192, NULL); if (!t) return -1;
	snprintf(m->id, sizeof m->id, "%s", id); snprintf(m->policy, sizeof m->policy, "everyone"); snprintf(m->type, sizeof m->type, "wireguard");
	char *sv = NULL;
	for (char *l = strtok_r(t, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) {
		if (!strncmp(l, "name=", 5) && utf8_name_ok(l + 5, 64)) snprintf(m->name, sizeof m->name, "%s", l + 5);
		else if (!strncmp(l, "type=", 5) && (!strcmp(l + 5, "wireguard") || !strcmp(l + 5, "openvpn"))) snprintf(m->type, sizeof m->type, "%s", l + 5);
		else if (!strncmp(l, "policy=", 7) && (!strcmp(l + 7, "everyone") || !strcmp(l + 7, "admins") || !strcmp(l + 7, "always"))) snprintf(m->policy, sizeof m->policy, "%s", l + 7);
		else if (!strncmp(l, "added=", 6)) m->added = atoll(l + 6);
	}
	free(t); if (!m->name[0]) snprintf(m->name, sizeof m->name, "VPN");
	return 0;
}
static int meta_write(const Meta *m)
{
	char p[PATH_MAX], b[1024]; meta_path(m->id, "meta", p, sizeof p);
	int n = snprintf(b, sizeof b, "name=%s\ntype=%s\npolicy=%s\nadded=%lld\n", m->name, m->type, m->policy, m->added);
	return put_file(p, b, (size_t)n, 0600);
}
/* the one meant to be on: RUN/vpn/on, "ID\nBY\nSINCE\n" */
typedef struct { char id[16], by[80]; long long since; } OnState;
static int on_read(OnState *o)
{
	memset(o, 0, sizeof *o); char p[PATH_MAX]; snprintf(p, sizeof p, "%s/vpn/on", RUNDIR);
	char *t = slurp(p, 1024, NULL); if (!t) return 0;
	char *l2 = strchr(t, '\n'); if (l2) *l2++ = 0;
	char *l3 = l2 ? strchr(l2, '\n') : NULL; if (l3) *l3++ = 0;
	if (id_ok(t)) { snprintf(o->id, sizeof o->id, "%s", t); if (l2 && utf8_name_ok(l2, 64)) snprintf(o->by, sizeof o->by, "%s", l2); if (l3) o->since = atoll(l3); }
	free(t); return o->id[0] != 0;
}
static void on_write(const char *id, const char *by)
{
	char p[PATH_MAX], b[200]; snprintf(p, sizeof p, "%s/vpn/on", RUNDIR);
	int n = snprintf(b, sizeof b, "%s\n%s\n%lld\n", id, by && *by ? by : "-", (long long)time(NULL)); put_file(p, b, (size_t)n, 0600);
}
static void on_clear(void) { char p[PATH_MAX]; snprintf(p, sizeof p, "%s/vpn/on", RUNDIR); unlink(p); }

/* ------------------------------------------------------------------ netlink */
typedef struct { char b[32768]; size_t n; } NL;
static struct nlmsghdr *nl_start(NL *m, int type, int flags) { memset(m, 0, sizeof *m); struct nlmsghdr *h = (void *)m->b; h->nlmsg_type = (uint16_t)type; h->nlmsg_flags = (uint16_t)(NLM_F_REQUEST | flags); m->n = NLMSG_HDRLEN; return h; }
static void *nl_put(NL *m, const void *d, size_t n) { size_t al = NLMSG_ALIGN(n); if (m->n + al > sizeof m->b) return NULL; void *p = m->b + m->n; memset(p, 0, al); if (d) memcpy(p, d, n); m->n += al; return p; }
static int nla(NL *m, int type, const void *d, size_t n)
{
	size_t tot = NLA_HDRLEN + n; if (m->n + NLA_ALIGN(tot) > sizeof m->b) return -1;
	struct nlattr *a = (void *)(m->b + m->n); a->nla_type = (uint16_t)type; a->nla_len = (uint16_t)tot; memset((char *)a + NLA_HDRLEN, 0, NLA_ALIGN(tot) - NLA_HDRLEN);
	if (d) memcpy((char *)a + NLA_HDRLEN, d, n); m->n += NLA_ALIGN(tot); return 0;
}
static int nla_u8(NL *m, int t, uint8_t v) { return nla(m, t, &v, 1); }
static int nla_u16(NL *m, int t, uint16_t v) { return nla(m, t, &v, 2); }
static int nla_u32(NL *m, int t, uint32_t v) { return nla(m, t, &v, 4); }
static int nla_str(NL *m, int t, const char *s) { return nla(m, t, s, strlen(s) + 1); }
static size_t nest_start(NL *m, int type) { size_t o = m->n; nla(m, type | NLA_F_NESTED, NULL, 0); return o; }
static void nest_end(NL *m, size_t o) { ((struct nlattr *)(m->b + o))->nla_len = (uint16_t)(m->n - o); }
static uint32_t nl_seq;
static int nl_open(int proto)
{
	int s = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, proto); if (s < 0) return -1;
	struct sockaddr_nl a = { .nl_family = AF_NETLINK }; if (bind(s, (struct sockaddr *)&a, sizeof a)) { close(s); return -1; }
	struct timeval tv = { 5, 0 }; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
	return s;
}
/* sent; each answer to cb (a dump), until the ack or the dump's end: 0, or -errno */
typedef int (*nl_cb)(struct nlmsghdr *h, void *ctx);
static int nl_talk(int s, NL *m, nl_cb cb, void *ctx)
{
	struct nlmsghdr *h = (void *)m->b; h->nlmsg_len = (uint32_t)m->n; h->nlmsg_seq = ++nl_seq; if (!cb) h->nlmsg_flags |= NLM_F_ACK;
	struct sockaddr_nl k = { .nl_family = AF_NETLINK };
	if (sendto(s, m->b, m->n, 0, (struct sockaddr *)&k, sizeof k) != (ssize_t)m->n) return -errno;
	static char rb[65536];
	for (;;) {
		ssize_t r = recv(s, rb, sizeof rb, 0); if (r < 0) { if (errno == EINTR) continue; return -errno; }
		for (struct nlmsghdr *q = (void *)rb; NLMSG_OK(q, (size_t)r); q = NLMSG_NEXT(q, r)) {
			if (q->nlmsg_seq != nl_seq) continue;
			if (q->nlmsg_type == NLMSG_ERROR) { struct nlmsgerr *e = NLMSG_DATA(q); return e->error; }
			if (q->nlmsg_type == NLMSG_DONE) return 0;
			if (cb && cb(q, ctx)) {}
		}
	}
}
static int rt_sock = -1, gn_sock = -1, wg_family = -1;
static int rt(void) { if (rt_sock < 0) rt_sock = nl_open(NETLINK_ROUTE); return rt_sock; }
static int fam_cb(struct nlmsghdr *h, void *ctx)
{
	struct genlmsghdr *g = NLMSG_DATA(h); (void)g; int len = (int)h->nlmsg_len - NLMSG_LENGTH(GENL_HDRLEN);
	for (struct nlattr *a = (void *)((char *)NLMSG_DATA(h) + GENL_HDRLEN); len >= (int)NLA_HDRLEN && a->nla_len >= NLA_HDRLEN && a->nla_len <= len; len -= NLA_ALIGN(a->nla_len), a = (void *)((char *)a + NLA_ALIGN(a->nla_len)))
		if ((a->nla_type & NLA_TYPE_MASK) == CTRL_ATTR_FAMILY_ID) *(int *)ctx = *(uint16_t *)((char *)a + NLA_HDRLEN);
	return 0;
}
/* the kernel's "wireguard" generic netlink family (-1: the kernel has no WireGuard) */
static int wg(void)
{
	if (gn_sock < 0) gn_sock = nl_open(NETLINK_GENERIC);
	if (gn_sock < 0) return -1;
	if (wg_family >= 0) return wg_family;
	NL m; nl_start(&m, GENL_ID_CTRL, 0); struct genlmsghdr g = { .cmd = CTRL_CMD_GETFAMILY, .version = 1 }; nl_put(&m, &g, sizeof g);
	nla_str(&m, CTRL_ATTR_FAMILY_NAME, WG_GENL_NAME);
	int id = -1; struct nlmsghdr *h = (void *)m.b; h->nlmsg_len = (uint32_t)m.n; h->nlmsg_seq = ++nl_seq;
	struct sockaddr_nl k = { .nl_family = AF_NETLINK };
	if (sendto(gn_sock, m.b, m.n, 0, (struct sockaddr *)&k, sizeof k) != (ssize_t)m.n) return -1;
	static char rb[16384]; ssize_t r = recv(gn_sock, rb, sizeof rb, 0);
	for (struct nlmsghdr *q = (void *)rb; r > 0 && NLMSG_OK(q, (size_t)r); q = NLMSG_NEXT(q, r)) if (q->nlmsg_type == GENL_ID_CTRL) fam_cb(q, &id);
	wg_family = id; return id;
}
static int if_index(const char *n) { return (int)if_nametoindex(n); }
static int link_del(const char *ifn)
{
	if (!if_index(ifn)) return 0;
	NL m; nl_start(&m, RTM_DELLINK, 0); struct ifinfomsg ii = { .ifi_family = AF_UNSPEC }; nl_put(&m, &ii, sizeof ii); nla_str(&m, IFLA_IFNAME, ifn);
	return nl_talk(rt(), &m, NULL, NULL);
}
static int link_add_wg(const char *ifn)
{
	NL m; nl_start(&m, RTM_NEWLINK, NLM_F_CREATE | NLM_F_EXCL); struct ifinfomsg ii = { .ifi_family = AF_UNSPEC }; nl_put(&m, &ii, sizeof ii);
	nla_str(&m, IFLA_IFNAME, ifn); size_t li = nest_start(&m, IFLA_LINKINFO); nla_str(&m, IFLA_INFO_KIND, "wireguard"); nest_end(&m, li);
	return nl_talk(rt(), &m, NULL, NULL);
}
static int link_up(int idx, int mtu)
{
	NL m; nl_start(&m, RTM_NEWLINK, 0); struct ifinfomsg ii = { .ifi_family = AF_UNSPEC, .ifi_index = idx, .ifi_flags = IFF_UP, .ifi_change = IFF_UP }; nl_put(&m, &ii, sizeof ii);
	if (mtu) nla_u32(&m, IFLA_MTU, (uint32_t)mtu);
	return nl_talk(rt(), &m, NULL, NULL);
}
static int addr_add(int idx, const Cidr *c)
{
	NL m; nl_start(&m, RTM_NEWADDR, NLM_F_CREATE | NLM_F_REPLACE); struct ifaddrmsg a = { .ifa_family = (uint8_t)c->fam, .ifa_prefixlen = (uint8_t)c->len, .ifa_index = (uint32_t)idx, .ifa_scope = RT_SCOPE_UNIVERSE }; nl_put(&m, &a, sizeof a);
	size_t al = c->fam == AF_INET ? 4 : 16; nla(&m, IFA_LOCAL, c->a, al); nla(&m, IFA_ADDRESS, c->a, al);
	if (c->fam == AF_INET6) nla_u32(&m, IFA_FLAGS, IFA_F_NODAD);
	return nl_talk(rt(), &m, NULL, NULL);
}
/* a route: to dst via the interface idx (0: type unreachable, no interface), in table tb */
static int route(int add, int idx, const Cidr *dst, uint32_t tb)
{
	NL m; nl_start(&m, add ? RTM_NEWROUTE : RTM_DELROUTE, add ? NLM_F_CREATE | NLM_F_REPLACE : 0);
	struct rtmsg r = { .rtm_family = (uint8_t)dst->fam, .rtm_dst_len = (uint8_t)dst->len, .rtm_table = tb < 256 ? (uint8_t)tb : RT_TABLE_UNSPEC,
		.rtm_protocol = RTPROT_STATIC, .rtm_scope = idx ? RT_SCOPE_LINK : RT_SCOPE_UNIVERSE, .rtm_type = idx ? RTN_UNICAST : RTN_UNREACHABLE };
	nl_put(&m, &r, sizeof r); if (dst->len) nla(&m, RTA_DST, dst->a, dst->fam == AF_INET ? 4 : 16);
	if (idx) nla_u32(&m, RTA_OIF, (uint32_t)idx);
	nla_u32(&m, RTA_TABLE, tb);
	return nl_talk(rt(), &m, NULL, NULL);
}
/* the two rules of the whole computer's table (wg-quick's): pref VPN_PREF "table main suppress_prefixlength 0",
   VPN_PREF+1 "not fwmark VPN_MARK table VPN_TABLE" */
static int rules(int add, int fam)
{
	int rc = 0;
	for (int k = 0; k < 2; k++) {
		NL m; nl_start(&m, add ? RTM_NEWRULE : RTM_DELRULE, add ? NLM_F_CREATE | NLM_F_EXCL : 0);
		struct fib_rule_hdr f = { .family = (uint8_t)fam, .action = FR_ACT_TO_TBL, .table = k ? RT_TABLE_UNSPEC : RT_TABLE_MAIN };
		if (k) f.flags = FIB_RULE_INVERT;
		nl_put(&m, &f, sizeof f); nla_u32(&m, FRA_PRIORITY, VPN_PREF + (uint32_t)k);
		if (k) { nla_u32(&m, FRA_FWMARK, VPN_MARK); nla_u32(&m, FRA_FWMASK, 0xffffffffu); nla_u32(&m, FRA_TABLE, VPN_TABLE); }
		else { nla_u32(&m, FRA_TABLE, RT_TABLE_MAIN); nla_u32(&m, FRA_SUPPRESS_PREFIXLEN, 0); }
		int r = nl_talk(rt(), &m, NULL, NULL);
		if (r && !(add && r == -EEXIST) && !(!add && r == -ENOENT) && !rc) rc = r;
	}
	return rc;
}
/* WireGuard's own settings (WG_CMD_SET_DEVICE): the key, the port, the mark, the peers with their endpoints
   (resolved: ep4/ep6 for each), allowed IPs and keepalives */
static int wg_set(const Conf *c, const struct sockaddr_storage *ep, const socklen_t *epl, int kick)
{
	int fam = wg(); if (fam < 0) return -EOPNOTSUPP;
	NL m; nl_start(&m, fam, 0); struct genlmsghdr g = { .cmd = WG_CMD_SET_DEVICE, .version = WG_GENL_VERSION }; nl_put(&m, &g, sizeof g);
	nla_str(&m, WGDEVICE_A_IFNAME, VPN_IF);
	if (!kick) {
		nla(&m, WGDEVICE_A_PRIVATE_KEY, c->key, 32); nla_u32(&m, WGDEVICE_A_FLAGS, WGDEVICE_F_REPLACE_PEERS);
		nla_u16(&m, WGDEVICE_A_LISTEN_PORT, (uint16_t)c->listen); nla_u32(&m, WGDEVICE_A_FWMARK, VPN_MARK);
	}
	size_t ps = nest_start(&m, WGDEVICE_A_PEERS);
	for (int i = 0; i < c->npeer; i++) {
		const Peer *p = &c->peer[i]; size_t pe = nest_start(&m, i);
		nla(&m, WGPEER_A_PUBLIC_KEY, p->pub, 32);
		/* (keepalive: the file's, else every 25 s, so a NAT keeps the way back open and "connected" means the server answers) */
		uint16_t ka = (uint16_t)(p->has_keepalive && p->keepalive ? p->keepalive : 25);
		if (kick) { nla_u32(&m, WGPEER_A_FLAGS, WGPEER_F_UPDATE_ONLY); nla_u16(&m, WGPEER_A_PERSISTENT_KEEPALIVE_INTERVAL, ka); nest_end(&m, pe); continue; }
		nla_u32(&m, WGPEER_A_FLAGS, WGPEER_F_REPLACE_ALLOWEDIPS);
		if (p->has_psk) nla(&m, WGPEER_A_PRESHARED_KEY, p->psk, 32);
		if (epl[i]) nla(&m, WGPEER_A_ENDPOINT, &ep[i], epl[i]);
		size_t ai = nest_start(&m, WGPEER_A_ALLOWEDIPS);
		for (int k = 0; k < p->nips; k++) { size_t one = nest_start(&m, k); nla_u16(&m, WGALLOWEDIP_A_FAMILY, (uint16_t)p->ips[k].fam);
			nla(&m, WGALLOWEDIP_A_IPADDR, p->ips[k].a, p->ips[k].fam == AF_INET ? 4 : 16); nla_u8(&m, WGALLOWEDIP_A_CIDR_MASK, (uint8_t)p->ips[k].len); nest_end(&m, one); }
		nest_end(&m, ai); nest_end(&m, pe);
	}
	nest_end(&m, ps);
	int r = nl_talk(gn_sock, &m, NULL, NULL);
	explicit_bzero(&m, sizeof m); return r;
}
typedef struct { long long hs; unsigned long long rx, tx; int got; } WgStat;
static void wg_attrs(struct nlattr *a, int len, WgStat *st, int depth)
{
	for (; len >= (int)NLA_HDRLEN && a->nla_len >= NLA_HDRLEN && a->nla_len <= len; len -= NLA_ALIGN(a->nla_len), a = (void *)((char *)a + NLA_ALIGN(a->nla_len))) {
		int t = a->nla_type & NLA_TYPE_MASK; void *d = (char *)a + NLA_HDRLEN; int dl = a->nla_len - NLA_HDRLEN;
		if (depth == 0 && t == WGDEVICE_A_PEERS) { st->got = 1; wg_attrs(d, dl, st, 1); }
		else if (depth == 1) wg_attrs(d, dl, st, 2);   /* (one peer) */
		else if (depth == 2) {
			if (t == WGPEER_A_LAST_HANDSHAKE_TIME && dl >= 16) { long long s; memcpy(&s, d, 8); if (s > st->hs) st->hs = s; }
			else if (t == WGPEER_A_RX_BYTES && dl >= 8) { unsigned long long v; memcpy(&v, d, 8); st->rx += v; }
			else if (t == WGPEER_A_TX_BYTES && dl >= 8) { unsigned long long v; memcpy(&v, d, 8); st->tx += v; }
		}
	}
}
static int wgget_cb(struct nlmsghdr *h, void *ctx)
{
	int len = (int)h->nlmsg_len - NLMSG_LENGTH(GENL_HDRLEN); WgStat *st = ctx; st->got |= 2;
	wg_attrs((void *)((char *)NLMSG_DATA(h) + GENL_HDRLEN), len, st, 0); return 0;
}
static int wg_get(WgStat *st)
{
	memset(st, 0, sizeof *st); int fam = wg(); if (fam < 0 || !if_index(VPN_IF)) return -1;
	NL m; nl_start(&m, fam, NLM_F_DUMP); struct genlmsghdr g = { .cmd = WG_CMD_GET_DEVICE, .version = WG_GENL_VERSION }; nl_put(&m, &g, sizeof g);
	nla_str(&m, WGDEVICE_A_IFNAME, VPN_IF);
	int r = nl_talk(gn_sock, &m, wgget_cb, st);
	return r ? -1 : 0;
}

/* ------------------------------------------------------------------ the name servers */
/* the connection with the default route (the lowest metric in /proc/net/route), as udhcpc.sh's best() */
static void best_if(char *o, size_t n)
{
	*o = 0; FILE *f = fopen("/proc/net/route", "re"); if (!f) return; char l[512]; long bm = -1;
	if (!fgets(l, sizeof l, f)) { fclose(f); return; }
	while (fgets(l, sizeof l, f)) { char ifn[32]; unsigned long d, g, fl, ref, use, metric, mask; if (sscanf(l, "%31s %lx %lx %lx %lu %lu %lu %lx", ifn, &d, &g, &fl, &ref, &use, &metric, &mask) == 8 && !d && !mask && (bm < 0 || (long)metric < bm) && strcmp(ifn, VPN_IF)) { bm = (long)metric; snprintf(o, n, "%s", ifn); } }
	fclose(f);
}
static void dns_on(const Conf *c)
{
	char p[PATH_MAX], q[PATH_MAX], b[1024] = ""; size_t k = 0;
	if (!c->ndns) return;
	for (int i = 0; i < c->ndns && k < sizeof b - 64; i++) k += (size_t)snprintf(b + k, sizeof b - k, "nameserver %s\n", c->dns[i]);
	if (*c->search && k < sizeof b - 300) k += (size_t)snprintf(b + k, sizeof b - k, "search %s\n", c->search);
	snprintf(p, sizeof p, "%s/resolv.conf", ETC); snprintf(q, sizeof q, "%s/vpn/resolv.before", RUNDIR);
	size_t ol = 0; char *old = slurp_any(p, 65536, &ol, 1);
	char sv[PATH_MAX]; snprintf(sv, sizeof sv, "%s/dns-vpn", RUNDIR);
	if (old && access(sv, F_OK)) put_file(q, old, ol, 0600);   /* (the network's own, from before the first VPN) */
	free(old);
	put_file(sv, b, k, 0644); put_inplace(p, b);
}
static void dns_off(void)
{
	char p[PATH_MAX], q[PATH_MAX], sv[PATH_MAX], bi[32]; snprintf(p, sizeof p, "%s/resolv.conf", ETC); snprintf(q, sizeof q, "%s/vpn/resolv.before", RUNDIR); snprintf(sv, sizeof sv, "%s/dns-vpn", RUNDIR);
	if (access(sv, F_OK)) return;
	unlink(sv);
	/* the connection in use now (it may have changed while the VPN was on): its own servers, as udhcpc.sh keeps them */
	best_if(bi, sizeof bi); char d[PATH_MAX]; snprintf(d, sizeof d, "%s/dns/%s", RUNDIR, bi);
	char *t = *bi && strchr(bi, '/') == NULL ? slurp(d, 65536, NULL) : NULL; if (!t) t = slurp(q, 65536, NULL);
	if (t) put_inplace(p, t);
	free(t); unlink(q);
}
static void proc_set(const char *rel, const char *v, char *old, size_t on)
{
	char p[PATH_MAX]; snprintf(p, sizeof p, "%s/%s", PROCNET, rel);
	if (old) { char *t = slurp(p, 64, NULL); snprintf(old, on, "%s", t ? t : ""); if (t) free(t); old[strcspn(old, "\n")] = 0; }
	int f = open(p, O_WRONLY | O_CLOEXEC); if (f >= 0) { if (write(f, v, strlen(v)) < 0) {} close(f); }
}

/* ------------------------------------------------------------------ up and down */
static int lock_fd = -1;
static void lock_take(void) { char p[PATH_MAX]; snprintf(p, sizeof p, "%s/vpn/lock", RUNDIR); lock_fd = open(p, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600); if (lock_fd >= 0) flock(lock_fd, LOCK_EX); }
static char *conf_text(const char *id, size_t *len) { char p[PATH_MAX]; meta_path(id, "conf", p, sizeof p); return slurp(p, MAXCONF, len); }
/* RUN/vpn/state: what up added, so down removes exactly that: "rules4 0|1", "rules6 0|1", "halves6 0|1", "block6 0|1",
   "svm OLD" */
static void state_write(int r4, int r6, int h6, int b6, const char *svm)
{
	char p[PATH_MAX], b[200]; snprintf(p, sizeof p, "%s/vpn/state", RUNDIR);
	int n = snprintf(b, sizeof b, "rules4 %d\nrules6 %d\nhalves6 %d\nblock6 %d\nsvm %s\n", r4, r6, h6, b6, svm && *svm ? svm : "-"); put_file(p, b, (size_t)n, 0600);
}
static void teardown(void)
{
	char p[PATH_MAX]; snprintf(p, sizeof p, "%s/vpn/state", RUNDIR); char *t = slurp(p, 1024, NULL);
	int r4 = 1, r6 = 1, h6 = 1, b6 = 1; char svm[16] = "";
	if (t) { char *q; if ((q = strstr(t, "rules4 "))) r4 = atoi(q + 7); if ((q = strstr(t, "rules6 "))) r6 = atoi(q + 7); if ((q = strstr(t, "halves6 "))) h6 = atoi(q + 8);
		if ((q = strstr(t, "block6 "))) b6 = atoi(q + 7); if ((q = strstr(t, "svm "))) sscanf(q + 4, "%15s", svm); free(t); }
	link_del(VPN_IF);   /* (its addresses and its routes go with it) */
	if (r4) rules(0, AF_INET);
	if (r6) rules(0, AF_INET6);
	Cidr a = { .fam = AF_INET6, .len = 1 }, b = { .fam = AF_INET6, .len = 1 }; b.a[0] = 0x80;
	if (h6 || b6) { route(0, 0, &a, RT_TABLE_MAIN); route(0, 0, &b, RT_TABLE_MAIN); }
	if (*svm && strcmp(svm, "-")) proc_set("ipv4/conf/all/src_valid_mark", svm, NULL, 0);
	dns_off(); unlink(p);
}
/* the endpoint of each peer, resolved now (its name looked up through the network in use) */
static int resolve(const Conf *c, struct sockaddr_storage *ep, socklen_t *epl, char *eps, size_t en, char *why, size_t wn)
{
	*eps = 0; size_t k = 0;
	for (int i = 0; i < c->npeer; i++) {
		epl[i] = 0; if (!c->peer[i].host[0]) continue;
		char ps[8]; snprintf(ps, sizeof ps, "%d", c->peer[i].port);
		struct addrinfo h = { .ai_socktype = SOCK_DGRAM, .ai_flags = AI_NUMERICSERV }, *res = NULL, *pick = NULL;
		int e = getaddrinfo(c->peer[i].host, ps, &h, &res);
		if (e || !res) { if (!i) { snprintf(why, wn, "the VPN's server %.100s could not be found (%s): is this computer online?", c->peer[i].host, gai_strerror(e)); return -1; } continue; }
		for (struct addrinfo *r = res; r && !pick; r = r->ai_next) if (r->ai_family == AF_INET) pick = r;
		for (struct addrinfo *r = res; r && !pick; r = r->ai_next) if (r->ai_family == AF_INET6) pick = r;
		if (pick) { memcpy(&ep[i], pick->ai_addr, pick->ai_addrlen); epl[i] = pick->ai_addrlen;
			char s[64] = ""; if (pick->ai_family == AF_INET) inet_ntop(AF_INET, &((struct sockaddr_in *)pick->ai_addr)->sin_addr, s, sizeof s); else inet_ntop(AF_INET6, &((struct sockaddr_in6 *)pick->ai_addr)->sin6_addr, s, sizeof s);
			if (k + strlen(s) + 2 < en) k += (size_t)snprintf(eps + k, en - k, "%s%s", k ? "," : "", s); }
		freeaddrinfo(res);
	}
	return 0;
}
static int wg_up(const char *id, Conf *c, char *why, size_t wn)
{
	struct sockaddr_storage ep[MAXPEERS]; socklen_t epl[MAXPEERS]; char eps[300];
	if (wg() < 0) { snprintf(why, wn, "this kernel has no WireGuard"); return -1; }
	dns_off();   /* (the network's own name servers to find the server: a VPN's own cannot answer while its tunnel is down) */
	if (resolve(c, ep, epl, eps, sizeof eps, why, wn)) return -1;
	link_del(VPN_IF);
	int r = link_add_wg(VPN_IF); if (r) { snprintf(why, wn, "the VPN's interface could not be made (%s)", strerror(-r)); return -1; }
	int idx = if_index(VPN_IF); if (!idx) { snprintf(why, wn, "the VPN's interface is not there"); return -1; }
	r = wg_set(c, ep, epl, 0); if (r) { snprintf(why, wn, "WireGuard did not take the settings (%s)", strerror(-r)); link_del(VPN_IF); return -1; }
	for (int i = 0; i < c->naddr; i++) if ((r = addr_add(idx, &c->addr[i]))) { snprintf(why, wn, "an address could not be set (%s)", strerror(-r)); link_del(VPN_IF); return -1; }
	int mtu = c->mtu ? c->mtu : 1420;
	for (int i = 0; i < c->naddr; i++) if (c->addr[i].fam == AF_INET6 && mtu < 1280) mtu = 1280;
	if ((r = link_up(idx, mtu))) { snprintf(why, wn, "the VPN's interface did not come up (%s)", strerror(-r)); link_del(VPN_IF); return -1; }
	int f4 = full4(c), f6 = full6(c), r4 = 0, r6 = 0, h6 = 0, b6 = 0; char svm[16] = "";
	/* the routes of the allowed IPs: the whole computer's in a table of its own, the others in the main table */
	for (int i = 0; i < c->npeer; i++) for (int k = 0; k < c->peer[i].nips; k++) {
		const Cidr *a = &c->peer[i].ips[k];
		if (!a->len) { if ((r = route(1, idx, a, VPN_TABLE))) { snprintf(why, wn, "the VPN's route could not be set (%s)", strerror(-r)); goto fail; } }
		else if ((r = route(1, idx, a, RT_TABLE_MAIN)) && r != -EEXIST) { snprintf(why, wn, "a route of the VPN could not be set (%s)", strerror(-r)); goto fail; }
	}
	if (f4) { if ((r = rules(1, AF_INET))) { snprintf(why, wn, "the VPN's routing rules could not be set (%s)", strerror(-r)); goto fail; } r4 = 1; proc_set("ipv4/conf/all/src_valid_mark", "1", svm, sizeof svm); }
	if (f6) {
		if (!rules(1, AF_INET6)) r6 = 1;
		else { /* (a kernel without IPv6 policy routing: the two halves of IPv6 into the tunnel) */
			rules(0, AF_INET6); Cidr a = { .fam = AF_INET6, .len = 1 }, b = { .fam = AF_INET6, .len = 1 }; b.a[0] = 0x80;
			if ((r = route(1, idx, &a, RT_TABLE_MAIN)) || (r = route(1, idx, &b, RT_TABLE_MAIN))) { snprintf(why, wn, "the VPN's IPv6 routes could not be set (%s)", strerror(-r)); goto fail; }
			h6 = 1; }
	} else if (f4 && !any6(c)) {
		/* the whole of IPv4 but no IPv6: IPv6 unreachable while it is on, so nothing goes round the tunnel by it */
		Cidr a = { .fam = AF_INET6, .len = 1 }, b = { .fam = AF_INET6, .len = 1 }; b.a[0] = 0x80;
		if (!route(1, 0, &a, RT_TABLE_MAIN) && !route(1, 0, &b, RT_TABLE_MAIN)) b6 = 1;
	}
	state_write(r4, r6, h6, b6, svm);
	dns_on(c);
	/* the first handshake now (WireGuard sends one when a keepalive is set on a running interface) */
	wg_set(c, ep, epl, 1);
	return 0;
fail:
	state_write(r4, r6, h6, b6, svm); teardown(); return -1;
}
static const char *state_now(WgStat *st)
{
	if (!if_index(VPN_IF) || wg_get(st)) return "down";
	long long now = time(NULL);
	if (st->hs && now - st->hs <= 185) return "connected";
	if (st->hs) return "noanswer";
	return "connecting";
}
static int cmd_down(int quiet)
{
	OnState o; int was = on_read(&o);
	teardown(); on_clear();
	if (was && !quiet) { Meta m; if (!meta_read(o.id, &m)) rr_health("vpn", "the VPN %s was turned off", m.name); }
	vpn_event(); return 0;
}
static int cmd_up(const char *id, const char *by)
{
	Meta m; if (meta_read(id, &m)) { printf("{\"ok\":false,\"error\":\"there is no such VPN\"}\n"); return 1; }
	if (strcmp(m.type, "wireguard")) { printf("{\"ok\":false,\"error\":\"this kind of VPN cannot be connected yet\"}\n"); return 1; }
	OnState o; if (on_read(&o)) { teardown(); }
	size_t len = 0; char *t = conf_text(id, &len); if (!t) { printf("{\"ok\":false,\"error\":\"the VPN's file is missing\"}\n"); return 1; }
	Conf c; char why[300] = ""; int bad = conf_parse(t, len, &c, why, sizeof why); explicit_bzero(t, len); free(t);
	if (bad) { printf("{\"ok\":false,\"error\":"); jstr(stdout, why); printf("}\n"); return 1; }
	on_write(id, by);   /* (meant to be on from now: vpn tick sets it up again should it go) */
	int r = wg_up(id, &c, why, sizeof why); conf_wipe(&c);
	if (r) { teardown(); on_clear(); rr_health("vpn", "the VPN %s did not connect: %s", m.name, why); vpn_event(); printf("{\"ok\":false,\"error\":"); jstr(stdout, why); printf("}\n"); return 1; }
	WgStat st; const char *s = "connecting";
	for (int i = 0; i < 40; i++) { s = state_now(&st); if (!strcmp(s, "connected")) break; struct timespec ts = { 0, 200000000 }; nanosleep(&ts, NULL); }
	rr_health("vpn", "the VPN %s was turned on%s%s", m.name, by && strcmp(by, "-") ? " by " : "", by && strcmp(by, "-") ? by : "");
	vpn_event();
	printf("{\"ok\":true,\"state\":\"%s\",\"message\":", s);
	jstr(stdout, !strcmp(s, "connected") ? "Connected: this computer's traffic goes through the VPN." : "On: waiting for the VPN's server to answer (it keeps trying).");
	printf("}\n"); return 0;
}
static void list_one(FILE *o, const Meta *m, const OnState *on, int first)
{
	size_t len = 0; char *t = conf_text(m->id, &len); Conf c; char why[200]; int ok = t && !conf_parse(t, len, &c, why, sizeof why); if (t) { explicit_bzero(t, len); free(t); }
	fprintf(o, "%s{\"id\":\"%s\",\"name\":", first ? "" : ",", m->id); jstr(o, m->name);
	fprintf(o, ",\"type\":\"%s\",\"policy\":\"%s\",\"added\":%lld", m->type, m->policy, m->added);
	if (ok) {
		char s[300]; snprintf(s, sizeof s, "%s:%d", c.peer[0].host, c.peer[0].port); if (strchr(c.peer[0].host, ':')) snprintf(s, sizeof s, "[%s]:%d", c.peer[0].host, c.peer[0].port);
		fprintf(o, ",\"server\":"); jstr(o, s);
		fprintf(o, ",\"addresses\":["); for (int i = 0; i < c.naddr; i++) { char a[64]; inet_ntop(c.addr[i].fam, c.addr[i].a, a, sizeof a); fprintf(o, "%s\"%s/%d\"", i ? "," : "", a, c.addr[i].len); }
		fprintf(o, "],\"dns\":["); for (int i = 0; i < c.ndns; i++) fprintf(o, "%s\"%s\"", i ? "," : "", c.dns[i]);
		fprintf(o, "],\"whole\":%s", full4(&c) || full6(&c) ? "true" : "false");
		conf_wipe(&c);
	} else fprintf(o, ",\"broken\":true");
	if (on && !strcmp(on->id, m->id)) {
		WgStat st; const char *s = state_now(&st);
		fprintf(o, ",\"on\":true,\"state\":\"%s\",\"since\":%lld,\"by\":", s, on->since); jstr(o, strcmp(on->by, "-") ? on->by : "");
		fprintf(o, ",\"rx\":%llu,\"tx\":%llu,\"handshake\":%lld", st.rx, st.tx, st.hs);
	} else fprintf(o, ",\"on\":false,\"state\":\"off\"");
	fputc('}', o);
}
static int ids(char out[][16], int max)
{
	DIR *d = opendir(VPN_DIR); int n = 0; struct dirent *e; if (!d) return 0;
	while ((e = readdir(d)) && n < max) { size_t L = strlen(e->d_name); if (L == 14 && !strcmp(e->d_name + 9, ".meta")) { char id[16]; snprintf(id, sizeof id, "%.9s", e->d_name); if (id_ok(id)) snprintf(out[n++], 16, "%s", id); } }
	closedir(d);
	for (int i = 1; i < n; i++) for (int k = i; k > 0; k--) { Meta a, b; meta_read(out[k - 1], &a); meta_read(out[k], &b); if (a.added > b.added || (a.added == b.added && strcmp(out[k - 1], out[k]) > 0)) { char t[16]; memcpy(t, out[k], 16); memcpy(out[k], out[k - 1], 16); memcpy(out[k - 1], t, 16); } else break; }
	return n;
}
static int cmd_list(void)
{
	char l[64][16]; int n = ids(l, 64); OnState on; int has = on_read(&on);
	printf("{\"ok\":true,\"wireguard\":%s,\"vpns\":[", wg() >= 0 ? "true" : "false");
	for (int i = 0; i < n; i++) { Meta m; if (!meta_read(l[i], &m)) list_one(stdout, &m, has ? &on : NULL, i == 0); }
	printf("],\"active\":"); if (has) printf("\"%s\"", on.id); else printf("null");
	printf("}\n"); return 0;
}
static int cmd_add(const char *type)
{
	if (strcmp(type, "wireguard")) return refuse("a kind of VPN that is not wireguard", type);
	size_t len = 0; char *in = read_all(0, MAXCONF + 200, &len);
	if (!in) { printf("{\"ok\":false,\"error\":\"the file is over 64 KB: it is not a WireGuard file\"}\n"); return 1; }
	char *nl = strchr(in, '\n'); if (!nl) { explicit_bzero(in, len); free(in); return refuse("a VPN without its name", ""); }
	*nl = 0; char *name = in, *conf = nl + 1; size_t cl = len - (size_t)(conf - in);
	if (!utf8_name_ok(name, 64)) { explicit_bzero(in, len); free(in); printf("{\"ok\":false,\"error\":\"the name has 1 to 64 characters\"}\n"); return 1; }
	Conf c; char why[300] = "";
	if (conf_parse(conf, cl, &c, why, sizeof why)) { explicit_bzero(in, len); free(in); printf("{\"ok\":false,\"error\":"); jstr(stdout, why); printf("}\n"); return 1; }
	char l[64][16]; if (ids(l, 64) >= 32) { conf_wipe(&c); explicit_bzero(in, len); free(in); printf("{\"ok\":false,\"error\":\"there are 32 VPNs already: remove one first\"}\n"); return 1; }
	mkdirs();
	Meta m; memset(&m, 0, sizeof m); uint8_t rb[4]; if (getrandom(rb, 4, 0) != 4) { conf_wipe(&c); free(in); return 1; }
	snprintf(m.id, sizeof m.id, "v%02x%02x%02x%02x", rb[0], rb[1], rb[2], rb[3]); snprintf(m.name, sizeof m.name, "%s", name);
	snprintf(m.type, sizeof m.type, "wireguard"); snprintf(m.policy, sizeof m.policy, "everyone"); m.added = (long long)time(NULL);
	char p[PATH_MAX]; meta_path(m.id, "conf", p, sizeof p);
	int bad = put_file(p, conf, cl, 0600) || meta_write(&m);
	explicit_bzero(in, len); free(in);
	if (bad) { conf_wipe(&c); meta_path(m.id, "conf", p, sizeof p); unlink(p); printf("{\"ok\":false,\"error\":\"the VPN could not be kept (the disk)\"}\n"); return 1; }
	printf("{\"ok\":true,\"id\":\"%s\",\"notes\":[", m.id); for (int i = 0; i < c.nnotes; i++) { if (i) putchar(','); jstr(stdout, c.notes[i]); } printf("]}\n");
	rr_health("vpn", "the VPN %s was added", m.name);
	conf_wipe(&c); vpn_event(); return 0;
}
static int cmd_set(const char *id, const char *key, const char *val)
{
	Meta m; if (meta_read(id, &m)) { printf("{\"ok\":false,\"error\":\"there is no such VPN\"}\n"); return 1; }
	if (!strcmp(key, "name")) {
		size_t n = 0; char *in = read_all(0, 200, &n); if (!in) return refuse("a name over 200 bytes", "");
		in[strcspn(in, "\n")] = 0; if (!utf8_name_ok(in, 64)) { free(in); printf("{\"ok\":false,\"error\":\"the name has 1 to 64 characters\"}\n"); return 1; }
		snprintf(m.name, sizeof m.name, "%s", in); free(in);
	} else if (!strcmp(key, "policy")) {
		if (!val || (strcmp(val, "everyone") && strcmp(val, "admins") && strcmp(val, "always"))) return refuse("a policy that is not everyone, admins or always", val);
		snprintf(m.policy, sizeof m.policy, "%s", val);
	} else return refuse("a setting that is not name or policy", key);
	if (meta_write(&m)) { printf("{\"ok\":false,\"error\":\"the setting could not be kept\"}\n"); return 1; }
	vpn_event(); printf("{\"ok\":true}\n"); return 0;
}
static int cmd_remove(const char *id)
{
	Meta m; if (meta_read(id, &m)) { printf("{\"ok\":false,\"error\":\"there is no such VPN\"}\n"); return 1; }
	OnState o; if (on_read(&o) && !strcmp(o.id, id)) cmd_down(1);
	char p[PATH_MAX]; meta_path(id, "conf", p, sizeof p);
	{ size_t n = 0; char *t = slurp(p, MAXCONF, &n); if (t) { int f = open(p, O_WRONLY | O_CLOEXEC | O_NOFOLLOW); if (f >= 0) { explicit_bzero(t, n); if (write(f, t, n) < 0) {} if (fsync(f)) {} close(f); } free(t); } }   /* (the key overwritten first) */
	unlink(p); meta_path(id, "meta", p, sizeof p); unlink(p);
	rr_health("vpn", "the VPN %s was removed", m.name); vpn_event(); printf("{\"ok\":true}\n"); return 0;
}
/* an "Always on" VPN, turned on at the start (the first one, if more than one says so) */
static int cmd_boot(void)
{
	char l[64][16]; int n = ids(l, 64);
	for (int i = 0; i < n; i++) { Meta m; if (!meta_read(l[i], &m) && !strcmp(m.policy, "always")) { int f = open("/dev/null", O_WRONLY | O_CLOEXEC); int sv = dup(1); if (f >= 0) { dup2(f, 1); close(f); } int r = cmd_up(m.id, "-"); if (sv >= 0) { dup2(sv, 1); close(sv); } return r; } }
	return 0;
}
/* while one is meant to be on: its interface back if it is gone (it could not resolve its server at the start, say) */
static int cmd_tick(void)
{
	OnState o; if (!on_read(&o)) return 0;
	if (if_index(VPN_IF)) return 0;
	Meta m; if (meta_read(o.id, &m)) { on_clear(); return 0; }
	size_t len = 0; char *t = conf_text(o.id, &len); if (!t) return 0;
	Conf c; char why[300] = ""; int bad = conf_parse(t, len, &c, why, sizeof why); explicit_bzero(t, len); free(t); if (bad) return 0;
	int r = wg_up(o.id, &c, why, sizeof why); conf_wipe(&c);
	if (!r) { rr_health("vpn", "the VPN %s was set up again", m.name); vpn_event(); }
	return 0;
}

#ifndef VPN_NO_MAIN   /* (the fuzz test, qt/host/fuzz/vpn_fuzz.c, has its own) */
int main(int argc, char **argv)
{
	if (argc < 2) { fprintf(stderr, "usage: vpn list|add|set|remove|up|down|boot|tick ...\n"); return 2; }
	signal(SIGPIPE, SIG_IGN); umask(077);
	const char *op = argv[1]; int n = argc - 2; char **a = argv + 2;
	if (!strcmp(op, "list")) { if (n) return refuse("arguments for list", a[0]); return cmd_list(); }
	mkdirs(); lock_take();
	if (!strcmp(op, "add")) { if (n != 1) return refuse("add without its kind", ""); return cmd_add(a[0]); }
	if (!strcmp(op, "set")) { if (n < 2 || n > 3 || !id_ok(a[0])) return refuse("a VPN that is not one", n ? a[0] : ""); return cmd_set(a[0], a[1], n == 3 ? a[2] : NULL); }
	if (!strcmp(op, "remove")) { if (n != 1 || !id_ok(a[0])) return refuse("a VPN that is not one", n ? a[0] : ""); return cmd_remove(a[0]); }
	if (!strcmp(op, "up")) { if (n != 2 || !id_ok(a[0]) || (strcmp(a[1], "-") && !utf8_name_ok(a[1], 64))) return refuse("a VPN or a name that is not one", n ? a[0] : ""); return cmd_up(a[0], a[1]); }
	if (!strcmp(op, "down")) { if (n) return refuse("arguments for down", a[0]); cmd_down(0); printf("{\"ok\":true}\n"); return 0; }
	if (!strcmp(op, "boot")) { if (n) return refuse("arguments for boot", a[0]); return cmd_boot(); }
	if (!strcmp(op, "tick")) { if (n) return refuse("arguments for tick", a[0]); return cmd_tick(); }
	return refuse("an unknown action", op);
}
#endif
