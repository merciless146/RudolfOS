/* (included by rudolfd.c, not built on its own: hence .inc) ---------- 1.6.2 (docs/plan-1.6.2.md Part 40): the Windows
 * programs from before 1.5.9, brought into the account.
 * Until 1.5.9 Debian and Wine ran as the desktop's user, so every account's Windows programs were in one Wine prefix, the
 * desktop user's (/home/rudolf/.wine). Since 1.5.9 (rudolfd-users.inc, plan 15.3) they run as the signed-in account's own
 * Linux user, with the account's folder as Debian's home, so Wine makes the account a prefix of its own the first time it
 * runs there, and nothing of the old one is in it: au_migrate gives the account what the desktop's user owned inside the
 * account's folder (ownership only), never what is in the desktop user's own home. On the owner's Vivobook (2026-11-23)
 * Starport GE's launcher could not find its folder and nothing at all was shown.
 *
 *   GET  wine/old[?dir=/home/rudolf/.wine/...]   what there is to bring (below); dir: is that folder of a program only in
 *                                                the old prefix (the message of a program that did not start, rudolf-debian)
 *   POST wine/bring    body {"n","p"}: an administrator's name and password when the computer has several accounts
 *                      (the old prefix was every account's); the copy in the background, its progress in GET wine/old's "job"
 *   POST wine/decline  no more notices for this account (Settings › Apps › Linux still offers it)
 *
 * GET wine/old: {"on": the signed-in account runs Debian as its own user, "pending" (offered, not declined, nothing
 * running), "offer" (something to bring, declined or not), "declined", "several" (more than one account: an administrator
 * decides; the programs' names are then not given, only how many), "accounts", "programs" (names), "count", "launchers"
 * (Wine's Start menu entries the account lacks), "bytes" (the old prefix, all of it), "free" (on the account's disk), "room",
 * "mode" (below), "own" (the account's prefix has programs of its own), "cut" (an earlier copy was cut off: started again
 * from the beginning), "brought" (a finished copy waits to take its place), "running", "job" (the copy's progress, this
 * account's only), "inOld" (dir given)}.
 *
 * The prefix is copied whole (a program lives as much in the prefix's registry, system.reg and user.reg, as in drive_c, and two
 * registries cannot be merged safely), as a copy owned by the account's user (group 1000, as Wine's own files in Debian are),
 * the old one never changed:
 *   "replace": the account's prefix has no programs of its own (Wine made it fresh, as on the Vivobook) or there is none: the
 *              copy becomes ~/.wine, the account's own is kept beside it as ~/.wine.own-DATE (nothing is lost, and launchers
 *              that use $HOME/.wine, as Starport GE's /usr/local/bin/starport-ge, find their folders);
 *   "beside":  the account's prefix has programs of its own: it stays ~/.wine, the copy goes beside it as ~/.wine-shared, and
 *              Wine's launchers brought with it are pointed at it (WINEPREFIX);
 *   "launchers": every program is in the account already (copied by hand): only Wine's launchers and icons.
 * Wine's launchers (~/.local/share/applications/wine, wine-*), its menus (~/.config/menus/applications-merged/wine-*), their
 * folders (~/.local/share/desktop-directories/wine-*) and the icons they name (~/.local/share/icons) come too, never over
 * one of the account's own.
 * Safety: the copy is made by a child of rudolfd that reads as the desktop's user (setfsuid 1000: nothing that user could not
 * read) and writes as the account's user (setfsuid UID, setfsgid 1000: nothing that user could not write), through folders
 * opened one step at a time without following links (O_NOFOLLOW), so neither side's links lead it anywhere else. Room is
 * checked first. The copy is made in ~/.wine.bringing; only once it is whole is it named ~/.wine.brought, and only then is the
 * account's own prefix renamed and the copy put in its place (not while a Windows program of the account runs: its
 * wineserver); a copy cut off (power, a crash) leaves the account's prefix as it was, and is started again. */
#include <sys/fsuid.h>
#include <sys/file.h>
#ifndef WO_OLDHOME_DEF
#define WO_OLDHOME_DEF "/home/rudolf"
#endif
#ifndef WO_STATE_DEF
#define WO_STATE_DEF PERSIST "/etc/rudolf/wine-old"
#endif
#define WO_PROG RUN "/wine-bring.json"
#define WO_LOCK RUN "/wine-bring.lock"
#define WO_MARGIN (256LL << 20)   /* (room left over on the account's disk after the copy) */
static char wo_oldhome[256] = WO_OLDHOME_DEF, wo_state[256] = WO_STATE_DEF;   /* (host fixtures choose) */
static int wo_slow_us;   /* (fixtures/wineoldapi.c only: a pause per file, so a test can cut a copy off) */

