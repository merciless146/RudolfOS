#!/bin/sh
# 1.5.3: oscopy.sh, as Make a USB stick (mkstick.sh) and Install here (install.sh) use it: copying the installed
# system, and the last check before a stick or an install is called ready. A stick made from an installed Rudolf OS has a boot
# image (boot files only), so it starts only with the installed system copied into its RUDOLFDATA (on 2026-11-07 one
# was reported ready with an empty os folder; the HP said "this drive has boot files only"). Stand-in folders.
HERE=$(cd "$(dirname "$0")" && pwd); LIB=$HERE/../../sys/root/usr/lib/rudolf; M=$LIB/mkstick.sh; export RUDOLF_LIB=$LIB
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
ok "...a copy that does not match this computer's: refused" $([ $r = 13 ] && [ "$(has "$o" "modules.sfs on the new drive does not match")" = 1 ] && echo 1 || echo 0) "$r $o"
slot $S 1.5.3; o=$(SRCOS=$T/src/os sh $M --check $S 1 2>&1); r=$?
ok "...a copy that matches: ready" $([ $r = 0 ] && echo 1 || echo 0) "$r $o"
sed -i 's/good=a/good=disc/' $S/os/system/state; chk $S 1
ok "...the system in use is the disc's, which a boot image does not have: refused" $([ $r = 11 ] && echo 1 || echo 0) "$r $o"
# the copy (Install here from a stick made from an installed Rudolf OS: the HP)
. $LIB/oscopy.sh
mkdir -p $T/media/rudolf; export RUDOLF_MEDIA=$T/media
ok "running from a whole disc image: no copy needed" $([ "$(os_running_bootonly)" = 0 ] && echo 1 || echo 0)
touch $T/media/rudolf/bootonly
ok "running from a drive with boot files only: the system must be copied" $([ "$(os_running_bootonly)" = 1 ] && echo 1 || echo 0)
D=$T/newdrive; mkdir -p $D/os; o=$(os_copy $D $T/src/os 2>&1); r=$?
ok "the copy says what it copies" $([ $r = 0 ] && [ "$(has "$o" "Copying the installed system")" = 1 ] && echo 1 || echo 0) "$r $o"
o=$(os_check $D 1 $T/src/os 2>&1); r=$?
ok "...and the new drive can start, read back the same" $([ $r = 0 ] && echo 1 || echo 0) "$r $o"
o=$(os_copy $T/nothing-here $T/missing/os 2>&1); r=$?
ok "no installed system to copy: refused, not skipped" $([ $r = 11 ] && [ "$(has "$o" "no installed system")" = 1 ] && echo 1 || echo 0) "$r $o"
# 1.5.7: os_bootimg: a stick gets the boot files of the system in use when the drive's are older (the Vivobook: 1.2.8's,
# whose Recovery mode could not start an installed system); downloaded and checked, else the drive's own with a warning
B=$T/bi; mkdir -p $B/etc $B/media/rudolf $B/os/system/b $B/cache $B/rel
echo b > $B/etc/slot; printf 'serial 2026111101\n' > $B/os/system/b/RELEASE; echo KERNEL-1101 > $B/os/system/b/vmlinuz
printf 'L\tvmlinuz\t11\tabc\thttps://github.com/merciless146/RudolfOS/releases/download/r2026111101/vmlinuz\n' > $B/os/system/b/manifest.txt
echo ID1 > $B/media/rudolf/id; printf 'serial 2026101000\n' > $B/media/rudolf/release; touch $B/media/rudolf/bootonly
# the release's boot image, unpacked (RUDOLF_ISODIR): what the stand-in download gives
mkrel() { rm -rf $B/rel; mkdir -p $B/rel/rudolf $B/rel/boot; touch $B/rel/rudolf/bootonly; echo ID1 > $B/rel/rudolf/id; echo KERNEL-1101 > $B/rel/boot/vmlinuz; echo recovery-installed > $B/rel/rudolf/features; }
printf '#!/bin/sh\necho "$1" >> %s/urls; cp -a %s/rel "$2"\n' $B $B > $B/fetch; chmod +x $B/fetch
bi() { rm -rf $B/cache/*; o=$(RUDOLF_ETC=$B/etc RUDOLF_FETCH=$B/fetch RUDOLF_ISODIR=1 sh -c ". $LIB/oscopy.sh; os_bootimg $B/os $B/media $B/cache" 2>&1); r=$?; }
mkrel; bi
ok "bootimg: the drive's boot files are older: the system's own boot image, downloaded from beside its kernel and checked" $([ $r = 0 ] && [ "$(echo "$o" | tail -1)" = $B/cache/boot-2026111101.iso ] && grep -qx 'https://github.com/merciless146/RudolfOS/releases/download/r2026111101/boot.iso' $B/urls && echo 1 || echo 0) "$r $o"
mkrel; echo OTHER > $B/rel/boot/vmlinuz; bi
ok "bootimg: ...refused when its kernel is not this system's" $([ $r = 2 ] && [ "$(has "$o" "its kernel is not this system's")" = 1 ] && [ ! -e $B/cache/boot-2026111101.iso ] && echo 1 || echo 0) "$r $o"
mkrel; echo ID2 > $B/rel/rudolf/id; bi
ok "bootimg: ...refused with another disc ID" $([ $r = 2 ] && [ "$(has "$o" "another Rudolf OS disc ID")" = 1 ] && echo 1 || echo 0) "$r $o"
mkrel; rm $B/rel/rudolf/features; bi
ok "bootimg: ...refused without Recovery mode for installed systems" $([ $r = 2 ] && [ "$(has "$o" "no Recovery mode")" = 1 ] && echo 1 || echo 0) "$r $o"
mkrel; printf '#!/bin/sh\nexit 1\n' > $B/fetch0; chmod +x $B/fetch0; o=$(RUDOLF_ETC=$B/etc RUDOLF_FETCH=$B/fetch0 RUDOLF_ISODIR=1 sh -c ". $LIB/oscopy.sh; os_bootimg $B/os $B/media $B/cache" 2>&1); r=$?
ok "bootimg: ...offline: none (the stick gets the drive's own, with a warning)" $([ $r = 2 ] && [ "$(has "$o" "could not be downloaded")" = 1 ] && grep -q 'Warning: the stick gets this drive' $M && echo 1 || echo 0) "$r $o"
echo recovery-installed > $B/media/rudolf/features; printf 'serial 2026111101\n' > $B/media/rudolf/release; bi
ok "bootimg: the drive's own boot files are current: kept (nothing downloaded)" $([ $r = 1 ] && echo 1 || echo 0) "$r $o"
echo disc > $B/etc/slot; bi; ok "bootimg: running the drive's own system: its own boot files" $([ $r = 1 ] && echo 1 || echo 0) "$r $o"
grep -q '/persist/os/bootimg/boot-\[0-9\]\*.iso) \[ "\$MKSTICK" = 1 \] && STICKBOOT=1' $LIB/install.sh; ok "install.sh takes that boot image only for Make a stick" $([ $? = 0 ] && echo 1 || echo 0) ""
echo "$((N-F))/$N passed"; [ $F = 0 ]
