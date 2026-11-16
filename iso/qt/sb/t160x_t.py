# Rudolf OS 1.6.0 in QEMU (docs/plan-1.6.0.md Part 13.7, "Before stable" 6): signed system extensions on the real
# release (its kernel, base, browser and drivers layers), started as t160_t.py starts it (UEFI, Secure Boot off,
# rudolf.debug=1 for the serial shell; the start-up picks the slot from the drive's state, as on a computer).
# An extension must be signed by a key in the base's /etc/rudolf/rpk-keys (the release key), which this test never
# has: it makes TEST keys and two custom builds of the release (Developer mode, as a developer's own build), each the
# release's layers with a desktop layer that adds the TEST system key to /etc/rudolf/rpk-keys and names its base ABI
# (/usr/share/rudolf/base-abi): build 1 (slot a, ABI 1) and build 2 (slot b, ABI 2, "the new base"). Nothing else of
# the release changes (FIX=1: this tree's rpk, sysext and rudolf-init are put into that desktop layer too).
# Extensions (rpk-tool build, signed by the TEST system key): hello 1.0 (ABI 1) and 2.0 (ABI 2), a program and a start
# script; hang (its start script never ends); corrupt (its file changed once installed); badsig (its file replaced
# by the same package signed by another key); breakdesk (its start script stops the desktop: rudolfd never runs).
#   1-4   an extension installed, in use after a restart (its program, its start script); the base unchanged
#   5-8   a start script that hangs is stopped after 20 s and the system starts; a changed file and another key's
#         signature are left out, each with its reason in the extensions' list, the health log (System health) and
#         the self-test; the computer starts and the desktop comes up
#   9-11  one that stops the desktop: the next start leaves it out ("the desktop did not come up"), the rest in use;
#         removal leaves nothing behind
#   12-14 an update of the base without the extension's build: the old one is left out ("built for another base"),
#         the computer starts on the new base and keeps it; with the right build installed it is in use
#   15-16 Roll back system (Update and recovery): back on the old base, the new base's build left out, the computer
#         starts; Recovery mode starts without extensions
#   python3 t160x_t.py REL-DIR      (REL-DIR with the release's boot.iso: the drive is laid out as Install here does; SB_PARTS and QEMU as for t160; root; QEMU_ACCEL=kvm where it can; FIX=1)
import os, sys, time, re, subprocess, shutil, struct, json, hashlib
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE); ISO = os.path.dirname(QT)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); FIX = os.environ.get('FIX') == '1'
PARTS = os.environ.get('SB_PARTS', '/nonexistent'); LOG = open('t160x.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[-1500:] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 3400; DRV = os.path.abspath('int160x.img'); MEM = int(os.environ.get('MEM', '3072')); W = os.path.abspath('w160x')
TOOL = f'{ISO}/pkgtools/rpk-tool.py'
shutil.rmtree(W, ignore_errors=True); os.makedirs(W)
def run_(a, **k): return subprocess.run(a, check=True, **k)
# ---- TEST keys (deleted with W at the end): the developer's (signs the two builds), the stand-in system key (signs
# the extensions; trusted only by the two builds' rpk-keys), another key (badsig)
for k in ('dev', 'sys', 'other'): run_([sys.executable, TOOL, 'keygen', f'{W}/{k}'], stdout=subprocess.DEVNULL)
DEVPUB = open(f'{W}/dev/rpk-signing.pub').read().strip(); SYSPUB = open(f'{W}/sys/rpk-signing.pub').read().strip()
ABI1 = hashlib.sha256(b'rudolf test base 1').hexdigest(); ABI2 = hashlib.sha256(b'rudolf test base 2').hexdigest()
SER = int(mv('serial'))
# ---- the two builds: the release's layers, a desktop layer of their own
def comp(sfs):
    o = subprocess.run(['unsquashfs', '-s', sfs], capture_output=True, text=True).stdout
    m = re.search(r'Compression (\w+)', o); return m.group(1) if m else 'xz'
def build(n, abi, serial):
    d = f'{W}/d{n}'; run_(['unsquashfs', '-q', '-d', d, f'{REL}/desktop.sfs'], stdout=subprocess.DEVNULL)
    os.makedirs(f'{d}/etc/rudolf', exist_ok=True); os.makedirs(f'{d}/usr/share/rudolf', exist_ok=True)
    keys = open(f'{ISO}/trusted-keys.txt').read()
    open(f'{d}/etc/rudolf/rpk-keys', 'w').write(keys + f'# TEST ONLY (t160x_t.py): the stand-in system key for the test extensions\n{SYSPUB}\n')
    open(f'{d}/usr/share/rudolf/base-abi', 'w').write(abi + '\n')
    if FIX:   # this tree's extension tools in place of the release's
        os.makedirs(f'{d}/usr/bin', exist_ok=True); os.makedirs(f'{d}/usr/lib/rudolf', exist_ok=True); os.makedirs(f'{d}/sbin', exist_ok=True)
        run_(['gcc', '-O2', '-o', f'{d}/usr/bin/rpk', f'{ISO}/sys/rpk.c', '-lcrypto'])
        for s, t in (('usr/lib/rudolf/sysext', 'usr/lib/rudolf/sysext'), ('sbin/rudolf-init', 'sbin/rudolf-init'), ('usr/lib/rudolf/session', 'usr/lib/rudolf/session')):
            shutil.copy(f'{ISO}/sys/root/{s}', f'{d}/{t}'); os.chmod(f'{d}/{t}', 0o755)
    L = f'{W}/l{n}'; os.makedirs(L)
    for x in ('base', 'chromium', 'modules'): os.link(f'{REL}/{x}.sfs', f'{L}/{x}.sfs')
    run_(['mksquashfs', d, f'{L}/desktop.sfs', '-noappend', '-comp', comp(f'{REL}/desktop.sfs'), '-quiet'], stdout=subprocess.DEVNULL)
    run_([sys.executable, f'{ISO}/systools/verity.py', 'add', f'{L}/desktop.sfs', 'desktop'], stdout=subprocess.DEVNULL)
    shutil.rmtree(d)
    run_([sys.executable, f'{ISO}/systools/make_release.py', '--layers', L, '--kernel', f'{REL}/vmlinuz', '--out', f'{W}/o{n}', '--key', f'{W}/dev/rpk-signing.key',
          '--serial', str(serial), '--version', mv('version') + f'-ext{n}', '--chromium', mv('chromium'), '--kernel-version', mv('kernel')], stdout=subprocess.DEVNULL)
    return L, f'{W}/o{n}/channel', serial
B1 = build(1, ABI1, SER + 1); B2 = build(2, ABI2, SER + 2)
# ---- the extensions
def ext(name, ver, abi, key, files, out_name=None):
    d = f'{W}/p/{name}-{ver}'; shutil.rmtree(d, ignore_errors=True); os.makedirs(d)
    open(f'{d}/PKGINFO', 'w').write(f'name = {name}\nversion = {ver}\nkind = sysext\ntitle = Test {name}\nsummary = a test extension (t160x)\nlicense = MIT\nbase = {abi}\n')
    for f, c in files.items():
        os.makedirs(os.path.dirname(f'{d}/{f}'), exist_ok=True); open(f'{d}/{f}', 'w').write(c); os.chmod(f'{d}/{f}', 0o755)
    o = f'{W}/t/{out_name or name + "-" + ver}.rpk'; os.makedirs(f'{W}/t', exist_ok=True)
    run_([sys.executable, TOOL, 'build', d, o, '--key', f'{W}/{key}/rpk-signing.key'], stdout=subprocess.DEVNULL); return o
def hello(v, abi):
    return ext('hello', v, abi, 'sys', {'usr/bin/rudolf-hello': f'#!/bin/sh\necho "hello from the test extension {v}"\n',
               'usr/lib/rudolf/ext.d/hello.start': f'#!/bin/sh\necho "hello {v} started" > /run/rudolf/hello.mark\necho "hello {v} start script ran"\n'})
hello('1.0', ABI1); hello('2.0', ABI2)
ext('hang', '1.0', ABI1, 'sys', {'usr/bin/rudolf-hang': '#!/bin/sh\necho hang\n', 'usr/lib/rudolf/ext.d/hang.start': '#!/bin/sh\necho "hang: waiting forever"\nexec sleep 100000\n'})
ext('corrupt', '1.0', ABI1, 'sys', {'usr/bin/rudolf-corrupt': '#!/bin/sh\necho corrupt\n'})
ext('badsig', '1.0', ABI1, 'sys', {'usr/bin/rudolf-badsig': '#!/bin/sh\necho badsig\n'})
ext('badsig', '1.0', ABI1, 'other', {'usr/bin/rudolf-badsig': '#!/bin/sh\necho badsig\n'}, out_name='badsig-other')
# (rudolfd never runs: no heartbeat, the desktop never answers)
ext('breakdesk', '1.0', ABI1, 'sys', {'usr/bin/rudolf-breakdesk': '#!/bin/sh\necho breakdesk\n',
    'usr/lib/rudolf/ext.d/breakdesk.start': '#!/bin/sh\nmount --bind /bin/false /usr/bin/rudolfd && echo "breakdesk: rudolfd replaced by false"\n'})

def make_drive(path):
    """Install here's layout: the release's boot image (boot.iso) at the start, RUDOLFDATA as partition 3 at 128 MiB"""
    start = 262144
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc/rudolf')
    for s, (L, ch, serial) in (('a', B1), ('b', B2)):
        d = f'{T}/os/system/{s}'; os.makedirs(d)
        shutil.copy(f'{REL}/vmlinuz', f'{d}/vmlinuz')
        for n in ('base', 'chromium', 'desktop', 'modules'): shutil.copy(f'{L}/{n}.sfs', f'{d}/{n}.sfs')
        for n in ('manifest.txt', 'manifest.sig'): shutil.copy(f'{ch}/{n}', f'{d}/{n}')
        open(f'{d}/RELEASE', 'w').write(f"serial {serial}\nversion {mv('version')}-ext{'1' if s == 'a' else '2'}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n")
        open(f'{d}/complete', 'w').write('1\n')
    # build 2 waits in slot b as a downloaded update does, until the test makes it the one to start (next=b)
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    open(f'{T}/os/update.conf', 'w').write('url=https://dev.example/rudolf\ndevurl=https://dev.example/rudolf\nauto=0\n')
    open(f'{T}/etc/rudolf/extra-keys', 'w').write(f'# Developer mode (Settings > Updates): a developer\'s publisher key\n{DEVPUB}\n')
    shutil.copytree(f'{W}/t', f'{T}/t')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'wb') as f:
        f.truncate((start + DMB * 2048 + 2048) * 512)
        with open(f'{REL}/boot.iso', 'rb') as i: shutil.copyfileobj(i, f)
        f.seek(446); f.write(bytes(16))
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(510); f.write(b'\x55\xaa'); f.seek(start * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)

