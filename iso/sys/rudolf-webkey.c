/*
 * rudolf-webkey - Standard's website data under a key kept in this computer's TPM (Rudolf OS 1.6.1,
 * docs/plan-1.6.1.md Part 20; docs/website-data-key.md).
 *
 * Chromium keeps its cookies, saved site passwords and payment cards encrypted with a key of its own. With
 * --password-store=basic (Rudolf OS's until 1.6.1) that key is built into Chromium, the same on every computer
 * ("v10"), so a drive taken out of the computer reads the shared profile's sign-ins as if they were plain. Asked
 * for a key store, Chromium asks it for a password ("Chromium Safe Storage") and encrypts with a key made from it
 * ("v11"). This program is that store for the desktop's Chromium, and the password lives sealed in the TPM:
 *
 *   rudolf-webkey status            (root) the TPM and Secure Boot's measurements, as JSON
 *   rudolf-webkey unseal            (root) the password on standard output. The sealed secret is in
 *                                   /persist/etc/rudolf-webkey/sealed (no secret in it: the TPM's own encryption, which
 *                                   only this TPM opens, and only while PCR 7 and 14 are as they were when it was
 *                                   sealed). Exit 0: unsealed; 3: a new one was made (the first time, or Secure Boot's
 *                                   measurements changed, or the TPM was cleared: what was encrypted before is lost and
 *                                   websites sign in again; the reason on standard error); 1: no TPM 2.0; 2: the TPM
 *                                   would not (its owner has a password, it is locked out, it failed): nothing changed,
 *                                   Chromium's built-in key is used for this start
 *   rudolf-webkey wallet            (the desktop's user) the key store on the session's D-Bus, as KWallet (Chromium's
 *                                   --password-store=kwallet6 and kwallet5): the password read from standard input,
 *                                   given for Chromium's one entry only; nothing written anywhere
 *   rudolf-webkey migrate PROFILE   (the desktop's user, Chromium not running) every "v10" value of the profile's
 *                                   databases (cookies, saved passwords, payment cards) encrypted again as "v11" with the
 *                                   password from standard input, so nothing stays under the built-in key
 *   rudolf-webkey count PROFILE     how many v10 and v11 values the databases hold (tests, System health)
 *
 * Why PCR 7 and 14 only: PCR 7 holds Secure Boot's own state (on or off, the keys PK, KEK, db and dbx) and the
 * certificate each start-up file was checked with (Rudolf OS's, through shim); PCR 14 holds shim's MOK list (the keys
 * enrolled with MokManager, Rudolf OS's among them). A start with Secure Boot off, with other keys, or from a stick
 * signed by another key gives other values, and the TPM refuses the secret. Updates of Rudolf OS change neither (the
 * same certificate checks every release). Not PCR 0-6: they change with every firmware or boot-loader update and with
 * the computer's own settings, which would sign websites out for nothing; not 11-12: Rudolf OS's start-up file measures
 * nothing there.
 *
 * The TPM is spoken to directly (its commands as the TCG's TPM 2.0 Library specification lays them out), through the
 * kernel's resource manager /dev/tpmrm0, which only root may open: no tpm2-tss in the image. The secret is made from
 * the kernel's random numbers; it is sealed under the owner hierarchy's storage key (the TCG's standard ECC P-256
 * template, made again from the TPM's seed each time: nothing is kept in the TPM) as a keyed-hash object that only a
 * policy session with those PCRs can open (no password opens it, noDA: no lockout). Chromium's password is HKDF of it.
 *
 * Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/random.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/crypto.h>
#include <gio/gio.h>

#ifndef WEBKEY_DIR
#define WEBKEY_DIR "/persist/etc/rudolf-webkey"
#endif
#define TPM_DEV "/dev/tpmrm0"

/* ---------------------------------------------------------------- the TPM ---------------------------------------------------------------- */
/* (the password in memory: no core file, no tracing by the same user) */
static int prctl_nodump(void) { return prctl(PR_SET_DUMPABLE, 0, 0, 0, 0); }
static int tpm_fd = -1;
static int tpm_open(void)
{
#ifdef WEBKEY_TEST
	/* (host tests only: swtpm's TCP server; a release is built without it) */
	const char *t = getenv("RUDOLF_TPM_TCP");
	if (t && *t) {
		char h[64]; int port; if (sscanf(t, "%63[^:]:%d", h, &port) != 2) return -1;
		int s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0); struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(port) };
		inet_pton(AF_INET, h, &a.sin_addr);
		if (s < 0 || connect(s, (struct sockaddr *)&a, sizeof a)) { if (s >= 0) close(s); return -1; }
		return tpm_fd = s;
	}
	const char *d = getenv("RUDOLF_TPM_DEV");
	return tpm_fd = open(d && *d ? d : TPM_DEV, O_RDWR | O_CLOEXEC);
#else
	return tpm_fd = open(TPM_DEV, O_RDWR | O_CLOEXEC);
#endif
}

