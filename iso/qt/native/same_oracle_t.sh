#!/bin/sh
# 1.6.0 (docs/plan-1.6.0.md Part 12, "The rule", and 1.5.9 12.3c; "Before stable" item 9): Oracle's window on Native compared
# with Standard's. Native: rudolf-native under Xvfb, Oracle in Rudolf's Chromium (the shell, an Isolated Web App built with a
# test key, at #app=browser: the desktop page in one-app mode with the shell's real tabs), framed by rudolf-native, as
# oracle_native_t.sh runs it. Standard: the same shell in the same Chromium as the session starts it (--class=RudolfDesktop,
# full screen), the desktop page in it and Oracle a window of the page; keys through rudolf-display's own key code
# (qt/host/fixtures/dispkeys.c). One stand-in rudolfd (qt/host/fixtures/native_rd.py) with the same settings and account for
# both, one engine after the other, the same screen size (1280x800). Oracle's window is given the same size on both.
# Looks (same_oracle.py, as same_t.sh: a part fails over SAME_LIMIT % of its pixels differing by more than SAME_TOL in a colour;
# both pictures and the difference kept in OUT): the title bar, the toolbar, the address box, the tabs, the bookmarks bar, the
# Home page, a local test site, Oracle's menu, a download (about:downloads), dark and light mode, another accent colour.
# Works (the same steps on both, their outcomes compared): a new tab (+), closing a tab (its ✕), Ctrl+T, Ctrl+W and Ctrl+N
# in a website, Alt+F4, back and forward, a website's full screen.
# In network and mount namespaces of its own (the shell's desktop is http://127.0.0.1:8080). Needs root, Xvfb, xdotool,
# ImageMagick, node, python3 with PIL, gcc, ip and WebKitGTK's headers.   sh same_oracle_t.sh [OUT] [CASE,...]
# (Debugging: SO60_ONLY=nat|std one engine (std alone with SO60_GEO="X Y W H", Native's frame); SO60_LOOKSONLY=1 no steps;
# SO60_HOLD=SECONDS each engine kept running at the end.)
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root (a network namespace)"; exit 0; }
[ -z "$SO60_INNER" ] && exec env SO60_INNER=1 unshare -n -m sh "$0" "$@"
command -v Xvfb >/dev/null && command -v xdotool >/dev/null && command -v import >/dev/null && command -v ip >/dev/null || { echo "SKIP: needs Xvfb, xdotool, ImageMagick and ip"; exit 0; }
python3 -c 'import PIL' 2>/dev/null || { echo "SKIP: needs python3's PIL"; exit 0; }
CHROME=${CHROME:-$(ls -d /opt/pw-browsers/chromium-*/chrome-linux*/chrome 2>/dev/null | tail -1)}
[ -x "$CHROME" ] || { echo "SKIP: no Chromium (CHROME)"; exit 0; }
HERE=$(cd "$(dirname "$0")" && pwd); REPO=$(cd "$HERE/../../.." && pwd); T=$(mktemp -d /tmp/sameora.XXXXXX); DSP=${SO60_DSP:-:84}
OUT=${1:-$T/out}; ONLY=$2; mkdir -p "$OUT"
ip link set lo up
# Rudolf OS's Chromium policy (downloads into /run/rudolf/dl/.incoming; websites open no file dialog), in this mount namespace only
mount --make-rprivate / 2>/dev/null; mount -t tmpfs none /run && mkdir -p /run/rudolf/dl/.incoming || { echo "FAIL /run"; exit 1; }
for p in /etc/chromium/policies/managed /etc/opt/chrome_for_testing/policies/managed; do [ -d $p ] && mount -t tmpfs none $p && cp $REPO/iso/sys/root/etc/chromium/policies/managed/rudolf.json $p/; done
trap 'kill $NP $RDP $XV $SP $CP 2>/dev/null; pkill -f "$T/" 2>/dev/null; timeout 1 tail -f /dev/null; pkill -9 -f "$T/" 2>/dev/null; for f in $T/*; do [ "$f" = "$OUT" ] || rm -rf "$f"; done; [ "$OUT" = "$T/out" ] || rm -rf $T' EXIT
gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $T/rudolf-native $REPO/iso/sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
gcc -O2 -Wall -o $T/rudolf-launch $REPO/iso/sys/rudolf-launch.c 2>>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
L=/usr/lib/x86_64-linux-gnu; gcc -O2 -w -o $T/dispkeys $REPO/iso/qt/host/fixtures/dispkeys.c -I$REPO/iso/sys -lX11 $L/libXtst.so.6 $L/libXfixes.so.3 $L/libXrandr.so.2 -lm 2>>$T/cc || { cat $T/cc; echo "FAIL build dispkeys"; exit 1; }
printf '%s\n' '#include <X11/Xlib.h>' '#include <stdio.h>' '#include <stdlib.h>' 'int main(int c,char**v){Display*d=XOpenDisplay(0);int x,y;Window k;if(!d||c<2)return 1;XTranslateCoordinates(d,strtoul(v[1],0,0),DefaultRootWindow(d),0,0,&x,&y,&k);printf("%d %d\n",x,y);return 0;}' > $T/xabs.c
gcc -o $T/xabs $T/xabs.c -lX11 || { echo "FAIL build xabs"; exit 1; }
python3 $REPO/iso/systools/mkiwa.py key $T/test.key >/dev/null; ID=$(python3 $REPO/iso/systools/mkiwa.py id $T/test.key)
python3 $REPO/iso/systools/mkiwa.py build $REPO/iso/shell $T/shell.swbn $T/test.key >/dev/null || { echo "FAIL the shell"; exit 1; }
# a local test site (a page of its own, its second page, a download, a full-screen button)
mkdir -p $T/site
printf '%s' '<!doctype html><meta charset=utf-8><title>Test site</title><body style="margin:0;font:16px sans-serif;background:#fdf6e3;color:#073642"><h1 style="margin:20px;color:#b58900">A local test site</h1><p style="margin:20px">Rudolf OS draws this page the same way on both desktops.</p><p style="margin:20px"><a id=next href="/two.html">The second page</a> · <a id=dl href="/notes60.txt" download>A download</a></p><div style="margin:20px;width:240px;height:60px;background:linear-gradient(90deg,#268bd2,#d33682)"></div><input id=i style="margin:20px"><script>addEventListener("click",e=>{if(e.target.id=="fs")document.documentElement.requestFullscreen()})</script><button id=fs style="margin:20px">Full screen</button></body>' > $T/site/index.html
printf '%s' '<!doctype html><title>Second page</title><body style="margin:0;font:16px sans-serif;background:#eef"><h1 style="margin:20px">The second page</h1></body>' > $T/site/two.html
printf 'A download from Oracle.\n' > $T/site/notes60.txt
(cd $T/site && exec python3 -m http.server 8333 --bind 127.0.0.1 >/dev/null 2>&1) & SP=$!
export LIBGL_ALWAYS_SOFTWARE=1
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
# (DevTools: PORT PREFIX JS [one])
evp() { node -e '
const [port, pre, expr, one] = process.argv.slice(1);
(async () => { const l = await (await fetch("http://127.0.0.1:" + port + "/json/list")).json(); const ts = l.filter(x => x.url.startsWith(pre)).slice(0, one ? 1 : 99); if (!ts.length) { console.log("NO TARGET"); return }
  const r = []; for (const t of ts) r.push(await new Promise(res => { const ws = new WebSocket(t.webSocketDebuggerUrl); const to = setTimeout(() => res("TIMEOUT"), 8000);
    ws.onopen = () => ws.send(JSON.stringify({id: 1, method: "Runtime.evaluate", params: {expression: expr, returnByValue: true, awaitPromise: true, userGesture: true}}));
    ws.onmessage = e => { const m = JSON.parse(e.data); if (m.id == 1) { clearTimeout(to); ws.close(); res(m.result.result ? m.result.result.value : m.result) } } }));
  console.log(JSON.stringify(r.length == 1 ? r[0] : r)) })().catch(e => console.log("ERR " + e.message))' "$@" 2>&1; }
targets() { node -e 'fetch("http://127.0.0.1:'$PORT'/json/list").then(r=>r.json()).then(l=>console.log(l.map(t=>t.type+" "+t.url).join("\n"))).catch(()=>{})' 2>/dev/null; }
PG=http://127.0.0.1:8080/
pg() { evp $PORT $PG "$1" one; }
# (Oracle's window in the page: the one-app page's own on Native, the front browser window of the desktop's on Standard)
OW='(()=>(typeof ONEAPP=="object"&&ONEAPP&&OA.main)||wins.filter(w=>w.app=="browser"&&w.el.isConnected).sort((a,b)=>(+a.el.style.zIndex||0)-(+b.el.style.zIndex||0)).pop())()'
ow() { pg "(()=>{const W=$OW;if(!W)return null;return $1})()"; }
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }

