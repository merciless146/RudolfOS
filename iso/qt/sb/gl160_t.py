# Rudolf OS 1.6.0 in QEMU: OpenGL for Linux apps run as a signed-in account's own Linux user (the owner's Vivobook,
# 2026-11-16: on Standard, Unvanquished opened a white window saying "Couldn't load a renderer." and SuperTuxKart did not
# start; both worked in 1.5.x). A release's own kernel and system in slot a (as t160), UEFI with Secure Boot off, QEMU's
# user network (Flathub). One account, signed in; then an OpenGL context made and a frame drawn and read back
# (glxq/glxdraw.c: GLX through X11; glxq/sdlgl.c: through SDL2, as Unvanquished's Daemon engine and SuperTuxKart do):
#   as root and the desktop's user (the display's own; for comparison), as the account's user on Rudolf OS itself, in a
#   Flatpak (Flathub's org.freedesktop.Platform with its GL extension, started as rudolfd starts Flatpak apps: su to the
#   account's user from rudolfd's own environment with the account's session), in Debian (a stand-in made of this
#   machine's own libraries and Mesa, run by rudolf-debian as the account's user), and (GL160_STK=1, the default) a
#   real game, SuperTuxKart from Flathub, started through the desktop's App Store launcher as a person would.
# QEMU's graphics (bochs, or no GPU: software rendering by Mesa) stand in for the Vivobook's AMD GPU: what the
# account's user is refused (the display's authorization, the devices, the environment) is refused the same here.
#   python3 gl160_t.py REL-DIR   (SB_PARTS and QEMU as for t160; root; QEMU_ACCEL=kvm where it can; needs the network)
import os, sys, time, re, subprocess, shutil, struct, json, base64
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); STK = os.environ.get('GL160_STK', '1') == '1'
SOFT = os.environ.get('GL160_SOFT') == '1'
# GL160_GLX: an X server GLX module (libglx.so) built from this tree, put in place of the release's before the test, to
# show a fix to it on a release built before it
GLX = os.environ.get('GL160_GLX', '')   # (the software display, Xvfb, instead of Xorg on QEMU's graphics)
PARTS = os.environ.get('SB_PARTS', '/nonexistent'); LOG = open('gl160.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:1500] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
out('release', mv('serial'), mv('version'))
DMB = 9000; DRV = os.path.abspath('gl160.img'); MEM = int(os.environ.get('MEM', '4096')); W = os.path.abspath('w160g')
shutil.rmtree(W, ignore_errors=True); os.makedirs(W)
RT = 'org.freedesktop.Platform//24.08'

# ---- the test programs and a Debian stand-in: this machine's own programs and libraries (Mesa's software drivers
# included), each at its own path under D, with its own loader. On Rudolf OS itself they run with that loader too.
G = os.path.join(HERE, 'glxq'); LIB = '/usr/lib/x86_64-linux-gnu'
T = os.path.abspath('gl160.tree'); shutil.rmtree(T, ignore_errors=True); D = f'{T}/debian'
os.makedirs(f'{D}/usr/bin'); os.makedirs(f'{D}{LIB}/dri'); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
cc = lambda o, *a: subprocess.run(['gcc', '-O2', '-o', f'{D}/usr/bin/{o}', *a], check=True)
cc('glxq', f'{G}/glxq.c', '-lX11'); cc('glxdraw', f'{G}/glxdraw.c', '-lGL', '-lX11'); cc('sdlgl', f'{G}/sdlgl.c', '-ldl')
GLIBC = ('libc.so.6', 'libm.so.6', 'ld-linux-x86-64.so.2', 'libpthread.so.0', 'libdl.so.2', 'librt.so.1', 'libresolv.so.2')
def deps(p):
    o = subprocess.run(['ldd', p], capture_output=True, text=True).stdout
    return [m for m in re.findall(r'(/\S+)\s+\(0x', o)]
for n in ('glxinfo', 'xdpyinfo'):
    if shutil.which(n): shutil.copy(shutil.which(n), f'{D}/usr/bin/{n}')
want = [f'{D}/usr/bin/glxinfo', f'{D}/usr/bin/xdpyinfo', f'{D}/usr/bin/glxq', f'{D}/usr/bin/glxdraw', f'{D}/usr/bin/sdlgl', f'{LIB}/libGLX_mesa.so.0', f'{LIB}/libSDL2-2.0.so.0', '/bin/busybox']
for n in ('swrast_dri.so', 'kms_swrast_dri.so', 'virtio_gpu_dri.so', 'libdril_dri.so'):
    if os.path.exists(f'{LIB}/dri/{n}'): shutil.copy(os.path.realpath(f'{LIB}/dri/{n}'), f'{D}{LIB}/dri/{n}'); want.append(f'{D}{LIB}/dri/{n}')
seen = set()
while want:
    p = want.pop()
    for q in deps(p):
        b = os.path.basename(q)
        if b in seen: continue
        seen.add(b); shutil.copy(os.path.realpath(q), f'{D}{LIB}/{b}'); want.append(q)
for p in (f'{LIB}/libGLX_mesa.so.0', f'{LIB}/libSDL2-2.0.so.0'):
    b = os.path.basename(p)
    if b not in seen: shutil.copy(os.path.realpath(p), f'{D}{LIB}/{b}')
os.makedirs(f'{D}/lib64'); shutil.copy(f'{D}{LIB}/ld-linux-x86-64.so.2', f'{D}/lib64/ld-linux-x86-64.so.2')
os.symlink('usr/lib', f'{D}/lib'); os.makedirs(f'{D}/bin'); shutil.copy('/bin/busybox', f'{D}/bin/busybox')
for c in ('sh', 'cat', 'echo', 'ls', 'id', 'env', 'head', 'grep'): os.symlink('busybox', f'{D}/bin/{c}')
for d in ('etc', 'proc', 'sys', 'dev', 'tmp', 'run', 'home/rudolf', 'root', '.oldroot'): os.makedirs(f'{D}/{d}', exist_ok=True)
open(f'{D}/etc/passwd', 'w').write('root:x:0:0::/root:/bin/sh\nrudolf:x:1000:1000::/home/rudolf:/bin/sh\n')
open(f'{D}/etc/rudolf-debian', 'w').write('test\n'); open(f'{D}/.rudolf-ready', 'w').close()
subprocess.run(['chmod', '-R', 'a+rX', D], check=True); subprocess.run(['chown', '-R', '200000:200000', D], check=True)
subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
if GLX: os.makedirs(f'{T}/glx'); shutil.copy(GLX, f'{T}/glx/libglx.so')
out('the stand-in:', subprocess.run(['du', '-sh', D], capture_output=True, text=True).stdout.strip())

def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    d = f'{T}/os/system/a'; os.makedirs(d)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs', 'manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{d}/{n}')
    open(f'{d}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n"); open(f'{d}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    Dt = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', Dt], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, Dt], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(Dt, 'rb') as dd: shutil.copyfileobj(dd, f)
    os.remove(Dt)

INIT_JS = "(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};return 1})()"
def boot(tag):
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf160g',
             '-netdev', 'user,id=u0', '-netdev', 'hubport,id=h1,hubid=0,netdev=u0',
             '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1' + (' rudolf.softdisplay=1' if SOFT else '')]
    vm = sbvm.VM('gl160' + tag, PARTS, [], secure=False, mem=MEM, extra=extra)
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
    if up: settle(g, js)
    return vm, g, up, js
# the desktop's page settled: the same page for 20 s
def settle(g, js):
    end = time.time() + 600
    while time.time() < end and js("typeof acctNew=='function'&&typeof userLogin=='function'", 20) is not True: time.sleep(5)
    run(g, "last=; same=0; i=0; while [ $i -lt 40 ]; do i=$((i+1)); v=$(timeout 20 cdp-eval 'Math.round(performance.timeOrigin)' 2>/dev/null | grep -o '\"value\":[0-9]*' | head -1); "
           "if [ -n \"$v\" ] && [ \"$v\" = \"$last\" ]; then same=$((same+5)); else same=0; last=$v; fi; [ $same -ge 20 ] && break; sleep 5; done; echo \"SETTLED $same\"", 1100)
    js(INIT_JS)
def run(g, c, t=60):
    f = os.path.join(W, 'c160.sh'); open(f, 'w').write(c + '\n'); g.put(f, '/tmp/c160.sh', '644')
    o = g.sh('sh /tmp/c160.sh 2>&1', t); out('$', c[:300].replace('\n', ' ; ')); out(o.rstrip()[-4000:]); return o
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

# what an app is started with: rudolfd's own environment (su keeps it), then the session's file, as rudolfd does
# (rudolfd.c api_apps "run" and api_native "run": setsid su -s /bin/sh USER -c 'set -a; . ENV; set +a; exec ...')
ASAPP = """#!/bin/sh
# asapp USER ENVFILE SCRIPT: as rudolfd starts an app: from rudolfd's own environment, su to the user, the
# session's file read (rudolfd.c api_apps "run", api_native "run")
u=$1; e=$2; shift 2; RD=$(pgrep -o -x rudolfd)
exec env -i $(tr '\\0' '\\n' < /proc/$RD/environ | grep -E '^[A-Za-z_][A-Za-z0-9_]*=[^ ]*$') su -s /bin/sh "$u" -c "set -a; . $e; set +a; exec sh $1" </dev/null 2>&1
"""
make_drive(DRV)
vm = None
try:
    vm, g, up, js = boot('a'); sh = lambda c, t=60: run(g, c, t)
    ok('1. the release starts', up, 'not up')
    if not up: raise SystemExit
    if GLX:
        # this tree's GLX module in place of the release's, and the display started again (as a new resolution does)
        o = sh("mount --bind /persist/glx/libglx.so /usr/lib/xorg/modules/extensions/libglx.so && echo BOUND; md5sum /usr/lib/xorg/modules/extensions/libglx.so; "
               "rm -f /run/rudolf/display-mode; touch /run/rudolf/screen-restart; i=0; while [ $i -lt 90 ] && [ ! -s /run/rudolf/display-mode ]; do i=$((i+1)); sleep 2; done; cat /run/rudolf/display-mode", 240)
        out('NOTE this tree\'s GLX module:', 'BOUND' in o); time.sleep(20); settle(g, js)
    # the X server's GLX: a GL provider for the screen (Xorg on the graphics card; the software display's Xvfb has its own)
    o = sh("cat /run/rudolf/display-mode; grep -E 'GLX|AIGLX|swrast' /run/rudolf/Xorg.log 2>/dev/null", 30)
    if o.strip().split('\n')[0].strip() == 'gpu':
        ok('1b. Xorg\'s GLX loads Mesa\'s driver: a GL provider for the screen', re.search(r'GLX: Initialized \w+ GL provider for screen', o) and 'AIGLX error' not in o, o)
    r = js("(async()=>{ACCTS.length=0;const b=await acctNew('boss','b1','admin');ACCTS.push(b);saveUser();return b.id})()", 120)
    out('account:', r)
    out('sign-in', signin(js, 'boss', 'b1')); si = wait_for(js, "!!(USER&&USER.disk&&hdOn())", 400)
    o = sh("cat /run/rudolf/acct-session.env; cat /persist/etc/rudolf/account-users", 30)
    m = re.search(r'^USER=(acct\d+)$', o, re.M); AU = m.group(1) if m else ''
    ok('2. the account is signed in, with its own Linux user and session', si is True and AU, o)
    if not AU: raise SystemExit
    # what the account's programs have: the devices, their groups, the display, the environment
    sh(f"cat /run/rudolf/display-mode /run/rudolf/display-reason; ls -ln /dev/dri /dev/dri/by-path 2>&1; id {AU}; id rudolf; grep -E '^(video|render|audio|input):' /etc/group; "
       f"ls -ld /run/user /run/user/* /tmp/.X11-unix /tmp/.X11-unix/*; ls -ln /run/rudolf/xauth; ls -ld /persist /persist/debian /var/lib/flatpak; "
       "RD=$(pgrep -o -x rudolfd); echo rudolfd=$RD; tr '\\0' '\\n' < /proc/$RD/environ; echo; cat /proc/$RD/status | grep -E '^(Uid|Gid|Groups)'; "
       "grep -iE 'glx|dri|glamor|swrast|provider|visual|fbconfig' /run/rudolf/Xorg.log 2>/dev/null; ls -la /usr/lib/xorg/modules/extensions; ls /usr/lib/x86_64-linux-gnu/dri | tr '\n' ' '; echo; ls /usr/lib/x86_64-linux-gnu | grep -iE 'GL|gallium|drm' | tr '\n' ' '; echo; pgrep -a Xvfb; pgrep -a Xorg; tail -5 /run/rudolf/udev.log 2>/dev/null; pgrep -a systemd-udevd | head -2; "
       "udevadm info -q all -n /dev/dri/card0 2>&1 | head -20; udevadm info -q all -n /dev/dri/renderD128 2>&1 | head -12", 120)
    B = '/persist/debian'
    open(f'{W}/asapp', 'w').write(ASAPP); g.put(f'{W}/asapp', '/tmp/asapp', '755')
    # a script run as root, or as a user as rudolfd starts an app
    def AS(who, env, script, t=240):
        c = f"cat > /tmp/gs.sh <<'GSEOF'\n{script}\nGSEOF\nchmod 755 /tmp/gs.sh; " + ("sh /tmp/gs.sh 2>&1" if who == 'root' else f"/tmp/asapp {who} {env} /tmp/gs.sh")
        return sh(c, t)
    # this machine's programs with their own loader, libraries and Mesa
    HOSTGL = (f"G=\"LIBGL_DRIVERS_PATH={B}{LIB}/dri LIBGL_DEBUG=verbose\"; L=\"{B}/lib64/ld-linux-x86-64.so.2 --library-path {B}{LIB}\"\n"
              f"id; echo DISPLAY=$DISPLAY XAUTHORITY=$XAUTHORITY XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR\n"
              f"env $G $L {B}/usr/bin/glxinfo -B 2>&1 | head -25\nenv $G $L {B}/usr/bin/glxq\n"
              f"env $G $L {B}/usr/bin/glxdraw; echo rc=$?\nenv $G $L {B}/usr/bin/sdlgl; echo rc=$?")
    # A. root (with the display's cookie) and the desktop's user: the display's own
    o = AS('root', '', 'export DISPLAY=:0 XAUTHORITY=/run/rudolf/xauth\n' + HOSTGL)
    ok('3. root draws with OpenGL (the display\'s cookie): the machine can', o.count('drew ok') == 2, o)
    o = AS('rudolf', '/run/rudolf/session.env', HOSTGL)
    out('NOTE the desktop\'s user (rudolf), as apps before 1.5.9:', o.count('drew ok'))
    # C. the account's user on Rudolf OS itself
    o = AS(AU, '/run/rudolf/acct-session.env', 'env | sort\n' + HOSTGL)
    ok('4. the account\'s user draws with OpenGL on Rudolf OS (GLX, and through SDL2 as the games)', o.count('drew ok') == 2, o)
    # D. Debian (the stand-in), as the account's user
    o = sh(f"rudolf-debian rebind >/dev/null 2>&1; rudolf-debian run -- sh -c 'id; env | sort; export LD_LIBRARY_PATH={LIB} LIBGL_DEBUG=verbose; /usr/bin/glxinfo -B 2>&1 | head -25; /usr/bin/glxdraw; echo rc=$?; /usr/bin/sdlgl; echo rc=$?' 2>&1", 240)
    ok('5. a Debian program run as the account\'s user draws with OpenGL (GLX and SDL2)', o.count('drew ok') == 2, o)
    # E. Flatpak: Flathub's runtime (and the GL extension it brings), started as rudolfd starts Flatpak apps
    o = sh("(flatpak remotes --system --columns=name | grep -qx flathub || flatpak remote-add --system flathub https://dl.flathub.org/repo/flathub.flatpakrepo; "
           f"flatpak install --system -y --noninteractive flathub {RT} > /tmp/fpi.log 2>&1; echo \"exit $?\" >> /tmp/fpi.log) </dev/null >/dev/null 2>&1 & echo started", 120)
    end = time.time() + 2400; o = ''
    while time.time() < end:
        o = g.sh("tail -c 300 /tmp/fpi.log", 30)
        if re.search(r'exit \d+', o): break
        time.sleep(30)
    o = sh("tail -15 /tmp/fpi.log; flatpak list --system --runtime --columns=application,branch,size", 120)
    ok('6. Flathub\'s runtime installed, with its GL extension (org.freedesktop.Platform.GL.default)', 'exit 0' in o and 'Platform.GL.default' in o, o)
    for who, env, home in ((AU, '/run/rudolf/acct-session.env', f'/home/{AU}'), ('rudolf', '/run/rudolf/session.env', '/home/rudolf'), ('root', '', '/root')):
        sh(f"rm -rf {home}/gl160; mkdir -p {home}/gl160; cp {B}/usr/bin/glxdraw {B}/usr/bin/sdlgl {B}/usr/bin/glxq {B}/usr/bin/glxinfo {home}/gl160/ 2>/dev/null; chown -R $(id -u {who}):$(id -g {who}) {home}/gl160", 30)
        FR = f"flatpak run --socket=x11 --device=dri --share=ipc --filesystem={home}/gl160:ro --env=LIBGL_DEBUG=verbose"
        o = AS(who, env, ("export DISPLAY=:0 XAUTHORITY=/run/rudolf/xauth\n" if who == 'root' else '') +
               f"{FR} --command=sh {RT} -c 'id; ls -l /dev/dri; echo DISPLAY=$DISPLAY XAUTHORITY=$XAUTHORITY; ls /usr/lib/x86_64-linux-gnu/GL /usr/lib/x86_64-linux-gnu/GL/default/lib/dri 2>&1 | head -12'\n"
               f"{FR} --command={home}/gl160/glxinfo {RT} -B 2>&1 | head -25\n{FR} --command={home}/gl160/glxq {RT}\n"
               f"{FR} --command={home}/gl160/glxdraw {RT}; echo rc=$?\n{FR} --command={home}/gl160/sdlgl {RT}; echo rc=$?")
        if who == AU: ok('7. a Flatpak app run as the account\'s user (as rudolfd starts it) draws with OpenGL (GLX, and SDL2 as the games)', o.count('drew ok') == 2, o)
        else: out(f'NOTE the same as {who}:', o.count('drew ok'))
    # F. a real game from Flathub, started from the desktop's launcher (the App Store's), as the owner did
    if STK:
        APP = 'net.supertuxkart.SuperTuxKart'
        sh(f"(flatpak install --system -y --noninteractive flathub {APP} > /tmp/fpa.log 2>&1; echo \"exit $?\" >> /tmp/fpa.log) </dev/null >/dev/null 2>&1 & echo started", 60)
        end = time.time() + 2400
        while time.time() < end:
            o = g.sh("tail -c 300 /tmp/fpa.log", 30)
            if re.search(r'exit \d+', o): break
            time.sleep(30)
        o = sh("tail -5 /tmp/fpa.log", 30)
        if 'exit 0' in o:
            js("(async()=>{try{await lxInstalled(true)}catch(e){};return 1})()", 120)
            r = js(f"lxApi('apps/run?id={APP}&desktop={APP}').then(()=>'started',e=>'ERR '+e.message)", 60)
            out('run:', r); time.sleep(120)
            o = sh(f"U=$(id -u {AU}); for p in /proc/[0-9]*; do [ \"$(stat -c %u $p 2>/dev/null)\" = $U ] && tr '\\0' ' ' < $p/cmdline 2>/dev/null | cut -c1-200 && echo; done | grep -iE 'supertux'; tail -40 /run/rudolf/apps/{APP}.log", 60)
            vm.shot('stk')
            ok('8. SuperTuxKart from Flathub, started from the desktop, runs as the account\'s user (no OpenGL error)', 'supertuxkart' in o.lower() and not re.search(r"(?i)(couldn'?t|could not|failed to) (create|load|initiali[sz]e).*(opengl|gl |glx|context|renderer|irrlicht|video)|GLXBadContext|BadMatch|segmentation fault|No such file", o.split('$ ', 1)[-1]), o)
        else: ok('8. SuperTuxKart from Flathub installed', False, o)
    vm.shot('end')
finally:
    try: vm and vm.stop()
    except Exception: pass
    shutil.rmtree(W, ignore_errors=True)
    for p in (DRV, T):
        try: shutil.rmtree(p) if os.path.isdir(p) else os.remove(p)
        except OSError: pass
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
