/* (included by rudolfd.c after rudolfd-desktop.inc, not built on its own) ---------- 1.6.1: security keys that unlock the
 * files (docs/plan-1.6.1.md Part 18; Part 17 items 1, 2, 4 and 6). docs/security-keys.md says what it protects and what to
 * do when keys are lost.
 *
 * rudolfd talks to FIDO2 security keys itself, with libfido2 (libfido2.so.1, opened when first needed with dlopen, so
 * rudolfd links nothing new; /dev/hidraw* is root's): Standard and Lite (the desktop page) and Native (rudolf-native) all
 * sign in through these requests, so the page cannot skip the key.
 *
 * The account's file key (fk, the key its home folder's encryption comes from) and, for an administrator, its private key
 * (priv: it opens the copies of other accounts' file keys their administrators keep) are sealed (AES-256-GCM) under
 *     W = HKDF-SHA-256(IKM = PWS ‖ KSEC, salt "rudolf-os file key v1", info = the account's id)
 * PWS, the password's secret, is kept in the account (the page's "fks" box, sealed with the password's key as its other
 * boxes are, so a new password only seals it again: acRekey, no key needed). KSEC, the key secret, is kept once for each
 * key, sealed under HKDF-SHA-256(that key's hmac-secret output for the account's salt, salt "rudolf-os security key v1",
 * info = the id). The hmac-secret output is computed inside the key from a secret that never leaves it (FIDO2's
 * hmac-secret extension), asked for without user verification (no PIN) every time, so a PIN set or removed later does
 * not change it. So the files need the password and one of the keys.
 * Until keys are required, the password's own copies stay (fk and priv in the account, and KSEC sealed with the
 * password's key in the record, "kp", for adding a second key without the first): the key is a second step at sign-in.
 * "Require a security key" (two keys at least: a spare) deletes "kp" here and the page deletes its fk and priv, so from
 * then on a copy of the disk opens nothing without one of the keys.
 *
 * The keys' records are rudolfd's, root-only: /persist/etc/fido/ID.json {"v":2,"salt","req","fk","pv","kp","keys":[{"name",
 * "cid","pub","ks","cnt","added","last"}]} (cid: the credential's id; pub: its P-256 public key, x‖y; fk, pv, kp, ks:
 * IV ‖ sealed ‖ tag).
 * Every touch is checked here: the assertion's signature over a new random challenge with the key's public key (so a
 * replayed answer is refused), user presence, and the signature counter (it must grow when the key counts).
 *
 *   GET  desktop/keys?acct=ID      {"keys":[{"i","name","added","last"}],"req","lib","devs"}   (no secrets)
 *   POST desktop/keys/add          {"n","p","name","pin"?,"k"?}: the password, then (keys required) a touch of one of the
 *                                  account's keys, then the new key touched twice (its credential, then its hmac-secret);
 *                                  "pin": the new key's PIN when it has one (asked on screen, never kept): 401 need "pin".
 *                                  The first key's answer has "fks", the box the page keeps in the account
 *   POST desktop/keys/remove       {"n","p","i"}  (not below two while they are required)
 *   POST desktop/keys/require      {"n","p","on","k"}: on needs two keys and a touch (the whole way checked: the key, the
 *                                  page's box, the file key); off needs a touch (the page then makes the password's copy
 *                                  again)
 *   POST desktop/keys/signin       {"n","p","k"?}: the page's sign-in and lock screen: without "k", 401 {"need":"key"} once
 *                                  the password is right (the screen then says to touch the key); with "k", waits 30 s
 *                                  for a touch: {"pk","fk","pv"?,"fks"?} ("fks": a new box for the page to keep, made
 *                                  when its own was lost while keys were not required)
 *   POST desktop/keys/reset        {"n","p","acct"}: an administrator removes another account's keys (its password was
 *                                  reset with the administrator's copy of its file key); the administrator's own key
 *                                  is touched when it has keys
 * desktop/login and desktop/unlock (Native) ask for the key the same way: 401 {"need":"key"} after a right password,
 * then {"k":true} waits for the touch. A key is the second step: an account with keys is not asked for a code too.
 * Wrong passwords at these requests are counted per account as the codes are (desktop/second: 5, then 30 s doubling). */
