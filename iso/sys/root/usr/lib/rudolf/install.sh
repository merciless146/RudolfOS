#!/bin/sh
# Install Rudolf OS onto a whole disk. Everything on that disk is erased.
# Layout: the Rudolf OS image (boots on BIOS and UEFI) followed by an ext4
# partition labelled RUDOLFDATA that keeps your files, settings and updates.
set -e
#   install.sh DISK [IMAGE]   IMAGE (1.2.4): a downloaded rudolf-os-*.iso to install instead of the running
#                             drive's own image, so an older stick can install a newer, Secure Boot ready system
DEV="$1"
[ -b "$DEV" ] || { echo "No such disk: $DEV"; exit 2; }
MEDIA=$(cat /etc/rudolf/media 2>/dev/null)
case "$MEDIA" in "$DEV"|"$DEV"[0-9]*|"$DEV"p[0-9]*) echo "That is the drive Rudolf OS is running from. Choose another disk (or use Settings > Storage and backup > Keep files on this drive)."; exit 3;; esac
SRC=$MEDIA
if [ -n "$2" ]; then
	SRC=$2
	case "$SRC" in *..*) echo "Bad file name."; exit 7;; /home/rudolf/*.iso|/home/rudolf/*.ISO) ;; *) echo "The image must be a .iso file in your home folder."; exit 7;; esac
	[ -f "$SRC" ] || { echo "No such file: $SRC"; exit 7; }
	# only a Rudolf OS disc image whose release list is signed with a key this Rudolf OS trusts
	mkdir -p /mnt/instsrc; umount /mnt/instsrc 2>/dev/null
	mount -t iso9660 -o ro,loop "$SRC" /mnt/instsrc || { echo "That file is not a disc image."; exit 7; }
	OK=1; [ -e /mnt/instsrc/rudolf/id ] || OK=0
	V=$(rudolf-update verify-manifest /mnt/instsrc/rudolf/manifest.txt /mnt/instsrc/rudolf/manifest.sig 2>&1) || OK=0
	umount /mnt/instsrc
	[ $OK = 1 ] || { echo "That file is not a Rudolf OS disc image signed by Rudolf OS ($V)."; exit 7; }
	echo "Installing from $(basename "$SRC"): $(echo "$V" | sed -n 's/^version //p')"
fi
N=${DEV##*/}
# a USB drive Rudolf OS mounted by itself (under /media) is unmounted first, and nothing on it is mounted while it is
# written (1.2.6: a stick formatted in the shop was refused as "in use")
BUSY=/run/rudolf/busy/$N
if [ ! -e $BUSY ]; then mkdir -p /run/rudolf/busy; : > $BUSY; trap "rm -f $BUSY" EXIT; fi
/usr/lib/rudolf/devplug release $N 2>/dev/null || { echo "$DEV is in use: a file on it is still open. Close File Explorer and any app using the drive, then try again."; exit 4; }
grep -qE "^${DEV}p?[0-9]* " /proc/mounts && { echo "$DEV is in use (mounted at $(grep -E "^${DEV}p?[0-9]* " /proc/mounts | awk '{print $2}' | head -1)). Unmount it first."; exit 4; }
SECTORS=$(cat /sys/block/$N/size)
# size of the Rudolf OS image: ISO9660 volume space size (2048-byte blocks)
BLOCKS=$(dd if="$SRC" bs=2048 skip=16 count=1 2>/dev/null | dd bs=1 skip=80 count=4 2>/dev/null | od -An -tu4 | tr -d ' ')
IMG=$((BLOCKS * 4))
NEED=$((IMG + 1024 * 2048))
[ "$SECTORS" -gt "$NEED" ] || { echo "The disk is too small (needs at least $((NEED / 2048)) MiB)."; exit 5; }
echo "Step 1/5: copying Rudolf OS ($((IMG / 2048)) MiB) to $DEV ..."
dd if="$SRC" of="$DEV" bs=2048 count="$BLOCKS" conv=fsync 2>&1 | tail -1
# read the copy back from the drive itself (not from memory) and compare it with the source (1.2.5)
echo "Step 2/5: checking the copy ..."
sync; blockdev --flushbufs "$DEV" 2>/dev/null; echo 3 > /proc/sys/vm/drop_caches 2>/dev/null
A=$(dd if="$SRC" bs=2048 count="$BLOCKS" 2>/dev/null | sha256sum | cut -c1-64)
B=$(dd if="$DEV" bs=2048 count="$BLOCKS" 2>/dev/null | sha256sum | cut -c1-64)
[ "$A" = "$B" ] || { echo "The copy on $DEV does not match Rudolf OS: the drive may be failing or was unplugged. Try another drive."; exit 9; }
echo "The copy matches (SHA-256 ${A%${A#????????????????}})."
echo "Step 3/5: removing old partition tables ..."
dd if=/dev/zero of="$DEV" bs=512 seek=$((SECTORS - 2048)) count=2048 conv=fsync 2>/dev/null
START=$(( (IMG + 2047) / 2048 * 2048 ))
SIZE=$((SECTORS - START - 2048))
le32() { printf "\\$(printf %03o $(($1 & 255)))\\$(printf %03o $((($1 >> 8) & 255)))\\$(printf %03o $((($1 >> 16) & 255)))\\$(printf %03o $((($1 >> 24) & 255)))"; }
{ printf '\000\376\377\377\203\376\377\377'; le32 $START; le32 $SIZE; } | dd of="$DEV" bs=1 seek=$((446 + 32)) conv=notrunc 2>/dev/null
sync
blockdev --rereadpt "$DEV" 2>/dev/null || true
sleep 2; mdev -s
case "$N" in *[0-9]) P="${DEV}p3";; *) P="${DEV}3";; esac
[ -b "$P" ] || { echo "The new data partition did not appear ($P)."; exit 6; }
echo "Step 4/5: creating the RUDOLFDATA partition ($((SIZE / 2048)) MiB) ..."
mke2fs -q -F -t ext4 -L RUDOLFDATA -m 1 "$P"
echo "Step 5/5: finishing ..."
mkdir -p /mnt/newdata && mount "$P" /mnt/newdata
mkdir -p /mnt/newdata/home/rudolf /mnt/newdata/os /mnt/newdata/etc
chown 1000:1000 /mnt/newdata/home/rudolf
echo "installed $(date)" > /mnt/newdata/etc/installed
umount /mnt/newdata
sync
[ -n "$MKSTICK" ] || echo "Rudolf OS is installed on $DEV. Remove the disc or USB drive and restart."
