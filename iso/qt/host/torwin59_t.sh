#!/bin/sh
# 1.5.9 (docs/plan-1.5.9.md 16.2 and 16.3): Oracle's Tor windows on Standard, started by rudolfd's own torwin/* (compiled from
# rudolfd.c: fixtures/torwinapi.c, answered through the stand-in rudolfd, fixtures/native_rd.py, which serves the page) under
# Xvfb, with Chromium (CHROME; Playwright's by default), the shell built with a test key, the session's own tor_chrome flags
# and a stand-in for Tor's SOCKS5 proxy (fixtures/socks59.py). torwin/open -> rudolf-launch --oracle and a Chromium of its own
# (--class=RudolfTor) as the desktop's user (here root), its profile in /run/rudolf/torwin-profile with WebRTC only through
# the proxy, a window at #app=browser&tor=1&url=...: Oracle alone, private, the onion pressed; its site through the proxy by
# name (nothing looked up here, nothing of the desktop's 127.0.0.1:8080); WebRTC without this computer's addresses; a second
# torwin/open a second window of the same Chromium; the onion pressed again -> torwin/back has its tabs for the desktop page,
# the window closes, Chromium goes and its profile with it. In network and mount namespaces of its own. Needs root, Xvfb,
# node, python3, gcc and ip.   sh torwin59_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root (a network namespace)"; exit 0; }
[ -z "$TW59_INNER" ] && exec env TW59_INNER=1 unshare -n -m sh "$0" "$@"
command -v Xvfb >/dev/null && command -v ip >/dev/null || { echo "SKIP: needs Xvfb and ip"; exit 0; }
CHROME=${CHROME:-$(ls -d /opt/pw-browsers/chromium-*/chrome-linux*/chrome 2>/dev/null | tail -1)}
[ -x "$CHROME" ] || { echo "SKIP: no Chromium (CHROME)"; exit 0; }
HERE=$(cd "$(dirname "$0")" && pwd); REPO=$(cd "$HERE/../../.." && pwd); T=$(mktemp -d /tmp/torwin.XXXXXX); DSP=${TW59_DSP:-:88}
ip link set lo up
ip link add ra59 type veth peer name rb59 2>/dev/null && ip addr add 10.59.0.1/24 dev ra59 && ip link set ra59 up && ip link set rb59 up && ip route add default via 10.59.0.2
mount --make-rprivate / 2>/dev/null; mount -t tmpfs none /run && mkdir -p /run/rudolf || { echo "FAIL /run"; exit 1; }
trap 'kill $RDP $XV $SP $XP 2>/dev/null; pkill -f "$T/" 2>/dev/null; pkill -f torwin-profile 2>/dev/null; timeout 1 tail -f /dev/null; pkill -9 -f "$T/" 2>/dev/null; pkill -9 -f torwin-profile 2>/dev/null; rm -rf $T' EXIT
gcc -O2 -Wall -o $T/rudolf-launch $REPO/iso/sys/rudolf-launch.c 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
gcc -O2 -w -DTORWIN_USER='"root"' -DTORWIN_LAUNCH="\"$T/rudolf-launch\"" -o $T/torwinapi $HERE/fixtures/torwinapi.c -lz 2>>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
python3 $REPO/iso/systools/mkiwa.py key $T/test.key >/dev/null; ID=$(python3 $REPO/iso/systools/mkiwa.py id $T/test.key)
python3 $REPO/iso/systools/mkiwa.py build $REPO/iso/shell $T/shell.swbn $T/test.key >/dev/null || { echo "FAIL the shell"; exit 1; }
D=$T/d; mkdir -p $D $D/idbm; node $REPO/iso/qt/host/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
echo $T/torwinapi > $D/torwin-fix; echo $ID > $D/iwa-id
# (Standard's page: engine chromium; no network status in this rudolfd: the page is told it has the network)
echo 'RUDOLFD.engine="chromium";Object.defineProperty(window,"__net",{get(){return 0},set(){},configurable:true});' > $D/inject.js
mkdir -p $T/site; printf '%s' '<!doctype html><title>Site 59</title><p id=p>through tor</p>' > $T/site/index.html
(cd $T/site && exec python3 -m http.server 8333 --bind 127.0.0.1 >/dev/null 2>&1) & SP=$!
python3 $HERE/fixtures/socks59.py 9050 8333 $T/socks.log & XP=$!; : > $T/socks.log
# the session's own tor_chrome, with this test's paths (and --no-sandbox as root, a DevTools port)
sed -n '/^tor_chrome() {/,/^}/p' $REPO/iso/sys/root/usr/lib/rudolf/session | sed "s#/opt/chromium/chrome#$CHROME#g; s#/usr/bin/rudolf-launch#$T/rudolf-launch#g" > $T/torfn.sh
mkdir -p $T/ruf; printf '{"manifest_version":3,"name":"stand-in","version":"1"}' > $T/ruf/manifest.json
( SHELLID=$ID SHELLINST="--install-isolated-web-app-from-file=$T/shell.swbn" SHELLFEAT=",IsolatedWebAppDevMode" GLFLAGS="--disable-gpu" RUF=$T/ruf; . $T/torfn.sh; tor_chrome )
[ -s /run/rudolf/tor-chrome ] || { echo "FAIL the session's tor_chrome wrote nothing"; exit 1; }
printf '%s\n' --no-sandbox --remote-debugging-port=9337 >> /run/rudolf/tor-chrome
echo "DISPLAY=$DSP XDG_RUNTIME_DIR=$T/xrun" > /run/rudolf/session.env; mkdir -p $T/xrun; chmod 700 $T/xrun
python3 $HERE/fixtures/native_rd.py 8080 $D > $T/rd.log 2>&1 & RDP=$!
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
api() { node -e 'const [m,p,b]=process.argv.slice(1);fetch("http://127.0.0.1:8080/api/"+p,{method:m,headers:{"X-Rudolf-Token":"tok59native"},body:b}).then(r=>r.text()).then(t=>console.log(t)).catch(e=>console.log("ERR "+e.message))' "$@"; }
evt() { node -e '
const [pre, expr, one] = process.argv.slice(1);
(async () => { const l = await (await fetch("http://127.0.0.1:9337/json/list")).json(); const ts = l.filter(x => x.url.startsWith(pre)).slice(0, one ? 1 : 99); if (!ts.length) { console.log("NO TARGET"); return }
  const r = []; for (const t of ts) r.push(await new Promise(res => { const ws = new WebSocket(t.webSocketDebuggerUrl); const to = setTimeout(() => res("TIMEOUT"), 8000);
    ws.onopen = () => ws.send(JSON.stringify({id: 1, method: "Runtime.evaluate", params: {expression: expr, returnByValue: true, awaitPromise: true, userGesture: true}}));
    ws.onmessage = e => { const m = JSON.parse(e.data); if (m.id == 1) { clearTimeout(to); ws.close(); res(m.result.result ? m.result.result.value : m.result) } } }));
  console.log(JSON.stringify(r.length == 1 ? r[0] : r)) })().catch(e => console.log("ERR " + e.message))' "$@" 2>&1; }
targets() { node -e 'fetch("http://127.0.0.1:9337/json/list").then(r=>r.json()).then(l=>console.log(l.map(t=>t.type+" "+t.url).join("\n"))).catch(()=>{})' 2>/dev/null; }
# (Chromium's browser processes with that flag: Chromium itself, not its helpers (--type=), not rudolf-launch or a shell)
cpids() { for p in $(pgrep -f -- "$1"); do [ "$(cat /proc/$p/comm 2>/dev/null)" = chrome ] || continue; c=$( { tr '\0' ' ' < /proc/$p/cmdline; } 2>/dev/null); case "$c" in *--type=*|'') ;; *) echo $p;; esac; done; }
procs() { cpids "$1" | wc -l | tr -d ' '; }
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
w8 'api GET desktop/accounts | grep -q ok' 80
PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)"); api POST desktop/login "{\"n\":\"Ann\",\"p\":\"$PW\"}" >/dev/null
o=$(api GET torwin/back)
ok "1. no Tor window yet: torwin/back says none is open" "$(echo "$o" | grep -q '"open":false,"urls":\[\]' && echo 1)" "$o"
o=$(api POST "torwin/open?url=$(node -e 'console.log(encodeURIComponent("http://site59.test/"))')&url=about%3Ahome")
w8 'targets | grep -q "^webview http://127.0.0.1:8080/#app=browser&tor=1"' 200
P=$(cpids --class=RudolfTor | head -1)
ok "2. torwin/open: rudolf-launch --oracle and a Chromium of its own (--class=RudolfTor) with the session's flags: Tor's SOCKS5 proxy, no name looked up here, WebRTC only through the proxy" "$(echo "$o" | grep -q '"ok":true' && [ "$(procs --class=RudolfTor)" = 1 ] && pgrep -f "^$T/rudolf-launch --oracle /run/rudolf/torwin.ctl" >/dev/null && tr '\0' '\n' < /proc/$P/cmdline | grep -qx -- '--proxy-server=socks5://127.0.0.1:9050' && tr '\0' '\n' < /proc/$P/cmdline | grep -qx -- '--host-resolver-rules=MAP \* ~NOTFOUND , EXCLUDE 127.0.0.1' && tr '\0' '\n' < /proc/$P/cmdline | grep -qx -- '--force-webrtc-ip-handling-policy=disable_non_proxied_udp' && echo 1)" "$o | $(tr '\0' ' ' < /proc/$P/cmdline 2>/dev/null | head -c 500) | $(tail -3 /run/rudolf/torwin.log 2>/dev/null)"
ok "...its profile of its own in memory (/run/rudolf/torwin-profile), WebRTC only through the proxy in its settings too" "$(tr '\0' '\n' < /proc/$P/cmdline | grep -qx -- '--user-data-dir=/run/rudolf/torwin-profile' && grep -q '"ip_handling_policy":"disable_non_proxied_udp"' /run/rudolf/torwin-profile/Default/Preferences && echo 1)" "$(cat /run/rudolf/torwin-profile/Default/Preferences 2>/dev/null | head -c 300)"
w8 '[ "$(evt http://127.0.0.1:8080/ "!!(typeof OA==\"object\"&&OA.main&&OA.main.tor)" one)" = true ]' 160
o=$(evt http://127.0.0.1:8080/ '(()=>{const W=OA.main,b=W.body.querySelector(".otor");return [W.body.querySelectorAll(".otab").length,!!W.priv,b.getAttribute("aria-pressed"),getComputedStyle(b).borderTopColor,typeof oaChromium=="function"&&oaChromium()].join("|")})()' one)
ok "3. the Tor window: Oracle alone with the two tabs, private, the onion pressed (white outline); on Standard an Oracle page of its own (no native desktop to tell)" "$([ "$o" = '"2|true|true|rgb(255, 255, 255)|false"' ] && echo 1)" "$o"
w8 'grep -q "^CONNECT site59.test 80 name" $T/socks.log' 80
ok "4. its site in a real tab (the shell's), through Tor's proxy by its name (nothing looked up here); nothing of the desktop's 127.0.0.1:8080" "$(grep -q '^CONNECT site59.test 80 name' $T/socks.log && ! grep -q ' ipv[46]$' $T/socks.log && ! grep -q ' 8080 ' $T/socks.log && w8 '[ "$(evt http://site59.test/ document.title)" = "\"Site 59\"" ]' 40 && targets | grep -q '^webview http://site59.test/' && echo 1)" "$(grep -v google $T/socks.log) | $(targets)"
o=$(evt http://site59.test/ 'new Promise(r=>{const pc=new RTCPeerConnection({iceServers:[{urls:"stun:127.0.0.1:3478"}]});const c=[];pc.onicecandidate=e=>{if(e.candidate)c.push(e.candidate.candidate);else r(c.join("|"))};pc.createDataChannel("x");pc.createOffer().then(o=>pc.setLocalDescription(o));setTimeout(()=>r(c.join("|")),6000)})')
ok "5. WebRTC in its tab: no UDP candidate (this computer's 10.59.0.1 not given away)" "$(! echo "$o" | grep -qi ' udp ' && echo 1)" "$o"
o=$(api POST "torwin/open?url=$(node -e 'console.log(encodeURIComponent("http://other59.test/"))')")
w8 '[ "$(targets | grep -c "^webview http://127.0.0.1:8080/#app=browser&tor=1")" = 2 ]' 120
ok "6. a second torwin/open (a new window from a Tor window): a second window of the same Chromium" "$([ "$(targets | grep -c '^webview http://127.0.0.1:8080/#app=browser&tor=1')" = 2 ] && [ "$(procs --class=RudolfTor)" = 1 ] && echo 1)" "$o | $(targets)"
# the onion pressed again in the first window: its tabs given back, the window closes
evt "http://127.0.0.1:8080/#app=browser&tor=1&url=http" 'OA.main.body.querySelector(".otor").click(),1' one >/dev/null
w8 '[ "$(targets | grep -c "^webview http://127.0.0.1:8080/#app=browser&tor=1")" = 1 ]' 80
o=$(api GET torwin/back); o2=$(api GET torwin/back)
ok "7. pressed again: torwin/back holds its tabs for the desktop page (once), the window closes, the Chromium stays for the other" "$({ echo "$o" | grep -q '"open":true,"urls":\["http://site59.test/","about:home"\]' || echo "$o" | grep -q '"open":true,"urls":\["http://other59.test/"\]'; } && echo "$o2" | grep -q '"urls":\[\]' && [ "$(procs --class=RudolfTor)" = 1 ] && echo 1)" "$o | $o2 | $(grep -E "torwin|tor\/" $D/calls | tail -5) | $(targets)"
[ -n "$TW59_HOLD" ] && { echo "HOLD $T"; timeout $TW59_HOLD tail -f /dev/null; }   # (debugging: the run kept as it is here)
evt http://127.0.0.1:8080/ 'window.__oneAppClose(),1' >/dev/null
w8 '[ "$(procs --class=RudolfTor)" = 0 ] && [ ! -e /run/rudolf/torwin-profile ] && [ ! -e /run/rudolf/torwin.pid ]' 120
o=$(api GET torwin/back)
ok "8. the last Tor window closed: its Chromium gone, the profile removed (nothing kept), torwin/back says none is open" "$([ "$(procs --class=RudolfTor)" = 0 ] && [ ! -e /run/rudolf/torwin-profile ] && [ ! -e /run/rudolf/torwin.ctl ] && echo "$o" | grep -q '"open":false' && echo 1)" "$o $(ls /run/rudolf) | procs $(procs --class=RudolfTor) | $(targets) | $(pgrep -fa rudolf-launch | grep -v bash | head -3) | $(tail -3 /run/rudolf/torwin.log)"
grep -q 'POST tor/stop' $D/calls 2>/dev/null; ok "...and Tor told (tor/stop)" "$(grep -q 'POST tor/stop' $D/calls && echo 1)" "$(grep tor/ $D/calls | tail -3)"
mv /run/rudolf/tor-chrome /run/rudolf/tor-chrome.x; o=$(api POST "torwin/open?url=about%3Ahome")
ok "9. without the session's tor-chrome (no Chromium with the shell): torwin/open says so" "$(echo "$o" | grep -q 'needs Rudolf.s Chromium with its shell' && echo 1)" "$o $(ls -la /run/rudolf)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
