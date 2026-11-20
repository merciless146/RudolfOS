#!/usr/bin/env python3
# 1.6.1 (docs/plan-1.6.1.md Part 18): a software FIDO2 security key for the tests of the security keys that unlock the
# files (rudolfd-fido.inc). It is a CTAP2.0 authenticator behind CTAPHID (the USB HID protocol of FIDO keys, 64-byte
# reports), written for these tests from the FIDO Alliance's CTAP 2.0 specification: authenticatorGetInfo,
# authenticatorMakeCredential and authenticatorGetAssertion with ES256 (P-256) keys and the hmac-secret extension, and
# authenticatorClientPIN with PIN protocol 1 (key agreement, set PIN, PIN token, retries: 8, then blocked). No resident
# credentials: a credential id is the key's private key and its two hmac-secret randoms sealed with the key's own master
# secret (AES-256-GCM), so a credential made by another key is not this key's (CTAP2_ERR_NO_CREDENTIALS).
# The reports go over a stream (a Unix socket or a TCP port, 64 bytes each way, no report ID):
#   - the host tests (iso/qt/host/fido161_t.sh): libfido2 itself, through rudolfd's test-only I/O functions;
#   - QEMU (iso/qt/sb/fido161_t.py): a virtio-serial port to the guest, where iso/qt/vfido_uhid.c makes a real HID device
#     of it with the kernel's uhid, so the guest's libfido2 sees /dev/hidrawN as it would a USB key.
# What the user does is the test's: --touch auto (presence given at once), never (no touch: the request times out
# after --timeout s) or file:PATH (presence given while PATH exists). A control socket (--ctl) takes lines:
#   touch auto|never|file:PATH, replay on|off (the next getAssertion answers with the last one's response, unchanged:
#   a replayed assertion), pin PIN (set the PIN directly), stats (JSON: the counts of requests).
#   python3 vfido.py STATE-DIR --hmac CID-B64 SALT-B64   (the tests' offline check: the key's output, computed from its state)
#   python3 vfido.py STATE-DIR --listen unix:PATH|tcp:HOST:PORT|connect:unix:PATH|connect:tcp:HOST:PORT [--touch MODE] [--pin PIN] [--ctl PATH]
import os, sys, socket, struct, threading, time, json, hashlib, hmac, argparse, select
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

# ---- CBOR (what CTAP2 uses: canonical maps, ints, byte and text strings, arrays, booleans)
def cenc(v):
    def head(mt, n):
        if n < 24: return bytes([mt << 5 | n])
        if n < 256: return bytes([mt << 5 | 24, n])
        if n < 65536: return bytes([mt << 5 | 25]) + struct.pack('>H', n)
        if n < 2 ** 32: return bytes([mt << 5 | 26]) + struct.pack('>I', n)
        return bytes([mt << 5 | 27]) + struct.pack('>Q', n)
    if v is True: return b'\xf5'
    if v is False: return b'\xf4'
    if v is None: return b'\xf6'
    if isinstance(v, int): return head(0, v) if v >= 0 else head(1, -1 - v)
    if isinstance(v, (bytes, bytearray)): return head(2, len(v)) + bytes(v)
    if isinstance(v, str): b = v.encode(); return head(3, len(b)) + b
    if isinstance(v, (list, tuple)): return head(4, len(v)) + b''.join(cenc(x) for x in v)
    if isinstance(v, dict):
        items = sorted(((cenc(k), cenc(x)) for k, x in v.items()), key=lambda kv: (len(kv[0]), kv[0]))
        return head(5, len(items)) + b''.join(k + x for k, x in items)
    raise TypeError(type(v))
