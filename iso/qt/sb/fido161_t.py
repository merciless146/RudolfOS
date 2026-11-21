# Rudolf OS 1.6.1 (docs/plan-1.6.1.md Part 18): security keys that unlock the files, on a real release in QEMU, Standard,
# with simulated FIDO2 keys. The keys are software CTAP2 authenticators on the host (iso/qt/host/vfido.py: ES256,
# hmac-secret, PIN protocol 1); in the guest, iso/qt/vfido_uhid.c makes each a real HID device with the kernel's uhid
# (a FIDO HID device, as a USB security key is: /dev/hidrawN), its reports carried over a serial port (QEMU's isa-serial
# on a socket: ttyS1), so what is tested is the guest's own libfido2 (hidraw, its udev enumeration) and rudolfd's checks,
# through the real page's Settings and its sign-in screen. Three keys: A1 and A2 (the account's), X (nobody's).
#   0. the release starts; this tree's page, rudolfd and libfido2 (1.6.1's ci_release.sh adds it) in place; the relay runs
#   1. an administrator's account on the disk (its encrypted home folder), a file in it
#   2. A1 plugged in: /dev/hidraw appears, rudolfd sees one key; Settings › Security keys › Add (the password, the touch
#      twice): one key, the account keeps its box
#   3. A2 plugged in too: added as the spare (A1 passed over); "Require a security key" (a touch): the password's copies
#      of the file key and of the administrator's key gone from the account (rudolfd's copy of the page's storage); the
#      keys' record root-only
#   4. signed out; the real sign-in screen (the name, the password, Enter): "Touch your security key", the touch (A2):
#      signed in, the same file key, the file there
#   5. no key plugged in: the sign-in says to plug it in, then Cancel: not signed in
#   6. only X (nobody's) plugged in: "not one of this account's keys", Cancel: not signed in
#   7. A1 again: signed in; the lock screen: the password, then the key: unlocked
#   8. rudolfd's native sign-in (Native's request, desktop/login): the password, then "need":"key"; with the touch, signed in
#   9. a copy of the disk (the drive image after the guest is stopped, read on the host): the account's storage and the
#      keys' record with the password open nothing; with A1's hmac-secret (from the software key's own state) the file key
#   python3 fido161_t.py REL-DIR    (SB_PARTS, QEMU, QEMU_ACCEL as for t160; root)
#   FIDO161_FIX=DIR: this tree's DIR/index.html, DIR/rudolfd and DIR/vfido_uhid (built static) put in place (an extra disk), and
#   DIR/lib*.so* (libfido2.so.1 and what it links) copied where the release has none (1.6.1's ci_release.sh adds them)
import os, sys, time, re, subprocess, shutil, struct, json, socket, threading, base64, hashlib, hmac
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT); sys.path.insert(0, os.path.join(QT, 'host'))
import sbvm, vfido
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); FIX = os.environ.get('FIDO161_FIX', '')
LOG = open('fido161.txt', 'w'); R = []; T0 = time.time()
def out(*a):
    s = ' '.join(str(x) for x in a); s = f'[{int(time.time() - T0):5d}s] ' + s; print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:1500] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 3000; DRV = os.path.abspath('fido161.img'); MEM = int(os.environ.get('MEM', '4096')); W = os.path.abspath('w161fido')
shutil.rmtree(W, ignore_errors=True); os.makedirs(W); os.chmod(W, 0o700)
def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    d = f'{T}/os/system/a'; os.makedirs(d)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs'): shutil.copy(f'{REL}/{n}', f'{d}/{n}')
    for n in ('manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{d}/{n}')
    open(f'{d}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes {mv('notes')}\n"); open(f'{d}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)
FD = os.path.join(W, 'fix'); os.makedirs(FD)
if FIX:
    for n in os.listdir(FIX): shutil.copy(f'{FIX}/{n}', f'{FD}/{n}')
FIXIMG = os.path.abspath('fido161fix.img'); subprocess.run(['truncate', '-s', '64M', FIXIMG], check=True)
subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'FIDOFIX', '-d', FD, FIXIMG], check=True)

