# Rudolf OS 1.6.1 kernel tuning in QEMU (docs/plan-1.6.1.md Part 12): measurements on a release, as installed (slot a on an
# NVMe drive with RUDOLFDATA, UEFI with Secure Boot off, 3.3 GB as the owner's Vivobook), nothing changed in the guest
# but what a phase says. Tests only. The numbers are written to kt161.txt and, as JSON lines, to kt161.json.
#   python3 kt161_t.py REL-DIR PHASE        (root; SB_PARTS and QEMU as for t160_t.py; QEMU_ACCEL=kvm where it can)
# PHASE:
#   startup   STARTS starts (the first, the first-time setup's, not counted): where the time goes before the desktop's first
#             heartbeat (the kernel, the start-up image, udev, rudolf-init, the X server, the desktop); one more start with
#             initcall_debug for the kernel's own slowest steps
#   latency   one start per PREEMPTS mode (preempt= at start-up): wake-up latency of an ordinary thread and the desktop
#             page's input-to-frame time, idle, with a busy loop on every processor, and during a large file copy
#   memory    MEMRUNS (swappiness:zram-percent:compressor ...): tabs, a game and a call at once on 3.3 GB, for each; the
#             memory stalls (PSI), the game's slow frames, the call's late periods, the memory guard's steps
#   power     /usr/lib/rudolf/power of this tree in the guest (QEMU has no CPU frequency driver): the fallback
# Environment: KERNEL (a folder with vmlinuz and modules.sfs to start instead of the release's), BASE (a base.sfs to use
# instead), APPEND (more start-up options), ENGINE (standard|native), MEM (MiB, 3400), STARTS (4), TAG (a name for the results).
import os, sys, time, re, subprocess, shutil, struct, json, base64, datetime
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE); ISO = os.path.dirname(QT)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PHASE = sys.argv[2]
PARTS = os.environ.get('SB_PARTS', '/nonexistent'); TAG = os.environ.get('TAG', PHASE)
KERNEL = os.environ.get('KERNEL', ''); BASE = os.environ.get('BASE', ''); APPEND = os.environ.get('APPEND', '')
ENGINE = os.environ.get('ENGINE', 'standard'); MEM = int(os.environ.get('MEM', '3400'))
LOG = open(f'kt161-{TAG}.txt', 'w'); JS = open(f'kt161-{TAG}.json', 'w'); T0 = time.time()
def out(*a):
    s = ' '.join(str(x) for x in a); s = f'[{int(time.time() - T0):5d}s] ' + s; print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def res(**kw):
    kw['tag'] = TAG; JS.write(json.dumps(kw) + '\n'); JS.flush(); out('RESULT', json.dumps(kw))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 6000; DRV = os.path.abspath(f'kt161-{TAG}.img'); W = os.path.abspath(f'wkt161-{TAG}')
shutil.rmtree(W, ignore_errors=True); os.makedirs(W)
subprocess.run(['gcc', '-O2', '-static', '-o', f'{W}/kload', os.path.join(HERE, 'k161', 'kload.c')], check=True)
VMLINUZ = f'{KERNEL}/vmlinuz' if KERNEL else f'{REL}/vmlinuz'

def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    d = f'{T}/os/system/a'; os.makedirs(d)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs'):
        src = f'{REL}/{n}'
        if n == 'modules.sfs' and KERNEL: src = f'{KERNEL}/modules.sfs'
        if n == 'vmlinuz' and KERNEL: src = f'{KERNEL}/vmlinuz'
        if n == 'base.sfs' and BASE: src = BASE
        shutil.copy(src, f'{d}/{n}')
    for n in ('manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{d}/{n}')
    open(f'{d}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n"); open(f'{d}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    if ENGINE == 'native': os.makedirs(f'{T}/etc', exist_ok=True); open(f'{T}/etc/rudolf-desktop-engine', 'w').write('native\n')
    K = f'{T}/k161'; os.makedirs(K); shutil.copy(f'{W}/kload', f'{K}/kload')
    shutil.copy(os.path.join(ISO, 'sys', 'root', 'usr', 'lib', 'rudolf', 'power'), f'{K}/power')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)

INIT_JS = "(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};return 1})()"
def boot(tag, append=''):
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf161',
             '-kernel', VMLINUZ, '-append', f'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1 {APPEND} {append}'.strip()]
    vm = sbvm.VM('kt161' + tag, PARTS, [], secure=False, mem=MEM, extra=extra); t = time.time()
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    up = False; end = time.time() + 600
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(2)
    out(f'start {tag}: serial shell {"up" if up else "NOT up"} after {time.time() - t:.0f} s')
    return vm, g, up
