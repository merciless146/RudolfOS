# Rudolf OS 1.4.0 in QEMU (UEFI, Secure Boot off): the development link end to end, on a drive installed as Install
# here makes it (the disc image, then RUDOLFDATA), with a stand-in for GitHub on the host (../ghstub.py). The test
# machine has no network, so the link's connections go over a second serial port (../sertun.c in the guest,
# ../sertun.py here); a test disc (rudolf.debug=1) lets the link use the stand-in (/persist/devlink/test-api).
#   python3 t140_t.py DISC.iso      DISC.iso: a 1.4.0 test disc (mkdisc.sh with RUDOLF_UKI_TEST_CMDLINE=rudolf.debug=1)
import base64, json, os, re, shutil, struct, subprocess, sys, time, urllib.request
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
DISC = os.path.abspath(sys.argv[1]); PARTS = os.environ['SB_PARTS']; LOG = open('t140.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:300] if d != '' and not c else ''))
def last(t): return t.strip().split('\n')[-1].strip() if t.strip() else ''

PORT = 18766; TOKEN = 'github_pat_T140test_0123456789abcdefSECRET'; REPO = 'owner/rudolfos-devlink'; GH = f'http://127.0.0.1:{PORT}'
ISZ = os.path.getsize(DISC); START = (ISZ + 1048575) // 1048576 * 2048; DMB = 1200
DRV = os.path.abspath('int140.img'); NAME = 't140'; TUN = f'/tmp/tun-{NAME}.sock'

# the drive: the disc image, then RUDOLFDATA with the guest side of the serial tunnel
subprocess.run(['gcc', '-O2', '-static', '-o', 'sertun', os.path.join(QT, 'sertun.c')], check=True)
with open(DRV, 'wb') as f:
    with open(DISC, 'rb') as o: shutil.copyfileobj(o, f)
    f.truncate((START + DMB * 2048 + 2048) * 512)
T = os.path.abspath('tree140'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/t140'); os.makedirs(f'{T}/devlink', mode=0o700)
shutil.copy('sertun', f'{T}/t140/sertun'); open(f'{T}/devlink/test-api', 'w').write('http://127.0.0.1:18765\n')
subprocess.run(['truncate', '-s', f'{DMB}M', 'data140'], check=True)
subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, 'data140'], check=True); shutil.rmtree(T)
with open(DRV, 'r+b') as f:
    f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', START, DMB * 2048))
    f.seek(START * 512)
    with open('data140', 'rb') as d: shutil.copyfileobj(d, f)
os.remove('data140')

stub = subprocess.Popen([sys.executable, os.path.join(QT, 'ghstub.py'), str(PORT), TOKEN, REPO])
tun = subprocess.Popen([sys.executable, os.path.join(QT, 'sertun.py'), TUN, '127.0.0.1', str(PORT)])
def ctl(path, data=None):
    r = urllib.request.urlopen(urllib.request.Request(GH + path, data=data, method='POST' if data is not None else 'GET'), timeout=20)
    return json.loads(r.read() or b'{}')
def files(): return {k: base64.b64decode(v) for k, v in ctl('/_files').items()}
def cmd(i, **kw): ctl(f'/_put?path=cmd/{i:06d}.json', json.dumps(kw).encode())
def result(i, t=180):
    end = time.time() + t
    while time.time() < end:
        f = files().get(f'res/{i:06d}.json')
        if f: return json.loads(f)
        time.sleep(2)
    return None

NVME = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf140,bootindex=0', '-serial', f'unix:{TUN},server,nowait']
def boot():
    vm = sbvm.VM(NAME, PARTS, [], secure=False, nvram=True, mem=3072, extra=NVME)
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3); return vm, g
def wait_shell(g, delay=45, limit=900):
    time.sleep(delay); end = time.time() + limit
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): return True
        except OSError: pass
        time.sleep(4)
    return False
def jsf(g):
    def js(e, t=60):
        for _ in range(3):
            r = g.js(e, t)
            if not (isinstance(r, tuple) and (r[0] == 'ERR' or (r[0] == 'EXC' and 'ReferenceError' in str(r[1])))): return r
            time.sleep(15)
        return r
    return js
def page(g):
    end = time.time() + 600
    while time.time() < end and g.js('typeof rdPost=="function"&&typeof devlPoll=="function"', 20) is not True: time.sleep(5)
    g.js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();try{svj('bd95setup',1)}catch(e){};const b=[...document.querySelectorAll('button')].find(b=>b.textContent.trim()=='Guest');if(b)b.click();return 1})()")
