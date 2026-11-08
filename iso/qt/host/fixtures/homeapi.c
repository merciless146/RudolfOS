/* 1.5.6: rudolfd's home/* requests, compiled from rudolfd.c itself (main renamed). Each argument is one request,
   "METHOD what?query[ <body-file]"; the reply (status line and body) is printed, one per request.
   ROOT (the folder for accounts) is the first argument. Run as root inside a private mount namespace. */
#define main rudolfd_main
#include "../../../sys/rudolfd.c"
#undef main
static void one(const char *spec)
{
	struct req r; memset(&r, 0, sizeof r);
	char sp[8192]; snprintf(sp, sizeof sp, "%s", spec);
	char *body = strstr(sp, " <"); if (body) { *body = 0; body += 2; }
	char *sp2 = strchr(sp, ' '); if (!sp2) return; *sp2 = 0;
	snprintf(r.method, sizeof r.method, "%s", sp);
	char *q = strchr(sp2 + 1, '?'); if (q) { *q = 0; snprintf(r.query, sizeof r.query, "%s", q + 1); }
	snprintf(r.path, sizeof r.path, "%s", sp2 + 1);
	if (body) { FILE *f = fopen(body, "rb"); if (f) { r.body = malloc(64 << 20); r.blen = fread(r.body, 1, 64 << 20, f); fclose(f); } }
	char tf[] = "/tmp/homeapi-XXXXXX"; r.fd = mkstemp(tf); unlink(tf);
	const char *w = r.path; int post = !strcmp(r.method, "POST");
	if (!strcmp(w, "home")) api_home(&r, "", post); else api_home(&r, w + 5, post);
	lseek(r.fd, 0, SEEK_SET); char b[1 << 16]; ssize_t n; size_t tot = 0; char *all = NULL;
	while ((n = read(r.fd, b, sizeof b)) > 0) { all = realloc(all, tot + n + 1); memcpy(all + tot, b, n); tot += n; }
	close(r.fd); if (all) all[tot] = 0;
	char *st = all ? all : ""; char *e = strstr(st, "\r\n"); char *bd = strstr(st, "\r\n\r\n");
	printf("%.*s|%s\n", e ? (int)(e - st) : 0, st, bd ? bd + 4 : "");
	free(all); free(r.body);
}
int main(int argc, char **argv)
{
	snprintf(home_root, sizeof home_root, "%s", argv[1]); home_nobind = getenv("BIND") ? 0 : 1; home_uid = 1000;
	for (int i = 2; i < argc; i++) one(argv[i]);
	return 0;
}
