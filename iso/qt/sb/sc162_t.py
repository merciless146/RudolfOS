# Rudolf OS 1.6.2 (docs/plan-1.6.2.md Part 34, step 1) in QEMU: driverless scanning on the real release (the newest beta,
# or REL-DIR) with this tree's rudolf-scan, firewall and list of listeners put in place, and libsane and sane-airscan from
# the build machine (the same Ubuntu 24.04 packages the base layer takes them from), laid over the release's library
# folder (SC162_FIX: a folder with rudolf-scan, firewall, listeners, libsane.so.1 and libsane-airscan.so.1, on an extra
# disk labelled SC162FIX). Two stand-in scanners (qt/host/fixtures/esclsrv.py) on this computer, reached by the guest at
# 10.0.2.100 (eSCL) and 10.0.2.101 (WSD only) through QEMU's user network (guestfwd; restrict=on: nothing else); this
# script plays their multicast DNS and WS-Discovery answers on QEMU's hub at the Ethernet level, as m161_t.py plays
# printers (the mDNS answer to the group 224.0.0.251:5353; the ProbeMatches by unicast to the Probe's port 3702, for its
# MessageID). Pages are read back over the serial line and checked by their pixels (qt/host/fixtures/scancheck.py).
#   0. the release boots; F. this tree's files in place, the firewall on
#   1. find (as the desktop's user, rudolf): the eSCL scanner (mDNS) and the WSD one (its window opened by "firewall wsd
#      open"), the blocked counter not rising; 2. a page in colour, checked; 3. grey and black and white; 4. the feeder's
#      three pages into one PDF, each page checked; 5. a paper jam, the cover open, busy: said plainly; 6. the WSD window
#      closed: WSD's answer dropped and counted; 7. the self-test's listeners line; 8. memory and nothing left running.
# A USB stand-in: QEMU has no USB device that speaks IPP over USB (usb-host passes a real one through; there is no
# emulated printer-scanner), so the USB path (ipp-usb's address) is tested on the host (qt/host/scan162_t.sh, 28-29).
#   python3 sc162_t.py REL-DIR    (SB_PARTS, QEMU, QEMU_ACCEL as for p161_t.py; root)
import os, sys, time, re, subprocess, shutil, struct, socket, threading, base64, urllib.request
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); FIX = os.environ.get('SC162_FIX', '')
FX = os.path.join(QT, 'host', 'fixtures')
LOG = open('sc162.txt', 'w'); R = []; T0 = time.time()
def out(*a):
    s = ' '.join(str(x) for x in a); s = f'[{int(time.time() - T0):5d}s] ' + s; print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:1500] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 3000; DRV = os.path.abspath('sc162.img'); MEM = int(os.environ.get('MEM', '3072')); W = os.path.abspath('w162s')
shutil.rmtree(W, ignore_errors=True); os.makedirs(W)

# ---------------------------------------------------------------- the stand-in scanners on this computer
def free_port():
    s = socket.socket(); s.bind(('127.0.0.1', 0)); p = s.getsockname()[1]; s.close(); return p
P1, P2 = free_port(), free_port()
SRV = [subprocess.Popen([sys.executable, f'{FX}/esclsrv.py', '127.0.0.1', str(P1), f'{W}/r1', '--name', 'QEMU Scanner']),
       subprocess.Popen([sys.executable, f'{FX}/esclsrv.py', '127.0.0.1', str(P2), f'{W}/r2', '--name', 'QEMU WSD Scanner', '--no-escl', '--advertise', '10.0.2.101:8642'])]
for _ in range(100):
    if os.path.exists(f'{W}/r1') and os.path.exists(f'{W}/r2'): break
    time.sleep(0.1)
def ctl(q, port=None): return urllib.request.urlopen(f'http://127.0.0.1:{port or P1}/_test/{q}', timeout=5).read().decode()

