#!/bin/sh
# 1.6.3 (docs/plan-1.6.3.md Part 1): rudolf-wordpro's first step -- the editor and its toolbar -- built as the release
# builds it (iso/sys/ui/build.sh, RUI_WERROR=1) and then used: the window opens, typing counts words and characters,
# Ctrl+B tags what is chosen, Ctrl+F finds, and the toolbar's lists are the page's (mkwordpro.py). The last cases are the
# page's unsaved changes -- the file's name and a * in the title, and its "Save changes to NAME?" before New, Open and
# Close -- and the format changes that are steps of the history as typed text is. It reads
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
ANS=$T/ans; : > "$ANS"   # (the answer the test would press when the page asks: written when it would press it)
RUDOLF_WP_STATE=$ST RUDOLF_WP_ANSWER=$ANS DISPLAY=$DSP sh -c '"$@"; echo "app exit $?" >> "'"$T"'/app.exit"' sh $RUN "$BIN" >"$T/wp.log" 2>&1 & WPP=$!
trap 'kill $WPP $RDP $XV 2>/dev/null; rm -rf "$T/ui" "$T/cc"' EXIT
NP=0; NF=0
ok() { if [ "$2" = 1 ]; then NP=$((NP+1)); echo "PASS $1"; else NF=$((NF+1)); echo "FAIL $1 - $3"; fi; }
i=0; while [ $i -lt 60 ] && [ ! -s "$ST" ]; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
# (the title is the page's own: the file's name, or Untitled, and * while the writing is not in its file -- so the
#  window is looked for by the end of its title, not by the whole of it)
WIN=$(DISPLAY=$DSP xdotool search --sync --name 'WordPro$' 2>/dev/null | head -1)
ok "1. the window opens, called 'Untitled - WordPro'" "$([ -n "$WIN" ] && echo 1)" "no window; $(cat "$T/wp.log")"
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
# (New is a step of the history, as the page's own is: undo brings the page back, redo empties it again. There are
#  unsaved changes on the page, so New asks first, and Don't save -- the answer the test would press -- throws it away)
echo "Don't save" > "$ANS"
DISPLAY=$DSP xdotool key ctrl+n; sleep 1.2
: > "$ANS"
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
# (a table is its own place too, and the page makes one through its own dialog (base_v24.html's tableDlg): Rows,
#  Columns, Header row and Borders, Cancel and Insert. The dialog is opened as its menu opens it, and Insert -- the
#  answer the test would press, in the file the app reads -- takes its defaults: 3 rows, 3 columns, a heading row and
#  borders, which are the page's own 3 x 3 with <th> in its first row and the .wp-t class)
ST4=$T/state4.txt; A4=$T/ans4; rm -f "$ST4"; : > "$A4"
RUDOLF_WP_STATE=$ST4 RUDOLF_WP_ANSWER=$A4 RUDOLF_WP_DIALOG=table DISPLAY=$DSP $RUN "$BIN" >"$T/wp4.log" 2>&1 & WPP4=$!
i=0; while [ $i -lt 40 ] && ! grep -q '^dialog 1$' "$ST4" 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
D4=$(grep -c '^dialog 1$' "$ST4" 2>/dev/null)
echo "Insert" > "$A4"   # (the answer: the dialog's own Insert)
i=0; while [ $i -lt 40 ] && ! grep -q '^objs 1$' "$ST4" 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
grep -q '^obj table 3x3$' "$ST4" && grep -q '^tbl hdr 1 nb 0$' "$ST4" && grep -q '^cells|||' "$ST4" && [ "$D4" = 1 ]; RC=$?
kill $WPP4 2>/dev/null
ok "15. the page's Insert table dialog opens on its defaults, and its Insert makes the page's 3 x 3 with a heading row and borders" "$([ $RC = 0 ] && echo 1)" "dialog up $D4; $(grep -E '^(objs|obj|tbl|dialog) ' "$ST4" 2>/dev/null | tr '\n' ' '); $(tail -20 "$T/wp4.log" 2>/dev/null | tr '\n' ' ')"
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
grep -q '^words 6$' "$ST5" && grep -q '^chars 15$' "$ST5" && grep -q '^tags b' "$ST5" &&
	grep -q '^obj picture 40x30' "$ST5" && grep -q '^obj table 2x2$' "$ST5" && grep -q '^tbl hdr 1 nb 0$' "$ST5" &&
	grep -q '<th>a</th><th>b</th>' "$T/out.DOC" && grep -q '<td>c</td><td>d</td>' "$T/out.DOC" &&
	grep -q 'data:image/png;base64,' "$T/out.DOC" && grep -q '<p>Hello <b>world</b>' "$T/out.DOC"; RC=$?
