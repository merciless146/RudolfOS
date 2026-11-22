#!/bin/sh
# 1.6.2 (docs/plan-1.6.2.md Part 27): Bring your files (bringfiles.sh) on disk images made here: an old disk with an EFI
# partition and Windows on NTFS (a Users tree with two people, Public and Default, OneDrive, AppData, programs), a
# dirty and a hibernated NTFS, an ext4 Linux with /home and XDG folders, a "Mac" (APFS: the message only), a BitLocker
# volume (cryptsetup's own test image, if it can be downloaded: the recovery key checked); the USB drives FAT32 images.
# Found and sized; nothing on the old disk written (its loop devices are read-only, its images' SHA-256 the same);
# copied and checked; the install's guard refusing until the copy (or "erase without keeping"); a cancelled copy
# marking nothing; a drive that fills up and the next one continuing; FAT32's 4 GB limit; the copy brought into an
# account's folder as the account's user (names that clash kept side by side, a file changed on the drive refused).
# Needs root (loop devices, mount), ntfs-3g (the old disk's NTFS through FUSE here: the kernel's ntfs3 in Rudolf OS),
# mkfs.vfat. RUDOLF_TEST_PATH=DIR runs the script with BusyBox's tools (as Rudolf OS has them).
#   sh bringfiles62_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); R=$(cd "$HERE/../../.." && pwd); BF=$R/iso/sys/root/usr/lib/rudolf/bringfiles.sh
T=$(mktemp -d); chmod 755 "$T"; P=0; F=0; LOOPS=
cleanup() { for m in $(awk -v t="$T/" 'index($2,t)==1 {print $2}' /proc/mounts | sort -r); do umount "$m" 2>/dev/null || umount -l "$m" 2>/dev/null; done
	for l in $LOOPS; do losetup -d "$l" 2>/dev/null; done; rm -rf "$T"; }
