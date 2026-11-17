# Rudolf OS 1.6.0 on the real release in QEMU: the owner's Vivobook report of 2026-11-16 (beta 2026111620, Native), against Standard:
#   1. an app's window (Notepad, Calculator, Settings, File Explorer) watched from the moment the shell makes it (fixtures/wincap.c,
#      every 50 ms): on the screen only at its own size, its page the app as it ends up or a plain colour (never the sign-in screen
#      or the desktop page at a large size)
#   2. the desktop's menu and then a second right-click elsewhere (the desktop's, an icon's, the taskbar's): nothing of the first
#      menu left on the screen (QEMU's own picture of the screen, before and after)
#   3. the desktop's icons after signing in (Standard: the page's, at once), and New folder, New text document, a file saved to the
#      Desktop folder by an app and a file deleted there on the desktop within 500 ms (timed in the computer: a watcher of the
#      shell's state every 20 ms)
#   4. a background chosen in Settings › Display Properties on the desktop within 300 ms (Standard: the page's preview), back on Revert
#   python3 v160_t.py REL-DIR    (SB_PARTS, QEMU, QEMU_ACCEL as for t160; root)
#   V160_FIX=DIR: DIR/rudolf-native, DIR/index.html and DIR/wincap put in place of the release's (an extra disk), as k160_t.py's K160_FIX
import os, sys, time, re, subprocess, shutil, struct, json
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); FIX = os.environ.get('V160_FIX', '')
LOG = open('v160.txt', 'w'); R = []; T0 = time.time(); TM = {}
def out(*a):
    s = ' '.join(str(x) for x in a); s = f'[{int(time.time() - T0):5d}s] ' + s; print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:1500] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 3000; DRV = os.path.abspath('v160.img'); MEM = int(os.environ.get('MEM', '4096')); W = os.path.abspath('w160v')
shutil.rmtree(W, ignore_errors=True); os.makedirs(W)
def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    d = f'{T}/os/system/a'; os.makedirs(d)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs'): shutil.copy(f'{REL}/{n}', f'{d}/{n}')
    for n in ('manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{d}/{n}')
    open(f'{d}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes {mv('notes')}\n"); open(f'{d}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)
FIXIMG = ''
if FIX:
    FD = os.path.join(W, 'fix'); shutil.copytree(FIX, FD)
    open(f'{FD}/rn', 'w').write('#!/bin/sh\nexport RUDOLF_NATIVE_STATE=/tmp/nstate RUDOLF_NATIVE_TEST_EVAL=/tmp/teval\nexec /tmp/fx/rudolf-native "$@"\n'); os.chmod(f'{FD}/rn', 0o755)
    for n in ('rudolf-native', 'wincap'):
        if os.path.exists(f'{FD}/{n}'): os.chmod(f'{FD}/{n}', 0o755)
    # (the watcher: the shell's state lines that matter, with the time in centiseconds since the start, whenever they change)
    open(f'{FD}/vw.sh', 'w').write('''#!/bin/sh
last=""
while [ ! -e /tmp/vw.stop ]; do
 t=$(cut -d' ' -f1 /proc/uptime)
 s=$(grep -E '^(user|signin|cmenu|dialog|wallpaper) ' /tmp/nstate 2>/dev/null; echo "dicons $(grep -c '^dicon ' /tmp/nstate 2>/dev/null) $(grep -E '^dicon .*[|](NEW FOLDER|NEW TEXT DOCUMENT.TXT|SAVED.TXT)$' /tmp/nstate 2>/dev/null | sed 's/.*|//' | tr '\\n' ',')")
 if [ "$s" != "$last" ]; then { echo "T $t"; echo "$s"; } >> /tmp/vw.log; last="$s"; fi
 usleep 20000
done
''')
    FIXIMG = os.path.abspath('v160fix.img'); subprocess.run(['truncate', '-s', '64M', FIXIMG], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'K160FIX', '-d', FD, FIXIMG], check=True)
