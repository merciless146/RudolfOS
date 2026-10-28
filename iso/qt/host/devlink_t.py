# rudolf-devlink (1.4.0) on the host, against a stand-in for GitHub (../ghstub.py), which also stands in for rudolfd's
# screenshot and power routes. python3 devlink_t.py   (needs gcc, libcrypto, libcurl.so.4)
import base64, json, os, shutil, signal, subprocess, sys, tempfile, time, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__)); SYS = os.path.join(HERE, '..', '..', 'sys')
T = tempfile.mkdtemp(); BIN = f'{T}/rudolf-devlink'
subprocess.run(['gcc', '-O2', '-w', '-o', BIN, f'{SYS}/rudolf-devlink.c', '-lcrypto', '-ldl'], check=True)
PORT = 18765; TOKEN = 'github_pat_TEST0123456789abcdefSECRET'; REPO = 'owner/rudolfos-devlink'; RDTOK = 'rdtoken0123'
API = f'http://127.0.0.1:{PORT}'
stub = subprocess.Popen([sys.executable, os.path.join(HERE, '..', 'ghstub.py'), str(PORT), TOKEN, REPO, RDTOK])
DIR, RUN = f'{T}/dir', f'{T}/run'; os.makedirs(DIR); os.makedirs(RUN)
ENV = dict(os.environ, RUDOLF_DEVLINK_TEST='1', RUDOLF_DEVLINK_API=API, RUDOLF_DEVLINK_DIR=DIR, RUDOLF_DEVLINK_RUN=RUN, RUDOLF_DEVLINK_RD=API, RUDOLF_TOKEN=RDTOK)
P = N = 0


def ok(name, cond, detail=''):
    global P, N
    N += 1; P += bool(cond); print(('PASS ' if cond else 'FAIL ') + name + ('' if cond else ' - ' + str(detail)[:400]), flush=True)


def ctl(path, data=None):
    r = urllib.request.urlopen(urllib.request.Request(API + path, data=data, method='POST' if data is not None else 'GET'), timeout=10)
    return json.loads(r.read() or b'{}')


def files(): return {k: base64.b64decode(v) for k, v in ctl('/_files').items()}
def cmd(i, **kw): ctl(f'/_put?path=cmd/{i:06d}.json', json.dumps(kw).encode())


def result(i, t=30):
    end = time.time() + t
    while time.time() < end:
        f = files().get(f'res/{i:06d}.json')
        if f: return json.loads(f)
        time.sleep(0.5)
    return None


def config(**kw):
    c = {'on': '1', 'repo': REPO, 'until': str(int(time.time()) + 3600), 'ask': '0', 'home': '0', 'restart': '0'}; c.update({k: str(v) for k, v in kw.items()})
    open(f'{DIR}/config', 'w').write(''.join(f'{k}={v}\n' for k, v in c.items()))


