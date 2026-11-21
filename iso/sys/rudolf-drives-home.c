/*
 * drives-home - the accounts' encrypted folders and sessions, for the drives helper (Rudolf OS 1.6.1,
 * docs/plan-1.6.1.md Part 11, drives part 2): /usr/lib/rudolf/root/drives-home, root's 0700, run only by the drives
 * helper after its own checks (it is not one of rudolf-rootd's helpers: rudolf-rootd refuses its name).
 *
 *   drives-home unlock ACCT 0|1 UID   the key on standard input: 128 hex digits (and a newline at most)
 *   drives-home lock ACCT
 *   drives-home bind ACCT UID         (rudolfd's start, the account signed in: its folders bound again)
 *   drives-home store                 (rudolfd's start, nobody signed in: the downloads' store bound)
 *
 * These are rudolfd's own functions (home_unlock, home_lock, home_bind, dl_store_bind and what they call: the
 * fscrypt key and policy, the Oracle folder of 1.6.0 Part 22, the binds, the downloads' store, the account's Linux
 * user's session of 1.5.9 15.3), built from rudolfd.c as the host tests build them (main renamed): what they do is
 * the same, line for line, as when rudolfd did it as root. Nothing here reads a request of the page: only these
 * arguments and the key. The account is checked again by home_unlock (its name, not the guest or the shared folder);
 * its Linux user's uid comes from the account list (au_uid), never from the caller; UID is the owner of what the
 * sign-in makes outside the folder, 1000 (the desktop's user) or the account's own uid only.
 * The answer is one line on standard output (made, unlocked, busy, or the error), its status the HTTP status less 300
 * (0: all is well). What the programs it starts print goes to standard error (rudolfd's log), so that nothing they
 * leave running holds the answer open.
 *
 * Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#define main rudolfd_main
#include "rudolfd.c"
#undef main

static int res_fd = 1;
static int answer(int http, const char *line)
{
	if (line && *line) { size_t n = strlen(line); if (write(res_fd, line, n) < 0 || write(res_fd, "\n", 1) < 0) {} }
	return http ? (http >= 300 && http <= 552 ? http - 300 : 200) : 0;
}
static int uid_arg(const char *acct, const char *s)
{
	char *e; long u = strtol(s ? s : "", &e, 10); if (!s || !*s || *e) return -1;
	if (u == 1000) return 1000;
	unsigned a = au_uid(acct); return a && u == (long)a ? (int)u : -1;
}
int main(int argc, char **argv)
{
	root_home_here = 1; au_on = 1; signal(SIGPIPE, SIG_IGN);
#ifdef RR_TEST
	/* (host tests: the stand-in places, from the helpers' test configuration) */
	snprintf(home_root, sizeof home_root, "%s", rr_env("RR_TEST_HOMEROOT", HOME_ROOT_DEF)); home_nobind = !strcmp(rr_env("RR_TEST_NOBIND", "0"), "1");
	snprintf(au_map, sizeof au_map, "%s", rr_env("RR_TEST_AUMAP", AU_MAP_DEF)); snprintf(au_homes, sizeof au_homes, "%s", rr_env("RR_TEST_AUHOMES", "/home"));
	snprintf(au_runuser, sizeof au_runuser, "%s", rr_env("RR_TEST_RUNUSER", "/run/user")); snprintf(au_etc, sizeof au_etc, "%s", rr_env("RR_TEST_ETC", "/etc"));
	snprintf(au_display, sizeof au_display, "%s", rr_env("RR_TEST_DISPLAY", "/usr/bin/rudolf-display")); au_on = !strcmp(rr_env("RR_TEST_AUON", "1"), "1");
#endif
	/* the answer on its own descriptor; standard output for what the programs started print: standard error */
	{ int d = fcntl(1, F_DUPFD_CLOEXEC, 3); if (d < 0 || dup2(2, 1) < 0) return answer(500, "the helper could not start"); res_fd = d; }
	const char *op = argc > 1 ? argv[1] : "";
	if (!strcmp(op, "unlock") && argc == 5) {
		const char *acct = argv[2]; int session = !strcmp(argv[3], "1"), uid = uid_arg(acct, argv[4]);
		if ((!session && strcmp(argv[3], "0")) || uid < 0) return answer(400, "bad account");
		char hx[132]; size_t n = 0; ssize_t r;
		while (n < sizeof hx - 1 && ((r = read(0, hx + n, sizeof hx - 1 - n)) > 0 || (r < 0 && errno == EINTR))) if (r > 0) n += (size_t)r;
		{ int nul = open("/dev/null", O_RDONLY | O_CLOEXEC); if (nul >= 0) { dup2(nul, 0); close(nul); } }   /* (nothing started inherits the key's pipe) */
		if (n == 129 && hx[128] == '\n') n = 128;
		unsigned char key[64]; int bad = n != 128;
		for (int i = 0; i < 64 && !bad; i++) { int a = hexval(hx[2 * i]), b = hexval(hx[2 * i + 1]); if (a < 0 || b < 0) bad = 1; else key[i] = (unsigned char)(a << 4 | b); }
		explicit_bzero(hx, sizeof hx);
		if (bad) { explicit_bzero(key, sizeof key); return answer(400, "bad key"); }
		home_uid = uid;
		char err[200] = ""; int made = 0, st = home_unlock(acct, key, session, err, sizeof err, &made); explicit_bzero(key, sizeof key);
		return answer(st, st ? err : made ? "made" : "unlocked");
	}
	if (!strcmp(op, "lock") && argc == 3) {
		int busy = 0, st = home_lock(argv[2], &busy);
		return answer(st, st ? "" : busy ? "busy" : "");
	}
	if (!strcmp(op, "bind") && argc == 4) {
		int uid = uid_arg(argv[2], argv[3]); if (uid < 0 || !home_acct_ok(argv[2])) return answer(400, "bad account");
		home_uid = uid; char err[200] = "";
		return home_bind(argv[2], err, sizeof err) ? answer(500, err) : 0;
	}
	if (!strcmp(op, "store") && argc == 2) { dl_store_bind(); return 0; }
	fprintf(stderr, "usage: drives-home unlock ACCT 0|1 UID | lock ACCT | bind ACCT UID | store\n");
	return answer(400, "unknown action");
}
