#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 23 A, the owner's Vivobook of 2026-11-20: six WebKit web processes of 250-380 MB, ~1 GB): the native
# desktop's hidden pages are one (#app=do-bg: what the service page do-svc, the portal's page do-portal and the start-up page do-start
# each did), and the shell's single actions go to it (oneapp59.js __oaDo) instead of a page of the whole desktop loaded for each.
# rudolf-native under Xvfb with the stand-in rudolfd (fixtures/native_rd.py) serving this tree's page:
#   1. after signing in: one hidden page only (do-bg), signed in as the service page and the portal's page (svc ready, portal ready),
#      opened once; no do-svc, do-portal or do-start page; the web processes counted (this rudolf-native's own)
#   2. each single action done by that page, no page opened for it: a quick settings switch (Dark mode: the page's setting), Do not
#      disturb (the notifications' centre), the sound muted from the taskbar, an app unpinned from the taskbar, the desktop's
#      New text document and its Rename, Delete (to the Trash), the start menu's search; and after them still one page
#   3. the hidden page's process killed: loaded again, ready, still the one page; the actions work again
#   4. signing out closes it; signing in opens it again (once)
# Needs Xvfb, xdotool, node, python3, gcc and WebKitGTK's headers.   sh bgpage161_t.sh [RUDOLF-NATIVE]
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/bgpage161.XXXXXX); PORT=${BGPAGE_PORT:-8622}; DSP=${BGPAGE_DSP:-:96}
command -v Xvfb >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs Xvfb and xdotool"; exit 0; }
BIN=$1
if [ -z "$BIN" ]; then BIN=$T/rudolf-native
	gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $BIN $HERE/../../sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
	grep -q warning $T/cc && { cat $T/cc; echo "FAIL build warnings"; exit 1; }
