/* 1.5.9 (plan 16.1): rudolfd's tor/* requests and its loop's Tor check, compiled from rudolfd.c itself (main renamed), as
   deskapi.c. torapi "METHOD tor/what?query" | tick ...: each request's status line and body, one per line; "tick" runs
   rudolfd's loop's tor_tick once. Built with -DTOR_BIN="..." -DTOR_USER="..." (a stand-in Tor, or the real one). */
#define main rudolfd_main
#include "../../../sys/rudolfd.c"
#undef main
static void one(const char *spec)
{
	if (!strcmp(spec, "tick")) { tor_tick(); puts("tick"); return; }
	struct req r; memset(&r, 0, sizeof r);
	char sp[4096]; snprintf(sp, sizeof sp, "%s", spec);
	char *sp2 = strchr(sp, ' '); if (!sp2) return; *sp2 = 0;
	snprintf(r.method, sizeof r.method, "%s", sp);
	char *q = strchr(sp2 + 1, '?'); if (q) { *q = 0; snprintf(r.query, sizeof r.query, "%s", q + 1); }
	snprintf(r.path, sizeof r.path, "%s", sp2 + 1);
	char tf[] = "/tmp/torapi-XXXXXX"; r.fd = mkstemp(tf); unlink(tf);
	if (strncmp(r.path, "tor/", 4)) reply_err(r.fd, 404, "not a Tor request"); else api_tor(&r, r.path + 4, !strcmp(r.method, "POST"));
	lseek(r.fd, 0, SEEK_SET); char b[1 << 16]; ssize_t n = read(r.fd, b, sizeof b - 1); close(r.fd); b[n > 0 ? n : 0] = 0;
	char *e = strstr(b, "\r\n"), *bd = strstr(b, "\r\n\r\n");
	printf("%.*s|%s\n", e ? (int)(e - b) : 0, b, bd ? bd + 4 : "");
}
int main(int argc, char **argv) { for (int i = 1; i < argc; i++) one(argv[i]); return 0; }
