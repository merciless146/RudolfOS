#!/bin/sh
# 1.6.0 (the owner's Vivobook reports of 2026-11-15 on the native desktop): rudolf-native with its app windows (WebKitGTK) under
# Xvfb and the stand-in rudolfd (fixtures/native_rd.py) serving the real page, checked moment by moment rather than by finished
# pictures:
#   1. an app's window shows no sign-in screen in any of its frames while it signs in with the native session (a probe given to
#      the page before its own code: #login looked at on every animation frame); the page opened by hand (no native session)
#      still shows it, and the desktop's own page (no #app=) shows it at once, as before
#   2. sound: the shell's own sounds (the start-up chime, the sign-in sound) heard; an app window's WebAudio and <audio> heard, its
#      stream playing; after a page muted a sound, the next one still at its own volume (PulseAudio kept the muted stream's volume
#      for every WebKit stream of that role), and a page's own volume still applied
#   3. a setting saved in Settings' window reaches the other open window within a second and the shell at once (op "set": the
#      page's storage sent to rudolfd, the settings read again), and a window that missed it never writes the old value back
#   4. the system monitor and the weather widgets have their timers and live values after signing in and after the native
#      desktop started again with the session going on; the windows counted are the open ones
#   5. the notice after an update pops up once (from the shell's start-up page, not from each app window), "Real browser tabs
#      are ready" never on Native, however many windows open
#   6. Ctrl+Alt+T in an app's window opens the Terminal's own window (not a second window inside that app's)
# Needs Xvfb, xdotool, node (and Playwright's Chromium for the page opened by hand), python3, gcc and WebKitGTK's headers.
#   sh native160_t.sh [RUDOLF-NATIVE]
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/native160.XXXXXX); PORT=${NATIVE160_PORT:-8616}; DSP=${NATIVE160_DSP:-:86}
command -v Xvfb >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs Xvfb and xdotool"; exit 0; }
BIN=$1
if [ -z "$BIN" ]; then BIN=$T/rudolf-native
	gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $BIN $HERE/../../sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
