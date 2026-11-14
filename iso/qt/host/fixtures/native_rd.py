# 1.5.9 (plan Part 12): a stand-in rudolfd for rudolf-native's tests (native59_t.sh). Serves the real desktop page
# (iso/www, with the token and the account's storage put in as rudolfd's lsync copy would) and the desktop/* requests
# as rudolfd-desktop.inc answers them, from files in DIR the test changes:
#   DIR/settings.json  what desktop/settings answers (its "gen" goes up whenever the file changes)
#   DIR/ls.json, DIR/keys.json   the page's storage and Ann's keys (fixtures/deskls.js)
#   DIR/calls          every request, one per line (the test reads it)
#   DIR/wall.jpg       a picture of your own
#   DIR/apps.json      the start menu's list as the page gave it (POST desktop/apps), else a small one
#   DIR/devlink.json   the development link's state (devlink: off when missing)
#   DIR/page.html      (when there) served as the page instead of the desktop page: a plain page for the window
#                      manager's checks (a drop-down)
#   DIR/iwa-id         (when there) the shell's ID, served as /iwa/id (Oracle in the shell: oracle_native_t.sh)
#   DIR/idbm/          the page's database mirror (idbm/*, as rudolfd.c's api_idbm: one file a record, a generation)
#   DIR/incoming/      Chromium's download folder (dl/incoming, dl/read, dl/file: del and keep, into DIR/dlhome/ACCT/)
#   DIR/mic.json       what mic answers ({"users": [...], "muted": false}); POST mic?muted=N changes "muted"
# Ann's password is in keys.json; a wrong one gets the page's message.   python3 native_rd.py PORT DIR
import http.server, json, os, sys, threading, urllib.parse
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
            self.send_response(code); self.send_header('content-type', ctype); self.send_header('content-length', str(len(b))); self.send_header('cache-control', 'no-store'); self.end_headers(); self.wfile.write(b)
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
            # (DIR/page.html, when there: served instead of the desktop page, for the window manager's checks)
            alt = rd('page.html')
            if alt is not None: return self.send(200, alt, 'text/html; charset=utf-8')
            return self.send(200, open(os.path.join(WWW, 'index.html'), encoding='utf-8').read().replace('<head>', '<head>' + INJ, 1), 'text/html; charset=utf-8')
        if u == '/iwa/id' and rd('iwa-id') is not None: return self.send(200, rd('iwa-id'), 'text/plain')
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
            o['wpacct'] = s['session']['id'] if s['session'] and o.get('wallpaper') else ''
            return self.j(o)
        if w == 'desktop/apps':
            # (what the page gives, after a page signed in, is kept and given back, as rudolfd does)
            if m == 'POST': open(os.path.join(D, 'apps.json'), 'wb').write(b); return self.j({'ok': True})
            given = rd('apps.json')
            if given: return self.send(200, given)
            apps = [{'k': 'notepad', 'name': 'Notepad', 'icon': '🗒️', 'kw': 'text', 'ic': 'notepad'}, {'k': 'calc', 'name': 'Calculator', 'icon': '🧮', 'kw': 'sums'},
                    {'k': 'settings', 'name': 'Settings', 'icon': '⚙️', 'kw': ''}, {'k': 'clock', 'name': 'Clock', 'icon': '⏰', 'kw': 'alarm'}]
            dyn = {}
            try:
                import re
                h = open(os.path.join(WWW, 'index.html'), encoding='utf-8').read(); mm = re.search(r'const DYN=(\{.*?\});', h)
                for k, v in json.loads(mm.group(1)).items(): dyn[k] = {'n': v.get('n', k), 'd': v.get('d', ''), 'l': v.get('l', '')}
            except Exception: pass
            # Notepad's picture: a green square (a PNG, as the page makes them from its icons)
            import base64, struct, zlib
            def png(w, h, rgb):
                raw = b''.join(b'\0' + bytes(rgb) * w for _ in range(h)); ch = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
                return b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + ch(b'IDAT', zlib.compress(raw)) + ch(b'IEND', b'')
            icons = {'notepad': 'data:image/png;base64,' + base64.b64encode(png(48, 48, (16, 185, 129))).decode()}
            # the desktop's icons as the page lists them for the account, and the page's places (1.6.0 Part 12, item 1)
            desk = [{'n': 'Notepad', 'k': 'notepad', 'kind': 'app', 'ic': 'notepad', 'icon': '🗒️'}, {'n': 'Calculator', 'k': 'calc', 'kind': 'app', 'ic': '', 'icon': '🧮'},
                    {'n': 'My Computer', 'k': 'explorer', 'kind': 'computer', 'ic': '', 'icon': '🖥️'}]
            # the start menu's rows as the page gives them (1.6.0 Part 12: the shell draws the page's menu)
            row = lambda a, t, ic='': {'t': 'li', 'a': a, 'v': '', 'cls': '', 'txt': t, 'ic': ic, 'emo': '', 'op': 1, 'fs': 14, 'fw': 400, 'col': 'rgb(0, 0, 0)', 'pl': 10, 'pt': 7, 'pb': 7, 'ls': 0, 'up': False}
            menu = [row('notepad', 'Notepad', 'notepad'), row('calc', '🧮 Calculator'), row('clock', '⏰ Clock'), {'t': 'hr'}, row('settings', '⚙️ Settings'), {'t': 'hr'}, row('logoff', '🚪 Log Off'), row('off', '🔌 Shut Down...')]
            return self.j({'ok': True, 'apps': apps, 'bg': dyn, 'icons': icons, 'ver': '9.9.9', 'desk': desk, 'cells': {'Calculator': [2, 1]}, 'is': 100, 'menu': menu,
                           'acct': s['session']['id'] if s['session'] else ''})
        if w == 'desktop/cells':
            if m == 'POST': open(os.path.join(D, 'cells.json'), 'wb').write(b); return self.j({'ok': True})
            return self.send(200, rd('cells.json', b'{}'))
        if w == 'desktop/wallpaper':
            x = rd('wall.jpg'); return self.send(200, x, 'image/jpeg') if x else self.err(404, 'no picture')
        # the development link: DIR/devlink.json as rudolfd's devlink answers (off when missing); End now removes it,
        # an answer removes the question
        if w == 'devlink': return self.j(json.loads(rd('devlink.json', b'{"ok":true,"available":true,"on":false}')))
        if w == 'devlink/off':
            try: os.unlink(os.path.join(D, 'devlink.json'))
            except OSError: pass
            return self.j({'ok': True})
        if w == 'devlink/answer':
            o = json.loads(rd('devlink.json', b'{}')); o.pop('pending', None); open(os.path.join(D, 'devlink.json'), 'w').write(json.dumps(o)); return self.j({'ok': True})
        if w == 'net': return self.j({'ok': True, 'interfaces': [{'name': 'lo', 'ip': '127.0.0.1', 'state': 'up', 'wireless': False}, {'name': 'wlan0', 'ip': '192.168.1.5', 'state': 'up', 'wireless': True}]})
        if w == 'volume':
            v = json.loads(rd('volume.json', b'{"volume": 70, "muted": false}'))
            if m == 'POST':
                qs = urllib.parse.parse_qs(q)
                if 'set' in qs: v['volume'] = max(0, min(100, int(qs['set'][0]))); v['muted'] = False
                if 'muted' in qs: v['muted'] = qs['muted'][0] == '1'
                open(os.path.join(D, 'volume.json'), 'w').write(json.dumps(v))
            v['ok'] = True; return self.j(v)
        if w == 'mic':
            v = json.loads(rd('mic.json', b'{"users": [], "muted": false}'))
            if m == 'POST': v['muted'] = urllib.parse.parse_qs(q).get('muted', ['0'])[0] == '1'; open(os.path.join(D, 'mic.json'), 'w').write(json.dumps(v))
            v['ok'] = True; return self.j(v)
        if w.startswith('idbm/'): return self.idbm(m, w[5:], urllib.parse.parse_qs(q), b)
        if w.startswith('dl/'): return self.dl(m, w[3:], urllib.parse.parse_qs(q))
        if w == 'power/status': return self.j({'ok': True, 'ac': False, 'battery': {'name': 'BAT0', 'capacity': 81, 'status': 'Discharging'}})
        if w == 'info': return self.j({'ok': True, 'battery': [{'name': 'BAT0', 'type': 'Battery', 'capacity': 81, 'status': 'Discharging'}]})
        return self.j({'ok': True})
    # the page's database mirror, as rudolfd.c's api_idbm
    def idbm(self, m, w, qs, b):
        I = os.path.join(D, 'idbm'); os.makedirs(I, exist_ok=True); f = (qs.get('f') or [''])[0]
        gen = lambda: int((rd('idbm/gen') or b'0').decode() or 0)
        if w in ('meta', 'list'):
            items = sorted(x[:16] for x in os.listdir(I) if len(x) == 21 and x.endswith('.json'))
            o = {'ok': True, 'gen': gen(), 'ready': os.path.exists(os.path.join(I, 'ready')), 'count': len(items)}
            if w == 'list': o['items'] = items
            return self.j(o)
        ok = len(f) == 16 and all(c in '0123456789abcdef' for c in f)
        if w == 'get':
            x = rd('idbm/%s.json' % f) if ok else None
            return self.send(200, x) if x else self.err(404, 'no such record')
        if m != 'POST': return self.err(400, 'use POST')
        with LOCK:
            if w == 'put':
                if not ok or not b.startswith(b'{'): return self.err(400, 'bad record')
                open(os.path.join(I, '.%s.tmp' % f), 'wb').write(b); os.rename(os.path.join(I, '.%s.tmp' % f), os.path.join(I, '%s.json' % f))
            elif w == 'del':
                try: os.unlink(os.path.join(I, '%s.json' % f))
                except OSError: pass
            elif w == 'ready': open(os.path.join(I, 'ready'), 'w').write('1'); return self.j({'ok': True})
            else: return self.err(404, 'unknown mirror action')
            g = gen() + 1; open(os.path.join(I, 'gen'), 'w').write(str(g))
        return self.j({'ok': True, 'gen': g})
    # Chromium's download folder, as rudolfd.c's api_dl (incoming, read, file: del and keep, space)
    def dl(self, m, w, qs):
        IN = os.path.join(D, 'incoming'); os.makedirs(IN, exist_ok=True); g = lambda k, d='': (qs.get(k) or [d])[0]
        name = os.path.basename(g('name'))
        if w == 'incoming':
            items = []
            for x in sorted(os.listdir(IN)):
                if x.startswith('.'): continue
                st = os.stat(os.path.join(IN, x)); items.append({'name': x, 'size': st.st_size, 'mtime': int(st.st_mtime), 'partial': x.endswith('.crdownload')})
            return self.j({'ok': True, 'items': items})
        if w == 'space': return self.j({'ok': True, 'total': 100 << 30, 'free': 50 << 30, 'live': False})
        if w == 'read':
            try: data = open(os.path.join(IN, name), 'rb').read()
            except OSError: return self.err(404, 'no such file')
            off, ln = int(g('off', '0')), int(g('len', '0'))
            data = data[off:] if off >= 0 else data[off:]; return self.send(200, data[:ln] if ln > 0 else data, 'application/octet-stream')
        if w == 'file' and m == 'POST':
            src = os.path.join(IN, name)
            if not os.path.exists(src): return self.err(404, 'no such file')
            to = g('to')
            if to == 'del': os.unlink(src); return self.j({'ok': True, 'message': 'deleted'})
            if to == 'keep':
                dd = os.path.join(D, 'dlhome', os.path.basename(g('acct')) or 'nobody'); os.makedirs(dd, exist_ok=True); dst = os.path.join(dd, name); os.rename(src, dst)
                return self.j({'ok': True, 'path': '/home/rudolf/Downloads/.accounts/%s/%s' % (os.path.basename(g('acct')), name), 'size': os.path.getsize(dst)})
            return self.err(400, 'to?')
        return self.err(404, 'unknown download action')
class S(http.server.ThreadingHTTPServer):
    daemon_threads = True
    def handle_error(self, *a): pass
S(('127.0.0.1', PORT), H).serve_forever()
