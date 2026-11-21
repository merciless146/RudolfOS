# Rudolf OS 1.6.1 (docs/plan-1.6.1.md Part 2) in QEMU: the mobile build's three panels, the groundwork, on the real release
# (the newest stable or beta, or REL-DIR) with this tree's rudolfd, page and rudolf-native put in place (PN161_FIX: as
# fix161.sh puts rudolfd and the root helpers, the page, rudolf-native; an extra disk). The touch screen is QEMU's tablet,
# which rudolf-display takes as one (an absolute pointer with a touch: "touchscreens, pens, and absolute pointers such as
# QEMU's tablet") and gives X as the pointer, as it gives a real touch screen; a swipe is a press, a quick move and a
# release on it. (QEMU's virtio-multitouch reports only the multi-touch axes, which rudolf-display does not read: noted.)
#   S. Standard: as the desktop builds are (no flag): html.panels absent, a swipe on the desktop switches nothing; with the
#      mobile build's flag (/etc/rudolf/build-kind "mobile", rudolfd's RUDOLFD.mobile): the panels on, a swipe to the left
#      on the desktop the next workspace, to the right the one before, a window full screen, a swipe on its title bar
#   N. Native: the same without the flag (nothing) and with it (rudolf-native's pn_on): a window maximised as it comes, a
#      swipe on its title bar and on the desktop switch the workspace shown, never past the first or the last; the keys
#   python3 pn161_t.py REL-DIR    (SB_PARTS, QEMU, QEMU_ACCEL as for t160; root)
import os, sys, time, re, subprocess, shutil, struct
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); FIX = os.environ.get('PN161_FIX', '')
LOG = open('pn161.txt', 'w'); R = []; T0 = time.time()
def out(*a):
    s = ' '.join(str(x) for x in a); s = f'[{int(time.time() - T0):5d}s] ' + s; print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:1500] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 3000; DRV = os.path.abspath('pn161.img'); MEM = int(os.environ.get('MEM', '4096')); W = os.path.abspath('w161pn')
shutil.rmtree(W, ignore_errors=True); os.makedirs(W)
def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    d = f'{T}/os/system/a'; os.makedirs(d)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs'): shutil.copy(f'{REL}/{n}', f'{d}/{n}')
    for n in ('manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{d}/{n}')
    open(f'{d}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n"); open(f'{d}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(446); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as dd: shutil.copyfileobj(dd, f)
    os.remove(D)
FIXIMG = ''
if FIX:
    FD = os.path.join(W, 'fix'); shutil.copytree(FIX, FD)
    open(f'{FD}/rn', 'w').write('#!/bin/sh\nexport RUDOLF_NATIVE_STATE=/tmp/nstate RUDOLF_NATIVE_TEST_EVAL=/tmp/teval\nexec /run/r161x/rudolf-native "$@"\n'); os.chmod(f'{FD}/rn', 0o755)
    FIXIMG = os.path.abspath('pn161fix.img'); subprocess.run(['truncate', '-s', '128M', FIXIMG], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'R161FIX', '-d', FD, FIXIMG], check=True)
INIT_JS = "(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};return 1})()"
def boot(tag):
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf161',
             '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1', '-device', 'virtio-multitouch-pci']
    if FIXIMG: extra += ['-drive', f'file={FIXIMG},format=raw,if=none,id=fx,readonly=on', '-device', 'nvme,drive=fx,serial=pn161fix']
    vm = sbvm.VM('pn161' + tag, PARTS, [], secure=False, mem=MEM, extra=extra)
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    time.sleep(30); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    return vm, g, up
def run(g, c, t=60, show=True):
    f = os.path.join(W, 'c.sh'); open(f, 'w').write(c + '\n'); g.put(f, '/tmp/c161.sh', '644')
    o = g.sh('sh /tmp/c161.sh 2>&1', t)
    if show: out('$', c[:160].replace('\n', ' ; ')); out(o.rstrip()[-2500:])
    return o
