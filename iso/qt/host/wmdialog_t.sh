#!/bin/sh
# 1.5.6: rudolf-display and dialogs (WM_TRANSIENT_FOR). Found on the Vivobook: Starport's installer (Inno Setup in
# Wine) showed its owner window, and its wizard, a dialog of it, stayed behind (the owner and the desktop were raised
# over it) where nobody could answer it. Now a shown window's dialogs stay above it and have the keyboard; a dialog
# whose owner is never shown is a window of its own; "kill <id>" in wmcmd ends a program. Needs Xvfb, gcc, the X
# headers and root (a private /run/rudolf).   sh wmdialog_t.sh
[ "$(id -u)" = 0 ] && [ -z "$WMT_INNER" ] && exec env WMT_INNER=1 unshare -m sh -c 'mount -t tmpfs none /run && mkdir -p /run/rudolf && exec sh "$0"' "$0"
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; T=$(mktemp -d)
L=/usr/lib/x86_64-linux-gnu
gcc -O2 $XCFLAGS -o $T/rd $SYS/rudolf-display.c -I$SYS 2>$T/cc.log -lX11 $L/libXtst.so.6 $L/libXfixes.so.3 $L/libXrandr.so.2 -lm || { cat $T/cc.log; echo "FAIL build"; exit 1; }
gcc -O2 -o $T/xc $HERE/fixtures/xwmclient.c -lX11 || { echo "FAIL build client"; exit 1; }
export DISPLAY=:78
Xvfb :78 -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
trap 'kill $RD $IN $OR $DK $XV 2>/dev/null; rm -rf $T' EXIT
for i in $(seq 50); do $T/xc getfocus >/dev/null 2>&1 && break; sleep 0.1; done
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
w8() { for i in $(seq 40); do eval "$1" && return 0; sleep 0.1; done; return 1; }
top() { $T/xc stack | tail -1; }
echo "bottom 48" > /run/rudolf/strut; echo desktop > /run/rudolf/wm
$T/xc desktop > $T/desk.log & DK=$!; sleep 0.3
$T/rd --input > $T/rd.log 2>&1 & RD=$!; sleep 1
DID=$(sed -n 's/^id //p' $T/desk.log | head -1)
# Inno Setup: the owner, then its wizard, then the owner raises itself
$T/xc inno > $T/inno.log & IN=$!
w8 'grep -q "^dlg" $T/inno.log'; OID=$(sed -n 's/^id //p' $T/inno.log); GID=$(sed -n 's/^dlg //p' $T/inno.log)
sleep 1.2
ok "the installer's window is shown" "$([ "$(cat /run/rudolf/wm)" = "win ${OID#0x}" ] && echo 1)" "$(cat /run/rudolf/wm) want ${OID#0x}"
w8 '[ "$(top)" = "$GID" ]'; ok "...and its wizard (a dialog of it) is on top, also after the owner raised itself" "$([ $? = 0 ] && echo 1)" "top $(top), wizard $GID, owner $OID"
w8 '[ "$($T/xc getfocus)" = "$GID" ]'; ok "...and the wizard has the keyboard" "$([ $? = 0 ] && echo 1)" "$($T/xc getfocus)"
ok "the wizard is not a separate entry in the window list (the taskbar)" "$(grep -q "^${GID#0x}	" /run/rudolf/windows || echo 1)" "$(cat /run/rudolf/windows)"
echo desktop > /run/rudolf/wm; w8 '[ "$(top)" = "$DID" ]'; ok "Super+Esc: the desktop covers both" "$([ $? = 0 ] && echo 1)" "$(top)"
echo "win ${OID#0x}" > /run/rudolf/wm; w8 '[ "$(top)" = "$GID" ]'; ok "back to the installer: the wizard on top again" "$([ $? = 0 ] && echo 1)" "$(top)"
# a dialog whose owner is never shown
$T/xc orphan > $T/or.log & OR=$!
w8 'grep -q "^dlg" $T/or.log'; RID=$(sed -n 's/^dlg //p' $T/or.log)
w8 '[ "$(cat /run/rudolf/wm)" = "win ${RID#0x}" ]'; ok "a dialog whose owner is hidden is shown as a window of its own" "$([ $? = 0 ] && echo 1)" "$(cat /run/rudolf/wm) want ${RID#0x}"
w8 '[ "$(top)" = "$RID" ]'; ok "...on top" "$([ $? = 0 ] && echo 1)" "$(top)"
ok "...and in the window list" "$(grep -q "^${RID#0x}	" /run/rudolf/windows && echo 1)" "$(cat /run/rudolf/windows)"
echo "kill ${RID#0x}" > /run/rudolf/wmcmd; w8 '! kill -0 $OR 2>/dev/null'; ok "wmcmd kill: the program behind it ends" "$([ $? = 0 ] && echo 1)"
w8 '[ "$(cat /run/rudolf/wm)" != "win ${RID#0x}" ]'; ok "...and its window is gone from the screen" "$([ $? = 0 ] && echo 1)" "$(cat /run/rudolf/wm)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
