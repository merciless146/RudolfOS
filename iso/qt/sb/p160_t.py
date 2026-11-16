# Rudolf OS 1.6.0 in QEMU: a Linux app's requests to the desktop on the Native desktop, against Standard (the owner's Vivobook,
# 2026-11-16: Sober, the Roblox client, opened on Native; signing in, its sign-in form spun for ever with no prompt; force-quit from
# Task Manager, it then did not open at all). On the real release (the newest beta, or REL-DIR) with this tree's rudolf-native, page
# and rudolf-portal put in place when P160_FIX has them (an extra disk), else the release as it is (to see the bugs on it).
# The app is a stand-in Flatpak (iso/qt/portalstand.c in a tiny runtime of its own, from a local repository on that disk: no Roblox,
# no account): started from its launcher, it does what its command file says, as Sober and other apps do:
#   keyring: a new keyring with a password (the Secret Service's CreateCollection: gnome-keyring-daemon's prompt, through the
#            Rudolf portal: the desktop's "new keyring password" prompt), then its secret kept in it
#   open:    a file dialog (the portal's FileChooser)          link: a web link (the portal's OpenURI: Oracle)
#   S. Standard (the reference): launched from the desktop page; the prompt within 2 s, typed into; the dialog; the link in Oracle;
#      force-quit while its prompt waits, started again, asked again
#   N. Native: launched from the start menu (as the owner did); with no app window open: the prompt on the screen above everything
#      within 2 s, typed into (the keyboard is the prompt's), the app gets its keyring; the file dialog, once, on top; the link in
#      Oracle's window; force-quit from Task Manager (Ctrl+Alt+Del) while its prompt waits: the prompt goes; started again from the
#      start menu, it opens and asks again; with two app windows open (Notepad, Calculator): its dialog once, by none of them
#   P160_FIX=DIR python3 p160_t.py REL-DIR   (SB_PARTS, QEMU, QEMU_ACCEL as for t160; root)
#   DIR: portalstand and repo/ (the stand-in Flatpak, always); rudolf-native, index.html, rudolf-portal (this tree's, when fixing)
import os, sys, time, re, subprocess, shutil, struct, json, base64
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); FIX = os.environ.get('P160_FIX', '')
LOG = open('p160.txt', 'w'); R = []; T0 = time.time()
def out(*a):
    s = ' '.join(str(x) for x in a); s = f'[{int(time.time() - T0):5d}s] ' + s; print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:1500] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 3000; DRV = os.path.abspath('p160.img'); MEM = int(os.environ.get('MEM', '4096')); W = os.path.abspath('w160p')
shutil.rmtree(W, ignore_errors=True); os.makedirs(W)
FIXED = bool(FIX) and os.path.exists(f'{FIX}/rudolf-native')
out('release', mv('serial'), mv('version'), 'this tree\'s fix:', 'yes' if FIXED else 'no (the release as it is)')
APP = 'org.rudolf.PortalStand'

def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    d = f'{T}/os/system/a'; os.makedirs(d)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs', 'manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{d}/{n}')
    open(f'{d}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n"); open(f'{d}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)
# the extra disk: the stand-in and (when fixing) this tree's programs, with a wrapper that starts rudolf-native with the tests' views
FD = os.path.join(W, 'fix'); shutil.copytree(FIX, FD) if FIX else os.makedirs(FD)
for n in ('rudolf-native', 'rudolf-portal', 'portalstand'):
    if os.path.exists(f'{FD}/{n}'): os.chmod(f'{FD}/{n}', 0o755)
FIXIMG = os.path.abspath('p160fix.img'); subprocess.run(['truncate', '-s', '96M', FIXIMG], check=True)
subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'P160FIX', '-d', FD, FIXIMG], check=True)

def boot(tag):
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf160',
             '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1',
             '-drive', f'file={FIXIMG},format=raw,if=none,id=fx,readonly=on', '-device', 'nvme,drive=fx,serial=p160fix']
    os.makedirs('p160' + tag, exist_ok=True)
    if not os.path.exists(PARTS): open(f'p160{tag}/vars.fd', 'ab').close()
    vm = sbvm.VM('p160' + tag, PARTS, [], secure=False, mem=MEM, extra=extra)
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
def wait_for(js, expr, secs, every=3):
    end = time.time() + secs; r = None
    while time.time() < end:
        r = js(expr, 60)
        if r is True: return True
        time.sleep(every)
    return r
