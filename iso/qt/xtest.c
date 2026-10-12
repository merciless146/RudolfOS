/* rudolf-xtest: a window for the Debian test (qt/mkdebrepo.sh). Speaks the X11 protocol directly over
 * /tmp/.X11-unix/X0, so it needs no libraries and runs in a bare Debian. Window class "RudolfXTest".
 * Prints whether the sound server's socket answers and, given a file, its first line.
 * Stays open until the window manager closes the connection, or 10 minutes.   GPL-2.0-or-later. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
static int xs;
static void put(const void *p, size_t n) { if (write(xs, p, n) != (ssize_t)n) { perror("write"); exit(1); } }
static int rd(void *p, size_t n) { size_t g = 0; while (g < n) { ssize_t r = read(xs, (char *)p + g, n - g); if (r <= 0) return -1; g += r; } return 0; }
static void prop(uint32_t win, uint32_t atom, const char *data, uint32_t n)
{
	uint32_t pad = (n + 3) & ~3u; uint8_t h[24] = {18, 0}; uint16_t len = 6 + pad / 4;
	memcpy(h + 2, &len, 2); memcpy(h + 4, &win, 4); memcpy(h + 8, &atom, 4); uint32_t type = 31; memcpy(h + 12, &type, 4);
	h[16] = 8; memcpy(h + 20, &n, 4); put(h, 24); char b[256] = {0}; memcpy(b, data, n); put(b, pad);
}
int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	printf("rudolf-xtest started\n");
	if (argc > 1) { FILE *f = fopen(argv[1], "r"); char l[200] = ""; if (f) { if (!fgets(l, sizeof l, f)) l[0] = 0; fclose(f); } printf("file: %s\n", f ? l : "(cannot open)"); }
	const char *ps = getenv("PULSE_SERVER"); int pa = socket(AF_UNIX, SOCK_STREAM, 0); struct sockaddr_un a = {.sun_family = AF_UNIX};
	snprintf(a.sun_path, sizeof a.sun_path, "%s", ps && !strncmp(ps, "unix:", 5) ? ps + 5 : "/run/user/1000/pulse/native");
	printf("sound socket: %s\n", connect(pa, (struct sockaddr *)&a, sizeof a) ? "no answer" : "ok"); close(pa);
	xs = socket(AF_UNIX, SOCK_STREAM, 0); struct sockaddr_un x = {.sun_family = AF_UNIX}; strcpy(x.sun_path, "/tmp/.X11-unix/X0");
	if (connect(xs, (struct sockaddr *)&x, sizeof x)) { printf("display: cannot connect\n"); return 2; }
	uint8_t setup[12] = {'l', 0, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0}; put(setup, 12);
	uint8_t h[8]; if (rd(h, 8) || h[0] != 1) { printf("display: refused\n"); return 2; }
	uint16_t more; memcpy(&more, h + 6, 2); uint8_t *d = malloc(more * 4); if (rd(d, more * 4)) return 2;
	uint32_t base, mask; memcpy(&base, d + 4, 4); memcpy(&mask, d + 8, 4);
	uint16_t vlen; memcpy(&vlen, d + 16, 2); uint8_t nfmt = d[21];
	uint8_t *scr = d + 32 + ((vlen + 3) & ~3) + 8 * nfmt;
	uint32_t root, white; memcpy(&root, scr, 4); memcpy(&white, scr + 8, 4);
	uint32_t win = base | (1 & mask);
	uint8_t cw[40] = {1, 0}; uint16_t len = 10; memcpy(cw + 2, &len, 2); memcpy(cw + 4, &win, 4); memcpy(cw + 8, &root, 4);
	uint16_t geo[5] = {40, 40, 480, 300, 0}; memcpy(cw + 12, geo, 10); uint16_t cls = 1; memcpy(cw + 22, &cls, 2);
	uint32_t vis = 0, vm = 0x2 | 0x800, ev = 0x8000 | 0x20000; memcpy(cw + 24, &vis, 4); memcpy(cw + 28, &vm, 4); memcpy(cw + 32, &white, 4); memcpy(cw + 36, &ev, 4);
	put(cw, 40);
	prop(win, 39, "Rudolf X Test", 13);
	prop(win, 67, "rudolfxtest\0RudolfXTest\0", 24);
	uint8_t mw[8] = {8, 0, 2, 0}; memcpy(mw + 4, &win, 4); put(mw, 8);
	printf("window shown\n");
	struct pollfd p = {xs, POLLIN, 0}; uint8_t e[32];
	for (int t = 0; t < 600; t++) { if (poll(&p, 1, 1000) > 0) { if (rd(e, 32)) break; if ((e[0] & 0x7f) == 17) break; } }
	printf("closed\n"); return 0;
}
