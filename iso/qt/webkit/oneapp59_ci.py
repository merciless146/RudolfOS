# 1.5.9 (plan Part 12, step 1): the page's one-app mode (index.html#app=NAME) in WebKitGTK, the engine of Lite and of the
# native desktop's app windows: rudolf-webkit shows the real page from a stand-in rudolfd (port 8296). First the desktop
# makes an account (as a sign-in leaves it); then #app=notepad signs in from the stand-in's native session, shows
# Notepad alone filling the page, and closes when asked (its files written first).
#   python3 oneapp59_ci.py ./rudolf-webkit [WWW]   (DISPLAY set; HOME is a scratch folder for WebKit's storage)
import http.server, json, os, subprocess, sys, threading, time, tempfile
BIN = sys.argv[1]; WWW = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../www')
RES = {}; SESSION = {}
TEST = r'''<script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0,engine:"webkit"};
try{localStorage.setItem('bd95setup','1')}catch(e){}
addEventListener('load',()=>{const post=(k,v)=>fetch('/result',{method:'POST',body:JSON.stringify({k,v})});
 const until=(f,t=30000)=>new Promise((res,rej)=>{const t0=Date.now();const iv=setInterval(()=>{let r;try{r=f()}catch(e){}if(r){clearInterval(iv);res(r)}else if(Date.now()-t0>t){clearInterval(iv);rej(new Error('timeout'))}},200)});
 (async()=>{try{await until(()=>typeof oaMerge3=='function');
  if(!ONEAPP){await FSREADY;const a=await acctNew('Ann','pw1','admin');ACCTS=[a];window.__sys=1;saveUser();USER=a;await userLogin(a,'pw1');saveUser();
   fs['C:'].SHARED={};fsSave();fsFlush();await new Promise(r=>setTimeout(r,1500));
   const pk=[...new Uint8Array(await pbkBits('pw1',a.ks,256))].map(x=>x.toString(16).padStart(2,'0')).join('');
   await post('account',{id:a.id,pk});location.replace('/?one#app=notepad');return}
  await until(()=>ONEAPP.main);await new Promise(r=>setTimeout(r,1000));
  const r=ONEAPP.main.el.getBoundingClientRect(),vis=e=>!!e&&getComputedStyle(e).display!='none';
  await post('open',{user:USER&&USER.n,wins:wins.map(w=>w.app),rect:[r.left,r.top,r.width,r.height],inner:[innerWidth,innerHeight],bar:vis(document.getElementById('bar')),login:vis(document.getElementById('login')),title:document.title,webkit:document.documentElement.classList.contains('webkit')});
  fs['C:'].SHARED['WK.TXT']='from WebKit';fsSave();window.__oneAppClose();
  await until(()=>/closed/.test((document.getElementById('oamsg')||{}).textContent||''),10000);
  const v=await kvGet('fs:system');await post('closed',{saved:!!(v&&v['C:'].SHARED&&v['C:'].SHARED['WK.TXT'])})}catch(e){post('error',String(e&&e.stack||e))}})()});
</script>'''
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def send(self, code, body, ctype='application/json'):
        b = body.encode() if isinstance(body, str) else body
        self.send_response(code); self.send_header('content-type', ctype); self.send_header('content-length', str(len(b))); self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        p = self.path.split('?')[0]
        if p in ('/', '/index.html'):
            return self.send(200, open(os.path.join(WWW, 'index.html'), encoding='utf-8').read().replace('<head>', '<head>' + TEST, 1), 'text/html; charset=utf-8')
        if p == '/api/desktop/session':
            return self.send(200, json.dumps(dict(ok=True, **SESSION))) if SESSION else self.send(404, '{"ok":false,"error":"no native session"}')
        if p == '/api/events/wait': time.sleep(5); return self.send(200, '{"ok":true,"events":""}')
        if p.startswith('/api/'): return self.send(200, '{"ok":true}')
        f = os.path.join(WWW, p.lstrip('/'))
        if p.startswith('/a/') and os.path.isfile(f): return self.send(200, open(f, 'rb').read(), 'application/octet-stream')
        self.send(404, 'not found', 'text/plain')
    def do_POST(self):
        n = int(self.headers.get('content-length') or 0); b = self.rfile.read(n) if n else b''
        if self.path == '/result':
            m = json.loads(b); RES[m['k']] = m['v']
            if m['k'] == 'account': SESSION.update(user={'n': 'Ann', 'id': m['v']['id'], 'r': 'admin'}, pk=m['v']['pk'])
        self.send(200, '{"ok":true}')
class S(http.server.ThreadingHTTPServer):
    daemon_threads = True
    def handle_error(self, *a): pass
srv = S(('127.0.0.1', 8296), H); threading.Thread(target=srv.serve_forever, daemon=True).start()
home = tempfile.mkdtemp(); env = dict(os.environ, HOME=home)
proc = subprocess.Popen([BIN, '--rudolf-kiosk', '--size=1000x700', '--draw=soft', 'http://127.0.0.1:8296/'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
t0 = time.time()
while time.time() - t0 < 150 and 'closed' not in RES and 'error' not in RES: time.sleep(0.5)
proc.terminate()
try: proc.wait(10)
except Exception: proc.kill()
srv.shutdown(); subprocess.run(['rm', '-rf', home])
P = []
def ok(n, c, d=None): P.append(bool(c)); print(('PASS ' if c else 'FAIL ') + n + ('' if c or d is None else ' - ' + json.dumps(d)[:500]))
o = RES.get('open') or {}
ok('1. WebKit: the desktop made an account (as a sign-in leaves it)', 'account' in RES, RES.get('error'))
ok('2. #app=notepad signs in from the native session, without its sign-in screen', o.get('user') == 'Ann' and not o.get('login'), o or RES.get('error'))
ok('3. ...Notepad alone, filling the page, no taskbar', o.get('wins') == ['notepad'] and o.get('rect') == [0, 0] + (o.get('inner') or []) and not o.get('bar'), o)
ok("4. ...the page's title is the app's (the window's title)", 'Notepad' in (o.get('title') or ''), o)
ok('5. closing it writes its files, then the page closes', (RES.get('closed') or {}).get('saved'), RES.get('closed') or RES.get('error'))
print('%d/%d passed' % (sum(P), len(P))); sys.exit(0 if all(P) else 1)
