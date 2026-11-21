# 1.6.1 (docs/plan-1.6.1.md Part 22): File Explorer's selection box and files dragged between two windows in WebKitGTK (the
# engine of Lite and Native), with real mouse input (xdotool) on a virtual screen, as xdrag161_t.js checks them in Chromium.
# A stand-in rudolfd serves the real page (iso/www) with a small reporter added, and keeps a made-up Games folder in memory.
# 1.6.1 Part 28: and files dragged from a window onto the desktop (Lite: the one page), Ctrl+drag, the desktop's Paste.
# Port 8612.   python3 xdrag61_ci.py ./rudolf-webkit   (DISPLAY set)
import hashlib, http.server, json, os, subprocess, sys, threading, time, urllib.parse
BIN = sys.argv[1]; WWW = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../www'); PORT = 8612
RES = {}; F = {}; CALLS = []
def mkdirs(p):
    while p and p != '/' and p not in F: F[p] = None; p = p.rsplit('/', 1)[0]
def put(p, b): mkdirs(p.rsplit('/', 1)[0]); F[p] = b
mkdirs('/home/rudolf/Games/DC'); put('/home/rudolf/Games/DC/manual.pdf', b'%PDF-1.4 made up for the test\n' * 20); mkdirs('/media')
kids = lambda d: [k for k in F if k.rsplit('/', 1)[0] == d and k != d]
REPORT = r'''<script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};
(function(){const post=(k,v)=>fetch('/result',{method:'POST',body:JSON.stringify({k,v})});window.__post=post;
 const until=(f,t)=>new Promise((res,rej)=>{const t0=Date.now(),iv=setInterval(()=>{let r;try{r=f()}catch(e){}if(r){clearInterval(iv);res(r)}else if(Date.now()-t0>t){clearInterval(iv);rej(new Error('timeout'))}},200)});
 const rc=e=>{if(!e)return null;const r=e.getBoundingClientRect();return [Math.round(r.left),Math.round(r.top),Math.round(r.width),Math.round(r.height)]};
 addEventListener('load',async()=>{try{await until(()=>typeof explorer=='function'&&typeof xfRun=='function'&&typeof acctNew=='function'&&FSLOADED,40000);
  svj('bd95setup',1);['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
  const a=await acctNew('Ann','pw-one','admin');ACCTS=[a];window.__sys=1;saveUser();USER=a;await userLogin(a,'pw-one');window.elevate=async()=>true;
  const D=node(downloadsPath());Object.keys(D).forEach(k=>delete D[k]);for(let i=1;i<=5;i++)D['PIC-'+i+'.TXT']='picture '+i;D['NOTE.TXT']='a note';fsSave();
  wins.slice().forEach(w=>w.close());const A=explorer(downloadsPath());A.el.style.cssText='left:0;top:0;width:620px;height:520px';
  const B=explorer(downloadsPath());B.el.style.cssText='left:640px;top:0;width:620px;height:520px';B.openRemote({dev:'games',root:'/home/rudolf/Games',path:'/home/rudolf/Games/DC',label:'Games'});
  window.WA=A;window.WB=B;await new Promise(r=>setTimeout(r,1500));
  const dk=downloadsPath().join('/'),q=(w,k)=>w.body.querySelector('[data-k="'+k+'"]');
  post('rects',{desk:rc(DESK),pic2:rc(q(A,dk+'/PIC-2.TXT')),pic3:rc(q(A,dk+'/PIC-3.TXT')),pic4:rc(q(A,dk+'/PIC-4.TXT')),A:rc(A.body.querySelector('.xc')),B:rc(B.body.querySelector('.xc')),pic1:rc(q(A,dk+'/PIC-1.TXT')),pic5:rc(q(A,dk+'/PIC-5.TXT')),note:rc(q(A,dk+'/NOTE.TXT')),pdf:rc(q(B,'R:manual.pdf'))});
  /* what is selected, what Downloads holds, a progress window, every 150 ms */
  setInterval(()=>{const xw=wins.find(w=>w.xfer);post('state',{sel:[...A.body.querySelectorAll('.xc [data-k].s')].map(e=>e.dataset.k.split('/').pop()),marq:!!A.body.querySelector('.xmarq'),
    dl:Object.keys(node(downloadsPath())||{}),desk:Object.keys(deskFiles()||{}),rows:Object.fromEntries([...A.body.querySelectorAll('.xc [data-k]')].map(e=>[e.dataset.k.split('/').pop(),rc(e)])),menu:[...document.querySelectorAll('.ctx .cmi')].map(x=>x.textContent).join('|'),cells:Object.fromEntries(Object.keys(deskFiles()||{}).map(n=>[n,cells['file:'+n]||null])),paste:(()=>{const m=[...document.querySelectorAll('.ctx .cmi')].find(x=>x.querySelector('span').textContent=='Paste');return m?rc(m).concat([m.classList.contains('dis')?1:0]):null})(),xf:xw?xw.body.querySelector('.xft').textContent:null,xfs:XFSEEN})},150);
  window.XFSEEN=[];new MutationObserver(()=>{wins.filter(w=>w.xfer).forEach(w=>{const t=w.body.querySelector('.xft').textContent;if(!XFSEEN.includes(t))XFSEEN.push(t)})}).observe(document.body,{subtree:true,childList:true,characterData:true});
  /* what WebKit tells a drop: its modifier keys and drag action (Ctrl+drag) */
  window.DROPS=[];['dragover','drop'].forEach(t=>document.addEventListener(t,e=>{const o={t,ctrl:e.ctrlKey,fx:e.dataTransfer&&e.dataTransfer.dropEffect,ea:e.dataTransfer&&e.dataTransfer.effectAllowed,xk:typeof XFK=='object'?XFK.ctrl:null};
   if(t=='drop'||!DROPS.length||JSON.stringify(DROPS.at(-1))!=JSON.stringify(o))DROPS.push(o);if(DROPS.length>40)DROPS.splice(0,DROPS.length-40)},true));
  document.addEventListener('keydown',e=>{if(e.key=='Control')DROPS.push({t:'keydown ctrl'})},true);
  setInterval(()=>post('drops',DROPS),500);
  post('ready',true)}catch(e){post('error',String(e&&e.message||e))}})})();</script>'''
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def send(self, code, body, ctype='application/json'):
        b = body.encode() if isinstance(body, str) else body
        self.send_response(code); self.send_header('content-type', ctype); self.send_header('content-length', str(len(b))); self.end_headers(); self.wfile.write(b)
    def j(self, o, code=200): o.setdefault('ok', code < 300); return self.send(code, json.dumps(o))
    def api(self, a, q, body):
        p = q.get('path', [''])[0]
        if a.startswith('fs/'): CALLS.append(a + ' ' + (p or q.get('from', [''])[0] + ' > ' + q.get('to', [''])[0]))
        if a == 'disks': return self.j({'disks': []})
        if a == 'fs/list':
            if p not in F or F[p] is not None: return self.j({'error': 'no such folder'}, 404)
            return self.j({'items': [{'name': k.rsplit('/', 1)[1], 'dir': F[k] is None, 'size': 0 if F[k] is None else len(F[k]), 'mtime': 1.7e12} for k in kids(p)]})
        if a == 'fs/tree':
            items = []
            def walk(d):
                for k in kids(d): items.append({'p': k[len(p) + 1:], 'dir': F[k] is None, 'size': 0 if F[k] is None else len(F[k]), 'mtime': 1.7e12}); F[k] is None and walk(k)
            walk(p); return self.j({'items': items})
        if a == 'fs/read':
            if F.get(p) is None: return self.j({'error': 'not found'}, 404)
            return self.send(200, F[p], 'application/octet-stream')
        if a == 'fs/hash':
            if F.get(p) is None: return self.j({'error': 'not found'}, 404)
            return self.j({'sha256': hashlib.sha256(F[p]).hexdigest(), 'size': len(F[p])})
        if a == 'fs/write':
            if p.rsplit('/', 1)[0] not in F: return self.j({'error': 'cannot write here'}, 500)
            put(p, body); return self.j({'message': 'saved', 'size': len(body)})
        if a == 'fs/mkdir': mkdirs(p); return self.j({'message': 'created'})
        if a == 'fs/delete':
            if p not in F: return self.j({'error': 'not found'}, 404)
            del F[p]; return self.j({'message': 'deleted'})
        if a == 'desktop/session': return self.j({'error': 'none'}, 404)
        return self.j({})
    def do_GET(self):
        u = urllib.parse.urlparse(self.path); q = urllib.parse.parse_qs(u.query)
        if u.path in ('/', '/index.html'):
            return self.send(200, open(os.path.join(WWW, 'index.html'), encoding='utf-8').read().replace('<head>', '<head>' + REPORT, 1), 'text/html; charset=utf-8')
        if u.path == '/api/events/wait': time.sleep(5); return self.j({'events': []})
        if u.path.startswith('/api/'): return self.api(u.path[5:], q, b'')
        f = os.path.normpath(os.path.join(WWW, u.path.lstrip('/')))
        if f.startswith(os.path.normpath(WWW)) and os.path.isfile(f):
            ct = {'.js': 'text/javascript', '.webp': 'image/webp', '.jpg': 'image/jpeg', '.woff2': 'font/woff2', '.woff': 'font/woff', '.wasm': 'application/wasm'}.get(os.path.splitext(f)[1], 'application/octet-stream')
            return self.send(200, open(f, 'rb').read(), ct)
        self.send(404, 'not found', 'text/plain')
    def do_POST(self):
        u = urllib.parse.urlparse(self.path); q = urllib.parse.parse_qs(u.query)
        n = int(self.headers.get('content-length') or 0); body = self.rfile.read(n)
        if u.path == '/result':
            m = json.loads(body); RES[m['k']] = m['v']; return self.send(200, '{"ok":true}')
        if u.path.startswith('/api/'): return self.api(u.path[5:], q, body)
        self.send(404, 'no', 'text/plain')