# ---- the keys on the host, and the serial line to the guest's relay
KEYS = {i: vfido.Key(os.path.join(W, n)) for i, n in enumerate(('A1', 'A2', 'X'))}
VFS = '/tmp/vf-fido161.sock'
class Mux:
    """the host's end of the serial line: frames of 66 bytes (TYPE, KEY, 64 bytes) to and from vfido_uhid; each key's
    CTAPHID served by vfido over a socket pair"""
    def __init__(s): s.c = None; s.ends = {}; s.lock = threading.Lock(); s.hello = threading.Event(); s.plugged = set()
    def connect(s, secs=60):
        end = time.time() + secs
        while time.time() < end:
            try: c = socket.socket(socket.AF_UNIX); c.connect(VFS); s.c = c; break
            except OSError: time.sleep(1)
        threading.Thread(target=s.rx, daemon=True).start()
    def send(s, t, k, data=b''):
        with s.lock: s.c.sendall(bytes([t, k]) + data.ljust(64, b'\0'))
    def rx(s):
        buf = b''
        while True:
            try: d = s.c.recv(4096)
            except OSError: return
            if not d: return
            buf += d
            while len(buf) >= 66:
                f, buf = buf[:66], buf[66:]
                if f[0] == 2: s.hello.set()
                elif f[0] == 0 and f[1] in s.ends: s.ends[f[1]].sendall(f[2:])
    def plug(s, k, on):
        if on and k not in s.ends:
            a, b = socket.socketpair(); s.ends[k] = a
            threading.Thread(target=vfido.serve, args=(b, KEYS[k], lambda m: None), daemon=True).start()
            def back(a=a, k=k):
                buf = b''
                while True:
                    try: d = a.recv(4096)
                    except OSError: return
                    if not d: return
                    buf += d
                    while len(buf) >= 64: f, buf = buf[:64], buf[64:]; s.send(0, k, f)
            threading.Thread(target=back, daemon=True).start()
        if not on and k in s.ends: s.ends.pop(k).close()
        s.send(1, k, bytes([1 if on else 0])); (s.plugged.add if on else s.plugged.discard)(k)
MUX = Mux()

INIT_JS = "(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};return 1})()"
def boot(tag):
    try: os.unlink(VFS)
    except OSError: pass
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf160',
             '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1',
             '-drive', f'file={FIXIMG},format=raw,if=none,id=fx,readonly=on', '-device', 'nvme,drive=fx,serial=fido161fix',
             '-chardev', f'socket,id=vf,path={VFS},server=on,wait=off', '-device', 'isa-serial,chardev=vf,index=1']
    vm = sbvm.VM('fido161' + tag, PARTS, [], secure=False, mem=MEM, extra=extra)
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    time.sleep(30); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    return vm, g, up
def run(g, c, t=60, show=True):
    f = os.path.join(W, 'c.sh'); open(f, 'w').write(c + '\n'); g.put(f, '/tmp/c161f.sh', '644')
    o = g.sh('sh /tmp/c161f.sh 2>&1', t)
    if show: out('$', c[:160].replace('\n', ' ; ')); out(o.rstrip()[-2500:])
    return o
def jsfn(g):
    def js(e, t=90):
        for _ in range(5):
            r = g.js(e, t)
            if not (isinstance(r, tuple) and (r[0] == 'ERR' or (r[0] == 'EXC' and 'ReferenceError' in str(r[1])))): return r
            time.sleep(10)
        return r
    return js
def wait_for(js, expr, secs, every=2):
    end = time.time() + secs; r = None
    while time.time() < end:
        r = js(expr, 60)
        if r is True: return True
        time.sleep(every)
    return r
TOK = "$(busybox wget -q -O- http://127.0.0.1:8080/ | grep -o 'token:\"[0-9a-f]*' | cut -d'\"' -f2)"
def api(sh, p, body=None):
    if body is not None:
        f = os.path.join(W, 'body.json'); open(f, 'w').write(json.dumps(body))
        return sh(f"cat > /tmp/b161.json <<'EOF'\n{json.dumps(body)}\nEOF\nbusybox wget -S -q -O- --header \"X-Rudolf-Token: {TOK}\" --post-file=/tmp/b161.json http://127.0.0.1:8080/api/{p} 2>&1; rm -f /tmp/b161.json", 90, False)
    return sh(f"busybox wget -q -O- --header \"X-Rudolf-Token: {TOK}\" http://127.0.0.1:8080/api/{p}", 60, False)
