#!/bin/sh
# 1.5.2: rudolf-display's window handling against a Windows game in Wine (a stand-in client, fixtures/xwmclient.c):
# a window shown gets WM_TAKE_FOCUS (Wine only takes keys for DirectInput then) and WM_STATE, and a window asking for
# full screen (_NET_WM_STATE) covers the whole screen, taskbar too. Needs Xvfb, gcc, the X headers and root (a
# private /run/rudolf, in a mount namespace).   sh wmfocus_t.sh   (XCFLAGS: more compiler flags, e.g. -I headers)
[ "$(id -u)" = 0 ] && [ -z "$WMT_INNER" ] && exec env WMT_INNER=1 unshare -m sh -c 'mount -t tmpfs none /run && mkdir -p /run/rudolf && exec sh "$0"' "$0"
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; T=$(mktemp -d)
L=/usr/lib/x86_64-linux-gnu
gcc -O2 $XCFLAGS -o $T/rd $SYS/rudolf-display.c -I$SYS 2>$T/cc.log -lX11 $L/libXtst.so.6 $L/libXfixes.so.3 $L/libXrandr.so.2 -lm || { cat $T/cc.log; echo "FAIL build"; exit 1; }
gcc -O2 -o $T/xc $HERE/fixtures/xwmclient.c -lX11 || { echo "FAIL build client"; exit 1; }
export DISPLAY=:77
Xvfb :77 -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
trap 'kill $RD $AP $DK $XV 2>/dev/null; rm -rf $T' EXIT
for i in $(seq 50); do $T/xc getfocus >/dev/null 2>&1 && break; sleep 0.1; done
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
w8() { for i in $(seq 40); do eval "$1" && return 0; sleep 0.1; done; return 1; }
echo "bottom 48" > /run/rudolf/strut; echo desktop > /run/rudolf/wm
$T/xc desktop > $T/desk.log & DK=$!; sleep 0.3
$T/rd --input > $T/rd.log 2>&1 & RD=$!; sleep 1
$T/xc app > $T/app.log & AP=$!
w8 'grep -q "^id" $T/app.log'; ID=$(sed -n 's/^id //p' $T/app.log | head -1); DID=$(sed -n 's/^id //p' $T/desk.log | head -1)
w8 '[ "$(cat /run/rudolf/wm)" = "win ${ID#0x}" ]'; ok "a new app window is shown" $([ $? = 0 ] && echo 1 || echo 0) "$(cat /run/rudolf/wm) want win ${ID#0x}"
w8 'grep -q "^geom 0 0 1280 752" $T/app.log'; ok "...above the taskbar (1280x752)" $([ $? = 0 ] && echo 1 || echo 0) "$(grep geom $T/app.log | tail -1)"
w8 'grep -q "^take_focus [1-9]" $T/app.log'; ok "...and gets WM_TAKE_FOCUS with a server time" $([ $? = 0 ] && echo 1 || echo 0) "$(cat $T/app.log)"
ok "...and the keyboard" $([ "$($T/xc getfocus)" = "$ID" ] && echo 1 || echo 0) "$($T/xc getfocus)"
ok "...and WM_STATE (NormalState)" $([ "$($T/xc wmstate $ID)" = 1 ] && echo 1 || echo 0) "$($T/xc wmstate $ID)"
n1=$(grep -c take_focus $T/app.log)
echo desktop > /run/rudolf/wm; w8 '[ "$($T/xc getfocus)" = "$DID" ]'; ok "Super+Esc (the desktop): the desktop has the keyboard" $([ $? = 0 ] && echo 1 || echo 0) "$($T/xc getfocus)"
echo "win ${ID#0x}" > /run/rudolf/wm; w8 '[ "$(grep -c take_focus $T/app.log)" -gt $n1 ]'; ok "back to the game: WM_TAKE_FOCUS again" $([ $? = 0 ] && echo 1 || echo 0) "$(cat $T/app.log)"
$T/xc full $ID 1; w8 'grep -q "^geom 0 0 1280 800" $T/app.log'; ok "it asks for full screen: it covers the whole screen, taskbar too" $([ $? = 0 ] && echo 1 || echo 0) "$(grep geom $T/app.log | tail -1)"
$T/xc full $ID 0; w8 '[ "$(grep geom $T/app.log | tail -1)" = "geom 0 0 1280 752" ]'; ok "full screen off: above the taskbar again" $([ $? = 0 ] && echo 1 || echo 0) "$(grep geom $T/app.log | tail -1)"
$T/xc full $ID 2; w8 'grep "^geom" $T/app.log | tail -1 | grep -q "1280 800"'; ok "toggle: full screen again" $([ $? = 0 ] && echo 1 || echo 0) "$(grep geom $T/app.log | tail -1)"
$T/xc full $ID 0; w8 '[ "$(grep geom $T/app.log | tail -1)" = "geom 0 0 1280 752" ]'
$T/xc resize $ID 1280 800; sleep 1; w8 '[ "$(grep geom $T/app.log | tail -1)" = "geom 0 0 1280 752" ]'; ok "a framed window making itself screen-sized is put back above the taskbar" $([ $? = 0 ] && [ "$($T/xc state $ID)" = - ] && echo 1 || echo 0) "$(grep geom $T/app.log | tail -2) $($T/xc state $ID)"
kill $AP; w8 '[ "$(cat /run/rudolf/wm)" = desktop ]'; ok "the game closes: the desktop" $([ $? = 0 ] && echo 1 || echo 0) "$(cat /run/rudolf/wm)"
# a Windows game's "full screen" in Wine: a borderless window that sizes itself to the screen, no _NET_WM_STATE
$T/xc borderless 1280 800 > $T/bl.log & AP=$!
w8 'grep -q "^id" $T/bl.log'; BID=$(sed -n 's/^id //p' $T/bl.log | head -1)
w8 '[ "$($T/xc state $BID)" = fullscreen ]'; ok "a borderless window that makes itself screen-sized is marked full screen" $([ $? = 0 ] && echo 1 || echo 0) "$($T/xc state $BID)"
sleep 1; ok "...and keeps the whole screen (1280x800)" $([ "$(grep geom $T/bl.log | tail -1)" = "geom 0 0 1280 800" ] && echo 1 || echo 0) "$(grep geom $T/bl.log | tail -3)"
ok "...and the keyboard" $([ "$($T/xc getfocus)" = "$BID" ] && echo 1 || echo 0) "$($T/xc getfocus)"
$T/xc resize $BID 800 600; w8 '[ "$(grep geom $T/bl.log | tail -1)" = "geom 0 0 1280 752" ]'; ok "it switches to windowed (800x600): above the taskbar again, mark gone" $([ $? = 0 ] && [ "$($T/xc state $BID)" = - ] && echo 1 || echo 0) "$(grep geom $T/bl.log | tail -1) $($T/xc state $BID)"
$T/xc resize $BID 1280 800; w8 '[ "$($T/xc state $BID)" = fullscreen ]'; ok "...and back to full screen" $([ $? = 0 ] && echo 1 || echo 0) "$(grep geom $T/bl.log | tail -1) $($T/xc state $BID)"
kill $AP
ok "rudolf-display kept running" $(kill -0 $RD 2>/dev/null && echo 1 || echo 0) "$(cat $T/rd.log)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
