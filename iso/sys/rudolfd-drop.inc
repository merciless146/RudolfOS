/* (included by rudolfd.c, not built on its own: hence .inc) ---------- 1.6.1 (docs/plan-1.6.1.md Part 13): files dropped from
 * outside Rudolf OS onto the native desktop
 * On Standard a file dragged from a Linux app (the Debian file manager, a Flatpak app, an AppImage) onto the desktop is read by
 * Chromium and kept by the page in the Desktop folder (shell25.js, DESK's drop). On Native the shell (rudolf-native) takes the
 * X drop (XDND) and hands the files' paths to a page of its own (oneapp59.js oaFs op=xdrop), which does what Standard's page
 * does with each file; the page reads each file here:
 *
 *   GET desktop/dropped?path=ABSOLUTE   the file (at most 64 MB), else {"ok":false,"error":WHY}, WHY one of: nobody (no
 *                                       account is signed in), path, outside (not the account's own files or a drive),
 *                                       link (a symbolic link on the way), gone, folder (not a plain file), unreadable
 *                                       (the account's Linux user may not read it), too-large
 *
 * What may be read: the signed-in account's own folder, under each name a Linux app gives it (/home/acctUID: its Linux
 * user's home; /persist/accounts/ID: the folder itself; /home/rudolf: the account's folder as Debian's programs see it,
 * rudolf-debian.c acct_binds), and the drives (/media, as the page's own fs/read reads them). Flatpak's document portal
 * (/run/user/UID/doc/..., /run/flatpak/doc/... inside a sandbox) is mapped to the real file it stands for, which must then
 * be one of those; nothing else is read. The account's files are opened by the account's own Linux user (a child process
 * that becomes it, 1.5.9 15.3), so a file it may not read is refused; every step of the path is opened without following
 * a link (O_NOFOLLOW), so no link leads out. The file is never changed or moved: the page keeps a copy. No shell is run. */
#include <sys/xattr.h>
#define DROP_MAX (64u << 20)
static char drop_media[256] = "/media", drop_flatdoc[256] = "/run/flatpak/doc";

/* p with single slashes and no "." or ".." (o: the result); 0 when it is not an absolute path */
static int drop_norm(const char *p, char *o, size_t on)
{
	if (!p || p[0] != '/' || strlen(p) >= on || strlen(p) > 4000) return 0;
	size_t k = 0; const char *s = p;
	while (*s) {
		while (*s == '/') s++;
		if (!*s) break;
		const char *e = strchr(s, '/'); size_t n = e ? (size_t)(e - s) : strlen(s);
		if (!home_name_ok(s, n)) return 0;
		o[k++] = '/'; memcpy(o + k, s, n); k += n; s += n;
	}
	if (!k) o[k++] = '/';
	o[k] = 0; return 1;
}
/* the rest of p below the folder r ("" for r itself), else NULL */
static const char *drop_under(const char *p, const char *r)
{
	size_t l = strlen(r); if (!l || strncmp(p, r, l)) return NULL;
	if (!p[l]) return p + l;
	return p[l] == '/' ? p + l + 1 : NULL;
}
/* WHERE: 1 the account's folder, 2 a drive, 3 the document portal, 4 the portal inside a Flatpak sandbox (rel: below it); 0 outside */
static int drop_where(const char *p, const char *acct, unsigned uid, const char **rel)
{
	char r[400]; const char *x;
	if (au_on && uid) { char nm[32]; au_name(uid, nm, sizeof nm); snprintf(r, sizeof r, "%s/%s", au_homes, nm); if ((x = drop_under(p, r))) { *rel = x; return 1; } }
	snprintf(r, sizeof r, "%s/%s", home_root, acct); if ((x = drop_under(p, r))) { *rel = x; return 1; }
	if ((x = drop_under(p, "/home/rudolf"))) { *rel = x; return 1; }   /* (Debian's view of the account's folder) */
	if ((x = drop_under(p, drop_media)) && *x) { *rel = x; return 2; }
	snprintf(r, sizeof r, "%s/%u/doc", au_runuser, uid); if ((x = drop_under(p, r)) && *x) { *rel = x; return 3; }
	if ((x = drop_under(p, drop_flatdoc)) && *x) { *rel = x; return 4; }   /* (the portal as a sandbox sees it: the same files) */
	return 0;
}
/* a file descriptor (and an errno) across a socket */
static void drop_send(int s, int fd, int err)
{
	struct msghdr m; memset(&m, 0, sizeof m); struct iovec v = { &err, sizeof err }; m.msg_iov = &v; m.msg_iovlen = 1;
	char cb[CMSG_SPACE(sizeof(int))]; memset(cb, 0, sizeof cb);
	if (fd >= 0) { m.msg_control = cb; m.msg_controllen = sizeof cb; struct cmsghdr *c = CMSG_FIRSTHDR(&m); c->cmsg_level = SOL_SOCKET; c->cmsg_type = SCM_RIGHTS; c->cmsg_len = CMSG_LEN(sizeof(int)); memcpy(CMSG_DATA(c), &fd, sizeof fd); }
	if (sendmsg(s, &m, MSG_NOSIGNAL) < 0) {}
}
static int drop_recv(int s, int *err)
{
	struct msghdr m; memset(&m, 0, sizeof m); int e = EIO; struct iovec v = { &e, sizeof e }; m.msg_iov = &v; m.msg_iovlen = 1;
	char cb[CMSG_SPACE(sizeof(int))]; m.msg_control = cb; m.msg_controllen = sizeof cb; int fd = -1;
	ssize_t n = recvmsg(s, &m, MSG_CMSG_CLOEXEC); if (n != (ssize_t)sizeof e) { *err = EIO; return -1; }
	for (struct cmsghdr *c = CMSG_FIRSTHDR(&m); c; c = CMSG_NXTHDR(&m, c)) if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS) memcpy(&fd, CMSG_DATA(c), sizeof fd);
	*err = e; return fd;
}
/* the folder holding rel's last name, every step a real folder: a link on the way is ELOOP (home_parent's O_NOFOLLOW gives
   ENOTDIR for it) */
