#!/bin/sh
# 1.5.9 (plan Part 12, steps 3 and 4): rudolf-native, the native desktop engine, on a virtual screen (Xvfb) with a
# stand-in rudolfd (fixtures/native_rd.py) serving the real desktop page: the sign-in screen (a wrong and a right
# password), the heartbeat, the wallpaper (colours, a gradient, a drawn background, a picture of your own), the taskbar
# (at the bottom and the top, its accent colour, the pinned apps, the tray, the clock), the start menu and its search,
# app windows in one-app mode sharing one WebKit web process, the lock screen and signing out; (step 4) the window
# manager: frames, focus, moving, resizing, maximise, minimise, the taskbar's list, full screen, dialogs, close; (step 7)
# the arrow on the screen, the apps' pictures, the display scaling, a drop-down in an app window, the development link's
# banner and question; (1.6.0 Part 12, item 1) the desktop's icons. (1.6.0 Part 12, "The rule": the frames' 1 px edge and
# 35 px title bar as the page's, Meta+L locks, Log Off from the page's start menu; same_t.sh compares the looks.) What is where comes from
# rudolf-native's own account of it (RUDOLF_NATIVE_STATE); colours from screenshots. Needs Xvfb, xdotool, ImageMagick,
# node, python3, gcc and WebKitGTK's headers (libwebkit2gtk-4.1-dev).   sh native59_t.sh [RUDOLF-NATIVE]
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/native59.XXXXXX); PORT=${NATIVE59_PORT:-8298}; DSP=${NATIVE59_DSP:-:79}
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
timeout 1 tail -f /dev/null; shot; p=$(px 5 5); ok "2. ...over the wallpaper, dimmed" "$(close "$p" "11 22 33" 4 && echo 1)" "$p"
w8 'grep -q "POST heartbeat" $D/calls'; ok "3. the heartbeat goes to rudolfd once the sign-in screen is up (the session's trial)" "$(grep -q 'POST heartbeat' $D/calls && echo 1)"
xdotool type --delay 15 'wrong'; xdotool key Return
w8 '[ -n "$(st signmsg)" ]'; ok "4. a wrong password: rudolfd's message, still the sign-in screen" "$([ "$(st signmsg)" = 'The user name or password is incorrect.' ] && [ "$(st signin)" = 1 ] && echo 1)" "$(st signmsg)"
LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return
w8 '[ "$(st user)" = Ann ] && [ "$(st signin)" = 0 ]'; ok "5. the right password (not ASCII): signed in, the taskbar shown" "$([ "$(st user)" = Ann ] && [ "$(st bar)" = "1 0 754 1280 46" ] && echo 1)" "$(head -8 $D/state)"
# ---- the wallpaper and the taskbar
w8 'grep -q "^hit pin calc" $D/state'; timeout 1 tail -f /dev/null; shot
ok "6. the wallpaper: the colour (solid)" "$(close "$(px 640 400)" "17 34 51" 2 && echo 1)" "$(px 640 400)"
ok "7. the taskbar: the standard dark bar" "$(close "$(px 800 757)" "17 24 39" 2 && echo 1)" "$(px 800 757)"
ok "8. ...the start button, then the pinned apps in order" "$(grep '^hit ' $D/state | head -4 | awk '{printf "%s:%s ", $2, $3}' | grep -q '^start:- pin:notepad pin:calc pin:settings' && echo 1)" "$(grep '^hit' $D/state)"
ok "9. ...the tray (network, sound, battery, notifications) and the clock" "$(for k in net vol bat notes clock; do grep -q "^hit $k " $D/state || echo no; done | grep -q no || echo 1)" "$(grep '^hit' $D/state)"
ok "10. ...the clock in 24 hours (h24) and the date as year/month/day (df)" "$(st clock | grep -qE '^[0-2][0-9]:[0-5][0-9]\|20[0-9][0-9]/[01][0-9]/[0-3][0-9]$' && echo 1)" "$(st clock)"
# 1.5.9 step 7 (docs/plan-1.5.9.md 12.3b)
printf '%s\n' '#include <X11/Xlib.h>' '#include <X11/extensions/Xfixes.h>' '#include <stdio.h>' \
	'int main(void) { Display *d = XOpenDisplay(NULL); if (!d) return 2; XFixesCursorImage *c = XFixesGetCursorImage(d); if (!c) return 1; printf("%d %d %d %d\n", c->width, c->height, c->xhot, c->yhot); return 0; }' > $T/cur.c
