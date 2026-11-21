/*
 * rudolf-confine.h - the confinement rudolfd and rudolf-rootd's helpers put themselves in (Rudolf OS 1.6.1,
 * docs/plan-1.6.1.md Part 20). Header only (static functions), so each program that uses it stays one file to build,
 * and nothing new is linked: no libseccomp, no libcap.
 *
 * Three layers, each the kernel's own, none needing a policy loaded by the start-up:
 *   - seccomp: a BPF filter on the system calls (the kernel has SECCOMP and SECCOMP_FILTER for Flatpak already).
 *     Either an allow list (everything else refused with EPERM, or only logged in "log" mode) or a deny list (the
 *     rest allowed). Every refusal and every logged call is written to the kernel's audit log (SECCOMP_FILTER_FLAG_LOG:
 *     "audit: type=1326 ... comm=... syscall=N"), which rudolfd reads into the health log.
 *   - Landlock: the files and folders the process may read, write, make or remove (CONFIG_SECURITY_LANDLOCK, added in
 *     1.6.1 for this: iso/kconfig-required.txt). Without it in the kernel the layer is skipped and said so; the others
 *     still apply. A Landlock domain also stops mount(2) and umount(2), so it is used only where nothing mounts.
 *   - capabilities: the bounding set (what this process and everything it starts can ever have) and the effective,
 *     permitted and inheritable sets cut to a list.
 * All three are inherited by every program the process starts and can never be undone by it: that is why rudolfd
 * puts the narrow ones only on what starts nothing (rudolfd-confine.inc).
 *
 * Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#ifndef RUDOLF_CONFINE_H
#define RUDOLF_CONFINE_H
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <linux/audit.h>
#include <linux/capability.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#ifndef O_PATH
#define O_PATH 010000000
#endif
#define CF_FN static __attribute__((unused))

#define CF_OFF 0
#define CF_LOG 1   /* everything that would be refused is allowed and logged (a beta's first run, finding what is missing) */
#define CF_ON  2   /* refused (EPERM) and logged */

/* "on", "1", "log", "off", "0" (and nothing: off); -1 for anything else */
CF_FN int cf_mode_parse(const char *s)
{
	if (!s || !*s) return CF_OFF;
	char w[8]; size_t n = strcspn(s, " \t\r\n"); if (n >= sizeof w) return -1;
	memcpy(w, s, n); w[n] = 0;
	if (!strcmp(w, "on") || !strcmp(w, "1")) return CF_ON;
	if (!strcmp(w, "log")) return CF_LOG;
	if (!strcmp(w, "off") || !strcmp(w, "0")) return CF_OFF;
	return -1;
}
/* the mode, the last one given winning: def (the build's), the file (one word), the start-up option rudolf.confine=,
   RUDOLFD_CONFINE in the environment (the host tests) */
CF_FN int cf_mode_read(const char *def, const char *file)
{
	int m = cf_mode_parse(def), x; if (m < 0) m = CF_OFF;
	char b[4096]; FILE *f;
	if (file && (f = fopen(file, "re"))) { size_t n = fread(b, 1, 63, f); b[n] = 0; fclose(f); if ((x = cf_mode_parse(b)) >= 0) m = x; }
	f = fopen(access("/etc/rudolf/cmdline", F_OK) ? "/proc/cmdline" : "/etc/rudolf/cmdline", "re");
	if (f) {
		size_t n = fread(b, 1, sizeof b - 1, f); b[n] = 0; fclose(f);
		const char *o = strstr(b, "rudolf.confine=");
		if (o && (o == b || o[-1] == ' ' || o[-1] == '\t') && (x = cf_mode_parse(o + 15)) >= 0) m = x;
	}
	const char *e = getenv("RUDOLFD_CONFINE");
	if (e && *e && (x = cf_mode_parse(e)) >= 0) m = x;
	return m;
}
CF_FN const char *cf_mode_name(int m) { return m == CF_ON ? "on" : m == CF_LOG ? "log" : "off"; }

