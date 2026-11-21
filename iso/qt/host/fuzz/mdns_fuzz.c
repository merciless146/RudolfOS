/* 1.6.1 (docs/plan-1.6.1.md Part 9): a fuzz test of rudolf-print's multicast DNS parsing (parse_mdns, rdname), compiled
 * from rudolf-print.c itself (main renamed). Find printers now reads every answer on the local network's group, so what
 * any device there sends reaches parse_mdns. First the hand-made packets that break DNS parsers (compression loops, a
 * pointer to itself, to past the end, chains of pointers, labels and lengths past the end, counts far beyond the
 * records, a TXT string past its record, an SRV too short), then mutations of a real answer for a bounded time, each
 * parsed with a fresh table; under AddressSanitizer and UBSan. A packet must be parsed within 50 ms (alarm: a loop).
 *   mdns_fuzz SECONDS [SEED]            prints "ok N" (packets parsed) and "found Name" for the real answer's printer */
#define main rudolf_print_main
#include "../../../sys/rudolf-print.c"
#undef main
#include <signal.h>

static void reset(void) { NS = 0; NA = 0; memset(S, 0, sizeof S); memset(A, 0, sizeof A); }
static void slow(int sig) { (void)sig; static const char m[] = "FAIL: a packet took too long (a loop)\n"; if (write(2, m, sizeof m - 1)) {} _exit(3); }
static long N;
static void fz_one(const unsigned char *p, size_t n)
{
	unsigned char *c = malloc(n ? n : 1); memcpy(c, p, n);   /* (its own allocation: ASan sees a read past the end) */
	struct itimerval it = {{0, 0}, {0, 50000}}; setitimer(ITIMER_REAL, &it, NULL);
	reset(); parse_mdns(c, n); N++;
	struct itimerval z = {{0, 0}, {0, 0}}; setitimer(ITIMER_REAL, &z, NULL);
	free(c);
}
static size_t lbl(unsigned char *o, const char *s) { size_t k = 0; for (const char *p = s; *p;) { const char *d = strchr(p, '.'); size_t l = d ? (size_t)(d - p) : strlen(p); o[k++] = l; memcpy(o + k, p, l); k += l; p += l; if (*p) p++; } o[k++] = 0; return k; }
/* a real answer: PTR, SRV, TXT and A for "Office Printer" (compressed names, as printers send them) */
static size_t real(unsigned char *m)
{
	size_t k = 0; unsigned char h[12] = {0, 0, 0x84, 0, 0, 0, 0, 4, 0, 0, 0, 0}; memcpy(m, h, 12); k = 12;
	size_t svc = k; k += lbl(m + k, "_ipp._tcp.local"); unsigned char t1[10] = {0, 12, 0, 1, 0, 0, 0, 120, 0, 0}; memcpy(m + k, t1, 10); k += 10;
	size_t rd = k; m[k++] = 14; memcpy(m + k, "Office Printer", 14); k += 14; m[k++] = 0xC0 | (svc >> 8); m[k++] = svc & 0xff; m[rd - 1] = k - rd;
	size_t inst = rd;
	m[k++] = 0xC0 | (inst >> 8); m[k++] = inst & 0xff; unsigned char t2[10] = {0, 33, 0x80, 1, 0, 0, 0, 120, 0, 0}; memcpy(m + k, t2, 10); k += 10;
	rd = k; unsigned char sp[6] = {0, 0, 0, 0, 0x02, 0x77}; memcpy(m + k, sp, 6); k += 6; size_t host = k; k += lbl(m + k, "office.local"); m[rd - 1] = k - rd;
	m[k++] = 0xC0 | (inst >> 8); m[k++] = inst & 0xff; unsigned char t3[10] = {0, 16, 0x80, 1, 0, 0, 0, 120, 0, 0}; memcpy(m + k, t3, 10); k += 10;
	rd = k; const char *tx[] = {"txtvers=1", "rp=ipp/print", "ty=Fuzz Model", "pdl=application/pdf"}; for (int i = 0; i < 4; i++) { m[k++] = strlen(tx[i]); memcpy(m + k, tx[i], strlen(tx[i])); k += strlen(tx[i]); } m[rd - 1] = k - rd;
	m[k++] = 0xC0 | (host >> 8); m[k++] = host & 0xff; unsigned char t4[10] = {0, 1, 0x80, 1, 0, 0, 0, 120, 0, 4}; memcpy(m + k, t4, 10); k += 10;
	unsigned char ip[4] = {192, 168, 1, 50}; memcpy(m + k, ip, 4); k += 4;
	return k;
}
int main(int argc, char **argv)
{
	int secs = argc > 1 ? atoi(argv[1]) : 10; unsigned seed = argc > 2 ? (unsigned)atoi(argv[2]) : (unsigned)time(NULL);
	signal(SIGALRM, slow);
	unsigned char m[9000]; size_t rn = real(m);
	reset(); parse_mdns(m, rn);
	int found = 0; for (int i = 0; i < NS; i++) if (S[i].srv && S[i].txt && strstr(S[i].inst, "Office Printer") && NA == 1 && !strcmp(A[0].ip, "192.168.1.50") && !strcmp(S[i].ty, "Fuzz Model") && S[i].port == 631) found = 1;
	if (!found) { fprintf(stderr, "FAIL: the real answer was not read (%d services, %d addresses)\n", NS, NA); return 1; }
	printf("found Office Printer\n");
	/* the hand-made ones */
	unsigned char h[64];
	unsigned char hd[12] = {0, 0, 0x84, 0, 0, 0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	memcpy(h, hd, 12); h[12] = 0xC0; h[13] = 12; fz_one(h, 24);                       /* a pointer to itself */
	memcpy(h, hd, 12); h[12] = 0xC0; h[13] = 14; h[14] = 0xC0; h[15] = 12; fz_one(h, 26);   /* two pointing at each other */
	memcpy(h, hd, 12); h[12] = 0xC0; h[13] = 0xff; fz_one(h, 14);                     /* past the end */
	memcpy(h, hd, 12); h[12] = 0xFF; h[13] = 0xff; fz_one(h, 14);
	memcpy(h, hd, 12); h[12] = 0xC0; fz_one(h, 13);                                   /* a pointer cut in half */
	memcpy(h, hd, 12); h[12] = 63; memset(h + 13, 'a', 10); fz_one(h, 23);            /* a label past the end */
	memcpy(h, hd, 12); h[12] = 0x7f; fz_one(h, 13);                                   /* the reserved label kinds */
	memcpy(h, hd, 12); h[12] = 0xbf; fz_one(h, 13);
	{ unsigned char q[12 + 2 * 30]; memcpy(q, hd, 12); for (int i = 0; i < 30; i++) { q[12 + 2 * i] = 0xC0; q[13 + 2 * i] = 12 + 2 * (i + 1); } q[12 + 58] = 0xC0; q[13 + 58] = 12; fz_one(q, sizeof q); }   /* a chain of 30 */
	{ unsigned char q[12 + 6]; memcpy(q, hd, 12); q[4] = 0xff; q[5] = 0xff; memset(q + 12, 0, 6); fz_one(q, sizeof q); }    /* 65535 questions, no room */
	for (size_t cut = 0; cut <= rn; cut++) fz_one(m, cut);                          /* every length of the real answer */
	{ unsigned char q[9000]; memcpy(q, m, rn); for (size_t i = 0; i < rn; i++) for (int v = 0; v < 256; v += 17) { unsigned char o = q[i]; q[i] = v; fz_one(q, rn); q[i] = o; } }   /* each byte changed */
	{ unsigned char q[9000]; size_t k = 12; memcpy(q, hd, 12); q[4] = q[5] = 0; q[6] = 0; q[7] = 1; q[8] = q[9] = q[10] = q[11] = 0;   /* a TXT string past its record, an SRV too short */
	  k += lbl(q + k, "X._ipp._tcp.local"); unsigned char t[10] = {0, 16, 0, 1, 0, 0, 0, 1, 0, 3}; memcpy(q + k, t, 10); k += 10; q[k++] = 200; q[k++] = 'a'; q[k++] = 'b'; fz_one(q, k);
	  k = 12; k += lbl(q + k, "X._ipp._tcp.local"); unsigned char t2[10] = {0, 33, 0, 1, 0, 0, 0, 1, 0, 6}; memcpy(q + k, t2, 10); k += 10; memset(q + k, 0, 6); k += 6; fz_one(q, k); }
	/* mutations for SECONDS */
	srand(seed); time_t end = time(NULL) + secs; unsigned char q[9000];
	while (time(NULL) < end) {
		size_t n = rn; memcpy(q, m, rn);
		int k = 1 + rand() % 8;
		for (int j = 0; j < k; j++) {
			int what = rand() % 6; size_t at = n ? (size_t)rand() % n : 0;
			if (what == 0) q[at] = rand();
			else if (what == 1) q[at] = 0xC0 | (rand() & 0x3f), at + 1 < n ? (void)(q[at + 1] = rand()) : (void)0;
			else if (what == 2 && n > 1) n = 1 + rand() % n;
			else if (what == 3 && n + 64 < sizeof q) { size_t l = rand() % 64; memmove(q + at + l, q + at, n - at); for (size_t z = 0; z < l; z++) q[at + z] = rand(); n += l; }
			else if (what == 4) { q[6 + rand() % 6] = rand(); }
			else if (what == 5) q[at] = rand() % 64;
		}
		fz_one(q, n);
	}
	printf("ok %ld (seed %u)\n", N, seed); return 0;
}