def start():
    for f in ('stop',):
        try: os.remove(f'{RUN}/{f}')
        except FileNotFoundError: pass
    return subprocess.Popen([BIN, 'run'], env=ENV, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def statusj():
    try: return json.load(open(f'{RUN}/status.json'))
    except Exception: return {}


time.sleep(1)
tf = f'{T}/tok'; open(tf, 'w').write(TOKEN + '\n'); os.chmod(tf, 0o600)
chk = lambda repo=REPO, f=tf: subprocess.run([BIN, 'check', repo, f], env=ENV, capture_output=True, text=True)
r = chk(); ok('check: a private repository the token can write to', r.returncode == 0 and r.stdout.strip() == 'ok', r.stdout)
bad = f'{T}/bad'; open(bad, 'w').write('github_pat_WRONG'); r = chk(f=bad)
ok('check: a wrong token is refused, and said so', r.returncode != 0 and 'refused the token' in r.stdout, r.stdout)
ctl('/_private?v=0', b''); r = chk(); ctl('/_private?v=1', b'')
ok('check: a public repository is refused', r.returncode != 0 and 'public' in r.stdout, r.stdout)
ctl('/_push?v=0', b''); r = chk(); ctl('/_push?v=1', b'')
ok('check: a token that cannot write is refused', r.returncode != 0 and 'cannot write' in r.stdout, r.stdout)
r = chk(repo='not a repo'); ok('check: a repository name not owner/name is refused', r.returncode != 0, r.stdout)
open(bad, 'w').write('x; rm -rf /'); r = chk(f=bad); ok('check: something that is not a token is refused before GitHub is asked', r.returncode != 0 and 'not a GitHub token' in r.stdout, r.stdout)

# ---- the link, Ask first off
open(f'{DIR}/token', 'w').write(TOKEN + '\n'); os.chmod(f'{DIR}/token', 0o600)
config(); d = start(); time.sleep(2)
ok('switched on: it says it is on, with no commands yet', statusj().get('state') == 'on' and 'no commands yet' in statusj().get('error', ''), statusj())
cmd(1, kind='sh', script='echo hello; echo oops >&2; exit 3')
cmd(2, kind='sh', script='echo second')
r1, r2 = result(1), result(2)
ok('sh: stdout, stderr and the exit status come back', r1 and r1['stdout'] == 'hello\n' and r1['stderr'] == 'oops\n' and r1['status'] == 3, r1)
ok('...commands run one at a time, in order', r1 and r2 and r2['stdout'] == 'second\n' and r1['ended'] <= r2['started'], (r1, r2))
ok('...as root, and the result says it was not asked (Ask first off)', r1 and r1['approved'].startswith('not asked'), r1)
cmd(3, kind='sh', script='sleep 30', timeout=2); r = result(3, 20)
ok('sh: the time limit ends a command, and says so', r and 'timeout' in r.get('statusText', '') and r['status'] == -1, r)
cmd(4, kind='sh', script='head -c 2100000 /dev/zero | tr "\\0" x'); r = result(4, 30)
ok('sh: output over 1 MB is cut, and said so', r and len(r['stdout']) < 1100000 and 'cut' in r['stdout'][-60:], r and len(r['stdout']))
cmd(5, kind='screenshot'); r = result(5)
png = files().get('res/000005/screen.png', b'')
ok('screenshot: a PNG of the screen, from rudolfd with its token', r and r['status'] == 0 and r['files'] == ['screen.png'] and png.startswith(b'\x89PNG'), r)
open(f'{T}/hello.txt', 'w').write('file content\n')
cmd(6, kind='get', path=f'{T}/hello.txt'); r = result(6)
ok('get: a file is sent', r and r['status'] == 0 and files().get('res/000006/hello.txt') == b'file content\n', r)
cmd(7, kind='get', path='/home'); r = result(7)
ok('get: nothing from home folders unless the owner allowed it', r and r['status'] == -1 and 'home folders' in r.get('error', ''), r)
cmd(8, kind='get', path=f'{DIR}/token'); r = result(8)
ok('get: the link\'s own token is never sent', r and r['status'] == -1 and 'token' in r.get('error', ''), r)
ctl('/_put?path=tools/test.bin', b'\x01\x02payload')
cmd(9, kind='put', src='tools/test.bin', name='test.bin'); r = result(9)
ok('put: a file from the repository is brought to the computer', r and r['status'] == 0 and open(f'{DIR}/files/test.bin', 'rb').read() == b'\x01\x02payload', r)
cmd(10, kind='put', url='https://example.com/x', name='x'); r = result(10)
ok('put: only files from the repository or a Rudolf OS release', r and r['status'] == -1, r)
cmd(11, kind='put', src='tools/test.bin', name='../evil'); r = result(11)
ok('put: a file name with a path in it is refused', r and r['status'] == -1, r)
cmd(12, kind='frobnicate'); r = result(12)
ok('an unknown kind of command is refused, and the next one still runs', r and r['status'] == -1 and 'unknown kind' in r.get('error', ''), r)
time.sleep(7)
lg = ctl('/_log'); polls = [x for x in lg if x['path'].startswith(f'/repos/{REPO}/contents/cmd?')]
ok('checks for commands with the last ETag (no cost when nothing changed)', polls and polls[-1]['etag'], polls[-1:] if polls else lg[-3:])
ok('...and only with the token (every request of the link authorized)', all(x['auth'] for x in lg if '/contents/' in x['path']), [x for x in lg if not x['auth'] and '/contents/' in x['path']][:3])
ctl('/_fail?n=3&code=503', b''); time.sleep(4)
st = statusj(); ok('GitHub not answering: it waits and says so', st.get('state') == 'waiting for the network', st)
cmd(13, kind='sh', script='echo back'); r = result(13, 90)
ok('...and carries on when GitHub answers again', r and r['stdout'] == 'back\n', r)
cmd(14, kind='end'); r = result(14); d.wait(20)
ok('end: the link ends itself, and is off', r and r['status'] == 0 and d.poll() is not None and 'on=0' in open(f'{DIR}/config').read(), (r, d.poll()))

# ---- Ask first
config(ask=1); d = start(); time.sleep(2)
cmd(15, kind='sh', script='echo allowed')
end = time.time() + 20
while time.time() < end and not os.path.exists(f'{RUN}/pending.json'): time.sleep(0.3)
pj = json.load(open(f'{RUN}/pending.json')) if os.path.exists(f'{RUN}/pending.json') else {}
ok('Ask first: the command waits, with its full text, for Allow', pj.get('id') == '000015' and 'echo allowed' in pj.get('what', '') and statusj().get('state') == 'waiting for approval', pj)
time.sleep(2); ok('...and nothing runs meanwhile', 'res/000015.json' not in files())
open(f'{RUN}/answer.000015', 'w').write('allow\n'); r = result(15)
ok('...Allow: it runs, and the result says so', r and r['stdout'] == 'allowed\n' and r['approved'] == 'allowed on the computer', r)
cmd(16, kind='sh', script='touch ' + T + '/must-not-exist')
end = time.time() + 20
while time.time() < end and not os.path.exists(f'{RUN}/pending.json'): time.sleep(0.3)
open(f'{RUN}/answer.000016', 'w').write('refuse\n'); r = result(16)
ok('...Refuse: it does not run, and the result says refused', r and r['approved'] == 'refused' and not os.path.exists(f'{T}/must-not-exist'), r)
open(f'{RUN}/stop', 'w').write('1'); d.wait(15)
ok('the stop file (End now in Settings) ends it at once', d.poll() is not None)

# ---- restart, and a command cut short by a restart
config(); d = start(); time.sleep(2)
cmd(17, kind='restart'); r = result(17); d.wait(60)
ok('restart: the result is sent first, then rudolfd is asked to restart', r and r['status'] == 0 and ctl('/_powered')['powered'] == 1, (r, ctl('/_powered')))
ok('...and it is not run again (the state says done)', 'last=000017' in open(f'{DIR}/state').read(), open(f'{DIR}/state').read())
open(f'{DIR}/state', 'w').write('last=000017\nrunning=000018\n'); cmd(18, kind='sh', script='echo would-run-twice')
d = start(); r = result(18)
ok('a command that a restart cut short is reported as interrupted, not run again', r and 'interrupted' in r.get('error', '') and 'would-run-twice' not in r.get('stdout', ''), r)
cmd(19, kind='sh', script='echo after'); r = result(19)
ok('...and the next one runs', r and r['stdout'] == 'after\n', r)
os.kill(d.pid, signal.SIGTERM); d.wait(15)

# ---- the time limit
config(until=int(time.time()) + 4); d = start(); d.wait(30)
ok('it ends by itself when its time is up, and is off', d.poll() is not None and 'on=0' in open(f'{DIR}/config').read() and 'time was up' in statusj().get('state', ''), statusj())
config(on=0); d = start(); d.wait(10)
ok('it does not run when it is switched off', d.poll() is not None and statusj().get('state') == 'off', statusj())

# ---- 1.4.1: another data partition (no record of what was done): commands with a result are not run again
os.remove(f'{DIR}/state'); open(f'{T}/ran-again', 'w').close(); os.remove(f'{T}/ran-again')
cmd(20, kind='sh', script=f'echo new'); config(); d = start(); r = result(20)
ok('with no record of what was done, only the command without a result runs (1.4.1)', r and r['stdout'] == 'new\n' and 'last=000020' in open(f'{DIR}/state').read(), (r, open(f'{DIR}/state').read()))
ok('...the earlier ones (restarts among them) are not run again', ctl('/_powered')['powered'] == 1 and json.loads(files()['res/000001.json'])['stdout'] == 'hello\n', ctl('/_powered'))
os.kill(d.pid, signal.SIGTERM); d.wait(15)

# ---- the token
leaks = [k for k, v in files().items() if TOKEN.encode() in v]
for root, _, fs in os.walk(T):
    for f in fs:
        p = os.path.join(root, f)
        if p in (tf, f'{DIR}/token', BIN) or f == 'bad': continue
        try:
            if TOKEN.encode() in open(p, 'rb').read(): leaks.append(p)
        except Exception: pass
ok('the token is in no result, status, log or state file', not leaks, leaks)
ok('the token file is readable by its owner only', oct(os.stat(f'{DIR}/token').st_mode & 0o777) == '0o600')
lines = open(f'{DIR}/log').read().strip().split('\n')
ok('the log has every command, its result and who allowed it', len(lines) >= 19 and all('"approved"' in l for l in lines), len(lines))

stub.kill(); shutil.rmtree(T)
print(f'{P}/{N} passed'); sys.exit(0 if P == N else 1)
