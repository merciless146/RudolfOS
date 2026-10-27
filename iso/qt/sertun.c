/* sertun - test scaffolding (not part of Rudolf OS): TCP connections from inside a QEMU guest to the host over a
 * serial port, for test machines without a network (this QEMU has no user-mode networking). The guest side listens
 * on 127.0.0.1:PORT and relays one connection at a time over the serial device in frames ("STN" [type][len:4 BE][data];
 * type 'O' open, 'D' data, 'C' close). The marker lets the host side skip what else the port carries (the firmware
 * writes its screen to every serial port at start-up); sertun.py on the host opens the matching connection there.
 *   sertun /dev/ttyS1 PORT        (static: gcc -O2 -static -o sertun sertun.c) */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

static int T;
static int wall(int fd, const void *b, size_t n) { const char *p = b; while (n) { ssize_t k = write(fd, p, n); if (k < 0) { if (errno == EINTR || errno == EAGAIN) { usleep(1000); continue; } return -1; } p += k; n -= k; } return 0; }
static int rall(int fd, void *b, size_t n) { char *p = b; while (n) { ssize_t k = read(fd, p, n); if (k <= 0) { if (k < 0 && (errno == EINTR || errno == EAGAIN)) { usleep(1000); continue; } return -1; } p += k; n -= k; } return 0; }
static void frame(char t, const void *d, uint32_t n) { unsigned char h[8] = { 'S', 'T', 'N', t, n >> 24, n >> 16, n >> 8, n }; wall(T, h, 8); if (n) wall(T, d, n); }

int main(int argc, char **argv)
{
	if (argc != 3) { fprintf(stderr, "usage: sertun DEVICE PORT\n"); return 2; }
	signal(SIGPIPE, SIG_IGN);
	T = open(argv[1], O_RDWR | O_NOCTTY); if (T < 0) { perror(argv[1]); return 1; }
	struct termios tio; if (!tcgetattr(T, &tio)) { cfmakeraw(&tio); cfsetspeed(&tio, B115200); tcsetattr(T, TCSANOW, &tio); }
	int s = socket(AF_INET, SOCK_STREAM, 0), one = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
	struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(atoi(argv[2])), .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
	if (bind(s, (struct sockaddr *)&a, sizeof a) || listen(s, 16)) { perror("bind"); return 1; }
	static char b[65536];
	for (;;) {
		int c = accept(s, NULL, NULL); if (c < 0) continue;
		frame('O', NULL, 0);
		int open_c = 1, host_open = 1;
		while (host_open) {
			struct pollfd p[2] = { { T, POLLIN, 0 }, { c, open_c ? POLLIN : 0, 0 } };
			if (poll(p, 2, -1) < 0) continue;
			if (p[1].revents & (POLLIN | POLLHUP | POLLERR)) {
				ssize_t k = read(c, b, sizeof b);
				if (k > 0) frame('D', b, k); else { open_c = 0; shutdown(c, SHUT_RD); frame('C', NULL, 0); }
			}
			if (p[0].revents & POLLIN) {
				/* skip to the marker */
				unsigned char h[5], m = 0; int got = 0;
				while (got < 3) { if (rall(T, &m, 1)) return 1; got = m == "STN"[got] ? got + 1 : m == 'S'; }
				if (rall(T, h, 5)) return 1;
				uint32_t n = (uint32_t)h[1] << 24 | h[2] << 16 | h[3] << 8 | h[4]; if (n > sizeof b) continue;
				if (n && rall(T, b, n)) return 1;
				if (h[0] == 'D' && open_c) wall(c, b, n);
				else if (h[0] == 'C') host_open = 0;
			}
		}
		close(c);
	}
}
