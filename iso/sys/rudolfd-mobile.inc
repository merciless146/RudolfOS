/* Rudolf OS 1.6.0 (docs/plan-1.6.0.md Parts 1 to 6): cellular modems, rudolfd's mobile requests. ModemManager's
   D-Bus API is read and driven through /usr/lib/rudolf/modem (mmcli); rudolfd checks what is asked and who asks.
     GET  mobile/status                each modem (maker, model, state, SIM, operator, signal, network type, the FCC
                                       unlock), and the modules Linux supports only partly (notes)
     POST mobile/fcc?id=VVVV[:PPPP]&on=1|0   the FCC unlock of one laptop module, turned on or off: an administrator's
                                       name and password in the body ({"n":ADMIN,"p":PASSWORD}), checked here
   (MODEM_SH moves the script for the host tests: qt/host/fixtures/mobileapi.c.) */
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
	return 0;
}
