/*
 * fprint - the root helper for fingerprint readers (Rudolf OS 1.6.3, docs/plan-1.6.3.md Part 2):
 * /usr/lib/rudolf/root/fprint, run by rudolf-rootd for rudolfd (or by rudolfd itself while it is root and was not
 * started by rudolf-rootd). It talks to the reader with libfprint itself, so nothing on the computer offers the
 * reader to anyone else: no service, no D-Bus name, no socket, no port. fprintd's job (a D-Bus service that checks
 * each caller with polkit) is not needed when only rudolfd asks, so fprintd is not shipped (docs/plan-1.6.3.md Part 2).
 *
 * The prints: each account's in a folder of its own, PERSIST/fprint/ACCOUNT (root's, 0700), encrypted by the kernel
 * (fscrypt v2) under the same key as the account's home folder (PERSIST/accounts/ACCOUNT), as Oracle's encrypted folder
 * is (rudolfd-home.inc, oweb_open). That key is unwrapped by the account's password at sign-in (1.5.6), so the prints
 * can be read only while the account's files are open: after a restart, or once the account has signed out, they
 * cannot be read at all, even by root, and no finger can stand in for the password. A copy of the disk holds them
 * only encrypted. One file per finger (FINGER: libfprint's own serialized print, 0600). An account removed: its
 * folder removed with it (encrypted names can be removed without the key), and the prints a reader keeps in itself
 * (match-on-chip readers) removed by their name, "rudolf:ACCOUNT".
 *
 *   fprint devices                 {"readers":[{"name","driver","id","scan","stages","identify","storage"}],
 *                                   "unsupported":[{"usb","name"}]}: a USB device on libfprint's own list of readers
 *                                   it does not support (its hwdb's "Known unsupported devices") is named there
 *   fprint list ACCOUNT            {"open":bool,"disk":bool,"fingers":[FINGER...]} (fingers only while open)
 *   fprint enroll ACCOUNT FINGER   the finger's scans (the reader's number of them), its progress in RUN/progress
 *                                  ({"stage","of","msg"}); {"ok":true,"finger"} or {"ok":false,"code","error"}
 *   fprint verify ACCOUNT SECONDS  waits up to SECONDS (1-60) for a finger, compared with that account's prints only:
 *                                  {"ok":true,"match":true,"finger"} | {"ok":true,"match":false} | {"ok":false,"code",
 *                                  "error"} (code: timeout, cancelled, locked, nofingers, noreader, busy, failed)
 *   fprint delete ACCOUNT FINGER|all
 *   fprint purge ACCOUNT           the account is gone: its folder, and its prints in the reader
 *   fprint cancel                  a waiting enroll or verify ends ("cancelled")
 * FINGER: left-thumb, left-index, left-middle, left-ring, left-little, and right- the same.
 *
 * Confinement: rudolf-rootd's own for its helpers (rudolf-rootd-confine.h: this one keeps CAP_DAC_READ_SEARCH only, to
 * open the account's folder, which belongs to the account's Linux user), and in this helper itself, however it is
 * started: after the account's folder is opened, every capability is dropped and no_new_privs set; with the
 * confinement switch on (rudolfd-confine.inc), Landlock too: read anywhere, write only the prints' folder, RUN and the
 * devices; no TCP of its own, no abstract sockets, no signals to other processes.
 *
 * Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include "rudolf-root.h"
#include "rudolf-confine.h"
#include <dirent.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <linux/fscrypt.h>
#include <fprint.h>

#ifndef RUDOLFD_CONFINE
#define RUDOLFD_CONFINE "off"
#endif
static const char *P_ACCTS, *P_STORE, *P_RUN, *P_UNSUP, *P_USB;
#define NFINGERS 10
static const char *const FINGERS[NFINGERS] = { "left-thumb", "left-index", "left-middle", "left-ring", "left-little",
	"right-thumb", "right-index", "right-middle", "right-ring", "right-little" };

static void jstr(GString *s, const char *v)
{
	g_string_append_c(s, '"');
	for (const unsigned char *c = (const unsigned char *)(v ? v : ""); *c; c++) {
		if (*c == '"' || *c == '\\') g_string_append_printf(s, "\\%c", *c);
		else if (*c < 0x20) g_string_append_printf(s, "\\u%04x", *c);
		else g_string_append_c(s, (char)*c);
	}
	g_string_append_c(s, '"');
}
static int say(const char *json) { printf("%s\n", json); fflush(stdout); return 0; }
static int fail(const char *code, const char *msg)
{
	GString *s = g_string_new("{\"ok\":false,\"code\":"); jstr(s, code); g_string_append(s, ",\"error\":"); jstr(s, msg); g_string_append(s, "}");
	say(s->str); g_string_free(s, TRUE); return 1;
}
/* an account's id as rudolfd's home_acct_ok takes it; never Guest's or the shared folder */
static int acct_ok(const char *a)
{
	if (!a || !*a || strlen(a) > 64 || !strcmp(a, "guest") || !strcmp(a, "shared")) return 0;
	for (const char *c = a; *c; c++) if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '-')) return 0;
	return 1;
}
static int finger_num(const char *f) { for (int i = 0; f && i < NFINGERS; i++) if (!strcmp(f, FINGERS[i])) return i; return -1; }

