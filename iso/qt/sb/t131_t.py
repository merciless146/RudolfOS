# Rudolf OS 1.3.1 in QEMU (UEFI, Secure Boot off): an installed computer whose boot files are older than 1.3.0 starts
# its installed update, with an older Rudolf OS USB stick that keeps its own files plugged in.
#   python3 t131_t.py OLD-DISC.iso OLD-STICK.iso REL-DIR
# OLD-DISC.iso: a disc from before 1.3.0 (1.2.6): the installed drive's boot files. Its start-up passes nothing on to
#   the update's kernel (no rudolf.mediadev=, no rudolf.persist=).
# OLD-STICK.iso: another older disc (1.2.0), written to a USB stick with its own RUDOLFDATA (home: "stick"), no update.
# REL-DIR: the release under test (manifest.txt/.sig and the five files), installed in slot a of the internal drive.
# The internal drive is a virtio disk: the firmware's boot entry for it names neither USB nor NVMe/SATA/SCSI, as on
# laptops whose firmware keeps a short boot entry (HD() only), so the start-up cannot tell which kind it started from.
# 1.3.0 took the first Rudolf OS drive found (the stick, /dev/sda1) and its own RUDOLFDATA: the laptop ran the stick's
# 1.1 system and files ("Rudolf OS 1.1", "checked never", no Wi-Fi). 1.3.1: the data partition that holds the slot,
# and the image on that drive.
import os, sys, time, re, subprocess, shutil, struct
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
OLD, OLDSTICK, REL = (os.path.abspath(x) for x in sys.argv[1:4])
PARTS = os.environ['SB_PARTS']; LOG = open('t131.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:300] if d != '' else ''))
def last(t): return t.strip().split('\n')[-1].strip() if t.strip() else ''
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
VER = '.'.join(mv('version').split('.')[:3]); DMB = 1200


def make_drive(path, image, who, slot=False):
    """the disc image, then RUDOLFDATA as partition 3, as Install here and Make a stick make them"""
    start = (os.path.getsize(image) + 1048575) // 1048576 * 2048
    if os.path.exists(path): os.remove(path)
    with open(path, 'wb') as f:
        with open(image, 'rb') as o: shutil.copyfileobj(o, f)
        f.truncate((start + DMB * 2048 + 2048) * 512)
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    open(f'{T}/home/rudolf/whoami.txt', 'w').write(who + '\n'); subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    if slot:
        a = f'{T}/os/system/a'; os.makedirs(a)
        for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs', 'manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{a}/{n}')
        open(f'{a}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n"); open(f'{a}/complete', 'w').write('1\n')
        # just updated: the new system is being tried (two starts), as on the laptop
        open(f'{T}/os/system/state', 'w').write('good=\nprev=\nnext=a\ntries=2\nfailed=\n')
    D = path + '.data'
    subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)


def session(vm):
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3); return g
def wait_shell(g, delay=45, limit=900):
    time.sleep(delay); end = time.time() + limit
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): return True
        except OSError: pass
        time.sleep(4)
    return False


INT = os.path.abspath('int131.img'); STK = os.path.abspath('oldstick131.img')
make_drive(INT, OLD, 'internal', slot=True); make_drive(STK, OLDSTICK, 'stick')
VIRTIO = ['-drive', f'file={INT},format=raw,if=none,id=vd', '-device', 'virtio-blk-pci,drive=vd,bootindex=0']
USB = ['-device', 'qemu-xhci,id=xh', '-drive', f'file={STK},format=raw,if=none,id=us', '-device', 'usb-storage,bus=xh.0,drive=us,removable=on']

for n in (1, 2):
    vm = sbvm.VM(f't131{n}', PARTS, [], secure=False, mem=3072, extra=VIRTIO + USB); g = session(vm); sh = lambda c, t=60: g.sh(c, t)
    try:
        up = wait_shell(g)
        if not up:
            for i in range(3): vm.shot(f'fail{i}'); time.sleep(5)
        st = sh('for f in slot media persist bootvia; do echo "$f=$(cat /etc/rudolf/$f)"; done; echo "who=$(cat /home/rudolf/whoami.txt)"; grep -o "RVER=.[0-9.]*" /usr/share/rudolf/www/index.html | head -1; echo "cmd=$(cat /etc/rudolf/cmdline)"', 30) if up else ''
        kv = dict(l.split('=', 1) for l in st.split('\n') if '=' in l and not l.startswith('RVER'))
        rv = (re.search(r"RVER=.([0-9.]+)", st) or [None, ''])[1]
        out(f'start {n}:', ' | '.join(f'{k}={v}' for k, v in kv.items() if k != 'cmd'), '| RVER', rv)
        ok(f'{n}. the installed drive (boot files from before 1.3.0) starts its update by kexec, a stick of its own plugged in', up and kv.get('slot') == 'a', kv.get('slot'))
        ok('...the firmware\'s boot entry does not say the kind of drive (as on the laptop)', kv.get('bootvia', 'x') == '', kv.get('bootvia'))
        ok('...the older boot files passed nothing on (no rudolf.persist=, no rudolf.mediadev=)', 'rudolf.persist=' not in kv.get('cmd', '') and 'rudolf.mediadev=' not in kv.get('cmd', ''), kv.get('cmd', '')[-160:])
        ok('...the update runs with the internal drive\'s files (not the stick\'s)', kv.get('persist') == '/dev/vda3' and kv.get('who') == 'internal', [kv.get('persist'), kv.get('who')])
        ok('...and the image on the same drive (not the stick\'s)', kv.get('media') == '/dev/vda1', kv.get('media'))
        ok(f'...and its own system: {VER} (1.3.0 ran the stick\'s older one)', rv == VER, rv)
        if n == 1:
            # the next start finds the same; the update is not marked failed meanwhile
            o = sh('cat /persist/os/system/state', 20)
            ok('...the internal drive keeps the update (being tried, or kept once the desktop started), not failed', ('next=a' in o or 'good=a' in o) and re.search(r'^failed=$', o, re.M) is not None, o.replace('\n', ' '))
            g.s.s.send(b'sync; reboot -f\n'); time.sleep(5)
    finally:
        vm.stop()
out(f'{sum(R)}/{len(R)} passed')
