#!/bin/sh
# 1.6.2 (docs/plan-1.6.2.md Part 38, step 5): Oracle's Chromium on the native desktop, its memory with one, three and six websites open.
# Set up as oracle_native_t.sh: rudolf-native under Xvfb with the stand-in rudolfd (qt/host/fixtures/native_rd.py) serving the real page,
# the shell (iso/shell) built with a test key, Rudolf OS's Chromium policy, Oracle from its pin (rudolf-launch --oracle). Each website is
# a site of its own (127.0.0.K, so site isolation gives each its own process, as two real websites), a page of about the size of a
# simple real one (some script, a few thousand elements). After each step (1, 3, 6 tabs; 20 s, then 10 s more for the processes to
# settle) the processes of Oracle's Chromium (its profile's: the browser, the GPU, the network and the utilities, the renderers) are
# read: their count, the renderers' count, and the sums of their RssAnon (their own memory, as Task Manager on the Vivobook gives it)
# and PSS. One line of JSON per step:
#   sh oramem162.sh [EXTRA-FLAGS...]      the flags added to Oracle's Chromium (after Standard's), e.g. --renderer-process-limit=4
# Runs in network and mount namespaces of its own. Needs root, Xvfb, xdotool, node, python3, gcc, ip and WebKitGTK's headers; a Chromium
# with Isolated Web Apps (CHROME; Playwright's by default). Display ORAMEM_DSP (:93).
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root (a network namespace)"; exit 0; }
[ -z "$ORAMEM_INNER" ] && exec env ORAMEM_INNER=1 unshare -n -m sh "$0" "$@"
command -v Xvfb >/dev/null && command -v xdotool >/dev/null && command -v ip >/dev/null || { echo "SKIP: needs Xvfb, xdotool and ip"; exit 0; }
CHROME=${CHROME:-$(ls -d /opt/pw-browsers/chromium-*/chrome-linux*/chrome 2>/dev/null | tail -1)}
[ -x "$CHROME" ] || { echo "SKIP: no Chromium (CHROME)"; exit 0; }
HERE=$(cd "$(dirname "$0")" && pwd); REPO=$(cd "$HERE/../../.." && pwd); T=$(mktemp -d /tmp/oramem.XXXXXX); DSP=${ORAMEM_DSP:-:93}
ip link set lo up
mount --make-rprivate / 2>/dev/null; mount -t tmpfs none /run && mkdir -p /run/rudolf/dl/.incoming || { echo "FAIL /run"; exit 1; }
for p in /etc/chromium/policies/managed /etc/opt/chrome_for_testing/policies/managed; do [ -d $p ] && mount -t tmpfs none $p && cp $REPO/iso/sys/root/etc/chromium/policies/managed/rudolf.json $p/; done
trap 'kill $NP $RDP $XV $SP 2>/dev/null; pkill -f "$T/" 2>/dev/null; timeout 1 tail -f /dev/null; pkill -9 -f "$T/" 2>/dev/null; rm -rf $T' EXIT
gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $T/rudolf-native $REPO/iso/sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
gcc -O2 -Wall -o $T/rudolf-launch $REPO/iso/sys/rudolf-launch.c 2>>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
python3 $REPO/iso/systools/mkiwa.py key $T/test.key >/dev/null; ID=$(python3 $REPO/iso/systools/mkiwa.py id $T/test.key)
python3 $REPO/iso/systools/mkiwa.py build $REPO/iso/shell $T/shell.swbn $T/test.key >/dev/null || { echo "FAIL the shell"; exit 1; }
D=$T/d; mkdir -p $D; node $REPO/iso/qt/host/fixtures/deskls.js $D/ls.json $D/keys.json >/dev/null || { echo "FAIL node"; exit 1; }
echo $ID > $D/iwa-id; echo /run/rudolf/dl/.incoming > $D/dl-dir; mkdir -p $D/idbm
printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","b2":"#000080","tb":"bottom","accent":"#2563eb"},"pins":["notepad","browser"],"notes":0,"wallpaper":false}' > $D/settings.json
# the websites: one page, a site for each address (some script and a few thousand elements, as a simple real page)
mkdir -p $T/site; cat > $T/site/index.html <<'HTML'
<!doctype html><title>Site</title><style>p{margin:2px;font:13px sans-serif}.c{color:#357}</style><body><h1 id=h>Site</h1><div id=b></div>
<script>document.getElementById('h').textContent=location.host;const b=document.getElementById('b'),f=document.createDocumentFragment();
for(let i=0;i<3000;i++){const p=document.createElement('p');p.className=i%3?'':'c';p.textContent='Paragraph '+i+' of a page about '+location.host+'.';f.appendChild(p)}b.appendChild(f);
window.__keep=Array.from({length:20000},(x,i)=>({i,s:'item '+i,a:[i,i*2,i*3]}));const c=document.createElement('canvas');c.width=600;c.height=300;const g=c.getContext('2d');g.fillStyle='#8ac';g.fillRect(0,0,600,300);b.prepend(c);</script>
HTML
printf '%s\n' 'import http.server,os,sys' 'class H(http.server.SimpleHTTPRequestHandler):' '    def log_message(self,*a): pass' "os.chdir(sys.argv[1]); http.server.ThreadingHTTPServer(('0.0.0.0',8333),H).serve_forever()" > $T/site.py
python3 $T/site.py $T/site >/dev/null 2>&1 & SP=$!
printf '%s\n' $ID "$CHROME" --no-sandbox --class=RudolfOracle --user-data-dir=$T/prof --no-first-run --no-default-browser-check --password-store=basic \
	--enable-features=SharedArrayBuffer,IsolatedWebApps,IsolatedWebAppDevMode --install-isolated-web-app-from-file=$T/shell.swbn --disable-gpu \
	--autoplay-policy=no-user-gesture-required --use-fake-ui-for-media-stream --disk-cache-size=67108864 --remote-debugging-port=9335 "$@" > $T/oracle-chrome
python3 $REPO/iso/qt/host/fixtures/native_rd.py 8080 $D > $T/rd.log 2>&1 & RDP=$!
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=$DSP LIBGL_ALWAYS_SOFTWARE=1
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
w8 'xdotool getmouselocation >/dev/null 2>&1' 40
mkdir -p $T/run; chmod 700 $T/run; mkfifo $T/ev; : > $T/ev.out
(cd $D && RUDOLF_NATIVE_TEST_EVAL=$T/ev HOME=$D XDG_RUNTIME_DIR=$T/run RUDOLF_NATIVE_STATE=$D/state RUDOLF_ORACLE_CMD=$T/oracle-chrome RUDOLF_LAUNCH=$T/rudolf-launch exec $T/rudolf-native --size=1280x800 --draw=soft --base=http://127.0.0.1:8080 > $T/native.log 2>&1) & NP=$!
st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
hit() { grep "^hit $1 $2 " $D/state | head -1 | awk '{print $4+int($6/2), $5+int($7/2)}'; }
ev() { node -e '
const [pre, expr] = process.argv.slice(1);
(async () => { const l = await (await fetch("http://127.0.0.1:9335/json/list")).json(); const t = l.find(x => x.url.startsWith(pre)); if (!t) { console.log("NO TARGET"); return }
  console.log(JSON.stringify(await new Promise(res => { const ws = new WebSocket(t.webSocketDebuggerUrl); const to = setTimeout(() => res("TIMEOUT"), 8000);
    ws.onopen = () => ws.send(JSON.stringify({id: 1, method: "Runtime.evaluate", params: {expression: expr, returnByValue: true, awaitPromise: true, userGesture: true}}));
    ws.onmessage = e => { const m = JSON.parse(e.data); if (m.id == 1) { clearTimeout(to); ws.close(); res(m.result.result ? m.result.result.value : m.result) } } }))) })().catch(e => console.log("ERR " + e.message))' "$1" "$2" 2>&1; }
targets() { node -e 'fetch("http://127.0.0.1:9335/json/list").then(r=>r.json()).then(l=>console.log(l.map(t=>t.type+" "+t.url).join("\n"))).catch(()=>{})' 2>/dev/null; }
# (Oracle's Chromium's processes: every process of its profile, rudolf-launch's not; RssAnon and PSS summed, MB; the renderers counted)
mem() { python3 - "$T/prof" <<'PY'
import os,sys
prof='--user-data-dir='+sys.argv[1]; P={}
for d in os.listdir('/proc'):
    if not d.isdigit(): continue
    try: a=open('/proc/%s/cmdline'%d,'rb').read().replace(b'\0',b' ').decode('utf-8','replace').split(); pp=int(open('/proc/%s/stat'%d).read().rsplit(')',1)[1].split()[1])
    except Exception: continue
    P[int(d)]=(pp,a)
# (the browser: its profile, no --type, not rudolf-launch; then every process under it: its zygotes, renderers, GPU, utilities)
top=[p for p,(pp,a) in P.items() if a and prof in a and not any(x.startswith('--type=') for x in a) and 'rudolf-launch' not in a[0]]
mine=set(top); ch=True
while ch:
    ch=False
    for p,(pp,a) in P.items():
        if p not in mine and pp in mine: mine.add(p); ch=True
ps=[]
for p in mine:
    try: st=open('/proc/%d/status'%p).read(); sm=open('/proc/%d/smaps_rollup'%p).read()
    except Exception: continue
    g=lambda t,k: int(([l.split()[1] for l in t.splitlines() if l.startswith(k+':')] or ['0'])[0])
    ty=([x[7:] for x in P[p][1] if x.startswith('--type=')] or ['browser'])[0]
    ps.append((ty,g(st,'RssAnon'),g(sm,'Pss')))
r=[p for p in ps if p[0]=='renderer']
print('{"procs":%d,"renderers":%d,"rssanon":%.1f,"pss":%.1f,"renderers_rssanon":%.1f,"renderers_pss":%.1f}'%(len(ps),len(r),sum(p[1] for p in ps)/1024,sum(p[2] for p in ps)/1024,sum(p[1] for p in r)/1024,sum(p[2] for p in r)/1024))
PY
}
w8 '[ "$(st signin)" = 1 ]' 120
PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)"); LC_ALL=C.UTF-8 xdotool type --delay 30 "$PW"; xdotool key Return
w8 '[ "$(st user)" = Ann ] && grep -q "^hit pin browser" $D/state' 120 || { echo "FAIL signing in"; tail $T/native.log; exit 1; }
w8 '[ "$(st svc | cut -d" " -f1)" = ready ]' 240
xdotool mousemove $(hit pin browser) click 1
w8 '[ "$(ev http://127.0.0.1:8080/ "typeof rtOn==\"function\"&&rtOn()&&!!OA.main")" = true ]' 160 || { echo "FAIL Oracle did not open"; tail $T/native.log; exit 1; }
timeout 10 tail -f /dev/null
echo "{\"flags\":\"$*\",\"tabs\":0,\"mem\":$(mem)}"
K=0
for N in 1 3 6; do
	while [ $K -lt $N ]; do K=$((K+1)); ev http://127.0.0.1:8080/ "OA.main.oNewTab('http://127.0.0.$((K+1)):8333/'),1" >/dev/null
		w8 "targets | grep -q '^webview http://127.0.0.$((K+1)):8333/'" 80; timeout 2 tail -f /dev/null; done
	timeout 30 tail -f /dev/null
	echo "{\"flags\":\"$*\",\"tabs\":$N,\"webviews\":$(targets | grep -c '^webview http://127.0.0'),\"mem\":$(mem)}"
done