typedef struct { uint8_t b[4096]; size_t n; } buf;
static void put8(buf *x, uint8_t v) { if (x->n < sizeof x->b) x->b[x->n++] = v; }
static void put16(buf *x, uint16_t v) { put8(x, v >> 8); put8(x, v & 0xff); }
static void put32(buf *x, uint32_t v) { put16(x, v >> 16); put16(x, v & 0xffff); }
static void putb(buf *x, const void *p, size_t n) { if (x->n + n <= sizeof x->b) { memcpy(x->b + x->n, p, n); x->n += n; } }
static void put2b(buf *x, const void *p, size_t n) { put16(x, (uint16_t)n); putb(x, p, n); }
/* a reader over an answer */
typedef struct { const uint8_t *p; size_t n, i; int bad; } rd;
static uint8_t get8(rd *r) { if (r->i + 1 > r->n) { r->bad = 1; return 0; } return r->p[r->i++]; }
static uint16_t get16(rd *r) { uint16_t a = get8(r); return (uint16_t)(a << 8 | get8(r)); }
static uint32_t get32(rd *r) { uint32_t a = get16(r); return a << 16 | get16(r); }
static const uint8_t *get2b(rd *r, uint16_t *len) { *len = get16(r); if (r->i + *len > r->n) { r->bad = 1; return NULL; } const uint8_t *q = r->p + r->i; r->i += *len; return q; }

#define ST_NO_SESSIONS 0x8001
#define ST_SESSIONS    0x8002
#define CC_CreatePrimary    0x131
#define CC_Create           0x153
#define CC_Load             0x157
#define CC_Unseal           0x15E
#define CC_FlushContext     0x165
#define CC_StartAuthSession 0x176
#define CC_GetCapability    0x17A
#define CC_PCR_Read         0x17E
#define CC_PolicyPCR        0x17F
#define CC_PolicyGetDigest  0x189
#define RH_OWNER 0x40000001u
#define RH_NULL  0x40000007u
#define RS_PW    0x40000009u
#define ALG_SHA256 0x000B
#define ALG_NULL   0x0010
#define ALG_KEYEDHASH 0x0008
#define ALG_ECC    0x0023
#define ALG_AES    0x0006
#define ALG_CFB    0x0043
#define ECC_P256   0x0003
#define SE_POLICY 0x01
#define SE_TRIAL  0x03
/* PCR 7 and 14 (docs at the top), as a TPMS_PCR_SELECTION's three bytes */
static const uint8_t PCRSEL[3] = { 0x80, 0x40, 0x00 };

static uint32_t last_rc;
/* one command: its answer's parameter area (after the handles, and the parameter size when sessions are on) in *r;
   the answer's response code (0 good; -1 the TPM did not answer) */
static long tpm_cmd(uint16_t tag, uint32_t cc, const buf *body, int nhandles_out, uint32_t *hout, rd *r, uint8_t *ans, size_t ansz)
{
	buf c = {{0}, 0}; put16(&c, tag); put32(&c, 0); put32(&c, cc); putb(&c, body->b, body->n);
	uint32_t L = (uint32_t)c.n; c.b[2] = L >> 24; c.b[3] = L >> 16; c.b[4] = L >> 8; c.b[5] = L;
	if (write(tpm_fd, c.b, c.n) != (ssize_t)c.n) return -1;
	size_t got = 0; ssize_t k;
	while (got < 10 || got < (size_t)(ans[2] << 24 | ans[3] << 16 | ans[4] << 8 | ans[5])) {
		struct pollfd pf = { tpm_fd, POLLIN, 0 }; if (poll(&pf, 1, 120000) <= 0) return -1;
		k = read(tpm_fd, ans + got, ansz - got); if (k <= 0) return -1; got += (size_t)k;
		if (got >= ansz) break;
	}
	rd a = { ans, got, 0, 0 }; get16(&a); uint32_t len = get32(&a); last_rc = get32(&a);
	if (a.bad || len != got) return -1;
	if (last_rc) return (long)last_rc;
	for (int i = 0; i < nhandles_out; i++) hout[i] = get32(&a);
	if (tag == ST_SESSIONS) { uint32_t ps = get32(&a); a.n = a.i + ps; if (a.n > got) return -1; }
	*r = a; return a.bad ? -1 : 0;
}
/* an empty password session for the owner hierarchy and the storage key (TPM_RS_PW) */
static void auth_pw(buf *b) { put32(b, 9); put32(b, RS_PW); put16(b, 0); put8(b, 0); put16(b, 0); }
static void flush(uint32_t h) { buf b = {{0}, 0}; put32(&b, h); rd r; uint8_t a[64]; tpm_cmd(ST_NO_SESSIONS, CC_FlushContext, &b, 0, NULL, &r, a, sizeof a); }

/* the storage key: the TCG's ECC P-256 storage template (restricted, decrypt, AES-128-CFB), made from the owner
   hierarchy's seed: the same key every time on this TPM, until the TPM is cleared */
