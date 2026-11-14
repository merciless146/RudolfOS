#!/bin/sh
# 1.5.9 (docs/plan-1.5.9.md 12.3c): Oracle on the native desktop runs in Rudolf's own Chromium with the shell's real tabs,
# as on Standard. rudolf-native under Xvfb with a stand-in rudolfd (qt/host/fixtures/native_rd.py) serving the real page,
# the shell (iso/shell) built with a test key, and a Chromium with Isolated Web Apps (CHROME; Playwright's by default):
# Oracle opened from its pin -> one Chromium (--class=RudolfOracle, through rudolf-launch --oracle), its window framed by
# the native desktop as Oracle (no title bar of Chromium's own), the page in one-app mode with the shell's real tabs
# (rtOn()), a local site in a real tab (a Controlled Frame), a second window (New private tab) in the same Chromium, the
# last window closed -> no Chromium left; the site's cookie kept in the profile (seen when Oracle opens again); without
# Chromium, Oracle's WebKit window as before, said once.
# Runs in a network namespace of its own (the shell's desktop is http://127.0.0.1:8080). Needs root, Xvfb, xdotool,
# ImageMagick, node, python3, gcc, ip and WebKitGTK's headers.   sh oracle_native_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root (a network namespace)"; exit 0; }
[ -z "$ON59_INNER" ] && exec env ON59_INNER=1 unshare -n sh "$0" "$@"
command -v Xvfb >/dev/null && command -v xdotool >/dev/null && command -v convert >/dev/null && command -v ip >/dev/null || { echo "SKIP: needs Xvfb, xdotool, ImageMagick and ip"; exit 0; }
CHROME=${CHROME:-$(ls -d /opt/pw-browsers/chromium-*/chrome-linux*/chrome 2>/dev/null | tail -1)}
[ -x "$CHROME" ] || { echo "SKIP: no Chromium (CHROME)"; exit 0; }
HERE=$(cd "$(dirname "$0")" && pwd); REPO=$(cd "$HERE/../../.." && pwd); T=$(mktemp -d /tmp/oranat.XXXXXX); DSP=${ON59_DSP:-:86}
ip link set lo up
trap 'kill $NP $RDP $XV $SP 2>/dev/null; pkill -f "$T/" 2>/dev/null; timeout 1 tail -f /dev/null; pkill -9 -f "$T/" 2>/dev/null; rm -rf $T' EXIT
gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $T/rudolf-native $REPO/iso/sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
gcc -O2 -Wall -o $T/rudolf-launch $REPO/iso/sys/rudolf-launch.c 2>>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
grep -q warning $T/cc && { cat $T/cc; echo "FAIL build warnings"; exit 1; }
# the shell, signed with a test key, installed from its file as the session does in developer mode
python3 $REPO/iso/systools/mkiwa.py key $T/test.key >/dev/null; ID=$(python3 $REPO/iso/systools/mkiwa.py id $T/test.key)
python3 $REPO/iso/systools/mkiwa.py build $REPO/iso/shell $T/shell.swbn $T/test.key >/dev/null || { echo "FAIL the shell"; exit 1; }
D=$T/d; mkdir -p $D; node $REPO/iso/qt/host/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
echo $ID > $D/iwa-id
printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","b2":"#000080","tb":"bottom","accent":"#2563eb"},"pins":["notepad","browser"],"notes":0,"wallpaper":false}' > $D/settings.json
# a website: it keeps a cookie
mkdir -p $T/site; printf '%s' '<!doctype html><title>Test site</title><p id=p>site</p><script>if(!document.cookie.includes("oracle59="))document.cookie="oracle59=kept; max-age=86400; path=/"</script>' > $T/site/index.html
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
const [pre, expr] = process.argv.slice(1);
(async () => { const l = await (await fetch("http://127.0.0.1:9335/json/list")).json(); const ts = l.filter(x => x.url.startsWith(pre)); if (!ts.length) { console.log("NO TARGET"); return }
  const r = []; for (const t of ts) r.push(await new Promise(res => { const ws = new WebSocket(t.webSocketDebuggerUrl); const to = setTimeout(() => res("TIMEOUT"), 8000);
    ws.onopen = () => ws.send(JSON.stringify({id: 1, method: "Runtime.evaluate", params: {expression: expr, returnByValue: true, awaitPromise: true}}));
    ws.onmessage = e => { const m = JSON.parse(e.data); if (m.id == 1) { clearTimeout(to); ws.close(); res(m.result.result ? m.result.result.value : m.result) } } }));
  console.log(JSON.stringify(r.length == 1 ? r[0] : r)) })().catch(e => console.log("ERR " + e.message))' "$1" "$2" 2>&1; }
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
# ---- a second window: the pin's Open a new window, in the same Chromium (the pin itself brings Oracle to the front)
xdotool mousemove $(hit pin browser) click 1; timeout 2 tail -f /dev/null; n1=$(grep -c "^client .* app=browser" $D/state)
xdotool mousemove $(hit pin browser) click 3; w8 'grep -q "^cmitem .*pnew:browser" $D/state' 40
P=$(grep "^cmitem .*pnew:browser" $D/state | head -1 | awk '{print $2, $3}'); cl=$(grep "^cmitem .*pclose:browser" $D/state | awk '{print $4}'); xdotool mousemove $P click 1
w8 '[ "$(grep -c "^client .* app=browser" $D/state)" = 2 ]' 120
ok "5. the pin while Oracle is open: Oracle to the front (no second window); its menu's Close all windows not greyed" "$([ "$n1" = 1 ] && [ "$cl" = 0 ] && echo 1)" "$n1 $cl"
ok "...its Open a new window: a second Oracle window, of the same Chromium" "$([ "$(grep -c '^client .* app=browser' $D/state)" = 2 ] && [ "$(chromes)" = 1 ] && [ "$(targets | grep -c '^app isolated-app://')" = 2 ] && echo 1)" "$(grep '^client' $D/state) | chromes $(chromes) | $(targets) | menu: $P $(grep '^cmitem' $D/state | head -8)"
# ---- the windows closed: Chromium goes with the last (one by the page's own ✕, the other by the frame's)
ev http://127.0.0.1:8080/ 'window.__oneAppClose(),1' >/dev/null
w8 '[ "$(grep -c "^client .* app=browser" $D/state)" -le 1 ]' 60; timeout 1 tail -f /dev/null
F2=$(grep "^client .* app=browser" $D/state | head -1); set -- $F2; xdotool mousemove $(($4 + $6 - 10)) $(($5 + 17)) click 1
w8 '[ "$(grep -c "^client .* app=browser" $D/state)" = 0 ]' 120
w8 '[ "$(st oracle | cut -d" " -f1)" = 0 ] && [ "$(chromes)" = 0 ]' 80
ok "6. Oracle's windows closed (the app's own ✕, the frame's ✕): they go, and Chromium with the last" "$([ "$(grep -c '^client .* app=browser' $D/state)" = 0 ] && [ "$(chromes)" = 0 ] && [ "$(st oracle | cut -d' ' -f1)" = 0 ] && echo 1)" "$(grep '^client' $D/state) | $(st oracle) | $(pgrep -af RudolfOracle | grep -v -- --type= | head -3)"
# ---- again: the same profile, the cookie still there
xdotool mousemove $(hit pin browser) click 1
w8 '[ "$(ev http://127.0.0.1:8080/ "typeof rtOn==\"function\"&&rtOn()&&!!OA.main")" = true ]' 120
ev http://127.0.0.1:8080/ 'webOpen("http://127.0.0.1:8333/"),1' >/dev/null
w8 'targets | grep -q "^webview http://127.0.0.1:8333/"' 80; timeout 1 tail -f /dev/null
o=$(ev http://127.0.0.1:8333/ 'document.cookie')
ok "7. Oracle opened again: Chromium started again with the same profile, the site's sign-in (its cookie) kept" "$([ "$o" = '"oracle59=kept"' ] && [ "$(chromes)" = 1 ] && echo 1)" "$o $(targets)"
ev http://127.0.0.1:8080/ 'window.__oneAppClose(),1' >/dev/null
w8 '[ "$(chromes)" = 0 ] && [ "$(st oracle | cut -d" " -f1)" = 0 ]' 80
# ---- without Chromium: Oracle's WebKit window, as before, said once
printf 'nothing\n' > $T/oracle-chrome
xdotool mousemove $(hit pin browser) click 1
w8 'grep -q "^app browser " $D/state' 120
ok "8. without Rudolf's Chromium: Oracle's WebKit window, as before (the page told: nochrome=1)" "$(grep -q '^app browser ' $D/state && [ "$(chromes)" = 0 ] && grep -q 'Oracle: .* names no Chromium or shell' $T/native.log && echo 1)" "$(grep '^app ' $D/state) $(tail -3 $T/native.log)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