# ---------------------------------------------------------------- their answers, as Ethernet frames on QEMU's hub
def csum(b):
    if len(b) % 2: b += b'\0'
    s = sum(struct.unpack('>%dH' % (len(b) // 2), b)); s = (s >> 16) + (s & 0xffff); s += s >> 16; return ~s & 0xffff
def frame(dmac, smac, sip, dip, sport, dport, payload, ttl=255):
    udp = struct.pack('>HHHH', sport, dport, 8 + len(payload), 0) + payload
    ph = socket.inet_aton(sip) + socket.inet_aton(dip) + struct.pack('>BBH', 0, 17, len(udp))
    c = csum(ph + udp) or 0xffff; udp = udp[:6] + struct.pack('>H', c) + udp[8:]
    ip = struct.pack('>BBHHHBBH4s4s', 0x45, 0, 20 + len(udp), 0, 0x4000, ttl, 17, 0, socket.inet_aton(sip), socket.inet_aton(dip))
    ip = ip[:10] + struct.pack('>H', csum(ip)) + ip[12:]
    return dmac + smac + b'\x08\x00' + ip + udp
def name(s): return b''.join(bytes([len(p.encode())]) + p.encode() for p in s.split('.')) + b'\0'
def rr(nm, t, data, cls=0x8001): return name(nm) + struct.pack('>HHIH', t, cls, 120, len(data)) + data
def mdns_answer():
    inst = 'QEMU Scanner._uscan._tcp.local'; host = 'qemu-scanner.local'
    txt = b''.join(bytes([len(x)]) + x for x in [b'txtvers=1', b'rs=eSCL', b'ty=QEMU Scanner', b'UUID=4509a320-00a0-008f-00b6-002507510eca'])
    recs = [rr('_uscan._tcp.local', 12, name(inst), 1), rr(inst, 33, struct.pack('>HHH', 0, 0, 8641) + name(host)), rr(inst, 16, txt), rr(host, 1, socket.inet_aton('10.0.2.100'))]
    return struct.pack('>HHHHHH', 0, 0x8400, 0, 1, 0, 3) + b''.join(recs)
def probe_match(mid):
    return (f'<?xml version="1.0" encoding="utf-8"?><soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing" xmlns:wsd="http://schemas.xmlsoap.org/ws/2005/04/discovery">'
            f'<soap:Header><wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/ProbeMatches</wsa:Action><wsa:MessageID>urn:uuid:1b2c3d4e-0000-4000-8000-000000008642</wsa:MessageID><wsa:RelatesTo>{mid}</wsa:RelatesTo></soap:Header>'
            f'<soap:Body><wsd:ProbeMatches><wsd:ProbeMatch><wsa:EndpointReference><wsa:Address>urn:uuid:6c1e0000-0000-4000-8000-000000008642</wsa:Address></wsa:EndpointReference>'
            f'<wsd:Types>wsdp:Device wscn:ScanDeviceType</wsd:Types><wsd:XAddrs>http://10.0.2.101:8642/wsd/device</wsd:XAddrs></wsd:ProbeMatch></wsd:ProbeMatches></soap:Body></soap:Envelope>').encode()
MC_MAC = bytes.fromhex('01005e0000fb'); SMAC = bytes.fromhex('525400123499')
HUB = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); HUB.bind(('127.0.0.1', 0)); HPORT = HUB.getsockname()[1]
QPORT = HPORT + 1
t = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
while True:
    try: t.bind(('127.0.0.1', QPORT)); t.close(); break
    except OSError: QPORT += 1
SEEN = {'mdns': 0, 'probes': 0}
def scanners():
    while True:
        f, _ = HUB.recvfrom(65536)
        if len(f) < 42 or f[12:14] != b'\x08\x00' or f[23] != 17: continue
        ihl = (f[14] & 15) * 4; ip = f[14:14 + ihl]; u = f[14 + ihl:]; src = socket.inet_ntoa(ip[12:16]); dst = socket.inet_ntoa(ip[16:20])
        sport, dport = struct.unpack('>HH', u[:4]); m = u[8:]; gmac = f[6:12]
        if dst == '224.0.0.251' and dport == 5353 and len(m) >= 12 and not m[2] & 0x80 and b'\x06_uscan\x04_tcp' in m:
            SEEN['mdns'] += 1; HUB.sendto(frame(MC_MAC, SMAC, '10.0.2.100', '224.0.0.251', 5353, 5353, mdns_answer()), ('127.0.0.1', QPORT))
        elif dst == '239.255.255.250' and dport == 3702 and b'ScanDeviceType' in m:
            mm = re.search(rb'MessageID>\s*([^<\s]+)\s*<', m)
            if mm:
                SEEN['probes'] += 1; GM[0] = gmac
                HUB.sendto(frame(gmac, SMAC, '10.0.2.101', src, 3702, sport, probe_match(mm.group(1).decode()), ttl=64), ('127.0.0.1', QPORT))
GM = [None]
threading.Thread(target=scanners, daemon=True).start()

# ---------------------------------------------------------------- the drive and the fix disk (as m161_t.py)
def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    d = f'{T}/os/system/a'; os.makedirs(d)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs'): shutil.copy(f'{REL}/{n}', f'{d}/{n}')
    for n in ('manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{d}/{n}')
    open(f'{d}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n"); open(f'{d}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(446); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as dd: shutil.copyfileobj(dd, f)
    os.remove(D)
make_drive(DRV)
FIXIMG = os.path.abspath('sc162fix.img'); subprocess.run(['truncate', '-s', '64M', FIXIMG], check=True)
subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'SC162FIX', '-d', FIX, FIXIMG], check=True)
NET = ['-netdev', f'user,id=u0,restrict=on,guestfwd=tcp:10.0.2.100:8641-tcp:127.0.0.1:{P1},guestfwd=tcp:10.0.2.101:8642-tcp:127.0.0.1:{P2}',
       '-netdev', 'hubport,id=hp1,hubid=0,netdev=u0',
       '-netdev', f'socket,id=s0,udp=127.0.0.1:{HPORT},localaddr=127.0.0.1:{QPORT}', '-netdev', 'hubport,id=hp2,hubid=0,netdev=s0']
extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf162',
         '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1'] + NET + \
        ['-drive', f'file={FIXIMG},format=raw,if=none,id=fx,readonly=on', '-device', 'nvme,drive=fx,serial=sc162fix']
