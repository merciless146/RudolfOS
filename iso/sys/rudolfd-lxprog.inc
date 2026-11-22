/* (included by rudolfd.c, not built on its own: hence .inc) ---------- 1.6.2 (docs/plan-1.6.2.md Part 25b): Linux apps'
 * progress on their taskbar buttons.
 *   POST portal/progress   {"ok":true,"apps":[{"id":"org.mozilla.firefox","wmclass":"firefox","v":42},...],"windows":[{"win":"1e00021","v":42}]}
 * The apps say it with the Unity LauncherEntry signal on the account's own session bus, where only that account's own
 * rudolf-portal hears it (rudolf-portal.c lp_*); the portal leaves a note per app beside its questions (PORTALD/req,
 * sticky: anyone may add, only the owner changes): "lp-UID-PORTALPID-N.prog", its owner's, 0600, holding "ID<tab>PERCENT".
 * Passed on here: only a note whose owner is the UID in its name, whose portal still runs (its process is that user's and
 * is rudolf-portal: as the apps' held requests, inhibit_scan), and whose user is the signed-in account's own (the user its
 * Linux apps run as: au_app_user); only a desktop file ID (letters, digits and . - _) and a whole number 0-100; a note
 * whose portal is gone is removed. Another account's notes are left alone and never listed. The ID's StartupWMClass, when
 * Flatpak exports its desktop file, goes with it, so the page and rudolf-native find its windows by the same rule
 * (lxprog62.js; rudolf-native.c lxp_*).
 * "windows": an app window's own progress on Standard, the XApp hint _NET_WM_XAPP_PROGRESS that rudolf-display reads from the
 * windows it manages (WINPROG: "ID<tab>PERCENT"; Firefox says its downloads so on X11): only a window ID (hex) and 1-100. Nothing is started (the narrow confinement's portal requests). */
