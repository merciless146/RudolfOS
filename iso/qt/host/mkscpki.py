#!/usr/bin/env python3
# Test PKI for the smart card tests (1.6.0, plan Part 20): a "DoD-like" root (pinned by the test), an intermediate and an
# EC intermediate under it, a card's certificate (expiring in 10 days), an unpinned root with a child, and a forgery
# (the pinned root's name as issuer, another key's signature). Writes OUT/bundle.p7b (DER, PKCS#7), OUT/card.der,
# OUT/pins.json ({sha1: name}) and OUT/expect.json.   python3 mkscpki.py OUT
import sys, json, datetime, hashlib, os
from cryptography import x509
from cryptography.x509.oid import NameOID
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa, ec
from cryptography.hazmat.primitives.serialization import pkcs7
out = sys.argv[1]; os.makedirs(out, exist_ok=True)
now = datetime.datetime.now(datetime.timezone.utc)
def name(cn): return x509.Name([x509.NameAttribute(NameOID.COUNTRY_NAME, 'US'), x509.NameAttribute(NameOID.ORGANIZATION_NAME, 'Test Government'), x509.NameAttribute(NameOID.COMMON_NAME, cn)])
def cert(cn, key, issuer_cn, issuer_key, ca, days=3650):
    b = (x509.CertificateBuilder().subject_name(name(cn)).issuer_name(name(issuer_cn)).public_key(key.public_key())
         .serial_number(x509.random_serial_number()).not_valid_before(now - datetime.timedelta(days=1)).not_valid_after(now + datetime.timedelta(days=days))
         .add_extension(x509.BasicConstraints(ca=ca, path_length=None), critical=True))
    return b.sign(issuer_key, hashes.SHA384() if isinstance(issuer_key, ec.EllipticCurvePrivateKey) else hashes.SHA256())
rk = rsa.generate_private_key(public_exponent=65537, key_size=2048); root = cert('Test DoD Root CA 9', rk, 'Test DoD Root CA 9', rk, True)
ik = rsa.generate_private_key(public_exponent=65537, key_size=2048); inter = cert('Test DOD ID CA-90', ik, 'Test DoD Root CA 9', rk, True)
ek = ec.generate_private_key(ec.SECP384R1()); einter = cert('Test DOD EMAIL CA-91', ek, 'Test DoD Root CA 9', rk, True)
ck = ec.generate_private_key(ec.SECP256R1()); card = cert('DOE.JOHN.Q.1234567890', ck, 'Test DOD EMAIL CA-91', ek, False, days=10)
ok = rsa.generate_private_key(public_exponent=65537, key_size=2048); other = cert('Other Root', ok, 'Other Root', ok, True)
oc = cert('Other CA', rsa.generate_private_key(public_exponent=65537, key_size=2048), 'Other Root', ok, True)
fk = rsa.generate_private_key(public_exponent=65537, key_size=2048); forged = cert('Forged CA', fk, 'Test DoD Root CA 9', fk, True)
open(f'{out}/bundle.p7b', 'wb').write(pkcs7.serialize_certificates([other, oc, forged, card, einter, inter, root], serialization.Encoding.DER))
open(f'{out}/card.der', 'wb').write(card.public_bytes(serialization.Encoding.DER))
sha1 = hashlib.sha1(root.public_bytes(serialization.Encoding.DER)).hexdigest()
json.dump({sha1: 'Test DoD Root CA 9'}, open(f'{out}/pins.json', 'w'))
json.dump({'roots': ['Test DoD Root CA 9'], 'cas': sorted(['Test DOD ID CA-90', 'Test DOD EMAIL CA-91', 'DOE.JOHN.Q.1234567890']), 'left': sorted(['Other Root', 'Other CA', 'Forged CA'])}, open(f'{out}/expect.json', 'w'))
for n, c in [('root', root), ('inter', inter)]: open(f'{out}/{n}.der', 'wb').write(c.public_bytes(serialization.Encoding.DER))
