/* (included by rudolfd.c, not built on its own: hence .inc) ---------- 1.6.1 (docs/plan-1.6.1.md Part 26): Blitzen
 * hardened against giving away secrets.
 * The owner, 2026-11-21: "Add whatever measures would be necessary to harden Blitzen from being socially engineered to
 * display or send sensitive information without proper authorization (in almost every case private keys or private
 * repository keys would NEVER be allowed to be disclosed)."
 * Every rule is here, in rudolfd, outside the model (a local model can ignore what its prompt says), with the detector
 * of rudolf-secrets.h:
 *   - what Blitzen is given (ai/chat's request: what the person typed or pasted, the files and logs added to it, what was
 *     looked up, the web pages) is redacted before the engine sees it;
 *   - what Blitzen writes (ai/chat's streamed answer) is redacted as it streams, before the page has any of it (so before
 *     it is shown, spoken, kept in the history, copied or used by any action), and once more whole at its end;
 *   - what Blitzen reads in Developer mode (ai/dev/file, joblog, a run's JSON, a devlink result) and looks up
 *     (ai/know/...) is redacted, and key files are refused whole (sd_keypath);
 *   - what Blitzen's actions send (ai/dev/push, pr, dispatch, dl-send; ai/run's program) is refused outright when it
 *     carries a private key, a token, a recovery key or one of this computer's secrets, whatever was allowed, and needs
 *     the person's authorization (ai/authorize, from the desktop's own prompt, bound to exactly what is sent, once, for
 *     two minutes) when it carries a password, a sign-in code or personal details;
 *   - each time something was hidden or refused, what kind and which guard (never the secret) goes into the health log
 *     (System health, Security), and the page's security log.
 * This computer's own secrets are matched by their value (guard_load): rudolfd's token, the model engine's key, the
 * signed-in account's developer and devlink tokens, the devlink's own token, the saved Wi-Fi passwords and the
 * account keyring's entries (rudolf-portal --keyring-fingerprints, as the account's user: only two keyed fingerprints of
 * each, never the secret). They are kept only as fingerprints, only in this request's process, and never given to the
 * model. There is no setting, no Developer mode and no administrator that turns any of this off.
 *
 *   POST ai/guard[?send=1]     the body (text) redacted: {"ok":true,"text":...,"hidden":[...]}; with send=1 nothing is
 *                              changed: {"ok":true,"secret":[...],"sensitive":[...]} (what an action would carry)
 *   POST ai/authorize?for=PATH the body is exactly what the request PATH (ai/dev/push?..., without grant=) will send,
 *                              shown to the person by the desktop and allowed there: {"ok":true,"grant":HASH}; refused
 *                              when it carries a secret
 */
