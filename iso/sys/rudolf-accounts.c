/*
 * accounts - the root helper for the accounts' Linux users (Rudolf OS 1.6.1, docs/plan-1.6.1.md Part 11):
 * /usr/lib/rudolf/root/accounts, run by rudolf-rootd for rudolfd (or by rudolfd itself while it is root and was not
 * started by rudolf-rootd).
 *
 *   accounts sync              the page's accounts' ids on standard input, one a line: a Linux user made for each new
 *                              one, the user of one that is gone stopped and removed (its uid never given again, its
 *                              Oracle folder deleted); status 1 when something changed, 0 when not
 *   accounts start             the same at rudolfd's start ("-" alone: the page's list could not be read, so nothing
 *                              made or removed), then every account's user written into /etc and the move of the
 *                              existing accounts' folders to their users (once each)
 *   accounts keyring-list      what the signed-in account's keyring holds (Settings › Privacy), as its own user
 *   accounts keyring-forget ID one of them removed
 *
 * These are rudolfd's own functions (au_sync's work, au_start, api_keyring's command), built from rudolfd.c as the
 * host tests build them (main renamed): they do line for line what rudolfd did as root. The page's storage is never
 * read here: rudolfd reads it and hands the ids on. Each id is checked again (letters, digits and "-", at most 64, not
 * the guest's, not "shared"; at most 256 of them, no line over 80 characters), and what is done with it comes from the
 * account list (/persist/etc/rudolf/account-users): the uids, which account is removed. The keyring's account is the
 * one signed in (RUN/home-session and its line in the list), never one the caller names; ID is checked as rudolfd
 * checks it (app:... or a Secret Service collection's path).
 * The answer on standard output (the keyring's), the status as said; what the programs started print goes to
 * standard error.
 *
 * Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#define main rudolfd_main
#include "rudolfd.c"
#undef main

static int res_fd = 1;
static int refuse(const char *what)
{
	rr_health("security", "the accounts helper refused %s", what);
	fprintf(stderr, "refused: %s\n", what); return 2;
}
/* the ids on standard input: -1 when the first line is "-" (no list), else how many; 2 (refused) as -2 */
static int read_ids(char (*ids)[80], int max)
{
	static char buf[AU_MAX * 82 + 8]; size_t n = 0; ssize_t r;
	while (n < sizeof buf - 1 && ((r = read(0, buf + n, sizeof buf - 1 - n)) > 0 || (r < 0 && errno == EINTR))) if (r > 0) n += (size_t)r;
	if (n >= sizeof buf - 1) return -2;
	buf[n] = 0; if (!strcmp(buf, "-\n")) return -1;
	int k = 0; char *save = NULL;
	if (n && buf[n - 1] != '\n') return -2;
	for (char *l = buf; *l; ) {
		char *e = strchr(l, '\n'); size_t L = (size_t)(e - l); if (!L || L >= 80 || k >= max) return -2;
		memcpy(ids[k], l, L); ids[k][L] = 0;
		if (!home_acct_ok(ids[k]) || !strcmp(ids[k], "guest") || !strcmp(ids[k], "shared")) return -2;
		k++; l = e + 1;
	}
	(void)save; return k;
}
int main(int argc, char **argv)
{
	au_on = 1; signal(SIGPIPE, SIG_IGN);
	root_home_here = 1;   /* (what it calls does the work here, never asks a helper: rudolfd-roothome.inc root_have) */
#ifdef RR_TEST
	/* (host tests: the stand-in places, from the helpers' test configuration) */
	snprintf(home_root, sizeof home_root, "%s", rr_env("RR_TEST_HOMEROOT", HOME_ROOT_DEF)); home_nobind = !strcmp(rr_env("RR_TEST_NOBIND", "0"), "1");
	snprintf(au_map, sizeof au_map, "%s", rr_env("RR_TEST_AUMAP", AU_MAP_DEF)); snprintf(au_homes, sizeof au_homes, "%s", rr_env("RR_TEST_AUHOMES", "/home"));
	snprintf(au_runuser, sizeof au_runuser, "%s", rr_env("RR_TEST_RUNUSER", "/run/user")); snprintf(au_etc, sizeof au_etc, "%s", rr_env("RR_TEST_ETC", "/etc"));
	snprintf(au_display, sizeof au_display, "%s", rr_env("RR_TEST_DISPLAY", "/usr/bin/rudolf-display"));
#endif
	{ int d = fcntl(1, F_DUPFD_CLOEXEC, 3); if (d < 0 || dup2(2, 1) < 0) return 2; res_fd = d; }
	const char *op = argc > 1 ? argv[1] : "";
	if ((!strcmp(op, "sync") || !strcmp(op, "start")) && argc == 2) {
		static char ids[AU_MAX][80]; int ni = read_ids(ids, AU_MAX);
		{ int nul = open("/dev/null", O_RDONLY | O_CLOEXEC); if (nul >= 0) { dup2(nul, 0); close(nul); } }
		if (ni == -2) return refuse("a list of accounts that is not one");
		if (!strcmp(op, "sync")) return ni < 0 ? 0 : au_sync_ids(ids, ni) ? 1 : 0;
		if (ni >= 0) au_sync_ids(ids, ni);
		au_start_users(); return 0;
	}
	if ((!strcmp(op, "keyring-list") && argc == 2) || (!strcmp(op, "keyring-forget") && argc == 3)) {
		int forget = argc == 3;
		if (forget && !au_keyring_id_ok(argv[2])) return refuse("a saved password's id that is not one");
		char au[32], ae[160]; au_app_user(au, sizeof au, ae, sizeof ae);
		if (!strcmp(au, "rudolf")) return 109;   /* (nobody signed in: 409, as rudolfd answers before asking) */
		/* (as api_keyring's commands: the id is checked above, so it is quoted as before) */
		char c[700];
		if (!forget) snprintf(c, sizeof c, "su -s /bin/sh %s -c 'set -a; . %s; set +a; exec " AU_PORTAL " --keyring-list' 2>/dev/null", au, ae);
		else snprintf(c, sizeof c, "su -s /bin/sh %s -c 'set -a; . %s; set +a; exec " AU_PORTAL " --keyring-forget \"$0\"' '%s' 2>&1", au, ae, argv[2]);
		char *o = run_out(c); size_t L = strlen(o); if (write(res_fd, o, L) < 0) {} free(o); return 0;
	}
	fprintf(stderr, "usage: accounts sync | start | keyring-list | keyring-forget ID\n");
	return refuse("an unknown action");
}
