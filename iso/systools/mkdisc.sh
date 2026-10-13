#!/bin/bash
# Make a new Rudolf OS disc image from an earlier one and a newer system release, without rebuilding everything:
# the kernel (BOOT/VMLINUZ and the EFI image), the four system layers and RUDOLF/RELEASE come from the release;
# packages, licences and source archives come from the earlier disc (the Rudolf OS source is refreshed).
# Used by the release workflow (input "disc"). GPL-2.0-or-later.
#
#   mkdisc.sh OLD.iso RELEASE-DIR VERSION OUT-DIR
# RELEASE-DIR holds manifest.txt and vmlinuz, base.sfs, chromium.sfs, desktop.sfs, modules.sfs.
# Writes OUT-DIR/rudolf-os-VERSION.iso and its .sha256.
set -e
OLD=$1; REL=$2; VER=$3; OUT=$4
[ -f "$OLD" ] && [ -f "$REL/manifest.txt" ] && [ -n "$VER" ] && [ -n "$OUT" ] || { echo "usage: $0 OLD.iso RELEASE-DIR VERSION OUT-DIR"; exit 1; }
HERE=$(cd "$(dirname "$0")/.." && pwd)
T=$(realpath -m "$OUT/tree"); rm -rf "$T"; mkdir -p "$OUT"
echo "== the earlier disc"; python3 "$HERE/systools/iso_extract.py" "$OLD" "$T" --all
for f in vmlinuz base.sfs chromium.sfs desktop.sfs modules.sfs; do [ -f "$REL/$f" ] || { echo "$REL/$f is missing"; exit 1; }; done
echo "== kernel and system layers from release $(sed -n 's/^serial //p' "$REL/manifest.txt")"
cp "$REL/vmlinuz" "$T/BOOT/VMLINUZ"
for n in base chromium desktop modules; do cp "$REL/$n.sfs" "$T/RUDOLF/${n^^}.SFS"; done
python3 "$HERE/mkimage.py" fat "$T/EFI/EFIBOOT.IMG" "$REL/vmlinuz:EFI/BOOT/BOOTX64.EFI"
m() { sed -n "s/^$1 //p" "$REL/manifest.txt"; }
# which release this disc is (updates must be newer); the ID stays the one the kernel's start-up looks for
printf 'serial %s\nversion %s\nreleased %s\nchromium %s\nkernel %s\nnotes %s\n' "$(m serial)" "$VER" "$(m released)" "$(m chromium)" "$(m kernel)" "$VER disc release" > "$T/RUDOLF/RELEASE"
[ "$(cat "$T/RUDOLF/ID")" = "$(cat "$HERE/initramfs/etc/rudolf-id")" ] || { echo "the disc ID $(cat "$T/RUDOLF/ID") is not the kernel's $(cat "$HERE/initramfs/etc/rudolf-id")"; exit 1; }
echo "== read-me, changes and source"
cp "$HERE/README.TXT" "$T/README.TXT"
[ -f "$HERE/../CHANGELOG.md" ] && sed 's/$/\r/' "$HERE/../CHANGELOG.md" > "$T/CHANGES.TXT"
SD=$(mktemp -d); git -C "$HERE/.." archive --format=tar --prefix=rudolf-os-$VER/ HEAD desktop iso libs LICENSE README.md CHANGELOG.md | tar -x -C "$SD"
tar -C "$SD" -cJf "$T/SOURCE/RUDOLF.TXZ" --owner=0 --group=0 rudolf-os-$VER; rm -rf "$SD"
echo "== checksums"
(cd "$T" && find . -type f ! -path ./RUDOLF/SHA256 | sed 's#^\./##' | sort | while read -r f; do l=$(echo "$f" | tr 'A-Z' 'a-z'); echo "$(sha256sum "$f" | cut -d' ' -f1)  $l"; done) > "$T/RUDOLF/SHA256"
echo "== boot loader and ISO"
B=$(mktemp -d); nasm -f bin -o "$B/mbr.bin" "$HERE/boot/mbr.asm"
python3 "$HERE/mkimage.py" iso "$OUT/rudolf-os-$VER.iso" "$T" --mbr "$B/mbr.bin" --boot BOOT/RBOOT.BIN --kernel BOOT/VMLINUZ --efi EFI/EFIBOOT.IMG --label "RUDOLF_OS_${VER//./_}"
rm -rf "$B" "$T"
(cd "$OUT" && sha256sum "rudolf-os-$VER.iso" > "rudolf-os-$VER.iso.sha256")
ls -la "$OUT"
