# Rudolf OS 1.6.2 (docs/plan-1.6.2.md Part 27) in QEMU: Bring your files on the real release (the newest beta, or the one
# given), with this tree's rudolfd, rudolf-rootd, root helpers, bringfiles.sh and install.sh put in place (BRING_FIX: a
# folder with rudolfd, rudolf-rootd, root/NAME and lib/NAME, on an extra disk; fix161.sh starts rudolfd again through
# rudolf-rootd as rudolf-init will). A second NVMe disk is the "old computer": Windows on NTFS (a Users tree: Ann with
# OneDrive, Bob, Public and Default; AppData, programs) and a Linux on ext4 with /home; a USB stick (FAT32, QEMU's
# usb-storage) is where the files go. Through rudolfd's API as the page calls it: found and sized (the kernel's ntfs3,
# mounted read-only, without programs); the installation refused before the copy; a cancelled copy marking nothing;
# copied and checked; the old disk unchanged (its SHA-256); an account's folder opened and the files brought into it
# (Documents into Documents, the rest into Documents/From the old computer, as the account's user); then the
# installation going ahead (past the guard).   python3 bring62_q.py REL-DIR   (SB_PARTS and QEMU as for r161; root)
# 1.6.2 (Part 27's BitLocker): BRING_KERNEL, a folder with a kernel built from this tree (iso/systools/testkernel.sh: vmlinuz and
# modules.sfs, dm-crypt built in) started in place of the release's; BRING_BITLK, cryptsetup's BitLocker test images (its
# tests/bitlk-images: made by Windows; cryptsetup cannot make one), three of them as the partitions of a third disk (AES-XTS,
# AES-CBC, AES-CBC with the Elephant diffuser), opened read-only through rudolfd's bring/unlock with their recovery keys; and
# cryptsetup with the libraries the release's base lacks in BRING_FIX/cs (as ci_release.sh adds it to the base layer).
import os, sys, time, re, subprocess, shutil, struct, json
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); FIX = os.environ.get('BRING_FIX', '')
KERN = os.environ.get('BRING_KERNEL', ''); BITLK = os.environ.get('BRING_BITLK', '')   # (Part 27's BitLocker)
KDIR = os.path.abspath(KERN) if KERN else REL
LOG = open('bring62.txt', 'w'); R = []; T0 = time.time()
def out(*a):
    s = ' '.join(str(x) for x in a); s = f'[{int(time.time() - T0):5d}s] ' + s; print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:1800] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 3000; DRV = os.path.abspath('b62.img'); MEM = int(os.environ.get('MEM', '3072')); W = os.path.abspath('w62')
shutil.rmtree(W, ignore_errors=True); os.makedirs(W)
def sh_(*a, **k): return subprocess.run(list(a), check=True, **k)
def mbr(path, parts):   # parts: [(start, sectors, type, data file)]
    with open(path, 'r+b') as f:
        for i, (st, n, ty, data) in enumerate(parts):
            f.seek(446 + 16 * i); f.write(bytes([0, 0xfe, 0xff, 0xff, ty, 0xfe, 0xff, 0xff]) + struct.pack('<II', st, n))
            if data:
                f.seek(st * 512)
                with open(data, 'rb') as d: shutil.copyfileobj(d, f)
                os.remove(data)
        f.seek(510); f.write(b'\x55\xaa')
