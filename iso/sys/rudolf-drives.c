/*
 * drives - the root helper for drives (Rudolf OS 1.6.1, docs/plan-1.6.1.md Part 11): /usr/lib/rudolf/root/drives,
 * run by rudolf-rootd for rudolfd (or by rudolfd itself while it is root and was not started by rudolf-rootd).
 *
 *   drives mount DEV               a USB drive's or SD card's partition under /media/DEV (devplug mount)
 *   drives umount DEV              unmounts /media/DEV, as rudolfd did before ("sync; umount; rmdir; rm -f")
 *   drives eject DEV               every partition of the drive unmounted, the drive let go (devplug eject)
 *   drives info DISK               what Rudolf OS a stick has (mkstick.sh --info)
 *   drives install DISK [IMAGE] [--sha=HEX]           install.sh: Rudolf OS onto DISK (erased)
 *   drives bootfiles-write DISK [IMAGE] [--sha=HEX]   bootfiles.sh write: only the boot files in front of RUDOLFDATA
 *   drives bootfiles-stage IMAGE [--sha=HEX]          bootfiles.sh stage: new boot files for this drive, next start
 *   drives mkstick DISK [IMAGE] [0|1] [--sha=HEX]     mkstick.sh: a Rudolf OS USB stick (erased)
 *   drives keepfiles                                  keepfiles.sh make: RUDOLFDATA on the drive Rudolf OS runs from
 *   drives home-unlock ACCT 0|1 UID   an account's encrypted folder opened at sign-in (its key on standard input,
 *                                     never an argument), with 1 its session too: the binds for Linux apps, the
 *                                     downloads' store, the account's Linux user's session (1.6.1 drives part 2)
 *   drives home-lock ACCT             signed out: the session ended, the key taken out (the folder and its Oracle
 *                                     folder unreadable again)
 *   drives home-bind ACCT UID         rudolfd started again while ACCT is signed in: its binds and session again
 *   drives home-store                 rudolfd's start with nobody signed in: the downloads' store bound
 *   drives vault-unlock               1.5.0's encrypted folder (/persist/vault) opened: its key on standard input
 *   drives vault-feature              whether the drive that keeps the files has ext4's encrypt feature
 *   drives vault-prepare              that feature added (Settings' encryption set up: tune2fs -O encrypt)
 *   drives vault-move                 vault-move: Debian, Flatpak's apps and Downloads into the encrypted folder
 *   drives bring-scan DISK            1.6.2 (docs/plan-1.6.2.md Part 27, Bring your files; bringfiles.sh): the old system's
 *                                     personal folders on DISK found and sized, its partitions opened read-only
 *   drives bring-unlock PART          a BitLocker partition opened read-only: the key on standard input, never an argument
 *   drives bring-copy                 the chosen folders copied onto a USB drive and checked (RUN/bring-copy.list)
 *   drives bring-cancel | bring-found the copy stopped | the copies on the plugged-in drives
 *   drives bring-decide DISK erase|partial   erase without keeping (or although some files could not be read)
 *   drives bring-close DISK           the old disk's partitions closed again
 *   drives bring-import ACCT DIR PERSON   one person's files from a copy (DIR, on a drive under /media) into the account's
 *                                     folder, run as the account's own Linux user (never root), the account signed in
 * (the home-* actions: ACCT is checked here, then rudolfd's own functions do the work, built as drives-home:
 * rudolf-drives-home.c)
 *
 * Everything is checked again here, whatever rudolfd checked: DEV a block device's kernel name (letters and digits,
 * one that the kernel has: /sys/class/block/DEV), DISK a whole disk (/sys/block/DISK); IMAGE a file in the home folder
 * (/home/rudolf/...), made canonical (a link out of the home folder is refused), a plain file of the desktop's user
 * or of the account signed in (another account's file, a hard link to one, is refused); --sha= 64 hex digits. The
 * scripts are run with their arguments as they are (never a command line for a shell). A refusal, and what erases or
 * rewrites a drive, are said in the health log. ACCT (home-*): an account's id as the desktop makes them (letters,
 * digits and "-", at most 64), not the guest or the shared folder, and not an account the account list
 * (/persist/etc/rudolf/account-users) says was removed; UID 1000 (the desktop's user) or that account's own uid from
 * the list, never another.
 *
 * Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include "rudolf-root.h"
#include <ctype.h>
#include <sys/mount.h>
#include <grp.h>

#define SYSFS  rr_env("RR_TEST_SYS", "/sys")
#define LIB    rr_env("RR_TEST_LIB", "/usr/lib/rudolf")
#define HOME   rr_env("RR_TEST_HOME", "/home/rudolf")
#define MEDIA  rr_env("RR_TEST_MEDIA", "/media")
#define RUNDIR rr_env("RR_TEST_RUN", "/run/rudolf")
#define AUMAP  rr_env("RR_TEST_AUMAP", "/persist/etc/rudolf/account-users")
#define ROOTDIR rr_env("RR_TEST_DIR", RR_DIR)
#define HOMEROOT rr_env("RR_TEST_HOMEROOT", "/persist/accounts")

static int refuse(const char *what, const char *arg)
{
	char a[80]; size_t k = 0; for (const char *c = arg ? arg : ""; *c && k < sizeof a - 1; c++) a[k++] = isprint((unsigned char)*c) ? *c : '?';
	a[k] = 0;
	rr_health("security", "the drives helper refused %s (%s)", what, a);
	fprintf(stderr, "refused: %s\n", what); return 2;
}
/* (a name of the right form that the kernel does not have, or no longer: a drive pulled out a moment ago; not said in
   the health log) */
