/* (included by rudolfd.c, not built on its own: hence .inc) ---------- 1.5.9: the native desktop's API (plan Part 12)
 * What the native desktop engine (rudolf-native) needs that only the desktop page knows: its settings, pinned apps and
 * accounts, read from the copy of the page's storage rudolfd already keeps (lsync, LSFILE); and the sign-in, checked
 * here as the page checks it, so rudolf-native never sees a password hash. All under /api/ with the token. docs/api.md.
 *
 *   GET  desktop/accounts      {"accounts":[{"id","n","r","key","old"}],"guest","last","setup"}   (no hashes, no keys)
 *   POST desktop/login         body {"n":NAME,"p":PASSWORD} or {"guest":true}: {"id","n","r"}; opens the home folder
 *   POST desktop/logout        locks the home folder, ends the session
 *   POST desktop/lock          the lock screen is up: desktop/session answers 423 until desktop/unlock
 *   POST desktop/unlock        body {"p":PASSWORD}, the session's account's
 *   GET  desktop/session       {"id","n","r","pk","fk"} for the one-app pages (oneapp59.js); 404 without a session
 *   GET  desktop/settings      the page's settings the shell draws, the pinned apps, the notifications' count, "gen",
 *                              (1.6.0) the desktop's widgets ("widgets", "wgown") and the weather's place and unit ("wloc", "wunit");
 *                              the Calendar's events ("cal", bd95cal:ID) for the calendar under the clock
 *   GET  desktop/apps          the start menu's list and the drawn backgrounds' pictures, as the page wrote them
 *   POST desktop/apps          (the page, after each sign-in)
 *   GET  desktop/cells?acct=ID       (1.6.0) where the shell put the desktop's icons: {"NAME":[COLUMN,ROW]}
 *   POST desktop/cells?acct=ID       (the shell, when an icon is moved)
 *   POST desktop/widgets?acct=ID     (1.6.0) the shell's desktop widgets (the page's bd95widgets:ID list), kept here with the
 *                                    page's value it replaced: desktop/settings gives it ("widgets", "wgown") until the
 *                                    page's own value changes (another engine's desktop, or a page that took the shell's)
 *   GET  desktop/wallpaper?acct=ID   a picture of your own (JPEG), as the page gave it, else (1.5.9 step 7) made here from
 *                                    the page's database mirror (idbm: the record "wp:PICTURE" of rudolfos/kv)
 *   POST desktop/wallpaper?acct=ID[&img=PICTURE]   (the page, when it loads or changes one)
 *
 * The sign-in's cryptography is the page's (core25.js, acct50.js, home56.js), in plain C so rudolfd links nothing new:
 * the password hash "p2$SALT$HASH" is PBKDF2-HMAC-SHA-256 with 210 000 rounds; the password's key (PKEY) is the same
 * with the account's salt (ks); the file key (fk) is AES-256-GCM sealed with it; the home folder's key is HKDF-SHA-512
 * of the file key ("rudolf-os home folder", the account's id). Each connection is a process of its own, so the session
 * is a root-only file (/run/rudolf/native-session). */
#define DESK_ROUNDS 210000
static char desk_ls[256] = LSFILE, desk_dir[256] = "/home/rudolf", desk_run[256] = RUN;
static int desk_uid = 1000;

/* ------------------------------------------------------------------ SHA-256, SHA-512, HMAC, PBKDF2, HKDF */
typedef struct { uint32_t h[8]; uint8_t b[64]; uint64_t n; size_t bl; } dsha256;
static const uint32_t DK256[64] = {
	0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
	0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
	0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
	0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
	0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
	0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };
