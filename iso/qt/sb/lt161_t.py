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
#          In the session's drawing mode and in software drawing (--draw=soft).
#   gpu:   in mixed drawing the desktop page's own web process paints on the processor, its tab's with the graphics card.
#   tabs:  a page's own full screen (a click, as a person's) fills the screen and Escape leaves it; a download in a tab
#          arrives complete in the desktop's download folder as the desktop's user; five tabs at once, each its own
#          process, their memory (PSS) reported, each under 300 MB.
# QEMU's own processor draws slowly: the screen is read until it shows what it should (up to 90 s), then the page goes on.
# Tests only.
#   LT_FIX=DIR python3 lt161_t.py REL-DIR      (root; SB_PARTS, QEMU, QEMU_ACCEL as for t160_t.py; LT_MODES=session,soft;
#                                               LT_PHASES=cover,gpu,tabs)
import os, sys, time, re, subprocess, shutil, struct, json, base64
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE); ISO = os.path.dirname(QT)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
from PIL import Image
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); FIX = os.environ.get('LT_FIX', '')
MODES = os.environ.get('LT_MODES', 'session,soft').split(','); PHASES = os.environ.get('LT_PHASES', 'cover,gpu,tabs').split(',')
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
/* the test reads the screen until it shows what it should (QEMU's own processor draws slowly), then opens the gate */
const gate=async k=>{for(let i=0;i<360;i++){try{if((await fetch('/gate-'+k,{cache:'no-store'})).ok)return}catch(e){}await sl(500)}};
async function run(){say('hello');
 send({rudolfShell:'tab',op:'open',tid:'t1',url:'http://127.0.0.1:8601/tab.html',part:'lt161'});
 send({rudolfShell:'tab',op:'place',tid:'t1',r:[100,100,700,450],vis:null,z:1});
 const ti=await wait(m=>m.ev=='title'&&m.tid=='t1'&&/^LT161 tab/.test(m.title||'')).catch(()=>null);say('title '+(ti?ti.title:'none'));
 await sl(1500);say('shot open');await gate('open');
 menu.style.display='block';say('cover');cov=true;
 send({rudolfShell:'tab',op:'place',tid:'t1',r:[100,100,700,450],vis:[[0,0,350,450]],z:1});
 const st=await wait(m=>m.ev=='still'&&m.tid=='t1'&&typeof m.img=='string').catch(()=>null);say('still '+(st?st.img.slice(0,23)+' '+st.img.length:'none'));
 await sl(1500);say('shot covered');await gate('covered');await sl(4500);say('again '+stills);cov=false;say('vis '+VC.visible+' '+VC.hidden);
 send({rudolfShell:'tab',op:'place',tid:'t1',r:[100,100,700,450],vis:null,z:1});menu.style.display='none';
 const s0=await wait(m=>m.ev=='still'&&m.tid=='t1'&&m.img===null).catch(()=>null);say('uncover '+!!s0);
 await sl(1500);say('shot uncovered');await gate('uncovered');say('done')}
