# 1.6.0 (docs/plan-1.6.0.md Part 19): the devlink's reference client (iso/systools/devlink.py, docs/DEVLINK.md) against the
# stand-in GitHub (../ghstub.py) and the real rudolf-devlink. python3 devlinkclient60_t.py   (needs gcc, libcrypto, libcurl.so.4)
import base64, json, os, subprocess, sys, tempfile, threading, time, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__)); SYS = os.path.join(HERE, '..', '..', 'sys'); TOOLS = os.path.join(HERE, '..', '..', 'systools')
sys.path.insert(0, TOOLS); import devlink
T = tempfile.mkdtemp(); BIN = f'{T}/rudolf-devlink'
open(f'{T}/upd', 'w').write('#!/bin/sh\n[ "$1" = status ] && echo \'{"ok":true,"current":{"serial":7,"version":"1.6.0","kernel":"6.18"},"signer":"official"}\'\n'); os.chmod(f'{T}/upd', 0o755)
subprocess.run(['gcc', '-O2', '-w', '-o', BIN, f'{SYS}/rudolf-devlink.c', '-lcrypto', '-ldl'], check=True)
PORT = 18766; TOKEN = 'github_pat_CLIENT0123456789abcdefSECRET'; REPO = 'dev/my-devlink'; RDTOK = 'rdtok'
API = f'http://127.0.0.1:{PORT}'
stub = subprocess.Popen([sys.executable, os.path.join(HERE, '..', 'ghstub.py'), str(PORT), TOKEN, REPO, RDTOK])
DIR, RUN = f'{T}/dir', f'{T}/run'; os.makedirs(DIR); os.makedirs(RUN)
ENV = dict(os.environ, RUDOLF_DEVLINK_UPDATE=f'{T}/upd', RUDOLF_DEVLINK_TEST='1', RUDOLF_DEVLINK_API=API, RUDOLF_DEVLINK_DIR=DIR, RUDOLF_DEVLINK_RUN=RUN, RUDOLF_DEVLINK_RD=API, RUDOLF_TOKEN=RDTOK)
CLI = [sys.executable, os.path.join(TOOLS, 'devlink.py')]
CENV = dict(os.environ, DEVLINK_API=API, DEVLINK_REPO=REPO, DEVLINK_TOKEN=TOKEN)
P = N = 0


def ok(name, cond, detail=''):
    global P, N
    N += 1; P += bool(cond); print(('PASS ' if cond else 'FAIL ') + name + ('' if cond else ' - ' + str(detail)[:500]), flush=True)


def ctl(path, data=None):
    r = urllib.request.urlopen(urllib.request.Request(API + path, data=data, method='POST' if data is not None else 'GET'), timeout=10)
    return json.loads(r.read() or b'{}')


def files(): return {k: base64.b64decode(v) for k, v in ctl('/_files').items()}


def cli(*a, inp=None, env=None):
    return subprocess.run(CLI + list(a), env=env or CENV, input=inp, capture_output=True, text=True, timeout=180)


time.sleep(1)
open(f'{DIR}/token', 'w').write(TOKEN + '\n'); os.chmod(f'{DIR}/token', 0o600)
open(f'{DIR}/config', 'w').write(''.join(f'{k}={v}\n' for k, v in {'on': '1', 'repo': REPO, 'until': int(time.time()) + 3600, 'ask': '0', 'home': '0', 'restart': '0', 'change': '1'}.items()))
d = devlink.DevLink(REPO, TOKEN, api=API)

# ---- before the computer answers
ok('1. the first id: 000001 (nothing in cmd/ or res/)', d.next_id() == '000001')
ctl('/_put?path=res/000004.json', b'{"id":"000004"}')
ok('2. the next id counts results too (one outlives its command)', d.next_id() == '000005', d.next_id())
ctl('/_del?path=res/000004.json', b'')
ok('3. the token is never in its repr or the command line help', TOKEN not in repr(d) and TOKEN not in cli('--help').stdout)
r = cli('run', 'sh', 'true', env=dict(CENV, DEVLINK_TOKEN='github_pat_WRONG'))
ok('4. a wrong token: said so, exit 2', r.returncode == 2 and 'refused the token' in r.stderr, r.stderr)
r = cli('run', 'sh', 'true', env=dict(CENV, DEVLINK_REPO='not-a-repo'))
ok('5. a repository not OWNER/NAME: said so before GitHub is asked', r.returncode == 2 and 'OWNER/NAME' in r.stderr, r.stderr)

