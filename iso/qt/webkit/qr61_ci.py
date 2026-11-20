# 1.6.1 (docs/plan-1.6.1.md Part 18): the authenticator app's setup QR code in WebKitGTK, as qr61_t.py checks it in
# Chromium (Standard). Lite shows the whole desktop page in rudolf-webkit; Native shows Settings as the page in one-app
# mode (#app=settings) in the same WebKit, so both are run here: User Settings › Sign-in options › Authenticator app ›
# Set up…, in the light and the dark theme; the screen is grabbed (ImageMagick's import) and the code read back with
# OpenCV's cv2.QRCodeDetector (and zbarimg, if installed) to the exact link the dialog shows; its quiet zone and contrast
# measured.   python3 qr61_ci.py ./rudolf-webkit   (DISPLAY set, 1280x800 or larger)
import http.server, json, os, shutil, subprocess, sys, tempfile, threading, time
import numpy as np, cv2
BIN = sys.argv[1]; WWW = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../www'); PORT = 8612
RES = {}; SESSION = {}
REPORT = r'''<script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};
window.__E=[];addEventListener('error',e=>__E.push(e.message));addEventListener('unhandledrejection',e=>__E.push(String(e.reason)));window.__rudolfLsFlush=()=>fetch('/api/lsync',{method:'POST',headers:{'X-Rudolf-Token':'t'},body:'{}'});
(function(){const post=(k,v)=>fetch('/result',{method:'POST',body:JSON.stringify({k,v})}),theme=new URLSearchParams(location.search).get('theme')||'light';
 const until=(f,t)=>new Promise((res,rej)=>{const t0=Date.now(),iv=setInterval(()=>{let r;try{r=f()}catch(e){}if(r){clearInterval(iv);res(r)}else if(Date.now()-t0>t){clearInterval(iv);rej(new Error('timed out'))}},100)});
 addEventListener('load',async()=>{try{await until(()=>typeof otpSetup=='function'&&typeof acctNew=='function'&&typeof qrMake=='function',30000);
  if(!ONEAPP){svj('bd95setup',1);['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
   const a=await acctNew('Ann','pw-one','admin');ACCTS=[a];window.__sys=1;saveUser();USER=a;await userLogin(a,'pw-one');fsSave();fsFlush();await new Promise(r=>setTimeout(r,1500));
   const hex=u=>[...new Uint8Array(u)].map(x=>x.toString(16).padStart(2,'0')).join('');
   await post('setup',{id:a.id,pk:hex(await pbkBits('pw-one',a.ks,256)),fk:hex(FKRAW)})}
  else await until(()=>USER&&document.querySelector('.win.oamain'),25000);
  if(!ONEAPP)settings('usr');
  await until(()=>{if(ONEAPP&&SW&&SW.sec!='usr')SW.show('usr');return document.querySelector('.otpa')},15000);S.dm=theme=='dark'?'dark':'light';themeApply();document.querySelector('.otpa').click();
  await until(()=>document.querySelector('.otpset .otpqrs'),10000);await new Promise(r=>setTimeout(r,800));
  const s=document.querySelector('.otpset .otpqrs'),k=document.querySelector('.otpset .otpk'),r=s.getBoundingClientRect(),q=k.getBoundingClientRect();
  await post('shown',{link:document.querySelector('.otpset .otpu').textContent.replace(/^Or a link the app may open: /,''),x:r.x,y:r.y,w:r.width,h:r.height,n:s.viewBox.baseVal.width,
   kx:q.x,ky:q.y,kh:q.height,dpr:devicePixelRatio,dark:document.documentElement.classList.contains('dark'),oneapp:!!ONEAPP,user:USER&&USER.n})}catch(e){post('error',String(e&&e.message||e)+' user='+(typeof USER!='undefined'&&USER&&USER.n)+' main='+!!document.querySelector('.win.oamain')+' otpa='+!!document.querySelector('.otpa')+' errs='+(window.__E||[]).join('|')+' SW='+(typeof SW!='undefined'&&SW&&SW.sec)+' main='+document.querySelector('.win.oamain').textContent.slice(-600))}})})();</script>'''
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
    def handle_error(self, *a): pass
srv = S(('127.0.0.1', PORT), H); threading.Thread(target=srv.serve_forever, daemon=True).start()
HOME = tempfile.mkdtemp(prefix='qr61-'); ZBAR = shutil.which('zbarimg')
N = [0, 0]
def ok(name, c, d=''):
    N[0] += 1; N[1] += bool(c); print(('PASS ' if c else 'FAIL ') + name + ('' if c else ' - ' + str(d)[:400])); sys.stdout.flush()