kill $WPP5 2>/dev/null
ok "16. the page's .DOC opens here (its paragraph, its bold, its picture, its table) and is written back" "$([ $RC = 0 ] && echo 1)" "$(grep -E '^(words|chars|tags|objs|obj) ' "$ST5" 2>/dev/null); $(sed 's/.*<\/style>//' "$T/out.DOC" 2>/dev/null | cut -c1-100)"
ST6=$T/state6.txt; rm -f "$ST6" "$T/out2.DOC"
RUDOLF_WP_STATE=$ST6 RUDOLF_WP_OPEN="$T/out.DOC" RUDOLF_WP_SAVE="$T/out2.DOC" DISPLAY=$DSP "$BIN" >"$T/wp6.log" 2>&1 & WPP6=$!
i=0; while [ $i -lt 40 ] && ! grep -q '^objs 2$' "$ST6" 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
i=0; while [ $i -lt 60 ] && [ ! -s "$T/out2.DOC" ]; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
cmp -s "$T/out.DOC" "$T/out2.DOC"; SAME=$?
grep -q '^words 6$' "$ST6" && grep -q '^chars 15$' "$ST6" && grep -q '^tags b' "$ST6" &&
	grep -q '^obj picture 40x30' "$ST6" && grep -q '^obj table 2x2$' "$ST6" && [ $SAME = 0 ]; RC=$?
kill $WPP6 2>/dev/null
ok "17. ...and what was saved here opens and saves again as the very same document (the round trip)" "$([ $RC = 0 ] && echo 1)" "$(grep -E '^(words|chars|tags|objs|obj) ' "$ST6" 2>/dev/null); the second save is $( [ $SAME = 0 ] && echo 'the same bytes' || echo 'different' ) ($(stat -c %s "$T/out.DOC" 2>/dev/null) and $(stat -c %s "$T/out2.DOC" 2>/dev/null) bytes)"
# (the picture is a step of the history: undo takes it away, redo brings the picture itself back -- not a place with
#  nothing behind it. The first window is taken down first, by the pid its window gives (the shell around gdb is not the
#  app, and the app is what holds the window), and the display is waited on until no WordPro is left: this copy has to be
#  the only one on the display for its keys to be its own. It runs as itself, so its pid is its window's.)
kill $WPP 2>/dev/null
P1=$(DISPLAY=$DSP xdotool getwindowpid "$WIN" 2>/dev/null); [ -n "$P1" ] && kill $P1 2>/dev/null
i=0; while [ $i -lt 40 ] && [ -n "$(DISPLAY=$DSP xdotool search --onlyvisible --name 'WordPro$' 2>/dev/null)" ]; do timeout 0.2 tail -f /dev/null; i=$((i+1)); done
ST7=$T/state7.txt; rm -f "$ST7"
RUDOLF_WP_STATE=$ST7 RUDOLF_WP_INSERT="picture:$T/pic.png" DISPLAY=$DSP "$BIN" >"$T/wp7.log" 2>&1 & WPP7=$!
i=0; while [ $i -lt 40 ] && ! grep -q '^objs 1$' "$ST7" 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
# (its window is looked for as it appears, a bounded number of tries: a search that never matches would hang the check)
W7=""; i=0; while [ $i -lt 40 ] && [ -z "$W7" ]; do
	W7=$(DISPLAY=$DSP xdotool search --onlyvisible --pid "$WPP7" 2>/dev/null | tail -1)
	[ -n "$W7" ] || timeout 0.25 tail -f /dev/null; i=$((i+1)); done
FOCUS=""
[ -n "$W7" ] && DISPLAY=$DSP xdotool windowraise "$W7" 2>/dev/null
i=0; while [ $i -lt 60 ] && [ -n "$W7" ]; do DISPLAY=$DSP xdotool windowfocus --sync "$W7" 2>/dev/null
	FOCUS=$(DISPLAY=$DSP xdotool getwindowfocus 2>/dev/null)
	[ "$FOCUS" = "$W7" ] && break; timeout 0.2 tail -f /dev/null; i=$((i+1)); done
