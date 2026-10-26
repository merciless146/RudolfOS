#!/bin/sh
# New boot files for a drive Rudolf OS is installed on, keeping its files (1.2.8). GPL-2.0-or-later.
#   bootfiles.sh stage IMAGE       on the drive Rudolf OS runs from: checks IMAGE and keeps a copy on RUDOLFDATA; the
#                                  start-up writes it before anything else at the next start (initramfs/init)
#   bootfiles.sh write DISK [IMAGE] from another drive (a USB stick): writes IMAGE (empty: this Rudolf OS's own) to the
#                                  start of DISK, in front of its RUDOLFDATA partition, which is left as it is
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
		case "$SRC" in *..*) die "Bad file name." 7;; /home/rudolf/*.iso|/home/rudolf/*.ISO) ;; *) die "The image must be a .iso file in your home folder." 7;; esac
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
		[ "$GOT" = "$WANT" ] && { echo "The copy matches."; return 0; }
		echo "The copy on $D does not match (try $try)."
	done
	die "The new boot files could not be written correctly: the drive may be failing. Your files are untouched; start from a Rudolf OS USB stick and use Install here > Replace the boot files only to try again." 9
}

# (tests: BF_LIB=1 . bootfiles.sh gives the functions only)
[ -n "$BF_LIB" ] && return 0
case "$1" in
stage)
	SLOT=$(cat /etc/rudolf/slot 2>/dev/null); PER=$(cat /etc/rudolf/persist 2>/dev/null); MED=$(cat /etc/rudolf/media 2>/dev/null)
	[ -n "$PER" ] && mountpoint -q /persist || die "Rudolf OS is not keeping files on this drive (running live)." 3
	DISK=$(disk_of "$PER")
	[ "$(disk_of "$MED")" = "$DISK" ] && [ "$PER" = "$(part3 "$DISK")" ] || die "Rudolf OS did not start from the drive it is installed on (a USB stick?). To give that drive new boot files, use Install here > Replace the boot files only." 3
	case "$SLOT" in a|b) ;; *) die "Rudolf OS is running the system on the drive's own image, not an installed update. Install the latest update (Settings > Update and recovery), restart, and try again." 3;; esac
	[ "$(sed -n 's/^good=//p' /persist/os/system/state)" = "$SLOT" ] || die "The update that is running has not been kept yet: restart once more, then try again." 3
	check_image "$2"; check_fit "$DISK"
	complete_slots /persist/os/system /live/media
	B=/persist/os/bootimg; mkdir -p $B; rm -f $B/pending $B/boot.iso $B/boot.iso.new
	echo "Keeping a copy for the next start ..."
	if [ "$SRC" = "$MED" ]; then dd if="$SRC" of=$B/boot.iso.new bs=2048 count=$BLOCKS 2>/dev/null; else cp "$SRC" $B/boot.iso.new; fi
	H=$(sha256sum $B/boot.iso.new | cut -c1-64)
	mv $B/boot.iso.new $B/boot.iso
	printf 'sha=%s\nstart=%s\nblocks=%s\nversion=%s\nserial=%s\n' "$H" "$START" "$BLOCKS" "$IVER" "$ISERIAL" > $B/pending.new && sync && mv $B/pending.new $B/pending && sync
	rm -f $B/result
	echo "Ready: the new boot files (Rudolf OS $IVER) are written the next time the computer starts, before anything else (about a minute)."
	echo "Keep the computer plugged in, and do not switch it off while it says \"Writing the new boot files\"."
	;;
write)
	DEV=$2; [ -b "$DEV" ] || die "No such disk: $DEV" 2
	N=${DEV##*/}
	MED=$(cat /etc/rudolf/media 2>/dev/null)
	[ "$(disk_of "$MED")" = "$DEV" ] && die "That is the drive Rudolf OS is running from: to give it new boot files, use Settings > Security > Secure Boot > Get new boot files." 3
	check_image "$3"; check_fit "$DEV"
	BUSY=/run/rudolf/busy/$N; mkdir -p /run/rudolf/busy; : > $BUSY
	trap "umount /mnt/bftarget /mnt/bfold 2>/dev/null; rm -f $BUSY" EXIT
	/usr/lib/rudolf/devplug release $N 2>/dev/null || die "$DEV is in use: a file on it is still open. Close File Explorer and any app using the drive, then try again." 4
	mkdir -p /mnt/bftarget /mnt/bfold
	mount -t ext4 "$(part3 "$DEV")" /mnt/bftarget || die "$DEV's RUDOLFDATA partition could not be opened (it is not changed)." 6
	mount -t iso9660 -o ro "$(part1 "$DEV")" /mnt/bfold 2>/dev/null || mount -t iso9660 -o ro "$DEV" /mnt/bfold 2>/dev/null || true
	[ -d /mnt/bftarget/os/system ] && complete_slots /mnt/bftarget/os/system /mnt/bfold
	[ -d /mnt/bftarget/os/system ] || [ $BOOTONLY = 0 ] || die "This drive has no installed update to start from, so boot files only would leave it without a system: use a full disc image (if it fits) or Install here." 6
	rm -f /mnt/bftarget/os/bootimg/pending
	umount /mnt/bfold 2>/dev/null; umount /mnt/bftarget; sync
	write_area "$DEV" "$SRC"
	echo "Done: $DEV has the new boot files (Rudolf OS $IVER), and its files are as they were. Turn on Secure Boot in its firmware settings if you want it."
	;;
*) echo "usage: bootfiles.sh stage IMAGE | write DISK [IMAGE]"; exit 2;;
esac
