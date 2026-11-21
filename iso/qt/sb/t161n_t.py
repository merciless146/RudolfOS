# Rudolf OS 1.6.1 in QEMU (docs/plan-1.6.1.md Part 19): the graphics driver choice on the real release (its kernel, base,
# browser and drivers layers), started as t160x_t.py starts it (UEFI, Secure Boot off, rudolf.debug=1 for the serial shell).
# QEMU has no NVIDIA card. The extension nvidia-driver is the one .github/workflows/nvidia-check.yml built for the release's
# kernel (NVTREE: its files; NVIDIA's real modules, NOT signed with a key the release's kernel trusts), else a stand-in (a
# module of the release with its signature taken off, named nvidia.ko): either way the kernel must refuse to load it.
# The extension is signed by a TEST system key, trusted only by this test's build of the release (its desktop layer adds the
# key, as t160x_t.py's builds do; FIX=1 puts this tree's gpudriver, rudolf-init, session, sysext and rpk in it too).
#   1   no NVIDIA card: gpudriver status has no card and offers nothing (Settings › Display shows no Graphics driver)
#   2   the extension installed: in use after a restart, its modules kept from loading on the open driver; "set nvidia" refused
#   3-6 the stand-in card (rudolf.gputest=10de:2684, only with rudolf.debug=1) with NVIDIA's driver chosen: its module is
#       refused by the kernel (signature), the computer goes back to the open driver by itself (the choice, the note with the
#       kernel's reason, the health log), nothing of NVIDIA's is loaded, the open driver draws, the desktop comes up
#   7   the start after: the open driver, no new note
#   8   Recovery mode with NVIDIA's driver chosen: nothing of it (no extensions, gpudriver not run), the choice kept
#   python3 t161n_t.py REL-DIR      (REL-DIR with the release's boot.iso; SB_PARTS and QEMU as for t160; root; QEMU_ACCEL=kvm; FIX=1; NVTREE)
import os, sys, time, re, subprocess, shutil, struct, hashlib
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE); ISO = os.path.dirname(QT)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); FIX = os.environ.get('FIX') == '1'; NVTREE = os.environ.get('NVTREE', '')
PARTS = os.environ.get('SB_PARTS', '/nonexistent'); LOG = open('t161n.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[-1500:] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 3400; DRV = os.path.abspath('int161n.img'); MEM = int(os.environ.get('MEM', '3072')); W = os.path.abspath('w161n')
TOOL = f'{ISO}/pkgtools/rpk-tool.py'; KV = mv('kernel')
shutil.rmtree(W, ignore_errors=True); os.makedirs(W)
def run_(a, **k): return subprocess.run(a, check=True, **k)
for k in ('dev', 'sys'): run_([sys.executable, TOOL, 'keygen', f'{W}/{k}'], stdout=subprocess.DEVNULL)
DEVPUB = open(f'{W}/dev/rpk-signing.pub').read().strip(); SYSPUB = open(f'{W}/sys/rpk-signing.pub').read().strip()
ABI = hashlib.sha256(b'rudolf test base nvidia').hexdigest(); SER = int(mv('serial'))
def comp(sfs):
    o = subprocess.run(['unsquashfs', '-s', sfs], capture_output=True, text=True).stdout
    m = re.search(r'Compression (\w+)', o); return m.group(1) if m else 'xz'
def build():
    d = f'{W}/d'; run_(['unsquashfs', '-q', '-d', d, f'{REL}/desktop.sfs'], stdout=subprocess.DEVNULL)
    for x in ('etc/rudolf', 'usr/share/rudolf', 'usr/bin', 'usr/lib/rudolf', 'sbin'): os.makedirs(f'{d}/{x}', exist_ok=True)
    open(f'{d}/etc/rudolf/rpk-keys', 'w').write(open(f'{ISO}/trusted-keys.txt').read() + f'# TEST ONLY (t161n_t.py): the stand-in system key\n{SYSPUB}\n')
    open(f'{d}/usr/share/rudolf/base-abi', 'w').write(ABI + '\n')
    if FIX:
        run_(['gcc', '-O2', '-o', f'{d}/usr/bin/rpk', f'{ISO}/sys/rpk.c', '-lcrypto'])
        for s in ('usr/lib/rudolf/sysext', 'usr/lib/rudolf/gpudriver', 'usr/lib/rudolf/session', 'sbin/rudolf-init', 'usr/share/rudolf/nvidia-gpus.txt'):
            if os.path.exists(f'{ISO}/sys/root/{s}'): shutil.copy(f'{ISO}/sys/root/{s}', f'{d}/{s}'); os.chmod(f'{d}/{s}', 0o755)
    L = f'{W}/l'; os.makedirs(L)
    for x in ('base', 'chromium', 'modules'): os.link(f'{REL}/{x}.sfs', f'{L}/{x}.sfs')
    run_(['mksquashfs', d, f'{L}/desktop.sfs', '-noappend', '-comp', comp(f'{REL}/desktop.sfs'), '-quiet'], stdout=subprocess.DEVNULL)
    run_([sys.executable, f'{ISO}/systools/verity.py', 'add', f'{L}/desktop.sfs', 'desktop'], stdout=subprocess.DEVNULL)
    shutil.rmtree(d)
    run_([sys.executable, f'{ISO}/systools/make_release.py', '--layers', L, '--kernel', f'{REL}/vmlinuz', '--out', f'{W}/o', '--key', f'{W}/dev/rpk-signing.key',
          '--serial', str(SER + 1), '--version', mv('version') + '-nv', '--chromium', mv('chromium'), '--kernel-version', KV], stdout=subprocess.DEVNULL)
    return L, f'{W}/o/channel', SER + 1
B = build()
# ---- the extension: NVIDIA's real modules built for this kernel (not signed by a trusted key), or a stand-in
P = f'{W}/p/nvidia-driver'; os.makedirs(P)
if NVTREE:
    run_(['cp', '-a', NVTREE + '/.', P]); out('the extension: the check build\'s files from', NVTREE)
else:
    md = f'{W}/m'; run_(['unsquashfs', '-q', '-d', md, f'{REL}/modules.sfs'], stdout=subprocess.DEVNULL)
    ko = subprocess.run(f"find {md} -name 'dummy.ko' -o -name 'e1000.ko' | head -1", shell=True, capture_output=True, text=True).stdout.strip()
    raw = open(ko, 'rb').read(); i = raw.rfind(b'~Module signature appended~\n')
    stripped = raw
    if i > 0:   # (the signature block ends the file: its length is in the 12 bytes before the magic)
        sl = int.from_bytes(raw[i - 4:i], 'big'); stripped = raw[:i - 12 - sl]
    os.makedirs(f'{P}/lib/modules/{KV}/extra/nvidia')
    for m in ('nvidia', 'nvidia-modeset', 'nvidia-drm', 'nvidia-uvm'): open(f'{P}/lib/modules/{KV}/extra/nvidia/{m}.ko', 'wb').write(stripped)
    os.makedirs(f'{P}/usr/share/rudolf', exist_ok=True); open(f'{P}/usr/share/rudolf/nvidia-test.txt', 'w').write('stand-in\n'); shutil.rmtree(md)
    out('the extension: a stand-in (', os.path.basename(ko), 'without its signature )')
for f in ('PKGINFO',):
    open(f'{P}/{f}', 'w').write(f'name = nvidia-driver\nversion = 0.test\nkind = sysext\ntitle = NVIDIA test\nsummary = t161n\nlicense = test\nbase = {ABI}\nkernel = {KV}\n')
os.makedirs(f'{W}/t'); run_([sys.executable, TOOL, 'build', P, f'{W}/t/nvidia-driver.rpk', '--key', f'{W}/sys/rpk-signing.key'], stdout=subprocess.DEVNULL); shutil.rmtree(P)

def make_drive(path):
    start = 262144
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc/rudolf')
    L, ch, serial = B; d = f'{T}/os/system/a'; os.makedirs(d)
    shutil.copy(f'{REL}/vmlinuz', f'{d}/vmlinuz')
    for n in ('base', 'chromium', 'desktop', 'modules'): shutil.copy(f'{L}/{n}.sfs', f'{d}/{n}.sfs')
    for n in ('manifest.txt', 'manifest.sig'): shutil.copy(f'{ch}/{n}', f'{d}/{n}')
    open(f'{d}/RELEASE', 'w').write(f"serial {serial}\nversion {mv('version')}-nv\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {KV}\nnotes test\n")
    open(f'{d}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    open(f'{T}/os/update.conf', 'w').write('url=https://dev.example/rudolf\ndevurl=https://dev.example/rudolf\nauto=0\n')
    open(f'{T}/etc/rudolf/extra-keys', 'w').write(f'# Developer mode: a developer\'s publisher key\n{DEVPUB}\n')
    shutil.copytree(f'{W}/t', f'{T}/t')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'wb') as f:
        f.truncate((start + DMB * 2048 + 2048) * 512)
        with open(f'{REL}/boot.iso', 'rb') as i: shutil.copyfileobj(i, f)
        f.seek(446); f.write(bytes(16))
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(510); f.write(b'\x55\xaa'); f.seek(start * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)

class TSer(Ser):
    def __init__(s, path, logf): super().__init__(path); s.lf = open(logf, 'a')
    def read(s, t=1.0):
        o = super().read(t); s.lf.write(o); s.lf.flush(); return o
N = [0]
def boot(tag, extra_cmd=''):
    N[0] += 1
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf161n',
             '-kernel', f'{REL}/vmlinuz', '-append', f'console=ttyS0 loglevel=3 consoleblank=0 rudolf.debug=1 {extra_cmd}'.strip()]
    vm = sbvm.VM(f't161n{N[0]}{tag}', PARTS, [], secure=False, mem=MEM, extra=extra)
    vm.serlog = os.path.join(vm.dir, 'serial.txt')
    g = Guest.__new__(Guest); g.N = vm.name; g.s = TSer(vm.ser_path, vm.serlog); g.q = vm.q; g.s.read(0.3)
    time.sleep(20); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    out(f'--- start {N[0]} ({tag}):', 'up' if up else 'NOT UP')
    if not up: out('the serial line:', open(vm.serlog, errors='replace').read()[-4000:])
    return vm, g, up