static int gone(const char *what) { fprintf(stderr, "refused: %s\n", what); return 2; }
static int is_block(const char *dev) { char p[PATH_MAX]; struct stat st; snprintf(p, sizeof p, "%s/class/block/%s", SYSFS, dev); return !lstat(p, &st); }
static int is_disk(const char *dev) { char p[PATH_MAX]; struct stat st; snprintf(p, sizeof p, "%s/block/%s", SYSFS, dev); return !lstat(p, &st); }
/* the uid of the account signed in with its folder on the disk (RUN/home-session's account, its line in the map) */
static unsigned session_uid(void)
{
	char p[PATH_MAX], id[80] = ""; snprintf(p, sizeof p, "%s/home-session", RUNDIR);
	FILE *f = fopen(p, "r"); if (!f) return 0;
	if (!fgets(id, sizeof id, f)) id[0] = 0;
	fclose(f); id[strcspn(id, "\r\n")] = 0;
	if (!*id) return 0;
	f = fopen(AUMAP, "r"); if (!f) return 0;
	char l[200]; unsigned u = 0;
	while (fgets(l, sizeof l, f)) { char a[80]; unsigned v; if (l[0] != '-' && sscanf(l, "%79s %u", a, &v) == 2 && !strcmp(a, id) && v >= 1001 && v <= 59999) u = v; }
	fclose(f); return u;
}
/* the account list's line for ID: its uid (0: none), *retired: it is a removed account's ("-ID UID") */
static unsigned acct_uid(const char *id, int *retired)
{
	FILE *f = fopen(AUMAP, "r"); if (retired) *retired = 0; if (!f) return 0;
	char l[200]; unsigned u = 0;
	while (fgets(l, sizeof l, f)) { char a[80]; unsigned v; int ret = l[0] == '-';
		if (sscanf(l + ret, "%79s %u", a, &v) == 2 && !strcmp(a, id) && v >= 1001 && v <= 59999) { if (ret) { if (retired) *retired = 1; } else u = v; } }
	fclose(f); return u;
}
static int acct_ok(const char *a)
{
	size_t n = a ? strlen(a) : 0; if (!n || n > 64 || !strcmp(a, "guest") || !strcmp(a, "shared")) return 0;
	for (const char *c = a; *c; c++) if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '-')) return 0;
	return 1;
}
/* the home-* actions: checked here, then drives-home (rudolfd's own functions) does them; standard input, output
   and error go on to it (the key, the answer) */
