# Rudolf OS 1.6.1 in QEMU: Lite's Oracle tabs on the real release (docs/plan-1.6.1.md Part 3, Lite as a compatibility option).
# The release as installed (slot a on an NVMe drive with RUDOLFDATA, UEFI with Secure Boot off), started with Lite chosen
# (/persist/etc/rudolf-desktop-engine: webkit), so the release's own session, X server, rudolf-display, WebKitGTK and
# GStreamer are what run. This tree's rudolf-webkit (LT_FIX: a folder with it, built for the release's Ubuntu) is started
# beside the release's Lite desktop, as the desktop's user on its screen, with a stand-in desktop page (as
# iso/qt/webkit/tabs58_ci.py's: tabs26.js's messages) served in the guest by busybox httpd (127.0.0.1:8600, the tab's
# page on 8601); what is on the screen is read from QEMU's screen. Checks:
#   cover: a tab placed and drawn; the desktop's stand-in menu over part of it (place with vis): rudolf-webkit sends a still
#          picture of the page, the desktop shows it under the menu, the menu is seen over the tab (not behind its page),
#          the live page still runs for its scripts and its picture is taken again; uncovered, the live page is back.
# In the session's drawing mode and in software drawing (--draw=soft). Tests only.
#   LT_FIX=DIR python3 lt161_t.py REL-DIR      (root; SB_PARTS, QEMU, QEMU_ACCEL as for t160_t.py; LT_MODES=session,soft)
import os, sys, time, re, subprocess, shutil, struct, json, base64
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE); ISO = os.path.dirname(QT)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
from PIL import Image
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); FIX = os.environ.get('LT_FIX', '')
MODES = os.environ.get('LT_MODES', 'session,soft').split(',')
LOG = open('lt161.txt', 'w'); R = []; T0 = time.time()
def out(*a):
    s = ' '.join(str(x) for x in a); s = f'[{int(time.time() - T0):5d}s] ' + s; print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:1500] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 3000; DRV = os.path.abspath('lt161.img'); MEM = int(os.environ.get('MEM', '3400')); W = os.path.abspath('w161lt')
shutil.rmtree(W, ignore_errors=True); os.makedirs(W)
out('release', mv('serial'), mv('version'), 'fix', FIX or '(none)')
if not FIX or not os.path.exists(f'{FIX}/rudolf-webkit'): out('LT_FIX: a folder with this tree\'s rudolf-webkit is needed'); sys.exit(2)

# the stand-in desktop page: the tab at 100,100 (700x450); its picture in #tab, the stand-in menu (red) over its right half
DESK = r'''<!doctype html><meta charset=utf-8><title>lt161</title><body style="background:#123;margin:0;overflow:hidden">
<div id=tab style="position:fixed;left:100px;top:100px;width:700px;height:450px"></div>
<div id=menu style="position:fixed;left:450px;top:100px;width:350px;height:450px;background:#c00;display:none"></div><script>
const say=s=>console.log('LT161 '+s);const send=m=>window.webkit.messageHandlers.rudolfShell.postMessage(m);
const ev=[];let hello=false,stills=0,cov=false;const VC={visible:0,hidden:0};const tabEl=document.getElementById('tab'),menu=document.getElementById('menu');
window.__rwkIn=m=>{ev.push(m);if(m.rudolfShell=='hello'&&!hello){hello=true;send({rudolfShell:'ready'});run().catch(e=>say('ERR '+e.message))}
 if(m.rudolfShell=='tabev'&&m.ev=='still'){if(m.img)stills++;tabEl.style.backgroundImage=m.img?'url("'+m.img+'")':'';tabEl.style.backgroundSize='100% 100%'}
 if(m.rudolfShell=='tabev'&&m.ev=='title'&&cov){const v=/^LT161 tab (\w+) \d+$/.exec(m.title||'');if(v)VC[v[1]]=(VC[v[1]]||0)+1}};
const wait=(f,t=30000)=>new Promise((res,rej)=>{const t0=Date.now();const iv=setInterval(()=>{const r=ev.find(f);if(r){clearInterval(iv);res(r)}else if(Date.now()-t0>t){clearInterval(iv);rej(new Error('timeout'))}},100)});
const sl=t=>new Promise(r=>setTimeout(r,t));
async function run(){say('hello');
 send({rudolfShell:'tab',op:'open',tid:'t1',url:'http://127.0.0.1:8601/tab.html',part:'lt161'});
 send({rudolfShell:'tab',op:'place',tid:'t1',r:[100,100,700,450],vis:null,z:1});
 const ti=await wait(m=>m.ev=='title'&&m.tid=='t1'&&/^LT161 tab/.test(m.title||'')).catch(()=>null);say('title '+(ti?ti.title:'none'));
 await sl(3000);say('shot open');await sl(9000);
 menu.style.display='block';say('cover');cov=true;
 send({rudolfShell:'tab',op:'place',tid:'t1',r:[100,100,700,450],vis:[[0,0,350,450]],z:1});
 const st=await wait(m=>m.ev=='still'&&m.tid=='t1'&&typeof m.img=='string').catch(()=>null);say('still '+(st?st.img.slice(0,23)+' '+st.img.length:'none'));
 await sl(1500);say('shot covered');await sl(9000);say('again '+stills);cov=false;say('vis '+VC.visible+' '+VC.hidden);
 send({rudolfShell:'tab',op:'place',tid:'t1',r:[100,100,700,450],vis:null,z:1});menu.style.display='none';
 const s0=await wait(m=>m.ev=='still'&&m.tid=='t1'&&m.img===null).catch(()=>null);say('uncover '+!!s0);
 await sl(2000);say('shot uncovered');await sl(9000);say('done')}
</script>'''
TAB = '''<!doctype html><meta charset=utf-8><title>LT161 tab</title><body style="background:#fde68a;font:40px sans-serif;margin:0">A tab
<script>let n=0;setInterval(()=>{document.title='LT161 tab '+document.visibilityState+' '+(++n)},500)</script>'''
os.makedirs(f'{W}/fix/www8600'); os.makedirs(f'{W}/fix/www8601')
open(f'{W}/fix/www8600/desk.html', 'w').write(DESK); open(f'{W}/fix/www8601/tab.html', 'w').write(TAB)
shutil.copy(f'{FIX}/rudolf-webkit', f'{W}/fix/rudolf-webkit')
FIXIMG = os.path.abspath('lt161fix.img'); subprocess.run(['truncate', '-s', '64M', FIXIMG], check=True)
subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'LT161FIX', '-d', f'{W}/fix', FIXIMG], check=True)

