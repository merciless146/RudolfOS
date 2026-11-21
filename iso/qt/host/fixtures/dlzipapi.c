/* 1.6.1: rudolfd's dl/zip, dl/unzip, dl/unzipped and dl/home's "unzip" (rudolfd-dlzip.inc), compiled from rudolfd.c itself (main
   renamed), as dropapi.c. dlzipapi STORE SCRIPT REQUEST...: STORE is the downloads folder (DLHOME, its .accounts inside), SCRIPT
   dlzip.sh; each REQUEST is "GET what?query" or "POST what?query" (what under dl/), or "wait" (until the job has ended, 120 s at
   most); prints "STATUS|BODY" (the body's first 600 bytes, new lines as spaces) per request. Runs where /run/rudolf may be written
   (dlzip161api_t.sh: a mount namespace of its own). */
#define main rudolfd_main
#include "../../../sys/rudolfd.c"
#undef main
static void one(const char *spec)
{
	struct req r; memset(&r, 0, sizeof r); const char *sp = strchr(spec, ' '); if (!sp) return;
	snprintf(r.method, sizeof r.method, "%.*s", (int)(sp - spec), spec);
	const char *w = sp + 1, *q = strchr(w, '?');
	if (q) { snprintf(r.path, sizeof r.path, "%.*s", (int)(q - w), w); snprintf(r.query, sizeof r.query, "%s", q + 1); } else snprintf(r.path, sizeof r.path, "%s", w);
	char tf[] = "/tmp/dlzipapi-XXXXXX"; r.fd = mkstemp(tf); unlink(tf);
	api_dl(&r, r.path, !strcmp(r.method, "POST"));
	lseek(r.fd, 0, SEEK_SET); char b[1 << 16]; ssize_t n; size_t tot = 0; char *all = NULL;
	while ((n = read(r.fd, b, sizeof b)) > 0) { all = realloc(all, tot + n + 1); memcpy(all + tot, b, n); tot += n; }
	close(r.fd); if (!all) { printf("none|\n"); return; } all[tot] = 0;
	char *e = strstr(all, "\r\n"), *bd = strstr(all, "\r\n\r\n"); size_t bl = bd ? tot - (bd + 4 - all) : 0;
	const char *raw = getenv("DLZIPAPI_RAW"); if (raw && bd) { FILE *f = fopen(raw, "wb"); if (f) { fwrite(bd + 4, 1, bl, f); fclose(f); } }
	printf("%.*s|", e ? (int)(e - all) : 0, all);
	for (size_t i = 0; bd && i < bl && i < 600; i++) putchar(bd[4 + i] == '\n' ? ' ' : bd[4 + i]);
	putchar('\n'); fflush(stdout); free(all);
}
int main(int argc, char **argv)
{
	if (argc < 3) return 2;
	home_nobind = 1; dl_store_paths(argv[1]); snprintf(DLZIP_SH, sizeof DLZIP_SH, "%s", argv[2]);
	mkdir(RUN, 0755); signal(SIGCHLD, SIG_DFL);
	for (int i = 3; i < argc; i++) {
		if (!strcmp(argv[i], "wait")) { for (int k = 0; k < 1200 && job_running(); k++) usleep(100000); printf("waited|%s\n", job_running() ? "running" : "ended"); fflush(stdout); continue; }
		one(argv[i]);
	}
	return 0;
}
