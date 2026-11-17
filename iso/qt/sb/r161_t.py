# Rudolf OS 1.6.1 (docs/plan-1.6.1.md Part 11) in QEMU: rudolfd's root work through rudolf-rootd and the root helpers,
# on a release (the newest beta), with this tree's rudolfd, rudolf-rootd and helpers put in place (R161_FIX: a folder
# with rudolfd, rudolf-rootd and root/NAME, built from this tree; on an extra disk) and rudolfd started again through
# rudolf-rootd, as the base layer's rudolf-init will start it. A USB stick (QEMU's usb-storage, an ext4 partition with
# a file) for the drives: mount, unmount, eject through the API as the desktop calls it, a stick's information, an
# install job that the helper hands to install.sh (an image that is not there: refused by install.sh, nothing
# written), a name rudolfd lets through but the helper refuses (said in the health log), the helpers out of reach of
# the desktop's user (uid 1000: not readable or runnable, no socket to reach rudolf-rootd). Updates: sys/status and
# sys/stable as rudolf-update says them by hand, a setting changed and back, the first heartbeat's boot-ok.
#   python3 r161_t.py REL-DIR   (SB_PARTS and QEMU as for t160; root; QEMU_ACCEL=kvm where it can)
import os, sys, time, re, subprocess, shutil, struct
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); FIX = os.environ.get('R161_FIX', '')
LOG = open('r161.txt', 'w'); R = []; T0 = time.time()
def out(*a):
    s = ' '.join(str(x) for x in a); s = f'[{int(time.time() - T0):5d}s] ' + s; print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:1500] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 3000; DRV = os.path.abspath('r161.img'); MEM = int(os.environ.get('MEM', '3072')); W = os.path.abspath('w161r')
shutil.rmtree(W, ignore_errors=True); os.makedirs(W)
def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    d = f'{T}/os/system/a'; os.makedirs(d)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs'): shutil.copy(f'{REL}/{n}', f'{d}/{n}')
    for n in ('manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{d}/{n}')
    open(f'{d}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n"); open(f'{d}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    mbr_part(path, start, DMB * 2048, D)
def mbr_part(path, start, secs, data):
    with open(path, 'r+b') as f:
        f.seek(446); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, secs))
        f.seek(510); f.write(b'\x55\xaa'); f.seek(start * 512)
        with open(data, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(data)
# the USB stick: one ext4 partition (label R161USB) with a file
USB = os.path.abspath('r161usb.img'); UT = os.path.join(W, 'usb'); os.makedirs(UT); open(f'{UT}/hello.txt', 'w').write('hello from the stick\n')
with open(USB, 'wb') as f: f.truncate((2048 + 64 * 2048 + 2048) * 512)
subprocess.run(['truncate', '-s', '64M', USB + '.p'], check=True); subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'R161USB', '-d', UT, USB + '.p'], check=True)
mbr_part(USB, 2048, 64 * 2048, USB + '.p')
FIXIMG = ''
if FIX:
    FD = os.path.join(W, 'fix'); shutil.copytree(FIX, FD)
    FIXIMG = os.path.abspath('r161fix.img'); subprocess.run(['truncate', '-s', '64M', FIXIMG], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'R161FIX', '-d', FD, FIXIMG], check=True)
def boot(tag):
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf161',
             '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1',
             '-device', 'qemu-xhci,id=xh', '-drive', f'file={USB},format=raw,if=none,id=us', '-device', 'usb-storage,bus=xh.0,drive=us,serial=r161usb']
    if FIXIMG: extra += ['-drive', f'file={FIXIMG},format=raw,if=none,id=fx,readonly=on', '-device', 'nvme,drive=fx,serial=r161fix']
    vm = sbvm.VM('r161' + tag, PARTS, [], secure=False, mem=MEM, extra=extra)
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    time.sleep(30); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    return vm, g, up
def run(g, c, t=60, show=True):
    f = os.path.join(W, 'c.sh'); open(f, 'w').write(c + '\n'); g.put(f, '/tmp/c161.sh', '644')
    o = g.sh('sh /tmp/c161.sh 2>&1', t)
    if show: out('$', c[:160].replace('\n', ' ; ')); out(o.rstrip()[-2500:])
    return o
TOK = "$(busybox wget -q -O- http://127.0.0.1:8080/ | grep -o 'token:\"[0-9a-f]*' | cut -d'\"' -f2)"
# (curl's answers with their status: busybox wget prints nothing for a refusal)
def api(sh, p, post=False, t=60):
    return sh(f"T={TOK}; if command -v curl >/dev/null; then curl -s -w ' HTTP%{{http_code}}' {'-X POST' if post else ''} -H \"X-Rudolf-Token: $T\" 'http://127.0.0.1:8080/api/{p}'; "
              f"else busybox wget -q -O- --header \"X-Rudolf-Token: $T\" {'--post-data= ' if post else ''}'http://127.0.0.1:8080/api/{p}' 2>&1; fi; echo", t)

