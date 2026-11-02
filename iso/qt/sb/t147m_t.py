# Rudolf OS 1.4.7 in QEMU, with 2 GB of memory: compressed swap (zram) and the memory guard. A test page in one of
# Oracle's real tabs takes memory until none is left (as Friday Night Funkin' did on a 3.4 GB laptop with 1.4.6).
# Passes when the desktop keeps responding, only that tab is closed, Oracle shows why with a notification, the browser
# is not restarted, and Reload opens the page again. The release's kernel with its system in slot a (as t145p).
#   python3 t147m_t.py REL-DIR
import os, sys, time, re, subprocess, shutil, struct, json
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); LOG = open('t147m.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:400] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 1500; DRV = os.path.abspath('int147m.img'); MEM = int(os.environ.get('MEM', '2048'))

def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    a = f'{T}/os/system/a'; os.makedirs(a)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs', 'manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{a}/{n}')
    open(f'{a}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n")
    open(f'{a}/complete', 'w').write('1\n'); open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)

# the test page: takes 32 MB more every 0.3 s, copies of one random block (compresses poorly; quick to copy in QEMU); once loaded again (Reload) it takes none
HOG = r'''<!doctype html><meta charset="utf-8"><title>Memory hog</title><body style="font:20px sans-serif"><h1 id=h>Memory hog</h1><script>
const again=localStorage.getItem('hogran')=='1';localStorage.setItem('hogran','1');const keep=[];let mb=0,x=2463534242;
const src=new Uint32Array(8<<20);for(let i=0;i<src.length;i++){x^=x<<13;x^=x>>>17;x^=x<<5;src[i]=x>>>0}
function step(){const a=new Uint32Array(8<<20);a.set(src);a[mb]=mb;keep.push(a);mb+=32;document.title='Memory hog '+mb+' MB';h.textContent=document.title;setTimeout(step,300)}
if(again){document.title='Memory hog (reloaded)';h.textContent=document.title}else setTimeout(step,1500)
</script>'''

EXTRA = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf147m',
         '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.trying=1 rudolf.debug=1']
def boot():
    vm = sbvm.VM('t147m', PARTS, [], secure=False, mem=MEM, extra=EXTRA)
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
    while up and time.time() < end and js('typeof browser=="function"&&typeof memGone=="function"', 20) is not True: time.sleep(5)
    # sign in as Guest (that loads the page again); click again until it has worked: on a slow start the login
    # screen may not be there yet
    end = time.time() + 300
    while up and time.time() < end:
        r = js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};const b=[...document.querySelectorAll('button')].find(b=>b.textContent.trim()=='Guest'&&b.offsetParent!==null);if(b){b.click();return 'clicked'}return typeof USER!='undefined'&&USER&&typeof launch=='function'?'in':'wait'})()", 20)
        out('sign-in:', r)
        if r == 'in': break
        time.sleep(12)
    time.sleep(5)
    return vm, g, up, js

