/* rudolf-kexec KERNEL "COMMAND LINE" - start another kernel (with its built-in initramfs) without going
 * through the firmware. The disc's start-up script uses it to hand over to an updated system slot.
 * Needs CONFIG_KEXEC_FILE. Built without a C library (x86-64 system calls only), so it adds about 2 KB
 * to the initramfs. GPL-2.0-or-later.   gcc -Os -static -nostdlib -fno-builtin -fno-stack-protector -o rudolf-kexec rudolf-kexec.c */
#define SYS_write 1
#define SYS_open 2
#define SYS_sync 162
#define SYS_reboot 169
#define SYS_exit 60
#define SYS_kexec_file_load 320
static long sc(long n, long a, long b, long c, long d, long e)
{
	long r; register long r10 __asm__("r10") = d; register long r8 __asm__("r8") = e;
	__asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8) : "rcx", "r11", "memory");
	return r;
}
static unsigned long len(const char *s) { unsigned long n = 0; while (s[n]) n++; return n; }
static void say(const char *s) { sc(SYS_write, 2, (long)s, len(s), 0, 0); }
__attribute__((used)) void cmain(long *sp)
{
	long argc = sp[0]; char **argv = (char **)(sp + 1);
	if (argc < 3) { say("usage: rudolf-kexec KERNEL CMDLINE\n"); sc(SYS_exit, 2, 0, 0, 0, 0); }
	long k = sc(SYS_open, (long)argv[1], 0 /* O_RDONLY */, 0, 0, 0);
	if (k < 0) { say("rudolf-kexec: cannot open the kernel\n"); sc(SYS_exit, 1, 0, 0, 0, 0); }
	long r = sc(SYS_kexec_file_load, k, -1, len(argv[2]) + 1, (long)argv[2], 4 /* KEXEC_FILE_NO_INITRAMFS */);
	if (r) { say("rudolf-kexec: the kernel refused the new kernel\n"); sc(SYS_exit, 1, 0, 0, 0, 0); }
	sc(SYS_sync, 0, 0, 0, 0, 0);
	sc(SYS_reboot, 0xfee1dead, 672274793, 0x45584543 /* LINUX_REBOOT_CMD_KEXEC */, 0, 0);
	say("rudolf-kexec: reboot into the new kernel failed\n");
	sc(SYS_exit, 1, 0, 0, 0, 0);
}
__asm__(".global _start\n_start:\n mov %rsp,%rdi\n and $-16,%rsp\n call cmain\n hlt\n");
