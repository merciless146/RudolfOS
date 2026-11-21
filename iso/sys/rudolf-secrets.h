/* rudolf-secrets.h - the secret detector shared by every guard of Blitzen (1.6.1, docs/plan-1.6.1.md Part 26).
 * GPL-2.0-or-later. One source, included by rudolfd (its ai/... guards), rudolf-know (what it indexes) and rudolf-dev
 * (what it reads and sends), and by the tests (iso/qt/host/secrets161_t.c). No library: it builds wherever they do.
 *
 * The owner, 2026-11-21: "Add whatever measures would be necessary to harden Blitzen from being socially engineered to
 * display or send sensitive information without proper authorization (in almost every case private keys or private
 * repository keys would NEVER be allowed to be disclosed)." Every rule here runs outside the model: a model that is
 * told, tricked or broken into writing a key still has its words pass through this before anyone sees them.
 *
 * What is found (sd_scan), each a span of the text and a kind:
 *   SD_PRIVKEY   private keys: PEM blocks ("-----BEGIN ... PRIVATE KEY-----", OpenSSH, PGP's PRIVATE KEY BLOCK, ENCRYPTED
 *                PRIVATE KEY), a key's body without its header (base64 that decodes to a private key's DER, or to
 *                openssh-key-v1), PuTTY key files, age secret keys, WireGuard's PrivateKey =, and a private key's value
 *                given in a private_key= / secret_key= context
 *   SD_TOKEN     GitHub (github_pat_, ghp_, gho_, ghu_, ghs_, ghr_), GitLab, Slack (xox*), AWS (AKIA/ASIA and the secret
 *                after one), JWTs, "Authorization: Bearer|token|Basic ...", Stripe, Google, OpenAI/Anthropic-style sk-,
 *                npm, PyPI, Hugging Face, Telegram bots, and high-entropy values after token= / secret= / api_key= / key=
 *   SD_OWN       one of this computer's own secrets, matched by its value (sd_known_add): the keyring's entries, the
 *                developer and devlink tokens, Wi-Fi passwords, rudolfd's token; the value is kept only as two keyed
 *                fingerprints, never as text, and never given to the model
 *   SD_RECOVERY  recovery keys (BitLocker's eight groups of six digits)
 *   SD_PASSWORD  a password given in a password= / passwd: / psk= / passphrase context, or in an address (user:pass@)
 *   SD_CODE      two-step codes in their context ("verification code 123456"), otpauth:// addresses, recovery codes
 *   SD_PERSONAL  e-mail addresses (only for what is sent: sd_scan's SD_ALL; the chat itself shows them)
 * The same rules are run again on the text with its spacing taken out (a key spelled out with spaces, dashes or one
 * piece a line), on what base64 and hex in it decode to (an encoded key), and, for this computer's own secrets, on all
 * three. Public keys are not secrets: ssh-ed25519/ssh-rsa lines, "BEGIN PUBLIC KEY", age1 recipients, WireGuard's
 * PublicKey, a pub/public context, and Rudolf OS's release and Library keys (SD_PUBLIC) are never hidden; neither are
 * hashes shown as hashes (sha256: ..., a commit) nor ordinary words and code.
 *
 * SD_SECRET (private keys, tokens, this computer's secrets, recovery keys) is never shown and never sent: no setting,
 * no Developer mode and no administrator turns that off (there is no switch to turn). SD_SENSITIVE (passwords, codes,
 * personal details) is hidden in what Blitzen shows, and sent only after the person allows it in the desktop's own prompt.
 *
 * sd_redact: the text with each span replaced by "[hidden: a private key]" and so on.
 * sd_stream_*: the same for a text that arrives in pieces (a model's answer): it holds back a short window (and a key
 * still being written, a run of key-like characters, a spelled-out run, a base64 block) so that a key split across the
 * pieces is still caught before any of it is given out.
 * sd_json_redact: every string of a JSON text redacted (a chat request, a reply).
 * sd_relay: llama-server's streamed HTTP answer read, its text redacted as a stream, written back as the same events
 * (OpenAI's chat.completion.chunk), with a last event {"rudolf":{"text":ALL,"hidden":[...]}}: the whole answer redacted
 * once more as one piece, which the page shows, keeps, copies and speaks in place of what it was streamed.
 * sd_keypath: a path that is a key file (*.pem, *.key, id_*, *.p12, *.pfx, *.kdbx, keys/, .ssh/, .gnupg/, the keyring's
 * files, /persist's secrets): refused to Blitzen whole.
 */
#ifndef RUDOLF_SECRETS_H
#define RUDOLF_SECRETS_H
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#define SD_FN static __attribute__((unused))
enum { SD_PRIVKEY = 1, SD_TOKEN = 2, SD_OWN = 4, SD_RECOVERY = 8, SD_PASSWORD = 16, SD_CODE = 32, SD_PERSONAL = 64 };
#define SD_SECRET (SD_PRIVKEY | SD_TOKEN | SD_OWN | SD_RECOVERY)
#define SD_SENSITIVE (SD_PASSWORD | SD_CODE | SD_PERSONAL)
#define SD_SHOWN (SD_SECRET | SD_PASSWORD | SD_CODE)   /* what is hidden from what Blitzen reads and writes */
#define SD_ALL (SD_SECRET | SD_SENSITIVE)               /* what is looked for in what is sent */
/* Rudolf OS's own public keys (CLAUDE.md): the release key and the Library's. Shown as they are. */
static const char *const SD_PUBLIC[] = { "UqTqXIW8vtITjwoMix+MkFVZ0GAee9+kOS1WBIs/3mo=", "kRBDN3Ys6i49gblu504quUt7NZHGZj/kexNTb6uoenY=", NULL };

SD_FN const char *sd_label(int k)
{
	return k & SD_PRIVKEY ? "a private key" : k & SD_TOKEN ? "a token" : k & SD_OWN ? "a secret of this computer" : k & SD_RECOVERY ? "a recovery key"
		: k & SD_PASSWORD ? "a password" : k & SD_CODE ? "a sign-in code" : k & SD_PERSONAL ? "personal details" : "a secret";
}
SD_FN const char *sd_name(int k)   /* (one word, for logs and JSON) */
{
	return k & SD_PRIVKEY ? "private-key" : k & SD_TOKEN ? "token" : k & SD_OWN ? "own-secret" : k & SD_RECOVERY ? "recovery-key"
		: k & SD_PASSWORD ? "password" : k & SD_CODE ? "code" : k & SD_PERSONAL ? "personal" : "secret";
}

/* ---------------- a growing buffer */
typedef struct { char *b; size_t n, cap; } sd_buf;
SD_FN void sd_add(sd_buf *o, const void *p, size_t n)
{
	if (o->n + n + 1 > o->cap) { size_t c = (o->n + n + 1) * 2 + 64; char *nb = realloc(o->b, c); if (!nb) abort(); o->b = nb; o->cap = c; }
	if (n) memcpy(o->b + o->n, p, n); o->n += n; o->b[o->n] = 0;
}
SD_FN void sd_adds(sd_buf *o, const char *s) { sd_add(o, s, strlen(s)); }
SD_FN void sd_wipe(sd_buf *o) { if (o->b) { memset(o->b, 0, o->cap); free(o->b); } o->b = NULL; o->n = o->cap = 0; }

/* ---------------- spans found */
typedef struct { size_t a, b; int k; } sd_hit;
typedef struct { sd_hit *h; size_t n, cap; } sd_hits;
SD_FN void sd_hit_add(sd_hits *H, size_t a, size_t b, int k)
{
	if (b <= a) return;
	if (H->n == H->cap) { H->cap = H->cap ? H->cap * 2 : 16; H->h = realloc(H->h, H->cap * sizeof *H->h); if (!H->h) abort(); }
	H->h[H->n++] = (sd_hit){ a, b, k };
}
static int sd_hcmp(const void *x, const void *y) { const sd_hit *a = x, *b = y; return a->a < b->a ? -1 : a->a > b->a ? 1 : 0; }
/* sorted, overlapping spans merged (their kinds or-ed) */
SD_FN void sd_hits_merge(sd_hits *H)
{
	if (H->n < 2) return; qsort(H->h, H->n, sizeof *H->h, sd_hcmp); size_t w = 0;
	for (size_t i = 1; i < H->n; i++) { if (H->h[i].a <= H->h[w].b) { if (H->h[i].b > H->h[w].b) H->h[w].b = H->h[i].b; H->h[w].k |= H->h[i].k; } else H->h[++w] = H->h[i]; }
	H->n = w + 1;
}

