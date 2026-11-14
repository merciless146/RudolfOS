#!/bin/sh
# 1.5.9 (plan Part 12, steps 3 and 4): rudolf-native, the native desktop engine, on a virtual screen (Xvfb) with a
# stand-in rudolfd (fixtures/native_rd.py) serving the real desktop page: the sign-in screen (a wrong and a right
# password), the heartbeat, the wallpaper (colours, a gradient, a drawn background, a picture of your own), the taskbar
# (at the bottom and the top, its accent colour, the pinned apps, the tray, the clock), the start menu and its search,
# app windows in one-app mode sharing one WebKit web process, the lock screen and signing out. What is where comes from
# rudolf-native's own account of it (RUDOLF_NATIVE_STATE); colours from screenshots. Needs Xvfb, xdotool, ImageMagick,
# node, python3, gcc and WebKitGTK's headers (libwebkit2gtk-4.1-dev).   sh native59_t.sh [RUDOLF-NATIVE]
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/native59.XXXXXX); PORT=8298; DSP=:79
command -v Xvfb >/dev/null && command -v xdotool >/dev/null && command -v convert >/dev/null || { echo "SKIP: needs Xvfb, xdotool and ImageMagick"; exit 0; }
BIN=$1
if [ -z "$BIN" ]; then BIN=$T/rudolf-native
	gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $BIN $HERE/../../sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
	grep -q warning $T/cc && { cat $T/cc; echo "FAIL build warnings"; exit 1; }
