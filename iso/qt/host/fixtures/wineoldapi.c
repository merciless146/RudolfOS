/* 1.6.2 (docs/plan-1.6.2.md Part 40): rudolfd's wine/old, wine/bring and wine/decline (rudolfd-wineold.inc), compiled from
   rudolfd.c itself (main renamed), as usersapi.c. wineoldapi LSFILE ACCOUNTS MAP OLDHOME STATE COMMAND...:
     get [DIR]            GET wine/old (?dir=DIR): the reply's body
     bring [NAME PASS]    POST wine/bring (with an administrator's name and password): the status line, then the body
     decline              POST wine/decline
     wait                 until the copy has ended (at most 120 s): its last progress
     answer ID            POST portal/answer?id=ID (body {}): the status line; whether the request and an answer are there
   The signed-in account: RUN/home-session and RUN/acct-session.env, written by the test. WO_SLOW=MICROSECONDS: a pause per
   file of the copy (so the test can cut one off). Run as root in a private mount namespace. */
#define main rudolfd_main
#include "../../../sys/rudolfd.c"
#undef main
static void show(struct req *r, int head)
{
	lseek(r->fd, 0, SEEK_SET); static char b[262144]; ssize_t n = read(r->fd, b, sizeof b - 1); b[n > 0 ? n : 0] = 0; close(r->fd);
	char *bd = strstr(b, "\r\n\r\n"); if (head) { char *e = strstr(b, "\r\n"); if (e) printf("%.*s\n", (int)(e - b), b); }
	printf("%s\n", bd ? bd + 4 : b); fflush(stdout);
}
static void mkreq(struct req *r, const char *q, const char *body)
{
	memset(r, 0, sizeof *r); char tf[] = "/tmp/wineoldapi-XXXXXX"; r->fd = mkstemp(tf); unlink(tf);
	snprintf(r->query, sizeof r->query, "%s", q ? q : ""); if (body) { r->body = strdup(body); r->blen = strlen(body); }
}
int main(int argc, char **argv)
{
	if (argc < 6) return 2;
	snprintf(desk_ls, sizeof desk_ls, "%s", argv[1]); snprintf(home_root, sizeof home_root, "%s", argv[2]);
	snprintf(au_map, sizeof au_map, "%s", argv[3]); snprintf(wo_oldhome, sizeof wo_oldhome, "%s", argv[4]); snprintf(wo_state, sizeof wo_state, "%s", argv[5]);
	au_on = 1; mkdir(RUN, 0755); if (getenv("WO_SLOW")) wo_slow_us = atoi(getenv("WO_SLOW"));
	for (int i = 6; i < argc; i++) {
		const char *c = argv[i]; struct req r;
		if (!strcmp(c, "get")) { char q[1200] = ""; if (i + 1 < argc && argv[i + 1][0] == '/') { str e = {0}; sadd(&e, "dir="); for (const char *p = argv[++i]; *p; p++) sadd(&e, isalnum((unsigned char)*p) || strchr("/._-", *p) ? "%c" : "%%%02X", (unsigned char)*p); snprintf(q, sizeof q, "%s", e.b); free(e.b); }
			mkreq(&r, q, NULL); api_wineold_old(&r, 0); show(&r, 0); }
		else if (!strcmp(c, "bring")) { char b[300] = ""; if (i + 2 < argc && strcmp(argv[i + 1], "-")) { snprintf(b, sizeof b, "{\"n\":\"%s\",\"p\":\"%s\"}", argv[i + 1], argv[i + 2]); i += 2; } else if (i + 1 < argc && !strcmp(argv[i + 1], "-")) i++;
			mkreq(&r, NULL, *b ? b : "{}"); api_wineold_bring(&r, 1); show(&r, 1); }
		else if (!strcmp(c, "decline")) { mkreq(&r, NULL, "{}"); api_wineold_decline(&r, 1); show(&r, 1); }
		else if (!strcmp(c, "wait")) { for (int k = 0; k < 1200; k++) { usleep(100000); char *j = slurp(WO_PROG, NULL); int end = j && !strstr(j, "\"running\":true") && !wo_running(); free(j); if (end) break; }
			char *j = slurp(WO_PROG, NULL); printf("%s", j ? j : "none\n"); free(j); fflush(stdout); }
		else if (!strcmp(c, "answer") && i + 1 < argc) { char q[100]; snprintf(q, sizeof q, "id=%s", argv[++i]); mkreq(&r, q, "{}"); snprintf(r.method, sizeof r.method, "POST"); api_portal(&r, "answer"); show(&r, 1); }
	}
	return 0;
}