vm = sbvm.VM('sc162', PARTS, [], secure=False, mem=MEM, extra=extra)
g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
time.sleep(30); end = time.time() + 1500; up = False
while time.time() < end:
    try:
        if 'ready' in g.sh('echo ready', 8): up = True; break
    except OSError: pass
    time.sleep(4)
ok(f"0. the release ({mv('version')}, {mv('serial')}) boots in QEMU", up)
def run(c, t=90, quiet=False):
    f = os.path.join(W, 'c.sh'); open(f, 'w').write(c + '\n'); g.put(f, '/tmp/c162.sh', '644')
    o = g.sh('sh /tmp/c162.sh 2>&1', t); out('$', c[:200].replace('\n', ' ; '))
    if not quiet: out(o.rstrip()[-2500:])
    return o
def fetch(path, local):
    o = run(f"base64 -w0 {path} 2>/dev/null || base64 {path} | tr -d '\\n'; echo; echo B64END", 120, quiet=True)
    b = o.split('B64END')[0].strip().split('\n')[-1].strip()
    try: open(local, 'wb').write(base64.b64decode(b)); return True
    except Exception as e: out('fetch failed', path, e, b[:200]); return False
def check(local, *a):
    r = subprocess.run([sys.executable, f'{FX}/scancheck.py', local] + list(a), capture_output=True, text=True); return r.stdout.strip() + r.stderr.strip()[-300:]
