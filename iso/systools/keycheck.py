# Normalise a signing key file for the release tools and check it is one the disc trusts.
#   keycheck.py KEYFILE TRUSTED-KEYS
# KEYFILE may hold the raw 32-byte Ed25519 key in base64 (what the tools read) or a PEM private key
# (openssl genpkey -algorithm ed25519); a PEM key is rewritten in place as raw base64. Prints the public key.
# GPL-2.0-or-later.
import base64, sys
from cryptography.hazmat.primitives import serialization as s
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
path, trusted = sys.argv[1], sys.argv[2]
raw = open(path, 'rb').read().strip()
try:
    if raw.startswith(b'-----BEGIN'):
        k = s.load_pem_private_key(raw, None)
        open(path, 'w').write(base64.b64encode(k.private_bytes(s.Encoding.Raw, s.PrivateFormat.Raw, s.NoEncryption())).decode() + '\n')
    else:
        k = Ed25519PrivateKey.from_private_bytes(base64.b64decode(raw))
except Exception as e:
    print('::error title=Signing key::RPK_SIGNING_KEY is not an Ed25519 key (raw base64 or PEM without a passphrase): %s' % type(e).__name__); sys.exit(1)
pub = base64.b64encode(k.public_key().public_bytes(s.Encoding.Raw, s.PublicFormat.Raw)).decode()
keys = [l.strip() for l in open(trusted) if l.strip() and not l.startswith('#')]
if pub not in keys:
    print('::error title=Signing key::RPK_SIGNING_KEY belongs to public key %s, which is not in %s; computers would refuse what it signs' % (pub, trusted)); sys.exit(1)
print('signing key OK, public key', pub)
