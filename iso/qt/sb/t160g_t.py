# Rudolf OS 1.6.0 in QEMU: OpenGL for an account's Linux apps (the owner's report of 2026-11-16 on the Vivobook, beta
# 2026111611: on Standard, Unvanquished, a Flatpak app, showed only "Couldn't load a renderer." and SuperTuxKart did not
# start, while 2D apps opened; Unvanquished had worked in 1.5.x). Since 1.5.9 (plan 15.3) an account's Linux apps run as
# its own Linux user (acctUID) and the display needs authorization.
# On a release's own kernel and system (slot a, UEFI, Secure Boot off, as t160), with an account signed in:
#   - glxdraw (glxq/glxdraw.c: a window, a GLX context made current, a frame drawn and read back) as root, the desktop's
#     user and the account's user, with the system's own Mesa (outside any sandbox);
#   - a Flatpak app (org.rudolfos.GLTest: glxdraw in Flathub's org.freedesktop.Platform 24.08 with its GL extension, Mesa
#     of the runtime, the permissions games ask for: x11, dri, ipc) started as the desktop starts one (apps/run: as the
#     account's user, through its launcher), and the same through flatpak run as the account's and the desktop's user;
#   - a Debian program (glxinfo and glxdraw in a Debian 13 made with debootstrap, Debian's own Mesa) run as the account
#     (rudolf-debian run: Debian's "rudolf" is the account's user);
#   - the display still refused to another account's user (not signed in).
# The Flatpak installation and Debian are made here, on the test machine (network: Flathub, Debian's mirror), into the
# RUDOLFDATA partition (/persist/flatpak, /persist/debian); the guest has no network.
#   python3 t160g_t.py REL-DIR      (SB_PARTS and QEMU as for t150/t160; root; QEMU_ACCEL=kvm where it can)
#   T160G_STK=1: also SuperTuxKart from Flathub (about 1 GB), started through apps/run
import os, sys, time, re, subprocess, shutil, struct, json, base64
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE); ISO = os.path.dirname(QT)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1])
PARTS = os.environ.get('SB_PARTS', '/nonexistent'); LOG = open('t160g.txt', 'w'); R = []
STK = os.environ.get('T160G_STK') == '1'
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:1500] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = int(os.environ.get('T160G_MB', '7000' if STK else '4500')); DRV = os.path.abspath('int160g.img'); MEM = int(os.environ.get('MEM', '4096'))
W = os.path.abspath('w160g'); APP = 'org.rudolfos.GLTest'; RT = '24.08'
def sh_host(c, **kw): out('host$', c[:300]); return subprocess.run(c, shell=True, check=True, **kw)

# ---- the test machine: glxdraw, a Flatpak installation, a Debian
shutil.rmtree(W, ignore_errors=True); os.makedirs(W)
T = f'{W}/tree'; os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc'); os.makedirs(f'{T}/t160g')
G = os.path.join(HERE, 'glxq')
subprocess.run(['gcc', '-O2', '-o', f'{T}/t160g/glxdraw', f'{G}/glxdraw.c', '-lGL', '-lX11'], check=True)
# the app: glxdraw and a script saying what the sandbox has, on the Platform runtime; exported as Flatpak does it
A = f'{W}/app'; os.makedirs(f'{A}/files/bin'); os.makedirs(f'{A}/files/share/applications'); os.makedirs(f'{A}/export/share/applications')
shutil.copy(f'{T}/t160g/glxdraw', f'{A}/files/bin/glxdraw')
open(f'{A}/files/bin/gltest', 'w').write('#!/bin/sh\n'
    'echo "GLTEST uid=$(id -u) groups=$(id -G) DISPLAY=$DISPLAY XAUTHORITY=$XAUTHORITY XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR WAYLAND_DISPLAY=$WAYLAND_DISPLAY"\n'
    'ls -l /dev/dri/ 2>&1 | tr "\\n" " "; echo; ls -la /tmp/.X11-unix/ 2>&1 | tr "\\n" " "; echo\n'
    'ls /usr/lib/x86_64-linux-gnu/GL/ 2>&1 | tr "\\n" " "; echo; ls /usr/lib/x86_64-linux-gnu/GL/default/lib 2>&1 | grep -c . \n'
    'LIBGL_DEBUG=verbose /app/bin/glxdraw 2>&1; echo "GLTEST rc=$?"\n')
