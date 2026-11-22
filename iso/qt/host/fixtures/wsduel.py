# 1.6.2 (docs/plan-1.6.2.md Part 30): a game's side of the LAN game relay, for duel162_t.sh: a minimal WebSocket client
# (masked text frames, as a browser sends) that hosts or joins, writes every message it gets to OUT (one a line), and,
# once paired, sends SEND (a JSON message) every half second; it leaves when OUT.stop appears, or the relay closes.
#   python3 wsduel.py URL host|join CODE VERSION OUT [SEND]
import base64, json, os, socket, struct, sys, time
url, mode, code, ver, out = sys.argv[1:6]
send = sys.argv[6] if len(sys.argv) > 6 else None
host, port = url[5:].split('/')[0].split(':')
s = socket.create_connection((host, int(port)), timeout=5)
key = base64.b64encode(os.urandom(16)).decode()
s.sendall(('GET /duel HTTP/1.1\r\nHost: %s:%s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n' % (host, port, key)).encode())
buf = b''
while b'\r\n\r\n' not in buf:
    d = s.recv(4096)
    if not d: sys.exit(1)
    buf += d
head, buf = buf.split(b'\r\n\r\n', 1)
f = open(out, 'w')
f.write('handshake ' + head.split(b'\r\n')[0].decode() + '\n'); f.flush()
def frame(t):
    p = t.encode(); m = os.urandom(4); n = len(p)
    h = bytes([0x81, 0x80 | n]) if n < 126 else bytes([0x81, 0x80 | 126]) + struct.pack('>H', n)
    return h + m + bytes(b ^ m[i & 3] for i, b in enumerate(p))
req = {'t': mode, 'v': ver, 'proto': 1}
if mode == 'join': req['code'] = code
s.sendall(frame(json.dumps(req)))
s.settimeout(0.2); paired = False; last = 0
while not os.path.exists(out + '.stop'):
    try:
        d = s.recv(65536)
        if not d: break
        buf += d
    except socket.timeout: pass
    while len(buf) >= 2:
        op, n, o = buf[0] & 15, buf[1] & 127, 2
        if n == 126:
            if len(buf) < 4: break
            n, o = struct.unpack('>H', buf[2:4])[0], 4
        if len(buf) < o + n: break
        p, buf = buf[o:o + n], buf[o + n:]
        if op == 1:
            f.write(p.decode() + '\n'); f.flush()
            if json.loads(p.decode()).get('t') == 'paired': paired = True
        elif op == 8:
            f.write('closed\n'); f.flush(); sys.exit(0)
    if paired and send and time.time() - last > 0.5:
        s.sendall(frame(send)); last = time.time()
f.write('left\n'); f.flush()
s.close()
