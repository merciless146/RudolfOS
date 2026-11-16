# Rudolf OS 1.6.0 on the real release in QEMU: Task Manager's "Check this computer…" on the Native desktop against Standard (the
# owner's Vivobook report of 2026-11-16, beta 1.6.0.261116, serial 2026111611). The keys go in as the computer's own (QEMU's
# keyboard, read by rudolf-display as on the Vivobook); the sound is QEMU's HDA recorded to a WAV file; the microphone is its HDA
# input (silent, but a real capture device).
#   Standard (Chromium, the reference): the keyboard step with every key of the check pressed once; the tone (a stream, heard); the
#     microphone (a level, no error); the drawing (the picture drawn, its frames); the system report's lines
#   Native: the same in Task Manager's own window; after the keyboard step Super alone opens the start menu and a bare arrow, F5 and
#     Ctrl reach the app; the system report's lines describe the computer (the renderer, the screen's size, the desktop); "Save to
#     a file" saves into the account's Documents and File Explorer has it; the health log's start-up line; the sleep step (rudolfd's
#     GET sleep: the time asleep by the kernel's clocks) and the kernel's own sleep from the shell
#   python3 hwc160_t.py REL-DIR    (SB_PARTS, QEMU, QEMU_ACCEL as for t160; root)
#   HWC_FIX=DIR: DIR/rudolf-native, DIR/index.html and DIR/rudolfd (this tree's, built) put in place of the release's (an extra disk)
import os, sys, time, re, subprocess, shutil, struct, json, base64, wave, array
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); FIX = os.environ.get('HWC_FIX', '')
LOG = open('hwc160.txt', 'w'); R = []; T0 = time.time()
def out(*a):
    s = ' '.join(str(x) for x in a); s = f'[{int(time.time() - T0):5d}s] ' + s; print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:1500] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 3000; DRV = os.path.abspath('hwc160.img'); MEM = int(os.environ.get('MEM', '4096')); W = os.path.abspath('w160h')
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
    for b in ('rudolf-native', 'rudolfd'):
        if os.path.exists(f'{FD}/{b}'): os.chmod(f'{FD}/{b}', 0o755)
    FIXIMG = os.path.abspath('hwc160fix.img'); subprocess.run(['truncate', '-s', '64M', FIXIMG], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'HWCFIX', '-d', FD, FIXIMG], check=True)
INIT_JS = "(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};return 1})()"
VMT = {}
def boot(tag):
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf160',
             '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1',
             '-audiodev', f'wav,id=snd0,path={os.path.abspath("hwc160-" + tag + ".wav")}', '-device', 'intel-hda', '-device', 'hda-duplex,audiodev=snd0']
    if FIXIMG: extra += ['-drive', f'file={FIXIMG},format=raw,if=none,id=fx,readonly=on', '-device', 'nvme,drive=fx,serial=hwcfix']
    vm = sbvm.VM('hwc160' + tag, PARTS, [], secure=False, mem=MEM, extra=extra); VMT[tag] = time.time()
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    time.sleep(30); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    return vm, g, up
def put_small(g, data, remote):
    b = base64.b64encode(data.encode()).decode()
    if len(b) < 800: g.sh(f"echo {b} | base64 -d > {remote}", 20); return
    f = os.path.join(W, 'p.tmp'); open(f, 'w').write(data); g.put(f, remote, '644')
def run(g, c, t=60, show=True):
    put_small(g, c + '\n', '/tmp/c160.sh')
    o = g.sh('sh /tmp/c160.sh 2>&1', t)
    if show: out('$', c[:160].replace('\n', ' ; ')); out(o.rstrip()[-2500:])
    return o
