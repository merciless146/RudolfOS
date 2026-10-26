#!/bin/bash
# Make a new Rudolf OS disc image from an earlier one and a newer system release, without rebuilding everything:
# the kernel (BOOT/VMLINUZ and the EFI image), the four system layers and RUDOLF/RELEASE come from the release;
# packages, licences and source archives come from the earlier disc (the Rudolf OS source is refreshed).
# Used by the release workflow (input "disc"). GPL-2.0-or-later.
#
#   mkdisc.sh OLD.iso RELEASE-DIR VERSION OUT-DIR
# RELEASE-DIR holds manifest.txt, manifest.sig and vmlinuz, base.sfs, chromium.sfs, desktop.sfs, modules.sfs.
# SB_PARTS, SB_KEY (and SB_CERT, SBSIGN): the Secure Boot parts and signing key (systools/mkefi.sh).
# Writes OUT-DIR/rudolf-os-VERSION.iso and its .sha256, and (1.2.8) the boot image rudolf-os-VERSION-boot.iso and its .sha256.
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
# the UEFI boot image with shim, the menu and the signed kernel bundle (Secure Boot, 1.2.2): SB_PARTS (the files
# of the draft release sb-parts) and SB_KEY are needed, see systools/mkefi.sh
bash "$HERE/systools/mkefi.sh" "$REL/vmlinuz" "$VER" "${SB_PARTS:?SB_PARTS: the folder with the Secure Boot parts}" "$T/EFI/EFIBOOT.IMG"
nasm -f bin -o "$T/BOOT/RBOOT.BIN" "$HERE/boot/rboot.asm"
# the signed release list of these layers: the start-up checks every block against its hashes
[ -f "$REL/manifest.sig" ] || { echo "$REL/manifest.sig is missing (the start-up needs the signed release list)"; exit 1; }
cp "$REL/manifest.txt" "$T/RUDOLF/MANIFEST.TXT"; cp "$REL/manifest.sig" "$T/RUDOLF/MANIFEST.SIG"
[ -d "$SB_PARTS/licences" ] && for f in "$SB_PARTS"/licences/*.copyright; do case $(basename "$f") in shim*) n=SHIM;; systemd*) n=SDBOOT;; *) continue;; esac; sed 's/$/\r/' "$f" > "$T/LICENSES/$n.TXT"; done
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
python3 "$HERE/mkimage.py" iso "$OUT/rudolf-os-$VER.iso" "$T" --mbr "$B/mbr.bin" --boot BOOT/RBOOT.BIN --kernel BOOT/VMLINUZ --efi EFI/EFIBOOT.IMG --label "RUDOLF_OS_${VER//./_}" --sums RUDOLF/SHA256
# the boot image (1.2.8): the same boot files without the system layers, sources and licence texts (which stay in
# the disc image and the source archive of the same release), small enough to fit in front of RUDOLFDATA on every
# drive Install here has made. Settings > Security > Secure Boot > Get new boot files writes it there (bootfiles.sh);
# the system then comes from the installed update. RUDOLF/BOOTONLY marks it for the start-up.
echo "== boot image"
BT=$(realpath -m "$OUT/btree"); rm -rf "$BT"; cp -a "$T" "$BT"; rm -f "$BT"/RUDOLF/*.SFS; rm -rf "$BT/SOURCE" "$BT/LICENSES"
printf 'Rudolf OS %s boot files only: the system comes from the update installed on the drive (RUDOLFDATA).\r\n' "$VER" > "$BT/RUDOLF/BOOTONLY"
printf 'Licences and source: rudolf-os-%s.iso and rudolf-os-%s-source.tar.xz in the same release.\r\n' "$VER" "$VER" > "$BT/LICENSES.TXT"
(cd "$BT" && find . -type f ! -path ./RUDOLF/SHA256 | sed 's#^\./##' | sort | while read -r f; do l=$(echo "$f" | tr 'A-Z' 'a-z'); echo "$(sha256sum "$f" | cut -d' ' -f1)  $l"; done) > "$BT/RUDOLF/SHA256"
python3 "$HERE/mkimage.py" iso "$OUT/rudolf-os-$VER-boot.iso" "$BT" --mbr "$B/mbr.bin" --boot BOOT/RBOOT.BIN --kernel BOOT/VMLINUZ --efi EFI/EFIBOOT.IMG --label "RUDOLF_BOOT_${VER//./_}" --sums RUDOLF/SHA256
rm -rf "$BT"
rm -rf "$B" "$T"
(cd "$OUT" && sha256sum "rudolf-os-$VER.iso" > "rudolf-os-$VER.iso.sha256" && sha256sum "rudolf-os-$VER-boot.iso" > "rudolf-os-$VER-boot.iso.sha256")
ls -la "$OUT"
