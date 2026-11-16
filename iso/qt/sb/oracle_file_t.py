# Rudolf OS 1.6.0 in QEMU: Oracle's file:/// pages on a real release (the Vivobook, 2026-11-15: Welcome, README and an
# .HTML file from File Explorer showed a blank white page while about:home and about:files showed). The newest beta
# (or the release in REL-DIR) starts from slot a as in t160, an account signs in, and Oracle opens about:home,
# file:///C:/README.TXT, file:///C:/WELCOME.HTM and a page in the account's Documents with a tel: link; for each page
# what the tab's frame holds (srcdoc, sandbox, what is on top of it), a copy of the page in a frame of the same kind
# that says whether its text is there, the screen's pixels where the frame is, the page's errors, the shell's notes
# and rudolfd's page log. Nothing is published.
#   python3 oracle_file_t.py REL-DIR   (QEMU as for t160; root; QEMU_ACCEL=kvm where it can)
import os, sys, time, re, subprocess, shutil, struct, json
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1])
LOG = open('oracle_file.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:1500] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 2000; DRV = os.path.abspath('intof.img'); MEM = int(os.environ.get('MEM', '3072'))
out('release', mv('serial'), mv('version'), 'chromium', mv('chromium'))

def make_drive(path):
    start = 2048
    with open(path, 'wb') as f: f.truncate((start + DMB * 2048 + 2048) * 512); f.seek(510); f.write(b'\x55\xaa')
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc')
    d = f'{T}/os/system/a'; os.makedirs(d)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs', 'manifest.txt', 'manifest.sig'): shutil.copy(f'{REL}/{n}', f'{d}/{n}')
    open(f'{d}/RELEASE', 'w').write(f"serial {mv('serial')}\nversion {mv('version')}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n")
    open(f'{d}/complete', 'w').write('1\n')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{DMB}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'r+b') as f:
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, DMB * 2048))
        f.seek(start * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)

def boot(tag):
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolfof',
             '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1']
    os.makedirs('of' + tag, exist_ok=True); open(f'of{tag}/vars.fd', 'ab').close()   # (sbvm copies Secure Boot's variables otherwise; not used without it)
    vm = sbvm.VM('of' + tag, '/nonexistent', [], secure=False, mem=MEM, extra=extra)
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    time.sleep(30); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    def js(e, t=90):
        for _ in range(5):
            r = g.js(e, t)
            if not (isinstance(r, tuple) and (r[0] == 'ERR' or (r[0] == 'EXC' and 'ReferenceError' in str(r[1])))): return r
            time.sleep(15)
        return r
    end = time.time() + 600
    while up and time.time() < end and js("typeof acctNew=='function'&&typeof userLogin=='function'&&typeof browser=='function'", 20) is not True: time.sleep(5)
    js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};return 1})()")
    return vm, g, up, js

def signin(js, n, pw, secs=900):
    r = js(f"(()=>{{if(window.__si=='wait')return 'already';const a=ACCTS.find(x=>x.n=={json.dumps(n)});if(!a)return 'no account';window.__si='wait';"
           f"(async()=>{{try{{if(!(await pwCheck(a,{json.dumps(pw)}))){{__si='bad';return}}if(USER)userLogout();const L=document.getElementById('login');USER=a;"
           f"await userLogin(a,{json.dumps(pw)});lastInput=Date.now();if(L)L.style.display='none';__si='in '+USER.n}}catch(e){{__si='error '+e.message}}}})();return 'started'}})()", 60)
    if r not in ('started', 'already'): return r
    end = time.time() + secs
    while time.time() < end:
        s = js("window.__si", 30)
        if isinstance(s, str) and s != 'wait': return s
        time.sleep(5)
    return 'still signing in'

