# 1.5.8 (plan 9.5): Oracle's tabs in rudolf-webkit, on a virtual screen (the Lite engine check, webkit-check.yml).
# A stand-in desktop page (port 8090) talks to rudolf-webkit as the desktop does (tabs26.js's messages); a test site
# (port 8091) has a page that forbids frames (X-Frame-Options: DENY, which a frame could not show), a prompt, and a
# download. Checks: hello and ready; a tab opens the page and reports its title; the prompt comes to the desktop and its
# answer goes back to the page; a download arrives in the download folder (as Chromium's do); the firewall blocks a
# page; the tab is placed, cut and closed. python3 tabs58_ci.py ./rudolf-webkit   (DISPLAY set, /run/rudolf/dl writable)
import http.server, json, os, subprocess, sys, threading, time
BIN = sys.argv[1]; DRAW = sys.argv[2] if len(sys.argv) > 2 else ''; RES = {}; PROMPT = []
DESK = r'''<!doctype html><meta charset=utf-8><title>desk</title><body style="background:#123">desk<script>
const post=(k,v)=>fetch('/result',{method:'POST',body:JSON.stringify({k,v})});
const send=m=>window.webkit.messageHandlers.rudolfShell.postMessage(m);
const ev=[];let hello=false;
window.__rwkIn=m=>{ev.push(m);if(m.rudolfShell=='hello'&&!hello){hello=true;send({rudolfShell:'ready'});run()}
 if(m.rudolfShell=='tabev'&&m.ev=='dialog')send({rudolfShell:'tab',op:'dialog',did:m.did,ok:true,text:'typed in the desktop'})};
const wait=(f,t=25000)=>new Promise((res,rej)=>{const t0=Date.now();const iv=setInterval(()=>{const r=ev.find(f);if(r){clearInterval(iv);res(r)}else if(Date.now()-t0>t){clearInterval(iv);rej(new Error('timeout'))}},100)});
async function step(k,f){try{await post(k,await f())}catch(e){await post(k,'FAIL '+e.message)}}
async function run(){await post('hello',true);
 send({rudolfShell:'tab',op:'open',tid:'t1',url:'http://127.0.0.1:8091/xfo.html',part:'utest'});
 send({rudolfShell:'tab',op:'place',tid:'t1',r:[100,100,700,450],vis:null,z:1});
 await step('title',async()=>(await wait(m=>m.ev=='title'&&m.tid=='t1'&&m.title=='XFO page')).title);
 await post('shot1',true); await new Promise(r=>setTimeout(r,2500));
 await step('nav',async()=>(await wait(m=>m.ev=='nav'&&m.tid=='t1'&&/xfo\.html$/.test(m.url))).url);
 await step('dialog',async()=>(await wait(m=>m.ev=='dialog'&&m.tid=='t1')).kind);
 send({rudolfShell:'tab',op:'place',tid:'t1',r:[100,100,700,450],vis:[[0,0,350,450]],z:1});
 await new Promise(r=>setTimeout(r,1500)); await post('shot2',true); await new Promise(r=>setTimeout(r,2000));
 send({rudolfShell:'tab',op:'go',tid:'t1',url:'http://127.0.0.1:8091/file.bin'});
 await step('download',async()=>(await wait(m=>m.ev=='download'&&m.tid=='t1')).url);
 send({rudolfShell:'tab',op:'fw',rules:{on:true,mode:'open',block:['blocked.test'],trust:[]}});
 send({rudolfShell:'tab',op:'go',tid:'t1',url:'http://blocked.test/'});
 await step('blocked',async()=>(await wait(m=>m.ev=='blocked'&&m.tid=='t1')).why);
 send({rudolfShell:'tab',op:'close',tid:'t1'});
 await post('done',true)}
</script>'''
XFO = '''<!doctype html><meta charset=utf-8><title>XFO page</title><body style="background:#fde68a;font:40px sans-serif">A page that forbids frames
<script>setTimeout(()=>{const v=prompt('Your name?','nobody');fetch('/prompt?v='+encodeURIComponent(v))},800)</script>'''
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def send(self, code, body, ctype='text/html', extra=()):
        b = body.encode() if isinstance(body, str) else body
        self.send_response(code); self.send_header('content-type', ctype); self.send_header('content-length', str(len(b)))
        for k, v in extra: self.send_header(k, v)
        self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        p = self.path
        if self.server.server_port == 8090: return self.send(200, DESK)
        if p.startswith('/xfo.html'): return self.send(200, XFO, extra=[('X-Frame-Options', 'DENY'), ('Content-Security-Policy', "frame-ancestors 'none'")])
        if p.startswith('/prompt'): PROMPT.append(p.split('v=', 1)[1] if 'v=' in p else ''); return self.send(200, 'ok', 'text/plain')
        if p.startswith('/file.bin'): return self.send(200, b'\0' * 100000, 'application/octet-stream', [('Content-Disposition', 'attachment; filename="file.bin"')])
        self.send(404, 'no')
    def do_POST(self):
        n = int(self.headers.get('content-length', 0)); d = json.loads(self.rfile.read(n) or b'{}'); RES[d.get('k')] = d.get('v'); self.send(200, 'ok', 'text/plain')
