/* 1.6.2 (docs/plan-1.6.2.md Part 34): a fuzz test of what rudolf-scan reads from the network and from files, compiled
 * from rudolf-scan.c itself (main renamed): its multicast DNS parsing (parse_mdns, rdname: any device on the link can
 * answer Find scanners), its small XML readers (xml_text, xml_block: WS-Discovery's ProbeMatches, a WSD scanner's
 * metadata, eSCL's capabilities from ipp-usb) and its PNG reader (png_decode: the pages it keeps). First real ones
 * (an answer for "Office Scanner", a ProbeMatches, a PNG made by png_encode), each cut at every length and with each
 * byte changed, then random mutations for SECONDS; under AddressSanitizer and UBSan; each input within 200 ms (alarm).
 *   scan_fuzz SECONDS [SEED]            prints "found Office Scanner", "xml ok", "png ok" and "ok N" (inputs read) */
#define main rudolf_scan_main
#include "../../../sys/rudolf-scan.c"
#undef main

static void reset(void) { NS = 0; NA = 0; memset(S, 0, sizeof S); memset(A, 0, sizeof A); }
static void slow(int sig) { (void)sig; static const char m[] = "FAIL: an input took too long (a loop)\n"; if (write(2, m, sizeof m - 1)) {} _exit(3); }
static long N;
static void arm(int on) { struct itimerval it = {{0, 0}, {0, on ? 200000 : 0}}; setitimer(ITIMER_REAL, &it, NULL); }
static void one_mdns(const unsigned char *p, size_t n) { unsigned char *c = malloc(n ? n : 1); memcpy(c, p, n); arm(1); reset(); parse_mdns(c, n); arm(0); free(c); N++; }
static void one_xml(const unsigned char *p, size_t n)
{
	char *c = malloc(n + 1); memcpy(c, p, n); c[n] = 0; const char *e = c + n; char t[300]; arm(1);
	xml_text(c, e, "RelatesTo", t, sizeof t, NULL); xml_text(c, e, "XAddrs", t, sizeof t, NULL); xml_text(c, e, "MakeAndModel", t, 8, NULL);
	const char *bs, *be; for (const char *q = c; q < e && xml_block(q, e, "ProbeMatch", &bs, &be) == 1; q = be + 1) { if (be < bs - 1 || be > e) { fprintf(stderr, "FAIL: a block outside its input\n"); _exit(4); } xml_text(bs, be, "Types", t, sizeof t, NULL); xml_text(bs, be, "Address", t, sizeof t, NULL); }
	for (const char *q = c; q < e && xml_block(q, e, "Hosted", &bs, &be) == 1; q = be + 1) xml_text(bs, be, "Address", t, sizeof t, NULL);
	arm(0); free(c); N++;
}
static void one_png(const unsigned char *p, size_t n) { unsigned char *c = malloc(n ? n : 1); memcpy(c, p, n); img m; arm(1); if (!png_decode(c, n, &m)) free(m.px); arm(0); free(c); N++; }
static size_t lbl(unsigned char *o, const char *s) { size_t k = 0; for (const char *p = s; *p;) { const char *d = strchr(p, '.'); size_t l = d ? (size_t)(d - p) : strlen(p); o[k++] = l; memcpy(o + k, p, l); k += l; p += l; if (*p) p++; } o[k++] = 0; return k; }
static size_t real(unsigned char *m)
{
	size_t k = 0; unsigned char h[12] = {0, 0, 0x84, 0, 0, 0, 0, 4, 0, 0, 0, 0}; memcpy(m, h, 12); k = 12;
	size_t svc = k; k += lbl(m + k, "_uscan._tcp.local"); unsigned char t1[10] = {0, 12, 0, 1, 0, 0, 0, 120, 0, 0}; memcpy(m + k, t1, 10); k += 10;
	size_t rd = k; m[k++] = 14; memcpy(m + k, "Office Scanner", 14); k += 14; m[k++] = 0xC0 | (svc >> 8); m[k++] = svc & 0xff; m[rd - 1] = k - rd;
	size_t inst = rd;
	m[k++] = 0xC0 | (inst >> 8); m[k++] = inst & 0xff; unsigned char t2[10] = {0, 33, 0x80, 1, 0, 0, 0, 120, 0, 0}; memcpy(m + k, t2, 10); k += 10;
	rd = k; unsigned char sp[6] = {0, 0, 0, 0, 0, 80}; memcpy(m + k, sp, 6); k += 6; size_t host = k; k += lbl(m + k, "scanner.local"); m[rd - 1] = k - rd;
	m[k++] = 0xC0 | (inst >> 8); m[k++] = inst & 0xff; unsigned char t3[10] = {0, 16, 0x80, 1, 0, 0, 0, 120, 0, 0}; memcpy(m + k, t3, 10); k += 10;
	rd = k; const char *tx[] = {"txtvers=1", "rs=eSCL", "ty=Fuzz Scanner", "UUID=1234"}; for (int i = 0; i < 4; i++) { m[k++] = strlen(tx[i]); memcpy(m + k, tx[i], strlen(tx[i])); k += strlen(tx[i]); } m[rd - 1] = k - rd;
	m[k++] = 0xC0 | (host >> 8); m[k++] = host & 0xff; unsigned char t4[10] = {0, 1, 0x80, 1, 0, 0, 0, 120, 0, 4}; memcpy(m + k, t4, 10); k += 10;
	unsigned char ip[4] = {192, 168, 1, 60}; memcpy(m + k, ip, 4); k += 4;
	return k;
}
static const char PM[] = "<?xml version=\"1.0\"?><soap:Envelope xmlns:soap=\"x\"><soap:Header><wsa:RelatesTo>urn:uuid:abc</wsa:RelatesTo></soap:Header><soap:Body><wsd:ProbeMatches>"
	"<wsd:ProbeMatch><wsa:EndpointReference><wsa:Address>urn:uuid:dev1</wsa:Address></wsa:EndpointReference><wsd:Types>wsdp:Device wscn:ScanDeviceType</wsd:Types>"
	"<wsd:XAddrs>http://192.168.1.60:80/wsd</wsd:XAddrs></wsd:ProbeMatch></wsd:ProbeMatches><wsdp:Hosted><wsa:Address>http://192.168.1.60/scan</wsa:Address></wsdp:Hosted></soap:Body></soap:Envelope>";
