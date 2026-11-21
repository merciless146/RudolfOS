# Rudolf OS 1.6.1 (docs/plan-1.6.1.md Part 9) in QEMU: printing through the print queue (rudolf-printq) on the real release
# (the newest stable or beta, or REL-DIR) with this tree's files put in place (P161_FIX: rudolfd, rudolf-rootd and the root
# helpers as fix161.sh puts them, rudolf-print, rudolf-printq, the page, the list of listeners; optionally CUPS's lp with
# its libraries in cups/; on an extra disk, iso/qt/sb/fix161p.sh), against virtual IPP Everywhere printers on the host
# (CUPS's ippeveprinter: one that takes PDF, one that takes only raster; the repository's fakeprinter.py when ippeveprinter
# is not there or does not answer, said so), reached through QEMU's user network (guestfwd: 10.0.2.100 and 10.0.2.101,
# port 631, each connection through socat; 10.0.2.102 a printer that takes the connection and never answers). QEMU has no USB printer to emulate: the
# IPP path only (the USB kinds are the host tests' and the owner's).
#   S. Standard (Chromium), an account signed in: a PDF printed as the PDF Viewer prints it (as it is) and a page as
#      WordPro prints it (made into a PDF by print/pdf), to the PDF printer; the page to the raster printer (the print
#      dialog's own raster, pdf.js); each one's file on the host checked (its text; PWG raster with ink); print/jobs: the
#      account's own (its Linux user), sent; Settings › Devices › Print queue lists them
#   U. users: a Linux program of the account's own user prints through the socket (its job the account's); the desktop's
#      user's jobs (another user) are shown without their names, and neither the page nor the account's user can cancel
#      them; their own user can; lp (CUPS's client, as Debian's apps print) as the account's user, when cups/ is there
#   Q. the queue runs as "print", its socket 0666, no TCP port of its own; the self-test's listeners line passes
#   N. Native: the same from an app window (WebKit, as Lite's page is): a page printed, print/jobs, Settings' queue
#   python3 p161_t.py REL-DIR    (SB_PARTS, QEMU, QEMU_ACCEL as for t160; root)
import os, sys, time, re, subprocess, shutil, struct, json, base64, socket, threading
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); FIX = os.environ.get('P161_FIX', '')
LOG = open('p161.txt', 'w'); R = []; T0 = time.time()
def out(*a):
    s = ' '.join(str(x) for x in a); s = f'[{int(time.time() - T0):5d}s] ' + s; print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:1500] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
DMB = 3000; DRV = os.path.abspath('p161.img'); MEM = int(os.environ.get('MEM', '4096')); W = os.path.abspath('w161p')
shutil.rmtree(W, ignore_errors=True); os.makedirs(W)
# ---------------------------------------------------------------- the printers, on the host
SPOOL1, SPOOL2 = os.path.join(W, 'pdfjobs'), os.path.join(W, 'rasjobs'); os.makedirs(SPOOL1); os.makedirs(SPOOL2)
PROCS = []
def answers(port):
    try: s = socket.create_connection(('127.0.0.1', port), 2); s.close(); return True
    except OSError: return False
KIND = 'ippeveprinter'
if shutil.which('ippeveprinter'):
    PROCS.append(subprocess.Popen(['ippeveprinter', '-p', '8631', '-n', 'localhost', '-d', SPOOL1, '-k', '-f', 'application/pdf,image/pwg-raster', '-F', 'application/pdf', '-m', 'PDF Printer', 'QEMU PDF'], stdout=open(f'{W}/ipp1.log', 'w'), stderr=subprocess.STDOUT))
    PROCS.append(subprocess.Popen(['ippeveprinter', '-p', '8632', '-n', 'localhost', '-d', SPOOL2, '-k', '-f', 'image/pwg-raster,image/urf', '-F', 'image/pwg-raster', '-m', 'Raster Printer', 'QEMU Raster'], stdout=open(f'{W}/ipp2.log', 'w'), stderr=subprocess.STDOUT))
    time.sleep(3)
if not (answers(8631) and answers(8632)):
    out('NOTE ippeveprinter', 'is not installed' if not shutil.which('ippeveprinter') else 'did not answer (' + open(f'{W}/ipp1.log').read().strip()[-200:] + ')', ': fakeprinter.py stands in')
    for p in PROCS: p.kill()
    KIND = 'fakeprinter.py'; FP = os.path.join(QT, 'fakeprinter.py')
    PROCS.append(subprocess.Popen([sys.executable, FP, SPOOL1, '8631', 'pdf'], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT))
    PROCS.append(subprocess.Popen([sys.executable, FP, SPOOL2, '8632', 'raster'], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)); time.sleep(2)
