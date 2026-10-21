# Rudolf OS 1.2.5 stage C in QEMU: make a Rudolf OS USB stick from a running Rudolf OS, start from it, and back up
# to a USB stick (UEFI, Secure Boot off).
#   python3 t125c_t.py DISC.iso NEWDIR
# DISC.iso: a test disc (mkdisc.sh with RUDOLF_UKI_TEST_CMDLINE=rudolf.debug=1: a serial shell).
# NEWDIR: a folder with a newer rudolfd (static), index.html, install.sh and mkstick.sh, put in place after the start
#   (to test them on a disc whose layers are older); the test adds small helper scripts and makes it a disk.
# The computer has three USB drives: the 4 GB stick to make (erased), a 64 MB FAT drive "BACKUPS", and the test disk.
#   1. Rudolf OS started from the disc makes a stick on the 4 GB drive (with "copy my files"): the image is read back
#      and compared, RUDOLFDATA is added, a file made in this session is on it
#   2. a second computer with only that stick starts from it, keeps files, and has the file
#   3. a backup written to BACKUPS has its checksum, and the drive is unmounted after
import os, sys, time, json, re, subprocess, shutil
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT); sys.path.insert(0, os.path.join(QT, '..'))
import sbvm, mkimage
def J(v, d):
    try: return json.loads(v) if isinstance(v, str) else d
    except ValueError: return d
from guest import Guest
from ser import Ser
ISO, NEWDIR = (os.path.abspath(x) for x in sys.argv[1:3])
PARTS = os.environ['SB_PARTS']; LOG = open('t125c.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:300] if d != '' else ''))
def last(t): return t.strip().split('\n')[-1].strip() if t.strip() else ''

# the drives
NEW = os.path.abspath('new125c.img'); D = os.path.abspath('new125c'); shutil.rmtree(D, ignore_errors=True); shutil.copytree(NEWDIR, D)
open(f'{D}/up.sh', 'w').write('#!/bin/sh\n# the newer page, scripts and rudolfd, until the next restart\ncp /mnt/new/index.html /usr/share/rudolf/www/index.html\n'
                              'cp /mnt/new/install.sh /mnt/new/mkstick.sh /usr/lib/rudolf/; chmod 755 /usr/lib/rudolf/install.sh /usr/lib/rudolf/mkstick.sh\n'
                              'cp /mnt/new/rudolfd /usr/lib/rudolf/rudolfd-new; chmod 755 /usr/lib/rudolf/rudolfd-new\npkill -x rudolfd; sleep 1\n(/usr/lib/rudolf/rudolfd-new > /run/rudolf/rudolfd.log 2>&1 &)\n'
                              'for i in 1 2 3 4 5 6 7 8 9 10; do wget -q -O /dev/null http://127.0.0.1:8080/ && break; sleep 1; done\necho restarted $i\n')
for f in ('up.sh', 'rudolfd', 'install.sh', 'mkstick.sh'): os.chmod(f'{D}/{f}', 0o755)
for f in (NEW,):
    if os.path.exists(f): os.remove(f)
subprocess.run(['truncate', '-s', '32M', NEW], check=True); subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'RUDOLFB', '-d', D, NEW], check=True)
STICK = os.path.abspath('stick125c.img')
if os.path.exists(STICK): os.remove(STICK)
subprocess.run(['truncate', '-s', '4G', STICK], check=True)
with open(STICK, 'r+b') as f: f.write(b'OLD DATA ' * 1000)   # something to erase
BK = os.path.abspath('backups125c.img'); t = os.path.abspath('bk-readme.txt'); open(t, 'w').write('backups go here\n')
if os.path.exists(BK): os.remove(BK)
mkimage.fat_image(BK, [(t, 'README.TXT')], label=b'BACKUPS    ')
def usb(i, f, boot=None):
    return ['-drive', f'file={f},format=raw,if=none,id=u{i}', '-device', f'usb-storage,bus=xh.0,drive=u{i},removable=on' + (f',bootindex={boot}' if boot is not None else '')]

