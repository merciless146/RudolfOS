/* rudolf-verity - check the system layers as they are read (Rudolf OS 1.2.2). GPL-2.0-or-later.
 *
 * Runs in the initramfs, which is part of the signed kernel. The release list (manifest.txt) is signed with the
 * Rudolf OS release key (Ed25519, the keys in /etc/rudolf-keys, from iso/trusted-keys.txt); for each layer it
 * holds "V <name> <data blocks> <root hash> <salt>" (systools/verity.py). This program checks that signature and
 * maps a layer through dm-verity, so every block the system reads is checked against the signed root hash.
 *
 *   rudolf-verity check MANIFEST SIG KEYS              exit 0: signed by a trusted key
 *   rudolf-verity open MANIFEST SIG KEYS NAME LOOPDEV  prints /dev/dm-N, the checked device
 * Exit codes: 0 ok, 2 not signed, 3 no V line for that layer (an older release), 4 the device could not be
 * set up, 1 usage. Ed25519 verification follows TweetNaCl (public domain; Bernstein, van Gastel, Janssen,
 * Lange, Schwabe, Smetsers). Static, no libraries. */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <linux/dm-ioctl.h>

typedef uint8_t u8; typedef uint64_t u64; typedef int64_t i64; typedef i64 gf[16];

/* ---------------------------------------------------------------- SHA-512 */
static u64 R(u64 x, int c) { return (x >> c) | (x << (64 - c)); }
static u64 dl64(const u8 *x) { u64 u = 0; for (int i = 0; i < 8; i++) u = (u << 8) | x[i]; return u; }
static void ts64(u8 *x, u64 u) { for (int i = 7; i >= 0; --i) { x[i] = u; u >>= 8; } }
static const u64 K[80] = {
	0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL, 0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
	0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL, 0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
	0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL, 0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
	0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL, 0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
	0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL, 0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
	0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
	0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
	0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL, 0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
	0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
	0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL};
static void blocks(u8 *x, const u8 *m, u64 n)
{
	u64 z[8], b[8], a[8], w[16], t;
	for (int i = 0; i < 8; i++) z[i] = a[i] = dl64(x + 8 * i);
	while (n >= 128) {
		for (int i = 0; i < 16; i++) w[i] = dl64(m + 8 * i);
		for (int i = 0; i < 80; i++) {
			for (int j = 0; j < 8; j++) b[j] = a[j];
			t = a[7] + (R(a[4], 14) ^ R(a[4], 18) ^ R(a[4], 41)) + ((a[4] & a[5]) ^ (~a[4] & a[6])) + K[i] + w[i % 16];
			b[7] = t + (R(a[0], 28) ^ R(a[0], 34) ^ R(a[0], 39)) + ((a[0] & a[1]) ^ (a[0] & a[2]) ^ (a[1] & a[2]));
			b[3] += t;
			for (int j = 0; j < 8; j++) a[(j + 1) % 8] = b[j];
			if (i % 16 == 15)
				for (int j = 0; j < 16; j++)
					w[j] += w[(j + 9) % 16] + (R(w[(j + 1) % 16], 1) ^ R(w[(j + 1) % 16], 8) ^ (w[(j + 1) % 16] >> 7)) + (R(w[(j + 14) % 16], 19) ^ R(w[(j + 14) % 16], 61) ^ (w[(j + 14) % 16] >> 6));
		}
		for (int i = 0; i < 8; i++) { a[i] += z[i]; z[i] = a[i]; }
		m += 128; n -= 128;
	}
	for (int i = 0; i < 8; i++) ts64(x + 8 * i, z[i]);
}
static const u8 IV[64] = {
	0x6a, 0x09, 0xe6, 0x67, 0xf3, 0xbc, 0xc9, 0x08, 0xbb, 0x67, 0xae, 0x85, 0x84, 0xca, 0xa7, 0x3b, 0x3c, 0x6e, 0xf3, 0x72, 0xfe, 0x94, 0xf8, 0x2b,
	0xa5, 0x4f, 0xf5, 0x3a, 0x5f, 0x1d, 0x36, 0xf1, 0x51, 0x0e, 0x52, 0x7f, 0xad, 0xe6, 0x82, 0xd1, 0x9b, 0x05, 0x68, 0x8c, 0x2b, 0x3e, 0x6c, 0x1f,
	0x1f, 0x83, 0xd9, 0xab, 0xfb, 0x41, 0xbd, 0x6b, 0x5b, 0xe0, 0xcd, 0x19, 0x13, 0x7e, 0x21, 0x79};
