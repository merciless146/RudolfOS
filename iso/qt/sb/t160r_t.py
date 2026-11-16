# Rudolf OS 1.6.0 in QEMU (the owner, 2026-11-16): Roll back to the last stable version on the real releases. A computer
# that got the 1.6.0 beta as an update over the stable 1.5.9 (slot a: 1.5.9, slot b: the beta, which runs), with an
# account and its files; this tree's rudolf-update (stable-info, to-stable) put into the guest over the release's own
# (bind-mounted, so the page's rudolfd uses it too). The official stable channel is served from here through the
# serial tunnel (../sertun.c), with the real signed lists (the release key's public half is all the guest needs):
#   1. the beta is not a published stable release (the real stable list, 1.5.9); a history signed by another key (a
#      throwaway TEST key made here) is not believed;
#   2. stable-info: 1.5.9 (released without a data record: known by its version to read layouts 1 and 2) can read this
#      computer's data; to-stable: it is in slot a, so it starts next, nothing downloaded, beta versions off;
#   3. 1.5.9 starts (the start-up's choice for the next start, as the initramfs makes it from the state) and opens the
#      account made on the beta: its folder, its own Linux user, its files (the decision for 1.6.0 -> 1.5.9, tested);
#   4. back up: Get beta versions on again brings the beta back from what is on the computer (no download), it starts,
#      and the account's files (the beta's and the one written on 1.5.9) are there;
#   5. the refusal: with the real signed list of the stable 1.5.8 (one Linux user for every account) as the stable
#      channel, to-stable refuses, saying why, and changes nothing.
#   python3 t160r_t.py BETA-DIR STABLE-DIR STABLE158-DIR RUDOLF-UPDATE   (SB_PARTS and QEMU as for t160; root; QEMU_ACCEL=kvm
#   where it can). Each DIR: vmlinuz, the four layers, manifest.txt and manifest.sig (STABLE158-DIR: the lists only).
import os, sys, time, re, subprocess, shutil, struct, json, base64, http.server, threading, functools
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE); ISO = os.path.dirname(QT)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
BETA, STABLE, OLD, RU = (os.path.abspath(a) for a in sys.argv[1:5])
PARTS = os.environ.get('SB_PARTS', '/nonexistent'); LOG = open('t160r.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:1500] if d != '' and not c else ''))
def man(d):
    t = open(f'{d}/manifest.txt').read(); return lambda k: re.search(rf'^{k} (.*)$', t, re.M).group(1)
mb, ms = man(BETA), man(STABLE)
out('beta', mb('serial'), mb('version'), '/ stable', ms('serial'), ms('version'), '/ old stable', man(OLD)('serial'), man(OLD)('version'))
DMB = 3000; DRV = os.path.abspath('int160r.img'); MEM = int(os.environ.get('MEM', '3072')); W = os.path.abspath('w160r')
shutil.rmtree(W, ignore_errors=True); os.makedirs(W)

# the official channels, served here: stable (1.5.9's real signed list), beta (the beta's), and a history of stable
# releases signed by a throwaway TEST key that also names the beta (it must not be believed)
SRV = f'{W}/srv/RudolfOS'
def channel(name, d):
    os.makedirs(f'{SRV}/{name}', exist_ok=True)
    for n in ('manifest.txt', 'manifest.sig'): shutil.copy(f'{d}/{n}', f'{SRV}/{name}/{n}')
channel('stable', STABLE); channel('beta', BETA)
subprocess.run([sys.executable, '-c', '''
import sys,base64
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
k=Ed25519PrivateKey.generate(); t=sys.argv[1]; s=sys.argv[2]
h=("rudolf-stable-history 1\\nS %s 1.5.9 2026-11-15\\nS %s 1.6.0 2026-11-16\\n" % (sys.argv[3], sys.argv[4])).encode()
open(s+"/stable/history.txt","wb").write(h); open(s+"/stable/history.sig","wb").write(k.sign(h))
''', W, SRV, ms('serial'), mb('serial')], check=True)
class Quiet(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *a): out('http:', self.requestline, a[1] if len(a) > 1 else '')
PORT = 18768; httpd = http.server.ThreadingHTTPServer(('127.0.0.1', PORT), functools.partial(Quiet, directory=f'{W}/srv'))
threading.Thread(target=httpd.serve_forever, daemon=True).start()
TUN = f'/tmp/tun-t160r-{os.getpid()}.sock'; GPORT = 18765; OFF = f'http://127.0.0.1:{GPORT}/RudolfOS/stable'
subprocess.run(['gcc', '-O2', '-static', '-o', f'{W}/sertun', os.path.join(QT, 'sertun.c')], check=True)
TUNLOG = os.path.abspath('sertun160r.log'); tun = subprocess.Popen([sys.executable, os.path.join(QT, 'sertun.py'), TUN, '127.0.0.1', str(PORT)], env=dict(os.environ, SERTUN_LOG=TUNLOG))

def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    for s, src in (('a', STABLE), ('b', BETA)):
        d = f'{T}/os/system/{s}'; os.makedirs(d); mv = man(src)
        for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs', 'manifest.txt', 'manifest.sig'): shutil.copy(f'{src}/{n}', f'{d}/{n}')
        open(f'{d}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes {mv('notes')}\n"); open(f'{d}/complete', 'w').write('1\n')
    # (the beta came as an update over 1.5.9 and was kept: good b, prev a; Get beta versions on)
    open(f'{T}/os/system/state', 'w').write('good=b\nprev=a\nnext=\ntries=0\nfailed=\n')
    open(f'{T}/os/update.conf', 'w').write('url=https://merciless146.github.io/RudolfOS/beta\nauto=0\n')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)

INIT_JS = "(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};if(!window.__cww){window.__cww=1;window.__cw=[];const w=console.warn,er=console.error;console.warn=function(...a){__cw.push('W '+a.map(x=>x&&x.message?x.message:String(x)).join(' ').slice(0,300));return w.apply(this,a)};console.error=function(...a){__cw.push('E '+a.map(x=>x&&x.message?x.message:String(x)).join(' ').slice(0,300));return er.apply(this,a)};addEventListener('error',e=>__cw.push('X '+e.message));addEventListener('unhandledrejection',e=>__cw.push('R '+(e.reason&&e.reason.message||e.reason)))}return 1})()"
SLOTREL = {'a': STABLE, 'b': BETA}
def boot(tag, slot, trying=False):
    # the start-up's choice, as the initramfs makes it from the state (good, or next with tries left) and hands over to that
    # slot's own kernel with rudolf.slot= (here started directly: the initramfs needs a boot drive to start from)
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf160r', '-serial', f'unix:{TUN},server,nowait',
             '-kernel', f'{SLOTREL[slot]}/vmlinuz', '-append', f'quiet loglevel=3 consoleblank=0 rudolf.slot={slot} rudolf.debug=1' + (' rudolf.trying=1' if trying else '')]
    global g_last
    vm = sbvm.VM('t160r' + tag, PARTS, [], secure=False, mem=MEM, extra=extra)
    g = Guest.__new__(Guest); g_last = g; g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    time.sleep(30); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    def js(e, t=90):
        if len(base64.b64encode(e.encode())) > 900: raise ValueError('a page expression too long for the serial shell: ' + e[:80])
        for _ in range(5):
            r = g.js(e, t)
            if not (isinstance(r, tuple) and (r[0] == 'ERR' or (r[0] == 'EXC' and 'ReferenceError' in str(r[1])))): return r
            time.sleep(15)
        return r
    end = time.time() + 600
    while up and time.time() < end and js("typeof acctNew=='function'&&typeof userLogin=='function'", 20) is not True: time.sleep(5)
    js(INIT_JS)
    if up:
        o = run(g, "last=; same=0; i=0; while [ $i -lt 40 ]; do i=$((i+1)); v=$(timeout 20 cdp-eval 'Math.round(performance.timeOrigin)' 2>/dev/null | grep -o '\"value\":[0-9]*' | head -1); "
                   "if [ -n \"$v\" ] && [ \"$v\" = \"$last\" ]; then same=$((same+5)); else same=0; last=$v; fi; [ $same -ge 20 ] && break; sleep 5; done; echo \"SETTLED $same $last after $i\"", 1100)
        js(INIT_JS)
    return vm, g, up, js
