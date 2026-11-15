/* (included by rudolfd.c, not built on its own: hence .inc) ---------- 1.5.9 (plan 15.3): each account is its own Linux user
 * Until 1.5.9 every account's Linux apps ran as the desktop's user (rudolf, 1000), so they could read every account's
 * files. Now each account (the page's bd95user list, from the lsync copy) has a Linux user of its own, acctUID with uid
 * 1001 and up, made when the account appears and removed when it goes (au_sync, after each lsync). Its uid is kept in
 * AU_MAP ("ID UID [moved]"; a removed account's line becomes "-ID UID", so a uid is never given again). The root file
 * system's /etc is in RAM, so rudolfd writes the users into /etc/passwd, group and shadow at every start (au_install).
 *
 * The account's folder (/persist/accounts/ID, encrypted with its key) belongs to its user, 0700. At sign-in with the
 * files on the disk (home_bind) the account's session starts (au_session_start): the folder at /home/acctUID (its
 * HOME), /run/user/UID, its own session D-Bus and the desktop portal as that user, and RUN/acct-session.env, which the
 * Linux apps and Flatpak apps rudolfd starts take (au_app_user: they run as the account's user). Signing out
 * (home_unbind) stops every program of that user (au_session_stop). Sound: the desktop's PulseAudio (one sound server
 * owns the sound card) offers a second socket, RUN/pulse/native, to the audio group, which account users are in, as
 * they are in video and input and the devices' group 1000 (cameras, controllers, the ADB phone).
 * The move of existing computers (au_migrate, once per account at start): what the desktop's user owned in the
 * account's folder becomes the account user's (ownership only, nothing copied), logged in the health log; what
 * cannot be moved is named there (Accounts) and in System health.
 * Second step (1.5.9): the display is the account user's only while it is signed in (au_xaccess: X's "localuser"
 * access, as the session gives it to root and the desktop's user; X runs with authorization, no longer -ac); Debian
 * and Wine run as it (rudolf-debian maps Debian's "rudolf" onto the account's user, with the account's folder as its
 * home); Library packages too (rpk run, POST pkg/exec). Not as the account's user: the Chromium window for websites
 * on Lite (part of the desktop). */
#ifndef AU_MAP_DEF
#define AU_MAP_DEF PERSIST "/etc/rudolf/account-users"
#endif
#define AU_FIRST 1001
#define AU_LAST 59999
#define AU_MAX 256
static int au_on;   /* rudolfd's main turns it on; host fixtures choose */
static char au_etc[256] = "/etc", au_map[256] = AU_MAP_DEF, au_homes[256] = "/home", au_runuser[256] = "/run/user";
typedef struct { char id[80]; unsigned uid; int retired, moved; } au_ent;

static void au_name(unsigned uid, char *o, size_t n) { snprintf(o, n, "acct%u", uid); }
static int au_read(au_ent *e, int max)
{
	char *t = slurp(au_map, NULL); if (!t) return 0;
	int n = 0; char *save = NULL;
	for (char *l = strtok_r(t, "\n", &save); l && n < max; l = strtok_r(NULL, "\n", &save)) {
		char id[80] = "", fl[16] = ""; unsigned u = 0; int ret = *l == '-';
		if (sscanf(l + ret, "%79s %u %15s", id, &u, fl) < 2 || u < AU_FIRST || u > AU_LAST || !home_acct_ok(id)) continue;
		snprintf(e[n].id, sizeof e[n].id, "%s", id); e[n].uid = u; e[n].retired = ret; e[n].moved = !strcmp(fl, "moved"); n++;
	}
	free(t); return n;
}
static int au_write(const au_ent *e, int n)
{
	char d[300]; snprintf(d, sizeof d, "%s", au_map); char *sl = strrchr(d, '/'); if (sl) { *sl = 0; mkdir(d, 0755); }
	str s = {0}; sadd(&s, "%s", "");
	for (int i = 0; i < n; i++) sadd(&s, "%s%s %u%s\n", e[i].retired ? "-" : "", e[i].id, e[i].uid, e[i].moved && !e[i].retired ? " moved" : "");
	char t[320]; snprintf(t, sizeof t, "%s.tmp", au_map);
	int f = open(t, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644), ok = f >= 0 && write(f, s.b, s.n) == (ssize_t)s.n && !fsync(f);
	if (f >= 0) close(f);
	free(s.b); if (!ok || rename(t, au_map)) { unlink(t); return -1; }
	return 0;
}
static const au_ent *au_find(const au_ent *e, int n, const char *id) { for (int i = 0; i < n; i++) if (!e[i].retired && !strcmp(e[i].id, id)) return &e[i]; return NULL; }
/* the account's uid, 0 when it has none */
static unsigned au_uid(const char *id)
{
	if (!au_on || !home_acct_ok(id)) return 0;
	au_ent e[AU_MAX]; int n = au_read(e, AU_MAX); const au_ent *x = au_find(e, n, id); return x ? x->uid : 0;
}