static void sha512(u8 *out, const u8 *m, u64 n)
{
	u8 h[64], x[256]; u64 b = n;
	memcpy(h, IV, 64);
	blocks(h, m, n); m += n - n % 128; n %= 128;
	memset(x, 0, 256); memcpy(x, m, n); x[n] = 128;
	n = 256 - 128 * (n < 112);
	x[n - 9] = b >> 61; ts64(x + n - 8, b << 3);
	blocks(h, x, n);
	memcpy(out, h, 64);
}

/* ---------------------------------------------------------------- field and group arithmetic */
static const gf gf0, gf1 = {1}, D2 = {0xf159, 0x26b2, 0x9b94, 0xebd6, 0xb156, 0x8283, 0x149a, 0x00e0, 0xd130, 0xeef3, 0x80f2, 0x198e, 0xfce7, 0x56df, 0xd9dc, 0x2406},
	Dc = {0x78a3, 0x1359, 0x4dca, 0x75eb, 0xd8ab, 0x4141, 0x0a4d, 0x0070, 0xe898, 0x7779, 0x4079, 0x8cc7, 0xfe73, 0x2b6f, 0x6cee, 0x5203},
	X = {0xd51a, 0x8f25, 0x2d60, 0xc956, 0xa7b2, 0x9525, 0xc760, 0x692c, 0xdc5c, 0xfdd6, 0xe231, 0xc0a4, 0x53fe, 0xcd6e, 0x36d3, 0x2169},
	Y = {0x6658, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666},
	I = {0xa0b0, 0x4a0e, 0x1b27, 0xc4ee, 0xe478, 0xad2f, 0x1806, 0x2f43, 0xd7a7, 0x3dfb, 0x0099, 0x2b4d, 0xdf0b, 0x4fc1, 0x2480, 0x2b83};
