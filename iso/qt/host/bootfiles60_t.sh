#!/bin/sh
# 1.6.0 (docs/plan-1.6.0.md "Before stable" 15): bootfiles.sh puts start-up files INTO a drive's existing EFI system
# partition, file by file, and never writes its area, partition table or disk signature. Drives made from real boot
# images (Install here's layout, as loop devices with partitions): 1.6.0's with recovery.efi changed; 1.4.1's, whose
# EFI system partition (15 MiB) has no room for 1.6.0's files beside its own (the owner's Vivobook).
#   BOOTISO=1.6.0's boot.iso OLDBOOT=rudolf-os-1.4.1-boot.iso sh bootfiles60_t.sh   (root; vfat; mtools; sbsigntool)
HERE=$(cd "$(dirname "$0")" && pwd); BF=$HERE/../../sys/root/usr/lib/rudolf/bootfiles.sh; CERT=$HERE/../../boot/sb/rudolf-sb.crt
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -s "$BOOTISO" ] && [ -s "$OLDBOOT" ] || { echo "SKIP: BOOTISO and OLDBOOT (boot images) are needed"; exit 0; }
grep -qw vfat /proc/filesystems || modprobe vfat 2>/dev/null; grep -qw vfat /proc/filesystems || { echo "SKIP: no vfat in this kernel"; exit 0; }
command -v sbverify >/dev/null && command -v mcopy >/dev/null || { echo "SKIP: sbverify and mtools are needed"; exit 0; }
W=$(mktemp -d); L=""; trap 'umount $W/m 2>/dev/null; [ -n "$L" ] && losetup -d $L 2>/dev/null; rm -rf "$W"' EXIT; P=0; F=0
ok() { if [ "$2" = 1 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 - $3"; fi; }
# a drive with IMAGE in front (Install here's layout), as a loop device with its partitions
drive() { cp "$1" $W/d.img; truncate -s 200M $W/d.img
	python3 - $W/d.img <<'X'
import sys,struct
f=open(sys.argv[1],'r+b'); f.seek(440); f.write(bytes.fromhex('6a5ba9d4')); f.seek(446); f.write(bytes(16))
f.seek(478); f.write(bytes([0,0xfe,0xff,0xff,0x83,0xfe,0xff,0xff])+struct.pack('<II',262144,131072))
X
	[ -n "$L" ] && losetup -d $L; L=$(losetup -f --show -P $W/d.img); udevadm settle 2>/dev/null; i=0; while [ ! -b ${L}p2 ] && [ $i -lt 20 ]; do i=$((i+1)); timeout 0.2 tail -f /dev/null; done; }
# the start-up files of a boot image, into a folder (mtools: no mount needed)
efi_off() { python3 -c "
import struct,sys
f=open(sys.argv[1],'rb'); f.seek(17*2048); bv=f.read(2048); f.seek(struct.unpack('<I',bv[0x47:0x4b])[0]*2048); c=f.read(2048)
print([struct.unpack('<I',c[32*i+8:32*i+12])[0]*2048 for i in range(2,64) if c[32*i]==0x88][0])" "$1"; }
mkdir -p $W/new $W/m; mcopy -s -n -i "$BOOTISO@@$(efi_off "$BOOTISO")" ::/EFI ::/loader $W/new/ 2>/dev/null
ok "0. 1.6.0's start-up files read from its boot image" "$([ -s $W/new/EFI/rudolf/recovery.efi ] && [ -s $W/new/EFI/BOOT/BOOTX64.EFI ] && echo 1)" "$(find $W/new | head)"
area() { dd if=$L bs=1M count=127 2>/dev/null | sha256sum | cut -c1-64; }
fs_hash() { dd if=${L}p2 bs=1M 2>/dev/null | sha256sum | cut -c1-64; }
run() { (export BF_LIB=1 BF_NOHEALTH=1 BF_SBCERT=$CERT BF_TMP=$W/t BF_KEEP=$W/keep; . $BF; mkdir -p $W/t; files_into $L $W/new); }
# ---- 1. 1.6.0's drive, its recovery.efi changed (one byte)
drive "$BOOTISO"
mount -t vfat ${L}p2 $W/m && f=$W/m/EFI/rudolf/recovery.efi && s=$(stat -c %s $f) && printf 'X' | dd of=$f bs=1 seek=$((s / 2)) conv=notrunc 2>/dev/null; sync; umount $W/m
MBR0=$(dd if=$L bs=512 count=1 2>/dev/null | od -An -tx1 | tr -d ' \n'); ET0=$(dd if=$L bs=2048 skip=17 count=3 2>/dev/null | sha256sum)
o=$(run 2>&1; echo "rc=$?")
mount -t vfat -o ro ${L}p2 $W/m; R=$(cmp $W/new/EFI/rudolf/recovery.efi $W/m/EFI/rudolf/recovery.efi && echo same); N=$(find $W/m -name '*.new' -o -name '*.old'); umount $W/m
ok "1. a changed recovery.efi put back in the EFI system partition (only it): the image's again; the old one kept on RUDOLFDATA (os/esp-old) until the next start" \
	"$(echo "$o" | grep -q 'rc=0' && [ "$R" = same ] && [ -s $W/keep/EFI/rudolf/recovery.efi ] && [ ! -e $W/keep/EFI/rudolf/rudolf.efi ] && [ -z "$N" ] && echo 1)" "$o $(find $W/keep)"
ok "...the partition table, the disk signature and the El Torito catalogue unchanged" "$([ "$(dd if=$L bs=512 count=1 2>/dev/null | od -An -tx1 | tr -d ' \n')" = "$MBR0" ] && [ "$(dd if=$L bs=2048 skip=17 count=3 2>/dev/null | sha256sum)" = "$ET0" ] && echo 1)" ""
o=$(run 2>&1; echo "rc=$?")
ok "2. again: nothing to change" "$(echo "$o" | grep -q 'already the image' && echo "$o" | grep -q 'rc=0' && echo 1)" "$o"
# ---- 3. rudolf.efi changed too: no room beside it (the partition has 2 MiB free), so Recovery mode's file is taken out
#      for room, kept, and written last; first with a stand-in sbverify that refuses the written copies: all put back
rm -rf $W/keep; mount -t vfat ${L}p2 $W/m && printf 'Y' | dd of=$W/m/EFI/rudolf/rudolf.efi bs=1 seek=4096 conv=notrunc 2>/dev/null && cp $W/m/EFI/rudolf/rudolf.efi $W/r0; sync; umount $W/m
printf '#!/bin/sh\nfor a; do case "$a" in *.new) exit 1;; esac; done\nexec sbverify "$@"\n' > $W/sbv; chmod 755 $W/sbv
o=$(SBVERIFY=$W/sbv run 2>&1; echo "rc=$?")
mount -t vfat -o ro ${L}p2 $W/m; N=$(find $W/m -name '*.new'); R=$(cmp $W/r0 $W/m/EFI/rudolf/rudolf.efi && cmp $W/new/EFI/rudolf/recovery.efi $W/m/EFI/rudolf/recovery.efi && echo same); umount $W/m
ok "3. a new file that does not check after it is written: the files as they were (recovery.efi, taken out for room, back), no NAME.new left, said" "$(echo "$o" | grep -q 'were put back as they were' && echo "$o" | grep -q 'rc=9' && [ -z "$N" ] && [ "$R" = same ] && echo 1)" "$o"
rm -rf $W/keep; o=$(run 2>&1; echo "rc=$?")
mount -t vfat -o ro ${L}p2 $W/m; R=$(cmp $W/new/EFI/rudolf/rudolf.efi $W/m/EFI/rudolf/rudolf.efi && cmp $W/new/EFI/rudolf/recovery.efi $W/m/EFI/rudolf/recovery.efi && [ -s $W/m/EFI/BOOT/BOOTX64.EFI ] && echo same); umount $W/m
ok "3b. ...and with the real check: rudolf.efi replaced (recovery.efi taken out for room and written last), both the image's; the old ones kept" \
	"$(echo "$o" | grep -q 'rc=0' && echo "$o" | grep -q 'taken out for room' && [ "$R" = same ] && cmp -s $W/r0 $W/keep/EFI/rudolf/rudolf.efi && echo 1)" "$o"
# ---- 4. 1.4.1's drive (the Vivobook's): no room beside its files
drive "$OLDBOOT"; A0=$(area)
o=$(run 2>&1; echo "rc=$?")
ok "4. 1.4.1's drive: its EFI system partition (15 MiB) has no room for 1.6.0's files: refused, said, nothing written" \
	"$(echo "$o" | grep -q 'has no room' && echo "$o" | grep -q 'rc=10' && [ "$(area)" = "$A0" ] && echo 1)" "$o"
# ---- 5. the scripts that wrote the whole area
ok "5. bootfiles.sh stage and write, and the start-up's staged write, never write the area (no dd over the drive outside the expert rebuild)" \
	"$(! sed -n '/^stage)/,/^esac/p' $BF | grep -q 'write_area\|dd if=' && ! grep -q 'dd if=\$BI/boot.iso of=\$BD' $HERE/../../initramfs/init && sed -n '/^cmd_area()/,/^}/p' $BF | grep -q 'area.old' && echo 1)" ""
echo "$P/$((P+F)) passed"; [ $F = 0 ]
