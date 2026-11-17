#!/bin/sh
# New boot files for a drive Rudolf OS is installed on, keeping its files (1.2.8). GPL-2.0-or-later.
#   bootfiles.sh stage IMAGE       on the drive Rudolf OS runs from: checks IMAGE and keeps a copy on RUDOLFDATA; the
#                                  start-up writes it before anything else at the next start (initramfs/init)
#   bootfiles.sh write DISK [IMAGE] from another drive (a USB stick): writes IMAGE (empty: this Rudolf OS's own) to the
#                                  start of DISK, in front of its RUDOLFDATA partition, which is left as it is
# 1.6.0 (docs/plan-1.6.0.md "Before stable" 15): the files are replaced, never the drive's layout. On the owner's Vivobook
# the start-up's staged write put a new boot image over the whole area (its EFI image in another place, a new disk
# signature), and every firmware entry for the drive then named nothing: the computer opened its setup screen. Now
# stage and write copy the image's start-up files INTO the drive's existing EFI system partition (partition 2): each
# new file beside the old one (NAME.new), checked (its Secure Boot signature, read back), then renamed over it, the old
# one kept on RUDOLFDATA (os/esp-old) until the next start from the drive (a release's EFI system partition has a few
# MiB free: Recovery mode's file is taken out for room while another is written, then written last); the partition
# table, the disk signature and the area are never written. Then the firmware's entry for the drive is checked (rudolfd --boot-entry fix). Anything wrong: the
# old files are put back, and it says so.
#   bootfiles.sh files IMAGE [DISK]   the same, by name (stage and write are this now; IMAGE empty: this Rudolf OS's own)
#   bootfiles.sh check | rescue        the drive Rudolf OS keeps its files on: its partition table, its EFI system
#                                      partition and files, the El Torito entry the firmware finds them by, and the
#                                      firmware's entry for it, said; rescue also puts that entry right (Settings >
#                                      Update and recovery > Check this drive's start-up). Nothing else is written.
#   bootfiles.sh area IMAGE [DISK]     (experts: "Rebuild the start-up area") the old way, for a drive whose EFI system
#                                      partition is too small for the new files: the image over the area, keeping the
#                                      disk signature and RUDOLFDATA's entry; the old area kept on RUDOLFDATA and put
#                                      back unless the new one mounts, its files check and the firmware's entry is right
# BF_FORCE_POWER=1 (tests): without the check that the computer is on mains power or its battery above 50 %.
# A drive installed from a disc before 1.2.2 has boot files that cannot start with Secure Boot on, and updates never
# replace them. Install here would, but it erases RUDOLFDATA. This writes only the area in front of it.
# IMAGE: a Rudolf OS disc image, or its boot image (rudolf-os-X.Y.Z-boot.iso: the boot files without the system
# layers, small enough to fit in front of RUDOLFDATA on every drive Install here has made). With a boot image the
# system comes from the installed update (a slot on RUDOLFDATA), so that slot is made complete first: any layer it
# still takes from the old disc image is copied into it and checked against its signed release list.
# RUDOLF_IMG_SHA: the image's SHA-256 (from its .sha256), checked when given.
export PATH=/usr/sbin:/usr/bin:/sbin:/bin
die() { echo "$1"; exit "${2:-1}"; }
# the disk a partition is on: /dev/sda3 -> /dev/sda, /dev/nvme0n1p3 -> /dev/nvme0n1
disk_of() { case "$1" in /dev/nvme*p[0-9]*|/dev/mmcblk*p[0-9]*|/dev/loop*p[0-9]*) echo "${1%p[0-9]*}";; /dev/[sv]d[a-z][0-9]*) echo "${1%%[0-9]*}";; *) echo "$1";; esac; }
le32() { dd if="$1" bs=1 skip="$2" count=4 2>/dev/null | od -An -tu4 | tr -d ' \n'; }
part3() { case "$1" in *[0-9]) echo "${1}p3";; *) echo "${1}3";; esac; }
part1() { case "$1" in *[0-9]) echo "${1}p1";; *) echo "${1}1";; esac; }

