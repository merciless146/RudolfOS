# Rudolf OS 1.2.5 stage D in QEMU: Task Manager's "Check this computer…" against the real system (UEFI, Secure Boot
# off): a virtual sound card (intel-hda), the network adapter and a download from an update server, a USB stick
# plugged in during the USB step (QMP device_add), graphics, and sleep for 10 seconds with the real-time clock waking
# the computer.
#   python3 t125d_t.py DISC.iso NEWDIR
# DISC.iso: a test disc (mkdisc.sh with RUDOLF_UKI_TEST_CMDLINE=rudolf.debug=1: a serial shell).
# NEWDIR: a newer rudolfd and rudolf-update (static) and index.html, put in place after the start. The test adds an
#   update server (a release list and a kernel, served by busybox httpd on 127.0.0.1) and its helper scripts.
import os, sys, time, json, subprocess, shutil, re
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
ISO, NEWDIR = (os.path.abspath(x) for x in sys.argv[1:3])
PARTS = os.environ['SB_PARTS']; LOG = open('t125d.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:300] if d != '' else ''))
def last(t): return t.strip().split('\n')[-1].strip() if t.strip() else ''
def J(v, d):
    try: return json.loads(v) if isinstance(v, str) else d
    except ValueError: return d

NEW = os.path.abspath('new125d.img'); D = os.path.abspath('new125d'); shutil.rmtree(D, ignore_errors=True); shutil.copytree(NEWDIR, D)
# the update server: a release list whose kernel is a 10 MB file next to it
os.makedirs(f'{D}/repo/ch'); os.makedirs(f'{D}/repo/f')
with open(f'{D}/repo/f/vmlinuz', 'wb') as f: f.write(os.urandom(1 << 20) * 10)
L = ['vmlinuz', 'base', 'chromium', 'desktop', 'modules']
open(f'{D}/repo/ch/manifest.txt', 'w').write('rudolf-system 1\nserial 2026102199\nversion 1.2.5.test\nreleased 2026-10-21\n' +
    ''.join(f'L\t{n}\t{10485760 if n == "vmlinuz" else 1}\t{"0" * 64}\t../f/{n}{"" if n == "vmlinuz" else ".sfs"}\n' for n in L))
open(f'{D}/up.sh', 'w').write('#!/bin/sh\n# the newer page and programs, until the next restart; the update server\ncp /mnt/new/index.html /usr/share/rudolf/www/index.html\n'
    'cp /mnt/new/rudolfd /usr/lib/rudolf/rudolfd-new; cp /mnt/new/rudolf-update /usr/bin/rudolf-update; chmod 755 /usr/lib/rudolf/rudolfd-new /usr/bin/rudolf-update\n'
    'mkdir -p /etc/rudolf; echo http://127.0.0.1:8077/ch > /etc/rudolf/update-url; busybox httpd -p 127.0.0.1:8077 -h /mnt/new/repo\n'
    'pkill -x rudolfd; sleep 1\n(/usr/lib/rudolf/rudolfd-new > /run/rudolf/rudolfd.log 2>&1 &)\n'
    'for i in 1 2 3 4 5 6 7 8 9 10; do wget -q -O /dev/null http://127.0.0.1:8080/ && break; sleep 1; done\necho restarted $i\n')
for f in ('up.sh', 'rudolfd', 'rudolf-update'): os.chmod(f'{D}/{f}', 0o755)
if os.path.exists(NEW): os.remove(NEW)
subprocess.run(['truncate', '-s', '64M', NEW], check=True); subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'RUDOLFB', '-d', D, NEW], check=True)
HP = os.path.abspath('hotplug125d.img')
if os.path.exists(HP): os.remove(HP)
subprocess.run(['truncate', '-s', '1G', HP], check=True)
extra = ['-device', 'qemu-xhci,id=xh', '-drive', f'file={NEW},format=raw,if=none,id=u0', '-device', 'usb-storage,bus=xh.0,drive=u0,removable=on',
         '-drive', f'file={HP},format=raw,if=none,id=hp', '-audiodev', 'none,id=snd0', '-device', 'intel-hda', '-device', 'hda-duplex,audiodev=snd0']
vm = sbvm.VM('t125d', PARTS, [ISO], secure=False, mem=3072, extra=extra)
g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
def sh(c, t=60): return g.sh(c, t)
def js(e, t=60):
    for _ in range(3):
        r = g.js(e, t)
        if not (isinstance(r, tuple) and (r[0] == 'ERR' or (r[0] == 'EXC' and 'ReferenceError' in str(r[1])))): return r
        time.sleep(15)
    return r
def until(e, t=120):
    end = time.time() + t
    while time.time() < end:
        if g.js(e, 20) is True: return True
        time.sleep(2)
    return False
