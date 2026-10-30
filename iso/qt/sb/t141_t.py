# Rudolf OS 1.4.1 in QEMU (UEFI, Secure Boot off, EFI variables kept across restarts): an installed drive from before
# 1.4.1 gets the installed layout through Get new boot files, then starts from it by its own boot entry.
#   python3 t141_t.py OLD-DISC.iso REL-DIR BOOT.iso PREV-REL-DIR
# OLD-DISC.iso: a disc from before 1.4.1 (1.2.6), the drive's boot files (its Debug start entry is made the default).
# REL-DIR: the release under test (1.4.1), installed in slot a. BOOT.iso: its boot image (a test disc's, rudolf.debug=1).
# PREV-REL-DIR: an older release (1.4.0) in slot b, for a rollback: its start-up does not know the installed layout.
# 1. The drive as Install here made it before 1.4.1 (the image's hybrid table, as on the Vivobook), with the boot
#    image staged as Get new boot files stages it. Its start hands over to slot a, whose start-up writes the boot
#    image, then the installed layout (partition 1 cleared, a disk signature of its own), and restarts.
# 2. It starts from its new boot files: the image read from the whole drive (a loop device), RUDOLFDATA, slot a; rudolfd
#    adds the "Rudolf OS" boot entry, first in the boot order.
# 3. The next start comes from that entry (BootCurrent). 3b (1.4.2): when firmware skips it, it takes the path of the
#    firmware's own entry for the drive that the start came through.
# 4. A rollback to slot b (1.4.0): its start-up ignores rudolf.mediadisk=, finds no media and still takes RUDOLFDATA.
# QEMU's firmware also starts the image's hybrid table, so the laptop's own refusal is not shown here (that is checked
# on the laptop); this test shows the layout is written right and that every start-up gets along with it.
import hashlib, os, re, shutil, struct, subprocess, sys, time
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
OLD, REL, BOOT, PREV = (os.path.abspath(x) for x in sys.argv[1:5])
PARTS = os.environ['SB_PARTS']; LOG = open('t141.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:300] if d != '' and not c else ''))
def last(t): return t.strip().split('\n')[-1].strip() if t.strip() else ''
def man(d): return open(f'{d}/manifest.txt').read()
mv = lambda d, k: re.search(rf'^{k} (.*)$', man(d), re.M).group(1)
VER = '.'.join(mv(REL, 'version').split('.')[:3]); DMB = 1500
DRV = os.path.abspath('int141.img')

def slot(T, name, rel):
    a = f'{T}/os/system/{name}'; os.makedirs(a)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs', 'manifest.txt', 'manifest.sig'): shutil.copy(f'{rel}/{n}', f'{a}/{n}')
    open(f'{a}/RELEASE', 'w').write(f"serial {mv(rel, 'serial')}\nversion {mv(rel, 'version')}\nreleased {mv(rel, 'released')}\nchromium {mv(rel, 'chromium')}\nkernel {mv(rel, 'kernel')}\nnotes test\n")
    open(f'{a}/complete', 'w').write('1\n')

start = (os.path.getsize(OLD) + 1048575) // 1048576 * 2048
with open(DRV, 'wb') as f:
    with open(OLD, 'rb') as o: shutil.copyfileobj(o, f)
    f.truncate((start + DMB * 2048 + 2048) * 512)
with open(DRV, 'r+b') as f:
    head = f.read(64 << 20); i = head.find(b'default rudolf-start.conf'); assert i > 0
    f.seek(i); f.write(b'default rudolf-debug.conf')
T = os.path.abspath('tree141'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
open(f'{T}/home/rudolf/whoami.txt', 'w').write('internal\n'); subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
slot(T, 'a', REL); slot(T, 'b', PREV)
open(f'{T}/os/system/state', 'w').write('good=a\nprev=b\nnext=\ntries=0\nfailed=\n')
# the boot image staged as bootfiles.sh stage does it
os.makedirs(f'{T}/os/bootimg'); shutil.copy(BOOT, f'{T}/os/bootimg/boot.iso')
blocks = struct.unpack('<I', open(BOOT, 'rb').read(16 * 2048 + 84)[16 * 2048 + 80:16 * 2048 + 84])[0]
bsha = hashlib.sha256(open(BOOT, 'rb').read()).hexdigest()
open(f'{T}/os/bootimg/pending', 'w').write(f'sha={bsha}\nstart={start}\nblocks={blocks}\nversion={VER}\nserial={mv(REL, "serial")}\n')
subprocess.run(['truncate', '-s', f'{DMB}M', 'data141'], check=True)
subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, 'data141'], check=True); shutil.rmtree(T)
with open(DRV, 'r+b') as f:
    f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
    f.seek(start * 512)
    with open('data141', 'rb') as d: shutil.copyfileobj(d, f)