# ---- the image: joined (if downloaded in parts), its checksum, a Rudolf OS disc signed by Rudolf OS, Secure Boot
# ready (1.2.2 or later), and its size. Sets SRC, BYTES, BLOCKS, BOOTONLY, IVER, ISERIAL.
check_image() {
	SRC=$1
	if [ -z "$SRC" ]; then
		SRC=$(cat /etc/rudolf/media); M=/live/media
		[ -b "$SRC" ] && mountpoint -q $M || die "This Rudolf OS's own image cannot be read." 7
	else
		# (1.6.0: or espcheck's download of the official boot image, /persist/os/esp-repair/boot.iso)
		case "$SRC" in *..*) die "Bad file name." 7;; /home/rudolf/*.iso|/home/rudolf/*.ISO|/persist/os/esp-repair/boot.iso) ;; *) die "The image must be a .iso file in your home folder." 7;; esac
		[ -f "$SRC" ] || /usr/lib/rudolf/joinparts.sh "$SRC" || exit 7
		[ -f "$SRC" ] || die "No such file: $SRC" 7
		if [ -n "$RUDOLF_IMG_SHA" ]; then
			echo "Checking $(basename "$SRC") against its .sha256 ..."
			[ "$(sha256sum "$SRC" | cut -c1-64)" = "$RUDOLF_IMG_SHA" ] || die "$(basename "$SRC") does not match its .sha256: download it again." 8
		fi
		M=/mnt/bfimg; mkdir -p $M; umount $M 2>/dev/null || true
		mount -t iso9660 -o ro,loop "$SRC" $M || die "That file is not a disc image." 7
	fi
	V=$(rudolf-update verify-manifest $M/rudolf/manifest.txt $M/rudolf/manifest.sig 2>&1); OK=$?
	[ -e $M/rudolf/id ] || OK=1
	IVER=$(sed -n 's/^version //p' $M/rudolf/release 2>/dev/null); ISERIAL=$(sed -n 's/^serial //p' $M/rudolf/release 2>/dev/null)
	BOOTONLY=0; [ -e $M/rudolf/bootonly ] && BOOTONLY=1
	SBOK=0; grep -qa "RUDOLF  CER" $M/efi/efiboot.img 2>/dev/null && grep -qa "GRUBX64 EFI" $M/efi/efiboot.img && SBOK=1
	[ "$M" = /mnt/bfimg ] && umount $M
	[ $OK = 0 ] || die "That file is not a Rudolf OS disc image signed by Rudolf OS ($V)." 7
	[ $SBOK = 1 ] || die "That image's boot files are older than Secure Boot support too (Rudolf OS 1.2.2 or later is needed)." 7
	BLOCKS=$(dd if="$SRC" bs=2048 skip=16 count=1 2>/dev/null | dd bs=1 skip=80 count=4 2>/dev/null | od -An -tu4 | tr -d ' ')
	BYTES=$((BLOCKS * 2048))
	echo "Image: Rudolf OS $IVER$([ $BOOTONLY = 1 ] && echo ', boot files only') ($((BYTES / 1048576)) MiB)."
}

# ---- the drive's RUDOLFDATA partition: the third entry of its partition table, as Install here makes it.
# Sets START (its first sector) and checks the image fits in front of it.
check_fit() {
	D=$1
	[ "$(dd if="$D" bs=1 skip=482 count=1 2>/dev/null | od -An -tx1 | tr -d ' ')" = 83 ] || die "$D was not installed by Rudolf OS's Install here (no RUDOLFDATA partition after the image): use Install here instead." 5
	START=$(le32 "$D" 486)
	[ "${START:-0}" -gt 2048 ] || die "$D's partition table is not as Install here makes it." 5
	[ $BYTES -le $((START * 512)) ] || die "This image ($((BYTES / 1048576)) MiB) does not fit in front of your files ($((START / 2048)) MiB there): use the boot image, rudolf-os-$IVER-boot.iso, from the same release." 5
}

# ---- an installed system that starts without the old image: its slots (good, then the previous one) get the
# layers they still take from the disc image (OLD: where it is mounted), each checked against the slot's signed list
complete_slots() {
	SYSD=$1; OLD=$2
	st() { sed -n "s/^$1=//p" $SYSD/state 2>/dev/null; }
	G=$(st good); P=$(st prev)
	case "$G" in a|b) ;; *) [ $BOOTONLY = 1 ] && die "This drive has no installed update to start from (its system is the one on its disc image), so boot files only would leave it without a system. Start it, install the latest update (Settings > Update and recovery), restart, and try again; or use a full disc image if it fits." 6; return 0;; esac
	NEED=0
	for S in $G $P; do
		case "$S" in a|b) ;; *) continue;; esac
		[ -s $SYSD/$S/vmlinuz ] && [ -e $SYSD/$S/complete ] || continue
		for L in base chromium desktop modules; do [ -s $SYSD/$S/$L.sfs ] || NEED=$((NEED + $(stat -c %s $OLD/rudolf/$L.sfs 2>/dev/null || echo 0))); done
	done
	FREE=$(($(df -k $SYSD | awk 'NR==2 {print $4}') * 1024))
	[ $NEED -lt $((FREE - 67108864)) ] || die "Not enough free space on RUDOLFDATA: $((NEED / 1048576)) MiB are needed to keep the installed system whole." 6
	for S in $G $P; do
		case "$S" in a|b) ;; *) continue;; esac
		[ -s $SYSD/$S/vmlinuz ] && [ -e $SYSD/$S/complete ] || continue
		for L in base chromium desktop modules; do
			[ -s $SYSD/$S/$L.sfs ] && continue
			W=$(awk -F'\t' -v l=$L '$1=="L" && $2==l {print $4}' $SYSD/$S/manifest.txt 2>/dev/null)
			echo "Slot $S: copying the '$L' layer it shares with the old disc image ..."
			if [ -n "$W" ] && [ -s $OLD/rudolf/$L.sfs ] && cp $OLD/rudolf/$L.sfs $SYSD/$S/$L.sfs.new && [ "$(sha256sum $SYSD/$S/$L.sfs.new | cut -c1-64)" = "$W" ]; then
				mv $SYSD/$S/$L.sfs.new $SYSD/$S/$L.sfs; sync
			else
				rm -f $SYSD/$S/$L.sfs.new
				[ "$S" = "$G" ] && die "The installed system's '$L' layer could not be copied from the old disc image (or does not match its release list). Nothing was changed on the boot area." 6
				# the previous system cannot start without the old image: it is no longer offered
				echo "Slot $S (the previous system) is no longer kept to go back to."
				sed -i "s/^prev=.*/prev=/" $SYSD/state; sync; break
			fi
		done
	done
}

