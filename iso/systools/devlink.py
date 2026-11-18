#!/usr/bin/env python3
# devlink.py - drive a Rudolf OS computer's development link (1.6.0, docs/plan-1.6.0.md Part 19; docs/DEVLINK.md).
# GPL-2.0-or-later. Python 3.8+, the standard library only: any agent (Claude, Codex, a person's own) or person can use it,
# from any computer, including Rudolf OS's Linux terminal (Debian).
#
# The link is a private GitHub repository used as a mailbox on its branch "link": this writes cmd/NNNNNN.json, the computer
# answers res/NNNNNN.json (and res/NNNNNN/NAME for files). See docs/DEVLINK.md for every command and result field.
#
#   devlink.py run sh 'uname -a' [--timeout S]     send a command and wait for its result (printed; files saved with --save DIR)
#   devlink.py run sh - < script.sh                the script from standard input
#   devlink.py run screenshot --save .             res/NNNNNN/screen.png saved here
#   devlink.py run get /persist/var/log/health.log --save .
#   devlink.py run put NAME --file LOCAL           LOCAL uploaded to the repository (files/NAME), then brought to the computer
#   devlink.py run put NAME --url URL              a file of a release of the computer's own Rudolf OS repository
#   devlink.py run restart | run end
#   devlink.py send KIND ...                       the same, without waiting: prints the command's id
#   devlink.py wait ID [--wait S] [--save DIR]     a result (exit status: the command's; 124 when none came in time)
#   devlink.py next | devlink.py last              the next free id; the newest command and result
#
# The repository: --repo OWNER/NAME or DEVLINK_REPO. The token (a fine-grained token for that repository alone, Contents:
# read and write): DEVLINK_TOKEN, or --token-file FILE (default ~/.config/rudolf-devlink/token). Never on the command line,
# never printed. DEVLINK_API: another API address (tests).
#
# As a module:  from devlink import DevLink;  d = DevLink('me/my-devlink', token);  r = d.run('sh', script='uname -a')
import argparse, base64, json, os, sys, time, urllib.error, urllib.parse, urllib.request

KINDS = ('sh', 'screenshot', 'get', 'put', 'restart', 'end')


class DevLinkError(Exception):
    pass


