#!/bin/sh
# 1.5.9 (docs/plan-1.5.9.md 12.3c): Oracle on the native desktop runs in Rudolf's own Chromium with the shell's real tabs,
# as on Standard. rudolf-native under Xvfb with a stand-in rudolfd (qt/host/fixtures/native_rd.py) serving the real page,
# the shell (iso/shell) built with a test key, and a Chromium with Isolated Web Apps (CHROME; Playwright's by default):
# Oracle opened from its pin -> one Chromium (--class=RudolfOracle, through rudolf-launch --oracle), its window framed by
# the native desktop as Oracle (no title bar of Chromium's own), the page in one-app mode with the shell's real tabs
# (rtOn()), a local site in a real tab (a Controlled Frame), a second window (New private tab) in the same Chromium, the
# last window closed -> no Chromium left; the site's cookie kept in the profile (seen when Oracle opens again); without
# Chromium, Oracle's WebKit window as before, said once. (Step 5) another app asked for from Oracle opens in a window of
# its own, through rudolf-launch's binding (rudolfNativeOracle), which no website tab has; its notifications are the native
# shell's pop-ups, their click its own action; a website's full screen covers the screen.
# (Step 5's rest) the desktop page's own full screen covers the screen; Oracle's keys as on Standard (Ctrl+W in a website
# closes its tab, not the window; Ctrl+T; Ctrl+Shift+N a private window of its own; Ctrl+Q does not quit Chromium); the
# clipboard between Oracle and a WebKit window, both ways; the devlink's screenshot (rudolf-display --shot) shows Oracle;
# a website's download filed by Oracle's page into the account's Downloads (Rudolf OS's Chromium policy), with the
# notification and about:downloads as on Standard (nothing on the taskbar), once; Open with from Oracle a window of its
# own with the file; the microphone in a call and the volume (a real sound server through Rudolf OS's hotkey, when
# PulseAudio is there); a website's file dialog: none with Rudolf OS's policy, as on Standard, and without it Chromium's
# dialog a plain window centred over Oracle's (not framed or cut as Oracle).
# Runs in network and mount namespaces of its own (the shell's desktop is http://127.0.0.1:8080; Rudolf OS's Chromium
# policy and /run/rudolf in a private mount). Needs root, Xvfb, xdotool, ImageMagick, node, python3, gcc, ip and
# WebKitGTK's headers; PulseAudio (pulseaudio, pactl) for the microphone and the volume.   sh oracle_native_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root (a network namespace)"; exit 0; }
[ -z "$ON59_INNER" ] && exec env ON59_INNER=1 unshare -n -m sh "$0" "$@"
command -v Xvfb >/dev/null && command -v xdotool >/dev/null && command -v convert >/dev/null && command -v ip >/dev/null || { echo "SKIP: needs Xvfb, xdotool, ImageMagick and ip"; exit 0; }
CHROME=${CHROME:-$(ls -d /opt/pw-browsers/chromium-*/chrome-linux*/chrome 2>/dev/null | tail -1)}
[ -x "$CHROME" ] || { echo "SKIP: no Chromium (CHROME)"; exit 0; }
HERE=$(cd "$(dirname "$0")" && pwd); REPO=$(cd "$HERE/../../.." && pwd); T=$(mktemp -d /tmp/oranat.XXXXXX); DSP=${ON59_DSP:-:86}
ip link set lo up
# Rudolf OS's Chromium policy (downloads into /run/rudolf/dl/.incoming; websites open no file dialog), in this mount namespace
# only: Chromium reads /etc/chromium/policies, Playwright's Chrome for Testing /etc/opt/chrome_for_testing/policies
mount --make-rprivate / 2>/dev/null; mount -t tmpfs none /run && mkdir -p /run/rudolf/dl/.incoming || { echo "FAIL /run"; exit 1; }
POL=""; for p in /etc/chromium/policies/managed /etc/opt/chrome_for_testing/policies/managed; do [ -d $p ] && mount -t tmpfs none $p && cp $REPO/iso/sys/root/etc/chromium/policies/managed/rudolf.json $p/ && POL="$POL $p"; done
trap 'kill $NP $RDP $XV $SP 2>/dev/null; pkill -f "$T/" 2>/dev/null; timeout 1 tail -f /dev/null; pkill -9 -f "$T/" 2>/dev/null; rm -rf $T' EXIT
gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $T/rudolf-native $REPO/iso/sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
gcc -O2 -Wall -o $T/rudolf-launch $REPO/iso/sys/rudolf-launch.c 2>>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
grep -q warning $T/cc && { cat $T/cc; echo "FAIL build warnings"; exit 1; }
# the shell, signed with a test key, installed from its file as the session does in developer mode
python3 $REPO/iso/systools/mkiwa.py key $T/test.key >/dev/null; ID=$(python3 $REPO/iso/systools/mkiwa.py id $T/test.key)
python3 $REPO/iso/systools/mkiwa.py build $REPO/iso/shell $T/shell.swbn $T/test.key >/dev/null || { echo "FAIL the shell"; exit 1; }
D=$T/d; mkdir -p $D; node $REPO/iso/qt/host/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
echo $ID > $D/iwa-id
# (the stand-in as rudolfd: Chromium's download folder, the databases' mirror; the sound server through the hotkey script)
echo /run/rudolf/dl/.incoming > $D/dl-dir; mkdir -p $D/idbm
if command -v pulseaudio >/dev/null && command -v pactl >/dev/null; then mkdir -p $T/pa/h $T/pa/r; chmod 700 $T/pa/r
	HOME=$T/pa/h XDG_RUNTIME_DIR=$T/pa/r pulseaudio -n --daemonize=no --exit-idle-time=-1 --use-pid-file=no -L "module-null-sink sink_name=out" \
		-L "module-null-source source_name=mic" -L "module-native-protocol-unix socket=$T/pa/s auth-anonymous=1" >$T/pa/log 2>&1 &
	export PULSE_SERVER=unix:$T/pa/s; i=0; while [ $i -lt 40 ] && ! pactl info >/dev/null 2>&1; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
	pactl info >/dev/null 2>&1 && echo $REPO/iso/sys/root/usr/lib/rudolf/hotkey > $D/hotkey; fi