INIT_JS = "(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};return 1})()"
def boot(tag):
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf160',
             '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1',
             '-audiodev', 'none,id=snd0', '-device', 'intel-hda', '-device', 'hda-duplex,audiodev=snd0']
    if FIXIMG: extra += ['-drive', f'file={FIXIMG},format=raw,if=none,id=fx,readonly=on', '-device', 'nvme,drive=fx,serial=k160fix']
    vm = sbvm.VM('v160' + tag, PARTS, [], secure=False, mem=MEM, extra=extra)
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    time.sleep(30); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    return vm, g, up
def run(g, c, t=60, show=True):
    f = os.path.join(W, 'c.sh'); open(f, 'w').write(c + '\n'); g.put(f, '/tmp/c160.sh', '644')
    o = g.sh('sh /tmp/c160.sh 2>&1', t)
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
def far_left(ref, now, box, ex):
    """the pixels in BOX (x y w h) less EX (x0 y0 x1 y1) that differ from REF by more than 60 in a channel: what is left of a menu"""
    from PIL import Image
    a = Image.open(ref).convert('RGB').load(); b = Image.open(now).convert('RGB').load(); x, y, w, h = box; n = t = 0
    for j in range(max(0, y), y + h):
        for i in range(max(0, x), x + w):
            if ex[0] <= i < ex[2] and ex[1] <= j < ex[3]: continue
            try: p = a[i, j]; q = b[i, j]
            except IndexError: continue
            t += 1
            if max(abs(p[0] - q[0]), abs(p[1] - q[1]), abs(p[2] - q[2])) > 60: n += 1
    return n, t

