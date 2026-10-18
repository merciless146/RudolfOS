# Rudolf OS 1.2 on QEMU: the move into the shell, Oracle's real tabs, the keys Chromium keeps for itself (rudolf-display's
# stand-ins), pointer capture for games and fullscreen, with real input from QEMU (keyboard, tablet, mouse).
#   python3 v120_t.py NAME ISO          (from iso/qt/shell2; boots with ../boot.sh, diagnostic start)
import subprocess, time, os, sys, json
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, QT)
from PIL import Image
N, ISO = sys.argv[1], sys.argv[2]
LOG = open(f'{N}_t.txt', 'w')
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
RES = []
def ok(n, c, d=''):
    RES.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:400] if d != '' else ''))
for f in (f'/tmp/ser-{N}.sock', f'/tmp/qmp-{N}.sock'):
    try: os.unlink(f)
    except OSError: pass
env = dict(os.environ, SERIAL=f'-serial unix:/tmp/ser-{N}.sock,server,nowait')
subprocess.run(f'sed "s#^ISO=.*#ISO={ISO}#" {QT}/boot.sh > boot120.sh', shell=True)
subprocess.Popen(['setsid', 'bash', os.path.abspath('boot120.sh'), N, 'bios', 'cd'], env=env, cwd=QT)
time.sleep(2)
import guest
from guest import Guest
from qmp import QMP
q = QMP(f'/tmp/qmp-{N}.sock')
class Share:
    def __init__(s, *a): pass
    def __getattr__(s, k): return getattr(q, k)
guest.QMP = Share
t0 = time.time(); pressed = False
while time.time() - t0 < 240 and not pressed:
    time.sleep(0.4)
    try:
        q.shot(os.path.abspath(f'{N}_bm.png')); im = Image.open(f'{N}_bm.png').convert('RGB')
        if im.size == (720, 400) and im.getpixel((10, 200))[2] > 150 and im.getpixel((10, 200))[0] < 20:
            q.key('d'); time.sleep(0.7); q.key('ret'); pressed = True; out('boot menu: diagnostic start')
    except Exception: pass
g = Guest(N)
def w(c, t=420):
    e = time.time() + t
    while time.time() < e:
        try:
            if g.js(c, 20) is True: return True
        except Exception: pass
        time.sleep(3)
    return False
UP = "typeof rd=='function'&&!!RD&&typeof browser=='function'"
CLEAN = "try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});window.netUp=()=>true;1"
def shot(name):
    q.shot(os.path.abspath(name)); return Image.open(name).convert('RGB')
VIEW = [1280, 800]
def px(im, x, y):
    """a pixel of the desktop page (its CSS pixels) on the screen: the page may be scaled onto the screen"""
    W, H = im.size; k = min(W / VIEW[0], H / VIEW[1]); ox = (W - VIEW[0] * k) / 2; oy = (H - VIEW[1] * k) / 2
    return im.getpixel((int(ox + x * k), int(oy + y * k)))
def at(x, y, btn='left'):
    im = shot(f'{N}_at.png'); W, H = im.size; k = min(W / VIEW[0], H / VIEW[1]); ox = (W - VIEW[0] * k) / 2; oy = (H - VIEW[1] * k) / 2
    q.at(ox + x * k, oy + y * k, btn, W=W, H=H)
green = lambda p: p[1] > 150 and p[0] < 60 and p[2] < 60
def fh():
    return g.js("JSON.stringify((()=>{const r=__W.body.querySelector('.fh').getBoundingClientRect();return[r.left,r.top,r.right,r.bottom]})())")

ok('desktop up', w(UP))
g.js(CLEAN)
out(g.sh('cat /usr/share/rudolf/VERSION; cat /run/rudolf/desktop-url; ls /usr/share/rudolf/www/iwa/', 20))
# --- the one-time move into the shell (by itself, shortly after start-up, once the databases are mirrored)
st = g.js("rd('shell').then(s=>JSON.stringify(s))", 30); out('shell status:', st)
e = time.time() + 420; active = False
while time.time() < e:
    a = g.sh('cat /run/rudolf/desktop-url 2>/dev/null', 15)
    if 'isolated-app://' in a: active = True; break
    time.sleep(8)
