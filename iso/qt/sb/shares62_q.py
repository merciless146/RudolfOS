# Rudolf OS 1.6.2 (docs/plan-1.6.2.md Part 35) in QEMU: network shares on the real release (the newest beta, or the one given),
# with this tree's kernel (SHARES_KERNEL: iso/systools/testkernel.sh's vmlinuz and modules.sfs, the SMB client a signed module)
# and this tree's rudolfd, rudolf-rootd, root helpers, rudolf-shares and rudolf-portal put in place (SHARES_FIX: a folder with
# rudolfd, rudolf-rootd, root/NAME, bin/NAME, on an extra disk; fix161.sh). Its second network card is on a bridge of the host's
# (br62, 192.168.62.0/24, addresses by dnsmasq as a home router gives them) with two servers in network namespaces: "nas62"
# (Samba 4: SMB 3 with encryption, a user's share and a guest share; multicast DNS), "win62" (a Windows computer's WS-Discovery,
# wsdd) and "old62" (SMB 1 only). SHARES_FIX/lib: this tree's firewall (1.6.2 step 3: its wsd chain), loaded in place of the release's.
# The firewall stays on. Through rudolfd's API as the page asks: the SMB client not loaded at start; two accounts (Ann, Bob),
# Ann signed in: her share opened (loaded then: the module signed), encrypted, for her own Linux user, a file copied each way
# with the right owners, the password kept in her keyring when asked and used again, a guest share, a wrong password and an
# SMB 1 server said plainly, SMB 1 refused when asked for; Bob's Linux user kept out; Disconnect; signed out: closed; Bob signed
# in: none of Ann's; the password never in a file, a log or the page.
#   python3 shares62_q.py REL-DIR     (SB_PARTS and QEMU as for r161; root; samba, wsdd, dnsmasq, python3-zeroconf)
import os, sys, time, re, subprocess, shutil, struct, json, base64
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); FIX = os.environ.get('SHARES_FIX', '')
KERN = os.environ.get('SHARES_KERNEL', ''); KDIR = os.path.abspath(KERN) if KERN else REL
LOG = open('shares62.txt', 'w'); R = []; T0 = time.time()
def out(*a):
    s = ' '.join(str(x) for x in a); s = f'[{int(time.time() - T0):5d}s] ' + s; print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:1800] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 3000; DRV = os.path.abspath('s62.img'); MEM = int(os.environ.get('MEM', '3072')); W = os.path.abspath('w62s')
shutil.rmtree(W, ignore_errors=True); os.makedirs(W)
def sh_(*a, **k): return subprocess.run(list(a), check=True, **k)
def q_(c): return subprocess.run(c, shell=True, capture_output=True, text=True).stdout
T160 = open(os.path.join(HERE, 't160_t.py')).read()
exec(re.search(r'^INIT_JS = .*$', T160, re.M).group(0)); exec(re.search(r'^INIT2_JS = .*$', T160, re.M).group(0))