# ---- writing: the image over the area in front of RUDOLFDATA, the partition table's RUDOLFDATA entry kept,
# read back and compared (the same as the start-up does for a staged image)
hex2bin() { for h in $(echo "$1" | sed 's/../& /g'); do printf "\\$(printf %03o 0x$h)"; done; }
expect_sha() { { dd if="$1" bs=478 count=1 2>/dev/null; hex2bin "$2"; dd if="$1" bs=2048 count=$3 2>/dev/null | tail -c +495; } | sha256sum | cut -c1-64; }
write_area() {
	D=$1; S=$2
	E=$(dd if="$D" bs=1 skip=478 count=16 2>/dev/null | od -An -tx1 | tr -d ' \n')
	WANT=$(expect_sha "$S" "$E" $BLOCKS)
	for try in 1 2; do
		echo "Writing the new boot files to $D ($((BYTES / 1048576)) MiB) ..."
		dd if="$S" of="$D" bs=2048 count=$BLOCKS conv=fsync 2>&1 | tail -1
		hex2bin "$E" | dd of="$D" bs=1 seek=478 conv=notrunc,fsync 2>/dev/null
		sync; blockdev --flushbufs "$D" 2>/dev/null; echo 3 > /proc/sys/vm/drop_caches 2>/dev/null
		GOT=$(dd if="$D" bs=2048 count=$BLOCKS 2>/dev/null | sha256sum | cut -c1-64)
		[ "$GOT" = "$WANT" ] && { echo "The copy matches."; . "${RUDOLF_LIB:-/usr/lib/rudolf}/layout.sh" || die "layout.sh is missing" 9; layout_apply "$D" "$BLOCKS"; return 0; }
		echo "The copy on $D does not match (try $try)."
	done
	die "The new boot files could not be written correctly: the drive may be failing. Your files are untouched; start from a Rudolf OS USB stick and use Install here > Replace the boot files only to try again." 9
}