static int drop_parent(int root, const char *rel, char *last, size_t ln)
{
	int fd = dup(root); if (fd < 0) return -1;
	for (const char *s = rel;;) {
		const char *e = strchr(s, '/'); size_t n = e ? (size_t)(e - s) : strlen(s);
		if (n >= ln || n >= 256) { close(fd); errno = ENAMETOOLONG; return -1; }
		char c[256]; memcpy(c, s, n); c[n] = 0;
		if (!e) { memcpy(last, c, n + 1); return fd; }
		struct stat st; int nf = -1, er = 0;
		if (fstatat(fd, c, &st, AT_SYMLINK_NOFOLLOW)) er = errno; else if (S_ISLNK(st.st_mode)) er = ELOOP;
		else if ((nf = openat(fd, c, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC)) < 0) er = errno;
		close(fd); if (er) { errno = er; return -1; }
		fd = nf; s = e + 1;
	}
}
/* rel below the folder ROOT, opened (read only, no link followed) by uid when uid is given, else as rudolfd; host: what the
   document portal says its file is (the xattr user.document-portal.host-path), read by uid, when rel is a portal path */
static int drop_open(int root, const char *rel, unsigned uid, int *err, const char *portal, char *host, size_t hn)
{
	int sv[2]; if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv)) { *err = errno; return -1; }
	pid_t c = fork(); if (c < 0) { *err = errno; close(sv[0]); close(sv[1]); return -1; }
	if (!c) {
		close(sv[0]);
		if (uid && (setgroups(0, NULL) || setgid(uid) || setuid(uid) || !getuid())) { drop_send(sv[1], -1, EPERM); _exit(0); }
		if (portal) {   /* (the portal's own answer: the path of the real file) */
			char b[4100]; ssize_t n = lgetxattr(portal, "user.document-portal.host-path", b, sizeof b - 1);
			if (n <= 0) { drop_send(sv[1], -1, n < 0 && errno != ENODATA && errno != ENOTSUP ? errno : ENOENT); _exit(0); }
			b[n] = 0; if (n && !b[n - 1]) n--;
			int e = 0; if (write(sv[1], &e, sizeof e) != sizeof e || write(sv[1], b, n) != n) {} _exit(0); }
		char last[256]; int e = 0, fd = -1;
		if (!*rel) e = EISDIR;
		else { int pf = drop_parent(root, rel, last, sizeof last);
			if (pf < 0) e = errno;
			else { fd = openat(pf, last, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_NOCTTY | O_CLOEXEC); if (fd < 0) e = errno; close(pf); } }
		struct stat st; if (fd >= 0 && (fstat(fd, &st) || !S_ISREG(st.st_mode))) { e = S_ISDIR(st.st_mode) ? EISDIR : EINVAL; close(fd); fd = -1; }
		drop_send(sv[1], fd, fd >= 0 ? 0 : e ? e : EIO); _exit(0);
	}
	close(sv[1]); int fd = -1;
	if (portal) { int e = EIO; ssize_t n = read(sv[0], &e, sizeof e); *err = n == (ssize_t)sizeof e ? e : EIO;
		if (!*err) { ssize_t k = 0, m; while (k < (ssize_t)hn - 1 && (m = read(sv[0], host + k, hn - 1 - k)) > 0) k += m; host[k] = 0; if (!k) *err = ENOENT; } }
	else fd = drop_recv(sv[0], err);
	close(sv[0]); int st; while (waitpid(c, &st, 0) < 0 && errno == EINTR) {}
	return fd;
}
static const char *drop_why(int e)
{
	if (e == ELOOP || e == EMLINK) return "link";
	if (e == ENOENT || e == ENOTDIR) return "gone";
	if (e == EISDIR || e == EINVAL) return "folder";
	return "unreadable";   /* (EACCES, EPERM, ENOKEY: the account's user may not read it) */
}
static void api_desktop_dropped(struct req *r)
{
	char *cur = slurp(HOME_SESSION, NULL); if (cur) trim(cur);
	if (!cur || !home_acct_ok(cur) || !strcmp(cur, "guest")) { free(cur); reply_err(r->fd, 409, "nobody"); return; }
	char acct[80]; snprintf(acct, sizeof acct, "%s", cur); free(cur);
	unsigned uid = au_on ? au_uid(acct) : (unsigned)home_uid;
	if (!uid) { reply_err(r->fd, 409, "nobody"); return; }
	/* (an address as long as the request's room for it may have been cut short: refused, never read as a shorter path) */
	if (strlen(r->query) >= sizeof r->query - 1) { reply_err(r->fd, 400, "path"); return; }
	char p[4100]; if (!drop_norm(qparam(r->query, "path"), p, sizeof p)) { reply_err(r->fd, 400, "path"); return; }
	const char *rel = NULL; int w = drop_where(p, acct, uid, &rel);
	if (w == 3 || w == 4) {   /* (the document portal: the real file it names, which must itself be the account's or a drive's) */
		char host[4100], hp[4100], pp[4200]; int e = 0; snprintf(pp, sizeof pp, "%s/%u/doc/%s", au_runuser, uid, rel);
		drop_open(-1, "", uid, &e, pp, host, sizeof host);
		if (e) { reply_err(r->fd, 403, e == ENOENT ? "outside" : drop_why(e)); return; }
		if (!drop_norm(host, hp, sizeof hp)) { reply_err(r->fd, 403, "outside"); return; }
		snprintf(p, sizeof p, "%s", hp); w = drop_where(p, acct, uid, &rel); if (w > 2) w = 0;
	}
	if (!w) { reply_err(r->fd, 403, "outside"); return; }
	int root = w == 1 ? home_root_fd(acct) : open(drop_media, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	if (root < 0) { reply_err(r->fd, 403, w == 1 ? "unreadable" : "gone"); return; }
	int e = 0, fd = drop_open(root, rel, w == 1 ? uid : 0, &e, NULL, NULL, 0); close(root);   /* (a drive: as the page reads drives) */
	if (fd < 0) { reply_err(r->fd, e == ENOENT || e == ENOTDIR ? 404 : 403, drop_why(e)); return; }
	struct stat st; if (fstat(fd, &st) || !S_ISREG(st.st_mode)) { close(fd); reply_err(r->fd, 403, "folder"); return; }
	if ((unsigned long long)st.st_size > DROP_MAX) { close(fd); reply_err(r->fd, 413, "too-large"); return; }
	serve_fd_as(r, fd, p, 1);
}
