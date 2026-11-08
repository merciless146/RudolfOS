# Rudolf OS 1.5.4 in QEMU: the Rudolf OS Library on the release itself (its rpk, rudolf-update and health log), with
# its system in slot a (as t150). A throwaway "Library" key signs a test Library, served over HTTP inside the guest
# (busybox httpd: rpk fetches it with wget, as from GitHub Pages) and as a disc repository; rpk is told to trust that
# key (RPK_LIBRARY_KEYS), as the real system trusts the Library's key in /etc/rudolf/library-keys. Checks:
#   - a Library package installs over the network, and from the disc's repository, and is updated;
#   - a Library-signed package that holds a program is refused (one replacing a system package: host test rpklib_t.sh);
#   - a release list (system update) signed with the Library key is refused by rudolf-update, the real one is not;
#   - each Library install, update, removal and refusal has a line in the health log.
#   python3 t152_t.py REL-DIR     (SB_PARTS and QEMU as for t150; root)
import os, sys, time, re, subprocess, shutil, struct, tarfile, base64, tempfile
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE); ISO = os.path.dirname(QT)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); LOG = open('t152.txt', 'w'); R = []
TOOL = os.path.join(ISO, 'pkgtools', 'rpk-tool.py')
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:600] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 1500; DRV = os.path.abspath('int152.img'); MEM = int(os.environ.get('MEM', '3072'))

def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf/Downloads'); os.makedirs(f'{T}/etc')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    a = f'{T}/os/system/a'; os.makedirs(a)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs', 'manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{a}/{n}')
    open(f'{a}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n")
    open(f'{a}/complete', 'w').write('1\n'); open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)

# ---- the test Library (throwaway keys, in a temporary folder; nothing is kept)
W = tempfile.mkdtemp(prefix='t152-')
def tool(*a): subprocess.run(['python3', TOOL] + list(a), check=True, stdout=subprocess.DEVNULL)
def pkg(name, ver, kind, key, outp):
    d = f'{W}/p/{name}-{ver}'; os.makedirs(f'{d}/www', exist_ok=True)
    info = f'name = {name}\nversion = {ver}\nkind = {kind}\ntitle = {name}\nsummary = t152\nlicense = MIT\n'
    open(f'{d}/www/hello.txt', 'w').write(f'{name} {ver}\n')
    if kind == 'cli':
        os.makedirs(f'{d}/bin', exist_ok=True); open(f'{d}/bin/{name}', 'w').write('#!/bin/sh\necho hi\n'); os.chmod(f'{d}/bin/{name}', 0o755); info += f'commands = {name}\n'
    open(f'{d}/PKGINFO', 'w').write(info); tool('build', d, outp, '--key', key)
def make_library():
    tool('keygen', f'{W}/lib'); K = f'{W}/lib/rpk-signing.key'
    B = f'{W}/t152'; os.makedirs(f'{B}/web/lib'); os.makedirs(f'{B}/web/lib2'); os.makedirs(f'{B}/disc')
    pkg('t152-hello', '1.0', 'addon', K, f'{B}/web/lib/t152-hello.rpk')
    pkg('t152-evil', '1.0', 'cli', K, f'{B}/web/lib/t152-evil.rpk')
    tool('index', f'{B}/web/lib', '--key', K, '--serial', '1')
    pkg('t152-hello', '1.1', 'addon', K, f'{B}/web/lib2/t152-hello.rpk')
    tool('index', f'{B}/web/lib2', '--key', K, '--serial', '2')
    pkg('t152-disc', '1.0', 'addon', K, f'{B}/disc/t152-disc.rpk')
    tool('index', f'{B}/disc', '--key', K, '--serial', '1')
    shutil.copy(f'{W}/lib/rpk-signing.pub', f'{B}/lib.pub')
    # the release's own list, signed with the Library key instead (a system update the Library must not be able to give)
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
    k = Ed25519PrivateKey.from_private_bytes(base64.b64decode(open(K).read().strip()))
    shutil.copy(f'{REL}/manifest.txt', f'{B}/manifest.txt'); shutil.copy(f'{REL}/manifest.sig', f'{B}/manifest.sig')
    open(f'{B}/lib.sig', 'wb').write(k.sign(open(f'{B}/manifest.txt', 'rb').read()))
    with tarfile.open(f'{W}/t152.tgz', 'w:gz') as t: t.add(B, arcname='t152')
    return f'{W}/t152.tgz'

EXTRA = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf152',
         '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1']
