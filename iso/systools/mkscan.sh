#!/bin/sh
# Rudolf OS 1.6.2 (docs/plan-1.6.2.md Part 34): scanning's files in the base layer, from the build machine's Ubuntu 24.04
# packages libsane1 and sane-airscan (ci_release.sh and mkrootfs.sh run it; the scanners' tests too):
#   usr/lib/MA/libsane.so.1                 SANE's dll, which rudolf-scan opens with dlopen
#   usr/lib/MA/sane/libsane-airscan.so.1    eSCL and WSD (step 1)
#   usr/lib/MA/sane/libsane-BACKEND.so.1    step 3: a small set of SANE's own drivers for older USB scanners, chosen by
#                                           what is common, every one GPL-2.0-or-later with SANE's exception (Ubuntu's
#                                           copyright file): pixma (Canon PIXMA, MAXIFY, imageCLASS/i-SENSYS, CanoScan),
#                                           genesys (Canon CanoScan LiDE, Plustek OpticBook, Visioneer, HP ScanJet G),
#                                           epson2 (Epson Perfection and older all-in-ones), epsonds (Epson DS and ES
#                                           document scanners), plustek (older Canon LiDE and Plustek OpticPro),
#                                           canon_lide70 (CanoScan LiDE 70 and 600), xerox_mfp (Samsung, Xerox and Dell
#                                           laser all-in-ones), fujitsu (Fujitsu/Ricoh fi and ScanSnap). None needs firmware
#                                           files; none needs a library the base layer does not have.
#   usr/share/rudolf/scan-backends          that list (rudolf-scan uses only these)
#   usr/share/rudolf/sane.d/BACKEND.conf    each one's settings from sane-backends, for USB only: its network and SCSI
#                                           lines left out, pixma's networking off (none of them looks on the network)
#   usr/share/rudolf/scan-usb-ids           the USB scanners SANE's drivers know (VVVV:PPPP a line, from its
#                                           20-sane.hwdb): /usr/lib/rudolf/scanusb gives only these to the account's user
#                                           while it scans, and rudolf-scan find lists them
# Prints the kilobytes added; fails when a package is missing or a library they need is in neither ROOT nor here.
#   sh mkscan.sh ROOT [BASEROOT]     (BASEROOT: where to look for the libraries they need, ROOT itself by default)
# GPL-2.0-or-later.
set -e
ROOT=$1; BASE=${2:-$1}; [ -d "$ROOT" ] || { echo "usage: mkscan.sh ROOT [BASEROOT]"; exit 2; }
MA=$(gcc -print-multiarch 2>/dev/null || true); [ -n "$MA" ] || MA=x86_64-linux-gnu
BACKENDS="pixma genesys epson2 epsonds plustek canon_lide70 xerox_mfp fujitsu"
L=$(dpkg -L libsane1 2>/dev/null | grep -E '/libsane\.so\.1$' | head -1); A=$(dpkg -L sane-airscan 2>/dev/null | grep -E '/sane/libsane-airscan\.so\.1$' | head -1)
HW=$(dpkg -L libsane1 2>/dev/null | grep -E '/20-sane\.hwdb$' | head -1); CD=$(dpkg -L libsane-common 2>/dev/null | grep -E '/sane\.d/dll\.conf$' | head -1); CD=${CD%/dll.conf}
[ -n "$L" ] && [ -n "$A" ] && [ -n "$HW" ] && [ -d "$CD" ] || { echo "mkscan: libsane1, libsane-common or sane-airscan is missing on the build machine (apt-get install libsane1 sane-airscan)"; exit 1; }
SD=$(dirname "$A"); mkdir -p "$ROOT/usr/lib/$MA/sane" "$ROOT/usr/share/rudolf/sane.d"
B=0; add() { cp -L "$1" "$2"; B=$((B + $(stat -L -c %s "$1"))); }
add "$L" "$ROOT/usr/lib/$MA/libsane.so.1"; add "$A" "$ROOT/usr/lib/$MA/sane/libsane-airscan.so.1"
: > "$ROOT/usr/share/rudolf/scan-backends.new"
for b in $BACKENDS; do
	[ -f "$SD/libsane-$b.so.1" ] || { echo "mkscan: SANE's $b driver is missing ($SD/libsane-$b.so.1)"; exit 1; }
	add "$SD/libsane-$b.so.1" "$ROOT/usr/lib/$MA/sane/libsane-$b.so.1"; echo "$b" >> "$ROOT/usr/share/rudolf/scan-backends.new"
	{ echo "# $b's settings from sane-backends ($CD/$b.conf), for USB only (Rudolf OS: mkscan.sh)"
	  [ "$b" = pixma ] && echo "networking=no"
	  [ -f "$CD/$b.conf" ] && grep -vE '^[[:space:]]*(net|tcp|scsi|bjnp|networking)([[:space:]]|=|$)' "$CD/$b.conf" | grep -vE '^[[:space:]]*#|^[[:space:]]*$' || true; } > "$ROOT/usr/share/rudolf/sane.d/$b.conf"
	B=$((B + $(stat -c %s "$ROOT/usr/share/rudolf/sane.d/$b.conf")))
done
mv "$ROOT/usr/share/rudolf/scan-backends.new" "$ROOT/usr/share/rudolf/scan-backends"
sed -n 's/^usb:v\([0-9A-Fa-f]\{4\}\)p\([0-9A-Fa-f]\{4\}\).*/\1:\2/p' "$HW" | tr 'a-f' 'A-F' | sort -u > "$ROOT/usr/share/rudolf/scan-usb-ids"
B=$((B + $(stat -c %s "$ROOT/usr/share/rudolf/scan-usb-ids")))
# every library they link: in the base layer, or in ROOT
miss=; for f in "$ROOT/usr/lib/$MA/libsane.so.1" "$ROOT"/usr/lib/$MA/sane/*.so.1; do
	for l in $(objdump -p "$f" | awk '/NEEDED/{print $2}'); do [ -e "$BASE/usr/lib/$MA/$l" ] || [ -e "$BASE/lib/$MA/$l" ] || [ -e "$ROOT/usr/lib/$MA/$l" ] || [ -e "$BASE/usr/lib/$l" ] || [ -e "$BASE/lib64/$l" ] || [ -e "$BASE/usr/lib64/$l" ] || miss="$miss ${f##*/}:$l"; done; done
[ -z "$miss" ] || { echo "mkscan: libraries the base layer does not have:$miss"; exit 1; }
echo "$((B / 1024)) KB: libsane, sane-airscan, $(echo $BACKENDS | wc -w) drivers ($BACKENDS), $(wc -l < "$ROOT/usr/share/rudolf/scan-usb-ids") USB scanners' ids"
