# Rudolf OS 1.6.1 (docs/plan-1.6.1.md Part 21; docs/plan-1.6.0.md Part 20's planned test): a smart card on the real release in
# QEMU, Standard. The card is a virtual PIV card (iso/qt/host/vpiv.py: SP 800-73's PIV application, a key and certificate
# from a test CA made here, never kept) in QEMU's USB CCID reader (usb-ccid with ccid-card-passthru: QEMU's VSCard protocol),
# so the guest has a real USB smart card reader with a card in it, and what is tested is the release's own: its devplug,
# pcscd, libccid, OpenSC's PIV driver and PKCS#11 module, NSS, /usr/lib/rudolf/smartcard, rudolfd's sc/*, Oracle's Chromium.
# The test's own changes in the guest: the test CA trusted in the desktop's certificate database (for the test site), and
# OpenSC told not to use a PIN pad (QEMU's reader claims one it does not have).
#   1. no reader: sc/status lists none
#   2. the reader plugged in (QMP device_add, as a USB reader plugged in): devplug's "Smart card reader", pcscd started,
#      sc/status lists the reader with no card
#   3. the card put in: sc/status shows the card (OpenSC's PIV token, named after its holder), its certificate as it is on
#      the card; the card was never asked for its PIN. Settings › Smart cards (sc60.js, the page's own) shows the holder's
#      name, the expiry date and "Expires in N days"
#   4. the desktop's certificate database (Oracle's Chromium's): the card's module ("Smart cards"), the card's certificate
#   5. Oracle signs in to a website with the card: a local test site (a TLS server here, reached through QEMU's guestfwd,
#      that asks for a certificate from the test CA) opened in Oracle; Chromium's certificate picker (Enter: the card's) and
#      its PIN box (the PIN typed as keys); the site sees the card's holder, the card checked the PIN and signed; nothing
#      signed before the PIN. (A wrong PIN is vpiv_t.py's, through NSS on the host: Chromium's retry is not driven here)
#   6. a Tor window (the page's own torOpenWin, with the test site; its proxy pointed at a SOCKS stand-in for Tor here): its
#      Chromium has a HOME of its own with no card module in its certificate database, and the card is asked nothing while it
#      runs. (Whether it reaches the site is only noted: its page stops at the first-start wizard, which the test skips on
#      the desktop's page)
#   7. "Lock the screen when my card is taken out" on, the card taken out: the screen locks; sc/status shows no card
#   8. the reader unplugged (device_del): sc/status lists none again
# Not covered here: a real CAC and reader (the owner's checklist), Native and Lite, the Debian container's pcsc_scan, the DoD
# bundle (sc60_t.js and smartcard60_t.sh test that on the host), a website reached through a Tor window (6), Chromium's retry
# after a wrong PIN (vpiv_t.py tests a wrong PIN through NSS on the host).
#   python3 sc161_t.py REL-DIR    (SB_PARTS, QEMU, QEMU_ACCEL as for t160; root)
#   SC161_FIX=DIR: this tree's DIR/index.html, DIR/smartcard and DIR/devplug put in place of the release's (an extra disk), and
#   DIR/libpcsclite.so.1 where the release has none (1.6.1's ci_release.sh adds it), before the desktop's page is loaded again.
#   Without it, 0a fails on 1.6.0 (OpenSC's module does not start) and the rest with it
import os, sys, time, re, subprocess, shutil, struct, json, socket, ssl, threading, base64, datetime
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT); sys.path.insert(0, os.path.join(QT, 'host'))
import sbvm, vpiv
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); FIX = os.environ.get('SC161_FIX', '')
LOG = open('sc161.txt', 'w'); R = []; T0 = time.time()
def out(*a):
    s = ' '.join(str(x) for x in a); s = f'[{int(time.time() - T0):5d}s] ' + s; print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:1500] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 3000; DRV = os.path.abspath('sc161.img'); MEM = int(os.environ.get('MEM', '4096')); W = os.path.abspath('w161sc')
