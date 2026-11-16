#!/bin/sh
# 1.6.0 (the owner's Vivobook, 2026-11-16: Sober's sign-in spun for ever on the Native desktop): the Rudolf portal's requests on
# Native. rudolf-native on a virtual screen (Xvfb) with the stand-in rudolfd (fixtures/native_rd.py, its portal/poll and
# portal/answer from DIR/portal) serving this tree's page; requests written as rudolf-portal writes them:
#   1. after signing in the shell's portal page starts unseen (do-portal), before any request
#   2. the keyring's "new keyring" prompt with no app window open: on the screen, above everything, within 2 s; typed into as a
#      person would (the keyboard is the page's), answered once with the password; the page is out of sight again
#   3. with two app windows open (Notepad, Calculator): a file Save dialog, drawn once, by the portal's page only (no app
#      window draws it), above them; cancelled: answered once
#   4. a notification: the shell's toast; a web link: Oracle's window at that address; each answered once
#   5. a request withdrawn by its app (rudolf-portal removes it): its dialog closes
#   6. no app window collects requests any more (their pages' portal/poll: none)
# Needs Xvfb, xdotool, ImageMagick, node, python3, gcc and WebKitGTK's headers.   sh nportal160_t.sh [RUDOLF-NATIVE]
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/nportal160.XXXXXX); PORT=${NPORTAL_PORT:-8611}; DSP=${NPORTAL_DSP:-:83}
command -v Xvfb >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs Xvfb and xdotool"; exit 0; }
BIN=$1
if [ -z "$BIN" ]; then BIN=$T/rudolf-native
	gcc -O2 -Wno-deprecated-declarations -o $BIN $HERE/../../sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