[ -n "$W7" ] && { sleep 0.6; DISPLAY=$DSP xdotool mousemove --window "$W7" 480 300 click 1; sleep 0.8; }
# (the state file is written again whenever the page changes, a moment after it: the undo is read here, before the redo
#  takes its place, and a key is pressed again while the state has not caught up -- at the page's own two ends, pressing
#  undo again or redo again is no step, so asking more than once cannot take more than the one change the case is about)
A=0
i=0; while [ $i -lt 12 ]; do
	DISPLAY=$DSP xdotool key ctrl+z; sleep 0.5
	grep -q '^objs 0$' "$ST7" && { A=1; break; }; i=$((i+1)); done
i=0; while [ $i -lt 12 ]; do
	DISPLAY=$DSP xdotool key ctrl+y; sleep 0.5
	grep -q '^objs 1$' "$ST7" && break; i=$((i+1)); done
ALIVE=$(kill -0 $WPP7 2>/dev/null && echo alive || echo gone)
grep -q '^objs 1$' "$ST7" && grep -q '^obj picture 40x30 [0-9][0-9]*$' "$ST7" && grep -q '^words 0$' "$ST7" && grep -q '^chars 0$' "$ST7"; RC=$?
kill $WPP7 2>/dev/null
ok "18. undo takes the picture away, and redo brings the picture itself back" "$([ $RC = 0 ] && [ "$A" = 1 ] && echo 1)" "after undo $A (objs 0), after redo $(grep -E '^(objs|obj|words|chars|hist) ' "$ST7" 2>/dev/null | tr '\n' ' ')(window $W7 of $WPP7, focus $FOCUS, the app $ALIVE; $(tail -3 "$T/wp7.log" 2>/dev/null | tr '\n' ' '))"
# (a table is a place too, and its entry keeps the cells: a redo that rebuilt an empty grid would still say "obj table" and
#  count no words, so the cells are the thing to look at. A page's .DOC holding only a 2 x 2 table opens, the table is
#  undone away, and a redo brings the table back -- with a, b, c and d standing in its cells again)
python3 - "$T/tab.DOC" <<'PY'
import sys
doc = ('<!--wp:%7B%7D--><style data-wp>.wp-t{border-collapse:collapse}</style>'
       '<table class="wp-t"><tr><th>a</th><th>b</th></tr><tr><td>c</td><td>d</td></tr></table>')
open(sys.argv[1], 'w').write(doc)
PY
ST8=$T/state8.txt; rm -f "$ST8"
RUDOLF_WP_STATE=$ST8 RUDOLF_WP_OPEN="$T/tab.DOC" DISPLAY=$DSP "$BIN" >"$T/wp8.log" 2>&1 & WPP8=$!
i=0; while [ $i -lt 40 ] && ! grep -q '^obj table 2x2$' "$ST8" 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
W8=""; i=0; while [ $i -lt 40 ] && [ -z "$W8" ]; do
	W8=$(DISPLAY=$DSP xdotool search --onlyvisible --pid "$WPP8" 2>/dev/null | tail -1)
	[ -n "$W8" ] || timeout 0.25 tail -f /dev/null; i=$((i+1)); done
F8=""
[ -n "$W8" ] && DISPLAY=$DSP xdotool windowraise "$W8" 2>/dev/null
i=0; while [ $i -lt 60 ] && [ -n "$W8" ]; do DISPLAY=$DSP xdotool windowfocus --sync "$W8" 2>/dev/null
	F8=$(DISPLAY=$DSP xdotool getwindowfocus 2>/dev/null)
	[ "$F8" = "$W8" ] && break; timeout 0.2 tail -f /dev/null; i=$((i+1)); done
[ -n "$W8" ] && { sleep 0.6; DISPLAY=$DSP xdotool mousemove --window "$W8" 480 300 click 1; sleep 0.8; }
B=0
i=0; while [ $i -lt 12 ]; do
	DISPLAY=$DSP xdotool key ctrl+z; sleep 0.5
	grep -q '^objs 0$' "$ST8" && { B=1; break; }; i=$((i+1)); done
i=0; while [ $i -lt 12 ]; do
	DISPLAY=$DSP xdotool key ctrl+y; sleep 0.5
	grep -q '^obj table 2x2$' "$ST8" && break; i=$((i+1)); done