/* ---- /etc: a line of each file kept or replaced (name:...), the others as they are */
static int au_etc_edit(const char *file, const char *name, const char *line, int mode)
{
	char p[300], t[320]; snprintf(p, sizeof p, "%s/%s", au_etc, file); snprintf(t, sizeof t, "%s.rudolfd", p);
	char *c = slurp(p, NULL); str s = {0}; sadd(&s, "%s", ""); size_t nl = strlen(name); int had = 0;
	for (char *l = c; l && *l; ) {
		char *e = strchr(l, '\n'); size_t L = e ? (size_t)(e - l) : strlen(l);
		if (L > nl && !strncmp(l, name, nl) && l[nl] == ':') { had = 1; if (line) sadd(&s, "%s\n", line); }
		else sadd(&s, "%.*s\n", (int)L, l);
		l = e ? e + 1 : l + L;
	}
	if (!had && line) sadd(&s, "%s\n", line);
	int f = open(t, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode), ok = f >= 0 && write(f, s.b, s.n) == (ssize_t)s.n;
	if (f >= 0) { if (fchmod(f, mode)) {} close(f); }
	free(c); free(s.b);
	if (!ok || rename(t, p)) { unlink(t); return -1; }
	return 0;
}
/* a supplementary group's members: NAME added (add) or taken out */
static int au_group_member(const char *group, const char *name, int add)
{
	char p[300]; snprintf(p, sizeof p, "%s/group", au_etc); char *c = slurp(p, NULL); if (!c) return -1;
	size_t gl = strlen(group); char *line = NULL;
	for (char *l = c; l && *l; ) { char *e = strchr(l, '\n'); size_t L = e ? (size_t)(e - l) : strlen(l);
		if (L > gl && !strncmp(l, group, gl) && l[gl] == ':') { line = strndup(l, L); break; }
		l = e ? e + 1 : l + L; }
	free(c); if (!line) return -1;
	/* group:x:gid:a,b,c */
	char *m = line; for (int k = 0; k < 3 && m; k++) { m = strchr(m, ':'); if (m) m++; }
	if (!m) { free(line); return -1; }
	str s = {0}; sadd(&s, "%.*s", (int)(m - line), line); int first = 1, had = 0;
	char *save = NULL; char *mem = strdup(m);
	for (char *x = strtok_r(mem, ",", &save); x; x = strtok_r(NULL, ",", &save)) {
		if (!strcmp(x, name)) { had = 1; if (!add) continue; }
		sadd(&s, "%s%s", first ? "" : ",", x); first = 0;
	}
	if (add && !had) sadd(&s, "%s%s", first ? "" : ",", name);
	int r = au_etc_edit("group", group, s.b, 0644);
	free(mem); free(s.b); free(line); return r;
}
static const char *const AU_GROUPS[] = { "audio", "video", "input", "rudolf", NULL };   /* (rudolf: the devices' group 1000) */
static int au_install(const au_ent *x)
{
	char nm[32], pl[700], gl[100], sl[100]; au_name(x->uid, nm, sizeof nm);
	snprintf(pl, sizeof pl, "%s:x:%u:%u:Rudolf OS account %s:%s/%s:/bin/sh", nm, x->uid, x->uid, x->id, au_homes, nm);
	snprintf(gl, sizeof gl, "%s:x:%u:", nm, x->uid); snprintf(sl, sizeof sl, "%s:*:19000:0:99999:7:::", nm);
	int r = au_etc_edit("passwd", nm, pl, 0644) | au_etc_edit("group", nm, gl, 0644) | au_etc_edit("shadow", nm, sl, 0600);
	for (int i = 0; AU_GROUPS[i]; i++) au_group_member(AU_GROUPS[i], nm, 1);
	return r;
}
static void au_uninstall(unsigned uid)
{
	char nm[32]; au_name(uid, nm, sizeof nm);
	au_etc_edit("passwd", nm, NULL, 0644); au_etc_edit("group", nm, NULL, 0644); au_etc_edit("shadow", nm, NULL, 0600);
	for (int i = 0; AU_GROUPS[i]; i++) au_group_member(AU_GROUPS[i], nm, 0);
}

