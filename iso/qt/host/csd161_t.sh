#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 15, item 2): one title bar for apps that draw their own. rudolf-native on a virtual screen (Xvfb)
# with the stand-in rudolfd (fixtures/native_rd.py), and a stand-in app that draws its own title bar as Electron's and GTK's do
# (fixtures/xwmclient.c csd: _MOTIF_WM_HINTS with no decorations, _GTK_FRAME_EXTENTS, _NET_WM_MOVERESIZE from its own bar):
# no title bar of Rudolf's (the frame its edge alone, _NET_FRAME_EXTENTS says so), moving by its bar, resizing from each edge
# and corner, the keyboard's move and size, maximize and restore (the transition), minimize, full screen, the three workspaces,
# GTK's margins cut away, the shadow beneath, a window switching between the two; Rudolf's own title bar on the other windows
# as before. Then Standard (rudolf-display, its window manager) given the same app: there too it has its own bar alone.
# Then real apps when the host has them: GTK 3 (gtk3-widget-factory), GTK 4 (gtk4-widget-factory) and Chromium as an
# Electron-style app (Playwright's, its own tab strip as its title bar). Needs root (a private /run, in a mount namespace),
# Xvfb, xdotool, ImageMagick, node, python3, gcc and WebKitGTK's headers.   sh csd161_t.sh [RUDOLF-NATIVE]
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$CSD_INNER" ] && exec env CSD_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; T=$(mktemp -d /tmp/csd161.XXXXXX); PORT=${CSD_PORT:-8616}; DSP=${CSD_DSP:-:88}
command -v Xvfb >/dev/null && command -v xdotool >/dev/null && command -v convert >/dev/null || { echo "SKIP: needs Xvfb, xdotool and ImageMagick"; exit 0; }
mount --make-rprivate / 2>/dev/null; mount -t tmpfs none /run; mkdir -p /run/rudolf   # (rudolf-display's files: this namespace's own)
BIN=$1
if [ -z "$BIN" ]; then BIN=$T/rudolf-native
	gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $BIN $SYS/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
	grep -q warning $T/cc && { cat $T/cc; echo "FAIL build warnings"; exit 1; }
