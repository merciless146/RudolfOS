# Rudolf OS 1.6.0: Oracle's file:/// pages on the real release (the Vivobook, 2026-11-15: every file:/// address showed a
# blank white page in Oracle, while its about: pages showed). The newest beta (or REL-DIR's release) in QEMU, started as
# t160 starts it (slot a, UEFI, rudolf.debug=1 for cdp-eval); an account signed in; then Oracle opens about:home,
# about:files?p=C:, file:///C:/README.TXT, file:///C:/WELCOME.HTM and the account's own CALL-TEST.HTML (from File
# Explorer's opener). For each: what the tab's frame holds (sandbox, srcdoc), whether its page ran and laid out (a probe
# appended to the same srcdoc posts back its text and height), and the screen's pixels in the frame (the README page and
# Oracle's own pages have a blue title bar; WELCOME.HTM and CALL-TEST.HTML have text). Then the phone-number link.
#   python3 oracle_file_t.py REL-DIR   (QEMU as for t160; root; QEMU_ACCEL=kvm where it can)
import os, sys, time, re, subprocess, shutil, struct, json
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1])
PARTS = os.environ.get('SB_PARTS', '/nonexistent'); LOG = open('oracle_file.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:1500] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
out('release', mv('serial'), mv('version'), 'chromium', mv('chromium'))
DMB = 2000; DRV = os.path.abspath('of160.img'); MEM = int(os.environ.get('MEM', '3072'))

def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    d = f'{T}/os/system/a'; os.makedirs(d)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs', 'manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{d}/{n}')
    open(f'{d}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n")
    open(f'{d}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)

def boot(tag):
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf160o',
             '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1']
    vm = sbvm.VM('of160' + tag, PARTS, [], secure=False, mem=MEM, extra=extra)
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    time.sleep(30); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    def js(e, t=90):
        for _ in range(5):
            r = g.js(e, t)
            if not (isinstance(r, tuple) and (r[0] == 'ERR' or (r[0] == 'EXC' and 'ReferenceError' in str(r[1])))): return r
            time.sleep(15)
        return r
    end = time.time() + 600
    while up and time.time() < end and js("typeof acctNew=='function'&&typeof userLogin=='function'", 20) is not True: time.sleep(5)
    js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};return 1})()")
    return vm, g, up, js

def pixels(png, rect, dpr):
    """non-white and blue (#2563eb-ish) pixels in the frame's rectangle of a screenshot"""
    try:
        from PIL import Image
    except ImportError:
        return {'err': 'no PIL'}
    im = Image.open(png).convert('RGB'); W, H = im.size
    x0, y0 = max(0, int(rect['x'] * dpr) + 2), max(0, int(rect['y'] * dpr) + 2)
    x1, y1 = min(W, int((rect['x'] + rect['w']) * dpr) - 2), min(H, int((rect['y'] + rect['h']) * dpr) - 2)
    if x1 <= x0 or y1 <= y0: return {'err': 'empty rect', 'size': [W, H]}
    nw = bl = n = 0
    for y in range(y0, y1, 2):
        for x in range(x0, x1, 2):
            r, g_, b = im.getpixel((x, y)); n += 1
            if r < 235 or g_ < 235 or b < 235: nw += 1
            if b > 180 and r < 90 and 70 < g_ < 140: bl += 1
    return {'n': n, 'nonwhite': nw, 'blue': bl, 'size': [W, H]}