static int home_action(const char *op, char **a, int n)
{
	const char *sub = !strcmp(op, "home-unlock") ? "unlock" : !strcmp(op, "home-lock") ? "lock" : !strcmp(op, "home-bind") ? "bind" : "store";
	int want = !strcmp(sub, "unlock") ? 3 : !strcmp(sub, "bind") ? 2 : !strcmp(sub, "lock") ? 1 : 0;
	if (n != want) return refuse("the wrong arguments for an account's folder", op);
	if (want) {
		int ret = 0; if (!strcmp(a[0], "guest") || !strcmp(a[0], "shared")) { printf("bad account\n"); return 100; }   /* (as rudolfd answered: 400, not an attack) */
		if (!acct_ok(a[0])) return refuse("an account name that is not one", a[0]);
		unsigned au = acct_uid(a[0], &ret); if (ret && !au) return refuse("a removed account", a[0]);
		if (!strcmp(sub, "unlock") && strcmp(a[1], "0") && strcmp(a[1], "1")) return refuse("a session choice that is not 0 or 1", a[1]);
		const char *us = a[want - 1];
		if (want >= 2) { char *e; long u = strtol(us, &e, 10); if (!*us || *e || (u != 1000 && (!au || u != (long)au))) return refuse("an owner that is neither the desktop's user nor the account's", us); }
	}
	char path[PATH_MAX]; snprintf(path, sizeof path, "%s/drives-home", ROOTDIR);
	char *argv[6] = { "drives-home", (char *)sub, NULL, NULL, NULL, NULL }; for (int i = 0; i < want; i++) argv[2 + i] = a[i];
	char *env[] = { RR_PATH, NULL };
	execve(path, argv, env); fprintf(stderr, "drives-home could not be started: %s\n", strerror(errno));
	printf("the account's folder could not be opened (drives-home did not start)\n"); return 200;   /* (HTTP 500, as drives-home answers) */
}
static int image_ok(const char *img, char *out, size_t on)
{
	unsigned own[3] = { RR_DESK_UID, session_uid(), 0 };
	return rr_home_file_ok(img, HOME, own, out, on);
}
static int run_script(const char *script, char *const args[], const char *sha)
{
	char path[PATH_MAX]; snprintf(path, sizeof path, "%s/%s", LIB, script);
	char e1[100] = "", *env[4] = { RR_PATH, NULL, NULL, NULL }; int k = 1;
	if (sha) { snprintf(e1, sizeof e1, "RUDOLF_IMG_SHA=%s", sha); for (char *c = e1 + 15; *c; c++) *c = (char)tolower((unsigned char)*c); env[k++] = e1; }
#ifdef RR_TEST
	char e2[PATH_MAX]; if (getenv("RR_TEST_LIB")) { snprintf(e2, sizeof e2, "RUDOLF_LIB=%s", LIB); env[k++] = e2; }
#endif
	char *argv[12]; int a = 0; argv[a++] = path; for (int i = 0; args[i] && a < 11; i++) argv[a++] = args[i]; argv[a] = NULL;
	execve(path, argv, env); fprintf(stderr, "%s could not be started: %s\n", script, strerror(errno)); return 127;
}