# ---- one engine: E = nat | std
start() {
	E=$1; D=$T/d; rm -rf $D $T/prof $T/run; mkdir -p $D $D/idbm $T/run; chmod 700 $T/run
	node $REPO/iso/qt/host/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
	echo $ID > $D/iwa-id; echo /run/rudolf/dl/.incoming > $D/dl-dir
	printf '%s\n' "{\"s\":{\"bs\":\"solid\",\"b1\":\"#112233\",\"b2\":\"#000080\",\"tb\":\"bottom\",\"accent\":\"#2563eb\"},\"pins\":[\"notepad\",\"browser\"],\"notes\":0,\"wallpaper\":false}" > $D/settings.json
	[ $E = std ] && echo 'RUDOLFD.engine="chromium";' > $D/inject.js
	python3 $REPO/iso/qt/host/fixtures/native_rd.py 8080 $D > $T/rd-$E.log 2>&1 & RDP=$!
	Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
	export DISPLAY=$DSP; w8 'xdotool getmouselocation >/dev/null 2>&1' 40
	PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)")
	FL="--no-sandbox --user-data-dir=$T/prof --no-first-run --no-default-browser-check --password-store=basic --enable-features=SharedArrayBuffer,IsolatedWebApps,IsolatedWebAppDevMode --install-isolated-web-app-from-file=$T/shell.swbn --disable-gpu --autoplay-policy=no-user-gesture-required --use-fake-ui-for-media-stream --lang=en-US"
	if [ $E = nat ]; then PORT=9335
		printf '%s\n' $ID "$CHROME" --class=RudolfOracle $FL --remote-debugging-port=$PORT > $T/oracle-chrome
		(cd $D && HOME=$D XDG_RUNTIME_DIR=$T/run RUDOLF_NATIVE_STATE=$D/state RUDOLF_ORACLE_CMD=$T/oracle-chrome RUDOLF_LAUNCH=$T/rudolf-launch exec env ${SO60_LDEBUG:+RUDOLF_LAUNCH_DEBUG=1} $T/rudolf-native --size=1280x800 --draw=soft --base=http://127.0.0.1:8080 > $T/native.log 2>&1) & NP=$!
		w8 '[ "$(st signin)" = 1 ]' 120
		LC_ALL=C.UTF-8 xdotool type --delay 30 "$PW"; xdotool key Return
		w8 '[ "$(st user)" = Ann ] && grep -q "^hit pin browser" $D/state' 120 || { echo "FAIL signing in (Native)"; tail $T/native.log; exit 1; }
		xdotool mousemove $(hit pin browser) click 1
		w8 '[ "$(pg "typeof rtOn==\"function\"&&rtOn()&&!!OA.main")" = true ]' 160 || { echo "FAIL Oracle (Native)"; tail $T/native.log; exit 1; }
		w8 'grep -q "^client .* app=browser Oracle" $D/state' 60
	else PORT=9338
		# (as the session starts Standard: the shell full screen, class RudolfDesktop)
		$T/rudolf-launch $ID $CHROME --rudolf-kiosk --class=RudolfDesktop --kiosk --start-fullscreen --window-position=0,0 --window-size=1280,800 --noerrdialogs --disable-infobars $FL --remote-debugging-port=$PORT > $T/std.log 2>&1 & CP=$!
		# (signed in already when the stand-in's account has no password asked; otherwise its password typed)
		w8 '[ "$(pg "typeof wins==\"object\"&&!!document.getElementById(\"lp\")")" = true ]' 240 || { echo "FAIL the desktop (Standard): $(targets)"; grep -v "^\[" $T/std.log | tail -5; exit 1; }
		SIGNED='(()=>{const l=document.getElementById("login");return !l||getComputedStyle(l).display=="none"})()'
		xdotool mousemove 640 400; k=0
		while [ "$(pg "$SIGNED")" != true ] && [ $k -lt 3 ]; do k=$((k + 1)); timeout 2 tail -f /dev/null
			pg '(()=>{const p=document.getElementById("lp");p.value="";p.focus();return 1})()' >/dev/null
			LC_ALL=C.UTF-8 xdotool type --delay 30 "$PW"; xdotool key Return
			w8 '[ "$(pg "$SIGNED")" = true ]' 80; done
		[ "$(pg "$SIGNED")" = true ] || { echo "FAIL signing in (Standard)"; exit 1; }
		timeout 3 tail -f /dev/null
		pg 'browser(),1' >/dev/null
		w8 '[ "$(ow "typeof rtOn==\"function\"&&rtOn()")" = true ]' 160 || { echo "FAIL Oracle (Standard): $(pg "[typeof rtOn,typeof rtOn==\"function\"&&rtOn(),typeof OA,typeof OA==\"object\"&&!!OA.main,USER.n,location.href]")"; exit 1; }
	fi
}
st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
hit() { grep "^hit $1 $2 " $D/state | head -1 | awk '{print $4+int($6/2), $5+int($7/2)}'; }
stop() { kill $NP $CP $RDP 2>/dev/null; pkill -f "$T/prof" 2>/dev/null; pkill -f "$T/rudolf-native" 2>/dev/null; timeout 1.5 tail -f /dev/null; pkill -9 -f "$T/prof" 2>/dev/null; kill $XV 2>/dev/null; timeout 1 tail -f /dev/null; NP=; CP=; }

# ---- the same settings on both (same_state.js's base): the page's own, and on Native the native desktop's (rudolfd's desktop/settings)
BASE='{"bs":"solid","b1":"#1e3a5f","b2":"#000080","tb":"bottom","accent":"#2563eb","h24":false,"df":"mdy","tz":"UTC","icons":"rudolf","scale":100}'
sets() {
	pg "(()=>{Object.assign(S,$BASE,$1);persistS();try{applySet()}catch(e){}try{a11yApply()}catch(e){}try{themeApply()}catch(e){}return JSON.stringify(S)})()" > $T/S.json
	[ $E = nat ] && node -e 'const fs=require("fs"),[f,s]=process.argv.slice(1);const j=JSON.parse(fs.readFileSync(f));j.s=JSON.parse(JSON.parse(fs.readFileSync(s,"utf8")));fs.writeFileSync(f+".t",JSON.stringify(j));fs.renameSync(f+".t",f)' $D/settings.json $T/S.json
	timeout 2 tail -f /dev/null
}
# ---- where Oracle's window is: Native's frame (FX FY FW FH), its page's origin on the screen (OX OY); Standard's window put at the same place and size
geo() {
	if [ $E = std ] && [ -n "$SO60_GEO" ] && [ -z "$FX" ]; then set -- $SO60_GEO; FX=$1; FY=$2; FW=$3; FH=$4; fi
	if [ $E = nat ]; then set -- $(grep "^client .* app=browser" $D/state | head -1); CW=0x$2; FR=0x$3
		set -- $($T/xabs $FR); FX=$1; FY=$2; set -- $(xdotool getwindowgeometry $FR | sed -n 's/.*Geometry: \([0-9]*\)x\([0-9]*\).*/\1 \2/p'); FW=$1; FH=$2
		set -- $($T/xabs $CW); OX=$(($1 + 4)); OY=$(($2 + 28))
	else
		pg "(()=>{const W=$OW,e=W.el;e.classList.remove('max');e.style.left='${FX}px';e.style.top='${FY}px';e.style.width='${FW}px';e.style.height='${FH}px';const r=e.getBoundingClientRect();e.style.width=(2*$FW-r.width)+'px';e.style.height=(2*$FH-r.height)+'px';dispatchEvent(new Event('resize'));return 1})()" >/dev/null
		timeout 1 tail -f /dev/null; OX=0; OY=0; echo "geo: $FX $FY $FW $FH -> $(ow '[W.el.getBoundingClientRect()].map(r=>[r.left,r.top,r.width,r.height].join(" "))[0]')"
	fi
}
# (the parts of Oracle's window on the screen, as JSON: x, y, w, h each; Native's title bar is the frame's own, Standard's the window's)
PARTS="(()=>{const W=$OW,b=W.body.getBoundingClientRect(),q=s=>W.body.querySelector(s);const r=e=>{if(!e||!e.isConnected)return null;const k=e.getBoundingClientRect();return [k.left,k.top,k.width,k.height].map(Math.round)};
 const t=W.el.querySelector('.tb'),tv=t&&getComputedStyle(t).display!='none';let tab=null;for(const[,x]of (typeof RT=='object'?RT:[]))if(x.shown()&&(!tab||x.z()>=tab.z()))tab=x;
 return JSON.stringify({title:tv?r(t):null,win:tv?r(W.el):null,tabs:r(q('.otabs')),toolbar:r(q('.ob')),address:r(q('.ad')),bookmarks:r(q('.obm')),page:r(q('.fh')),status:r(q('.stat')),menu:r(q('.odd')),plus:r(q('.otabs > button')),tabx:r(q('.otab.on u')),back:r(q('.ob .bk')),fwd:r(q('.ob .fw')),menubtn:r(q('.ob .mn')),tab:tab?r(tab.el):null})})()"
