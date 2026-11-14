# 1.5.9 (plan Part 12): a stand-in rudolfd for rudolf-native's tests (native59_t.sh). Serves the real desktop page
# (iso/www, with the token and the account's storage put in as rudolfd's lsync copy would) and the desktop/* requests
# as rudolfd-desktop.inc answers them, from files in DIR the test changes:
#   DIR/settings.json  what desktop/settings answers (its "gen" goes up whenever the file changes)
#   DIR/ls.json, DIR/keys.json   the page's storage and Ann's keys (fixtures/deskls.js)
#   DIR/calls          every request, one per line (the test reads it)
#   DIR/wall.jpg       a picture of your own
# Ann's password is in keys.json; a wrong one gets the page's message.   python3 native_rd.py PORT DIR
import http.server, json, os, sys, threading
PORT = int(sys.argv[1]); D = sys.argv[2]; WWW = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../../www')
TOK = 'tok59native'; ST = {'session': None, 'locked': False, 'gen': 1, 'mt': 0}
LOCK = threading.Lock()
def rd(n, d=None):
    try: return open(os.path.join(D, n), 'rb').read()
    except OSError: return d
def note(s):
    with LOCK: open(os.path.join(D, 'calls'), 'a').write(s + '\n')
KEYS = json.loads(rd('keys.json', b'{}'))
LS = json.loads(rd('ls.json', b'{"data":{}}'))['data']
# the page's storage, put in before the page's own code runs (as rudolfd's lsync copy is)
INJ = ('<script>window.RUDOLFD={token:"%s",base:"",boot:"uefi",media:"disk",live:0,recovery:0,engine:"native"};'
       '(function(){var L=%s;try{if(!localStorage.getItem("bd95user"))for(var k in L)localStorage.setItem(k,L[k])}catch(e){}})();</script>') % (TOK, json.dumps(LS).replace('</', '<\\/'))
