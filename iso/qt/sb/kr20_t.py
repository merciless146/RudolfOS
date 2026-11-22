# Rudolf Kryptonite (2.0, docs/plan-2.0.md "The newest kernel"): a test build's kernel (Linux 7.3.y with this tree's
# kconfig.sh, built by systools/testkernel.sh) in QEMU on the newest beta release's layers, next to the release's own
# kernel on the same machine. Installed as in kt161_t.py (slot a on an NVMe drive with RUDOLFDATA, UEFI with Secure Boot
# off: the Secure Boot path is recovery_sb_t.sh's, with test keys), X11 as today. Tests only.
#   python3 kr20_t.py REL-DIR KERNEL-DIR        (root; SB_PARTS and QEMU as for kt161_t.py; QEMU_ACCEL=kvm where it can)
# KERNEL-DIR holds vmlinuz and modules.sfs. Each kernel is started on a new drive for Standard and for Native (the
# first start: the first-time setup, then started again as an installed computer is). Checks, for the test kernel:
#   the kernel is the test build's; modules are signed and enforced; the desktop reaches ready on Standard, and on Native
#   with Native still the engine (no fall-back to Standard); the self-test fails nothing the release's kernel passes; no
#   kernel warnings (BUG, Oops, WARNING, call traces) the release's kernel does not have; no firmware the kernel asks
#   for and does not find that the release's kernel finds; the modules loaded are the release's (each one missing named).
# Environment: KV (the version expected, 7.3.y), ENGINES (standard native), MEM (MiB, 3400). Screenshots and logs in kr20/.
import os, sys, time, re, subprocess, shutil, struct, json
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE); ISO = os.path.dirname(QT)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); KDIR = os.path.abspath(sys.argv[2])
PARTS = os.environ.get('SB_PARTS', '/nonexistent'); KV = os.environ.get('KV', '')
ENGINES = os.environ.get('ENGINES', 'standard native').split(); MEM = int(os.environ.get('MEM', '3400'))
W = os.path.abspath('kr20'); shutil.rmtree(W, ignore_errors=True); os.makedirs(W)
LOG = open(f'{W}/kr20.txt', 'w'); T0 = time.time()
def out(*a, quiet=False):
    s = f'[{int(time.time() - T0):5d}s] ' + ' '.join(str(x) for x in a); LOG.write(s + '\n'); LOG.flush()
    if not quiet: print(s[:800], flush=True)
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 6000

def make_drive(path, kernel, engine):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    d = f'{T}/os/system/a'; os.makedirs(d)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs'):
        src = f'{REL}/{n}'
        if kernel and n in ('vmlinuz', 'modules.sfs'): src = f'{kernel}/{n}'
        shutil.copy(src, f'{d}/{n}')
    for n in ('manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{d}/{n}')
    open(f'{d}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n"); open(f'{d}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    if engine == 'native': open(f'{T}/etc/rudolf-desktop-engine', 'w').write('native\n')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)

INIT_JS = "(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};return 1})()"
def boot(tag, drv, kernel, append='', secs=300):
    extra = ['-drive', f'file={drv},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf20',
             '-kernel', f'{kernel or REL}/vmlinuz', '-append', f'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1 {append}'.strip()]
    vm = sbvm.VM('kr20' + tag, PARTS, [], secure=False, mem=MEM, extra=extra); t = time.time()
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    up = False; end = time.time() + secs; seen = ''
    while time.time() < end:
        try:
            o = g.sh('echo ready', 8); seen = (seen + o)[-200000:]
            if 'ready' in o: up = True; break
        except OSError: pass
        time.sleep(2)
    out(f'start {tag}: serial shell {"up" if up else "NOT up"} after {time.time() - t:.0f} s')
    if not up:
        try: vm.shot(f'{tag}-notup')
        except Exception: pass
        open(f'{W}/serial-{tag}.txt', 'w').write(seen); out(f'{tag}: the serial line said (last lines):\n' + '\n'.join(seen.replace('\r', '').splitlines()[-80:]))
    return vm, g, up
DIAG = [('pad', 'rudolf.pad=' + 'x' * 60), ('ttyS0', 'console=ttyS0,115200'),
        ('serial', 'console=ttyS0,115200 ignore_loglevel earlyprintk=serial,ttyS0,115200')]
