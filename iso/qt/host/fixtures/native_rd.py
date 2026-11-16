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
#   DIR/dl-dir         (when there) the path of Chromium's download folder (Downloads/.incoming): dl/incoming, dl/read and
#                      dl/file as rudolfd answers them for it (oracle_native_t.sh); each dl/ request is noted with the engine
#                      that asked ("chrome" or "webkit", from its User-Agent)
#   DIR/idbm/          (when there) rudolfd's mirror of the pages' databases (rudolfd.c api_idbm), so a file one engine's page
#                      saved (Oracle's, in Chromium) is seen by a page started after it in the other (a WebKit window)
#   DIR/inject.js      (when there) a script given to the page before its own (a test's: no network status here)
#   DIR/torwin-fix     (when there) the path of fixtures/torwinapi.c built: torwin/* answered by rudolfd's own code (16.2)
#   DIR/tor-on         (made by tor/start) rudolfd's Tor stood in: tor/status says on (1.5.9, plan 16.1)
#   DIR/stats-on, DIR/shell.json, DIR/lsync.json   (1.6.0) stats answered; the shell's state; the pages' last lsync copy
#   DIR/health-on, DIR/system.json, DIR/sleep-ms   (1.6.0, hwcnative160_t.sh) the health log, the system's answer, sleep as rudolfd's
#   DIR/hotkey         (when there) the path of Rudolf OS's hotkey script: mic and volume answered through it as rudolfd does
#                      (the sound server: pactl, PULSE_SERVER from the environment)
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
# (1.6.0, when DIR/lsjs is there) rudolfd's own copy-keeping script (rudolfd.c LSJS, read from its source), so the pages send their
# storage here (lsync) as they do to rudolfd
def lsjs():
    import re
    src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../../sys/rudolfd.c'), encoding='utf-8').read()
    body = src[src.index('static const char LSJS[] ='):]; body = body[:body.index('";\n') + 2]
    body = re.sub(r'/\*.*?\*/', '', body, flags=re.S)
    return ''.join(json.loads('"' + m + '"') for m in re.findall(r'"((?:[^"\\]|\\.)*)"', body))
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
            ua = self.headers.get('User-Agent') or ''
            note('%s %s%s%s' % (m, u[5:], '?' + q if q else '', (' [%s]' % ('chrome' if 'Chrome/' in ua else 'webkit')) if u.startswith('/api/dl/') else ''))
            if self.headers.get('X-Rudolf-Token') != TOK: return self.err(403, 'missing token')
            return self.api(m, u[5:], q, b)
        if u in ('/', '/index.html'):
            note('PAGE /')
            # (DIR/page.html, when there: served instead of the desktop page, for the window manager's checks)
            alt = rd('page.html')
            if alt is not None: return self.send(200, alt, 'text/html; charset=utf-8')
            return self.send(200, open(os.path.join(WWW, 'index.html'), encoding='utf-8').read().replace('<head>', '<head>' + INJ + (('<script type="application/json" id="rudolf-ls">%s</script><script>%s</script>' % ((rd('lsfile.json') or b'').decode().replace('<', '\\u003c'), lsjs())) if rd('lsjs') is not None else '') + ('<script>%s</script>' % rd('inject.js').decode() if rd('inject.js') else ''), 1), 'text/html; charset=utf-8')
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
        # (1.5.9, plan 16.2) Standard's Tor windows: rudolfd's own torwin/* (fixtures/torwinapi.c, its path in DIR/torwin-fix)
        if w.startswith('torwin/') and rd('torwin-fix'):
            import subprocess
            o = subprocess.run([rd('torwin-fix').decode().strip(), '%s %s%s' % (m, w, '?' + q if q else '')], capture_output=True, timeout=30).stdout.decode()
            st, _, body = o.strip().partition('|'); code = int(st.split()[1]) if len(st.split()) > 1 and st.split()[1].isdigit() else 500
            return self.send(code, body)
        # (1.5.9, plan 16.1) rudolfd's Tor, stood in: on once started (DIR/tor-on), off when stopped at once
        if w in ('tor/start', 'tor/stop', 'tor/status'):
            if w == 'tor/start' and m == 'POST': open(os.path.join(D, 'tor-on'), 'w').write('1')
            if w == 'tor/stop' and m == 'POST' and 'now=1' in q:
                try: os.unlink(os.path.join(D, 'tor-on'))
                except OSError: pass
            on = os.path.exists(os.path.join(D, 'tor-on'))
            return self.j({'ok': True, 'state': 'on' if on else 'off', 'pct': 100 if on else 0, 'line': '', 'secs': 1, 'stopping': False})
        if w == 'net': return self.j({'ok': True, 'interfaces': [{'name': 'lo', 'ip': '127.0.0.1', 'state': 'up', 'wireless': False}, {'name': 'wlan0', 'ip': '192.168.1.5', 'state': 'up', 'wireless': True}]})
        if w.startswith('dl/') and rd('dl-dir') is not None: return self.dl(m, w[3:], q)
        if w in ('mic', 'volume') and rd('hotkey') is not None: return self.hotkey(m, w, q)
        if w.startswith('idbm/') and os.path.isdir(os.path.join(D, 'idbm')): return self.idbm(m, w[5:], q, b)
        if w == 'volume': return self.j({'ok': True, 'volume': 70, 'muted': False})
        if w == 'power/status': return self.j({'ok': True, 'ac': False, 'battery': {'name': 'BAT0', 'capacity': 81, 'status': 'Discharging'}})
        if w == 'info': return self.j({'ok': True, 'battery': [{'name': 'BAT0', 'type': 'Battery', 'capacity': 81, 'status': 'Discharging'}]})
        # (1.6.0, native160_t.sh) the system monitor's stats, when DIR/stats-on is there; the shell's state (shell26.js shellOffer)
        # from DIR/shell.json; the pages' lsync copies kept in DIR/lsync.json (the last one), counted in DIR/lsync-n, each noted in
        # DIR/lsync.log (part KEYS, or all N)
        if w == 'stats' and rd('stats-on') is not None: return self.j({'ok': True, 'cpu': 12.5, 'load': 0.1, 'memTotal': 4 << 30, 'memFree': 1 << 30, 'memAvailable': 3 << 30, 'battery': None, 'brightness': None, 'tempC': None, 'processes': ''})
        if w == 'shell' and rd('shell.json') is not None: return self.send(200, rd('shell.json'))
        # (1.6.0, hwcnative160_t.sh) when DIR/health-on is there: the health log (a start-up a minute ago, then what was noted: DIR/health.log);
        # DIR/system.json: rudolfd's system answer; GET sleep and a suspend as rudolfd's (sleep_now: DIR/sleep-ms is the time it slept, an
        # empty file a sleep the kernel refused)
        if w == 'health' and rd('health-on') is not None:
            import time
            if m == 'POST':
                with LOCK: open(os.path.join(D, 'health.log'), 'ab').write(b + b'\n')
                return self.j({'ok': True, 'message': 'noted'})
            ev = [json.loads(l) for l in (rd('health.log', b'') or b'').decode().splitlines() if l.strip().startswith('{')]
            ev = [{'t': int(time.time()) - (len(ev) - i), 'k': e.get('k'), 'm': e.get('m')} for i, e in enumerate(ev)]
            return self.j({'ok': True, 'now': int(time.time()), 'uptime': 40, 'events': list(reversed([{'t': int(time.time()) - 60, 'k': 'start', 'm': 'start-up (a)'}] + ev))})
        if w == 'system' and rd('system.json') is not None: return self.send(200, rd('system.json'))
        if w == 'sleep' and rd('sleep-ms') is not None:
            v = rd('sleep.json'); return self.send(200, b'{"ok":true,"last":' + (v or b'null') + b',"mode":"s2idle","wakealarm":true}')
        if w == 'power' and m == 'POST' and 'action=suspend' in q and rd('sleep-ms') is not None:
            import time; ms = (rd('sleep-ms') or b'').decode().strip(); n = int(rd('sleep-n', b'0') or 0) + 1; open(os.path.join(D, 'sleep-n'), 'w').write(str(n))
            rec = {'seq': int(time.time() * 1000), 'at': int(time.time()), 'asked': 10, 'slept_ms': int(ms) if ms else 0, 'ok': bool(ms), 'error': '' if ms else 'Device or resource busy',
                   'success': 1 if ms else 0, 'fail': 0 if ms else 1, 'hw_us': -1, 'mode': 's2idle'}
            open(os.path.join(D, 'sleep.json'), 'w').write(json.dumps(rec)); return self.j({'ok': True, 'message': 'suspending'})
        if w == 'lsync' and m == 'POST':
            with LOCK:
                open(os.path.join(D, 'lsync.json'), 'wb').write(b); n = int(rd('lsync-n', b'0') or 0) + 1; open(os.path.join(D, 'lsync-n'), 'w').write(str(n))
                # (the copy itself, as rudolfd keeps it: a page's part put into it, or all of a page's storage; its gen rudolfd's)
                o = json.loads(b or b'{}'); f = json.loads(rd('lsfile.json', b'{"gen":0,"data":{}}'))
                if o.get('part'):
                    for k in o.get('del') or []: f['data'].pop(k, None)
                    f['data'].update(o.get('set') or {})
                else: f['data'] = o.get('data') or {}
                f['gen'] = max(f['gen'] + 1, o.get('gen') or 0); open(os.path.join(D, 'lsfile.json'), 'w').write(json.dumps(f))
                open(os.path.join(D, 'lsync.log'), 'a').write(('part ' + ','.join(sorted(o.get('set') or {})) if o.get('part') else 'all %d' % len(o.get('data') or {})) + '\n')
            return self.j({'ok': True, 'message': 'saved', 'gen': f['gen']})
        if w == 'lsync' and q.startswith('key='):   # (one key of the copy, as rudolfd's api_lsync_get)
            import urllib.parse; f = json.loads(rd('lsfile.json', b'{"gen":0,"data":{}}')); v = (f.get('data') or {}).get(urllib.parse.unquote(q[4:]))
            open(os.path.join(D, 'lsync.log'), 'a').write('get %s\n' % urllib.parse.unquote(q[4:]))
            return self.j({'ok': True, 'gen': f['gen'], 'v': v if isinstance(v, str) else None})
        return self.j({'ok': True})
    def dl(self, m, what, q):
        # (rudolfd.c api_dl, for the incoming folder only: what the desktop page needs to file a small download)
        import urllib.parse; a = {k: v[0] for k, v in urllib.parse.parse_qs(q).items()}; inc = rd('dl-dir').decode().strip()
        name = a.get('name', '')
        if what == 'incoming':
            items = []
            for n in sorted(os.listdir(inc)) if os.path.isdir(inc) else []:
                f = os.path.join(inc, n)
                if n.startswith('.') or not os.path.isfile(f): continue
                st = os.stat(f); items.append({'name': n, 'size': st.st_size, 'mtime': int(st.st_mtime * 1000), 'partial': n.endswith('.crdownload')})
            return self.j({'ok': True, 'items': items})
        if what == 'space': return self.j({'ok': True, 'free': 1 << 34, 'total': 1 << 35, 'live': False})
        if not name or '/' in name or name.startswith('.'): return self.err(400, 'which file?')
        f = os.path.join(inc, name)
        if what == 'read':
            if a.get('from', 'incoming') != 'incoming' or not os.path.isfile(f): return self.err(404, 'the file is not there any more')
            b = open(f, 'rb').read(); off = int(a.get('off', '0')); n = int(a.get('len', str(8 << 20)))
            if off < 0: off = max(0, len(b) + off)
            return self.send(200, b[off:off + n], 'application/octet-stream')
        if what == 'file' and m == 'POST':
            if not os.path.isfile(f): return self.err(404, 'the file is not there any more')
            if a.get('to') == 'del': os.unlink(f); return self.j({'ok': True, 'message': 'deleted'})
            return self.err(400, 'to? (the stand-in deletes only)')
        return self.err(404, 'unknown download action')
    def idbm(self, m, what, q, b):
        import re, urllib.parse; I = os.path.join(D, 'idbm'); f = urllib.parse.parse_qs(q).get('f', [''])[0]
        with LOCK:
            gen = int(rd('idbm/gen', b'0') or 0); ready = os.path.exists(os.path.join(I, 'ready'))
            items = sorted(x for x in os.listdir(I) if re.fullmatch('[0-9a-f]{16}', x))
            if what == 'meta': return self.j({'ok': True, 'gen': gen, 'ready': ready, 'count': len(items)})
            if what == 'list': return self.j({'ok': True, 'gen': gen, 'ready': ready, 'items': items})
            if what == 'ready' and m == 'POST': open(os.path.join(I, 'ready'), 'w').write('1'); return self.j({'ok': True})
            if not re.fullmatch('[0-9a-f]{16}', f): return self.err(400, 'which record?')
            if what == 'get': x = rd('idbm/' + f); return self.send(200, x) if x is not None else self.err(404, 'no such record')
            if m != 'POST' or what not in ('put', 'del'): return self.err(404, 'unknown action')
            if what == 'put': open(os.path.join(I, f), 'wb').write(b)
            elif os.path.exists(os.path.join(I, f)): os.unlink(os.path.join(I, f))
            open(os.path.join(I, 'gen'), 'w').write(str(gen + 1)); return self.j({'ok': True, 'gen': gen + 1})
    def hotkey(self, m, w, q):
        # (rudolfd.c: GET/POST mic and volume, through /usr/lib/rudolf/hotkey)
        import subprocess, urllib.parse; a = {k: v[0] for k, v in urllib.parse.parse_qs(q).items()}; hk = rd('hotkey').decode().strip()
        env = dict(os.environ, HOTKEY_PACTL='pactl', HOTKEY_EVENTS=os.path.join(D, 'events'))
        run = lambda *x: subprocess.run(['sh', hk] + list(x), env=env, capture_output=True, text=True, timeout=10).stdout.strip()
        if w == 'mic':
            if m == 'POST':
                if a.get('muted') not in ('0', '1'): return self.err(400, 'muted is 0 or 1')
                run('mic', 'muted', a['muted'])
            return self.j({'ok': True, 'muted': run('mic', 'get') == 'muted', 'users': [x for x in run('mic', 'users').split('\n') if x]})
        if m != 'POST': o = run('volume', 'get')
        elif a.get('set', '').isdigit(): o = run('volume', 'set', a['set'])
        elif a.get('muted') in ('0', '1'): o = run('volume', 'muted', a['muted'])
        else: return self.err(400, 'set=0-100 or muted=0|1')
        v = o.split()
        if not v or not v[0].isdigit(): return self.err(503, 'the sound server is not running')
        return self.j({'ok': True, 'volume': int(v[0]), 'muted': len(v) > 1 and v[1] == 'muted'})
class S(http.server.ThreadingHTTPServer):
    daemon_threads = True
    def handle_error(self, *a): pass
S(('127.0.0.1', PORT), H).serve_forever()