def run(g, c, t=60):
    f = os.path.join(W, 'c160r.sh'); open(f, 'w').write(c + '\n'); g.put(f, '/tmp/c160r.sh', '644')
    o = g.sh('sh /tmp/c160r.sh 2>&1', t); out('$', c[:300].replace('\n', ' ; ')); out(o.rstrip()[-3000:]); return o
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
    return 'still signing in'
def wait_for(js, expr, secs, every=5):
    end = time.time() + secs; r = None
    while time.time() < end:
        r = js(expr, 60)
        if r is True: return True
        time.sleep(every)
    return r
def on_disk(js, secs=300): return wait_for(js, "!!(USER&&USER.disk&&hdOn())", secs, 5)
def signout(js):
    return js("(async()=>{try{fsFlush()}catch(e){}try{await homeChain}catch(e){}if(USER)userLogout();await new Promise(r=>setTimeout(r,4000));return 'out'})()", 180)
def tunnel(g, sh):
    g.put(f'{W}/sertun', '/tmp/sertun', '755')
    o = sh('pgrep -f "^/tmp/sertun /dev/ttyS1" >/dev/null || (setsid sh -c \'while :; do /tmp/sertun /dev/ttyS1 %d; echo "sertun ended: $?" >&2; sleep 2; done\' '
           '</dev/null >/dev/null 2>>/tmp/sertun.err &); sleep 3; pgrep -f "^/tmp/sertun /dev/ttyS1" >/dev/null && echo TUNNEL-UP' % GPORT, 30)
    return 'TUNNEL-UP' in o
