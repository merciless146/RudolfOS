/* 1.5.9 (plan 15.4): rudolfd's devlink requests, compiled from rudolfd.c itself (main renamed), as deskapi.c.
   Built with -DDLBIN='"FAKE"' (a stand-in rudolf-devlink).
   devlinkapi LSFILE "METHOD what?query[ <body-file]"... : each request's status line and body, one per line;
   devlinkapi LSFILE boot : what rudolfd does with the link at start-up (dl_boot) */
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
	if (body) { FILE *f = fopen(body, "rb"); if (f) { r.body = malloc(1 << 20); r.blen = fread(r.body, 1, (1 << 20) - 1, f); r.body[r.blen] = 0; fclose(f); } }
	char tf[] = "/tmp/devlinkapi-XXXXXX"; r.fd = mkstemp(tf); unlink(tf);
	const char *w = r.path; if (!strncmp(w, "devlink", 7)) w += 7; if (*w == '/') w++;
	api_devlink(&r, w, !strcmp(r.method, "POST"));
	lseek(r.fd, 0, SEEK_SET); char b[1 << 16]; ssize_t n; size_t tot = 0; char *all = NULL;
	while ((n = read(r.fd, b, sizeof b)) > 0) { all = realloc(all, tot + n + 1); memcpy(all + tot, b, n); tot += n; }
	close(r.fd); if (all) all[tot] = 0;
	char *st = all ? all : ""; char *e = strstr(st, "\r\n"); char *bd = strstr(st, "\r\n\r\n");
	printf("%.*s|%s\n", e ? (int)(e - st) : 0, st, bd ? bd + 4 : "");
	free(all); free(r.body);
}
int main(int argc, char **argv)
{
	if (argc < 3) return 2;
	snprintf(desk_ls, sizeof desk_ls, "%s", argv[1]);
	if (!strcmp(argv[2], "boot")) { dl_boot(); return 0; }
	for (int i = 2; i < argc; i++) one(argv[i]);
	return 0;
}
