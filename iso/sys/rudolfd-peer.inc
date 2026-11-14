/* (included by rudolfd.c, not built on its own: hence .inc) ---------- 1.5.9 (plan Part 15.1): only the desktop gets
 * the page and the API.
 * The desktop page carries the per-boot API token (and rudolf-ls, the copy of the page's storage), and the API runs as
 * root. Until 1.5.9 any program on the computer could connect to 127.0.0.1:8080 and read both: a Linux app, a Wine
 * program or a Flatpak running as uid 1000, like the desktop. Now, for "/" and "/index.html" and for every /api/
 * request, rudolfd finds out who is connecting:
 *   - the peer's socket, from /proc/net/tcp: the line whose local port is the peer's and whose remote end is ours
 *     gives the socket's owner (uid) and its inode;
 *   - root (uid 0): allowed (rudolfd's own helpers, the devlink, the session's scripts: root can do all of it anyway);
 *   - else the socket must belong to a process of the desktop's memory group (/sys/fs/cgroup/desktop, made by the
 *     memory script; the session starts Chromium, rudolf-webkit or rudolf-native in it, and their children stay in it).
 *     Programs rudolfd starts (Linux apps, Flatpak, Wine, Debian, Chromium's window for websites) are rudolfd's
 *     children, outside that group, and a user cannot move a process into it (cgroup.procs belongs to root).
 * The process that answered last is tried first (RUN/peer-hint: usually the browser's network process), so a check
 * reads one process's descriptors, not all the desktop's.
 * No desktop group (a kernel without cgroup v2, the host tests without RUDOLFD_DESKTOP_CG), or an empty one (the
 * desktop could not be put in it): not enforced, as before, said once in the health log.
 * RUDOLFD_DESKTOP_CG=DIR (tests): DIR/cgroup.procs stands for the desktop group's. */
#ifndef PEER_CG_DEFAULT
#define PEER_CG_DEFAULT "/sys/fs/cgroup/desktop"
#endif
#ifndef PEER_HINT
#define PEER_HINT RUN "/peer-hint"
#endif
static int peer_cached = -2;   /* per connection (each is its own process): -2 not looked at yet */
static unsigned peer_uid_seen = (unsigned)-1;

static const char *peer_cg(void) { const char *e = getenv("RUDOLFD_DESKTOP_CG"); return e && *e ? e : PEER_CG_DEFAULT; }

/* the uid and inode of the socket at the other end of fd (a loopback TCP connection); 0 when found */
static int peer_sock(int fd, unsigned *uid, unsigned long *ino)
{
	struct sockaddr_in me, them; socklen_t l1 = sizeof me, l2 = sizeof them;
	if (getsockname(fd, (struct sockaddr *)&me, &l1) || getpeername(fd, (struct sockaddr *)&them, &l2) || them.sin_family != AF_INET) return -1;
	unsigned pport = ntohs(them.sin_port), mport = ntohs(me.sin_port), paddr = them.sin_addr.s_addr, maddr = me.sin_addr.s_addr;
	FILE *f = fopen("/proc/net/tcp", "r"); if (!f) return -1;
	char line[512]; int found = -1;
	if (!fgets(line, sizeof line, f)) { fclose(f); return -1; }   /* the heading */
	while (fgets(line, sizeof line, f)) {
		unsigned la, lp, ra, rp, u; unsigned long in;
		/* "  sl  local_address rem_address   st tx_queue:rx_queue tr:tm->when retrnsmt   uid  timeout inode" */
		if (sscanf(line, " %*d: %8X:%4X %8X:%4X %*X %*X:%*X %*X:%*X %*X %u %*u %lu", &la, &lp, &ra, &rp, &u, &in) != 6) continue;
		/* (the addresses are written as the kernel holds them: network order read as a host-order number) */
		if (lp == pport && rp == mport && la == paddr && ra == maddr) { *uid = u; *ino = in; found = 0; break; }
	}
	fclose(f); return found;
}

/* does process pid hold the socket with this inode? */
static int peer_holds(int pid, unsigned long ino)
{
	char d[64], want[48]; snprintf(d, sizeof d, "/proc/%d/fd", pid); snprintf(want, sizeof want, "socket:[%lu]", ino);
	DIR *D = opendir(d); if (!D) return 0;
	struct dirent *e; int hit = 0; char p[400], t[64];
	while (!hit && (e = readdir(D))) {
		if (e->d_name[0] == '.') continue;
		snprintf(p, sizeof p, "%s/%s", d, e->d_name);
		ssize_t k = readlink(p, t, sizeof t - 1); if (k <= 0) continue; t[k] = 0;
		if (!strcmp(t, want)) hit = 1;
	}
	closedir(D); return hit;
}

/* 1: the desktop (or root); 0: another program; -1: there is no desktop group to check against (not enforced) */
static int peer_desktop(int fd)
{
	if (peer_cached != -2) return peer_cached;
	char cp[300]; snprintf(cp, sizeof cp, "%s/cgroup.procs", peer_cg());
	unsigned uid; unsigned long ino;
	if (peer_sock(fd, &uid, &ino)) return peer_cached = access(cp, R_OK) ? -1 : 0;   /* (not a loopback TCP peer: not the desktop) */
	peer_uid_seen = uid;
	if (uid == 0) return peer_cached = 1;
	FILE *f = fopen(cp, "r"); if (!f) return peer_cached = -1;
	int pids[4096], n = 0, pid;
	while (n < 4096 && fscanf(f, "%d", &pid) == 1) pids[n++] = pid;
	fclose(f);
	/* an empty group: the desktop did not get into it (the session could not write it): checking would shut the desktop
	   itself out, so not enforced (said in the health log) */
	if (!n) return peer_cached = -1;
	int hint = 0; { char *h = slurp(PEER_HINT, NULL); if (h) { hint = atoi(h); free(h); } }
	if (hint > 0) for (int i = 0; i < n; i++) if (pids[i] == hint) { if (peer_holds(hint, ino)) return peer_cached = 1; break; }
	for (int i = 0; i < n; i++) {
		if (pids[i] == hint || !peer_holds(pids[i], ino)) continue;
		char tmp[160]; snprintf(tmp, sizeof tmp, "%s.%d", PEER_HINT, (int)getpid());
		int w = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
		if (w >= 0) { char b[16]; int k = snprintf(b, sizeof b, "%d", pids[i]); if (write(w, b, k) == k && !close(w)) rename(tmp, PEER_HINT); else { close(w); unlink(tmp); } }
		return peer_cached = 1;
	}
	return peer_cached = 0;
}

/* refused: said in the health log at most once a minute (with the uid), never with anything the page holds */
static void peer_refused(int fd, const char *path)
{
	static const char T[] = RUN "/peer-refused";
	struct stat st; time_t now = time(NULL);
	if (stat(T, &st) || now - st.st_mtime >= 60) {
		int w = open(T, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600); if (w >= 0) close(w);
		health_add("security", "a program that is not the desktop (Linux user %u) asked for %s: refused", peer_uid_seen, strncmp(path, "/api/", 5) ? "the desktop's page" : "the system API");
	}
	reply_err(fd, 403, "only Rudolf OS's desktop may use this");
}

/* said once per start when it cannot be checked (no desktop group) */
static void peer_unchecked_note(void)
{
	static const char T[] = RUN "/peer-unchecked";
	if (!access(T, F_OK)) return;
	int w = open(T, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600); if (w >= 0) close(w);
	health_add("security", "the desktop's memory group is missing or empty, so any program on this computer can reach the desktop's page and API (as before 1.5.9)");
}
