#!/bin/sh
# 1.6.3 (docs/plan-1.6.3.md Part 1): rudolf-wordpro, built as the release builds it (iso/sys/ui/build.sh, RUI_WERROR=1)
# and then used: the window opens, typing counts words and characters, Ctrl+B tags what is chosen, Ctrl+F finds, the
# toolbar's lists are the page's (mkwordpro.py), the menu bar is the page's eight menus, and Insert ▸ Picture… puts a
# picture in (scaled as the page scales it) which undo and redo keep as a step of their own. It reads RUDOLF_WP_STATE, so
# no screenshot is needed here; the page-by-page comparison against the web WordPro comes with the step that makes every
# one of its views.
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
# (the app of the first cases is put away by its window's own process: it was a child of this shell rather than the shell)
for w in $(DISPLAY=$DSP xdotool search --name '^WordPro$' 2>/dev/null); do
	p=$(DISPLAY=$DSP xdotool getwindowpid "$w" 2>/dev/null); [ -n "$p" ] && kill "$p" 2>/dev/null; done
sleep 1.5
# (a fresh app of the same kind: the window takes the keys, the press puts the writing ready -- retried, because a press
#  into a window that has just appeared can be lost -- and RUDOLF_WP_DO is the check's hand on the toolbar's buttons)
start_bul() {
	ST_B=$1; rm -f "$ST_B"
	RUDOLF_WP_STATE=$ST_B RUDOLF_WP_DO=$2 DISPLAY=$DSP "$BIN" >"$T/wp_bul.log" 2>&1 & WPP_B=$!
	i=0; while [ $i -lt 80 ] && [ ! -s "$ST_B" ]; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
	WIN_B=$(DISPLAY=$DSP xdotool search --sync --name '^WordPro$' 2>/dev/null | head -1)
	i=0; while [ $i -lt 80 ] && [ -n "$WIN_B" ]; do DISPLAY=$DSP xdotool windowfocus --sync "$WIN_B" 2>/dev/null
		[ "$(DISPLAY=$DSP xdotool getwindowfocus 2>/dev/null)" = "$WIN_B" ] && break; timeout 0.2 tail -f /dev/null; i=$((i+1)); done
	sleep 2; DISPLAY=$DSP xdotool mousemove --window "$WIN_B" 480 300 click 1; sleep 0.8
	i=0; while [ $i -lt 8 ]; do
		DISPLAY=$DSP xdotool type --delay 12 'Hello'; sleep 1.2
		grep -q '^chars 5$' "$ST_B" && break
		DISPLAY=$DSP xdotool key ctrl+a; DISPLAY=$DSP xdotool key BackSpace; sleep 0.4; i=$((i+1)); done
}
wait_for() { i=0; while [ $i -lt 60 ] && ! eval "$1" >/dev/null 2>&1; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done; }
# ---- 12. a bullet goes on with one press and comes off with the next (the page's own toggle)
start_bul "$T/state_bul" bul
wait_for 'grep -q "^chars 7$" "$ST_B"'     # (the five letters and the bullet's two)
ok "12. Bullets puts a bullet on the writing" "$([ $? = 0 ] && echo 1)" \
	"$(head -14 "$ST_B" | tr '\n' ' '); $(head -6 "$T/wp_bul.log")"
kill $WPP_B 2>/dev/null; sleep 0.6
# ---- 13. ...and a second press takes it off again (before this, every press added another one)
start_bul "$T/state_bul2" "bul,bul"
wait_for 'grep -q "^chars 5$" "$ST_B" && grep -q "^text Hello$" "$ST_B"'
ok "13. a second Bullets press takes the bullet off again" "$([ $? = 0 ] && echo 1)" \
	"$(head -14 "$ST_B" | tr '\n' ' '); $(head -6 "$T/wp_bul.log")"
kill $WPP_B 2>/dev/null; sleep 0.4
# ---- 14. the menu bar is the page's own eight menus, in its order (base_v24.html's MENUS)
grep -q '^mb 8 File,Edit,View,Insert,Format,Table,Tools,Help$' "$ST"
ok "14. the menu bar has the page's eight menus, in its order" "$([ $? = 0 ] && echo 1)" "$(grep '^mb' "$ST")"
# ---- 15. File opens with the page's items, and the ones whose step is later are greyed (and so refuse)
start_bul "$T/state_menu" menu:0
wait_for 'grep -q "^menuopen File 14$" "$ST_B"'
if grep -q '^menuopen File 14$' "$ST_B" && grep -q '^grey .*Page setup…' "$ST_B" && grep -q '^grey .*Print…' "$ST_B" \
   && ! grep -q '^grey .* Close$' "$ST_B"; then G=1; else G=0; fi
