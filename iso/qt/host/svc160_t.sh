#!/bin/sh
# 1.6.0 (docs/plan-1.6.0.md Part 12, item 12): the native desktop's background work runs once, whatever is open. rudolf-native under
# Xvfb with the stand-in rudolfd (fixtures/native_rd.py) serving this tree's page, and a script in every page (fixtures/svc160_inject.js)
# standing in for the person: with no app window open an alarm set a minute ahead rings once, a Calendar reminder shows once, a
# cellular notice (an incoming call's) and rudolfd's events reach the desktop once; with three app windows open each still once (from
# the desktop's service page, #app=do-svc, since 1.6.1 the shell's hidden page #app=do-bg, never from the windows); a Clock timer started and its window closed still rings; the
# service page is not on the taskbar, opens again when it ends or its process stops, ends at sign-out; an app's window runs only the
# timers it needs (svc60.js). Needs Xvfb, xdotool, ImageMagick, node, python3, gcc and WebKitGTK's headers.   sh svc160_t.sh [RUDOLF-NATIVE]
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/svc160.XXXXXX); PORT=${SVC160_PORT:-8651}; DSP=${SVC160_DSP:-:95}
command -v Xvfb >/dev/null && command -v xdotool >/dev/null && command -v convert >/dev/null || { echo "SKIP: needs Xvfb, xdotool and ImageMagick"; exit 0; }
BIN=$1
if [ -z "$BIN" ]; then BIN=$T/rudolf-native
	gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $BIN $HERE/../../sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
	grep -q warning $T/cc && { cat $T/cc; echo "FAIL build warnings"; exit 1; }
