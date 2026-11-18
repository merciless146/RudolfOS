# 1.6.0 (docs/plan-1.6.0.md Part 19 item 4): rudolf-dev, GitHub for Blitzen as a developer's agent, against a stand-in
# GitHub here (a repository with branches, files, workflow runs and pull requests), and as the devlink command against
# ../ghstub.py and the real rudolf-devlink.   python3 devagent60_t.py   (gcc, libcrypto, libcurl.so.4)
import base64, json, os, re, subprocess, sys, tempfile, threading, time, urllib.parse, urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__)); SYS = os.path.join(HERE, '..', '..', 'sys')
T = tempfile.mkdtemp(); DEV = f'{T}/rudolf-dev'
subprocess.run(['gcc', '-O2', '-w', '-o', DEV, f'{SYS}/rudolf-dev.c', '-lcrypto', '-ldl'], check=True)
os.symlink(DEV, f'{T}/devlink')
TOKEN = 'github_pat_AGENT0123456789abcdef'; R = 'dev/RudolfOS'
P = N = 0


def ok(name, cond, detail=''):
    global P, N
    N += 1; P += bool(cond); print(('PASS ' if cond else 'FAIL ') + name + ('' if cond else ' - ' + str(detail)[:600]), flush=True)


# ---- a stand-in GitHub: one repository
G = {'branches': {'main': 'c0'}, 'files': {'main': {'README.md': b'hello\n', 'desktop/a.js': b'let a=1\n'}}, 'commits': 0, 'pulls': [], 'dispatches': [], 'reqs': []}
RUNS = {'workflow_runs': [{'id': 77, 'name': 'Rudolf OS release', 'head_branch': 'main', 'status': 'completed', 'conclusion': 'failure', 'html_url': 'https://github.com/dev/RudolfOS/actions/runs/77'}]}
LOG = ''.join(f'line {i}\n' for i in range(20000)) + 'error: the build failed\n'


class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass

    def send(self, code, obj=None, raw=None, loc=None):
        b = raw if raw is not None else (json.dumps(obj).encode() if obj is not None else b'')
        self.send_response(code)
        if loc: self.send_header('Location', loc)
        self.send_header('Content-Length', str(len(b))); self.end_headers(); self.wfile.write(b)

    def go(self, m):
        u = urllib.parse.urlparse(self.path); q = dict(urllib.parse.parse_qsl(u.query)); p = urllib.parse.unquote(u.path)
        n = int(self.headers.get('Content-Length') or 0); body = json.loads(self.rfile.read(n) or b'null') if n else None
        G['reqs'].append((m, p, self.headers.get('Authorization')))
        if p.startswith('/blob/'): return self.send(200, raw=LOG.encode())   # (a log's own address: no token goes there)
        if self.headers.get('Authorization') != 'Bearer ' + TOKEN: return self.send(401, {'message': 'Bad credentials'})
        pre = f'/repos/{R}'
        if not p.startswith(pre): return self.send(404, {'message': 'Not Found'})
        p = p[len(pre):]
        if p == '' and m == 'GET': return self.send(200, {'full_name': R, 'default_branch': 'main', 'private': False, 'permissions': {'push': True}})
        if p == '/actions/runs': return self.send(200, RUNS)
        if p == '/actions/runs/77/jobs': return self.send(200, {'jobs': [{'id': 501, 'name': 'build', 'conclusion': 'failure'}]})
        if p == '/actions/jobs/501/logs': return self.send(302, loc=f'http://127.0.0.1:{PORT}/blob/501')
        if p == '/actions/workflows': return self.send(200, {'workflows': [{'id': 9, 'name': 'Rudolf OS release', 'path': '.github/workflows/rudolf-release.yml'}]})
        mm = re.match(r'/actions/workflows/([^/]+)/dispatches$', p)
        if mm and m == 'POST': G['dispatches'].append((mm.group(1), body)); return self.send(204)
        mm = re.match(r'/git/ref/heads/(.+)$', p)
        if mm and m == 'GET':
            b = mm.group(1); return self.send(200, {'ref': 'refs/heads/' + b, 'object': {'sha': G['branches'][b], 'type': 'commit'}}) if b in G['branches'] else self.send(404, {'message': 'Not Found'})
        if p == '/git/refs' and m == 'POST':
            b = body['ref'][len('refs/heads/'):]
            if b in G['branches']: return self.send(422, {'message': 'Reference already exists'})
            src = [k for k, v in G['branches'].items() if v == body['sha']][0]; G['branches'][b] = body['sha']; G['files'][b] = dict(G['files'][src]); return self.send(201, {'ref': body['ref']})
        mm = re.match(r'/contents/(.+)$', p)
        if mm:
            path = mm.group(1); br = q.get('ref') or (body or {}).get('branch') or 'main'; fs = G['files'].get(br, {})
            if m == 'GET':
                if path not in fs: return self.send(404, {'message': 'Not Found'})
                if 'raw' in (self.headers.get('Accept') or ''): return self.send(200, raw=fs[path])
                return self.send(200, {'path': path, 'sha': 'sha-' + str(hash(fs[path]) & 0xffff)})
            if m == 'PUT':
                if path in fs and body.get('sha') != 'sha-' + str(hash(fs[path]) & 0xffff): return self.send(409, {'message': 'sha does not match'})
                if path not in fs and body.get('sha'): return self.send(422, {'message': 'not there'})
                fs[path] = base64.b64decode(body['content']); G['commits'] += 1; G['branches'][br] = f'c{G["commits"]}'
                return self.send(201 if 'sha' not in body else 200, {'content': {'path': path}})
        if p == '/pulls' and m == 'POST':
            G['pulls'].append(body); n = len(G['pulls']); return self.send(201, {'number': n, 'html_url': f'https://github.com/{R}/pull/{n}'})
        return self.send(404, {'message': 'Not Found'})

    def do_GET(self): self.go('GET')
    def do_POST(self): self.go('POST')
    def do_PUT(self): self.go('PUT')


