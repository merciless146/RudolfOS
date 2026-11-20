#!/usr/bin/env python3
# 1.6.1 (docs/plan-1.6.1.md Part 18): the authenticator app's setup QR code (desktop/qr61.js), decoded back.
#  1. the encoder alone (node): links and texts of many lengths, levels M and Q, versions 1 to 10, each of the 8 masks
#     forced and the chosen one, the largest text each version holds; every code decoded back to the exact text by
#     OpenCV's cv2.QRCodeDetector (and by zbarimg, when it is installed: both must agree).
#  2. the page (Chromium, the built iso/www/index.html with a stand-in rudolfd): User Settings › Sign-in options ›
#     Authenticator app › Set up…: the QR code beside the setup key, read from a screenshot back to the exact otpauth://
#     link the dialog shows, in the light, dark and high-contrast themes; its quiet zone (4 modules, white) and its
#     contrast (dark #000 on white, 21:1) measured in the screenshot.
#   python3 qr61_t.py [CHROME]     (build iso/www first; needs node, numpy, cv2, Python's playwright)
import json, os, random, shutil, subprocess, sys, tempfile, threading, http.server, functools
import numpy as np, cv2
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.abspath(os.path.join(HERE, '../../..'))
QR = os.path.join(ROOT, 'desktop/qr61.js'); WWW = os.path.join(ROOT, 'iso/www')
R = []
def ok(n, c, d=None):
    R.append(bool(c)); print(('PASS ' if c else 'FAIL ') + n + ('' if c or d is None else ' - ' + str(d)[:400]), flush=True)
ZBAR = shutil.which('zbarimg'); DET = cv2.QRCodeDetector()
print('decoders: OpenCV %s cv2.QRCodeDetector%s' % (cv2.__version__, ', zbarimg ' + ZBAR if ZBAR else ' (zbarimg not installed)'))
TMP = tempfile.mkdtemp(prefix='qr61_')
def zbar(img):
    p = os.path.join(TMP, 'z.png'); cv2.imwrite(p, img)
    r = subprocess.run([ZBAR, '-q', '--raw', '-Sbinary', p], capture_output=True)
    return r.stdout.decode('utf-8', 'replace').rstrip('\n') if r.returncode == 0 else None
RETRY = [0]
def decode(img):
    # OpenCV's classic detector misses some codes at some sizes (it finds no finder patterns): then its ArUco-based
    # detector, and other sizes, are tried; anything it reads must still be the exact text, and zbarimg must agree.
    t = ''
    for det in (DET, cv2.QRCodeDetectorAruco()):
        for f in (1, 1.5, 0.75, 2):
            im = img if f == 1 else cv2.resize(img, None, fx=f, fy=f, interpolation=cv2.INTER_NEAREST)
            t, _, _ = det.detectAndDecode(im)
            if t: break
        if t: break
    if t and (det is not DET or f != 1): RETRY[0] += 1
    z = zbar(img) if ZBAR else t
    return t, z
def render(mod, scale=8):
    m = np.array(mod, dtype=np.uint8); img = np.pad((1 - m) * 255, 4, constant_values=255).astype(np.uint8)
    return cv2.resize(img, None, fx=scale, fy=scale, interpolation=cv2.INTER_NEAREST)

# ---- 1. the encoder alone ----
CAP = {'M': [14, 26, 42, 62, 84, 106, 122, 152, 180, 213], 'Q': [11, 20, 32, 46, 60, 74, 86, 108, 130, 151]}  # bytes, from the standard's table
random.seed(61)
AL = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ234567'
def link(name, key): return 'otpauth://totp/' + name + '?secret=' + key + '&issuer=Rudolf%20OS'
cases = []
for name in ['Rudolf%20OS%3Ajo', 'Rudolf%20OS%3Ajohn', 'Rudolf%20OS%3AAlexandra%20Montgomery-Smith', 'Rudolf%20OS%3A' + 'x' * 60]:
    key = ''.join(random.choice(AL[26:] + AL[:26].upper()) for _ in range(32))
    for e in 'MQ': cases.append((link(name, key), e, None))
