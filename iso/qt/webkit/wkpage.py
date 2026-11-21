# 1.6.1: the real desktop page (iso/www) in rudolf-webkit on a virtual screen, for the Lite engine check's tests of the page
# (lock61_ci.py, dynbg61_ci.py): a stand-in rudolfd serves the page with a test script added at the top of <head> (it posts
# its results to /result), answers /api/ (the test's own answers first), and serves the page's files; rudolf-webkit runs it.
import http.server, json, os, subprocess, tempfile, threading, time, urllib.parse
WWW = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../www')
CT = {'.js': 'text/javascript', '.webp': 'image/webp', '.jpg': 'image/jpeg', '.png': 'image/png', '.woff2': 'font/woff2', '.woff': 'font/woff',
      '.wasm': 'application/wasm', '.svg': 'image/svg+xml', '.json': 'application/json', '.css': 'text/css'}
def base_api(a, q):
    if a == 'idle': return {'ok': True, 'idle': 0}
    if a == 'media': return {'ok': True, 'playing': 0}
    if a == 'apps/windows': return {'ok': True, 'windows': [], 'mode': 'desktop'}
    if a == 'info': return {'ok': True, 'backlight': False, 'live': 0, 'boot': 'uefi', 'kernel': 'test'}
    if a == 'events': return {'ok': True, 'events': ''}
    if a == 'volume': return {'ok': True, 'volume': 50, 'muted': False}
    return {'ok': True}
class Page:
    """serve(test_script, port, api=None) -> Page; .res (results), .run(bin, draw) starts rudolf-webkit; .stop()"""
    def __init__(s, script, port, api=None):
        s.res = {}; s.errors = []; s.p = None; P = s
        class H(http.server.BaseHTTPRequestHandler):
            def log_message(self, *a): pass
            def send(self, code, body, ctype='application/json'):
                b = body.encode() if isinstance(body, str) else body
                self.send_response(code); self.send_header('content-type', ctype); self.send_header('content-length', str(len(b))); self.end_headers(); self.wfile.write(b)
            def route(self):
                u, _, q = self.path.partition('?'); u = urllib.parse.unquote(u)
                if u in ('/', '/index.html'):
                    return self.send(200, open(os.path.join(WWW, 'index.html'), encoding='utf-8').read().replace('<head>', '<head>' + script, 1), 'text/html; charset=utf-8')
                if u.startswith('/api/'):
                    a = u[5:]
                    if a == 'events/wait': time.sleep(1.5); a = 'events'
                    r = api(a, q) if api else None
                    return self.send(200, json.dumps(r if r is not None else base_api(a, q)))
                f = os.path.normpath(os.path.join(WWW, u.lstrip('/')))
                if f.startswith(os.path.normpath(WWW)) and os.path.isfile(f):
                    return self.send(200, open(f, 'rb').read(), CT.get(os.path.splitext(f)[1], 'application/octet-stream'))
                self.send(404, 'not found', 'text/plain')
            def do_GET(self): self.route()
            def do_POST(self):
                n = int(self.headers.get('content-length') or 0); body = self.rfile.read(n)
                if self.path == '/result':
                    m = json.loads(body); P.res[m['k']] = m['v']
                    if m['k'] == 'pageerror': P.errors.append(m['v'])
                    return self.send(200, '{"ok":true}')
                self.route()
        class S(http.server.ThreadingHTTPServer):
            daemon_threads = True
            def handle_error(self, *a): pass
        s.port = port; s.srv = S(('127.0.0.1', port), H); threading.Thread(target=s.srv.serve_forever, daemon=True).start()
    def run(s, binary, draw='soft', size='1280x800'):
        s.home = tempfile.mkdtemp(prefix='wkpage-')
        s.p = subprocess.Popen([binary, '--size=' + size, '--draw=' + draw, 'http://127.0.0.1:%d/' % s.port], env=dict(os.environ, HOME=s.home),
                               stdout=open(os.path.join(s.home, 'wk.log'), 'w'), stderr=subprocess.STDOUT)
        return s.p
    def stop(s):
        if s.p:
            s.p.terminate()
            try: s.p.wait(5)
            except Exception: s.p.kill()
        s.srv.shutdown()
def shot(name): subprocess.run(['import', '-window', 'root', name]); return name
def px(f, x, y):
    try:
        o = subprocess.run(['convert', f, '-crop', '1x1+%d+%d' % (x, y), '-depth', '8', 'txt:-'], capture_output=True, text=True).stdout
        h = o.split('#', 2)[-1][:6] if '#' in o else ''
        return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)) if len(h) == 6 else None
    except Exception: return None
def changed(a, b, box):
    """how many pixels differ between two screenshots in box (x, y, w, h) (ImageMagick's compare, absolute error count)"""
    x, y, w, h = box; crop = f'{w}x{h}+{x}+{y}'
    r = subprocess.run(['compare', '-metric', 'AE', '-fuzz', '3%', a + '[' + crop + ']', b + '[' + crop + ']', 'null:'], capture_output=True, text=True)
    try: return int(float(r.stderr.split()[0]))
    except Exception: return -1
class Checks:
    def __init__(s): s.n = 0; s.ok = 0
    def __call__(s, name, c, d=''):
        s.n += 1; s.ok += bool(c); print(('PASS ' if c else 'FAIL ') + name + ('' if c else ' - ' + str(d)[:400]), flush=True)
    def end(s):
        print('%d/%d passed' % (s.ok, s.n)); return 0 if s.n == s.ok else 1
# the page's side: post(k, v), wait(ms), until(f, ms); errors posted as pageerror
JS_HEAD = r'''window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0,engine:"webkit"};
const post=(k,v)=>fetch('/result',{method:'POST',body:JSON.stringify({k,v})});const wait=t=>new Promise(r=>setTimeout(r,t));
const until=(f,t)=>new Promise((res,rej)=>{const t0=Date.now(),iv=setInterval(()=>{let r;try{r=f()}catch(e){}if(r){clearInterval(iv);res(r)}else if(Date.now()-t0>t){clearInterval(iv);rej(new Error('timeout'))}},200)});
addEventListener('error',e=>post('pageerror',String(e.message)));'''
