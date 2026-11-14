# 1.6.0 (plan Part 12, "The rule"): a stand-in rudolfd for same_t.sh, serving the SAME state to both engines:
#   - to Standard (the desktop page in Chromium): the real page, with the page's storage handed in as rudolfd hands it
#     (rudolfd.c serve_index: the "rudolf-ls" copy and its LSJS script, taken from rudolfd.c itself), the engine
#     "chromium", and POST lsync kept (so what the page writes is what the shell reads);
#   - to Native (rudolf-native): desktop/* answered from that same storage as rudolfd-desktop.inc answers them
#     (desktop/settings from bd95set:ID, bd95pins:ID and bd95notif:ID; desktop/apps as the page gave it).
# Files in DIR:  ls.json (the storage: {"gen":N,"data":{...}}, the test rewrites it between cases), keys.json (Ann's
# password, fixtures/deskls.js), calls (every request), session (when there: Native starts signed in as Ann),
# wall.jpg (a picture of your own).        python3 same_rd.py PORT DIR
import http.server, json, os, re, sys, threading, time
PORT = int(sys.argv[1]); D = sys.argv[2]; HERE = os.path.dirname(os.path.abspath(__file__))
WWW = os.path.join(HERE, '../../www'); TOK = 'tok160same'; LOCK = threading.Lock()
ST = {'locked': False}
def rd(n, d=None):
    try: return open(os.path.join(D, n), 'rb').read()
    except OSError: return d
def note(s):
    with LOCK: open(os.path.join(D, 'calls'), 'a').write(s + '\n')
def ls():
    try: return json.loads(rd('ls.json', b'{"gen":1,"data":{}}'))
    except ValueError: return {'gen': 1, 'data': {}}
# rudolfd's own LSJS (the C string literal, unescaped)
SRC = open(os.path.join(HERE, '../../sys/rudolfd.c'), encoding='utf-8').read()
m = re.search(r'static const char LSJS\[\] =\s*((?:"(?:[^"\\]|\\.)*"\s*)+);', SRC)
LSJS = ''.join(json.loads('"' + p + '"') for p in re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1)))
KEYS = json.loads(rd('keys.json', b'{}'))
def session():
    s = rd('session')
    return json.loads(s) if s else None
