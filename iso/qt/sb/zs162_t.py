# Rudolf OS 1.6.2 (docs/plan-1.6.2.md Part 31, item 2): the system's images with each compression, measured in QEMU on one
# kind of disk and one desktop engine. Each variant (a folder with the four layers made by zs162_layers.sh) is installed
# as slot a on its own drive with the test kernel (testkernel.sh: the release's, with zstd and lz4) and started STARTS
# times (and once before, not counted: the first-time setup, on Chromium, then the engine chosen), the variants in turn
# and the order swapped each round (runners differ over time). Each start is cold: a new virtual machine, the host's page
# cache dropped and the drive read through QEMU without its own cache settings (the host cache dropped before each start). Measured each start:
#   ready    the health log's "the desktop was ready N s after start-up" (Settings › System health › Last starts)
#   up       the start's desktop-up mark (uptime, s) and when the start-up was handed the desktop (bootok)
#   memory   MemTotal - MemAvailable after 30 s idle (MiB)
#   chromium Rudolf's Chromium started cold (the guest's caches dropped), headless, until it has drawn a page (s)
#   oracle   the same with Oracle's own program and flags (/run/rudolf/oracle-chrome: its shell and extension; a fresh
#            profile): how Oracle's first window starts on Native and Lite, where Chromium is not running yet. (On the
#            Chromium engine the desktop is that Chromium, so its files are partly in memory already: as there.)
# and on the first counted start of each variant, the decompression: each layer mounted again (loop), the guest's caches
# dropped, every file read; the time, and the processors' system time (/proc/stat: system, irq, softirq).
#   python3 zs162_t.py LAYERS-DIR KERNEL-DIR DISK ENGINE     (root; SB_PARTS, QEMU as for kt161_t.py; QEMU_ACCEL=kvm)
# LAYERS-DIR holds one folder per variant (its name: xz-1m, zstd19-256k, ...); manifest.txt and manifest.sig of the release
# beside them. DISK: nvme (virtio NVMe, as fast as the runner's disk), emmc (NVMe limited by QEMU to 150 MB/s and 4000
# reads/s, an eMMC's), usb (a USB stick: usb-storage on xHCI, 30 MB/s, 1500 reads/s). ENGINE: chromium, native, lite.
# Environment: STARTS (5), VARIANTS (a comma list; default: every folder), MEM (MiB, 3400), DMB (the data partition, 4000).
# Writes zs162-DISK-ENGINE.json (one line a start) and .txt (the log). Nothing is published.
import os, sys, time, re, subprocess, shutil, struct, json
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE); ISO = os.path.dirname(QT)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
LD, KER, DISK, ENGINE = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2]), sys.argv[3], sys.argv[4]
assert DISK in ('nvme', 'emmc', 'usb') and ENGINE in ('chromium', 'native', 'lite'), 'DISK: nvme emmc usb; ENGINE: chromium native lite'
STARTS = int(os.environ.get('STARTS', '5')); MEM = int(os.environ.get('MEM', '3400')); DMB = int(os.environ.get('DMB', '4000'))
PARTS = os.environ.get('SB_PARTS', '/nonexistent'); TAG = f'{DISK}-{ENGINE}'
VARS = [v for v in (os.environ.get('VARIANTS') or ','.join(sorted(d for d in os.listdir(LD) if os.path.isfile(f'{LD}/{d}/base.sfs')))).split(',') if v]
LOG = open(f'zs162-{TAG}.txt', 'w'); JS = open(f'zs162-{TAG}.json', 'w'); T0 = time.time()
def out(*a, quiet=False):
    s = f'[{int(time.time() - T0):5d}s] ' + ' '.join(str(x) for x in a); LOG.write(s + '\n'); LOG.flush()
    if not quiet: print(s[:700], flush=True)
man = open(f'{LD}/manifest.txt').read(); mv = lambda k: (re.search(rf'^{k} (.*)$', man, re.M) or re.search('()', '')).group(1)
W = os.path.abspath(f'wzs162-{TAG}'); shutil.rmtree(W, ignore_errors=True); os.makedirs(W)
ENGFILE = {'chromium': None, 'native': 'native', 'lite': 'webkit'}[ENGINE]