/* ---------------- small helpers */
static int sd_b64c(unsigned char c) { return isalnum(c) || c == '+' || c == '/' || c == '=' || c == '-' || c == '_'; }
static int sd_tokc(unsigned char c) { return isalnum(c) || c == '_' || c == '-'; }
static int sd_hexc(unsigned char c) { return isxdigit(c); }
SD_FN int sd_ncmp(const char *s, size_t n, size_t i, const char *w)   /* (case-insensitive prefix at i) */
{
	size_t l = strlen(w); if (i + l > n) return 0;
	for (size_t k = 0; k < l; k++) if (tolower((unsigned char)s[i + k]) != tolower((unsigned char)w[k])) return 0;
	return 1;
}
/* W (as it is written: the armour of a key is in capitals) at a word's start, from FROM */
SD_FN const char *sd_find_w(const char *s, size_t n, size_t from, const char *w, size_t *at)
{
	size_t l = strlen(w); for (size_t i = from; i + l <= n; i++) if (!memcmp(s + i, w, l) && (!i || !isalpha((unsigned char)s[i - 1]))) { *at = i; return s + i; } return NULL;
}
SD_FN const char *sd_find_ci(const char *s, size_t n, size_t from, const char *w, size_t *at)
{
	size_t l = strlen(w); for (size_t i = from; i + l <= n; i++) if (sd_ncmp(s, n, i, w)) { *at = i; return s + i; } return NULL;
}
static size_t sd_run(const char *s, size_t n, size_t i, int (*f)(unsigned char)) { while (i < n && f((unsigned char)s[i])) i++; return i; }
static int isalnum_u(unsigned char c) { return isalnum(c) != 0; }
static int sd_b64n(unsigned char c) { return c != '=' && sd_b64c(c); }
/* a base64 run: its characters, then its padding (an "=" inside it ends it: key=VALUE is two) */
static size_t sd_b64r(const char *s, size_t n, size_t i) { i = sd_run(s, n, i, sd_b64n); while (i < n && s[i] == '=') i++; return i; }
/* log2 without libm (rudolfd links none): the exponent, and ln of the mantissa by its series */
static double sd_log2(double x)
{
	int e = 0; while (x >= 2) { x /= 2; e++; } while (x < 1) { x *= 2; e--; }
	double y = (x - 1) / (x + 1), y2 = y * y, t = y, s = 0; for (int k = 1; k < 30; k += 2) { s += t / k; t *= y2; }
	return e + 2 * s / 0.69314718055994530942;
}
/* character classes and Shannon entropy (bits per character) of s[a,b) */
SD_FN double sd_entropy(const char *s, size_t a, size_t b)
{
	int c[256] = {0}; size_t n = b - a; if (!n) return 0; for (size_t i = a; i < b; i++) c[(unsigned char)s[i]]++;
	double e = 0; for (int i = 0; i < 256; i++) if (c[i]) { double p = (double)c[i] / n; e -= p * sd_log2(p); } return e;
}
SD_FN int sd_classes(const char *s, size_t a, size_t b)
{
	int lo = 0, up = 0, di = 0, ot = 0; for (size_t i = a; i < b; i++) { unsigned char c = s[i]; if (islower(c)) lo = 1; else if (isupper(c)) up = 1; else if (isdigit(c)) di = 1; else ot = 1; }
	return lo + up + di + ot;
}
/* a value that looks like a secret's: long enough, of several kinds of characters, not all one or two letters */
SD_FN int sd_keyish(const char *s, size_t a, size_t b, size_t min)
{
	size_t n = b - a; if (n < min) return 0;
	int hex = 1; for (size_t i = a; i < b; i++) if (!sd_hexc((unsigned char)s[i])) { hex = 0; break; }
	if (hex) return n >= 32 && sd_classes(s, a, b) >= 2;
	return sd_classes(s, a, b) >= 3 && sd_entropy(s, a, b) >= 3.2;
}
SD_FN int sd_is_public(const char *s, size_t a, size_t b)
{
	for (int i = 0; SD_PUBLIC[i]; i++) { size_t l = strlen(SD_PUBLIC[i]); if (b - a >= l - 1 && b - a <= l) { if (!memcmp(s + a, SD_PUBLIC[i], b - a)) return 1; } }
	return 0;
}
/* base64 (standard or URL) to bytes; -1 when it is not */
SD_FN long sd_b64dec(const char *s, size_t n, unsigned char *o, size_t on)
{
	unsigned v = 0; int bits = 0; size_t k = 0;
	for (size_t i = 0; i < n; i++) { unsigned char c = s[i]; int d;
		if (c >= 'A' && c <= 'Z') d = c - 'A'; else if (c >= 'a' && c <= 'z') d = c - 'a' + 26; else if (c >= '0' && c <= '9') d = c - '0' + 52;
		else if (c == '+' || c == '-') d = 62; else if (c == '/' || c == '_') d = 63; else if (c == '=') break; else if (isspace(c)) continue; else return -1;
		v = v << 6 | d; bits += 6; if (bits >= 8) { bits -= 8; if (k < on) o[k++] = (v >> bits) & 255; } }
	return (long)k;
}
/* the decoded bytes are a private key: DER SEQUENCE { INTEGER 0|1, ... } (PKCS#1, PKCS#8, SEC1: public keys start with a
 * SEQUENCE or a large INTEGER instead), or OpenSSH's own format */
SD_FN int sd_der_private(const unsigned char *d, size_t n)
{
	if (n >= 15 && !memcmp(d, "openssh-key-v1", 14)) return 1;
	if (n < 16 || d[0] != 0x30) return 0; size_t i = 1;
	if (d[i] & 0x80) { int l = d[i] & 0x7f; if (l < 1 || l > 3) return 0; i += 1 + l; } else i++;
	if (i + 3 > n) return 0;
	return d[i] == 0x02 && d[i + 1] == 0x01 && (d[i + 2] == 0x00 || d[i + 2] == 0x01) && i + 3 < n && (d[i + 3] == 0x02 || d[i + 3] == 0x30 || d[i + 3] == 0x04);
}
SD_FN int sd_printable(const unsigned char *d, size_t n)
{
	if (n < 8) return 0; size_t p = 0; for (size_t i = 0; i < n; i++) if ((d[i] >= 0x20 && d[i] < 0x7f) || d[i] == '\n' || d[i] == '\r' || d[i] == '\t' || d[i] >= 0xc2) p++;
	return p * 100 >= n * 90;
}

/* ---------------- this computer's own secrets, by value: two keyed polynomial fingerprints (mod 2^61-1) a length */
#define SD_M61 ((1ULL << 61) - 1)
static uint64_t sd_mulm(uint64_t a, uint64_t b) { __uint128_t p = (__uint128_t)a * b; uint64_t lo = (uint64_t)(p & SD_M61), hi = (uint64_t)(p >> 61); uint64_t r = lo + hi; if (r >= SD_M61) r -= SD_M61; return r; }
typedef struct { size_t len; uint64_t h1, h2; } sd_fp;
static struct { uint64_t b1, b2; sd_fp *f; size_t n, cap; int ready; } SD_K;
SD_FN void sd_known_init(uint64_t b1, uint64_t b2)
{
	if (!b1 || !b2) { int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC); uint64_t r[2] = { 0x9e3779b97f4a7c15ULL, 0xc2b2ae3d27d4eb4fULL };
		if (fd >= 0) { if (read(fd, r, sizeof r) != (ssize_t)sizeof r) {} close(fd); } b1 = r[0]; b2 = r[1]; }
	SD_K.b1 = (b1 % (SD_M61 - 1000)) + 257; SD_K.b2 = (b2 % (SD_M61 - 1000)) + 263; SD_K.ready = 1;
}
SD_FN void sd_fingerprint(const char *v, size_t n, uint64_t *h1, uint64_t *h2)
{
	uint64_t a = 0, b = 0; for (size_t i = 0; i < n; i++) { a = sd_mulm(a, SD_K.b1) + (unsigned char)v[i] + 1; if (a >= SD_M61) a -= SD_M61; b = sd_mulm(b, SD_K.b2) + (unsigned char)v[i] + 1; if (b >= SD_M61) b -= SD_M61; }
	*h1 = a; *h2 = b;
}
/* a fingerprint made elsewhere (rudolf-portal --keyring-fingerprints, with the same bases) */
SD_FN void sd_known_add_fp(size_t len, uint64_t h1, uint64_t h2)
{
	if (len < 8 || len > 512) return; for (size_t i = 0; i < SD_K.n; i++) if (SD_K.f[i].len == len && SD_K.f[i].h1 == h1 && SD_K.f[i].h2 == h2) return;
	if (SD_K.n == SD_K.cap) { SD_K.cap = SD_K.cap ? SD_K.cap * 2 : 16; SD_K.f = realloc(SD_K.f, SD_K.cap * sizeof *SD_K.f); if (!SD_K.f) abort(); }
	SD_K.f[SD_K.n++] = (sd_fp){ len, h1, h2 };
}
/* a secret this program holds: only its fingerprint is kept (secrets shorter than 8 characters would match words) */
SD_FN void sd_known_add(const char *v, size_t n)
{
	if (!SD_K.ready) sd_known_init(0, 0);
	while (n && (v[n - 1] == '\n' || v[n - 1] == '\r' || v[n - 1] == ' ')) n--;
	if (n < 8 || sd_is_public(v, 0, n)) return; uint64_t a, b; sd_fingerprint(v, n, &a, &b); sd_known_add_fp(n, a, b);
	/* and its pieces of a third of it (10 at least), each with a letter and a digit or a sign in it (not a word): a
	 * secret given in two or three parts is still found, part by part */
	size_t w = n / 3 > 10 ? n / 3 : 10;
	if (n >= 16) for (size_t i = 0, c = 0; i + w <= n && c < 64; i++) { int al = 0, ot = 0; for (size_t q = i; q < i + w; q++) { if (isalpha((unsigned char)v[q])) al = 1; else ot = 1; }
		if (al && ot) { sd_fingerprint(v + i, w, &a, &b); sd_known_add_fp(w, a, b); c++; } }
	/* and as it is with its spacing taken out, for the text with its spacing taken out (sd_stripped) */
	char t[520]; size_t k = 0; for (size_t i = 0; i < n && k < sizeof t; i++) { unsigned char c = v[i]; if (isspace(c) || strchr("-.,`'\"|*>", c)) continue; t[k++] = c; }
	if (k != n && k >= 8) { sd_fingerprint(t, k, &a, &b); sd_known_add_fp(k, a, b); }
	memset(t, 0, sizeof t);
}
SD_FN void sd_known_clear(void) { free(SD_K.f); SD_K.f = NULL; SD_K.n = SD_K.cap = 0; }
/* every place in s where a known secret is, as hits of kind SD_OWN (map: s's offsets back to the original's) */
SD_FN void sd_known_scan(const char *s, size_t n, const size_t *map, size_t orig_n, sd_hits *H)
{
	if (!SD_K.n || !SD_K.ready) return;
	size_t lens[64]; int nl = 0;
	for (size_t i = 0; i < SD_K.n && nl < 64; i++) { int seen = 0; for (int j = 0; j < nl; j++) if (lens[j] == SD_K.f[i].len) seen = 1; if (!seen) lens[nl++] = SD_K.f[i].len; }
	for (int j = 0; j < nl; j++) { size_t L = lens[j]; if (L > n) continue;
		uint64_t p1 = 1, p2 = 1; for (size_t i = 0; i < L; i++) { p1 = sd_mulm(p1, SD_K.b1); p2 = sd_mulm(p2, SD_K.b2); }   /* B^L */
		uint64_t a = 0, b = 0;
		for (size_t i = 0; i < n; i++) {
			a = sd_mulm(a, SD_K.b1) + (unsigned char)s[i] + 1; if (a >= SD_M61) a -= SD_M61; b = sd_mulm(b, SD_K.b2) + (unsigned char)s[i] + 1; if (b >= SD_M61) b -= SD_M61;
			if (i >= L) { uint64_t o = (unsigned char)s[i - L] + 1; a = (a + SD_M61 - sd_mulm(o, p1)) % SD_M61; b = (b + SD_M61 - sd_mulm(o, p2)) % SD_M61; }
			if (i + 1 >= L) for (size_t k = 0; k < SD_K.n; k++) if (SD_K.f[k].len == L && SD_K.f[k].h1 == a && SD_K.f[k].h2 == b) {
				size_t st = i + 1 - L, en = i + 1;
				if (!map) { while (st > 0 && (isalnum((unsigned char)s[st - 1]) || strchr("_+/-", s[st - 1]))) st--; while (en < n && (isalnum((unsigned char)s[en]) || strchr("_+/-", s[en]))) en++; while (en < n && s[en] == '=') en++; }   /* (the rest of the piece it is in) */
				size_t oa = map ? map[st] : st, ob = map ? map[en - 1] + 1 : en; if (ob > orig_n) ob = orig_n;
				sd_hit_add(H, oa, ob, SD_OWN); break; }
		}
	}
}