if not active:
    out('no move by itself yet:', g.js("rd('shell').then(s=>JSON.stringify(s))", 30), '- asking for it')
    g.js("rdPost('shell/switch').then(()=>1)", 20)
    e = time.time() + 240
    while time.time() < e and 'isolated-app://' not in g.sh('cat /run/rudolf/desktop-url 2>/dev/null', 15): time.sleep(8)
ok('the desktop moved into the shell', 'isolated-app://' in g.sh('cat /run/rudolf/desktop-url', 15), g.sh('cat /run/rudolf/desktop-url; tail -5 /run/rudolf/session.log', 20))
ok('the desktop is up inside the shell, with real tabs', w(UP + "&&!!SHELL&&typeof rtOn=='function'&&rtOn()", 300), g.js('JSON.stringify([!!window.SHELL,window.SHELL_CAPS,location.href])'))
g.js(CLEAN)
VIEW = g.js('[innerWidth,innerHeight]') or VIEW; out('desktop size', VIEW)
g.js("window.__mark=1;1")

# --- a website in a real tab (a server inside the guest)
g.sh("mkdir -p /tmp/www && cd /tmp/www && printf '<title>Green</title><body style=\"margin:0;background:rgb(0,200,0);height:100vh\"><p>green</p>' > green.html"
     " && printf '<script>if(location.search==\"?set\")document.cookie=\"sid=alice; max-age=86400; path=/\";document.title=\"cookie:\"+(document.cookie||\"none\")</script><body style=\"margin:0;background:rgb(0,200,0);height:100vh\">' > cookie.html"
     " && printf '<title>FS</title><body style=\"margin:0;background:rgb(0,200,0);height:100vh\"><button id=b style=\"position:absolute;left:20px;top:20px;width:200px;height:80px\" onclick=\"document.documentElement.requestFullscreen()\">Full screen</button>' > fs.html"
     " && (busybox httpd -p 8099 -h /tmp/www &); sleep 1", 20)
g.js("wins.slice().forEach(x=>x.close());browser('http://localhost:8099/green.html');window.__W=wins[wins.length-1];__W.el.style.left='60px';__W.el.style.top='40px';1")
ok('a website opens in a real tab', w("RT.size==1&&__W.body.querySelector('.otab.on').title=='Green'", 60), g.js("JSON.stringify([RT.size,__W.body.querySelector('.otab.on').title,__W.body.querySelector('.stat').textContent])"))
time.sleep(1.5)
r = json.loads(fh()); im = shot(f'{N}_tab.png')
p = [px(im, (r[0] + r[2]) / 2, (r[1] + r[3]) / 2), px(im, r[0] + 4, r[3] - 4), px(im, r[0] + 4, r[1] - 12)]
ok('the page is shown over the tab area of Oracle', green(p[0]) and green(p[1]) and not green(p[2]), p)

# --- a cookie set by a site stays (it is the site's own, first-party cookie now)
g.js("__W.body.querySelector('.ad').value='http://localhost:8099/cookie.html?set';__W.body.querySelector('.ad').dispatchEvent(new KeyboardEvent('keydown',{key:'Enter'}));1")
ok('a site sets its cookie', w("__W.body.querySelector('.otab.on').title=='cookie:sid=alice'", 30), g.js("__W.body.querySelector('.otab.on').title"))
g.js("__W.close();browser('http://localhost:8099/cookie.html');window.__W=wins[wins.length-1];__W.el.style.left='60px';__W.el.style.top='40px';1")
ok('...and still has it in a new Oracle window', w("__W.body.querySelector('.otab.on').title=='cookie:sid=alice'", 30), g.js("__W.body.querySelector('.otab.on').title"))

