/* rudolf-steamquit.h: 1.6.2 Part 14 (the owner's Vivobook, 2026-11-22: "Steam is hard to shut down, but Task Manager's Force quit
 * handles it"). Closing Steam's window does not end Steam: Steam goes on running for its tray icon, and Rudolf OS has no tray (neither
 * Standard nor Native shows one), so it is left in the background, out of reach, using memory, until it is force quit. Steam is asked
 * to quit as Steam quits itself ("steam -shutdown" inside its Flatpak), then, if it is still there, Rudolf's usual escalation (TERM,
 * then KILL after 3 s):
 *   - when its last window is closed (rudolf-apps launch: sq_watch), unless one of its games is running (Steam's game launcher,
 *     "reaper SteamLaunch", is in its sandbox): a game is never ended this way;
 *   - at sign-out, before the account's programs are stopped (rudolfd-users.inc au_session_stop).
 * Force quit (Task Manager) is unchanged. Used by rudolf-apps.c and rudolfd (rudolfd-users.inc).
 * Copyright (C) 2026 the Rudolf OS authors. SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUDOLF_STEAMQUIT_H
#define RUDOLF_STEAMQUIT_H
#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#define SQ_APP "com.valvesoftware.Steam"
/* the processes of the user uid in Steam's Flatpak sandbox (its /.flatpak-info names it), in the folder proc (/proc; tests: their own);
   game: set when Steam's game launcher is among them. How many (at most max kept in pids, which may be NULL). */
static __attribute__((unused)) int sq_pids(const char *proc, unsigned uid, int *pids, int max, int *game)
{
	DIR *d = opendir(proc); struct dirent *e; int n = 0; if (game) *game = 0;
	while (d && (e = readdir(d))) {
		char *end; long pid = strtol(e->d_name, &end, 10); if (pid <= 1 || *end) continue;
		char p[400]; struct stat st; snprintf(p, sizeof p, "%s/%ld", proc, pid); if (stat(p, &st) || st.st_uid != uid) continue;
		snprintf(p, sizeof p, "%s/%ld/root/.flatpak-info", proc, pid); FILE *f = fopen(p, "r"); if (!f) continue;
		char b[4096]; size_t k = fread(b, 1, sizeof b - 1, f); fclose(f); b[k] = 0;
		if (!strstr(b, "\nname=" SQ_APP "\n") && strncmp(b, "name=" SQ_APP "\n", sizeof "name=" SQ_APP)) continue;
		if (pids && n < max) pids[n] = (int)pid; n++;
		if (game) { snprintf(p, sizeof p, "%s/%ld/cmdline", proc, pid); f = fopen(p, "r"); if (f) { k = fread(b, 1, sizeof b - 1, f); fclose(f); b[k] = 0;
			for (size_t i = 0; i < k; i++) if (!b[i]) b[i] = ' ';
			if (strstr(b, "SteamLaunch") && strstr(b, "reaper")) *game = 1; } }
	}
	if (d) closedir(d); return n;
}
/* (a process there and not ended: one that has ended but is not yet collected by its parent, a zombie, is gone) */
static __attribute__((unused)) int sq_alive(int pid)
{
	if (kill(pid, 0)) return 0; char p[64], b[512]; snprintf(p, sizeof p, "/proc/%d/stat", pid); FILE *f = fopen(p, "r"); if (!f) return 1;
	size_t k = fread(b, 1, sizeof b - 1, f); fclose(f); b[k] = 0; char *e = strrchr(b, ')'); return !(e && e[1] == ' ' && (e[2] == 'Z' || e[2] == 'X'));
}
/* (which of these processes, kept when Steam was asked to quit, are still Steam's: a Steam started again meanwhile is not touched) */
static __attribute__((unused)) int sq_left(const char *proc, unsigned uid, int *these, int n)
{
	int now[512], m = sq_pids(proc, uid, now, 512, NULL), k = 0; if (m > 512) m = 512;
	for (int i = 0; i < n; i++) { int in = 0; for (int j = 0; j < m && !in; j++) in = now[j] == these[i]; if (in && sq_alive(these[i])) these[k++] = these[i]; }
	return k;
}
/* Rudolf's escalation for what is left of those: TERM, then KILL after 3 s (as au_kill_user). How many were left. */
static __attribute__((unused)) int sq_end(const char *proc, unsigned uid, int *these, int n, void (*pause_ms)(int))
{
	n = sq_left(proc, uid, these, n); if (!n) return 0; int was = n;
	for (int i = 0; i < n; i++) kill(these[i], SIGTERM);
	for (int t = 0; t < 30 && (n = sq_left(proc, uid, these, n)); t++) pause_ms(100);
	for (int i = 0; i < n; i++) kill(these[i], SIGKILL);
	return was;
}
#endif