fi
D=$T/d; mkdir -p $D; node $HERE/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)")
# Ann saw an older release (the update notice is due once); the shell's real tabs offered as ready (shellOffer's conditions)
node -e "const f=require('fs'),p='$D/ls.json',l=JSON.parse(f.readFileSync(p));const s=JSON.parse(l.data['bd95set:uAnnA1b2']);s.seenRel='v0';l.data['bd95set:uAnnA1b2']=JSON.stringify(s);f.writeFileSync(p,JSON.stringify(l))"
printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","tb":"bottom","accent":"#2563eb"},"pins":["notepad"],"notes":0,"wallpaper":false,"widgets":[{"t":"mon","x":900,"y":60,"txt":""},{"t":"weather","x":900,"y":220,"txt":""}],"wloc":{"lat":1,"lon":2,"name":"Testville","cc":"GB"},"wunit":"C"}' > $D/settings.json
touch $D/stats-on $D/lsjs; printf '%s' '{"ok":true,"id":"abc","on":true,"ready":true,"installed":true,"active":false,"failed":false}' > $D/shell.json
# the probe: the sign-in screen in an app's window (#app=), on each frame of its first 12 s; the page's notices counted
cat > $D/inject.js <<'E'
(()=>{window.__nt=[];if(!/^#app=/.test(location.hash))return;const L=[];window.__lgSeen=L;const t0=performance.now();let n=0;
function f(){n++;const e=document.getElementById('login');if(e){const c=getComputedStyle(e);if(c.display!='none'&&c.visibility!='hidden'&&+c.opacity>0&&e.getClientRects().length)L.push(Math.round(performance.now()-t0))}
if(performance.now()-t0<12000)requestAnimationFrame(f);else window.__lgFrames=n}requestAnimationFrame(f)})();
E
python3 $HERE/fixtures/native_rd.py $PORT $D > $T/rd.log 2>&1 & RDP=$!
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=$DSP LIBGL_ALWAYS_SOFTWARE=1
trap 'kill $NP $RDP $XV 2>/dev/null; pkill -f "$T/" 2>/dev/null; rm -rf $T' EXIT
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
w8 'xdotool getmouselocation >/dev/null 2>&1' 40
mkfifo $D/ev
# (2) a sound server of its own (a null sink), when this computer has PulseAudio: the shell's sounds and the windows' recorded
PA=; if command -v pulseaudio >/dev/null && command -v parec >/dev/null; then export XDG_RUNTIME_DIR=$T/xdg; mkdir -p -m 700 $XDG_RUNTIME_DIR
	mkdir -p $T/pahome; HOME=$T/pahome pulseaudio -n --daemonize=yes --exit-idle-time=-1 --use-pid-file=no -L module-stream-restore -L "module-null-sink sink_name=n160" -L "module-native-protocol-unix auth-anonymous=1 socket=$XDG_RUNTIME_DIR/pulse/native" >/dev/null 2>&1
	export PULSE_SERVER=unix:$XDG_RUNTIME_DIR/pulse/native; w8 'pactl info >/dev/null 2>&1' 40 && pactl set-default-sink n160 && PA=1; fi
# (the app windows' media stream: WebKit's, not a WebAudio one: "CORKED MUTE VOLUME", the last such)
wkmedia() { pactl list sink-inputs | awk '/^Sink Input/{if(b&&!w)print c" "m" "v; b=0;w=0;c="";m="";v=""} /Corked:/{c=$2} /Mute:/{m=$2} /^\tVolume:/{v=$5} /application.process.binary = "WebKitWebProcess"/{b=1} /media.role = "webaudio"/{w=1} END{if(b&&!w)print c" "m" "v}' | tail -1; }
rec() { [ -n "$PA" ] || return; parec -d n160.monitor --format=s16le --rate=44100 --channels=1 > $T/rec.$1 & echo $! > $T/rec.pid; }
recstop() { [ -n "$PA" ] || return; kill $(cat $T/rec.pid) 2>/dev/null; wait $(cat $T/rec.pid) 2>/dev/null; python3 -c "import array,sys;a=array.array('h',open(sys.argv[1],'rb').read()[:-1] if len(open(sys.argv[1],'rb').read())%2 else open(sys.argv[1],'rb').read());print(max([abs(x) for x in a] or [0]))" $T/rec.$1; }
start() { (cd $D && export HOME=$D $N160_ENV && RUDOLF_WX_BASE=http://127.0.0.1:9 RUDOLF_WX_GEO=http://127.0.0.1:9 RUDOLF_NATIVE_STATE=$D/state RUDOLF_NATIVE_TEST_EVAL=$D/ev exec $BIN --size=1280x800 --draw=soft --base=http://127.0.0.1:$PORT >> $T/native.log 2>&1) & NP=$!; }
start
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
hit() { grep "^hit $1 $2 " $D/state | head -1 | awk '{print $4+int($6/2), $5+int($7/2)}'; }
# a page's answer (RUDOLF_NATIVE_TEST_EVAL: "KEY JS" in, "KEY RESULT" out)
ev() { n0=$(cat $D/ev.out 2>/dev/null | wc -l); printf '%s %s\n' "$1" "$2" > $D/ev; w8 "[ \$(cat $D/ev.out 2>/dev/null | wc -l) -gt $n0 ]" 40; tail -n +$((n0+1)) $D/ev.out | head -1 | cut -d' ' -f2-; }
napps() { grep -c "^app $1 " $D/state 2>/dev/null; }
open_app() { k=$1; q=$2; n0=$(napps $k); xdotool mousemove $(hit start -) click 1; timeout 1 tail -f /dev/null; xdotool type --delay 30 "$q"; timeout 0.6 tail -f /dev/null; xdotool key Return
	w8 "[ \$(napps $k) -gt $n0 ]" 120; }
# toasts seen (the shell's pop-ups), sampled
TL=$T/toasts; : > $TL; watch_toasts() { while :; do grep '^toast ' $D/state 2>/dev/null | sed 's/^[^|]*|//' | while read -r t; do echo "$(date +%s) $t"; done >> $TL; timeout 0.5 tail -f /dev/null; done; }
watch_toasts & WP=$!; trap 'kill $WP $NP $RDP $XV 2>/dev/null; [ -n "$PA" ] && pactl exit 2>/dev/null; pkill -f "$T/" 2>/dev/null; rm -rf $T' EXIT
pops() { awk -v p="$1" 'index($0,p){t=$1; if(!l||t-l>20)n++; l=t} END{print n+0}' $TL; }
w8 '[ "$(st signin)" = 1 ]' 120
rec login; LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return
w8 '[ "$(st user)" = Ann ] && [ "$(st signin)" = 0 ]' 120
ok "0. signed in" "$([ "$(st user)" = Ann ] && echo 1)" "$(head -5 $D/state)"
ok "2a. the shell's own sounds as the page's: the start-up chime at the sign-in screen, the sign-in sound" "$(st sounds | grep -q 'startup:1 login:1 ' && echo 1)" "$(st sounds)"
timeout 1.5 tail -f /dev/null; L=$(recstop login)
[ -n "$PA" ] && ok "2b. ...heard (PulseAudio: the loudest sample $L)" "$([ "${L:-0}" -gt 1000 ] && echo 1)" "$L"
# ---- 4. the widgets after signing in
w8 'st mon | grep -q "^12%|"' 40
ok "4a. the system monitor shows the stats (CPU, memory) with its timer running" "$(st mon | grep -q '^12%|1024 / 4096 MB|[0-9]*|1$' && echo 1)" "$(st mon)"
w8 '[ "$(st weather | cut -c1)" = 3 ]' 160
ok "4b. the weather: its answer or its error (here no connection), not Loading…" "$([ "$(st weather | cut -c1)" = 3 ] && echo 1)" "$(st weather)"
# ---- 5. the notice after an update, from the shell's start-up page
w8 '[ "$(pops "Rudolf OS was updated")" -ge 1 ]' 120
ok "5a. the notice after an update pops up once after signing in (no app window open)" "$([ "$(pops 'Rudolf OS was updated')" = 1 ] && echo 1)" "$(cat $TL | tail -5; grep do-start $D/calls | head -2)"
# ---- 1. app windows opening: no sign-in screen in any frame
open_app notepad notepad; timeout 13 tail -f /dev/null
o=$(ev notepad 'JSON.stringify({seen:__lgSeen,frames:__lgFrames||0,opened:OA.opened,user:USER&&USER.n})')
ok "1a. Notepad's window: no frame with the sign-in screen while it signed in, the app open" "$(echo "$o" | grep -q '"seen":\[\],"frames":[1-9][0-9]*,"opened":1,"user":"Ann"' && echo 1)" "$o"
open_app calc calculator; open_app settings settings; timeout 13 tail -f /dev/null
o=$(ev calc 'JSON.stringify(__lgSeen)'); o2=$(ev settings 'JSON.stringify(__lgSeen)')
ok "1b. ...nor Calculator's nor Settings' window" "$([ "$o" = '[]' ] && [ "$o2" = '[]' ] && echo 1)" "$o | $o2"
o=$(ev notepad 'getComputedStyle(document.getElementById("login")).display+"|"+document.documentElement.classList.contains("oalg")')
ok "1c. ...and the sign-in screen is not left hidden by the window's first-paint rule (it is the page's own none)" "$([ "$o" = 'none|false' ] && echo 1)" "$o"
# ---- 2. a sound in an app's window (WebKit), recorded
if [ -n "$PA" ]; then rec page
	o=$(ev notepad '(()=>{const c=new AudioContext(),o=c.createOscillator();o.frequency.value=440;o.connect(c.destination);o.start();setTimeout(()=>{o.stop();c.close()},3500);return c.state})()')
	timeout 0.8 tail -f /dev/null; [ -n "$N160_DEBUG" ] && { pactl list sink-inputs | grep -E 'Sink Input|binary|application.name|media.role|Corked|Mute'; P=$(pgrep -f WebKitWebProcess | head -1); tr '\0' '\n' < /proc/$P/environ | grep -E 'PULSE|XDG|GST'; grep -oE '[^ ]*(gstpulse|gstalsa|libpulse|gstautodetect)[^ ]*' /proc/$P/maps | sort -u; pgrep -a bwrap; }
	timeout 3.5 tail -f /dev/null; L=$(recstop page); ok "2c. an app window's own sound (WebKit's): heard (the loudest sample $L)" "$([ "${L:-0}" -gt 1000 ] && echo 1)" "$o $L"; fi
# ---- 2. a media element's sound in an app's window (the Media Player's way: <audio>), its stream playing (not corked) and heard
if [ -n "$PA" ]; then rec media
	o=$(ev notepad '(()=>{const n=44100*3,b=new ArrayBuffer(44+n*2),v=new DataView(b),s=(o,t)=>{for(let i=0;i<t.length;i++)v.setUint8(o+i,t.charCodeAt(i))};s(0,"RIFF");v.setUint32(4,36+n*2,true);s(8,"WAVEfmt ");v.setUint32(16,16,true);v.setUint16(20,1,true);v.setUint16(22,1,true);v.setUint32(24,44100,true);v.setUint32(28,88200,true);v.setUint16(32,2,true);v.setUint16(34,16,true);s(36,"data");v.setUint32(40,n*2,true);for(let i=0;i<n;i++)v.setInt16(44+i*2,Math.sin(i*2*Math.PI*660/44100)*12000,true);const a=new Audio(URL.createObjectURL(new Blob([b],{type:"audio/wav"})));window.__ta=a;a.play().then(()=>window.__tp="playing",e=>window.__tp="refused "+e.name);return 1})()')
	cs=""; for i in 1 2 3 4; do timeout 0.8 tail -f /dev/null; cs="$cs $(wkmedia | cut -d' ' -f1)"; done
	timeout 1 tail -f /dev/null; L=$(recstop media); o2=$(ev notepad 'window.__tp+"|"+__ta.currentTime.toFixed(1)')
	ok "2d. an app window's <audio> (the Media Player's kind): its stream plays, not corked, and is heard (the loudest sample $L)" "$(echo "$cs" | grep -q 'no' && [ "${L:-0}" -gt 1000 ] && echo 1)" "corked:$cs | $o2 | $L"; fi
# ---- 2. a page that muted its own sound (a muted video, the Media Player's mute, an app's volume at 0): later sounds still heard
if [ -n "$PA" ]; then
	W8='(()=>{const n=44100*4,b=new ArrayBuffer(44+n*2),v=new DataView(b),s=(o,t)=>{for(let i=0;i<t.length;i++)v.setUint8(o+i,t.charCodeAt(i))};s(0,"RIFF");v.setUint32(4,36+n*2,true);s(8,"WAVEfmt ");v.setUint32(16,16,true);v.setUint16(20,1,true);v.setUint16(22,1,true);v.setUint32(24,44100,true);v.setUint32(28,88200,true);v.setUint16(32,2,true);v.setUint16(34,16,true);s(36,"data");v.setUint32(40,n*2,true);for(let i=0;i<n;i++)v.setInt16(44+i*2,Math.sin(i*2*Math.PI*550/44100)*12000,true);return URL.createObjectURL(new Blob([b],{type:"audio/wav"}))})()'
	ev notepad "(()=>{const a=new Audio($W8);window.__m1=a;a.play();setTimeout(()=>{a.muted=true;a.volume=0},700);setTimeout(()=>a.pause(),2000);return 1})()" >/dev/null
	timeout 3.5 tail -f /dev/null; rec mute2
	ev notepad "(()=>{const a=new Audio($W8);window.__m2=a;a.play();return 1})()" >/dev/null
	timeout 1.5 tail -f /dev/null; [ -n "$N160_DEBUG" ] && pactl list sink-inputs | grep -E 'Sink Input|Volume:|Mute|role|application.name|binary|restore'; mv=$(wkmedia | awk '{print $2"/"$3}'); timeout 2 tail -f /dev/null; L=$(recstop mute2)
	ev notepad "(()=>{__m2.pause();return 1})()" >/dev/null; timeout 1.5 tail -f /dev/null
	ev notepad "(()=>{const a=new Audio($W8);window.__m3=a;a.volume=.1;a.play();return 1})()" >/dev/null; timeout 0.7 tail -f /dev/null; rec vol; timeout 2 tail -f /dev/null; LV=$(recstop vol); ev notepad "__m3.pause(),1" >/dev/null
	ok "2f. ...and a page's own volume still applies to its sound (10 %: the loudest sample $LV, under a fifth of the full one's)" "$([ "${LV:-0}" -gt 100 ] && [ "${LV:-0}" -lt 2400 ] && echo 1)" "$LV"
	ok "2e. after a page muted a sound (a muted video, the Media Player's mute), the next sound plays at its own volume, heard (mute/volume: $mv, the loudest sample $L)" "$(echo "$mv" | grep -q '^no/[1-9]' && [ "${L:-0}" -gt 1000 ] && echo 1)" "$mv $L"; fi
# ---- 5. still once, with three windows opened; never "Real browser tabs are ready"
timeout 8 tail -f /dev/null
ok "5b. the update notice still popped up once with three app windows opened" "$([ "$(pops 'Rudolf OS was updated')" = 1 ] && echo 1)" "$(cat $TL | sort -u -k2 | head)"
ok "5c. no \"Real browser tabs are ready\" on Native" "$([ "$(pops 'Real browser tabs')" = 0 ] && echo 1)" "$(grep -i 'real browser' $TL | head -3)"
# ---- 4. the windows counted
w8 'st mon | grep -q "|3|1$"' 20
ok "4c. the system monitor counts the open windows (3)" "$(st mon | grep -q '|3|1$' && echo 1)" "$(st mon) $(grep '^app ' $D/state)"
# ---- 3. a setting saved in Settings: the other window and the shell at once
: > $D/calls
o=$(ev settings '(()=>{S.accent="#e11d48";SC=cln(S);persistS();try{applySet()}catch(e){}return S.accent})()')
t0=$(date +%s%N); w8 '[ "$(ev notepad "S.accent")" = "#e11d48" ]' 8; t1=$(date +%s%N)
ok "3a. Settings' window saved the accent: Notepad's window has it within a second" "$([ "$(ev notepad 'S.accent')" = '#e11d48' ] && [ $(( (t1 - t0) / 1000000 )) -lt 1500 ] && echo 1)" "$(ev notepad 'S.accent') $(( (t1 - t0) / 1000000 )) ms"
w8 'grep -q "^POST lsync" $D/calls && grep -A99 "^POST lsync" $D/calls | grep -q "^GET desktop/settings"' 8
ok "3b. ...and the shell was told: the page's storage sent to rudolfd, then the settings read again at once" "$(grep -q '^POST lsync' $D/calls && grep -A99 '^POST lsync' $D/calls | grep -q '^GET desktop/settings' && echo 1)" "$(head -20 $D/calls)"
o=$(cat $D/lsync.log)
ok "3c. ...sending only what changed (Native: rudolfd puts it into its copy, so another engine's page cannot put older values back)" "$(echo "$o" | grep -q '^part .*bd95set:uAnnA1b2' && ! echo "$o" | grep -q '^all' && echo 1)" "$(echo "$o" | tail -5)"
# a window that missed a change (another window wrote the storage, no event reached it) saves its settings: the other's value is kept
o=$(ev notepad '(()=>{const k="bd95set:"+uid(),v=JSON.parse(localStorage.getItem(k));v.tbstyle="glass";localStorage.setItem(k,JSON.stringify(v));S.h24=!S.h24;if(SC)SC.h24=S.h24;persistS();const w=JSON.parse(localStorage.getItem(k));return [w.tbstyle,w.h24===S.h24,S.tbstyle,w.accent].join("|")})()')
ok "3d. a window that missed another's change and then saves its own keeps the other's (no older value written back)" "$(echo "$o" | grep -q '^glass|true|glass|#e11d48$' && echo 1)" "$o"
w8 '[ "$(ev settings "S.tbstyle")" = glass ]' 8
ok "3e. ...and Settings' window took that window's change too" "$([ "$(ev settings 'S.tbstyle')" = glass ] && echo 1)" "$(ev settings 'S.tbstyle')"
# a window whose shared storage has not caught up yet (WebKit's storage reached the other window's process seconds later in QEMU):
# told by the shell, it takes rudolfd's copy, which the saving window sent first
python3 -c 'import json,sys;p=sys.argv[1];f=json.load(open(p));k="bd95set:uAnnA1b2";v=json.loads(f["data"][k]);v["accent"]="#22c55e";f["data"][k]=json.dumps(v);json.dump(f,open(p,"w"))' $D/lsfile.json
o=$(ev notepad '(window.__oaSetIn(),1)'); w8 '[ "$(ev notepad "S.accent")" = "#22c55e" ]' 5
ok "3f. told by the shell before the shared storage reached it, a window takes rudolfd's copy (the newer settings)" "$([ "$(ev notepad 'S.accent')" = '#22c55e' ] && grep -q '^get bd95set:uAnnA1b2' $D/lsync.log && echo 1)" "$(ev notepad 'S.accent') $(tail -3 $D/lsync.log)"
# ---- 6. Ctrl+Alt+T in an app's window
t0n=$(napps terminal); ev notepad 'OA.main.focus(),1' >/dev/null; W=$(grep '^app notepad ' $D/state | awk '{print $3}'); xdotool windowactivate --sync $((0x$W)) 2>/dev/null; timeout 0.5 tail -f /dev/null; xdotool key ctrl+alt+t
w8 "[ \$(napps terminal) -gt $t0n ]" 60
o=$(ev notepad 'wins.length')
ok "6. Ctrl+Alt+T in Notepad's window opens the Terminal's own window, not a second one inside Notepad's" "$([ $(napps terminal) -gt $t0n ] && [ "$o" = 1 ] && echo 1)" "$(grep '^app ' $D/state) | Notepad's windows: $o"
# ---- 4. the native desktop started again, the session going on: the widgets' timers and values again
kill $NP; wait $NP 2>/dev/null; rm -f $D/state; start
w8 '[ "$(st user)" = Ann ]' 120; w8 'st mon | grep -q "^12%|"' 40
ok "4d. started again with the session going on: the system monitor's timer and values (not CPU: —, Windows: 0 for good)" "$(st mon | grep -q '^12%|1024 / 4096 MB|0|1$' && echo 1)" "$(st mon)"
w8 '[ "$(st weather | cut -c1)" = 3 ]' 160
ok "4e. ...and the weather's look-up (not Loading… for good)" "$([ "$(st weather | cut -c1)" = 3 ] && echo 1)" "$(st weather)"
timeout 6 tail -f /dev/null
ok "5d. ...and the update notice did not pop up again" "$([ "$(pops 'Rudolf OS was updated')" = 1 ] && echo 1)" "$(cat $TL | sort -u -k2 | head) | copy: $(node -e "const l=JSON.parse(require('fs').readFileSync('$D/lsfile.json'));console.log(l.gen,JSON.parse(l.data['bd95set:uAnnA1b2']||'{}').seenRel)" 2>&1)"
open_app notepad notepad; w8 'st mon | grep -q "|1|1$"' 20
ok "4f. ...a window opened: counted" "$(st mon | grep -q '|1|1$' && echo 1)" "$(st mon)"
# ---- 1. the page opened by hand (no native session) still asks; the desktop's own page shows its sign-in at once (Chromium)
CH=$(ls -d /opt/pw-browsers/chromium-*/chrome-linux*/chrome 2>/dev/null | tail -1)
if node -e "require('playwright')" 2>/dev/null; then
	node -e '
const {chromium}=require("playwright");(async()=>{const b=await chromium.launch();const p=await b.newPage();
 await fetch("http://127.0.0.1:'$PORT'/api/desktop/logout",{method:"POST",headers:{"X-Rudolf-Token":"tok59native"}});
 const r=[];for(const h of ["","#app=notepad"]){await p.goto("http://127.0.0.1:'$PORT'/"+h);
  const v=await p.evaluate(()=>new Promise(res=>{const t0=performance.now();const f=()=>{const e=document.getElementById("login");const s=e&&getComputedStyle(e).display!="none";if(s||performance.now()-t0>25000)res(s?Math.round(performance.now()-t0):-1);else requestAnimationFrame(f)};f()}));r.push(v)}
 console.log(r.join(" "));await b.close()})().catch(e=>console.log("ERR "+e.message))' > $T/pw.txt 2>&1
	o=$(cat $T/pw.txt); a=${o% *}; b=${o#* }
	ok "1d. no native session: the desktop's page shows its sign-in screen at once, an app's page after finding none (and within 20 s)" "$([ "$a" -ge 0 ] 2>/dev/null && [ "$a" -lt 3000 ] && [ "$b" -ge 0 ] 2>/dev/null && [ "$b" -lt 21000 ] && echo 1)" "$o"
else echo "SKIP 1d: no Playwright"; fi
echo "$((N-F))/$N passed"; [ $F = 0 ]
