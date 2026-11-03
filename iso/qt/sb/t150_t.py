# Rudolf OS 1.5.0 in QEMU: the firewall, the health log, remote access and data encryption, on the release's own
# kernel with its system in slot a (as t147m).
# The firewall is checked from outside: a second network card is joined to this script over UDP (QEMU's dgram
# network), and the script sends real TCP SYN packets to the guest (10.5.0.2) from 10.5.0.1, answering its ARP:
# a SYN-ACK means the port is reachable, a RST that nothing listens, nothing that the firewall dropped it.
#   python3 t150_t.py REL-DIR
import os, sys, time, re, subprocess, shutil, struct, json, socket, random
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); LOG = open('t150.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:400] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 1500; DRV = os.path.abspath('int150.img'); MEM = int(os.environ.get('MEM', '3072'))
PG, PH = 47611, 47612   # QEMU listens on PG for frames from here; sends the guest's frames to PH
GMAC = bytes.fromhex('525400123499'); HMAC = bytes.fromhex('525400aabbcc'); HIP = bytes([10, 5, 0, 1]); GIP = bytes([10, 5, 0, 2])

def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf/Downloads'); os.makedirs(f'{T}/etc')
    os.makedirs(f'{T}/debian/usr/bin'); open(f'{T}/debian/marker', 'w').write('debian here\n')
    open(f'{T}/home/rudolf/Downloads/secret.txt', 'w').write('my secret download\n')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    a = f'{T}/os/system/a'; os.makedirs(a)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs', 'manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{a}/{n}')
    open(f'{a}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n")
    open(f'{a}/complete', 'w').write('1\n'); open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    # (no -O encrypt: a drive formatted before 1.5.0; setup adds the feature)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', '^encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)

# ---- raw frames to the second network card
def csum(b):
    if len(b) % 2: b += b'\0'
    s = sum(struct.unpack('!%dH' % (len(b) // 2), b)); s = (s >> 16) + (s & 0xffff); s += s >> 16; return ~s & 0xffff
def syn(port, sport):
    tcp = struct.pack('!HHIIBBHHH', sport, port, random.randrange(1 << 32), 0, 5 << 4, 0x02, 64240, 0, 0)
    ph = HIP + GIP + struct.pack('!BBH', 0, 6, len(tcp)); tcp = tcp[:16] + struct.pack('!H', csum(ph + tcp)) + tcp[18:]
    ip = struct.pack('!BBHHHBBH4s4s', 0x45, 0, 20 + len(tcp), random.randrange(65536), 0, 64, 6, 0, HIP, GIP); ip = ip[:10] + struct.pack('!H', csum(ip)) + ip[12:]
    return GMAC + HMAC + b'\x08\x00' + ip + tcp
def arp_reply(fr):
    # who-has 10.5.0.1 from the guest: answer with our MAC
    if fr[12:14] != b'\x08\x06' or fr[20:22] != b'\x00\x01' or fr[38:42] != HIP: return None
    smac, sip = fr[22:28], fr[28:32]
    return smac + HMAC + b'\x08\x06' + struct.pack('!HHBBH', 1, 0x0800, 6, 4, 2) + HMAC + HIP + smac + sip
U = None
def probe(port, wait=6):
    """'open' (SYN-ACK), 'closed' (RST) or 'dropped' (nothing)"""
    sport = random.randrange(20000, 60000); U.sendto(syn(port, sport), ('127.0.0.1', PG)); end = time.time() + wait; tries = 1
    while time.time() < end:
        U.settimeout(0.5)
        try: fr, _ = U.recvfrom(4096)
        except socket.timeout:
            if tries < 3 and time.time() > end - wait + 2 * tries: U.sendto(syn(port, sport), ('127.0.0.1', PG)); tries += 1
            continue
        a = arp_reply(fr)
        if a: U.sendto(a, ('127.0.0.1', PG)); U.sendto(syn(port, sport), ('127.0.0.1', PG)); continue
        if fr[12:14] == b'\x08\x00' and fr[23] == 6 and fr[26:30] == GIP:
            ihl = (fr[14] & 15) * 4; t = fr[14 + ihl:]; sp, dp = struct.unpack('!HH', t[:4]); fl = t[13]
            if sp == port and dp == sport: return 'open' if fl & 0x12 == 0x12 else 'closed' if fl & 0x04 else 'other %x' % fl
    return 'dropped'

EXTRA = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf150',
         '-device', 'e1000,netdev=n1,mac=52:54:00:12:34:99',
         '-netdev', f'dgram,id=n1,local.type=inet,local.host=127.0.0.1,local.port={PG},remote.type=inet,remote.host=127.0.0.1,remote.port={PH}',
         '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1']
def boot(tag):
    vm = sbvm.VM('t150' + tag, PARTS, [], secure=False, mem=MEM, extra=EXTRA)
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    time.sleep(45); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    def js(e, t=60):
        for _ in range(4):
            r = g.js(e, t)
            if not (isinstance(r, tuple) and (r[0] == 'ERR' or (r[0] == 'EXC' and 'ReferenceError' in str(r[1])))): return r
            time.sleep(15)
        return r
    end = time.time() + 600
    while up and time.time() < end and js('typeof vaultSignin=="function"&&typeof fwStatus=="function"', 20) is not True: time.sleep(5)
    return vm, g, up, js
def guest_in(js, out):
    end = time.time() + 300
    while time.time() < end:
        r = js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};const b=[...document.querySelectorAll('button')].find(b=>b.textContent.trim()=='Guest'&&b.offsetParent!==null);if(b){b.click();return 'clicked'}return typeof USER!='undefined'&&USER&&typeof launch=='function'?'in':'wait'})()", 20)
        out('sign-in:', r)
        if r == 'in': return True
        time.sleep(12)
    return False
# the desktop's own sign-in with an account and its password (what the sign-in screen does after checking it)
SIGNIN = "(async()=>{const a=ACCTS.find(x=>x.n=='boss');if(!a)return 'no account';if(!(await pwCheck(a,'b1')))return 'bad';if(USER)userLogout();USER=a;await userLogin(a,'b1');lastInput=Date.now();return 'in '+USER.n})()"

make_drive(DRV)
U = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); U.bind(('127.0.0.1', PH))
vm = None
try:
    vm, g, up, js = boot('a')
    ok('1. Rudolf OS 1.5.0 starts', up)
    if not up: raise SystemExit
    guest_in(js, out); time.sleep(5)
    sh = lambda c, t=30: g.sh(c, t)
    # the second network card: 10.5.0.2
    o = sh("for i in /sys/class/net/*; do [ \"$(cat $i/address)\" = 52:54:00:12:34:99 ] && N=${i##*/}; done; ip link set $N up; ip addr add 10.5.0.2/24 dev $N; echo IF=$N; nft list table inet rudolf | head -30; /usr/lib/rudolf/firewall status")
    out(o)
    ok('2. the firewall is loaded at start-up (table inet rudolf, input chain, on)', 'chain input' in o and 'counter name "blocked" drop' in o and re.search(r'^on \d+', o, re.M), o)
    sh("mkdir -p /tmp/w && echo hi > /tmp/w/index.html && (busybox httpd -p 0.0.0.0:8099 -h /tmp/w &); sleep 1")
    p1 = probe(8099); c1 = sh('/usr/lib/rudolf/firewall status')
    ok('3. from another computer, a program listening on the network cannot be reached (dropped, counted)', p1 == 'dropped' and int(re.search(r'on (\d+)', c1).group(1)) > 0, [p1, c1])
    r = js("rdPost('firewall?on=0').then(s=>s.on)")
    p2 = probe(8099); p3 = probe(8100)
    ok('4. with the firewall off it can (SYN-ACK), and a closed port answers RST', r is False and p2 == 'open' and p3 == 'closed', [r, p2, p3])
    js("rdPost('firewall?on=1').then(s=>s.on)"); p4 = probe(8099)
    ok('5. on again: dropped again; Security Settings data says on', p4 == 'dropped' and js("fwStatus().then(s=>s.on)") is True, p4)
    # remote access, with an administrator account
    r = js("(async()=>{ACCTS.push({n:'boss',h:await pwMake('b1'),r:'admin'});saveUser();await rdPost('remote/accounts',rmAccounts());const s=await rdPost('remote?on=1');return s.on})()")
    # (it listens once its certificate is made: a few seconds on QEMU's emulated processor)
    sh("for i in $(seq 60); do netstat -ltn 2>/dev/null | grep -q ':8443 ' && break; sleep 1; done", 90)
    time.sleep(10)   # the desktop's status checks meanwhile (they started a second rudolf-remote before 1.5.1)
    o = sh("echo PIDS $(pgrep -x rudolf-remote | wc -l); echo LOOPS $(pgrep -f '[w]hile \\[ -e /run/rudolf/remote/on' | wc -l); echo INUSE $(grep -c 'already in use' /run/rudolf/remote.log); ls /persist/etc/remote/; echo CODE $(cat /run/rudolf/remote/code); nft list chain inet rudolf remote; netstat -ltn | grep 8443")
    out(o)
    ok('6. remote access starts once (one rudolf-remote), its certificate is made, the firewall opens 8443 to private addresses', r is True and re.search(r'^PIDS 1$', o, re.M) and re.search(r'^LOOPS 1$', o, re.M) and re.search(r'^INUSE 0$', o, re.M) and 'cert.pem' in o and 'tcp dport 8443 accept' in o and re.search(r'^CODE \d{6}$', o, re.M) is not None and ':8443' in o, o)
    p5 = probe(8443)
    ok('7. ...and another computer on the local network reaches it (10.5.0.1 -> 8443: SYN-ACK)', p5 == 'open', p5)
    st = sh("timeout 60 /opt/chromium/chrome --headless=new --no-sandbox --disable-gpu --ignore-certificate-errors --user-data-dir=/tmp/hc --dump-dom https://127.0.0.1:8443/status 2>/dev/null | grep -o '{.*}'", 90)
    ok('8. it answers over HTTPS (status: not paired, not signed in)', '"paired":false' in st and '"signedIn":false' in st, st)
    page = sh("echo PAGE $(timeout 60 /opt/chromium/chrome --headless=new --no-sandbox --disable-gpu --ignore-certificate-errors --user-data-dir=/tmp/hc --dump-dom https://127.0.0.1:8443/ 2>/dev/null | grep -c 'Pairing code')", 90)
    ok('9. the viewer page is served', re.search(r'^PAGE [1-9]', page, re.M) is not None, page)
    js("rdPost('remote?on=0').then(s=>s.on)"); time.sleep(4); p6 = probe(8443); o = sh("pgrep -x rudolf-remote | wc -l; nft list chain inet rudolf remote | grep -c 8443")
    ok('10. turned off: it stops, and 8443 is closed to the network again', p6 == 'dropped' and re.findall(r'^(\d+)$', o, re.M)[:2] == ['0', '0'], [p6, o])
    # data encryption: set up as the administrator (what Encrypt my data does after its dialog)
    r = js(SIGNIN, 120); out('signed in:', r)
    r = js("(async()=>{const dk=crypto.getRandomValues(new Uint8Array(64));const data={v:1,rec:await vWrap('RECOVERYKEYTEST1234567890ABCDE',dk),accts:{[USER.id]:await vWrap('b1',dk)}};const s=await rdPost('vault/setup',JSON.stringify(data));VAULTDK=dk;const u=await vUnlock(dk);return s.state+' '+u.state})()", 120)
    out('setup:', r)
    for _ in range(60):
        j = sh("cat /run/rudolf/job.name /run/rudolf/job.status 2>/dev/null; echo; cat /run/rudolf/job.log 2>/dev/null | tail -3")
        if re.search(r'^vault\s*\n?\s*\d+', j): break
        time.sleep(5)
    out(j)
    o = sh("tune2fs -l $(cat /etc/rudolf/persist) | grep -i features; ls -la /persist | grep -E 'debian|flatpak|vault'; ls -la /home/rudolf | grep Downloads; cat /home/rudolf/Downloads/secret.txt /persist/debian/marker; ls /persist/etc | grep vault")
    out(o)
    ok('11. setup adds the drive\'s encrypt feature and moves Debian, Flatpak and Downloads into the encrypted folder', ' encrypt' in o and 'debian -> /persist/vault/debian' in o and 'flatpak -> /persist/vault/flatpak' in o and 'Downloads -> /persist/vault/downloads' in o and 'vault.moving' not in o, o)
    ok('12. ...and the files are there, readable while it is unlocked', 'my secret download' in o and 'debian here' in o, o)
    js("fsFlush();1"); time.sleep(5)
    h = sh("cat /persist/var/log/health.log | cut -c1-160")
    ok('13. the health log has the start-up, the desktop ready, the firewall, remote access and encryption', all(f'"k":"{k}"' in h for k in ('start', 'ready', 'firewall', 'remote', 'vault')), h[-1500:])
    # a restart: locked until an account signs in
    sh('sync'); vm.stop(); time.sleep(3)
    vm, g, up, js = boot('b'); sh = lambda c, t=30: g.sh(c, t)
    ok('14. it starts again', up)
    guest_in(js, out); time.sleep(5)
    o = sh("ls /persist/vault | sed 's/^/LS /'; cat /home/rudolf/Downloads/secret.txt /persist/debian/marker 2>&1 | sed 's/^/CAT /'")
    names = re.findall(r'^LS (.*)$', o, re.M); cats = '\n'.join(re.findall(r'^CAT (.*)$', o, re.M))
    st = js("rd('vault').then(s=>s.state)")
    ok('15. after a restart it is locked: the names are scrambled and the files cannot be read', st == 'locked' and len(names) == 3 and not set(names) & {'debian', 'flatpak', 'downloads'} and 'my secret download' not in cats and 'debian here' not in cats, [st, names, cats])
    lk = js("fetch('/api/debian/start',{method:'POST',headers:{'X-Rudolf-Token':RD.token}}).then(r=>r.status)")
    ok('16. Linux programs say it is locked (423) instead of failing', lk == 423, lk)
    r = js(SIGNIN, 180); time.sleep(5); out('signed in:', r)
    o = sh("cat /home/rudolf/Downloads/secret.txt /persist/debian/marker; ls /persist/vault")
    st = js("rd('vault').then(s=>s.state)")
    ok('17. signing in with the password unlocks it', st == 'on' and 'my secret download' in o and 'debian here' in o, [st, o])
    fw = sh('/usr/lib/rudolf/firewall status')
    ok('18. the firewall is on again after the restart', re.search(r'^on \d+', fw, re.M) is not None, fw)
    out(sh("tail -15 /persist/var/log/health.log | cut -c1-200"))
finally:
    try: vm and vm.stop()
    except Exception: pass
try: os.remove(DRV)
except OSError: pass
out(f'{sum(R)}/{len(R)} passed')
