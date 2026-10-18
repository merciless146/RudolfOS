# Check the Secure Boot signing key (the secret RUDOLF_SB_KEY) against the certificate in the repository, so a
# release is never signed with a key that the enrolled certificate does not match.  Rudolf OS 1.2.2.
#   sbkeycheck.py KEYFILE CERTFILE
# Prints the certificate's SHA-256 fingerprint. GPL-2.0-or-later.
import sys
from cryptography import x509
from cryptography.hazmat.primitives import serialization as s, hashes
from cryptography.hazmat.primitives.asymmetric import rsa
key_path, crt_path = sys.argv[1], sys.argv[2]
try:
    k = s.load_pem_private_key(open(key_path, 'rb').read(), None)
except Exception as e:
    print('::error title=Secure Boot key::RUDOLF_SB_KEY is not a PEM private key without a passphrase: %s' % type(e).__name__); sys.exit(1)
if not isinstance(k, rsa.RSAPrivateKey) or k.key_size != 2048:
    print('::error title=Secure Boot key::RUDOLF_SB_KEY must be an RSA-2048 key (made by iso/systools/mksbkey.sh)'); sys.exit(1)
c = x509.load_pem_x509_certificate(open(crt_path, 'rb').read())
if c.public_key().public_numbers() != k.public_key().public_numbers():
    print('::error title=Secure Boot key::RUDOLF_SB_KEY does not belong to %s; computers that enrolled that certificate would refuse what it signs' % crt_path); sys.exit(1)
print('Secure Boot key OK, certificate', c.fingerprint(hashes.SHA256()).hex(':').upper(), c.subject.rfc4514_string())