def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    d = f'{T}/os/system/a'; os.makedirs(d)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs', 'manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{d}/{n}')
    open(f'{d}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n"); open(f'{d}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    open(f'{T}/etc/rudolf-desktop-engine', 'w').write('webkit\n')   # (Lite)
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)

def boot(tag):
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf161',
             '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1',
             '-drive', f'file={FIXIMG},format=raw,if=none,id=fx,readonly=on', '-device', 'nvme,drive=fx,serial=lt161fix']
    os.makedirs('lt161' + tag, exist_ok=True)
    if not os.path.exists(PARTS): open(f'lt161{tag}/vars.fd', 'ab').close()
    vm = sbvm.VM('lt161' + tag, PARTS, [], secure=False, mem=MEM, extra=extra)
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    time.sleep(30); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    return vm, g, up
def put_small(g, data, remote):
    b = base64.b64encode(data.encode()).decode()
    if len(b) < 800: g.sh(f"echo {b} | base64 -d > {remote}", 20); return
    f = os.path.join(W, 'p.tmp'); open(f, 'w').write(data); g.put(f, remote, '644')
def run(g, c, t=60, show=True):
    put_small(g, c + '\n', '/tmp/c161.sh')
    o = g.sh('sh /tmp/c161.sh 2>&1', t)
    if show: out('$', c[:160].replace('\n', ' ; ')); out(o.rstrip()[-2500:])
    return o
def px(f, x, y):
    try: return Image.open(f).convert('RGB').getpixel((x, y))
    except Exception as e: return None
yellow = lambda c: bool(c) and c[0] > 200 and c[1] > 190 and c[2] < 180
red = lambda c: bool(c) and c[0] > 170 and c[1] < 60 and c[2] < 60

# the fix in place: rudolf-webkit in a tmpfs the desktop's user can run from, the pages served by busybox httpd
APPLY = ("FX=$(blkid | grep 'LABEL=\"LT161FIX\"' | cut -d: -f1 | head -1); echo fix=$FX; mkdir -p /tmp/fx; mountpoint -q /tmp/fx || mount -o ro $FX /tmp/fx; "
         "X=/run/lt161; mkdir -p $X; mountpoint -q $X || mount -t tmpfs -o exec,mode=755 none $X; cp /tmp/fx/rudolf-webkit $X/; chmod 755 $X/rudolf-webkit; "
         "cp -r /tmp/fx/www8600 /tmp/fx/www8601 $X/; chmod -R a+rX $X; "
         "busybox httpd -p 127.0.0.1:8600 -h $X/www8600; busybox httpd -p 127.0.0.1:8601 -h $X/www8601; sleep 1; busybox wget -q -O- http://127.0.0.1:8601/tab.html | head -c 80; echo; "
         "ldd $X/rudolf-webkit | grep -c 'not found'; ls -l $X")