def J(v, d):
    try: return json.loads(v) if isinstance(v, str) else d
    except ValueError: return d
def st(js): return J(js("rd('devlink').then(r=>JSON.stringify(r))"), {})
def on(js, ask=0, restart=1, token=TOKEN):
    return js(f"rd('devlink/on?repo={REPO.replace('/', '%2F')}&hours=1&ask={ask}&restart={restart}&home=0',{{method:'POST',body:{json.dumps(token)}}}).then(r=>r.message,e=>'ERR '+e.message)", 120)
def tunnel(sh): sh('pgrep -f "sertun /dev/ttyS1" >/dev/null || (setsid /persist/t140/sertun /dev/ttyS1 18765 </dev/null >/dev/null 2>&1 &); echo started', 20)

vm, g = boot(); sh = lambda c, t=60: g.sh(c, t); js = jsf(g)
try:
    ok('the drive starts (the 1.4.0 test disc, files kept)', wait_shell(g) and last(sh('mountpoint -q /persist && echo kept', 20)) == 'kept')
    tunnel(sh); page(g)
    s = st(js); ok('the development link is there, and off', s.get('available') is True and s.get('on') is False and not s.get('hasToken'), s)
    r = on(js, token='github_pat_WRONGWRONGWRONG')
    ok('a wrong token is refused with GitHub\'s reason, and nothing is kept', isinstance(r, str) and r.startswith('ERR') and 'refused the token' in r and last(sh('ls /persist/devlink/token 2>/dev/null | wc -l', 20)) == '0', r)
    r = on(js)
    s = st(js); ok('switched on with the right token: on, for an hour', r == 'on' and s.get('on') is True and 3300 < s.get('secondsLeft', 0) <= 3600, [r, s])
    ps = sh('ps -o user,args | grep "[r]udolf-devlink run"', 20)
    ok('...rudolf-devlink runs, as root', 'root' in ps, ps)
    ok('...the token is kept root-only on RUDOLFDATA', last(sh('stat -c "%a %U" /persist/devlink/token', 20)) == '600 root')
    n1 = last(sh(f'grep -rl "{TOKEN}" /run/rudolf /tmp /home 2>/dev/null | wc -l', 60)); n2 = last(sh(f'wget -qO- http://127.0.0.1:8080/ | grep -c "{TOKEN}"', 60))
    ok('...and in no file under /run, /tmp or /home, nor in the desktop page', n1 == '0' and n2 == '0', [n1, n2])
    bt = ''
    for _ in range(15):   # the desktop polls every 4 s, and may be reloading (it moves into its shell once after a first start)
        bt = js("(()=>{const b=document.getElementById('devlinkBanner');return b?b.textContent:''})()")
        if isinstance(bt, str) and bt: break
        time.sleep(2)
    ok('the banner shows in the desktop', isinstance(bt, str) and 'Development link on' in bt and 'owner/rudolfos-devlink' in bt and 'End now' in bt, bt)
    cmd(1, kind='sh', script='id -u; cat /etc/rudolf/slot'); r = result(1)
    ok('sh: a command from the repository runs as root, and its output comes back', r and r['stdout'] == '0\ndisc\n' and r['status'] == 0, r)
    cmd(2, kind='screenshot'); r = result(2)
    png = files().get('res/000002/screen.png', b'')
    w, h = struct.unpack('>II', png[16:24]) if len(png) > 24 else (0, 0)
    ok('screenshot: the real screen, through rudolfd', r and r['status'] == 0 and png.startswith(b'\x89PNG') and w >= 640 and h >= 360, [r, w, h, len(png)])
    cmd(3, kind='get', path='/etc/rudolf/cmdline'); r = result(3)
    ok('get: a file from the computer', r and r['status'] == 0 and b'rudolf.debug=1' in files().get('res/000003/cmdline', b''), r)
    blob = os.urandom(300000); ctl('/_put?path=tools/blob.bin', blob)
    cmd(4, kind='put', src='tools/blob.bin', name='blob.bin'); r = result(4)
    md = last(sh('md5sum /persist/devlink/files/blob.bin | cut -c1-32', 20))
    import hashlib
    ok('put: a file from the repository, byte for byte', r and r['status'] == 0 and md == hashlib.md5(blob).hexdigest(), [r, md])
    # Ask first, switched on again with the kept token
    js("rdPost('devlink/off').then(()=>1)"); time.sleep(3)
    r = on(js, ask=1, token='')
    ok('switched on again with the kept token (none pasted), now with Ask first', r == 'on' and st(js).get('ask') is True, r)
    cmd(5, kind='sh', script='echo asked-and-allowed')
    shown = False
    for _ in range(40):
        if js("!!document.getElementById('devlinkAsk')") is True: shown = True; break
        time.sleep(3)
    txt = js("(document.getElementById('devlinkAsk')||{}).textContent||''")
    ok('Ask first: the command waits on the screen with its text', shown and 'echo asked-and-allowed' in txt and 'res/000005.json' not in files(), txt)
    js("document.querySelector('#devlinkAsk .dly').click(),1"); r = result(5)
    ok('...Allow on the screen runs it', r and r['stdout'] == 'asked-and-allowed\n' and r['approved'] == 'allowed on the computer', r)
    # restart: the link stays on (ticked) and comes back by itself
    js("rdPost('devlink/off').then(()=>1)"); time.sleep(3); on(js, ask=0, restart=1, token='')
    cmd(6, kind='restart'); r = result(6)
    ok('restart: the result first, then the computer restarts', r and r['status'] == 0, r)
