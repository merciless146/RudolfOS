/* 1.6.1: rudolfd's requests for File Explorer's transfers (fs/copy, fs/move, fs/hash, fs/write in pieces, fs/delete; dl/file
   to=copy), compiled from rudolfd.c itself (main renamed). Each argument is one request, "METHOD what?query[ <body-file]";
   the reply's status line and body are printed, one per request. Run inside a private mount namespace with a stand-in
   /media and /home/rudolf. */
#define main rudolfd_main
#include "../../../sys/rudolfd.c"
#undef main
int main(int argc, char **argv)
{
	for (int i = 1; i < argc; i++) {
		struct req r; memset(&r, 0, sizeof r);
		char sp[4096]; snprintf(sp, sizeof sp, "%s", argv[i]);
		char *body = strstr(sp, " <"); if (body) { *body = 0; body += 2; }
		char *w = strchr(sp, ' '); if (!w) continue; *w++ = 0; snprintf(r.method, sizeof r.method, "%s", sp);
		char *q = strchr(w, '?'); if (q) { *q = 0; snprintf(r.query, sizeof r.query, "%s", q + 1); }
		if (body) { FILE *f = fopen(body, "rb"); if (f) { r.body = malloc(64 << 20); r.blen = fread(r.body, 1, 64 << 20, f); fclose(f); } }
		char tf[] = "/tmp/fsxfer-XXXXXX"; r.fd = mkstemp(tf); unlink(tf);
		int post = !strcmp(r.method, "POST");
		dl_store_state();
		if (!strncmp(w, "fs/", 3)) api_fs(&r, w + 3); else if (!strncmp(w, "dl/", 3)) api_dl(&r, w + 3, post);
		lseek(r.fd, 0, SEEK_SET); static char b[1 << 20]; ssize_t n = read(r.fd, b, sizeof b - 1); close(r.fd); b[n > 0 ? n : 0] = 0;
		char *e = strstr(b, "\r\n"), *bd = strstr(b, "\r\n\r\n"); printf("%.*s|%s\n", e ? (int)(e - b) : 0, b, bd ? bd + 4 : "");
		free(r.body);
	}
	return 0;
}
