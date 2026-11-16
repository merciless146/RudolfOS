# Rudolf OS 1.6.0 in QEMU: the native desktop against Standard on a real release (docs/native-findings-2026-11-15.md,
# "Status on 2026-11-16", the areas never compared). One computer, started twice: Standard (the desktop page in Chromium),
# then Native (rudolf-native) with the same account and settings, each with a sound card (intel-hda, hda-duplex), two
# screens (virtio-vga, two outputs) and sleep (S3). The same steps on each, and what each shows compared:
#   the sign-in and the desktop, Settings' Bluetooth page with no adapter, Settings' Sound page (the sound card), the
#   second screen, the desktop after sleep and waking (QEMU's system_wakeup), what the system sees (sound cards, screens,
#   Bluetooth, printers, batteries) and the desktop's log on each.
# Native is driven as a person would (QMP's keyboard and pointer); where to click is read from Standard's page (the
# rule: Native draws everything where Standard does). Pictures are compared with ../native/pxdiff.py; each is also
# printed in the log as a small PNG in base64 (between "PNG name" and "END"), as the run's pictures cannot always be
# downloaded.
#   python3 t160n_t.py REL-DIR   (SB_PARTS and QEMU as for t160; root; QEMU_ACCEL=kvm where it can)
import os, sys, time, re, subprocess, shutil, struct, json, base64, io
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE); ISO = os.path.dirname(QT)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1])
PARTS = os.environ.get('SB_PARTS', '/nonexistent'); LOG = open('t160n.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:600] if d != '' and not c else ''))
def note(n, d): out('NOTE ' + n + ': ' + str(d)[:1500])
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 3000; DRV = os.path.abspath('n160.img'); MEM = int(os.environ.get('MEM', '4096')); W = os.path.abspath('w160n')
shutil.rmtree(W, ignore_errors=True); os.makedirs(W)

def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    d = f'{T}/os/system/a'; os.makedirs(d)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs', 'manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{d}/{n}')
    open(f'{d}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n"); open(f'{d}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    # (Standard first: an update keeps the engine the computer had, and a computer with none is Standard)
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)

INIT_JS = "(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};return 1})()"
DEV = ['-vga', 'none', '-device', 'virtio-vga,id=vga0,max_outputs=2,xres=1280,yres=800', '-audiodev', 'none,id=snd0', '-device', 'intel-hda', '-device', 'hda-duplex,audiodev=snd0',
       '-global', 'ICH9-LPC.disable_s3=0']
def boot(tag, std=True):
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf160n',
             '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1'] + DEV
    vm = sbvm.VM('n160' + tag, PARTS, [], secure=False, mem=MEM, extra=extra)
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
    if up and std:
        end = time.time() + 600
        while time.time() < end and js("typeof acctNew=='function'&&typeof userLogin=='function'", 20) is not True: time.sleep(5)
        js(INIT_JS)
        o = sh_(g, "last=; same=0; i=0; while [ $i -lt 40 ]; do i=$((i+1)); v=$(timeout 20 cdp-eval 'Math.round(performance.timeOrigin)' 2>/dev/null | grep -o '\"value\":[0-9]*' | head -1); "
                   "if [ -n \"$v\" ] && [ \"$v\" = \"$last\" ]; then same=$((same+5)); else same=0; last=$v; fi; [ $same -ge 20 ] && break; sleep 5; done; echo \"SETTLED $same\"", 1100)
        js(INIT_JS)
    return vm, g, up, js
def sh_(g, c, t=60):
    f = os.path.join(W, 'c.sh'); open(f, 'w').write(c + '\n'); g.put(f, '/tmp/c.sh', '644')
    o = g.sh('sh /tmp/c.sh 2>&1', t); out('$', c[:200].replace('\n', ' ; ')); out(o.rstrip()[-3000:]); return o

