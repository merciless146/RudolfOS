#!/bin/sh
# 1.5.7 (plan stage 5): rudolf-selftest on a stand-in computer (its /sys, /run/rudolf, /persist, /home/rudolf, /dev and
# /proc moved into a folder): each check passes when things are as they should be, fails when they are not, and is
# skipped when it does not apply; the script changes nothing.   sh selftest_t.sh (root, for the camera's group)
HERE=$(cd "$(dirname "$0")" && pwd); S=$HERE/../../sys/root/usr/lib/rudolf/selftest
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; NP=0; NF=0
ok() { if [ "$2" = 1 ]; then NP=$((NP+1)); echo "PASS $1"; else NF=$((NF+1)); echo "FAIL $1 - $3"; fi; }
has() { echo "$OUT" | grep -q "$1"; }
export ST_SYS=$W/sys ST_RUN=$W/run ST_PERSIST=$W/persist ST_HOME=$W/home ST_DEV=$W/dev ST_PROC=$W/proc
A=$ST_PERSIST/accounts/u-ann
mkdir -p $ST_RUN $ST_PERSIST/var/log $ST_PERSIST/etc $ST_HOME $ST_DEV $ST_PROC/self $A $ST_SYS/class/power_supply/BAT0 $ST_SYS/block/zram0 $ST_SYS/fs/cgroup/desktop $ST_SYS/class/net/eth0
for f in Documents Pictures Music Videos Downloads; do mkdir -p $A/$f; ln -s $A/$f $ST_HOME/$f; done   # (a link stands for the bind)
touch $ST_RUN/persistent; echo u-ann > $ST_RUN/home-session
: > $ST_PERSIST/var/log/rudolfd.log; : > $ST_PERSIST/var/log/rudolfd.log.1; ln -s $ST_PERSIST/var/log/rudolfd.log $ST_RUN/rudolfd.log
printf '{"t":1,"k":"start","m":"x"}\n' > $ST_PERSIST/var/log/health.log
mkdir -p $W/store/.incoming; ln -s $W/store $ST_RUN/dl
mkdir -p $ST_PROC/100; echo rudolfd > $ST_PROC/100/comm; : > $ST_PROC/self/mountinfo
# Debian, running, seeing the account's folders
mkdir -p $ST_PERSIST/debian $ST_RUN/debian $ST_PROC/200 $W/debroot/home/rudolf; touch $ST_PERSIST/debian/.rudolf-ready; echo 200 > $ST_RUN/debian/init.pid; echo debian-init > $ST_PROC/200/comm
for f in Documents Pictures Music Videos; do ln -s $A/$f $W/debroot/home/rudolf/$f; done; ln -s $W/debroot $ST_PROC/200/root
touch $ST_DEV/video0 $ST_DEV/video1; chgrp video $ST_DEV/video0 $ST_DEV/video1; chmod 660 $ST_DEV/video0 $ST_DEV/video1
echo care=80 > $ST_PERSIST/etc/power.conf; echo 80 > $ST_SYS/class/power_supply/BAT0/charge_control_end_threshold
echo 3221225472 > $ST_SYS/block/zram0/disksize; printf 'Filename Type Size Used Priority\n/dev/zram0 partition 3145724 0 100\n' > $ST_PROC/swaps
echo 'cpu memory' > $ST_SYS/fs/cgroup/cgroup.controllers; echo 805306368 > $ST_SYS/fs/cgroup/desktop/memory.low; echo 4242 > $ST_SYS/fs/cgroup/desktop/cgroup.procs
find $W -newer $W/sys -type f 2>/dev/null | sort > $W/before; sleep 1
OUT=$(sh $S); RC=$?
ok "1. everything as it should be: no failure, exit 0" "$([ $RC = 0 ] && ! echo "$OUT" | grep -q ^FAIL && echo 1)" "$OUT"
ok "2. ...the folders, Debian's view, the camera, the battery limit, memory and the logs pass" "$(has 'PASS folders' && has 'PASS debian' && has 'PASS camera - 2 camera' && has 'PASS battery-limit - charging stops at 80 %' && has 'PASS compressed-memory - 3072 MiB' && has 'PASS desktop-memory - the desktop keeps 768 MiB, with its browser in it' && has 'PASS logs - .* (1 earlier' && has 'PASS rudolfd' && has 'PASS downloads-folder' && echo 1)" "$OUT"
ok "3. ...no Wi-Fi adapter: skipped; a summary line" "$(has 'SKIP wifi - no Wi-Fi adapter' && echo "$OUT" | tail -1 | grep -q '^[0-9]* passed, 0 failed, [0-9]* skipped$' && echo 1)" "$OUT"
ok "4. it changed nothing" "$([ -z "$(find $W -newer $W/before -type f 2>/dev/null)" ] && echo 1)" "$(find $W -newer $W/before)"
# what went wrong on the Vivobook during 1.5.6, each made again
rm $ST_HOME/Music; mkdir $ST_HOME/Music                                         # a bind missing
rm $W/debroot/home/rudolf/Pictures; mkdir $W/debroot/home/rudolf/Pictures       # Debian sees its own Pictures
chgrp root $ST_DEV/video1; chmod 600 $ST_DEV/video1                             # the camera for the system only (1.5.6)
echo care=70 > $ST_PERSIST/etc/power.conf                                        # 70 % asked, 80 % kept
touch -d '3 days ago' $W/store/.incoming/old.crdownload                          # an old partial download
rm $ST_RUN/rudolfd.log; : > $ST_RUN/rudolfd.log                                  # rudolfd's log in /run only
echo 0 > $ST_SYS/fs/cgroup/desktop/memory.low
OUT=$(sh $S); RC=$?
ok "5. problems: exit 1" "$([ $RC = 1 ] && echo 1)" "$OUT"
ok "6. ...each named: the missing bind, Debian's Pictures, the camera, the battery limit, the partial download, the log, the desktop's memory" "$(has 'FAIL folders - Linux apps do not see the account.s: Music' && has 'FAIL debian - .*: Pictures' && has 'FAIL camera - not usable by apps: video1(root 600)' && has 'FAIL battery-limit - set to 70 %, the battery keeps 80 %' && has 'FAIL downloads-partial - 1 unfinished' && has 'FAIL logs' && has 'FAIL desktop-memory' && echo 1)" "$OUT"
echo 805306368 > $ST_SYS/fs/cgroup/desktop/memory.low
# a live start, nobody signed in, no camera, no battery, Debian not set up
rm -rf $ST_RUN/persistent $ST_RUN/home-session $ST_DEV/video* $ST_SYS/class/power_supply/BAT0 $ST_PERSIST/debian/.rudolf-ready
OUT=$(sh $S); RC=$?
ok "7. a live start with nothing to check: skipped, not failed" "$([ $RC = 0 ] && has 'SKIP storage - a live start' && has 'SKIP folders - nobody is signed in' && has 'SKIP camera - no camera' && has 'SKIP battery-limit - no battery' && has 'SKIP debian - Linux (Debian) is not set up' && echo 1)" "$OUT"
ok "8. rudolfd answers it (GET selftest), and rudolf-selftest is a command" "$(grep -q 'if (!strcmp(p, "selftest"))' $HERE/../../sys/rudolfd.c && [ "$(readlink $HERE/../../sys/root/usr/bin/rudolf-selftest)" = ../lib/rudolf/selftest ] && [ -x $S ] && echo 1)" ""
echo "$NP/$((NP+NF)) passed"; [ $NF = 0 ]