# --- a download goes through Oracle's checks: small ones into C:\DOWNLOADS, large ones stay on the disk (1.2.1)
g.sh("cd /tmp/www && printf 'hello from the guest' > notes.txt && head -c 31457280 /dev/zero > big.mp4 && printf '<title>Files</title><body style=\"margin:0;background:rgb(0,200,0);height:100vh\"><a id=f href=\"/notes.txt\" download>notes</a><br><br><br><a id=b href=\"/big.mp4\" download>big</a>' > files.html", 15)
g.js("__W.body.querySelector('.ad').value='http://localhost:8099/files.html';__W.body.querySelector('.ad').dispatchEvent(new KeyboardEvent('keydown',{key:'Enter'}));1")
w("__W.body.querySelector('.otab.on').title=='Files'", 30); time.sleep(1.5)
r = json.loads(fh())
at(r[0] + 20, r[1] + 10); time.sleep(3)
ok('a download from a real tab lands in C:\\DOWNLOADS', w("!!(fs['C:'].DOWNLOADS&&fs['C:'].DOWNLOADS['NOTES.TXT']=='hello from the guest')", 20), g.js("JSON.stringify(DLS.slice(-1))"))
ok('...and not left in the Downloads folder on the disk', 'notes' not in g.sh('ls -a /home/rudolf/Downloads /home/rudolf/Downloads/.incoming 2>/dev/null', 15))
r = json.loads(fh())
at(r[0] + 15, r[1] + 58); time.sleep(3)
ok('a 30 MB download from a real tab is kept on the disk', w("(()=>{const v=fs['C:'].DOWNLOADS&&fs['C:'].DOWNLOADS['BIG.MP4'];const i=v&&dlRefInfo(v);return !!i&&i.s==31457280&&i.av=='part'})()", 60), g.js("JSON.stringify(DLS.slice(-1))"))
ok('...in /home/rudolf/Downloads, with nothing left in the incoming folder', '31457280' in g.sh('stat -c %s /home/rudolf/Downloads/big.mp4', 15) and g.sh('ls /home/rudolf/Downloads/.incoming | wc -l', 15).strip().endswith('0'))
ok('...and it opens from the disk', w("(()=>{openPath(['C:','DOWNLOADS','BIG.MP4']);return true})()", 5) and w("!!document.querySelector('.win video')&&document.querySelector('.win video').src.includes('/dlf/')", 20))
g.js("document.querySelectorAll('.win').forEach(x=>{const w=wins.find(w=>w.el===x);if(w&&w!==__W)w.close()});1")
ok('the web frames extension is left out with the shell', '/run/rudolf/frames' not in g.sh("ps -o args | grep '[r]udolf-kiosk' | head -1 | cut -c1-3000", 15))
nw = g.js('wins.length')
g.js("window.open('http://localhost:8099/green.html');1"); time.sleep(2)
ok('a new browser window asked for by the desktop opens in Oracle', g.js('wins.length') == nw + 1 and g.sh("ps -o args | grep -c '[c]lass=RudolfWeb'", 15).strip().splitlines()[-1:] == ['0'], g.js('JSON.stringify(wins.map(w=>w.tb.title))'))
g.js("wins[wins.length-1].close();1")

# --- Ctrl+T and Ctrl+W with QEMU's keyboard: Chromium would act on them itself (closing the desktop's window);
#     rudolf-display hands them over as Ctrl+F21 / Ctrl+F20, and Oracle opens and closes tabs
r = json.loads(fh())
at((r[0] + r[2]) / 2, (r[1] + r[3]) / 2); time.sleep(1)
q.key('ctrl', 't'); time.sleep(1.5)
ok('Ctrl+T inside a website opens an Oracle tab', g.js("__W.body.querySelectorAll('.otab').length") == 2, g.js("__W.body.querySelectorAll('.otab').length"))
q.key('ctrl', 'w'); time.sleep(1.5)
ok('Ctrl+W in Oracle closes the tab', g.js("__W.body.querySelectorAll('.otab').length") == 1, g.js("__W.body.querySelectorAll('.otab').length"))
at((r[0] + r[2]) / 2, (r[1] + r[3]) / 2); time.sleep(1)
q.key('ctrl', 'w'); time.sleep(2)
ok('Ctrl+W inside a website closes its Oracle window', g.js("wins.includes(__W)") is False, g.js("wins.length"))
ok('...and never the desktop (no restart)', g.js("window.__mark===1") is True and 'shell was closed' not in g.sh('cat /run/rudolf/session.log', 20), g.sh("grep -c 'shell was closed' /run/rudolf/session.log", 15))
q.key('ctrl', 'q'); time.sleep(2)
ok('Ctrl+Q does not quit the desktop', g.js("window.__mark===1") is True)

