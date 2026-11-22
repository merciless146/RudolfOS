#!/usr/bin/env python3
# A game's side of the duel protocol for duel161_t.sh (a WebSocket client, no packages; GPL-2.0-or-later).
#   duelws.py probe HOST PORT                  "open" if a TCP connection is made within 2 s, else "blocked"
#   duelws.py host HOST PORT VER OUT STOPFILE  hosts: every message heard into OUT, one a line; answers a game message
#                                              with {"t":"g","k":"st"}; until STOPFILE exists or the relay closes
#   duelws.py join HOST PORT VER CODE OUT      joins; once paired sends {"t":"g","k":"hi"}; a second later leaves
import base64, json, os, socket, sys, time

def conn(h, p):
    s = socket.create_connection((h, int(p)), timeout=3)
    k = base64.b64encode(os.urandom(16)).decode()
    s.sendall(('GET /?want=x HTTP/1.1\r\nHost: %s:%s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n' % (h, p, k)).encode())
    b = b''
    while b'\r\n\r\n' not in b:
        d = s.recv(4096)
        if not d: raise SystemExit('closed during the handshake')
        b += d
    if not b.startswith(b'HTTP/1.1 101'): raise SystemExit('refused: ' + b.split(b'\r\n')[0].decode())
    return s, b.split(b'\r\n\r\n', 1)[1]

def send(s, t):
    d = t.encode(); m = os.urandom(4); h = bytes([0x81])
    h += bytes([0x80 | len(d)]) if len(d) < 126 else bytes([0x80 | 126, len(d) >> 8, len(d) & 255])
    s.sendall(h + m + bytes(c ^ m[i & 3] for i, c in enumerate(d)))

def frames(s, rest, until):
    buf = rest
    while time.time() < until:
        while len(buf) >= 2:
            n = buf[1] & 127; o = 2
            if n == 126:
                if len(buf) < 4: break
                n = buf[2] << 8 | buf[3]; o = 4
            if len(buf) < o + n: break
            op, d = buf[0] & 15, buf[o:o + n]; buf = buf[o + n:]
            if op == 8: yield None; return
            if op == 1: yield d.decode()
        s.settimeout(max(0.05, min(0.3, until - time.time())))
        try: d = s.recv(65536)
        except socket.timeout: yield ''; continue
        except OSError: yield None; return
        if not d: yield None; return
        buf += d

def hello(want, ver, code=None):
    o = {'t': 'hello', 'proto': 1, 'game': 'ember-reach', 'ver': ver, 'want': want}
    if code: o['code'] = code
    return json.dumps(o)

a = sys.argv
if a[1] == 'probe':
    try: socket.create_connection((a[2], int(a[3])), timeout=2).close(); print('open')
    except OSError: print('blocked')
elif a[1] == 'host':
    s, rest = conn(a[2], a[3]); out = open(a[5], 'a', buffering=1); send(s, hello('host', a[4]))
    for m in frames(s, rest, time.time() + 60):
        if m is None: out.write('(closed)\n'); break
        if m: out.write(m + '\n')
        if m.startswith('{"t":"g"') and '"k":"hi"' in m: send(s, '{"t":"g","k":"st"}')
        if os.path.exists(a[6]): break
    s.close()
elif a[1] == 'join':
    s, rest = conn(a[2], a[3]); out = open(a[6], 'a', buffering=1); send(s, hello('join', a[4], a[5])); left = None
    for m in frames(s, rest, time.time() + 8):
        if m is None: out.write('(closed)\n'); break
        if m: out.write(m + '\n')
        if '"paired"' in (m or ''): send(s, '{"t":"g","k":"hi"}'); left = time.time() + 1
        if left and time.time() > left: break
    s.close()
