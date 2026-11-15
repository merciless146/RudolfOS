/* 1.5.9 (plan 16.2): rudolfd's torwin/* requests (Standard's Tor windows), compiled from rudolfd.c itself (main renamed), as
   deskapi.c. torwinapi "METHOD torwin/what?query" ...: each request's status line and body, one per line. Built with
   -DTORWIN_USER="..." -DTORWIN_LAUNCH="..." (the host tests). */
#define main rudolfd_main
#include "../../../sys/rudolfd.c"
#undef main
static void one(const char *spec)
{
	struct req r; memset(&r, 0, sizeof r);
	char sp[4096]; snprintf(sp, sizeof sp, "%s", spec);
	char *sp2 = strchr(sp, ' '); if (!sp2) return; *sp2 = 0;
	snprintf(r.method, sizeof r.method, "%s", sp);
	char *q = strchr(sp2 + 1, '?'); if (q) { *q = 0; snprintf(r.query, sizeof r.query, "%s", q + 1); }
	snprintf(r.path, sizeof r.path, "%s", sp2 + 1);
	char tf[] = "/tmp/torwinapi-XXXXXX"; r.fd = mkstemp(tf); unlink(tf);
	if (strncmp(r.path, "torwin/", 7)) reply_err(r.fd, 404, "not a Tor window request"); else api_torwin(&r, r.path + 7, !strcmp(r.method, "POST"));
	lseek(r.fd, 0, SEEK_SET); char b[1 << 16]; ssize_t n = read(r.fd, b, sizeof b - 1); close(r.fd); b[n > 0 ? n : 0] = 0;
	char *e = strstr(b, "\r\n"), *bd = strstr(b, "\r\n\r\n");
	printf("%.*s|%s\n", e ? (int)(e - b) : 0, b, bd ? bd + 4 : "");
}
int main(int argc, char **argv) { for (int i = 1; i < argc; i++) one(argv[i]); return 0; }
