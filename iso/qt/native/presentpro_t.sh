#!/bin/sh
# 1.6.3 (docs/plan-1.6.3.md Part 1): rudolf-present's first step -- the page's window, its toolbar, its eight menus, its
# list of slides, its stage and its notes, with the page's eight layouts, the text typed into a place, bold, italic,
# underline, strikethrough, the three alignments, bullets and numbering, undo and redo, and the page's keys -- built as the
# release builds it (iso/sys/ui/build.sh) and then used: a title typed in the editor, a second slide made the page's way, a
# body's two lines, bold and the lists, undo, the page's PageUp and PageDown, a layout that keeps what it still has, a
# duplicated and a deleted slide, a hidden slide, the speaker's notes, the menus' own rows, and Enter opening the place the
# stage has. It reads RUDOLF_PP_STATE, so no screenshot is needed here; the page-by-page comparison against the web
# PresentPro comes with the step that makes every one of its views.
#   sh presentpro_t.sh [OUT]
# Needs Xvfb, xdotool, node, gcc and GTK 3's headers. (display :90, port 8686)
HERE=$(cd "$(dirname "$0")" && pwd); R=$(cd "$HERE/../../.." && pwd); T=$(mktemp -d /tmp/pp_t.XXXXXX)
PORT=${PP_PORT:-8686}; DSP=${PP_DSP:-:90}; OUT=${1:-$T/out}; mkdir -p "$OUT"
command -v Xvfb >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs Xvfb and xdotool"; exit 0; }
BIN=$T/rudolf-present
sh "$R/iso/sys/ui/build.sh" "$T/ui" "$BIN" presentpro 2>"$T/cc" || { cat "$T/cc"; echo "FAIL build"; exit 1; }
[ -s "$T/cc" ] && { echo "FAIL rudolf-present builds with warnings:"; cat "$T/cc"; exit 1; }
echo "PASS 0. rudolf-present builds without a warning ($(stat -c %s "$BIN") bytes)"
export FONTCONFIG_FILE=$HERE/fonts-test.conf
D=$T/d; mkdir -p "$D"
node "$R/iso/qt/host/fixtures/deskls.js" "$D/ls.json" "$D/keys.json" || { echo "FAIL node"; exit 1; }
python3 - "$D" <<'Y'
import json, sys
D = sys.argv[1]; k = json.load(open(D + '/keys.json'))['Ann']
json.dump({'ok': True, 'id': k['id'], 'n': 'Ann', 'r': 'admin', 'pk': k['pk'], 'fk': k['fk']}, open(D + '/session', 'w'))
Y
python3 "$HERE/same_rd.py" "$PORT" "$D" >"$T/rd.log" 2>&1 & RDP=$!
Xvfb "$DSP" -screen 0 1400x1000x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
trap 'kill $APPP $RDP $XV 2>/dev/null; rm -rf "$T/ui" "$T/cc"' EXIT
i=0; while [ $i -lt 40 ] && ! DISPLAY=$DSP xdotool getmouselocation >/dev/null 2>&1; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
# ---- 0b. it opens its window and writes the state the checks read (gdb says why when it does not)
if command -v gdb >/dev/null 2>&1; then
	rm -f "$T/smoke"
	RUDOLF_PP_STATE=$T/smoke DISPLAY=$DSP timeout 12 gdb -batch -ex 'set confirm off' -ex run -ex 'bt 12' --args "$BIN" >"$T/gdb.txt" 2>&1
	if [ ! -s "$T/smoke" ]; then
		echo "FAIL the program does not open its window; gdb says:"; tail -40 "$T/gdb.txt"; echo "FAIL 0b"; exit 1
	fi
	echo "PASS 0b. it opens its window and writes the state the checks read"
fi
ST=$T/state; APPP=
NP=0; NF=0
ok() { if [ "$2" = 1 ]; then NP=$((NP+1)); echo "PASS $1"; else NF=$((NF+1)); echo "FAIL $1 - $3"; fi; }
start() { rm -f "$ST"; env "$@" RUDOLF_PP_STATE=$ST DISPLAY=$DSP "$BIN" >>"$T/app.log" 2>&1 & APPP=$!
	i=0; while [ $i -lt 80 ] && [ ! -s "$ST" ]; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
	i=0; while [ $i -lt 24 ] && [ -z "$WIN" ]; do WIN=$(DISPLAY=$DSP xdotool search --name 'PresentPro$' 2>/dev/null | head -1); timeout 0.25 tail -f /dev/null; i=$((i+1)); done
	sleep 0.5; }
stop() { [ -n "$APPP" ] && { kill "$APPP" 2>/dev/null; wait "$APPP" 2>/dev/null; APPP=; }; WIN=; sleep 0.2; }
# (the window takes the keys, as a person's first press gives them to it: there is no window manager on this display)
focus() { i=0; while [ $i -lt 40 ]; do DISPLAY=$DSP xdotool windowfocus --sync "$1" 2>/dev/null
	[ "$(DISPLAY=$DSP xdotool getwindowfocus 2>/dev/null)" = "$1" ] && return 0; timeout 0.2 tail -f /dev/null; i=$((i+1)); done; return 0; }
