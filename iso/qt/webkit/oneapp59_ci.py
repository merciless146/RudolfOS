# 1.5.9 (plan Part 12, step 1): the page's one-app mode (index.html#app=NAME) in WebKitGTK, as oneapp59_t.js checks it in
# Chromium. A stand-in rudolfd serves the real page (iso/www) with a small reporter added; rudolf-webkit shows it on a
# virtual screen. First an account is made and signed in as the page does (its keys reported); then each app opens with
# #app=NAME, signed in from the stand-in's desktop/session with those keys: alone, filling the page, its title as the
# page's, its home folder readable, and closed cleanly.   python3 oneapp59_ci.py ./rudolf-webkit   (DISPLAY set)
import http.server, json, os, subprocess, sys, tempfile, threading, time
BIN = sys.argv[1]; WWW = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../www'); PORT = 8296
RES = {}; SESSION = {}
REPORT = r'''<script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};
window.__rudolfLsFlush=()=>fetch('/api/lsync',{method:'POST',headers:{'X-Rudolf-Token':'t'},body:'{}'});
(function(){const post=(k,v)=>fetch('/result',{method:'POST',body:JSON.stringify({k,v})});
 const until=(f,t)=>new Promise((res,rej)=>{const t0=Date.now(),iv=setInterval(()=>{let r;try{r=f()}catch(e){}if(r){clearInterval(iv);res(r)}else if(Date.now()-t0>t){clearInterval(iv);rej(new Error('timeout'))}},200)});
 addEventListener('load',async()=>{try{await until(()=>typeof oaOpen=='function'&&typeof acctNew=='function',30000);
  if(!ONEAPP){svj('bd95setup',1);const o=document.getElementById('oobe');o&&o.remove();
   const a=await acctNew('Ann','pw-one','admin');ACCTS=[a];window.__sys=1;saveUser();USER=a;await userLogin(a,'pw-one');
   HOME.DOCUMENTS['NOTE.TXT']='written before';fsSave();fsFlush();await new Promise(r=>setTimeout(r,1500));
   const hex=u=>[...new Uint8Array(u)].map(x=>x.toString(16).padStart(2,'0')).join('');
   return post('setup',{id:a.id,pk:hex(await pbkBits('pw-one',a.ks,256)),fk:hex(FKRAW)})}
  await until(()=>document.querySelector('.win.oamain')||document.getElementById('oaclosed'),25000);
  await new Promise(r=>setTimeout(r,500));
  const w=document.querySelector('.win.oamain'),vis=e=>!!e&&getComputedStyle(e).display!='none'&&e.getClientRects().length>0,r=w&&w.getBoundingClientRect(),tb=w&&w.querySelector('.tb');
  await post('open',{user:USER&&USER.n,login:vis(document.getElementById('login')),bar:vis(document.getElementById('bar')),r:r&&[r.left,r.top,r.width,r.height],vw:[innerWidth,innerHeight],
   tb:tb&&vis(tb),title:document.title,wt:tb&&tb.querySelector('.t').textContent.trim(),home:HOME&&HOME.DOCUMENTS&&HOME.DOCUMENTS['NOTE.TXT'],app:OA.main&&OA.main.app});
  window.__oneAppClose();
  await until(()=>document.getElementById('oaclosed'),10000);
  await post('closed',{main:!!document.querySelector('.win.oamain')})}catch(e){post('error',String(e&&e.message||e))}})})();</script>'''
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def send(self, code, body, ctype='application/json'):
        b = body.encode() if isinstance(body, str) else body
        self.send_response(code); self.send_header('content-type', ctype); self.send_header('content-length', str(len(b))); self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        u = self.path.split('?')[0]
        if u in ('/', '/index.html'):
            return self.send(200, open(os.path.join(WWW, 'index.html'), encoding='utf-8').read().replace('<head>', '<head>' + REPORT, 1), 'text/html; charset=utf-8')
        if u == '/api/events/wait': time.sleep(5); return self.send(200, '{"ok":true,"events":[]}')
        if u == '/api/desktop/session': return self.send(200, json.dumps(SESSION)) if SESSION else self.send(404, '{"ok":false}')
        if u.startswith('/api/'): return self.send(200, '{"ok":true}')
        f = os.path.normpath(os.path.join(WWW, u.lstrip('/')))
        if f.startswith(os.path.normpath(WWW)) and os.path.isfile(f):
            ct = {'.js': 'text/javascript', '.webp': 'image/webp', '.jpg': 'image/jpeg', '.woff2': 'font/woff2', '.woff': 'font/woff', '.wasm': 'application/wasm'}.get(os.path.splitext(f)[1], 'application/octet-stream')
            return self.send(200, open(f, 'rb').read(), ct)
        self.send(404, 'not found', 'text/plain')
    def do_POST(self):
        n = int(self.headers.get('content-length') or 0); body = self.rfile.read(n)
        if self.path == '/result':
            m = json.loads(body); RES[m['k']] = m['v']
        self.send(200, '{"ok":true}')
class S(http.server.ThreadingHTTPServer):
    daemon_threads = True
    def handle_error(self, *a): pass   # (rudolf-webkit closed while a long poll waited)
srv = S(('127.0.0.1', PORT), H); threading.Thread(target=srv.serve_forever, daemon=True).start()
HOME = tempfile.mkdtemp(prefix='oneapp59-')
def run(hash_, want, secs=60):
    RES.clear()
    p = subprocess.Popen([BIN, '--size=1100x760', '--draw=soft', 'http://127.0.0.1:%d/%s' % (PORT, hash_)], env=dict(os.environ, HOME=HOME),
                         stdout=open(os.path.join(HOME, 'wk.log'), 'a'), stderr=subprocess.STDOUT)
    t0 = time.time()
    while time.time() - t0 < secs and not any(k in RES for k in want + ['error']): time.sleep(0.3)
    p.terminate()
    try: p.wait(5)
    except Exception: p.kill()
N = [0, 0]
def ok(name, c, d=''):
    N[0] += 1; N[1] += bool(c); print(('PASS ' if c else 'FAIL ') + name + ('' if c else ' - ' + str(d)[:400])); sys.stdout.flush()
run('', ['setup'])
ok('0. an account made and signed in once in WebKit (its keys reported)', isinstance(RES.get('setup'), dict) and len(RES['setup'].get('fk') or '') == 64, RES)
if isinstance(RES.get('setup'), dict):
    s = RES['setup']; SESSION.update({'ok': True, 'id': s['id'], 'n': 'Ann', 'r': 'admin', 'pk': s['pk'], 'fk': s['fk']})
    for k in ['notepad', 'calc', 'settings', 'explorer', 'paint', 'taskmgr']:
        run('#app=' + k, ['closed'])
        o = RES.get('open') or {}
        ok(k + ' 1. opens alone, signed in from the session, no sign-in screen', o.get('user') == 'Ann' and not o.get('login') and o.get('app') == k, RES)
        ok(k + ' 2. ...filling the page, no title bar or taskbar', o.get('r') and o['r'][:2] == [0, 0] and o['r'][2:] == o.get('vw') and not o.get('tb') and not o.get('bar'), o)
        ok(k + " 3. ...the page's title is the window's", o.get('title') and o.get('title') == o.get('wt'), o)
        if k == 'notepad': ok(k + ' 4. the home folder opened with the keys', o.get('home') == 'written before', o)
        ok(k + ' 5. closed cleanly', isinstance(RES.get('closed'), dict) and not RES['closed'].get('main'), RES)
srv.shutdown()
print('%d/%d passed' % (N[1], N[0])); sys.exit(0 if N[0] == N[1] else 1)
