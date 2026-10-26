# Rudolf OS 1.2.8 stage C in QEMU: the webcam driver and the camera switch (UEFI, Secure Boot off).
#   python3 t128a_t.py DISC.iso
# DISC.iso: a 1.2.8 test disc (mkdisc.sh with RUDOLF_UKI_TEST_CMDLINE=rudolf.debug=1: a serial shell).
# QEMU 10.2 has no USB camera (usb-video), so this checks what can be checked without one: uvcvideo is in the modules
# layer and in the index that loads it when a USB Video Class camera is plugged in (class 0x0E, subclass 1), it loads,
# and the switch in Settings > Privacy (rudolfd camera) unloads it, keeps it from loading again (also after a
# restart) and lets it load again. With no camera the hardware check skips its camera step. A real camera completes it.
import os, sys, time, json, re, subprocess
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
ISO = os.path.abspath(sys.argv[1])
PARTS = os.environ['SB_PARTS']; LOG = open('t128a.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:300] if d != '' else ''))
def last(t): return t.strip().split('\n')[-1].strip() if t.strip() else ''
def J(v, d):
    try: return json.loads(v) if isinstance(v, str) else d
    except ValueError: return d
DATA = os.path.abspath('data128a.img')
if os.path.exists(DATA): os.remove(DATA)
subprocess.run(['truncate', '-s', '1G', DATA], check=True); subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'RUDOLFDATA', DATA], check=True)
def boot(tag):
    vm = sbvm.VM('t128a' + tag, PARTS, [ISO, DATA], secure=False, mem=3072)
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q
    time.sleep(40); end = time.time() + 900
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): return vm, g, True
        except OSError: pass
        time.sleep(4)
    return vm, g, False
def js(g, e, t=60):
    for _ in range(3):
        r = g.js(e, t)
        if not (isinstance(r, tuple) and (r[0] == 'ERR' or (r[0] == 'EXC' and 'ReferenceError' in str(r[1])))): return r
        time.sleep(15)
    return r
def page(g):
    end = time.time() + 600
    while time.time() < end and g.js('typeof rdPost=="function"&&typeof hwCheck=="function"', 20) is not True: time.sleep(5)
    js(g, "(()=>{const o=document.getElementById('oobe');if(o)o.remove();svj('bd95setup',1);const b=[...document.querySelectorAll('button')].find(b=>b.textContent.trim()=='Guest');if(b)b.click();return 1})()")
vm, g, up = boot('1'); sh = lambda c, t=60: g.sh(c, t)
try:
    ok('starts, keeping files', up and last(sh('mountpoint -q /persist && echo kept', 20)) == 'kept')
    kr = last(sh('uname -r', 20))
    fn = last(sh(f'find /lib/modules/{kr} -name uvcvideo.ko', 30))
    ok('uvcvideo (USB Video Class webcams) is in the modules layer', fn.endswith('/uvcvideo.ko'), fn)
    n = last(sh(f'grep -c "/uvcvideo.ko .*ic0Eisc01" /lib/modules/{kr}/modules.dep.bb', 30))
    ok('...plugging in a UVC camera loads it (its class alias is in the index)', n.isdigit() and int(n) >= 1, n)
    o = sh('modprobe uvcvideo 2>&1; lsmod | grep -c "^uvcvideo "', 30)
    ok('...it loads (with videodev), and waits for a camera', last(o) == '1', o[-200:])
    page(g)
    c = J(js(g, "rd('camera').then(r=>JSON.stringify(r))"), {})
    ok('rudolfd camera: on, the driver loaded, no camera device', c.get('on') is True and c.get('loaded') is True and c.get('devices') == 0, c)
    r = js(g, "rdPost('camera?on=0').then(r=>r.message,e=>'ERR '+e.message)")
    ok('switching it off (Settings > Privacy > Camera) unloads the driver', r == 'the camera is off' and last(sh('lsmod | grep -c "^uvcvideo "', 20)) == '0', r)
    sh('modprobe uvcvideo 2>/dev/null; echo tried', 30); o = sh('lsmod | grep -c "^uvcvideo "', 30)
    ok('...and keeps it from loading again (as when a camera is plugged in: its file is covered)', last(o) == '0' and last(sh('grep -c uvcvideo /proc/mounts', 20)) == '1', o[-200:])
    ok('...also after a restart (kept on RUDOLFDATA)', last(sh('cat /persist/etc/camera-off', 20)) == '1')
    c = J(js(g, "rd('camera').then(r=>JSON.stringify(r))"), {})
    ok('rudolfd camera says off', c.get('on') is False and c.get('loaded') is False, c)
finally:
    vm.stop()
vm, g, up = boot('2'); sh = lambda c, t=60: g.sh(c, t)
try:
    ok('after a restart', up)
    sh('modprobe uvcvideo 2>/dev/null; echo tried', 30); o = sh('lsmod | grep -c "^uvcvideo "', 30)
    ok('...the camera is still off: the driver does not load', last(o) == '0' and last(sh('grep -c uvcvideo /proc/mounts', 20)) == '1', o[-200:])
    page(g)
    r = js(g, "rdPost('camera?on=1').then(r=>r.message,e=>'ERR '+e.message)")
    sh('modprobe uvcvideo; echo tried', 30); o = sh('lsmod | grep -c "^uvcvideo "', 30)
    ok('switching it on lets it load again, and forgets "off"', r == 'the camera is on' and last(o) == '1' and last(sh('ls /persist/etc/camera-off 2>/dev/null | wc -l', 20)) == '0', [r, o[-100:]])
    # the hardware check's camera step with no camera
    js(g, "window.elevate=async()=>true;USER.r='admin';HWC_STEPS.splice(0,HWC_STEPS.findIndex(s=>s[0]=='cam'));HWC_STEPS.length=1;hwCheck();1")
    time.sleep(5); t = js(g, "document.querySelector('.hwc .hwcb').innerText")
    ok('the hardware check skips the camera step when there is none', isinstance(t, str) and 'No camera was found' in t, t)
    n = last(sh('dmesg | grep -iE "uvcvideo|videodev" | grep -ciE "error|fail|symbol"', 20))
    ok('no driver errors', n == '0', n)
finally:
    vm.stop()
os.remove(DATA)
out('%d/%d passed' % (sum(R), len(R)))