# rudolf-display, for the devlink's screenshot (rudolfd's /api/screenshot: rudolf-display --shot)
L=/usr/lib/x86_64-linux-gnu; gcc -O2 -w -o $T/rdisp $REPO/iso/sys/rudolf-display.c -I$REPO/iso/sys -lX11 $L/libXtst.so.6 $L/libXfixes.so.3 $L/libXrandr.so.2 -lm 2>>$T/cc || { echo "FAIL build rudolf-display"; exit 1; }
# (a window's place on the screen)
printf '%s\n' '#include <X11/Xlib.h>' '#include <stdio.h>' '#include <stdlib.h>' 'int main(int c,char**v){Display*d=XOpenDisplay(0);int x,y;Window k;if(!d||c<2)return 1;XTranslateCoordinates(d,strtoul(v[1],0,0),DefaultRootWindow(d),0,0,&x,&y,&k);printf("%d %d\n",x,y);return 0;}' > $T/xabs.c
gcc -o $T/xabs $T/xabs.c -lX11 || { echo "FAIL build xabs"; exit 1; }
printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","b2":"#000080","tb":"bottom","accent":"#2563eb"},"pins":["notepad","browser"],"notes":0,"wallpaper":false}' > $D/settings.json
# a website: it keeps a cookie
mkdir -p $T/site; printf '%s' '<!doctype html><title>Test site</title><p id=p>site</p><script>if(!document.cookie.includes("oracle59="))document.cookie="oracle59=kept; max-age=86400; path=/"</script>' > $T/site/index.html
# another page: a file input, a text box, a download (a page of one colour, for the screenshot)
printf '%s' '<!doctype html><title>Upload</title><body style="margin:0;background:#33aa77"><input type=file id=f style="position:fixed;left:20px;top:20px"><textarea id=t style="position:fixed;left:20px;top:80px;width:300px;height:80px"></textarea><a id=dl href="/notes59.txt" download style="position:fixed;left:20px;top:180px">download</a></body>' > $T/site/up.html
printf 'A download from Oracle.\n' > $T/site/notes59.txt
(cd $T/site && exec python3 -m http.server 8333 --bind 127.0.0.1 >/dev/null 2>&1) & SP=$!
# what the session writes for rudolf-native (Standard's flags; --no-sandbox as root here)
cmdf() { printf '%s\n' $ID "$CHROME" --no-sandbox --class=RudolfOracle --user-data-dir=$T/prof --no-first-run --no-default-browser-check --password-store=basic \
	--enable-features=SharedArrayBuffer,IsolatedWebApps,IsolatedWebAppDevMode --install-isolated-web-app-from-file=$T/shell.swbn --disable-gpu \
	--autoplay-policy=no-user-gesture-required --use-fake-ui-for-media-stream --remote-debugging-port=9335 > $T/oracle-chrome; }
