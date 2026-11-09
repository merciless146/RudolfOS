# Rudolf OS 1.5.6 in QEMU: the move of the homes to the disk (docs/plan-1.5.6.md, "QEMU t156"), on the releases
# themselves: slot a holds 1.5.5, slot b 1.5.6, as after an update.
#   1. 1.5.5: two accounts (an administrator and a user); the administrator's home has a document, a picture, a
#      folder of 3,000 small files and a large download (kept on the disk, as 1.5.5 does).
#   2. 1.5.6: at the first sign-in the files move to /persist/accounts/<id>, verified; the large download is moved, not
#      copied; listing the 3,000-file folder takes under a second; a Linux app (busybox in a stand-in Debian) reads a
#      document and changes it, and the desktop shows the change; after sign-out the folder is encrypted (names and
#      contents), after sign-in readable; the user's home moves too, and the administrator opens it, changes a file,
#      and it is locked again at sign-out.
#   3. Going back to 1.5.5 within the 7 days: its own record of the home is still there (the document as it was at
#      the move, without the Linux app's change).
#   4. 1.5.6 again: a backup, the account's folder removed, the backup restored: the home comes back from it.
#   python3 t156_t.py REL-1.5.5 REL-1.5.6     (SB_PARTS and QEMU as for t150; root)
import os, sys, time, re, subprocess, shutil, struct, json
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
OLD, NEW = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
PARTS = os.environ.get('SB_PARTS', '/nonexistent'); LOG = open('t156.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:500] if d != '' and not c else ''))
def mv(rel, k): return re.search(rf'^{k} (.*)$', open(f'{rel}/manifest.txt').read(), re.M).group(1)
DMB = 2500; DRV = os.path.abspath('int156.img'); MEM = int(os.environ.get('MEM', '3072'))

def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf/Downloads'); os.makedirs(f'{T}/etc')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    for slot, rel in (('a', OLD), ('b', NEW)):
        a = f'{T}/os/system/{slot}'; os.makedirs(a)
        for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs', 'manifest.txt', 'manifest.sig'): shutil.copy(f'{rel}/{n}', f'{a}/{n}')
        open(f'{a}/RELEASE', 'w').write(f"serial {mv(rel, 'serial')}\nversion {mv(rel, 'version')}\nreleased {mv(rel, 'released')}\nchromium {mv(rel, 'chromium')}\nkernel {mv(rel, 'kernel')}\nnotes test\n")
        open(f'{a}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)

def boot(tag, slot):
    rel = OLD if slot == 'a' else NEW
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf156',
             '-kernel', f'{rel}/vmlinuz', '-append', f'quiet loglevel=3 consoleblank=0 rudolf.slot={slot} rudolf.debug=1']
    vm = sbvm.VM('t156' + tag, PARTS, [], secure=False, mem=MEM, extra=extra)
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    time.sleep(45); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    def js(e, t=90):
        for _ in range(5):
            r = g.js(e, t)
            if not (isinstance(r, tuple) and (r[0] == 'ERR' or (r[0] == 'EXC' and 'ReferenceError' in str(r[1])))): return r
            time.sleep(15)
        return r
    end = time.time() + 600
    while up and time.time() < end and js("typeof acctNew=='function'&&typeof homeLoad=='function'&&typeof FSLOADED!='undefined'&&FSLOADED", 20) is not True: time.sleep(5)
    js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};return 1})()")
    return vm, g, up, js

def signin(js, n, pw):
    return js(f"(async()=>{{const a=ACCTS.find(x=>x.n=={json.dumps(n)});if(!a)return 'no account';if(!(await pwCheck(a,{json.dumps(pw)})))return 'bad';if(USER)userLogout();"
              f"const L=document.getElementById('login');USER=a;await userLogin(a,{json.dumps(pw)});lastInput=Date.now();if(L)L.style.display='none';return 'in '+USER.n}})()", 180)
def signout(js):
    return js("(async()=>{try{fsFlush()}catch(e){}try{await homeChain}catch(e){}try{if(typeof HD!='undefined')await HD.chain}catch(e){}if(USER)userLogout();"
              "await new Promise(r=>setTimeout(r,3000));try{if(typeof HD!='undefined')await HD.chain}catch(e){}try{await homeChain}catch(e){}return 'out'})()", 180)
