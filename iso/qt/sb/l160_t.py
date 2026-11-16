# Rudolf OS 1.6.0 in QEMU: a Linux app's sign-in through a web page, both ways (the owner's Vivobook report of 2026-11-16:
# "Continue with Google" in Claude's Linux desktop app flashed and nothing opened). On the real release (the newest beta, or
# REL-DIR) with this tree's fix put in place (L160_FIX: the page, rudolfd, rudolf-debian, rudolf-open; an extra disk):
#   out:  an app in Debian runs xdg-open (as Electron's shell.openExternal does) with a sign-in page; rudolf-open (bound
#         over Debian's /usr/local/bin/xdg-open by rudolf-debian) asks the Rudolf portal on the account's session bus, the
#         desktop opens the page in Oracle;
#   back: the page sends Oracle to teststand://done?code=CODE (a redirect, as a site does after signing in); Oracle asks
#         "Open Test Stand?" and, after Open, the app's launcher (MimeType x-scheme-handler/teststand) is started with the
#         link: the app receives the code.
# The app is iso/qt/teststand.c (static, no toolkit), the page its own (127.0.0.1:8600), Debian a stand-in with busybox
# (as t160's and n160's). First on Standard (Chromium, Oracle's real tabs), then on Native (its Oracle window: Chromium),
# each with an account signed in (its own Linux user, its session bus and portal). Flatpak: the guest has no runtime to
# install an app offline, so the Flatpak path (rudolf-apps launch with the link) is covered by the host test only.
#   L160_FIX=DIR python3 l160_t.py REL-DIR   (SB_PARTS, QEMU, QEMU_ACCEL as for t160; root; L160_ENGINES=standard,native)
import os, sys, time, re, subprocess, shutil, struct, json, base64, random
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE); ISO = os.path.dirname(QT)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); FIX = os.environ.get('L160_FIX', '')
ENGINES = os.environ.get('L160_ENGINES', 'standard,native').split(',')
LOG = open('l160.txt', 'w'); R = []; T0 = time.time()
def out(*a):
    s = ' '.join(str(x) for x in a); s = f'[{int(time.time() - T0):5d}s] ' + s; print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:1500] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 3000; DRV = os.path.abspath('l160.img'); MEM = int(os.environ.get('MEM', '4096')); W = os.path.abspath('w160l')
shutil.rmtree(W, ignore_errors=True); os.makedirs(W)
out('release', mv('serial'), mv('version'), 'fix', FIX or '(none: the release as it is)')

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
FIXIMG = ''
if FIX:
    FIXIMG = os.path.abspath('l160fix.img'); subprocess.run(['truncate', '-s', '96M', FIXIMG], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'L160FIX', '-d', FIX, FIXIMG], check=True)