/* ---------------- the rules on one text */
static const char *const SD_PREFIX[][2] = {   /* a token's own beginning, its least length after it, its charset (t: [A-Za-z0-9_-], a: alnum) */
	{ "github_pat_", "8t" }, { "ghp_", "8a" }, { "gho_", "8a" }, { "ghu_", "8a" }, { "ghs_", "8a" }, { "ghr_", "8a" },
	{ "glpat-", "20t" }, { "xoxb-", "10t" }, { "xoxp-", "10t" }, { "xoxa-", "10t" }, { "xoxr-", "10t" }, { "xoxs-", "10t" }, { "xoxe-", "10t" }, { "xapp-", "10t" },
	{ "sk_live_", "16a" }, { "rk_live_", "16a" }, { "sk_test_", "16a" }, { "whsec_", "24a" }, { "AIza", "35t" }, { "sk-ant-", "20t" }, { "sk-proj-", "20t" }, { "sk-", "32t" },
	{ "npm_", "36a" }, { "pypi-AgEIcHlwaS5vcmc", "40t" }, { "hf_", "30a" }, { "dop_v1_", "40a" }, { "shpat_", "32a" }, { "AGE-SECRET-KEY-1", "50a" }, { NULL, NULL } };
static const char *const SD_CTX_TOKEN[] = { "private_key", "privatekey", "private-key", "secret_key", "secretkey", "client_secret", "api_key", "apikey", "api-key",
	"access_key", "accesskey", "auth_token", "authtoken", "access_token", "refresh_token", "session_token", "token", "secret", "cookie", "credential", "key", NULL };
static const char *const SD_CTX_PASS[] = { "password", "passwd", "passphrase", "pwd", "psk", "sae_password", "wpa_passphrase", "pin", NULL };
static const char *const SD_PLACEHOLDER[] = { "xxx", "***", "...", "<", "${", "$(", "%s", "{{", "null", "none", "true", "false", "undefined", "your", "example", "redacted",
	"[hidden", "changeit", "secret", "password", "token", "placeholder", "dummy", "******", NULL };
SD_FN int sd_placeholder(const char *s, size_t a, size_t b)
{
	if (b <= a) return 1; size_t n = b - a; int same = 1; for (size_t i = a + 1; i < b; i++) if (s[i] != s[a]) same = 0; if (same && n > 1) return 1;
	for (int i = 0; SD_PLACEHOLDER[i]; i++) if (sd_ncmp(s, b, a, SD_PLACEHOLDER[i])) return 1;
	if (s[a] == '$' || s[a] == '@' || s[a] == '<' || s[a] == '%') return 1;
	return 0;
}
/* keyword K at i is part of an identifier (db_password, GITHUB_TOKEN, "client-secret"): the identifier's start and end */
SD_FN void sd_ident(const char *s, size_t n, size_t i, size_t kl, size_t *ia, size_t *ib)
{
	size_t a = i, b = i + kl; while (a > 0 && (isalnum((unsigned char)s[a - 1]) || s[a - 1] == '_' || s[a - 1] == '-')) a--; while (b < n && (isalnum((unsigned char)s[b]) || s[b] == '_' || s[b] == '-')) b++;
	*ia = a; *ib = b;
}
/* after an identifier: [spaces] [quote] (= | : | => | :=) [spaces] [quote] VALUE: the value's span, and whether it was quoted */
SD_FN int sd_assign(const char *s, size_t n, size_t i, size_t *va, size_t *vb, int *quoted)
{
	if (i < n && (s[i] == '"' || s[i] == '\'')) i++;
	while (i < n && (s[i] == ' ' || s[i] == '\t')) i++;
	if (i < n && s[i] == ':' && i + 1 < n && s[i + 1] == '=') i += 2; else if (i + 1 < n && s[i] == '=' && s[i + 1] == '>') i += 2; else if (i < n && (s[i] == '=' || s[i] == ':')) i++; else return 0;
	if (i < n && s[i] == '=') return 0;   /* (==: a comparison) */
	while (i < n && (s[i] == ' ' || s[i] == '\t')) i++;
	char q = 0; if (i < n && (s[i] == '"' || s[i] == '\'' || s[i] == '`')) q = s[i++];
	size_t a = i; if (q) { while (i < n && s[i] != q && s[i] != '\n') i++; }
	else while (i < n && !isspace((unsigned char)s[i]) && !strchr(",;)}]\"'`", s[i])) i++;
	*va = a; *vb = i; *quoted = q != 0; return i > a;
}
SD_FN int sd_line_has(const char *s, size_t n, size_t i, const char *w, size_t back)   /* w in the same line, within BACK before i */
{
	size_t a = i > back ? i - back : 0; for (size_t k = i; k > a; k--) if (s[k - 1] == '\n') { a = k; break; }
	size_t at; return sd_find_ci(s, i, a, w, &at) != NULL;
}

SD_FN void sd_rules(const char *s, size_t n, sd_hits *H, int depth);
/* after a BEGIN line (ending at LE): 1 when a key's body follows (its armour's headers, then base64), 0 when what follows
 * is not one, -1 when too little has come to tell (a stream) */
