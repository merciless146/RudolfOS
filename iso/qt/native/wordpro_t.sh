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
# ---- 10. undo puts the writing back *with its marks*: the bold is still there, and the cursor is where it was
# (a step is the writing and its tag set, so undoing a letter no longer takes every mark in the document with it)
DISPLAY=$DSP xdotool mousemove --window "$WIN" 480 300 click 1; sleep 0.5
DISPLAY=$DSP xdotool key ctrl+a; DISPLAY=$DSP xdotool key ctrl+b; sleep 0.8   # (bold all of it)
DISPLAY=$DSP xdotool key Right; sleep 0.3; DISPLAY=$DSP xdotool type --delay 12 '!'; sleep 0.9
grep -q '^chars 7$' "$ST"   # (the letter went in: 6 + 1, the newline not a character)
DISPLAY=$DSP xdotool key ctrl+z; sleep 1.0
RC=0
grep -q '^chars 6$' "$ST" || RC=1
grep '^tags' "$ST" | grep -q 'b' || RC=1     # (the undo did not take the bold with it)
ok "10. Ctrl+Z takes the letter back and leaves the bold where it was" \
	"$([ $RC = 0 ] && echo 1)" "$(grep -E '^(chars|tags|text) ' "$ST" | tr '\n' ' ')"
# ---- 11. Find marks what it found (the page's highlight, not only the choice)
DISPLAY=$DSP xdotool key ctrl+f; sleep 0.6
DISPLAY=$DSP xdotool type --delay 12 'two'; DISPLAY=$DSP xdotool key Return; sleep 0.9
grep '^tags' "$ST" | grep -q 'find'
ok "11. Ctrl+F marks the match (the find tag is on the writing, not only on the choice)" \
	"$([ $? = 0 ] && echo 1)" "$(grep -E '^(tags|sel) ' "$ST" | tr '\n' ' ')"
kill $WPP 2>/dev/null; sleep 0.6
# (the app of the first cases is put away by its window's own process: it was a child of this shell rather than the shell,
#  and two WordPro windows on one display take each other's presses)
for w in $(DISPLAY=$DSP xdotool search --name '^WordPro$' 2>/dev/null); do
	p=$(DISPLAY=$DSP xdotool getwindowpid "$w" 2>/dev/null); [ -n "$p" ] && kill "$p" 2>/dev/null; done
sleep 1.5
# ---- 12. a bullet goes on with one press and comes off with the next (the page's own toggle)
# (RUDOLF_WP_DO is the check's hand on the toolbar's buttons, which it cannot aim; the command runs once there is writing)
ST10=$T/state_bul; rm -f "$ST10"
RUDOLF_WP_STATE=$ST10 RUDOLF_WP_DO=bul DISPLAY=$DSP "$BIN" >"$T/wp_bul.log" 2>&1 & WPP2=$!
i=0; while [ $i -lt 60 ] && [ ! -s "$ST10" ]; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
WIN2=""; i=0; while [ $i -lt 60 ] && [ -z "$WIN2" ]; do   # (this app's window, by its process)
	for w in $(DISPLAY=$DSP xdotool search --name '^WordPro$' 2>/dev/null); do
		[ "$(DISPLAY=$DSP xdotool getwindowpid "$w" 2>/dev/null)" = "$WPP2" ] && WIN2=$w && break; done
	[ -z "$WIN2" ] && timeout 0.2 tail -f /dev/null; i=$((i+1)); done
i=0; while [ $i -lt 60 ] && [ -n "$WIN2" ]; do DISPLAY=$DSP xdotool windowfocus --sync "$WIN2" 2>/dev/null
	[ "$(DISPLAY=$DSP xdotool getwindowfocus 2>/dev/null)" = "$WIN2" ] && break; timeout 0.2 tail -f /dev/null; i=$((i+1)); done
sleep 2; DISPLAY=$DSP xdotool windowraise "$WIN2"; sleep 0.5   # (the first app's window is still on the display: this one is above it)
DISPLAY=$DSP xdotool mousemove --window "$WIN2" 480 300 click 1; sleep 0.8
i=0; while [ $i -lt 3 ]; do
	DISPLAY=$DSP xdotool type --delay 12 'Hello'; sleep 1
	grep -q '^chars 5$' "$ST10" && break
	DISPLAY=$DSP xdotool key ctrl+a; DISPLAY=$DSP xdotool key BackSpace; sleep 0.4; i=$((i+1)); done