static void set25519(gf r, const gf a) { for (int i = 0; i < 16; i++) r[i] = a[i]; }
static void car25519(gf o)
{
	for (int i = 0; i < 16; i++) {
		o[i] += (1LL << 16);
		i64 c = o[i] >> 16;
		o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15);
		o[i] -= c << 16;
	}
}
static void sel25519(gf p, gf q, int b)
{
	i64 c = ~(b - 1);
	for (int i = 0; i < 16; i++) { i64 t = c & (p[i] ^ q[i]); p[i] ^= t; q[i] ^= t; }
}
static void pack25519(u8 *o, const gf n)
{
	gf m, t; int b;
	for (int i = 0; i < 16; i++) t[i] = n[i];
	car25519(t); car25519(t); car25519(t);
	for (int j = 0; j < 2; j++) {
		m[0] = t[0] - 0xffed;
		for (int i = 1; i < 15; i++) { m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1); m[i - 1] &= 0xffff; }
		m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
		b = (m[15] >> 16) & 1;
		m[14] &= 0xffff;
		sel25519(t, m, 1 - b);
	}
	for (int i = 0; i < 16; i++) { o[2 * i] = t[i] & 0xff; o[2 * i + 1] = t[i] >> 8; }
}
static int vn(const u8 *x, const u8 *y, int n) { u64 d = 0; for (int i = 0; i < n; i++) d |= x[i] ^ y[i]; return (1 & ((d - 1) >> 8)) - 1; }
static int neq25519(const gf a, const gf b) { u8 c[32], d[32]; pack25519(c, a); pack25519(d, b); return vn(c, d, 32); }
static u8 par25519(const gf a) { u8 d[32]; pack25519(d, a); return d[0] & 1; }
static void unpack25519(gf o, const u8 *n) { for (int i = 0; i < 16; i++) o[i] = n[2 * i] + ((i64)n[2 * i + 1] << 8); o[15] &= 0x7fff; }
static void A(gf o, const gf a, const gf b) { for (int i = 0; i < 16; i++) o[i] = a[i] + b[i]; }
static void Z(gf o, const gf a, const gf b) { for (int i = 0; i < 16; i++) o[i] = a[i] - b[i]; }
static void M(gf o, const gf a, const gf b)
{
	i64 t[31];
	for (int i = 0; i < 31; i++) t[i] = 0;
	for (int i = 0; i < 16; i++) for (int j = 0; j < 16; j++) t[i + j] += a[i] * b[j];
	for (int i = 0; i < 15; i++) t[i] += 38 * t[i + 16];
	for (int i = 0; i < 16; i++) o[i] = t[i];
	car25519(o); car25519(o);
}
static void S(gf o, const gf a) { M(o, a, a); }
static void inv25519(gf o, const gf i)
{
	gf c; set25519(c, i);
	for (int a = 253; a >= 0; a--) { S(c, c); if (a != 2 && a != 4) M(c, c, i); }
	set25519(o, c);
}
static void pow2523(gf o, const gf i)
{
	gf c; set25519(c, i);
	for (int a = 250; a >= 0; a--) { S(c, c); if (a != 1) M(c, c, i); }
	set25519(o, c);
}
static void add(gf p[4], gf q[4])
{
	gf a, b, c, d, t, e, f, g, h;
	Z(a, p[1], p[0]); Z(t, q[1], q[0]); M(a, a, t);
	A(b, p[0], p[1]); A(t, q[0], q[1]); M(b, b, t);
	M(c, p[3], q[3]); M(c, c, D2);
	M(d, p[2], q[2]); A(d, d, d);
	Z(e, b, a); Z(f, d, c); A(g, d, c); A(h, b, a);
	M(p[0], e, f); M(p[1], h, g); M(p[2], g, f); M(p[3], e, h);
}
static void cswap(gf p[4], gf q[4], u8 b) { for (int i = 0; i < 4; i++) sel25519(p[i], q[i], b); }
static void pack(u8 *r, gf p[4])
{
	gf tx, ty, zi;
	inv25519(zi, p[2]); M(tx, p[0], zi); M(ty, p[1], zi);
	pack25519(r, ty); r[31] ^= par25519(tx) << 7;
}
static void scalarmult(gf p[4], gf q[4], const u8 *s)
{
	set25519(p[0], gf0); set25519(p[1], gf1); set25519(p[2], gf1); set25519(p[3], gf0);
	for (int i = 255; i >= 0; --i) {
		u8 b = (s[i / 8] >> (i & 7)) & 1;
		cswap(p, q, b); add(q, p); add(p, p); cswap(p, q, b);
	}
}
static void scalarbase(gf p[4], const u8 *s)
{
	gf q[4];
	set25519(q[0], X); set25519(q[1], Y); set25519(q[2], gf1); M(q[3], X, Y);
	scalarmult(p, q, s);
}
static const u64 L[32] = {0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x10};
static void modL(u8 *r, i64 x[64])
{
	i64 carry; int i, j;
	for (i = 63; i >= 32; --i) {
		carry = 0;
		for (j = i - 32; j < i - 12; ++j) { x[j] += carry - 16 * x[i] * L[j - (i - 32)]; carry = (x[j] + 128) >> 8; x[j] -= carry << 8; }
		x[j] += carry; x[i] = 0;
	}
	carry = 0;
	for (j = 0; j < 32; j++) { x[j] += carry - (x[31] >> 4) * L[j]; carry = x[j] >> 8; x[j] &= 255; }
	for (j = 0; j < 32; j++) x[j] -= carry * L[j];
	for (i = 0; i < 32; i++) { x[i + 1] += x[i] >> 8; r[i] = x[i] & 255; }
}
static void reduce(u8 *r) { i64 x[64]; for (int i = 0; i < 64; i++) x[i] = (u64)r[i]; for (int i = 0; i < 64; i++) r[i] = 0; modL(r, x); }
static int unpackneg(gf r[4], const u8 p[32])
{
	gf t, chk, num, den, den2, den4, den6;
	set25519(r[2], gf1); unpack25519(r[1], p);
	S(num, r[1]); M(den, num, Dc); Z(num, num, r[2]); A(den, r[2], den);
	S(den2, den); S(den4, den2); M(den6, den4, den2); M(t, den6, num); M(t, t, den);
	pow2523(t, t); M(t, t, num); M(t, t, den); M(t, t, den); M(r[0], t, den);
	S(chk, r[0]); M(chk, chk, den);
	if (neq25519(chk, num)) M(r[0], r[0], I);
	S(chk, r[0]); M(chk, chk, den);
	if (neq25519(chk, num)) return -1;
	if (par25519(r[0]) == (p[31] >> 7)) Z(r[0], gf0, r[0]);
	M(r[3], r[0], r[1]);
	return 0;
}
/* 1 if sig is the Ed25519 signature of msg by public key pk */
static int ed25519_verify(const u8 *sig, const u8 *msg, size_t n, const u8 *pk)
{
	gf p[4], q[4]; u8 t[32], h[64];
	if (sig[63] & 0xe0) return 0;      /* S must be below 2^253 (and the check below rejects S >= L) */
	{ /* S < L */
		int lt = 0;
		for (int i = 31; i >= 0; i--) { if (sig[32 + i] < L[i]) { lt = 1; break; } if (sig[32 + i] > L[i]) break; }
		if (!lt) return 0;
	}
	if (unpackneg(q, pk)) return 0;
	u8 *m = malloc(n + 64); if (!m) return 0;
	memcpy(m, sig, 32); memcpy(m + 32, pk, 32); memcpy(m + 64, msg, n);
	sha512(h, m, n + 64); free(m);
	reduce(h);
	scalarmult(p, q, h);
	scalarbase(q, sig + 32);
	add(p, q);
	pack(t, p);
	return !vn(sig, t, 32);
}

