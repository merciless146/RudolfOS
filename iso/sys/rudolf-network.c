/*
 * network - the root helper for the network (Rudolf OS 1.6.1, docs/plan-1.6.1.md Part 11):
 * /usr/lib/rudolf/root/network, run by rudolf-rootd for rudolfd (or by rudolfd itself while it is root and was not
 * started by rudolf-rootd).
 *
 *   network wifi status|scan|disconnect|reconnect|radios|bands [IF]   /usr/lib/rudolf/wifi.sh ACTION [IF]
 *   network wifi country IF CC          wifi.sh country IF CC (CC: two capital letters, or 00)
 *   network wifi adapter IF             wifi.sh adapter IF (the adapter to use from now on)
 *   network wifi radio IF BSSID on|off  wifi.sh radio IF BSSID on|off (one radio of the network in use)
 *   network wifi band IF 2.4|5|6 on|off wifi.sh band IF BAND on|off (this computer's own radios)
 *   network wifi connect|forget IF      the request (the network's name, its password, its security: one a line, at
 *                                       most 600 bytes) on standard input, never as an argument; written to a file of
 *                                       root's (0600) that wifi.sh reads and removes, as rudolfd wrote it before
 *   network wifi-link IF                the network an adapter is connected to and its signal (wpa_cli status and
 *                                       signal_poll): two lines, the name and the RSSI (the taskbar's network icon)
 *   network dhcp IF                     the adapter up and an address asked for (udhcpc, /usr/lib/rudolf/udhcpc.sh)
 *   network static IF ADDR GW DNS       a fixed address (ip addr flush, add, link up, and the default route via GW);
 *                                       DNS into /etc/resolv.conf. GW and DNS may be empty
 *   network firewall on|off|status      /usr/lib/rudolf/firewall ACTION
 *   network firewall remote on|off      /usr/lib/rudolf/firewall remote on|off (remote access's port)
 *   network firewall duel on PORT|off   /usr/lib/rudolf/firewall duel on PORT|off (1.6.2 Part 30: the LAN game relay's port,
 *                                       1024-65535, from the local network while a game is hosted)
 *   network hotspot status|stop         /usr/lib/rudolf/hotspot status|stop
 *   network hotspot start               the request (name, password, band: one a line, at most 200 bytes) on standard
 *                                       input, written to a file of root's for /usr/lib/rudolf/hotspot start FILE
 *   network metered-state               /usr/lib/rudolf/netplug state
 *   network metered IF on|off|auto      /usr/lib/rudolf/netplug set IF on|off|auto
 *
 * IF: an adapter's name, letters, digits and - _ . : (as rudolfd checks it), at most 16; for wifi it may be empty
 * (wifi.sh then picks the adapter). For dhcp and static (which run ip and udhcpc on it) it must also be one the kernel
 * has (/sys/class/net/IF) and not start with "-". Everything is checked again here, whatever rudolfd checked. Programs
 * are run with their arguments as they are, never a command line for a shell, with no environment but PATH. A refusal
 * is said in the health log (Security).
 *
 * Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include "rudolf-root.h"
#include <ctype.h>
#include <sys/wait.h>

#define SYSFS  rr_env("RR_TEST_SYS", "/sys")
#define LIB    rr_env("RR_TEST_LIB", "/usr/lib/rudolf")
#define RUNDIR rr_env("RR_TEST_RUN", "/run/rudolf")
#define ETC    rr_env("RR_TEST_ETC", "/etc")
#define WPA    "/run/rudolf/wpa"   /* (wpa_supplicant's control sockets, wifi.sh's CTRL) */
/* where the system's commands this helper runs are looked for (never the caller's PATH) */
#ifdef RR_TEST
/* (host tests: only in RR_TEST_BIN, where stand-ins answer for them: a host test must never change this computer's network) */
#define CMD_DIRS rr_env("RR_TEST_BIN", "/nonexistent")
#else
#define CMD_DIRS "/usr/sbin:/usr/bin:/sbin:/bin"
#endif
static int find_cmd(const char *name, char *out, size_t on)
{
	char dirs[PATH_MAX]; snprintf(dirs, sizeof dirs, "%s", CMD_DIRS); char *sv = NULL;
	for (char *d = strtok_r(dirs, ":", &sv); d; d = strtok_r(NULL, ":", &sv)) { snprintf(out, on, "%s/%s", d, name); if (!access(out, X_OK)) return 0; }
	return -1;
}
static int refuse(const char *what, const char *arg)
{
	char a[80]; size_t k = 0; for (const char *c = arg ? arg : ""; *c && k < sizeof a - 1; c++) a[k++] = isprint((unsigned char)*c) ? *c : '?';
	a[k] = 0;
	rr_health("security", "the network helper refused %s (%s)", what, a);
	fprintf(stderr, "refused: %s\n", what); return 2;
}
/* a program run and waited for: its status (127: not started). argv[0]: a full path, or a command looked for in
   CMD_DIRS. out: -1 to keep this helper's own, -2 for /dev/null; err the same */