/* 1.6.2 (docs/plan-1.6.2.md Part 27, Bring your files): bringfiles.sh's actions, the arguments checked here again */
#define BRING_TOP "/Rudolf OS - Bring your files/"
static int bring_person_ok(const char *p)
{
	size_t n = strlen(p); if (!n || n > 120 || !strcmp(p, ".") || !strcmp(p, "..")) return 0;
	for (const char *c = p; *c; c++) if ((unsigned char)*c < 32 || *c == '/' || *c == 127) return 0;
	return 1;
}
static int bring_action(const char *op, char **a, int n)
{
	const char *sub = op + 6;
	if (!strcmp(sub, "scan") || !strcmp(sub, "close") || !strcmp(sub, "decide")) {
		if (n != (!strcmp(sub, "decide") ? 2 : 1) || !rr_dev_ok(a[0], 16)) return refuse("a disk name that is not one", n ? a[0] : "");
		if (!is_disk(a[0])) return gone("a disk the kernel does not have");
		if (n == 2 && strcmp(a[1], "erase") && strcmp(a[1], "partial")) return refuse("a choice that is not erase or partial", a[1]);
		if (!strcmp(sub, "decide")) rr_health("disk", "Bring your files: %s's personal files are to be erased (%s)", a[0], a[1]);
		char *s[] = { (char *)sub, a[0], n == 2 ? a[1] : NULL, NULL }; return run_script("bringfiles.sh", s, NULL);
	}
	if (!strcmp(sub, "unlock")) {
		if (n != 1 || !rr_dev_ok(a[0], 32)) return refuse("a partition name that is not one", n ? a[0] : "");
		if (!is_block(a[0])) return gone("a partition the kernel does not have");
		rr_health("disk", "Bring your files: BitLocker on %s is being opened read-only (the key is not kept)", a[0]);
		char *s[] = { "unlock", a[0], NULL }; return run_script("bringfiles.sh", s, NULL);   /* (the key: standard input, as it came) */
	}
	if (!strcmp(sub, "copy") || !strcmp(sub, "cancel") || !strcmp(sub, "found")) {
		if (n) return refuse("arguments for Bring your files", op);
		char l[PATH_MAX]; snprintf(l, sizeof l, "%s/bring-copy.list", RUNDIR);
		char *s[] = { (char *)sub, !strcmp(sub, "copy") ? l : NULL, NULL }; return run_script("bringfiles.sh", s, NULL);
	}
	if (!strcmp(sub, "import")) {
		if (n != 3 || !acct_ok(a[0])) return refuse("an account name that is not one", n ? a[0] : "");
		int ret = 0; unsigned au = acct_uid(a[0], &ret); if (ret && !au) return refuse("a removed account", a[0]);
		/* the account signed in now (its folder open), and its own Linux user (1000 for an account without one) */
		char p[PATH_MAX], id[80] = ""; snprintf(p, sizeof p, "%s/home-session", RUNDIR);
		FILE *f = fopen(p, "r"); if (f) { if (!fgets(id, sizeof id, f)) id[0] = 0; fclose(f); } id[strcspn(id, "\r\n")] = 0;
		if (strcmp(id, a[0])) return refuse("an account that is not signed in", a[0]);
		unsigned uid = au ? au : RR_DESK_UID;
		/* the copy: a folder under the drives' folder, in Bring your files' folder, with no link on the way */
		char m[PATH_MAX], rp[PATH_MAX]; snprintf(m, sizeof m, "%s/", MEDIA);
		if (strlen(a[1]) >= PATH_MAX - 1 || strncmp(a[1], m, strlen(m)) || !strstr(a[1], BRING_TOP) || strchr(a[1], '\n') || !realpath(a[1], rp) || strcmp(rp, a[1]))
			return refuse("a copy that is not one on a drive", a[1]);
		if (!bring_person_ok(a[2])) return refuse("a person's folder that is not one", a[2]);
		char home[PATH_MAX]; snprintf(home, sizeof home, "%s/%s", HOMEROOT, a[0]);
		char path[PATH_MAX]; snprintf(path, sizeof path, "%s/bringfiles.sh", LIB);
		if (setgroups(0, NULL) || setgid(uid) || setuid(uid) || !getuid()) { fprintf(stderr, "could not become the account's user\n"); return 1; }
		char *argv2[] = { path, "import", a[1], a[2], home, NULL }, *env[] = { RR_PATH, NULL };
		execve(path, argv2, env); fprintf(stderr, "bringfiles.sh could not be started: %s\n", strerror(errno)); return 127;
	}
	return refuse("an unknown action", op);
}

