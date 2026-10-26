# Rudolf OS 1.2.7 stage B in QEMU: Make a Rudolf OS USB stick from a disc image downloaded in parts (UEFI, Secure Boot
# off), as on a laptop where joining the parts by hand was not possible (its Linux terminal did not start).
#   python3 t127c_t.py DISC.iso IMAGE.iso
# DISC.iso: a 1.2.7 test disc (mkdisc.sh with RUDOLF_UKI_TEST_CMDLINE=rudolf.debug=1: a serial shell).
# IMAGE.iso: a published Rudolf OS disc image (disc-1.2.6), split here into parts the way the release is (30408704
# bytes each), with its .sha256. The parts go to Downloads on the disk, the .sha256 to C:\DOWNLOADS (browser storage),
# as Oracle saves them.
#   - rudolfd install/images lists the set as not joined yet; info.bootSecure is 1 on the 1.2.7 disc
#   - with one part damaged: the join stops ("does not match its .sha256"), nothing is written to the stick
#   - with good parts: joined, checked, the stick is made; a computer with only the stick starts from it, with its
#     Secure Boot files (EFI/rudolf/rudolf.cer) and the image's version
import os, sys, time, json, re, subprocess, shutil, hashlib
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT); sys.path.insert(0, os.path.join(QT, '..', 'systools'))
import sbvm, iso_extract
from guest import Guest
from ser import Ser
ISO, IMG = (os.path.abspath(x) for x in sys.argv[1:3])
PARTS = os.environ['SB_PARTS']; LOG = open('t127c.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:300] if d != '' else ''))
def last(t): return t.strip().split('\n')[-1].strip() if t.strip() else ''
def J(v, d):
    try: return json.loads(v) if isinstance(v, str) else d
    except ValueError: return d
NAME = os.path.basename(IMG); VER = re.search(r'rudolf-os-([\d.]+)\.iso', NAME).group(1)
# the parts, on a test disk (RUDOLFB), with a copy script; the image's checksum
P = os.path.abspath('parts127c'); shutil.rmtree(P, ignore_errors=True); os.makedirs(P)
h = hashlib.sha256(); n = 0
with open(IMG, 'rb') as f:
    while True:
        b = f.read(30408704)
        if not b: break
        h.update(b); open(f'{P}/{NAME}.part{n:02d}', 'wb').write(b); n += 1
SHA = h.hexdigest(); out('image', NAME, 'parts', n, 'sha256', SHA[:16])
open(f'{P}/get.sh', 'w').write('#!/bin/sh\nmkdir -p /home/rudolf/Downloads; cp /mnt/p/*.part* /home/rudolf/Downloads/; chown -R 1000:1000 /home/rudolf/Downloads; ls /home/rudolf/Downloads | grep -c part\n')
os.chmod(f'{P}/get.sh', 0o755)
PD = os.path.abspath('parts127c.img')
if os.path.exists(PD): os.remove(PD)
subprocess.run(['truncate', '-s', str((os.path.getsize(IMG) // 1048576 + 64) * 1048576), PD], check=True)
subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'RUDOLFB', '-d', P, PD], check=True); shutil.rmtree(P)
DATA = os.path.abspath('data127c.img'); STICK = os.path.abspath('stick127c.img')
for f in (DATA, STICK):
    if os.path.exists(f): os.remove(f)
subprocess.run(['truncate', '-s', '3G', DATA], check=True); subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'RUDOLFDATA', DATA], check=True)
subprocess.run(['truncate', '-s', '4G', STICK], check=True)
with open(STICK, 'r+b') as f: f.write(b'OLD DATA ' * 1000)
def usb(i, f, boot=None):
    return ['-drive', f'file={f},format=raw,if=none,id=u{i}', '-device', f'usb-storage,bus=xh.0,drive=u{i},removable=on' + (f',bootindex={boot}' if boot is not None else '')]
def session(vm):
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3); return g
def wait_shell(g, delay=45):
    time.sleep(delay); end = time.time() + 900
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): return True
        except OSError: pass
        time.sleep(4)
    return False

vm = sbvm.VM('t127c1', PARTS, [ISO, DATA], secure=False, mem=3072, extra=['-device', 'qemu-xhci,id=xh'] + usb(0, STICK) + usb(1, PD))
g = session(vm); sh = lambda c, t=60: g.sh(c, t)
def js(e, t=60):
    for _ in range(3):
        r = g.js(e, t)
        if not (isinstance(r, tuple) and (r[0] == 'ERR' or (r[0] == 'EXC' and 'ReferenceError' in str(r[1])))): return r
        time.sleep(15)
    return r
def until(e, t=120):
    end = time.time() + t
    while time.time() < end:
        if g.js(e, 20) is True: return True
        time.sleep(3)
    return False
def job(limit=1800):
    j = {}; end = time.time() + limit
    while time.time() < end:
        j = J(js("rd('job').then(j=>JSON.stringify(j))"), {})
        if not j.get('running') and j.get('status', -1) >= 0: break
        time.sleep(10)
    return j
