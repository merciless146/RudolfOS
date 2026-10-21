# Rudolf OS 1.2.5 stage B in QEMU: the boot menu's default entry and wait, and "Restart into Windows" (UEFI,
# Secure Boot off, EFI variables kept across restarts).
#   python3 t125b_t.py DISC.iso WIN.img NEWDIR
# DISC.iso: a test disc (mkdisc.sh with RUDOLF_UKI_TEST_CMDLINE=rudolf.debug=1: a serial shell).
# WIN.img: mkwindisk.py's stand-in Windows disk: an EFI system partition with \EFI\Microsoft\Boot\bootmgfw.efi
#   (winstub.c: prints RUDOLF-WINDOWS-STANDIN, waits 3 s, restarts) and RUDOLF/BOOTOPT.BIN, the firmware entry
#   Windows Setup would add, which the test stores as Boot0042.
# NEWDIR: a folder with a newer rudolfd (static) and index.html, put in place after each start (to test them on a
#   disc whose layers are older; the RAM overlay forgets them at each restart). The test adds two small helper
#   scripts and makes it a disk labelled RUDOLFB: the serial shell garbles long command lines, so they stay short.
import os, sys, time, json, re
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
import subprocess, shutil
ISO, WIN, NEWDIR = (os.path.abspath(x) for x in sys.argv[1:4])
NEW = os.path.abspath('new125b.img'); D = os.path.abspath('new125b'); shutil.rmtree(D, ignore_errors=True); shutil.copytree(NEWDIR, D)
open(f'{D}/v.sh', 'w').write('#!/bin/sh\n# an EFI variable as text: V=[value], V=[] when it is not set\nf=/sys/firmware/efi/efivars/$1-${2:-4a67b082-0a4c-41cf-b6c7-440b29bb8c4f}\n'
                             'if [ -e $f ]; then echo "V=[$(tail -c +5 $f | tr -d \'\\000\')]"; else echo "V=[]"; fi\n')
open(f'{D}/bo.sh', 'w').write('#!/bin/sh\n# the Windows Boot Manager entry, as Windows Setup adds it: Boot0042\nmkdir -p /mnt/win; grep -q " /mnt/win " /proc/mounts || mount -o ro $(findfs LABEL=WINSTANDIN) /mnt/win\n'
                              "printf '\\007\\000\\000\\000' > /tmp/bo; cat /mnt/win/RUDOLF/BOOTOPT.BIN >> /tmp/bo\n"
                              'dd if=/tmp/bo of=/sys/firmware/efi/efivars/Boot0042-8be4df61-93ca-11d2-aa0d-00e098032b8c bs=4096 2>/dev/null; ls /sys/firmware/efi/efivars | grep -c Boot0042\n')
open(f'{D}/up.sh', 'w').write('#!/bin/sh\n# the newer page and rudolfd, until the next restart\ncp /mnt/new/index.html /usr/share/rudolf/www/index.html\npkill -x rudolfd; sleep 1\n'
                              '(/mnt/new/rudolfd > /run/rudolf/rudolfd.log 2>&1 &)\nfor i in 1 2 3 4 5 6 7 8 9 10; do wget -q -O /dev/null http://127.0.0.1:8080/ && break; sleep 1; done\necho restarted $i\n')
for f in ('v.sh', 'bo.sh', 'up.sh', 'rudolfd'): os.chmod(f'{D}/{f}', 0o755)
if os.path.exists(NEW): os.remove(NEW)
subprocess.run(['truncate', '-s', '32M', NEW], check=True); subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'RUDOLFB', '-d', D, NEW], check=True)
PARTS = os.environ['SB_PARTS']; LOG = open('t125b.txt', 'w'); R = []
LG = '4a67b082-0a4c-41cf-b6c7-440b29bb8c4f'; GG = '8be4df61-93ca-11d2-aa0d-00e098032b8c'; EV = '/sys/firmware/efi/efivars'


def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:300] if d != '' else ''))
def last(t): return t.strip().split('\n')[-1].strip() if t.strip() else ''

drives = [ISO, WIN, NEW]
vm = sbvm.VM('t125b', PARTS, drives, secure=False, nvram=True, mem=3072)
from ser import Ser
g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q
SEEN = []   # everything read from the serial port while waiting for a start (the stand-in prints there)
def sh(c, t=60): return g.sh(c, t)
def wait_shell(t=900, delay=40):
    end = time.time() + t; t0 = time.time()
    while time.time() < end:
        if time.time() - t0 < delay: SEEN.append(g.s.read(2)); continue
        try:
            o = g.sh('echo ready', 8); SEEN.append(o)   # whatever came before the shell answered is kept too
            if 'ready' in o: return True
        except OSError: pass
        time.sleep(3)
    return False
def wait_desktop(t=600):
    end = time.time() + t
    while time.time() < end:
        if isinstance(g.js('typeof RVER=="string"&&RVER', 20), str): return True
        time.sleep(5)
    return False
def js(e, t=60):
    for _ in range(3):
        r = g.js(e, t)
        if not (isinstance(r, tuple) and r[0] == 'ERR'): return r
        time.sleep(15); wait_desktop()
    return r
def until(e, t=90):
    end = time.time() + t
    while time.time() < end:
        if g.js(e, 20) is True: return True
        time.sleep(3)
    return False
def var(name, guid=LG):   # an EFI variable's value as text (UTF-16 shown as ASCII), or '' when it is not set
    m = re.search(r'V=\[([^\]\n]*)\]', sh(f'/mnt/new/v.sh {name} {guid}', 20)); return m.group(1) if m else None
