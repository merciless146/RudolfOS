#!/bin/sh
# 1.6.3 (docs/plan-1.6.3.md Part 1): rudolf-sheetpro's first step -- the page's window, its toolbar, its eight menus, its
# formula bar and its grid, with values and formulas, undo and redo, and the page's keys -- built as the release builds it
# (iso/sys/ui/build.sh) and then used: typing puts values in cells and Enter goes down, a formula is worked out and shown,
# Tab and Enter run along a row and come back to its first column, Ctrl+` shows the formulas, Ctrl+B marks a cell, undo and
# redo walk the changes, the name box chooses a cell or a range, the status line adds up a choice, and Ctrl+Down goes to the
# end of a filled run. The menus are the page's own (File's 15 rows and Data's 12) and the toolbar's lists are the page's
# (mksheetpro.py). It reads RUDOLF_SP_STATE, so no screenshot is needed here; the page-by-page comparison against the web
# SpreadsheetPro comes with the step that makes every one of its views.
#   sh sheetpro_t.sh [OUT]
# Needs Xvfb, xdotool, node, gcc and GTK 3's headers. (display :89, port 8685)
HERE=$(cd "$(dirname "$0")" && pwd); R=$(cd "$HERE/../../.." && pwd); T=$(mktemp -d /tmp/sp_t.XXXXXX)
PORT=${SP_PORT:-8685}; DSP=${SP_DSP:-:89}; OUT=${1:-$T/out}; mkdir -p "$OUT"
command -v Xvfb >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs Xvfb and xdotool"; exit 0; }
BIN=$T/rudolf-sheetpro
sh "$R/iso/sys/ui/build.sh" "$T/ui" "$BIN" sheetpro 2>"$T/cc" || { cat "$T/cc"; echo "FAIL build"; exit 1; }
[ -s "$T/cc" ] && { echo "FAIL rudolf-sheetpro builds with warnings:"; cat "$T/cc"; exit 1; }
echo "PASS 0. rudolf-sheetpro builds without a warning ($(stat -c %s "$BIN") bytes)"
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
RUDOLF_SP_STATE=$ST RUDOLF_SP_GOTO=A1 DISPLAY=$DSP sh -c '"$@"; echo "app exit $?" >> "'"$T"'/app.exit"' sh "$BIN" >"$T/sp.log" 2>&1 & SPP=$!
trap 'kill $SPP $RDP $XV 2>/dev/null; rm -rf "$T/ui" "$T/cc"' EXIT
NP=0; NF=0
ok() { if [ "$2" = 1 ]; then NP=$((NP+1)); echo "PASS $1"; else NF=$((NF+1)); echo "FAIL $1 - $3"; fi; }
i=0; while [ $i -lt 60 ] && [ ! -s "$ST" ]; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
WIN=$(DISPLAY=$DSP xdotool search --sync --name 'SpreadsheetPro$' 2>/dev/null | head -1)
grep -q '^title Book1 - SpreadsheetPro$' "$ST" 2>/dev/null && grep -q '^menus 8$' "$ST"; RC=$?
[ -n "$WIN" ] || RC=1
ok "1. the window opens, called 'Book1 - SpreadsheetPro', with the page's eight menus" "$([ $RC = 0 ] && echo 1)" "no window, or $(head -4 "$ST" 2>/dev/null); $(cat "$T/sp.log")"
# (the window takes the keys, as a person's first press gives them to it; there is no window manager on this display, so
#  the focus is set with X's own request, waited for, and the press goes in as the X server's own event)
i=0; while [ $i -lt 60 ]; do DISPLAY=$DSP xdotool windowfocus --sync "$WIN" 2>/dev/null
	[ "$(DISPLAY=$DSP xdotool getwindowfocus 2>/dev/null)" = "$WIN" ] && break; timeout 0.2 tail -f /dev/null; i=$((i+1)); done
# (the grid has the focus: RUDOLF_SP_GOTO=A1 puts the cursor there as the window opens, and a press of Down shows that the
#  keys are its own -- tried again, as a person would, while a key sent to a window that has just appeared can be lost)
i=0; while [ $i -lt 8 ]; do
	DISPLAY=$DSP xdotool key Down; sleep 0.5
	grep -q '^sel 1 0 1 0 1 0$' "$ST" && break; i=$((i+1)); done