srv = ThreadingHTTPServer(('127.0.0.1', 0), H); PORT = srv.server_address[1]; threading.Thread(target=srv.serve_forever, daemon=True).start()
ENV = dict(os.environ, RUDOLF_DEV_API=f'http://127.0.0.1:{PORT}')


def dev(*a, inp='', token=TOKEN):
    return subprocess.run([DEV] + list(a), input=(token + '\n' + inp).encode(), env=ENV, capture_output=True, timeout=120)


r = dev('repo', R); ok('1. the repository, with the token from standard input', r.returncode == 0 and json.loads(r.stdout)['default_branch'] == 'main', r.stderr)
ok('...the token never an argument (ps shows nothing of it)', all(TOKEN not in x for x in ['repo', R]))
r = dev('repo', R, token='github_pat_WRONG000000'); ok('2. a wrong token: exit 3, GitHub\'s word for it', r.returncode == 3 and b'Bad credentials' in r.stderr, r.stderr)
r = dev('repo', 'not/a/repo'); ok('3. a repository that is not OWNER/NAME: refused here', r.returncode == 2 and b'OWNER/NAME' in r.stderr, r.stderr)
r = dev('repo', R, token='not a token; rm -rf'); ok('...and something that is not a token', r.returncode == 2, r.stderr)
r = dev('runs', R); ok('4. the runs', r.returncode == 0 and json.loads(r.stdout)['workflow_runs'][0]['conclusion'] == 'failure', r.stderr)
r = dev('jobs', R, '77'); ok('...a run\'s jobs', r.returncode == 0 and json.loads(r.stdout)['jobs'][0]['id'] == 501)
r = dev('joblog', R, '501'); log = r.stdout.decode()
ok('5. a job\'s log: its last 64 KB, from a whole line, the error at the end', r.returncode == 0 and len(r.stdout) <= 65536 and log.startswith('line ') and log.endswith('error: the build failed\n'), len(r.stdout))
ok('...the token not sent to the log\'s own address', [x for x in G['reqs'] if x[1].startswith('/blob/')][0][2] is None, G['reqs'][-1])
r = dev('file', R, 'desktop/a.js'); ok('6. a file', r.returncode == 0 and r.stdout == b'let a=1\n', r.stderr)
r = dev('file', R, '../etc/passwd'); ok('...not outside the repository', r.returncode == 2)
r = dev('workflows', R); ok('7. the workflows', r.returncode == 0 and json.loads(r.stdout)['workflows'][0]['id'] == 9)
r = dev('dispatch', R, 'rudolf-release.yml', 'main'); ok('...one started', r.returncode == 0 and G['dispatches'] == [('rudolf-release.yml', {'ref': 'main'})], (r.stderr, G['dispatches']))
files = 'desktop/a.js\t' + base64.b64encode(b'let a=2\n').decode() + '\n' + 'docs/new.md\t' + base64.b64encode(b'# New\n').decode() + '\n'
r = dev('push', R, 'main', 'main', 'x', inp=files); ok('8. push: never to the default branch', r.returncode == 2 or r.returncode == 5, r.stderr)
r = dev('push', R, 'dev', 'blitzen/x', 'x', inp=files); ok('...from a branch that is not there: nothing made', r.returncode == 4 and 'blitzen/x' not in G['branches'], r.stderr)
before = dict(G['files']['main'])
r = dev('push', R, 'main', 'blitzen/fix-a', 'Blitzen: a is 2', inp=files)
ok('9. push: a new branch from main with the files', r.returncode == 0 and G['files']['blitzen/fix-a']['desktop/a.js'] == b'let a=2\n' and G['files']['blitzen/fix-a']['docs/new.md'] == b'# New\n', (r.returncode, r.stderr))
ok('...main untouched', G['files']['main'] == before)
r = dev('push', R, 'main', 'blitzen/fix-a', 'again', inp=files); ok('...a branch already there: refused (exit 5)', r.returncode == 5 and b'already there' in r.stderr, r.stderr)
r = dev('push', R, 'main', 'blitzen/wf', 'x', inp='.github/workflows/x.yml\t' + base64.b64encode(b'on: push').decode() + '\n')
ok('...never the repository\'s workflows', r.returncode == 5 and 'blitzen/wf' not in G['branches'], r.stderr)
r = dev('pr', R, 'blitzen/fix-a', 'main', 'Blitzen: a is 2', inp='Asked for in Blitzen.\n')
ok('10. a pull request opened', r.returncode == 0 and json.loads(r.stdout)['url'].endswith('/pull/1') and G['pulls'][0]['head'] == 'blitzen/fix-a' and G['pulls'][0]['body'] == 'Asked for in Blitzen.\n', (r.stderr, G['pulls']))
src = open(f'{SYS}/rudolf-dev.c').read()
ok('11. nothing that merges, releases, deletes or changes settings', not re.search(r'"(DELETE|PATCH)"|/merge|/releases|/secrets|/collaborators|/hooks', src))