def run(g, c, t=60):
    f = os.path.join(W, 'c161n.sh'); open(f, 'w').write(c + '\n'); g.put(f, '/tmp/c161n.sh', '644')
    o = g.sh('sh /tmp/c161n.sh 2>&1', t); out('$', c[:300].replace('\n', ' ; ')); out(o.rstrip()[-3000:]); return o
def desktop_up(g, secs=480):
    o = run(g, f"i=0; while [ $i -lt {secs // 5} ]; do [ -e /run/rudolf/heartbeat ] && break; i=$((i+1)); sleep 5; done; [ -e /run/rudolf/heartbeat ] && echo DESKTOP-UP", secs + 60)
    return 'DESKTOP-UP' in o, o
def stop(vm, g):
    try: g.sh('sync', 60)
    except Exception: pass
    vm.stop(); time.sleep(3)
G = '/usr/lib/rudolf/gpudriver'
nomods = lambda o: bool(re.search(r'^MODS=\s*$', o, re.M))
INFO = f"{G} status; echo; echo CHOICE=$(cat /persist/etc/gpu-driver 2>/dev/null); echo NOTE=$(cat /persist/etc/gpu-driver.fallback 2>/dev/null); " \
       "echo GPU=$(cat /run/rudolf/gpu 2>/dev/null); echo MODS=$(grep -oE '^nvidia[a-z_]*' /proc/modules | tr '\\n' ' '); cat /run/rudolf/gpudriver.log 2>/dev/null; " \
       "awk -F'\\t' '$1==\"nvidia-driver\"' /run/rudolf/ext/state; grep -c ' /usr/lib/modules/.*/nvidia.*\\.ko\\| nvidia.ko' /proc/mounts"

