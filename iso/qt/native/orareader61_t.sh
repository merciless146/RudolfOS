#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 10, docs/plan-oracle.md item 7): reader mode with the real shell, in a real Chromium with Isolated Web
# Apps (Playwright's, or CHROME), on Standard and on Native, as orafind61_t.sh (and same_oracle_t.sh) starts them. On each: an article in
# a real tab: 📖 shows once it has loaded (the shell's check in the page); F9 typed inside the website shows the article in reader mode
# (the page's HTML from the Controlled Frame, read with Readability in Oracle's page): its title and text, the website's frame hidden,
# the page's own script not run in it; F9 typed inside the reader gives the page back as it was (the website's frame shown again, what
# was typed into it still there). In network and mount namespaces of its own. Needs root, Xvfb, xdotool, node, python3, gcc, ip and
# WebKitGTK's headers.   sh orareader61_t.sh [nat|std ...]
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root (a network namespace)"; exit 0; }
[ -z "$OR61_INNER" ] && exec env OR61_INNER=1 unshare -n -m sh "$0" "$@"
command -v Xvfb >/dev/null && command -v xdotool >/dev/null && command -v ip >/dev/null || { echo "SKIP: needs Xvfb, xdotool and ip"; exit 0; }
CHROME=${CHROME:-$(ls -d /opt/pw-browsers/chromium-*/chrome-linux*/chrome 2>/dev/null | tail -1)}
[ -x "$CHROME" ] || { echo "SKIP: no Chromium (CHROME)"; exit 0; }
HERE=$(cd "$(dirname "$0")" && pwd); REPO=$(cd "$HERE/../../.." && pwd); T=$(mktemp -d /tmp/orareader.XXXXXX); DSP=${OR61_DSP:-:81}
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
python3 - $T/site/a.html <<'EOF2'
import sys
P = 'Reindeer are well adapted to the cold: their fur traps air, their noses warm the air they breathe, and their hooves change with the seasons. '
open(sys.argv[1], 'w').write('<!doctype html><meta charset=utf-8><title>The reindeer year</title><body style="font:18px sans-serif"><nav class="menu">Menu items</nav><article><h1>The reindeer year</h1>'
  + ''.join(f'<p>{P * 2}</p>' for _ in range(8)) + '<p><input id=i value=""></p></article><script>window.ran=(window.ran||0)+1;addEventListener("keydown",e=>{if(e.key=="F9")document.title="the page saw F9"},true)</script>')
EOF2
(cd $T/site && exec python3 -m http.server 8635 --bind 127.0.0.1 >/dev/null 2>&1) & SP=$!
SITE=http://127.0.0.1:8635/a.html
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
	if [ $E = nat ]; then PORT=9365
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
	else PORT=9368
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
key() { for k in "$@"; do xdotool key $k; done; }   # (F9 is not among the keys rudolf-display or rudolf-native hand over)
# (every Oracle window's tabs, by title: Native's windows are pages of their own, Standard's are windows of one page)
alltabs() { evp $PORT $PG '(()=>{const L=typeof ONEAPP=="object"&&ONEAPP?[OA.main]:wins.filter(w=>w.app=="browser");return L.filter(w=>w&&w.el&&w.el.isConnected).map(W=>[...W.body.querySelectorAll(".otab")].map(d=>d.title||d.textContent).join("+")).join(" | ")})()' | tr -d '"[]' | sed 's/,/ | /g'; }
xy() { ow "(()=>{const r=$1.getBoundingClientRect();return Math.round(r.left+r.width/2)+' '+Math.round(r.top+r.height/2)})()" | tr -d '"'; }
offs() { OX=0; OY=0; if [ $E = nat ]; then set -- $(grep "^client .* app=browser" $D/state | head -1); CW=0x$2; set -- $($T/xabs $CW); OX=$(($1 + 4)); OY=$(($2 + 28)); fi; }
clickin() { set -- $(xy "$1"); xdotool mousemove $(($1 + OX)) $(($2 + OY)) click 1; timeout 0.6 tail -f /dev/null; }
rdr() { ow '(()=>{const f=[...W.body.querySelectorAll(".fh iframe[srcdoc]")].find(x=>x.style.display!="none"&&/Content-Security-Policy/.test(x.srcdoc));if(!f)return "none";const m=/<h1 id="t">([^<]*)<\/h1>/.exec(f.srcdoc);return (m?m[1]:"?")+(/hooves change/.test(f.srcdoc)?" +text":"")+(/Menu items/.test(f.srcdoc)?" +menu":"")})()' | tr -d '"'; }
vis() { evp $PORT isolated-app:// '[...TABS.values()].filter(t=>/a\.html/.test(t.url)).map(t=>t.f.style.visibility).join(",")' one | tr -d '"'; }
run() {
	ow "W.oNewTab('$SITE'),1" >/dev/null
	w8 '[ "$(evp $PORT $SITE "document.readyState" one)" = "\"complete\"" ]' 60 || { ok "the site in a real tab" 0 "$(evp $PORT $SITE 1 one)"; return; }
	w8 '[ "$(ow "!W.body.querySelector(\".ord\").hidden")" = true ]' 20
	ok "the article has loaded: 📖 shows (the shell's check in the page)" "$([ "$(ow '!W.body.querySelector(".ord").hidden')" = true ] && echo 1)" "$(ow 'W.body.querySelector(".ord").outerHTML')"
	offs; clickin 'W.body.querySelector(".fh")'; evp $PORT $SITE 'document.getElementById("i").value="typed before",1' one >/dev/null
	key F9; w8 '[ "$(rdr)" != none ]' 40
	ok "F9 inside the website: the article in reader mode (its title and text, not the menu)" "$([ "$(rdr)" = "The reindeer year +text" ] && echo 1)" "$(rdr)"
	ok "...the website's frame hidden meanwhile" "$([ "$(vis)" = hidden ] && echo 1)" "$(vis)"
	ok "...and the page's own key handler did not see F9" "$([ "$(evp $PORT $SITE document.title one)" = '"The reindeer year"' ] && echo 1)" "$(evp $PORT $SITE document.title one)"
	clickin '[...W.body.querySelectorAll(".fh iframe[srcdoc]")].find(x=>x.style.display!="none")'; key F9; w8 '[ "$(rdr)" = none ]' 20
	ok "F9 inside the reader: the page back" "$([ "$(rdr)" = none ] && [ "$(vis)" = visible ] && echo 1)" "$(rdr) $(vis)"
	ok "...as it was (not loaded again: what was typed is there, its script ran once)" "$([ "$(evp $PORT $SITE '[document.getElementById("i").value,window.ran].join(" ")' one)" = '"typed before 1"' ] && echo 1)" "$(evp $PORT $SITE '[document.getElementById("i").value,window.ran].join(" ")' one)"
}
[ $# -gt 0 ] || set -- std nat
for e in "$@"; do start $e && run; stop; done
echo "$((N - F))/$N passed"; [ $F = 0 ]