def cdec(b, o=0):
    ib = b[o]; mt, ai = ib >> 5, ib & 31; o += 1
    if ai < 24: n = ai
    elif ai == 24: n = b[o]; o += 1
    elif ai == 25: n = struct.unpack('>H', b[o:o + 2])[0]; o += 2
    elif ai == 26: n = struct.unpack('>I', b[o:o + 4])[0]; o += 4
    elif ai == 27: n = struct.unpack('>Q', b[o:o + 8])[0]; o += 8
    else: raise ValueError('indefinite length')
    if mt == 0: return n, o
    if mt == 1: return -1 - n, o
    if mt == 2: return bytes(b[o:o + n]), o + n
    if mt == 3: return bytes(b[o:o + n]).decode(), o + n
    if mt == 4:
        r = []
        for _ in range(n): x, o = cdec(b, o); r.append(x)
        return r, o
    if mt == 5:
        r = {}
        for _ in range(n): k, o = cdec(b, o); x, o = cdec(b, o); r[k] = x
        return r, o
    if mt == 7: return {20: False, 21: True, 22: None}.get(ai), o
    raise ValueError('cbor type %d' % mt)

OK, ERR_INVALID_CBOR, ERR_MISSING, ERR_INVALID_PARAM = 0x00, 0x12, 0x14, 0x02
ERR_EXCLUDED, ERR_UNSUPPORTED_ALG, ERR_NO_CREDS, ERR_TIMEOUT = 0x19, 0x26, 0x2E, 0x2F
ERR_PIN_INVALID, ERR_PIN_BLOCKED, ERR_PIN_AUTH_INVALID, ERR_PIN_NOT_SET, ERR_PIN_REQUIRED, ERR_PIN_POLICY = 0x31, 0x32, 0x33, 0x35, 0x36, 0x37
ERR_UNSUPPORTED_EXT, ERR_INVALID_CMD = 0x16, 0x01
AAGUID = bytes.fromhex('5275646f6c664f53205465737420313631')[:16]   # "RudolfOS Test 161": a test key's own

def cose_pub(pub, alg=-7):
    n = pub.public_numbers()
    return {1: 2, 3: alg, -1: 1, -2: n.x.to_bytes(32, 'big'), -3: n.y.to_bytes(32, 'big')}
def pub_from_cose(k):
    return ec.EllipticCurvePublicNumbers(int.from_bytes(k[-2], 'big'), int.from_bytes(k[-3], 'big'), ec.SECP256R1()).public_key()
def cbc(key, data, enc):
    c = Cipher(algorithms.AES(key), modes.CBC(b'\0' * 16)); x = c.encryptor() if enc else c.decryptor()
    return x.update(data) + x.finalize()