def jsfn(g):
    def js1(e, t):
        if len(base64.b64encode(e.encode())) < 800: return g.js(e, t)
        open(f'{W}/l.js', 'w').write(e); g.put(f'{W}/l.js', '/tmp/l.js', '644')
        o = g.sh('cdp-eval "$(cat /tmp/l.js)" 2>&1', t); m = re.search(r'\{"id":1.*\}', o)
        if not m: return ('ERR', o[-400:])
        res = json.loads(m.group(0)).get('result', {})
        if 'exceptionDetails' in res: return ('EXC', res['exceptionDetails'].get('exception', {}).get('description', '')[:300])
        return res.get('result', {}).get('value')
    def js(e, t=90):
        for _ in range(3):
            r = js1(e, t)
            if not (isinstance(r, tuple) and (r[0] == 'ERR' or (r[0] == 'EXC' and 'ReferenceError' in str(r[1])))): return r
            time.sleep(10)
        return r
    return js
AS_RUDOLF = "su -s /bin/sh rudolf -c 'XDG_RUNTIME_DIR=/run/user/1000 PULSE_SERVER=unix:/run/user/1000/pulse/native %s'"
def wav_loud(path, a, b):
    """the loudest sample (0-32767) of QEMU's recording between seconds a and b of it"""
    try:
        w = wave.open(path); fr = w.getframerate(); n = w.getnframes()
        w.setpos(min(n, int(a * fr))); left = max(0, min(n, int(b * fr)) - int(a * fr)); m = 0
        while left > 0:
            d = array.array('h', w.readframes(min(left, 1 << 20))); left -= 1 << 20
            if not d: break
            m = max(m, max(d), -min(d))
        return m
    except Exception as e: return f'ERR {e}'
TOK = "$(busybox wget -q -O- http://127.0.0.1:8080/ | grep -o 'token:\"[0-9a-f]*' | cut -d'\"' -f2)"
def api(sh, p, post=False):
    return sh(f"busybox wget -q -O- --header \"X-Rudolf-Token: {TOK}\" {'--post-data= ' if post else ''}http://127.0.0.1:8080/api/{p}", 30, False)
# every key of the check (HWC_KROWS) as QEMU's key names
KEYS = ['esc'] + [f'f{i}' for i in range(1, 13)] + ['grave_accent', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 'minus', 'equal', 'backspace', 'tab'] + list('qwertyuiop') + \
       ['bracket_left', 'bracket_right', 'backslash', 'caps_lock'] + list('asdfghjkl') + ['semicolon', 'apostrophe', 'ret', 'shift'] + list('zxcvbnm') + \
       ['comma', 'dot', 'slash', 'shift_r', 'ctrl', 'meta_l', 'alt', 'spc', 'alt_r', 'ctrl_r', 'left', 'up', 'down', 'right']
# the check driven in the page: to a step (Skip, else the main button, on each screen before it)
GO_JS = ("(()=>{window.__G='?';(async()=>{try{if(!window.__H||!document.body.contains(__H))window.__H=hwCheck();for(let k=0;k<120;k++){const h=__H.querySelector('h3').textContent;"
         "if(h==%s){__G='at '+h;return}const bs=[...__H.querySelectorAll('.hwcbt button')];const b=bs.find(x=>x.textContent=='Skip')||bs.find(x=>x.className=='pri');if(b)b.click();"
         "await new Promise(r=>setTimeout(r,900))}__G='stuck at '+__H.querySelector('h3').textContent}catch(e){__G='ERR '+e}})();return 1})()")
KEYS_JS = "(()=>{const m=__H.querySelector('.hwkm');return (m?m.textContent:'?')+'|'+[...__H.querySelectorAll('.hwkb span')].filter(x=>x.style.background!='rgb(22, 163, 74)').map(x=>x.dataset.c).join(' ')})()"
MIC_JS = ("(()=>{window.__M='?';(async()=>{let d=[];try{d=(await navigator.mediaDevices.enumerateDevices()).map(x=>x.kind+':'+(x.label||'-'))}catch(e){d=['ERR '+e]}"
          "const r=await hwcMicPeak(3000);__M=JSON.stringify({devices:d,r})})().catch(e=>__M='ERR '+e);return 1})()")