fi
D=$T/d; mkdir -p $D; node $HERE/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)")
set_s() { printf '%s\n' "$1" > $D/settings.json.t; mv $D/settings.json.t $D/settings.json; }
set_s '{"s":{"bs":"solid","b1":"#112233","b2":"#000080","tb":"bottom","accent":"#2563eb","h24":true,"df":"ymd"},"pins":["notepad","calc","settings"],"notes":2,"wallpaper":false}'
python3 $HERE/fixtures/native_rd.py $PORT $D > $T/rd.log 2>&1 & RDP=$!
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=$DSP LIBGL_ALWAYS_SOFTWARE=1
trap 'kill $NP $RDP $XV 2>/dev/null; pkill -f "$T/" 2>/dev/null; rm -rf $T' EXIT
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
w8 'xdpyinfo >/dev/null 2>&1 || xdotool getmouselocation >/dev/null 2>&1' 40
(cd $D && HOME=$D RUDOLF_NATIVE_STATE=$D/state exec $BIN --size=1280x800 --draw=soft --base=http://127.0.0.1:$PORT > $T/native.log 2>&1) & NP=$!
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 700)"; F=$((F+1)); fi; }
st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
shot() { import -window root $T/s.png 2>/dev/null; }
px() { convert $T/s.png -format "%[fx:int(255*p{$1,$2}.r)] %[fx:int(255*p{$1,$2}.g)] %[fx:int(255*p{$1,$2}.b)]" info:; }
close() { a=$1; b=$2; t=${3:-8}; echo "$a|$b" | awk -F'|' -v t=$t '{split($1,x," ");split($2,y," ");for(i=1;i<=3;i++){d=x[i]-y[i];if(d<0)d=-d;if(d>t)exit 1}}'; }
click() { xdotool mousemove $1 $2 click 1; }
hit() { grep "^hit $1 $2 " $D/state | head -1 | awk '{print $4+int($6/2), $5+int($7/2)}'; }
# ---- the sign-in screen
w8 '[ "$(st signin)" = 1 ]' 120; ok "1. the sign-in screen first, without the taskbar" "$([ "$(st signin)" = 1 ] && [ "$(st bar | cut -d' ' -f1)" = 0 ] && echo 1)" "$(cat $D/state 2>/dev/null; tail $T/native.log)"
shot; p=$(px 5 5); ok "2. ...over the wallpaper, dimmed" "$(close "$p" "11 22 33" 4 && echo 1)" "$p"
w8 'grep -q "POST heartbeat" $D/calls'; ok "3. the heartbeat goes to rudolfd once the sign-in screen is up (the session's trial)" "$(grep -q 'POST heartbeat' $D/calls && echo 1)"
xdotool type --delay 15 'wrong'; xdotool key Return
w8 '[ -n "$(st signmsg)" ]'; ok "4. a wrong password: rudolfd's message, still the sign-in screen" "$([ "$(st signmsg)" = 'The user name or password is incorrect.' ] && [ "$(st signin)" = 1 ] && echo 1)" "$(st signmsg)"
LC_ALL=C.UTF-8 xdotool type --delay 30 "$PW"; xdotool key Return
w8 '[ "$(st user)" = Ann ] && [ "$(st signin)" = 0 ]'; ok "5. the right password (not ASCII): signed in, the taskbar shown" "$([ "$(st user)" = Ann ] && [ "$(st bar)" = "1 0 754 1280 46" ] && echo 1)" "$(head -8 $D/state)"
# ---- the wallpaper and the taskbar
w8 'grep -q "^hit pin calc" $D/state'; timeout 1 tail -f /dev/null; shot
ok "6. the wallpaper: the colour (solid)" "$(close "$(px 640 400)" "17 34 51" 2 && echo 1)" "$(px 640 400)"
ok "7. the taskbar: the standard dark bar" "$(close "$(px 800 757)" "17 24 39" 2 && echo 1)" "$(px 800 757)"
ok "8. ...the start button, then the pinned apps in order" "$(grep '^hit ' $D/state | head -4 | awk '{printf "%s:%s ", $2, $3}' | grep -q '^start:- pin:notepad pin:calc pin:settings' && echo 1)" "$(grep '^hit' $D/state)"
ok "9. ...the tray (network, sound, battery, notifications) and the clock" "$(for k in net vol bat notes clock; do grep -q "^hit $k " $D/state || echo no; done | grep -q no || echo 1)" "$(grep '^hit' $D/state)"
ok "10. ...the clock in 24 hours (h24) and the date as year/month/day (df)" "$(st clock | grep -qE '^[0-2][0-9]:[0-5][0-9]\|20[0-9][0-9]/[01][0-9]/[0-3][0-9]$' && echo 1)" "$(st clock)"
set_s '{"s":{"bs":"gradient","b1":"#ff0000","b2":"#0000ff","tb":"top","accent":"#16a34a","tbac":true,"tbacm":"negative","h24":false,"df":"dmy"},"pins":["calc"],"notes":0,"wallpaper":false}'
w8 '[ "$(st bar | cut -d" " -f3)" = 0 ]' 40; timeout 1 tail -f /dev/null; shot
ok "11. settings changed in the page: the taskbar moves to the top" "$([ "$(st bar)" = "1 0 0 1280 46" ] && echo 1)" "$(st bar)"
ok "12. ...in the accent colour's negative (tbac, tbacm), as the page draws it" "$(close "$(px 800 3)" "233 92 181" 3 && echo 1)" "$(px 800 3)"
ok "13. ...the wallpaper a gradient from the first colour to the second" "$(close "$(px 3 60)" "250 0 5" 12 && close "$(px 1276 796)" "0 0 255" 12 && echo 1)" "$(px 3 60) $(px 1276 796)"
ok "14. ...the clock in 12 hours, the date day/month/year; the pins as now" "$(st clock | grep -qE '^1?[0-9]:[0-5][0-9] (AM|PM)\|[0-3][0-9]/[01][0-9]/20[0-9][0-9]$' && [ "$(grep -c '^hit pin' $D/state)" = 1 ] && echo 1)" "$(st clock) $(grep -c '^hit pin' $D/state)"
set_s '{"s":{"bs":"solid","b1":"#000000","tb":"bottom","dyn":"alpine","dm":"dark","accent":"#2563eb","tbstyle":"retro"},"pins":["notepad","calc","settings"],"notes":0,"wallpaper":false}'
w8 '[ "$(st bar | cut -d" " -f3)" = 754 ]' 40; timeout 2 tail -f /dev/null; shot
a=$(px 300 300); b=$(px 900 500)
ok "15. a drawn background: its dark picture (WebP, through WebKit's libwebp)" "$( ! close "$a" "$b" 10 && ! close "$a" "0 0 0" 10 && echo 1)" "$a / $b"
ok "16. ...and the Retro taskbar: silver" "$(close "$(px 800 760)" "192 192 192" 2 && echo 1)" "$(px 800 760)"
convert -size 64x64 xc:'#cc2244' $D/wall.jpg
set_s '{"s":{"bs":"solid","b1":"#000000","tb":"bottom","wpimg":"w1","wpfit":"fill","accent":"#2563eb"},"pins":["notepad","calc","settings"],"notes":0,"wallpaper":true}'
timeout 4 tail -f /dev/null; shot
ok "17. a picture of your own (from the page, through rudolfd), filling the screen" "$(close "$(px 640 400)" "204 34 68" 8 && close "$(px 4 4)" "204 34 68" 8 && echo 1)" "$(px 640 400) $(px 4 4)"
# ---- the start menu and app windows
set_s '{"s":{"bs":"solid","b1":"#112233","tb":"bottom","accent":"#2563eb","h24":true},"pins":["notepad","calc","settings"],"notes":2,"wallpaper":false}'
timeout 4 tail -f /dev/null
click $(hit start -); w8 '[ "$(st menu)" = 1 ]' 20; ok "18. the start button opens the start menu" "$([ "$(st menu)" = 1 ] && echo 1)"
xdotool type --delay 20 'calcul'; xdotool key Return
w8 'grep -q "^app calc .* Calculator$" $D/state' 80
ok "19. ...searching and Enter: the app in a window of its own, signed in, its title from the page (one-app mode)" "$(grep -q '^app calc [0-9a-f]* 1 Calculator$' $D/state && [ "$(st menu)" = 0 ] && echo 1)" "$(grep '^app' $D/state; tail -3 $T/native.log)"
click $(hit pin notepad); w8 'grep -q "^app notepad .* Notepad$" $D/state' 80
ok "20. a pinned app opens its window" "$(grep -q '^app notepad [0-9a-f]* 1' $D/state && echo 1)" "$(grep '^app' $D/state)"
n=$(ps -eo comm | grep -c '^WebKitWebProces'); ok "21. ...both windows share one WebKit web process" "$([ "$n" = 1 ] && echo 1)" "$n"
timeout 2 tail -f /dev/null; shot; ok "22. ...and they draw (the Calculator's dark keys, Notepad's white page)" "$( ! close "$(px 300 300)" "246 245 244" 3 && echo 1)" "$(px 300 300)"
click $(hit pin calc); timeout 1 tail -f /dev/null
ok "23. a pinned app already open is brought to the front, not opened again" "$([ "$(grep -c '^app ' $D/state)" = 2 ] && grep -q '^app calc [0-9a-f]* 1' $D/state && echo 1)" "$(grep '^app' $D/state)"
# ---- lock and sign out
click $(hit start -); w8 '[ "$(st menu)" = 1 ]' 20; L=$(grep '^menubtn lock ' $D/state | awk '{print $3+int($5/2), $4+int($6/2)}'); click $L
w8 '[ "$(st locked)" = 1 ]' 20; ok "24. Lock: the lock screen, rudolfd told (new app windows get no session meanwhile)" "$([ "$(st locked)" = 1 ] && [ "$(st signin)" = 1 ] && grep -q 'POST desktop/lock' $D/calls && echo 1)" "$(head -6 $D/state)"
xdotool type --delay 15 'nope'; xdotool key Return; w8 '[ -n "$(st signmsg)" ]' 20
ok "25. ...a wrong password: refused" "$([ "$(st locked)" = 1 ] && [ "$(st signmsg)" = 'The password is incorrect.' ] && echo 1)" "$(st signmsg)"
LC_ALL=C.UTF-8 xdotool type --delay 30 "$PW"; xdotool key Return; w8 '[ "$(st locked)" = 0 ]' 40
ok "26. ...the right one: back, the windows as they were" "$([ "$(st locked)" = 0 ] && [ "$(grep -c '^app ' $D/state)" = 2 ] && echo 1)" "$(head -6 $D/state)"
click $(hit start -); w8 '[ "$(st menu)" = 1 ]' 20; L=$(grep '^menubtn logout ' $D/state | awk '{print $3+int($5/2), $4+int($6/2)}'); click $L
w8 '[ "$(st signin)" = 1 ]' 20; ok "27. Sign out: the app windows close, rudolfd ends the session, the sign-in screen" "$([ "$(st user)" = - ] && [ "$(grep -c '^app ' $D/state)" = 0 ] && grep -q 'POST desktop/logout' $D/calls && echo 1)" "$(head -6 $D/state)"
kill $NP 2>/dev/null
echo "$((N-F))/$N passed"; [ $F = 0 ]