#define DROR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static void dsha256_block(uint32_t *h, const uint8_t *p)
{
	uint32_t w[64], a, b, c, d, e, f, g, k;
	for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
	for (int i = 16; i < 64; i++) {
		uint32_t s0 = DROR32(w[i - 15], 7) ^ DROR32(w[i - 15], 18) ^ (w[i - 15] >> 3), s1 = DROR32(w[i - 2], 17) ^ DROR32(w[i - 2], 19) ^ (w[i - 2] >> 10);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}
	a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4]; f = h[5]; g = h[6]; k = h[7];
	for (int i = 0; i < 64; i++) {
		uint32_t t1 = k + (DROR32(e, 6) ^ DROR32(e, 11) ^ DROR32(e, 25)) + ((e & f) ^ (~e & g)) + DK256[i] + w[i];
		uint32_t t2 = (DROR32(a, 2) ^ DROR32(a, 13) ^ DROR32(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
		k = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
	}
	h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += k;
}
static void dsha256_init(dsha256 *s)
{
	static const uint32_t H0[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
	memcpy(s->h, H0, sizeof H0); s->n = 0; s->bl = 0;
}
static void dsha256_add(dsha256 *s, const void *d, size_t n)
{
	const uint8_t *p = d; s->n += n;
	while (n) { size_t k = 64 - s->bl; if (k > n) k = n; memcpy(s->b + s->bl, p, k); s->bl += k; p += k; n -= k; if (s->bl == 64) { dsha256_block(s->h, s->b); s->bl = 0; } }
}
static void dsha256_end(dsha256 *s, uint8_t *out)
{
	uint64_t bits = s->n * 8; uint8_t z = 0x80; dsha256_add(s, &z, 1); z = 0;
	while (s->bl != 56) dsha256_add(s, &z, 1);
	for (int i = 7; i >= 0; i--) { uint8_t c = bits >> (8 * i); dsha256_add(s, &c, 1); }
	for (int i = 0; i < 8; i++) { out[4 * i] = s->h[i] >> 24; out[4 * i + 1] = s->h[i] >> 16; out[4 * i + 2] = s->h[i] >> 8; out[4 * i + 3] = s->h[i]; }
}
typedef struct { uint64_t h[8]; uint8_t b[128]; uint64_t n; size_t bl; } dsha512;
static const uint64_t DK512[80] = {
	0x428a2f98d728ae22ULL,0x7137449123ef65cdULL,0xb5c0fbcfec4d3b2fULL,0xe9b5dba58189dbbcULL,0x3956c25bf348b538ULL,0x59f111f1b605d019ULL,
	0x923f82a4af194f9bULL,0xab1c5ed5da6d8118ULL,0xd807aa98a3030242ULL,0x12835b0145706fbeULL,0x243185be4ee4b28cULL,0x550c7dc3d5ffb4e2ULL,
	0x72be5d74f27b896fULL,0x80deb1fe3b1696b1ULL,0x9bdc06a725c71235ULL,0xc19bf174cf692694ULL,0xe49b69c19ef14ad2ULL,0xefbe4786384f25e3ULL,
	0x0fc19dc68b8cd5b5ULL,0x240ca1cc77ac9c65ULL,0x2de92c6f592b0275ULL,0x4a7484aa6ea6e483ULL,0x5cb0a9dcbd41fbd4ULL,0x76f988da831153b5ULL,
	0x983e5152ee66dfabULL,0xa831c66d2db43210ULL,0xb00327c898fb213fULL,0xbf597fc7beef0ee4ULL,0xc6e00bf33da88fc2ULL,0xd5a79147930aa725ULL,
	0x06ca6351e003826fULL,0x142929670a0e6e70ULL,0x27b70a8546d22ffcULL,0x2e1b21385c26c926ULL,0x4d2c6dfc5ac42aedULL,0x53380d139d95b3dfULL,
	0x650a73548baf63deULL,0x766a0abb3c77b2a8ULL,0x81c2c92e47edaee6ULL,0x92722c851482353bULL,0xa2bfe8a14cf10364ULL,0xa81a664bbc423001ULL,
	0xc24b8b70d0f89791ULL,0xc76c51a30654be30ULL,0xd192e819d6ef5218ULL,0xd69906245565a910ULL,0xf40e35855771202aULL,0x106aa07032bbd1b8ULL,
	0x19a4c116b8d2d0c8ULL,0x1e376c085141ab53ULL,0x2748774cdf8eeb99ULL,0x34b0bcb5e19b48a8ULL,0x391c0cb3c5c95a63ULL,0x4ed8aa4ae3418acbULL,
	0x5b9cca4f7763e373ULL,0x682e6ff3d6b2b8a3ULL,0x748f82ee5defb2fcULL,0x78a5636f43172f60ULL,0x84c87814a1f0ab72ULL,0x8cc702081a6439ecULL,
	0x90befffa23631e28ULL,0xa4506cebde82bde9ULL,0xbef9a3f7b2c67915ULL,0xc67178f2e372532bULL,0xca273eceea26619cULL,0xd186b8c721c0c207ULL,
	0xeada7dd6cde0eb1eULL,0xf57d4f7fee6ed178ULL,0x06f067aa72176fbaULL,0x0a637dc5a2c898a6ULL,0x113f9804bef90daeULL,0x1b710b35131c471bULL,
	0x28db77f523047d84ULL,0x32caab7b40c72493ULL,0x3c9ebe0a15c9bebcULL,0x431d67c49c100d4cULL,0x4cc5d4becb3e42b6ULL,0x597f299cfc657e2aULL,
	0x5fcb6fab3ad6faecULL,0x6c44198c4a475817ULL };
#define DROR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))
static void dsha512_block(uint64_t *h, const uint8_t *p)
{
	uint64_t w[80], a, b, c, d, e, f, g, k;
	for (int i = 0; i < 16; i++) { w[i] = 0; for (int j = 0; j < 8; j++) w[i] = w[i] << 8 | p[8 * i + j]; }
	for (int i = 16; i < 80; i++) {
		uint64_t s0 = DROR64(w[i - 15], 1) ^ DROR64(w[i - 15], 8) ^ (w[i - 15] >> 7), s1 = DROR64(w[i - 2], 19) ^ DROR64(w[i - 2], 61) ^ (w[i - 2] >> 6);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}
	a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4]; f = h[5]; g = h[6]; k = h[7];
	for (int i = 0; i < 80; i++) {
		uint64_t t1 = k + (DROR64(e, 14) ^ DROR64(e, 18) ^ DROR64(e, 41)) + ((e & f) ^ (~e & g)) + DK512[i] + w[i];
		uint64_t t2 = (DROR64(a, 28) ^ DROR64(a, 34) ^ DROR64(a, 39)) + ((a & b) ^ (a & c) ^ (b & c));
		k = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
	}
	h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += k;
}
static void dsha512_init(dsha512 *s)
{
	static const uint64_t H0[8] = {0x6a09e667f3bcc908ULL,0xbb67ae8584caa73bULL,0x3c6ef372fe94f82bULL,0xa54ff53a5f1d36f1ULL,
		0x510e527fade682d1ULL,0x9b05688c2b3e6c1fULL,0x1f83d9abfb41bd6bULL,0x5be0cd19137e2179ULL};
	memcpy(s->h, H0, sizeof H0); s->n = 0; s->bl = 0;
}
static void dsha512_add(dsha512 *s, const void *d, size_t n)
{
	const uint8_t *p = d; s->n += n;
	while (n) { size_t k = 128 - s->bl; if (k > n) k = n; memcpy(s->b + s->bl, p, k); s->bl += k; p += k; n -= k; if (s->bl == 128) { dsha512_block(s->h, s->b); s->bl = 0; } }
}
static void dsha512_end(dsha512 *s, uint8_t *out)
{
	uint64_t bits = s->n * 8; uint8_t z = 0x80; dsha512_add(s, &z, 1); z = 0;
	while (s->bl != 112) dsha512_add(s, &z, 1);
	for (int i = 0; i < 8; i++) dsha512_add(s, &z, 1);
	for (int i = 7; i >= 0; i--) { uint8_t c = bits >> (8 * i); dsha512_add(s, &c, 1); }
	for (int i = 0; i < 8; i++) for (int j = 0; j < 8; j++) out[8 * i + j] = s->h[i] >> (56 - 8 * j);
}
/* HMAC-SHA-256 with the key's inner and outer states made once (PBKDF2 uses them 420 000 times) */
typedef struct { dsha256 in, out; } dhmac256k;
static void dhmac256_key(dhmac256k *m, const uint8_t *k, size_t kl)
{
	uint8_t kb[64] = {0}, pad[64];
	if (kl > 64) { dsha256 s; dsha256_init(&s); dsha256_add(&s, k, kl); dsha256_end(&s, kb); } else memcpy(kb, k, kl);
	for (int i = 0; i < 64; i++) pad[i] = kb[i] ^ 0x36;
	dsha256_init(&m->in); dsha256_add(&m->in, pad, 64);
	for (int i = 0; i < 64; i++) pad[i] = kb[i] ^ 0x5c;
	dsha256_init(&m->out); dsha256_add(&m->out, pad, 64);
	explicit_bzero(kb, sizeof kb); explicit_bzero(pad, sizeof pad);
}
static void dhmac256(const dhmac256k *m, const void *d, size_t n, uint8_t *out)
{
	dsha256 s = m->in; uint8_t t[32]; dsha256_add(&s, d, n); dsha256_end(&s, t);
	s = m->out; dsha256_add(&s, t, 32); dsha256_end(&s, out);
}
static void dpbkdf2_256(const char *pw, const uint8_t *salt, size_t sl, int rounds, uint8_t *out, size_t ol)
{
	dhmac256k m; dhmac256_key(&m, (const uint8_t *)pw, strlen(pw));
	uint8_t *sb = malloc(sl + 4); memcpy(sb, salt, sl);
	for (uint32_t blk = 1; ol; blk++) {
		uint8_t u[32], t[32];
		sb[sl] = blk >> 24; sb[sl + 1] = blk >> 16; sb[sl + 2] = blk >> 8; sb[sl + 3] = blk;
		dhmac256(&m, sb, sl + 4, u); memcpy(t, u, 32);
		for (int r = 1; r < rounds; r++) { dhmac256(&m, u, 32, u); for (int i = 0; i < 32; i++) t[i] ^= u[i]; }
		size_t k = ol < 32 ? ol : 32; memcpy(out, t, k); out += k; ol -= k;
		explicit_bzero(u, 32); explicit_bzero(t, 32);
	}
	free(sb); explicit_bzero(&m, sizeof m);
}
static void dhmac512(const uint8_t *k, size_t kl, const uint8_t *d, size_t n, uint8_t *out)
{
	uint8_t kb[128] = {0}, pad[128], t[64]; dsha512 s;
	if (kl > 128) { dsha512_init(&s); dsha512_add(&s, k, kl); dsha512_end(&s, kb); } else memcpy(kb, k, kl);
	for (int i = 0; i < 128; i++) pad[i] = kb[i] ^ 0x36;
	dsha512_init(&s); dsha512_add(&s, pad, 128); dsha512_add(&s, d, n); dsha512_end(&s, t);
	for (int i = 0; i < 128; i++) pad[i] = kb[i] ^ 0x5c;
	dsha512_init(&s); dsha512_add(&s, pad, 128); dsha512_add(&s, t, 64); dsha512_end(&s, out);
	explicit_bzero(kb, sizeof kb); explicit_bzero(t, sizeof t);
}
/* HKDF-SHA-512, one block (64 bytes): what WebCrypto's deriveBits({name:'HKDF',hash:'SHA-512'}, 512) gives */
static void dhkdf512(const uint8_t *ikm, size_t il, const char *salt, const char *info, uint8_t *out)
{
	uint8_t prk[64]; dhmac512((const uint8_t *)salt, strlen(salt), ikm, il, prk);
	size_t n = strlen(info); uint8_t *b = malloc(n + 1); memcpy(b, info, n); b[n] = 1;
	dhmac512(prk, 64, b, n + 1, out); free(b); explicit_bzero(prk, sizeof prk);
}

/* ------------------------------------------------------------------ AES-256 (encryption only) and GCM opening */
static uint8_t DSBOX[256];
static void daes_tables(void)
{
	if (DSBOX[0]) return;
	uint8_t p = 1, q = 1;
	do {   /* the S-box from the multiplicative inverse in GF(2^8) and the affine map */
		p = p ^ (p << 1) ^ (p & 0x80 ? 0x1b : 0);
		q ^= q << 1; q ^= q << 2; q ^= q << 4; if (q & 0x80) q ^= 0x09;
		uint8_t x = q ^ (uint8_t)(q << 1 | q >> 7) ^ (uint8_t)(q << 2 | q >> 6) ^ (uint8_t)(q << 3 | q >> 5) ^ (uint8_t)(q << 4 | q >> 4);
		DSBOX[p] = x ^ 0x63;
	} while (p != 1);
	DSBOX[0] = 0x63;
}
static uint8_t dxt(uint8_t x) { return (uint8_t)(x << 1) ^ (x & 0x80 ? 0x1b : 0); }
static void daes256_expand(const uint8_t *key, uint8_t *rk /* 240 */)
{
	daes_tables(); memcpy(rk, key, 32); uint8_t rc = 1;
	for (int i = 8; i < 60; i++) {
		uint8_t t[4]; memcpy(t, rk + 4 * (i - 1), 4);
		if (i % 8 == 0) { uint8_t u = t[0]; t[0] = DSBOX[t[1]] ^ rc; t[1] = DSBOX[t[2]]; t[2] = DSBOX[t[3]]; t[3] = DSBOX[u]; rc = dxt(rc); }
		else if (i % 8 == 4) for (int j = 0; j < 4; j++) t[j] = DSBOX[t[j]];
		for (int j = 0; j < 4; j++) rk[4 * i + j] = rk[4 * (i - 8) + j] ^ t[j];
	}
}
static void daes256_block(const uint8_t *rk, const uint8_t *in, uint8_t *out)
{
	uint8_t s[16]; for (int i = 0; i < 16; i++) s[i] = in[i] ^ rk[i];
	for (int r = 1; r <= 14; r++) {
		uint8_t t[16];
		for (int c = 0; c < 4; c++) for (int j = 0; j < 4; j++) t[4 * c + j] = DSBOX[s[4 * ((c + j) % 4) + j]];   /* SubBytes, ShiftRows */
		if (r < 14) for (int c = 0; c < 4; c++) {   /* MixColumns */
			uint8_t *a = t + 4 * c, a0 = a[0], a1 = a[1], a2 = a[2], a3 = a[3], x = a0 ^ a1 ^ a2 ^ a3;
			a[0] ^= x ^ dxt(a0 ^ a1); a[1] ^= x ^ dxt(a1 ^ a2); a[2] ^= x ^ dxt(a2 ^ a3); a[3] ^= x ^ dxt(a3 ^ a0);
		}
		for (int i = 0; i < 16; i++) s[i] = t[i] ^ rk[16 * r + i];
	}
	memcpy(out, s, 16);
}
static void dgf_mul(uint8_t *x, const uint8_t *h)   /* x = x * h in GCM's field */
{
	uint8_t z[16] = {0}, v[16]; memcpy(v, h, 16);
	for (int i = 0; i < 128; i++) {
		if (x[i / 8] & (0x80 >> (i % 8))) for (int j = 0; j < 16; j++) z[j] ^= v[j];
		int lsb = v[15] & 1; for (int j = 15; j > 0; j--) v[j] = v[j] >> 1 | v[j - 1] << 7; v[0] >>= 1; if (lsb) v[0] ^= 0xe1;
	}
	memcpy(x, z, 16);
}
/* AES-256-GCM with a 12-byte IV and a 16-byte tag at the end of ct (as WebCrypto seals): 0 and the plain text in out
   (ctl - 16 bytes), or -1 when the tag does not match (the wrong key) */
static int daes_gcm_open(const uint8_t *key, const uint8_t *iv, const uint8_t *ct, size_t ctl, uint8_t *out)
{
	if (ctl < 16) return -1;
	size_t n = ctl - 16; uint8_t rk[240], h[16] = {0}, j0[16], ctr[16], ks[16], g[16] = {0};
	daes256_expand(key, rk); daes256_block(rk, h, h);
	memcpy(j0, iv, 12); j0[12] = j0[13] = j0[14] = 0; j0[15] = 1;
	for (size_t i = 0; i < n; i += 16) { size_t k = n - i < 16 ? n - i : 16; for (size_t j = 0; j < k; j++) g[j] ^= ct[i + j]; dgf_mul(g, h); }
	uint8_t lb[16] = {0}; uint64_t cb = (uint64_t)n * 8; for (int j = 0; j < 8; j++) lb[15 - j] = cb >> (8 * j);
	for (int j = 0; j < 16; j++) g[j] ^= lb[j];
	dgf_mul(g, h);
	daes256_block(rk, j0, ks); uint8_t diff = 0; for (int j = 0; j < 16; j++) diff |= (g[j] ^ ks[j]) ^ ct[n + j];
	if (diff) { explicit_bzero(rk, sizeof rk); return -1; }
	memcpy(ctr, j0, 16);
	for (size_t i = 0; i < n; i += 16) {
		for (int j = 15; j >= 12; j--) if (++ctr[j]) break;
		daes256_block(rk, ctr, ks); size_t k = n - i < 16 ? n - i : 16; for (size_t j = 0; j < k; j++) out[i + j] = ct[i + j] ^ ks[j];
	}
	explicit_bzero(rk, sizeof rk); explicit_bzero(ks, sizeof ks); return 0;
}
/* base64 (with or without padding); -1 when it is not */
static int db64d(const char *s, uint8_t *out, size_t cap)
{
	size_t n = 0; uint32_t acc = 0; int bits = 0;
	for (; s && *s && *s != '='; s++) {
		int v = *s >= 'A' && *s <= 'Z' ? *s - 'A' : *s >= 'a' && *s <= 'z' ? *s - 'a' + 26 : *s >= '0' && *s <= '9' ? *s - '0' + 52 : *s == '+' || *s == '-' ? 62 : *s == '/' || *s == '_' ? 63 : -1;
		if (v < 0) return -1;
		acc = acc << 6 | v; bits += 6;
		if (bits >= 8) { bits -= 8; if (n >= cap) return -1; out[n++] = acc >> bits; }
	}
	return (int)n;
}
static void dhex(const uint8_t *b, size_t n, char *out) { for (size_t i = 0; i < n; i++) sprintf(out + 2 * i, "%02x", b[i]); out[2 * n] = 0; }

/* ------------------------------------------------------------------ a small JSON reader (values stay where they are) */
static const char *dj_ws(const char *p) { while (p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++; return p; }
/* the end of the value at p (NULL: not JSON) */
static const char *dj_end(const char *p, int depth)
{
	p = dj_ws(p); if (!p || !*p || depth > 64) return NULL;
	if (*p == '"') { for (p++; *p && *p != '"'; p++) if (*p == '\\' && !*++p) return NULL; return *p ? p + 1 : NULL; }
	if (*p == '{' || *p == '[') {
		char c = *p == '{' ? '}' : ']'; p = dj_ws(p + 1); if (*p == c) return p + 1;
		for (;;) {
			if (c == '}') { if (*p != '"') return NULL; p = dj_end(p, depth + 1); if (!p) return NULL; p = dj_ws(p); if (*p != ':') return NULL; p++; }
			p = dj_end(p, depth + 1); if (!p) return NULL; p = dj_ws(p);
			if (*p == ',') { p = dj_ws(p + 1); continue; }
			return *p == c ? p + 1 : NULL;
		}
	}
	const char *s = p; while (*p && !strchr(",}] \t\r\n:", *p)) p++;
	return p > s ? p : NULL;
}
/* the value of a member of the object at o (its key compared as written: the page's keys have no escapes) */
static const char *dj_get(const char *o, const char *key)
{
	o = dj_ws(o); if (!o || *o != '{') return NULL;
	const char *p = dj_ws(o + 1); size_t kl = strlen(key);
	while (p && *p == '"') {
		const char *ke = dj_end(p, 0); if (!ke) return NULL;
		int hit = (size_t)(ke - p) == kl + 2 && !memcmp(p + 1, key, kl);
		p = dj_ws(ke); if (*p != ':') return NULL; p = dj_ws(p + 1);
		if (hit) return p;
		p = dj_end(p, 0); if (!p) return NULL; p = dj_ws(p); if (*p != ',') return NULL; p = dj_ws(p + 1);
	}
	return NULL;
}
/* the i-th element of the array at a */
static const char *dj_at(const char *a, int i)
{
	a = dj_ws(a); if (!a || *a != '[') return NULL;
	const char *p = dj_ws(a + 1); if (*p == ']') return NULL;
	for (int k = 0; p; k++) { if (k == i) return p; p = dj_end(p, 0); if (!p) return NULL; p = dj_ws(p); if (*p != ',') return NULL; p = dj_ws(p + 1); }
	return NULL;
}
static int dj_count(const char *a) { int n = 0; while (dj_at(a, n)) n++; return n; }
static void dj_utf8(str *s, unsigned c)
{
	if (c < 0x80) sadd(s, "%c", c);
	else if (c < 0x800) sadd(s, "%c%c", 0xc0 | c >> 6, 0x80 | (c & 63));
	else if (c < 0x10000) sadd(s, "%c%c%c", 0xe0 | c >> 12, 0x80 | (c >> 6 & 63), 0x80 | (c & 63));
	else sadd(s, "%c%c%c%c", 0xf0 | c >> 18, 0x80 | (c >> 12 & 63), 0x80 | (c >> 6 & 63), 0x80 | (c & 63));
}
/* the string at p, unescaped (malloc'd; NULL when it is not a string) */
static char *dj_str(const char *p)
{
	p = dj_ws(p); if (!p || *p != '"' || !dj_end(p, 0)) return NULL;
	str s = {0}; sadd(&s, "%s", "");
	for (p++; *p != '"'; p++) {
		if (*p != '\\') { sadd(&s, "%c", *p); continue; }
		p++;
		switch (*p) {
		case 'n': sadd(&s, "\n"); break; case 't': sadd(&s, "\t"); break; case 'r': sadd(&s, "\r"); break;
		case 'b': sadd(&s, "\b"); break; case 'f': sadd(&s, "\f"); break;
		case 'u': {
			unsigned c = 0; for (int i = 1; i <= 4; i++) { int v = hexval(p[i]); if (v < 0) { free(s.b); return NULL; } c = c << 4 | v; } p += 4;
			if (c >= 0xd800 && c < 0xdc00 && p[1] == '\\' && p[2] == 'u') {
				unsigned d = 0; int okk = 1; for (int i = 3; i <= 6; i++) { int v = hexval(p[i]); if (v < 0) okk = 0; else d = d << 4 | v; }
				if (okk && d >= 0xdc00 && d < 0xe000) { c = 0x10000 + ((c - 0xd800) << 10) + (d - 0xdc00); p += 6; }
			}
			dj_utf8(&s, c); break; }
		default: sadd(&s, "%c", *p);
		}
	}
	return s.b;
}
static int dj_true(const char *p) { p = dj_ws(p); return p && !strncmp(p, "true", 4); }
static int dj_truthy(const char *p) { p = dj_ws(p); return p && (dj_true(p) || ((*p >= '1' && *p <= '9') || (*p == '"' && p[1] != '"'))); }
static double dj_num(const char *p, double d) { p = dj_ws(p); if (!p || !(*p == '-' || (*p >= '0' && *p <= '9'))) return d; return strtod(p, NULL); }
static void dj_raw(str *s, const char *p, const char *def)   /* the value at p as written (it is JSON already) */
{
	const char *e = p ? dj_end(p, 0) : NULL; p = dj_ws(p);
	if (e) sadd(s, "%.*s", (int)(e - p), p); else sadd(s, "%s", def);
}

/* ------------------------------------------------------------------ the page's storage (the lsync copy) */
typedef struct { char *file; const char *data; } dls;
static int dls_open(dls *l) { l->file = slurp(desk_ls, NULL); l->data = l->file ? dj_get(l->file, "data") : NULL; return l->data != NULL; }
static void dls_close(dls *l) { free(l->file); l->file = NULL; l->data = NULL; }
/* a key's value (the page keeps JSON in strings): malloc'd, NULL when it is not there */
static char *dls_get(dls *l, const char *key) { return l->data ? dj_str(dj_get(l->data, key)) : NULL; }
/* the accounts (bd95user): an array, or (pages from before accounts) one account */
static char *dls_accounts(dls *l) { char *u = dls_get(l, "bd95user"); if (!u) return NULL; const char *a = dj_get(u, "accts");
	if (a) { const char *e = dj_end(a, 0); a = dj_ws(a); if (!e) { free(u); return NULL; } char *r = strndup(a, e - a); free(u); return r; }
	str s = {0}; sadd(&s, "[%s]", u); free(u); return s.b; }
/* an account's id as the page's uid() makes it */
static void dacct_id(const char *a, char *out, size_t n)
{
	char *id = dj_str(dj_get(a, "id"));
	if (id && *id) snprintf(out, n, "%s", id);
	else { char *nm = dj_str(dj_get(a, "n")); snprintf(out, n, "u-%s", nm ? nm : ""); for (char *c = out; *c; c++) *c = tolower((unsigned char)*c); free(nm); }
	free(id);
}
static const char *dacct_find(const char *list, const char *name)
{
	for (int i = 0;; i++) { const char *a = dj_at(list, i); if (!a) return NULL; char *n = dj_str(dj_get(a, "n")); int hit = n && name && !strcasecmp(n, name); free(n); if (hit) return a; }
}

/* ------------------------------------------------------------------ the session (root-only files in /run/rudolf) */
typedef struct { char id[80], n[256], r[16], pk[65], fk[65]; } dsess;
static void dpath(char *o, size_t n, const char *f) { snprintf(o, n, "%s/%s", desk_run, f); }
static int dsess_read(dsess *s)
{
	char p[300]; dpath(p, sizeof p, "native-session"); char *t = slurp(p, NULL); memset(s, 0, sizeof *s);
	if (!t) return 0;
	char *l[5] = {0}; int k = 0; char *c = t; while (k < 5 && c) { l[k++] = c; c = strchr(c, '\n'); if (c) *c++ = 0; }
	if (k >= 3 && l[0][0]) { snprintf(s->id, sizeof s->id, "%s", l[0]); snprintf(s->n, sizeof s->n, "%s", l[1]); snprintf(s->r, sizeof s->r, "%s", l[2]);
		if (k > 3 && l[3]) snprintf(s->pk, sizeof s->pk, "%s", l[3]);
		if (k > 4 && l[4]) snprintf(s->fk, sizeof s->fk, "%s", l[4]); }
	explicit_bzero(t, strlen(t)); free(t); return s->id[0] != 0;
}
static int dsess_write(const dsess *s)
{
	char p[300], tp[310]; dpath(p, sizeof p, "native-session"); snprintf(tp, sizeof tp, "%s.tmp", p);
	int f = open(tp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600); if (f < 0) return -1;
	int ok = dprintf(f, "%s\n%s\n%s\n%s\n%s\n", s->id, s->n, s->r, s->pk, s->fk) > 0; close(f);
	if (!ok || rename(tp, p)) { unlink(tp); return -1; }
	return 0;
}
/* five wrong passwords in a row: 15 s of waiting, as the page's sign-in screen */
static int dfail_wait(void)
{
	char p[300]; dpath(p, sizeof p, "native-login-fails"); char *t = slurp(p, NULL); long long n = 0, until = 0;
	if (t) { sscanf(t, "%lld %lld", &n, &until); free(t); }
	long long now = time(NULL); return until > now ? (int)(until - now) : 0;
}
static void dfail_note(int ok)
{
	char p[300]; dpath(p, sizeof p, "native-login-fails");
	if (ok) { unlink(p); return; }
	char *t = slurp(p, NULL); long long n = 0, until = 0; if (t) { sscanf(t, "%lld %lld", &n, &until); free(t); }
	n++; if (n >= 5) { n = 0; until = time(NULL) + 15; }
	char b[64]; snprintf(b, sizeof b, "%lld %lld\n", n, until); write_str(p, b);
}
/* the password against "p2$SALT$HASH" (constant time); -2: a hash from before 1.4 (SHA-256 only), -1 wrong */
static int dpw_check(const char *h, const char *pw)
{
	if (!h || strncmp(h, "p2$", 3)) return -2;
	const char *s = h + 3, *d = strchr(s, '$'); if (!d) return -1;
	char sb[128]; size_t sl = d - s; if (sl >= sizeof sb) return -1; memcpy(sb, s, sl); sb[sl] = 0;
	uint8_t salt[96], want[64], got[32]; int n = db64d(sb, salt, sizeof salt), wl = db64d(d + 1, want, sizeof want);
	if (n <= 0 || wl != 32) return -1;
	dpbkdf2_256(pw, salt, n, DESK_ROUNDS, got, 32);
	uint8_t diff = 0; for (int i = 0; i < 32; i++) diff |= got[i] ^ want[i];
	explicit_bzero(got, sizeof got); return diff ? -1 : 0;
}
static char *dbody_field(struct req *r, const char *k) { return r->body && r->blen ? dj_str(dj_get(r->body, k)) : NULL; }
static void dfree_secret(char *p) { if (p) { explicit_bzero(p, strlen(p)); free(p); } }

static void api_desktop_login(struct req *r)
{
	int w = dfail_wait();
	if (w) { char m[120]; snprintf(m, sizeof m, "Too many failed attempts. Please wait %d seconds and try again.", w); reply_err(r->fd, 429, m); return; }
	dls l; if (!dls_open(&l)) { reply_err(r->fd, 409, "Rudolf OS is not set up yet: start it once with the Standard desktop engine."); return; }
	char *list = dls_accounts(&l); dsess s; memset(&s, 0, sizeof s);
	if (r->body && dj_true(dj_get(r->body, "guest"))) {
		char *sys = dls_get(&l, "bd95sys"); const char *g = sys ? dj_get(sys, "guest") : NULL; int allowed = !g || dj_true(g); free(sys);
		if (!allowed) { free(list); dls_close(&l); reply_err(r->fd, 403, "The guest account is turned off."); return; }
		snprintf(s.id, sizeof s.id, "guest"); snprintf(s.n, sizeof s.n, "Guest"); snprintf(s.r, sizeof s.r, "guest");
	} else {
		char *n = dbody_field(r, "n"), *pw = dbody_field(r, "p");
		const char *a = list && n ? dacct_find(list, n) : NULL;
		if (!a || !pw) {
			dfail_note(0); health_add("signin", "wrong password for an account that does not exist (native sign-in)");
			free(n); dfree_secret(pw); free(list); dls_close(&l); reply_err(r->fd, 403, "The user name or password is incorrect."); return;
		}
		char *h = dj_str(dj_get(a, "h")), *nm = dj_str(dj_get(a, "n")), *role = dj_str(dj_get(a, "r"));
		const char *pkq = dj_get(a, "pk");
		int st = pkq && *dj_ws(pkq) != 'n' ? -3 : dpw_check(h, pw);
		const char *why = st == -3 ? "This account signs in with a security key: use the Standard desktop engine for it."
			: st == -2 ? "This account's password is from an older Rudolf OS: sign in once with the Standard desktop engine." : NULL;
		if (why) { free(h); free(nm); free(role); free(n); dfree_secret(pw); free(list); dls_close(&l); reply_err(r->fd, 409, why); return; }
		if (st) {
			dfail_note(0); health_add("signin", "wrong password for the account %.100s (native sign-in)", nm ? nm : n);
			free(h); free(nm); free(role); free(n); dfree_secret(pw); free(list); dls_close(&l); reply_err(r->fd, 403, "The user name or password is incorrect."); return;
		}
		dacct_id(a, s.id, sizeof s.id); snprintf(s.n, sizeof s.n, "%s", nm ? nm : n); snprintf(s.r, sizeof s.r, "%s", role && !strcmp(role, "admin") ? "admin" : "user");
		free(h); free(nm); free(role); free(n);
		/* the password's key (the account's salt), and the file key it seals */
		char *ks = dj_str(dj_get(a, "ks")); uint8_t salt[96], pk[32], fk[64]; int sl = ks ? db64d(ks, salt, sizeof salt) : -1, fl = -1; free(ks);
		if (sl <= 0) { dfree_secret(pw); free(list); dls_close(&l); reply_err(r->fd, 409, "This account has no key yet: sign in once with the Standard desktop engine."); return; }
		dpbkdf2_256(pw, salt, sl, DESK_ROUNDS, pk, 32); dfree_secret(pw); dhex(pk, 32, s.pk);
		const char *box = dj_get(a, "fk");
		if (box && *dj_ws(box) == '{') {
			char *iv = dj_str(dj_get(box, "iv")), *ct = dj_str(dj_get(box, "ct")); uint8_t ivb[16], ctb[128];
			int il = iv ? db64d(iv, ivb, sizeof ivb) : -1, cl = ct ? db64d(ct, ctb, sizeof ctb) : -1; free(iv); free(ct);
			if (il == 12 && cl > 16 && cl - 16 <= (int)sizeof fk && !daes_gcm_open(pk, ivb, ctb, cl, fk)) { fl = cl - 16; dhex(fk, fl, s.fk); }
			else { explicit_bzero(pk, sizeof pk); free(list); dls_close(&l); health_add("signin", "the file key of the account %.100s did not open (native sign-in)", s.n); reply_err(r->fd, 500, "This account's key did not open: sign in with the Standard desktop engine."); return; }
		}
		explicit_bzero(pk, sizeof pk);
		/* an account whose files are on the disk: its encrypted folder, with the key the page makes (home56.js hdKeyHex) */
		if (dj_truthy(dj_get(a, "disk")) && fl == 32) {
			uint8_t hk[64]; char err[200] = ""; int made = 0; dhkdf512(fk, 32, "rudolf-os home folder", s.id, hk);
			int hs = home_acct_ok(s.id) ? home_unlock(s.id, hk, !home_nobind, err, sizeof err, &made) : 400; explicit_bzero(hk, sizeof hk);
			if (hs) { explicit_bzero(fk, sizeof fk); explicit_bzero(&s, sizeof s); free(list); dls_close(&l); char m[300]; snprintf(m, sizeof m, "Your files could not be opened: %s", err[0] ? err : "the folder did not open"); reply_err(r->fd, 500, m); return; }
		}
		explicit_bzero(fk, sizeof fk);
	}
	free(list); dls_close(&l);
	char lp[300]; dpath(lp, sizeof lp, "native-locked"); unlink(lp);
	if (dsess_write(&s)) { explicit_bzero(&s, sizeof s); reply_err(r->fd, 500, "the session could not be kept"); return; }
	dfail_note(1);
	str o = {0}; sadd(&o, "{\"ok\":true,\"id\":"); sjson(&o, s.id); sadd(&o, ",\"n\":"); sjson(&o, s.n); sadd(&o, ",\"r\":"); sjson(&o, s.r); sadd(&o, "}");
	explicit_bzero(&s, sizeof s); reply_json(r->fd, &o);
}

/* ------------------------------------------------------------------ a picture of your own (1.5.9 step 7)
 * The picture lives in the page's IndexedDB, which the page mirrors here record by record (idbm, rudolfd.c: one file per
 * record, named by FNV-1a 64 of "db\0store\0" and the key as JSON). The page also gives the shell a copy of it
 * (POST desktop/wallpaper), but only once a page has loaded the picture: at the first start of the Native engine no
 * page has, and the shell drew the colours. So the copy is made here from the mirror whenever it is missing or is of
 * another picture (ID.img says which picture ID.jpg is). */
static int dput(const char *path, const char *b, size_t n);
static int dwp_id_ok(const char *v) { size_t n = v ? strlen(v) : 0; return n > 0 && n <= 40 && strspn(v, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") == n; }
static void didbm_name(const char *db, const char *store, const char *keyjson, char out[17])
{
	uint64_t h = 0xcbf29ce484222325ULL; const char *parts[3] = { db, store, keyjson };
	for (int i = 0; i < 3; i++) { for (const unsigned char *c = (const unsigned char *)parts[i]; *c; c++) { h ^= *c; h *= 0x100000001b3ULL; } if (i < 2) { h ^= 0; h *= 0x100000001b3ULL; } }
	snprintf(out, 17, "%016llx", (unsigned long long)h);
}
/* which picture the account's settings show ("" none): bd95set:ID's wpimg */
static void dwp_current(dls *l, const char *acct, char *out, size_t n)
{
	char k[160]; snprintf(k, sizeof k, "bd95set:%s", acct); char *set = dls_get(l, k), *v = set ? dj_str(dj_get(set, "wpimg")) : NULL;
	snprintf(out, n, "%s", v && dwp_id_ok(v) ? v : ""); free(v); free(set);
}
/* 0: ACCT's copy of picture IMG is there (made now from the mirror if it was missing or of another picture) */
static int dwp_ensure(const char *acct, const char *img)
{
	if (!home_acct_ok(acct) || !dwp_id_ok(img)) return -1;
	char d[300], jp[400], ip[400]; snprintf(d, sizeof d, "%s/.rudolf-wallpaper", desk_dir); snprintf(jp, sizeof jp, "%s/%s.jpg", d, acct); snprintf(ip, sizeof ip, "%s/%s.img", d, acct);
	char *was = slurp(ip, NULL); if (was) trim(was);
	int have = !access(jp, R_OK), same = was ? !strcmp(was, img) : have;   /* (a copy the page gave without saying which: kept) */
	free(was); if (have && same) return 0;
	char kj[64], nm[17], rp[400]; snprintf(kj, sizeof kj, "\"wp:%s\"", img); didbm_name("rudolfos", "kv", kj, nm);
	snprintf(rp, sizeof rp, "%s/.rudolf-idb/%s.json", desk_dir, nm);
	size_t rn = 0; char *rec = slurp(rp, &rn), *v = rec ? dj_str(dj_get(rec, "v")) : NULL; free(rec);
	static const char PRE[] = "data:image/jpeg;base64,";
	if (!v || strncmp(v, PRE, sizeof PRE - 1)) { free(v); return have ? 0 : -1; }
	size_t cap = strlen(v) / 4 * 3 + 4; uint8_t *b = malloc(cap); int bn = b ? db64d(v + sizeof PRE - 1, b, cap) : -1; free(v);
	int r = -1;
	if (bn > 2 && b[0] == 0xff && b[1] == 0xd8) {
		if (mkdir(d, 0755) == 0 && chown(d, desk_uid, desk_uid)) {}
		r = dput(jp, (const char *)b, bn) || dput(ip, img, strlen(img)) ? -1 : 0;
	}
	free(b); return r ? (have ? 0 : -1) : 0;
}
/* the account whose picture the sign-in screen shows: the one signed in last (bd95set is its settings) */
static void dlast_acct(dls *l, char *out, size_t n)
{
	char *list = dls_accounts(l); double best = -1; out[0] = 0;
	for (int i = 0; list; i++) { const char *a = dj_at(list, i); if (!a) break; double t = dj_num(dj_get(a, "last"), 0); if (t > best) { best = t; dacct_id(a, out, n); } }
	free(list);
}

/* the account's settings key: bd95set:ID (signed in) or bd95set (the last settings, before) */
static void api_desktop_settings(struct req *r)
{
	dls l; dls_open(&l); dsess s; int in = dsess_read(&s);
	char k[160]; snprintf(k, sizeof k, in ? "bd95set:%s" : "bd95set", s.id);
	char *set = dls_get(&l, k); if (!set && in) set = dls_get(&l, "bd95set");
	static const char *const KEYS[] = { "bs", "b1", "b2", "dyn", "dm", "lat", "wpimg", "wpfit", "tb", "tbstyle", "tbac", "tbacm", "tbbstyle", "tbbac",
		"tbbacm", "twstyle", "twacm", "accent", "darkui", "ck", "h24", "df", "tz", "doff", "ff", "fs", "lang", "scale", "icons", "icstyle", NULL };
	str o = {0}; sadd(&o, "{\"ok\":true,\"gen\":%.0f,\"acct\":", l.file ? dj_num(dj_get(l.file, "gen"), 0) : 0); sjson(&o, in ? s.id : "");
	sadd(&o, ",\"s\":{"); int n = 0;
	for (int i = 0; KEYS[i]; i++) { const char *v = set ? dj_get(set, KEYS[i]) : NULL; if (!v || !dj_end(v, 0)) continue; sadd(&o, "%s\"%s\":", n++ ? "," : "", KEYS[i]); dj_raw(&o, v, "null"); }
	sadd(&o, "},\"pins\":");
	snprintf(k, sizeof k, "bd95pins:%s", in ? s.id : "nobody"); char *pins = dls_get(&l, k);
	if (pins && *dj_ws(pins) == '[' && dj_end(pins, 0)) dj_raw(&o, pins, "[]"); else sadd(&o, "[\"explorer\",\"browser\",\"notepad\",\"settings\"]");
	snprintf(k, sizeof k, "bd95notif:%s", in ? s.id : "nobody"); char *nt = dls_get(&l, k);
	/* the picture of your own: the account's, before sign-in the last account's (whose settings bd95set are) */
	char wa[80] = "", img[48] = ""; if (in) snprintf(wa, sizeof wa, "%s", strcmp(s.id, "guest") ? s.id : ""); else dlast_acct(&l, wa, sizeof wa);
	if (wa[0]) dwp_current(&l, wa, img, sizeof img);
	if (!in && wa[0] && set) { char *v = dj_str(dj_get(set, "wpimg")); if (!v || strcmp(v, img)) img[0] = 0; free(v); }   /* (bd95set is someone else's) */
	int wok = wa[0] && img[0] && !dwp_ensure(wa, img);
	sadd(&o, ",\"notes\":%d,\"wallpaper\":%s,\"wpacct\":", nt ? dj_count(nt) : 0, wok ? "true" : "false"); sjson(&o, wok ? wa : "");
	/* 1.6.0 (Part 12, item 2): the desktop's widgets: the shell's own list while the page's (bd95widgets:ID) is still the one it
	   replaced, else the page's; the weather widget's place and unit (weather47.js: bd95wloc, bd95wunit) */
	char *wg = NULL, *own = NULL; int wgown = 0;
	if (in) {
		snprintf(k, sizeof k, "bd95widgets:%s", s.id); wg = dls_get(&l, k);
		char op[400]; snprintf(op, sizeof op, "%s/.rudolf-widgets/%s.json", desk_dir, s.id); own = home_acct_ok(s.id) ? slurp(op, NULL) : NULL;
		if (own) { char *was = dj_str(dj_get(own, "was")); const char *li = dj_get(own, "list"); wgown = was && li && *dj_ws(li) == '[' && dj_end(li, 0) && !strcmp(was, wg ? wg : ""); free(was); }
	}
	sadd(&o, ",\"widgets\":"); if (wgown) dj_raw(&o, dj_get(own, "list"), "[]"); else if (wg && *dj_ws(wg) == '[' && dj_end(wg, 0)) dj_raw(&o, wg, "[]"); else sadd(&o, "[]");
	sadd(&o, ",\"wgown\":%s,\"wloc\":", wgown ? "true" : "false");
	char *wl = dls_get(&l, "bd95wloc"), *wu = dls_get(&l, "bd95wunit");
	if (wl && *dj_ws(wl) == '{' && dj_end(wl, 0)) dj_raw(&o, wl, "null"); else sadd(&o, "null");
	sadd(&o, ",\"wunit\":"); if (wu && *dj_ws(wu) == '"' && dj_end(wu, 0)) dj_raw(&o, wu, "\"\""); else sadd(&o, "\"\"");
	/* 1.6.0 (Part 12, item 3): the Calendar's events (apps25b.js CALK: bd95cal:ID), for the calendar under the clock */
	char *ce = NULL; if (in) { snprintf(k, sizeof k, "bd95cal:%s", s.id); ce = dls_get(&l, k); }
	sadd(&o, ",\"cal\":"); if (ce && *dj_ws(ce) == '[' && dj_end(ce, 0)) dj_raw(&o, ce, "[]"); else sadd(&o, "[]");
	sadd(&o, "}");
	free(wg); free(own); free(wl); free(wu); free(ce);
	free(set); free(pins); free(nt); dls_close(&l); explicit_bzero(&s, sizeof s); reply_json(r->fd, &o);
}

static void api_desktop_accounts(struct req *r)
{
	dls l; if (!dls_open(&l)) { { static const char E[] = "{\"ok\":true,\"setup\":false,\"accounts\":[],\"guest\":false,\"last\":\"\"}"; reply(r->fd, 200, "application/json", E, sizeof E - 1, NULL); } return; }
	char *list = dls_accounts(&l), *sys = dls_get(&l, "bd95sys"), *setup = dls_get(&l, "bd95setup");
	const char *g = sys ? dj_get(sys, "guest") : NULL;
	str o = {0}; sadd(&o, "{\"ok\":true,\"setup\":%s,\"guest\":%s,\"accounts\":[", setup || list ? "true" : "false", !g || dj_true(g) ? "true" : "false");
	double best = -1; char last[80] = "";
	for (int i = 0; list; i++) {
		const char *a = dj_at(list, i); if (!a) break;
		char id[80], *n = dj_str(dj_get(a, "n")), *role = dj_str(dj_get(a, "r")), *h = dj_str(dj_get(a, "h")); dacct_id(a, id, sizeof id);
		const char *pk = dj_get(a, "pk"); double t = dj_num(dj_get(a, "last"), 0); if (t > best) { best = t; snprintf(last, sizeof last, "%s", id); }
		sadd(&o, "%s{\"id\":", i ? "," : ""); sjson(&o, id); sadd(&o, ",\"n\":"); sjson(&o, n ? n : ""); sadd(&o, ",\"r\":\"%s\",\"key\":%s,\"old\":%s}",
			role && !strcmp(role, "admin") ? "admin" : "user", pk && *dj_ws(pk) != 'n' ? "true" : "false", h && !strncmp(h, "p2$", 3) ? "false" : "true");
		free(n); free(role); dfree_secret(h);
	}
	sadd(&o, "],\"last\":"); sjson(&o, last); sadd(&o, "}");
	free(list); free(sys); free(setup); dls_close(&l); reply_json(r->fd, &o);
}

/* a file the page gives the shell, written whole and owned by the desktop user */
static int dput(const char *path, const char *b, size_t n)
{
	char tp[420]; snprintf(tp, sizeof tp, "%s.tmp", path);
	int f = open(tp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0644); if (f < 0) return -1;
	int ok = write(f, b, n) == (ssize_t)n && !fsync(f); close(f);
	if (!ok || rename(tp, path)) { unlink(tp); return -1; }
	if (chown(path, desk_uid, desk_uid)) {}
	return 0;
}

/* desktop/...: 1 when handled (engine and priority are rudolfd.c's) */
static int api_desktop(struct req *r, const char *w, int post)
{
	if (!strcmp(w, "accounts")) { api_desktop_accounts(r); return 1; }
	if (!strcmp(w, "settings")) { api_desktop_settings(r); return 1; }
	if (!strcmp(w, "login")) { if (!post) reply_err(r->fd, 405, "POST"); else api_desktop_login(r); return 1; }
	if (!strcmp(w, "session")) {
		dsess s; char lp[300]; dpath(lp, sizeof lp, "native-locked");
		if (!dsess_read(&s)) { reply_err(r->fd, 404, "no native session"); return 1; }
		if (!access(lp, F_OK)) { explicit_bzero(&s, sizeof s); reply_err(r->fd, 423, "the screen is locked"); return 1; }
		str o = {0}; sadd(&o, "{\"ok\":true,\"id\":"); sjson(&o, s.id); sadd(&o, ",\"n\":"); sjson(&o, s.n); sadd(&o, ",\"r\":"); sjson(&o, s.r);
		sadd(&o, ",\"pk\":"); sjson(&o, s.pk); sadd(&o, ",\"fk\":"); sjson(&o, s.fk); sadd(&o, "}");
		explicit_bzero(&s, sizeof s); reply_json(r->fd, &o); return 1;
	}
	if (!strcmp(w, "logout")) {
		if (!post) { reply_err(r->fd, 405, "POST"); return 1; }
		dsess s; char p[300], lp[300]; dpath(p, sizeof p, "native-session"); dpath(lp, sizeof lp, "native-locked");
		if (dsess_read(&s) && home_acct_ok(s.id) && strcmp(s.id, "guest")) { int busy = 0; home_lock(s.id, &busy); }
		explicit_bzero(&s, sizeof s); unlink(p); unlink(lp); reply_ok(r->fd, "signed out"); return 1;
	}
	if (!strcmp(w, "lock")) {
		if (!post) { reply_err(r->fd, 405, "POST"); return 1; }
		dsess s; char lp[300]; dpath(lp, sizeof lp, "native-locked");
		if (!dsess_read(&s)) { reply_err(r->fd, 404, "no native session"); return 1; }
		explicit_bzero(&s, sizeof s); write_str(lp, "1"); reply_ok(r->fd, "locked"); return 1;
	}
	if (!strcmp(w, "unlock")) {
		if (!post) { reply_err(r->fd, 405, "POST"); return 1; }
		int wt = dfail_wait(); if (wt) { char m[120]; snprintf(m, sizeof m, "Too many failed attempts. Please wait %d seconds and try again.", wt); reply_err(r->fd, 429, m); return 1; }
		dsess s; if (!dsess_read(&s)) { reply_err(r->fd, 404, "no native session"); return 1; }
		char lp[300]; dpath(lp, sizeof lp, "native-locked"); int okk = !strcmp(s.r, "guest");
		if (!okk) {
			dls l; dls_open(&l); char *list = dls_accounts(&l), *pw = dbody_field(r, "p");
			for (int i = 0; list && pw; i++) { const char *a = dj_at(list, i); if (!a) break; char id[80]; dacct_id(a, id, sizeof id);
				if (!strcmp(id, s.id)) { char *h = dj_str(dj_get(a, "h")); okk = dpw_check(h, pw) == 0; free(h); break; } }
			dfree_secret(pw); free(list); dls_close(&l);
		}
		if (!okk) { dfail_note(0); health_add("signin", "wrong password at the lock screen for the account %.100s (native)", s.n); explicit_bzero(&s, sizeof s); reply_err(r->fd, 403, "The password is incorrect."); return 1; }
		dfail_note(1); explicit_bzero(&s, sizeof s); unlink(lp); reply_ok(r->fd, "unlocked"); return 1;
	}
	if (!strcmp(w, "apps")) {
		char p[300]; snprintf(p, sizeof p, "%s/.rudolf-apps.json", desk_dir);
		if (!post) { size_t n; char *b = slurp(p, &n); if (!b) { static const char E[] = "{\"ok\":true,\"apps\":[],\"bg\":{}}"; reply(r->fd, 200, "application/json", E, sizeof E - 1, NULL); return 1; } reply(r->fd, 200, "application/json", b, n, NULL); free(b); return 1; }
		if (!r->body || r->blen > (1u << 20) || *dj_ws(r->body) != '{' || !dj_end(r->body, 0) || *dj_ws(dj_end(r->body, 0))) { reply_err(r->fd, 400, "a JSON object, at most 1 MB"); return 1; }
		if (dput(p, r->body, r->blen)) reply_err(r->fd, 500, "could not save"); else reply_ok(r->fd, "saved");
		return 1;
	}
	/* 1.6.0 (Part 12, item 1): where the shell put the desktop's icons, per account (the page's own places are in its
	   storage, bd95cells, given in desktop/apps) */
	if (!strcmp(w, "cells")) {
		const char *a = qparam(r->query, "acct"); char d[300], p[400];
		if (!home_acct_ok(a)) { reply_err(r->fd, 400, "bad account"); return 1; }
		snprintf(d, sizeof d, "%s/.rudolf-cells", desk_dir); snprintf(p, sizeof p, "%s/%s.json", d, a);
		if (!post) { size_t n; char *b = slurp(p, &n); if (!b) { static const char E[] = "{}"; reply(r->fd, 200, "application/json", E, 2, NULL); return 1; } reply(r->fd, 200, "application/json", b, n, NULL); free(b); return 1; }
		if (!r->body || r->blen > (64u << 10) || *dj_ws(r->body) != '{' || !dj_end(r->body, 0) || *dj_ws(dj_end(r->body, 0))) { reply_err(r->fd, 400, "a JSON object, at most 64 KB"); return 1; }
		if (mkdir(d, 0755) == 0 && chown(d, desk_uid, desk_uid)) {}
		if (dput(p, r->body, r->blen)) reply_err(r->fd, 500, "could not save"); else reply_ok(r->fd, "saved");
		return 1;
	}
	/* 1.6.0 (Part 12, item 2): the shell's widgets, with the page's value they replace (desktop/settings) */
	if (!strcmp(w, "widgets")) {
		const char *a = qparam(r->query, "acct"); char d[300], p[400];
		if (!home_acct_ok(a)) { reply_err(r->fd, 400, "bad account"); return 1; }
		snprintf(d, sizeof d, "%s/.rudolf-widgets", desk_dir); snprintf(p, sizeof p, "%s/%s.json", d, a);
		if (!post) { size_t n; char *b = slurp(p, &n); if (!b) { reply_err(r->fd, 404, "none"); return 1; } reply(r->fd, 200, "application/json", b, n, NULL); free(b); return 1; }
		if (!r->body || r->blen > (256u << 10) || *dj_ws(r->body) != '[' || !dj_end(r->body, 0) || *dj_ws(dj_end(r->body, 0))) { reply_err(r->fd, 400, "a JSON array, at most 256 KB"); return 1; }
		dls l; dls_open(&l); char k[160]; snprintf(k, sizeof k, "bd95widgets:%s", a); char *was = dls_get(&l, k); dls_close(&l);
		str o = {0}; sadd(&o, "{\"list\":%.*s,\"was\":", (int)r->blen, r->body); sjson(&o, was ? was : ""); sadd(&o, "}"); free(was);
		if (mkdir(d, 0755) == 0 && chown(d, desk_uid, desk_uid)) {}
		if (dput(p, o.b, o.n)) reply_err(r->fd, 500, "could not save"); else reply_ok(r->fd, "saved");
		free(o.b); return 1;
	}
	if (!strcmp(w, "wallpaper")) {
		const char *a = qparam(r->query, "acct"); char d[300], p[400];
		if (!home_acct_ok(a)) { reply_err(r->fd, 400, "bad account"); return 1; }
		snprintf(d, sizeof d, "%s/.rudolf-wallpaper", desk_dir); snprintf(p, sizeof p, "%s/%s.jpg", d, a);
		char ip[400]; snprintf(ip, sizeof ip, "%s/%s.img", d, a);
		if (!post) {
			/* (made from the page's database mirror when missing or of another picture) */
			{ dls l; dls_open(&l); char img[48] = ""; dwp_current(&l, a, img, sizeof img); dls_close(&l); if (img[0]) dwp_ensure(a, img); }
			size_t n; char *b = slurp(p, &n); if (!b) { reply_err(r->fd, 404, "no picture"); return 1; } reply(r->fd, 200, "image/jpeg", b, n, NULL); free(b); return 1; }
		if (!r->body || !r->blen) { unlink(p); unlink(ip); reply_ok(r->fd, "removed"); return 1; }
		if (r->blen > (16u << 20) || (unsigned char)r->body[0] != 0xff || (unsigned char)r->body[1] != 0xd8) { reply_err(r->fd, 400, "a JPEG picture, at most 16 MB"); return 1; }
		if (mkdir(d, 0755) == 0 && chown(d, desk_uid, desk_uid)) {}
		const char *img = qparam(r->query, "img");
		if (dput(p, r->body, r->blen)) reply_err(r->fd, 500, "could not save");
		else { if (dwp_id_ok(img)) dput(ip, img, strlen(img)); else unlink(ip); reply_ok(r->fd, "saved"); }
		return 1;
	}
	return 0;
}
