#!/bin/sh
# Rudolf OS 1.6.3 (docs/plan-1.6.3.md Part 9): what a base layer itself must carry for the Native desktop to capture --
# not what the tree it was made from had, and not that mkrootfs.sh ran mkpipewire.sh. Asked of the packed layer, so a
# build that copied the files and then lost them in mklayers.sh (or repacked the wrong tree) is caught here.
#   usr/bin/pipewire                                  the daemon (WebKitGTK 2.52 captures only through PipeWire)
#   usr/bin/wireplumber                               its session manager (the camera's monitor lives in it)
#   usr/lib/MA/spa-0.2/v4l2/libspa-v4l2.so            the camera's SPA plug-in
#   etc/wireplumber/.../51-no-alsa.{lua,conf}         the drop-in that keeps WirePlumber off the sound cards
#   and no usr/bin/pipewire-pulse                     PulseAudio stays the sound server (the owner's choice, 2026-11-27)
#   sh layercheck.sh BASE.SFS
# Prints PASS with the sizes, or FAIL naming what is missing; exits 1 then, 2 when the image cannot be read.
# Needs unsquashfs (apt-get install squashfs-tools). GPL-2.0-or-later.
set -e
S=$1; [ -f "$S" ] || { echo "usage: layercheck.sh BASE.SFS"; exit 2; }
command -v unsquashfs >/dev/null 2>&1 || { echo "layercheck: unsquashfs is not installed here (apt-get install squashfs-tools)"; exit 2; }
L=$(mktemp); trap 'rm -f "$L"' EXIT
unsquashfs -lls "$S" > "$L" 2>/dev/null || { echo "layercheck: $S is not a squashfs image"; exit 2; }
bad=
# a regular, executable file in the layer (mkpipewire.sh copies with cp -L, so there is no symlink to follow)
exe() { grep -qE "^-..x.* squashfs-root$1\$" "$L" || bad="$bad $2"; }
any() { grep -qE " squashfs-root$1\$" "$L" || bad="$bad $2"; }
exe /usr/bin/pipewire "pipewire (the daemon)"
exe /usr/bin/wireplumber "wireplumber (the session manager)"
any '/usr/lib/[^/]+/spa-0\.2/v4l2/libspa-v4l2\.so' "the v4l2 SPA plug-in (libspa-v4l2.so)"
any '/etc/wireplumber/.*/51-no-alsa\.(lua|conf)' "the 51-no-alsa drop-in (WirePlumber must not take the sound cards)"
if grep -qE ' squashfs-root/usr/bin/pipewire-pulse$' "$L"; then bad="$bad pipewire-pulse (PulseAudio must stay the sound server)"; fi
if [ -n "$bad" ]; then
	echo "layercheck: $S does not carry:$bad"
	echo "layercheck: the Native desktop cannot capture with this base layer (docs/plan-1.6.3.md Part 9): mkpipewire.sh adds"
	echo "layercheck: them in mkrootfs.sh and ci_release.sh; a layer made another way must add them too."
	exit 1
fi
echo "PASS $(basename "$S") carries PipeWire's daemon, WirePlumber, the camera's v4l2 SPA plug-in and the 51-no-alsa"
echo "     drop-in, and no pipewire-pulse"
