#!/bin/sh
# 2.0 (Rudolf Kryptonite, the kryptonite branch; docs/plan-2.0.md "Status: the 7.3 kernel", finding b): with Secure Boot
# off, a layer that does not match its signed hash starts unchecked (the start-up script mounts its loop device itself
# and Task Manager says so). The checked device set up on that loop device first held it, so the mount failed with
# "Device or resource busy", the layer was called damaged and the start went back to the previous system: kr20_t.py's
# 7.3 kernel (its own modules layer under the beta's list) never reached the system. rudolf-verity close removes the
# checked device first. A throwaway key signs a list for a small fake layer; a second layer of the same size with other
# contents plays the changed one. Needs root, the device-mapper with dm-verity, loop devices, mksquashfs.
#   sudo sh verityfallback20_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); R=$HERE/../..
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
command -v mksquashfs >/dev/null && command -v losetup >/dev/null || { echo "SKIP: needs mksquashfs and losetup"; exit 0; }
modprobe dm-verity 2>/dev/null; [ -e /dev/mapper/control ] || { echo "SKIP: no device-mapper"; exit 0; }
W=$(mktemp -d); LP=""
cleanup() { umount "$W/m" 2>/dev/null; [ -x "$W/rv" ] && "$W/rv" close vfb20 2>/dev/null; [ -n "$LP" ] && losetup -d "$LP" 2>/dev/null; rm -rf "$W"; }
trap cleanup EXIT
P=0; N=0; ok() { N=$((N+1)); if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else echo "FAIL $1 - $3"; fi; }
gcc -Os -static -o "$W/rv" "$R/sys/rudolf-verity.c" || exit 1
mkdir "$W/d" "$W/m"; head -c 2097152 /dev/urandom > "$W/d/r"; mksquashfs "$W/d" "$W/good.sfs" -quiet -no-progress -noappend >/dev/null || exit 1
head -c 2097152 /dev/urandom > "$W/d/r"; mksquashfs "$W/d" "$W/changed.sfs" -quiet -no-progress -noappend >/dev/null || exit 1
python3 "$R/systools/verity.py" add "$W/good.sfs" vfb20 > "$W/vline" && python3 "$R/systools/verity.py" add "$W/changed.sfs" vfb20 >/dev/null || exit 1
python3 - "$W" <<'P'
import sys, base64
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization
w = sys.argv[1]; k = Ed25519PrivateKey.generate()
pub = k.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
open(w + '/keys', 'w').write(base64.b64encode(pub).decode() + '\n')
b, root, salt = open(w + '/vline').read().split()
text = ('version 0.0.0\nV\tvfb20\t%s\t%s\t%s\n' % (b, root, salt)).encode()
open(w + '/manifest.txt', 'wb').write(text); open(w + '/manifest.sig', 'wb').write(k.sign(text))
P
# the start-up script's steps (initramfs/init, the system layers), on the changed layer
LP=$(losetup -f --show -r "$W/changed.sfs") || exit 1
DEV=$("$W/rv" open "$W/manifest.txt" "$W/manifest.sig" "$W/keys" vfb20 "$LP" 2>"$W/e"); r=$?
ok "1. the changed layer (same size): the checked device is set up" $([ $r = 0 ] && [ -b "$DEV" ]; echo $?) "exit $r: $(cat "$W/e")"
mount -t squashfs -o ro "$DEV" "$W/m" 2>"$W/e"; r=$?; [ $r = 0 ] && umount "$W/m"
ok "2. ...and it does not mount through the checked device (its blocks do not match the signed hash)" $([ $r != 0 ]; echo $?) "it mounted"
mount -t squashfs -o ro "$LP" "$W/m" 2>"$W/e"; r=$?; [ $r = 0 ] && umount "$W/m"
ok "3. ...while the checked device stands, the loop device is held: the mount is refused (the finding)" $([ $r != 0 ]; echo $?) "it mounted"
"$W/rv" close vfb20 2>"$W/e"; r=$?
ok "4. rudolf-verity close removes the checked device" $([ $r = 0 ] && ! [ -e /dev/mapper/rudolf-vfb20 ] && ! dmsetup info rudolf-vfb20 >/dev/null 2>&1; echo $?) "exit $r: $(cat "$W/e")"
mount -t squashfs -o ro "$LP" "$W/m" 2>"$W/e"; r=$?
ok "5. ...then the loop device mounts (the unchecked start with Secure Boot off)" $([ $r = 0 ] && [ -s "$W/m/r" ]; echo $?) "$(cat "$W/e")"
umount "$W/m" 2>/dev/null; losetup -d "$LP"; LP=""
# the right layer still goes through the checked device, and the script closes it only on the unchecked path
LP=$(losetup -f --show -r "$W/good.sfs") || exit 1
DEV=$("$W/rv" open "$W/manifest.txt" "$W/manifest.sig" "$W/keys" vfb20 "$LP" 2>"$W/e") && mount -t squashfs -o ro "$DEV" "$W/m" 2>>"$W/e"; r=$?
ok "6. the right layer mounts through its checked device" $([ $r = 0 ] && [ -s "$W/m/r" ]; echo $?) "$(cat "$W/e")"
umount "$W/m" 2>/dev/null; "$W/rv" close vfb20 2>/dev/null
I=$R/initramfs/init
awk '/does not match its signed hash/{f=1} f&&/rudolf-verity close "\$L"/{c=1} f&&/mount -t squashfs -o ro "\$LOOP"/{print (c?"ok":"late"); exit}' "$I" | grep -q '^ok$'
ok "7. the start-up script closes the checked device before mounting the loop device, on the unchecked path only" $? "initramfs/init"
grep -B6 'rudolf-verity close "$L"' "$I" | grep -q 'SB = 1'; ok "8. ...after the Secure Boot and official-recovery refusals (which still stop the start)" $? "the close is not in the else branch"
echo "$P/$N passed"; [ $P = $N ]