/* ---- every program of a user stopped (TERM, then KILL after 3 s) */
static int au_kill_user(unsigned uid)
{
	int left = 0;
	for (int round = 0; round < 2; round++) {
		DIR *d = opendir("/proc"); struct dirent *e; left = 0;
		while (d && (e = readdir(d))) {
			int pid = atoi(e->d_name); if (pid <= 1 || pid == getpid()) continue;
			char p[64]; struct stat st; snprintf(p, sizeof p, "/proc/%d", pid);
			if (stat(p, &st) || st.st_uid != uid) continue;
			kill(pid, round ? SIGKILL : SIGTERM); left++;
		}
		if (d) closedir(d);
		if (!left) break;
		if (!round) for (int i = 0; i < 30; i++) { usleep(100000); DIR *d2 = opendir("/proc"); int any = 0; while (d2 && (e = readdir(d2))) { int pid = atoi(e->d_name); char p[64]; struct stat st; snprintf(p, sizeof p, "/proc/%d", pid); if (pid > 1 && !stat(p, &st) && st.st_uid == uid) { any = 1; break; } } if (d2) closedir(d2); if (!any) { left = 0; break; } }
		if (!left) break;
	}
	return left;
}

/* ---- the account's folder becomes its user's: what the desktop's user (1000) owns, ownership only */
static int au_mv_n, au_mv_fail, au_mv_to; static char au_mv_err[200];
static int au_mv_one(const char *p, const struct stat *st, int fl, struct FTW *fw)
{
	(void)fl; (void)fw;
	if (st->st_uid != 1000 && st->st_gid != 1000) return 0;
	if (lchown(p, st->st_uid == 1000 ? (uid_t)au_mv_to : st->st_uid, st->st_gid == 1000 ? (gid_t)au_mv_to : st->st_gid)) { if (!au_mv_fail++) snprintf(au_mv_err, sizeof au_mv_err, "%s", strerror(errno)); }
	else au_mv_n++;
	return 0;
}
static int au_move_folder(const char *id, unsigned uid, int *moved)
{
	char d[400]; snprintf(d, sizeof d, "%s/%s", home_root, id);
	au_mv_n = au_mv_fail = 0; au_mv_to = (int)uid; au_mv_err[0] = 0;
	int r = nftw(d, au_mv_one, 32, FTW_PHYS | FTW_MOUNT);
	struct stat st; if (!stat(d, &st) && st.st_uid == uid) { if (chmod(d, 0700)) au_mv_fail++; }
	if (moved) *moved = au_mv_n;
	return r || au_mv_fail ? -1 : 0;
}