num = lambda s: int((re.search(r'on (\d+)', s) or re.search(r'(\d+)', '0')).group(1))
AS = "su -s /bin/sh rudolf -c"
if up:
    run("IP=ip; command -v ip >/dev/null || IP='busybox ip'; for i in $(seq 1 30); do $IP -4 addr show eth0 | grep -q 'inet ' && break; sleep 2; done; "
        "$IP -4 addr show eth0 | grep -q 'inet ' || { $IP addr add 10.0.2.15/24 dev eth0; $IP link set eth0 up; }; $IP -4 addr show eth0 | grep inet; $IP route")
    o = run("FX=; for d in /dev/nvme*n1; do blkid $d 2>/dev/null | grep -q SC162FIX && FX=$d; done; mkdir -p /tmp/fx /run/s162x /run/s162l/sane; mount -o ro $FX /tmp/fx; mount -t tmpfs -o exec,mode=755 none /run/s162x; "
        "cp /tmp/fx/rudolf-scan /tmp/fx/firewall /run/s162x/; chmod 755 /run/s162x/*; cp /tmp/fx/libsane.so.1 /run/s162l/; cp /tmp/fx/libsane-airscan.so.1 /run/s162l/sane/; "
        "M=/usr/lib/x86_64-linux-gnu; [ -e $M/libsane.so.1 ] && echo RELEASE-HAS-LIBSANE; mount -t overlay overlay -o lowerdir=/run/s162l:$M $M && echo OVERLAY-OK; "
        "touch /usr/bin/rudolf-scan 2>/dev/null; [ -e /usr/bin/rudolf-scan ] && mount --bind /run/s162x/rudolf-scan /usr/bin/rudolf-scan || { mkdir -p /run/s162b; cp /run/s162x/rudolf-scan /run/s162b/; echo BIN-IN-RUN; }; "
        "mount --bind /run/s162x/firewall /usr/lib/rudolf/firewall; mount --bind /tmp/fx/listeners /usr/share/rudolf/listeners; "
        "/usr/lib/rudolf/firewall on; /usr/lib/rudolf/firewall status; nft list chain inet rudolf wsd; ls -la $M/libsane.so.1 $M/sane/")
    BIN = '/run/s162b/rudolf-scan' if 'BIN-IN-RUN' in o else '/usr/bin/rudolf-scan'
    ok("F. this tree's rudolf-scan, firewall (its wsd chain) and listeners in place; libsane and sane-airscan laid over the library folder", 'OVERLAY-OK' in o and 'udp dport @wsdopen accept' in o and 'libsane-airscan.so.1' in o, o)
    run("mkdir -p /tmp/sc && chown rudolf:rudolf /tmp/sc && chmod 700 /tmp/sc")
    b0 = run('/usr/lib/rudolf/firewall status'); run('/usr/lib/rudolf/firewall wsd open')
    o = run(f"{AS} '{BIN} find'", 60); b1 = run('/usr/lib/rudolf/firewall status')
    ok('1. find, as the desktop\'s user, the firewall on: the eSCL scanner (multicast DNS) and the WSD scanner (its metadata asked through QEMU\'s forwarding), the blocked counter not rising',
       '"id":"escl:http://10.0.2.100:8641/eSCL","name":"QEMU Scanner","model":"QEMU Scanner","via":"network","proto":"eSCL"' in o and '"id":"wsd:http://10.0.2.101:8642/wsd/scan","name":"QEMU WSD Scanner (WSD)"' in o and num(b0) == num(b1), f'{o} {SEEN} {b0} {b1}')
    ID = 'escl:http://10.0.2.100:8641/eSCL'
    o = run(f"{AS} '{BIN} scan {ID} /tmp/sc res=75'", 120); fetch('/tmp/sc/page-1.png', f'{W}/p1.png'); c = check(f'{W}/p1.png', 'dpi=75')
    ok('2. a page in colour at 75 dpi, scanned as the desktop\'s user: its pixels where they belong', '"pages":["page-1.png"]' in o and c.startswith('ok'), f'{o} {c}')
    o = run(f"{AS} '{BIN} scan {ID} /tmp/sc res=100 mode=grey first=2'; {AS} '{BIN} scan {ID} /tmp/sc res=100 mode=bw first=3'", 120)
    fetch('/tmp/sc/page-2.png', f'{W}/p2.png'); fetch('/tmp/sc/page-3.png', f'{W}/p3.png'); c2 = check(f'{W}/p2.png', 'dpi=100', 'mode=grey'); c3 = check(f'{W}/p3.png', 'dpi=100', 'mode=bw')
    ok('3. grey and black and white at 100 dpi: checked', c2.startswith('ok') and c3.startswith('ok'), f'{o} {c2} {c3}')
    ctl('set?adf=loaded&pages=3')
    o = run(f"{AS} '{BIN} scan {ID} /tmp/sc source=adf res=75 first=10' && {AS} '{BIN} save pdf /tmp/sc/three.pdf /tmp/sc/page-10.png /tmp/sc/page-11.png /tmp/sc/page-12.png'; ls -la /tmp/sc", 180)
    fetch('/tmp/sc/three.pdf', f'{W}/three.pdf'); pi = subprocess.run(['pdfinfo', f'{W}/three.pdf'], capture_output=True, text=True).stdout
    cs = [check(f'{W}/three.pdf', f'pdfpage={k}', f'page={k}', 'dpi=75', 'tol=10') for k in (1, 2, 3)]
    ok('4. the feeder\'s three pages into one PDF (rudolf-scan save pdf): 3 pages, each its own picture', re.search(r'Pages:\s+3', pi) is not None and all(x.startswith('ok') for x in cs) and ' rudolf rudolf ' in o, f'{o} {pi} {cs}')
    codes = []
    for st in ('adf=jam', 'adf=open'):
        ctl('set?' + st); o = run(f"{AS} '{BIN} scan {ID} /tmp/sc source=adf first=20'", 60); codes.append((re.search(r'"code":"(\w+)"', o) or re.search('()', '')).group(1))
    ctl('set?adf=loaded&busy=1'); o = run(f"{AS} '{BIN} scan {ID} /tmp/sc first=20'", 90); codes.append((re.search(r'"code":"(\w+)"', o) or re.search('()', '')).group(1)); ctl('set?busy=0')
    ok('5. a paper jam, the cover open, busy: said plainly (jam, open, busy), no page kept', codes == ['jam', 'open', 'busy'] and 'page-20' not in run('ls /tmp/sc'), codes)
    run("echo 'destroy element inet rudolf wsdopen { 3702 }' > /tmp/d.nft; nft -f /tmp/d.nft")
    p0 = SEEN['probes']; b0 = run('/usr/lib/rudolf/firewall status'); o = run(f"{AS} '{BIN} find'", 60); b1 = run('/usr/lib/rudolf/firewall status')
    ok('6. the WSD window closed (as ten seconds after it opens): the ProbeMatches dropped and counted, the WSD scanner not listed; the eSCL one still is', SEEN['probes'] > p0 and 'wsd:' not in o and 'QEMU Scanner' in o and num(b1) > num(b0), f'{o} {b0} {b1}')
    o = run('/usr/lib/rudolf/selftest 2>/dev/null | grep " listeners "', 180)
    ok("7. the self-test's listeners line (the list with rudolf-scan's ports 5353 and 3702) passes", 'PASS listeners' in o, o)
    o = run(f"{AS} '{BIN} scan {ID} /tmp/sc res=300 first=30'; pgrep -x rudolf-scan || echo NONE-RUNNING; free -k | head -2", 180)
    pk = re.search(r'"peakKB":(\d+)', o)
    ok('8. A4 at 300 dpi in colour: at most 80 MB while scanning; nothing running afterwards (nothing while idle)', pk is not None and int(pk.group(1)) < 81920 and 'NONE-RUNNING' in o, o)
    if pk: print(f"::notice title=Scanning in QEMU (Part 34)::{pk.group(1)} KB at most scanning A4 at 300 dpi in colour; nothing running while idle")
vm.stop()
for p in SRV: p.kill()
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if all(R) else 1)
