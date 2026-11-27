#!/bin/sh
# Rudolf OS 1.6.3 (docs/plan-1.6.3.md Part 9): PipeWire and WirePlumber in the base layer, beside PulseAudio, "from the
# build machine's Ubuntu 24.04 packages (installed on demand, as mkfprint.sh's)":
#   usr/bin/pipewire             the daemon (a capture source for WebKitGTK 2.52, which captures only through PipeWire)
#   usr/bin/wireplumber          its session manager, with the ALSA monitor off (systools/pwconf): PulseAudio keeps
#                                the sound cards; the v4l2 and libcamera monitors publish the camera
#   usr/lib/MA/libpipewire-0.3.so.0*, usr/lib/MA/pipewire-0.3/, usr/lib/MA/spa-0.2/
#                                the library, its modules and the SPA plug-ins (v4l2, support, alsa as a library only)
#   usr/lib/MA/wireplumber-*/ , usr/share/pipewire/, usr/share/wireplumber/
#                                WirePlumber's modules and both packages' own configuration
#   etc/wireplumber/             the drop-in that keeps WirePlumber off the sound cards (51-no-alsa.lua, 0.4's Lua;
#                                wireplumber.conf.d/51-no-alsa.conf, 0.5's) and etc/wireplumber/wireplumber.conf.d
# No pipewire-pulse: PulseAudio stays the sound server (the owner's choice, 2026-11-27). Nothing here listens.
# Prints what it added ("N KB"); fails, named, when a piece is missing or over the budget (12 MB).
#   sh mkpipewire.sh ROOT     (as root: ROOT is the base layer's tree)
# GPL-2.0-or-later.
set -e
ROOT=$1; [ -d "$ROOT" ] || { echo "usage: mkpipewire.sh ROOT"; exit 2; }
MA=$(gcc -print-multiarch 2>/dev/null || true); [ -n "$MA" ] || MA=x86_64-linux-gnu
HERE=$(cd "$(dirname "$0")" && pwd)
PKGS="pipewire pipewire-bin wireplumber libpipewire-0.3-0t64 libspa-0.2-modules"
have() { dpkg -s "$1" >/dev/null 2>&1; }
if ! have pipewire-bin || ! have wireplumber; then
	{ apt-get update -qq && apt-get install -y -qq $PKGS; } >/dev/null 2>&1 || true
fi
for p in $PKGS; do have "$p" || { echo "mkpipewire: $p could not be installed on the build machine (apt-get install $PKGS)"; exit 1; }; done
L=$(ls /usr/lib/$MA/libpipewire-0.3.so.0* 2>/dev/null | head -1)
[ -n "$L" ] || L=$(dpkg -L libpipewire-0.3-0t64 2>/dev/null | grep -E '/libpipewire-0\.3\.so\.0\.[0-9]+$' | head -1)
PW=$(command -v pipewire || true); WP=$(command -v wireplumber || true)
[ -n "$PW" ] || { echo "mkpipewire: /usr/bin/pipewire is missing (apt-get install pipewire-bin)"; exit 1; }
[ -n "$WP" ] || { echo "mkpipewire: /usr/bin/wireplumber is missing (apt-get install wireplumber)"; exit 1; }
[ -n "$L" ] || { echo "mkpipewire: libpipewire-0.3.so.0 is missing (apt-get install libpipewire-0.3-0t64)"; exit 1; }
[ -d /usr/lib/$MA/spa-0.2 ] || { echo "mkpipewire: the SPA plug-ins are missing (apt-get install libspa-0.2-modules)"; exit 1; }
# WebKitGTK 2.52 needs PipeWire 0.3.64 or newer; the camera cannot be captured under an older one
V=$(pipewire --version 2>/dev/null | sed -n 's/^.*PipeWire *//p' | head -1)
[ -n "$V" ] || { echo "mkpipewire: pipewire --version said nothing"; exit 1; }
awk -v v="$V" 'BEGIN{n=split(v,a,"."); if (a[1]+0 < 0 || (a[1]+0 == 0 && (a[2]+0 < 3 || (a[2]+0 == 3 && a[3]+0 < 64)))) exit 1}' || { echo "mkpipewire: PipeWire $V is older than 0.3.64, which WebKitGTK 2.52's camera needs"; exit 1; }
B=0
copy() {  # (a file, then what it links that the layer lacks: as mkfprint.sh does)
	x=$1
	if [ ! -e "$x" ]; then return 0; fi
	n=$(basename "$x")
	if [ -e "$ROOT$x" ]; then return 0; fi
	case "$x" in /usr/*) d=$ROOT$x;; *) d=$ROOT/usr/bin/$n;; esac
	mkdir -p "$(dirname "$d")" || { echo "mkpipewire: could not make $(dirname "$d")"; exit 1; }
	cp -L "$x" "$d" || { echo "mkpipewire: could not copy $x"; exit 1; }
}
for x in $PW $WP "$L"; do copy "$x"; for d in $(ldd "$x" 2>/dev/null | awk '/=>/{print $3}'); do
	if [ -e "$ROOT$d" ] || [ -e "$ROOT/usr/lib/$MA/$(basename "$d")" ]; then continue; fi
	copy "$d"; done; done
for d in /usr/lib/$MA/pipewire-0.3 /usr/lib/$MA/spa-0.2 /usr/share/pipewire /usr/share/wireplumber; do
	[ -d "$d" ] || continue
	(cd "$d" && find . -type f) | while read -r f; do copy "$d/$f"; done
done
for d in /usr/lib/$MA/wireplumber-* ; do
	[ -d "$d" ] || continue
	(cd "$d" && find . -type f) | while read -r f; do copy "$d/$f"; done
done
# (the copy() above runs in a subshell in the pipes: sum the sizes again rather than lose them)
B=$(find "$ROOT/usr/lib/$MA/pipewire-0.3" "$ROOT/usr/lib/$MA/spa-0.2" "$ROOT/usr/share/pipewire" "$ROOT/usr/share/wireplumber" "$ROOT/usr/bin/pipewire" "$ROOT/usr/bin/wireplumber" -type f 2>/dev/null -printf '%s\n' | awk '{s+=$1} END{print s+0}')
for d in "$ROOT"/usr/lib/$MA/wireplumber-*; do if [ -d "$d" ]; then B=$((B + $(find "$d" -type f -printf '%s\n' | awk '{s+=$1} END{print s+0}'))); fi; done
# the configuration that keeps WirePlumber off the sound cards (both of its dialects: 0.4's Lua, 0.5's conf)
for f in "$ROOT/usr/bin/pipewire" "$ROOT/usr/bin/wireplumber" "$ROOT/usr/lib/$MA/libpipewire-0.3.so.0" "$ROOT/usr/lib/$MA/spa-0.2"; do
	[ -e "$f" ] || { echo "mkpipewire: $f was not copied: the packages are not what this script knows"; exit 1; }
done
mkdir -p "$ROOT/etc/wireplumber/main.lua.d" "$ROOT/etc/wireplumber/wireplumber.conf.d"
cp -L "$HERE/pwconf/51-no-alsa.lua" "$ROOT/etc/wireplumber/main.lua.d/51-no-alsa.lua"
cp -L "$HERE/pwconf/51-no-alsa.conf" "$ROOT/etc/wireplumber/wireplumber.conf.d/51-no-alsa.conf"
[ $B -lt 12582912 ] || { echo "mkpipewire: PipeWire and WirePlumber add $((B / 1024)) KB to the base layer: over its 12 MB budget (docs/plan-1.6.3.md Part 9)"; exit 1; }
echo "$((B / 1024)) KB"