def jparse(o):
    try: return json.loads(o[o.index('{'):o.rindex('}') + 1])
    except Exception: return {'raw': o[-400:]}
FKHEX = "(FKRAW?[...FKRAW].map(x=>x.toString(16).padStart(2,'0')).join(''):'')"
# the Settings dialogs, driven as a person would (fido61.js): the add dialog's name and password, Continue
ADD = "(n=>{document.querySelector('.fka').click();setTimeout(()=>{const d=document.querySelector('.fidoask');d.querySelector('.fnm').value=n;d.querySelector('.fpw').value='b1';d.querySelector('.y').click()},500);return 1})"
SIGNIN = "(pw=>{lu.value='boss';lp.value=pw;lp.dispatchEvent(new Event('input'));tryLogin();return LP.get()===pw?'typed':'not typed'})"

make_drive(DRV)
vm = None
try:
    vm, g, up = boot('s'); js = jsfn(g); sh = lambda c, t=60, show=True: run(g, c, t, show)
    ok('0. the release starts (Standard)', up)
    if not up: raise RuntimeError('did not start')
    end = time.time() + 600
    while time.time() < end and js("typeof acctNew=='function'&&typeof userLogin=='function'", 20) is not True: time.sleep(5)
    o = sh("FX=; for d in /dev/nvme*n1; do blkid $d 2>/dev/null | grep -q FIDOFIX && FX=$d; done; echo fix=$FX; mkdir -p /tmp/fx; mount -o ro $FX /tmp/fx; ls -la /tmp/fx; "
           "WWW=/usr/share/rudolf/www; [ -f /persist/os/current/index.html ] && WWW=/persist/os/current; "
           "[ -f /tmp/fx/index.html ] && cp /tmp/fx/index.html /tmp/page.html && chmod 644 /tmp/page.html && mount --bind /tmp/page.html $WWW/index.html && echo page; "
           "L=/usr/lib/x86_64-linux-gnu; for f in /tmp/fx/lib*.so*; do [ -f $f ] && { [ -e $L/$(basename $f) ] || { cp $f $L/ && chmod 644 $L/$(basename $f) && echo lib $(basename $f); }; }; done; "
           # (this tree's rudolfd bound over the release's, so whatever starts it again starts this one; then the old one stopped)
           "if [ -x /tmp/fx/rudolfd ]; then cp /tmp/fx/rudolfd /tmp/rudolfd.fix; chmod 755 /tmp/rudolfd.fix; B=$(command -v rudolfd || echo /usr/bin/rudolfd); mount --bind /tmp/rudolfd.fix $B; "
           # (the running one is found by its command line too: its process name is not always rudolfd's)
           "rp() { { pgrep -x rudolfd; pgrep -f '^/usr/bin/rudolfd'; pgrep -f '^rudolfd'; } | sort -u | tr '\\n' ' '; }; "
           "for t in 1 2 3; do P=$(rp); [ -n \"$P\" ] && kill $P; sleep 2; P=$(rp); [ -n \"$P\" ] && kill -9 $P; sleep 1; "
           "echo try $t: $(pgrep -a rudolf | tr '\\n' ';'); [ -n \"$(rp)\" ] || { setsid /tmp/rudolfd.fix >> /tmp/rd.fix.log 2>&1 < /dev/null & }; sleep 4; echo now: $(pgrep -a rudolf | tr '\\n' ';'); tail -3 /tmp/rd.fix.log; "
           f"busybox wget -q -O- --header \"X-Rudolf-Token: {TOK}\" 'http://127.0.0.1:8080/api/desktop/keys?acct=uX' 2>&1 | grep -q '\"lib\"' && {{ echo rudolfd ours $(pgrep -x rudolfd); break; }}; done; "
           "for p in $(rp); do ls -l /proc/$p/exe; done; tail -5 /run/rudolf/rudolfd.log; (ss -ltnp 2>/dev/null || netstat -ltnp 2>/dev/null) | grep 8080; fi; "
           "ls -la /dev/uhid /dev/ttyS1; cp /tmp/fx/vfido_uhid /tmp/vfido_uhid; chmod 755 /tmp/vfido_uhid; setsid /tmp/vfido_uhid /dev/ttyS1 > /tmp/vfido.log 2>&1 < /dev/null & sleep 1; echo relay $(pgrep -f vfido_uhid); "
           "pkill -f '[-]-rudolf-kiosk'; echo reloaded", 120)
    MUX.connect(); MUX.send(3, 0); got = MUX.hello.wait(30)   # (its first hello went before the host was there: asked again)
    ok('0b. this tree\'s page, rudolfd and libfido2 in place; the relay runs on ttyS1 and answers the host', 'page' in o and 'rudolfd ours' in o and got, o)
    time.sleep(20); end = time.time() + 600
    while time.time() < end and js("typeof acctNew=='function'&&typeof fidoSignin=='function'", 20) is not True: time.sleep(5)
    time.sleep(20); js(INIT_JS)
    # (what the key's dialog says, kept as it changes: the touch is quick)
    js("(()=>{const _d=fidoDlg;fidoDlg=function(t){const D=_d(t),m=D.msg;D.msg=(x,e)=>{(window.__fm=window.__fm||[]).push(x);return m(x,e)};return D};return 1})()")
    # 1. the account
    r = js("(async()=>{ACCTS.length=0;const b=await acctNew('boss','b1','admin');ACCTS.push(b);saveUser();return b.id})()", 120); out('account', r); ID = r
    r = js("(async()=>{try{const a=ACCTS.find(x=>x.n=='boss');if(USER)userLogout();USER=a;await userLogin(a,'b1');lastInput=Date.now();"
           "const L=document.getElementById('login');if(L)L.style.display='none';return 'in '+USER.n}catch(e){return 'ERR '+e.message}})()", 300)
    out('sign-in:', r); disk = wait_for(js, "!!(USER&&USER.disk&&hdOn())", 300); time.sleep(5)
    js("(()=>{HOME.DOCUMENTS['KEEP161.TXT']='mine, before the keys';fsSave();fsFlush();return 1})()"); time.sleep(5)
    FK0 = js(FKHEX); ok('1. an administrator\'s account on the disk (its encrypted home folder), a file in it', disk is True and len(FK0 or '') == 64, (disk, FK0))
    # 2. A1 plugged in, added in Settings
    MUX.plug(0, True); time.sleep(4)
    o = sh("ls -la /dev/hidraw*; cat /sys/class/hidraw/hidraw*/device/uevent 2>/dev/null | grep -E 'HID_NAME|HID_ID'; tail -3 /tmp/vfido.log", 30)
    st = js(f"(async()=>JSON.stringify(await fidoReq('desktop/keys?acct={ID}')))()", 60)
    try: st = json.loads(st)
    except Exception: st = {'raw': st}
    out('desktop/keys:', st, '| wget:', api(sh, f'desktop/keys?acct={ID}'), sh("printf 'GET /api/desktop/keys?acct=uX HTTP/1.0\\r\\n\\r\\n' | busybox nc 127.0.0.1 8080 | tail -3; tail -20 /run/rudolf/rudolfd.log", 30, False))
    ok('2a. A1 plugged in: a FIDO HID device (/dev/hidraw), rudolfd (libfido2) sees one key', 'hidraw' in o and 'Rudolf OS test security key 1' in o and st.get('devs') == 1 and st.get('lib'), (o, st))
    js("settings('usr')"); time.sleep(5); js(ADD + "('Blue key')")
    r = wait_for(js, "!!(USER.fido&&USER.fido.n==1&&USER.fks)", 120); vm.shot('added-1')
    ok('2. Settings › Security keys › Add: the password, the key touched twice; one key, the account keeps its box', r is True, js("JSON.stringify({fido:USER.fido,fks:!!USER.fks,calls:document.querySelector('.fks')&&document.querySelector('.fks').textContent})"))
    # 3. A2 too: the spare; then required
    MUX.plug(1, True); time.sleep(4); js(ADD + "('Spare')")
    r = wait_for(js, "!!(USER.fido&&USER.fido.n==2)", 120)
    ok('3a. A2 plugged in too: added as the spare (A1 passed over)', r is True, js("JSON.stringify(USER.fido)"))
    time.sleep(3); js("(()=>{document.querySelector('.fkq').click();setTimeout(()=>{const d=document.querySelector('.fidoask');d.querySelector('.fpw').value='b1';d.querySelector('.y').click()},500);return 1})()")
    r = wait_for(js, "!!(USER.fido&&USER.fido.req&&!USER.fk&&!USER.priv)", 120); vm.shot('required')
    js("(()=>{try{saveUser();window.__rudolfLsFlush&&__rudolfLsFlush()}catch(e){}return 1})()"); time.sleep(8)
    o = sh(f"stat -c '%a %U' /persist/etc/fido/{ID}.json /persist/etc/fido", 30)
    ls = jparse(api(sh, 'lsync?key=bd95user')); acc = {}
    try: acc = [a for a in json.loads(ls.get('value') or ls.get('v') or '{}').get('accts', []) if a.get('id') == ID][0]
    except Exception: pass
    ok('3. required (a touch): the password\'s copies of the file key and the administrator\'s key gone from the account, as rudolfd keeps it; the record root-only',
       r is True and acc and 'fk' not in acc and 'priv' not in acc and 'fks' in acc and o.count('600 root') == 1 and '700 root' in o, (r, o, list(acc.keys()) if acc else ls))
    # 4. the real sign-in screen with the key
    js("(()=>{userLogout();showLogin();return 1})()"); time.sleep(5)
    r = js(SIGNIN + "('b1')"); out('typed:', r)
    seen = wait_for(js, "(window.__fm||[]).some(x=>/touch your security key/.test(x))", 60); msg = js("JSON.stringify(window.__fm||[])")
    r = wait_for(js, "!!(USER&&USER.n=='boss'&&getComputedStyle(document.getElementById('login')).display=='none'&&FKRAW)", 120); time.sleep(5); vm.shot('signed-in')
    after = js("JSON.stringify({fk:" + FKHEX + ",doc:HOME&&HOME.DOCUMENTS&&HOME.DOCUMENTS['KEEP161.TXT'],disk:!!(USER.disk&&hdOn())})")
    a4 = json.loads(after) if isinstance(after, str) else {}
    ok('4. the real sign-in screen: "Touch your security key", the touch: signed in, the same file key, the file there',
       seen is True and r is True and a4.get('fk') == FK0 and a4.get('doc') == 'mine, before the keys', (seen, msg, r, after))
    # 5. no key
    MUX.plug(0, False); MUX.plug(1, False); time.sleep(3)
    js("(()=>{userLogout();showLogin();return 1})()"); time.sleep(5); js(SIGNIN + "('b1')")
    wait_for(js, "!!document.querySelector('.fidodlg')", 30); t1 = js("(document.querySelector('.fidodlg .fidom')||{}).textContent||''")
    err = wait_for(js, "/Plug in your security key/.test((document.querySelector('.fidodlg .fidom')||{}).textContent||'')", 60); vm.shot('no-key')
    js("(()=>{const d=document.querySelector('.fidodlg');d&&d.querySelector('.n').click();return 1})()"); time.sleep(3)
    ok('5. no key plugged in: the sign-in says to plug it in (after the wait), Cancel: not signed in', err is True and js("getComputedStyle(document.getElementById('login')).display!='none'") is True, (t1, err))
    # 6. only X
    MUX.plug(2, True); time.sleep(4); js(SIGNIN + "('b1')")
    err = wait_for(js, "/not one of this account/.test((document.querySelector('.fidodlg .fidom')||{}).textContent||'')", 90); vm.shot('wrong-key')
    js("(()=>{const d=document.querySelector('.fidodlg');d&&d.querySelector('.n').click();return 1})()"); time.sleep(3)
    ok('6. only a key that is nobody\'s: "not one of this account\'s keys", Cancel: not signed in', err is True and js("getComputedStyle(document.getElementById('login')).display!='none'") is True, err)
    MUX.plug(2, False); sh("rm -f /run/rudolf/second-*", 20, False)   # (the refusals above counted: not to wait here)
    # 7. A1 again; the lock screen
    MUX.plug(0, True); time.sleep(4); js(SIGNIN + "('b1')")
    r = wait_for(js, "!!(USER&&USER.n=='boss'&&getComputedStyle(document.getElementById('login')).display=='none'&&FKRAW)", 120)
    js("lockScreen()"); time.sleep(3)
    js("(()=>{const i=document.querySelector('#lockov .lkp');i.value='b1';i.dispatchEvent(new Event('input'));document.querySelector('#lockov .lku').click();return 1})()")
    lk = wait_for(js, "!document.getElementById('lockov')", 90)
    ok('7. A1 again: signed in; the lock screen: the password, then the key: unlocked', r is True and lk is True, (r, lk))
    # 8. rudolfd's native sign-in (Native's request)
    # (through the page's own fetch with the token: busybox wget gives no body with a 401)
    o1 = js("(async()=>JSON.stringify(await fidoReq('desktop/login',{n:'boss',p:'b1'})))()", 90)
    o2 = js("(async()=>JSON.stringify(await fidoReq('desktop/login',{n:'boss',p:'b1',k:true})))()", 90)
    s2 = jparse(api(sh, 'desktop/session')); sh("rm -f /run/rudolf/native-session", 20, False)
    ok('8. rudolfd\'s native sign-in (desktop/login): the password, then "need":"key"; with the touch, signed in with the same file key',
       '"need":"key"' in str(o1) and '"ok":true' in str(o2) and s2.get('fk') == FK0, (str(o1)[-300:], str(o2)[-300:], {k: (v if k not in ('pk', 'fk') else len(v)) for k, v in s2.items()}))
    # what the guest kept (for the offline part)
    LSJ = api(sh, 'lsync?key=bd95user')
    sh("sync; sync", 30, False)
