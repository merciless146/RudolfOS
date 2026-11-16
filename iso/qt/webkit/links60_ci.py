# 1.6.0: an app's own link scheme in Oracle's tabs on the Lite engine (rudolf-webkit), on a virtual screen (webkit-check.yml).
# A sign-in page sends the tab to teststand://done?code=... (a Linux app's own scheme, as claude:// after "Continue with
# Google"): by a redirect (HTTP 302) and by the page's script. rudolf-webkit must report it as Chromium does (a load
# error net::ERR_UNKNOWN_URL_SCHEME with the link), so the desktop gives it to the app (desktop/links60.js), and keep the
# page (no error page: the page's address and title stay). A plain page that cannot be reached still reports WebKit's own
# error. Ports 8600 (the stand-in desktop) and 8601 (the site).   python3 links60_ci.py ./rudolf-webkit   (DISPLAY set)
import http.server, json, subprocess, sys, threading, time
BIN = sys.argv[1]; RES = {}
DESK = r'''<!doctype html><meta charset=utf-8><title>desk</title><body style="background:#123">desk<script>
const post=(k,v)=>fetch('/result',{method:'POST',body:JSON.stringify({k,v})});
const send=m=>window.webkit.messageHandlers.rudolfShell.postMessage(m);
const ev=[];let hello=false;
window.__rwkIn=m=>{ev.push(m);if(m.rudolfShell=='hello'&&!hello){hello=true;send({rudolfShell:'ready'});run()}};
const wait=(f,t=25000)=>new Promise((res,rej)=>{const t0=Date.now();const iv=setInterval(()=>{const r=ev.find(f);if(r){clearInterval(iv);res(r)}else if(Date.now()-t0>t){clearInterval(iv);rej(new Error('timeout'))}},100)});
async function step(k,f){try{await post(k,await f())}catch(e){await post(k,'FAIL '+e.message)}}
const sch=t=>m=>m.ev=='load'&&m.tid==t&&/ERR_UNKNOWN_URL_SCHEME/.test(m.error||'');
async function run(){await post('hello',true);
 send({rudolfShell:'tab',op:'open',tid:'t1',url:'http://127.0.0.1:8601/signin',part:'ltest'});
 send({rudolfShell:'tab',op:'place',tid:'t1',r:[100,100,700,450],vis:null,z:1});
 await step('title',async()=>(await wait(m=>m.ev=='title'&&m.tid=='t1'&&m.title=='Signed in')).title);
 await step('script',async()=>(await wait(sch('t1'))).url);
 await new Promise(r=>setTimeout(r,2500));
 await step('after',async()=>{const n=ev.filter(m=>m.tid=='t1'&&m.ev=='nav').map(m=>m.url);const t=ev.filter(m=>m.tid=='t1'&&m.ev=='title').map(m=>m.title);return {nav:n.at(-1),title:t.at(-1)}});
 send({rudolfShell:'tab',op:'open',tid:'t2',url:'http://127.0.0.1:8601/redirect',part:'ltest'});
 send({rudolfShell:'tab',op:'place',tid:'t2',r:[100,100,700,450],vis:null,z:2});
 await step('redirect',async()=>(await wait(sch('t2'))).url);
 send({rudolfShell:'tab',op:'go',tid:'t2',url:'http://127.0.0.1:8609/nothing-here'});
 await step('plain',async()=>{const m=await wait(m=>m.ev=='load'&&m.tid=='t2'&&m.error&&!/UNKNOWN_URL_SCHEME/.test(m.error));return m.error});
 send({rudolfShell:'tab',op:'close',tid:'t1'});send({rudolfShell:'tab',op:'close',tid:'t2'});
 await post('done',true)}
</script>'''
SIGNIN = '''<!doctype html><meta charset=utf-8><title>Signed in</title><body style="background:#bbf7d0;font:30px sans-serif">Signed in. Going back to the app...
<script>setTimeout(()=>{location.href='teststand://done?code=S1gn-In_42&state=xyz'},700)</script>'''
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def send(self, code, body, ctype='text/html', extra=()):
        b = body.encode() if isinstance(body, str) else body
        self.send_response(code); self.send_header('content-type', ctype); self.send_header('content-length', str(len(b)))
        for k, v in extra: self.send_header(k, v)
        self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        if self.server.server_port == 8600: return self.send(200, DESK)
        if self.path.startswith('/signin'): return self.send(200, SIGNIN)
        if self.path.startswith('/redirect'): return self.send(302, '', extra=[('Location', 'teststand://done?code=R3dir_7')])
        self.send(404, 'no')
    def do_POST(self):
        n = int(self.headers.get('content-length', 0)); d = json.loads(self.rfile.read(n) or b'{}'); RES[d.get('k')] = d.get('v'); self.send(200, 'ok', 'text/plain')
for port in (8600, 8601):
    s = http.server.ThreadingHTTPServer(('127.0.0.1', port), H); threading.Thread(target=s.serve_forever, daemon=True).start()
log = open('links60.txt', 'w')
p = subprocess.Popen([BIN, '--rudolf-kiosk', '--size=1280x800', 'http://127.0.0.1:8600/'], stdout=log, stderr=subprocess.STDOUT)
end = time.time() + 120
while time.time() < end and 'done' not in RES and p.poll() is None: time.sleep(0.5)
after = RES.get('after') or {}
checks = [
    ('rudolf-webkit says hello, the desktop answers ready', RES.get('hello') is True),
    ("a page's script sends the tab to an app's own scheme: reported as Chromium does, with the link", RES.get('script') == 'teststand://done?code=S1gn-In_42&state=xyz'),
    ('...and the page stays (its address and title; no error page)', isinstance(after, dict) and str(after.get('nav', '')).endswith('/signin') and after.get('title') == 'Signed in'),
    ('a redirect (HTTP 302) to the scheme: reported the same way', RES.get('redirect') == 'teststand://done?code=R3dir_7'),
    ("a web page that cannot be reached still reports WebKit's own error", isinstance(RES.get('plain'), str) and not str(RES.get('plain')).startswith('FAIL')),
    ('rudolf-webkit still runs', p.poll() is None),
]
p.terminate(); log.close()
ok = 0
for n, c in checks:
    ok += bool(c); print(('PASS ' if c else 'FAIL ') + n)
print('results', json.dumps(RES))
print(f'{ok}/{len(checks)} passed'); sys.exit(0 if ok == len(checks) else 1)