DISPLAY=$DSP xdotool key ctrl+Home; sleep 0.6
# (a cell is typed into as a person types: the characters go into the grid, and Enter puts them in and goes down)
cell() { DISPLAY=$DSP xdotool type --delay 20 "$1"; sleep 0.4; DISPLAY=$DSP xdotool key Return; sleep 0.7; }
cell 5; cell 7; cell '=A1+A2'
grep -q '^cell A1 5$' "$ST" && grep -q '^cell A2 7$' "$ST" && grep -q '^cell A3 =A1+A2$' "$ST" && grep -q '^show A3 12$' "$ST"
ok "2. typing puts the values in, and a formula is worked out and shown (A3 = A1 + A2 is 12)" "$([ $? = 0 ] && echo 1)" "$(grep -E '^(cell|show) ' "$ST" | tr '\n' ' ')"
DISPLAY=$DSP xdotool key ctrl+Home; sleep 0.4; DISPLAY=$DSP xdotool key Down; DISPLAY=$DSP xdotool key Down; sleep 0.7
grep -q '^fx =A1+A2$' "$ST" && grep -q '^selname A3$' "$ST"
ok "3. the formula bar shows what the cell holds, and the name box where the cursor is" "$([ $? = 0 ] && echo 1)" "$(grep -E '^(fx|sel|selname) ' "$ST" | tr '\n' ' ')"
# (Tab runs along the row and Enter comes back to the column the run started in, as the page's own keys do)
DISPLAY=$DSP xdotool key Down; sleep 0.3
DISPLAY=$DSP xdotool type --delay 20 'a'; sleep 0.3; DISPLAY=$DSP xdotool key Tab; sleep 0.4
DISPLAY=$DSP xdotool type --delay 20 'b'; sleep 0.3; DISPLAY=$DSP xdotool key Return; sleep 0.8
grep -q '^cell A4 a$' "$ST" && grep -q '^cell B4 b$' "$ST" && grep -q '^sel 4 0 4 0 4 0$' "$ST"
ok "4. Tab goes a cell right, Enter puts it in and comes back to the row's first column" "$([ $? = 0 ] && echo 1)" "$(grep -E '^(sel|cell A4|cell B4) ' "$ST" | tr '\n' ' ')"
cell '=1/0'; cell '=SUM(A1:A3)'
grep -q '^show A5 #DIV/0!$' "$ST" && grep -q '^show A6 24$' "$ST"
ok "5. a division by nothing is #DIV/0!, and SUM(A1:A3) is 24" "$([ $? = 0 ] && echo 1)" "$(grep -E '^show ' "$ST" | tr '\n' ' ')"
DISPLAY=$DSP xdotool key ctrl+Home; sleep 0.4
DISPLAY=$DSP xdotool key ctrl+Down; sleep 0.8
grep -q '^sel 5 0 5 0 5 0$' "$ST"
ok "6. Ctrl+Down goes to the end of the filled run (A1 to A6)" "$([ $? = 0 ] && echo 1)" "$(grep '^sel ' "$ST")"
DISPLAY=$DSP xdotool key ctrl+a; sleep 0.8
grep -q '^selname A1:B6$' "$ST" && grep -q '^agg 48|12|5|24|7$' "$ST"
ok "7. Ctrl+A chooses what has something in it, and the status line adds it up (Sum 48, Average 12, Min 5, Max 24, Count 7)" "$([ $? = 0 ] && echo 1)" "$(grep -E '^(selname|agg) ' "$ST" | tr '\n' ' ')"
# (undo and redo walk the changes: the last cell typed goes, and comes back)
DISPLAY=$DSP xdotool key ctrl+z; sleep 1
A=$(grep -c '^cell A6 ' "$ST")
DISPLAY=$DSP xdotool key ctrl+y; sleep 1
B=$(grep -c '^cell A6 ' "$ST")
[ "$A" = 0 ] && [ "$B" = 1 ]
ok "8. Ctrl+Z takes the last cell away and Ctrl+Y brings it back" "$([ $? = 0 ] && echo 1)" "after undo $A, after redo $B ($(grep '^hist' "$ST"))"
DISPLAY=$DSP xdotool key ctrl+Home; sleep 0.4
DISPLAY=$DSP xdotool key ctrl+b; sleep 0.9
grep -q '^fmt A1 b1 i0 u0 s0 ha0 fc- bg-$' "$ST"
ok "9. Ctrl+B makes the cell bold, and the toolbar says so" "$([ $? = 0 ] && echo 1)" "$(grep '^fmt ' "$ST" | tr '\n' ' ')"
DISPLAY=$DSP xdotool key ctrl+b; sleep 0.9
! grep -q '^fmt A1 ' "$ST"
ok "10. ...and a second Ctrl+B takes it off, as the page's own does" "$([ $? = 0 ] && echo 1)" "$(grep '^fmt ' "$ST" | tr '\n' ' ')"
DISPLAY=$DSP xdotool key ctrl+grave; sleep 0.9
A=$(grep -c '^show A3 =A1+A2$' "$ST")
DISPLAY=$DSP xdotool key ctrl+grave; sleep 0.9
B=$(grep -c '^show A3 12$' "$ST")
[ "$A" = 1 ] && [ "$B" = 1 ]
ok "11. Ctrl+\` shows the formulas in the grid, and takes them back to their values" "$([ $? = 0 ] && echo 1)" "formulas $A, values $B ($(grep -E '^(hist|show A3) ' "$ST" | tr '\n' ' '))"
# (the name box: Ctrl+G puts the cursor in it, a cell's name chooses that cell and a range's its cells)
DISPLAY=$DSP xdotool key ctrl+g; sleep 0.5; DISPLAY=$DSP xdotool key ctrl+a; DISPLAY=$DSP xdotool type --delay 20 'C3'; DISPLAY=$DSP xdotool key Return; sleep 0.9
A=$(grep -c '^sel 2 2 2 2 2 2$' "$ST")
DISPLAY=$DSP xdotool key ctrl+g; sleep 0.5; DISPLAY=$DSP xdotool key ctrl+a; DISPLAY=$DSP xdotool type --delay 20 'A1:B2'; DISPLAY=$DSP xdotool key Return; sleep 0.9
B=$(grep -c '^selname A1:B2$' "$ST")
[ "$A" = 1 ] && [ "$B" = 1 ]
ok "12. the name box chooses a cell (C3) and a range (A1:B2)" "$([ $? = 0 ] && echo 1)" "cell $A, range $B ($(grep -E '^(sel|selname) ' "$ST" | tr '\n' ' '))"
kill -0 $SPP 2>/dev/null || echo "NOTE the app had exited: $(cat "$T/app.exit" 2>/dev/null)"
# (the menus are the page's own: a second copy opens one as a press on the bar opens it, and its state says how many rows
#  it has -- File's 15 and Data's 12, separators and all)
for M in File Data; do
	ST2=$T/state_$M.txt; rm -f "$ST2"
	RUDOLF_SP_STATE=$ST2 RUDOLF_SP_MENU=$M DISPLAY=$DSP "$BIN" >"$T/sp_$M.log" 2>&1 & SPP2=$!
	i=0; while [ $i -lt 40 ] && ! grep -q '^menu ' "$ST2" 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
	eval "N_$M=$(sed -n 's/^menu [A-Za-z]* //p' "$ST2" 2>/dev/null | head -1)"
	kill $SPP2 2>/dev/null; sleep 0.5