SD_FN int sd_pem_body(const char *s, size_t n, size_t le)
{
	size_t i = le < n ? le + 1 : n; int lines = 0;
	while (i < n && lines < 8) {
		size_t e = i; while (e < n && s[e] != '\n') e++;
		if (e == n) { size_t r = sd_run(s, n, i, sd_b64c); if (r < e && !memchr(s + i, ':', e - i)) return 0; return e - i >= 40 && r == e ? 1 : -1; }
		size_t r = sd_run(s, e, i, sd_b64c); size_t L = e - i; if (L && s[e - 1] == '\r') L--;
		if (L == 0 || memchr(s + i, ':', L)) { i = e + 1; lines++; continue; }   /* (a blank line, "Version: ...", "Proc-Type: ...") */
		return r - i >= 16 && r - i + 1 >= L;
	}
	return i >= n ? -1 : 0;
}
/* base64 or hex that decodes to a private key, or to text with a secret in it: the whole run */
SD_FN void sd_decoded(const char *s, size_t n, size_t a, size_t b, sd_hits *H, int depth, const size_t *map, size_t on)
{
	size_t len = b - a; if (len < 16 || depth > 1) return;
	unsigned char *d = malloc(len + 4); if (!d) return; long k = -1; int hex = 1;
	for (size_t i = a; i < b; i++) if (!sd_hexc((unsigned char)s[i])) { hex = 0; break; }
	if (hex && len % 2 == 0 && len >= 16) { k = 0; for (size_t i = a; i + 1 < b; i += 2) { unsigned v; char t[3] = { s[i], s[i + 1], 0 }; sscanf(t, "%x", &v); d[k++] = v; } }
	else k = sd_b64dec(s + a, len, d, len);
	if (k > 0) {
		int kind = 0;
		if (sd_der_private(d, k)) kind = SD_PRIVKEY;
		else if (sd_printable(d, k)) { sd_hits h2 = {0}; sd_rules((const char *)d, k, &h2, depth + 1); for (size_t i = 0; i < h2.n; i++) kind |= h2.h[i].k; free(h2.h);
			sd_hits h3 = {0}; sd_known_scan((const char *)d, k, NULL, k, &h3); if (h3.n) kind |= SD_OWN; free(h3.h); }
		if (kind) { size_t oa = map ? map[a] : a, ob = map ? map[b - 1] + 1 : b; if (ob > on) ob = on; sd_hit_add(H, oa, ob, kind); }
	}
	memset(d, 0, len + 4); free(d);
}
SD_FN void sd_rules(const char *s, size_t n, sd_hits *H, int depth)
{
	size_t at;
	/* PEM, PGP armour and the like: from BEGIN to its END (or the end of the text) */
	for (size_t i = 0; sd_find_w(s, n, i, "BEGIN", &at); i = at + 5) {
		size_t le = at; while (le < n && s[le] != '\n') le++;
		size_t pk; if (!sd_find_w(s, le, at, "PRIVATE KEY", &pk) && !sd_find_w(s, le, at, "SECRET KEY", &pk)) continue;
		if (pk - at > 40) continue;
		if (!sd_pem_body(s, n, le)) continue;   /* (a key is named, as in a sentence about one, but none follows) */
		size_t ls = at; while (ls > 0 && s[ls - 1] == '-' && at - ls < 12) ls--;   /* (the dashes before it) */
		size_t e = n, en;
		if (sd_find_w(s, n, le, "END", &en) && sd_find_w(s, n, en, "KEY", &pk)) { e = pk + 3; while (e < n && s[e] != '\n') e++; }
		sd_hit_add(H, ls, e, SD_PRIVKEY); at = e > at + 5 ? e - 5 : at;
	}
	/* the end of a key without its beginning: the base64 lines before "END ... PRIVATE KEY" */
	for (size_t i = 0; sd_find_w(s, n, i, "END", &at); i = at + 3) {
		size_t le = at; while (le < n && s[le] != '\n') le++; size_t pk;
		if (!sd_find_w(s, le, at, "PRIVATE KEY", &pk) || pk - at > 40) continue;
		size_t ls = at; while (ls > 0 && s[ls - 1] != '\n') ls--;
		size_t st = ls; while (st > 0) { size_t p = st - 1, q = p; while (q > 0 && s[q - 1] != '\n') q--; size_t r = q; while (r < p && sd_b64c((unsigned char)s[r])) r++; if (r < p || p - q < 16) break; st = q; }
		sd_hit_add(H, st, le, SD_PRIVKEY);
	}
	if (sd_find_ci(s, n, 0, "PuTTY-User-Key-File-", &at)) { size_t e = n, pm; if (sd_find_ci(s, n, at, "Private-MAC:", &pm)) { e = pm; while (e < n && s[e] != '\n') e++; } sd_hit_add(H, at, e, SD_PRIVKEY); }
	/* tokens by their own beginning */
	for (size_t i = 0; i < n; i++) {
		if (i && sd_tokc((unsigned char)s[i - 1]) && s[i - 1] != '-' ) continue;
		for (int p = 0; SD_PREFIX[p][0]; p++) { size_t l = strlen(SD_PREFIX[p][0]); if (i + l > n || memcmp(s + i, SD_PREFIX[p][0], l)) continue;
			int min = atoi(SD_PREFIX[p][1]); char cs = SD_PREFIX[p][1][strlen(SD_PREFIX[p][1]) - 1];
			size_t e = i + l; while (e < n && (cs == 't' ? sd_tokc((unsigned char)s[e]) : isalnum((unsigned char)s[e]))) e++;
			if ((int)(e - i - l) >= min) { sd_hit_add(H, i, e, !strcmp(SD_PREFIX[p][0], "AGE-SECRET-KEY-1") ? SD_PRIVKEY : SD_TOKEN); i = e; break; } }
	}
	/* AWS: the key's id, and a 40-character secret near it or in its context */
	for (size_t i = 0; i + 20 <= n; i++) {
		if ((memcmp(s + i, "AKIA", 4) && memcmp(s + i, "ASIA", 4)) || (i && isalnum((unsigned char)s[i - 1]))) continue;
		size_t e = i + 4; while (e < n && (isupper((unsigned char)s[e]) || isdigit((unsigned char)s[e]))) e++;
		if (e - i != 20) continue; sd_hit_add(H, i, e, SD_TOKEN);
		for (size_t j = e; j < n && j < e + 300; j++) { if (j && sd_b64c((unsigned char)s[j - 1])) continue; size_t r = j; while (r < n && (isalnum((unsigned char)s[r]) || s[r] == '/' || s[r] == '+')) r++;
			if (r - j == 40 && sd_keyish(s, j, r, 40)) sd_hit_add(H, j, r, SD_TOKEN); if (r > j) j = r; }
	}
	/* JWTs */
	for (size_t i = 0; sd_find_ci(s, n, i, "eyJ", &at); i = at + 3) {
		if (s[at] != 'e' || (at && sd_tokc((unsigned char)s[at - 1]))) continue;
		size_t p = sd_run(s, n, at, sd_tokc); if (p - at < 10 || p >= n || s[p] != '.') continue;
		size_t q = sd_run(s, n, p + 1, sd_tokc); if (q - p < 10 || q >= n || s[q] != '.') continue;
		size_t r = sd_run(s, n, q + 1, sd_tokc); if (r - q < 8) continue; sd_hit_add(H, at, r, SD_TOKEN); at = r;
	}
	/* Authorization: Bearer|token|Basic VALUE */
	for (size_t i = 0; sd_find_ci(s, n, i, "Bearer", &at) || sd_find_ci(s, n, i, "Basic ", &at) || sd_find_ci(s, n, i, "Authorization: token ", &at); i = at + 5) {
		size_t v = at; while (v < n && !isspace((unsigned char)s[v])) v++; while (v < n && (s[v] == ' ' || s[v] == '\t')) v++;
		size_t e = v; while (e < n && !isspace((unsigned char)s[e]) && !strchr("\"'`,;", s[e])) e++;
		if (e - v >= 12 && !sd_placeholder(s, v, e) && sd_classes(s, v, e) >= 2) sd_hit_add(H, v, e, SD_TOKEN);
	}
	/* user:password@ in an address */
	for (size_t i = 0; sd_find_ci(s, n, i, "://", &at); i = at + 3) {
		size_t u = at + 3, c = u; while (c < n && s[c] != ':' && s[c] != '@' && s[c] != '/' && !isspace((unsigned char)s[c])) c++;
		if (c >= n || s[c] != ':') continue; size_t p = c + 1, e = p; while (e < n && s[e] != '@' && s[e] != '/' && !isspace((unsigned char)s[e])) e++;
		if (e < n && s[e] == '@' && e > p && !sd_placeholder(s, p, e)) sd_hit_add(H, p, e, SD_PASSWORD);
	}
	/* otpauth:// (an authenticator's secret) */
	for (size_t i = 0; sd_find_ci(s, n, i, "otpauth://", &at); i = at + 10) { size_t e = at; while (e < n && !isspace((unsigned char)s[e]) && !strchr("\"'`)>", s[e])) e++; size_t sc;
		if (sd_find_ci(s, e, at, "secret=", &sc) && sd_run(s, e, sc + 7, isalnum_u) - sc - 7 >= 16) sd_hit_add(H, at, e, SD_CODE); }
	/* two-step and recovery codes in their context */
	static const char *const CODE[] = { "verification code", "two-step code", "2fa code", "authenticator code", "one-time code", "security code", "sign-in code", "login code", "otp", "recovery code", "backup code", NULL };
	for (int c = 0; CODE[c]; c++) for (size_t i = 0; sd_find_ci(s, n, i, CODE[c], &at); i = at + 1) {
		if (at && isalnum((unsigned char)s[at - 1])) continue; size_t e = at + strlen(CODE[c]); if (e < n && isalpha((unsigned char)s[e]) && s[e] != 's') continue;
		size_t lim = e + 24 < n ? e + 24 : n;
		for (size_t j = e; j < lim; j++) { if (!isalnum((unsigned char)s[j])) continue; size_t r = j; while (r < n && (isalnum((unsigned char)s[r]) || (s[r] == '-' && r + 1 < n && isalnum((unsigned char)s[r + 1])))) r++;
			int dg = 0; for (size_t q = j; q < r; q++) if (isdigit((unsigned char)s[q])) dg++;
			if (r - j >= 6 && r - j <= 24 && dg * 2 >= (int)(r - j) - 2 && dg >= 4) { sd_hit_add(H, j, r, SD_CODE); break; } j = r; }
	}
	/* BitLocker's recovery key: 8 groups of 6 digits */
	for (size_t i = 0; i + 55 <= n; i++) { if (i && isdigit((unsigned char)s[i - 1])) continue; size_t p = i; int g = 0;
		while (g < 8) { size_t q = p; while (q < n && isdigit((unsigned char)s[q])) q++; if (q - p != 6) break; g++; p = q; if (g < 8) { if (p < n && s[p] == '-') p++; else break; } }
		if (g == 8) { sd_hit_add(H, i, p, SD_RECOVERY); i = p; } }
	/* WireGuard's PrivateKey = ..., and the values after a password or a token's name */
	for (size_t i = 0; i < n; i++) {
		if (!isalpha((unsigned char)s[i]) || (i && isalnum((unsigned char)s[i - 1]))) continue;
		int kind = 0; size_t kl = 0;
		for (int k = 0; SD_CTX_PASS[k] && !kind; k++) if (sd_ncmp(s, n, i, SD_CTX_PASS[k])) { kind = SD_PASSWORD; kl = strlen(SD_CTX_PASS[k]); }
		for (int k = 0; SD_CTX_TOKEN[k] && !kind; k++) if (sd_ncmp(s, n, i, SD_CTX_TOKEN[k])) { kind = SD_TOKEN; kl = strlen(SD_CTX_TOKEN[k]); }
		if (!kind) continue;
		size_t ia, ib; sd_ident(s, n, i, kl, &ia, &ib);
		/* (public keys and fingerprints, and words that only contain the name: "passing", "keyboard", "tokenizer") */
		int pub = 0; for (size_t q = ia; q + 3 <= ib; q++) if (sd_ncmp(s, ib, q, "pub") || sd_ncmp(s, ib, q, "fingerprint") || sd_ncmp(s, ib, q, "hash") || sd_ncmp(s, ib, q, "_id") || sd_ncmp(s, ib, q, "websocket")) pub = 1;
		static const char *const NOT[] = { "store", "file", "path", "policy", "length", "len", "min", "max", "field", "prompt", "label", "reset", "change", "manager", "mode", "type", "check",
			"hint", "strength", "url", "uri", "name", "count", "size", "dir", "endpoint", "expir", "ttl", "help", "input", "box", "button", "form", NULL };
		for (size_t q = i + kl; q < ib && !pub; q++) for (int z = 0; NOT[z]; z++) if (sd_ncmp(s, ib, q, NOT[z])) { pub = 1; break; }
		if (sd_line_has(s, n, ia, "public", 40) || sd_line_has(s, n, ia, "pubkey", 40)) pub = 1;
		size_t end = i + kl; if (end < n && isalpha((unsigned char)s[end]) && !(kind == SD_TOKEN && (sd_ncmp(s, n, end, "s") || sd_ncmp(s, n, end, "_")))) { if (!(end < ib && (s[end] == '_' || s[end] == '-'))) { i = ib; continue; } }
		size_t va, vb; int q; if (pub || !sd_assign(s, n, ib, &va, &vb, &q)) { i = ib; continue; }
		if (sd_placeholder(s, va, vb) || sd_is_public(s, va, vb)) { i = vb; continue; }
		if (kind == SD_PASSWORD) {
			/* code (password = input(...), pw = getpass(), PASSWORD=$PW) is not a password; a literal or a plain word with a digit is */
			int code = 0; for (size_t k = va; k < vb; k++) if (strchr("()[]{}?", s[k]) || (!q && strchr("<>*&|", s[k])) || (k + 1 < vb && ((s[k] == '=' && s[k + 1] == '=') || (s[k] == '&' && s[k + 1] == '&') || (s[k] == '|' && s[k + 1] == '|')))) code = 1;
			int ident = 1; for (size_t k = va; k < vb; k++) if (!(isalpha((unsigned char)s[k]) || s[k] == '_' || s[k] == '.')) ident = 0;
			if (!code && (q ? vb - va >= 3 : vb - va >= 4 && !ident)) sd_hit_add(H, va, vb, SD_PASSWORD);
		} else {
			int code = 0; for (size_t k = va; k < vb; k++) if (strchr("()[]{} ", s[k]) || (!q && s[k] == '.')) code = 1;
			if (code) { i = vb; continue; }
			int priv = sd_ncmp(s, n, ia, "private") || sd_ncmp(s, n, ia, "secret_key") || sd_ncmp(s, n, ia, "secretkey");
			for (size_t k = ia; k + 7 <= ib; k++) if (sd_ncmp(s, ib, k, "private")) priv = 1;
			if (sd_keyish(s, va, vb, priv ? 16 : 20) || (priv && vb - va >= 40)) sd_hit_add(H, va, vb, priv ? SD_PRIVKEY : SD_TOKEN);
		}
		i = vb;
	}
	/* e-mail addresses (personal details: sent only after the person allows it) */
	for (size_t i = 0; i < n; i++) { if (s[i] != '@' || !i) continue; size_t a = i; while (a > 0 && (isalnum((unsigned char)s[a - 1]) || strchr("._%+-", s[a - 1]))) a--;
		size_t b = i + 1; while (b < n && (isalnum((unsigned char)s[b]) || s[b] == '.' || s[b] == '-')) b++; while (b > i + 1 && s[b - 1] == '.') b--;
		size_t dot = 0; for (size_t k = i + 1; k < b; k++) if (s[k] == '.') dot = k;
		if (a == i || !dot || b - dot < 3) continue;
		if (sd_find_ci(s, i, a, "noreply", &at) || sd_find_ci(s, i, a, "no-reply", &at) || sd_find_ci(s, b, i, "noreply", &at) || sd_find_ci(s, b, i, "example.", &at)) continue;
		sd_hit_add(H, a, b, SD_PERSONAL); }
	/* (public keys' and certificates' armour: what is inside is public) */
	size_t pubs[16][2]; int np = 0;
	for (size_t i = 0; np < 16 && sd_find_ci(s, n, i, "BEGIN", &at); i = at + 5) { size_t le = at; while (le < n && s[le] != '\n') le++; size_t k;
		if (!sd_find_ci(s, le, at, "PUBLIC KEY", &k) && !sd_find_ci(s, le, at, "CERTIFICATE", &k)) continue;
		size_t e = n, en; if (sd_find_ci(s, n, le, "END", &en)) { e = en; while (e < n && s[e] != '\n') e++; } pubs[np][0] = at; pubs[np][1] = e; np++; }
	/* a run that looks like a piece of a key or token on its own: 24 or more characters of base64, with small and capital
	 * letters and digits (3 of each at least) and random-looking; not a hash, an address's path, or data in a data: URI */
	for (size_t i = 0; i < n; ) {
		if (!sd_b64n((unsigned char)s[i]) || (i && (s[i - 1] == '/' || s[i - 1] == '.' || s[i - 1] == '#' || s[i - 1] == '?' || s[i - 1] == '&'))) { size_t e = sd_b64r(s, n, i); i = e > i ? e : i + 1; continue; }
		size_t e = sd_b64r(s, n, i);
		if (e - i >= 20) { int lo = 0, up = 0, dg = 0; for (size_t q = i; q < e; q++) { unsigned char c = s[q]; lo += islower(c) != 0; up += isupper(c) != 0; dg += isdigit(c) != 0; }
			int hashy = sd_line_has(s, n, i, "sha", 16) || sd_line_has(s, n, i, "hash", 16) || sd_line_has(s, n, i, "integrity", 24) || sd_line_has(s, n, i, "base64,", 12)
				|| sd_line_has(s, n, i, "checksum", 16) || sd_line_has(s, n, i, "digest", 16) || sd_line_has(s, n, i, "nonce", 12) || sd_line_has(s, n, i, "http", 60) || sd_line_has(s, n, i, "ssh-", 30)
				|| sd_line_has(s, n, i, "public", 40) || sd_line_has(s, n, i, "age1", 4);
			int slash = 0; for (size_t q = i; q < e; q++) if (s[q] == '/') slash++;
			for (int q = 0; q < np; q++) if (i >= pubs[q][0] && e <= pubs[q][1]) hashy = 1;
			{ size_t a3 = i, b3 = e; if (b3 - a3 > 2 && s[a3] == '0' && (s[a3 + 1] == 'x' || s[a3 + 1] == 'X')) a3 += 2; while (b3 > a3 && strchr("uUlL", s[b3 - 1])) b3--;
			  int hx = b3 > a3; for (size_t q = a3; q < b3; q++) if (!sd_hexc((unsigned char)s[q])) hx = 0; if (hx) hashy = 1; }   /* (a number in hex: 0x9bdc06a725c71235ULL) */
			size_t at2; if (sd_find_ci(s, e, i, "abcdefgh", &at2) || sd_find_ci(s, e, i, "0123456789", &at2) || sd_find_ci(s, e, i, "qwerty", &at2)) hashy = 1;   /* (an alphabet: a charset in code) */
			/* (a name in code, EVP_PKEY_CTX_set1_hkdf_key; a path, repos/owner/Name: parts that are words) */
			{ int segs = 0, wordy = 0; size_t a2 = i; for (size_t q = i; q <= e; q++) if (q == e || s[q] == '_' || s[q] == '/') { size_t L = q - a2; int lw = L >= 2, uw = L >= 2, cap = L >= 3 && isupper((unsigned char)s[a2]);
					for (size_t z = a2; z < q; z++) { unsigned char c = s[z]; if (!(islower(c) || isdigit(c))) lw = 0; if (!(isupper(c) || isdigit(c))) uw = 0; if (z > a2 && !islower(c)) cap = 0; }
					segs++; if ((lw || uw || cap) && L <= 12) wordy++; a2 = q + 1; }
				if (segs >= 2 && (wordy * 2 >= segs || wordy >= 2)) hashy = 1; }
			/* (CamelCase names change between small and capital letters seldom, a random string at most of its characters) */
			int words = 0; for (size_t q = i; q + 2 < e; q++) if (isupper((unsigned char)s[q]) && islower((unsigned char)s[q + 1]) && islower((unsigned char)s[q + 2])) words++;
			int tr = 0; for (size_t q = i + 1; q < e; q++) { unsigned char x = s[q - 1], y = s[q]; int cx = islower(x) ? 0 : isupper(x) ? 1 : isdigit(x) ? 2 : 3, cy = islower(y) ? 0 : isupper(y) ? 1 : isdigit(y) ? 2 : 3; tr += cx != cy; }
			/* (base64 of ordinary text is not a token itself: what it says is looked at below) */
			if (!hashy) { unsigned char dd[96]; long dk = e - i <= 128 ? sd_b64dec(s + i, e - i, dd, sizeof dd) : -1; if (dk > 0 && sd_printable(dd, dk)) hashy = 1; memset(dd, 0, sizeof dd); }
			if (lo >= 3 && up >= 3 && dg >= 1 && (tr * 2 >= (int)(e - i) || (tr * 5 >= (int)(e - i) * 2 && words * 2 < up)) && !hashy && slash < 3 && sd_entropy(s, i, e) >= (e - i < 24 ? 3.8 : 4.0) && !sd_is_public(s, i, e)) sd_hit_add(H, i, e, SD_TOKEN); }
		i = e > i ? e : i + 1;
	}
	/* long runs: a key's body without its header (base64 that decodes to a private key), or something encoded */
	for (size_t i = 0; i < n; ) {
		if (!sd_b64n((unsigned char)s[i])) { i++; continue; }
		size_t e = sd_b64r(s, n, i);
		/* (a PEM body: base64 lines one after another) */
		size_t f = e; while (f + 1 < n && s[f] == '\n') { size_t g = sd_b64r(s, n, f + 1); if (g - f - 1 < 16) break; f = g; }
		if (f - i >= 16 && !sd_is_public(s, i, e)) {
			if (f > e) { size_t L = f - i; char *t = malloc(L + 1); size_t *mp = malloc((L + 1) * sizeof *mp), k = 0;
				for (size_t q = i; q < f; q++) if (s[q] != '\n') { mp[k] = q; t[k++] = s[q]; }
				sd_decoded(t, k, 0, k, H, depth, mp, n); free(t); free(mp); }
			else sd_decoded(s, n, i, e, H, depth, NULL, n);
		}
		i = e > i ? e : i + 1;
	}
}
/* the text with its spacing taken out ("g h p _ 1 2 3", one piece a line), mapped back to the text's offsets */
SD_FN void sd_stripped(const char *s, size_t n, sd_hits *H)
{
	char *t = malloc(n + 1); size_t *mp = malloc((n + 1) * sizeof *mp), k = 0; if (!t || !mp) { free(t); free(mp); return; }
	for (size_t i = 0; i < n; i++) { unsigned char c = s[i];
		if (c == 0xe2 && i + 2 < n && (unsigned char)s[i + 1] == 0x80 && ((unsigned char)s[i + 2] >= 0x8b && (unsigned char)s[i + 2] <= 0x8d)) { i += 2; continue; }   /* (zero-width) */
		if (isspace(c) || strchr("-.,`'\"|*>", c)) continue; mp[k] = i; t[k++] = c; }
	t[k] = 0;
	if (k && k < n) {
		sd_hits h = {0};
		for (size_t i = 0; i < k; i++) {   /* the tokens' beginnings only (with their dashes gone, those that have them are matched without) */
			if (mp[i] && isalnum((unsigned char)s[mp[i] - 1])) continue;   /* (at a word's start in the text) */
			for (int p = 0; SD_PREFIX[p][0]; p++) { const char *w = SD_PREFIX[p][0]; char nd[40]; size_t m = 0; for (const char *c = w; *c && m < 39; c++) if (*c != '-') nd[m++] = *c; nd[m] = 0;
				if (m < 4 || !strcmp(nd, "skant") || !strcmp(nd, "skproj") || i + m > k || memcmp(t + i, nd, m)) continue; size_t e = i + m;
				/* (the run goes on over spacing of a few characters at most, never over the end of a sentence or a list's comma) */
				while (e < k && e < i + m + 100 && sd_tokc((unsigned char)t[e])) { if (e > i) { size_t g0 = mp[e - 1] + 1, g1 = mp[e]; int stop = g1 - g0 > 3;
						for (size_t q = g0; q < g1 && !stop; q++) if (s[q] == '.' || s[q] == ',') stop = 1; if (stop) break; } e++; }
				int lo = 0, up = 0, dg = 0; for (size_t q = i + m; q < e; q++) { unsigned char c = t[q]; lo |= islower(c) != 0; up |= isupper(c) != 0; dg |= isdigit(c) != 0; }
				if ((int)(e - i - m) >= atoi(SD_PREFIX[p][1]) && lo + up + dg >= 3) { size_t a = mp[i], b = mp[e - 1] + 1;
					/* only when it was really spread out: a token written whole is the raw rules' */
					if (b - a > e - i) sd_hit_add(&h, a, b, !strcmp(w, "AGE-SECRET-KEY-1") ? SD_PRIVKEY : SD_TOKEN); break; } }
		}
		for (size_t i = 0; i < k; ) { if (!sd_b64n((unsigned char)t[i])) { i++; continue; } size_t e = sd_b64r(t, k, i);
			if (e - i >= 40 && mp[e - 1] + 1 - mp[i] > e - i) sd_decoded(t, k, i, e, &h, 0, mp, n); i = e; }
		/* a key's armour spelled out ("B E G I N  O P E N S S H  P R I V A T E  K E Y", one piece a line): from it to its end */
		{ size_t at2, pk2; for (size_t i = 0; sd_find_ci(t, k, i, "PRIVATEKEY", &pk2); i = pk2 + 10) { size_t b0 = pk2 > 30 ? pk2 - 30 : 0;
				if (memcmp(t + pk2, "PRIVATEKEY", 10) || !sd_find_ci(t, pk2, b0, "BEGIN", &at2) || memcmp(t + at2, "BEGIN", 5)) continue; size_t e2 = k, en2;
				if (sd_find_ci(t, k, pk2 + 10, "END", &en2) && sd_find_ci(t, k, en2, "PRIVATEKEY", &e2)) e2 += 10; else e2 = k;
				/* (no further than the spelling goes: pieces of at most 6 characters, with spacing and dashes between) */
				size_t piece = 1;
				for (size_t q = at2 + 1; q < e2; q++) { size_t g0 = mp[q - 1] + 1, g1 = mp[q]; int stop = g1 - g0 > 40;
					for (size_t z = g0; z < g1 && !stop; z++) if (!isspace((unsigned char)s[z]) && s[z] != '-' && !((unsigned char)s[z] & 0x80)) stop = 1;   /* (zero-width spaces too) */
					if (stop) { e2 = q; break; }
					if (g1 == g0) { if (++piece > 6) { size_t st0 = q; while (st0 > at2 && mp[st0 - 1] + 1 == mp[st0]) st0--; e2 = st0 > at2 ? st0 : q; break; } } else piece = 1; }
				size_t maxrun = 0, run = 0; for (size_t q = mp[at2]; q <= mp[e2 - 1]; q++) { unsigned char c = s[q]; if (isspace(c) || (c & 0x80) || strchr("-.,`'\"|*>", c)) run = 0; else if (++run > maxrun) maxrun = run; }
				if (maxrun <= 6 && e2 >= pk2 + 10) sd_hit_add(&h, mp[at2], mp[e2 - 1] + 1, SD_PRIVKEY); } }
		sd_known_scan(t, k, mp, n, &h);
		/* only what was really spelled out: in the text, pieces of at most 16 characters with spacing between */
		for (size_t i = 0; i < h.n; i++) { size_t a = h.h[i].a, b = h.h[i].b, run = 0, maxrun = 0;
			for (size_t q = a; q < b; q++) { unsigned char c = s[q]; if (isspace(c) || (c & 0x80) || strchr("-.,`'\"|*>", c)) run = 0; else if (++run > maxrun) maxrun = run; }
			if (maxrun <= 16) sd_hit_add(H, a, b, h.h[i].k); }
		free(h.h);
	}
	memset(t, 0, k); free(t); free(mp);
}
/* everything found in s[0,n) of the kinds in MASK */
SD_FN void sd_scan(const char *s, size_t n, int mask, sd_hits *H)
{
	sd_hits h = {0};
	sd_rules(s, n, &h, 0); sd_known_scan(s, n, NULL, n, &h); sd_stripped(s, n, &h);
	for (size_t i = 0; i < h.n; i++) if (h.h[i].k & mask) { int k = h.h[i].k & mask; if (sd_is_public(s, h.h[i].a, h.h[i].b)) continue; sd_hit_add(H, h.h[i].a, h.h[i].b, k); }
	free(h.h); sd_hits_merge(H);
}
SD_FN int sd_kinds(const char *s, size_t n, int mask) { sd_hits h = {0}; sd_scan(s, n, mask, &h); int k = 0; for (size_t i = 0; i < h.n; i++) k |= h.h[i].k; free(h.h); return k; }
/* s with each hit replaced by "[hidden: a private key]"; the kinds found or-ed into *kinds */
SD_FN char *sd_redact_n(const char *s, size_t n, int mask, int *kinds, size_t *outn)
{
	sd_hits h = {0}; sd_scan(s, n, mask, &h); sd_buf o = {0}; size_t at = 0;
	for (size_t i = 0; i < h.n; i++) { sd_add(&o, s + at, h.h[i].a - at); sd_adds(&o, "[hidden: "); sd_adds(&o, sd_label(h.h[i].k)); sd_adds(&o, "]"); at = h.h[i].b; if (kinds) *kinds |= h.h[i].k; }
	sd_add(&o, s + at, n - at); free(h.h); if (outn) *outn = o.n; return o.b;
}
SD_FN char *sd_redact(const char *s, int mask, int *kinds) { return sd_redact_n(s ? s : "", s ? strlen(s) : 0, mask, kinds, NULL); }

