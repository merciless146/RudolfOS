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
	/* the accounts' Linux users (1.6.1 Part 11): /etc's users written, the accounts' folders given to their users
	   (lchown), a removed account's programs stopped, the keyring asked as the account's user (su) */
	{ "accounts", CB(CAP_CHOWN) | CB(CAP_DAC_OVERRIDE) | CB(CAP_DAC_READ_SEARCH) | CB(CAP_FOWNER) | CB(CAP_KILL) | CB(CAP_SETUID)
	  | CB(CAP_SETGID) },
	/* power and the clock (1.6.1 Part 11): restart and shut down (reboot(2), umount -a -r), the clock (settimeofday,
	   ntpd's adjtimex, hwclock), the wake alarm, /sys's power files and the backlight, Chromium stopped at the end */
	{ "power", CB(CAP_SYS_BOOT) | CB(CAP_SYS_TIME) | CB(CAP_SYS_ADMIN) | CB(CAP_DAC_OVERRIDE) | CB(CAP_KILL) | CB(CAP_WAKE_ALARM) },
	/* the network (1.6.1 Part 11): addresses, routes, the firewall's rules and the hotspot's NAT (ip, udhcpc, nft:
	   CAP_NET_ADMIN, CAP_NET_RAW for DHCP's raw socket, CAP_NET_BIND_SERVICE for dnsmasq's ports 53 and 67), Wi-Fi's
	   control sockets and saved networks (wpa_supplicant, root's 0600: CAP_DAC_OVERRIDE), the radios' rfkill and the
	   drivers' power saving under /sys, the hotspot's and wpa_supplicant's own processes stopped (CAP_KILL), its
	   dnsmasq run as its own user (CAP_SETUID, CAP_SETGID); no CAP_SYS_ADMIN, so no mount and no_new_privs */
	{ "network", CB(CAP_NET_ADMIN) | CB(CAP_NET_RAW) | CB(CAP_NET_BIND_SERVICE) | CB(CAP_DAC_OVERRIDE) | CB(CAP_DAC_READ_SEARCH)
	  | CB(CAP_FOWNER) | CB(CAP_CHOWN) | CB(CAP_KILL) | CB(CAP_SETUID) | CB(CAP_SETGID) },
	{ NULL, 0 }
};
/* the system calls of CF_HELPER_DENY a helper may still make, by name: the power helper restarts and sets the clock */
static const struct { const char *name; int nr; } HELPER_ALLOW[] = {
	{ "power", SYS_reboot }, { "power", SYS_settimeofday }, { "power", SYS_clock_settime }, { "power", SYS_clock_adjtime }, { "power", SYS_adjtimex },
	{ NULL, -1 }
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