os.chmod(f'{A}/files/bin/gltest', 0o755)
desk = f'[Desktop Entry]\nType=Application\nName=GL test\nExec=gltest\nIcon=application-x-executable\nCategories=Game;\n'
open(f'{A}/files/share/applications/{APP}.desktop', 'w').write(desk); open(f'{A}/export/share/applications/{APP}.desktop', 'w').write(desk)
open(f'{A}/metadata', 'w').write(f'[Application]\nname={APP}\nruntime=org.freedesktop.Platform/x86_64/{RT}\nsdk=org.freedesktop.Sdk/x86_64/{RT}\ncommand=gltest\n\n'
                                  '[Context]\nshared=ipc;\nsockets=x11;\ndevices=dri;\n')
FP = f'{T}/flatpak'; env = dict(os.environ, FLATPAK_SYSTEM_DIR=FP)
sh_host(f'flatpak build-export --no-update-summary {W}/repo {A} && flatpak build-update-repo {W}/repo', stdout=subprocess.DEVNULL)
sh_host('flatpak remote-add --system --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo', env=env)
sh_host(f'flatpak remote-add --system --no-gpg-verify t160g file://{W}/repo', env=env)
sh_host(f'flatpak install --system -y --noninteractive flathub org.freedesktop.Platform//{RT} org.freedesktop.Platform.GL.default//{RT}', env=env, stdout=subprocess.DEVNULL)
sh_host(f'flatpak install --system -y --noninteractive t160g {APP}', env=env)
if STK: sh_host('flatpak install --system -y --noninteractive flathub net.supertuxkart.SuperTuxKart', env=env, stdout=subprocess.DEVNULL)
sh_host('flatpak list --system --columns=application,branch,runtime', env=env)
# Debian 13 (Debian's root is 200000 on Rudolf OS, its rudolf 1000 the person's user), with Mesa and glxinfo
D = f'{T}/debian'
sh_host(f'debootstrap --variant=minbase --include=mesa-utils,libgl1-mesa-dri,libgl1,libx11-6 trixie {D} http://deb.debian.org/debian', stdout=subprocess.DEVNULL)
sh_host(f'rm -rf {D}/dev/* {D}/var/cache/apt/archives/*.deb; mkdir -p {D}/home/rudolf {D}/.oldroot {D}/opt; cp {T}/t160g/glxdraw {D}/opt/glxdraw; '
        f'grep -q "^rudolf:" {D}/etc/passwd || echo "rudolf:x:1000:1000::/home/rudolf:/bin/bash" >> {D}/etc/passwd; '
        f'grep -q "^rudolf:" {D}/etc/group || echo "rudolf:x:1000:" >> {D}/etc/group; echo test > {D}/etc/rudolf-debian; touch {D}/.rudolf-ready; '
        f'chown -R 200000:200000 {D}; chown -R 1000:1000 {D}/home/rudolf')
sh_host(f'chown -R 1000:1000 {T}/home/rudolf; du -sm {FP} {D}')

def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    d = f'{T}/os/system/a'; os.makedirs(d)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs'): shutil.copy(f'{REL}/{n}', f'{d}/{n}')
    for n in ('manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{d}/{n}')
    open(f'{d}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n"); open(f'{d}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    Dd = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', Dd], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, Dd], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(Dd, 'rb') as dd: shutil.copyfileobj(dd, f)
    os.remove(Dd)

INIT_JS = "(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};return 1})()"
TUN = f'/tmp/tun-t160g-{os.getpid()}.sock'
def boot(tag):
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf160g',
             '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1']
    vm = sbvm.VM('t160g' + tag, PARTS, [], secure=False, mem=MEM, extra=extra)
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    time.sleep(30); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    def js(e, t=90):
        if len(base64.b64encode(e.encode())) > 900: raise ValueError('a page expression too long for the serial shell: ' + e[:80])
        for _ in range(5):
            r = g.js(e, t)
            if not (isinstance(r, tuple) and (r[0] == 'ERR' or (r[0] == 'EXC' and 'ReferenceError' in str(r[1])))): return r
            time.sleep(15)
        return r
    end = time.time() + 600
    while up and time.time() < end and js("typeof acctNew=='function'&&typeof userLogin=='function'", 20) is not True: time.sleep(5)
    js(INIT_JS)
    if up:
        o = run(g, "last=; same=0; i=0; while [ $i -lt 40 ]; do i=$((i+1)); v=$(timeout 20 cdp-eval 'Math.round(performance.timeOrigin)' 2>/dev/null | grep -o '\"value\":[0-9]*' | head -1); "
                   "if [ -n \"$v\" ] && [ \"$v\" = \"$last\" ]; then same=$((same+5)); else same=0; last=$v; fi; [ $same -ge 20 ] && break; sleep 5; done; echo \"SETTLED $same $last after $i\"", 1100)
        js(INIT_JS)
    return vm, g, up, js
