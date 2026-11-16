# Rudolf OS 1.6.0 in QEMU (docs/plan-1.6.0.md, "Cellular and ARM64", stage A): the cellular work on a release's own
# kernel and layers (the newest beta, as t160), with this tree's scripts put into the running system (the RAM overlay
# takes them; nothing is written to the release's layers). UEFI without Secure Boot, no modem. The files go in on a
# second drive (an ext4 image made here, CELLX): this tree's modem script, udev-run and 90-rudolf.rules; and, for a
# release made before Part 9 (no udev yet: /run/rudolf/devices missing), udev itself from this machine (Ubuntu 24.04's,
# the one the base layer takes), started as rudolf-init starts it, so ModemManager is checked with udev all the same.
#   item 1 (Part 1): ModemManager with udev and no modem: not started by itself; started by hand it runs with udev, finds
#          no modem and logs nothing (no error, no warning); status says so; its plug-ins (QMI, MBIM, QRTR, AT) and FCC
#          unlock scripts are there, the unlock off; stopped again.
#   item 3 (Part 3): the connection beside a virtual Wi-Fi link: mac80211_hwsim's two radios, one an access point
#          (hostapd, dnsmasq) in a far network namespace that holds "the internet" (203.0.113.1); a stand-in cellular
#          link (two QEMU network cards on their own hub, one moved to the far side) that the stand-in ModemManager
#          (qt/mmstub.c, on the guest's system bus) gives as its modem's interface. Connected with this tree's modem
#          service: Wi-Fi carries the traffic, cellular kept in reserve (its route behind), the firewall's cellular set;
#          Wi-Fi taken down: traffic on cellular within seconds (counted on the far side), the notice; Wi-Fi brought
#          back: traffic on Wi-Fi again, the notice.
#   python3 cell60_q.py REL-DIR   (QEMU and SB_PARTS as for t160; root; QEMU_ACCEL=kvm where it can)
import os, sys, time, re, subprocess, shutil, struct, json
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE); ISO = os.path.dirname(QT)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent')
LOG = open('cell60.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[-1500:].replace('\n', ' | ') if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 2000; DRV = os.path.abspath('cell60.img'); XDRV = os.path.abspath('cell60x.img'); MEM = int(os.environ.get('MEM', '3072'))
SYSROOT = os.path.join(ISO, 'sys', 'root')

def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    d = f'{T}/os/system/a'; os.makedirs(d)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs', 'manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{d}/{n}')
    open(f'{d}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n"); open(f'{d}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as dd: shutil.copyfileobj(dd, f)
    os.remove(D)

def extras(path):
    """the second drive: root/ (copied over the running system), libs/ (put in place only where missing), setup.sh"""
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/root'); os.makedirs(f'{T}/libs')
    def put(src, dst, base='root'):
        d = f'{T}/{base}{dst}'; os.makedirs(os.path.dirname(d), exist_ok=True); shutil.copy2(src, d, follow_symlinks=True)
    for f in ('usr/lib/rudolf/modem', 'usr/lib/rudolf/udev-run', 'usr/lib/udev/rules.d/90-rudolf.rules', 'usr/lib/rudolf/firewall', 'usr/lib/rudolf/udhcpc.sh', 'usr/share/rudolf/apn-fixes'): put(os.path.join(SYSROOT, f), '/' + f)
    # (item 3) the stand-in ModemManager built here for the guest (named ModemManager there, so the modem service sees it
    # run), and iproute2's ip (moving an interface into another network namespace) with what it needs
    os.makedirs(f'{T}/bin', exist_ok=True)
    cf = subprocess.run('pkg-config --cflags --libs mm-glib gio-unix-2.0', shell=True, capture_output=True, text=True).stdout.split()
    subprocess.run(['gcc', '-O2', '-w', '-o', f'{T}/bin/ModemManager', os.path.join(QT, 'mmstub.c')] + cf, check=True)
    shutil.copy2('/usr/sbin/ip' if os.path.exists('/usr/sbin/ip') else '/bin/ip', f'{T}/bin/ip2', follow_symlinks=True)
    for x in (f'{T}/bin/ModemManager', f'{T}/bin/ip2'):
        for l in re.findall(r'=> (/\S+)', subprocess.run(['ldd', x], capture_output=True, text=True).stdout): put(l, l, 'libs')
    os.makedirs(f'{T}/udev', exist_ok=True)
    U = subprocess.run("dpkg -L udev 2>/dev/null | grep -E '^/usr/(bin/udevadm|lib/systemd/systemd-udevd|lib/udev/[^/]+|lib/udev/rules\\.d/[^/]+\\.rules)$'", shell=True, capture_output=True, text=True).stdout.split()
    libs = set()
    for x in U:
        if not os.path.isfile(x) or os.path.basename(x) in ('71-seat.rules', '73-seat-late.rules', '80-net-setup-link.rules', '90-vconsole.rules', '99-systemd.rules'): continue
        put(x, x, 'udev')
        if x.endswith('.rules'): continue
        for l in re.findall(r'=> (/\S+)', subprocess.run(['ldd', x], capture_output=True, text=True).stdout): libs.add(l)
    for l in libs: put(l, l, 'libs')
    open(f'{T}/setup.sh', 'w').write(r'''X=$(dirname "$0")
# place DIR all|new: each file of DIR at its path (BusyBox's cp -a makes no folders on the way): all, or only new ones
place() { (cd $X/$1 && find . -type f) | while read -r f; do t="/${f#./}"; [ $2 = new ] && [ -e "$t" ] && continue; mkdir -p "${t%/*}"; cp -p "$X/$1/${f#./}" "$t"; done; }
place root all; place libs new
if [ "$(cat /run/rudolf/devices 2>/dev/null)" = udev ]; then echo "this release has udev"
else place udev new
	for g in disk:6 lp:7 kmem:15 dialout:20 cdrom:24 tape:26 render:990 kvm:991 sgx:992; do grep -q "^${g%%:*}:" /etc/group || echo "${g%%:*}:x:${g#*:}:" >> /etc/group; done
	pkill '^mdev$'; echo > /proc/sys/kernel/hotplug
	/usr/lib/systemd/systemd-udevd --daemon --resolve-names=early 2>>/run/rudolf/udev.log && udevadm control --ping --timeout=5 2>>/run/rudolf/udev.log && echo udev > /run/rudolf/devices
	udevadm trigger --type=subsystems --action=add 2>>/run/rudolf/udev.log; udevadm trigger --type=devices --action=add 2>>/run/rudolf/udev.log; udevadm settle --timeout=15 2>>/run/rudolf/udev.log
	echo "udev from the test machine: $(cat /run/rudolf/devices 2>/dev/null)"; tail -5 /run/rudolf/udev.log 2>/dev/null; fi
''')
    if os.path.exists(path): os.remove(path)
    subprocess.run(['truncate', '-s', '200M', path], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'CELLX', '-d', T, path], check=True); shutil.rmtree(T)

def boot(tag):
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolfcell',
             '-drive', f'file={XDRV},format=raw,if=none,id=cx', '-device', 'nvme,drive=cx,serial=cellx',
             '-netdev', 'hubport,id=c1,hubid=1', '-device', 'e1000,netdev=c1,mac=52:54:00:ce:11:01', '-netdev', 'hubport,id=c2,hubid=1', '-device', 'e1000,netdev=c2,mac=52:54:00:ce:11:02',
             '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1']
    os.makedirs('cell60' + tag, exist_ok=True); open(f'cell60{tag}/vars.fd', 'ab').close()   # (unused without Secure Boot: sbvm copies one from SB_PARTS otherwise)
    vm = sbvm.VM('cell60' + tag, PARTS, [], secure=False, mem=MEM, extra=extra)
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    time.sleep(30); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    return vm, g, up

