# Rudolf OS 1.6.1 in QEMU (docs/plan-1.6.1.md Part 19): game mode on the real release (its kernel, base, browser and drivers
# layers; this tree's gamemode, power and rudolf-init in a desktop layer of a test build, FIX=1 as t160x_t.py does), started
# as t160x_t.py starts it (UEFI, Secure Boot off, rudolf.debug=1 for the serial shell). What only a real kernel and the
# release's own processes can show: the cpu controller turned on for the desktop's group and off again (the kernel refuses
# it while a real-time process is in a group), cpu.weight lowered and put back, the power settings asked for game=1 and
# back (QEMU has no frequency driver: the energy preference is a clean no-op), and the watcher putting everything back by
# itself when a full screen is no longer said (the page or the native desktop stopped) and when a stand-in game crashes.
#   1   before: off, the desktop's group as the release leaves it (its cpu controller, its weight)
#   2-3 a Linux game full screen (gamemode on page linux): on, the desktop slowed (cpu on, weight 20), game=1, no preference
#   4   not said again for 6 s: everything back by itself, as it was before
#   5-6 a stand-in game (a process) full screen on the native desktop's word, then killed: off within seconds, as before
#   7   the desktop came up, and still answers after
#   python3 t161g_t.py REL-DIR      (REL-DIR with the release's boot.iso; SB_PARTS and QEMU as for t160; root; QEMU_ACCEL=kvm; FIX=1)
import os, sys, time, re, subprocess, shutil, struct, hashlib
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE); ISO = os.path.dirname(QT)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); FIX = os.environ.get('FIX') == '1'; 
PARTS = os.environ.get('SB_PARTS', '/nonexistent'); LOG = open('t161g.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[-1500:] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 3400; DRV = os.path.abspath('int161g.img'); MEM = int(os.environ.get('MEM', '3072')); W = os.path.abspath('w161g')
TOOL = f'{ISO}/pkgtools/rpk-tool.py'; KV = mv('kernel')
shutil.rmtree(W, ignore_errors=True); os.makedirs(W)
def run_(a, **k): return subprocess.run(a, check=True, **k)
for k in ('dev', 'sys'): run_([sys.executable, TOOL, 'keygen', f'{W}/{k}'], stdout=subprocess.DEVNULL)
DEVPUB = open(f'{W}/dev/rpk-signing.pub').read().strip(); SYSPUB = open(f'{W}/sys/rpk-signing.pub').read().strip()
ABI = hashlib.sha256(b'rudolf test base game').hexdigest(); SER = int(mv('serial'))
def comp(sfs):
    o = subprocess.run(['unsquashfs', '-s', sfs], capture_output=True, text=True).stdout
    m = re.search(r'Compression (\w+)', o); return m.group(1) if m else 'xz'
def build():
    d = f'{W}/d'; run_(['unsquashfs', '-q', '-d', d, f'{REL}/desktop.sfs'], stdout=subprocess.DEVNULL)
    for x in ('etc/rudolf', 'usr/share/rudolf', 'usr/bin', 'usr/lib/rudolf', 'sbin'): os.makedirs(f'{d}/{x}', exist_ok=True)
    open(f'{d}/etc/rudolf/rpk-keys', 'w').write(open(f'{ISO}/trusted-keys.txt').read() + f'# TEST ONLY (t161g_t.py): the stand-in system key\n{SYSPUB}\n')
    open(f'{d}/usr/share/rudolf/base-abi', 'w').write(ABI + '\n')
    if FIX:
        run_(['gcc', '-O2', '-o', f'{d}/usr/bin/rpk', f'{ISO}/sys/rpk.c', '-lcrypto'])
        for s in ('usr/lib/rudolf/gamemode', 'usr/lib/rudolf/power', 'sbin/rudolf-init'):
            if os.path.exists(f'{ISO}/sys/root/{s}'): shutil.copy(f'{ISO}/sys/root/{s}', f'{d}/{s}'); os.chmod(f'{d}/{s}', 0o755)
    L = f'{W}/l'; os.makedirs(L)
    for x in ('base', 'chromium', 'modules'): os.link(f'{REL}/{x}.sfs', f'{L}/{x}.sfs')
    run_(['mksquashfs', d, f'{L}/desktop.sfs', '-noappend', '-comp', comp(f'{REL}/desktop.sfs'), '-quiet'], stdout=subprocess.DEVNULL)
    run_([sys.executable, f'{ISO}/systools/verity.py', 'add', f'{L}/desktop.sfs', 'desktop'], stdout=subprocess.DEVNULL)
    shutil.rmtree(d)
    run_([sys.executable, f'{ISO}/systools/make_release.py', '--layers', L, '--kernel', f'{REL}/vmlinuz', '--out', f'{W}/o', '--key', f'{W}/dev/rpk-signing.key',
          '--serial', str(SER + 1), '--version', mv('version') + '-gm', '--chromium', mv('chromium'), '--kernel-version', KV], stdout=subprocess.DEVNULL)
    return L, f'{W}/o/channel', SER + 1
B = build()
def make_drive(path):
    start = 262144
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc/rudolf')
    L, ch, serial = B; d = f'{T}/os/system/a'; os.makedirs(d)
    shutil.copy(f'{REL}/vmlinuz', f'{d}/vmlinuz')
    for n in ('base', 'chromium', 'desktop', 'modules'): shutil.copy(f'{L}/{n}.sfs', f'{d}/{n}.sfs')
    for n in ('manifest.txt', 'manifest.sig'): shutil.copy(f'{ch}/{n}', f'{d}/{n}')
    open(f'{d}/RELEASE', 'w').write(f"serial {serial}\nversion {mv('version')}-gm\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {KV}\nnotes test\n")
    open(f'{d}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    open(f'{T}/os/update.conf', 'w').write('url=https://dev.example/rudolf\ndevurl=https://dev.example/rudolf\nauto=0\n')
    open(f'{T}/etc/rudolf/extra-keys', 'w').write(f'# Developer mode: a developer\'s publisher key\n{DEVPUB}\n')
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

class TSer(Ser):
    def __init__(s, path, logf): super().__init__(path); s.lf = open(logf, 'a')
    def read(s, t=1.0):
        o = super().read(t); s.lf.write(o); s.lf.flush(); return o
N = [0]
def boot(tag, extra_cmd=''):
    N[0] += 1
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf161g',
             '-kernel', f'{REL}/vmlinuz', '-append', f'console=ttyS0 loglevel=3 consoleblank=0 rudolf.debug=1 {extra_cmd}'.strip()]
    vm = sbvm.VM(f't161g{N[0]}{tag}', PARTS, [], secure=False, mem=MEM, extra=extra)
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
    f = os.path.join(W, 'c161g.sh'); open(f, 'w').write(c + '\n'); g.put(f, '/tmp/c161g.sh', '644')
    o = g.sh('sh /tmp/c161g.sh 2>&1', t); out('$', c[:300].replace('\n', ' ; ')); out(o.rstrip()[-3000:]); return o
def desktop_up(g, secs=480):
    o = run(g, f"i=0; while [ $i -lt {secs // 5} ]; do [ -e /run/rudolf/heartbeat ] && break; i=$((i+1)); sleep 5; done; [ -e /run/rudolf/heartbeat ] && echo DESKTOP-UP", secs + 60)
    return 'DESKTOP-UP' in o, o
def stop(vm, g):
    try: g.sh('sync', 60)
    except Exception: pass
    vm.stop(); time.sleep(3)
GM = '/usr/lib/rudolf/gamemode'
CGS = "echo CTL=$(cat /sys/fs/cgroup/cgroup.subtree_control); echo W=$(cat /sys/fs/cgroup/desktop/cpu.weight 2>/dev/null); echo GAME=$(sed -n 's/^game=//p' /run/rudolf/power-now); echo EPP=$(cat /sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference 2>/dev/null); ls /run/rudolf/game 2>/dev/null | tr '\\n' ' '; echo"
def kv(o, k): m = re.findall(rf'^{k}=(.*)$', o, re.M); return m[-1].strip() if m else None
make_drive(DRV)
vm = None
try:
  try:
        vm, g, up = boot('game')
        if not up: ok('0. the test build starts', False, 'not up'); raise SystemExit
        up2, o0 = desktop_up(g)
        o = run(g, f"{GM} status; echo; " + CGS, 60); ctl0, w0 = kv(o, 'CTL'), kv(o, 'W')
        ok('1. before: game mode off; the desktop\'s group as the release leaves it', up2 and '"on":false' in o and ctl0 is not None, o)
        o = run(g, f"{GM} on page linux; echo; " + CGS + "; grep -c . /sys/fs/cgroup/desktop/cgroup.procs", 60)
        ok('2. a Linux game full screen: on, the desktop slowed: the cpu controller on for its group, cpu.weight 20 (the kernel took it)', '"on":true' in o and '"throttled":true' in o and 'cpu' in (kv(o, 'CTL') or '') and kv(o, 'W') == '20', o)
        ok('3. ...the power settings asked for game=1; no energy preference in QEMU (a clean no-op)', kv(o, 'GAME') == '1' and not kv(o, 'EPP'), o)
        o = run(g, "sleep 10; " + CGS + f"; {GM} status", 60)
        ok('4. not said again for 6 s (the page stopped): everything back by itself, as it was before', kv(o, 'GAME') == '0' and kv(o, 'W') == w0 and kv(o, 'CTL') == ctl0 and '"on":false' in o, o)
        o = run(g, f"sleep 1000 & P=$!; echo PID=$P; for i in 1 2 3; do {GM} on native linux $P >/dev/null; sleep 2; done; " + CGS + "; kill -9 $P; wait $P 2>/dev/null; sleep 4; echo AFTER; " + CGS + "; cat /run/rudolf/game/src.* /run/rudolf/game/watch/pid 2>&1; ls -d /proc/$P 2>&1; head -3 /proc/$P/status 2>&1; ps | grep -c '[g]amemode watch\\|[w]atch_'; date +%s", 90)
        # (wait: BusyBox's sleep returns at once when a child of the shell dies, so without reaping the killed game first the
        #  4 s were no time at all, and game mode was looked at before its watcher's next round: found 2026-11-21)
        a, b2 = o.split('AFTER')[0], o.split('AFTER')[-1]
        ok('5. a stand-in game full screen on the native desktop\'s word: on, slowed', kv(a, 'W') == '20' and kv(a, 'GAME') == '1', o)
        ok('6. ...the game crashes (killed): off within seconds, everything as before', kv(b2, 'W') == w0 and kv(b2, 'CTL') == ctl0 and kv(b2, 'GAME') == '0', o)
        o = run(g, "rm -f /run/rudolf/heartbeat; sleep 20; ls /run/rudolf/heartbeat && echo STILL-UP", 60)
        ok('7. the desktop still answers after', 'STILL-UP' in o, o)
        stop(vm, g); vm = None
  except SystemExit: out('stopped early')
finally:
    try: vm and vm.stop()
    except Exception: pass
    shutil.rmtree(W, ignore_errors=True)
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
