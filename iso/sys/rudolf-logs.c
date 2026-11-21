/*
 * logs - the root helper that reads the kernel's messages (Rudolf OS 1.6.1, docs/plan-1.6.1.md Part 11):
 * /usr/lib/rudolf/root/logs, run by rudolf-rootd for rudolfd (or by rudolfd itself while it is root and was not
 * started by rudolf-rootd). Reading them needs root (CAP_SYSLOG) where kernel.dmesg_restrict is 1.
 *
 *   logs dmesg        dmesg (each message with its time)
 *   logs dmesg-raw    dmesg -r (each with its level, <0> to <7>)
 *
 * Nothing else: no argument is taken, nothing is written. rudolfd picks the lines it wants (crashes, Wi-Fi's, the
 * graphics' firmware) as before. dmesg is looked for in the system's folders only, with no environment but PATH.
 *
 * Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include "rudolf-root.h"

#ifdef RR_TEST
#define CMD_DIRS rr_env("RR_TEST_BIN", "/nonexistent")
#else
#define CMD_DIRS "/usr/sbin:/usr/bin:/sbin:/bin"
#endif
int main(int argc, char **argv)
{
	if (argc != 2 || (strcmp(argv[1], "dmesg") && strcmp(argv[1], "dmesg-raw"))) {
		rr_health("security", "the logs helper refused an action that is not dmesg or dmesg-raw (%.40s)", argc > 1 && strspn(argv[1], "abcdefghijklmnopqrstuvwxyz-") == strlen(argv[1]) ? argv[1] : "?");
		fprintf(stderr, "refused: an action that is not dmesg or dmesg-raw\n"); return 2;
	}
	char dirs[PATH_MAX], exe[PATH_MAX]; snprintf(dirs, sizeof dirs, "%s", CMD_DIRS); char *sv = NULL; int found = 0;
	for (char *d = strtok_r(dirs, ":", &sv); d; d = strtok_r(NULL, ":", &sv)) { snprintf(exe, sizeof exe, "%s/dmesg", d); if (!access(exe, X_OK)) { found = 1; break; } }
	if (!found) { fprintf(stderr, "dmesg is not there\n"); return 127; }
	char *a1[] = { "dmesg", NULL }, *a2[] = { "dmesg", "-r", NULL }, *env[] = { RR_PATH, NULL };
	execve(exe, strcmp(argv[1], "dmesg") ? a2 : a1, env); fprintf(stderr, "dmesg could not be started: %s\n", strerror(errno)); return 127;
}