make_drive(DRV)
vm = None
try:
    vm, g, up, js = boot()
    ok('1. Rudolf OS starts (%d MB of memory)' % MEM, up)
    if not up: raise SystemExit
    sh = lambda c, t=30: g.sh(c, t)
    m = sh('cat /run/rudolf/memory; cat /proc/swaps; cat /sys/kernel/mm/lru_gen/min_ttl_ms; cat /proc/sys/vm/swappiness; ls /proc/pressure; pgrep -x rudolf-memguard | head -1; free -m')
    out(m)
    ok('2. compressed swap in memory is on (zram, zstd, half the memory)', re.search(r'zram: on, \d+ MiB, zstd', m) and '/dev/zram0' in m, m)
    ok('3. the kernel ends a process instead of thrashing (lru_gen min_ttl_ms 1000), memory pressure (PSI) is there', re.search(r'^1000$', m, re.M) and 'memory' in m, m)
    ok('4. the memory guard runs', re.search(r'^\d+$', m.split('memory\n', 1)[-1], re.M) is not None, m)
    # the browser itself: the parent of its graphics process
    adj = sh("G=$(pgrep -f 'type=gpu-process' | head -1); B=$(cut -d' ' -f4 /proc/$G/stat); for p in $B $G $(pgrep -x rudolfd | head -1); do cat /proc/$p/oom_score_adj; done")
    v = re.findall(r'^(-?\d+)$', adj, re.M)
    ok('5. the desktop\'s browser, its graphics process and rudolfd are ended last (oom_score_adj below 0)', len(v) == 3 and all(int(x) < 0 for x in v), adj)
    # the desktop page says which page process is its own (it keeps its processor busy for half a second at start)
    dr = ''
    for _ in range(20):
        dr = sh("cat /run/rudolf/desktop-renderer 2>/dev/null | sed 's/^/DR /'")
        m = re.search(r'^DR (\d+)', dr, re.M)
        if m: break
        time.sleep(6)
    dpid = m.group(1) if m else ''
    iam = js("JSON.stringify(window.__memIam||null)")
    out('desktop page process:', dpid, iam)
    ok('5b. the desktop page tells the memory guard which process is its own', dpid != '' and js("!!window.__memIam&&window.__memIam.pid==" + (dpid or '0')) is True, [dr, iam])
    # the test page, served in the guest
    sh("mkdir -p /tmp/hog && cat > /tmp/hog/index.html <<'EOF'\n" + HOG + "\nEOF\n(busybox httpd -p 127.0.0.1:8099 -h /tmp/hog &) ; sleep 1; wget -qO- http://127.0.0.1:8099/ | head -c 40")
    sh(': > /run/rudolf/memguard.log; rm -f /run/rudolf/memguard.events; wc -l < /run/rudolf/session.log > /tmp/sl0')
    js("window.__mark147=1;wins.slice().forEach(w=>{w.__force=1;w.close()});browser('http://127.0.0.1:8099/');1")
    time.sleep(20); vm.shot('hog-start')
    out('tab title:', js("wins.at(-1).el.querySelector('.ttl,.title')&&wins.at(-1).el.querySelector('.ttl,.title').textContent||document.title"))
    # watch: the desktop answers, memory, what the guard did; up to 15 minutes (QEMU emulates the processor: slow)
    t0 = time.time(); worst = 0; ended = False; samples = []
    while time.time() - t0 < 900:
        a = time.time(); r = g.js('window.__mark147===1', 90); dt = time.time() - a; worst = max(worst, dt)
        st = sh("awk '/MemAvailable|SwapFree/{printf \"%s %d MB  \", $1, $2/1024}' /proc/meminfo; cat /proc/pressure/memory | head -1; tail -2 /run/rudolf/memguard.log", 60)
        samples.append((round(time.time() - t0), round(dt, 1), r)); out(f'{round(time.time()-t0)}s desktop answered in {dt:.1f}s ({r}) | {st.strip()}')
        if 'ended the largest web page' in st: ended = True; break
        time.sleep(3)
    time.sleep(10); vm.shot('hog-ended')
    ok('6. the memory guard ended the test page when memory ran out', ended, samples[-3:])
    ev = sh("cat /run/rudolf/memguard.events | sed 's/^/EV /'; [ -n '%s' ] && [ -d /proc/%s ] && echo DESKALIVE" % (dpid, dpid))
    ended_pids = re.findall(r'^EV \d+ (\d+) ', ev, re.M)
    ok('6b. ...not the desktop page\'s own process, which is still there', ended_pids and dpid not in ended_pids and 'DESKALIVE' in ev, ev)
    ok('7. the desktop kept responding (same page, never more than 30 s to answer)', js('window.__mark147===1') is True and worst < 30, worst)
    sl = sh("tail -n +$(( $(cat /tmp/sl0) + 1 )) /run/rudolf/session.log | grep -E 'restartin[g] browser|browse[r] exited|memory guar[d]' | sed 's/^/LOG /' | head")
    sl = '\n'.join(l[4:] for l in sl.split('\n') if l.startswith('LOG '))
    out(sl)
    ok('8. the browser was not restarted (no watchdog restart)', 'restarting browser' not in sl and 'browser exited' not in sl, sl)
    page = js("(()=>{const W=wins.at(-1);const f=[...W.el.querySelectorAll('iframe')].find(f=>/used too much memory/.test(f.srcdoc||''));return !!f&&getComputedStyle(f).display!='none'})()")
    ok('9. Oracle shows "This page was closed because it used too much memory" in the tab', page is True, page)
    note = js("NC.some(n=>n.t=='Page closed to free memory'&&/Memory hog/.test(n.b))")
    ok('10. ...and a notification says so', note is True, note)
    js("wins.at(-1).el.dispatchEvent(new KeyboardEvent('keydown',{key:'F5',bubbles:true}));1"); time.sleep(20); vm.shot('hog-reloaded')
    rel = js("(()=>{const W=wins.at(-1);const f=[...W.el.querySelectorAll('iframe')].find(f=>/used too much memory/.test(f.srcdoc||''));return [!f||getComputedStyle(f).display=='none',W.el.innerText.includes('reloaded')||document.title]})()")
    hogt = sh("pgrep -f 'type=renderer' | wc -l")
    out('after Reload:', rel, 'renderers:', hogt)
    ok('11. Reload opens the page again (a real tab)', isinstance(rel, list) and rel[0] is True, rel)
    out(sh('cat /run/rudolf/memguard.log; free -m; cat /proc/swaps', 30))
finally:
    try: vm and vm.stop()
    except Exception: pass
try: os.remove(DRV)
except OSError: pass
out(f'{sum(R)}/{len(R)} passed')