def boot(tag):
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf160',
             '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1']
    if FIXIMG: extra += ['-drive', f'file={FIXIMG},format=raw,if=none,id=fx,readonly=on', '-device', 'nvme,drive=fx,serial=l160fix']
    os.makedirs('l160' + tag, exist_ok=True)
    if not os.path.exists(PARTS): open(f'l160{tag}/vars.fd', 'ab').close()
    vm = sbvm.VM('l160' + tag, PARTS, [], secure=False, mem=MEM, extra=extra)
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
# (this tree's cdp-eval when given, with CDP_TARGET: the target pick() chose, as Standard's shell can leave more than one
# page of the desktop's in Chromium's list, and cdp-eval would take the first)
CDP = "T=$(cat /run/l160x/target 2>/dev/null); if [ -x /run/l160x/cdp-eval ]; then CDP_TARGET=$T /run/l160x/cdp-eval"
def jsfn(g):
    def js1(e, t):
        b = base64.b64encode(e.encode()).decode()
        if len(b) < 800: o = g.sh(f'{CDP} "$(echo {b} | base64 -d)"; else cdp-eval "$(echo {b} | base64 -d)"; fi 2>&1', t)
        else:
            open(f'{W}/l.js', 'w').write(e); g.put(f'{W}/l.js', '/tmp/l.js', '644')
            o = g.sh(f'{CDP} "$(cat /tmp/l.js)"; else cdp-eval "$(cat /tmp/l.js)"; fi 2>&1', t)
        m = re.search(r'\{"id":1.*\}', o)
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
def pick(g, js, show=True):
    """the desktop's page among Chromium's targets: the one in the shell (Oracle's real tabs), with an Oracle window if any"""
    o = g.sh("busybox wget -q -O- http://127.0.0.1:9222/json/list 2>&1", 30)
    try: L = json.loads(o[o.index('['):o.rindex(']') + 1])
    except Exception: L = []
    best, info = None, []
    for t in L:
        if not str(t.get('url', '')).startswith('http://127.0.0.1:8080/'): continue
        g.sh(f"echo {t['id']} > /run/l160x/target", 10)
        r = js("JSON.stringify({shell:typeof SHELL!='undefined'&&!!SHELL,oracle:typeof wins!='undefined'&&wins.some(w=>w.body&&w.body.querySelector('.otabs')),user:typeof USER!='undefined'&&USER&&USER.n})", 20)
        try: d = json.loads(r)
        except Exception: d = {}
        sc = (2 if d.get('shell') else 0) + (1 if d.get('oracle') else 0)
        info.append([t['id'][:8], t.get('type'), t.get('url', '')[:70], d])
        if best is None or sc > best[0]: best = (sc, t['id'])
    g.sh(f"echo {best[1] if best else ''} > /run/l160x/target", 10)
    if show: out('the desktop\'s pages in Chromium:', info, '- chosen:', best)
    return best
def wait_for(js, expr, secs, every=5):
    end = time.time() + secs; r = None
    while time.time() < end:
        r = js(expr, 60)
        if r is True: return True
        time.sleep(every)
    return r

# this tree's files in place of the release's, until the next start: the page (bound over the one rudolfd serves), rudolfd
# (started again in place of the release's), rudolf-portal (the account's, started at sign-in), rudolf-debian and
# rudolf-open (in a copy of /usr/lib/rudolf bound over it)
APPLY = ("FX=$(blkid | grep 'LABEL=\"L160FIX\"' | cut -d: -f1 | head -1); echo fix=$FX; mkdir -p /tmp/fx; [ -n \"$FX\" ] && mount -o ro $FX /tmp/fx; ls -la /tmp/fx; "
         "X=/run/l160x; mkdir -p $X; mountpoint -q $X || mount -t tmpfs -o exec,mode=755 none $X; "
         "WWW=/usr/share/rudolf/www; [ -f /persist/os/current/index.html ] && WWW=/persist/os/current; echo www=$WWW; "
         "if [ -f /tmp/fx/index.html ]; then cp /tmp/fx/index.html $X/page.html; chmod 644 $X/page.html; mount --bind $X/page.html $WWW/index.html; grep -c 'function lkOpen' $WWW/index.html; fi; "
         "if [ -f /tmp/fx/rudolf-debian ]; then rudolf-debian stop 2>/dev/null; cp /tmp/fx/rudolf-debian $X/rudolf-debian; chmod 755 $X/rudolf-debian; mount --bind $X/rudolf-debian /usr/bin/rudolf-debian; fi; "
         "if [ -f /tmp/fx/rudolf-open ]; then mkdir -p $X/lib; cp -a /usr/lib/rudolf/. $X/lib/; cp /tmp/fx/rudolf-open $X/lib/rudolf-open; chmod 755 $X/lib/rudolf-open; mount --bind $X/lib /usr/lib/rudolf; ls -l /usr/lib/rudolf/rudolf-open; fi; "
         "if [ -f /tmp/fx/cdp-eval ]; then cp /tmp/fx/cdp-eval $X/cdp-eval; chmod 755 $X/cdp-eval; fi; "
         "if [ -f /tmp/fx/rudolf-portal ]; then cp /tmp/fx/rudolf-portal $X/rudolf-portal; chmod 755 $X/rudolf-portal; mount --bind $X/rudolf-portal /usr/bin/rudolf-portal; fi; "
         "if [ -f /tmp/fx/rudolfd ]; then cp /tmp/fx/rudolfd $X/rudolfd; chmod 755 $X/rudolfd; mount --bind $X/rudolfd /usr/bin/rudolfd; P=$(pidof rudolfd | tr ' ' '\\n' | sort -n | head -1); kill $P; sleep 2; (setsid /usr/bin/rudolfd >> /run/rudolf/rudolfd.log 2>&1 < /dev/null &); sleep 3; echo rudolfd $(pidof rudolfd); fi")
