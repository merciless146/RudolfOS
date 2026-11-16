# Rudolf OS 1.6.0 in QEMU: Oracle's pages drawn on a real release (the Vivobook, 2026-11-15: Welcome, README and an .HTML
# file from File Explorer showed a blank white page in Oracle). In Rudolf's Chromium 158 a sandboxed frame's own process
# sometimes started with stale owner properties and its page stayed blank; the desktop page now sends them again when such a
# frame has loaded (base_v24.html: frameOwnerRefresh). The newest beta (or the release in REL-DIR) starts from slot a as in
# t160 (UEFI, rudolf.debug=1 for cdp-eval), an account signs in, and Oracle opens README.TXT, WELCOME.HTM, a page in the
# account's Documents (File Explorer's opener), about:oracle and about:files, each in fresh windows and in one window one
# after another; each time the screen's pixels where the frame is say whether the page is drawn (not white). Then the
# phone-number test: a tel: link in that page, clicked with QEMU's pointer: the desktop asks how to call it.
# Only a release built with the fix passes (the beta of 2026-11-15, 2026111506, does not). Nothing is published.
#   python3 oracle_file_t.py REL-DIR   (QEMU as for t160; root; QEMU_ACCEL=kvm where it can; ROUNDS=3)
import os, sys, time, re, subprocess, shutil, struct, json
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1])
LOG = open('oracle_file.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:1500] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 2000; DRV = os.path.abspath('intof.img'); MEM = int(os.environ.get('MEM', '3072')); ROUNDS = int(os.environ.get('ROUNDS', '3'))
out('release', mv('serial'), mv('version'), 'chromium', mv('chromium'))

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
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolfof',
             '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1']
    os.makedirs('of' + tag, exist_ok=True); open(f'of{tag}/vars.fd', 'ab').close()   # (sbvm copies Secure Boot's variables otherwise; not used without it)
    vm = sbvm.VM('of' + tag, '/nonexistent', [], secure=False, mem=MEM, extra=extra)
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
    while up and time.time() < end and js("typeof acctNew=='function'&&typeof userLogin=='function'&&typeof browser=='function'", 20) is not True: time.sleep(5)
    js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};return 1})()")
    return vm, g, up, js

def signin(js, n, pw, secs=900):
    r = js(f"(()=>{{if(window.__si=='wait')return 'already';const a=ACCTS.find(x=>x.n=={json.dumps(n)});if(!a)return 'no account';window.__si='wait';"
           f"(async()=>{{try{{if(!(await pwCheck(a,{json.dumps(pw)}))){{__si='bad';return}}if(USER)userLogout();const L=document.getElementById('login');USER=a;"
           f"await userLogin(a,{json.dumps(pw)});lastInput=Date.now();if(L)L.style.display='none';__si='in '+USER.n}}catch(e){{__si='error '+e.message}}}})();return 'started'}})()", 60)
    if r not in ('started', 'already'): return r
    end = time.time() + secs
    while time.time() < end:
        s = js("window.__si", 30)
        if isinstance(s, str) and s != 'wait': return s
        time.sleep(5)
    return 'still signing in'

# the shown frame of the newest Oracle window: where it is (CSS pixels of the page), its sandbox, the window's address
FRAME = ("JSON.stringify((()=>{const W=wins.filter(w=>w.body&&w.body.querySelector('.otabs')).pop();if(!W)return{err:'no Oracle window'};"
         "const fr=[...W.body.querySelectorAll('.fh iframe')].find(f=>f.style.display=='block');if(!fr)return{err:'no frame shown'};const r=fr.getBoundingClientRect();"
         "return{rect:[r.x,r.y,r.width,r.height],iw:innerWidth,ih:innerHeight,sb:fr.getAttribute('sandbox'),cd:fr.contentDocument===null,ad:W.body.querySelector('.ad').value}})())")

def drawn(png, o):
    """the share of pixels that are not white inside the frame (a few pixels in from its edges)"""
    from PIL import Image
    im = Image.open(png).convert('RGB'); sc = im.width / o['iw']
    x, y, w, h = [int(v * sc) for v in o['rect']]
    c = im.crop((max(0, x + 4), max(0, y + 4), min(im.width, x + w - 4), min(im.height, y + h - 4)))
    px = list(c.getdata()); return round(sum(1 for p in px if min(p) < 235) / max(1, len(px)), 4)

CALL = ('<!doctype html><title>Call test</title><body style="font:16px Arial;margin:0">'
        '<a id="c" href="tel:5551234567" style="display:block;padding:120px 20px;font-size:28px;background:#fef3c7">Call this number</a></body>')