static void mutate(const unsigned char *src, size_t sn, unsigned char *q, size_t *qn, size_t cap)
{
	size_t n = sn; memcpy(q, src, sn); int k = 1 + rand() % 8;
	for (int j = 0; j < k; j++) {
		int what = rand() % 6; size_t at = n ? (size_t)rand() % n : 0;
		if (what == 0 && n) q[at] = rand();
		else if (what == 1 && n) q[at] = "<>/: \"=\0\xc0"[rand() % 9];
		else if (what == 2 && n > 1) n = 1 + rand() % n;
		else if (what == 3 && n + 64 < cap) { size_t l = rand() % 64; memmove(q + at + l, q + at, n - at); for (size_t z = 0; z < l; z++) q[at + z] = rand(); n += l; }
		else if (what == 4 && n > 8) { size_t l = 1 + rand() % 8; if (at + l > n) l = n - at; memmove(q + at, q + at + l, n - at - l); n -= l; }
		else if (what == 5 && n) q[at] = rand() % 64;
	}
	*qn = n;
}
int main(int argc, char **argv)
{
	int secs = argc > 1 ? atoi(argv[1]) : 10; unsigned seed = argc > 2 ? (unsigned)atoi(argv[2]) : (unsigned)time(NULL);
	signal(SIGALRM, slow);
	static unsigned char m[9000], q[70000]; size_t rn = real(m), qn;
	reset(); parse_mdns(m, rn);
	int found = 0; for (int i = 0; i < NS; i++) if (S[i].srv && S[i].txt && strstr(S[i].inst, "Office Scanner") && NA == 1 && !strcmp(A[0].ip, "192.168.1.60") && !strcmp(S[i].ty, "Fuzz Scanner") && !strcmp(S[i].rs, "eSCL") && S[i].port == 80) found = 1;
	if (!found) { fprintf(stderr, "FAIL: the real answer was not read (%d services, %d addresses)\n", NS, NA); return 1; }
	printf("found Office Scanner\n");
	{ char t[100]; const char *bs, *be; const char *e = PM + sizeof PM - 1;
	  if (xml_text(PM, e, "RelatesTo", t, sizeof t, NULL) != 1 || strcmp(t, "urn:uuid:abc") || xml_block(PM, e, "ProbeMatch", &bs, &be) != 1 || xml_text(bs, be, "XAddrs", t, sizeof t, NULL) != 1 || strcmp(t, "http://192.168.1.60:80/wsd")
	      || xml_text(bs, be, "Address", t, sizeof t, NULL) != 1 || strcmp(t, "urn:uuid:dev1") || xml_block(PM, e, "Hosted", &bs, &be) != 1 || xml_text(bs, be, "Address", t, sizeof t, NULL) != 1 || strcmp(t, "http://192.168.1.60/scan")) { fprintf(stderr, "FAIL: the real ProbeMatches was not read\n"); return 1; }
	  printf("xml ok\n"); }
	img pi = {0}; img_alloc(&pi, 37, 23, 3); for (int i = 0; i < 37 * 23 * 3; i++) pi.px[i] = i * 7; pi.dpi = 150; buf pb = {0}; png_encode(&pi, &pb);
	{ img back; if (png_decode(pb.b, pb.n, &back) || back.w != 37 || back.h != 23 || back.dpi != 150 || memcmp(back.px, pi.px, 37 * 23 * 3)) { fprintf(stderr, "FAIL: a PNG written was not read back as it was\n"); return 1; } free(back.px); printf("png ok\n"); }
	for (size_t cut = 0; cut <= rn; cut++) one_mdns(m, cut);
	for (size_t i = 0; i < rn; i++) for (int v = 0; v < 256; v += 17) { memcpy(q, m, rn); q[i] = v; one_mdns(q, rn); }
	for (size_t cut = 0; cut < sizeof PM; cut++) one_xml((const unsigned char *)PM, cut);
	for (size_t i = 0; i < sizeof PM - 1; i++) { memcpy(q, PM, sizeof PM - 1); q[i] = '<'; one_xml(q, sizeof PM - 1); q[i] = '>'; one_xml(q, sizeof PM - 1); q[i] = 0; one_xml(q, sizeof PM - 1); }
	for (size_t cut = 0; cut <= pb.n; cut += 1) one_png(pb.b, cut);
	for (size_t i = 0; i < pb.n; i++) for (int v = 0; v < 256; v += 51) { memcpy(q, pb.b, pb.n); q[i] = v; one_png(q, pb.n); }
	srand(seed); time_t end = time(NULL) + secs;
	while (time(NULL) < end) {
		mutate(m, rn, q, &qn, 9000); one_mdns(q, qn);
		mutate((const unsigned char *)PM, sizeof PM - 1, q, &qn, sizeof q); one_xml(q, qn);
		mutate(pb.b, pb.n, q, &qn, sizeof q); one_png(q, qn);
	}
	free(pb.b); free(pi.px);
	printf("ok %ld (seed %u)\n", N, seed); return 0;
}