int main(int argc, char **argv)
{
	if (argc < 2) { fprintf(stderr, "usage: drives mount|umount|eject|info|install|bootfiles-write|bootfiles-stage|mkstick|keepfiles|home-unlock|home-lock|home-bind|home-store|bring-* ...\n"); return 2; }
	const char *op = argv[1];
	/* --sha=HEX, wherever it is; the other arguments in order */
	const char *sha = NULL; char *a[8]; int n = 0;
	for (int i = 2; i < argc; i++) {
		if (!strncmp(argv[i], "--sha=", 6)) { if (sha || !rr_hex_ok(argv[i] + 6, 64)) return refuse("a checksum that is not 64 hex digits", argv[i]); sha = argv[i] + 6; continue; }
		if (n >= 7) return refuse("too many arguments", op);
		a[n++] = argv[i];
	}
	a[n] = NULL;
	if (!strcmp(op, "mount") || !strcmp(op, "umount") || !strcmp(op, "eject")) {
		if (n != 1 || !rr_dev_ok(a[0], 32)) return refuse("a device name that is not one", n ? a[0] : "");
		if (!is_block(a[0])) return gone("a device the kernel does not have");
		if (!strcmp(op, "umount")) {
			/* as rudolfd's "sync; umount /media/DEV && rmdir /media/DEV; rm -f /run/rudolf/drives/DEV" (whose status was rm's) */
			char m[PATH_MAX], j[PATH_MAX]; snprintf(m, sizeof m, "%s/%s", MEDIA, a[0]); snprintf(j, sizeof j, "%s/drives/%s", RUNDIR, a[0]);
			sync();
			if (!umount2(m, UMOUNT_NOFOLLOW)) { if (rmdir(m)) {} }
			else fprintf(stderr, "umount: %s: %s\n", m, strerror(errno));
			unlink(j); return 0;
		}
		char *s[] = { (char *)op, a[0], NULL }; return run_script("devplug", s, NULL);
	}
	if (!strncmp(op, "home-", 5) && (!strcmp(op, "home-unlock") || !strcmp(op, "home-lock") || !strcmp(op, "home-bind") || !strcmp(op, "home-store"))) { if (sha) return refuse("a checksum for an account's folder", op); return home_action(op, a, n); }
	if (!strcmp(op, "vault-unlock") || !strcmp(op, "vault-feature") || !strcmp(op, "vault-prepare")) {
		if (n || sha) return refuse("arguments for the encrypted folder", op);
		char path[PATH_MAX]; snprintf(path, sizeof path, "%s/drives-home", ROOTDIR);
		char *argv2[] = { "drives-home", (char *)op, NULL }, *env[] = { RR_PATH, NULL };
		execve(path, argv2, env); fprintf(stderr, "drives-home could not be started: %s\n", strerror(errno));
		printf("the encrypted folder could not be opened (drives-home did not start)\n"); return 200;
	}
	if (!strncmp(op, "bring-", 6)) { if (sha) return refuse("a checksum for Bring your files", op); return bring_action(op, a, n); }
	if (!strcmp(op, "vault-move")) { if (n || sha) return refuse("arguments for vault-move", op); char *s[] = { NULL }; return run_script("vault-move", s, NULL); }
	if (!strcmp(op, "info")) {
		if (n != 1 || !rr_dev_ok(a[0], 16)) return refuse("a disk name that is not one", n ? a[0] : "");
		if (!is_disk(a[0])) return gone("a disk the kernel does not have");
		char d[64]; snprintf(d, sizeof d, "/dev/%s", a[0]); char *s[] = { "--info", d, NULL }; return run_script("mkstick.sh", s, NULL);
	}
	if (!strcmp(op, "keepfiles")) { if (n) return refuse("arguments for keepfiles", a[0]); rr_health("disk", "RUDOLFDATA is being added to the drive Rudolf OS runs from"); char *s[] = { "make", NULL }; return run_script("keepfiles.sh", s, NULL); }
	if (!strcmp(op, "bootfiles-stage")) {
		char img[PATH_MAX]; if (n != 1 || !image_ok(a[0], img, sizeof img)) return refuse("an image that is not a file in the home folder", n ? a[0] : "");
		rr_health("disk", "new boot files are being checked for the next start (%.200s)", img);
		char *s[] = { "stage", img, NULL }; return run_script("bootfiles.sh", s, sha);
	}
	if (!strcmp(op, "install") || !strcmp(op, "bootfiles-write") || !strcmp(op, "mkstick")) {
		int stick = !strcmp(op, "mkstick");
		if (n < 1 || n > (stick ? 3 : 2) || !rr_dev_ok(a[0], 16)) return refuse("a disk name that is not one", n ? a[0] : "");
		if (!is_disk(a[0])) return gone("a disk the kernel does not have");
		char img[PATH_MAX] = ""; if (stick && n > 1 && !strcmp(a[1], "@address")) snprintf(img, sizeof img, "@address");   /* (1.6.0, Part 16: mkstick.sh downloads and checks it) */
		else if (n > 1 && *a[1] && !image_ok(a[1], img, sizeof img)) return refuse("an image that is not a file in the home folder", a[1]);
		const char *files = n > 2 ? a[2] : "0"; if (strcmp(files, "0") && strcmp(files, "1")) return refuse("a files choice that is not 0 or 1", files);
		char d[64]; snprintf(d, sizeof d, "/dev/%s", a[0]);
		rr_health("disk", "%s: %s is being written%s%.200s", stick ? "Make a stick" : !strcmp(op, "install") ? "Install" : "Replace the boot files", d, *img ? " from " : "", img);
		if (stick) { char *s[] = { d, img, (char *)files, NULL }; return run_script("mkstick.sh", s, sha); }
		if (!strcmp(op, "install")) { char *s[] = { d, *img ? img : NULL, NULL }; return run_script("install.sh", s, sha); }
		char *s[] = { "write", d, img, NULL }; return run_script("bootfiles.sh", s, sha);
	}
	return refuse("an unknown action", op);
}