class H(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.0'
    def log_message(self, *a): pass
    def send(self, code, body, ctype='application/json'):
        b = body.encode() if isinstance(body, str) else body
        try:
            self.send_response(code); self.send_header('content-type', ctype); self.send_header('content-length', str(len(b))); self.end_headers(); self.wfile.write(b)
        except Exception: pass
    def j(self, o, code=200): self.send(code, json.dumps(o))
    def err(self, code, m): self.j({'ok': False, 'error': m}, code)
    def body(self):
        n = int(self.headers.get('content-length') or 0); return self.rfile.read(n) if n else b''
    def do_GET(self): self.go('GET')
    def do_POST(self): self.go('POST')
    def go(self, m):
        u, _, q = self.path.partition('?'); b = self.body() if m == 'POST' else b''
        if u.startswith('/api/'):
            note('%s %s%s' % (m, u[5:], '?' + q if q else ''))
            if self.headers.get('X-Rudolf-Token') != TOK: return self.err(403, 'missing token')
            return self.api(m, u[5:], q, b)
        if u in ('/', '/index.html'):
            note('PAGE /')
            return self.send(200, open(os.path.join(WWW, 'index.html'), encoding='utf-8').read().replace('<head>', '<head>' + INJ, 1), 'text/html; charset=utf-8')
        f = os.path.normpath(os.path.join(WWW, u.lstrip('/')))
        if f.startswith(os.path.normpath(WWW)) and os.path.isfile(f):
            ct = {'.js': 'text/javascript', '.webp': 'image/webp', '.jpg': 'image/jpeg', '.png': 'image/png', '.woff2': 'font/woff2', '.woff': 'font/woff', '.wasm': 'application/wasm'}.get(os.path.splitext(f)[1], 'application/octet-stream')
            return self.send(200, open(f, 'rb').read(), ct)
        self.send(404, 'not found', 'text/plain')
    def api(self, m, w, q, b):
        s = ST
        if w == 'events/wait':
            import time; time.sleep(3); return self.j({'ok': True, 'events': []})
        if w == 'desktop/accounts':
            return self.j({'ok': True, 'setup': True, 'guest': True, 'last': 'uAnnA1b2', 'accounts': [{'id': 'uAnnA1b2', 'n': 'Ann', 'r': 'admin', 'key': False, 'old': False}, {'id': 'uBobC3d4', 'n': 'Bob', 'r': 'user', 'key': False, 'old': False}]})
        if w == 'desktop/login':
            o = json.loads(b or b'{}')
            if o.get('guest'): s['session'] = {'ok': True, 'id': 'guest', 'n': 'Guest', 'r': 'guest', 'pk': '', 'fk': ''}; return self.j({'ok': True, 'id': 'guest', 'n': 'Guest', 'r': 'guest'})
            k = KEYS.get('Ann')
            if (o.get('n') or '').lower() != 'ann' or not k or o.get('p') != k['pw']: return self.err(403, 'The user name or password is incorrect.')
            s['session'] = {'ok': True, 'id': k['id'], 'n': 'Ann', 'r': 'admin', 'pk': k['pk'], 'fk': k['fk']}; s['locked'] = False
            return self.j({'ok': True, 'id': k['id'], 'n': 'Ann', 'r': 'admin'})
        if w == 'desktop/session':
            if not s['session']: return self.err(404, 'no native session')
            if s['locked']: return self.err(423, 'the screen is locked')
            return self.j(s['session'])
        if w == 'desktop/logout': s['session'] = None; s['locked'] = False; return self.j({'ok': True, 'message': 'signed out'})
        if w == 'desktop/lock': s['locked'] = True; return self.j({'ok': True})
        if w == 'desktop/unlock':
            o = json.loads(b or b'{}')
            if o.get('p') != KEYS['Ann']['pw']: return self.err(403, 'The password is incorrect.')
            s['locked'] = False; return self.j({'ok': True})
        if w == 'desktop/settings':
            p = os.path.join(D, 'settings.json'); mt = os.path.getmtime(p) if os.path.exists(p) else 0
            if mt != s['mt']: s['mt'] = mt; s['gen'] += 1
            o = json.loads(rd('settings.json', b'{}')); o.update({'ok': True, 'gen': s['gen'], 'acct': s['session']['id'] if s['session'] else ''})
            if not s['session']: o.update({'pins': [], 'notes': 0, 'wallpaper': False})
            return self.j(o)
        if w == 'desktop/apps':
            if m == 'POST': return self.j({'ok': True})
            apps = [{'k': 'notepad', 'name': 'Notepad', 'icon': '🗒️', 'kw': 'text'}, {'k': 'calc', 'name': 'Calculator', 'icon': '🧮', 'kw': 'sums'},
                    {'k': 'settings', 'name': 'Settings', 'icon': '⚙️', 'kw': ''}, {'k': 'clock', 'name': 'Clock', 'icon': '⏰', 'kw': 'alarm'}]
            dyn = {}
            try:
                import re
                h = open(os.path.join(WWW, 'index.html'), encoding='utf-8').read(); mm = re.search(r'const DYN=(\{.*?\});', h)
                for k, v in json.loads(mm.group(1)).items(): dyn[k] = {'n': v.get('n', k), 'd': v.get('d', ''), 'l': v.get('l', '')}
            except Exception: pass
            return self.j({'ok': True, 'apps': apps, 'bg': dyn})
        if w == 'desktop/wallpaper':
            x = rd('wall.jpg'); return self.send(200, x, 'image/jpeg') if x else self.err(404, 'no picture')
        if w == 'net': return self.j({'ok': True, 'interfaces': [{'name': 'lo', 'ip': '127.0.0.1', 'state': 'up', 'wireless': False}, {'name': 'wlan0', 'ip': '192.168.1.5', 'state': 'up', 'wireless': True}]})
        if w == 'volume': return self.j({'ok': True, 'volume': 70, 'muted': False})
        if w == 'info': return self.j({'ok': True, 'battery': [{'name': 'BAT0', 'type': 'Battery', 'capacity': 81, 'status': 'Discharging'}]})
        return self.j({'ok': True})
class S(http.server.ThreadingHTTPServer):
    daemon_threads = True
    def handle_error(self, *a): pass
S(('127.0.0.1', PORT), H).serve_forever()
