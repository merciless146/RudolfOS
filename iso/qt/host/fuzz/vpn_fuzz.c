/* 1.6.2 (docs/plan-1.6.2.md Part 33): a fuzz test of the VPN helper's parsing (rudolf-vpn.c, compiled from it with
 * VPN_NO_MAIN): the WireGuard file an administrator imports, and the names. A valid file read back (its peer, its
 * networks, its DNS servers), then cut at every byte, every byte changed, and mutations (bytes flipped, lines doubled,
 * dropped and swapped, long lines, keys of every length) for a bounded time. Under AddressSanitizer and UBSan; each
 * input within 50 ms (alarm: a loop).   vpn_fuzz SECONDS [SEED]   prints "valid", then "ok N" (inputs read) */
#define VPN_NO_MAIN
#include "../../../sys/rudolf-vpn.c"
#include <sys/time.h>

static void slow(int sig) { (void)sig; static const char m[] = "FAIL: an input took too long (a loop)\n"; if (write(2, m, sizeof m - 1)) {} _exit(3); }
static long N;
static void one(const char *p, size_t n)
{
	char *c = malloc(n ? n : 1); memcpy(c, p, n);   /* (its own allocation: ASan sees a read past the end) */
	struct itimerval it = {{0, 0}, {0, 50000}}, z = {{0, 0}, {0, 0}}; setitimer(ITIMER_REAL, &it, NULL);
	Conf k; char why[300]; conf_parse(c, n, &k, why, sizeof why);
	char nm[80]; size_t L = n < 79 ? n : 79; memcpy(nm, p, L); nm[L] = 0; utf8_name_ok(nm, 64);
	setitimer(ITIMER_REAL, &z, NULL); N++; free(c);
}
/* (the keys: 32 bytes of plain text in base64, "not-a-key: ...", never a key of anything) */
static const char GOOD[] = "[Interface]\nPrivateKey = bm90LWEta2V5OiB0aGUgZnV6eiB0ZXN0J3Mgb3duLiE=\nAddress = 10.66.0.2/32, fd66::2/128\n"
	"DNS = 10.66.0.1, fd66::1, home.lan\nMTU = 1380\n# a comment\n\n[Peer]\nPublicKey = bm90LWEta2V5OiBhIHBlZXIgb2YgdGhlIGZ1enouLiE=\n"
	"PresharedKey = bm90LWEta2V5OiBhIHByZXNoYXJlZCBzdGFuZC1pbiE=\nAllowedIPs = 0.0.0.0/0, ::/0\nEndpoint = [2001:db8::1]:51820\nPersistentKeepalive = 25\n"
	"[Peer]\nPublicKey = bm90LWEta2V5OiB0aGUgc2Vjb25kIHBlZXIncy4uLiE=\nAllowedIPs = 192.168.5.7/24\nEndpoint = vpn.example.com:443\n";
int main(int argc, char **argv)
{
	signal(SIGALRM, slow);
	double secs = argc > 1 ? atof(argv[1]) : 10; unsigned seed = argc > 2 ? (unsigned)atoi(argv[2]) : (unsigned)time(NULL); srand(seed);
	Conf k; char why[300] = "";
	if (conf_parse(GOOD, strlen(GOOD), &k, why, sizeof why) || k.npeer != 2 || k.peer[0].nips != 2 || k.peer[1].ips[0].a[3] != 0 || k.peer[1].ips[0].len != 24 || k.ndns != 2 || strcmp(k.search, "home.lan")
	    || k.peer[0].port != 51820 || strcmp(k.peer[0].host, "2001:db8::1") || strcmp(k.peer[1].host, "vpn.example.com") || k.mtu != 1380 || !full4(&k) || !full6(&k) || !k.peer[0].has_psk) { printf("FAIL: the valid file: %s\n", why); return 1; }
	printf("valid\n");
	size_t L = strlen(GOOD); char b[8192];
	for (size_t i = 0; i <= L; i++) one(GOOD, i);
	for (size_t i = 0; i < L; i++) for (int v = 0; v < 256; v += 17) { memcpy(b, GOOD, L); b[i] = (char)v; one(b, L); }
	struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
	for (;;) {
		clock_gettime(CLOCK_MONOTONIC, &t1); if ((t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9 > secs) break;
		size_t n = L; memcpy(b, GOOD, L);
		int ops = 1 + rand() % 8;
		for (int o = 0; o < ops; o++) {
			int w = rand() % 6; size_t at = n ? (size_t)rand() % n : 0;
			if (w == 0 && n) b[at] = (char)(rand() & 255);
			else if (w == 1 && n + 64 < sizeof b) { size_t k2 = 1 + (size_t)rand() % 64; memmove(b + at + k2, b + at, n - at); for (size_t q = 0; q < k2; q++) b[at + q] = "=,/:[]#\n ab0"[rand() % 12]; n += k2; }
			else if (w == 2 && n) { size_t k2 = 1 + (size_t)rand() % (n - at); memmove(b + at, b + at + k2, n - at - k2); n -= k2; }
			else if (w == 3 && n * 2 < sizeof b) { memcpy(b + n, b, n); n *= 2; }
			else if (w == 4 && n + 5000 < sizeof b) { memset(b + n, 'A', 4500); n += 4500; }
			else if (w == 5 && n + 64 < sizeof b) { const char *k3 = rand() % 2 ? "\nPrivateKey = " : "\nAllowedIPs = "; size_t kl = strlen(k3); memcpy(b + n, k3, kl); n += kl; size_t r = (size_t)rand() % 50; for (size_t q = 0; q < r; q++) b[n++] = "ABCabc019+/=./,:"[rand() % 16]; }
		}
		one(b, n);
	}
	printf("ok %ld (seed %u)\n", N, seed); return 0;
}