# --- pointer capture for games: a click asks the shell to capture the pointer; QEMU's relative mouse moves arrive
g.js("wins.slice().forEach(x=>x.close());1"); time.sleep(0.5)   # nothing over the middle of the screen
g.js("window.__pl={moves:0,dx:0,dy:0,buttons:[],locked:[],result:null};window.__plh=async()=>{if(__pl.result!==null)return;__pl.result='asked';__pl.result=await shellLock({move:(x,y)=>{__pl.moves++;__pl.dx+=x;__pl.dy+=y},button:(b,d)=>__pl.buttons.push(b+(d?'v':'^')),change:l=>__pl.locked.push(l)},true)};document.addEventListener('mousedown',__plh,{capture:true});1")
at(VIEW[0] / 2, VIEW[1] / 2); time.sleep(1.5)
for i in range(10): q.mouse(8, -4); time.sleep(0.05)
time.sleep(0.8)
q.click('right'); time.sleep(0.8)
pl = g.js('JSON.stringify(__pl)'); out('pointer capture:', pl); out('the shell saw:', g.js("JSON.stringify(SHELL_LOG.filter(l=>l.includes(' move ')).slice(-14))"))
plj = json.loads(pl) if isinstance(pl, str) else {}
ok('a game can capture the pointer through the shell', plj.get('result') is True and plj.get('locked', [None])[0] is True, pl)
ok('...and gets the mouse movement and buttons', plj.get('moves', 0) >= 3 and plj.get('dx', 0) > 0 and plj.get('dy', 0) < 0 and '2v' in plj.get('buttons', []), pl)
q.key('esc'); time.sleep(1)
ok('Esc releases the pointer', g.js('JSON.stringify(__pl.locked)') and 'false' in g.js('JSON.stringify(__pl.locked)'), g.js('JSON.stringify(__pl.locked)'))
g.js("document.removeEventListener('mousedown',__plh,{capture:true});shellUnlock();1")

# --- fullscreen from a website (Controlled Frame has no fullscreen of its own: the stand-in)
g.js("browser('http://localhost:8099/fs.html');window.__W=wins[wins.length-1];__W.el.style.left='60px';__W.el.style.top='40px';1")
w("__W.body.querySelector('.otab.on').title=='FS'", 30); time.sleep(1.5)
r = json.loads(fh())
at(r[0] + 120, r[1] + 60); time.sleep(2)
im = shot(f'{N}_fs.png')
ok('a website can go fullscreen', green(px(im, VIEW[0] - 10, VIEW[1] - 10)) and green(px(im, VIEW[0] - 10, 300)), [px(im, VIEW[0] - 10, VIEW[1] - 10), px(im, VIEW[0] - 10, 300)])
q.key('esc'); time.sleep(1.5)
im = shot(f'{N}_fs_exit.png')
ok('Esc brings it back into its window', not green(px(im, VIEW[0] - 10, VIEW[1] - 10)) and green(px(im, r[0] + 300, r[3] - 20)), [px(im, VIEW[0] - 10, VIEW[1] - 10), px(im, r[0] + 300, r[3] - 20)])
g.js("__W.close();1")

