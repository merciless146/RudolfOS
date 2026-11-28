#!/bin/sh
# 1.6.3 (docs/plan-1.6.3.md Part 1): rudolf-sheetpro's first step -- the page's window, its toolbar, its eight menus, its
# formula bar and its grid, with values and formulas, undo and redo, and the page's keys -- built as the release builds it
# (iso/sys/ui/build.sh) and then used: typing puts values in cells and Enter goes down, a formula is worked out and shown,
# Tab and Enter run along a row and come back to its first column, Ctrl+` shows the formulas, Ctrl+B marks a cell, undo and
# redo walk the changes, the name box chooses a cell or a range, the status line adds up a choice, and Ctrl+Down goes to the
# end of a filled run. The menus are the page's own (File's 15 rows and Data's 12) and the toolbar's lists are the page's
# (mksheetpro.py). A .RSHEET the page wrote opens with its cells, its formula and its styles, and the file's name in the
# title; a change marks the book unsaved (the page's * in the title), and Ctrl+S writes it back as the page's own JSON. A
# book of two sheets is not opened at all (this step shows one, and opening it would drop the other on the next save);
# a CSV opens as the page reads one (quotes, a doubled quote, CRLF, the first line's own delimiter) and Ctrl+S writes it
# back the way the page writes a CSV, and a CSV written without a chooser (RUDOLF_SP_EXPORT) is the page's own too.
# It reads RUDOLF_SP_STATE, so no screenshot is needed here; the page-by-page comparison against the web SpreadsheetPro
# comes with the step that makes every one of its views.
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
# ---- 14. a .RSHEET the page wrote opens: its cells, its formula worked out, its style, and its name in the title
# (the file is sprof45.js's own v2 JSON: a number, a formula, a bold cell and a centred one)
kill $SPP 2>/dev/null; sleep 0.8
F=$T/grid.RSHEET
cat > "$F" <<'RS'
{"v":2,"app":"SpreadsheetPro","active":0,"names":{},"styles":[{"b":1},{"ha":"center"}],"sheets":[{"name":"Sheet1","color":"","cells":[[0,5],[1,null,"=A1*2"],[702,"x",null,0],[1404,"say \"hi\", \\o/"],[1405,0.333333333333333],[1406,7,null,1]],"cw":{},"rh":{},"hid":{},"hc":{},"freeze":{"r":0,"c":0},"merges":[],"cf":[],"dv":[],"cm":{},"charts":[],"filter":null,"grid":true,"print":{}}]}
RS
ST3=$T/state_open; rm -f "$ST3"
RUDOLF_SP_STATE=$ST3 DISPLAY=$DSP "$BIN" "$F" >"$T/sp_open.log" 2>&1 & SPP3=$!
i=0; while [ $i -lt 60 ] && ! grep -q '^dirty ' "$ST3" 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
RC=0
grep -q '^title grid.RSHEET - SpreadsheetPro$' "$ST3" || RC=1
grep -q '^file grid.RSHEET$' "$ST3" || RC=1
grep -q '^dirty 0$' "$ST3" || RC=1
grep -q '^cell A1 5$' "$ST3" || RC=1
grep -q '^cell B1 =A1\*2$' "$ST3" || RC=1
grep -q '^show B1 10$' "$ST3" || RC=1
grep -q '^cell A2 x$' "$ST3" || RC=1
grep -q '^fmt A2 b1 i0 u0 s0 ha0 ' "$ST3" || RC=1
grep -q '^show C3 7$' "$ST3" || RC=1
grep -q '^fmt C3 b0 i0 u0 s0 ha2 ' "$ST3" || RC=1
ok "14. a .RSHEET the page wrote opens with its cells, its formula worked out and its styles, and its name in the title" \
	"$([ $RC = 0 ] && echo 1)" "$(grep -E '^(title|file|dirty|cell|show|fmt) ' "$ST3" | tr '\n' ' '); $(cat "$T/sp_open.log")"
# ---- 15. a change marks the book unsaved (the page's * in the title) and Ctrl+S writes it back as the page's own JSON
# (the window of the book just opened, by its own title: the first copy's window may still be there, its shell killed
#  rather than the program it started, and a key sent to that one would leave this book untouched)
WIN3=$(DISPLAY=$DSP xdotool search --name '^grid\.RSHEET - SpreadsheetPro$' 2>/dev/null | head -1)
[ -n "$WIN3" ] || WIN3=$(DISPLAY=$DSP xdotool search --name 'SpreadsheetPro$' 2>/dev/null | tail -1)
i=0; while [ $i -lt 60 ]; do DISPLAY=$DSP xdotool windowfocus --sync "$WIN3" 2>/dev/null
	[ "$(DISPLAY=$DSP xdotool getwindowfocus 2>/dev/null)" = "$WIN3" ] && break; timeout 0.2 tail -f /dev/null; i=$((i+1)); done