def decode(img):
    t = ''
    for det in (cv2.QRCodeDetector(), cv2.QRCodeDetectorAruco()):
        for f in (1, 1.5, 2):
            t, _, _ = det.detectAndDecode(img if f == 1 else cv2.resize(img, None, fx=f, fy=f, interpolation=cv2.INTER_NEAREST))
            if t: break
        if t: break
    z = t
    if ZBAR:
        pth = os.path.join(HOME, 'z.png'); cv2.imwrite(pth, img); r = subprocess.run([ZBAR, '-q', '--raw', '-Sbinary', pth], capture_output=True)
        z = r.stdout.decode('utf-8', 'replace').rstrip('\n') if r.returncode == 0 else None
    return t, z
def lum(rgb):
    v = rgb / 255.0; v = np.where(v <= 0.03928, v / 12.92, ((v + 0.055) / 1.055) ** 2.4)
    return 0.2126 * v[..., 0] + 0.7152 * v[..., 1] + 0.0722 * v[..., 2]
def run(url, want, secs=70):
    RES.clear(); shot = None
    p = subprocess.Popen([BIN, '--size=1200x780', '--draw=soft', 'http://127.0.0.1:%d/%s' % (PORT, url)], env=dict(os.environ, HOME=HOME),
                         stdout=open(os.path.join(HOME, 'wk.log'), 'a'), stderr=subprocess.STDOUT)
    t0 = time.time()
    while time.time() - t0 < secs and not any(k in RES for k in want + ['error']): time.sleep(0.3)
    if 'shown' in RES:
        time.sleep(1); f = os.path.join(HOME, 'shot.png'); subprocess.run(['import', '-window', 'root', f], capture_output=True)
        shot = cv2.imread(f, cv2.IMREAD_COLOR)
    p.terminate()
    try: p.wait(5)
    except Exception: p.kill()
    return shot
print('decoders: OpenCV %s cv2.QRCodeDetector%s' % (cv2.__version__, ', zbarimg' if ZBAR else ''))
i = 1
# Lite first (it makes the account; its keys then sign in Native's one-app Settings, as rudolfd's session does), then
# Native, then Lite's dark theme (it makes the account again, so it comes last)
for url, what in [('', 'Lite (the whole page)'), ('#app=settings', 'Native (Settings in one-app mode)'),
                  ('?theme=dark#app=settings', 'Native, dark theme'), ('?theme=dark', 'Lite, dark theme')]:
    img = run(url, ['shown']); o = RES.get('shown') or {}
    if i == 1:
        ok('0. an account made and signed in once in WebKit', isinstance(RES.get('setup'), dict) and len(RES['setup'].get('fk') or '') == 64, RES)
        if isinstance(RES.get('setup'), dict):
            st = RES['setup']; SESSION.update({'ok': True, 'id': st['id'], 'n': 'Ann', 'r': 'admin', 'pk': st['pk'], 'fk': st['fk']})
    if img is None or not o:
        ok('%d. %s: the dialog with the QR code' % (i, what), False, RES); i += 1; continue
    # where the code is on the screen: the window's position found from the dialog's own picture (the white square)
    d = o['dpr'] or 1; w = int(o['w'] * d); pad = int(24 * d)
    g = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY); x0 = y0 = None
    # rudolf-webkit's window is placed by the X server at 0,0 on a bare Xvfb (no window manager)
    x0, y0 = int(o['x'] * d), int(o['y'] * d)
    crop = img[max(0, y0 - pad):y0 + w + pad, max(0, x0 - pad):x0 + w + pad]
    t, z = decode(crop)
    ok('%d. %s: the code beside the setup key, read from the screen to the exact link shown (%s)' % (i, what, 'OpenCV and zbarimg' if ZBAR else 'OpenCV'),
       t == o['link'] and z == o['link'] and o['x'] + o['w'] <= o['kx'] + 1 and o['link'].startswith('otpauth://totp/Rudolf%20OS%3AAnn?secret=') and
       (o['dark'] == ('dark' in url)) and (o['oneapp'] == ('app=' in url)), (t, z, o))
    inner = cv2.cvtColor(img[y0:y0 + w, x0:x0 + w], cv2.COLOR_BGR2RGB).astype(float); mpx = w / o['n']; q4 = max(1, int(4 * mpx) - 1)
    ring = np.concatenate([inner[:q4].reshape(-1, 3), inner[-q4:].reshape(-1, 3), inner[:, :q4].reshape(-1, 3), inner[:, -q4:].reshape(-1, 3)])
    L = lum(inner); lo, hi = L.min(), L.max()
    ok('%d. ...its quiet zone 4 modules of white, contrast %.1f:1' % (i, (hi + .05) / (lo + .05)), ring.min() >= 250 and (hi + .05) / (lo + .05) >= 15, (ring.min(), lo, hi))
    i += 1
srv.shutdown(); shutil.rmtree(HOME, ignore_errors=True)
print('%d/%d passed' % (N[1], N[0])); sys.exit(0 if N[0] == N[1] else 1)
