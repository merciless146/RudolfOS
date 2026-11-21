/* 1.6.1 (docs/plan-1.6.1.md Part 20): the probe of confine161_t.sh. Built with RUN and PERSIST as rudolfd's, and the
 * confinement's own lists (rudolfd-confine-tables.h, rudolfd-confine-narrow.h, rudolf-rootd-confine.h):
 *   confineprobe bpf            every system call number (0-511) of each table (x86-64, i386, x32) through each filter's
 *                               program, run here by a small BPF interpreter, against the lists: one line per mistake
 *   confineprobe narrow ROOT    the narrow layer put on with ROOT in front of its folders, then each allowed and refused
 *                               path and system call tried for real: "PASS what" / "FAIL what"
 *   confineprobe tree           layer 1 (rudolfd's whole tree), then its refused calls and its capabilities
 *   confineprobe helper NAME    rudolf-rootd's confinement for that helper
 */
#define _GNU_SOURCE
#include "rudolf-confine.h"
#include <sched.h>
#include <signal.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifndef RUN
#define RUN "/run/rudolf"
#endif
#ifndef PERSIST
#define PERSIST "/persist"
#endif
#include "rudolfd-confine-tables.h"
#include "rudolfd-confine-narrow.h"
#include "rudolf-rootd-confine.h"

static int fails;
static void ok(const char *what, int c) { printf("%s %s\n", c ? "PASS" : "FAIL", what); if (!c) fails++; fflush(stdout); }

/* ---- a classic BPF interpreter for what cf_bpf makes (LD|W|ABS, JMP JEQ/JGE/JA, RET) ---- */
static uint32_t run_bpf(const struct sock_filter *p, int n, uint32_t arch, uint32_t nr)
{
	uint32_t A = 0; int pc = 0;
	for (int steps = 0; steps < 10000 && pc >= 0 && pc < n; steps++) {
		const struct sock_filter *f = &p[pc];
		switch (f->code) {
		case BPF_LD | BPF_W | BPF_ABS: A = f->k == offsetof(struct seccomp_data, arch) ? arch : f->k == offsetof(struct seccomp_data, nr) ? nr : 0xdeadbeef; pc++; break;
		case BPF_JMP | BPF_JEQ | BPF_K: pc += 1 + (A == f->k ? f->jt : f->jf); break;
		case BPF_JMP | BPF_JGE | BPF_K: pc += 1 + (A >= f->k ? f->jt : f->jf); break;
		case BPF_JMP | BPF_JA: pc += 1 + (int)f->k; break;
		case BPF_RET | BPF_K: return f->k;
		default: return 0xffffffffu;
		}
	}
	return 0xfffffffeu;
}
static int in(const int *l, int n, int x) { for (int i = 0; i < n; i++) if (l[i] == x) return 1; return 0; }
static int len(const int *l) { int n = 0; while (l[n] >= 0) n++; return n; }
/* one filter: every number of each table; returns the mistakes */
static int check_filter(const char *name, const int *nr, int nn, const int *i386, int ni, int allow, int mode)
{
	static struct sock_filter p[CF_MAXPROG]; int n = cf_bpf(p, CF_MAXPROG, nr, nn, i386, ni, allow, mode), bad = 0;
	if (n < 0) { printf("BPF %s: the program is too long\n", name); return 1; }
	const uint32_t RF = mode == CF_ON ? (SECCOMP_RET_ERRNO | EPERM) : SECCOMP_RET_LOG, NO = SECCOMP_RET_ERRNO | EPERM;
	for (uint32_t x = 0; x < 512; x++) {
		uint32_t want = in(nr, nn, (int)x) == !!allow ? SECCOMP_RET_ALLOW : RF, got = run_bpf(p, n, CF_ARCH, x);
		if (got != want) { printf("BPF %s: call %u gives %#x, not %#x\n", name, x, got, want); bad++; }
#ifdef __x86_64__
		want = i386 ? (in(i386, ni, (int)x) == !!allow ? SECCOMP_RET_ALLOW : RF) : RF;
		got = run_bpf(p, n, AUDIT_ARCH_I386, x);
		if (got != want) { printf("BPF %s: 32-bit call %u gives %#x, not %#x\n", name, x, got, want); bad++; }
		got = run_bpf(p, n, CF_ARCH, CF_X32_BIT | x);
		if (got != NO) { printf("BPF %s: x32 call %u gives %#x, not refused\n", name, x, got); bad++; }
#endif
	}
	if (run_bpf(p, n, 0x12345678u, 0) != NO) { printf("BPF %s: another machine's calls are not refused\n", name); bad++; }
	printf("BPF %s: %d instructions, 512 calls x 3 tables checked, %d wrong\n", name, n, bad);
	return bad;
}

