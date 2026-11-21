/* 1.6.1 (docs/plan-1.6.1.md Part 27 item 4): rudolfd's ai/know/... and home/* requests, compiled from rudolfd.c itself (main
   renamed), as homeapi.c; built with -DKNOW_BIN and -DKNOW_DIR (rudolf-know built here, its folder here). Each argument is one
   request, "METHOD what?query[ <body-file]" (what: home..., ai/know/... or ai/dev/...), its reply printed (status line|body); "SESSION ID"
   writes rudolfd's record of who is signed in ("-": nobody), "GONE ID" is what rudolfd does for an account removed.
   ROOT (the folder for accounts) is the first argument. Run as root inside a private mount namespace. */
#define main rudolfd_main
#include "../../../sys/rudolfd.c"
#undef main
static void one(const char *spec)
{
	struct req r; memset(&r, 0, sizeof r);
	char sp[8192]; snprintf(sp, sizeof sp, "%s", spec);
	if (!strncmp(sp, "SESSION ", 8)) { if (!strcmp(sp + 8, "-")) unlink(HOME_SESSION); else { char l[100]; snprintf(l, sizeof l, "%s\n", sp + 8); write_str(HOME_SESSION, l); } printf("SESSION %s\n", sp + 8); return; }
	if (!strncmp(sp, "GONE ", 5)) { know_acct_gone(sp + 5); printf("GONE %s\n", sp + 5); return; }
	char *body = strstr(sp, " <"); if (body) { *body = 0; body += 2; }
	char *sp2 = strchr(sp, ' '); if (!sp2) return; *sp2 = 0;
	snprintf(r.method, sizeof r.method, "%s", sp);
	char *q = strchr(sp2 + 1, '?'); if (q) { *q = 0; snprintf(r.query, sizeof r.query, "%s", q + 1); }
	snprintf(r.path, sizeof r.path, "%s", sp2 + 1);
	if (body) { FILE *f = fopen(body, "rb"); if (f) { r.body = malloc(1 << 20); r.blen = fread(r.body, 1, (1 << 20) - 1, f); r.body[r.blen] = 0; fclose(f); } }
	char tf[] = "/tmp/knowapi-XXXXXX"; r.fd = mkstemp(tf); unlink(tf);
	const char *w = r.path; int post = !strcmp(r.method, "POST");
	if (!strncmp(w, "ai/know/", 8)) api_ai_know(&r, w + 8);
	else if (!strncmp(w, "ai/dev/", 7)) api_ai_dev(&r, w + 7);   /* (blzdev161_t.sh: built with -DDEV_BIN, -DDEV_DIR, -DDEV_WAITF) */
	else if (!strcmp(w, "home")) api_home(&r, "", post); else api_home(&r, w + 5, post);
	lseek(r.fd, 0, SEEK_SET); char b[1 << 16]; ssize_t n; size_t tot = 0; char *all = NULL;
	while ((n = read(r.fd, b, sizeof b)) > 0) { all = realloc(all, tot + n + 1); memcpy(all + tot, b, n); tot += n; }
	close(r.fd); if (all) all[tot] = 0;
	char *st = all ? all : ""; char *e = strstr(st, "\r\n"); char *bd = strstr(st, "\r\n\r\n");
	printf("%.*s|%s\n", e ? (int)(e - st) : 0, st, bd ? bd + 4 : "");
	free(all); free(r.body);
}
int main(int argc, char **argv)
{
	snprintf(home_root, sizeof home_root, "%s", argv[1]); home_nobind = 1; home_uid = 1000;
	for (int i = 2; i < argc; i++) one(argv[i]);
	return 0;
}