trap cleanup EXIT; [ -n "$KEEP" ] && trap "" EXIT
ok() { if [ "$2" = 1 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 ($3)"; fi; }
for x in mkntfs ntfs-3g mkfs.vfat mkfs.ext4 losetup python3; do command -v $x >/dev/null || { echo "FAIL $x is missing (apt-get install ntfs-3g dosfstools e2fsprogs)"; exit 1; }; done
export RUDOLF_BRING_STATE=$T/state RUDOLF_BRING_SYS=$T/sys RUDOLF_BRING_DEV=$T/dev RUDOLF_BRING_MEDIA=$T/media RUDOLF_BRING_NTFS=ntfs-3g
mkdir -p $T/sys/block $T/sys/class/block $T/dev $T/media $T/img
J() { python3 -c "import json,sys; d=json.loads(open(sys.argv[1]).read().split('RESULT ',1)[-1]); print(eval(sys.argv[2]))" "$@" 2>&1; }
# a fake disk NAME with partitions (image files): sysfs entries and device links to read-only loop devices
disk() { n=$1; shift; mkdir -p $T/sys/block/$n; echo 2097152 > $T/sys/block/$n/size; head -c 1048576 /dev/urandom > $T/img/$n.hdr
	l=$(losetup -fr --show $T/img/$n.hdr); LOOPS="$LOOPS $l"; ln -sf $l $T/dev/$n; i=1
	for im in "$@"; do p=${n}p$i; mkdir -p $T/sys/block/$n/$p $T/sys/class/block/$p; echo $i > $T/sys/block/$n/$p/partition
		l=$(losetup -fr --show "$im"); LOOPS="$LOOPS $l"; ln -sf $l $T/dev/$p; i=$((i + 1)); done; }
usb() { n=$1; truncate -s ${2}M $T/img/$n.img; mkfs.ext4 -q -F -L USB$n -m 0 $T/img/$n.img   # (the drives ext4 here: this kernel has no FAT; FAT32's limit is tested with RUDOLF_BRING_TESTFS)
	mkdir -p $T/media/$n; mount -o loop,rw $T/img/$n.img $T/media/$n; }
mk() { # mk IMG: an NTFS image, its tree made by the function named $2 (in the image mounted rw at $T/w)
	truncate -s ${3:-96}M "$1"; mkntfs -F -Q -q -L Windows "$1" >/dev/null 2>&1; mkdir -p $T/w; ntfs-3g "$1" $T/w || return 1; ( cd $T/w && $2 ); umount $T/w; }
win_tree() {
	mkdir -p Windows/System32 "Program Files/App" Users/Ann/Documents/Taxes Users/Ann/Pictures/Trip Users/Ann/Desktop Users/Ann/AppData/Local/Temp \
		Users/Ann/OneDrive/Documents Users/Ann/OneDrive/Notes "Users/Ann/Saved Games" Users/Ann/Music Users/Bob/Videos Users/Public/Documents Users/Default/Documents '$Recycle.Bin'
	echo k > Windows/System32/kernel32.dll; echo a > "Program Files/App/app.exe"; echo t > Users/Ann/AppData/Local/Temp/x.txt
	echo letter > "Users/Ann/Documents/Letter to Mary.docx"; echo taxes > Users/Ann/Documents/Taxes/return.pdf; echo i > Users/Ann/Documents/setup.exe
	echo d > Users/Ann/Pictures/desktop.ini; for i in 1 2 3 4 5; do head -c 300000 /dev/urandom > Users/Ann/Pictures/Trip/IMG_000$i.JPG; done
	echo note > Users/Ann/Desktop/todo.txt; echo od > Users/Ann/OneDrive/Documents/budget.xlsx; echo n > Users/Ann/OneDrive/Notes/n.txt
	echo s > "Users/Ann/Saved Games/save1.sav"; echo reg > Users/Ann/NTUSER.DAT; echo song > Users/Ann/Music/song.mp3
	head -c 900000 /dev/urandom > Users/Bob/Videos/birthday.mp4; echo p > Users/Public/Documents/shared.txt; echo x > Users/Default/Documents/default.txt
	head -c 64 /dev/zero > hiberfil.sys; echo pf > pagefile.sys; }
win_hib() { win_tree; printf 'HIBR' | dd of=hiberfil.sys conv=notrunc 2>/dev/null; }
mk $T/img/win.img win_tree || { echo "FAIL ntfs-3g could not mount the NTFS image here"; exit 1; }
cp $T/img/win.img $T/img/dirty.img; mk $T/img/hib.img win_hib
# the dirty flag set on $Volume (MFT record 3, $VOLUME_INFORMATION's flags), as Windows leaves it when not shut down
python3 - "$T/img/dirty.img" <<'E'
import sys, struct
f = open(sys.argv[1], 'r+b'); b = f.read(512)
bps, spc = struct.unpack_from('<HB', b, 11); cl = bps * spc; mft = struct.unpack_from('<q', b, 48)[0]; c = struct.unpack_from('<b', b, 64)[0]
rs = 2 ** -c if c < 0 else c * cl; off = mft * cl + 3 * rs; f.seek(off); r = bytearray(f.read(rs))
uo, uc = struct.unpack_from('<HH', r, 4); usn = r[uo:uo + 2]
for i in range(1, uc): r[i * 512 - 2:i * 512] = r[uo + 2 * i:uo + 2 * i + 2]
a = struct.unpack_from('<H', r, 20)[0]
while True:
    t, l = struct.unpack_from('<II', r, a)
    if t == 0x70: co = struct.unpack_from('<H', r, a + 20)[0]; fl = struct.unpack_from('<H', r, a + co + 10)[0]; struct.pack_into('<H', r, a + co + 10, fl | 1); break
    a += l
for i in range(1, uc): r[uo + 2 * i:uo + 2 * i + 2] = r[i * 512 - 2:i * 512]; r[i * 512 - 2:i * 512] = usn
f.seek(off); f.write(r)
mm = struct.unpack_from('<q', b, 56)[0]; f.seek(mm * cl + 3 * rs); f.write(r)   # ($MFTMirr has the record too)
E
truncate -s 32M $T/img/efi.img; mkfs.vfat -n EFI $T/img/efi.img >/dev/null
# Linux: ext4 with /etc/os-release and /home/carol (German XDG folder names), a cache and a link
mkdir -p $T/lin/etc $T/lin/home/carol/Dokumente $T/lin/home/carol/Bilder $T/lin/home/carol/.config $T/lin/home/carol/.cache $T/lin/home/carol/Projekte $T/lin/home/carol/Desktop
echo 'PRETTY_NAME="Debian GNU/Linux 12 (bookworm)"' > $T/lin/etc/os-release
printf 'XDG_DOCUMENTS_DIR="$HOME/Dokumente"\nXDG_PICTURES_DIR="$HOME/Bilder"\n' > $T/lin/home/carol/.config/user-dirs.dirs
echo brief > $T/lin/home/carol/Dokumente/brief.odt; head -c 200000 /dev/urandom > $T/lin/home/carol/Bilder/katze.png; echo c > $T/lin/home/carol/.cache/c
echo code > $T/lin/home/carol/Projekte/main.c; ln -s /etc $T/lin/home/carol/Dokumente/etc-link
truncate -s 32M $T/img/lin.img; mkfs.ext4 -q -F -d $T/lin $T/img/lin.img
# a Mac: an APFS container (its superblock's magic is all the scan reads)
truncate -s 8M $T/img/mac.img; printf 'NXSB' | dd of=$T/img/mac.img bs=1 seek=32 conv=notrunc 2>/dev/null
for i in win dirty hib lin; do sha256sum $T/img/$i.img | cut -c1-64 > $T/img/$i.sum0; done
disk old0 $T/img/efi.img $T/img/win.img; disk dty0 $T/img/dirty.img; disk hib0 $T/img/hib.img; disk lin0 $T/img/lin.img; disk mac0 $T/img/mac.img
BFX() { sh $BF "$@"; }

# ---- 1. what is found
BFX scan old0 > $T/s1 2>&1; r=$?
ok "1. the old disk scanned (Windows on its second partition)" "$( [ $r = 0 ] && [ "$(J $T/s1 "d['found'] and d['systems'][0]['os']+'/'+d['systems'][0]['part']+'/'+d['systems'][0]['state']")" = windows/old0p2/ok ] && echo 1)" "rc $r $(tail -3 $T/s1)"
ok "2. its people: Ann, Bob and Public; not Default" "$( [ "$(J $T/s1 "','.join(sorted(p['name'] for p in d['systems'][0]['people']))")" = Ann,Bob,Public ] && echo 1)" "$(J $T/s1 "d['systems'][0]['people']")"
AF=$(J $T/s1 "','.join(f['k']+':'+str(f['files']) for p in d['systems'][0]['people'] if p['name']=='Ann' for f in p['folders'])")
ok "3. Ann's folders, sized: Desktop 1, Documents 3 (with OneDrive's), Pictures 5 (no desktop.ini), Music 1, Other 2 (Saved Games, OneDrive's Notes; no AppData, NTUSER.DAT)" "$( [ "$AF" = "Desktop:1,Documents:3,Pictures:5,Music:1,Other:2" ] && echo 1)" "$AF"
ok "4. the sizes in bytes (Pictures 1,500,000)" "$( [ "$(J $T/s1 "[f['bytes'] for p in d['systems'][0]['people'] if p['name']=='Ann' for f in p['folders'] if f['k']=='Pictures'][0]")" = 1500000 ] && echo 1)" "$(J $T/s1 "d['total']")"
ok "5. mounted read-only, without programs, device files or set-user-ID" "$(grep " $T/state/m/old0p2 " /proc/mounts | grep -q 'ro,nosuid,nodev,noexec\|ro,nosuid,nodev,noexec' && echo 1)" "$(grep $T/state/m /proc/mounts)"
BFX scan dty0 > $T/s2 2>&1; BFX scan hib0 > $T/s3 2>&1
ok "6. a dirty NTFS (not shut down): found, read read-only, said" "$( [ "$(J $T/s2 "d['systems'][0]['state']")" = dirty ] && echo 1)" "$(tail -2 $T/s2)"
ok "7. a hibernated Windows (or Fast Startup): said" "$( [ "$(J $T/s3 "d['systems'][0]['state']")" = hibernated ] && echo 1)" "$(tail -2 $T/s3)"
BFX scan lin0 > $T/s4 2>&1
ok "8. Linux: Debian's name, carol, her XDG folders (Dokumente as Documents, Bilder as Pictures), Projekte under Other, no .cache, no link" "$( [ "$(J $T/s4 "d['systems'][0]['os']+'|'+d['systems'][0]['name']+'|'+','.join(p['name']+':'+'+'.join(f['k']+str(f['files']) for f in p['folders']) for p in d['systems'][0]['people'])")" = 'linux|Debian GNU/Linux 12 (bookworm)|carol:Documents1+Pictures1+Other1' ] && echo 1)" "$(J $T/s4 "d['systems']")"
BFX scan mac0 > $T/s5 2>&1
ok "9. a Mac (APFS): found as a disk Rudolf OS cannot read (the page says how to copy on the Mac)" "$( [ "$(J $T/s5 "str(d['found'])+d['locked'][0]['kind']")" = Trueapfs ] && echo 1)" "$(tail -2 $T/s5)"

# ---- 2. the installation's guard, a cancelled copy, the copy, then the guard again
BFX guard /dev/old0 > $T/g1 2>&1; r=$?
ok "10. the install refused (12) before the files are kept or the person chose" "$( [ $r = 12 ] && grep -q 'Nothing was erased' $T/g1 && echo 1)" "rc $r $(cat $T/g1)"
usb usb0 64
ID_ANN=$(J $T/s1 "[p['id'] for p in d['systems'][0]['people'] if p['name']=='Ann'][0]"); ID_BOB=$(J $T/s1 "[p['id'] for p in d['systems'][0]['people'] if p['name']=='Bob'][0]")
DST="$T/media/usb0/Rudolf OS - Bring your files/Old computer 2026-11-22"
L() { printf '%s continue=%s\n%s\n' "$1" "$2" "$3" > $T/list; shift 3; for s in "$@"; do printf '%s\n' "$s" >> $T/list; done; }
TB=$(printf '\t')
L old0 0 "$DST" "$ID_ANN${TB}Documents" "$ID_ANN${TB}Pictures" "$ID_ANN${TB}Desktop" "$ID_ANN${TB}Music" "$ID_ANN${TB}Other" "$ID_BOB${TB}Videos"
# (cancelled: the stop file made while it copies; a copying file is never left)
( BFX copy $T/list > $T/c0 2>&1; echo $? > $T/c0.rc ) & CP=$!
for i in $(seq 1 200); do grep -q 'to copy into' $T/c0 2>/dev/null && break; timeout 0.05 tail -f /dev/null; done; BFX cancel >/dev/null; wait $CP
r=$(cat $T/c0.rc); BFX guard /dev/old0 > $T/g2 2>&1; g=$?
ok "11. a cancelled copy: stopped (130 or done before the stop), nothing marked: the install still refused" "$( { [ "$r" = 130 ] || [ "$r" = 0 -a -z "$(grep Cancelled $T/c0)" ]; } && { [ "$r" = 0 ] || [ $g = 12 ]; } && [ -z "$(find "$T/media/usb0" -name '*.part')" ] && echo 1)" "rc $r guard $g $(tail -2 $T/c0)"
rm -f $T/state/old0.copied
BFX scan old0 > /dev/null 2>&1; BFX copy $T/list > $T/c1 2>&1; r=$?
ok "12. copied and checked (read back from the drive): 0" "$( [ $r = 0 ] && grep -q '^Copied and checked: 13 files' $T/c1 && echo 1)" "rc $r $(tail -4 $T/c1)"
ok "13. the folders on the drive: Ann/Documents/Taxes/return.pdf, Ann/Documents/OneDrive/budget.xlsx, Ann/Other/Saved Games, Bob/Videos" "$( [ -f "$DST/Ann/Documents/Taxes/return.pdf" ] && [ -f "$DST/Ann/Documents/OneDrive/budget.xlsx" ] && [ -f "$DST/Ann/Other/Saved Games/save1.sav" ] && [ -f "$DST/Ann/Other/OneDrive/Notes/n.txt" ] && [ -f "$DST/Bob/Videos/birthday.mp4" ] && echo 1)" "$(cd "$DST" && find . -type f | tr '\n' ' ')"
ok "14. only personal files: no setup.exe, desktop.ini, AppData, NTUSER.DAT, Windows" "$( [ -z "$(cd "$DST" && find . -iname '*.exe' -o -iname desktop.ini -o -iname appdata -o -iname 'ntuser*' -o -iname windows)" ] && echo 1)" ""
ok "15. bring.sha256 lists each file with its SHA-256, and it matches" "$( [ "$(grep -c . "$DST/bring.sha256")" = 13 ] && (cd "$DST" && sed 's/^\([0-9a-f]*\)  [0-9]*  /\1  /' bring.sha256 | sha256sum -c --quiet) && grep -q "^person	Ann	Ann$" "$DST/bring.txt" && echo 1)" "$(head -3 "$DST/bring.sha256")"
ok "16. the progress lines for the page" "$(grep -q '^PROGRESS 13 13 ' $T/c1 && echo 1)" "$(grep PROGRESS $T/c1 | tail -1)"
BFX guard /dev/old0 > $T/g3 2>&1; r=$?
ok "17. the install allowed after the copy (0), the old disk closed (unmounted)" "$( [ $r = 0 ] && ! grep -q "$T/state/m/old0p2 " /proc/mounts && echo 1)" "rc $r $(cat $T/g3)"
# ---- 3. a drive that fills up, then the next one; FAT32's 4 GB
usb usb1 34; usb usb2 64; A=$(df -Pk $T/media/usb1 | awk 'NR==2 {print $4}'); dd if=/dev/zero of="$T/media/usb1/filler" bs=1K count=$((A - 2600)) 2>/dev/null
BFX scan old0 > /dev/null 2>&1
D1="$T/media/usb1/Rudolf OS - Bring your files/Old computer"; L old0 0 "$D1" "$ID_ANN${TB}Pictures" "$ID_BOB${TB}Videos"
BFX copy $T/list > $T/c2 2>&1; r=$?
n1=$(grep -c . "$D1/bring.sha256" 2>/dev/null)
BFX guard /dev/old0 > /dev/null 2>&1; g=$?
ok "18. the drive full: 28, what is on it checked ($n1 files), the install still refused" "$( [ $r = 28 ] && grep -q 'The drive is full' $T/c2 && [ "${n1:-0}" -gt 0 ] && [ $g = 12 ] && grep -q '^last 0$' "$D1/bring.txt" && echo 1)" "rc $r guard $g $(tail -3 $T/c2)"
BFX scan old0 > /dev/null 2>&1
D2="$T/media/usb2/Rudolf OS - Bring your files/Old computer"; L old0 1 "$D2" "$ID_ANN${TB}Pictures" "$ID_BOB${TB}Videos"
BFX copy $T/list > $T/c3 2>&1; r=$?; n2=$(grep -c . "$D2/bring.sha256" 2>/dev/null)
BFX guard /dev/old0 > /dev/null 2>&1; g=$?
ok "19. continued on the next drive: the rest ($n1 + $n2 = 6), then the install allowed" "$( [ $r = 0 ] && [ $((n1 + n2)) = 6 ] && [ $g = 0 ] && echo 1)" "rc $r guard $g $(tail -3 $T/c3)"
BFX scan old0 > /dev/null 2>&1; fat4g() { truncate -s 4300000000 Users/Bob/Videos/long.mp4; }
big_tree() { win_tree; rm -f hiberfil.sys; fat4g; }; mk $T/img/big.img big_tree; disk big0 $T/img/big.img
BFX scan big0 > $T/s6 2>&1; IDB=$(J $T/s6 "[p['id'] for p in d['systems'][0]['people'] if p['name']=='Bob'][0]")
L big0 0 "$T/media/usb2/Rudolf OS - Bring your files/Big" "$IDB${TB}Videos"; RUDOLF_BRING_TESTFS=vfat BFX copy $T/list > $T/c4 2>&1; r=$?
ok "20. FAT32 (said as one) and a file over 4 GB: said before copying (27), nothing copied" "$( [ $r = 27 ] && grep -q 'long.mp4' $T/c4 && [ ! -e "$T/media/usb2/Rudolf OS - Bring your files/Big/Bob" ] && echo 1)" "rc $r $(tail -3 $T/c4)"
BFX close big0
# ---- 4. "erase without keeping"; the destination checks
BFX scan lin0 > /dev/null 2>&1; BFX guard /dev/lin0 > /dev/null 2>&1; g1=$?; BFX decide lin0 erase > /dev/null; BFX guard /dev/lin0 > /dev/null 2>&1; g2=$?
ok "21. erase without keeping: refused before (12), allowed after the choice (0)" "$( [ $g1 = 12 ] && [ $g2 = 0 ] && echo 1)" "$g1 $g2"
BFX scan old0 > /dev/null 2>&1
L old0 0 "$T/media/usb0/elsewhere" "$ID_ANN${TB}Desktop"; BFX copy $T/list > $T/c5 2>&1; r1=$?
mkdir -p "$T/media/usb0/x"; ln -s /tmp "$T/media/usb0/Rudolf OS - Bring your files/lnk" 2>/dev/null
L old0 0 "$T/media/usb0/Rudolf OS - Bring your files/lnk" "$ID_ANN${TB}Desktop"; BFX copy $T/list > $T/c6 2>&1; r2=$?
L old0 0 "$DST" "$ID_ANN${TB}Desktop"; BFX copy $T/list > $T/c8 2>&1; r3=$?
mkdir -p "$T/media/usb0/Rudolf OS - Bring your files/Planted"; ln -s /etc "$T/media/usb0/Rudolf OS - Bring your files/Planted/Ann"
L old0 0 "$T/media/usb0/Rudolf OS - Bring your files/Planted" "$ID_ANN${TB}Desktop"; BFX copy $T/list > $T/c9 2>&1; r4=$?
L old0 0 "$DST-2" "p999${TB}Desktop" "$ID_ANN${TB}../../etc"; BFX copy $T/list > $T/c7 2>&1
ok "22. only into the copy's folder on a drive, never through a link (nor one planted inside), never over an earlier copy; a person or folder the scan did not find is not copied" "$( [ $r1 = 2 ] && [ $r2 = 2 ] && [ $r3 = 2 ] && grep -q 'already holds a copy' $T/c8 && [ $r4 = 2 ] && [ ! -e /etc/Desktop ] && grep -q 'Not a person: p999' $T/c7 && grep -q 'Not a folder' $T/c7 && echo 1)" "$r1 $r2 $(cat $T/c6 | tail -1) $(head -3 $T/c7)"
BFX close old0; BFX close dty0; BFX close hib0; BFX close lin0
SAME=1; for i in win dirty hib lin; do [ "$(sha256sum $T/img/$i.img | cut -c1-64)" = "$(cat $T/img/$i.sum0)" ] || SAME=0; done
ok "23. nothing on the old disks changed (their images' SHA-256 as before)" "$SAME" ""
# ---- 5. BitLocker (cryptsetup's own test image: its recovery key and password)
if command -v cryptsetup >/dev/null && [ -n "${BITLK_IMAGES:-}" ] && [ -f "$BITLK_IMAGES/bitlk-aes-xts-128.img" ]; then
	disk blk0 "$BITLK_IMAGES/bitlk-aes-xts-128.img"; BFX scan blk0 > $T/s7 2>&1
	ok "24. BitLocker: found as locked" "$( [ "$(J $T/s7 "d['locked'][0]['kind']+'/'+d['locked'][0]['part']")" = bitlocker/blk0p1 ] && echo 1)" "$(tail -2 $T/s7)"
	echo 111111-222222 | RUDOLF_BRING_TESTKEY=1 BFX unlock blk0p1 > $T/u1 2>&1; r1=$?
	echo '235818 357951 253979 013365 241120 245575 342914 591910' | RUDOLF_BRING_TESTKEY=1 BFX unlock blk0p1 > $T/u2 2>&1; r2=$?
	echo '235818357951253979013365241120245575342914591910' | RUDOLF_BRING_TESTKEY=1 BFX unlock blk0p1 > /dev/null 2>&1; r3=$?
	ok "25. the recovery key: a wrong one refused (3), the right one opens it (typed with spaces, or without dashes)" "$( [ $r1 = 3 ] && [ $r2 = 0 ] && [ $r3 = 0 ] && echo 1)" "$r1 $r2 $r3 $(cat $T/u1 $T/u2)"
	ok "26. the key is stored nowhere (not in the state folder)" "$( ! grep -rqs 235818 $T/state && echo 1)" ""
else echo "NOTE BitLocker not tested here (cryptsetup or BITLK_IMAGES missing)"; fi
# ---- 6. after installing: the copies found, one person's files brought into an account's folder (as its user)
BFX found > $T/f1 2>&1
ok "27. the copies on the plugged-in drives found: who and how much" "$(python3 -c "
import json,sys; d=json.load(open('$T/f1')); s=[x for x in d['sets'] if x['dir'].endswith('Old computer 2026-11-22')][0]
print(1 if s['system']=='windows' and s['last'] and sorted((p['name'],p['files']) for p in s['people'])==[('Ann',12),('Bob',1)] else 0)" 2>&1)" "$(cat $T/f1)"
H=$T/home; mkdir -p $H/Documents $H/Pictures $H/Desktop $H/Music $H/Videos $H/Downloads $H/.rudolf
echo "my own letter" > "$H/Documents/Letter to Mary.docx"; cp "$DST/Ann/Desktop/todo.txt" $H/Desktop/todo.txt; chown -R 4321:4321 $H
U="setpriv --reuid=4321 --regid=4321 --clear-groups"
$U env RUDOLF_BRING_MEDIA=$T/media RUDOLF_BRING_STATE=$T/state sh $BF import "$DST" Ann $H > $T/i1 2>&1; r=$?
ok "28. brought in as the account's user: Documents, Pictures, Desktop, Music; Other into Documents/From the old computer" "$( [ $r = 0 ] && [ -f $H/Documents/Taxes/return.pdf ] && [ -f $H/Pictures/Trip/IMG_0003.JPG ] && [ -f $H/Music/song.mp3 ] && [ -f "$H/Documents/From the old computer/Saved Games/save1.sav" ] && [ "$(stat -c %u $H/Documents/Taxes/return.pdf)" = 4321 ] && echo 1)" "rc $r $(tail -3 $T/i1)"
ok "29. a name that clashes kept side by side; the same file already there not copied twice" "$( [ "$(cat "$H/Documents/Letter to Mary.docx")" = "my own letter" ] && [ -f "$H/Documents/Letter to Mary (from the old computer).docx" ] && [ ! -e "$H/Desktop/todo (from the old computer).txt" ] && grep -q '1 already there' $T/i1 && echo 1)" "$(ls $H/Documents $H/Desktop | tr '\n' ' ')"
ok "30. the list of what came in, for the antivirus (.rudolf/bring-imported.txt)" "$( [ "$(grep -c . $H/.rudolf/bring-imported.txt)" = 11 ] && grep -qx 'Pictures/Trip/IMG_0001.JPG' $H/.rudolf/bring-imported.txt && echo 1)" "$(cat $H/.rudolf/bring-imported.txt | head -3)"
echo changed >> "$DST/Bob/Videos/birthday.mp4"
$U env RUDOLF_BRING_MEDIA=$T/media RUDOLF_BRING_STATE=$T/state sh $BF import "$DST" Bob $H > $T/i2 2>&1; r=$?
ok "31. a file changed on the drive since it was checked: not brought in, said" "$( [ $r = 3 ] && grep -q 'birthday.mp4 (changed on the drive' $T/i2 && [ ! -e $H/Videos/birthday.mp4 ] && echo 1)" "rc $r $(tail -2 $T/i2)"
$U env RUDOLF_BRING_MEDIA=$T/media sh $BF import /etc Ann $H > $T/i3 2>&1; r1=$?; $U env RUDOLF_BRING_MEDIA=$T/media sh $BF import "$DST" .. $H > /dev/null 2>&1; r2=$?
ok "32. only a copy under the drives, only a person of the copy" "$( [ $r1 = 2 ] && [ $r2 = 2 ] && echo 1)" "$r1 $r2"
# ---- 7. the same "Only personal files" as Copy to another drive
DC=$R/iso/sys/root/usr/lib/rudolf/drivecopy.sh
ok "33. the lists of what is left out are drivecopy.sh's" "$( [ "$(grep '^PF=' $BF)" = "$(grep '^PF=' $DC)" ] && [ "$(grep '^PX=' $BF)" = "$(grep '^PX=' $DC)" ] && echo 1)" ""
echo "$P passed, $F failed"; [ $F = 0 ]
