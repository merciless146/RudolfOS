#!/bin/sh
# 1.5.8: a stick's rescue system (slot r, kept by Make a stick, never changed by updates): the start-up falls back to it
# when no installed system can start, Recovery mode can choose it, and if it cannot start either the start-up says so
# and stops (no restart loop). The script's own lines, as in startfall57_t.sh.   sh startfall58_t.sh (root)
HERE=$(cd "$(dirname "$0")" && pwd); I=$HERE/../../initramfs/init
[ "$(id -u)" = 0 ] || { echo "run as root"; exit 2; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
{ echo 'say() { echo "$*"; }; fail() { echo FAIL-STOP; exit 0; }; sync() { :; }; umount() { :; }; umount_media() { :; }; mount_media() { :; }; blkid() { echo "x UUID=\"u1\" y"; }; whole() { return 1; }; sleep() { :; }'
  echo 'reboot() { echo REBOOT; exit 0; }; kexec_stub() { echo "KEXEC $2"; exit 0; }'
  # (1.5.7: drives for Recovery mode from a USB stick: /fakes/NAME is what the partition NAME holds; sda is the stick)
  echo 'is_usb() { case "$1" in *sda*) return 0;; esac; return 1; }; disk_of() { echo "${1%[0-9]}"; }; is_data() { [ -d "/fakes/${1##*/}" ]; }'
  echo 'mount() { if [ "$1 $2" = "-t ext4" ]; then [ -d "/fakes/${5##*/}" ] || return 1; rm -rf /mnt/persist; cp -a "/fakes/${5##*/}" /mnt/persist; fi; return 0; }'
  echo 'has() { case " $CMD " in *" $1 "*) return 0;; esac; return 1; }'
  sed -n '/^SYSD=\/mnt\/persist/,/^slot_bad() {/p' "$I"; sed -n '/^slot_bad() {/,/^}/p' "$I" | tail -n +2
  sed -n '/^REC=""/,/^# the media check/p' "$I" | sed '$d' | sed 's/rudolf-kexec /kexec_stub /; s#/dev/nvme?n?p3 /dev/mmcblk?p3 /dev/sd?3 /dev/vd?3#$DEVS#'
  sed -n '/^for L in desktop chromium modules base; do/,/\[ -n "\$F" \] ||/p' "$I"; echo 'done; echo LAYERS-OK'
} > $W/part.sh
grep -q 'kexec_stub' $W/part.sh && grep -q 'slot_bad' $W/part.sh || { echo "FAIL could not take the script's lines"; exit 1; }
unshare -m --propagation private sh -s "$W" <<'XEOF'
W=$1; P=0; F=0; ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 - $3"; fi; }
mount -t tmpfs t /mnt; S=/mnt/persist/os/system
slot() { mkdir -p $S/$1; for f in vmlinuz base.sfs chromium.sfs desktop.sfs modules.sfs; do echo x > $S/$1/$f; done; echo 1 > $S/$1/complete; }
reset() { rm -rf /mnt/persist /mnt/media; mkdir -p $S /mnt/media/rudolf; touch /mnt/media/rudolf/bootonly; slot a; slot b; slot r; printf "$1" > $S/state; }
run() { (CMD="$1"; SLOT="$2"; PERSIST=/dev/sda3; MEDIA=/dev/sda1; SB=1; . $W/part.sh) 2>&1; }
reset 'good=a\nprev=b\nnext=\ntries=0\nfailed=\n'; o=$(run "rudolf.boot=uefi" "")
ok "1. a working installed system starts as before (not the rescue system)" $(echo "$o" | grep -q 'KEXEC .*rudolf.slot=a' && ! echo "$o" | grep -q 'rescue'; echo $?) "$o"
reset 'good=a\nprev=b\nnext=\ntries=0\nfailed=\n'; rm $S/a/complete; o=$(run "rudolf.boot=uefi" "")
ok "2. the good system incomplete (a damaged update on the stick): the rescue system, saying so" $(echo "$o" | grep -q 'KEXEC .*rudolf.slot=r' && echo "$o" | grep -q 'starting its rescue system' && ! echo "$o" | grep -q 'boot files only'; echo $?) "$o"
reset 'good=\nprev=\nnext=\ntries=0\nfailed=\n'; o=$(run "rudolf.boot=uefi" "")
ok "3. no good system recorded at all: the rescue system" $(echo "$o" | grep -q 'KEXEC .*rudolf.slot=r'; echo $?) "$o"
reset 'good=a\nprev=b\nnext=\ntries=0\nfailed=\n'; rm $S/a/complete $S/b/complete; o=$(run "rudolf.boot=uefi rudolf.recovery=1" "")
ok "4. Recovery mode: the last good one, else the previous one, else the rescue system, with safe settings" $(echo "$o" | grep -q 'KEXEC .*rudolf.recovery=1.*rudolf.slot=r'; echo $?) "$o"
reset 'good=a\nprev=b\nnext=\ntries=0\nfailed=\n'; o=$(run "rudolf.slot=r" r)
ok "5. the rescue system starts (its layers)" $(echo "$o" | grep -q LAYERS-OK; echo $?) "$o"
reset 'good=a\nprev=b\nnext=\ntries=0\nfailed=\n'; rm $S/r/base.sfs; o=$(run "rudolf.slot=r" r)
ok "6. a damaged rescue system: says so and stops, no restart loop, the state untouched" $(echo "$o" | grep -q 'rescue system on this drive cannot start' && echo "$o" | grep -q FAIL-STOP && ! echo "$o" | grep -q REBOOT && grep -q '^good=a' $S/state; echo $?) "$o"
reset 'good=a\nprev=b\nnext=\ntries=0\nfailed=\n'; rm -rf $S/r; rm $S/a/complete $S/b/complete; o=$(run "rudolf.boot=uefi" "")
ok "7. a stick made before 1.5.8 (no rescue system), nothing that starts: boot files only, as before" $(echo "$o" | grep -q 'boot files only' && ! echo "$o" | grep -q 'rudolf.slot=r'; echo $?) "$o"
reset 'good=a\nprev=b\nnext=\ntries=0\nfailed=\n'; rm /mnt/media/rudolf/bootonly; for f in base chromium desktop modules; do echo x > /mnt/media/rudolf/$f.sfs; done; rm $S/a/complete; o=$(run "rudolf.boot=uefi" "")
ok "8. a whole disc image: its own system, as before (the rescue system is for boot images)" $(! echo "$o" | grep -q 'rudolf.slot=r' && echo "$o" | grep -q LAYERS-OK; echo $?) "$o"
# 1.5.9 (plan 8.4): Recovery mode starts the official recovery system (slot r marked official) before the last good one
reset 'good=a\nprev=b\nnext=\ntries=0\nfailed=\n'; echo 'official recovery system' > $S/r/official; o=$(run "rudolf.boot=uefi rudolf.recovery=1" "")
ok "9. Recovery mode with an official recovery system: that one, though slot a can start" $(echo "$o" | grep -q 'KEXEC .*rudolf.recovery=1.*rudolf.slot=r'; echo $?) "$o"
o=$(run "rudolf.boot=uefi" "")
ok "10. ...a normal start still starts the good system" $(echo "$o" | grep -q 'KEXEC .*rudolf.slot=a'; echo $?) "$o"
echo "$P/$((P+F)) passed"; [ $F = 0 ]
XEOF
