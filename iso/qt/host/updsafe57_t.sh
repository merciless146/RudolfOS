#!/bin/bash
# 1.5.7: system updates never change the installed system from a recovery start or from a system running off a USB
# stick, never count on a stick's layer, and only schedule a complete slot. A throwaway release (key made here, never
# kept) on a local server.   bash updsafe57_t.sh
set -u; cd "$(dirname "$0")"; I=$(cd ../.. && pwd); T=$(mktemp -d); P=0; F=0
SRV=""; trap '[ -n "$SRV" ] && kill $SRV 2>/dev/null; rm -rf "$T"' EXIT
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
RU=$T/ru; gcc -O2 -w -o $RU $I/sys/rudolf-update.c -lcrypto -ldl || exit 1
python3 - "$T" <<'P'
import sys,base64,os
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization as s
t=sys.argv[1];k=Ed25519PrivateKey.generate()
open(t+'/test.key','w').write(base64.b64encode(k.private_bytes(s.Encoding.Raw,s.PrivateFormat.Raw,s.NoEncryption())).decode())
open(t+'/keys','w').write(base64.b64encode(k.public_key().public_bytes(s.Encoding.Raw,s.PublicFormat.Raw)).decode()+'\n')
P
# the disc (a USB stick): its layers; the new release keeps the disc's chromium and changes the others
mkdir -p $T/media/rudolf $T/media/boot $T/l
for f in base chromium desktop modules; do head -c 300000 /dev/urandom > $T/media/rudolf/$f.sfs; done; head -c 200000 /dev/urandom > $T/media/boot/vmlinuz
printf 'serial 2026110100\nversion 1.5.6\n' > $T/media/rudolf/release
cp $T/media/rudolf/chromium.sfs $T/l/; for f in base desktop modules; do head -c 300000 /dev/urandom > $T/l/$f.sfs; done; head -c 200000 /dev/urandom > $T/kern
python3 $I/systools/make_release.py --layers $T/l --kernel $T/kern --out $T/srv --key $T/test.key --serial 2026111099 --version 1.5.7 >/dev/null 2>&1 || { echo "make_release failed"; exit 1; }
rm -f $T/test.key
(cd $T/srv && exec python3 -m http.server 8173 --bind 127.0.0.1 >/dev/null 2>&1) & SRV=$!
for i in 1 2 3 4 5 6 7 8 9 10; do python3 -c 'import urllib.request;urllib.request.urlopen("http://127.0.0.1:8173/channel/manifest.txt")' 2>/dev/null && break; sleep 0.3; done
# a fresh computer: RUDOLFDATA ($T/sys), started as SLOT, with the mounts MOUNTS and the start-up options CMD
fresh() { rm -rf $T/sys $T/conf; mkdir -p $T/sys; printf 'url=http://127.0.0.1:8173/channel\n' > $T/conf; echo "$1" > $T/slot; printf '%b' "$2" > $T/mounts; echo "$3" > $T/cmd; }
run() { RU_SYSD=$T/sys RU_MEDIA=$T/media RU_CONF=$T/conf RU_KEYS=$T/keys RU_SLOT=$T/slot RU_CMDLINE=$T/cmd RU_MOUNTS=$T/mounts RU_SYSBLOCK=$T/sysblock $RU "$@"; }
STICK="/dev/sda1 $T/media iso9660 ro 0 0\n/dev/nvme0n1p3 /persist ext4 rw 0 0\n"
OWN="/dev/sda1 $T/media iso9660 ro 0 0\n/dev/sda3 /persist ext4 rw 0 0\n"
INST="/dev/loop0 $T/media iso9660 ro 0 0\n/dev/nvme0n1p3 /persist ext4 rw 0 0\n"
mkdir -p $T/sysblock/loop0/loop; echo /dev/nvme0n1 > $T/sysblock/loop0/loop/backing_file