/* ---- the programs in a prefix: folders of drive_c that are not Wine's own */
#define WO_MAXP 64
static const char *const WO_PF_SKIP[] = { "Common Files", "Internet Explorer", "Windows Media Player", "Windows NT", "Windows Photo Viewer",
	"Windows Mail", "Windows Sidebar", "Windows Defender", "Windows Journal", "Windows Portable Devices", "Microsoft.NET", "Uninstall Information", NULL };
static const char *const WO_USER_SKIP[] = { "AppData", "Application Data", "Contacts", "Cookies", "Desktop", "Documents", "Downloads", "Favorites",
	"Links", "Local Settings", "Music", "My Documents", "My Music", "My Pictures", "My Videos", "NetHood", "Pictures", "PrintHood", "Recent",
	"Saved Games", "Searches", "SendTo", "Start Menu", "Templates", "Temp", "Videos", NULL };
static const char *const WO_C_SKIP[] = { "Program Files", "Program Files (x86)", "ProgramData", "users", "windows", NULL };
static int wo_in(const char *const *l, const char *n) { for (int i = 0; l[i]; i++) if (!strcasecmp(l[i], n)) return 1; return 0; }
static void wo_dirs(const char *prefix, const char *sub, const char *const *skip, char (*out)[400], int *n, int max)
{
	char d[700]; snprintf(d, sizeof d, "%s/%s", prefix, sub); DIR *D = opendir(d); struct dirent *e;
	while (D && (e = readdir(D)) && *n < max) {
		if (e->d_name[0] == '.' || (skip && wo_in(skip, e->d_name))) continue;
		char p[1000]; struct stat st; snprintf(p, sizeof p, "%s/%s", d, e->d_name);
		if (lstat(p, &st) || !S_ISDIR(st.st_mode) || strlen(sub) + strlen(e->d_name) + 2 > 400) continue;
		snprintf(out[(*n)++], 400, "%s/%s", sub, e->d_name);
	}
	if (D) closedir(D);
}
static int wo_progs(const char *prefix, char (*out)[400], int max)
{
	int n = 0; wo_dirs(prefix, "drive_c/Program Files", WO_PF_SKIP, out, &n, max); wo_dirs(prefix, "drive_c/Program Files (x86)", WO_PF_SKIP, out, &n, max);
	wo_dirs(prefix, "drive_c", WO_C_SKIP, out, &n, max);
	char u[WO_MAXP][400]; int nu = 0; wo_dirs(prefix, "drive_c/users", NULL, u, &nu, WO_MAXP);
	for (int i = 0; i < nu; i++) wo_dirs(prefix, u[i], WO_USER_SKIP, out, &n, max);
	return n;
}
static int wo_isdir(const char *p) { struct stat st; return !lstat(p, &st) && S_ISDIR(st.st_mode); }
static long long wo_sum;
static int wo_sum_one(const char *p, const struct stat *st, int fl, struct FTW *fw) { (void)p; (void)fl; (void)fw; if (S_ISREG(st->st_mode)) wo_sum += st->st_size; return 0; }
static long long wo_bytes(const char *d) { wo_sum = 0; nftw(d, wo_sum_one, 32, FTW_PHYS); return wo_sum; }
/* Wine's launchers under D (its applications/wine folder), at most 6 deep: how many have no file of the same place under H */
static int wo_lmissing(const char *d, const char *h, int depth)
{
	if (depth > 6) return 0; DIR *D = opendir(d); struct dirent *e; int n = 0;
	while (D && (e = readdir(D))) {
		if (e->d_name[0] == '.') continue;
		char p[1200], q[1200]; struct stat st; snprintf(p, sizeof p, "%s/%s", d, e->d_name); snprintf(q, sizeof q, "%s/%s", h, e->d_name);
		if (lstat(p, &st)) continue;
		if (S_ISDIR(st.st_mode)) n += wo_lmissing(p, q, depth + 1);
		else if (S_ISREG(st.st_mode) && strlen(e->d_name) > 8 && !strcmp(e->d_name + strlen(e->d_name) - 8, ".desktop") && access(q, F_OK)) n++;
	}
	if (D) closedir(D); return n;
}

