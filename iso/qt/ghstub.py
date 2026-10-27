# A stand-in for the parts of GitHub's REST API the development link uses (1.4.0), and for rudolfd's screenshot and
# power routes when the host test runs rudolf-devlink outside Rudolf OS. One private repository, its branch "link".
#   python3 ghstub.py PORT TOKEN REPO [RDTOKEN]
# Control (for tests, no authorization): GET /_files (JSON: path -> base64), POST /_put?path=P (body: the file),
# POST /_del?path=P, GET /_log (requests seen: method, path, authorized), POST /_fail?n=N&code=C (the next N API
# requests answer C), POST /_private?v=0|1, POST /_push?v=0|1, GET /_powered (how often power?action=reboot came).
import base64, hashlib, http.server, json, socketserver, sys, threading, urllib.parse

PORT, TOKEN, REPO = int(sys.argv[1]), sys.argv[2], sys.argv[3]
RDTOKEN = sys.argv[4] if len(sys.argv) > 4 else ''
FILES = {}            # path -> bytes, on branch "link"
LOG = []
STATE = {'fail': 0, 'failcode': 500, 'private': True, 'push': True, 'powered': 0}
LOCK = threading.Lock()
PNG = base64.b64decode('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR4nGP4z8AAAAMBAQDJ/pLvAAAAAElFTkSuQmCC')


def sha(b): return hashlib.sha1(b'blob %d\0' % len(b) + b).hexdigest()


class H(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *a): pass

    def send(self, code, body=b'', ctype='application/json', extra=None):
        if isinstance(body, (dict, list)): body = json.dumps(body).encode()
        self.send_response(code); self.send_header('Content-Type', ctype); self.send_header('Content-Length', str(len(body)))
        for k, v in (extra or {}).items(): self.send_header(k, v)
        self.end_headers(); self.wfile.write(body)

    def body(self):
        n = int(self.headers.get('Content-Length') or 0); return self.rfile.read(n) if n else b''

    def handle_one(self, method):
        u = urllib.parse.urlparse(self.path); q = dict(urllib.parse.parse_qsl(u.query)); p = u.path
        b = self.body() if method in ('PUT', 'POST') else b''
        # ---- control
        if p == '/_files': return self.send(200, {k: base64.b64encode(v).decode() for k, v in FILES.items()})
        if p == '/_put': FILES[q['path']] = b; return self.send(200, {'ok': True})
        if p == '/_del': FILES.pop(q['path'], None); return self.send(200, {'ok': True})
        if p == '/_log': return self.send(200, LOG)
        if p == '/_fail': STATE['fail'] = int(q.get('n', 1)); STATE['failcode'] = int(q.get('code', 500)); return self.send(200, {'ok': True})
        if p == '/_private': STATE['private'] = q.get('v') == '1'; return self.send(200, {'ok': True})
        if p == '/_push': STATE['push'] = q.get('v') == '1'; return self.send(200, {'ok': True})
        if p == '/_powered': return self.send(200, {'powered': STATE['powered']})
        # ---- rudolfd stand-in (host test only)
        if p.startswith('/api/'):
            if self.headers.get('X-Rudolf-Token') != RDTOKEN: return self.send(403, {'ok': False, 'error': 'missing token'})
            if p == '/api/screenshot': return self.send(200, PNG, 'image/png')
            if p == '/api/power' and q.get('action') == 'reboot': STATE['powered'] += 1; return self.send(200, {'ok': True})
            return self.send(404, {'ok': False})
        # ---- GitHub
        auth = self.headers.get('Authorization') == 'Bearer ' + TOKEN
        LOG.append({'method': method, 'path': self.path, 'auth': auth, 'etag': self.headers.get('If-None-Match') or ''})
        if STATE['fail'] > 0:
            STATE['fail'] -= 1; return self.send(STATE['failcode'], {'message': 'stand-in failure'})
        if not auth: return self.send(401, {'message': 'Bad credentials'})
        base = '/repos/' + REPO
        if p == base and method == 'GET':
            return self.send(200, {'full_name': REPO, 'private': STATE['private'], 'permissions': {'admin': False, 'push': STATE['push'], 'pull': True}})
        if not p.startswith(base + '/contents/'): return self.send(404, {'message': 'Not Found'})
        path = p[len(base + '/contents/'):]
        if method == 'GET':
            if q.get('ref') != 'link': return self.send(404, {'message': 'No commit found for the ref'})
            if path in FILES:
                if self.headers.get('Accept') == 'application/vnd.github.raw': return self.send(200, FILES[path], 'application/octet-stream')
                return self.send(200, {'name': path.split('/')[-1], 'path': path, 'sha': sha(FILES[path]), 'size': len(FILES[path])})
            kids = sorted(k for k in FILES if k.startswith(path + '/') and '/' not in k[len(path) + 1:])
            if not kids: return self.send(404, {'message': 'Not Found'})
            listing = [{'name': k.split('/')[-1], 'path': k, 'sha': sha(FILES[k]), 'type': 'file'} for k in kids]
            etag = '"' + hashlib.sha1(json.dumps(listing).encode()).hexdigest() + '"'
            if self.headers.get('If-None-Match') == etag: return self.send(304, b'', extra={'ETag': etag})
            return self.send(200, listing, extra={'ETag': etag})
        if method == 'PUT':
            j = json.loads(b or b'{}')
            if j.get('branch') != 'link': return self.send(422, {'message': 'branch'})
            if path in FILES and j.get('sha') != sha(FILES[path]): return self.send(422, {'message': '"sha" wasn\'t supplied.'})
            new = path not in FILES
            FILES[path] = base64.b64decode(j['content']); return self.send(201 if new else 200, {'content': {'path': path, 'sha': sha(FILES[path])}})
        return self.send(405, {'message': 'Method Not Allowed'})

    def do_GET(self): self.handle_one('GET')
    def do_PUT(self): self.handle_one('PUT')
    def do_POST(self): self.handle_one('POST')


class S(socketserver.ThreadingMixIn, http.server.HTTPServer): daemon_threads = True; allow_reuse_address = True


S(('0.0.0.0', PORT), H).serve_forever()