class Key:
    def __init__(self, d, touch='auto', timeout=8.0):
        self.d = d; os.makedirs(d, exist_ok=True); self.touch = touch; self.timeout = timeout; self.replay = False; self.last = None
        st = os.path.join(d, 'state.json')
        if os.path.exists(st): self.s = json.load(open(st))
        else: self.s = {'master': os.urandom(32).hex(), 'count': 0, 'pin': None, 'retries': 8}; self.save()
        self.ka = ec.generate_private_key(ec.SECP256R1())   # PIN protocol 1's key agreement key (new at each power-up)
        self.token = os.urandom(32); self.stats = {}
    def save(self):
        p = os.path.join(self.d, 'state.json'); open(p + '.t', 'w').write(json.dumps(self.s)); os.replace(p + '.t', p)
    def presence(self, keepalive):
        """True when the user touched the key (the test's mode); keepalive() is called while it waits"""
        t0 = time.time()
        while time.time() - t0 < self.timeout:
            m = self.touch
            if m == 'auto' or (m.startswith('file:') and os.path.exists(m[5:])): return True
            keepalive(); time.sleep(0.1)
        return False
    def seal(self, b):
        n = os.urandom(12); return n + AESGCM(bytes.fromhex(self.s['master'])).encrypt(n, b, b'cred')
    def unseal(self, c):
        try: return AESGCM(bytes.fromhex(self.s['master'])).decrypt(c[:12], c[12:], b'cred')
        except Exception: return None
    def shared(self, peer):
        return hashlib.sha256(self.ka.exchange(ec.ECDH(), pub_from_cose(peer))).digest()
    def bump(self):
        self.s['count'] += 1; self.save(); return self.s['count']
    # ---- the CTAP2 commands
    def cmd(self, b, keepalive):
        c = b[0]; self.stats[c] = self.stats.get(c, 0) + 1
        try: p = cdec(b, 1)[0] if len(b) > 1 else {}
        except Exception: return bytes([ERR_INVALID_CBOR])
        if c == 0x04:
            return bytes([OK]) + cenc({1: ['FIDO_2_0'], 2: ['hmac-secret'], 3: AAGUID, 4: {'rk': False, 'up': True, 'plat': False, 'clientPin': self.s['pin'] is not None},
                                       5: 1200, 6: [1]})
        if c == 0x01: return self.make(p, keepalive)
        if c == 0x02:
            if self.replay and self.last: return self.last
            r = self.get(p, keepalive)
            if r[0] == OK: self.last = r
            return r
        if c == 0x06: return self.pin(p)
        if c == 0x07: return bytes([OK])   # reset: not used
        return bytes([ERR_INVALID_CMD])
    def pin_auth_ok(self, p, cdh):
        if p.get(9) != 1 or not isinstance(p.get(8), bytes): return False
        return hmac.compare_digest(hmac.new(self.token, cdh, 'sha256').digest()[:16], p[8])
    def make(self, p, keepalive):
        cdh, rp, user, algs = p.get(1), p.get(2), p.get(3), p.get(4)
        if not isinstance(cdh, bytes) or not isinstance(rp, dict) or not isinstance(user, dict) or not isinstance(algs, list): return bytes([ERR_MISSING])
        if not any(isinstance(a, dict) and a.get('alg') == -7 for a in algs): return bytes([ERR_UNSUPPORTED_ALG])
        rph = hashlib.sha256(rp['id'].encode()).digest()
        if self.s['pin'] is not None:
            if 8 not in p: return bytes([ERR_PIN_REQUIRED])
            if not self.pin_auth_ok(p, cdh): return bytes([ERR_PIN_AUTH_INVALID])
        for x in p.get(5) or []:
            m = self.unseal(x.get('id', b''))
            if m and m[:32] == rph:
                if not self.presence(keepalive): return bytes([ERR_TIMEOUT])
                return bytes([ERR_EXCLUDED])
        if not self.presence(keepalive): return bytes([ERR_TIMEOUT])
        k = ec.generate_private_key(ec.SECP256R1()); raw = k.private_numbers().private_value.to_bytes(32, 'big')
        cid = self.seal(rph + raw + os.urandom(32) + os.urandom(32))   # rpIdHash, the key, CredRandomWithUV, CredRandomWithoutUV
        ext = (p.get(6) or {}).get('hmac-secret') is True
        flags = 0x01 | 0x40 | (0x80 if ext else 0) | (0x04 if 8 in p else 0)
        acd = AAGUID + struct.pack('>H', len(cid)) + cid + cenc(cose_pub(k.public_key()))
        ad = rph + bytes([flags]) + struct.pack('>I', self.bump()) + acd + (cenc({'hmac-secret': True}) if ext else b'')
        return bytes([OK]) + cenc({1: 'none', 2: ad, 3: {}})
    def get(self, p, keepalive):
        rpid, cdh, allow = p.get(1), p.get(2), p.get(3) or []
        if not isinstance(rpid, str) or not isinstance(cdh, bytes): return bytes([ERR_MISSING])
        rph = hashlib.sha256(rpid.encode()).digest(); hit = None
        for x in allow:
            m = self.unseal(x.get('id', b''))
            if m and m[:32] == rph: hit = (x['id'], m); break
        if not hit: return bytes([ERR_NO_CREDS])
        uv = False
        if 6 in p:
            if self.s['pin'] is None: return bytes([ERR_PIN_NOT_SET])
            if not self.pin_auth_ok({8: p[6], 9: p.get(7)}, cdh): return bytes([ERR_PIN_AUTH_INVALID])
            uv = True
        up = (p.get(5) or {}).get('up', True)
        if up and not self.presence(keepalive): return bytes([ERR_TIMEOUT])
        cid, m = hit; k = ec.derive_private_key(int.from_bytes(m[32:64], 'big'), ec.SECP256R1())
        extout = b''; hs = (p.get(4) or {}).get('hmac-secret')
        if hs is not None:
            try:
                sh = self.shared(hs[1]); se, sa = hs[2], hs[3]
                if not hmac.compare_digest(hmac.new(sh, se, 'sha256').digest()[:16], sa): return bytes([ERR_INVALID_PARAM])
                salts = cbc(sh, se, False); cr = m[64:96] if uv else m[96:128]
                out = b''.join(hmac.new(cr, salts[i:i + 32], 'sha256').digest() for i in range(0, len(salts), 32))
                extout = cenc({'hmac-secret': cbc(sh, out, True)})
            except Exception: return bytes([ERR_INVALID_PARAM])
        flags = (0x01 if up else 0) | (0x04 if uv else 0) | (0x80 if extout else 0)
        ad = rph + bytes([flags]) + struct.pack('>I', self.bump()) + extout
        sig = k.sign(ad + cdh, ec.ECDSA(hashes.SHA256()))
        return bytes([OK]) + cenc({1: {'id': cid, 'type': 'public-key'}, 2: ad, 3: sig})
    def pin(self, p):
        sub = p.get(2)
        if p.get(1) != 1 and sub != 1: return bytes([ERR_INVALID_PARAM])
        if sub == 1: return bytes([OK]) + cenc({3: self.s['retries']})
        if sub == 2: return bytes([OK]) + cenc({1: cose_pub(self.ka.public_key(), -25)})
        if sub == 3:   # setPIN
            if self.s['pin'] is not None: return bytes([ERR_PIN_AUTH_INVALID])
            sh = self.shared(p[3]); np_ = p[5]
            if not hmac.compare_digest(hmac.new(sh, np_, 'sha256').digest()[:16], p[4]): return bytes([ERR_PIN_AUTH_INVALID])
            pin = cbc(sh, np_, False).rstrip(b'\0')
            if len(pin) < 4: return bytes([ERR_PIN_POLICY])
            self.s['pin'] = hashlib.sha256(pin).digest()[:16].hex(); self.save(); return bytes([OK])
        if sub == 5:   # getPINToken
            if self.s['pin'] is None: return bytes([ERR_PIN_NOT_SET])
            if self.s['retries'] <= 0: return bytes([ERR_PIN_BLOCKED])
            sh = self.shared(p[3]); ph = cbc(sh, p[6], False)
            if not hmac.compare_digest(ph, bytes.fromhex(self.s['pin'])):
                self.s['retries'] -= 1; self.save(); self.ka = ec.generate_private_key(ec.SECP256R1())
                return bytes([ERR_PIN_BLOCKED if self.s['retries'] <= 0 else ERR_PIN_INVALID])
            self.s['retries'] = 8; self.save(); self.token = os.urandom(32)
            return bytes([OK]) + cenc({2: cbc(sh, self.token, True)})
        return bytes([ERR_INVALID_PARAM])

