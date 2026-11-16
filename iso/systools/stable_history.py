#!/usr/bin/env python3
"""The list of every release published as stable, signed with the release key. GPL-2.0-or-later.

  stable_history.py PAGES_DIR KEYFILE [--seed-from-git]
Rudolf OS 1.6.0 (the owner, 2026-11-16): Settings' "Roll back to the last stable version" shows only when the running
system is not a published stable release (or Get beta versions is on); a computer finds out from
PAGES_DIR/stable/history.txt and history.sig, checked with the official key (sys/rudolf-update.c, stable_fetch):

    rudolf-stable-history 1
    S 2026111503 1.5.9.261115 2026-11-15
    ...

One line per release ever published as stable, oldest first. Run by the release workflow when it promotes a beta and
at every run (where the lists are kept from expiring), so the list follows stable/manifest.txt. A release on the list
stays on it (computers keep the longest signed list they have seen). --seed-from-git (when there is no list yet): the
releases stable/manifest.txt named in PAGES_DIR's git history, each still checked against its own signature with the
public keys in iso/trusted-keys.txt; only those signed by the release key are added."""
import argparse, base64, os, subprocess, sys
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey, Ed25519PublicKey
from cryptography.exceptions import InvalidSignature

HERE = os.path.dirname(os.path.abspath(__file__)); ISO = os.path.dirname(HERE)
HEAD = 'rudolf-stable-history 1'


def pubkeys(path):
    out = []
    for l in open(path).read().splitlines():
        l = l.split('#')[0].strip()
        if len(l) == 44:
            try: out.append(Ed25519PublicKey.from_public_bytes(base64.b64decode(l)))
            except Exception: pass
    return out


def signed(text, sig, keys):
    for k in keys:
        try: k.verify(sig, text); return True
        except InvalidSignature: pass
    return False


def fields(text):
    d = {}
    for l in text.decode(errors='replace').splitlines():
        k, _, v = l.partition(' ')
        if k in ('serial', 'version', 'released') and k not in d: d[k] = v.strip()
    return d if d.get('serial', '').isdigit() else None


def git(pages, *a):
    return subprocess.run(['git', '-C', pages] + list(a), capture_output=True, check=False).stdout


def main():
    a = argparse.ArgumentParser(); a.add_argument('pages'); a.add_argument('key'); a.add_argument('--seed-from-git', action='store_true')
    a.add_argument('--keys', default=os.path.join(ISO, 'trusted-keys.txt'), help='the public keys (tests only: another list)')
    o = a.parse_args()
    st = os.path.join(o.pages, 'stable'); hp = os.path.join(st, 'history.txt'); mp = os.path.join(st, 'manifest.txt')
    if not os.path.exists(mp): print('stable: no release list, no history'); return 0
    keys = pubkeys(o.keys)
    rows = {}
    if os.path.exists(hp):
        t = open(hp, 'rb').read(); g = open(os.path.join(st, 'history.sig'), 'rb').read() if os.path.exists(os.path.join(st, 'history.sig')) else b''
        if not signed(t, g, keys): sys.exit('stable/history.txt is not signed by the release key: not extended (look at it by hand)')
        for l in t.decode().splitlines()[1:]:
            p = l.split(' ')
            if len(p) >= 2 and p[0] == 'S' and p[1].isdigit(): rows[int(p[1])] = l
    elif o.seed_from_git:
        # every version of stable/manifest.txt in the pages branch's history, with the signature committed beside it
        for c in git(o.pages, 'log', '--format=%H', '--', 'stable/manifest.txt').decode().split():
            t = git(o.pages, 'show', f'{c}:stable/manifest.txt'); g = git(o.pages, 'show', f'{c}:stable/manifest.sig')
            f = fields(t) if t and len(g) == 64 and signed(t, g, keys) else None
            if f: rows.setdefault(int(f['serial']), f"S {f['serial']} {f.get('version', '?')} {f.get('released', '?')}")
    t = open(mp, 'rb').read(); g = open(os.path.join(st, 'manifest.sig'), 'rb').read()
    f = fields(t)
    if not f or not signed(t, g, keys): sys.exit('stable/manifest.txt is not signed by the release key')
    rows.setdefault(int(f['serial']), f"S {f['serial']} {f.get('version', '?')} {f.get('released', '?')}")
    text = ('\n'.join([HEAD] + [rows[k] for k in sorted(rows)]) + '\n').encode()
    if os.path.exists(hp) and open(hp, 'rb').read() == text: print(f'stable/history.txt: {len(rows)} releases, unchanged'); return 0
    key = Ed25519PrivateKey.from_private_bytes(base64.b64decode(open(o.key).read().strip()))
    if not any(k.public_bytes_raw() == key.public_key().public_bytes_raw() for k in keys): sys.exit(f'the key is not one of {o.keys}')
    open(hp, 'wb').write(text); open(os.path.join(st, 'history.sig'), 'wb').write(key.sign(text))
    print(f'stable/history.txt: {len(rows)} releases, signed')
    return 0


if __name__ == '__main__': sys.exit(main())