static long make_primary(uint32_t *h)
{
	buf b = {{0}, 0}; put32(&b, RH_OWNER); auth_pw(&b);
	put16(&b, 4); put16(&b, 0); put16(&b, 0);            /* inSensitive: no password, no data */
	buf p = {{0}, 0};
	put16(&p, ALG_ECC); put16(&p, ALG_SHA256);
	put32(&p, (1u << 1) | (1u << 4) | (1u << 5) | (1u << 6) | (1u << 10) | (1u << 16) | (1u << 17));   /* fixedTPM fixedParent sensitiveDataOrigin userWithAuth noDA restricted decrypt */
	put16(&p, 0);                                        /* authPolicy */
	put16(&p, ALG_AES); put16(&p, 128); put16(&p, ALG_CFB); put16(&p, ALG_NULL); put16(&p, ECC_P256); put16(&p, ALG_NULL);
	put16(&p, 0); put16(&p, 0);                          /* unique: x, y empty */
	put2b(&b, p.b, p.n);
	put16(&b, 0); put32(&b, 0);                          /* outsideInfo, creationPCR */
	rd r; uint8_t a[2048]; return tpm_cmd(ST_SESSIONS, CC_CreatePrimary, &b, 1, h, &r, a, sizeof a);
}
static long start_session(uint8_t type, uint32_t *h)
{
	buf b = {{0}, 0}; put32(&b, RH_NULL); put32(&b, RH_NULL);
	uint8_t nonce[16]; if (getrandom(nonce, sizeof nonce, 0) != sizeof nonce) return -1;
	put2b(&b, nonce, sizeof nonce); put16(&b, 0); put8(&b, type); put16(&b, ALG_NULL); put16(&b, ALG_SHA256);
	rd r; uint8_t a[256]; return tpm_cmd(ST_NO_SESSIONS, CC_StartAuthSession, &b, 1, h, &r, a, sizeof a);
}
static long policy_pcr(uint32_t s)
{
	buf b = {{0}, 0}; put32(&b, s); put16(&b, 0); put32(&b, 1); put16(&b, ALG_SHA256); put8(&b, 3); putb(&b, PCRSEL, 3);
	rd r; uint8_t a[64]; return tpm_cmd(ST_NO_SESSIONS, CC_PolicyPCR, &b, 0, NULL, &r, a, sizeof a);
}
/* the policy every sealed secret has: PolicyPCR over PCR 7 and 14 as they are now (a trial session) */
static long policy_now(uint8_t dig[32])
{
	uint32_t s; long rc = start_session(SE_TRIAL, &s); if (rc) return rc;
	if ((rc = policy_pcr(s))) { flush(s); return rc; }
	buf b = {{0}, 0}; put32(&b, s); rd r; uint8_t a[128];
	if ((rc = tpm_cmd(ST_NO_SESSIONS, CC_PolicyGetDigest, &b, 0, NULL, &r, a, sizeof a))) { flush(s); return rc; }
	uint16_t n; const uint8_t *d = get2b(&r, &n); flush(s);
	if (!d || n != 32) return -1;
	memcpy(dig, d, 32); return 0;
}
/* sealed: TPM2B_PRIVATE and TPM2B_PUBLIC, as the TPM gave them (only this TPM can open them) */
typedef struct { uint8_t priv[1024], pub[512]; uint16_t npriv, npub; } sealed;
static long seal(uint32_t parent, const uint8_t *secret, size_t n, const uint8_t pol[32], sealed *out)
{
	buf b = {{0}, 0}; put32(&b, parent); auth_pw(&b);
	put16(&b, (uint16_t)(4 + n)); put16(&b, 0); put2b(&b, secret, n);      /* inSensitive: no password; the data */
	buf p = {{0}, 0};
	put16(&p, ALG_KEYEDHASH); put16(&p, ALG_SHA256);
	put32(&p, (1u << 1) | (1u << 4) | (1u << 10));       /* fixedTPM fixedParent noDA: no userWithAuth (only the policy opens it) */
	put2b(&p, pol, 32); put16(&p, ALG_NULL); put16(&p, 0);
	put2b(&b, p.b, p.n); put16(&b, 0); put32(&b, 0);
	rd r; uint8_t a[4096]; long rc = tpm_cmd(ST_SESSIONS, CC_Create, &b, 0, NULL, &r, a, sizeof a); if (rc) return rc;
	uint16_t l1, l2; const uint8_t *pv = get2b(&r, &l1), *pb = get2b(&r, &l2);
	if (!pv || !pb || l1 > sizeof out->priv || l2 > sizeof out->pub) return -1;
	memcpy(out->priv, pv, l1); out->npriv = l1; memcpy(out->pub, pb, l2); out->npub = l2; return 0;
}
static long load(uint32_t parent, const sealed *s, uint32_t *h)
{
	buf b = {{0}, 0}; put32(&b, parent); auth_pw(&b); put2b(&b, s->priv, s->npriv); put2b(&b, s->pub, s->npub);
	rd r; uint8_t a[512]; return tpm_cmd(ST_SESSIONS, CC_Load, &b, 1, h, &r, a, sizeof a);
}
static long unseal_obj(uint32_t obj, uint8_t *out, size_t *n)
{
	uint32_t s; long rc = start_session(SE_POLICY, &s); if (rc) return rc;
	if ((rc = policy_pcr(s))) { flush(s); return rc; }
	buf b = {{0}, 0}; put32(&b, obj);
	put32(&b, 9); put32(&b, s); put16(&b, 0); put8(&b, 0); put16(&b, 0);   /* the policy session (ended with the command) */
	rd r; uint8_t a[512]; rc = tpm_cmd(ST_SESSIONS, CC_Unseal, &b, 0, NULL, &r, a, sizeof a);
	if (rc) { flush(s); return rc; }
	uint16_t l; const uint8_t *d = get2b(&r, &l); if (!d || l > *n) { OPENSSL_cleanse(a, sizeof a); return -1; }
	memcpy(out, d, l); *n = l; OPENSSL_cleanse(a, sizeof a); return 0;
}
static long pcr_read(uint8_t v7[32], uint8_t v14[32])
{
	buf b = {{0}, 0}; put32(&b, 1); put16(&b, ALG_SHA256); put8(&b, 3); putb(&b, PCRSEL, 3);
	rd r; uint8_t a[512]; long rc = tpm_cmd(ST_NO_SESSIONS, CC_PCR_Read, &b, 0, NULL, &r, a, sizeof a); if (rc) return rc;
	get32(&r); uint32_t ns = get32(&r); for (uint32_t i = 0; i < ns; i++) { get16(&r); uint8_t k = get8(&r); for (int j = 0; j < k; j++) get8(&r); }
	uint32_t nd = get32(&r); uint16_t l; const uint8_t *d;
	if (nd != 2 || !(d = get2b(&r, &l)) || l != 32) return -1;
	memcpy(v7, d, 32); if (!(d = get2b(&r, &l)) || l != 32) return -1; memcpy(v14, d, 32); return 0;
}
static uint32_t tpm_prop(uint32_t prop)
{
	buf b = {{0}, 0}; put32(&b, 6 /* TPM_CAP_TPM_PROPERTIES */); put32(&b, prop); put32(&b, 1);
	rd r; uint8_t a[256]; if (tpm_cmd(ST_NO_SESSIONS, CC_GetCapability, &b, 0, NULL, &r, a, sizeof a)) return 0;
	get8(&r); get32(&r); if (get32(&r) < 1) return 0; get32(&r); return get32(&r);
}
/* a response code in words (the format-1 codes carry the session's or parameter's number: masked off) */
static const char *rc_words(uint32_t rc)
{
	uint32_t base = (rc & 0x80) ? (rc & 0xBF) : rc;
	switch (base) {
	case 0x9D: return "the policy failed (Secure Boot's measurements, PCR 7 or 14, are not what they were)";
	case 0x9F: return "the sealed secret is not this TPM's any more (cleared, or replaced)";
	case 0x8B: case 0x95: return "the sealed secret is not this TPM's any more (cleared, or replaced)";
	case 0x8E: return "the TPM's owner has a password (set by another system)";
	case 0x921: return "the TPM is locked out (too many wrong passwords)";
	case 0x922: case 0x908: return "the TPM is busy";
	case 0x101: return "the TPM failed";
	case 0x100: return "the TPM was not started (TPM2_Startup)";
	case 0x128: return "a PCR changed while it was being read";
	}
	return NULL;
}
static int rc_policy(uint32_t rc) { return (rc & 0x80) && (rc & 0xBF) == 0x9D; }
static int rc_notmine(uint32_t rc) { uint32_t b = (rc & 0x80) ? (rc & 0xBF) : rc; return b == 0x9F || b == 0x8B || b == 0x95 || b == 0x9A /* size */ || b == 0x84 /* value */; }