fi
D=$T/d; mkdir -p $D; node $HERE/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)")
printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","tb":"bottom","accent":"#2563eb","h24":true},"pins":["notepad","calc","settings"],"notes":2,"wallpaper":false}' > $D/settings.json
python3 $HERE/fixtures/native_rd.py $PORT $D > $T/rd.log 2>&1 & RDP=$!
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=$DSP LIBGL_ALWAYS_SOFTWARE=1
mkfifo $T/ev
trap 'kill $NP $RDP $XV 2>/dev/null; pkill -f "$T/" 2>/dev/null; [ -n "$KEEP" ] && cp $T/native.log $KEEP; rm -rf $T' EXIT
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
w8 'xdotool getmouselocation >/dev/null 2>&1' 40
(cd $D && HOME=$D RUDOLF_NATIVE_STATE=$D/state RUDOLF_NATIVE_TEST_EVAL=$T/ev exec $BIN --size=1280x800 --draw=soft --base=http://127.0.0.1:$PORT > $T/native.log 2>&1) & NP=$!
exec 7<>$T/ev
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
click() { xdotool mousemove $1 $2 click 1; }
hit() { grep "^hit $1 $2 " $D/state | head -1 | awk '{print $4+int($6/2), $5+int($7/2)}'; }
cmact() { grep "^cmitem [0-9]* [0-9]* 0 $1|" $D/state | head -1 | awk '{print $2, $3}'; }
flyb() { grep "^flybtn $1 " $D/state | head -1 | awk '{print $3, $4}'; }
# (a page's answer: KEY JS, read back from the FIFO's .out; the hidden page by its name)
ev() { : > $T/ev.out; printf '%s %s\n' "$1" "$2" >&7; w8 "grep -q '^$1 ' $T/ev.out 2>/dev/null" 40; grep "^$1 " $T/ev.out | tail -1 | cut -d' ' -f2-; : > $T/ev.out; }
pages() { grep '^page ' $D/state | cut -d' ' -f2 | sort | tr '\n' ' '; }
wp() { ps -eo pid=,ppid=,comm= | awk -v r=$NP '{p[$1]=$2; c[$1]=$3} END {a[r]=1; do {m=0; for (k in p) if (!(k in a) && (p[k] in a)) {a[k]=1; m=1}} while (m); n=0; for (k in a) if (c[k] ~ /^WebKitWebProces/) n++; print n}'; }
opened() { grep -c "the shell's hidden page is opened" $T/native.log; }
others() { grep -c "is opened\|a page of its own" $T/native.log | head -1; }
dolog() { ev do-bg "JSON.stringify(OA.doLog||[])"; }
# ---- 1. signed in: one hidden page
w8 '[ "$(st signin)" = 1 ]' 160; LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return
w8 '[ "$(st user)" = Ann ]' 120; w8 '[ "$(st svc | cut -d" " -f1)" = ready ] && [ "$(st portal | cut -d" " -f1)" = ready ]' 240; timeout 6 tail -f /dev/null
ok "1. signed in: one hidden page (do-bg), ready as the service page and the portal's, opened once" "$([ "$(pages)" = "do-bg " ] && [ "$(st svc)" = "ready 1" ] && [ "$(st portal | cut -d' ' -f1)" = ready ] && [ "$(opened)" = 1 ] && echo 1)" "pages: $(pages); $(st svc); $(st portal); $(grep 'is opened' $T/native.log)"
ok "1b. ...no do-svc, do-portal or do-start page, no app window" "$(! grep -qE '^(page|app) do-(svc|portal|start) ' $D/state && ! grep -q '^client .* app=' $D/state && echo 1)" "$(grep -E '^(page|app|client) ' $D/state)"
W0=$(wp); echo "NOTE web processes after start-up: $W0"
ok "1c. ...one web process of this rudolf-native's (WebKitGTK shares it between the pages here)" "$([ "$W0" -le 1 ] && echo 1)" "$W0"
ok "1d. ...it is the start-up page too (the notice after an update's part ran: &start=1)" "$(ev do-bg "ONEAPP.q.get('start')+'|'+(typeof whatsNew=='function'&&/help/.test(String(whatsNew)))" | grep -q '^1|true$' && echo 1)" "$(ev do-bg "location.hash")"
# ---- 2. the single actions
shut() { click 300 300; w8 '[ "$(st fly)" = - ]' 20; }
NL0=$(ev do-bg "String(!!S.nl)")
xdotool key super+a; w8 '[ "$(st fly | cut -d" " -f1)" = qs ] && [ -n "$(flyb qs:1)" ]' 20; B=$(flyb qs:1)
click $B; w8 '[ "$(ev do-bg "String(!!S.nl)")" != "$NL0" ]' 40
ok "2a. a quick settings switch (Night light): done by the hidden page (its setting changed), no page opened for it" "$([ "$(ev do-bg "String(!!S.nl)")" != "$NL0" ] && dolog | grep -q '"qs"' && [ "$(pages)" = "do-bg " ] && echo 1)" "nl $NL0 -> $(ev do-bg "String(!!S.nl)"); $(dolog); $(pages); $(st fly)"
click $B; w8 '[ "$(ev do-bg "String(!!S.nl)")" = "$NL0" ]' 40; shut
xdotool key super+n; w8 '[ "$(st fly | cut -d" " -f1)" = nc ] && [ -n "$(flyb dnd)" ]' 20
DB=$(flyb dnd); [ -n "$DB" ] && click $DB; w8 '[ "$(ev do-bg "String(!!S.dnd)")" = true ]' 40
ok "2b. Do not disturb in the notifications' centre: done by the hidden page" "$([ "$(ev do-bg "String(!!S.dnd)")" = true ] && dolog | grep -q '"nc"' && [ "$(ev do-bg "String(!!(document.getElementById('ncp')))")" = false ] && [ "$(pages)" = "do-bg " ] && echo 1)" "$(st fly); $(ev do-bg "String(S.dnd)"); $(dolog); $(pages)"
[ -n "$DB" ] && click $DB; w8 '[ "$(ev do-bg "String(!!S.dnd)")" = false ]' 40; shut
M0=$(ev do-bg "String(S.snd.master.on)")
xdotool mousemove $(hit vol -) click 3; w8 '[ -n "$(cmact mute)" ]' 20; click $(cmact mute); w8 '[ "$(ev do-bg "String(S.snd.master.on)")" != "$M0" ]' 40
ok "2c. the sound muted from the taskbar's menu: done by the hidden page" "$([ "$(ev do-bg "String(S.snd.master.on)")" != "$M0" ] && dolog | grep -q '"mute"' && [ "$(pages)" = "do-bg " ] && echo 1)" "$M0 -> $(ev do-bg "String(S.snd.master.on)"); $(grep '^cmitem' $D/state | head -6 | tr '\n' ' '); $(dolog)"
xdotool mousemove $(hit vol -) click 3; w8 '[ -n "$(cmact mute)" ]' 20; click $(cmact mute); timeout 1 tail -f /dev/null
xdotool mousemove $(hit pin calc) click 3; w8 '[ -n "$(cmact pin0:calc)" ]' 20; click $(cmact pin0:calc); CK="String(pinList().includes('calc'))"; w8 '[ "$(ev do-bg "$CK")" = false ]' 40
# (the stand-in rudolfd gives the shell its pins from DIR/settings.json, so the taskbar here keeps them: the page's own list is checked)
ok "2d. an app unpinned from the taskbar: done by the hidden page (the page's pins without it)" "$([ "$(ev do-bg "String(pinList().includes('calc'))")" = false ] && dolog | grep -q '"pin"' && [ "$(pages)" = "do-bg " ] && echo 1)" "$(ev do-bg "JSON.stringify(pinList())"); $(dolog)"
cmi() { grep "^cmitem .*|$1$" $D/state | head -1 | awk '{print $2, $3}'; }
di() { grep "^dicon .*|$1$" $D/state | head -1; }
dc() { di "$1" | awk '{print $4+42, $5+24}'; }
xdotool mousemove 900 500 click 3; w8 '[ -n "$(cmi "New text document")" ]' 20; click $(cmi "New text document"); w8 '[ "$(st dialog | cut -d" " -f1)" = rename ]' 80
xdotool type --delay 30 'bg.txt'; xdotool key Return; w8 '[ -n "$(di BG.TXT)" ]' 80
ok "2e. the desktop's New text document and its Rename: done by the hidden page" "$([ -n "$(di BG.TXT)" ] && [ "$(pages)" = "do-bg " ] && echo 1)" "$(grep '^dicon\|^dialog' $D/state | tr '\n' ' '); $(pages)"
for t in 1 2 3; do timeout 0.4 tail -f /dev/null; click $(dc BG.TXT); w8 'di BG.TXT | grep -q " 1 |file|"' 8 && break; done; xdotool key Delete; w8 '[ -z "$(di BG.TXT)" ]' 80
ok "2f. Delete: the file to the Trash, by the hidden page (the desktop's menu's Move to Trash)" "$([ -z "$(di BG.TXT)" ] && dolog | grep -q '"desk"' && [ "$(pages)" = "do-bg " ] && echo 1)" "$(di BG.TXT); $(dolog); $(pages)"
click $(hit start -); w8 '[ "$(st menu)" = 1 ]' 20; xdotool type --delay 30 'notep'; w8 'grep -q "^menuq notep" $D/state' 20; timeout 2 tail -f /dev/null
ok "2g. the start menu's search: the hidden page's (no search page opened)" "$([ "$(pages)" = "do-bg " ] && ! grep -q 'do-srch' $D/state && echo 1)" "$(pages); $(grep '^menuq\|^srow' $D/state | head -4 | tr '\n' ' ')"
xdotool key Escape; w8 '[ "$(st menu)" = 0 ]' 20
ok "2h. after them: still one hidden page, opened once, no page of its own for any action, the web processes as before" "$([ "$(pages)" = "do-bg " ] && [ "$(opened)" = 1 ] && ! grep -q 'a page of its own' $T/native.log && [ "$(wp)" -le "$W0" ] && echo 1)" "$(pages); $(opened); $(wp); $(grep 'did not take' $T/native.log)"
# ---- 3. its process killed: loaded again
WP=$(pgrep -P $NP -f WebKitWebProcess | head -1); [ -z "$WP" ] && WP=$(for p in $(pgrep -f WebKitWebProcess); do [ "$(ps -o ppid= -p $p | tr -d ' ')" = $NP ] && echo $p; done | head -1)
if [ -n "$WP" ]; then kill $WP; timeout 3 tail -f /dev/null; w8 '[ "$(st svc | cut -d" " -f1)" = ready ] && [ "$(st portal | cut -d" " -f1)" = ready ]' 240
	ok "3. the hidden page's process killed: loaded again, ready, still the one page" "$([ "$(st svc | cut -d' ' -f1)" = ready ] && [ "$(st portal | cut -d' ' -f1)" = ready ] && [ "$(pages)" = "do-bg " ] && echo 1)" "$(st svc) $(st portal) $(pages)"
else ok "3. the hidden page's process killed: loaded again, ready, still the one page" 0 "no WebKitWebProcess under $NP"; fi
xdotool mousemove $(hit vol -) click 3; w8 '[ -n "$(cmact mute)" ]' 20; M1=$(ev do-bg "String(S.snd.master.on)"); click $(cmact mute); w8 '[ "$(ev do-bg "String(S.snd.master.on)")" != "$M1" ]' 40
ok "3b. ...and takes the actions again" "$([ "$(ev do-bg "String(S.snd.master.on)")" != "$M1" ] && [ "$(pages)" = "do-bg " ] && echo 1)" "$M1 $(ev do-bg "String(S.snd.master.on)") $(pages)"
# ---- 4. signing out closes it, signing in opens it again
click $(hit start -); w8 '[ "$(st menu)" = 1 ]' 20; M=$(grep '^mrow ' $D/state | head -1 | awk '{print $3+60, $4+40}'); i=0; while [ $i -lt 30 ] && ! grep -q "^mrow logoff " $D/state; do xdotool mousemove $M click 5; timeout 0.15 tail -f /dev/null; i=$((i+1)); done
L=$(grep '^mrow logoff ' $D/state | awk '{print $3+int($5/2), $4+int($6/2)}'); click $L
w8 '[ "$(st signin)" = 1 ] && [ "$(st user)" = - ]' 60; timeout 4 tail -f /dev/null
ok "4. signing out closes the hidden page" "$([ -z "$(pages)" ] && [ "$(st svc | cut -d' ' -f1)" = - ] && echo 1)" "$(pages) $(st svc)"
LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return; w8 '[ "$(st user)" = Ann ]' 120; w8 '[ "$(st svc | cut -d" " -f1)" = ready ] && [ "$(st portal | cut -d" " -f1)" = ready ]' 240
ok "4b. signing in again: the one hidden page again" "$([ "$(pages)" = "do-bg " ] && echo 1)" "$(pages) $(st svc)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