ok(f'0. the virtual printers answer on the host ({KIND})', answers(8631) and answers(8632))
U1, U2, U3 = 'ipp://10.0.2.100:631/ipp/print', 'ipp://10.0.2.101:631/ipp/print', 'ipp://10.0.2.102:631/ipp/print'
# a PDF of its own (as a PDF the PDF Viewer prints): one page, its text
def mkpdf(text):
    c = b'BT /F1 26 Tf 72 700 Td (' + text.encode() + b') Tj ET'
    objs = [b'<< /Type /Catalog /Pages 2 0 R >>', b'<< /Type /Pages /Kids [3 0 R] /Count 1 >>', b'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>',
            b'<< /Length %d >>\nstream\n' % len(c) + c + b'\nendstream', b'<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>']
    o = b'%PDF-1.4\n'; offs = []
    for i, x in enumerate(objs): offs.append(len(o)); o += b'%d 0 obj\n' % (i + 1) + x + b'\nendobj\n'
    x = len(o); o += b'xref\n0 %d\n0000000000 65535 f \n' % (len(objs) + 1) + b''.join(b'%010d 00000 n \n' % f for f in offs) + b'trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n' % (len(objs) + 1, x)
    return o
PDF_B64 = base64.b64encode(mkpdf('Rudolf PDF Viewer print test')).decode()
open(f'{W}/doc.pdf', 'wb').write(mkpdf('Rudolf Linux user print test'))
def texts(d):
    r = []
    for f in sorted(os.listdir(d)):
        p = os.path.join(d, f)
        if f.endswith('.pdf') or open(p, 'rb').read(4) == b'%PDF': r.append(subprocess.run(['pdftotext', p, '-'], capture_output=True, text=True).stdout)
    return r
def pwg_ink(p):
    b = open(p, 'rb').read()
    if b[:4] != b'RaS2': return None
    h = b[4:4 + 1796]; w, hh = struct.unpack('>II', h[372:380]); bpp = struct.unpack('>I', h[388:392])[0] // 8; i = 4 + 1796; ink = 0; y = 0
    while y < hh and i < len(b):
        rep = b[i]; i += 1; x = 0; line = bytearray()
        while x < w:
            c = b[i]; i += 1
            if c < 128: line += b[i:i + bpp] * (c + 1); i += bpp; x += c + 1
            else: n = 257 - c; line += b[i:i + n * bpp]; i += n * bpp; x += n
        ink += sum(1 for v in line[::bpp] if v < 128) * (rep + 1); y += rep + 1
    return (w, hh, bpp, ink)
# ---------------------------------------------------------------- the drive and the fix disk
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
FIXIMG = ''
if FIX:
    FD = os.path.join(W, 'fix'); shutil.copytree(FIX, FD); shutil.copy(f'{W}/doc.pdf', f'{FD}/doc.pdf')
    # (the native shell started with the tests' views: the release's own program, run from here; RUDOLF_NATIVE_TEST_EVAL)
    open(f'{FD}/rn', 'w').write('#!/bin/sh\nexport RUDOLF_NATIVE_STATE=/tmp/nstate RUDOLF_NATIVE_TEST_EVAL=/tmp/teval\nexec /run/r161x/rudolf-native.rel "$@"\n'); os.chmod(f'{FD}/rn', 0o755)
    FIXIMG = os.path.abspath('p161fix.img'); subprocess.run(['truncate', '-s', '256M', FIXIMG], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'R161FIX', '-d', FD, FIXIMG], check=True)
# (each connection the guest makes to 10.0.2.100-102:631 is a command QEMU starts: socat to the host's printer; sleep for the one
# that never answers)
NET = ['-netdev', 'user,id=u0,restrict=on,guestfwd=tcp:10.0.2.100:631-cmd:socat - TCP:127.0.0.1:8631,guestfwd=tcp:10.0.2.101:631-cmd:socat - TCP:127.0.0.1:8632,guestfwd=tcp:10.0.2.102:631-cmd:sleep 600',
       '-netdev', 'hubport,id=hp1,hubid=0,netdev=u0']
