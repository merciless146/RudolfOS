#!/bin/sh
# 1.6.0 (the owner's Vivobook report of 2026-11-16, beta 1.6.0.261116): Task Manager's "Check this computer…" on the Native desktop:
# rudolf-native with its app windows (WebKitGTK) under Xvfb and the stand-in rudolfd (fixtures/native_rd.py) serving the real page.
#   1. the keyboard step: every key of the check answers in Task Manager's window (19 did not on the Vivobook: F1 opened Help in front
#      of the check, Super the start menu, and the keys after them went there; WebKit named Super "OSLeft"); the shell's keys are
#      given back when the step ends; outside it a bare arrow, F5 and Ctrl reach the app, Super alone opens the start menu and the
#      app sees MetaLeft as on Standard, F1 still opens Help, Meta+E File Explorer; Settings › Keyboard's "Press keys…" takes Meta+E
# Needs Xvfb, xdotool, node, python3, gcc and WebKitGTK's headers.   sh hwcnative160_t.sh [RUDOLF-NATIVE]
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/hwcn160.XXXXXX); PORT=${HWCN160_PORT:-8642}; DSP=${HWCN160_DSP:-:87}
command -v Xvfb >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs Xvfb and xdotool"; exit 0; }
BIN=$1
if [ -z "$BIN" ]; then BIN=$T/rudolf-native
	gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $BIN $HERE/../../sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
fi
D=$T/d; mkdir -p $D; node $HERE/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)")
printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","tb":"bottom","accent":"#2563eb"},"pins":["notepad"],"notes":0,"wallpaper":false}' > $D/settings.json
touch $D/health-on $D/stats-on; printf '' > $D/sleep-ms
printf '%s' '{"ok":true,"machine":"Stand-in","firmware":"test","cpu":{"model":"Test CPU","threads":2},"gpu":"","audio":" 0 [Generic]: HDA-Intel - HD-Audio Generic","storage":"","net":"","input":"","kernel":{"release":"6.18.63-rudolf","machine":"x86_64","cmdline":"","modules":"","messages":""},"memory":{},"services":{"sound":true,"display":true},"display":"gpu 1280 800 60.00 native 1280x800","renderer":"AMD Radeon Graphics (radeonsi, mendocino)","session":""}' > $D/system.json
python3 $HERE/fixtures/native_rd.py $PORT $D > $T/rd.log 2>&1 & RDP=$!
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=$DSP LIBGL_ALWAYS_SOFTWARE=1
trap 'kill $NP $RDP $XV 2>/dev/null; [ -n "$PA" ] && pactl exit 2>/dev/null; pkill -f "$T/" 2>/dev/null; rm -rf $T' EXIT
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
w8 'xdotool getmouselocation >/dev/null 2>&1' 40
mkfifo $D/ev
# (a sound server of its own: a null sink recorded, and a test microphone, a sine wave: WebKit lists no monitor as a microphone)
PA=; if command -v pulseaudio >/dev/null && command -v parec >/dev/null; then export XDG_RUNTIME_DIR=$T/xdg; mkdir -p -m 700 $XDG_RUNTIME_DIR $T/pahome
	HOME=$T/pahome pulseaudio -n --daemonize=yes --exit-idle-time=-1 --use-pid-file=no -L "module-null-sink sink_name=h160" -L "module-sine-source source_name=mic160 frequency=440" -L "module-native-protocol-unix auth-anonymous=1 socket=$XDG_RUNTIME_DIR/pulse/native" >/dev/null 2>&1
	export PULSE_SERVER=unix:$XDG_RUNTIME_DIR/pulse/native; w8 'pactl info >/dev/null 2>&1' 40 && pactl set-default-sink h160 && pactl set-default-source mic160 && PA=1; fi