def step(): return J(js("JSON.stringify([document.querySelector('.hwc .hwcn').textContent,document.querySelector('.hwc h3').textContent,document.querySelector('.hwc .hwcb').innerText,[...document.querySelectorAll('.hwc .hwcbt button')].map(b=>b.textContent)])"), ['', '', '', []])
def click(l): return js(f"(()=>{{const b=[...document.querySelectorAll('.hwc .hwcbt button')].find(b=>b.textContent=={json.dumps(l)});if(!b)return 'no button';b.click();return 'ok'}})()")
def has(l, t=60): return until(f"[...document.querySelectorAll('.hwc .hwcbt button')].some(b=>b.textContent=={json.dumps(l)})", t)
try:
    time.sleep(45); end = time.time() + 900
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): break
        except OSError: pass
        time.sleep(4)
    sh('mkdir -p /mnt/new', 10)
    for _ in range(30):
        nd = last(sh('findfs LABEL=RUDOLFB', 10))
        if nd.startswith('/dev/'): break
        time.sleep(3)
    sh(f'mount -o ro {nd} /mnt/new 2>/dev/null || mount --bind /media/{nd[5:]} /mnt/new', 20)
    out('new programs:', last(sh('sh /mnt/new/up.sh', 60)))
    for _ in range(4):
        g.js("location.replace('http://127.0.0.1:8080/');1", 10); time.sleep(15)
        if until("typeof hwCheck=='function'", 90): break
    js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();svj('bd95setup',1);const b=[...document.querySelectorAll('button')].find(b=>b.textContent.trim()=='Guest');if(b)b.click();return 1})()")
    until('!!USER', 120); js("USER.r='admin';window.elevate=async()=>true;1")
    ok('the desktop with the check is up', js("typeof hwCheck") == 'function')
    r = J(js("rd('speedtest').then(r=>JSON.stringify(r),e=>JSON.stringify({err:e.message}))", 120), {})
    ok('speedtest downloads the release\'s kernel from the update server', r.get('bytes') == 10485760 and r.get('ms', -1) >= 0, r)
    js("launch('taskmgr');1"); time.sleep(3); js("[...document.querySelectorAll('.tmtabs button')].find(b=>b.dataset.t=='hw').click();1")
    ok('Task Manager › Hardware has Check this computer', until("!!document.querySelector('.hwcgo')", 60))
    js("document.querySelector('.hwcgo').click();1"); time.sleep(3)
    s = step(); ok('sound: the virtual card is listed', s[1] == 'Sound' and re.search(r'\d sound cards?\.', s[2]), s)
    click('Yes, I heard both'); has('Next', 30) or has('Try again', 5); s = step()
    ok('microphone: the virtual input is opened and measured', s[1] == 'Microphone' and re.search(r'Heard you|Hardly anything was heard|could not be used', s[2]), s[2])
    if 'Try again' in s[3]: pass
    click('Next'); has('Next', 120); s = step()
    ok('network: the adapter and the download from the update server', s[1] == 'Network' and 'Wired' in s[2] and re.search(r'Download: [\d.]+ Mb/s \(', s[2]), s[2])
    click('Next'); time.sleep(3)
    ok('USB: waits for a stick', 'Plug a USB stick' in step()[2])
    out(g.q.cmd('device_add', driver='usb-storage', bus='xh.0', drive='hp', id='hpdev'))
    ok('...finds the one plugged in (QMP device_add)', has('Next', 40) and 'Found' in step()[2], step()[2])
    click('Next'); has('Next', 60); s = step()
    ok('graphics: what draws and the frames per second', s[1] == 'Graphics' and 'Frames per second' in s[2] and 'drawn by' in s[2], s[2])
    click('Next'); time.sleep(3); s = step()
    ok('sleep: offered', s[1] == 'Sleep' and 'Sleep for 10 seconds' in s[3], s)
    t0 = time.time(); click('Sleep for 10 seconds'); time.sleep(5)
    st = g.q.cmd('query-status'); out('while asleep:', st)
    woke = has('Next', 120)
    s = step(); ok('...the computer sleeps and the clock wakes it', woke and 'Awake again' in s[2], [st, s[2], round(time.time() - t0)])
    click('Next'); time.sleep(2); s = step()
    ok('the results', s[1] == 'Results' and all(n in s[2] for n in ('Sound', 'Microphone', 'Network', 'USB', 'Graphics', 'Sleep')), s[2])
    rep = js("sysReport('').then(r=>r.text)", 120)
    ok('...and the system report has them', isinstance(rep, str) and 'Hardware check: sound pass' in rep, (re.search(r'Hardware check.*', rep or '') or [''])[0] if isinstance(rep, str) else rep)
    vm.shot('results')
finally:
    vm.stop()
out('%d/%d passed' % (sum(R), len(R)))