INIT_JS = "(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};return 1})()"
def boot(tag):
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf161',
             '-kernel', f'{REL}/vmlinuz', '-append', 'quiet loglevel=3 consoleblank=0 rudolf.slot=a rudolf.debug=1'] + NET
    if FIXIMG: extra += ['-drive', f'file={FIXIMG},format=raw,if=none,id=fx,readonly=on', '-device', 'nvme,drive=fx,serial=p161fix']
    vm = sbvm.VM('p161' + tag, PARTS, [], secure=False, mem=MEM, extra=extra)
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    time.sleep(30); end = time.time() + 900; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    return vm, g, up
def run(g, c, t=60, show=True):
    f = os.path.join(W, 'c.sh'); open(f, 'w').write(c + '\n'); g.put(f, '/tmp/c161.sh', '644')
    o = g.sh('sh /tmp/c161.sh 2>&1', t)
    if show: out('$', c[:160].replace('\n', ' ; ')); out(o.rstrip()[-2500:])
    return o
def bigjs(g, e, t=90):
    # (an expression longer than the serial console's line: through a file, as cdp-eval's argument from it)
    f = os.path.join(W, 'x.js'); open(f, 'w').write(e); g.put(f, '/tmp/x.js', '644')
    o = g.sh('cdp-eval "$(cat /tmp/x.js)" 2>&1', t); m = re.search(r'\{"id":1.*\}', o)
    if not m: return ('ERR', o[-400:])
    res = json.loads(m.group(0)).get('result', {})
    if 'exceptionDetails' in res: return ('EXC', res['exceptionDetails'].get('exception', {}).get('description', '')[:300])
    return res.get('result', {}).get('value')
def jsfn(g):
    def js(e, t=90):
        for _ in range(5):
            r = g.js(e, t) if len(e) < 1500 else bigjs(g, e, t)
            if not (isinstance(r, tuple) and (r[0] == 'ERR' or (r[0] == 'EXC' and 'ReferenceError' in str(r[1])))): return r
            time.sleep(10)
        return r
    return js
def wait_for(js, expr, secs, every=2):
    end = time.time() + secs; r = None
    while time.time() < end:
        r = js(expr, 60)
        if r is True: return True
        time.sleep(every)
    return r
def files(d, n, secs=90):
    end = time.time() + secs
    while time.time() < end and len(os.listdir(d)) < n: time.sleep(2)
    return sorted(os.listdir(d))
NETUP = ("IP=ip; command -v ip >/dev/null || IP='busybox ip'; for i in $(seq 1 15); do $IP -4 addr show eth0 | grep -q 'inet ' && break; sleep 2; done; "
         "$IP -4 addr show eth0 | grep -q 'inet ' || { $IP addr add 10.0.2.15/24 dev eth0; $IP link set eth0 up; }; $IP -4 addr show eth0 | grep inet; "
         f"for i in $(seq 1 20); do rudolf-print info {U1} 2>/dev/null | grep -q '\"ok\":true' && echo printer-reachable && break; sleep 2; done")
# the page's printing, as the PDF Viewer and WordPro do it (a job's answer as JSON text in window.__pq)
PRINT_JS = r"""(()=>{window.__pq='busy';(async()=>{try{const out={};
 const b=atob('%s'),u=new Uint8Array(b.length);for(let i=0;i<b.length;i++)u[i]=b.charCodeAt(i);const pdf=new Blob([u],{type:'application/pdf'});
 out.pdf=await prJSON('print/job?'+new URLSearchParams({uri:'%s',format:'application/pdf',title:'%s PDF',name:'Host PDF printer'}),pdf);
 const html='<html><body style="font:24px sans-serif"><h1>Rudolf page print test</h1><p>%s</p></body></html>';
 const r=await fetch('/api/print/pdf',{method:'POST',headers:{'X-Rudolf-Token':RD.token},body:html});const page=await r.blob();
 out.page=await prJSON('print/job?'+new URLSearchParams({uri:'%s',format:'application/pdf',title:'%s page',name:'Host PDF printer'}),page);
 if(%d){const info=await prJSON('print/info?uri='+encodeURIComponent('%s'));const plan=prPlan(info,false);out.plan=plan;
  const ras=await prRaster(page,{kind:plan.kind,dpi:plan.dpi,color:plan.color,from:1,to:0,duplex:false,tumble:false,sheetBack:plan.sheetBack,media:'na_letter_8.5x11in'});
  out.raster=await prJSON('print/job?'+new URLSearchParams({uri:'%s',format:plan.format,title:'%s raster',name:'Host raster printer'}),ras)}
 window.__pq=JSON.stringify(out)}catch(e){window.__pq='ERR '+e.message}})();return 1})()"""
