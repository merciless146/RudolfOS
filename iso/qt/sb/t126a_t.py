# Rudolf OS 1.2.6 stage A in QEMU: "Check this disc or USB drive for errors" from the UEFI boot menu (UEFI, Secure
# Boot off, EFI variables kept across restarts).
#   python3 t126a_t.py DISC.iso
# DISC.iso: a 1.2.6 test disc (mkdisc.sh with RUDOLF_UKI_TEST_CMDLINE=rudolf.debug=1: a serial shell).
# The boot menu's default is set to the check entry (Settings' Boot menu, rudolfd bootmenu/set), then the computer
# starts three times, pressing Enter at the "Press Enter to continue" (the check waits for it on the screen):
#   1. the disc as made: every file intact, the boot loader included (1.2.5 and older: always "ERRORS FOUND")
#   2. the same disc with its list as older discs have it (the boot loader's line as built, not as patched): intact
#   3. the disc with one byte of README.TXT changed: ERRORS FOUND, naming readme.txt
# The result is read where Settings reads it (rudolfd's check, from /etc/rudolf/check.txt).
import os, sys, time, json, re, hashlib, shutil, struct
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT); sys.path.insert(0, os.path.join(QT, '..', 'systools'))
import sbvm, iso_extract
from guest import Guest
from ser import Ser
ISO = os.path.abspath(sys.argv[1])
PARTS = os.environ['SB_PARTS']; LOG = open('t126a.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:300] if d != '' else ''))
def last(t): return t.strip().split('\n')[-1].strip() if t.strip() else ''

def extent(iso, path):
    with open(iso, 'rb') as f: lba, size = iso_extract.find(f, path)
    return lba * 2048, size
def variant(name, change):
    """a copy of the disc with one change made in place (the files keep their places and sizes)"""
    p = os.path.abspath(name); shutil.copy(ISO, p)
    with open(p, 'r+b') as f: change(f, p)
    return p
def old_list(f, p):   # the boot loader's line as older discs have it: the file as built (the kernel's place zero)
    o, n = extent(p, 'BOOT/RBOOT.BIN'); f.seek(o); b = bytearray(f.read(n)); j = b.find(b'RBTB'); b[j + 4:j + 12] = b'\0' * 8
    built = hashlib.sha256(b).hexdigest().encode()
    so, sn = extent(p, 'RUDOLF/SHA256'); f.seek(so); s = f.read(sn)
    m = re.search(rb'^([0-9a-f]{64})  boot/rboot\.bin$', s, re.M); assert m
    f.seek(so + m.start(1)); f.write(built)
    out('older list: boot/rboot.bin', m.group(1)[:12].decode(), '->', built[:12].decode())
def damage(f, p):
    o, n = extent(p, 'README.TXT'); f.seek(o + 100); c = f.read(1); f.seek(o + 100); f.write(bytes([c[0] ^ 0x20]))

def boot(iso, tag):
    """start from iso with the boot menu's default kept from before; Enter at the check; the result"""
    vm = sbvm.VM('t126a', PARTS, [iso], secure=False, nvram=True, mem=3072)
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q
    try:
        time.sleep(40); end = time.time() + 1200; up = False
        while time.time() < end:
            try:
                if 'ready' in g.sh('echo ready', 8): up = True; break
            except OSError: pass
            vm.keys('ret')   # the check's "Press Enter to continue" (and the boot menu, which then starts its default)
            time.sleep(8)
        if not up: vm.shot(tag + '-nostart'); return None, None, vm
        r = last(g.sh('cat /etc/rudolf/check.txt 2>&1', 20)); e = last(g.sh('tail -c +5 /sys/firmware/efi/efivars/LoaderEntrySelected-4a67b082-0a4c-41cf-b6c7-440b29bb8c4f | tr -d "\\000"', 20))
        vm.shot(tag)
        return r, e, vm
    except Exception as x:
        out('error', tag, x); return None, None, vm

try:
    # first start: the debug entry (the disc's default), to choose the check entry as the menu's default
    vm = sbvm.VM('t126a', PARTS, [ISO], secure=False, nvram=True, mem=3072)
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q
    time.sleep(40); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    ok('starts (UEFI, the debug entry has the serial shell)', up)
    end = time.time() + 600
    while time.time() < end and g.js('typeof rdPost=="function"', 20) is not True: time.sleep(5)
    r = g.js("rdPost('bootmenu/set?default=rudolf-check.conf&timeout=1').then(()=>'saved',e=>e.message)", 60)
    ok('the boot menu starts the check by default (Settings > Boot menu)', r == 'saved', r)
    vm.stop()

    r, e, vm = boot(ISO, 'good'); vm.stop()
    out('1. selected', e, '| result:', r)
    m = re.match(r'All (\d+) files are intact\.$', r or '')
    ok('1. the disc as made: all files intact, the boot loader included', m and int(m.group(1)) > 40 and e == 'rudolf-check.conf', [r, e])

    v = variant('old126a.iso', old_list)
    r, e, vm = boot(v, 'older-list'); vm.stop(); os.remove(v)
    out('2. result:', r)
    ok('2. a list as older discs have it (the boot loader as built): intact too', bool(re.match(r'All \d+ files are intact\.$', r or '')), r)

    v = variant('bad126a.iso', damage)
    r, e, vm = boot(v, 'damaged'); vm.stop(); os.remove(v)
    out('3. result:', r)
    ok('3. one byte of README.TXT changed: ERRORS FOUND, naming it', bool(re.match(r'ERRORS FOUND in 1 of \d+ files \(readme\.txt\)', r or '')), r)
finally:
    try: vm.stop()
    except Exception: pass
out('%d/%d passed' % (sum(R), len(R)))