/* ---------------------------------------------------------------- files, keys, manifest */
static u8 *slurp(const char *p, size_t *n)
{
	int f = open(p, O_RDONLY); if (f < 0) return NULL;
	struct stat st; if (fstat(f, &st) || st.st_size > (16 << 20)) { close(f); return NULL; }
	u8 *b = malloc(st.st_size + 1); size_t got = 0;
	while (b && got < (size_t)st.st_size) { ssize_t k = read(f, b + got, st.st_size - got); if (k <= 0) break; got += k; }
	close(f); if (!b || got != (size_t)st.st_size) { free(b); return NULL; }
	b[got] = 0; if (n) *n = got; return b;
}
static int b64(const char *s, u8 *out, int max)
{
	static const char *T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	int n = 0, bits = 0; unsigned v = 0;
	for (; *s && *s != '='; s++) {
		const char *p = strchr(T, *s); if (!p || !*s) return -1;
		v = (v << 6) | (p - T); bits += 6;
		if (bits >= 8) { bits -= 8; if (n >= max) return -1; out[n++] = v >> bits; }
	}
	return n;
}
static int signed_ok(const char *man, const char *sig, const char *keys)
{
	size_t ml, sl, kl; u8 *m = slurp(man, &ml), *s = slurp(sig, &sl), *k = slurp(keys, &kl); int ok = 0;
	if (m && s && k && sl == 64)
		for (char *l = strtok((char *)k, "\n"); l && !ok; l = strtok(NULL, "\n")) {
			while (*l == ' ' || *l == '\t') l++;
			if (!*l || *l == '#') continue;
			char *e = l; while (*e && *e != ' ' && *e != '\t' && *e != '\r') e++; *e = 0;
			u8 pk[33]; if (b64(l, pk, 33) == 32 && ed25519_verify(s, m, ml, pk)) ok = 1;
		}
	free(m); free(s); free(k);
	return ok;
}
static int hexok(const char *h, size_t n) { if (strlen(h) != n) return 0; for (; *h; h++) if (!strchr("0123456789abcdef", *h)) return 0; return 1; }