grep -q '^obj table 2x2$' "$ST8" && grep -q '^words 4$' "$ST8" && grep -q '^chars 4$' "$ST8"; RC=$?
kill $WPP8 2>/dev/null
ok "19. undo takes the table away, and redo brings the table back with its cells" "$([ $RC = 0 ] && [ "$B" = 1 ] && echo 1)" "the undo took it away: $B, after redo $(grep -E '^(objs|obj|words|chars|hist) ' "$ST8" 2>/dev/null | tr '\n' ' ')(window $W8 of $WPP8, focus $F8)"
# (a cell's writing is part of the page, so a change in it is a step of the history as typed text is: Ctrl+Z in the cell
#  takes the letters back and Ctrl+Y puts them in again, the cursor still standing in the cell as a person's would. The
#  page opens with the cursor in the table's first cell (RUDOLF_WP_FOCUS, the tests' own way in: this display has no window
#  manager, so a click cannot be told where the page's first cell is). A page holding only the table leaves the writing
#  empty, which is how the letters are told from the writing's -- a cell's words are the page's, never its text)
ST9=$T/state9.txt; rm -f "$ST9"
RUDOLF_WP_STATE=$ST9 RUDOLF_WP_OPEN="$T/tab.DOC" RUDOLF_WP_FOCUS=cell DISPLAY=$DSP "$BIN" >"$T/wp9.log" 2>&1 & WPP9=$!
i=0; while [ $i -lt 40 ] && ! grep -q '^obj table 2x2$' "$ST9" 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
W9=""; i=0; while [ $i -lt 40 ] && [ -z "$W9" ]; do
	W9=$(DISPLAY=$DSP xdotool search --onlyvisible --pid "$WPP9" 2>/dev/null | tail -1)
	[ -n "$W9" ] || timeout 0.25 tail -f /dev/null; i=$((i+1)); done
F9=""
[ -n "$W9" ] && DISPLAY=$DSP xdotool windowraise "$W9" 2>/dev/null
i=0; while [ $i -lt 60 ] && [ -n "$W9" ]; do DISPLAY=$DSP xdotool windowfocus --sync "$W9" 2>/dev/null
	F9=$(DISPLAY=$DSP xdotool getwindowfocus 2>/dev/null)
	[ "$F9" = "$W9" ] && break; timeout 0.2 tail -f /dev/null; i=$((i+1)); done
sleep 3   # (the cursor is put in the cell a moment after the page opens, once the window has the display's focus)
DISPLAY=$DSP xdotool type --delay 60 'x'; sleep 1.2
cp "$ST9" "$T/st9.typed" 2>/dev/null
C=0; grep -q '^cells|ax|b|c|d$' "$ST9" && grep -q '^text $' "$ST9" && grep -q '^hist 3 3$' "$ST9" && C=1
U=0
i=0; while [ $i -lt 12 ]; do
	DISPLAY=$DSP xdotool key ctrl+z; sleep 0.5
	grep -q '^cells|a|b|c|d$' "$ST9" && grep -q '^hist 2 3$' "$ST9" && { U=1; break; }; i=$((i+1)); done
cp "$ST9" "$T/st9.undo" 2>/dev/null
i=0; while [ $i -lt 12 ]; do
	DISPLAY=$DSP xdotool key ctrl+y; sleep 0.5
	grep -q '^cells|ax|b|c|d$' "$ST9" && break; i=$((i+1)); done
ALIVE9=$(kill -0 $WPP9 2>/dev/null && echo alive || echo gone)
grep -q '^obj table 2x2$' "$ST9" && grep -q '^cells|ax|b|c|d$' "$ST9" && grep -q '^text $' "$ST9" &&
	grep -q '^hist 3 3$' "$ST9"; RC=$?