def session(vm):
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3); return g
def helpers(g):
    def sh(c, t=60): return g.sh(c, t)
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
    def wait_shell(t=900, delay=45):
        time.sleep(delay); end = time.time() + t
        while time.time() < end:
            try:
                if 'ready' in g.sh('echo ready', 8): return True
            except OSError: pass
            time.sleep(4)
        return False
    def desktop():
        if not until('typeof RVER=="string"&&!!document.body', 600): return False
        js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();svj('bd95setup',1);const b=[...document.querySelectorAll('button')].find(b=>b.textContent.trim()=='Guest');if(b)b.click();return 1})()")
        until('!!USER', 120); return until("typeof rdPost=='function'&&typeof lxApi=='function'", 60) and js("USER.r='admin';window.elevate=async()=>true;1") == 1
    return sh, js, until, wait_shell, desktop

# ---- 1. the first computer: Rudolf OS from the disc; makes the stick
vm = sbvm.VM('t125c1', PARTS, [ISO], secure=False, mem=3072, extra=['-device', 'qemu-xhci,id=xh'] + usb(0, STICK) + usb(1, BK) + usb(2, NEW))
g = session(vm); sh, js, until, wait_shell, desktop = helpers(g)
try:
    ok('the first computer starts from the disc', wait_shell())
    sh('mkdir -p /mnt/new', 10)
    for _ in range(30):   # USB drives can appear a little after the shell
        nd = last(sh('findfs LABEL=RUDOLFB', 10))
        if nd.startswith('/dev/'): break
        time.sleep(3)
    # Rudolf OS mounts plugged-in drives itself (devplug, under /media): use that mount when it is there
    sh(f'mount -o ro {nd} /mnt/new 2>/dev/null || mount --bind /media/{nd[5:]} /mnt/new', 20)
    out('test disk:', nd, sh('ls /mnt/new', 20)[-200:])
    out('new rudolfd:', last(sh('sh /mnt/new/up.sh', 60)))   # the drive may be mounted noexec
    for _ in range(4):
        g.js("location.replace('http://127.0.0.1:8080/');1", 10); time.sleep(15)
        if until("typeof stkSection=='function'", 90): break
    ok('the desktop (with the new page) is up', desktop() and js("typeof stkSection") == 'function')
    sh("echo made-in-the-first-session > /home/rudolf/stick-marker.txt; chown 1000:1000 /home/rudolf/stick-marker.txt", 20)
    info = J(js("rd('info').then(i=>JSON.stringify(i))"), {}); disks = J(js("rd('disks').then(d=>JSON.stringify(d.disks))"), [])
    t = J(js("rd('info').then(async i=>JSON.stringify(stkTargets((await rd('disks')).disks,i).map(k=>[k.name,k.bytes,k.model])))"), [])
    out('started from', info.get('mediaDev'), '| drives:', [(d['name'], d['bytes'], d['removable']) for d in disks])
    ok('Make a stick offers the 4 GB USB drive only (not the disc, the 64 MB drive or the test disk)', len(t) == 1 and 4.2e9 < t[0][1] < 4.4e9, t)
    dev = t[0][0] if t else 'sdz'
    MD = (info.get('mediaDev') or '/dev/sr0')[5:]   # the drive Rudolf OS started from (here the disc: sr0)
    r = js(f"rdPost('mkstick?disk={dev}&confirm={dev}&image=&files=1').then(r=>r.message,e=>e.message)")
    ok('making the stick starts', r == 'making the stick', r)
    j = {}; end = time.time() + 1500
    while time.time() < end:
        j = J(js("rd('job').then(j=>JSON.stringify(j))"), {})
        if not j.get('running') and j.get('status', -1) >= 0: break
        time.sleep(10)
    log = j.get('log') or ''; out(log[-1200:])
    ok('...and finishes', j.get('status') == 0 and 'stick is ready' in log, log[-300:])
    ok('...after reading the copy back and comparing it', 'Step 2/5: checking the copy' in log and 'The copy matches' in log)
    ok('...with the file made in this session copied', 'Copying your files and settings' in log)
    r = js(f"rdPost('mkstick?disk={dev}&confirm=sdq&image=&files=0').then(r=>r.message,e=>e.message)")
    ok('a mistyped confirmation is refused', r == 'confirm by sending the drive name twice', r)
    r = js(f"rdPost('mkstick?disk={MD}&confirm={MD}&image=&files=0').then(r=>r.message,e=>e.message)"); time.sleep(8)
    jl = J(js("rd('job').then(j=>JSON.stringify(j))"), {})
    ok('the drive Rudolf OS runs from is refused', jl.get('status', 0) != 0 and 'running from' in (jl.get('log') or ''), (jl.get('log') or '')[-200:])
    # 3. a backup to the 64 MB drive
    bp = J(js("rd('info').then(async i=>JSON.stringify(stkBackupParts((await rd('disks')).disks,i).map(x=>x.part)))"), [])
    bk = [x for x in bp if x.get('label') == 'BACKUPS']
    ok('Back up lists the BACKUPS drive', len(bk) == 1, bp)
    if bk:
        r = js("(async()=>{const i=await rd('info');const p=stkBackupParts((await rd('disks')).disks,i).find(x=>x.part.label=='BACKUPS').part;try{return await stkBackupTo(p)}catch(e){return 'ERR '+e.message}})()", 300)
        ok('a backup is written and read back', isinstance(r, str) and r.startswith('rudolf-backup-'), r)
        bdev = bk[0]['name']; mp = bk[0].get('mount') or ''
        # Rudolf OS mounted the drive itself when it was plugged in: the backup uses that mount and leaves it
        o = sh(f"cd {mp}/RUDOLF-BACKUP && sha256sum -c *.sha256; cd /" if mp else f"mkdir -p /mnt/bk; mount -o ro /dev/{bdev} /mnt/bk && cd /mnt/bk/RUDOLF-BACKUP && sha256sum -c *.sha256; cd /; umount /mnt/bk", 60)
        ok('...its checksum file checks out on the drive itself', ': OK' in o, o[-200:])
        mounted = f'/dev/{bdev} ' in sh('cat /proc/mounts', 20)
        ok('...and the drive is left as it was (mounted by Rudolf OS, or not mounted)', mounted == bool(mp), [mp, mounted])
        ok('a Rudolf OS stick\'s own partitions are not offered for backups', not any(x.get('label') in ('RUDOLF EFI', 'RUDOLFDATA') for x in bp), [x.get('label') for x in bp])
    vm.shot('made')
