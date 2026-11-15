#!/bin/sh
# 1.6.0: Settings › System health › Log video problems on the native desktop too. The session's own lines (from
# "Log video problems" to the Lite desktop's start), run with stand-ins for su and oracle_chrome, in a mount namespace
# with /run and /persist of its own: with the switch on, rudolf-native gets GStreamer's warnings into webkit-video.log
# and Oracle's Chromium there its media/ messages; off, neither; Lite as before; Standard (Chromium) untouched. The
# native desktop's log is kept on the disk for this start (rudolf-init) and in the system report (rudolfd). Needs root.
#   sh videolog60_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root (a mount namespace)"; exit 0; }
[ -z "$VL60_INNER" ] && exec env VL60_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; SES=$SYS/root/usr/lib/rudolf/session; T=$(mktemp -d /tmp/vl60.XXXXXX)
mount --make-rprivate / 2>/dev/null; mount -t tmpfs none /run && mkdir -p /run/rudolf && mkdir -p /persist && mount -t tmpfs none /persist && mkdir -p /persist/etc || { echo "FAIL mounts"; exit 1; }
trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 600)"; F=$((F+1)); fi; }
awk '/# 1.5.8: Settings › System health › Log video problems/{f=1} f&&/^\telif \[ \$ENGINE = webkit \]; then/{print; print "\t\tsu -s /bin/sh rudolf -c \"exec env $VLOG /usr/bin/rudolf-webkit\""; print "\tfi"; exit} f' $SES > $T/block.sh
grep -q 'rudolf-native' $T/block.sh || { echo "FAIL the session's lines not found"; exit 1; }
run() { # ENGINE SWITCH
	rm -f /run/rudolf/webkit-video.log $T/su $T/extra; [ "$2" = 1 ] && echo video_log=1 > /persist/etc/rudolf-memory || echo video_log=0 > /persist/etc/rudolf-memory
	( ENGINE=$1; EXTRA="--keep-me"; SCW=1920; SCH=1080; WKSOFT=; START=http://127.0.0.1:8080/
	  su() { shift 4; echo "$1" >> $T/su; }; chown() { :; }; oracle_chrome() { echo "$EXTRA" > $T/extra; }
	  . $T/block.sh; wait; echo "$EXTRA" > $T/after ) 2>$T/err; }
run native 1
ok "1. Native, the switch on: rudolf-native started with GStreamer's warnings to webkit-video.log" "$(grep -q '^exec env GST_DEBUG=\*:2 GST_DEBUG_NO_COLOR=1 GST_DEBUG_FILE=/run/rudolf/webkit-video.log /usr/bin/rudolf-native ' $T/su && [ -f /run/rudolf/webkit-video.log ] && echo 1)" "$(cat $T/su $T/err)"
ok "...Oracle's Chromium there with its media/ messages, its other flags kept, and nothing added for later" "$(grep -q -- '--keep-me --enable-logging=stderr --v=0 --vmodule=\*/media/\*=1' $T/extra && [ "$(cat $T/after)" = "--keep-me" ] && echo 1)" "$(cat $T/extra $T/after)"
run native 0
ok "2. Native, the switch off: neither (no GStreamer log, Oracle's flags as they were)" "$(grep -q '^exec env  */usr/bin/rudolf-native ' $T/su && ! grep -q GST_DEBUG $T/su && [ "$(cat $T/extra)" = "--keep-me" ] && [ ! -e /run/rudolf/webkit-video.log ] && echo 1)" "$(cat $T/su $T/extra)"
run webkit 1
ok "3. Lite, the switch on: as before (GStreamer's warnings), no Oracle flags" "$(grep -q 'GST_DEBUG_FILE=/run/rudolf/webkit-video.log /usr/bin/rudolf-webkit' $T/su && [ ! -e $T/extra ] && echo 1)" "$(cat $T/su)"
run chromium 1
ok "4. Standard, the switch on: untouched (Chromium's desktop has its own logs)" "$([ ! -s $T/su ] && [ ! -e /run/rudolf/webkit-video.log ] && echo 1)" "$(cat $T/su 2>/dev/null)"
grep -q 'for f in session.log webkit-desktop.log native-desktop.log ' $SYS/root/sbin/rudolf-init
ok "5. the native desktop's log kept on the disk for each start (rudolf-init), so a restart no longer loses it" "$([ $? = 0 ] && echo 1)"
grep -q 'facts_add(&s, "native", RUN "/native-desktop.log"' $SYS/rudolfd.c && grep -q 'facts_add(&s, "nativeBefore", PERSIST "/var/log/starts/1/native-desktop.log"' $SYS/rudolfd.c
ok "6. the system report includes it, this start's and the one before" "$([ $? = 0 ] && echo 1)"
# the size limit: the session's own line
L=$(grep -m1 'for VL in /run/rudolf/webkit-video.log /run/rudolf/native-desktop.log' $SES); mkdir -p $T/disk; ln -sf $T/disk/native-desktop.log /run/rudolf/native-desktop.log; head -c 5000000 /dev/zero | tr '\0' 'x' > $T/disk/native-desktop.log
( eval "$L" ); ok "7. both logs kept under 4 MB (the native one through its link to the disk: the file shortened, the link kept)" "$([ -L /run/rudolf/native-desktop.log ] && [ $(stat -c %s $T/disk/native-desktop.log) = 2097152 ] && echo 1)" "$(ls -l /run/rudolf/ $T/disk)"
grep -q "Log video problems (the Lite and Native desktops)" $HERE/../../../desktop/apps57.js
ok "8. Settings says so: \"(the Lite and Native desktops)\"" "$([ $? = 0 ] && echo 1)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