#include <dlfcn.h>
#include <stdbool.h>
#include <sys/random.h>
#define DF_MAX 5
#define DF_RP "rudolf-os.local"
static char fido_dir[256] = PERSIST "/etc/fido";
/* ---------------------------------------------------------------- libfido2, opened when needed (its ABI: libfido2.so.1) */
typedef struct dfx_dev dfx_dev; typedef struct dfx_info dfx_info; typedef struct dfx_cred dfx_cred; typedef struct dfx_assert dfx_assert;
typedef struct dfx_cbor dfx_cbor; typedef struct dfx_es256 dfx_es256;
typedef struct { void *(*open)(const char *); void (*close)(void *); int (*read)(void *, unsigned char *, size_t, int); int (*write)(void *, const unsigned char *, size_t); } dfx_io;
enum { DFX_OPT_OMIT = 0, DFX_OPT_FALSE = 1, DFX_OPT_TRUE = 2 };
#define DFX_EXT_HMAC 0x01
#define DFX_COSE_ES256 (-7)
#define DFX_ERR_UNSUPPORTED_EXT 0x16
#define DFX_ERR_EXCLUDED 0x19
#define DFX_ERR_UNSUPPORTED_OPT 0x2b
#define DFX_ERR_NO_CREDS 0x2e
#define DFX_ERR_USER_TIMEOUT 0x2f
#define DFX_ERR_PIN_INVALID 0x31
#define DFX_ERR_PIN_BLOCKED 0x32
#define DFX_ERR_PIN_AUTH_BLOCKED 0x34
#define DFX_ERR_PIN_REQUIRED 0x36
#define DFX_ERR_ACTION_TIMEOUT 0x3a
#define DFX_FN(X) \
	X(void, fido_init, (int)) X(dfx_info *, fido_dev_info_new, (size_t)) X(void, fido_dev_info_free, (dfx_info **, size_t)) \
	X(int, fido_dev_info_manifest, (dfx_info *, size_t, size_t *)) X(const dfx_info *, fido_dev_info_ptr, (const dfx_info *, size_t)) \
	X(const char *, fido_dev_info_path, (const dfx_info *)) X(dfx_dev *, fido_dev_new, (void)) X(void, fido_dev_free, (dfx_dev **)) \
	X(int, fido_dev_open, (dfx_dev *, const char *)) X(int, fido_dev_close, (dfx_dev *)) X(int, fido_dev_set_io_functions, (dfx_dev *, const dfx_io *)) \
	X(bool, fido_dev_is_fido2, (const dfx_dev *)) X(bool, fido_dev_has_pin, (const dfx_dev *)) X(int, fido_dev_set_timeout, (dfx_dev *, int)) \
	X(dfx_cbor *, fido_cbor_info_new, (void)) X(void, fido_cbor_info_free, (dfx_cbor **)) X(int, fido_dev_get_cbor_info, (dfx_dev *, dfx_cbor *)) \
	X(char **, fido_cbor_info_extensions_ptr, (const dfx_cbor *)) X(size_t, fido_cbor_info_extensions_len, (const dfx_cbor *)) \
	X(dfx_cred *, fido_cred_new, (void)) X(void, fido_cred_free, (dfx_cred **)) X(int, fido_cred_set_type, (dfx_cred *, int)) \
	X(int, fido_cred_set_clientdata_hash, (dfx_cred *, const unsigned char *, size_t)) X(int, fido_cred_set_rp, (dfx_cred *, const char *, const char *)) \
	X(int, fido_cred_set_user, (dfx_cred *, const unsigned char *, size_t, const char *, const char *, const char *)) \
	X(int, fido_cred_set_extensions, (dfx_cred *, int)) X(int, fido_cred_set_rk, (dfx_cred *, int)) X(int, fido_cred_exclude, (dfx_cred *, const unsigned char *, size_t)) \
	X(int, fido_dev_make_cred, (dfx_dev *, dfx_cred *, const char *)) X(const unsigned char *, fido_cred_id_ptr, (const dfx_cred *)) X(size_t, fido_cred_id_len, (const dfx_cred *)) \
	X(const unsigned char *, fido_cred_pubkey_ptr, (const dfx_cred *)) X(size_t, fido_cred_pubkey_len, (const dfx_cred *)) X(int, fido_cred_type, (const dfx_cred *)) \
	X(dfx_assert *, fido_assert_new, (void)) X(void, fido_assert_free, (dfx_assert **)) X(int, fido_assert_set_clientdata_hash, (dfx_assert *, const unsigned char *, size_t)) \
	X(int, fido_assert_set_rp, (dfx_assert *, const char *)) X(int, fido_assert_allow_cred, (dfx_assert *, const unsigned char *, size_t)) \
	X(int, fido_assert_set_extensions, (dfx_assert *, int)) X(int, fido_assert_set_hmac_salt, (dfx_assert *, const unsigned char *, size_t)) \
	X(int, fido_assert_set_up, (dfx_assert *, int)) X(int, fido_dev_get_assert, (dfx_dev *, dfx_assert *, const char *)) X(size_t, fido_assert_count, (const dfx_assert *)) \
	X(const unsigned char *, fido_assert_id_ptr, (const dfx_assert *, size_t)) X(size_t, fido_assert_id_len, (const dfx_assert *, size_t)) \
	X(const unsigned char *, fido_assert_hmac_secret_ptr, (const dfx_assert *, size_t)) X(size_t, fido_assert_hmac_secret_len, (const dfx_assert *, size_t)) \
	X(uint32_t, fido_assert_sigcount, (const dfx_assert *, size_t)) X(uint8_t, fido_assert_flags, (const dfx_assert *, size_t)) \
	X(int, fido_assert_verify, (const dfx_assert *, size_t, int, const void *)) X(dfx_es256 *, es256_pk_new, (void)) X(void, es256_pk_free, (dfx_es256 **)) \
	X(int, es256_pk_from_ptr, (dfx_es256 *, const void *, size_t)) X(const char *, fido_strerr, (int))
#define DFX_PTR(r, n, a) r (*n) a;
static struct { int tried, ok; DFX_FN(DFX_PTR) } FX;
static int dfx_load(void)
{
	if (FX.tried) return FX.ok;
	FX.tried = 1; void *h = dlopen("libfido2.so.1", RTLD_NOW | RTLD_LOCAL); if (!h) return 0;
#define DFX_SYM(r, n, a) if (!(FX.n = (r (*) a)dlsym(h, #n))) { dlclose(h); return 0; }
	DFX_FN(DFX_SYM)
	FX.fido_init(0); FX.ok = 1; return 1;
}
/* ---------------------------------------------------------------- where the keys are: hidraw (libfido2's list), or the tests' */
#ifdef RUDOLFD_FIDO_TEST
/* (the host tests only, never in a release: RUDOLFD_FIDO_SOCKS is a list of Unix sockets, each a software key, vfido.py;
   the reports go over it as they would to /dev/hidrawN, without the report ID) */
static void *dft_open(const char *p) { int *f = malloc(sizeof *f); *f = socket(AF_UNIX, SOCK_STREAM, 0); struct sockaddr_un a = {.sun_family = AF_UNIX};
	snprintf(a.sun_path, sizeof a.sun_path, "%s", p); if (*f < 0 || connect(*f, (struct sockaddr *)&a, sizeof a)) { if (*f >= 0) close(*f); free(f); return NULL; } return f; }
static void dft_close(void *h) { close(*(int *)h); free(h); }
static int dft_read(void *h, unsigned char *b, size_t n, int ms) { int f = *(int *)h; size_t got = 0;
	while (got < 64 && got < n) { struct pollfd p = {f, POLLIN, 0}; if (poll(&p, 1, ms < 0 ? -1 : ms) <= 0) return -1; ssize_t k = read(f, b + got, (n < 64 ? n : 64) - got); if (k <= 0) return -1; got += k; } return (int)got; }
static int dft_write(void *h, const unsigned char *b, size_t n) { int f = *(int *)h; if (n == 65) { b++; n = 64; } return write(f, b, n) == (ssize_t)n ? (int)(n + (n == 64)) : -1; }
static int dfx_devs(char paths[][256], int max) { const char *e = getenv("RUDOLFD_FIDO_SOCKS"); int n = 0; if (!e) return 0;
	char t[2048]; snprintf(t, sizeof t, "%s", e); for (char *s = strtok(t, ","); s && n < max; s = strtok(NULL, ",")) if (!access(s, F_OK)) snprintf(paths[n++], 256, "%s", s); return n; }
static dfx_dev *dfx_open(const char *path) { static const dfx_io io = { dft_open, dft_close, dft_read, dft_write };
	dfx_dev *d = FX.fido_dev_new(); if (!d) return NULL; FX.fido_dev_set_io_functions(d, &io); if (FX.fido_dev_open(d, path)) { FX.fido_dev_free(&d); return NULL; } return d; }