FINDINGS = []
def diagnose(tag, drv, kernel):
    """a start that did not reach the serial shell, again: with a longer command line of no meaning (does the place of things
    in memory matter?), with the serial line as the console, and with the kernel's messages on it. Returns the options of
    the first that starts, so the checks go on with it (the finding is reported)."""
    first = None
    for name, app in DIAG:
        vm, g, up = boot(f'{tag}{name}', drv, kernel, app, 240)
        if up:
            o = run(g, "uname -r; cat /proc/cmdline; dmesg | grep -iE 'ttyS|8250|serial|console|initramfs|Unpacking|corrupt' | head -30", 60)
            out(f'{tag}: started with {app!r}:\n{o[-3000:]}')
        FINDINGS.append(f'{tag}: {"starts" if up else "does not start"} with {app!r}')
        stop(vm, g)
        if up and first is None: first = app
    return first
LOOK = r'''
echo "UNAME $(uname -r)"
echo "ENGINE $(cat /run/rudolf/desktop-engine 2>/dev/null) (chosen: $(cat /run/rudolf/engine-choice 2>/dev/null))"
echo "SIGENF $(cat /sys/module/module/parameters/sig_enforce 2>/dev/null)"
echo "LOCKDOWN $(cat /sys/kernel/security/lockdown 2>/dev/null)"
for p in Xorg chrome rudolf-native rudolfd rudolf-display; do n=0; for c in /proc/[0-9]*/comm; do [ "$(cat $c 2>/dev/null)" = "$p" ] && n=$((n+1)); done; echo "PROC $p $n"; done
cut -d' ' -f1 /proc/modules | sort | sed 's/^/MOD /'
ls /sys/module | sed 's/^/SYSMOD /'
dmesg | grep -E 'BUG:|Oops|WARNING:|Call Trace|general protection|kernel panic|segfault' | sed 's/^\[[ 0-9.]*\] //' | head -40 | sed 's/^/WARN /'
dmesg | grep -E 'Direct firmware load for .* failed|firmware: failed to load' | sed 's/.*load for \([^ ]*\).*/\1/; s/.*failed to load \([^ ]*\).*/\1/' | sort -u | sed 's/^/FWMISS /'
dmesg | grep -iE 'module verification failed|Loading of unsigned module|key was rejected|Lockdown:' | head -10 | sed 's/^/SIG /'
/usr/lib/rudolf/selftest 2>&1 | sed 's/^/ST /'
'''
def look(o):
    r = {'uname': '', 'engine': '', 'sigenf': '', 'procs': {}, 'mods': set(), 'sysmod': set(), 'warn': [], 'fwmiss': set(), 'sig': [], 'st': {}}
    for l in o.splitlines():
        if l.startswith('UNAME '): r['uname'] = l[6:].strip()
        elif l.startswith('ENGINE '): r['engine'] = l[7:].split()[0] if l[7:].split() else ''
        elif l.startswith('SIGENF '): r['sigenf'] = l[7:].strip()
        elif l.startswith('PROC '): p = l.split(); r['procs'][p[1]] = int(p[2]) if len(p) > 2 and p[2].isdigit() else 0
        elif l.startswith('MOD '): r['mods'].add(l[4:].strip())
        elif l.startswith('SYSMOD '): r['sysmod'].add(l[7:].strip())
        elif l.startswith('WARN '): r['warn'].append(l[5:].strip())
        elif l.startswith('FWMISS '): r['fwmiss'].add(l[7:].strip())
        elif l.startswith('SIG '): r['sig'].append(l[4:].strip())
        elif l.startswith('ST '):
            m = re.match(r'ST (PASS|FAIL|SKIP) (\S+) - (.*)', l)
            if m: r['st'][m.group(2)] = (m.group(1), m.group(3)[:200])
    return r