/* ---- the users as the accounts are: after each lsync, and at start */
static int au_sync(void)
{
	if (!au_on) return 0;
	dls l; if (!dls_open(&l)) return 0;
	char *list = dls_accounts(&l); dls_close(&l); if (!list) return 0;
	au_ent e[AU_MAX]; int n = au_read(e, AU_MAX), changed = 0; unsigned next = AU_FIRST;
	for (int i = 0; i < n; i++) if (e[i].uid >= next) next = e[i].uid + 1;
	char ids[AU_MAX][80]; int ni = 0;
	for (int i = 0; ni < AU_MAX; i++) {
		const char *a = dj_at(list, i); if (!a) break;
		char *role = dj_str(dj_get(a, "r")); int guest = role && !strcmp(role, "guest"); free(role); if (guest) continue;
		char id[80]; dacct_id(a, id, sizeof id); if (!home_acct_ok(id) || !strcmp(id, "guest") || !strcmp(id, "shared")) continue;
		snprintf(ids[ni++], 80, "%s", id);
		if (!au_find(e, n, id) && n < AU_MAX && next <= AU_LAST) {
			memset(&e[n], 0, sizeof e[n]); snprintf(e[n].id, sizeof e[n].id, "%s", id); e[n].uid = next++;
			/* a new account: nothing to move (its folder is made by its user) */
			char d[400]; struct stat st; snprintf(d, sizeof d, "%s/%s", home_root, id); e[n].moved = stat(d, &st) != 0;
			if (au_install(&e[n])) health_add("account", "the Linux user for the account %.80s could not be written", id);
			else health_add("account", "the account %.80s has its own Linux user (acct%u)", id, e[n].uid);
			n++; changed = 1;
		}
	}
	free(list);
	/* accounts that are gone: their user stops, and is removed (its uid not given again) */
	for (int i = 0; i < n; i++) {
		if (e[i].retired) continue;
		int still = 0; for (int k = 0; k < ni; k++) if (!strcmp(ids[k], e[i].id)) still = 1;
		if (still) continue;
		au_kill_user(e[i].uid); au_uninstall(e[i].uid); e[i].retired = 1; changed = 1;
		health_add("account", "the account %.80s was removed, and its Linux user (acct%u) with it", e[i].id, e[i].uid);
	}
	if (changed && au_write(e, n)) health_add("account", "the list of the accounts' Linux users could not be kept");
	return changed;
}
/* at start: the users into /etc, then the move of existing accounts' folders (once each) */
static void au_start(void)
{
	if (!au_on) return;
	au_sync();
	au_ent e[AU_MAX]; int n = au_read(e, AU_MAX), changed = 0;
	for (int i = 0; i < n; i++) {
		if (e[i].retired) continue;
		au_install(&e[i]);
		if (e[i].moved) continue;
		char d[400]; struct stat st; snprintf(d, sizeof d, "%s/%s", home_root, e[i].id);
		if (stat(d, &st)) continue;   /* (not on the disk: nothing to move) */
		int k = 0;
		if (au_move_folder(e[i].id, e[i].uid, &k)) {
			health_add("account", "the files of the account %.80s could not all be given to its Linux user (acct%u): %d moved, %d not (%s); they stay readable to the account", e[i].id, e[i].uid, k, au_mv_fail, au_mv_err[0] ? au_mv_err : "?");
			char f[300]; snprintf(f, sizeof f, RUN "/account-move-failed"); write_str(f, e[i].id);
		} else { e[i].moved = 1; changed = 1; health_add("account", "the account %.80s's files belong to its own Linux user (acct%u) now: %d moved", e[i].id, e[i].uid, k); }
	}
	if (changed) au_write(e, n);
}

/* ---- the display (1.5.9, 15.3's second step): given to the account's user at sign-in, taken back at sign-out */
static char au_display[256] = "/usr/bin/rudolf-display";   /* (host fixtures choose) */
static int au_xaccess(const char *user, int allow)
{
	if (access(au_display, X_OK)) return 0;
	char c[500]; snprintf(c, sizeof c, "DISPLAY=:0 XAUTHORITY=" RUN "/xauth timeout 5 %s --%s %s >/dev/null 2>&1", au_display, allow ? "xallow" : "xdeny", user);
	int r = run(c);
	if (r) health_add("account", "the display could not be %s the Linux user %s", allow ? "given to" : "taken from", user);
	return r;
}