# ---- 1.6.0 ("Before stable" 15): the start-up files replaced inside the existing EFI system partition
esp_part() { case "$1" in *[0-9]) echo "${1}p2";; *) echo "${1}2";; esac; }
SBCERT=${BF_SBCERT:-/usr/share/rudolf/rudolf-sb.crt}
SBV=${SBVERIFY:-sbverify}
health() { [ -n "$BF_NOHEALTH" ] || rudolfd --health boot "$1" 2>/dev/null; echo "$1"; }
# the computer on mains power, or its battery above 50 % (or none: a desktop computer)
power_ok() {
	[ -n "$BF_FORCE_POWER" ] && return 0; bat=0
	for p in /sys/class/power_supply/*; do [ -e "$p/type" ] || continue
		case "$(cat "$p/type")" in Mains|USB|USB_C|USB_PD) [ "$(cat "$p/online" 2>/dev/null)" = 1 ] && return 0;;
		Battery) bat=1; [ "$(cat "$p/capacity" 2>/dev/null || echo 0)" -gt 50 ] && return 0;; esac; done
	[ $bat = 0 ]; }
# a start-up file's signature: Rudolf OS's for its own (rudolf.efi, recovery.efi, the boot menu grubx64.efi); for shim
# (BOOTX64.EFI) and MokManager (mmx64.efi), which Microsoft signs, a signature at all; other files: none
sig_ok() {
	case "$(basename "$1" | tr A-Z a-z)" in
	bootx64.efi|mmx64.efi) $SBV --list "$1" 2>/dev/null | grep -qi 'signature\|subject';;
	*.efi) $SBV --cert "$SBCERT" "$1" >/dev/null 2>&1;;
	*) return 0;; esac; }
# the drive's partition table as Install here leaves it (1 empty, 2 the EFI system partition, 3 RUDOLFDATA):
# "P2START P2SIZE P3START" in sectors, or nothing
inst_table() {
	dd if="$1" bs=512 count=1 2>/dev/null | od -An -tu1 -v | tr -s ' \n' ' ' | awk '{n=split($0,b," ");
		if (n>=512 && b[511]==85 && b[512]==170 && b[451]==0 && b[467]==239 && b[483]==131)
			printf "%d %d %d\n", b[471]+b[472]*256+b[473]*65536+b[474]*16777216, b[475]+b[476]*256+b[477]*65536+b[478]*16777216, b[487]+b[488]*256+b[489]*65536+b[490]*16777216}'; }
# the El Torito UEFI entry of the image at the start of a drive: "LBA COUNT" (2048-byte blocks; 512-byte sectors)
elt_entry() {
	c=$(dd if="$1" bs=2048 skip=17 count=1 2>/dev/null | dd bs=1 skip=71 count=4 2>/dev/null | od -An -tu4 | tr -d ' \n')
	[ -n "$c" ] && [ "$c" -gt 0 ] || return 1
	dd if="$1" bs=2048 skip="$c" count=1 2>/dev/null | od -An -tu1 -v | tr -s ' \n' ' ' | awk '{n=split($0,b," "); p=b[2];
		for (i=1;i<64;i++) { o=32*i; if (b[o+1]==144||b[o+1]==145) { p=b[o+2]; continue }
			if (b[o+1]==136 && p==239) { printf "%d %d\n", b[o+9]+b[o+10]*256+b[o+11]*65536+b[o+12]*16777216, b[o+7]+b[o+8]*256; exit } } }'; }
BT=${BF_TMP:-/tmp/bootfiles.$$}
# (the old start-up files are kept on RUDOLFDATA, os/esp-old, until the next start from the drive: the EFI system
#  partition of a release's boot image has a few MiB free, not room for a second copy of a 12 MiB start-up file)
KEEP=${BF_KEEP:-/persist/os/esp-old}
# put_back: every file of the list as it was (from KEEP), NAME.new removed; then BAD is said
put_back() {
	while read -r f; do [ -n "$f" ] || continue; [ -f "$E/$f.new" ] && rm -f "$E/$f.new"
		if [ -f "$KEEP/$f" ]; then cmp -s "$KEEP/$f" "$E/$f" 2>/dev/null || { cp "$KEEP/$f" "$E/$f.new" && mv -f "$E/$f.new" "$E/$f"; }
		elif [ -f "$KEEP/$f.none" ]; then rm -f "$E/$f"; fi
	done < "$BT/todo"; sync; }
# files_into DISK NEW: the files of NEW (the new image's EFI image, mounted) into DISK's EFI system partition.
# One file at a time: written beside the old one (NAME.new), flushed, read back and its signature checked, then renamed
# over the old one, which is kept in KEEP. When the partition has no room for a new file beside the old one, the
# Recovery mode file (EFI/rudolf/recovery.efi: not needed for a normal start) is taken out first, kept, and written last.
# The files the firmware needs to start (\EFI\BOOT\BOOTX64.EFI, the boot menu, rudolf.efi) are always there, whole.
files_into() {
	D=$1; NEW=$2; P=$(esp_part "$D"); E=$BT/esp; mkdir -p "$E"; BAD=
	[ -b "$P" ] || die "$D has no EFI system partition (partition 2): nothing was changed." 5
	mount -t vfat -o rw,flush "$P" "$E" 2>/dev/null || die "The EFI system partition on $D cannot be opened: nothing was changed." 5
	(cd "$NEW" && find . -type f | sed 's|^\./||' | sort) > "$BT/all"
	: > "$BT/todo"; RC=EFI/rudolf/recovery.efi
	while read -r f; do
		[ -f "$E/$f" ] && cmp -s "$NEW/$f" "$E/$f" && continue
		case "$f" in *.efi|*.EFI) sig_ok "$NEW/$f" || { umount "$E"; die "$f in the new image is not signed as it should be: nothing was changed." 7; };; esac
		echo "$f" >> "$BT/todo"
	done < "$BT/all"
	if [ ! -s "$BT/todo" ]; then umount "$E"; echo "The start-up files on $D are already the image's: nothing to change."; return 0; fi
	# recovery.efi last (its room may be needed for the others)
	grep -vx "$RC" "$BT/todo" > "$BT/t2"; grep -qx "$RC" "$BT/todo" && echo "$RC" >> "$BT/t2"; mv "$BT/t2" "$BT/todo"
	free=$(($(df -k "$E" | awk 'NR==2 {print $4}') * 1024)); size=$(($(df -k "$E" | awk 'NR==2 {print $2}') / 1024))
	spare=0; [ -f "$E/$RC" ] && [ -f "$NEW/$RC" ] && spare=$(stat -c %s "$E/$RC")
	# room: each new file in turn beside the old one (the largest), with recovery.efi's room if need be; and at the end
	# every new file in place of the old
	big=0; tot=0; while read -r f; do s=$(stat -c %s "$NEW/$f"); o=0; [ -f "$E/$f" ] && o=$(stat -c %s "$E/$f"); [ $s -gt $big ] && big=$s; tot=$((tot + s - o)); done < "$BT/todo"
	if [ $((big + 65536)) -gt $((free + spare)) ] || [ $((tot + 65536)) -gt $free ]; then umount "$E"
		health "the start-up files were not replaced: the EFI system partition on $D ($size MiB, made by an older Rudolf OS) has no room for them ($(( (big > tot ? big : tot) / 1048576)) MiB needed, $((free / 1048576)) MiB free). Nothing was changed; the computer starts as before."
		exit 10; fi
	# (recovery.efi taken out for room is always written again, last, even when it was the image's already)
	if [ $((big + 65536)) -gt $free ] && [ $spare -gt 0 ] && ! grep -qx "$RC" "$BT/todo"; then echo "$RC" >> "$BT/todo"; fi
	# the old ones kept first
	mkdir -p "$KEEP" || { umount "$E"; die "The old start-up files could not be kept: nothing was changed." 6; }
	while read -r f; do mkdir -p "$KEEP/$(dirname "$f")"; rm -f "$KEEP/$f" "$KEEP/$f.none"
		if [ -f "$E/$f" ]; then cp "$E/$f" "$KEEP/$f" && cmp -s "$E/$f" "$KEEP/$f" || { umount "$E"; die "The old start-up files could not be kept ($f): nothing was changed." 6; }
		else : > "$KEEP/$f.none"; fi
	done < "$BT/todo"; sync
	echo "Replacing $(wc -l < "$BT/todo") start-up files on $D, one at a time (each written beside the old one, checked, then renamed; the old ones kept in $KEEP) ..."
	while read -r f; do
		mkdir -p "$E/$(dirname "$f")"
		s=$(stat -c %s "$NEW/$f"); fr=$(($(df -k "$E" | awk 'NR==2 {print $4}') * 1024))
		if [ $((s + 65536)) -gt $fr ] && [ "$f" != "$RC" ] && [ -f "$E/$RC" ] && [ -f "$KEEP/$RC" ]; then echo "  (Recovery mode's start-up file taken out for room; kept, and written last)"; rm -f "$E/$RC"; fi
		cp "$NEW/$f" "$E/$f.new" || { BAD="$f could not be written (the drive may be full or failing)"; break; }
		sync; umount "$E"; echo 3 > /proc/sys/vm/drop_caches 2>/dev/null
		mount -t vfat -o rw,flush "$P" "$E" 2>/dev/null || { BAD="the EFI system partition could not be opened again"; break; }
		cmp -s "$NEW/$f" "$E/$f.new" || { BAD="$f did not read back as written"; break; }
		case "$f" in *.efi|*.EFI) sig_ok "$E/$f.new" || { BAD="$f is not signed as it should be, read back"; break; };; esac
		mv -f "$E/$f.new" "$E/$f" || { BAD="$f could not be renamed"; break; }
		sync; echo "  $f"
	done < "$BT/todo"
	sync; umount "$E" 2>/dev/null; echo 3 > /proc/sys/vm/drop_caches 2>/dev/null
	# checked once more as the firmware finds them: \EFI\BOOT\BOOTX64.EFI there, each file the image's
	if [ -z "$BAD" ] && mount -t vfat -o ro "$P" "$E" 2>/dev/null; then
		[ -s "$E/EFI/BOOT/BOOTX64.EFI" ] || BAD="\\EFI\\BOOT\\BOOTX64.EFI is missing"
		while read -r f; do cmp -s "$NEW/$f" "$E/$f" || BAD="$f is not as written"; done < "$BT/todo"
		umount "$E"
	else BAD=${BAD:-"the EFI system partition could not be read back"}; fi
	if [ -n "$BAD" ]; then
		if mount -t vfat -o rw,flush "$P" "$E" 2>/dev/null; then put_back; umount "$E"; fi
		health "the start-up files on $D were put back as they were: $BAD."; exit 9; fi
	health "the start-up files on $D replaced in its EFI system partition, one at a time ($(tr '\n' ' ' < "$BT/todo" | sed 's/ $//')); the old ones kept until the next start from the drive; the partition table and the disk signature unchanged"
}
# the drive Rudolf OS keeps its files on, or nothing
sys_disk() { PER=$(cat /etc/rudolf/persist 2>/dev/null); [ -n "$PER" ] && mountpoint -q /persist && disk_of "$PER"; }
# files IMAGE [DISK]: the image's start-up files (IMAGE empty: this Rudolf OS's own) into DISK's EFI system partition
cmd_files() {
	power_ok || die "Connect the computer to power first (or charge it above 50 %): the start-up files must not be cut off half written." 11
	check_image "$1"; D=$2; [ -n "$D" ] || D=$(sys_disk); [ -n "$D" ] || die "Rudolf OS is not installed on a drive here." 3
	[ -n "$(inst_table "$D")" ] || die "$D does not have the installed layout (partition 2 the EFI system partition, 3 RUDOLFDATA): nothing was changed." 5
	mkdir -p "$BT/i" "$BT/n"; trap 'umount "$BT/n" "$BT/i" "$BT/esp" 2>/dev/null; rm -rf "${BT:?}"; [ -n "$BUSY" ] && rm -f "$BUSY"' EXIT
	if [ -z "$1" ]; then mount -t vfat -o ro,loop /live/media/efi/efiboot.img "$BT/n" || die "This Rudolf OS's own start-up files cannot be read." 7
	else { mount -t iso9660 -o ro,loop "$SRC" "$BT/i" && mount -t vfat -o ro,loop "$BT/i/efi/efiboot.img" "$BT/n"; } || die "The image's start-up files cannot be read." 7; fi
	files_into "$D" "$BT/n"
	umount "$BT/n" "$BT/i" 2>/dev/null
	# the firmware's entry for the drive: checked (and put right) before any restart
	if [ "$D" = "$(sys_disk)" ] && [ -d /sys/firmware/efi/efivars ]; then
		rudolfd --boot-entry fix || { echo "The firmware's entry for this drive is not right yet (above): do not restart before it is (Settings > Update and recovery > Check this drive's start-up)."; exit 12; }
	fi
	echo "Done: the start-up files are Rudolf OS $IVER's. Your files are as they were."
}
# check|rescue
cmd_check() {
	D=$(sys_disk); [ -n "$D" ] || die "Rudolf OS is not installed on a drive here (it keeps no files on a drive)." 3
	echo "The drive: $D"; BAD=
	t=$(inst_table "$D")
	if [ -z "$t" ]; then echo "Its partition table is not as Install here makes it (1 empty, 2 the EFI system partition, 3 RUDOLFDATA)."; BAD=1
	else set -- $t; echo "Its partition table: the EFI system partition at sector $1 ($(($2 / 2048)) MiB), RUDOLFDATA at sector $3; disk signature $(dd if="$D" bs=1 skip=440 count=4 2>/dev/null | od -An -tx1 | tr -d ' \n')."
		e=$(elt_entry "$D")
		if [ -n "$e" ]; then set -- $e $t
			if [ $(($1 * 4)) = "$3" ]; then echo "The firmware finds the EFI system partition there (El Torito: block $1, $2 sectors)."; else echo "The El Torito entry (block $1) does not point at partition 2 (sector $3)."; BAD=1; fi
		else echo "The image in front of RUDOLFDATA has no El Torito entry for UEFI."; BAD=1; fi
	fi
	P=$(esp_part "$D"); mkdir -p "$BT/esp"; trap 'umount "$BT/esp" 2>/dev/null; rm -rf "${BT:?}"' EXIT
	if mount -t vfat -o ro "$P" "$BT/esp" 2>/dev/null; then
		for f in EFI/BOOT/BOOTX64.EFI EFI/BOOT/grubx64.efi EFI/BOOT/mmx64.efi EFI/rudolf/rudolf.efi EFI/rudolf/recovery.efi; do
			if [ ! -s "$BT/esp/$f" ]; then v=missing; case "$f" in */BOOTX64.EFI|*/grubx64.efi|*/rudolf.efi) BAD=1;; esac
			elif sig_ok "$BT/esp/$f"; then case "$f" in */BOOTX64.EFI|*/mmx64.efi) v=signed;; *) v="signed by Rudolf OS";; esac
			else v="not signed as it should be"; fi
			echo "  \\$(echo "$f" | tr / '\\\\'): $v"; done
		o=$(cd "$BT/esp" && find . -name '*.old' | sed 's|^\./||' | tr '\n' ' '); [ -n "$o" ] && echo "  kept from the last change: $o"
		umount "$BT/esp"
	else echo "Its EFI system partition (partition 2) cannot be opened."; BAD=1; fi
	if [ -d /sys/firmware/efi/efivars ]; then echo "The firmware's entry for it:"; rudolfd --boot-entry "$([ "$1" = rescue ] && echo fix || echo check)"; [ $? = 2 ] && BAD=1
	else echo "This computer started without UEFI: there is no firmware entry."; fi
	if [ -z "$BAD" ]; then echo "All is right: the computer can start from this drive."; else echo "Not all is right (above)."; fi
	[ -z "$BAD" ]
}
# area IMAGE [DISK] (experts): the image over the area, keeping the disk signature and RUDOLFDATA's entry; the old area
# put back unless the new one mounts, its start-up files check and the firmware's entry is right
cmd_area() {
	power_ok || die "Connect the computer to power first (or charge it above 50 %)." 11
	check_image "$1"; D=$2; [ -n "$D" ] || D=$(sys_disk); [ -n "$D" ] || die "Rudolf OS is not installed on a drive here." 3
	[ "$D" = "$(sys_disk)" ] || die "Only the drive Rudolf OS keeps its files on can have its start-up area rebuilt (its firmware entry is checked)." 3
	check_fit "$D"
	S=/persist/os/bootimg; mkdir -p $S; echo "Keeping the old start-up area on RUDOLFDATA ..."
	dd if="$D" of=$S/area.old bs=512 count="$START" 2>/dev/null && sync || die "The old start-up area could not be kept: nothing was changed." 6
	SIG=$(dd if="$D" bs=1 skip=440 count=4 2>/dev/null | od -An -tx1 | tr -d ' \n'); P3=$(dd if="$D" bs=1 skip=478 count=16 2>/dev/null | od -An -tx1 | tr -d ' \n')
	echo "Writing the new start-up area ($((BYTES / 1048576)) MiB) ..."
	dd if="$SRC" of="$D" bs=2048 count="$BLOCKS" conv=fsync 2>/dev/null
	{ hex2bin "$SIG" | dd of="$D" bs=1 seek=440 conv=notrunc,fsync; dd if=/dev/zero of="$D" bs=1 seek=446 count=16 conv=notrunc,fsync; hex2bin "$P3" | dd of="$D" bs=1 seek=478 conv=notrunc,fsync; } 2>/dev/null
	sync; blockdev --flushbufs "$D" 2>/dev/null; echo 3 > /proc/sys/vm/drop_caches
	if cmd_check rescue; then rm -f $S/area.old; health "the start-up area of $D was rebuilt from Rudolf OS $IVER's image (the disk signature and RUDOLFDATA's entry kept), checked, the firmware's entry right"; return 0; fi
	echo "The new start-up area did not check: putting the old one back ..."
	dd if=$S/area.old of="$D" bs=512 conv=fsync,notrunc 2>/dev/null; sync; blockdev --flushbufs "$D" 2>/dev/null; echo 3 > /proc/sys/vm/drop_caches
	rudolfd --boot-entry fix >/dev/null 2>&1
	health "the start-up area of $D did not check after it was rebuilt, so the old one was put back"; exit 9
}