#include "rudolf-secrets.h"
/* (rudolfd-dev.inc's, included after this) */
static int dev_token(const char *acct, const char *key, char *tok, size_t n);
static int dev_keyring(char *au, size_t an, char *ae, size_t en);
static char *dev_run(const char *const *argv, const char *in, size_t inl, int secs, int drop, int *st, char *err, size_t en);
#ifndef GUARD_GRANTS
#define GUARD_GRANTS RUN "/ai/grants"
#endif
static int guard_loaded;
/* the values of "psk=", "sae_password=", "password=" lines (wpa_supplicant's saved networks) */
static void guard_wifi(const char *path)
{
	char *t = slurp(path, NULL); if (!t) return;
	for (char *sv, *l = strtok_r(t, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) {
		while (*l == ' ' || *l == '\t') l++;
		const char *K[] = { "psk=", "sae_password=", "password=", "wep_key0=", "private_key_passwd=", NULL };
		for (int i = 0; K[i]; i++) if (!strncmp(l, K[i], strlen(K[i]))) { char *v = l + strlen(K[i]); size_t n = strlen(v);
			if (n >= 2 && *v == '"' && v[n - 1] == '"') { v++; n -= 2; } sd_known_add(v, n); }
	}
	explicit_bzero(t, strlen(t)); free(t);
}
/* this computer's own secrets, by value (once a process: each connection is a process of its own) */
static void guard_load(void)
{
	if (guard_loaded) return; guard_loaded = 1; sd_known_init(0, 0);
	sd_known_add(token, strlen(token));
	{ char *k = slurp(RUN "/ai/key", NULL); if (k) { sd_known_add(k, strlen(k)); explicit_bzero(k, strlen(k)); free(k); } }
	{ char d[128], p[200]; dl_dir(d, sizeof d); snprintf(p, sizeof p, "%s/token", d); char *k = slurp(p, NULL); if (k) { sd_known_add(k, strlen(k)); explicit_bzero(k, strlen(k)); free(k); } }
	guard_wifi(PERSIST "/etc/wifi.conf"); guard_wifi(RUN "/wifi.conf");
	char acct[80]; if (!know_acct(acct, sizeof acct)) snprintf(acct, sizeof acct, "rudolf");
	char t[512]; if (dev_token(acct, "github", t, sizeof t)) sd_known_add(t, strlen(t)); explicit_bzero(t, sizeof t);
	if (dev_token(acct, "devlink", t, sizeof t)) sd_known_add(t, strlen(t)); explicit_bzero(t, sizeof t);
	/* the account keyring's entries: their fingerprints only, made by the account's own rudolf-portal */
	char au[32], ae[160];
	if (dev_keyring(au, sizeof au, ae, sizeof ae) && !access(AU_PORTAL, X_OK)) {
		char in[80]; snprintf(in, sizeof in, "%016llx %016llx\n", (unsigned long long)SD_K.b1, (unsigned long long)SD_K.b2);
		const char *a[] = { "/bin/su", "-s", "/bin/sh", au, "-c", "set -a; . \"$0\"; set +a; exec " AU_PORTAL " --keyring-fingerprints", ae, NULL };
		int st; char *o = dev_run(a, in, strlen(in), 20, 0, &st, NULL, 0);
		for (char *sv, *l = o && !st ? strtok_r(o, "\n", &sv) : NULL; l; l = strtok_r(NULL, "\n", &sv)) {
			unsigned long long n, h1, h2; if (sscanf(l, "%llu %llx %llx", &n, &h1, &h2) == 3) sd_known_add_fp(n, h1, h2); }
		free(o);
	}
}
static void guard_kinds_json(str *s, int k)
{
	sadd(s, "["); int first = 1; for (int b = 1; b <= SD_PERSONAL; b <<= 1) if (k & b) { sadd(s, "%s\"%s\"", first ? "" : ",", sd_name(b)); first = 0; } sadd(s, "]");
}
static void guard_labels(char *o, size_t n, int k)
{
	*o = 0; size_t w = 0; int c = 0; for (int b = 1; b <= SD_PERSONAL; b <<= 1) if (k & b) c++;
	int i = 0; for (int b = 1; b <= SD_PERSONAL; b <<= 1) if (k & b) { i++; w += snprintf(o + w, w < n ? n - w : 0, "%s%s", i == 1 ? "" : i == c ? " and " : ", ", sd_label(b)); }
}
/* the record: what kind, which guard; never the secret */
static void guard_note(const char *guard, int k, int refused)
{
	if (!k) return; char l[200]; guard_labels(l, sizeof l, k);
	health_add("security", "Blitzen: %s %s (%s)", l, refused ? "was refused" : "was hidden", guard);
}
/* S[0,N) redacted for what Blitzen reads or shows; the kinds into *k */
static char *guard_text(const char *s, size_t n, int *k, size_t *on) { guard_load(); return sd_redact_n(s ? s : "", s ? n : 0, SD_SHOWN, k, on); }
static void guard_reply_text(struct req *r, const char *field, const char *t, const char *guard)
{
	int k = 0; size_t on; char *x = guard_text(t, t ? strlen(t) : 0, &k, &on); guard_note(guard, k, 0);
	str s = {0}; sadd(&s, "{\"ok\":true,\"%s\":", field); sjson(&s, x); sadd(&s, ",\"hidden\":"); guard_kinds_json(&s, k); sadd(&s, "}"); free(x); reply_json(r->fd, &s);
}
/* a JSON reply's strings redacted, then sent */
static void guard_reply_json(struct req *r, str *s, const char *guard)
{
	guard_load(); int k = 0; size_t on; char *x = sd_json_redact(s->b ? s->b : "{}", s->b ? s->n : 2, SD_SHOWN, &k, &on); guard_note(guard, k, 0);
	free(s->b); reply(r->fd, 200, "application/json", x, on, NULL); free(x);
}
/* ---------------- the actions' authorization */
static void guard_hash(const char *path, const char *query, const char *body, size_t bl, char out[65])
{
	str q = {0}; sadd(&q, "%s?", path);
	/* (the query without grant=: what is sent, as the person was shown it) */
	if (query) { char *c = strdup(query); int first = 1; for (char *sv, *p = strtok_r(c, "&", &sv); p; p = strtok_r(NULL, "&", &sv)) if (strncmp(p, "grant=", 6)) { sadd(&q, "%s%s", first ? "" : "&", p); first = 0; } free(c); }
	sadd(&q, "\n"); if (body && bl) { if (q.n + bl + 1 > q.cap) { q.cap = q.n + bl + 1; q.b = realloc(q.b, q.cap); } memcpy(q.b + q.n, body, bl); q.n += bl; q.b[q.n] = 0; }
	sd_sha256_hex(q.b, q.n, out); explicit_bzero(q.b, q.n); free(q.b);
}
static int guard_grant_take(const char *hash)
{
	if (!hash || strlen(hash) != 64 || strspn(hash, "0123456789abcdef") != 64) return 0;
	char p[300]; snprintf(p, sizeof p, GUARD_GRANTS "/%s", hash); char *t = slurp(p, NULL); if (!t) return 0;
	long long until = atoll(t); free(t); int ok = !unlink(p) && until >= (long long)time(NULL);   /* (once: taken away as it is used) */
	return ok;
}
/* WHAT is about to send PAYLOAD to DEST: 0 to go on; otherwise the request was answered (refused, or the person's
 * authorization is needed). PATH and the request's query and body are what the authorization is bound to. */
static int guard_action(struct req *r, const char *path, const char *what, const char *payload, size_t n, const char *dest)
{
	guard_load(); int k = sd_kinds(payload ? payload : "", payload ? n : 0, SD_ALL); char l[200];
	if (k & SD_SECRET) {
		guard_labels(l, sizeof l, k & SD_SECRET); guard_note(what, k & SD_SECRET, 1);
		str s = {0}; sadd(&s, "{\"ok\":false,\"refused\":"); guard_kinds_json(&s, k & SD_SECRET);
		char m[400]; snprintf(m, sizeof m, "Refused: this would send %s. Blitzen never sends private keys, tokens or this computer's secrets, whatever is allowed.", l);
		sadd(&s, ",\"error\":"); sjson(&s, m); sadd(&s, "}"); reply(r->fd, 403, "application/json", s.b, s.n, NULL); free(s.b); return 1;
	}
	if (k & SD_SENSITIVE) {
		char h[65]; guard_hash(path, r->query, r->body, r->blen, h);
		if (guard_grant_take(qparam(r->query, "grant"))) {
			char want[65]; snprintf(want, sizeof want, "%s", qparam(r->query, "grant")); if (!strcmp(want, h)) { guard_labels(l, sizeof l, k); health_add("security", "Blitzen: %s sent to %s (%s, the person allowed it)", l, dest, what); return 0; }
		}
		guard_labels(l, sizeof l, k & SD_SENSITIVE);
		str s = {0}; sadd(&s, "{\"ok\":false,\"needs\":\"authorization\",\"kinds\":"); guard_kinds_json(&s, k & SD_SENSITIVE); sadd(&s, ",\"where\":"); sjson(&s, dest);
		char m[400]; snprintf(m, sizeof m, "This would send %s to %s: it goes only if you allow it in the desktop's own prompt.", l, dest);
		sadd(&s, ",\"error\":"); sjson(&s, m); sadd(&s, "}"); reply(r->fd, 428, "application/json", s.b, s.n, NULL); free(s.b); return 1;
	}
	return 0;
}
static void api_ai_authorize(struct req *r)
{
	const char *f = qparam(r->query, "for");
	if (!f || strncmp(f, "ai/dev/", 7) || strlen(f) > 4000) { reply_err(r->fd, 400, "for=ai/dev/... (what will be sent)"); return; }
	char path[200]; const char *q = strchr(f, '?'); size_t pl = q ? (size_t)(q - f) : strlen(f); if (pl >= sizeof path) { reply_err(r->fd, 400, "too long"); return; }
	memcpy(path, f, pl); path[pl] = 0;
	guard_load(); int k = sd_kinds(r->body ? r->body : "", r->body ? r->blen : 0, SD_ALL);
	{ char *qd = q ? strdup(q + 1) : strdup(""); for (char *c = qd; *c; c++) if (*c == '+') *c = ' ';
	  /* (what the query carries: a branch, a title, a message) */ k |= sd_kinds(qd, strlen(qd), SD_ALL); free(qd); }
	if (k & SD_SECRET) { guard_note("an authorization", k & SD_SECRET, 1); reply_err(r->fd, 403, "Refused: what would be sent has a private key, a token or one of this computer's secrets in it. That is never sent."); return; }
	char h[65]; guard_hash(path, q ? q + 1 : NULL, r->body, r->blen, h);
	mkdir(RUN "/ai", 0700); mkdir(GUARD_GRANTS, 0700);
	char p[300], v[40]; snprintf(p, sizeof p, GUARD_GRANTS "/%s", h); snprintf(v, sizeof v, "%lld\n", (long long)time(NULL) + 120);
	if (write_str(p, v)) { reply_err(r->fd, 500, "the authorization could not be kept"); return; }
	str s = {0}; sadd(&s, "{\"ok\":true,\"grant\":\"%s\"}", h); reply_json(r->fd, &s);
}
static void api_ai_guard(struct req *r)
{
	const char *sd = qparam(r->query, "send");
	if (!r->body || r->blen > 600000) { reply_err(r->fd, 400, "nothing to look at"); return; }
	if (sd && !strcmp(sd, "1")) { guard_load(); int k = sd_kinds(r->body, r->blen, SD_ALL); str s = {0}; sadd(&s, "{\"ok\":true,\"secret\":"); guard_kinds_json(&s, k & SD_SECRET); sadd(&s, ",\"sensitive\":"); guard_kinds_json(&s, k & SD_SENSITIVE); sadd(&s, "}"); reply_json(r->fd, &s); return; }
	int k = 0; size_t on; char *x = guard_text(r->body, r->blen, &k, &on); guard_note("what was given to Blitzen", k, 0);
	str s = {0}; sadd(&s, "{\"ok\":true,\"text\":"); sjson(&s, x); sadd(&s, ",\"hidden\":"); guard_kinds_json(&s, k); sadd(&s, "}"); explicit_bzero(x, on); free(x); reply_json(r->fd, &s);
}
/* ai/chat: the request's strings redacted (SD_SHOWN); NULL when it is not JSON */
static char *guard_chat_in(const char *b, size_t n, int *k, size_t *on)
{
	guard_load(); size_t i = 0; while (i < n && isspace((unsigned char)b[i])) i++; if (i >= n || b[i] != '{') return NULL;
	return sd_json_redact(b, n, SD_SHOWN, k, on);
}
/* the engine's streamed answer to the page, redacted; the time of use touched every 30 s while it streams */
static time_t guard_t0;
static int guard_tick(void) { if (time(NULL) - guard_t0 > 30) { ai_touch(); guard_t0 = time(NULL); } return 0; }
static void guard_chat_out(int efd, int pfd, int input_kinds)
{
	guard_load(); sd_relay_res res = { 0, input_kinds, 0 }; guard_t0 = time(NULL);
	if (sd_relay(efd, pfd, &res, guard_tick)) { const char *m = "HTTP/1.1 502 Error\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"ok\":false,\"error\":\"Blitzen is not answering\"}"; send_all(pfd, m, strlen(m)); }
	guard_note("Blitzen's answer", res.kinds, 0);
}
/* a list of PATH<tab>BASE64 lines (ai/dev/push): every path checked, every file decoded into OUT for guard_action */
static int guard_push_files(const char *b, size_t n, str *out, char *bad, size_t bn)
{
	char *c = malloc(n + 1); memcpy(c, b, n); c[n] = 0; int ok = 1;
	for (char *sv, *l = strtok_r(c, "\n", &sv); l && ok; l = strtok_r(NULL, "\n", &sv)) {
		char *t = strchr(l, '\t'); if (!t) continue; *t = 0;
		if (sd_keypath(l)) { snprintf(bad, bn, "%s", l); ok = 0; break; }
		size_t L = strlen(t + 1); unsigned char *d = malloc(L + 4); long dn = sd_b64dec(t + 1, L, d, L + 4);
		sadd(out, "%s\n", l); if (dn > 0) { if (out->n + dn + 2 > out->cap) { out->cap = out->n + dn + 2; out->b = realloc(out->b, out->cap); } memcpy(out->b + out->n, d, dn); out->n += dn; out->b[out->n++] = '\n'; out->b[out->n] = 0; }
		explicit_bzero(d, L + 4); free(d);
	}
	explicit_bzero(c, n); free(c); return ok;
}
/* a devlink command's JSON: a key file asked for (get, put), or named in a script, is refused */
static int guard_dl_cmd(const char *j, size_t n, char *bad, size_t bn)
{
	sd_buf v = {0}; int ok = 1;
	for (const char *key = "path"; key; key = !strcmp(key, "path") ? "src" : !strcmp(key, "src") ? "name" : NULL) {
		size_t at = sd_jget(j, n, 0, key); if (at && j[at] == '"') { v.n = 0; sd_jstr(j, n, at, &v); if (v.b && sd_keypath(v.b)) { snprintf(bad, bn, "%s", v.b); ok = 0; } } }
	size_t at = sd_jget(j, n, 0, "script");
	if (ok && at && j[at] == '"') { v.n = 0; sd_jstr(j, n, at, &v);
		for (char *sv, *w = v.b ? strtok_r(v.b, " \t\n;|&<>()'\"`$=", &sv) : NULL; w; w = strtok_r(NULL, " \t\n;|&<>()'\"`$=", &sv)) if (strchr(w, '/') || strchr(w, '.')) { if (sd_keypath(w)) { snprintf(bad, bn, "%s", w); ok = 0; break; } } }
	sd_wipe(&v); return ok;
}