/* ---------------- a path that is a key file, or a secret's store: never read for Blitzen, never sent */
SD_FN int sd_keypath(const char *p)
{
	if (!p || !*p) return 0; char low[1024]; size_t n = 0; for (; p[n] && n < sizeof low - 1; n++) low[n] = tolower((unsigned char)p[n]); low[n] = 0;
	const char *b = strrchr(low, '/'); b = b ? b + 1 : low; size_t bl = strlen(b);
	if (bl > 4 && !strcmp(b + bl - 4, ".pub") && strncmp(low, "/persist/", 9)) return 0;   /* (a public key) */
	static const char *const EXT[] = { ".pem", ".key", ".p12", ".pfx", ".kdbx", ".keystore", ".jks", ".gpg", ".asc", ".ppk", ".age", ".p8", ".der", ".keyring", ".netrc", NULL };
	for (int i = 0; EXT[i]; i++) { size_t l = strlen(EXT[i]); if (bl >= l && !strcmp(b + bl - l, EXT[i])) return 1; }
	static const char *const NAME[] = { ".netrc", ".pgpass", ".git-credentials", "credentials", "credentials.json", "token", ".env", "secrets", "secrets.json", "shadow", "gshadow", "wifi.conf", "wpa_supplicant.conf", "authorized_keys2", NULL };
	for (int i = 0; NAME[i]; i++) if (!strcmp(b, NAME[i])) return 1;
	if (!strncmp(b, "id_", 3) && strcmp(b + bl - (bl > 4 ? 4 : bl), ".pub")) return 1;   /* (id_ed25519, id_rsa: not id_rsa.pub) */
	if (!strncmp(b, "ssh_host_", 9) && strstr(b, "_key") && !strstr(b, ".pub")) return 1;
	static const char *const DIR[] = { "keys/", ".ssh/", ".gnupg/", "keyrings/", ".password-store/", "rudolf-devlink/", "/persist/os/dev/", "/persist/etc/", "/etc/rudolf/keys", "/run/rudolf/ai/", ".mozilla/", "secrets/", ".aws/", ".kube/", ".docker/config.json", NULL };
	char wrapped[1030]; snprintf(wrapped, sizeof wrapped, "/%s", low);
	for (int i = 0; DIR[i]; i++) { if (DIR[i][0] == '/') { if (!strncmp(low, DIR[i], strlen(DIR[i]))) return 1; } else { char pat[64]; snprintf(pat, sizeof pat, "/%s", DIR[i]); if (strstr(wrapped, pat)) return 1;
		size_t dl = strlen(DIR[i]); if (dl && DIR[i][dl - 1] == '/' && !strncmp(b, DIR[i], dl - 1) && b[dl - 1] == 0) return 1; } }
	if (strstr(low, "/persist/") && (strstr(low, "secret") || strstr(low, "token") || strstr(low, ".key"))) return 1;
	return 0;
}