/* ---- who: the signed-in account, its user and its home (its session's HOME), when Debian and Wine run as it */
static int wo_acct(char *id, size_t idn, unsigned *uid, char *home, size_t hn)
{
	if (!au_on || access(AU_ENV, R_OK)) return -1;
	char *cur = slurp(HOME_SESSION, NULL); if (!cur) return -1; trim(cur);
	unsigned u = au_uid(cur); snprintf(id, idn, "%s", cur); free(cur); if (!u) return -1;
	char *ev = slurp(AU_ENV, NULL); if (!ev) return -1;
	char *h = strstr(ev, "\nHOME="); h = h ? h + 6 : !strncmp(ev, "HOME=", 5) ? ev + 5 : NULL;
	if (h) { char *e = strchr(h, '\n'); if (e) *e = 0; }
	char nm[32]; au_name(u, nm, sizeof nm); const char *b = h ? strrchr(h, '/') : NULL; struct stat st;
	int ok = h && *h == '/' && !strstr(h, "..") && b && !strcmp(b + 1, nm) && strlen(h) < hn && !stat(h, &st) && S_ISDIR(st.st_mode) && st.st_uid == u;
	if (ok) snprintf(home, hn, "%s", h);
	free(ev); *uid = u; return ok ? 0 : -1;
}
static int wo_several(void) { au_ent e[AU_MAX]; int n = au_read(e, AU_MAX), k = 0; for (int i = 0; i < n; i++) if (!e[i].retired) k++; return k; }
/* the account's line in the state file: "declined", "brought MODE TIME", or "" */
static void wo_state_get(const char *id, char *o, size_t n)
{
	o[0] = 0; char *t = slurp(wo_state, NULL); if (!t) return; size_t il = strlen(id);
	for (char *sv, *l = strtok_r(t, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) if (!strncmp(l, id, il) && l[il] == ' ') snprintf(o, n, "%s", l + il + 1);
	free(t);
}
static int wo_state_set(const char *id, const char *v)
{
	char *t = slurp(wo_state, NULL); str s = {0}; sadd(&s, "%s", ""); size_t il = strlen(id);
	for (char *sv, *l = t ? strtok_r(t, "\n", &sv) : NULL; l; l = strtok_r(NULL, "\n", &sv)) if (strncmp(l, id, il) || l[il] != ' ') sadd(&s, "%s\n", l);
	sadd(&s, "%s %s\n", id, v); free(t);
	char d[300]; snprintf(d, sizeof d, "%s", wo_state); char *sl = strrchr(d, '/'); if (sl) { *sl = 0; mkdir(d, 0755); }
	char tp[300]; snprintf(tp, sizeof tp, "%s.tmp", wo_state);
	int f = open(tp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644), ok = f >= 0 && write(f, s.b, s.n) == (ssize_t)s.n;
	if (f >= 0) close(f); free(s.b);
	if (!ok || rename(tp, wo_state)) { unlink(tp); return -1; }
	return 0;
}
static int wo_running(void)
{
	int f = open(WO_LOCK, O_RDONLY | O_CLOEXEC); if (f < 0) return 0;
	int busy = flock(f, LOCK_SH | LOCK_NB) && errno == EWOULDBLOCK; close(f); return busy;
}
/* a Windows program of the user running: its wineserver (Wine's own, one per prefix) */
static int wo_wine_runs(unsigned uid)
{
	DIR *d = opendir(AU_PROC); struct dirent *e; int r = 0;
	while (d && (e = readdir(d)) && !r) {
		int pid = atoi(e->d_name); if (pid <= 1) continue;
		char p[300]; struct stat st; snprintf(p, sizeof p, AU_PROC "/%d", pid); if (stat(p, &st) || st.st_uid != uid) continue;
		snprintf(p, sizeof p, AU_PROC "/%d/comm", pid); char *c = slurp(p, NULL); if (c) { trim(c); r = !strcmp(c, "wineserver"); free(c); }
	}
	if (d) closedir(d); return r;
}

/* ---- what there is to bring, for the signed-in account */
typedef struct {
	int on, several, accounts, count, launchers, own, cut, brought, running, declined;
	char id[80], home[300], mode[12], names[WO_MAXP][400];
	unsigned uid; long long bytes, free; int nnames;
} wo_info;
static void wo_look(wo_info *w)
{
	memset(w, 0, sizeof *w);
	if (wo_acct(w->id, sizeof w->id, &w->uid, w->home, sizeof w->home)) return;
	w->on = 1; w->accounts = wo_several(); w->several = w->accounts > 1; w->running = wo_running();
	char st[80]; wo_state_get(w->id, st, sizeof st); w->declined = !strcmp(st, "declined");
	char old[300], mine[400], shared[400], p[1200], q[1200]; snprintf(old, sizeof old, "%s/.wine", wo_oldhome);
	snprintf(mine, sizeof mine, "%s/.wine", w->home); snprintf(shared, sizeof shared, "%s/.wine-shared", w->home);
	snprintf(p, sizeof p, "%s/.wine.bringing", w->home); w->cut = wo_isdir(p) && !w->running;
	snprintf(p, sizeof p, "%s/.wine.brought", w->home); w->brought = wo_isdir(p);
	if (!wo_isdir(old)) return;
	static char op[WO_MAXP][400], mp[WO_MAXP][400]; int no = wo_progs(old, op, WO_MAXP), nm = wo_isdir(mine) ? wo_progs(mine, mp, WO_MAXP) : 0;
	for (int i = 0; i < nm; i++) { snprintf(p, sizeof p, "%s/%s", old, mp[i]); if (access(p, F_OK)) w->own = 1; }   /* (one the old prefix does not have) */
	for (int i = 0; i < no; i++) {
		snprintf(p, sizeof p, "%s/%s", mine, op[i]); snprintf(q, sizeof q, "%s/%s", shared, op[i]);
		if (!access(p, F_OK) || !access(q, F_OK)) continue;
		const char *b = strrchr(op[i], '/'); snprintf(w->names[w->nnames++], 400, "%s", b ? b + 1 : op[i]);
	}
	w->count = w->nnames;
	snprintf(p, sizeof p, "%s/.local/share/applications/wine", wo_oldhome); snprintf(q, sizeof q, "%s/.local/share/applications/wine", w->home);
	w->launchers = wo_lmissing(p, q, 0);
	snprintf(w->mode, sizeof w->mode, "%s", w->count || w->brought ? (!wo_isdir(mine) || !w->own ? "replace" : "beside") : w->launchers ? "launchers" : "");
	if (w->count || w->brought) w->bytes = w->brought ? 0 : wo_bytes(old);
	struct statvfs vf; if (!statvfs(w->home, &vf)) w->free = (long long)vf.f_bavail * vf.f_frsize;
}

/* ---- the copy (a child of rudolfd's, in the background) */
static unsigned wo_uid; static long long wo_total, wo_copied; static int wo_files, wo_skipped; static time_t wo_said; static char wo_err[300], wo_jid[80], wo_mode[12], wo_own[64];
static void wo_as_src(void) { setfsgid(1000); setfsuid(1000); }
static void wo_as_dst(void) { setfsgid(1000); setfsuid(wo_uid); }
static void wo_as_root(void) { setfsuid(0); setfsgid(0); }
static void wo_say(const char *phase, int running)
{
	int keep = setfsuid(-1); wo_as_root();
	str s = {0}; sadd(&s, "{\"id\":"); sjson(&s, wo_jid); sadd(&s, ",\"running\":%s,\"phase\":\"%s\",\"copied\":%lld,\"total\":%lld,\"files\":%d,\"skipped\":%d,\"mode\":\"%s\",\"own\":",
		running ? "true" : "false", phase, wo_copied, wo_total, wo_files, wo_skipped, wo_mode); sjson(&s, wo_own); sadd(&s, ",\"error\":"); sjson(&s, wo_err); sadd(&s, "}\n");
	int f = open(WO_PROG ".tmp", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644); if (f >= 0) { if (write(f, s.b, s.n) < 0) {} close(f); rename(WO_PROG ".tmp", WO_PROG); }
	free(s.b); wo_said = time(NULL);
	if (keep == 1000) wo_as_src(); else if (keep) wo_as_dst();   /* (as it was) */
}
static int wo_fail(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static int wo_fail(const char *fmt, ...) { if (wo_err[0]) return -1; va_list a; va_start(a, fmt); vsnprintf(wo_err, sizeof wo_err, fmt, a); va_end(a); return -1; }
/* NAME in dfd removed, whatever it is (as the account's user: a link is removed, never followed) */
static void wo_rm(int dfd, const char *name, int depth)
{
	wo_as_dst(); struct stat st; if (fstatat(dfd, name, &st, AT_SYMLINK_NOFOLLOW)) return;
	if (S_ISDIR(st.st_mode) && depth < 80) {
		int fd = openat(dfd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC); DIR *d = fd >= 0 ? fdopendir(fd) : NULL; struct dirent *e;
		if (!d && fd >= 0) close(fd);
		while (d && (e = readdir(d))) if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) wo_rm(dirfd(d), e->d_name, depth + 1);
		if (d) closedir(d);
		wo_as_dst(); unlinkat(dfd, name, AT_REMOVEDIR);
	} else unlinkat(dfd, name, 0);
}
static int wo_copyfile(int in, int out, int rewrite)
{
	static char b[1 << 20];
	if (rewrite) {   /* (a launcher of the copy put beside: WINEPREFIX pointed at it) */
		ssize_t n = read(in, b, sizeof b - 1); if (n < 0) return wo_fail("a file could not be read (%s)", strerror(errno)); b[n] = 0;
		static const char A[] = "WINEPREFIX=\"/home/rudolf/.wine\"", B[] = "WINEPREFIX=\"/home/rudolf/.wine-shared\"";
		str s = {0}; sadd(&s, "%s", ""); char *p = b, *f;
		while ((f = strstr(p, A))) { sadd(&s, "%.*s%s", (int)(f - p), p, B); p = f + sizeof A - 1; }
		sadd(&s, "%s", p); int ok = write(out, s.b, s.n) == (ssize_t)s.n; free(s.b); wo_copied += n;
		return ok ? 0 : wo_fail("a file could not be written (%s)", strerror(errno));
	}
	for (;;) {
		ssize_t n = read(in, b, sizeof b); if (n < 0) return wo_fail("a file could not be read (%s)", strerror(errno)); if (!n) break;
		for (ssize_t o = 0; o < n; ) { ssize_t w = write(out, b + o, n - o);
			if (w <= 0) return wo_fail(errno == ENOSPC || errno == EDQUOT ? "the disk is full" : "a file could not be written (%s)", strerror(errno)); o += w; }
		wo_copied += n; if (time(NULL) != wo_said) wo_say("copy", 1);
	}
	return 0;
}
/* one entry NAME of sfd into dfd: a folder (all of it), a file, a link (as a link); anything else is left out. no_over: an
   existing one is kept (the launchers) */
static int wo_copy(int sfd, int dfd, const char *name, int depth, int no_over, int rewrite)
{
	if (depth > 64) return wo_fail("the folders go too deep");
	if (wo_slow_us) usleep(wo_slow_us);
	wo_as_src(); struct stat st; if (fstatat(sfd, name, &st, AT_SYMLINK_NOFOLLOW)) { wo_skipped++; return 0; }
	if (S_ISDIR(st.st_mode)) {
		wo_as_dst(); if (mkdirat(dfd, name, 0700) && !(no_over && errno == EEXIST)) return wo_fail("a folder could not be made (%s)", strerror(errno));
		int nd = openat(dfd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC); if (nd < 0) return wo_fail("a folder could not be opened (%s)", strerror(errno));
		wo_as_src(); int ns = openat(sfd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC); DIR *d = ns >= 0 ? fdopendir(ns) : NULL; struct dirent *e;
		if (!d) { if (ns >= 0) close(ns); wo_skipped++; close(nd); return 0; }
		int r = 0;
		while (!r && (e = readdir(d))) if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) r = wo_copy(dirfd(d), nd, e->d_name, depth + 1, no_over, rewrite);
		closedir(d);
		if (!r) { struct timespec ts[2] = { st.st_atim, st.st_mtim }; if (fchmod(nd, (st.st_mode & 0777) | 0700)) {} if (futimens(nd, ts)) {} }
		close(nd); return r;
	}
	if (S_ISREG(st.st_mode)) {
		wo_as_src(); int in = openat(sfd, name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC); if (in < 0) { wo_skipped++; return 0; }
		wo_as_dst(); int out = openat(dfd, name, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
		if (out < 0) { close(in); if (no_over && errno == EEXIST) return 0; return wo_fail("a file could not be made (%s)", strerror(errno)); }
		size_t nl = strlen(name); int r = wo_copyfile(in, out, rewrite && nl > 8 && !strcmp(name + nl - 8, ".desktop"));
		struct timespec ts[2] = { st.st_atim, st.st_mtim };
		if (!r) { if (fchmod(out, st.st_mode & 0777)) {} if (futimens(out, ts)) {} wo_files++; }   /* (never set-user-ID or set-group-ID) */
		close(in); close(out); return r;
	}
	if (S_ISLNK(st.st_mode)) {
		char t[4096]; wo_as_src(); ssize_t n = readlinkat(sfd, name, t, sizeof t - 1); if (n < 0) { wo_skipped++; return 0; } t[n] = 0;
		wo_as_dst(); if (symlinkat(t, dfd, name) && !(no_over && errno == EEXIST)) return wo_fail("a link could not be made (%s)", strerror(errno));
		wo_files++; return 0;
	}
	wo_skipped++; return 0;
}
/* the folder REL (a/b/c) below fd, made where missing, as the account's user; its descriptor, or -1 */
static int wo_mkpath(int fd, const char *rel)
{
	char b[400]; snprintf(b, sizeof b, "%s", rel); int cur = dup(fd); wo_as_dst();
	for (char *sv, *c = strtok_r(b, "/", &sv); c && cur >= 0; c = strtok_r(NULL, "/", &sv)) {
		if (mkdirat(cur, c, 0700) && errno != EEXIST) { close(cur); return -1; }
		int n = openat(cur, c, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC); close(cur); cur = n;
	}
	return cur;
}
/* Wine's launchers, menus, their folders and the icons they name, from the desktop user's home into the account's */
static int wo_icon_ok(const char *v) { return *v && strlen(v) < 200 && !strchr(v, '/') && !strstr(v, ".."); }
static void wo_icons_of(const char *d, char (*ic)[200], int *n, int depth)
{
	if (depth > 6) return; wo_as_src(); DIR *D = opendir(d); struct dirent *e;
	while (D && (e = readdir(D)) && *n < 256) {
		if (e->d_name[0] == '.') continue; char p[1200]; struct stat st; snprintf(p, sizeof p, "%s/%s", d, e->d_name);
		if (lstat(p, &st)) continue;
		if (S_ISDIR(st.st_mode)) { wo_icons_of(p, ic, n, depth + 1); continue; }
		size_t l = strlen(e->d_name); if (!S_ISREG(st.st_mode) || l < 9 || strcmp(e->d_name + l - 8, ".desktop")) continue;
		char *t = slurp(p, NULL); for (char *sv, *ln = t ? strtok_r(t, "\n", &sv) : NULL; ln && *n < 256; ln = strtok_r(NULL, "\n", &sv))
			if (!strncmp(ln, "Icon=", 5)) { trim(ln + 5); if (wo_icon_ok(ln + 5)) snprintf(ic[(*n)++], 200, "%s", ln + 5); }
		free(t);
	}
	if (D) closedir(D);
}
static int wo_srcdir(const char *rel)
{
	char p[700]; snprintf(p, sizeof p, "%s/%s", wo_oldhome, rel); wo_as_src();
	return open(p, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);   /* (the desktop user's own home: as that user) */
}
static void wo_launchers(int hfd, int beside)
{
	static const char *const D[][2] = { { ".local/share/applications", "wine" }, { ".config/menus/applications-merged", "wine-" },
		{ ".local/share/desktop-directories", "wine-" } };
	for (size_t k = 0; k < sizeof D / sizeof D[0]; k++) {
		int s = wo_srcdir(D[k][0]); if (s < 0) continue;
		int d = -1; DIR *dir = fdopendir(s); struct dirent *e;
		while (dir && (e = readdir(dir))) {
			size_t pl = strlen(D[k][1]); if (strncmp(e->d_name, D[k][1], pl) || (pl == 4 && e->d_name[4] && e->d_name[4] != '-')) continue;   /* ("wine" and "wine-...") */
			if (d < 0 && (d = wo_mkpath(hfd, D[k][0])) < 0) break;
			wo_copy(dirfd(dir), d, e->d_name, 0, 1, beside); wo_err[0] = 0;   /* (a launcher that cannot come is left out, not the whole) */
		}
		if (dir) closedir(dir); else close(s); if (d >= 0) close(d);
	}
	char ic[256][200]; int ni = 0, sd = -1; char p[700]; snprintf(p, sizeof p, "%s/.local/share/applications", wo_oldhome); wo_icons_of(p, ic, &ni, 0);
	int s = ni ? wo_srcdir(".local/share/icons") : -1; if (s < 0) return;
	/* (icons/THEME/SIZE/apps/NAME.png|xpm|svg) */
	DIR *t = fdopendir(s); struct dirent *te;
	while (t && (te = readdir(t))) { if (te->d_name[0] == '.') continue;
		wo_as_src(); int tf = openat(dirfd(t), te->d_name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC); DIR *z = tf >= 0 ? fdopendir(tf) : NULL; struct dirent *ze;
		if (!z && tf >= 0) close(tf);
		while (z && (ze = readdir(z))) { if (ze->d_name[0] == '.') continue;
			for (int i = 0; i < ni; i++) for (int x = 0; x < 3; x++) {
				char rel[600], f[260]; snprintf(f, sizeof f, "%s.%s", ic[i], x == 0 ? "png" : x == 1 ? "xpm" : "svg");
				snprintf(rel, sizeof rel, "%s/apps", ze->d_name); wo_as_src(); int af = openat(dirfd(z), rel, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC); if (af < 0) continue;
				struct stat st; if (!fstatat(af, f, &st, AT_SYMLINK_NOFOLLOW) && S_ISREG(st.st_mode)) {
					char dr[700]; snprintf(dr, sizeof dr, ".local/share/icons/%s/%s/apps", te->d_name, ze->d_name);
					if ((sd = wo_mkpath(hfd, dr)) >= 0) { wo_copy(af, sd, f, 0, 1, 0); wo_err[0] = 0; close(sd); }
				}
				close(af);
			}
		}
		if (z) closedir(z);
	}
	if (t) closedir(t); else close(s);
}
static void wo_job(const char *id, unsigned uid, const char *home, const char *mode, long long total)
{
	wo_uid = uid; wo_total = total; snprintf(wo_jid, sizeof wo_jid, "%s", id); snprintf(wo_mode, sizeof wo_mode, "%s", mode); umask(0);
	int lk = open(WO_LOCK, O_WRONLY | O_CREAT | O_CLOEXEC, 0644); if (lk < 0 || flock(lk, LOCK_EX | LOCK_NB)) _exit(1);
	wo_say("copy", 1);
	int hfd = open(home, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC); struct stat hs;
	if (hfd < 0 || fstat(hfd, &hs) || hs.st_uid != uid) { wo_fail("the account's folder could not be opened"); goto out; }
	int replace = !strcmp(mode, "replace"), beside = !strcmp(mode, "beside");
	if (replace || beside) {
		wo_rm(hfd, ".wine.bringing", 0);   /* (a copy cut off before: started again) */
		wo_as_dst(); struct stat bs; int have = !fstatat(hfd, ".wine.brought", &bs, AT_SYMLINK_NOFOLLOW) && S_ISDIR(bs.st_mode);
		if (!have) {
			wo_rm(hfd, ".wine.brought", 0);
			int ofd = wo_srcdir(".wine"); if (ofd < 0) { wo_fail("the Windows programs from before could not be read"); goto out; }
			wo_as_dst(); if (mkdirat(hfd, ".wine.bringing", 0700)) { close(ofd); wo_fail("the copy's folder could not be made (%s)", strerror(errno)); goto out; }
			int dfd = openat(hfd, ".wine.bringing", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC); if (dfd < 0) { close(ofd); wo_fail("the copy's folder could not be opened"); goto out; }
			DIR *d = fdopendir(ofd); struct dirent *e; int r = 0;
			while (!r && d && (e = readdir(d))) if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) r = wo_copy(dirfd(d), dfd, e->d_name, 1, 0, 0);
			if (d) closedir(d); else close(ofd);
			if (!r) { wo_as_dst(); if (fchmod(dfd, 0755)) {} if (syncfs(dfd)) {} }
			close(dfd);
			if (r) { wo_rm(hfd, ".wine.bringing", 0); goto out; }   /* (the account's own prefix was never touched) */
			wo_as_dst(); if (renameat(hfd, ".wine.bringing", hfd, ".wine.brought")) { wo_fail("the copy could not be kept (%s)", strerror(errno)); goto out; }
		}
		wo_say("finish", 1);
		if (wo_wine_runs(uid)) { wo_fail("a Windows program of yours is running: close it, then choose Bring them in again (the copy is kept, so that is quick)"); goto out; }
		wo_as_dst();
		if (replace) {
			struct stat ms; if (!fstatat(hfd, ".wine", &ms, AT_SYMLINK_NOFOLLOW)) {
				time_t now = time(NULL); struct tm tm; localtime_r(&now, &tm); strftime(wo_own, sizeof wo_own, ".wine.own-%Y%m%d-%H%M%S", &tm);
				if (renameat(hfd, ".wine", hfd, wo_own)) { wo_fail("your own Wine folder could not be set aside (%s)", strerror(errno)); wo_own[0] = 0; goto out; }
			}
			if (renameat(hfd, ".wine.brought", hfd, ".wine")) { int e = errno; if (wo_own[0] && renameat(hfd, wo_own, hfd, ".wine")) {} wo_fail("the copy could not take its place (%s)", strerror(e)); wo_own[0] = 0; goto out; }
		} else {
			struct stat ss; if (!fstatat(hfd, ".wine-shared", &ss, AT_SYMLINK_NOFOLLOW)) { wo_fail("~/.wine-shared is there already"); goto out; }
			if (renameat(hfd, ".wine.brought", hfd, ".wine-shared")) { wo_fail("the copy could not be put beside yours (%s)", strerror(errno)); goto out; }
		}
		if (syncfs(hfd)) {}
	}
	wo_say("launchers", 1); wo_launchers(hfd, beside); wo_err[0] = 0;
	wo_as_root();
	{ char v[80]; snprintf(v, sizeof v, "brought %s %lld", mode, (long long)time(NULL)); wo_state_set(id, v); }
	health_add("account", "the Windows programs from before 1.5.9 were brought into the account %.80s (%s: %d files, %lld MB%s%s%s)", id, mode, wo_files, wo_copied >> 20,
		wo_skipped ? ", some could not be read" : "", wo_own[0] ? "; its own Wine folder kept as " : "", wo_own);
out:
	if (hfd >= 0) close(hfd);
	wo_say(wo_err[0] ? "error" : "done", 0);
	if (wo_err[0]) { wo_as_root(); health_add("account", "the Windows programs from before 1.5.9 were not brought into the account %.80s: %s", id, wo_err); }
	_exit(wo_err[0] ? 1 : 0);
}