WIN=

# ---- 1. the window: the page's, with its eight menus and its one title slide
start
RC=0; grep -q '^title Untitled presentation - PresentPro$' "$ST" || RC=1
grep -q '^menus 8$' "$ST" || RC=1
grep -q '^slides 1$' "$ST" || RC=1
grep -q '^cur 0$' "$ST" || RC=1
grep -q '^layout title$' "$ST" || RC=1
grep -q '^theme Office$' "$ST" || RC=1
grep -q '^size 960x540$' "$ST" || RC=1
grep -q '^zoom Fit$' "$ST" || RC=1
[ -n "$WIN" ] || RC=1
ok "1. the window opens, called 'Untitled presentation - PresentPro', with the page's eight menus and one title slide" \
	"$([ $RC = 0 ] && echo 1)" "no window, or $(head -9 "$ST" 2>/dev/null); $(cat "$T/app.log")"
stop

# ---- 2. the toolbar's and the menus' lists are the page's own (mkpresentpro.py reads them at build time)
start
WANT=$(python3 - "$R" <<'Y'
import re, sys
R = sys.argv[1]
office = open(R + '/desktop/office45.js', encoding='utf-8').read()
ppro = open(R + '/desktop/ppro45.js', encoding='utf-8').read()
pprof = open(R + '/desktop/pprof45.js', encoding='utf-8').read()
fonts = len(re.findall(r"'([^']+)'", re.search(r'const OFF_FONTS=\[([^\]]*)\];', office).group(1)))
sizes = len(re.sub(r'\s', '', re.search(r"title='Font size'>\$\{\[([0-9,\s]+)\]", ppro).group(1)).split(','))
themes = len(re.findall(r"[A-Za-z0-9_]+:\{", re.search(r'const PP_THEMES=\{(.*?)\n\};', pprof, re.S).group(1)))
lays = len(re.findall(r"\['[^']+','", re.search(r'const PP_LAYOUTS=\[(.*?)\];', pprof, re.S).group(1)))
shapes = len(re.findall(r"\['[^']+','", re.search(r'const PP_SHAPES=\[(.*?)\];', pprof, re.S).group(1)))
print('fonts %d sizes %d layouts %d themes %d shapes %d' % (fonts, sizes, lays, themes, shapes))
Y
)
grep -q "^lists $WANT\$" "$ST"
ok "2. the toolbar's and the menus' lists are the page's ($WANT)" "$([ $? = 0 ] && echo 1)" "wanted '$WANT', got '$(grep '^lists ' "$ST")'"
stop

# ---- 3. a title is typed into the place, as the page's editing object takes it
start RUDOLF_PP_EDIT=title
focus "$WIN"
DISPLAY=$DSP xdotool type --delay 25 "Quarterly review"; sleep 0.5
DISPLAY=$DSP xdotool key Escape; sleep 0.8
grep -q '^text title Quarterly review$' "$ST"
ok "3. a title typed into the place is on the slide" "$([ $? = 0 ] && echo 1)" "$(grep -E '^(text|fmt) ' "$ST" | tr '\n' ' ') $(cat "$T/app.log")"
stop

# ---- 4. Ctrl+M makes the page's second slide (a content one), and its two lines are the slide's
start RUDOLF_PP_DO="new;text:body=First point
Second point"
RC=0; grep -q '^slides 2$' "$ST" || RC=1
grep -q '^cur 1$' "$ST" || RC=1
grep -q '^layout content$' "$ST" || RC=1
grep -q '^text body First point|Second point$' "$ST" || RC=1
grep -q '^slide 0 title $' "$ST" || RC=1
grep -q '^slide 1 content $' "$ST" || RC=1
ok "4. a new slide follows the page's rule (a title slide is followed by a content one) and keeps its two lines" \
	"$([ $RC = 0 ] && echo 1)" "$(grep -E '^(slides|cur|layout|text|slide) ' "$ST" | tr '\n' ' ')"
stop

# ---- 5. the toolbar's bold marks the place, and the state says so
start RUDOLF_PP_DO="new;text:title=Numbers;bold"
grep -q '^fmt title b1 ' "$ST"
ok "5. bold marks the place it is chosen on, and nothing else" "$([ $? = 0 ] && echo 1)" "$(grep '^fmt ' "$ST")"
stop

