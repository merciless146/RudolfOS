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
# shell's pop-ups, their click its own action; a website's full screen covers the screen. (Step 5, second part, 4e to 4m and 6b, 6c)
# the page's own full screen; a file opened from Oracle in another app's window and a web page's file from an app in Oracle's
# (Chromium); the files one between Oracle's page (Chromium's storage) and the app windows (WebKit's), through rudolfd's mirror;
# the clipboard both ways, and after Chromium has gone; the keys (Ctrl+Shift+N and Ctrl+N windows of their own, Chromium's own
# keys doing nothing); a website's file dialog over Oracle's window, centred, not cut; the devlink's screenshot (rudolf-display
# --shot); the call's 🎙️ and the volume keys' indicator the native desktop's; a download taken into Downloads and told; a
# download in progress when the last window closes (Chromium waits, its page takes it in). The WebKit windows' pages are read
# through rudolf-native's test hook (RUDOLF_NATIVE_TEST_EVAL). ON59_HOLD=SECONDS keeps the run as it is after step 4c.
# Runs in a network namespace of its own (the shell's desktop is http://127.0.0.1:8080). Needs root, Xvfb, xdotool,
# ImageMagick, xclip, node, python3, gcc, ip and WebKitGTK's headers.   sh oracle_native_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root (a network namespace)"; exit 0; }
[ -z "$ON59_INNER" ] && exec env ON59_INNER=1 unshare -n sh "$0" "$@"
command -v Xvfb >/dev/null && command -v xdotool >/dev/null && command -v convert >/dev/null && command -v ip >/dev/null && command -v xclip >/dev/null || { echo "SKIP: needs Xvfb, xdotool, ImageMagick, xclip and ip"; exit 0; }
CHROME=${CHROME:-$(ls -d /opt/pw-browsers/chromium-*/chrome-linux*/chrome 2>/dev/null | tail -1)}
[ -x "$CHROME" ] || { echo "SKIP: no Chromium (CHROME)"; exit 0; }
HERE=$(cd "$(dirname "$0")" && pwd); REPO=$(cd "$HERE/../../.." && pwd); T=$(mktemp -d /tmp/oranat.XXXXXX); DSP=${ON59_DSP:-:86}
ip link set lo up
trap 'kill $NP $RDP $XV $SP 2>/dev/null; pkill -f "$T/" 2>/dev/null; timeout 1 tail -f /dev/null; pkill -9 -f "$T/" 2>/dev/null; rm -rf $T' EXIT
gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $T/rudolf-native $REPO/iso/sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
gcc -O2 -Wall -o $T/rudolf-launch $REPO/iso/sys/rudolf-launch.c 2>>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
L=/usr/lib/x86_64-linux-gnu; gcc -O2 -o $T/rudolf-display $REPO/iso/sys/rudolf-display.c -I$REPO/iso/sys -lX11 $L/libXtst.so.6 $L/libXfixes.so.3 $L/libXrandr.so.2 -lm 2>$T/cc2 || { cat $T/cc2; echo "FAIL build"; exit 1; }
grep -q warning $T/cc && { cat $T/cc; echo "FAIL build warnings"; exit 1; }
# the shell, signed with a test key, installed from its file as the session does in developer mode
python3 $REPO/iso/systools/mkiwa.py key $T/test.key >/dev/null; ID=$(python3 $REPO/iso/systools/mkiwa.py id $T/test.key)
python3 $REPO/iso/systools/mkiwa.py build $REPO/iso/shell $T/shell.swbn $T/test.key >/dev/null || { echo "FAIL the shell"; exit 1; }
D=$T/d; mkdir -p $D; node $REPO/iso/qt/host/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
echo $ID > $D/iwa-id
printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","b2":"#000080","tb":"bottom","accent":"#2563eb"},"pins":["notepad","browser"],"notes":0,"wallpaper":false}' > $D/settings.json
# a website: it keeps a cookie
mkdir -p $T/site; printf '%s' '<!doctype html><title>Test site</title><p id=p>site words</p><input type=file id=f><a id=dl href=report59.txt download>a download</a><textarea id=i style="position:fixed;left:0;right:0;bottom:0;height:45%;width:100%"></textarea><script>if(!document.cookie.includes("oracle59="))document.cookie="oracle59=kept; max-age=86400; path=/";window.__mark=Math.random()</script>' > $T/site/index.html
head -c 3000 /dev/urandom | od -An -tx1 | tr -d ' \n' | head -c 4000 > $T/site/report59.txt
# (the site's server: its files, and slow59.txt, a download that waits halfway until the test makes the file go59)
cat > $T/site.py <<'PYEOF'
import http.server, os, sys, time
class H(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        if self.path.startswith('/slow59.txt'):
            self.send_response(200); self.send_header('content-type', 'application/octet-stream'); self.send_header('content-length', str(16 * 8192)); self.end_headers()
            for i in range(16):
                self.wfile.write(b'x' * 8192); self.wfile.flush(); time.sleep(0.3)
                while i == 8 and not os.path.exists('go59'): time.sleep(0.2)   # (half of it, then the rest once the test says so)
            return
        return super().do_GET()
os.chdir(sys.argv[1]); http.server.ThreadingHTTPServer(('127.0.0.1', 8333), H).serve_forever()
PYEOF
python3 $T/site.py $T/site >/dev/null 2>&1 & SP=$!
# what the session writes for rudolf-native (Standard's flags; --no-sandbox as root here)
cmdf() { printf '%s\n' $ID "$CHROME" --no-sandbox --class=RudolfOracle --user-data-dir=$T/prof --no-first-run --no-default-browser-check --password-store=basic \
	--enable-features=SharedArrayBuffer,IsolatedWebApps,IsolatedWebAppDevMode --install-isolated-web-app-from-file=$T/shell.swbn --disable-gpu \
	--autoplay-policy=no-user-gesture-required --use-fake-ui-for-media-stream --use-fake-device-for-media-stream --remote-debugging-port=9335 > $T/oracle-chrome; }
cmdf
mkdir -p $T/prof/Default $D/incoming; printf '{"download":{"default_directory":"%s","prompt_for_download":false,"directory_upgrade":true},"savefile":{"default_directory":"%s"},"safebrowsing":{"enabled":false,"enhanced":false}}' $D/incoming $D/incoming > $T/prof/Default/Preferences
python3 $REPO/iso/qt/host/fixtures/native_rd.py 8080 $D > $T/rd.log 2>&1 & RDP=$!
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=$DSP LIBGL_ALWAYS_SOFTWARE=1
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
w8 'xdotool getmouselocation >/dev/null 2>&1' 40
mkdir -p $T/run; chmod 700 $T/run; mkfifo $T/ev; : > $T/ev.out
(cd $D && RUDOLF_NATIVE_TEST_EVAL=$T/ev HOME=$D XDG_RUNTIME_DIR=$T/run RUDOLF_NATIVE_STATE=$D/state RUDOLF_ORACLE_CMD=$T/oracle-chrome RUDOLF_LAUNCH=$T/rudolf-launch exec $T/rudolf-native --size=1280x800 --draw=soft --base=http://127.0.0.1:8080 > $T/native.log 2>&1) & NP=$!
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
# (a WebKit app window's page, through rudolf-native's test hook: wev APP JS)
wev() { n0=$(grep -c "^$1 " $T/ev.out); printf '%s %s\n' "$1" "$2" > $T/ev; i=0; while [ $i -lt 40 ]; do [ "$(grep -c "^$1 " $T/ev.out)" -gt "$n0" ] && break; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; grep "^$1 " $T/ev.out | tail -1 | cut -d' ' -f2-; }
# (the frame of an app's window: x y w h of its content; a click on its title bar gives it the keyboard)
cgeo() { grep "^client .* app=$1" $D/state | head -1 | awk '{print $4+1, $5+int(($7>0)?0:0), $6, $7}'; }
ctitle() { set -- $(grep "^client .* app=$1" $D/state | head -1); [ -n "$2" ] || return 0
	case "$*" in *focus=1*) ;; *) H=$(grep "^hit win $2 " $D/state | head -1 | awk '{print $4+int($6/2), $5+int($7/2)}'); [ -n "$H" ] && { xdotool mousemove $H click 1; timeout 0.5 tail -f /dev/null; } ;; esac   # (its taskbar button brings it to the front)
	set -- $(grep "^client .* app=$1" $D/state | head -1); [ -n "$2" ] && xdotool mousemove $(($4 + $6 / 3)) $(($5 + 12)) click 1; timeout 0.4 tail -f /dev/null; }
