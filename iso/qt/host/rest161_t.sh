#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 23 C, the owner: "if it won't cause any issues with Oracle or other Rudolf/Debian/Flatpak/Wine apps, then
# add it"): on the native desktop an app's window minimized or covered for a while gives its page's memory back (its page unloaded) and
# comes back where it was when shown. rudolf-native under Xvfb with the stand-in rudolfd (fixtures/native_rd.py), Idle windows on, the
# time in seconds (RUDOLF_NATIVE_REST_SECS=4):
#   1. Notepad with two files open (the second scrolled, a selection) minimized: it rests (its page empty), Task Manager says "Resting
#      (no memory used)"; its taskbar button: back, the same tabs, the current one, the scroll and the selection, the same size and place
#   2. File Explorer at a folder, covered by a maximized window: it rests; the window over it minimized: back at that folder
#   3. never: unsaved changes (typed in Notepad), a dialog open, sound or video playing, a request still going (a download), Settings
#      (not on the list), Oracle's window, a Linux app's X window, the shell's hidden page; each says why
#   4. closing a resting window: closed at once; Idle windows off (Settings): nothing rests
# Needs Xvfb, xdotool, node, python3, gcc and WebKitGTK's headers.   sh rest161_t.sh [RUDOLF-NATIVE]
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/rest161.XXXXXX); PORT=${REST_PORT:-8624}; DSP=${REST_DSP:-:99}
command -v Xvfb >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs Xvfb and xdotool"; exit 0; }
BIN=$1
if [ -z "$BIN" ]; then BIN=$T/rudolf-native
	gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $BIN $HERE/../../sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
	grep -q warning $T/cc && { cat $T/cc; echo "FAIL build warnings"; exit 1; }
