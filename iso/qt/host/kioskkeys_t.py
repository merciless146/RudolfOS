#!/usr/bin/env python3
# 1.5.6: Chromium's own shortcuts in kiosk mode, pressed on a real X keyboard (XTest, Xvfb), against a page with the
# desktop's guard (desktop/keys56.js) and without it. Without it, Alt+F4 closed the desktop on the Vivobook, and in
# kiosk mode F5 reloads it, Ctrl+H puts the history over it, Shift+Esc opens Chromium's task manager...; with it,
# none of them does anything, and the page still sees every key.
#   python3 kioskkeys_t.py [CHROME]   (needs Xvfb, gcc, the X11 and Xtst libraries)
import subprocess, time, http.server, threading, os, sys, signal, tempfile, socket, struct
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.join(HERE, '..', '..', '..')
CH = sys.argv[1] if len(sys.argv) > 1 else None
if not CH:
    import glob
    c = sorted(glob.glob('/opt/pw-browsers/chromium-*/chrome-linux*/chrome')); CH = c[-1] if c else 'chromium'
T = tempfile.mkdtemp(); XK = os.path.join(T, 'xkeys')
L = '/usr/lib/x86_64-linux-gnu'
subprocess.run(['gcc', '-O2', '-o', XK, os.path.join(HERE, 'fixtures', 'xkeys.c'), '-lX11', L + '/libXtst.so.6'], check=True)
DISPLAY = ':81'; env = dict(os.environ, DISPLAY=DISPLAY)
GUARD = open(os.path.join(ROOT, 'desktop', 'keys56.js')).read()
S = {'guard': False, 'loads': 0, 'keys': 0, 'ev': []}
def page():
    return ("<!doctype html><html><body style='margin:0;background:#123'><input id=i autofocus><script>fetch('/hit?load');"
            "document.addEventListener('visibilitychange',()=>fetch('/hit?ev=vis-'+document.visibilityState));addEventListener('blur',()=>fetch('/hit?ev=blur'));"
            "addEventListener('keydown',()=>fetch('/hit?key'),true)</script>" + ("<script>" + GUARD + "</script>" if S['guard'] else '') + "</body></html>")
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        if self.path == '/hit?load': S['loads'] += 1
        elif self.path == '/hit?key': S['keys'] += 1
        elif self.path.startswith('/hit?ev='): S['ev'].append(self.path[8:])
        self.send_response(200); self.send_header('content-type', 'text/html'); self.end_headers()
        if not self.path.startswith('/hit'): self.wfile.write(page().encode())
srv = http.server.ThreadingHTTPServer(('127.0.0.1', 8150), H); threading.Thread(target=srv.serve_forever, daemon=True).start()
xv = subprocess.Popen(['Xvfb', DISPLAY, '-screen', '0', '1280x800x24', '-nolisten', 'tcp'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(1)
def viewable():   # the viewable top-level windows (the X protocol, no xwininfo)
    s = socket.socket(socket.AF_UNIX); s.connect('/tmp/.X11-unix/X' + DISPLAY[1:])
    def rd(n):
        b = b''
        while len(b) < n: b += s.recv(n - len(b))
        return b
    s.sendall(b'l\0' + struct.pack('<HHHH', 11, 0, 0, 0) + b'\0\0'); h = rd(8); d = rd(struct.unpack('<H', h[6:8])[0] * 4)
    off = 32 + ((struct.unpack('<H', d[16:18])[0] + 3) & ~3) + 8 * d[21]; root = struct.unpack('<I', d[off:off + 4])[0]
    def req(b):
        s.sendall(b); r = rd(32)
        return None if r[0] == 0 else r + rd(struct.unpack('<I', r[4:8])[0] * 4)
    r = req(struct.pack('<BBHI', 15, 0, 2, root)); n = struct.unpack('<H', r[16:18])[0]; out = 0
    for w in struct.unpack('<%dI' % n, r[32:32 + 4 * n]):
        a = req(struct.pack('<BBHI', 3, 0, 2, w))
        if a and a[26] == 2 and not a[27]: out += 1
    s.close(); return out
P = [None]
def start():
    prof = tempfile.mkdtemp(dir=T)
    P[0] = subprocess.Popen([CH, '--no-sandbox', '--class=RudolfDesktop', '--kiosk', '--start-fullscreen', '--window-position=0,0', '--window-size=1280,800',
        '--user-data-dir=' + prof, '--no-first-run', '--no-default-browser-check', '--noerrdialogs', '--disable-infobars', '--disable-session-crashed-bubble',
        '--password-store=basic', 'http://127.0.0.1:8150/'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
    l0 = S['loads']
    for i in range(120):
        time.sleep(0.25)
        if S['loads'] > l0 and viewable(): break
    time.sleep(1.5)
def stop():
    try: os.killpg(P[0].pid, signal.SIGKILL)
    except OSError: pass
    time.sleep(0.5)
def press(combo):
    """what the combination did: closed, reloaded, a new window, hidden or unfocused; and whether the page saw it"""
    if P[0] is None or P[0].poll() is not None: start()
    l0, w0, k0, e0 = S['loads'], viewable(), S['keys'], len(S['ev'])
    subprocess.run([XK, combo], env=env); time.sleep(2)
    alive = P[0].poll() is None; w1 = viewable() if alive else 0; did = []
    if not alive or not w1: did.append('closed')
    if S['loads'] > l0: did.append('reloaded')
    if alive and w1 > w0: did.append('new window')
    ev = S['ev'][e0:]
    if 'vis-hidden' in ev: did.append('hidden')
    elif 'blur' in ev: did.append('unfocused')
    if did: stop(); start()
    return did, S['keys'] > k0
R = []
def ok(n, c, d=''):
    R.append(bool(c)); print(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:600] if not c and d != '' else ''), flush=True)
try:
    # without the guard: the harness sees the damage (Alt+F4 closes, F5 reloads, Ctrl+H hides the page)
    S['guard'] = False; start()
    base = {c: press(c)[0] for c in ['alt+F4', 'F5', 'ctrl+h']}
    ok('without the guard: Alt+F4 closes the desktop (as it did on the Vivobook), F5 reloads it, Ctrl+H hides it', 'closed' in base['alt+F4'] and 'reloaded' in base['F5'] and 'hidden' in base['ctrl+h'], base)
    stop(); S['guard'] = True; start()
    combos = ['alt+F4', 'ctrl+w', 'ctrl+shift+w', 'ctrl+F4', 'F5', 'ctrl+r', 'ctrl+shift+r', 'shift+F5', 'ctrl+F5', 'ctrl+t', 'ctrl+n', 'ctrl+shift+n',
              'ctrl+h', 'ctrl+j', 'ctrl+shift+o', 'ctrl+shift+Delete', 'F1', 'alt+Home', 'ctrl+s', 'ctrl+o', 'shift+Escape', 'F12', 'ctrl+shift+i',
              'ctrl+shift+j', 'ctrl+p', 'ctrl+f', 'ctrl+d', 'ctrl+shift+a', 'ctrl+shift+m', 'F7']
    bad, blind = {}, []
    for c in combos:
        did, saw = press(c)
        if did: bad[c] = did
        if not saw: blind.append(c)
    ok('with the guard: none of %d shortcuts closes, reloads, covers or leaves the desktop' % len(combos), not bad, bad)
    ok('...and the desktop still sees each key (its own shortcuts keep working)', not blind, blind)
finally:
    stop(); xv.kill(); srv.shutdown()
print('%d/%d passed' % (sum(R), len(R))); sys.exit(0 if all(R) else 1)
