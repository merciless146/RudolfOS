# Rudolf OS 1.4.4 in QEMU (UEFI, Secure Boot off): the release's kernel started directly (as an update's handover
# does) with its system in slot a of an NVMe drive's RUDOLFDATA, and the machine's clock at 2016-01-01.
#   python3 t144k_t.py REL-DIR      (REL-DIR: a release's vmlinuz, *.sfs, manifest.txt, manifest.sig)
# Checks: the clock floor; the laptop hotkey drivers are there; no sit0; the volume keys (QEMU's volumeup, audiomute
# through the PS/2 keyboard) reach the sound server and the desktop's indicator; the brightness without a backlight;
# rudolfd's idle; Power and battery on a machine without battery, lid or power modes; the power button (QEMU's ACPI
# button) with Ask; the screen off after a minute and on again at a key; the network icon on a wired card.
import os, sys, time, re, subprocess, shutil, struct, json
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); LOG = open('t144k.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:300] if d != '' and not c else ''))
def last(t): return t.strip().split('\n')[-1].strip() if t.strip() else ''
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 1500; DRV = os.path.abspath('int144.img')

def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    a = f'{T}/os/system/a'; os.makedirs(a)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs', 'manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{a}/{n}')
    open(f'{a}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n")
    open(f'{a}/complete', 'w').write('1\n'); open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)

make_drive(DRV)
EXTRA = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf144',
         '-rtc', 'base=2016-01-01T00:00:00', '-audiodev', 'none,id=snd0', '-device', 'intel-hda', '-device', 'hda-duplex,audiodev=snd0',
         '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.trying=1 rudolf.debug=1']
vm = sbvm.VM('t144k', PARTS, [], secure=False, mem=3072, extra=EXTRA)
g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3); sh = lambda c, t=60: g.sh(c, t)
def js(e, t=60):
    for _ in range(3):
        r = g.js(e, t)
        if not (isinstance(r, tuple) and (r[0] == 'ERR' or (r[0] == 'EXC' and 'ReferenceError' in str(r[1])))): return r
        time.sleep(15)
    return r
def J(e, t=60):
    v = js(f"Promise.resolve({e}).then(r=>JSON.stringify(r),x=>JSON.stringify({{err:String(x&&x.message||x)}}))", t)
    try: return json.loads(v) if isinstance(v, str) else {'raw': v}
    except ValueError: return {'raw': v}
