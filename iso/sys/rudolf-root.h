/*
 * rudolf-root.h - what rudolf-rootd and the root helpers share (Rudolf OS 1.6.1, docs/plan-1.6.1.md Part 11).
 *
 * rudolfd parses every HTTP request of the desktop's page and API; since 1.6.1 what needs root is done by small
 * helpers (/usr/lib/rudolf/root/NAME) that rudolf-rootd runs for it. rudolf-rootd starts rudolfd itself and hands it
 * the one end of a socket pair (RUDOLF_ROOTD_FD): only the rudolfd it started can ask it anything (there is no socket
 * in the file system to connect to). Each request comes on a socket pair of its own, made by rudolfd and passed over
 * that first one: a message "RRQ1" NAME ARG... (each ending in a zero byte) with three descriptors (the helper's
 * standard input, output and error), answered by four bytes, the helper's exit status (as a shell gives it: 0-255,
 * 128+N for signal N; 254: refused by rudolf-rootd, 253: the helper could not be started).
 * Every helper checks its arguments again itself: it never trusts rudolfd (whose parsing it is there to distrust).
 *
 * Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#ifndef RUDOLF_ROOT_H
#define RUDOLF_ROOT_H
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define RR_DIR      "/usr/lib/rudolf/root"   /* the helpers: root:root 0700 */
#define RR_DESK_UID 1000                     /* the desktop's user, rudolf: what rudolfd becomes (Part 11, the last step) */
#define RR_MAGIC    "RRQ1"
#define RR_MAXMSG   65536
#define RR_MAXARGS  32
#define RR_MAXARG   4096
#define RR_REFUSED  254
#define RR_NOSTART  253
#define RR_PATH     "PATH=/usr/sbin:/usr/bin:/sbin:/bin"
#define RR_FN       static __attribute__((unused))   /* (each program uses only some) */

/* host tests only (built with -DRR_TEST): other places for what a helper reads, so its checks can be tried on a
   stand-in tree. A release is built without it, and then nothing in the environment changes where a helper looks. */
#ifdef RR_TEST
/* (the environment, else RR_TEST_CONF's KEY=VALUE lines: rudolf-rootd and rudolfd give a helper no environment) */
RR_FN const char *rr_env(const char *k, const char *def)
{
	const char *v = getenv(k); if (v && *v) return v;
#ifdef RR_TEST_CONF
	static char kv[64][600]; static int n = -1;
	if (n < 0) { n = 0; FILE *f = fopen(RR_TEST_CONF, "r"); while (f && n < 64 && fgets(kv[n], sizeof kv[n], f)) { kv[n][strcspn(kv[n], "\n")] = 0; n++; } if (f) fclose(f); }
	size_t L = strlen(k); for (int i = 0; i < n; i++) if (!strncmp(kv[i], k, L) && kv[i][L] == '=') return kv[i] + L + 1;
#endif
	return def;
}
#else
#define rr_env(k, def) (def)
#endif

/* ---- the messages: data with descriptors (SCM_RIGHTS) ---- */
RR_FN int rr_send(int s, const void *buf, size_t n, const int *fds, int nfd)
{
	struct iovec iov = { (void *)buf, n };
	union { char b[CMSG_SPACE(sizeof(int) * 4)]; struct cmsghdr a; } u;
	struct msghdr m = { .msg_iov = &iov, .msg_iovlen = 1 };
	if (nfd > 0) {
		memset(&u, 0, sizeof u); m.msg_control = u.b; m.msg_controllen = CMSG_SPACE(sizeof(int) * nfd);
		struct cmsghdr *c = CMSG_FIRSTHDR(&m); c->cmsg_level = SOL_SOCKET; c->cmsg_type = SCM_RIGHTS; c->cmsg_len = CMSG_LEN(sizeof(int) * nfd);
		memcpy(CMSG_DATA(c), fds, sizeof(int) * nfd);
	}
	ssize_t r; do r = sendmsg(s, &m, MSG_NOSIGNAL); while (r < 0 && errno == EINTR);
	return r == (ssize_t)n ? 0 : -1;
}
/* the message into buf (at most n bytes; longer: -1); up to maxfd descriptors into fds (*nfd: how many came; any more
   are closed). Descriptors arrive close-on-exec. */
RR_FN ssize_t rr_recv(int s, void *buf, size_t n, int *fds, int *nfd, int maxfd)
{
	struct iovec iov = { buf, n };
	union { char b[CMSG_SPACE(sizeof(int) * 8)]; struct cmsghdr a; } u;
	struct msghdr m = { .msg_iov = &iov, .msg_iovlen = 1, .msg_control = u.b, .msg_controllen = sizeof u.b };
	ssize_t r; do r = recvmsg(s, &m, MSG_CMSG_CLOEXEC); while (r < 0 && errno == EINTR);
	*nfd = 0;
	for (struct cmsghdr *c = CMSG_FIRSTHDR(&m); c; c = CMSG_NXTHDR(&m, c)) {
		if (c->cmsg_level != SOL_SOCKET || c->cmsg_type != SCM_RIGHTS) continue;
		int k = (int)((c->cmsg_len - CMSG_LEN(0)) / sizeof(int)); int *p = (int *)CMSG_DATA(c);
		for (int i = 0; i < k; i++) { if (*nfd < maxfd) fds[(*nfd)++] = p[i]; else close(p[i]); }
	}
	if (r >= 0 && (m.msg_flags & (MSG_TRUNC | MSG_CTRUNC))) { for (int i = 0; i < *nfd; i++) close(fds[i]); *nfd = 0; return -1; }
	return r;
}

