# 1.6.1 (docs/plan-1.6.1.md Part 10, docs/plan-oracle.md item 2): closed tabs in Oracle's Lite tabs (rudolf-webkit), on a virtual
# screen (the Lite engine check, webkit-check.yml): Ctrl+Shift+T typed inside a website goes to the desktop (tabev keys cst, which
# reopens Oracle's last closed tab: desktop/oraclosed61.js), before the page's own handler sees it; Ctrl+T still "ct".
#   python3 closed61_ci.py ./rudolf-webkit   (DISPLAY set; xdotool)
import http.server, json, shutil, subprocess, sys, threading, time
BIN = sys.argv[1]; RES = {}
DESK = r'''<!doctype html><meta charset=utf-8><title>desk</title><body style="background:#123">desk<script>
const post=(k,v)=>fetch('/result',{method:'POST',body:JSON.stringify({k,v})});
const send=m=>window.webkit.messageHandlers.rudolfShell.postMessage(m);
const ev=[];let hello=false;
window.__rwkIn=m=>{ev.push(m);if(m.rudolfShell=='hello'&&!hello){hello=true;send({rudolfShell:'ready'});run()}};
const wait=(f,t=20000)=>new Promise((res,rej)=>{const t0=Date.now();const iv=setInterval(()=>{const r=ev.find(f);if(r){clearInterval(iv);res(r)}else if(Date.now()-t0>t){clearInterval(iv);rej(new Error('timeout'))}},50)});
async function step(k,f){try{await post(k,await f())}catch(e){await post(k,'FAIL '+e.message)}}
async function run(){await post('hello',true);
 send({rudolfShell:'tab',op:'open',tid:'t1',url:'http://127.0.0.1:8603/page.html',part:'utest'});
 send({rudolfShell:'tab',op:'place',tid:'t1',r:[100,100,900,500],vis:null,z:1});
 await step('title',async()=>(await wait(m=>m.ev=='title'&&m.tid=='t1'&&m.title=='A page')).title);
 await post('ready_keys',true);
 await step('keys',async()=>{const k=[];for(const want of ['cst','ct']){k.push((await wait(m=>m.ev=='keys'&&m.k==want)).k)}return k});
 await step('seen',async()=>{const t=ev.filter(m=>m.ev=='title').pop();return t.title});
 await post('done',true)}
</script>'''
PAGE = '<!doctype html><meta charset=utf-8><title>A page</title><body style="font:20px sans-serif">A page<script>addEventListener("keydown",e=>{if(e.ctrlKey&&e.code=="KeyT")document.title="the page saw "+(e.shiftKey?"Ctrl+Shift+T":"Ctrl+T")},true)</script>'
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def send(self, code, body, ctype='text/html'):
        b = body.encode(); self.send_response(code); self.send_header('content-type', ctype); self.send_header('content-length', str(len(b))); self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        if self.server.server_port == 8602: return self.send(200, DESK)
        if self.path.startswith('/page.html'): return self.send(200, PAGE)
        self.send(404, 'no')
    def do_POST(self):
        n = int(self.headers.get('content-length', 0)); d = json.loads(self.rfile.read(n) or b'{}'); RES[d.get('k')] = d.get('v'); self.send(200, 'ok', 'text/plain')
for port in (8602, 8603):
    s = http.server.ThreadingHTTPServer(('127.0.0.1', port), H); threading.Thread(target=s.serve_forever, daemon=True).start()
log = open('closed61.txt', 'w')
p = subprocess.Popen([BIN, '--rudolf-kiosk', '--size=1280x800', 'http://127.0.0.1:8602/'], stdout=log, stderr=subprocess.STDOUT)
end = time.time() + 90; typed = False
while time.time() < end and 'done' not in RES and p.poll() is None:
    if not typed and 'ready_keys' in RES:
        typed = True; time.sleep(0.8)
        subprocess.run(['xdotool', 'mousemove', '500', '400', 'click', '1']); time.sleep(0.4)
        for k in ('ctrl+shift+t', 'ctrl+t'): subprocess.run(['xdotool', 'key', k]); time.sleep(0.5)
    time.sleep(0.3)
time.sleep(0.5)
checks = [
    ('a tab opens the page', RES.get('title') == 'A page'),
    ('Ctrl+Shift+T in the page goes to Oracle (keys cst); Ctrl+T still ct', RES.get('keys') == ['cst', 'ct']),
    ("...before the page's own handler sees either", RES.get('seen') == 'A page'),
    ('rudolf-webkit still runs', p.poll() is None),
]
p.terminate(); log.close()
ok = 0
for n, c in checks:
    ok += bool(c); print(('PASS ' if c else 'FAIL ') + n)
print('results', json.dumps(RES))
print(f'{ok}/{len(checks)} passed'); sys.exit(0 if ok == len(checks) else 1)
