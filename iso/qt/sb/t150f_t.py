# Rudolf OS 1.5.0 in QEMU: an update that cannot start falls back to the system that worked. Slot a holds the
# release (good), slot b the same release with its desktop layer damaged, waiting to be tried (next=b, tries=1).
# Start 1: the release's kernel tries slot b; its layer cannot be mounted, so the start-up marks b failed and restarts.
# Start 2: slot a, the system the state file now names as good (a computer's own boot files choose it from the
# state file; here, with no Rudolf OS boot drive in the virtual machine, it is passed as rudolf.slot=a), which starts;
# the health log says the update fell back.
#   python3 t150f_t.py REL-DIR
import os, sys, time, re, subprocess, shutil, struct, json, socket, random
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); LOG = open('t150f.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:400] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 2400; DRV = os.path.abspath('int150f.img'); MEM = int(os.environ.get('MEM', '3072'))
PG, PH = 47611, 47612   # QEMU listens on PG for frames from here; sends the guest's frames to PH
GMAC = bytes.fromhex('525400123499'); HMAC = bytes.fromhex('525400aabbcc'); HIP = bytes([10, 5, 0, 1]); GIP = bytes([10, 5, 0, 2])

def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf/Downloads'); os.makedirs(f'{T}/etc')
    os.makedirs(f'{T}/debian/usr/bin'); open(f'{T}/debian/marker', 'w').write('debian here\n')
    open(f'{T}/home/rudolf/Downloads/secret.txt', 'w').write('my secret download\n')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    for sl in ('a', 'b'):
        a = f'{T}/os/system/{sl}'; os.makedirs(a)
        for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs', 'manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{a}/{n}')
        open(f'{a}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n")
        open(f'{a}/complete', 'w').write('1\n')
    with open(f'{T}/os/system/b/desktop.sfs', 'r+b') as f: f.write(b'\0' * 4096)   # not a squashfs any more
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=b\ntries=1\nfailed=\n')
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    # (no -O encrypt: a drive formatted before 1.5.0; setup adds the feature)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)


def run(append, tag, wait_up=True):
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf150f', '-no-reboot',
             '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.debug=1 ' + append]
    vm = sbvm.VM('t150f' + tag, PARTS, [], secure=False, mem=MEM, extra=extra)
    return vm
def state():
    # read the state file from the drive image (the VM is stopped)
    o = subprocess.run(f"debugfs -R 'cat /os/system/state' -f /dev/null {DRV}?offset={2048*512} 2>/dev/null || true", shell=True, capture_output=True, text=True).stdout
    if 'good=' not in o:
        subprocess.run(['dd', f'if={DRV}', 'of=/tmp/t150f.part', 'bs=1M', 'skip=1', 'status=none'], check=True)
        o = subprocess.run(['debugfs', '-R', 'cat /os/system/state', '/tmp/t150f.part'], capture_output=True, text=True).stdout
        os.remove('/tmp/t150f.part')
    return o
make_drive(DRV)
vm = None
try:
    # start 1: the update in slot b, being tried
    vm = run('rudolf.slot=b rudolf.trying=1', 'a'); t0 = time.time()
    while time.time() - t0 < 600 and vm.p.poll() is None: time.sleep(3)
    gone = vm.p.poll() is not None; out('start 1 ended by itself after', round(time.time() - t0), 's' if gone else 's (still running)')
    try: vm.stop()
    except Exception: pass
    st = state(); out(st)
    ok('1. the damaged update stops its own start and restarts', gone)
    ok('2. ...after marking slot b failed and leaving a as the system that works', 'good=a' in st and 'failed=b' in st and re.search(r'^next=$', st, re.M), st)
    # start 2: the kernel chooses (as after a restart)
    vm = sbvm.VM('t150fb', PARTS, [], secure=False, mem=MEM, extra=['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf150f',
             '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.debug=1 rudolf.slot=a'])
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    time.sleep(45); end = time.time() + 900; up = False; t1 = time.time(); shots = 0; seen = ''
    while time.time() < end:
        try:
            r = g.sh('echo ready', 8); seen += r
            if 'ready' in r: up = True; break
        except OSError: pass
        if time.time() - t1 > 60 * shots: vm.shot('start2'); shots += 1
        time.sleep(4)
    if not up: out('serial during start 2:', seen[-1500:])
    ok('3. the next start comes up', up)
    if up:
        time.sleep(20)
        o = g.sh("echo SLOT $(cat /etc/rudolf/slot); grep -E 'fallback|started' /persist/var/log/health.log | cut -c1-200; cat /persist/os/system/state")
        out(o)
        ok('4. ...on slot a, the system that works', re.search(r'^SLOT a$', o, re.M) is not None, o)
        ok('5. the health log says the update did not start and the computer went back', '"k":"fallback"' in o and 'slot b did not start' in o, o)
finally:
    try: vm and vm.stop()
    except Exception: pass
try: os.remove(DRV)
except OSError: pass
out(f'{sum(R)}/{len(R)} passed')
