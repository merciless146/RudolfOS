#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 27): Blitzen's code runs, links and actions sandboxed, on Standard (the desktop page in Chromium) and on
# Native's app window (rudolf-native under Xvfb: WebKitGTK), the owner's rule: "Rudolf OS on the native desktop should appear and
# function exactly as it does in Chromium mode". The stand-in rudolfd (fixtures/native_rd.py) serves the real page; the model and
# Blitzen's look-ups are stood in by fixtures/blzbox161_inject.js, which runs the scenario (__blzT): JavaScript, Lua and an HTML page
# Blitzen "wrote" try every way out (the page and rudolfd's token, the network, the desktop's storage, a worker of their own, the
# desktop's windows and dialogs, the person's files), ordinary code still runs, Run and check still fixes code, the person's files
# change only after Keep, links: the pages read open, others ask. Here: no request of any exploit (/api/blzleak-...) reaches the
# stand-in, and no extra request for the page (which carries the token).
# Needs node with Playwright; Native also Xvfb, xdotool, gcc and WebKitGTK's headers (skipped without them).
#   sh blzbox161_t.sh [RUDOLF-NATIVE]
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/blzbox161.XXXXXX); PORT=${BLZB_PORT:-8641}; DSP=${BLZB_DSP:-:96}
NP=; XV=; RDP=
trap 'kill $NP $RDP $XV 2>/dev/null; pkill -f "$T/" 2>/dev/null; rm -rf $T' EXIT
trap 'exit 1' INT TERM PIPE HUP
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
tally() { while IFS= read -r l; do echo "$l"; case "$l" in PASS*) N=$((N+1));; FAIL*) N=$((N+1)); F=$((F+1));; esac; done; }
D=$T/d; mkdir -p $D; node $HERE/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)")
printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","tb":"bottom","accent":"#2563eb","h24":true},"pins":["notepad","explorer"],"notes":0,"wallpaper":false}' > $D/settings.json
cp $HERE/fixtures/blzbox161_inject.js $D/inject.js
mkdir -p $T/q/qt/host; cp -r $HERE/fixtures $T/q/qt/host/; cp -r $HERE/../../www $T/q/www; mkdir -p $T/q/sys; cp $HERE/../../sys/rudolfd.c $T/q/sys/
python3 $T/q/qt/host/fixtures/native_rd.py $PORT $D > $T/rd.log 2>&1 & RDP=$!
w8 "curl -s -o /dev/null http://127.0.0.1:$PORT/" 40 || { echo "FAIL the stand-in rudolfd did not start"; exit 1; }
leaks() { grep -c 'blzleak' $D/calls 2>/dev/null; }; pages() { grep -c '^PAGE /$' $D/calls 2>/dev/null; }
# ---- Standard
P0=$(pages)
node $HERE/fixtures/blzbox161_std.js http://127.0.0.1:$PORT "$PW" > $T/std.out 2>&1; tally < $T/std.out
ok "Standard: no request of any exploit reached rudolfd" "$([ "$(leaks)" = 0 ] && echo 1)" "$(grep blzleak $D/calls | head -5)"
ok "Standard: the page (which carries the token) asked for once, by the desktop alone" "$([ "$(pages)" = $((P0+1)) ] && echo 1)" "$P0 -> $(pages)"
# ---- Native's app window
if ! command -v Xvfb >/dev/null || ! command -v xdotool >/dev/null || ! pkg-config --exists webkit2gtk-4.1 2>/dev/null && [ -z "$1" ]; then
	echo "SKIP Native: needs Xvfb, xdotool and WebKitGTK's headers"; echo "$((N-F))/$N passed"; [ $F = 0 ]; exit; fi
BIN=$1
if [ -z "$BIN" ]; then BIN=$T/rudolf-native
	gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $BIN $HERE/../../sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
fi
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=$DSP LIBGL_ALWAYS_SOFTWARE=1
mkfifo $T/ev
w8 'xdotool getmouselocation >/dev/null 2>&1' 40
(cd $D && HOME=$D RUDOLF_NATIVE_STATE=$D/state RUDOLF_NATIVE_TEST_EVAL=$T/ev exec $BIN --size=1280x800 --draw=soft --base=http://127.0.0.1:$PORT > $T/native.log 2>&1) & NP=$!
exec 7<>$T/ev
st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
ev() { : > $T/ev.out; printf '%s %s\n' "$1" "$2" >&7; w8 "grep -q '^$1 ' $T/ev.out 2>/dev/null" ${3:-40}; grep "^$1 " $T/ev.out | tail -1 | cut -d' ' -f2-; : > $T/ev.out; }
w8 '[ "$(st signin)" = 1 ]' 160; LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return
w8 '[ "$(st user)" = Ann ]' 120; w8 '[ "$(st svc | cut -d" " -f1)" = ready ]' 240; timeout 3 tail -f /dev/null
L0=$(leaks); P0=$(pages)
ev do-bg "oaPost({op:'open',app:'blitzen'});1" >/dev/null
w8 "grep -q '^client .* app=blitzen ' \$D/state" 120 && w8 "[ \"\$(ev blitzen 'String(!!(window.__blzT&&OA.main&&OA.main.el&&OA.main.blz))')\" = true ]" 120
ok "Native: Blitzen in an app window of its own (the stand-in model)" "$([ "$(ev blitzen 'String(!!(window.__blzT&&OA.main&&OA.main.blz))')" = true ] && echo 1)" "$(tail -5 $T/native.log)"
P1=$(pages)
ev blitzen "__blzT.start()" >/dev/null
w8 "[ \"\$(ev blitzen 'String(__blzT.res.done)')\" = true ]" 960
ev blitzen "__blzT.res.r.map(x=>(x[1]?'PASS ':'FAIL ')+'Native: '+x[0]+(x[1]?'':' - '+x[2])).join('\\u0001')" 80 | tr '\001' '\n' > $T/nat.out
[ "$(ev blitzen 'String(__blzT.res.done)')" = true ] || echo "FAIL Native: the scenario did not finish" >> $T/nat.out
tally < $T/nat.out
ok "Native: no request of any exploit reached rudolfd" "$([ "$(leaks)" = "$L0" ] && echo 1)" "$(grep blzleak $D/calls | head -5)"
ok "Native: the page (which carries the token) asked for by the desktop's windows alone (none during the scenario)" "$([ "$(pages)" = "$P1" ] && echo 1)" "$P0 $P1 -> $(pages)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