/* ---------------- a text in pieces (a model's answer) */
#define SD_HOLD 160   /* what is held back at least, in bytes */
#define SD_CTX 80     /* what was given out before, kept to read what follows in its context */
typedef struct { sd_buf all, pend; size_t ctx; int kinds, mask; } sd_stream;
SD_FN void sd_stream_init(sd_stream *st, int mask) { memset(st, 0, sizeof *st); st->mask = mask; sd_add(&st->all, "", 0); sd_add(&st->pend, "", 0); }
/* where the safe part of P ends: before the window, before an unfinished key, a run of key-like characters, a
 * spelled-out run or a block of base64 lines that reach into the window */
SD_FN size_t sd_stream_cut(const char *p, size_t n, int final)
{
	if (final) return n;
	if (n <= SD_HOLD) return 0;
	size_t cut = n - SD_HOLD, at, en;
	/* an unfinished BEGIN ... PRIVATE KEY: hold from its line */
	for (size_t i = 0; sd_find_w(p, n, i, "BEGIN", &at); i = at + 5) { size_t le = at; while (le < n && p[le] != '\n') le++; size_t k;
		if (le == n || ((sd_find_w(p, le, at, "PRIVATE", &k) || sd_find_w(p, le, at, "SECRET", &k)) && sd_pem_body(p, n, le) != 0 && !(sd_find_w(p, n, le, "END", &en) && sd_find_w(p, n, en, "KEY", &k)))) { size_t ls = at; while (ls && p[ls - 1] != '\n') ls--; if (ls < cut) cut = ls; break; } }
	/* back to a space; then back over a run of key-like characters, base64 lines, or short pieces with spaces between */
	while (cut > 0 && !isspace((unsigned char)p[cut - 1])) cut--;
	for (;;) {
		size_t c0 = cut;
		/* the line before: a base64 line of 16 or more characters goes with what follows */
		if (cut > 0 && p[cut - 1] == '\n') { size_t q = cut - 1, a = q; while (a > 0 && p[a - 1] != '\n') a--; size_t r = a; while (r < q && sd_b64c((unsigned char)p[r])) r++; if (r == q && q - a >= 16) cut = a; }
		/* short pieces with a space between (a spelled-out key): "g h p _ a b c" */
		size_t q = cut; int pieces = 0; while (q > 0) { size_t e = q; while (e > 0 && (p[e - 1] == ' ' || p[e - 1] == '-' || p[e - 1] == '\t')) e--; size_t a = e; while (a > 0 && sd_b64c((unsigned char)p[a - 1])) a--; if (e == a || e - a > 4 || e == q) break; q = a; pieces++; }
		if (pieces >= 4) cut = q;
		while (cut > 0 && !isspace((unsigned char)p[cut - 1])) cut--;
		if (cut == c0) break;
	}
	return cut;
}
/* the next part to give out (may be empty); FINAL: all that is left */
SD_FN void sd_stream_push(sd_stream *st, const char *t, size_t n, int final, sd_buf *out)
{
	sd_add(&st->all, t, n); sd_add(&st->pend, t, n);
	/* the context before: the end of what was given out (st->ctx bytes at the start of pend) */
	size_t cx = st->ctx, cut = sd_stream_cut(st->pend.b + cx, st->pend.n - cx, final) + cx;
	if (cut <= cx) return;
	sd_hits h = {0}; sd_scan(st->pend.b, st->pend.n, st->mask, &h);
	for (size_t i = 0; i < h.n; i++) if (h.h[i].a < cut && h.h[i].b > cut && !final) cut = h.h[i].a > cx ? h.h[i].a : cx;   /* (a hit not finished yet: wait for it) */
	if (cut > cx) {
		size_t at = cx;
		for (size_t i = 0; i < h.n; i++) { if (h.h[i].b <= cx || h.h[i].a >= cut) continue; size_t a = h.h[i].a < cx ? cx : h.h[i].a, b = h.h[i].b > cut ? cut : h.h[i].b;
			if (a > at) sd_add(out, st->pend.b + at, a - at); if (b > at) { sd_adds(out, "[hidden: "); sd_adds(out, sd_label(h.h[i].k)); sd_adds(out, "]"); at = b; } st->kinds |= h.h[i].k; }
		if (cut > at) sd_add(out, st->pend.b + at, cut - at);
		/* keep SD_CTX bytes of what went out as the next context */
		size_t keep = cut > SD_CTX ? cut - SD_CTX : 0; memmove(st->pend.b, st->pend.b + keep, st->pend.n - keep); st->pend.n -= keep; st->pend.b[st->pend.n] = 0; st->ctx = cut - keep;
	}
	free(h.h);
}
SD_FN void sd_stream_free(sd_stream *st) { sd_wipe(&st->all); sd_wipe(&st->pend); }

