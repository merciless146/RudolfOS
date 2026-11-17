# Rudolf OS 1.6.0 on the real release in QEMU (docs/plan-1.6.0.md Part 12, item 12): the Native desktop's background work runs once,
# whatever is open, as on Standard. An alarm and a Calendar reminder a minute ahead, a Clock timer whose window is closed:
#   1. Standard (the reference): the alarm rings once (one notification in the page's centre)
#   2. Native: the desktop's service page (#app=do-svc) is up after sign-in, unseen (not on the taskbar); with no app window open the
#      alarm rings once and the reminder shows once (the shell's toasts); with three app windows open (Notepad, Calculator, Settings)
#      once again, from the service page; a Clock timer started in the Clock's window, the window closed: it rings once; the service
#      page closed: opened again; an app's window runs only the timers it needs (svc60.js); the pages' memory
#   python3 s160_t.py REL-DIR    (SB_PARTS, QEMU, QEMU_ACCEL as for t160; root)
#   S160_FIX=DIR: DIR/rudolf-native and DIR/index.html (this tree's, built) put in place of the release's (an extra disk)
import os, sys, time, re, subprocess, shutil, struct, json
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); FIX = os.environ.get('S160_FIX', '')
LOG = open('s160.txt', 'w'); R = []; T0 = time.time()
def out(*a):
    s = ' '.join(str(x) for x in a); s = f'[{int(time.time() - T0):5d}s] ' + s; print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:1500] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 3000; DRV = os.path.abspath('s160.img'); MEM = int(os.environ.get('MEM', '4096')); W = os.path.abspath('w160s')
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
    # (with a wrapper that starts the program with the tests' views: run from this disk, as /tmp may not allow programs)
    FD = os.path.join(W, 'fix'); shutil.copytree(FIX, FD)
    open(f'{FD}/rn', 'w').write('#!/bin/sh\nexport RUDOLF_NATIVE_STATE=/tmp/nstate RUDOLF_NATIVE_TEST_EVAL=/tmp/teval\nexec /tmp/fx/rudolf-native "$@"\n'); os.chmod(f'{FD}/rn', 0o755)
    if os.path.exists(f'{FD}/rudolf-native'): os.chmod(f'{FD}/rudolf-native', 0o755)
    FIXIMG = os.path.abspath('s160fix.img'); subprocess.run(['truncate', '-s', '64M', FIXIMG], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'S160FIX', '-d', FD, FIXIMG], check=True)
INIT_JS = "(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};return 1})()"
def boot(tag):
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf160',
             '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1',
             '-audiodev', 'none,id=snd0', '-device', 'intel-hda', '-device', 'hda-duplex,audiodev=snd0']
    if FIXIMG: extra += ['-drive', f'file={FIXIMG},format=raw,if=none,id=fx,readonly=on', '-device', 'nvme,drive=fx,serial=s160fix']
    vm = sbvm.VM('s160' + tag, PARTS, [], secure=False, mem=MEM, extra=extra)
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
TOK = "$(busybox wget -q -O- http://127.0.0.1:8080/ | grep -o 'token:\"[0-9a-f]*' | cut -d'\"' -f2)"
def api(sh, p, post=False):
    return sh(f"busybox wget -q -O- --header \"X-Rudolf-Token: {TOK}\" {'--post-data= ' if post else ''}http://127.0.0.1:8080/api/{p}", 30, False)

