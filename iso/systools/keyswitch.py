#!/usr/bin/env python3
# 1.6.3 Part 0: the release key's replacement, step by step.
#
# A release is signed by the one key RPK_SIGNING_KEY holds, and only that key may sign: keycheck.py refuses a release
# signed by any key that is not the *first* in iso/trusted-keys.txt, because a computer that has not updated trusts only
# the keys it shipped with and would refuse a release signed by another. So while the key is replaced the list holds the
# old key and the new one, old first; the switch is one change of that order, made at the moment the owner points the
# release environment's RPK_SIGNING_KEY at the new key and deletes the repository secret (docs/secure-boot.md,
# "Replacing the release key").
#
# What the switch's day needs, and nothing else: say where the replacement stands (--status), keep the list one the
# tools read (--check, run by deadcheck.py), and make the one change of order (--switch --yes). It reads no private key
# and no secret, and writes nothing but the list it is given.
#
# A later rotation changes ROTATION below and nothing else: the gate never names a key, so a list that has moved past
# this pair still checks.
# GPL-2.0-or-later.
import os, re, sys

# The rotation of 2026-11-24, for --status and --switch's default target only: the key of 1.4.9 to 1.6.3, and the one
# the owner made offline beside it.
ROTATION = {'old': 'UqTqXIW8vtITjwoMix+MkFVZ0GAee9+kOS1WBIs/3mo=',
            'new': 'IcNJiy4zFkKUU3g3jalcgTQlneiDXv3E0/bbJt19cVg='}
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PATH = os.path.join(ROOT, 'trusted-keys.txt')
SHAPE = re.compile(r'[A-Za-z0-9+/]{43}=')          # one Ed25519 public key, base64 (as mkuki.py and the tools read it)
HEAD = ('# Release and package signing keys this Rudolf OS trusts (Ed25519 public keys, base64, one per line).\n'
        '# Copied into the system as /etc/rudolf/rpk-keys. Never put a private key here.\n')
USAGE = 'usage: keyswitch.py [--status | --check | --switch [--yes] [--to KEY]] [TRUSTED-KEYS]'


def keys(path):
    """The keys in the list, in order, comments and blank lines left out."""
    return [l.strip() for l in open(path) if l.strip() and not l.startswith('#')]


def problems(path):
    """What would stop the list working -- the bounds mkuki.py (recovery.efi) and the tools need. No key is named here,
    so a later rotation does not have to change the gate."""
    if not os.path.exists(path):
        return [f'{path} is not there']
    try:
        ks = keys(path)
    except OSError as e:
        return [f'{path}: {e.strerror}']
    bad = []
    for k in ks:
        if not SHAPE.fullmatch(k):
            bad.append(f'{path}: {k[:12]}... is not one Ed25519 public key in base64 (43 characters and =)')
    seen = set()
    for k in ks:
        if k in seen:
            bad.append(f'{path}: the key {k[:12]}... is given twice; a key given twice signs nothing and hides a mistake')
        seen.add(k)
    if not 1 <= len(ks) <= 4:
        bad.append(f'{path}: one to four public keys (mkuki.py puts them in recovery.efi), not {len(ks)}')
    return bad


def status(path):
    ks = keys(path)
    signs = ks[0] if ks else '(nothing)'
    print(f'keyswitch: {len(ks)} key(s) trusted, the first one signs: {signs}')
    for i, k in enumerate(ks):
        name = {v: k2 for k2, v in ROTATION.items()}.get(k, 'another key')
        print(f'  {i + 1}. {k}  {name}{"  <- signs" if i == 0 else ""}')
    old, new = ROTATION['old'] in ks, ROTATION['new'] in ks
    if signs == ROTATION['old'] and new:
        print('keyswitch: the 2026-11-24 rotation is ready: both keys are trusted and the old one signs.')
        print('  On the day: `keyswitch.py --switch --yes`, then the owner sets the release environment\'s '
              'RPK_SIGNING_KEY to the new key and deletes the repository secret, then the change merges.')
    elif signs == ROTATION['new']:
        print('keyswitch: switched: the new key signs. The old one may leave the list (Part 0 item 3e) once every '
              'supported stable release trusts the new one.')
    elif old or new:
        print('keyswitch: the 2026-11-24 pair is in the list but does not sign: a later key does. Nothing of this '
              'rotation is left to do.')
    else:
        print('keyswitch: the 2026-11-24 pair is no longer in the list. If a new rotation has begun, change ROTATION in '
              'this tool; the --check gate does not name any key, so it still holds.')


def switch(path, yes, to):
    bad = problems(path)
    if bad:
        for b in bad:
            print('keyswitch:', b)
        return 1
    ks = keys(path)
    if to is None:
        to = ROTATION['new']
    if to not in ks:
        print(f'keyswitch: nothing to switch: {path} does not trust the key to move first ({to}); give --to with one it '
              'does trust')
        return 1
    if ks[0] == to:
        print(f'keyswitch: already switched: {to} is the first key in the list, so it signs')
        return 0
    if not yes:
        print('keyswitch: the switch is one change of order and the owner makes it at the moment the environment\'s '
              'secret changes: give --yes to write it')
        return 1
    rest = [k for k in ks if k != to]
    text = HEAD + (
        '# 1.6.3 Part 0: the release key was replaced (the new one made offline by the owner). While every supported\n'
        '# release trusts both, only the first key signs (keycheck.py), so the new key leads and the old one follows it;\n'
        '# the old key may leave the list once the stable releases every computer may still be on trust the new one\n'
        '# (Part 0 item 3e).\n'
        f'{to}\n')
    for k in rest:
        text += (f'# The key this one replaced: kept until no supported release is signed by it.\n' if k == ks[0] else '') + k + '\n'
    open(path, 'w').write(text)
    print(f'keyswitch: switched: {path} names {to} first, so that key signs')
    print('keyswitch: now the owner: the release environment\'s RPK_SIGNING_KEY = that key, and the repository secret '
          'deleted; then this change merges and the next release is signed by it')
    return 0


def main(argv):
    if '-h' in argv or '--help' in argv:
        print(USAGE)
        return 0
    args = [a for a in argv[1:] if not a.startswith('-')]
    to = argv[argv.index('--to') + 1] if '--to' in argv and argv.index('--to') + 1 < len(argv) else None
    if '--to' in argv and to is None:
        # on the day this would otherwise switch to the default key, not the one meant
        print('keyswitch: --to needs the public key to move first'); print(USAGE); return 1
    path = next((a for a in args if a != to), PATH)
    if '--check' in argv:
        bad = problems(path)
        for b in bad:
            print('keyswitch:', b)
        print(f'keyswitch: {"FAILED" if bad else "OK"} ({len(bad)} found)')
        return 1 if bad else 0
    if '--switch' in argv:
        return switch(path, '--yes' in argv, to)
    status(path)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