(cd $D && HOME=$D RUDOLF_NATIVE_STATE=$D/state RUDOLF_NATIVE_TEST_EVAL=$D/ev exec $BIN --size=1280x800 --draw=soft --base=http://127.0.0.1:$PORT > $T/native.log 2>&1) & NP=$!
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
ev() { n0=$(cat $D/ev.out 2>/dev/null | wc -l); printf '%s %s\n' "$1" "$2" > $D/ev; w8 "[ \$(cat $D/ev.out 2>/dev/null | wc -l) -gt $n0 ]" 60; tail -n +$((n0+1)) $D/ev.out | head -1 | cut -d' ' -f2-; }
napps() { grep -c "^app $1 " $D/state 2>/dev/null; }
rec() { [ -n "$PA" ] || return; parec -d h160.monitor --format=s16le --rate=44100 --channels=1 > $T/rec.$1 & echo $! > $T/rec.pid; }
recstop() { [ -n "$PA" ] || return; kill $(cat $T/rec.pid) 2>/dev/null; wait $(cat $T/rec.pid) 2>/dev/null; python3 -c "import array,sys;d=open(sys.argv[1],'rb').read();d=d[:len(d)//2*2];a=array.array('h',d);print(max([abs(x) for x in a] or [0]))" $T/rec.$1; }
w8 '[ "$(st signin)" = 1 ]' 120
LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return
w8 '[ "$(st user)" = Ann ] && [ "$(st signin)" = 0 ]' 120
ok "0. signed in" "$([ "$(st user)" = Ann ] && echo 1)" "$(head -5 $D/state)"
timeout 6 tail -f /dev/null
xdotool key ctrl+shift+Escape; w8 '[ "$(napps taskmgr)" -ge 1 ]' 120; timeout 8 tail -f /dev/null
ok "0b. Task Manager's window" "$([ "$(napps taskmgr)" -ge 1 ] && echo 1)" "$(grep '^app' $D/state)"
GO='(()=>{window.__G="?";(async()=>{if(!window.__H||!document.body.contains(__H))window.__H=hwCheck();for(let k=0;k<90;k++){const h=__H.querySelector("h3").textContent;if(h==%s){__G="at "+h;return}const bs=[...__H.querySelectorAll(".hwcbt button")];const b=bs.find(x=>x.textContent=="Skip")||bs.find(x=>x.className=="pri");if(b)b.click();await new Promise(r=>setTimeout(r,800))}__G="stuck at "+__H.querySelector("h3").textContent})();return 1})()'
go() { ev taskmgr "$(printf "$GO" "\"$1\"")" >/dev/null; w8 "ev taskmgr '__G' | grep -q '^at '" 80; ev taskmgr '__G'; }
# ---- 1. the keyboard step
go Keyboard >/dev/null; timeout 1 tail -f /dev/null
ok "1a. the keyboard step has every key: the shell's grabs are off while Task Manager is in front" "$(st keys | grep -q '^grabbed=0 superraw=1 kbcap=taskmgr' && echo 1)" "$(st keys)"
for k in Escape F1 F2 F3 F4 F5 F6 F7 F8 F9 F10 F11 F12 grave 1 2 3 4 5 6 7 8 9 0 minus equal BackSpace Tab q w e r t y u i o p bracketleft bracketright backslash Caps_Lock a s d f g h j k l semicolon apostrophe Return Shift_L z x c v b n m comma period slash Shift_R Control_L Super_L Alt_L space Alt_R Control_R Left Up Down Right; do
	xdotool key $k; timeout 0.12 tail -f /dev/null; done
xdotool key Caps_Lock; timeout 1 tail -f /dev/null
o=$(ev taskmgr '__H.querySelector(".hwkm").textContent+"|"+[...__H.querySelectorAll(".hwkb span")].filter(x=>x.style.background!="rgb(22, 163, 74)").map(x=>x.dataset.c).join(" ")')
ok "1b. ...every one of the 76 keys answers (F1-F12, the left Ctrl, Super and Alt, the arrows), nothing opened over the check" "$([ "$o" = '76 of 76 keys|' ] && [ "$(st menu)" = 0 ] && [ "$(napps help)" = 0 ] && echo 1)" "$o menu=$(st menu) $(grep '^app' $D/state)"
ev taskmgr '(()=>{[...__H.querySelectorAll(".hwcbt button")].find(b=>b.textContent=="Done").click();return 1})()' >/dev/null; timeout 1 tail -f /dev/null
o=$(ev taskmgr '__H.querySelector(".hwkm").textContent')
ok "1c. ...Done: \"Every key works.\", and the shell has its keys again" "$([ "$o" = 'Every key works.' ] && st keys | grep -q '^grabbed=1 superraw=1 kbcap=-' && echo 1)" "$o $(st keys)"
ev taskmgr '(()=>{__H.querySelector(".hwcbt button").click();return 1})()' >/dev/null
ev taskmgr '(()=>{window.__K=[];const f=e=>__K.push(e.type[3]+":"+e.code+":"+e.key+":"+e.keyCode+(e.metaKey?":M":""));addEventListener("keydown",f,true);addEventListener("keyup",f,true);return 1})()' >/dev/null
xdotool key F5 Left Control_L Up; timeout 0.6 tail -f /dev/null; o=$(ev taskmgr '__K.join(" ")')
ok "1d. outside the step a bare F5, the arrows and Ctrl reach the app in front" "$(echo "$o" | grep -q 'd:F5:F5:116 u:F5:F5:116 d:ArrowLeft:ArrowLeft:37 u:ArrowLeft:ArrowLeft:37 d:ControlLeft:Control:17 u:ControlLeft:Control:17 d:ArrowUp' && echo 1)" "$o"
ev taskmgr '(()=>{__K.length=0;return 1})()' >/dev/null; xdotool key Super_L; w8 '[ "$(st menu)" = 1 ]' 12; o=$(ev taskmgr '__K.join(" ")')
ok "1e. Super alone opens the start menu, and the app saw it as Chromium names it (MetaLeft, Meta, 91, metaKey)" "$([ "$(st menu)" = 1 ] && [ "$o" = 'd:MetaLeft:Meta:91:M u:MetaLeft:Meta:91' ] && echo 1)" "menu=$(st menu) $o"
xdotool key Escape; w8 '[ "$(st menu)" = 0 ]' 12
xdotool key F1; w8 '[ "$(napps help)" -ge 1 ]' 60
ok "1f. F1 still opens Help (the shell's shortcut) outside the step" "$([ "$(napps help)" -ge 1 ] && echo 1)" "$(grep '^app' $D/state)"
# Settings › Keyboard: Press keys… then Meta+E is taken by the page (not the shell's)
xdotool key super+i; w8 '[ "$(napps settings)" -ge 1 ]' 80; timeout 8 tail -f /dev/null
o=$(ev settings '(()=>{try{SW.show("kbd",true)}catch(e){}return 1})()'); timeout 3 tail -f /dev/null
o=$(ev settings '(()=>{const r=[...document.querySelectorAll(".kbt tbody tr")].find(x=>/Open Help/.test(x.textContent));if(!r)return "no row";r.querySelector("button").click();return "ok"})()')
timeout 1 tail -f /dev/null; k0=$(st keys); n0=$(napps explorer); xdotool key super+e; timeout 2 tail -f /dev/null
o2=$(ev settings '(()=>{const r=[...document.querySelectorAll(".kbt tbody tr")].find(x=>/Open Help/.test(x.textContent));return r?r.querySelector("kbd").textContent:"?"})()')
ok "1h. Settings › Keyboard's Press keys…: Meta+E goes to the page (the shell's keys off meanwhile), not File Explorer" "$([ "$o" = ok ] && echo "$k0" | grep -q 'grabbed=0' && [ "$(napps explorer)" = "$n0" ] && echo "$o2" | grep -q 'E' && st keys | grep -q 'grabbed=1' && echo 1)" "$o | $k0 | $o2 | $(st keys) | $(grep '^app' $D/state)"
ev settings '(()=>{try{localStorage.removeItem("bd95kb:"+uid())}catch(e){}return 1})()' >/dev/null
n0=$(napps explorer); xdotool key super+e; w8 "[ \$(napps explorer) -gt $n0 ]" 60
ok "1g. Meta+E opens File Explorer (an exact combination the shell keeps)" "$([ "$(napps explorer)" -gt "$n0" ] && echo 1)" "$(grep '^app' $D/state)"
echo "$F of $N failed"; [ $F = 0 ]