fi
D=$T/d; mkdir -p $D; node $HERE/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)")
printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","tb":"bottom","accent":"#2563eb","h24":true,"idlerest":true},"pins":["notepad","explorer","settings","taskmgr"],"notes":2,"wallpaper":false}' > $D/settings.json
python3 $HERE/fixtures/native_rd.py $PORT $D > $T/rd.log 2>&1 & RDP=$!
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=$DSP LIBGL_ALWAYS_SOFTWARE=1
mkfifo $T/ev
trap 'kill $NP $RDP $XV 2>/dev/null; pkill -f "$T/" 2>/dev/null; rm -rf $T' EXIT
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
w8 'xdotool getmouselocation >/dev/null 2>&1' 40
(cd $D && HOME=$D RUDOLF_NATIVE_STATE=$D/state RUDOLF_NATIVE_TEST_EVAL=$T/ev RUDOLF_NATIVE_REST_SECS=4 exec $BIN --size=1280x800 --draw=soft --base=http://127.0.0.1:$PORT > $T/native.log 2>&1) & NP=$!
exec 7<>$T/ev
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
click() { xdotool mousemove $1 $2 click 1; }
hit() { grep "^hit $1 $2 " $D/state | head -1 | awk '{print $4+int($6/2), $5+int($7/2)}'; }
cmact() { grep "^cmitem [0-9]* [0-9]* 0 $1|" $D/state | head -1 | awk '{print $2, $3}'; }
flyb() { grep "^flybtn $1 " $D/state | head -1 | awk '{print $3, $4}'; }
# (a page's answer: KEY JS, read back from the FIFO's .out; the hidden page by its name)
ev() { : > $T/ev.out; printf '%s %s\n' "$1" "$2" >&7; w8 "grep -q '^$1 ' $T/ev.out 2>/dev/null" 40; grep "^$1 " $T/ev.out | tail -1 | cut -d' ' -f2-; : > $T/ev.out; }
pages() { grep '^page ' $D/state | cut -d' ' -f2 | sort | tr '\n' ' '; }
wp() { ps -eo pid=,ppid=,comm= | awk -v r=$NP '{p[$1]=$2; c[$1]=$3} END {a[r]=1; do {m=0; for (k in p) if (!(k in a) && (p[k] in a)) {a[k]=1; m=1}} while (m); n=0; for (k in a) if (c[k] ~ /^WebKitWebProces/) n++; print n}'; }
opened() { grep -c "the shell's hidden page is opened" $T/native.log; }
others() { grep -c "is opened\|a page of its own" $T/native.log | head -1; }
dolog() { ev do-bg "JSON.stringify(OA.doLog||[])"; }
cl() { grep "^client $1 " $D/state | head -1; }; cv() { cl $1 | awk -v f=$2 '{print $f}'; }
cid() { grep "^client .* app=$1 " $D/state | head -1 | cut -d' ' -f2; }
rs() { grep "^rest $1 " $D/state | head -1 | cut -d' ' -f3-; }   # RESTING IDLE WHY
mini() { I=$(cid $1); click $(( $(cv $I 4) + $(cv $I 6) + 2 - 75 )) $(( $(cv $I 5) + 18 )); w8 "cl $I | grep -q ' min=1 '" 20; }
openp() { click $(hit pin $1); w8 "grep -q '^client .* app=$1 ' \$D/state" 120; w8 "[ \"\$(ev $1 'String(!!(OA.main&&OA.main.el))')\" = true ]" 60; timeout 1 tail -f /dev/null; }
w8 '[ "$(st signin)" = 1 ]' 160; LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return
w8 '[ "$(st user)" = Ann ]' 120; w8 '[ "$(st svc | cut -d" " -f1)" = ready ]' 240
# ---- 1. Notepad
openp notepad; NI=$(cid notepad); G0="$(cv $NI 4) $(cv $NI 5) $(cv $NI 6) $(cv $NI 7)"
ev notepad "fs['C:']['LONG.TXT']=Array.from({length:400},(_,i)=>'line '+i).join('\n');fsSave();OA.main.dtabs.open(['C:','README.TXT']);OA.main.dtabs.open(['C:','LONG.TXT']);const ta=OA.main.body.querySelector('textarea');ta.scrollTop=900;ta.setSelectionRange(50,55);1" >/dev/null
mini notepad; w8 '[ "$(rs notepad | cut -d" " -f1)" = 1 ]' 160
ok "1. Notepad minimized: it rests, its page unloaded" "$([ "$(rs notepad | cut -d' ' -f1)" = 1 ] && [ "$(ev notepad 'location.href')" = about:blank ] && grep -q "notepad's window rests" $T/native.log && echo 1)" "$(rs notepad); $(ev notepad 'location.href')"
openp taskmgr; TM=$(ev taskmgr "[...document.querySelectorAll('.tmt tbody tr')].map(r=>r.innerText.replace(/\\s+/g,' ')).join(' / ')")
ok "1b. ...Task Manager: Resting (no memory used)" "$(echo "$TM" | grep -q 'Notepad[^/]*Resting (no memory used)' && echo 1)" "$TM"
I=$(cid taskmgr); click $(( $(cv $I 4) + $(cv $I 6) + 2 - 21 )) $(( $(cv $I 5) + 18 )); w8 '[ -z "$(cid taskmgr)" ]' 40
click $(hit win $NI); w8 '[ "$(ev notepad "String(!!OA.restored)")" = true ]' 80; timeout 1 tail -f /dev/null
R=$(ev notepad "(()=>{const T=OA.main.dtabs,ta=OA.main.body.querySelector('textarea');return JSON.stringify([T.tabs.map(t=>t.p&&t.p.join('/')),T.i,ta.scrollTop,ta.selectionStart,ta.selectionEnd])})()")
ok "1c. ...its taskbar button: back where it was (the two files, the second current, its scroll and selection), the same size and place" "$([ "$R" = '[["C:/README.TXT","C:/LONG.TXT"],1,900,50,55]' ] && [ "$(cv $NI 4) $(cv $NI 5) $(cv $NI 6) $(cv $NI 7)" = "$G0" ] && [ "$(rs notepad | cut -d' ' -f1)" = 0 ] && echo 1)" "$R / $(cl $NI) / $G0"
ok "1d. ...its page back in the web process the windows share here (one, as before)" "$([ "$(wp)" -le 1 ] && echo 1)" "$(wp)"
# ---- 2. File Explorer, covered
openp explorer; EI=$(cid explorer)
ev explorer "(()=>{const ad=OA.main.body.querySelector('.ad');ad.value='C:\\\\USERS';ad.dispatchEvent(new KeyboardEvent('keydown',{key:'Enter',bubbles:true}));return 1})()" >/dev/null; timeout 1 tail -f /dev/null
openp settings; SI=$(cid settings); [ "$(cl $SI | grep -o 'max=[01]')" = max=1 ] || { xdotool mousemove 300 15; I=$SI; click $(( $(cv $I 4) + $(cv $I 6) + 2 - 48 )) $(( $(cv $I 5) + 18 )); w8 "cl $SI | grep -q ' max=1 '" 20; }
w8 '[ "$(rs explorer | cut -d" " -f1)" = 1 ]' 160
ok "2. File Explorer at C:\\USERS, covered by a maximized window: it rests" "$([ "$(rs explorer | cut -d' ' -f1)" = 1 ] && cl $EI | grep -q ' min=0 ' && echo 1)" "$(rs explorer); $(cl $EI); $(cl $SI)"
mini settings; w8 '[ "$(ev explorer "String(!!OA.restored)")" = true ]' 80; timeout 1 tail -f /dev/null
ok "2c. ...the window over it minimized: back at its folder" "$([ "$(ev explorer "OA.main.body.querySelector('.ad').value")" = 'C:\USERS' ] && [ "$(rs explorer | cut -d' ' -f1)" = 0 ] && echo 1)" "$(ev explorer "OA.main.body.querySelector('.ad').value") $(rs explorer)"
timeout 9 tail -f /dev/null
ok "2b. ...Settings, not on the list, minimized: never rests (said so)" "$(rs settings | grep -q '^0 1 this app cannot be restored$' && [ "$(ev settings 'location.protocol')" = http: ] && echo 1)" "$(rs settings)"
# ---- 3. never
click $(hit win $NI); w8 "cl $NI | grep -q ' min=0 '" 20; timeout 1 tail -f /dev/null; xdotool type --delay 20 'unsaved'; mini notepad; timeout 9 tail -f /dev/null
ok "3a. Notepad with unsaved changes minimized: never rests (unsaved changes)" "$(rs notepad | grep -q '^0 1 unsaved changes$' && [ "$(ev notepad 'location.protocol')" = http: ] && echo 1)" "$(rs notepad)"
ev notepad "(()=>{const ta=OA.main.body.querySelector('textarea');const t=OA.main.dtabs.tabs[OA.main.dtabs.i];ta.value=t.saved;ta.dispatchEvent(new Event('input'));const d=document.createElement('div');d.className='rmodal';d.innerHTML='<div class=rmbox>x</div>';document.body.appendChild(d);window.__t3=d;return 1})()" >/dev/null; timeout 9 tail -f /dev/null
ok "3b. ...a dialog open: never rests" "$(rs notepad | grep -q '^0 .*dialog' && echo 1)" "$(rs notepad)"
# (the page's dialog covers the screen, as Standard's: the window is shown again; minimized again for the next ones)
ev notepad "(()=>{window.__t3.remove();return 1})()" >/dev/null; timeout 1 tail -f /dev/null; cl $NI | grep -q ' min=1 ' || mini notepad
ev notepad "(()=>{const c=new AudioContext();const o=c.createOscillator();o.connect(c.destination);o.start();window.__t3=c;return c.state})()" >/dev/null; timeout 9 tail -f /dev/null
ok "3c. ...sound playing (Web Audio): never rests" "$(rs notepad | grep -q '^0 .*\(sound\|playing\)' && echo 1)" "$(rs notepad)"
ev notepad "(()=>{window.__t3.close();window.__t3=null;rd('test/slow?ms=25000').catch(()=>{});return 1})()" >/dev/null; cl $NI | grep -q ' min=1 ' || mini notepad; timeout 9 tail -f /dev/null
ok "3d. ...a request still going (a download): never rests" "$(rs notepad | grep -q '^0 .*\(request\|job\|download\)' && echo 1)" "$(rs notepad)"
ok "3e. the shell's hidden page never rests (it is no window)" "$(! grep -q '^rest do-bg' $D/state && [ "$(st svc | cut -d' ' -f1)" = ready ] && echo 1)" "$(grep '^rest\|^svc' $D/state)"
XC=$T/xc; gcc -O2 -o $XC $HERE/fixtures/xwmclient.c -lX11 2>/dev/null && { $XC app > $T/xc.log & XP=$!; w8 'grep -q "^id" $T/xc.log' 40; XID=$(sed -n 's/^id 0x//p' $T/xc.log | head -1); w8 '[ -n "$(cl $XID)" ]' 40
	click $(( $(cv $XID 4) + $(cv $XID 6) + 2 - 75 )) $(( $(cv $XID 5) + 18 )); timeout 9 tail -f /dev/null
	ok "3f. a Linux app's X window minimized: left alone (no page, never rests)" "$(cl $XID | grep -q ' min=1 ' && ! grep "^rest " $D/state | grep -qv '^rest \(notepad\|explorer\|settings\|taskmgr\|browser\) ' && kill -0 $XP 2>/dev/null && echo 1)" "$(cl $XID); $(grep '^rest' $D/state)"; kill $XP 2>/dev/null; }
ev do-bg "oaPost({op:'open',app:'browser'});1" >/dev/null; w8 'grep -q "^client .* app=browser " $D/state' 120; timeout 3 tail -f /dev/null; mini browser; timeout 9 tail -f /dev/null
ok "3g. Oracle's window minimized: never rests" "$(rs browser | grep -q '^0 0 Oracle$' && [ "$(ev browser 'location.protocol')" = http: ] && echo 1)" "$(rs browser)"
# ---- 4. closing a resting window; Idle windows off
I=$(cid explorer); mini explorer; w8 '[ "$(rs explorer | cut -d" " -f1)" = 1 ]' 160
xdotool mousemove $(hit win $I) click 3; w8 '[ -n "$(grep "^cmitem .*|Close" $D/state)" ]' 20; C=$(grep '^cmitem .*|Close$' $D/state | head -1 | awk '{print $2, $3}'); [ -n "$C" ] && click $C; w8 '[ -z "$(cid explorer)" ]' 20
ok "4. a resting window closed (its taskbar button's Close): closed at once" "$([ -z "$(cid explorer)" ] && echo 1)" "$(grep '^cmitem' $D/state | tr '\n' ' '); $(cl $I)"
printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","tb":"bottom","accent":"#2563eb","h24":true,"idlerest":false},"pins":["notepad","explorer","settings","taskmgr"],"notes":2,"wallpaper":false}' > $D/settings.json.t; mv $D/settings.json.t $D/settings.json
timeout 6 tail -f /dev/null; click $(hit win $NI); w8 "cl $NI | grep -q ' min=0 '" 20; w8 '[ "$(ev notepad "String(!!(OA.main&&OA.main.el))")" = true ]' 80; timeout 2 tail -f /dev/null
ev notepad "(()=>{const ta=OA.main.body.querySelector('textarea');const t=OA.main.dtabs.tabs[OA.main.dtabs.i];if(t)ta.value=t.saved;return 1})()" >/dev/null; mini notepad; timeout 10 tail -f /dev/null
ok "4b. Idle windows off: a minimized Notepad (nothing unsaved) does not rest" "$(rs notepad | grep -q '^0 0 off$' && echo 1)" "$(rs notepad)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