APP = {}
def one(name, kernel, engine):
    drv = os.path.join(W, f'{name}-{engine}.img'); make_drive(drv, kernel, engine); app = APP.get(name, '')
    vm, g, up = boot(f'{name}{engine}0', drv, kernel, app)   # the first start: the first-time setup
    if not up:
        stop(vm, g); app = diagnose(f'{name}{engine}', drv, kernel)
        if app is None: return {'up': False}
        APP[name] = app; FINDINGS.append(f'{name}: the checks go on with {app!r} on the command line')
        vm, g, up = boot(f'{name}{engine}0', drv, kernel, app)
        if not up: stop(vm, g); return {'up': False}
    if up:
        wait_desktop(g); g.js(INIT_JS, 60); run(g, 'sleep 20; cat /run/rudolf/desktop-engine', 40)
    stop(vm, g)
    vm, g, up = boot(f'{name}{engine}1', drv, kernel, app)
    if not up: stop(vm, g); return {'up': False}
    d = wait_desktop(g); run(g, 'sleep 30', 50)
    r = look(run(g, LOOK, 400)); r['up'] = True; r['desktop'] = d
    try: vm.shot(f'{name}-{engine}')
    except Exception as e: out('screenshot:', e)
    stop(vm, g)
    try: os.remove(drv)
    except OSError: pass
    out(f'{name} {engine}: {r["uname"]}, desktop {"ready" if d else "NOT ready"}, engine {r["engine"]}, {len(r["mods"])} modules, '
        f'self-test {sum(1 for v in r["st"].values() if v[0] == "PASS")} pass {sum(1 for v in r["st"].values() if v[0] == "FAIL")} fail')
    return r

P = F = 0; LINES = []
def ok(name, cond, detail=''):
    global P, F
    if cond: P += 1
    else: F += 1
    LINES.append(f'{"PASS" if cond else "FAIL"} {name}' + (f' - {detail}' if detail and not cond else ''))
    out(LINES[-1])

try:
    for engine in ENGINES:
        rel = one('release', '', engine); new = one('test', KDIR, engine)
        e = engine.capitalize()
        ok(f'{e}: the test kernel starts as the release kernel does (no start-up options added)', not APP.get('test'), f'only with {APP.get("test")!r}' if APP.get('test') else '')
        if not new.get('up'): ok(f'{e}: the test kernel starts (the serial shell)', False, 'not up'); continue
        ok(f'{e}: the test kernel is the one started ({new["uname"]})', new['uname'].startswith(KV) and new['uname'].endswith('-rudolf-test'), new['uname'])
        ok(f'{e}: modules signed and enforced (sig_enforce Y, nothing refused)', new['sigenf'] == 'Y' and not new['sig'], f'{new["sigenf"]} {new["sig"]}')
        ok(f'{e}: the desktop reaches ready (the first heartbeat)', new.get('desktop'), 'no /run/rudolf/bootok')
        if engine == 'native':
            ok('Native: Native is the engine (no fall-back to Standard), rudolf-native runs', new['engine'] == 'native' and new['procs'].get('rudolf-native', 0) > 0, f'{new["engine"]} {new["procs"]}')
        else:
            ok('Standard: Standard is the engine, Chromium runs', new['engine'] in ('standard', 'chromium') and new['procs'].get('chrome', 0) > 0, f'{new["engine"]} {new["procs"]}')
        rf = {k for k, v in rel.get('st', {}).items() if v[0] == 'FAIL'}; nf = {k: v for k, v in new['st'].items() if v[0] == 'FAIL'}
        extra = {k: v for k, v in nf.items() if k not in rf}
        ok(f'{e}: the self-test fails nothing the release kernel passes ({len(new["st"])} checks; release fails {sorted(rf) or "none"})', len(new['st']) > 10 and not extra, json.dumps(extra) if extra else f'{len(new["st"])} checks')
        rw = {re.sub(r'\d+', 'N', x) for x in rel.get('warn', [])}; nw = [x for x in new['warn'] if re.sub(r'\d+', 'N', x) not in rw]
        ok(f'{e}: no kernel warnings the release kernel does not have', not nw, ' | '.join(nw[:8]))
        fm = sorted(new['fwmiss'] - rel.get('fwmiss', set()))
        ok(f'{e}: no firmware missing that the release kernel finds', not fm, ' '.join(fm))
        miss = sorted(m for m in rel.get('mods', set()) - new['mods'] if m not in new['sysmod']); more = sorted(new['mods'] - rel.get('mods', set()))
        out(f'{e}: modules loaded only with the release kernel: {miss}; only with the test kernel: {more}')
        ok(f'{e}: every module the release kernel loads is loaded (or built in)', not miss, ' '.join(miss))
finally:
    print('==== SUMMARY kr20')
    for l in FINDINGS: print('FINDING', l)
    for l in LINES: print(l)
    print(f'{P}/{P + F} passed'); LOG.write('\n'.join(LINES) + f'\n{P}/{P + F} passed\n'); LOG.close()
sys.exit(0 if F == 0 and P > 0 else 1)
