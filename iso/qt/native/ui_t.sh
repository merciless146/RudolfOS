#!/bin/sh
# 1.6.2 (docs/plan-1.6.2.md Part 39, step 1): librudolf-ui's look against the page's (ui_t.js): its sample window (iso/sys/ui/ui-sample.c)
# built here as the release builds it (mkui.py from the page's styles, then gcc), the desktop page in Playwright's Chromium, the same
# rudolfd stand-in (same_rd.py) for both. Needs Xvfb, ImageMagick (import), python3 with PIL, node with Playwright's Chromium, gcc and
# GTK 3's headers.   sh ui_t.sh [OUT] [CASES] [SAMPLE]       (ports 8680-8689, display :88)
HERE=$(cd "$(dirname "$0")" && pwd); R=$(cd "$HERE/../../.." && pwd); T=$(mktemp -d /tmp/ui_t.XXXXXX); PORT=${UI_PORT:-8681}; DSP=${UI_DSP:-:88}
OUT=${1:-$T/out}; ONLY=$2; BIN=$3
command -v Xvfb >/dev/null && command -v import >/dev/null || { echo "SKIP: needs Xvfb and ImageMagick"; exit 0; }
python3 -c 'import PIL' 2>/dev/null || { echo "SKIP: needs python3's PIL"; exit 0; }
if [ -z "$BIN" ]; then BIN=$T/ui-sample
	sh $R/iso/sys/ui/build.sh $T/ui $BIN ui-sample 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
	[ -s $T/cc ] && { echo "FAIL the sample builds with warnings:"; cat $T/cc; exit 1; }
fi
export FONTCONFIG_FILE=$HERE/fonts-test.conf; D=$T/d; mkdir -p $D; node $R/iso/qt/host/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
# (Native's side is signed in as Ann: the stand-in's session file, as its desktop/login writes it)
python3 - "$D" <<'Y'
import json, sys; D = sys.argv[1]; k = json.load(open(D + '/keys.json'))['Ann']
json.dump({'ok': True, 'id': k['id'], 'n': 'Ann', 'r': 'admin', 'pk': k['pk'], 'fk': k['fk']}, open(D + '/session', 'w'))
Y
python3 $HERE/same_rd.py $PORT $D > $T/rd.log 2>&1 & RDP=$!
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
trap 'kill $RDP $XV 2>/dev/null; rm -rf $T/d $T/ui $T/cc' EXIT
i=0; while [ $i -lt 40 ] && ! DISPLAY=$DSP xdpyinfo >/dev/null 2>&1; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
node $HERE/ui_t.js $BIN $D $OUT $PORT $DSP $ONLY; r=$?
echo "pictures: $OUT"; exit $r
