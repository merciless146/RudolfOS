/* (included by rudolfd-root.inc, not built on its own: hence .inc) ---------- 1.6.1 (docs/plan-1.6.1.md Part 11, drives
 * part 2): the accounts' encrypted folders and sessions through the drives helper
 * At sign-in an account's folder is unlocked (its key added to the file system, the folder made the first time; its
 * Oracle folder, /persist/oracle/ID, under the same key), its folders bound over /home/rudolf/... for Linux apps, the
 * downloads' store bound, and its own Linux user's session started (au_session_start); at sign-out all of it undone
 * and the key taken out. All of that needs root, so it is one chain asked of the drives helper:
 *   drives home-unlock ACCT 0|1 UID   the key on standard input (128 hex digits, never an argument); 1: with the session
 *   drives home-lock ACCT             drives home-bind ACCT UID (rudolfd's start)          drives home-store
 * UID: what rudolfd's request had as the owner of what the sign-in makes (home_uid: the account's user for the page's
 * home/unlock, the desktop's for the native sign-in, as in 1.6.0), which the helper takes only if it is one of those two.
 * The drives helper checks the account (its name, not the guest or the shared folder, not a removed account of the
 * account list) and hands the work to /usr/lib/rudolf/root/drives-home, which is these same functions of rudolfd
 * (rudolfd-home.inc, rudolfd-users.inc, rudolfd.c's downloads' store) built as a program of their own
 * (rudolf-drives-home.c), so what they do is the same line for line. Its answer: one line (made, unlocked, busy, or
 * the error) and the status HTTP-300 (0 when all is well). rudolfd's API is unchanged: home/unlock, home/lock and the
 * native sign-in and sign-out answer as before.
 * The vault (1.5.0's encrypted folder, /persist/vault) the same way: drives vault-unlock (its key on standard input),
 * vault-feature (whether the drive has ext4's encrypt feature: tune2fs -l of the block device), vault-prepare (the
 * feature added at setup: tune2fs -O encrypt, its output in RUN/vault.err as before), vault-move (the job).
 * Where there is no drives-home (rudolfd built for the host tests, or
 * put on an older system by hand) and rudolfd is root, it does the work itself as before. */
