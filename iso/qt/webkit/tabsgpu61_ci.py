# 1.6.1 (docs/plan-1.6.1.md Part 3, from 1.5.8: stutter in Oracle's tabs on Lite): in mixed drawing (--draw=mixed, Settings'
# default with a graphics card) only the desktop page's own web process paints on the processor; Oracle's tabs' processes
# are painted by the graphics card (rudolf-webkit's desk_cpu_paint). A stand-in desktop page (as tabs58_ci.py's) opens two
# tabs; each WebKit web process of this rudolf-webkit is read from /proc: Skia's processor painting in its environment and
# its painting threads (SkiaCPU...). Checks: the desktop's process paints on the processor, the tabs' do not, and the tabs
# are drawn on the screen; the desktop's process ended: the page is loaded again in a new process that paints on the
# processor again, and a tab opened after it does not; --draw=soft and --draw=gpu as before (none of them set).
#   python3 tabsgpu61_ci.py ./rudolf-webkit   (DISPLAY set; a virtual screen)
import http.server, json, os, subprocess, sys, threading, time, signal
BIN = os.path.abspath(sys.argv[1]); RES = {}
DESK = r'''<!doctype html><meta charset=utf-8><title>desk</title><body style="background:#123;margin:0">desk<script>
const post=(k,v)=>fetch('/result',{method:'POST',body:JSON.stringify({k,v})});
const send=m=>window.webkit.messageHandlers.rudolfShell.postMessage(m);
const ev=[];let hello=false;
window.__rwkIn=m=>{ev.push(m);if(m.rudolfShell=='hello'&&!hello){hello=true;send({rudolfShell:'ready'});run()}};
const wait=(f,t=25000)=>new Promise((res,rej)=>{const t0=Date.now();const iv=setInterval(()=>{const r=ev.find(f);if(r){clearInterval(iv);res(r)}else if(Date.now()-t0>t){clearInterval(iv);rej(new Error('timeout'))}},100)});
async function run(){const n=+(new URLSearchParams(location.search).get('n')||2);await post('hello',performance.timeOrigin);
 for(let i=1;i<=n;i++){send({rudolfShell:'tab',op:'open',tid:'t'+i,url:'http://127.0.0.1:8603/tab'+i,part:'g'+i});
  send({rudolfShell:'tab',op:'place',tid:'t'+i,r:[100*i,100,500,400],vis:null,z:i})}
 try{for(let i=1;i<=n;i++)await wait(m=>m.ev=='load'&&m.tid=='t'+i&&m.loading===false);await post('loaded',n)}catch(e){await post('loaded','FAIL '+e.message)}}
</script>'''
TAB = '<!doctype html><title>tab</title><body style="background:#fde68a;margin:0"><div style="width:200px;height:200px;background:#c80;animation:s 2s linear infinite"></div><style>@keyframes s{to{transform:rotate(360deg)}}</style>'
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def send(self, code, body, ctype='text/html'):
        b = body.encode(); self.send_response(code); self.send_header('content-type', ctype); self.send_header('content-length', str(len(b))); self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        self.send(200, DESK if self.server.server_port == 8602 else TAB)
    def do_POST(self):
        n = int(self.headers.get('content-length', 0)); d = json.loads(self.rfile.read(n) or b'{}'); RES.setdefault(d.get('k'), []).append(d.get('v')); self.send(200, 'ok', 'text/plain')
for port in (8602, 8603):
    s = http.server.ThreadingHTTPServer(('127.0.0.1', port), H); s.daemon_threads = True; threading.Thread(target=s.serve_forever, daemon=True).start()

def children(pid):
    """the web processes this rudolf-webkit started: (pid, start time, cpu painting in its environment, SkiaCPU threads)"""
    out = []
    for d in os.listdir('/proc'):
        if not d.isdigit(): continue
        try:
            st = open(f'/proc/{d}/stat').read(); ppid = int(st.rsplit(')', 1)[1].split()[1]); start = int(st.rsplit(')', 1)[1].split()[19])
            if ppid != pid or 'WebKitWebProces' not in open(f'/proc/{d}/comm').read(): continue
            env = open(f'/proc/{d}/environ', 'rb').read().split(b'\0')
            thr = sum(1 for t in os.listdir(f'/proc/{d}/task') if open(f'/proc/{d}/task/{t}/comm').read().startswith('SkiaCPU'))
            out.append((int(d), start, b'WEBKIT_SKIA_ENABLE_CPU_RENDERING=1' in env, thr))
        except Exception: pass
    return sorted(out, key=lambda x: x[1])