def jsfn(g):
    def js(e, t=90):
        for _ in range(5):
            r = g.js(e, t)
            if not (isinstance(r, tuple) and (r[0] == 'ERR' or (r[0] == 'EXC' and 'ReferenceError' in str(r[1])))): return r
            time.sleep(10)
        return r
    return js
def wait_for(js, expr, secs, every=2):
    end = time.time() + secs; r = None
    while time.time() < end:
        r = js(expr, 60)
        if r is True: return True
        time.sleep(every)
    return r
# a swipe on QEMU's tablet (a touch screen to rudolf-display): pressed at (x0,y), moved quickly to (x1,y), let go
def swipe(q, x0, y0, x1, y1, SW, SH, steps=6, ms=240):
    q.move(x0, y0, SW, SH); time.sleep(0.1); q.cmd('input-send-event', events=[{'type': 'btn', 'data': {'down': True, 'button': 'left'}}])
    for i in range(1, steps + 1): time.sleep(ms / 1000 / steps); q.move(x0 + (x1 - x0) * i / steps, y0 + (y1 - y0) * i / steps, SW, SH)
    time.sleep(0.05); q.cmd('input-send-event', events=[{'type': 'btn', 'data': {'down': False, 'button': 'left'}}]); time.sleep(1.2)
FLAG = "mkdir -p /etc/rudolf; echo mobile > /etc/rudolf/build-kind; cat /etc/rudolf/build-kind"
make_drive(DRV)
vm = None
try:
    # ------------------------------------------------------------------ S. Standard
    vm, g, up = boot('s'); js = jsfn(g); sh = lambda c, t=60, show=True: run(g, c, t, show)
    ok('S0. the release starts (Standard)', up)
    if not up: raise RuntimeError('did not start')
    sh("for i in $(seq 1 90); do busybox wget -q -O /dev/null http://127.0.0.1:8080/favicon.png && break; sleep 2; done; cat /usr/share/rudolf/VERSION; "
       "cat /run/rudolf/inputs 2>/dev/null | head; grep -l -i multitouch /sys/class/input/*/device/name 2>/dev/null; for d in /sys/class/input/event*; do echo \"$d $(cat $d/device/name) abs=$(cat $d/device/capabilities/abs)\"; done", 240)
    def fix(reload):
        g.put(os.path.join(HERE, 'fix161.sh'), '/tmp/fix161.sh', '644')
        return sh("sh /tmp/fix161.sh; WWW=/usr/share/rudolf/www; [ -f /persist/os/current/index.html ] && WWW=/persist/os/current; cp /tmp/fx/index.html /tmp/page.html; chmod 644 /tmp/page.html; mountpoint -q $WWW/index.html || mount --bind /tmp/page.html $WWW/index.html; "
                  + ("pkill -f '[-]-rudolf-kiosk'; echo reloaded" if reload else "echo placed"), 300)
    if FIX: o = fix(True); ok('S0b. this tree\'s rudolfd and page in place', 'through rudolf-rootd' in o and 'reloaded' in o, o)
    LOGIN = ("(async()=>{try{const a=ACCTS.find(x=>x.n=='boss');if(USER)userLogout();USER=a;await userLogin(a,'b1');lastInput=Date.now();"
             "const L=document.getElementById('login');if(L)L.style.display='none';return 'in '+USER.n}catch(e){return 'ERR '+e.message}})()")
    def page_in(make, extra='true'):
        # (the page may load once more after a reload: signed in once it is up, again if it loaded meanwhile)
        for attempt in range(3):
            end = time.time() + 600
            while time.time() < end and js(f"typeof acctNew=='function'&&typeof wsGo=='function'&&typeof pnSwipe=='function'&&typeof ACCTS=='object'&&({extra})", 20) is not True: time.sleep(5)
            time.sleep(20); js(INIT_JS)
            if make or not js("ACCTS.some(x=>x.n=='boss')"): js("(async()=>{ACCTS.length=0;const b=await acctNew('boss','b1','admin');ACCTS.push(b);saveUser();return b.id})()", 120)
            out('sign-in:', js(LOGIN, 300))
            if wait_for(js, "!!(USER&&USER.disk&&hdOn())", 300) is True: break
        time.sleep(8)
    page_in(True)
    SW, SH = js("[innerWidth,innerHeight]") or [1280, 800]; my = SH // 3
    js("(()=>{wins.slice().forEach(w=>w.close());wsGo(1);return 1})()"); time.sleep(1)
    swipe(g.q, SW * 0.8, my, SW * 0.2, my, SW, SH)
    ok('S1. the desktop build (no flag): no panels, a swipe on the desktop switches nothing', js("!document.documentElement.classList.contains('panels')&&WS==1") is True, js("[document.documentElement.className,WS]"))
    o = sh(FLAG + "; pkill -f '[-]-rudolf-kiosk'; echo reloaded", 60)
    page_in(False, "typeof RUDOLFD=='object'&&RUDOLFD.mobile==1")
    ok('S2. the mobile build\'s flag: RUDOLFD.mobile, the panels on (each a workspace full screen: the tablet layout)', js("RUDOLFD.mobile==1&&document.documentElement.classList.contains('panels')&&document.documentElement.classList.contains('tablet')") is True, js("[document.documentElement.className,RUDOLFD.mobile]"))
    js("(()=>{wins.slice().forEach(w=>w.close());wsGo(1);return 1})()"); time.sleep(1)
    seq = []
    for _ in range(3): swipe(g.q, SW * 0.8, my, SW * 0.2, my, SW, SH); seq.append(js('WS'))
    swipe(g.q, SW * 0.2, my, SW * 0.8, my, SW, SH); seq.append(js('WS'))
    ok('S3. Standard: swipes on the touch screen: left, left, left (not past the third), right', seq == [2, 3, 3, 2], seq)
    vm.shot('std-panel2')
    js("(()=>{launch('notepad');return 1})()"); time.sleep(4)
    g2 = js("(()=>{const w=wins.find(o=>o.app=='notepad'&&o.ws==WS);if(!w)return null;const r=w.el.getBoundingClientRect(),d=document.getElementById('desk').getBoundingClientRect(),t=w.el.querySelector('.tb').getBoundingClientRect();return [r.width-d.width,r.height-d.height,t.top+t.height/2]})()")
    ok('S4. Standard: a window on a panel fills it', isinstance(g2, list) and abs(g2[0]) < 2 and abs(g2[1]) < 2, g2)
    if isinstance(g2, list): swipe(g.q, SW * 0.7, g2[2], SW * 0.15, g2[2], SW, SH)
    ok('S5. Standard: a swipe on its title bar: the next panel', js('WS') == 3, js('WS'))
    vm.shot('std-titlebar')
    js("(async()=>{wins.slice().forEach(w=>w.close());try{fsFlush()}catch(e){}try{await homeChain}catch(e){}try{await __rudolfLsFlush()}catch(e){}return 1})()", 60)
    sh("rm -f /etc/rudolf/build-kind; echo native > /persist/etc/rudolf-desktop-engine; rm -f /persist/etc/rudolf-desktop-engine.failed; sync", 60)
    time.sleep(5); vm.stop(); time.sleep(3)
    # ------------------------------------------------------------------ N. Native
    vm, g, up = boot('n'); sh = lambda c, t=60, show=True: run(g, c, t, show)
    ok('N0. the release starts again (Native)', up)
    if not up: raise RuntimeError('did not start')
    sh("for i in $(seq 1 60); do pgrep -x rudolf-native >/dev/null && break; sleep 3; done; cat /run/rudolf/desktop-engine", 240)
    state = lambda: sh("cat /tmp/nstate", 30, False)
    def st_get(st, k):
        m = re.search(rf'^{k} (.*)$', st, re.M); return m.group(1) if m else None
    def restart_native(flag):
        return sh((FLAG + "; " if flag else "rm -f /etc/rudolf/build-kind; ") + "rm -f /tmp/nstate; P0=$(pgrep -f 'rudolf-native [-]-rudolf-kiosk'); kill $P0; sleep 3; kill -9 $P0 2>/dev/null; for i in $(seq 1 90); do [ -s /tmp/nstate ] && break; sleep 2; done; grep -m1 '^screen ' /tmp/nstate", 300)
    if FIX:
        fix(False)
        o = sh("X=/run/r161x; cp /tmp/fx/rudolf-native $X/rudolf-native; chmod 755 $X/rudolf-native; rm -f /tmp/teval /tmp/teval.out; mkfifo -m 666 /tmp/teval; mountpoint -q /usr/bin/rudolf-native || mount --bind /tmp/fx/rn /usr/bin/rudolf-native; echo bound", 60)
    o = restart_native(False); ok('N0b. this tree\'s rudolf-native started with the tests\' views', 'screen ' in o, o)
    st = state(); SW, SH = [int(x) for x in (st_get(st, 'screen') or '1280 800').split()]
    def signin():
        st = state()
        if st_get(st, 'signin') == '1':
            time.sleep(3); g.q.type('b1'); time.sleep(0.5); g.q.key('ret')
            for _ in range(30):
                time.sleep(3); st = state()
                if st_get(st, 'user') == 'boss': break
        return st
    st = signin(); ok('N1. signed in on the native sign-in screen', st_get(st, 'user') == 'boss', st[:500]); time.sleep(15)
    def hit(st, k):
        m = re.search(rf'^hit {k} (\d+) (\d+) (\d+) (\d+)$', st, re.M); return (int(m.group(1)) + int(m.group(3)) / 2, int(m.group(2)) + int(m.group(4)) / 2) if m else None
    def open_app(k, q, wait=60):
        st = state(); h = hit(st, 'start \\S+')
        if h: g.q.at(*h, 'left', SW, SH); time.sleep(1.5); g.q.type(q); time.sleep(1.5); g.q.key('ret')
        for _ in range(wait):
            time.sleep(1)
            if re.search(rf'^client .* app={k} ', state(), re.M): return True
        return False
    my = SH // 3
    swipe(g.q, SW * 0.8, my, SW * 0.2, my, SW, SH); st = state()
    ok('N2. Native, the desktop build (no flag): a swipe on the desktop switches nothing', st_get(st, 'ws') == '1', st_get(st, 'ws'))
    o = restart_native(True); st = signin(); time.sleep(15)
    out('open notepad', open_app('notepad', 'notepad')); time.sleep(8); st = state()
    cl = [l for l in st.split('\n') if l.startswith('client ') and 'app=notepad' in l]; out(cl)
    ok('N3. Native with the mobile build\'s flag: a window maximised as it comes (a panel is the workspace, full screen)', cl and 'max=1' in cl[0], cl)
    ty = None
    if cl:
        p = cl[0].split(); ty = int(p[4]) + 12   # (its frame's top: the title bar)
    seq = []
    if ty is not None: swipe(g.q, SW * 0.7, ty, SW * 0.15, ty, SW, SH); seq.append(st_get(state(), 'ws'))
    swipe(g.q, SW * 0.8, my, SW * 0.2, my, SW, SH); seq.append(st_get(state(), 'ws'))
    swipe(g.q, SW * 0.8, my, SW * 0.2, my, SW, SH); seq.append(st_get(state(), 'ws'))
    swipe(g.q, SW * 0.2, my, SW * 0.8, my, SW, SH); seq.append(st_get(state(), 'ws'))
    ok('N4. Native: a swipe on the window\'s title bar, then on the desktop: the next workspace, the third, not past it, back', seq == ['2', '3', '3', '2'], seq)
    vm.shot('nat-panel')
    def ws_after(keys, want):
        g.q.key(*keys); v = None
        for _ in range(8):
            time.sleep(1.5); v = st_get(state(), 'ws')
            if v == want: break
        return v
    a = ws_after(('ctrl', 'alt', 'left'), '1'); b = ws_after(('ctrl', 'alt', 'right'), '2')
    ok('N5. Native: the keys (Ctrl+Alt+arrows) as before', a == '1' and b == '2', (a, b))
    out(sh("grep -v 'dbus\\|Warning' /run/rudolf/native-desktop.log | tail -20", 30, False))
except RuntimeError as e:
    ok('the run went on: ' + str(e), False)
finally:
    try:
        if vm: vm.stop()
    except Exception: pass
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