def make_drive(v):
    path = f'{W}/{v}.img'; start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = f'{W}/{v}.tree'; shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    d = f'{T}/os/system/a'; os.makedirs(d)
    shutil.copy(f'{KER}/vmlinuz', f'{d}/vmlinuz')
    for n in ('base', 'chromium', 'desktop', 'modules'): shutil.copy(f'{LD}/{v}/{n}.sfs', f'{d}/{n}.sfs')
    for n in ('manifest.txt', 'manifest.sig'): shutil.copy(f'{LD}/{n}', f'{d}/{n}')
    open(f'{d}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes zs162 {v}\n")
    open(f'{d}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as dd: shutil.copyfileobj(dd, f)
    os.remove(D); return path

def drive_args(path):
    d = f'file={path},format=raw,if=none,id=nv'
    if DISK == 'emmc': d += ',throttling.bps-read=157286400,throttling.iops-read=4000,throttling.bps-write=52428800'
    if DISK == 'usb': d += ',throttling.bps-read=31457280,throttling.iops-read=1500,throttling.bps-write=10485760'
    dev = ['-device', 'nvme,drive=nv,serial=zs162'] if DISK != 'usb' else ['-device', 'qemu-xhci,id=xh', '-device', 'usb-storage,drive=nv,bus=xh.0']
    return ['-drive', d] + dev

INIT_JS = "(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};return 1})()"
FAILS = [0]
def boot(v, tag, kernel=None, append=''):
    subprocess.run('sync; echo 3 > /proc/sys/vm/drop_caches', shell=True)   # (the host's copy of the drive: cold)
    extra = drive_args(f'{W}/{v}.img') + ['-kernel', kernel or f'{KER}/vmlinuz', '-append', f'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1 {append}'.strip()]
    name = f'zs{tag}'; os.makedirs(name, exist_ok=True); open(f'{name}/vars.fd', 'ab').close()
    vm = sbvm.VM(name, PARTS, [], secure=False, mem=MEM, extra=extra); t = time.time()
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; seen = g.s.read(0.3)
    up = False; end = time.time() + (900 if FAILS[0] < 2 else 420)
    while time.time() < end:
        try:
            o = g.sh('echo "UP$((40+2))"', 8); seen = (seen + o)[-12000:]
            if 'UP42' in o: up = True; break
        except OSError: pass
        if vm.p.poll() is not None: break   # (QEMU ended)
        time.sleep(2)
    out(f'start {tag} ({v}): serial shell {"up" if up else "NOT up"} after {time.time() - t:.0f} s')
    if not up:
        FAILS[0] += 1
        try: ql = open(os.path.join(vm.dir, 'qemu.log')).read()[-2000:]
        except OSError: ql = ''
        out('  QEMU:', 'running' if vm.p.poll() is None else f'ended ({vm.p.returncode})', '| its log:', ql.replace('\n', ' / ') or '(empty)')
        out('  the serial line, last:', seen[-5000:].replace('\r', '').replace('\n', ' / '))
        try: vm.shot('notup'); out('  screenshot taken')
        except Exception as e: out('  no screenshot:', e)
    else: FAILS[0] = 0
    return vm, g, up
def run(g, c, t=60):
    f = os.path.join(W, 'c.sh'); open(f, 'w').write(c + '\n'); g.put(f, '/tmp/zs.sh', '644')
    o = g.sh('sh /tmp/zs.sh 2>&1', t); out('$', c[:200].replace('\n', ' ; '), quiet=True); out(o.rstrip()[-4000:], quiet=True); return o
def stop(vm, g):
    try: g.sh('sync', 30)
    except Exception: pass
    vm.stop(); time.sleep(2)

WAIT = r'''
i=0; while [ $i -lt 600 ] && [ ! -e /run/rudolf/bootok ]; do sleep 1; i=$((i+1)); done
[ -e /run/rudolf/bootok ] && echo "BOOTOK $(cut -d' ' -f1 /proc/uptime)" || echo NO-DESKTOP
# the health log's line (the page or the native desktop writes it a few seconds after it is up): a new one, this start's
i=0; while [ $i -lt 120 ] && [ "$(grep -c 'was ready' /persist/var/log/health.log 2>/dev/null)" -le %d ]; do sleep 1; i=$((i+1)); done
echo "READYN $(grep -c 'was ready' /persist/var/log/health.log 2>/dev/null)"
grep -o 'the desktop was ready [0-9.]* s[^"]*' /persist/var/log/health.log 2>/dev/null | tail -1 | sed 's/^/HL /'
echo "UP $(cat /persist/var/log/starts/0/desktop-up 2>/dev/null)"
echo "EN $(cat /run/rudolf/engine-choice 2>/dev/null) -> $(cat /run/rudolf/desktop-engine 2>/dev/null)"
for n in base chromium desktop modules; do echo "SQ $n $(od -An -tu2 -j20 -N2 /persist/os/system/a/$n.sfs | tr -d ' ')"; done
'''
MEASURE = r'''
sleep 30; awk '/^MemTotal:/{t=$2} /^MemAvailable:/{a=$2} END{print "MEM", int((t-a)/1024)}' /proc/meminfo
sync; echo 3 > /proc/sys/vm/drop_caches; rm -rf /tmp/zsp; A=$(cut -d' ' -f1 /proc/uptime)
N=$(timeout 180 /opt/chromium/chrome --headless=new --no-sandbox --disable-gpu --no-first-run --user-data-dir=/tmp/zsp --dump-dom 'data:text/html,<p>zs-ok</p>' 2>/dev/null | grep -c zs-ok)
echo "CHROMIUM $A $(cut -d' ' -f1 /proc/uptime) $N"; rm -rf /tmp/zsp
OF=bare; : > /tmp/zsoa
if [ -s /run/rudolf/oracle-chrome ]; then OF=oracle; tail -n +3 /run/rudolf/oracle-chrome | grep -v -e '^--user-data-dir=' -e '^$' > /tmp/zsoa; fi
sync; echo 3 > /proc/sys/vm/drop_caches; rm -rf /tmp/zso; A=$(cut -d' ' -f1 /proc/uptime)
N=$(timeout 180 sh -c "exec /opt/chromium/chrome $(tr '\n' ' ' < /tmp/zsoa) --headless=new --no-sandbox --user-data-dir=/tmp/zso --dump-dom 'data:text/html,<p>zs-ok</p>'" 2>/dev/null | grep -c zs-ok)
echo "ORACLE $A $(cut -d' ' -f1 /proc/uptime) $N $OF $(wc -l < /tmp/zsoa)"; rm -rf /tmp/zso
'''
DECOMP = r'''
mkdir -p /tmp/zsm
for n in base chromium desktop modules; do
	sync; echo 3 > /proc/sys/vm/drop_caches
	mount -t squashfs -o ro,loop /persist/os/system/a/$n.sfs /tmp/zsm || { echo "DX $n mount-failed"; continue; }
	S0=$(awk '/^cpu /{print $4+$7+$8}' /proc/stat); U0=$(cut -d' ' -f1 /proc/uptime)
	find /tmp/zsm -type f -exec cat {} + > /dev/null 2>&1
	S1=$(awk '/^cpu /{print $4+$7+$8}' /proc/stat); U1=$(cut -d' ' -f1 /proc/uptime)
	B=$(du -sb /tmp/zsm 2>/dev/null | cut -f1); umount /tmp/zsm
	echo "DX $n $U0 $U1 $S0 $S1 $B"
done
'''
def num(rx, o, f=float):
    m = re.search(rx, o, re.M); return f(m.group(1)) if m else None

drives = {}
for v in VARS: drives[v] = make_drive(v); out('drive for', v, os.path.getsize(f'{LD}/{v}/base.sfs'))
seen = {v: 0 for v in VARS}; ALL = []; CONTROL = []
try:
    for i in range(STARTS + 1):
        for v in (VARS if i % 2 else VARS[::-1]):
            vm, g, up = boot(v, f'{i}{v}')
            if not up:
                res = {'variant': v, 'start': i, 'error': 'no serial shell'}; JS.write(json.dumps(res) + '\n'); stop(vm, g)
                if os.environ.get('KERNEL2') and not CONTROL:
                    CONTROL.append(1); vm2, g2, up2 = boot(v, f'c{i}{v}', os.environ['KERNEL2'], 'console=ttyS0,115200 loglevel=6')   # (its messages on the serial line)
                    out(f'control: the same drive with the release\'s own kernel: serial shell {"up" if up2 else "NOT up"}'); stop(vm2, g2)
                if FAILS[0] >= 4: out('four starts in a row without a serial shell: stopped (see the lines above)'); raise SystemExit(3)
                continue
            o = run(g, WAIT % seen[v], 800)
            seen[v] = num(r'^READYN (\d+)', o, int) or seen[v]
            if i == 0:   # the first start: the first-time setup on Chromium; then the engine chosen for the next starts
                r = None
                for _ in range(20):
                    r = g.js(INIT_JS, 60)
                    if r == 1: break
                    time.sleep(10)
                eng = f'echo {ENGFILE} > /persist/etc/rudolf-desktop-engine; ' if ENGFILE else 'rm -f /persist/etc/rudolf-desktop-engine; '
                out('setup:', r, run(g, f'sleep 20; {eng}sync; cat /persist/etc/rudolf-desktop-engine 2>/dev/null; echo set', 60).strip()[-200:])
                stop(vm, g); continue
            m = run(g, MEASURE, 600)
            res = {'variant': v, 'start': i, 'disk': DISK, 'engine': ENGINE,
                   'ready_s': num(r'^HL the desktop was ready ([0-9.]+) s', o), 'ready_line': num(r'^HL (.*)$', o, str),
                   'bootok_s': num(r'^BOOTOK ([0-9.]+)', o), 'up_mark': num(r'^UP ([0-9.]+)', o), 'engine_seen': num(r'^EN (.*)$', o, str),
                   'comp': dict(re.findall(r'^SQ (\w+) (\d+)', o, re.M)), 'mem_idle_mib': num(r'^MEM (\d+)', m, int)}
            c = re.search(r'^CHROMIUM ([0-9.]+) ([0-9.]+) (\d+)', m, re.M)
            if c: res['chromium_s'] = round(float(c.group(2)) - float(c.group(1)), 2); res['chromium_ok'] = c.group(3) != '0'
            c = re.search(r'^ORACLE ([0-9.]+) ([0-9.]+) (\d+) (\w+) (\d+)', m, re.M)
            if c: res['oracle_s'] = round(float(c.group(2)) - float(c.group(1)), 2); res['oracle_ok'] = c.group(3) != '0'; res['oracle_flags'] = f'{c.group(4)} ({c.group(5)} arguments)'
            if i == 1:
                d = run(g, DECOMP, 900); dx = {}
                for n, u0, u1, s0, s1, b in re.findall(r'^DX (\w+) ([0-9.]+) ([0-9.]+) (\d+) (\d+) (\d+)', d, re.M):
                    dx[n] = {'read_s': round(float(u1) - float(u0), 2), 'sys_cpu_s': round((int(s1) - int(s0)) / 100, 2), 'bytes': int(b)}
                res['decomp'] = dx
            out('RESULT', json.dumps(res)); JS.write(json.dumps(res) + '\n'); JS.flush(); ALL.append(res)
            stop(vm, g)
finally:
    JS.close(); out('done')
    print('==== SUMMARY', TAG)
    med = lambda x: sorted(x)[len(x) // 2] if x else None
    for v in VARS:
        R = [r for r in ALL if r['variant'] == v]
        def m(k): return med([r[k] for r in R if isinstance(r.get(k), (int, float))])
        dx = next((r['decomp'] for r in R if r.get('decomp')), {})
        print(f"{v}: ready {m('ready_s')} s ({' '.join(str(r.get('ready_s')) for r in R)}), chromium {m('chromium_s')} s, oracle {m('oracle_s')} s, "
              f"memory {m('mem_idle_mib')} MiB, decompression {sum(x['sys_cpu_s'] for x in dx.values()):.1f} s CPU / {sum(x['read_s'] for x in dx.values()):.1f} s read, engine {R[0].get('engine_seen') if R else None}")
