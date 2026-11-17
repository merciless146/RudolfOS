# 1.6.0 (docs/plan-1.6.0.md Part 14): gcr's secret exchange "[sx-aes-1]" written again here from gcr-secret-exchange.c
# (Diffie-Hellman in IKE's 1536-bit group, HKDF-SHA256 to a 16-byte key, AES-128-CBC with PKCS#7 padding), to check
# rudolf-portal's (its --sx-test) against another implementation. Prints PASS/FAIL lines.   python3 sxpeer.py RUDOLF-PORTAL
# 2026-11-17: the shared value's length. The two ends rudolf-portal meets disagree on it: gnome-keyring-daemon, its real
# peer (its own GcrSecretExchange, daemon/dbus/gkd-secret-exchange.c, through its PKCS#11 module's DH derive), keeps the
# value at the prime's 192 bytes, leading zero bytes and all (gnome-keyring 46's egg/egg-dh.c egg_dh_gen_secret: "Pad the
# secret with zero bytes to match length of prime in bytes"), and libgcr's own (gcr 3.41, gcr-prompter's) strips them.
# One shared value in 256 starts with a zero byte: there the keys differ, and the daemon cannot read the password it was
# sent ("couldn't create new collection: Cannot import an invalid key", the app told "dismissed": keyprompt60_t.sh's
# occasional failure, about 1 run in 25). 12 sends one such value, as the daemon does it, every time.
import base64, os, subprocess, sys
from cryptography.hazmat.primitives import hashes, padding
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives.kdf.hkdf import HKDF
P = int("FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7EDEE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3DC2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F83655D23DCA3AD961C62F356208552BB9ED529077096966D670C354E4ABC9804F1746C08CA237327FFFFFFFFFFFFFFFF", 16)
N = F = 0
def ok(name, c, why=""):
    global N, F
    N += 1
    if c: print("PASS " + name)
    else: F += 1; print("FAIL " + name + " - " + str(why)[:400])