/* ---------------------------------------------------------------- the sealed file ---------------------------------------------------------------- */
#define MAGIC "RWK1"
static char wkdir[PATH_MAX] = WEBKEY_DIR;
static int write_sealed(const sealed *s)
{
	char p[PATH_MAX + 16], t[PATH_MAX + 16]; snprintf(p, sizeof p, "%s/sealed", wkdir); snprintf(t, sizeof t, "%s/sealed.t", wkdir);
	if (mkdir(wkdir, 0700) && errno != EEXIST) return -1;
	if (chmod(wkdir, 0700)) return -1;
	int f = open(t, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0600); if (f < 0) return -1;
	buf b = {{0}, 0}; putb(&b, MAGIC, 4); putb(&b, PCRSEL, 3); put2b(&b, s->priv, s->npriv); put2b(&b, s->pub, s->npub);
	int ok = write(f, b.b, b.n) == (ssize_t)b.n && !fsync(f); close(f);
	if (!ok || rename(t, p)) { unlink(t); return -1; }
	return 0;
}
/* 0 read; -1 none; -2 unreadable (damaged, another format) */
static int read_sealed(sealed *s)
{
	char p[PATH_MAX + 16]; snprintf(p, sizeof p, "%s/sealed", wkdir);
	int f = open(p, O_RDONLY | O_CLOEXEC | O_NOFOLLOW); if (f < 0) return -1;
	uint8_t d[2048]; ssize_t n = read(f, d, sizeof d); close(f);
	rd r = { d, n > 0 ? (size_t)n : 0, 0, 0 };
	if (n < 7 || memcmp(d, MAGIC, 4) || memcmp(d + 4, PCRSEL, 3)) return -2;
	r.i = 7; uint16_t l1, l2; const uint8_t *pv = get2b(&r, &l1), *pb = get2b(&r, &l2);
	if (!pv || !pb || r.bad || l1 > sizeof s->priv || l2 > sizeof s->pub) return -2;
	memcpy(s->priv, pv, l1); s->npriv = l1; memcpy(s->pub, pb, l2); s->npub = l2; return 0;
}
static void set_aside(void)
{
	char p[PATH_MAX + 16], o[PATH_MAX + 16]; snprintf(p, sizeof p, "%s/sealed", wkdir); snprintf(o, sizeof o, "%s/sealed.old", wkdir);
	if (rename(p, o)) {}
}

/* Chromium's password from the secret: HKDF-SHA-256, 24 bytes, in base64 (32 characters) */
static int derive(const uint8_t *secret, size_t n, char out[64])
{
	uint8_t k[24]; EVP_PKEY_CTX *c = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL); size_t kl = sizeof k; int ok = 0;
	if (c && EVP_PKEY_derive_init(c) > 0 && EVP_PKEY_CTX_set_hkdf_md(c, EVP_sha256()) > 0
	    && EVP_PKEY_CTX_set1_hkdf_salt(c, (const unsigned char *)"rudolf-os website data", 22) > 0
	    && EVP_PKEY_CTX_set1_hkdf_key(c, secret, (int)n) > 0 && EVP_PKEY_CTX_add1_hkdf_info(c, (const unsigned char *)"chromium safe storage", 21) > 0
	    && EVP_PKEY_derive(c, k, &kl) > 0 && kl == sizeof k) ok = 1;
	EVP_PKEY_CTX_free(c);
	if (ok) EVP_EncodeBlock((unsigned char *)out, k, sizeof k);
	OPENSSL_cleanse(k, sizeof k); return ok ? 0 : -1;
}