/* ---------------------------------------------------------------- device-mapper */
static int dm(int c, unsigned long req, struct dm_ioctl *d, const char *name, size_t size)
{
	d->version[0] = DM_VERSION_MAJOR; d->version[1] = 0; d->version[2] = 0;
	d->data_size = size; d->data_start = sizeof *d;
	snprintf(d->name, sizeof d->name, "%s", name);
	return ioctl(c, req, d);
}
static int verity_open(const char *name, const char *loop, unsigned long long blocks, const char *root, const char *salt)
{
	int c = open("/dev/mapper/control", O_RDWR);
	if (c < 0) { mkdir("/dev/mapper", 0755); mknod("/dev/mapper/control", S_IFCHR | 0600, makedev(10, 236)); c = open("/dev/mapper/control", O_RDWR); }
	if (c < 0) { perror("rudolf-verity: /dev/mapper/control"); return -1; }
	char dn[64]; snprintf(dn, sizeof dn, "rudolf-%s", name);
	union { struct dm_ioctl d; char b[16384]; } u;
	memset(&u, 0, sizeof u);
	if (dm(c, DM_DEV_CREATE, &u.d, dn, sizeof u)) { perror("rudolf-verity: create"); close(c); return -1; }
	unsigned dev_minor = minor(u.d.dev);
	memset(&u, 0, sizeof u);
	struct dm_target_spec *t = (struct dm_target_spec *)(u.b + sizeof u.d);
	char *params = (char *)(t + 1);
	/* version 1, data and hash on the same device, 4 KB blocks, the tree right after the data */
	snprintf(params, 4096, "1 %s %s 4096 4096 %llu %llu sha256 %s %s", loop, loop, blocks, blocks, root, salt);
	t->sector_start = 0; t->length = blocks * 8; snprintf(t->target_type, sizeof t->target_type, "verity");
	size_t pl = strlen(params) + 1; pl = (pl + 7) & ~7UL; t->next = 0;
	u.d.target_count = 1; u.d.flags = DM_READONLY_FLAG;
	if (dm(c, DM_TABLE_LOAD, &u.d, dn, sizeof u.d + sizeof *t + pl)) { perror("rudolf-verity: table"); memset(&u, 0, sizeof u); dm(c, DM_DEV_REMOVE, &u.d, dn, sizeof u.d); close(c); return -1; }
	memset(&u, 0, sizeof u);
	if (dm(c, DM_DEV_SUSPEND, &u.d, dn, sizeof u.d)) { perror("rudolf-verity: resume"); memset(&u, 0, sizeof u); dm(c, DM_DEV_REMOVE, &u.d, dn, sizeof u.d); close(c); return -1; }
	close(c);
	char node[32]; snprintf(node, sizeof node, "/dev/dm-%u", dev_minor);
	if (access(node, F_OK)) mknod(node, S_IFBLK | 0400, makedev(major(u.d.dev) ? major(u.d.dev) : 253, dev_minor));
	printf("%s\n", node);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc == 5 && !strcmp(argv[1], "check")) return signed_ok(argv[2], argv[3], argv[4]) ? 0 : 2;
	if (argc == 7 && !strcmp(argv[1], "open")) {
		if (!signed_ok(argv[2], argv[3], argv[4])) { fprintf(stderr, "rudolf-verity: the release list is not signed by a trusted key\n"); return 2; }
		u8 *m = slurp(argv[2], NULL); if (!m) return 2;
		for (char *l = strtok((char *)m, "\n"); l; l = strtok(NULL, "\n")) {
			if (strncmp(l, "V\t", 2)) continue;
			char *f[5]; int k = 0; char *p = l;
			for (char *s = strtok_r(p, "\t", &p); s && k < 5; s = strtok_r(NULL, "\t", &p)) f[k++] = s;
			if (k != 5 || strcmp(f[1], argv[5])) continue;
			unsigned long long blocks = strtoull(f[2], NULL, 10);
			if (!blocks || !hexok(f[3], 64) || !hexok(f[4], 64)) { fprintf(stderr, "rudolf-verity: bad V line for %s\n", argv[5]); return 3; }
			return verity_open(argv[5], argv[6], blocks, f[3], f[4]) ? 4 : 0;
		}
		fprintf(stderr, "rudolf-verity: the release list has no hash for %s\n", argv[5]);
		return 3;
	}
	fprintf(stderr, "usage: rudolf-verity check MANIFEST SIG KEYS | open MANIFEST SIG KEYS NAME LOOPDEV\n");
	return 1;
}
