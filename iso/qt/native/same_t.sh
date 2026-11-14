#!/bin/sh
# 1.6.0 (plan Part 12, "The rule": Native appears exactly as Standard): the same state drawn by both engines, compared
# part by part (same_t.js: the wallpaper, the taskbar in its five styles, its buttons' styles and the accent colour's
# modes, the start button, the pinned apps, the tray, the clock, the desktop's icons, the start menu, the title bars in
# their five styles, the sign-in and lock screens). A part fails over 2 % of its pixels differing beyond a small
# tolerance (SAME_LIMIT, SAME_TOL). The pictures (each part from both engines and the difference) are kept in OUT.
# Needs Xvfb, xdotool, ImageMagick (import), python3 with PIL, node with Playwright's Chromium, gcc and WebKitGTK's
# headers; faketime if there (the clock at the same minute in both).   sh same_t.sh [OUT] [CASE,...] [RUDOLF-NATIVE]
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/same_t.XXXXXX); PORT=${SAME_PORT:-8301}; DSP=${SAME_DSP:-:83}
OUT=${1:-$T/out}; ONLY=$2; BIN=$3
command -v Xvfb >/dev/null && command -v xdotool >/dev/null && command -v import >/dev/null || { echo "SKIP: needs Xvfb, xdotool and ImageMagick"; exit 0; }
python3 -c 'import PIL' 2>/dev/null || { echo "SKIP: needs python3's PIL"; exit 0; }
if [ -z "$BIN" ]; then BIN=$T/rudolf-native
	gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $BIN $HERE/../../sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
fi
D=$T/d; mkdir -p $D; node $HERE/../host/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
FT=$(ls /usr/lib/x86_64-linux-gnu/faketime/libfaketime.so.1 /usr/lib/*/faketime/libfaketime.so.1 2>/dev/null | head -1)
python3 $HERE/same_rd.py $PORT $D > $T/rd.log 2>&1 & RDP=$!
Xvfb $DSP -screen 0 1920x1080x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
trap 'kill $RDP $XV 2>/dev/null; pkill -f "$T/" 2>/dev/null; rm -rf $T/d $T/rudolf-native $T/cc' EXIT
i=0; while [ $i -lt 40 ] && ! DISPLAY=$DSP xdotool getmouselocation >/dev/null 2>&1; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
SAME_FAKETIME=$FT node $HERE/same_t.js $BIN $D $OUT $PORT $DSP $ONLY; r=$?
echo "pictures: $OUT"; exit $r
