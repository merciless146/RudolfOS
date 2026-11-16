/* Rudolf OS 1.6.0 (docs/plan-1.6.0.md Parts 1 to 6): cellular modems, rudolfd's mobile requests. ModemManager's
   D-Bus API is read and driven through /usr/lib/rudolf/modem (mmcli); rudolfd checks what is asked and who asks.
     GET  mobile/status                each modem (maker, model, state, SIM, operator, signal, network type, the FCC
                                       unlock), and the modules Linux supports only partly (notes)
     POST mobile/fcc?id=VVVV[:PPPP]&on=1|0   the FCC unlock of one laptop module, turned on or off: an administrator's
                                       name and password in the body ({"n":ADMIN,"p":PASSWORD}), checked here
   (Part 2)
     POST mobile/pin?id=N              the SIM's PIN: the body {"n":ADMIN,"p":PASSWORD,"pin":"1234"}
     POST mobile/puk?id=N              its PUK and a new PIN: {"n","p","puk":"12345678","pin":"1234"}
     GET  mobile/settings?id=N         the SIM's operator, the automatic APN, the one typed in, roaming, the network type
     POST mobile/settings?id=N&k=apn|apnuser|apnpass|roaming|mode|autoconnect|limit|startday|dataoff&v=VALUE
                                       (Part 4: the settings' answer has "usage": this month's data, the limit)
     GET  mobile/airplane, POST mobile/airplane?on=1|0   Wi-Fi, Bluetooth and the modems off together, and back
   (Part 3)
     POST mobile/connect?id=N&on=1|0   connect (and connect automatically again) or disconnect (until Connect)
     GET  mobile/notes?since=N         the cellular connection's notices (mobile data in use, back on Wi-Fi), for both
                                       desktops: {"ok":true,"last":N,"notes":[{"n","t","b"}]}
   (MODEM_SH moves the script for the host tests: qt/host/fixtures/mobileapi.c.) */
