# Rudolf OS 1.4.5 part 5 in QEMU: Shut down and Restart from the desktop's Power dialog, timed; what was saved just
# before is there after the start; the data partition is clean; a desktop step that stalls does not keep the
# computer on. The release's kernel is started directly with its system in slot a of an NVMe drive (as t144k).
#   python3 t145p_t.py REL-DIR      (on a release before 1.4.5 it measures the old way, for comparison)
import os, sys, time, re, subprocess, shutil, struct, json
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); LOG = open('t145p.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:300] if d != '' and not c else ''))
def last(t): return t.strip().split('\n')[-1].strip() if t.strip() else ''
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 1500; DRV = os.path.abspath('int145.img')

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

EXTRA = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf145', '-no-reboot',
         '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.trying=1 rudolf.debug=1']
N = [0]
def boot():
    N[0] += 1
    vm = sbvm.VM(f't145p{N[0]}', PARTS, [], secure=False, mem=3072, extra=EXTRA)
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
    while up and time.time() < end and js('typeof shutdownDlg=="function"&&typeof fsFlush=="function"', 20) is not True: time.sleep(5)
    js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};const b=[...document.querySelectorAll('button')].find(b=>b.textContent.trim()=='Guest');if(b)b.click();return 1})()")
    time.sleep(12); js('1')
    return vm, g, up, js
def press(vm, js, action, stall=False):
    """choose action in the Power dialog; the seconds until QEMU ends (the power is off, or a restart with -no-reboot)"""
    if stall: js("window.__rudolfLsFlush=()=>new Promise(()=>{});1")
    js("shutdownDlg();1"); time.sleep(1)
    t0 = time.time()
    js(f"document.querySelector('.rmbox button[data-a={action}]').click();1", 10)
    try: vm.p.wait(120); return time.time() - t0
    except subprocess.TimeoutExpired: vm.stop(); return None

make_drive(DRV)
try:
    vm, g, up, js = boot()
    ok('1. Rudolf OS starts', up)
    if not up: raise SystemExit
    # something saved just before: a file in the home folder (C:), a setting, a file on the data partition
    r = js("(()=>{const h=homePath('DOCUMENTS');let d=node(h);if(!d){d=node(homePath());d.DOCUMENTS={};d=d.DOCUMENTS}d['T145.TXT']='kept '+Date.now();fsSave();S.t145=1;persistS();return 1})()")
    out('saved:', r)
    t = press(vm, js, 'poweroff')
    out(f'shut down: {t and round(t, 1)} s')
    ok('2. Shut down: the power is off within 10 s', t is not None and t <= 10, t)
    vm, g, up, js = boot()
    ok('3. starts again', up)
    k = js("(()=>{const d=node(homePath('DOCUMENTS'));return [!!(d&&d['T145.TXT']),S.t145===1]})()")
    ok('...the file and the setting saved just before are there', k == [True, True], k)
    dm = g.sh("dmesg | grep -iE 'EXT4-fs.*(recovery|orphan)' | head -3; echo", 20)
    ok('...the data partition was clean (no journal recovery)', 'recovery' not in dm, dm)
    lg = g.sh('cat /persist/var/log/shutdown.log 2>/dev/null; echo', 20); out('shutdown.log:', lg.strip().replace('\n', ' | '))
    ok('...the log has the shutdown with its steps, and this start', re.search(r'poweroff desktop:files=\d+.*system:apps=\d+', lg) is not None and 'start uptime=' in lg, lg)
    t = press(vm, js, 'reboot')
    out(f'restart: {t and round(t, 1)} s')
    ok('4. Restart: the machine restarts within 10 s', t is not None and t <= 10, t)
    vm, g, up, js = boot()
    t = press(vm, js, 'poweroff', stall=True)
    out(f'shut down with a stalled desktop step: {t and round(t, 1)} s')
    ok('5. a desktop step that never finishes: still off within 12 s (3 s for the desktop)', t is not None and t <= 12, t)
finally:
    try: vm.stop()
    except Exception: pass
try: os.remove(DRV)
except OSError: pass
out(f'{sum(R)}/{len(R)} passed')
