# Rudolf OS 1.6.1 (docs/plan-1.6.1.md Part 20; docs/website-data-key.md) in QEMU: the shared browser profile's key sealed in
# a TPM 2.0 (swtpm, as QEMU's CRB TPM: the interface of AMD's and Intel's firmware TPMs) on a release's own layers, as
# installed (slot a on an NVMe drive with RUDOLFDATA), UEFI with Secure Boot off and OVMF measuring into the TPM, with a
# kernel that has the TPM's driver (KERNEL: a folder with vmlinuz and modules.sfs built from this tree's kconfig.sh; the
# release's kernel has none, so the check says so and stops). This tree's rudolf-webkey and session are on a disk of their
# own (WK161FIX); the session's own webkey_start is what runs. Checks: the TPM seen, its PCR 7 measured by the firmware;
# webkey_start: a new key (the first time), Chromium's key store on the desktop's session bus, PWSTORE kwallet6, rudolfd's
# webkey saying so; the release's own Chromium, as the desktop's user, keeping a cookie as v11 and reading it back; the
# key store unreachable for an account's own Linux user; a restart: the same key, the cookie still read; Secure Boot's
# keys changed in the firmware (MS keys enrolled, Secure Boot still off: PCR 7 changes): a new key, said in the health log
# and by webkey, the old cookie unreadable; PCR 7 extended by hand: the same; no TPM attached: Chromium's built-in key,
# said so. Tests only.
#   python3 wk161_t.py REL-DIR      (root; SB_PARTS, QEMU, QEMU_ACCEL as for t160_t.py; KERNEL; WK_FIX: a folder with
#                                    rudolf-webkey and tpmext built for the guest; swtpm on this machine)
import os, sys, time, re, subprocess, shutil, struct, json
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE); ISO = os.path.dirname(QT)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); KERNEL = os.environ.get('KERNEL', ''); FIX = os.environ.get('WK_FIX', '')
PARTS = os.environ.get('SB_PARTS', '/nonexistent'); MEM = int(os.environ.get('MEM', '3072'))
W = os.path.abspath('w161wk'); DRV = os.path.abspath('wk161.img'); FIXIMG = os.path.abspath('wk161fix.img'); DMB = 3000
LOG = open('wk161.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:700] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
shutil.rmtree(W, ignore_errors=True); os.makedirs(W)

def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    d = f'{T}/os/system/a'; os.makedirs(d)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs'):
        src = f'{REL}/{n}'
        if KERNEL and n in ('vmlinuz', 'modules.sfs'): src = f'{KERNEL}/{n}'
        shutil.copy(src, f'{d}/{n}')
    for n in ('manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{d}/{n}')
    open(f'{d}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n"); open(f'{d}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)
make_drive(DRV)
# the fix disk: this tree's rudolf-webkey and session, the test's tool, two pages that set and show a cookie
F = f'{W}/fix'; os.makedirs(f'{F}/www')
for n in ('rudolf-webkey', 'tpmext', 'rudolfd'): shutil.copy(os.path.join(FIX, n), f'{F}/{n}')
shutil.copy(os.path.join(ISO, 'sys', 'root', 'usr', 'lib', 'rudolf', 'session'), f'{F}/session')
open(f'{F}/www/set.html', 'w').write("<html><script>document.cookie='sid='+location.hash.slice(1)+'; max-age=86400; path=/';document.write('set')</script></html>")
open(f'{F}/www/show.html', 'w').write("<html><script>document.write('cookie=['+document.cookie+']')</script></html>")
subprocess.run(['truncate', '-s', '64M', FIXIMG], check=True); subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'WK161FIX', '-d', F, FIXIMG], check=True)
# the firmware's variables: without keys, and with Microsoft's enrolled but Secure Boot left off (PCR 7 differs)
VARS0 = f'{W}/vars0.fd'; VARSK = f'{W}/varsk.fd'
shutil.copy('/usr/share/OVMF/OVMF_VARS_4M.fd', VARS0)
r = subprocess.run(['virt-fw-vars', '-i', '/usr/share/OVMF/OVMF_VARS_4M.fd', '-o', VARSK, '--enroll-microsoft', '--set-false', 'SecureBootEnable'], capture_output=True, text=True)
HAVE_KEYS = r.returncode == 0 and os.path.exists(VARSK)
if not HAVE_KEYS: out('(virt-fw-vars could not enroll keys: the firmware-keys step is skipped)', r.stderr[-300:])
TPMD = f'{W}/tpm'; os.makedirs(TPMD)
swtpm = None
def tpm_up():
    global swtpm
    sock = f'{TPMD}/sock'
    try: os.unlink(sock)
    except OSError: pass
    swtpm = subprocess.Popen(['swtpm', 'socket', '--tpm2', '--tpmstate', f'dir={TPMD}', '--ctrl', f'type=unixio,path={sock}', '--log', f'file={W}/swtpm.log'])
    for _ in range(50):
        if os.path.exists(sock): break
        time.sleep(0.1)
    return ['-chardev', f'socket,id=chrtpm,path={sock}', '-tpmdev', 'emulator,id=tpm0,chardev=chrtpm', '-device', 'tpm-crb,tpmdev=tpm0']

