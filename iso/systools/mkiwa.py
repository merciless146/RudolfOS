#!/usr/bin/env python3
"""Rudolf OS: build and sign an Isolated Web App bundle (.swbn) without npm. GPL-2.0-or-later.

  mkiwa.py key KEYFILE                      make a new Ed25519 key (raw 32-byte seed, base64) and print its bundle ID
  mkiwa.py id KEYFILE                       print the bundle ID of a key
  mkiwa.py build DIR OUT.swbn KEYFILE       bundle every file under DIR and sign it
  mkiwa.py derive RELEASEKEY OUTKEY         the shell's key, derived from the release signing key (raw seed, base64):
                                            HKDF-SHA256, so it is a separate key and the release key cannot be
                                            worked out from it; the same release key always gives the same bundle ID
  mkiwa.py update DIR URL OUT.json          the update list Chromium reads ({"versions":[{"version","src"}]}),
                                            with the version from DIR/.well-known/manifest.webmanifest

The format follows the Web Bundles draft ("b2") and the Isolated Web App integrity block, version 2:
  OUT.swbn = integrity block (CBOR) + web bundle (CBOR)
  web bundle      = [magic, "b2", section lengths, [index, responses], total length]
  integrity block = [magic, "2b", {"webBundleId": ID}, [[{"ed25519PublicKey": key}, signature]]]
  signed data     = len64(SHA-512 of the web bundle) + it + len64(integrity block without signatures) + it
                    + len64(signature attributes) + them
  bundle ID       = base32 (lower case, no padding) of the public key + 00 01 02
"""
import base64, hashlib, mimetypes, os, struct, sys
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization

# ---------------------------------------------------------------- canonical CBOR (the subset needed)
def _head(major, n):
    if n < 24: return bytes([major << 5 | n])
    for ai, fmt in ((24, '>B'), (25, '>H'), (26, '>I'), (27, '>Q')):
        if n < 1 << (8 * struct.calcsize(fmt)): return bytes([major << 5 | ai]) + struct.pack(fmt, n)
    raise ValueError('too big')
def cbor(v):
    if isinstance(v, bool): return b'\xf5' if v else b'\xf4'
    if isinstance(v, int): return _head(0, v)
    if isinstance(v, (bytes, bytearray)): return _head(2, len(v)) + bytes(v)
    if isinstance(v, str): b = v.encode(); return _head(3, len(b)) + b
    if isinstance(v, list): return _head(4, len(v)) + b''.join(cbor(x) for x in v)
    if isinstance(v, dict):
        items = sorted(((cbor(k), cbor(x)) for k, x in v.items()), key=lambda kv: (len(kv[0]), kv[0]))  # deterministic order
        return _head(5, len(items)) + b''.join(k + x for k, x in items)
    raise TypeError(type(v))

WBN_MAGIC = bytes.fromhex('f09f8c90f09f93a6')    # 🌐📦
IB_MAGIC = bytes.fromhex('f09f968bf09f93a6')     # 🖋📦

def web_bundle(files):
    """files: {"/path": (bytes, content-type)} -> web bundle bytes"""
    responses, index, off = [], {}, 0
    # the responses section is a CBOR array; offsets in the index count from the start of that array
    head_len = len(_head(4, len(files)))
    off = head_len
    for url in sorted(files):
        body, ctype = files[url]
        hdr = cbor({b':status': b'200', b'content-type': ctype.encode()})
        enc = cbor([hdr, body])
        index[url] = [off, len(enc)]; off += len(enc); responses.append(enc)
    sec_index = cbor(index)
    sec_resp = _head(4, len(files)) + b''.join(responses)
    lengths = cbor(['index', len(sec_index), 'responses', len(sec_resp)])
    body = cbor(WBN_MAGIC) + cbor(b'b2\x00\x00') + cbor(lengths) + _head(4, 2) + sec_index + sec_resp
    # the last item: the bundle's whole length (8 bytes, big-endian), counting itself (1 + 1 + 8 bytes)
    total = 1 + len(body) + 1 + 8
    return _head(4, 5) + body + b'\x48' + struct.pack('>Q', total)

def load_key(path):
    seed = base64.b64decode(open(path).read().strip())
    return Ed25519PrivateKey.from_private_bytes(seed[:32])
def pubkey(k): return k.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
def bundle_id(pk): return base64.b32encode(pk + b'\x00\x01\x02').decode().lower().rstrip('=')

def sign(bundle, key):
    pk = pubkey(key); bid = bundle_id(pk)
    attrs = {'webBundleId': bid}
    unsigned = cbor([IB_MAGIC, b'2b\x00\x00', attrs, []])
    sig_attrs = cbor({'ed25519PublicKey': pk})
    h = hashlib.sha512(bundle).digest()
    L = lambda b: struct.pack('>Q', len(b)) + b
    sig = key.sign(L(h) + L(unsigned) + L(sig_attrs))
    block = cbor([IB_MAGIC, b'2b\x00\x00', attrs, [[{'ed25519PublicKey': pk}, sig]]])
    return block + bundle, bid

def collect(d):
    files = {}
    for root, _, names in os.walk(d):
        for n in names:
            p = os.path.join(root, n); rel = '/' + os.path.relpath(p, d).replace(os.sep, '/')
            ct = 'application/manifest+json' if n.endswith('.webmanifest') else (mimetypes.guess_type(n)[0] or 'application/octet-stream')
            if ct.startswith('text/') or ct in ('application/javascript', 'application/json', 'application/manifest+json'): ct += '; charset=utf-8'
            files[rel] = (open(p, 'rb').read(), ct)
    if '/index.html' in files: files['/'] = files['/index.html']
    return files

if __name__ == '__main__':
    a = sys.argv[1:]
    if len(a) == 2 and a[0] == 'key':
        k = Ed25519PrivateKey.generate()
        seed = k.private_bytes(serialization.Encoding.Raw, serialization.PrivateFormat.Raw, serialization.NoEncryption())
        fd = os.open(a[1], os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600); os.write(fd, base64.b64encode(seed) + b'\n'); os.close(fd)
        print(bundle_id(pubkey(k)))
    elif len(a) == 2 and a[0] == 'id':
        print(bundle_id(pubkey(load_key(a[1]))))
    elif len(a) == 3 and a[0] == 'derive':
        from cryptography.hazmat.primitives.kdf.hkdf import HKDF
        from cryptography.hazmat.primitives import hashes
        src = base64.b64decode(open(a[1]).read().strip())[:32]
        seed = HKDF(algorithm=hashes.SHA256(), length=32, salt=b'rudolf-os', info=b'isolated web app: rudolf os shell v1').derive(src)
        fd = os.open(a[2], os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600); os.write(fd, base64.b64encode(seed) + b'\n'); os.close(fd)
        print(bundle_id(pubkey(load_key(a[2]))))
    elif len(a) == 4 and a[0] == 'update':
        import json
        v = json.load(open(os.path.join(a[1], '.well-known', 'manifest.webmanifest')))['version']
        open(a[3], 'w').write(json.dumps({'versions': [{'version': v, 'src': a[2]}]}) + '\n'); print(v)
    elif len(a) == 4 and a[0] == 'build':
        out, bid = sign(web_bundle(collect(a[1])), load_key(a[3]))
        open(a[2], 'wb').write(out); print(bid, len(out), 'bytes')
    else:
        sys.exit(__doc__)