class DevLink:
    def __init__(self, repo, token, api=None, branch='link'):
        if not repo or repo.count('/') != 1 or any(c in repo for c in ' ?#'):
            raise DevLinkError('the repository is written as OWNER/NAME')
        if not token:
            raise DevLinkError('no token (DEVLINK_TOKEN or --token-file)')
        self.repo, self._token, self.branch = repo, token, branch
        self.api = (api or os.environ.get('DEVLINK_API') or 'https://api.github.com').rstrip('/')

    def __repr__(self):   # (never the token)
        return f'DevLink({self.repo!r})'

    # ---- GitHub's contents API
    def _req(self, method, path, body=None, raw=False, query=None):
        q = dict(query or {})
        url = f'{self.api}/repos/{self.repo}{path}' + ('?' + urllib.parse.urlencode(q) if q else '')
        data = json.dumps(body).encode() if body is not None else None
        h = {'Authorization': 'Bearer ' + self._token, 'Accept': 'application/vnd.github.raw' if raw else 'application/vnd.github+json',
             'X-GitHub-Api-Version': '2022-11-28', 'User-Agent': 'rudolf-devlink-client'}
        if data is not None:
            h['Content-Type'] = 'application/json'
        for attempt in range(5):
            try:
                with urllib.request.urlopen(urllib.request.Request(url, data=data, headers=h, method=method), timeout=120) as r:
                    b = r.read()
                    return r.status, (b if raw else (json.loads(b) if b else None))
            except urllib.error.HTTPError as e:
                b = e.read()
                if e.code in (500, 502, 503, 504, 429) and attempt < 4:
                    time.sleep(2 ** attempt); continue
                try:
                    j = json.loads(b)
                except Exception:
                    j = {'message': b[:200].decode('utf-8', 'replace')}
                return e.code, j
            except (urllib.error.URLError, TimeoutError, ConnectionError) as e:
                if attempt < 4:
                    time.sleep(2 ** attempt); continue
                raise DevLinkError(f'GitHub could not be reached: {e}')

    def _list(self, folder):
        code, j = self._req('GET', f'/contents/{folder}', query={'ref': self.branch})
        if code == 404:
            return []
        if code == 401:
            raise DevLinkError('GitHub refused the token: it is wrong, expired or revoked')
        if code != 200 or not isinstance(j, list):
            raise DevLinkError(f'could not list {folder}/ (GitHub answered {code}: {(j or {}).get("message", "")})')
        return [x['name'] for x in j]

    def _ids(self, folder):
        return sorted(n[:6] for n in self._list(folder) if len(n) == 11 and n.endswith('.json') and n[:6].isdigit())

    def put_file(self, path, data, message):
        """A file on the branch (a new one, or a new version); returns True when written."""
        code, j = self._req('GET', f'/contents/{path}', query={'ref': self.branch})
        sha = j.get('sha') if code == 200 and isinstance(j, dict) else None
        body = {'message': message, 'content': base64.b64encode(data).decode(), 'branch': self.branch}
        if sha:
            body['sha'] = sha
        code, j = self._req('PUT', f'/contents/{path}', body)
        if code in (200, 201):
            return True
        raise DevLinkError(f'{path} could not be written (GitHub answered {code}: {(j or {}).get("message", "")})')

    # ---- the link
    def last(self):
        c, r = self._ids('cmd'), self._ids('res')
        return {'command': c[-1] if c else None, 'result': r[-1] if r else None}

    def next_id(self):
        """One more than the highest id in cmd/ or res/ (a result may outlive its command)."""
        ids = self._ids('cmd') + self._ids('res')
        n = max((int(i) for i in ids), default=0) + 1
        if n > 999999:
            raise DevLinkError('the ids are used up (999999): start a new repository')
        return '%06d' % n

    def send(self, kind, **fields):
        """Write cmd/NNNNNN.json; returns its id. Another writer taking the same id: the next one is taken."""
        if kind not in KINDS:
            raise DevLinkError('kind: ' + ', '.join(KINDS))
        if kind == 'sh' and not fields.get('script'):
            raise DevLinkError('sh needs a script')
        if kind == 'get' and not str(fields.get('path', '')).startswith('/'):
            raise DevLinkError('get needs a full path')
        if kind == 'put' and not fields.get('name'):
            raise DevLinkError('put needs a name')
        cmd = {'kind': kind}
        cmd.update({k: v for k, v in fields.items() if v is not None})
        data = (json.dumps(cmd, indent=1) + '\n').encode()
        for attempt in range(8):
            cid = self.next_id()
            body = {'message': f'command {cid} ({kind})', 'content': base64.b64encode(data).decode(), 'branch': self.branch}
            code, j = self._req('PUT', f'/contents/cmd/{cid}.json', body)
            if code in (200, 201):
                return cid
            if code in (409, 422):   # (the id was taken, or the branch moved under us: look again)
                time.sleep(1 + attempt); continue
            if code == 404:
                raise DevLinkError('the repository or its branch "link" was not found (make the branch first), or the token cannot see it')
            raise DevLinkError(f'the command could not be written (GitHub answered {code}: {(j or {}).get("message", "")})')
        raise DevLinkError('the command could not be written: another writer kept taking the next id')

    def result(self, cid):
        code, b = self._req('GET', f'/contents/res/{cid}.json', raw=True, query={'ref': self.branch})
        if code == 404:
            return None
        if code != 200:
            raise DevLinkError(f'res/{cid}.json could not be read (GitHub answered {code})')
        return json.loads(b)

    def wait(self, cid, wait=900, every=5, save=None):
        """The result of command cid (None when none came within wait seconds); its files saved into save."""
        end = time.time() + wait
        while True:
            r = self.result(cid)
            if r is not None:
                if save:
                    os.makedirs(save, exist_ok=True)
                    r['saved'] = []
                    for name in r.get('files') or []:
                        if '/' in name or name.startswith('.'):
                            continue
                        code, b = self._req('GET', f'/contents/res/{cid}/{name}', raw=True, query={'ref': self.branch})
                        if code == 200:
                            p = os.path.join(save, f'{cid}-{name}')
                            with open(p, 'wb') as f:
                                f.write(b)
                            r['saved'].append(p)
                return r
            if time.time() >= end:
                return None
            time.sleep(every)

    def run(self, kind, wait=900, save=None, **fields):
        if kind == 'put' and fields.get('file'):
            local = fields.pop('file'); name = fields['name']
            with open(local, 'rb') as f:
                data = f.read()
            if len(data) > 95 << 20:
                raise DevLinkError('the contents API takes at most about 95 MB: attach it to a release instead (put --url)')
            self.put_file(f'files/{name}', data, f'file {name}')
            fields['src'] = f'files/{name}'
        cid = self.send(kind, **fields)
        to = int(fields.get('timeout') or 0)
        r = self.wait(cid, wait=max(wait, to + 120), save=save)
        if r is None:
            r = {'id': cid, 'kind': kind, 'status': None, 'error': f'no result within {max(wait, to + 120)} s (is the link on, and Ask first answered?)'}
        return r