#else
static int dfx_devs(char paths[][256], int max)
{
	dfx_info *l = FX.fido_dev_info_new(16); size_t n = 0; int k = 0; if (!l) return 0;
	if (!FX.fido_dev_info_manifest(l, 16, &n)) for (size_t i = 0; i < n && k < max; i++) { const char *p = FX.fido_dev_info_path(FX.fido_dev_info_ptr(l, i)); if (p) snprintf(paths[k++], 256, "%s", p); }
	FX.fido_dev_info_free(&l, 16); return k;
}
static dfx_dev *dfx_open(const char *path) { dfx_dev *d = FX.fido_dev_new(); if (!d) return NULL; if (FX.fido_dev_open(d, path)) { FX.fido_dev_free(&d); return NULL; } return d; }
#endif
static void dfx_close(dfx_dev **d) { if (*d) { FX.fido_dev_close(*d); FX.fido_dev_free(d); } }
/* ---------------------------------------------------------------- HKDF-SHA-256 and AES-256-GCM sealing (the opening is desktop.inc's) */
static void dhkdf256(const uint8_t *ikm, size_t il, const char *salt, const char *info, uint8_t out[32])
{
	dhmac256k m; uint8_t prk[32]; dhmac256_key(&m, (const uint8_t *)salt, strlen(salt)); dhmac256(&m, ikm, il, prk);
	size_t n = strlen(info); uint8_t *b = malloc(n + 1); memcpy(b, info, n); b[n] = 1;
	dhmac256_key(&m, prk, 32); dhmac256(&m, b, n + 1, out); free(b); explicit_bzero(prk, sizeof prk); explicit_bzero(&m, sizeof m);
}
/* AES-256-GCM: out = ct ‖ tag (n + 16 bytes), as WebCrypto seals */
static void daes_gcm_seal(const uint8_t *key, const uint8_t *iv, const uint8_t *pt, size_t n, uint8_t *out)
{
	uint8_t rk[240], h[16] = {0}, j0[16], ctr[16], ks[16], g[16] = {0};
	daes256_expand(key, rk); daes256_block(rk, h, h);
	memcpy(j0, iv, 12); j0[12] = j0[13] = j0[14] = 0; j0[15] = 1; memcpy(ctr, j0, 16);
	for (size_t i = 0; i < n; i += 16) {
		for (int j = 15; j >= 12; j--) if (++ctr[j]) break;
		daes256_block(rk, ctr, ks); size_t k = n - i < 16 ? n - i : 16; for (size_t j = 0; j < k; j++) out[i + j] = pt[i + j] ^ ks[j];
	}
	for (size_t i = 0; i < n; i += 16) { size_t k = n - i < 16 ? n - i : 16; for (size_t j = 0; j < k; j++) g[j] ^= out[i + j]; dgf_mul(g, h); }
	uint8_t lb[16] = {0}; uint64_t cb = (uint64_t)n * 8; for (int j = 0; j < 8; j++) lb[15 - j] = cb >> (8 * j);
	for (int j = 0; j < 16; j++) g[j] ^= lb[j];
	dgf_mul(g, h); daes256_block(rk, j0, ks); for (int j = 0; j < 16; j++) out[n + j] = g[j] ^ ks[j];
	explicit_bzero(rk, sizeof rk); explicit_bzero(ks, sizeof ks);
}
static void drandom(uint8_t *b, size_t n) { if (getrandom(b, n, 0) != (ssize_t)n) { int f = open("/dev/urandom", O_RDONLY | O_CLOEXEC); if (f < 0 || read(f, b, n) != (ssize_t)n) abort(); close(f); } }
/* KEK for one key: from the password's key and that key's hmac-secret output */
/* the keys of the scheme: a key's own (from its hmac-secret alone) seals the account's key secret; the files' (from the
   password's secret and the key secret together) seals the file key and the administrator's private key */
static void dfido_kkey(const uint8_t hs[32], const char *id, uint8_t k[32]) { dhkdf256(hs, 32, "rudolf-os security key v1", id, k); }
static void dfido_wkey(const uint8_t pws[32], const uint8_t ksec[32], const char *id, uint8_t k[32])
{
	uint8_t ikm[64]; memcpy(ikm, pws, 32); memcpy(ikm + 32, ksec, 32); dhkdf256(ikm, 64, "rudolf-os file key v1", id, k); explicit_bzero(ikm, sizeof ikm);
}
/* sealed = IV(12) ‖ ct ‖ tag; 0 and the plain text (its length in *n), or -1 */
static int dfido_open_box(const uint8_t kek[32], const uint8_t *box, int bl, uint8_t *out, int cap, int *n)
{
	if (bl < 12 + 16 || bl - 28 > cap) return -1;
	if (daes_gcm_open(kek, box, box + 12, bl - 12, out)) return -1; *n = bl - 28; return 0;
}
static int dfido_seal_box(const uint8_t kek[32], const uint8_t *pt, int n, uint8_t *out) { drandom(out, 12); daes_gcm_seal(kek, out, pt, n, out + 12); return n + 28; }
/* ---------------------------------------------------------------- the account's record */
typedef struct { char name[64]; uint8_t cid[256]; int cidl; uint8_t pub[64]; long long cnt, added, last; uint8_t ks[12 + 32 + 16]; int ksl; } dfkey;
typedef struct { int n, req; uint8_t salt[32]; uint8_t fk[12 + 64 + 16]; int fkl; uint8_t pv[12 + 512 + 16]; int pvl; uint8_t kp[12 + 32 + 16]; int kpl; dfkey k[DF_MAX]; } dfrec;
static void dfido_path(const char *id, char *p, size_t n) { snprintf(p, n, "%s/%s.json", fido_dir, id); }
static int dfido_b64f(const char *o, const char *k, uint8_t *out, int cap) { char *v = dj_str(dj_get(o, k)); int n = v && *v ? db64d(v, out, cap) : v ? 0 : -1; free(v); return n; }
/* 0: read (or none: n 0); -1: there but not readable (then nothing may be changed, and nothing opens) */
static int dfido_read(const char *id, dfrec *R)
{
	memset(R, 0, sizeof *R); char p[400]; dfido_path(id, p, sizeof p); char *t = slurp(p, NULL); if (!t) return 0;
	int bad = dfido_b64f(t, "salt", R->salt, 32) != 32; R->req = dj_truthy(dj_get(t, "req"));
	R->fkl = dfido_b64f(t, "fk", R->fk, sizeof R->fk); R->pvl = dfido_b64f(t, "pv", R->pv, sizeof R->pv); R->kpl = dfido_b64f(t, "kp", R->kp, sizeof R->kp);
	if (R->fkl < 28 || R->pvl < 0 || R->kpl < 0) bad = 1;
	const char *ks = dj_get(t, "keys");
	for (int i = 0; !bad && ks && i < DF_MAX; i++) { const char *k = dj_at(ks, i); if (!k) break; dfkey *K = &R->k[R->n];
		char *nm = dj_str(dj_get(k, "name")); snprintf(K->name, sizeof K->name, "%s", nm ? nm : "Security key"); free(nm);
		K->cidl = dfido_b64f(k, "cid", K->cid, sizeof K->cid); int pl = dfido_b64f(k, "pub", K->pub, sizeof K->pub); K->ksl = dfido_b64f(k, "ks", K->ks, sizeof K->ks);
		K->cnt = (long long)dj_num(dj_get(k, "cnt"), 0); K->added = (long long)dj_num(dj_get(k, "added"), 0); K->last = (long long)dj_num(dj_get(k, "last"), 0);
		if (K->cidl <= 0 || pl != 64 || K->ksl != 60) bad = 1; else R->n++; }
	if (!bad && !R->n) bad = 1;
	explicit_bzero(t, strlen(t)); free(t);
	if (bad) { health_add("signin", "the security keys' record of the account %.80s is not readable", id); return -1; }
	return 0;
}
static int dfido_write(const char *id, const dfrec *R)
{
	char p[400], tp[420]; dfido_path(id, p, sizeof p);
	if (!R->n) { unlink(p); return 0; }
	if (mkdir(fido_dir, 0700) && errno != EEXIST) return -1; chmod(fido_dir, 0700);
	str o = {0}; sadd(&o, "{\"v\":2,\"req\":%s,\"salt\":\"", R->req ? "true" : "false"); b64(&o, R->salt, 32);
	sadd(&o, "\",\"fk\":\""); b64(&o, R->fk, R->fkl); sadd(&o, "\",\"pv\":\""); if (R->pvl > 0) b64(&o, R->pv, R->pvl);
	sadd(&o, "\",\"kp\":\""); if (R->kpl > 0 && !R->req) b64(&o, R->kp, R->kpl); sadd(&o, "\",\"keys\":[");
	for (int i = 0; i < R->n; i++) { const dfkey *K = &R->k[i]; sadd(&o, "%s{\"name\":", i ? "," : ""); sjson(&o, K->name);
		sadd(&o, ",\"cid\":\""); b64(&o, K->cid, K->cidl); sadd(&o, "\",\"pub\":\""); b64(&o, K->pub, 64); sadd(&o, "\",\"ks\":\""); b64(&o, K->ks, K->ksl);
		sadd(&o, "\",\"cnt\":%lld,\"added\":%lld,\"last\":%lld}", K->cnt, K->added, K->last); }
	sadd(&o, "]}\n");
	snprintf(tp, sizeof tp, "%s.%ld.tmp", p, (long)getpid());
	int f = open(tp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0600), ok = f >= 0 && write(f, o.b, o.n) == (ssize_t)o.n && !fsync(f);
	if (f >= 0) close(f); explicit_bzero(o.b, o.n); free(o.b);
	if (!ok || rename(tp, p)) { unlink(tp); return -1; }
	return 0;
}
static int dfido_count(const char *id) { dfrec R; if (!home_acct_ok(id) || dfido_read(id, &R)) return 0; int n = R.n; explicit_bzero(&R, sizeof R); return n; }
static int dfx_why(int rc, char *why, size_t wn)
{
	if (rc == DFX_ERR_NO_CREDS) { snprintf(why, wn, "This security key is not one of this account's keys."); return 403; }
	if (rc == DFX_ERR_ACTION_TIMEOUT || rc == DFX_ERR_USER_TIMEOUT) { snprintf(why, wn, "The security key was not touched in time."); return 408; }
	if (rc == DFX_ERR_PIN_REQUIRED || rc == DFX_ERR_UNSUPPORTED_OPT) { snprintf(why, wn, "This security key asks for its PIN for everything (\"always verify\"): turn that off with its maker's tool to use it here."); return 409; }
	if (rc == DFX_ERR_PIN_BLOCKED || rc == DFX_ERR_PIN_AUTH_BLOCKED) { snprintf(why, wn, "The security key's PIN is blocked: reset the key with its maker's tool."); return 403; }
	snprintf(why, wn, "The security key did not answer as expected (%s).", FX.fido_strerr ? FX.fido_strerr(rc) : "?"); return 502;
}
/* a touch of any of R's keys within WAIT seconds (0: only when one is plugged in now): 0, the key's index in *which, its
   hmac-secret output and the device's path; else an HTTP status and the reason. The record's counters are updated (written
   by the caller). SKIP: a device path not to use (the one touched before, when adding) */