shutil.rmtree(W, ignore_errors=True); os.makedirs(W); os.chmod(W, 0o700)
K = f'{W}/k'; P = vpiv.pki(K); CN = P['card']; card = vpiv.PIVCard(K)
SITE_PORT = int(os.environ.get('SC161_PORT', '8661'))
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
# the extra disk: this tree's files, the test CA's certificate (public) and the test site's address
FD = os.path.join(W, 'fix'); os.makedirs(FD)
if FIX:
    for n in ('index.html', 'smartcard', 'devplug', 'libpcsclite.so.1'):
        if os.path.exists(f'{FIX}/{n}'): shutil.copy(f'{FIX}/{n}', f'{FD}/{n}')
shutil.copy(f'{K}/ca.der', f'{FD}/ca.der')
FIXIMG = os.path.abspath('sc161fix.img'); subprocess.run(['truncate', '-s', '64M', FIXIMG], check=True)
subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'SC161FIX', '-d', FD, FIXIMG], check=True)

# the test site: a TLS server that asks for a certificate from the test CA (each connection's certificate noted), reached
# from the guest at 10.0.2.100:443 through QEMU's guestfwd (a relay started by QEMU for each connection)
SEEN = []
srv = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER); srv.load_cert_chain(f'{K}/server.pem', f'{K}/server.key')
srv.verify_mode = ssl.CERT_OPTIONAL; srv.load_verify_locations(f'{K}/ca.pem')
LS = socket.socket(); LS.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); LS.bind(('127.0.0.1', SITE_PORT)); LS.listen(20)
def site():
    while True:
        try: c, _ = LS.accept()
        except OSError: return
        def one(c):
            try:
                c.settimeout(120); t = srv.wrap_socket(c, server_side=True); pc = t.getpeercert() or {}
                who = dict(x[0] for x in pc.get('subject', ())).get('commonName', '')
                SEEN.append(who or '(no certificate)'); t.recv(4000)
                body = (f'<!doctype html><title>Card {"ok " + who if who else "none"}</title><h1>{"Signed in with the card: " + who if who else "No card certificate"}</h1>').encode()
                t.sendall(b'HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: ' + str(len(body)).encode() + b'\r\n\r\n' + body); t.close()
            except Exception as e: SEEN.append('handshake: ' + str(e)[:100]); c.close()
        threading.Thread(target=one, args=(c,), daemon=True).start()
threading.Thread(target=site, daemon=True).start()
# a SOCKS5 stand-in for Tor (the Tor window's proxy, pointed here): CONNECT to anything goes to the test site
SOCKS = []
SL = socket.socket(); SL.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); SL.bind(('127.0.0.1', SITE_PORT + 1)); SL.listen(20)
def socks():
    def rx(c, n):
        b = b''
        while len(b) < n:
            d = c.recv(n - len(b))
            if not d: raise ConnectionError('closed')
            b += d
        return b
    def pipe(a, b):
        try:
            while True:
                d = a.recv(65536)
                if not d: break
                b.sendall(d)
        except OSError: pass
        for x in (a, b):
            try: x.shutdown(socket.SHUT_RDWR)
            except OSError: pass
    def one(c):
        try:
            v, n = rx(c, 2); rx(c, n); c.sendall(b'\x05\x00')
            v, cmd, _, at = rx(c, 4)
            host = socket.inet_ntoa(rx(c, 4)) if at == 1 else rx(c, rx(c, 1)[0]).decode() if at == 3 else rx(c, 16).hex()
            port = struct.unpack('!H', rx(c, 2))[0]; SOCKS.append(f'{host}:{port}')
            t = socket.create_connection(('127.0.0.1', SITE_PORT)); c.sendall(b'\x05\x00\x00\x01\x00\x00\x00\x00\x00\x00')
            threading.Thread(target=pipe, args=(c, t), daemon=True).start(); pipe(t, c)
        except Exception as e: SOCKS.append('error: ' + str(e)[:80]); c.close()
    while True:
        try: c, _ = SL.accept()
        except OSError: return
        threading.Thread(target=one, args=(c,), daemon=True).start()
