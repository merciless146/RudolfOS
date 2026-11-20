/* 1.6.1 (docs/plan-1.6.1.md Part 12, the owner 2026-11-20: "where the start-up's time goes", to make it shorter): the time
   before the kernel's clock starts, which the start-up's steps (/run/rudolf/startup-times, all on the kernel's uptime) could
   not see: the computer's firmware (its self-test, Secure Boot's checks, finding the drive) and the boot loader (loading the
   start-up files). Two sources, both read once at rudolfd's start, as root:
   - ACPI's Firmware Performance Data Table (the kernel's /sys/firmware/acpi/fpdt/boot, CONFIG_ACPI_FPDT): when the firmware
     started loading the boot loader and when the boot loader handed over (ExitBootServices), in ns from the processor's
     reset: the split between the firmware and the boot loader.
   - x86's time-stamp counter, which counts from power-on (a constant, non-stop TSC), divided by its frequency (the kernel's
     log: "tsc: Refined TSC clocksource calibration" or "tsc: Detected"): the whole time before the kernel's clock, the
     kernel's unpacking included.
   After an update's handover (kexec) the system's kernel's clock starts again at 0: the boot files' kernel and start-up
   image's time comes in as rudolf.t0= (initramfs/init), and is taken off here so the firmware's part is the firmware's.
   Written as one line "fw SECONDS before the kernel: ..." into the start-up's steps (System health › Last starts shows it
   first) and kept for /api/health's beforeKernel (the health log's "ready" line). Nothing is written when neither source
   gives a believable number (0 to 300 s), e.g. in a virtual machine without FPDT, or on ARM64 without FPDT.
   RUDOLF_BOOTTIME_ROOT (a folder standing for /), RUDOLF_BOOTTIME_KLOG (a file standing for the kernel's log) and
   RUDOLF_BOOTTIME_TSC (the counter's value) are for the host test (iso/qt/host/boottime61_t.sh) only. */
#include <sys/klog.h>

static double bt_before_kernel = -1;   /* firmware + boot loader + the first kernel's unpacking, s; -1 not known */

static int bt_read_ull(const char *root, const char *path, unsigned long long *v)
{
	char p[512]; snprintf(p, sizeof p, "%s%s", root, path); FILE *f = fopen(p, "r"); if (!f) return 0;
	int ok = fscanf(f, "%llu", v) == 1; fclose(f); return ok;
}
static int bt_read_dbl(const char *root, const char *path, double *v)
{
	char p[512]; snprintf(p, sizeof p, "%s%s", root, path); FILE *f = fopen(p, "r"); if (!f) return 0;
	int ok = fscanf(f, "%lf", v) == 1; fclose(f); return ok;
}
/* the TSC's frequency in MHz from the kernel's log (the refined calibration preferred); 0 when not found */
static double bt_tsc_mhz(void)
{
	char *log = NULL; int n = 0; const char *kf = getenv("RUDOLF_BOOTTIME_KLOG");
	if (kf && *kf) { size_t sz = 0; log = slurp(kf, &sz); n = log ? (int)sz : 0; }
	else { int len = klogctl(10, NULL, 0); if (len > 0 && len < (64 << 20)) { log = malloc(len + 1); if (log) { n = klogctl(3, log, len); if (n < 0) n = 0; log[n] = 0; } } }
	if (!log) return 0;
	double det = 0, ref = 0; char *p;
	if ((p = strstr(log, "tsc: Refined TSC clocksource calibration: "))) ref = atof(p + 42);
	if ((p = strstr(log, "tsc: Detected "))) det = atof(p + 14);
	free(log); return ref > 0 ? ref : det;
}
#if defined(__x86_64__) || defined(__i386__)
static unsigned long long bt_rdtsc(void) { unsigned int lo, hi; __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi)); return ((unsigned long long)hi << 32) | lo; }
#endif