# Debian: a stand-in with busybox (installed, as t160's), the test app and its launcher (the scheme it opens)
DEBIAN = ("D=/persist/debian; mkdir -p $D/bin $D/lib $D/lib64 $D/etc $D/proc $D/sys $D/dev $D/tmp $D/run $D/home/rudolf $D/root $D/.oldroot $D/usr/local/bin $D/usr/share/applications; cp /bin/busybox $D/bin/; "
          "for l in $(ldd /bin/busybox 2>/dev/null | grep -o '/[^ ]*'); do mkdir -p $D$(dirname $l); cp -L $l $D$l; done; "
          "for c in sh cat echo ls wget id env; do ln -sf busybox $D/bin/$c; done; echo 'rudolf:x:1000:1000::/home/rudolf:/bin/sh' > $D/etc/passwd; echo test > $D/etc/rudolf-debian; touch $D/.rudolf-ready; "
          "cp /tmp/fx/teststand $D/usr/local/bin/teststand; chmod 755 $D/usr/local/bin/teststand; "
          "printf '[Desktop Entry]\\nType=Application\\nName=Test Stand\\nComment=Signs in through a web page\\nExec=/usr/local/bin/teststand %%u\\nMimeType=x-scheme-handler/teststand;\\nCategories=Utility;\\n' > $D/usr/share/applications/teststand.desktop; "
          "chown -R 200000:200000 $D; rudolf-debian status 2>&1 | head -5; rudolf-debian apps 2>&1 | head -c 600")
# the account's own folder (Debian's home while it is signed in)
AHOME = "H=$(sed -n 's/^HOME=//p' /run/rudolf/acct-session.env 2>/dev/null); [ -n \"$H\" ] || H=/home/rudolf; echo $H"
# the newest Oracle window's dialog: "Open Test Stand?"
DLG = ("JSON.stringify((()=>{const o=document.querySelector('.sdlg [role=alertdialog],.rmodal [role=alertdialog]');const W=wins.filter(w=>w.body&&w.body.querySelector('.otabs')).pop();"
       "return {dlg:o?{t:o.querySelector('h3').textContent,m:o.querySelector('.sdm').textContent}:null,ad:W?W.body.querySelector('.ad').value:null,st:W?(W.body.querySelector('.st')||{}).textContent:null,user:USER&&USER.n,oneapp:typeof ONEAPP!='undefined'&&!!ONEAPP}})())")

