#!/usr/bin/env python3
# 1.6.1 (docs/plan-1.6.1.md Part 21; docs/plan-1.6.0.md Part 20's planned test): a virtual PIV card for the smart card tests.
# The card is NIST SP 800-73's PIV application, just enough of it for OpenSC's PIV driver: SELECT of the PIV AID (its
# application property template), GET DATA (the discovery object, the CHUID, the PIV Authentication certificate in 9A),
# VERIFY of the PIV PIN (3 tries, then blocked), GENERAL AUTHENTICATE with 9A's key (ECC P-256: what TLS client
# authentication asks), GET RESPONSE and command chaining. Everything else answers as a card that does not know it.
# The keys are made by the test, in a folder of its own (never committed): `pki` below.
# Two ways in:
#   vpcd     vsmartcard's virtual reader (vpcd, a pcscd driver: vsmartcard-vpcd): the card connects to vpcd's port, as
#            vsmartcard's vicc does (its own card types have no PIV, so this is the card vicc would be)
#   vscard   QEMU's USB CCID reader (-device usb-ccid -device ccid-card-passthru,chardev=…): QEMU's VSCard protocol on a
#            socket, so the guest sees a real USB smart card reader with this card in it (its libccid, pcscd, OpenSC)
#   python3 vpiv.py pki DIR                  the test CA, the card's key and certificate, a TLS server's (DIR, mode 700)
#   python3 vpiv.py vpcd DIR [HOST [PORT]]   the card in vpcd's reader until stopped
#   python3 vpiv.py vscard DIR SOCKET        the card in QEMU's reader until stopped
import sys, os, socket, struct, threading, time, datetime, ipaddress, json
PIN = b'123456'; PUK = b'12345678'
PIV_AID = bytes.fromhex('A000000308000010000100')

def tlv(tag, val):
    t = bytes([tag]) if isinstance(tag, int) and tag < 256 else (tag if isinstance(tag, bytes) else tag.to_bytes(2, 'big'))
    n = len(val)
    l = bytes([n]) if n < 0x80 else bytes([0x81, n]) if n < 0x100 else bytes([0x82]) + n.to_bytes(2, 'big')
    return t + l + val

def tlv_parse(b):
    """{tag: value} of one level of BER-TLV (one- or two-byte tags)"""
    o, r = 0, {}
    while o < len(b):
        t = b[o]; o += 1
        if t & 0x1f == 0x1f: t = (t << 8) | b[o]; o += 1
        n = b[o]; o += 1
        if n & 0x80:
            k = n & 0x7f; n = int.from_bytes(b[o:o + k], 'big'); o += k
        r[t] = b[o:o + n]; o += n
    return r

def pki(d, days=20, server_ips=('10.0.2.100', '127.0.0.1')):
    """the test CA (EC P-384), the card's PIV Authentication key and certificate (EC P-256, clientAuth, `days` left) and a
    TLS server's certificate for server_ips, all under d. Returns the names"""
    from cryptography import x509
    from cryptography.x509.oid import NameOID, ExtendedKeyUsageOID
    from cryptography.hazmat.primitives import hashes, serialization as S
    from cryptography.hazmat.primitives.asymmetric import ec
    os.makedirs(d, exist_ok=True); os.chmod(d, 0o700)
    now = datetime.datetime.now(datetime.timezone.utc)
    nm = lambda cn: x509.Name([x509.NameAttribute(NameOID.COUNTRY_NAME, 'US'), x509.NameAttribute(NameOID.ORGANIZATION_NAME, 'Rudolf OS test'), x509.NameAttribute(NameOID.COMMON_NAME, cn)])
    def mk(cn, key, icn, ikey, ca, days, ext=()):
        b = (x509.CertificateBuilder().subject_name(nm(cn)).issuer_name(nm(icn)).public_key(key.public_key()).serial_number(x509.random_serial_number())
             .not_valid_before(now - datetime.timedelta(days=1)).not_valid_after(now + datetime.timedelta(days=days))
             .add_extension(x509.BasicConstraints(ca=ca, path_length=None), critical=True))
        for e, c in ext: b = b.add_extension(e, critical=c)
        return b.sign(ikey, hashes.SHA384())
    cak = ec.generate_private_key(ec.SECP384R1()); ca = mk('Rudolf Test Smart Card CA', cak, 'Rudolf Test Smart Card CA', cak, True, 365,
        [(x509.KeyUsage(False, False, False, False, False, True, True, False, False), True)])
    ck = ec.generate_private_key(ec.SECP256R1()); card = mk('TEST.CARDHOLDER.1234567890', ck, 'Rudolf Test Smart Card CA', cak, False, days,
        [(x509.KeyUsage(True, False, False, False, False, False, False, False, False), True), (x509.ExtendedKeyUsage([ExtendedKeyUsageOID.CLIENT_AUTH]), False)])
    sk = ec.generate_private_key(ec.SECP256R1()); srv = mk('Rudolf test site', sk, 'Rudolf Test Smart Card CA', cak, False, 30,
        [(x509.SubjectAlternativeName([x509.IPAddress(ipaddress.ip_address(i)) for i in server_ips] + [x509.DNSName('localhost')]), False),
         (x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH]), False)])
    pem, der = S.Encoding.PEM, S.Encoding.DER
    w = lambda n, b: (open(os.path.join(d, n), 'wb').write(b), os.chmod(os.path.join(d, n), 0o600))
    w('ca.pem', ca.public_bytes(pem)); w('ca.der', ca.public_bytes(der))
    w('card.key', ck.private_bytes(pem, S.PrivateFormat.PKCS8, S.NoEncryption())); w('card.der', card.public_bytes(der)); w('card.pem', card.public_bytes(pem))
    w('server.key', sk.private_bytes(pem, S.PrivateFormat.PKCS8, S.NoEncryption())); w('server.pem', srv.public_bytes(pem) + ca.public_bytes(pem))
    return {'ca': 'Rudolf Test Smart Card CA', 'card': 'TEST.CARDHOLDER.1234567890', 'expires': card.not_valid_after_utc.strftime('%Y-%m-%d')}