/* ---- the checks every helper makes again ---- */
/* a helper's name: lower-case letters, digits and "-", a letter first, at most 32 */
RR_FN int rr_name_ok(const char *s)
{
	if (!s || !(*s >= 'a' && *s <= 'z') || strlen(s) > 32) return 0;
	for (; *s; s++) if (!((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') || *s == '-')) return 0;
	return 1;
}
/* a block device's kernel name (sdb, sdb1, nvme0n1p2, mmcblk0p1): lower-case letters and digits only, at most max;
   so no "/", no "..", nothing a shell or a path could read otherwise */
RR_FN int rr_dev_ok(const char *d, size_t max)
{
	if (!d || !*d || strlen(d) > max) return 0;
	for (; *d; d++) if (!((*d >= 'a' && *d <= 'z') || (*d >= '0' && *d <= '9'))) return 0;
	return 1;
}
/* p, made canonical (every link followed, no "." or ".."), into out; 1 when it is root itself or inside it (root is
   made canonical too). p must exist. */
RR_FN int rr_canon_in(const char *p, const char *root, char *out, size_t on)
{
	char rr[PATH_MAX], rp[PATH_MAX];
	if (!p || !*p || !realpath(root, rr) || !realpath(p, rp)) return 0;
	size_t L = strlen(rr);
	if (strncmp(rp, rr, L) || (rp[L] && rp[L] != '/' && !(L == 1 && rr[0] == '/'))) return 0;
	if (strlen(rp) >= on) return 0;
	memcpy(out, rp, strlen(rp) + 1); return 1;
}
/* a file in the home folder given by the desktop (a disc image to install, a boot image): the text as rudolfd checks
   it (root/..., letters, digits and / . _ - only, no "..", under 200), then made canonical: its folder must really be
   inside root (a link out of it is refused), its name plain (not hidden); if it is there, it must be a plain file (not
   a link) owned by one of the uids in own (0-terminated: the desktop's user and the signed-in account's). It may be
   missing (an image still in parts: joinparts.sh joins them). The canonical path into out. */
RR_FN int rr_home_file_ok(const char *p, const char *root, const unsigned *own, char *out, size_t on)
{
	size_t L = strlen(root);
	if (!p || strlen(p) >= 200 || strncmp(p, root, L) || p[L] != '/' || strstr(p, "..")) return 0;
	for (const char *c = p; *c; c++) if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || strchr("/._-", *c))) return 0;
	const char *b = strrchr(p, '/'); if (!b || !b[1] || b[1] == '.') return 0;
	char dir[PATH_MAX], cd[PATH_MAX]; snprintf(dir, sizeof dir, "%.*s", (int)(b - p), p);
	if (!rr_canon_in(dir, root, cd, sizeof cd)) return 0;
	char full[PATH_MAX]; if ((size_t)snprintf(full, sizeof full, "%s/%s", cd, b + 1) >= sizeof full || strlen(full) >= on) return 0;
	struct stat st;
	if (!lstat(full, &st)) {
		if (!S_ISREG(st.st_mode)) return 0;
		int mine = 0; for (int i = 0; own && own[i]; i++) if (st.st_uid == own[i]) mine = 1;
		if (!mine) return 0;
	} else if (errno != ENOENT) return 0;
	memcpy(out, full, strlen(full) + 1); return 1;
}
/* hex digits only, exactly n of them */
RR_FN int rr_hex_ok(const char *s, size_t n)
{
	if (!s || strlen(s) != n) return 0;
	for (; *s; s++) if (!((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f') || (*s >= 'A' && *s <= 'F'))) return 0;
	return 1;
}

/* ---- the health log (as rudolfd's health_add writes it: {"t":TIME,"k":KIND,"m":TEXT}, one per line) ---- */
static void rr_health(const char *kind, const char *fmt, ...) __attribute__((unused)) __attribute__((format(printf, 2, 3)));
RR_FN void rr_health(const char *kind, const char *fmt, ...)
{
	char m[400]; va_list ap; va_start(ap, fmt); vsnprintf(m, sizeof m, fmt, ap); va_end(ap);
	const char *path = rr_env("RR_TEST_HEALTH", access("/run/rudolf/persistent", F_OK) ? "/run/rudolf/health.log" : "/persist/var/log/health.log");
	char line[1200]; size_t n = (size_t)snprintf(line, sizeof line, "{\"t\":%lld,\"k\":\"%s\",\"m\":\"", (long long)time(NULL), kind);
	for (const unsigned char *c = (const unsigned char *)m; *c && n < sizeof line - 12; c++) {
		if (*c == '"' || *c == '\\') n += (size_t)snprintf(line + n, sizeof line - n, "\\%c", *c);
		else if (*c < 0x20) n += (size_t)snprintf(line + n, sizeof line - n, "\\u%04x", *c);
		else line[n++] = (char)*c;
	}
	n += (size_t)snprintf(line + n, sizeof line - n, "\"}\n");
	int f = open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC | O_NOFOLLOW, 0600);
	if (f >= 0) { if (write(f, line, n) < 0) {} close(f); }
}
#endif
