# Rudolf OS 1.6.0 in QEMU (docs/plan-1.6.0.md Part 15): the security checks moved from 1.5.9, on a release's own kernel
# with its system in slot a (as t150), UEFI with Secure Boot off.
#   15.3 (third step): two accounts; the move of their folders on an existing data partition (owned by the desktop's
#        user, as before 1.5.9) to their own Linux users at the next start; one account's programs (as Flatpak apps,
#        Debian's and Wine's run: its user; and a program in a sandbox, bwrap as Flatpak starts one; and Debian, a
#        stand-in with busybox) refused the other's files; the display refused to the other account's user; signing out
#        stops the account's programs.
#   15.1: those programs refused the desktop's page (no token in it) and its API (even with the right token).
#   8.7 (without Secure Boot): Developer mode on with a throwaway key, a release signed by it started from slot b (the
#        system says "custom"), Return to official (keep) back to the official release with the custom slot cleared;
#        the official recovery system made from the official one, checked and made again when changed (the rudolf-update
#        of this tree, put into the guest: the release's own may be older).
#   python3 t160_t.py REL-DIR [RUDOLF-UPDATE]   (SB_PARTS and QEMU as for t150; root; QEMU_ACCEL=kvm where it can)
import os, sys, time, re, subprocess, shutil, struct, json, base64, urllib.request
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE); ISO = os.path.dirname(QT)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); RU = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else ''
PARTS = os.environ.get('SB_PARTS', '/nonexistent'); LOG = open('t160.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:500] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 3000; DRV = os.path.abspath('int160.img'); MEM = int(os.environ.get('MEM', '3072')); W = os.path.abspath('w160')

# a throwaway developer key and a release signed by it (the same layers, another serial): slot b
shutil.rmtree(W, ignore_errors=True); os.makedirs(f'{W}/l')
subprocess.run([sys.executable, '-c', '''
import sys,base64
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization as s
k=Ed25519PrivateKey.generate(); t=sys.argv[1]
open(t+"/dev.key","w").write(base64.b64encode(k.private_bytes(s.Encoding.Raw,s.PrivateFormat.Raw,s.NoEncryption())).decode())
open(t+"/dev.pub","w").write(base64.b64encode(k.public_key().public_bytes(s.Encoding.Raw,s.PublicFormat.Raw)).decode())
''', W], check=True)
for n in ('base', 'chromium', 'desktop', 'modules'): os.link(f'{REL}/{n}.sfs', f'{W}/l/{n}.sfs')
os.link(f'{REL}/vmlinuz', f'{W}/l/vmlinuz')
DSER = int(mv('serial')) + 1
subprocess.run([sys.executable, f'{ISO}/systools/make_release.py', '--layers', f'{W}/l', '--kernel', f'{W}/l/vmlinuz', '--out', f'{W}/out',
                '--key', f'{W}/dev.key', '--serial', str(DSER), '--version', mv('version') + '-dev', '--chromium', mv('chromium'), '--kernel-version', mv('kernel')], check=True, stdout=subprocess.DEVNULL)
DEVPUB = open(f'{W}/dev.pub').read().strip()

def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    for s, src, rl in (('a', REL, f"serial {mv('serial')}\nversion {mv('version')}\n"), ('b', f'{W}/out/channel', f"serial {DSER}\nversion {mv('version')}-dev\n")):
        d = f'{T}/os/system/{s}'; os.makedirs(d)
        for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs'): shutil.copy(f'{REL}/{n}', f'{d}/{n}')
        for n in ('manifest.txt', 'manifest.sig'): shutil.copy(f'{src}/{n}', f'{d}/{n}')
        open(f'{d}/RELEASE', 'w').write(rl + f"released {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n"); open(f'{d}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)

def boot(tag, slot):
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf160', '-serial', f'unix:{TUN},server,nowait',
             '-kernel', f'{(W + "/out/channel") if False else REL}/vmlinuz', '-append', f'quiet loglevel=3 consoleblank=0 rudolf.slot={slot} rudolf.debug=1']
    global g_last
    vm = sbvm.VM('t160' + tag, PARTS, [], secure=False, mem=MEM, extra=extra)
    g = Guest.__new__(Guest); g_last = g; g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    time.sleep(30); end = time.time() + 900; up = False
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
    while up and time.time() < end and js("typeof acctNew=='function'&&typeof userLogin=='function'", 20) is not True: time.sleep(5)
    js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};if(!window.__cww){window.__cww=1;window.__cw=[];const w=console.warn,er=console.error;console.warn=function(...a){__cw.push('W '+a.map(x=>x&&x.message?x.message:String(x)).join(' ').slice(0,300));return w.apply(this,a)};console.error=function(...a){__cw.push('E '+a.map(x=>x&&x.message?x.message:String(x)).join(' ').slice(0,300));return er.apply(this,a)};addEventListener('error',e=>__cw.push('X '+e.message));addEventListener('unhandledrejection',e=>__cw.push('R '+(e.reason&&e.reason.message||e.reason)))}return 1})()")
    return vm, g, up, js
