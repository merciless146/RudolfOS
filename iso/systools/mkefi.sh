#!/bin/bash
# Make the UEFI boot image of a Rudolf OS disc (Rudolf OS 1.2.2), Secure Boot ready:
#   EFI/BOOT/BOOTX64.EFI   shim (signed by Microsoft; Ubuntu's)  - starts grubx64.efi if its signature is trusted
#   EFI/BOOT/mmx64.efi     MokManager: where the Rudolf OS key is enrolled the first time
#   EFI/BOOT/grubx64.efi   systemd-boot, the boot menu, signed with the Rudolf OS key (shim starts this name)
#   EFI/rudolf/rudolf.efi  the kernel bundle (systemd stub + the kernel + its fixed options), signed likewise
#   EFI/rudolf/rudolf.cer  the Rudolf OS certificate, to choose in MokManager ("Enroll key from disk")
#   loader/                the menu (iso/boot/sb/loader)
# With Secure Boot off, shim starts the menu without checking anything, as before.
#   mkefi.sh VMLINUZ VERSION PARTS-DIR OUT.IMG
# PARTS-DIR: the files of the draft release sb-parts (shimx64.efi, mmx64.efi, systemd-bootx64.efi,
# linuxx64.efi.stub), checked against iso/boot/sb/parts.txt. SB_KEY / SB_CERT: the signing key and its
# certificate (SB_CERT defaults to iso/boot/sb/rudolf-sb.crt); SBSIGN: the sbsign program. GPL-2.0-or-later.
set -e
K=$1; VER=$2; P=$3; OUT=$4
[ -f "$K" ] && [ -n "$VER" ] && [ -d "$P" ] && [ -n "$OUT" ] || { echo "usage: $0 VMLINUZ VERSION PARTS-DIR OUT.IMG"; exit 1; }
HERE=$(cd "$(dirname "$0")/.." && pwd)
SB_CERT=${SB_CERT:-$HERE/boot/sb/rudolf-sb.crt}; SBSIGN=${SBSIGN:-sbsign}
[ -n "$SB_KEY" ] && [ -f "$SB_KEY" ] || { echo "SB_KEY: the Secure Boot signing key is needed"; exit 1; }
python3 "$HERE/systools/sbkeycheck.py" "$SB_KEY" "$SB_CERT"
(cd "$P" && grep -v '^#' "$HERE/boot/sb/parts.txt" | sha256sum -c --quiet) || { echo "the Secure Boot parts in $P are not the pinned ones (iso/boot/sb/parts.txt)"; exit 1; }
T=$(mktemp -d)
"$SBSIGN" --key "$SB_KEY" --cert "$SB_CERT" --output "$T/grubx64.efi" "$P/systemd-bootx64.efi" >/dev/null
python3 "$HERE/systools/mkuki.py" "$P/linuxx64.efi.stub" "$K" "$VER" "$T/uki.efi" >/dev/null
"$SBSIGN" --key "$SB_KEY" --cert "$SB_CERT" --output "$T/rudolf.efi" "$T/uki.efi" >/dev/null
openssl x509 -in "$SB_CERT" -outform DER -out "$T/rudolf.cer"
L=$HERE/boot/sb/loader
F=("$P/shimx64.efi:EFI/BOOT/BOOTX64.EFI" "$P/mmx64.efi:EFI/BOOT/mmx64.efi" "$T/grubx64.efi:EFI/BOOT/grubx64.efi"
   "$T/rudolf.efi:EFI/rudolf/rudolf.efi" "$T/rudolf.cer:EFI/rudolf/rudolf.cer" "$L/loader.conf:loader/loader.conf")
for e in "$L"/entries/*.conf; do F+=("$e:loader/entries/$(basename "$e")"); done
python3 "$HERE/mkimage.py" fat "$OUT" "${F[@]}"
rm -rf "$T"
