#!/bin/sh
# 1.6.3 (docs/plan-1.6.3.md Part 1): rudolf-wordpro's first step -- the editor and its toolbar -- built as the release
# builds it (iso/sys/ui/build.sh, RUI_WERROR=1) and then used: the window opens, typing counts words and characters,
# Ctrl+B tags what is chosen, Ctrl+F finds, and the toolbar's lists are the page's (mkwordpro.py). It reads
# RUDOLF_WP_STATE, so no screenshot is needed here; the page-by-page comparison against the web WordPro comes with the
# step that makes every one of its views.
#   sh wordpro_t.sh [OUT]
# Needs Xvfb, xdotool, gcc and GTK 3's headers. (display :88, port 8684)
HERE=$(cd "$(dirname "$0")" && pwd); R=$(cd "$HERE/../../.." && pwd); T=$(mktemp -d /tmp/wp_t.XXXXXX)
PORT=${WP_PORT:-8684}; DSP=${WP_DSP:-:88}; OUT=${1:-$T/out}; mkdir -p "$OUT"
command -v Xvfb >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs Xvfb and xdotool"; exit 0; }
BIN=$T/rudolf-wordpro
sh "$R/iso/sys/ui/build.sh" "$T/ui" "$BIN" wordpro 2>"$T/cc" || { cat "$T/cc"; echo "FAIL build"; exit 1; }
[ -s "$T/cc" ] && { echo "FAIL rudolf-wordpro builds with warnings:"; cat "$T/cc"; exit 1; }
echo "PASS 0. rudolf-wordpro builds without a warning ($(stat -c %s "$BIN") bytes)"
export FONTCONFIG_FILE=$HERE/fonts-test.conf
D=$T/d; mkdir -p "$D"
node "$R/iso/qt/host/fixtures/deskls.js" "$D/ls.json" "$D/keys.json" || { echo "FAIL node"; exit 1; }
python3 - "$D" <<'Y'
import json, sys
D = sys.argv[1]; k = json.load(open(D + '/keys.json'))['Ann']
json.dump({'ok': True, 'id': k['id'], 'n': 'Ann', 'r': 'admin', 'pk': k['pk'], 'fk': k['fk']}, open(D + '/session', 'w'))
Y
python3 "$HERE/same_rd.py" "$PORT" "$D" >"$T/rd.log" 2>&1 & RDP=$!
Xvfb "$DSP" -screen 0 1280x900x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
trap 'kill $RDP $XV 2>/dev/null; rm -rf "$T/ui" "$T/cc"' EXIT
i=0; while [ $i -lt 40 ] && ! DISPLAY=$DSP xdotool getmouselocation >/dev/null 2>&1; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
ST=$T/state
RUDOLF_WP_STATE=$ST DISPLAY=$DSP "$BIN" >"$T/wp.log" 2>&1 & WPP=$!
trap 'kill $WPP $RDP $XV 2>/dev/null; rm -rf "$T/ui" "$T/cc"' EXIT
NP=0; NF=0
ok() { if [ "$2" = 1 ]; then NP=$((NP+1)); echo "PASS $1"; else NF=$((NF+1)); echo "FAIL $1 - $3"; fi; }
i=0; while [ $i -lt 60 ] && [ ! -s "$ST" ]; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
WIN=$(DISPLAY=$DSP xdotool search --sync --name '^WordPro$' 2>/dev/null | head -1)
ok "1. the window opens, called WordPro" "$([ -n "$WIN" ] && echo 1)" "no window; $(cat "$T/wp.log")"
grep -q '^styles 9$' "$ST" && grep -q '^fonts 12$' "$ST"
ok "2. the toolbar's lists are the page's: 9 styles, 12 fonts (mkwordpro.py)" "$([ $? = 0 ] && echo 1)" "$(head -20 "$ST")"
# (a press in the page's writing, as a person's first press in it, then the typing)
DISPLAY=$DSP xdotool windowactivate --sync "$WIN" 2>/dev/null; DISPLAY=$DSP xdotool mousemove --window "$WIN" 480 300 click 1; sleep 0.3
DISPLAY=$DSP xdotool type --window "$WIN" --delay 12 'Hello world'
sleep 1
grep -q '^words 2$' "$ST" && grep -q '^chars 10$' "$ST"
ok "3. typing counts the words and the characters" "$([ $? = 0 ] && echo 1)" "$(grep -E '^(words|chars) ' "$ST")"
DISPLAY=$DSP xdotool key --window "$WIN" ctrl+a; DISPLAY=$DSP xdotool key --window "$WIN" ctrl+b; sleep 1
grep -q 'tags b' "$ST"
ok "4. Ctrl+B tags what is chosen" "$([ $? = 0 ] && echo 1)" "$(grep '^tags' "$ST")"
DISPLAY=$DSP xdotool key --window "$WIN" ctrl+f; sleep 0.6
DISPLAY=$DSP xdotool type --window "$WIN" --delay 12 'world'; DISPLAY=$DSP xdotool key --window "$WIN" Return; sleep 0.8
grep -q '^sel 1$' "$ST" && grep -q '^text Hello world$' "$ST"
ok "5. Ctrl+F finds 'world' (it is chosen), and the writing is unchanged" "$([ $? = 0 ] && echo 1)" "$(grep -E '^(sel|text) ' "$ST")"
DISPLAY=$DSP xdotool mousemove --window "$WIN" 480 300 click 1; sleep 0.3
DISPLAY=$DSP xdotool key --window "$WIN" ctrl+a; DISPLAY=$DSP xdotool type --window "$WIN" --delay 12 'one
two'
sleep 1
grep -q '^words 2$' "$ST" && grep -q '^chars 6$' "$ST"
ok "6. a second line is counted as text, its newline not a character" "$([ $? = 0 ] && echo 1)" "$(grep -E '^(words|chars|text) ' "$ST")"
# the build-time reading of the page: a page without the lists stops the build instead of quietly shipping a blank toolbar
mkdir -p "$T/page/iso/sys/apps" "$T/page/iso/sys/ui" "$T/page/desktop"
sed 's/const WP_STYLES=/const WP_STYLES_GONE=/' "$R/desktop/base_v24.html" > "$T/page/desktop/base_v24.html"
cp "$R/iso/sys/apps/mkwordpro.py" "$T/page/iso/sys/apps/mkwordpro.py"
python3 "$T/page/iso/sys/apps/mkwordpro.py" "$T/gone.h" >"$T/mk.log" 2>&1; RC=$?
[ $RC = 1 ] && grep -q 'WP_STYLES' "$T/mk.log"
ok "7. mkwordpro.py refuses a page it cannot read (no quiet default)" "([ $RC = 1 ] && grep -q WP_STYLES "$T/mk.log" && echo 1)" "$RC $(cat "$T/mk.log")"
cp "$ST" "$OUT/state.txt" 2>/dev/null; cp "$T/wp.log" "$OUT/app.log" 2>/dev/null
echo "$NP passed, $NF failed"; [ $NF = 0 ]
