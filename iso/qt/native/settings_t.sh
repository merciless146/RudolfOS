#!/bin/sh
# 1.6.2 (docs/plan-1.6.2.md Part 39): the native Settings against the web one (settings_t.js: each page done compared light and dark, two
# accent colours, its checklist, Search in Settings, its memory), rudolf-settings built as the release builds it (iso/sys/ui/build.sh). Needs
# Xvfb, xdotool, ImageMagick, python3 with PIL, node with Playwright's Chromium, gcc and GTK 3's headers.   sh settings_t.sh [OUT] [CASES] [RUDOLF-SETTINGS]
# (ports 8690-8699: 8693; display :93)
HERE=$(cd "$(dirname "$0")" && pwd); R=$(cd "$HERE/../../.." && pwd); T=$(mktemp -d /tmp/set_t.XXXXXX); PORT=${SET_PORT:-8693}; DSP=${SET_DSP:-:93}
OUT=${1:-$T/out}; ONLY=$2; BIN=$3
command -v Xvfb >/dev/null && command -v xdotool >/dev/null && command -v import >/dev/null || { echo "SKIP: needs Xvfb, xdotool and ImageMagick"; exit 0; }
if [ -z "$BIN" ]; then BIN=$T/rudolf-settings
	sh $R/iso/sys/ui/build.sh $T/ui $BIN settings 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
	[ -s $T/cc ] && { echo "FAIL rudolf-settings builds with warnings:"; cat $T/cc; exit 1; }
fi
# (the native shell, for the case that starts it from there: built here when WebKitGTK's headers are)
if [ -z "$SET_NATIVE" ] && pkg-config --exists webkit2gtk-4.1 2>/dev/null; then gcc -O2 -Wno-deprecated-declarations -o $T/rudolf-native $R/iso/sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/ncc && export SET_NATIVE=$T/rudolf-native || cat $T/ncc; fi
export FONTCONFIG_FILE=$HERE/fonts-test.conf; D=$T/d; mkdir -p $D; node $R/iso/qt/host/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
python3 - "$D" <<'Y'
import json, sys; D = sys.argv[1]; k = json.load(open(D + '/keys.json'))['Ann']
json.dump({'ok': True, 'id': k['id'], 'n': 'Ann', 'r': 'admin', 'pk': k['pk'], 'fk': k['fk']}, open(D + '/session', 'w'))
Y
python3 $HERE/same_rd.py $PORT $D > $T/rd.log 2>&1 & RDP=$!
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
trap 'kill $RDP $XV 2>/dev/null; rm -rf $T/d $T/ui $T/cc $T/ncc $T/rudolf-native' EXIT
i=0; while [ $i -lt 40 ] && ! DISPLAY=$DSP xdotool getmouselocation >/dev/null 2>&1; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
node $HERE/settings_t.js $BIN $D $OUT $PORT $DSP $ONLY; r=$?
echo "pictures: $OUT"; exit $r