static void dfido_ok(const char *id) { long long n, u, l; dsec_read(id, &n, &u, &l); if (n || u) dsec_write(id, 0, 0, l); }
static int dfido_touch(dfrec *R, const char *id, int wait, int *which, uint8_t hs[32], char *path, char *why, size_t wn)
{
	if (!R->n) { snprintf(why, wn, "This account has no security keys."); return 404; }
	if (!dfx_load()) { snprintf(why, wn, "Security keys cannot be used: libfido2 is missing from this Rudolf OS."); return 503; }
	time_t end = time(NULL) + (wait > 0 ? wait : 0); int last = 0;
	for (;;) {
		char paths[8][256]; int nd = dfx_devs(paths, 8);
		for (int d = 0; d < nd; d++) {
			dfx_dev *dev = dfx_open(paths[d]); if (!dev) continue;
			if (!FX.fido_dev_is_fido2(dev)) { dfx_close(&dev); snprintf(why, wn, "This security key is an old U2F key without hmac-secret: it cannot protect files."); last = 409; continue; }
			dfx_assert *a = FX.fido_assert_new(); uint8_t cdh[32]; drandom(cdh, 32);
			FX.fido_assert_set_clientdata_hash(a, cdh, 32); FX.fido_assert_set_rp(a, DF_RP); FX.fido_assert_set_extensions(a, DFX_EXT_HMAC);
			FX.fido_assert_set_hmac_salt(a, R->salt, 32); FX.fido_assert_set_up(a, DFX_OPT_TRUE);
			for (int i = 0; i < R->n; i++) FX.fido_assert_allow_cred(a, R->k[i].cid, R->k[i].cidl);
			FX.fido_dev_set_timeout(dev, 30000);
			int rc = FX.fido_dev_get_assert(dev, a, NULL), hit = -1;
			if (!rc && FX.fido_assert_count(a) == 1) {
				const unsigned char *ip = FX.fido_assert_id_ptr(a, 0); size_t il = FX.fido_assert_id_len(a, 0);
				for (int i = 0; i < R->n; i++) if ((size_t)R->k[i].cidl == il && ip && !memcmp(R->k[i].cid, ip, il)) hit = i;
				if (hit < 0) { snprintf(why, wn, "This security key is not one of this account's keys."); rc = -100; }
				else {
					dfx_es256 *pk = FX.es256_pk_new(); uint8_t raw[65]; raw[0] = 4; memcpy(raw + 1, R->k[hit].pub, 64);
					int vr = pk ? FX.es256_pk_from_ptr(pk, raw, 65) : -1; if (!vr) vr = FX.fido_assert_verify(a, 0, DFX_COSE_ES256, pk); FX.es256_pk_free(&pk);
					uint32_t cnt = FX.fido_assert_sigcount(a, 0); uint8_t fl = FX.fido_assert_flags(a, 0);
					const unsigned char *h = FX.fido_assert_hmac_secret_ptr(a, 0); size_t hl = FX.fido_assert_hmac_secret_len(a, 0);
					if (vr) { snprintf(why, wn, "The security key's answer is not valid (its signature)."); rc = -101; health_add("signin", "a security key's answer with a wrong signature (account %.80s)", id); }
					else if (!(fl & 1)) { snprintf(why, wn, "The security key was not touched."); rc = -102; }
					else if ((cnt || R->k[hit].cnt) && (long long)cnt <= R->k[hit].cnt) { snprintf(why, wn, "The security key's answer is not new (its counter): it may have been copied."); rc = -103; health_add("signin", "a security key's counter did not grow (account %.80s): a copied key?", id); }
					else if (!h || hl != 32) { snprintf(why, wn, "The security key did not give its hmac-secret."); rc = -104; }
					else { memcpy(hs, h, 32); R->k[hit].cnt = cnt; R->k[hit].last = time(NULL); *which = hit; if (path) snprintf(path, 256, "%s", paths[d]); }
				}
				explicit_bzero(cdh, sizeof cdh);
			}
			FX.fido_assert_free(&a); dfx_close(&dev);
			if (!rc) return 0;
			last = rc > 0 ? dfx_why(rc, why, wn) : 403;
			return last;   /* (a key that answered, wrongly or not in time, is the answer: it is not asked again) */
		}
		if (!nd && !last) snprintf(why, wn, "Plug in your security key and touch it.");
		if (time(NULL) >= end) return last ? last : 401;
		usleep(250000);
	}
}
/* the account (by name), its password checked as the sign-in does: 0, the account, its id and the password's key */
/* the password's key (PBKDF2 of the password with the account's salt "ks", as the page's): 0, or -1 */
static int dfido_pk(const char *a, const char *pw, uint8_t pk[32])
{
	char *ks = dj_str(dj_get(a, "ks")); uint8_t salt[96]; int sl = ks ? db64d(ks, salt, sizeof salt) : -1; free(ks);
	if (sl <= 0) return -1; dpbkdf2_256(pw, salt, sl, DESK_ROUNDS, pk, 32); return 0;
}
/* the account (by name), its password checked as the sign-in does: 0, the account, its id and the password's key */
static int dfido_pw(dls *l, char **list, const char *n, const char *pw, const char **acct, char id[80], uint8_t pk[32], char *why, size_t wn)
{
	*list = dls_accounts(l); const char *a = *list && n ? dacct_find(*list, n) : NULL; *acct = a;
	if (!a || !pw) { snprintf(why, wn, "The user name or password is incorrect."); return 403; }
	dacct_id(a, id, 80); int wt = dsec_wait(id); if (wt) { snprintf(why, wn, "Too many wrong attempts. Please wait %d seconds and try again.", wt); return 429; }
	char *h = dj_str(dj_get(a, "h")); int st = dpw_check(h, pw); free(h);
	if (st == -2) { snprintf(why, wn, "This account's password is from an older Rudolf OS: sign in once with the Chromium desktop engine."); return 409; }
	if (st) { dsec_fail(id); snprintf(why, wn, "The user name or password is incorrect."); return 403; }
	return dfido_pk(a, pw, pk) ? (snprintf(why, wn, "This account has no key yet: sign in once first."), 409) : 0;
}
/* a box of the page's (AES-256-GCM with the password's key, {iv, ct}: fk, priv, fks): 0 and its contents */
static int dfido_page_box(const char *a, const char *k, const uint8_t pk[32], uint8_t *out, int cap, int *n)
{
	const char *box = dj_get(a, k); if (!box || *dj_ws(box) != '{') return -1;
	char *iv = dj_str(dj_get(box, "iv")), *ct = dj_str(dj_get(box, "ct")); uint8_t ivb[16], ctb[700];
	int il = iv ? db64d(iv, ivb, sizeof ivb) : -1, cl = ct ? db64d(ct, ctb, sizeof ctb) : -1; free(iv); free(ct);
	if (il != 12 || cl <= 16 || cl - 16 > cap || daes_gcm_open(pk, ivb, ctb, cl, out)) return -1; *n = cl - 16; return 0;
}
/* a new box for the page ({iv, ct}, as acEnc makes them) */
static void dfido_page_seal(str *o, const uint8_t pk[32], const uint8_t *pt, int n)
{
	uint8_t iv[12], ct[96]; drandom(iv, 12); daes_gcm_seal(pk, iv, pt, n, ct);
	sadd(o, "{\"iv\":\""); b64(o, iv, 12); sadd(o, "\",\"ct\":\""); b64(o, ct, n + 16); sadd(o, "\"}"); explicit_bzero(ct, sizeof ct);
}
static void dfido_err(struct req *r, int code, const char *need, const char *why)
{
	str e = {0}; sadd(&e, "{\"ok\":false,"); if (need) sadd(&e, "\"need\":\"%s\",", need); sadd(&e, "\"error\":"); sjson(&e, why); sadd(&e, "}");
	reply(r->fd, code, "application/json", e.b, e.n, NULL); free(e.b);
}
/* the account's key secret through a touch of one of R's keys: 0, or an HTTP status */
static int dfido_ksec(dfrec *R, const char *id, int wait, uint8_t ksec[32], char *path, char *why, size_t wn)
{
	int w = -1, n = 0; uint8_t hs[32], kk[32]; int rc = dfido_touch(R, id, wait, &w, hs, path, why, wn); if (rc) return rc;
	dfido_kkey(hs, id, kk); explicit_bzero(hs, sizeof hs);
	int bad = dfido_open_box(kk, R->k[w].ks, R->k[w].ksl, ksec, 32, &n) || n != 32; explicit_bzero(kk, sizeof kk);
	if (bad) { snprintf(why, wn, "The security key's copy of the account's key did not open."); health_add("signin", "a security key's copy did not open (account %.80s)", id); return 403; }
	dfido_write(id, R); return 0;   /* (the key's counter and last use) */
}
/* the file key (and the administrator's private key) from the password's secret and the key secret: 0, or -1 */
static int dfido_files(const dfrec *R, const uint8_t pws[32], const uint8_t ksec[32], const char *id, uint8_t fk[64], int *fl, uint8_t pv[512], int *pvl)
{
	uint8_t wk[32]; dfido_wkey(pws, ksec, id, wk); *pvl = 0;
	int bad = dfido_open_box(wk, R->fk, R->fkl, fk, 64, fl) || (R->pvl > 0 && dfido_open_box(wk, R->pv, R->pvl, pv, 512, pvl));
	explicit_bzero(wk, sizeof wk); return bad ? -1 : 0;
}
/* everything a sign-in with a key gives: 0 and fk/pv; *FKS 1 when the password's secret was made again (the page's box
   was missing or did not open while keys were not required: then from the password's copy of the file key) and PWS is
   the new secret for the page's box. An HTTP status otherwise */
