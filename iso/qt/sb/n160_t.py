# Rudolf OS 1.6.0 on the Native desktop in QEMU, over a long run: the owner's six Vivobook reports of 2026-11-15 (beta
# 1.6.0.261116), on the real release (not a Linux host), checked moment by moment rather than by finished pictures.
#   1. an app's window shows the page's sign-in screen before the app (#login seen in any frame of an app's window)
#   2. sound from the native app windows (WebKit's web processes): a PulseAudio stream from WebKitWebProcess and sound in
#      QEMU's recording; the desktop's own sounds (sign-in, notices) on Native as on Standard
#   3. display settings applied live: Settings changes the native shell (rudolfd's desktop/settings, what the shell reads)
#      and the other open app windows at once, and nothing puts the old values back
#   4. the widgets keep live values (the system monitor's CPU, memory and the real number of windows; the weather's
#      answer or its error, never "Loading…" for long) through apps opening and closing, a restart of the native desktop
#      and the screen saver
#   5. no notice is shown twice (the update notice once, no "Real browser tabs are ready" on Native at all), the
#      notifications' list holding each event once
#   6. Terminal opens the Terminal with Linux (a Debian stand-in, as t160's), as on Standard, not a Command Prompt
#      with only C: tabs
# First on Standard (Chromium, the reference): an account, its widgets, an older release seen, the screen saver after 1
# minute; then the release started again with the Native engine and used for N160_MIN minutes (default 35).
#   python3 n160_t.py REL-DIR    (SB_PARTS, QEMU, QEMU_ACCEL as for t160; root)
#   N160_FIX=DIR: DIR/rudolf-native and/or DIR/index.html (this tree's, built) put in place of the release's, to check
#   the fixes on the real release (an extra disk; rudolfd and the session are the release's own)
import os, sys, time, re, subprocess, shutil, struct, json, base64, wave, array
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE); ISO = os.path.dirname(QT)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); FIX = os.environ.get('N160_FIX', '')
MINUTES = float(os.environ.get('N160_MIN', '35'))
LOG = open('n160.txt', 'w'); R = []; T0 = time.time()
def out(*a):
    s = ' '.join(str(x) for x in a); s = f'[{int(time.time() - T0):5d}s] ' + s; print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:1500] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 3000; DRV = os.path.abspath('n160.img'); MEM = int(os.environ.get('MEM', '4096')); W = os.path.abspath('w160n')
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
# the fixed files, on a disk of their own (faster than the serial line)
FIXIMG = ''
if FIX:
    FIXIMG = os.path.abspath('n160fix.img'); subprocess.run(['truncate', '-s', '64M', FIXIMG], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'N160FIX', '-d', FIX, FIXIMG], check=True)

INIT_JS = "(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};return 1})()"
def boot(tag):
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf160',
             '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1',
             '-audiodev', f'wav,id=snd0,path={os.path.abspath("n160-" + tag + ".wav")}', '-device', 'intel-hda', '-device', 'hda-duplex,audiodev=snd0']
    if FIXIMG: extra += ['-drive', f'file={FIXIMG},format=raw,if=none,id=fx,readonly=on', '-device', 'nvme,drive=fx,serial=n160fix']
    vm = sbvm.VM('n160' + tag, PARTS, [], secure=False, mem=MEM, extra=extra)
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    time.sleep(30); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    return vm, g, up
def put_small(g, data, remote):
    # (small files in one command line, base64: g.put waits seconds for each)
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
        # (a long expression from a file: busybox ash cuts a command line at 1024 characters)
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
def wait_for(js, expr, secs, every=5):
    end = time.time() + secs; r = None
    while time.time() < end:
        r = js(expr, 60)
        if r is True: return True
        time.sleep(every)
    return r