# ---- the link on
link = subprocess.Popen([BIN, 'run'], env=ENV, stdout=subprocess.PIPE, stderr=subprocess.STDOUT); time.sleep(2)
r = cli('run', 'sh', 'echo hello; echo oops >&2; exit 3', '--wait', '60')
ok('6. run sh: the output, the errors and the status as its exit status', r.stdout == 'hello\n' and 'oops' in r.stderr and r.returncode == 3 and '[000001 sh: status 3' in r.stderr, (r.returncode, r.stdout, r.stderr))
c1 = json.loads(files()['cmd/000001.json'])
ok('...the command as docs/DEVLINK.md says', c1 == {'kind': 'sh', 'script': 'echo hello; echo oops >&2; exit 3'}, c1)
r = cli('run', 'sh', '-', '--timeout', '30', '--wait', '60', inp='echo from stdin\n')
ok('7. a script from standard input, with its time limit', r.stdout == 'from stdin\n' and json.loads(files()['cmd/000002.json']).get('timeout') == 30, (r.stdout, r.stderr))
res = d.run('screenshot', wait=60, save=f'{T}/out')
ok('8. screenshot: its file saved', res['status'] == 0 and res['saved'] == [f'{T}/out/000003-screen.png'] and open(res['saved'][0], 'rb').read()[:4] == b'\x89PNG', res)
open(f'{T}/local.txt', 'w').write('brought over\n')
r = cli('run', 'put', 'note.txt', '--file', f'{T}/local.txt', '--wait', '60')
ok('9. put --file: uploaded to files/NAME, then brought to the computer', r.returncode == 0 and files().get('files/note.txt') == b'brought over\n' and open(f'{DIR}/files/note.txt').read() == 'brought over\n', (r.stderr, os.listdir(DIR)))
open(f'{T}/local.txt', 'w').write('second version\n')
r = cli('run', 'put', 'note.txt', '--file', f'{T}/local.txt', '--wait', '60')
ok('...a second time: the repository\'s file replaced (its sha given)', r.returncode == 0 and open(f'{DIR}/files/note.txt').read() == 'second version\n', r.stderr)
r = cli('run', 'get', f'{DIR}/files/note.txt', '--save', f'{T}/out', '--wait', '60')
ok('10. get: the file saved here', r.returncode == 0 and open(f'{T}/out/000006-note.txt').read() == 'second version\n', (r.stderr, os.listdir(f'{T}/out')))
r = cli('run', 'get', 'relative/path')
ok('11. get with a path that is not full: refused here, nothing sent', r.returncode == 2 and 'full path' in r.stderr and 'cmd/000007.json' not in files(), r.stderr)

# ---- two writers at once: each gets its own id
link.terminate(); link.wait()
out = []
def w(i): out.append(devlink.DevLink(REPO, TOKEN, api=API).send('sh', script=f'echo writer {i}'))
th = [threading.Thread(target=w, args=(i,)) for i in range(3)]; [t.start() for t in th]; [t.join() for t in th]
ok('12. three writers at once: three different ids, none lost', len(set(out)) == 3 and all(f'cmd/{i}.json' in files() for i in out), out)
r = cli('last'); last = json.loads(r.stdout)
ok('13. last: the newest command and result', last['command'] == max(out) and last['result'] == '000006', last)
r = cli('wait', max(out), '--wait', '3')
ok('14. wait with no answer in time: exit 124, said so', r.returncode == 124 and 'no result within 3 s' in r.stderr, (r.returncode, r.stderr))
link = subprocess.Popen([BIN, 'run'], env=ENV, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
r = cli('wait', max(out), '--wait', '60')
ok('15. the link on again: the waiting commands run in order', r.returncode == 0 and r.stdout.startswith('writer '), (r.stdout, r.stderr))
ctl('/_fail?n=2&code=502', b'')
r = cli('run', 'sh', 'echo after a hiccup', '--wait', '60')
ok('16. GitHub failing twice: tried again', r.returncode == 0 and r.stdout == 'after a hiccup\n', (r.returncode, r.stderr))
r = cli('run', 'end', '--wait', '60'); link.wait(timeout=30)
ok('17. end: the link ends', r.returncode == 0 and link.returncode is not None, r.stderr)
ok('18. the token was never written to the repository', not any(TOKEN.encode() in v for v in files().values()))
stub.terminate()
print(f'{P}/{N} passed'); sys.exit(0 if P == N else 1)