try:
    ok('starts from the 1.2.7 disc, keeping files', wait_shell(g) and last(sh('mountpoint -q /persist && echo kept', 20)) == 'kept')
    for _ in range(30):
        nd = last(sh('findfs LABEL=RUDOLFB', 10))
        if nd.startswith('/dev/'): break
        time.sleep(3)
    sh('mkdir -p /mnt/p', 10); sh(f'mount -o ro {nd} /mnt/p 2>/dev/null || mount --bind /media/{nd[5:]} /mnt/p', 20)
    ok(f'the {n} parts are in Downloads on the disk', last(sh('sh /mnt/p/get.sh', 600)) == str(n))
    until("typeof stkSection=='function'&&typeof imgSha=='function'", 600)
    js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();svj('bd95setup',1);const b=[...document.querySelectorAll('button')].find(b=>b.textContent.trim()=='Guest');if(b)b.click();return 1})()")
    until('!!USER', 120); js("USER.r='admin';window.elevate=async()=>true;1")
    # the .sha256 as Oracle keeps a file that small: in C:\DOWNLOADS (browser storage), in capitals
    js(f"(()=>{{const D=node(downloadsPath());D[{json.dumps((NAME + '.sha256').upper())}]='data:application/octet-stream;base64,'+btoa({json.dumps(SHA + '  ' + NAME + chr(10))});fsSave();return 1}})()")
    info = J(js("rd('info').then(i=>JSON.stringify(i))"), {})
    ok('rudolfd says this disc\'s boot files start with Secure Boot (bootSecure 1)', info.get('bootSecure') == 1, info.get('bootSecure'))
    ims = J(js("rd('install/images').then(i=>JSON.stringify(i.images))"), [])
    st = [x for x in ims if x.get('parts')]
    ok('install/images lists the parts as one image, not joined yet', len(st) == 1 and st[0]['parts'] == n and st[0]['name'] == NAME and st[0]['path'] == '/home/rudolf/Downloads/' + NAME, ims)
    path = st[0]['path'] if st else '/home/rudolf/Downloads/' + NAME
    ok('...and the page finds its SHA-256 in C:\\DOWNLOADS', js(f"imgSha({json.dumps(path)})") == SHA)
    t = J(js("rd('info').then(async i=>JSON.stringify(stkTargets((await rd('disks')).disks,i).map(k=>k.name)))"), [])
    dev = t[0] if t else 'sdz'
    # a damaged part: refused, nothing written
    sh(f'cp /home/rudolf/Downloads/{NAME}.part03 /tmp/p03; printf X | dd of=/home/rudolf/Downloads/{NAME}.part03 bs=1 seek=4096 conv=notrunc 2>/dev/null; echo done', 30)
    r = js(f"rdPost('mkstick?disk={dev}&confirm={dev}&image='+encodeURIComponent({json.dumps(path)})+'&files=0&sha='+imgSha({json.dumps(path)})).then(r=>r.message,e=>e.message)")
    j = job(); log = j.get('log') or ''; out(log[-500:])
    ok('with one part damaged, the join stops: it does not match the .sha256', j.get('status', 0) != 0 and 'does not match its .sha256' in log, log[-300:])
    ok('...and nothing is written to the stick (and no half image is left)', 'Step 1/5' not in log and last(sh(f'head -c 8 /dev/{dev}', 20)).startswith('OLD DATA') and last(sh(f'ls /home/rudolf/Downloads/ | grep -c "{NAME}$"', 20)) == '0', sh(f'head -c 8 /dev/{dev}', 20))
    sh(f'cp /tmp/p03 /home/rudolf/Downloads/{NAME}.part03; chown 1000:1000 /home/rudolf/Downloads/{NAME}.part03; echo restored', 30)
    r = js(f"rdPost('mkstick?disk={dev}&confirm={dev}&image='+encodeURIComponent({json.dumps(path)})+'&files=0&sha='+imgSha({json.dumps(path)})).then(r=>r.message,e=>e.message)")
    j = job(); log = j.get('log') or ''; out(log[-800:])
    ok(f'with good parts: joined ({n} parts), checked against the .sha256, and the stick is made', j.get('status') == 0 and f'Joining {n} parts' in log and 'matches its .sha256' in log and 'stick is ready' in log, log[-300:])
    ok('...the joined image is kept in Downloads for next time', last(sh(f'stat -c %s /home/rudolf/Downloads/{NAME}', 20)) == str(os.path.getsize(IMG)))
finally:
    vm.stop()

vm = sbvm.VM('t127c2', PARTS, [], secure=False, mem=3072, extra=['-device', 'qemu-xhci,id=xh'] + usb(0, STICK, boot=0))
g = session(vm); sh = lambda c, t=60: g.sh(c, t)
try:
    # a published disc has no debug start: the stick is checked from the outside (its boot menu starts in QEMU)
    time.sleep(90); vm.shot('stick')
finally:
    vm.stop()
with open(STICK, 'rb') as f:
    head = f.read(os.path.getsize(IMG))
# install.sh adds RUDOLFDATA as the third entry of the partition table (16 bytes at 478): the rest is the image
img = open(IMG, 'rb').read()
ok('the stick holds the downloaded image, byte for byte (but its RUDOLFDATA partition entry)', head[:478] + head[494:] == img[:478] + img[494:] and head[478:494] != img[478:494])
del img
with open('stickimg127c.iso', 'wb') as f: f.write(head)
with open('stickimg127c.iso', 'rb') as f:
    lba, size = iso_extract.find(f, 'EFI/EFIBOOT.IMG'); f.seek(lba * 2048); efi = f.read(size)
    lba, size = iso_extract.find(f, 'RUDOLF/RELEASE'); f.seek(lba * 2048); rel = f.read(size).decode()
os.remove('stickimg127c.iso')
ok(f'...with Secure Boot boot files (EFI/rudolf/rudolf.cer, grubx64.efi) and version {VER}', b'RUDOLF  CER' in efi and b'GRUBX64 EFI' in efi and f'version {VER}' in rel, rel[:80])
out('%d/%d passed' % (sum(R), len(R)))