class PIVCard:
    """the card: process(apdu) -> response. log: what it was asked (the PIN never written down)"""
    def __init__(s, d, pin=PIN):
        from cryptography.hazmat.primitives import serialization as S
        s.key = S.load_pem_private_key(open(os.path.join(d, 'card.key'), 'rb').read(), None)
        s.cert = open(os.path.join(d, 'card.der'), 'rb').read()
        s.pin = pin.ljust(8, b'\xff'); s.tries = 3; s.verified = False; s.selected = False
        s.out = b''; s.chain = b''; s.log = []; s.lock = threading.Lock()
        s.counts = {'verify_ok': 0, 'verify_bad': 0, 'sign': 0, 'cert_read': 0, 'select': 0}
        fascn = bytes.fromhex('D4E739DA739CED39CE739D836858210842108421C84210C3EB')   # (SP 800-73's test FASC-N)
        guid = bytes.fromhex('52756446f1f0f1f0f1f0f1f0f1f0f1f0')
        s.objects = {
            b'\x5f\xc1\x02': tlv(0x53, tlv(0x30, fascn) + tlv(0x34, guid) + tlv(0x35, b'20301231') + tlv(0x3e, b'') + tlv(0xfe, b'')),   # CHUID
            b'\x5f\xc1\x05': tlv(0x53, tlv(0x70, s.cert) + tlv(0x71, b'\x00') + tlv(0xfe, b'')),                                        # 9A
            b'\x7e': tlv(0x7e, tlv(0x4f, PIV_AID) + tlv(0x5f2f, b'\x40\x00')),                                                         # discovery
        }
    def atr(s): return bytes.fromhex('3b085275646f6c665056')   # (T=0, no interface bytes, as QEMU's passthru checks an ATR; "RudolfPV": in no list of OpenSC's)
    def reset(s):
        with s.lock: s.verified = False; s.selected = False; s.out = b''; s.chain = b''; s.log.append('reset')
    def _reply(s, data, le):
        lim = le if le else 256
        if len(data) <= lim: s.out = b''; return data + b'\x90\x00'
        s.out = data[lim:]; return data[:lim] + (b'\x61' + bytes([min(len(s.out), 255) if len(s.out) < 256 else 0]))
    def process(s, apdu):
        with s.lock: return s._process(bytes(apdu))
    def _process(s, a):
        if len(a) < 4: return b'\x67\x00'
        cla, ins, p1, p2 = a[:4]; data = b''; le = 0; ext = False
        if len(a) == 5: le = a[4]
        elif len(a) > 5 and a[4] != 0:
            n = a[4]; data = a[5:5 + n]; le = a[5 + n] if len(a) > 5 + n else 0
        elif len(a) >= 7 and a[4] == 0:
            ext = True
            if len(a) == 7: le = int.from_bytes(a[5:7], 'big') or 65536
            else:
                n = int.from_bytes(a[5:7], 'big'); data = a[7:7 + n]; le = int.from_bytes(a[7 + n:9 + n], 'big') if len(a) >= 9 + n else 0
        if ext and le == 0: le = 65536
        if ins == 0xc0:   # GET RESPONSE
            d = s.out; return s._reply(d, le) if d else b'\x6a\x88'
        s.out = b''
        if cla & 0x10:    # command chaining: more to come
            s.chain += data; return b'\x90\x00'
        if s.chain: data = s.chain + data; s.chain = b''
        r = s._cmd(ins, p1, p2, data)
        if isinstance(r, tuple): return s._reply(r[0], le if not ext else le)
        return r
    def _cmd(s, ins, p1, p2, data):
        if ins == 0xa4:
            if p1 == 0x04 and data and PIV_AID.startswith(data) and len(data) >= 5:
                s.selected = True; s.counts['select'] += 1; s.log.append('select PIV')
                apt = tlv(0x61, tlv(0x4f, PIV_AID[5:9] + b'\x01\x00') + tlv(0x79, tlv(0x4f, PIV_AID[:5])) + tlv(0x50, b'Rudolf test PIV')
                          + tlv(0xac, tlv(0x80, b'\x11') + tlv(0x06, b'\x00')))
                return (apt,)
            s.selected = False; return b'\x6a\x82'
        if not s.selected: return b'\x6d\x00' if ins not in (0xcb, 0x20, 0x87) else b'\x69\x85'
        if ins == 0xcb and p1 == 0x3f and p2 == 0xff:
            t = tlv_parse(data).get(0x5c, b'')
            o = s.objects.get(t)
            s.log.append('get data ' + t.hex() + ('' if o else ' (none)'))
            if t == b'\x5f\xc1\x05' and o: s.counts['cert_read'] += 1
            return (o,) if o else b'\x6a\x82'
        if ins == 0x20 and p2 == 0x80:
            if p1 == 0xff: s.verified = False; s.log.append('verify: logged out'); return b'\x90\x00'
            if not data:
                return b'\x90\x00' if s.verified else (b'\x69\x83' if not s.tries else bytes([0x63, 0xc0 | s.tries]))
            if not s.tries: s.log.append('verify: blocked'); return b'\x69\x83'
            if len(data) == 8 and data == s.pin:
                s.tries = 3; s.verified = True; s.counts['verify_ok'] += 1; s.log.append('verify: right PIN'); return b'\x90\x00'
            s.tries -= 1; s.verified = False; s.counts['verify_bad'] += 1; s.log.append(f'verify: wrong PIN ({s.tries} left)')
            return b'\x69\x83' if not s.tries else bytes([0x63, 0xc0 | s.tries])
        if ins == 0x87:
            if p2 != 0x9a or p1 != 0x11: s.log.append(f'general authenticate {p1:02x} {p2:02x}: not this card\'s'); return b'\x6a\x86'
            if not s.verified: s.log.append('general authenticate: no PIN'); return b'\x69\x82'
            t = tlv_parse(tlv_parse(data).get(0x7c, b''))
            ch = t.get(0x81)
            if ch is None or 0x82 not in t: return b'\x6a\x80'
            from cryptography.hazmat.primitives import hashes
            from cryptography.hazmat.primitives.asymmetric import ec, utils
            h = {20: hashes.SHA1(), 28: hashes.SHA224(), 32: hashes.SHA256(), 48: hashes.SHA384(), 64: hashes.SHA512()}.get(len(ch))
            if not h: return b'\x6a\x80'
            sig = s.key.sign(ch, ec.ECDSA(utils.Prehashed(h)))
            s.counts['sign'] += 1; s.log.append(f'general authenticate: signed ({len(ch)} bytes)')
            return (tlv(0x7c, tlv(0x82, sig)),)
        return b'\x6d\x00'