sleep 1.6
grep -q '^text • Hello$' "$ST10"
ok "12. Bullets puts a bullet on the writing" "$([ $? = 0 ] && echo 1)" "$(head -12 "$ST10" | tr '\n' ' '); $(head -3 "$T/wp_bul.log")"
kill $WPP2 2>/dev/null; sleep 0.5
# ---- 13. ...and a second press takes it off (before this, every press added another one)
ST11=$T/state_bul2; rm -f "$ST11"
RUDOLF_WP_STATE=$ST11 RUDOLF_WP_DO=bul,bul DISPLAY=$DSP "$BIN" >"$T/wp_bul2.log" 2>&1 & WPP3=$!
i=0; while [ $i -lt 60 ] && [ ! -s "$ST11" ]; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
WIN3=""; i=0; while [ $i -lt 60 ] && [ -z "$WIN3" ]; do   # (this app's window, by its process)
	for w in $(DISPLAY=$DSP xdotool search --name '^WordPro$' 2>/dev/null); do
		[ "$(DISPLAY=$DSP xdotool getwindowpid "$w" 2>/dev/null)" = "$WPP3" ] && WIN3=$w && break; done
	[ -z "$WIN3" ] && timeout 0.2 tail -f /dev/null; i=$((i+1)); done
i=0; while [ $i -lt 60 ] && [ -n "$WIN3" ]; do DISPLAY=$DSP xdotool windowfocus --sync "$WIN3" 2>/dev/null
	[ "$(DISPLAY=$DSP xdotool getwindowfocus 2>/dev/null)" = "$WIN3" ] && break; timeout 0.2 tail -f /dev/null; i=$((i+1)); done
sleep 2; DISPLAY=$DSP xdotool windowraise "$WIN3"; sleep 0.5
DISPLAY=$DSP xdotool mousemove --window "$WIN3" 480 300 click 1; sleep 0.8
i=0; while [ $i -lt 3 ]; do
	DISPLAY=$DSP xdotool type --delay 12 'Hello'; sleep 1
	grep -q '^chars 5$' "$ST11" && break
	DISPLAY=$DSP xdotool key ctrl+a; DISPLAY=$DSP xdotool key BackSpace; sleep 0.4; i=$((i+1)); done
sleep 1.8
grep -q '^text Hello$' "$ST11"
ok "13. a second Bullets press takes the bullet off again" "$([ $? = 0 ] && echo 1)" "$(head -12 "$ST11" | tr '\n' ' '); $(head -3 "$T/wp_bul2.log")"
kill $WPP3 2>/dev/null; sleep 0.4
# the build-time reading of the page: a page without the lists stops the build instead of quietly shipping a blank toolbar
mkdir -p "$T/page/iso/sys/apps" "$T/page/iso/sys/ui" "$T/page/desktop"
sed 's/const WP_STYLES=/const WP_STYLES_GONE=/' "$R/desktop/base_v24.html" > "$T/page/desktop/base_v24.html"
cp "$R/iso/sys/apps/mkwordpro.py" "$T/page/iso/sys/apps/mkwordpro.py"
python3 "$T/page/iso/sys/apps/mkwordpro.py" "$T/gone.h" >"$T/mk.log" 2>&1; RC=$?
if [ $RC = 1 ] && grep -q 'WP_STYLES' "$T/mk.log"; then
	ok "9. mkwordpro.py refuses a page it cannot read (no quiet default)" 1
else
	ok "9. mkwordpro.py refuses a page it cannot read (no quiet default)" 0 "rc=$RC $(cat "$T/mk.log")"
fi
kill -0 $WPP 2>/dev/null || echo "NOTE the app had exited: $(cat "$T/app.exit" 2>/dev/null)"
cp "$ST" "$OUT/state.txt" 2>/dev/null; cp "$T/wp.log" "$OUT/app.log" 2>/dev/null
echo "$NP passed, $NF failed"; [ $NF = 0 ]