static int cmd_unseal(void)
{
	if (tpm_open() < 0) { fprintf(stderr, "no TPM 2.0 (%s: %s)\n", TPM_DEV, strerror(errno)); return 1; }
	uint32_t prim; long rc;
	for (int i = 0; i < 3 && (rc = make_primary(&prim)) && ((rc & 0xFFF) == 0x922 || (rc & 0xFFF) == 0x908); i++) usleep(300000);
	if (rc) { const char *w = rc > 0 ? rc_words((uint32_t)rc) : NULL; fprintf(stderr, "the TPM's storage key could not be made: %s (0x%lx)\n", w ? w : "the TPM refused", rc); return 2; }
	sealed s; int have = read_sealed(&s); const char *why = have == -1 ? "the first time" : have == -2 ? "the sealed file was damaged" : NULL;
	uint8_t sec[64]; size_t sn = sizeof sec; char pw[64];
	if (have == 0) {
		uint32_t obj; rc = load(prim, &s, &obj);
		if (!rc) {
			for (int i = 0; i < 3; i++) { sn = sizeof sec; rc = unseal_obj(obj, sec, &sn); if (rc != 0x128) break; }   /* (a PCR changed meanwhile) */
			flush(obj);
		}
		if (!rc && sn == 32) {
			flush(prim);
			if (derive(sec, sn, pw)) { OPENSSL_cleanse(sec, sizeof sec); return 2; }
			OPENSSL_cleanse(sec, sizeof sec); printf("%s\n", pw); OPENSSL_cleanse(pw, sizeof pw); return 0;
		}
		if (rc > 0 && rc_policy((uint32_t)rc)) why = "Secure Boot's measurements changed (PCR 7 or 14: Secure Boot turned off or on, its keys or the enrolled MOK keys changed)";
		else if (rc > 0 && rc_notmine((uint32_t)rc)) why = "the TPM was cleared or is not the one that sealed it";
		else { const char *w = rc > 0 ? rc_words((uint32_t)rc) : NULL; fprintf(stderr, "the TPM would not open the secret: %s (0x%lx); nothing changed\n", w ? w : "it refused", rc); flush(prim); return 2; }
		set_aside();
	}
	/* a new secret, sealed to PCR 7 and 14 as they are now */
	uint8_t pol[32];
	if (getrandom(sec, 32, 0) != 32 || (rc = policy_now(pol)) || (rc = seal(prim, sec, 32, pol, &s))) {
		OPENSSL_cleanse(sec, sizeof sec); const char *w = rc > 0 ? rc_words((uint32_t)rc) : NULL;
		fprintf(stderr, "a new secret could not be sealed: %s (0x%lx)\n", w ? w : "the TPM refused", rc); flush(prim); return 2; }
	flush(prim);
	if (write_sealed(&s)) { OPENSSL_cleanse(sec, sizeof sec); fprintf(stderr, "the sealed secret could not be saved in %s: %s\n", wkdir, strerror(errno)); return 2; }
	if (derive(sec, 32, pw)) { OPENSSL_cleanse(sec, sizeof sec); return 2; }
	OPENSSL_cleanse(sec, sizeof sec);
	fprintf(stderr, "a new key: %s\n", why ? why : "?");
	printf("%s\n", pw); OPENSSL_cleanse(pw, sizeof pw); return 3;
}

static int cmd_status(void)
{
	char sb[8] = "?"; FILE *f = fopen("/etc/rudolf/secureboot", "r"); if (f) { if (fscanf(f, "%7s", sb) != 1) strcpy(sb, "?"); fclose(f); }
	if (tpm_open() < 0) { printf("{\"tpm\":false,\"secureboot\":\"%s\"}\n", sb); return 1; }
	uint32_t fam = tpm_prop(0x100), man = tpm_prop(0x105);
	uint8_t v7[32], v14[32]; long rc = pcr_read(v7, v14);
	char m[5] = {0}; for (int i = 0; i < 4; i++) { char c = (char)(man >> (24 - 8 * i)); m[i] = c >= 32 && c < 127 && c != '"' && c != '\\' ? c : ' '; }
	sealed s; int have = read_sealed(&s);
	printf("{\"tpm\":true,\"family\":\"%c%c%c\",\"maker\":\"%s\",\"secureboot\":\"%s\",\"sealed\":%s", (fam >> 24) & 0x7f, (fam >> 16) & 0x7f, (fam >> 8) & 0x7f, m, sb, have == 0 ? "true" : "false");
	if (!rc) { printf(",\"pcr7\":\""); for (int i = 0; i < 32; i++) printf("%02x", v7[i]); printf("\",\"pcr14\":\""); for (int i = 0; i < 32; i++) printf("%02x", v14[i]); printf("\""); }
	printf("}\n"); return 0;
}