# a command run in the guest from a script file (a long command line echoed back over the serial line mixed with its
# output), its output written to this test's log in full
def run(g, c, t=60):
    f = os.path.join(W, 'c160.sh'); open(f, 'w').write(c + '\n'); g.put(f, '/tmp/c160.sh', '644')
    o = g.sh('sh /tmp/c160.sh 2>&1', t); out('$', c[:200].replace('\n', ' ; ')); out(o.rstrip()[-3000:]); return o
def signin(js, n, pw, secs=900):
    r = js(f"(()=>{{if(window.__si=='wait')return 'already';const a=ACCTS.find(x=>x.n=={json.dumps(n)});if(!a)return 'no account';window.__si='wait';"
           f"(async()=>{{try{{if(!(await pwCheck(a,{json.dumps(pw)}))){{__si='bad';return}}if(USER)userLogout();const L=document.getElementById('login');USER=a;"
           f"await userLogin(a,{json.dumps(pw)});lastInput=Date.now();if(L)L.style.display='none';__si='in '+USER.n}}catch(e){{__si='error '+e.message}}}})();return 'started'}})()", 60)
    if r not in ('started', 'already'): return r
    end = time.time() + secs
    while time.time() < end:
        s = js("window.__si", 30)
        if isinstance(s, str) and s != 'wait': return s
        time.sleep(5)
    out('page:', js("JSON.stringify({si:window.__si,user:USER&&USER.n,accts:ACCTS.map(a=>a.n),err:(window.__cw||[]).slice(-5)})", 30))
    out(g_last.sh("tail -25 /run/rudolf/rudolfd.log; tail -15 /run/rudolf/page.log; tail -15 /run/rudolf/session.log; ps w | grep -v ' \\[' | tail -40", 60))
    return 'still signing in'
def wait_for(js, expr, secs, every=5):
    end = time.time() + secs; r = None
    while time.time() < end:
        r = js(expr, 60)
        if r is True: return True
        time.sleep(every)
    return r
def signout(js):
    return js("(async()=>{try{fsFlush()}catch(e){}try{await homeChain}catch(e){}if(USER)userLogout();await new Promise(r=>setTimeout(r,4000));return 'out'})()", 180)

# 15.4 and 1.5.8 4.2: the development link, with a stand-in for GitHub here (../ghstub.py); the test machine has no
# network, so the link's connections go over its second serial port (../sertun.c in the guest, ../sertun.py here)
QT = os.path.dirname(HERE); PORT = 18767; DLTOKEN = 'github_pat_T160test_0123456789abcdefSECRET'; REPO = 'owner/rudolfos-devlink'
GH = f'http://127.0.0.1:{PORT}'; TUN = f'/tmp/tun-t160-{os.getpid()}.sock'
subprocess.run(['gcc', '-O2', '-static', '-o', f'{W}/sertun', os.path.join(QT, 'sertun.c')], check=True)
stub = subprocess.Popen([sys.executable, os.path.join(QT, 'ghstub.py'), str(PORT), DLTOKEN, REPO])
tun = subprocess.Popen([sys.executable, os.path.join(QT, 'sertun.py'), TUN, '127.0.0.1', str(PORT)])
def ctl(path, data=None):
    r = urllib.request.urlopen(urllib.request.Request(GH + path, data=data, method='POST' if data is not None else 'GET'), timeout=20)
    return json.loads(r.read() or b'{}')
