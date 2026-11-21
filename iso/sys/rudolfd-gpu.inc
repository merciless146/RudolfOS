/* (included by rudolfd.c, not built on its own: hence .inc) ---------- 1.6.1 (docs/plan-1.6.1.md Part 19): Settings ›
 * Display › Graphics driver, through /usr/lib/rudolf/gpudriver (root):
 *   GET  gpu/driver                    the card, whether NVIDIA's driver is offered for it (GeForce 16 and newer, by PCI
 *                                      ID), its extension, the choice, what is in use, the last switch back and why
 *   POST gpu/driver/set?to=open|nvidia the choice for the next start: only for an administrator whose password rudolfd
 *                                      checks itself (the body {"n":ADMIN,"p":PASSWORD}), as the system extensions
 *   POST gpu/driver/ack                the switch-back's note was shown (the desktop's notification): not shown again */
#ifndef GPU_SH
#define GPU_SH "/usr/lib/rudolf/gpudriver"
#endif
static void api_gpudriver(struct req *r, const char *what, int post)
{
	if (access(GPU_SH, X_OK)) { reply_err(r->fd, 404, "this Rudolf OS has no graphics driver choice"); return; }
	char *o = NULL;
	if (!*what) o = run_out(GPU_SH " status 2>/dev/null");
	else if (!post) { reply_err(r->fd, 405, "use POST"); return; }
	else if (!strcmp(what, "ack")) o = run_out(GPU_SH " ack 2>/dev/null");
	else if (!strcmp(what, "set")) {
		char *to = qparam(r->query, "to");
		if (!to || (strcmp(to, "open") && strcmp(to, "nvidia"))) { reply_err(r->fd, 400, "to=open or to=nvidia"); return; }
		char *an = NULL, *ap = NULL, why[200] = "";
		if (r->body && r->blen && r->body[0] == '{') { an = dbody_field(r, "n"); ap = dbody_field(r, "p"); }
		int adm = an && ap ? desk_admin_check(an, ap, why, sizeof why) : -1;
		dfree_secret(ap);
		if (adm) { free(an); reply_err(r->fd, adm == 429 ? 429 : 403, why[0] ? why : "an administrator's name and password are needed"); return; }
		health_add("hardware", "graphics driver for the next start: %s (asked by %.60s)", !strcmp(to, "nvidia") ? "NVIDIA's driver" : "the open driver (NVK)", an);
		free(an);
		o = run_out(!strcmp(to, "nvidia") ? GPU_SH " set nvidia 2>/dev/null" : GPU_SH " set open 2>/dev/null");
	} else { reply_err(r->fd, 404, "unknown graphics driver action"); return; }
	if (!o || o[0] != '{') { free(o); reply_err(r->fd, 500, "gpudriver failed"); return; }
	reply(r->fd, 200, "application/json", o, strlen(o), NULL); free(o);
}
