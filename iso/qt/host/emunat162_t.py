#!/usr/bin/env python3
# 1.6.2: Emu's Nintendo 64 and Dreamcast games in their native emulators, for real: Flathub's Rosalie's Mupen GUI
# (com.github.Rosalie241.RMG) and Flycast (org.flycast.Flycast), started by the real rudolfd (built here, in private
# mount and network namespaces, as peer59_t.sh does) through native/run with a game from the library, on Xvfb, with a
# virtual gamepad (uinput). The games are Rudolf's own test programs (iso/qt/emu/mkn64.py, mkdc.py): a red screen, green
# while A is held, blue while Start is held. Each byte order (.z64 .n64 .v64) and each disc format (.gdi with its
# tracks, .cdi, .chd when chdman is there) is added with games/import, started, shown full screen in the expected
# colour, answers the gamepad, and stops cleanly (Emu's native/stop, and the emulator's own way out).
#   sudo python3 emunat162_t.py OUT-DIR     (needs: the two apps installed system-wide; Xvfb, python3-evdev, python3-pil,
#   python3-xlib, gcc, curl; a user "rudolf"; /dev/uinput). Screenshots and logs go to OUT-DIR.
# Host tests never stop processes they did not start: everything here is this script's or rudolfd's (started here).
import json, os, re, shutil, signal, subprocess, sys, time, urllib.parse, urllib.request, urllib.error
HERE = os.path.dirname(os.path.abspath(__file__)); TOP = os.path.normpath(os.path.join(HERE, '../../..'))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else 'emunat-out')
if os.getuid() != 0: print('SKIP: needs root'); sys.exit(0)
if not os.environ.get('EMUNAT_INNER'):
    os.makedirs(OUT, exist_ok=True)
    os.execvp('unshare', ['unshare', '-m', '-n', '--propagation', 'private', 'env', 'EMUNAT_INNER=1', sys.executable, os.path.abspath(__file__), OUT])
N = F = 0
def ok(name, cond, detail=''):
    global N, F
    N += 1
    if cond: print('PASS ' + name, flush=True)
    else: F += 1; print('FAIL ' + name + (' - ' + str(detail)[:900] if detail != '' else ''), flush=True)
def sh(c, **k): return subprocess.run(c, shell=True, capture_output=True, text=True, **k)
def wait(f, t, step=0.25):
    end = time.time() + t
    while time.time() < end:
        v = f()
        if v: return v
        time.sleep(step)
    return None
import pwd
RU = pwd.getpwnam('rudolf'); UID, GID = RU.pw_uid, RU.pw_gid
T = '/tmp/emunat.%d' % os.getpid(); os.makedirs(T); os.chmod(T, 0o755)
mine = []   # processes started here
def cleanup():
    for p in reversed(mine):
        try: p.terminate(); p.wait(5)
        except Exception:
            try: p.kill()
            except Exception: pass
import atexit; atexit.register(cleanup)
sh('ip link set lo up')
# (all of /run/user: the runner's logind removes /run/user/UID when a su session ends, which would take a mount there away)
for d in ('/run/rudolf', '/home/rudolf', '/persist', '/media', '/usr/share/rudolf/www', '/usr/lib/rudolf', '/run/user'): os.makedirs(d, exist_ok=True)
for d in ('/run/rudolf', '/home/rudolf', '/persist', '/media', '/run/user'): sh('mount -t tmpfs none ' + d)
os.makedirs('/run/user/%d' % UID)
sh('mount --bind %s/iso/www /usr/share/rudolf/www' % TOP); sh('mount --bind %s/iso/sys/root/usr/lib/rudolf /usr/lib/rudolf' % TOP)
os.chown('/home/rudolf', UID, GID); os.chmod('/home/rudolf', 0o755); os.chown('/run/user/%d' % UID, UID, GID); os.chmod('/run/user/%d' % UID, 0o700)
r = sh('gcc -O2 -w -o %s/rudolfd %s/iso/sys/rudolfd.c -lz' % (T, TOP)); ok('rudolfd builds', r.returncode == 0, r.stderr[-800:])
r = sh('gcc -O2 -w -o %s/rudolf-display %s/iso/sys/rudolf-display.c -lX11 -lXtst -lXfixes -lXrandr -lm' % (T, TOP)); ok('rudolf-display builds', r.returncode == 0, r.stderr[-800:])
# the test games
SRC = '/media/emutest'; os.makedirs(SRC + '/dc')
sh('python3 %s/iso/qt/emu/mkn64.py "%s/Rudolf N64 Test.z64" "%s/Rudolf N64 Test (n64).n64" "%s/Rudolf N64 Test (v64).v64"' % (TOP, SRC, SRC, SRC))
sh('python3 %s/iso/qt/emu/mkdc.py %s/dc' % (TOP, SRC))
CHD = bool(shutil.which('chdman'))
if CHD:
    r = sh('chdman createcd -i "%s/dc/gdi/Rudolf DC Test.gdi" -o "%s/Rudolf DC Test (chd).chd"' % (SRC, SRC)); ok('chdman makes a .chd from the .gdi', r.returncode == 0, r.stdout[-400:] + r.stderr[-400:])