class VPCD:
    """the card in vsmartcard's vpcd reader: connects to vpcd (pcscd's driver) and answers it, until stop()"""
    def __init__(s, card, host='127.0.0.1', port=0x8C7B):
        s.card, s.host, s.port, s.sock, s.run = card, host, port, None, True
    def _rx(s, n):
        b = b''
        while len(b) < n:
            c = s.sock.recv(n - len(b))
            if not c: raise ConnectionError('vpcd closed')
            b += c
        return b
    def serve(s, wait=60):
        end = time.time() + wait
        while s.run:
            try: s.sock = socket.create_connection((s.host, s.port), 2); break
            except OSError:
                if time.time() > end: raise
                time.sleep(0.5)
        s.sock.settimeout(None)
        try:
            while s.run:
                n = struct.unpack('!H', s._rx(2))[0]; m = s._rx(n) if n else b''
                if n == 1:
                    if m[0] == 4: s.sock.sendall(struct.pack('!H', len(s.card.atr())) + s.card.atr())
                    elif m[0] in (0, 2): s.card.reset()
                    continue
                r = s.card.process(m); s.sock.sendall(struct.pack('!H', len(r)) + r)
        except (ConnectionError, OSError): pass
    def start(s, wait=60):
        s.t = threading.Thread(target=s.serve, args=(wait,), daemon=True); s.t.start(); return s
    def stop(s):
        s.run = False
        try: s.sock.shutdown(socket.SHUT_RDWR); s.sock.close()
        except Exception: pass