def run(g, c, t=60):
    f = os.path.join(W, 'c161.sh'); open(f, 'w').write(c + '\n'); g.put(f, '/tmp/c161.sh', '644')
    o = g.sh('sh /tmp/c161.sh 2>&1', t); out('$', c[:160].replace('\n', ' ; ')); out(o.rstrip()[-2500:]); return o
def wait_desktop(g, secs=420):
    o = run(g, f"i=0; while [ $i -lt {secs} ] && [ ! -e /run/rudolf/bootok ]; do sleep 1; i=$((i+1)); done; [ -e /run/rudolf/bootok ] && echo DESKTOP-UP || echo NO-DESKTOP", secs + 30)
    return 'DESKTOP-UP' in o
def stop(vm, g):
    try: g.sh('sync', 30)
    except Exception: pass
    vm.stop(); time.sleep(2)

# ---- start-up: what happened when, as uptime seconds
TIMES = r'''
U=$(cut -d' ' -f1 /proc/uptime); touch /tmp/k161now
echo "NOW $U $(stat -c %Y /tmp/k161now) $(date +%s)"
for f in /etc/rudolf/initramfs.log /run/rudolf/persistent /run/rudolf/devices /persist/var/log/starts/0/start.txt /run/rudolf/gpu /run/rudolf/session.env /tmp/.X11-unix/X0 /run/rudolf/bootok /run/rudolf/desktop-engine; do
	[ -e $f ] && echo "MT $(stat -c %Y $f) $f"; done
tail -1 /persist/var/log/shutdown.log | sed 's/^/SL /'
H=$(getconf CLK_TCK 2>/dev/null || echo 100); echo "HZ $H"
for p in /proc/[0-9]*; do [ -s $p/cmdline ] || continue; c=$(cat $p/comm 2>/dev/null); s=$(sed 's/.*) //' $p/stat 2>/dev/null | cut -d' ' -f20); [ -n "$s" ] && echo "PS $s $c"; done | sort -n -k2 | awk '!seen[$3]++'
dmesg | grep -E 'Run /init|Freeing unused kernel image|rtc_cmos.*setting system clock|amdgpu|bochs|drm\]' | head -12 | sed 's/^/DM /'
cat /persist/var/log/starts/0/desktop-up 2>/dev/null | sed 's/^/UP /'
grep -h 'was ready' /persist/var/log/health*.log 2>/dev/null | tail -2 | sed 's/^/HL /'
cat /run/rudolf/udev.log 2>/dev/null | tail -3 | sed 's/^/UL /'
cat /etc/rudolf/initramfs.log 2>/dev/null | tail -12 | sed 's/^/IL /'
'''
def parse_times(o):
    r = {}; up = wall = None; hz = 100; ps = {}
    for l in o.splitlines():
        p = l.split()
        if not p: continue
        if p[0] == 'NOW' and len(p) >= 3: up = float(p[1]); wall = float(p[2])
        elif p[0] == 'HZ' and len(p) > 1 and p[1].isdigit(): hz = int(p[1])
        elif p[0] == 'MT' and len(p) >= 3 and up is not None: r['file ' + p[2]] = round(up - (wall - float(p[1])), 1)
        elif p[0] == 'PS' and len(p) >= 3 and p[1].isdigit(): ps.setdefault(p[2], round(int(p[1]) / hz, 2))
        elif p[0] == 'DM' and 'Run /init' in l:
            m = re.search(r'\[\s*([0-9.]+)\]', l); r['kernel: /init started'] = float(m.group(1)) if m else None
        elif p[0] == 'SL': m = re.search(r'uptime=([0-9.]+)', l); r['rudolf-init: data partition (after udev)'] = float(m.group(1)) if m else None
        elif p[0] == 'UP': r['session: desktop-up mark'] = float(p[1])
    for k in ('systemd-udevd', 'rudolfd', 'Xorg', 'Xvfb', 'rudolf-display', 'chrome', 'rudolf-native', 'rudolf-memguard', 'pulseaudio', 'wpa_supplicant', 'dbus-daemon'):
        if k in ps: r['process ' + k] = ps[k]
    return r

