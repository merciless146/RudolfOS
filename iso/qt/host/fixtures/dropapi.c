/* 1.6.1 (docs/plan-1.6.1.md Part 13): rudolfd's desktop/dropped (rudolfd-drop.inc), compiled from rudolfd.c itself (main renamed),
   as deskapi.c. dropapi ROOT MAP HOMES RUNUSER MEDIA FLATDOC ACCOUNT PATH...: for each PATH, GET desktop/dropped?path=PATH with
   ACCOUNT signed in ("-": nobody); prints "STATUS|LENGTH|BODY" (the body's first 200 bytes) per line. ROOT: the accounts' folders,
   MAP: the account-users map (each account's uid), HOMES and RUNUSER: /home and /run/user, MEDIA: the drives, FLATDOC: the document
   portal's path inside a Flatpak sandbox. DROPAPI_RAW=FILE: the last answer's body written there.
   (served by native_rd.py for the native harness: dropapi ... -serve "METHOD what?query": the same line for one request) */
#define main rudolfd_main
#include "../../../sys/rudolfd.c"
#undef main
static char *g_esc(const char *s)
{
	char *o = malloc(strlen(s) * 3 + 1), *q = o;
	for (; *s; s++) { unsigned char c = (unsigned char)*s; if (isalnum(c) || strchr("/._-~", c)) *q++ = c; else q += sprintf(q, "%%%02X", c); }
	*q = 0; return o;
}
static void one(const char *path, const char *query)
{
	struct req r; memset(&r, 0, sizeof r); snprintf(r.method, sizeof r.method, "GET"); snprintf(r.path, sizeof r.path, "desktop/dropped");
	if (query) snprintf(r.query, sizeof r.query, "%s", query);
	else { char *e = g_esc(path); snprintf(r.query, sizeof r.query, "path=%s", e); free(e); }
	char tf[] = "/tmp/dropapi-XXXXXX"; r.fd = mkstemp(tf); unlink(tf);
	api_desktop_dropped(&r);
	lseek(r.fd, 0, SEEK_SET); char b[1 << 16]; ssize_t n; size_t tot = 0; char *all = NULL;
	while ((n = read(r.fd, b, sizeof b)) > 0) { all = realloc(all, tot + n + 1); memcpy(all + tot, b, n); tot += n; }
	close(r.fd); if (!all) { printf("none|0|\n"); return; } all[tot] = 0;
	char *e = strstr(all, "\r\n"), *bd = strstr(all, "\r\n\r\n"); size_t bl = bd ? tot - (bd + 4 - all) : 0;
	const char *raw = getenv("DROPAPI_RAW"); if (raw && bd) { FILE *f = fopen(raw, "wb"); if (f) { fwrite(bd + 4, 1, bl, f); fclose(f); } }
	printf("%.*s|%zu|", e ? (int)(e - all) : 0, all, bl);
	for (size_t i = 0; bd && i < bl && i < 200; i++) putchar(bd[4 + i] == '\n' ? ' ' : bd[4 + i]);
	putchar('\n'); free(all);
}
int main(int argc, char **argv)
{
	if (argc < 9) return 2;
	snprintf(home_root, sizeof home_root, "%s", argv[1]); snprintf(au_map, sizeof au_map, "%s", argv[2]);
	snprintf(au_homes, sizeof au_homes, "%s", argv[3]); snprintf(au_runuser, sizeof au_runuser, "%s", argv[4]);
	snprintf(drop_media, sizeof drop_media, "%s", argv[5]); snprintf(drop_flatdoc, sizeof drop_flatdoc, "%s", argv[6]);
	au_on = 1; mkdir(RUN, 0755);
	if (strcmp(argv[7], "-")) write_str(HOME_SESSION, argv[7]); else unlink(HOME_SESSION);
	for (int i = 8; i < argc; i++) {
		if (!strcmp(argv[i], "-serve") && i + 1 < argc) { const char *q = strchr(argv[++i], '?'); one(NULL, q ? q + 1 : ""); continue; }
		one(argv[i], NULL);
	}
	return 0;
}