ok('the test games are made', all(os.path.exists(SRC + '/' + f) for f in ('Rudolf N64 Test.z64', 'Rudolf N64 Test (n64).n64', 'Rudolf N64 Test (v64).v64', 'dc/Rudolf DC Test.cdi', 'dc/gdi/Rudolf DC Test.gdi')), sh('ls -lR ' + SRC).stdout)
# the display (Xvfb, ours), the session's bus (ours, the desktop user's), rudolfd
DISP = ':%d' % (90 + os.getpid() % 9)
X = subprocess.Popen(['Xvfb', DISP, '-screen', '0', '1280x720x24', '-nolisten', 'tcp'], stdout=subprocess.DEVNULL, stderr=open(T + '/xvfb.log', 'w')); mine.append(X)
ok('Xvfb started', wait(lambda: os.path.exists('/tmp/.X11-unix/X' + DISP[1:]), 15), open(T + '/xvfb.log').read())
BUS = '/run/user/%d/bus' % UID
def as_rudolf(): os.setgid(GID); os.setuid(UID)
D = subprocess.Popen(['dbus-daemon', '--session', '--address=unix:path=' + BUS, '--nofork', '--nopidfile'], preexec_fn=as_rudolf, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL); mine.append(D)
wait(lambda: os.path.exists(BUS), 10)
# Rudolf's window manager, as on Xorg (--input: windows and input, the X server draws): what shows a game full screen
open('/run/rudolf/wm', 'w').write('desktop\n')
WM = subprocess.Popen([T + '/rudolf-display', '--input'], env=dict(os.environ, DISPLAY=DISP), stdout=open(T + '/display.log', 'w'), stderr=subprocess.STDOUT); mine.append(WM)
ok('rudolf-display (the window manager) runs', wait(lambda: WM.poll() is None and os.path.exists('/run/rudolf/windows') or None, 10) or WM.poll() is None, open(T + '/display.log').read()[-500:])
open('/run/rudolf/session.env', 'w').write('DISPLAY=%s XDG_RUNTIME_DIR=/run/user/%d DBUS_SESSION_BUS_ADDRESS=unix:path=%s XDG_CURRENT_DESKTOP=Rudolf\n' % (DISP, UID, BUS))
RD = subprocess.Popen(['env', 'RUDOLFD_DESKTOP_CG=%s/none' % T, T + '/rudolfd'], stdout=open(T + '/rudolfd.log', 'w'), stderr=subprocess.STDOUT); mine.append(RD)
def page():
    try: return urllib.request.urlopen('http://127.0.0.1:8080/', timeout=5).read().decode('utf8', 'replace')
    except Exception: return ''
pg = wait(page, 20) or ''
import re
m = re.search(r'RUDOLFD=\{token:"([0-9a-f]+)"', pg); TOK = m.group(1) if m else ''
ok('rudolfd serves the page with its token', bool(TOK), open(T + '/rudolfd.log').read()[-500:])
def api(path, body=None, method=None):
    q = urllib.request.Request('http://127.0.0.1:8080/api/' + path, data=body.encode() if isinstance(body, str) else body, method=method or ('POST' if body is not None else 'GET'), headers={'X-Rudolf-Token': TOK})
    try: rr = urllib.request.urlopen(q, timeout=30); return rr.status, json.loads(rr.read() or b'{}')
    except urllib.error.HTTPError as e:
        t = e.read().decode('utf8', 'replace')
        try: return e.code, json.loads(t)
        except Exception: return e.code, {'error': t}
    except Exception as e: return 0, {'error': str(e)}