def phase_startup():
    n = int(os.environ.get('STARTS', '4')); all_t = []
    for i in range(n + 1):
        vm, g, up = boot(f's{i}')
        if not up: stop(vm, g); continue
        d = wait_desktop(g)
        if i == 0:   # the first start: the first-time setup, then started again as an installed computer is
            g.js(INIT_JS, 60); stop(vm, g); continue
        run(g, 'sleep 20', 40); o = run(g, TIMES, 120); t = parse_times(o)
        res(kind='startup', start=i, desktop=d, times=t); all_t.append(t); stop(vm, g)
    if all_t:
        keys = sorted({k for t in all_t for k in t}, key=lambda k: sorted(x[k] for x in all_t if x.get(k) is not None)[0] if any(x.get(k) is not None for x in all_t) else 999)
        out('---- start-up, median of', len(all_t), 'starts (uptime, s)')
        for k in keys:
            v = sorted(x[k] for x in all_t if x.get(k) is not None)
            if v: out(f'{v[len(v) // 2]:8.1f}  {k}   ({" ".join(str(x) for x in v)})')
    # the kernel's own steps (initcall_debug: each initcall's time; printk's own cost makes the total a little longer)
    vm, g, up = boot('si', 'initcall_debug log_buf_len=8M ignore_loglevel')
    if up:
        wait_desktop(g)
        o = run(g, r"dmesg | grep -E 'Run /init'; dmesg | sed -n 's/.*initcall \([^ ]*\) returned [-0-9]* after \([0-9]*\) usecs.*/\2 \1/p' | sort -rn | head -30; "
                   r"echo TOTAL $(dmesg | sed -n 's/.*returned [-0-9]* after \([0-9]*\) usecs.*/\1/p' | awk '{s+=$1} END{print s}'); "
                   r"dmesg | grep -iE 'async|deferred|waiting|timed out|firmware' | head -20", 120)
        top = [(int(a), b) for a, b in re.findall(r'^(\d+) (\S+)$', o, re.M)]
        tot = re.search(r'TOTAL (\d+)', o); ri = re.search(r'\[\s*([0-9.]+)\] Run /init', o)
        res(kind='initcalls', total_us=int(tot.group(1)) if tot else None, run_init=float(ri.group(1)) if ri else None, top=top[:30])
        stop(vm, g)

# ---- responsiveness: wake-up latency and the page's input-to-frame time
PAGE_JS = ("(()=>{window.__k={lat:[],q:[],fr:[]};let last=0;const raf=t=>{if(last&&__k.on)__k.fr.push(t-last);last=t;requestAnimationFrame(raf)};"
           "requestAnimationFrame(raf);addEventListener('keydown',e=>{if(!__k.on)return;const t0=e.timeStamp;__k.q.push(performance.now()-t0);"
           "requestAnimationFrame(()=>__k.lat.push(performance.now()-t0))},true);return 1})()")
def stats(v):
    v = sorted(v)
    if not v: return {}
    q = lambda p: round(v[min(len(v) - 1, int(p * (len(v) - 1) + .5))], 1)
    return {'n': len(v), 'p50': q(.5), 'p95': q(.95), 'p99': q(.99), 'max': round(v[-1], 1)}
def page_input(g, secs=15):
    g.js("window.__k&&(__k.lat=[],__k.q=[],__k.fr=[],__k.on=1)", 30)
    end = time.time() + secs
    while time.time() < end: g.q.key('shift'); time.sleep(0.15)
    time.sleep(1); r = g.js("(()=>{__k.on=0;return JSON.stringify({lat:__k.lat,q:__k.q,fr:__k.fr})})()", 60)
    try: j = json.loads(r)
    except Exception: return {'error': str(r)[:200]}
    fr = j['fr']; return {'input_to_frame_ms': stats(j['lat']), 'input_queue_ms': stats(j['q']), 'frame_ms': stats(fr), 'frames_over_33ms': sum(1 for x in fr if x > 33.4)}
def wl(o):
    m = re.search(r'wlat n=(\d+) p50=(\d+) p99=(\d+) p999=(\d+) max=(\d+)', o)
    return dict(zip(('n', 'p50_us', 'p99_us', 'p999_us', 'max_us'), map(int, m.groups()))) if m else {'error': o[-200:]}