/* the line, or "" when nothing is known; t0: rudolf.t0 (0 without a handover) */
static void boottime_line(char *out, size_t sz, double t0)
{
	const char *root = getenv("RUDOLF_BOOTTIME_ROOT"); if (!root) root = "";
	out[0] = 0; double up = -1; if (!bt_read_dbl(root, "/proc/uptime", &up)) return;
	/* the firmware's own record */
	unsigned long long ld = 0, ex = 0; double fw = -1, bl = -1;
	if (bt_read_ull(root, "/sys/firmware/acpi/fpdt/boot/load_image_start_ns", &ld) && bt_read_ull(root, "/sys/firmware/acpi/fpdt/boot/exitbootservice_exit_ns", &ex)
	    && ld > 0 && ex > ld && ex < 300000000000ULL) { fw = ld / 1e9; bl = (ex - ld) / 1e9; }
	/* the processor's counter */
	double tsc = -1; int have_tsc = 0;
	const char *tv = getenv("RUDOLF_BOOTTIME_TSC"); unsigned long long ticks = 0;
	if (tv && *tv) { ticks = strtoull(tv, NULL, 10); have_tsc = 1; }
#if defined(__x86_64__) || defined(__i386__)
	else {
		/* only a counter that runs at one rate and never stops counts the time since power-on */
		char p[512]; snprintf(p, sizeof p, "%s/proc/cpuinfo", root); size_t cn = 0; char *ci = slurp(p, &cn);
		if (ci) { char *fl = strstr(ci, "\nflags"); if (fl) { char *e = strchr(fl + 1, '\n'); if (e) *e = 0; if (strstr(fl, " constant_tsc") && strstr(fl, " nonstop_tsc")) { ticks = bt_rdtsc(); have_tsc = 1; } } free(ci); }
	}
#endif
	if (have_tsc) { double mhz = bt_tsc_mhz(); if (mhz > 1) { tsc = ticks / (mhz * 1e6) - up - (t0 > 0 ? t0 : 0); if (tsc < 0 || tsc > 300) tsc = -1; } }
	double total = tsc >= 0 ? tsc : (fw >= 0 ? fw + bl : -1);
	if (total < 0) return;
	if (tsc >= 0 && fw >= 0 && fw + bl > tsc + 0.5) fw = bl = -1;   /* (the two disagree: the counter's whole is kept, no split) */
	bt_before_kernel = total;
	if (fw >= 0 && tsc >= 0) snprintf(out, sz, "fw %.2f before the kernel: the computer's firmware %.1f s, the boot loader %.1f s, the kernel's unpacking %.1f s", total, fw, bl, total - fw - bl > 0 ? total - fw - bl : 0);
	else if (fw >= 0) snprintf(out, sz, "fw %.2f before the kernel: the computer's firmware %.1f s, the boot loader %.1f s (the kernel's unpacking not counted)", total, fw, bl);
	else snprintf(out, sz, "fw %.2f before the kernel: the computer's firmware, the boot loader and the kernel's unpacking (no split: the firmware gives no record)", total);
}
/* at rudolfd's start: once per start-up (a restarted rudolfd finds its line there) */
static void boottime_note(void)
{
	char *have = slurp(RUN "/startup-times", NULL);
	if (have && (!strncmp(have, "fw ", 3) || strstr(have, "\nfw "))) {
		char *l = !strncmp(have, "fw ", 3) ? have : strstr(have, "\nfw ") + 1; bt_before_kernel = atof(l + 3); free(have); return;
	}
	double t0 = 0; char *t = have ? (!strncmp(have, "t0 ", 3) ? have : strstr(have, "\nt0 ")) : NULL; if (t) t0 = atof(t + (*t == '\n' ? 4 : 3));
	free(have);
	char line[300]; boottime_line(line, sizeof line, t0); if (!line[0]) return;
	FILE *f = fopen(RUN "/startup-times", "a"); if (f) { fprintf(f, "%s\n", line); fclose(f); }
}
