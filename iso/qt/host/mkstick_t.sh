#!/bin/sh
# 1.5.3: mkstick.sh's last check, before it says a stick is ready. A stick made from an installed Rudolf OS has a boot
# image (boot files only), so it starts only with the installed system copied into its RUDOLFDATA (on 2026-11-07 one
# was reported ready with an empty os folder; the HP said "this drive has boot files only"). Stand-in folders.
HERE=$(cd "$(dirname "$0")" && pwd); M=$HERE/../../sys/root/usr/lib/rudolf/mkstick.sh
T=$(mktemp -d); trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
chk() { o=$(sh $M --check "$@" 2>&1); r=$?; }
slot() { mkdir -p $1/os/system/a; echo "good=a" > $1/os/system/state; for L in vmlinuz base.sfs chromium.sfs modules.sfs desktop.sfs; do echo "$L of $2" > $1/os/system/a/$L; done; echo 1 > $1/os/system/a/complete; }
S=$T/stick; mkdir -p $S/os
chk $S 0; ok "a whole disc image on the stick: nothing more needed" $([ $r = 0 ] && echo 1 || echo 0) "$r $o"
chk $S 1; ok "a boot image and an empty os folder (the HP's stick): refused" $([ $r = 11 ] && [ "$(has "$o" "has no system")" = 1 ] && echo 1 || echo 0) "$r $o"
slot $S 1.5.3; rm $S/os/system/a/complete
chk $S 1; ok "...a slot without 'complete': refused" $([ $r = 12 ] && [ "$(has "$o" "incomplete")" = 1 ] && echo 1 || echo 0) "$r $o"
slot $S 1.5.3; rm $S/os/system/a/chromium.sfs
chk $S 1; ok "...a layer missing: refused, naming it" $([ $r = 12 ] && [ "$(has "$o" "missing chromium.sfs")" = 1 ] && echo 1 || echo 0) "$r $o"
slot $S 1.5.3; : > $S/os/system/a/base.sfs
chk $S 1; ok "...an empty layer: refused" $([ $r = 12 ] && [ "$(has "$o" "missing base.sfs")" = 1 ] && echo 1 || echo 0) "$r $o"
slot $S 1.5.3; chk $S 1; ok "...the whole system copied: ready" $([ $r = 0 ] && echo 1 || echo 0) "$r $o"
slot $T/src 1.5.3; echo "a different modules" > $S/os/system/a/modules.sfs
o=$(SRCOS=$T/src/os sh $M --check $S 1 2>&1); r=$?
ok "...a copy that does not match this computer's: refused" $([ $r = 13 ] && [ "$(has "$o" "modules.sfs on the new stick does not match")" = 1 ] && echo 1 || echo 0) "$r $o"
slot $S 1.5.3; o=$(SRCOS=$T/src/os sh $M --check $S 1 2>&1); r=$?
ok "...a copy that matches: ready" $([ $r = 0 ] && echo 1 || echo 0) "$r $o"
sed -i 's/good=a/good=disc/' $S/os/system/state; chk $S 1
ok "...the system in use is the disc's, which a boot image does not have: refused" $([ $r = 11 ] && echo 1 || echo 0) "$r $o"
echo "$((N-F))/$N passed"; [ $F = 0 ]