DISPLAY=$DSP xdotool key ctrl+Home; sleep 0.5
DISPLAY=$DSP xdotool type --delay 20 '9'; sleep 0.4; DISPLAY=$DSP xdotool key Return; sleep 1.2
RC=0
# (the window's own title says it at once, the page's * for changes; the state file follows a moment later)
TITLE3=$(DISPLAY=$DSP xdotool getwindowname "$WIN3" 2>/dev/null)
[ "$TITLE3" = "grid.RSHEET * - SpreadsheetPro" ] || RC=1
grep -q '^dirty 1$' "$ST3" || RC=1
DISPLAY=$DSP xdotool key ctrl+s; sleep 1.6
TITLE3=$(DISPLAY=$DSP xdotool getwindowname "$WIN3" 2>/dev/null)
[ "$TITLE3" = "grid.RSHEET - SpreadsheetPro" ] || RC=1
grep -q '^dirty 0$' "$ST3" || RC=1
grep -q '^say Saved grid.RSHEET$' "$ST3" || RC=1
python3 - "$F" <<'PY' || RC=1
import json, sys
d = json.load(open(sys.argv[1]))
assert d['v'] == 2 and d['app'] == 'SpreadsheetPro', d
assert [s['name'] for s in d['sheets']] == ['Sheet1'], d['sheets']
cells = {a[0]: a for a in d['sheets'][0]['cells']}
assert sorted(cells) == [0, 1, 702, 1404, 1405, 1406], sorted(cells)
assert cells[0] == [0, 9], cells[0]
assert cells[1] == [1, None, '=A1*2'], cells[1]
assert cells[702] == [702, 'x', None, 0], cells[702]
assert cells[1404] == [1404, 'say "hi", \\o/'], cells[1404]     # (the escapes survive both ways)
assert cells[1405] == [1405, 0.333333333333333], cells[1405]      # (15 significant digits, as the page keeps them)
assert cells[1406] == [1406, 7, None, 1], cells[1406]
assert d['styles'] == [{'b': 1}, {'ha': 'center'}], d['styles']
assert d['sheets'][0]['name'] == 'Sheet1', d['sheets'][0]['name']
PY
ok "15. a change puts the page's * in the title, and Ctrl+S saves the page's own JSON (cells, formulas and styles)" \
	"$([ $RC = 0 ] && echo 1)" "title '$TITLE3'; $(sed -n '1,12p' "$ST3" | tr '\n' ' '); $(tail -3 "$T/sp_open.log" 2>/dev/null); $(head -c 300 "$F" 2>/dev/null)"
cp "$ST3" "$OUT/state_open.txt" 2>/dev/null
kill $SPP3 2>/dev/null; sleep 0.4
# ---- 17. a book of two sheets is refused, not opened and cut down to the first
# (the step shows one sheet. opening such a book and saving it would drop every sheet but the first without a word, so the
#  file is left exactly as it is and the status line says why)
F2=$T/two.RSHEET
cat > "$F2" <<'RS'
{"v":2,"app":"SpreadsheetPro","active":0,"names":{},"styles":[],"sheets":[{"name":"One","cells":[[0,5]],"cw":{},"rh":{},"hid":{},"hc":{},"freeze":{"r":0,"c":0},"merges":[],"cf":[],"dv":[],"cm":{},"charts":[],"filter":null,"grid":true,"print":{}},{"name":"Two","cells":[[0,6]],"cw":{},"rh":{},"hid":{},"hc":{},"freeze":{"r":0,"c":0},"merges":[],"cf":[],"dv":[],"cm":{},"charts":[],"filter":null,"grid":true,"print":{}}]}
RS
SUM2=$(md5sum "$F2" | cut -d' ' -f1); ST5=$T/state_two; rm -f "$ST5"
RUDOLF_SP_STATE=$ST5 DISPLAY=$DSP "$BIN" "$F2" >"$T/sp_two.log" 2>&1 & SPP5=$!
i=0; while [ $i -lt 60 ] && ! grep -q '^dirty ' "$ST5" 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
RC=0
grep -q '^title Book1 - SpreadsheetPro$' "$ST5" || RC=1
grep -q '^file -$' "$ST5" || RC=1
grep -q '^cells 0$' "$ST5" || RC=1
grep -q '^say two.RSHEET has 2 sheets' "$ST5" || RC=1
[ "$(md5sum "$F2" | cut -d' ' -f1)" = "$SUM2" ] || RC=1
ok "17. a book of two sheets is refused with the reason, and the file is left as it was" \
	"$([ $RC = 0 ] && echo 1)" "$(grep -E '^(title|file|cells|say) ' "$ST5" 2>/dev/null | tr '\n' ' '); $(cat "$T/sp_two.log"); $F2"
