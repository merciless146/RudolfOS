# Rudolf OS 1.2.3 in QEMU: a UEFI computer (Secure Boot off) starting from a USB stick with the disc image on it.
#   python3 t123_t.py DISC.iso
# DISC.iso: a test disc made with mkdisc.sh, the test Secure Boot key and RUDOLF_UKI_TEST_CMDLINE=rudolf.debug=1
# (a serial shell). SB_PARTS: the files of the draft release sb-parts; QEMU: the QEMU source tree with build/.
#   - the firmware in the modules layer: NVIDIA GSP (with the links of other chips) and Realtek Ethernet,
#     read through dm-verity and identical to the pinned list
#   - Task Manager: started from a USB drive; the installed release; each start-up option once
#   - an update slot started by kexec: still each option once, and the release says slot a (trial start)
import os, sys, time, hashlib, subprocess
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE); ISO_DIR = os.path.dirname(QT)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
ISO = os.path.abspath(sys.argv[1]); PARTS = os.environ['SB_PARTS']
LOG = open('t123.txt', 'w'); R = []


def guest(vm):
    """the serial shell, sharing the VM's QMP connection (QEMU takes one QMP client at a time)"""
    from ser import Ser
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    return g


def wait_shell(g, t=900, delay=45):
    # the firmware also reads the serial line: nothing may be typed there before Linux runs (a key would open
    # the firmware's setup), so the first try comes after the firmware and the boot menu
    time.sleep(delay)
    end = time.time() + t
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): return True
        except OSError: pass
        time.sleep(4)
    return False


def wait_desktop(g, t=600):
    end = time.time() + t
    while time.time() < end:
        r = g.js('typeof RVER=="string"&&!!document.body&&RVER', 20)
        if isinstance(r, str): return True
        time.sleep(5)
    return False


def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:300] if d != '' else ''))
persist = os.path.abspath('persist123.img')
if os.path.exists(persist): os.remove(persist)
subprocess.run(['truncate', '-s', '2G', persist], check=True)
subprocess.run(['mkfs.ext4', '-q', '-L', 'RUDOLFDATA', persist], check=True)
USB = ['-device', 'qemu-xhci,id=xh', '-drive', f'file={ISO},format=raw,if=none,id=usb0,readonly=on',
       '-device', 'usb-storage,bus=xh.0,drive=usb0,bootindex=0']
vm = sbvm.VM('t123', PARTS, [persist], secure=False, extra=USB)
g = guest(vm)
def sh(c, t=60):
    return g.sh(c, t).strip()
def js(e, t=60):
    for _ in range(3):
        r = g.js(e, t)
        if not (isinstance(r, tuple) and r[0] == 'ERR'): return r
        time.sleep(15); wait_desktop(g)
    return r
try:
    ok('starts from the USB stick (UEFI, Secure Boot off)', wait_shell(g))
    # ---- firmware
    want = {}
    for line in open(os.path.join(ISO_DIR, 'firmware-extra.txt')):
        t = line.split()
        if t and t[0] == 'F': want[t[2]] = t[1]
    pick = ['nvidia/ga102/gsp/gsp-570.144.bin', 'nvidia/tu102/gsp/gsp-570.144.bin', 'rtl_nic/rtl8168h-2.fw']
    # (short commands: the serial shell's echo of a long line wraps and garbles the answer)
    got = {p: sh('sha256sum /usr/lib/firmware/' + p, 180).split(' ')[0] for p in pick}
    ok('the GSP and Ethernet firmware is there, as pinned', all(got[p] == want[p] for p in pick), got)
    lk = {f: sh(f'[ -r /usr/lib/firmware/nvidia/{f} ] && echo yes || echo no') for f in
          ('ga104/gsp/gsp-570.144.bin', 'ga104/gsp/booter_load-570.144.bin', 'ad104/gsp/gsp-570.144.bin', 'tu104/gsp/bootloader-570.144.bin', 'gb203/gsp/fmc-570.144.bin')}
    ok('...other chips find it through the links (with the base layer\'s booter files)', all(v.endswith('yes') for v in lk.values()), lk)
    dm = sh('for d in /sys/block/dm-*; do cat $d/dm/name; done')
    ok('the modules layer is checked by dm-verity', 'rudolf-modules' in dm, dm)
    cl = sh('cat /proc/cmdline').split()
    ok('each start-up option once', len(cl) == len(set(cl)), ' '.join(cl))
    ok('the system started from /dev/sd* (the USB stick)', sh('cat /etc/rudolf/media').startswith('/dev/sd'), sh('cat /etc/rudolf/media'))
    ok('the desktop is 1.2.3', wait_desktop(g) and js('RVER') == '1.2.3', js('RVER'))
    ok('rudolfd and Task Manager: started from a USB drive', js("rd('info').then(i=>i.media)") == 'usb', js("rd('info').then(i=>i.media)"))
    rel = js("sysuStatus().then(s=>tmRel(s))")
    ok('Task Manager: the installed release (from the disc)', isinstance(rel, str) and rel.startswith('1.2.3.') and 'from the disc' in rel, rel)
    rep = str(js("sysReport().then(r=>typeof r=='string'?r:r.text)", 90))
    line = next((l for l in rep.split('\n') if l.startswith('Started from')), '')
    ok('the system report: USB drive, release', 'usb' in line and 'release 1.2.3.' in rep, line + ' | ' + next((l for l in rep.split('\n') if l.startswith('Rudolf OS:')), ''))
    # ---- an update slot (the same release, as a trial start): kexec with the options of this start
    for c in ('mkdir -p /persist/os/system/a', 'cd /persist/os/system/a', 'cp /live/media/boot/vmlinuz vmlinuz',
              'cp /live/media/rudolf/manifest.txt /live/media/rudolf/manifest.sig .', 'cp /live/media/rudolf/release RELEASE',
              'echo 1 > complete', 'printf "good=disc\\nnext=a\\ntries=2\\n" > ../state', 'cd /; sync'):
        sh(c, 120)
    out(sh('ls /persist/os/system/a; cat /persist/os/system/state'))
    g.s.s.send(b'sync; reboot -f\n'); time.sleep(5)
    ok('the update slot starts (kexec)', wait_shell(g, delay=60))
    cl = sh('cat /proc/cmdline').split()
    ok('...as slot a on a trial start', 'rudolf.slot=a' in cl and 'rudolf.trying=1' in cl, ' '.join(cl))
    ok('...with each start-up option once', len(cl) == len(set(cl)), ' '.join(cl))
    ok('...and the desktop says slot a (trial start)', wait_desktop(g) and 'slot a (trial start)' in str(js("sysuStatus().then(s=>tmRel(s))")), js("sysuStatus().then(s=>tmRel(s))"))
    vm.shot('end')
finally:
    vm.stop()
out('%d/%d passed' % (sum(R), len(R)))
