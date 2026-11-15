# Rudolf OS 1.6.0 in QEMU (docs/plan-1.6.0.md, "Cellular and ARM64", stage A): the cellular work on a release's own
# kernel and layers (the newest beta, as t160), with this tree's scripts put into the running system (the RAM overlay
# takes them; nothing is written to the release's layers). UEFI without Secure Boot, no modem.
#   item 1 (Part 1): ModemManager with udev and no modem: not started by itself; started by hand it runs with udev, finds
#          no modem and logs nothing (no error, no warning); status says so; its plug-ins (QMI, MBIM, QRTR, AT) and FCC
#          unlock scripts are there, the unlock off; stopped again.
#   python3 cell60_q.py REL-DIR   (QEMU and SB_PARTS as for t160; root; QEMU_ACCEL=kvm where it can)
import os, sys, time, re, subprocess, shutil, struct, json
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE); ISO = os.path.dirname(QT)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent')
LOG = open('cell60.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:700] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 2000; DRV = os.path.abspath('cell60.img'); MEM = int(os.environ.get('MEM', '3072'))
SYSROOT = os.path.join(ISO, 'sys', 'root')

def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    d = f'{T}/os/system/a'; os.makedirs(d)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs', 'manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{d}/{n}')
    open(f'{d}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n"); open(f'{d}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as dd: shutil.copyfileobj(dd, f)
    os.remove(D)

def boot(tag):
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolfcell',
             '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1']
    vm = sbvm.VM('cell60' + tag, PARTS, [], secure=False, mem=MEM, extra=extra)
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    time.sleep(30); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    return vm, g, up

make_drive(DRV)
vm = None
try:
    vm, g, up = boot('a'); sh = lambda c, t=60: g.sh(c, t)
    ok('0. the release starts (' + mv('version') + ')', up, 'not up')
    if not up: raise SystemExit
    # this tree's modem script, in place of the release's (the RAM overlay)
    g.put(os.path.join(SYSROOT, 'usr/lib/rudolf/modem'), '/usr/lib/rudolf/modem')
    time.sleep(20)   # (the start-up's devices settled)
    o = sh("cat /run/rudolf/devices; pgrep -x ModemManager >/dev/null && echo MM-RUNNING || echo MM-NOT-RUNNING; ls /dev/cdc-wdm* /dev/wwan* 2>&1 | head -3; cat /run/rudolf/modem.log 2>&1 | tail -5")
    ok('1. devices through udev; no modem, so ModemManager was not started by itself', o.startswith('udev') and 'MM-NOT-RUNNING' in o, o)
    o = sh("ACTION=add MDEV=ttyS0 /usr/lib/rudolf/modem hotplug; sleep 1; pgrep -x ModemManager >/dev/null && echo MM-RUNNING || echo MM-NOT-RUNNING")
    ok("...a port that is not a modem's (a serial port) does not start it", 'MM-NOT-RUNNING' in o, o)
    o = sh("rm -f /run/rudolf/modem.log; /usr/lib/rudolf/modem start; echo rc=$?; sleep 5; pgrep -x ModemManager >/dev/null && echo MM-RUNNING; mmcli -L 2>&1; cat /run/rudolf/modem.log", 120)
    ok('2. started by hand: ModemManager runs with udev (not told of ports by Rudolf OS), on the system bus, finds no modem', 'rc=0' in o and 'MM-RUNNING' in o and 'ModemManager started (with udev)' in o and 'No modems were found' in o, o)
    log = sh("sleep 15; cat /run/rudolf/modem.log", 60)
    lines = [l for l in log.splitlines() if l.strip() and 'ModemManager started' not in l and l.strip() != 'cat /run/rudolf/modem.log']
    ok('3. ...and logs nothing at all at its warning level: no error, no warning (no "Invalid rule match")', not [l for l in lines if re.search(r'error|warn|invalid|critical|fail', l, re.I)] and 'Invalid rule match' not in log, log)
    o = sh("/usr/lib/rudolf/modem status")
    m = re.search(r'\{"running".*\}', o)
    try: st = json.loads(m.group(0)) if m else None
    except Exception: st = None
    ok('4. status: running, no modems, no module noted (QEMU has none)', st and st['running'] and st['modems'] == [] and st['notes'] == [], o)
    o = sh("ls /usr/lib/*/ModemManager/libmm-plugin-*.so | wc -l; ls /usr/lib/*/ModemManager/libmm-plugin-*.so | grep -c -E 'quectel|fibocom|sierra|telit|generic|qcom-soc|foxconn'; ls /usr/share/ModemManager/fcc-unlock.available.d | wc -l; ls /etc/ModemManager/fcc-unlock.d 2>/dev/null | wc -l; "
           "ldd $(command -v ModemManager) | grep -c -E 'libqmi|libmbim|libqrtr'; ls /usr/share/mobile-broadband-provider-info/serviceproviders.xml")
    n = [int(x) for x in re.findall(r'^(\d+)$', o, re.M)]
    ok('5. its plug-ins (Quectel, Fibocom, Sierra, Telit, generic AT...), QMI, MBIM and QRTR, the FCC scripts (none turned on) and the operators\' database', len(n) >= 5 and n[0] >= 20 and n[1] >= 6 and n[2] >= 5 and n[3] == 0 and n[4] == 3 and 'serviceproviders.xml' in o, o)
    o = sh("/usr/lib/rudolf/modem stop; sleep 2; pgrep -x ModemManager >/dev/null && echo MM-RUNNING || echo MM-NOT-RUNNING; tail -2 /run/rudolf/modem.log")
    ok('6. stopped again (as when the last modem goes)', 'MM-NOT-RUNNING' in o and 'ModemManager stopped' in o, o)
    o = sh("/usr/lib/rudolf/modem status")
    ok('7. ...status: not running, an empty list', '{"running":false,"modems":[],"notes":[]}' in o, o)
finally:
    try: vm and vm.stop()
    except Exception: pass
    for f in (DRV,):
        try: os.remove(f)
        except OSError: pass
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
