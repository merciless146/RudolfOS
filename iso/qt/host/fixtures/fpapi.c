/* 1.6.3 (docs/plan-1.6.3.md Part 2): rudolfd's fingerprint requests (rudolfd-fprint.inc), compiled from rudolfd.c itself
   with RD_ROOT_DIR (the fprint helper there, built with -DRR_TEST and its places in its configuration) and AU_PROC (a
   stand-in /proc: no process of the computer is ever stopped by the removal of an account here).
   fpapi LSFILE DESKDIR RUN ACCOUNTS FPSTATE FPRUN AUETC AUMAP "METHOD what?query[ <body-file]"...: each request's status
   line and body, one per line, as fidoapi.c; "gone ID ID..." (one argument) the accounts that stay: au_sync_ids as
   rudolfd runs it after the page's storage changes. Requests: fp/..., home/... (api_home), else desktop/... */
#define main rudolfd_main
#include "../../../sys/rudolfd.c"
#undef main
static void one(const char *spec)
{
	if (!strncmp(spec, "gone", 4)) {
		char ids[AU_MAX][80]; int n = 0; char b[4096]; snprintf(b, sizeof b, "%s", spec + 4);
		for (char *t = strtok(b, " "); t && n < AU_MAX; t = strtok(NULL, " ")) snprintf(ids[n++], 80, "%s", t);
		printf("gone %d\n", au_sync_ids(ids, n)); return;
	}
	struct req r; memset(&r, 0, sizeof r);
	char sp[8192]; snprintf(sp, sizeof sp, "%s", spec);
	char *body = strstr(sp, " <"); if (body) { *body = 0; body += 2; }
	char *sp2 = strchr(sp, ' '); if (!sp2) return; *sp2 = 0;
	snprintf(r.method, sizeof r.method, "%s", sp);
	char *q = strchr(sp2 + 1, '?'); if (q) { *q = 0; snprintf(r.query, sizeof r.query, "%s", q + 1); }
	snprintf(r.path, sizeof r.path, "%s", sp2 + 1);
	if (body) { FILE *f = fopen(body, "rb"); if (f) { r.body = malloc(1 << 20); r.blen = fread(r.body, 1, (1 << 20) - 1, f); r.body[r.blen] = 0; fclose(f); } }
	char tf[] = "/tmp/fpapi-XXXXXX"; r.fd = mkstemp(tf); unlink(tf);
	int post = !strcmp(r.method, "POST");
	if (!strncmp(r.path, "fp/", 3)) api_fp(&r, r.path + 3, post);
	else if (!strncmp(r.path, "home/", 5)) api_home(&r, r.path + 5, post);
	else if (!api_desktop(&r, r.path + (!strncmp(r.path, "desktop/", 8) ? 8 : 0), post)) reply_err(r.fd, 404, "not a desktop request");
	lseek(r.fd, 0, SEEK_SET); char b[1 << 16]; ssize_t n; size_t tot = 0; char *all = NULL;
	while ((n = read(r.fd, b, sizeof b)) > 0) { all = realloc(all, tot + n + 1); memcpy(all + tot, b, n); tot += n; }
	close(r.fd); if (all) all[tot] = 0;
	char *st = all ? all : ""; char *e = strstr(st, "\r\n"); char *bd = strstr(st, "\r\n\r\n");
	printf("%.*s|%s\n", e ? (int)(e - st) : 0, st, bd ? bd + 4 : "");
	fflush(stdout); free(all); free(r.body);
}
int main(int argc, char **argv)
{
	if (argc < 9) return 2;
	snprintf(desk_ls, sizeof desk_ls, "%s", argv[1]); snprintf(desk_dir, sizeof desk_dir, "%s", argv[2]); snprintf(desk_run, sizeof desk_run, "%s", argv[3]);
	snprintf(home_root, sizeof home_root, "%s", argv[4]); snprintf(fp_state, sizeof fp_state, "%s", argv[5]); snprintf(fp_hrun, sizeof fp_hrun, "%s", argv[6]);
	snprintf(au_etc, sizeof au_etc, "%s", argv[7]); snprintf(au_map, sizeof au_map, "%s", argv[8]);
	home_nobind = 1; home_uid = 0; desk_uid = getuid();
	for (int i = 9; i < argc; i++) one(argv[i]);
	return 0;
}