done
[ "$N_File" = 15 ] && [ "$N_Data" = 12 ]
ok "13. File opens with the page's 15 rows and Data with its 12" "$([ $? = 0 ] && echo 1)" "File $N_File, Data $N_Data; $(grep '^menu' "$T/state_File.txt" 2>/dev/null) $(grep '^menu' "$T/state_Data.txt" 2>/dev/null)"
# the build-time reading of the page: a page without the lists stops the build instead of quietly shipping a blank toolbar
mkdir -p "$T/page/iso/sys/apps" "$T/page/desktop"
sed 's/const OFF_FONTS=/const OFF_FONTS_GONE=/' "$R/desktop/office45.js" > "$T/page/desktop/office45.js"
cp "$R/desktop/spro45.js" "$T/page/desktop/spro45.js"
cp "$R/iso/sys/apps/mksheetpro.py" "$T/page/iso/sys/apps/mksheetpro.py"
python3 "$T/page/iso/sys/apps/mksheetpro.py" "$T/gone.h" >"$T/mk.log" 2>&1; RC=$?
if [ $RC = 1 ] && grep -q 'OFF_FONTS' "$T/mk.log"; then
	ok "14. mksheetpro.py refuses a page it cannot read (no quiet default)" 1
else
	ok "14. mksheetpro.py refuses a page it cannot read (no quiet default)" 0 "rc=$RC $(cat "$T/mk.log")"
fi
cp "$ST" "$OUT/state.txt" 2>/dev/null; cp "$T/sp.log" "$OUT/app.log" 2>/dev/null
for f in "$T/state_File.txt" "$T/state_Data.txt"; do [ -f "$f" ] && cp "$f" "$OUT/$(basename "$f")"; done
echo "$NP passed, $NF failed"; [ $NF = 0 ]
