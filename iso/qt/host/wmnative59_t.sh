#!/bin/sh
# 1.5.9 (plan Part 12, step 4): while the native desktop engine is the window manager (/run/rudolf/wm-native, made by
# the session), rudolf-display leaves the windows alone: a new app window is neither moved, resized nor shown in place
# of the desktop; without the file it manages them as before. Needs Xvfb, gcc, the X headers and root (a private
# /run/rudolf, in a mount namespace).   sh wmnative59_t.sh
[ "$(id -u)" = 0 ] && [ -z "$WMN_INNER" ] && exec env WMN_INNER=1 unshare -m sh -c 'mount -t tmpfs none /run && mkdir -p /run/rudolf && exec sh "$0"' "$0"
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; T=$(mktemp -d)
L=/usr/lib/x86_64-linux-gnu
gcc -O2 $XCFLAGS -o $T/rd $SYS/rudolf-display.c -I$SYS 2>$T/cc.log -lX11 $L/libXtst.so.6 $L/libXfixes.so.3 $L/libXrandr.so.2 -lm || { cat $T/cc.log; echo "FAIL build"; exit 1; }
gcc -O2 -o $T/xc $HERE/fixtures/xwmclient.c -lX11 || { echo "FAIL build client"; exit 1; }
export DISPLAY=:76
Xvfb :76 -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
trap 'kill $RD $AP $XV 2>/dev/null; rm -rf $T' EXIT
for i in $(seq 50); do $T/xc getfocus >/dev/null 2>&1 && break; sleep 0.1; done
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
w8() { for i in $(seq 30); do eval "$1" && return 0; sleep 0.1; done; return 1; }
echo "bottom 48" > /run/rudolf/strut; echo desktop > /run/rudolf/wm; touch /run/rudolf/wm-native
$T/rd --input > $T/rd.log 2>&1 & RD=$!; sleep 1
$T/xc app > $T/app.log & AP=$!
w8 'grep -q "^id" $T/app.log'; sleep 2
ok "1. with the native window manager: a new window is left where it is (not fitted above the taskbar)" "$( ! grep -q '^geom 0 0 1280 752' $T/app.log && echo 1)" "$(cat $T/app.log)"
ok "2. ...and the desktop stays shown (rudolf-display does not switch to it)" "$([ "$(cat /run/rudolf/wm)" = desktop ] && echo 1)" "$(cat /run/rudolf/wm)"
rm -f /run/rudolf/wm-native
w8 'grep -q "^geom 0 0 1280 752" $T/app.log'; ok "3. without it, rudolf-display manages windows as before" "$([ $? = 0 ] && echo 1)" "$(cat $T/app.log)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