TOK = "$(busybox wget -q -O- http://127.0.0.1:8080/ | grep -o 'token:\"[0-9a-f]*' | cut -d'\"' -f2)"
def api(sh, p, post=False):
    return sh(f"busybox wget -q -O- --header \"X-Rudolf-Token: {TOK}\" {'--post-data= ' if post else ''}'http://127.0.0.1:8080/api/{p}'", 60, False)

# this tree's page and rudolf-portal (bound over the release's; the account's portal is started at its sign-in, after this)
APPLY = ("FX=; for d in /dev/nvme*n1; do blkid $d 2>/dev/null | grep -q P160FIX && FX=$d; done; echo fix=$FX; mkdir -p /tmp/fx; mountpoint -q /tmp/fx || mount -o ro $FX /tmp/fx; ls -la /tmp/fx; "
         "X=/run/p160x; mkdir -p $X; mountpoint -q $X || mount -t tmpfs -o exec,mode=755 none $X; "
         "WWW=/usr/share/rudolf/www; [ -f /persist/os/current/index.html ] && WWW=/persist/os/current; "
         "if [ -f /tmp/fx/index.html ]; then cp /tmp/fx/index.html $X/page.html; chmod 644 $X/page.html; mountpoint -q $WWW/index.html || mount --bind $X/page.html $WWW/index.html; grep -c 'da0==.portal.' $WWW/index.html; fi; "
         "if [ -f /tmp/fx/rudolf-portal ]; then mountpoint -q /usr/bin/rudolf-portal || mount --bind /tmp/fx/rudolf-portal /usr/bin/rudolf-portal; cmp /tmp/fx/rudolf-portal /usr/bin/rudolf-portal && echo portal-bound; "
         # (the other half, from the branch linux-app-links (not on main yet): the account's portal, its own Linux user's, may leave
         # its questions in the desktop user's folders (sticky, 1733, as its session and rudolfd make them); without it every request
         # of the account's apps is lost, on Standard and Native alike)
         "chmod 1733 /run/rudolf/portal/req /run/rudolf/portal/resp; ls -ld /run/rudolf/portal/req; fi")
# the stand-in Flatpak, installed from the extra disk's repository (no network)
INSTALL = (f"flatpak remote-add --system --no-gpg-verify --if-not-exists pstand file:///tmp/fx/repo 2>&1; flatpak install --system -y --noninteractive pstand {APP} 2>&1 | tail -3; "
           f"flatpak list --system --app --columns=application | grep -x {APP}; ls /var/lib/flatpak/exports/share/applications/")
AHOME = "H=$(sed -n 's/^HOME=//p' /run/rudolf/acct-session.env 2>/dev/null); [ -n \"$H\" ] || H=/home/rudolf; echo $H"
def data_dir(sh):
    h = sh(AHOME, 30, False).strip().split('\n')[-1]; return h, f'{h}/.var/app/{APP}/data'
def cmd_file(sh, dd, h, lines):
    b = base64.b64encode(('\n'.join(lines) + '\n').encode()).decode()
    sh(f"mkdir -p {dd}; echo {b} | base64 -d > {dd}/portalstand.cmd; U=$(stat -c %u:%g {h}); chown -R $U {h}/.var; cat {dd}/portalstand.cmd", 30, False)
def app_log(sh, dd): return sh(f"cat {dd}/portalstand.log 2>/dev/null", 30, False)
def wait_log(sh, dd, pat, secs, n=1):
    end = time.time() + secs; lg = ''
    while time.time() < end:
        lg = app_log(sh, dd)
        if len(re.findall(pat, lg)) >= n: return True, lg
        time.sleep(3)
    return False, lg
def started(lg): return len(re.findall(r' started \d+', lg))
# (inside the guest: from the app's "keyring prompting" (its own clock) to when CHECK is true, in ms; CHECK a shell condition)
def prompt_ms(sh, dd, check, n):
    # (the guest's own uptime at both ends: when the app's line is seen (polled every 50 ms) and when CHECK is true)
    o = sh(f"z() {{ usleep 50000 2>/dev/null || sleep 0.05 2>/dev/null || sleep 1; }}; L={dd}/portalstand.log; i=0; while [ $(grep -c 'keyring prompting' $L 2>/dev/null) -lt {n} ] && [ $i -lt 2400 ]; do z; i=$((i+1)); done; "
           f"T0=$(cut -d' ' -f1 /proc/uptime); i=0; while ! {check} && [ $i -lt 600 ]; do z; i=$((i+1)); done; T1=$(cut -d' ' -f1 /proc/uptime); {check} || T1=-1; "
           "echo \"MS $(awk -v a=$T0 -v b=$T1 'BEGIN{if(b<0)print -1; else printf \"%d\", (b-a)*1000}')\"", 180, False)
    m = re.search(r'MS (-?\d+)', o); return int(m.group(1)) if m else None