os.remove('data141')
mbr0 = open(DRV, 'rb').read(512)

NVME = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf141']   # no bootindex: QEMU would make the firmware put its own entry first at every start
vm = sbvm.VM('t141', PARTS, [], secure=False, nvram=True, mem=3072, extra=NVME)
g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3); sh = lambda c, t=60: g.sh(c, t)
def wait_shell(delay=45, limit=900):
    time.sleep(delay); end = time.time() + limit
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): return True
        except OSError: pass
        time.sleep(4)
    return False
def state():
    st = sh('for f in slot media persist; do echo "$f=$(cat /etc/rudolf/$f)"; done; echo "who=$(cat /home/rudolf/whoami.txt)"; echo "cmd=$(cat /etc/rudolf/cmdline)"; echo "mnt=$(grep " /live/media " /proc/mounts | cut -d" " -f1)"', 30)
    return dict(l.split('=', 1) for l in st.split('\n') if '=' in l)
E = '/sys/firmware/efi/efivars'; G = '8be4df61-93ca-11d2-aa0d-00e098032b8c'
def efi(n): return last(sh(f'tail -c +5 {E}/{n}-{G} 2>/dev/null | od -An -tx2 | tr -d " \\n"', 20))
def boot_entries(): return sh(f'for f in {E}/Boot[0-9A-F][0-9A-F][0-9A-F][0-9A-F]-{G}; do n=${{f##*/}}; printf "%s=" ${{n%%-*}}; tail -c +11 $f | tr -d "\\000" | head -c 30; echo; done', 30)
try:
    # 1. the old drive's start hands over to slot a (1.4.1), which writes the boot image and the layout, then restarts
    up = wait_shell()
    ok('1. the drive installed before 1.4.1 starts, the new boot files are written and it restarts by itself', up)
    st = state()
    mbr = bytes.fromhex(last(sh('dd if=/dev/nvme0n1 bs=512 count=1 2>/dev/null | od -An -tx1 -v | tr -d " \\n"', 30)))
    ok('2. it starts from its new boot files (the boot image, 1.4.1), then slot a', st.get('slot') == 'a' and 'rudolf.slot=a' in st.get('cmd', '') and last(sh('cat /persist/os/bootimg/result', 20)).startswith('written'), [st.get('slot'), last(sh('cat /persist/os/bootimg/result', 20))])
    ok('...the drive has the installed layout: partition 1 cleared, partitions 2 (EFI) and 3 (RUDOLFDATA) kept', len(mbr) == 512 and mbr[446:462] == bytes(16) and mbr[462 + 4] == 0xef and mbr[478:494] == mbr0[478:494] and mbr[510:] == b'\x55\xaa', mbr[440:512].hex())
    ok('...and a disk signature of its own', mbr[440:444] not in (mbr0[440:444], bytes(4)), [mbr0[440:444].hex(), mbr[440:444].hex()])
    ok('...the kernel sees no partition 1', last(sh('ls /dev/nvme0n1p1 2>/dev/null | wc -l', 20)) == '0' and last(sh('ls /dev/nvme0n1p2 /dev/nvme0n1p3 | wc -l', 20)) == '2')
    ok('...the image is read from the whole drive, through a loop device (media /dev/nvme0n1)', st.get('media') == '/dev/nvme0n1' and st.get('mnt', '').startswith('/dev/loop'), [st.get('media'), st.get('mnt')])
    ok('...with its own files on RUDOLFDATA', st.get('persist') == '/dev/nvme0n1p3' and st.get('who') == 'internal', [st.get('persist'), st.get('who')])
    ok('...the handover passed the whole drive as rudolf.mediadisk= (not rudolf.mediadev=)', 'rudolf.mediadisk=/dev/nvme0n1' in st.get('cmd', '') and 'rudolf.mediadev=' not in st.get('cmd', ''), st.get('cmd', '')[-200:])
    be = boot_entries(); order = efi('BootOrder'); m = re.search(r'Boot([0-9A-F]{4})=Rudolf OS(?! \()', be)
    ok('...rudolfd made the "Rudolf OS" boot entry, first in the boot order', m and order[:4] == m.group(1), [be, order])
    sig = mbr[440:444].hex(); ent = last(sh(f'tail -c +5 {E}/Boot{m.group(1) if m else "XXXX"}-{G} 2>/dev/null | od -An -tx1 -v | tr -d " \\n"', 20))
    fwn = re.search(r'Boot([0-9A-F]{4})=UEFI QEMU NVMe', be)
    fw = last(sh(f'tail -c +5 {E}/Boot{fwn.group(1) if fwn else "XXXX"}-{G} 2>/dev/null | od -An -tx1 -v | tr -d " \\n"', 20))
    fwp = fw[fw.find('02010c00'):fw.find('7fff0400') + 8] if '02010c00' in fw and '7fff0400' in fw else 'none'
    ok('...naming the drive itself, as the firmware\'s own entry for it does (PciRoot/Pci/NVMe, no partition or file)', ent.endswith(fwp) and len(fwp) > 20, [ent[:200], fwp])
    mine = m.group(1) if m else ''
    # 1.4.2: the desktop's Chromium does not tell pages it is controlled by automation (Google's sign-in refuses that)
    ca = sh('for p in /proc/[0-9]*; do tr "\\000" " " < $p/cmdline 2>/dev/null | grep -q "^/opt/chromium/chrome .*RudolfDesktop" && tr "\\000" " " < $p/cmdline; done | head -1', 30)
    ok('...the desktop\'s Chromium hides automation from pages (--disable-blink-features=AutomationControlled)', ca.strip() != '' and ('AutomationControlled' in ca or 'remote-debugging-pipe' not in ca), ca[-300:])
    # 3. the next start through that entry; rudolfd writes nothing when all is already so
    out('NOTE before the restart: BootOrder', efi('BootOrder'))
    pre = last(sh(f'tail -c +5 {E}/Boot{mine}-{G} 2>/dev/null | od -An -tx1 -v | tr -d " \\n"', 20)) if mine else ''
    sh('sync; reboot -f', 10)
    time.sleep(5)
    up = wait_shell(delay=40)
    cur = efi('BootCurrent'); st = state()
    out('NOTE after the restart: BootOrder', efi('BootOrder'), '| entries:', boot_entries().replace('\n', ' ; ')[:600])
    for n in sorted(set(re.findall(r'Boot([0-9A-F]{4})=', boot_entries()))):
        out('NOTE', n, last(sh(f'tail -c +5 {E}/Boot{n}-{G} | od -An -tx1 -v | tr -d " \\n"', 20))[:300])
    ok('3. the next start came from the "Rudolf OS" entry (BootCurrent)', up and mine and cur == mine, [cur, mine])
    ok('...the same system, files and drive', st.get('slot') == 'a' and st.get('who') == 'internal' and st.get('media') == '/dev/nvme0n1')
    post = last(sh(f'tail -c +5 {E}/Boot{mine}-{G} 2>/dev/null | od -An -tx1 -v | tr -d " \\n"', 20)) if mine else ''
    if pre != post: out('NOTE the firmware changed the entry: before', pre[:400], 'after', post[:400])
    ok('...and rudolfd wrote nothing to the firmware this time (all was already so)', last(sh('grep -c "boot entry Boot\\|first in the boot order\\|removed the old boot entry" /run/rudolf/rudolfd.log', 20)) == '0', sh('cat /run/rudolf/rudolfd.log', 20)[-300:])
    # 3b. (1.4.2) firmware that will not start our entry (the Vivobook's AMI skipped it and started the entry made by
    #     hand in its setup): an entry naming the drive and a file that is not there stands in for it. The firmware
    #     falls through to its own entry for the drive; rudolfd takes that entry's path, and the next start uses ours.
    if mine and fwp != 'none':
        fn = '\\EFI\\NOPE.EFI'.encode('utf-16-le') + b'\0\0'
        bad = bytes.fromhex(fwp[:-8]) + bytes([4, 4, 4 + len(fn), 0]) + fn + bytes.fromhex('7fff0400')
        opt = struct.pack('<IIH', 7, 1, len(bad)) + 'Rudolf OS'.encode('utf-16-le') + b'\0\0' + bad
        oct_ = ''.join('\\%03o' % b for b in opt)
        sh(f'f={E}/Boot{mine}-{G}; chattr -i $f 2>/dev/null; rm -f $f; printf "{oct_}" > $f; sync; reboot -f', 10)
        time.sleep(5)
        up = wait_shell(delay=40)
        cur = efi('BootCurrent'); ent = last(sh(f'tail -c +5 {E}/Boot{mine}-{G} 2>/dev/null | od -An -tx1 -v | tr -d " \\n"', 20))
        ok('3b. firmware that skips our entry: the start came through its own entry for the drive', up and fwn and cur == fwn.group(1), [cur, mine])
        ok('...and rudolfd gave ours that entry\'s path', ent.endswith(fwp) and 'NOPE' not in ent and 'started from' in sh('cat /run/rudolf/rudolfd.log', 20), [ent[:200], sh('cat /run/rudolf/rudolfd.log', 20)[-300:]])
        sh('sync; reboot -f', 10)
        time.sleep(5)
        up = wait_shell(delay=40); cur = efi('BootCurrent')
        ok('...so the next start came through ours, and rudolfd left it as it was', up and cur == mine and last(sh('grep -c "boot entry Boot" /run/rudolf/rudolfd.log', 20)) == '0', [cur, mine, sh('cat /run/rudolf/rudolfd.log', 20)[-300:]])
    # 4. a rollback to slot b (1.4.0): its start-up knows nothing of the installed layout
    sh('printf "good=a\\nprev=b\\nnext=b\\ntries=2\\nfailed=\\n" > /persist/os/system/state; sync; reboot -f', 10)
    time.sleep(5)
    up = wait_shell(delay=40); st = state()
    PV = tuple(int(x) for x in mv(PREV, 'version').split('.')[:3])
    ok(f'4. an older slot ({".".join(map(str, PV))}) starts from the converted drive, with its files (rudolf.persist=)', up and st.get('slot') == 'b' and st.get('persist') == '/dev/nvme0n1p3' and st.get('who') == 'internal', st)
    if PV < (1, 4, 1):
        ok('...without mounting the whole drive (no media: it does not know rudolf.mediadisk=), so RUDOLFDATA stays usable', st.get('media', 'x') == '' and last(sh('touch /persist/t141-w && echo writable', 20)) == 'writable', [st.get('media'), st.get('mnt')])
    else:
        ok('...reading the image from the whole drive through a loop device, RUDOLFDATA still usable', st.get('media') == '/dev/nvme0n1' and st.get('mnt', '').startswith('/dev/loop') and last(sh('touch /persist/t141-w && echo writable', 20)) == 'writable', [st.get('media'), st.get('mnt')])
finally:
    vm.stop()
os.remove(DRV)
out('%d/%d passed' % (sum(R), len(R)))
