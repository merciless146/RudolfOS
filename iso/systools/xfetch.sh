#!/bin/bash
# xfetch.sh COMMIT: the X server's sources for xshim.sh and xbuild.sh, from github.com/NetBSD/xsrc (unmodified
# upstream releases) at COMMIT, only the parts they use, into src/xorg/xsrc (1.4.3: the release build rebuilds the
# X server when its build changes; before, only full local builds did). GPL-2.0-or-later.
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd); D=$HERE/src/xorg/xsrc; REV=$1
[ -n "$REV" ] || { echo "usage: xfetch.sh COMMIT"; exit 2; }
PARTS='/external/mit/libdrm/dist/*.h
/external/mit/libdrm/dist/include/
/external/mit/libdrm/dist/meson.build
/external/mit/libxcvt/dist/
/external/mit/libpciaccess/dist/include/
/external/mit/libpciaccess/dist/meson.build
/external/mit/libxkbfile/dist/include/
/external/mit/libxkbfile/dist/meson.build
/external/mit/libXfont2/dist/include/
/external/mit/libXfont2/dist/configure.ac
/external/mit/MesaLib/dist/src/gbm/main/gbm.h
/external/mit/MesaLib/dist/VERSION
/external/mit/MesaLib/dist/include/EGL/
/external/mit/MesaLib/dist/include/KHR/
/external/mit/MesaLib/dist/include/GL/
/external/mit/libepoxy/dist/
/external/mit/xorg-server/dist/'
if [ -d "$D/.git" ] && [ "$(git -C "$D" rev-parse HEAD 2>/dev/null)" = "$REV" ]; then
	echo "$PARTS" | git -C "$D" sparse-checkout set --no-cone --stdin; echo "X sources: $REV (already here)"; exit 0
fi
rm -rf "$D"; mkdir -p "$D"; git -C "$D" init -q
git -C "$D" remote add origin https://github.com/NetBSD/xsrc
git -C "$D" config remote.origin.promisor true; git -C "$D" config remote.origin.partialclonefilter blob:none
echo "$PARTS" | git -C "$D" sparse-checkout set --no-cone --stdin
for i in 1 2 3; do git -C "$D" fetch -q --depth 1 --filter=blob:none origin "$REV" && git -C "$D" checkout -q FETCH_HEAD && break; sleep 20; done
[ "$(git -C "$D" rev-parse HEAD)" = "$REV" ] || { echo "the X sources ($REV) could not be fetched"; exit 1; }
echo "X sources: $REV"