# what the shown tab of the newest Oracle window holds; a copy of its page in a frame of the same kind (sandbox
# allow-scripts, srcdoc) with a probe that posts back what it sees; what is on top of the frame
PROBE = r"""(async()=>{const W=wins.filter(w=>w.body&&w.body.querySelector('.otabs')).pop();if(!W)return{err:'no Oracle window'};
const frs=[...W.body.querySelectorAll('.fh iframe')],fr=frs.find(f=>f.style.display!='none');const st=(W.body.querySelector('.stat')||{}).textContent,ad=(W.body.querySelector('.ad')||{}).value;
const o={title:W.el.innerText.split('\n')[0].slice(0,80),ad,st,frames:frs.map(f=>({d:f.style.display,sb:f.getAttribute('sandbox'),src:f.getAttribute('src'),sd:f.hasAttribute('srcdoc')?f.getAttribute('srcdoc').length:-1,head:(f.getAttribute('srcdoc')||'').slice(0,160)})),shell:typeof SHELL!='undefined'&&!!SHELL,shellVer:typeof SHELL_VER!='undefined'?SHELL_VER:'',rt:typeof RT!='undefined'?RT.size:-1,engine:RD&&RD.engine,dpr:devicePixelRatio,iw:innerWidth,ih:innerHeight,errs:(window.__ofErr||[]).slice(-10)};
if(!fr)return o;const r=fr.getBoundingClientRect();o.rect=[r.x,r.y,r.width,r.height];const cs=getComputedStyle(fr);o.css={vis:cs.visibility,op:cs.opacity,z:cs.zIndex,filter:cs.filter,zoom:cs.zoom,bg:cs.backgroundColor};
const top=document.elementFromPoint(r.x+r.width/2,r.y+r.height/2);o.top=top===fr?'the frame':top?(top.tagName+'.'+top.className+'#'+top.id):null;
let cw=null;try{cw=fr.contentWindow;o.cwLen=cw?cw.length:-1}catch(e){o.cwErr=e.message}try{o.cdoc=fr.contentDocument?'reachable':'null'}catch(e){o.cdoc='err '+e.message}
const html=fr.getAttribute('srcdoc');if(html==null)return o;
o.probe=await new Promise(res=>{const p=document.createElement('iframe');p.setAttribute('sandbox','allow-scripts');p.style.cssText='position:fixed;left:0;top:0;width:300px;height:200px;opacity:0;pointer-events:none';
 const k='of'+Math.random().toString(36).slice(2);const on=e=>{if(e.source===p.contentWindow&&e.data&&e.data[k]){removeEventListener('message',on);res(e.data[k]);p.remove()}};addEventListener('message',on);
 setTimeout(()=>{removeEventListener('message',on);p.remove();res('no answer from the copy')},8000);
 p.srcdoc=html+'<script>addEventListener("load",function(){var b=document.body;parent.postMessage({'+JSON.stringify(k)+':{text:b?b.innerText.length:-1,first:b?b.innerText.slice(0,80):"",h:document.documentElement.scrollHeight,url:location.href,bg:b?getComputedStyle(b).backgroundColor:""}},"*")})<'+'/script>';document.body.appendChild(p)});
return o})()"""

def jsbig(js, e, t=90):
    """a long expression in pieces: the serial line takes at most 4095 characters"""
    js("window.__ofS='';1", 30)
    for i in range(0, len(e), 1200): js(f"window.__ofS+={json.dumps(e[i:i + 1200])};1", 30)
    return js("(0,eval)(window.__ofS)", t)

def pixels(png, rect, iw):
    try:
        from PIL import Image
    except Exception as e: return {'err': 'no PIL: ' + str(e)}
    im = Image.open(png).convert('RGB'); sc = im.width / iw
    x, y, w, h = [int(v * sc) for v in rect]; x0, y0 = max(0, x + 4), max(0, y + 4); x1, y1 = min(im.width, x + w - 4), min(im.height, y + h - 4)
    if x1 <= x0 or y1 <= y0: return {'err': 'empty rect', 'size': im.size}
    c = im.crop((x0, y0, x1, y1)); px = list(c.getdata()); n = len(px)
    nw = sum(1 for p in px if min(p) < 235)   # not white
    return {'size': im.size, 'notWhite': round(nw / n, 4), 'colors': len(set(px[::7]))}

