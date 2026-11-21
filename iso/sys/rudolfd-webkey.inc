/* (included by rudolfd.c, not built on its own: hence .inc) ---------- 1.6.1 (docs/plan-1.6.1.md Part 20;
 * docs/website-data-key.md): what keeps the shared browser profile's website data at this start, for Settings ›
 * Oracle › Website data. The session writes it (webkey_start, after rudolf-webkey unseal):
 *   GET webkey   {"ok":true,"state":S,"why":W,"t":T}: S tpm (under a key sealed in this computer's TPM; W says why it is a
 *                new one, if it is), none (no TPM 2.0: Chromium's built-in key), failed (the TPM would not give it at this
 *                start, W says why: Chromium's built-in key for this start), off (not started: no TPM key in this
 *                Rudolf OS, or the desktop is not one that uses the shared profile). Never the key. */
#ifndef WEBKEY_JSON
#define WEBKEY_JSON RUN "/webkey.json"
#endif
static void api_webkey(struct req *r)
{
	char *j = slurp(WEBKEY_JSON, NULL); str s = {0};
	if (j) { size_t n = strcspn(j, "\n"); j[n] = 0; }
	if (!j || j[0] != '{' || j[strlen(j) - 1] != '}' || !strstr(j, "\"state\":\"")) sadd(&s, "{\"ok\":true,\"state\":\"off\"}");
	else { sadd(&s, "{\"ok\":true,"); sadd(&s, "%s", j + 1); }
	free(j); reply_json(r->fd, &s);
}