def run(g, c, t=60):
    f = os.path.join(W, 'c160g.sh'); open(f, 'w').write(c + '\n'); g.put(f, '/tmp/c160g.sh', '644')
    o = g.sh('sh /tmp/c160g.sh 2>&1', t); out('$', c[:200].replace('\n', ' ; ')); out(o.rstrip()[-4000:]); return o
def signin(js, n, pw, secs=900):
    r = js(f"(()=>{{if(window.__si=='wait')return 'already';const a=ACCTS.find(x=>x.n=={json.dumps(n)});if(!a)return 'no account';window.__si='wait';"
           f"(async()=>{{try{{if(!(await pwCheck(a,{json.dumps(pw)}))){{__si='bad';return}}if(USER)userLogout();const L=document.getElementById('login');USER=a;"
           f"await userLogin(a,{json.dumps(pw)});lastInput=Date.now();if(L)L.style.display='none';__si='in '+USER.n}}catch(e){{__si='error '+e.message}}}})();return 'started'}})()", 60)
    if r not in ('started', 'already'): return r
    end = time.time() + secs
    while time.time() < end:
        s = js("window.__si", 30)
        if isinstance(s, str) and s != 'wait': return s
        time.sleep(5)
    return 'still signing in'
def wait_for(js, expr, secs, every=5):
    end = time.time() + secs; r = None
    while time.time() < end:
        r = js(expr, 60)
        if r is True: return True
        time.sleep(every)
    return r
def drew(o): return 'drew ok' in o