snap() {   # snap CASE: the screen and the parts (in screen pixels)
	timeout ${2:-1.5} tail -f /dev/null; import -window root $OUT/$E-$1.png
	# (the title bar's words: Standard's window's; Native's frame's, with Chromium's own name for its window, for a failure's details)
	if [ $E = nat ]; then set -- "$1" $(grep "^client .* app=browser" $D/state | head -1); t=$(echo "$*" | sed 's/.* app=browser //'); echo "$1 $t" >> $OUT/$E-titles.txt; echo "$1 (X: $(xdotool getwindowname 0x$3 2>&1))" >> $OUT/$E-xnames.txt
	else echo "$1 $(ow 'W.el.querySelector(".tb .t").textContent' | sed 's/^"//; s/"$//')" >> $OUT/$E-titles.txt; fi
	pg "$PARTS" | node -e 'let s="";process.stdin.on("data",d=>s+=d).on("end",()=>{const [E,ox,oy,fx,fy,fw,fh,th]=process.argv.slice(1);const p=JSON.parse(JSON.parse(s));for(const k in p)if(p[k]){p[k][0]+=+ox;p[k][1]+=+oy}
		if(E=="nat"){const bw=+ox-+fx;p.title=[+fx+bw,+fy+bw,+fw-2*bw,+th-bw];p.win=[+fx,+fy,+fw,+fh]}console.log(JSON.stringify(p))})' $E $OX $OY $FX $FY $FW $FH $((OY - FY)) > $OUT/$E-$1.json
}
key() { if [ $E = std ]; then $T/dispkeys feed 0 "$@"; else for k in "$@"; do xdotool key $k; done; fi; }
click() { xdotool mousemove $1 $2 click 1; timeout 0.6 tail -f /dev/null; }
mid() {   # mid PART [FX FY]: a point of a part on the screen now (its middle, or at those fractions of it)
	pg "$PARTS" | node -e 'let s="";process.stdin.on("data",d=>s+=d).on("end",()=>{const [k,fx,fy,ox,oy]=process.argv.slice(1);const p=JSON.parse(JSON.parse(s))[k];console.log(p?(p[0]+(+ox)+Math.round(p[2]*(+fx||.5)))+" "+(p[1]+(+oy)+Math.round(p[3]*(+fy||.5))):"0 0")})' $1 "$2" "$3" $OX $OY; }