def flow(tag, vm, g, js, sh, launch):
    """the app's sign-in, both ways; launch() starts the app (the desktop's own way on each engine)"""
    code = f'{tag}-{random.randint(100000, 999999)}'
    h = sh(AHOME, 30, False).strip().split('\n')[-1]; out('Debian\'s home (the account\'s folder):', h)
    sh(f"rm -f {h}/teststand.out {h}/teststand.log; echo {code} > {h}/teststand.code; chown $(stat -c %u:%g {h}) {h}/teststand.code; "
       "pgrep -a rudolf-portal; ls -l /run/rudolf/acct-session.env; rudolf-debian run -- /usr/local/bin/xdg-open --version; rudolf-debian run -- /bin/sh -c 'id; echo BROWSER=$BROWSER; echo XDG_DATA_DIRS=$XDG_DATA_DIRS; echo BUS=$DBUS_SESSION_BUS_ADDRESS; ls -l /usr/local/bin /run/rudolf-os/share/applications'", 90)
    o = sh("rudolf-debian run -- /usr/local/bin/xdg-open --version 2>&1", 60, False)
    ok(f'{tag}1. Debian\'s xdg-open is rudolf-open (bound by rudolf-debian)', 'rudolf-open' in o, o)
    launch()
    end = time.time() + 120; lg = ''
    while time.time() < end:
        lg = sh(f"cat {h}/teststand.log 2>/dev/null", 20, False)
        if 'redirect sent' in lg: break
        time.sleep(4)
    out('the app\'s log:', lg)
    pick(g, js)
    out('the desktop:', js("JSON.stringify({href:location.href,shell:typeof SHELL!='undefined'&&!!SHELL,wins:wins.map(w=>w.el.innerText.split('\\n')[0].slice(0,40)),"
        "rt:typeof RT!='undefined'?[...RT.entries()].map(([k,v])=>[k,v.url&&v.url(),v.shown&&v.shown()]):null,seen:[...LX.seen],cw:(window.__cw||[]).slice(-12)})", 40))
    vm.shot(f'l160{tag}-page')
    o = sh("ls -ld /run/rudolf/portal/req /run/rudolf/portal/resp; ls -l /run/rudolf/portal/req; tail -n 5 /run/rudolf/portal-*.log 2>/dev/null", 30)
    ok(f'{tag}2b. the account\'s portal may leave its questions for the desktop (the folder: sticky, every user\'s)', 'drwx-wx-wt' in o, o)
    ok(f'{tag}2. the app\'s xdg-open (as Electron\'s shell.openExternal) succeeds', 'xdg-open exit 0' in lg, lg)
    ok(f'{tag}3. ...and the sign-in page opens in Oracle, which follows its redirect to the app\'s scheme', 'page asked' in lg and 'redirect sent' in lg, lg)
    d = None; end = time.time() + 90
    while time.time() < end:
        pick(g, js, False); d = js(DLG, 40)
        try: d = json.loads(d)
        except Exception: d = {'raw': d}
        if d.get('dlg'): break
        time.sleep(4)
    vm.shot(f'l160{tag}-ask'); out('Oracle:', d)
    ok(f'{tag}4. Oracle asks first: "Open Test Stand?", saying the site that wants it', bool(d.get('dlg')) and d['dlg']['t'] == 'Open Test Stand?' and '127.0.0.1 wants to open Test Stand' in d['dlg']['m'], d)
    js("(()=>{const b=document.querySelector('.sdlg .lko,.rmodal .lko');if(b)b.click();return !!b})()", 30)
    end = time.time() + 90; got = ''
    while time.time() < end:
        got = sh(f"cat {h}/teststand.out 2>/dev/null", 20, False)
        if 'code=' in got: break
        time.sleep(4)
    vm.shot(f'l160{tag}-after')
    ok(f'{tag}5. after Open the app is started with the link and receives the code', f'teststand://done?code={code}&state=t160' in got, [got, code])
    out(sh("tail -n 5 /run/rudolf/debian/teststand.log 2>/dev/null; grep -iE 'link|teststand' /run/rudolf/rudolfd.log | tail -5; tail -5 /run/rudolf/portal-*.log 2>/dev/null; tail -15 /run/rudolf/page.log 2>/dev/null", 60, False))

