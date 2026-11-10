#!/bin/sh
# 1.5.7: desktop-priority on|off, with a stand-in cgroup, /proc and renice.   sh deskprio57_t.sh
cd "$(dirname "$0")"; S=../../sys/root/usr/lib/rudolf/desktop-priority
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT; P=0; F=0
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1"; fi; }
mkdir -p $T/cg $T/proc/101/task/101 $T/proc/101/task/102 $T/proc/202/task/202 $T/proc/303/task/303
printf '101\n' > $T/cg/cgroup.procs; echo 100 > $T/cg/cpu.weight; echo 'default 100' > $T/cg/io.weight
printf 'chrome\0--rudolf-kiosk\0' > $T/proc/202/cmdline; printf 'game\0' > $T/proc/303/cmdline
printf '#!/bin/sh\necho "$*" >> %s/renice.log\n' $T > $T/renice; chmod +x $T/renice
printf '#!/bin/sh\necho "$*" >> %s/ionice.log\n' $T > $T/ionice; chmod +x $T/ionice
run() { DP_CGROUP=$T/cg DP_PROC=$T/proc DP_RENICE=$T/renice DP_IONICE=$T/ionice DP_RUNF=$T/flag sh $S "$@"; }
o=$(run on); ok "1. on: every thread of the desktop's cgroup (2) to -10" $( [ "$o" = "desktop priority on: 2 threads" ] && grep -qx -- '-10 -p 101' $T/renice.log && grep -qx -- '-10 -p 102' $T/renice.log; echo $?)
ok "2. on: the cgroup's processor and disk weights to 1000" $( [ "$(cat $T/cg/cpu.weight)" = 1000 ] && [ "$(cat $T/cg/io.weight)" = "default 1000" ]; echo $?)
ok "2b. on: the disk too (ionice best-effort 0), and the flag rudolfd reads" $( grep -qx -- '-c 2 -n 0 -p 101' $T/ionice.log && [ -s $T/flag ]; echo $?)
ok "3. a process outside the desktop is not touched" $( ! grep -q '303\|202' $T/renice.log; echo $?)
: > $T/renice.log; o=$(run off); ok "4. off: back to 0 and weight 100" $( grep -qx -- '0 -p 101' $T/renice.log && [ "$(cat $T/cg/cpu.weight)" = 100 ] && [ "$(cat $T/cg/io.weight)" = "default 100" ] && [ ! -e $T/flag ]; echo $?)
: > $T/renice.log; : > $T/cg/cgroup.procs; o=$(run on); ok "5. no cgroup: the desktop found by --rudolf-kiosk" $( grep -qx -- '-10 -p 202' $T/renice.log && ! grep -q 303 $T/renice.log; echo $?)
run bad >/dev/null; ok "6. anything else: usage, exit 2" $([ $? = 2 ]; echo $?)
ok "7. sets the priority (renice N), not adds to it (BusyBox renice -n adds)" $(grep -q '$RENICE $N -p' $S && ! grep -q 'RENICE -n' $S; echo $?)
echo "$P/$((P+F)) passed"; [ $F = 0 ]
