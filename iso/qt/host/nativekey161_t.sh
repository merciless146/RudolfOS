#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 18): Native's sign-in and lock screen with a security key (rudolf-native.c sn_key_step), on
# a virtual screen (Xvfb) with the stand-in rudolfd (fixtures/native_rd.py) answering as rudolfd-fido.inc does when the
# account has keys ($D/fidokey): the password, then 401 "need":"key"; rudolf-native says to touch the key and asks again
# with "k" at once (the password kept, not typed again); a wrong key ($D/fidotouch not "ok"): the reason shown, still the
# sign-in screen; Enter asks again with the key; the right key: signed in. The same at the lock screen.
#   sh nativekey161_t.sh [RUDOLF-NATIVE]   (Xvfb, xdotool, gcc, WebKitGTK's headers, node, python3)
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/nativekey161.XXXXXX); PORT=${NATIVEKEY_PORT:-8614}; DSP=${NATIVEKEY_DSP:-:83}
command -v Xvfb >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs Xvfb and xdotool"; exit 0; }
BIN=$1
if [ -z "$BIN" ]; then BIN=$T/rudolf-native
	gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $BIN $HERE/../../sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
fi
D=$T/d; mkdir -p $D; node $HERE/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)")
printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","b2":"#000080","tb":"bottom","accent":"#2563eb"},"pins":[],"notes":0,"wallpaper":false}' > $D/settings.json
touch $D/apps-fixed $D/fidokey; echo wrong > $D/fidotouch
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
calls() { tr '\n' ' ' < $D/fidocalls 2>/dev/null; }
w8 '[ "$(st signin)" = 1 ]' 120; ok "0. the sign-in screen" "$([ "$(st signin)" = 1 ] && echo 1)" "$(tail -5 $T/native.log)"
LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return
w8 '[ "$(calls)" = "p k " ] && [ -n "$(st signmsg)" ]' 60
ok "1. the password right, \"need\":\"key\": asked again with the key at once (the password kept), a wrong key: its reason shown, still the sign-in screen" \
   "$([ "$(calls)" = "p k " ] && [ "$(st signin)" = 1 ] && [ "$(st user)" = - ] && st signmsg | grep -q "not one of this account" && echo 1)" "$(calls) | $(st signmsg) | $(st signin) $(st user)"
echo ok > $D/fidotouch; xdotool key Return
w8 '[ "$(st user)" = Ann ] && [ "$(st signin)" = 0 ]' 60
ok "2. Enter: asked again with the key (not the password typed again): the right key, signed in" "$([ "$(st user)" = Ann ] && [ "$(calls)" = "p k k " ] && echo 1)" "$(calls) | $(head -4 $D/state)"
xdotool key super+l; w8 '[ "$(st locked)" = 1 ]' 20
: > $D/fidocalls; LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return
w8 '[ "$(st locked)" = 0 ]' 60
ok "3. the lock screen: the password, then the key (asked at once): unlocked" "$([ "$(st locked)" = 0 ] && [ "$(calls)" = "p k " ] && echo 1)" "$(calls) | locked $(st locked) | $(st signmsg)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