/* ---------- seccomp ---------- */
#if defined(__x86_64__)
#define CF_ARCH AUDIT_ARCH_X86_64
#define CF_X32_BIT 0x40000000u
#elif defined(__aarch64__)
#define CF_ARCH AUDIT_ARCH_AARCH64
#endif
#ifndef SECCOMP_FILTER_FLAG_LOG
#define SECCOMP_FILTER_FLAG_LOG (1UL << 1)
#endif
#ifndef SECCOMP_RET_LOG
#define SECCOMP_RET_LOG 0x7ffc0000U
#endif
#define CF_MAXPROG 1024

/* the filter's program: nr[] the listed calls of this machine's own system-call table; i386[] (x86-64 only) the
   listed calls of the 32-bit table (Wine's and Steam's 32-bit programs started by rudolfd use it), or NULL (then every
   32-bit call is refused, or logged).
   allow=1: the listed calls are allowed, the rest refused (or logged); allow=0: the listed calls are refused (or
   logged), the rest allowed. x32 calls (an ABI Rudolf OS
   never uses) and another machine's are always refused. Returns the program's length, or -1 if it is too long. */
CF_FN int cf_bpf(struct sock_filter *p, int max, const int *nr, int nn, const int *i386, int ni, int allow, int mode)
{
	const uint32_t RF = mode == CF_ON ? (SECCOMP_RET_ERRNO | (EPERM & SECCOMP_RET_DATA)) : SECCOMP_RET_LOG;
	const uint32_t HIT = allow ? SECCOMP_RET_ALLOW : RF, MISS = allow ? RF : SECCOMP_RET_ALLOW;
	const uint32_t NO = SECCOMP_RET_ERRNO | (EPERM & SECCOMP_RET_DATA);
	int k = 0;
#define CF_EMIT(c, jt, jf, kk) do { if (k >= max) return -1; p[k++] = (struct sock_filter)BPF_JUMP(c, kk, jt, jf); } while (0)
#define CF_STMT(c, kk) do { if (k >= max) return -1; p[k++] = (struct sock_filter)BPF_STMT(c, kk); } while (0)
	CF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, arch));
#ifdef __x86_64__
	/* this machine's table first; anything else jumps (a long jump: the list is longer than a short one reaches) to the
	   32-bit table's block after it */
	CF_EMIT(BPF_JMP | BPF_JEQ | BPF_K, 1, 0, CF_ARCH);
	int ja = k; CF_STMT(BPF_JMP | BPF_JA, 0);
	CF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr));
	CF_EMIT(BPF_JMP | BPF_JGE | BPF_K, 0, 1, CF_X32_BIT);
	CF_STMT(BPF_RET | BPF_K, NO);
#else
	CF_EMIT(BPF_JMP | BPF_JEQ | BPF_K, 1, 0, CF_ARCH);
	CF_STMT(BPF_RET | BPF_K, NO);
	CF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr));
#endif
	for (int i = 0; i < nn; i++) { if (nr[i] < 0) continue; CF_EMIT(BPF_JMP | BPF_JEQ | BPF_K, 0, 1, (uint32_t)nr[i]); CF_STMT(BPF_RET | BPF_K, HIT); }
	CF_STMT(BPF_RET | BPF_K, MISS);
#ifdef __x86_64__
	p[ja].k = (uint32_t)(k - ja - 1);
	CF_EMIT(BPF_JMP | BPF_JEQ | BPF_K, 1, 0, AUDIT_ARCH_I386);
	CF_STMT(BPF_RET | BPF_K, NO);   /* (another machine's calls) */
	if (i386) {
		CF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr));
		for (int i = 0; i386 && i < ni; i++) { if (i386[i] < 0) continue; CF_EMIT(BPF_JMP | BPF_JEQ | BPF_K, 0, 1, (uint32_t)i386[i]); CF_STMT(BPF_RET | BPF_K, HIT); }
		CF_STMT(BPF_RET | BPF_K, MISS);
	} else CF_STMT(BPF_RET | BPF_K, RF);
#else
	(void)i386; (void)ni;
#endif
#undef CF_EMIT
#undef CF_STMT
	return k;
}
/* the filter put on this process (and so on everything it starts from now on). 0, or -1 with errno.
   Without no_new_privs the kernel asks for CAP_SYS_ADMIN, which rudolfd has as root; once it runs as the desktop's
   user (Part 11) the caller sets no_new_privs first. */