def px(f, x, y):
    try:
        o = subprocess.run(['convert', f, '-crop', '1x1+%d+%d' % (x, y), '-depth', '8', 'txt:-'], capture_output=True, text=True).stdout
        h = o.split('#', 2)[-1][:6] if '#' in o else ''
        return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)) if len(h) == 6 else None
    except Exception: return None
def start(draw, n=2):
    RES.clear(); home = f'/tmp/tabsgpu61-{draw}-{os.getpid()}'; os.makedirs(home, exist_ok=True)
    p = subprocess.Popen([BIN, '--size=1280x800', '--draw=' + draw, f'http://127.0.0.1:8602/?n={n}'], env=dict(os.environ, HOME=home), stdout=open(home + '.log', 'w'), stderr=subprocess.STDOUT)
    end = time.time() + 60
    while time.time() < end and 'loaded' not in RES and p.poll() is None: time.sleep(0.3)
    time.sleep(3)   # (painting threads start with the first frames)
    return p
N = [0, 0]
def ok(name, c, d=''):
    N[0] += 1; N[1] += bool(c); print(('PASS ' if c else 'FAIL ') + name + ('' if c else ' - ' + str(d)[:500])); sys.stdout.flush()

p = start('mixed'); L = children(p.pid); print('mixed', L, RES.get('loaded'))
ok('mixed 1. the desktop page and two tabs: three web processes', len(L) == 3 and RES.get('loaded') == [2], [L, RES])
ok("mixed 2. the desktop's (the first) paints on the processor, with its painting threads", len(L) >= 1 and L[0][2] and L[0][3] > 0, L)
ok("mixed 3. the tabs' paint with the graphics card: no processor painting, no painting threads", len(L) == 3 and not any(x[2] or x[3] for x in L[1:]), L)
subprocess.run(['import', '-window', 'root', 'tabsgpu.png'])
c1, c2 = px('tabsgpu.png', 150 + 250, 450), px('tabsgpu.png', 650, 450)
ok('mixed 4. the tabs are drawn on the screen', c1 and c1[0] > 200 and c1[1] > 190 and c1[2] < 180 and c2 and c2[0] > 200 and c2[2] < 180, [c1, c2])
# the desktop's process stops: rudolf-webkit loads the page again
desk = L[0][0] if L else 0
if desk: os.kill(desk, signal.SIGKILL)
end = time.time() + 40
while time.time() < end and len(RES.get('hello', [])) < 2 and p.poll() is None: time.sleep(0.3)
end = time.time() + 40
while time.time() < end and len(RES.get('loaded', [])) < 2 and p.poll() is None: time.sleep(0.3)
time.sleep(3); L2 = children(p.pid); print('after the desktop stopped', L2, RES.get('loaded'))
ok('mixed 5. the desktop stopped: loaded again, its tabs opened again', len(RES.get('hello', [])) >= 2 and RES.get('loaded', [None, None])[-1] == 2, RES)
new = [x for x in L2 if x[0] != desk and x[0] not in [y[0] for y in L]]
ok("mixed 6. ...the new desktop process paints on the processor again, the new tabs' with the graphics card", len(new) >= 3 and new[0][2] and new[0][3] > 0 and not any(x[2] or x[3] for x in new[1:]), new)
p.terminate(); p.wait(10)
for draw, want in (('soft', False), ('gpu', False)):
    p = start(draw, 1); L = children(p.pid); print(draw, L)
    ok(f'{draw}: as before, no process paints with Skia on the processor by the environment', len(L) == 2 and all(x[2] == want for x in L), L)
    p.terminate(); p.wait(10)
print('%d/%d passed' % (N[1], N[0])); sys.exit(0 if N[0] == N[1] else 1)
