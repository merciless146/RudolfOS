#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 10, docs/plan-oracle.md item 1): find in page with the real shell, in a real Chromium with Isolated
# Web Apps (Playwright's, or CHROME), on Standard and on Native, as same_oracle_t.sh starts them: Standard the shell full screen
# (rudolf-launch, class RudolfDesktop, keys through rudolf-display's own key code: fixtures/dispkeys.c), Native rudolf-native under
# Xvfb with Oracle in Rudolf's Chromium (rudolf-launch --oracle, the page in one-app mode). One stand-in rudolfd (fixtures/native_rd.py).
# On each: a local page with 17 matches of "reindeer" in a real tab (a Controlled Frame); Ctrl+F typed inside that page opens
# Oracle's find bar (the page's own Ctrl+F handler does not see it); the words typed: "1 of 17" (the Controlled Frame's own find, as
# Chromium highlights; which one answered is said); Enter "2 of 17", Shift+Enter "1 of 17"; F3 inside the page the next; Match case;
# Escape closes the bar. In network and mount namespaces of its own. Needs root, Xvfb, xdotool, node, python3, gcc, ip and WebKitGTK's
# headers.   sh orafind61_t.sh [nat|std ...]
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root (a network namespace)"; exit 0; }
[ -z "$OF61_INNER" ] && exec env OF61_INNER=1 unshare -n -m sh "$0" "$@"
command -v Xvfb >/dev/null && command -v xdotool >/dev/null && command -v ip >/dev/null || { echo "SKIP: needs Xvfb, xdotool and ip"; exit 0; }
CHROME=${CHROME:-$(ls -d /opt/pw-browsers/chromium-*/chrome-linux*/chrome 2>/dev/null | tail -1)}
[ -x "$CHROME" ] || { echo "SKIP: no Chromium (CHROME)"; exit 0; }
HERE=$(cd "$(dirname "$0")" && pwd); REPO=$(cd "$HERE/../../.." && pwd); T=$(mktemp -d /tmp/orafind.XXXXXX); DSP=${OF61_DSP:-:83}
ip link set lo up
mount --make-rprivate / 2>/dev/null; mount -t tmpfs none /run && mkdir -p /run/rudolf/dl/.incoming || { echo "FAIL /run"; exit 1; }
for p in /etc/chromium/policies/managed /etc/opt/chrome_for_testing/policies/managed; do [ -d $p ] && mount -t tmpfs none $p && cp $REPO/iso/sys/root/etc/chromium/policies/managed/rudolf.json $p/; done
trap 'kill $NP $RDP $XV $SP $CP 2>/dev/null; pkill -f "$T/" 2>/dev/null; timeout 1 tail -f /dev/null; pkill -9 -f "$T/" 2>/dev/null; rm -rf $T' EXIT
gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $T/rudolf-native $REPO/iso/sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
gcc -O2 -Wall -o $T/rudolf-launch $REPO/iso/sys/rudolf-launch.c 2>>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
L=/usr/lib/x86_64-linux-gnu; gcc -O2 -w -o $T/dispkeys $REPO/iso/qt/host/fixtures/dispkeys.c -I$REPO/iso/sys -lX11 $L/libXtst.so.6 $L/libXfixes.so.3 $L/libXrandr.so.2 -lm 2>>$T/cc || { cat $T/cc; echo "FAIL build dispkeys"; exit 1; }
printf '%s\n' '#include <X11/Xlib.h>' '#include <stdio.h>' '#include <stdlib.h>' 'int main(int c,char**v){Display*d=XOpenDisplay(0);int x,y;Window k;if(!d||c<2)return 1;XTranslateCoordinates(d,strtoul(v[1],0,0),DefaultRootWindow(d),0,0,&x,&y,&k);printf("%d %d\n",x,y);return 0;}' > $T/xabs.c
gcc -o $T/xabs $T/xabs.c -lX11 || { echo "FAIL build xabs"; exit 1; }
python3 $REPO/iso/systools/mkiwa.py key $T/test.key >/dev/null; ID=$(python3 $REPO/iso/systools/mkiwa.py id $T/test.key)
python3 $REPO/iso/systools/mkiwa.py build $REPO/iso/shell $T/shell.swbn $T/test.key >/dev/null || { echo "FAIL the shell"; exit 1; }
mkdir -p $T/site
python3 - $T/site/find.html <<'EOF'
import sys
w = ['reindeer'] * 10 + ['Reindeer'] * 5
open(sys.argv[1], 'w').write('<!doctype html><meta charset=utf-8><title>Find site</title><body style="font:18px sans-serif">' + ''.join(f'<p>Line {i}: the {x} pulls the sleigh.</p>' for i, x in enumerate(w))
  + '<p>A <a href="/x">reindeer link</a> and a split rein<b>deer</b> word.</p><script>addEventListener("keydown",e=>{if((e.ctrlKey&&e.code=="KeyF")||e.code=="F3")document.title="the page saw "+e.code},true)</script>')