/* ---- the account's session: at sign-in with its files on the disk (home_bind), until sign-out (home_unbind) */
#define AU_ENV RUN "/acct-session.env"
#ifndef AU_KEYRING
#define AU_KEYRING "/usr/lib/rudolf/keyring"
#endif
#ifndef AU_KEYRING_DAEMON
#define AU_KEYRING_DAEMON "/usr/bin/gnome-keyring-daemon"
#endif
/* au_keep (rudolfd-home.inc): the same account bound again (rudolfd started again in its session): its programs go on */
static void au_session_stop(const char *id)
{
	if (au_keep) return;
	unsigned uid = au_uid(id); if (!uid) { unlink(AU_ENV); return; }
	unlink(AU_ENV);
	char xn[32]; au_name(uid, xn, sizeof xn); au_xaccess(xn, 0);
	int left = au_kill_user(uid);
	if (left) health_add("account", "%d programs of the account %.80s did not stop at sign-out", left, id);
	char nm[32], h[300], rd[300]; au_name(uid, nm, sizeof nm); snprintf(h, sizeof h, "%s/%s", au_homes, nm); snprintf(rd, sizeof rd, "%s/%u", au_runuser, uid);
	if (!home_nobind) umount2(h, MNT_DETACH);
	char c[400]; snprintf(c, sizeof c, "rm -rf '%s'", rd); run(c);
}
static int au_session_start(const char *id)
{
	unsigned uid = au_uid(id); if (!uid) return 0;
	char nm[32], h[300], src[400], rd[300]; au_name(uid, nm, sizeof nm);
	snprintf(h, sizeof h, "%s/%s", au_homes, nm); snprintf(src, sizeof src, "%s/%s", home_root, id); snprintf(rd, sizeof rd, "%s/%u", au_runuser, uid);
	/* (a folder made before its user existed: its user's now, quickly, as the start would) */
	{ struct stat st; if (!stat(src, &st) && st.st_uid != uid) { int k; au_move_folder(id, uid, &k); } }
	/* (the same session again: rudolfd was started again while the account was signed in) */
	if (au_keep) { char *ev = slurp(AU_ENV, NULL); char want[340]; snprintf(want, sizeof want, "HOME=%s\n", h); int same = ev && strstr(ev, want); free(ev); if (same) return 0; }
	mkdir(h, 0700); if (chown(h, uid, uid)) {} chmod(h, 0700);
	if (!home_nobind && mount(src, h, NULL, MS_BIND, NULL)) { health_add("account", "the account %.80s's folder could not be its Linux user's home (%s)", id, strerror(errno)); return -1; }
	mkdir(au_runuser, 0755); mkdir(rd, 0700); if (chown(rd, uid, uid)) {} chmod(rd, 0700);
	str e = {0};
	/* (1.6.0, Part 14: with the keyring, the desktop is also named GNOME's, after Rudolf: Electron and Chromium-based apps
	   keep their sign-ins in the Secret Service only for desktops they know, else in a plain file) */
	int kr = !home_nobind && !access(AU_KEYRING_DAEMON, X_OK) && !access(AU_KEYRING, X_OK);
	sadd(&e, "DISPLAY=:0\nHOME=%s\nUSER=%s\nLOGNAME=%s\nXDG_RUNTIME_DIR=%s\nXDG_CURRENT_DESKTOP=%s\nPULSE_SERVER=unix:" RUN "/pulse/native\n", h, nm, nm, rd, kr ? "Rudolf:GNOME" : "Rudolf");
	/* its own session D-Bus, and the desktop portal on it (file dialogs, links, notifications) */
	if (!home_nobind && !access("/usr/bin/dbus-daemon", X_OK)) {
		char c[900]; snprintf(c, sizeof c, "su -s /bin/sh %s -c 'dbus-daemon --session --fork --nopidfile --address=unix:path=%s/bus' >/dev/null 2>&1", nm, rd);
		if (!run(c)) sadd(&e, "DBUS_SESSION_BUS_ADDRESS=unix:path=%s/bus\n", rd);
	}
	char t[300]; snprintf(t, sizeof t, "%s.tmp", AU_ENV); int f = open(t, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	if (f >= 0) { if (write(f, e.b, e.n) < 0) {} close(f); rename(t, AU_ENV); }
	int bus = strstr(e.b, "DBUS_SESSION_BUS_ADDRESS") != NULL; free(e.b);
	au_xaccess(nm, 1);   /* (its programs may use the display: the Linux apps' windows) */
	if (!home_nobind && bus && !access("/usr/bin/rudolf-portal", X_OK)) {
		char c[600]; snprintf(c, sizeof c, "su -s /bin/sh %s -c 'set -a; . " AU_ENV "; set +a; exec setsid /usr/bin/rudolf-portal' </dev/null >>" RUN "/portal-%u.log 2>&1 &", nm, uid);
		run(c);
	}
	/* 1.6.0 (Part 14): the keyring for its Linux apps (the Secret Service), as its user, in its own folder */
	if (kr && bus) {
		char c[600]; snprintf(c, sizeof c, "su -s /bin/sh %s -c 'set -a; . " AU_ENV "; set +a; exec setsid " AU_KEYRING " start' </dev/null >>" RUN "/keyring-%u.log 2>&1 &", nm, uid);
		run(c);
	}
	return 0;
}
/* who runs a Linux app now: the signed-in account's user and its session's environment, else the desktop's */
static void au_app_user(char *user, size_t un, char *env, size_t en)
{
	snprintf(user, un, "rudolf"); snprintf(env, en, RUN "/session.env");
	if (!au_on || access(AU_ENV, R_OK)) return;
	char *cur = slurp(HOME_SESSION, NULL); if (!cur) return; trim(cur);
	unsigned uid = au_uid(cur); free(cur); if (!uid) return;
	au_name(uid, user, un); snprintf(env, en, AU_ENV);
}

/* p is below SUB in the signed-in account's home (its session running): Wine's prefix, the icons Wine makes */
static int au_home_has(const char *p, const char *sub)
{
	if (!au_on || !p || strstr(p, "/../") || access(AU_ENV, R_OK)) return 0;
	char *ev = slurp(AU_ENV, NULL); if (!ev) return 0;
	char *h = strstr(ev, "\nHOME="); int ok = 0;
	if (h) { h += 6; char *e = strchr(h, '\n'); if (e) *e = 0;
		char want[400]; snprintf(want, sizeof want, "%s%s", h, sub);
		size_t l = strlen(want); ok = *h == '/' && !strncmp(p, want, l); }
	free(ev); return ok;
}

/* GET accounts/users: each account's Linux user (System health) */
static void api_account_users(struct req *r)
{
	au_ent e[AU_MAX]; int n = au_read(e, AU_MAX); char *cur = slurp(HOME_SESSION, NULL); if (cur) trim(cur);
	char *mf = slurp(RUN "/account-move-failed", NULL); if (mf) trim(mf);
	str s = {0}; sadd(&s, "{\"ok\":true,\"on\":%s,\"users\":[", au_on ? "true" : "false"); int k = 0;
	for (int i = 0; i < n; i++) { if (e[i].retired) continue; char nm[32]; au_name(e[i].uid, nm, sizeof nm);
		sadd(&s, "%s{\"id\":", k++ ? "," : ""); sjson(&s, e[i].id); sadd(&s, ",\"uid\":%u,\"user\":\"%s\",\"moved\":%s,\"signedIn\":%s}", e[i].uid, nm,
			e[i].moved ? "true" : "false", cur && !strcmp(cur, e[i].id) && !access(AU_ENV, F_OK) ? "true" : "false"); }
	sadd(&s, "],\"moveFailed\":"); sjson(&s, mf ? mf : ""); sadd(&s, "}"); free(cur); free(mf); reply_json(r->fd, &s);
}