AS_RUDOLF = "su -s /bin/sh rudolf -c 'XDG_RUNTIME_DIR=/run/user/1000 PULSE_SERVER=unix:/run/user/1000/pulse/native %s'"
SND_JS = ("(()=>{window.__snd={};try{const c=new (window.AudioContext||window.webkitAudioContext)(),o=c.createOscillator(),gn=c.createGain();gn.gain.value=.5;o.frequency.value=440;o.connect(gn);gn.connect(c.destination);o.start();"
          "__snd.ctx=c.state;setTimeout(()=>{__snd.ctx2=c.state;o.stop();c.close()},6000)}catch(e){__snd.err=String(e)}"
          "try{const n=44100,b=new ArrayBuffer(44+n*2),v=new DataView(b),s=(o,t)=>{for(let i=0;i<t.length;i++)v.setUint8(o+i,t.charCodeAt(i))};s(0,'RIFF');v.setUint32(4,36+n*2,true);s(8,'WAVEfmt ');v.setUint32(16,16,true);v.setUint16(20,1,true);v.setUint16(22,1,true);"
          "v.setUint32(24,44100,true);v.setUint32(28,88200,true);v.setUint16(32,2,true);v.setUint16(34,16,true);s(36,'data');v.setUint32(40,n*2,true);for(let i=0;i<n;i++)v.setInt16(44+i*2,Math.sin(i*2*Math.PI*880/44100)*12000,true);"
          "const a=new Audio(URL.createObjectURL(new Blob([b],{type:'audio/wav'})));a.loop=true;window.__sa=a;a.play().then(()=>__snd.el='playing',e=>__snd.el='refused '+e.name);setTimeout(()=>a.pause(),6000)}catch(e){__snd.err2=String(e)}"
          "try{sfx('notify',1);__snd.sfx=AC?AC.state:'no AC'}catch(e){__snd.sfx='ERR '+e}return 'started'})()")
def wav_loud(path, a, b):
    """the loudest sample (0-32767) of QEMU's recording between seconds a and b of it"""
    try:
        w = wave.open(path); fr = w.getframerate(); ch = w.getnchannels(); n = w.getnframes()
        w.setpos(min(n, int(a * fr))); left = max(0, min(n, int(b * fr)) - int(a * fr)); m = 0
        while left > 0:
            d = array.array('h', w.readframes(min(left, 1 << 20))); left -= 1 << 20
            if not d: break
            m = max(m, max(d), -min(d))
        return m
    except Exception as e: return f'ERR {e}'