/* ---------------- JSON strings */
static int sd_hexv(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
static void sd_utf8(sd_buf *o, unsigned cp) { char b[4]; int n = cp < 0x80 ? (b[0] = cp, 1) : cp < 0x800 ? (b[0] = 0xc0 | cp >> 6, b[1] = 0x80 | (cp & 63), 2) : cp < 0x10000 ? (b[0] = 0xe0 | cp >> 12, b[1] = 0x80 | (cp >> 6 & 63), b[2] = 0x80 | (cp & 63), 3) : (b[0] = 0xf0 | cp >> 18, b[1] = 0x80 | (cp >> 12 & 63), b[2] = 0x80 | (cp >> 6 & 63), b[3] = 0x80 | (cp & 63), 4); sd_add(o, b, n); }
/* the JSON string at s[i] (its opening quote): decoded into o; the index after its closing quote, or 0 */
SD_FN size_t sd_jstr(const char *s, size_t n, size_t i, sd_buf *o)
{
	if (i >= n || s[i] != '"') return 0; i++; sd_add(o, "", 0);
	while (i < n && s[i] != '"') {
		if (s[i] != '\\') { size_t j = i; while (j < n && s[j] != '"' && s[j] != '\\') j++; sd_add(o, s + i, j - i); i = j; continue; }
		if (i + 1 >= n) return 0; char c = s[i + 1]; i += 2;
		if (c == 'n') sd_add(o, "\n", 1); else if (c == 't') sd_add(o, "\t", 1); else if (c == 'r') sd_add(o, "\r", 1); else if (c == 'b') sd_add(o, "\b", 1); else if (c == 'f') sd_add(o, "\f", 1);
		else if (c == 'u') { if (i + 4 > n) return 0; unsigned cp = 0; for (int k = 0; k < 4; k++) { int v = sd_hexv(s[i + k]); if (v < 0) return 0; cp = cp << 4 | v; } i += 4;
			if (cp >= 0xd800 && cp < 0xdc00 && i + 6 <= n && s[i] == '\\' && s[i + 1] == 'u') { unsigned lo = 0; int ok = 1; for (int k = 0; k < 4; k++) { int v = sd_hexv(s[i + 2 + k]); if (v < 0) ok = 0; else lo = lo << 4 | v; }
				if (ok && lo >= 0xdc00 && lo < 0xe000) { cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00); i += 6; } }
			if (!cp) sd_add(o, " ", 1); else sd_utf8(o, cp); }
		else sd_add(o, &c, 1);
	}
	return i < n ? i + 1 : 0;
}
SD_FN void sd_jput(sd_buf *o, const char *s, size_t n)
{
	sd_add(o, "\"", 1);
	for (size_t i = 0; i < n; i++) { unsigned char c = s[i]; char e[8];
		if (c == '"' || c == '\\') { e[0] = '\\'; e[1] = c; sd_add(o, e, 2); } else if (c == '\n') sd_add(o, "\\n", 2); else if (c == '\r') sd_add(o, "\\r", 2); else if (c == '\t') sd_add(o, "\\t", 2);
		else if (c < 0x20) { snprintf(e, sizeof e, "\\u%04x", c); sd_add(o, e, 6); } else sd_add(o, &c, 1); }
	sd_add(o, "\"", 1);
}
/* every string of the JSON text J redacted (keys are strings too: a key never holds one); the kinds found in *kinds */
SD_FN char *sd_json_redact(const char *j, size_t n, int mask, int *kinds, size_t *outn)
{
	sd_buf o = {0}; sd_add(&o, "", 0);
	for (size_t i = 0; i < n; ) {
		if (j[i] != '"') { size_t k = i; while (k < n && j[k] != '"') k++; sd_add(&o, j + i, k - i); i = k; continue; }
		sd_buf v = {0}; size_t e = sd_jstr(j, n, i, &v);
		if (!e) { sd_add(&o, j + i, n - i); sd_wipe(&v); break; }   /* (not JSON: kept as it was; the caller sends the redacted text only if it parsed) */
		int k = 0; size_t rn; char *r = sd_redact_n(v.b, v.n, mask, &k, &rn);
		if (k) { sd_jput(&o, r, rn); if (kinds) *kinds |= k; } else sd_add(&o, j + i, e - i);
		free(r); sd_wipe(&v); i = e;
	}
	if (outn) *outn = o.n; return o.b;
}
/* a minimal JSON reader: the value of KEY in the object at s[i] (its "{"), or 0 */
SD_FN size_t sd_jskip(const char *s, size_t n, size_t i)
{
	while (i < n && isspace((unsigned char)s[i])) i++; if (i >= n) return 0;
	if (s[i] == '"') { i++; while (i < n && s[i] != '"') { if (s[i] == '\\') i++; i++; } return i < n ? i + 1 : 0; }
	if (s[i] == '{' || s[i] == '[') { int d = 0; for (; i < n; i++) { if (s[i] == '"') { size_t e = sd_jskip(s, n, i); if (!e) return 0; i = e - 1; continue; } if (s[i] == '{' || s[i] == '[') d++; else if (s[i] == '}' || s[i] == ']') { if (!--d) return i + 1; } } return 0; }
	while (i < n && !strchr(",}] \t\r\n", s[i])) i++; return i;
}
SD_FN size_t sd_jget(const char *s, size_t n, size_t i, const char *key)
{
	while (i < n && isspace((unsigned char)s[i])) i++; if (i >= n || s[i] != '{') return 0; i++;
	for (;;) { while (i < n && (isspace((unsigned char)s[i]) || s[i] == ',')) i++; if (i >= n || s[i] != '"') return 0;
		sd_buf k = {0}; size_t e = sd_jstr(s, n, i, &k); if (!e) { sd_wipe(&k); return 0; } int hit = k.b && !strcmp(k.b, key); sd_wipe(&k);
		i = e; while (i < n && (isspace((unsigned char)s[i]) || s[i] == ':')) i++; if (hit) return i; size_t v = sd_jskip(s, n, i); if (!v) return 0; i = v; }
}

