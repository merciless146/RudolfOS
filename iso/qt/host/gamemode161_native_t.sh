#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 19): game mode on the native desktop. rudolf-native on a virtual screen (Xvfb) with the
# stand-in rudolfd (fixtures/native_rd.py) and a stand-in game (fixtures/fsgame.c: an X window that asks to be full screen,
# with its _NET_WM_PID): before, nothing is said; full screen, the native desktop says so every 2 s (game/on?src=native,
# kind=linux, its pid); leaving full screen, game/off; full screen again, then the game crashes (SIGKILL): game/off; Game mode off (rudolfd says enabled=false):
# no game/off after a game it did not count. Needs Xvfb, xdotool, gcc and WebKitGTK's headers.   sh gamemode161_native_t.sh [RUDOLF-NATIVE]
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/gmn161.XXXXXX); PORT=${GMN_PORT:-8299}; DSP=${GMN_DSP:-:81}
command -v Xvfb >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs Xvfb and xdotool"; exit 0; }
BIN=$1
if [ -z "$BIN" ]; then BIN=$T/rudolf-native
	gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $BIN $HERE/../../sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
	grep -q warning $T/cc && { cat $T/cc; echo "FAIL build warnings"; exit 1; }
fi
gcc -O2 -o $T/fsgame $HERE/fixtures/fsgame.c -lX11 || { echo "FAIL build fsgame"; exit 1; }
D=$T/d; mkdir -p $D; node $HERE/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)")
printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","b2":"#000080","tb":"bottom","accent":"#2563eb"},"pins":["notepad"],"notes":0,"wallpaper":false}' > $D/settings.json
python3 $HERE/fixtures/native_rd.py $PORT $D > $T/rd.log 2>&1 & RDP=$!
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=$DSP LIBGL_ALWAYS_SOFTWARE=1
trap 'kill $NP $RDP $XV $GP 2>/dev/null; pkill -f "$T/" 2>/dev/null; rm -rf $T' EXIT
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
w8 'xdotool getmouselocation >/dev/null 2>&1' 40
(cd $D && HOME=$D RUDOLF_NATIVE_STATE=$D/state exec $BIN --size=1280x800 --draw=soft --base=http://127.0.0.1:$PORT > $T/native.log 2>&1) & NP=$!
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 700)"; F=$((F+1)); fi; }
st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
calls() { n=$(grep -c "$1" $D/calls 2>/dev/null); echo ${n:-0}; }   # (grep -c prints 0 itself when nothing matches)
w8 '[ "$(st signin)" = 1 ]' 120 || { echo "FAIL the sign-in screen did not come: $(tail $T/native.log)"; exit 1; }
LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return
w8 '[ "$(st user)" = Ann ]' 120; ok "0. signed in" "$([ "$(st user)" = Ann ] && echo 1)" "$(head -5 $D/state)"
timeout 3 tail -f /dev/null
ok "1. before: no game, no full screen said to rudolfd (the pages only ask: GET game/status)" "$([ "$(calls 'POST game/')" = 0 ] && echo 1)" "$(grep game/ $D/calls | head)"
$T/fsgame & GP=$!
w8 "[ \$(calls 'POST game/on?src=native&kind=linux&pid=$GP') -ge 2 ]" 40
ok "2. the stand-in game full screen: game/on?src=native&kind=linux with its pid, again every 2 s" "$([ "$(calls "POST game/on?src=native&kind=linux&pid=$GP")" -ge 2 ] && echo 1)" "$(grep game/ $D/calls | tail -3)"
ok "...the window manager has it full screen" "$(grep "^client " $D/state | grep -q 'full=1' && echo 1)" "$(grep '^client' $D/state)"
kill -USR1 $GP; w8 "[ \$(calls 'POST game/off?src=native') -ge 1 ]" 20
ok "3. it leaves full screen: game/off?src=native" "$([ "$(calls 'POST game/off?src=native')" = 1 ] && echo 1)" "$(grep game/ $D/calls | tail -3)"
n0=$(calls 'POST game/on'); timeout 3 tail -f /dev/null
ok "...and nothing more is said while it is windowed" "$([ "$(calls 'POST game/on')" = "$n0" ] && echo 1)" "$(grep game/ $D/calls | tail -3)"
kill -USR2 $GP; w8 "[ \$(calls 'POST game/on') -gt $n0 ]" 20; ok "4. full screen again: game/on again" "$([ "$(calls 'POST game/on')" -gt "$n0" ] && echo 1)"
kill -9 $GP; wait $GP 2>/dev/null; w8 "[ \$(calls 'POST game/off?src=native') -ge 2 ]" 20
ok "5. the game crashes (SIGKILL): game/off said (root also stops counting it: its pid is gone)" "$([ "$(calls 'POST game/off?src=native')" = 2 ] && echo 1)" "$(grep game/ $D/calls | tail -3)"
touch $D/gamemode-off; $T/fsgame & GP=$!; w8 "[ \$(calls 'pid=$GP') -ge 2 ]" 40; o3=$(calls 'POST game/off'); kill -9 $GP; wait $GP 2>/dev/null; timeout 4 tail -f /dev/null
ok "6. Game mode off (rudolfd: enabled=false): the full screen is said (root decides), but nothing is held, and no game/off for it" "$([ "$(calls 'POST game/off')" = "$o3" ] && echo 1)" "$(grep game/ $D/calls | tail -4)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
