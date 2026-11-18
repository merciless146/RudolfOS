/* origin.h - where this build of Rudolf OS comes from (1.6.0, docs/plan-1.6.0.md Part 19 item 2). GPL-2.0-or-later.
 *
 * /etc/rudolf/origin, written when the system is built (mkrootfs.sh, from ci_release.sh's GITHUB_REPOSITORY and
 * GITHUB_REPOSITORY_ID), one KEY=VALUE a line:
 *   repo=OWNER/NAME        the GitHub repository whose releases this build's updates, Blitzen's model, Debian's base and
 *                          Make a stick's boot files come from, and where system reports go
 *   repo_id=N              its number (GitHub's release-asset addresses carry it)
 *   pages=https://...      its GitHub Pages address: the update channels are pages/beta and pages/stable
 * The official build's file names the official repository; a fork's build names the fork (docs/custom-builds.md). A
 * missing file or key: the official one. The official recovery system and "Return to official Rudolf OS" always use
 * the official addresses (OFFICIAL_*), whatever this file says. RUDOLF_ORIGIN_FILE: another file (tests). */
#ifndef RUDOLF_ORIGIN_H
#define RUDOLF_ORIGIN_H
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OFFICIAL_REPO "merciless146/RudolfOS"
#define OFFICIAL_REPO_ID "1414456577"
#define OFFICIAL_PAGES "https://merciless146.github.io/RudolfOS"

/* a value that may be put into an address: letters, digits and / : . _ - only, and no ".." */
__attribute__((unused)) static int origin_value_ok(const char *v)
{
	if (!*v || strstr(v, "..")) return 0;
	for (const char *c = v; *c; c++) if (!isalnum((unsigned char)*c) && !strchr("/:._-", *c)) return 0;
	return 1;
}
/* KEY's value (repo, repo_id or pages), or the official one */
__attribute__((unused)) static const char *origin_get(const char *key)
{
	static char v[3][200]; static int loaded;
	if (!loaded) {
		loaded = 1; snprintf(v[0], sizeof v[0], OFFICIAL_REPO); snprintf(v[1], sizeof v[1], OFFICIAL_REPO_ID); snprintf(v[2], sizeof v[2], OFFICIAL_PAGES);
		const char *p = getenv("RUDOLF_ORIGIN_FILE"); FILE *f = fopen(p && *p ? p : "/etc/rudolf/origin", "r");
		char l[300];
		while (f && fgets(l, sizeof l, f)) {
			l[strcspn(l, "\r\n")] = 0; char *e = strchr(l, '='); if (!e) continue; *e = 0; const char *val = e + 1;
			int i = !strcmp(l, "repo") ? 0 : !strcmp(l, "repo_id") ? 1 : !strcmp(l, "pages") ? 2 : -1;
			if (i < 0 || !origin_value_ok(val) || strlen(val) >= sizeof v[0]) continue;
			if (i == 0 && (strchr(val, '/') != strrchr(val, '/') || !strchr(val, '/') || val[0] == '/')) continue;   /* (OWNER/NAME) */
			if (i == 1 && strspn(val, "0123456789") != strlen(val)) continue;
			if (i == 2 && strncmp(val, "https://", 8)) continue;
			snprintf(v[i], sizeof v[i], "%s", val);
			{ size_t n = strlen(v[i]); while (n && v[i][n - 1] == '/') v[i][--n] = 0; }
		}
		if (f) fclose(f);
	}
	return !strcmp(key, "repo") ? v[0] : !strcmp(key, "repo_id") ? v[1] : v[2];
}
/* https://github.com/REPO/releases/download/TAG (this build's releases) */
__attribute__((unused)) static void origin_release(char *out, size_t n, const char *tag) { snprintf(out, n, "https://github.com/%s/releases/download/%s", origin_get("repo"), tag); }
#endif