# ---- the home network: a bridge, dnsmasq's addresses, the two servers
SH = os.path.join(W, 'srv'); PIDS = []; U1, U2 = 'ann62q', 'bob62q'
P1 = base64.b32encode(os.urandom(10)).decode().lower() + ',x=1'   # (made for this run; a comma, as an option would have)
def net_up():
    q_('ip link del br62 2>/dev/null; ip link del tap62 2>/dev/null; for n in nas62 old62 win62; do ip netns del $n 2>/dev/null; ip link del v$n 2>/dev/null; done')
    sh_('ip', 'link', 'add', 'br62', 'type', 'bridge'); sh_('ip', 'addr', 'add', '192.168.62.1/24', 'dev', 'br62'); sh_('ip', 'link', 'set', 'br62', 'up')
    sh_('ip', 'tuntap', 'add', 'tap62', 'mode', 'tap'); sh_('ip', 'link', 'set', 'tap62', 'master', 'br62', 'up')
    for ns, a in (('nas62', 2), ('old62', 3), ('win62', 5)):
        sh_('ip', 'netns', 'add', ns); sh_('ip', 'link', 'add', 'v' + ns, 'type', 'veth', 'peer', 'name', 'e' + ns); sh_('ip', 'link', 'set', 'v' + ns, 'master', 'br62', 'up')
        sh_('ip', 'link', 'set', 'e' + ns, 'netns', ns); sh_('ip', '-n', ns, 'addr', 'add', f'192.168.62.{a}/24', 'dev', 'e' + ns)
        sh_('ip', '-n', ns, 'link', 'set', 'e' + ns, 'up'); sh_('ip', '-n', ns, 'link', 'set', 'lo', 'up'); sh_('ip', '-n', ns, 'route', 'add', '224.0.0.0/4', 'dev', 'e' + ns)
    for u in (U1, U2): q_(f'useradd -M -s /bin/false {u}')
    for d in ('docs', 'pub', 'old'): os.makedirs(f'{SH}/{d}', exist_ok=True)
    sh_('chown', U1, f'{SH}/docs'); os.chmod(f'{SH}/pub', 0o777); os.chmod(f'{SH}/old', 0o777)
    open(f'{SH}/docs/onserver.txt', 'w').write('from the NAS\n'); sh_('chown', U1, f'{SH}/docs/onserver.txt')
    for n, addr, lo, hi in (('nas', '192.168.62.2', 'SMB2_10', 'SMB3'), ('old', '192.168.62.3', 'NT1', 'NT1')):
        b = f'{W}/{n}'
        for d in ('priv', 'lock', 'state', 'cache', 'pid', 'ncalrpc', 'log'): os.makedirs(f'{b}/{d}', exist_ok=True)
        open(f'{b}/smb.conf', 'w').write(f'''[global]
 workgroup = HOME62
 netbios name = {n.upper()}62
 server role = standalone server
 map to guest = Bad User
 interfaces = {addr}
 bind interfaces only = yes
 lock directory = {b}/lock
 state directory = {b}/state
 cache directory = {b}/cache
 private dir = {b}/priv
 pid directory = {b}/pid
 ncalrpc dir = {b}/ncalrpc
 log file = {b}/log/%m.log
 disable netbios = yes
 server min protocol = {lo}
 server max protocol = {hi}
[docs]
 path = {SH}/docs
 valid users = {U1}
 read only = no
[pub]
 path = {SH}/pub
 guest ok = yes
 read only = no
 force user = nobody
[old]
 path = {SH}/old
 guest ok = yes
''')
    os.makedirs('/run/samba', exist_ok=True)
    subprocess.run(['smbpasswd', '-c', f'{W}/nas/smb.conf', '-s', '-a', U1], input=f'{P1}\n{P1}\n', text=True, capture_output=True)
    for c in ([ 'setsid', 'ip', 'netns', 'exec', 'nas62', 'smbd', '-s', f'{W}/nas/smb.conf', '-F', '--no-process-group'],
              [ 'setsid', 'ip', 'netns', 'exec', 'old62', 'smbd', '-s', f'{W}/old/smb.conf', '-F', '--no-process-group'],
              [ 'setsid', 'ip', 'netns', 'exec', 'win62', 'wsdd', '-i', 'ewin62', '-n', 'WIN62', '-w', 'HOME62'],
              [ 'setsid', 'ip', 'netns', 'exec', 'nas62', sys.executable, os.path.join(QT, 'host', 'fixtures', 'smbmdns.py'), 'Nas62', '192.168.62.2'],
              [ 'dnsmasq', '--no-daemon', '--interface=br62', '--bind-interfaces', '--port=0', '--dhcp-range=192.168.62.100,192.168.62.150,12h', '--dhcp-leasefile=' + W + '/leases']):
        PIDS.append(subprocess.Popen(c, stdout=open(f'{W}/{c[4] if len(c) > 4 else c[0]}-{len(PIDS)}.log', 'w'), stderr=subprocess.STDOUT))
def net_down():
    for p in PIDS:
        try: p.terminate(); p.wait(10)
        except Exception: pass
    q_('ip link del tap62; ip link del br62; ip netns del nas62; ip netns del old62; ip netns del win62')
    for u in (U1, U2): q_(f'userdel {u}')
