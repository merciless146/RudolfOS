#!/bin/sh
# 1.5.7: the start-up (initramfs/init) never strands the computer: a system slot missing a layer goes back to the
# previous system (the Vivobook: an update that counted on a USB stick), and Recovery mode starts the installed system
# with safe settings instead of the drive's own, which an installed computer's drive does not have. The script's own
# lines, with /mnt in memory and stand-ins for kexec and reboot.   sh startfall57_t.sh (root)
HERE=$(cd "$(dirname "$0")" && pwd); I=$HERE/../../initramfs/init
[ "$(id -u)" = 0 ] || { echo "run as root"; exit 2; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
{ echo 'say() { echo "$*"; }; fail() { echo FAIL-STOP; exit 0; }; sync() { :; }; umount() { :; }; mount() { :; }; umount_media() { :; }; mount_media() { :; }; blkid() { echo "x UUID=\"u1\" y"; }; whole() { return 1; }; sleep() { :; }'
  echo 'reboot() { echo REBOOT; exit 0; }; kexec_stub() { echo "KEXEC $2"; exit 0; }'
  echo 'has() { case " $CMD " in *" $1 "*) return 0;; esac; return 1; }'
  sed -n '/^SYSD=\/mnt\/persist/,/^slot_bad() {/p' "$I"; sed -n '/^slot_bad() {/,/^}/p' "$I" | tail -n +2
  sed -n '/^REC=""/,/^# the media check/p' "$I" | sed '$d' | sed 's/rudolf-kexec /kexec_stub /'
  sed -n '/^for L in desktop chromium modules base; do/,/\[ -n "\$F" \] ||/p' "$I"; echo 'done; echo LAYERS-OK'
} > $W/part.sh
grep -q 'kexec_stub' $W/part.sh && grep -q 'slot_bad' $W/part.sh || { echo "FAIL could not take the script's lines"; exit 1; }
unshare -m --propagation private sh -s "$W" <<'XEOF'
W=$1; P=0; F=0; ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 - $3"; fi; }
mount -t tmpfs t /mnt; S=/mnt/persist/os/system
slot() { mkdir -p $S/$1; for f in vmlinuz base.sfs chromium.sfs desktop.sfs modules.sfs; do echo x > $S/$1/$f; done; echo 1 > $S/$1/complete; }
reset() { rm -rf /mnt/persist /mnt/media; mkdir -p $S /mnt/media/rudolf; touch /mnt/media/rudolf/bootonly; slot a; slot b; printf "$1" > $S/state; }
run() { (CMD="$1"; SLOT="$2"; PERSIST=/dev/nvme0n1p3; MEDIA=/dev/nvme0n1; SB=1; . $W/part.sh) 2>&1; }
# Recovery mode on an installed computer (boot files only on its drive)
reset 'good=b\nprev=a\nnext=\ntries=0\nfailed=\n'; o=$(run "rudolf.boot=uefi rudolf.recovery=1" "")
ok "1. Recovery mode starts the installed system (the last good one) with safe settings" $(echo "$o" | grep -q 'KEXEC .*rudolf.recovery=1.*rudolf.slot=b' && ! echo "$o" | grep -q 'boot files only'; echo $?) "$o"
ok "2. ...and changes nothing" $(printf 'good=b\nprev=a\nnext=\ntries=0\nfailed=\n' | cmp -s - $S/state; echo $?)
reset 'good=b\nprev=a\nnext=a\ntries=2\nfailed=\n'; o=$(run "rudolf.recovery=1" "")
ok "3. ...never an update waiting to be tried" $(echo "$o" | grep -q 'rudolf.slot=b' && ! echo "$o" | grep -q 'rudolf.trying'; echo $?) "$o"
reset 'good=b\nprev=a\nnext=\ntries=0\nfailed=\n'; rm $S/b/complete; o=$(run "rudolf.recovery=1" "")
ok "4. ...the previous one when the last good one is incomplete" $(echo "$o" | grep -q 'rudolf.slot=a'; echo $?) "$o"
reset 'good=disc\nprev=\nnext=\ntries=0\nfailed=\n'; o=$(run "rudolf.recovery=1" "")
ok "5. ...and the drive's own system when nothing is installed (as before)" $(! echo "$o" | grep -q KEXEC; echo $?) "$o"
reset 'good=b\nprev=a\nnext=\ntries=0\nfailed=\n'; o=$(run "rudolf.boot=uefi" "")
ok "6. a normal start is unchanged: the good system" $(echo "$o" | grep -q 'KEXEC .*rudolf.slot=b' && ! echo "$o" | grep -q 'rudolf.recovery'; echo $?) "$o"
# the Vivobook: an update in slot a without its chromium layer, being tried
reset 'good=b\nprev=\nnext=\ntries=0\nfailed=\n'; rm $S/a/chromium.sfs; o=$(run "rudolf.boot=uefi rudolf.slot=a rudolf.trying=1" a)
ok "7. a system without a layer: back to the previous one (not the boot-files-only dead end)" $(echo "$o" | grep -q "no 'chromium' layer" && echo "$o" | grep -q REBOOT && ! echo "$o" | grep -q 'boot files only'; echo $?) "$o"
ok "8. ...marked as failed, and the good one is still good" $(grep -q '^failed=a' $S/state && grep -q '^good=b' $S/state && grep -q '^next=$' $S/state; echo $?) "$(cat $S/state)"
reset 'good=a\nprev=b\nnext=\ntries=0\nfailed=\n'; rm $S/a/base.sfs; o=$(run "rudolf.slot=a" a)
ok "9. the good system itself without a layer: the previous one becomes the good one" $(grep -q '^good=b' $S/state && grep -q '^failed=a' $S/state; echo $?) "$(cat $S/state)"
reset 'good=b\nprev=a\nnext=\ntries=0\nfailed=\n'; o=$(run "rudolf.slot=b" b)
ok "10. a complete system starts" $(echo "$o" | grep -q LAYERS-OK; echo $?) "$o"
echo "$P/$((P+F)) passed"; [ $F = 0 ]
XEOF