kill $WPP9 2>/dev/null
ok "20. a change in a table's cell is a step of the history: Ctrl+Z takes it back, Ctrl+Y puts it in again" "$([ $RC = 0 ] && [ "$C" = 1 ] && [ "$U" = 1 ] && echo 1)" "the letters in the cell: $C, the undo took them back: $U; typed $(grep -E '^(words|hist|cells|text|step) ' "$T/st9.typed" 2>/dev/null | tr '\n' ' '), undone $(grep -E '^(words|hist|cells|text|step) ' "$T/st9.undo" 2>/dev/null | tr '\n' ' '), redone $(grep -E '^(words|hist|cells|text|step) ' "$ST9" 2>/dev/null | tr '\n' ' ')(window $W9, focus $F9, the app $ALIVE9; $(tail -2 "$T/wp9.log" 2>/dev/null | tr '\n' ' '))"
# (the page's unsaved changes (base_v24.html's confirmDiscard): the file's name and a * in the title while the writing is
#  not in its file, and the page's own question -- Save changes to NAME?, with Cancel, Don't save and Save -- before New,
#  Open and Close throw the writing away. Ctrl+N is New here and RUDOLF_WP_CLOSE is the window's close: this display has
#  no window manager to press a close button on. Escape is Cancel, and RUDOLF_WP_ANSWER names the button the test would
#  press, in a file the app reads while the question is up)
ST10=$T/state10.txt; rm -f "$ST10"; A10=$T/ans10; : > "$A10"
RUDOLF_WP_STATE=$ST10 RUDOLF_WP_ANSWER=$A10 DISPLAY=$DSP "$BIN" >"$T/wp10.log" 2>&1 & WPP10=$!
i=0; while [ $i -lt 40 ] && [ ! -s "$ST10" ]; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
W10=$(DISPLAY=$DSP xdotool search --sync --onlyvisible --pid "$WPP10" 2>/dev/null | tail -1)
i=0; while [ $i -lt 60 ] && [ -n "$W10" ]; do DISPLAY=$DSP xdotool windowfocus --sync "$W10" 2>/dev/null
	[ "$(DISPLAY=$DSP xdotool getwindowfocus 2>/dev/null)" = "$W10" ] && break; timeout 0.2 tail -f /dev/null; i=$((i+1)); done
[ -n "$W10" ] && { sleep 0.6; DISPLAY=$DSP xdotool mousemove --window "$W10" 480 300 click 1; sleep 0.8; }
T0=$(DISPLAY=$DSP xdotool getwindowname "$W10" 2>/dev/null)
DISPLAY=$DSP xdotool type --delay 12 'Hello'; sleep 1.2
T1=$(DISPLAY=$DSP xdotool getwindowname "$W10" 2>/dev/null)
D1=$(grep -c '^dirty 1$' "$ST10")
DISPLAY=$DSP xdotool key ctrl+n; sleep 1
i=0; while [ $i -lt 20 ] && ! grep -q '^ask 1$' "$ST10" 2>/dev/null; do timeout 0.2 tail -f /dev/null; i=$((i+1)); done
Q1=$(grep -c '^ask 1$' "$ST10"); K1=$(grep -c '^text Hello$' "$ST10")   # (the question is up, and nothing has gone yet)
DISPLAY=$DSP xdotool key Escape; sleep 1
Q0=$(grep -c '^ask 0$' "$ST10"); K2=$(grep -c '^text Hello$' "$ST10")   # (Cancel: the page stays as it was)
DISPLAY=$DSP xdotool key ctrl+n; sleep 1
i=0; while [ $i -lt 20 ] && ! grep -q '^ask 1$' "$ST10" 2>/dev/null; do timeout 0.2 tail -f /dev/null; i=$((i+1)); done
echo "Don't save" > "$A10"; sleep 1.2   # (the answer the test would press: the question's Don't save)
N1=$(grep -c '^dirty 0$' "$ST10"); N2=$(grep -c '^text $' "$ST10")   # (a new page, with nothing unsaved)
T2=$(DISPLAY=$DSP xdotool getwindowname "$W10" 2>/dev/null)
kill $WPP10 2>/dev/null
ok "21. the unsaved changes: the name and * in the title, and the page's question before New throws the writing away" \
	"$([ "$T0" = 'Untitled - WordPro' ] && [ "$T1" = 'Untitled * - WordPro' ] && [ "$T2" = 'Untitled - WordPro' ] && [ "$D1$Q1$K1$Q0$K2$N1$N2" = 1111111 ] && echo 1)" \
	"titles '$T0', '$T1', '$T2'; dirty $D1; asked $Q1 with the writing kept $K1; Cancel $Q0 kept it $K2; after Don't save dirty $N1 and empty $N2 ($(grep -E '^(dirty|ask|answer|text|words|hist) ' "$ST10" 2>/dev/null | tr '\n' ' '); $(tail -2 "$T/wp10.log" 2>/dev/null | tr '\n' ' '))"
