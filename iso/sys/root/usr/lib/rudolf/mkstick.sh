#!/bin/sh
# Make a Rudolf OS USB stick from inside Rudolf OS (1.2.5). GPL-2.0-or-later. Everything on the stick is erased.
#   mkstick.sh DISK [IMAGE] [FILES]
# IMAGE: "" for this Rudolf OS (the image on the drive it started from, and the updates installed on it; 1.5.7: with the
#        boot files of the system in use when the drive's are older, os_bootimg), or a
#        downloaded rudolf-os-*.iso in the home folder (checked as install.sh does). FILES: 1 to copy this
#        session's files and settings onto the new stick too.
# install.sh writes the image, reads it back and compares it, and adds RUDOLFDATA after it; this adds the rest.
#   mkstick.sh --check DIR BOOTONLY   (1.5.3, also for iso/qt/host/mkstick_t.sh) whether the RUDOLFDATA mounted at DIR
#                                     can start: with a boot image (BOOTONLY=1), its system slot must be complete
export PATH=${RUDOLF_TEST_PATH:+$RUDOLF_TEST_PATH:}/usr/sbin:/usr/bin:/sbin:/bin
set -e
# 1.5.3: a stick made from an installed Rudolf OS needs that system copied (oscopy.sh), checked before "Done"
. ${RUDOLF_LIB:-/usr/lib/rudolf}/oscopy.sh
if [ "$1" = --check ]; then os_check "$2" "$3" "$SRCOS"; exit $?; fi
# --info DISK: the stick's image (partition 1) and RUDOLFDATA (partition 3), mounted read-only for a moment
if [ "$1" = --info ]; then
	D=$2; N=${D##*/}; case "$N" in *[0-9]) P1=${D}p1; P3=${D}p3;; *) P1=${D}1; P3=${D}3;; esac
	I=/run/rudolf/stkinfo-$N; mkdir -p $I/i $I/d
	mount -t iso9660 -o ro "$P1" $I/i 2>/dev/null || mount -t iso9660 -o ro "$D" $I/i 2>/dev/null || { rmdir $I/i $I/d $I 2>/dev/null; echo '{"rudolf":false}'; exit 0; }
	if [ -e $I/i/rudolf/id ]; then mount -t ext4 -o ro,noload "$P3" $I/d 2>/dev/null || true; printf '{"rudolf":true,"info":'; os_info $I/i $I/d | tr -d '\n'; echo '}'
	else echo '{"rudolf":false}'; fi
	umount $I/d 2>/dev/null; umount $I/i 2>/dev/null; rmdir $I/i $I/d $I 2>/dev/null; exit 0
fi
DEV=$1; IMG=$2; FILES=$3
N=${DEV##*/}
[ -b "$DEV" ] && [ -r /sys/block/$N/size ] || { echo "No such drive: $DEV"; exit 2; }
MEDIA=$(cat /etc/rudolf/media 2>/dev/null)
case "$MEDIA" in "$DEV"|"$DEV"[0-9]*|"$DEV"p[0-9]*) echo "That is the drive Rudolf OS is running from. Choose another USB stick."; exit 3;; esac
# a USB stick or a memory card, of 2 GB or more (internal disks: Settings > Update and recovery > Install here)
RM=$(cat /sys/block/$N/removable)
case $(readlink -f /sys/block/$N/device) in */usb*) RM=1;; */mmc*) RM=1;; esac
[ "$RM" = 1 ] || { echo "$N is not a USB stick or memory card. To put Rudolf OS on an internal disk, use Install here."; exit 8; }
SECTORS=$(cat /sys/block/$N/size)
[ "$SECTORS" -ge 3900000 ] || { echo "The stick is too small: Rudolf OS needs at least 2 GB."; exit 5; }
# nothing on the stick is mounted by itself until it is finished (install.sh unmounts what was; 1.2.6)
BUSY=/run/rudolf/busy/$N; mkdir -p /run/rudolf/busy; : > $BUSY; trap "rm -f $BUSY" EXIT
# 1.5.7: the boot files of the installed system in use (its Recovery mode and safeguards), not the drive's older ones
if [ -z "$IMG" ] && mountpoint -q /persist && [ -d /persist/os/system ]; then
	if B=$(os_bootimg /persist/os /live/media /persist/os/bootimg); then IMG=$B; echo "The stick gets the boot files of this system (release $(sed -n 's/^serial //p' /persist/os/system/$(cat /etc/rudolf/slot)/RELEASE))."
	elif [ $? = 2 ]; then echo "Warning: the stick gets this drive's own boot files, which are older than this system: on it, Recovery mode may not start this computer's Rudolf OS. Connect to the internet and make it again to fix that."; fi
