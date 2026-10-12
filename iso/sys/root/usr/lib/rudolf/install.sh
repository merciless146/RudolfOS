#!/bin/sh
# Install Rudolf OS onto a whole disk. Everything on that disk is erased.
# Layout: the Rudolf OS image (boots on BIOS and UEFI) followed by an ext4
# partition labelled RUDOLFDATA that keeps your files, settings and updates.
set -e
DEV="$1"
[ -b "$DEV" ] || { echo "No such disk: $DEV"; exit 2; }
MEDIA=$(cat /etc/rudolf/media 2>/dev/null)
case "$MEDIA" in "$DEV"|"$DEV"[0-9]*|"$DEV"p[0-9]*) echo "That is the drive Rudolf OS is running from. Choose another disk."; exit 3;; esac
grep -q "^$DEV" /proc/mounts && { echo "$DEV is in use. Unmount its partitions first."; exit 4; }
N=${DEV##*/}
SECTORS=$(cat /sys/block/$N/size)
# size of the Rudolf OS image: ISO9660 volume space size (2048-byte blocks)
BLOCKS=$(dd if="$MEDIA" bs=2048 skip=16 count=1 2>/dev/null | dd bs=1 skip=80 count=4 2>/dev/null | od -An -tu4 | tr -d ' ')
IMG=$((BLOCKS * 4))
NEED=$((IMG + 1024 * 2048))
[ "$SECTORS" -gt "$NEED" ] || { echo "The disk is too small (needs at least $((NEED / 2048)) MiB)."; exit 5; }
echo "Step 1/4: copying Rudolf OS ($((IMG / 2048)) MiB) to $DEV ..."
dd if="$MEDIA" of="$DEV" bs=2048 count="$BLOCKS" conv=fsync 2>&1 | tail -1
echo "Step 2/4: removing old partition tables ..."
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
echo "Step 3/4: creating the RUDOLFDATA partition ($((SIZE / 2048)) MiB) ..."
mke2fs -q -F -t ext4 -L RUDOLFDATA -m 1 "$P"
echo "Step 4/4: finishing ..."
mkdir -p /mnt/newdata && mount "$P" /mnt/newdata
mkdir -p /mnt/newdata/home/rudolf /mnt/newdata/os /mnt/newdata/etc
chown 1000:1000 /mnt/newdata/home/rudolf
echo "installed $(date)" > /mnt/newdata/etc/installed
umount /mnt/newdata
sync
echo "Rudolf OS is installed on $DEV. Remove the disc or USB drive and restart."