static int dfido_signin_keys(dfrec *R, const char *a, const char *id, const uint8_t pk[32], uint8_t fk[64], int *fl, uint8_t pv[512], int *pvl, uint8_t pws[32], int *fks, char *why, size_t wn)
{
	uint8_t ksec[32]; char path[256] = ""; int n = 0; *fks = 0;
	int rc = dfido_ksec(R, id, 30, ksec, path, why, wn); if (rc) return rc;
	if (!dfido_page_box(a, "fks", pk, pws, 32, &n) && n == 32 && !dfido_files(R, pws, ksec, id, fk, fl, pv, pvl)) { explicit_bzero(ksec, sizeof ksec); return 0; }
	if (R->req || dfido_page_box(a, "fk", pk, fk, 64, fl)) {
		explicit_bzero(ksec, sizeof ksec); snprintf(why, wn, "Your files' key box (in your account) is missing or damaged, and the password alone cannot open your files: an administrator can reset your password with their copy (docs/security-keys.md).");
		health_add("signin", "the account %.80s's security key box (fks) is missing or damaged", id); return 409; }
	if (dfido_page_box(a, "priv", pk, pv, 512, pvl)) *pvl = 0;
	drandom(pws, 32); uint8_t wk[32]; dfido_wkey(pws, ksec, id, wk); explicit_bzero(ksec, sizeof ksec);
	R->fkl = dfido_seal_box(wk, fk, *fl, R->fk); R->pvl = *pvl > 0 ? dfido_seal_box(wk, pv, *pvl, R->pv) : 0; explicit_bzero(wk, sizeof wk);
	if (dfido_write(id, R)) { snprintf(why, wn, "The keys' record could not be kept."); return 500; }
	*fks = 1; return 0;
}
/* ---------------------------------------------------------------- adding a key */
static int dfido_has_hmac(dfx_dev *dev)
{
	dfx_cbor *ci = FX.fido_cbor_info_new(); int ok = 0;
	if (ci && !FX.fido_dev_get_cbor_info(dev, ci)) { char **e = FX.fido_cbor_info_extensions_ptr(ci); size_t n = FX.fido_cbor_info_extensions_len(ci); for (size_t i = 0; i < n; i++) if (e[i] && !strcmp(e[i], "hmac-secret")) ok = 1; }
	FX.fido_cbor_info_free(&ci); return ok;
}
static int dfido_make(dfrec *R, const char *id, const char *uname, const char *pin, const char *skip, int wait, dfkey *K, uint8_t hs[32], char *why, size_t wn)
{
	time_t end = time(NULL) + wait; int last = 0;
	for (;;) {
		char paths[8][256]; int nd = dfx_devs(paths, 8);
		for (int d = 0; d < nd; d++) {
			if (skip && *skip && !strcmp(paths[d], skip)) continue;
			dfx_dev *dev = dfx_open(paths[d]); if (!dev) continue;
			if (!FX.fido_dev_is_fido2(dev) || !dfido_has_hmac(dev)) { dfx_close(&dev); snprintf(why, wn, "This security key cannot protect files: it has no hmac-secret (an old U2F-only key?)."); return 409; }
			if (FX.fido_dev_has_pin(dev) && (!pin || !*pin)) { dfx_close(&dev); snprintf(why, wn, "This security key has a PIN: type it to add the key."); return 401; }
			dfx_cred *c = FX.fido_cred_new(); uint8_t cdh[32]; drandom(cdh, 32);
			FX.fido_cred_set_type(c, DFX_COSE_ES256); FX.fido_cred_set_clientdata_hash(c, cdh, 32); FX.fido_cred_set_rp(c, DF_RP, "Rudolf OS");
			FX.fido_cred_set_user(c, (const unsigned char *)id, strlen(id), uname, uname, NULL); FX.fido_cred_set_extensions(c, DFX_EXT_HMAC); FX.fido_cred_set_rk(c, DFX_OPT_FALSE);
			for (int i = 0; i < R->n; i++) FX.fido_cred_exclude(c, R->k[i].cid, R->k[i].cidl);
			FX.fido_dev_set_timeout(dev, 30000);
			int rc = FX.fido_dev_make_cred(dev, c, pin && *pin ? pin : NULL);
			if (!rc) {
				const unsigned char *ip = FX.fido_cred_id_ptr(c), *pp = FX.fido_cred_pubkey_ptr(c); size_t il = FX.fido_cred_id_len(c), pl = FX.fido_cred_pubkey_len(c);
				if (FX.fido_cred_type(c) != DFX_COSE_ES256 || !ip || il > sizeof K->cid || !pp || pl != 64) { snprintf(why, wn, "The security key made a credential this cannot use."); rc = -1; }
				else { memcpy(K->cid, ip, il); K->cidl = (int)il; memcpy(K->pub, pp, 64); }
			}
			FX.fido_cred_free(&c);
			if (rc == DFX_ERR_EXCLUDED && d + 1 < nd) { dfx_close(&dev); snprintf(why, wn, "This security key is one of this account's already: plug in another one."); last = 409; continue; }
			if (rc) { dfx_close(&dev); if (rc == DFX_ERR_EXCLUDED) { snprintf(why, wn, "This security key is one of this account's already: plug in another one."); return 409; }
				if (rc == DFX_ERR_PIN_INVALID) { snprintf(why, wn, "Wrong PIN for the security key."); return 401; }
				if (rc == DFX_ERR_UNSUPPORTED_EXT) { snprintf(why, wn, "This security key cannot protect files: it has no hmac-secret."); return 409; }
				return rc > 0 ? dfx_why(rc, why, wn) : 502; }
			/* the second touch: its hmac-secret for the account's salt */
			dfrec one; memset(&one, 0, sizeof one); memcpy(one.salt, R->salt, 32); one.n = 1; one.k[0] = *K;
			dfx_assert *a = FX.fido_assert_new(); drandom(cdh, 32);
			FX.fido_assert_set_clientdata_hash(a, cdh, 32); FX.fido_assert_set_rp(a, DF_RP); FX.fido_assert_set_extensions(a, DFX_EXT_HMAC);
			FX.fido_assert_set_hmac_salt(a, R->salt, 32); FX.fido_assert_set_up(a, DFX_OPT_TRUE); FX.fido_assert_allow_cred(a, K->cid, K->cidl);
			rc = FX.fido_dev_get_assert(dev, a, NULL);
			if (!rc) { dfx_es256 *pk = FX.es256_pk_new(); uint8_t raw[65]; raw[0] = 4; memcpy(raw + 1, K->pub, 64); int vr = FX.es256_pk_from_ptr(pk, raw, 65); if (!vr) vr = FX.fido_assert_verify(a, 0, DFX_COSE_ES256, pk); FX.es256_pk_free(&pk);
				const unsigned char *h = FX.fido_assert_hmac_secret_ptr(a, 0);
				if (vr || FX.fido_assert_hmac_secret_len(a, 0) != 32 || !h) { snprintf(why, wn, "The security key did not give a usable hmac-secret."); rc = -1; }
				else { memcpy(hs, h, 32); K->cnt = FX.fido_assert_sigcount(a, 0); } }
			FX.fido_assert_free(&a); dfx_close(&dev); explicit_bzero(&one, sizeof one);
			if (rc) return rc > 0 ? dfx_why(rc, why, wn) : 502;
			K->added = K->last = time(NULL); return 0;
		}
		if (last == 409) return 409;
		if (!last) snprintf(why, wn, skip && *skip ? "Now plug in the new security key and touch it." : "Plug in the security key and touch it.");
		if (time(NULL) >= end) return 408;
		usleep(250000);
	}
}
/* ---------------------------------------------------------------- the requests */
static void api_keys(struct req *r, const char *w, int post)
{
	char why[400] = "";
	if (!*w) {
		if (post) { reply_err(r->fd, 405, "GET"); return; }
		const char *acct = qparam(r->query, "acct"); if (!home_acct_ok(acct) || !strcmp(acct, "guest")) { reply_err(r->fd, 400, "acct?"); return; }
		dfrec R; int bad = dfido_read(acct, &R); int lib = dfx_load(); char paths[8][256]; int nd = lib ? dfx_devs(paths, 8) : 0;
		str o = {0}; sadd(&o, "{\"ok\":true,\"bad\":%s,\"req\":%s,\"lib\":%s,\"devs\":%d,\"keys\":[", bad ? "true" : "false", R.req ? "true" : "false", lib ? "true" : "false", nd);
		for (int i = 0; i < R.n; i++) { sadd(&o, "%s{\"i\":%d,\"name\":", i ? "," : "", i); sjson(&o, R.k[i].name); sadd(&o, ",\"added\":%lld,\"last\":%lld}", R.k[i].added, R.k[i].last); }
		sadd(&o, "]}"); explicit_bzero(&R, sizeof R); reply_json(r->fd, &o); return;
	}
	if (!post) { reply_err(r->fd, 405, "POST"); return; }
	if (*w != '/') { reply_err(r->fd, 404, "unknown"); return; } w++;
	char *n = dbody_field(r, "n"), *pw = dbody_field(r, "p"), id[80] = ""; uint8_t pk[32], fk[64], pv[512], pws[32], ksec[32]; int fl = 0, pvl = 0, sl = 0;
	int k = r->body && dj_true(dj_get(r->body, "k"));
	dls l; dls_open(&l); char *list = NULL; const char *a = NULL; dfrec R; memset(&R, 0, sizeof R);
	int rc = dfido_pw(&l, &list, n, pw, &a, id, pk, why, sizeof why);
#define DF_DONE(code, need) do { dfido_err(r, code, need, why); goto out; } while (0)
#define DF_TOUCH() if (!k) { snprintf(why, sizeof why, "Touch your security key."); DF_DONE(401, "key"); }
	if (rc) { if (rc == 403 && a) health_add("signin", "wrong password for the account %.100s (security keys)", n); DF_DONE(rc, NULL); }
	if (dfido_read(id, &R)) { snprintf(why, sizeof why, "This account's security keys' record is damaged: see System health."); DF_DONE(500, NULL); }
	if (!strcmp(w, "signin")) {
		if (!R.n) { snprintf(why, sizeof why, "This account has no security keys."); DF_DONE(404, NULL); }
		/* (first without "k": the password was right, so the screen says to touch the key; then "k" waits for the touch) */
		if (!k) { dfido_ok(id); snprintf(why, sizeof why, "Touch your security key."); DF_DONE(401, "key"); }
		int fks = 0; rc = dfido_signin_keys(&R, a, id, pk, fk, &fl, pv, &pvl, pws, &fks, why, sizeof why);
		if (rc == 401) DF_DONE(401, "key");
		if (rc) { if (rc == 403) { dsec_fail(id); health_add("signin", "a security key refused for the account %.100s", n); } DF_DONE(rc, "key"); }
		char h1[65], h2[129]; dhex(pk, 32, h1); dhex(fk, fl, h2);
		str o = {0}; sadd(&o, "{\"ok\":true,\"req\":%s,\"pk\":\"%s\",\"fk\":\"%s\"", R.req ? "true" : "false", h1, h2);
		if (pvl > 0) { sadd(&o, ",\"pv\":\""); b64(&o, pv, pvl); sadd(&o, "\""); }
		if (fks) { sadd(&o, ",\"fks\":"); dfido_page_seal(&o, pk, pws, 32); }
		sadd(&o, "}"); explicit_bzero(h1, sizeof h1); explicit_bzero(h2, sizeof h2); dfido_ok(id);
		reply(r->fd, 200, "application/json", o.b, o.n, NULL); explicit_bzero(o.b, o.n); free(o.b); goto out;
	}
	if (!strcmp(w, "add")) {
		if (R.n >= DF_MAX) { snprintf(why, sizeof why, "An account can have %d security keys: remove one first.", DF_MAX); DF_DONE(409, NULL); }
		if (!dfx_load()) { snprintf(why, sizeof why, "Security keys cannot be used: libfido2 is missing from this Rudolf OS."); DF_DONE(503, NULL); }
		char skip[256] = "", *nm = dbody_field(r, "name"), *pin = dbody_field(r, "pin"); dfkey K; memset(&K, 0, sizeof K); uint8_t hs[32], kk[32]; int fks = 0;
		snprintf(K.name, sizeof K.name, "%s", nm && *nm ? nm : "Security key"); free(nm);
		for (char *c = K.name; *c; c++) if ((unsigned char)*c < 0x20) *c = ' ';
		if (!R.n) {   /* the first key: the account's key secret and the password's secret made, the file key from the password's copy */
			if (dfido_page_box(a, "fk", pk, fk, 64, &fl)) { dfree_secret(pin); snprintf(why, sizeof why, "This account's file key could not be opened with its password: sign out and in again first."); DF_DONE(409, NULL); }
			if (dfido_page_box(a, "priv", pk, pv, 512, &pvl)) pvl = 0;
			drandom(R.salt, 32); drandom(ksec, 32); drandom(pws, 32); fks = 1;
			uint8_t wk[32]; dfido_wkey(pws, ksec, id, wk); R.fkl = dfido_seal_box(wk, fk, fl, R.fk); R.pvl = pvl > 0 ? dfido_seal_box(wk, pv, pvl, R.pv) : 0; explicit_bzero(wk, sizeof wk);
		} else if (!R.req && R.kpl > 0 && !dfido_open_box(pk, R.kp, R.kpl, ksec, 32, &sl) && sl == 32) {}   /* (not required yet: the password's copy of the key secret) */
		else { if (!k) { dfree_secret(pin); snprintf(why, sizeof why, "Touch one of your security keys first."); DF_DONE(401, "key"); }
			rc = dfido_ksec(&R, id, 30, ksec, skip, why, sizeof why); if (rc) { dfree_secret(pin); DF_DONE(rc, rc == 401 || rc == 408 ? "key" : NULL); } }
		char uname[256]; { char *u = dj_str(dj_get(a, "n")); snprintf(uname, sizeof uname, "%s", u ? u : id); free(u); }
		rc = dfido_make(&R, id, uname, pin, skip, 45, &K, hs, why, sizeof why); dfree_secret(pin);
		if (rc) DF_DONE(rc, rc == 401 && strstr(why, "PIN") ? "pin" : rc == 408 ? "key" : NULL);
		dfido_kkey(hs, id, kk); explicit_bzero(hs, sizeof hs); K.ksl = dfido_seal_box(kk, ksec, 32, K.ks); explicit_bzero(kk, sizeof kk);
		if (!R.req) R.kpl = dfido_seal_box(pk, ksec, 32, R.kp);
		R.k[R.n++] = K; explicit_bzero(&K, sizeof K);
		if (dfido_write(id, &R)) { snprintf(why, sizeof why, "The key could not be kept (the disk?)."); DF_DONE(500, NULL); }
		health_add("signin", "a security key was added to the account %.100s (%d now)", n, R.n);
		str o = {0}; sadd(&o, "{\"ok\":true,\"n\":%d,\"req\":%s", R.n, R.req ? "true" : "false"); if (fks) { sadd(&o, ",\"fks\":"); dfido_page_seal(&o, pk, pws, 32); }
		sadd(&o, "}"); reply(r->fd, 200, "application/json", o.b, o.n, NULL); free(o.b); goto out;
	}
	if (!strcmp(w, "remove")) {
		const char *iq = r->body ? dj_get(r->body, "i") : NULL; int i = iq ? (int)dj_num(iq, -1) : -1;
		if (i < 0 || i >= R.n) { snprintf(why, sizeof why, "No such key."); DF_DONE(404, NULL); }
		if (R.req && R.n <= 2) { snprintf(why, sizeof why, "Two keys are needed while a security key is required: add another one first, or stop requiring keys."); DF_DONE(409, NULL); }
		if (R.n == 1 && dfido_page_box(a, "fk", pk, fk, 64, &fl)) { snprintf(why, sizeof why, "The password's copy of your file key is missing: stop requiring keys first."); DF_DONE(409, NULL); }
		for (int j = i; j < R.n - 1; j++) R.k[j] = R.k[j + 1]; R.n--; explicit_bzero(&R.k[R.n], sizeof R.k[R.n]); if (!R.n) R.req = 0;
		if (dfido_write(id, &R)) { snprintf(why, sizeof why, "The change could not be kept."); DF_DONE(500, NULL); }
		health_add("signin", "a security key was removed from the account %.100s (%d left)", n, R.n);
		str o = {0}; sadd(&o, "{\"ok\":true,\"n\":%d,\"req\":%s}", R.n, R.req ? "true" : "false"); reply_json(r->fd, &o); goto out;
	}
	if (!strcmp(w, "require")) {
		int on = r->body && dj_true(dj_get(r->body, "on"));
		if (!R.n) { snprintf(why, sizeof why, "This account has no security keys."); DF_DONE(404, NULL); }
		if (on && R.n < 2) { snprintf(why, sizeof why, "Add a second security key first: a spare, in case one is lost."); DF_DONE(409, NULL); }
		DF_TOUCH();
		/* the whole way checked before anything changes: a key, the page's box, the file key */
		char path[256] = ""; rc = dfido_ksec(&R, id, 30, ksec, path, why, sizeof why); if (rc) DF_DONE(rc, rc == 401 || rc == 408 ? "key" : NULL);
		if (dfido_page_box(a, "fks", pk, pws, 32, &sl) || sl != 32 || dfido_files(&R, pws, ksec, id, fk, &fl, pv, &pvl)) {
			snprintf(why, sizeof why, "Your account's key box is not saved yet: sign out and in again (with your key), then try again."); DF_DONE(409, NULL); }
		R.req = on; if (on) { explicit_bzero(R.kp, sizeof R.kp); R.kpl = 0; } else R.kpl = dfido_seal_box(pk, ksec, 32, R.kp);
		if (dfido_write(id, &R)) { snprintf(why, sizeof why, "The change could not be kept."); DF_DONE(500, NULL); }
		health_add("signin", "the account %.100s %s a security key", n, on ? "now requires" : "no longer requires");
		str o = {0}; sadd(&o, "{\"ok\":true,\"req\":%s}", on ? "true" : "false"); reply_json(r->fd, &o); goto out;
	}
	if (!strcmp(w, "reset")) {   /* (an administrator: N and P are the administrator's) */
		char *role = dj_str(dj_get(a, "r")), *tg = dbody_field(r, "acct"); int adm = role && !strcmp(role, "admin"); free(role);
		if (!adm) { free(tg); snprintf(why, sizeof why, "Only an administrator can do this."); DF_DONE(403, NULL); }
		if (!home_acct_ok(tg) || !strcmp(tg, id)) { free(tg); snprintf(why, sizeof why, "acct?"); DF_DONE(400, NULL); }
		if (R.n) { if (!k) { free(tg); snprintf(why, sizeof why, "Touch your security key."); DF_DONE(401, "key"); }
			char path[256] = ""; rc = dfido_ksec(&R, id, 30, ksec, path, why, sizeof why); if (rc) { free(tg); DF_DONE(rc, rc == 401 || rc == 408 ? "key" : NULL); } }
		dfrec T; int had = dfido_read(tg, &T) ? -1 : T.n; explicit_bzero(&T, sizeof T); dfido_write(tg, &T);
		health_add("signin", "administrator %.100s removed the security keys of the account %.80s (%d)", n, tg, had); free(tg);
		reply_ok(r->fd, "removed"); goto out;
	}
	snprintf(why, sizeof why, "unknown"); DF_DONE(404, NULL);
#undef DF_DONE
#undef DF_TOUCH
out:
	explicit_bzero(pk, sizeof pk); explicit_bzero(fk, sizeof fk); explicit_bzero(pv, sizeof pv); explicit_bzero(pws, sizeof pws); explicit_bzero(ksec, sizeof ksec); explicit_bzero(&R, sizeof R);
	free(n); dfree_secret(pw); free(list); dls_close(&l);
}
/* ---------------------------------------------------------------- Native's sign-in and lock screen (desktop/login, desktop/unlock)
   0: the account has no keys (as before); 1: a key was touched (the file key from the keys' copy, or while keys are not
   required and the page's box is missing, the password's: in FK and *FL when FK is given); -1: answered already (401
   need "key" first, then the reason) */