kill $SPP5 2>/dev/null; sleep 0.4
# ---- 18. a CSV opens as the page reads one, and Ctrl+S writes it back the way the page writes it
# (a quote inside a quoted field is doubled; the lines end CRLF; the first row is 5,7,a,b and the second a text with
#  quotes of its own and 0.5, so the file that comes back is the file that went in)
CS=$T/made.csv
printf '5,7,"a,b"\r\n"say ""hi""",0.5,\r\n' > "$CS"
ST6=$T/state_csv; rm -f "$ST6"
RUDOLF_SP_STATE=$ST6 DISPLAY=$DSP "$BIN" "$CS" >"$T/sp_csv.log" 2>&1 & SPP6=$!
i=0; while [ $i -lt 60 ] && ! grep -q '^dirty ' "$ST6" 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
RC=0
grep -q '^title made.csv - SpreadsheetPro$' "$ST6" || RC=1
grep -q '^file made.csv$' "$ST6" || RC=1
grep -q '^cell A1 5$' "$ST6" || RC=1
grep -q '^show A1 5$' "$ST6" || RC=1
grep -q '^cell B1 7$' "$ST6" || RC=1
grep -q '^cell C1 a,b$' "$ST6" || RC=1
grep -q '^cell A2 say "hi"$' "$ST6" || RC=1
grep -q '^show B2 0.5$' "$ST6" || RC=1
WIN6=$(DISPLAY=$DSP xdotool search --name '^made\.csv - SpreadsheetPro$' 2>/dev/null | head -1)
[ -n "$WIN6" ] || RC=1
i=0; while [ $i -lt 60 ] && [ -n "$WIN6" ]; do DISPLAY=$DSP xdotool windowfocus --sync "$WIN6" 2>/dev/null
	[ "$(DISPLAY=$DSP xdotool getwindowfocus 2>/dev/null)" = "$WIN6" ] && break; timeout 0.2 tail -f /dev/null; i=$((i+1)); done
[ -n "$WIN6" ] && { DISPLAY=$DSP xdotool key ctrl+s; sleep 1.6; }
grep -q '^say Saved made.csv$' "$ST6" || RC=1
python3 - "$CS" <<'PY' || RC=1
import sys
b = open(sys.argv[1], 'rb').read()
assert b == b'5,7,"a,b"\r\n"say ""hi""",0.5,\r\n', b
PY
ok "18. a CSV opens as the page reads one (quotes, a doubled quote, CRLF), and Ctrl+S writes the page's own CSV back" \
	"$([ $RC = 0 ] && echo 1)" "$(grep -E '^(title|file|cell|show|say) ' "$ST6" 2>/dev/null | tr '\n' ' '); $(cat "$T/sp_csv.log"); $(head -c 200 "$CS")"
kill $SPP6 2>/dev/null; sleep 0.4
# ---- 19. the sheet is exported as the page writes it (what each cell shows, quoted where it must be, CRLF)
# (RUDOLF_SP_EXPORT names the file, so the row's chooser is not needed. its own book, not case 14's: that one has been
#  typed into and saved by case 15, and a case that reads another's leftovers is a test of the order, not of the export)
F3=$T/ex.RSHEET
cat > "$F3" <<'RS'
{"v":2,"app":"SpreadsheetPro","active":0,"names":{},"styles":[{"b":1},{"ha":"center"}],"sheets":[{"name":"Sheet1","color":"","cells":[[0,5],[1,null,"=A1*2"],[702,"x",null,0],[1406,7,null,1]],"cw":{},"rh":{},"hid":{},"hc":{},"freeze":{"r":0,"c":0},"merges":[],"cf":[],"dv":[],"cm":{},"charts":[],"filter":null,"grid":true,"print":{}}]}
RS
EX=$T/out.csv; rm -f "$EX"; ST7=$T/state_export; rm -f "$ST7"
RUDOLF_SP_STATE=$ST7 RUDOLF_SP_EXPORT=$EX DISPLAY=$DSP "$BIN" "$F3" >"$T/sp_ex.log" 2>&1 & SPP7=$!
i=0; while [ $i -lt 60 ] && ! grep -q '^say Exported ' "$ST7" 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
RC=0
grep -q '^say Exported out.csv$' "$ST7" || RC=1
python3 - "$EX" <<'PY' || RC=1
import sys
b = open(sys.argv[1], 'rb').read()
assert b == b'5,10,\r\nx,,\r\n,,7\r\n', b      # (the formula's 10, not '=A1*2'; the empty cells still take their commas)
PY
ok "19. the sheet is exported as the page writes a CSV: what each cell shows, quoted where it must be, CRLF lines" \
	"$([ $RC = 0 ] && echo 1)" "$(grep -E '^(say|file) ' "$ST7" 2>/dev/null | tr '\n' ' '); $(head -c 200 "$EX" 2>/dev/null); $(cat "$T/sp_ex.log")"