# the library: games/info makes its folders; games/import adds the made games (the job, games-import.sh)
c, j = api('games/info', ''); ok('games/info makes the library', c == 200 and os.path.isdir('/home/rudolf/Games/N64') and os.path.isdir('/home/rudolf/Games/DC'), (c, j))
lst = [SRC + '/Rudolf N64 Test.z64', SRC + '/Rudolf N64 Test (n64).n64', SRC + '/Rudolf N64 Test (v64).v64', SRC + '/dc/gdi/Rudolf DC Test.gdi', SRC + '/dc/Rudolf DC Test.cdi'] + ([SRC + '/Rudolf DC Test (chd).chd'] if CHD else [])
c, j = api('games/import', '\n'.join(lst) + '\n'); ok('games/import starts', c == 200, (c, j))
jj = wait(lambda: (lambda s: s if not s[1].get('running') and s[1].get('name') == 'games-import' else None)(api('job')), 120) or (0, {})
log = jj[1].get('log', ''); open(OUT + '/games-import.log', 'w').write(log)
G = '/home/rudolf/Games'
WANT = {'n64': [G + '/N64/Rudolf N64 Test.z64', G + '/N64/Rudolf N64 Test (n64).n64', G + '/N64/Rudolf N64 Test (v64).v64'],
        'dc': [G + '/DC/Rudolf DC Test/Rudolf DC Test.gdi', G + '/DC/Rudolf DC Test.cdi'] + ([G + '/DC/Rudolf DC Test (chd).chd'] if CHD else [])}
ok('the import adds every test game where the library looks (N64/, DC/, a .gdi in DC/<name>/ with its tracks)',
   all(os.path.isfile(p) for v in WANT.values() for p in v) and all(os.path.isfile(G + '/DC/Rudolf DC Test/' + t) for t in ('track01.iso', 'track02.raw', 'track03.iso')) and ('done %d added, 0 skipped' % sum(map(len, WANT.values()))) in log, log)
c, j = api('fs/list?path=' + urllib.parse.quote(G + '/N64')); ok('...and fs/list (the library\'s scan) lists the N64 games', c == 200 and sum(1 for i in j.get('items', []) if i['name'].startswith('Rudolf N64 Test')) == 3, (c, j))
# the gamepad: a virtual Xbox 360-style pad (the layout SDL knows), readable by the desktop user
import evdev
from evdev import UInput, ecodes as E, AbsInfo
cap = {E.EV_KEY: [E.BTN_A, E.BTN_B, E.BTN_X, E.BTN_Y, E.BTN_TL, E.BTN_TR, E.BTN_SELECT, E.BTN_START, E.BTN_MODE, E.BTN_THUMBL, E.BTN_THUMBR],
       E.EV_ABS: [(E.ABS_X, AbsInfo(0, -32768, 32767, 16, 128, 0)), (E.ABS_Y, AbsInfo(0, -32768, 32767, 16, 128, 0)), (E.ABS_Z, AbsInfo(0, 0, 255, 0, 0, 0)),
                  (E.ABS_RX, AbsInfo(0, -32768, 32767, 16, 128, 0)), (E.ABS_RY, AbsInfo(0, -32768, 32767, 16, 128, 0)), (E.ABS_RZ, AbsInfo(0, 0, 255, 0, 0, 0)),
                  (E.ABS_HAT0X, AbsInfo(0, -1, 1, 0, 0, 0)), (E.ABS_HAT0Y, AbsInfo(0, -1, 1, 0, 0, 0))]}
PAD = UInput(cap, name='Rudolf test pad', vendor=0x045e, product=0x028e, version=0x114, bustype=E.BUS_USB)
time.sleep(1)
for n in os.listdir('/dev/input'):
    if n.startswith('event') or n.startswith('js'):
        try:
            if 'Rudolf test pad' in open('/sys/class/input/%s/device/name' % n).read(): os.chmod('/dev/input/' + n, 0o666)
        except Exception: pass
ok('the virtual gamepad is there', PAD.device is not None and os.access(PAD.device.path, os.R_OK), getattr(PAD, 'devnode', ''))
def press(btn, down):
    PAD.write(E.EV_KEY, btn, 1 if down else 0); PAD.syn()
# the screen
from PIL import ImageGrab
from Xlib import display as xdisplay, X as XX, Xatom
from Xlib.protocol import event as xevent
XD = xdisplay.Display(DISP)
def shot(name):
    im = ImageGrab.grab(xdisplay=DISP); im.save(OUT + '/' + name + '.png'); return im