make_drive(DRV)
vm = None
try:
    # ------------------------------------------------------------------ Standard (Chromium): the reference
    vm, g, up = boot('s'); js = jsfn(g); sh = lambda c, t=60, show=True: run(g, c, t, show)
    ok('0. the release starts (Standard)', up)
    if not up: raise SystemExit
    end = time.time() + 600
    while time.time() < end and js("typeof acctNew=='function'&&typeof userLogin=='function'", 20) is not True: time.sleep(5)
    time.sleep(40); js(INIT_JS)
    r = js("(async()=>{ACCTS.length=0;const b=await acctNew('boss','b1','admin');ACCTS.push(b);saveUser();return b.id})()", 120); BOSS = str(r); out('account', BOSS)
    r = js("(async()=>{try{const a=ACCTS.find(x=>x.n=='boss');if(!(await pwCheck(a,'b1')))return 'bad';if(USER)userLogout();USER=a;await userLogin(a,'b1');lastInput=Date.now();"
           "const L=document.getElementById('login');if(L)L.style.display='none';return 'in '+USER.n}catch(e){return 'ERR '+e.message}})()", 300)
    out('sign-in (Standard):', r)
    wait_for(js, "!!(USER&&USER.disk&&hdOn())", 300)
    # the widgets (the clock, the system monitor, the weather), an older release seen (the update notice due once), the
    # screen saver after a minute without the lock; Debian's stand-in (installed, as t160's)
    r = js("(()=>{const W=[{t:'clock',x:900,y:60,txt:''},{t:'mon',x:900,y:220,txt:''},{t:'weather',x:900,y:360,txt:''}];WG=W;svj(WGK(),W);try{wgDraw()}catch(e){}"
           "S.seenRel='r1';persistS();SYSC.sson=true;SYSC.ssmin=1;SYSC.lockss=false;SYSC.ssstyle='stars';sysSave();try{fsFlush()}catch(e){};return WGK()})()", 60)
    out('widgets, seenRel, screen saver:', r)
    sh("D=/persist/debian; mkdir -p $D/bin $D/lib $D/lib64 $D/etc $D/proc $D/sys $D/dev $D/tmp $D/run $D/home/rudolf $D/root $D/.oldroot; cp /bin/busybox $D/bin/; for l in $(ldd /bin/busybox 2>/dev/null | grep -o '/[^ ]*'); do mkdir -p $D$(dirname $l); cp -L $l $D$l; done; "
       "for c in sh cat echo ls wget id; do ln -sf busybox $D/bin/$c; done; echo 'rudolf:x:1000:1000::/home/rudolf:/bin/sh' > $D/etc/passwd; echo test > $D/etc/rudolf-debian; touch $D/.rudolf-ready; chown -R 200000:200000 $D; "
       "grep -q '^video_log=1' /persist/etc/rudolf-memory 2>/dev/null || echo video_log=1 >> /persist/etc/rudolf-memory; rudolf-debian status 2>&1 | head -5", 120)
    # Standard's sound (Chromium): a stream from the browser, heard in the recording
    t_s = time.time(); js(SND_JS); time.sleep(3)
    o = sh(AS_RUDOLF % 'pactl list sink-inputs' + " | grep -E 'Sink Input|application.name|application.process.binary|media.role|Mute|Corked' ; " + AS_RUDOLF % 'pactl list sinks short', 60)
    STD_SND = js("JSON.stringify(window.__snd)"); out('Standard sound:', STD_SND)
    ok('S1. (reference) Standard plays the page\'s sound through PulseAudio', 'Sink Input' in o, o)
    # Standard's Terminal: what it opens, its tabs
    js("launch('terminal');1"); time.sleep(12)
    STD_TERM = js("JSON.stringify(wins.filter(w=>w.app=='terminal'||/Terminal|Command Prompt|Debian/.test(w.title||'')).map(w=>({t:w.title,tabs:[...w.el.querySelectorAll('.dttab span')].map(e=>e.textContent),linux:!!w.el.querySelector('.dtbox.dtlinux,.xterm')})))")
    out('Standard Terminal:', STD_TERM, js("JSON.stringify(DEB.st)"))
    STD_NOTIF = js("JSON.stringify(NC.map(n=>n.t))"); out('Standard notifications:', STD_NOTIF)
    js("(async()=>{try{fsFlush()}catch(e){}try{await homeChain}catch(e){}try{await __rudolfLsFlush()}catch(e){}return 1})()", 60)
    sh("echo native > /persist/etc/rudolf-desktop-engine; rm -f /persist/etc/rudolf-desktop-engine.failed; sync; ls -l /home/rudolf/.rudolf-ls.json", 60)
    time.sleep(5); vm.stop(); time.sleep(3)

    # ------------------------------------------------------------------ Native
    vm, g, up = boot('n'); sh = lambda c, t=60, show=True: run(g, c, t, show)
    ok('0b. the release starts again', up)
    if not up: raise SystemExit
    o = sh("for i in $(seq 1 60); do pgrep -f /usr/bin/rudolf-native >/dev/null && break; sleep 3; done; cat /run/rudolf/desktop-engine; pgrep -f /usr/bin/rudolf-native | head -3; mount | grep -E ' /tmp | /run ' ; tail -5 /run/rudolf/native-desktop.log", 240)
    ok('0c. the Native desktop runs', 'native' in o and re.search(r'^\d+$', o, re.M), o)
    # the tests' views (RUDOLF_NATIVE_STATE, RUDOLF_NATIVE_TEST_EVAL) through a wrapper bound over the program, the fixes if
    # given; the page with a probe of its own first frames: when #login shows in an app's window (__lgSeen)
    PROBE = ("<script>(()=>{if(!/^#app=/.test(location.hash))return;const L=[];window.__lgSeen=L;const t0=performance.now();let n=0;"
             "function f(){n++;const e=document.getElementById('login');if(e){const c=getComputedStyle(e);if(c.display!='none'&&c.visibility!='hidden'&&+c.opacity>0&&e.getClientRects().length)L.push(Math.round(performance.now()-t0))}"
             "if(performance.now()-t0<15000)requestAnimationFrame(f);else window.__lgFrames=n}requestAnimationFrame(f);"
             "document.addEventListener('readystatechange',()=>{const e=document.getElementById('login');if(e&&document.readyState=='interactive'){const c=getComputedStyle(e);if(c.display!='none')window.__lgFirst=1}})})()</script>")
    open(f'{W}/probe.html', 'w').write(PROBE); g.put(f'{W}/probe.html', '/tmp/probe.html', '644')
    o = sh("FX=$(blkid | grep 'LABEL=\"N160FIX\"' | cut -d: -f1 | head -1); echo fix=$FX; mkdir -p /tmp/fx; [ -n \"$FX\" ] && mount -o ro $FX /tmp/fx; ls -la /tmp/fx; "
           "WWW=/usr/share/rudolf/www; [ -f /persist/os/current/index.html ] && WWW=/persist/os/current; echo www=$WWW; "
           "if [ -f /tmp/fx/index.html ]; then cp /tmp/fx/index.html /tmp/page.html; else cp $WWW/index.html /tmp/page.html; fi; "
           "awk 'BEGIN{while((getline l < \"/tmp/probe.html\")>0)p=p l} {if(!d&&(i=index($0,\"<head>\"))>0){print substr($0,1,i+5) p substr($0,i+6);d=1}else print}' /tmp/page.html > /tmp/page2.html; grep -c __lgSeen /tmp/page2.html; chmod 644 /tmp/page2.html; mount --bind /tmp/page2.html $WWW/index.html; "
           "X=/run/n160x; mkdir -p $X; mount -t tmpfs -o exec,mode=755 none $X; "
           # (this tree's rudolfd: started in place of the release's, with its log; its new token is the one the pages get)
           "if [ -f /tmp/fx/rudolfd ]; then cp /tmp/fx/rudolfd $X/rudolfd; chmod 755 $X/rudolfd; mount --bind $X/rudolfd /usr/bin/rudolfd; P=$(pidof rudolfd | tr ' ' '\\n' | sort -n | head -1); kill $P; sleep 2; (setsid /usr/bin/rudolfd >> /run/rudolf/rudolfd.log 2>&1 < /dev/null &); sleep 3; pidof rudolfd; fi; "
           "if [ -f /tmp/fx/rudolf-launch ]; then cp /tmp/fx/rudolf-launch $X/rudolf-launch; chmod 755 $X/rudolf-launch; mount --bind $X/rudolf-launch /usr/bin/rudolf-launch; fi; "
           "if [ -f /tmp/fx/rudolf-native ]; then cp /tmp/fx/rudolf-native $X/rn.real; else cp /usr/bin/rudolf-native $X/rn.real; fi; chmod 755 $X/rn.real; "
           "rm -f /tmp/teval /tmp/teval.out; mkfifo -m 666 /tmp/teval; "
           "G=; if [ -d /tmp/fx/gst ]; then mkdir -p $X/gst; cp /tmp/fx/gst/* $X/gst/; G='GST_PLUGIN_PATH=/run/n160x/gst '; fi; "
           "printf '#!/bin/sh\\nexport %sRUDOLF_NATIVE_STATE=/tmp/nstate RUDOLF_NATIVE_TEST_EVAL=/tmp/teval\\nexec /run/n160x/rn.real \"$@\"\\n' \"$G\" > $X/rn; chmod 755 $X/rn; mount --bind $X/rn /usr/bin/rudolf-native; "
           "pkill -f 'rudolf-native --rudolf-kiosk'; for i in $(seq 1 60); do [ -s /tmp/nstate ] && break; sleep 2; done; head -3 /tmp/nstate; ps w | grep -E 'rn.real|rudolf-native' | grep -v grep | head -5; tail -5 /run/rudolf/native-desktop.log; grep -v '^+' /run/rudolf/session.log | tail -5", 240)
    ok('0d. the native desktop started again with the tests\' views' + (' and this tree\'s files' if FIX else ''), 'screen ' in o and (not FIX or 'index.html' in o), o)
    def state():
        return sh("cat /tmp/nstate", 30, False)
    def st_get(st, k):
        m = re.search(rf'^{k} (.*)$', st, re.M); return m.group(1) if m else None
    st = state(); SW, SH = [int(x) for x in st_get(st, 'screen').split()]
    out('screen', SW, SH, 'signin', st_get(st, 'signin'), 'user', st_get(st, 'user'))
    def at(x, y, btn='left'): g.q.at(x, y, btn, SW, SH)
    def tev(key, expr, t=30):
        n0 = sh("wc -l < /tmp/teval.out 2>/dev/null || echo 0", 20, False).strip().split('\n')[-1]
        put_small(g, expr.replace('\n', ' '), '/tmp/j.js')
        o = sh(f"timeout 5 sh -c '{{ printf \"%s \" {key}; cat /tmp/j.js; echo; }} > /tmp/teval'; for i in $(seq 1 {t}); do tail -n +$(({n0 or 0}+1)) /tmp/teval.out 2>/dev/null | grep -q '^{key} ' && break; sleep 1; done; tail -n +$(({n0 or 0}+1)) /tmp/teval.out | grep -m1 '^{key} '", t + 20, False)
        o = o.strip(); return o[len(key) + 1:] if o.startswith(key + ' ') else o   # (this window's answer only, not a late one of another's)
    # signing in at the native sign-in screen, as the owner does: the password typed
    if st_get(st, 'signin') == '1':
        time.sleep(3); g.q.type('b1'); time.sleep(0.5); g.q.key('ret')
        for _ in range(30):
            time.sleep(3); st = state()
            if st_get(st, 'user') == 'boss': break
    if st_get(st, 'user') != 'boss':
        out('typing at the sign-in screen did not sign in:', st[:800]); shot('signin')
        sh("T=$(busybox wget -q -O- http://127.0.0.1:8080/ | grep -o 'token:\"[0-9a-f]*' | cut -d'\"' -f2); busybox wget -q -O- --header \"X-Rudolf-Token: $T\" --post-data '{\"n\":\"boss\",\"p\":\"b1\"}' http://127.0.0.1:8080/api/desktop/login; echo; pkill -f /run/n160x/rn.real; sleep 10", 60)
        for _ in range(20):
            time.sleep(3); st = state()
            if st_get(st, 'user') == 'boss': break
    ok('0e. signed in on the native sign-in screen', st_get(st, 'user') == 'boss', st[:600])
    t_in = time.time()
    # ---- a log of the run: every few seconds the shell's state (toasts, widgets, windows), the desktop's restarts
    SEEN = []        # (time, toast title)
    WEATHER = []     # (time, state)
    PIDS = set(); HB = []
    def sample(tag=''):
        o = sh("cat /tmp/nstate; echo PID $(pgrep -f /run/n160x/rn.real | tr '\\n' ' '); echo HBAGE $(( $(date +%s) - $(stat -c %Y /run/rudolf/heartbeat 2>/dev/null || echo 0) ))", 30, False)
        tnow = int(time.time() - t_in)
        for t in re.findall(r'^toast \d+ \d+ \d+ \d+ \d+\|(.*)$', o, re.M):
            if not SEEN or SEEN[-1][1] != t or tnow - SEEN[-1][0] > 30: SEEN.append((tnow, t))
        w = st_get(o, 'weather'); WEATHER.append((tnow, w))
        p = st_get(o, 'PID'); PIDS.add((p or '').strip()); HB.append((tnow, st_get(o, 'HBAGE')))
        return o
    def toast_n():
        c = {}
        for t, x in SEEN:
            if c.get(x) and t - c[x][-1] <= 20: continue   # the same pop-up still showing
            c.setdefault(x, []).append(t)
        return c
    def idle(secs, every=4):
        end = time.time() + secs
        while time.time() < end: sample(); time.sleep(every)
    def menu_open(k, q):
        rows = []
        for attempt in range(3):
            # (a menu or pop-up left open, the screen saver: closed first)
            g.q.key('esc'); time.sleep(.5); g.q.move(SW / 2 + attempt * 7, SH / 2, SW, SH); time.sleep(.8)
            st = sample(); h = re.search(r'^hit start \S+ (\d+) (\d+) (\d+) (\d+)$', st, re.M)
            if not h: out('no start button in', st[:400]); time.sleep(3); continue
            x, y, w, hh = map(int, h.groups()); at(x + w / 2, y + hh / 2); time.sleep(1.5)
            if st_get(sample(), 'menu') != '1': out('the start menu did not open; again'); continue
            g.q.type(q); time.sleep(1.5); st = sample()
            rows = re.findall(r'^mrow (\S+) (\d+) (\d+) (\d+) (\d+)$', st, re.M); out('start menu rows for', repr(q), [r[0] for r in rows])
            if rows: break
        row = next((r for r in rows if r[0] == k), None)
        if row: at(int(row[1]) + 30, int(row[2]) + int(row[4]) / 2)
        else: g.q.key('ret')
        return True
    def apps(st): return re.findall(r'^app (\S+) [0-9a-f]+ \d (.*)$', st, re.M)
    def open_app(k, q, wait=25):
        n0 = len([a for a in apps(sample()) if a[0] == k]); menu_open(k, q)
        for _ in range(wait):
            time.sleep(1); st = sample()
            if len([a for a in apps(st) if a[0] == k]) > n0: return st
        return None
    def close_app(k): tev(k, "(()=>{window.__oneAppClose();return 1})()", 10); time.sleep(4)
    def mon_text():
        # the system monitor's values as the shell draws them: read from its picture is not possible here, so from
        # what it reads (rudolfd's stats with the shell's own token: the same request, wg_mon) and the test view
        return sh("busybox wget -q -O- --header \"X-Rudolf-Token: $(busybox wget -q -O- http://127.0.0.1:8080/ | grep -o 'token:\"[0-9a-f]*' | cut -d'\"' -f2)\" http://127.0.0.1:8080/api/stats | head -c 300", 30, False)
    idle(60)
    shot = lambda tag: vm.shot(tag)
    shot('signed-in')

    # ---- 1. an app's window opening: the sign-in screen in any of its frames
    LG = {}
    for k, q in (('notepad', 'notepad'), ('calc', 'calculator'), ('settings', 'settings')):
        st = open_app(k, q)
        if not st: LG[k] = 'did not open'; continue
        time.sleep(16); LG[k] = tev(k, "JSON.stringify({seen:window.__lgSeen,frames:window.__lgFrames,first:window.__lgFirst||0,user:USER&&USER.n,login:getComputedStyle(document.getElementById('login')).display})")
    out('#login in the app windows\' first 15 s:', LG)
    ok('1. an app\'s window opening with the native session shows no sign-in screen in any frame', all(isinstance(v, str) and '"seen":[]' in v for v in LG.values()), LG)
    shot('three-apps')

    # ---- 2. sound from an app window (WebKit) and the desktop's own sounds
    r = tev('notepad', SND_JS); time.sleep(3)
    o = sh(AS_RUDOLF % 'pactl list sink-inputs' + " | grep -E 'Sink Input|application.name|application.process.binary|media.role|Mute|Corked|Sink:' ; " + AS_RUDOLF % 'pactl list sinks short' + "; " + AS_RUDOLF % 'pactl info' + " | grep -i default; "
           "P=$(pgrep -f WebKitWebProcess | head -1); echo WEBPROC $P; tr '\\0' '\\n' < /proc/$P/environ 2>/dev/null | grep -E 'PULSE|XDG_RUNTIME|^HOME|GST'; grep -oE '[^ ]*(gstpulse|gstalsa|libpulse|gstautodetect)[^ ]*' /proc/$P/maps 2>/dev/null | sort -u; "
           "pgrep -a bwrap; ls -la /run/user/1000/pulse/ ~rudolf/.cache/gstreamer-1.0/ 2>&1; tail -40 /run/rudolf/webkit-video.log 2>&1; tail -15 /run/rudolf/pulse.log", 90)
    NAT_SND = tev('notepad', "JSON.stringify(window.__snd)")
    out('Native sound state:', NAT_SND)
    ok('2a. a native app window (WebKit) plays through PulseAudio: a stream from WebKitWebProcess', 'WebKitWebProcess' in o or re.search(r'application.name = "(WebKit|rudolf|Notepad)', o) is not None, o)
    ok('2b. ...the page\'s own sounds start (AudioContext running, <audio> playing)', '"ctx":"running"' in NAT_SND or '"ctx2":"running"' in NAT_SND, NAT_SND)
    # (the Media Player's kind of sound: an <audio> element alone, its stream sampled while it plays: playing, not corked)
    AUD_JS = ("(()=>{const n=44100*6,b=new ArrayBuffer(44+n*2),v=new DataView(b),s=(o,t)=>{for(let i=0;i<t.length;i++)v.setUint8(o+i,t.charCodeAt(i))};s(0,'RIFF');v.setUint32(4,36+n*2,true);s(8,'WAVEfmt ');v.setUint32(16,16,true);v.setUint16(20,1,true);v.setUint16(22,1,true);"
              "v.setUint32(24,44100,true);v.setUint32(28,88200,true);v.setUint16(32,2,true);v.setUint16(34,16,true);s(36,'data');v.setUint32(40,n*2,true);for(let i=0;i<n;i++)v.setInt16(44+i*2,Math.sin(i*2*Math.PI*660/44100)*12000,true);"
              "const a=new Audio(URL.createObjectURL(new Blob([b],{type:'audio/wav'})));window.__ta=a;a.play().then(()=>window.__tp='playing',e=>window.__tp='refused '+e.name);return 1})()")
    time.sleep(4); tev('notepad', AUD_JS)
    o = sh("for i in 1 2 3 4; do sleep 1; " + AS_RUDOLF % 'pactl list sink-inputs' + " | awk '/^Sink Input/{c=\"\"} /Corked:/{c=$2} /media.role = \"music\"|application.process.binary = \"WebKitWebProcess\"/{print \"corked=\" c}' | sort -u | tr '\\n' ' '; echo; done", 60)
    tp = tev('notepad', "window.__tp+'|'+__ta.currentTime.toFixed(1)+'|'+__ta.paused")
    ok('2d. a native app window\'s <audio> (the Media Player\'s kind) plays: its stream not corked while it plays', 'corked=no' in o or 'corked=' not in o and 'playing' in str(tp), [o, tp])
    # (the owner's report 2) a page that mutes a sound (a muted video, the Media Player's mute, an app's volume at 0): later sounds of
    # the app windows still at their own volume (PulseAudio kept the muted stream's volume for every stream of that name)
    tev('notepad', "(()=>{const a=__ta;a.currentTime=0;a.play();setTimeout(()=>{a.muted=true;a.volume=0},700);setTimeout(()=>a.pause(),2000);return 1})()"); time.sleep(4)
    tev('notepad', "(()=>{const a=new Audio(__ta.src);window.__tb=a;a.play();return 1})()"); time.sleep(2)
    WK = ("awk '/^Sink Input/{if(b&&!w)print \"stream \" c\" \"m\" \"v; b=0;w=0;c=\"\";m=\"\";v=\"\"} /Corked:/{c=$2} /Mute:/{m=$2} /^\\tVolume:/{v=$5} /application.process.binary = \"WebKitWebProcess\"/{b=1} "
          "/media.role = \"webaudio\"/{w=1} END{if(b&&!w)print \"stream \" c\" \"m\" \"v}'")
    o = sh(AS_RUDOLF % 'pactl list sink-inputs' + ' | ' + WK + '; ' + AS_RUDOLF % 'pactl list sink-inputs' + " | grep -E 'Sink Input|Volume:|Mute|media.role|restore.id'", 60)
    m = re.findall(r'^stream (\S+) (\S+) (\S+)$', o, re.M)
    ok('2e. after a page muted a sound, the app windows\' next sound plays at its own volume (not muted, not at 0 %)', m and m[-1][1] == 'no' and m[-1][2] not in ('0%',), o)
    tev('notepad', "(()=>{try{__tb.pause()}catch(e){}return 1})()")

    # ---- 3. display settings applied live: from the Settings window, to the shell (rudolfd's desktop/settings) and the
    #         other window (Notepad), within a few seconds, and still so later
    CH = [('darkui', 1), ('accent', '#e11d48'), ('tbstyle', 'glass'), ('h24', True), ('ff', 'Georgia')]
    res3 = {}
    for k, v in CH:
        tev('settings', f"(()=>{{S[{json.dumps(k)}]={json.dumps(v)};SC=cln(S);persistS();try{{applySet()}}catch(e){{}}try{{themeApply()}}catch(e){{}}return JSON.stringify(S[{json.dumps(k)}])}})()")
        t1 = time.time(); seen_rd = seen_np = None
        for _ in range(30):
            time.sleep(.5)
            if seen_np is None:   # (the window first: each look at rudolfd's answer takes a second or more on the serial line)
                r = tev('notepad', f"JSON.stringify(S[{json.dumps(k)}])", 10)
                if r == json.dumps(v): seen_np = round(time.time() - t1, 1)
            if seen_rd is None:
                o = sh(f"busybox wget -q -O- --header \"X-Rudolf-Token: $(busybox wget -q -O- http://127.0.0.1:8080/ | grep -o 'token:\"[0-9a-f]*' | cut -d'\"' -f2)\" http://127.0.0.1:8080/api/desktop/settings | grep -o '\"{k}\":[^,}}]*'", 20, False)
                if f'"{k}":{json.dumps(v)}' in o: seen_rd = round(time.time() - t1, 1)
            if seen_rd is not None and seen_np is not None: break
        res3[k] = (seen_rd, seen_np)
    out('display settings: seconds until the shell\'s source (rudolfd) and the Notepad window had each:', res3)
    ok('3a. each display setting reaches what the shell draws from (rudolfd) within 5 s', all(a is not None and a <= 5 for a, b in res3.values()), res3)
    ok('3b. ...and the other open app window within 2 s', all(b is not None and b <= 2 for a, b in res3.values()), res3)
    shot('settings-changed')

    # ---- the long run: apps opening and closing, a restart of the native desktop, the screen saver, Oracle
    t_end = t_in + MINUTES * 60; rnd = 0; restarted = False; ORACLE = None
    CYCLE = [('explorer', 'file explorer'), ('media', 'media player'), ('taskmgr', 'task manager'), ('paint', 'paint'), ('wordlite', 'wordlite'), ('browser', 'oracle')]
    TERM = None
    while time.time() < t_end:
        rnd += 1; k, q = CYCLE[(rnd - 1) % len(CYCLE)]
        st = open_app(k, q, 40); out(f'round {rnd}: {k}', 'opened' if st else 'did not open')
        if k == 'taskmgr' and st:
            time.sleep(8); out('Task Manager\'s list:', tev('taskmgr', "document.querySelector('.win.oamain .body')?.innerText.slice(0,600)"))
        if k == 'browser': ORACLE = st; time.sleep(30); sh("pkill -f 'rudolf-launch --oracle' ; true", 20)
        idle(20)
        if st and k != 'browser': close_app(k)
        if rnd >= 2 and not TERM:   # (again in the next rounds when the start menu did not open it)
            st = open_app('terminal', 'terminal', 40); time.sleep(12)
            t6 = (apps(st) if st else None, tev('terminal', "JSON.stringify({t:document.title,tabs:[...document.querySelectorAll('.dttab span')].map(e=>e.textContent),deb:typeof DEB=='object'?DEB.st:null,linux:!!document.querySelector('.xterm')})"))
            out('Native Terminal:', t6)
            if st: TERM = t6; close_app('terminal')
        if rnd == 4 and not restarted:
            # the native desktop started again (as the session's watchdog or a crash does): the session goes on
            restarted = True; sh("pkill -f /run/n160x/rn.real; sleep 8; pgrep -f /run/n160x/rn.real", 60); idle(30)
        # a minute and a half without input: the screen saver starts; then the pointer moves
        if rnd in (3, 6):
            idle(100); st = sample(); out('screen saver:', st_get(st, 'saver')); shot(f'saver{rnd}'); at(SW / 2, SH / 3); time.sleep(5); g.q.move(SW / 2 + 40, SH / 3, SW, SH); idle(10)
        if rnd % 2 == 0: shot(f'round{rnd}')
        out('widgets now:', st_get(sample(), 'weather'), mon_text()[:200])
    st = sample(); shot('end')
    o = sh(f"busybox wget -q -O- --header \"X-Rudolf-Token: $(busybox wget -q -O- http://127.0.0.1:8080/ | grep -o 'token:\"[0-9a-f]*' | cut -d'\"' -f2)\" http://127.0.0.1:8080/api/desktop/settings | head -c 1500", 30)
    kept = {k: f'"{k}":{json.dumps(v)}' in o for k, v in CH}
    ok('3c. after the run (apps opened and closed, Oracle, a restart) every changed setting is still there: nothing put the old ones back', all(kept.values()), kept)
    # ---- 4. the widgets
    wx = [w for t, w in WEATHER if w]; last = wx[-1] if wx else ''
    loading = [t for t, w in WEATHER if w and w.startswith('0 ')]
    longest = 0; run0 = None
    for t, w in WEATHER:
        if w and w.startswith('0 '): run0 = t if run0 is None else run0; longest = max(longest, t - run0)
        else: run0 = None
    out('weather states:', sorted(set(w for w in wx)), 'longest "Loading…":', longest, 's')
    ok('4a. the weather widget never says "Loading…" for more than 60 s (its answer, or "needs an internet connection")', longest <= 60, (longest, WEATHER[-20:]))
    o = mon_text(); out('stats:', o)
    ok('4b. the system monitor\'s source (rudolfd\'s stats) answers at the end of the run', '"cpu"' in o, o)
    nw = len([a for a in apps(st)]); out('windows open at the end:', nw, apps(st))
    # ---- 5. notices
    c = toast_n(); out('pop-ups seen (title: times after sign-in):', c)
    rep = {x: ts for x, ts in c.items() if len(ts) > 1}
    ok('5a. no notice popped up twice', not rep, rep)
    ok('5b. no "Real browser tabs are ready" on Native', not any('Real browser tabs' in x for x in c), c)
    o = sh(f"busybox wget -q -O- --header \"X-Rudolf-Token: $(busybox wget -q -O- http://127.0.0.1:8080/ | grep -o 'token:\"[0-9a-f]*' | cut -d'\"' -f2)\" http://127.0.0.1:8080/api/desktop/settings | grep -o '\"notif\":\\[.*' | head -c 3000", 30)
    titles = re.findall(r'"t":"([^"]*)"', o); dup = {t for t in titles if titles.count(t) > 1}
    out('the notifications\' list:', titles)
    ok('5c. the notifications\' list holds each event once', not dup, titles)
    # ---- 6. Terminal
    ok('6. Terminal opens the Terminal with Linux (Debian), as on Standard', TERM and 'Debian' in str(TERM[1]) and 'Command Prompt' not in str(TERM[1]), [TERM, STD_TERM])
    # what happened meanwhile: restarts, heartbeats, logs
    out('rudolf-native processes seen:', PIDS, 'heartbeat ages (max):', max((int(a) for t, a in HB if a and a.lstrip('-').isdigit()), default=None))
    sh("grep -iE 'restart|switch|not responding|did not' /run/rudolf/session.log | grep -v '^+' | tail -20; tail -60 /run/rudolf/native-desktop.log; tail -40 /run/rudolf/page.log; tail -30 /run/rudolf/rudolfd.log; "
       "ls -la /home/rudolf/.rudolf-ls.json; grep -o '\"gen\":[0-9]*' /home/rudolf/.rudolf-ls.json | head -2; tail -20 /persist/var/log/health.log 2>/dev/null", 120)
    # the recordings: the Native start's sound at all
    vm.stop(); vm = None; time.sleep(2)
    for tag in ('s', 'n'):
        p = os.path.abspath(f'n160-{tag}.wav'); out(f'recording {tag}:', os.path.getsize(p) if os.path.exists(p) else 'none', 'loudest', wav_loud(p, 0, 100000))
    ok('2c. sound heard in the Native start\'s recording', isinstance(wav_loud(os.path.abspath('n160-n.wav'), 0, 100000), int) and wav_loud(os.path.abspath('n160-n.wav'), 0, 100000) > 500)
finally:
    try: vm and vm.stop()
    except Exception: pass
    shutil.rmtree(W, ignore_errors=True)
    for p in ('n160-s.wav', 'n160-n.wav'):
        try: os.remove(p)
        except OSError: pass
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
