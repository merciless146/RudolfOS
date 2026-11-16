#!/bin/sh
# 1.5.9 (docs/plan-1.5.9.md 16.2 and 16.3): Browse with Tor on the native desktop. rudolf-native under Xvfb with a stand-in
# rudolfd (qt/host/fixtures/native_rd.py, its tor/* stood in), the shell built with a test key, Chromium (CHROME;
# Playwright's by default), and a stand-in for Tor's SOCKS5 proxy on 127.0.0.1:9050 (fixtures/socks59.py) that writes down
# what it is asked. The Tor window's Chromium is started with the session's own flags (its tor_chrome, run here). An Oracle
# window's onion pressed -> a Tor window of its own Chromium (--class=RudolfTor, its profile in the runtime folder), framed
# as Oracle, its tabs those of the window, which closed; every site's request goes through the proxy with the site's name
# (looked up by Tor, never here), nothing for the desktop's own 127.0.0.1:8080; WebRTC finds no address of this computer;
# the tab private; pressed again -> its tabs in a normal Oracle window (Oracle's own Chromium), the Tor window and its
# Chromium gone with its profile, Tor told. (1.6.0, plan Part 15, "Before stable" item 10) Ctrl+W, T, N and Q in a website in the
# Tor window are Rudolf's, as in Oracle's windows (rudolf-native's key grab on RudolfTor): the tab closed, a new tab, a new Tor
# window, Q nothing (the site sees Ctrl+F23); the site's own typing untouched. In network and mount namespaces of its own. Needs root, Xvfb, xdotool, node,
# python3, gcc, ip and WebKitGTK's headers.   sh tor_native_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root (a network namespace)"; exit 0; }
[ -z "$TN59_INNER" ] && exec env TN59_INNER=1 unshare -n -m sh "$0" "$@"
command -v Xvfb >/dev/null && command -v xdotool >/dev/null && command -v ip >/dev/null || { echo "SKIP: needs Xvfb, xdotool and ip"; exit 0; }
CHROME=${CHROME:-$(ls -d /opt/pw-browsers/chromium-*/chrome-linux*/chrome 2>/dev/null | tail -1)}
[ -x "$CHROME" ] || { echo "SKIP: no Chromium (CHROME)"; exit 0; }
HERE=$(cd "$(dirname "$0")" && pwd); REPO=$(cd "$HERE/../../.." && pwd); T=$(mktemp -d /tmp/tornat.XXXXXX); DSP=${TN59_DSP:-:87}
ip link set lo up
# (an address of this computer's own besides loopback, which WebRTC would give away: a pair of virtual network cards)
ip link add ra59 type veth peer name rb59 2>/dev/null && ip addr add 10.59.0.1/24 dev ra59 && ip link set ra59 up && ip link set rb59 up && ip route add default via 10.59.0.2
mount --make-rprivate / 2>/dev/null; mount -t tmpfs none /run && mkdir -p /run/rudolf/dl/.incoming || { echo "FAIL /run"; exit 1; }
trap 'kill $NP $RDP $XV $SP $XP 2>/dev/null; pkill -f "$T/" 2>/dev/null; timeout 1 tail -f /dev/null; pkill -9 -f "$T/" 2>/dev/null; rm -rf $T' EXIT
gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $T/rudolf-native $REPO/iso/sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
gcc -O2 -Wall -o $T/rudolf-launch $REPO/iso/sys/rudolf-launch.c 2>>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
grep -q warning $T/cc && { cat $T/cc; echo "FAIL build warnings"; exit 1; }
# (a window's place on the screen)
printf '%s\n' '#include <X11/Xlib.h>' '#include <stdio.h>' '#include <stdlib.h>' 'int main(int c,char**v){Display*d=XOpenDisplay(0);int x,y;Window k;if(!d||c<2)return 1;XTranslateCoordinates(d,strtoul(v[1],0,0),DefaultRootWindow(d),0,0,&x,&y,&k);printf("%d %d\n",x,y);return 0;}' > $T/xabs.c
gcc -o $T/xabs $T/xabs.c -lX11 || { echo "FAIL build xabs"; exit 1; }
python3 $REPO/iso/systools/mkiwa.py key $T/test.key >/dev/null; ID=$(python3 $REPO/iso/systools/mkiwa.py id $T/test.key)
python3 $REPO/iso/systools/mkiwa.py build $REPO/iso/shell $T/shell.swbn $T/test.key >/dev/null || { echo "FAIL the shell"; exit 1; }
D=$T/d; mkdir -p $D; node $REPO/iso/qt/host/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
# (no network status in this test's rudolfd: the page is told it has the network, so sites by name are tried, not "offline")
echo 'Object.defineProperty(window,"__net",{get(){return 0},set(){},configurable:true});' > $D/inject.js
echo $ID > $D/iwa-id; echo /run/rudolf/dl/.incoming > $D/dl-dir; mkdir -p $D/idbm
printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","b2":"#000080","tb":"bottom","accent":"#2563eb"},"pins":["notepad","browser"],"notes":0,"wallpaper":false}' > $D/settings.json
# a website (site59.test through the proxy; 127.0.0.1:8333 directly)
mkdir -p $T/site; printf '%s' '<!doctype html><title>Site 59</title><p id=p>through tor</p>' > $T/site/index.html
printf '%s' '<!doctype html><title>Keys 59</title><input id=i style="position:fixed;left:20px;top:20px"><script>K=[];addEventListener("keyup",e=>{if(e.ctrlKey)K.push(e.code)},true)</script>' > $T/site/keys.html
(cd $T/site && exec python3 -m http.server 8333 --bind 127.0.0.1 >/dev/null 2>&1) & SP=$!
python3 $REPO/iso/qt/host/fixtures/socks59.py 9050 8333 $T/socks.log & XP=$!; : > $T/socks.log
# Oracle's Chromium, as the session writes it for rudolf-native (Standard's flags; --no-sandbox as root here)
printf '%s\n' $ID "$CHROME" --no-sandbox --class=RudolfOracle --user-data-dir=$T/prof --no-first-run --no-default-browser-check --password-store=basic \
	--enable-features=SharedArrayBuffer,IsolatedWebApps,IsolatedWebAppDevMode --install-isolated-web-app-from-file=$T/shell.swbn --disable-gpu \
	--remote-debugging-port=9335 > $T/oracle-chrome