fresh disc "$STICK" "rudolf.boot=uefi"
o=$(run download 2>&1); r=$?
ok "1. running from a USB stick on an installed computer: no update (the Vivobook, 1.5.6 rescue stick)" $([ $r = 6 ] && echo "$o" | grep -q 'USB drive or disc' && [ ! -e $T/sys/a ] && [ ! -s $T/sys/state ]; echo $?) "$r $o"
ok "2. ...and the desktop is told so (canUpdate false)" $(run status | grep -q '"canUpdate":false'; echo $?)
fresh b "/dev/nvme0n1 $T/media iso9660 ro 0 0\n/dev/nvme0n1p3 /persist ext4 rw 0 0\n" "rudolf.slot=b rudolf.recovery=1"
o=$(run download 2>&1); r=$?
ok "3. recovery mode: no update" $([ $r = 6 ] && echo "$o" | grep -q 'recovery'; echo $?) "$r $o"
ok "4. recovery mode: boot-ok keeps nothing" $(printf 'good=a\nprev=\nnext=b\ntries=1\nfailed=\n' > $T/sys/state; run boot-ok; grep -q '^next=b' $T/sys/state && grep -q '^good=a' $T/sys/state; echo $?)
fresh disc "$OWN" "rudolf.boot=uefi"
o=$(run download 2>&1); r=$?
ok "5. a USB drive with its own RUDOLFDATA updates itself, using its own copy of an unchanged layer" $([ $r = 0 ] && echo "$o" | grep -q "chromium: unchanged (the disc's copy is used)" && [ ! -e $T/sys/a/chromium.sfs ] && [ -e $T/sys/a/complete ] && grep -q '^next=a' $T/sys/state; echo $?) "$r $o"
fresh disc "$INST" "rudolf.boot=uefi"
o=$(run download 2>&1); r=$?
ok "6. an installed computer's own disc image (through a loop device) counts as permanent" $([ $r = 0 ] && echo "$o" | grep -q "disc's copy is used"; echo $?) "$r $o"
# running the installed slot b with a rescue stick plugged in as the "disc": the unchanged layer goes into the slot
fresh b "$STICK" "rudolf.slot=b"; mkdir -p $T/sys/b; printf 'serial 2026110500\nversion 1.5.6\n' > $T/sys/b/RELEASE; echo 1 > $T/sys/b/complete
for f in base chromium desktop modules; do head -c 300000 /dev/urandom > $T/sys/b/$f.sfs; done; cp $T/media/boot/vmlinuz $T/sys/b/vmlinuz
printf 'good=b\nprev=\nnext=\ntries=0\nfailed=\n' > $T/sys/state
o=$(run download 2>&1); r=$?
ok "7. a stick's layer is copied into the new system, never counted on" $([ $r = 0 ] && cmp -s $T/sys/a/chromium.sfs $T/media/rudolf/chromium.sfs && ! echo "$o" | grep -q "disc's copy is used" && grep -q '^next=a' $T/sys/state; echo $?) "$r $o"
ok "8. ...every file of the new system is in its slot" $(for f in vmlinuz base.sfs chromium.sfs desktop.sfs modules.sfs; do [ -s $T/sys/a/$f ] || exit 1; done; echo 0)
ok "9. the updater checks a slot is complete before scheduling it" $(grep -q 'missing from the new system; it is not used' $I/sys/rudolf-update.c; echo $?)
# 1.5.8: running a stick's rescue system (slot r): an update goes into slot a or b, never into r, and r stays as it was
fresh r "$OWN" "rudolf.slot=r"; mkdir -p $T/sys/r; printf 'serial 2026110500\nversion 1.5.6\n' > $T/sys/r/RELEASE; echo 1 > $T/sys/r/complete
for f in base chromium desktop modules; do head -c 300000 /dev/urandom > $T/sys/r/$f.sfs; done; cp $T/media/boot/vmlinuz $T/sys/r/vmlinuz
printf 'good=\nprev=\nnext=\ntries=0\nfailed=\n' > $T/sys/state; H=$(cat $T/sys/r/*.sfs | sha256sum)
o=$(run download 2>&1); r=$?
ok "10. running the rescue system: the update goes into slot a, the rescue system untouched" $([ $r = 0 ] && grep -q '^next=a' $T/sys/state && [ -e $T/sys/a/complete ] && [ "$(cat $T/sys/r/*.sfs | sha256sum)" = "$H" ] && grep -q 'version 1.5.6' $T/sys/r/RELEASE; echo $?) "$r $o"
s2=$(run status)
ok "11. ...status says it runs the rescue system, and lists it" $(echo "$s2" | grep -q '"booted":"r"' && echo "$s2" | grep -q '"r":{"complete":true,"serial":2026110500'; echo $?) "$s2"
echo "$P/$((P+F)) passed"; [ $F = 0 ]
