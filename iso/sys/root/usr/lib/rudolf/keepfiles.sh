#!/bin/sh
# Keep files on the USB stick (or disk) Rudolf OS is running from (Rudolf OS 1.2.4). GPL-2.0-or-later.
# A stick written with Rufus or Etcher holds only the disc image, so Rudolf OS runs live and nothing is kept.
# This adds the RUDOLFDATA partition in the free space after the image, without touching the image (the
# running system keeps working), the way install.sh lays out a whole disk:
#   keepfiles.sh check    "ok DISK FREE-MB" or "no REASON"
#   keepfiles.sh make     add partition 3 (MBR) after the image, make the file system; the live session's
#                         files move onto it when the computer restarts or shuts down
#   keepfiles.sh finish   (rudolfd, at restart/shut-down once the browser has stopped) copy the live session's
#                         home folder and settings onto the new partition
export PATH=/usr/sbin:/usr/bin:/sbin:/bin
PEND=/run/rudolf/keepfiles-pending
MIN=$((1024 * 2048))   # at least 1 GiB of free space (512-byte sectors)

# the whole disk the system started from (partition 1 of the image, or the disk itself)
media_disk() {
	M=$(cat /etc/rudolf/media 2>/dev/null); M=${M#/dev/}
	case "$M" in
		""|sr*) return 1;;
		nvme*n*p[0-9]*|mmcblk*p[0-9]*) echo "${M%p[0-9]*}";;
		sd*[0-9]|vd*[0-9]) echo "${M%%[0-9]*}";;
		*) echo "$M";;
	esac
}
part3() { case "$1" in *[0-9]) echo "/dev/${1}p3";; *) echo "/dev/${1}3";; esac; }

# sets D, SECTORS, START, SIZE or prints why not
plan() {
	[ -e /run/rudolf/persistent ] && { echo "no Rudolf OS already keeps your files here (RUDOLFDATA)"; return 1; }
	[ -e $PEND ] && { echo "no already done: restart to start keeping your files"; return 1; }
	D=$(media_disk) || { echo "no Rudolf OS started from a disc: write the image to a USB stick to keep files"; return 1; }
	[ -b /dev/$D ] && [ -r /sys/block/$D/size ] || { echo "no the drive Rudolf OS started from was not found"; return 1; }
	for p in /sys/block/$D/$D*; do
		[ -e "$p/partition" ] || continue
		[ "$(cat $p/partition)" -ge 3 ] && { echo "no this drive already has more partitions; use Install from another Rudolf OS drive"; return 1; }
	done
	# the third partition entry of the image's partition table must be free
	E=$(dd if=/dev/$D bs=1 skip=$((446 + 32)) count=16 2>/dev/null | od -An -tx1 | tr -d ' \n')
	[ "$E" = 00000000000000000000000000000000 ] || { echo "no the partition table has no free entry"; return 1; }
	SECTORS=$(cat /sys/block/$D/size)
	BLOCKS=$(dd if=/dev/$D bs=2048 skip=16 count=1 2>/dev/null | dd bs=1 skip=80 count=4 2>/dev/null | od -An -tu4 | tr -d ' ')
	[ -n "$BLOCKS" ] && [ "$BLOCKS" -gt 0 ] || { echo "no the Rudolf OS image on this drive was not recognized"; return 1; }
	IMG=$((BLOCKS * 4))
	START=$(( (IMG + 2047) / 2048 * 2048 ))
	SIZE=$((SECTORS - START - 2048))
	[ "$SIZE" -ge "$MIN" ] || { echo "no not enough free space after Rudolf OS on this drive (at least 1 GB is needed)"; return 1; }
	return 0
}

le32() { printf "\\$(printf %03o $(($1 & 255)))\\$(printf %03o $((($1 >> 8) & 255)))\\$(printf %03o $((($1 >> 16) & 255)))\\$(printf %03o $((($1 >> 24) & 255)))"; }

case "$1" in
check)
	plan || exit 0
	echo "ok $D $((SIZE / 2048))"
	;;
make)
	R=$(plan) || true
	[ -z "$R" ] || { echo "${R#no }"; exit 1; }
	plan >/dev/null
	echo "Step 1/3: adding a partition of $((SIZE / 2048)) MiB after Rudolf OS on $D ..."
	# an old partition table at the end of the drive (GPT backup from earlier use) would confuse other systems
	dd if=/dev/zero of=/dev/$D bs=512 seek=$((SECTORS - 2048)) count=2048 conv=fsync 2>/dev/null
	{ printf '\000\376\377\377\203\376\377\377'; le32 $START; le32 $SIZE; } | dd of=/dev/$D bs=1 seek=$((446 + 32)) conv=notrunc,fsync 2>/dev/null
	sync
	# the running system keeps the drive busy, so the kernel is told about the new partition alone
	rudolfd --addpart /dev/$D 3 $START $SIZE || { echo "The new partition could not be added while Rudolf OS runs. Restart, then try again."; exit 2; }
	mdev -s 2>/dev/null; P=$(part3 $D)
	for i in $(seq 30); do [ -b "$P" ] && break; sleep 0.2; mdev -s 2>/dev/null; done
	[ -b "$P" ] || { echo "The new partition did not appear ($P)."; exit 3; }
	echo "Step 2/3: creating the RUDOLFDATA file system ..."
	mke2fs -q -F -t ext4 -L RUDOLFDATA -m 1 "$P" || { echo "Could not create the file system."; exit 4; }
	echo "Step 3/3: preparing it ..."
	mkdir -p /mnt/keepfiles && mount -t ext4 "$P" /mnt/keepfiles || { echo "Could not use the new partition."; exit 5; }
	mkdir -p /mnt/keepfiles/home/rudolf /mnt/keepfiles/os /mnt/keepfiles/etc
	chown 1000:1000 /mnt/keepfiles/home/rudolf
	echo "keepfiles $(date)" > /mnt/keepfiles/etc/installed
	umount /mnt/keepfiles; sync
	echo "$P" > $PEND
	echo "Done. Restart Rudolf OS: your files, settings and apps are kept on this drive from now on."
	;;
finish)
	[ -s $PEND ] || exit 0
	P=$(cat $PEND)
	mkdir -p /mnt/keepfiles && mount -t ext4 "$P" /mnt/keepfiles || exit 1
	# everything the live session made: accounts, files, the desktop's databases, Linux apps' home
	cp -a /home/rudolf/. /mnt/keepfiles/home/rudolf/ 2>/dev/null
	chown 1000:1000 /mnt/keepfiles/home/rudolf
	# the settings rudolf-init restores from RUDOLFDATA, and saved Wi-Fi networks
	for f in asound.conf rudolf-display rudolf-screen rudolf-shell machine-id; do [ -f /etc/$f ] && cp /etc/$f /mnt/keepfiles/etc/; done
	[ -f /run/rudolf/wifi.conf ] && cp /run/rudolf/wifi.conf /mnt/keepfiles/etc/wifi.conf && chmod 600 /mnt/keepfiles/etc/wifi.conf
	sync; umount /mnt/keepfiles; rm -f $PEND
	;;
*)
	echo "usage: $0 check|make|finish"; exit 2;;
esac