def sget(data, k):
    try: return json.loads(data[k]) if k in data else None
    except ValueError: return None
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
        if u == '/v1/forecast':   # (Open-Meteo for Native: RUDOLF_WX_BASE; Standard's page gets the same through Playwright)
            note('WX ' + q); return self.send(200, open(os.path.join(HERE, 'same_rd_wx.json'), 'rb').read())
        if u == '/v1/search':
            note('GEO ' + q); return self.j({'results': [{'name': 'Berlin', 'latitude': 52.52, 'longitude': 13.41, 'country_code': 'DE', 'timezone': 'Europe/Berlin', 'admin1': 'Land Berlin', 'country': 'Germany'}]})
        if u in ('/', '/index.html'):
            note('PAGE /')
            L = json.dumps(ls()).replace('<', '\\u003c')
            inj = ('<script type="application/json" id="rudolf-ls">%s</script><script>window.RUDOLFD={token:"%s",base:"",boot:"uefi",media:"disk",live:0,recovery:0,engine:"chromium"};%s</script>'
                   % (L, TOK, LSJS))
            return self.send(200, open(os.path.join(WWW, 'index.html'), encoding='utf-8').read().replace('<head>', '<head>' + inj, 1), 'text/html; charset=utf-8')
        f = os.path.normpath(os.path.join(WWW, u.lstrip('/')))
        if f.startswith(os.path.normpath(WWW)) and os.path.isfile(f):
            ct = {'.js': 'text/javascript', '.webp': 'image/webp', '.jpg': 'image/jpeg', '.png': 'image/png', '.woff2': 'font/woff2', '.woff': 'font/woff', '.wasm': 'application/wasm', '.svg': 'image/svg+xml'}.get(os.path.splitext(f)[1], 'application/octet-stream')
            return self.send(200, open(f, 'rb').read(), ct)
        self.send(404, 'not found', 'text/plain')
    def api(self, m, w, q, b):
        L = ls(); data = L['data']; s = session()
        if w == 'events/wait': time.sleep(3); return self.j({'ok': True, 'events': []})
        if w == 'lsync':
            if m == 'POST':
                o = json.loads(b); o['gen'] = max(int(o.get('gen') or 0), int(L.get('gen') or 0) + 1)
                p = os.path.join(D, 'ls.json'); open(p + '.t', 'w').write(json.dumps(o)); os.replace(p + '.t', p)
            return self.j({'ok': True, 'message': 'saved'})
        if w == 'desktop/accounts':
            acc = (sget(data, 'bd95user') or {}).get('accts', [])
            last = max(acc, key=lambda a: a.get('last', 0))['id'] if acc else ''
            return self.j({'ok': True, 'setup': True, 'guest': True, 'last': last, 'accounts': [{'id': a['id'], 'n': a['n'], 'r': 'admin' if a.get('r') == 'admin' else 'user', 'key': False, 'old': not str(a.get('h', '')).startswith('p2$')} for a in acc]})
        if w == 'desktop/login':
            o = json.loads(b or b'{}'); k = KEYS.get('Ann')
            if (o.get('n') or '').lower() != 'ann' or not k or o.get('p') != k['pw']: return self.err(403, 'The user name or password is incorrect.')
            s = {'ok': True, 'id': k['id'], 'n': 'Ann', 'r': 'admin', 'pk': k['pk'], 'fk': k['fk']}; open(os.path.join(D, 'session'), 'w').write(json.dumps(s)); ST['locked'] = False
            return self.j({'ok': True, 'id': k['id'], 'n': 'Ann', 'r': 'admin'})
        if w == 'desktop/session':
            if not s: return self.err(404, 'no native session')
            if ST['locked']: return self.err(423, 'the screen is locked')
            return self.j(s)
        if w == 'desktop/logout':
            try: os.unlink(os.path.join(D, 'session'))
            except OSError: pass
            return self.j({'ok': True})
        if w == 'desktop/lock': ST['locked'] = True; return self.j({'ok': True})
        if w == 'desktop/unlock':
            o = json.loads(b or b'{}')
            if o.get('p') != KEYS['Ann']['pw']: return self.err(403, 'The password is incorrect.')
            ST['locked'] = False; return self.j({'ok': True})
        if w == 'desktop/settings':
            # (rudolfd-desktop.inc api_desktop_settings)
            sid = s['id'] if s else ''
            st = sget(data, 'bd95set:' + sid) if sid else None
            if st is None: st = sget(data, 'bd95set') or {}
            K = ["bs", "b1", "b2", "dyn", "dm", "lat", "wpimg", "wpfit", "tb", "tbstyle", "tbac", "tbacm", "tbbstyle", "tbbac", "tbbacm", "twstyle", "twacm", "accent", "darkui", "ck", "h24", "df", "tz", "doff", "ff", "fs", "lang", "scale", "icons", "icstyle"]
            pins = sget(data, 'bd95pins:' + (sid or 'nobody'))
            nt = sget(data, 'bd95notif:' + (sid or 'nobody')) or []
            wall = bool(sid and st.get('wpimg') and rd('wall.jpg'))
            # (1.6.0 item 2) the widgets: the shell's own list while the page's is the one it replaced, else the page's
            wraw = data.get('bd95widgets:' + sid) if sid else None; own = None
            try: own = json.loads(rd('widgets.json', b'null'))
            except ValueError: pass
            wgown = bool(sid and own and own.get('was') == (wraw or ''))
            wg = own['list'] if wgown else (sget(data, 'bd95widgets:' + sid) if sid else None)
            return self.j({'ok': True, 'gen': L.get('gen', 1), 'acct': sid, 's': {k: st[k] for k in K if k in st},
                           'pins': pins if isinstance(pins, list) else ["explorer", "browser", "notepad", "settings"], 'notes': len(nt), 'wallpaper': wall, 'wpacct': sid if wall else '',
                           'widgets': wg if isinstance(wg, list) else [], 'wgown': wgown, 'wloc': sget(data, 'bd95wloc'), 'wunit': sget(data, 'bd95wunit') or '', 'cal': (sget(data, 'bd95cal:' + sid) if sid else None) or []})
        if w == 'desktop/widgets':
            if m == 'POST':
                sid = s['id'] if s else ''; open(os.path.join(D, 'widgets.json'), 'w').write(json.dumps({'list': json.loads(b), 'was': data.get('bd95widgets:' + sid) or ''})); return self.j({'ok': True})
            x = rd('widgets.json'); return self.send(200, x) if x else self.err(404, 'none')
        if w == 'desktop/apps':
            if m == 'POST': open(os.path.join(D, 'apps.json'), 'wb').write(b); return self.j({'ok': True})
            given = rd('apps.json')
            return self.send(200, given) if given else self.err(404, 'no list yet')
        if w == 'desktop/cells':
            if m == 'POST': open(os.path.join(D, 'cells.json'), 'wb').write(b); return self.j({'ok': True})
            return self.send(200, rd('cells.json', b'{}'))
        if w == 'desktop/wallpaper':
            if m == 'POST': open(os.path.join(D, 'wall.jpg'), 'wb').write(b); return self.j({'ok': True})
            x = rd('wall.jpg'); return self.send(200, x, 'image/jpeg') if x else self.err(404, 'no picture')
        if w == 'stats': return self.j({'ok': True, 'cpu': 12.3, 'load': 0.2, 'memTotal': 8589934592, 'memFree': 4294967296, 'memAvailable': 5368709120})
        if w == 'devlink': return self.j({'ok': True, 'available': True, 'on': False})
        if w == 'net': return self.j({'ok': True, 'interfaces': [{'name': 'wlan0', 'ip': '192.168.1.5/24', 'state': 'up', 'wireless': True, 'virtual': False, 'default': True, 'ssid': 'Home', 'rssi': -55}]})
        if w == 'volume':   # (kept: set=N, muted=0|1, step=up|down, as rudolfd's)
            if m == 'POST':
                for kv in q.split('&'):
                    k, _, v = kv.partition('=')
                    if k == 'set' and v.isdigit(): ST['vol'] = max(0, min(100, int(v))); ST['muted'] = False
                    if k == 'muted': ST['muted'] = v == '1'
                    if k == 'step': ST['vol'] = max(0, min(100, ST.get('vol', 70) + (5 if v == 'up' else -5)))
            return self.j({'ok': True, 'volume': ST.get('vol', 70), 'muted': ST.get('muted', False)})
        if w == 'power/status': return self.j({'ok': True, 'ac': False, 'battery': {'name': 'BAT0', 'capacity': 81, 'status': 'Discharging'}, 'now': {'saver': False}})
        if w == 'info': return self.j({'ok': True, 'battery': [{'name': 'BAT0', 'type': 'Battery', 'capacity': 81, 'status': 'Discharging'}]})
        return self.j({'ok': True})
class S(http.server.ThreadingHTTPServer):
    daemon_threads = True
    def handle_error(self, *a): pass
S(('127.0.0.1', PORT), H).serve_forever()