def started(delay=40):
    """after a start: the serial shell, the newer rudolfd and page when given, the desktop signed in"""
    if not wait_shell(delay=delay): return False
    if True:
        sh('mkdir -p /mnt/new', 10); sh('grep -q " /mnt/new " /proc/mounts || mount -o ro $(findfs LABEL=RUDOLFB) /mnt/new', 20)
        out('new rudolfd:', last(sh('/mnt/new/up.sh', 60)))
        # reload the page from the new rudolfd (a reload that came too early shows Chromium's error page: try again)
        for _ in range(4):
            g.js("location.replace('http://127.0.0.1:8080/');1", 10); time.sleep(15)
            if wait_desktop(90) and g.js('typeof rdPost=="function"', 20) is True: break
    if not wait_desktop(): return False
    js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();svj('bd95setup',1);const b=[...document.querySelectorAll('button')].find(b=>b.textContent.trim()=='Guest');if(b)b.click();return 1})()")
    until('!!USER', 120); js("USER.r='admin';window.elevate=async()=>true;1")
    return until("typeof rdPost=='function'&&typeof bootSection=='function'", 60)
def restart():
    js("rdPost('power?action=reboot').catch(()=>0);1"); time.sleep(8); return started()

try:
    ok('starts (UEFI, the debug entry has the serial shell)', started())
    # the Windows Boot Manager entry Windows Setup would have added (Boot0042), from the stand-in disk
    out('Boot0042 written:', last(sh('/mnt/new/bo.sh', 30)))
    m = js("rd('bootmenu').then(m=>JSON.stringify(m))"); m = json.loads(m) if isinstance(m, str) else {}
    ok('bootmenu: UEFI, started from systemd-boot, with its entries', m.get('uefi') and m.get('loader') and 'rudolf-start.conf' in m.get('entries', []) and 'rudolf-recovery.conf' in m.get('entries', []), m)
    ok('...which entry started', m.get('selected') in m.get('entries', []) and m.get('selected', '').startswith('rudolf-'), m.get('selected'))
    ok('...and the Windows Boot Manager entry', m.get('windows') == [{'n': '0042', 'title': 'Windows Boot Manager'}], m.get('windows'))
    first = m.get('selected')
    # (the page's Boot menu section and the Power dialog button are tested against a stand-in rudolfd: shell2/boot_t.js)
    r = js("rdPost('bootmenu/set?default=nonsense.conf').then(()=>'saved',e=>e.message)")
    ok('an entry the menu does not have is refused', r == 'that is not an entry of the boot menu', r)
    r = js("rdPost('bootmenu/set?timeout=60').then(()=>'saved',e=>e.message)")
    ok('...and a wait over 10 seconds', r == 'the wait is 0 to 10 seconds', r)
    r = js("rdPost('bootmenu/set?default=rudolf-recovery.conf&timeout=2').then(()=>'saved',e=>e.message)")
    ok('both are stored in the firmware', r == 'saved' and var('LoaderEntryDefault') == 'rudolf-recovery.conf' and var('LoaderConfigTimeout') == '2', [r, var('LoaderEntryDefault'), var('LoaderConfigTimeout')])
    m = js("rd('bootmenu').then(m=>JSON.stringify([m.default,m.timeout]))")
    ok('...and bootmenu reports them', m == '["rudolf-recovery.conf","2"]', m)
    # the restart itself uses the debug entry (the same start as the test disc's normal one, with its serial shell;
    # recovery mode would start a different desktop)
    r = js("rdPost('bootmenu/set?default=rudolf-debug.conf').then(()=>'saved',e=>e.message)")
    ok('after a restart the menu starts the chosen entry by itself', r == 'saved' and restart() and var('LoaderEntrySelected') == 'rudolf-debug.conf', [r, var('LoaderEntrySelected')])
    ok('...and the choices are still there', var('LoaderEntryDefault') == 'rudolf-debug.conf' and var('LoaderConfigTimeout') == '2')
    r = js("rdPost('bootmenu/set?default=&timeout=').then(()=>'saved',e=>e.message)")
    ok('"As on the disc" removes them again', r == 'saved' and var('LoaderEntryDefault') == '' and var('LoaderConfigTimeout') == '', [r, var('LoaderEntryDefault'), var('LoaderConfigTimeout')])
    sh('/mnt/new/bo.sh', 30)
    r = js("rdPost('bootmenu/windows?n=0042').then(r=>r.message,e=>e.message)")
    ok('Restart into Windows sets the next start (BootNext 0042)', r == 'Windows starts at the next restart' and var('BootNext', GG) == 'B', [r, repr(var('BootNext', GG))])
    r = js("rdPost('bootmenu/windows?n=0001').then(r=>r.message,e=>e.message)")
    ok('...and only to a Windows entry', r == 'that is not a Windows entry', r)
    SEEN.clear(); js("rdPost('power?action=reboot').catch(()=>0);1"); time.sleep(5)
    back = started(delay=150)   # the stand-in's start comes first: read the serial port all that time
    ser = ''.join(SEEN)
    out('firmware said:', ' | '.join(sorted(set(re.findall(r'BdsDxe:[^\r\n\x1b]{0,120}', ser))))[:800] or '(no BdsDxe lines)', '| serial bytes', len(ser))
    ok('the firmware started Windows Boot Manager (the stand-in)', 'RUDOLF-WINDOWS-STANDIN' in ser.replace('\x1b', '').replace('\r', ''), ser[-300:])
    ok('...once: the next start is Rudolf OS again', back and var('BootNext', GG) == '', var('BootNext', GG))
    vm.shot('end')
finally:
    vm.stop()
out('%d/%d passed' % (sum(R), len(R)))
