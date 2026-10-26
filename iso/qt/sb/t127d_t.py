# Rudolf OS 1.2.7 stage B in QEMU: Linux apps and the terminal on an installation updated from an older disc
# (UEFI, Secure Boot off).
#   python3 t127d_t.py DISC.iso
# DISC.iso: a 1.2.7 test disc (mkdisc.sh with RUDOLF_UKI_TEST_CMDLINE=rudolf.debug=1: a serial shell).
# The computer's data partition (RUDOLFDATA, ext4) is made the way an older installation can leave it: Flatpak's
# repository without its tmp folder, and a Debian folder that says it is set up but is not a working system (no /proc
# in it), owned by root instead of Debian's shifted root (200000). A real laptop (1.2.6) failed with
# "open(O_TMPFILE): No such file or directory" in the App Store and "the Debian sandbox did not start" in the terminal.
#   - the start-up repair makes Flatpak's tmp folder again (and says so in /run/rudolf/repairs)
#   - rudolf-fscheck (rudolfd debian/check): the data partition's kind, temporary files everywhere, user namespaces
#   - the Debian sandbox names the step that failed and why, instead of only "did not start"
#   - Settings > Apps > Linux > Check shows the problems; the system report has them
import os, sys, time, json, re, subprocess, shutil
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
ISO = os.path.abspath(sys.argv[1])
PARTS = os.environ['SB_PARTS']; LOG = open('t127d.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:300] if d != '' else ''))
def last(t): return t.strip().split('\n')[-1].strip() if t.strip() else ''
def J(v, d):
    try: return json.loads(v) if isinstance(v, str) else d
    except ValueError: return d
# the data partition, as an older installation left it
D = os.path.abspath('data127d'); shutil.rmtree(D, ignore_errors=True)
os.makedirs(f'{D}/flatpak/repo/objects'); open(f'{D}/flatpak/repo/config', 'w').write('[core]\nrepo_version=1\nmode=bare-user-only\n')
os.makedirs(f'{D}/debian/etc'); open(f'{D}/debian/etc/rudolf-debian', 'w').write('13.7\n'); open(f'{D}/debian/.rudolf-ready', 'w').write('')
os.makedirs(f'{D}/home/rudolf'); os.makedirs(f'{D}/etc')
DATA = os.path.abspath('data127d.img')
if os.path.exists(DATA): os.remove(DATA)
subprocess.run(['truncate', '-s', '2G', DATA], check=True)
subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'RUDOLFDATA', '-m', '1', '-d', D, DATA], check=True)
vm = sbvm.VM('t127d', PARTS, [ISO, DATA], secure=False, mem=3072)
g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q
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
        time.sleep(3)
    return False
try:
    time.sleep(45); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    ok('starts, keeping files on the older data partition', up and last(sh('mountpoint -q /persist && echo kept', 20)) == 'kept')
    o = sh('cat /run/rudolf/repairs', 20); d = last(sh('[ -d /persist/flatpak/repo/tmp ] && echo made', 20))
    ok('the start-up repair made Flatpak\'s missing tmp folder again, and said so', 'repaired: /persist/flatpak/repo/tmp' in o and d == 'made', [o[-200:], d])
    o = sh('/usr/bin/rudolf-fscheck', 120); out(o[-1500:])
    kv = dict(l.split('=', 1) for l in o.replace('\r', '').split('\n') if '=' in l and not l.startswith(('echo', '/usr')))
    ok('rudolf-fscheck: the data partition is mounted, ext4 with extents', kv.get('persist') == 'mounted' and kv.get('persist_kind') == 'ext4' and 'extent' in kv.get('persist_features', ''), {k: kv.get(k) for k in ('persist', 'persist_kind', 'persist_features')})
    ok('...temporary files work on it and in Flatpak\'s repository, /var/tmp and /tmp', all(kv.get(k) == 'ok' for k in ('tmpfile_persist', 'tmpfile_flatpak', 'tmpfile_flatpak_repo', 'tmpfile_var_tmp', 'tmpfile_tmp')), {k: v for k, v in kv.items() if k.startswith('tmpfile')})
    ok('...user namespaces work', kv.get('userns') == 'ok', kv.get('userns'))
    ok('...and a sandbox may mount its own /proc (1.2.8)', kv.get('proc_mount') == 'ok', kv.get('proc_mount'))
    ok('...the Debian folder\'s owner is reported as wrong', 'expected 200000' in kv.get('debian_owner', ''), kv.get('debian_owner'))
    ok('...and the Debian sandbox names the step that failed and why (before 1.2.7: only "did not start")', re.search(r'did not start: .+ failed \(.+\)', kv.get('debian', '')), kv.get('debian'))
    # the page: Settings > Apps > Linux > Check, and the system report
    end = time.time() + 600
    while time.time() < end and g.js('typeof lxCheckShow=="function"&&typeof sysReport=="function"', 20) is not True: time.sleep(5)
    js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();svj('bd95setup',1);const b=[...document.querySelectorAll('button')].find(b=>b.textContent.trim()=='Guest');if(b)b.click();return 1})()")
    until('!!USER', 120)
    r = J(js("lxCheck().then(r=>JSON.stringify(r))", 120), {})
    ok('rudolfd debian/check gives the same, as JSON', r.get('ok') is True and r.get('persist_kind') == 'ext4' and 'did not start' in r.get('debian', ''), r)
    t = js("(async()=>{const b=document.createElement('div');document.body.appendChild(b);await lxCheckShow(b);const t=b.innerText;b.remove();return t})()", 120)
    ok('Settings › Apps › Linux › Check lists the problems', isinstance(t, str) and re.search(r'\d problem\(s\): .*Debian sandbox', t) and '⚠️' in t, (t or '')[:300])
    rep = js("sysReport('').then(r=>r.text)", 180)
    sec = re.search(r'== Linux apps and terminal\n(.*?)\n\n', rep or '', re.S)
    ok('the system report has a "Linux apps and terminal" section with the check', sec and 'Debian sandbox: the Debian sandbox did not start' in sec.group(1) and 'Kind: ext4' in sec.group(1), (sec.group(1) if sec else str(rep)[:200])[:300])
    vm.shot('end')
finally:
    vm.stop()
out('%d/%d passed' % (sum(R), len(R)))