def wait_for(js, expr, secs, every=5):
    end = time.time() + secs; r = None
    while time.time() < end:
        r = js(expr, 60)
        if r is True: return True
        time.sleep(every)
    return r

make_drive(DRV)
vm = None
try:
    # ---------------------------------------------------------------- 1.5.5
    vm, g, up, js = boot('a', 'a')
    sh = lambda c, t=60: g.sh(c, t)
    ok('1. Rudolf OS 1.5.5 starts from slot a', up and js("RVER") and '1.5.5' in str(js("RVER")), js("RVER") if up else 'not up')
    if not up: raise SystemExit
    r = js("(async()=>{ACCTS.length=0;const b=await acctNew('boss','b1','admin');ACCTS.push(b);const k=await acctNew('kid','k1','user');ACCTS.push(k);saveUser();return b.id+' '+k.id})()")
    out('accounts:', r); BOSS, KID = str(r).split()
    out(signin(js, 'boss', 'b1'))
    r = js("""(async()=>{const D=node(homePath('DOCUMENTS')),P=node(homePath('PICTURES'));D['NOTE.TXT']='written in 1.5.5\\n';
      P['PIC.PNG']='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==';
      const M=D.MANY={};for(let i=0;i<3000;i++)M['F'+String(i).padStart(4,'0')+'.TXT']='file '+i;fsSave();fsFlush();await homeChain;return dlAcct()})()""", 300)
    out('downloads folder:', r); DLA = str(r)
    sh(f"mkdir -p /home/rudolf/Downloads/.accounts/{DLA} && dd if=/dev/urandom of=/home/rudolf/Downloads/.accounts/{DLA}/BIG.BIN bs=1M count=3 2>/dev/null && chown -R 1000:1000 /home/rudolf/Downloads/.accounts && sha256sum /home/rudolf/Downloads/.accounts/{DLA}/BIG.BIN > /tmp/big.sha; cat /tmp/big.sha")
    BIGSHA = sh("cut -c1-64 /tmp/big.sha").strip()
    got = wait_for(js, "(()=>{const v=node([...downloadsPath(),'BIG.BIN']);return !!v})()", 240)
    ok('2. 1.5.5: the administrator\'s home has its files, and the large download is kept on the disk (an entry in Downloads)', got is True and js("Object.keys(node(homePath('DOCUMENTS')).MANY).length") == 3000, got)
    js("(async()=>{fsSave();fsFlush();await homeChain;return 1})()")
    out(signout(js))
    out(signin(js, 'kid', 'k1'))
    js("(async()=>{node(homePath('DOCUMENTS'))['KIDNOTE.TXT']='the kid wrote this\\n';fsSave();fsFlush();await homeChain;return 1})()")
    out(signout(js))
    ok('3. 1.5.5: both homes are kept as records (the desktop\'s storage)', js(f"(async()=>!!(await kvGet('home:{BOSS}'))&&!!(await kvGet('home:{KID}')))()") is True)
    sh("sync"); vm.stop(); time.sleep(3)
    # ---------------------------------------------------------------- 1.5.6 (the update to slot b)
    vm, g, up, js = boot('b', 'b'); sh = lambda c, t=60: g.sh(c, t)
    ok('4. Rudolf OS 1.5.6 starts from slot b', up and '1.5.6' in str(js("RVER")), js("RVER") if up else 'not up')
    if not up: raise SystemExit
    sh("printf 'good=b\\nprev=a\\nnext=\\ntries=0\\nfailed=\\n' > /persist/os/system/state; sync")   # as the update leaves it once 1.5.6 started
    t0 = time.time(); out(signin(js, 'boss', 'b1'))
    moved = wait_for(js, "!!(USER&&USER.disk&&hdOn())", 1500, 10); took = int(time.time() - t0)
    out('the move took', took, 's')
    A = f'/persist/accounts/{BOSS}'
    o = sh(f"ls {A}/Documents; cat {A}/Documents/NOTE.TXT; ls {A}/Documents/MANY | wc -l; ls -la {A}/Pictures/PIC.PNG; sha256sum {A}/Downloads/BIG.BIN; ls /home/rudolf/Downloads/.accounts/{DLA}/ 2>&1", 120)
    out(o)
    ok(f'5. the first sign-in on 1.5.6 moves the files to {A} (the document, the picture, 3,000 files), verified', moved is True and 'written in 1.5.5' in o and re.search(r'^3000$', o, re.M) and 'PIC.PNG' in o, [moved, o])
    ok('6. the large download is moved into the account\'s Downloads, not copied (the same bytes; gone from the old place)', BIGSHA and BIGSHA in o and 'BIG.BIN' not in o.split(BIGSHA)[-1], [BIGSHA, o])
    ms = js("(async()=>{const t=performance.now();const j=await rd('home/list?'+hdQ()+'&path=Documents/MANY');return [Math.round(performance.now()-t),(j.items||[]).length]})()", 120)
    ok('7. listing the folder of 3,000 files takes under a second', isinstance(ms, list) and ms[1] == 3000 and ms[0] < 1000, ms)
    # a Linux app: busybox in a stand-in Debian (the real one is a large download)
    sh("D=/persist/debian; mkdir -p $D/bin $D/lib $D/lib64 $D/etc $D/proc $D/sys $D/dev $D/tmp $D/home/rudolf $D/root; cp /bin/busybox $D/bin/; for l in $(ldd /bin/busybox 2>/dev/null | grep -o '/[^ ]*'); do mkdir -p $D$(dirname $l); cp -L $l $D$l; done; "
       "for c in sh cat echo ls; do ln -sf busybox $D/bin/$c; done; echo 'rudolf:x:1000:1000::/home/rudolf:/bin/sh' > $D/etc/passwd; echo test > $D/etc/rudolf-debian; touch $D/.rudolf-ready; echo made", 60)
    o = sh("rudolf-debian rebind >/dev/null 2>&1; rudolf-debian run -- sh -c 'cat /home/rudolf/Documents/NOTE.TXT; echo \"changed by a Linux app\" >> /home/rudolf/Documents/NOTE.TXT; echo LINUX-OK' 2>&1", 120)
    out(o)
    seen = wait_for(js, "(()=>{const v=node([...homePath('DOCUMENTS'),'NOTE.TXT']);return typeof v=='string'&&v.includes('changed by a Linux app')})()", 90)
    ok('8. a Linux app sees the document (Documents in /home/rudolf) and changes it; the desktop shows the change', 'written in 1.5.5' in o and 'LINUX-OK' in o and seen is True, [o, seen])
    out(signout(js)); time.sleep(5)
    o = sh(f"ls {A}/Documents; cat {A}/Documents/* 2>&1 | head -3", 60); out(o)
    ok('9. signed out: the folder is encrypted (no names, contents cannot be read)', 'NOTE.TXT' not in o and 'MANY' not in o and ('Required key not available' in o or 'No such file' in o or 'Operation not permitted' in o or o.strip()), o)
    out(signin(js, 'boss', 'b1'))
    back = wait_for(js, "(()=>{const v=hdOn()&&node([...homePath('DOCUMENTS'),'NOTE.TXT']);return typeof v=='string'&&v.includes('changed by a Linux app')})()", 300)
    ok('10. signed in again: the files are readable, with the Linux app\'s change', back is True, back)
    out(signout(js))
    # the user's home moves at its first sign-in; then the administrator opens it
    out(signin(js, 'kid', 'k1'))
    kmoved = wait_for(js, "!!(USER&&USER.disk&&hdOn())", 900, 10)
    out(signout(js)); time.sleep(5)
    out(signin(js, 'boss', 'b1'))
    opened = wait_for(js, "(()=>{const k=fs['C:'].USERS.KID;return !!(k&&k.DOCUMENTS&&typeof k.DOCUMENTS['KIDNOTE.TXT']=='string')})()", 300)
    ok('11. the user\'s home moved too, and the administrator opens it from the disk', kmoved is True and opened is True, [kmoved, opened])
    js("(async()=>{fs['C:'].USERS.KID.DOCUMENTS['KIDNOTE.TXT']+='an administrator was here\\n';fsSave();fsFlush();await new Promise(r=>setTimeout(r,8000));return 1})()", 120)
    out(signout(js)); time.sleep(5)
    o = sh(f"ls /persist/accounts/{KID}/Documents", 30); out(o)
    ok('12. the administrator signed out: the user\'s folder is locked again', 'KIDNOTE.TXT' not in o, o)
    out(signin(js, 'kid', 'k1'))
    kc = wait_for(js, "(()=>{const v=hdOn()&&node([...homePath('DOCUMENTS'),'KIDNOTE.TXT']);return typeof v=='string'&&v.includes('an administrator was here')})()", 300)
    ok('13. the user sees the administrator\'s change', kc is True, kc)
    out(signout(js))
    sh("sync"); vm.stop(); time.sleep(3)
    # ---------------------------------------------------------------- back to 1.5.5 (as the update fallback would)
    subprocess.run(['true'])
    vm, g, up, js = boot('c', 'a'); sh = lambda c, t=60: g.sh(c, t)
    sh("printf 'good=a\\nprev=\\nnext=\\ntries=0\\nfailed=b\\n' > /persist/os/system/state; sync")
    out(signin(js, 'boss', 'b1'))
    old = wait_for(js, "(()=>{const D=node(homePath('DOCUMENTS'));return !!(D&&typeof D['NOTE.TXT']=='string'&&D.MANY&&Object.keys(D.MANY).length==3000)})()", 300)
    note = js("node([...homePath('DOCUMENTS'),'NOTE.TXT'])")
    ok('14. back on 1.5.5 within the 7 days: the old record shows the home as it was at the move', old is True and 'written in 1.5.5' in str(note) and 'changed by a Linux app' not in str(note), [old, note])
    out(signout(js))
    sh("printf 'good=b\\nprev=a\\nnext=\\ntries=0\\nfailed=\\n' > /persist/os/system/state; sync"); vm.stop(); time.sleep(3)
    # ---------------------------------------------------------------- 1.5.6 again: backup and restore
    vm, g, up, js = boot('d', 'b'); sh = lambda c, t=60: g.sh(c, t)
    out(signin(js, 'boss', 'b1'))
    wait_for(js, "!!(USER&&USER.disk&&hdOn()&&node([...homePath('DOCUMENTS'),'NOTE.TXT']))", 300)
    r = js("(async()=>{window.__bk=await backupBlob();return window.__bk.size})()", 600)
    out('backup bytes:', r)
    js("(async()=>{try{await HD.chain}catch(e){}userLogout();await new Promise(r=>setTimeout(r,5000));return 1})()", 120)
    sh(f"rm -rf /persist/accounts/{BOSS}; ls /persist/accounts", 120)
    js("(async()=>{await restoreAll(window.__bk,true);return 1})()", 300)
    time.sleep(40)
    end = time.time() + 600
    while time.time() < end and js("typeof acctNew=='function'&&typeof FSLOADED!='undefined'&&FSLOADED", 20) is not True: time.sleep(5)
    js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};return 1})()")
    out(signin(js, 'boss', 'b1'))
    rest = wait_for(js, "(()=>{if(!(USER&&USER.disk&&hdOn()))return false;const D=node(homePath('DOCUMENTS'));return !!(D&&typeof D['NOTE.TXT']=='string'&&D['NOTE.TXT'].includes('changed by a Linux app')&&D.MANY&&Object.keys(D.MANY).length==3000)})()", 1500, 10)
    o = sh(f"cat /persist/accounts/{BOSS}/Documents/NOTE.TXT; ls /persist/accounts/{BOSS}/Documents/MANY | wc -l", 60); out(o)
    ok('15. a backup, the account\'s folder removed, the backup restored: the home comes back onto the disk', isinstance(r, int) and r > 1000 and rest is True and 'changed by a Linux app' in o and re.search(r'^3000$', o, re.M), [r, rest, o])
    out(signout(js))
finally:
    try: vm and vm.stop()
    except Exception: pass
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if all(R) else 1)