class S(http.server.ThreadingHTTPServer):
    daemon_threads = True
srv = S(('127.0.0.1', PORT), H); threading.Thread(target=srv.serve_forever, daemon=True).start()
home = os.path.join(os.environ.get('RUNNER_TEMP', '/tmp'), 'xdrag61-home'); os.makedirs(home, exist_ok=True)
log = open('xdrag61.txt', 'w')
p = subprocess.Popen([BIN, '--rudolf-kiosk', '--size=1280x800', f'http://127.0.0.1:{PORT}/'], stdout=log, stderr=subprocess.STDOUT, env=dict(os.environ, HOME=home))
def until(f, t=20):
    end = time.time() + t
    while time.time() < end:
        try:
            if f(): return True
        except Exception: pass
        time.sleep(0.15)
    return False
X = lambda *a: subprocess.run(['xdotool', *map(str, a)])
def drag(x0, y0, x1, y1, steps=14, ctrl=False, mid=None):
    X('mousemove', x0, y0); time.sleep(0.2)
    if ctrl == 'start': X('keydown', 'ctrl'); time.sleep(0.15)
    X('mousedown', '1'); time.sleep(0.15)
    for i in range(1, steps + 1):
        X('mousemove', int(x0 + (x1 - x0) * i / steps), int(y0 + (y1 - y0) * i / steps)); time.sleep(0.05)
        if i == steps - 2 and mid: time.sleep(0.4); mid()
    if ctrl == 'mid': X('keydown', 'ctrl'); time.sleep(0.1)
    X('mousemove', x1 + 1, y1 + 1); time.sleep(0.3); X('mouseup', '1'); time.sleep(0.2)
    if ctrl: X('keyup', 'ctrl')