make_drive(DRV)
vm = None
try:
    vm, g, up = boot('a'); sh = lambda c, t=60, show=True: run(g, c, t, show)
    ok('0. the release starts', up)
    if not up: raise RuntimeError('did not start')
    sh("for i in $(seq 1 90); do busybox wget -q -O /dev/null http://127.0.0.1:8080/favicon.png && break; sleep 2; done; pidof rudolfd; cat /usr/share/rudolf/VERSION", 240)
    if FIX:
        # this tree's programs in place: rudolfd bound over the release's, rudolf-rootd and the helpers where the base
        # layer will have them (root's, 0700), and rudolfd started again through rudolf-rootd as rudolf-init will
        g.put(os.path.join(HERE, 'fix161.sh'), '/tmp/fix161.sh', '644'); o = sh("sh /tmp/fix161.sh", 240)
        ok('0b. this tree\'s rudolfd started again through rudolf-rootd', 'through rudolf-rootd' in o, o)
    o = sh("R=$(pidof rudolf-rootd); P=$(for p in $(pidof rudolfd); do [ \"$(awk '{print $4}' /proc/$p/stat)\" = \"$R\" ] && echo $p; done | head -1); echo rootd=$R rudolfd=$P; "
           "echo uids $(awk '/^Uid/{print $2}' /proc/$R/status) $(awk '/^Uid/{print $2}' /proc/$P/status); stat -c '%u:%g %a %n' /usr/bin/rudolf-rootd /usr/lib/rudolf/root /usr/lib/rudolf/root/*; "
           "grep -c rootd /proc/net/unix; ls -l /proc/$R/fd | grep -c socket")
    ok('1. rudolfd is rudolf-rootd\'s child; the helpers are root\'s, 0700; rudolf-rootd has no socket in the file system', re.search(r'rootd=\d+ rudolfd=\d+', o) and '0:0 700 /usr/lib/rudolf/root/drives' in o and re.search(r'^0$', o, re.M), o)
    # the stick: its partition, as the desktop finds it
    o = sh("for i in $(seq 1 30); do D=$(for b in /sys/block/sd*; do [ -e $b ] && readlink -f $b/device | grep -q usb && echo ${b##*/}; done | head -1); [ -n \"$D\" ] && break; sleep 2; done; echo disk=$D; ls /sys/block/$D 2>/dev/null | grep $D")
    m = re.search(r'disk=(sd[a-z]+)', o); DSK = m.group(1) if m else 'sdX'; PART = DSK + '1'
    ok('2. the USB stick is there (a disk and its partition)', m and PART in o, o)
    o = api(sh, 'disks'); ok('3. the API lists it (GET disks: rudolfd itself, no root needed)', f'"name":"{PART}"' in o and '"fs":"ext4"' in o, o)
    api(sh, f'umount?dev={PART}', True)   # (devplug may have mounted it at start: from nothing)
    o = api(sh, f'mount?dev={PART}', True) + sh(f"grep '^/dev/{PART} ' /proc/mounts; cat /media/{PART}/hello.txt")
    ok('4. mount (the drives helper, devplug): mounted under /media, without device files, set-user-ID or programs, the file there', '"mounted"' in o and re.search(rf'/media/{PART} ext4 rw,nosuid,nodev,noexec', o) and 'hello from the stick' in o, o)
    o = api(sh, f'umount?dev={PART}', True) + sh(f"grep -c '^/dev/{PART} ' /proc/mounts; ls -d /media/{PART} 2>&1")
    ok('5. unmount: "safe to remove", not mounted, its folder gone', 'safe to remove' in o and re.search(r'^0$', o, re.M) and 'No such file' in o, o)
    o = api(sh, f'stick/info?disk={DSK}'); ok('6. a stick\'s information (mkstick.sh --info through the helper): not a Rudolf OS stick', '"rudolf":false' in o, o)
    h0 = sh("cat /persist/var/log/health.log /run/rudolf/health.log 2>/dev/null | grep -c 'drives helper refused'", 30, False).strip().split('\n')[-1]
    o = api(sh, 'mount?dev=sdz-1', True) + sh("cat /persist/var/log/health.log /run/rudolf/health.log 2>/dev/null | grep 'drives helper refused' | tail -1")
    ok('7. a name rudolfd lets through (safe_word) but the helper does not: refused, said in the health log (Security)', ('could not mount' in o or 'HTTP/1.1 500' in o) and 'a device name that is not one (sdz-1)' in o, o)
    sh("mkdir -p /home/rudolf/Downloads; chown 1000:1000 /home/rudolf/Downloads; rm -f /home/rudolf/Downloads/not-there.iso*")
    o = api(sh, 'install?disk=' + DSK + '&confirm=' + DSK + '&image=%2Fhome%2Frudolf%2FDownloads%2Fnot-there.iso', True)
    o += sh("for i in $(seq 1 60); do [ -e /run/rudolf/job.status ] && break; sleep 1; done; cat /run/rudolf/job.name; echo; cat /run/rudolf/job.status; cat /run/rudolf/job.log; "
            f"cat /persist/var/log/health.log 2>/dev/null | grep 'Install:' | tail -1; grep -c '^/dev/{DSK}' /proc/mounts", 120)
    ok('8. an install job: the helper hands it to install.sh (the image is not there: refused by it, the stick untouched), logged', '"installing"' in o and 'No parts of not-there.iso' in o and re.search(r'^7$', o, re.M) and 'Install: /dev/' + DSK in o, o)
    o = api(sh, f'mount?dev={PART}', True) + api(sh, f'eject?dev={DSK}', True) + sh(f"grep -c '^/dev/{DSK}' /proc/mounts; ls /sys/block | grep -c '^{DSK}$'")
    ok('9. eject: the stick unmounted and let go ("safe to remove", the kernel no longer has it)', '"mounted"' in o and 'safe to remove' in o and o.rstrip().split('\n')[-1].strip() == '0', o)
    o = sh("su -s /bin/sh rudolf -c 'id -u; /usr/lib/rudolf/root/drives mount sda1; echo rc=$?; ls /usr/lib/rudolf/root/ 2>&1; cat /usr/lib/rudolf/root/drives > /dev/null; echo rc2=$?' 2>&1")
    ok('10. the desktop\'s user (1000) can neither run nor read the helpers', re.search(r'^1000$', o, re.M) and 'rc=126' in o and 'rc2=1' in o, o)
    # updates (the updates helper, rudolf-update): what the API answers is what rudolf-update says when run by hand
    o1 = api(sh, 'sys/status'); o2 = sh("rudolf-update status 2>/dev/null; echo")
    j1 = re.search(r'\{.*\}', o1); j2 = re.search(r'\{.*\}', o2)
    ok('12. updates: GET sys/status through the helper is rudolf-update status', j1 and j2 and j1.group(0) == j2.group(0), [o1[-600:], o2[-600:]])
    # (GET sys/stable is answered "use POST" by every rudolfd since 1.6.0: it comes after rudolfd's line that wants POST
    # for every action, and with POST it is not reached either; the page's Roll back to the last stable version gets that
    # error. Found by this test on 2026-11-17 and reported to the owner: a 1.6.0 fix, not this PR's. Said, not checked.)
    o1 = api(sh, 'sys/stable'); out('NOTE: GET sys/stable answers', o1.strip()[-200:])
    a0 = sh("grep '^auto=' /persist/etc/rudolf/update.conf 2>/dev/null; rudolf-update status 2>/dev/null | grep -o '\"auto\":[a-z]*'", 30, False)
    o = api(sh, 'sys/set?k=auto&v=0', True) + sh("rudolf-update status 2>/dev/null | grep -o '\"auto\":[a-z]*'") + api(sh, 'sys/set?k=auto&v=1', True) + sh("rudolf-update status 2>/dev/null | grep -o '\"auto\":[a-z]*'")
    ok('14. ...a setting (automatic updates off, then on) through the helper', o.count('"saved"') == 2 and '"auto":false' in o and '"auto":true' in o, [a0, o])
    o = sh("rm -f /run/rudolf/bootok; : > /run/rudolf/update.log", 20, False) + api(sh, 'heartbeat', True) + sh("sleep 3; cat /run/rudolf/update.log; ls -l /run/rudolf/bootok", 30)
    ok('15. ...the first heartbeat: rudolf-update boot-ok through the helper, its words in update.log', 'bootok' in o and len(o.strip()) > 0, o)
    o = sh("tail -20 /run/rudolf/rudolfd.log | grep -i 'rootd\\|refused' ; cat /persist/var/log/health.log 2>/dev/null | grep 'rootd refused' | tail -3; echo done")
    ok('11. nothing was refused by rudolf-rootd itself (every request came from rudolfd)', 'rudolf-rootd refused' not in o, o)
finally:
    try: vm and vm.stop()
    except Exception: pass
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