</script>'''
TAB = '''<!doctype html><meta charset=utf-8><title>LT161 tab</title><body style="background:#fde68a;font:40px sans-serif;margin:0">A tab
<script>let n=0;setInterval(()=>{document.title='LT161 tab '+document.visibilityState+' '+(++n)},500)</script>'''
# (the tabs phase) a page's own full screen, a download, five tabs at once
DESK2 = r'''<!doctype html><meta charset=utf-8><title>lt161 tabs</title><body style="background:#123;margin:0;overflow:hidden"><script>
const say=s=>console.log('LT161 '+s);const send=m=>window.webkit.messageHandlers.rudolfShell.postMessage(m);
const ev=[];let hello=false;
window.__rwkIn=m=>{ev.push(m);if(m.rudolfShell=='hello'&&!hello){hello=true;send({rudolfShell:'ready'});run().catch(e=>say('ERR '+e.message))}};
const wait=(f,t=60000)=>new Promise((res,rej)=>{const t0=Date.now();const iv=setInterval(()=>{const i=ev.findIndex(f);if(i>=0){clearInterval(iv);res(ev.splice(i,1)[0])}else if(Date.now()-t0>t){clearInterval(iv);rej(new Error('timeout'))}},100)});
const sl=t=>new Promise(r=>setTimeout(r,t));
const gate=async k=>{for(let i=0;i<360;i++){try{if((await fetch('/gate-'+k,{cache:'no-store'})).ok)return}catch(e){}await sl(500)}};
async function run(){say('hello');
 send({rudolfShell:'tab',op:'open',tid:'t1',url:'http://127.0.0.1:8601/fs.html',part:'lt161'});
 send({rudolfShell:'tab',op:'place',tid:'t1',r:[200,150,600,400],vis:null,z:1});
 await wait(m=>m.ev=='load'&&m.tid=='t1'&&m.loading===false).catch(()=>0);await sl(1500);say('shot fsready');
 const on=await wait(m=>m.ev=='fs'&&m.tid=='t1',180000).catch(()=>null);say('fs '+(on&&on.on?'on':'none'));
 send({rudolfShell:'tab',op:'place',tid:'t1',r:null});await sl(1000);say('shot fs');await gate('fs');
 const off=await wait(m=>m.ev=='fs'&&m.tid=='t1',180000).catch(()=>null);say('fs '+(off&&off.on===false?'off':'none'));
 send({rudolfShell:'tab',op:'place',tid:'t1',r:[200,150,600,400],vis:null,z:1});await sl(1000);say('shot back');await gate('back');
 send({rudolfShell:'tab',op:'go',tid:'t1',url:'http://127.0.0.1:8601/file.bin'});
 const dl=await wait(m=>m.ev=='download'&&m.tid=='t1').catch(()=>null);say('download '+(dl?dl.url:'none'));await sl(4000);say('dl done');await gate('dl');
 send({rudolfShell:'tab',op:'close',tid:'t1'});
 for(let i=1;i<=5;i++){send({rudolfShell:'tab',op:'open',tid:'f'+i,url:'http://127.0.0.1:8601/page'+i+'.html',part:'five'});
  send({rudolfShell:'tab',op:'place',tid:'f'+i,r:i==5?[100,100,800,500]:null,z:i})}
 let n=0;for(let i=1;i<=5;i++){if(await wait(m=>m.ev=='load'&&m.tid=='f'+i&&m.loading===false,120000).catch(()=>null))n++}
 say('five '+n);await sl(5000);say('shot five');await gate('five');say('done')}