GFX_JS = ("(()=>{window.__F='?';const c=document.createElement('canvas');c.width=320;c.height=180;document.body.appendChild(c);"
          "hwcFps(c,3000).then(f=>{__F=JSON.stringify({fps:f,drawn:hwcFps.drawn,err:hwcFps.err});c.remove()},e=>__F='ERR '+e);return 1})()")
REP_JS = ("(()=>{window.__R='?';sysReport('').then(r=>__R=r.summary.split('\\n').filter(l=>/^(Pages drawn with|Desktop size|Desktop|Display|Drawn by):/.test(l)).join(' ## '),e=>__R='ERR '+e);return 1})()")

make_drive(DRV)
vm = None
STD = {}
try:
    # ------------------------------------------------------------------ 1. Standard (Chromium): the reference
    vm, g, up = boot('s'); js = jsfn(g); sh = lambda c, t=60, show=True: run(g, c, t, show)
    ok('0. the release starts (Standard)', up)
    if not up: raise RuntimeError('did not start')
    end = time.time() + 600
    while time.time() < end and js("typeof acctNew=='function'&&typeof userLogin=='function'", 20) is not True: time.sleep(5)
    if FIX:   # (this tree's page on Standard too: the same fixes in Chromium)
        sh("FX=; for d in /dev/nvme*n1; do blkid $d 2>/dev/null | grep -q HWCFIX && FX=$d; done; mkdir -p /tmp/fx; [ -n \"$FX\" ] && mount -o ro $FX /tmp/fx; "
           "WWW=/usr/share/rudolf/www; [ -f /persist/os/current/index.html ] && WWW=/persist/os/current; [ -f /tmp/fx/index.html ] && cp /tmp/fx/index.html /tmp/page.html && chmod 644 /tmp/page.html && mount --bind /tmp/page.html $WWW/index.html; echo $WWW", 60)
        js("location.reload();1", 20); time.sleep(20); end = time.time() + 300
        while time.time() < end and js("typeof acctNew=='function'&&typeof userLogin=='function'&&typeof hwcToneEnd=='function'", 20) is not True: time.sleep(5)
    time.sleep(40); js(INIT_JS)
    r = js("(async()=>{ACCTS.length=0;const b=await acctNew('boss','b1','admin');ACCTS.push(b);saveUser();return b.id})()", 120); out('account', r)
    r = js("(async()=>{try{const a=ACCTS.find(x=>x.n=='boss');if(USER)userLogout();USER=a;await userLogin(a,'b1');lastInput=Date.now();"
           "const L=document.getElementById('login');if(L)L.style.display='none';return 'in '+USER.n}catch(e){return 'ERR '+e.message}})()", 300)
    out('sign-in (Standard):', r)
    end = time.time() + 300
    while time.time() < end and js("!!(USER&&USER.disk&&hdOn())", 30) is not True: time.sleep(5)
    time.sleep(10)
    out('input devices:', sh("cat /run/rudolf/inputs 2>/dev/null | head; cat /sys/power/mem_sleep /sys/power/state; ls /sys/power/suspend_stats 2>&1 | head -20", 30, False))
    # the tone
    api(sh, 'volume?set=80', True); t1 = time.time() - VMT['s']; js("(()=>{hwcTone();return 1})()"); time.sleep(1)
    o = sh(AS_RUDOLF % 'pactl list sink-inputs' + " | grep -E 'Sink Input|application.name|application.process.binary|Mute|Corked'", 30, False); time.sleep(2.5); t2 = time.time() - VMT['s']
    STD['tone'] = (t1, t2); STD['streams'] = o; out('Standard tone streams:', o)
    # the microphone
    js(MIC_JS); time.sleep(6); r = js("window.__M"); STD['mic'] = r; out('Standard microphone:', r)
    ok('S1. (reference) Standard: the check\'s microphone opens (no error)', isinstance(r, str) and '"db"' in r, r)
    js(GFX_JS); time.sleep(6); r = js("window.__F"); out('Standard drawing:', r)
    ok('S2. Standard: the check\'s moving picture is drawn (it links and draws), at a frame rate', isinstance(r, str) and '"drawn":true' in r, r)
    js(REP_JS); time.sleep(10); r = js("window.__R"); out('Standard report:', r); STD['rep'] = r
    # the keyboard step
    js(GO_JS % "'Keyboard'"); time.sleep(25); r = js("window.__G"); out('Standard check:', r)
    vm.keys(*KEYS, gap=0.25); time.sleep(2); vm.q.key('caps_lock'); time.sleep(1)
    r = js(KEYS_JS); out('Standard keyboard step:', r); STD['keys'] = r
    ok('S3. (reference) Standard: every key of the keyboard step answers', isinstance(r, str) and r.startswith('76 of 76'), r)
    vm.shot('std-keys')
    js("(async()=>{try{__H.remove()}catch(e){}try{fsFlush()}catch(e){}try{await homeChain}catch(e){}try{await __rudolfLsFlush()}catch(e){}return 1})()", 60)
    sh("echo native > /persist/etc/rudolf-desktop-engine; rm -f /persist/etc/rudolf-desktop-engine.failed; sync", 60)
    time.sleep(5); vm.stop(); time.sleep(3)
    L = wav_loud(os.path.abspath('hwc160-s.wav'), 0, 100000); out('Standard: the loudest sound in the recording:', L)
    ok('S4. (reference) Standard: the check\'s tone plays (a stream, sound in QEMU\'s recording)', 'Sink Input' in STD.get('streams', '') and isinstance(L, int) and L > 500, L)

    # ------------------------------------------------------------------ 2. Native
    vm, g, up = boot('n'); sh = lambda c, t=60, show=True: run(g, c, t, show)
    ok('0b. the release starts again (Native)', up)
    if not up: raise RuntimeError('did not start')
    o = sh("for i in $(seq 1 60); do pgrep -x rudolf-native >/dev/null && break; sleep 3; done; cat /run/rudolf/desktop-engine; pgrep -x rudolf-native", 240)
    ok('0c. the Native desktop runs', 'native' in o, o)
    out('health log (before):', sh("tail -5 /persist/var/log/health.log", 30, False))
    o = sh("FX=; for d in /dev/nvme*n1; do blkid $d 2>/dev/null | grep -q HWCFIX && FX=$d; done; echo fix=$FX; mkdir -p /tmp/fx; [ -n \"$FX\" ] && mount -o ro $FX /tmp/fx; ls -la /tmp/fx; "
           "WWW=/usr/share/rudolf/www; [ -f /persist/os/current/index.html ] && WWW=/persist/os/current; [ -f /tmp/fx/index.html ] && cp /tmp/fx/index.html /tmp/page.html && chmod 644 /tmp/page.html && mount --bind /tmp/page.html $WWW/index.html; "
           "if [ -x /tmp/fx/rudolfd ]; then P=$(pgrep -x rudolfd); kill $P; sleep 2; kill -9 $P 2>/dev/null; setsid /tmp/fx/rudolfd >> /run/rudolf/rudolfd.log 2>&1 < /dev/null & sleep 4; echo rudolfd now $(pgrep -x rudolfd); fi; "
           "rm -f /tmp/teval /tmp/teval.out; mkfifo -m 666 /tmp/teval; mount --bind /tmp/fx/rn /usr/bin/rudolf-native; "
           "P0=$(pgrep -f 'rudolf-native [-]-rudolf-kiosk'); kill $P0; sleep 3; kill -9 $P0 2>/dev/null; for i in $(seq 1 90); do [ -s /tmp/nstate ] && break; sleep 2; done; head -3 /tmp/nstate", 400)
    ok('0d. the native desktop started again with the tests\' views' + (' and this tree\'s files' if FIX else ''), 'screen ' in o, o)
    state = lambda: sh("cat /tmp/nstate", 30, False)
    def st_get(st, k):
        m = re.search(rf'^{k} (.*)$', st, re.M); return m.group(1) if m else None
    st = state(); SW, SH = [int(x) for x in st_get(st, 'screen').split()]
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
    hl = sh("tail -6 /persist/var/log/health.log", 30, False); out('health log:', hl)
    if FIX: ok('H1. Native: the health log\'s start-up line is the shell\'s (its sign-in screen), with what took the time', 'Native:' in hl and 'the desktop program started at' in hl, hl)
    if os.environ.get('HWC_SND'):   # (diagnostic: when WebKit's sound starts, recorded inside the guest)
        g.q.key('ctrl', 'alt', 'delete'); time.sleep(15)
        REC = "su -s /bin/sh rudolf -c 'XDG_RUNTIME_DIR=/run/user/1000 PULSE_SERVER=unix:/run/user/1000/pulse/native parec -d @DEFAULT_MONITOR@ --format=s16le --rate=8000 --channels=1 --latency-msec=20 > /tmp/rec.raw' & echo $! > /tmp/rec.pid; date +%s%3N > /tmp/rec.t0"
        AN = "kill $(cat /tmp/rec.pid); sleep 1; T0=$(cat /tmp/rec.t0); echo t0 $T0; ls -la /tmp/rec.raw; od -An -td2 -v -w2 /tmp/rec.raw | awk -v t0=$T0 '{v=$1<0?-$1:$1; if(v>m)m=v; n++; if(n%400==0){printf \"%d:%d \", t0+n/8, m; m=0}} END{print \"\"}'"
        for name, expr in (('old tone (new context at once, 2 s, closed)', "(()=>{window.__T0=Date.now();const a=new AudioContext(),o=a.createOscillator(),g=a.createGain();o.connect(g);g.connect(a.destination);g.gain.value=0.2;o.frequency.value=440;o.frequency.setValueAtTime(660,a.currentTime+1);o.start();o.stop(a.currentTime+2);o.onended=()=>a.close();return 1})()"),
                           ('this tree\'s hwcTone', "(()=>{window.__T0=Date.now();hwcTone();return 1})()"),
                           ('this tree\'s hwcTone again (Play again)', "(()=>{window.__T0=Date.now();hwcTone();return 1})()"),
                           ('a 4 s oscillator', "(()=>{window.__T0=Date.now();const a=new AudioContext(),o=a.createOscillator();o.frequency.value=500;o.connect(a.destination);o.start();setTimeout(()=>{o.stop();a.close()},4000);return 1})()")):
            sh(REC, 20, False); time.sleep(1); tev('taskmgr', expr)
            pol = sh("for i in $(seq 1 16); do printf '%s:%s ' $(date +%s%3N) $(" + AS_RUDOLF % 'pactl list short sink-inputs' + " | wc -l); sleep 0.25; done", 60, False)
            time.sleep(4); t0 = tev('taskmgr', "String(window.__T0)"); o = sh(AN, 120, False)
            out(f'SOUND {name}: started at {t0}; streams: {pol.strip()}; recording (ms:loudest per 50 ms): {o.strip()[-1500:]}')
        tev('taskmgr', "(()=>{hwcToneEnd();return 1})()")
        raise RuntimeError('sound diagnostic done')
    g.q.key('ctrl', 'alt', 'delete'); tm = False
    for _ in range(40):
        time.sleep(1); st = state()
        if re.search(r'^client .* focus=1 app=taskmgr ', st, re.M): tm = True; break
    ok('N0. Task Manager is open in front (Ctrl+Alt+Del)', tm, [l for l in st.split('\n') if l.startswith('client')])
    time.sleep(8)
    # the tone, from Task Manager's window
    api(sh, 'volume?set=80', True); t1 = time.time() - VMT['n']; tev('taskmgr', "(()=>{hwcTone();return 1})()"); time.sleep(1)
    o = sh(AS_RUDOLF % 'pactl list sink-inputs' + " | grep -E 'Sink Input|application.name|application.process.binary|Mute|Corked'; " + AS_RUDOLF % 'pactl list sinks short' + "; " + AS_RUDOLF % 'pactl list sources short', 30, False)
    time.sleep(2.5); t2 = time.time() - VMT['n']; out('Native tone streams:', o); TONE = o
    # the microphone
    tev('taskmgr', MIC_JS); time.sleep(7); r = tev('taskmgr', "window.__M"); out('Native microphone:', r)
    if 'Invalid constraint' in r or '"devices":[]' in r:
        out('WebKit\'s web process:', sh("P=$(pgrep -f WebKitWebProcess | head -1); echo $P; tr '\\0' '\\n' < /proc/$P/environ | grep -E 'PULSE|XDG|GST|HOME'; ls -la /proc/$P/root/ 2>&1 | head -3; pgrep -a bwrap | head -3; "
                                       "grep -oE '[^ ]*(gstpulse|libpulse|gstcoreelements)[^ ]*' /proc/$P/maps | sort -u; ls /usr/lib/x86_64-linux-gnu/gstreamer-1.0/ | head -60 | tr '\\n' ' '; grep -i 'capture\\|device' /run/rudolf/native-desktop.log | tail -10", 60, False))
    ok('N1. Native: the check\'s microphone opens (no "Invalid constraint"), as on Standard', '"db"' in r, (r, STD.get('mic')))
    r = tev('taskmgr', GFX_JS); time.sleep(6); r = tev('taskmgr', "window.__F"); out('Native drawing:', r)
    ok('N2. Native: the check\'s moving picture is drawn, at a frame rate', '"drawn":true' in r, r)
    tev('taskmgr', REP_JS); time.sleep(10); r = tev('taskmgr', "window.__R"); out('Native report:', r)
    ok('N3. Native: the system report describes the computer: the graphics chip (not "Apple GPU"), the screen\'s size, the native desktop',
       'Apple GPU' not in r and f'Desktop size: {SW}x{SH}' in r.replace(' @1', '') and 'Desktop: native' in r, (r, STD.get('rep')))
    # the keyboard step
    tev('taskmgr', GO_JS % "'Keyboard'"); time.sleep(25); r = tev('taskmgr', "window.__G"); out('Native check:', r, st_get(state(), 'keys'))
    vm.keys(*KEYS, gap=0.25); time.sleep(2); vm.q.key('caps_lock'); time.sleep(1)
    r = tev('taskmgr', KEYS_JS); st = state(); out('Native keyboard step:', r, st_get(st, 'keys'), [l for l in st.split('\n') if l.startswith(('client', 'menu '))])
    ok('N4. Native: every key of the keyboard step answers, as on Standard (F1-F12, the left Ctrl, Super and Alt, the arrows)', r.startswith('76 of 76'), (r, STD.get('keys')))
    vm.shot('nat-keys')
    tev('taskmgr', "(()=>{try{[...__H.querySelectorAll('.hwcbt button')].find(b=>b.textContent=='Stop').click()}catch(e){}return 1})()"); time.sleep(2)
    st = state(); ok('N5. ...the shell has its keys again when the step ends', (st_get(st, 'keys') or '').startswith('grabbed=1'), st_get(st, 'keys'))
    tev('taskmgr', "(()=>{window.__K=[];const f=e=>__K.push(e.type[3]+':'+e.code);addEventListener('keydown',f,true);addEventListener('keyup',f,true);return 1})()")
    vm.keys('f5', 'left', 'ctrl', 'up', gap=0.4); time.sleep(1); r = tev('taskmgr', "__K.join(' ')")
    ok('N6. Native: F5, the arrows and Ctrl reach the app in front', all(f'd:{c}' in r for c in ('F5', 'ArrowLeft', 'ControlLeft', 'ArrowUp')), r)
    vm.q.key('meta_l'); time.sleep(3); st = state(); r = tev('taskmgr', "__K.join(' ')")
    ok('N7. Native: Super alone opens the start menu, and the app saw the key (MetaLeft) as on Standard', st_get(st, 'menu') == '1' and 'd:MetaLeft' in r, (st_get(st, 'menu'), r))
    vm.q.key('esc'); time.sleep(2)
    # the report saved to a file
    tev('taskmgr', "(()=>{sysReportDialog();setTimeout(()=>document.querySelector('.rmodal .srs').click(),800);return 1})()"); time.sleep(15)
    r = tev('taskmgr', "(()=>{const m=document.querySelector('.rmodal .srm');return (m?m.textContent:'?')+'|'+(document.querySelector('.rmodal .srf')?'folder':'')})()"); out('report saved:', r)
    m = re.search(r'Saved as (C:\\USERS\\[^\\]+\\DOCUMENTS\\REPORT-\d+\.TXT)', r); time.sleep(5)
    disk = sh("find /persist /home -name 'REPORT-*.TXT' 2>/dev/null | head -5", 30, False); out('on the disk:', disk)
    ok('N8. Native: "Save to a file" saves into the account\'s Documents, says the full path and offers the folder; the file is on the disk', m is not None and 'folder' in r and 'REPORT-' in disk, (r, disk))
    if m:
        tev('taskmgr', "(()=>{document.querySelector('.rmodal .srf').click();return 1})()"); ex = False
        for _ in range(30):
            time.sleep(1)
            if re.search(r'^client .* app=explorer ', state(), re.M): ex = True; break
        time.sleep(8); r = tev('explorer', "(()=>{const t=document.body.innerText;return /REPORT-\\d+\\.TXT/.test(t)?'listed':'not listed'})()") if ex else 'no window'
        ok('N9. ...Open the folder: File Explorer\'s own window at Documents lists the report', r == 'listed', r)
    # sleep: the kernel's own, then the check's (rudolfd's GET sleep)
    o = sh("cat /sys/power/mem_sleep; cat /sys/power/suspend_stats/success /sys/power/suspend_stats/fail 2>&1; echo 0 > /sys/class/rtc/rtc0/wakealarm; echo +8 > /sys/class/rtc/rtc0/wakealarm; "
           "A=$(cut -d' ' -f1 /proc/uptime); D0=$(date +%s); echo mem > /sys/power/state; echo rc=$?; B=$(cut -d' ' -f1 /proc/uptime); D1=$(date +%s); echo uptime $A $B date $D0 $D1; cat /sys/power/suspend_stats/success; dmesg | grep -E 'PM: suspend (entry|exit)|ACPI: PM' | tail -4", 120)
    out('the kernel\'s sleep in QEMU:', o)
    if FIX:
        r = api(sh, 'sleep'); out('GET sleep:', r)
        tev('taskmgr', "(()=>{try{__H.remove()}catch(e){}window.__H=null;return 1})()"); tev('taskmgr', GO_JS % "'Sleep'")
        for _ in range(30):
            time.sleep(8); r = tev('taskmgr', "window.__G")
            if r.startswith(('at ', 'stuck', 'ERR')): break
        out('to Sleep:', r)
        tev('taskmgr', "(()=>{[...__H.querySelectorAll('.hwcbt button')].find(b=>/Sleep for/.test(b.textContent)).click();return 1})()"); time.sleep(60)
        r = tev('taskmgr', "__H.querySelector('.hwcb').innerText.replace(/\\n/g,' ')"); r2 = api(sh, 'sleep'); out('sleep step:', r, r2)
        okk = re.search(r'"slept_ms":(\d+)', r2); asleep = int(okk.group(1)) if okk else 0
        ok('N10. Native: the sleep step says what rudolfd measured (asleep by the kernel\'s clocks, or why not), never only the time waited',
           ('Awake again after' in r and asleep >= 5000) or ('did not sleep' in r and asleep < 5000), (r, r2))
    vm.stop(); time.sleep(3)
    L = wav_loud(os.path.abspath('hwc160-n.wav'), 0, 100000); out('Native: the loudest sound in the recording:', L)
    ok('N11. Native: the check\'s tone plays: a stream from the app window\'s WebKit, not corked or muted, and sound in QEMU\'s recording', 'WebKitWebProcess' in TONE and 'Corked: no' in TONE and 'Mute: no' in TONE and isinstance(L, int) and L > 500, (TONE, L))
except RuntimeError as e:
    ok('the run went on: ' + str(e), False)
finally:
    try:
        if vm: vm.stop()
    except Exception: pass
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
