#!/bin/sh
# Rudolf OS 1.6.3 (docs/plan-1.6.3.md Part 2): fingerprint readers' files in the base layer, from the build machine's
# Ubuntu 24.04 libfprint-2-2 (ci_release.sh and mkrootfs.sh run it; installed on the build machine with libfprint-2-dev,
# for the fprint helper's build, when missing):
#   usr/lib/MA/libfprint-2.so.2 and what it links that ROOT does not have yet (libgusb, libgudev, json-glib, Ubuntu's
#                                     libfprint-2-tod: the base layer's GLib, NSS, pixman and libusb are shared)
#   usr/share/rudolf/libfprint-unsupported   the USB readers libfprint knows it does not support (VVVV:PPPP a line, from
#                                     its own hwdb's "Known unsupported devices"): Settings › Users names one plainly
# No daemon: fprintd is not shipped (the fprint root helper talks to libfprint itself, for rudolfd only), and nothing
# here listens. Prints what it added ("N KB"); fails when the package is missing or over the budget, 3 MB.
#   sh mkfprint.sh ROOT     (as root: ROOT is the base layer's tree)
# GPL-2.0-or-later.
set -e
ROOT=$1; [ -d "$ROOT" ] || { echo "usage: mkfprint.sh ROOT"; exit 2; }
MA=$(gcc -print-multiarch 2>/dev/null || true); [ -n "$MA" ] || MA=x86_64-linux-gnu
if ! dpkg -s libfprint-2-dev >/dev/null 2>&1 || ! dpkg -s libfprint-2-2 >/dev/null 2>&1; then
	{ apt-get update -qq && apt-get install -y -qq libfprint-2-2 libfprint-2-dev; } >/dev/null 2>&1 || { echo "mkfprint: libfprint-2-2 and libfprint-2-dev could not be installed on the build machine"; exit 1; }
fi
L=$(dpkg -L libfprint-2-2 2>/dev/null | grep -E '/libfprint-2\.so\.2$' | head -1)
HW=$(dpkg -L libfprint-2-2 2>/dev/null | grep -E '/60-autosuspend-libfprint-2\.hwdb$' | head -1)
[ -n "$L" ] && [ -n "$HW" ] || { echo "mkfprint: libfprint-2-2 is missing on the build machine (apt-get install libfprint-2-2 libfprint-2-dev)"; exit 1; }
B=0
for x in $L $(ldd "$L" | awk '/=>/{print $3}'); do
	n=$(basename "$x")
	[ -e "$ROOT$x" ] || [ -e "$ROOT/usr$x" ] || [ -e "$ROOT/usr/lib/$MA/$n" ] || [ -e "$ROOT/lib/$MA/$n" ] && continue
	mkdir -p "$ROOT/usr/lib/$MA"; cp -L "$x" "$ROOT/usr/lib/$MA/$n"; B=$((B + $(stat -L -c %s "$x")))
done
mkdir -p "$ROOT/usr/share/rudolf"
awk '/^# Known unsupported devices/{f=1;next} f&&/^usb:v[0-9A-Fa-f]{4}p[0-9A-Fa-f]{4}\*$/{print tolower(substr($0,6,4)) ":" tolower(substr($0,11,4))} f&&/^ /{exit}' "$HW" > "$ROOT/usr/share/rudolf/libfprint-unsupported"
[ -s "$ROOT/usr/share/rudolf/libfprint-unsupported" ] || { echo "mkfprint: libfprint's list of unsupported readers was not found in $HW"; exit 1; }
B=$((B + $(stat -c %s "$ROOT/usr/share/rudolf/libfprint-unsupported")))
[ $B -lt 3145728 ] || { echo "mkfprint: libfprint adds $((B / 1024)) KB to the base layer: over its 3 MB budget (docs/plan-1.6.3.md Part 2)"; exit 1; }
echo "$((B / 1024)) KB"