ntabs() { ow 'W.body.querySelectorAll(".otab").length'; }
nwin() { if [ $E = nat ]; then grep -c "^client .* app=browser" $D/state; else pg 'wins.filter(w=>w.app=="browser"&&w.el.isConnected).length'; fi; }
# what each engine did in the steps (the "works" checks), one line a step, compared at the end
did() { echo "$1 $2" >> $OUT/$E-did.txt; }
# (what the address box says once it has WORDS in it, waited for up to 8 s)
adis() { AW=$1; w8 'ow "W.body.querySelector(\".ad\").value" | grep -qF -- "$AW"' 32; ow 'W.body.querySelector(".ad").value'; }
# (whether the address box has the keyboard, waited for up to 4 s)
adkb() { w8 '[ "$(ow "document.activeElement==W.body.querySelector(\".ad\")")" = true ]' 16; ow 'document.activeElement==W.body.querySelector(".ad")'; }
# (whether the site's colour covers the whole screen, polled for up to 5 s until it is WANT: yes or no)
fscov() { i=0; while :; do import -window root $T/fs.png; c=$(python3 -c 'import sys;from PIL import Image;I=Image.open(sys.argv[1]).convert("RGB");print("yes" if I.getpixel((5,5))==I.getpixel((1270,790))==(253,246,227) else "no")' $T/fs.png); [ "$c" = $1 ] || [ $i -ge 10 ] && break; i=$((i+1)); timeout 0.5 tail -f /dev/null; done; echo $c; }

