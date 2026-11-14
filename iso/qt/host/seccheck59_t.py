# 1.5.9 (plan 15.6): the daily security check (iso/systools/seccheck.py) against stand-ins for GitHub, Ubuntu's chromium
# snap and Ubuntu's mainline kernel list: no issue when the release is current, one issue "Security update available:
# ..." when Chromium or the kernel (its own series only) is newer, never a second while one is open, the latest release
# (stable or beta), and nothing built, released, promoted or dispatched.   python3 seccheck59_t.py
import base64, json, os, subprocess, sys, threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__)); SCRIPT = os.path.join(HERE, '..', '..', 'systools', 'seccheck.py')
PORT = 8322; R = 'owner/RudolfOS'
S = {'stable': None, 'beta': None, 'snap': '158.0.8127.18', 'snapfail': False, 'mainline': [], 'issues': [], 'log': []}


def man(serial, ver, chrom, kern):
    return f'rudolf-system 1\nserial {serial}\nversion {ver}\nreleased 2026-11-12\nexpires 2026-11-26\nurgent 0\nchromium {chrom}\nkernel {kern}-rudolf\nnotes x\n'


class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass

    def send(self, code, obj):
        b = json.dumps(obj).encode() if not isinstance(obj, bytes) else obj
        self.send_response(code); self.send_header('content-type', 'application/json'); self.send_header('content-length', str(len(b))); self.end_headers(); self.wfile.write(b)

    def body(self): n = int(self.headers.get('content-length') or 0); return json.loads(self.rfile.read(n) or b'{}')

    def do_GET(self):
        S['log'].append(('GET', self.path))
        p = self.path
        for ch in ('stable', 'beta'):
            if p.startswith(f'/repos/{R}/contents/{ch}/manifest.txt'):
                return self.send(200, {'content': base64.b64encode(S[ch].encode()).decode()}) if S[ch] else self.send(404, {'message': 'Not Found'})
        if p.startswith('/snap'):
            if S['snapfail']: return self.send(503, {})
            return self.send(200, {'channel-map': [{'channel': {'name': 'stable', 'architecture': 'arm64'}, 'version': '999.0'}, {'channel': {'name': 'stable', 'architecture': 'amd64'}, 'version': S['snap'] + '-snap1'}, {'channel': {'name': 'beta', 'architecture': 'amd64'}, 'version': '999.1'}]})
        if p.startswith('/mainline'):
            return self.send(200, ('<html>' + ''.join(f'<a href="v{v}/">v{v}/</a>' for v in S['mainline']) + '</html>').encode())
        if p.startswith(f'/repos/{R}/issues?'):
            from urllib.parse import urlparse, parse_qs
            q = parse_qs(urlparse(p).query); lb = (q.get('labels') or [''])[0]
            return self.send(200, [i for i in S['issues'] if i['state'] == 'open' and (not lb or lb in i['labels'])])
        self.send(404, {})

    def do_POST(self):
        S['log'].append(('POST', self.path))
        if self.path == f'/repos/{R}/issues':
            b = self.body(); i = {'number': 100 + len(S['issues']), 'title': b['title'], 'body': b['body'], 'labels': b.get('labels', []), 'state': 'open'}
            S['issues'].append(i); return self.send(201, i)
        self.send(404, {})

    def do_PATCH(self):
        S['log'].append(('PATCH', self.path))
        for i in S['issues']:
            if self.path == f'/repos/{R}/issues/{i["number"]}': i.update(self.body()); return self.send(200, i)
        self.send(404, {})


srv = ThreadingHTTPServer(('127.0.0.1', PORT), H); threading.Thread(target=srv.serve_forever, daemon=True).start()
B = f'http://127.0.0.1:{PORT}'
ENV = dict(os.environ, SECCHECK_GITHUB=B, SECCHECK_SNAP=B + '/snap', SECCHECK_MAINLINE=B + '/mainline/', GITHUB_REPOSITORY=R, GITHUB_TOKEN='t', SECCHECK_RETRY='0')
P = N = 0


def ok(n, c, d=''):
    global P, N
    N += 1; P += bool(c); print(('PASS ' if c else 'FAIL ') + n + ('' if c else ' - ' + str(d)[:500]), flush=True)


def run():
    r = subprocess.run([sys.executable, SCRIPT], env=ENV, capture_output=True, text=True, timeout=120); return r.returncode, r.stdout + r.stderr


