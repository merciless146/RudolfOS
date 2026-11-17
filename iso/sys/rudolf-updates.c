/*
 * updates - the root helper for system updates (Rudolf OS 1.6.1, docs/plan-1.6.1.md Part 11): /usr/lib/rudolf/root/updates,
 * run by rudolf-rootd for rudolfd (or by rudolfd itself while it is root and was not started by rudolf-rootd).
 *
 *   updates check|download|rollback|stable-info|to-stable|status|speedtest|boot-ok
 *   updates return-official keep|erase
 *   updates set url|auto VALUE              (the official beta address only through "set channel beta")
 *   updates set channel beta|stable
 *   updates developer off
 *   updates developer on KEY URL            (KEY: 44 characters of base64; URL: letters, digits and /:._-~%+)
 *   updates page-install                    the desktop's page from standard input into /persist/os/current (as
 *                                           rudolfd's POST update did: over 100000 bytes, "<html" in its first 4096)
 *   updates page-rollback                   the page before it back (POST rollback)
 * Each is rudolf-update's own command (it checks every release's signature with the keys this Rudolf OS trusts),
 * run with its arguments as they are, never through a shell; the arguments are checked here again as rudolfd checks
 * them. A refusal, and what changes the system (a download, rolling back, to stable, Return to official, Developer mode,
 * the channel, the desktop's page), are said in the health log.
 *
 * Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include "rudolf-root.h"
#include <ctype.h>
#include <dirent.h>
#include <sys/wait.h>

#define RU      rr_env("RR_TEST_UPDATE", "/usr/bin/rudolf-update")
#define PERSIST rr_env("RR_TEST_PERSIST", "/persist")

static int refuse(const char *what, const char *arg)
{
	char a[80]; size_t k = 0; for (const char *c = arg ? arg : ""; *c && k < sizeof a - 1; c++) a[k++] = isprint((unsigned char)*c) ? *c : '?';
	a[k] = 0;
	rr_health("security", "the updates helper refused %s (%s)", what, a);
	fprintf(stderr, "refused: %s\n", what); return 2;
}
/* an address or a setting's value as rudolfd allows it: letters, digits and / : . _ - ~ % + (no spaces, no quotes) */
static int value_ok(const char *v, size_t max)
{
	if (!v || strlen(v) > max) return 0;
	for (; *v; v++) if (!isalnum((unsigned char)*v) && !strchr("/:._-~%+", *v)) return 0;
	return 1;
}
static int ru(char *const args[])
{
	const char *p = RU; char *argv[8]; int a = 0; argv[a++] = (char *)p;
	for (int i = 0; args[i] && a < 7; i++) argv[a++] = args[i];
	argv[a] = NULL; char *env[] = { RR_PATH, NULL };
	execve(p, argv, env); fprintf(stderr, "rudolf-update could not be started: %s\n", strerror(errno)); return 127;
}
/* rm -rf of a folder of ours, never following a link out of it */
static void rm_tree(const char *path)
{
	struct stat st; if (lstat(path, &st)) return;
	if (S_ISDIR(st.st_mode)) {
		DIR *d = opendir(path); struct dirent *e;
		while (d && (e = readdir(d))) { if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue; char c[PATH_MAX]; snprintf(c, sizeof c, "%s/%s", path, e->d_name); rm_tree(c); }
		if (d) closedir(d);
		rmdir(path);
	} else unlink(path);
}
static int page_install(void)
{
	/* the page, from standard input: as rudolfd checked it (it stays the desktop's own page: this is no signed update) */
	size_t cap = 1 << 20, n = 0; char *b = malloc(cap); ssize_t r;
	if (!b) return 1;
	while ((r = read(0, b + n, cap - n)) > 0 || (r < 0 && errno == EINTR)) { if (r < 0) continue; n += (size_t)r; if (n == cap) { if (cap >= (64u << 20)) { free(b); return refuse("a page over 64 MB", ""); } cap *= 2; char *nb = realloc(b, cap); if (!nb) { free(b); return 1; } b = nb; } }
	if (n < 100000 || !memmem(b, n > 4096 ? 4096 : n, "<html", 5)) { free(b); return refuse("a page that is not a Rudolf OS page", ""); }
	char os[PATH_MAX], nw[PATH_MAX], cur[PATH_MAX], prev[PATH_MAX], f[PATH_MAX];
	snprintf(os, sizeof os, "%s/os", PERSIST); snprintf(nw, sizeof nw, "%s/new", os); snprintf(cur, sizeof cur, "%s/current", os); snprintf(prev, sizeof prev, "%s/previous", os);
	/* (as before: "mkdir -p new && rm -rf previous && [ -d current ] && mv current previous; true") */
	mkdir(os, 0755); if (mkdir(nw, 0755) && errno != EEXIST) { free(b); return 1; }
	rm_tree(prev); struct stat st; if (!lstat(cur, &st) && S_ISDIR(st.st_mode)) { if (rename(cur, prev)) {} }
	snprintf(f, sizeof f, "%s/index.html", nw);
	int fd = open(f, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0644);
	if (fd < 0 || write(fd, b, n) != (ssize_t)n || fsync(fd)) { if (fd >= 0) close(fd); free(b); fprintf(stderr, "could not save the update\n"); return 1; }
	close(fd); free(b);
	if (rename(nw, cur)) { fprintf(stderr, "could not save the update\n"); return 1; }
	char dis[PATH_MAX]; snprintf(dis, sizeof dis, "%s/disabled", os); unlink(dis); sync();
	rr_health("update", "a new desktop page was installed (it is used after a restart)");
	return 0;
}
static int page_rollback(void)
{
	/* (as before: "[ -d previous ] && rm -rf current && mv previous current || { rm -rf current; true; }; sync", whose
	   status was sync's: always 0, so rudolfd always answered "returned to the previous version"; kept) */
	char cur[PATH_MAX], prev[PATH_MAX]; snprintf(cur, sizeof cur, "%s/os/current", PERSIST); snprintf(prev, sizeof prev, "%s/os/previous", PERSIST);
	struct stat st; int rc;
	if (!lstat(prev, &st) && S_ISDIR(st.st_mode)) { rm_tree(cur); rc = rename(prev, cur) ? 1 : 0; if (rc) rm_tree(cur); }
	else { rm_tree(cur); rc = 1; }
	sync(); rr_health("update", "the desktop's page was rolled back%s", rc ? " (there was no previous one: the release's page is used)" : "");
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 2) { fprintf(stderr, "usage: updates ACTION [ARG...]\n"); return 2; }
	const char *op = argv[1]; int n = argc - 2; char **a = argv + 2;
	static const char *const PLAIN[] = { "check", "download", "rollback", "stable-info", "to-stable", "status", "speedtest", "boot-ok", NULL };
	static const char *const LOGGED[] = { "download", "rollback", "to-stable", NULL };
	for (int i = 0; PLAIN[i]; i++) if (!strcmp(op, PLAIN[i])) {
		if (n) return refuse("arguments where there are none", op);
		for (int j = 0; LOGGED[j]; j++) if (!strcmp(op, LOGGED[j])) rr_health("update", "rudolf-update %s was asked for", op);
		char *s[] = { (char *)op, NULL }; return ru(s);
	}
	if (!strcmp(op, "return-official")) {
		if (n != 1 || (strcmp(a[0], "keep") && strcmp(a[0], "erase"))) return refuse("Return to official without keep or erase", n ? a[0] : "");
		rr_health("update", "Return to official Rudolf OS (%s) was asked for", a[0]);
		char *s[] = { "return-official", a[0], NULL }; return ru(s);
	}
	if (!strcmp(op, "set")) {
		if (n != 2) return refuse("a setting without its value", op);
		if (!strcmp(a[0], "channel")) {
			if (strcmp(a[1], "beta") && strcmp(a[1], "stable")) return refuse("a channel that is not beta or stable", a[1]);
			rr_health("update", "the update channel was set to %s", a[1]);
		} else if (!strcmp(a[0], "url") || !strcmp(a[0], "auto")) {
			if (!value_ok(a[1], 600)) return refuse("a setting's value with a character not allowed", a[1]);
			/* (the official beta address only through Get beta versions, which needs an administrator: set channel beta) */
			if (!strcmp(a[0], "url") && strstr(a[1], "merciless146.github.io/RudolfOS/beta")) return refuse("the beta address as an update address", a[1]);
		} else return refuse("an unknown setting", a[0]);
		char *s[] = { "set", a[0], a[1], NULL }; return ru(s);
	}
	if (!strcmp(op, "developer")) {
		if (n == 1 && !strcmp(a[0], "off")) { rr_health("update", "Developer mode was turned off"); char *s[] = { "developer", "off", NULL }; return ru(s); }
		if (n != 3 || strcmp(a[0], "on")) return refuse("Developer mode without on KEY URL or off", n ? a[0] : "");
		if (strlen(a[1]) != 44 || strspn(a[1], "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=") != 44) return refuse("a developer's key that is not 44 characters of base64", a[1]);
		if (!*a[2] || !value_ok(a[2], 300)) return refuse("a developer's update address with a character not allowed", a[2]);
		rr_health("security", "Developer mode was turned on (key %.44s, %.200s)", a[1], a[2]);
		char *s[] = { "developer", "on", a[1], a[2], NULL }; return ru(s);
	}
	if (!strcmp(op, "page-install")) { if (n) return refuse("arguments for page-install", a[0]); return page_install(); }
	if (!strcmp(op, "page-rollback")) { if (n) return refuse("arguments for page-rollback", a[0]); return page_rollback(); }
	return refuse("an unknown action", op);
}
