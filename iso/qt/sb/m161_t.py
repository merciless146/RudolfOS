# Rudolf OS 1.6.1 (docs/plan-1.6.1.md Part 9) in QEMU: Find printers with the firewall on, on the real release (the
# newest stable, or REL-DIR) with this tree's rudolf-print and firewall put in place (M161_FIX: a folder with
# rudolf-print, firewall and listeners, on an extra disk labelled M161FIX). The guest's network card is on QEMU's hub with
# the user network (its address, 10.0.2.15) and a socket netdev to this script, which plays two printers on that link
# at the Ethernet level: one that answers by multicast (to 224.0.0.251:5353, as RFC 6762 says), one that answers by
# unicast to the question's source (as legacy-unicast responders do), both IP TTL 255; and a packet from source port
# 5353 to another port.
#   B. the release as it is (1.6.0): its own find, the firewall on: no printer (the bug found on the owner's Vivobook)
#   F. this tree's rudolf-print and firewall: both printers found, the blocked counter not rising for mDNS; the packet to
#      another port from 5353 dropped and counted; the list of listeners' self-test line still passes
#   python3 m161_t.py REL-DIR    (SB_PARTS, QEMU, QEMU_ACCEL as for p161_t.py; root)
import os, sys, time, re, subprocess, shutil, struct, socket, threading
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); FIX = os.environ.get('M161_FIX', '')
LOG = open('m161.txt', 'w'); R = []; T0 = time.time()
def out(*a):
    s = ' '.join(str(x) for x in a); s = f'[{int(time.time() - T0):5d}s] ' + s; print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:1500] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 3000; DRV = os.path.abspath('m161.img'); MEM = int(os.environ.get('MEM', '3072')); W = os.path.abspath('w161m')
shutil.rmtree(W, ignore_errors=True); os.makedirs(W)

# ---------------------------------------------------------------- the printers, as Ethernet frames on QEMU's hub
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
def rr(nm, t, data): return name(nm) + struct.pack('>HHIH', t, 0x8001, 120, len(data)) + data
def answer(label, a_addr, qid=0):
    inst = label + '._ipp._tcp.local'; host = label.replace(' ', '-').lower() + '.local'
    txt = b''.join(bytes([len(x)]) + x for x in [b'txtvers=1', b'rp=ipp/print', ('ty=' + label + ' Model').encode(), b'note=QEMU lab'])
    recs = [rr('_ipp._tcp.local', 12, name(inst)), rr(inst, 33, struct.pack('>HHH', 0, 0, 631) + name(host)), rr(inst, 16, txt), rr(host, 1, socket.inet_aton(a_addr))]
    return struct.pack('>HHHHHH', qid, 0x8400, 0, 1, 0, 3) + b''.join(recs)
MC_MAC = bytes.fromhex('01005e0000fb'); P1 = bytes.fromhex('525400123477'); P2 = bytes.fromhex('525400123478')
HUB = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); HUB.bind(('127.0.0.1', 0)); HPORT = HUB.getsockname()[1]
QPORT = HPORT + 1   # (QEMU's end: a free port next to ours, checked below)
t = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
while True:
    try: t.bind(('127.0.0.1', QPORT)); t.close(); break
    except OSError: QPORT += 1
SEEN = {'queries': 0, 'sport5353': 0}
def printers():
    while True:
        f, _ = HUB.recvfrom(65536)
        if len(f) < 42 or f[12:14] != b'\x08\x00' or f[23] != 17: continue
        ihl = (f[14] & 15) * 4; ip = f[14:14 + ihl]; u = f[14 + ihl:]; src = socket.inet_ntoa(ip[12:16]); dst = socket.inet_ntoa(ip[16:20])
        sport, dport = struct.unpack('>HH', u[:4]); m = u[8:]
        if dport == 5353: SEEN['mdns'] = SEEN.get('mdns', 0) + 1
        if dst != '224.0.0.251' or dport != 5353 or len(m) < 12 or m[2] & 0x80: continue
        if b'\x04_ipp\x04_tcp' not in m: continue
        SEEN['queries'] += 1; SEEN['sport5353'] += sport == 5353
        gmac = f[6:12]; qid = struct.unpack('>H', m[:2])[0]
        HUB.sendto(frame(MC_MAC, P1, '10.0.2.77', '224.0.0.251', 5353, 5353, answer('QEMU Multicast Printer', '10.0.2.77')), ('127.0.0.1', QPORT))
        HUB.sendto(frame(gmac, P2, '10.0.2.78', src, 5353, sport, answer('QEMU Unicast Printer', '10.0.2.78', qid)), ('127.0.0.1', QPORT))
        GMAC[0] = gmac
GMAC = [None]
threading.Thread(target=printers, daemon=True).start()

# ---------------------------------------------------------------- the drive and the fix disk (as p161_t.py)
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
FIXIMG = os.path.abspath('m161fix.img'); subprocess.run(['truncate', '-s', '64M', FIXIMG], check=True)
subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'M161FIX', '-d', FIX, FIXIMG], check=True)
NET = ['-netdev', 'user,id=u0,restrict=on', '-netdev', 'hubport,id=hp1,hubid=0,netdev=u0',
       '-netdev', f'socket,id=s0,udp=127.0.0.1:{HPORT},localaddr=127.0.0.1:{QPORT}', '-netdev', 'hubport,id=hp2,hubid=0,netdev=s0']
extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf161',
         '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1'] + NET + \
        ['-drive', f'file={FIXIMG},format=raw,if=none,id=fx,readonly=on', '-device', 'nvme,drive=fx,serial=m161fix']
vm = sbvm.VM('m161', PARTS, [], secure=False, mem=MEM, extra=extra)
g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
time.sleep(30); end = time.time() + 1500; up = False
while time.time() < end:
    try:
        if 'ready' in g.sh('echo ready', 8): up = True; break
    except OSError: pass
    time.sleep(4)
ok(f"0. the release ({mv('version')}, {mv('serial')}) boots in QEMU", up)
def run(c, t=90):
    f = os.path.join(W, 'c.sh'); open(f, 'w').write(c + '\n'); g.put(f, '/tmp/c161.sh', '644')
    o = g.sh('sh /tmp/c161.sh 2>&1', t); out('$', c[:200].replace('\n', ' ; ')); out(o.rstrip()[-2500:]); return o
if up:
    run("IP=ip; command -v ip >/dev/null || IP='busybox ip'; for i in $(seq 1 30); do $IP -4 addr show eth0 | grep -q 'inet ' && break; sleep 2; done; "
        "$IP -4 addr show eth0 | grep -q 'inet ' || { $IP addr add 10.0.2.15/24 dev eth0; $IP link set eth0 up; }; $IP -4 addr show eth0 | grep inet; "
        "$IP route; /usr/lib/rudolf/firewall status; [ -f /persist/etc/firewall-off ] && echo FIREWALL-WAS-OFF; /usr/lib/rudolf/firewall on; /usr/lib/rudolf/firewall status")
    # (QEMU's restricted user network gives no default route, which on Wi-Fi carries 1.6.0's question to the group (it sets
    # no interface): a route for 224.0.0.0/4 stands in for it while the release's own find asks; this tree's needs none)
    run("ip route add 224.0.0.0/4 dev eth0 2>/dev/null || busybox ip route add 224.0.0.0/4 dev eth0")
    b0 = run('/usr/lib/rudolf/firewall status'); o = run('/usr/bin/rudolf-print find', 60); b1 = run('/usr/lib/rudolf/firewall status')
    run("ip route del 224.0.0.0/4 dev eth0 2>/dev/null || busybox ip route del 224.0.0.0/4 dev eth0")
    num = lambda s: int((re.search(r'on (\d+)', s) or re.search(r'(\d+)', '0')).group(1))
    ok('B. the release as it is (1.6.0), the firewall on: its find lists no network printer, the answers blocked (the bug)', '"ok":true' in o and 'QEMU' not in o and SEEN['queries'] > 0 and num(b1) > num(b0), f'{o} {SEEN} {b0} {b1}')
    run("FX=; for d in /dev/nvme*n1; do blkid $d 2>/dev/null | grep -q M161FIX && FX=$d; done; mkdir -p /tmp/fx /run/m161x; mount -o ro $FX /tmp/fx; mount -t tmpfs -o exec,mode=755 none /run/m161x; "
        "cp /tmp/fx/rudolf-print /tmp/fx/firewall /run/m161x/; chmod 755 /run/m161x/*; mount --bind /run/m161x/rudolf-print /usr/bin/rudolf-print; mount --bind /run/m161x/firewall /usr/lib/rudolf/firewall; "
        "mount --bind /tmp/fx/listeners /usr/share/rudolf/listeners; /usr/lib/rudolf/firewall on; nft list chain inet rudolf mdns")
    q0 = dict(SEEN); b0 = run('/usr/lib/rudolf/firewall status'); o = run('/usr/bin/rudolf-print find', 60); b1 = run('/usr/lib/rudolf/firewall status')
    ok('F1. this tree\'s find and firewall: both printers listed (the multicast answer and the unicast one), asked from port 5353', '"name":"QEMU Multicast Printer","uri":"ipp://10.0.2.77:631/ipp/print"' in o and '"name":"QEMU Unicast Printer","uri":"ipp://10.0.2.78:631/ipp/print"' in o and SEEN['sport5353'] > q0['sport5353'], f'{o} {SEEN}')
    ok('F2. ...the firewall\'s blocked counter did not rise for mDNS', num(b0) == num(b1), f'{b0} {b1}')
    run('nc -u -l -p 9999 > /tmp/m161.9999 2>/dev/null & echo $! > /tmp/m161.nc; true')
    b0 = run('/usr/lib/rudolf/firewall status')
    if GMAC[0]: HUB.sendto(frame(GMAC[0], P2, '10.0.2.78', '10.0.2.15', 5353, 9999, b'from 5353 to 9999'), ('127.0.0.1', QPORT))
    time.sleep(3); b1 = run('/usr/lib/rudolf/firewall status'); o = run('kill $(cat /tmp/m161.nc) 2>/dev/null; wc -c < /tmp/m161.9999')
    ok('F3. a packet from source port 5353 to another port (9999): dropped, counted', GMAC[0] is not None and num(b1) > num(b0) and o.strip().endswith('0'), f'{b0} {b1} {o}')
    o = run('/usr/lib/rudolf/selftest 2>/dev/null | grep " listeners "', 180)
    ok("F4. the self-test's listeners line (the list with rudolf-print's port 5353) passes", 'PASS listeners' in o, o)
vm.stop()
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if all(R) else 1)