/* /proc/self/status, opened before any confinement (Landlock would refuse it under a test folder) and read again */
static int stfd = -1;
static const char *status_line(const char *key)
{
	static char b[8192]; ssize_t n = pread(stfd, b, sizeof b - 1, 0); if (n <= 0) return NULL; b[n] = 0;
	size_t k = strlen(key); for (char *l = b; l && *l; l = strchr(l, '\n') ? strchr(l, '\n') + 1 : NULL) if (!strncmp(l, key, k)) return l + k;
	return NULL;
}
static uint64_t status_hex(const char *key) { const char *v = status_line(key); return v ? strtoull(v, NULL, 16) : ~0ULL; }
static long status_num(const char *key) { const char *v = status_line(key); return v ? strtol(v, NULL, 10) : -1; }
static int eperm(long r) { return r < 0 && errno == EPERM; }
static int writes(const char *p) { int f = open(p, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600); if (f < 0) return 0; int w = write(f, "x", 1) == 1; close(f); return w; }

int main(int argc, char **argv)
{
	stfd = open("/proc/self/status", O_RDONLY | O_CLOEXEC);
	if (argc >= 2 && !strcmp(argv[1], "bpf")) {
		int bad = check_filter("layer 1 (rudolfd's tree, on)", CONFINE_TREE_DENY, len(CONFINE_TREE_DENY), CONFINE_TREE_DENY32, len(CONFINE_TREE_DENY32), 0, CF_ON);
		bad += check_filter("layer 1 (rudolfd's tree, log)", CONFINE_TREE_DENY, len(CONFINE_TREE_DENY), CONFINE_TREE_DENY32, len(CONFINE_TREE_DENY32), 0, CF_LOG);
		bad += check_filter("layer 2 (narrow, on)", CONFINE_NARROW, len(CONFINE_NARROW), NULL, 0, 1, CF_ON);
		bad += check_filter("layer 2 (narrow, log)", CONFINE_NARROW, len(CONFINE_NARROW), NULL, 0, 1, CF_LOG);
		bad += check_filter("the helpers (on)", CF_HELPER_DENY, len(CF_HELPER_DENY), NULL, 0, 0, CF_ON);
		/* what the narrow list must never have */
		static const int NEVER[] = { SYS_execve, SYS_execveat, SYS_mount, SYS_umount2, SYS_ptrace, SYS_kill, SYS_tkill, SYS_tgkill,
			SYS_setns, SYS_unshare, SYS_chroot, SYS_pivot_root, SYS_reboot, SYS_kexec_load, SYS_init_module, SYS_finit_module,
			SYS_delete_module, SYS_bpf, SYS_perf_event_open, SYS_process_vm_writev, SYS_process_vm_readv, SYS_keyctl,
			SYS_add_key, SYS_request_key, SYS_swapon, SYS_swapoff, SYS_settimeofday, SYS_clock_settime, SYS_open_by_handle_at,
			SYS_userfaultfd, SYS_io_uring_setup, SYS_fsopen, SYS_fsmount, SYS_move_mount, SYS_open_tree, SYS_mount_setattr,
			SYS_pidfd_send_signal, SYS_rt_sigqueueinfo, SYS_rt_tgsigqueueinfo, SYS_acct, SYS_quotactl, -1 };
		for (int i = 0; NEVER[i] >= 0; i++) if (in(CONFINE_NARROW, len(CONFINE_NARROW), NEVER[i])) { printf("BPF the narrow list has call %d, which it must never have\n", NEVER[i]); bad++; }
		/* and the tree's list must never refuse what rudolfd's programs need */
		static const int NEED[] = { SYS_execve, SYS_clone, SYS_mount, SYS_umount2, SYS_init_module, SYS_finit_module, SYS_delete_module,
			SYS_swapon, SYS_reboot, SYS_settimeofday, SYS_syslog, SYS_ptrace, SYS_unshare, SYS_pivot_root, SYS_keyctl, -1 };
		for (int i = 0; NEED[i] >= 0; i++) if (in(CONFINE_TREE_DENY, len(CONFINE_TREE_DENY), NEED[i])) { printf("BPF layer 1 refuses call %d, which rudolfd's programs need\n", NEED[i]); bad++; }
		printf("%s bpf: %d wrong\n", bad ? "FAIL" : "PASS", bad);
		return !!bad;
	}
	if (argc >= 3 && !strcmp(argv[1], "narrow")) {
		const char *R = argv[2]; char p[1024];
		int abi = cf_landlock_abi(); uint64_t eff0 = status_hex("CapEff:\t");
		ok("before: no confinement (seccomp 0)", status_num("Seccomp:\t") == 0);
		if (!prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0) && prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) { ok("no_new_privs", 0); return 1; }
		int ll = cf_landlock(CONFINE_WRITE, (int)(sizeof CONFINE_WRITE / sizeof *CONFINE_WRITE), R, CF_SCOPE_SIGNAL | CF_SCOPE_ABSTRACT, 1, NULL);
		ok(abi ? "Landlock put on" : "no Landlock in this kernel: skipped", abi ? ll == 0 : ll == 1);
		ok("capabilities cut", cf_caps(CONFINE_NARROW_KEEP) == 0);
		ok("the narrow system-call list put on", cf_seccomp(CONFINE_NARROW, len(CONFINE_NARROW), NULL, 0, 1, CF_ON) == 0);
		ok("seccomp: a filter (mode 2), no_new_privs", status_num("Seccomp:\t") == 2 && status_num("NoNewPrivs:\t") == 1);
		ok("capabilities: effective and permitted are the narrow list's (as far as this process had them)", status_hex("CapEff:\t") == (CONFINE_NARROW_KEEP & eff0) && status_hex("CapPrm:\t") == (CONFINE_NARROW_KEEP & eff0));
		ok("...and the bounding set too (nothing started could get more)", (status_hex("CapBnd:\t") & ~CONFINE_NARROW_KEEP) == 0);
		/* the allowed folders: a file made, written, renamed, removed in each */
		for (size_t i = 0; i < sizeof CONFINE_WRITE / sizeof *CONFINE_WRITE; i++) {
			const struct cf_rule *w = &CONFINE_WRITE[i]; char what[300];
			snprintf(p, sizeof p, "%s%s", R, w->path); struct stat st;
			if (stat(p, &st)) { snprintf(what, sizeof what, "%s: there (the test made it)", w->path); ok(what, 0); continue; }
			if (w->access == CF_R) { snprintf(what, sizeof what, "%s: read", w->path); ok(what, S_ISDIR(st.st_mode) ? access(p, R_OK) == 0 : 1); continue; }
			if (!S_ISDIR(st.st_mode)) { int f = open(p, O_WRONLY | O_CLOEXEC); snprintf(what, sizeof what, "%s: written (a file rule)", w->path); ok(what, f >= 0); if (f >= 0) close(f); continue; }
			char f1[1100], f2[1100], d[1100]; snprintf(f1, sizeof f1, "%s/cf-probe", p); snprintf(f2, sizeof f2, "%s/cf-probe2", p); snprintf(d, sizeof d, "%s/cf-dir", p);
			int good = writes(f1) && !rename(f1, f2) && !unlink(f2) && !mkdir(d, 0700) && !rmdir(d);
			snprintf(what, sizeof what, "%s: a file made, written, renamed and removed; a folder made and removed", w->path); ok(what, good);
		}
		/* refused: writing anywhere else (each outside folder the test made), running a program, mounting */
		static const char *const OUTSIDE[] = { "/etc", "/usr", "/persist", "/persist/os", "/persist/debian", "/run", "/var", "/boot", NULL };
		for (int i = 0; OUTSIDE[i]; i++) {
			char what[300], f[1100]; snprintf(p, sizeof p, "%s%s", R, OUTSIDE[i]); snprintf(f, sizeof f, "%s/cf-probe", p);
			errno = 0; int w = writes(f); int e = errno; if (w) unlink(f);
			snprintf(what, sizeof what, "%s: a file made there is refused (Landlock)", OUTSIDE[i]);
			if (abi) ok(what, !w && e == EACCES); else ok("(no Landlock: writing elsewhere is not limited)", 1);
		}
		snprintf(p, sizeof p, "%s/etc/existing", R);
		{ int f = open(p, O_WRONLY | O_CLOEXEC); int e = errno; if (f >= 0) close(f);
		  if (abi) ok("/etc/existing: an existing file outside is not written either", f < 0 && e == EACCES); }
		snprintf(p, sizeof p, "%s/etc/existing", R);
		{ int f = open(p, O_RDONLY | O_CLOEXEC); ok("reading outside the folders is allowed (as root reads today)", f >= 0); if (f >= 0) close(f); }
		char *av[] = { "/bin/true", NULL };
		ok("running a program: refused (execve)", eperm(syscall(SYS_execve, "/bin/true", av, NULL)));
		ok("execveat: refused", eperm(syscall(SYS_execveat, AT_FDCWD, "/bin/true", av, NULL, 0)));
		ok("mount: refused", eperm(mount("none", "/mnt", "tmpfs", 0, NULL)));
		ok("umount: refused", eperm(umount2("/proc", MNT_DETACH)));
		ok("ptrace: refused", eperm(syscall(SYS_ptrace, 16 /* PTRACE_ATTACH */, 1, 0, 0)));
		ok("kill of another process: refused", eperm(kill(1, 0)));
		ok("tgkill: refused", eperm(syscall(SYS_tgkill, 1, 1, 0)));
		ok("setns: refused", eperm(syscall(SYS_setns, 0, 0)));
		ok("unshare: refused", eperm(unshare(CLONE_NEWNS)));
		ok("chroot: refused", eperm(chroot("/")));
		ok("reboot: refused", eperm(syscall(SYS_reboot, 0, 0, 0, NULL)));
		ok("init_module/finit_module: refused", eperm(syscall(SYS_finit_module, -1, "", 0)) && eperm(syscall(SYS_init_module, NULL, 0, "")));
		ok("kexec_load: refused", eperm(syscall(SYS_kexec_load, 0, 0, NULL, 0)));
		ok("bpf: refused", eperm(syscall(SYS_bpf, 0, NULL, 0)));
		ok("keyctl: refused", eperm(syscall(SYS_keyctl, 0, 0, 0, 0, 0)));
		ok("io_uring_setup: refused", eperm(syscall(SYS_io_uring_setup, 1, NULL)));
		ok("userfaultfd: refused", eperm(syscall(SYS_userfaultfd, 0)));
		ok("settimeofday: refused", eperm(syscall(SYS_settimeofday, NULL, NULL)));
		ok("swapon: refused", eperm(syscall(SYS_swapon, "/nonexistent", 0)));
		ok("a 32-bit call (int 0x80 getpid): refused", ({ long r; __asm__ volatile("int $0x80" : "=a"(r) : "a"(20) : "memory"); r == -EPERM; }));
		/* TCP of its own: refused (Landlock's network rules, ABI 4) */
		if (abi >= 4) {
			int s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0); struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(1), .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
			errno = 0; int c = s >= 0 ? connect(s, (struct sockaddr *)&a, sizeof a) : -1; int e = errno; if (s >= 0) close(s);
			ok("a TCP connection of its own: refused (Landlock)", c < 0 && e == EACCES);
			s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0); a.sin_port = htons(48611);
			errno = 0; c = s >= 0 ? bind(s, (struct sockaddr *)&a, sizeof a) : -1; e = errno; if (s >= 0) close(s);
			ok("...and a port of its own: refused", c < 0 && e == EACCES);
		}
		/* allowed: what a request does */
		ok("getpid, time, getrandom: allowed", getpid() > 0 && time(NULL) > 0 && syscall(SYS_getrandom, p, 8, 0) == 8);
		int sv[2]; ok("socketpair (rudolf-rootd's requests): allowed", socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sv) == 0);
		pid_t ch = fork(); if (ch == 0) { _exit(setgid(65534) || setuid(65534) ? 3 : 0); }   /* (rudolfd-drop.inc's child) */
		int st = 0; waitpid(ch, &st, 0);
		ok("fork, and a child becoming another user (the dropped file's opener): allowed", ch > 0 && WIFEXITED(st) && WEXITSTATUS(st) == 0);
		printf("%s narrow: %d failed\n", fails ? "FAIL" : "PASS", fails);
		return !!fails;
	}
	if (argc >= 2 && !strcmp(argv[1], "tree")) {
		/* (what the kernel answers before, for the calls the filter must leave alone: a test machine may refuse them itself) */
		errno = 0; syscall(SYS_swapon, "/nonexistent", 0); int sw0 = errno; errno = 0; syscall(SYS_finit_module, -1, "", 0); int fm0 = errno;
		ok("the bounding set cut", cf_caps_bound(~CONFINE_TREE_DROP) == 0);
		ok("layer 1's deny list put on", cf_seccomp(CONFINE_TREE_DENY, len(CONFINE_TREE_DENY), CONFINE_TREE_DENY32, len(CONFINE_TREE_DENY32), 0, CF_ON) == 0);
		ok("the dropped capabilities are gone from the bounding set, the rest kept", (status_hex("CapBnd:\t") & CONFINE_TREE_DROP) == 0 && (status_hex("CapBnd:\t") & CFB(CAP_SYS_ADMIN)));
		ok("...and this process keeps its own (root's work goes on)", (status_hex("CapEff:\t") & CFB(CAP_SYS_ADMIN)) != 0);
		ok("bpf: refused", eperm(syscall(SYS_bpf, 0, NULL, 0)));
		ok("perf_event_open: refused", eperm(syscall(SYS_perf_event_open, NULL, 0, -1, -1, 0)));
		ok("kexec_load, kexec_file_load: refused", eperm(syscall(SYS_kexec_load, 0, 0, NULL, 0)) && eperm(syscall(SYS_kexec_file_load, -1, -1, 0, "", 0)));
		ok("open_by_handle_at: refused", eperm(syscall(SYS_open_by_handle_at, -1, NULL, 0)));
		ok("iopl, ioperm: refused", eperm(syscall(SYS_iopl, 0)) && eperm(syscall(SYS_ioperm, 0, 0, 0)));
		ok("acct, quotactl: refused", eperm(syscall(SYS_acct, NULL)) && eperm(syscall(SYS_quotactl, 0, NULL, 0, NULL)));
		ok("process_vm_writev: refused", eperm(syscall(SYS_process_vm_writev, 1, NULL, 0, NULL, 0, 0)));
		ok("32-bit acct (int 0x80): refused", ({ long r; __asm__ volatile("int $0x80" : "=a"(r) : "a"(51), "b"(0) : "memory"); r == -EPERM; }));
		/* what rudolfd's programs need still works: a program run, a mount in a namespace of its own, swapon asked */
		pid_t c = fork(); if (c == 0) { char *av[] = { "/bin/true", NULL }; execv("/bin/true", av); _exit(9); }
		int st; waitpid(c, &st, 0); ok("a program started (as the shell's commands are)", WIFEXITED(st) && WEXITSTATUS(st) == 0);
		c = fork(); if (c == 0) { _exit(unshare(CLONE_NEWNS) || mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) || mount("none", "/mnt", "tmpfs", 0, NULL) ? 9 : 0); }
		waitpid(c, &st, 0); ok("a mount (in a namespace of its own: the Debian container, the binds)", WIFEXITED(st) && WEXITSTATUS(st) == 0);
		errno = 0; syscall(SYS_swapon, "/nonexistent", 0); ok("swapon is not refused by the filter (the memory settings' zram): the kernel's own answer", errno == sw0);
		errno = 0; syscall(SYS_finit_module, -1, "", 0); ok("finit_module is not refused by the filter (the camera's driver): the kernel's own answer", errno == fm0);
		printf("%s tree: %d failed\n", fails ? "FAIL" : "PASS", fails);
		return !!fails;
	}
	if (argc >= 3 && !strcmp(argv[1], "helper")) {
		uint64_t keep = 0; for (int i = 0; HELPER_CAPS[i].name; i++) if (!strcmp(HELPER_CAPS[i].name, argv[2])) keep = HELPER_CAPS[i].keep;
		if (!keep) { ok("a known helper", 0); return 1; }
		uint64_t eff0 = status_hex("CapEff:\t");
		ok("capabilities cut", cf_caps(keep) == 0);
		ok("the helpers' deny list put on", cf_seccomp(CF_HELPER_DENY, len(CF_HELPER_DENY), NULL, 0, 0, CF_ON) == 0);
		ok("its capabilities and no others (effective, permitted, bounding)", status_hex("CapEff:\t") == (keep & eff0) && status_hex("CapPrm:\t") == (keep & eff0) && (status_hex("CapBnd:\t") & ~keep) == 0);
		ok("ptrace: refused", eperm(syscall(SYS_ptrace, 16, 1, 0, 0)));
		ok("modules: refused", eperm(syscall(SYS_finit_module, -1, "", 0)) && eperm(syscall(SYS_delete_module, "x", 0)));
		ok("reboot, the clock, swap: refused", eperm(syscall(SYS_reboot, 0, 0, 0, NULL)) && eperm(syscall(SYS_settimeofday, NULL, NULL)) && eperm(syscall(SYS_swapon, "/x", 0)));
		ok("kexec, bpf: refused", eperm(syscall(SYS_kexec_load, 0, 0, NULL, 0)) && eperm(syscall(SYS_bpf, 0, NULL, 0)));
		pid_t c = fork(); if (c == 0) { char *av[] = { "/bin/true", NULL }; execv("/bin/true", av); _exit(9); }
		int st; waitpid(c, &st, 0); ok("its scripts still run", WIFEXITED(st) && WEXITSTATUS(st) == 0);
		printf("%s helper %s: %d failed\n", fails ? "FAIL" : "PASS", argv[2], fails);
		return !!fails;
	}
	fprintf(stderr, "usage: confineprobe bpf | narrow ROOT | tree | helper NAME\n"); return 2;
}