threading.Thread(target=socks, daemon=True).start()
RELAY = os.path.join(W, 'relay.py')
open(RELAY, 'w').write(f'''import socket,sys,os,threading
s=socket.create_connection(("127.0.0.1",int(sys.argv[1])))
def up():
    while True:
        d=os.read(0,65536)
        if not d: break
        s.sendall(d)
    try: s.shutdown(socket.SHUT_WR)
    except OSError: pass
threading.Thread(target=up,daemon=True).start()
while True:
    d=s.recv(65536)
    if not d: break
    os.write(1,d)
''')
VSC = f'/tmp/vsc-sc161.sock'
INIT_JS = "(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};return 1})()"
def boot(tag):
    try: os.unlink(VSC)
    except OSError: pass
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf160',
             '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1',
             '-drive', f'file={FIXIMG},format=raw,if=none,id=fx,readonly=on', '-device', 'nvme,drive=fx,serial=sc161fix',
             # (the guest's network: QEMU's own, nothing outside but the test site)
             '-netdev', f'user,id=u0,restrict=on,guestfwd=tcp:10.0.2.100:443-cmd:{sys.executable} {RELAY} {SITE_PORT},guestfwd=tcp:10.0.2.100:9050-cmd:{sys.executable} {RELAY} {SITE_PORT + 1}',
             '-netdev', 'hubport,id=n1,hubid=0,netdev=u0',
             '-chardev', f'socket,id=vsc,path={VSC},server=on,wait=off']
    vm = sbvm.VM('sc161' + tag, PARTS, [], secure=False, mem=MEM, extra=extra)
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    time.sleep(30); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    return vm, g, up
def run(g, c, t=60, show=True):
    f = os.path.join(W, 'c.sh'); open(f, 'w').write(c + '\n'); g.put(f, '/tmp/c161.sh', '644')
    o = g.sh('sh /tmp/c161.sh 2>&1', t)
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
def api(sh, p, post=False):
    return sh(f"busybox wget -q -O- --header \"X-Rudolf-Token: {TOK}\" {'--post-data= ' if post else ''}http://127.0.0.1:8080/api/{p}", 60, False)
def status(sh):
    o = api(sh, 'sc/status')
    try: return json.loads(o[o.index('{'):])
    except Exception: return {'raw': o[-400:]}
def wait_status(sh, cond, secs):
    end = time.time() + secs; s = {}
    while time.time() < end:
        s = status(sh)
        if cond(s): return s
        time.sleep(3)
    return s
def card_in(s): return [r for r in s.get('readers', []) if r.get('card')]