EOF
(cd $T/site && exec python3 -m http.server 8633 --bind 127.0.0.1 >/dev/null 2>&1) & SP=$!
SITE=http://127.0.0.1:8633/find.html
export LIBGL_ALWAYS_SOFTWARE=1
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
evp() { node -e '
const [port, pre, expr, one] = process.argv.slice(1);
(async () => { const l = await (await fetch("http://127.0.0.1:" + port + "/json/list")).json(); const ts = l.filter(x => x.url.startsWith(pre)).slice(0, one ? 1 : 99); if (!ts.length) { console.log("NO TARGET"); return }
  const r = []; for (const t of ts) r.push(await new Promise(res => { const ws = new WebSocket(t.webSocketDebuggerUrl); const to = setTimeout(() => res("TIMEOUT"), 8000);
    ws.onopen = () => ws.send(JSON.stringify({id: 1, method: "Runtime.evaluate", params: {expression: expr, returnByValue: true, awaitPromise: true, userGesture: true}}));
    ws.onmessage = e => { const m = JSON.parse(e.data); if (m.id == 1) { clearTimeout(to); ws.close(); res(m.result.result ? m.result.result.value : m.result) } } }));
  console.log(JSON.stringify(r.length == 1 ? r[0] : r)) })().catch(e => console.log("ERR " + e.message))' "$@" 2>&1; }
PG=http://127.0.0.1:8080/
pg() { evp $PORT $PG "$1" one; }
OW='(()=>(typeof ONEAPP=="object"&&ONEAPP&&OA.main)||wins.filter(w=>w.app=="browser"&&w.el.isConnected).sort((a,b)=>(+a.el.style.zIndex||0)-(+b.el.style.zIndex||0)).pop())()'
ow() { pg "(()=>{const W=$OW;if(!W)return null;return $1})()"; }
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS [$E] $1"; else echo "FAIL [$E] $1 - $(echo "$3" | head -c 600)"; F=$((F+1)); fi; }
st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
hit() { grep "^hit $1 $2 " $D/state | head -1 | awk '{print $4+int($6/2), $5+int($7/2)}'; }
start() {
	E=$1; D=$T/d; rm -rf $D $T/prof $T/run; mkdir -p $D $D/idbm $T/run; chmod 700 $T/run
	node $REPO/iso/qt/host/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
	echo $ID > $D/iwa-id; echo /run/rudolf/dl/.incoming > $D/dl-dir
	printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","b2":"#000080","tb":"bottom","accent":"#2563eb"},"pins":["notepad","browser"],"notes":0,"wallpaper":false}' > $D/settings.json
	[ $E = std ] && echo 'RUDOLFD.engine="chromium";' > $D/inject.js
	python3 $REPO/iso/qt/host/fixtures/native_rd.py 8080 $D > $T/rd-$E.log 2>&1 & RDP=$!
	Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
	export DISPLAY=$DSP; w8 'xdotool getmouselocation >/dev/null 2>&1' 40
	PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)")
	FL="--no-sandbox --user-data-dir=$T/prof --no-first-run --no-default-browser-check --password-store=basic --enable-features=SharedArrayBuffer,IsolatedWebApps,IsolatedWebAppDevMode --install-isolated-web-app-from-file=$T/shell.swbn --disable-gpu --lang=en-US"
	if [ $E = nat ]; then PORT=9345
		printf '%s\n' $ID "$CHROME" --class=RudolfOracle $FL --remote-debugging-port=$PORT > $T/oracle-chrome
		(cd $D && HOME=$D XDG_RUNTIME_DIR=$T/run RUDOLF_NATIVE_STATE=$D/state RUDOLF_ORACLE_CMD=$T/oracle-chrome RUDOLF_LAUNCH=$T/rudolf-launch exec $T/rudolf-native --size=1280x800 --draw=soft --base=http://127.0.0.1:8080 > $T/native.log 2>&1) & NP=$!
		w8 '[ "$(st signin)" = 1 ]' 240; k=0
		while ! { [ "$(st user)" = Ann ] && grep -q "^hit pin browser" $D/state; } && [ $k -lt 3 ]; do k=$((k + 1)); timeout 2 tail -f /dev/null
			[ $k -gt 1 ] && { xdotool key ctrl+a BackSpace; }
			LC_ALL=C.UTF-8 xdotool type --delay 30 "$PW"; xdotool key Return
			w8 '[ "$(st user)" = Ann ] && grep -q "^hit pin browser" $D/state' 120; done
		[ "$(st user)" = Ann ] || { echo "FAIL signing in (Native)"; tail $T/native.log; F=$((F+1)); return 1; }
		xdotool mousemove $(hit pin browser) click 1
		w8 '[ "$(pg "typeof rtOn==\"function\"&&rtOn()&&!!OA.main")" = true ]' 160 || { echo "FAIL Oracle (Native)"; tail $T/native.log; F=$((F+1)); return 1; }
	else PORT=9348
		$T/rudolf-launch $ID $CHROME --rudolf-kiosk --class=RudolfDesktop --kiosk --start-fullscreen --window-position=0,0 --window-size=1280,800 --noerrdialogs --disable-infobars $FL --remote-debugging-port=$PORT > $T/std.log 2>&1 & CP=$!
		w8 '[ "$(pg "typeof wins==\"object\"&&!!document.getElementById(\"lp\")")" = true ]' 240 || { echo "FAIL the desktop (Standard)"; F=$((F+1)); return 1; }
		SIGNED='(()=>{const l=document.getElementById("login");return !l||getComputedStyle(l).display=="none"})()'
		xdotool mousemove 640 400; k=0
		while [ "$(pg "$SIGNED")" != true ] && [ $k -lt 3 ]; do k=$((k + 1)); timeout 2 tail -f /dev/null
			pg '(()=>{const p=document.getElementById("lp");p.value="";p.focus();return 1})()' >/dev/null
			LC_ALL=C.UTF-8 xdotool type --delay 30 "$PW"; xdotool key Return
			w8 '[ "$(pg "$SIGNED")" = true ]' 80; done
		[ "$(pg "$SIGNED")" = true ] || { echo "FAIL signing in (Standard)"; F=$((F+1)); return 1; }
		timeout 3 tail -f /dev/null
		pg 'browser(),1' >/dev/null
		w8 '[ "$(ow "typeof rtOn==\"function\"&&rtOn()")" = true ]' 160 || { echo "FAIL Oracle (Standard)"; F=$((F+1)); return 1; }
	fi
}
stop() { kill $NP $CP $RDP 2>/dev/null; pkill -f "$T/prof" 2>/dev/null; pkill -f "$T/rudolf-native" 2>/dev/null; timeout 1.5 tail -f /dev/null; pkill -9 -f "$T/prof" 2>/dev/null; kill $XV 2>/dev/null; timeout 1 tail -f /dev/null; NP=; CP=; }
# (Ctrl+F and F3 are not among the keys rudolf-display hands over on Standard, so they are typed as they are on both)
key() { for k in "$@"; do xdotool key $k; done; }
bar() { ow '(()=>{const b=W.body.querySelector(".ofind");return b?(b.hidden?"hidden":"shown")+" "+b.querySelector(".ofn").textContent:"none"})()' | tr -d '"'; }
# (a point inside the site's page on the screen: the middle of its tab's area)
sitept() { ow '(()=>{const r=W.body.querySelector(".fh").getBoundingClientRect();return Math.round(r.left+r.width/2)+" "+Math.round(r.top+80)})()' | tr -d '"'; }
run() {
	ow "W.oNewTab('$SITE'),1" >/dev/null
	w8 '[ "$(evp $PORT $SITE "document.readyState" one)" = "\"complete\"" ]' 60 || { ok "the site in a real tab" 0 "$(evp $PORT $SITE 1 one)"; return; }
	timeout 1.5 tail -f /dev/null
	set -- $(sitept); OX=0; OY=0
	if [ $E = nat ]; then set -- $(grep "^client .* app=browser" $D/state | head -1); CW=0x$2; set -- $($T/xabs $CW); OX=$(($1 + 4)); OY=$(($2 + 28)); set -- $(sitept); fi
	xdotool mousemove $(($1 + OX)) $(($2 + OY)) click 1; timeout 0.8 tail -f /dev/null
	key ctrl+f; w8 '[ "$(bar)" != "hidden " ]' 20
	ok "Ctrl+F typed inside the website opens Oracle's find bar" "$([ "$(bar | cut -d' ' -f1)" = shown ] && echo 1)" "$(bar)"
	ok "...and the website's own Ctrl+F handler did not see it" "$([ "$(evp $PORT $SITE document.title one)" = '"Find site"' ] && echo 1)" "$(evp $PORT $SITE document.title one)"
	timeout 0.5 tail -f /dev/null; xdotool type --delay 40 reindeer
	w8 '[ "$(bar)" = "shown 1 of 17" ]' 24; ok 'the words typed: "1 of 17"' "$([ "$(bar)" = 'shown 1 of 17' ] && echo 1)" "$(bar)"
	H=$(evp $PORT isolated-app:// '[...TABS.values()].filter(t=>/find\.html/.test(t.url)).map(t=>t.fnat===true?"the frame'"'"'s own find":typeof t.f.find=="function"&&t.fnat!==false?"the frame'"'"'s own find (no answer yet)":"the walker (this Chromium'"'"'s Controlled Frame has no find)").join(",")' one)
	echo "      (answered by: $H)"
	xdotool key Return; w8 '[ "$(bar)" = "shown 2 of 17" ]' 16; ok 'Enter: "2 of 17"' "$([ "$(bar)" = 'shown 2 of 17' ] && echo 1)" "$(bar)"
	xdotool key shift+Return; w8 '[ "$(bar)" = "shown 1 of 17" ]' 16; ok 'Shift+Enter: "1 of 17"' "$([ "$(bar)" = 'shown 1 of 17' ] && echo 1)" "$(bar)"
	ow '(()=>{const c=W.body.querySelector(".ofc");c.checked=true;c.dispatchEvent(new Event("change"));return 1})()' >/dev/null
	w8 '[ "$(bar)" = "shown 1 of 12" ]' 16; ok 'Match case: "1 of 12" (the 5 "Reindeer" left out)' "$([ "$(bar)" = 'shown 1 of 12' ] && echo 1)" "$(bar)"
	ow '(()=>{const c=W.body.querySelector(".ofc");c.checked=false;c.dispatchEvent(new Event("change"));return 1})()' >/dev/null; w8 '[ "$(bar)" = "shown 1 of 17" ]' 16
	set -- $(sitept); xdotool mousemove $(($1 + OX)) $(($2 + OY)) click 1; timeout 0.6 tail -f /dev/null
	key F3; w8 '[ "$(bar)" = "shown 2 of 17" ]' 16; ok 'F3 inside the website: the next, "2 of 17"' "$([ "$(bar)" = 'shown 2 of 17' ] && echo 1)" "$(bar)"
	set -- $(ow '(()=>{const r=W.body.querySelector(".ofq").getBoundingClientRect();return Math.round(r.left+r.width/2)+" "+Math.round(r.top+r.height/2)})()' | tr -d '"'); xdotool mousemove $(($1 + OX)) $(($2 + OY)) click 1; timeout 0.4 tail -f /dev/null; xdotool key Escape; w8 '[ "$(bar | cut -d" " -f1)" = hidden ]' 12
	ok 'Escape: the bar closes' "$([ "$(bar | cut -d' ' -f1)" = hidden ] && echo 1)" "$(bar)"
}
[ $# -gt 0 ] || set -- std nat
for e in "$@"; do start $e && run; stop; done
echo "$((N - F))/$N passed"; [ $F = 0 ]