CF_FN int cf_seccomp(const int *nr, int nn, const int *i386, int ni, int allow, int mode)
{
	static struct sock_filter prog[CF_MAXPROG];
	int n = cf_bpf(prog, CF_MAXPROG, nr, nn, i386, ni, allow, mode);
	if (n < 0) { errno = E2BIG; return -1; }
	struct sock_fprog fp = { (unsigned short)n, prog };
	if (syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_LOG, &fp) == 0) return 0;
	if (errno == EINVAL && syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, 0, &fp) == 0) return 0;   /* (a kernel without the flag) */
	return -1;
}

/* ---------- capabilities ---------- */
#ifndef CAP_LAST_CAP
#define CAP_LAST_CAP 40
#endif
/* keep: a bit mask of capability numbers (1ULL << CAP_X). The bounding set loses every other one (what this process
   and the programs it starts can ever have, set-user-ID programs included), and this process's own sets are cut to
   keep. 0, or -1 (errno) if one could not be dropped. */
/* the bounding set cut to keep (what this process and every program it starts can ever have, set-user-ID programs
   included); this process's own sets are left as they are. 0, or -1 (errno) */
CF_FN int cf_caps_bound(uint64_t keep)
{
	for (int c = 0; c <= 63; c++) {
		if (keep & (1ULL << c)) continue;
		if (prctl(PR_CAPBSET_READ, c, 0, 0, 0) < 0) break;   /* (past the kernel's last) */
		if (prctl(PR_CAPBSET_DROP, c, 0, 0, 0)) return -1;
	}
	return 0;
}
CF_FN int cf_caps(uint64_t keep)
{
	/* (EPERM without CAP_SETPCAP: after the drop to uid 1000 the bounding set is not this process's to cut, and it
	   holds no capability to lose) */
	if (cf_caps_bound(keep) && errno != EPERM) return -1;
	struct __user_cap_header_struct h = { _LINUX_CAPABILITY_VERSION_3, 0 };
	struct __user_cap_data_struct d[2];
	if (syscall(SYS_capget, &h, d)) return -1;
	for (int i = 0; i < 2; i++) {
		uint32_t k = (uint32_t)(keep >> (32 * i));
		d[i].effective &= k; d[i].permitted &= k; d[i].inheritable &= k;
	}
	if (syscall(SYS_capset, &h, d)) return -1;
	prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0, 0, 0);
	return 0;
}

/* ---------- Landlock ---------- */
#ifndef __NR_landlock_create_ruleset
#define __NR_landlock_create_ruleset 444
#define __NR_landlock_add_rule 445
#define __NR_landlock_restrict_self 446
#endif
/* (the kernel's own ABI, written here so that building needs no kernel headers newer than Landlock's first) */
struct cf_ll_ruleset { uint64_t fs, net, scoped; };
struct cf_ll_path { uint64_t allowed; int32_t fd; } __attribute__((packed));
#define CF_LL_RULE_PATH 1
#define CF_FS_EXECUTE     (1ULL << 0)
#define CF_FS_WRITE_FILE  (1ULL << 1)
#define CF_FS_READ_FILE   (1ULL << 2)
#define CF_FS_READ_DIR    (1ULL << 3)
#define CF_FS_REMOVE_DIR  (1ULL << 4)
#define CF_FS_REMOVE_FILE (1ULL << 5)
#define CF_FS_MAKE_CHAR   (1ULL << 6)
#define CF_FS_MAKE_DIR    (1ULL << 7)
#define CF_FS_MAKE_REG    (1ULL << 8)
#define CF_FS_MAKE_SOCK   (1ULL << 9)
#define CF_FS_MAKE_FIFO   (1ULL << 10)
#define CF_FS_MAKE_BLOCK  (1ULL << 11)
#define CF_FS_MAKE_SYM    (1ULL << 12)
#define CF_FS_REFER       (1ULL << 13)   /* ABI 2 */
#define CF_FS_TRUNCATE    (1ULL << 14)   /* ABI 3 */
#define CF_FS_IOCTL_DEV   (1ULL << 15)   /* ABI 5 */
#define CF_NET_BIND       (1ULL << 0)    /* ABI 4 */
#define CF_NET_CONNECT    (1ULL << 1)
#define CF_SCOPE_ABSTRACT (1ULL << 0)    /* ABI 6 */
#define CF_SCOPE_SIGNAL   (1ULL << 1)
/* what a rule may grant, in short */
#define CF_R  (CF_FS_READ_FILE | CF_FS_READ_DIR)
#define CF_W  (CF_R | CF_FS_WRITE_FILE | CF_FS_TRUNCATE | CF_FS_MAKE_REG | CF_FS_MAKE_DIR | CF_FS_REMOVE_FILE | CF_FS_REMOVE_DIR | CF_FS_REFER | CF_FS_MAKE_SYM)
#define CF_WF (CF_R | CF_FS_WRITE_FILE | CF_FS_TRUNCATE)   /* write existing files only (devices, /sys) */
struct cf_rule { const char *path; uint64_t access; };