class TSer(Ser):   # the serial line, also written to a file (the start-up's own messages there: console=ttyS0)
    def __init__(s, path, logf): super().__init__(path); s.lf = open(logf, 'a')
    def read(s, t=1.0):
        o = super().read(t); s.lf.write(o); s.lf.flush(); return o

N = [0]
def boot(tag, extra_cmd=''):
    N[0] += 1
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf160x',
             '-kernel', f'{REL}/vmlinuz', '-append', f'console=ttyS0 loglevel=3 consoleblank=0 rudolf.debug=1 {extra_cmd}'.strip()]
    vm = sbvm.VM(f't160x{N[0]}{tag}', PARTS, [], secure=False, mem=MEM, extra=extra)
    vm.serlog = os.path.join(vm.dir, 'serial.txt')
    g = Guest.__new__(Guest); g.N = vm.name; g.s = TSer(vm.ser_path, vm.serlog); g.q = vm.q; g.s.read(0.3)
    time.sleep(20); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    out(f'--- start {N[0]} ({tag}):', 'up' if up else 'NOT UP')
    if not up: out('the serial line:', open(vm.serlog, errors='replace').read()[-4000:])
    return vm, g, up
def run(g, c, t=60):
    f = os.path.join(W, 'c160x.sh'); open(f, 'w').write(c + '\n'); g.put(f, '/tmp/c160x.sh', '644')
    o = g.sh('sh /tmp/c160x.sh 2>&1', t); out('$', c[:300].replace('\n', ' ; ')); out(o.rstrip()[-3000:]); return o
