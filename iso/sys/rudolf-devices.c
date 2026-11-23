/*
 * devices - the root helper for Bluetooth and Thunderbolt (Rudolf OS 1.6.1, docs/plan-1.6.1.md Part 11):
 * /usr/lib/rudolf/root/devices, run by rudolf-rootd for rudolfd (or by rudolfd itself while it is root and was not
 * started by rudolf-rootd).
 *
 *   devices bt status                     /usr/lib/rudolf/bluetooth status (JSON: the adapter and the devices)
 *   devices bt on|off|visible             the adapter's power (remembered), visible to phones for 3 minutes
 *   devices bt scan SECONDS               look for devices nearby (3 to 60 seconds)
 *   devices bt answer yes|no              the answer to the pairing question
 *   devices bt pair|connect|disconnect|remove MAC   one device (AA:BB:CC:DD:EE:FF, capital hex digits)
 *   devices tb json                       /usr/lib/rudolf/thunderbolt json (the protection and the devices)
 *   devices tb allow|deny ID              approve a device on the cable or not (ID: hex digits and "-", with a "-")
 *   devices tb forget UID                 no longer approve that device by itself (hex digits and "-")
 *   devices scanusb allow|done UID        /usr/lib/rudolf/scanusb allow|done UID (1.6.2 Part 34: the USB scanners SANE's
 *                                         drivers know given to that user while it scans, then given back; 1000-59999)
 *
 * Everything is checked again here, whatever rudolfd checked (as rudolfd's bt_mac_ok and tb_arg_ok do). The scripts are
 * run in place of this helper with their arguments as they are, never a command line for a shell, with no environment
 * but PATH. A refusal is said in the health log (Security).
 *
 * Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include "rudolf-root.h"
#include <ctype.h>

#define LIB rr_env("RR_TEST_LIB", "/usr/lib/rudolf")
static int refuse(const char *what, const char *arg)
{
	char a[80]; size_t k = 0; for (const char *c = arg ? arg : ""; *c && k < sizeof a - 1; c++) a[k++] = isprint((unsigned char)*c) ? *c : '?';
	a[k] = 0;
	rr_health("security", "the devices helper refused %s (%s)", what, a);
	fprintf(stderr, "refused: %s\n", what); return 2;
}
/* a program of Rudolf OS's own (in LIB) in place of this helper: its output and status are the caller's */
static int exec_lib(const char *prog, char *const args[])
{
	char path[PATH_MAX]; snprintf(path, sizeof path, "%s/%s", LIB, prog);
	char *argv[6]; int a = 0; argv[a++] = path; for (int i = 0; args[i] && a < 5; i++) argv[a++] = args[i]; argv[a] = NULL;
	char *env[] = { RR_PATH, NULL };
	execve(path, argv, env); fprintf(stderr, "%s could not be started: %s\n", prog, strerror(errno)); return 127;
}
/* as rudolfd's bt_mac_ok: AA:BB:CC:DD:EE:FF, capital hex digits */
static int mac_ok(const char *m)
{
	if (!m || strlen(m) != 17) return 0;
	for (int i = 0; i < 17; i++) { if (i % 3 == 2) { if (m[i] != ':') return 0; } else if (!isxdigit((unsigned char)m[i]) || islower((unsigned char)m[i])) return 0; }
	return 1;
}
/* as rudolfd's tb_arg_ok: hex digits and "-", 1 to 64; a device's ID (not a unique ID) has a "-" (0-1) */
static int tb_ok(const char *a, int uid)
{
	if (!a || !*a || strlen(a) > 64) return 0;
	for (const char *c = a; *c; c++) if (!(isxdigit((unsigned char)*c) || *c == '-')) return 0;
	return uid || strchr(a, '-') != NULL;
}
static int one_of(const char *s, const char *const set[]) { for (int i = 0; set[i]; i++) if (!strcmp(s, set[i])) return 1; return 0; }

int main(int argc, char **argv)
{
	if (argc < 3) { fprintf(stderr, "usage: devices bt|tb ACTION ...\n"); return 2; }
	const char *area = argv[1], *w = argv[2]; int n = argc - 3; char **a = argv + 3;
	if (!strcmp(area, "bt")) {
		static const char *const plain[] = { "status", "on", "off", "visible", NULL }, *const dev[] = { "pair", "connect", "disconnect", "remove", NULL };
		if (one_of(w, plain)) { if (n) return refuse("arguments for a Bluetooth request", a[0]); char *s[] = { (char *)w, NULL }; return exec_lib("bluetooth", s); }
		if (!strcmp(w, "scan")) {
			if (n != 1 || strspn(a[0], "0123456789") != strlen(a[0]) || !*a[0] || strlen(a[0]) > 2 || atoi(a[0]) < 3 || atoi(a[0]) > 60) return refuse("a search that is not 3 to 60 seconds", n ? a[0] : "");
			char *s[] = { "scan", a[0], NULL }; return exec_lib("bluetooth", s);
		}
		if (!strcmp(w, "answer")) {
			if (n != 1 || (strcmp(a[0], "yes") && strcmp(a[0], "no"))) return refuse("an answer that is not yes or no", n ? a[0] : "");
			char *s[] = { "answer", a[0], NULL }; return exec_lib("bluetooth", s);
		}
		if (one_of(w, dev)) {
			if (n != 1 || !mac_ok(a[0])) return refuse("a Bluetooth device that is not one", n ? a[0] : "");
			char *s[] = { (char *)w, a[0], NULL }; return exec_lib("bluetooth", s);
		}
		return refuse("an unknown Bluetooth action", w);
	}
	if (!strcmp(area, "tb")) {
		if (!strcmp(w, "json")) { if (n) return refuse("arguments for a Thunderbolt request", a[0]); char *s[] = { "json", NULL }; return exec_lib("thunderbolt", s); }
		if (!strcmp(w, "allow") || !strcmp(w, "deny") || !strcmp(w, "forget")) {
			if (n != 1 || !tb_ok(a[0], !strcmp(w, "forget"))) return refuse("a Thunderbolt device that is not one", n ? a[0] : "");
			char *s[] = { (char *)w, a[0], NULL }; return exec_lib("thunderbolt", s);
		}
		return refuse("an unknown Thunderbolt action", w);
	}
	if (!strcmp(area, "scanusb")) {
		if ((!strcmp(w, "allow") || !strcmp(w, "done")) && n == 1 && *a[0] && strlen(a[0]) <= 5 && strspn(a[0], "0123456789") == strlen(a[0]) && atoi(a[0]) >= 1000 && atoi(a[0]) <= 59999) { char *s[] = { (char *)w, a[0], NULL }; return exec_lib("scanusb", s); }
		return refuse("a USB scanner request that is not one", w);
	}
	return refuse("an unknown area", area);
}