/* ---------------------------------------------------------------- the key store (KWallet's D-Bus) ---------------------------------------------------------------- */
static char wallet_pw[64];
static const char KW_XML[] =
	"<node><interface name='org.kde.KWallet'>"
	"<method name='isEnabled'><arg type='b' direction='out'/></method>"
	"<method name='networkWallet'><arg type='s' direction='out'/></method>"
	"<method name='localWallet'><arg type='s' direction='out'/></method>"
	"<method name='wallets'><arg type='as' direction='out'/></method>"
	"<method name='open'><arg type='s' direction='in'/><arg type='x' direction='in'/><arg type='s' direction='in'/><arg type='i' direction='out'/></method>"
	"<method name='openAsync'><arg type='s' direction='in'/><arg type='x' direction='in'/><arg type='s' direction='in'/><arg type='b' direction='in'/><arg type='i' direction='out'/></method>"
	"<signal name='walletAsyncOpened'><arg type='i'/><arg type='i'/></signal>"
	"<method name='isOpen'><arg type='i' direction='in'/><arg type='b' direction='out'/></method>"
	"<method name='close'><arg type='i' direction='in'/><arg type='b' direction='in'/><arg type='s' direction='in'/><arg type='i' direction='out'/></method>"
	"<method name='hasFolder'><arg type='i' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='b' direction='out'/></method>"
	"<method name='createFolder'><arg type='i' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='b' direction='out'/></method>"
	"<method name='hasEntry'><arg type='i' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='b' direction='out'/></method>"
	"<method name='readPassword'><arg type='i' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='out'/></method>"
	"<method name='writePassword'><arg type='i' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='i' direction='out'/></method>"
	"</interface></node>";
#define KW_FOLDER "Chromium Keys"
#define KW_ENTRY "Chromium Safe Storage"
static void kw_call(GDBusConnection *c, const gchar *sender, const gchar *path, const gchar *iface, const gchar *m, GVariant *a, GDBusMethodInvocation *inv, gpointer u)
{
	(void)sender; (void)iface; (void)u;
	if (!strcmp(m, "isEnabled")) g_dbus_method_invocation_return_value(inv, g_variant_new("(b)", TRUE));
	else if (!strcmp(m, "networkWallet") || !strcmp(m, "localWallet")) g_dbus_method_invocation_return_value(inv, g_variant_new("(s)", "kdewallet"));
	else if (!strcmp(m, "wallets")) { const gchar *w[] = { "kdewallet", NULL }; g_dbus_method_invocation_return_value(inv, g_variant_new("(^as)", w)); }
	else if (!strcmp(m, "open")) g_dbus_method_invocation_return_value(inv, g_variant_new("(i)", 1));
	else if (!strcmp(m, "openAsync")) {
		/* (Chromium 157's own: the answer is a transaction's number, the wallet's handle comes after in walletAsyncOpened) */
		static gint32 tid; gint32 t = ++tid; g_dbus_method_invocation_return_value(inv, g_variant_new("(i)", t));
		g_dbus_connection_emit_signal(c, NULL, path, "org.kde.KWallet", "walletAsyncOpened", g_variant_new("(ii)", t, 1), NULL);
	}
	else if (!strcmp(m, "isOpen")) g_dbus_method_invocation_return_value(inv, g_variant_new("(b)", TRUE));
	else if (!strcmp(m, "close")) g_dbus_method_invocation_return_value(inv, g_variant_new("(i)", 0));
	else if (!strcmp(m, "hasFolder") || !strcmp(m, "createFolder")) { gint32 h; const gchar *f, *app; g_variant_get(a, "(i&s&s)", &h, &f, &app); g_dbus_method_invocation_return_value(inv, g_variant_new("(b)", !strcmp(f, KW_FOLDER))); }
	else if (!strcmp(m, "hasEntry") || !strcmp(m, "readPassword")) {
		gint32 h; const gchar *f, *k, *app; g_variant_get(a, "(i&s&s&s)", &h, &f, &k, &app);
		int mine = !strcmp(f, KW_FOLDER) && !strcmp(k, KW_ENTRY);
		if (!strcmp(m, "hasEntry")) g_dbus_method_invocation_return_value(inv, g_variant_new("(b)", mine));
		else g_dbus_method_invocation_return_value(inv, g_variant_new("(s)", mine ? wallet_pw : ""));
	} else if (!strcmp(m, "writePassword")) {
		/* (Chromium writes only when it read an empty password, which it never does here: anything else is refused,
		    as is every other entry: this store holds one password and keeps nothing) */
		gint32 h; const gchar *f, *k, *v, *app; g_variant_get(a, "(i&s&s&s&s)", &h, &f, &k, &v, &app);
		g_dbus_method_invocation_return_value(inv, g_variant_new("(i)", !strcmp(f, KW_FOLDER) && !strcmp(k, KW_ENTRY) && !strcmp(v, wallet_pw) ? 0 : -1));
	} else g_dbus_method_invocation_return_dbus_error(inv, "org.freedesktop.DBus.Error.UnknownMethod", "not in this key store");
}
static const GDBusInterfaceVTable KW_VT = { kw_call, NULL, NULL, { 0 } };
static int wallet_names = 0;
static void got_name(GDBusConnection *c, const gchar *n, gpointer u) { (void)c; (void)u; fprintf(stderr, "rudolf-webkey: %s\n", n); if (++wallet_names == 2) { printf("ready\n"); fflush(stdout); } }
static void lost_name(GDBusConnection *c, const gchar *n, gpointer u) { (void)u; if (!c) { fprintf(stderr, "rudolf-webkey: no session bus\n"); exit(1); } fprintf(stderr, "rudolf-webkey: %s is another program's\n", n); exit(1); }
static void bus_up(GDBusConnection *c, const gchar *n, gpointer u)
{
	(void)u; GDBusNodeInfo *ni = g_dbus_node_info_new_for_xml(KW_XML, NULL);
	const char *path = strstr(n, "kwalletd6") ? "/modules/kwalletd6" : "/modules/kwalletd5";
	g_dbus_connection_register_object(c, path, ni->interfaces[0], &KW_VT, NULL, NULL, NULL);
}
static int read_pw_stdin(char *out, size_t n)
{
	if (!fgets(out, (int)n, stdin)) return -1;
	out[strcspn(out, "\r\n")] = 0;
	size_t l = strlen(out); if (l < 16) return -1;
	for (size_t i = 0; i < l; i++) if (!((out[i] >= 'A' && out[i] <= 'Z') || (out[i] >= 'a' && out[i] <= 'z') || (out[i] >= '0' && out[i] <= '9') || out[i] == '+' || out[i] == '/' || out[i] == '=')) return -1;
	return 0;
}
static int cmd_wallet(void)
{
	if (read_pw_stdin(wallet_pw, sizeof wallet_pw)) { fprintf(stderr, "rudolf-webkey wallet: no password on standard input\n"); return 2; }
	if (prctl_nodump()) {}
	g_bus_own_name(G_BUS_TYPE_SESSION, "org.kde.kwalletd6", G_BUS_NAME_OWNER_FLAGS_NONE, bus_up, got_name, lost_name, NULL, NULL);
	g_bus_own_name(G_BUS_TYPE_SESSION, "org.kde.kwalletd5", G_BUS_NAME_OWNER_FLAGS_NONE, bus_up, got_name, lost_name, NULL, NULL);
	GMainLoop *l = g_main_loop_new(NULL, FALSE); g_main_loop_run(l); return 0;
}

