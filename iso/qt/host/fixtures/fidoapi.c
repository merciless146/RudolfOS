/* 1.6.1 (docs/plan-1.6.1.md Part 18): rudolfd's security keys (rudolfd-fido.inc) and the native sign-in with them, compiled
   from rudolfd.c itself with RUDOLFD_FIDO_TEST: libfido2 talks to software keys (vfido.py) on the Unix sockets listed in
   RUDOLFD_FIDO_SOCKS (each one a key plugged in) instead of /dev/hidraw*; the keys' records go to RUDOLFD_FIDO_DIR.
   fidoapi LSFILE DIR RUN "METHOD what?query[ <body-file]"...: each request's status line and body, one per line, as deskapi.c */
#define RUDOLFD_FIDO_TEST 1
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
	char tf[] = "/tmp/fidoapi-XXXXXX"; r.fd = mkstemp(tf); unlink(tf);
	int post = !strcmp(r.method, "POST");
	if (!strncmp(r.path, "keys", 4)) api_keys(&r, r.path + 4, post);
	else if (!api_desktop(&r, r.path, post)) reply_err(r.fd, 404, "not a desktop request");
	lseek(r.fd, 0, SEEK_SET); char b[1 << 16]; ssize_t n; size_t tot = 0; char *all = NULL;
	while ((n = read(r.fd, b, sizeof b)) > 0) { all = realloc(all, tot + n + 1); memcpy(all + tot, b, n); tot += n; }
	close(r.fd); if (all) all[tot] = 0;
	char *st = all ? all : ""; char *e = strstr(st, "\r\n"); char *bd = strstr(st, "\r\n\r\n");
	printf("%.*s|%s\n", e ? (int)(e - st) : 0, st, bd ? bd + 4 : "");
	free(all); free(r.body);
}
int main(int argc, char **argv)
{
	if (argc < 4) return 2;
	snprintf(desk_ls, sizeof desk_ls, "%s", argv[1]); snprintf(desk_dir, sizeof desk_dir, "%s", argv[2]); snprintf(desk_run, sizeof desk_run, "%s", argv[3]);
	const char *fd = getenv("RUDOLFD_FIDO_DIR"); if (fd) snprintf(fido_dir, sizeof fido_dir, "%s", fd);
	home_nobind = 1; home_uid = 1000; desk_uid = getuid();
	for (int i = 4; i < argc; i++) one(argv[i]);
	return 0;
}
