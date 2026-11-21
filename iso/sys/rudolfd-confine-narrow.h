/* (from the traces: iso/systools/confinetrace.py; the added ones by hand, each with why) 1.6.1 (docs/plan-1.6.1.md
 * Part 20): the system calls of the narrow confinement: those rudolfd itself made in the host tests (strace -f of every
 * host test that builds rudolfd or a fixture of it: the number of tests it was seen in), and those the C library or
 * rudolfd make on paths the tests do not reach. Never: exec, mount, umount, kill, ptrace, setns, unshare, chroot, reboot,
 * setsid, syslog, vfork (seen in the tests, made by the requests that start programs, which are not narrow). */
static const int CONFINE_NARROW[] = {
#ifdef SYS_accept4
	SYS_accept4,   /* traced (2 tests) */
#endif
#ifdef SYS_access
	SYS_access,   /* traced (22 tests) */
#endif
#ifdef SYS_alarm
	SYS_alarm,   /* traced (1 test) */
#endif
#ifdef SYS_arch_prctl
	SYS_arch_prctl,   /* traced (22 tests) */
#endif
#ifdef SYS_bind
	SYS_bind,   /* traced (2 tests) */
#endif
#ifdef SYS_brk
	SYS_brk,   /* traced (22 tests) */
#endif
#ifdef SYS_chdir
	SYS_chdir,   /* traced (1 test) */
#endif
#ifdef SYS_chmod
	SYS_chmod,   /* traced (6 tests) */
#endif
#ifdef SYS_chown
	SYS_chown,   /* traced (5 tests) */
#endif
#ifdef SYS_clock_getres
	SYS_clock_getres,   /* added: the C library */
#endif
#ifdef SYS_clock_gettime
	SYS_clock_gettime,   /* traced (1 test) */
#endif
#ifdef SYS_clock_nanosleep
	SYS_clock_nanosleep,   /* traced (5 tests) */
#endif
#ifdef SYS_clone
	SYS_clone,   /* traced (8 tests) */
#endif
#ifdef SYS_clone3
	SYS_clone3,   /* traced (11 tests) */
#endif
#ifdef SYS_close
	SYS_close,   /* traced (22 tests) */
#endif
#ifdef SYS_connect
	SYS_connect,   /* traced (5 tests) */
#endif
#ifdef SYS_dup
	SYS_dup,   /* traced (2 tests) */
#endif
#ifdef SYS_dup2
	SYS_dup2,   /* traced (10 tests) */
#endif
#ifdef SYS_dup3
	SYS_dup3,   /* added: the C library */
#endif
#ifdef SYS_eventfd2
	SYS_eventfd2,   /* traced (1 test) */
#endif
#ifdef SYS_exit
	SYS_exit,   /* added: a thread or the C library ending */
#endif
#ifdef SYS_exit_group
	SYS_exit_group,   /* traced (22 tests) */
#endif
#ifdef SYS_faccessat
	SYS_faccessat,   /* added: access() as newer C libraries make it */
#endif
#ifdef SYS_faccessat2
	SYS_faccessat2,   /* added: access() as newer C libraries make it */
#endif
#ifdef SYS_fchdir
	SYS_fchdir,   /* added: the files API */
#endif
#ifdef SYS_fchmod
	SYS_fchmod,   /* traced (2 tests) */
#endif
#ifdef SYS_fchmodat
	SYS_fchmodat,   /* added: chmod() in newer C libraries */
#endif
#ifdef SYS_fchown
	SYS_fchown,   /* traced (3 tests) */
#endif
#ifdef SYS_fchownat
	SYS_fchownat,   /* traced (3 tests) */
#endif
#ifdef SYS_fcntl
	SYS_fcntl,   /* traced (7 tests) */
#endif
#ifdef SYS_fdatasync
	SYS_fdatasync,   /* added: the files API */
#endif
#ifdef SYS_fgetxattr
	SYS_fgetxattr,   /* added: the document portal's path */
#endif
#ifdef SYS_flock
	SYS_flock,   /* traced (1 test) */
#endif
#ifdef SYS_fstat
	SYS_fstat,   /* traced (22 tests) */
#endif
#ifdef SYS_fstatfs
	SYS_fstatfs,   /* added: statvfs */
#endif
#ifdef SYS_fsync
	SYS_fsync,   /* traced (6 tests) */
#endif
#ifdef SYS_ftruncate
	SYS_ftruncate,   /* added: the files API */
#endif
#ifdef SYS_futex
	SYS_futex,   /* traced (2 tests) */
#endif
#ifdef SYS_getcwd
	SYS_getcwd,   /* added: the C library */
#endif
#ifdef SYS_getdents
	SYS_getdents,   /* added: readdir on older builds */
#endif
#ifdef SYS_getdents64
	SYS_getdents64,   /* traced (6 tests) */
#endif
#ifdef SYS_getegid
	SYS_getegid,   /* traced (1 test) */
#endif
#ifdef SYS_geteuid
	SYS_geteuid,   /* traced (3 tests) */
#endif
#ifdef SYS_getgid
	SYS_getgid,   /* added: the C library */
#endif
#ifdef SYS_getgroups
	SYS_getgroups,   /* added: the C library */
#endif
#ifdef SYS_getitimer
	SYS_getitimer,   /* added: timeouts */
#endif
#ifdef SYS_getpeername
	SYS_getpeername,   /* traced (2 tests) */
#endif
#ifdef SYS_getpgid
	SYS_getpgid,   /* added: the processes list */
#endif
#ifdef SYS_getpgrp
	SYS_getpgrp,   /* added: the C library */
#endif
#ifdef SYS_getpid
	SYS_getpid,   /* traced (5 tests) */
#endif
#ifdef SYS_getppid
	SYS_getppid,   /* added: the C library */
#endif
#ifdef SYS_getpriority
	SYS_getpriority,   /* added: the processes list */
#endif
#ifdef SYS_getrandom
	SYS_getrandom,   /* traced (21 tests) */
#endif
#ifdef SYS_getrusage
	SYS_getrusage,   /* added: the C library */
#endif
#ifdef SYS_getsid
	SYS_getsid,   /* added: the processes list */
#endif
#ifdef SYS_getsockname
	SYS_getsockname,   /* traced (2 tests) */
#endif
#ifdef SYS_getsockopt
	SYS_getsockopt,   /* added: the sockets (SO_PEERCRED) */
#endif
#ifdef SYS_gettid
	SYS_gettid,   /* added: the C library */
#endif
#ifdef SYS_gettimeofday
	SYS_gettimeofday,   /* added: the clock without the vDSO */
#endif
#ifdef SYS_getuid
	SYS_getuid,   /* traced (6 tests) */
#endif
#ifdef SYS_getxattr
	SYS_getxattr,   /* added: the document portal's path */
#endif
#ifdef SYS_ioctl
	SYS_ioctl,   /* traced (9 tests) */
#endif
#ifdef SYS_lchown
	SYS_lchown,   /* traced (1 test) */
#endif
#ifdef SYS_lgetxattr
	SYS_lgetxattr,   /* traced (1 test) */
#endif
#ifdef SYS_link
	SYS_link,   /* added: the files API (hard links checked) */
#endif
#ifdef SYS_linkat
	SYS_linkat,   /* added: the files API */
#endif
#ifdef SYS_listen
	SYS_listen,   /* traced (2 tests) */
#endif
#ifdef SYS_lseek
	SYS_lseek,   /* traced (17 tests) */
#endif
#ifdef SYS_lstat
	SYS_lstat,   /* added: lstat */
#endif
#ifdef SYS_madvise
	SYS_madvise,   /* traced (1 test) */
#endif
#ifdef SYS_membarrier
	SYS_membarrier,   /* added: the C library */
#endif
#ifdef SYS_memfd_create
	SYS_memfd_create,   /* traced (1 test) */
#endif
#ifdef SYS_mkdir
	SYS_mkdir,   /* traced (13 tests) */
#endif
#ifdef SYS_mkdirat
	SYS_mkdirat,   /* traced (3 tests) */
#endif
#ifdef SYS_mmap
	SYS_mmap,   /* traced (22 tests) */
#endif
#ifdef SYS_mprotect
	SYS_mprotect,   /* traced (22 tests) */
#endif
#ifdef SYS_mremap
	SYS_mremap,   /* added: realloc of a large reply (the files API, the page) */
#endif
#ifdef SYS_msync
	SYS_msync,   /* added: the C library */
#endif
#ifdef SYS_munmap
	SYS_munmap,   /* traced (22 tests) */
#endif
#ifdef SYS_nanosleep
	SYS_nanosleep,   /* added: sleeps (events/wait) */
#endif
#ifdef SYS_newfstatat
	SYS_newfstatat,   /* traced (14 tests) */
#endif
#ifdef SYS_open
	SYS_open,   /* added: the C library's open on older builds */
#endif
#ifdef SYS_openat
	SYS_openat,   /* traced (22 tests) */
#endif
#ifdef SYS_pipe2
	SYS_pipe2,   /* traced (9 tests) */
#endif
#ifdef SYS_poll
	SYS_poll,   /* traced (3 tests) */
#endif
#ifdef SYS_ppoll
	SYS_ppoll,   /* added: poll() in newer C libraries */
#endif
#ifdef SYS_prctl
	SYS_prctl,   /* traced (1 test) */
#endif
#ifdef SYS_pread64
	SYS_pread64,   /* traced (22 tests) */
#endif
#ifdef SYS_prlimit64
	SYS_prlimit64,   /* traced (22 tests) */
#endif
#ifdef SYS_pselect6
	SYS_pselect6,   /* added: the C library */
#endif
#ifdef SYS_pwrite64
	SYS_pwrite64,   /* added: the files API */
#endif
#ifdef SYS_read
	SYS_read,   /* traced (22 tests) */
#endif
#ifdef SYS_readlink
	SYS_readlink,   /* traced (4 tests) */
#endif
#ifdef SYS_readlinkat
	SYS_readlinkat,   /* traced (1 test) */
#endif
#ifdef SYS_readv
	SYS_readv,   /* added: the C library */
#endif
#ifdef SYS_recvfrom
	SYS_recvfrom,   /* traced (2 tests) */
#endif
#ifdef SYS_recvmsg
	SYS_recvmsg,   /* traced (2 tests) */
#endif
#ifdef SYS_rename
	SYS_rename,   /* traced (9 tests) */
#endif
#ifdef SYS_renameat
	SYS_renameat,   /* traced (1 test) */
#endif
#ifdef SYS_renameat2
	SYS_renameat2,   /* added: rename() in newer C libraries */
#endif
#ifdef SYS_restart_syscall
	SYS_restart_syscall,   /* traced (2 tests) */
#endif
#ifdef SYS_rmdir
	SYS_rmdir,   /* added: fs/delete */
#endif
#ifdef SYS_rseq
	SYS_rseq,   /* traced (22 tests) */
#endif
#ifdef SYS_rt_sigaction
	SYS_rt_sigaction,   /* traced (8 tests) */
#endif
#ifdef SYS_rt_sigpending
	SYS_rt_sigpending,   /* added: the C library */
#endif
#ifdef SYS_rt_sigprocmask
	SYS_rt_sigprocmask,   /* traced (13 tests) */
#endif
#ifdef SYS_rt_sigreturn
	SYS_rt_sigreturn,   /* traced (1 test) */
#endif
#ifdef SYS_rt_sigtimedwait
	SYS_rt_sigtimedwait,   /* added: the C library */
#endif
#ifdef SYS_sched_getaffinity
	SYS_sched_getaffinity,   /* added: sysconf (the CPUs) */
#endif
#ifdef SYS_sched_yield
	SYS_sched_yield,   /* added: the C library's locks */
#endif
#ifdef SYS_select
	SYS_select,   /* added: the C library */
#endif
#ifdef SYS_sendmsg
	SYS_sendmsg,   /* traced (3 tests) */
#endif
#ifdef SYS_sendto
	SYS_sendto,   /* traced (1 test) */
#endif
#ifdef SYS_set_robust_list
	SYS_set_robust_list,   /* traced (22 tests) */
#endif
#ifdef SYS_set_tid_address
	SYS_set_tid_address,   /* traced (22 tests) */
#endif
#ifdef SYS_setgid
	SYS_setgid,   /* traced (1 test) */
#endif
#ifdef SYS_setgroups
	SYS_setgroups,   /* traced (1 test) */
#endif
#ifdef SYS_setitimer
	SYS_setitimer,   /* added: timeouts */
#endif
#ifdef SYS_setresgid
	SYS_setresgid,   /* added: rudolfd-drop.inc's child */
#endif
#ifdef SYS_setresuid
	SYS_setresuid,   /* added: rudolfd-drop.inc's child */
#endif
#ifdef SYS_setsockopt
	SYS_setsockopt,   /* traced (3 tests) */
#endif
#ifdef SYS_setuid
	SYS_setuid,   /* traced (1 test) */
#endif
#ifdef SYS_shutdown
	SYS_shutdown,   /* traced (2 tests) */
#endif
#ifdef SYS_sigaltstack
	SYS_sigaltstack,   /* traced (1 test) */
#endif
#ifdef SYS_socket
	SYS_socket,   /* traced (7 tests) */
#endif
#ifdef SYS_socketpair
	SYS_socketpair,   /* traced (2 tests) */
#endif
#ifdef SYS_stat
	SYS_stat,   /* added: the C library's stat on older builds */
#endif
#ifdef SYS_statfs
	SYS_statfs,   /* traced (2 tests) */
#endif
#ifdef SYS_statx
	SYS_statx,   /* added: the C library's stat on newer builds */
#endif
#ifdef SYS_symlink
	SYS_symlink,   /* traced (3 tests) */
#endif
#ifdef SYS_symlinkat
	SYS_symlinkat,   /* added: the files API */
#endif
#ifdef SYS_sync
	SYS_sync,   /* traced (3 tests) */
#endif
#ifdef SYS_sysinfo
	SYS_sysinfo,   /* added: memory, uptime (the info and stats replies) */
#endif
#ifdef SYS_time
	SYS_time,   /* added: the clock without the vDSO */
#endif
#ifdef SYS_umask
	SYS_umask,   /* traced (1 test) */
#endif
#ifdef SYS_uname
	SYS_uname,   /* added: the C library */
#endif
#ifdef SYS_unlink
	SYS_unlink,   /* traced (17 tests) */
#endif
#ifdef SYS_unlinkat
	SYS_unlinkat,   /* traced (1 test) */
#endif
#ifdef SYS_utimensat
	SYS_utimensat,   /* traced (1 test) */
#endif
#ifdef SYS_wait4
	SYS_wait4,   /* traced (12 tests) */
#endif
#ifdef SYS_waitid
	SYS_waitid,   /* added: waitpid */
#endif
#ifdef SYS_write
	SYS_write,   /* traced (22 tests) */
#endif
#ifdef SYS_writev
	SYS_writev,   /* added: the C library's stdio */
#endif
	-1
};