make_drive(DRV)
vm = None; vs = None
try:
    vm, g, up = boot('s'); js = jsfn(g); sh = lambda c, t=60, show=True: run(g, c, t, show)
    ok('0. the release starts (Standard)', up)
    if not up: raise RuntimeError('did not start')
    end = time.time() + 600
    while time.time() < end and js("typeof acctNew=='function'&&typeof userLogin=='function'", 20) is not True: time.sleep(5)
    # the release as built: OpenSC's module starts (C_Initialize: it needs pcsc-lite's client library, libpcsclite.so.1)
    o = sh("pkcs11-tool --module $(cat /etc/rudolf-pkcs11) -L; echo rc=$?; ls -la /usr/lib/x86_64-linux-gnu/libpcsclite.so.1", 60)
    N0A = '0a. OpenSC\'s module starts in the release (libpcsclite.so.1, which it opens, in the base layer)'
    if FIX: out(('PASS' if 'rc=0' in o else 'NOTE (put in place below, as 1.6.1\'s ci_release.sh adds it)') + ' ' + N0A + ' [as released, not counted with SC161_FIX]')
    else: ok(N0A, 'rc=0' in o, o)
    # this tree's files in place (FIX): the page, smartcard and devplug bound over the release's; libpcsclite.so.1 (1.6.1's
    # ci_release.sh adds it to the base layer) copied into the system's writable layer, and the card's module added to the
    # desktop's database as the session does at start-up (/usr/lib/rudolf/smartcard nss). QEMU's USB CCID reader says it
    # has a PIN pad (bPINSupport), which it does not have, so OpenSC is told not to use one (opensc.conf, the test's own; a
    # real reader without a PIN pad needs nothing). The test CA trusted by Oracle's Chromium (as the DoD's roots are, "C,,":
    # the desktop's database). Then the desktop's Chromium started again so that it reads them.
    o = sh("FX=; for d in /dev/nvme*n1; do blkid $d 2>/dev/null | grep -q SC161FIX && FX=$d; done; echo fix=$FX; mkdir -p /tmp/fx; mount -o ro $FX /tmp/fx; ls -la /tmp/fx; "
           "WWW=/usr/share/rudolf/www; [ -f /persist/os/current/index.html ] && WWW=/persist/os/current; "
           "[ -f /tmp/fx/index.html ] && cp /tmp/fx/index.html /tmp/page.html && chmod 644 /tmp/page.html && mount --bind /tmp/page.html $WWW/index.html && echo page; "
           "for n in smartcard devplug; do [ -f /tmp/fx/$n ] && cp /tmp/fx/$n /tmp/$n.fix && chmod 755 /tmp/$n.fix && mount --bind /tmp/$n.fix /usr/lib/rudolf/$n && echo $n; done; "
           "L=/usr/lib/x86_64-linux-gnu/libpcsclite.so.1; [ -e $L ] || { [ -f /tmp/fx/libpcsclite.so.1 ] && cp /tmp/fx/libpcsclite.so.1 $L && chmod 644 $L && echo libpcsclite; }; "
           "/usr/lib/rudolf/smartcard nss; echo nss=$?; "
           "printf 'app default {\\n\\treader_driver pcsc {\\n\\t\\tenable_pinpad = false;\\n\\t}\\n}\\n' > /etc/opensc/opensc.conf; "
           "cp /tmp/fx/ca.der /tmp/sc161ca.der; chmod 644 /tmp/sc161ca.der; "
           "su -s /bin/sh rudolf -c \"certutil -d sql:/home/rudolf/.pki/nssdb -A -n 'Rudolf test CA' -t 'C,,' -i /tmp/sc161ca.der\"; echo ca=$?; "
           "su -s /bin/sh rudolf -c 'modutil -dbdir sql:/home/rudolf/.pki/nssdb -list' | grep -E '^ *[0-9]\\.|library name'; "
           "ip -4 addr | grep -E 'inet |^[0-9]'; ip route; pkill -f '[-]-rudolf-kiosk'; echo reloaded", 120)
    ok('0b. the test CA in the desktop\'s certificate database' + (', this tree\'s page, smart card scripts and libpcsclite in place, the card\'s module added (nss)' if FIX else ''),
       'ca=0' in o and (not FIX or 'nss=0' in o), o)
    time.sleep(20); end = time.time() + 600
    while time.time() < end and js("typeof acctNew=='function'&&typeof userLogin=='function'&&typeof scSection=='function'", 20) is not True: time.sleep(5)
    time.sleep(30); js(INIT_JS)
    r = js("(async()=>{ACCTS.length=0;const b=await acctNew('boss','b1','admin');ACCTS.push(b);saveUser();return b.id})()", 120); out('account', r)
    r = js("(async()=>{try{const a=ACCTS.find(x=>x.n=='boss');if(USER)userLogout();USER=a;await userLogin(a,'b1');lastInput=Date.now();"
           "const L=document.getElementById('login');if(L)L.style.display='none';return 'in '+USER.n}catch(e){return 'ERR '+e.message}})()", 300)
    out('sign-in:', r); wait_for(js, "!!(USER&&USER.disk&&hdOn())", 300); time.sleep(5)

    # 1. no reader
    s = status(sh); ok('1. no reader: sc/status lists none', s.get('ok') and s.get('readers') == [], s)
    # 2. the reader plugged in
    vm.q.cmd('device_add', driver='usb-ccid', id='ccid')
    vs = vpiv.VSCard(card, VSC); vm.q.cmd('device_add', driver='ccid-card-passthru', id='card', chardev='vsc', bus='ccid.0'); vs.connect(60)
    s = wait_status(sh, lambda s: s.get('readers'), 90)
    o = sh("grep -h '^Smart card reader' /run/rudolf/usb/* 2>/dev/null; echo pcscd=$(pidof pcscd); ls -la /run/pcscd", 30)
    ok('2. the reader plugged in: devplug\'s "Smart card reader", pcscd started, sc/status lists the reader with no card',
       'Smart card reader|' in o and re.search(r'pcscd=\d', o) and s.get('pcscd') and s.get('readers') and not card_in(s), (s, o))
    out('reader:', [r.get('name') for r in s.get('readers', [])], 'QEMU:', vs.ev)
    # 3. the card put in
    vs.insert(); s = wait_status(sh, card_in, 90); rd0 = (card_in(s) or [{}])[0]
    ders = [base64.b64decode(c['der']) for c in rd0.get('certs', []) if c.get('der')]
    ok('3. the card put in: sc/status shows the card (OpenSC\'s PIV token, named after its holder)', rd0.get('label') == CN and rd0.get('maker') == 'piv_II', s)
    ok('...its certificate as it is on the card; the card was never asked for its PIN', open(f'{K}/card.der', 'rb').read() in ders and card.counts['verify_ok'] + card.counts['verify_bad'] == 0, (len(ders), card.counts))
    js("(()=>{const d=document.createElement('div');d.id='sc161';d.style.cssText='position:fixed;left:0;top:0;width:900px;background:#fff;color:#000;z-index:99999';document.body.appendChild(d);scSection(d);return 1})()")
    exp = datetime.datetime.strptime(P['expires'], '%Y-%m-%d')
    r = wait_for(js, f"(()=>{{const t=(document.getElementById('sc161')||{{}}).innerText||'';return /{CN}, until /.test(t)&&/Expires in \\d+ days/.test(t)}})()", 30)
    t = js("(document.getElementById('sc161')||{}).innerText||''")
    ok('...Settings › Smart cards (the page\'s own sc60.js on rudolfd\'s sc/status): the reader, the holder\'s name, the expiry date, "Expires in N days"', r is True, t)
    vm.shot('settings-smart-cards'); js("(()=>{const d=document.getElementById('sc161');d&&d.remove();return 1})()")
    # 4. the desktop's certificate database
    o = sh("su -s /bin/sh rudolf -c 'modutil -dbdir sql:/home/rudolf/.pki/nssdb -list; certutil -d sql:/home/rudolf/.pki/nssdb -L -h all'", 60)
    ok('4. the desktop\'s certificate database (Oracle\'s Chromium\'s): the card\'s module ("Smart cards") and the card\'s certificate',
       'Smart cards' in o and f'{CN}:Certificate for PIV Authentication' in o, o)
    # 5. Oracle signs in to the test site with the card
    n0 = len(SEEN); js("(()=>{browser('https://10.0.2.100/');return 1})()")
    end = time.time() + 60
    while time.time() < end and len(SEEN) <= n0: time.sleep(1)
    time.sleep(10); vm.shot('picker'); v0 = card.counts['verify_ok'] + card.counts['verify_bad']
    vm.keys('ret'); time.sleep(10); vm.shot('pin-box')
    asked = card.counts['verify_ok'] + card.counts['verify_bad'] - v0; signed0 = card.counts['sign']
    out('after the picker: the card', card.log[-6:], 'the site', SEEN[n0:])
    # (the PIN box opens without the keyboard on Standard: clicked into its field first, where it is drawn, at the top)
    vm.q.at(660, 65); time.sleep(2); vm.q.type('123456'); time.sleep(1); vm.shot('pin-typed'); vm.keys('ret')
    end = time.time() + 60
    while time.time() < end and CN not in SEEN[n0:]: time.sleep(1)
    time.sleep(5); vm.shot('signed-in')
    out('the card:', card.counts, card.log[-10:]); out('the site saw:', SEEN[n0:])
    ok('5. Oracle: the test site asks for a certificate; Chromium\'s picker, then its PIN box: nothing signed before the PIN', signed0 == 0 and asked == 0, (card.counts, card.log[-8:]))
    ok('...the PIN typed in Chromium\'s box: the card checked it and signed, the site sees the card\'s holder (TLS client authentication)',
       CN in SEEN[n0:] and card.counts['sign'] >= 1 and card.counts['verify_ok'] >= 1, (SEEN[n0:], card.counts))
    # 6. a Tor window: no card there
    v0 = card.counts['verify_ok'] + card.counts['verify_bad']; s0 = card.counts['sign']; n1 = len(SEEN)
    # (its proxy, Tor's 127.0.0.1:9050 in the session's flags for the Tor Chromium, pointed at the SOCKS stand-in above; nothing
    # else of the flags changed)
    o = sh("sed -i 's#socks5://127.0.0.1:9050#socks5://10.0.2.100:9050#' /run/rudolf/tor-chrome; grep -c 'socks5://10.0.2.100:9050' /run/rudolf/tor-chrome", 30)
    js("(()=>{window.__tw='wait';torOpenWin(['https://10.0.2.100/?tor']).then(()=>window.__tw='ok',e=>window.__tw='error '+e.message);return 1})()")
    r = wait_for(js, "window.__tw!='wait'", 60); out('torOpenWin:', js("window.__tw"))
    end = time.time() + 120
    while time.time() < end and not SOCKS: time.sleep(2)
    time.sleep(10); vm.shot('tor-window')
    o = sh("for p in $(pgrep -f 'class=RudolfTor'); do echo \"pid $p $(tr '\\0' '\\n' < /proc/$p/environ 2>/dev/null | grep '^HOME=')\"; done; "
           "H=$(for p in $(pgrep -f 'class=RudolfTor'); do tr '\\0' '\\n' < /proc/$p/environ 2>/dev/null | sed -n 's/^HOME=//p'; done | head -1); echo home=$H; "
           "[ -n \"$H\" ] && ls -la $H/.pki/nssdb 2>&1 | head -5; [ -d \"$H/.pki/nssdb\" ] && echo cards=$(modutil -dbdir sql:$H/.pki/nssdb -list 2>&1 | grep -c 'Smart cards')", 60)
    th = re.search(r'^home=(\S*)', o, re.M)
    ok('6. a Tor window (the page\'s own torOpenWin): its Chromium has a HOME of its own, with no card module in its certificate database',
       th and th.group(1) and not th.group(1).startswith('/home/rudolf') and not re.search(r'cards=[1-9]', o), o)
    ok('...the card was asked for no PIN and signed nothing while it ran; the test site got no card certificate',
       card.counts['verify_ok'] + card.counts['verify_bad'] == v0 and card.counts['sign'] == s0 and CN not in SEEN[n1:], (card.counts, SEEN[n1:]))
    # (the site through the Tor window itself: here its page stops at the first-start wizard, since the test skips the real
    # one on the desktop's page, so whether it reached the site is only noted)
    out('NOTE the Tor window through the SOCKS stand-in reached the test site:', 'yes' if any('10.0.2.100:443' in x for x in SOCKS) else 'no (not counted)', SOCKS, SEEN[n1:])
    sh("pkill -f 'class=RudolfTor'; echo closed", 30)
    # 7. Lock the screen when my card is taken out
    js("(()=>{S.sclock=true;try{persistS()}catch(e){};scLockWatch();return 1})()")
    r0 = wait_for(js, "SC_HAD===true", 90, 2); out('the watch has seen the card:', r0, js("JSON.stringify({u:USER&&USER.n,lw:!!SC_LW,had:SC_HAD,locked:LOCKED,sclock:S.sclock})"))
    vs.remove(); r = wait_for(js, "!!LOCKED", 60, 1); vm.shot('locked')
    if r is not True: out('the page:', js("JSON.stringify({u:USER&&USER.n,lw:!!SC_LW,had:SC_HAD,locked:LOCKED,sclock:S.sclock,err:(window.__cw||[]).slice(-5)})"))
    s = wait_status(sh, lambda s: s.get('readers') and not card_in(s), 60)
    ok('7. "Lock the screen when my card is taken out": the card taken out, the screen locks', r is True, r)
    ok('...sc/status: the reader with no card', s.get('readers') and not card_in(s), s)
    # 8. the reader unplugged
    vs.close(); vm.q.cmd('device_del', id='ccid')
    s = wait_status(sh, lambda s: s.get('readers') == [], 90)
    ok('8. the reader unplugged: sc/status lists none again', s.get('readers') == [], s)
    o = sh("tail -20 /run/rudolf/session.log 2>/dev/null | grep -iE 'smart|pcsc' ; grep -iE 'pcsc|smart' /run/rudolf/devplug.log 2>/dev/null | tail -10", 30)
except Exception as e:
    import traceback; out('ERROR', traceback.format_exc()[-1500:]); R.append(False)
finally:
    try: vs and vs.close()
    except Exception: pass
    if vm: vm.stop()
    for x in (LS, SL):
        try: x.close()
        except Exception: pass
    shutil.rmtree(K, ignore_errors=True)
out('the card asked:', card.log[-40:])
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