def colour(im):
    w, h = im.size; px = [im.getpixel((int(w * x), int(h * y)))[:3] for x, y in ((.5, .5), (.3, .3), (.7, .7), (.3, .7), (.7, .3))]
    def k(p):
        r, g, b = p
        if r > 150 and g < 90 and b < 90: return 'red'
        if g > 150 and r < 90 and b < 90: return 'green'
        if b > 150 and r < 90 and g < 90: return 'blue'
        if r < 30 and g < 30 and b < 30: return 'black'
        return 'other%s' % (p,)
    ks = [k(p) for p in px]; return ks[0] if len(set(ks)) == 1 else 'mixed:' + ','.join(ks)
def windows():
    out = []
    def walk(w, depth):
        try:
            for c in w.query_tree().children:
                a = c.get_attributes()
                if a.map_state == XX.IsViewable and not a.override_redirect:
                    g = c.get_geometry(); cl = c.get_wm_class() or ('', '')
                    out.append((c, g.width, g.height, cl))
        except Exception: pass
    walk(XD.screen().root, 0); return out
def app_window(cls):
    for w, ww, hh, cl in windows():
        if any(cls in (x or '').lower() for x in cl): return (w, ww, hh, cl)
    return None
def is_fullscreen(w):
    try:
        st = w.get_full_property(XD.intern_atom('_NET_WM_STATE'), Xatom.ATOM)
        return st is not None and XD.intern_atom('_NET_WM_STATE_FULLSCREEN') in st.value
    except Exception: return False
def close_window(w):
    ev = xevent.ClientMessage(window=w, client_type=XD.intern_atom('WM_PROTOCOLS'), data=(32, [XD.intern_atom('WM_DELETE_WINDOW'), XX.CurrentTime, 0, 0, 0]))
    w.send_event(ev); XD.flush()
def theirs():   # the desktop user's processes other than the session's bus started here and the services it started
    o = [l.split(None, 2) for l in sh('ps -o pid=,ppid=,args= -u %d' % UID).stdout.strip().splitlines() if l.strip()]
    return [' '.join(x) for x in o if int(x[0]) != D.pid and int(x[1]) != D.pid]
def npid():
    try: return open('/run/rudolf/native.pid').read().strip()
    except Exception: return ''
def stopped(t=20): return wait(lambda: (not theirs() and not (npid() and os.path.exists('/proc/' + npid()))) or None, t)
def native_log():
    try: return open('/run/rudolf/native.log').read()[-3000:]
    except Exception: return ''