cclose() { NL=$(grep "^client .* app=$1" $D/state | head -1); [ -n "$NL" ] || return 0; ctitle $1; set -- $(grep "^client .* app=$1" $D/state | head -1); xdotool mousemove $(($4 + $6 + 2 - 21)) $(($5 + 18)) click 1; w8 "! grep -q '^client .* app=$1' $D/state" 40; }
clip() { xclip -o -selection clipboard 2>/dev/null; }
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
ev http://127.0.0.1:8080/ 'OA.main.oNewTab("http://127.0.0.1:8333/"),1' >/dev/null
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
ok "...and back in its frame when it leaves" "$(grep -q '^client .* full=0 .* app=browser .*Oracle' $D/state && echo 1)" "$(grep '^client' $D/state)"
[ -n "$ON59_HOLD" ] && { echo "HOLD $T $D $$"; timeout $ON59_HOLD tail -f /dev/null; }   # (debugging: the run kept as it is here)
# ---- (step 5, second part) the desktop page's own full screen (F11, a game in the page): Oracle's window covers the screen
o=$(ev http://127.0.0.1:8080/ 'document.documentElement.requestFullscreen().then(()=>1,e=>String(e))')
w8 'grep -q "^client .* full=1 .* app=browser" $D/state' 60
ok "4e. the desktop page's own full screen (F11, a game in Oracle's page): Oracle's window covers the screen, as on Standard" "$(grep -q '^client .* 1280 800 min=0 max=0 full=1 .* app=browser' $D/state && [ "$(ev http://127.0.0.1:8080/ '[innerWidth,innerHeight].join()')" = '"1280,800"' ] && echo 1)" "$o $(grep '^client' $D/state) $(ev http://127.0.0.1:8080/ '[innerWidth,innerHeight].join()')"
ev http://127.0.0.1:8080/ 'document.exitFullscreen().then(()=>1,e=>String(e))' >/dev/null
w8 'grep -q "^client .* full=0 .* app=browser" $D/state' 60
ok "...and back in its frame" "$(grep -q '^client .* full=0 .* app=browser .*Oracle' $D/state && [ "$(ev http://127.0.0.1:8080/ 'innerWidth')" = "$CW" ] && echo 1)" "$(grep '^client' $D/state)"
# ---- Open with: a file of Oracle's opened in another app: that app's window of its own, with the file (Oracle's files are the others')
ev http://127.0.0.1:8080/ '(()=>{const d=node(homePath("DOCUMENTS"));d["ORACLE59.TXT"]="written in Oracle";fsSave();openPath(homePath("DOCUMENTS").concat("ORACLE59.TXT"));return 1})()' >/dev/null
w8 'grep -q "^app notepad " $D/state' 120
w8 '[ "$(wev notepad "document.querySelector(\".win.oamain textarea\")?.value||\"\"")" = "written in Oracle" ]' 60
o=$(wev notepad 'document.querySelector(".win.oamain textarea")?.value||""')
ok "4f. a file opened from Oracle (its downloads, Open): Notepad's window of its own, the file in it (written in Oracle's page, read in Notepad's)" "$([ "$o" = 'written in Oracle' ] && [ "$(ev http://127.0.0.1:8080/ 'wins.filter(W=>W.app=="notepad").length')" = 0 ] && echo 1)" "$o | $(grep '^app ' $D/state)"
# (and the other way: a web page's file opened from an app's window is Oracle's, in Rudolf's Chromium, not a WebKit window)
wev notepad '(()=>{const d=node(homePath("DOCUMENTS"));d["PAGE59.HTML"]="<title>Page 59</title><p>a file";fsSave();openPath(homePath("DOCUMENTS").concat("PAGE59.HTML"));return 1})()' >/dev/null
w8 '[ "$(grep -c "^client .* app=browser" $D/state)" = 2 ]' 120
w8 'ev http://127.0.0.1:8080/ "document.title.includes(\"Page 59\")" | grep -q true' 60
ok "...a web page's file opened from Notepad's window: an Oracle window of its own, in the same Chromium, not a WebKit window" "$([ "$(grep -c '^client .* app=browser' $D/state)" = 2 ] && [ "$(chromes)" = 1 ] && ! grep -q '^app browser ' $D/state && ev http://127.0.0.1:8080/ 'document.title.includes("Page 59")' | grep -q true && echo 1)" "$(grep -E '^(client|app) ' $D/state) | $(ev http://127.0.0.1:8080/ 'OA.main&&OA.main.el.querySelector(".tb .t").textContent')"
ev http://127.0.0.1:8080/ 'document.title.includes("Page 59")?(window.__oneAppClose(),1):0' >/dev/null
w8 '[ "$(grep -c "^client .* app=browser" $D/state)" = 1 ]' 60
# ---- the files are one: Oracle's page (Chromium's storage) and the app windows (WebKit's) see each other's changes at once
ev http://127.0.0.1:8080/ '(()=>{node(homePath("DOCUMENTS"))["LIVE59.TXT"]="from Oracle";fsSave();return 1})()' >/dev/null
w8 '[ "$(wev notepad "String((node(homePath(\"DOCUMENTS\"))||{})[\"LIVE59.TXT\"])")" = "from Oracle" ]' 60
ok "4g. a file Oracle's page saved: in the open Notepad window's files at once" "$([ "$(wev notepad 'String((node(homePath("DOCUMENTS"))||{})["LIVE59.TXT"])')" = 'from Oracle' ] && echo 1)" "$(wev notepad 'Object.keys(node(homePath("DOCUMENTS"))||{}).join()')"
wev notepad '(()=>{node(homePath("DOCUMENTS"))["BACK59.TXT"]="from Notepad";fsSave();return 1})()' >/dev/null
w8 '[ "$(ev http://127.0.0.1:8080/ "String((node(homePath(\"DOCUMENTS\"))||{})[\"BACK59.TXT\"])")" = "\"from Notepad\"" ]' 60
ok "...and one an app window saved: in Oracle's page at once (neither overwrites the other)" "$([ "$(ev http://127.0.0.1:8080/ 'String((node(homePath("DOCUMENTS"))||{})["BACK59.TXT"])')" = '"from Notepad"' ] && [ "$(wev notepad 'String((node(homePath("DOCUMENTS"))||{})["LIVE59.TXT"])')" = 'from Oracle' ] && echo 1)" "$(ev http://127.0.0.1:8080/ 'Object.keys(node(homePath("DOCUMENTS"))||{}).join()')"
# ---- the clipboard: copied in Oracle (a website), pasted in Notepad's window; and back
ev http://127.0.0.1:8333/ '(()=>{const r=document.createRange();r.selectNodeContents(document.getElementById("p"));getSelection().removeAllRanges();getSelection().addRange(r);return document.execCommand("copy")})()' >/dev/null
w8 '[ "$(clip)" = "site words" ]' 20
c1=$(clip); NV=$(wev notepad 'document.querySelector(".win.oamain textarea").value'); wev notepad '(()=>{const t=document.querySelector(".win.oamain textarea");t.value="";t.focus();return 1})()' >/dev/null
ctitle notepad; set -- $(cgeo notepad); xdotool mousemove $(($1 + $3 / 2)) $(($2 + $4 / 2)) click 1; timeout 0.3 tail -f /dev/null; xdotool key ctrl+v
w8 '[ "$(wev notepad "document.querySelector(\".win.oamain textarea\").value")" = "site words" ]' 20
ok "4h. the clipboard: text copied in a website in Oracle, pasted in Notepad's window" "$([ "$c1" = 'site words' ] && [ "$(wev notepad 'document.querySelector(".win.oamain textarea").value')" = 'site words' ] && echo 1)" "clipboard '$c1', Notepad '$(wev notepad 'document.querySelector(".win.oamain textarea").value')'"
wev notepad '(()=>{const t=document.querySelector(".win.oamain textarea");t.value="from notepad 59";t.focus();t.select();return document.execCommand("copy")})()' >/dev/null
w8 '[ "$(clip)" = "from notepad 59" ]' 20; c2=$(clip)
ctitle browser; set -- $(cgeo browser); xdotool mousemove $(($1 + $3 / 2)) $(($2 + $4 - 40)) click 1; timeout 0.3 tail -f /dev/null; xdotool key ctrl+v
w8 '[ "$(ev http://127.0.0.1:8333/ "document.getElementById(\"i\").value")" = "\"from notepad 59\"" ]' 20
ok "...and back: copied in Notepad, pasted in the website in Oracle" "$([ "$c2" = 'from notepad 59' ] && [ "$(ev http://127.0.0.1:8333/ 'document.getElementById("i").value')" = '"from notepad 59"' ] && echo 1)" "clipboard '$c2', the site '$(ev http://127.0.0.1:8333/ 'document.getElementById("i").value')'"
wev notepad "(()=>{document.querySelector('.win.oamain textarea').value=$(node -e 'console.log(JSON.stringify(process.argv[1]))' "$NV");return 1})()" >/dev/null; cclose notepad
# ---- the keys (keys56.js's rules): in a website in Oracle, Chromium's own keys do not act; Oracle's do
ev http://127.0.0.1:8080/ 'window.__k59=1' >/dev/null; m0=$(ev http://127.0.0.1:8333/ '__mark')
ctitle browser; set -- $(cgeo browser); xdotool mousemove $(($1 + $3 / 2)) $(($2 + $4 - 40)) click 1; timeout 0.3 tail -f /dev/null
xdotool key ctrl+shift+n
w8 '[ "$(grep -c "^client .* app=browser" $D/state)" = 2 ]' 120
w8 '[ "$(ev http://127.0.0.1:8080/ "!!(OA.main&&OA.main.priv)")" = "[false,true]" ] || [ "$(ev http://127.0.0.1:8080/ "!!(OA.main&&OA.main.priv)")" = "[true,false]" ]' 80
o=$(ev http://127.0.0.1:8080/ '!!(OA.main&&OA.main.priv)')
ok "4i. Ctrl+Shift+N in Oracle: a private Oracle window of its own (as on Standard), of the same Chromium" "$([ "$(grep -c '^client .* app=browser' $D/state)" = 2 ] && [ "$(chromes)" = 1 ] && echo "$o" | grep -q true && echo 1)" "$o | $(grep '^client' $D/state)"
ev http://127.0.0.1:8080/ 'OA.main&&OA.main.priv?(window.__oneAppClose(),1):0' >/dev/null
w8 '[ "$(grep -c "^client .* app=browser" $D/state)" = 1 ]' 60
ctitle browser; set -- $(cgeo browser); xdotool mousemove $(($1 + $3 / 2)) $(($2 + $4 - 40)) click 1; timeout 0.3 tail -f /dev/null
for k in F5 ctrl+r ctrl+h ctrl+j ctrl+shift+o ctrl+shift+Delete ctrl+s ctrl+o ctrl+p ctrl+d F1 F7 shift+Escape; do xdotool key $k; timeout 0.4 tail -f /dev/null; xdotool key Escape; done
timeout 1.5 tail -f /dev/null
pg=$(targets | grep -c '^page '); o=$(ev http://127.0.0.1:8080/ 'window.__k59')
ok "...Chromium's own keys (F5, Ctrl+R, Ctrl+H, Ctrl+J, Ctrl+Shift+O, Ctrl+Shift+Delete, Ctrl+S, Ctrl+O, Ctrl+P, Ctrl+D, F1, F7, Shift+Esc) open nothing of Chromium's and do not reload Oracle's page" "$([ "$pg" = 0 ] && [ "$o" = 1 ] && [ "$(grep -c '^client .* app=browser' $D/state)" = 1 ] && [ "$(grep -c '^client ' $D/state)" = 1 ] && echo 1)" "pages $pg, mark $o | $(targets) | $(grep '^client' $D/state)"
ok "...F5 and Ctrl+R reload the website's tab (Oracle's own keys)" "$([ "$(ev http://127.0.0.1:8333/ '__mark')" != "$m0" ] && echo 1)" "$m0 $(ev http://127.0.0.1:8333/ '__mark')"
xdotool key ctrl+n
w8 '[ "$(grep -c "^client .* app=browser" $D/state)" = 2 ]' 120
ok "...Ctrl+N: a new Oracle window of its own" "$([ "$(grep -c '^client .* app=browser' $D/state)" = 2 ] && [ "$(chromes)" = 1 ] && echo 1)" "$(grep '^client' $D/state)"
w8 '[ "$(ev http://127.0.0.1:8080/ "typeof OA==\"object\"&&!!OA.main")" = "[true,true]" ]' 80
# (the newest window closed again: the one without the website)
ev http://127.0.0.1:8080/ '!document.querySelector(".otab")||[...document.querySelectorAll(".otab")].every(t=>!/Test site/.test(t.textContent))?(window.__oneAppClose(),1):0' >/dev/null
w8 '[ "$(grep -c "^client .* app=browser" $D/state)" = 1 ]' 60
# ---- the file dialog: a website's file chooser is Chromium's own dialog (Rudolf's policy, AllowFileSelectionDialogs, turns it off on
#      both engines; here, without the policy): a dialog over Oracle's window, centred on it, framed as a dialog, not cut as Oracle's window
ctitle browser; ev http://127.0.0.1:8333/ 'document.getElementById("f").click(),1' >/dev/null
w8 'grep -q "^client .* app=- " $D/state' 60
DL=$(grep "^client .* app=- " $D/state | head -1); OL=$(grep "^client .* app=browser" $D/state | head -1)
if [ -n "$DL" ]; then set -- $DL; dx=$(($4 + $6 / 2)); dy=$(($5 + $7 / 2)); set -- $OL; ox=$(($4 + $6 / 2)); oy=$(($5 + $7 / 2)); fi
set -- $DL; dlx=$4; dly=$5
ok "4j. a website's file dialog in Oracle: Chromium's dialog, a window over Oracle's, centred on it (kept on the screen)" "$([ -n "$DL" ] && { [ $((dx - ox)) -le 30 ] && [ $((ox - dx)) -le 30 ] || [ "$dlx" = 0 ]; } && { [ $((dy - oy)) -le 40 ] && [ $((oy - dy)) -le 40 ] || [ "$dly" -le 0 ]; } && echo 1)" "dialog $DL | Oracle $OL"
dw=$(echo "$DL" | cut -d' ' -f2); dg=$(xdotool getwindowgeometry $((0x$dw)) 2>/dev/null | tr '\n' ' ')
ok "...not cut as Oracle's window is (its own title bar and edges shown), not a second Oracle on the taskbar" "$([ -n "$DL" ] && ! echo "$DL" | grep -q 'app=browser' && [ "$(grep -c '^hit win ' $D/state)" = 1 ] && echo "$dg" | grep -q "Geometry: $(echo "$DL" | cut -d' ' -f6)x$(echo "$DL" | cut -d' ' -f7)" && echo 1)" "$DL | $dg | $(grep '^hit win' $D/state)"
xdotool key Escape; w8 '! grep -q "^client .* app=- " $D/state' 40 || { set -- $(grep "^client .* app=- " $D/state | head -1); [ -n "$2" ] && xdotool windowclose $((0x$2)) 2>/dev/null; }
# ---- the devlink's screenshot: rudolfd's screenshot (rudolf-display --shot, the X screen) shows Oracle's window as drawn
set -- $(cgeo browser); ev http://127.0.0.1:8080/ '(()=>{const d=document.createElement("div");d.id="shot59";d.style.cssText="position:fixed;left:0;top:0;width:100%;height:100%;background:#ff00ff;z-index:2147483647";document.body.appendChild(d);return 1})()' >/dev/null
timeout 1 tail -f /dev/null
DISPLAY=$DSP $T/rudolf-display --shot > $T/shot.ppm 2>/dev/null
p=$(convert $T/shot.ppm -format "%[fx:int(255*p{$(($1 + $3 / 2)),$(($2 + 40 + $4 / 2))}.r)] %[fx:int(255*p{$(($1 + $3 / 2)),$(($2 + 40 + $4 / 2))}.g)] %[fx:int(255*p{$(($1 + $3 / 2)),$(($2 + 40 + $4 / 2))}.b)]" info: 2>&1)
ok "4k. the devlink's screenshot (rudolfd's, through rudolf-display --shot): Oracle's window as it is drawn" "$([ "$p" = '255 0 255' ] && echo 1)" "$p at $(($1 + $3 / 2)),$(($2 + 40 + $4 / 2))"
ev http://127.0.0.1:8080/ 'document.getElementById("shot59").remove(),1' >/dev/null
# ---- the volume and the call's 🎙️: a call in a website in Oracle is the native taskbar's 🎙️ (rudolfd's mic: the sound server's apps
#      recording), its click mutes the microphone for everything; Oracle's page draws neither in its window
ev http://127.0.0.1:8333/ 'navigator.mediaDevices.getUserMedia({audio:true}).then(s=>{window.__s=s;return s.getAudioTracks().length},e=>String(e))' > $T/gum
printf '{"users":["Oracle"],"muted":false}' > $D/mic.json
w8 '[ "$(st mic)" = "1 0" ]' 40
ok "4l. a call in a website in Oracle (the microphone used): the native taskbar's 🎙️" "$([ "$(st mic)" = '1 0' ] && grep -q '^hit call ' $D/state && echo 1)" "$(cat $T/gum) | mic $(st mic) | $(grep '^hit call' $D/state)"
H=$(grep '^hit call ' $D/state | head -1 | awk '{print $4+int($6/2), $5+int($7/2)}'); [ -n "$H" ] && xdotool mousemove $H click 1
w8 '[ "$(st mic)" = "1 1" ]' 40
ok "...its click mutes the microphone for everything (rudolfd's mic?muted=1)" "$([ "$(st mic)" = '1 1' ] && grep -q 'POST mic?muted=1' $D/calls && echo 1)" "mic $(st mic) | $(grep 'mic' $D/calls | tail -3)"
ok "...Oracle's page draws no 🎙️ or taskbar of its own in its window" "$([ "$(ev http://127.0.0.1:8080/ '[...document.querySelectorAll("#bar,.call59")].filter(e=>e.offsetWidth).length' one)" = 0 ] && echo 1)" "$(ev http://127.0.0.1:8080/ '[...document.querySelectorAll("#bar,.call59")].filter(e=>e.offsetWidth).length')"
ev http://127.0.0.1:8333/ 'window.__s&&__s.getTracks().forEach(t=>t.stop()),1' >/dev/null; printf '{"users":[],"muted":false}' > $D/mic.json
ev http://127.0.0.1:8080/ 'osdEvents("osd volume 35"),document.getElementById("osd")&&document.getElementById("osd").style.display' > $T/osd
w8 'grep -q "^osd " $D/state' 20
ok "...the volume keys' indicator (rudolfd's osd event, read by Oracle's page): the native desktop's, at the top of the screen, not inside Oracle's window" "$(grep -q '^osd volume 35' $D/state && ! grep -q '"flex"' $T/osd && echo 1)" "$(cat $T/osd) | $(grep '^osd' $D/state)"
# ---- downloads: a website's download in Oracle goes where Standard's go (Chromium's folder, the policy's), is in Oracle's
#      Downloads list, taken into the account's Downloads and told in a notification (Standard shows no progress on the taskbar)
ev http://127.0.0.1:8333/ 'document.getElementById("dl").click(),1' >/dev/null
w8 '[ "$(ev http://127.0.0.1:8080/ "(DLS.find(r=>/REPORT59/.test(r.n))||{}).st||\"\"")" = "\"saved\"" ]' 120
o=$(ev http://127.0.0.1:8080/ 'JSON.stringify(DLS.filter(r=>/REPORT59/.test(r.n)).map(r=>[r.n,r.st,r.why]))')
ok "4m. a download in Oracle: in Oracle's Downloads list, saved into the account's Downloads (dl/incoming, the file taken in)" "$(echo "$o" | grep -q 'REPORT59.TXT\\",\\"saved' && [ "$(ev http://127.0.0.1:8080/ 'String(!!(node(downloadsPath())||{})["REPORT59.TXT"])')" = '"true"' ] && [ -z "$(ls $D/incoming)" ] && echo 1)" "$o | $(ls $D/incoming) | $(grep 'dl/' $D/calls | tail -4)"
w8 'grep -q "^toast .*|Downloaded" $D/state' 30
ok "...its notification is the native shell's pop-up (as Standard's)" "$(grep -q '^toast .*|Downloaded' $D/state && echo 1)" "$(grep '^toast' $D/state)"
app_n=$(grep -c '^app ' $D/state); ev http://127.0.0.1:8080/ 'launch("explorer"),1' >/dev/null; w8 'grep -q "^app explorer " $D/state' 120
w8 '[ "$(wev explorer "String(!!(node(downloadsPath())||{})[\"REPORT59.TXT\"])")" = "true" ]' 40
ok "...and it is in Downloads for the other apps (File Explorer's window)" "$([ "$(wev explorer 'String(!!(node(downloadsPath())||{})["REPORT59.TXT"])')" = true ] && echo 1)" "$(wev explorer 'Object.keys(node(downloadsPath())||{}).join()')"
cclose explorer
# ---- a second window: the pin's Open a new window, in the same Chromium (the pin itself brings Oracle to the front)
xdotool mousemove $(hit pin browser) click 1; timeout 2 tail -f /dev/null; n1=$(grep -c "^client .* app=browser" $D/state)
xdotool mousemove $(hit pin browser) click 3; w8 'grep -q "^cmitem .*pnew:browser" $D/state' 40
P=$(grep "^cmitem .*pnew:browser" $D/state | head -1 | awk '{print $2, $3}'); cl=$(grep "^cmitem .*pclose:browser" $D/state | awk '{print $4}'); xdotool mousemove $P click 1
w8 '[ "$(grep -c "^client .* app=browser" $D/state)" = 2 ]' 120
ok "5. the pin while Oracle is open: Oracle to the front (no second window); its menu's Close all windows not greyed" "$([ "$n1" = 1 ] && [ "$cl" = 0 ] && echo 1)" "$n1 $cl"
ok "...its Open a new window: a second Oracle window, of the same Chromium" "$([ "$(grep -c '^client .* app=browser' $D/state)" = 2 ] && [ "$(chromes)" = 1 ] && [ "$(targets | grep -c '^app isolated-app://')" = 2 ] && echo 1)" "$(grep '^client' $D/state) | chromes $(chromes) | $(targets) | menu: $P $(grep '^cmitem' $D/state | head -8)"
ev http://127.0.0.1:8333/ 'location.href="/slow59.txt",1' >/dev/null; w8 'ls $D/incoming | grep -q crdownload' 40
ev http://127.0.0.1:8333/ '(()=>{const t=document.getElementById("i");t.value="last words";t.select();return document.execCommand("copy")})()' >/dev/null; w8 '[ "$(clip)" = "last words" ]' 20
# ---- the windows closed: Chromium goes with the last (one by the page's own ✕, the other by the frame's)
ev http://127.0.0.1:8080/ 'window.__oneAppClose(),1' one >/dev/null
w8 '[ "$(grep -c "^client .* app=browser" $D/state)" -le 1 ]' 60; timeout 1 tail -f /dev/null
timeout 1 tail -f /dev/null
for try in 1 2 3; do F2=$(grep "^client .* app=browser" $D/state | head -1); [ -n "$F2" ] || break; set -- $F2; xdotool mousemove $(($4 + $6 - 10)) $(($5 + 17)); timeout 0.3 tail -f /dev/null; xdotool click 1
	w8 '! grep -q "^client .* app=browser" $D/state' 40 && break; done
w8 '[ "$(grep -c "^client .* app=browser" $D/state)" = 0 ]' 120
ok "6. Oracle's windows closed (the app's own ✕, the frame's ✕): they go (Chromium with the last, once its download has finished: 6c)" "$([ "$(grep -c '^client .* app=browser' $D/state)" = 0 ] && echo 1)" "$(grep '^client' $D/state) | $(st oracle) | $(pgrep -af RudolfOracle | grep -v -- --type= | head -3)"
ok "6c. a download in progress when Oracle's last window closed: Chromium waits for it (as on Standard, where it never closes)" "$(grep -q 'Chromium waits for them' $T/native.log && [ "$(chromes)" = 1 ] && echo 1)" "chromes $(chromes) $(ls $D/incoming) $(grep 'rudolf-launch' $T/native.log | tail -3)"
: > $T/site/go59; w8 '[ "$(chromes)" = 0 ]' 80
ok "...its page, off the screen, takes the finished download in (as the desktop does on Standard), then Chromium closes" "$([ "$(chromes)" = 0 ] && grep -q 'the downloads have finished' $T/native.log && [ -z "$(ls $D/incoming)" ] && [ "$(grep -c '^client ' $D/state)" = 0 ] && echo 1)" "chromes $(chromes) | $(ls $D/incoming) | $(grep '^client' $D/state) | $(grep 'rudolf-launch' $T/native.log | tail -3)" "$(ls -l $D/incoming) $(grep 'rudolf-launch' $T/native.log | tail -3)"
# (copied in Oracle, then Oracle closed: it is still on the clipboard, as on Standard, where Chromium never goes)
ok "6b. text copied in Oracle stays on the clipboard after Oracle (and Chromium) closed" "$([ "$(clip)" = 'last words' ] && echo 1)" "'$(clip)'"
# ---- again: the same profile, the cookie still there
xdotool mousemove $(hit pin browser) click 1
w8 '[ "$(ev http://127.0.0.1:8080/ "typeof rtOn==\"function\"&&rtOn()&&!!OA.main")" = true ]' 120
ev http://127.0.0.1:8080/ 'OA.main.oNewTab("http://127.0.0.1:8333/"),1' >/dev/null
w8 'targets | grep -q "^webview http://127.0.0.1:8333/"' 80; timeout 1 tail -f /dev/null
o=$(ev http://127.0.0.1:8333/ 'document.cookie')
ok "7. Oracle opened again: Chromium started again with the same profile, the site's sign-in (its cookie) kept" "$([ "$o" = '"oracle59=kept"' ] && [ "$(chromes)" = 1 ] && echo 1)" "$o $(targets)"
w8 '[ "$(ev http://127.0.0.1:8080/ "(DLS.find(r=>/SLOW59/.test(r.n))||{}).st||\"\"" one)" = "\"saved\"" ]' 60
ok "...and the download that finished meanwhile is taken into Downloads, whole" "$([ "$(ev http://127.0.0.1:8080/ 'String((node(downloadsPath())||{})["SLOW59.TXT"]||"").length' one)" = 131072 ] && [ ! -e $D/incoming/slow59.txt ] && echo 1)" "$(ev http://127.0.0.1:8080/ 'JSON.stringify(DLS.map(r=>[r.n,r.st]))' one) $(ls $D/incoming)"
ev http://127.0.0.1:8080/ 'window.__oneAppClose(),1' >/dev/null
w8 '[ "$(chromes)" = 0 ] && [ "$(st oracle | cut -d" " -f1)" = 0 ]' 80
# ---- without Chromium: Oracle's WebKit window, as before, said once
printf 'nothing\n' > $T/oracle-chrome
xdotool mousemove $(hit pin browser) click 1
w8 'grep -q "^app browser " $D/state' 120
ok "8. without Rudolf's Chromium: Oracle's WebKit window, as before (the page told: nochrome=1)" "$(grep -q '^app browser ' $D/state && [ "$(chromes)" = 0 ] && grep -q 'Oracle: .* names no Chromium or shell' $T/native.log && echo 1)" "$(grep '^app ' $D/state) $(tail -3 $T/native.log)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