for e in 'MQ':
    for v, c in enumerate(CAP[e]):  # the most each version holds, and one byte more (the next version)
        for L in (c, c + 1):
            if L <= CAP[e][-1]: cases.append((''.join(random.choice(AL + ':/?=&%') for _ in range(L)), e, None))
    for m in range(8): cases.append(('otpauth://totp/Rudolf%20OS%3Amask' + str(m) + '?secret=JBSWY3DPEHPK3PXP&issuer=Rudolf%20OS', e, m))
cases += [('A', 'M', None), ('héllo wörld ✓ Rudolf', 'Q', None), ('x' * (CAP['M'][-1] + 1), 'M', None), ('x' * (CAP['Q'][-1] + 1), 'Q', None)]
js = "const {qrMake}=require(%s);const t=JSON.parse(require('fs').readFileSync(0));console.log(JSON.stringify(t.map(([s,e,m])=>qrMake(s,e,m))))" % json.dumps(QR)
res = json.loads(subprocess.run(['node', '-e', js], input=json.dumps(cases), capture_output=True, text=True, check=True).stdout)
bad = []; seen = set(); capok = True
for (s, e, m), c in zip(cases, res):
    n = len(s.encode())
    if n > CAP[e][-1]:
        if c is not None: bad.append(('too long, not refused', e, n))
        continue
    want = next(i + 1 for i, k in enumerate(CAP[e]) if n <= k)
    if c is None or c['ver'] != want: capok = False; bad.append(('version', e, n, c and c['ver'], want)); continue
    if c['size'] != 17 + 4 * c['ver'] or (m is not None and c['mask'] != m): bad.append(('size/mask', e, n)); continue
    t, z = decode(render(c['mod']))
    seen.add((e, c['ver'], c['mask']))
    if t != s or z != s: bad.append(('decoded', e, n, c['ver'], c['mask'], t[:30], (z or '')[:30]))
ok('1. %d codes decoded back to the exact text (OpenCV%s; %d needed OpenCV\'s second try)' % (len(cases), ' and zbarimg' if ZBAR else '', RETRY[0]), not bad, bad)
ok('2. versions 1 to 10 at both levels, the version the standard\'s capacity table gives for each length', capok and
   all((e, v) in {(a, b) for a, b, _ in seen} for e in 'MQ' for v in range(1, 11)), sorted(seen))
ok('3. all 8 masks decode, and the chosen masks vary (the penalty decides)', {x for _, _, x in seen} == set(range(8)), sorted({x for _, _, x in seen}))
ok('4. longer than version 10 holds: null (the dialog then shows the link and key only)', not any(b[0].startswith('too long') for b in bad))
# the penalty: the chosen mask has the least penalty of the 8
js2 = ("const {qrMake,qrPenalty}=require(%s);const s='otpauth://totp/Rudolf%%20OS%%3Ajohn?secret=JBSWY3DPEHPK3PXPJBSWY3DPEHPK3PXP&issuer=Rudolf%%20OS';"
       "const c=qrMake(s,'M'),p=[...Array(8).keys()].map(m=>qrPenalty(qrMake(s,'M',m).mod));console.log(JSON.stringify({m:c.mask,p}))") % json.dumps(QR)
pp = json.loads(subprocess.run(['node', '-e', js2], capture_output=True, text=True, check=True).stdout)
ok('5. the chosen mask is the one with the lowest penalty', pp['p'][pp['m']] == min(pp['p']), pp)

# ---- 2. the page ----
from playwright.sync_api import sync_playwright
class H(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *a): pass
srv = http.server.ThreadingHTTPServer(('127.0.0.1', 8611), functools.partial(H, directory=WWW))
threading.Thread(target=srv.serve_forever, daemon=True).start()
INIT = """window.RUDOLFD={token:'x',base:'',boot:'uefi',media:'disk',live:0,recovery:0};const f0=window.fetch;
window.fetch=(u,o)=>{u=String(u);if(u.includes('/api/'))return Promise.resolve(new Response('{"ok":true}',{status:200,headers:{'content-type':'application/json'}}));return f0(u,o)}"""
def lum(rgb):
    v = rgb / 255.0; v = np.where(v <= 0.03928, v / 12.92, ((v + 0.055) / 1.055) ** 2.4)
    return 0.2126 * v[..., 0] + 0.7152 * v[..., 1] + 0.0722 * v[..., 2]