fi
XC=$T/xc; gcc -O2 -Wall -o $XC $HERE/fixtures/xwmclient.c -lX11 2>$T/cc || { cat $T/cc; echo "FAIL build xwmclient"; exit 1; }
D=$T/d; mkdir -p $D; node $HERE/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)")
printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","b2":"#000080","tb":"bottom","accent":"#2563eb","h24":true,"df":"ymd"},"pins":["notepad","calc","settings"],"notes":2,"wallpaper":false}' > $D/settings.json
python3 $HERE/fixtures/native_rd.py $PORT $D > $T/rd.log 2>&1 & RDP=$!
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=$DSP LIBGL_ALWAYS_SOFTWARE=1 XWM_DIR=$T/dir
trap 'kill $NP $RDP $XV $A1 $A2 $A3 $A4 $G3 $G4 $CR $RDS 2>/dev/null; pkill -f "$T/" 2>/dev/null; rm -rf $T' EXIT
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
w8 'xdotool getmouselocation >/dev/null 2>&1' 40
(cd $D && HOME=$D RUDOLF_NATIVE_STATE=$D/state exec $BIN --size=1280x800 --draw=soft --base=http://127.0.0.1:$PORT > $T/native.log 2>&1) & NP=$!
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
shot() { import -window root $T/s.png 2>/dev/null; }
px() { convert $T/s.png -format "%[fx:int(255*p{$1,$2}.r)] %[fx:int(255*p{$1,$2}.g)] %[fx:int(255*p{$1,$2}.b)]" info:; }
close() { echo "$1|$2" | awk -F'|' -v t=${3:-8} '{split($1,x," ");split($2,y," ");for(i=1;i<=3;i++){d=x[i]-y[i];if(d<0)d=-d;if(d>t)exit 1}}'; }
hit() { grep "^hit $1 $2 " $D/state | head -1 | awk '{print $4+int($6/2), $5+int($7/2)}'; }
cl() { grep "^client $1 " $D/state | head -1; }          # client ID FRAME X Y W H min= max= full= focus= app= TITLE
cv() { cl $1 | awk -v f=$2 '{print $f}'; }               # 4 x, 5 y, 6 w, 7 h
geo() { echo "$(cv $1 4) $(cv $1 5) $(cv $1 6) $(cv $1 7)"; }
flag() { cl $1 | grep -o "$2=[0-9]" | cut -d= -f2; }
csd() { grep "^clientcsd $1 " $D/state | head -1 | cut -d' ' -f3-; }   # csd il it ir ib
ext() { $XC props 0x$1 | sed -n 's/^extents //p'; }
nst() { $XC props 0x$1 | sed -n 's/^state //p'; }
lastgeom() { grep '^geom' $1 | tail -1 | cut -d' ' -f2-; }
idof() { w8 "grep -q '^id' $1" 40; sed -n 's/^id 0x//p' $1 | head -1; }
# (1.6.0 Part 17) the window transitions, watched in the state every 10 ms for 1.2 s, as native59_t.sh does
lpw() { : > $T/lpw; i=0; while [ $i -lt 120 ]; do { grep "^lp " $D/state; grep "^client $1 " $D/state; echo ---; } >> $T/lpw 2>/dev/null; timeout 0.01 tail -f /dev/null; i=$((i+1)); done; }
# a drag with the pointer from X Y by DX DY, in five steps (the app asks _NET_WM_MOVERESIZE on the press, direction DIR)
pdrag() { echo $5 > $T/dir; xdotool mousemove $1 $2 mousedown 1; timeout 0.3 tail -f /dev/null; k=1; while [ $k -le 5 ]; do xdotool mousemove $(($1+$3*k/5)) $(($2+$4*k/5)); timeout 0.08 tail -f /dev/null; k=$((k+1)); done; xdotool mouseup 1; timeout 0.4 tail -f /dev/null; }
# ---- signed in
w8 '[ "$(st signin)" = 1 ]' 120; LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return
w8 '[ "$(st user)" = Ann ] && [ "$(st signin)" = 0 ]' 120; w8 'grep -q "^hit pin calc" $D/state'; timeout 1 tail -f /dev/null
SUP=$($XC supported | tr '\n' ' ')
ok "1. _NET_SUPPORTED says what such apps need: _NET_WM_MOVERESIZE, MAXIMIZED_VERT and _HORZ, _NET_FRAME_EXTENTS (and its request), _GTK_FRAME_EXTENTS, _GTK_SHOW_WINDOW_MENU" "$(for a in _NET_WM_MOVERESIZE _NET_WM_STATE_MAXIMIZED_VERT _NET_WM_STATE_MAXIMIZED_HORZ _NET_FRAME_EXTENTS _NET_REQUEST_FRAME_EXTENTS _GTK_FRAME_EXTENTS _GTK_SHOW_WINDOW_MENU _NET_WM_STATE_FULLSCREEN _NET_WM_STATE_HIDDEN; do echo " $SUP" | grep -q " $a " || echo no; done | grep -q no || echo 1)" "$SUP"
ok "2. _NET_REQUEST_FRAME_EXTENTS before a window asking for no decorations is shown: its edges alone (1 1 1 1)" "$([ "$($XC reqfe | sed -n 's/^extents //p')" = "1 1 1 1" ] && echo 1)" "$($XC reqfe)"
# ---- a framed window (Rudolf's title bar) and one drawing its own
$XC app > $T/a1.log & A1=$!; ID1=$(idof $T/a1.log); w8 '[ -n "$(cl $ID1)" ]' 40
$XC csd 500 360 > $T/a2.log & A2=$!; ID=$(idof $T/a2.log); w8 '[ -n "$(cl $ID)" ]' 40; timeout 1.2 tail -f /dev/null
ok "3. a window asking for no decorations: no title bar of Rudolf's (csd), its frame the window's size and its 1 px edges" "$([ "$(csd $ID)" = "1 0 0 0 0" ] && [ "$(cv $ID 6) $(cv $ID 7)" = "500 360" ] && [ "$(xwininfo -id 0x$(cl $ID | cut -d' ' -f3) 2>/dev/null | awk '/Width:/{w=$2} /Height:/{h=$2} END{print w, h}')" = "502 362" ] && echo 1)" "$(cl $ID) $(csd $ID)"
ok "4. ..._NET_FRAME_EXTENTS says so (1 1 1 1); a framed window's says its title bar (1 1 35 1)" "$([ "$(ext $ID)" = "1 1 1 1" ] && [ "$(ext $ID1)" = "1 1 35 1" ] && echo 1)" "$(ext $ID) / $(ext $ID1)"
X=$(cv $ID 4); Y=$(cv $ID 5); [ -n "$CSD_DEBUG" ] && { xwininfo -tree -root > $CSD_DEBUG/tree.txt; cp $D/state $CSD_DEBUG/state5; }
ok "5. ...the window is told it is 1 px inside its frame (ConfigureNotify)" "$([ "$(lastgeom $T/a2.log)" = "$((X+1)) $((Y+1)) 500 360" ] && echo 1)" "$(lastgeom $T/a2.log) frame $X $Y"
ok "6. ...it has the keyboard, a taskbar button and its title there" "$([ "$(flag $ID focus)" = 1 ] && grep -q "^hit win $ID " $D/state && grep -q "^client $ID .* Own title bar stand-in$" $D/state && echo 1)" "$(cl $ID); $(grep '^hit win' $D/state)"
shot; [ -n "$CSD_DEBUG" ] && cp $T/s.png $CSD_DEBUG/s7.png
T7=$(close "$(px $((X+100)) $((Y+3)))" "31 41 55" 3 && ! close "$(px $((X+100)) $((Y+12)))" "37 99 235" 20 && echo 1)
ok "7. on the screen: its own (dark) bar at the frame's top, under the 1 px grey edge; no accent-coloured bar of Rudolf's" "$(close "$(px $((X+100)) $((Y+3)))" "31 41 55" 3 && close "$(px $((X+100)) $Y)" "156 163 175" 12 && ! close "$(px $((X+100)) $((Y+12)))" "37 99 235" 20 && echo 1)" "$(px $((X+100)) $((Y+3))) / edge $(px $((X+100)) $Y)"
X1=$(cv $ID1 4); Y1=$(cv $ID1 5)
ok "8. ...its frame's outline as Rudolf's framed windows' (the same corner)" "$([ "$(px $X $Y)" = "$(px $X1 $Y1)" ] && echo 1)" "$(px $X $Y) / $(px $X1 $Y1)"
ok "9. ...the shadow beneath it, as other windows' (the desktop darker just below its frame)" "$(p=$(px $((X+250)) $((Y+362+6))); echo "$p" | awk '{exit !($1+$2+$3 < 17+34+51-8)}' && echo 1)" "$(px $((X+250)) $((Y+362+6)))"
# ---- moving by its own bar: the app asks _NET_WM_MOVERESIZE (move) on the press
pdrag $((X+100)) $((Y+15)) 150 90 8
ok "10. dragging its own bar moves it (_NET_WM_MOVERESIZE, move): the pointer's way" "$([ "$(cv $ID 4) $(cv $ID 5)" = "$((X+150)) $((Y+90))" ] && echo 1)" "$(cl $ID) from $X $Y; $(tail -3 $T/a2.log)"
ok "11. ...and it is told where it is once let go" "$([ "$(lastgeom $T/a2.log)" = "$((X+151)) $((Y+91)) 500 360" ] && echo 1)" "$(lastgeom $T/a2.log)"
xdotool mousemove $((X+400)) $((Y+300)); timeout 0.4 tail -f /dev/null
ok "12. ...the drag ends with the button: the pointer moving afterwards leaves it" "$([ "$(cv $ID 4) $(cv $ID 5)" = "$((X+150)) $((Y+90))" ] && echo 1)" "$(cl $ID)"
pdrag $((X+250)) $((Y+105)) 0 -400 8
ok "13. ...as Rudolf's own title bar: not above the desktop's top" "$([ "$(cv $ID 5)" = 0 ] && echo 1)" "$(cl $ID)"
pdrag $((X+250)) 15 -150 140 8
# ---- resizing from each edge and corner: the app asks _NET_WM_MOVERESIZE with the edge (0 top-left, clockwise to 7 left)
# (expected as drag_motion makes it, the page's addResize: the size with the edges and the pointer's way, at least 200 x 120)
exp() { awk -v d=$1 -v x=$2 -v y=$3 -v w=$4 -v h=$5 -v dx=$6 -v dy=$7 'BEGIN{e=(d==0)?5:(d==1)?4:(d==2)?6:(d==3)?2:(d==4)?10:(d==5)?8:(d==6)?9:1;
	ow=w+2; oh=h+2; if(e%2==1){nw=ow-dx; if(nw<200)nw=200; x=x+(ow-nw); w=nw} if(int(e/2)%2==1){w=ow+dx; if(w<200)w=200}
	if(int(e/4)%2==1){nh=oh-dy; if(nh<120)nh=120; nt=y+(oh-nh); if(nt<0)nt=0; h=oh+y-nt; y=nt} if(int(e/8)%2==1){h=oh+dy; if(h<120)h=120} print x, y, w, h}'; }
R=""; for d in 0 1 2 3 4 5 6 7; do G0=$(geo $ID); set -- $G0; e=$(exp $d $1 $2 $3 $4 40 30)
	pdrag $(($1+250)) $(($2+200)) 40 30 $d; g=$(geo $ID); [ "$g" = "$e" ] || R="$R [dir $d: $g, want $e]"; done
ok "14. a resize from each edge and corner (_NET_WM_MOVERESIZE 0-7) as a drag of Rudolf's own edges" "$([ -z "$R" ] && echo 1)" "$R"
ok "15. ...told its new size once let go" "$([ "$(lastgeom $T/a2.log | cut -d' ' -f3-)" = "$(cv $ID 6) $(cv $ID 7)" ] && echo 1)" "$(lastgeom $T/a2.log) / $(cl $ID)"
# ---- the keyboard's move and size (the app's own Move and Size: GTK's window menu, Alt+F7 and Alt+F8)
G0=$(geo $ID); set -- $G0; echo 10 > $T/dir; xdotool mousemove $(($1+250)) $(($2+200)) click 1; timeout 0.4 tail -f /dev/null
xdotool key Right Right Right Down Down; timeout 0.3 tail -f /dev/null; xdotool key Return; timeout 0.4 tail -f /dev/null
ok "16. the keyboard's move (_NET_WM_MOVERESIZE 10): the arrows move it 10 px a step, Enter ends it" "$([ "$(geo $ID)" = "$(($1+30)) $(($2+20)) $3 $4" ] && echo 1)" "$(geo $ID) from $G0"
G0=$(geo $ID); set -- $G0; echo 9 > $T/dir; xdotool mousemove $(($1+250)) $(($2+200)) click 1; timeout 0.4 tail -f /dev/null
xdotool key Right Right Down; timeout 0.3 tail -f /dev/null; xdotool key Return; timeout 0.4 tail -f /dev/null
ok "17. the keyboard's size (9): from the bottom-right corner, as a drag of it" "$([ "$(geo $ID)" = "$1 $2 $(($3+2+20)) $(($4+2+10))" ] && echo 1)" "$(geo $ID) from $G0"
G0=$(geo $ID); set -- $G0; echo 10 > $T/dir; xdotool mousemove $(($1+250)) $(($2+200)) click 1; timeout 0.4 tail -f /dev/null
xdotool key Left Left Up; timeout 0.3 tail -f /dev/null; xdotool key Escape; timeout 0.4 tail -f /dev/null
ok "18. ...Escape puts it back where it was" "$([ "$(geo $ID)" = "$G0" ] && echo 1)" "$(geo $ID) from $G0"
xdotool key a; timeout 0.3 tail -f /dev/null; echo 8 > $T/dir
ok "19. ...and the keyboard is the app's again (the shell's grab gone)" "$([ "$($XC getfocus)" = "0x$ID" ] && echo 1)" "$($XC getfocus)"
# ---- maximize and restore from the app (its button; a double-click on its bar), minimize
G0=$(geo $ID); lpw $ID & LW=$!; $XC max 0x$ID 1; wait $LW
ok "20. maximize from the app (_NET_WM_STATE_MAXIMIZED_VERT and _HORZ): Rudolf's maximize, with its transition (lp max)" "$([ "$(flag $ID max)" = 1 ] && grep -q "^lp max " $T/lpw && echo 1)" "$(cl $ID); $(grep '^lp' $T/lpw | sort | uniq -c | head -3)"
ok "21. ...where Rudolf puts a maximized window: #desk less 6 px (its frame's height the window's and its edges)" "$([ "$(geo $ID)" = "0 0 1274 752" ] && echo 1)" "$(geo $ID)"
ok "22. ...and its _NET_WM_STATE says so (the app shows its restore button)" "$([ "$(nst $ID)" = "MAXIMIZED_VERT,MAXIMIZED_HORZ" ] && grep -q '^state MAXIMIZED_VERT,MAXIMIZED_HORZ' $T/a2.log && echo 1)" "$(nst $ID); $(grep '^state' $T/a2.log | tail -2)"
pdrag 400 15 100 100 8
ok "23. ...maximized, its bar does not move it (as Rudolf's own title bar)" "$([ "$(geo $ID)" = "0 0 1274 752" ] && echo 1)" "$(geo $ID)"
lpw $ID & LW=$!; $XC max 0x$ID 2; wait $LW
ok "24. restore (toggled): back to its size and place, with the transition (lp res), the state cleared" "$([ "$(flag $ID max)" = 0 ] && [ "$(geo $ID)" = "$G0" ] && grep -q "^lp res " $T/lpw && [ "$(nst $ID)" = - ] && echo 1)" "$(cl $ID) want $G0; $(nst $ID)"
$XC iconify 0x$ID; timeout 1.2 tail -f /dev/null
ok "25. minimize from the app (WM_CHANGE_STATE): minimized, WM_STATE iconic, _NET_WM_STATE_HIDDEN" "$([ "$(flag $ID min)" = 1 ] && [ "$($XC wmstate 0x$ID)" = 3 ] && [ "$(nst $ID)" = HIDDEN ] && echo 1)" "$(cl $ID) $($XC wmstate 0x$ID) $(nst $ID)"
xdotool mousemove $(hit win $ID) click 1; timeout 1.2 tail -f /dev/null
ok "26. ...its taskbar button brings it back, the same size, no title bar of Rudolf's" "$([ "$(flag $ID min)" = 0 ] && [ "$(geo $ID)" = "$G0" ] && [ "$(nst $ID)" = - ] && [ "$(csd $ID | cut -d' ' -f1)" = 1 ] && echo 1)" "$(cl $ID) $(nst $ID)"
# ---- full screen
$XC full 0x$ID 1; timeout 0.6 tail -f /dev/null
ok "27. full screen: the whole screen, taskbar too; no edges (_NET_FRAME_EXTENTS 0 0 0 0)" "$([ "$(flag $ID full)" = 1 ] && [ "$(lastgeom $T/a2.log)" = "0 0 1280 800" ] && [ "$(ext $ID)" = "0 0 0 0" ] && [ "$(nst $ID)" = FULLSCREEN ] && echo 1)" "$(cl $ID) $(lastgeom $T/a2.log) $(ext $ID)"
$XC full 0x$ID 0; timeout 0.6 tail -f /dev/null
ok "28. ...and back" "$([ "$(flag $ID full)" = 0 ] && [ "$(geo $ID)" = "$G0" ] && [ "$(ext $ID)" = "1 1 1 1" ] && echo 1)" "$(cl $ID) $(ext $ID)"
# ---- the three workspaces
xdotool key ctrl+alt+shift+Right; timeout 0.8 tail -f /dev/null
ok "29. moved to workspace 2 (Ctrl+Alt+Shift+Right): the desktop follows it there, its frame as it was" "$([ "$(st ws)" = 2 ] && grep -q "^clientws $ID 2 0" $D/state && [ "$(geo $ID)" = "$G0" ] && echo 1)" "$(st ws) $(grep "^clientws $ID" $D/state)"
xdotool key ctrl+alt+Left; timeout 0.8 tail -f /dev/null
ok "30. ...workspace 1: it is out of sight there (not mapped, not on the taskbar)" "$([ "$(st ws)" = 1 ] && grep -q "^clientws $ID 2 1" $D/state && ! grep -q "^hit win $ID " $D/state && [ "$(xwininfo -id 0x$(cl $ID | cut -d' ' -f3) | awk '/Map State/{print $3}')" = IsUnMapped ] && echo 1)" "$(st ws) $(grep "^clientws $ID" $D/state)"
xdotool key ctrl+alt+Right; timeout 0.8 tail -f /dev/null; xdotool key ctrl+alt+shift+Right; timeout 0.8 tail -f /dev/null
ok "31. ...to workspace 3 with it" "$([ "$(st ws)" = 3 ] && grep -q "^clientws $ID 3 0" $D/state && echo 1)" "$(st ws) $(grep "^clientws $ID" $D/state)"
xdotool key ctrl+alt+shift+Left; timeout 0.8 tail -f /dev/null; xdotool key ctrl+alt+shift+Left; timeout 0.8 tail -f /dev/null
ok "31b. ...and back to 1: shown again, still its own bar alone, its size and place kept" "$([ "$(st ws)" = 1 ] && grep -q "^clientws $ID 1 0" $D/state && [ "$(csd $ID)" = "1 0 0 0 0" ] && [ "$(geo $ID)" = "$G0" ] && echo 1)" "$(st ws) $(grep "^clientws $ID" $D/state) $(csd $ID) $(geo $ID)"
# ---- the window's menu (a right-click on a GTK header bar: _GTK_SHOW_WINDOW_MENU)
set -- $G0; $XC menu 0x$ID $(($1+120)) $(($2+15)); w8 'grep -q "^cmitem .*|Close$" $D/state' 20
ok "32. _GTK_SHOW_WINDOW_MENU: the window's menu at the pointer (as its taskbar button's)" "$(grep -q '^cmitem .*wn:max|Maximize$' $D/state && grep -q '^cmitem .*wn:close|Close$' $D/state && echo 1)" "$(grep '^cmitem' $D/state | head -4)"
xdotool key Escape; timeout 0.5 tail -f /dev/null
# ---- GTK's margins (_GTK_FRAME_EXTENTS: its shadows, see-through around the window it draws)
$XC csd 540 400 20 20 15 25 > $T/a3.log & A3=$!; ID3=$(idof $T/a3.log); w8 '[ -n "$(cl $ID3)" ]' 40; timeout 1 tail -f /dev/null
X=$(cv $ID3 4); Y=$(cv $ID3 5)
ok "33. a GTK window with margins (20 20 15 25): they are cut away (the clip), the frame round what it draws (500 x 360)" "$([ "$(csd $ID3)" = "1 20 15 20 25" ] && [ "$(cv $ID3 6) $(cv $ID3 7)" = "500 360" ] && [ "$(lastgeom $T/a3.log)" = "$((X+1-20)) $((Y+1-15)) 540 400" ] && echo 1)" "$(cl $ID3) $(csd $ID3) $(lastgeom $T/a3.log)"
$XC gtkext 0x$ID3 0 0 0 0; timeout 0.6 tail -f /dev/null
ok "34. ...GTK drops them (as it does maximized): nothing cut, the window it draws the same size" "$([ "$(csd $ID3)" = "1 0 0 0 0" ] && [ "$(cv $ID3 6) $(cv $ID3 7)" = "500 360" ] && echo 1)" "$(cl $ID3) $(csd $ID3)"
kill $A3; w8 '[ -z "$(cl $ID3)" ]' 20
# ---- a window switching: Chromium's "Use system title bar and borders" turned on and off while it is open
$XC deco 0x$ID 1; timeout 0.6 tail -f /dev/null; shot; X=$(cv $ID 4); Y=$(cv $ID 5)
ok "35. it asks for decorations again: Rudolf's title bar (35 px), as any other window" "$([ "$(csd $ID | cut -d' ' -f1)" = 0 ] && [ "$(ext $ID)" = "1 1 35 1" ] && [ "$(lastgeom $T/a2.log | cut -d' ' -f2)" = $((Y+35)) ] && close "$(px $((X+60)) $((Y+4)))" "37 99 235" 3 && echo 1)" "$(csd $ID) $(ext $ID) $(lastgeom $T/a2.log) $(px $((X+60)) $((Y+4)))"
$XC deco 0x$ID 0; timeout 0.6 tail -f /dev/null
ok "36. ...and none again: its own bar alone" "$([ "$(csd $ID | cut -d' ' -f1)" = 1 ] && [ "$(ext $ID)" = "1 1 1 1" ] && echo 1)" "$(csd $ID) $(ext $ID)"
# ---- games: a borderless window the size of the screen is full screen at once, as before
$XC csd 1280 800 > $T/a4.log & A4=$!; ID4=$(idof $T/a4.log); w8 '[ "$(flag $ID4 full)" = 1 ]' 20
ok "37. a borderless window the screen's size (a game, Wine's): full screen, as rudolf-display takes it" "$([ "$(flag $ID4 full)" = 1 ] && [ "$(lastgeom $T/a4.log)" = "0 0 1280 800" ] && echo 1)" "$(cl $ID4) $(lastgeom $T/a4.log)"
kill $A4; w8 '[ -z "$(cl $ID4)" ]' 20
# ---- Rudolf's own title bar on the other windows, as before
X=$(cv $ID1 4); Y=$(cv $ID1 5); shot
ok "38. the framed window keeps Rudolf's title bar (35 px), dragged by it as before" "$(pdrag $((X+60)) $((Y+15)) 40 30 8; [ "$(cv $ID1 4) $(cv $ID1 5)" = "$((X+40)) $((Y+30))" ] && [ -z "$(csd $ID1)" ] && echo 1)" "$(cl $ID1)"
kill $A1 $A2; w8 '[ -z "$(cl $ID)" ]' 20
# ---- real apps, when the host has them: GTK 3, GTK 4 (their header bars) and Chromium (an Electron-style app: its own tab strip)
real() {   # NAME LOGFILE COMMAND...: the new client's window (the newest client line), "" when none came
	n0=$(grep -c '^client ' $D/state); nm=$1; lg=$2; shift 2; (NO_AT_BRIDGE=1 GDK_BACKEND=x11 GTK_A11Y=none exec "$@" > $lg 2>&1) & echo $! > $T/$nm.pid
	w8 "[ \$(grep -c '^client ' $D/state) -gt $n0 ]" 120; timeout 2.5 tail -f /dev/null; grep '^client ' $D/state | awk '$12 == "app=-"' | tail -1 | cut -d' ' -f2; }
rtest() {   # NAME ID N DX DY LOG: no title bar of Rudolf's; a drag of its own bar (at DX from its middle, DY down) moves it; a double-click there maximizes and restores
	nm=$1; R=$2; [ -n "$CSD_DEBUG" ] && { shot; cp $T/s.png $CSD_DEBUG/real$3.png; cl $R > $CSD_DEBUG/real$3.txt; }; X=$(cv $R 4); Y=$(cv $R 5); W=$(cv $R 6); [ -n "$(csd $R)" ] || echo "(not csd) "
	ok "$3. $nm: no title bar of Rudolf's (its own, drawn by $nm), its frame's edges in _NET_FRAME_EXTENTS" "$([ "$(csd $R | cut -d' ' -f1)" = 1 ] && [ "$(ext $R)" = "1 1 1 1" ] && echo 1)" "$(cl $R) $(csd $R) $(ext $R)"
	xdotool mousemove $((X+W/2+$4)) $((Y+$5)) mousedown 1; timeout 0.3 tail -f /dev/null; k=1; while [ $k -le 6 ]; do xdotool mousemove $((X+W/2+$4+k*15)) $((Y+$5+k*10)); timeout 0.1 tail -f /dev/null; k=$((k+1)); done; xdotool mouseup 1; timeout 0.6 tail -f /dev/null
	# (GTK asks from where the button went down; Chromium once the pointer has gone past its drag threshold, the first step here)
	ok "$3b. $nm: dragging its own bar moves it (it asks _NET_WM_MOVERESIZE)" "$(dx=$(($(cv $R 4)-X)); dy=$(($(cv $R 5)-Y)); [ "$dx $dy" = "90 60" ] || [ "$dx $dy" = "75 50" ] && echo 1)" "$(cl $R) from $X $Y; $(tail -2 $6)"
	X=$(cv $R 4); Y=$(cv $R 5); G0=$(geo $R); xdotool mousemove $((X+W/2+$4)) $((Y+$5)) click --repeat 2 --delay 90 1; w8 '[ "$(flag $R max)" = 1 ]' 12; timeout 0.8 tail -f /dev/null
	ok "$3c. $nm: a double-click on its bar maximizes it (Rudolf's maximize: _NET_WM_STATE)" "$([ "$(flag $R max)" = 1 ] && [ "$(geo $R | cut -d' ' -f1-3)" = "0 0 1274" ] && echo 1)" "$(cl $R)"
	xdotool mousemove $((637+$4)) $((0+$5)) click --repeat 2 --delay 90 1; w8 '[ "$(flag $R max)" = 0 ]' 12; timeout 0.8 tail -f /dev/null
	# (its place; its size is what it asks again after: gtk3-widget-factory's smallest is wider than this screen)
	ok "$3d. $nm: ...and again restores it" "$([ "$(flag $R max)" = 0 ] && [ "$(geo $R | cut -d' ' -f1-2)" = "$(echo $G0 | cut -d' ' -f1-2)" ] && echo 1)" "$(cl $R) want $G0"; }
if command -v gtk3-widget-factory >/dev/null; then R=$(real gtk3 $T/gtk3.log gtk3-widget-factory); G3=$(cat $T/gtk3.pid)
	[ -n "$R" ] && rtest "GTK 3 (gtk3-widget-factory $(pkg-config --modversion gtk+-3.0 2>/dev/null))" $R 39 ${CSD_G3X:--300} 18 $T/gtk3.log || ok "39. GTK 3: a window" 0 "$(tail -5 $T/gtk3.log)"; kill $G3; w8 '[ -z "$(cl $R)" ]' 20
else echo "SKIP 39. GTK 3: gtk3-widget-factory is not installed"; fi
if command -v gtk4-widget-factory >/dev/null; then R=$(real gtk4 $T/gtk4.log gtk4-widget-factory); G4=$(cat $T/gtk4.pid)
	[ -n "$R" ] && rtest "GTK 4 (gtk4-widget-factory $(pkg-config --modversion gtk4 2>/dev/null))" $R 40 ${CSD_G4X:--300} 18 $T/gtk4.log || ok "40. GTK 4: a window" 0 "$(tail -5 $T/gtk4.log)"; kill $G4; w8 '[ -z "$(cl $R)" ]' 20
else echo "SKIP 40. GTK 4: gtk4-widget-factory is not installed"; fi
CHR=${CSD_CHROME:-$(ls -d /opt/pw-browsers/chromium-*/chrome-linux64/chrome $HOME/.cache/ms-playwright/chromium-*/chrome-linux*/chrome 2>/dev/null | head -1)}
# (Chromium leaves its own frame off under a window manager it does not know: "Use system title bar and borders" turned off here, as an
# Electron app's frame: false is; Part 15 item 1 is the other way round)
if [ -x "$CHR" ]; then mkdir -p $T/cprof/Default; echo '{"browser":{"custom_chrome_frame":true}}' > $T/cprof/Default/Preferences; R=$(real chromium $T/chromium.log $CHR --user-data-dir=$T/cprof --no-first-run --no-default-browser-check --disable-gpu --no-sandbox --window-size=700,500 --window-position=60,60 "data:text/html,<title>Electron-style</title><body>own title bar</body>"); CR=$(cat $T/chromium.pid)
	[ -n "$R" ] && rtest "Chromium ($($CHR --version 2>/dev/null | awk '{print $2}'), Electron's frame)" $R 41 150 14 $T/chromium.log || ok "41. Chromium: a window" 0 "$(tail -5 $T/chromium.log)"; kill $CR; w8 '[ -z "$(cl $R)" ]' 20
else echo "SKIP 41. Chromium: none on this host"; fi
kill $NP 2>/dev/null; w8 '! kill -0 $NP 2>/dev/null' 20
# ---- Standard (rudolf-display, the window manager when the desktop is the page): the same app shows its own bar alone there too
L=/usr/lib/x86_64-linux-gnu
if gcc -O2 -o $T/rds $SYS/rudolf-display.c -I$SYS -lX11 $L/libXtst.so.6 $L/libXfixes.so.3 $L/libXrandr.so.2 -lm 2>$T/cc2; then
	echo "bottom 48" > /run/rudolf/strut; echo desktop > /run/rudolf/wm; rm -f /run/rudolf/wm-native
	$T/rds --input > $T/rds.log 2>&1 & RDS=$!; timeout 1 tail -f /dev/null
	$XC csd 500 360 > $T/s1.log & A1=$!; IDS=$(idof $T/s1.log); w8 'grep -q "^geom 0 0 1280 752" $T/s1.log' 30; timeout 0.5 tail -f /dev/null; shot
	ok "42. Standard: the same app has its own bar alone (no title bar of Rudolf's above it: its bar at the window's top)" "$(grep -q '^geom 0 0 1280 752' $T/s1.log && close "$(px 300 3)" "31 41 55" 3 && echo 1)" "$(lastgeom $T/s1.log) $(px 300 3)"
	ok "43. ...as on Native (7): one title bar, the app's own, on both" "$([ "$T7" = 1 ] && echo 1)" "Native's 7: $T7"
else echo "SKIP 42. Standard: rudolf-display did not build ($(head -2 $T/cc2))"; fi
echo "$((N-F))/$N passed"; [ $F = 0 ]