def b(n): return n.to_bytes((n.bit_length() + 7) // 8 or 1, "big")
def parse(t):
    lines = t.strip().split("|")
    return lines[0], dict(l.split("=", 1) for l in lines[1:] if "=" in l)
def keyfile(**kw): return "|".join(["[sx-aes-1]"] + ["%s=%s" % (k, base64.b64encode(v).decode()) for k, v in kw.items()])
def kdf(shared, pad=False): return HKDF(algorithm=hashes.SHA256(), length=16, salt=b"\0" * 32, info=b"").derive(shared.to_bytes(192, "big") if pad else b(shared))
def enc(key, data, iv=None, pad=True):
    iv = iv or os.urandom(16)
    if pad: p = padding.PKCS7(128).padder(); data = p.update(data) + p.finalize()
    e = Cipher(algorithms.AES(key), modes.CBC(iv)).encryptor(); return iv, e.update(data) + e.finalize()
def dec(key, iv, ct):
    d = Cipher(algorithms.AES(key), modes.CBC(iv)).decryptor(); u = padding.PKCS7(128).unpadder()
    return u.update(d.update(ct) + d.finalize()) + u.finalize()
def portal():
    p = subprocess.Popen([sys.argv[1], "--sx-test"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
    return p, p.stdout.readline().strip()
def ask(p, line): p.stdin.write(line + "\n"); p.stdin.flush(); return p.stdout.readline().strip()

p, first = portal(); head, kv = parse(first)
their = int.from_bytes(base64.b64decode(kv.get("public", "")), "big")
ok("1. the portal begins as gcr does: [sx-aes-1] with its public key only, 1 < key < p", head == "[sx-aes-1]" and list(kv) == ["public"] and 1 < their < P - 1, first)
priv = int.from_bytes(os.urandom(192), "big") % (P - 2) + 1; pub = pow(2, priv, P); key = kdf(pow(their, priv, P))
ok("2. our public key received (the transport key derived): no secret in it", ask(p, "recv " + keyfile(public=b(pub))) == "nosecret")
sec = "pässwörd ✓ 🔑 with spaces".encode()
out = ask(p, "send " + base64.b64encode(sec).decode()); h2, kv2 = parse(out)
got = None
try: got = dec(key, base64.b64decode(kv2["iv"]), base64.b64decode(kv2["secret"]))
except Exception as e: got = e
ok("3. a secret sent by the portal is read here (the same key, AES-128-CBC, PKCS#7): public, secret, iv in gcr's order", got == sec and list(kv2) == ["public", "secret", "iv"] and len(base64.b64decode(kv2["iv"])) == 16, (out, got))
iv, ct = enc(key, b"from the daemon")
ok("4. a secret sent from here is read by the portal", ask(p, "recv " + keyfile(public=b(pub), secret=ct, iv=iv)) == "secret " + base64.b64encode(b"from the daemon").decode())
out = ask(p, "send -"); ok("5. a reply without a secret (a confirmation): its public key only", list(parse(out)[1]) == ["public"], out)
out = ask(p, "send "); h6, kv6 = parse(out)
ok("6. an empty password is sent as one block of padding", len(base64.b64decode(kv6.get("secret", ""))) == 16 and dec(key, base64.b64decode(kv6["iv"]), base64.b64decode(kv6["secret"])) == b"", out)
iv, ct = enc(key, b"x" * 16, pad=False)
ok("7. bad padding refused", ask(p, "recv " + keyfile(public=b(pub), secret=ct, iv=iv)) == "invalid")
iv, ct = enc(key, b"short iv")
ok("8. an IV that is not 16 bytes refused", ask(p, "recv " + keyfile(public=b(pub), secret=ct, iv=iv[:8])) == "invalid")
ok("9. a message that is not a key file refused", ask(p, "recv not a key file") == "invalid")
p.stdin.close(); p.wait()
for n, v in (("1", 1), ("p-1", P - 1), ("p", P)):
    q, _ = portal(); r = ask(q, "recv " + keyfile(public=b(v))); q.stdin.close(); q.wait()
    ok("10. a weak public key (%s) refused" % n, r == "invalid", r)
# 12. gnome-keyring-daemon's rule (the shared value kept at 192 bytes) with a value whose first byte is zero: our key
# chosen until it is (one in 256 is), then a secret each way read with the daemon's key
p, first = portal(); their = int.from_bytes(base64.b64decode(parse(first)[1].get("public", "")), "big")
while True:
    priv = int.from_bytes(os.urandom(192), "big") % (P - 2) + 1; shared = pow(their, priv, P)
    if shared < 1 << 1528: break
dkey = kdf(shared, pad=True); r12 = ask(p, "recv " + keyfile(public=b(pow(2, priv, P))))
out = ask(p, "send " + base64.b64encode(b"daemon-pw").decode()); kv12 = parse(out)[1]
try: got = dec(dkey, base64.b64decode(kv12["iv"]), base64.b64decode(kv12["secret"]))
except Exception as e: got = "not readable with the daemon's key (%s)" % type(e).__name__
iv, ct = enc(dkey, b"from the daemon"); r12b = ask(p, "recv " + keyfile(public=b(pow(2, priv, P)), secret=ct, iv=iv)); p.stdin.close(); p.wait()
ok("12. a shared value starting with a zero byte, kept at the prime's 192 bytes as gnome-keyring-daemon keeps it: a secret each way read", r12 == "nosecret" and got == b"daemon-pw" and r12b == "secret " + base64.b64encode(b"from the daemon").decode(), (r12, got, r12b))
# and against libgcr's own GcrSecretExchange (gcr-prompter's side, not the daemon's: see 12), where it is installed. libgcr
# strips the shared value's leading zeros, so one exchange in 256 may differ from the daemon's rule: three fresh exchanges,
# one of them agreeing is enough (an implementation that does not agree fails all three)
try:
    import ctypes; g = ctypes.CDLL("libgcr-base-3.so.1")
except OSError: g = None
if g:
    for f in ("gcr_secret_exchange_new", "gcr_secret_exchange_send"): getattr(g, f).restype = ctypes.c_void_p
    g.gcr_secret_exchange_new.argtypes = [ctypes.c_char_p]; g.gcr_secret_exchange_receive.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    g.gcr_secret_exchange_send.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_ssize_t]
    g.gcr_secret_exchange_get_secret.restype = ctypes.c_char_p; g.gcr_secret_exchange_get_secret.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    def gcr_once():
        x = g.gcr_secret_exchange_new(None); p, first = portal()
        r1 = g.gcr_secret_exchange_receive(x, first.replace("|", "\n").encode())
        r2 = ask(p, "recv " + ctypes.string_at(g.gcr_secret_exchange_send(x, None, 0)).decode().replace("\n", "|"))
        out = ask(p, "send " + base64.b64encode(sec).decode()).replace("|", "\n")
        r3 = g.gcr_secret_exchange_receive(x, out.encode()); got = g.gcr_secret_exchange_get_secret(x, None) if r3 else None
        s4 = ctypes.string_at(g.gcr_secret_exchange_send(x, b"from gcr", -1)).decode().replace("\n", "|"); r4 = ask(p, "recv " + s4)
        p.stdin.close(); p.wait()
        return (r1 and r2 == "nosecret" and r3 and got == sec and r4 == "secret " + base64.b64encode(b"from gcr").decode()), (r1, r2, r3, got, r4)
    tries = [gcr_once() for _ in range(3)]
    ok("11. with libgcr's own GcrSecretExchange: its key taken, a secret read by gcr, one from gcr read here (%d of 3 exchanges)" % sum(t[0] for t in tries), any(t[0] for t in tries), [t[1] for t in tries])
else: print("SKIP 11: no libgcr-base-3")
print("%d/%d passed" % (N - F, N)); sys.exit(1 if F else 0)
