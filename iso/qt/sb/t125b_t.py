# Rudolf OS 1.2.5 stage B in QEMU: the boot menu's default entry and wait, and "Restart into Windows" (UEFI,
# Secure Boot off, EFI variables kept across restarts).
#   python3 t125b_t.py DISC.iso WIN.img [NEW.img]
# DISC.iso: a test disc (mkdisc.sh with RUDOLF_UKI_TEST_CMDLINE=rudolf.debug=1: a serial shell).
# WIN.img: mkwindisk.py's stand-in Windows disk: an EFI system partition with \EFI\Microsoft\Boot\bootmgfw.efi
#   (winstub.c: prints RUDOLF-WINDOWS-STANDIN, waits 3 s, restarts) and RUDOLF/BOOTOPT.BIN, the firmware entry
#   Windows Setup would add, which the test stores as Boot0042.
# NEW.img (optional): an ext4 disk labelled RUDOLFB with a newer rudolfd and index.html, put in place after each
#   start (to test them on a disc whose layers are older; the RAM overlay forgets them at the next restart).
import os, sys, time, json
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
ISO, WIN = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2]); NEW = os.path.abspath(sys.argv[3]) if len(sys.argv) > 3 else None
PARTS = os.environ['SB_PARTS']; LOG = open('t125b.txt', 'w'); R = []
LG = '4a67b082-0a4c-41cf-b6c7-440b29bb8c4f'; GG = '8be4df61-93ca-11d2-aa0d-00e098032b8c'; EV = '/sys/firmware/efi/efivars'


def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:300] if d != '' else ''))
def last(t): return t.strip().split('\n')[-1].strip() if t.strip() else ''

drives = [ISO, WIN] + ([NEW] if NEW else [])
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
            if 'ready' in g.sh('echo ready', 8): return True
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
    return last(sh(f"f={EV}/{name}-{guid}; [ -e $f ] && tail -c +5 $f | tr -d '\\000' || echo ''", 20))
def started():
    """after a start: the serial shell, the newer rudolfd and page when given, the desktop signed in"""
    if not wait_shell(): return False
    if NEW:
        sh("mkdir -p /mnt/new; grep -q ' /mnt/new ' /proc/mounts || mount -o ro $(findfs LABEL=RUDOLFB) /mnt/new; "
           "cp /mnt/new/index.html /usr/share/rudolf/www/index.html; pkill -x rudolfd; sleep 1; "
           "(/mnt/new/rudolfd > /run/rudolf/rudolfd.log 2>&1 &); for i in 1 2 3 4 5 6 7 8 9 10; do wget -q -O /dev/null http://127.0.0.1:8080/ && break; sleep 1; done; echo restarted", 60)
        # reload the page from the new rudolfd (a reload that came too early shows Chromium's error page: try again)
        for _ in range(4):
            g.js("location.replace('http://127.0.0.1:8080/');1", 10); time.sleep(15)
            if wait_desktop(90) and g.js('typeof rdPost=="function"', 20) is True: break
    if not wait_desktop(): return False
    js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();svj('bd95setup',1);const b=[...document.querySelectorAll('button')].find(b=>b.textContent.trim()=='Guest');if(b)b.click();return 1})()")
    until('!!USER', 120); js("USER.r='admin';window.elevate=async()=>true;1")
    return True
def restart():
    js("rdPost('power?action=reboot').catch(()=>0);1"); time.sleep(8); return started()