def _token(a):
    t = os.environ.get('DEVLINK_TOKEN', '').strip()
    if t:
        return t
    p = os.path.expanduser(a.token_file or '~/.config/rudolf-devlink/token')
    try:
        st = os.stat(p)
        if st.st_mode & 0o077:
            print(f'devlink: {p} can be read by others: chmod 600 it', file=sys.stderr)
        return open(p).read().strip()
    except OSError:
        return ''


def _print(r):
    if r.get('status') is None and r.get('error'):
        print('devlink: ' + r['error'], file=sys.stderr); return 124
    out, err = r.get('stdout') or '', r.get('stderr') or ''
    if out:
        sys.stdout.write(out if out.endswith('\n') else out + '\n')
    if err:
        sys.stderr.write(err if err.endswith('\n') else err + '\n')
    tail = [f"[{r.get('id')} {r.get('kind')}: status {r.get('status')}"]
    for k in ('statusText', 'error', 'approved'):
        if r.get(k):
            tail.append(f'{k} {r[k]}')
    if r.get('saved'):
        tail.append('saved ' + ' '.join(r['saved']))
    elif r.get('files'):
        tail.append('files ' + ' '.join(r['files']))
    print('; '.join(tail) + ']', file=sys.stderr)
    s = r.get('status')
    return s if isinstance(s, int) and 0 <= s < 256 else 1


def main(argv=None):
    ap = argparse.ArgumentParser(prog='devlink', description='Drive a Rudolf OS computer\'s development link (docs/DEVLINK.md).')
    ap.add_argument('--repo', default=os.environ.get('DEVLINK_REPO', ''))
    ap.add_argument('--token-file')
    sub = ap.add_subparsers(dest='op', required=True)
    for op in ('run', 'send'):
        s = sub.add_parser(op); s.add_argument('kind', choices=KINDS); s.add_argument('arg', nargs='?')
        s.add_argument('--timeout', type=int, help='sh: seconds (1-3600, 600 when not given)')
        s.add_argument('--file', help='put: a local file, uploaded first'); s.add_argument('--url', help='put: a release file')
        s.add_argument('--src', help='put: a file already in the repository')
        if op == 'run':
            s.add_argument('--wait', type=int, default=900); s.add_argument('--save')
    w = sub.add_parser('wait'); w.add_argument('id'); w.add_argument('--wait', type=int, default=900); w.add_argument('--save')
    sub.add_parser('next'); sub.add_parser('last')
    a = ap.parse_args(argv)
    try:
        d = DevLink(a.repo, _token(a))
        if a.op == 'next':
            print(d.next_id()); return 0
        if a.op == 'last':
            print(json.dumps(d.last())); return 0
        if a.op == 'wait':
            if not (len(a.id) == 6 and a.id.isdigit()):
                raise DevLinkError('an id is six digits')
            r = d.wait(a.id, wait=a.wait, save=a.save)
            return _print(r if r is not None else {'id': a.id, 'status': None, 'error': f'no result within {a.wait} s'})
        f = {}
        if a.kind == 'sh':
            f['script'] = sys.stdin.read() if a.arg in (None, '-') else a.arg
            if a.timeout:
                f['timeout'] = a.timeout
        elif a.kind == 'get':
            f['path'] = a.arg
        elif a.kind == 'put':
            f.update(name=a.arg, url=a.url, src=a.src)
            if a.file:
                f['file'] = a.file
        if a.op == 'send':
            if f.get('file'):
                with open(f.pop('file'), 'rb') as fh:
                    d.put_file(f'files/{a.arg}', fh.read(), f'file {a.arg}')
                f['src'] = f'files/{a.arg}'
            print(d.send(a.kind, **f)); return 0
        return _print(d.run(a.kind, wait=a.wait, save=a.save, **f))
    except DevLinkError as e:
        print('devlink: ' + str(e), file=sys.stderr); return 2


if __name__ == '__main__':
    sys.exit(main())