static int dfido_native(struct req *r, const char *a, const char *id, const uint8_t pk[32], uint8_t *fk, int *fl)
{
	dfrec R; char why[400] = ""; if (!home_acct_ok(id)) return 0;
	if (dfido_read(id, &R)) { dfido_err(r, 500, NULL, "This account's security keys' record is damaged: see System health."); return -1; }
	if (!R.n) return 0;
	int k = r->body && dj_true(dj_get(r->body, "k")); uint8_t f2[64], pv[512], pws[32], ksec[32]; int f2l = 0, pvl = 0, sl = 0, rc; char path[256] = "";
	if (!k) { explicit_bzero(&R, sizeof R); dfido_err(r, 401, "key", "Touch your security key."); return -1; }
	rc = dfido_ksec(&R, id, 30, ksec, path, why, sizeof why);
	if (!rc && !(!dfido_page_box(a, "fks", pk, pws, 32, &sl) && sl == 32 && !dfido_files(&R, pws, ksec, id, f2, &f2l, pv, &pvl))) {
		/* (the page's box missing: only while keys are not required, the password's copy; the page makes the box again at its next sign-in) */
		if (R.req || dfido_page_box(a, "fk", pk, f2, 64, &f2l)) { rc = 409; snprintf(why, sizeof why, "Your files' key box is missing or damaged: sign in with the Chromium desktop engine, or ask an administrator."); } }
	explicit_bzero(pv, sizeof pv); explicit_bzero(pws, sizeof pws); explicit_bzero(ksec, sizeof ksec); explicit_bzero(&R, sizeof R);
	if (rc) { if (rc == 403) { dsec_fail(id); health_add("signin", "a security key refused at the native sign-in (account %.80s)", id); }
		dfido_err(r, rc, "key", why); return -1; }
	if (fk) { memcpy(fk, f2, f2l); *fl = f2l; }
	explicit_bzero(f2, sizeof f2); return 1;
}
/* the lock screen (desktop/unlock): A's password was right; 0 no keys, 1 a key touched, -1 answered */
static int dfido_unlock(struct req *r, const char *a, const char *id, const char *pw)
{
	if (!dfido_count(id)) return 0;
	uint8_t pk[32]; if (dfido_pk(a, pw, pk)) { dfido_err(r, 409, NULL, "This account has no key yet."); return -1; }
	int k = dfido_native(r, a, id, pk, NULL, NULL); explicit_bzero(pk, sizeof pk); return k;
}