static int run_prog(char *const argv[], int out, int err)
{
	char exe[PATH_MAX]; if (argv[0][0] == '/') snprintf(exe, sizeof exe, "%s", argv[0]); else if (find_cmd(argv[0], exe, sizeof exe)) return 127;
	pid_t p = fork(); if (p < 0) return 127;
	if (p == 0) {
		int nul = open("/dev/null", O_RDWR | O_CLOEXEC);
		if (out == -2) out = nul;
		if (err == -2) err = nul;
		if (out >= 0 && dup2(out, 1) < 0) _exit(127);
		if (err >= 0 && dup2(err, 2) < 0) _exit(127);
		if (nul >= 0) dup2(nul, 0);
		char *env[] = { RR_PATH, NULL };
		execve(exe, argv, env); _exit(127);
	}
	int st = 0; while (waitpid(p, &st, 0) < 0) if (errno != EINTR) return 127;
	return WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
}
/* a program's standard output (its errors to /dev/null), at most n-1 bytes, into out */
static void prog_out(char *const argv[], char *out, size_t n)
{
	out[0] = 0; int pf[2]; if (pipe2(pf, O_CLOEXEC)) return;
	pid_t p = fork();
	if (p == 0) { close(pf[0]); _exit(run_prog(argv, pf[1], -2)); }
	close(pf[1]); size_t k = 0; ssize_t r;
	char sink[512];
	while ((r = read(pf[0], k < n - 1 ? out + k : sink, k < n - 1 ? n - 1 - k : sizeof sink)) > 0 || (r < 0 && errno == EINTR)) if (r > 0 && k < n - 1) k += (size_t)r;
	out[k] = 0; close(pf[0]);
	if (p > 0) while (waitpid(p, NULL, 0) < 0 && errno == EINTR) {}
}
/* a program of Rudolf OS's own (in LIB) in place of this helper: its output and status are the caller's */
static int exec_lib(const char *prog, char *const args[])
{
	char path[PATH_MAX]; snprintf(path, sizeof path, "%s/%s", LIB, prog);
	char *argv[12]; int a = 0; argv[a++] = path; for (int i = 0; args[i] && a < 11; i++) argv[a++] = args[i]; argv[a] = NULL;
	char *env[] = { RR_PATH, NULL };
	execve(path, argv, env); fprintf(stderr, "%s could not be started: %s\n", prog, strerror(errno)); return 127;
}
/* an adapter's name as rudolfd's safe_word checks it: letters, digits and - _ . :, 1 to max */
static int if_ok(const char *s, size_t max)
{
	if (!s || !*s || strlen(s) > max) return 0;
	for (; *s; s++) if (!isalnum((unsigned char)*s) && !strchr("-_.:", *s)) return 0;
	return 1;
}
/* ...and one the kernel has, not read as an option by ip or udhcpc */
static int if_real(const char *s)
{
	if (!if_ok(s, 16) || *s == '-' || strchr(s, ':') || !strcmp(s, ".") || !strcmp(s, "..")) return 0;
	char p[PATH_MAX]; snprintf(p, sizeof p, "%s/class/net/%s", SYSFS, s); return !access(p, F_OK);
}
static int chars_ok(const char *s, size_t max, const char *chars) { size_t n = s ? strlen(s) : 0; return n <= max && strspn(s, chars) == n; }
/* the request on standard input (1 to max bytes) into a new file of root's, 0600 (an old one removed first) */
static int req_file(const char *path, size_t max)
{
	char b[1024]; size_t n = 0; ssize_t r;
	while ((r = read(0, b + n, sizeof b - n)) > 0 || (r < 0 && errno == EINTR)) if (r > 0) { n += (size_t)r; if (n > max) break; }
	if (!n || n > max) return -1;
	unlink(path);
	int f = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
	if (f < 0 || write(f, b, n) != (ssize_t)n) { if (f >= 0) { close(f); unlink(path); } explicit_bzero(b, sizeof b); return -1; }
	close(f); explicit_bzero(b, sizeof b); return 0;
}
/* the value after KEY= on the first line that starts with it, into out */
static void line_val(const char *text, const char *key, char *out, size_t on)
{
	out[0] = 0; size_t L = strlen(key);
	for (const char *l = text; l && *l; ) {
		const char *e = strchr(l, '\n'); size_t len = e ? (size_t)(e - l) : strlen(l);
		if (len >= L && !strncmp(l, key, L)) { size_t v = len - L; if (v >= on) v = on - 1; memcpy(out, l + L, v); out[v] = 0; return; }
		l = e ? e + 1 : NULL;
	}
}

