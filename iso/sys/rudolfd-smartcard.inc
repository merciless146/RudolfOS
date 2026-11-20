/* rudolfd-smartcard.inc: smart cards and the DoD Common Access Card (1.6.0, docs/plan-1.6.0.md Part 20).
 * GPL-2.0-or-later. Included by rudolfd.c. The work is /usr/lib/rudolf/smartcard's; the desktop (sc61.js) shows it.
 *   GET  sc/status          the readers, the cards, their certificates (read without a PIN): the script's JSON
 *   POST sc/dod/fetch       {"n","p"} (an administrator): the DoD's PKCS#7 bundle, as {"ok","url","hex"}: the desktop
 *                           checks it (the DoD's roots, every certificate chained to them) before anything is installed
 *   POST sc/dod/install     {"n","p","certs":"root HEX\nca HEX\n…"} (an administrator): the certificates the desktop
 *                           checked, into the desktop's certificate database (roots trusted for websites only)
 *   POST sc/dod/remove      {"n","p"} (an administrator): the DoD certificates removed
 *   GET  sc/dod/list        the DoD certificates installed, as {"ok","certs":[nickname…]}
 * The PIN never comes here: Chromium's own box asks the card for it. */
#define SC_SH "/usr/lib/rudolf/smartcard"
static int sc_hex(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
static int api_sc(struct req *r, const char *p, int post)
{
	if (!strcmp(p, "sc/status")) {
		char *o = run_out(SC_SH " status 2>/dev/null"); if (!o || o[0] != '{') { free(o); reply_err(r->fd, 500, "the smart card service did not answer"); return 1; }
		str s = {0}; sadd(&s, "%s", o); free(o); reply_json(r->fd, &s); return 1;
	}
	if (!strcmp(p, "sc/dod/list")) {
		char *o = run_out(SC_SH " dod-list 2>/dev/null"); str s = {0}; sadd(&s, "{\"ok\":true,\"certs\":["); int n = 0;
		for (char *l = o ? strtok(o, "\n") : NULL; l; l = strtok(NULL, "\n")) if (*l) { sadd(&s, "%s", n++ ? "," : ""); sjson(&s, l); }
		sadd(&s, "]}"); free(o); reply_json(r->fd, &s); return 1;
	}
	if (!strcmp(p, "sc/dod/fetch") || !strcmp(p, "sc/dod/install") || !strcmp(p, "sc/dod/remove")) {
		if (!post) { reply_err(r->fd, 405, "POST"); return 1; }
		char *an = mobile_admin(r); if (!an) return 1;
		if (!strcmp(p, "sc/dod/fetch")) {
			char f[] = RUN "/sc-dod.p7b"; unlink(f);
			char *o = run_out(SC_SH " dod-fetch " RUN "/sc-dod.p7b 2>&1"); trim(o);
			size_t n = 0; char *b = slurp(f, &n); unlink(f);
			if (!b || !n || n > 4 * 1024 * 1024) { reply_err(r->fd, 502, o && *o ? o : "the DoD's bundle could not be downloaded"); free(o); free(b); free(an); return 1; }
			str s = {0}; sadd(&s, "{\"ok\":true,\"url\":"); sjson(&s, o ? o : ""); sadd(&s, ",\"hex\":\"");
			for (size_t i = 0; i < n; i++) sadd(&s, "%02x", (unsigned char)b[i]);
			sadd(&s, "\"}"); free(o); free(b); free(an); reply_json(r->fd, &s); return 1;
		}
		if (!strcmp(p, "sc/dod/remove")) {
			char *o = run_out(SC_SH " dod-remove 2>&1"); free(o);
			health_add("security", "the DoD's certificates were removed from Oracle by %.80s", an); free(an); reply_ok(r->fd, "removed"); return 1;
		}
		char *c = dbody_field(r, "certs"); if (!c || strlen(c) > 8 * 1024 * 1024) { free(c); free(an); reply_err(r->fd, 400, "certs?"); return 1; }
		char d[] = RUN "/sc-dodXXXXXX"; if (!mkdtemp(d)) { free(c); free(an); reply_err(r->fd, 500, "no room"); return 1; }
		int k = 0, bad = 0, nr = 0;
		for (char *l = strtok(c, "\n"); l && k < 200; l = strtok(NULL, "\n")) {
			int root = !strncmp(l, "root ", 5); if (!root && strncmp(l, "ca ", 3)) { bad = 1; break; }
			const char *h = l + (root ? 5 : 3); size_t hl = strlen(h); if (!hl || hl % 2 || hl > 64 * 1024) { bad = 1; break; }
			char fn[200]; snprintf(fn, sizeof fn, "%s/%s-%d.der", d, root ? "root" : "ca", k++); FILE *f = fopen(fn, "wb"); if (!f) { bad = 1; break; }
			for (size_t i = 0; i < hl; i += 2) { int a = sc_hex(h[i]), b2 = sc_hex(h[i + 1]); if (a < 0 || b2 < 0) { bad = 1; break; } fputc(a * 16 + b2, f); }
			fclose(f); if (bad) break; nr += root;
		}
		free(c);
		if (bad || !k || !nr) { char rm[300]; snprintf(rm, sizeof rm, "rm -rf %s", d); run(rm); free(an); reply_err(r->fd, 400, "certificates as \"root HEX\" or \"ca HEX\" lines, a root among them"); return 1; }
		char cmd[300]; snprintf(cmd, sizeof cmd, SC_SH " dod-install %s 2>&1", d); char *o = run_out(cmd); trim(o);
		snprintf(cmd, sizeof cmd, "rm -rf %s", d); run(cmd);
		health_add("security", "the DoD's certificates (%d) were added to Oracle by %.80s", k, an);
		str s = {0}; sadd(&s, "{\"ok\":true,\"installed\":%d}", o ? atoi(o) : 0); free(o); free(an); reply_json(r->fd, &s); return 1;
	}
	return 0;
}