ok "15. File's menu is the page's, with the later steps greyed" "$G" "$(grep -E '^(menuopen|grey)' "$ST_B")"
kill $WPP_B 2>/dev/null; sleep 0.6
# ---- 16. a command from a menu does what the toolbar's button does (Format > Bold)
start_bul "$T/state_bold" mi:4:0
wait_for 'grep -q "^chars 5$" "$ST_B"'
grep -q 'tags b' "$ST_B"
ok "16. Format > Bold bolds the writing, as the toolbar's B does" "$([ $? = 0 ] && echo 1)" "$(head -14 "$ST_B" | tr '\n' ' ')"
kill $WPP_B 2>/dev/null; sleep 0.4
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
# ---- 17. the menu bar is the page's: a page without MENUS stops the build instead of leaving a hand-copied bar
cp "$R/desktop/base_v24.html" "$T/page/desktop/base_v24.html"
sed -i 's/const MENUS=/const MENUS_GONE=/' "$T/page/desktop/base_v24.html"
python3 "$T/page/iso/sys/apps/mkwordpro.py" "$T/gone2.h" >"$T/mk2.log" 2>&1; RC=$?
if [ $RC = 1 ] && grep -q 'MENUS' "$T/mk2.log"; then
	ok "17. mkwordpro.py refuses a page without the menu bar (no hand-copied labels)" 1
else
	ok "17. mkwordpro.py refuses a page without the menu bar (no hand-copied labels)" 0 "rc=$RC $(cat "$T/mk2.log")"
fi
# ---- 18. the page's commands the app has not got are counted, menu by menu (62 of them now): a command the page adds
#      moves a number here, so it cannot arrive as a silent greyed entry, and a wired one the page renames or drops stops
#      the build by its id
L18=1
for pair in "File 10" "Edit 4" "View 6" "Insert 14" "Format 10" "Table 12" "Tools 4" "Help 1"; do
	grep -q "^later $pair$" "$ST" || L18=0
done
ok "18. the page's commands the app has not got are counted, menu by menu" "$L18" "$(grep '^later' "$ST" | tr '\n' ' ')"
kill -0 $WPP 2>/dev/null || echo "NOTE the app had exited: $(cat "$T/app.exit" 2>/dev/null)"
# ---- 19. Insert › Picture… puts a picture in as the page's readImg does: at most 1000 px, and no wider than the writing
#      area (the sheet's 816 px inside its two 96 px margins), so a 900x300 picture shows at 624x208
IMG=$T/pic.png
if command -v convert >/dev/null 2>&1; then convert -size 900x300 "xc:#c81e1e" "$IMG" 2>/dev/null; fi
[ -s "$IMG" ] || python3 -c "from PIL import Image; Image.new('RGB',(900,300),(200,30,30)).save('$IMG')" 2>/dev/null
[ -s "$IMG" ] || { echo "FAIL no picture to insert (no ImageMagick, no PIL)"; exit 1; }
start_bul "$T/state_pic" "pic:$IMG"
wait_for 'grep -q "^pics 1$" "$ST_B"'
if grep -q '^pics 1$' "$ST_B" && grep -q '^pic 0 624x208$' "$ST_B" && grep -q '^chars 5$' "$ST_B"; then P19=1; else P19=0; fi
ok "19. Insert ▸ Picture… puts the picture in at the page's width (900x300 shows 624x208), and it is no character" \
	"$P19" "$(head -22 "$ST_B" | tr '\n' ' '); $(head -4 "$T/wp_bul.log")"
kill $WPP_B 2>/dev/null; sleep 0.5
# ---- 20. the history keeps the picture: Ctrl+Z takes it back out (the writing stays), Ctrl+Y puts it back -- a picture is
#      a step of its own, where before the whole writing was put back without it
start_bul "$T/state_pic_undo" "pic:$IMG,undo"
wait_for 'grep -q "^say $" "$ST_B" && grep -q "^chars 5$" "$ST_B"'
if grep -q '^pics 0$' "$ST_B" && grep -q '^chars 5$' "$ST_B" && grep -q '^text Hello$' "$ST_B"; then P20=1; else P20=0; fi
ok "20. Ctrl+Z takes the picture out and leaves the writing" "$P20" "$(head -22 "$ST_B" | tr '\n' ' ')"
kill $WPP_B 2>/dev/null; sleep 0.4
start_bul "$T/state_pic_redo" "pic:$IMG,undo,redo"
wait_for 'grep -q "^pics 1$" "$ST_B"'
ok "21. Ctrl+Y brings the picture back" "$([ $? = 0 ] && echo 1)" "$(head -22 "$ST_B" | tr '\n' ' ')"
kill $WPP_B 2>/dev/null; sleep 0.4
# ---- 22. a file that is not a picture is refused with the page's words, and nothing goes in
printf 'not a picture\n' > "$T/notpic.txt"
start_bul "$T/state_pic_bad" "pic:$T/notpic.txt"
wait_for 'grep -q "^say .*not an image file" "$ST_B"'
if grep -q '^pics 0$' "$ST_B" && grep -q '^say .*not an image file' "$ST_B"; then P22=1; else P22=0; fi
ok "22. a file that is not a picture is refused, and nothing goes in" "$P22" "$(head -22 "$ST_B" | tr '\n' ' ')"
kill $WPP_B 2>/dev/null; sleep 0.4
cp "$ST" "$OUT/state.txt" 2>/dev/null; cp "$T/wp.log" "$OUT/app.log" 2>/dev/null
echo "$NP passed, $NF failed"; [ $NF = 0 ]