</script>'''
FS = '''<!doctype html><meta charset=utf-8><title>fs</title><body style="background:#fde68a;margin:0">
<button id=b style="position:fixed;inset:0;width:100%;height:100%;background:#fde68a;border:0;font:30px sans-serif">Full screen</button>
<script>b.onclick=()=>document.documentElement.requestFullscreen().catch(e=>document.title='ERR '+e.message)</script>'''
PAGE = '''<!doctype html><meta charset=utf-8><title>page %d</title><body style="background:#e0f2fe;font:16px sans-serif">
<h1>Page %d</h1><canvas id=c width=600 height=300></canvas><p>%s</p><script>const x=c.getContext('2d');let t=0;
setInterval(()=>{t++;x.fillStyle='hsl('+(t*7%%360)+',70%%,60%%)';x.fillRect((t*13)%%600,(t*7)%%300,40,40)},100)</script>'''
os.makedirs(f'{W}/fix/www8600'); os.makedirs(f'{W}/fix/www8601')
open(f'{W}/fix/www8600/desk.html', 'w').write(DESK); open(f'{W}/fix/www8601/tab.html', 'w').write(TAB)
open(f'{W}/fix/www8600/desk2.html', 'w').write(DESK2); open(f'{W}/fix/www8601/fs.html', 'w').write(FS)
open(f'{W}/fix/www8601/file.bin', 'wb').write(bytes(range(256)) * 400)
for i in range(1, 6): open(f'{W}/fix/www8601/page{i}.html', 'w').write(PAGE % (i, i, ' '.join(['Some words of text in a paragraph.'] * 400)))
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

SEEN = {}
def screen_until(tag, want, gate, secs=90):
    """QEMU's screen read until it shows what it should (or secs pass); then the page's gate is opened"""
    t0 = time.time(); f = None
    while time.time() - t0 < secs:
        f = vm.shot(tag)
        if want(f): SEEN[gate] = round(time.time() - t0, 1); break
        time.sleep(2)
    g.sh(f"touch /run/lt161/www8600/gate-{gate}", 20)
    return f
# the fix in place: rudolf-webkit in a tmpfs the desktop's user can run from, the pages served by busybox httpd
APPLY = ("FX=$(blkid | grep 'LABEL=\"LT161FIX\"' | cut -d: -f1 | head -1); echo fix=$FX; mkdir -p /tmp/fx; mountpoint -q /tmp/fx || mount -o ro $FX /tmp/fx; "
         "X=/run/lt161; mkdir -p $X; mountpoint -q $X || mount -t tmpfs -o exec,mode=755 none $X; cp /tmp/fx/rudolf-webkit $X/; chmod 755 $X/rudolf-webkit; "
         "cp -r /tmp/fx/www8600 /tmp/fx/www8601 $X/; chmod -R a+rX $X; "
         "busybox httpd -p 127.0.0.1:8600 -h $X/www8600; busybox httpd -p 127.0.0.1:8601 -h $X/www8601; sleep 1; busybox wget -q -O- http://127.0.0.1:8601/tab.html | head -c 80; echo; "
         "LD=$(ls /lib64/ld-linux-x86-64.so.2 2>/dev/null | head -1); $LD --list $X/rudolf-webkit > $X/libs.txt 2>&1; echo libs $(grep -c . $X/libs.txt) missing $(grep -c 'not found' $X/libs.txt); grep 'not found' $X/libs.txt; ls -l $X")
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
    ok('0c. this tree\'s rudolf-webkit in place, with every library it needs in the release; the pages served', 'LT161 tab' in o and re.search(r'^libs [1-9]\d* missing 0$', o, re.M), o)
    for mode in (MODES if 'cover' in PHASES else []):
        m = smode if mode == 'session' else mode
        SEEN.clear()
        tag = f'{mode}({m})'
        sh(f"pkill -f '/run/lt161/rudolf-webkit' ; rm -f /run/lt161/www8600/gate-*; rm -rf /tmp/lth; mkdir -p /tmp/lth; chown rudolf /tmp/lth; : > /tmp/lt.log; chmod 666 /tmp/lt.log; "
           f"su -s /bin/sh rudolf -c 'cd /tmp/lth; exec env DISPLAY=:0 HOME=/tmp/lth /run/lt161/rudolf-webkit --size={SW}x{SH} --draw={m} http://127.0.0.1:8600/desk.html' >> /tmp/lt.log 2>&1 &", 30)
        WANT = {'open': lambda f: yellow(px(f, 600, 400)), 'covered': lambda f: red(px(f, 650, 400)) and yellow(px(f, 250, 400)),
                'uncovered': lambda f: yellow(px(f, 650, 400))}
        shots = {}; end = time.time() + 600; lg = ''
        while time.time() < end:
            lg = sh("grep -o 'LT161 .*' /tmp/lt.log", 20, False)
            for k in ('open', 'covered', 'uncovered'):
                if f'LT161 shot {k}' in lg and k not in shots: shots[k] = screen_until(f'{mode}-{k}', WANT[k], k)
            if 'LT161 done' in lg or 'LT161 ERR' in lg: break
            time.sleep(1)
        out('the page:', lg.strip().replace('\n', ' | '))
        P1 = px(shots.get('open'), 600, 400); P2L = px(shots.get('covered'), 250, 400); P2R = px(shots.get('covered'), 650, 400); P3 = px(shots.get('uncovered'), 650, 400)
        out('pixels', tag, 'open', P1, 'covered: left', P2L, 'right', P2R, 'uncovered', P3, '- on the screen after (s):', {k: SEEN.get(k) for k in WANT})
        vis = re.search(r'LT161 vis (\d+) (\d+)', lg)
        ok(f'{tag} 1. a tab opens its page and is drawn on the screen', 'LT161 title LT161 tab' in lg and yellow(P1), [lg[-300:], P1])
        st = re.search(r'LT161 still (\S+) (\d+)', lg)
        ok(f'{tag} 2. covered by the desktop\'s menu: the desktop gets a still picture of the tab', bool(st) and st.group(1) == 'data:image/jpeg;base64,' and int(st.group(2)) > 2000, lg[-300:])
        ok(f'{tag} 3. ...the menu is seen over the tab, not behind its page', red(P2R), P2R)
        ok(f'{tag} 4. ...with the picture of the page beside it', yellow(P2L), P2L)
        ag = re.search(r'LT161 again (\d+)', lg)
        ok(f'{tag} 5. ...its picture taken again while covered', bool(ag) and int(ag.group(1)) >= 2, lg[-300:])
        ok(f'{tag} 6. uncovered: the live page is back', 'LT161 uncover true' in lg and yellow(P3), P3)
        ok(f'{tag} 7. ...the live page ran on while covered, visible to its scripts', bool(vis) and int(vis.group(1)) > 5 and vis.group(2) == '0', lg[-300:])
        out(sh("grep -v 'LT161' /tmp/lt.log | tail -15", 20, False))
        sh("pkill -f '/run/lt161/rudolf-webkit'", 20, False); time.sleep(3)
    # 1.6.1 (stutter in Oracle's tabs): in mixed drawing the desktop page's own process paints on the processor, the tabs'
    # with the graphics card (here QEMU's has none: Mesa's llvmpipe stands in, as WebKit sees it the same way)
    if 'gpu' in PHASES:
        sh(f"pkill -f '/run/lt161/rudolf-webkit' ; rm -f /run/lt161/www8600/gate-*; rm -rf /tmp/lth; mkdir -p /tmp/lth; chown rudolf /tmp/lth; : > /tmp/lt.log; chmod 666 /tmp/lt.log; "
           f"su -s /bin/sh rudolf -c 'cd /tmp/lth; exec env DISPLAY=:0 HOME=/tmp/lth /run/lt161/rudolf-webkit --size={SW}x{SH} --draw=mixed http://127.0.0.1:8600/desk.html' >> /tmp/lt.log 2>&1 &", 30)
        end = time.time() + 120
        while time.time() < end and 'LT161 shot open' not in sh("grep -o 'LT161 .*' /tmp/lt.log", 20, False): time.sleep(2)
        o = sh("P=$(pgrep -f '^/run/lt161/rudolf-webkit' | head -1); echo P=$P; for d in /proc/[0-9]*; do [ \"$(awk '{print $4}' $d/stat 2>/dev/null)\" = \"$P\" ] || continue; "
               "grep -q WebKitWebProces $d/comm 2>/dev/null || continue; e=$(tr '\\0' '\\n' < $d/environ | grep -c '^WEBKIT_SKIA_ENABLE_CPU_RENDERING=1'); "
               "t=$(cat $d/task/*/comm 2>/dev/null | grep -c '^SkiaCPU'); echo \"WP $(awk '{print $22}' $d/stat) ${d#/proc/} $e $t\"; done | sort -n -k2", 60)
        L = sorted([tuple(int(x) for x in m) for m in re.findall(r'^WP (\d+) (\d+) (\d+) (\d+)$', o, re.M)])
        ok('gpu 1. mixed drawing: the desktop page\'s process and its tab\'s', len(L) == 2, o)
        ok('gpu 2. ...the desktop\'s paints on the processor, with its painting threads', len(L) >= 1 and L[0][2] == 1 and L[0][3] > 0, L)
        ok('gpu 3. ...the tab\'s with the graphics card (no processor painting, no painting threads)', len(L) == 2 and L[1][2] == 0 and L[1][3] == 0, L)
        SEEN.clear(); shot = screen_until('mixed-tab', lambda f: yellow(px(f, 600, 400)), 'open')
        ok('gpu 4. ...and the tab is drawn on the screen', yellow(px(shot, 600, 400)), [px(shot, 600, 400), SEEN.get('open')])
        sh("pkill -f '/run/lt161/rudolf-webkit'", 20, False); time.sleep(3)
    # 1.6.1 (Oracle's WebKit tabs: a download, full screen, memory with 5 tabs), on the release's own Lite session
    if 'tabs' in PHASES:
        sh(f"pkill -f '/run/lt161/rudolf-webkit' ; rm -f /run/lt161/www8600/gate-* /run/rudolf/dl/.incoming/file.bin*; rm -rf /tmp/lth; mkdir -p /tmp/lth; chown rudolf /tmp/lth; : > /tmp/lt.log; chmod 666 /tmp/lt.log; "
           f"su -s /bin/sh rudolf -c 'cd /tmp/lth; exec env DISPLAY=:0 HOME=/tmp/lth /run/lt161/rudolf-webkit --size={SW}x{SH} --draw={smode} http://127.0.0.1:8600/desk2.html' >> /tmp/lt.log 2>&1 &", 30)
        log = lambda: sh("grep -o 'LT161 .*' /tmp/lt.log", 20, False)
        def wait_log(k, secs):
            end = time.time() + secs; lg = ''
            while time.time() < end:
                lg = log()
                if k in lg or 'LT161 ERR' in lg: break
                time.sleep(2)
            return lg
        lg = wait_log('LT161 shot fsready', 240)
        SEEN.clear()
        if 'shot fsready' in lg:
            for _ in range(4):   # (a person's click on the page's button; once more if the page did not take it yet)
                g.q.at(500, 350, W=SW, H=SH)
                lg = wait_log('LT161 fs ', 40)
                if 'LT161 fs on' in lg: break
        ok('tabs 1. a page\'s own full screen (a click on its button) is said to the desktop', 'LT161 fs on' in lg, lg[-300:])
        wait_log('LT161 shot fs', 60)
        f = screen_until('fullscreen', lambda f: all(yellow(px(f, x, y)) for x, y in ((5, 5), (SW - 6, SH - 6), (SW // 2, 60))), 'fs')
        ok('tabs 2. ...and fills the screen', all(yellow(px(f, x, y)) for x, y in ((5, 5), (SW - 6, SH - 6), (SW // 2, 60))), [px(f, 5, 5), px(f, SW - 6, SH - 6), SEEN.get('fs')])
        time.sleep(1); g.q.key('esc'); lg = wait_log('LT161 shot back', 120)
        ok('tabs 3. Escape leaves it, said to the desktop', 'LT161 fs off' in lg, lg[-300:])
        f = screen_until('back', lambda f: not yellow(px(f, 5, 5)) and yellow(px(f, 300, 200)), 'back')
        ok('tabs 4. ...and the tab is back where it was', not yellow(px(f, 5, 5)) and yellow(px(f, 300, 200)), [px(f, 5, 5), px(f, 300, 200)])
        lg = wait_log('LT161 dl done', 120)
        o = sh("ls -ln /run/rudolf/dl/.incoming/ | grep file.bin; stat -c '%s %U' /run/rudolf/dl/.incoming/file.bin 2>&1", 20)
        g.sh("touch /run/lt161/www8600/gate-dl", 20)
        ok('tabs 5. a download in a tab is said to the desktop', 'LT161 download http://127.0.0.1:8601/file.bin' in lg, lg[-300:])
        ok('tabs 6. ...and arrives complete where the desktop files downloads from (as the desktop\'s user)', re.search(r'^102400 rudolf$', o, re.M) and '.crdownload' not in o, o)
        lg = wait_log('LT161 shot five', 400)
        ok('tabs 7. five tabs at once, all loaded', 'LT161 five 5' in lg, lg[-300:])
        o = sh("P=$(pgrep -f '^/run/lt161/rudolf-webkit' | head -1); for d in /proc/[0-9]*; do [ \"$(awk '{print $4}' $d/stat 2>/dev/null)\" = \"$P\" ] || continue; "
               "grep -q WebKitWebProces $d/comm 2>/dev/null || continue; echo \"WM $(awk '{print $22}' $d/stat) $(awk '/^Pss:/{print int($2/1024)}' $d/smaps_rollup)\"; done | sort -n -k2; "
               "free -m | head -2", 60)
        g.sh("touch /run/lt161/www8600/gate-five", 20)
        M = sorted([(int(a), int(b)) for a, b in re.findall(r'^WM (\d+) (\d+)$', o, re.M)])
        out('memory (PSS, MB) on the release: the desktop page', M[0][1] if M else '-', '; five tabs', [m[1] for m in M[1:]], '; total', sum(m[1] for m in M))
        ok('tabs 8. ...each in its own web process (the desktop page\'s and five)', len(M) == 6, o)
        ok('tabs 9. ...each tab under 300 MB (PSS)', len(M) == 6 and all(0 < m[1] < 300 for m in M[1:]), M)
        sh("pkill -f '/run/lt161/rudolf-webkit'", 20, False); time.sleep(3)
    ok('8. the release\'s own Lite desktop still runs', 'rudolf-webkit' in sh("pgrep -a -f 'rudolf-webkit --rudolf-kiosk'", 20, False))
finally:
    try: vm and vm.stop()
    except Exception: pass
    shutil.rmtree(W, ignore_errors=True)
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