make_drive(DRV)
vm = None
try:
    vm, g, up = boot('a'); sh = lambda c, t=60, show=True: run(g, c, t, show)
    ok('0. the release starts (Lite chosen)', up)
    if not up: raise SystemExit
    o = sh("for i in $(seq 1 100); do [ -e /run/rudolf/heartbeat ] && pgrep -f 'rudolf-webkit --rudolf-kiosk' >/dev/null && break; sleep 3; done; "
           "cat /run/rudolf/desktop-engine; ls /run/rudolf/heartbeat; tr '\\0' ' ' < /proc/$(pgrep -o -f 'rudolf-webkit --rudolf-kiosk')/cmdline; echo; cat /run/rudolf/screen", 360)
    ok('0b. the release\'s Lite desktop runs and answers (its heartbeat)', 'webkit' in o and '/run/rudolf/heartbeat' in o and 'rudolf-webkit --rudolf-kiosk' in o, o)
    smode = (re.search(r'--draw=(\w+)', o) or [None, 'gpu'])[1]
    scr = re.search(r'^(\d+) (\d+)$', o, re.M); SW, SH = (int(scr.group(1)), int(scr.group(2))) if scr else (1280, 800)
    o = sh(APPLY, 120)
    ok('0c. this tree\'s rudolf-webkit in place, with every library it needs in the release; the pages served', 'A tab' in o and re.search(r'^0$', o, re.M), o)
    for mode in MODES:
        m = smode if mode == 'session' else mode
        tag = f'{mode}({m})'
        sh(f"pkill -f '/run/lt161/rudolf-webkit' ; rm -rf /tmp/lth; mkdir -p /tmp/lth; chown rudolf /tmp/lth; : > /tmp/lt.log; chmod 666 /tmp/lt.log; "
           f"su -s /bin/sh rudolf -c 'cd /tmp/lth; exec env DISPLAY=:0 HOME=/tmp/lth /run/lt161/rudolf-webkit --size={SW}x{SH} --draw={m} http://127.0.0.1:8600/desk.html' >> /tmp/lt.log 2>&1 &", 30)
        shots = {}; end = time.time() + 240; lg = ''
        while time.time() < end:
            lg = sh("grep -o 'LT161 .*' /tmp/lt.log", 20, False)
            for k in ('open', 'covered', 'uncovered'):
                if f'LT161 shot {k}' in lg and k not in shots: time.sleep(1.5); shots[k] = vm.shot(f'{mode}-{k}')
            if 'LT161 done' in lg or 'LT161 ERR' in lg: break
            time.sleep(1)
        out('the page:', lg.strip().replace('\n', ' | '))
        P1 = px(shots.get('open'), 600, 400); P2L = px(shots.get('covered'), 250, 400); P2R = px(shots.get('covered'), 650, 400); P3 = px(shots.get('uncovered'), 650, 400)
        out('pixels', tag, 'open', P1, 'covered: left', P2L, 'right', P2R, 'uncovered', P3)
        vis = re.search(r'LT161 vis (\d+) (\d+)', lg)
        ok(f'{tag} 1. a tab opens its page and is drawn on the screen', 'LT161 title LT161 tab' in lg and yellow(P1), [lg[-300:], P1])
        st = re.search(r'LT161 still (\S+) (\d+)', lg)
        ok(f'{tag} 2. covered by the desktop\'s menu: the desktop gets a still picture of the tab', bool(st) and st.group(1) == 'data:image/jpeg;base64' and int(st.group(2)) > 2000, lg[-300:])
        ok(f'{tag} 3. ...the menu is seen over the tab, not behind its page', red(P2R), P2R)
        ok(f'{tag} 4. ...with the picture of the page beside it', yellow(P2L), P2L)
        ag = re.search(r'LT161 again (\d+)', lg)
        ok(f'{tag} 5. ...its picture taken again while covered', bool(ag) and int(ag.group(1)) >= 2, lg[-300:])
        ok(f'{tag} 6. uncovered: the live page is back', 'LT161 uncover true' in lg and yellow(P3), P3)
        ok(f'{tag} 7. ...the live page ran on while covered, visible to its scripts', bool(vis) and int(vis.group(1)) > 5 and vis.group(2) == '0', lg[-300:])
        out(sh("grep -v 'LT161' /tmp/lt.log | tail -15", 20, False))
        sh("pkill -f '/run/lt161/rudolf-webkit'", 20, False); time.sleep(3)
    ok('8. the release\'s own Lite desktop still runs', 'rudolf-webkit' in sh("pgrep -a -f 'rudolf-webkit --rudolf-kiosk'", 20, False))
finally:
    try: vm and vm.stop()
    except Exception: pass
    shutil.rmtree(W, ignore_errors=True)
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
