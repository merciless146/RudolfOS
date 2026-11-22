/* 1.6.2 (docs/plan-1.6.2.md Part 30): a fuzz test of the LAN game relay's parsing (rudolf-duel.c, compiled from it with
 * RD_NO_MAIN): what any computer on the local network can send it. Multicast DNS: its own announcement read back by
 * find's parser (duel_parse) as a round trip, the hand-made packets that break DNS parsers (compression loops, pointers
 * past the end, labels past the end, counts far beyond the records, a TXT string past its record), then mutations for a
 * bounded time, both through duel_parse (find) and mdns_wants (the relay answering questions). The games' messages: the
 * JSON fields the relay reads (jget) and its UTF-8 check, on hand-made and mutated text. Under AddressSanitizer and
 * UBSan; each input within 50 ms (alarm: a loop).
 *   duel_fuzz SECONDS [SEED]   prints "found Test PC" for the round trip, and "ok N" (inputs read) */
#define RD_NO_MAIN
#include "../../../sys/rudolf-duel.c"
#include <signal.h>
#include <sys/time.h>

static void slow(int sig) { (void)sig; static const char m[] = "FAIL: an input took too long (a loop)\n"; if (write(2, m, sizeof m - 1)) {} _exit(3); }
static long N;
static void arm(void) { struct itimerval it = {{0, 0}, {0, 50000}}; setitimer(ITIMER_REAL, &it, NULL); }
static void disarm(void) { struct itimerval z = {{0, 0}, {0, 0}}; setitimer(ITIMER_REAL, &z, NULL); }
static void reset(void) { NF = 0; NAR = 0; memset(FOUND, 0, sizeof FOUND); memset(AR, 0, sizeof AR); }
static void dns_one(const unsigned char *p, size_t n)
{
	unsigned char *c = malloc(n ? n : 1); memcpy(c, p, n);   /* (its own allocation: ASan sees a read past the end) */
	arm(); reset(); duel_parse(c, n);
	unsigned char *q = malloc(n ? n : 1); memcpy(q, p, n); if (n > 2) q[2] &= 0x7f;   /* (the same as a question) */
	size_t qe = 0; mdns_wants(q, n, &qe); disarm(); N++;
	free(c); free(q);
}
static void text_one(const unsigned char *p, size_t n)
{
	char *c = malloc(n ? n : 1); memcpy(c, p, n); char out[64];
	arm(); jget(c, n, "t", out, sizeof out); jget(c, n, "ver", out, sizeof out); jget(c, n, "want", out, sizeof out); jget(c, n, "game", out, sizeof out); jget(c, n, "proto", out, sizeof out); jget(c, n, "code", out, 8); utf8_ok((unsigned char *)c, n); disarm(); N++;
	free(c);
}
int main(int argc, char **argv)
{
	int secs = argc > 1 ? atoi(argv[1]) : 10; unsigned seed = argc > 2 ? (unsigned)atoi(argv[2]) : (unsigned)time(NULL);
	signal(SIGALRM, slow); srand(seed);
	/* the relay's own announcement, read back */
	snprintf(ME.name, sizeof ME.name, "Test PC"); snprintf(ME.code, sizeof ME.code, "ABC234"); snprintf(ME.v, sizeof ME.v, "0.5.2"); snprintf(ME.game, sizeof ME.game, "ember-reach");
	snprintf(ME.host, sizeof ME.host, "rudolf-duel-abc234.local"); snprintf(ME.inst, sizeof ME.inst, "Test PC." SVC); ME.port = 47731; inet_pton(AF_INET, "192.168.1.20", &ME.addr);
	buf a = {0}; mdns_answer(&a, 120, 0, NULL, 0);
	reset(); duel_parse(a.b, a.n);
	int found = 0; for (int i = 0; i < NF; i++) if (FOUND[i].srv && FOUND[i].txt && !strcmp(FOUND[i].inst, "Test PC." SVC) && FOUND[i].port == 47731 && !strcmp(FOUND[i].code, "ABC234") && !strcmp(FOUND[i].v, "0.5.2") && !strcmp(FOUND[i].game, "ember-reach") && NAR == 1 && !strcmp(AR[0].ip, "192.168.1.20") && !strcasecmp(AR[0].name, FOUND[i].host)) found = 1;
	if (!found) { fprintf(stderr, "FAIL: the announcement was not read back (%d services, %d addresses)\n", NF, NAR); return 1; }
	printf("found Test PC\n");
	/* a question for the service is one the relay answers; one for another isn't */
	{ buf q = {0}; put16(&q, 0); put16(&q, 0); put16(&q, 1); put16(&q, 0); put16(&q, 0); put16(&q, 0); pname(&q, NULL, SVC); put16(&q, 12); put16(&q, 1); size_t e = 0; int w1 = mdns_wants(q.b, q.n, &e);
	  buf q2 = {0}; put16(&q2, 0); put16(&q2, 0); put16(&q2, 1); put16(&q2, 0); put16(&q2, 0); put16(&q2, 0); pname(&q2, NULL, "_ipp._tcp.local"); put16(&q2, 12); put16(&q2, 1); int w2 = mdns_wants(q2.b, q2.n, &e);
	  if (!w1 || w2) { fprintf(stderr, "FAIL: questions: ours %d, another's %d\n", w1, w2); return 1; } free(q.b); free(q2.b); }
	/* the JSON fields and the UTF-8 check, by hand */
	{ char o[64];
	  struct { const char *s, *k, *want; int ty; } J[] = {
		{"{\"t\":\"host\",\"v\":\"0.5.2\",\"proto\":1}", "t", "host", 's'}, {"{\"t\":\"host\",\"v\":\"0.5.2\",\"proto\":1}", "proto", "1", 'n'},
		{" { \"x\" : {\"t\":\"no\"} , \"t\" : \"join\" }", "t", "join", 's'}, {"{\"s\":\"\\\"t\\\":\\\"x\\\"\",\"t\":\"st\"}", "t", "st", 's'},
		{"{\"a\":[\"t\",{\"t\":1}],\"t\":\"ok\"}", "t", "ok", 's'}, {"[\"t\",\"host\"]", "t", "", 0}, {"{\"t\":", "t", "", 0}, {"{\"t\":\"unterminated", "t", "", 0}, {"", "t", "", 0}};
	  for (size_t i = 0; i < sizeof J / sizeof J[0]; i++) { int ty = jget(J[i].s, strlen(J[i].s), J[i].k, o, sizeof o); if (ty != J[i].ty || (ty && strcmp(o, J[i].want))) { fprintf(stderr, "FAIL: jget %zu: %s -> %d '%s'\n", i, J[i].s, ty, o); return 1; } }
	  const char *good[] = {"plain", "\xc3\xa9", "\xe2\x9c\x93", "\xf0\x9f\x9a\x80"}, *bad[] = {"\xc0\xaf", "\xe0\x80\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xc3", "\x80", "\xff"};
	  for (size_t i = 0; i < 4; i++) if (!utf8_ok((const unsigned char *)good[i], strlen(good[i]))) { fprintf(stderr, "FAIL: utf8 good %zu\n", i); return 1; }
	  for (size_t i = 0; i < 7; i++) if (utf8_ok((const unsigned char *)bad[i], strlen(bad[i]))) { fprintf(stderr, "FAIL: utf8 bad %zu\n", i); return 1; } }
	/* the hand-made packets */
	unsigned char h[80]; unsigned char hd[12] = {0, 0, 0x84, 0, 0, 1, 0, 0x30, 0, 0, 0, 0};
	memcpy(h, hd, 12); h[12] = 0xC0; h[13] = 12; dns_one(h, 24);                          /* a pointer to itself */
	memcpy(h, hd, 12); h[12] = 0xC0; h[13] = 14; h[14] = 0xC0; h[15] = 12; dns_one(h, 26);  /* two pointing at each other */
	memcpy(h, hd, 12); h[12] = 0xC0; h[13] = 0xff; dns_one(h, 14);                        /* past the end */
	memcpy(h, hd, 12); h[12] = 0xC0; dns_one(h, 13);                                      /* a pointer cut in half */
	memcpy(h, hd, 12); h[12] = 63; memset(h + 13, 'a', 10); dns_one(h, 23);               /* a label past the end */
	memcpy(h, hd, 12); h[12] = 0x7f; dns_one(h, 13); memcpy(h, hd, 12); h[12] = 0xbf; dns_one(h, 13);   /* reserved label kinds */
	memcpy(h, hd, 12); h[5] = 0xff; h[12] = 0; dns_one(h, 13);                            /* far more questions than there are */
	{ unsigned char q[12 + 2 * 30]; memcpy(q, hd, 12); for (int i = 0; i < 30; i++) { q[12 + 2 * i] = 0xC0; q[13 + 2 * i] = 12 + 2 * (i + 1); } q[70] = 0xC0; q[71] = 12; dns_one(q, sizeof q); }
	/* a TXT string longer than its record, an SRV too short, an A of the wrong length: in the real answer */
	for (size_t k = 12; k < a.n; k++) { unsigned char *c = malloc(a.n); memcpy(c, a.b, a.n); c[k] = 0xff; dns_one(c, a.n); c[k] = 0; dns_one(c, a.n); c[k] = 0xc0; dns_one(c, a.n); free(c); }
	for (size_t cut = 0; cut < a.n; cut++) dns_one(a.b, cut);
	/* mutations, for the time given */
	const char *seeds[] = {"{\"t\":\"hello\",\"proto\":1,\"game\":\"ember-reach\",\"ver\":\"0.5.2\",\"want\":\"join\",\"code\":\"ABC234\"}", "{\"t\":\"st\",\"r\":1,\"s\":{\"p\":[1,2,3],\"q\":[1,0,0,0]},\"c\":{\"th\":0.5}}", "{\"t\":\"shot\",\"s\":{\"k\":\"pulse\",\"id\":\"g1\",\"d\":6,\"p\":[0,0,0],\"v\":[1,2,3]}}"};
	time_t end = time(NULL) + secs;
	while (time(NULL) < end) {
		unsigned char m[2048]; size_t n = a.n; memcpy(m, a.b, n);
		int k = 1 + rand() % 8; for (int i = 0; i < k; i++) { size_t at = rand() % n; switch (rand() % 4) { case 0: m[at] = rand(); break; case 1: m[at] ^= 1 << (rand() % 8); break; case 2: m[at] = 0xC0 | (rand() & 1); break; default: n = 12 + rand() % (n - 11); } }
		dns_one(m, n);
		const char *s = seeds[rand() % 3]; size_t tl = strlen(s); unsigned char t[512]; memcpy(t, s, tl);
		k = 1 + rand() % 6; for (int i = 0; i < k; i++) { size_t at = rand() % tl; switch (rand() % 4) { case 0: t[at] = rand(); break; case 1: t[at] = "{}[]\",:\\"[rand() % 8]; break; case 2: t[at] = 0x80 | rand(); break; default: tl = rand() % tl + 1; } }
		text_one(t, tl);
	}
	printf("ok %ld\n", N);
	free(a.b);
	return 0;
}
