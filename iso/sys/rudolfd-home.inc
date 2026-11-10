/* (included by rudolfd.c, not built on its own: hence .inc) -------------------------------- 1.5.6: your files on the disk (docs/plan-1.5.6.md)
 * Part of rudolfd (included by rudolfd.c). Stage 1: each account's home folder as real files on RUDOLFDATA,
 * /persist/accounts/<account id>/, encrypted by the kernel (fscrypt v2, as the vault of 1.5.0) with that account's own
 * 64-byte file key. The desktop unwraps the key at sign-in and hands it over (POST home/unlock); rudolfd adds it to
 * the file system and, the first time, makes the folder with that key's policy. Signing out takes it out again
 * (home/lock): the names and contents are then unreadable, also to root.
 *
 * Every path is relative to the account's folder and is walked one component at a time with openat(O_NOFOLLOW), so
 * no "..", no link and no mount point leads out of it. A file's version ("v") is its modification time and size;
 * a write with an older base version is refused (409), so the desktop can ask which copy to keep.
 *
 *   GET  home                          {"supported","accounts":[{"id","unlocked"}],"session"}
 *   POST home/unlock?acct=ID[&session=1]   body: the key, 128 hex digits; session=1 also mounts the account's
 *                                          Documents, Pictures, Music and Videos over /home/rudolf/… for Linux apps
 *   POST home/lock?acct=ID
 *   GET  home/tree?acct=ID[&path=P]    every folder and file below P: [{"p","d","s","m","v"}]
 *   GET  home/list?acct=ID[&path=P]    one level
 *   GET  home/stat?acct=ID&path=P
 *   GET  home/read?acct=ID&path=P      the file (Range works, for video, PDFs and reading in pieces)
 *   GET  home/link?acct=ID&path=P      a /dlf/ link to it (stage 2), walked again at every request
 *   POST home/write?acct=ID&path=P[&base=V|new][&mkdirs=1][&part=1|&final=1]   body: the file (or the next piece)
 *   POST home/mkdir?acct=ID&path=P
 *   POST home/rename?acct=ID&from=P&to=Q[&replace=1]
 *   POST home/copy?acct=ID&from=P&to=Q
 *   POST home/delete?acct=ID&path=P    (for good; the Trash comes in stage 4)
 *   POST home/adopt?acct=ID&from=D&to=P   a large download (stage 2) moved from Downloads into the folder
 * Downloads and the Linux apps' Downloads folder stay as in 1.5.5 until stage 4 (Chromium's .incoming lives there). */
#define HOME_ROOT_DEF PERSIST "/accounts"
#define HOME_KEYS RUN "/home-keys"
#define HOME_SESSION RUN "/home-session"
/* 1.5.6: an account's folder name is its id (the desktop's "u" and 8 letters and digits, upper and lower case). The
   downloads' check (dl_acct_ok) takes lower case only (dlAcct() lowers the id), and refused most accounts here: QEMU
   t156 found that an account like uQppGToP7 never moved to the disk ("bad account" at each sign-in) */
static int home_acct_ok(const char *a)
{
	if (!a || !*a || strlen(a) > 64) return 0;
	for (const char *c = a; *c; c++) if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '-')) return 0;
	return 1;
}
static char home_root[256] = HOME_ROOT_DEF;
static const char *const HOME_SUB[] = { "Desktop", "Documents", "Downloads", "Music", "Pictures", "Videos", ".Trash", ".rudolf", NULL };
/* 1.5.6: Downloads too, when the downloads store has its own bind (dl_store_bind; the last entry, skipped without it) */
static const char *const HOME_BIND[] = { "Documents", "Pictures", "Music", "Videos", "Downloads", NULL };
static int home_bind_skip(int i) { return !strcmp(HOME_BIND[i], "Downloads") && !dl_store_bound; }
static int home_uid = 1000, home_nobind = 0;   /* (tests change these) */