# ---- CTAPHID over a stream of 64-byte reports
CMD_PING, CMD_MSG, CMD_INIT, CMD_CBOR, CMD_CANCEL, CMD_KEEPALIVE, CMD_WINK, CMD_ERROR = 0x81, 0x83, 0x86, 0x90, 0x91, 0xBB, 0x88, 0xBF
def frames(cid, cmd, data):
    out = [cid + bytes([cmd]) + struct.pack('>H', len(data)) + data[:57]]; data = data[57:]; seq = 0
    while data: out.append(cid + bytes([seq]) + data[:59]); data = data[59:]; seq += 1
    return [f.ljust(64, b'\0') for f in out]
def serve(conn, key, log):
    buf = b''; chans = set(); lock = threading.Lock()
    def send(cid, cmd, data):
        with lock:
            for f in frames(cid, cmd, data): conn.sendall(f)
    def rd():
        nonlocal buf
        while len(buf) < 64:
            x = conn.recv(4096)
            if not x: raise EOFError
            buf += x
        f, buf = buf[:64], buf[64:]; return f
    while True:
        try: f = rd()
        except (EOFError, OSError): return
        cid, cmd = f[:4], f[4]
        if not cmd & 0x80: continue   # a continuation without its start: dropped
        n = struct.unpack('>H', f[5:7])[0]; data = f[7:7 + min(n, 57)]; seq = 0
        while len(data) < n:
            g = rd()
            if g[:4] != cid or g[4] != seq: break
            data += g[5:5 + min(59, n - len(data))]; seq += 1
        if cmd == CMD_INIT:
            new = os.urandom(4); chans.add(new)
            send(cid, CMD_INIT, data[:8] + new + bytes([2, 1, 6, 1, 0x04 | 0x08 | 0x01])); continue   # CBOR, no MSG (NMSG), WINK
        if cmd == CMD_PING: send(cid, CMD_PING, data); continue
        if cmd == CMD_WINK: send(cid, CMD_WINK, b''); continue
        if cmd == CMD_CANCEL: continue
        if cmd == CMD_MSG: send(cid, CMD_ERROR, b'\x01'); continue   # no U2F
        if cmd == CMD_CBOR:
            r = key.cmd(data, lambda: send(cid, CMD_KEEPALIVE, b'\x02'))
            log('cmd 0x%02x -> 0x%02x' % (data[0] if data else 0, r[0]))
            send(cid, CMD_CBOR, r); continue
        send(cid, CMD_ERROR, b'\x01')