# what the tab's frame shows: its attributes, and a probe appended to the same srcdoc (posts back what the page laid out)
PROBE = r"""(async()=>{const W=window.__ofW;if(!W)return JSON.stringify({err:'no window'});
 const frs=[...W.body.querySelectorAll('.fh iframe')],fr=frs.find(f=>f.style.display=='block')||frs[0];if(!fr)return JSON.stringify({err:'no frame'});
 const b=fr.getBoundingClientRect(),o={frames:frs.length,disp:frs.map(f=>f.style.display),sandbox:fr.getAttribute('sandbox'),allow:fr.getAttribute('allow'),
  src:fr.getAttribute('src'),srcdocLen:(fr.getAttribute('srcdoc')||'').length,head:(fr.getAttribute('srcdoc')||'').slice(0,160),
  rect:{x:b.x,y:b.y,w:b.width,h:b.height},vis:getComputedStyle(fr).visibility,op:getComputedStyle(fr).opacity,filter:getComputedStyle(fr).filter,
  stat:(W.body.querySelector('.stat')||{}).textContent,addr:(W.body.querySelector('.ad')||{}).value,title:document.title,dpr:devicePixelRatio,
  zWin:W.el.style.zIndex,fhBg:getComputedStyle(W.body.querySelector('.fh')).backgroundColor,cw:!!fr.contentWindow};
 /* elements that cover the frame's middle */
 try{const e=document.elementFromPoint(b.x+b.width/2,b.y+Math.min(60,b.height/2));o.atMid=e?(e.tagName+'.'+String(e.className||'').slice(0,40)+(e===fr?' (the frame)':'')):null}catch(x){o.atMid=String(x)}
 return JSON.stringify(o)})()"""
RUNPROBE = r"""(async()=>{const W=window.__ofW;const frs=[...W.body.querySelectorAll('.fh iframe')],fr=frs.find(f=>f.style.display=='block')||frs[0];
 const h=fr.getAttribute('srcdoc')||'';let got=null,loads=0;const on=e=>{if(e.source===fr.contentWindow&&e.data&&e.data.__ofprobe)got=e.data};addEventListener('message',on);
 fr.addEventListener('load',()=>loads++);
 fr.srcdoc=h+"<scr"+"ipt>setTimeout(function(){var b=document.body;parent.postMessage({__ofprobe:1,url:location.href,origin:String(self.origin),txt:b?b.innerText.length:-1,h:document.documentElement.scrollHeight,kids:b?b.children.length:-1,ready:document.readyState,csp:document.querySelector('meta[http-equiv]')?1:0},'*')},300)</scr"+"ipt>";
 for(let i=0;i<40&&!got;i++)await new Promise(r=>setTimeout(r,150));removeEventListener('message',on);
 /* the page as Oracle made it, again */
 fr.srcdoc=h;return JSON.stringify({probe:got,loads})})()"""