run() {
	: > $OUT/$E-did.txt; : > $OUT/$E-titles.txt
	sets '{}'; geo
	# ---- looks
	snap home 3
	ad=$(mid address); click $ad; xdotool type --delay 20 "http://127.0.0.1:8333/"; xdotool key Return
	w8 '[ "$(evp $PORT http://127.0.0.1:8333/ "document.title" one)" = "\"Test site\"" ]' 60; snap site 3
	click $(mid menubtn); snap menu 1; xdotool key Escape; click $(mid address .3 .5); xdotool key Escape
	evp $PORT http://127.0.0.1:8333/ 'document.getElementById("dl").click(),1' one >/dev/null; timeout 3 tail -f /dev/null
	click $(mid address); xdotool key ctrl+a; xdotool type --delay 20 "about:downloads"; xdotool key Return; snap download 3
	sets '{"dm":"dark","darkui":true}'; snap dark 2
	sets '{"dm":"light","darkui":false}'; snap light 2
	sets '{"accent":"#16a34a"}'; snap accent 2
	sets '{}'
	[ -n "$SO60_LOOKSONLY" ] && return 0
	# ---- works: the same steps, what came of them (the site opened again at its own address for these: ?w)
	K=0
	# (the site opened in a new tab at an address of its own, ?wK; its tab brought to the front by its address)
	siteopen() { K=$((K + 1)); SW="http://127.0.0.1:8333/?w$K"; ow "W.oNewTab('$SW'),1" >/dev/null; w8 '[ "$(evp $PORT "$SW" "document.readyState" one)" = "\"complete\"" ]' 40; timeout 1 tail -f /dev/null; }
	sitetab() { evp $PORT "$SW" 'document.title="Site w'$K'",1' one >/dev/null; timeout 2 tail -f /dev/null
		ow '(()=>{const o=[...W.body.querySelectorAll(".otab")].find(t=>/Site w'$K'/.test(t.textContent));o&&o.click();return !!o})()' >/dev/null; timeout 1 tail -f /dev/null; }
	# (a point of an element of the site's page on the screen; a click there gives the site the keyboard)
	sitexy() { set -- "$1" $(mid tab 0.0001 0.0001); o=$(evp $PORT "$SW" "(()=>{const r=document.querySelector('$1').getBoundingClientRect();return Math.round(r.left+r.width/2)+' '+Math.round(r.top+r.height/2)})()" one | tr -d '"'); set -- $2 $3 $o; echo $(($1 + $3)) $(($2 + $4)); }
	insite() { click $(sitexy h1); }
	siteopen
	n=$(ntabs); click $(mid plus); did "a new tab (+)" "$(($(ntabs) - n)) more tab, windows $(nwin), the address box has the keyboard: $(adkb)"
	n=$(ntabs); key ctrl+w; timeout 1 tail -f /dev/null; did "Ctrl+W in Oracle's page (its new tab)" "$((n - $(ntabs))) fewer, windows $(nwin)"
	n=$(ntabs); click $(mid tabx); timeout 1 tail -f /dev/null; did "a tab's ✕" "$((n - $(ntabs))) fewer, windows $(nwin)"
	siteopen; insite; n=$(ntabs); key ctrl+t; w8 '[ "$(ntabs)" != $n ]' 12; did "Ctrl+T in a website" "$(($(ntabs) - n)) more tab, windows $(nwin), the address box has the keyboard: $(adkb)"
	key ctrl+w; timeout 1 tail -f /dev/null; did "...Ctrl+W there (the new tab's page)" "$((n - $(ntabs) + 1)) fewer, windows $(nwin)"
	sitetab; insite; n=$(ntabs); key ctrl+w; timeout 1.5 tail -f /dev/null; did "Ctrl+W in a website" "$((n - $(ntabs))) fewer tab, windows $(nwin), the site's tab gone: $([ "$(evp $PORT "$SW" 1 one)" = "NO TARGET" ] && echo yes || echo no)"
	siteopen; click $(sitexy '#next'); w8 'targets | grep -q "^webview http://127.0.0.1:8333/two.html"' 32
	did "a link followed (its page in the tab)" "$(targets | grep -c '^webview http://127.0.0.1:8333/two.html')"
	click $(mid back); did "back (◀)" "$(adis "?w$K")"
	click $(mid fwd); did "forward (▶)" "$(adis two.html)"
	click $(mid tab .5 .5); key alt+Left; did "Alt+Left in a website" "$(adis "?w$K")"
	click $(sitexy '#fs'); did "a website's full screen (its button): covers the screen" "$(fscov yes)"
	key Escape; did "...Esc: covers the screen" "$(fscov no)"
	n=$(nwin); insite; key ctrl+n; w8 '[ "$(nwin)" != $n ]' 24; timeout 1 tail -f /dev/null; did "Ctrl+N in a website" "windows $n -> $(nwin)"
	n=$(nwin); key alt+F4; w8 '[ "$(nwin)" != $n ]' 16; did "Alt+F4 (the new window in front)" "windows $(nwin)"
}

[ -n "$SO60_ONLY" ] || SO60_ONLY="nat std"
for e in $SO60_ONLY; do start $e; run
	[ -n "$SO60_HOLD" ] && { echo "HOLD $e $T $$"; timeout $SO60_HOLD tail -f /dev/null; }
	stop; done
echo "pictures: $OUT"
python3 $HERE/same_oracle.py $OUT $ONLY