gcc -o $T/cur $T/cur.c -lX11 -lXfixes 2>/dev/null; xdotool mousemove 640 300; timeout 0.5 tail -f /dev/null; c=$($T/cur 2>/dev/null)
ok "47. the screen shows the arrow, not X's bare cross (step 7, bug 2)" "$(echo "$c" | awk '$3<=3 && $4<=3 {print 1}')" "$c"
P=$(hit pin notepad); ok "48. a pinned app shows its picture as the page draws it, and the start button the version (bug 3)" "$(close "$(px $P)" "16 185 129" 6 && [ "$(st icons)" = "1 9.9.9" ] && echo 1)" "$(px $P) $(st icons)"
# 1.6.0 (Part 12, item 1): the desktop's icons
di() { grep "^dicon .*|$1$" $D/state | head -1; }
cl() { grep "^client $1 " $D/state | head -1; }          # client ID FRAME X Y W H min= max= full= focus= app= TITLE
cv() { cl $1 | awk -v f=$2 '{print $f}'; }               # one field: 4 x, 5 y, 6 w, 7 h
w8 '[ -n "$(di Notepad)" ]' 20; timeout 0.5 tail -f /dev/null; shot
ok "56. the desktop's icons, on the page's grid: in the columns, and where the page put one (the Calculator)" "$([ "$(di Notepad | cut -d' ' -f2-5)" = "0 0 8 8" ] && [ "$(di Calculator | cut -d' ' -f2-5)" = "2 1 176 94" ] && [ "$(di 'My Computer' | cut -d' ' -f2-3)" = "0 1" ] && close "$(px 50 34)" "16 185 129" 6 && echo 1)" "$(grep '^dicon' $D/state) $(px 50 34)"
xdotool mousemove 50 40 click --repeat 2 --delay 80 1; w8 'grep -q "^app notepad .* Notepad$" $D/state' 80
ok "57. a double-click opens the icon's app" "$(grep -q '^app notepad' $D/state && [ "$(di Notepad | cut -d' ' -f7)" = 1 ] && echo 1)" "$(grep '^app\|^dicon' $D/state)"
NID=$(grep '^client .* app=notepad' $D/state | cut -d' ' -f2); X=$(cv $NID 4); Y=$(cv $NID 5); FW=$(( $(cv $NID 6) + 2 )); click $((X+FW-21)) $((Y+18)); w8 '[ -z "$(cl $NID)" ]' 40
xdotool mousemove 218 134 mousedown 1; for k in 1 2 3 4 5 6; do xdotool mousemove $((218+k*42)) $((134+k*29)); timeout 0.1 tail -f /dev/null; done; xdotool mouseup 1; timeout 0.5 tail -f /dev/null
ok "58. dragging an icon: to the nearest cell, kept by rudolfd for the shell (desktop/cells)" "$([ "$(di Calculator | cut -d' ' -f2-3)" = "5 3" ] && grep -q '"Calculator":\[5,3\]' $D/cells.json && echo 1)" "$(di Calculator) $(cat $D/cells.json 2>/dev/null)"
cmi() { grep "^cmitem .*|$1$" $D/state | head -1 | awk '{print $2, $3}'; }   # an item of the menu that is open, its middle
dc() { di "$1" | awk '{print $4+42, $5+24}'; }                           # an icon's picture
xdotool mousemove 900 500 click 3; w8 '[ "$(st dmenu)" = desk ] && [ -n "$(cmi "Arrange icons by name")" ]' 20; click $(cmi "Arrange icons by name"); timeout 0.8 tail -f /dev/null
ok "59. the desktop's menu: Arrange icons by name" "$([ "$(di Calculator | cut -d' ' -f2-3)" = "0 0" ] && [ "$(di 'My Computer' | cut -d' ' -f2-3)" = "0 1" ] && [ "$(di Notepad | cut -d' ' -f2-3)" = "0 2" ] && echo 1)" "$(grep '^dicon\|^dmenu' $D/state)"
xdotool mousemove 50 40 click 3; w8 '[ "$(st dmenu)" = icon ]' 20; xdotool key Escape
ok "60. an icon's own menu" "$([ "$(st dmenu)" = icon ] && echo 1)"
# 1.6.0 (Part 12, item 1, second part): the Desktop folder's files, the page's menus and dialogs drawn by the shell
xdotool mousemove 900 500 click 3; w8 '[ -n "$(cmi "New text document")" ]' 20; click $(cmi "New text document"); w8 '[ "$(st dialog | cut -d" " -f1)" = rename ]' 80
ok "61. the desktop's menu: New text document, then the page's Rename dialog, its name chosen" "$([ "$(st dialog)" = "rename NEW TEXT DOCUMENT.TXT|" ] && echo 1)" "$(st dialog)"
xdotool type --delay 30 'notes.txt'; xdotool key Return; w8 '[ -n "$(di NOTES.TXT)" ]' 80
ok "62. ...a name typed and Enter: the file's icon on the desktop (the page's upper case)" "$(di NOTES.TXT | grep -q '|file|NOTES.TXT$' && [ "$(st dialog | cut -d' ' -f1)" = - ] && echo 1)" "$(grep '^dicon\|^dialog' $D/state)"
xdotool mousemove $(dc NOTES.TXT) click 3; w8 '[ "$(st dmenu)" = file ]' 20
ok "63. a file's menu: Open, Open with…, Rename, Move to Trash, Properties" "$([ "$(st cmenu | cut -d' ' -f5-)" = NOTES.TXT ] && grep '^cmitem' $D/state | cut -d'|' -f2 | tr '\n' ',' | grep -q '^Open,Open with…,Rename,Move to Trash,Properties,$' && echo 1)" "$(grep '^cmenu\|^cmitem' $D/state)"
xdotool key Escape; w8 '[ -z "$(st cmenu)" ]' 20; click $(dc NOTES.TXT); w8 'di NOTES.TXT | grep -q " 1 |file|"' 20; xdotool key F2; w8 '[ "$(st dialog | cut -d" " -f1)" = rename ]' 40; xdotool type --delay 30 'plan.txt'; xdotool key Return; w8 '[ -n "$(di PLAN.TXT)" ]' 80
ok "64. F2: Rename, the new name kept" "$([ -n "$(di PLAN.TXT)" ] && [ -z "$(di NOTES.TXT)" ] && echo 1)" "$(di NOTES.TXT) / $(di PLAN.TXT) / $(st dialog) / $(st dmenu)"
xdotool mousemove 900 500 click 3; w8 '[ -n "$(cmi "New folder")" ]' 20; click $(cmi "New folder"); w8 '[ "$(st dialog | cut -d" " -f1)" = rename ]' 80
xdotool type --delay 30 'plan.txt'; xdotool key Return; w8 '[ -n "$(st dialog | cut -d"|" -f2)" ]' 40
ok "65. ...a name already used: the page's message, the dialog still open" "$([ "$(st dialog)" = "rename NEW FOLDER|That name is already used." ] && echo 1)" "$(st dialog)"
xdotool key Escape; w8 '[ -n "$(di "NEW FOLDER")" ]' 40; click $(dc PLAN.TXT); w8 'di PLAN.TXT | grep -q " 1 |file|"' 20; xdotool key Delete; w8 '[ -z "$(di PLAN.TXT)" ]' 80
xdotool mousemove $(dc Trash) click 3; w8 '[ "$(st dmenu)" = icon ]' 20
ok "66. Delete: the file to the Trash, whose menu can empty it now" "$([ -z "$(di PLAN.TXT)" ] && [ -n "$(di 'NEW FOLDER')" ] && grep -q '^cmitem [0-9]* [0-9]* 0 icon:Empty trash…|Empty trash…$' $D/state && echo 1)" "$(grep '^dicon\|^cmitem' $D/state)"
xdotool key Escape
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
set_s '{"s":{"bs":"solid","b1":"#112233","tb":"bottom","accent":"#2563eb","scale":150},"pins":["notepad","calc","settings"],"notes":2,"wallpaper":false}'
w8 '[ "$(st bar | cut -d" " -f5)" = 69 ]' 40; timeout 1 tail -f /dev/null
ok "49. the display scaling (150 %): the taskbar at the page's size (bug 3)" "$([ "$(st bar)" = "1 0 731 1280 69" ] && [ "$(grep '^hit pin notepad' $D/state | awk '{print $7}')" = 51 ] && echo 1)" "$(st bar) $(grep '^hit pin' $D/state)"
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
ok "32. ...the active title bar in the accent colour" "$(close "$(px $((X+60)) $((Y+4)))" "37 99 235" 3 && echo 1)" "$(px $((X+60)) $((Y+4)))"
xdotool mousemove $((X+60)) $((Y+15)) mousedown 1; for k in 1 2 3 4 5; do xdotool mousemove $((X+60+k*40)) $((Y+15+k*20)); timeout 0.1 tail -f /dev/null; done; xdotool mouseup 1; timeout 0.5 tail -f /dev/null
ok "33. dragging the title bar moves it" "$([ "$(cv $ID 4)" = $((X+200)) ] && [ "$(cv $ID 5)" = $((Y+100)) ] && echo 1)" "$(cl $ID) from $X $Y"
ok "34. ...and the window is told where it is (a ConfigureNotify with the screen's coordinates)" "$(grep -q "^geom $((X+200+1)) $((Y+100+35)) 400 300" $T/xc.log && echo 1)" "$(grep geom $T/xc.log | tail -2)"
X=$(cv $ID 4); Y=$(cv $ID 5); FW=$((W+2)); FH=$(( $(cv $ID 7) + 36 ))
xdotool mousemove $((X+FW-2)) $((Y+FH-2)) mousedown 1; for k in 1 2 3 4; do xdotool mousemove $((X+FW-2+k*20)) $((Y+FH-2+k*15)); timeout 0.1 tail -f /dev/null; done; xdotool mouseup 1; timeout 0.5 tail -f /dev/null
ok "35. dragging the bottom-right corner resizes it" "$([ "$(cv $ID 6)" = 480 ] && [ "$(cv $ID 7)" = 360 ] && echo 1)" "$(cl $ID)"
W=$(cv $ID 6); FW=$((W+2)); click $((X+FW-48)) $((Y+18)); timeout 0.5 tail -f /dev/null
ok "36. maximise: the whole screen above the taskbar" "$([ "$(flag $ID max)" = 1 ] && [ "$(cv $ID 4) $(cv $ID 5) $(cv $ID 6) $(cv $ID 7)" = "0 0 1278 718" ] && echo 1)" "$(cl $ID)"
xdotool mousemove 300 15 click --repeat 2 --delay 80 1; timeout 0.5 tail -f /dev/null
ok "37. a double-click on the title bar: back to its size and place" "$([ "$(flag $ID max)" = 0 ] && [ "$(cv $ID 4) $(cv $ID 6)" = "$X 480" ] && echo 1)" "$(cl $ID)"
click $((X+FW-75)) $((Y+18)); timeout 0.5 tail -f /dev/null
ok "38. minimise: out of sight (WM_STATE iconic), the next window gets the keyboard" "$([ "$(flag $ID min)" = 1 ] && [ "$($XC wmstate 0x$ID)" = 3 ] && [ "$(flag $ID focus)" = 0 ] && grep -q '^client .* focus=1 ' $D/state && echo 1)" "$(grep '^client' $D/state) $($XC wmstate 0x$ID)"
click $(hit win $ID); timeout 0.5 tail -f /dev/null
ok "39. its taskbar button brings it back, in front" "$([ "$(flag $ID min)" = 0 ] && [ "$(flag $ID focus)" = 1 ] && [ "$($XC getfocus)" = "0x$ID" ] && echo 1)" "$(cl $ID)"
CID=$(grep '^client .* app=calc' $D/state | cut -d' ' -f2); click $(( $(cv $CID 4) + 40 )) $(( $(cv $CID 5) + 36 + $(cv $CID 7) / 2 )); timeout 0.5 tail -f /dev/null
ok "40. a click into another window gives it the keyboard (and the click)" "$([ "$(flag $CID focus)" = 1 ] && [ "$(flag $ID focus)" = 0 ] && echo 1)" "$(grep '^client' $D/state)"
$XC full 0x$ID 1; timeout 0.5 tail -f /dev/null
ok "41. a window asking for full screen covers the whole screen, taskbar too" "$([ "$(flag $ID full)" = 1 ] && grep -q '^geom 0 0 1280 800' $T/xc.log && [ "$($XC stack | tail -1)" = "0x$(cl $ID | cut -d' ' -f3)" ] && echo 1)" "$(cl $ID); $(tail -2 $T/xc.log); $($XC stack | tail -2)"
$XC full 0x$ID 0; timeout 0.5 tail -f /dev/null
ok "42. ...and back" "$([ "$(flag $ID full)" = 0 ] && [ "$(cv $ID 6)" = 480 ] && echo 1)" "$(cl $ID)"
$XC inno > $T/inno.log & IP=$!; w8 'grep -q "^dlg" $T/inno.log'; DLG=$(sed -n 's/^dlg 0x//p' $T/inno.log); OW=$(sed -n 's/^id 0x//p' $T/inno.log); w8 '[ -n "$(cl $DLG)" ]' 20
ok "43. a dialog is centred over its window" "$(dx=$(( $(cv $DLG 4) + ($(cv $DLG 6) + 2) / 2 - $(cv $OW 4) - ($(cv $OW 6) + 2) / 2 )); [ ${dx#-} -le 2 ] && echo 1)" "$(cl $OW) / $(cl $DLG)"
ok "44. ...and is not on the taskbar" "$( ! grep -q "^hit win $DLG " $D/state && echo 1)"
kill $IP 2>/dev/null; w8 '[ -z "$(cl $DLG)" ]' 20
X=$(cv $ID 4); Y=$(cv $ID 5); FW=$(( $(cv $ID 6) + 2 )); click $((X+FW-21)) $((Y+18))
w8 'grep -q "^delete" $T/xc.log' 20; w8 '[ -z "$(cl $ID)" ]' 20
ok "45. close: WM_DELETE_WINDOW, the program closes its window, the frame and the taskbar button go" "$(grep -q '^delete' $T/xc.log && [ -z "$(cl $ID)" ] && ! grep -q "^hit win $ID " $D/state && echo 1)" "$(cat $T/xc.log | tail -2)"
NID=$(grep '^client .* app=notepad' $D/state | cut -d' ' -f2); X=$(cv $NID 4); Y=$(cv $NID 5); FW=$(( $(cv $NID 6) + 2 )); click $((X+FW-21)) $((Y+18))
w8 '[ -z "$(cl $NID)" ]' 40
ok "46. an app window's close button asks the page (as its ✕), which closes it" "$([ -z "$(cl $NID)" ] && ! grep -q '^app notepad' $D/state && echo 1)" "$(grep '^client\|^app' $D/state)"
# ---- a drop-down in an app window (bug 5: WebKit's own pop-up under the window manager)
cp $HERE/fixtures/native_select.html $D/page.html; click $(hit pin notepad)
w8 'grep -q "^client .* app=notepad .*Drop-down$" $D/state' 80; timeout 1.5 tail -f /dev/null
SC=$(grep '^client .* app=notepad' $D/state | cut -d' ' -f2); X=$(cv $SC 4); Y=$(cv $SC 5)
click $((X+1+150)) $((Y+35+40)); timeout 1.5 tail -f /dev/null; shot
ncol() { convert $T/s.png -crop 290x60+$((X+1+25))+$((Y+35+64)) -format '%k' info:; }
ok "50. a drop-down opens its list over the window, drawn (its items' writing, the chosen one highlighted)" "$([ "$(ncol)" -gt 4 ] && echo 1)" "$(ncol)"
xdotool key Down Return; w8 'grep -q "^client .* app=notepad .*chose two$" $D/state' 20
ok "51. ...and choosing in it works" "$(grep -q '^client .* app=notepad .*chose two$' $D/state && echo 1)" "$(grep '^client' $D/state)"
rm -f $D/page.html; X=$(cv $SC 4); Y=$(cv $SC 5); FW=$(( $(cv $SC 6) + 2 )); click $((X+FW-21)) $((Y+18)); timeout 6 tail -f /dev/null
[ -n "$(cl $SC)" ] && { click $((X+FW-21)) $((Y+18)); w8 '[ -z "$(cl $SC)" ]' 20; }
# ---- lock and sign out
xdotool key super+l
w8 '[ "$(st locked)" = 1 ]' 20; ok "24. Lock: the lock screen, rudolfd told (new app windows get no session meanwhile)" "$([ "$(st locked)" = 1 ] && [ "$(st signin)" = 1 ] && grep -q 'POST desktop/lock' $D/calls && [ "$($XC stack | tail -1)" = "0x$(xdotool search --name '^Sign in$' | head -1 | xargs printf '%x')" ] && echo 1)" "$(head -6 $D/state)"
xdotool type --delay 15 'nope'; xdotool key Return; w8 '[ -n "$(st signmsg)" ]' 20
ok "25. ...a wrong password: refused" "$([ "$(st locked)" = 1 ] && [ "$(st signmsg)" = 'The password is incorrect.' ] && echo 1)" "$(st signmsg)"
LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return; w8 '[ "$(st locked)" = 0 ]' 40
ok "26. ...the right one: back, the windows as they were" "$([ "$(st locked)" = 0 ] && [ "$(grep -c '^app ' $D/state)" = 1 ] && [ "$(flag $CID min)" = 0 ] && echo 1)" "$(grep '^app\|^client\|^locked' $D/state)"
click $(hit start -); w8 '[ "$(st menu)" = 1 ]' 20; xdotool type --delay 40 'log off'; w8 'grep -q "^mrow logoff " $D/state' 20; L=$(grep '^mrow logoff ' $D/state | awk '{print $3+int($5/2), $4+int($6/2)}'); click $L
w8 '[ "$(st signin)" = 1 ]' 20; ok "27. Sign out: the app windows close, rudolfd ends the session, the sign-in screen" "$([ "$(st user)" = - ] && [ "$(grep -c '^app ' $D/state)" = 0 ] && grep -q 'POST desktop/logout' $D/calls && echo 1)" "$(head -6 $D/state)"
# ---- the development link (bugs 4 and 6: shown once by the shell, not in every app window)
printf '%s' '{"ok":true,"available":true,"on":true,"repo":"someone/devlink","secondsLeft":3600,"ask":true,"status":{"state":"on"},"pending":{"id":"c7","kind":"run","what":"uname -a"}}' > $D/devlink.json
w8 '[ "$(st devlink)" = "1 24" ]' 40; timeout 1 tail -f /dev/null; shot
ok "52. the development link on: the shell's red banner at the top of the screen" "$([ "$(st devlink)" = "1 24" ] && close "$(px 1170 12)" "176 0 32" 6 && echo 1)" "$(st devlink) $(px 1170 12)"
w8 '[ "$(st dlask)" = c7 ]' 20; ok "53. ...and a command waiting: the shell asks, Refuse first" "$([ "$(st dlask)" = c7 ] && echo 1)" "$(st dlask)"
xdotool key Return; w8 'grep -q "POST devlink/answer?id=c7&a=refuse" $D/calls' 20
ok "54. ...refused: rudolfd told, the question gone" "$(grep -q 'POST devlink/answer?id=c7&a=refuse' $D/calls && w8 '[ "$(st dlask)" = - ]' 20 && echo 1)" "$(grep devlink $D/calls | tail -3)"
click 1220 12; w8 '[ "$(st devlink)" = "0 0" ]' 30
ok "55. End now: the link turned off, the banner gone" "$(grep -q 'POST devlink/off' $D/calls && [ "$(st devlink)" = "0 0" ] && echo 1)" "$(st devlink)"
kill $NP 2>/dev/null
echo "$((N-F))/$N passed"; [ $F = 0 ]