# ---- as devlink, against ../ghstub.py and the real rudolf-devlink (as devlinkclient60_t.py)
BIN = f'{T}/rudolf-devlink'; subprocess.run(['gcc', '-O2', '-w', '-o', BIN, f'{SYS}/rudolf-devlink.c', '-lcrypto', '-ldl'], check=True)
open(f'{T}/upd', 'w').write('#!/bin/sh\n[ "$1" = status ] && echo \'{"ok":true,"current":{"serial":7,"version":"1.6.0","kernel":"6.18"},"signer":"official"}\'\n'); os.chmod(f'{T}/upd', 0o755)
GP = 18767; LT = 'github_pat_LINK0123456789abcdefSECRET'; LR = 'dev/my-devlink'; API = f'http://127.0.0.1:{GP}'
stub = subprocess.Popen([sys.executable, os.path.join(HERE, '..', 'ghstub.py'), str(GP), LT, LR, 'rdtok']); time.sleep(1)
D, RUN = f'{T}/dir', f'{T}/run'; os.makedirs(D); os.makedirs(RUN)
open(f'{D}/token', 'w').write(LT + '\n'); os.chmod(f'{D}/token', 0o600)
open(f'{D}/config', 'w').write(''.join(f'{k}={v}\n' for k, v in {'on': '1', 'repo': LR, 'until': int(time.time()) + 3600, 'ask': '0', 'home': '0', 'restart': '0', 'change': '1'}.items()))
LENV = dict(os.environ, RUDOLF_DEVLINK_UPDATE=f'{T}/upd', RUDOLF_DEVLINK_TEST='1', RUDOLF_DEVLINK_API=API, RUDOLF_DEVLINK_DIR=D, RUDOLF_DEVLINK_RUN=RUN, RUDOLF_DEVLINK_RD=API, RUDOLF_TOKEN='rdtok')
link = subprocess.Popen([BIN, 'run'], env=LENV, stdout=subprocess.PIPE, stderr=subprocess.STDOUT); time.sleep(2)
CE = dict(os.environ, DEVLINK_API=API, DEVLINK_REPO=LR, DEVLINK_TOKEN=LT)
dl = lambda *a, inp=None: subprocess.run([f'{T}/devlink'] + list(a), env=CE, input=inp, capture_output=True, text=True, timeout=200)
r = dl('run', 'sh', 'echo hello; exit 3', '--wait', '60')
ok('12. devlink run sh: the output, and its status as the exit status (as devlink.py)', r.stdout == 'hello\n' and r.returncode == 3 and '[000001 sh: status 3' in r.stderr, (r.returncode, r.stdout, r.stderr))
r = dl('run', 'screenshot', '--save', f'{T}/out', '--wait', '60')
ok('13. devlink run screenshot --save', r.returncode == 0 and open(f'{T}/out/000002-screen.png', 'rb').read()[:4] == b'\x89PNG', r.stderr)
open(f'{T}/f.txt', 'w').write('over\n'); r = dl('run', 'put', 'f.txt', '--file', f'{T}/f.txt', '--wait', '60')
ok('14. devlink run put --file', r.returncode == 0 and open(f'{D}/files/f.txt').read() == 'over\n', r.stderr)
r = subprocess.run([DEV, 'dl-send', LR], input=LT + '\n{"kind":"sh","script":"echo via rudolfd"}\n', env=dict(os.environ, RUDOLF_DEV_API=API), capture_output=True, text=True, timeout=60)
cid = json.loads(r.stdout)['id'] if r.returncode == 0 else ''
r2 = subprocess.run([DEV, 'dl-wait', LR, cid or '000000', '60'], input=LT + '\n', env=dict(os.environ, RUDOLF_DEV_API=API), capture_output=True, text=True, timeout=120)
ok('15. dl-send and dl-wait (what rudolfd uses)', cid == '000004' and json.loads(r2.stdout)['result']['stdout'] == 'via rudolfd\n', (r.stdout, r.stderr, r2.stdout))
r = subprocess.run([DEV, 'dl-send', LR], input=LT + '\n{"kind":"format-disk"}\n', env=dict(os.environ, RUDOLF_DEV_API=API), capture_output=True, text=True, timeout=60)
ok('...a kind the link does not have: refused here', r.returncode == 2, r.stderr)
r = dl('last'); ok('16. devlink last', json.loads(r.stdout) == {'command': '000004', 'result': '000004'}, r.stdout)
link.terminate(); r = dl('wait', '000009', '--wait', '3'); ok('17. devlink wait: none in time, exit 124', r.returncode == 124, r.stderr)
r = subprocess.run([f'{T}/devlink', 'run', 'sh', 'true'], env=dict(os.environ, DEVLINK_API=API, DEVLINK_REPO=LR, HOME=T), capture_output=True, text=True)
ok('18. devlink with no token: said so, exit 2', r.returncode == 2 and 'no token' in r.stderr, r.stderr)
# ---- rudolfd's ai/dev (iso/sys/rudolfd-dev.inc): what it promises, read
I = open(f'{SYS}/rudolfd-dev.inc').read(); body = I[I.index('static void api_ai_dev('):]
ok('19. rudolfd: nothing of it unless Developer mode is on (the first thing checked)', re.search(r'static void api_ai_dev\(struct req \*r, const char \*what\)\n\{\n\tif \(!dl_devmode\(\)\) \{ reply_err\(r->fd, 403', I) is not None)
ok('...the token from the keyring, to rudolf-dev on its standard input only, as nobody', 'sadd(&in, "%s\\n", tok)' in body and 'dev_run(a, in.b, in.n, secs, 1,' in body and '--keyring-lookup' in I and not re.search(r'a\[\d\] = tok', body))
ok('...the token kept with the account\'s keyring (rudolf-portal --keyring-store), on standard input', '--keyring-%s %s' in I and 'dev_run(a, tok, clear ? 0 : tl + 1' in I)
ok('...GitHub\'s refusals as the page can say them (401, 404, 409)', 'st == 3 ? 401 : st == 4 ? 404 : st == 5 ? 409' in I)
ok('...each change noted in System health', all(x in body for x in ['pushed a new branch', 'opened a pull request', 'started a workflow', 'sent a command to your development link']))
stub.terminate(); srv.shutdown()
print(f'{P}/{N} passed'); sys.exit(0 if P == N else 1)