/* ---------------------------------------------------------------- the profile's databases ---------------------------------------------------------------- */
/* (SQLite through the base layer's own libsqlite3, opened when needed: nothing new to build against) */
typedef struct sqlite3 sqlite3; typedef struct sqlite3_stmt sqlite3_stmt;
static int (*q_open)(const char *, sqlite3 **, int, const char *);
static int (*q_prepare)(sqlite3 *, const char *, int, sqlite3_stmt **, const char **);
static int (*q_step)(sqlite3_stmt *);
static const void *(*q_blob)(sqlite3_stmt *, int);
static int (*q_bytes)(sqlite3_stmt *, int);
static long long (*q_int64)(sqlite3_stmt *, int);
static int (*q_bind_blob)(sqlite3_stmt *, int, const void *, int, void (*)(void *));
static int (*q_bind_int64)(sqlite3_stmt *, int, long long);
static int (*q_finalize)(sqlite3_stmt *);
static int (*q_exec)(sqlite3 *, const char *, void *, void *, char **);
static int (*q_close)(sqlite3 *);
static int (*q_busy)(sqlite3 *, int);
static int sql_load(void)
{
	if (q_open) return 0;
	void *h = dlopen("libsqlite3.so.0", RTLD_NOW | RTLD_LOCAL); if (!h) return -1;
#define L(v, n) if (!(*(void **)&v = dlsym(h, n))) return -1;
	L(q_open, "sqlite3_open_v2") L(q_prepare, "sqlite3_prepare_v2") L(q_step, "sqlite3_step") L(q_blob, "sqlite3_column_blob")
	L(q_bytes, "sqlite3_column_bytes") L(q_int64, "sqlite3_column_int64") L(q_bind_blob, "sqlite3_bind_blob") L(q_bind_int64, "sqlite3_bind_int64")
	L(q_finalize, "sqlite3_finalize") L(q_exec, "sqlite3_exec") L(q_close, "sqlite3_close") L(q_busy, "sqlite3_busy_timeout")
#undef L
	return 0;
}
#define SQLITE_ROW 100
#define SQLITE_DONE 101
#define SQLITE_OPEN_READWRITE 0x2
#define SQLITE_TRANSIENT ((void (*)(void *))-1)
/* Chromium's key from a password: PBKDF2-HMAC-SHA1, salt "saltysalt", 1 round, 16 bytes; AES-128-CBC, the IV 16 spaces */
static void ckey(const char *pw, uint8_t k[16]) { PKCS5_PBKDF2_HMAC_SHA1(pw, (int)strlen(pw), (const unsigned char *)"saltysalt", 9, 1, 16, k); }
static int aes(int enc, const uint8_t k[16], const uint8_t *in, int n, uint8_t *out, int *on)
{
	static const uint8_t iv[16] = "                "; EVP_CIPHER_CTX *c = EVP_CIPHER_CTX_new(); int a = 0, b = 0, ok;
	ok = c && EVP_CipherInit_ex(c, EVP_aes_128_cbc(), NULL, k, iv, enc) > 0 && EVP_CipherUpdate(c, out, &a, in, n) > 0 && EVP_CipherFinal_ex(c, out + a, &b) > 0;
	EVP_CIPHER_CTX_free(c); *on = a + b; return ok ? 0 : -1;
}
/* the databases and their encrypted columns, under each profile folder */
static const struct { const char *db, *table, *col; } ENC[] = {
	{ "Cookies", "cookies", "encrypted_value" }, { "Network/Cookies", "cookies", "encrypted_value" },
	{ "Login Data", "logins", "password_value" }, { "Login Data For Account", "logins", "password_value" },
	{ "Web Data", "credit_cards", "card_number_encrypted" }, { "Web Data", "token_service", "encrypted_token" },
	{ "Account Web Data", "credit_cards", "card_number_encrypted" }, { NULL, NULL, NULL }
};
/* count (or, with to, re-encrypt) the v10 values of one column: n10, n11 counted; -1 on a failure */
static int one_col(const char *path, const char *table, const char *col, const uint8_t *k10, const uint8_t *k11, long *n10, long *n11)
{
	sqlite3 *db; if (q_open(path, &db, SQLITE_OPEN_READWRITE, NULL)) { q_close(db); return 0; }
	q_busy(db, 5000);
	char sql[256]; sqlite3_stmt *st; int rc = 0;
	snprintf(sql, sizeof sql, "SELECT rowid, %s FROM \"%s\"", col, table);
	if (q_prepare(db, sql, -1, &st, NULL)) { q_close(db); return 0; }   /* (no such table in this version: nothing to do) */
	if (k11 && q_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL)) { q_finalize(st); q_close(db); return -1; }
	snprintf(sql, sizeof sql, "UPDATE \"%s\" SET %s = ?1 WHERE rowid = ?2", table, col); sqlite3_stmt *up = NULL;
	if (k11 && q_prepare(db, sql, -1, &up, NULL)) rc = -1;
	while (!rc && q_step(st) == SQLITE_ROW) {
		long long id = q_int64(st, 0); const uint8_t *v = q_blob(st, 1); int n = q_bytes(st, 1);
		if (n < 3 || !v) continue;
		if (!memcmp(v, "v11", 3)) { (*n11)++; continue; }
		if (memcmp(v, "v10", 3)) continue;
		(*n10)++;
		if (!k11) continue;
		uint8_t *pt = malloc((size_t)n + 32), *ct = malloc((size_t)n + 64); int pn, cn;
		if (!pt || !ct || aes(0, k10, v + 3, n - 3, pt, &pn) || aes(1, k11, pt, pn, ct + 3, &cn)) { if (pt) OPENSSL_cleanse(pt, (size_t)n); free(pt); free(ct); continue; }   /* (not the built-in key's: left as it is) */
		memcpy(ct, "v11", 3); OPENSSL_cleanse(pt, (size_t)pn); free(pt);
		q_bind_blob(up, 1, ct, cn + 3, SQLITE_TRANSIENT); q_bind_int64(up, 2, id);
		if (q_step(up) != SQLITE_DONE) rc = -1;
		q_finalize(up); up = NULL; if (q_prepare(db, sql, -1, &up, NULL)) rc = -1;
		free(ct); (*n10)--; (*n11)++;
	}
	q_finalize(st); if (up) q_finalize(up);
	if (k11) { if (q_exec(db, rc ? "ROLLBACK" : "COMMIT", NULL, NULL, NULL)) rc = -1; }
	q_close(db); return rc;
}
static int cmd_profile(const char *prof, int migrate)
{
	if (sql_load()) { fprintf(stderr, "rudolf-webkey: libsqlite3 could not be loaded\n"); return 2; }
	char pw[64]; uint8_t k10[16], k11[16]; ckey("peanuts", k10);
	if (migrate) { if (read_pw_stdin(pw, sizeof pw)) { fprintf(stderr, "rudolf-webkey migrate: no password on standard input\n"); return 2; } ckey(pw, k11); OPENSSL_cleanse(pw, sizeof pw); }
	long n10 = 0, n11 = 0; int bad = 0;
	/* the profile's folders: Default and "Profile N" */
	GDir *d = g_dir_open(prof, 0, NULL); const char *e;
	while (d && (e = g_dir_read_name(d))) {
		if (strcmp(e, "Default") && strncmp(e, "Profile ", 8)) continue;
		for (int i = 0; ENC[i].db; i++) {
			char p[PATH_MAX]; snprintf(p, sizeof p, "%s/%s/%s", prof, e, ENC[i].db);
			struct stat st; if (lstat(p, &st) || !S_ISREG(st.st_mode)) continue;
			if (one_col(p, ENC[i].table, ENC[i].col, k10, migrate ? k11 : NULL, &n10, &n11)) bad++;
		}
	}
	if (d) g_dir_close(d);
	OPENSSL_cleanse(k11, sizeof k11);
	printf("v10 %ld v11 %ld%s\n", n10, n11, bad ? " (some could not be changed: is Chromium running?)" : "");
	return bad ? 1 : 0;
}

int main(int argc, char **argv)
{
#ifdef WEBKEY_TEST
	{ const char *w = getenv("RUDOLF_WEBKEY_DIR"); if (w && *w) snprintf(wkdir, sizeof wkdir, "%s", w); }
#endif
	if (argc == 2 && !strcmp(argv[1], "status")) return cmd_status();
	if (argc == 2 && !strcmp(argv[1], "unseal")) { prctl_nodump(); return cmd_unseal(); }
	if (argc == 2 && !strcmp(argv[1], "wallet")) return cmd_wallet();
	if (argc == 3 && !strcmp(argv[1], "migrate")) { prctl_nodump(); return cmd_profile(argv[2], 1); }
	if (argc == 3 && !strcmp(argv[1], "count")) return cmd_profile(argv[2], 0);
	fprintf(stderr, "usage: rudolf-webkey status | unseal | wallet | migrate PROFILE | count PROFILE\n"); return 2;
}