/* the kernel's Landlock ABI (0: none) */
CF_FN int cf_landlock_abi(void)
{
	long v = syscall(__NR_landlock_create_ruleset, NULL, 0, 1UL /* LANDLOCK_CREATE_RULESET_VERSION */);
	return v < 0 ? 0 : (int)v;
}
/* what this ABI handles */
CF_FN uint64_t cf_fs_handled(int abi)
{
	uint64_t a = (1ULL << 13) - 1;
	if (abi >= 2) a |= CF_FS_REFER;
	if (abi >= 3) a |= CF_FS_TRUNCATE;
	if (abi >= 5) a |= CF_FS_IOCTL_DEV;
	return a;
}
/* the rules (prefix: a folder put in front of every path, for the host tests; "" in use), a missing path skipped
   (said in skipped, if given, as a count). scope: CF_SCOPE_* wanted (dropped on kernels before ABI 6); net: 1 to refuse
   every TCP bind and connect (ABI 4). 0 restricted, 1 no Landlock in this kernel (nothing done), -1 failed (errno). */
CF_FN int cf_landlock(const struct cf_rule *r, int nr, const char *prefix, uint64_t scope, int net, int *skipped)
{
	int abi = cf_landlock_abi(); if (abi <= 0) return 1;
	uint64_t fs = cf_fs_handled(abi);
	struct cf_ll_ruleset ra = { fs, abi >= 4 && net ? (CF_NET_BIND | CF_NET_CONNECT) : 0, abi >= 6 ? scope : 0 };
	size_t rsz = abi >= 6 ? sizeof ra : abi >= 4 ? 2 * sizeof(uint64_t) : sizeof(uint64_t);
	int rs = (int)syscall(__NR_landlock_create_ruleset, &ra, rsz, 0);
	if (rs < 0) return -1;
	int sk = 0;
	for (int i = 0; i < nr; i++) {
		char p[1024]; snprintf(p, sizeof p, "%s%s", prefix ? prefix : "", r[i].path);
		int fd = open(p, O_PATH | O_CLOEXEC);
		if (fd < 0) { sk++; continue; }
		struct cf_ll_path pb = { r[i].access & fs, fd };
		/* a file (not a folder) may only be given what applies to a file */
		int isdir = 0; { int d = open(p, O_RDONLY | O_DIRECTORY | O_CLOEXEC); if (d >= 0) { isdir = 1; close(d); } }
		if (!isdir) pb.allowed &= CF_FS_EXECUTE | CF_FS_WRITE_FILE | CF_FS_READ_FILE | CF_FS_TRUNCATE | CF_FS_IOCTL_DEV;
		if (pb.allowed && syscall(__NR_landlock_add_rule, rs, CF_LL_RULE_PATH, &pb, 0)) { int e = errno; close(fd); close(rs); errno = e; return -1; }
		close(fd);
	}
	if (skipped) *skipped = sk;
	if (!prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0) && prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) { int e = errno; close(rs); errno = e; return -1; }
	int rc = (int)syscall(__NR_landlock_restrict_self, rs, 0);
	int e = errno; close(rs); errno = e;
	return rc ? -1 : 0;
}
#endif