def make_drive(path):
    with open(path, 'wb') as f: f.truncate((2048 + DMB * 2048 + 2048) * 512)
    T = path + '.tree'; shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    d = f'{T}/os/system/a'; os.makedirs(d)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs', 'manifest.txt', 'manifest.sig'): shutil.copy(f'{KDIR if n in ("vmlinuz", "modules.sfs") else REL}/{n}', f'{d}/{n}')
    open(f'{d}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n"); open(f'{d}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    sh_('chown', '-R', '1000:1000', f'{T}/home/rudolf')
    sh_('truncate', '-s', f'{DMB}M', path + '.data'); sh_('mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, path + '.data'); shutil.rmtree(T)
    mbr(path, [(2048, DMB * 2048, 0x83, path + '.data')])
# the old computer: Windows (NTFS, 300 MB) and Linux (ext4, 100 MB) on a 3 GB disk
OLD = os.path.abspath('b62old.img'); NT = OLD + '.ntfs'; WM = os.path.join(W, 'wm')
with open(OLD, 'wb') as f: f.truncate(1536 * 1024 ** 2)
sh_('truncate', '-s', '600M', NT); sh_('mkntfs', '-F', '-Q', '-q', '-L', 'Windows', NT, stdout=subprocess.DEVNULL); os.makedirs(WM); sh_('ntfs-3g', NT, WM)
for p in ('Windows/System32', 'Program Files/App', 'Users/Ann/Documents/Taxes', 'Users/Ann/Pictures/Trip', 'Users/Ann/Desktop', 'Users/Ann/AppData/Local',
          'Users/Ann/OneDrive/Documents', 'Users/Ann/Saved Games', 'Users/Bob/Videos', 'Users/Bob/Movies', 'Users/Public/Documents', 'Users/Default/Documents'): os.makedirs(f'{WM}/{p}')
def wr(p, b): open(f'{WM}/{p}', 'wb').write(b)
wr('Windows/System32/kernel32.dll', b'k'); wr('Program Files/App/app.exe', b'MZ'); wr('Users/Ann/AppData/Local/x.txt', b't'); wr('Users/Ann/Documents/setup.exe', b'MZ')
wr('Users/Ann/Documents/Letter to Mary.docx', b'letter'); wr('Users/Ann/Documents/Taxes/return.pdf', b'%PDF taxes'); wr('Users/Ann/Desktop/todo.txt', b'todo')
for i in range(1, 41): wr(f'Users/Ann/Pictures/Trip/IMG_{i:04d}.JPG', os.urandom(400000))
wr('Users/Ann/OneDrive/Documents/budget.xlsx', b'budget'); wr('Users/Ann/Saved Games/save1.sav', b'save'); wr('Users/Ann/NTUSER.DAT', b'reg')
wr('Users/Bob/Videos/birthday.mp4', os.urandom(3000000)); wr('Users/Public/Documents/shared.txt', b'p'); wr('Users/Bob/Videos/long.mp4', os.urandom(200 * 1024 ** 2)); wr('Users/Default/Documents/d.txt', b'd')
sh_('umount', WM)
LT = os.path.join(W, 'lin'); os.makedirs(f'{LT}/etc'); os.makedirs(f'{LT}/home/carol/Documents'); os.makedirs(f'{LT}/home/carol/Pictures')
open(f'{LT}/etc/os-release', 'w').write('PRETTY_NAME="Debian GNU/Linux 12 (bookworm)"\n'); open(f'{LT}/home/carol/Documents/notes.odt', 'w').write('notes')
sh_('truncate', '-s', '100M', OLD + '.ext'); sh_('mkfs.ext4', '-q', '-F', '-d', LT, OLD + '.ext')
mbr(OLD, [(2048, 600 * 2048, 0x07, NT), (2048 + 600 * 2048, 100 * 2048, 0x83, OLD + '.ext')])
OLDSUM = subprocess.run(f"head -c 754974720 '{OLD}' | sha256sum", shell=True, capture_output=True, text=True).stdout[:64]   # (the partitions' part of the disk)
# the USB stick: FAT32
USB = os.path.abspath('b62usb.img')
with open(USB, 'wb') as f: f.truncate((2048 + 400 * 2048) * 512)
sh_('truncate', '-s', '399M', USB + '.p'); sh_('mkfs.vfat', '-F', '32', '-n', 'STICK', USB + '.p', stdout=subprocess.DEVNULL); mbr(USB, [(2048, 399 * 2048, 0x0c, USB + '.p')])
FIXIMG = ''
if FIX:
    FD = os.path.join(W, 'fix'); shutil.copytree(FIX, FD); FIXIMG = os.path.abspath('b62fix.img'); sh_('truncate', '-s', '64M', FIXIMG)
    sh_('mkfs.ext4', '-q', '-F', '-L', 'R161FIX', '-d', FD, FIXIMG)
# (Part 27's BitLocker) a disk of three BitLocker partitions, cryptsetup's own test images: [(image, recovery key, cipher)]
BLK = ''; BLV = []
if BITLK:
    conf = open(f'{BITLK}/images.conf').read()
    def rp(n, k='RP'): return re.search(rf'^\[{re.escape(n)}\]\n(?:[^\[].*\n)*?{k}=(\S+)', conf, re.M).group(1)
    BLSUM = {n: rp(n, 'SHA256SUM') for n in ('bitlk-aes-xts-128', 'bitlk-aes-cbc-128', 'bitlk-aes-cbc-elephant-128')}   # (the decrypted volume's, cryptsetup's tests check it)
    BLV = [(n, rp(n), c) for n, c in (('bitlk-aes-xts-128', 'aes-xts-plain64'), ('bitlk-aes-cbc-128', 'aes-cbc-eboiv'), ('bitlk-aes-cbc-elephant-128', 'aes-cbc-elephant'))]
    BLK = os.path.abspath('b62bitlk.img'); st = 2048; parts = []
    for n, _, _ in BLV:
        sz = os.path.getsize(f'{BITLK}/{n}.img') // 512; shutil.copy(f'{BITLK}/{n}.img', BLK + '.' + n); parts.append((st, sz, 0x07, BLK + '.' + n)); st += sz
    with open(BLK, 'wb') as f: f.truncate((st + 2048) * 512)
    mbr(BLK, parts)
def boot(tag):
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf162',
             '-drive', f'file={OLD},format=raw,if=none,id=old', '-device', 'nvme,drive=old,serial=oldlaptop',
             '-kernel', f'{KDIR}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1',
             '-device', 'qemu-xhci,id=xh', '-drive', f'file={USB},format=raw,if=none,id=us', '-device', 'usb-storage,bus=xh.0,drive=us,serial=b62usb']
    if FIXIMG: extra += ['-drive', f'file={FIXIMG},format=raw,if=none,id=fx,readonly=on', '-device', 'nvme,drive=fx,serial=b62fix']
    if BLK: extra += ['-drive', f'file={BLK},format=raw,if=none,id=bl', '-device', 'nvme,drive=bl,serial=bitlkdisk']
    vm = sbvm.VM('b62' + tag, PARTS, [], secure=False, mem=MEM, extra=extra)
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    time.sleep(30); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    return vm, g, up
def run(g, c, t=60, show=True):
    f = os.path.join(W, 'c.sh'); open(f, 'w').write(c + '\n'); g.put(f, '/tmp/c62.sh', '644')
    o = g.sh('sh /tmp/c62.sh 2>&1', t)
    if show: out('$', c[:160].replace('\n', ' ; ')); out(o.rstrip()[-2500:])
    return o
TOK = "$(busybox wget -q -O- http://127.0.0.1:8080/ | grep -o 'token:\"[0-9a-f]*' | cut -d'\"' -f2)"
def api(sh, p, post=False, body=None, t=60):
    if body is not None:
        f = os.path.join(W, 'body'); open(f, 'w').write(body); g.put(f, '/tmp/b62.body', '644')
    pd = '--data-binary @/tmp/b62.body' if body is not None else ''
    return sh(f"T={TOK}; if command -v curl >/dev/null; then curl -s -w ' HTTP%{{http_code}}' {'-X POST' if post else ''} {pd} -H \"X-Rudolf-Token: $T\" 'http://127.0.0.1:8080/api/{p}'; "
              f"else busybox wget -q -O- --header \"X-Rudolf-Token: $T\" {('--post-file=/tmp/b62.body' if body is not None else '--post-data= ') if post else ''} 'http://127.0.0.1:8080/api/{p}' 2>&1; fi; echo", t)
def job(sh, name, t=900):
    for _ in range(3):   # (a command that did not arrive whole over the serial line: asked again)
        o = sh(f"for i in $(seq 1 {t // 2}); do [ \"$(cat /run/rudolf/job.name 2>/dev/null)\" = {name} ] && [ -e /run/rudolf/job.status ] && break; sleep 2; done; echo STATUS=$(cat /run/rudolf/job.status); tail -c 3000 /run/rudolf/job.log", t + 30)
        m = re.search(r'STATUS=(-?\d+)', o)
        if m: return int(m.group(1)), o
    return None, o
def bitlocker(sh):
    """(Part 27's BitLocker) the three BitLocker partitions opened read-only through rudolfd, as the page asks with the recovery key"""
    o = sh("uname -r; grep -c device-mapper /proc/misc; ls /dev/mapper/control; "
           "[ -x /tmp/fx/cs/cryptsetup ] && { cp /tmp/fx/cs/cryptsetup /usr/sbin/cryptsetup; chmod 755 /usr/sbin/cryptsetup; for l in /tmp/fx/cs/lib/*; do [ -e /usr/lib/x86_64-linux-gnu/${l##*/} ] || [ -e /lib/x86_64-linux-gnu/${l##*/} ] || cp $l /usr/lib/x86_64-linux-gnu/; done; echo cs-in; }; "
           "cryptsetup --version; ls /usr/lib/udev/rules.d | grep -c 'dm\\|lvm'; pidof systemd-udevd >/dev/null && echo udevd-runs", 120)
    ok('B0. the kernel built from this tree: the device mapper (/dev/mapper/control); cryptsetup in place', '/dev/mapper/control' in o and re.search(r'cryptsetup 2\.\d', o), o)
    o = sh("for b in /sys/block/nvme*n1; do echo ${b##*/} $(cat $b/device/serial 2>/dev/null); done")
    m = re.search(r'^(nvme\dn1) bitlkdisk', o, re.M); BD = m.group(1) if m else 'nvme3n1'; P = [f'{BD}p{i}' for i in (1, 2, 3)]
    o = api(sh, f'bring/scan?disk={BD}', True); st, lg = job(sh, 'bring-scan', 300)
    o2 = api(sh, f'bring/result?disk={BD}')
    ok('B1. the disk scanned: its three partitions BitLocker, locked (their key asked for)', st == 0 and all(f'"part":"{p}","kind":"bitlocker"' in o2 for p in P), [st, lg[-400:], o2[-800:]])
    o = api(sh, f'bring/unlock?part={P[0]}', True, '111111-222222-333333-444444-555555-666666-777777-888888')
    ok('B2. a wrong recovery key: refused as "That key does not open it." (403), nothing opened', ('does not open it' in o or ' 403' in o) and f'rbl-{P[0]}' not in sh('ls /dev/mapper', 30, False), o[-300:])
    for (n, key, ci), p in zip(BLV, P):
        o = api(sh, f'bring/unlock?part={p}', True, key)
        o2 = sh(f"DM_DISABLE_UDEV=1 cryptsetup status rbl-{p}; for d in /sys/block/dm-*; do [ \"$(cat $d/dm/name)\" = rbl-{p} ] && cat $d/ro; done; dd if=/dev/mapper/rbl-{p} bs=512 count=1 2>/dev/null | head -c 11 | tail -c 8; echo; sha256sum /dev/mapper/rbl-{p} | cut -c1-64", 120)
        ok(f'B3. {n} ({ci}): opened read-only with its recovery key; decrypted right (its NTFS boot sector, and the whole volume\'s SHA-256 as cryptsetup\'s tests know it)', 'opened (read-only)' in o and f'cipher:  {ci}' in o2 and re.search(r'mode: +readonly', o2) and re.search(r'^1$', o2, re.M) and 'NTFS    ' in o2 and BLSUM[n] in o2, [o[-300:], o2[-800:], BLSUM[n]])
    o = sh("grep -A3 -E '^name +: (xts|cbc|ecb)\\(aes\\)' /proc/crypto | grep -E '^(name|module)'")
    ok('B3b. the kernel\'s own ciphers did it: xts(aes), cbc(aes) and ecb(aes) (the Elephant diffuser\'s), built in (no module: the drives helper may load none)', all(re.search(rf'name +: {m}\(aes\)\nmodule +: kernel', o) for m in ('xts', 'cbc', 'ecb')), o)
    o = api(sh, f'bring/scan?disk={BD}', True); st, lg = job(sh, 'bring-scan', 300); o2 = sh("grep '/dev/mapper/rbl-' /proc/mounts")
    # (cryptsetup's test images hold no whole NTFS inside: ntfs-3g refuses dislocker's decryption of them the same way ($MFTMirr), so
    #  the opened volumes are read as NTFS that cannot be mounted; a real Windows disk is the owner's check)
    ok('B4. scanned again: each opened volume read through its mapping, as the NTFS inside (here "ntfs-unreadable": the test images\' NTFS is not whole), never mounted writable', st == 0 and all(f'"part":"{p}","kind":"ntfs-unreadable"' in lg or re.search(rf'/dev/mapper/rbl-{p} \S+ ntfs3 ro,nosuid,nodev,noexec', o2) for p in P) and ' rw,' not in o2, [st, lg[-500:], o2])
    sh("rm -f /tmp/b62.body", 10, False)
    keys = '|'.join(k.replace('-', '') for _, k, _ in BLV) + '|' + '|'.join(k for _, k, _ in BLV)
    o = sh(f"grep -rlsE '{keys}' /persist /run /tmp /var /etc /home 2>/dev/null | head; echo grep-done; tail -5 /run/rudolf/health.log /persist/var/log/health.log 2>/dev/null | grep -i bitlocker", 300)
    ok('B5. the recovery keys are stored nowhere (not in /persist, /run, /tmp, /var, /etc, /home: the files, logs and health log say only that it was opened)', re.search(r'^grep-done$', o, re.M) and not re.search(r'^/', o.split('grep-done')[0], re.M) and 'opened read-only' in o, o[-800:])
    o = api(sh, f'bring/close?disk={BD}', True); o2 = sh("echo MAP $(ls /dev/mapper) MOUNTS $(grep -c '/dev/mapper/rbl-' /proc/mounts)")
    ok('B6. closed (bring/close): unmounted, the BitLocker volumes closed (only /dev/mapper/control left)', 'MAP control MOUNTS 0' in o2, [o[-300:], o2])
    # (what DM_DISABLE_UDEV in bringfiles.sh is for: cryptsetup asked without it, as before this change, the right key, in the
    #  background so a wait for udev cannot hold the serial shell: cryptsetup blocks signals while it waits, so only KILL ends it)
    f = os.path.join(W, 'k'); open(f, 'w').write(BLV[0][1]); g.put(f, '/tmp/b62.k', '600'); os.remove(f)
    o = sh(f"( timeout -s KILL 40 cryptsetup open --type bitlk --readonly --key-file=/tmp/b62.k /dev/{P[0]} rbltest; echo rc=$? > /tmp/b62.rc ) >/dev/null 2>&1 & "
           "for i in $(seq 1 50); do [ -s /tmp/b62.rc ] && break; sleep 1; done; cat /tmp/b62.rc; rm -f /tmp/b62.k; DM_DISABLE_UDEV=1 cryptsetup close rbltest 2>/dev/null; echo MAP $(ls /dev/mapper)", 120)
    m = re.search(r'rc=(\d+)', o)
    out(f'(cryptsetup without DM_DISABLE_UDEV, udev running without the device mapper\'s rules: {"it waited for udev until killed after 40 s" if m and m.group(1) == "137" else "returned " + (m.group(1) if m else "nothing")}; {o.strip().splitlines()[-1] if o.strip() else ""})')
    ok('B7. ...which is why bringfiles.sh asks it with DM_DISABLE_UDEV: without it cryptsetup never returns here (killed after 40 s); the test\'s mapping closed again', m and m.group(1) == '137' and 'MAP control' in o, o[-400:])
make_drive(DRV)
vm = None
try:
    vm, g, up = boot('a'); sh = lambda c, t=60, show=True: run(g, c, t, show)
    ok('0. the release starts', up)
    if not up: raise RuntimeError('did not start')
    sh("for i in $(seq 1 90); do busybox wget -q -O /dev/null http://127.0.0.1:8080/favicon.png && break; sleep 2; done; cat /usr/share/rudolf/VERSION; uname -r", 240)
    if FIX:
        g.put(os.path.join(HERE, 'fix161.sh'), '/tmp/fix161.sh', '644'); o = sh("sh /tmp/fix161.sh; cp /tmp/fx/lib/bringfiles.sh /tmp/fx/lib/install.sh /usr/lib/rudolf/ && chmod 755 /usr/lib/rudolf/bringfiles.sh /usr/lib/rudolf/install.sh && echo libs-in", 240)
        ok('0b. this tree\'s rudolfd (through rudolf-rootd), helpers, bringfiles.sh and install.sh in place', 'through rudolf-rootd' in o and 'libs-in' in o, o)
    o = sh("for b in /sys/block/nvme*n1; do echo ${b##*/} $(cat $b/device/serial 2>/dev/null); done; for i in $(seq 1 30); do D=$(for b in /sys/block/sd*; do [ -e $b ] && readlink -f $b/device | grep -q usb && echo ${b##*/}; done | head -1); [ -n \"$D\" ] && break; sleep 2; done; echo usb=$D; grep /media/ /proc/mounts")
    m = re.search(r'^(nvme\dn1) oldlaptop', o, re.M); OD = m.group(1) if m else 'nvme1n1'
    m = re.search(r'usb=(sd[a-z]+)', o); UD = m.group(1) if m else 'sda'; UP = UD + '1'
    ok('1. the old disk and the USB stick are there (the stick mounted by Rudolf OS)', m and f'/media/{UP} vfat rw' in o, o)
    s0 = sh(f"head -c 754974720 /dev/{OD} | sha256sum | cut -c1-64", 600, False).strip().split('\n')[-1]
    ok('1b. the old disk as made (its SHA-256 in the guest is the image\'s)', s0 == OLDSUM, [s0, OLDSUM])
    if BLV: bitlocker(sh)
    # found and sized
    o = api(sh, f'bring/scan?disk={OD}', True); st, lg = job(sh, 'bring-scan', 600)
    o2 = api(sh, f'bring/result?disk={OD}'); m = re.search(r'\{"ok":true.*\}', o2); res = json.loads(m.group(0)) if m else {}
    sysw = [s for s in res.get('systems', []) if s['os'] == 'windows']; sysl = [s for s in res.get('systems', []) if s['os'] == 'linux']
    ann = [p for s in sysw for p in s['people'] if p['name'] == 'Ann']; af = {f['k']: f['files'] for p in ann for f in p['folders']}
    ok('2. scanned (a job through the drives helper): Windows on the first partition (Ann, Bob, Public; not Default), Linux on the second (carol)', st == 0 and sysw and sorted(p['name'] for p in sysw[0]['people']) == ['Ann', 'Bob', 'Public'] and sysl and [p['name'] for p in sysl[0]['people']] == ['carol'], [st, lg[-800:], o2[-1500:]])
    ok('3. Ann\'s folders sized, only personal files: Documents 3 (with OneDrive\'s; no setup.exe), Pictures 40, Desktop 1, Other 1 (Saved Games)', af == {'Desktop': 1, 'Documents': 3, 'Pictures': 40, 'Other': 1}, af)
    o = sh(f"grep '/run/rudolf/bring/m/' /proc/mounts")
    ok('4. the old disk\'s partitions mounted read-only by the kernel\'s ntfs3 and ext4, without programs, device files or set-user-ID', re.search(rf'/dev/{OD}p1 /run/rudolf/bring/m/{OD}p1 ntfs3 ro,nosuid,nodev,noexec', o) and re.search(rf'/dev/{OD}p2 /run/rudolf/bring/m/{OD}p2 ext4 ro,nosuid,nodev,noexec', o), o)
    # the installation refused before the copy
    o = api(sh, f'install?disk={OD}&confirm={OD}', True); st, lg = job(sh, 'install', 300)
    ok('5. Install here before the files are kept: refused by install.sh (12), nothing written', st == 12 and 'Nothing was erased' in lg and 'Step 1/5' not in lg, [o[-300:], st, lg[-600:]])
    pid = lambda n: [p['id'] for s in sysw for p in s['people'] if p['name'] == n][0]
    A, B = pid('Ann'), pid('Bob'); DEST = f'/media/{UP}/Rudolf OS - Bring your files/Old computer 2026-11-22 10.30'
    body = f"{OD} continue=0\n{DEST}\n{A}\tDocuments\n{A}\tPictures\n{A}\tDesktop\n{A}\tOther\n{B}\tVideos\n"
    # a cancelled copy (Bob's videos, 200 MB: the stop asked for as soon as it counts), then its folder taken away
    # (in one command in the guest: the copy asked for, then Stop as soon as its log starts; with KVM 200 MB is copied in
    # about a second, so the serial line's round trips would come too late)
    f = os.path.join(W, 'body'); open(f, 'w').write(f"{OD} continue=0\n/media/{UP}/Rudolf OS - Bring your files/Cancelled\n{B}\tVideos\n"); g.put(f, '/tmp/b62.body', '644')
    sh(f"T={TOK}; rm -f /run/rudolf/job.log; busybox wget -q -O- --header \"X-Rudolf-Token: $T\" --post-file=/tmp/b62.body http://127.0.0.1:8080/api/bring/copy; "
       "for i in $(seq 1 400); do grep -q 'Counting' /run/rudolf/job.log 2>/dev/null && break; sleep 0.02; done; "
       "busybox wget -q -O- --header \"X-Rudolf-Token: $T\" --post-data= http://127.0.0.1:8080/api/bring/cancel; echo", 60)
    st, lg = job(sh, 'bring-copy', 300)
    ok('6. a cancelled copy: stopped (130), "Nothing is erased"', st == 130 and 'Nothing is erased' in lg, [st, lg[-500:]])
    if st != 130: raise RuntimeError('the copy was not cancelled: the installation is not tried, so the old disk stays for the next checks')
    o = api(sh, f'install?disk={OD}&confirm={OD}', True); st2, lg2 = job(sh, 'install', 300)
    ok('6b. ...nothing marked: the installation still refused (12)', st2 == 12 and 'Step 1/5' not in lg2, [st2, lg2[-300:]])
    sh(f"rm -rf '/media/{UP}/Rudolf OS - Bring your files/Cancelled'; df -k /media/{UP} | tail -1", 60)
    # the copy, checked
    o = api(sh, 'bring/copy', True, body); st, lg = job(sh, 'bring-copy', 900)
    o2 = sh(f"cd '{DEST}' && find . -type f | sort | head -60; grep -c . bring.sha256; sed 's/^\\([0-9a-f]*\\)  [0-9]*  /\\1  /' bring.sha256 | sha256sum -c 2>&1 | grep -c ': OK'; cat bring.txt")
    ok('7. copied and checked onto the stick (0): 47 files, each SHA-256 in bring.sha256 and matching', st == 0 and 'Copied and checked: 47 files' in lg and re.search(r'^47\n47$', o2, re.M), [st, lg[-700:], o2[-1500:]])
    ok('8. the folders: Ann/Documents (OneDrive\'s under Documents/OneDrive), Ann/Other/Saved Games, Bob/Videos; no setup.exe, AppData or NTUSER.DAT', './Ann/Documents/OneDrive/budget.xlsx' in o2 and './Ann/Other/Saved Games/save1.sav' in o2 and './Bob/Videos/birthday.mp4' in o2 and not re.search(r'setup\.exe|AppData|NTUSER', o2), o2[-1500:])
    s1 = sh(f"head -c 754974720 /dev/{OD} | sha256sum | cut -c1-64", 600, False).strip().split('\n')[-1]
    ok('9. the old disk unchanged by looking and copying (its SHA-256)', s1 == OLDSUM, [s1, OLDSUM])
    # after installing (here, the same computer): an account's folder opened, the files brought in as its user
    key = os.urandom(64).hex(); ACC = 'uBring62t'
    o = api(sh, f'home/unlock?acct={ACC}&session=1', True, key) + sh(f"cat /run/rudolf/home-session; ls -ld /persist/accounts/{ACC}/Documents")
    o += api(sh, 'bring/found')
    ok('10. the copy found on the stick (bring/found): Ann and Bob, with their files', '"made"' in o and '"name":"Ann","files":45' in o and '"name":"Bob","files":2' in o, o[-1200:])
    o = api(sh, f'bring/import?acct={ACC}&dir={DEST.replace(" ", "%20")}&person=Ann', True); st, lg = job(sh, 'bring-import', 600)
    o2 = sh(f"cd /persist/accounts/{ACC} && find Documents Pictures Desktop -type f | sort | head -50; stat -c '%u %n' Documents/Taxes/return.pdf; wc -l < .rudolf/bring-imported.txt")
    ok('11. brought in (0): Documents, Pictures, Desktop; the rest in Documents/From the old computer; the account\'s user\'s; the list for the antivirus', st == 0 and 'Documents/Taxes/return.pdf' in o2 and 'Pictures/Trip/IMG_0040.JPG' in o2 and 'Documents/From the old computer/Saved Games/save1.sav' in o2 and re.search(r'^1000 Documents/Taxes/return\.pdf', o2, re.M) and re.search(r'^45$', o2, re.M), [st, lg[-600:], o2[-1500:]])
    o = api(sh, f'bring/import?acct={ACC}&dir={DEST.replace(" ", "%20")}&person=Ann', True); st, lg = job(sh, 'bring-import', 600)
    ok('12. brought in again: nothing twice (45 already there)', st == 0 and '45 already there' in lg, lg[-500:])
    o = api(sh, f'bring/import?acct=uOther000&dir={DEST.replace(" ", "%20")}&person=Ann', True); st, lg = job(sh, 'bring-import', 120)
    ok('13. another account than the one signed in: refused by the helper', st == 2 or 'refused' in lg, [st, lg[-300:]])
    # the installation goes ahead now (past the guard; the old disk closed first)
    o = api(sh, f'install?disk={OD}&confirm={OD}', True); st, lg = job(sh, 'install', 900)
    o2 = sh("grep -c '/run/rudolf/bring/m/' /proc/mounts")
    ok('14. after the copy: the installation goes ahead (past the guard: the old disk closed, its copy started)', 'Not installed' not in lg and 'Step 1/5' in lg and re.search(r'^0$', o2, re.M), [st, lg[-800:], o2])
    o = sh("cat /persist/var/log/health.log /run/rudolf/health.log 2>/dev/null | grep -i 'bring your files\\|Install:' | tail -6")
    ok('15. the health log says what was done (the copy, the installation)', 'Bring your files' in o and 'Install:' in o, o)
finally:
    try: vm and vm.stop()
    except Exception: pass
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