static int wifi(int n, char **a)
{
	/* (with no adapter at all, as Settings' service restart runs wifi.sh reconnect) */
	if (n == 1 && (!strcmp(a[0], "status") || !strcmp(a[0], "scan") || !strcmp(a[0], "disconnect") || !strcmp(a[0], "reconnect") || !strcmp(a[0], "radios") || !strcmp(a[0], "bands"))) { char *s[] = { a[0], NULL }; return exec_lib("wifi.sh", s); }
	if (n < 2) return refuse("a Wi-Fi request without its adapter", n ? a[0] : "");
	const char *w = a[0], *ifn = a[1];
	if (*ifn && !if_ok(ifn, 16)) return refuse("a Wi-Fi adapter's name that is not one", ifn);
	if (!strcmp(w, "status") || !strcmp(w, "scan") || !strcmp(w, "disconnect") || !strcmp(w, "reconnect") || !strcmp(w, "radios") || !strcmp(w, "bands")) {
		if (n != 2) return refuse("arguments for a Wi-Fi request", a[2]);
		char *s[] = { (char *)w, (char *)ifn, NULL }; return exec_lib("wifi.sh", s);
	}
	if (!strcmp(w, "country")) {
		const char *c = n == 3 ? a[2] : NULL;
		if (!c || strlen(c) != 2 || !((isupper((unsigned char)c[0]) && isupper((unsigned char)c[1])) || !strcmp(c, "00"))) return refuse("a country that is not two capital letters", c ? c : "");
		char *s[] = { "country", (char *)ifn, (char *)c, NULL }; return exec_lib("wifi.sh", s);
	}
	if (!strcmp(w, "adapter")) {
		if (n != 2 || !*ifn) return refuse("an adapter to use that is not named", "");
		char *s[] = { "adapter", (char *)ifn, NULL }; return exec_lib("wifi.sh", s);
	}
	if (!strcmp(w, "radio")) {
		const char *b = n == 4 ? a[2] : NULL, *on = n == 4 ? a[3] : NULL;
		int okb = b && strlen(b) == 17; for (int k = 0; okb && k < 17; k++) okb = (k % 3 == 2) ? b[k] == ':' : isxdigit((unsigned char)b[k]);
		if (!okb || (strcmp(on, "on") && strcmp(on, "off"))) return refuse("a radio that is not one", b ? b : "");
		char *s[] = { "radio", (char *)ifn, (char *)b, (char *)on, NULL }; return exec_lib("wifi.sh", s);
	}
	if (!strcmp(w, "band")) {
		const char *b = n == 4 ? a[2] : NULL, *on = n == 4 ? a[3] : NULL;
		if (!b || (strcmp(b, "2.4") && strcmp(b, "5") && strcmp(b, "6")) || (strcmp(on, "on") && strcmp(on, "off"))) return refuse("a band that is not 2.4, 5 or 6", b ? b : "");
		char *s[] = { "band", (char *)ifn, (char *)b, (char *)on, NULL }; return exec_lib("wifi.sh", s);
	}
	if (!strcmp(w, "connect") || !strcmp(w, "forget")) {
		if (n != 2) return refuse("arguments for a Wi-Fi request", a[2]);
		char f[PATH_MAX]; snprintf(f, sizeof f, "%s/%s", RUNDIR, !strcmp(w, "connect") ? "wifi.req" : "wifi-forget.req");
		if (req_file(f, 600)) { fprintf(stderr, "could not write the request\n"); return refuse("a Wi-Fi request that is empty or over 600 bytes", w); }
		char *s[] = { (char *)w, (char *)ifn, f, NULL }; return exec_lib("wifi.sh", s);
	}
	return refuse("an unknown Wi-Fi action", w);
}

