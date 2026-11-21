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
            K = ["bs", "b1", "b2", "dyn", "dm", "lat", "wpimg", "wpfit", "tb", "tbstyle", "tbac", "tbacm", "tbbstyle", "tbbac", "tbbacm", "twstyle", "twacm", "accent", "darkui", "ck", "h24", "df", "tz", "doff", "ff", "fs", "lang", "scale", "icons", "icstyle", "nl", "dnd", "sr", "mag", "cb", "vk", "touch", "tx"]
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
        if w == 'mic':   # (DIR/mic: the apps using a microphone, a JSON list; muted=0|1 kept, as rudolfd's)
            if m == 'POST': ST['micmuted'] = q == 'muted=1'
            return self.j({'ok': True, 'users': json.loads(rd('mic', b'[]')), 'muted': ST.get('micmuted', False)})
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