def make_drive(path):
    with open(path, 'wb') as f: f.truncate((2048 + DMB * 2048 + 2048) * 512)
    T = path + '.tree'; shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    d = f'{T}/os/system/a'; os.makedirs(d)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs', 'manifest.txt', 'manifest.sig'): shutil.copy(f'{KDIR if n in ("vmlinuz", "modules.sfs") else REL}/{n}', f'{d}/{n}')
    open(f'{d}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n"); open(f'{d}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    sh_('chown', '-R', '1000:1000', f'{T}/home/rudolf')
    sh_('truncate', '-s', f'{DMB}M', path + '.data'); sh_('mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, path + '.data'); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(446); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', 2048, DMB * 2048)); f.seek(510); f.write(b'\x55\xaa')
        f.seek(2048 * 512)
        with open(path + '.data', 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(path + '.data')
FIXIMG = ''
if FIX:
    shutil.copytree(FIX, f'{W}/fix'); FIXIMG = os.path.abspath('s62fix.img'); sh_('truncate', '-s', '96M', FIXIMG)
    sh_('mkfs.ext4', '-q', '-F', '-L', 'R161FIX', '-d', f'{W}/fix', FIXIMG)
def run(g, c, t=60, show=True):
    f = os.path.join(W, 'c.sh'); open(f, 'w').write(c + '\n'); g.put(f, '/tmp/c62.sh', '644')
    o = g.sh('sh /tmp/c62.sh 2>&1', t)
    if show: out('$', c[:200].replace('\n', ' ; ')); out(o.rstrip()[-2500:])
    return o
def boot(tag):
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf162',
             '-kernel', f'{KDIR}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1',
             '-device', 'e1000,netdev=n62,mac=52:54:00:62:00:10', '-netdev', 'tap,id=n62,ifname=tap62,script=no,downscript=no']
    if FIXIMG: extra += ['-drive', f'file={FIXIMG},format=raw,if=none,id=fx,readonly=on', '-device', 'nvme,drive=fx,serial=s62fix']
    vm = sbvm.VM('s62' + tag, PARTS, [], secure=False, mem=MEM, extra=extra)
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
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
    return vm, g, up, js
def settle(g, js):
    end = time.time() + 600
    while time.time() < end and js("typeof acctNew=='function'&&typeof userLogin=='function'", 20) is not True: time.sleep(5)
    js(INIT_JS); js(INIT2_JS)
    o = run(g, "last=; same=0; i=0; while [ $i -lt 40 ]; do i=$((i+1)); v=$(timeout 20 cdp-eval 'Math.round(performance.timeOrigin)' 2>/dev/null | grep -o '\"value\":[0-9]*' | head -1); "
               "if [ -n \"$v\" ] && [ \"$v\" = \"$last\" ]; then same=$((same+5)); else same=0; last=$v; fi; [ $same -ge 20 ] && break; sleep 5; done; echo \"SETTLED $same $last after $i\"", 1100)
    js(INIT_JS); js(INIT2_JS); return 'SETTLED 2' in o or 'SETTLED 3' in o
def signin(js, n, pw, secs=600):
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
def on_disk(js, secs=300):
    end = time.time() + secs
    while time.time() < end:
        if js("!!(USER&&USER.disk&&hdOn())", 60) is True: return True
        time.sleep(5)
    return False
def signout(js): return js("(async()=>{try{fsFlush()}catch(e){}try{await homeChain}catch(e){}if(USER)userLogout();await new Promise(r=>setTimeout(r,4000));return 'out'})()", 180)
TOK = "$(busybox wget -q -O- http://127.0.0.1:8080/ | grep -o 'token:\"[0-9a-f]*' | cut -d'\"' -f2)"
def api(g, p, post=False, body=None, t=90):
    """the API as the page asks it (an HTTP request of its own through busybox nc: the status and the answer's JSON)"""
    f = os.path.join(W, 'body'); open(f, 'w').write(body or ''); g.put(f, '/tmp/s62.body', '600'); os.remove(f)
    o = run(g, f"T={TOK}; n=$(wc -c < /tmp/s62.body); {{ printf '{'POST' if post else 'GET'} /api/{p} HTTP/1.0\\r\\nHost: 127.0.0.1\\r\\nX-Rudolf-Token: %s\\r\\nContent-Length: %s\\r\\n\\r\\n' \"$T\" $n; cat /tmp/s62.body; }} | busybox nc 127.0.0.1 8080; echo; rm -f /tmp/s62.body", t, False)
    out('API', p, '->', o.strip()[-600:]); m = re.search(r'HTTP/1\.[01] (\d+)', o); b = re.search(r'\r?\n\r?\n(\{.*\})', o, re.S)
    try: return (json.loads(b.group(1)) if b else {}), (int(m.group(1)) if m else 0)
    except ValueError: return {}, int(m.group(1)) if m else 0
