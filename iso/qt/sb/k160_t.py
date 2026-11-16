# Rudolf OS 1.6.0 on the real release in QEMU: the power key, the hardware keys, Ctrl+Alt+Del and quick settings on the Native
# desktop, against Standard (the owner's Vivobook reports of 2026-11-15, beta 1.6.0.261116). The keys go in as the computer's own:
# QEMU's keyboard (send-key: volumeup, volumedown, audiomute, ctrl-alt-delete; the Vivobook's Fn+F1/F2/F3 are the same evdev keys,
# KEY_MUTE, KEY_VOLUMEDOWN, KEY_VOLUMEUP, from asus-nb-wmi) and its power button (system_powerdown: ACPI's power button, KEY_POWER),
# read by rudolf-display as on the Vivobook. QEMU has no backlight, so the brightness keys' path is checked from rudolfd's event
# ("osd brightness N", as hotkey writes it) and the "none" answer.
#   1. Standard (the reference): the power key shows the page's power dialog, the volume keys the page's indicator and change the
#      volume, Ctrl+Alt+Del Task Manager above every window
#   2. Native: with the desktop, an app's window (Notepad, Calculator open) and Oracle in front: the power key's dialog at once (within
#      3 s), drawn by the shell above every window, once; never left for a page (rudolfd's events file empty, no page's .rmodal); the
#      volume keys change the volume with the shell's indicator; Ctrl+Alt+Del's Task Manager above every window; the brightness's
#      indicator from the event
#   3. Native's quick settings: each tile with two app windows open: the windows' pages, the shell's state, the X screen's gamma
#      (Night light, High contrast: xrandr), Lock over every window, Power's choices
#   python3 k160_t.py REL-DIR    (SB_PARTS, QEMU, QEMU_ACCEL as for t160; root)
#   K160_FIX=DIR: DIR/rudolf-native and DIR/index.html (this tree's, built) put in place of the release's (an extra disk)
import os, sys, time, re, subprocess, shutil, struct, json
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); FIX = os.environ.get('K160_FIX', '')
LOG = open('k160.txt', 'w'); R = []; T0 = time.time()
def out(*a):
    s = ' '.join(str(x) for x in a); s = f'[{int(time.time() - T0):5d}s] ' + s; print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:1500] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 3000; DRV = os.path.abspath('k160.img'); MEM = int(os.environ.get('MEM', '4096')); W = os.path.abspath('w160k')
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
    FIXIMG = os.path.abspath('k160fix.img'); subprocess.run(['truncate', '-s', '64M', FIXIMG], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'K160FIX', '-d', FD, FIXIMG], check=True)
