/* sdtool161: the secret detector (iso/sys/rudolf-secrets.h) from the command line, for the tests (secrets161_t.py,
 * blzguard161_t.py, blzguard161_t.js). Never installed. GPL-2.0-or-later.
 *   sdtool161 redact [MASK]          standard input redacted to standard output; the kinds found on standard error
 *   sdtool161 stream N [MASK]        the same, given to the stream N bytes at a time: what it gives out (each piece
 *                                    after a \x1f), then \x1e and the final text
 *   sdtool161 known FILE ...         as redact, with each line of FILE a secret of this computer (FILE is deleted)
 *   sdtool161 keypath PATH           exit 0 when PATH is a key file
 *   sdtool161 json                   a JSON text's strings redacted
 *   sdtool161 relay                  standard input is an engine's whole HTTP answer; standard output the page's
 *   sdtool161 fp B1 B2               the fingerprints of standard input's value (as rudolf-portal --keyring-fingerprints)
 *   SDTOOL_KNOWN=FILE: secrets of this computer for any of these (one a line) */
#define _GNU_SOURCE
#include "../../sys/rudolf-secrets.h"
#include <sys/socket.h>
#include <sys/wait.h>
static char *slurp_fd(int fd, size_t *n) { sd_buf b = {0}; char t[65536]; ssize_t g; sd_add(&b, "", 0); while ((g = read(fd, t, sizeof t)) > 0) sd_add(&b, t, g); *n = b.n; return b.b; }
static void known_file(const char *f)
{
	FILE *k = fopen(f, "r"); if (!k) return; char l[4096];
	while (fgets(l, sizeof l, k)) { l[strcspn(l, "\r\n")] = 0; if (*l) sd_known_add(l, strlen(l)); memset(l, 0, sizeof l); }
	fclose(k);
}
static void kinds_out(int k) { for (int b = 1; b <= SD_PERSONAL; b <<= 1) if (k & b) fprintf(stderr, "%s\n", sd_name(b)); }
int main(int argc, char **argv)
{
	if (argc < 2) return 2;
	sd_known_init(0, 0);
	if (getenv("SDTOOL_KNOWN")) known_file(getenv("SDTOOL_KNOWN"));
	const char *c = argv[1]; size_t n; int k = 0;
	if (!strcmp(c, "keypath")) return argc > 2 && sd_keypath(argv[2]) ? 0 : 1;
	if (!strcmp(c, "fp") && argc > 3) { unsigned long long b1 = strtoull(argv[2], NULL, 16), b2 = strtoull(argv[3], NULL, 16); sd_known_clear(); sd_known_init(b1, b2); SD_K.b1 = b1; SD_K.b2 = b2;
		char *s = slurp_fd(0, &n); sd_known_add(s, n); for (size_t i = 0; i < SD_K.n; i++) printf("%zu %llx %llx\n", SD_K.f[i].len, (unsigned long long)SD_K.f[i].h1, (unsigned long long)SD_K.f[i].h2); return 0; }
	if (!strcmp(c, "known")) { for (int i = 2; i < argc; i++) known_file(argv[i]); c = "redact"; argc = 2; }
	if (!strcmp(c, "redact")) { int mask = argc > 2 ? atoi(argv[2]) : SD_SHOWN; char *s = slurp_fd(0, &n); size_t on; char *r = sd_redact_n(s, n, mask, &k, &on); fwrite(r, 1, on, stdout); kinds_out(k); return 0; }
	if (!strcmp(c, "json")) { char *s = slurp_fd(0, &n); size_t on; char *r = sd_json_redact(s, n, SD_SHOWN, &k, &on); fwrite(r, 1, on, stdout); kinds_out(k); return 0; }
	if (!strcmp(c, "stream") && argc > 2) {
		size_t step = atoi(argv[2]); int mask = argc > 3 ? atoi(argv[3]) : SD_SHOWN; char *s = slurp_fd(0, &n); sd_stream st; sd_stream_init(&st, mask);
		for (size_t i = 0; i <= n; i += step) { size_t m = i + step <= n ? step : n - i; sd_buf o = {0}; sd_stream_push(&st, s + i, m, i + step > n, &o); if (o.n) { fputc(0x1f, stdout); fwrite(o.b, 1, o.n, stdout); } sd_wipe(&o); if (i + step > n) break; }
		size_t rn; char *all = sd_redact_n(st.all.b, st.all.n, mask, &k, &rn); fputc(0x1e, stdout); fwrite(all, 1, rn, stdout); kinds_out(k | st.kinds); return 0; }
	if (!strcmp(c, "relay")) {
		char *s = slurp_fd(0, &n); int sv[2]; if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv)) return 1;
		pid_t p = fork(); if (!p) { close(sv[0]); size_t step = getenv("SDTOOL_STEP") ? atoi(getenv("SDTOOL_STEP")) : 7; for (size_t i = 0; i < n; i += step) { if (write(sv[1], s + i, i + step <= n ? step : n - i) < 0) {} usleep(200); } _exit(0); }
		close(sv[1]); sd_relay_res res = {0}; int r = sd_relay(sv[0], 1, &res, NULL); waitpid(p, NULL, 0); kinds_out(res.kinds); return r ? 1 : 0; }
	return 2;
}
