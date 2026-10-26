# Rudolf OS 1.3.0 stage B in QEMU (UEFI, Secure Boot off): which data partition a start uses (item 3), crashes of
# Rudolf OS programs (item 4), the Linux password (item 6), and the Linux sandbox on a computer that starts its
# installed update by kexec, as every updated computer does (item 7).
#   python3 t130_t.py DISC.iso REL-DIR
# DISC.iso: a 1.3.0 test disc (mkdisc.sh with RUDOLF_UKI_TEST_CMDLINE=rudolf.debug=1: a serial shell).
# REL-DIR: its test release (manifest.txt/.sig and the five files), installed in slot a of the internal drive.
# TEST_RPK_PUB: not needed. Needs a static busybox on the host (/bin/busybox) for a small Debian stand-in, and gcc.
# Drives are made as Install here and Make a stick make them: the disc image, then RUDOLFDATA as partition 3.
#   A. an internal NVMe drive (installed: the 1.3.0 update in slot a, a Debian stand-in, home: "internal") alone: it
#      starts its update by kexec, which keeps the same drive and data partition; the Debian sandbox starts and runs
#      a program, can mount its own /proc; a new Linux password is set; a crashing Rudolf OS program is reported
#   B. the same drive and a Rudolf OS USB stick with its own RUDOLFDATA (home: "stick"), started from the stick: the
#      stick's files (1.2.9 took the first RUDOLFDATA found, the internal drive's on a laptop with NVMe)
#   C. the same drive and a stick holding only the disc image (as Rufus writes it): the internal drive's files and
#      update, as before (the start-up finds the stick first and cannot tell which drive the firmware started)
import os, sys, time, json, re, subprocess, shutil, struct, hashlib
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
DISC, REL = (os.path.abspath(x) for x in sys.argv[1:3])
PARTS = os.environ['SB_PARTS']; LOG = open('t130.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:300] if d != '' else ''))
def last(t): return t.strip().split('\n')[-1].strip() if t.strip() else ''
def J(v, d):
    try: return json.loads(v) if isinstance(v, str) else d
    except ValueError: return d
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
ISZ = os.path.getsize(DISC); START = (ISZ + 1048575) // 1048576 * 2048; DMB = 1200
def debian_standin(T):
    """a tiny Debian stand-in: busybox, accounts, the folders the sandbox mounts, owned by Debian's shifted root"""
    D = f'{T}/debian'
    for d in ('bin', 'usr/sbin', 'etc', 'proc', 'sys', 'dev', 'tmp', 'run', 'root', 'home/rudolf'): os.makedirs(f'{D}/{d}', exist_ok=True)
    shutil.copy('/bin/busybox', f'{D}/bin/busybox'); os.symlink('busybox', f'{D}/bin/sh'); os.symlink('/bin/busybox', f'{D}/usr/sbin/chpasswd')
    open(f'{D}/etc/passwd', 'w').write('root:x:0:0:root:/root:/bin/sh\nrudolf:x:1000:1000:rudolf:/home/rudolf:/bin/sh\n')
    open(f'{D}/etc/shadow', 'w').write('root:!:20000:0:99999:7:::\nrudolf:!:20000:0:99999:7:::\n'); os.chmod(f'{D}/etc/shadow', 0o640)
    open(f'{D}/etc/group', 'w').write('root:x:0:\nrudolf:x:1000:\n'); open(f'{D}/etc/resolv.conf', 'w').write('')
    open(f'{D}/etc/rudolf-debian', 'w').write('13.7\n'); open(f'{D}/.rudolf-ready', 'w').write('13.7\n')
    subprocess.run(['chown', '-R', '200000:200000', D], check=True)
def make_drive(path, who, slot=False, data=True):
    if os.path.exists(path): os.remove(path)
    with open(path, 'wb') as f:
        with open(DISC, 'rb') as o: shutil.copyfileobj(o, f)
        f.truncate((START + DMB * 2048 + 2048) * 512)
    if not data: return
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    open(f'{T}/home/rudolf/whoami.txt', 'w').write(who + '\n'); subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    if slot:
        a = f'{T}/os/system/a'; os.makedirs(a)
        for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs', 'manifest.txt', 'manifest.sig'): os.link(f'{REL}/{n}', f'{a}/{n}') if os.stat(REL).st_dev == os.stat(T).st_dev else shutil.copy(f'{REL}/{n}', f'{a}/{n}')
        open(f'{a}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n"); open(f'{a}/complete', 'w').write('1\n')
        open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
        debian_standin(T)
        # a Rudolf OS program that crashes (item 4); the kernel names it by its first 15 characters
        subprocess.run(['sh', '-c', f'printf "int main(void){{volatile int *p=0;return *p;}}" > {T}/c.c && gcc -static -O0 -o {T}/rudolf-crashtest {T}/c.c && rm {T}/c.c'], check=True)
    Dimg = path + '.data'
    subprocess.run(['truncate', '-s', f'{DMB}M', Dimg], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, Dimg], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', START, DMB * 2048))
        f.seek(START * 512)
        with open(Dimg, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(Dimg)
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
    while time.time() < end and g.js('typeof rdPost=="function"&&typeof jobCrashText=="function"', 20) is not True: time.sleep(5)
    js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();svj('bd95setup',1);const b=[...document.querySelectorAll('button')].find(b=>b.textContent.trim()=='Guest');if(b)b.click();return 1})()")
INT = os.path.abspath('int130.img'); STK = os.path.abspath('stick130.img'); RUF = os.path.abspath('rufus130.img')
NVME = lambda f: ['-drive', f'file={f},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf130']
USB = lambda f: ['-device', 'qemu-xhci,id=xh', '-drive', f'file={f},format=raw,if=none,id=us', '-device', 'usb-storage,bus=xh.0,drive=us,removable=on,bootindex=0']
make_drive(INT, 'internal', slot=True)

# ---- A. the internal drive alone
vm = sbvm.VM('t130a', PARTS, [], secure=False, mem=3072, extra=NVME(INT)); g = session(vm); sh = lambda c, t=60: g.sh(c, t); js = jsf(g)
try:
    up = wait_shell(g)
    ok('A. the internal NVMe drive starts its installed update by kexec (slot a)', up and last(sh('cat /etc/rudolf/slot', 20)) == 'a', sh('cat /etc/rudolf/slot', 20)[-60:])
    ok('...with its own drive and data partition (passed on at the handover)', last(sh('cat /etc/rudolf/media', 20)) == '/dev/nvme0n1p1' and last(sh('cat /etc/rudolf/persist', 20)) == '/dev/nvme0n1p3' and last(sh('cat /home/rudolf/whoami.txt', 20)) == 'internal', sh('cat /etc/rudolf/media /etc/rudolf/persist', 20)[-80:])
    o = sh('grep -o "rudolf.persist=[^ ]*" /etc/rudolf/cmdline', 20); u = last(sh('blkid /dev/nvme0n1p3 | sed -n "s/.* UUID=.\\([^\\"]*\\).*/\\1/p"', 20))
    ok('...the handover named them (rudolf.mediadev=, rudolf.persist=UUID)', u and f'rudolf.persist={u}' in o and 'rudolf.mediadev=/dev/nvme0n1p1' in sh('cat /etc/rudolf/cmdline', 20), [o[-80:], u])
    o = sh('/usr/bin/rudolf-debian start', 60)
    ok('7. the Debian sandbox starts on a computer started by kexec', re.search(r'^\d+$', last(o)) is not None, o[-200:])
    o = sh('/usr/bin/rudolf-debian run -- /bin/busybox echo hello-from-debian', 60)
    ok('...and runs a program in it', last(o) == 'hello-from-debian', o[-200:])
    o = sh('/usr/bin/rudolf-fscheck | grep -E "^(proc_mount|debian)="', 90)
    ok('...a sandbox may mount its own /proc, and Check says the sandbox started', 'proc_mount=ok' in o and 'debian=started' in o, o[-200:])
    # 6. a new Linux password, through rudolfd as Settings asks for it
    page(g, js)
    js("USER.r='admin';1")
    r = js("lxApi('debian/passwd','newpass1').then(r=>r.message,e=>'ERR '+e.message)")
    j = {}
    for _ in range(30):
        j = J(js("rd('job').then(j=>JSON.stringify(j))"), {})
        if not j.get('running') and j.get('status', -1) >= 0: break
        time.sleep(3)
    sw = last(sh('grep "^root:" /persist/debian/etc/shadow', 20)); fields = sw.split(':')
    good = False
    if len(fields) > 1 and fields[1].startswith('$'):
        _, alg, salt, _h = fields[1].split('$', 3)
        want = subprocess.run(['openssl', 'passwd', f'-{alg}', '-salt', salt, 'newpass1'], capture_output=True, text=True).stdout.strip()
        good = want == fields[1]
    elif len(fields) > 1 and len(fields[1]) == 13:   # busybox's chpasswd (the stand-in) uses DES
        good = subprocess.run(['busybox', 'cryptpw', '-m', 'des', '-S', fields[1][:2], 'newpass1'], capture_output=True, text=True).stdout.strip() == fields[1]
    ok('6. Change the Linux password: rudolfd sets it in Debian (root and rudolf), and it is the new one', r == 'started' and j.get('status') == 0 and good and last(sh('grep -c "^rudolf:!" /persist/debian/etc/shadow', 20)) == '0', [r, j.get('status'), sw[:40]])
    r = js("lxApi('debian/passwd','ab').then(r=>r.message,e=>'ERR '+e.message)")
    ok('...a password that is too short is refused', isinstance(r, str) and r.startswith('ERR'), r)
    # 4. a Rudolf OS program that crashes
    sh('/persist/rudolf-crashtest; echo ran', 30); time.sleep(2)
    c = J(js("rd('crashes').then(r=>JSON.stringify(r.crashes))"), [])
    ok('4. rudolfd lists the crash of a Rudolf OS program (from the kernel log), for the system report', any(x.get('program', '').startswith('rudolf-crashtes') and x.get('kind') == 'segmentation fault' and x.get('where', '').startswith('rudolf-crashtest') for x in c), c)
    rep = js("sysReport('').then(r=>r.text)", 180)
    ok('...and the system report has it', isinstance(rep, str) and re.search(r'== Crashes \(Rudolf OS programs since start-up\)\nrudolf-crashtes: segmentation fault', rep), (re.search(r'== Crashes[\s\S]*?\n\n', rep or '') or [''])[0] if isinstance(rep, str) else rep)
finally:
    vm.stop()

# ---- B. a Rudolf OS stick with its own RUDOLFDATA, and the internal drive
make_drive(STK, 'stick')
vm = sbvm.VM('t130b', PARTS, [], secure=False, mem=3072, extra=NVME(INT) + USB(STK)); g = session(vm); sh = lambda c, t=60: g.sh(c, t)
try:
    up = wait_shell(g)
    m = last(sh('cat /etc/rudolf/media', 20)); p = last(sh('cat /etc/rudolf/persist', 20)); w = last(sh('cat /home/rudolf/whoami.txt', 20))
    ok('B. started from a stick with its own files, on a computer with Rudolf OS installed: the stick\'s files, not the internal drive\'s', up and m.startswith('/dev/sd') and p == m[:-1] + '3' and w == 'stick', [m, p, w])
    ok('...and the stick\'s own system (not the internal drive\'s installed update)', last(sh('cat /etc/rudolf/slot', 20)) == 'disc')
finally:
    vm.stop()
os.remove(STK)

# ---- C. a stick with only the disc image (as Rufus writes it), and the internal drive
make_drive(RUF, '', data=False)
vm = sbvm.VM('t130c', PARTS, [], secure=False, mem=3072, extra=NVME(INT) + USB(RUF)); g = session(vm); sh = lambda c, t=60: g.sh(c, t)
try:
    up = wait_shell(g)
    p = last(sh('cat /etc/rudolf/persist', 20)); w = last(sh('cat /home/rudolf/whoami.txt', 20)); sl = last(sh('cat /etc/rudolf/slot', 20))
    ok('C. a stick without files plugged in: the internal drive\'s files and installed update, as before', up and p == '/dev/nvme0n1p3' and w == 'internal' and sl == 'a', [p, w, sl])
    ok('...and the update\'s kernel stays with the same start drive (rudolf.mediadev)', last(sh('cat /etc/rudolf/media', 20)) == last(sh('grep -o "rudolf.mediadev=[^ ]*" /etc/rudolf/cmdline | cut -d= -f2', 20)))
finally:
    vm.stop()
for f in (INT, RUF): os.remove(f)
out('%d/%d passed' % (sum(R), len(R)))