kill $SPP7 2>/dev/null; sleep 0.4
# ---- 20. a modified book is not thrown away: closing asks, and Cancel keeps it with nothing written
# (RUDOLF_SP_DO=close is the check's hand on the window's own X: it waits for the first change, and asks again if the
#  question is put off. the question is the page's own overlay in the window: Escape is Cancel, and Cancel keeps the book)
F4=$T/ask.RSHEET
cat > "$F4" <<'RS'
{"v":2,"app":"SpreadsheetPro","active":0,"names":{},"styles":[],"sheets":[{"name":"Sheet1","color":"","cells":[[0,1],[702,"keep"]],"cw":{},"rh":{},"hid":{},"hc":{},"freeze":{"r":0,"c":0},"merges":[],"cf":[],"dv":[],"cm":{},"charts":[],"filter":null,"grid":true,"print":{}}]}
RS
ST8=$T/state_ask; rm -f "$ST8"
RUDOLF_SP_STATE=$ST8 RUDOLF_SP_DO=close DISPLAY=$DSP "$BIN" "$F4" >"$T/sp_ask.log" 2>&1 & SPP8=$!
i=0; while [ $i -lt 60 ] && ! grep -q '^dirty ' "$ST8" 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
WIN8=$(DISPLAY=$DSP xdotool search --name '^ask\.RSHEET - SpreadsheetPro$' 2>/dev/null | head -1)
i=0; while [ $i -lt 60 ] && [ -n "$WIN8" ]; do DISPLAY=$DSP xdotool windowfocus --sync "$WIN8" 2>/dev/null
	[ "$(DISPLAY=$DSP xdotool getwindowfocus 2>/dev/null)" = "$WIN8" ] && break; timeout 0.2 tail -f /dev/null; i=$((i+1)); done
DISPLAY=$DSP xdotool key ctrl+Home; sleep 0.4
DISPLAY=$DSP xdotool type --delay 20 '4'; sleep 0.4; DISPLAY=$DSP xdotool key Tab; sleep 0.8
RC=0; WHY=""
chk() { if eval "$1" >/dev/null 2>&1; then :; else RC=1; WHY="$WHY $2"; fi; }
SUM4=$(md5sum "$F4" | cut -d' ' -f1)   # (the book as it was while the question waits)
chk 'grep -q "^dirty 1$" "$ST8"' dirty
i=0; while [ $i -lt 40 ] && ! grep -q '^ask 1$' "$ST8" 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
chk 'grep -q "^ask 1$" "$ST8"' question          # (the question is up)
chk '[ -d "/proc/$SPP8" ]' alive                 # (the window is still there)
chk 'grep -q "^cells 2$" "$ST8"' cells           # (with both of its cells)
chk 'grep -q "^cell A2 keep$" "$ST8"' a2
chk 'case "$(sed -n "s/^say //p" "$ST8")" in *"has changes that are not saved"*) true ;; *) false ;; esac' say
chk '[ "$(md5sum "$F4" | cut -d" " -f1)" = "$SUM4" ]' untouched   # (nothing is written while the question waits)
DISPLAY=$DSP xdotool key Escape; sleep 0.9        # (Cancel: the book stays as it is)
chk 'grep -q "^answer 0$" "$ST8"' cancel          # (Cancel: the question was answered, and the book stays)
chk '[ -d "/proc/$SPP8" ]' alive2
chk 'grep -q "^cells 2$" "$ST8"' cells2
chk '[ "$(md5sum "$F4" | cut -d" " -f1)" = "$SUM4" ]' untouched2
ok "20. closing a modified book asks first, and Cancel keeps it with nothing written" \
	"$([ $RC = 0 ] && echo 1)" "failed:${WHY:-none} | $(grep -E '^(title|dirty|ask|answer|cells|say) ' "$ST8" 2>/dev/null | tr '\n' ' '); $(cat "$T/sp_ask.log")"
