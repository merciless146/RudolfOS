/* 1.5.6: rudolfd's fs/* requests (api_fs), compiled from rudolfd.c itself (main renamed): each argument is one
   request, "what?query"; the reply's status line and body are printed, one per request. Run inside a private mount
   namespace with a stand-in /media. */
#define main rudolfd_main
#include "../../../sys/rudolfd.c"
#undef main
int main(int argc, char **argv)
{
	for (int i = 1; i < argc; i++) {
		struct req r; memset(&r, 0, sizeof r); snprintf(r.method, sizeof r.method, "GET");
		char sp[4096]; snprintf(sp, sizeof sp, "%s", argv[i]); char *q = strchr(sp, '?'); if (q) { *q = 0; snprintf(r.query, sizeof r.query, "%s", q + 1); }
		char tf[] = "/tmp/fsapi-XXXXXX"; r.fd = mkstemp(tf); unlink(tf);
		api_fs(&r, sp);
		lseek(r.fd, 0, SEEK_SET); static char b[1 << 20]; ssize_t n = read(r.fd, b, sizeof b - 1); close(r.fd); b[n > 0 ? n : 0] = 0;
		char *e = strstr(b, "\r\n"), *bd = strstr(b, "\r\n\r\n"); printf("%.*s|%s\n", e ? (int)(e - b) : 0, b, bd ? bd + 4 : "");
	}
	return 0;
}
