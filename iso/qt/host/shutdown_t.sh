#!/bin/sh
# 1.4.5 part 5: /usr/lib/rudolf/shutdown in test mode (SHUTDOWN_TEST): the order of its steps, the 5-second limit
# for the desktop's Chromium, the log. sh shutdown_t.sh   (SH="busybox sh" for BusyBox's shell)
SH=${SH:-sh}; HERE=$(cd "$(dirname "$0")" && pwd); S=$HERE/../../sys/root/usr/lib/rudolf/shutdown
T=$(mktemp -d); trap 'rm -rf $T' EXIT; export SHUTDOWN_TEST=$T; mkdir -p $T/persist
mkdir -p $T/bin; printf '#!/bin/sh\nsleep 0.1\n' > $T/bin/usleep; chmod +x $T/bin/usleep; export PATH=$T/bin:$PATH   # BusyBox's usleep, as on Rudolf OS
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
# Chromium closes after 1 s (10 checks)
echo 10 > $T/chromium-running; t0=$(date +%s); $SH $S poweroff 'files=120,session=130,storage=410'; t1=$(date +%s)
a=$(cat $T/actions)
ok "every app is asked to close at once, before any waiting" $(echo "$a" | head -4 | grep -c TERM | grep -qx 4 && echo 1 || echo 0) "$a"
ok "...the sign-in browser window, the user's processes and the development link among them" $(echo "$a" | grep -q 'RudolfWeb' && echo "$a" | grep -q 'TERM -u rudolf' && echo "$a" | grep -q 'TERM -x rudolf-devlink' && echo 1 || echo 0)
ok "then what is left is stopped, the disk synced, the data partition made read-only, everything unmounted, the power off" $(echo "$a" | grep -n . | grep -E 'KILL -u rudolf|^.*sync$|remount,ro|umount -a -r|poweroff -f' | cut -d: -f1 | tr '\n' ' ' | awk '{ok=NF>=5; for(i=2;i<=NF;i++) if($i<=$(i-1)) ok=0; print ok}') "$a"
ok "the session is told not to start the desktop again" $([ "$(cat $T/run/power)" = poweroff ] && echo 1 || echo 0)
L=$(cat $T/persist/var/log/shutdown.log)
ok "the log: the desktop's and the system's steps" $(echo "$L" | grep -qE 'poweroff desktop:files=120,session=130,storage=410 system:apps=[0-9]+,stopped=[0-9]+,synced=[0-9]+' && echo 1 || echo 0) "$L"
ok "...Chromium's wait was about 1 s, not 5" $(echo "$L" | sed -n 's/.*apps=\([0-9]*\).*/\1/p' | awk '{print ($1>=800&&$1<3000)?1:0}') "$L"
# Chromium never closes: at most 5 s
rm -f $T/actions; echo 1000 > $T/chromium-running; t0=$(date +%s); $SH $S reboot; t1=$(date +%s)
ok "a desktop that does not close is stopped after 5 s at most" $([ $((t1 - t0)) -le 7 ] && grep -q 'KILL -f /opt/chromium/chrome' $T/actions && echo 1 || echo 0) "$((t1 - t0)) s"
ok "...and Restart restarts" $(tail -1 $T/actions | grep -qx 'reboot -f' && echo 1 || echo 0) "$(tail -1 $T/actions)"
ok "a strange desktop-times argument is cleaned" $($SH $S poweroff "a;rm -rf /" >/dev/null 2>&1; tail -1 $T/persist/var/log/shutdown.log | grep -q 'desktop:armrf ' && echo 1 || echo 0) "$(tail -1 $T/persist/var/log/shutdown.log)"
ok "anything else than poweroff or reboot is refused" $($SH $S halt >/dev/null 2>&1; [ $? = 2 ] && echo 1 || echo 0)
echo "$((N-F))/$N passed"; [ $F = 0 ]
