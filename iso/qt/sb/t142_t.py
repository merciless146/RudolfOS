# Rudolf OS 1.4.2 in QEMU (UEFI, Secure Boot off): an update's kernel started with nothing passed on (as boot files
# from before 1.3.0 start it: rudolf.slot=a only) takes its own system, not another data partition's slot a.
#   python3 t142_t.py OLD-DISC.iso REL-DIR PREV-REL-DIR
# The Vivobook (1.4.1): its 1.1 stick handed over to 1.4.1 in the internal drive's slot a with nothing passed on; the
# 1.4.1 start-up took the first RUDOLFDATA found, the stick's, whose slot a held 1.4.0, and started 1.4.0's layers
# with the 1.4.1 kernel, which refused 1.4.0's drivers (each build signs its own): no Wi-Fi, no graphics driver.
# Here: the release's kernel is started directly (-kernel), as the handover does, with rudolf.slot=a. A drive with a
# data partition whose slot a holds PREV-REL (home: "stick") is a virtio disk, found first; the internal NVMe drive
# holds REL in slot a (home: "internal").
import os, sys, time, re, subprocess, shutil, struct
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
OLD, REL, PREV = (os.path.abspath(x) for x in sys.argv[1:4])
PARTS = os.environ['SB_PARTS']; LOG = open('t142.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:300] if d != '' and not c else ''))
def last(t): return t.strip().split('\n')[-1].strip() if t.strip() else ''
def man(d): return open(f'{d}/manifest.txt').read()
mv = lambda d, k: re.search(rf'^{k} (.*)$', man(d), re.M).group(1)
VER = '.'.join(mv(REL, 'version').split('.')[:3]); PVER = '.'.join(mv(PREV, 'version').split('.')[:3]); DMB = 1500


def make_drive(path, who, rel):
    start = (os.path.getsize(OLD) + 1048575) // 1048576 * 2048
    if os.path.exists(path): os.remove(path)
    with open(path, 'wb') as f:
        with open(OLD, 'rb') as o: shutil.copyfileobj(o, f)
        f.truncate((start + DMB * 2048 + 2048) * 512)
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    open(f'{T}/home/rudolf/whoami.txt', 'w').write(who + '\n'); subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    a = f'{T}/os/system/a'; os.makedirs(a)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs', 'manifest.txt', 'manifest.sig'): shutil.copy(f'{rel}/{n}', f'{a}/{n}')
    open(f'{a}/RELEASE', 'w').write(f"serial {mv(rel, 'serial')}\nversion {mv(rel, 'version')}\nreleased {mv(rel, 'released')}\nchromium {mv(rel, 'chromium')}\nkernel {mv(rel, 'kernel')}\nnotes test\n")
    open(f'{a}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    D = path + '.data'
    subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)


INT = os.path.abspath('int142.img'); STK = os.path.abspath('stick142.img')
make_drive(INT, 'internal', REL); make_drive(STK, 'stick', PREV)
EXTRA = ['-drive', f'file={STK},format=raw,if=none,id=vd', '-device', 'virtio-blk-pci,drive=vd,addr=0x4',
         '-drive', f'file={INT},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf142,addr=0x9',
         '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.trying=1 rudolf.debug=1']
vm = sbvm.VM('t142', PARTS, [], secure=False, mem=3072, extra=EXTRA)
g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3); sh = lambda c, t=60: g.sh(c, t)
try:
    time.sleep(45); up = False; end = time.time() + 900
    while time.time() < end:
        try:
            if 'ready' in sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    if not up:
        for i in range(3): vm.shot(f'fail{i}'); time.sleep(5)
    st = sh('for f in slot media persist; do echo "$f=$(cat /etc/rudolf/$f)"; done; echo "who=$(cat /home/rudolf/whoami.txt)"; echo "first=$(findfs LABEL=RUDOLFDATA)"; modprobe cfg80211 2>/dev/null; echo "mods=$(grep -c "^cfg80211 " /proc/modules)"; echo "cmd=$(cat /proc/cmdline)"', 30) if up else ''
    kv = dict(l.split('=', 1) for l in st.split('\n') if '=' in l)
    rv = last(sh("grep -o \"RVER='[0-9.]*'\" /usr/share/rudolf/www/index.html | head -1 | tr -dc 0-9.; echo", 90)) if up else ''
    out('start:', ' | '.join(f'{k}={v}' for k, v in kv.items()), '| RVER', rv)
    ok('1. the update\'s kernel, started with nothing passed on (rudolf.slot=a only), comes up', up and kv.get('slot') == 'a', kv.get('slot'))
    if kv.get('first') != '/dev/vda3':
        out(f'NOTE the first RUDOLFDATA found was {kv.get("first")}, not the other drive\'s: the case is not exercised')
    ok('...the other drive\'s data partition (slot a: ' + PVER + ') is the first one found', kv.get('first') == '/dev/vda3', kv.get('first'))
    ok('...the start-up took the data partition whose slot a is this kernel\'s own (the internal drive)', kv.get('persist') == '/dev/nvme0n1p3' and kv.get('who') == 'internal', [kv.get('persist'), kv.get('who')])
    ok(f'...and runs its own system ({VER}), not the other drive\'s ({PVER})', rv == VER, rv)
    ok('...with its drivers (cfg80211, the Wi-Fi core, loads: its signature is this build\'s)', kv.get('mods') == '1', kv.get('mods'))
finally:
    vm.stop()
for f in (INT, STK):
    try: os.remove(f)
    except OSError: pass
out(f'{sum(R)}/{len(R)} passed')