make_drive(DRV)
vm = None
try:
  try:
        vm, g, up = boot('install')
        if not up: ok('0. the test build starts', False, 'not up'); raise SystemExit
        o = run(g, INFO, 60)
        ok('1. no NVIDIA card: gpudriver offers nothing (no card; Settings › Display shows no Graphics driver)', '"card":null' in o and '"offered":false' in o and nomods(o), o)
        o = run(g, f"rpk install-file --system /persist/t/nvidia-driver.rpk; echo rc=$?", 300)
        ok('...the extension nvidia-driver installs (TEST system key), in use after a restart', 'rc=0' in o, o)
        stop(vm, g)
        vm, g, up = boot('open')
        o = run(g, INFO + f"; {G} set nvidia; echo; echo KO=$(ls /lib/modules/$(uname -r)/extra/nvidia/ | tr '\\n' ' '); echo FW=$(ls /lib/firmware/nvidia/*/ 2>/dev/null | head -3 | tr '\\n' ' '); "
                   "echo COVERED=$(grep -c 'extra/nvidia/nvidia' /proc/mounts); grep -h ' sysext \\|sysext ' /proc/mounts | cut -d' ' -f2-3; tail -5 /run/rudolf/sysext.log; "
                   "grep -h '\"k\":\"extension\"' /persist/var/log/health.log | tail -3", 60)
        ok('2. the extension in use, its modules and GSP firmware laid over the base; the open driver keeps the modules from loading (covered); set nvidia refused (no card)',
           up and re.search(r'^nvidia-driver\t[^\t]*\tin-use', o, re.M) and nomods(o) and '"error":"this computer has no NVIDIA graphics card"' in o and re.search(r'^KO=.*nvidia\.ko', o, re.M) and re.search(r'^FW=\S', o, re.M) and re.search(r'^COVERED=[1-9]', o, re.M), o)
        up2, o2 = desktop_up(g)
        run(g, "echo nvidia > /persist/etc/gpu-driver; sync", 20)   # (as Settings would, had the card been offered)
        stop(vm, g)
        vm, g, up = boot('fail', 'rudolf.gputest=10de:2684')
        o = run(g, INFO + "; dmesg | grep -iE 'nvidia|module verification|Key was rejected|Loading of unsigned' | tail -5; grep -h 'graphics\\|open driver' /persist/var/log/health.log | tail -3", 90)
        ok('3. the stand-in card with NVIDIA\'s driver chosen: offered, tried, its module refused by the kernel (not signed by a trusted key)',
           up and '"offered":true' in o and re.search(r'did not load: (Key was rejected by service|Required key not available|Operation not permitted|Invalid module format)', o), o)
        ok('4. ...back to the open driver by itself: the choice reset, the note with the reason (the desktop\'s notification)', 'CHOICE=open' in o and 'NOTE=' in o and '"fellBack"' in o and re.search(r'NOTE=\S', o), o)
        ok('5. ...nothing of NVIDIA\'s loaded, the open driver draws, and the health log says so', nomods(o) and re.search(r'GPU=\S', o) and 'back on the open driver' in o, o)
        up2, o2 = desktop_up(g)
        ok('6. ...and the desktop comes up', up2, o2)
        stop(vm, g)
        vm, g, up = boot('after', 'rudolf.gputest=10de:2684')
        o = run(g, f"{G} ack >/dev/null; " + INFO, 60)
        ok('7. the start after: the open driver, nothing tried, no new note', up and 'CHOICE=open' in o and 'NOTE=' in o and not re.search(r'NOTE=\S', o) and nomods(o), o)
        run(g, "echo nvidia > /persist/etc/gpu-driver; sync", 20)
        stop(vm, g)
        vm, g, up = boot('recovery', 'rudolf.recovery=1 rudolf.gputest=10de:2684')
        o = run(g, INFO + "; cat /run/rudolf/ext/state", 60)
        ok('8. Recovery mode with NVIDIA\'s driver chosen: no extensions, gpudriver not run, nothing of NVIDIA\'s loaded, the choice kept',
           up and 'Recovery mode starts without system extensions' in o and nomods(o) and 'CHOICE=nvidia' in o and not re.search(r'NOTE=\S', o), o)
        stop(vm, g); vm = None
  except SystemExit: out('stopped early')
finally:
    try: vm and vm.stop()
    except Exception: pass
    shutil.rmtree(W, ignore_errors=True)
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