for port in (8090, 8091):
    s = http.server.ThreadingHTTPServer(('127.0.0.1', port), H); threading.Thread(target=s.serve_forever, daemon=True).start()
log = open('tabs.txt', 'w')
p = subprocess.Popen([BIN, '--rudolf-kiosk', '--size=1280x800'] + (['--draw=' + DRAW] if DRAW else []) + ['http://127.0.0.1:8090/'], stdout=log, stderr=subprocess.STDOUT)
end = time.time() + 120; shot = False; s1 = s2 = False
suf = '-' + DRAW if DRAW else ''
while time.time() < end and 'done' not in RES and p.poll() is None:
    # 1.5.8: what is really on the screen: the new tab's first drawing (no reload), and a cut where the desktop covers it
    if not s1 and 'shot1' in RES: time.sleep(1.5); subprocess.run(['import', '-window', 'root', 'tabs1' + suf + '.png']); s1 = True
    if not s2 and 'shot2' in RES: time.sleep(.8); subprocess.run(['import', '-window', 'root', 'tabs2' + suf + '.png']); s2 = True
    if not shot and 'dialog' in RES:
        time.sleep(2.5); subprocess.run(['import', '-window', 'root', 'tabs.png']); shot = True
    time.sleep(0.5)
time.sleep(1); dl = '/run/rudolf/dl/.incoming/file.bin'
def px(f, x, y):
    # one pixel as (r, g, b) 0-255, from ImageMagick's text format ("0,0: (253,230,138) ...")
    try:
        out = subprocess.run(['convert', f, '-crop', '1x1+%d+%d' % (x, y), '-depth', '8', 'txt:-'], capture_output=True, text=True).stdout
        import re; m = re.search(r'\((\d+),(\d+),(\d+)', out.replace(' ', ''))
        return tuple(int(v) for v in m.groups()) if m else None
    except Exception: return None
yellow = lambda c: bool(c) and c[0] > 200 and c[1] > 190 and c[2] < 180
desk = lambda c: bool(c) and c[0] < 70 and c[1] < 80 and c[2] < 100
P1, P2L, P2R = px('tabs1' + suf + '.png', 600, 400), px('tabs2' + suf + '.png', 250, 400), px('tabs2' + suf + '.png', 650, 400)
print('pixels', DRAW or 'default', P1, P2L, P2R)
checks = [
    ('rudolf-webkit says hello, the desktop answers ready', RES.get('hello') is True),
    ('a tab opens a page that forbids frames, and reports its title', RES.get('title') == 'XFO page'),
    ('...and its address', str(RES.get('nav', '')).endswith('/xfo.html')),
    ("the page's prompt comes to the desktop", RES.get('dialog') == 'prompt'),
    ("...and the desktop's answer goes back to the page", PROMPT == ['typed%20in%20the%20desktop'] or PROMPT == ['typed in the desktop']),
    ('a download is reported and arrives in the download folder, complete', str(RES.get('download', '')).endswith('/file.bin') and os.path.exists(dl) and os.path.getsize(dl) == 100000 and not os.path.exists(dl + '.crdownload')),
    ('the firewall blocks a page in a tab', 'blocked list' in str(RES.get('blocked', ''))),
    ('a new tab shows its page at once, without a reload (' + (DRAW or 'default') + ')', yellow(P1)),
    ('a cut shows the desktop where it covers the tab, the page elsewhere (' + (DRAW or 'default') + ')', yellow(P2L) and desk(P2R)),
    ('rudolf-webkit still runs', p.poll() is None),
]
p.terminate(); log.close()
ok = 0
for n, c in checks:
    ok += bool(c); print(('PASS ' if c else 'FAIL ') + n)
print('results', json.dumps(RES), 'prompt', PROMPT)
print(f'{ok}/{len(checks)} passed'); sys.exit(0 if ok == len(checks) else 1)
