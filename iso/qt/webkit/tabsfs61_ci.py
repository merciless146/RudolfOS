# 1.6.1 (docs/plan-1.6.1.md Part 3, from 1.5.8: Oracle's WebKit tabs: a download, full screen, memory with 5 tabs), on a
# virtual screen with a stand-in desktop page (as tabs58_ci.py's): a page's own full screen (a click on its button: a real
# click, as a person's) fills the screen and is said to the desktop (tabev fs), Escape leaves it and the tab is back where
# it was; then five tabs open at once, each its own web process, all loaded, their memory reported (PSS) and each under a
# bound. A download in a tab is tabs58_ci.py's. Real sites (YouTube, a Google sign-in) are the owner's checks.
#   python3 tabsfs61_ci.py ./rudolf-webkit   (DISPLAY set; xdotool)
import http.server, json, os, subprocess, sys, threading, time
BIN = os.path.abspath(sys.argv[1]); RES = {}
DESK = r'''<!doctype html><meta charset=utf-8><title>desk</title><body style="background:#123;margin:0">desk<script>
const post=(k,v)=>fetch('/result',{method:'POST',body:JSON.stringify({k,v})});
const send=m=>window.webkit.messageHandlers.rudolfShell.postMessage(m);
const ev=[];let hello=false;
window.__rwkIn=m=>{ev.push(m);if(m.rudolfShell=='hello'&&!hello){hello=true;send({rudolfShell:'ready'});run().catch(e=>post('error',e.message))}};
const wait=(f,t=25000)=>new Promise((res,rej)=>{const t0=Date.now();const iv=setInterval(()=>{const i=ev.findIndex(f);if(i>=0){clearInterval(iv);res(ev.splice(i,1)[0])}else if(Date.now()-t0>t){clearInterval(iv);rej(new Error('timeout'))}},100)});
const sl=t=>new Promise(r=>setTimeout(r,t));
async function run(){
 send({rudolfShell:'tab',op:'open',tid:'t1',url:'http://127.0.0.1:8605/fs.html',part:'fs'});
 send({rudolfShell:'tab',op:'place',tid:'t1',r:[200,150,600,400],vis:null,z:1});
 await wait(m=>m.ev=='load'&&m.tid=='t1'&&m.loading===false);await sl(1500);await post('fsready',true);
 const on=await wait(m=>m.ev=='fs'&&m.tid=='t1',40000).catch(()=>null);await post('fson',!!(on&&on.on));
 /* (as tabs26.js does in full screen: nothing placed) */
 send({rudolfShell:'tab',op:'place',tid:'t1',r:null});await sl(1500);await post('fsshot',true);
 const off=await wait(m=>m.ev=='fs'&&m.tid=='t1',40000).catch(()=>null);await post('fsoff',!!(off&&off.on===false));
 send({rudolfShell:'tab',op:'place',tid:'t1',r:[200,150,600,400],vis:null,z:1});await sl(1500);await post('backshot',true);await sl(2500);
 send({rudolfShell:'tab',op:'close',tid:'t1'});
 for(let i=1;i<=5;i++){send({rudolfShell:'tab',op:'open',tid:'f'+i,url:'http://127.0.0.1:8605/page'+i,part:'five'});
  send({rudolfShell:'tab',op:'place',tid:'f'+i,r:i==5?[100,100,800,500]:null,z:i})}
 try{for(let i=1;i<=5;i++)await wait(m=>m.ev=='load'&&m.tid=='f'+i&&m.loading===false);await post('five',5)}catch(e){await post('five','FAIL '+e.message)}}
</script>'''
FS = '''<!doctype html><meta charset=utf-8><title>fs</title><body style="background:#fde68a;margin:0">
<button id=b style="position:fixed;inset:0;width:100%;height:100%;background:#fde68a;border:0;font:30px sans-serif">Full screen</button>
<script>b.onclick=()=>document.documentElement.requestFullscreen().catch(e=>document.title='ERR '+e.message)</script>'''
PAGE = '''<!doctype html><meta charset=utf-8><title>page %d</title><body style="background:#e0f2fe;font:16px sans-serif">
<h1>Page %d</h1><canvas id=c width=600 height=300></canvas><p>%s</p><script>const x=c.getContext('2d');let t=0;
setInterval(()=>{t++;x.fillStyle='hsl('+(t*7%%360)+',70%%,60%%)';x.fillRect((t*13)%%600,(t*7)%%300,40,40)},100)</script>'''
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def send(self, code, body, ctype='text/html'):
        b = body.encode(); self.send_response(code); self.send_header('content-type', ctype); self.send_header('content-length', str(len(b))); self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        if self.server.server_port == 8604: return self.send(200, DESK)
        if self.path.startswith('/fs.html'): return self.send(200, FS)
        n = int(self.path[5:] or 0) if self.path.startswith('/page') else 0
        self.send(200, PAGE % (n, n, ' '.join(['Some words of text in a paragraph.'] * 400)))
    def do_POST(self):
        n = int(self.headers.get('content-length', 0)); d = json.loads(self.rfile.read(n) or b'{}'); RES[d.get('k')] = d.get('v'); self.send(200, 'ok', 'text/plain')