fi
D=$T/d; mkdir -p $D; node $HERE/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)"); UID_=$(node -e "console.log(require('$D/keys.json').Ann.id)")
printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","b2":"#000080","tb":"bottom","accent":"#2563eb","h24":true,"df":"ymd"},"pins":["notepad","calc","settings","clock"],"notes":2,"wallpaper":false}' > $D/settings.json
cp $HERE/fixtures/svc160_inject.js $D/inject.js; echo '{}' > $D/plan.json
echo '[{"n":1,"t":"📶 Mobile data","b":"old"}]' > $D/mobnotes.json; echo '{"ok":true,"users":[],"muted":false}' > $D/mic.json
python3 $HERE/fixtures/native_rd.py $PORT $D > $T/rd.log 2>&1 & RDP=$!
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=$DSP LIBGL_ALWAYS_SOFTWARE=1
trap 'kill $NP $RDP $XV 2>/dev/null; pkill -f "$T/" 2>/dev/null; rm -rf $T' EXIT
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
w8 'xdotool getmouselocation >/dev/null 2>&1' 40
(cd $D && HOME=$D RUDOLF_NATIVE_STATE=$D/state exec $BIN --size=1280x800 --draw=soft --base=http://127.0.0.1:$PORT > $T/native.log 2>&1) & NP=$!
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
click() { xdotool mousemove $1 $2 click 1; }
hit() { grep "^hit $1 $2 " $D/state | head -1 | awk '{print $4+int($6/2), $5+int($7/2)}'; }
# the toasts shown with this title: how many, and from which pages ("-" the shell's own)
tn() { grep "^toasted [0-9]* [^|]*|$1\$" $D/state | wc -l; }
tfrom() { grep "^toasted [0-9]* [^|]*|$1\$" $D/state | awk '{print $3}' | cut -d'|' -f1 | sort -u | tr '\n' ' '; }
seed() { printf '%s\n' "$1" > $D/plan.json.t; mv $D/plan.json.t $D/plan.json; }
hm() { date -d "+$1 sec" +%H:%M; }; ymd() { date -d "+$1 sec" +%Y-%m-%d; }
list() { grep "test/list?k=$1" $D/calls >/dev/null; }
# ---- signed in: the service page, unseen
w8 '[ "$(st signin)" = 1 ]' 160; LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return
w8 '[ "$(st user)" = Ann ]' 120; w8 '[ "$(st svc | cut -d" " -f1)" = ready ]' 240
ok "1. signed in: the desktop's service page is opened once and signs in (no app window open)" "$([ "$(st svc)" = "ready 1" ] && ! grep -q '^client .* app=' $D/state && echo 1)" "$(st svc) $(grep -E '^(client|app) ' $D/state) $(tail -5 $T/native.log)"
ok "2. ...not on the taskbar (no window's button) and not a window of the screen" "$(! grep -q '^hit win ' $D/state && ! grep -q 'app=do-bg' $D/state && echo 1)" "$(grep -E '^hit win|do-bg' $D/state)"
# ---- no app window open: an alarm a minute ahead, a reminder, a cellular notice (an incoming call), the microphone, an event
A1=$(hm 70); seed "{\"id\":\"a1\",\"seed\":{\"bd95alarms:$UID_\":\"[{\\\"t\\\":\\\"$A1\\\",\\\"label\\\":\\\"Wake\\\",\\\"days\\\":[],\\\"on\\\":true}]\",\"bd95cal:$UID_\":\"[{\\\"id\\\":1,\\\"title\\\":\\\"Dentist\\\",\\\"d\\\":\\\"$(ymd 70)\\\",\\\"t\\\":\\\"$A1\\\",\\\"rem\\\":0}]\"}}"
w8 'grep -q "test/list?k=do-bg" $D/calls' 40
echo '[{"n":1,"t":"📶 Mobile data","b":"old"},{"n":2,"t":"📞 Incoming call","b":"+15550100"}]' > $D/mobnotes.json
echo '{"ok":true,"users":["Zoom"],"muted":false}' > $D/mic.json
printf 'svctest 1\n' > $D/next-events
w8 '[ "$(tn "📞 Incoming call")" -ge 1 ]' 80; timeout 8 tail -f /dev/null
ok "3. no window open: an incoming call's notice (rudolfd's cellular notices) shows once" "$([ "$(tn "📞 Incoming call")" = 1 ] && echo 1)" "$(grep '^toasted' $D/state)"
w8 '[ "$(st mic)" = "1 0" ]' 40
ok "4. ...a call in an app (the microphone in use): the taskbar's 🎙️, from the shell itself (the service page does not poll it again)" "$([ "$(st mic)" = "1 0" ] && echo 1)" "$(st mic)"
w8 'grep -q "test/evgot?k=do-bg" $D/calls' 40
ok "5. ...rudolfd's events that are a page's reach the service page (none waits for a window)" "$([ "$(grep -c 'test/evgot' $D/calls)" = 1 ] && grep -q 'test/evgot?k=do-bg' $D/calls && echo 1)" "$(grep 'test/evgot' $D/calls)"
w8 '[ "$(tn "⏰ Wake")" -ge 1 ] && [ "$(tn "📅 Dentist")" -ge 1 ]' 520; timeout 6 tail -f /dev/null
ok "6. ...an alarm set a minute ahead rings once (the service page's)" "$([ "$(tn "⏰ Wake")" = 1 ] && [ "$(tfrom "⏰ Wake")" = "do-bg " ] && echo 1)" "$A1 $(date +%H:%M:%S) $(grep '^toasted' $D/state)"
ok "7. ...the Calendar's reminder shows once" "$([ "$(tn "📅 Dentist")" = 1 ] && [ "$(tfrom "📅 Dentist")" = "do-bg " ] && echo 1)" "$(grep '^toasted' $D/state)"
grep -q 'POST heartbeat' $D/calls; HB0=$(grep -c 'heartbeat' $D/calls)
# ---- three app windows open
for a in notepad calc settings; do click $(hit pin $a); w8 "grep -q '^client .* app=$a ' \$D/state" 120; done
w8 'list notepad && list calc && list settings' 60
ok "8. three app windows open" "$(for a in notepad calc settings; do grep -q "^client .* app=$a " $D/state || echo no; done | grep -q no || echo 1)" "$(grep '^client' $D/state)"
A2=$(hm 70); seed "{\"id\":\"a2\",\"seed\":{\"bd95alarms:$UID_\":\"[{\\\"t\\\":\\\"$A2\\\",\\\"label\\\":\\\"Second\\\",\\\"days\\\":[],\\\"on\\\":true}]\",\"bd95cal:$UID_\":\"[{\\\"id\\\":2,\\\"title\\\":\\\"Meeting\\\",\\\"d\\\":\\\"$(ymd 70)\\\",\\\"t\\\":\\\"$A2\\\",\\\"rem\\\":0}]\"}}"
echo '[{"n":1,"t":"📶 Mobile data","b":"old"},{"n":2,"t":"📞 Incoming call","b":"+15550100"},{"n":3,"t":"📞 Call again","b":"+15550101"}]' > $D/mobnotes.json
printf 'svctest 2\n' > $D/next-events
w8 '[ "$(tn "📞 Call again")" -ge 1 ]' 80; w8 '[ "$(grep -c test/evgot $D/calls)" -ge 2 ]' 40; timeout 8 tail -f /dev/null
ok "9. three windows: the incoming call's notice once (it was the shell's and every window's)" "$([ "$(tn "📞 Call again")" = 1 ] && echo 1)" "$(grep '^toasted' $D/state)"
ok "10. ...rudolfd's event once, in the window in front (as before), not the service page as well" "$([ "$(grep -c 'test/evgot' $D/calls)" = 2 ] && ! grep 'test/evgot' $D/calls | tail -1 | grep -q do-bg && echo 1)" "$(grep 'test/evgot' $D/calls)"
w8 '[ "$(tn "⏰ Second")" -ge 1 ] && [ "$(tn "📅 Meeting")" -ge 1 ]' 520; timeout 8 tail -f /dev/null
ok "11. ...the alarm rings ONCE (the service page's, not each window's)" "$([ "$(tn "⏰ Second")" = 1 ] && [ "$(tfrom "⏰ Second")" = "do-bg " ] && echo 1)" "$A2 $(date +%H:%M:%S) $(grep '^toasted' $D/state)"
ok "12. ...the reminder once" "$([ "$(tn "📅 Meeting")" = 1 ] && [ "$(tfrom "📅 Meeting")" = "do-bg " ] && echo 1)" "$(grep '^toasted' $D/state)"
# what each page runs (svc60.js), as the pages said: test/list's body, kept by native_rd.py as DIR/list-K.json
ok "13. an app's window (Notepad) runs only what it needs: the theme's and the icons' ticks, not the alarms', the heartbeat or the updates'" "$(node -e "
const l=JSON.parse(require('fs').readFileSync('$D/list-notepad.json','utf8'));const run=m=>l.filter(h=>h.mod==m&&h.run).length;
const ok=run('a11y25.js')>=2&&run('apps25b.js')==0&&run('core25.js')==0&&run('term26.js')==0&&run('sysupd25.js')==0&&run('shell25.js')==0&&run('linux26.js')==0;
console.log(ok?1:0)" 2>&1)" "$(cat $D/list-notepad.json 2>/dev/null | head -c 600)"
ok "14. ...the service page runs them (the alarms', the session's, the updates'), not what the shell does itself (heartbeat, 🎙️, cellular)" "$(node -e "
const l=JSON.parse(require('fs').readFileSync('$D/list-do-bg.json','utf8'));const run=m=>l.filter(h=>h.mod==m&&h.run).length;
const hb=l.filter(h=>/heartbeat/.test(h.f)&&h.run).length;
console.log(run('apps25b.js')==2&&run('term26.js')==2&&run('shell25.js')>=2&&run('call59.js')==0&&run('mobile60.js')==0&&hb==0?1:0)" 2>&1)" "$(cat $D/list-do-bg.json 2>/dev/null | head -c 600)"
# ---- a Clock timer started, its window closed: still rings
seed '{"id":"a2","clock":{"secs":40,"label":"Tea"}}'
click $(hit pin clock); w8 'grep -q "test/timerset?n=1" $D/calls' 120; w8 '! grep -q "^client .* app=clock " $D/state' 60
ok "15. a Clock timer started (40 s), then the Clock's window closed" "$(grep -q 'test/timerset?n=1' $D/calls && ! grep -q '^client .* app=clock ' $D/state && echo 1)" "$(grep -E 'timerset|^client' $D/state $D/calls)"
w8 '[ "$(tn "⏰ Tea")" -ge 1 ]' 280; timeout 6 tail -f /dev/null
ok "16. ...the timer rings once, its window closed (the service page's)" "$([ "$(tn "⏰ Tea")" = 1 ] && [ "$(tfrom "⏰ Tea")" = "do-bg " ] && echo 1)" "$(grep '^toasted' $D/state)"
# ---- the service page ends: opened again; its process stops: loaded again
seed '{"id":"a2","close":"c1"}'
w8 '[ "$(st svc)" = "ready 2" ]' 160
ok "17. the service page closed: opened again by the shell (and signs in)" "$([ "$(st svc)" = "ready 2" ] && echo 1)" "$(st svc) $(grep 'service page' $T/native.log)"
timeout 4 tail -f /dev/null
ok "18. ...the timer that rang is not rung again" "$([ "$(tn "⏰ Tea")" = 1 ] && echo 1)" "$(grep '^toasted' $D/state)"
WP=$(pgrep -P $NP -f WebKitWebProcess | head -1)
if [ -n "$WP" ]; then kill $WP; timeout 3 tail -f /dev/null; w8 '[ "$(st svc | cut -d" " -f1)" = ready ]' 240
	ok "19. the pages' process stopped (killed): the service page is loaded again and ready" "$([ "$(st svc | cut -d' ' -f1)" = ready ] && grep -q 'stopped' $T/native.log && echo 1)" "$(st svc) $(grep -E 'stopped|service' $T/native.log | tail -4)"
else ok "19. the pages' process stopped (killed): the service page is loaded again and ready" 0 "no WebKitWebProcess of rudolf-native's ($NP): $(ps -eo pid,ppid,args | grep -i webkit | head -3)"; fi
# ---- signing out ends it
xdotool key super+l 2>/dev/null; w8 '[ "$(st locked)" = 1 ]' 40
ok "20. locked: the service page stays (alarms still ring while locked, as on Standard)" "$([ "$(st svc | cut -d' ' -f1)" = ready ] && echo 1)" "$(st locked) $(st svc)"
LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return; w8 '[ "$(st locked)" = 0 ]' 60
click $(hit start -); w8 '[ "$(st menu)" = 1 ]' 20; M=$(grep '^mrow ' $D/state | head -1 | awk '{print $3+60, $4+40}'); i=0; while [ $i -lt 30 ] && ! grep -q "^mrow logoff " $D/state; do xdotool mousemove $M click 5; timeout 0.15 tail -f /dev/null; i=$((i+1)); done
L=$(grep '^mrow logoff ' $D/state | awk '{print $3+int($5/2), $4+int($6/2)}'); click $L
w8 '[ "$(st signin)" = 1 ] && [ "$(st user)" = - ]' 60; timeout 6 tail -f /dev/null
ok "21. signing out ends the service page (and it is not opened again until the next sign-in)" "$([ "$(st svc | cut -d' ' -f1)" = - ] && echo 1)" "$(st svc) $(st user)"
LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return; w8 '[ "$(st user)" = Ann ]' 120; w8 '[ "$(st svc | cut -d" " -f1)" = ready ]' 240
ok "22. signed in again (a user switch is a sign-out and a sign-in): the account's service page again" "$([ "$(st svc | cut -d' ' -f1)" = ready ] && echo 1)" "$(st svc)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