INIT_JS = "(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};return 1})()"
def boot(tag):
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf160',
             '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1',
             '-audiodev', 'none,id=snd0', '-device', 'intel-hda', '-device', 'hda-duplex,audiodev=snd0']
    if FIXIMG: extra += ['-drive', f'file={FIXIMG},format=raw,if=none,id=fx,readonly=on', '-device', 'nvme,drive=fx,serial=k160fix']
    vm = sbvm.VM('k160' + tag, PARTS, [], secure=False, mem=MEM, extra=extra)
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
    api(sh, 'volume?set=50', True); v0 = api(sh, 'volume'); g.q.cmd('system_powerdown'); t1 = time.time()
    r = wait_for(js, "document.querySelectorAll('.rmodal').length>0", 15, 0.5); out('Standard power dialog after', round(time.time() - t1, 1), 's')
    ok('S1. Standard: the power button shows the page\'s power dialog', r is True, r)
    js("document.querySelectorAll('.rmodal').forEach(e=>e.remove());1")
    g.q.key('volumeup'); r = wait_for(js, "(()=>{const o=document.getElementById('osd');return !!o&&o.style.display!='none'})()", 10, 0.5); v1 = api(sh, 'volume')
    ok('S2. Standard: the volume-up key shows the page\'s indicator and changes the volume', r is True and v1 != v0, (r, v0, v1))
    g.q.key('ctrl', 'alt', 'delete'); r = wait_for(js, "wins.some(w=>w.app=='taskmgr'&&w.el.classList.contains('ontop'))", 20, 1)
    ok('S3. Standard: Ctrl+Alt+Del opens Task Manager above every window', r is True, r)
    vm.shot('std-taskmgr')
    js("(async()=>{try{wins.slice().forEach(w=>w.close())}catch(e){}try{fsFlush()}catch(e){}try{await homeChain}catch(e){}try{await __rudolfLsFlush()}catch(e){}return 1})()", 60)
    sh("echo native > /persist/etc/rudolf-desktop-engine; rm -f /persist/etc/rudolf-desktop-engine.failed; sync", 60)
    time.sleep(5); vm.stop(); time.sleep(3)

    # ------------------------------------------------------------------ 2. Native
    vm, g, up = boot('n'); sh = lambda c, t=60, show=True: run(g, c, t, show)
    ok('0b. the release starts again (Native)', up)
    if not up: raise RuntimeError('did not start')
    o = sh("for i in $(seq 1 60); do pgrep -x rudolf-native >/dev/null && break; sleep 3; done; cat /run/rudolf/desktop-engine; pgrep -x rudolf-native", 240)
    ok('0c. the Native desktop runs', 'native' in o, o)
    o = sh("FX=; for d in /dev/nvme*n1; do blkid $d 2>/dev/null | grep -q K160FIX && FX=$d; done; echo fix=$FX; mkdir -p /tmp/fx; [ -n \"$FX\" ] && mount -o ro $FX /tmp/fx; ls -la /tmp/fx; "
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
    for k, q in (('notepad', 'notepad'), ('calc', 'calculator')): out('open', k, open_app(k, q))
    time.sleep(10)
    ok('B0. the brightness button: shown when the screen has a backlight (QEMU has none: rudolfd\'s info)', ('hit bri ' in state()) == ('"backlight":true' in api(sh, 'info')), api(sh, 'info')[:300])
    fronts = [('the desktop', lambda: (g.q.key('meta_l', 'd'), time.sleep(2))), ('Notepad', lambda: (tev('notepad', "(()=>{document.querySelector('textarea').focus();return 1})()"), time.sleep(1))), ('Oracle', None)]
    for fn, front in fronts:
        if front is None:
            if not open_app('browser', 'oracle', 60): out('Oracle did not open'); continue
            time.sleep(15)
        else: front()
        st = state(); out('in front:', fn, [l for l in st.split('\n') if 'focus=1' in l])
        g.q.cmd('system_powerdown'); t1 = time.time(); got = None
        for _ in range(12):
            time.sleep(0.5); st = state()
            if st_get(st, 'power') == '1': got = round(time.time() - t1, 1); break   # (each reading over the serial line takes 2-3 s)
        left = sh("cat /run/rudolf/events 2>/dev/null | head -5; echo END", 20, False)
        inpage = tev('notepad', "document.querySelectorAll('.rmodal').length").split('\n')[-1].replace('notepad ', '')
        ok(f'B1. Native, {fn} in front: the power button shows the shell\'s power dialog at once (at the first readings), above every window, in no page', got is not None and got <= 8 and st_get(st, 'powertop') == '1' and left.strip() == 'END' and inpage == '0', (got, st_get(st, 'powertop'), left, inpage))
        vm.shot(f'nat-power-{fn.split()[-1]}'); g.q.key('esc'); time.sleep(1.5)
        api(sh, 'volume?set=50', True); st0 = state(); v0 = api(sh, 'volume'); g.q.key('volumeup'); time.sleep(1.2); st = state(); v1 = api(sh, 'volume'); o1 = st_get(st, 'osdlast') or ''
        ok(f'B2. Native, {fn} in front: the volume-up key (Fn+F3) changes the volume with the shell\'s indicator', v1 != v0 and 'volume 55|' in o1 and o1 != (st_get(st0, 'osdlast') or ''), (v0, v1, o1))
        time.sleep(2); g.q.key('volumedown'); time.sleep(1.2); st = state(); v2 = api(sh, 'volume'); o2 = st_get(st, 'osdlast') or ''
        ok(f'B3. Native, {fn} in front: the volume-down key (Fn+F2) too', v2 != v1 and o2 != o1 and 'volume 50|' in o2, (v1, v2, o2))
        time.sleep(2); sh("echo 'osd brightness 60' >> /run/rudolf/events", 20, False); time.sleep(1.5); st = state()
        ok(f'B4. Native, {fn} in front: the brightness keys\' event shows the shell\'s indicator', 'brightness 60|' in (st_get(st, 'osdlast') or ''), st_get(st, 'osdlast'))
        time.sleep(2); g.q.key('ctrl', 'alt', 'delete'); tm = False
        for _ in range(30):
            time.sleep(1); st = state()
            if re.search(r'^client .* focus=1 app=taskmgr ', st, re.M) and re.search(r'^ontop taskmgr$', st, re.M): tm = True; break
        ok(f'B5. Native, {fn} in front: Ctrl+Alt+Del opens Task Manager in front, above every window', tm, [l for l in st.split('\n') if l.startswith(('client', 'ontop'))])
        vm.shot(f'nat-taskmgr-{fn.split()[-1]}')
        tev('taskmgr', "(()=>{window.__oneAppClose();return 1})()", 10); time.sleep(4)
    # a while later: nothing shows the power dialog again (the old bug: Oracle drew it minutes later)
    time.sleep(60); st = state(); ok('B6. a minute later no window shows the power dialog again', st_get(st, 'power') == '0' and tev('notepad', "document.querySelectorAll('.rmodal').length") == '0', st_get(st, 'power'))
    # ------------------------------------------------------------------ 3. quick settings with two app windows open
    SIG = "(()=>{const h=document.documentElement,n=document.getElementById('nightl');return JSON.stringify({dark:h.classList.contains('dark'),nl:!!(n&&getComputedStyle(n).display!='none'),dnd:!!S.dnd,sr:h.classList.contains('sr'),mag:/scale/.test(document.body.style.transform||''),hc:/contrast|saturate/.test(h.style.filter||''),touch:h.classList.contains('touch')})})()"
    QSL = ['Dark mode', 'Night light', 'Do not disturb', 'Screen reader', 'Magnifier', 'High contrast', 'On-screen keyboard', 'Touch mode', 'Airplane mode']
    for i, t in enumerate(QSL):
        for on in (1, 0):
            pid0 = sh("pgrep -f 'rudolf-native [-]-rudolf-kiosk'", 20, False).strip()
            st = state(); h = hit(st, 'qs -')
            if h: at(*h)
            for _ in range(8):
                time.sleep(1); st = state()
                if (st_get(st, 'fly') or '').startswith('qs'): break
            m = re.search(rf'^flybtn qs:{i} (\d+) (\d+)$', st, re.M)
            if m: at(int(m.group(1)), int(m.group(2)))
            else: out('quick settings did not open:', st_get(st, 'fly'))
            time.sleep(1); at(SW * 3 // 4, SH - 140); time.sleep(9); st = state()   # (a click on the desktop closes quick settings, as on Standard)
            gm = sh("DISPLAY=:0 XAUTHORITY=/run/rudolf/xauth xrandr --verbose 2>/dev/null | grep -m1 Gamma", 20, False).strip()
            sigs = {k: tev(k, SIG) for k in ('notepad', 'calc')}
            out(f'{t} {"on" if on else "off"}:', st_get(st, 'a11y'), st_get(st, 'gamma'), gm, sigs)
            if on: vm.shot(f'nat-tile-{i}')
            a = dict(kv.split('=') for kv in (st_get(st, 'a11y') or '').split() if '=' in kv)
            key = ['dark', 'nl', 'dnd', 'sr', 'mag', 'hc', 'vk', 'touch', None][i]
            pid1 = sh("pgrep -f 'rudolf-native [-]-rudolf-kiosk'", 20, False).strip()
            if pid1 != pid0: out('THE NATIVE DESKTOP RESTARTED', pid0, pid1, sh("grep -v '^+' /run/rudolf/session.log | tail -15; dmesg | grep -iE 'segfault|traps|killed process|oom' | tail -5; grep -v 'dbus\\|Warning\\|vkEnum\\|dawn' /run/rudolf/native-desktop.log | tail -25", 30, False))
            if key: ok(f'Q{i}. Native: {t} {"on" if on else "off"}: the shell', a.get(key) == str(on) and pid1 == pid0, a)
    def qs_btn(k):
        st = state(); h = hit(st, 'qs -')
        if h: at(*h)
        time.sleep(3); st = state(); m = re.search(rf'^flybtn {k} (\d+) (\d+)$', st, re.M)
        if m: at(int(m.group(1)), int(m.group(2)))
        time.sleep(3); return state()
    st = qs_btn('power')
    ok('Q9. Native: quick settings › Power: the power dialog above every window, Standard\'s choices', st_get(st, 'power') == '1' and st_get(st, 'powertop') == '1', st_get(st, 'powerrows'))
    g.q.key('esc'); time.sleep(1.5)
    st = qs_btn('lock')
    ok('Q10. Native: quick settings › Lock: the lock screen above every window', st_get(st, 'locked') == '1' and st_get(st, 'signtop') == '1', (st_get(st, 'locked'), st_get(st, 'signtop')))
    vm.shot('nat-lock')
    out(sh("tail -40 /run/rudolf/native-desktop.log", 30, False))
except RuntimeError as e:
    ok('the run went on: ' + str(e), False)
finally:
    try:
        if vm: vm.stop()
    except Exception: pass
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
