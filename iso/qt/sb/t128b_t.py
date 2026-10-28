# Rudolf OS 1.2.8 stage C in QEMU: new boot files for a drive installed from a disc before Secure Boot support,
# keeping its files (item 17; UEFI, Secure Boot off).
#   python3 t128b_t.py OLD.iso REL-DIR BOOT.iso DISC.iso
# OLD.iso: the published 1.2.0 disc image (446 MiB; no Secure Boot boot files).
# REL-DIR: a 1.2.8 test release (manifest.txt/.sig and its five files, test keys); BOOT.iso: its boot image and
# DISC.iso: its test disc (mkdisc.sh with RUDOLF_UKI_TEST_CMDLINE=rudolf.debug=1, which the boot image has too).
# The laptop's drive is made as Install here made it from the 1.2.0 disc: the image, then RUDOLFDATA at 446 MiB,
# holding the 1.2.8 update in slot a (the system that runs), an older slot b that still takes layers from the old
# image, a file in the home folder and the boot image in Downloads.
#   1. Started from that drive (1.2.0's kernel, then the update's): Settings' Get new boot files keeps a copy;
#      slot b, which cannot start without the old image, is no longer offered to go back to
#   2. the next start writes the boot image in front of RUDOLFDATA, compares it and restarts
#   3. the drive alone starts from its new boot files (shim, the menu, the signed kernel) and the installed update;
#      the file is still there; rudolfd says the boot files start with Secure Boot
#   4. Install here > Replace the boot files only, from the 1.2.8 disc, for a second such drive: a whole disc image
#      that does not fit is refused; the boot image is written, and that drive's files are as they were
import os, sys, time, json, re, subprocess, shutil, hashlib, struct
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT); sys.path.insert(0, os.path.join(QT, '..', 'systools'))
import sbvm, iso_extract
from guest import Guest
from ser import Ser
OLD, REL, BOOT, DISC = (os.path.abspath(x) for x in sys.argv[1:5])
# the release under test: its version as written in REL's manifest (1.2.8 when this test was made)
VER = next('.'.join(l.split()[1].split('.')[:3]) for l in open(f'{REL}/manifest.txt') if l.startswith('version '))
PARTS = os.environ['SB_PARTS']; TPUB = open(os.environ['TEST_RPK_PUB']).read().strip() if os.environ.get('TEST_RPK_PUB') else ''; LOG = open('t128b.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:300] if d != '' else ''))
def last(t): return t.strip().split('\n')[-1].strip() if t.strip() else ''
def J(v, d):
    try: return json.loads(v) if isinstance(v, str) else d
    except ValueError: return d
def sha(f):
    h = hashlib.sha256()
    with open(f, 'rb') as x:
        for b in iter(lambda: x.read(1 << 20), b''): h.update(b)
    return h.hexdigest()
BSHA = sha(BOOT); BBYTES = os.path.getsize(BOOT)
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
# 1.2.0's kernel, to start the old drive with a serial shell (its own menu has no debug entry)
K120 = os.path.abspath('k120'); shutil.rmtree(K120, ignore_errors=True)
with open(OLD, 'rb') as f:
    lba, size = iso_extract.find(f, 'BOOT/VMLINUZ'); f.seek(lba * 2048); os.makedirs(K120); open(f'{K120}/vmlinuz', 'wb').write(f.read(size))
    lba, size = iso_extract.find(f, 'RUDOLF/ID'); f.seek(lba * 2048); OLDID = f.read(size)
OSIZE = os.path.getsize(OLD); START = (OSIZE + 1048575) // 1048576 * 2048
out('old image', OSIZE // 1048576, 'MiB; RUDOLFDATA at sector', START, '; boot image', BBYTES // 1048576, 'MiB')
def make_drive(path, with_boot_in_downloads):
    """a drive as Install here made it from OLD, with the 1.2.8 update installed in slot a"""
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True)
    a = f'{T}/os/system/a'; b = f'{T}/os/system/b'; os.makedirs(a); os.makedirs(b)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs', 'manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{a}/{n}')
    rel = f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n"
    open(f'{a}/RELEASE', 'w').write(rel); open(f'{a}/complete', 'w').write('1\n')
    # slot b: an older update that took its desktop layer from the old disc image (a hash that image does not have)
    for n in ('vmlinuz', 'base.sfs', 'manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{b}/{n}')
    open(f'{b}/RELEASE', 'w').write(rel); open(f'{b}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=b\nnext=\ntries=0\nfailed=\n')
    os.makedirs(f'{T}/home/rudolf/Downloads'); os.makedirs(f'{T}/etc')
    open(f'{T}/home/rudolf/keepme.txt', 'w').write('my homework\n')
    if with_boot_in_downloads: shutil.copy(BOOT, f'{T}/home/rudolf/Downloads/rudolf-os-1.2.8-boot.iso')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    D = path + '.data'
    if os.path.exists(D): os.remove(D)
    subprocess.run(['truncate', '-s', '1600M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    if os.path.exists(path): os.remove(path)
    total = START + 1600 * 2048 + 2048
    with open(path, 'wb') as f:
        with open(OLD, 'rb') as o: shutil.copyfileobj(o, f)
        f.truncate(total * 512)
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', START, 1600 * 2048))
        f.seek(START * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)
def debugfs(path, cmd):
    """read RUDOLFDATA of a drive image on the host"""
    D = path + '.p3'
    subprocess.run(['dd', f'if={path}', f'of={D}', 'bs=1M', f'skip={START // 2048}', 'count=1600', 'conv=sparse', 'status=none'], check=True)
    r = subprocess.run(['debugfs', '-R', cmd, D], capture_output=True, text=True).stdout
    fsck = subprocess.run(['e2fsck', '-fn', D], capture_output=True, text=True).returncode
    os.remove(D); return r, fsck
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
def jsf(g):
    def js(e, t=60):
        for _ in range(3):
            r = g.js(e, t)
            if not (isinstance(r, tuple) and (r[0] == 'ERR' or (r[0] == 'EXC' and 'ReferenceError' in str(r[1])))): return r
            time.sleep(15)
        return r
    return js
def page(g, js):
    end = time.time() + 600
    while time.time() < end and g.js('typeof bootFilesDlg=="function"&&typeof rdPost=="function"', 20) is not True: time.sleep(5)
    js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();svj('bd95setup',1);const b=[...document.querySelectorAll('button')].find(b=>b.textContent.trim()=='Guest');if(b)b.click();return 1})()")
    time.sleep(3); js("USER.r='admin';window.elevate=async()=>true;1")
def job(js, limit=1200):
    j = {}; end = time.time() + limit
    while time.time() < end:
        j = J(js("rd('job').then(j=>JSON.stringify(j))"), {})
        if not j.get('running') and j.get('status', -1) >= 0: break
        time.sleep(8)
    return j
def wait_exit(vm, limit):
    end = time.time() + limit
    while time.time() < end and vm.p.poll() is None: time.sleep(3)
    return vm.p.poll() is not None
OLDK = ['-kernel', f'{K120}/vmlinuz', '-append', 'rudolf.debug=1', '-no-reboot']
DRIVE = os.path.abspath('laptop128b.img'); make_drive(DRIVE, True)

# ---- 1. started from the old drive: the update runs; Get new boot files
vm = sbvm.VM('t128b1', PARTS, [DRIVE], secure=False, mem=3072, extra=OLDK); g = session(vm); sh = lambda c, t=60: g.sh(c, t); js = jsf(g)
try:
    up = wait_shell(g)
    ok('1. the old drive starts (1.2.0\'s kernel, then the installed 1.2.8 update in slot a)', up and last(sh('cat /etc/rudolf/slot', 20)) == 'a' and last(sh('cat /etc/rudolf/persist', 20)) == '/dev/sda3', sh('cat /etc/rudolf/slot /etc/rudolf/media /etc/rudolf/persist', 20)[-120:])
    # 1.2.8: started by kexec (as every computer running an installed update), the start-up options were mounted over
    # /proc/cmdline (1.2.3-1.2.7), so the kernel refused a new /proc to the Debian sandbox and Flatpak
    o = sh('grep -c " /proc/cmdline " /proc/self/mountinfo', 20)
    ok('...nothing is mounted over /proc/cmdline (1.2.7 mounted the start-up options there after a kexec)', last(o) == '0', o[-100:])
    o = sh('/usr/bin/rudolf-fscheck | grep proc_mount', 60)
    ok('...so a sandbox may mount its own /proc (the Debian sandbox: "mounting /proc failed (Operation not permitted)")', last(o) == 'proc_mount=ok', o[-200:])
    ok('...and the start-up options, with the debug option, are in /etc/rudolf/cmdline', 'rudolf.debug=1' in sh('cat /etc/rudolf/cmdline', 20) and 'rudolf.slot=a' in sh('cat /etc/rudolf/cmdline', 20))
    page(g, js)
    # the test release is signed with the test key, which this test system's update tool does not trust by itself
    if TPUB: sh(f'echo {TPUB} >> /etc/rudolf/rpk-keys; echo added', 20)
    info = J(js("rd('info').then(i=>JSON.stringify(i))"), {})
    ok('...rudolfd says its boot files predate Secure Boot (bootSecure 0), nothing staged', info.get('bootSecure') == 0 and info.get('bootFiles', {}).get('pending') is False, [info.get('bootSecure'), info.get('bootFiles')])
    ims = J(js("rd('install/images').then(r=>JSON.stringify(r.images))"), [])
    ok('...the downloaded boot image is listed as one', any(x.get('boot') and x['name'] == 'rudolf-os-1.2.8-boot.iso' for x in ims), ims)
    # a whole disc image would not fit: tried with this system's own (the 1.2.0 image itself) and refused on its age
    r = js(f"rdPost('bootfiles?image=/home/rudolf/Downloads/rudolf-os-1.2.8-boot.iso&sha={'0' * 64}').then(r=>r.message,e=>e.message)")
    j = job(js); log = j.get('log') or ''
    ok('a boot image that does not match its .sha256 is refused, nothing kept', j.get('status') == 8 and 'does not match its .sha256' in log and last(sh('ls /persist/os/bootimg/ 2>/dev/null | wc -l', 20)) == '0', log[-200:])
    r = js(f"rdPost('bootfiles?image=/home/rudolf/Downloads/rudolf-os-1.2.8-boot.iso&sha={BSHA}').then(r=>r.message,e=>e.message)")
    j = job(js); log = j.get('log') or ''; out(log[-900:])
    ok('Get new boot files: the image checked (signature, Secure Boot boot files, fit) and kept for the next start', j.get('status') == 0 and 'boot files only' in log and f'Ready: the new boot files (Rudolf OS {VER})' in log, log[-300:])
    ok('...slot b, which took a layer from the old image, is no longer offered to go back to', 'Slot b (the previous system) is no longer kept' in log and last(sh('grep -c "^prev=$" /persist/os/system/state', 20)) == '1', sh('cat /persist/os/system/state', 20)[-120:])
    p = sh('cat /persist/os/bootimg/pending', 20)
    ok('...pending: its SHA-256, the RUDOLFDATA start and size', f'sha={BSHA}' in p and f'start={START}' in p and f'blocks={BBYTES // 2048}' in p, p[-200:])
    info = J(js("rd('info').then(i=>JSON.stringify(i))"), {})
    ok('...and rudolfd says so (Settings shows "written the next time the computer starts")', info.get('bootFiles', {}).get('pending') is True)
    try: g.js("rdPost('power?action=reboot').then(()=>1,()=>0)", 20)
    except OSError: pass   # QEMU (-no-reboot) may have ended before the answer came back
    ok('...restart', wait_exit(vm, 180))
finally:
    vm.stop()

# ---- 2. the next start writes them
vm = sbvm.VM('t128b2', PARTS, [DRIVE], secure=False, mem=3072, extra=OLDK)
try:
    time.sleep(30)
    try: vm.shot('writing')
    except OSError: pass   # it may have finished and restarted already
    ok('2. the next start writes the new boot files and restarts by itself', wait_exit(vm, 900))
finally:
    vm.stop()
with open(DRIVE, 'rb') as f:
    head = f.read(BBYTES); mbr = head[:512]
img = open(BOOT, 'rb').read()
# 1.4.1: an internal drive also gets the installed layout (layout.sh): partition 1 cleared, a disk signature of its own
ok('...the boot image is at the start of the drive, byte for byte (but the disk signature and partitions 1 and 3)', head[:440] + head[444:446] + head[462:478] + head[494:] == img[:440] + img[444:446] + img[462:478] + img[494:])
ok('...with the installed layout: partition 1 cleared, a disk signature of its own', head[446:462] == bytes(16) and head[440:444] not in (img[440:444], bytes(4)), [head[440:462].hex()])
ok('...RUDOLFDATA\'s partition entry is as it was', mbr[478:494] == bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', START, 1600 * 2048))
r, fsck = debugfs(DRIVE, 'cat /os/bootimg/result')
ok('...RUDOLFDATA is intact, and the start-up noted what it wrote', fsck == 0 and 'written' in r and VER in r, [fsck, r])

# ---- 3. the drive alone, from its new boot files
vm = sbvm.VM('t128b3', PARTS, [DRIVE], secure=False, mem=3072); g = session(vm); sh = lambda c, t=60: g.sh(c, t); js = jsf(g)
try:
    up = wait_shell(g)
    ok('3. the drive starts from its new boot files (shim, the menu, the signed kernel) and the installed update', up and last(sh('cat /etc/rudolf/slot', 20)) == 'a' and last(sh('[ -e /live/media/rudolf/bootonly ] && echo bootonly', 20)) == 'bootonly', sh('cat /etc/rudolf/slot; ls /live/media/rudolf/', 20)[-200:])
    ok('...the file in the home folder is still there', last(sh('cat /home/rudolf/keepme.txt', 20)) == 'my homework')
    o = sh('/usr/bin/rudolf-fscheck | grep proc_mount', 60)
    ok('...a sandbox may mount its own /proc here too', last(o) == 'proc_mount=ok', o[-200:])
    page(g, js)
    info = J(js("rd('info').then(i=>JSON.stringify(i))"), {})
    ok('...rudolfd says the boot files start with Secure Boot now, and what was written', info.get('bootSecure') == 1 and (info.get('bootFiles', {}).get('result') or '').startswith('written'), [info.get('bootSecure'), info.get('bootFiles')])
    t = ''
    for _ in range(4):
        t = js("(async()=>{wins.slice().forEach(w=>w.close());settings('sec');await new Promise(r=>setTimeout(r,6000));return document.querySelector('.sb28')?.innerText||''})()", 90)
        if isinstance(t, str) and 'Off' in t: break
        time.sleep(10)
    ok('...and Settings offers Prepare Secure Boot (no more "made before Secure Boot")', isinstance(t, str) and 'Prepare Secure Boot' in t and 'made before Rudolf OS supported Secure Boot' not in t, t)
finally:
    vm.stop()

os.remove(DRIVE)
# ---- 4. Install here > Replace the boot files only, from the 1.2.8 disc, for a second old drive
SECOND = os.path.abspath('second128b.img'); make_drive(SECOND, False)
B = os.path.abspath('bootdisk128b'); shutil.rmtree(B, ignore_errors=True); os.makedirs(B); shutil.copy(BOOT, f'{B}/rudolf-os-1.2.8-boot.iso')
BD = os.path.abspath('bootdisk128b.img')
if os.path.exists(BD): os.remove(BD)
subprocess.run(['truncate', '-s', '96M', BD], check=True); subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'RUDOLFB', '-d', B, BD], check=True); shutil.rmtree(B)
# (the second drive is plugged in after the start: attached from the start, its RUDOLFDATA would be the disc's /persist)
vm = sbvm.VM('t128b4', PARTS, [DISC, BD], secure=False, mem=3072, extra=['-device', 'qemu-xhci,id=xh', '-drive', f'file={SECOND},format=raw,if=none,id=hp']); g = session(vm); sh = lambda c, t=60: g.sh(c, t); js = jsf(g)
try:
    up = wait_shell(g)
    page(g, js)
    if TPUB: sh(f'echo {TPUB} >> /etc/rudolf/rpk-keys; echo added', 20)
    out(g.q.cmd('device_add', driver='usb-storage', bus='xh.0', drive='hp', id='hpdev')); time.sleep(15)
    ok('4. the 1.2.8 disc starts, and the second drive is plugged in', up and last(sh('findfs LABEL=RUDOLFDATA', 20)).startswith('/dev/sd'))
    dk = J(js("rd('disks').then(r=>JSON.stringify(r.disks.filter(d=>d.partitions.some(p=>p.label=='RUDOLFDATA')).map(d=>d.name)))"), [])
    dev = dk[0] if dk else 'sdz'
    js(f"rdPost('install?disk={dev}&confirm={dev}&bootonly=1').then(r=>r.message,e=>e.message)")
    j = job(js); log = j.get('log') or ''
    ok('a whole disc image (this disc\'s own) does not fit in front of that drive\'s files: refused, the boot image named', j.get('status') == 5 and 'does not fit in front of your files' in log and f'rudolf-os-{VER}-boot.iso' in log, log[-300:])
    nd = last(sh('findfs LABEL=RUDOLFB', 20)); sh(f'mkdir -p /mnt/b; mount -o ro {nd} /mnt/b; mkdir -p /home/rudolf/Downloads; cp /mnt/b/*.iso /home/rudolf/Downloads/; echo copied', 120)
    js(f"rdPost('install?disk={dev}&confirm={dev}&bootonly=1&image=/home/rudolf/Downloads/rudolf-os-1.2.8-boot.iso&sha={BSHA}').then(r=>r.message,e=>e.message)")
    j = job(js); log = j.get('log') or ''; out(log[-600:])
    ok('Replace the boot files only: written and compared, the drive\'s files kept', j.get('status') == 0 and 'The copy matches' in log and 'its files are as they were' in log, log[-300:])
finally:
    vm.stop()
with open(SECOND, 'rb') as f: head = f.read(BBYTES)
r, fsck = debugfs(SECOND, 'cat /home/rudolf/keepme.txt')
ok('...on the host: the boot image is there, RUDOLFDATA intact with the file in it', head[:478] + head[494:] == img[:478] + img[494:] and head[478:494] == bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', START, 1600 * 2048) and fsck == 0 and 'my homework' in r, [fsck, r])
for f in (SECOND, BD): os.remove(f)
shutil.rmtree(K120)
out('%d/%d passed' % (sum(R), len(R)))
