# Rudolf OS 1.4.3 in QEMU (UEFI, Secure Boot off): the X server offers GLX, so apps and games that draw with OpenGL
# through X11 open their windows. On the Vivobook, Oolite (through SDL) stopped at once: "Could not create display
# surface: GLX is not supported" (Xorg was built with -Dglx=false from 1.1 to 1.4.2).
#   python3 t143_t.py DISC.iso      DISC.iso: a test disc (mkdisc.sh with RUDOLF_UKI_TEST_CMDLINE=rudolf.debug=1)
# The drive: the disc image, then RUDOLFDATA with two test programs (glxq/: glxq asks the X server for GLX with
# libX11 only; glxdraw opens a window, makes a GLX context current, draws a frame and reads it back) and the OpenGL
# client libraries they need, which Rudolf OS does not ship (apps bring their own: Flatpak runtimes). Mesa's drivers
# are the system's own.
import os, re, shutil, struct, subprocess, sys, time
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
DISC = os.path.abspath(sys.argv[1]); PARTS = os.environ['SB_PARTS']; LOG = open('t143.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:300] if d != '' and not c else ''))
def last(t): return t.strip().split('\n')[-1].strip() if t.strip() else ''

ISZ = os.path.getsize(DISC); START = (ISZ + 1048575) // 1048576 * 2048; DMB = 600
DRV = os.path.abspath('int143.img'); L = '/usr/lib/x86_64-linux-gnu'
GLH = os.environ.get('GL_HEADERS', os.path.join(QT, '..', 'src', 'xorg', 'xsrc', 'external', 'mit', 'MesaLib', 'dist', 'include'))
T = os.path.abspath('tree143'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/t143/lib')
G = os.path.join(HERE, 'glxq')
subprocess.run(['gcc', '-O2', '-o', f'{T}/t143/glxq', f'{G}/glxq.c', '-lX11'], check=True)
subprocess.run(['gcc', '-O2', f'-I{GLH}', '-o', f'{T}/t143/glxdraw', f'{G}/glxdraw.c', f'{L}/libGL.so.1', '-lX11'], check=True)
for n in ('libGL.so.1', 'libGLX.so.0', 'libGLX_mesa.so.0', 'libGLdispatch.so.0', 'libxcb-glx.so.0', 'libX11-xcb.so.1', 'libXxf86vm.so.1'):
    shutil.copy(os.path.realpath(f'{L}/{n}'), f'{T}/t143/lib/{n}')
with open(DRV, 'wb') as f:
    with open(DISC, 'rb') as o: shutil.copyfileobj(o, f)
    f.truncate((START + DMB * 2048 + 2048) * 512)
subprocess.run(['truncate', '-s', f'{DMB}M', 'data143'], check=True)
subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, 'data143'], check=True); shutil.rmtree(T)
with open(DRV, 'r+b') as f:
    f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', START, DMB * 2048))
    f.seek(START * 512)
    with open('data143', 'rb') as d: shutil.copyfileobj(d, f)
os.remove('data143')

vm = sbvm.VM('t143', PARTS, [], secure=False, nvram=True, mem=3072, extra=['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf143,bootindex=0'])
g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3); sh = lambda c, t=60: g.sh(c, t)
try:
    time.sleep(45); up = False; end = time.time() + 900
    while time.time() < end:
        try:
            if 'ready' in sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    ok('1. Rudolf OS starts (test disc)', up)
    # the desktop's display: Xorg on the graphics card (QEMU's: bochs), or the software display (Xvfb)
    mode = ''
    for i in range(40):
        mode = last(sh('cat /run/rudolf/display-mode 2>/dev/null; echo', 10)) if up else ''
        if mode: break
        time.sleep(3)
    out('display:', mode, '|', last(sh('cat /run/rudolf/display-reason 2>/dev/null; echo', 10)))
    if mode == 'gpu':
        xl = sh("grep -iE 'glx|dri|LoadModule|Loading|Failed|undefined' /run/rudolf/Xorg.log | grep -v 'kbd\\|mouse\\|libinput'", 20)
        out('Xorg.log:', xl.strip().replace('\n', ' | ')[:3000])
        ok('2. Xorg (on the graphics card) offers GLX', 'Initializing extension GLX' in xl, xl[-300:])
        ok('...with an OpenGL provider for the screen (Mesa\'s driver)', re.search(r'GLX: Initialized \w+ GL provider', xl) is not None, xl[-300:])
    else:
        out(f'NOTE the desktop runs on the software display ({mode}), not Xorg: the Xorg checks do not apply here')
    q = last(sh('DISPLAY=:0 /persist/t143/glxq', 30))
    ok('3. an app asking the X server for GLX gets it', q.startswith('GLX 1.'), q)
    d = sh('DISPLAY=:0 LD_LIBRARY_PATH=/persist/t143/lib /persist/t143/glxdraw 2>&1', 60)
    out('glxdraw:', d.strip().replace('\n', ' | ')[-400:])
    ok('4. an OpenGL app (as Oolite through SDL) opens a window, makes a GLX context current and draws a frame', 'drew ok' in d, d[-300:])
finally:
    vm.stop()
try: os.remove(DRV)
except OSError: pass
out(f'{sum(R)}/{len(R)} passed')
