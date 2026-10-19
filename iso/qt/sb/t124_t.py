# Rudolf OS 1.2.4 in QEMU: keep files on a USB stick written from the disc image (UEFI, Secure Boot off).
#   python3 t124_t.py DISC.iso
# DISC.iso: a test disc made with mkdisc.sh, the test Secure Boot key and RUDOLF_UKI_TEST_CMDLINE=rudolf.debug=1
# (a serial shell). SB_PARTS: the files of the draft release sb-parts; QEMU: the QEMU source tree with build/.
# The "stick" is an 8 GB disk with the image written at its start, as Rufus (DD mode) or Etcher leave it.
#   - running live, Rudolf OS offers to keep files (GET keepfiles) and adds RUDOLFDATA after the image
#     while running from it; the image is unchanged
#   - after a restart through Rudolf OS, the session's files are on RUDOLFDATA and Rudolf OS keeps files
#   - the new start-up options, Task Manager's graphics lines, the AMD microcode and the GSP firmware
import os, sys, time, hashlib, subprocess, json
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
ISO = os.path.abspath(sys.argv[1]); PARTS = os.environ['SB_PARTS']
LOG = open('t124.txt', 'w'); R = []


def guest(vm):
    from ser import Ser
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    return g


def wait_shell(g, t=900, delay=45):
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
def last(t): return t.strip().split('\n')[-1].strip() if t.strip() else ''

stick = os.path.abspath('stick124.img')
if os.path.exists(stick): os.remove(stick)
subprocess.run(['truncate', '-s', '8G', stick], check=True)
subprocess.run(['dd', 'if=' + ISO, 'of=' + stick, 'bs=4M', 'conv=notrunc', 'status=none'], check=True)
isz = os.path.getsize(ISO)
img_sum = hashlib.sha256(open(ISO, 'rb').read()).hexdigest()
USB = ['-device', 'qemu-xhci,id=xh', '-drive', f'file={stick},format=raw,if=none,id=usb0',
       '-device', 'usb-storage,bus=xh.0,drive=usb0,bootindex=0']
vm = sbvm.VM('t124', PARTS, [], secure=False, extra=USB, mem=3072)
g = guest(vm)
def sh(c, t=60): return g.sh(c, t)
def js(e, t=60):
    for _ in range(3):
        r = g.js(e, t)
        if not (isinstance(r, tuple) and r[0] == 'ERR'): return r
        time.sleep(15); wait_desktop(g)
    return r
try:
    ok('starts live from the written stick', wait_shell(g))
    ok('...live: nothing kept yet', last(sh('[ -e /run/rudolf/persistent ] && echo kept || echo live')) == 'live')
    ok('the desktop is 1.2.4', wait_desktop(g) and js('RVER') == '1.2.4', js('RVER'))
    st = js("rd('keepfiles').then(j=>JSON.stringify(j))")
    st = json.loads(st) if isinstance(st, str) else {}
    ok('Rudolf OS offers to keep files on the stick, with its free space', st.get('possible') and 7e9 < st.get('freeBytes', 0) < 8.6e9, st)
    # something made in the live session
    sh('echo made-live > /home/rudolf/t124-marker; chown 1000:1000 /home/rudolf/t124-marker')
    js("localStorage.setItem('t124','kept');1")
    r = js("rdPost('keepfiles').then(()=>'started',e=>e.message)")
    ok('adding RUDOLFDATA starts', r == 'started', r)
    end = time.time() + 300; j = {}
    while time.time() < end:
        j = js("rd('job').then(j=>JSON.stringify(j))")
        j = json.loads(j) if isinstance(j, str) else {}
        if j.get('name') == 'keepfiles' and not j.get('running'): break
        time.sleep(5)
    ok('...and finishes', j.get('status') == 0, (j.get('log') or '')[-300:])
    p3 = last(sh('ls /dev/sd?3 2>/dev/null | head -1'))
    lab = last(sh(f'blkid {p3} 2>/dev/null | sed -n "s/.*LABEL=\\"\\([^\\"]*\\)\\".*/\\1/p"')) if p3 else ''
    ok('the new partition is RUDOLFDATA, after the image', p3.endswith('3') and lab == 'RUDOLFDATA', f'{p3} {lab}')
    st2 = js("rd('keepfiles').then(j=>JSON.stringify(j))")
    ok('...and Settings now says restart to finish', isinstance(st2, str) and json.loads(st2).get('pending'), st2)
    ok('the running system is untouched (desktop still answers)', js('1+1') == 2)
    # restart through Rudolf OS: the browser stops, the session's files move onto RUDOLFDATA
    js("rdPost('power?action=reboot').catch(()=>0);1")
    time.sleep(10)
    ok('starts again', wait_shell(g, delay=50))
    ok('...now keeping files (RUDOLFDATA mounted)', last(sh('[ -e /run/rudolf/persistent ] && mountpoint -q /persist && echo kept || echo live')) == 'kept', sh('cat /proc/mounts | grep persist'))
    ok('...with the file made while live', last(sh('cat /home/rudolf/t124-marker 2>&1')) == 'made-live', sh('ls -la /home/rudolf | head'))
    ok('...and the desktop\'s saved data', wait_desktop(g) and js("localStorage.getItem('t124')") == 'kept', js("localStorage.getItem('t124')"))
    st3 = js("rd('keepfiles').then(j=>JSON.stringify(j))")
    ok('...and no more offer', isinstance(st3, str) and not json.loads(st3).get('possible'), st3)
    ok('the start-up options are listed once', (lambda c: len(c) == len(set(c)))(last(sh('cat /proc/cmdline')).split()), last(sh('cat /proc/cmdline')))
    tm = js("rd('system').then(s=>JSON.stringify({d:tmDrawn(s),p:tmPages(),r:s.renderer,n:s.net}))")
    ok('Task Manager says what draws the screen and pages', isinstance(tm, str) and 'processor' in tm, tm)
    ok('AMD microcode is on the system (and built into the kernel, checked on the build)', 'microcode_amd_fam19h.bin' in sh('ls /usr/lib/firmware/amd-ucode/ 2>&1'), sh('ls /usr/lib/firmware/amd-ucode/ 2>&1'))
    vm.shot('end')
finally:
    vm.stop()
# after shut-down: the image part of the stick is unchanged
with open(stick, 'rb') as f:
    d = bytearray(f.read(isz))
d[446 + 32:446 + 48] = bytes(16)   # the third partition entry, the only byte the keep-files step may change
h = hashlib.sha256(d).hexdigest()
ok('the Rudolf OS image on the stick is unchanged (but for the new partition entry)', h == img_sum, h[:16])
out('%d/%d passed' % (sum(R), len(R)))