# the Tor window's Chromium: the session's own tor_chrome, run here with this test's paths (and --no-sandbox, a DevTools port)
sed -n '/^tor_chrome() {/,/^}/p' $REPO/iso/sys/root/usr/lib/rudolf/session | sed "s#/opt/chromium/chrome#$CHROME#g; s#/usr/bin/rudolf-launch#$T/rudolf-launch#g; s#/run/rudolf/tor-chrome#$T/tor-chrome#g" > $T/torfn.sh
mkdir -p $T/ruf; printf '{"manifest_version":3,"name":"stand-in","version":"1"}' > $T/ruf/manifest.json
( SHELLID=$ID SHELLINST="--install-isolated-web-app-from-file=$T/shell.swbn" SHELLFEAT=",IsolatedWebAppDevMode" GLFLAGS="--disable-gpu" RUF=$T/ruf; . $T/torfn.sh; tor_chrome )
[ -s $T/tor-chrome ] || { echo "FAIL the session's tor_chrome wrote nothing"; cat $T/torfn.sh | head -5; exit 1; }
printf '%s\n' --no-sandbox --remote-debugging-port=9336 >> $T/tor-chrome
python3 $REPO/iso/qt/host/fixtures/native_rd.py 8080 $D > $T/rd.log 2>&1 & RDP=$!
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=$DSP LIBGL_ALWAYS_SOFTWARE=1
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
w8 'xdotool getmouselocation >/dev/null 2>&1' 40
mkdir -p $T/run; chmod 700 $T/run
(cd $D && HOME=$D XDG_RUNTIME_DIR=$T/run RUDOLF_NATIVE_STATE=$D/state RUDOLF_ORACLE_CMD=$T/oracle-chrome RUDOLF_TOR_CMD=$T/tor-chrome RUDOLF_LAUNCH=$T/rudolf-launch exec $T/rudolf-native --size=1280x800 --draw=soft --base=http://127.0.0.1:8080 > $T/native.log 2>&1) & NP=$!
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
hit() { grep "^hit $1 $2 " $D/state | head -1 | awk '{print $4+int($6/2), $5+int($7/2)}'; }
# (DevTools of either Chromium: PORT PREFIX JS [one])
evp() { node -e '
const [port, pre, expr, one] = process.argv.slice(1);
(async () => { const l = await (await fetch("http://127.0.0.1:" + port + "/json/list")).json(); const ts = l.filter(x => x.url.startsWith(pre)).slice(0, one ? 1 : 99); if (!ts.length) { console.log("NO TARGET"); return }
  const r = []; for (const t of ts) r.push(await new Promise(res => { const ws = new WebSocket(t.webSocketDebuggerUrl); const to = setTimeout(() => res("TIMEOUT"), 8000);
    ws.onopen = () => ws.send(JSON.stringify({id: 1, method: "Runtime.evaluate", params: {expression: expr, returnByValue: true, awaitPromise: true, userGesture: true}}));
    ws.onmessage = e => { const m = JSON.parse(e.data); if (m.id == 1) { clearTimeout(to); ws.close(); res(m.result.result ? m.result.result.value : m.result) } } }));
  console.log(JSON.stringify(r.length == 1 ? r[0] : r)) })().catch(e => console.log("ERR " + e.message))' "$@" 2>&1; }
ev() { evp 9335 "$@"; }; evt() { evp 9336 "$@"; }
targets() { node -e 'fetch("http://127.0.0.1:'$1'/json/list").then(r=>r.json()).then(l=>console.log(l.map(t=>t.type+" "+t.url).join("\n"))).catch(()=>{})' 2>/dev/null; }
# (Chromium's browser processes with that flag: Chromium itself, not its helpers (--type=), not rudolf-launch or a shell)
cpids() { for p in $(pgrep -f -- "$1"); do [ "$(cat /proc/$p/comm 2>/dev/null)" = chrome ] || continue; c=$( { tr '\0' ' ' < /proc/$p/cmdline; } 2>/dev/null); case "$c" in *--type=*|'') ;; *) echo $p;; esac; done; }
procs() { cpids "$1" | wc -l | tr -d ' '; }
# (the X window names of the clients, for a failure's details)
xnames() { for w in $(grep '^client ' $D/state | awk '{print $2}'); do printf '%s: %s; ' $w "$(xdotool getwindowname 0x$w 2>&1)"; done; grep -i "title" $T/native.log | tail -5; }
w8 '[ "$(st signin)" = 1 ]' 120
PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)"); LC_ALL=C.UTF-8 xdotool type --delay 30 "$PW"; xdotool key Return
w8 '[ "$(st user)" = Ann ] && grep -q "^hit pin browser" $D/state' 120 || { echo "FAIL signing in"; tail $T/native.log; exit 1; }
# ---- an Oracle window with two tabs: a site by its name (site59.test: no name is looked up here) and the local one
xdotool mousemove $(hit pin browser) click 1
w8 '[ "$(ev http://127.0.0.1:8080/ "typeof rtOn==\"function\"&&rtOn()&&!!OA.main" one)" = true ]' 160
ev http://127.0.0.1:8080/ 'OA.main.oNewTab("http://site59.test/"),1' one >/dev/null; timeout 2 tail -f /dev/null
o=$(ev http://127.0.0.1:8080/ '(()=>{const b=OA.main.body.querySelector(".obm .otor");return b&&[b.title,b.getAttribute("aria-pressed"),getComputedStyle(b).borderTopColor].join("|")})()' one)
ok "1. Oracle's window on the native desktop: the onion at the bookmarks bar's right end, not pressed (black outline)" "$([ "$o" = '"Browse Oracle with Tor|false|rgb(0, 0, 0)"' ] && echo 1)" "$o"
: > $T/socks.log
ev http://127.0.0.1:8080/ 'OA.main.body.querySelector(".otor").click(),1' one >/dev/null
w8 '[ "$(st tor | cut -d" " -f1)" = 1 ]' 80
w8 'targets 9336 | grep -q "^webview http://127.0.0.1:8080/#app=browser&tor=1"' 160
w8 '[ "$(evt http://127.0.0.1:8080/ "!!(typeof OA==\"object\"&&OA.main&&OA.main.tor)" one)" = true ]' 120
ok "2. pressed: a Tor window, Rudolf's Chromium of its own (--class=RudolfTor, the session's flags), at #app=browser&tor=1&url=..." "$([ "$(procs --class=RudolfTor)" = 1 ] && tr '\0' '\n' < /proc/$(cpids --class=RudolfTor | head -1)/cmdline | grep -qx -- '--proxy-server=socks5://127.0.0.1:9050' && targets 9336 | grep -q 'tor=1&.*url=http%3A%2F%2Fsite59.test%2F' && echo 1)" "$(st tor) | $(targets 9336) | $(tail -5 $T/native.log)"
ok "...its profile of its own in the runtime folder (memory), never Oracle's" "$(tr '\0' '\n' < /proc/$(cpids --class=RudolfTor | head -1)/cmdline | grep -qx -- "--user-data-dir=$T/run/rudolf-tor-profile" && [ -d $T/run/rudolf-tor-profile ] && echo 1)" "$(pgrep -af -- '--class=RudolfTor' | head -2)"
w8 '[ "$(grep -c "^client .* app=browser" $D/state)" = 1 ]' 80
CL=$(grep "^client .* app=browser" $D/state | head -1)
ok "3. framed by the native desktop as Oracle (the onion's window, \"(Tor)\" in its title); the normal window it came from closed" "$(echo "$CL" | grep -q '(Tor)' && [ "$(ev http://127.0.0.1:8080/ '!!(typeof OA=="object"&&OA.main&&OA.main.el.isConnected)')" != true ] && echo 1)" "$CL | $(xnames) | $(grep '^client' $D/state) | $(ev http://127.0.0.1:8080/ 'document.title')"
o=$(evt http://127.0.0.1:8080/ '(()=>{const W=OA.main,b=W.body.querySelector(".otor");return [W.body.querySelectorAll(".otab").length,!!W.priv,b.getAttribute("aria-pressed"),getComputedStyle(b).borderTopColor].join("|")})()' one)
ok "...its tabs the window's (2), private, the onion pressed (white outline)" "$([ "$o" = '"2|true|true|rgb(255, 255, 255)"' ] && echo 1)" "$o"
w8 'grep -q "^CONNECT site59.test 80 name" $T/socks.log' 80
w8 '[ "$(evt http://site59.test/ "document.title" one)" = "\"Site 59\"" ]' 80
ok "4. the site in the Tor window, a real tab (the shell's): through Tor's proxy, given its name (looked up by Tor, never on this computer), and shown" "$(grep -q '^CONNECT site59.test 80 name' $T/socks.log && [ "$(evt http://site59.test/ 'document.title' one)" = '"Site 59"' ] && ! grep -q ' ipv[46]$' $T/socks.log && targets 9336 | grep -q '^webview http://site59.test/' && echo 1)" "$(targets 9336) | $(grep -v google $T/socks.log)"
ok "...the desktop's own page (127.0.0.1:8080) not through the proxy" "$(! grep -q ' 8080 ' $T/socks.log && echo 1)" "$(cat $T/socks.log)"
RTC='new Promise(r=>{const pc=new RTCPeerConnection({iceServers:[{urls:"stun:127.0.0.1:3478"}]});const c=[];pc.onicecandidate=e=>{if(e.candidate)c.push(e.candidate.candidate);else r(c.join("|"))};pc.createDataChannel("x");pc.createOffer().then(o=>pc.setLocalDescription(o));setTimeout(()=>r(c.join("|")),6000)})'
RTCT=$(evt http://site59.test/ "$RTC")
ok "5. WebRTC in the Tor window gathers no UDP candidate (no address of this computer: UDP only through the proxy, which has none)" "$(! echo "$RTCT" | grep -qi ' udp ' && echo 1)" "$RTCT"
o=$(evt http://site59.test/ 'fetch("http://127.0.0.1:8333/").then(r=>"reached "+r.status,e=>"refused")'); n0=$(wc -l < $T/socks.log)
o2=$(evt http://site59.test/ 'fetch("http://other59.test/").then(r=>"reached "+r.status,e=>"refused")')
ok "...and every other name goes to the proxy too (other59.test asked of it)" "$(grep -q '^CONNECT other59.test 80 name' $T/socks.log && echo 1)" "$o2 | $(cat $T/socks.log)"
ok "6. the normal Oracle's Chromium closed with its last window (Oracle's other windows and apps untouched)" "$(w8 '[ "$(procs --class=RudolfOracle)" = 0 ]' 80 && echo 1)" "$(procs --class=RudolfOracle) $(st oracle)"
# ---- 1.6.0 (plan Part 15, "Before stable" item 10): Ctrl+W, T, N and Q in a website in the Tor window are Rudolf's, as in Oracle's
# windows (rudolf-native grabs them on the Tor window, RudolfTor as RudolfOracle, and gives them back as Ctrl+F20..F23): close the
# tab, a new tab, a new Tor window, and Q nothing; the site's own typing untouched. Real keys (xdotool) into a click in the site.
PGT="http://127.0.0.1:8080/#app=browser&tor=1"
evt "$PGT" 'OA.main.oNewTab("http://keys59.test/keys.html"),1' one >/dev/null
w8 '[ "$(evt http://keys59.test/ document.title one)" = "\"Keys 59\"" ]' 80
ntab() { evt "$PGT" 'OA.main.body.querySelectorAll(".otab").length' one; }
torw() { targets 9336 | grep -c "^webview $PGT"; }
# (the Keys 59 tab in front, then a click in its text box: the Tor window's page starts 4, 28 px inside Chromium's window)
keyson() { evt "$PGT" '(()=>{const o=[...OA.main.body.querySelectorAll(".otab")].find(t=>/Keys 59/.test(t.textContent));o&&o.click();return !!o})()' one >/dev/null; timeout 0.5 tail -f /dev/null
  set -- $(grep "^client .* app=browser" $D/state | head -1); set -- $($T/xabs 0x$2)
  o=$(evt "$PGT" '(()=>{let b=null;for(const[,t]of RT)if(t.shown()&&(!b||t.z()>=b.z()))b=t;if(!b)return"";const r=b.el.getBoundingClientRect();return Math.round(r.left)+" "+Math.round(r.top)})()' one | tr -d '"')
  set -- $1 $2 $o; xdotool mousemove $(($1 + 4 + $3 + 60)) $(($2 + 28 + $4 + 30)) click 1; timeout 0.5 tail -f /dev/null; }
keyson; n0=$(ntab); xdotool type --delay 40 wtnq; timeout 0.5 tail -f /dev/null
o=$(evt http://keys59.test/ 'document.getElementById("i").value' one)
ok "8. keys typed in a website in the Tor window reach the site as they are (w, t, n, q)" "$([ "$o" = '"wtnq"' ] && [ "$(ntab)" = $n0 ] && echo 1)" "$o tabs $n0 $(ntab) | $(grep '^client' $D/state)"
xdotool key ctrl+t; w8 '[ "$(ntab)" = $((n0 + 1)) ]' 20
ok "...Ctrl+T in it: a new tab of the Tor window, as in Oracle's windows; no window of Chromium's own" "$([ "$(ntab)" = $((n0 + 1)) ] && [ "$(torw)" = 1 ] && [ "$(procs --class=RudolfTor)" = 1 ] && [ "$(grep -c '^client ' $D/state)" = 1 ] && echo 1)" "$n0 $(ntab) | $(grep '^client' $D/state) | $(targets 9336)"
# (the new tab closed again with Ctrl+W there, in the window's own page: the tabs as step 7 expects them)
xdotool key ctrl+w; w8 '[ "$(ntab)" = $n0 ]' 20
ok "...Ctrl+W in the new tab (the Tor window's own page): it closes, the window stays" "$([ "$(ntab)" = $n0 ] && [ "$(torw)" = 1 ] && echo 1)" "$n0 $(ntab) $(torw)"
keyson; xdotool key ctrl+n; w8 '[ "$(torw)" = 2 ] && [ "$(grep -c "^client .* app=browser .*(Tor)" $D/state)" = 2 ]' 120
ok "...Ctrl+N: a new Tor window (the native desktop's op tor), of the same Chromium, framed as Oracle; not a normal or Chromium's own window" "$([ "$(torw)" = 2 ] && [ "$(grep -c '^client .* app=browser .*(Tor)' $D/state)" = 2 ] && [ "$(procs --class=RudolfTor)" = 1 ] && [ "$(procs --class=RudolfOracle)" = 0 ] && echo 1)" "$(torw) | $(grep '^client' $D/state) | $(xnames)"
CLOSE2='(()=>{if(typeof OA=="object"&&OA.main&&![...OA.main.body.querySelectorAll(".otab")].some(t=>/Keys 59/.test(t.textContent)))window.__oneAppClose();return 1})()'
w8 'evt "$PGT" "$CLOSE2" >/dev/null; timeout 0.5 tail -f /dev/null; [ "$(torw)" = 1 ] && [ "$(grep -c "^client .* app=browser" $D/state)" = 1 ]' 60 || echo "(the new Tor window did not close: $(grep '^client' $D/state))"
keyson; n1=$(ntab); xdotool key ctrl+q; timeout 1.5 tail -f /dev/null
o=$(evt http://keys59.test/ 'K.join()' one)
ok "...Ctrl+Q (Ctrl+F23, as the site sees it let go): nothing, as in Oracle's windows (Chromium did not quit, the window and its tabs stay)" "$([ "$(procs --class=RudolfTor)" = 1 ] && [ "$(torw)" = 1 ] && [ "$(ntab)" = $n1 ] && echo "$o" | grep -q F23 && echo 1)" "$(procs --class=RudolfTor) $(torw) $n1 $(ntab) keys $o"
keyson; xdotool key ctrl+w; w8 '[ "$(ntab)" = $((n1 - 1)) ]' 20
ok "...Ctrl+W: the website's tab closes; the Tor window and its Chromium stay" "$([ "$(ntab)" = $((n1 - 1)) ] && [ "$(torw)" = 1 ] && [ "$(procs --class=RudolfTor)" = 1 ] && [ "$(evt http://keys59.test/ 1 one)" = "NO TARGET" ] && grep -q '^client .* app=browser .*(Tor)' $D/state && echo 1)" "$n1 $(ntab) $(torw) | $(grep '^client' $D/state) | $(xnames)"
# ---- pressed again: the tabs in a normal Oracle window, the Tor window and its Chromium gone with the profile, Tor told
: > $D/calls 2>/dev/null
evt http://127.0.0.1:8080/ 'OA.main.body.querySelector(".otor").click(),1' one >/dev/null
w8 '[ "$(ev http://127.0.0.1:8080/ "!!(typeof OA==\"object\"&&OA.main&&!OA.main.tor)" one)" = true ]' 200
o=$(ev http://127.0.0.1:8080/ '(()=>{const W=OA.main;return [W.body.querySelectorAll(".otab").length,!!W.priv,W.body.querySelector(".otor").getAttribute("aria-pressed")].join("|")})()' one)
ok "7. pressed again: its tabs in a normal Oracle window (Oracle's Chromium), not private, the onion not pressed" "$([ "$o" = '"2|false|false"' ] && [ "$(procs --class=RudolfOracle)" = 1 ] && targets 9335 | grep -q 'untor=1' && echo 1)" "$o | $(targets 9335)"
w8 '[ "$(procs --class=RudolfTor)" = 0 ] && [ "$(st tor | cut -d" " -f1)" = 0 ]' 80
ok "...the Tor window closed, its Chromium gone, its profile removed (nothing kept)" "$([ "$(procs --class=RudolfTor)" = 0 ] && [ ! -e $T/run/rudolf-tor-profile ] && [ "$(grep -c '^client .* app=browser' $D/state)" = 1 ] && ! grep -q '^client .*(Tor)' $D/state && echo 1)" "$(st tor) $(ls $T/run) $(grep '^client' $D/state)"
ok "...and rudolfd told (tor/stop: Tor stops a minute later)" "$(grep -q 'POST tor/stop' $D/calls && echo 1)" "$(grep tor/ $D/calls)"
# (the same WebRTC in a normal Oracle window, for comparison: there it does gather this computer's UDP candidates)
ev http://127.0.0.1:8080/ 'OA.main.oNewTab("http://127.0.0.1:8333/"),1' one >/dev/null; w8 'targets 9335 | grep -q "^webview http://127.0.0.1:8333/"' 60
RTCN=$(ev http://127.0.0.1:8333/ "$RTC" one)
if echo "$RTCN" | grep -qi ' udp '; then ok "...(a normal Oracle window, for comparison, gathers UDP candidates: so 5 means something)" 1 ""
else echo "NOTE 5 compares with nothing here: a normal window gathers no UDP candidate either in this network namespace ($RTCN)"; fi
echo "$((N-F))/$N passed"; [ $F = 0 ]