static void dl_store_state(void);
static int root_home_here;   /* 1: this process is drives-home itself (rudolf-drives-home.c sets it) */
/* 1: rudolfd asks the helper NAME (it is there, or rudolfd is not root); 0: it does the work itself as before */
static int root_have(const char *name)
{
	if (root_home_here) return 0;
	char p[PATH_MAX]; snprintf(p, sizeof p, "%s/%s", RD_ROOT_DIR, name);
	return !access(p, F_OK) || geteuid() != 0;
}
static int root_home_fwd(void) { return root_have("drives-home"); }
/* the helper's status (-1: it could not be asked), its one line of answer in out; in: its standard input */
static int root_home_ask(const char *const argv[], const char *in, size_t inl, char *out, size_t on)
{
	int pi[2], po[2]; out[0] = 0;
	if (pipe2(po, O_CLOEXEC)) return -1;
	if (pipe2(pi, O_CLOEXEC)) { close(po[0]); close(po[1]); return -1; }
	int ok = !inl || write(pi[1], in, inl) == (ssize_t)inl; close(pi[1]);
	int st = ok ? root_call(argv, pi[0], po[1], 2) : -1; close(pi[0]); close(po[1]);
	size_t n = 0; ssize_t r;
	while (n < on - 1 && ((r = read(po[0], out + n, on - 1 - n)) > 0 || (r < 0 && errno == EINTR))) if (r > 0) n += (size_t)r;
	close(po[0]); out[n] = 0; out[strcspn(out, "\n")] = 0;
	return st;
}
static int root_home_http(int st) { return st >= 100 && st <= 252 ? st + 300 : st == 2 ? 400 : 500; }
static int root_home_unlock(const char *acct, const unsigned char *key, int session, int uid, char *err, size_t en, int *made)
{
	char hx[129], us[16], o[200]; for (int i = 0; i < 64; i++) sprintf(hx + 2 * i, "%02x", key[i]);
	snprintf(us, sizeof us, "%d", uid);
	const char *av[] = { "drives", "home-unlock", acct, session ? "1" : "0", us, NULL };
	int st = root_home_ask(av, hx, 128, o, sizeof o); explicit_bzero(hx, sizeof hx);
	if (made) *made = 0;
	if (session) dl_store_state();
	if (!st) { if (made) *made = !strcmp(o, "made"); return 0; }
	snprintf(err, en, "%s", st == 2 ? "bad account" : *o && st >= 100 && st <= 252 ? o : "the account's folder could not be opened (the drives helper did not run)");
	return root_home_http(st);
}
static int root_home_lock(const char *acct, int *busy)
{
	const char *av[] = { "drives", "home-lock", acct, NULL }; char o[300];
	int st = root_home_ask(av, NULL, 0, o, sizeof o);
	dl_store_state();
	if (busy) *busy = !st && !strcmp(o, "busy");
	return st ? root_home_http(st) : 0;
}
static int root_home_bind(const char *acct, int uid, char *err, size_t en)
{
	char us[16], o[200]; snprintf(us, sizeof us, "%d", uid);
	const char *av[] = { "drives", "home-bind", acct, us, NULL };
	int st = root_home_ask(av, NULL, 0, o, sizeof o);
	dl_store_state();
	if (!st) return 0;
	snprintf(err, en, "%s", *o && st >= 100 && st <= 252 ? o : "the account's folders could not be shared with Linux apps (the drives helper did not run)");
	return -1;
}
static void root_home_store(void)
{
	const char *av[] = { "drives", "home-store", NULL }; char o[300];
	if (root_home_ask(av, NULL, 0, o, sizeof o)) fprintf(stderr, "rudolfd: the downloads store was not bound (the drives helper)\n");
	dl_store_state();
}
static int root_vault_unlock(const unsigned char *key, char *m, size_t mn)
{
	char hx[129], o[200]; for (int i = 0; i < 64; i++) sprintf(hx + 2 * i, "%02x", key[i]);
	const char *av[] = { "drives", "vault-unlock", NULL };
	int st = root_home_ask(av, hx, 128, o, sizeof o); explicit_bzero(hx, sizeof hx);
	dl_store_state();   /* (the unlock binds the downloads' store again: dl_home_boot) */
	if (!st) return 0;
	snprintf(m, mn, "%s", *o && st >= 100 && st <= 252 ? o : "the encrypted folder could not be opened (the drives helper did not run)");
	return root_home_http(st);
}
static int root_vault_feature(void)
{
	const char *av[] = { "drives", "vault-feature", NULL }; char o[32];
	return !root_home_ask(av, NULL, 0, o, sizeof o) && !strcmp(o, "1");
}
static int root_vault_prepare(void)
{
	const char *av[] = { "drives", "vault-prepare", NULL }; char o[32];
	return root_home_ask(av, NULL, 0, o, sizeof o) ? 1 : 0;
}
static void root_vault_move(void) { const char *av[] = { "drives", "vault-move", NULL }; root_job("vault", av); }
/* 1.6.1 (Part 11, accounts): the accounts' Linux users (rudolfd-users.inc's au_start and au_sync) through the accounts
 * helper (rudolf-accounts.c: /usr/lib/rudolf/root/accounts). rudolfd reads the page's accounts (it reads the page's
 * storage; the helper never does) and hands their ids on, one a line on standard input ("-": the page's list could not
 * be read); the helper checks each again and does the rest from the account list (/persist/etc/rudolf/account-users):
 *   accounts sync      the users made and removed as the accounts are (status 1: something changed)
 *   accounts start     at rudolfd's start: that, then every user into /etc and the move of existing accounts' folders
 *   accounts keyring-list | keyring-forget ID   Settings › Privacy › Saved passwords for apps, as the account's user
 * -2: the helper did not start (not there, or refused by its confinement: plan Part 20's list without it) and rudolfd
 * is root: rudolfd does it itself, as before. */
static int root_accounts(const char *op, char (*ids)[80], int ni)
{
	str in = {0}; sadd(&in, "%s", ni < 0 ? "-\n" : "");
	for (int i = 0; i < ni; i++) sadd(&in, "%s\n", ids[i]);
	const char *av[] = { "accounts", op, NULL }; char o[64];
	int st = root_home_ask(av, in.b, in.n, o, sizeof o); free(in.b);
	if ((st < 0 || st == RR_NOSTART) && geteuid() == 0) { fprintf(stderr, "rudolfd: the accounts helper did not start: rudolfd does it itself\n"); return -2; }
	return st == 1 ? 1 : 0;
}
