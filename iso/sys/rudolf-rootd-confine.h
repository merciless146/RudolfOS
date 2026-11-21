/* (included by rudolf-rootd.c, and by the host test's probe: iso/qt/host/fixtures/confineprobe.c) 1.6.1
 * (docs/plan-1.6.1.md Part 20): each helper's capabilities and the system calls no helper makes. */
#define CB(c) (1ULL << (c))
static const struct { const char *name; uint64_t keep; } HELPER_CAPS[] = {
	/* the drives: mount and unmount, whole disks partitioned and written (install, sticks), files owned and given
	   their modes, the firmware's boot entries (efivars: their immutable flag), loop devices, the scripts' su */
	{ "drives", CB(CAP_CHOWN) | CB(CAP_DAC_OVERRIDE) | CB(CAP_DAC_READ_SEARCH) | CB(CAP_FOWNER) | CB(CAP_FSETID) | CB(CAP_KILL)
	  | CB(CAP_SETUID) | CB(CAP_SETGID) | CB(CAP_SYS_ADMIN) | CB(CAP_SYS_RESOURCE) | CB(CAP_LINUX_IMMUTABLE) | CB(CAP_MKNOD)
	  | CB(CAP_SYS_CHROOT) },
	/* the updates: a release's files on RUDOLFDATA and the ESP, its layer mounted to read its keys */
	{ "updates", CB(CAP_CHOWN) | CB(CAP_DAC_OVERRIDE) | CB(CAP_DAC_READ_SEARCH) | CB(CAP_FOWNER) | CB(CAP_FSETID) | CB(CAP_SYS_ADMIN)
	  | CB(CAP_SYS_RESOURCE) },
	{ NULL, 0 }
};
static const int CF_HELPER_DENY[] = {
#define D(n) SYS_##n,
#ifdef SYS_kexec_load
	D(kexec_load)
#endif
#ifdef SYS_kexec_file_load
	D(kexec_file_load)
#endif
	D(bpf) D(perf_event_open) D(open_by_handle_at)
#ifdef SYS_iopl
	D(iopl) D(ioperm)
#endif
	D(acct) D(quotactl)
#ifdef SYS_quotactl_fd
	D(quotactl_fd)
#endif
	D(process_vm_writev) D(process_vm_readv) D(ptrace) D(init_module) D(finit_module) D(delete_module) D(reboot)
	D(settimeofday) D(clock_settime) D(clock_adjtime) D(adjtimex) D(swapon) D(swapoff)
#ifdef SYS_uselib
	D(uselib)
#endif
#undef D
	-1
};