def phase_latency():
    modes = os.environ.get('PREEMPTS', 'voluntary full lazy none').replace(',', ' ').split(); reps = int(os.environ.get('REPS', '1'))
    for rep in range(reps):
        for mode in modes:
            vm, g, up = boot(f'l{mode}{rep}', f'preempt={mode}')
            if not up: stop(vm, g); continue
            if not wait_desktop(g): out('no desktop'); stop(vm, g); continue
            g.js(INIT_JS, 60); run(g, 'sleep 25; cat /sys/kernel/debug/sched/preempt 2>/dev/null; dmesg | grep -i "dynamic preempt" | tail -1; nproc', 60)
            g.js(PAGE_JS, 60); K = '/persist/k161/kload'
            r = {'idle': wl(run(g, f'{K} wlat 20', 60))}; r['idle'].update(page_input(g))
            run(g, f'for i in $(seq $(nproc)); do {K} burn & done; sleep 2', 30)
            r['cpu'] = wl(run(g, f'{K} wlat 30', 70)); r['cpu'].update(page_input(g))
            run(g, f'pkill -f "{K} burn"; sleep 2', 30)
            run(g, 'dd if=/dev/zero of=/persist/k161/big bs=1M count=1500 conv=fsync 2>&1 | tail -1; sync; echo 3 > /proc/sys/vm/drop_caches', 300)
            run(g, '(cp /persist/k161/big /persist/k161/big2; sync; echo COPY-DONE > /tmp/copydone) >/dev/null 2>&1 & sleep 1', 30)
            r['copy'] = wl(run(g, f'{K} wlat 30', 70)); r['copy'].update(page_input(g))
            r['copy']['copy_still_running'] = 'COPY-DONE' not in run(g, 'cat /tmp/copydone 2>/dev/null; true', 30)
            run(g, 'wait; while [ ! -e /tmp/copydone ]; do sleep 1; done; rm -f /persist/k161/big /persist/k161/big2; sync', 300)
            res(kind='latency', preempt=mode, rep=rep, kernel=os.path.basename(KERNEL) or 'release', **r)
            stop(vm, g)