ready = until(lambda: RES.get('ready') is True or 'error' in RES, 60); time.sleep(1)
R = RES.get('rects') or {}; st = lambda: RES.get('state') or {}
checks = [('the page set up two File Explorer windows (Downloads; the Games drive)', bool(ready) and 'error' not in RES and R.get('A') and R.get('pdf'))]
if checks[0][1]:
    A, B, pic1, pic5, note, pdf = R['A'], R['B'], R['pic1'], R['pic5'], R['note'], R['pdf']
    # 1. the selection box: from the empty space below the rows, up over them
    seen = {}
    def snap(): time.sleep(0.4); seen.update(st())
    X('mousemove', A[0] + A[2] - 30, A[1] + A[3] - 30, 'click', '1'); time.sleep(0.5)   # (the window has the pointer and the focus first)
    for attempt in range(2):
        drag(A[0] + A[2] - 30, A[1] + A[3] - 30, pic1[0] + 60, pic1[1] + 4, mid=snap); time.sleep(0.6)
        if seen.get('marq'): break
        print('(the box: tried again: the first press did not reach the page)', json.dumps(seen)); seen.clear()
    after = st()
    checks.append(('a box pressed on an empty spot and dragged: drawn, the items it touches selected while dragging (WebKit, real mouse)', seen.get('marq') and len(seen.get('sel') or []) >= 3))
    checks.append(('...the selection stays after the button is let go, the box gone', len(after.get('sel') or []) >= 5 and not after.get('marq')))
    X('mousemove', A[0] + A[2] - 30, A[1] + A[3] - 30, 'click', '1'); time.sleep(0.6)
    checks.append(('a plain click on an empty spot clears it', not (st().get('sel') or [])))
    # 2. a file dragged from Downloads into the Games window: moved (checked first)
    X('mousemove', note[0] + 40, note[1] + note[3] // 2, 'click', '1'); time.sleep(0.4)
    drag(note[0] + 40, note[1] + note[3] // 2, B[0] + B[2] // 2, B[1] + B[3] - 40); until(lambda: '/home/rudolf/Games/DC/note.txt' in F and 'NOTE.TXT' not in (st().get('dl') or []), 15); time.sleep(0.8)
    checks.append(('a file dragged from Downloads into the Games window: on the drive, gone from Downloads (checked with fs/hash first)', '/home/rudolf/Games/DC/note.txt' in F and 'NOTE.TXT' not in (st().get('dl') or []) and any(c.startswith('fs/hash') for c in CALLS)))
    checks.append(('...the progress window came ("Moving 1 item")', 'Moving 1 item' in (st().get('xfs') or [])))
    # 3. a drive's file dragged into Downloads with Ctrl: copied (the drive keeps it)
    time.sleep(2.2)
    for attempt in range(2):
        drag(pdf[0] + 40, pdf[1] + pdf[3] // 2, A[0] + A[2] // 2, A[1] + A[3] - 40, ctrl='start'); X('keyup', 'ctrl'); until(lambda: 'MANUAL.PDF' in (st().get('dl') or []), 10); time.sleep(0.8)
        if 'MANUAL.PDF' in (st().get('dl') or []): break
        print('(Ctrl+drag: tried again: nothing arrived)')
    print('drops (Ctrl held from the start)', json.dumps((RES.get('drops') or [])[-8:]))
    checks.append(('a drive\'s file dragged into Downloads with Ctrl held: copied, the drive keeps it', 'MANUAL.PDF' in (st().get('dl') or []) and '/home/rudolf/Games/DC/manual.pdf' in F))
    # (and Ctrl pressed only once the drag is under way: what WebKit tells the drop, printed for the record)
    if '/home/rudolf/Games/DC/manual.pdf' in F:
        RES['drops'] = []; time.sleep(2.2); r2 = R.get('pdf')
        drag(pdf[0] + 40, pdf[1] + pdf[3] // 2, A[0] + A[2] // 2, A[1] + A[3] - 60, ctrl='mid'); time.sleep(2.5)
        print('Ctrl pressed during the drag: the drive', 'kept it (copied)' if '/home/rudolf/Games/DC/manual.pdf' in F else 'lost it (moved)', 'drops', json.dumps((RES.get('drops') or [])[-8:]))
    # 4. (Part 28) a file dragged from the window onto the desktop's empty area (below the windows): moved, its icon in that cell
    dk = R.get('desk'); rw = lambda n: (st().get('rows') or {}).get(n); pic2 = rw('PIC-2.TXT')
    if dk and pic2:
        cell = lambda x, y: [min((x - dk[0] - 8) // 84, (dk[2] - 8) // 84 - 1), min((y - dk[1] - 8) // 86, (dk[3] - 8) // 86 - 1)]   # (gridInfo: the last row and column take the rest)
        time.sleep(2.2); X('mousemove', pic2[0] + 40, pic2[1] + pic2[3] // 2, 'click', '1'); time.sleep(0.4)
        drag(pic2[0] + 40, pic2[1] + pic2[3] // 2, 1000, 640); until(lambda: 'PIC-2.TXT' in (st().get('desk') or []), 10); time.sleep(0.8)
        checks.append(('Lite: a file dragged from the window onto the desktop: in the Desktop folder, gone from Downloads, its icon in the cell under the pointer',
                       'PIC-2.TXT' in (st().get('desk') or []) and 'PIC-2.TXT' not in (st().get('dl') or []) and (st().get('cells') or {}).get('PIC-2.TXT') == cell(1000, 640)))
        time.sleep(2.2)
        for attempt in range(2):
            pic3 = rw('PIC-3.TXT'); drag(pic3[0] + 40, pic3[1] + pic3[3] // 2, 1160, 640, ctrl='start'); X('keyup', 'ctrl'); until(lambda: 'PIC-3.TXT' in (st().get('desk') or []), 10); time.sleep(0.8)
            if 'PIC-3.TXT' in (st().get('desk') or []): break
            print('(Ctrl+drag onto the desktop: tried again)')
        checks.append(('Lite: Ctrl+drag onto the desktop: copied (Downloads keeps it)', 'PIC-3.TXT' in (st().get('desk') or []) and 'PIC-3.TXT' in (st().get('dl') or [])))
        # 5. Copy (Ctrl+C in the list), the desktop's right-click menu: Paste
        time.sleep(2.2); pic4 = rw('PIC-4.TXT'); X('mousemove', pic4[0] + 40, pic4[1] + pic4[3] // 2, 'click', '1'); time.sleep(0.4); X('key', 'ctrl+c'); time.sleep(0.5)
        X('mousemove', 840, 700, 'click', '3'); until(lambda: st().get('paste'), 5); pm = st().get('paste')
        if pm and not pm[4]: time.sleep(0.5); X('mousemove', pm[0] + 20, pm[1] + pm[3] // 2); time.sleep(0.3); X('click', '1'); until(lambda: 'PIC-4.TXT' in (st().get('desk') or []), 8); time.sleep(0.6)
        checks.append(('Lite: Copy in File Explorer, the desktop\'s Paste: copied onto the desktop, its icon where the menu was opened', bool(pm) and not pm[4] and 'PIC-4.TXT' in (st().get('desk') or []) and 'PIC-4.TXT' in (st().get('dl') or []) and (st().get('cells') or {}).get('PIC-4.TXT') == cell(840, 700)))
checks.append(('rudolf-webkit still runs', p.poll() is None))
p.terminate(); log.close(); srv.shutdown()
ok = 0
for n, c in checks:
    ok += bool(c); print(('PASS ' if c else 'FAIL ') + n)
print('state', json.dumps(RES.get('state')), 'error', RES.get('error'), 'calls', CALLS[-12:])
print(f'{ok}/{len(checks)} passed'); sys.exit(0 if ok == len(checks) else 1)