make_drive(DRV)
vm = None
try:
    # ------------------------------------------------------------------ Standard (Chromium): the reference
    vm, g, up = boot('s'); js = jsfn(g); sh = lambda c, t=60, show=True: run(g, c, t, show)
    ok('0. the release starts (Standard)', up)
    if not up: raise RuntimeError('did not start')
    end = time.time() + 600
    while time.time() < end and js("typeof acctNew=='function'&&typeof userLogin=='function'", 20) is not True: time.sleep(5)
    time.sleep(40); js(INIT_JS)
    r = js("(async()=>{ACCTS.length=0;const b=await acctNew('boss','b1','admin');ACCTS.push(b);saveUser();return b.id})()", 120); out('account', r)
    r = js("(async()=>{try{const a=ACCTS.find(x=>x.n=='boss');if(USER)userLogout();USER=a;const t=performance.now();await userLogin(a,'b1');lastInput=Date.now();"
           "const L=document.getElementById('login');if(L)L.style.display='none';const n=[...document.querySelectorAll('#desk > .ic')].filter(e=>e.offsetWidth).length;return 'in '+USER.n+' icons '+n+' after '+Math.round(performance.now()-t)+' ms'}catch(e){return 'ERR '+e.message}})()", 300)
    out('sign-in (Standard):', r); wait_for(js, "!!(USER&&USER.disk&&hdOn())", 300); time.sleep(10)
    m = re.search(r'icons (\d+)', str(r)); ok('S1. Standard: the desktop\'s icons once signed in (' + str(r) + ')', m and int(m.group(1)) > 4, r)
    r = js("(async()=>{const t=performance.now();deskNew(1);for(let i=0;i<500&&!document.querySelector('#desk > .ic.fic[data-fn=\"NEW FOLDER\"]');i++)await new Promise(r=>setTimeout(r,5));"
           "const d=Math.round(performance.now()-t);document.querySelectorAll('.rmodal').forEach(m=>m.remove());return d})()", 60)
    TM['std_newfolder_ms'] = r; ok('S2. Standard: New folder on the desktop at once (' + str(r) + ' ms)', isinstance(r, (int, float)) and r < 500, r)
    r = js("(async()=>{settings('disp');await new Promise(r=>setTimeout(r,1500));const b0=getComputedStyle(document.body).backgroundImage;const t=performance.now();"
           "document.querySelector('#dyns .dynb[data-dyn=teal]').click();for(let i=0;i<300&&getComputedStyle(document.body).backgroundImage==b0;i++)await new Promise(r=>setTimeout(r,5));"
           "const d=Math.round(performance.now()-t);secRevert('disp');await new Promise(r=>setTimeout(r,300));const back=getComputedStyle(document.body).backgroundImage==b0;wins.filter(w=>w.app=='settings').forEach(w=>{w._ok=1;w.close()});return d+' '+back})()", 90)
    TM['std_preview'] = r; ok('S3. Standard: a background chosen previewed on the desktop at once, back on Revert (' + str(r) + ')', str(r).endswith(' true'), r)
    js("(async()=>{try{fsFlush()}catch(e){}try{await homeChain}catch(e){}try{await __rudolfLsFlush()}catch(e){}return 1})()", 60)
    sh("echo native > /persist/etc/rudolf-desktop-engine; rm -f /persist/etc/rudolf-desktop-engine.failed; sync", 60)
    time.sleep(5); vm.stop(); time.sleep(3)

    # ------------------------------------------------------------------ Native
    vm, g, up = boot('n'); sh = lambda c, t=60, show=True: run(g, c, t, show)
    ok('0b. the release starts again (Native)', up)
    if not up: raise RuntimeError('did not start')
    o = sh("for i in $(seq 1 60); do pgrep -x rudolf-native >/dev/null && break; sleep 3; done; cat /run/rudolf/desktop-engine; pgrep -x rudolf-native", 240)
    ok('0c. the Native desktop runs', 'native' in o, o)
    o = sh("FX=; for d in /dev/nvme*n1; do blkid $d 2>/dev/null | grep -q K160FIX && FX=$d; done; echo fix=$FX; mkdir -p /tmp/fx; [ -n \"$FX\" ] && mount -o ro $FX /tmp/fx; ls -la /tmp/fx; "
           "WWW=/usr/share/rudolf/www; [ -f /persist/os/current/index.html ] && WWW=/persist/os/current; [ -f /tmp/fx/index.html ] && cp /tmp/fx/index.html /tmp/page.html && chmod 644 /tmp/page.html && mount --bind /tmp/page.html $WWW/index.html; "
           "rm -f /tmp/teval /tmp/teval.out /tmp/vw.log /tmp/vw.stop; mkfifo -m 666 /tmp/teval; "
           "mount --bind /tmp/fx/rn /usr/bin/rudolf-native; "
           "P0=$(pgrep -f 'rudolf-native [-]-rudolf-kiosk'); echo before $P0; kill $P0; sleep 3; kill -9 $P0 2>/dev/null; sleep 1; ls -la /usr/bin/rudolf-native; for i in $(seq 1 90); do [ -s /tmp/nstate ] && break; sleep 2; done; head -3 /tmp/nstate; "
           "(setsid sh /tmp/fx/vw.sh </dev/null >/dev/null 2>&1 &); sleep 1; pgrep -f vw.sh", 300)
    ok('0d. the native desktop started again with the tests\' views and the watcher', 'screen ' in o, o)
    state = lambda: sh("cat /tmp/nstate", 30, False)
    def st_get(st, k):
        m = re.search(rf'^{k} (.*)$', st, re.M); return m.group(1) if m else None
    st = state()
    if not st_get(st, 'screen'): raise RuntimeError('no state from the native desktop')
    SW, SH = [int(x) for x in st_get(st, 'screen').split()]
    def at(x, y, btn='left'): g.q.at(x, y, btn, SW, SH)
    def tev(key, expr, t=30):
        n0 = sh("wc -l < /tmp/teval.out 2>/dev/null || echo 0", 20, False).strip().split('\n')[-1]
        open(f'{W}/j.js', 'w').write(expr.replace('\n', ' ')); g.put(f'{W}/j.js', '/tmp/j.js', '644')
        o = sh(f"timeout 5 sh -c '{{ printf \"%s \" {key}; cat /tmp/j.js; echo; }} > /tmp/teval'; for i in $(seq 1 {t}); do n=$(wc -l < /tmp/teval.out 2>/dev/null || echo 0); [ $n -gt {n0 or 0} ] && break; sleep 1; done; grep '^{key} ' /tmp/teval.out | tail -1", t + 10, False)
        o = o.strip(); return o[len(key) + 1:] if o.startswith(key + ' ') else o
    def tev_at(key, expr):
        """the page asked (as tev), the moment written in the computer itself (seconds since its start): the serial line's own delay left out"""
        open(f'{W}/j.js', 'w').write(expr.replace('\n', ' ')); g.put(f'{W}/j.js', '/tmp/j.js', '644')
        o = sh(f"t=$(cut -d' ' -f1 /proc/uptime); timeout 5 sh -c '{{ printf \"%s \" {key}; cat /tmp/j.js; echo; }} > /tmp/teval'; echo AT $t", 30, False)
        m = re.search(r'AT ([0-9.]+)', o); return float(m.group(1)) if m else None
    def vwlog():
        """the watcher's log: [(seconds, {line kind: rest})]"""
        L = []; cur = None
        for l in sh("cat /tmp/vw.log", 30, False).split('\n'):
            l = l.strip()
            if l.startswith('T '):
                try: cur = (float(l[2:]), {}); L.append(cur)
                except ValueError: cur = None
            elif cur and ' ' in l: k, v = l.split(' ', 1); cur[1][k] = v
        return L
    def first(L, pred, after=0):
        for t, d in L:
            if t >= after and pred(d): return t
        return None
    # ---- 3: the icons after signing in
    if st_get(st, 'signin') == '1':
        time.sleep(3); g.q.type('b1'); time.sleep(0.5); g.q.key('ret')
        for _ in range(30):
            time.sleep(3); st = state()
            if st_get(st, 'user') == 'boss': break
    ok('0e. signed in on the native sign-in screen', st_get(st, 'user') == 'boss', st[:600])
    time.sleep(25); L = vwlog()
    tin = first(L, lambda d: (d.get('user') or '').startswith('boss')); tic = first(L, lambda d: int((d.get('dicons') or '0').split()[0]) > 4, tin or 0)
    TM['nat_icons_after_signin_s'] = None if tin is None or tic is None else round(tic - tin, 2)
    ok(f'N3a. Native: the desktop\'s icons once signed in ({TM["nat_icons_after_signin_s"]} s after the sign-in was accepted; Standard: at once), within 1 s', TM['nat_icons_after_signin_s'] is not None and TM['nat_icons_after_signin_s'] <= 1.0, (tin, tic))
    def hit(st, k):
        m = re.search(rf'^hit {k} (\d+) (\d+) (\d+) (\d+)$', st, re.M); return (int(m.group(1)) + int(m.group(3)) / 2, int(m.group(2)) + int(m.group(4)) / 2) if m else None
    # ---- 1: the app windows, watched from when each is made
    def judge(log):
        """log lines "I T V W H [S hex]": the shown ones at the last one's size, each the app as it ends up or plain"""
        rows = []
        for l in log.split('\n'):
            a = l.split()
            if len(a) >= 5 and a[0].isdigit(): rows.append((int(a[0]), int(a[1]), int(a[2]), int(a[3]), int(a[4]), a[6] if len(a) > 6 and a[5] == 'S' else ''))
        shown = [r for r in rows if r[2] == 1]
        if not shown: return {'err': 'never shown', 'rows': rows[-3:]}
        fin = shown[-1]; bad = []; fs = bytes.fromhex(fin[5]) if fin[5] else b''
        for r in shown:
            if (r[3], r[4]) != (fin[3], fin[4]): bad.append((r[1], f'{r[3]}x{r[4]}')); continue
            if not fs or not r[5]: continue
            s = bytes.fromhex(r[5]); diff = sum(1 for i in range(0, len(s), 3) if max(abs(s[i + c] - fs[i + c]) for c in range(3)) > 40) / (len(s) / 3)
            plain = max(max(s[c::3]) - min(s[c::3]) for c in range(3)) <= 8
            if diff > .06 and not plain: bad.append((r[1], f'differs {round(100 * diff)}%'))
        return {'bad': bad, 'first_ms': shown[0][1], 'size': f'{fin[3]}x{fin[4]}', 'before': sorted({f'{r[3]}x{r[4]}' for r in rows if r[2] == 0})}
    for k, q in (('notepad', 'notepad'), ('calc', 'calculator'), ('settings', 'settings'), ('explorer', 'file explorer')):
        sh(f"rm -rf /tmp/wc-{k}; mkdir -p /tmp/wc-{k}; (DISPLAY=:0 XAUTHORITY=/run/rudolf/xauth WINCAP_NOPPM=1 setsid /tmp/fx/wincap /tmp/nstate {k} 50 40 /tmp/wc-{k} </dev/null >/dev/null 2>&1 &)", 20, False)
        st = state(); h = hit(st, 'start \\S+')
        if h: at(*h); time.sleep(1.5); g.q.type(q); time.sleep(1.5); g.q.key('ret')
        time.sleep(14); lg = sh(f"cat /tmp/wc-{k}/log", 30, False); j = judge(lg); TM[f'nat_win_{k}'] = j
        ok(f'N1. Native: {k}\'s window on the screen only at its own size, its page the app or a plain colour ({j})', not j.get('err') and not j['bad'], lg[-1500:])
        vm.shot(f'win-{k}'); tev(k, "(()=>{window.__oneAppClose();return 1})()", 10); time.sleep(4)
    # ---- 2: menus replaced by others
    def box(s): return [int(x) for x in s.split()[:4]] if s else None
    for name, a, b in (('desktop-then-desktop', (SW * 0.6, SH * 0.3), (SW * 0.5, SH * 0.25)), ('desktop-then-taskbar', (SW * 0.3, SH * 0.45), (SW * 0.3 + 120, SH - 20))):
        g.q.key('esc'); time.sleep(1); ref = vm.shot(f'menu-{name}-ref')
        at(*a, btn='right'); time.sleep(2); b1 = box(st_get(state(), 'cmenu'))
        at(*b, btn='right'); time.sleep(1.5); now = vm.shot(f'menu-{name}'); b2 = box(st_get(state(), 'cmenu'))
        n, t = far_left(ref, now, b1, [b2[0] - 5, b2[1] - 5, b2[0] + b2[2] + 5, b2[1] + b2[3] + 5]) if b1 and b2 else (-1, 0)
        ok(f'N2. Native: {name}: nothing of the first menu left on the screen ({n} of {t} pixels)', n == 0, (b1, b2))
        g.q.key('esc'); time.sleep(1)
    # ---- 3: the Desktop folder's changes
    for item, name, key in (('New folder', 'NEW FOLDER', 'nat_newfolder_s'), ('New text document', 'NEW TEXT DOCUMENT.TXT', 'nat_newtext_s')):
        g.q.key('esc'); time.sleep(1); at(SW // 2, SH // 3, 'right'); time.sleep(2); st = state()
        m = re.search(rf'^cmitem (\d+) (\d+) \d+ [^|]*\|{item}$', st, re.M)
        if m: at(int(m.group(1)), int(m.group(2)))
        time.sleep(6); L = vwlog()
        tc = None
        for i in range(1, len(L)):
            if 'cmenu' in L[i - 1][1] and 'cmenu' not in L[i][1]: tc = L[i][0]
        ti = first(L, lambda d: name + ',' in (d.get('dicons') or ''), tc or 0)
        TM[key] = None if tc is None or ti is None else round(ti - tc, 2)
        ok(f'N3b. Native: {item} on the desktop within 500 ms ({TM[key]} s after the menu\'s click)', TM[key] is not None and TM[key] <= 0.5, (tc, ti))
        g.q.key('esc'); time.sleep(2)
    st = state(); h = hit(st, 'start \\S+')
    if h: at(*h); time.sleep(1.5); g.q.type('notepad'); time.sleep(1.5); g.q.key('ret')
    time.sleep(12)
    for what, js1, pred in (('saved by an app', "(()=>{const d=deskFiles();d['SAVED.TXT']='hello';fsSave();EXPL.forEach(f=>f());return 1})()", lambda d: 'SAVED.TXT,' in (d.get('dicons') or '')),
                            ('deleted', "(()=>{const d=deskFiles();delete d['SAVED.TXT'];fsSave();EXPL.forEach(f=>f());return 1})()", lambda d: 'SAVED.TXT,' not in (d.get('dicons') or ''))):
        t0 = tev_at('notepad', js1); time.sleep(5); L = vwlog()
        t1 = first(L, pred, t0 or 0) if t0 else None; TM['nat_file_' + what.split()[0]] = None if t1 is None else round(t1 - t0, 2)
        ok(f'N3c. Native: a file {what} on the desktop within 500 ms of the page being asked ({TM["nat_file_" + what.split()[0]]} s)', t1 is not None and t1 - t0 <= 0.5, (t0, t1))
    tev('notepad', "(()=>{window.__oneAppClose();return 1})()", 10); time.sleep(4)
    # ---- 4: Display Properties' background, previewed
    st = state(); h = hit(st, 'start \\S+')
    if h: at(*h); time.sleep(1.5); g.q.type('settings'); time.sleep(1.5); g.q.key('ret')
    time.sleep(15); w0 = st_get(state(), 'wallpaper'); vm.shot('wall-0')
    t0 = tev_at('settings', "(()=>{document.querySelector('#dyns .dynb[data-dyn=teal]').click();return S.dyn})()"); time.sleep(3)
    L = vwlog(); t1 = first(L, lambda d: (d.get('wallpaper') or '').startswith('teal|') and (d.get('wallpaper') or '').endswith('prev=1'), t0 or 0) if t0 else None; vm.shot('wall-teal')
    TM['nat_preview_s'] = None if t1 is None else round(t1 - t0, 2)
    ok(f'N4a. Native: a background chosen in Display Properties on the desktop within 300 ms of the page being asked ({TM["nat_preview_s"]} s; Standard {TM.get("std_preview")} ms)', t1 is not None and t1 - t0 <= 0.3, (w0, t0, t1))
    tev('settings', "(()=>{secRevert('disp');return S.dyn})()"); time.sleep(3); w2 = st_get(state(), 'wallpaper'); vm.shot('wall-revert')
    ok('N4b. Native: ...back on Revert', w2 == w0, (w0, w2))
    out('timings', json.dumps(TM))
    out(sh("grep -v 'dbus\\|Warning\\|vkEnum\\|dawn' /run/rudolf/native-desktop.log | tail -30", 30, False))
except RuntimeError as e:
    ok('the run went on: ' + str(e), False)
finally:
    try:
        if vm: vm.stop()
    except Exception: pass
open('v160-timings.json', 'w').write(json.dumps(TM, indent=1))
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
