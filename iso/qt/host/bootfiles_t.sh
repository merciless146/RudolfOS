#!/bin/sh
# bootfiles.sh (1.2.8) on the host, with busybox: the installed system made whole before the boot area is replaced
# (complete_slots), the fit check, and writing that keeps the RUDOLFDATA partition entry. sh bootfiles_t.sh
F=$(cd "$(dirname "$0")/../../sys/root/usr/lib/rudolf" && pwd)/bootfiles.sh
T=$(mktemp -d); cd "$T" || exit 1
P=0; N=0; ok() { N=$((N + 1)); if [ "$1" = 0 ]; then P=$((P + 1)); echo "PASS $2"; else echo "FAIL $2 ${3:-}"; fi; }
BF_LIB=1 . "$F"
# an old disc image's layers (OLD), a good slot a that takes 'chromium' from it, and a previous slot b whose 'desktop'
# does not match the old image (it cannot start without it any more)
mkdir -p old/rudolf sys/a sys/b
for l in base chromium desktop modules; do head -c 4096 /dev/urandom > old/rudolf/$l.sfs; done
sha() { sha256sum "$1" | cut -c1-64; }
for s in a b; do echo k > sys/$s/vmlinuz; echo 1 > sys/$s/complete; done
for l in base desktop modules; do head -c 4096 /dev/urandom > sys/a/$l.sfs; done
for l in base chromium modules; do head -c 4096 /dev/urandom > sys/b/$l.sfs; done
{ for l in base desktop modules; do printf 'L\t%s\t4096\t%s\tu\n' $l $(sha sys/a/$l.sfs); done; printf 'L\tchromium\t4096\t%s\tu\n' $(sha old/rudolf/chromium.sfs); } > sys/a/manifest.txt
{ printf 'L\tdesktop\t4096\t%s\tu\n' 0000000000000000000000000000000000000000000000000000000000000000; } > sys/b/manifest.txt
printf 'good=a\nprev=b\nnext=\ntries=0\nfailed=\n' > sys/state
BOOTONLY=1
out=$(complete_slots "$T/sys" "$T/old" 2>&1); st=$?
ok $st "complete_slots runs" "$out"
cmp -s sys/a/chromium.sfs old/rudolf/chromium.sfs; ok $? "the good slot gets the layer it took from the old image, checked against its list"
[ ! -e sys/b/desktop.sfs ] && grep -q '^prev=$' sys/state; ok $? "a previous slot that cannot be made whole is no longer offered (prev cleared)" "$(cat sys/state)"
echo "$out" | grep -q "no longer kept"; ok $? "...and says so"
# a good slot whose layer does not match: nothing changes, it stops
rm sys/a/chromium.sfs; printf 'good=a\nprev=\n' > sys/state; head -c 4096 /dev/urandom > old/rudolf/chromium.sfs
out=$( (complete_slots "$T/sys" "$T/old") 2>&1); st=$?
[ $st = 6 ] && [ ! -e sys/a/chromium.sfs ] && [ ! -e sys/a/chromium.sfs.new ]; ok $? "a layer that does not match the good slot's list stops it (exit 6), nothing left behind" "$st $out"
# a drive whose system is its own image (no installed update): boot files only would leave it without a system
printf 'good=disc\n' > sys/state; out=$( (complete_slots "$T/sys" "$T/old") 2>&1); st=$?
[ $st = 6 ] && echo "$out" | grep -q "no installed update"; ok $? "no installed update: refused for a boot image" "$st"
BOOTONLY=0; out=$( (complete_slots "$T/sys" "$T/old") 2>&1); ok $? "...but a whole image is fine" "$out"
# the fit: a disk with RUDOLFDATA at 446 MiB (as from the 1.2.0 disc), a 27 MiB boot image and a 522 MiB disc image
truncate -s 600M disk.img
python3 - <<'PY'
import struct
d=bytearray(512); d[478:494]=bytes([0,0xfe,0xff,0xff,0x83,0xfe,0xff,0xff])+struct.pack('<II',446*2048,100000); d[510:512]=b'\x55\xaa'
open('disk.img','r+b').write(d)
PY
IVER=1.2.8; BYTES=$((27 * 1048576)); out=$( (check_fit disk.img) 2>&1); ok $? "a 27 MiB boot image fits in front of RUDOLFDATA at 446 MiB" "$out"
BYTES=$((522 * 1048576)); out=$( (check_fit disk.img) 2>&1); st=$?
[ $st = 5 ] && echo "$out" | grep -q "rudolf-os-1.2.8-boot.iso"; ok $? "a 522 MiB disc image does not, and the boot image is named" "$out"
printf '\0' | dd of=disk.img bs=1 seek=482 conv=notrunc 2>/dev/null; out=$( (check_fit disk.img) 2>&1); [ $? = 5 ]; ok $? "a disk without the RUDOLFDATA entry is refused" "$out"
# writing keeps the entry and reads back the copy
python3 - <<'PY'
import struct,os
d=bytearray(open('disk.img','rb').read(512)); d[482]=0x83; open('disk.img','r+b').write(d)
img=bytearray(os.urandom(2*1048576)); img[478:494]=b'\0'*16; open('img.iso','wb').write(img)
PY
BLOCKS=1024; BYTES=$((BLOCKS * 2048)); out=$(write_area disk.img img.iso 2>&1)
echo "$out" | grep -q "The copy matches" && [ "$(dd if=disk.img bs=1 skip=486 count=4 2>/dev/null | od -An -tu4 | tr -d ' ')" = $((446 * 2048)) ] && cmp -s -n 478 disk.img img.iso; ok $? "write_area: the image written, RUDOLFDATA's entry kept, the copy compared" "$out"
cd /; rm -rf "$T"
echo "$P/$N passed"; [ $P = $N ]
