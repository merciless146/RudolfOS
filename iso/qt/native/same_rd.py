# 1.6.0 (plan Part 12, "The rule"): a stand-in rudolfd for same_t.sh, serving the SAME state to both engines:
#   - to Standard (the desktop page in Chromium): the real page, with the page's storage handed in as rudolfd hands it
#     (rudolfd.c serve_index: the "rudolf-ls" copy and its LSJS script, taken from rudolfd.c itself), the engine
#     "chromium", and POST lsync kept (so what the page writes is what the shell reads);
#   - to Native (rudolf-native): desktop/* answered from that same storage as rudolfd-desktop.inc answers them
#     (desktop/settings from bd95set:ID, bd95pins:ID and bd95notif:ID; desktop/apps as the page gave it).
# Files in DIR:  ls.json (the storage: {"gen":N,"data":{...}}, the test rewrites it between cases), keys.json (Ann's
# password, fixtures/deskls.js), calls (every request), session (when there: Native starts signed in as Ann),
# wall.jpg (a picture of your own), tm/ (test media, served at /tm/NAME).        python3 same_rd.py PORT DIR
import http.server, json, os, re, sys, threading, time
from urllib.parse import unquote as monunq
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
# (1.6.3 Part 8, item 4, deskicons163_t.js) the accounts' homes on the disk, as rudolfd's home/* keeps them (rudolfd-home.inc), in
# DIR/dh/ACCOUNT: the folder, its tree with each file's version, reading, writing, folders, deleting, renaming and copying; the key
# is not checked (the test's), nothing is encrypted
def disk_home(h, m, w, q, b):
    import urllib.parse, shutil
    a = urllib.parse.parse_qs(q); acct = re.sub(r'[^A-Za-z0-9_-]', '', (a.get('acct') or [''])[0]); root = os.path.join(D, 'dh', acct)
    def at(k):
        rel = (a.get(k) or [''])[0]; f = os.path.normpath(os.path.join(root, rel))
        return f if acct and rel and (f + os.sep).startswith(root + os.sep) and f != root else None
    ver = lambda f: '%d-%d' % (os.stat(f).st_mtime_ns, os.path.getsize(f))
    if w == 'home': return h.j({'ok': True, 'supported': True})
    if not acct: return h.err(400, 'no account')
    if w == 'home/unlock' and m == 'POST':
        made = not os.path.isdir(root); os.makedirs(root, exist_ok=True); return h.j({'ok': True, 'message': 'made' if made else 'opened'})
    if w in ('home/lock', 'home/forget', 'home/restore') and m == 'POST': return h.j({'ok': True, 'message': w})
    if w == 'home/recovered': return h.j({'ok': True, 'items': []})
    if w == 'home/tree':
        items = []
        for d, ds, fs in os.walk(root):
            for n in ds: f = os.path.join(d, n); items.append({'p': os.path.relpath(f, root), 'd': 1})
            for n in fs: f = os.path.join(d, n); items.append({'p': os.path.relpath(f, root), 's': os.path.getsize(f), 'v': ver(f)})
        return h.j({'ok': True, 'items': items})
    if w == 'home/read':
        f = at('path')
        if not f or not os.path.isfile(f): return h.err(404, 'no such file')
        return h.send(200, open(f, 'rb').read(), 'application/octet-stream')
    if w == 'home/write' and m == 'POST':
        f = at('path')
        if not f: return h.err(400, 'bad path')
        os.makedirs(os.path.dirname(f), exist_ok=True); open(f + '.t', 'wb').write(b or b''); os.replace(f + '.t', f); return h.j({'ok': True, 'v': ver(f)})
    if w == 'home/append' and m == 'POST':
        f = at('path')
        if not f: return h.err(400, 'bad path')
        os.makedirs(os.path.dirname(f), exist_ok=True); open(f, 'ab').write(b or b''); return h.j({'ok': True, 'v': ver(f)})
    if w == 'home/mkdir' and m == 'POST':
        f = at('path')
        if not f: return h.err(400, 'bad path')
        os.makedirs(f, exist_ok=True); return h.j({'ok': True})
    if w == 'home/delete' and m == 'POST':
        f = at('path')
        if f and os.path.isdir(f): shutil.rmtree(f)
        elif f and os.path.exists(f): os.unlink(f)
        return h.j({'ok': True, 'message': 'deleted'})
    if w in ('home/rename', 'home/copy') and m == 'POST':
        f, t = at('from'), at('to')
        if not f or not t or not os.path.exists(f): return h.err(404, 'no such file')
        os.makedirs(os.path.dirname(t), exist_ok=True)
        if w == 'home/rename': os.replace(f, t)
        elif os.path.isdir(f): shutil.copytree(f, t, dirs_exist_ok=True)
        else: shutil.copyfile(f, t)
        return h.j({'ok': True, 'v': ver(t) if os.path.isfile(t) else ''})
    if w in ('home/list', 'home/hash', 'home/link', 'home/adopt'): return h.j({'ok': True, 'items': []})
    return False
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
    # (the engine the page is told it runs in, as rudolfd's /run/rudolf/desktop-engine: "chromium" for both unless DIR/engine is there,
    # then "native" for WebKit's pages, Native's)
    def engine(self): return 'native' if os.path.exists(os.path.join(D, 'engine')) and 'Chrome' not in (self.headers.get('User-Agent') or '') else 'chromium'
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
            inj = ('<script type="application/json" id="rudolf-ls">%s</script><script>window.RUDOLFD={token:"%s",base:"",boot:"uefi",media:"disk",live:0,recovery:0,engine:"%s"};%s</script>'
                   % (L, TOK, self.engine(), LSJS))
            return self.send(200, open(os.path.join(WWW, 'index.html'), encoding='utf-8').read().replace('<head>', '<head>' + inj, 1), 'text/html; charset=utf-8')
        if u == '/pkg/rudolf-emberreach/www/index.html' and rd('progs') is not None:   # (DIR/progs: a Library web app's page, appsbeh_t.js)
            note('PKG ' + u); return self.send(200, '<!doctype html><title>Ember Reach</title><body style="background:#331"><h1 id=er>Ember Reach</h1>', 'text/html; charset=utf-8')
        if u.startswith('/tm/'):   # (the test's media: DIR/tm/NAME, made by the test with ffmpeg; test finding N1)
            f = os.path.join(D, 'tm', os.path.basename(u))
            if os.path.isfile(f):
                return self.send(200, open(f, 'rb').read(), {'.webm': 'video/webm', '.mp4': 'video/mp4', '.ogg': 'audio/ogg', '.wav': 'audio/wav'}.get(os.path.splitext(f)[1], 'application/octet-stream'))
        f = os.path.normpath(os.path.join(WWW, u.lstrip('/')))
        if f.startswith(os.path.normpath(WWW)) and os.path.isfile(f):
            ct = {'.js': 'text/javascript', '.webp': 'image/webp', '.jpg': 'image/jpeg', '.png': 'image/png', '.woff2': 'font/woff2', '.woff': 'font/woff', '.wasm': 'application/wasm', '.svg': 'image/svg+xml'}.get(os.path.splitext(f)[1], 'application/octet-stream')
            return self.send(200, open(f, 'rb').read(), ct)
        self.send(404, 'not found', 'text/plain')
    def api(self, m, w, q, b):
        L = ls(); data = L['data']; s = session()
        if (w == 'home' or w.startswith('home/')) and rd('diskhome') is not None:   # (1.6.3 Part 8, item 4: DIR/diskhome: the accounts' homes on the disk)
            r = disk_home(self, m, w, q, b)
            if r is not False: return r
        if w in ('events/wait', 'events') and rd('wm-native') is not None and 'shell=1' not in q:   # (1.6.0: rudolfd's on the native desktop: the shell's only)
            time.sleep(2); return self.j({'ok': True, 'events': '', 'native': True})
        if w in ('events/wait', 'events') and 'shell=1' in q and rd('std-on') is not None:   # (Standard's case: the native shell, left running from the case before, is not this machine's desktop now and takes none of its events)
            time.sleep(2); return self.j({'ok': True, 'events': ''})
        if w == 'events/wait':   # (DIR/event: one event for the page that asks first, as rudolfd's)
            for _ in range(30):
                with LOCK:
                    e = rd('event')
                    if e:
                        os.unlink(os.path.join(D, 'event')); return self.j({'ok': True, 'events': e.decode()})
                time.sleep(.1)
            return self.j({'ok': True, 'events': ''})
        if w == 'lsync':
            if m == 'POST':
                o = json.loads(b); g = max(int(o.get('gen') or 0), int(L.get('gen') or 0) + 1)
                # (1.6.0: a native page's part, its changed keys put into the copy, as rudolfd-desktop.inc lsync_merge does)
                if o.get('part'):
                    d = dict(L.get('data') or {})
                    for k in o.get('del') or []: d.pop(k, None)
                    d.update({k: v for k, v in (o.get('set') or {}).items() if isinstance(v, str)}); o = {'gen': g, 'data': d}
                o['gen'] = g
                p = os.path.join(D, 'ls.json'); open(p + '.t', 'w').write(json.dumps(o)); os.replace(p + '.t', p)
                return self.j({'ok': True, 'message': 'saved', 'gen': g})
            if q.startswith('key='):   # (one key of the copy, as rudolfd's api_lsync_get)
                import urllib.parse; v = (L.get('data') or {}).get(urllib.parse.unquote(q[4:]))
                return self.j({'ok': True, 'gen': L.get('gen') or 0, 'v': v if isinstance(v, str) else None})
            return self.j({'ok': True, 'message': 'saved'})
        # (1.6.2 Part 39, taskmgr_t.js) DIR/tm: Task Manager's requests answered from a JSON file ({"processes": {...}, "system": {...}, ...}: the
        # same for the page's Task Manager and the native one); the actions taken answered as rudolfd answers them (and noted in DIR/calls)
        if rd('tm') is not None:
            T = json.loads(rd('tm'))
            if w in T and w not in ('bt', 'remote') and not (w == 'job' and rd('wifijob')) and (m == 'GET' or w in ('apps/running', 'wifi/scan', 'print/jobs')): return self.j(T[w])
            if w == 'proc/end' and m == 'POST': return self.j({'ok': True, 'message': 'proc', 'ended': 3, 'kept': 0} if 'tree=1' in q else {'ok': True, 'message': 'proc'})
            if w in ('service/restart', 'apps/end', 'apps/kill', 'shell/switch') and m == 'POST': return self.j({'ok': True, 'message': w})
            if w == 'memory/dropcache' and m == 'POST': return self.j({'ok': True, 'freed': 104857600})
            if w == 'desktop/priority': return self.j({'ok': True, 'on': False})
            # (Settings › Bring in files, settings_t.js U14-U15: rudolfd's bring/* answered ok; the account's files from DIR/home, a Range read as
            # rudolfd's api_home_read answers it, home/delete removing one)
            if w.startswith('bring/') and m == 'POST': return self.j({'ok': True, 'message': w})
            # (Settings › Volume Controls, settings_t.js V-checklist: the sound card, the microphone, the echo canceller, as rudolfd answers them)
            if w in ('audio', 'audio/mic') and m == 'POST': return self.j({'ok': True, 'message': 'sound card set' if w == 'audio' else 'microphone chosen'})
            if w == 'sound/echo' and m == 'POST': return self.j({'ok': True, 'on': 'on=1' in q, 'active': 'on=1' in q})
            # (Settings › Network, settings_t.js N-checklist: rudolfd's wifi/*, net/static, net/dhcp, proxy, net/metered, net/hotspot and vpn/* answered as rudolfd does,
            # each body kept in DIR/body-NAME; a Wi-Fi connection's job finished at once)
            if m == 'POST' and 'net' in T and (w.startswith('wifi/') or w.startswith('vpn/') or w in ('net/static', 'net/dhcp', 'proxy', 'net/metered', 'net/hotspot')):
                open(os.path.join(D, 'body-' + w.replace('/', '_')), 'wb').write(b)
                if w == 'wifi/connect': open(os.path.join(D, 'wifijob'), 'w').write(b.decode().split('\n')[0])
                if w.startswith('vpn/') and w != 'vpn/on' and w != 'vpn/off':
                    o = json.loads(b or b'{}'); k = KEYS.get('Ann')
                    if not k or o.get('n') != 'Ann' or o.get('p') != k['pw']: return self.err(403, 'Wrong password.')
                return self.j({'ok': True, 'message': {'wifi/forget': 'Forgotten.', 'net/static': 'Manual address set.', 'net/dhcp': 'Automatic address.', 'proxy': 'Proxy saved.', 'net/hotspot': 'Sharing.'}.get(w, 'Done.')})
            if w == 'job' and m == 'GET' and rd('wifijob'):
                n = rd('wifijob').decode(); os.unlink(os.path.join(D, 'wifijob')); return self.j({'ok': True, 'running': False, 'name': 'wifi', 'status': 0, 'log': 'Connected to ' + n})
            # (Settings › Oracle, settings_t.js O-checklist: rudolfd's shell?on= and shell/switch)
            if m == 'POST' and w in ('shell', 'shell/switch') and 'net' in T and not rd('engine'): return self.j({'ok': True, 'on': 'on=1' in q, 'message': 'saved'})
            # (Settings › Mobile network, settings_t.js M-checklist: rudolfd's mobile/settings, mobile/pin, mobile/puk as rudolfd answers them; the PIN's body kept in DIR/mobpin)
            if w in ('mobile/settings', 'mobile/pin', 'mobile/puk') and m == 'POST' and 'mobile/settings' in T:
                if w == 'mobile/settings': return self.j({'ok': True, 'message': 'saved'})
                open(os.path.join(D, 'mobpin'), 'wb').write(b); o = json.loads(b or b'{}'); k = KEYS.get('Ann')
                if not k or o.get('n') != 'Ann' or o.get('p') != k['pw']: return self.err(403, 'Wrong password.')
                return self.j({'ok': True, 'message': 'PIN accepted: the SIM is unlocked.'}) if o.get('pin') == '1234' else self.err(409, 'Wrong PIN. 1 tries left before the SIM asks for its PUK.')
            # (Settings › Remote access, settings_t.js R-checklist: rudolfd's remote, remote?on=, remote/end, remote/accounts (kept in DIR/remote-accounts) and health)
            if (w == 'remote' or w.startswith('remote/')) and 'remote' in T:
                rp = os.path.join(D, 'remote.json'); S = json.loads(rd('remote.json') or json.dumps(T['remote']))
                if m == 'GET': return self.j(S)
                if w == 'remote/accounts': open(os.path.join(D, 'remote-accounts'), 'wb').write(b); return self.j({'ok': True, 'message': 'stored'})
                if w == 'remote/end': S['session'] = None; open(rp, 'w').write(json.dumps(S)); return self.j({'ok': True, 'message': 'ending'})
                if w == 'remote':
                    S['on'] = 'on=1' in q; S['running'] = S['on']; open(rp, 'w').write(json.dumps(S)); return self.j({'ok': True, 'message': 'ok'})
            if w == 'health' and m == 'POST' and 'remote' in T: open(os.path.join(D, 'health'), 'ab').write(b + b'\n'); return self.j({'ok': True})
            # (Settings › Bluetooth, settings_t.js B-checklist: rudolfd's bt/* as rudolf-devices answers them, the status kept in DIR/bt.json from settings_rd.json's)
            if w.startswith('bt') and 'bt' in T:
                bp = os.path.join(D, 'bt.json'); S = json.loads(rd('bt.json') or json.dumps(T['bt']))
                def bsave(): open(bp, 'w').write(json.dumps(S))
                import urllib.parse; Q = dict(urllib.parse.parse_qsl(q)); dv = next((x for x in S['devices'] if x['mac'] == Q.get('mac')), None)
                if w == 'bt': return self.j(S)
                if w == 'bt/ask': return self.j({'ok': True, 'ask': json.loads(rd('btask')) if rd('btask') else None})
                if m != 'POST': return self.err(404, 'unknown Bluetooth request')
                if w == 'bt/answer':
                    try: os.unlink(os.path.join(D, 'btask'))
                    except OSError: pass
                    return self.j({'ok': True})
                if w == 'bt/power': S['powered'] = Q.get('on') == '1'; bsave(); return self.j(S)
                if w == 'bt/visible': S['visible'] = True; bsave(); return self.j(S)
                if w == 'bt/scan': time.sleep(.5); bsave(); return self.j(S)
                if w in ('bt/pair', 'bt/connect', 'bt/disconnect', 'bt/remove'):
                    if not dv: return self.j({'ok': False, 'message': '', 'error': 'it did not work', 'status': S}, 409)
                    if w == 'bt/pair': dv['paired'] = dv['connected'] = True
                    elif w == 'bt/connect': dv['connected'] = True
                    elif w == 'bt/disconnect': dv['connected'] = False
                    else: S['devices'].remove(dv)
                    bsave(); return self.j({'ok': True, 'message': '', 'error': '', 'status': S})
            if w in ('home/read', 'home/delete'):
                import urllib.parse; rel = urllib.parse.parse_qs(q).get('path', [''])[0]; f = os.path.normpath(os.path.join(D, 'home', rel))
                if not f.startswith(os.path.join(D, 'home') + os.sep) or not os.path.isfile(f): return self.err(404, 'no such file')
                if w == 'home/delete' and m == 'POST': os.unlink(f); return self.j({'ok': True, 'message': 'deleted'})
                x = open(f, 'rb').read(); r = re.fullmatch(r'bytes=(-?\d+)(?:-(\d+))?', self.headers.get('Range') or '')
                if r and r.group(1).startswith('-'): x = x[int(r.group(1)):]
                elif r: x = x[int(r.group(1)):(int(r.group(2)) + 1 if r.group(2) else None)]
                return self.send(206 if r else 200, x, 'application/octet-stream')
        if w == 'desktop/admin' and m == 'POST':   # (1.6.2 Part 39: an administrator's permission, as rudolfd-desktop.inc checks it)
            o = json.loads(b or b'{}'); k = KEYS.get('Ann')
            return self.j({'ok': True, 'message': 'allowed'}) if k and o.get('n') == 'Ann' and o.get('p') == k['pw'] else self.err(403, 'Wrong password.')
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
        # (item 6) the time since the last input (DIR/idle, ms; none: unknown) and what is playing (DIR/media, JSON), as the test sets them
        if w == 'idle':
            x = rd('idle'); return self.j({'ok': True, 'idle': int(x) if x and x.strip() else None})
        if w == 'media': return self.j(dict({'ok': True, 'playing': 0, 'mic': 0}, **json.loads(rd('media', b'{}') or b'{}')))
        if w == 'desktop/unlock':
            o = json.loads(b or b'{}')
            if o.get('p') != KEYS['Ann']['pw']: return self.err(403, 'The password is incorrect.')
            ST['locked'] = False; return self.j({'ok': True})
        if w == 'desktop/settings':
            # (rudolfd-desktop.inc api_desktop_settings)
            sid = s['id'] if s else ''
            st = sget(data, 'bd95set:' + sid) if sid else None
            if st is None: st = sget(data, 'bd95set') or {}
            K = ["bs", "b1", "b2", "dyn", "dm", "lat", "wpimg", "wpfit", "tb", "tbstyle", "tbac", "tbacm", "tbbstyle", "tbbac", "tbbacm", "twstyle", "twacm", "accent", "darkui", "ck", "h24", "df", "tz", "doff", "ff", "fs", "lang", "scale", "icons", "icstyle", "nl", "dnd", "sr", "mag", "cb", "vk", "touch", "tx", "tbpcol"]
            pins = sget(data, 'bd95pins:' + (sid or 'nobody'))
            nt = sget(data, 'bd95notif:' + (sid or 'nobody')) or []
            wall = bool(sid and st.get('wpimg') and rd('wall.jpg'))
            # (1.6.0 item 2) the widgets: the shell's own list while the page's is the one it replaced, else the page's
            wraw = data.get('bd95widgets:' + sid) if sid else None; own = None
            try: own = json.loads(rd('widgets.json', b'null'))
            except ValueError: pass
            wgown = bool(sid and own and own.get('was') == (wraw or ''))
            wg = own['list'] if wgown else (sget(data, 'bd95widgets:' + sid) if sid else None)
            K = K + ["nlv", "nlsched", "magz"]   # (1.6.0: Night light's strength and hours, the magnifier's zoom)
            return self.j({'ok': True, 'gen': L.get('gen', 1), 'acct': sid, 's': {k: st[k] for k in K if k in st},
                           'pins': pins if isinstance(pins, list) else ["explorer", "browser", "notepad", "settings"], 'notes': len(nt), 'wallpaper': wall, 'wpacct': sid if wall else '',
                           'widgets': wg if isinstance(wg, list) else [], 'wgown': wgown, 'wloc': sget(data, 'bd95wloc'), 'wunit': sget(data, 'bd95wunit') or '', 'cal': (sget(data, 'bd95cal:' + sid) if sid else None) or [], 'notif': nt if isinstance(nt, list) else [], 'kb': (sget(data, 'bd95kb:' + sid) if sid else None) or {}, 'sys': sget(data, 'bd95sys') or {}})
        if w == 'desktop/widgets':
            if m == 'POST':
                sid = s['id'] if s else ''; open(os.path.join(D, 'widgets.json'), 'w').write(json.dumps({'list': json.loads(b), 'was': data.get('bd95widgets:' + sid) or ''})); return self.j({'ok': True})
            x = rd('widgets.json'); return self.send(200, x) if x else self.err(404, 'none')
        if w == 'desktop/apps':
            # (1.6.0: each account's last one too, apps.ACCT.json, given for desktop/apps?acct=ACCT)
            import urllib.parse, re as _re; qa = urllib.parse.parse_qs(q or '').get('acct', [''])[0]
            if m == 'POST':
                open(os.path.join(D, 'apps.json'), 'wb').write(b)
                try: pa = json.loads(b).get('acct') or ''
                except ValueError: pa = ''
                if _re.fullmatch(r'[A-Za-z0-9-]{1,64}', pa): open(os.path.join(D, 'apps.' + pa + '.json'), 'wb').write(b)
                return self.j({'ok': True})
            given = (rd('apps.' + qa + '.json') if _re.fullmatch(r'[A-Za-z0-9-]{1,64}', qa) else None) or rd('apps.json')
            return self.send(200, given) if given else self.err(404, 'no list yet')
        if w == 'desktop/cells':
            if m == 'POST': open(os.path.join(D, 'cells.json'), 'wb').write(b); return self.j({'ok': True})
            return self.send(200, rd('cells.json', b'{}'))
        if w == 'desktop/wallpaper':
            if m == 'POST': open(os.path.join(D, 'wall.jpg'), 'wb').write(b); return self.j({'ok': True})
            x = rd('wall.jpg'); return self.send(200, x, 'image/jpeg') if x else self.err(404, 'no picture')
        if w == 'sys/status':   # (the release, as rudolf-update's status: Settings › About's Chromium row comes from it on both engines)
            return self.j({'ok': True, 'current': {'version': '1.6.0', 'chromium': '142.0.7444.59', 'released': '2026-11-15'}})
        if w == 'desktop/engine' and m == 'GET' and rd('engine'): return self.j(dict({'ok': True}, **json.loads(rd('engine'))[self.engine()]))   # (DIR/engine: rudolfd's answer for each engine's page)
        if w == 'stats': return self.j({'ok': True, 'cpu': 12.3, 'load': 0.2, 'memTotal': 8589934592, 'memFree': 4294967296, 'memAvailable': 5368709120})
        if w == 'devlink': return self.j({'ok': True, 'available': True, 'on': False})
        # (1.6.2 Part 34: the Scan app's case: one scanner found, what it can do)
        if w == 'scan/find': return self.j({'ok': True, 'scanners': [{'id': 'escl:http://192.168.1.30:80/eSCL', 'name': 'Office Scanner', 'model': 'Office Scanner', 'via': 'network', 'proto': 'eSCL'}]})
        if w == 'scan/caps': return self.j({'ok': True, 'sources': [{'source': 'flatbed', 'width': 215.9, 'height': 296.9, 'resolutions': [75, 100, 150, 200, 300]}, {'source': 'adf', 'width': 215.9, 'height': 296.9, 'resolutions': [75, 100, 150, 200, 300]}], 'modes': ['color', 'grey', 'bw']})
        if w == 'mic':   # (DIR/mic: the apps using a microphone, a JSON list; muted=0|1 kept, as rudolfd's)
            if m == 'POST': ST['micmuted'] = q == 'muted=1'
            return self.j({'ok': True, 'users': json.loads(rd('mic', b'[]')), 'muted': ST.get('micmuted', False)})
        if w == 'vpn' and rd('vpn'): return self.j(json.loads(rd('vpn')))   # (1.6.2 Part 33: DIR/vpn, a case's VPNs, as rudolfd's GET vpn)
        if w == 'net' and rd('net'): return self.j(json.loads(rd('net')))   # (1.6.0 Part 5: DIR/net, a case's own network list: mobile data)
        if w == 'net': return self.j({'ok': True, 'interfaces': [{'name': 'wlan0', 'ip': '192.168.1.5/24', 'state': 'up', 'wireless': True, 'virtual': False, 'default': True, 'ssid': 'Home', 'rssi': -55}]})
        if w == 'volume':   # (kept: set=N, muted=0|1, step=up|down, as rudolfd's)
            if m == 'POST':
                for kv in q.split('&'):
                    k, _, v = kv.partition('=')
                    if k == 'set' and v.isdigit(): ST['vol'] = max(0, min(100, int(v))); ST['muted'] = False
                    if k == 'muted': ST['muted'] = v == '1'
                    if k == 'step' and v == 'mute': ST['muted'] = not ST.get('muted', False)
                    elif k == 'step': ST['vol'] = max(0, min(100, ST.get('vol', 70) + (5 if v == 'up' else -5))); ST['muted'] = False
            return self.j({'ok': True, 'volume': ST.get('vol', 70), 'muted': ST.get('muted', False)})
        # (item 8) Linux's and Blitzen's state for the Phone screen's and Blitzen's windows (DIR/deb, DIR/ai: JSON, as the test sets them)
        if w == 'debian/status': return self.j(dict({'ok': True}, **json.loads(rd('deb', b'{}') or b'{}')))
        if w == 'ai/status': return self.j(dict({'ok': True}, **json.loads(rd('ai', b'{}') or b'{}')))
        if w in ('mobile/status', 'mobile/connect'):   # (1.6.0 Part 3: DIR/mobile, "connected" or "registered": a modem with its SIM unlocked)
            st = (rd('mobile', b'') or b'').decode().strip()
            if w == 'mobile/connect' and m == 'POST' and st:
                st = 'connected' if 'on=1' in q else 'registered'
                with open(os.path.join(D, 'mobile'), 'w') as f: f.write(st)
                return self.j({'ok': True, 'message': 'Connected' if st == 'connected' else 'Disconnected'})
            mods = [{'id': 0, 'maker': 'Quectel', 'model': 'EG25-G', 'state': st, 'failed': '', 'locked': '', 'sim': True, 'iface': 'wwan0' if st == 'connected' else ''}] if st else []
            return self.j({'ok': True, 'running': bool(st), 'modems': mods, 'notes': [], 'route': 'eth0', 'cellular': False})
        if w == 'mobile/airplane':   # (1.6.0 Part 2: quick settings' Airplane mode; DIR/airplane: 1 or 0)
            if m == 'POST':
                with open(os.path.join(D, 'airplane'), 'w') as f: f.write('1' if 'on=1' in q else '0')
            return self.j({'ok': True, 'on': rd('airplane', b'0').strip() == b'1'})
        if w == 'power/status': return self.j({'ok': True, 'ac': False, 'battery': {'name': 'BAT0', 'capacity': 81, 'status': 'Discharging'}, 'now': {'saver': False}})
        # (the memory guard's "which page process is the desktop's": the stand-in has no processes; answered, the page would keep its
        # processor busy for 0.6 s by performance.now(), which the test's clock holds still, and never finish)
        if w == 'memory/iam': return self.err(404, 'no processes here')
        if w == 'info': return self.j({'ok': True, 'backlight': rd('backlight') is not None, 'battery': [{'name': 'BAT0', 'type': 'Battery', 'capacity': 81, 'status': 'Discharging'}]})
        # (1.6.0) DIR/backlight: the screen's brightness (0-100), as rudolfd's brightness and brightness/step (hotkey's 5 % steps, at least 5 %)
        if w in ('brightness', 'brightness/step'):
            x = rd('backlight')
            if x is None: return self.err(404, 'this screen has no adjustable backlight')
            v = int(x.strip() or b'70')
            if m == 'POST':
                if w == 'brightness/step': v = max(5, min(100, v + (5 if 'd=up' in q else -5)))
                elif 'value=' in q: v = max(5, min(100, int(q.split('value=')[1].split('&')[0] or 0)))
                with open(os.path.join(D, 'backlight'), 'w') as f: f.write(str(v))
            return self.j({'ok': True, 'brightness': v})
        # (2026-11-16) DIR/progs: the programs rudolfd knows, as on the owner's Vivobook (appsbeh_t.js): a Library web app (Ember Reach), a
        # Flatpak app (Discord), a Windows program in Debian's Wine (Starport GE); their icons are a plain square made here (no one's logo)
        if rd('progs') is not None:
            # (2026-11-17, the owner's Vivobook: Discord and Steam slow to open on Native, or never) DIR/slowapps: rudolfd's lists of the
            # installed programs answered after that many seconds, as a slow disk and a busy start may make them
            if w in ('pkg/status', 'apps/installed', 'debian/apps', 'debian/status') and (rd('slowapps') or '').strip(): time.sleep(float(rd('slowapps')))
            if w == 'pkg/status': return self.j({'ok': True, 'installed': [{'name': 'rudolf-emberreach', 'title': 'Ember Reach', 'kind': 'webapp', 'icon': '\U0001f525', 'entry': 'www/index.html', 'mounted': True, 'summary': 'A Library game', 'version': '1.0'}], 'available': [], 'sources': []})
            if w == 'apps/installed': return self.j({'ok': True, 'apps': [{'id': 'com.discordapp.Discord', 'name': 'Discord', 'launchers': [{'desktop': 'com.discordapp.Discord.desktop', 'name': 'Discord', 'icon': 'com.discordapp.Discord', 'mime': '', 'wmclass': 'discord', 'cats': 'Network;'}]}]})
            if w == 'debian/status': return self.j({'ok': True, 'installed': True, 'running': False})
            if w == 'debian/apps': return self.j({'ok': True, 'apps': [{'id': 'starport-ge', 'name': 'Starport GE', 'icon': '/usr/share/pixmaps/starport-ge.png', 'mime': '', 'wmclass': 'starport.exe', 'categories': 'Game;', 'menu': True}]})
            if w in ('apps/icon', 'debian/icon'): return self.send(200, square_png((200, 40, 160) if w == 'apps/icon' else (40, 160, 200)), 'image/png')
            if w in ('apps/run', 'debian/launch', 'pkg/run'): return self.j({'ok': True})
        # (1.6.1 Part 1) DIR/monitors: the monitors as rudolf-display --monitors writes them (/run/rudolf/monitors); screen/arrange
        # does what screenctl arrange and rudolf-display do: the main one first, the others to its right in that order, their workspaces
        if w == 'screen':
            mons = rd('monitors')
            return self.j({'ok': True, 'res': 'auto', 'fps': 60, 'display': 'gpu', 'monitors': json.loads(mons) if mons else None})
        if w == 'screen/arrange' and m == 'POST':
            mons = rd('monitors'); spec = monunq(q.partition('set=')[2].split('&')[0])
            if not mons: return self.err(400, 'the monitors are not known')
            M = json.loads(mons); by = {x['name']: x for x in M['monitors']}; L = []; x0 = 0
            for it in spec.split(','):
                n, _, ws = it.partition('=')
                if n not in by: return self.err(400, 'not the monitors plugged in')
                o = dict(by[n], ws=int(ws), main=not L, x=x0, y=0); x0 += o['w']; L.append(o)
            if len(L) != len(by): return self.err(400, 'not the monitors plugged in')
            M['monitors'] = L
            with open(os.path.join(D, 'monitors'), 'w') as f: f.write(json.dumps(M))
            return self.j({'ok': True, 'message': 'the displays are arranged anew'})
        if w == 'bootmenu': return self.j(dict({'ok': True, 'uefi': True, 'loader': True, 'windows': []}, **json.loads(rd('bootmenu', b'{}') or b'{}')))
        return self.j({'ok': True})
def square_png(c, n=48):   # (a plain square of one colour: a stand-in for a program's own icon)
    import zlib, struct
    raw = b''.join(b'\x00' + bytes(c) * n for _ in range(n))
    ch = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    return b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', n, n, 8, 2, 0, 0, 0)) + ch(b'IDAT', zlib.compress(raw)) + ch(b'IEND', b'')
class S(http.server.ThreadingHTTPServer):
    daemon_threads = True
    def handle_error(self, *a): pass
S(('127.0.0.1', PORT), H).serve_forever()