make_drive(DRV); extras(XDRV)
vm = None
try:
    vm, g, up = boot('a'); sh = lambda c, t=60: g.sh(c, t)
    ok('0. the release starts (' + mv('version') + ')', up, 'not up')
    if not up: raise SystemExit
    time.sleep(20)   # (the start-up's devices settled)
    o = sh("cat /run/rudolf/devices; pgrep '^ModemManager$' >/dev/null && echo MM-RUNNING || echo MM-NOT-RUNNING; ls /dev/cdc-wdm* /dev/wwan* 2>&1 | head -3; cat /run/rudolf/modem.log 2>&1 | tail -5")
    ok('1. no modem, so ModemManager was not started by itself', 'MM-NOT-RUNNING' in o, o)
    # this tree's files, in place of the release's (the RAM overlay), from the second drive; udev if the release has none
    o = sh("mkdir -p /mnt/cx; for d in /dev/nvme*n1; do mount -o ro $d /mnt/cx 2>/dev/null && [ -e /mnt/cx/setup.sh ] && break; umount /mnt/cx 2>/dev/null; done; sh /mnt/cx/setup.sh; cat /run/rudolf/devices", 180)
    out(o)
    ok('1b. devices through udev (' + ('the release\'s' if 'this release has udev' in o else 'from the test machine, as rudolf-init starts it: the release was made before Part 9') + ')', re.search(r'^udev\s*$', o, re.M), o)
    o = sh("ACTION=add MDEV=ttyS0 /usr/lib/rudolf/modem hotplug; sleep 1; pgrep '^ModemManager$' >/dev/null && echo MM-RUNNING || echo MM-NOT-RUNNING")
    ok("...a port that is not a modem's (a serial port) does not start it", 'MM-NOT-RUNNING' in o, o)
    o = sh("command -v ModemManager mmcli; rm -f /run/rudolf/modem.log; /usr/lib/rudolf/modem start; echo rc=$?; sleep 5; pgrep '^ModemManager$' >/dev/null && echo MM-RUNNING; mmcli -L 2>&1; cat /run/rudolf/modem.log", 120)
    ok('2. started by hand: ModemManager runs with udev (not told of ports by Rudolf OS), on the system bus, finds no modem', 'rc=0' in o and 'MM-RUNNING' in o and 'ModemManager started (with udev)' in o and 'No modems were found' in o, o)
    log = sh("sleep 15; cat /run/rudolf/modem.log", 60)
    lines = [l for l in log.splitlines() if l.strip() and 'ModemManager started' not in l and l.strip() != 'cat /run/rudolf/modem.log']
    ok('3. ...and logs nothing at all at its warning level: no error, no warning (no "Invalid rule match")', not [l for l in lines if re.search(r'error|warn|invalid|critical|fail', l, re.I)] and 'Invalid rule match' not in log, log)
    o = sh("/usr/lib/rudolf/modem status")
    m = re.search(r'\{"running".*\}', o)
    try: st = json.loads(m.group(0)) if m else None
    except Exception: st = None
    ok('4. status: running, no modems, no module noted (QEMU has none)', st and st['running'] and st['modems'] == [] and st['notes'] == [], o)
    o = sh("ls /usr/lib/*/ModemManager/libmm-plugin-*.so | wc -l; ls /usr/lib/*/ModemManager/libmm-plugin-*.so | grep -c -E 'quectel|fibocom|sierra|telit|generic|qcom-soc|foxconn'; ls /usr/share/ModemManager/fcc-unlock.available.d | wc -l; ls /etc/ModemManager/fcc-unlock.d 2>/dev/null | wc -l; "
           "ls /usr/lib/*/libqmi-glib.so.* /usr/lib/*/libmbim-glib.so.* /usr/lib/*/libqrtr-glib.so.* | sed 's/\\.so.*//' | sort -u | wc -l; ls /usr/share/mobile-broadband-provider-info/serviceproviders.xml")
    n = [int(x) for x in re.findall(r'^(\d+)$', o, re.M)]
    ok('5. its plug-ins (Quectel, Fibocom, Sierra, Telit, generic AT...), QMI, MBIM and QRTR, the FCC scripts (none turned on) and the operators\' database', len(n) >= 5 and n[0] >= 20 and n[1] >= 6 and n[2] >= 5 and n[3] == 0 and n[4] == 3 and 'serviceproviders.xml' in o, o)
    o = sh("/usr/lib/rudolf/modem stop; sleep 2; pgrep '^ModemManager$' >/dev/null && echo MM-RUNNING || echo MM-NOT-RUNNING; tail -2 /run/rudolf/modem.log")
    ok('6. stopped again (as when the last modem goes)', 'MM-NOT-RUNNING' in o and 'ModemManager stopped' in o, o)
    o = sh("/usr/lib/rudolf/modem status")
    ok('7. ...status: not running, an empty list', '{"running":false,"modems":[],"notes":[]' in o, o)
    # ---- item 3: the connection ----
    o = sh(r"""ifn() { for d in /sys/class/net/*; do [ "$(cat $d/address)" = "$1" ] && echo ${d##*/}; done; }; C=$(ifn 52:54:00:ce:11:01); CF=$(ifn 52:54:00:ce:11:02); echo "C=$C CF=$CF" > /tmp/c3
for i in $C $CF; do [ -s /run/rudolf/udhcpc/$i.pid ] && kill $(cat /run/rudolf/udhcpc/$i.pid); done
(unshare -n sh -c 'echo $$ > /tmp/far.pid; exec sleep 3600' </dev/null >/dev/null 2>&1 &); sleep 1; FP=$(cat /tmp/far.pid); echo "FP=$FP" >> /tmp/c3
modprobe mac80211_hwsim radios=2; sleep 2; W1=; for d in /sys/class/net/*; do [ -e $d/phy80211 ] && W1=${d##*/}; done; W0=$(for d in /sys/class/net/*; do [ -e $d/phy80211 ] && echo ${d##*/}; done | head -1)
echo "W0=$W0 W1=$W1" >> /tmp/c3; iw phy $(cat /sys/class/net/$W1/phy80211/name) set netns $FP; /mnt/cx/bin/ip2 link set $CF netns $FP || ip link set $CF netns $FP
cat /tmp/c3; nsenter -t $FP -n ip link 2>&1 | grep -o '^[0-9]*: [^:]*'""", 60)
    out(o); env3 = dict(re.findall(r'(\w+)=(\S*)', ' '.join(l for l in o.splitlines() if l.startswith(('C=', 'FP=', 'W0=')))))
    C, CF, W0, W1 = env3.get('C', ''), env3.get('CF', ''), env3.get('W0', ''), env3.get('W1', '')
    far = lambda c: f"nsenter -t $(cat /tmp/far.pid) -n sh -c '{c}'"
    o = sh(far(f"ip link set lo up; ip addr add 203.0.113.1/32 dev lo; ip link set {CF} up; ip addr add 10.64.12.33/30 dev {CF}; ip link set {W1} up; ip addr add 192.168.50.1/24 dev {W1}; "
               f"printf \"interface={W1}\\ndriver=nl80211\\nssid=cell60\\nhw_mode=g\\nchannel=6\\n\" > /tmp/far-ap.conf; hostapd -B -P /tmp/far-ap.pid /tmp/far-ap.conf; "
               f"dnsmasq --conf-file=/dev/null --interface={W1} --bind-interfaces --dhcp-range=192.168.50.10,192.168.50.50,1h --dhcp-option=3,192.168.50.1 --port=0 --pid-file=/tmp/far-dns.pid --dhcp-leasefile=/tmp/far.leases; sleep 2; ip addr; cat /tmp/far-ap.pid"), 60)
    out(o)
    ok('8. the far side: the access point on the second Wi-Fi radio and the far end of the cellular link, in their own network namespace', bool(C and CF and W0 and W1) and '203.0.113.1' in o and '10.64.12.33' in o, o)
    o = sh(f"printf 'cell60\\n\\nopen\\n' > /tmp/wreq; /usr/lib/rudolf/wifi.sh connect {W0} /tmp/wreq; for i in $(seq 60); do ip route | grep -q 'default .*dev {W0}' && break; sleep 1; done; ip route; ping -c1 -W3 203.0.113.1 >/dev/null && echo REACH", 120)
    ok('9. Rudolf OS joins the virtual Wi-Fi (wifi.sh): its address and default route (metric 600), the internet reached', re.search(rf'default via 192\.168\.50\.1 dev {W0} .*metric 600', o) and 'REACH' in o, o)
    o = sh(f"/usr/lib/rudolf/firewall on; printf 'add 0 iccid=8901260123456789012 iface={C} ip4=10.64.12.34/30 gw4=10.64.12.33 dns=10.177.0.34 enabled=1\\n' > /tmp/mminit; mkfifo /tmp/mmctl; "
           "(DBUS_SYSTEM_BUS_ADDRESS=unix:path=/run/dbus/system_bus_socket MMSTUB_TEST_MACHINE=1 setsid /mnt/cx/bin/ModemManager --ctl /tmp/mmctl --log /tmp/mmcalls --init /tmp/mminit </dev/null >/tmp/mmstub.out 2>&1 &); sleep 3; "
           "mmcli -L; /usr/lib/rudolf/modem connect 0; ip route; cat /tmp/mmcalls", 120)
    ok('10. the stand-in ModemManager on the guest\'s bus; Connect: its link set up (10.64.12.34), its route kept behind Wi-Fi\'s (700)', 'Connected' in o and re.search(rf'default via 10\.64\.12\.33 dev {C} .*metric 700', o) and 'Connect apn=fast.t-mobile.com' in o, o)
    o = sh(f"ip route get 203.0.113.1; ping -c2 -W3 203.0.113.1 >/dev/null && echo REACH; nft list set inet rudolf cellular", 60)
    ok('11. Wi-Fi works: the traffic on Wi-Fi, cellular unused; the cellular link in the firewall\'s set', re.search(rf'dev {W0}', o.split('\n', 2)[-1] if o else '') and 'REACH' in o and f'"{C}"' in o, o)
    o = sh(f"(setsid sh /usr/lib/rudolf/modem watch </dev/null >/dev/null 2>&1 &); sleep 4; R0=$(" + far(f"cat /sys/class/net/{CF}/statistics/rx_packets") + f"); "
           f"T0=$(date +%s%N); ip link set {W0} down; for i in $(seq 50); do ip route get 203.0.113.1 | grep -q 'dev {C}' && break; sleep 0.2; done; T1=$(date +%s%N); echo MS=$(( (T1 - T0) / 1000000 )); "
           f"ping -c3 -W3 203.0.113.1 >/dev/null && echo REACH; R1=$(" + far(f"cat /sys/class/net/{CF}/statistics/rx_packets") + "); echo FARRX=$((R1 - R0)); sleep 4; cat /run/rudolf/cell-notes", 120)
    ms = re.search(r'MS=(\d+)', o); ms = int(ms.group(1)) if ms else 99999; frx = re.search(r'FARRX=(\d+)', o); frx = int(frx.group(1)) if frx else 0
    ok(f'12. Wi-Fi taken down: the traffic on cellular within seconds ({ms} ms), the internet reached through it (counted on the far end)', ms < 5000 and 'REACH' in o and frx >= 3, o)
    ok('...and the notice (both desktops: mobile/notes): mobile data carries the traffic now', 'Using mobile data|The Wi-Fi connection is gone' in o, o)
    o = sh(f"ip link set {W0} up; for i in $(seq 90); do ip route get 203.0.113.1 | grep -q 'dev {W0}' && break; sleep 1; done; ip route get 203.0.113.1; ping -c1 -W3 203.0.113.1 >/dev/null && echo REACH; sleep 4; tail -2 /run/rudolf/cell-notes; ip route", 150)
    ok('13. Wi-Fi brought back: the traffic on Wi-Fi again, cellular back in reserve, the notice', re.search(rf'203\.0\.113\.1 via 192\.168\.50\.1 dev {W0}', o) and 'REACH' in o and 'Back on Wi-Fi' in o and re.search(rf'dev {C} .*metric 700', o), o)
    o = sh("/usr/lib/rudolf/modem disconnect 0; ip route; nft list set inet rudolf cellular; kill $(cat /run/rudolf/modem-watch.pid) 2>/dev/null; pkill '^ModemManager$'", 60)
    ok('14. Disconnect: the cellular route and firewall entry gone', 'Disconnected' in o and f'dev {C}' not in o and f'"{C}"' not in o, o)
finally:
    try: vm and vm.stop()
    except Exception: pass
    for f in (DRV, XDRV):
        try: os.remove(f)
        except OSError: pass
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