try:
    ok('starts (UEFI, the debug entry has the serial shell)', started())
    # the Windows Boot Manager entry Windows Setup would have added (Boot0042), from the stand-in disk
    p = last(sh('findfs LABEL=WINSTANDIN'))
    out(sh(f"mkdir -p /mnt/win; mount -o ro {p} /mnt/win; ls -R /mnt/win | head; printf '\\007\\000\\000\\000' > /tmp/bo; cat /mnt/win/RUDOLF/BOOTOPT.BIN >> /tmp/bo; "
           f"dd if=/tmp/bo of={EV}/Boot0042-{GG} bs=4096 2>&1 | tail -1; ls {EV} | grep -c Boot0042", 30))
    m = js("rd('bootmenu').then(m=>JSON.stringify(m))"); m = json.loads(m) if isinstance(m, str) else {}
    ok('bootmenu: UEFI, started from systemd-boot, with its entries', m.get('uefi') and m.get('loader') and 'rudolf-start.conf' in m.get('entries', []) and 'rudolf-recovery.conf' in m.get('entries', []), m)
    ok('...which entry started', m.get('selected') in m.get('entries', []) and m.get('selected', '').startswith('rudolf-'), m.get('selected'))
    ok('...and the Windows Boot Manager entry', m.get('windows') == [{'n': '0042', 'title': 'Windows Boot Manager'}], m.get('windows'))
    first = m.get('selected')
    # Settings › Update and recovery › Boot menu
    js("settings('upd');1")
    ok('Settings shows the boot menu choices', until("!!document.querySelector('.boot30 .bmd')&&document.querySelectorAll('.boot30 .bmd option').length>=4", 60), js("(document.querySelector('.boot30')||{}).innerText"))
    ok('...and Restart into Windows', until("[...document.querySelectorAll('.boot30 button')].some(b=>b.textContent=='Restart into Windows')", 20))
    r = js("rdPost('bootmenu/set?default=nonsense.conf').then(()=>'saved',e=>e.message)")
    ok('an entry the menu does not have is refused', r == 'that is not an entry of the boot menu', r)
    r = js("rdPost('bootmenu/set?timeout=60').then(()=>'saved',e=>e.message)")
    ok('...and a wait over 10 seconds', r == 'the wait is 0 to 10 seconds', r)
    js("(()=>{const b=document.querySelector('.boot30');b.querySelector('.bmd').value='rudolf-recovery.conf';b.querySelector('.bmt').value='2';b.querySelector('.bms').click();return 1})()")
    ok('Save stores both in the firmware', until("/Saved/.test(document.querySelector('.boot30 .bmm').textContent)", 30) and var('LoaderEntryDefault') == 'rudolf-recovery.conf' and var('LoaderConfigTimeout') == '2',
       [var('LoaderEntryDefault'), var('LoaderConfigTimeout')])
    # the restart itself uses the debug entry (the same start as the test disc's normal one, with its serial shell;
    # recovery mode would start a different desktop)
    r = js("rdPost('bootmenu/set?default=rudolf-debug.conf').then(()=>'saved',e=>e.message)")
    ok('after a restart the menu starts the chosen entry by itself', r == 'saved' and restart() and var('LoaderEntrySelected') == 'rudolf-debug.conf', [r, var('LoaderEntrySelected')])
    ok('...and the choices are still there', var('LoaderEntryDefault') == 'rudolf-debug.conf' and var('LoaderConfigTimeout') == '2')
    r = js("rdPost('bootmenu/set?default=&timeout=').then(()=>'saved',e=>e.message)")
    ok('"As on the disc" removes them again', r == 'saved' and var('LoaderEntryDefault') == '' and var('LoaderConfigTimeout') == '', [var('LoaderEntryDefault'), var('LoaderConfigTimeout')])
    # the Power dialog: Restart into Windows (the stand-in prints, waits 3 s and restarts the computer)
    sh(f"[ -e {EV}/Boot0042-{GG} ] || {{ mount -o ro $(findfs LABEL=WINSTANDIN) /mnt/win 2>/dev/null; dd if=/tmp/bo of={EV}/Boot0042-{GG} bs=4096 2>/dev/null; }}; true", 20)
    js("shutdownDlg();1")
    ok('the Power dialog has Restart into Windows', until("!!document.querySelector('.rmodal .bmwin')", 30))
    SEEN.clear(); js("document.querySelector('.rmodal .bmwin').click();1"); time.sleep(5)
    back = started()
    ser = ''.join(SEEN)
    ok('the firmware started Windows Boot Manager (the stand-in)', 'RUDOLF-WINDOWS-STANDIN' in ser.replace('\x1b', '').replace('\r', ''), ser[-300:])
    ok('...once: the next start is Rudolf OS again', back and var('BootNext', GG) == '', var('BootNext', GG))
    vm.shot('end')
finally:
    vm.stop()
out('%d/%d passed' % (sum(R), len(R)))