# ---- memory: tabs, a game and a call at once
MEMRUN = r'''
K=/persist/k161/kload; SW=%s; ZP=%s; ZC=%s; TABS="%s"; GAME=%s
pkill -f "$K" 2>/dev/null; sleep 2
swapoff /dev/zram0 2>/dev/null; echo 1 > /sys/block/zram0/reset
echo $ZC > /sys/block/zram0/comp_algorithm || echo "COMP-REFUSED $ZC"
T=$(awk '/^MemTotal:/{print $2}' /proc/meminfo); S=$((T * 1024 / 100 * ZP)); [ $S -gt 8589934592 ] && S=8589934592
echo $S > /sys/block/zram0/disksize && mkswap /dev/zram0 >/dev/null && swapon -p 100 /dev/zram0
echo $SW > /proc/sys/vm/swappiness; sync; echo 3 > /proc/sys/vm/drop_caches; sleep 5
echo "CONF swappiness=$(cat /proc/sys/vm/swappiness) zram=$(($(cat /sys/block/zram0/disksize) / 1048576))MiB comp=$(sed 's/.*\[\(.*\)\].*/\1/' /sys/block/zram0/comp_algorithm)"
ML0=$(wc -l < /run/rudolf/memguard.log 2>/dev/null || echo 0); ME0=$(wc -l < /run/rudolf/memguard.events 2>/dev/null || echo 0)
OOM0=$(grep -c . /proc/vmstat >/dev/null; awk '/^oom_kill /{print $2}' /proc/vmstat)
SI0=$(awk '/^pswpin /{print $2}' /proc/vmstat); SO0=$(awk '/^pswpout /{print $2}' /proc/vmstat)
F0=$(sed -n 's/^full .*total=\([0-9]*\).*/\1/p' /proc/pressure/memory); S0=$(sed -n 's/^some .*total=\([0-9]*\).*/\1/p' /proc/pressure/memory)
for m in $TABS; do $K tabs $m >/dev/null 2>&1 & sleep 3; done
$K game $GAME 100 > /tmp/game.out 2>&1 & sleep 15
$K call 80 > /tmp/call.out 2>&1 &
i=0; MINA=999999; CRIT=0; MAXF=0
while [ $i -lt 95 ]; do i=$((i+1)); sleep 1
	A=$(awk '/^MemAvailable:/{print int($2/1024)}' /proc/meminfo); [ $A -lt $MINA ] && MINA=$A
	f=$(sed -n 's/^full avg10=\([0-9.]*\).*/\1/p' /proc/pressure/memory); MAXF=$(echo "$f $MAXF" | awk '{print ($1>$2)?$1:$2}')
	[ -e /run/rudolf/memguard.critical ] && [ $(( $(date +%%s) - $(stat -c %%Y /run/rudolf/memguard.critical) )) -le 1 ] && CRIT=$((CRIT+1))
	pgrep -f "$K game" >/dev/null || break
done
wait $(pgrep -f "$K game") 2>/dev/null; sleep 2
F1=$(sed -n 's/^full .*total=\([0-9]*\).*/\1/p' /proc/pressure/memory); S1=$(sed -n 's/^some .*total=\([0-9]*\).*/\1/p' /proc/pressure/memory)
echo "PSI some_ms=$(( (S1 - S0) / 1000 )) full_ms=$(( (F1 - F0) / 1000 )) max_full_avg10=$MAXF min_avail_mib=$MINA critical_s=$CRIT"
echo "SWAP in=$(( $(awk '/^pswpin /{print $2}' /proc/vmstat) - SI0 )) out=$(( $(awk '/^pswpout /{print $2}' /proc/vmstat) - SO0 )) zram_mm=$(cat /sys/block/zram0/mm_stat)"
echo "OOM $(( $(awk '/^oom_kill /{print $2}' /proc/vmstat) - OOM0 ))"
echo "GUARD log=$(( $(wc -l < /run/rudolf/memguard.log 2>/dev/null || echo 0) - ML0 )) ended=$(( $(wc -l < /run/rudolf/memguard.events 2>/dev/null || echo 0) - ME0 ))"
tail -n +$((ML0 + 1)) /run/rudolf/memguard.log 2>/dev/null | tail -5 | sed 's/^/GL /'
cat /tmp/game.out /tmp/call.out
pkill -f "$K"; sleep 3; swapoff /dev/zram0; echo DONE
'''
def phase_memory():
    runs = os.environ.get('MEMRUNS', '180:100:zstd 100:100:zstd 60:100:zstd 200:100:zstd 180:150:zstd 180:100:lzo-rle').replace(',', ' ').split()
    tabs = os.environ.get('TABS', '350 350 350 200').replace(',', ' '); game = os.environ.get('GAME', '1100')
    vm, g, up = boot('m')
    if not up or not wait_desktop(g): out('no desktop'); stop(vm, g); return
    g.js(INIT_JS, 60); run(g, 'sleep 30; free -m; cat /sys/block/zram0/comp_algorithm; cat /run/rudolf/memory', 60)
    for spec in runs:
        sw, zp, zc = spec.split(':')
        o = run(g, MEMRUN % (sw, zp, zc, tabs, game), 600)
        def num(k, d=None):
            m = re.search(k + r'=([0-9.]+)', o); return float(m.group(1)) if m else d
        gm = re.search(r'game frames=(\d+) slow33=(\d+) slow100=(\d+) p99=([0-9.]+) max=([0-9.]+)', o)
        cl = re.search(r'call n=(\d+) late10=(\d+) p99=([0-9.]+) max=([0-9.]+)', o)
        oom = re.search(r'^OOM (\d+)', o, re.M); conf = re.search(r'^CONF (.*)$', o, re.M)
        res(kind='memory', spec=spec, conf=conf.group(1) if conf else None, comp_refused='COMP-REFUSED' in o,
            psi_some_ms=num('some_ms'), psi_full_ms=num('full_ms'), max_full_avg10=num('max_full_avg10'), min_avail_mib=num('min_avail_mib'),
            guard_critical_s=num('critical_s'), guard_log_lines=num('GUARD log'), guard_ended=num('ended'), oom_kills=int(oom.group(1)) if oom else None,
            swap_in=num('SWAP in'), swap_out=num('out'),
            game=dict(zip(('frames', 'slow33', 'slow100', 'p99_ms', 'max_ms'), map(float, gm.groups()))) if gm else None,
            call=dict(zip(('periods', 'late10', 'p99_ms', 'max_ms'), map(float, cl.groups()))) if cl else None)
    stop(vm, g)

# ---- power: this tree's power script on QEMU's processors (no frequency driver), with Rudolf's own settings
def phase_power():
    vm, g, up = boot('p')
    if not up or not wait_desktop(g): stop(vm, g); return
    P = '/persist/k161/power'
    o = run(g, f"ls /sys/devices/system/cpu/cpufreq/ 2>&1 | head; cat /sys/devices/system/cpu/amd_pstate/status 2>&1; cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_driver 2>&1; "
               f"sh {P} status; echo; sh {P} set cpu=tuned; echo; echo RC=$?; sh {P} set game=1; echo; sh {P} set game=0; echo; sh {P} set cpu=std; echo; cat /run/rudolf/power-now", 120)
    res(kind='power', ok=('"ok":true' in o and '"cpu":' in o), out=o[-1500:])
    stop(vm, g)

make_drive(DRV)
try:
    {'startup': phase_startup, 'latency': phase_latency, 'memory': phase_memory, 'power': phase_power}[PHASE]()
finally:
    JS.close(); out('done')