finally:
    pass
time.sleep(20)
try:
    up = wait_shell(g, delay=30)
    tunnel(sh); page(g)
    s = st(js)
    ok('...after the restart the link is on again by itself (stay on across a restart was ticked)', up and s.get('on') is True, s)
    cmd(7, kind='sh', script='cut -d. -f1 /proc/uptime'); r = result(7, 400)
    ok('...and runs the next command (on the newly started system)', r and r['status'] == 0 and r['stdout'].strip().isdigit() and int(r['stdout']) < 1200, r)
    time.sleep(6); ok('...with the banner', js("!!document.getElementById('devlinkBanner')") is True)
    rep = js("sysReport('').then(r=>r.text)", 180)
    ok('the system report says it is on, without the token', isinstance(rep, str) and re.search(r'Development link: on until .*owner/rudolfos-devlink', rep) and TOKEN not in rep, (re.search(r'Development link.*', rep or '') or [''])[0] if isinstance(rep, str) else rep)
    lg = js("rd('devlink/log').then(r=>r.log)")
    ok('the log has every command and who allowed it', isinstance(lg, str) and all(f'"id":"{i:06d}"' in lg for i in range(1, 8)) and 'allowed on the computer' in lg, (lg or '')[-300:] if isinstance(lg, str) else lg)
    # End now
    js("document.querySelector('#devlinkBanner .dle').click(),1"); time.sleep(8)
    ok('End now: the link stops, the banner goes, and it is off', st(js).get('on') is False and js("!!document.getElementById('devlinkBanner')") is False and last(sh('pgrep -f "rudolf-devlink run" | wc -l', 20)) == '0' and last(sh('grep -c "^on=0" /persist/devlink/config', 20)) == '1')
    # on without "stay on across a restart": off after the next start
    on(js, ask=0, restart=0, token='')
    ok('switched on without staying across a restart', st(js).get('on') is True and st(js).get('restart') is False)
    g.s.s.send(b'sync; reboot -f\n')
finally:
    pass
time.sleep(20)
try:
    up = wait_shell(g, delay=30); page(g)
    s = st(js)
    ok('...after a restart it is off, and does not run', up and s.get('on') is False and last(sh('pgrep -f "rudolf-devlink run" | wc -l', 20)) == '0' and last(sh('grep -c "^on=0" /persist/devlink/config', 20)) == '1', s)
    rep = js("sysReport('').then(r=>r.text)", 180)
    ok('...and the system report says so', isinstance(rep, str) and 'Development link: off' in rep, (re.search(r'Development link.*', rep or '') or [''])[0] if isinstance(rep, str) else rep)
    r = js("rdPost('devlink/forget').then(r=>r.message,e=>'ERR '+e.message)")
    ok('Forget the token deletes it', r == 'the token is deleted' and last(sh('ls /persist/devlink/token 2>/dev/null | wc -l', 20)) == '0', r)
finally:
    vm.stop(); stub.kill(); tun.kill()
os.remove(DRV)
out('%d/%d passed' % (sum(R), len(R)))