/* ---------------------------------------------------------------- the prints' folder, under the account's key */
static int policy_id(int dfd, unsigned char *id)   /* 1 encrypted (v2), 0 not, -1 error */
{
	struct fscrypt_get_policy_ex_arg a; memset(&a, 0, sizeof a); a.policy_size = sizeof a.policy;
	if (ioctl(dfd, FS_IOC_GET_ENCRYPTION_POLICY_EX, &a)) return errno == ENODATA ? 0 : -1;
	if (a.policy.version != FSCRYPT_POLICY_V2) return -1;
	memcpy(id, a.policy.v2.master_key_identifier, FSCRYPT_KEY_IDENTIFIER_SIZE); return 1;
}
static int key_present(int dfd, const unsigned char *id)
{
	struct fscrypt_get_key_status_arg a; memset(&a, 0, sizeof a); a.key_spec.type = FSCRYPT_KEY_SPEC_TYPE_IDENTIFIER;
	memcpy(a.key_spec.u.identifier, id, FSCRYPT_KEY_IDENTIFIER_SIZE);
	return !ioctl(dfd, FS_IOC_GET_ENCRYPTION_KEY_STATUS, &a) && a.status == FSCRYPT_KEY_STATUS_PRESENT;
}
/* the account's key identifier from its home folder: 1 open (its key present), 0 locked, -1 no folder on the disk */
static int acct_key(const char *acct, unsigned char *id)
{
	int rfd = open(P_ACCTS, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC); if (rfd < 0) return -1;
	int d = openat(rfd, acct, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC); close(rfd); if (d < 0) return -1;
	int p = policy_id(d, id), k = p == 1 && key_present(d, id); close(d);
	return p != 1 ? -1 : k;
}
/* the account's prints' folder, open (O_DIRECTORY), made the first time with its home folder's policy when mk; -1 with
   *why: "disk" (its files are not on this disk), "locked" (not open now), "none" (no prints yet), "failed" */
