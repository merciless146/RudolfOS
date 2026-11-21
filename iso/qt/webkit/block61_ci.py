# 1.6.1 (docs/plan-1.6.1.md Part 10, docs/plan-oracle.md item 5): ad and tracker blocking in Oracle's Lite tabs (rudolf-webkit), on a
# virtual screen (the Lite engine check, webkit-check.yml). A stand-in desktop page (port 8606) sends the lists as orablock61.js does
# (op block); a test site (port 8607, served on every 127.0.0.x) has a page at 127.0.0.1 with an "ad" picture and a "tracker" script
# from 127.0.0.2 and a picture of its own. Checks (by what reaches the site): the ad and the tracker never load, the page's own picture
# does, the tab counts 2 (tabev adblock); turned off for 127.0.0.1: both load and the count is 0; "blocked from other sites only"
# blocks them on 127.0.0.1's page; switched off altogether: they load, except in a private window's tab, which always blocks.
#   python3 block61_ci.py ./rudolf-webkit   (DISPLAY set)
import http.server, json, subprocess, sys, threading, time
BIN = sys.argv[1]; RES = {}; HITS = []
DESK = r'''<!doctype html><meta charset=utf-8><title>desk</title><body style="background:#123">desk<script>
const post=(k,v)=>fetch('/result',{method:'POST',body:JSON.stringify({k,v})});
const send=m=>window.webkit.messageHandlers.rudolfShell.postMessage(m);
const ev=[];let hello=false;
window.__rwkIn=m=>{ev.push(m);if(m.rudolfShell=='hello'&&!hello){hello=true;send({rudolfShell:'ready'});run()}};
const sleep=t=>new Promise(r=>setTimeout(r,t));
const hits=async()=>(await (await fetch('/hits')).json());
const cfg=o=>send({rudolfShell:'tab',op:'block',cfg:Object.assign({on:true,b:[],tp:[],ex:[],off:[]},o)});
let N=0;async function page(part){const tid='t'+(++N);await fetch('/clear');send({rudolfShell:'tab',op:'open',tid,url:'http://127.0.0.1:8607/page.html?'+N,part:part||'utest'});
 send({rudolfShell:'tab',op:'place',tid,r:[100,100,700,400],vis:null,z:1});await sleep(3000);const c=ev.filter(m=>m.ev=='adblock'&&m.tid==tid).pop();send({rudolfShell:'tab',op:'close',tid});return {hits:(await hits()).sort(),n:c?c.n:null}}
async function run(){await post('hello',true);
 cfg({b:['127.0.0.2']});await sleep(2500);await post('blocked',await page());
 cfg({b:['127.0.0.2'],off:['127.0.0.1']});await sleep(2500);await post('offsite',await page());
 cfg({tp:['127.0.0.2']});await sleep(2500);await post('thirdparty',await page());
 cfg({on:false,b:['127.0.0.2']});await sleep(2500);await post('alloff',await page());await post('private',await page('priv-x1'));
 await post('done',true)}
</script>'''
PAGE = '<!doctype html><meta charset=utf-8><title>Site</title><body>A site <img src="http://127.0.0.2:8607/ad.png"> <img src="/own.png"><script src="http://127.0.0.2:8607/track.js"></script>'
PNG = bytes.fromhex('89504e470d0a1a0a0000000d4948445200000001000000010806000000') + b'\x1f\x15\xc4\x89\x00\x00\x00\rIDATx\x9cc\xf8\x0f\x00\x00\x01\x01\x00\x05\x18\xd8N\x00\x00\x00\x00IEND\xaeB`\x82'
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def send(self, code, body, ctype='text/html'):
        b = body.encode() if isinstance(body, str) else body
        self.send_response(code); self.send_header('content-type', ctype); self.send_header('content-length', str(len(b))); self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        p = self.path; host = self.headers.get('Host', '')
        if self.server.server_port == 8606:
            if p == '/hits': return self.send(200, json.dumps(HITS), 'application/json')
            if p == '/clear': HITS.clear(); return self.send(200, 'ok', 'text/plain')
            return self.send(200, DESK)
        if p.startswith('/page.html'): return self.send(200, PAGE)
        HITS.append(host.split(':')[0] + p)
        if p.endswith('.png'): return self.send(200, PNG, 'image/png')
        if p.endswith('.js'): return self.send(200, 'var tracked=1', 'text/javascript')
        self.send(404, 'no')
    def do_POST(self):
        n = int(self.headers.get('content-length', 0)); d = json.loads(self.rfile.read(n) or b'{}'); RES[d.get('k')] = d.get('v'); self.send(200, 'ok', 'text/plain')
for port, addr in ((8606, '127.0.0.1'), (8607, '0.0.0.0')):
    s = http.server.ThreadingHTTPServer((addr, port), H); threading.Thread(target=s.serve_forever, daemon=True).start()
log = open('block61.txt', 'w')
p = subprocess.Popen([BIN, '--rudolf-kiosk', '--size=1280x800', 'http://127.0.0.1:8606/'], stdout=log, stderr=subprocess.STDOUT)
end = time.time() + 120
while time.time() < end and 'done' not in RES and p.poll() is None: time.sleep(0.3)
time.sleep(0.5)
OWN = '127.0.0.1/own.png'; ADS = ['127.0.0.2/ad.png', '127.0.0.2/track.js']
g = lambda k: RES.get(k) or {}
blocked = lambda r: OWN in r.get('hits', []) and not any(a in r.get('hits', []) for a in ADS)
loaded = lambda r: OWN in r.get('hits', []) and all(a in r.get('hits', []) for a in ADS)
checks = [
    ("blocked: the ad and the tracker never reach the site, the page's own picture does", blocked(g('blocked'))),
    ('...the tab counts 2', g('blocked').get('n') == 2),
    ('turned off for the site (127.0.0.1): both load, the count is 0', loaded(g('offsite')) and g('offsite').get('n') == 0),
    ('"blocked from other sites only": blocked on 127.0.0.1\'s page', blocked(g('thirdparty'))),
    ('switched off: they load', loaded(g('alloff'))),
    ("...but a private window's tab always blocks", blocked(g('private'))),
    ('rudolf-webkit still runs', p.poll() is None),
]
p.terminate(); log.close()
ok = 0
for n, c in checks:
    ok += bool(c); print(('PASS ' if c else 'FAIL ') + n)
print('results', json.dumps(RES)[:900])
print(f'{ok}/{len(checks)} passed'); sys.exit(0 if ok == len(checks) else 1)