/* ---------------- llama-server's answer, relayed (rudolfd's ai/chat; the tests' stand-in engine) */
typedef struct { int kinds; int input_kinds; size_t chars; } sd_relay_res;
static int sd_wr(int fd, const char *b, size_t n) { while (n) { ssize_t w = write(fd, b, n); if (w < 0 && errno == EINTR) continue; if (w <= 0) return -1; b += w; n -= w; } return 0; }
/* one SSE line's JSON: its text (choices[0].delta.content, or .text), or an error */
SD_FN int sd_sse_text(const char *j, size_t n, sd_buf *txt, int *is_err)
{
	*is_err = 0; size_t c = sd_jget(j, n, 0, "choices");
	if (!c) { if (sd_jget(j, n, 0, "error")) *is_err = 1; return 0; }
	if (j[c] != '[') return 0; size_t f = c + 1; while (f < n && isspace((unsigned char)j[f])) f++; if (f >= n || j[f] != '{') return 0;
	size_t d = sd_jget(j, n, f, "delta"), v = d ? sd_jget(j, n, d, "content") : sd_jget(j, n, f, "text");
	if (!v || j[v] != '"') return 0; return sd_jstr(j, n, v, txt) != 0;
}
SD_FN void sd_sse_emit(int pfd, const char *t, size_t n)
{
	if (!n) return; sd_buf o = {0}; sd_adds(&o, "data: {\"object\":\"chat.completion.chunk\",\"choices\":[{\"index\":0,\"delta\":{\"content\":"); sd_jput(&o, t, n); sd_adds(&o, "}}]}\n\n");
	sd_wr(pfd, o.b, o.n); sd_wipe(&o);
}
SD_FN void sd_sse_final(int pfd, sd_stream *st, sd_relay_res *res)
{
	int k = 0; size_t rn; char *all = sd_redact_n(st->all.b, st->all.n, st->mask, &k, &rn); st->kinds |= k; res->kinds |= st->kinds; res->chars = rn;
	sd_buf o = {0}; sd_adds(&o, "data: {\"rudolf\":{\"text\":"); sd_jput(&o, all, rn); sd_adds(&o, ",\"hidden\":[");
	int first = 1; for (int b = 1; b <= SD_PERSONAL; b <<= 1) if (st->kinds & b) { if (!first) sd_adds(&o, ","); first = 0; sd_adds(&o, "\""); sd_adds(&o, sd_name(b)); sd_adds(&o, "\""); }
	sd_adds(&o, "],\"inputHidden\":["); first = 1;
	for (int b = 1; b <= SD_PERSONAL; b <<= 1) if (res->input_kinds & b) { if (!first) sd_adds(&o, ","); first = 0; sd_adds(&o, "\""); sd_adds(&o, sd_name(b)); sd_adds(&o, "\""); }
	sd_adds(&o, "]}}\n\n"); sd_wr(pfd, o.b, o.n); sd_wipe(&o); memset(all, 0, rn); free(all);
}
/* EFD: the engine's connection (its whole HTTP answer); PFD: the page's (what it gets: an HTTP answer of its own).
 * Returns 0, or -1 when the engine gave no answer. */
SD_FN int sd_relay(int efd, int pfd, sd_relay_res *res, int (*tick)(void))
{
	sd_buf raw = {0}, body = {0}; char b[16384]; ssize_t g; size_t he = 0;
	/* the head */
	while (!he) { g = read(efd, b, sizeof b); if (g < 0 && errno == EINTR) continue; if (g <= 0) break; sd_add(&raw, b, g);
		char *e = strstr(raw.b, "\r\n\r\n"); if (e) he = e - raw.b + 4; if (raw.n > 65536 && !he) break; }
	if (!he) { sd_wipe(&raw); return -1; }
	int code = atoi(raw.b + 9 > raw.b + raw.n ? "502" : raw.b + 9); if (code < 100) code = 502;
	char ctype[120] = "application/json"; int chunked = 0; long long clen = -1;
	for (char *l = raw.b; l < raw.b + he; ) { char *e = strstr(l, "\r\n"); if (!e) break;
		if (!strncasecmp(l, "Content-Type:", 13)) { char *v = l + 13; while (*v == ' ') v++; size_t m = e - v < (long)sizeof ctype - 1 ? (size_t)(e - v) : sizeof ctype - 1; memcpy(ctype, v, m); ctype[m] = 0; }
		if (!strncasecmp(l, "Transfer-Encoding:", 18)) { size_t at2; if (sd_find_ci(l, e - l, 0, "chunked", &at2)) chunked = 1; }
		if (!strncasecmp(l, "Content-Length:", 15)) clen = atoll(l + 15);
		l = e + 2; }
	int sse = strstr(ctype, "event-stream") != NULL;
	{ char h[400]; int l = snprintf(h, sizeof h, "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n",
		code, code == 200 ? "OK" : "Error", sse ? "text/event-stream" : "application/json"); sd_wr(pfd, h, l); }
	/* the body, dechunked as it comes */
	sd_buf in = {0}; sd_add(&in, raw.b + he, raw.n - he); sd_wipe(&raw);
	sd_stream st; sd_stream_init(&st, SD_SHOWN); sd_buf lines = {0}; sd_add(&lines, "", 0); int done = 0, eof = 0; long long got = 0;
	size_t cpos = 0; long long csize = -1;   /* (chunked: where the next chunk's size line is, the current chunk's bytes left) */
	for (;;) {
		/* what has come: into body (dechunked) */
		if (chunked) { for (;;) {
				if (csize <= 0) { if (csize == 0) { if (in.n - cpos < 2) break; cpos += 2; csize = -1; }   /* (the CRLF after a chunk) */
					char *nl = memchr(in.b + cpos, '\n', in.n - cpos); if (!nl) break; long long z = strtoll(in.b + cpos, NULL, 16); cpos = nl - in.b + 1;
					if (z == 0) { eof = 1; break; } csize = z; }
				size_t take = in.n - cpos < (size_t)csize ? in.n - cpos : (size_t)csize; if (!take) break;
				sd_add(&body, in.b + cpos, take); cpos += take; csize -= take; if (csize == 0) { /* (wait for its CRLF) */ } }
			if (cpos > 65536) { memmove(in.b, in.b + cpos, in.n - cpos); in.n -= cpos; in.b[in.n] = 0; cpos = 0; } }
		else { sd_add(&body, in.b, in.n); got += in.n; in.n = 0; if (clen >= 0 && got >= clen) eof = 1; }
		/* SSE: each complete line */
		if (sse) { sd_add(&lines, body.b, body.n); body.n = 0;
			char *p = lines.b, *nl;
			while ((nl = memchr(p, '\n', lines.n - (p - lines.b)))) {
				size_t L = nl - p; if (L && p[L - 1] == '\r') L--;
				if (L > 5 && !strncmp(p, "data:", 5)) { const char *d = p + 5; size_t dn = L - 5; while (dn && *d == ' ') { d++; dn--; }
					if (dn == 6 && !strncmp(d, "[DONE]", 6)) { sd_buf o = {0}; sd_stream_push(&st, "", 0, 1, &o); sd_sse_emit(pfd, o.b, o.n); sd_wipe(&o); sd_sse_final(pfd, &st, res); sd_wr(pfd, "data: [DONE]\n\n", 14); done = 1; }
					else if (!done) { sd_buf t = {0}; int err; if (sd_sse_text(d, dn, &t, &err)) { sd_buf o = {0}; sd_stream_push(&st, t.b, t.n, 0, &o); sd_sse_emit(pfd, o.b, o.n); sd_wipe(&o); }
						else if (err) { int k = 0; size_t rn; char *r = sd_json_redact(d, dn, SD_SHOWN, &k, &rn); res->kinds |= k; sd_wr(pfd, "data: ", 6); sd_wr(pfd, r, rn); sd_wr(pfd, "\n\n", 2); free(r); }
						sd_wipe(&t); } }
				p = nl + 1; }
			size_t rest = lines.n - (p - lines.b); memmove(lines.b, p, rest); lines.n = rest; lines.b[rest] = 0; }
		if (eof || done) break;
		g = read(efd, b, sizeof b); if (g < 0 && errno == EINTR) continue; if (g <= 0) break; sd_add(&in, b, g);
		if (tick) tick();
	}
	if (sse && !done) { sd_buf o = {0}; sd_stream_push(&st, "", 0, 1, &o); sd_sse_emit(pfd, o.b, o.n); sd_wipe(&o); sd_sse_final(pfd, &st, res); sd_wr(pfd, "data: [DONE]\n\n", 14); }
	if (!sse) { int k = 0; size_t rn; char *r = sd_json_redact(body.b ? body.b : "", body.n, SD_SHOWN, &k, &rn); res->kinds |= k; sd_wr(pfd, r, rn); free(r); }
	res->kinds |= st.kinds; sd_stream_free(&st); sd_wipe(&lines); sd_wipe(&in); sd_wipe(&body); return 0;
}

/* ---------------- SHA-256 (what an authorization is bound to: the exact bytes the person was shown) */
typedef struct { uint32_t h[8]; uint8_t b[64]; uint64_t n; size_t f; } sd_sha;
static const uint32_t SD_K256[64] = { 0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
	0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
	0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
	0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };
#define SD_ROR(x, n) ((x) >> (n) | (x) << (32 - (n)))
static void sd_sha_blk(sd_sha *c, const uint8_t *p)
{
	uint32_t w[64]; for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4 * i] << 24 | p[4 * i + 1] << 16 | p[4 * i + 2] << 8 | p[4 * i + 3];
	for (int i = 16; i < 64; i++) { uint32_t s0 = SD_ROR(w[i - 15], 7) ^ SD_ROR(w[i - 15], 18) ^ w[i - 15] >> 3, s1 = SD_ROR(w[i - 2], 17) ^ SD_ROR(w[i - 2], 19) ^ w[i - 2] >> 10; w[i] = w[i - 16] + s0 + w[i - 7] + s1; }
	uint32_t a = c->h[0], b = c->h[1], cc = c->h[2], d = c->h[3], e = c->h[4], f = c->h[5], g = c->h[6], h = c->h[7];
	for (int i = 0; i < 64; i++) { uint32_t t1 = h + (SD_ROR(e, 6) ^ SD_ROR(e, 11) ^ SD_ROR(e, 25)) + ((e & f) ^ (~e & g)) + SD_K256[i] + w[i], t2 = (SD_ROR(a, 2) ^ SD_ROR(a, 13) ^ SD_ROR(a, 22)) + ((a & b) ^ (a & cc) ^ (b & cc));
		h = g; g = f; f = e; e = d + t1; d = cc; cc = b; b = a; a = t1 + t2; }
	c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d; c->h[4] += e; c->h[5] += f; c->h[6] += g; c->h[7] += h;
}
SD_FN void sd_sha256_hex(const void *m, size_t n, char out[65])
{
	sd_sha c = { { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 }, {0}, 0, 0 }; const uint8_t *p = m;
	for (size_t i = 0; i < n; i++) { c.b[c.f++] = p[i]; if (c.f == 64) { sd_sha_blk(&c, c.b); c.f = 0; } }
	uint64_t bits = (uint64_t)n * 8; c.b[c.f++] = 0x80; if (c.f > 56) { while (c.f < 64) c.b[c.f++] = 0; sd_sha_blk(&c, c.b); c.f = 0; }
	while (c.f < 56) c.b[c.f++] = 0; for (int i = 7; i >= 0; i--) c.b[c.f++] = bits >> (8 * i); sd_sha_blk(&c, c.b);
	for (int i = 0; i < 8; i++) sprintf(out + 8 * i, "%08x", c.h[i]);
}
#pragma GCC diagnostic pop
#endif