cmdf
python3 $REPO/iso/qt/host/fixtures/native_rd.py 8080 $D > $T/rd.log 2>&1 & RDP=$!
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=$DSP LIBGL_ALWAYS_SOFTWARE=1
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
w8 'xdotool getmouselocation >/dev/null 2>&1' 40
mkdir -p $T/run; chmod 700 $T/run
(cd $D && HOME=$D XDG_RUNTIME_DIR=$T/run RUDOLF_NATIVE_STATE=$D/state RUDOLF_ORACLE_CMD=$T/oracle-chrome RUDOLF_LAUNCH=$T/rudolf-launch exec $T/rudolf-native --size=1280x800 --draw=soft --base=http://127.0.0.1:8080 > $T/native.log 2>&1) & NP=$!
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
hit() { grep "^hit $1 $2 " $D/state | head -1 | awk '{print $4+int($6/2), $5+int($7/2)}'; }
# (DevTools: the pages in Chromium; the shell itself cannot be reached, its desktop frame and tabs can)
ev() { node -e '
const [pre, expr, one] = process.argv.slice(1);
(async () => { const l = await (await fetch("http://127.0.0.1:9335/json/list")).json(); const ts = l.filter(x => x.url.startsWith(pre)).slice(0, one ? 1 : 99); if (!ts.length) { console.log("NO TARGET"); return }
  const r = []; for (const t of ts) r.push(await new Promise(res => { const ws = new WebSocket(t.webSocketDebuggerUrl); const to = setTimeout(() => res("TIMEOUT"), 8000);
    ws.onopen = () => ws.send(JSON.stringify({id: 1, method: "Runtime.evaluate", params: {expression: expr, returnByValue: true, awaitPromise: true, userGesture: true}}));
    ws.onmessage = e => { const m = JSON.parse(e.data); if (m.id == 1) { clearTimeout(to); ws.close(); res(m.result.result ? m.result.result.value : m.result) } } }));
  console.log(JSON.stringify(r.length == 1 ? r[0] : r)) })().catch(e => console.log("ERR " + e.message))' "$1" "$2" "$3" 2>&1; }
targets() { node -e 'fetch("http://127.0.0.1:9335/json/list").then(r=>r.json()).then(l=>console.log(l.map(t=>t.type+" "+t.url).join("\n"))).catch(()=>{})' 2>/dev/null; }
# (Chromium's browser processes of this profile: not its helpers (--type=), not rudolf-launch)
chromes() { n=0; for p in $(pgrep -f -- "--user-data-dir=$T/prof"); do c=$( { tr '\0' ' ' < /proc/$p/cmdline; } 2>/dev/null); case "$c" in *--type=*|*rudolf-launch*|'') ;; *) n=$((n+1));; esac; done; echo $n; }
w8 '[ "$(st signin)" = 1 ]' 120
PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)"); LC_ALL=C.UTF-8 xdotool type --delay 30 "$PW"; xdotool key Return
w8 '[ "$(st user)" = Ann ] && grep -q "^hit pin browser" $D/state' 120 || { echo "FAIL signing in"; tail $T/native.log; exit 1; }
# ---- Oracle from its pin
xdotool mousemove $(hit pin browser) click 1
w8 'grep -q "^client .* app=browser" $D/state' 120
CL=$(grep "^client .* app=browser" $D/state | head -1)
ok "1. Oracle from its pin: one Chromium (--class=RudolfOracle, through rudolf-launch --oracle)" "$([ -n "$CL" ] && [ "$(chromes)" = 1 ] && [ "$(st oracle | cut -d' ' -f1)" = 1 ] && echo 1)" "$CL | $(st oracle) | $(pgrep -af RudolfOracle | head -3) | $(tail -5 $T/native.log)"
ok "...not a WebKit window (no app window of rudolf-native's own for Oracle)" "$(grep -q '^app browser ' $D/state && echo 0 || echo 1)" "$(grep '^app ' $D/state)"
w8 '[ "$(ev http://127.0.0.1:8080/ "typeof rtOn==\"function\"&&rtOn()&&!!OA.main")" = true ]' 120
o=$(ev http://127.0.0.1:8080/ '[ONEAPP&&ONEAPP.k,rtOn(),innerWidth,innerHeight,location.hash.slice(0,12)]')
ok "2. the page in the shell: Oracle alone (one-app mode, the shell's # passed on) with the shell's real tabs (rtOn)" "$(echo "$o" | grep -q '^\["browser",true,[0-9]*,[0-9]*,"#app=browser"\]' && echo 1)" "$o $(targets)"
w8 'grep -q "^client .* app=browser Oracle" $D/state' 60
CL=$(grep "^client .* app=browser" $D/state | head -1); set -- $CL; CX=$4; CY=$5; CW=$6; CH=$7
ok "3. framed by the native desktop as Oracle: the page's title (through the shell), the taskbar's button" "$(echo "$CL" | grep -q 'app=browser Oracle$' && grep -q '^hit win ' $D/state && echo 1)" "$CL"
iw=$(echo "$o" | cut -d, -f3); ih=$(echo "$o" | cut -d, -f4)
ok "...the page has the whole window inside the frame (Chromium's own title bar and edges cut away)" "$([ "$iw" = "$CW" ] && [ "$ih" = "$CH" ] && echo 1)" "page ${iw}x$ih, window ${CW}x$CH"
import -window root $T/s1.png 2>/dev/null
p=$(convert $T/s1.png -format "%[fx:int(255*p{$((CX+300)),$((CY+36))}.r)] %[fx:int(255*p{$((CX+300)),$((CY+36))}.g)] %[fx:int(255*p{$((CX+300)),$((CY+36))}.b)]" info:)
ok "...no title bar of Chromium's own below the frame's (its light blue)" "$([ "$p" != '211 227 253' ] && echo 1)" "$p"
# ---- a website in a real tab
ev http://127.0.0.1:8080/ 'webOpen("http://127.0.0.1:8333/"),1' >/dev/null
w8 'targets | grep -q "^webview http://127.0.0.1:8333/"' 80
ok "4. a website opened in Oracle is a real tab (a Controlled Frame of the shell), not a frame of the page" "$(targets | grep -q '^webview http://127.0.0.1:8333/' && echo 1)" "$(targets)"
w8 '[ "$(ev http://127.0.0.1:8333/ "document.cookie")" = "\"oracle59=kept\"" ]' 40
ok "...it keeps its cookie" "$([ "$(ev http://127.0.0.1:8333/ 'document.cookie')" = '"oracle59=kept"' ] && echo 1)" "$(ev http://127.0.0.1:8333/ 'document.cookie')"
# ---- (step 5) what the page tells the native desktop: another app asked for from Oracle is a window of its own
o=$(ev http://127.0.0.1:8080/ 'typeof rudolfNativeOracle')
ev http://127.0.0.1:8080/ 'launch("notepad"),1' >/dev/null
w8 'grep -q "^app notepad " $D/state' 120
ok "4b. another app opened from Oracle (rudolf-launch's binding, its private pipe): a window of its own, as from a WebKit window" "$([ "$o" = '"function"' ] && grep -q '^app notepad ' $D/state && [ "$(ev http://127.0.0.1:8080/ 'wins.filter(W=>W.app=="notepad").length')" = 0 ] && echo 1)" "$o $(grep '^app ' $D/state)"
ok "...and no website tab has the binding" "$([ "$(ev http://127.0.0.1:8333/ 'typeof rudolfNativeOracle')" = '"undefined"' ] && echo 1)" "$(ev http://127.0.0.1:8333/ 'typeof rudolfNativeOracle')"
# (Notepad closed again: its ✕, 9 px from the frame's right edge)
NL=$(grep "^client .* app=notepad" $D/state | head -1); if [ -n "$NL" ]; then set -- $NL; xdotool mousemove $(($4 + $6 + 2 - 21)) $(($5 + 18)) click 1; w8 '! grep -q "^client .* app=notepad" $D/state' 40; fi
# ---- (step 5) Oracle's notifications: the native shell's pop-ups and centre; a click does the notification's own action in its page
ev http://127.0.0.1:8080/ 'notify("Oracle test","A notification from Oracle",{action:()=>launch("calc")}),1' >/dev/null
w8 'grep -q "^toast .*|Oracle test" $D/state' 60
TL=$(grep "^toast .*|Oracle test" $D/state | head -1); NL=$(st notes)
ok "4d. a notification from Oracle's page: the native shell's pop-up in the screen's corner, counted on the bell and in the centre at once" "$([ -n "$TL" ] && [ "$(echo "$NL" | cut -d' ' -f2)" -ge 1 ] && [ "$(ev http://127.0.0.1:8080/ 'document.querySelectorAll("#ntoasts .ntoast").length')" = 0 ] && echo 1)" "$TL | notes $NL"
set -- $TL; xdotool mousemove $(($3 + $5 / 2)) $(($4 + 20)) click 1
w8 'grep -q "^app calc " $D/state' 60
ok "...its click does the notification's own action, run in Oracle's page (here: Calculator, a window of its own)" "$(grep -q '^app calc ' $D/state && ! grep -q '^toast .*|Oracle test' $D/state && echo 1)" "$(grep -E '^(app|toast)' $D/state)"
CL2=$(grep "^client .* app=calc" $D/state | head -1); if [ -n "$CL2" ]; then set -- $CL2; xdotool mousemove $(($4 + $6 + 2 - 21)) $(($5 + 18)) click 1; w8 '! grep -q "^client .* app=calc" $D/state' 40; fi
# ---- (step 5) a website's full screen (a game, a video) is the window's: the native desktop's full screen, the taskbar under it
ev http://127.0.0.1:8333/ 'document.documentElement.requestFullscreen().then(()=>1,e=>String(e))' >/dev/null
w8 'grep -q "^client .* full=1 .* app=browser" $D/state' 60
FL=$(grep "^client .* app=browser" $D/state | head -1)
ok "4c. a website in Oracle going full screen: Oracle's window covers the screen (the native desktop's full screen)" "$(echo "$FL" | grep -q ' 1280 800 min=0 max=0 full=1' && [ "$(ev http://127.0.0.1:8333/ '[innerWidth,innerHeight].join()')" = '"1280,800"' ] && echo 1)" "$FL $(ev http://127.0.0.1:8333/ '[innerWidth,innerHeight].join()')"
ev http://127.0.0.1:8333/ 'document.exitFullscreen().then(()=>1,e=>String(e))' >/dev/null
w8 'grep -q "^client .* full=0 .* app=browser" $D/state' 60
ok "...and back in its frame when it leaves" "$(grep -q '^client .* full=0 .* app=browser Oracle' $D/state && echo 1)" "$(grep '^client' $D/state)"
# ==== (step 5's rest) what the page in Chromium needs from the native desktop, each as on Standard
PG=http://127.0.0.1:8080/
# (where things are on the screen: Oracle's page starts 4, 28 px inside Chromium's window (its own edges cut away); a point in the
# front website tab, in the tab's own pixels; the front Oracle window's address box, in the page)
corg() { set -- $(grep "^client .* app=browser" $D/state | head -1); set -- $($T/xabs 0x$2); echo $(($1 + 4)) $(($2 + 28)); }
tabxy() { a=$1; b=$2; set -- $(corg); o=$(ev $PG '(()=>{let b=null;for(const[,t]of RT)if(t.shown()&&(!b||t.z()>=b.z()))b=t;if(!b)return"";const r=b.el.getBoundingClientRect();return Math.round(r.left)+" "+Math.round(r.top)})()' one | tr -d '"'); set -- $1 $2 $o; echo $(($1 + $3 + a)) $(($2 + $4 + b)); }
adxy() { set -- $(corg); o=$(ev $PG '(()=>{const W=wins.filter(W=>W.app=="browser"&&W.el.isConnected).sort((a,b)=>(+a.el.style.zIndex||0)-(+b.el.style.zIndex||0)).pop();const r=W.el.querySelector("input.ad").getBoundingClientRect();return Math.round(r.left+r.width/2)+" "+Math.round(r.top+r.height/2)})()' one | tr -d '"'); set -- $1 $2 $o; echo $(($1 + $3)) $(($2 + $4)); }
ntabs() { ev $PG 'document.querySelectorAll(".otab").length' one; }
nora() { grep -c "^client .* app=browser" $D/state; }
xclose() { set -- $(grep "^client .* app=$1" $D/state | head -1); [ -n "$1" ] || return 0; xdotool mousemove $(($4 + $6 + 2 - 21)) $(($5 + 18)) click 1; w8 "! grep -q '^client .* app=$1' $D/state" 40; }
# ---- the desktop page's own full screen (shell26.js's stand-in fills the page: on Standard the page is the screen)
ev $PG 'OA.main.el.requestFullscreen().then(()=>1,e=>String(e))' one >/dev/null
w8 'grep -q "^client .* full=1 .* app=browser" $D/state' 40
o=$(ev $PG '[innerWidth,innerHeight,!!document.fullscreenElement].join()' one)
ok "4e. the desktop page's own full screen in Oracle (its stand-in): Oracle's window covers the screen, as the page does on Standard" "$(grep -q '^client .* 1280 800 min=0 max=0 full=1 .* app=browser' $D/state && [ "$o" = '"1280,800,true"' ] && echo 1)" "$(grep '^client' $D/state) $o"
# (Esc where the page has the keyboard, as a person clicking it first: Oracle's status line, at the screen's foot in full screen;
# Chromium's own "press and hold Esc" notice covers the address box)
o=$(ev $PG 'Math.round(innerWidth/2)+" "+(innerHeight-8)' one | tr -d '"')
xdotool mousemove $o click 1; timeout 0.5 tail -f /dev/null; xdotool key Escape; w8 '! grep -q "^client .* full=1" $D/state' 40
o=$(ev $PG '[innerWidth,!!document.fullscreenElement].join()' one)
ok "...Esc: the page leaves it and the window is back in its frame" "$(grep -q '^client .* full=0 .* app=browser' $D/state && [ "$o" = "\"$CW,false\"" ] && echo 1)" "$(grep '^client' $D/state) $o"
# ---- the keys (keys56.js's rules; Chromium's own Ctrl+W, T, N, Q and Alt+F4 handed over as on Standard, rudolf-display's desk_key)
# (the website of 4, its tab with the shell's script (its title, "Test site", comes from it))
w8 '[ "$(ev $PG "[...document.querySelectorAll(\".otab\")].some(t=>t.textContent.startsWith(\"Test site\"))" one)" = true ]' 20
ev $PG 'window.__tk=[];addEventListener("message",e=>{const m=e.data||{};if(m.rudolfShell=="tabev"&&m.ev=="keys")__tk.push(m.k)});wins.filter(W=>W.app=="browser"&&W!==OA.main).pop().focus(),1' one >/dev/null
timeout 1 tail -f /dev/null; xdotool mousemove $(tabxy 500 150) click 1; timeout 1 tail -f /dev/null
n0=$(ntabs); xdotool key ctrl+t; w8 '[ "$(ntabs)" = $((n0 + 1)) ]' 20
ok "4f. Ctrl+T in a website in Oracle: a new tab (as on Standard), Chromium's own Ctrl+T not acting" "$([ "$(ntabs)" = $((n0 + 1)) ] && [ "$(nora)" = 1 ] && [ "$(chromes)" = 1 ] && echo 1)" "$n0 $(ntabs) $(nora) $(chromes) keys $(ev $PG '__tk.join()' one) at $(tabxy 500 150) $(ev $PG '[...document.querySelectorAll(".otab")].map(t=>t.textContent).join("|")' one)"
# (back to the website's tab, the front window's first: a click on it, then on the page)
set -- $(corg); o=$(ev $PG '(()=>{const W=wins.filter(W=>W.app=="browser"&&W.el.isConnected).sort((a,b)=>(+a.el.style.zIndex||0)-(+b.el.style.zIndex||0)).pop();const r=W.el.querySelector(".otab").getBoundingClientRect();return Math.round(r.left+20)+" "+Math.round(r.top+r.height/2)})()' one | tr -d '"'); set -- $1 $2 $o
xdotool mousemove $(($1 + $3)) $(($2 + $4)) click 1; timeout 1 tail -f /dev/null; xdotool mousemove $(tabxy 500 150) click 1; timeout 0.5 tail -f /dev/null; xdotool key ctrl+w
w8 '[ "$(ntabs)" = $n0 ]' 20; timeout 1 tail -f /dev/null
ok "...Ctrl+W in a website: its tab closes; Oracle's window and Chromium stay (before: Chromium closed the window, and with it Chromium)" "$([ "$(ntabs)" = $n0 ] && [ "$(nora)" = 1 ] && [ "$(chromes)" = 1 ] && ! targets | grep -q '^webview http://127.0.0.1:8333/$' && echo 1)" "$(ntabs) $(nora) $(chromes) keys $(ev $PG '__tk.join()' one) $(targets)"
xdotool mousemove $(adxy) click 1; timeout 0.5 tail -f /dev/null; xdotool key ctrl+shift+n
w8 'grep -q "^client .* app=browser .*(Private)" $D/state' 80
ok "...Ctrl+Shift+N in Oracle: a private window of its own (as on Standard), of the same Chromium" "$(grep -q '^client .* app=browser .*(Private)' $D/state && [ "$(nora)" = 2 ] && [ "$(chromes)" = 1 ] && echo 1)" "$(grep '^client' $D/state) $(chromes)"
P2=$(grep "^client .* app=browser .*(Private)" $D/state | head -1); if [ -n "$P2" ]; then set -- $P2; xdotool mousemove $(($4 + $6 - 10)) $(($5 + 17)) click 1; w8 '[ "$(nora)" = 1 ]' 40; fi
xdotool mousemove $(adxy) click 1; timeout 0.5 tail -f /dev/null; xdotool key ctrl+q; timeout 1.5 tail -f /dev/null
ok "...Ctrl+Q: Chromium does not quit (every Oracle window went with it)" "$([ "$(nora)" = 1 ] && [ "$(chromes)" = 1 ] && echo 1)" "$(nora) $(chromes)"
# ---- the clipboard: copied in Oracle, pasted in a WebKit window (Notepad), copied there, pasted back in Oracle
ev $PG 'webOpen("http://127.0.0.1:8333/up.html"),1' one >/dev/null
w8 'targets | grep -q "^webview http://127.0.0.1:8333/up.html"' 60; timeout 1.5 tail -f /dev/null
xdotool mousemove $(tabxy 100 120) click 1; timeout 0.5 tail -f /dev/null
xdotool type --delay 20 "copied in Oracle"; xdotool key ctrl+a ctrl+c; timeout 0.5 tail -f /dev/null
ev $PG 'launch("notepad"),1' one >/dev/null; w8 'grep -q "^client .* app=notepad" $D/state' 80; timeout 1.5 tail -f /dev/null
NL=$(grep "^client .* app=notepad" $D/state | head -1); set -- $NL; xdotool mousemove $(($4 + $6 / 2)) $(($5 + $7 / 2)) click 1; timeout 0.5 tail -f /dev/null
xdotool key ctrl+v; xdotool type --delay 20 " and back"; xdotool key ctrl+a ctrl+c; timeout 0.5 tail -f /dev/null; xclose notepad
ev http://127.0.0.1:8333/up.html 't.value="";1' >/dev/null; xdotool mousemove $(tabxy 100 120) click 1; timeout 0.5 tail -f /dev/null; xdotool key ctrl+v; timeout 0.5 tail -f /dev/null
o=$(ev http://127.0.0.1:8333/up.html 't.value')
ok "4g. the clipboard: copied in Oracle, pasted into Notepad (a WebKit window) and copied there, pasted back in Oracle" "$([ "$o" = '"copied in Oracle and back"' ] && echo 1)" "$o"
# ---- the devlink's screenshot (rudolfd's /api/screenshot: rudolf-display --shot, the X screen) shows Oracle's window
$T/rdisp --shot > $T/shot.ppm 2>/dev/null; set -- $(tabxy 500 150)
p=$(convert $T/shot.ppm -format "%[fx:int(255*p{$1,$2}.r)] %[fx:int(255*p{$1,$2}.g)] %[fx:int(255*p{$1,$2}.b)]" info: 2>&1)
ok "4h. the devlink's screenshot (rudolf-display --shot) shows Oracle's window with its website" "$([ "$p" = '51 170 119' ] && echo 1)" "$p at $1,$2"
# ---- a download: Chromium saves it where Rudolf OS's policy says (Downloads/.incoming); Oracle's page files it into the account's
# Downloads, about:downloads and the notification saying so, as on Standard (nothing on the taskbar there); a WebKit window open
# meanwhile leaves it to Oracle (one page takes each file)
if [ -n "$POL" ]; then
ev $PG 'launch("notepad"),1' one >/dev/null; w8 'grep -q "^client .* app=notepad" $D/state' 80; timeout 2 tail -f /dev/null
: > $D/calls; ev http://127.0.0.1:8333/up.html 'dl.click(),1' >/dev/null
w8 'grep -q "^toast .*|Downloaded" $D/state' 80; TOAST=$(grep "^toast .*|Downloaded" $D/state)
o=$(ev $PG 'JSON.stringify((DLS.filter(r=>r.n=="NOTES59.TXT").pop()||{}).st||"")+" "+JSON.stringify((node(downloadsPath())||{})["NOTES59.TXT"]||"")' one)
ok "4i. a download in Oracle: Chromium saves it in Downloads/.incoming, Oracle's page files it into the account's Downloads (about:downloads: saved)" "$(echo "$o" | grep -q 'saved.*A download from Oracle' && [ -z "$(ls -A /run/rudolf/dl/.incoming)" ] && echo 1)" "$o | $(ls -A /run/rudolf/dl/.incoming) | $(grep 'dl/' $D/calls | sort | uniq -c | head)"
ok "...its notification is the native shell's pop-up, as from any window; the native taskbar shows no download (as Standard's)" "$([ -n "$TOAST" ] && ! grep -q '^hit dl' $D/state && echo 1)" "$(grep -E '^(toast|notes)' $D/state)"
ok "...and only Oracle's page took the file (the WebKit window did not ask for it)" "$(grep -q 'dl/read.*\[chrome\]' $D/calls && ! grep -q 'dl/\(incoming\|read\|file\).*\[webkit\]' $D/calls && echo 1)" "$(grep 'dl/' $D/calls | sort | uniq -c)"
xclose notepad
# ---- Open with, from Oracle: the app chosen in Oracle's dialog opens the file in a window of its own (on Standard: a desktop window)
ev $PG 'openWith(downloadsPath().concat("NOTES59.TXT")),1' one >/dev/null
ev $PG '[...document.querySelectorAll(".rmodal .owb")].find(b=>/Notepad/.test(b.textContent)).click(),1' one >/dev/null
w8 'grep -q "^client .* app=notepad NOTES59.TXT - Notepad" $D/state' 80
ok "4k. Open with in Oracle (Notepad): the file in a window of its own, not inside Oracle's page" "$(grep -q '^client .* app=notepad NOTES59.TXT - Notepad' $D/state && [ "$(ev $PG 'wins.filter(W=>W.app=="notepad").length' one)" = 0 ] && echo 1)" "$(grep '^client' $D/state) $(ev $PG 'wins.map(W=>W.app).join()' one)"
xclose notepad
else echo "SKIP 4i, 4k: no Chromium policy folder here (/etc/chromium/policies or /etc/opt/chrome_for_testing/policies)"; fi
# ---- a call in Oracle: the microphone (rudolfd's /api/mic: the sound server's recording apps) and the volume (/api/volume)
if [ -s $D/hotkey ]; then
o=$(ev http://127.0.0.1:8333/up.html 'navigator.mediaDevices.getUserMedia({audio:true}).then(s=>{window.__s=s;return "ok"},e=>"ERR "+e)')
w8 'grep -q "^mic [1-9][0-9]* 0" $D/state && grep -q "^hit call " $D/state' 40
ok "4j. a call in Oracle (its website records from the microphone): the native taskbar's 🎙️, as for a WebKit window's call" "$([ "$o" = '"ok"' ] && grep -q '^mic [1-9][0-9]* 0' $D/state && grep -q '^hit call ' $D/state && echo 1)" "$o $(grep '^mic' $D/state) $(pactl list source-outputs short)"
xdotool mousemove $(hit call -) click 1; w8 'grep -q "^mic [1-9][0-9]* 1" $D/state' 20
ok "...its click mutes the microphone for everything (the sound server's), Oracle's call too" "$(grep -q '^mic [1-9][0-9]* 1' $D/state && pactl get-source-mute @DEFAULT_SOURCE@ | grep -q yes && echo 1)" "$(grep '^mic' $D/state) $(pactl get-source-mute @DEFAULT_SOURCE@)"
ev http://127.0.0.1:8333/up.html '(()=>{const c=new AudioContext(),o=c.createOscillator();o.connect(c.destination);o.start();window.__c=c;return c.state})()' >/dev/null
pactl set-sink-volume @DEFAULT_SINK@ 40%; timeout 1 tail -f /dev/null
xdotool mousemove $(hit vol -) click 1; w8 'grep -q "^fly vol" $D/state' 20
FLY=$(grep "^fly vol" $D/state); M=$(grep "^flybtn mute" $D/state | cut -d' ' -f3-)
ok "...Oracle's sound plays through the system's output, whose volume the taskbar's pop-up shows (40 %)" "$(pactl list sink-inputs | grep -q 'application.name = "Chromium"' && echo "$FLY" | grep -q ' 40 0$' && echo 1)" "$FLY | $(pactl list sink-inputs | grep -E 'Sink:|application.name')"
[ -n "$M" ] && xdotool mousemove $M click 1; timeout 1 tail -f /dev/null
ok "...and mutes it (Oracle's sound with it)" "$(pactl get-sink-mute @DEFAULT_SINK@ | grep -q yes && echo 1)" "$(pactl get-sink-mute @DEFAULT_SINK@) $(grep '^fly' $D/state)"
xdotool key Escape; pactl set-sink-mute @DEFAULT_SINK@ 0; pactl set-source-mute @DEFAULT_SOURCE@ 0
ev http://127.0.0.1:8333/up.html '__s.getTracks().forEach(t=>t.stop()),__c.close(),1' >/dev/null
w8 'grep -q "^mic 0" $D/state' 40
ok "...the call ended: the 🎙️ goes" "$(grep -q '^mic 0' $D/state && ! grep -q '^hit call ' $D/state && echo 1)" "$(grep '^mic' $D/state)"
else echo "SKIP 4j: no sound server (pulseaudio, pactl)"; fi
# ---- a website's file input: with Rudolf OS's policy no file dialog, as on Standard (websites cannot open the system's file dialog)
if [ -n "$POL" ]; then
n1=$(grep -c "^client " $D/state); xdotool mousemove $(tabxy 60 30) click 1; timeout 3 tail -f /dev/null
ok "4l. a website's file input in Oracle, with Rudolf OS's Chromium policy: no file dialog opens, as on Standard" "$([ "$(grep -c '^client ' $D/state)" = "$n1" ] && echo 1)" "$(grep '^client ' $D/state)"
fi
# ---- a second window: the pin's Open a new window, in the same Chromium (the pin itself brings Oracle to the front)
xdotool mousemove $(hit pin browser) click 1; timeout 2 tail -f /dev/null; n1=$(grep -c "^client .* app=browser" $D/state)
xdotool mousemove $(hit pin browser) click 3; w8 'grep -q "^cmitem .*pnew:browser" $D/state' 40
P=$(grep "^cmitem .*pnew:browser" $D/state | head -1 | awk '{print $2, $3}'); cl=$(grep "^cmitem .*pclose:browser" $D/state | awk '{print $4}'); xdotool mousemove $P click 1
w8 '[ "$(grep -c "^client .* app=browser" $D/state)" = 2 ]' 120
ok "5. the pin while Oracle is open: Oracle to the front (no second window); its menu's Close all windows not greyed" "$([ "$n1" = 1 ] && [ "$cl" = 0 ] && echo 1)" "$n1 $cl"
ok "...its Open a new window: a second Oracle window, of the same Chromium" "$([ "$(grep -c '^client .* app=browser' $D/state)" = 2 ] && [ "$(chromes)" = 1 ] && [ "$(targets | grep -c '^app isolated-app://')" = 2 ] && echo 1)" "$(grep '^client' $D/state) | chromes $(chromes) | $(targets) | menu: $P $(grep '^cmitem' $D/state | head -8)"
# ---- the windows closed: Chromium goes with the last (one by the page's own ✕, the other by the frame's)
ev http://127.0.0.1:8080/ 'window.__oneAppClose(),1' one >/dev/null
w8 '[ "$(grep -c "^client .* app=browser" $D/state)" -le 1 ]' 60; timeout 1 tail -f /dev/null
timeout 1 tail -f /dev/null
for try in 1 2 3; do F2=$(grep "^client .* app=browser" $D/state | head -1); [ -n "$F2" ] || break; set -- $F2; xdotool mousemove $(($4 + $6 - 10)) $(($5 + 17)); timeout 0.3 tail -f /dev/null; xdotool click 1
	w8 '! grep -q "^client .* app=browser" $D/state' 40 && break; done
w8 '[ "$(grep -c "^client .* app=browser" $D/state)" = 0 ]' 120
w8 '[ "$(st oracle | cut -d" " -f1)" = 0 ] && [ "$(chromes)" = 0 ]' 80
ok "6. Oracle's windows closed (the app's own ✕, the frame's ✕): they go, and Chromium with the last" "$([ "$(grep -c '^client .* app=browser' $D/state)" = 0 ] && [ "$(chromes)" = 0 ] && [ "$(st oracle | cut -d' ' -f1)" = 0 ] && echo 1)" "$(grep '^client' $D/state) | $(st oracle) | $(pgrep -af RudolfOracle | grep -v -- --type= | head -3)"
# (from here without Rudolf OS's Chromium policy: 9 below needs Chromium's own file dialog)
for p in $POL; do rm -f $p/rudolf.json; done
# ---- again: the same profile, the cookie still there
xdotool mousemove $(hit pin browser) click 1
w8 '[ "$(ev http://127.0.0.1:8080/ "typeof rtOn==\"function\"&&rtOn()&&!!OA.main")" = true ]' 120
ev http://127.0.0.1:8080/ 'webOpen("http://127.0.0.1:8333/"),1' >/dev/null
w8 'targets | grep -q "^webview http://127.0.0.1:8333/"' 80; timeout 1 tail -f /dev/null
o=$(ev http://127.0.0.1:8333/ 'document.cookie')
ok "7. Oracle opened again: Chromium started again with the same profile, the site's sign-in (its cookie) kept" "$([ "$o" = '"oracle59=kept"' ] && [ "$(chromes)" = 1 ] && echo 1)" "$o $(targets)"
# ---- (step 5) a website's file input with Chromium's own file dialog (a Chromium without Rudolf OS's policy): a window of its own,
# centred over Oracle's (maximised here, so the dialog fits inside it), framed as a dialog: not as Oracle, not cut, not on the taskbar
CL=$(grep "^client .* app=browser" $D/state | head -1); set -- $CL; xdotool mousemove $(($4 + 300)) $(($5 + 12)) click --repeat 2 --delay 80 1
w8 'grep -q "^client .* max=1 .* app=browser" $D/state' 40
ev $PG 'webOpen("http://127.0.0.1:8333/up.html"),1' one >/dev/null; w8 'targets | grep -q "^webview http://127.0.0.1:8333/up.html"' 60; timeout 1 tail -f /dev/null
xdotool mousemove $(tabxy 60 30) click 1
w8 'grep "^client " $D/state | grep -qv "app=browser"' 40; timeout 1 tail -f /dev/null
DG=$(grep "^client " $D/state | grep -v "app=browser" | head -1); CL=$(grep "^client .* app=browser" $D/state | head -1)
set -- $(corg); TY=$2; set -- $CL; OX=$4; OY=$5; TH0=$((TY - OY)); OFW=$(($6 + 2)); OFH=$(($7 + TH0 + 1))
set -- $DG; DX=$4; DY=$5; DFW=$(($6 + 2)); DFH=$(($7 + TH0 + 1)); EX=$((OX + (OFW - DFW) / 2)); EY=$((OY + (OFH - DFH) / 2)); [ $EX -lt 0 ] && EX=0; [ $EY -lt 0 ] && EY=0
set -- $($T/xabs 0x$2 2>/dev/null)
ok "9. a website's file input, Chromium's own dialog (without Rudolf OS's policy): a window of its own, centred over Oracle's" "$([ -n "$DG" ] && [ $((DX - EX)) -ge -1 ] && [ $((DX - EX)) -le 1 ] && [ $((DY - EY)) -ge -1 ] && [ $((DY - EY)) -le 1 ] && echo 1)" "$DG | expected $EX $EY | $CL"
ok "...framed as a dialog: not as Oracle (no Oracle picture or name, its window not cut by Oracle's insets), not on the taskbar" "$(echo "$DG" | grep -q ' app=- ' && [ "$1" = $((DX + 1)) ] && [ "$2" = $((DY + TH0)) ] && ! grep -q "^hit win $(echo "$DG" | cut -d' ' -f2) " $D/state && echo 1)" "$DG | at $1 $2 | $(grep '^hit win' $D/state)"
xdotool key Escape; w8 '! grep "^client " $D/state | grep -qv "app=browser"' 40
ev http://127.0.0.1:8080/ 'window.__oneAppClose(),1' >/dev/null
w8 '[ "$(chromes)" = 0 ] && [ "$(st oracle | cut -d" " -f1)" = 0 ]' 80
# ---- without Chromium: Oracle's WebKit window, as before, said once
printf 'nothing\n' > $T/oracle-chrome
xdotool mousemove $(hit pin browser) click 1
w8 'grep -q "^app browser " $D/state' 120
ok "8. without Rudolf's Chromium: Oracle's WebKit window, as before (the page told: nochrome=1)" "$(grep -q '^app browser ' $D/state && [ "$(chromes)" = 0 ] && grep -q 'Oracle: .* names no Chromium or shell' $T/native.log && echo 1)" "$(grep '^app ' $D/state) $(tail -3 $T/native.log)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
