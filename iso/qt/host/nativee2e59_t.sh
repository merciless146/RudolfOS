#!/bin/sh
# 1.5.9 (plan Part 12): the native desktop engine end to end, with the real rudolfd (built here) and the real desktop
# page, in a private mount namespace: the page's storage as rudolfd's lsync copy keeps it (accounts made as the page
# makes them, fixtures/deskls.js), rudolf-native signs in through rudolfd (its own PBKDF2 and AES-GCM), and an app
# window signs in from rudolfd's session with the keys rudolfd made (its storage restored from the lsync copy, as
# rudolfd's page does) and shows its title. Needs root, Xvfb, xdotool, curl, node, gcc, WebKitGTK's headers; port 8080
# free.
#   sh nativee2e59_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
command -v Xvfb >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs Xvfb and xdotool"; exit 0; }
[ -z "$E2E_INNER" ] && exec env E2E_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; T=$(mktemp -d /tmp/nate2e.XXXXXX)
mount -t tmpfs none /run; mkdir -p /run/rudolf /usr/share/rudolf/www /home/rudolf /persist
mount --bind $HERE/../../www /usr/share/rudolf/www; mount -t tmpfs none /home/rudolf; mount -t tmpfs none /persist
gcc -O2 -w -o $T/rudolfd $SYS/rudolfd.c -lz || { echo "FAIL build rudolfd"; exit 1; }
gcc -O2 -w -o $T/rudolf-native $SYS/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm || { echo "FAIL build rudolf-native"; exit 1; }
node $HERE/fixtures/deskls.js $T/ls.json $T/keys.json || { echo "FAIL node"; exit 1; }
cp $T/ls.json /home/rudolf/.rudolf-ls.json
PW=$(node -e "console.log(require('$T/keys.json').Ann.pw)")
$T/rudolfd > $T/rd.log 2>&1 & RDP=$!
Xvfb :75 -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=:75 LIBGL_ALWAYS_SOFTWARE=1
trap 'kill $NP $RDP $XV 2>/dev/null; umount /usr/share/rudolf/www /home/rudolf /persist 2>/dev/null; rm -rf $T' EXIT
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
w8 'curl -s http://127.0.0.1:8080/api/ping >/dev/null 2>&1' 40
mkdir -p $T/h; (cd $T/h && HOME=$T/h RUDOLF_NATIVE_STATE=$T/state exec $T/rudolf-native --rudolf-kiosk --size=1280x800 --draw=soft > $T/native.log 2>&1) & NP=$!
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 600)"; F=$((F+1)); fi; }
st() { grep "^$1 " $T/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
w8 '[ "$(st signin)" = 1 ]' 120; ok "1. rudolf-native got rudolfd's token and shows the sign-in screen" "$([ "$(st signin)" = 1 ] && echo 1)" "$(tail -5 $T/native.log)"
w8 '[ -e /run/rudolf/heartbeat ]' 60; ok "2. ...and its heartbeat reached rudolfd (the session's trial ends)" "$([ -e /run/rudolf/heartbeat ] && echo 1)"
# (the last account used, Bob, is filled in: Ann typed over it)
xdotool key shift+Tab ctrl+a; xdotool type --delay 20 Ann; xdotool key Tab; LC_ALL=C.UTF-8 xdotool type --delay 30 "$PW"; xdotool key Return
w8 '[ "$(st user)" = Ann ]' 120; ok "3. signed in with Ann's password, checked by rudolfd (PBKDF2 in C)" "$([ "$(st user)" = Ann ] && [ -s /run/rudolf/native-session ] && echo 1)" "$(head -6 $T/state; st signmsg)"
ok "4. ...the taskbar has her pins and her settings (desktop/settings)" "$(grep '^hit pin' $T/state | awk '{printf "%s ", $3}' | grep -q '^notepad calc $' && [ "$(st bar | cut -d' ' -f3)" = 0 ] && echo 1)" "$(grep '^hit pin\|^bar' $T/state)"
P=$(grep '^hit pin calc ' $T/state | awk '{print $4+int($6/2), $5+int($7/2)}'); xdotool mousemove $P click 1
w8 'grep -q "^app calc .* Calculator$" $T/state' 160
ok "5. the Calculator's window signs in from rudolfd's session (the keys rudolfd made) and shows its title" "$(grep -q '^app calc .* Calculator$' $T/state && echo 1)" "$(grep '^app\|^client' $T/state; grep -i 'desktop/session\|error' $T/rd.log | tail -3)"
ok "6. ...framed by the window manager" "$(grep -q '^client .* app=calc Calculator$' $T/state && echo 1)" "$(grep '^client' $T/state)"
kill $NP 2>/dev/null
echo "$((N-F))/$N passed"; [ $F = 0 ]