def print_js(tag, raster): return PRINT_JS % (PDF_B64, U1, tag, tag, U1, tag, 1 if raster else 0, U2, U2, tag)
JOBS_JS = "(()=>{window.__pj='busy';prJSON('print/jobs').then(j=>window.__pj=JSON.stringify(j)).catch(e=>window.__pj='ERR '+e.message);return 1})()"
CANCEL_JS = "(()=>{window.__pc='busy';prJSON('print/cancel?id=%d').then(j=>window.__pc=JSON.stringify(j)).catch(e=>window.__pc='ERR '+e.message);return 1})()"
def jobs_of(t):
    try: return json.loads(t).get('jobs', [])
    except Exception: return []

make_drive(DRV)
vm = None
try:
    # ------------------------------------------------------------------ S. Standard
    vm, g, up = boot('s'); js = jsfn(g); sh = lambda c, t=60, show=True: run(g, c, t, show)
    ok('S0. the release starts (Standard)', up)
    if not up: raise RuntimeError('did not start')
    sh("for i in $(seq 1 90); do busybox wget -q -O /dev/null http://127.0.0.1:8080/favicon.png && break; sleep 2; done; cat /usr/share/rudolf/VERSION", 240)
    if FIX:
        g.put(os.path.join(HERE, 'fix161.sh'), '/tmp/fix161.sh', '644'); g.put(os.path.join(HERE, 'fix161p.sh'), '/tmp/fix161p.sh', '644')
        o = sh("sh /tmp/fix161p.sh reload", 300)
        ok('S0b. this tree\'s printing in place: rudolfd again, the queue running as "print", the page', 'through rudolf-rootd' in o and re.search(r'queue 666 as uid 952', o), o)
    o = sh(NETUP, 60); ok('S0c. the guest reaches the host\'s printers (QEMU\'s user network)', 'printer-reachable' in o, o)
    # (the page started again by fix161p.sh's reload: signed in once it is up, again should it load once more meanwhile)
    for attempt in range(3):
        end = time.time() + 600
        while time.time() < end and js("typeof acctNew=='function'&&typeof userLogin=='function'&&typeof prJSON=='function'&&typeof ACCTS=='object'", 20) is not True: time.sleep(5)
        time.sleep(30); js(INIT_JS)
        r = js("(async()=>{ACCTS.length=0;const b=await acctNew('boss','b1','admin');ACCTS.push(b);saveUser();return b.id})()", 120); out('account', r)
        r = js("(async()=>{try{const a=ACCTS.find(x=>x.n=='boss');if(USER)userLogout();USER=a;await userLogin(a,'b1');lastInput=Date.now();"
               "const L=document.getElementById('login');if(L)L.style.display='none';return 'in '+USER.n}catch(e){return 'ERR '+e.message}})()", 300)
        out('sign-in (Standard):', r)
        if str(r).startswith('ERR'): continue   # (the page loaded again meanwhile: from the start)
        if wait_for(js, "!!(USER&&USER.disk&&hdOn())", 300) is True: break
    time.sleep(10)
    AU = sh("sed -n 's/^USER=//p' /run/rudolf/acct-session.env", 30, False).strip().split('\n')[-1]
    out('the account\'s Linux user:', AU)
    js(print_js('Standard', True)); r = wait_for(js, "window.__pq!='busy'", 300, 3); res = js("window.__pq"); out('printed:', res)
    try: P = json.loads(res)
    except Exception: P = {}
    ok('S1. a PDF (as the PDF Viewer prints it) and a page (WordPro: print/pdf) to the PDF printer, a page as raster to the other: queued',
       all((P.get(k) or {}).get('queued') for k in ('pdf', 'page', 'raster')), res)
    f1 = files(SPOOL1, 2, 120); f2 = files(SPOOL2, 1, 180); tx = texts(SPOOL1)
    ok('S2. the PDF printer has both: the PDF as it was, the page with its text', any('Rudolf PDF Viewer print test' in t for t in tx) and any('Rudolf page print test' in t for t in tx), (f1, tx))
    ink = pwg_ink(os.path.join(SPOOL2, f2[0])) if f2 else None
    ok('S3. the raster printer has the page as PWG raster made by the print dialog, with ink', ink and ink[3] > 200, (f2, ink, (P.get('plan'))))
    time.sleep(3); js(JOBS_JS); wait_for(js, "window.__pj!='busy'", 60, 2); jl = js("window.__pj"); J = jobs_of(jl)
    mine = [j for j in J if j.get('mine')]
    ok('S4. print/jobs: the three, sent, the signed-in account\'s own (its Linux user)', len(mine) >= 3 and all(j['state'] == 'sent' for j in mine[:3]) and all(j['user'] == AU and AU.startswith('acct') for j in mine[:3]), jl)
    js("(()=>{wins.slice().forEach(w=>w.close());settings('devs');return 1})()"); time.sleep(6)
    rows = js("JSON.stringify([...document.querySelectorAll('.pq61 tr')].map(r=>r.textContent))"); out('Settings › Devices › Print queue:', rows)
    ok('S5. Settings › Devices › Print queue lists them, with their printers', isinstance(rows, str) and 'Standard page' in rows and 'Host raster printer' in rows and 'Sent to the printer' in rows, rows)
    vm.shot('std-queue')
    # ------------------------------------------------------------------ U. users
    o = sh(f"cp /tmp/fx/doc.pdf /tmp/doc.pdf; chmod 644 /tmp/doc.pdf; su -s /bin/sh {AU} -c 'rudolf-printq submit {U1} /tmp/doc.pdf application/pdf' ; echo; "
           f"su -s /bin/sh rudolf -c 'for i in 1 2 3 4 5 6; do rudolf-printq submit {U3} /tmp/doc.pdf application/pdf; done'", 60)
    # (the desktop user's six jobs to the printer that never answers: each waits for the one before it to give up, 15 s,
    # so the last two are still waiting for a minute: those are the ones looked at)
    ids = [int(x) for x in re.findall(r'"job":(\d+)', o)]; out('jobs', ids); ids = ids[:1] + ids[-2:]
    time.sleep(3); js(JOBS_JS); wait_for(js, "window.__pj!='busy'", 60, 2); jl = js("window.__pj"); J = {j['id']: j for j in jobs_of(jl)}
    a = J.get(ids[0], {}) if ids else {}; d = [J.get(i, {}) for i in ids[1:3]]
    ok('U1. a program of the account\'s own Linux user prints through the queue: its job is the account\'s', a.get('mine') and a.get('user') == AU, (ids, a))
    ok('U2. the desktop user\'s jobs (another user): listed without their names, no Cancel', len(ids) == 3 and all(x.get('mine') is False and x.get('name') == '' and x.get('cancel') is False for x in d), d)
    js(CANCEL_JS % ids[2] if len(ids) > 2 else 'null'); wait_for(js, "window.__pc!='busy'", 60, 2); c = js("window.__pc")
    o = sh(f"su -s /bin/sh {AU} -c 'rudolf-printq cancel {ids[2] if len(ids) > 2 else 0}; echo; CUPS_SERVER=/run/cups/cups.sock true'", 30)
    ok('U3. neither the page (the account) nor the account\'s own user can cancel them', 'another account' in str(c) and 'another account' in o, (c, o))
    o = sh(f"su -s /bin/sh rudolf -c 'rudolf-printq cancel {ids[2] if len(ids) > 2 else 0}; rudolf-printq cancel {ids[1] if len(ids) > 1 else 0}'", 30)
    ok('U4. their own user can', o.count('"ok":true') == 2, o)
    if FIX and os.path.isdir(os.path.join(FIX, 'cups')):
        o = sh(f"su -s /bin/sh {AU} -c 'export LD_LIBRARY_PATH=/tmp/fx/cups/lib CUPS_SERVER=/run/cups/cups.sock; /tmp/fx/cups/lpstat -p; /tmp/fx/cups/lp -d Host_PDF_printer -t LinuxApp /tmp/doc.pdf; sleep 6; /tmp/fx/cups/lpstat -W completed -o'", 90)
        n = len(files(SPOOL1, len(f1) + 1, 60))
        ok('U5. lp (CUPS\'s own client, as Debian\'s apps print) as the account\'s user: the queue\'s printers, the job printed', 'printer Host_PDF_printer' in o and 'request id is Host_PDF_printer-' in o and n > len(f1) and any('Rudolf Linux user print test' in t for t in texts(SPOOL1)), o)
    else: out('NOTE no cups/ on the fix disk: lp not tried in the guest (the host test prints with lp and GTK)')
    # ------------------------------------------------------------------ Q. the queue itself
    o = sh("P=$(pgrep -f 'rudolf-printq serve' | tail -1); grep -E '^(Uid|Gid|Groups|NoNewPrivs)' /proc/$P/status; stat -c '%a %U:%G' /run/cups/cups.sock /run/cups; "
           "for f in /proc/$P/fd/*; do readlink $f; done | sed -n 's/socket:\\[\\(.*\\)\\]/\\1/p' > /tmp/ino; for i in $(cat /tmp/ino); do grep -h \" $i \" /proc/net/tcp /proc/net/tcp6 /proc/net/udp /proc/net/udp6; done | wc -l | sed 's/^/tcp-sockets /'; "
           "/usr/lib/rudolf/selftest 2>/dev/null | grep -E ' (listeners|print-queue) '", 90)
    ok('Q1. the queue: the "print" user with group lp, no_new_privs; its socket 0666 (made by root before it became "print"), its folder its own; no network socket of its own', re.search(r'^Uid:\s+952\s', o, re.M) and re.search(r'^Groups:.*\b7\b', o, re.M) and re.search(r'NoNewPrivs:\s+1', o) and '666 root:root' in o and '755 print:print' in o and 'tcp-sockets 0' in o, o)
    ok('Q2. the self-test: its listeners line passes with the queue\'s socket named; its print-queue line, running', re.search(r'^PASS listeners', o, re.M) and re.search(r'^PASS print-queue - running', o, re.M), o)
    js("(async()=>{try{fsFlush()}catch(e){}try{await homeChain}catch(e){}try{await __rudolfLsFlush()}catch(e){}return 1})()", 60)
    sh("echo native > /persist/etc/rudolf-desktop-engine; rm -f /persist/etc/rudolf-desktop-engine.failed; sync", 60)
    time.sleep(5); vm.stop(); time.sleep(3)
    # ------------------------------------------------------------------ N. Native
    vm, g, up = boot('n'); sh = lambda c, t=60, show=True: run(g, c, t, show)
    ok('N0. the release starts again (Native)', up)
    if not up: raise RuntimeError('did not start')
    sh("for i in $(seq 1 60); do pgrep -x rudolf-native >/dev/null && break; sleep 3; done; cat /run/rudolf/desktop-engine", 240)
    if FIX:
        g.put(os.path.join(HERE, 'fix161.sh'), '/tmp/fix161.sh', '644'); g.put(os.path.join(HERE, 'fix161p.sh'), '/tmp/fix161p.sh', '644')
        o = sh("sh /tmp/fix161p.sh; X=/run/r161x; cp /usr/bin/rudolf-native $X/rudolf-native.rel; chmod 755 $X/rudolf-native.rel; rm -f /tmp/teval /tmp/teval.out; mkfifo -m 666 /tmp/teval; "
               "mount --bind /tmp/fx/rn /usr/bin/rudolf-native; P0=$(pgrep -f 'rudolf-native [-]-rudolf-kiosk'); kill $P0; sleep 3; kill -9 $P0 2>/dev/null; for i in $(seq 1 90); do [ -s /tmp/nstate ] && break; sleep 2; done; head -3 /tmp/nstate", 400)
        ok('N0b. this tree\'s printing in place again; the native desktop started again with the tests\' views', 'screen ' in o, o)
    sh(NETUP, 60)
    state = lambda: sh("cat /tmp/nstate", 30, False)
    def st_get(st, k):
        m = re.search(rf'^{k} (.*)$', st, re.M); return m.group(1) if m else None
    st = state(); SW, SH = [int(x) for x in (st_get(st, 'screen') or '1280 800').split()]
    def at(x, y, btn='left'): g.q.at(x, y, btn, SW, SH)
    def tev(key, expr, t=30):
        n0 = sh("wc -l < /tmp/teval.out 2>/dev/null || echo 0", 20, False).strip().split('\n')[-1]
        open(f'{W}/j.js', 'w').write(expr.replace('\n', ' ')); g.put(f'{W}/j.js', '/tmp/j.js', '644')
        o = sh(f"timeout 5 sh -c '{{ printf \"%s \" {key}; cat /tmp/j.js; echo; }} > /tmp/teval'; for i in $(seq 1 {t}); do n=$(wc -l < /tmp/teval.out 2>/dev/null || echo 0); [ $n -gt {n0 or 0} ] && break; sleep 1; done; grep '^{key} ' /tmp/teval.out | tail -1", t + 10, False)
        o = o.strip(); return o[len(key) + 1:] if o.startswith(key + ' ') else o
    def twait(key, expr, secs):
        end = time.time() + secs; v = None
        while time.time() < end:
            v = tev(key, expr)
            if v not in ('busy', '') and not v.startswith('NO WINDOW'): return v
            time.sleep(3)
        return v
    if st_get(st, 'signin') == '1':
        time.sleep(3); g.q.type('b1'); time.sleep(0.5); g.q.key('ret')
        for _ in range(30):
            time.sleep(3); st = state()
            if st_get(st, 'user') == 'boss': break
    ok('N1. signed in on the native sign-in screen', st_get(st, 'user') == 'boss', st[:600])
    time.sleep(20)
    def hit(st, k):
        m = re.search(rf'^hit {k} (\d+) (\d+) (\d+) (\d+)$', st, re.M); return (int(m.group(1)) + int(m.group(3)) / 2, int(m.group(2)) + int(m.group(4)) / 2) if m else None
    def open_app(k, q, wait=60, tries=3):
      for _ in range(tries):   # (the start menu's search is slow without KVM: tried again, Escape first)
          g.q.key('esc'); time.sleep(1); st = state(); h = hit(st, 'start \\S+')
          if h: at(*h); time.sleep(1.5); g.q.type(q); time.sleep(1.5); g.q.key('ret')
          for _ in range(wait):
              time.sleep(1)
              if re.search(rf'^client .* app={k} ', state(), re.M): return True
      return False
    out('open notepad', open_app('notepad', 'notepad')); time.sleep(10)
    n1 = len(os.listdir(SPOOL1))
    tev('notepad', print_js('Native', False)); res = twait('notepad', "window.__pq", 300); out('printed (Native):', res)
    try: P = json.loads(res)
    except Exception: P = {}
    ok('N2. Native, an app window (WebKit): a PDF and a page printed through the queue', all((P.get(k) or {}).get('queued') for k in ('pdf', 'page')), res)
    files(SPOOL1, n1 + 2, 120); tx = texts(SPOOL1)
    ok('N3. ...the PDF printer has them (the page\'s text)', sum('Rudolf page print test' in t for t in tx) >= 2 and sum('Rudolf PDF Viewer print test' in t for t in tx) >= 2, tx)
    tev('notepad', JOBS_JS); jl = twait('notepad', "window.__pj", 60); J = jobs_of(jl)
    ok('N4. ...print/jobs from the app window: the Native jobs, the account\'s own, sent', sum(1 for j in J if j.get('mine') and j.get('name', '').startswith('Native') and j.get('state') == 'sent') >= 2, jl)
    # (Settings › Devices opened from the app's page, as its links open it: the shell opens Settings' own window there)
    tev('notepad', "(()=>{settings('devs');return 1})()"); opened = False
    for _ in range(90):
        time.sleep(2)
        if re.search(r'^client .* app=settings ', state(), re.M): opened = True; break
    if not opened: out('open settings from the start menu', open_app('settings', 'settings', 120))
    # (its page loads slowly without KVM: asked until the queue is drawn, Settings › Devices opened again if it is not there)
    end = time.time() + 300; rows = ''
    while time.time() < end:
        time.sleep(8)
        rows = tev('settings', "(()=>{if(typeof settings!='function'||typeof pqSection!='function')return 'loading';if(!document.querySelector('.pq61'))settings('devs');return JSON.stringify([...document.querySelectorAll('.pq61 tr')].map(r=>r.textContent))})()")
        if 'Native page' in rows: break
    out('Native Settings › Devices › Print queue:', rows)
    ok('N5. Native\'s Settings › Devices › Print queue lists them as Standard\'s does (the queue starts empty at each start)', 'Native page' in rows and 'Native PDF' in rows and 'Sent to the printer' in rows, rows)
    vm.shot('nat-queue')
    out(sh("tail -20 /run/rudolf/printq.log; grep -iE 'refused|asked for' /persist/var/log/confine.log 2>/dev/null | tail -5", 30, False))
except RuntimeError as e:
    ok('the run went on: ' + str(e), False)
finally:
    try:
        if vm: vm.stop()
    except Exception: pass
    for p in PROCS: p.kill()
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
