#!/bin/sh
# 1.6.3 (docs/plan-1.6.3.md Part 2, step 3): Native's lock screen with a finger (rudolf-native.c fp_*), on a virtual screen
# (Xvfb) with the stand-in rudolfd (fixtures/native_rd.py) answering fp/status and desktop/unlock {"f":true} as
# rudolfd-fprint.inc does ($D/fpready: a finger is taken now; $D/fpanswer: the next finger, ok, no, code or tries):
# "Or put your finger on the reader" shown; a finger not recognised said and the reader asked again; the right finger
# unlocks; an account with an authenticator app: after the finger the code is asked (the finger's one-time answer sent
# with it, a wrong code asked again); five tries: rudolfd's reason shown, the password still unlocks and gives the reader
# up; no finger offered when rudolfd says none is taken.
#   sh nativefp163_t.sh [RUDOLF-NATIVE]   (Xvfb, xdotool, gcc, WebKitGTK's headers, node, python3)
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/nativefp163.XXXXXX); PORT=${NATIVEFP_PORT:-8731}; DSP=${NATIVEFP_DSP:-:84}
command -v Xvfb >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs Xvfb and xdotool"; exit 0; }
BIN=$1
if [ -z "$BIN" ]; then BIN=$T/rudolf-native
	gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $BIN $HERE/../../sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
fi
D=$T/d; mkdir -p $D; node $HERE/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)")
printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","b2":"#000080","tb":"bottom","accent":"#2563eb"},"pins":[],"notes":0,"wallpaper":false}' > $D/settings.json
touch $D/apps-fixed $D/fpready; : > $D/fpanswer; : > $D/fpcalls
python3 $HERE/fixtures/native_rd.py $PORT $D > $T/rd.log 2>&1 & RDP=$!
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=$DSP LIBGL_ALWAYS_SOFTWARE=1
trap 'kill $NP $RDP $XV 2>/dev/null; rm -rf $T' EXIT
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
w8 'xdotool getmouselocation >/dev/null 2>&1' 40
(cd $D && HOME=$D RUDOLF_NATIVE_STATE=$D/state exec $BIN --size=1280x800 --draw=soft --base=http://127.0.0.1:$PORT > $T/native.log 2>&1) & NP=$!
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 700)"; F=$((F+1)); fi; }
st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
calls() { tr '\n' ' ' < $D/fpcalls 2>/dev/null; }
w8 '[ "$(st signin)" = 1 ]' 120; LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return
w8 '[ "$(st user)" = Ann ] && [ "$(st signin)" = 0 ]' 60
ok "0. signed in with the password (no finger at the sign-in screen)" "$([ "$(st user)" = Ann ] && [ ! -s $D/fpcalls ] && echo 1)" "$(calls) | $(head -4 $D/state)"
xdotool key super+l; w8 '[ "$(st locked)" = 1 ] && [ "$(st signfp | cut -c1)" = 1 ]' 40
ok "1. the lock screen: \"Or put your finger on the reader\", rudolfd waiting for one" "$([ "$(st locked)" = 1 ] && [ "$(st signfp)" = "1 -" ] && calls | grep -q '^f ' && echo 1)" "$(st signfp) | $(calls)"
n0=$(grep -c '^f$' $D/fpcalls); echo no > $D/fpanswer; w8 '[ $(grep -c "^f$" $D/fpcalls) -gt $n0 ]' 40
ok "2. a finger not recognised: said, the reader asked again, still locked" "$([ "$(st locked)" = 1 ] && st signfp | grep -q 'not recognised: try again (4 tries left)' && echo 1)" "$(st signfp) | $(calls)"
echo ok > $D/fpanswer; w8 '[ "$(st locked)" = 0 ]' 40
ok "3. the right finger: unlocked" "$([ "$(st locked)" = 0 ] && [ "$(st signfp | cut -c1)" = 0 ] && echo 1)" "$(st locked) | $(st signfp)"
: > $D/fpcalls; xdotool key super+l; w8 '[ "$(st locked)" = 1 ] && calls | grep -q "^f "' 40; echo code > $D/fpanswer
w8 '[ "$(st signfp)" = "0 code" ]' 40
ok "4. an authenticator app: after the finger the code is asked (the field takes it), still locked" "$([ "$(st locked)" = 1 ] && st signmsg | grep -q '6-digit code' && echo 1)" "$(st signfp) | $(st signmsg)"
xdotool type --delay 40 111111; xdotool key Return; w8 'calls | grep -q "ft 111111 $"' 40; timeout 1 tail -f /dev/null
ok "5. a wrong code (sent with the finger's answer): asked again, still locked, the finger not asked again" "$([ "$(st locked)" = 1 ] && st signmsg | grep -q 'not right' && calls | grep -q "^\(f \)*ft 111111 $" && echo 1)" "$(calls) | $(st signmsg)"
xdotool type --delay 40 123456; xdotool key Return; w8 '[ "$(st locked)" = 0 ]' 40
ok "6. the right code: unlocked" "$([ "$(st locked)" = 0 ] && calls | grep -q "^\(f \)*ft 111111 ft 123456 $" && echo 1)" "$(calls)"
: > $D/fpcalls; xdotool key super+l; w8 '[ "$(st locked)" = 1 ] && calls | grep -q "^f "' 40; echo tries > $D/fpanswer
w8 'st signfp | grep -q "Too many"' 40
c7=$(calls); timeout 2 tail -f /dev/null
ok "7. five tries: rudolfd's reason shown, the reader not asked again" "$(st signfp | grep -q '^0 .*Too many fingers' && [ "$(calls)" = "$c7" ] && echo 1)" "$(st signfp) | $(calls)"
LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return; w8 '[ "$(st locked)" = 0 ]' 40
ok "8. the password still unlocks" "$([ "$(st locked)" = 0 ] && echo 1)" "$(st locked) $(st signmsg)"
: > $D/fpcalls; rm -f $D/fpready; xdotool key super+l; w8 '[ "$(st locked)" = 1 ]' 40; timeout 2 tail -f /dev/null
ok "9. rudolfd says no finger is taken now (after a restart): none offered, the reader not asked" "$([ "$(st signfp)" = "0 -" ] && [ ! -s $D/fpcalls ] && echo 1)" "$(st signfp) | $(calls)"
touch $D/fpready; LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return; w8 '[ "$(st locked)" = 0 ]' 40
: > $D/fpcalls; xdotool key super+l; w8 'calls | grep -q "^f "' 40
LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return; w8 '[ "$(st locked)" = 0 ]' 40; w8 'grep -q cancel $D/fpcalls' 20
ok "10. the password while a finger is awaited: unlocked, and the reader given up (fp/cancel)" "$([ "$(st locked)" = 0 ] && grep -q cancel $D/fpcalls && echo 1)" "$(calls)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
