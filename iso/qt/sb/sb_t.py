# Rudolf OS 1.2.2: Secure Boot, end to end in QEMU (OVMF with Microsoft's keys, as PCs ship).
#   python3 sb_t.py DISC.iso TESTDATA.img
# DISC.iso: a test disc made with mkdisc.sh, the test Secure Boot key and RUDOLF_UKI_TEST_CMDLINE=rudolf.debug=1
# (so a serial shell answers; a real disc has no such option). TESTDATA.img: an ext4 image labelled TESTDATA with
# unsigned-vmlinuz (the kernel without its signature) and other.crt (a second certificate, PEM).
# SB_PARTS: the files of the draft release sb-parts; QEMU: the QEMU source tree with build/.
# SB_CERT (optional): the test certificate the disc was signed with. A disc made from a published beta carries the
# real release certificate in its base layer while its boot files carry the test key, so rudolfd rightly says the
# key enrolled is not its own; with SB_CERT given, B then expects that.
#   A  Secure Boot on: shim refuses the menu until the key is enrolled; enrolment from the disc (MokManager)
#   B  then Rudolf OS starts: lockdown, every layer through dm-verity, Settings / Task Manager say so,
#      /dev/mem refused, the desktop page on RUDOLFDATA ignored
#   C  the boot menu: Recovery mode reaches the system (LoaderEntrySelected)
#   D  an update slot with a changed layer is dropped; one with an unsigned kernel does not start
#   E  Prepare Secure Boot: MokManager enrolls a requested key after its one-time password
#   F  Secure Boot off (a UEFI computer without keys): starts as before, layers still checked
import os, sys, time, json, subprocess
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
ISO, TD = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
PARTS = os.environ['SB_PARTS']
LOG = open('sb_t.txt', 'w')
R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:300] if d != '' else ''))
persist = os.path.abspath('persist.img')
if not os.path.exists(persist):
    subprocess.run(['truncate', '-s', '3G', persist], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-L', 'RUDOLFDATA', persist], check=True)
SHIMG = '605dab50-e046-4300-abb6-3dd810dd8b23'


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


def wait_blue(vm, tag, t=240):
    """wait until shim's or MokManager's blue screen shows (its frame is dark blue around the middle)"""
    from PIL import Image
    end = time.time() + t
    while time.time() < end:
        f = vm.shot(tag)
        try:
            im = Image.open(f).convert('RGB'); w, h = im.size
            px = [im.getpixel((int(w * x), int(h * y))) for x, y in ((0.25, 0.3), (0.75, 0.7), (0.3, 0.75))]
            if all(b > 120 and r < 40 and g < 40 for r, g, b in px): return True
        except Exception: pass
        time.sleep(1)
    return False


def keys(vm, *seq):
    for k in seq:
        if isinstance(k, (int, float)): time.sleep(k)
        elif k.startswith('type:'): vm.q.type(k[5:])
        else: vm.q.key(*k.split('+')); time.sleep(0.6)


def reboot(g):
    g.s.s.send(b'sync; reboot -f\n'); time.sleep(3)


# ---------------------------------------------------------------- A: enrolment
vm = sbvm.VM('sbA', PARTS, [ISO, persist, TD])
ok('A: shim refuses the unenrolled boot menu (Verification failed)', wait_blue(vm, 'shim'))
# shim: "Verification failed" -> OK -> MokManager: press a key -> Enroll key from disk
keys(vm, 'ret', 2.5, 'spc', 2.5); vm.shot('mok')
keys(vm, 'down', 'ret', 2); vm.shot('volumes')          # the disc's UEFI image is the only volume
keys(vm, 'ret', 2); vm.shot('root')                      # EFI/ loader/
keys(vm, 'ret', 1.5, 'down', 'down', 'ret', 1.5)         # EFI -> ../ BOOT/ rudolf/ -> rudolf
vm.shot('rudolf'); keys(vm, 'down', 'ret', 2)            # ../ rudolf.cer rudolf.efi -> rudolf.cer
vm.shot('enroll'); keys(vm, 'down', 'ret', 2)            # View key 0 / Continue
keys(vm, 'down', 'ret', 2); vm.shot('enrolled')          # No / Yes
keys(vm, 'ret')                                          # Reboot
g = guest(vm)
ok('A: with the key enrolled from the disc, Rudolf OS starts with Secure Boot on', wait_shell(g))

# ---------------------------------------------------------------- B: the started system
sh = lambda c, t=30: g.sh(c, t).strip()
ok('B: the start-up saw Secure Boot on', sh('cat /etc/rudolf/secureboot') == '1')
ok('B: the kernel is locked down (integrity)', '[integrity]' in sh('cat /sys/kernel/security/lockdown'), sh('cat /sys/kernel/security/lockdown'))
dm = sh('for d in /sys/block/dm-*; do cat $d/dm/name; done')
ok('B: every system layer is mapped through dm-verity', all('rudolf-' + n in dm for n in ('base', 'chromium', 'desktop', 'modules')), dm)
ok('B: no layer started unchecked', sh('cat /etc/rudolf/unverified') == '')
ok('B: /dev/mem is refused', 'not permitted' in sh('head -c 1 /dev/mem 2>&1'))
ok('B: signed drivers load', int(sh('lsmod | wc -l') or 0) >= 1)
ok('B: the desktop starts', wait_desktop(g))
st = g.js("rd('secureboot/status').then(s=>JSON.stringify(s))")
s = json.loads(st) if isinstance(st, str) and st.startswith('{') else {}
import hashlib
TC = os.environ.get('SB_CERT'); mine = True
if TC:   # is the certificate built into this system the one the disc was signed with?
    mine = hashlib.sha256(open(TC, 'rb').read()).hexdigest() in sh('sha256sum /usr/share/rudolf/rudolf-sb.crt')
    if not mine: out('B: this system carries the release certificate and its boot files the test key (a disc made from a published beta)')
ok('B: rudolfd reports Secure Boot on, the key enrolled, the lockdown' if mine else 'B: rudolfd reports Secure Boot on, the lockdown, and that the key enrolled (the test key) is not the release key',
   s.get('on') is True and s.get('enrolled') is mine and 'integrity' in s.get('lockdown', ''), st)
ok('B: Task Manager says so', 'on, kernel locked down' in str(g.js("sbStatus(true).then(sbSummary)")), g.js("sbStatus(true).then(sbSummary)"))
# the desktop moves into the shell once after its first start (Chromium restarts): wait for it again
time.sleep(20); wait_desktop(g)
def js2(e, t=60):
    for _ in range(3):
        r = g.js(e, t)
        if not (isinstance(r, tuple) and r[0] == 'ERR'): return r
        time.sleep(15); wait_desktop(g)
    return r
st = js2("(async()=>{const W=win('Security','x',600,500);secPanelS(W.body,W);await new Promise(r=>setTimeout(r,4000));const t=W.body.querySelector('.sb28 .sbt');W.close();return t?t.textContent:'no section'})()")
ok('B: Settings > Security says so', str(st).startswith('On.'), st)
rep = js2("sysReport().then(r=>typeof r=='string'?r:(r.text||JSON.stringify(r)))", 90)
ok('B: the system report has a Secure Boot line', 'Secure Boot: on' in str(rep), str(rep)[:200])
sh("mkdir -p /persist/os/current && (echo '<!-- planted -->'; head -c 120000 /dev/zero | tr '\\0' x) > /persist/os/current/index.html")
page = sh('(command -v curl >/dev/null && curl -s http://127.0.0.1:8080/ || wget -qO- http://127.0.0.1:8080/) | head -c 2000')
ok('B: a desktop page planted on RUDOLFDATA is not served', len(page) > 100 and 'planted' not in page, page[:120])
sh('rm -rf /persist/os/current')

# ---------------------------------------------------------------- C: the boot menu
reboot(g); time.sleep(12); vm.shot('menu')
keys(vm, 'down', 'ret')                                  # Recovery mode
ok('C: Recovery mode from the UEFI menu starts', wait_shell(g))
# 1.2.8: the start-up options the system uses (with the menu's choice) are in /etc/rudolf/cmdline, no longer over /proc/cmdline
ok('C: ...and the system knows it (rudolf.recovery=1)', 'rudolf.recovery=1' in sh('cat /etc/rudolf/cmdline') and sh('cat /etc/rudolf/boot-entry').startswith('rudolf-recovery'), sh('cat /etc/rudolf/cmdline; cat /etc/rudolf/boot-entry').replace('\n', ' | '))

# ---------------------------------------------------------------- D: update slots that must not start
sh('mkdir -p /mnt/td && mount -o ro LABEL=TESTDATA /mnt/td 2>/dev/null || mount -o ro $(findfs LABEL=TESTDATA) /mnt/td')
M = '/live/media'
mk = lambda slot, kernel: sh(f'S=/persist/os/system/{slot}; rm -rf $S; mkdir -p $S && cp {kernel} $S/vmlinuz && for n in base chromium desktop modules; do cp {M}/rudolf/$n.sfs $S/; done && cp {M}/rudolf/manifest.txt {M}/rudolf/manifest.sig $S/ && sed -n "s/^serial //p" {M}/rudolf/manifest.txt | sed "s/^/serial /" > $S/RELEASE && echo 1 > $S/complete && echo made', 300)
ok('D: a slot with the disc system (as an update would be) is made', 'made' in mk('a', M + '/boot/vmlinuz'))
# one byte of the desktop layer changed (in its first block, which mounting reads): its hash no longer matches
sh("printf 'X' | dd of=/persist/os/system/a/desktop.sfs bs=1 seek=100 conv=notrunc 2>/dev/null; printf 'good=a\\nprev=\\nnext=\\ntries=0\\nfailed=\\n' > /persist/os/system/state; sync")
reboot(g)
ok('D: the changed slot is refused and the disc system starts', wait_shell(g) and sh('cat /etc/rudolf/slot') == 'disc', sh('cat /etc/rudolf/slot'))
ok('D: ...and the slot is marked failed, not tried again', 'failed=a' in sh('cat /persist/os/system/state') and 'good=a' not in sh('cat /persist/os/system/state'), sh('cat /persist/os/system/state'))
sh('mkdir -p /mnt/td && (mount -o ro LABEL=TESTDATA /mnt/td 2>/dev/null || mount -o ro $(findfs LABEL=TESTDATA) /mnt/td)')
ok('D: a slot with an unsigned kernel is made', 'made' in mk('b', '/mnt/td/unsigned-vmlinuz'))
sh("printf 'good=b\\nprev=\\nnext=\\ntries=0\\nfailed=\\n' > /persist/os/system/state; sync")
reboot(g)
ok('D: an unsigned kernel is not started (kexec refused): the disc system starts', wait_shell(g) and sh('cat /etc/rudolf/slot') == 'disc', sh('cat /etc/rudolf/slot; dmesg | grep -i kexec | tail -2'))
sh("rm -rf /persist/os/system/a /persist/os/system/b; printf 'good=\\nprev=\\nnext=\\ntries=0\\nfailed=\\n' > /persist/os/system/state; sync")

# ---------------------------------------------------------------- E: Prepare Secure Boot (another certificate)
sh('mkdir -p /mnt/td && (mount -o ro LABEL=TESTDATA /mnt/td 2>/dev/null || mount -o ro $(findfs LABEL=TESTDATA) /mnt/td); cp /usr/share/rudolf/rudolf-sb.crt /tmp/own.crt; cp /mnt/td/other.crt /usr/share/rudolf/rudolf-sb.crt')
wait_desktop(g)
pw = g.js("rdPost('secureboot/prepare').then(r=>r.password,e=>'ERR '+e.message)", 60)
ok('E: Prepare Secure Boot stores the request and gives a one-time password', isinstance(pw, str) and pw.isdigit() and len(pw) == 8, pw)
ok('E: ...and the request is waiting', json.loads(g.js("rd('secureboot/status').then(s=>JSON.stringify(s))")).get('pending') is True)
reboot(g)
ok('E: at the next start MokManager offers the request', wait_blue(vm, 'mok-request'))
keys(vm, 'spc', 2.5); vm.shot('mok-menu')                # Press any key -> Continue boot / Enroll MOK / ...
keys(vm, 'down', 'ret', 2); vm.shot('mok-enroll')        # Enroll MOK -> View key 0 / Continue
keys(vm, 'down', 'ret', 2)                               # Continue -> No / Yes
keys(vm, 'down', 'ret', 2); vm.shot('mok-password')      # Yes -> Password:
keys(vm, 'type:' + str(pw), 'ret', 3); vm.shot('mok-done')
keys(vm, 'ret')                                          # Reboot
ok('E: after MokManager it starts again', wait_shell(g))
sh('mkdir -p /mnt/td && (mount -o ro LABEL=TESTDATA /mnt/td 2>/dev/null || mount -o ro $(findfs LABEL=TESTDATA) /mnt/td); cp /mnt/td/other.crt /usr/share/rudolf/rudolf-sb.crt')
wait_desktop(g)
st = json.loads(g.js("rd('secureboot/status').then(s=>JSON.stringify(s))"))
ok('E: the requested certificate is now enrolled (the one-time password was right)', st.get('enrolled') is True and st.get('pending') is False, st)
vm.stop()

# ---------------------------------------------------------------- F: Secure Boot off
vm = sbvm.VM('sbF', PARTS, [ISO, persist, TD], secure=False)
g = guest(vm); time.sleep(30); vm.shot('start')
ok('F: with Secure Boot off (no keys), Rudolf OS starts through the same chain', wait_shell(g))
sh = lambda c, t=30: g.sh(c, t).strip()
ok('F: the start-up saw Secure Boot off, no lockdown', sh('cat /etc/rudolf/secureboot') == '0' and '[none]' in (sh('cat /sys/kernel/security/lockdown 2>/dev/null') or '[none]'))
dm = sh('for d in /sys/block/dm-*; do cat $d/dm/name; done')
ok('F: the layers are still checked (dm-verity)', all('rudolf-' + n in dm for n in ('base', 'chromium', 'desktop', 'modules')), dm)
ok('F: the desktop starts', wait_desktop(g))
st = json.loads(g.js("rd('secureboot/status').then(s=>JSON.stringify(s))"))
ok('F: rudolfd reports Secure Boot off and offers to prepare it', st.get('on') is False and st.get('efi') is True, st)
vm.stop()

out(f'\nRESULT {sum(R)}/{len(R)}')