fi
D=$T/d; mkdir -p $D/portal/req $D/portal/resp; node $HERE/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)")
printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","b2":"#000080","tb":"bottom","accent":"#2563eb","h24":true,"df":"ymd"},"pins":["notepad","calc","settings"],"notes":0,"wallpaper":false}' > $D/settings.json
python3 $HERE/fixtures/native_rd.py $PORT $D > $T/rd.log 2>&1 & RDP=$!
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=$DSP LIBGL_ALWAYS_SOFTWARE=1
mkfifo $T/teval
trap 'kill $NP $RDP $XV 2>/dev/null; pkill -f "$T/" 2>/dev/null; rm -rf $T' EXIT
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
w8 'xdpyinfo >/dev/null 2>&1 || xdotool getmouselocation >/dev/null 2>&1' 40
(cd $D && HOME=$D RUDOLF_NATIVE_STATE=$D/state RUDOLF_NATIVE_TEST_EVAL=$T/teval exec $BIN --size=1280x800 --draw=soft --base=http://127.0.0.1:$PORT > $T/native.log 2>&1) & NP=$!
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
hit() { grep "^hit $1 $2 " $D/state | head -1 | awk '{print $4+int($6/2), $5+int($7/2)}'; }
# (a page's answer: "KEY JS" into the shell's test channel, its answer from teval.out)
ev() { : > $T/teval.out; printf '%s %s\n' "$1" "$2" > $T/teval; w8 "[ -s $T/teval.out ]" 40; sed -n "s/^$1 //p" $T/teval.out | tail -1; }
now() { date +%s%N | cut -c1-13; }
req() { printf '%s' "$2" > $D/portal/req/$1.json.t && mv $D/portal/req/$1.json.t $D/portal/req/$1.json; }
answers() { cat $D/portal/answers 2>/dev/null | grep -c "^$1 "; }
polls() { cat $D/calls 2>/dev/null | grep -c 'portal/poll'; }
# ---- sign in
w8 '[ "$(st signin)" = 1 ]' 160 || { echo "FAIL no sign-in screen: $(tail -5 $T/native.log)"; exit 1; }
# (the sign-in screen may take its keys a moment after it shows, on a slower computer: typed again until signed in)
for a in 1 2 3 4; do timeout 3 tail -f /dev/null; xdotool key ctrl+a BackSpace; LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return
	w8 '[ "$(st user)" = Ann ] && [ "$(st signin)" = 0 ]' 120 && break; echo "(not signed in yet: $(st signmsg))"; done
w8 '[ "$(st portal | cut -d" " -f1)" = ready ]' 400
ok "1. after signing in the shell's portal page is ready, unseen, with no request yet" "$([ "$(st portal | cut -d' ' -f1)" = ready ] && grep -q '^page do-portal$' $D/state && ! grep -q 'app=do-portal' $D/state && echo 1)" "$(st portal) $(head -5 $D/state) $(st signmsg) $(grep -E '^(page|app|client) ' $D/state) $(tail -5 $T/native.log) $(tail -5 $D/calls)"
# ---- 2. the keyring's prompt, no app window open
ok "2a. no app window is open" "$(! grep -q '^app ' $D/state && echo 1)" "$(grep '^app ' $D/state)"
t0=$(now); req k100-1 '{"app":"org.vinegarhq.Sober","proc":"sober","kind":"keyring","type":"password","title":"","message":"Choose password for new keyring","description":"An application wants to create a new keyring called “Sober”. Choose the password you want to use for it.","warning":"","choice":"","cont":"Continue","cancel":"Cancel","newpass":true,"chosen":false}'
w8 'grep -q "^ontop do-portal$" $D/state && grep -q "full=1 focus=1 app=do-portal " $D/state' 40; t1=$(now)
timeout 0.6 tail -f /dev/null
k=$(ev do-portal "(()=>{const o=document.querySelector('.krp60');return o?[o.querySelector('.kra').textContent,o.querySelector('.krm').textContent,document.activeElement&&document.activeElement.className].join('|'):'none'})()")
ok "2b. the keyring's prompt is on the screen, above everything, focused, within 2 s ($((t1-t0)) ms)" "$([ $((t1-t0)) -le 2000 ] && grep -q '^ontop do-portal$' $D/state && echo "$k" | grep -q '^Sober|Choose password for new keyring|kp1' && echo 1)" "$((t1-t0)) ms; $k; $(grep -E '^(ontop|client|page|app|portal) ' $D/state)"
import -window root $T/k.png 2>/dev/null
xdotool type --delay 30 'pw-new1'; xdotool key Tab; xdotool type --delay 30 'pw-new1'; xdotool key Return
w8 '[ "$(answers k100-1)" -ge 1 ]' 40; timeout 2 tail -f /dev/null
ok "2c. typed in as a person would: answered once, with the password" "$([ "$(answers k100-1)" = 1 ] && grep -q '^k100-1 {"response":0,"password":"pw-new1"' $D/portal/answers && [ ! -e $D/portal/req/k100-1.json ] && echo 1)" "$(cat $D/portal/answers 2>/dev/null)"
w8 '! grep -q "^ontop do-portal$" $D/state' 20
ok "2d. the prompt is gone from the screen; the portal's page stays for the next one" "$(! grep -q '^ontop do-portal$' $D/state && ! grep -q '^hit task.*do-portal' $D/state && [ "$(st portal | cut -d' ' -f1)" = ready ] && echo 1)" "$(grep -E '^(ontop|client|page|app|portal) ' $D/state)"
# ---- 3. two app windows open: a file dialog, once, by the portal's page only
click() { xdotool mousemove $1 $2 click 1; }
click $(hit pin notepad); w8 'grep -q "^app notepad " $D/state' 120; click $(hit pin calc); w8 'grep -q "^app calc " $D/state' 120; timeout 4 tail -f /dev/null
ok "3a. Notepad's and Calculator's windows are open" "$(grep -q '^app notepad ' $D/state && grep -q '^app calc ' $D/state && echo 1)" "$(grep '^app ' $D/state)"
p0=$(polls); timeout 5 tail -f /dev/null; p1=$(polls)
t0=$(now); req 900-7 '{"app":"org.rudolf.PortalStand","kind":"save","title":"Save the replay","multiple":false,"directory":false,"name":"replay.txt","folder":"","file":"","accept":"","patterns":[]}'
w8 'grep -q "^ontop do-portal$" $D/state' 40; t1=$(now); timeout 1 tail -f /dev/null
fd=$(ev do-portal "document.querySelectorAll('.lxfd').length+'|'+((document.querySelector('.lxfd h3')||{}).textContent||'')")
inapp="$(ev notepad "document.querySelectorAll('.rmodal').length")|$(ev calc "document.querySelectorAll('.rmodal').length")"
ok "3b. a file dialog: once, in the portal's page, above both windows ($((t1-t0)) ms), none in the app windows" "$(echo "$fd" | grep -q '^1|Save the replay — PortalStand' && [ "$inapp" = '0|0' ] && grep -q '^ontop do-portal$' $D/state && echo 1)" "$fd / $inapp / $(grep -E '^(ontop|client) ' $D/state)"
xdotool key Escape; w8 '[ "$(answers 900-7)" -ge 1 ]' 40; timeout 2 tail -f /dev/null
ok "3c. cancelled: answered once (no file)" "$([ "$(answers 900-7)" = 1 ] && grep -q '^900-7 {"response":1}' $D/portal/answers && echo 1)" "$(cat $D/portal/answers)"
ok "6. no app window collects the portal's requests: only the shell does (twice a second)" "$(n=$((p1-p0)); [ $n -ge 6 ] && [ $n -le 14 ] && echo 1)" "$((p1-p0)) polls in 5 s"
# ---- 4. a notification, a link
req n900-8 '{"app":"org.rudolf.PortalStand","kind":"notify","title":"PortalStand","body":"Signed in as a stand-in"}'
w8 'grep -q "^toast .*|PortalStand" $D/state' 40
ok "4a. a notification: the shell's own toast" "$(grep -q '^toast .*|PortalStand' $D/state && [ "$(answers n900-8)" = 1 ] && echo 1)" "$(grep '^toast' $D/state) $(cat $D/portal/answers)"
req 900-9 '{"app":"org.rudolf.PortalStand","kind":"url","uri":"https://example.org/signin?x=1"}'
w8 'grep -q "^app browser " $D/state' 80
ok "4b. a web link: Oracle's window opens (here WebKit's, no Chromium), answered once" "$(grep -q '^app browser ' $D/state && [ "$(answers 900-9)" = 1 ] && echo 1)" "$(grep '^app ' $D/state) $(cat $D/portal/answers) $(grep 'open\|browser' $D/calls | tail -3)"
# ---- 5. withdrawn
req 900-10 '{"app":"org.rudolf.PortalStand","kind":"open","title":"Open a level","multiple":false,"directory":false,"name":"","folder":"","file":"","accept":"","patterns":[]}'
w8 'grep -q "^ontop do-portal$" $D/state' 40; timeout 1 tail -f /dev/null; a=$(ev do-portal "document.querySelectorAll('.lxfd').length")
rm -f $D/portal/req/900-10.json; w8 '! grep -q "^ontop do-portal$" $D/state' 40; b=$(ev do-portal "document.querySelectorAll('.lxfd').length")
ok "5. a dialog whose app went away (its request withdrawn) closes, unanswered" "$([ "$a" = 1 ] && [ "$b" = 0 ] && [ "$(answers 900-10)" = 0 ] && echo 1)" "$a $b $(grep -E '^(ontop) ' $D/state)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