static int store_open(const char *acct, int mk, const char **why)
{
	unsigned char id[FSCRYPT_KEY_IDENTIFIER_SIZE]; int k = acct_key(acct, id);
	if (k < 0) { *why = "disk"; return -1; }
	if (!k) { *why = "locked"; return -1; }
	mkdir(P_STORE, 0700);
	int rfd = open(P_STORE, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC); if (rfd < 0) { *why = "failed"; return -1; }
	struct stat st; if (fstat(rfd, &st) || st.st_uid != 0 || (st.st_mode & 077)) { if (!fchown(rfd, 0, 0) && !fchmod(rfd, 0700)) {} }
	int d = openat(rfd, acct, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	if (d < 0 && errno == ENOENT) {
		if (!mk) { close(rfd); *why = "none"; return -1; }
		if (mkdirat(rfd, acct, 0700)) { close(rfd); *why = "failed"; return -1; }
		d = openat(rfd, acct, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
		struct fscrypt_policy_v2 pol; memset(&pol, 0, sizeof pol);
		pol.version = FSCRYPT_POLICY_V2; pol.contents_encryption_mode = FSCRYPT_MODE_AES_256_XTS; pol.filenames_encryption_mode = FSCRYPT_MODE_AES_256_CTS; pol.flags = FSCRYPT_POLICY_FLAGS_PAD_32;
		memcpy(pol.master_key_identifier, id, sizeof id);
		if (d < 0 || ioctl(d, FS_IOC_SET_ENCRYPTION_POLICY, &pol)) { if (d >= 0) close(d); unlinkat(rfd, acct, AT_REMOVEDIR); close(rfd); *why = "failed"; return -1; }
	} else if (d >= 0) {
		/* a folder there under any other key (or none) is never used: it is not this account's */
		unsigned char have[FSCRYPT_KEY_IDENTIFIER_SIZE];
		if (policy_id(d, have) != 1 || memcmp(have, id, sizeof have) || fstat(d, &st) || st.st_uid != 0) { close(d); close(rfd); *why = "failed"; return -1; }
	}
	close(rfd);
	if (d < 0) *why = "failed";
	return d;
}
static int rm_tree_at(int dfd, const char *name)
{
	int d = openat(dfd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	if (d < 0) return unlinkat(dfd, name, 0);
	DIR *dd = fdopendir(d); if (!dd) { close(d); return -1; }
	struct dirent *e; while ((e = readdir(dd))) { if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue; rm_tree_at(dirfd(dd), e->d_name); }
	closedir(dd); return unlinkat(dfd, name, AT_REMOVEDIR);
}

/* ---------------------------------------------------------------- the confinement in this helper itself */
static void confine_self(void)
{
	cf_caps(0);   /* (fails harmlessly when already without them) */
	prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	int m = cf_mode_read(RUDOLFD_CONFINE, "/persist/etc/rudolf-confine");
	if (m != CF_ON) return;
	const struct cf_rule rules[] = { { "/", CF_R | CF_FS_EXECUTE }, { P_STORE, CF_W }, { P_RUN, CF_W }, { "/dev", CF_WF | CF_FS_IOCTL_DEV }, { "/sys", CF_WF } };
	if (cf_landlock(rules, (int)(sizeof rules / sizeof rules[0]), "", CF_SCOPE_ABSTRACT | CF_SCOPE_SIGNAL, 1, NULL) < 0)
		rr_health("security", "the fingerprint helper could not confine itself (Landlock: %s)", strerror(errno));
}

/* ---------------------------------------------------------------- the reader */
static FpContext *ctx;
static FpDevice *reader(void)
{
	if (!ctx) { ctx = fp_context_new(); fp_context_enumerate(ctx); }
	GPtrArray *a = fp_context_get_devices(ctx);
	return a && a->len ? g_ptr_array_index(a, 0) : NULL;
}
/* one reader user at a time: the lock in RUN, waited for up to 5 s */
static int lock_fd = -1;
static int take_lock(void)
{
	mkdir(P_RUN, 0700);
	char p[PATH_MAX]; snprintf(p, sizeof p, "%s/lock", P_RUN);
	lock_fd = open(p, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600); if (lock_fd < 0) return -1;
	for (int i = 0; i < 50; i++) { if (!flock(lock_fd, LOCK_EX | LOCK_NB)) return 0; usleep(100000); }
	return -1;
}
/* cancelling: "fprint cancel" makes RUN/cancel; the one waiting sees it (every 200 ms) */
static GCancellable *canc;
static time_t started;
static int timed_out;
static gboolean poll_cancel(gpointer u)
{
	(void)u; char p[PATH_MAX]; snprintf(p, sizeof p, "%s/cancel", P_RUN); struct stat st;
	if (!stat(p, &st) && st.st_mtime >= started) g_cancellable_cancel(canc);
	return G_SOURCE_CONTINUE;
}
static gboolean on_timeout(gpointer u) { (void)u; timed_out = 1; g_cancellable_cancel(canc); return G_SOURCE_REMOVE; }
static void progress_write(int stage, int of, const char *msg)
{
	char p[PATH_MAX], t[PATH_MAX]; snprintf(p, sizeof p, "%s/progress", P_RUN); snprintf(t, sizeof t, "%s/progress.t", P_RUN);
	GString *s = g_string_new(NULL); g_string_append_printf(s, "{\"stage\":%d,\"of\":%d,\"msg\":", stage, of); jstr(s, msg); g_string_append(s, "}\n");
	int f = open(t, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0644);
	if (f >= 0) { if (write(f, s->str, s->len) < 0) {} close(f); if (rename(t, p)) unlink(t); }
	g_string_free(s, TRUE);
}
static const char *retry_msg(const GError *e)
{
	switch (e->code) {
	case FP_DEVICE_RETRY_TOO_SHORT: return "The swipe was too short: try again.";
	case FP_DEVICE_RETRY_CENTER_FINGER: return "Put your finger in the middle of the reader.";
	case FP_DEVICE_RETRY_REMOVE_FINGER: return "Lift your finger, then put it on the reader again.";
	default: return "The reader did not read the finger well: try again.";
	}
}
static void on_enroll(FpDevice *d, gint done, FpPrint *p, gpointer u, GError *e)
{
	(void)p; (void)u;
	int of = fp_device_get_nr_enroll_stages(d);
	progress_write(done, of, e ? retry_msg(e) : done < of ? "Lift your finger and put it on the reader again." : "Done.");
}

static int open_reader(FpDevice **out)
{
	FpDevice *d = reader(); if (!d) return fail("noreader", "No fingerprint reader that Rudolf OS supports is connected.");
	GError *e = NULL;
	if (!fp_device_open_sync(d, NULL, &e)) { int r = fail("failed", e ? e->message : "the reader did not open"); g_clear_error(&e); return r; }
	*out = d; return 0;
}
static char *store_path(const char *acct) { return g_strdup_printf("%s/%s", P_STORE, acct); }

static int cmd_enroll(const char *acct, const char *finger)
{
	int fn = finger_num(finger); if (fn < 0) return fail("failed", "unknown finger");
	const char *why = ""; int d = store_open(acct, 1, &why);
	if (d < 0) return fail(!strcmp(why, "disk") ? "disk" : !strcmp(why, "locked") ? "locked" : "failed",
		!strcmp(why, "disk") ? "This account's files are not on this computer's disk." : !strcmp(why, "locked") ? "This account's files are not open: sign in to it with its password first." : "The fingerprints' folder could not be made.");
	confine_self();
	if (take_lock()) { close(d); return fail("busy", "The fingerprint reader is in use."); }
	FpDevice *dev = NULL; if (open_reader(&dev)) { close(d); return 1; }
	char cp[PATH_MAX]; snprintf(cp, sizeof cp, "%s/cancel", P_RUN); unlink(cp);
	canc = g_cancellable_new(); started = time(NULL);
	g_timeout_add(200, poll_cancel, NULL); g_timeout_add_seconds(120, on_timeout, NULL);
	progress_write(0, fp_device_get_nr_enroll_stages(dev), "Put your finger on the reader.");
	FpPrint *tmpl = fp_print_new(dev); fp_print_set_finger(tmpl, (FpFinger)(FP_FINGER_LEFT_THUMB + fn));
	char *un = g_strdup_printf("rudolf:%s", acct); fp_print_set_username(tmpl, un); g_free(un);
	GError *e = NULL; FpPrint *p = fp_device_enroll_sync(dev, tmpl, canc, on_enroll, NULL, &e);
	if (!p) {
		int c = e && g_error_matches(e, G_IO_ERROR, G_IO_ERROR_CANCELLED);
		int r = fail(c ? (timed_out ? "timeout" : "cancelled") : "failed", c ? (timed_out ? "No finger was put on the reader in time." : "Cancelled.") : e ? e->message : "the finger was not taken");
		progress_write(-1, 0, c ? "Cancelled." : "The finger was not taken."); g_clear_error(&e); fp_device_close_sync(dev, NULL, NULL); close(d); return r;
	}
	guchar *data = NULL; gsize len = 0;
	if (!fp_print_serialize(p, &data, &len, &e)) { int r = fail("failed", e ? e->message : "the print could not be kept"); g_clear_error(&e); fp_device_close_sync(dev, NULL, NULL); close(d); return r; }
	char tn[64]; snprintf(tn, sizeof tn, "%s.t", finger); unlinkat(d, tn, 0);
	int f = openat(d, tn, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
	int ok = f >= 0 && write(f, data, len) == (ssize_t)len && !fsync(f); if (f >= 0) close(f);
	ok = ok && !renameat(d, tn, d, finger); if (!ok) unlinkat(d, tn, 0);
	g_free(data); g_object_unref(p); fp_device_close_sync(dev, NULL, NULL); close(d);
	if (!ok) return fail("failed", "The print could not be kept.");
	GString *s = g_string_new("{\"ok\":true,\"finger\":"); jstr(s, finger); g_string_append(s, "}"); say(s->str); g_string_free(s, TRUE);
	return 0;
}

static int cmd_verify(const char *acct, int secs)
{
	const char *why = ""; int d = store_open(acct, 0, &why);
	if (d < 0) return fail(!strcmp(why, "none") ? "nofingers" : !strcmp(why, "failed") ? "failed" : "locked",
		!strcmp(why, "none") ? "This account has no fingerprints." : !strcmp(why, "failed") ? "The fingerprints' folder could not be opened." : "Use your password: this account's files are not open.");
	/* this account's prints, and only its own */
	GPtrArray *prints = g_ptr_array_new_with_free_func(g_object_unref);
	DIR *dd = fdopendir(dup(d)); struct dirent *de;
	while (dd && (de = readdir(dd))) {
		if (finger_num(de->d_name) < 0) continue;
		int f = openat(d, de->d_name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW); if (f < 0) continue;
		struct stat st; guchar *b = NULL; ssize_t n = -1;
		if (!fstat(f, &st) && S_ISREG(st.st_mode) && st.st_size > 0 && st.st_size < (1 << 22)) { b = g_malloc(st.st_size); n = read(f, b, st.st_size); }
		close(f);
		if (b && n == st.st_size) { GError *e = NULL; FpPrint *p = fp_print_deserialize(b, n, &e); if (p) g_ptr_array_add(prints, p); g_clear_error(&e); }
		g_free(b);
	}
	if (dd) closedir(dd);
	close(d);
	confine_self();
	if (!prints->len) { g_ptr_array_unref(prints); return fail("nofingers", "This account has no fingerprints."); }
	if (take_lock()) { g_ptr_array_unref(prints); return fail("busy", "The fingerprint reader is in use."); }
	FpDevice *dev = NULL; if (open_reader(&dev)) { g_ptr_array_unref(prints); return 1; }
	char cp[PATH_MAX]; snprintf(cp, sizeof cp, "%s/cancel", P_RUN); unlink(cp);
	canc = g_cancellable_new(); started = time(NULL);
	g_timeout_add(200, poll_cancel, NULL); g_timeout_add_seconds(secs, on_timeout, NULL);
	int idf = fp_device_has_feature(dev, FP_DEVICE_FEATURE_IDENTIFY), rc = -1; FpPrint *hit = NULL;
	for (;;) {
		GError *e = NULL; FpPrint *m = NULL; gboolean match = FALSE; gboolean ok;
		if (idf) { ok = fp_device_identify_sync(dev, prints, canc, NULL, NULL, &m, NULL, &e); match = ok && m; }
		else ok = fp_device_verify_sync(dev, g_ptr_array_index(prints, 0), canc, NULL, NULL, &match, NULL, &e);
		if (ok) { if (match) hit = m ? m : g_ptr_array_index(prints, 0); rc = 0; break; }
		if (e && e->domain == FP_DEVICE_RETRY) { g_clear_error(&e); continue; }   /* a poor read: not a try, the finger again */
		int c = e && g_error_matches(e, G_IO_ERROR, G_IO_ERROR_CANCELLED);
		rc = fail(c ? (timed_out ? "timeout" : "cancelled") : "failed", c ? (timed_out ? "No finger was put on the reader in time." : "Cancelled.") : e ? e->message : "the reader failed");
		g_clear_error(&e); break;
	}
	if (!rc) {
		if (hit) { int fi = fp_print_get_finger(hit) - FP_FINGER_LEFT_THUMB; GString *s = g_string_new("{\"ok\":true,\"match\":true,\"finger\":"); jstr(s, fi >= 0 && fi < NFINGERS ? FINGERS[fi] : ""); g_string_append(s, "}"); say(s->str); g_string_free(s, TRUE); }
		else say("{\"ok\":true,\"match\":false}");
	}
	fp_device_close_sync(dev, NULL, NULL); g_ptr_array_unref(prints);
	return rc ? 1 : 0;
}

/* the prints a match-on-chip reader keeps in itself for this account (by their name), removed; best effort */
static void device_forget(const char *acct, const char *finger)
{
	FpDevice *dev = reader(); if (!dev) return;
	if (!fp_device_open_sync(dev, NULL, NULL)) return;
	if (fp_device_has_feature(dev, FP_DEVICE_FEATURE_STORAGE_LIST) && fp_device_has_feature(dev, FP_DEVICE_FEATURE_STORAGE_DELETE)) {
		GPtrArray *a = fp_device_list_prints_sync(dev, NULL, NULL); char *un = g_strdup_printf("rudolf:%s", acct);
		for (guint i = 0; a && i < a->len; i++) { FpPrint *p = g_ptr_array_index(a, i); const char *u = fp_print_get_username(p);
			int fi = fp_print_get_finger(p) - FP_FINGER_LEFT_THUMB;
			if (u && !strcmp(u, un) && (!finger || (fi >= 0 && fi < NFINGERS && !strcmp(FINGERS[fi], finger)))) fp_device_delete_print_sync(dev, p, NULL, NULL); }
		g_free(un); if (a) g_ptr_array_unref(a);
	}
	fp_device_close_sync(dev, NULL, NULL);
}
static int cmd_delete(const char *acct, const char *finger)
{
	int all = !strcmp(finger, "all"); if (!all && finger_num(finger) < 0) return fail("failed", "unknown finger");
	const char *why = ""; int d = store_open(acct, 0, &why);
	if (d < 0 && !strcmp(why, "none")) { say("{\"ok\":true}"); return 0; }
	if (d < 0 && all && strcmp(why, "disk")) {
		/* all of them, the folder locked: removed whole (its names need no key to be removed) */
		int rfd = open(P_STORE, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC); int r = rfd >= 0 ? rm_tree_at(rfd, acct) : -1; if (rfd >= 0) close(rfd);
		confine_self(); if (!take_lock()) device_forget(acct, NULL);
		if (r && errno != ENOENT) return fail("failed", "The fingerprints could not be removed.");
		say("{\"ok\":true}"); return 0;
	}
	if (d < 0) return fail(!strcmp(why, "disk") ? "disk" : "failed", "The fingerprints' folder could not be opened.");
	int r = 0;
	if (all) { close(d); int rfd = open(P_STORE, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC); r = rfd >= 0 ? rm_tree_at(rfd, acct) : -1; if (rfd >= 0) close(rfd); }
	else { r = unlinkat(d, finger, 0); if (r && errno == ENOENT) r = 0; close(d); }
	confine_self(); if (!take_lock()) device_forget(acct, all ? NULL : finger);
	if (r) return fail("failed", "The fingerprint could not be removed.");
	say("{\"ok\":true}"); return 0;
}
static int cmd_list(const char *acct)
{
	unsigned char id[FSCRYPT_KEY_IDENTIFIER_SIZE]; int k = acct_key(acct, id);
	GString *s = g_string_new(NULL); g_string_append_printf(s, "{\"ok\":true,\"disk\":%s,\"open\":%s,\"fingers\":[", k >= 0 ? "true" : "false", k > 0 ? "true" : "false");
	if (k > 0) {
		const char *why = ""; int d = store_open(acct, 0, &why), n = 0;
		if (d >= 0) { for (int i = 0; i < NFINGERS; i++) { struct stat st; if (!fstatat(d, FINGERS[i], &st, AT_SYMLINK_NOFOLLOW) && S_ISREG(st.st_mode)) { if (n++) g_string_append_c(s, ','); jstr(s, FINGERS[i]); } } close(d); }
	}
	g_string_append(s, "]}"); say(s->str); g_string_free(s, TRUE); return 0;
}
/* libfprint's own list of readers it does not support (its hwdb, "# Known unsupported devices": usb:vVVVVpPPPP*),
   kept as VVVV:PPPP lines by the build; the USB devices plugged in, named from the kernel's sysfs */
static int unsupported(const char *vp)
{
	FILE *f = fopen(P_UNSUP, "re"); if (!f) return 0;
	char l[64]; int hit = 0; while (!hit && fgets(l, sizeof l, f)) { l[strcspn(l, "\r\n")] = 0; if (!strcasecmp(l, vp)) hit = 1; }
	fclose(f); return hit;
}
static void sysread(const char *dir, const char *name, char *out, size_t n)
{
	char p[PATH_MAX]; snprintf(p, sizeof p, "%s/%s/%s", P_USB, dir, name); out[0] = 0;
	FILE *f = fopen(p, "re"); if (!f) return; if (!fgets(out, (int)n, f)) out[0] = 0; fclose(f); out[strcspn(out, "\r\n")] = 0;
	for (char *c = out; *c; c++) if ((unsigned char)*c < 0x20 || *c == 0x7f) *c = ' ';
}
static int cmd_devices(void)
{
	confine_self();
	GString *s = g_string_new("{\"ok\":true,\"readers\":[");
	FpDevice *d = reader();
	if (d) {
		g_string_append(s, "{\"name\":"); jstr(s, fp_device_get_name(d)); g_string_append(s, ",\"driver\":"); jstr(s, fp_device_get_driver(d));
		g_string_append(s, ",\"id\":"); jstr(s, fp_device_get_device_id(d));
		g_string_append_printf(s, ",\"scan\":\"%s\",\"stages\":%d,\"identify\":%s,\"storage\":%s}", fp_device_get_scan_type(d) == FP_SCAN_TYPE_SWIPE ? "swipe" : "press",
			fp_device_get_nr_enroll_stages(d), fp_device_has_feature(d, FP_DEVICE_FEATURE_IDENTIFY) ? "true" : "false", fp_device_has_feature(d, FP_DEVICE_FEATURE_STORAGE) ? "true" : "false");
	}
	g_string_append(s, "],\"unsupported\":[");
	DIR *dd = opendir(P_USB); struct dirent *e; int n = 0;
	while (dd && (e = readdir(dd)) && n < 16) {
		if (e->d_name[0] == '.' || strchr(e->d_name, ':')) continue;   /* (devices only, not their interfaces) */
		char v[16], p[16], vp[16], nm[200], mf[200]; sysread(e->d_name, "idVendor", v, sizeof v); sysread(e->d_name, "idProduct", p, sizeof p);
		if (strlen(v) != 4 || strlen(p) != 4) continue;
		snprintf(vp, sizeof vp, "%s:%s", v, p); if (!unsupported(vp)) continue;
		sysread(e->d_name, "product", nm, sizeof nm); sysread(e->d_name, "manufacturer", mf, sizeof mf);
		char full[420]; snprintf(full, sizeof full, "%s%s%s", mf, mf[0] && nm[0] ? " " : "", nm[0] ? nm : mf[0] ? "" : "Fingerprint reader");
		if (n++) g_string_append_c(s, ',');
		g_string_append(s, "{\"usb\":"); jstr(s, vp); g_string_append(s, ",\"name\":"); jstr(s, full); g_string_append(s, "}");
	}
	if (dd) closedir(dd);
	g_string_append(s, "]}"); say(s->str); g_string_free(s, TRUE); return 0;
}

int main(int argc, char **argv)
{
	P_ACCTS = rr_env("RUDOLF_FP_ACCTS", "/persist/accounts"); P_STORE = rr_env("RUDOLF_FP_STORE", "/persist/fprint");
	P_RUN = rr_env("RUDOLF_FP_RUN", "/run/rudolf/fprint"); P_UNSUP = rr_env("RUDOLF_FP_UNSUP", "/usr/share/rudolf/libfprint-unsupported");
	P_USB = rr_env("RUDOLF_FP_USB", "/sys/bus/usb/devices");
#ifdef RR_TEST
	{ const char *v = rr_env("FP_VIRTUAL_IMAGE", ""); if (*v) setenv("FP_VIRTUAL_IMAGE", v, 1); }   /* (libfprint's virtual reader: host tests only) */
#else
	unsetenv("FP_VIRTUAL_IMAGE"); unsetenv("FP_VIRTUAL_DEVICE"); unsetenv("FP_VIRTUAL_DEVICE_STORAGE"); unsetenv("FP_DEVICE_EMULATION");
#endif
	umask(077);
	if (argc < 2) return fail("failed", "fprint devices|list|enroll|verify|delete|purge|cancel");
	const char *c = argv[1];
	if (!strcmp(c, "devices") && argc == 2) return cmd_devices();
	if (!strcmp(c, "cancel") && argc == 2) {
		mkdir(P_RUN, 0700); char p[PATH_MAX]; snprintf(p, sizeof p, "%s/cancel", P_RUN);
		int f = open(p, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0600); if (f >= 0) close(f);
		struct timespec ts[2] = { { 0, UTIME_NOW }, { 0, UTIME_NOW } }; utimensat(AT_FDCWD, p, ts, AT_SYMLINK_NOFOLLOW);
		say("{\"ok\":true}"); return 0;
	}
	if (argc < 3 || !acct_ok(argv[2])) return fail("failed", "bad account");
	const char *a = argv[2];
	if (!strcmp(c, "list") && argc == 3) return cmd_list(a);
	if (!strcmp(c, "enroll") && argc == 4) return cmd_enroll(a, argv[3]);
	if (!strcmp(c, "verify") && argc == 4) { int s = atoi(argv[3]); if (s < 1 || s > 60) return fail("failed", "seconds: 1-60"); return cmd_verify(a, s); }
	if (!strcmp(c, "delete") && argc == 4) return cmd_delete(a, argv[3]);
	if (!strcmp(c, "purge") && argc == 3) {
		int rfd = open(P_STORE, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC); int r = rfd >= 0 ? rm_tree_at(rfd, a) : 0; if (rfd >= 0) close(rfd);
		int gone = !r || errno == ENOENT;
		confine_self(); if (!take_lock()) device_forget(a, NULL);
		if (!gone) return fail("failed", "The fingerprints could not be removed.");
		say("{\"ok\":true}"); return 0;
	}
	return fail("failed", "unknown request");
}