#ifndef MOBILE_RUN
#define MOBILE_RUN RUN
#endif
#ifndef MODEM_SH
#define MODEM_SH "/usr/lib/rudolf/modem"
#endif
/* a module's id as ModemManager's FCC unlock scripts are named: vvvv or vvvv:pppp (hex) */
static int mobile_devid_ok(const char *s)
{
	if (!s) return 0; size_t n = strlen(s); if (n != 4 && n != 9) return 0;
	for (size_t i = 0; i < n; i++) { if (i == 4) { if (s[i] != ':') return 0; continue; } if (!isxdigit((unsigned char)s[i]) || isupper((unsigned char)s[i])) return 0; }
	return 1;
}
/* the administrator's name (malloc'd) when the body names one whose password is right, else NULL with the answer sent */
static char *mobile_admin(struct req *r)
{
	char *an = NULL, *ap = NULL, why[200] = "";
	if (r->body && r->blen && r->body[0] == '{') { an = dbody_field(r, "n"); ap = dbody_field(r, "p"); }
	int adm = an && ap ? desk_admin_check(an, ap, why, sizeof why) : -1;
	dfree_secret(ap);
	if (adm) { free(an); reply_err(r->fd, adm == 429 ? 429 : 403, why[0] ? why : "an administrator's name and password are needed"); return NULL; }
	return an;
}
/* the SIM's codes go to the script in a root-only file, never on a command line */
static int mobile_codes(const char *pin, const char *puk)
{
	const char *f = RUN "/mobile-codes"; unlink(f);
	int fd = open(f, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600); if (fd < 0) return -1;
	int ok = dprintf(fd, "pin=%s\n", pin ? pin : "") > 0 && (!puk || dprintf(fd, "puk=%s\n", puk) > 0); close(fd);
	return ok ? 0 : -1;
}
static int mobile_digits(const char *s, size_t lo, size_t hi) { if (!s) return 0; size_t n = strlen(s); if (n < lo || n > hi) return 0; for (size_t i = 0; i < n; i++) if (!isdigit((unsigned char)s[i])) return 0; return 1; }
/* a setting's value: no quote, backslash, comma or control character (the script gets it in single quotes) */
static int mobile_value_ok(const char *v) { if (!v || strlen(v) > 100) return 0; for (const unsigned char *c = (const unsigned char *)v; *c; c++) if (*c < 32 || *c == 127 || *c == '\'' || *c == '"' || *c == '\\' || *c == ',' || *c == '`' || *c == '$') return 0; return 1; }
static int api_mobile2(struct req *r, const char *p, int post)
{
	const char *id = qparam(r->query, "id");
	if (!strcmp(p, "mobile/pin") || !strcmp(p, "mobile/puk")) {
		int ispuk = p[7] == 'p' && p[8] == 'u';
		if (!post) { reply_err(r->fd, 405, "POST"); return 1; }
		if (!mobile_digits(id, 1, 4)) { reply_err(r->fd, 400, "id=MODEM"); return 1; }
		char *pin = r->body && r->blen && r->body[0] == '{' ? dbody_field(r, "pin") : NULL, *puk = ispuk && r->body && r->blen && r->body[0] == '{' ? dbody_field(r, "puk") : NULL;
		if (!mobile_digits(pin, 4, 8) || (ispuk && !mobile_digits(puk, 8, 8))) { dfree_secret(pin); dfree_secret(puk); reply_err(r->fd, 400, ispuk ? "A PUK is 8 digits, and the new PIN 4 to 8 digits." : "A PIN is 4 to 8 digits."); return 1; }
		char *an = mobile_admin(r); if (!an) { dfree_secret(pin); dfree_secret(puk); return 1; }
		int w = mobile_codes(pin, puk); dfree_secret(pin); dfree_secret(puk);
		if (w) { free(an); reply_err(r->fd, 500, "could not hand the code to the modem service"); return 1; }
		char cmd[200]; snprintf(cmd, sizeof cmd, MODEM_SH " %s %s " RUN "/mobile-codes 2>&1", ispuk ? "puk" : "pin", id);
		char *o = run_out(cmd); trim(o); unlink(RUN "/mobile-codes");
		int good = o && (strstr(o, "PIN accepted") || strstr(o, "PUK accepted"));
		if (good) health_add("modem", "the SIM's %s was given by %.80s: unlocked", ispuk ? "PUK (and a new PIN)" : "PIN", an);
		else if (o && strstr(o, "blocked for good")) health_add("modem", "the SIM is blocked for good: a wrong PUK the last time (given by %.80s)", an);
		else if (o && strstr(o, "now blocked")) health_add("modem", "the SIM's PIN is blocked: three wrong PINs (the last given by %.80s); its PUK is needed", an);
		else if (o && strstr(o, "Wrong")) health_add("modem", "a wrong %s for the SIM, given by %.80s", ispuk ? "PUK" : "PIN", an);
		if (good) reply_ok(r->fd, o); else reply_err(r->fd, 400, o && *o ? o : "the modem service did not answer");
		free(o); free(an); return 1;
	}
	if (!strcmp(p, "mobile/settings")) {
		if (!mobile_digits(id, 1, 4)) { reply_err(r->fd, 400, "id=MODEM"); return 1; }
		char cmd[300];
		if (!post) { snprintf(cmd, sizeof cmd, MODEM_SH " settings %s 2>/dev/null", id); char *o = run_out(cmd); trim(o);
			if (!o || o[0] != '{' || strstr(o, "\"error\"")) { free(o); reply_err(r->fd, 404, "there is no such modem"); return 1; }
			str js = {0}; sadd(&js, "{\"ok\":true,"); sadd(&js, "%s", o + 1); free(o); reply_json(r->fd, &js); return 1; }
		const char *k = qparam(r->query, "k"), *v = qparam(r->query, "v");
		if (!k || (strcmp(k, "apn") && strcmp(k, "apnuser") && strcmp(k, "apnpass") && strcmp(k, "roaming") && strcmp(k, "mode") && strcmp(k, "autoconnect") && strcmp(k, "limit") && strcmp(k, "startday") && strcmp(k, "dataoff")) || !mobile_value_ok(v)) { reply_err(r->fd, 400, "k=apn|apnuser|apnpass|roaming|mode|autoconnect|limit|startday|dataoff&v=VALUE (no quotes, commas or backslashes)"); return 1; }
		snprintf(cmd, sizeof cmd, MODEM_SH " set %s %s '%s' 2>&1", id, k, v); char *o = run_out(cmd); trim(o);
		if (o && !strcmp(o, "saved")) { if (!strcmp(k, "roaming") && *v == '1') health_add("modem", "mobile data when roaming was turned on"); reply_ok(r->fd, o); }
		else reply_err(r->fd, 400, o && *o ? o : "the setting was not saved");
		free(o); return 1;
	}
	if (!strcmp(p, "mobile/connect")) {
		if (!post) { reply_err(r->fd, 405, "POST"); return 1; }
		const char *on = qparam(r->query, "on");
		if (!mobile_digits(id, 1, 4) || !on || (strcmp(on, "1") && strcmp(on, "0"))) { reply_err(r->fd, 400, "id=MODEM&on=1|0"); return 1; }
		char cmd[160]; snprintf(cmd, sizeof cmd, MODEM_SH " %s %s 2>&1", *on == '1' ? "connect" : "disconnect", id);
		char *o = run_out(cmd); trim(o);
		if (o && (!strcmp(o, "Connected") || !strcmp(o, "Disconnected"))) reply_ok(r->fd, o); else reply_err(r->fd, 400, o && *o ? o : "the modem service did not answer");
		free(o); return 1;
	}
	if (!strcmp(p, "mobile/notes")) {
		long since = 0; const char *sn = qparam(r->query, "since"); if (sn) since = atol(sn);
		char *t = slurp(MOBILE_RUN "/cell-notes", NULL); str js = {0}; long last = 0; int first = 1;
		sadd(&js, "{\"ok\":true,\"notes\":[");
		for (char *l = t, *e; l && *l; l = e ? e + 1 : NULL) { e = strchr(l, '\n'); if (e) *e = 0;
			char *b1 = strchr(l, '|'), *b2 = b1 ? strchr(b1 + 1, '|') : NULL; if (!b2) { if (!e) break; continue; }
			*b1 = 0; *b2 = 0; long n = atol(l); if (n > last) last = n;
			if (n > since) { sadd(&js, "%s{\"n\":%ld,\"t\":", first ? "" : ",", n); sjson(&js, b1 + 1); sadd(&js, ",\"b\":"); sjson(&js, b2 + 1); sadd(&js, "}"); first = 0; }
			if (!e) break; }
		sadd(&js, "],\"last\":%ld}", last); free(t); reply_json(r->fd, &js); return 1;
	}
	if (!strcmp(p, "mobile/airplane")) {
		if (!post) { char *o = run_out(MODEM_SH " airplane status 2>/dev/null"); trim(o); str js = {0}; sadd(&js, "{\"ok\":true,\"on\":%s}", o && strstr(o, "true") ? "true" : "false"); free(o); reply_json(r->fd, &js); return 1; }
		const char *on = qparam(r->query, "on"); if (!on || (strcmp(on, "1") && strcmp(on, "0"))) { reply_err(r->fd, 400, "on=1|0"); return 1; }
		char *o = run_out(*on == '1' ? MODEM_SH " airplane on 2>&1" : MODEM_SH " airplane off 2>&1"); trim(o);
		reply_ok(r->fd, o && *o ? o : "done"); free(o); return 1;
	}
	return 0;
}
static int api_mobile(struct req *r, const char *p, int post)
{
	if (!strcmp(p, "mobile/status")) {
		char *o = run_out(MODEM_SH " status 2>/dev/null"); trim(o);
		if (!o || o[0] != '{') { free(o); str e = {0}; sadd(&e, "{\"ok\":true,\"running\":false,\"modems\":[],\"notes\":[]}"); reply_json(r->fd, &e); return 1; }
		str js = {0}; sadd(&js, "{\"ok\":true,"); sadd(&js, "%s", o + 1); free(o); reply_json(r->fd, &js); return 1;
	}
	if (!strcmp(p, "mobile/fcc")) {
		if (!post) { reply_err(r->fd, 405, "POST"); return 1; }
		const char *id = qparam(r->query, "id"), *on = qparam(r->query, "on");
		if (!mobile_devid_ok(id) || !on || (strcmp(on, "1") && strcmp(on, "0"))) { reply_err(r->fd, 400, "id=VVVV:PPPP&on=1|0"); return 1; }
		char *an = mobile_admin(r); if (!an) return 1;
		char cmd[160]; snprintf(cmd, sizeof cmd, MODEM_SH " fcc %s %s 2>&1", *on == '1' ? "on" : "off", id);
		char *o = run_out(cmd); trim(o);
		if (!o || (strncmp(o, "The cellular module", 19) && strncmp(o, "The FCC unlock is off", 21))) { reply_err(r->fd, 400, o && *o ? o : "the FCC unlock could not be changed"); free(o); free(an); return 1; }
		health_add("hardware", "the FCC unlock of the cellular module %s was turned %s by %.80s", id, *on == '1' ? "on" : "off", an);
		reply_ok(r->fd, o); free(o); free(an); return 1;
	}
	return api_mobile2(r, p, post);
}
