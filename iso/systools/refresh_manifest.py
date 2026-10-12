#!/usr/bin/env python3
"""Keep a channel's signed release list from expiring while there is nothing new to release.
GPL-2.0-or-later.

  refresh_manifest.py CHANNEL_DIR KEYFILE [--days-left 5] [--expires-days 14]
When CHANNEL_DIR/manifest.txt expires within --days-left days, writes it again with a new expiry date and
signs it. The serial and the files stay the same, so computers see "up to date" and download nothing."""
import argparse, base64, datetime, os, sys
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

a = argparse.ArgumentParser(); a.add_argument('channel'); a.add_argument('key')
a.add_argument('--days-left', type=int, default=5); a.add_argument('--expires-days', type=int, default=14)
o = a.parse_args()
mp = os.path.join(o.channel, 'manifest.txt')
if not os.path.exists(mp): print(f'{o.channel}: no release list'); sys.exit(0)
lines = open(mp, 'rb').read().decode().split('\n')
today = datetime.date.today()
exp = next((l[8:] for l in lines if l.startswith('expires ')), '')
if exp and datetime.date.fromisoformat(exp) - today > datetime.timedelta(days=o.days_left):
    print(f'{o.channel}: valid until {exp}'); sys.exit(0)
new = (today + datetime.timedelta(days=o.expires_days)).isoformat()
lines = [f'expires {new}' if l.startswith('expires ') else l for l in lines]
text = '\n'.join(lines).encode()
key = Ed25519PrivateKey.from_private_bytes(base64.b64decode(open(o.key).read().strip()))
open(mp, 'wb').write(text); open(os.path.join(o.channel, 'manifest.sig'), 'wb').write(key.sign(text))
print(f'{o.channel}: re-signed, now valid until {new}')