# (Close asks it too, and the window is not taken down until the writing is saved or let go: the close is asked again
#  while anything is unsaved, so a Cancel is followed by another question, and a Don't save is the one that lets it go)
ST11=$T/state11.txt; rm -f "$ST11"; A11=$T/ans11; : > "$A11"
RUDOLF_WP_STATE=$ST11 RUDOLF_WP_ANSWER=$A11 RUDOLF_WP_CLOSE=1 DISPLAY=$DSP "$BIN" >"$T/wp11.log" 2>&1 & WPP11=$!
i=0; while [ $i -lt 40 ] && [ ! -s "$ST11" ]; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
W11=$(DISPLAY=$DSP xdotool search --sync --onlyvisible --pid "$WPP11" 2>/dev/null | tail -1)
i=0; while [ $i -lt 60 ] && [ -n "$W11" ]; do DISPLAY=$DSP xdotool windowfocus --sync "$W11" 2>/dev/null
	[ "$(DISPLAY=$DSP xdotool getwindowfocus 2>/dev/null)" = "$W11" ] && break; timeout 0.2 tail -f /dev/null; i=$((i+1)); done
[ -n "$W11" ] && { sleep 0.6; DISPLAY=$DSP xdotool mousemove --window "$W11" 480 300 click 1; sleep 0.8; }
DISPLAY=$DSP xdotool type --delay 12 'Bye'; sleep 1
i=0; while [ $i -lt 24 ] && ! grep -q '^ask 1$' "$ST11" 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
C1=$(grep -c '^ask 1$' "$ST11"); A1=$(kill -0 $WPP11 2>/dev/null && echo 1)
DISPLAY=$DSP xdotool key Escape; sleep 1
A2=$(kill -0 $WPP11 2>/dev/null && echo 1)   # (Cancel has let the window stay, with its writing)
rm -f "$ST11"   # (nothing written any more would be an answer that left the question up: the state is read afresh below)
i=0; while [ $i -lt 24 ] && ! grep -q '^ask 1$' "$ST11" 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
C2=$(grep -c '^ask 1$' "$ST11"); K=$(grep -c '^text Bye$' "$ST11")   # (the close asks again: the writing is still unsaved)
echo "Don't save" > "$A11"   # (the answer the test would press: the question's Don't save)
i=0; while [ $i -lt 24 ] && kill -0 $WPP11 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
G=$(kill -0 $WPP11 2>/dev/null || echo 1)
ok "22. ...and Close asks the same question: Cancel keeps the window, Don't save lets it go" \
	"$([ "$C1$A1$C2$K$A2$G" = 111111 ] && echo 1)" \
	"asked $C1 with the window up $A1; Cancel $C2 kept the writing $K and the window $A2; Don't save took the window down $G ($(grep -E '^(dirty|ask|answer|text) ' "$ST11" 2>/dev/null | tr '\n' ' '); $(tail -2 "$T/wp11.log" 2>/dev/null | tr '\n' ' '))"
# (a format change is a step of the history as typing is: Ctrl+A and Ctrl+B is a step of its own, so Ctrl+Z takes the bold
#  away and leaves the writing, Ctrl+Y puts it back, and an underline over the same choice is a step of its own again)
ST12=$T/state12.txt; rm -f "$ST12"
RUDOLF_WP_STATE=$ST12 DISPLAY=$DSP "$BIN" >"$T/wp12.log" 2>&1 & WPP12=$!
i=0; while [ $i -lt 40 ] && [ ! -s "$ST12" ]; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
W12=$(DISPLAY=$DSP xdotool search --sync --onlyvisible --pid "$WPP12" 2>/dev/null | tail -1)
i=0; while [ $i -lt 60 ] && [ -n "$W12" ]; do DISPLAY=$DSP xdotool windowfocus --sync "$W12" 2>/dev/null
	[ "$(DISPLAY=$DSP xdotool getwindowfocus 2>/dev/null)" = "$W12" ] && break; timeout 0.2 tail -f /dev/null; i=$((i+1)); done
