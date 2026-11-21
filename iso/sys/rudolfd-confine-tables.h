/* (included by rudolfd-confine.inc, and by the host test's probe: iso/qt/host/fixtures/confineprobe.c) 1.6.1
 * (docs/plan-1.6.1.md Part 20): what rudolfd's confinement allows. RUN and PERSIST are rudolfd's. */
#define CFB(c) (1ULL << (c))
/* ---- layer 1: all of rudolfd ---- */
static const int CONFINE_TREE_DENY[] = {
#ifdef SYS_kexec_load
	SYS_kexec_load,
#endif
#ifdef SYS_kexec_file_load
	SYS_kexec_file_load,
#endif
#ifdef SYS_bpf
	SYS_bpf,
#endif
#ifdef SYS_perf_event_open
	SYS_perf_event_open,
#endif
#ifdef SYS_open_by_handle_at
	SYS_open_by_handle_at,
#endif
#ifdef SYS_iopl
	SYS_iopl,
#endif
#ifdef SYS_ioperm
	SYS_ioperm,
#endif
#ifdef SYS_acct
	SYS_acct,
#endif
#ifdef SYS_quotactl
	SYS_quotactl,
#endif
#ifdef SYS_quotactl_fd
	SYS_quotactl_fd,
#endif
#ifdef SYS_process_vm_writev
	SYS_process_vm_writev,
#endif
#ifdef SYS_uselib
	SYS_uselib,
#endif
#ifdef SYS_nfsservctl
	SYS_nfsservctl,
#endif
#ifdef SYS_lookup_dcookie
	SYS_lookup_dcookie,
#endif
	-1
};
/* the same in the 32-bit table (Wine's and Steam's 32-bit programs started by rudolfd): kexec_load, bpf,
   perf_event_open, open_by_handle_at, iopl, ioperm, acct, quotactl, quotactl_fd, process_vm_writev, uselib,
   nfsservctl, lookup_dcookie, vm86old, vm86 */
static const int CONFINE_TREE_DENY32[] = { 283, 357, 336, 342, 110, 101, 51, 131, 443, 348, 86, 169, 253, 113, 166, -1 };
static const uint64_t CONFINE_TREE_DROP = CFB(CAP_SYS_PACCT) | CFB(CAP_MAC_ADMIN) | CFB(CAP_MAC_OVERRIDE) | CFB(CAP_AUDIT_CONTROL)
	| CFB(CAP_AUDIT_READ) | CFB(CAP_BPF) | CFB(CAP_PERFMON) | CFB(CAP_CHECKPOINT_RESTORE);

/* ---- layer 2: a request that starts nothing ---- */
static const uint64_t CONFINE_NARROW_KEEP = CFB(CAP_CHOWN) | CFB(CAP_DAC_OVERRIDE) | CFB(CAP_DAC_READ_SEARCH) | CFB(CAP_FOWNER)
	| CFB(CAP_FSETID) | CFB(CAP_SETUID) | CFB(CAP_SETGID) | CFB(CAP_SYS_PTRACE) | CFB(CAP_SYS_RESOURCE);
/* where a narrow request may write, make and remove (its own folders; the files it serves and changes) */
static const struct cf_rule CONFINE_WRITE[] = {
	{ "/", CF_R },                          /* (read anywhere, as root reads today) */
	{ RUN, CF_W },                          /* rudolfd's run folder: events, heartbeat, the page's log, the portal's files */
	{ "/run/user/1000", CF_W },             /* the desktop user's: the tabs' memory request (processes) */
	{ PERSIST "/etc", CF_W },               /* rudolfd's settings (memory, the engine) */
	{ PERSIST "/var/log", CF_W },           /* the health log */
	{ PERSIST "/accounts", CF_W },          /* the accounts' folders (home/..., through the files API) */
	{ "/home", CF_W },                      /* the shared folders (fs/...), the desktop user's files */
	{ "/media", CF_W },                     /* the drives (fs/...) */
	{ "/proc/sys/vm/drop_caches", CF_WF },  /* memory/dropcache */
	{ "/dev/null", CF_WF },
	{ "/tmp", CF_W },
};