def opened(): return [i for i in S['issues'] if i['state'] == 'open']


S['stable'] = man(2026111219, '1.5.8.261112', '158.0.8127.18', '6.18.63'); S['mainline'] = ['6.18.61', '6.18.62', '6.18.63', '6.17.20']
rc, o = run()
ok('1. the release has the newest Chromium and kernel: no issue', rc == 0 and not S['issues'] and 'has the newest' in o, o)
S['snap'] = '158.0.8127.54'; rc, o = run()
ok('2. a newer Chromium in Ubuntu\'s snap (stable, amd64): one issue, "Security update available: Chromium …"', rc == 0 and len(opened()) == 1 and opened()[0]['title'] == 'Security update available: Chromium 158.0.8127.54', (o, S['issues']))
ok('...labelled, saying what the release has and that nothing was built', 'security-update' in opened()[0]['labels'] and '158.0.8127.18' in opened()[0]['body'] and 'builds, releases and promotes nothing' in opened()[0]['body'], opened()[0])
rc, o = run()
ok('3. checked again: no second issue (and the open one not rewritten when nothing changed)', len(S['issues']) == 1 and 'already says so' in o, (o, S['issues']))
S['mainline'].append('6.18.64'); rc, o = run()
ok('4. a newer kernel of the release\'s series in Ubuntu\'s mainline builds: the same issue updated, both named', len(S['issues']) == 1 and opened()[0]['title'] == 'Security update available: Chromium 158.0.8127.54, kernel 6.18.64' and 'Linux 6.18.64' in opened()[0]['body'], (o, S['issues']))
S['issues'][0]['state'] = 'closed'; S['snap'] = '158.0.8127.18'; S['mainline'] = ['6.18.63', '6.19', '6.19.1', '7.0.2']; rc, o = run()
ok('5. a new kernel series (6.19, 7.0) is not a security update of 6.18: no issue', len(opened()) == 0, (o, S['issues']))
S['issues'].append({'number': 50, 'title': 'Chromium 158.0.8127.30 is available (security fixes)', 'body': 'old', 'labels': ['chromium-update'], 'state': 'open'})
S['snap'] = '158.0.8127.60'; rc, o = run()
ok('6. an issue still open from the Chromium check before 1.5.9: updated, not a new one', len(opened()) == 1 and opened()[0]['number'] == 50 and opened()[0]['title'] == 'Security update available: Chromium 158.0.8127.60', (o, S['issues']))
S['issues'][-1]['state'] = 'closed'
S['beta'] = man(2026111405, '1.5.9.261114', '158.0.8127.60', '6.18.63'); rc, o = run()
ok('7. the latest release is the newest of stable and beta: a beta with the new Chromium already: no issue', len(opened()) == 0 and '1.5.9.261114 (beta' in o, (o, S['issues']))
S['snapfail'] = True; S['mainline'] = ['6.18.63', '6.18.65']; rc, o = run()
ok('8. the snap store not answering: a warning, and the kernel is still checked', rc == 0 and 'could not be read' in o and len(opened()) == 1 and opened()[0]['title'] == 'Security update available: kernel 6.18.65', (o, S['issues']))
S['stable'] = S['beta'] = None; rc, o = run()
ok('9. no manifest at all: a warning, no issue, not a failure', rc == 0 and 'no release manifest' in o, o)
bad = [x for x in S['log'] if x[0] != 'GET' and not x[1].startswith(f'/repos/{R}/issues')] + [x for x in S['log'] if any(w in x[1] for w in ('/actions', '/dispatches', '/releases', '/git/', '/merges', '/pulls'))]
ok('10. it never builds, releases, promotes or dispatches: it only reads, and writes issues', not bad, bad)
wf = ''.join(l for l in open(os.path.join(HERE, '..', '..', '..', '.github', 'workflows', 'security-check.yml')) if not l.lstrip().startswith('#'))
ok('11. the workflow: daily, issues and read-only contents, nothing that builds or dispatches', "schedule: [{cron:" in wf and 'permissions: {contents: read, issues: write}' in wf and not any(w in wf for w in ('workflow_run', 'dispatches', 'gh workflow', 'release create', 'contents: write')), wf)
srv.shutdown(); print(f'{P}/{N} passed'); sys.exit(0 if P == N else 1)
