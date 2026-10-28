# Rudolf OS 1.4.1: the partition table of a drive holding a Rudolf OS image (sourced by install.sh and bootfiles.sh;
# the start-up's "new boot files" writer does the same in initramfs/init). GPL-2.0-or-later.
#
# The disc image's own table is a hybrid one: partition 1 (the ISO 9660 file system) starts at sector 0 and holds
# partition 2 (the EFI system partition). Firmware starts a USB stick as a CD (El Torito) and never reads that table,
# but an internal drive is started from its partitions, and UEFI firmware takes overlapping partitions for an invalid
# table: the ASUS Vivobook Go did not start from its drive at all. So:
#   - an internal drive gets the installed layout: partition 1 cleared (the start-up reads the image from the whole
#     drive), partition 2 and RUDOLFDATA (partition 3) kept, and a disk signature of its own (the image's is the same
#     on every drive made from it, so a boot entry naming the drive could match a stick);
#   - a USB stick or memory card keeps (or gets back) the image's partition 1.
# rudolfd adds the drive's boot entry at the next start.

# the drive is on USB (a stick, a USB disk) or a memory card
layout_removable() { case "$(readlink -f /sys/block/${1##*/} 2>/dev/null)" in */usb*|*/mmc*) return 0;; esac; [ "$(cat /sys/block/${1##*/}/removable 2>/dev/null)" = 1 ]; }

layout_installed() {
	dd if=/dev/zero of="$1" bs=1 seek=446 count=16 conv=notrunc,fsync 2>/dev/null
	_sg=00000000
	while [ "$_sg" = 00000000 ]; do _sg=$(dd if=/dev/urandom bs=4 count=1 2>/dev/null | od -An -tx1 | tr -d ' \n'); done
	for _h in $(echo "$_sg" | sed 's/../& /g'); do printf "\\$(printf %03o 0x$_h)"; done | dd of="$1" bs=1 seek=440 conv=notrunc,fsync 2>/dev/null
	sync
}

# BLOCKS: the image's size in 2048-byte blocks. Only when partition 1 is empty (an image copied from an installed drive)
layout_stick() {
	[ "$(dd if="$1" bs=1 skip=446 count=16 2>/dev/null | od -An -tx1 | tr -d ' \n')" = 00000000000000000000000000000000 ] || return 0
	_n=$(($2 * 4))
	{ printf '\200\376\377\377\027\376\377\377\000\000\000\000'; printf "\\$(printf %03o $((_n & 255)))\\$(printf %03o $(((_n >> 8) & 255)))\\$(printf %03o $(((_n >> 16) & 255)))\\$(printf %03o $(((_n >> 24) & 255)))"; } | dd of="$1" bs=1 seek=446 conv=notrunc,fsync 2>/dev/null
	sync
}

# the right table for the drive
layout_apply() { if layout_removable "$1"; then layout_stick "$1" "$2"; else layout_installed "$1"; fi; }