# --- Task Manager (1.2): the real system's processes, services, kernel and hardware; ending a process; the report
g.js("browser('http://localhost:8099/green.html');window.__W=wins[wins.length-1];1"); time.sleep(3)
pr = g.js("rd('processes').then(p=>JSON.stringify({n:p.items.length,tags:[...new Set(p.items.map(x=>x.tag))],launch:p.items.some(x=>x.name=='rudolf-launch'&&x.prot),rudolfd:p.items.some(x=>x.name=='rudolfd'&&x.prot),pages:p.pages||null}))", 30)
out('processes:', pr)
pj = json.loads(pr) if isinstance(pr, str) else {}
ok('rudolfd lists the processes, tagged by kind', pj.get('n', 0) > 30 and all(t in pj.get('tags', []) for t in ('desktop', 'web', 'gpu', 'kernel', 'system')) and pj.get('launch') and pj.get('rudolfd'), pr)
time.sleep(4)
pg = g.js("rd('processes').then(p=>JSON.stringify(p.pages))", 30); out('page memory:', pg)
ok('...and the memory of the desktop page and the real tab', isinstance(pg, str) and '127.0.0.1:8080' in pg and 'localhost:8099' in pg, pg)
sy = g.js("rd('system').then(s=>JSON.stringify({rel:s.kernel.release,boot:s.kernel.boot,mods:s.kernel.modules.split('\\n').length,svc:s.services,gpu:s.gpu,disp:s.display,cpu:s.cpu.model,input:s.input.split('\\n').length}))", 40)
out('system:', sy)
sj = json.loads(sy) if isinstance(sy, str) else {}
ok('rudolfd describes the kernel, the services and the hardware', sj.get('rel') and 'mods' in sj and sj.get('svc', {}).get('display') and sj.get('svc', {}).get('x') and sj.get('svc', {}).get('shell') and sj.get('cpu') and sj.get('input', 0) >= 2, sy)
sp = g.sh("sleep 900 >/dev/null 2>&1 & echo PID=$!", 15).split('PID=')[-1].split()[0]
g.js("window.__el=elevate;elevate=async()=>true;launch('taskmgr');window.__T=wins[wins.length-1];__T.body.querySelector(\"[data-t='procs']\").click();1"); time.sleep(4)
ok('Task Manager shows the processes', w("__T.body.querySelectorAll('tr[data-pid]').length>30", 30), g.js("__T.body.querySelectorAll('tr[data-pid]').length"))
g.js(f"(()=>{{const f=__T.body.querySelector('.tmf');f.value='{sp}';f.dispatchEvent(new Event('input'));return 1}})()"); time.sleep(1)
g.js("__T.body.querySelector('tr[data-pid]').click();__T.body.querySelector('.tme').click();1"); time.sleep(3)
ok('End process ends it (an administrator approved)', sp not in g.sh("pgrep -x sleep | tr '\\n' ' '", 15), sp)
r = g.js("rdPost('proc/end?pid='+'%s').then(()=>'ended',e=>e.message)" % g.sh("pgrep -x rudolfd | head -1", 15).strip().splitlines()[-1], 20)
ok('...but not one Rudolf OS needs', 'cannot be ended' in str(r), r)
g.js("__T.body.querySelector(\"[data-t='system']\").click();1")
ok('the System tab shows the services and the kernel', w("['Services','Kernel','Running','the shell'].every(k=>__T.body.textContent.includes(k))", 30))
g.js("__T.body.querySelector(\"[data-t='hw']\").click();1")
ok('the Hardware tab shows the graphics card and storage', w("__T.body.textContent.includes('graphics (')&&__T.body.textContent.includes('Storage')", 30), g.js("__T.body.textContent.slice(0,400)"))
g.shot(f'{N}_taskmgr.png')
rp = g.js("sysReport('test report from the VM').then(r=>JSON.stringify(r))", 60)
rj = json.loads(rp) if isinstance(rp, str) else {}
out(rj.get('text', '')[:1500])
ok('the system report describes the computer now', all(k in rj.get('text', '') for k in ('Rudolf OS: 1.2.0', 'Started from: BIOS, cd, live', 'Desktop: shell 1.2.0', 'Display: ', 'Services: display running', '== Computer', 'Graphics: ', '== Kernel', '== Processes')), rj.get('summary', ''))
ok('...without network addresses', not __import__('re').search(r'\b\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\b', rj.get('text', '')))
g.js("elevate=__el;wins.slice().forEach(x=>x.close());1")
out(g.sh("tail -15 /run/rudolf/session.log", 20))
out('RESULT %d/%d' % (sum(RES), len(RES)))
subprocess.run(['pkill', '-f', f'qmp-{N}.sock'])