make_drive(DRV)
vm = None
try:
    # ================================================================== Standard (Chromium): the reference
    vm, g, up = boot('s'); js = jsfn(g); sh = lambda c, t=60, show=True: run(g, c, t, show)
    ok('S0. the release starts (Standard)', up)
    if not up: raise RuntimeError('did not start')
    wait_for(js, "typeof acctNew=='function'&&typeof userLogin=='function'", 600)
    o = sh(APPLY, 120)
    if FIXED: js("(()=>{setTimeout(()=>location.reload(),300);return 1})()", 30); time.sleep(20); wait_for(js, "typeof acctNew=='function'&&typeof userLogin=='function'", 600)
    ok('S0b. this tree\'s page and rudolf-portal in place' if FIXED else 'S0b. the release as it is', (not FIXED) or ('portal-bound' in o and js("typeof krPrompt=='function'&&/da0=='portal'/.test(document.documentElement.innerHTML)", 30) is True), o[-600:])
    js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};return 1})()")
    out('account', js("(async()=>{ACCTS.length=0;const a=await acctNew('john','j1','admin');ACCTS.push(a);saveUser();return a.id})()", 120))
    SI = ("(async()=>{try{const a=ACCTS.find(x=>x.n=='john');if(USER)userLogout();USER=a;await userLogin(a,'j1');lastInput=Date.now();"
          "const L=document.getElementById('login');if(L)L.style.display='none';return 'in '+USER.n}catch(e){return 'ERR '+e.message}})()")
    out('sign-in:', js(SI, 300))
    end = time.time() + 600
    while time.time() < end and js("typeof SHELL!='undefined'&&!!SHELL", 20) is not True: time.sleep(10)
    if js("!!USER&&USER.n=='john'", 20) is not True: out('sign-in again after the move into the shell:', js(SI, 300))
    wait_for(js, "!!(USER&&USER.disk&&hdOn())", 300)
    js("(()=>{window.UNL=()=>{const L=document.getElementById('login');if(L&&USER)L.style.display='none'};UNL();return 1})()", 30)
    sh("for i in $(seq 1 40); do [ -f /run/rudolf/acct-session.env ] && pgrep -f /usr/bin/rudolf-portal >/dev/null && break; sleep 3; done; pgrep -af rudolf-portal; grep -v PASS /run/rudolf/acct-session.env", 150)
    o = sh(INSTALL, 300)
    ok('S1. the stand-in Flatpak installs (a local repository)', APP in o, o)
    r = js(f"(async()=>{{await lxInstalled(true);lxRegister();const x=lxLaunchers().find(x=>x.app=='{APP}');return x?x.name+'|'+x.desktop:'none'}})()", 120); out('its launcher:', r)
    h, dd = data_dir(sh)
    cmd_file(sh, dd, h, ['keyring SoberS pw-s1', 'open', 'link https://example.org/signin-s'])
    js("(()=>{window.__krT=0;window.__krN=0;new MutationObserver(()=>{const k=document.querySelector('.krp60');if(k&&!k.__seen){k.__seen=1;__krN++;__krT=Date.now()}}).observe(document.body,{childList:true});return 1})()", 30)
    LAUNCH_S = f"(()=>{{UNL();const x=lxLaunchers().find(x=>x.app=='{APP}');if(!x)return 'none';lxRun(x);return 'started'}})()"
    out('launch:', js(LAUNCH_S, 30))
    s1, lg = wait_log(sh, dd, r'keyring prompting', 120)
    time.sleep(3); kr = js("JSON.stringify({n:__krN,t:__krT,txt:(document.querySelector('.krp60 .krm')||{}).textContent||'',who:(document.querySelector('.krp60 .kra')||{}).textContent||''})", 30)
    try: kr = json.loads(kr)
    except Exception: kr = {'raw': kr}
    m = re.search(r'(\d+\.\d+) keyring prompting', lg); dt = int(kr.get('t', 0) - float(m.group(1)) * 1000) if m and kr.get('t') else None
    vm.shot('p160s-keyring')
    ok(f'S2. Standard: the app\'s new keyring: the desktop\'s prompt within 2 s ({dt} ms), naming the app', s1 and kr.get('n') == 1 and dt is not None and dt <= 2000 and 'new keyring' in kr.get('txt', '') and kr.get('who', '').lower() == 'portalstand', [kr, lg[-500:]])
    g.q.type('pw-s1'); g.q.key('tab'); g.q.type('pw-s1'); g.q.key('ret')
    c1, lg = wait_log(sh, dd, r'keyring created', 60)
    ok('S3. Standard: typed in, answered: the app has its keyring and its secret', c1, lg[-600:])
    r = wait_for(js, "document.querySelectorAll('.lxfd').length==1", 60, 2); vm.shot('p160s-open')
    ok('S4. Standard: its file dialog, once', r is True, r)
    g.q.key('esc'); c1, lg = wait_log(sh, dd, r'open response 1', 60)
    r = wait_for(js, "wins.some(w=>w.body&&w.body.querySelector('.otabs')&&/example\\.org/.test(w.body.querySelector('.ad').value))", 90, 3); vm.shot('p160s-link')
    l1, lg = wait_log(sh, dd, r'link response 0', 30)
    ok('S5. Standard: cancelled, answered once; its link opens in Oracle', c1 and l1 and r is True and len(re.findall('open response', lg)) == 1, [r, lg[-600:]])
    # force-quit while its prompt waits; started again
    js("(async()=>{for(const w of wins.slice())try{w.close()}catch(e){}return 1})()", 30); time.sleep(3)
    cmd_file(sh, dd, h, ['keyring StuckS pw-s2']); n0 = started(app_log(sh, dd)); out('launch:', js(LAUNCH_S, 30))
    wait_log(sh, dd, r'keyring prompting', 120, 2); time.sleep(3)
    r = js(f"(async()=>{{const l=await apps57();if(!l)return 'Task Manager has no list of running apps (apps57: null)';const x=l.find(a=>a.id=='flatpak:{APP}');if(!x)return 'not listed: '+JSON.stringify(l).slice(0,400);return String(await forceQuit57(x,'PortalStand'))}})()", 60)
    time.sleep(8); left = js("document.querySelectorAll('.krp60').length", 30); reqs = sh("ls /run/rudolf/portal/req/ 2>/dev/null; pgrep -f 'portalstand app' || echo none", 30, False)
    ok('S6. Standard: Task Manager lists it and force-quits it while its prompt waits: the app is ended and its prompt goes', r == 'true' and left == 0 and 'none' in reqs and not re.search(r'^k', reqs, re.M), [r, left, reqs])
    cmd_file(sh, dd, h, ['keyring AgainS pw-s3']); out('launch:', js(LAUNCH_S, 30))
    s2, lg = wait_log(sh, dd, r'keyring prompting', 120, 3)
    ok('S7. Standard: started again, it opens and asks again', s2 and started(lg) > n0, lg[-500:])
    time.sleep(4); g.q.type('pw-s3'); g.q.key('tab'); g.q.type('pw-s3'); g.q.key('ret'); c1, lg = wait_log(sh, dd, r'keyring created /org/freedesktop/secrets/collection/AgainS', 60)
    ok('S8. Standard: ...answered: the app has its keyring', c1, lg[-400:])
    out(sh(f"tail -5 /run/rudolf/portal-*.log; tail -3 /run/rudolf/apps/{APP}.log", 30, False))
    js("(async()=>{try{fsFlush()}catch(e){}try{await __rudolfLsFlush()}catch(e){}return 1})()", 60)
    sh("pkill -f 'portalstand app'; echo native > /persist/etc/rudolf-desktop-engine; rm -f /persist/etc/rudolf-desktop-engine.failed; sync", 60)
    time.sleep(5); vm.stop(); vm = None; time.sleep(3)

    # ================================================================== Native
    vm, g, up = boot('n'); sh = lambda c, t=60, show=True: run(g, c, t, show)
    ok('N0. the release starts again (Native)', up)
    if not up: raise RuntimeError('did not start')
    o = sh("for i in $(seq 1 60); do pgrep -f /usr/bin/rudolf-native >/dev/null && break; sleep 3; done; cat /run/rudolf/desktop-engine", 240)
    ok('N0b. the Native desktop runs', 'native' in o, o)
    o = sh(APPLY + "; X=/run/p160x; if [ -f /tmp/fx/rudolf-native ]; then cp /tmp/fx/rudolf-native $X/rn.real; else cp /usr/bin/rudolf-native $X/rn.real; fi; chmod 755 $X/rn.real; "
           "printf '#!/bin/sh\\nexport RUDOLF_NATIVE_STATE=/tmp/nstate RUDOLF_NATIVE_TEST_EVAL=/tmp/teval\\nexec /run/p160x/rn.real \"$@\"\\n' > $X/rn; chmod 755 $X/rn; mount --bind $X/rn /usr/bin/rudolf-native; "
           "rm -f /tmp/teval /tmp/teval.out; mkfifo -m 666 /tmp/teval; "
           "P0=$(pgrep -f 'rudolf-native [-]-rudolf-kiosk'); kill $P0; sleep 3; kill -9 $P0 2>/dev/null; for i in $(seq 1 90); do [ -s /tmp/nstate ] && break; sleep 2; done; head -3 /tmp/nstate", 300)
    state = lambda: sh("cat /tmp/nstate", 30, False)
    def st_get(st, k):
        m = re.search(rf'^{k} (.*)$', st, re.M); return m.group(1) if m else None
    st = state()
    if not st_get(st, 'screen'): raise RuntimeError('no state from the native desktop')
    SW, SH = [int(x) for x in st_get(st, 'screen').split()]
    def at(x, y, btn='left'): g.q.at(x, y, btn, SW, SH)
    def hit(st, k):
        m = re.search(rf'^hit {k} (\d+) (\d+) (\d+) (\d+)$', st, re.M); return (int(m.group(1)) + int(m.group(3)) / 2, int(m.group(2)) + int(m.group(4)) / 2) if m else None
    def tev(key, expr, t=30):
        n0 = sh("wc -l < /tmp/teval.out 2>/dev/null || echo 0", 20, False).strip().split('\n')[-1]
        open(f'{W}/j.js', 'w').write(expr.replace('\n', ' ')); g.put(f'{W}/j.js', '/tmp/j.js', '644')
        o = sh(f"timeout 5 sh -c '{{ printf \"%s \" {key}; cat /tmp/j.js; echo; }} > /tmp/teval'; for i in $(seq 1 {t}); do n=$(wc -l < /tmp/teval.out 2>/dev/null || echo 0); [ $n -gt {n0 or 0} ] && break; sleep 1; done; grep '^{key} ' /tmp/teval.out | tail -1", t + 10, False)
        o = o.strip(); return o[len(key) + 1:] if o.startswith(key + ' ') else o
    if st_get(st, 'signin') == '1':
        time.sleep(3); g.q.type('j1'); time.sleep(0.5); g.q.key('ret')
        for _ in range(30):
            time.sleep(3); st = state()
            if st_get(st, 'user') == 'john': break
    ok('N0c. signed in on the native sign-in screen', st_get(st, 'user') == 'john', st[:600])
    sh("for i in $(seq 1 40); do [ -f /run/rudolf/acct-session.env ] && pgrep -f /usr/bin/rudolf-portal >/dev/null && break; sleep 3; done; pgrep -af rudolf-portal; mount | grep -c rudolf-portal", 150)
    for _ in range(40):
        st = state()
        if (st_get(st, 'portal') or '').startswith('ready'): break
        time.sleep(4)
    out('portal:', st_get(st, 'portal'), [l for l in st.split('\n') if l.startswith(('page ', 'app '))])
    good = (st_get(st, 'portal') or '').startswith('ready') and not re.search(r'^app ', st, re.M)
    ok('N1. the shell\'s portal page is ready after signing in, unseen', good, [st_get(st, 'portal'), [l for l in st.split('\n') if l.startswith(('page', 'app'))]])
    if not good: out(sh("grep -iE 'portal|page|stopped' /run/rudolf/native-desktop.log | tail -25; ls -la /run/rudolf/portal /run/rudolf/portal/req", 30, False))
    h, dd = data_dir(sh)
    def menu_launch(q, wait_log_pat=r' started \d+', n=1, secs=90):
        st = state(); hs = hit(st, 'start \\S+')
        if hs: at(*hs)
        time.sleep(2); g.q.type(q)
        for _ in range(10):
            time.sleep(2); st = state()
            if re.search(rf'^srch .*{q}', st, re.M): break
        out('the start menu found:', [l for l in st.split('\n') if l.startswith(('srch ', 'menuq'))][:4]); g.q.key('ret')
        return wait_log(sh, dd, wait_log_pat, secs, n)
    ONTOP = "grep -q '^ontop do-portal$' /tmp/nstate"
    # ---- launched from the start menu, no app window open: the keyring's prompt
    cmd_file(sh, dd, h, ['keyring SoberN pw-n1', 'open', 'link https://example.org/signin-n'])
    st = state(); ok('N2a. no app window is open', not re.search(r'^app ', st, re.M), [l for l in st.split('\n') if l.startswith('app ')])
    n0 = started(app_log(sh, dd)); s1, lg = menu_launch('PortalStand', r' started \d+', n0 + 1)
    ok('N2b. Native: the app opens from the start menu', s1, lg[-400:])
    if not s1: out('started through rudolfd instead (to go on):', api(sh, f'apps/run?id={APP}&desktop={APP}', True))
    ms = prompt_ms(sh, dd, ONTOP, 1); vm.shot('p160n-keyring')
    kq = tev('do-portal', "(()=>{const o=document.querySelector('.krp60');return o?[o.querySelector('.kra').textContent,o.querySelector('.krm').textContent,document.activeElement&&document.activeElement.className].join('|'):'none'})()")
    st = state(); inwin = [l for l in st.split('\n') if l.startswith(('ontop', 'client')) and 'do-portal' in l]
    ok(f'N3. Native, no app window open: the keyring\'s prompt on the screen, above everything, focused, within 2 s ({ms} ms)', ms is not None and 0 <= ms <= 2000 and kq.lower().startswith('portalstand|') and 'new keyring' in kq and kq.endswith('kp1'), [ms, kq, inwin])
    g.q.type('pw-n1'); g.q.key('tab'); g.q.type('pw-n1'); g.q.key('ret')
    c1, lg = wait_log(sh, dd, r'keyring created', 60)
    ok('N4. Native: typed in as a person would, answered: the app has its keyring and its secret', c1, lg[-600:])
    ms = prompt_ms(sh, dd, "grep -q 'doing open' " + dd + "/portalstand.log && " + ONTOP, 1); time.sleep(2)
    fd = tev('do-portal', "document.querySelectorAll('.lxfd').length+'|'+((document.querySelector('.lxfd h3')||{}).textContent||'')"); vm.shot('p160n-open')
    ok(f'N5. Native: its file dialog, once, on top ({ms} ms)', fd.startswith('1|') and 'PortalStand' in fd, [fd, ms])
    g.q.key('esc'); c1, lg = wait_log(sh, dd, r'open response 1', 60); l1, lg = wait_log(sh, dd, r'link response 0', 60)
    o1 = False
    for _ in range(20):
        st = state()
        if re.search(r'^client .* app=browser ', st, re.M): o1 = True; break
        time.sleep(3)
    vm.shot('p160n-link')
    ok('N6. Native: cancelled, answered once; its link opens in Oracle\'s window', c1 and l1 and o1 and len(re.findall('open response', lg)) == 1, [o1, lg[-500:], [l for l in st.split('\n') if l.startswith('client')]])
    # ---- force-quit from Task Manager while the prompt waits (as the owner did), then started again from the start menu
    cmd_file(sh, dd, h, ['keyring StuckN pw-n2']); n0 = started(app_log(sh, dd))
    s1, lg = menu_launch('PortalStand', r' started \d+', n0 + 1)
    if not s1: out('started through rudolfd instead (to go on):', api(sh, f'apps/run?id={APP}&desktop={APP}', True))
    ms = prompt_ms(sh, dd, ONTOP, 2); out('the prompt after', ms, 'ms')
    g.q.key('ctrl', 'alt', 'delete'); tm = False
    for _ in range(30):
        time.sleep(1); st = state()
        if re.search(r'^client .* focus=1 app=taskmgr ', st, re.M): tm = True; break
    # (Task Manager's own Force quit, as its button does; an async answer is kept in the page and read after)
    tev('taskmgr', f"(()=>{{window.__fq='…';(async()=>{{const l=await apps57();if(!l)return 'Task Manager has no list of running apps (apps57: null)';const x=l.find(a=>a.id=='flatpak:{APP}');if(!x)return 'not listed: '+JSON.stringify(l).slice(0,300);return String(await forceQuit57(x,'PortalStand'))}})().then(v=>window.__fq=v,e=>window.__fq='ERR '+e.message);return 1}})()", 20)
    r = ''
    for _ in range(10):
        time.sleep(2); r = tev('taskmgr', "String(window.__fq)", 20)
        if r != '…': break
    time.sleep(8); st = state(); reqs = sh("ls /run/rudolf/portal/req/ 2>/dev/null; pgrep -f 'portalstand app' || echo none", 30, False)
    vm.shot('p160n-forcequit')
    ok('N7. Native: Task Manager (Ctrl+Alt+Del) force-quits it while its prompt waits: the app ends, its prompt goes', tm and r == 'true' and not re.search(r'^ontop do-portal$', st, re.M) and 'none' in reqs and not re.search(r'^k', reqs, re.M), [tm, r, reqs, [l for l in st.split('\n') if l.startswith('ontop')]])
    tev('taskmgr', "(()=>{window.__oneAppClose();return 1})()", 10); time.sleep(4)
    cmd_file(sh, dd, h, ['keyring AgainN pw-n3']); n0 = started(app_log(sh, dd))
    s1, lg = menu_launch('PortalStand', r' started \d+', n0 + 1)
    ok('N8. Native: after the force-quit it opens again from the start menu', s1, lg[-400:])
    if not s1: out('started through rudolfd instead (to go on):', api(sh, f'apps/run?id={APP}&desktop={APP}', True))
    ms = prompt_ms(sh, dd, ONTOP, 3); vm.shot('p160n-again')
    ok(f'N9. Native: ...and its keyring\'s prompt is on top again within 2 s ({ms} ms)', ms is not None and 0 <= ms <= 2000, ms)
    g.q.type('pw-n3'); g.q.key('tab'); g.q.type('pw-n3'); g.q.key('ret')
    c1, lg = wait_log(sh, dd, r'keyring created /org/freedesktop/secrets/collection/AgainN', 60)
    ok('N10. Native: ...answered: the app has its keyring', c1, lg[-400:])
    sh("pkill -f 'portalstand app'", 20, False)
    # ---- two app windows open: the dialog once, by none of them
    def open_app(k, q, wait=40):
        st = state(); hs = hit(st, 'start \\S+')
        if hs: at(*hs)
        time.sleep(1.5); g.q.type(q); time.sleep(2.5); g.q.key('ret')
        for _ in range(wait):
            time.sleep(1)
            if re.search(rf'^client .* app={k} ', state(), re.M): return True
        return False
    out('Notepad:', open_app('notepad', 'notepad'), 'Calculator:', open_app('calc', 'calculator')); time.sleep(8)
    cmd_file(sh, dd, h, ['open']); n0 = started(app_log(sh, dd))
    s1, lg = menu_launch('PortalStand', r' started \d+', n0 + 1)
    if not s1: out('started through rudolfd instead (to go on):', api(sh, f'apps/run?id={APP}&desktop={APP}', True))
    for _ in range(20):
        st = state()
        if re.search(r'^ontop do-portal$', st, re.M): break
        time.sleep(2)
    time.sleep(2); fd = tev('do-portal', "document.querySelectorAll('.lxfd').length"); np_ = tev('notepad', "document.querySelectorAll('.rmodal').length"); ca = tev('calc', "document.querySelectorAll('.rmodal').length")
    vm.shot('p160n-two')
    ok('N11. Native, Notepad and Calculator open: the app\'s file dialog once, above them, in neither window', re.search(r'^ontop do-portal$', st, re.M) and fd == '1' and np_ == '0' and ca == '0', [fd, np_, ca, [l for l in st.split('\n') if l.startswith(('ontop', 'app '))]])
    g.q.key('esc'); c1, lg = wait_log(sh, dd, r'open response 1', 60); time.sleep(5); lg = app_log(sh, dd)
    ok('N12. Native: ...answered once', c1 and lg.count('open response') == len(re.findall(r'doing open', lg)), lg[-500:])
    out(sh(f"pkill -f 'portalstand app'; tail -8 /run/rudolf/portal-*.log; tail -5 /run/rudolf/apps/{APP}.log; grep -E 'portal' /run/rudolf/native-desktop.log | tail -15", 30, False))
except RuntimeError as e:
    ok('the run went on: ' + str(e), False)
finally:
    try:
        if vm: vm.stop()
    except Exception: pass
    shutil.rmtree(W, ignore_errors=True)
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