def desktop_up(g, secs=480):
    """the desktop answered (its heartbeat), and the session marked this start's extensions good"""
    o = run(g, f"i=0; while [ $i -lt {secs // 5} ]; do [ -e /run/rudolf/heartbeat ] && break; i=$((i+1)); sleep 5; done; "
               "[ -e /run/rudolf/heartbeat ] && echo DESKTOP-UP; sleep 25; ls /run/rudolf/ext/.good 2>/dev/null && echo MARKED-GOOD", secs + 60)
    return 'DESKTOP-UP' in o, o
def state(g): return run(g, "echo SLOT=$(cat /etc/rudolf/slot); cat /run/rudolf/ext/state; echo; cat /persist/os/system/state", 30)
def stop(vm, g):
    try: g.sh('sync', 60)
    except Exception: pass
    vm.stop(); time.sleep(3)
def health(g, pat): return run(g, f"grep -h '\"k\":\"extension\"' /persist/var/log/health.log | grep -E '{pat}' | tail -5", 30)
RPKI = 'rpk install-file --system'

make_drive(DRV)
vm = None
try:
  try:
        # ---- start 1: build 1 (slot a): the extensions installed
        vm, g, up = boot('install')
        ok('0. build 1 (the release with the TEST system key, Developer mode) starts from slot a', up and re.search(r'^SLOT=a$', state(g), re.M), 'not up')
        if not up: raise SystemExit
        o = run(g, f"cat /usr/share/rudolf/base-abi; for x in hello-1.0 hang-1.0 corrupt-1.0 badsig-1.0; do {RPKI} /persist/t/$x.rpk; echo rc=$?; done; "
                   f"rpk sysext-status; ls /usr/bin/rudolf-hello 2>&1", 120)
        ok('1. extensions signed by the (TEST) system key install for this base, in use after a restart', o.count('rc=0') == 4 and o.count('in use after the next restart') == 4
           and '"name":"hello","version":"1.0"' in o and '"state":"next"' in o and 'No such file' in o, o)
        o = run(g, f"{RPKI} /persist/t/hello-2.0.rpk; echo rc=$?; {RPKI} /persist/t/badsig-other.rpk; echo rc=$?", 60)
        ok('...one built for another base and one signed by another key are refused', o.count('rc=1') == 2 and 'another Rudolf OS base' in o and "release key" in o, o)
        # the changed file and the other key's signature, as anything that writes to the data partition could do
        run(g, "f=/persist/pkg/store/corrupt.rpk; s=$(stat -c %s $f); printf 'X' | dd of=$f bs=1 seek=$((s / 3)) conv=notrunc 2>/dev/null; "
               "cp /persist/t/badsig-other.rpk /persist/pkg/store/badsig.rpk; sync; md5sum /persist/pkg/store/*.rpk", 60)
        stop(vm, g)
        # ---- start 2: in use; the hanging start script stopped; the two broken ones left out
        vm, g, up = boot('inuse')
        o = state(g) + run(g, "rudolf-hello; cat /run/rudolf/hello.mark; cat /run/rudolf/ext/hello.log; grep -c . /proc/mounts; grep sysext /proc/mounts | cut -d' ' -f1-3", 60)
        ok('2. after the restart: hello in use, its program there and its start script run', up and 'hello from the test extension 1.0' in o and 'hello 1.0 started' in o
           and re.search(r'^hello\t1.0\tin-use', o, re.M), o)
        ok('3. ...laid over the base read-only (overlays on /usr), nothing written to the base layer', re.search(r'^sysext /usr overlay', o, re.M) and 'with extensions' in o, o)
        o2 = run(g, "touch /usr/bin/x-test 2>&1; echo rc=$?; ls /run/rudolf/ext/", 30)
        ok('4. /usr stays read-only with an extension in use', 'rc=1' in o2 or 'Read-only' in o2, o2)
        ok('5. a start script that never ends is stopped after its time limit, the extension stays in use, said in System health',
           re.search(r'^hang\t1.0\tin-use', o, re.M) and 'did not finish in 20 s' in health(g, 'hang'), o)
        ok('6. a changed extension file is left out, with its reason', re.search(r'^corrupt\t1.0\tleft-out\t.*no longer the signed one', o, re.M), o)
        ok('7. one signed by another key (its file replaced) is left out, with its reason', re.search(r'^badsig\t1.0\tleft-out\t.*(no longer the signed one|not signed by Rudolf OS)', o, re.M), o)
        h = health(g, 'corrupt|badsig'); st = run(g, "/usr/lib/rudolf/selftest 2>&1 | grep -E ' extensions '; rpk sysext-status", 300)
        ok('...each in the health log (System health) and the self-test, in plain words', 'left out the system extension corrupt 1.0' in h and 'left out the system extension badsig 1.0' in h
           and re.search(r'FAIL extensions - left out: .*corrupt 1.0 \(', st) and '"problem":' in st, h + st)
        up2, o = desktop_up(g)
        ok('8. ...and the computer started: the desktop came up, and this start\'s extensions were marked good', up2 and 'MARKED-GOOD' in o and 'hello 1.0' in run(g, 'cat /persist/pkg/sysext/good', 20), o)
        o = run(g, f"for x in hang corrupt badsig; do rpk remove --system $x; echo rc=$?; done; {RPKI} /persist/t/breakdesk-1.0.rpk; echo rc=$?; ls /persist/pkg/store /persist/pkg/db", 120)
        ok('...removed (hang, corrupt, badsig): nothing of them left; breakdesk installed', o.count('rc=0') == 4 and not re.search(r'\b(hang|corrupt|badsig)\.(rpk|info)', o), o)
        stop(vm, g)
        # ---- start 3: breakdesk stops the desktop
        vm, g, up = boot('breakdesk')
        o = state(g) + run(g, "cat /run/rudolf/ext/breakdesk.log; sleep 150; ls /run/rudolf/heartbeat 2>&1; cat /persist/pkg/sysext/trying", 260)
        ok('9. an extension that stops the desktop: in use, the desktop never answers, the start stays on trial', up and re.search(r'^breakdesk\t1.0\tin-use', o, re.M)
           and 'No such file' in o and re.search(r'^breakdesk 1.0$', o, re.M) and not re.search(r'^hello 1.0$', o.split('cat /persist/pkg/sysext/trying')[-1], re.M), o)
        stop(vm, g)   # (the person switches it off)
        # ---- start 4: left out, the rest in use; removed; then the update to build 2 (without hello's build)
        vm, g, up = boot('trial')
        o = state(g); up2, o2 = desktop_up(g)
        ok('10. the next start leaves it out ("the desktop did not come up"), keeps the others, and the desktop comes up', up and up2
           and re.search(r'^breakdesk\t1.0\tleft-out\tthe desktop did not come up', o, re.M) and re.search(r'^hello\t1.0\tin-use', o, re.M)
           and 'the desktop did not come up at the first start with it' in health(g, 'breakdesk'), o + o2)
        o = run(g, "rpk remove --system breakdesk; echo rc=$?; ls /persist/pkg/store /persist/pkg/db /persist/pkg/sysext/off 2>&1", 60)
        ok('11. ...removing it leaves nothing behind', 'rc=0' in o and 'breakdesk' not in o.split('rc=0')[-1], o)
        # the update: build 2 is in slot b (as a download leaves it), made the one to start next, on trial
        run(g, "printf 'good=a\\nprev=\\nnext=b\\ntries=2\\nfailed=\\n' > /persist/os/system/state; sync", 30)
        stop(vm, g)
        # ---- start 5: the new base, without hello's build for it
        vm, g, up = boot('update')
        o = state(g) + run(g, "cat /usr/share/rudolf/base-abi; rudolf-hello 2>&1; rpk sysext-status", 60); up2, o2 = desktop_up(g)
        o3 = run(g, "sleep 20; cat /persist/os/system/state; /usr/lib/rudolf/selftest 2>&1 | grep -E ' extensions '", 300)
        ok('12. an update of the base without the extension\'s build: the computer starts on the new base, the old build left out (built for another base)',
           up and re.search(r'^SLOT=b$', o, re.M) and ABI2 in o and re.search(r'^hello\t1.0\tleft-out\t.*another Rudolf OS base', o, re.M) and 'not found' in o, o)
        ok('13. ...the desktop comes up and the update is kept; System health and the self-test say why it is left out', up2 and re.search(r'^good=b$', o3, re.M)
           and 'another Rudolf OS base' in health(g, 'hello') and 'FAIL extensions - left out: hello 1.0' in o3, o2 + o3)
        o = run(g, f"{RPKI} /persist/t/hello-2.0.rpk; echo rc=$?", 60)
        stop(vm, g)
        # ---- start 6: with the right build
        vm, g, up = boot('rightbuild')
        o = state(g) + run(g, "rudolf-hello; cat /run/rudolf/hello.mark", 60); up2, o2 = desktop_up(g)
        ok('14. ...with the build for the new base installed, it is in use after a restart', up and up2 and re.search(r'^hello\t2.0\tin-use', o, re.M)
           and 'hello from the test extension 2.0' in o and re.search(r'^SLOT=b$', o, re.M), o + o2)
        o = run(g, "rudolf-update rollback; cat /persist/os/system/state", 60)
        stop(vm, g)
        # ---- start 7: rolled back to the old base
        vm, g, up = boot('rollback')
        o = state(g) + run(g, "rudolf-hello 2>&1", 30); up2, o2 = desktop_up(g)
        ok('15. Roll back system with an extension: the old base starts, the new base\'s build left out, the desktop comes up', up and up2 and re.search(r'^SLOT=a$', o, re.M)
           and re.search(r'^hello\t2.0\tleft-out\t.*another Rudolf OS base', o, re.M) and 'not found' in o, o + o2)
        stop(vm, g)
        # ---- start 8: Recovery mode (with the right build back: none used all the same)
        vm, g, up = boot('recovery', 'rudolf.recovery=1')
        o = state(g) + run(g, "rudolf-hello 2>&1; grep -c ' /usr overlay' /proc/mounts", 30)
        ok('16. Recovery mode starts without extensions, and says so', up and re.search(r'^start\tRecovery mode starts without system extensions', o, re.M) and 'not found' in o, o)
        stop(vm, g); vm = None
  except SystemExit: out('stopped early')
finally:
    try: vm and vm.stop()
    except Exception: pass
    shutil.rmtree(W, ignore_errors=True)
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