# (tests: BF_LIB=1 . bootfiles.sh gives the functions only)
[ -n "$BF_LIB" ] && return 0
case "$1" in
stage)
	# (1.6.0: IMAGE's start-up files into this drive's EFI system partition, now, while the system runs; nothing is left
	#  for the start-up to write any more. Before, the image was kept on RUDOLFDATA and written over the whole area at
	#  the next start: the owner's Vivobook could not start after it.)
	SLOT=$(cat /etc/rudolf/slot 2>/dev/null); PER=$(cat /etc/rudolf/persist 2>/dev/null); MED=$(cat /etc/rudolf/media 2>/dev/null)
	[ -n "$PER" ] && mountpoint -q /persist || die "Rudolf OS is not keeping files on this drive (running live)." 3
	DISK=$(disk_of "$PER")
	[ "$(disk_of "$MED")" = "$DISK" ] && [ "$PER" = "$(part3 "$DISK")" ] || die "Rudolf OS did not start from the drive it is installed on (a USB stick?). To give that drive new boot files, use Install here > Replace the boot files only." 3
	case "$SLOT" in a|b) ;; *) die "Rudolf OS is running the system on the drive's own image, not an installed update. Install the latest update (Settings > Update and recovery), restart, and try again." 3;; esac
	[ "$(sed -n 's/^good=//p' /persist/os/system/state)" = "$SLOT" ] || die "The update that is running has not been kept yet: restart once more, then try again." 3
	B=/persist/os/bootimg; [ -e $B/pending ] && { rm -f $B/pending $B/boot.iso; echo "(new boot files kept from before for the start-up are no longer written: removed)"; }
	cmd_files "$2" "$DISK"
	;;