with sync_playwright() as pw:
    b = pw.chromium.launch(executable_path=sys.argv[1] if len(sys.argv) > 1 else None)
    p = b.new_page(viewport={'width': 1280, 'height': 900}); errs = []; p.on('pageerror', lambda e: errs.append(str(e)))
    p.add_init_script(INIT); p.goto('http://127.0.0.1:8611/index.html')
    p.wait_for_function("typeof otpSetup=='function'&&typeof qrMake=='function'&&IDB&&IDB.db", timeout=30000)
    p.evaluate("""(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
      ACCTS.length=0;ACCTS.push({n:'john',h:await pwMake('j1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'j1');window.elevate=async()=>true})()""")
    p.evaluate("settings('usr')"); p.wait_for_timeout(1200)
    for i, theme in enumerate(['light', 'dark', 'contrast']):
        p.evaluate("t=>{S.dm=t=='dark'?'dark':'light';S.cb=t=='contrast'?'contrast':'';themeApply()}", theme); p.wait_for_timeout(300)
        p.evaluate("document.querySelectorAll('.otpset').forEach(o=>o.remove());document.querySelector('.otpa').click()"); p.wait_for_timeout(500)
        shown = p.evaluate("document.querySelector('.otpset .otpu').textContent.replace(/^Or a link the app may open: /,'')")
        info = p.evaluate("""(()=>{const s=document.querySelector('.otpset .otpqrs'),k=document.querySelector('.otpset .otpk'),r=s.getBoundingClientRect(),q=k.getBoundingClientRect();
          return {x:r.x,y:r.y,w:r.width,h:r.height,n:s.viewBox.baseVal.width,kx:q.x,ky:q.y,kh:q.height,dark:document.documentElement.classList.contains('dark')}})()""")
        if i == 0:
            ok('6. the dialog: the QR code beside the setup key (to its left, side by side)', info['w'] >= 150 and info['x'] + info['w'] <= info['kx'] + 1 and
               info['y'] < info['ky'] + info['kh'] and info['ky'] < info['y'] + info['h'], info)
            if os.environ.get('QR61_SHOT'): p.screenshot(path=os.environ['QR61_SHOT'])
        # a screenshot of the code with the dialog around it, as a phone's camera sees it
        pad = 24; clip = {'x': info['x'] - pad, 'y': info['y'] - pad, 'width': info['w'] + 2 * pad, 'height': info['h'] + 2 * pad}
        png = p.screenshot(clip=clip); img = cv2.imdecode(np.frombuffer(png, np.uint8), cv2.IMREAD_COLOR)
        t, z = decode(img)
        ok('%d. %s theme%s: the screenshot decodes to the exact link shown (%s)' % (7 + i * 2, theme, ' (dark page)' if info['dark'] else '',
           'OpenCV and zbarimg' if ZBAR else 'OpenCV'), t == shown and z == shown and shown.startswith('otpauth://totp/Rudolf%20OS%3Ajohn?secret='), (t, z, shown))
        # the quiet zone and the contrast, measured in the screenshot (one module = w/n pixels)
        mpx = info['w'] / info['n']; rgb = cv2.cvtColor(img, cv2.COLOR_BGR2RGB).astype(float)
        o = pad; inner = rgb[o:o + int(info['h']), o:o + int(info['w'])]; q4 = int(4 * mpx) - 1
        ring = np.concatenate([inner[:q4].reshape(-1, 3), inner[-q4:].reshape(-1, 3), inner[:, :q4].reshape(-1, 3), inner[:, -q4:].reshape(-1, 3)])
        L = lum(inner); lo, hi = L.min(), L.max()
        ok('%d. %s theme: the quiet zone 4 modules of white (%.1f px each), dark modules black: contrast %.1f:1' % (8 + i * 2, theme, mpx, (hi + .05) / (lo + .05)),
           ring.min() >= 250 and lo < 0.01 and hi > 0.97 and (hi + .05) / (lo + .05) >= 15, (ring.min(), lo, hi))
    ok('13. no page errors', not errs, errs)
    b.close()
srv.shutdown(); shutil.rmtree(TMP, ignore_errors=True)
print('%d/%d passed' % (sum(R), len(R))); sys.exit(0 if all(R) else 1)
