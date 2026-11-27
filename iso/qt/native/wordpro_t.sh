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
# (a backtrace in the log when it stops: the first builds of this step crashed, and the reason is what fixes it)
RUN=""; command -v gdb >/dev/null && RUN="gdb -batch -ex run -ex bt --args"
RUDOLF_WP_STATE=$ST DISPLAY=$DSP sh -c '"$@"; echo "app exit $?" >> "'"$T"'/app.exit"' sh $RUN "$BIN" >"$T/wp.log" 2>&1 & WPP=$!
trap 'kill $WPP $RDP $XV 2>/dev/null; rm -rf "$T/ui" "$T/cc"' EXIT
NP=0; NF=0
ok() { if [ "$2" = 1 ]; then NP=$((NP+1)); echo "PASS $1"; else NF=$((NF+1)); echo "FAIL $1 - $3"; fi; }
i=0; while [ $i -lt 60 ] && [ ! -s "$ST" ]; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
WIN=$(DISPLAY=$DSP xdotool search --sync --name '^WordPro$' 2>/dev/null | head -1)
ok "1. the window opens, called WordPro" "$([ -n "$WIN" ] && echo 1)" "no window; $(cat "$T/wp.log")"
grep -q '^styles 9$' "$ST" && grep -q '^fonts 12$' "$ST"
ok "2. the toolbar's lists are the page's: 9 styles, 12 fonts (mkwordpro.py)" "$([ $? = 0 ] && echo 1)" "$(head -20 "$ST")"
# (the window takes the keys, as a person's first press gives them to it; there is no window manager on this display, so
#  the focus is set with X's own request, waited for, and the keys and the press go in as the X server's own events)
i=0; while [ $i -lt 60 ]; do DISPLAY=$DSP xdotool windowfocus --sync "$WIN" 2>/dev/null
	[ "$(DISPLAY=$DSP xdotool getwindowfocus 2>/dev/null)" = "$WIN" ] && break; timeout 0.2 tail -f /dev/null; i=$((i+1)); done
sleep 2; DISPLAY=$DSP xdotool mousemove --window "$WIN" 480 300 click 1; sleep 1   # (the window opens with its writing focused: the press is the person's, not a requirement)
i=0; while [ $i -lt 3 ]; do   # (a first press into a window that has just appeared can be lost: tried again, as a person would)
	DISPLAY=$DSP xdotool type --delay 12 'Hello world'; sleep 1
	grep -q '^words 2$' "$ST" && break
	DISPLAY=$DSP xdotool key ctrl+a; DISPLAY=$DSP xdotool key BackSpace; sleep 0.4; i=$((i+1)); done