make_drive(DRV)
vm = None
try:
    vm, g, up, js = boot('a'); sh = lambda c, t=60: g.sh(c, t)
    ok('1. the release starts', up, 'not up')
    if not up: raise SystemExit
    r = js("(async()=>{ACCTS.length=0;const a=await acctNew('john','j1','admin');ACCTS.push(a);saveUser();return a.id})()", 120); out('account', r)
    r = js("(()=>{window.__si='wait';(async()=>{try{const a=ACCTS.find(x=>x.n=='john');const L=document.getElementById('login');USER=a;await userLogin(a,'j1');lastInput=Date.now();if(L)L.style.display='none';__si='in'}catch(e){__si='error '+e.message}})();return 1})()", 60)
    for _ in range(120):
        s = js("window.__si", 30)
        if s != 'wait': break
        time.sleep(5)
    out('sign-in', s)
    time.sleep(20)
    env = js("JSON.stringify({ua:navigator.userAgent,shell:typeof SHELL!='undefined'&&!!SHELL,rtOn:typeof rtOn=='function'&&rtOn(),engine:typeof RD!='undefined'&&RD&&RD.engine,href:location.href,dpr:devicePixelRatio,w:innerWidth,h:innerHeight,oext:typeof oextCfg=='function'?oextCfg():null,dark:typeof WEB_DARK!='undefined'?WEB_DARK:null,shellLog:typeof SHELL_LOG!='undefined'?SHELL_LOG.slice(-10):null})")
    out('page:', env)
    out(sh("cat /proc/cmdline; ls /run/rudolf | head -50; busybox wget -q -O- http://127.0.0.1:9222/json/version 2>&1 | head -c 600"))
    # the account's own page, opened from File Explorer's opener (as a double-click does)
    js("(async()=>{node(homePath('DOCUMENTS'))['CALL-TEST.HTML']='<!doctype html><title>Call test</title><body style=\"font:16px Arial;padding:20px\"><h1>Call test</h1><p><a id=\"tel\" href=\"tel:5551234567\">Call this number</a></p></body>';fsSave();try{fsFlush()}catch(e){}return 1})()", 60)
    URLS = [('about:home', 'blue'), ('about:files?p=C:', 'blue'), ('file:///C:/README.TXT', 'blue'), ('file:///C:/WELCOME.HTM', 'text'), ('explorer:CALL-TEST.HTML', 'text')]
    res = {}
    for u, want in URLS:
        if u.startswith('explorer:'):
            o = js("(()=>{wins.slice().forEach(w=>{try{w.close()}catch(e){}});const n=wins.length;openPath(homePath('DOCUMENTS').concat('CALL-TEST.HTML'));const W=wins[wins.length-1];window.__ofW=W&&W.el.classList.contains('oracle')?W:null;return String(!!window.__ofW)+' '+(W&&W.el.className)})()", 60)
        else:
            o = js(f"(()=>{{wins.slice().forEach(w=>{{try{{w.close()}}catch(e){{}}}});browser({json.dumps(u)});const W=wins[wins.length-1];window.__ofW=W;return String(!!W)+' '+(W&&W.el.className)}})()", 60)
        out(f'== {u}: opened', o); time.sleep(6)
        p = js(PROBE, 60)
        try: pj = json.loads(p)
        except Exception: pj = {'raw': p}
        out('frame:', json.dumps(pj)[:1500])
        png = vm.shot(re.sub(r'[^a-z0-9]+', '_', u.lower())[:30])
        px = pixels(png, pj['rect'], pj.get('dpr', 1)) if 'rect' in pj else {'err': 'no rect'}
        out('pixels:', px)
        pr = js(RUNPROBE, 60); out('probe:', pr); time.sleep(2)
        try: prj = json.loads(pr).get('probe') or {}
        except Exception: prj = {}
        shows = 'n' in px and (px['blue'] > 200 if want == 'blue' else px['nonwhite'] > 100)
        res[u] = (shows, prj)
        ok(f'2. Oracle shows {u}', shows, json.dumps({'frame': pj, 'pixels': px, 'probe': prj})[:1500])
    out(sh("tail -n 40 /run/rudolf/page.log 2>/dev/null; busybox wget -q -O- http://127.0.0.1:9222/json/list 2>&1 | grep -E '\"(type|url)\"' | head -60"))
    out('shell log:', js("JSON.stringify(typeof SHELL_LOG!='undefined'?SHELL_LOG.slice(-20):null)"))
    # the phone-number link: clicked inside the page, the desktop asks how to call it
    o = js(r"""(async()=>{wins.slice().forEach(w=>{try{w.close()}catch(e){}});openPath(homePath('DOCUMENTS').concat('CALL-TEST.HTML'));const W=wins[wins.length-1];window.__ofW=W;
     await new Promise(r=>setTimeout(r,4000));const frs=[...W.body.querySelectorAll('.fh iframe')],fr=frs.find(f=>f.style.display=='block')||frs[0];
     const n0=document.querySelectorAll('.win').length,h=fr.getAttribute('srcdoc')||'';
     fr.srcdoc=h+"<scr"+"ipt>setTimeout(function(){document.getElementById('tel').click()},400)</scr"+"ipt>";await new Promise(r=>setTimeout(r,4000));
     const ws=[...document.querySelectorAll('.win,.rmodal,.ntf,.toast')].map(e=>(e.querySelector('.tt,.ti,h3')||e).textContent.slice(0,80));
     return JSON.stringify({n0,n1:document.querySelectorAll('.win').length,ws:ws.slice(-6),body:document.body.innerText.match(/call[^\n]{0,80}/ig)})})()""", 90)
    out('tel:', o)
    ok('3. the phone-number link asks how to call it', isinstance(o, str) and re.search(r'5551234567|call', o.split('"ws"', 1)[-1], re.I), o)
finally:
    if vm: vm.stop()
out('RESULT', f'{sum(R)}/{len(R)} passed')
sys.exit(0 if R and all(R) else 1)