kill $SPP8 2>/dev/null; sleep 0.4
# ---- 21. ...and Save answers it: the change is written and the window goes
# (RUDOLF_SP_ANSWER is the check's press on the overlay's Save, which it cannot aim; the way through is the one a click takes)
ST12=$T/state_save; rm -f "$ST12"
RUDOLF_SP_STATE=$ST12 RUDOLF_SP_DO=close RUDOLF_SP_ANSWER=save DISPLAY=$DSP "$BIN" "$F4" >"$T/sp_save.log" 2>&1 & SPP12=$!
i=0; while [ $i -lt 60 ] && ! grep -q '^dirty ' "$ST12" 2>/dev/null; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
WIN12=$(DISPLAY=$DSP xdotool search --name '^ask\.RSHEET - SpreadsheetPro$' 2>/dev/null | head -1)
i=0; while [ $i -lt 60 ] && [ -n "$WIN12" ]; do DISPLAY=$DSP xdotool windowfocus --sync "$WIN12" 2>/dev/null
	[ "$(DISPLAY=$DSP xdotool getwindowfocus 2>/dev/null)" = "$WIN12" ] && break; timeout 0.2 tail -f /dev/null; i=$((i+1)); done
DISPLAY=$DSP xdotool key ctrl+Home; sleep 0.4
DISPLAY=$DSP xdotool type --delay 20 '7'; sleep 0.4; DISPLAY=$DSP xdotool key Tab; sleep 0.8
i=0; while [ $i -lt 40 ] && [ -d "/proc/$SPP12" ]; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
RC=0
[ -d "/proc/$SPP12" ] && RC=1                            # (the window is gone)
python3 - "$F4" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
cells = {a[0]: a[1] for a in d['sheets'][0]['cells']}
assert cells.get(0) == 7, cells          # (the change was saved, not thrown away)
assert cells.get(1407) == 'keep', cells
PY
[ $? = 0 ] || RC=1
ok "21. Save answers the question: the change is written and the window closes" \
	"$([ $RC = 0 ] && echo 1)" "$(grep -E '^(dirty|ask|say) ' "$ST12" 2>/dev/null | tr '\n' ' '); $(head -c 200 "$F4" 2>/dev/null)"
kill $SPP12 2>/dev/null; sleep 0.4
# ---- 22. a book with nothing to lose closes at once, with no question
ST9=$T/state_clean; rm -f "$ST9"
RUDOLF_SP_STATE=$ST9 RUDOLF_SP_DO=close DISPLAY=$DSP "$BIN" "$F4" >"$T/sp_clean.log" 2>&1 & SPP9=$!
i=0; while [ $i -lt 60 ] && [ -d "/proc/$SPP9" ]; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
RC=0
[ -d "/proc/$SPP9" ] && RC=1
grep -q '^ask 1$' "$ST9" 2>/dev/null && RC=1
ok "22. a book with nothing to lose closes at once, without a question" \
	"$([ $RC = 0 ] && echo 1)" "$(grep -E '^(dirty|ask|cells) ' "$ST9" 2>/dev/null | tr '\n' ' '); $(cat "$T/sp_clean.log")"
kill $SPP9 2>/dev/null; sleep 0.4
# the build-time reading of the page: a page without the lists stops the build instead of quietly shipping a blank toolbar
mkdir -p "$T/page/iso/sys/apps" "$T/page/desktop"
sed 's/const OFF_FONTS=/const OFF_FONTS_GONE=/' "$R/desktop/office45.js" > "$T/page/desktop/office45.js"
cp "$R/desktop/spro45.js" "$T/page/desktop/spro45.js"
cp "$R/iso/sys/apps/mksheetpro.py" "$T/page/iso/sys/apps/mksheetpro.py"
python3 "$T/page/iso/sys/apps/mksheetpro.py" "$T/gone.h" >"$T/mk.log" 2>&1; RC=$?
if [ $RC = 1 ] && grep -q 'OFF_FONTS' "$T/mk.log"; then
	ok "23. mksheetpro.py refuses a page it cannot read (no quiet default)" 1
else
	ok "23. mksheetpro.py refuses a page it cannot read (no quiet default)" 0 "rc=$RC $(cat "$T/mk.log")"
fi
cp "$ST" "$OUT/state.txt" 2>/dev/null; cp "$T/sp.log" "$OUT/app.log" 2>/dev/null
for f in "$T/state_File.txt" "$T/state_Data.txt"; do [ -f "$f" ] && cp "$f" "$OUT/$(basename "$f")"; done
echo "$NP passed, $NF failed"; [ $NF = 0 ]