make_drive(DRV)
vm = None
try:
    # ------------------------------------------------------------------ 1. Standard (Chromium): the reference
    vm, g, up = boot('s'); js = jsfn(g); sh = lambda c, t=60, show=True: run(g, c, t, show)
    ok('0. the release starts (Standard)', up)
    if not up: raise RuntimeError('did not start')
    end = time.time() + 600
    while time.time() < end and js("typeof acctNew=='function'&&typeof userLogin=='function'", 20) is not True: time.sleep(5)
    time.sleep(40); js(INIT_JS)
    r = js("(async()=>{ACCTS.length=0;const b=await acctNew('boss','b1','admin');ACCTS.push(b);saveUser();return b.id})()", 120); out('account', r)
    r = js("(async()=>{try{const a=ACCTS.find(x=>x.n=='boss');if(USER)userLogout();USER=a;await userLogin(a,'b1');lastInput=Date.now();"
           "const L=document.getElementById('login');if(L)L.style.display='none';return 'in '+USER.n}catch(e){return 'ERR '+e.message}})()", 300)
    out('sign-in (Standard):', r); wait_for(js, "!!(USER&&USER.disk&&hdOn())", 300); time.sleep(10)
    out('the input devices rudolf-display reads:', sh("cat /run/rudolf/inputs 2>/dev/null | head -20", 30, False))
    # an alarm this minute's next: rung once by the page (its centre's list, as Standard keeps it)
    hm = sh("date -d '+70 sec' +%H:%M", 20, False).strip().split('\n')[-1]; out('alarm (Standard) at', hm)
    js(f"(()=>{{svj(ALK(),[{{t:'{hm}',label:'Wake',days:[],on:true}}]);return 1}})()")
    r = wait_for(js, "NC.filter(n=>n.t=='⏰ Wake').length>=1", 180, 3); time.sleep(8)
    n = js("NC.filter(n=>n.t=='⏰ Wake').length")
    ok('S1. Standard: an alarm set a minute ahead rings once (one notification)', n == 1, (r, n))
    js("(async()=>{try{wins.slice().forEach(w=>w.close())}catch(e){}try{fsFlush()}catch(e){}try{await homeChain}catch(e){}try{await __rudolfLsFlush()}catch(e){}return 1})()", 60)
    sh("echo native > /persist/etc/rudolf-desktop-engine; rm -f /persist/etc/rudolf-desktop-engine.failed; sync", 60)
    time.sleep(5); vm.stop(); time.sleep(3)

    # ------------------------------------------------------------------ 2. Native
    vm, g, up = boot('n'); sh = lambda c, t=60, show=True: run(g, c, t, show)
    ok('0b. the release starts again (Native)', up)
    if not up: raise RuntimeError('did not start')
    o = sh("for i in $(seq 1 60); do pgrep -x rudolf-native >/dev/null && break; sleep 3; done; cat /run/rudolf/desktop-engine; pgrep -x rudolf-native", 240)
    ok('0c. the Native desktop runs', 'native' in o, o)
    o = sh("FX=; for d in /dev/nvme*n1; do blkid $d 2>/dev/null | grep -q S160FIX && FX=$d; done; echo fix=$FX; mkdir -p /tmp/fx; [ -n \"$FX\" ] && mount -o ro $FX /tmp/fx; ls -la /tmp/fx; "
           "WWW=/usr/share/rudolf/www; [ -f /persist/os/current/index.html ] && WWW=/persist/os/current; [ -f /tmp/fx/index.html ] && cp /tmp/fx/index.html /tmp/page.html && chmod 644 /tmp/page.html && mount --bind /tmp/page.html $WWW/index.html; "
           "rm -f /tmp/teval /tmp/teval.out; mkfifo -m 666 /tmp/teval; mount | grep -E ' /tmp | /run '; "
           "mount --bind /tmp/fx/rn /usr/bin/rudolf-native; "
           "P0=$(pgrep -f 'rudolf-native [-]-rudolf-kiosk'); echo before $P0; kill $P0; sleep 3; kill -9 $P0 2>/dev/null; sleep 1; echo after $(pgrep -f 'rudolf-native [-]-rudolf-kiosk'); ls -la /usr/bin/rudolf-native; for i in $(seq 1 90); do [ -s /tmp/nstate ] && break; sleep 2; done; head -3 /tmp/nstate; pgrep -af rudolf-native; tail -8 /run/rudolf/native-desktop.log; tail -8 /run/rudolf/session.log 2>/dev/null", 300)
    ok('0d. the native desktop started again with the tests\' views' + (' and this tree\'s files' if FIX else ''), 'screen ' in o, o)
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
    if st_get(st, 'signin') == '1':
        time.sleep(3); g.q.type('b1'); time.sleep(0.5); g.q.key('ret')
        for _ in range(30):
            time.sleep(3); st = state()
            if st_get(st, 'user') == 'boss': break
    ok('0e. signed in on the native sign-in screen', st_get(st, 'user') == 'boss', st[:600])
    time.sleep(20)
    def hit(st, k):
        m = re.search(rf'^hit {k} (\d+) (\d+) (\d+) (\d+)$', st, re.M); return (int(m.group(1)) + int(m.group(3)) / 2, int(m.group(2)) + int(m.group(4)) / 2) if m else None
    def open_app(k, q, wait=40):
        st = state(); h = hit(st, 'start \\S+')
        if h: at(*h); time.sleep(1.5); g.q.type(q); time.sleep(1.5); g.q.key('ret')
        for _ in range(wait):
            time.sleep(1)
            if re.search(rf'^client .* app={k} ', state(), re.M): return True
        return False
    def toasts(t): return len(re.findall(rf'^toasted \d+ [^|]*\|{re.escape(t)}$', state(), re.M))
    def tfrom(t): return sorted(set(re.findall(rf'^toasted \d+ ([^|]*)\|{re.escape(t)}$', state(), re.M)))
    def svc(): return st_get(state(), 'svc') or ''
    for _ in range(60):
        if svc().startswith('ready'): break
        time.sleep(3)
    st = state()
    ok('N1. Native: the desktop\'s service page is up after sign-in, unseen (no window, no taskbar button)', svc().startswith('ready') and 'app=do-svc' not in st and not re.search(r'^hit win ', st, re.M), (svc(), [l for l in st.split('\n') if l.startswith(('client', 'hit win'))]))
    def ahead(label, cal):
        hm = sh("date -d '+70 sec' +%H:%M; date -d '+70 sec' +%Y-%m-%d", 20, False).strip().split('\n')[-2:]
        r = tev('do-svc', f"(()=>{{svj(ALK(),[{{t:'{hm[0]}',label:'{label}',days:[],on:true}}]);svj(CALK(),[{{id:Date.now(),title:'{cal}',d:'{hm[1]}',t:'{hm[0]}',rem:0}}]);return 1}})()")
        out('alarm and reminder at', hm, r); return hm[0]
    def rings(t, secs=200):
        end = time.time() + secs
        while time.time() < end and toasts(t) < 1: time.sleep(4)
        time.sleep(10); return toasts(t), tfrom(t)
    a = ahead('Wake', 'Dentist'); n = rings('⏰ Wake'); c = rings('📅 Dentist', 30)
    ok('N2. Native, no app window open: the alarm rings once (the service page\'s)', n == (1, ['do-svc']), (a, n))
    ok('N3. Native, no app window open: the Calendar\'s reminder shows once', c == (1, ['do-svc']), c)
    for k, q in (('notepad', 'notepad'), ('calc', 'calculator'), ('settings', 'settings')): out('open', k, open_app(k, q))
    time.sleep(10); st = state()
    ok('N4. Native: three app windows open', all(re.search(rf'^client .* app={k} ', st, re.M) for k in ('notepad', 'calc', 'settings')), [l for l in st.split('\n') if l.startswith('client')])
    a = ahead('Second', 'Meeting'); n = rings('⏰ Second'); c = rings('📅 Meeting', 30)
    ok('N5. Native, three windows open: the alarm rings ONCE (the service page\'s, not each window\'s)', n == (1, ['do-svc']), (a, n))
    ok('N6. Native, three windows open: the reminder once', c == (1, ['do-svc']), c)
    l = tev('notepad', "JSON.stringify(SVC.held.filter(h=>!h.gone).map(h=>[h.mod,h.real!=null?1:0]))")
    try: L = json.loads(l)
    except Exception: L = []
    run = lambda m: sum(1 for x in L if x[0] == m and x[1])
    ok('N7. Native: Notepad\'s window runs only what it needs (the theme\'s ticks; not the alarms\', the updates\', the session\'s save)', L and run('a11y25.js') >= 2 and run('apps25b.js') == 0 and run('term26.js') == 0 and run('shell25.js') == 0, l[:600])
    out('the pages\' process:', sh("for p in $(pgrep -f WebKitWebProcess); do grep -E 'VmRSS' /proc/$p/status | tr '\\n' ' '; echo $p; done", 30, False))
    # a Clock timer started in its window, the window closed
    if open_app('clock', 'clock'):
        time.sleep(6)
        r = tev('clock', "(()=>{document.querySelector('.tmtabs [data-t=timer]').click();const q=s=>document.querySelector('.clb '+s);q('.tm').value=0;q('.ts').value=40;q('.tl').value='Tea';q('.ta').click();const n=TIMERS.length;setTimeout(()=>window.__oneAppClose(),1500);return n})()")
        time.sleep(8); st = state(); out('timer:', r, 'clock open:', bool(re.search(r'^client .* app=clock ', st, re.M)))
        ok('N8. Native: a Clock timer started (40 s), its window closed', r.strip().endswith('1') and not re.search(r'^client .* app=clock ', st, re.M), r)
        n = rings('⏰ Tea', 120)
        ok('N9. Native: the timer rings once, its window closed (the service page\'s)', n == (1, ['do-svc']), n)
    else: ok('N8. Native: the Clock opened', False, state()[:800])
    s0 = svc(); tev('do-svc', "(()=>{oaPost({op:'close'});return 1})()", 10)
    for _ in range(40):
        time.sleep(3)
        if svc().startswith('ready') and svc() != s0: break
    ok('N10. Native: the service page closed: opened again by the shell, signed in', svc().startswith('ready') and svc() != s0, (s0, svc()))
    vm.shot('nat-svc')
    out(sh("grep -iE 'service page|toast' /run/rudolf/native-desktop.log | tail -20", 30, False))
except RuntimeError as e:
    ok('the run went on: ' + str(e), False)
finally:
    try:
        if vm: vm.stop()
    except Exception: pass
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