grep -q '^words 2$' "$ST" && grep -q '^chars 11$' "$ST"
ok "3. typing counts the words and the characters" "$([ $? = 0 ] && echo 1)" "$(grep -E '^(words|chars) ' "$ST")"
DISPLAY=$DSP xdotool key ctrl+a; DISPLAY=$DSP xdotool key ctrl+b; sleep 1
grep -q 'tags b' "$ST"
ok "4. Ctrl+B tags what is chosen" "$([ $? = 0 ] && echo 1)" "$(grep '^tags' "$ST")"
# (the cursor is put inside the bold text: a choice's end is not in the choice, as the page's sel() has it too)
DISPLAY=$DSP xdotool key Home; sleep 0.7
[ "$(sed -n 's/^on //p' "$ST")" = b ]
ok "5. the toolbar says so: bold is on where the writing is" "$([ $? = 0 ] && echo 1)" "$(grep '^on' "$ST")"
DISPLAY=$DSP xdotool key ctrl+a; DISPLAY=$DSP xdotool key ctrl+b; sleep 0.8
DISPLAY=$DSP xdotool key Home; sleep 0.7
[ -z "$(sed -n 's/^on //p' "$ST")" ] && ! grep -q 'tags b' "$ST"
ok "6. ...and a second Ctrl+B takes it off, as the page's does" "$([ $? = 0 ] && echo 1)" "$(grep -E '^(on|tags)' "$ST")"
DISPLAY=$DSP xdotool key ctrl+f; sleep 0.6
DISPLAY=$DSP xdotool type --delay 12 'world'; DISPLAY=$DSP xdotool key Return; sleep 0.8
grep -q '^sel 1$' "$ST" && grep -q '^text Hello world$' "$ST"
ok "7. Ctrl+F finds 'world' (it is chosen), and the writing is unchanged" "$([ $? = 0 ] && echo 1)" "$(grep -E '^(sel|text) ' "$ST")"
DISPLAY=$DSP xdotool mousemove --window "$WIN" 480 300 click 1; sleep 0.5
DISPLAY=$DSP xdotool key ctrl+a; DISPLAY=$DSP xdotool type --delay 12 'one'; DISPLAY=$DSP xdotool key Return; DISPLAY=$DSP xdotool type --delay 12 'two'
sleep 1
grep -q '^words 2$' "$ST" && grep -q '^chars 6$' "$ST"
kill -0 $WPP 2>/dev/null || echo "NOTE the app had exited: $(cat "$T/app.exit" 2>/dev/null)"
ok "8. a second line is counted as text, its newline not a character" "$([ $? = 0 ] && echo 1)" "$(grep -E '^(words|chars|text) ' "$ST")"
grep -q '^menus 8$' "$ST"
ok "9. the menu bar is the page's eight menus, File to Help" "$([ $? = 0 ] && echo 1)" "$(grep '^menus' "$ST")"
# (a second copy of the app opens one menu as a press on the bar opens it, so the items that appear are the page's own)
ST2=$T/state2.txt; rm -f "$ST2"
RUDOLF_WP_STATE=$ST2 RUDOLF_WP_MENU=Format DISPLAY=$DSP "$BIN" >"$T/wp2.log" 2>&1 & WPP2=$!
i=0; while [ $i -lt 40 ] && ! grep -q '^menu ' "$ST2" 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
grep -q '^menu Format 24$' "$ST2"; RC=$?
kill $WPP2 2>/dev/null
ok "10. ...and Format opens with the page's items: six toggles, the four cases, the lists, 24 rows with their separators" "$([ $RC = 0 ] && echo 1)" "$(grep '^menu' "$ST2" 2>/dev/null); $(tail -3 "$T/wp2.log" 2>/dev/null)"
# (New is a step of the history, as the page's own is: undo brings the page back, redo empties it again)
DISPLAY=$DSP xdotool key ctrl+n; sleep 0.8
DISPLAY=$DSP xdotool key space; sleep 0.9
A=$(grep -c '^words 0$' "$ST")
DISPLAY=$DSP xdotool key ctrl+z; sleep 0.9
B=$(grep -c '^words 2$' "$ST")
DISPLAY=$DSP xdotool key ctrl+y; sleep 0.9
C=$(grep -c '^words 0$' "$ST")
[ "$A$B$C" = 111 ]
ok "11. New is a step of the history: undo brings the page back, redo empties it again" "$([ $? = 0 ] && echo 1)" "after New $A, after undo $B, after redo $C (words: $(grep '^words' "$ST"))"
kill -0 $WPP 2>/dev/null || echo "NOTE the app had exited: $(cat "$T/app.exit" 2>/dev/null)"
# the build-time reading of the page: a page without the lists stops the build instead of quietly shipping a blank toolbar
mkdir -p "$T/page/iso/sys/apps" "$T/page/iso/sys/ui" "$T/page/desktop"
sed 's/const WP_STYLES=/const WP_STYLES_GONE=/' "$R/desktop/base_v24.html" > "$T/page/desktop/base_v24.html"
cp "$R/iso/sys/apps/mkwordpro.py" "$T/page/iso/sys/apps/mkwordpro.py"
python3 "$T/page/iso/sys/apps/mkwordpro.py" "$T/gone.h" >"$T/mk.log" 2>&1; RC=$?
if [ $RC = 1 ] && grep -q 'WP_STYLES' "$T/mk.log"; then
	ok "12. mkwordpro.py refuses a page it cannot read (no quiet default)" 1
else
	ok "12. mkwordpro.py refuses a page it cannot read (no quiet default)" 0 "rc=$RC $(cat "$T/mk.log")"
fi
kill -0 $WPP 2>/dev/null || echo "NOTE the app had exited: $(cat "$T/app.exit" 2>/dev/null)"
# (the shortcuts the bar advertises: Ctrl+H is the Find bar's replace box, as the page's)
DISPLAY=$DSP xdotool key ctrl+h; sleep 0.8
DISPLAY=$DSP xdotool type --delay 12 'TWO'; sleep 0.8
grep -q '^findbar 1$' "$ST" && grep -q '^repl TWO$' "$ST" && grep -q '^find world$' "$ST"
ok "13. Ctrl+H opens the Find bar on its replace box, not its find box" "$([ $? = 0 ] && echo 1)" "$(grep -E '^(findbar|find|repl) ' "$ST")"
# (a picture and a table are places in the writing, with their own data, not characters or words of it)
python3 - "$T/pic.png" <<'PY'
import sys, zlib, struct
w, h = 40, 30
raw = b''.join(b'\x00' + b'\xc0\x00\x00' * w for _ in range(h))
def chunk(t, d):
    return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
png = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
       + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))
open(sys.argv[1], 'wb').write(png)
PY
ST3=$T/state3.txt; rm -f "$ST3"
RUDOLF_WP_STATE=$ST3 RUDOLF_WP_INSERT="picture:$T/pic.png" DISPLAY=$DSP "$BIN" >"$T/wp3.log" 2>&1 & WPP3=$!
i=0; while [ $i -lt 40 ] && ! grep -q '^objs 1$' "$ST3" 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
grep -q '^obj picture 40x30' "$ST3" && grep -q '^words 0$' "$ST3" && grep -q '^chars 0$' "$ST3"; RC=$?
kill $WPP3 2>/dev/null
ok "14. a picture keeps its size, and is neither a word nor a character of the writing" "$([ $RC = 0 ] && echo 1)" "$(grep -E '^(objs|obj|words|chars) ' "$ST3" 2>/dev/null); $(tail -2 "$T/wp3.log" 2>/dev/null)"
ST4=$T/state4.txt; rm -f "$ST4"
RUDOLF_WP_STATE=$ST4 RUDOLF_WP_INSERT="table" DISPLAY=$DSP "$BIN" >"$T/wp4.log" 2>&1 & WPP4=$!
i=0; while [ $i -lt 40 ] && ! grep -q '^objs 1$' "$ST4" 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
grep -q '^obj table 3x3$' "$ST4"; RC=$?
kill $WPP4 2>/dev/null
ok "15. a table is its own place too: the page's default 3 x 3" "$([ $RC = 0 ] && echo 1)" "$(grep -E '^(objs|obj) ' "$ST4" 2>/dev/null)"
# (the page's .DOC: what the web WordPro writes opens here, and what is saved here opens again as it was)
python3 - "$T/pic.png" "$T/in.DOC" <<'PY'
import sys, base64
pic = base64.b64encode(open(sys.argv[1], 'rb').read()).decode()
doc = ('<!--wp:%7B%7D--><style data-wp>.wp-t{border-collapse:collapse}</style>'
       '<p>Hello <b>world</b></p>'
       '<img src="data:image/png;base64,' + pic + '" width="40" height="30">'
       '<table class="wp-t"><tr><th>a</th><th>b</th></tr><tr><td>c</td><td>d</td></tr></table>')
open(sys.argv[2], 'w').write(doc)
PY
ST5=$T/state5.txt; rm -f "$ST5" "$T/out.DOC"
RUDOLF_WP_STATE=$ST5 RUDOLF_WP_OPEN="$T/in.DOC" RUDOLF_WP_SAVE="$T/out.DOC" DISPLAY=$DSP "$BIN" >"$T/wp5.log" 2>&1 & WPP5=$!
i=0; while [ $i -lt 40 ] && ! grep -q '^objs 2$' "$ST5" 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
i=0; while [ $i -lt 60 ] && [ ! -s "$T/out.DOC" ]; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
grep -q '^words 2$' "$ST5" && grep -q '^chars 11$' "$ST5" && grep -q '^tags b' "$ST5" &&
	grep -q '^obj picture 40x30' "$ST5" && grep -q '^obj table 2x2$' "$ST5" &&
	grep -q '<th>a</th><th>b</th>' "$T/out.DOC" && grep -q '<td>c</td><td>d</td>' "$T/out.DOC" &&
	grep -q 'data:image/png;base64,' "$T/out.DOC" && grep -q '<p>Hello <b>world</b>' "$T/out.DOC"; RC=$?
kill $WPP5 2>/dev/null
ok "16. the page's .DOC opens here (its paragraph, its bold, its picture, its table) and is written back" "$([ $RC = 0 ] && echo 1)" "$(grep -E '^(words|chars|tags|objs|obj) ' "$ST5" 2>/dev/null); $(sed 's/.*<\/style>//' "$T/out.DOC" 2>/dev/null | cut -c1-100)"
ST6=$T/state6.txt; rm -f "$ST6" "$T/out2.DOC"
RUDOLF_WP_STATE=$ST6 RUDOLF_WP_OPEN="$T/out.DOC" RUDOLF_WP_SAVE="$T/out2.DOC" DISPLAY=$DSP "$BIN" >"$T/wp6.log" 2>&1 & WPP6=$!
i=0; while [ $i -lt 40 ] && ! grep -q '^objs 2$' "$ST6" 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
i=0; while [ $i -lt 60 ] && [ ! -s "$T/out2.DOC" ]; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
cmp -s "$T/out.DOC" "$T/out2.DOC"; SAME=$?
grep -q '^words 2$' "$ST6" && grep -q '^chars 11$' "$ST6" && grep -q '^tags b' "$ST6" &&
	grep -q '^obj picture 40x30' "$ST6" && grep -q '^obj table 2x2$' "$ST6" && [ $SAME = 0 ]; RC=$?