# ---- 6. the page's Home bullet takes the layout's list away, and its numbering puts numbers in
start RUDOLF_PP_DO="new;tab;bullets"
RC=0; grep -q '^fmt body b0 i0 u0 s0 ha-1 bl0 ' "$ST" || RC=1
ok "6. the bullets item takes the body's list away (the layout gives it one)" "$([ $RC = 0 ] && echo 1)" "$(grep '^fmt ' "$ST")"
stop
start RUDOLF_PP_DO="new;tab;numbering"
grep -q '^fmt body b0 i0 u0 s0 ha-1 bl2 ' "$ST"
ok "7. the numbering item puts numbers in the body" "$([ $? = 0 ] && echo 1)" "$(grep '^fmt ' "$ST")"
stop

# ---- 8. undo goes back from what is on the screen, as WordPro's and SpreadsheetPro's does
start RUDOLF_PP_DO="new;text:title=A;text:title=B;undo"
RC=0; grep -q '^text title A$' "$ST" || RC=1
ok "8. undo goes back a step (the title reads A again), and the history says how many steps there are" "$([ $RC = 0 ] && echo 1)" \
	"$(grep -E '^(text|hist) ' "$ST" | tr '\n' ' ')"
stop

# ---- 9. the page's PageDown and PageUp walk the deck
start RUDOLF_PP_DO="new"
focus "$WIN"
DISPLAY=$DSP xdotool key Page_Up; sleep 0.7
A=$(grep '^cur ' "$ST")
DISPLAY=$DSP xdotool key Page_Down; sleep 0.7
B=$(grep '^cur ' "$ST")
[ "$A" = "cur 0" ] && [ "$B" = "cur 1" ]
ok "9. PageUp and PageDown go back and forward a slide" "$([ $? = 0 ] && echo 1)" "after PageUp '$A', after PageDown '$B'"
stop

# ---- 10. a layout keeps what it still has and drops what it has not
start RUDOLF_PP_DO="new;tab;text:body=Keep me;layout:two"
RC=0; grep -q '^layout two$' "$ST" || RC=1
grep -q '^text body Keep me$' "$ST" || RC=1
grep -q '^text body2 ' "$ST" && RC=1
ok "10. the two-content layout keeps the body it still has and the second place is empty" "$([ $RC = 0 ] && echo 1)" \
	"$(grep -E '^(layout|text) ' "$ST" | tr '\n' ' ')"
stop

# ---- 11. a slide is duplicated and deleted
start RUDOLF_PP_DO="new;text:title=Twice;dup"
RC=0; grep -q '^slides 3$' "$ST" || RC=1
grep -q '^slide 2 content Twice$' "$ST" || RC=1
ok "11. a slide is duplicated with its words (three slides, the third reading 'Twice')" "$([ $RC = 0 ] && echo 1)" \
	"$(grep -E '^(slides|slide) ' "$ST" | tr '\n' ' ')"
stop
start RUDOLF_PP_DO="new;dup;del"
RC=0; grep -q '^slides 2$' "$ST" || RC=1
ok "12. a slide is deleted, and the deck keeps the one it had" "$([ $RC = 0 ] && echo 1)" "$(grep -E '^(slides|slide) ' "$ST" | tr '\n' ' ')"
stop

# ---- 13. a hidden slide, and the speaker's notes
start RUDOLF_PP_DO="new;hide;notes=The speaker says this"
RC=0; grep -q '^hidden 1$' "$ST" || RC=1
grep -q "^slide 1 content  hidden\$" "$ST" || RC=1
grep -q '^notes The speaker says this$' "$ST" || RC=1
ok "13. a slide is hidden (the list and the state say so) and the notes are kept with it" "$([ $RC = 0 ] && echo 1)" \
	"$(grep -E '^(hidden|slide|notes) ' "$ST" | tr '\n' ' ')"
stop

# ---- 14. the menus are the page's own: File's 18 rows, Home's 20 and Slide Show's 7
M_OK=1
for m in File:18 Home:20 "Slide Show:7"; do
	n=${m#*:}; name=${m%:*}
	start RUDOLF_PP_MENU="$name"; sleep 0.4
	grep -q "^menu $name $n\$" "$ST" || M_OK=0
	stop
done
ok "14. the page's menus open with its own rows (File 18, Home 20, Slide Show 7)" "$M_OK" "$(grep '^menu ' "$T/menus" 2>/dev/null)"

# ---- 15. Enter opens the place the stage has, and what is typed is the slide's
start
focus "$WIN"
DISPLAY=$DSP xdotool key Return; sleep 1
DISPLAY=$DSP xdotool type --delay 25 "Hello"; sleep 0.5
DISPLAY=$DSP xdotool key Escape; sleep 0.8
grep -q '^text title Hello$' "$ST"
ok "15. Enter opens the title's place, and what is typed goes on the slide" "$([ $? = 0 ] && echo 1)" \
	"$(grep -E '^(text|say) ' "$ST" | tr '\n' ' '); $(cat "$T/app.log")"
stop

cp "$ST" "$OUT/state.txt" 2>/dev/null
cat "$T/app.log" > "$OUT/app.log" 2>/dev/null
echo
echo "$NP passed, $NF failed"
[ "$NF" = 0 ] || exit 1