def boot(tag):
    vm = sbvm.VM('t152' + tag, PARTS, [], secure=False, mem=MEM, extra=EXTRA)
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    time.sleep(45); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    return vm, g, up

TGZ = make_library(); make_drive(DRV); vm = None
ENV = 'export RPK_LIBRARY_KEYS=/tmp/t152/lib.pub RPK_SOURCES=/tmp/t152/sources RPK_DISC=/tmp/t152/nodisc; '
try:
    vm, g, up = boot('a')
    ok('1. Rudolf OS starts', up)
    if not up: raise SystemExit
    sh = lambda c, t=60: g.sh(c, t)
    out(g.put(TGZ, '/tmp/t152.tgz', '644'))
    o = sh("cd /tmp && tar xzf t152.tgz && ls /tmp/t152 && (busybox httpd -p 127.0.0.1:8152 -h /tmp/t152/web &) && sleep 1 && echo http://127.0.0.1:8152/lib > /tmp/t152/sources && wget -q -O - http://127.0.0.1:8152/lib/index.txt | head -2")
    ok('2. the test Library is served over HTTP inside the guest', 'rpk-index 1' in o, o)
    hl0 = int(sh("wc -l < /persist/var/log/health.log").strip().split()[-1] or 0)
    s = sh(ENV + 'rpk refresh >/dev/null 2>&1; rpk status')
    ok('3. rpk reads it as the Library (its key trusted, marked library)', '"loc":"http://127.0.0.1:8152/lib","type":"web","ok":true,"library":true' in s, s)
    o = sh(ENV + 'rpk install t152-hello 2>&1; echo RC=$?; cat /pkg/t152-hello/www/hello.txt; grep -x "signed = library" /persist/pkg/db/t152-hello.info', 120)
    ok('4. a Library package for the desktop installs over the network, is served, and is recorded as the Library\'s', 'RC=0' in o and 't152-hello 1.0' in o and 'signed = library' in o, o)
    o = sh(ENV + 'rpk install t152-evil 2>&1; echo RC=$?; [ -e /persist/pkg/store/t152-evil.rpk ] && echo KEPT=yes || echo KEPT=no; command -v t152-evil >/dev/null && echo CMD=yes || echo CMD=no', 120)
    ok('5. a Library package that holds a program is refused (not stored, no command)', 'RC=1' in o and 'can only hold files for the desktop' in o and re.search(r'^KEPT=no', o, re.M) and re.search(r'^CMD=no', o, re.M), o)
    o = sh(ENV + 'echo http://127.0.0.1:8152/lib2 > /tmp/t152/sources; rpk update 2>&1; echo RC=$?; cat /pkg/t152-hello/www/hello.txt', 180)
    ok('6. a newer Library version updates it', 'RC=0' in o and 't152-hello 1.1' in o, o)
    o = sh("export RPK_LIBRARY_KEYS=/tmp/t152/lib.pub RPK_SOURCES=/dev/null RPK_DISC=/tmp/t152/disc; rpk install t152-disc 2>&1; echo RC=$?; cat /pkg/t152-disc/www/hello.txt", 120)
    ok("7. a Library package installs from the disc's repository", 'RC=0' in o and 't152-disc 1.0' in o, o)
    o = sh(ENV + 'rpk remove t152-hello 2>&1; echo RC=$?; ls /pkg/t152-hello 2>&1 | head -1', 60)
    ok('8. it is removed', 'RC=0' in o and 'Removed t152-hello' in o, o)
    o = sh('cd /tmp/t152; rudolf-update verify-manifest manifest.txt lib.sig; echo RC1=$?; rudolf-update verify-manifest manifest.txt manifest.sig; echo RC2=$?')
    ok('9. a system update signed with the Library key is refused; the real release list is accepted', 'RC1=1' in o and 'not signed by Rudolf OS' in o and 'RC2=0' in o and f"serial {mv('serial')}" in o, o)
    o = sh(f"sed -n '{hl0 + 1},$p' /persist/var/log/health.log | grep '\"k\":\"library\"' | cut -c1-220")
    out(o)
    ok('10. health log: the network install, the update, the disc install, the removal and the refusal', all(x in o for x in ('installed t152-hello 1.0', 'updated t152-hello from 1.0 to 1.1', 'installed t152-disc 1.0', 'removed t152-hello 1.1', 'refused t152-evil 1.0')), o)
finally:
    try: vm and vm.stop()
    except Exception: pass
    shutil.rmtree(W, ignore_errors=True)
try: os.remove(DRV)
except OSError: pass
out(f'{sum(R)}/{len(R)} passed')