files) cmd_files "$2" "$3";;
check|rescue) cmd_check "$1";;
area) cmd_area "$2" "$3";;
write)
	# (1.6.0: from a USB stick, into DISK's EFI system partition, never over its area; its RUDOLFDATA is not opened)
	DEV=$2; [ -b "$DEV" ] || die "No such disk: $DEV" 2
	N=${DEV##*/}
	MED=$(cat /etc/rudolf/media 2>/dev/null)
	[ "$(disk_of "$MED")" = "$DEV" ] && die "That is the drive Rudolf OS is running from: to give it new boot files, use Settings > Security > Secure Boot > Get new boot files." 3
	[ -n "$(inst_table "$DEV")" ] || die "$DEV does not have the installed layout (partition 2 the EFI system partition, 3 RUDOLFDATA): use Install here instead." 5
	BUSY=/run/rudolf/busy/$N; mkdir -p /run/rudolf/busy; : > "$BUSY"
	/usr/lib/rudolf/devplug release "$N" 2>/dev/null || { rm -f "$BUSY"; die "$DEV is in use: a file on it is still open. Close File Explorer and any app using the drive, then try again." 4; }
	cmd_files "$3" "$DEV"; rm -f "$BUSY"
	echo "Done: $DEV has the new boot files (Rudolf OS $IVER), and its files are as they were. Turn on Secure Boot in its firmware settings if you want it."
	;;
*) echo "usage: bootfiles.sh stage IMAGE | write DISK [IMAGE] | files IMAGE [DISK] | check | rescue | area IMAGE [DISK]"; exit 2;;
esac
