#!/bin/sh
# 1.5.8: each start's logs outlast a restart. rudolf-init keeps the last 4 starts (var/log/starts/0..3, with /run's
# logs pointing into the newest), the session marks when the desktop answered, the start-up (initramfs) keeps what it
# decided before handing over, restarting or stopping, and /usr/lib/rudolf/starts reads it all as JSON for System
# health. The scripts' own lines.   sh startlogs58_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); SY=$HERE/../../sys/root; I=$HERE/../../initramfs/init
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; P=0; F=0
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
sed -n '/^starts_keep() {/,/^}/p' $SY/sbin/rudolf-init > $W/keep.sh
export STARTS_PERSIST=$W/p STARTS_RUN=$W/run STARTS_ETC=$W/etc; mkdir -p $W/p/os/system/b $W/run $W/etc
printf 'serial 2026111100\n' > $W/p/os/system/b/RELEASE; echo b > $W/etc/slot; echo rudolf-start.conf > $W/etc/boot-entry; echo 'rudolf.boot=uefi rudolf.slot=b' > $W/etc/cmdline
echo '23:01:02 Rudolf OS: hello from the start-up' > $W/etc/initramfs.log; mkdir -p $W/p/var/log; printf '== x\nold decision\n' > $W/p/var/log/boot-decisions.log
for n in 1 2 3 4 5; do (. $W/keep.sh; starts_keep); echo "start $n" > $W/run/session.log; done
S=$W/p/var/log/starts
ok "1. the last 4 starts are kept, newest in 0" $([ -d $S/3 ] && [ ! -d $S/4 ] && grep -qx 'start 5' $S/0/session.log && grep -qx 'start 2' $S/3/session.log; echo $?)
ok "2. /run's logs are written straight into this start's folder" $([ "$(readlink $W/run/session.log)" = $S/0/session.log ] && [ "$(readlink $W/run/webkit-desktop.log)" = $S/0/webkit-desktop.log ] && [ "$(readlink $W/run/devlink/daemon.log)" = $S/0/daemon.log ] && [ "$(cat $W/run/start-logs)" = $S/0 ]; echo $?)
ok "3. what started: slot, release, menu entry, options, and the start-up's own lines" $(grep -qx 'slot=b' $S/0/start.txt && grep -qx 'release=2026111100' $S/0/start.txt && grep -q 'entry=rudolf-start.conf' $S/0/start.txt && grep -q 'hello from the start-up' $S/0/initramfs.log && grep -q 'old decision' $S/0/boot-decisions.log; echo $?)
ok "4. only root reads them" $([ "$(stat -c %a $S)" = 700 ]; echo $?)
# the session's mark
sed -n '/^desk_up_mark() {/,/}$/p' $SY/usr/lib/rudolf/session | head -1 > $W/mark.sh; grep -q 'desk_up_mark' $W/mark.sh || echo "no desk_up_mark"
o=$(sed "s#/run/rudolf/#$W/run/#g; s#/proc/uptime#$W/uptime#" $W/mark.sh); echo "$o" > $W/mark.sh; echo '42.5 1.0' > $W/uptime
(. $W/mark.sh; ENGINE=webkit; desk_up_mark); ok "5. no answer yet: nothing marked" $([ ! -e $S/0/desktop-up ]; echo $?)
touch $W/run/heartbeat; (. $W/mark.sh; ENGINE=webkit; desk_up_mark); echo '99 1' > $W/uptime; (. $W/mark.sh; ENGINE=chromium; desk_up_mark)
ok "6. the desktop's first answer is marked once: when, and with which engine" $([ "$(cat $S/0/desktop-up)" = "42 webkit" ]; echo $?) "$(cat $S/0/desktop-up)"
# the reader
printf 'Tue Nov 10 22:00:00 UTC 2026 browser exited\nTue Nov 10 22:00:08 UTC 2026 browser exited\nTue Nov 10 22:00:09 UTC 2026 the WebKit desktop did not start (it closed 3 times in a row as it started) - using Chromium\n' > $S/1/session.log
printf '23:00:01 Rudolf OS: say "hi" \\ there\n' > $S/1/initramfs.log
J=$(STARTS_DIR=$S sh $SY/usr/lib/rudolf/starts)
echo "$J" | python3 -c '
import json,sys;j=json.load(sys.stdin);s=j["starts"];a=s[0];b=s[1]
assert len(s)==4,len(s); assert a["n"]==0 and a["desktopUp"]==42 and a["engine"]=="webkit" and a["slot"]=="b" and a["release"]=="2026111100",a
assert b["desktopUp"] is None and b["exits"]==2 and "closed 3 times" in b["gaveUp"] and "\"hi\" \\ there" in b["startup"],b
' 2>$W/err; ok "7. the reader: valid JSON, a start that came up and one that did not (with why, and the start-up's quoted lines)" $? "$(cat $W/err) $J"
# the start-up keeps its decisions on RUDOLFDATA before it hands over, restarts or stops
{ sed -n '/^say() {/,/^}/p' $I | sed '/^efv()/,$d'; } > $W/i.sh
sed -i "s#/dev/console#$W/console#; s#/tmp/initramfs.log#$W/il#g; s#/mnt/persist#$W/mp#g" $W/i.sh
mkdir -p $W/mp/os; (. $W/i.sh; SLOT=a; ENTRY=rudolf-start.conf; note "start: good=b -> slot a"; say "Rudolf OS: this system has no 'chromium' layer."; keeplog "slot a given up")
ok "8. the start-up's decisions are kept on RUDOLFDATA (boot-decisions.log)" $(grep -q '== .* slot a given up (kernel .*, slot a, entry rudolf-start.conf)' $W/mp/var/log/boot-decisions.log && grep -q "start: good=b -> slot a" $W/mp/var/log/boot-decisions.log && grep -q "no 'chromium' layer" $W/mp/var/log/boot-decisions.log && [ ! -s $W/il ]; echo $?) "$(cat $W/mp/var/log/boot-decisions.log 2>&1)"
for i in $(seq 1 400); do echo "line $i"; done > $W/mp/var/log/boot-decisions.log; (. $W/i.sh; keeplog "x")
ok "9. ...the last 300 lines" $([ $(wc -l < $W/mp/var/log/boot-decisions.log) = 300 ]; echo $?)
grep -q 'fail() { keeplog "stopped"' $I && grep -q 'keeplog "handing over to slot $T"' $I && grep -q 'keeplog "slot $SLOT given up"' $I && grep -q 'cp /tmp/initramfs.log /newroot/etc/rudolf/initramfs.log' $I; ok "10. kept before every hand-over, restart and stop; the rest goes with the start" $?
grep -q '"system/starts"' $HERE/../../sys/rudolfd.c && grep -q 'sessionBefore' $HERE/../../sys/rudolfd.c; ok "11. rudolfd serves it (GET system/starts) and gives Blitzen the start before" $?
echo "$P/$((P+F)) passed"; [ $F = 0 ]