def boot(tag, vars_fd, tpm=True):
    extra = ['-drive', f'if=pflash,format=raw,unit=1,file={vars_fd}',
             '-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf161',
             '-drive', f'file={FIXIMG},format=raw,if=none,id=fx,readonly=on', '-device', 'nvme,drive=fx,serial=wk161fix',
             '-kernel', f'{KERNEL}/vmlinuz' if KERNEL else f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1']
    if tpm: extra += tpm_up()
    vm = sbvm.VM('wk161' + tag, PARTS, [], secure=False, mem=MEM, extra=extra); t = time.time()
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    up = False; end = time.time() + 900
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(3)
    out(f'start {tag}: serial shell {"up" if up else "NOT up"} after {time.time() - t:.0f} s')
    if up:
        o = run(g, "i=0; while [ $i -lt 400 ] && [ ! -e /run/rudolf/session.env ]; do sleep 1; i=$((i+1)); done; ls /run/rudolf/session.env", 430)
    return vm, g, up
def run(g, c, t=120):
    f = os.path.join(W, 'c.sh'); open(f, 'w').write(c + '\n'); g.put(f, '/tmp/c.sh', '644')
    o = g.sh('sh /tmp/c.sh 2>&1', t); out('$', c[:160].replace('\n', ' ; ')); out(o.rstrip()[-2500:]); return o
def stop(vm, g):
    global swtpm
    try: g.sh('sync', 30)
    except Exception: pass
    vm.stop(); time.sleep(2)
    if swtpm: swtpm.terminate(); swtpm.wait(); swtpm = None

# in the guest: the fix disk, this tree's programs where the session's function looks for them, the function itself
SETUP = r'''
FX=; for d in /dev/nvme*n1; do blkid $d 2>/dev/null | grep -q WK161FIX && FX=$d; done
mkdir -p /tmp/fx; mountpoint -q /tmp/fx || mount -o ro $FX /tmp/fx
X=/run/wk161; mkdir -p $X; mountpoint -q $X || mount -t tmpfs -o exec,mode=755 none $X
cp /tmp/fx/rudolf-webkey /tmp/fx/tpmext $X/; chmod 755 $X/rudolf-webkey $X/tpmext; cp -r /tmp/fx/www $X/; chmod -R a+rX $X/www
sed -n '/^PWSTORE=basic$/,/^}$/p' /tmp/fx/session | sed "s#/usr/bin/rudolf-webkey#$X/rudolf-webkey#g" > $X/wk.sh
grep -c webkey_start $X/wk.sh
pgrep -f "httpd -p 8625" >/dev/null || busybox httpd -p 127.0.0.1:8625 -h $X/www
# this tree's rudolfd (its webkey request), started again through rudolf-rootd as fix161.sh does
if ! mountpoint -q /usr/bin/rudolfd; then cp /tmp/fx/rudolfd $X/rudolfd; chmod 755 $X/rudolfd; mount --bind $X/rudolfd /usr/bin/rudolfd
  R=$(pidof rudolf-rootd); [ -n "$R" ] && kill $R; P=$(pidof rudolfd | tr ' ' '\n' | sort -n | head -1); [ -n "$P" ] && kill $P 2>/dev/null
  for i in $(seq 1 20); do busybox wget -q -O /dev/null http://127.0.0.1:8080/favicon.png 2>/dev/null || break; sleep 0.5; done
  (setsid /usr/bin/rudolf-rootd /usr/bin/rudolfd >> /run/rudolf/rudolfd.log 2>&1 < /dev/null &)
  for i in $(seq 1 60); do busybox wget -q -O /dev/null http://127.0.0.1:8080/favicon.png 2>/dev/null && break; sleep 1; done; fi
'''
START = r'''
set -a; . /run/rudolf/session.env; set +a; PROFILE=/home/rudolf/.wk161-prof; mkdir -p $PROFILE; chown rudolf:rudolf $PROFILE
. /run/wk161/wk.sh; webkey_start; echo "PWSTORE=$PWSTORE WKPID=$WKPID"; cat /run/rudolf/webkey.json; cat /run/rudolf/webkey.err 2>/dev/null | tail -2
'''
def chrome(store, page):
    return (f"set -a; . /run/rudolf/session.env; set +a; su -s /bin/sh rudolf -c \"timeout 120 /opt/chromium/chrome --headless=new --no-sandbox --user-data-dir=/home/rudolf/.wk161-prof "
            f"--password-store={store} --no-first-run --no-default-browser-check --dump-dom 'http://127.0.0.1:8625/{page}'\" 2>/dev/null | grep -o 'cookie=\\[[^]]*\\]\\|set'; "
            "/run/wk161/rudolf-webkey count /home/rudolf/.wk161-prof")
def api_webkey():
    return "T=$(busybox wget -q -O - http://127.0.0.1:8080/ | sed -n 's/.*RUDOLFD={token:\"\\([0-9a-f]*\\)\".*/\\1/p' | head -1); busybox wget -q -O - --header \"X-Rudolf-Token: $T\" http://127.0.0.1:8080/api/webkey; echo"

vm = None
try:
    vm, g, up = boot('a', VARS0)
    if not up: ok('the guest started', False, 'no serial shell')
    else:
        o = run(g, "ls -la /dev/tpm0 /dev/tpmrm0; dmesg | grep -i tpm | tail -3", 30)
        if '/dev/tpmrm0' not in o or 'No such file' in o:
            ok('1. the kernel has the TPM driver (/dev/tpmrm0)' + ('' if KERNEL else ': the release\'s kernel has none (KERNEL: one built from this tree)'), False, o)
            raise SystemExit
        ok('1. the kernel has the TPM driver: /dev/tpmrm0, root\'s only', re.search(r'crw-------.*root.*/dev/tpmrm0', o) or re.search(r'crw-rw----\s+\d+ root\s+(root|tss).*/dev/tpmrm0', o), o)
        o = run(g, SETUP + "\n/run/wk161/rudolf-webkey status", 60); st = re.search(r'\{"tpm":true.*\}', o)
        ok("2. rudolf-webkey sees a TPM 2.0; the firmware measured Secure Boot's state into PCR 7", st and '"family":"2.0"' in o and not re.search(r'"pcr7":"0{64}"', o), o[-600:])
        P7A = re.search(r'"pcr7":"([0-9a-f]{64})"', o); P7A = P7A.group(1) if P7A else ''
        o = run(g, START, 120)
        ok("3. the session's webkey_start: a new key the first time, the key store started, Chromium told to use it (kwallet6)", 'PWSTORE=kwallet6' in o and '"state":"tpm"' in o and 'the first time' in o, o)
        o = run(g, api_webkey(), 30)
        ok("...and rudolfd's webkey says so (never the key)", '"ok":true,"state":"tpm"' in o or ('"state":"tpm"' in o and '"ok":true' in o), o)
        o = run(g, "stat -c 'mode=%a owner=%U %n' /persist/etc/rudolf-webkey /persist/etc/rudolf-webkey/sealed; tail -3 /persist/var/log/health.log 2>/dev/null || tail -3 /run/rudolf/health.log", 30)
        ok('...the sealed secret root\'s only on RUDOLFDATA; the health log says a new key was sealed', 'mode=700 owner=root /persist/etc/rudolf-webkey' in o and 'mode=600 owner=root /persist/etc/rudolf-webkey/sealed' in o and 'a new key sealed' in o, o)
        o = run(g, chrome('kwallet6', 'set.html#kept-under-the-tpm'), 200)
        ok("4. the release's own Chromium, as the desktop's user, with the key store: the cookie kept as v11", 'v10 0 v11 1' in o, o)
        o = run(g, chrome('kwallet6', 'show.html'), 200)
        ok('...and read back', 'cookie=[sid=kept-under-the-tpm]' in o, o)
        o = run(g, "su -s /bin/sh nobody -c 'ls /run/user/1000/ >/dev/null 2>&1 && echo REACHED || echo REFUSED; [ -w /run/user/1000/bus ] && echo BUS-OPEN'; "
                   "stat -c 'mode=%a owner=%U %n' /run/user/1000", 30)
        ok("5. another user (an account's own Linux user) cannot reach the key store: the desktop's bus is in its user's folder alone (0700)", 'REFUSED' in o and 'BUS-OPEN' not in o and 'mode=700 owner=rudolf /run/user/1000' in o, o)
        stop(vm, g)
        # a restart: the same key
        vm, g, up = boot('b', VARS0)
        if up:
            run(g, SETUP, 60); o = run(g, START, 120)
            ok('6. after a restart (Secure Boot as it was): the same key, nothing new said, the cookie still read', 'PWSTORE=kwallet6' in o and '"why":""' in o, o)
            o = run(g, chrome('kwallet6', 'show.html'), 200); ok('...the cookie still read', 'cookie=[sid=kept-under-the-tpm]' in o, o)
            stop(vm, g)
        else: ok('6. started again', False)
        # Secure Boot's keys changed in the firmware (Secure Boot still off, so this kernel still starts)
        if HAVE_KEYS:
            vm, g, up = boot('c', VARSK)
            if up:
                o = run(g, SETUP + "\n/run/wk161/rudolf-webkey status", 60); P7C = re.search(r'"pcr7":"([0-9a-f]{64})"', o)
                o2 = run(g, START, 120)
                ok("7. the firmware's Secure Boot keys changed (Microsoft's enrolled): PCR 7 differs, a new key, said why", P7C and P7C.group(1) != P7A and 'PWSTORE=kwallet6' in o2 and "Secure Boot's measurements changed" in o2, [P7A, o, o2])
                o = run(g, "tail -4 /persist/var/log/health.log 2>/dev/null; " + api_webkey(), 30)
                ok("...said in the health log (websites sign in again) and by rudolfd's webkey", 'websites sign in again' in o and "Secure Boot's measurements changed" in o, o)
                o = run(g, chrome('kwallet6', 'show.html'), 200)
                ok('...and what was kept under the old key cannot be read (Chromium drops it)', 'cookie=[]' in o, o)
                o = run(g, chrome('kwallet6', 'set.html#new-key') + "; /run/wk161/tpmext 7 9; /run/wk161/rudolf-webkey unseal 2>&1 >/dev/null | tail -1; echo rc=$?", 200)
                ok('8. PCR 7 extended by hand (a change after the start): rudolf-webkey makes a new key, said why', "Secure Boot's measurements changed" in o, o)
                stop(vm, g)
            else: ok('7. started with the keys enrolled', False)
        # no TPM
        vm, g, up = boot('d', VARS0, tpm=False)
        if up:
            run(g, SETUP, 60); o = run(g, START, 120)
            ok("9. no TPM: Chromium's built-in key (basic), said in rudolfd's webkey and the health log", 'PWSTORE=basic' in o and '"state":"none"' in o, o)
            o = run(g, "tail -2 /persist/var/log/health.log", 30); ok('...the health log says so', 'has no TPM 2.0' in o, o)
            stop(vm, g)
        else: ok('9. started without a TPM', False)
except SystemExit: pass
finally:
    try: vm and vm.stop()
    except Exception: pass
    if swtpm: swtpm.kill()
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