/* ---- the requests */
static void wo_json(str *s, const wo_info *w, const char *dir)
{
	sadd(s, "{\"ok\":true,\"on\":%s", w->on ? "true" : "false");
	int offer = w->on && (w->count || w->launchers || w->brought);
	sadd(s, ",\"offer\":%s,\"pending\":%s,\"declined\":%s,\"several\":%s,\"accounts\":%d,\"count\":%d,\"launchers\":%d,\"bytes\":%lld,\"free\":%lld,\"room\":%s",
		offer ? "true" : "false", offer && !w->declined && !w->running ? "true" : "false", w->declined ? "true" : "false", w->several ? "true" : "false",
		w->accounts, w->count, w->launchers, w->bytes, w->free, w->free >= w->bytes + WO_MARGIN || w->brought ? "true" : "false");
	sadd(s, ",\"mode\":\"%s\",\"own\":%s,\"cut\":%s,\"brought\":%s,\"running\":%s,\"programs\":[", w->mode, w->own ? "true" : "false", w->cut ? "true" : "false",
		w->brought ? "true" : "false", w->running ? "true" : "false");
	for (int i = 0; i < w->nnames && !w->several; i++) { if (i) sadd(s, ","); sjson(s, w->names[i]); }   /* (several accounts: how many only) */
	sadd(s, "],\"job\":");
	char *j = w->on ? slurp(WO_PROG, NULL) : NULL; char jid[90]; snprintf(jid, sizeof jid, "{\"id\":\"%s\"", w->id);
	/* (its own "running" is what the copy last said; "alive", whether it still runs: a copy cut off said running and is not) */
	if (j && !strncmp(j, jid, strlen(jid))) { trim(j); size_t l = strlen(j); if (l && j[l - 1] == '}') j[l - 1] = 0; sadd(s, "%s,\"alive\":%s}", j, w->running ? "true" : "false"); }
	else sadd(s, "null");
	free(j);
	if (dir) {
		/* (the folder named in a program's message: /home/rudolf/.wine/REL, in the old prefix and in neither of the account's) */
		int in = 0; static const char W[] = "/home/rudolf/.wine/";
		if (w->on && !strncmp(dir, W, sizeof W - 1) && !strstr(dir, "..") && strlen(dir) < 700) {
			char a[1200], b[1200], c[1200]; const char *rel = dir + sizeof W - 1;
			snprintf(a, sizeof a, "%s/.wine/%s", wo_oldhome, rel); snprintf(b, sizeof b, "%s/.wine/%s", w->home, rel); snprintf(c, sizeof c, "%s/.wine-shared/%s", w->home, rel);
			in = *rel && !access(a, F_OK) && access(b, F_OK) && access(c, F_OK);
		}
		sadd(s, ",\"inOld\":%s", in ? "true" : "false");
	}
	sadd(s, "}");
}
static void api_wineold_old(struct req *r, int post)
{
	if (post) { reply_err(r->fd, 405, "GET"); return; }
	wo_info w; wo_look(&w); const char *d = qparam(r->query, "dir"); char dir[800]; if (d) snprintf(dir, sizeof dir, "%s", d);
	str s = {0}; wo_json(&s, &w, d ? dir : NULL); reply_json(r->fd, &s);
}
static void api_wineold_decline(struct req *r, int post)
{
	if (!post) { reply_err(r->fd, 405, "POST"); return; }
	wo_info w; wo_look(&w);
	if (!w.on) { reply_err(r->fd, 409, "no account is signed in with its own Linux user"); return; }
	if (wo_state_set(w.id, "declined")) { reply_err(r->fd, 500, "it could not be kept"); return; }
	reply_ok(r->fd, "declined");
}
static void api_wineold_bring(struct req *r, int post)
{
	if (!post) { reply_err(r->fd, 405, "POST"); return; }
	wo_info w; wo_look(&w);
	if (!w.on) { reply_err(r->fd, 409, "no account is signed in with its own Linux user"); return; }
	if (w.running) { reply_err(r->fd, 409, "they are being brought in already"); return; }
	if (!w.count && !w.launchers && !w.brought) { reply_err(r->fd, 409, "there is nothing from before to bring in"); return; }
	if (strcmp(w.mode, "launchers") && w.free < w.bytes + WO_MARGIN && !w.brought) {
		char m[200]; snprintf(m, sizeof m, "not enough room: the copy needs %.1f GB, and %.1f GB is free", (w.bytes + WO_MARGIN) / 1e9, w.free / 1e9);
		reply_err(r->fd, 507, m); return; }
	if (strcmp(w.mode, "launchers") && wo_wine_runs(w.uid)) { reply_err(r->fd, 409, "close your Windows programs first"); return; }
	char who[100] = "";
	if (w.several) {
		char why[200]; char *an = NULL, *ap = NULL; why[0] = 0;
		if (r->body && r->blen && r->body[0] == '{') { an = dbody_field(r, "n"); ap = dbody_field(r, "p"); }
		int adm = an && ap ? desk_admin_check(an, ap, why, sizeof why) : 403; dfree_secret(ap);
		if (adm) { free(an); reply_err(r->fd, adm == 429 ? 429 : 403, why[0] ? why : "this computer has several accounts: an administrator's name and password are needed"); return; }
		snprintf(who, sizeof who, "%.80s", an); free(an);
	}
	health_add("account", "the Windows programs from before 1.5.9 are being brought into the account %.80s (%s%s%s)", w.id, w.mode, who[0] ? ", allowed by the administrator " : "", who);
	unlink(WO_PROG);
	pid_t p = fork();
	if (p < 0) { reply_err(r->fd, 500, "it could not be started"); return; }
	if (!p) {
		if (fork()) _exit(0);
		setsid(); for (int i = 3; i < 1024; i++) close(i);
		int n = open("/dev/null", O_RDWR); if (n >= 0) { dup2(n, 0); dup2(n, 1); dup2(n, 2); if (n > 2) close(n); }
		wo_job(w.id, w.uid, w.home, w.mode, w.bytes);
	}
	waitpid(p, NULL, 0);
	str s = {0}; sadd(&s, "{\"ok\":true,\"started\":true,\"mode\":\"%s\"}", w.mode); reply_json(r->fd, &s);
}