APPS = {'n64': ('com.github.Rosalie241.RMG', 'rmg'), 'dc': ('org.flycast.Flycast', 'flycast')}
for sysk in ('n64', 'dc'):
    app, cls = APPS[sysk]
    c, j = api('native/status?id=' + app); ok('%s: native/status says %s is installed' % (sysk, app), c == 200 and j.get('installed'), (c, j))
    for k, game in enumerate(WANT[sysk]):
        tag = '%s %s' % (sysk, os.path.basename(game)); fn = re.sub(r'[^A-Za-z0-9]+', '_', os.path.basename(game))
        open('/run/rudolf/native.log', 'w').close()
        c, j = api('native/run?id=%s&file=%s' % (app, urllib.parse.quote(game)), ''); ok('%s: native/run starts it' % tag, c == 200 and j.get('ok') is not False, (c, j))
        aw = wait(lambda: app_window(cls), 90)
        ok('%s: its window appears' % tag, aw, [(x[3], x[1], x[2]) for x in windows()])
        if not aw: shot(fn + '-nowindow'); print(native_log()); print(open(T + '/rudolfd.log').read()[-1500:]); api('native/stop?id=' + app, ''); stopped(); continue
        col = wait(lambda: (lambda cc: cc if cc == 'red' else None)(colour(shot(fn + '-1start'))), 60)
        ok('%s: the game shows its red screen' % tag, col == 'red', colour(shot(fn + '-1start')) + ' ' + native_log()[-600:])
        aw = app_window(cls) or aw
        ok('%s: full screen (the window covers the screen%s)' % (tag, ', or says it is full screen'), (aw[1], aw[2]) == (1280, 720) or is_fullscreen(aw[0]), (aw[1], aw[2], aw[3]))
        press(E.BTN_A, True); g = wait(lambda: (lambda cc: cc if cc == 'green' else None)(colour(shot(fn + '-2a'))), 8)
        ok('%s: holding the gamepad\'s A turns it green (the input reaches the game)' % tag, g == 'green', colour(shot(fn + '-2a')))
        press(E.BTN_A, False); r2 = wait(lambda: (lambda cc: cc if cc == 'red' else None)(colour(shot(fn + '-3rel'))), 8)
        ok('%s: ...and letting go, red again' % tag, r2 == 'red', colour(shot(fn + '-3rel')))
        press(E.BTN_START, True); b = wait(lambda: (lambda cc: cc if cc == 'blue' else None)(colour(shot(fn + '-4start'))), 8); press(E.BTN_START, False)
        ok('%s: holding Start turns it blue' % tag, b == 'blue', colour(shot(fn + '-4start')))
        if k == 0:   # the keyboard too: Flycast's own keys (X is A, Return is Start); for RMG the keys Rudolf sets up
            ka = {'n64': 'x', 'dc': 'x'}[sysk]
            sh('DISPLAY=%s xdotool keydown %s' % (DISP, ka)); g = wait(lambda: (lambda cc: cc if cc == 'green' else None)(colour(shot(fn + '-2key'))), 8); sh('DISPLAY=%s xdotool keyup %s' % (DISP, ka))
            ok('%s: the keyboard\'s %s (A) turns it green' % (tag, ka.upper()), g == 'green', colour(shot(fn + '-2key')))
        if sysk == 'n64' and k == 0:
            time.sleep(2)
            eep = sh('find /home/rudolf/.var/app/%s -iname "*.eep*" 2>/dev/null' % app).stdout.split()
            data = open(eep[0], 'rb').read(8) if eep else b''
            ok('%s: the EEPROM save (A pressed: RUDOLF64 in block 0) is in the emulator\'s own folder' % tag, data == b'RUDOLF64', (eep, data, sh('find /home/rudolf/.var/app/%s -newer /run/rudolf/session.env -type f | head -40' % app).stdout))
        # how it ends: the first game of each through the emulator's own way out (its window closed, as Rudolf's close
        # button does); the others through Emu's native/stop
        if k == 0:
            close_window(aw[0]); s = stopped(20)
            ok('%s: closing its window (Rudolf\'s close button) ends the emulator, with nothing left running' % tag, s, theirs())
            if not s: shot(fn + '-5afterclose'); api('native/stop?id=' + app, ''); stopped()
        elif k == 1 and sysk == 'n64':
            sh('DISPLAY=%s xdotool key Escape' % DISP); s = stopped(20)
            ok('%s: Esc (what Emu tells the player ends the game) ends the emulator, with nothing left running' % tag, s, (theirs(), [(x[3], x[1], x[2]) for x in windows()]))
            if not s: shot(fn + '-5afteresc'); api('native/stop?id=' + app, ''); stopped()
        else:
            c, j = api('native/stop?id=' + app, ''); s = stopped(20)
            ok('%s: native/stop ends it, with nothing left running' % tag, c == 200 and s, (c, j, theirs()))
        open(OUT + '/' + fn + '-native.log', 'w').write(native_log()); print('--- native.log (%s), last lines:\n%s' % (tag, native_log()[-1200:]))
        c, j = api('native/status?id=' + app); ok('%s: ...and native/status says it is not running' % tag, c == 200 and not j.get('running'), j)
PAD.close()
for f in ('/home/rudolf/.var/app/com.github.Rosalie241.RMG/config/RMG/mupen64plus.cfg', '/home/rudolf/.var/app/org.flycast.Flycast/config/flycast/emu.cfg'):
    try: t = open(f).read(); print('--- %s: its sections\n%s' % (f, '\n'.join(l for l in t.splitlines() if l.startswith('['))))
    except Exception as e: print('--- %s: %s' % (f, e))
print('--- rudolf-display:\n' + open(T + '/display.log').read()[-1500:])
shutil.copy(T + '/rudolfd.log', OUT + '/rudolfd.log')
sh('mkdir -p %s/config && cp -r /home/rudolf/.var/app/*/config %s/config/ 2>/dev/null; find /home/rudolf/.var -type f | grep -v -e mesa_shader -e /cache/ > %s/files.txt; chmod -R a+rX %s' % (OUT, OUT, OUT, OUT))
print('%d/%d passed' % (N - F, N)); sys.exit(1 if F else 0)