kill $WPP6 2>/dev/null
ok "17. ...and what was saved here opens and saves again as the very same document (the round trip)" "$([ $RC = 0 ] && echo 1)" "$(grep -E '^(words|chars|tags|objs|obj) ' "$ST6" 2>/dev/null); the second save is $( [ $SAME = 0 ] && echo 'the same bytes' || echo 'different' ) ($(stat -c %s "$T/out.DOC" 2>/dev/null) and $(stat -c %s "$T/out2.DOC" 2>/dev/null) bytes)"
# (the picture is a step of the history: undo takes it away, redo brings the picture itself back -- not a place with
#  nothing behind it. A copy of the app takes the keys: the same window, focused on this display as the first one was)
# (the first window is taken down first: this copy has to be the only one on the display for its keys to be its own)
kill $WPP 2>/dev/null; sleep 0.5
ST7=$T/state7.txt; rm -f "$ST7"
rm -f "$T/wp7.exit"
RUDOLF_WP_STATE=$ST7 RUDOLF_WP_INSERT="picture:$T/pic.png" DISPLAY=$DSP sh -c '"$@"; echo "app exit $?" >> "'"$T"'/wp7.exit"' sh $RUN "$BIN" >"$T/wp7.log" 2>&1 & WPP7=$!
i=0; while [ $i -lt 40 ] && ! grep -q '^objs 1$' "$ST7" 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
W7=$(DISPLAY=$DSP xdotool search --sync --onlyvisible --pid "$WPP7" 2>/dev/null | tail -1)
[ -n "$W7" ] || W7=$(DISPLAY=$DSP xdotool search --onlyvisible --name '^WordPro$' 2>/dev/null | tail -1)
FOCUS=""
[ -n "$W7" ] && DISPLAY=$DSP xdotool windowraise "$W7" 2>/dev/null
i=0; while [ $i -lt 60 ] && [ -n "$W7" ]; do DISPLAY=$DSP xdotool windowfocus --sync "$W7" 2>/dev/null
	FOCUS=$(DISPLAY=$DSP xdotool getwindowfocus 2>/dev/null)
	[ "$FOCUS" = "$W7" ] && break; timeout 0.2 tail -f /dev/null; i=$((i+1)); done
[ -n "$W7" ] && { sleep 0.6; DISPLAY=$DSP xdotool mousemove --window "$W7" 480 300 click 1; sleep 0.8; }
# (the state file is written again whenever the page changes: the undo is read here, before the redo takes its place)
A=0
i=0; while [ $i -lt 4 ]; do   # (a first press into a window that has just appeared can be lost: tried again, as a person would)
	DISPLAY=$DSP xdotool key ctrl+z; sleep 0.9
	grep -q '^objs 0$' "$ST7" && { A=1; break; }; i=$((i+1)); done
i=0; while [ $i -lt 4 ]; do
	DISPLAY=$DSP xdotool key ctrl+y; sleep 0.9
	grep -q '^objs 1$' "$ST7" && break; i=$((i+1)); done
grep -q '^objs 1$' "$ST7" && grep -q '^obj picture 40x30 [0-9][0-9]*$' "$ST7" && grep -q '^words 0$' "$ST7" && grep -q '^chars 0$' "$ST7"; RC=$?
kill $WPP7 2>/dev/null
ok "18. undo takes the picture away, and redo brings the picture itself back" "$([ $RC = 0 ] && [ "$A" = 1 ] && echo 1)" "after undo $A (objs 0), after redo $(grep -E '^(objs|obj|words|chars) ' "$ST7" 2>/dev/null | tr '\n' ' ')(window $W7, focus $FOCUS; $(cat "$T/wp7.exit" 2>/dev/null) $(grep -E 'SIG|signal|abort|#[0-9]+ ' "$T/wp7.log" 2>/dev/null | tail -3 | tr '\n' ' '))"
cp "$ST" "$OUT/state.txt" 2>/dev/null; cp "$T/wp.log" "$OUT/app.log" 2>/dev/null
for f in "$T/state5.txt" "$T/state6.txt" "$T/state7.txt"; do [ -f "$f" ] && cp "$f" "$OUT/$(basename "$f")"; done
[ -f "$T/out.DOC" ] && head -c 4000 "$T/out.DOC" > "$OUT/out.txt" 2>/dev/null; [ -f "$T/out.DOC" ] && wc -c < "$T/out.DOC" > "$OUT/out-size.txt"
echo "$NP passed, $NF failed"; [ $NF = 0 ]
