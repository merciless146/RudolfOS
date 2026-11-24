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
# 1.6.3 Part 0: while the release key is replaced, TRUSTED-KEYS holds the old key and the new one, and only the first
# one signs: computers that have not yet updated trust only the old key, so a release signed by the new one would be
# refused by them. The new key is moved to the first line (by a pull request) only once a release that trusts both
# has been the stable release for a while; RPK_SIGNING_KEY is switched to it at the same time.
if pub != keys[0]:
    print('::error title=Signing key::RPK_SIGNING_KEY belongs to public key %s, which %s trusts but which does not sign yet: '
          'the signing key is the first key there (%s). Computers that have not updated trust only that one; switch '
          'keys only as docs/plan-1.6.3.md Part 0 says' % (pub, trusted, keys[0])); sys.exit(1)
print('signing key OK, public key', pub)