make_drive(DRV)
vm = None
try:
    if 'standard' in ENGINES:
        # ------------------------------------------------------------------ Standard (Chromium: Oracle's real tabs)
        vm, g, up = boot('s'); js = jsfn(g); sh = lambda c, t=60, show=True: run(g, c, t, show)
        ok('S0. the release starts (Standard)', up)
        if not up: raise SystemExit
        wait_for(js, "typeof acctNew=='function'&&typeof userLogin=='function'", 600)
        o = sh(APPLY, 240); sh(DEBIAN, 120)
        js("(()=>{setTimeout(()=>location.reload(),300);return 1})()", 30); time.sleep(20)
        r = wait_for(js, "typeof acctNew=='function'&&typeof userLogin=='function'&&typeof lkOpen=='function'", 600)
        ok('S0b. this tree\'s page, rudolfd and Debian programs in place', (not FIX) or (r is True and 'rudolfd ' in o), [r, o[-600:]])
        # the desktop's one-time move into the shell (Oracle's real tabs) first, before anyone signs in: by itself right
        # after start-up, else as the click on "Real browser tabs are ready" does (shell/switch)
        end = time.time() + 900; asked = 0
        while time.time() < end and js("typeof SHELL!='undefined'&&!!SHELL", 20) is not True:
            st = js("(async()=>{try{return JSON.stringify(await rd('shell'))}catch(e){return 'ERR '+e.message}})()", 30)
            if asked < 3 and isinstance(st, str) and '"installed":true' in st and '"active":true' not in st:
                out('the shell, not moved into by itself:', st, '- as the notice\'s click does')
                js("(()=>{try{fsFlush()}catch(e){}setTimeout(()=>rdPost('shell/switch').catch(()=>{}),600);return 1})()", 30); asked += 1; time.sleep(40)
            time.sleep(10)
        pick(g, js)
        wait_for(js, "typeof acctNew=='function'&&typeof userLogin=='function'&&typeof lkOpen=='function'", 300)
        js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};"
           "if(!window.__cw){window.__cw=[];const w=console.warn,er=console.error;console.warn=function(...a){__cw.push('W '+a.map(String).join(' ').slice(0,300));return w.apply(this,a)};"
           "console.error=function(...a){__cw.push('E '+a.map(String).join(' ').slice(0,300));return er.apply(this,a)};addEventListener('error',e=>__cw.push('X '+e.message))}return 1})()")
        r = js("(async()=>{ACCTS.length=0;const a=await acctNew('john','j1','admin');ACCTS.push(a);saveUser();return a.id})()", 120); out('account', r)
        SI = ("(async()=>{try{const a=ACCTS.find(x=>x.n=='john');if(USER)userLogout();USER=a;await userLogin(a,'j1');lastInput=Date.now();"
              "const L=document.getElementById('login');if(L)L.style.display='none';return 'in '+USER.n}catch(e){return 'ERR '+e.message}})()")
        out('sign-in:', js(SI, 300))
        if wait_for(js, "!!(USER&&USER.n=='john'&&USER.disk&&hdOn())", 300) is not True:
            pick(g, js); time.sleep(30); out('sign-in again:', js(SI, 300)); wait_for(js, "!!(USER&&USER.n=='john'&&USER.disk&&hdOn())", 300)
        ok('S0c. the desktop in the shell (Oracle\'s real tabs), the account signed in with its own folder', js("!!(SHELL&&USER&&USER.n=='john'&&hdOn())", 30) is True,
           js("JSON.stringify({shell:typeof SHELL!='undefined'&&!!SHELL,user:USER&&USER.n,hd:typeof hdOn=='function'&&hdOn()})", 30))
        out('engine/shell:', js("JSON.stringify({engine:RD&&RD.engine,shell:typeof SHELL!='undefined'&&!!SHELL,user:USER&&USER.n,hd:typeof hdOn=='function'&&hdOn(),lk:typeof lkOpen})", 30))
        js("(()=>{window.UNL=()=>{const L=document.getElementById('login');if(L&&USER)L.style.display='none'};UNL();return 1})()", 30)
        r = js("(async()=>{DEB.st=null;DEB.appsAt=0;await debApps();lxRegister();return JSON.stringify(lxLaunchers().filter(x=>x.deb).map(x=>[x.deb,x.mime]))})()", 120)
        out('Debian\'s apps in the desktop:', r)
        def launch_s():
            out('launch:', js("(()=>{const L=document.getElementById('login');if(L&&USER)L.style.display='none';const x=lxLaunchers().find(x=>x.deb=='teststand');if(!x)return 'no launcher';lxRun(x);return 'started'})()", 30))
        flow('S', vm, g, js, sh, launch_s)
        js("(async()=>{try{fsFlush()}catch(e){}try{await __rudolfLsFlush()}catch(e){}return 1})()", 60)
        sh("echo native > /persist/etc/rudolf-desktop-engine; rm -f /persist/etc/rudolf-desktop-engine.failed; sync", 60)
        time.sleep(5); vm.stop(); vm = None; time.sleep(3)

    if 'native' in ENGINES:
        # ------------------------------------------------------------------ Native (its Oracle window: Chromium)
        if 'standard' not in ENGINES: raise SystemExit('Native needs the Standard part first (the account, Debian\'s stand-in)')
        vm, g, up = boot('n'); js = jsfn(g); sh = lambda c, t=60, show=True: run(g, c, t, show)
        ok('N0. the release starts again (Native)', up)
        if not up: raise SystemExit
        o = sh("for i in $(seq 1 60); do pgrep -f /usr/bin/rudolf-native >/dev/null && break; sleep 3; done; cat /run/rudolf/desktop-engine; pgrep -f /usr/bin/rudolf-native | head -3", 240)
        ok('N0b. the Native desktop runs', 'native' in o, o)
        o = sh(APPLY + "; X=/run/l160x; cp /usr/bin/rudolf-native $X/rn.real 2>/dev/null; chmod 755 $X/rn.real; "
               "printf '#!/bin/sh\\nexport RUDOLF_NATIVE_STATE=/tmp/nstate\\nexec /run/l160x/rn.real \"$@\"\\n' > $X/rn; chmod 755 $X/rn; mount --bind $X/rn /usr/bin/rudolf-native; "
               "pkill -f 'rudolf-native --rudolf-kiosk'; for i in $(seq 1 60); do [ -s /tmp/nstate ] && break; sleep 2; done; head -3 /tmp/nstate", 300)
        st = lambda: sh("cat /tmp/nstate", 30, False)
        sv = lambda s, k: (re.search(rf'^{k} (.*)$', s, re.M) or [None, None])[1]
        s = st()
        if sv(s, 'signin') == '1':
            time.sleep(3); g.q.type('j1'); time.sleep(0.5); g.q.key('ret')
            for _ in range(30):
                time.sleep(3); s = st()
                if sv(s, 'user') == 'john': break
        if sv(s, 'user') != 'john':
            out('typing at the sign-in screen did not sign in:', s[:600])
            sh("T=$(busybox wget -q -O- http://127.0.0.1:8080/ | grep -o 'token:\"[0-9a-f]*' | cut -d'\"' -f2); busybox wget -q -O- --header \"X-Rudolf-Token: $T\" --post-data '{\"n\":\"john\",\"p\":\"j1\"}' http://127.0.0.1:8080/api/desktop/login; echo; pkill -f /run/l160x/rn.real; sleep 10", 60)
            for _ in range(20):
                time.sleep(3); s = st()
                if sv(s, 'user') == 'john': break
        ok('N0c. signed in on the native sign-in screen', sv(s, 'user') == 'john', s[:600])
        sh("for i in $(seq 1 40); do [ -f /run/rudolf/acct-session.env ] && pgrep rudolf-portal >/dev/null && break; sleep 3; done; cat /run/rudolf/acct-session.env | grep -v PASS; pgrep -a rudolf-portal", 150)
        # (Oracle's windows on Native are Rudolf's Chromium, started at the first one with the session's flags: with the
        # DevTools port too here, as Standard's has with rudolf.debug=1, so the test can read and press Oracle's dialog)
        sh("grep -qx -- '--remote-debugging-port=9222' /run/rudolf/oracle-chrome || echo '--remote-debugging-port=9222' >> /run/rudolf/oracle-chrome; tail -3 /run/rudolf/oracle-chrome", 30)
        time.sleep(20)
        def launch_n():
            # (the desktop's API, as the Start menu's launcher calls it: debian/launch)
            out('launch:', sh("T=$(busybox wget -q -O- http://127.0.0.1:8080/ | grep -o 'token:\"[0-9a-f]*' | cut -d'\"' -f2); busybox wget -q -O- --header \"X-Rudolf-Token: $T\" --post-data '' 'http://127.0.0.1:8080/api/debian/launch?id=teststand'; echo", 60, False))
        flow('N', vm, g, js, sh, launch_n)
        out(sh("tail -n 30 /run/rudolf/native-desktop.log 2>/dev/null", 60, False))
finally:
    try: vm and vm.stop()
    except Exception: pass
    shutil.rmtree(W, ignore_errors=True)
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