CALL = "<!doctype html><title>Call test</title><body><h1>Call test</h1><p><a id=c href=\"tel:5551234567\">Call this number</a></p></body>"
make_drive(DRV)
vm = None
try:
    vm, g, up, js = boot('a'); sh = lambda c, t=60: g.sh(c, t)
    ok('1. the release starts', up, 'not up')
    if not up: raise SystemExit
    r = js("(async()=>{ACCTS.length=0;const a=await acctNew('john','j1','admin');ACCTS.push(a);saveUser();return a.id})()", 120); out('account', r)
    out('sign-in:', signin(js, 'john', 'j1'))
    # the shell: the desktop moves into it by itself after the start (1.2's one-time move); wait for its hello
    end = time.time() + 900
    while time.time() < end and js("typeof SHELL!='undefined'&&!!SHELL", 20) is not True: time.sleep(10)
    if js("typeof SHELL!='undefined'&&!!SHELL", 20) is not True and js("!!USER", 20) is not True:
        out('sign-in again after the move:', signin(js, 'john', 'j1'))
    out('engine/shell:', js("({engine:RD&&RD.engine,shell:typeof SHELL!='undefined'&&!!SHELL,ver:typeof SHELL_VER!='undefined'?SHELL_VER:'',ua:navigator.userAgent,user:USER&&USER.n})", 30))
    ok('2. the desktop is in the shell (Controlled Frame), as on the Vivobook', js("typeof SHELL!='undefined'&&!!SHELL", 20) is True)
    jsbig(js, "(()=>{window.__ofErr=[];addEventListener('error',e=>__ofErr.push('error '+e.message+' '+(e.filename||'')+':'+e.lineno));addEventListener('unhandledrejection',e=>__ofErr.push('rejection '+String(e.reason&&e.reason.message||e.reason)));"
       "const w=console.warn,er=console.error;console.warn=function(){__ofErr.push('warn '+[].map.call(arguments,String).join(' ').slice(0,300));return w.apply(console,arguments)};console.error=function(){__ofErr.push('console.error '+[].map.call(arguments,String).join(' ').slice(0,300));return er.apply(console,arguments)};return 1})()")
    r = js(f"(async()=>{{let d=fs;for(const k of homePath('DOCUMENTS')){{if(!isDir(d[k]))d[k]={{}};d=d[k]}}d['CALL-TEST.HTML']={json.dumps(CALL)};fsSave();try{{fsFlush()}}catch(e){{}};return homePath('DOCUMENTS').join('/')}})()", 60); out('documents:', r)
    DOCS = r if isinstance(r, str) else 'C:/USERS/JOHN/DOCUMENTS'
    pages = [('about:home', 'Oracle'), ('about:files?p=C:', 'Index of C:'), ('file:///C:/README.TXT', 'README.TXT'), ('file:///C:/WELCOME.HTM', 'Welcome to Oracle'), (f'file:///{DOCS}/CALL-TEST.HTML', 'Call this number')]
    res = {}
    for i, (u, want) in enumerate(pages):
        js(f"(()=>{{wins.slice().forEach(w=>{{if(w.body&&w.body.querySelector('.otabs'))w.close()}});browser({json.dumps(u)});return 1}})()", 30)
        time.sleep(6)
        o = jsbig(js, PROBE, 60); png = vm.shot(f'p{i}'); px = pixels(png, o['rect'], o['iw']) if isinstance(o, dict) and o.get('rect') else {}
        out(f'--- {u}'); out(json.dumps(o)[:3000]); out('pixels', px)
        res[u] = (o, px, want)
    jl = sh("busybox wget -q -O- http://127.0.0.1:9222/json/list 2>&1 | grep -E '\"(type|url)\"' | head -40", 30); out('devtools targets:\n' + jl)
    out('shell notes:', js("typeof SHELL_LOG!='undefined'?SHELL_LOG.slice(-20):null", 30))
    out('page log:\n' + sh("tail -n 40 /run/rudolf/page.log 2>&1", 30))
    for k, (u, (o, px, want)) in enumerate(res.items()):
        pr = o.get('probe') if isinstance(o, dict) else None
        seen = isinstance(px, dict) and px.get('notWhite', 0) > 0.01
        ok(f'{3 + k}. {u}: the page is drawn in Oracle\'s frame (not a blank white page), its text there', seen and isinstance(pr, dict) and pr.get('text', 0) > 0 and want in (pr.get('first', '') + json.dumps(o)), [px, pr, o.get('top') if isinstance(o, dict) else o])
    # the phone-number test: clicking the tel: link asks how to call it
    js(f"(()=>{{wins.slice().forEach(w=>{{if(w.body&&w.body.querySelector('.otabs'))w.close()}});browser({json.dumps(pages[4][0])});return 1}})()", 30); time.sleep(6)
    o = jsbig(js, PROBE, 60)
    if isinstance(o, dict) and o.get('rect'):
        x, y, w, h = o['rect']; sc = 1
        out('click the tel: link at', x + 60, y + 95)
        g.q.cmd('input-send-event', events=[{'type': 'abs', 'data': {'axis': 'x', 'value': int((x + 60) / o['iw'] * 32767)}}, {'type': 'abs', 'data': {'axis': 'y', 'value': int((y + 95) / o['ih'] * 32767)}}]); time.sleep(0.5)
        g.q.click(); time.sleep(4); vm.shot('tel')
    o = js("[...document.querySelectorAll('.rmodal [role=dialog]')].map(d=>d.getAttribute('aria-label')+': '+d.innerText.slice(0,120)).concat(wins.slice(-3).map(w=>w.el.innerText.split('\\n')[0].slice(0,60)))", 30); out('windows after the tel: click:', o)
    ok('8. the tel: link clicked: the desktop asks how to call it (Call a phone number, 5551234567)', isinstance(o, list) and any('Call a phone number' in str(t) and '5551234567' in str(t) for t in o), o)
finally:
    try: vm and vm.stop()
    except Exception: pass
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