try:
    time.sleep(45); up = False; end = time.time() + 900
    while time.time() < end:
        try:
            if 'ready' in sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    ok('1. Rudolf OS starts with the release kernel', up)
    if not up: raise SystemExit
    rel = mv('released'); d = last(sh('date -u +%F', 10))
    ok(f'2. the machine\'s clock read 2016-01-01: at least the release date ({rel}) after the start', d >= rel, [d, last(sh('cat /run/rudolf/clock-floored; echo', 10))])
    ok('...and the start-up says it floored it', '2016-01-01' in sh('cat /run/rudolf/clock-floored', 10))
    mods = sh("for m in asus-nb-wmi asus-wmi ideapad-laptop thinkpad_acpi hp-wmi dell-wmi acer-wmi; do find /lib/modules -name \"$m.ko*\" | head -1 | grep -q . && echo \"have $m\" || echo \"MISSING $m\"; done", 30)
    have = [l for l in mods.split('\n') if l.startswith('have ')]; miss = [l for l in mods.split('\n') if l.startswith('MISSING ')]
    ok('3. the laptop hotkey drivers are there, as modules', not miss and len(have) == 7, mods)
    ok('4. no sit0 (the IPv6-in-IPv4 tunnel is not built)', last(sh('[ -e /sys/class/net/sit0 ] && echo yes || echo no', 10)) == 'no')
    # the desktop
    end = time.time() + 600
    while time.time() < end and js('typeof osdShow=="function"&&typeof pwrPoll=="function"', 20) is not True: time.sleep(5)
    js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};const b=[...document.querySelectorAll('button')].find(b=>b.textContent.trim()=='Guest');if(b)b.click();return 1})()")
    time.sleep(10)
    # the hooks are put in again when the page has been loaded again (signing in as Guest reloads it)
    HOOK = "(()=>{if(!window.__ev){window.__ev=[];evOn(t=>{if(t)__ev.push(t)})}if(!window.__offh){window.__offh=1;window.__offs=0;A.off=()=>{__offs++}}window.__ev.length=0;return 1})()"
    hook = lambda: js(HOOK)
    hook()
    n = J("rd('net')")
    ifs = {i['name']: i for i in n.get('interfaces', [])}
    ok('5. the network: the wired card, not virtual (every interface says whether it is)', 'eth0' in ifs and ifs['eth0'].get('virtual') is False and all('virtual' in i for i in ifs.values()), ifs)
    ok('...the icon says wired', js("$('#net').dataset.type") == 'wired', js("$('#net').title"))
    # volume keys
    v0 = last(sh('/usr/lib/rudolf/hotkey volume get', 30)); out('sound server volume:', v0)
    if v0 and v0 != 'none':
        hook(); g.key('volumedown'); time.sleep(3); v0 = last(sh('/usr/lib/rudolf/hotkey volume get', 30)); hook(); g.key('volumeup'); time.sleep(3); v1 = last(sh('/usr/lib/rudolf/hotkey volume get', 30))
        ok('6. the volume-up key raises the system volume by 5 %', v1.split()[0].isdigit() and int(v1.split()[0]) == min(100, (int(v0.split()[0]) // 5 + 1) * 5), [v0, v1])
        ok('...and the desktop hears it (osd volume) and shows the indicator', any('osd volume' in e for e in js('__ev') or []) and js("!!document.getElementById('osd')") is True, js('__ev'))
        g.key('audiomute'); time.sleep(3)
        ok('...the mute key mutes', 'muted' in sh('/usr/lib/rudolf/hotkey volume get', 30))
        g.key('audiomute'); time.sleep(2)
    else:
        out('NOTE no sound server in this machine: the volume keys are checked through their events only')
        hook(); g.key('volumeup'); time.sleep(3)
        ok('6. the volume-up key runs hotkey (osd volume none: no sound output)', any('osd volume' in e for e in js('__ev') or []), js('__ev'))
    hook(); sh('/usr/lib/rudolf/hotkey brightness up', 20); time.sleep(3)
    ok('7. brightness without a backlight: the indicator says so, nothing fails', any('osd brightness none' in e for e in js('__ev') or []) and 'cannot' in (js("$('#osd')&&$('#osd').textContent") or ''), [js('__ev'), js("$('#osd')&&$('#osd').textContent")])
    i0 = J("rd('idle')").get('idle'); g.key('shift'); time.sleep(2); i1 = J("rd('idle')").get('idle')
    ok('8. rudolfd\'s idle: it drops when a key is pressed', i1 is not None and (i0 is None or i1 < 5000), [i0, i1])
    p = J("rd('power/status')")
    ok('9. Power and battery on QEMU: no battery, no lid, no power modes; reported', p.get('battery') is None and p.get('can', {}).get('lid') is False and p.get('can', {}).get('profile') is False and p.get('ac') is True, p)
    ok('...no battery icon on the taskbar', js("!$('#bat')||$('#bat').style.display=='none'") is True)
    hook(); vm.q.cmd('system_powerdown'); time.sleep(5)
    ok('10. the power button (QEMU\'s ACPI button) with Ask: the desktop\'s Shut down dialog', isinstance(js('__offs'), int) and js('__offs') >= 1, [js('__offs'), js('__ev')])
    r = J("rdPost('power/set?screen_ac=1')")
    ok('11. Turn the screen off after 1 minute (plugged in)', r.get('set', {}).get('screen_ac') == 1, r)
    sh('rm -f /run/rudolf/screen-off', 10); time.sleep(80)
    s1 = last(sh('cat /run/rudolf/screen-off 2>/dev/null; echo', 10)); g.key('shift'); time.sleep(3); s2 = last(sh('cat /run/rudolf/screen-off 2>/dev/null; echo', 10))
    ok('...after a minute without input the screen is off, and a key turns it on', s1 == '1' and s2 == '0', [s1, s2])
    J("rdPost('power/set?screen_ac=10')")
    t = J("rd('time/status')")
    ok('12. the clock from the internet: no network here, so nothing set and nothing failed (rudolfd answers)', t.get('ok') is True, t)
finally:
    vm.stop()
try: os.remove(DRV)
except OSError: pass
out(f'{sum(R)}/{len(R)} passed')
