#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 12): the time before the kernel's clock (the computer's firmware, the boot loader, the
# kernel's unpacking) from ACPI's FPDT and the processor's time-stamp counter, as rudolfd writes it into the start-up's
# steps (rudolfd-boottime.inc), and how Last starts shows it (sec50.js startSteps). Fake files only.   sh boottime61_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; P=0; F=0
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
mkdir -p $W/run
cat > $W/h.c <<'C'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char *slurp(const char *path, size_t *len) { FILE *f = fopen(path, "rb"); if (!f) return NULL; char *b = malloc(1 << 20); size_t n = fread(b, 1, (1 << 20) - 1, f); fclose(f); b[n] = 0; if (len) *len = n; return b; }
#include "rudolfd-boottime.inc"
int main(int c, char **v) { if (c > 1 && !strcmp(v[1], "note")) { boottime_note(); printf("%.2f\n", bt_before_kernel); return 0; }
	char l[300]; boottime_line(l, sizeof l, c > 1 ? atof(v[1]) : 0); puts(l); return 0; }
C
gcc -O2 -w -I$SYS -DRUN="\"$W/run\"" -o $W/bt $W/h.c || { echo "FAIL build"; exit 1; }
R=$W/root; export RUDOLF_BOOTTIME_ROOT=$R RUDOLF_BOOTTIME_KLOG=$W/klog
fp() { mkdir -p $R/sys/firmware/acpi/fpdt/boot; echo $1 > $R/sys/firmware/acpi/fpdt/boot/load_image_start_ns; echo $2 > $R/sys/firmware/acpi/fpdt/boot/exitbootservice_exit_ns; }
mkdir -p $R/proc; echo '20.00 30.00' > $R/proc/uptime; printf 'processor\t: 0\nflags\t\t: fpu tsc\n' > $R/proc/cpuinfo
printf '[0.000] tsc: Detected 2395.000 MHz processor\n[1.2] tsc: Refined TSC clocksource calibration: 2395.500 MHz\n' > $W/klog
ticks() { python3 -c "print(int(($1)*2395.5e6))"; }

fp 3200000000 4900000000; export RUDOLF_BOOTTIME_TSC=$(ticks 26.0)
O=$($W/bt); ok "1. FPDT and the counter: 6.0 s before the kernel, firmware 3.2, boot loader 1.7, unpacking 1.1" $(echo "$O" | grep -qx "fw 6.00 before the kernel: the computer's firmware 3.2 s, the boot loader 1.7 s, the kernel's unpacking 1.1 s"; echo $?) "$O"
export RUDOLF_BOOTTIME_TSC=$(ticks 34.0)
O=$($W/bt 8); ok "2. after a handover (rudolf.t0=8) the first stage is taken off: still 6.0" $(echo "$O" | grep -q '^fw 6.00 '; echo $?) "$O"
unset RUDOLF_BOOTTIME_TSC
O=$($W/bt); ok "3. FPDT only (no non-stop counter): 4.9 s, split, unpacking not counted" $(echo "$O" | grep -qx "fw 4.90 before the kernel: the computer's firmware 3.2 s, the boot loader 1.7 s (the kernel's unpacking not counted)"; echo $?) "$O"
rm -rf $R/sys; export RUDOLF_BOOTTIME_TSC=$(ticks 25.0)
O=$($W/bt); ok "4. the counter only: 5.0 s, no split" $(echo "$O" | grep -q '^fw 5.00 before the kernel: .*(no split'; echo $?) "$O"
printf '[0.000] tsc: Detected 2395.500 MHz processor\n' > $W/klog
O=$($W/bt); ok "5. the counter's rate from 'Detected' when not refined" $(echo "$O" | grep -q '^fw 5.00 '; echo $?) "$O"
export RUDOLF_BOOTTIME_TSC=$(ticks 10.0)
O=$($W/bt); ok "6. a counter behind the kernel's clock (reset since power-on) gives nothing" $([ -z "$O" ]; echo $?) "$O"
export RUDOLF_BOOTTIME_TSC=$(ticks 500.0)
O=$($W/bt); ok "7. more than 300 s (a warm restart's counter) gives nothing" $([ -z "$O" ]; echo $?) "$O"
fp 3200000000 4900000000
O=$($W/bt); ok "8. ... and with FPDT there, FPDT's 4.9 s is used" $(echo "$O" | grep -q '^fw 4.90 '; echo $?) "$O"
export RUDOLF_BOOTTIME_TSC=$(ticks 23.0)
O=$($W/bt); ok "9. FPDT longer than the counter's whole (they disagree): the counter kept, no split" $(echo "$O" | grep -q '^fw 3.00 .*(no split'; echo $?) "$O"
unset RUDOLF_BOOTTIME_TSC; rm -rf $R/sys
O=$($W/bt); ok "10. neither (a virtual machine without FPDT, no non-stop counter): nothing" $([ -z "$O" ]; echo $?) "$O"
printf 'processor\t: 0\nflags\t\t: fpu tsc constant_tsc nonstop_tsc\n' > $R/proc/cpuinfo; printf '[0.000] tsc: Detected 1.0 MHz processor\n' > $W/klog
O=$($W/bt); ok "11. a real counter read (rdtsc) on this host runs without a crash, and a nonsense rate gives nothing" $([ -z "$O" ]; echo $?) "$O"
# rudolfd's note, once per start-up
printf 'flags\t\t: fpu\n' > $R/proc/cpuinfo; fp 3200000000 4900000000; printf 't0 8.0 before the handover\n12.0 kernel and start-up image\n' > $W/run/startup-times
O=$($W/bt note); ok "12. rudolfd's note: the line added to the steps, beforeKernel 4.9" $([ "$O" = 4.90 ] && grep -q '^fw 4.90 ' $W/run/startup-times && [ "$(wc -l < $W/run/startup-times)" = 3 ]; echo $?) "$O"
fp 1000000000 2000000000; O=$($W/bt note); ok "13. a restarted rudolfd keeps the line already there (not added twice)" $([ "$O" = 4.90 ] && [ "$(grep -c '^fw ' $W/run/startup-times)" = 1 ]; echo $?) "$O"
# Last starts (sec50.js startSteps): the time before the kernel first, then the steps, then the total from power-on
node -e "
const s=require('fs').readFileSync('$HERE/../../../desktop/sec50.js','utf8');const m=/function startSteps[\s\S]*?\n(?=async function startsShow)/.exec(s);eval(m[0]);
const o=startSteps('t0 8.0 before the handover\n12.0 kernel and start-up image\n30.5 the desktop answered\nfw 4.90 before the kernel: the computer\'s firmware 3.2 s');
const L=o.split('\n');if(!/^ +4\.9 s  before the kernel/.test(L[0])||!/^ +8\.0 s  before the handover/.test(L[1])||!/30\.5 s\)$/.test(L[3])||!/^ +43\.4 s  from power-on to the desktop's answer$/.test(L[L.length-1])){console.log(o);process.exit(1)}" 2>&1 > $W/js.out; R14=$?
ok "14. Last starts: before the kernel first, the handover, the steps, then 43.4 s from power-on" $R14 "$(cat $W/js.out)"
echo "$P passed, $F failed"; [ $F = 0 ]