#ifndef LXP_DIR
#define LXP_DIR PORTALD "/req"
#endif
#ifndef LXP_WINPROG
#define LXP_WINPROG RUN "/winprog"
#endif
#ifndef LXP_EXPORTS
#define LXP_EXPORTS "/var/lib/flatpak/exports/share/applications"
#endif
static long LXP_UID_TEST = -1;   /* (fixtures/lxprogapi.c only: the signed-in account's user) */
static long lxp_uid(void)
{
	if (LXP_UID_TEST >= 0) return LXP_UID_TEST;
	char au[32], ae[160]; au_app_user(au, sizeof au, ae, sizeof ae);
	struct passwd *pw = getpwnam(au); return pw ? (long)pw->pw_uid : -1;
}
static int lxp_id_ok(const char *s)
{
	size_t n = s ? strlen(s) : 0; if (!n || n > 120 || s[0] == '.' || s[0] == '-' || strstr(s, "..")) return 0;
	for (const char *c = s; *c; c++) if (!isalnum((unsigned char)*c) && *c != '.' && *c != '-' && *c != '_') return 0;
	return 1;
}
/* the app's StartupWMClass, from the desktop file Flatpak exports for it ("" when none, or not of the same characters) */
static void lxp_wmclass(const char *id, char *out, size_t n)
{
	out[0] = 0; char f[400]; if (strlen(id) > 120) return; snprintf(f, sizeof f, LXP_EXPORTS "/%.120s.desktop", id);
	int fd = open(f, O_RDONLY | O_CLOEXEC | O_NOFOLLOW); if (fd < 0) return;
	char b[16384]; ssize_t k = read(fd, b, sizeof b - 1); close(fd);
	if (k <= 0) return;
	b[k] = 0;
	int entry = 0;
	for (char *sv, *l = strtok_r(b, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) {
		if (l[0] == '[') { entry = !strncmp(l, "[Desktop Entry]", 15); continue; }
		if (entry && !strncmp(l, "StartupWMClass=", 15)) { char v[128]; snprintf(v, sizeof v, "%s", l + 15); v[strcspn(v, "\r")] = 0;
			if (lxp_id_ok(v)) snprintf(out, n, "%s", v);
			return; }
	}
}
static void api_lxprog(struct req *r)
{
	long au = lxp_uid(); DIR *d = opendir(LXP_DIR); struct dirent *e;
	struct { char id[128]; int sum, n; } a[32]; int na = 0;
	while (d && (e = readdir(d))) {
		unsigned uid, seq; long pid; char x;
		if (strncmp(e->d_name, "lp-", 3) || sscanf(e->d_name, "lp-%u-%ld-%u.prog%c", &uid, &pid, &seq, &x) != 3 || pid <= 1) continue;
		size_t nl = strlen(e->d_name); if (nl < 6 || strcmp(e->d_name + nl - 5, ".prog")) continue;
		char f[400]; snprintf(f, sizeof f, LXP_DIR "/%s", e->d_name); struct stat st;
		if (lstat(f, &st) || !S_ISREG(st.st_mode) || st.st_uid != uid || (st.st_mode & 077) || st.st_size > 200) continue;
		char pp[64]; snprintf(pp, sizeof pp, "/proc/%ld/comm", pid); FILE *c = fopen(pp, "r"); char comm[32] = "";
		if (c) { if (!fgets(comm, sizeof comm, c)) comm[0] = 0; fclose(c); }
		if (strncmp(comm, "rudolf-portal", 13) || stat(pp, &st) || st.st_uid != uid) { unlink(f); continue; }   /* (its portal is gone: so is its note) */
		if (au < 0 || (long)uid != au) continue;   /* (another account's: not this desktop's) */
		int fd = open(f, O_RDONLY | O_CLOEXEC | O_NOFOLLOW); if (fd < 0) continue;
		char t[256]; ssize_t k = read(fd, t, sizeof t - 1); close(fd); if (k <= 0) continue; t[k] = 0;
		char *tab = strchr(t, '\t'), *nlp = tab ? strchr(tab, '\n') : NULL; if (!tab || !nlp || nlp[1]) continue;
		*tab = 0; *nlp = 0; const char *v = tab + 1; size_t vl = strlen(v);
		if (!lxp_id_ok(t) || !vl || vl > 3 || strspn(v, "0123456789") != vl || atoi(v) > 100) continue;
		int i; for (i = 0; i < na && strcmp(a[i].id, t); i++) {}
		if (i == na) { if (na >= 32) continue; snprintf(a[na].id, sizeof a[na].id, "%.120s", t); a[na].sum = a[na].n = 0; na++; }
		a[i].sum += atoi(v); a[i].n++;   /* (the same app's progress from two of its programs: their average, as the page's) */
	}
	if (d) closedir(d);
	str s = {0}; sadd(&s, "{\"ok\":true,\"apps\":[");
	for (int i = 0; i < na; i++) { char wc[128]; lxp_wmclass(a[i].id, wc, sizeof wc);
		sadd(&s, "%s{\"id\":", i ? "," : ""); sjson(&s, a[i].id); sadd(&s, ",\"wmclass\":"); sjson(&s, wc); sadd(&s, ",\"v\":%d}", (a[i].sum + a[i].n / 2) / a[i].n); }
	sadd(&s, "],\"windows\":[");
	char *w = slurp(LXP_WINPROG, NULL); int nw = 0;
	for (char *sv, *l = w ? strtok_r(w, "\n", &sv) : NULL; l && nw < 64; l = strtok_r(NULL, "\n", &sv)) {
		char *tab = strchr(l, '\t'); if (!tab) continue; *tab = 0; const char *v = tab + 1; size_t il = strlen(l), vl = strlen(v);
		if (!il || il > 16 || strspn(l, "0123456789abcdef") != il || !vl || vl > 3 || strspn(v, "0123456789") != vl || atoi(v) < 1 || atoi(v) > 100) continue;
		sadd(&s, "%s{\"win\":", nw++ ? "," : ""); sjson(&s, l); sadd(&s, ",\"v\":%d}", atoi(v)); }
	free(w);
	sadd(&s, "]}"); reply_json(r->fd, &s);
}
