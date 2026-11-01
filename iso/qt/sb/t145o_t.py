# Rudolf OS 1.4.5 parts 3 and 4 in QEMU: SpreadsheetPro and PresentPro in the real desktop (its Chromium, its fonts):
# typing with the keyboard, a formula, an .xlsx saved and opened again, a deck presented with the keyboard (a presenter
# remote's keys), pictures of the screen at each step. The release's kernel with its system in slot a (as t145p).
#   python3 t145o_t.py REL-DIR
import os, sys, time, re, subprocess, shutil, struct, json
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); LOG = open('t145o.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:300] if d != '' and not c else ''))
def last(t): return t.strip().split('\n')[-1].strip() if t.strip() else ''
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 1500; DRV = os.path.abspath('int145o.img')

def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    a = f'{T}/os/system/a'; os.makedirs(a)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs', 'manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{a}/{n}')
    open(f'{a}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n")
    open(f'{a}/complete', 'w').write('1\n'); open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)

EXTRA = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf145o', 
         '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.trying=1 rudolf.debug=1']
N = [0]
def boot():
    N[0] += 1
    vm = sbvm.VM(f't145o{N[0]}', PARTS, [], secure=False, mem=3072, extra=EXTRA)
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    time.sleep(45); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    def js(e, t=60):   # the page is loaded again when Guest signs in: wait and try again
        for _ in range(4):
            r = g.js(e, t)
            if not (isinstance(r, tuple) and (r[0] == 'ERR' or (r[0] == 'EXC' and 'ReferenceError' in str(r[1])))): return r
            time.sleep(15)
        return r
    end = time.time() + 600
    while up and time.time() < end and js('typeof sheetApp=="function"&&typeof deckApp=="function"', 20) is not True: time.sleep(5)
    js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};const b=[...document.querySelectorAll('button')].find(b=>b.textContent.trim()=='Guest');if(b)b.click();return 1})()")
    time.sleep(12); js('1')
    return vm, g, up, js

make_drive(DRV)
vm = None
try:
    vm, g, up, js = boot()
    ok('1. Rudolf OS starts', up)
    if not up: raise SystemExit
    def shot(n): vm.shot(n)
    keys = lambda *k: vm.keys(*k, gap=0.25)
    def typ(t):
        m = {' ': 'spc', '=': 'equal', '*': 'kp_multiply', '+': 'kp_add', '.': 'dot', ',': 'comma', '(': 'shift-9', ')': 'shift-0', ':': 'shift-semicolon', '"': 'shift-apostrophe', '-': 'minus', '/': 'slash'}
        for ch in t:
            if ch in m: vm.keys(m[ch], gap=0.08)
            elif ch.isupper(): vm.keys('shift-' + ch.lower(), gap=0.08)
            else: vm.keys(ch, gap=0.08)
    ok('2. SpreadsheetPro and PresentPro are in the Start menu\'s Office group', js("(()=>{startMenuApps();return [...document.querySelectorAll('#menu li.offi')].map(l=>l.textContent).join('|')})()") == '📝 WordPro|📊 SpreadsheetPro|📽️ PresentPro')
    js("(()=>{wins.slice().forEach(w=>{w.__force=1;w.close()});launch('sheet');window.W=wins.at(-1);W.el.querySelector('.mx').click();return 1})()"); time.sleep(4)
    typ('Fruit'); keys('tab'); typ('Price'); keys('ret'); typ('Apples'); keys('tab'); typ('1.25'); keys('ret'); typ('Pears'); keys('tab'); typ('2.5'); keys('ret')
    typ('Total'); keys('tab'); typ('=SUM(B2:B3)'); keys('ret'); time.sleep(1); shot('sheet')
    r = js("JSON.stringify([W.sp.value('A2'),W.sp.value('B3'),W.sp.cell('B4')&&W.sp.cell('B4').f,W.sp.value('B4')])")
    ok('3. SpreadsheetPro: typed with the keyboard, a formula worked out (3.75)', r == '["Apples",2.5,"=SUM(B2:B3)",3.75]', r)
    r = js("(async()=>{const b=await spToXlsx(W.sp.book);const d=await offDataUrl(b);node(['C:'])['T145O.XLSX']=d;const b2=await spFromXlsx(offBytes(d));SPE.recalc(b2,null);const s=b2.sheets[0];return [s.cells.get(spK(3,1)).f,s.cells.get(spK(3,1)).val].join('|')})()", 60)
    ok('...an .xlsx saved and opened again keeps the formula and its result', r == '=SUM(B2:B3)|3.75', r)
    fps = js("(async()=>{const sc=W.el.querySelector('.sp-sc');let n=0;const t0=performance.now();await new Promise(res=>{const f=()=>{sc.scrollTop+=60;n++;if(performance.now()-t0<2000)requestAnimationFrame(f);else res()};requestAnimationFrame(f)});return Math.round(n/((performance.now()-t0)/1000))})()", 30)
    out('scrolling in QEMU (no graphics card, so slower than a laptop):', fps, 'frames a second')
    ok('...scrolls', isinstance(fps, (int, float)) and fps > 5, fps)
    js("(()=>{W.__force=1;W.close();window.P=deckApp(null,ppTemplate('talk'));P.el.querySelector('.mx').click();return 1})()"); time.sleep(4); shot('deck')
    ok('4. PresentPro opens a template with its theme and six slides', js("P.pp.deck.slides.length==6&&!!P.el.querySelector('.pp-stage .pp-slide')") is True)
    js("P.pp.present(0);1"); time.sleep(3); shot('show1')
    keys('pgdn'); time.sleep(1.5); keys('right'); time.sleep(1.5)
    st = js("JSON.stringify(P.show&&P.show.state)")
    ok('5. presenting: a presenter remote\'s Page Down and the arrow keys move on (slide 3)', '"slide":2' in str(st), st)
    keys('p'); time.sleep(1.5); shot('presenter')
    ok('...P shows the presenter view with the notes and the time', js("(()=>{const o=document.querySelector('.pp-show');return o.classList.contains('pv')&&/\\d:\\d\\d/.test(o.querySelector('.pp-pvh .e').textContent)})()") is True)
    keys('p'); keys('b'); time.sleep(0.5); ok('...B makes the screen black', js("P.show.state.blank=='#000'") is True); keys('b')
    keys('pgup'); time.sleep(1.5); ok('...Page Up goes back (slide 2)', '"slide":1' in str(js("JSON.stringify(P.show.state)")))
    keys('esc'); time.sleep(1); ok('...Esc ends the show', js("!document.querySelector('.pp-show')") is True)
finally:
    try: vm and vm.stop()
    except Exception: pass
try: os.remove(DRV)
except OSError: pass
out(f'{sum(R)}/{len(R)} passed')