for port in (8604, 8605):
    s = http.server.ThreadingHTTPServer(('127.0.0.1', port), H); s.daemon_threads = True; threading.Thread(target=s.serve_forever, daemon=True).start()
def px(f, x, y):
    try:
        o = subprocess.run(['convert', f, '-crop', '1x1+%d+%d' % (x, y), '-depth', '8', 'txt:-'], capture_output=True, text=True).stdout
        h = o.split('#', 2)[-1][:6] if '#' in o else ''
        return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)) if len(h) == 6 else None
    except Exception: return None
yellow = lambda c: bool(c) and c[0] > 200 and c[1] > 190 and c[2] < 180
desk = lambda c: bool(c) and c[0] < 70 and c[1] < 80 and c[2] < 100
def until(k, secs):
    end = time.time() + secs
    while time.time() < end and k not in RES and 'error' not in RES and p.poll() is None: time.sleep(0.3)
    return k in RES
def pss(pid):
    try: return int(next(l for l in open(f'/proc/{pid}/smaps_rollup') if l.startswith('Pss:')).split()[1]) // 1024
    except Exception: return 0
def webprocs(pid):
    L = []
    for d in os.listdir('/proc'):
        if not d.isdigit(): continue
        try:
            st = open(f'/proc/{d}/stat').read().rsplit(')', 1)[1].split()
            if int(st[1]) == pid and 'WebKitWebProces' in open(f'/proc/{d}/comm').read(): L.append((int(st[19]), int(d), pss(int(d))))
        except Exception: pass
    return sorted(L)
N = [0, 0]
def ok(name, c, d=''):
    N[0] += 1; N[1] += bool(c); print(('PASS ' if c else 'FAIL ') + name + ('' if c else ' - ' + str(d)[:500])); sys.stdout.flush()
home = f'/tmp/tabsfs61-{os.getpid()}'; os.makedirs(home, exist_ok=True)
p = subprocess.Popen([BIN, '--size=1280x800', '--draw=soft', 'http://127.0.0.1:8604/'], env=dict(os.environ, HOME=home), stdout=open(home + '.log', 'w'), stderr=subprocess.STDOUT)
if until('fsready', 60):
    for _ in range(3):   # (a click before the page took the pointer: once more)
        subprocess.run(['xdotool', 'mousemove', '500', '350', 'click', '1'])
        if until('fson', 8): break
until('fsshot', 30); time.sleep(0.8); subprocess.run(['import', '-window', 'root', 'tabsfs1.png'])
ok('1. a page\'s own full screen (a click on its button) is said to the desktop', RES.get('fson') is True, RES)
c = [px('tabsfs1.png', 5, 5), px('tabsfs1.png', 1274, 794), px('tabsfs1.png', 640, 150)]
ok('2. ...and fills the screen', all(yellow(x) for x in c), c)
time.sleep(1); subprocess.run(['xdotool', 'key', 'Escape'])
until('backshot', 30); time.sleep(0.8); subprocess.run(['import', '-window', 'root', 'tabsfs2.png'])
ok('3. Escape leaves it, said to the desktop', RES.get('fsoff') is True, RES)
c = [px('tabsfs2.png', 5, 5), px('tabsfs2.png', 300, 200)]
ok('4. ...and the tab is back where it was', desk(c[0]) and yellow(c[1]), c)
until('five', 90); time.sleep(4)
L = webprocs(p.pid); tabs = L[1:]
print('memory (PSS, MB): the desktop page', L[0][2] if L else '-', '; five tabs', [x[2] for x in tabs], 'total', sum(x[2] for x in L))
ok('5. five tabs open at once, all loaded', RES.get('five') == 5, RES)
ok('6. ...each in its own web process (the desktop page\'s and five)', len(L) == 6, L)
ok('7. ...each tab under 300 MB (PSS)', len(tabs) == 5 and all(0 < x[2] < 300 for x in tabs), tabs)
ok('rudolf-webkit still runs', p.poll() is None)
p.terminate()
try: p.wait(10)
except Exception: p.kill()
print('%d/%d passed' % (N[1], N[0])); sys.exit(0 if N[0] == N[1] else 1)