# this tree's rudolf-update over the release's own (the page's rudolfd runs it too), and the official address served here
U = f'RU_OFFICIAL_URL={OFF} rudolf-update'
def put_ru(g, sh):
    g.put(RU, '/tmp/ru160r'); return sh("mount --bind /tmp/ru160r /usr/bin/rudolf-update && rudolf-update stable-info >/dev/null; echo rc=$?", 60)
def state(sh): return dict(re.findall(r'^(good|prev|next|tries|failed)=(.*)$', sh("cat /persist/os/system/state"), re.M))
def J(o):
    m = re.search(r'^\{.*\}$', o, re.M)
    try: return json.loads(m.group(0)) if m else {}
    except ValueError: return {}

make_drive(DRV)
vm = None
try:
    # ---- the beta, with an account and its files
    vm, g, up, js = boot('b1', 'b'); sh = lambda c, t=60: run(g, c, t)
    ok('0. the beta starts (slot b)', up and mb('version') in sh("cat /etc/rudolf/slot; rudolf-update status"), 'not up')
    if not up: raise SystemExit
    r = js("(async()=>{ACCTS.length=0;const b=await acctNew('boss','b1','admin');ACCTS.push(b);saveUser();return b.id})()", 120)
    BOSS = str(r); out('account', BOSS)
    out('sign-in', signin(js, 'boss', 'b1')); od = on_disk(js); out('on the disk:', od)
    # (the file put there from the shell, as the account's own: busybox's chown has no --reference)
    o = sh(f"cat /persist/etc/rudolf/account-users; d=/persist/accounts/{BOSS}/Documents; u=$(stat -c %u /persist/accounts/{BOSS}); mkdir -p $d && echo 'made on the beta' > $d/BETA.TXT && chown $u:$u $d $d/BETA.TXT && sync; "
           f"stat -c '%u %a %n' /persist/accounts/{BOSS} $d/BETA.TXT; cat /run/rudolf/acct-session.env | grep HOME; ls -la /home/acct*/.local/share/keyrings 2>&1 | head -5")
    m = re.search(rf'^{BOSS} (\d+)', o, re.M); UB = m.group(1) if m else '?'
    ok('1. an account on the beta: its own Linux user, its folder and a file of its own', od is True and m and f'{UB} 700 /persist/accounts/{BOSS}' in o and re.search(rf'^{UB} \d+ \S+BETA.TXT', o, re.M), o)
    out(signout(js)); time.sleep(5)
    # ---- the button's facts, with this tree's rudolf-update
    put_ru(g, sh); out('tunnel', tunnel(g, sh))
    sh(f"{U} set url http://127.0.0.1:{GPORT}/RudolfOS/beta; cat /persist/os/update.conf")
    o = sh(f"{U} check; {U} status; ls -l /persist/os/system/ | grep stable; cat /persist/os/system/stable-history.txt 2>&1 | head -3", 180); s = J(o)
    ok('2. on the beta: not a published stable release, Get beta versions on, the last stable one is 1.5.9 (the real signed list); the history signed by another key is not kept',
       s.get('runningStable') is False and s.get('beta') is True and s.get('stableVersion') == ms('version') and 'stable.txt' in o and 'No such file' in o, o)
    o = sh(f"{U} stable-info", 60); i = J(o)
    ok('3. stable-info: 1.5.9 is in slot a, it reads this computer\'s data (layout 2; 1.5.9, released without a record, known to read 1 and 2), and what it does not use is said',
       i.get('how') == 'slot' and i.get('slot') == 'a' and i.get('compatible') is True and i.get('dataFmt') == 2 and (i.get('stable') or {}).get('reads') == [1, 2] and len(i.get('notes') or []) == 3, o)
    r = js("rd('sys/status').then(s=>JSON.stringify([s.runningStable,s.stableVersion]),e=>'ERR '+e.message)", 60)
    ok('2b. the page asks rudolfd (sys/status, which runs rudolf-update): what shows the button is there (not a stable release; the last stable one 1.5.9)', isinstance(r, str) and r.startswith('[') and json.loads(r) == [False, ms('version')], r)
    o = sh(f"{U} to-stable; echo rc=$?; cat /persist/os/system/state /persist/os/update.conf", 120); st = state(sh)
    ok('4. to-stable: slot a starts next, nothing downloaded, Get beta versions off (the stable address, no "tostable")',
       'rc=0' in o and 'nothing to download' in o and st.get('next') == 'a' and st.get('tries') == '2' and f'url={OFF}' in o and 'tostable=1' not in o and 'downloading' not in o, o)
    sh('sync', 60); vm.stop(); time.sleep(3)
    # ---- 1.5.9
    vm, g, up, js = boot('a1', 'a', trying=True); sh = lambda c, t=60: run(g, c, t)
    o = sh("cat /etc/rudolf/slot; rudolf-update boot-ok; rudolf-update status; cat /persist/os/system/state") if up else 'not up'
    ok('5. 1.5.9 starts from slot a and is kept (its own boot-ok)', up and ms('version') in o and state(sh).get('good') == 'a' and state(sh).get('prev') == 'b', o)
    if not up: raise SystemExit
    out('sign-in', si := signin(js, 'boss', 'b1')); od = on_disk(js)
    o = sh(f"stat -c '%u %a %n' /persist/accounts/{BOSS} /persist/accounts/{BOSS}/Documents/BETA.TXT; id acct{UB}; "
           f"su -s /bin/sh acct{UB} -c 'cat /home/acct{UB}/Documents/BETA.TXT; echo made on 1.5.9 > /home/acct{UB}/Documents/STABLE.TXT; ls -a /home/acct{UB}/.local/share 2>&1'; sync", 120)
    ok('6. THE DECISION (1.6.0 -> 1.5.9): 1.5.9 signs the beta\'s account in, opens its folder (its own Linux user, 0700), reads and writes its files',
       si == 'in boss' and od is True and f'{UB} 700 /persist/accounts/{BOSS}' in o and 'made on the beta' in o and 'Permission denied' not in o, [si, od, o])
    out(signout(js)); time.sleep(5)
    # ---- back up to the beta
    put_ru(g, sh); out('tunnel', tunnel(g, sh))
    o = sh(f"{U} set channel beta; {U} check; {U} download; echo rc=$?; cat /persist/os/system/state", 900); st = state(sh)
    ok('7. back up: Get beta versions on, the beta comes back from what is on this computer (nothing downloaded), slot b next',
       'rc=0' in o and st.get('next') == 'b' and 'downloading' not in o and o.count('unchanged') >= 5, o)
    sh('sync', 60); vm.stop(); time.sleep(3)
    vm, g, up, js = boot('b2', 'b', trying=True); sh = lambda c, t=60: run(g, c, t)
    o = sh("cat /etc/rudolf/slot; rudolf-update boot-ok; rudolf-update status") if up else 'not up'
    ok('8. the beta starts again (slot b) and is kept', up and mb('version') in o and state(sh).get('good') == 'b', o)
    if not up: raise SystemExit
    out('sign-in', si := signin(js, 'boss', 'b1')); od = on_disk(js)
    o = sh(f"su -s /bin/sh acct{UB} -c 'cat /home/acct{UB}/Documents/BETA.TXT /home/acct{UB}/Documents/STABLE.TXT'", 60)
    ok('9. the account\'s files on the beta again: the beta\'s and the one written on 1.5.9', si == 'in boss' and od is True and 'made on the beta' in o and 'made on 1.5.9' in o, [si, od, o])
    out(signout(js)); time.sleep(3)
    # ---- the refusal: the stable channel naming 1.5.8 (its real signed list), on a computer that never saw a later one
    put_ru(g, sh); out('tunnel', tunnel(g, sh)); channel('stable', OLD)
    # (what to-stable must not change is kept after the check, which writes down when it checked)
    o = sh(f"rm -f /persist/os/system/stable.txt /persist/os/system/stable.sig; {U} check >/dev/null; cp /persist/os/system/state /tmp/state0; cp /persist/os/update.conf /tmp/conf0; "
           f"{U} stable-info; {U} to-stable; echo rc=$?; cmp /tmp/state0 /persist/os/system/state && cmp /tmp/conf0 /persist/os/update.conf && ! ls /persist/os/system | grep -q '[.]new$' && echo UNCHANGED", 180); i = J(o)
    ok('10. a stable release that cannot read this computer\'s data (1.5.8: one Linux user for every account) is refused, saying why; nothing changed',
       i.get('compatible') is False and 'rc=8' in o and 'cannot read the accounts and files on this computer' in o and 'UNCHANGED' in o, o)
finally:
    try: tun.kill(); httpd.shutdown()
    except Exception: pass
    try: vm and vm.stop()
    except Exception: pass
    shutil.rmtree(W, ignore_errors=True)
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
