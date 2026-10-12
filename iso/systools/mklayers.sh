#!/bin/bash
# Split the root file system into the layers Rudolf OS mounts at start-up and updates one by one.
#   mklayers.sh ROOTFS OUTDIR      writes base.sfs chromium.sfs desktop.sfs modules.sfs
# Reproducible (fixed times and owners, sorted input), so a layer whose files did not change
# comes out byte-identical and an update skips it, and a changed layer differs only around what changed
# (good for delta downloads). GPL-2.0-or-later.
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
R=$(realpath "$1"); O=$(realpath -m "$2"); mkdir -p "$O"
MK=$HERE/src/squashfs-tools/squashfs-tools/mksquashfs
S=$(mktemp -d -p "$(dirname "$R")"); trap 'rm -rf "$S"' EXIT
export SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH:-1760000000}
part() { # name, paths inside the root file system
	local n=$1; shift; mkdir -p "$S/$n"
	for p in "$@"; do [ -e "$R/$p" ] || continue; mkdir -p "$S/$n/$(dirname "$p")"; cp -al "$R/$p" "$S/$n/$p"; done
}
part chromium opt/chromium
part desktop usr/share/rudolf/www
part modules usr/lib/modules
mkdir -p "$S/base"; cp -al "$R/." "$S/base/"; rm -rf "$S/base/opt/chromium" "$S/base/usr/share/rudolf/www" "$S/base/usr/lib/modules"
mkdir -p "$S/base/opt/chromium" "$S/base/usr/share/rudolf/www" "$S/base/usr/lib/modules"
for n in base chromium desktop modules; do
	rm -f "$O/$n.sfs"
	"$MK" "$S/$n" "$O/$n.sfs" -comp xz -Xbcj x86 -b 1M -noappend -all-root -quiet -no-progress
	echo "$n.sfs $(stat -c %s "$O/$n.sfs")"
done