# (a picture in the log: small, so the run's log can be read for it)
def logpng(name, f, w=480):
    try:
        from PIL import Image
        im = Image.open(f).convert('RGB'); im = im.resize((w, max(1, im.height * w // im.width))); b = io.BytesIO(); im.save(b, 'PNG', optimize=True)
        out('PNG ' + name); s = base64.b64encode(b.getvalue()).decode()
        for i in range(0, len(s), 4000): out(s[i:i + 4000])
        out('END')
    except Exception as e: out('(no picture: ' + str(e) + ')')
def shot(vm, tag, head=0):
    vm.n += 1; f = os.path.join(vm.dir, f'{vm.n:02d}_{tag}.png')
    r = vm.q.cmd('screendump', filename=f, format='png', device='vga0', head=head)
    time.sleep(0.8)
    if 'error' in r: out('screendump', tag, r); return None
    return f
def diff(name, a, b, rect=None):
    from PIL import Image
    if not a or not b: return None
    A = Image.open(a); B = Image.open(b); rr = rect or [0, 0, min(A.width, B.width), min(A.height, B.height)]
    try: return json.loads(subprocess.run([sys.executable, os.path.join(QT, 'native', 'pxdiff.py'), a, b, ','.join(map(str, rr)), ','.join(map(str, rr)), '32', os.path.join(W, name)], capture_output=True, text=True).stdout)
    except Exception as e: return {'pct': 100, 'err': str(e)}
def sysview(g):
    return sh_(g, "echo '== sound'; cat /proc/asound/cards; echo '== screens'; for s in /sys/class/drm/card*-*/status; do echo \"$s $(cat $s)\"; done; cat /run/rudolf/screen 2>/dev/null; "
                  "echo '== bluetooth'; ls /sys/class/bluetooth 2>&1; echo '== printers'; (lpstat -p 2>&1 || ls /etc/cups 2>&1) | head -5; echo '== power'; ls /sys/class/power_supply 2>&1; "
                  "cat /sys/power/state; echo '== engine'; cat /run/rudolf/engine-choice /run/rudolf/desktop-engine 2>/dev/null; pgrep -c -f 'WebKitWebProcess' ; pgrep -fa 'rudolf-native|chrome --rudolf-kiosk' | cut -c1-160 | head -4", 90)
def sleepwake(vm, g, tag):
    a = shot(vm, tag + '-before-sleep')
    g.s.s.send(b"(sleep 2; echo mem > /sys/power/state) >/dev/null 2>&1 &\n"); time.sleep(20)
    st = vm.q.cmd('query-status'); out('status while asleep:', st)
    vm.q.cmd('system_wakeup'); time.sleep(45)
    b = shot(vm, tag + '-after-wake')
    o = sh_(g, "dmesg | grep -iE 'PM: suspend (entry|exit)|ACPI: PM' | tail -4; grep -iE 'woke|restart|not responding|did not' /run/rudolf/session.log | grep -v '^+' | tail -5", 60)
    return a, b, st, o

make_drive(DRV)
vm = None; P = {}
try:
    # ---------------------------------------------------------------- Standard
    vm, g, up, js = boot('s'); sh = lambda c, t=60: sh_(g, c, t)
    ok('1. the release starts on Standard', up)
    if not up: raise SystemExit
    r = js("(async()=>{ACCTS.length=0;const a=await acctNew('Ann','a1','admin');ACCTS.push(a);saveUser();return a.id})()", 120); out('account:', r)
    r = js("(()=>{window.__si='wait';(async()=>{try{const a=ACCTS.find(x=>x.n=='Ann');const L=document.getElementById('login');USER=a;await userLogin(a,'a1');lastInput=Date.now();if(L)L.style.display='none';__si='in'}catch(e){__si='error '+e.message}})();return 1})()", 60)
    end = time.time() + 400
    while time.time() < end and js("window.__si", 30) == 'wait': time.sleep(5)
    ok('2. Ann signed in on Standard', js("window.__si", 30) == 'in', js("window.__si", 30)); time.sleep(20)
    scr = js("[innerWidth,innerHeight]"); out('screen (Standard):', scr)
    P['set'] = js("(()=>{const e=[...document.querySelectorAll('#pins > *')].find(b=>b.title=='Settings');if(!e)return null;const r=e.getBoundingClientRect();return [Math.round(r.left+r.width/2),Math.round(r.top+r.height/2)]})()"); out('Settings pin:', P['set'])
    sv_s = sysview(g)
    std_desk = shot(vm, 'std-desktop'); std_h1 = shot(vm, 'std-second-screen', 1)
    js("settings('bt')"); time.sleep(12)
    P['win'] = js("(()=>{const w=[...document.querySelectorAll('.win')].filter(e=>e.offsetParent).pop();if(!w)return null;const r=w.getBoundingClientRect();return [Math.round(r.left),Math.round(r.top),Math.round(r.width),Math.round(r.height)]})()")
    P['nav'] = {k: js(f"(()=>{{const w=[...document.querySelectorAll('.win')].filter(e=>e.offsetParent).pop();const e=w&&[...w.querySelectorAll('*')].find(x=>!x.children.length&&x.textContent.trim()=={json.dumps(k)}&&x.getClientRects().length);if(!e)return null;const r=e.getBoundingClientRect();return [Math.round(r.left+r.width/2),Math.round(r.top+r.height/2)]}})()") for k in ('Bluetooth', 'Sound')}
    out('Settings window:', P['win'], 'nav:', P['nav'])
    P['bttext'] = js("(()=>{const w=[...document.querySelectorAll('.win')].filter(e=>e.offsetParent).pop();return w?w.innerText.replace(/\\s+/g,' ').slice(0,400):null})()"); out('Bluetooth page (Standard):', P['bttext'])
    std_bt = shot(vm, 'std-bluetooth')
    if P['nav'].get('Sound'): vm.q.at(*P['nav']['Sound'], W=scr[0], H=scr[1]); time.sleep(6)
    std_snd = shot(vm, 'std-sound')
    P['sndtext'] = js("(()=>{const w=[...document.querySelectorAll('.win')].filter(e=>e.offsetParent).pop();return w?w.innerText.replace(/\\s+/g,' ').slice(0,300):null})()"); out('Sound page (Standard):', P['sndtext'])
    js("(()=>{[...document.querySelectorAll('.win')].forEach(w=>{const b=w.querySelector('.tb .x, .tb button:last-child');if(b)b.click()});return 1})()"); time.sleep(4)
    sa, sb, sst, so = sleepwake(vm, g, 'std')
    r = js("1+1", 60); ok('3. Standard answers after sleep and waking', r == 2, r)
    std_wake = sb
    sh("grep -iE 'error|warn' /run/rudolf/page.log 2>/dev/null | tail -5; echo native > /persist/etc/rudolf-desktop-engine; sync; cat /persist/etc/rudolf-desktop-engine", 60)
    vm.stop(); time.sleep(3)
    # ---------------------------------------------------------------- Native
    vm, g, up, js = boot('n', std=False); sh = lambda c, t=60: sh_(g, c, t)
    ok('4. the release starts again (Native chosen)', up)
    if not up: raise SystemExit
    end = time.time() + 300
    while time.time() < end and 'NATIVE-UP' not in g.sh("pgrep -f '/usr/bin/rudolf-native' >/dev/null && echo NATIVE-UP", 15): time.sleep(5)
    time.sleep(40)
    sv_n = sysview(g)
    ok('5. the native desktop runs', 'rudolf-native' in sv_n, sv_n[-400:])
    nat_signin = shot(vm, 'nat-signin'); logpng('nat-signin', nat_signin)
    vm.q.type('a1'); time.sleep(0.5); vm.q.key('ret'); time.sleep(40)
    nat_desk = shot(vm, 'nat-desktop'); nat_h1 = shot(vm, 'nat-second-screen', 1)
    d = diff('desktop', std_desk, nat_desk); ok(f'6. the desktop after signing in: {d and d.get("pct")} % differs from Standard (at most 5 %)', d and d.get('pct', 100) <= 5, d)
    logpng('std-desktop', std_desk); logpng('nat-desktop', nat_desk)
    if std_h1 and nat_h1:
        d = diff('screen2', std_h1, nat_h1); ok(f'7. the second screen: {d and d.get("pct")} % differs from Standard\'s (at most 5 %)', d and d.get('pct', 100) <= 5, d)
        logpng('std-second-screen', std_h1); logpng('nat-second-screen', nat_h1)
    else: note('7. the second screen', 'no picture of the second output (QEMU: %s %s)' % (std_h1, nat_h1))
    # Settings, then its Bluetooth page, as a person: the Settings pin, then Bluetooth in its list
    if P.get('set'):
        vm.q.at(*P['set'], W=scr[0], H=scr[1]); time.sleep(25)
        if P['nav'].get('Bluetooth'): vm.q.at(*P['nav']['Bluetooth'], W=scr[0], H=scr[1]); time.sleep(8)
        vm.q.move(scr[0] // 2, scr[1] - 120, W=scr[0], H=scr[1]); time.sleep(1)
        nat_bt = shot(vm, 'nat-bluetooth'); logpng('std-bluetooth', std_bt); logpng('nat-bluetooth', nat_bt)
        d = diff('bluetooth', std_bt, nat_bt, P['win']); ok(f'8. Settings › Bluetooth with no adapter: {d and d.get("pct")} % of the window differs from Standard (at most 5 %)', d and d.get('pct', 100) <= 5, d)
        if P['nav'].get('Sound'): vm.q.at(*P['nav']['Sound'], W=scr[0], H=scr[1]); time.sleep(6)
        vm.q.move(scr[0] // 2, scr[1] - 120, W=scr[0], H=scr[1]); time.sleep(1)
        nat_snd = shot(vm, 'nat-sound'); logpng('std-sound', std_snd); logpng('nat-sound', nat_snd)
        d = diff('sound', std_snd, nat_snd, P['win']); ok(f'9. Settings › Sound with the sound card: {d and d.get("pct")} % of the window differs from Standard (at most 5 %)', d and d.get('pct', 100) <= 5, d)
        vm.q.key('alt', 'f4'); time.sleep(4)
    na, nb, nst_, no = sleepwake(vm, g, 'nat')
    alive = g.sh("pgrep -f '/usr/bin/rudolf-native' >/dev/null && echo NATIVE-ALIVE", 20)
    ok('10. Native is still running after sleep and waking (not restarted)', 'NATIVE-ALIVE' in alive and 'restart' not in no.lower(), no[-400:])
    d = diff('wake', std_wake, nb); ok(f'11. the desktop after waking: {d and d.get("pct")} % differs from Standard\'s (at most 5 %)', d and d.get('pct', 100) <= 5, d)
    logpng('std-after-wake', std_wake); logpng('nat-after-wake', nb)
    sh("tail -40 /run/rudolf/native-desktop.log; grep -iE 'restart|switch|not responding|did not|give' /run/rudolf/session.log | grep -v '^+' | tail -8", 60)
    note('the system on each (Standard, then Native)', '')
    for a_, b_ in zip(sv_s.split('== ')[1:], sv_n.split('== ')[1:]):
        k = a_.split('\n', 1)[0]
        if k in ('sound', 'screens', 'bluetooth', 'printers', 'power'): ok(f'12. what the system sees, {k}: the same on both engines', a_.strip() == b_.strip(), {'std': a_.strip()[:300], 'nat': b_.strip()[:300]})
finally:
    if vm:
        try: vm.stop()
        except Exception: pass
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