def raw(g, p): return run(g, f"T={TOK}; printf 'GET /api/{p} HTTP/1.0\\r\\nX-Rudolf-Token: %s\\r\\n\\r\\n' \"$T\" | busybox nc 127.0.0.1 8080", 60, False)
net_up(); make_drive(DRV); vm = None
try:
    vm, g, up, js = boot('a'); sh = lambda c, t=60, show=True: run(g, c, t, show)
    ok('0. the release starts (this tree\'s kernel)' if KERN else '0. the release starts', up)
    if not up: raise RuntimeError('did not start')
    if FIX:
        g.put(os.path.join(HERE, 'fix161.sh'), '/tmp/fix161.sh', '644')
        o = sh("sh /tmp/fix161.sh reload; for b in /tmp/fx/bin/*; do cp $b /usr/bin/${b##*/} && chmod 755 /usr/bin/${b##*/} && echo in ${b##*/}; done; for l in /tmp/fx/lib/*; do [ -e $l ] && cp $l /usr/lib/rudolf/${l##*/} && chmod 755 /usr/lib/rudolf/${l##*/} && echo in ${l##*/}; done; [ -e /tmp/fx/lib/firewall ] && /usr/lib/rudolf/firewall start && echo firewall-reloaded", 300)
        ok('0b. this tree\'s rudolfd (through rudolf-rootd), helpers, rudolf-shares and rudolf-portal in place', 'through rudolf-rootd' in o and 'in rudolf-shares' in o, o[-800:])
    o = sh("uname -r; lsmod | grep -c '^cifs ' ; grep -cw cifs /proc/filesystems; /usr/lib/rudolf/firewall status; "
           "for i in /sys/class/net/*; do [ \"$(cat $i/address)\" = 52:54:00:62:00:10 ] && echo nic=${i##*/}; done", 60)
    m = re.search(r'nic=(\S+)', o); NIC = m.group(1) if m else 'eth1'
    ok('1. the SMB client is not loaded at start (no cifs module, no cifs file system); the firewall on', re.search(r'^0\n0$', o, re.M) and re.search(r'^on ', o, re.M), o)
    o = sh(f"for i in $(seq 1 60); do ip -4 addr show {NIC} | grep -q 'inet 192.168.62.' && break; [ $i = 20 ] && (udhcpc -i {NIC} -q -n -s /usr/lib/rudolf/udhcpc.sh >/dev/null 2>&1 &); sleep 2; done; ip -4 addr show {NIC} | grep inet", 200)
    ok(f'2. on the home network ({NIC}: an address from the router, 192.168.62.100-150)', re.search(r'inet 192\.168\.62\.1[0-5]\d', o), o)
    ok('2b. the page settled', settle(g, js))
    r = js("(async()=>{ACCTS.length=0;const a=await acctNew('ann','a1','admin');ACCTS.push(a);const b=await acctNew('bob','b1','user');ACCTS.push(b);saveUser();return a.id+' '+b.id})()", 120)
    out('accounts:', r); ANN, BOB = str(r).split()
    out('sign-in ann', signin(js, 'ann', 'a1')); ok('3. Ann signed in, her folder open', on_disk(js))
    o = sh("cat /run/rudolf/home-session; cat /persist/etc/rudolf/account-users")
    ua = re.search(rf'^{ANN} (\d+)', o, re.M); ub = re.search(rf'^{BOB} (\d+)', o, re.M); UA, UB = (ua.group(1) if ua else '?'), (ub.group(1) if ub else '?')
    ok(f'3b. Ann and Bob have Linux users of their own ({UA}, {UB})', ua and ub, o)
    # the wrong ways first
    j, c = api(g, 'shares/open?server=nas62&share=docs', True, f'{U1}\nwrong\n\n')
    ok('4. a wrong password: 401, said plainly ("The name or password is not right...")', c == 401 and j.get('why') == 'password' and 'not right' in j.get('error', ''), [c, j])
    j, c = api(g, 'shares/open?server=nas62&share=docs&ver=1.0', True, f'{U1}\n{P1}\n\n'); j2, c2 = api(g, 'shares/open?server=nas62&share=docs&ver=2.0', True, f'{U1}\n{P1}\n\n')
    ok('5. SMB 1.0 or 2.0 asked for: refused (400) before anything is tried', c == 400 and c2 == 400 and 'never SMB 1' in j.get('error', ''), [c, j, c2, j2])
    j, c = api(g, 'shares/open?server=192.168.62.3&share=old&guest=1', True, '\n\n\n')
    ok('6. the SMB 1 server: not opened, said plainly (426: speaks only SMB 1, old and unsafe, turn on SMB 2 or 3)', c == 426 and j.get('why') == 'smb1' and 'SMB 1' in j.get('error', ''), [c, j])
    j, c = api(g, 'shares/open?server=nas62&share=nothere', True, f'{U1}\n{P1}\n\n')
    ok('7. a share the server does not have: 404, named', c == 404 and j.get('why') == 'noshare' and 'nothere' in j.get('error', ''), [c, j])
    # Ann's share
    j, c = api(g, 'shares/open?server=nas62&share=docs&keep=1', True, f'{U1}\n{P1}\n\n'); ID = j.get('id', 'x'); MP = j.get('path', '/x')
    o = sh(f"grep ' {MP} ' /proc/mounts; stat -c '%u %a' /run/rudolf/shares/{UA} {MP}; lsmod | grep '^cifs '; modinfo -F signer cifs; modinfo -F sig_hashalgo cifs")
    ok('8. Ann\'s share opened (nas62 by multicast DNS), encrypted, kept in her keyring as asked; the SMB client loaded only now, its module signed (SHA-256)',
       c == 200 and j.get('encrypted') is True and j.get('kept') is True and re.search(r'^cifs ', o, re.M) and 'sha256' in o, [c, j, o])
    ok(f'9. mounted for Ann\'s own Linux user {UA} only: nosuid, nodev, noexec, its folders {UA}\'s, 0700', re.search(rf'cifs rw,nosuid,nodev,noexec.*uid={UA},forceuid,gid={UA},forcegid', o) and re.search(rf'^{UA} 700\n{UA} 700$', o, re.M), o)
    o2 = sh(f"stat -c '%u %a' {MP}/onserver.txt"); r1 = raw(g, f'fs/read?path={MP}/onserver.txt')
    j, c = api(g, f'fs/write?path={MP}/fromrudolf.txt', True, 'from Rudolf\n')
    time.sleep(2); so = q_(f"stat -c '%U %s' {SH}/docs/fromrudolf.txt; cat {SH}/docs/fromrudolf.txt"); o3 = sh(f"stat -c '%u %a' {MP}/fromrudolf.txt")
    ok(f'10. a file each way through File Explorer\'s fs/ requests: the server\'s read ({UA}\'s here, 0600); one written here is the server\'s user {U1}\'s there, {UA}\'s here',
       'from the NAS' in r1 and re.search(rf'^{UA} 600', o2, re.M) and so.startswith(f'{U1} 12') and 'from Rudolf' in so and re.search(rf'^{UA} 600', o3, re.M), [r1[-200:], o2, so, o3])
    o = sh(f"su -s /bin/sh acct{UB} -c 'ls /run/rudolf/shares/{UA}; cat {MP}/onserver.txt' 2>&1; echo rc=$?")
    ok(f'11. Bob\'s Linux user (acct{UB}) cannot see Ann\'s shares or read their files', 'ermission denied' in o and 'from the NAS' not in o, o)
    j, c = api(g, 'shares/open?server=nas62&share=pub&guest=1', True, '\n\n\n')
    ok('12. the guest share opened as a guest', c == 200 and j.get('path', '').startswith(f'/run/rudolf/shares/{UA}/'), [c, j])
    j, c = api(g, 'shares/open')
    ok('13. the open shares listed for Ann: docs (encrypted, as ann62q) and pub (guest)', c == 200 and sorted(s['share'] for s in j.get('shares', [])) == ['docs', 'pub'], [c, j])
    j, c = api(g, f'shares/close?id={ID}', True); o = sh(f"grep -c ' {MP} ' /proc/mounts; ls -d {MP} 2>&1")
    ok('14. Disconnect: closed, its folder gone', c == 200 and re.search(r'^0$', o, re.M) and 'No such file' in o, [c, j, o])
    j, c = api(g, 'shares/kept?server=nas62&share=docs&user=' + U1)
    j2, c2 = api(g, 'shares/open?server=nas62&share=docs', True, f'{U1}\n\n\n')
    ok('15. opened again without typing the password: the one kept in Ann\'s keyring used', j.get('kept') is True and c2 == 200 and j2.get('kept') is True, [j, c2, j2])
    # the password nowhere but the keyring (Ann's login keyring, which she asked to keep it in)
    pw1 = P1[:6]; pw2 = P1[6:]
    o = sh(f"p={pw1}; p=\"$p{pw2}\"; grep -rlsF \"$p\" /run /tmp /var /etc /persist /home /proc/mounts 2>/dev/null | grep -v '/keyrings/login.keyring$' | grep -vx /tmp/c62.sh | head; echo grep-done", 600)
    pg = js(f"(document.documentElement.outerHTML+JSON.stringify(localStorage)).includes({json.dumps(pw1)}+{json.dumps(pw2)})")
    ok('16. the password in no file, log or mount table (the keyring only), nor in the page', re.search(r'^grep-done$', o, re.M) and not re.search(r'^/', o.split('grep-done')[0], re.M) and pg is False, [o[-600:], pg])
    # 1.6.2 Part 35 step 3: File Explorer's Network, with the firewall on
    j, c = api(g, 'shares/found', t=60); sv = {x.get('name'): x for x in j.get('servers', [])}
    o = sh("/usr/lib/rudolf/firewall status; busybox netstat -uln 2>/dev/null | grep -E ':(3702|5353) ' | grep -v rudolf-print; echo nl-done")
    ok('20. found on the home network with the firewall on: the NAS by multicast DNS (Nas62, 192.168.62.2), the Windows computer by WS-Discovery (WIN62, 192.168.62.5, HOME62)',
       c == 200 and sv.get('Nas62', {}).get('how') == 'mdns' and sv.get('Nas62', {}).get('ip') == '192.168.62.2' and sv.get('WIN62', {}).get('how') == 'wsd' and sv.get('WIN62', {}).get('ip') == '192.168.62.5' and sv.get('WIN62', {}).get('group') == 'HOME62', [c, j])
    ok('21. ...and afterwards nothing listening for the network (no socket on 3702 or 5353 left), the firewall still on', re.search(r'^on ', o, re.M) and re.search(r'^nl-done$', o, re.M) and not re.search(r':(3702|5353) ', o), o)
    j, c = api(g, 'shares/list?server=nas62&ip=192.168.62.2', True, f'{U1}\n{P1}\n\n'); j2, c2 = api(g, 'shares/list?server=nas62&ip=192.168.62.2', True, f'{U1}\nwrong\n\n')
    ok('22. the NAS\'s shared folders for Ann (docs, pub, old; not IPC$), over SMB 3.1.1; a wrong password said plainly', c == 200 and [x['name'] for x in j.get('shares', [])] == ['docs', 'pub', 'old'] and j.get('dialect') == '3.1.1' and c2 == 401 and j2.get('why') == 'password', [c, j, c2, j2])
    # signed out: every share closed; Bob sees none
    out('sign-out', signout(js)); time.sleep(5)
    o = sh(f"grep -c ' /run/rudolf/shares/' /proc/mounts; ls /run/rudolf/shares/{UA} 2>&1 | wc -l")
    ok('17. Ann signed out: her shares closed', re.search(r'^0\n0$', o, re.M), o)
    out('sign-in bob', signin(js, 'bob', 'b1')); on_disk(js)
    j, c = api(g, 'shares/open'); j2, c2 = api(g, f'fs/list?path=/run/rudolf/shares/{UA}/{ID}')
    ok('18. Bob signed in: none of Ann\'s shares listed, and her shares\' folder refused (403)', c == 200 and j.get('shares') == [] and c2 == 403, [j, c2, j2])
    o = sh("grep -i 'network share' /persist/var/log/health.log /run/rudolf/health.log 2>/dev/null | tail -3; tail -c 1200 /run/rudolf/rudolfd.log")
    ok('19. the health log says a share was opened (\\\\server\\share), nothing more', 'network share was opened' in o and P1 not in o, o[-800:])
finally:
    try: vm and vm.stop()
    except Exception: pass
    net_down()
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