[ -n "$W12" ] && { sleep 0.6; DISPLAY=$DSP xdotool mousemove --window "$W12" 480 300 click 1; sleep 0.8; }
DISPLAY=$DSP xdotool type --delay 12 'Hello world'; sleep 1.2
DISPLAY=$DSP xdotool key ctrl+a; DISPLAY=$DSP xdotool key ctrl+b; sleep 1.2
F1=$(grep -c 'tags b' "$ST12"); H1=$(sed -n 's/^hist //p' "$ST12")
DISPLAY=$DSP xdotool key ctrl+z; sleep 1.2
F2=$(grep -c '^tags$' "$ST12"); W2=$(grep -c '^words 2$' "$ST12"); H2=$(sed -n 's/^hist //p' "$ST12")
DISPLAY=$DSP xdotool key ctrl+y; sleep 1.2
F3=$(grep -c 'tags b' "$ST12"); C3=$(grep -c '^chars 11$' "$ST12"); H3=$(sed -n 's/^hist //p' "$ST12")
DISPLAY=$DSP xdotool key ctrl+a; DISPLAY=$DSP xdotool key ctrl+u; sleep 1.2
U1=$(grep -c 'tags b,u' "$ST12")
DISPLAY=$DSP xdotool key ctrl+z; sleep 1.2
U2=$(grep -c 'tags b$' "$ST12")
NU=${H1#* }
kill $WPP12 2>/dev/null
ok "23. a format change is a step of the history: Ctrl+Z takes the bold away, Ctrl+Y puts it back, and the writing stays" \
	"$([ "$F1$F2$W2$F3$C3$U1$U2" = 1111111 ] && [ "$H2" = "$((NU-1)) $NU" ] && [ "$H3" = "$NU $NU" ] && echo 1)" \
	"bold $F1 ($H1); undone $F2 ($H2) with the writing kept $W2; redone $F3 ($H3) with the writing $C3; an underline over it $U1, and its undo $U2 ($(grep -E '^(wordshist|words|hist|tags) ' "$ST12" 2>/dev/null | tr '\n' ' '); $(tail -2 "$T/wp12.log" 2>/dev/null | tr '\n' ' '))"
# (the question's Save: the page's own order is the save first and New after it, so the writing goes to the file it came
#  from -- the document was opened, so Save writes it without asking where -- and the page that follows is a new one,
#  Untitled, with nothing unsaved and no * in its title)
python3 - "$T/save.DOC" <<'PY'
import sys
open(sys.argv[1], 'w').write('<!--wp:%7B%7D--><style data-wp>.wp-t{border-collapse:collapse}</style><p>Hi</p>')
PY
ST13=$T/state13.txt; rm -f "$ST13"; A13=$T/ans13; : > "$A13"
RUDOLF_WP_STATE=$ST13 RUDOLF_WP_ANSWER=$A13 RUDOLF_WP_OPEN="$T/save.DOC" DISPLAY=$DSP "$BIN" >"$T/wp13.log" 2>&1 & WPP13=$!
i=0; while [ $i -lt 40 ] && ! grep -q '^text Hi$' "$ST13" 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
W13=""; i=0; while [ $i -lt 40 ] && [ -z "$W13" ]; do
	W13=$(DISPLAY=$DSP xdotool search --onlyvisible --pid "$WPP13" 2>/dev/null | tail -1)
	[ -n "$W13" ] || timeout 0.25 tail -f /dev/null; i=$((i+1)); done
i=0; while [ $i -lt 60 ] && [ -n "$W13" ]; do DISPLAY=$DSP xdotool windowfocus --sync "$W13" 2>/dev/null
	[ "$(DISPLAY=$DSP xdotool getwindowfocus 2>/dev/null)" = "$W13" ] && break; timeout 0.2 tail -f /dev/null; i=$((i+1)); done
[ -n "$W13" ] && { sleep 0.6; DISPLAY=$DSP xdotool mousemove --window "$W13" 480 300 click 1; sleep 0.8; }
T0=$(DISPLAY=$DSP xdotool getwindowname "$W13" 2>/dev/null)   # (save.DOC - WordPro: the page has a file)
DISPLAY=$DSP xdotool key ctrl+End; sleep 0.4
DISPLAY=$DSP xdotool type --delay 12 ' there'; sleep 1.2
T1=$(DISPLAY=$DSP xdotool getwindowname "$W13" 2>/dev/null)
D1=$(grep -c '^dirty 1$' "$ST13")
DISPLAY=$DSP xdotool key ctrl+n; sleep 1
i=0; while [ $i -lt 20 ] && ! grep -q '^ask 1$' "$ST13" 2>/dev/null; do timeout 0.2 tail -f /dev/null; i=$((i+1)); done
Q1=$(grep -c '^ask 1$' "$ST13"); K1=$(grep -c '^words 2$' "$ST13")   # (the question is up, and the writing is still there)
echo "Save" > "$A13"; sleep 1.5   # (the answer the test would press: the question's Save)
AN=$(sed -n 's/^answer //p' "$ST13")
W1=$(grep -c 'there' "$T/save.DOC" 2>/dev/null)   # (the change is in the file it came from)
N1=$(grep -c '^dirty 0$' "$ST13"); N2=$(grep -c '^text $' "$ST13")
T2=$(DISPLAY=$DSP xdotool getwindowname "$W13" 2>/dev/null)
kill $WPP13 2>/dev/null
ok "24. the question's Save writes the page to its file, and the page that follows is a clean Untitled one" \
	"$([ "$T0" = 'save.DOC - WordPro' ] && [ "$T1" = 'save.DOC * - WordPro' ] && [ "$D1$Q1$K1$W1" = 1111 ] && [ "$AN" = 2 ] && [ "$N1$N2" = 11 ] && [ "$T2" = 'Untitled - WordPro' ] && echo 1)" \
	"titles '$T0', '$T1', '$T2'; dirty $D1; asked $Q1 with the writing kept $K1; the file has the change $W1 (answer $AN); after it dirty $N1 and empty $N2 ($(grep -E '^(dirty|ask|answer|text|words|hist) ' "$ST13" 2>/dev/null | tr '\n' ' '); $(grep -o '<p>[^<]*</p>' "$T/save.DOC" 2>/dev/null | head -2 | tr '\n' ' '); $(tail -2 "$T/wp13.log" 2>/dev/null | tr '\n' ' '))"
# (a table the page draws without borders and without a heading row: its file says .wp-t nb and no <th>, and the page
#  says it back in its own words -- tbl hdr 0 nb 1 -- and writes the very same class and the very same <td>s again)
python3 - "$T/nb.DOC" <<'PY'
import sys
doc = ('<!--wp:%7B%7D--><style data-wp>.wp-t{border-collapse:collapse}</style>'
       '<table class="wp-t nb"><tr><td>a</td><td>b</td></tr><tr><td>c</td><td>d</td></tr></table>')
open(sys.argv[1], 'w').write(doc)
PY
ST14=$T/state14.txt; rm -f "$ST14" "$T/nb-out.DOC"
RUDOLF_WP_STATE=$ST14 RUDOLF_WP_OPEN="$T/nb.DOC" RUDOLF_WP_SAVE="$T/nb-out.DOC" DISPLAY=$DSP "$BIN" >"$T/wp14.log" 2>&1 & WPP14=$!
i=0; while [ $i -lt 40 ] && ! grep -q '^objs 1$' "$ST14" 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
i=0; while [ $i -lt 60 ] && [ ! -s "$T/nb-out.DOC" ]; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
grep -q '^words 4$' "$ST14" && grep -q '^obj table 2x2$' "$ST14" && grep -q '^tbl hdr 0 nb 1$' "$ST14" &&
	grep -q '<table class="wp-t nb">' "$T/nb-out.DOC" && ! grep -q '<th>' "$T/nb-out.DOC" && grep -q '<td>a</td><td>b</td>' "$T/nb-out.DOC"; RC=$?
kill $WPP14 2>/dev/null
ok "25. ...and the page's .wp-t nb -- no borders, no heading row -- is read and written as it was, with no <th>" "$([ $RC = 0 ] && echo 1)" "$(grep -E '^(words|objs|obj|tbl) ' "$ST14" 2>/dev/null | tr '\n' ' '); $(sed 's/.*<\/style>//' "$T/nb-out.DOC" 2>/dev/null | cut -c1-120); $(tail -2 "$T/wp14.log" 2>/dev/null | tr '\n' ' ')"
cp "$ST" "$OUT/state.txt" 2>/dev/null; cp "$T/wp.log" "$OUT/app.log" 2>/dev/null

for f in "$T/state5.txt" "$T/state6.txt" "$T/state7.txt" "$T/state10.txt" "$T/state11.txt" "$T/state12.txt" "$T/state13.txt" "$T/state14.txt"; do [ -f "$f" ] && cp "$f" "$OUT/$(basename "$f")"; done
[ -f "$T/out.DOC" ] && head -c 4000 "$T/out.DOC" > "$OUT/out.txt" 2>/dev/null; [ -f "$T/out.DOC" ] && wc -c < "$T/out.DOC" > "$OUT/out-size.txt"
echo "$NP passed, $NF failed"; [ $NF = 0 ]