static int home_name_ok(const char *c, size_t n)
{
	if (!n || n > 255 || (n == 1 && c[0] == '.') || (n == 2 && c[0] == '.' && c[1] == '.')) return 0;
	for (size_t i = 0; i < n; i++) if ((unsigned char)c[i] < 32 || c[i] == '/' || c[i] == 127) return 0;
	return 1;
}
/* a path inside the account's folder: names joined by single slashes; "" is the folder itself */
static int home_rel_ok(const char *p)
{
	if (!p) return 0;
	size_t L = strlen(p); if (L > 4000 || p[0] == '/') return 0;
	if (!L) return 1;
	const char *s = p;
	for (;;) { const char *e = strchr(s, '/'); size_t n = e ? (size_t)(e - s) : strlen(s); if (!home_name_ok(s, n)) return 0; if (!e) return 1; s = e + 1; }
}
static int home_root_fd(const char *acct)
{
	char d[400]; snprintf(d, sizeof d, "%s/%s", home_root, acct);
	return open(d, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
}
/* the folder holding rel's last name (opened, and checked to be a real folder at every step), and that name;
   mk: make missing folders on the way. Returns -1 (errno set) when a step is missing, a link or not a folder. */
static int home_parent(int root, const char *rel, char *last, size_t ln, int mk)
{
	int fd = dup(root); if (fd < 0) return -1;
	const char *s = rel;
	for (;;) {
		const char *e = strchr(s, '/');
		size_t n = e ? (size_t)(e - s) : strlen(s);
		if (!e) { if (n >= ln) { close(fd); errno = ENAMETOOLONG; return -1; } memcpy(last, s, n); last[n] = 0; return fd; }
		char c[256]; memcpy(c, s, n); c[n] = 0;
		int nf = openat(fd, c, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
		if (nf < 0 && mk && errno == ENOENT) { if (!mkdirat(fd, c, 0755) || errno == EEXIST) { fchownat(fd, c, home_uid, home_uid, AT_SYMLINK_NOFOLLOW); nf = openat(fd, c, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC); } }
		close(fd); if (nf < 0) return -1;
		fd = nf; s = e + 1;
	}
}
static int home_open_dir(int root, const char *rel)
{
	if (!*rel) return dup(root);
	char last[256]; int pf = home_parent(root, rel, last, sizeof last, 0); if (pf < 0) return -1;
	int f = openat(pf, last, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC); int e = errno; close(pf); errno = e; return f;
}
static void home_ver(const struct stat *st, char *v, size_t n) { snprintf(v, n, "%lld.%09ld-%lld", (long long)st->st_mtim.tv_sec, st->st_mtim.tv_nsec, (long long)st->st_size); }

/* ---- the key ---- */
static int home_policy_id(int dfd, unsigned char *id)   /* 1: encrypted (id filled), 0: not encrypted, -1: error */
{
	struct fscrypt_get_policy_ex_arg a; memset(&a, 0, sizeof a); a.policy_size = sizeof a.policy;
	if (ioctl(dfd, FS_IOC_GET_ENCRYPTION_POLICY_EX, &a)) return errno == ENODATA ? 0 : -1;
	if (a.policy.version != FSCRYPT_POLICY_V2) return -1;
	memcpy(id, a.policy.v2.master_key_identifier, FSCRYPT_KEY_IDENTIFIER_SIZE); return 1;
}
static int home_key_present(const unsigned char *id)
{
	int fd = open(home_root, O_RDONLY | O_DIRECTORY | O_CLOEXEC); if (fd < 0) return 0;
	struct fscrypt_get_key_status_arg a; memset(&a, 0, sizeof a); a.key_spec.type = FSCRYPT_KEY_SPEC_TYPE_IDENTIFIER; memcpy(a.key_spec.u.identifier, id, FSCRYPT_KEY_IDENTIFIER_SIZE);
	int r = ioctl(fd, FS_IOC_GET_ENCRYPTION_KEY_STATUS, &a); close(fd);
	return !r && a.status == FSCRYPT_KEY_STATUS_PRESENT;
}
static int home_unlocked(const char *acct)
{
	int d = home_root_fd(acct); if (d < 0) return 0;
	unsigned char id[FSCRYPT_KEY_IDENTIFIER_SIZE]; int p = home_policy_id(d, id); close(d);
	return p == 1 && home_key_present(id);
}
static int home_key_remove(const unsigned char *id, int *busy)
{
	int fd = open(home_root, O_RDONLY | O_DIRECTORY | O_CLOEXEC); if (fd < 0) return -1;
	struct fscrypt_remove_key_arg a; memset(&a, 0, sizeof a); a.key_spec.type = FSCRYPT_KEY_SPEC_TYPE_IDENTIFIER; memcpy(a.key_spec.u.identifier, id, FSCRYPT_KEY_IDENTIFIER_SIZE);
	int r = ioctl(fd, FS_IOC_REMOVE_ENCRYPTION_KEY, &a); close(fd);
	if (busy) *busy = !r && (a.removal_status_flags & FSCRYPT_KEY_REMOVAL_STATUS_FLAG_FILES_BUSY);
	return r;
}
/* 1.5.6: Debian (if running) sees the same folders: its mount namespace is its own (rudolf-debian rebind) */
static void home_debian_rebind(void)
{
	if (home_nobind || access("/usr/bin/rudolf-debian", X_OK)) return;
	if (run("/usr/bin/rudolf-debian rebind >/dev/null 2>&1")) fprintf(stderr, "rudolfd: Debian could not be given the account's folders\n");
}
static void home_unbind(void)
{
	char *cur = slurp(HOME_SESSION, NULL); if (!cur) return; free(cur);
	for (int i = 0; HOME_BIND[i]; i++) { char t[200]; snprintf(t, sizeof t, "/home/rudolf/%s", HOME_BIND[i]); umount2(t, MNT_DETACH); }
	unlink(HOME_SESSION);
	home_debian_rebind();
}
static void dl_store_tidy(const char *acct);
static int home_bind(const char *acct, char *err, size_t en)
{
	home_unbind();
	if (home_nobind) { write_str(HOME_SESSION, acct); return 0; }
	dl_store_bind();   /* before Downloads is covered by the account's */
	for (int i = 0; HOME_BIND[i]; i++) {
		if (home_bind_skip(i)) continue;
		char s[400], t[200]; snprintf(s, sizeof s, "%s/%s/%s", home_root, acct, HOME_BIND[i]); snprintf(t, sizeof t, "/home/rudolf/%s", HOME_BIND[i]);
		mkdir(t, 0755); chown(t, home_uid, home_uid);
		if (mount(s, t, NULL, MS_BIND, NULL)) { snprintf(err, en, "%s could not be shared with Linux apps (%s)", HOME_BIND[i], strerror(errno)); for (int j = 0; j < i; j++) { snprintf(t, sizeof t, "/home/rudolf/%s", HOME_BIND[j]); umount2(t, MNT_DETACH); } return -1; }
	}
	char line[80]; snprintf(line, sizeof line, "%s\n", acct); write_str(HOME_SESSION, line);
	if (dl_store_bound) dl_store_tidy(acct);
	home_debian_rebind();
	return 0;
}
/* 0 ok; otherwise an HTTP status and err */
static int home_unlock(const char *acct, const unsigned char *key, int session, char *err, size_t en, int *made)
{
	if (made) *made = 0;
	if (!home_acct_ok(acct) || !strcmp(acct, "guest") || !strcmp(acct, "shared")) { snprintf(err, en, "bad account"); return 400; }
	mkdir(home_root, 0711);
	int rfd = open(home_root, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (rfd < 0) { snprintf(err, en, "the folder for accounts could not be made"); return 500; }
	struct fscrypt_add_key_arg *a = calloc(1, sizeof *a + 64);
	if (!a) { close(rfd); snprintf(err, en, "out of memory"); return 500; }
	a->key_spec.type = FSCRYPT_KEY_SPEC_TYPE_IDENTIFIER; a->raw_size = 64; memcpy(a->raw, key, 64);
	int rc = ioctl(rfd, FS_IOC_ADD_ENCRYPTION_KEY, a); int er = errno;
	unsigned char id[FSCRYPT_KEY_IDENTIFIER_SIZE]; memcpy(id, a->key_spec.u.identifier, sizeof id);
	explicit_bzero(a, sizeof *a + 64); free(a);
	if (rc) { close(rfd); snprintf(err, en, "the key could not be added (%s)", er == ENOTTY || er == EOPNOTSUPP ? "the drive is not prepared for encryption" : strerror(er)); return er == ENOTTY || er == EOPNOTSUPP ? 409 : 500; }
	int d = openat(rfd, acct, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	if (d < 0 && errno == ENOENT) {
		/* the first time: the folder, with this key's policy, and the usual folders inside */
		if (mkdirat(rfd, acct, 0700)) { close(rfd); home_key_remove(id, NULL); snprintf(err, en, "the folder could not be made"); return 500; }
		fchownat(rfd, acct, home_uid, home_uid, 0);
		d = openat(rfd, acct, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
		struct fscrypt_policy_v2 p; memset(&p, 0, sizeof p);
		p.version = FSCRYPT_POLICY_V2; p.contents_encryption_mode = FSCRYPT_MODE_AES_256_XTS; p.filenames_encryption_mode = FSCRYPT_MODE_AES_256_CTS; p.flags = FSCRYPT_POLICY_FLAGS_PAD_32;
		memcpy(p.master_key_identifier, id, sizeof id);
		if (d < 0 || ioctl(d, FS_IOC_SET_ENCRYPTION_POLICY, &p)) { int pe = errno; if (d >= 0) close(d); unlinkat(rfd, acct, AT_REMOVEDIR); close(rfd); home_key_remove(id, NULL);
			snprintf(err, en, "the encrypted folder could not be made (%s)", pe == EOPNOTSUPP ? "the drive is not prepared for encryption" : strerror(pe)); return pe == EOPNOTSUPP ? 409 : 500; }
		for (int i = 0; HOME_SUB[i]; i++) { mkdirat(d, HOME_SUB[i], 0755); fchownat(d, HOME_SUB[i], home_uid, home_uid, 0); }
		if (made) *made = 1;
	} else if (d < 0) { close(rfd); home_key_remove(id, NULL); snprintf(err, en, "the account's folder could not be opened"); return 500; }
	else {
		unsigned char have[FSCRYPT_KEY_IDENTIFIER_SIZE]; int p = home_policy_id(d, have);
		if (p != 1 || memcmp(have, id, sizeof id)) {
			close(d); close(rfd);
			/* a key of another account is never taken out by mistake: only one that no other open account uses */
			if (!(p == 1 && !memcmp(have, id, sizeof id))) { int other = 0; DIR *dd = opendir(HOME_KEYS); struct dirent *de; char hx[2 * FSCRYPT_KEY_IDENTIFIER_SIZE + 1]; for (size_t i = 0; i < sizeof id; i++) sprintf(hx + 2 * i, "%02x", id[i]);
				while (dd && (de = readdir(dd))) { char f[300]; snprintf(f, sizeof f, HOME_KEYS "/%s", de->d_name); char *c = slurp(f, NULL); if (c && !strncmp(c, hx, 32) && strcmp(de->d_name, acct)) other = 1; free(c); }
				if (dd) closedir(dd);
				if (!other) home_key_remove(id, NULL); }
			snprintf(err, en, p == 1 ? "that key does not open this account's folder" : "this account's folder is not encrypted"); return 403;
		}
	}
	close(d); close(rfd);
	mkdir(HOME_KEYS, 0700);
	char f[300], hx[2 * FSCRYPT_KEY_IDENTIFIER_SIZE + 2]; for (size_t i = 0; i < sizeof id; i++) sprintf(hx + 2 * i, "%02x", id[i]); strcat(hx, "\n");
	snprintf(f, sizeof f, HOME_KEYS "/%s", acct); write_str(f, hx);
	if (session && home_bind(acct, err, en)) return 500;
	return 0;
}
static int home_lock(const char *acct, int *busy)
{
	if (!home_acct_ok(acct)) return 400;
	char *cur = slurp(HOME_SESSION, NULL); if (cur) { trim(cur); if (!strcmp(cur, acct)) home_unbind(); free(cur); }
	int d = home_root_fd(acct); if (d < 0) return 404;
	unsigned char id[FSCRYPT_KEY_IDENTIFIER_SIZE]; int p = home_policy_id(d, id); close(d);
	if (p != 1) return 404;
	sync();
	int r = home_key_remove(id, busy);
	char f[300]; snprintf(f, sizeof f, HOME_KEYS "/%s", acct); unlink(f);
	return r && errno != ENOKEY ? 500 : 0;
}

/* ---- listing ---- */
static void home_entry_json(str *s, int *n, const char *rel, const struct stat *st)
{
	char v[64]; home_ver(st, v, sizeof v);
	sadd(s, "%s{\"p\":", (*n)++ ? "," : ""); sjson(s, rel);
	sadd(s, ",\"d\":%d,\"s\":%lld,\"m\":%lld,\"v\":\"%s\"}", S_ISDIR(st->st_mode) ? 1 : 0, S_ISDIR(st->st_mode) ? 0LL : (long long)st->st_size,
		(long long)st->st_mtim.tv_sec * 1000 + st->st_mtim.tv_nsec / 1000000, v);
}
/* everything below dfd (rel: its path), depth first; at most lim entries; skips links and special files */
static int home_walk_json(int dfd, const char *rel, str *s, int *n, int lim, int deep)
{
	int fd2 = dup(dfd); DIR *d = fd2 < 0 ? NULL : fdopendir(fd2); if (!d) { if (fd2 >= 0) close(fd2); return 0; }
	struct dirent *de; int over = 0;
	while ((de = readdir(d))) {
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
		if (*n >= lim) { over = 1; break; }
		struct stat st; if (fstatat(dfd, de->d_name, &st, AT_SYMLINK_NOFOLLOW)) continue;
		if (!S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode)) continue;
		size_t L = strlen(rel) + strlen(de->d_name) + 2; char *p = malloc(L); if (!p) break;
		snprintf(p, L, "%s%s%s", rel, *rel ? "/" : "", de->d_name);
		home_entry_json(s, n, p, &st);
		if (deep && S_ISDIR(st.st_mode)) { int c = openat(dfd, de->d_name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC); if (c >= 0) { over |= home_walk_json(c, p, s, n, lim, deep); close(c); } }
		free(p); if (over) break;
	}
	closedir(d); return over;
}
/* ---- removing and copying (inside the account's folder; links are removed, never followed) ---- */
static int home_rm_at(int dfd, const char *name)
{
	struct stat st; if (fstatat(dfd, name, &st, AT_SYMLINK_NOFOLLOW)) return errno == ENOENT ? 0 : -1;
	if (!S_ISDIR(st.st_mode)) return unlinkat(dfd, name, 0);
	int c = openat(dfd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC); if (c < 0) return -1;
	int c2 = dup(c); DIR *d = c2 < 0 ? NULL : fdopendir(c2); struct dirent *de; int r = 0;
	while (d && (de = readdir(d))) { if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue; if (home_rm_at(c, de->d_name)) r = -1; }
	if (d) closedir(d); else if (c2 >= 0) close(c2);
	close(c); return r ? r : unlinkat(dfd, name, AT_REMOVEDIR);
}
static int home_cp_at(int sd, const char *sn, int dd, const char *dn)
{
	struct stat st; if (fstatat(sd, sn, &st, AT_SYMLINK_NOFOLLOW)) return -1;
	if (S_ISDIR(st.st_mode)) {
		if (mkdirat(dd, dn, 0755) && errno != EEXIST) return -1;
		fchownat(dd, dn, home_uid, home_uid, AT_SYMLINK_NOFOLLOW);
		int a = openat(sd, sn, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC), b = openat(dd, dn, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
		int r = (a < 0 || b < 0) ? -1 : 0; int a2 = a < 0 ? -1 : dup(a); DIR *d = a2 < 0 ? NULL : fdopendir(a2); struct dirent *de;
		while (!r && d && (de = readdir(d))) { if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue; if (home_cp_at(a, de->d_name, b, de->d_name)) r = -1; }
		if (d) closedir(d);
		if (a >= 0) close(a);
		if (b >= 0) close(b);
		return r;
	}
	if (!S_ISREG(st.st_mode)) return 0;
	int in = openat(sd, sn, O_RDONLY | O_NOFOLLOW | O_CLOEXEC); if (in < 0) return -1;
	int out = openat(dd, dn, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0644); if (out < 0) { close(in); return -1; }
	char buf[65536]; ssize_t n; int r = 0;
	while ((n = read(in, buf, sizeof buf)) > 0) if (write(out, buf, n) != n) { r = -1; break; }
	if (n < 0) r = -1;
	fchown(out, home_uid, home_uid); struct timespec ts[2] = { st.st_atim, st.st_mtim }; futimens(out, ts);
	close(in); if (close(out)) r = -1; if (r) unlinkat(dd, dn, 0); return r;
}
/* ---- writing: a temporary file next to it, then a rename (a power cut leaves the old file or the new one) ---- */
static int home_write_at(int pf, const char *name, const void *buf, size_t n, int part, int fin, const char *base, char *v, size_t vn, char *err, size_t en)
{
	struct stat st; int have = !fstatat(pf, name, &st, AT_SYMLINK_NOFOLLOW);
	if (have && !S_ISREG(st.st_mode)) { snprintf(err, en, "a folder or another kind of item has that name"); return 409; }
	if (base && *base && !part) {
		if (!strcmp(base, "new")) { if (have) { home_ver(&st, v, vn); snprintf(err, en, "a file with that name already exists"); return 409; } }
		else if (have) { char cur[64]; home_ver(&st, cur, sizeof cur); if (strcmp(cur, base)) { snprintf(v, vn, "%s", cur); snprintf(err, en, "the file was changed meanwhile"); return 409; } }
	}
	char tmp[300]; snprintf(tmp, sizeof tmp, ".%.200s.rudolf-part", name);
	if (part) {   /* the next piece of a large file */
		int f = openat(pf, tmp, O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW | O_CLOEXEC, 0644); if (f < 0) { snprintf(err, en, "%s", strerror(errno)); return 500; }
		fchown(f, home_uid, home_uid);
		if (n && write(f, buf, n) != (ssize_t)n) { int e = errno; close(f); snprintf(err, en, "%s", e == ENOSPC ? "the disk is full" : strerror(e)); return e == ENOSPC ? 507 : 500; }
		close(f); snprintf(v, vn, "part"); return 0;
	}
	if (!fin) {
		int f = openat(pf, tmp, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0644); if (f < 0) { snprintf(err, en, "%s", strerror(errno)); return 500; }
		fchown(f, home_uid, home_uid);
		if (n && write(f, buf, n) != (ssize_t)n) { int e = errno; close(f); unlinkat(pf, tmp, 0); snprintf(err, en, "%s", e == ENOSPC ? "the disk is full" : strerror(e)); return e == ENOSPC ? 507 : 500; }
		fsync(f); close(f);
	} else if (n) {   /* the last piece, given with final=1 */
		int f = openat(pf, tmp, O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW | O_CLOEXEC, 0644); if (f < 0 || write(f, buf, n) != (ssize_t)n) { if (f >= 0) close(f); snprintf(err, en, "the last piece could not be written"); return 500; }
		fsync(f); close(f);
	}
	if (renameat(pf, tmp, pf, name)) { snprintf(err, en, "%s", strerror(errno)); return 500; }
	if (fstatat(pf, name, &st, AT_SYMLINK_NOFOLLOW)) { snprintf(err, en, "the file is not there after writing"); return 500; }
	home_ver(&st, v, vn); return 0;
}

static void api_home(struct req *r, const char *what, int post)
{
	if (!*what && !post) {
		str s = {0}; int sup = vault_feature();
		sadd(&s, "{\"ok\":true,\"supported\":%s,\"accounts\":[", sup ? "true" : "false");
		DIR *d = opendir(home_root); struct dirent *de; int n = 0;
		while (d && (de = readdir(d))) { if (!home_acct_ok(de->d_name)) continue; sadd(&s, "%s{\"id\":\"%s\",\"unlocked\":%s}", n++ ? "," : "", de->d_name, home_unlocked(de->d_name) ? "true" : "false"); }
		if (d) closedir(d);
		char *cur = slurp(HOME_SESSION, NULL); if (cur) trim(cur);
		sadd(&s, "],\"session\":"); sjson(&s, cur && *cur ? cur : "");
		/* 1.5.6: Downloads is among the session's binds (the downloads store has its own, dl_store_bind) */
		sadd(&s, ",\"downloadsBound\":%s}", cur && *cur && dl_store_bound && !home_nobind ? "true" : "false"); free(cur);
		reply_json(r->fd, &s); return;
	}
	char acct[80]; { const char *a = qparam(r->query, "acct"); snprintf(acct, sizeof acct, "%s", a ? a : ""); }
	if (!home_acct_ok(acct)) { reply_err(r->fd, 400, "bad account"); return; }
	if (!strcmp(what, "unlock")) {
		if (!post) { reply_err(r->fd, 400, "use POST"); return; }
		const char *sq = qparam(r->query, "session"); int session = sq && !strcmp(sq, "1");
		unsigned char key[64]; const char *h = r->body ? r->body : "";
		if (r->blen < 128) { reply_err(r->fd, 400, "bad key"); return; }
		for (int i = 0; i < 64; i++) { int a = hexval(h[2 * i]), b = hexval(h[2 * i + 1]); if (a < 0 || b < 0) { explicit_bzero(key, sizeof key); reply_err(r->fd, 400, "bad key"); return; } key[i] = a << 4 | b; }
		explicit_bzero(r->body, r->blen);
		char err[200] = ""; int made = 0, st = home_unlock(acct, key, session, err, sizeof err, &made); explicit_bzero(key, sizeof key);
		if (st) { if (st == 403) health_add("vault", "an account's folder was not opened: the key did not match"); reply_err(r->fd, st, err); return; }
		/* stage 4: "made" when the folder is new (the desktop then fills it from a restored backup, if it has one) */
		reply_ok(r->fd, made ? "made" : "unlocked"); return;
	}
	if (!strcmp(what, "lock")) {
		if (!post) { reply_err(r->fd, 400, "use POST"); return; }
		int busy = 0, st = home_lock(acct, &busy);
		if (st == 404) { reply_ok(r->fd, "not open"); return; }
		if (st) { reply_err(r->fd, 500, "the folder could not be locked"); return; }
		reply_ok(r->fd, busy ? "locked; some files were still open and stay readable until they are closed" : "locked"); return;
	}
	if (!home_unlocked(acct)) { reply_err(r->fd, 423, "the account's folder is locked"); return; }
	int root = home_root_fd(acct); if (root < 0) { reply_err(r->fd, 404, "no folder for that account"); return; }
	char p1[4096], p2[4096];
	{ const char *a = qparam(r->query, "path"); snprintf(p1, sizeof p1, "%s", a ? a : ""); }
	if (!home_rel_ok(p1)) { close(root); reply_err(r->fd, 400, "bad path"); return; }
	if (!strcmp(what, "tree") || !strcmp(what, "list")) {
		int d = home_open_dir(root, p1); close(root);
		if (d < 0) { reply_err(r->fd, 404, "no such folder"); return; }
		str s = {0}; int n = 0; sadd(&s, "{\"ok\":true,\"items\":[");
		int over = home_walk_json(d, p1, &s, &n, 200000, !strcmp(what, "tree")); close(d);
		sadd(&s, "],\"more\":%s}", over ? "true" : "false"); reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "stat") || !strcmp(what, "read")) {
		if (!*p1) { close(root); reply_err(r->fd, 400, "no file"); return; }
		char last[256]; int pf = home_parent(root, p1, last, sizeof last, 0); close(root);
		if (pf < 0) { reply_err(r->fd, 404, "not found"); return; }
		struct stat st; if (fstatat(pf, last, &st, AT_SYMLINK_NOFOLLOW) || (!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode))) { close(pf); reply_err(r->fd, 404, "not found"); return; }
		if (!strcmp(what, "stat")) { str s = {0}; int n = 0; sadd(&s, "{\"ok\":true,\"item\":"); home_entry_json(&s, &n, p1, &st); sadd(&s, "}"); close(pf); reply_json(r->fd, &s); return; }
		if (S_ISDIR(st.st_mode)) { close(pf); reply_err(r->fd, 400, "that is a folder"); return; }
		int f = openat(pf, last, O_RDONLY | O_NOFOLLOW | O_CLOEXEC); close(pf);
		if (f < 0) { reply_err(r->fd, 404, "not found"); return; }
		serve_fd_as(r, f, last, 1); return;
	}
	if (!strcmp(what, "hash")) {
		/* 1.5.6: SHA-256 of a file in the account's folder, for File Explorer's Find duplicates (read in place,
		   through the folder's own descriptor, as read is) */
		if (!*p1) { close(root); reply_err(r->fd, 400, "no file"); return; }
		char last[256]; int pf = home_parent(root, p1, last, sizeof last, 0); close(root);
		if (pf < 0) { reply_err(r->fd, 404, "not found"); return; }
		int f = openat(pf, last, O_RDONLY | O_NOFOLLOW); close(pf);
		struct stat st; if (f < 0 || fstat(f, &st) || !S_ISREG(st.st_mode)) { if (f >= 0) close(f); reply_err(r->fd, 404, "not found"); return; }
		char cmd[64]; snprintf(cmd, sizeof cmd, "sha256sum </proc/self/fd/%d 2>/dev/null", f);
		char *h = run_out(cmd); close(f);
		if (!h || strlen(h) < 64) { free(h); reply_err(r->fd, 500, "could not read the file"); return; }
		h[64] = 0; for (int i = 0; i < 64; i++) if (!isxdigit((unsigned char)h[i])) { free(h); reply_err(r->fd, 500, "could not read the file"); return; }
		str s = {0}; sadd(&s, "{\"ok\":true,\"sha256\":\"%s\",\"size\":%lld}", h, (long long)st.st_size); free(h); reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "link")) {
		/* stage 2: a link (/dlf/<id>, as Downloads' files have) for <video>, <img> and the PDF viewer. It holds the
		   account and the path, not a resolved path: each request walks the folder again (home_serve_link), so a link
		   a Linux app puts in place of the file afterwards still leads nowhere */
		if (!*p1) { close(root); reply_err(r->fd, 400, "no file"); return; }
		char last[256]; int pf = home_parent(root, p1, last, sizeof last, 0); close(root);
		struct stat st; if (pf < 0 || fstatat(pf, last, &st, AT_SYMLINK_NOFOLLOW) || !S_ISREG(st.st_mode)) { if (pf >= 0) close(pf); reply_err(r->fd, 404, "not found"); return; }
		close(pf);
		unsigned char rb[16]; char id[33]; int rf = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
		if (rf < 0 || read(rf, rb, 16) != 16) { if (rf >= 0) close(rf); reply_err(r->fd, 500, "no randomness"); return; }
		close(rf); for (int i = 0; i < 16; i++) sprintf(id + i * 2, "%02x", rb[i]);
		mkdir(RUN "/dlf", 0700);
		char lf[128]; snprintf(lf, sizeof lf, RUN "/dlf/%s", id);
		size_t L = strlen(acct) + strlen(p1) + 16; char *c = malloc(L); snprintf(c, L, "home:%s:%s\n", acct, p1); write_str(lf, c); free(c);
		str s = {0}; sadd(&s, "{\"ok\":true,\"url\":\"/dlf/%s\"}", id); reply_json(r->fd, &s); return;
	}
	if (!post) { close(root); reply_err(r->fd, 400, "use POST"); return; }
	if (!strcmp(what, "append")) {
		/* 1.5.7: the desktop's Event Viewer events, kept in the account's folder (.rudolf/events.log); only a .log
		   file directly in .rudolf, a line or a few at a time, kept to 256 KiB and the one before it (.1) */
		size_t pl = strlen(p1);
		if (strncmp(p1, ".rudolf/", 8) || strchr(p1 + 8, '/') || pl < 13 || pl > 60 || strcmp(p1 + pl - 4, ".log") || p1[8] == '.') { close(root); reply_err(r->fd, 400, "only .rudolf/*.log"); return; }
		if (r->blen > 16384) { close(root); reply_err(r->fd, 413, "too long"); return; }
		char last[256]; int pf = home_parent(root, p1, last, sizeof last, 1); close(root);
		if (pf < 0) { reply_err(r->fd, 404, "the folder is not there"); return; }
		struct stat st;
		if (!fstatat(pf, last, &st, AT_SYMLINK_NOFOLLOW)) {
			if (!S_ISREG(st.st_mode)) { close(pf); reply_err(r->fd, 409, "not a file"); return; }
			if (st.st_size > 262144) { char old1[300]; snprintf(old1, sizeof old1, "%s.1", last); renameat(pf, last, pf, old1); }
		}
		int f = openat(pf, last, O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW | O_CLOEXEC, 0600);
		if (f < 0) { close(pf); reply_err(r->fd, 500, "could not open the log"); return; }
		if (fchown(f, home_uid, home_uid)) {}
		int bad = r->blen && write(f, r->body, r->blen) != (ssize_t)r->blen;
		if (!bad && r->blen && r->body[r->blen - 1] != '\n') bad = write(f, "\n", 1) != 1;
		close(f); close(pf);
		if (bad) { reply_err(r->fd, 507, "could not write"); return; }
		reply_ok(r->fd, "ok"); return;
	}
	if (!strcmp(what, "write")) {
		if (!*p1) { close(root); reply_err(r->fd, 400, "no file"); return; }
		char base[80] = ""; int mk = 0, part = 0, fin = 0;
		{ const char *a = qparam(r->query, "base"); snprintf(base, sizeof base, "%s", a ? a : ""); }
		{ const char *a = qparam(r->query, "mkdirs"); mk = a && !strcmp(a, "1"); }
		{ const char *a = qparam(r->query, "part"); part = a && !strcmp(a, "1"); }
		{ const char *a = qparam(r->query, "final"); fin = a && !strcmp(a, "1"); }
		char last[256]; int pf = home_parent(root, p1, last, sizeof last, mk); close(root);
		if (pf < 0) { reply_err(r->fd, 404, "the folder is not there"); return; }
		char v[80] = "", err[200] = ""; int st = home_write_at(pf, last, r->body, r->blen, part, fin, base, v, sizeof v, err, sizeof err); close(pf);
		if (st == 409) { str s = {0}; sadd(&s, "{\"ok\":false,\"conflict\":true,\"error\":"); sjson(&s, err); sadd(&s, ",\"v\":"); sjson(&s, v); sadd(&s, "}"); reply(r->fd, 409, "application/json", s.b, s.n, NULL); free(s.b); return; }
		if (st) { reply_err(r->fd, st, err); return; }
		str s = {0}; sadd(&s, "{\"ok\":true,\"v\":"); sjson(&s, v); sadd(&s, "}"); reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "adopt")) {
		/* stage 2: a large download (kept in Downloads, /home/rudolf/Downloads[/.accounts/<this account>]/name) moves
		   into the account's folder; across the vault's encryption a rename is refused (EXDEV), so it is copied */
		char from[1024]; { const char *a = qparam(r->query, "from"); snprintf(from, sizeof from, "%s", a ? a : ""); }
		{ const char *a = qparam(r->query, "to"); snprintf(p2, sizeof p2, "%s", a ? a : ""); }
		{ char fb[1024]; const char *f2 = dl_in(from, fb, sizeof fb); if (f2 != from) snprintf(from, sizeof from, "%s", fb); }
		/* the account's downloads folder is its id in lower case (the desktop's dlAcct()); QEMU t156: uQppGToP7's
		   large download was refused as "bad path", and the whole move with it */
		char lac[80]; size_t li = 0; for (; acct[li] && li < sizeof lac - 1; li++) lac[li] = tolower((unsigned char)acct[li]); lac[li] = 0;
		char mine[200]; snprintf(mine, sizeof mine, "%s/%s/", DLACCTS, lac);
		size_t lh = strlen(DLHOME); int top = !strncmp(from, DLHOME, lh) && from[lh] == '/' && !strchr(from + lh + 1, '/');
		if (!dl_path_ok(from) || !(top || !strncmp(from, mine, strlen(mine))) || !*p2 || !home_rel_ok(p2)) { close(root); reply_err(r->fd, 400, "bad path"); return; }
		struct stat fs1; if (lstat(from, &fs1) || !S_ISREG(fs1.st_mode)) { close(root); reply_err(r->fd, 404, "the download is not there"); return; }
		char last[256]; int pf = home_parent(root, p2, last, sizeof last, 1); close(root);
		if (pf < 0) { reply_err(r->fd, 404, "the folder is not there"); return; }
		struct stat st; if (!fstatat(pf, last, &st, AT_SYMLINK_NOFOLLOW)) home_rm_at(pf, last);
		int rc = renameat(AT_FDCWD, from, pf, last);
		if (rc && errno == EXDEV) {
			char dir[1024]; snprintf(dir, sizeof dir, "%s", from); char *sl = strrchr(dir, '/'); *sl = 0;
			int df = open(dir, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
			rc = df < 0 ? -1 : home_cp_at(df, sl + 1, pf, last);
			if (!rc) unlinkat(df, sl + 1, 0);
			if (df >= 0) close(df);
		} else if (!rc) fchownat(pf, last, home_uid, home_uid, AT_SYMLINK_NOFOLLOW);
		int e = errno; char v[80] = "";
		if (!rc && !fstatat(pf, last, &st, AT_SYMLINK_NOFOLLOW)) home_ver(&st, v, sizeof v);
		close(pf);
		if (rc) { reply_err(r->fd, e == ENOSPC ? 507 : 500, e == ENOSPC ? "the disk is full" : "the download could not be moved"); return; }
		str s = {0}; sadd(&s, "{\"ok\":true,\"v\":"); sjson(&s, v); sadd(&s, "}"); reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "mkdir")) {
		if (!*p1) { close(root); reply_err(r->fd, 400, "no folder"); return; }
		char last[256]; int pf = home_parent(root, p1, last, sizeof last, 1); close(root);
		if (pf < 0) { reply_err(r->fd, 404, "the folder is not there"); return; }
		int rc = mkdirat(pf, last, 0755); int e = errno; if (!rc) fchownat(pf, last, home_uid, home_uid, AT_SYMLINK_NOFOLLOW); close(pf);
		if (rc && e != EEXIST) { reply_err(r->fd, 500, strerror(e)); return; }
		reply_ok(r->fd, rc ? "already there" : "made"); return;
	}
	if (!strcmp(what, "delete")) {
		if (!*p1) { close(root); reply_err(r->fd, 400, "the account's folder itself cannot be deleted"); return; }
		char last[256]; int pf = home_parent(root, p1, last, sizeof last, 0); close(root);
		if (pf < 0) { reply_ok(r->fd, "not there"); return; }
		int rc = home_rm_at(pf, last); close(pf);
		if (rc) { reply_err(r->fd, 500, "it could not all be deleted"); return; }
		reply_ok(r->fd, "deleted"); return;
	}
	if (!strcmp(what, "rename") || !strcmp(what, "copy")) {
		{ const char *a = qparam(r->query, "from"); snprintf(p1, sizeof p1, "%s", a ? a : ""); }
		{ const char *a = qparam(r->query, "to"); snprintf(p2, sizeof p2, "%s", a ? a : ""); }
		const char *rq = qparam(r->query, "replace"); int replace = rq && !strcmp(rq, "1");
		if (!*p1 || !*p2 || !home_rel_ok(p1) || !home_rel_ok(p2)) { close(root); reply_err(r->fd, 400, "bad path"); return; }
		size_t l1 = strlen(p1); if (!strncmp(p2, p1, l1) && (p2[l1] == '/' || !p2[l1])) { close(root); reply_err(r->fd, 400, "a folder cannot go inside itself"); return; }
		char a1[256], a2[256]; int f1 = home_parent(root, p1, a1, sizeof a1, 0), f2 = home_parent(root, p2, a2, sizeof a2, 1); close(root);
		if (f1 < 0 || f2 < 0) { if (f1 >= 0) close(f1); if (f2 >= 0) close(f2); reply_err(r->fd, 404, "not found"); return; }
		struct stat st; if (fstatat(f1, a1, &st, AT_SYMLINK_NOFOLLOW)) { close(f1); close(f2); reply_err(r->fd, 404, "not found"); return; }
		int exists = !fstatat(f2, a2, &st, AT_SYMLINK_NOFOLLOW);
		if (exists && !replace) { close(f1); close(f2); reply_err(r->fd, 409, "something with that name is already there"); return; }
		int rc;
		if (!strcmp(what, "rename")) { if (exists) home_rm_at(f2, a2); rc = renameat(f1, a1, f2, a2); }
		else { if (exists) home_rm_at(f2, a2); rc = home_cp_at(f1, a1, f2, a2); }
		int e = errno; close(f1); close(f2);
		if (rc) { reply_err(r->fd, e == ENOSPC ? 507 : 500, e == ENOSPC ? "the disk is full" : strerror(e)); return; }
		reply_ok(r->fd, !strcmp(what, "rename") ? "renamed" : "copied"); return;
	}
	close(root); reply_err(r->fd, 404, "unknown request");
}

/* /dlf/<id> for a file in an account's folder ("home:<acct>:<path>"): the folder walked again, never a link followed */
static void home_serve_link(struct req *r, const char *spec)
{
	char acct[80]; const char *c = strchr(spec, ':');
	if (!c || (size_t)(c - spec) >= sizeof acct) { reply_err(r->fd, 404, "not found"); return; }
	memcpy(acct, spec, c - spec); acct[c - spec] = 0; const char *rel = c + 1;
	if (!home_acct_ok(acct) || !*rel || !home_rel_ok(rel) || !home_unlocked(acct)) { reply_err(r->fd, 404, "not found"); return; }
	int root = home_root_fd(acct); if (root < 0) { reply_err(r->fd, 404, "not found"); return; }
	char last[256]; int pf = home_parent(root, rel, last, sizeof last, 0); close(root);
	if (pf < 0) { reply_err(r->fd, 404, "not found"); return; }
	int f = openat(pf, last, O_RDONLY | O_NOFOLLOW | O_CLOEXEC); close(pf);
	serve_fd_as(r, f, last, 1);
}

/* 1.5.6: once Downloads is the account's own for Linux apps, the top of the downloads store (Linux apps' Downloads
   until then) is out of sight. Until 1.5.6 the desktop kept a copy of the Linux owner's Downloads there, outside the
   account's encrypted folder: a copy with the same name and the same bytes as a file in the account's Downloads is
   deleted. Other files (what Linux apps saved there) move into the account's Downloads, under a free name, but only
   when this computer has one account folder: with several, whose they are cannot be known, and they stay. Partial
   downloads and hidden names (.incoming, .quarantine, .accounts) are left alone. Done here rather than in the
   desktop so that it is in rudolfd's log. */
static int dl_same_bytes(int da, const char *na, int db, const char *nb)
{
	int fa = openat(da, na, O_RDONLY | O_NOFOLLOW | O_CLOEXEC), fb = openat(db, nb, O_RDONLY | O_NOFOLLOW | O_CLOEXEC), same = fa >= 0 && fb >= 0;
	static char ba[65536], bb[65536];
	while (same) { ssize_t ka = read(fa, ba, sizeof ba), kb = ka > 0 ? read(fb, bb, (size_t)ka) : read(fb, bb, 1);
		if (ka < 0 || kb < 0 || ka != kb) { same = 0; break; } if (!ka) break; if (memcmp(ba, bb, (size_t)ka)) same = 0; }
	if (fa >= 0) close(fa); if (fb >= 0) close(fb); return same;
}
/* a name in dir, compared as the desktop does (C: ignores case) */
static int dl_name_taken(int dd, const char *n)
{
	int d2 = dup(dd); DIR *d = d2 >= 0 ? fdopendir(d2) : NULL; struct dirent *e; int yes = 0;
	if (!d) { if (d2 >= 0) close(d2); return 1; }
	rewinddir(d);   /* a dup shares the read position with dd: from the start each time */
	while (!yes && (e = readdir(d))) if (!strcasecmp(e->d_name, n)) yes = 1;
	closedir(d); return yes;
}
static void dl_store_tidy(const char *acct)
{
	char ad[600]; snprintf(ad, sizeof ad, "%s/%s/Downloads", home_root, acct);
	int sd = open(DLSTORE, O_RDONLY | O_DIRECTORY | O_CLOEXEC), dd = open(ad, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	if (sd < 0 || dd < 0) { if (sd >= 0) close(sd); if (dd >= 0) close(dd); return; }
	int accounts = 0; { DIR *h = opendir(home_root); struct dirent *e; while (h && (e = readdir(h))) if (home_acct_ok(e->d_name)) accounts++; if (h) closedir(h); }
	int s2 = dup(sd); DIR *d = s2 >= 0 ? fdopendir(s2) : NULL; struct dirent *e; if (d) rewinddir(d); int same = 0, moved = 0, kept = 0, failed = 0;
	char names[256][256]; int nn = 0;   /* read first: the folder changes while files go */
	while (d && (e = readdir(d)) && nn < 256) { if (e->d_name[0] == '.') continue; snprintf(names[nn++], 256, "%s", e->d_name); }
	if (d) closedir(d); else if (s2 >= 0) close(s2);
	for (int i = 0; i < nn; i++) {
		const char *n = names[i]; size_t l = strlen(n); struct stat ss, ds;
		if (fstatat(sd, n, &ss, AT_SYMLINK_NOFOLLOW) || !S_ISREG(ss.st_mode)) continue;
		if (l > 11 && !strcmp(n + l - 11, ".crdownload")) continue;
		if (!fstatat(dd, n, &ds, AT_SYMLINK_NOFOLLOW) && S_ISREG(ds.st_mode) && ds.st_size == ss.st_size && dl_same_bytes(sd, n, dd, n)) {
			if (!unlinkat(sd, n, 0)) same++; else failed++;
			continue;
		}
		if (accounts != 1) { kept++; continue; }
		char t[300]; snprintf(t, sizeof t, "%s", n);
		for (int k = 2; dl_name_taken(dd, t) && k < 1000; k++) {
			const char *dot = strrchr(n, '.'); if (!dot || dot == n) dot = n + l;
			snprintf(t, sizeof t, "%.*s (%d)%s", (int)(dot - n), n, k, dot);
		}
		int rc = renameat(sd, n, dd, t);
		if (rc && errno == EXDEV) { rc = home_cp_at(sd, n, dd, t); if (!rc) unlinkat(sd, n, 0); }
		if (!rc) { fchownat(dd, t, home_uid, home_uid, AT_SYMLINK_NOFOLLOW); moved++; } else failed++;
	}
	/* an empty .accounts left in the account's Downloads by the second 1.5.6 beta (the store taken for the account's
	   folder): removed only while empty */
	{ int ac = openat(dd, ".accounts", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	  if (ac >= 0) { char lac[80]; size_t li = 0; for (; acct[li] && li < sizeof lac - 1; li++) lac[li] = tolower((unsigned char)acct[li]); lac[li] = 0;
	    unlinkat(ac, lac, AT_REMOVEDIR); close(ac); unlinkat(dd, ".accounts", AT_REMOVEDIR); } }
	close(sd); close(dd);
	if (same || moved || kept || failed)
		fprintf(stderr, "rudolfd: Downloads for Linux apps is %s's own now: %d copies at the top of the store deleted, %d files moved in, %d left (%d account folders), %d failed\n", acct, same, moved, kept, accounts, failed);
}

/* 1.5.6: /persist/accounts/<the signed-in account>/<a folder Linux apps see>/rest -> /home/rudolf/<folder>/rest (the
   binds; Downloads only while the downloads store has its own). 1 when it was rewritten. */
static int home_linux_path(const char *in, char *out, size_t n)
{
	if (!in || strncmp(in, home_root, strlen(home_root)) || in[strlen(home_root)] != '/' || strstr(in, "/../")) return 0;
	char *cur = slurp(HOME_SESSION, NULL); if (!cur) return 0; trim(cur);
	const char *a = in + strlen(home_root) + 1; size_t al = strlen(cur); int ok = 0;
	if (*cur && !strncmp(a, cur, al) && a[al] == '/') {
		const char *rest = a + al + 1;
		for (int i = 0; HOME_BIND[i] && !ok; i++) { size_t bl = strlen(HOME_BIND[i]);
			if (!strncmp(rest, HOME_BIND[i], bl) && rest[bl] == '/' && !home_bind_skip(i)) { snprintf(out, n, "/home/rudolf/%s", rest); ok = 1; } }
	}
	free(cur); return ok;
}
