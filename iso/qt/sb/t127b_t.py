# Rudolf OS 1.2.7 stage B in QEMU: the new USB Wi-Fi drivers and their firmware (UEFI, Secure Boot off).
#   python3 t127b_t.py DISC.iso
# DISC.iso: a test disc (mkdisc.sh with RUDOLF_UKI_TEST_CMDLINE=rudolf.debug=1: a serial shell).
# QEMU has no USB Wi-Fi device, so this checks what a plugged-in adapter would need: each driver is in the modules
# layer and loads, it claims the USB IDs of a known adapter (modinfo), and every firmware file it names is there.
import os, sys, time, re
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
ISO = os.path.abspath(sys.argv[1])
PARTS = os.environ['SB_PARTS']; LOG = open('t127b.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:300] if d != '' else ''))
def last(t): return t.strip().split('\n')[-1].strip() if t.strip() else ''
# driver, a known adapter's USB ID (vendor:product), the firmware it loads
DRV = [('ath9k_htc', '0cf3:9271', 'TP-Link TL-WN722N v1 (AR9271)', ['ath9k_htc/htc_9271-1.4.0.fw', 'htc_9271.fw', 'htc_7010.fw']),
       ('mt7601u', '148f:7601', 'MT7601U nano adapters', ['mt7601u.bin']),
       ('mt76x0u', '148f:7610', 'MT7610U AC600 adapters', ['mediatek/mt7610u.bin']),
       ('mt76x2u', '0e8d:7612', 'MT7612U AC1200 adapters (Alfa AWUS036ACM)', ['mt7662.bin', 'mt7662_rom_patch.bin'])]
vm = sbvm.VM('t127b', PARTS, [ISO], secure=False, mem=3072)
g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q
def sh(c, t=60): return g.sh(c, t)
try:
    time.sleep(40); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    ok('starts (the debug entry has the serial shell)', up)
    kr = last(sh('uname -r', 20)); out('kernel', kr)
    for mod, uid, name, fws in DRV:
        v, p = uid.split(':')
        # short commands: the serial shell garbles long lines
        # the module index Rudolf OS uses is busybox's (modules.dep.bb, with the aliases): what loads a driver when
        # an adapter is plugged in (mdev runs modprobe with the device's alias)
        fn = last(sh(f'find /lib/modules/{kr} -name {mod}.ko', 30))
        ok(f'{mod}: in the modules layer', fn.endswith('/' + mod + '.ko'), fn)
        n = last(sh(f'grep -ic "/{mod}.ko .*v{v}p{p}" /lib/modules/{kr}/modules.dep.bb', 30))
        ok(f'...plugging in {name} ({uid}) loads it (the alias is in the index)', n.isdigit() and int(n) >= 1, n)
        o = sh(f'modprobe {mod} 2>&1; lsmod | grep -c "^{mod} "', 30)
        ok('...loads (and waits for an adapter)', last(o) == '1', o[-200:])
        miss = [f for f in fws if last(sh(f"[ -e /lib/firmware/{f} ] || [ -e /usr/lib/firmware/{f} ] && echo yes || echo no", 20)) != "yes"]
        ok(f'...its firmware is there ({", ".join(fws)})', not miss, miss)
    o = sh('dmesg | grep -iE "ath9k_htc|mt7601u|mt76x[02]u" | grep -iE "error|fail|unknown symbol" | head -3', 20)
    ok('no driver errors while loading', not re.search(r'error|fail|unknown symbol', o, re.I), o[-300:])
finally:
    vm.stop()
out('%d/%d passed' % (sum(R), len(R)))