finally:
    if vm:
        try: vm.q.cmd('quit')
        except Exception: pass
        time.sleep(5)
# 9. a copy of the disk, read on the host
try:
    part = os.path.join(W, 'data.img'); subprocess.run(['dd', f'if={DRV}', f'of={part}', 'bs=1M', 'skip=1', f'count={DMB}', 'status=none'], check=True)
    def dump(p):
        d = os.path.join(W, 'dump'); subprocess.run(['debugfs', '-R', f'dump {p} {d}', part], capture_output=True)
        try: return open(d, 'rb').read()
        finally:
            try: os.unlink(d)
            except OSError: pass
    rec = json.loads(dump(f'/etc/fido/{ID}.json') or b'{}'); lsf = dump('/home/rudolf/.rudolf-ls.json')
    accts = json.loads(json.loads(lsf)['data']['bd95user'])['accts'] if lsf else []
    a = [x for x in accts if x.get('id') == ID][0] if accts else {}
    from cryptography.hazmat.primitives.ciphers.aead import AESGCM
    pk = hashlib.pbkdf2_hmac('sha256', b'b1', base64.b64decode(a['ks']), 210000, 32) if a else b''
    def hk(ikm, salt): prk = hmac.new(salt, ikm, 'sha256').digest(); return hmac.new(prk, ID.encode() + b'\x01', 'sha256').digest()
    def opens(key, b):
        try: return AESGCM(key).decrypt(b[:12], b[12:], None)
        except Exception: return None
    pws = opens(pk, base64.b64decode(a['fks']['iv']) + base64.b64decode(a['fks']['ct'])) if a.get('fks') else None
    fkb = base64.b64decode(rec.get('fk', '')) if rec else b''
    pw_only = {'fk' in a, 'priv' in a, bool(rec.get('kp'))}
    ok('9a. a copy of the disk (read on the host): the account has no password copy, the record no password copy of the key secret, so the password alone opens nothing',
       bool(rec) and bool(a) and pw_only == {False} and pws is not None, (list(a.keys()), list(rec.keys())))
    got = []
    for i, k in ((0, 'A1'), (1, 'A2')):
        for kk in rec.get('keys', []):
            h = vfido.offline_hmac(os.path.join(W, k), kk['cid'], rec['salt'])
            if h is None: continue
            ks = opens(hk(h, b'rudolf-os security key v1'), base64.b64decode(kk['ks']))
            fk = ks and pws and opens(hk(pws + ks, b'rudolf-os file key v1'), fkb)
            got.append(fk.hex() if fk else None)
    ok('9. ...with the password and either key\'s hmac-secret: the file key the account used', got and all(x == FK0 for x in got) and len(got) == 2, got)
except Exception as e:
    ok('9. a copy of the disk, read on the host', False, repr(e))
out('%d/%d passed' % (sum(R), len(R)))
sys.exit(0 if R and all(R) else 1)