def ctl_loop(path, key):
    try: os.unlink(path)
    except OSError: pass
    s = socket.socket(socket.AF_UNIX); s.bind(path); s.listen(4)
    while True:
        c, _ = s.accept(); f = c.makefile('rw')
        for line in f:
            w = line.split(); r = 'ok'
            if not w: continue
            if w[0] == 'touch': key.touch = w[1]
            elif w[0] == 'replay': key.replay = w[1] == 'on'
            elif w[0] == 'pin': key.s['pin'] = hashlib.sha256(w[1].encode()).digest()[:16].hex(); key.s['retries'] = 8; key.save()
            elif w[0] == 'stats': r = json.dumps({'cmds': {('0x%02x' % k): v for k, v in key.stats.items()}, 'count': key.s['count'], 'retries': key.s['retries']})
            else: r = 'unknown'
            f.write(r + '\n'); f.flush()
        c.close()

def offline_hmac(state, cid_b64, salt_b64):
    """(the tests' offline check) what this key's hmac-secret gives for a credential and a salt, without user
    verification, as the key computes it inside"""
    import base64
    k = Key(state); m = k.unseal(base64.b64decode(cid_b64))
    if not m: return None
    return hmac.new(m[96:128], base64.b64decode(salt_b64), 'sha256').digest()

def main():
    if len(sys.argv) == 5 and sys.argv[2] == '--hmac':
        h = offline_hmac(sys.argv[1], sys.argv[3], sys.argv[4]); print(h.hex() if h else 'NONE'); return
    ap = argparse.ArgumentParser(); ap.add_argument('state'); ap.add_argument('--listen', required=True); ap.add_argument('--touch', default='auto')
    ap.add_argument('--timeout', type=float, default=8.0); ap.add_argument('--pin'); ap.add_argument('--ctl'); ap.add_argument('--log')
    a = ap.parse_args(); key = Key(a.state, a.touch, a.timeout)
    if a.pin: key.s['pin'] = hashlib.sha256(a.pin.encode()).digest()[:16].hex(); key.save()
    lf = open(a.log, 'a') if a.log else None
    log = (lambda m: (lf.write('%.2f %s\n' % (time.time(), m)), lf.flush())) if lf else (lambda m: None)
    if a.ctl: threading.Thread(target=ctl_loop, args=(a.ctl, key), daemon=True).start()
    L = a.listen
    if L.startswith('connect:'):   # (QEMU's chardev is the server: reconnect when the guest's side restarts)
        t = L[8:]
        while True:
            try:
                if t.startswith('unix:'): c = socket.socket(socket.AF_UNIX); c.connect(t[5:])
                else: h, pt = t[4:].rsplit(':', 1); c = socket.create_connection((h, int(pt)))
                log('connected'); serve(c, key, log); c.close()
            except OSError: pass
            time.sleep(0.5)
    if L.startswith('unix:'):
        try: os.unlink(L[5:])
        except OSError: pass
        s = socket.socket(socket.AF_UNIX); s.bind(L[5:])
    else:
        h, pt = L[4:].rsplit(':', 1); s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); s.bind((h, int(pt)))
    s.listen(8)
    while True:
        c, _ = s.accept()
        threading.Thread(target=lambda c=c: (serve(c, key, log), c.close()), daemon=True).start()

if __name__ == '__main__': main()