fi
# 1.5.8: room for the image and, from an installed Rudolf OS, its system, checked before anything is erased (before,
# a stick too small for the system was erased and given the boot files, then refused: a boot-files-only stick)
if [ -n "$IMG" ]; then IB=$(stat -c %s "$IMG" 2>/dev/null || echo 0); else IB=$(( $(dd if="$MEDIA" bs=2048 skip=16 count=1 2>/dev/null | dd bs=1 skip=80 count=4 2>/dev/null | od -An -tu4 | tr -d ' ') * 2048 )); fi
NEEDK=$((IB / 1024 + 65536))
SYSNEED=0; case "$IMG" in /persist/os/bootimg/*) SYSNEED=1;; "") [ "$(os_running_bootonly)" = 1 ] && SYSNEED=1;; esac
[ $SYSNEED = 1 ] && mountpoint -q /persist && [ -d /persist/os/system ] && NEEDK=$((NEEDK + $(du -sk /persist/os | cut -f1) + 262144))
[ $((SECTORS / 2)) -ge $NEEDK ] || { echo "The stick is too small for this Rudolf OS: it needs $((NEEDK / 1024 + 1)) MiB. Nothing on it was changed. Use a bigger stick."; exit 5; }
# from here on the stick is being written: anything that stops the script clears its start area again
DONE=0; H0=$(dd if="$DEV" bs=65536 count=1 2>/dev/null | sha256sum | cut -c1-64)
cleanup() {
	umount /mnt/newstick 2>/dev/null; umount /mnt/stickimg 2>/dev/null
	# (only when something was written: a stick refused before that, as in use, is left as it was)
	if [ "$DONE" != 1 ] && [ "$(dd if="$DEV" bs=65536 count=1 2>/dev/null | sha256sum | cut -c1-64)" != "$H0" ]; then
		dd if=/dev/zero of="$DEV" bs=1M count=8 conv=fsync 2>/dev/null; blockdev --rereadpt "$DEV" 2>/dev/null
		echo "The stick could not be made, so it was cleared: it will not start as a half-made Rudolf OS stick. Make it again, or try another stick."
	fi
	rm -f $BUSY
}
trap cleanup EXIT
MKSTICK=1 /usr/lib/rudolf/install.sh "$DEV" $IMG
case "$N" in *[0-9]) P="${DEV}p3";; *) P="${DEV}3";; esac
# 1.5.3: is the image now on the stick a boot image (boot files only)? Then the installed system must be copied
case "$N" in *[0-9]) P1="${DEV}p1";; *) P1="${DEV}1";; esac
BOOTONLY=0; mkdir -p /mnt/stickimg
if mount -t iso9660 -o ro "$P1" /mnt/stickimg 2>/dev/null; then [ -e /mnt/stickimg/rudolf/bootonly ] && BOOTONLY=1; umount /mnt/stickimg; fi
mkdir -p /mnt/newstick && mount -t ext4 "$P" /mnt/newstick || { echo "The new stick's RUDOLFDATA partition could not be opened."; exit 6; }
FREE=$(df -k /mnt/newstick | awk 'NR==2 {print $4}')
# the updates installed on this Rudolf OS, so the new stick starts the same version (not with a downloaded image)
case "$IMG" in /persist/os/bootimg/*) OWNSYS=1;; "") OWNSYS=1;; *) OWNSYS=0;; esac
if [ $OWNSYS = 1 ] && [ -d /persist/os/system ] && mountpoint -q /persist; then
	NEED=$(du -sk /persist/os | cut -f1)
	if [ "$NEED" -lt $((FREE - 262144)) ]; then
		os_copy /mnt/newstick /persist/os
		os_rescue /mnt/newstick
		FREE=$(df -k /mnt/newstick | awk 'NR==2 {print $4}')
	elif [ $BOOTONLY = 1 ]; then echo "Not enough room on the stick for Rudolf OS ($((NEED / 1024)) MiB). Use a bigger stick."; exit 11
	else echo "Not enough room for the installed updates: the new stick starts the version on this drive's disc image and updates itself."; fi
fi
# 1.5.3: the stick must be able to start: with a boot image, its copied system complete and the same as this one's
echo "Checking the new stick ..."
os_check /mnt/newstick $BOOTONLY /persist/os
if [ "$FILES" = 1 ]; then
	NEED=$(du -sk /home/rudolf | cut -f1)
	[ "$NEED" -lt $((FREE - 65536)) ] || { echo "Your files ($((NEED / 1024)) MiB) do not fit on the new stick. It is ready without them."; sync; umount /mnt/newstick; DONE=1; exit 10; }
	echo "Copying your files and settings ($((NEED / 1024)) MiB) ..."
	cp -a /home/rudolf/. /mnt/newstick/home/rudolf/
	chown 1000:1000 /mnt/newstick/home/rudolf
	# the settings rudolf-init restores from RUDOLFDATA (not machine-id: the new stick is a computer of its own)
	E=/etc; mountpoint -q /persist && [ -d /persist/etc ] && E=/persist/etc
	for f in asound.conf rudolf-display rudolf-screen rudolf-shell; do [ -f $E/$f ] && cp $E/$f /mnt/newstick/etc/; done
	W=/run/rudolf/wifi.conf; [ -f /persist/etc/wifi.conf ] && W=/persist/etc/wifi.conf
	[ -f $W ] && cp $W /mnt/newstick/etc/wifi.conf && chmod 600 /mnt/newstick/etc/wifi.conf
fi
echo "made-by-mkstick $(date)" >> /mnt/newstick/etc/installed
sync; umount /mnt/newstick; sync; DONE=1
echo "Done: the Rudolf OS USB stick is ready on $DEV. You can remove it now."
