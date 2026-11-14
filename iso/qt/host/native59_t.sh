#!/bin/sh
# 1.5.9 (plan Part 12, steps 3 and 4): rudolf-native, the native desktop engine, on a virtual screen (Xvfb) with a
# stand-in rudolfd (fixtures/native_rd.py) serving the real desktop page: the sign-in screen (a wrong and a right
# password), the heartbeat, the wallpaper (colours, a gradient, a drawn background, a picture of your own), the taskbar
# (at the bottom and the top, its accent colour, the pinned apps, the tray, the clock), the start menu and its search,
# app windows in one-app mode sharing one WebKit web process, the lock screen and signing out; (step 4) the window
# manager: frames, focus, moving, resizing, maximise, minimise, the taskbar's list, full screen, dialogs, close. What is where comes from
# rudolf-native's own account of it (RUDOLF_NATIVE_STATE); colours from screenshots. Step 7 (the Vivobook's bugs): the
# pointer, the page's scale and the logo on a light bar, the picture of your own before its copy is made, a dropdown
# in an app window, the development link's banner and question from the shell. Needs Xvfb, xdotool, ImageMagick,
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
trap 'kill $NP $RDP $XV $XP $IP 2>/dev/null; pkill -f "$T/" 2>/dev/null; rm -rf $T' EXIT
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
# 1.5.9 step 7: the pointer over the desktop is the usual arrow (left_ptr: its tip at the top left), not X's bare ✕
cat > $T/cur.c <<'EOF2'
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include <stdio.h>
int main(void) { Display *d = XOpenDisplay(NULL); if (!d) return 2; XFixesCursorImage *c = XFixesGetCursorImage(d); if (!c) return 3; printf("%d %d %d %d\n", c->width, c->height, c->xhot, c->yhot); return 0; }
EOF2
gcc -o $T/cur $T/cur.c -lX11 -lXfixes 2>/dev/null; xdotool mousemove 640 300; timeout 0.3 tail -f /dev/null
c=$($T/cur); ok "47. the pointer over the desktop is the arrow, not X's bare ✕ (step 7)" "$(echo "$c" | awk '{exit !($3<4 && $4<4)}' && echo 1)" "$c"
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
m=$(convert $T/s.png -crop 22x22+11+766 -colorspace Gray -format '%[fx:int(255*mean)]' info:)
ok "48. ...its logo dark on the silver button, as the page's (not faded: step 7)" "$([ "$m" -lt 160 ] && echo 1)" "$m"
set_s '{"s":{"bs":"solid","b1":"#008080","tb":"bottom","accent":"#2563eb","scale":125,"tbbstyle":"candy","fs":13},"pins":["notepad","calc","settings"],"notes":0,"wallpaper":false}'
w8 '[ "$(st bar)" = "1 0 742 1280 58" ]' 40; timeout 1 tail -f /dev/null; shot
ok "49. the page's scale (125 %): the taskbar and the title bars grow as the page's do (step 7)" "$([ "$(st bar)" = "1 0 742 1280 58" ] && [ "$(st scale)" = "1.25 58 38" ] && grep -q '^hit start - 4 750 174 43' $D/state && echo 1)" "$(st bar) / $(st scale) / $(grep '^hit start' $D/state)"
p=$(hit net -); p=$(px $p); ok "50. ...the buttons in their own style (Candy: pink), the bar in its own (Standard: dark)" "$(echo "$p" | awk '{exit !($1>$2+60)}' && close "$(px 700 795)" "17 24 39" 3 && echo 1)" "$p / $(px 700 795)"
# a picture of your own whose copy rudolfd has not made yet (the first start): a drawn background, not the colours;
# then the copy appears (no change to the settings) and is drawn at once, without a restart
rm -f $D/wall.jpg
set_s '{"s":{"bs":"solid","b1":"#008080","tb":"bottom","wpimg":"w1","wpfit":"fill","accent":"#2563eb"},"pins":["notepad","calc","settings"],"notes":0,"wallpaper":"file"}'
w8 '[ "$(st bar)" = "1 0 754 1280 46" ]' 40; timeout 2 tail -f /dev/null; shot
a=$(px 640 400); ok "51. no copy of the picture yet: the first drawn background meanwhile, not teal (step 7)" "$( ! close "$a" "0 128 128" 12 && ! close "$a" "$(px 100 100)" 6 && echo 1)" "$a / $(px 100 100)"
convert -size 64x64 xc:'#cc2244' $D/wall.jpg; timeout 5 tail -f /dev/null; shot
ok "52. ...the copy appears: the picture, without a restart" "$(close "$(px 640 400)" "204 34 68" 8 && echo 1)" "$(px 640 400)"
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
# ---- the window manager (step 4)
XC=$T/xc; gcc -O2 -o $XC $HERE/fixtures/xwmclient.c -lX11 || { echo "FAIL build xwmclient"; exit 1; }
cl() { grep "^client $1 " $D/state | head -1; }          # client ID FRAME X Y W H min= max= full= focus= app= TITLE
cv() { cl $1 | awk -v f=$2 '{print $f}'; }               # one field: 4 x, 5 y, 6 w, 7 h
flag() { cl $1 | grep -o "$2=[0-9]" | cut -d= -f2; }
ok "28. rudolf-native is the window manager; the app windows are framed" "$([ "$(st wm)" = 1 ] && [ "$(grep -c '^client .* app=calc' $D/state)" = 1 ] && [ "$(grep -c '^client .* app=notepad' $D/state)" = 1 ] && echo 1)" "$(grep '^client\|^wm' $D/state)"
$XC app > $T/xc.log & XP=$!
w8 'grep -q "^id" $T/xc.log'; ID=$(sed -n 's/^id 0x//p' $T/xc.log | head -1)
w8 '[ -n "$(cl $ID)" ]' 40
ok "29. a Linux app's window gets a frame, the keyboard and WM_STATE" "$([ "$(flag $ID focus)" = 1 ] && [ "$($XC getfocus)" = "0x$ID" ] && [ "$($XC wmstate 0x$ID)" = 1 ] && echo 1)" "$(cl $ID) $($XC getfocus)"
w8 'grep -q "^take_focus [1-9]" $T/xc.log' 20; ok "30. ...WM_TAKE_FOCUS with a server time (as Wine needs)" "$(grep -q '^take_focus [1-9]' $T/xc.log && echo 1)" "$(cat $T/xc.log)"
ok "31. ...its title on the taskbar" "$(grep -q "^hit win $ID " $D/state && grep -q "^client $ID .* Starport stand-in$" $D/state && echo 1)" "$(grep '^hit win' $D/state)"
timeout 0.5 tail -f /dev/null; shot; X=$(cv $ID 4); Y=$(cv $ID 5); W=$(cv $ID 6)
ok "32. ...the active title bar in the accent colour" "$(close "$(px $((X+60)) $((Y+15)))" "37 99 235" 3 && echo 1)" "$(px $((X+60)) $((Y+15)))"
xdotool mousemove $((X+60)) $((Y+15)) mousedown 1; for k in 1 2 3 4 5; do xdotool mousemove $((X+60+k*40)) $((Y+15+k*20)); timeout 0.1 tail -f /dev/null; done; xdotool mouseup 1; timeout 0.5 tail -f /dev/null
ok "33. dragging the title bar moves it" "$([ "$(cv $ID 4)" = $((X+200)) ] && [ "$(cv $ID 5)" = $((Y+100)) ] && echo 1)" "$(cl $ID) from $X $Y"
ok "34. ...and the window is told where it is (a ConfigureNotify with the screen's coordinates)" "$(grep -q "^geom $((X+200+4)) $((Y+100+30)) 400 300" $T/xc.log && echo 1)" "$(grep geom $T/xc.log | tail -2)"
X=$(cv $ID 4); Y=$(cv $ID 5); FW=$((W+8)); FH=$(( $(cv $ID 7) + 34 ))
xdotool mousemove $((X+FW-2)) $((Y+FH-2)) mousedown 1; for k in 1 2 3 4; do xdotool mousemove $((X+FW-2+k*20)) $((Y+FH-2+k*15)); timeout 0.1 tail -f /dev/null; done; xdotool mouseup 1; timeout 0.5 tail -f /dev/null
ok "35. dragging the bottom-right corner resizes it" "$([ "$(cv $ID 6)" = 480 ] && [ "$(cv $ID 7)" = 360 ] && echo 1)" "$(cl $ID)"
W=$(cv $ID 6); FW=$((W+8)); click $((X+FW-61)) $((Y+15)); timeout 0.5 tail -f /dev/null
ok "36. maximise: the whole screen above the taskbar" "$([ "$(flag $ID max)" = 1 ] && [ "$(cv $ID 4) $(cv $ID 5) $(cv $ID 6) $(cv $ID 7)" = "0 0 1272 720" ] && echo 1)" "$(cl $ID)"
xdotool mousemove 300 15 click --repeat 2 --delay 80 1; timeout 0.5 tail -f /dev/null
ok "37. a double-click on the title bar: back to its size and place" "$([ "$(flag $ID max)" = 0 ] && [ "$(cv $ID 4) $(cv $ID 6)" = "$X 480" ] && echo 1)" "$(cl $ID)"
click $((X+FW-97)) $((Y+15)); timeout 0.5 tail -f /dev/null
ok "38. minimise: out of sight (WM_STATE iconic), the next window gets the keyboard" "$([ "$(flag $ID min)" = 1 ] && [ "$($XC wmstate 0x$ID)" = 3 ] && [ "$(flag $ID focus)" = 0 ] && grep -q '^client .* focus=1 ' $D/state && echo 1)" "$(grep '^client' $D/state) $($XC wmstate 0x$ID)"
click $(hit win $ID); timeout 0.5 tail -f /dev/null
ok "39. its taskbar button brings it back, in front" "$([ "$(flag $ID min)" = 0 ] && [ "$(flag $ID focus)" = 1 ] && [ "$($XC getfocus)" = "0x$ID" ] && echo 1)" "$(cl $ID)"
CID=$(grep '^client .* app=calc' $D/state | cut -d' ' -f2); click $(( $(cv $CID 4) + 820 )) $(( $(cv $CID 5) + 520 )); timeout 0.5 tail -f /dev/null
ok "40. a click into another window gives it the keyboard (and the click)" "$([ "$(flag $CID focus)" = 1 ] && [ "$(flag $ID focus)" = 0 ] && echo 1)" "$(grep '^client' $D/state)"
$XC full 0x$ID 1; timeout 0.5 tail -f /dev/null
ok "41. a window asking for full screen covers the whole screen, taskbar too" "$([ "$(flag $ID full)" = 1 ] && grep -q '^geom 0 0 1280 800' $T/xc.log && [ "$($XC stack | tail -1)" = "0x$(cl $ID | cut -d' ' -f3)" ] && echo 1)" "$(cl $ID); $(tail -2 $T/xc.log); $($XC stack | tail -2)"
$XC full 0x$ID 0; timeout 0.5 tail -f /dev/null
ok "42. ...and back" "$([ "$(flag $ID full)" = 0 ] && [ "$(cv $ID 6)" = 480 ] && echo 1)" "$(cl $ID)"
$XC inno > $T/inno.log & IP=$!; w8 'grep -q "^dlg" $T/inno.log'; DLG=$(sed -n 's/^dlg 0x//p' $T/inno.log); OW=$(sed -n 's/^id 0x//p' $T/inno.log); w8 '[ -n "$(cl $DLG)" ]' 20
ok "43. a dialog is centred over its window" "$(dx=$(( $(cv $DLG 4) + ($(cv $DLG 6) + 8) / 2 - $(cv $OW 4) - ($(cv $OW 6) + 8) / 2 )); [ ${dx#-} -le 2 ] && echo 1)" "$(cl $OW) / $(cl $DLG)"
ok "44. ...and is not on the taskbar" "$( ! grep -q "^hit win $DLG " $D/state && echo 1)"
kill $IP 2>/dev/null; w8 '[ -z "$(cl $DLG)" ]' 20
X=$(cv $ID 4); Y=$(cv $ID 5); FW=$(( $(cv $ID 6) + 8 )); click $((X+FW-25)) $((Y+15))
w8 'grep -q "^delete" $T/xc.log' 20; w8 '[ -z "$(cl $ID)" ]' 20
ok "45. close: WM_DELETE_WINDOW, the program closes its window, the frame and the taskbar button go" "$(grep -q '^delete' $T/xc.log && [ -z "$(cl $ID)" ] && ! grep -q "^hit win $ID " $D/state && echo 1)" "$(cat $T/xc.log | tail -2)"
NID=$(grep '^client .* app=notepad' $D/state | cut -d' ' -f2); X=$(cv $NID 4); Y=$(cv $NID 5); FW=$(( $(cv $NID 6) + 8 )); click $((X+FW-25)) $((Y+15))
w8 '[ -z "$(cl $NID)" ]' 40
ok "46. an app window's close button asks the page (as its ✕), which closes it" "$([ -z "$(cl $NID)" ] && ! grep -q '^app notepad' $D/state && echo 1)" "$(grep '^client\|^app' $D/state)"
# ---- 1.5.9 step 7: a dropdown in an app window (Settings › Display Properties' Appearance): drawn by the page in its
# colours (WebKit's own look was a blank box), and its list opens (WebKit's pop-up, under the native window manager)
click $(hit pin settings); w8 'grep -q "^app settings .* Settings$" $D/state' 120; SID=$(grep '^client .* app=settings' $D/state | cut -d' ' -f2)
timeout 12 tail -f /dev/null; X=$(cv $SID 4); Y=$(cv $SID 5); shot
p=$(px $((X+430)) $((Y+515))); ok "53. a dropdown in an app window: the page's dark colours, not a blank box" "$(close "$p" "11 18 32" 10 && echo 1)" "$p at $((X+430)) $((Y+515))"
pop() { xwininfo -root -children 2>/dev/null | grep -c '^ *0x[0-9a-f]* "RudolfNative": ("RudolfNative" "RudolfNative")  [0-9]*x[0-9]*+'; }
xwininfo -root -tree > $T/before; click $((X+480)) $((Y+515)); timeout 1.5 tail -f /dev/null; xwininfo -root -tree > $T/after; shot
nw=$(diff $T/before $T/after | grep '^> *0x' | grep -o '[0-9]*x[0-9]*+[0-9-]*+[0-9-]*' | head -1)
sd=$(echo "$nw" | grep -q x && convert $T/s.png -crop "$nw" -colorspace Gray -format '%[fx:int(1000*standard_deviation)]' info: 2>/dev/null)
ok "54. ...its list opens, drawn (WebKit's pop-up under the window manager: its options' text on it)" "$(echo "$nw" | awk -Fx '{split($2,a,"+"); exit !($1>60 && a[1]>30)}' && [ "${sd:-0}" -gt 60 ] && echo 1)" "$nw sd=$sd"
xdotool key Escape; timeout 0.5 tail -f /dev/null
# ---- 1.5.9 step 7: the development link's banner, once, from the shell (not in every app window), and its question
printf '%s' '{"ok":true,"available":true,"on":true,"repo":"owner/rudolf-devlink","secondsLeft":5400,"status":{"state":"on"},"pending":{"id":"c42","kind":"run","what":"uname -a"}}' > $D/devlink.json
w8 '[ -n "$(st devlink)" ]' 30; timeout 3 tail -f /dev/null; shot
ok "55. the development link on: a red banner along the top, with the time left" "$(st devlink | grep -q '^0 1280 28 ⚠ Development link on: commands from GitHub (owner/rudolf-devlink) .* 1 h 30 min left$' && close "$(px 300 6)" "176 0 32" 6 && echo 1)" "$(st devlink) $(px 300 6)"
ok "56. ...the windows keep below it, and an app window's page draws no banner of its own" "$([ "$(cv $SID 5)" -ge 28 ] && ! close "$(px $(( $(cv $SID 4) + 300 )) $(( $(cv $SID 5) + 40 )))" "176 0 32" 30 && echo 1)" "$(cl $SID) $(px $(( $(cv $SID 4) + 300 )) $(( $(cv $SID 5) + 40 )))"
q=$(st devask); ok "57. ...a command waiting: the question, once, from the shell" "$(echo "$q" | grep -q '^c42 no ' && echo 1)" "$q"
click $(echo "$q" | awk '{print $3, $4}'); w8 'grep -q "POST devlink/answer?id=c42&a=refuse" $D/calls' 20
printf '%s' '{"ok":true,"available":true,"on":true,"repo":"owner/rudolf-devlink","secondsLeft":5400,"status":{"state":"on"}}' > $D/devlink.json; timeout 5 tail -f /dev/null
ok "58. ...Refuse answers rudolfd, and the question goes" "$(grep -q 'POST devlink/answer?id=c42&a=refuse' $D/calls && [ -z "$(st devask)" ] && echo 1)" "$(st devask) $(grep devlink $D/calls | tail -2)"
click $(st devend); w8 'grep -q "POST devlink/off" $D/calls' 20; rm -f $D/devlink.json; w8 '[ -z "$(st devlink)" ]' 30
ok "59. End now switches it off; the banner goes and the windows may use the top again" "$(grep -q 'POST devlink/off' $D/calls && [ -z "$(st devlink)" ] && echo 1)" "$(st devlink)"
X=$(cv $SID 4); Y=$(cv $SID 5); FW=$(( $(cv $SID 6) + 8 )); click $((X+FW-25)) $((Y+15)); w8 '[ -z "$(cl $SID)" ]' 40
# ---- lock and sign out
click $(hit start -); w8 '[ "$(st menu)" = 1 ]' 20; L=$(grep '^menubtn lock ' $D/state | awk '{print $3+int($5/2), $4+int($6/2)}'); click $L
w8 '[ "$(st locked)" = 1 ]' 20; ok "24. Lock: the lock screen, rudolfd told (new app windows get no session meanwhile)" "$([ "$(st locked)" = 1 ] && [ "$(st signin)" = 1 ] && grep -q 'POST desktop/lock' $D/calls && [ "$($XC stack | tail -1)" = "0x$(xdotool search --name '^Sign in$' | head -1 | xargs printf '%x')" ] && echo 1)" "$(head -6 $D/state)"
xdotool type --delay 15 'nope'; xdotool key Return; w8 '[ -n "$(st signmsg)" ]' 20
ok "25. ...a wrong password: refused" "$([ "$(st locked)" = 1 ] && [ "$(st signmsg)" = 'The password is incorrect.' ] && echo 1)" "$(st signmsg)"
LC_ALL=C.UTF-8 xdotool type --delay 30 "$PW"; xdotool key Return; w8 '[ "$(st locked)" = 0 ]' 40
ok "26. ...the right one: back, the windows as they were" "$([ "$(st locked)" = 0 ] && [ "$(grep -c '^app ' $D/state)" = 1 ] && [ "$(flag $CID min)" = 0 ] && echo 1)" "$(grep '^app\|^client\|^locked' $D/state)"
click $(hit start -); w8 '[ "$(st menu)" = 1 ]' 20; L=$(grep '^menubtn logout ' $D/state | awk '{print $3+int($5/2), $4+int($6/2)}'); click $L
w8 '[ "$(st signin)" = 1 ]' 20; ok "27. Sign out: the app windows close, rudolfd ends the session, the sign-in screen" "$([ "$(st user)" = - ] && [ "$(grep -c '^app ' $D/state)" = 0 ] && grep -q 'POST desktop/logout' $D/calls && echo 1)" "$(head -6 $D/state)"
kill $NP 2>/dev/null
echo "$((N-F))/$N passed"; [ $F = 0 ]
