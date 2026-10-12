/* cdp-eval: evaluate JavaScript in the Rudolf OS desktop page through the Chrome DevTools
 * protocol (only reachable when the system was started with rudolf.debug=1).
 * Usage: cdp-eval 'expression'      prints the JSON result.
 * Copyright (C) 2026 the Rudolf OS authors. SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/random.h>
#include <sys/time.h>
#define DBG(...) do { if (getenv("CDP_DEBUG")) fprintf(stderr, __VA_ARGS__); } while (0)

static int port(void) { const char *p = getenv("CDP_PORT"); return p ? atoi(p) : 9222; }
static int dial(void)
{
	int s = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(port()) };
	a.sin_addr.s_addr = htonl(0x7f000001);
	struct timeval tv = { .tv_sec = 60 };
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
	if (connect(s, (struct sockaddr *)&a, sizeof a)) { perror("connect to the DevTools port (start with rudolf.debug=1)"); exit(2); }
	return s;
}
static void sendall(int s, const void *b, size_t n) { const char *p = b; while (n) { ssize_t w = write(s, p, n); if (w <= 0) exit(3); p += w; n -= w; } }
static size_t readn(int s, void *b, size_t n) { size_t g = 0; while (g < n) { ssize_t r = read(s, (char *)b + g, n - g); if (r <= 0) exit(4); g += r; } return g; }

int main(int argc, char **argv)
{
	if (argc < 2) { fprintf(stderr, "usage: cdp-eval 'expression'\n"); return 1; }
	/* 1. find the page's websocket */
	int s = dial();
	char q[160];
	snprintf(q, sizeof q, "GET /json/list HTTP/1.1\r\nHost: 127.0.0.1:%d\r\nConnection: close\r\n\r\n", port());
	sendall(s, q, strlen(q));
	static char buf[1 << 20]; size_t n = 0; ssize_t r;
	while ((r = read(s, buf + n, sizeof buf - 1 - n)) > 0) {
		n += r; buf[n] = 0;
		char *he = strstr(buf, "\r\n\r\n"), *cl = strcasestr(buf, "Content-Length:");
		if (he && cl && n >= (size_t)(he + 4 - buf) + strtoul(cl + 15, NULL, 10)) break;
	}
	buf[n] = 0; close(s);
	char *p = strstr(buf, "\"type\": \"page\"");
	char *w = p ? strstr(p, "\"webSocketDebuggerUrl\": \"ws://") : NULL;
	if (!w) { char *any = strstr(buf, "ws://127.0.0.1:9222/devtools/page/"); if (!any) { fprintf(stderr, "no page\n"); return 2; } w = any - 30; }
	char *path = strstr(w, "/devtools/page/"), *e = strchr(path, '"');
	*e = 0;
	DBG("target %s\n", path);
	/* 2. websocket handshake */
	s = dial();
	char hs[512];
	snprintf(hs, sizeof hs, "GET %s HTTP/1.1\r\nHost: 127.0.0.1:%d\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n", path, port());
	sendall(s, hs, strlen(hs));
	n = 0;
	while (n < 4 || memcmp(buf + n - 4, "\r\n\r\n", 4)) { readn(s, buf + n, 1); n++; if (n > 4000) return 5; }
	if (!strstr(buf, " 101 ")) { fprintf(stderr, "handshake failed\n"); return 5; }
	/* 3. Runtime.evaluate, the expression JSON-escaped */
	size_t L = strlen(argv[1]);
	char *msg = malloc(L * 6 + 256), *m = msg;
	m += sprintf(m, "{\"id\":1,\"method\":\"Runtime.evaluate\",\"params\":{\"returnByValue\":true,\"awaitPromise\":true,\"expression\":\"");
	for (const unsigned char *c = (const unsigned char *)argv[1]; *c; c++) {
		if (*c == '"' || *c == '\\') { *m++ = '\\'; *m++ = *c; }
		else if (*c < 0x20) m += sprintf(m, "\\u%04x", *c);
		else *m++ = *c;
	}
	m += sprintf(m, "\"}}");
	size_t ml = m - msg;
	unsigned char hdr[14]; size_t hl = 0; unsigned char mask[4];
	if (getrandom(mask, 4, 0) != 4) memcpy(mask, "rudf", 4);
	hdr[hl++] = 0x81;
	if (ml < 126) hdr[hl++] = 0x80 | ml;
	else if (ml < 65536) { hdr[hl++] = 0x80 | 126; hdr[hl++] = ml >> 8; hdr[hl++] = ml; }
	else { hdr[hl++] = 0x80 | 127; for (int i = 7; i >= 0; i--) hdr[hl++] = (unsigned long long)ml >> (8 * i); }
	memcpy(hdr + hl, mask, 4); hl += 4;
	for (size_t i = 0; i < ml; i++) msg[i] ^= mask[i & 3];
	sendall(s, hdr, hl); sendall(s, msg, ml);
	/* 4. read frames until the answer to id 1 */
	for (;;) {
		unsigned char h[2]; readn(s, h, 2);
		unsigned long long len = h[1] & 0x7f;
		if (len == 126) { unsigned char x[2]; readn(s, x, 2); len = (x[0] << 8) | x[1]; }
		else if (len == 127) { unsigned char x[8]; readn(s, x, 8); len = 0; for (int i = 0; i < 8; i++) len = (len << 8) | x[i]; }
		char *d = malloc(len + 1); readn(s, d, len); d[len] = 0;
		DBG("frame op=%x len=%llu %.120s\n", h[0], len, d);
		if (strstr(d, "\"id\":1,") || strstr(d, "\"id\":1}")) { puts(d); return strstr(d, "exceptionDetails") ? 6 : 0; }
		free(d);
	}
}