finally:
    vm.stop()

# ---- 2. a second computer with only the new stick
vm = sbvm.VM('t125c2', PARTS, [], secure=False, mem=3072, extra=['-device', 'qemu-xhci,id=xh'] + usb(0, STICK, boot=0))
g = session(vm); sh, js, until, wait_shell, desktop = helpers(g)
try:
    ok('a computer with only the new stick starts from it', wait_shell(delay=60))
    ok('...keeping files on the stick (RUDOLFDATA)', last(sh('[ -e /run/rudolf/persistent ] && mountpoint -q /persist && echo kept || echo live', 20)) == 'kept', sh('grep persist /proc/mounts', 20))
    ok('...and the file from the first computer is there', last(sh('cat /home/rudolf/stick-marker.txt 2>&1', 20)) == 'made-in-the-first-session')
    ok('...the desktop runs', until('typeof RVER=="string"&&RVER=="1.2.5"', 600), g.js('typeof RVER=="string"&&RVER', 20))
    o = sh('grep -c made-by-mkstick /persist/etc/installed; [ -s /persist/etc/machine-id ] && echo has-own-id', 20)
    ok('...marked as made by Make a stick, with its own machine id', 'has-own-id' in o and re.search(r'^1\s*$', o, re.M), o)
    vm.shot('second')
finally:
    vm.stop()
out('%d/%d passed' % (sum(R), len(R)))