make_drive(DRV)
vm = None
try:
    vm, g, up, js = boot('a'); sh = lambda c, t=60: g.sh(c, t)
    ok('1. the release starts', up, 'not up')
    if not up: raise SystemExit
    r = js("(async()=>{ACCTS.length=0;const a=await acctNew('john','j1','admin');ACCTS.push(a);saveUser();return a.id})()", 120); out('account', r)
    out('sign-in:', signin(js, 'john', 'j1'))
    # the shell: the desktop moves into it by itself after the first start (1.2's one-time move); its hello within 10 minutes
    end = time.time() + 600
    while time.time() < end and js("typeof SHELL!='undefined'&&!!SHELL", 20) is not True: time.sleep(10)
    if js("!!USER&&USER.n=='john'", 20) is not True: out('sign-in again after the move:', signin(js, 'john', 'j1'))
    out('engine/shell:', js("JSON.stringify({engine:RD&&RD.engine,shell:typeof SHELL!='undefined'&&!!SHELL,ua:navigator.userAgent,user:USER&&USER.n})", 30))
    r = js(f"(()=>{{let d=fs['C:'];for(const k of homePath('DOCUMENTS').slice(1))d=d[k]||(d[k]={{}});d['CALL-TEST.HTML']={json.dumps(CALL)};fsSave();try{{fsFlush()}}catch(e){{}};return homePath('DOCUMENTS').join('/')}})()", 60)
    out('documents:', r); DOCS = r if isinstance(r, str) else 'C:/USERS/JOHN/DOCUMENTS'
    PAGES = ['file:///C:/README.TXT', 'file:///C:/WELCOME.HTM', f'file:///{DOCS}/CALL-TEST.HTML', 'about:oracle', 'about:files?p=C:']
    blank, opened, n = [], [], 0
    for k in range(ROUNDS):
        for same in (False, True):
            if same: js("(()=>{wins.slice().forEach(w=>{if(w.body&&w.body.querySelector('.otabs'))w.close()});browser('about:home');return 1})()", 30); time.sleep(3)
            for u in PAGES:
                if same:   # one window, the address typed (as a bookmark's click)
                    js(f"(()=>{{const W=wins.filter(w=>w.body&&w.body.querySelector('.otabs')).pop(),ad=W.body.querySelector('.ad');ad.value={json.dumps(u)};ad.dispatchEvent(new KeyboardEvent('keydown',{{key:'Enter',bubbles:true}}));return 1}})()", 30)
                elif 'CALL-TEST' in u:   # File Explorer's double-click: openPath
                    js("(()=>{wins.slice().forEach(w=>{if(w.body&&w.body.querySelector('.otabs'))w.close()});openPath(homePath('DOCUMENTS').concat('CALL-TEST.HTML'));return 1})()", 30)
                else:
                    js(f"(()=>{{wins.slice().forEach(w=>{{if(w.body&&w.body.querySelector('.otabs'))w.close()}});browser({json.dumps(u)});return 1}})()", 30)
                time.sleep(3); n += 1
                o = js(FRAME, 30)
                try: o = json.loads(o)
                except Exception: o = {'err': str(o)}
                if 'rect' not in o: blank.append([k, same, u, o]); continue
                d = drawn(vm.shot(f'r{k}{"s" if same else "f"}_{PAGES.index(u)}'), o)
                if d < 0.005: blank.append([k, same, u, d])
                if not (o.get('sb') == 'allow-scripts' and o.get('cd')): opened.append([u, o])
                out(f'{k} {"same window" if same else "fresh window"} {u}: {d}')
    ok(f'2. Oracle\'s pages are drawn, never a blank white page ({n} pages: README.TXT, WELCOME.HTM, an .HTML file from File Explorer, about:oracle, about:files; fresh windows and one window)', not blank, blank[:6])
    ok('3. they stay in sandboxed frames (allow-scripts; the desktop page not reachable from them)', not opened, opened[:3])
    # the phone-number test: the page's tel: link, clicked with QEMU's pointer
    js("(()=>{wins.slice().forEach(w=>{if(w.body&&w.body.querySelector('.otabs'))w.close()});document.querySelectorAll('.rmodal').forEach(x=>x.remove());try{S.telApp=''}catch(e){}openPath(homePath('DOCUMENTS').concat('CALL-TEST.HTML'));return 1})()", 30)
    time.sleep(4); o = js(FRAME, 30)
    try: o = json.loads(o)
    except Exception: o = {}
    if 'rect' in o:
        png = vm.shot('tel0'); from PIL import Image; im = Image.open(png); sc = im.width / o['iw']
        x, y, w, h = o['rect']; g.q.at(int((x + 140) * sc), int((y + 60) * sc), W=im.width, H=im.height); time.sleep(3); vm.shot('tel1')
    d = js("(()=>{const d=document.querySelector('.rmodal [aria-label=\"Call a phone number\"]');return d?d.innerText.slice(0,300):null})()", 30)
    out('after the click:', d)
    ok('4. the tel: link clicked: the desktop asks how to call it (Call 5551234567)', isinstance(d, str) and '5551234567' in d, d)
    out('page log:\n' + sh("tail -n 20 /run/rudolf/page.log 2>&1", 30))
finally:
    try: vm and vm.stop()
    except Exception: pass
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