def dlfiles(): return {k: base64.b64decode(v) for k, v in ctl('/_files').items()}
def dlcmd(i, **kw): ctl(f'/_put?path=cmd/{i:06d}.json', json.dumps(kw).encode())
def dlresult(i, t=240):
    end = time.time() + t
    while time.time() < end:
        f = dlfiles().get(f'res/{i:06d}.json')
        if f: return json.loads(f)
        time.sleep(3)
    return None
def tunnel(sh): sh('pgrep -f "sertun /dev/ttyS1" >/dev/null || (setsid /persist/t160/sertun /dev/ttyS1 18765 </dev/null >/dev/null 2>&1 &); echo started', 20)
def dlpost(js, what, body):
    return js(f"rd({json.dumps(what)},{{method:'POST',body:{json.dumps(json.dumps(body))}}}).then(r=>r.message||'ok',e=>'ERR '+e.message)", 120)
make_drive(DRV)
vm = None
try:
    vm, g, up, js = boot('a', 'a'); sh = lambda c, t=60: run(g, c, t)
    ok('1. the release starts (slot a)', up, 'not up')
    if not up: raise SystemExit
    r = js("(async()=>{ACCTS.length=0;const b=await acctNew('boss','b1','admin');ACCTS.push(b);const k=await acctNew('kid','k1','user');ACCTS.push(k);saveUser();return b.id+' '+k.id})()", 120)
    out('accounts:', r); BOSS, KID = str(r).split()
    for n, pw in (('boss', 'b1'), ('kid', 'k1')):
        out('sign-in', n, signin(js, n, pw)); out('on the disk:', wait_for(js, "!!(USER&&USER.disk&&hdOn())", 600, 10))
        out(signout(js))
    time.sleep(5)
    users = sh("cat /persist/etc/rudolf/account-users; ls -ln /persist/accounts")
    out(users)
    # each account's test file, put in its folder here (the desktop's own file saving is not what these checks are
    # about: the first run's file written through the page never reached the disk before the sign-out)
    out(sh(f"for a in {BOSS}:boss {KID}:kid; do d=/persist/accounts/${{a%%:*}}/Documents; mkdir -p $d; echo \"${{a#*:}} secret\" > $d/SECRET.TXT; done; ls -l /persist/accounts/*/Documents/SECRET.TXT"))
    # as before 1.5.9: the folders the desktop's user's, and not yet moved
    sh(f"chown -R 1000:1000 /persist/accounts/{BOSS} /persist/accounts/{KID}; sed -i 's/ moved$//' /persist/etc/rudolf/account-users; sync")
    vm.stop(); time.sleep(3)
    vm, g, up, js = boot('a2', 'a'); sh = lambda c, t=60: run(g, c, t)
    ok('2. started again', up)
    if not up: raise SystemExit
    o = sh(f"cat /persist/etc/rudolf/account-users; stat -c '%u %a %n' /persist/accounts/{BOSS} /persist/accounts/{BOSS}/Documents/SECRET.TXT /persist/accounts/{KID}; grep -h 'own Linux user' /persist/var/log/health*.log /run/rudolf/health*.log 2>/dev/null | tail -3")
    ub = re.search(rf'^{BOSS} (\d+)', o, re.M); uk = re.search(rf'^{KID} (\d+)', o, re.M); UB, UK = (ub.group(1) if ub else '?'), (uk.group(1) if uk else '?')
    ok('3. 15.3: the move on an existing data partition: each account\'s folder and files belong to its own Linux user now, 0700, logged', ub and uk and f'{UB} 700 /persist/accounts/{BOSS}' in o and f'{UK} 700 /persist/accounts/{KID}' in o and re.search(rf'^{UB} \d+ /persist/accounts/{BOSS}/Documents/SECRET.TXT', o, re.M) and 'own Linux user' in o, o)
    out('sign-in boss', signin(js, 'boss', 'b1')); out('on the disk:', wait_for(js, "!!(USER&&USER.disk&&hdOn())", 600, 10)); time.sleep(5)
    TOK = js("RUDOLFD.token"); out('token length', len(str(TOK)))
    o = sh(f"cat /run/rudolf/acct-session.env; ls -ld /home/acct{UB}; id acct{UB}")
    ok('4. signed in: the account\'s session, its home its own folder', f'HOME=/home/acct{UB}' in o, o)
    def AS(uid, cmds, t=120):
        return sh(f"cat > /tmp/u160.sh <<'EOF'\n{cmds}\nEOF\nchmod 644 /tmp/u160.sh; su -s /bin/sh acct{uid} -c 'sh /tmp/u160.sh' 2>&1; echo rc=$?", t)
    o = AS(UB, f"id -u; cat /persist/accounts/{KID}/Documents/SECRET.TXT 2>&1; ls /persist/accounts/{KID} 2>&1; cat /home/acct{UB}/Documents/SECRET.TXT 2>&1")
    ok('5. 15.3: the account\'s programs (its user) cannot read the other account\'s files, and can read their own', re.search(rf'^{UB}$', o, re.M) and 'kid secret' not in o and o.count('Permission denied') >= 2 and 'boss secret' in o, o)
    o = AS(UB, f"id -u; busybox wget -q -O- http://127.0.0.1:8080/ 2>&1 | head -c 300; echo; busybox wget -S -O- --header 'X-Rudolf-Token: {TOK}' http://127.0.0.1:8080/api/info 2>&1 | head -c 300; echo")
    ok('6. 15.1: the account\'s programs get neither the page (with its token) nor the API, even with the right token', re.search(rf'^{UB}$', o, re.M) and 'RUDOLFD' not in o and '"ok":true' not in o and ('403' in o or 'refused' in o.lower() or 'Forbidden' in o), o)
    o = AS(UB, f"command -v bwrap; bwrap --ro-bind / / --dev /dev --proc /proc --tmpfs /tmp --unshare-pid --die-with-parent sh -c 'echo IN-SANDBOX; cat /persist/accounts/{KID}/Documents/SECRET.TXT; busybox wget -q -O- --header \"X-Rudolf-Token: {TOK}\" http://127.0.0.1:8080/api/info; echo' 2>&1 | head -c 400")
    ok('7. ...a program in a sandbox (bwrap, as Flatpak starts one) the same', 'IN-SANDBOX' in o and 'kid secret' not in o and '"ok":true' not in o, o)
    # Debian: a stand-in (busybox), as t156; it runs as the account's user
    sh("D=/persist/debian; mkdir -p $D/bin $D/lib $D/lib64 $D/etc $D/proc $D/sys $D/dev $D/tmp $D/run $D/home/rudolf $D/root $D/.oldroot; cp /bin/busybox $D/bin/; for l in $(ldd /bin/busybox 2>/dev/null | grep -o '/[^ ]*'); do mkdir -p $D$(dirname $l); cp -L $l $D$l; done; "
       "for c in sh cat echo ls wget id; do ln -sf busybox $D/bin/$c; done; echo 'rudolf:x:1000:1000::/home/rudolf:/bin/sh' > $D/etc/passwd; echo test > $D/etc/rudolf-debian; touch $D/.rudolf-ready; chown -R 200000:200000 $D", 120)
    o = sh(f"rudolf-debian rebind >/dev/null 2>&1; rudolf-debian run -- sh -c 'id -u; cat /home/rudolf/Documents/SECRET.TXT; wget -q -O- --header \"X-Rudolf-Token: {TOK}\" http://127.0.0.1:8080/api/info; echo; ls /home/rudolf/../ 2>&1' 2>&1", 180)
    ok('8. Debian (Wine runs in it): its programs are the account\'s, see its folder only, and are refused the API', 'boss secret' in o and 'kid secret' not in o and '"ok":true' not in o, o)
    o = AS(UB, "DISPLAY=:0 rudolf-display --xsize 2>&1") + AS(UK, "DISPLAY=:0 rudolf-display --xsize 2>&1; echo rc=$?")
    rcs = re.findall(r'^rc=(\d+)', o, re.M)
    ok('9. 15.3: the display: the signed-in account\'s programs may use it, the other account\'s user is refused', re.search(r'^\d+ \d+$', o, re.M) and len(rcs) == 3 and rcs[1] != '0', o)
    mine = f'for p in /proc/[0-9]*; do [ "$(stat -c %u $p 2>/dev/null)" = {UB} ] && grep -qx sleep $p/comm 2>/dev/null && echo ${{p#/proc/}}; done'
    sh(f"(su -s /bin/sh acct{UB} -c 'exec sleep 600' &) ; sleep 2"); o1 = sh(mine)
    out(signout(js)); time.sleep(6); o2 = sh(mine + f"; ls /home/acct{UB} 2>&1 | head -2")
    ok('10. 15.3: signing out stops every program of the account\'s user, its home goes', re.search(r'^\d+$', o1, re.M) and not re.search(r'^\d+$', o2, re.M), [o1, o2])
    # 8.7 (without Secure Boot) and the recovery repair, with this tree's rudolf-update
    U = 'rudolf-update'
    if RU:
        g.put(RU, '/tmp/ru160'); U = '/tmp/ru160'
    o = sh(f"{U} boot-ok; cat /persist/os/system/r/official 2>&1; {U} status", 300)
    ok('11. the official recovery system made from the official release that started', 'official recovery system: release' in o and '"signer":"official"' in o, o)
    o = sh(f"rm /persist/os/system/r/desktop.sfs; head -c 1000 /dev/urandom > /persist/os/system/r/desktop.sfs; {U} recovery-check; echo rc=$?; cmp /persist/os/system/r/desktop.sfs /persist/os/system/a/desktop.sfs && echo SAME", 300)
    ok('12. a recovery system file changed: found at the check, made again from the official system', 'made again from this official system' in o and 'SAME' in o, o)
    o = sh(f"RU_NOAUTOFETCH=1 {U} developer on '{DEVPUB}' https://dev.example/rudolf; printf 'good=a\\nprev=\\nnext=b\\ntries=2\\nfailed=\\n' > /persist/os/system/state; sync", 60)
    ok('13. 8.7: Developer mode on with a developer\'s key', 'Developer mode is on' in o, o)
    vm.stop(); time.sleep(3)
    vm, g, up, js = boot('b', 'b'); sh = lambda c, t=60: run(g, c, t)
    if RU: g.put(RU, '/tmp/ru160')
    o = sh(f"cat /etc/rudolf/slot; {U} boot-ok; {U} status", 300) if up else 'not up'
    ok('14. ...a release signed by that key starts from slot b, the system says it is a custom build, the recovery system stays official', up and '"signer":"custom"' in o and '"recoveryOfficial":true' in o, o)
    o = sh(f"RU_NOESP=1 {U} return-official keep 2>&1; cat /persist/os/system/state", 600) if up else ''
    ok('15. Return to official (keep): the official recovery system goes into the spare slot, to start next', 'Returning to the official Rudolf OS' in o and 'ret=1' in o, o)
    nxt = (re.search(r'^next=(\w)', o, re.M) or [None, 'a'])[1]
    vm.stop(); time.sleep(3)
    vm, g, up, js = boot('c', nxt); sh = lambda c, t=60: run(g, c, t)
    if RU and up: g.put(RU, '/tmp/ru160')
    o = sh(f"{U} boot-ok; {U} status; echo; ls -1 /persist/os/system | cat; cat /persist/etc/rudolf/extra-keys 2>&1", 300) if up else 'not up'
    ok('16. ...the official release started again: official, Developer mode off, the custom build\'s slot cleared', up and '"signer":"official"' in o and '"developer":false' in o and not re.search(r'^b$', o, re.M) and '"b":{"complete":false' in o, o)
    # 1.5.9 15.4: switching the development link on: only where it is allowed, and only with an administrator's name
    # and password (t140 switched it on with the token alone, as before 1.5.9)
    if up:
        g.put(f'{W}/sertun', '/tmp/sertun', '755')
        sh("mkdir -p /persist/t160 && cp /tmp/sertun /persist/t160/sertun; mkdir -p -m 700 /persist/devlink; echo http://127.0.0.1:18765 > /persist/devlink/test-api; rm -f /persist/devlink/allowed", 30)
        tunnel(sh)
        q = f"devlink/on?repo={REPO.replace('/', '%2F')}&hours=1&ask=0&restart=1&home=0"
        r0 = dlpost(js, q, {'n': 'boss', 'p': 'b1', 'token': DLTOKEN})
        r1 = dlpost(js, 'devlink/allow?on=1', {'n': 'kid', 'p': 'k1'})
        r2 = dlpost(js, 'devlink/allow?on=1', {'n': 'boss', 'p': 'b1'})
        r3 = dlpost(js, q, {'n': 'kid', 'p': 'k1', 'token': DLTOKEN})
        r4 = dlpost(js, q, {'token': DLTOKEN})
        out('devlink:', [r0, r1, r2, r3, r4])
        ok('17. 15.4: the link is refused until allowed, allowing it and switching it on need an administrator (a user\'s password and none refused)',
           str(r0).startswith('ERR') and 'not allowed' in str(r0) and str(r1).startswith('ERR') and 'administrator' in str(r1) and not str(r2).startswith('ERR')
           and str(r3).startswith('ERR') and 'administrator' in str(r3) and str(r4).startswith('ERR'), [r0, r1, r2, r3, r4])
        r5 = dlpost(js, q, {'n': 'boss', 'p': 'b1', 'token': DLTOKEN})
        dlcmd(1, kind='sh', script='id -u; echo linked'); r = dlresult(1)
        ok('18. ...with the administrator\'s password it is on, and a command from the repository runs (as root)', r5 == 'on' and r and r.get('stdout') == '0\nlinked\n', [r5, r])
        # 1.5.8 4.2: on again by itself after a restart, and answering with the desktop not running
        vm.stop(); time.sleep(3)
        vm, g, up, js = boot('d', nxt); sh = lambda c, t=60: run(g, c, t)
        if up:
            tunnel(sh)
            sh("pkill -f /usr/lib/rudolf/session; sleep 1; pkill -9 -f '[-]-rudolf-kiosk'; pkill -9 -f rudolf-native; sleep 2; rm -f /run/rudolf/heartbeat; pgrep -f 'rudolf-devlink run' >/dev/null && echo LINK-RUNS", 60)
            dlcmd(2, kind='sh', script='echo no-desktop'); r = dlresult(2, 400)
            ok('19. 4.2: after a restart the link is on by itself, and answers with the desktop not running (Ask first off)', r and r.get('stdout') == 'no-desktop\n', r)
            sh("sed -i 's/^ask=.*/ask=1/' /persist/devlink/config; grep -q '^ask=' /persist/devlink/config || echo ask=1 >> /persist/devlink/config; rm -f /tmp/ran160; sleep 65", 120)
            dlcmd(3, kind='sh', script='touch /tmp/ran160; echo ran'); r = dlresult(3, 300)
            o = sh("ls /tmp/ran160 2>&1", 20)
            ok('20. ...with Ask first on and no desktop to ask: answered at once ("no desktop to ask"), not run', r and r.get('status') == -1 and r.get('approved') == 'no desktop to ask' and 'No such file' in o, [r, o])
        else: ok('19. 4.2: started again', False, 'not up')
finally:
    try: stub.kill(); tun.kill()
    except Exception: pass
    try: vm and vm.stop()
    except Exception: pass
    shutil.rmtree(W, ignore_errors=True)
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
