#!/bin/sh
# Make a Rudolf OS USB stick from inside Rudolf OS (1.2.5). GPL-2.0-or-later. Everything on the stick is erased.
#   mkstick.sh DISK [IMAGE] [FILES]
# IMAGE: "" for this Rudolf OS (the image on the drive it started from, and the updates installed on it), or a
#        downloaded rudolf-os-*.iso in the home folder (checked as install.sh does). FILES: 1 to copy this
#        session's files and settings onto the new stick too.
# install.sh writes the image, reads it back and compares it, and adds RUDOLFDATA after it; this adds the rest.
#   mkstick.sh --check DIR BOOTONLY   (1.5.3, also for iso/qt/host/mkstick_t.sh) whether the RUDOLFDATA mounted at DIR
#                                     can start: with a boot image (BOOTONLY=1), its system slot must be complete
export PATH=${RUDOLF_TEST_PATH:+$RUDOLF_TEST_PATH:}/usr/sbin:/usr/bin:/sbin:/bin
set -e
# 1.5.3: an installed Rudolf OS keeps only its boot files in front of RUDOLFDATA (its system is in RUDOLFDATA's os
# folder), so a stick made from it has a boot image, and starts only if that system was copied over. On 2026-11-07 a
# stick was reported ready with an empty os folder and would not start (the HP: "this drive has boot files only").
# Now that is checked before "Done", and a missing or incomplete copy is an error.
stick_check() {
	D=$1; BO=$2
	[ "$BO" = 1 ] || return 0
	G=$(sed -n 's/^good=//p' "$D/os/system/state" 2>/dev/null || true)   # (set -e: no state file is not an error here)
	case "$G" in a|b) ;; *) echo "The new stick has no system: this Rudolf OS's installed system was not copied onto it."; return 11;; esac
	[ -e "$D/os/system/$G/complete" ] || { echo "The new stick's system (slot $G) is incomplete."; return 12; }
	for L in vmlinuz base.sfs chromium.sfs modules.sfs desktop.sfs; do
		[ -s "$D/os/system/$G/$L" ] || { echo "The new stick's system is missing $L."; return 12; }
	done
	# the copy is read back and compared with this computer's (the slot in use, as the stick will start it)
	for L in vmlinuz base.sfs chromium.sfs modules.sfs desktop.sfs; do
		[ -n "$SRCOS" ] || break
		cmp -s "$SRCOS/system/$G/$L" "$D/os/system/$G/$L" || { echo "The copy of $L on the new stick does not match: the stick may be failing or was unplugged."; return 13; }
	done
	return 0
}
if [ "$1" = --check ]; then stick_check "$2" "$3"; exit $?; fi
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
MKSTICK=1 /usr/lib/rudolf/install.sh "$DEV" $IMG
case "$N" in *[0-9]) P="${DEV}p3";; *) P="${DEV}3";; esac
# 1.5.3: is the image now on the stick a boot image (boot files only)? Then the installed system must be copied
case "$N" in *[0-9]) P1="${DEV}p1";; *) P1="${DEV}1";; esac
BOOTONLY=0; mkdir -p /mnt/stickimg
if mount -t iso9660 -o ro "$P1" /mnt/stickimg 2>/dev/null; then [ -e /mnt/stickimg/rudolf/bootonly ] && BOOTONLY=1; umount /mnt/stickimg; fi
mkdir -p /mnt/newstick && mount -t ext4 "$P" /mnt/newstick || { echo "The new stick's RUDOLFDATA partition could not be opened."; exit 6; }
trap "umount /mnt/newstick 2>/dev/null; rm -f $BUSY" EXIT
FREE=$(df -k /mnt/newstick | awk 'NR==2 {print $4}')
# the updates installed on this Rudolf OS, so the new stick starts the same version (not with a downloaded image)
if [ -z "$IMG" ] && [ -d /persist/os/system ] && mountpoint -q /persist; then
	NEED=$(du -sk /persist/os | cut -f1)
	if [ "$NEED" -lt $((FREE - 262144)) ]; then
		echo "Copying the installed updates ($((NEED / 1024)) MiB) ..."
		cp -a /persist/os/. /mnt/newstick/os/
		FREE=$(df -k /mnt/newstick | awk 'NR==2 {print $4}')
	elif [ $BOOTONLY = 1 ]; then echo "Not enough room on the stick for Rudolf OS ($((NEED / 1024)) MiB). Use a bigger stick."; exit 11
	else echo "Not enough room for the installed updates: the new stick starts the version on this drive's disc image and updates itself."; fi
fi
# 1.5.3: the stick must be able to start: with a boot image, its copied system complete and the same as this one's
echo "Checking the new stick ..."
sync; echo 3 > /proc/sys/vm/drop_caches 2>/dev/null || true
SRCOS=/persist/os stick_check /mnt/newstick $BOOTONLY
if [ "$FILES" = 1 ]; then
	NEED=$(du -sk /home/rudolf | cut -f1)
	[ "$NEED" -lt $((FREE - 65536)) ] || { echo "Your files ($((NEED / 1024)) MiB) do not fit on the new stick. It is ready without them."; exit 10; }
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
sync; umount /mnt/newstick; trap "rm -f $BUSY" EXIT; sync
echo "Done: the Rudolf OS USB stick is ready on $DEV. You can remove it now."