class VSCard:
    """the card in QEMU's USB CCID reader (ccid-card-passthru's VSCard protocol on a unix socket or host:port):
    insert() and remove() the card, as taking it out of the reader"""
    INIT, ERROR, READER_ADD, READER_REMOVE, ATR, CARD_REMOVE, APDU, FLUSH, FLUSH_DONE = range(1, 10)
    def __init__(s, card, where):
        s.card, s.where, s.sock, s.run, s.reader, s.inserted, s.ev = card, where, None, True, None, False, []
        s.wl = threading.Lock(); s.ready = threading.Event()
    def _send(s, typ, rid, data=b''):
        with s.wl: s.sock.sendall(struct.pack('!III', typ, rid, len(data)) + data)
    def _rx(s, n):
        b = b''
        while len(b) < n:
            c = s.sock.recv(n - len(b))
            if not c: raise ConnectionError('QEMU closed')
            b += c
        return b
    def connect(s, wait=120):
        end = time.time() + wait
        while True:
            try:
                if isinstance(s.where, tuple): s.sock = socket.create_connection(s.where, 2)
                else: s.sock = socket.socket(socket.AF_UNIX); s.sock.connect(s.where)
                break
            except OSError:
                if time.time() > end: raise
                time.sleep(0.5)
        s.sock.settimeout(None)
        s._send(s.INIT, 0xffffffff, b'VSCD' + struct.pack('!I', 2) + struct.pack('!I', 0))
        s.t = threading.Thread(target=s._loop, daemon=True); s.t.start()
        s._send(s.READER_ADD, 0xffffffff, b'Rudolf virtual PIV\x00')
        if not s.ready.wait(30): raise TimeoutError('QEMU did not take the reader: ' + repr(s.ev))
        return s
    def _loop(s):
        try:
            while s.run:
                typ, rid, n = struct.unpack('!III', s._rx(12)); d = s._rx(n) if n else b''
                if typ == s.INIT: s.ev.append('init')
                elif typ == s.ERROR:
                    code = struct.unpack('!I', d[:4])[0] if len(d) >= 4 else -1; s.ev.append(f'error {code} reader {rid}')
                    if code == 0 and s.reader is None: s.reader = rid if rid != 0xffffffff else 0; s.ready.set()
                elif typ == s.APDU:
                    r = s.card.process(d) if s.inserted else b'\x6f\x00'
                    s._send(s.APDU, rid, r)
                elif typ == s.FLUSH: s._send(s.FLUSH_DONE, rid)
                else: s.ev.append(f'message {typ}')
        except (ConnectionError, OSError) as e: s.ev.append('closed: ' + str(e))
    def insert(s):
        s.card.reset(); s.inserted = True; s._send(s.ATR, s.reader or 0, s.card.atr())
    def remove(s):
        s.inserted = False; s._send(s.CARD_REMOVE, s.reader or 0); s.card.reset()
    def close(s):
        s.run = False
        try: s.sock.close()
        except Exception: pass

if __name__ == '__main__':
    a = sys.argv[1:]
    if not a: print(__doc__ or open(__file__).read().split('\nimport')[0]); sys.exit(2)
    if a[0] == 'pki': print(json.dumps(pki(a[1]))); sys.exit(0)
    card = PIVCard(a[1])
    if a[0] == 'vpcd':
        v = VPCD(card, a[2] if len(a) > 2 else '127.0.0.1', int(a[3]) if len(a) > 3 else 0x8C7B); v.serve(600)
    elif a[0] == 'vscard':
        v = VSCard(card, a[2]).connect(); v.insert()
        try:
            while True: time.sleep(1)
        except KeyboardInterrupt: v.close()
