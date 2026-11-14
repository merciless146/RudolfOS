/* 1.5.9 (plan Part 12): rudolfd's desktop/* requests, compiled from rudolfd.c itself (main renamed), as homeapi.c.
   deskapi ROOT LSFILE DIR RUN "METHOD what?query[ <body-file]"...: each request's status line and body, one per line
   (ROOT: the accounts' folders; LSFILE: the lsync copy; DIR: the desktop user's home; RUN: for the session).
   deskapi crypto pbkdf2 PW SALT-B64 ROUNDS | hkdf IKM-HEX SALT INFO | gcm KEY-HEX IV-B64 CT-B64 | sha256 TEXT | sha512 TEXT */
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
	if (body) { FILE *f = fopen(body, "rb"); if (f) { r.body = malloc(32 << 20); r.blen = fread(r.body, 1, (32 << 20) - 1, f); r.body[r.blen] = 0; fclose(f); } }
	char tf[] = "/tmp/deskapi-XXXXXX"; r.fd = mkstemp(tf); unlink(tf);
	if (!api_desktop(&r, r.path, !strcmp(r.method, "POST"))) reply_err(r.fd, 404, "not a desktop request");
	lseek(r.fd, 0, SEEK_SET); char b[1 << 16]; ssize_t n; size_t tot = 0; char *all = NULL;
	while ((n = read(r.fd, b, sizeof b)) > 0) { all = realloc(all, tot + n + 1); memcpy(all + tot, b, n); tot += n; }
	close(r.fd); if (all) all[tot] = 0;
	char *st = all ? all : ""; char *e = strstr(st, "\r\n"); char *bd = strstr(st, "\r\n\r\n");
	printf("%.*s|%s\n", e ? (int)(e - st) : 0, st, bd ? bd + 4 : "");
	free(all); free(r.body);
}
static int crypto(int argc, char **argv)
{
	uint8_t o[64], k[64], iv[16], ct[256]; char h[260];
	if (argc == 6 && !strcmp(argv[2], "pbkdf2")) { int n = db64d(argv[4], k, sizeof k); dpbkdf2_256(argv[3], k, n, atoi(argv[5]), o, 32); dhex(o, 32, h); puts(h); return 0; }
	if (argc == 6 && !strcmp(argv[2], "hkdf")) { int n = strlen(argv[3]) / 2; for (int i = 0; i < n; i++) k[i] = hexval(argv[3][2 * i]) << 4 | hexval(argv[3][2 * i + 1]); dhkdf512(k, n, argv[4], argv[5], o); dhex(o, 64, h); puts(h); return 0; }
	if (argc == 6 && !strcmp(argv[2], "gcm")) { for (int i = 0; i < 32; i++) k[i] = hexval(argv[3][2 * i]) << 4 | hexval(argv[3][2 * i + 1]);
		int il = db64d(argv[4], iv, sizeof iv), cl = db64d(argv[5], ct, sizeof ct); uint8_t pt[256];
		if (il != 12 || cl < 16 || daes_gcm_open(k, iv, ct, cl, pt)) { puts("FAIL"); return 0; } dhex(pt, cl - 16, h); puts(h); return 0; }
	if (argc == 4 && !strcmp(argv[2], "sha256")) { dsha256 s; dsha256_init(&s); dsha256_add(&s, argv[3], strlen(argv[3])); dsha256_end(&s, o); dhex(o, 32, h); puts(h); return 0; }
	if (argc == 4 && !strcmp(argv[2], "sha512")) { dsha512 s; dsha512_init(&s); dsha512_add(&s, argv[3], strlen(argv[3])); dsha512_end(&s, o); dhex(o, 64, h); puts(h); return 0; }
	return 2;
}
int main(int argc, char **argv)
{
	if (argc > 2 && !strcmp(argv[1], "crypto")) return crypto(argc, argv);
	if (argc < 5) return 2;
	snprintf(home_root, sizeof home_root, "%s", argv[1]); snprintf(desk_ls, sizeof desk_ls, "%s", argv[2]);
	snprintf(desk_dir, sizeof desk_dir, "%s", argv[3]); snprintf(desk_run, sizeof desk_run, "%s", argv[4]);
	home_nobind = 1; home_uid = 1000; desk_uid = getuid();
	for (int i = 5; i < argc; i++) one(argv[i]);
	return 0;
}