make_drive(DRV)
vm = None
try:
    vm, g, up, js = boot('a'); sh = lambda c, t=60: run(g, c, t)
    ok('1. the release starts (slot a)', up, 'not up')
    if not up: raise SystemExit
    r = js("(async()=>{ACCTS.length=0;const b=await acctNew('boss','b1','admin');ACCTS.push(b);const k=await acctNew('kid','k1','user');ACCTS.push(k);saveUser();return b.id+' '+k.id})()", 120)
    out('accounts:', r); BOSS, KID = str(r).split()
    # (kid signed in once, so it has its own Linux user; then boss, whose session the apps run in)
    out('sign-in kid', signin(js, 'kid', 'k1')); out('kid on the disk:', wait_for(js, "!!(USER&&USER.disk&&hdOn())", 300))
    out('sign-in boss', signin(js, 'boss', 'b1')); out('boss on the disk:', wait_for(js, "!!(USER&&USER.disk&&hdOn())", 300)); time.sleep(5)
    o = sh("cat /persist/etc/rudolf/account-users; cat /run/rudolf/acct-session.env; cat /run/rudolf/display-mode /run/rudolf/display-reason /run/rudolf/devices 2>&1; "
           "ls -l /dev/dri/; id rudolf; mount | grep -E 'flatpak|/persist '; ls /var/lib/flatpak/app; flatpak --version; "
           "grep -iE 'glx|dri3|glamor|AIGLX' /run/rudolf/Xorg.log | head -20")
    ub = re.search(rf'^{BOSS} (\d+)', o, re.M); uk = re.search(rf'^{KID} (\d+)', o, re.M); UB, UK = (ub.group(1) if ub else '?'), (uk.group(1) if uk else '?')
    ok('2. signed in: the account\'s session (its own Linux user)', ub and f'USER=acct{UB}' in o, o)
    out(sh(f"id acct{UB}; id acct{UK}"))
    ENV = '/run/rudolf/acct-session.env'
    def AS(user, cmd, t=180, envf=ENV):
        return sh(f"cat > /tmp/u160g.sh <<'EOF'\n{cmd}\nEOF\nchmod 644 /tmp/u160g.sh; su -s /bin/sh {user} -c 'set -a; . {envf}; set +a; sh /tmp/u160g.sh' 2>&1; echo rc=$?", t)
    # the system's own Mesa, outside any sandbox
    o0 = sh("DISPLAY=:0 XAUTHORITY=/run/rudolf/xauth LIBGL_DEBUG=verbose /persist/t160g/glxdraw 2>&1", 120)
    o1 = AS('rudolf', "LIBGL_DEBUG=verbose /persist/t160g/glxdraw", envf='/run/rudolf/session.env')
    o2 = AS(f'acct{UB}', "LIBGL_DEBUG=verbose /persist/t160g/glxdraw")
    ok('3. OpenGL (GLX) outside a sandbox: root, the desktop\'s user and the signed-in account\'s user draw a frame', drew(o0) and drew(o1) and drew(o2), [o0[-500:], o1[-500:], o2[-500:]])
    # the Flatpak app, as the desktop starts it (apps/run: the account's user, its launcher)
    sh(f"rm -f /run/rudolf/apps/{APP}.log")
    r = js(f"lxApi('apps/run?id={APP}&desktop={APP}').then(()=>'ok',e=>'ERR '+e.message)", 60); out('apps/run:', r)
    o = ''
    for _ in range(40):
        o = sh(f"cat /run/rudolf/apps/{APP}.log 2>&1", 30)
        if 'GLTEST rc=' in o: break
        time.sleep(6)
    ok('4. a Flatpak app started by the desktop (apps/run, the account\'s user) makes a GLX context and draws (Mesa of its runtime)', drew(o) and f'uid={UB}' in o, o)
    o5 = AS(f'acct{UB}', f"flatpak run {APP}", 300)
    o6 = AS('rudolf', f"flatpak run {APP}", 300, envf='/run/rudolf/session.env')
    ok('5. ...the same through flatpak run as the account\'s user', drew(o5), o5)
    out(('NOTE the desktop\'s user (as before 1.5.9): ' + ('draws' if drew(o6) else 'does not draw either')))
    # Debian, as the account (Debian's rudolf is the account's user)
    o = sh("rudolf-debian rebind >/dev/null 2>&1; rudolf-debian run -- sh -c 'id; LIBGL_DEBUG=verbose glxinfo -B 2>&1 | head -20; LIBGL_DEBUG=verbose /opt/glxdraw 2>&1; echo rc=$?' 2>&1", 300)
    ok('6. a Debian program run as the account (glxinfo, glxdraw with Debian\'s Mesa) draws', drew(o) and 'direct rendering:' in o, o)
    # the display authorization is kept: another account's user is refused
    o = AS(f'acct{UK}', "/persist/t160g/glxdraw; rudolf-display --xsize", 120)
    ok('7. the display is still refused to another account\'s user (not signed in)', not drew(o) and 'no display' in o, o)
    if STK:
        sh("rm -f /run/rudolf/apps/net.supertuxkart.SuperTuxKart.log")
        r = js("lxApi('apps/run?id=net.supertuxkart.SuperTuxKart&desktop=net.supertuxkart.SuperTuxKart').then(()=>'ok',e=>'ERR '+e.message)", 60); out('apps/run STK:', r)
        time.sleep(150)
        o = sh(f"tail -c 3000 /run/rudolf/apps/net.supertuxkart.SuperTuxKart.log; echo; pgrep -u acct{UB} -fl supertuxkart | head -3; echo PROCS=$(pgrep -u acct{UB} -f supertuxkart | wc -l)", 60)
        vm.shot('stk')
        ok('8. SuperTuxKart (Flathub) started by the desktop as the account\'s user is running after 150 s, with no OpenGL error', re.search(r'PROCS=[1-9]', o) and not re.search(r'(?i)opengl.*(fail|error|not supported)|no GLX|could not create', o), o)
finally:
    try: vm and vm.stop()
    except Exception: pass
    shutil.rmtree(W, ignore_errors=True)
    try: os.remove(DRV)
    except OSError: pass
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