int main(int argc, char **argv)
{
	if (argc < 2) { fprintf(stderr, "usage: network wifi|wifi-link|dhcp|static|firewall|hotspot|metered-state|metered ...\n"); return 2; }
	const char *op = argv[1]; int n = argc - 2; char **a = argv + 2;
	if (!strcmp(op, "wifi")) return wifi(n, a);
	if (!strcmp(op, "wifi-link")) {
		if (n != 1 || !if_ok(a[0], 16)) return refuse("a Wi-Fi adapter's name that is not one", n ? a[0] : "");
		char o[8192], ss[256], rs[64];
		char *s1[] = { "wpa_cli", "-p", WPA, "-i", a[0], "status", NULL }; prog_out(s1, o, sizeof o); line_val(o, "ssid=", ss, sizeof ss);
		char *s2[] = { "wpa_cli", "-p", WPA, "-i", a[0], "signal_poll", NULL }; prog_out(s2, o, sizeof o); line_val(o, "RSSI=", rs, sizeof rs);
		printf("%s\n%s\n", ss, rs); return 0;
	}
	if (!strcmp(op, "dhcp")) {
		if (n != 1 || !if_real(a[0])) return refuse("an adapter that is not one", n ? a[0] : "");
		char sc[PATH_MAX]; snprintf(sc, sizeof sc, "%s/udhcpc.sh", LIB);
		char *s1[] = { "ip", "link", "set", a[0], "up", NULL }; run_prog(s1, -2, -2);
		char *s2[] = { "udhcpc", "-i", a[0], "-n", "-q", "-t", "5", "-O", "43", "-s", sc, NULL }; return run_prog(s2, -2, -2);
	}
	if (!strcmp(op, "static")) {
		if (n != 4 || !if_real(a[0])) return refuse("an adapter that is not one", n ? a[0] : "");
		if (!*a[1] || !chars_ok(a[1], 63, "0123456789./") || !chars_ok(a[2], 63, "0123456789.") || !chars_ok(a[3], 63, "0123456789.")) return refuse("an address that is not one", a[1]);
		char *s1[] = { "ip", "addr", "flush", "dev", a[0], NULL }, *s2[] = { "ip", "addr", "add", a[1], "dev", a[0], NULL }, *s3[] = { "ip", "link", "set", a[0], "up", NULL };
		char *s4[] = { "ip", "route", "replace", "default", "via", a[2], NULL };
		int rc = run_prog(s1, -1, -1); if (!rc) rc = run_prog(s2, -1, -1); if (!rc) rc = run_prog(s3, -1, -1); if (!rc && *a[2]) rc = run_prog(s4, -1, -1);
		if (*a[3]) { char p[PATH_MAX], v[96]; snprintf(p, sizeof p, "%s/resolv.conf", ETC); snprintf(v, sizeof v, "nameserver %s\n", a[3]);
			int f = open(p, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644); if (f >= 0) { if (write(f, v, strlen(v)) < 0) {} close(f); } }
		return rc ? 1 : 0;
	}
	if (!strcmp(op, "firewall")) {
		if (n == 1 && (!strcmp(a[0], "on") || !strcmp(a[0], "off") || !strcmp(a[0], "status"))) { char *s[] = { a[0], NULL }; return exec_lib("firewall", s); }
		if (n == 2 && !strcmp(a[0], "remote") && (!strcmp(a[1], "on") || !strcmp(a[1], "off"))) { char *s[] = { "remote", a[1], NULL }; return exec_lib("firewall", s); }
		if (n == 2 && !strcmp(a[0], "duel") && !strcmp(a[1], "off")) { char *s[] = { "duel", "off", NULL }; return exec_lib("firewall", s); }
		if (n == 3 && !strcmp(a[0], "duel") && !strcmp(a[1], "on") && chars_ok(a[2], 5, "0123456789") && atoi(a[2]) >= 1024 && atoi(a[2]) <= 65535) { char *s[] = { "duel", "on", a[2], NULL }; return exec_lib("firewall", s); }
		return refuse("a firewall action that is not one", n ? a[0] : "");
	}
	if (!strcmp(op, "hotspot")) {
		if (n == 1 && (!strcmp(a[0], "status") || !strcmp(a[0], "stop"))) { char *s[] = { a[0], NULL }; return exec_lib("hotspot", s); }
		if (n == 1 && !strcmp(a[0], "start")) {
			char f[PATH_MAX]; snprintf(f, sizeof f, "%s/hotspot.req", RUNDIR);
			if (req_file(f, 200)) { fprintf(stderr, "could not write the request\n"); return refuse("a hotspot request that is empty or over 200 bytes", ""); }
			char *s[] = { "start", f, NULL }; return exec_lib("hotspot", s);
		}
		return refuse("a hotspot action that is not one", n ? a[0] : "");
	}
	if (!strcmp(op, "metered-state")) {
		if (n) return refuse("arguments for metered-state", a[0]);
		char *s[] = { "state", NULL }; return exec_lib("netplug", s);
	}
	if (!strcmp(op, "metered")) {
		if (n != 2 || !if_ok(a[0], 15) || (strcmp(a[1], "on") && strcmp(a[1], "off") && strcmp(a[1], "auto"))) return refuse("a metered setting that is not one", n ? a[0] : "");
		char *s[] = { "set", a[0], a[1], NULL }; return exec_lib("netplug", s);
	}
	return refuse("an unknown action", op);
}
