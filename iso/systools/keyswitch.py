#!/usr/bin/env python3
# 1.6.3 Part 0: the release key's replacement, step by step.
#
# A release is signed by the one key RPK_SIGNING_KEY holds, and only that key may sign: keycheck.py refuses a release
# signed by any key that is not the *first* in iso/trusted-keys.txt, because a computer that has not updated trusts only
# the keys it shipped with, and would refuse a release signed by another. So while the key is replaced the list holds
# the old key and the new one, old first; the switch is one change of that order, made at the same moment the owner sets
# the release environment's RPK_SIGNING_KEY to the new key and deletes the repository secret (docs/secure-boot.md,
# "Replacing the release key").
#
# This tool says where the replacement stands (--status, the default), checks that the list is one the tools and
# mkuki.py can read (--check, run by deadcheck.py before every pull request), and makes that one change (--switch --yes,
# only when the owner says the moment has come). It reads no private key and no secret, and never writes anything but
# the list it is given.
# GPL-2.0-or-later.
import os, re, sys

OLD = 'UqTqXIW8vtITjwoMix+MkFVZ0GAee9+kOS1WBIs/3mo='   # the release key of 1.4.9 to 1.6.3
NEW = 'IcNJiy4zFkKUU3g3jalcgTQlneiDXv3E0/bbJt19cVg='   # made offline by the owner, 2026-11-24
OFFICIAL = {OLD: 'the old key', NEW: 'the new key'}
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PATH = os.path.join(ROOT, 'trusted-keys.txt')
SHAPE = re.compile(r'[A-Za-z0-9+/]{43}=')          # one Ed25519 public key, base64 (as mkuki.py and the tools read it)
HEAD = ('# Release and package signing keys this Rudolf OS trusts (Ed25519 public keys, base64, one per line).\n'
        '# Copied into the system as /etc/rudolf/rpk-keys. Never put a private key here.\n')


def keys(path):
    """The keys in the list, in order, comments and blank lines left out."""
    return [l.strip() for l in open(path) if l.strip() and not l.startswith('#')]


def problems(path):
    """What would stop the list working: it is what mkuki.py (recovery.efi) and keycheck.py need it to be."""
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
    if not set(ks) & set(OFFICIAL):
        bad.append(f'{path}: neither of the two official release keys is trusted ({OLD[:12]}... the old one, '
                   f'{NEW[:12]}... the new one); every computer would refuse this tree\'s releases')
    return bad


def status(path):
    ks = keys(path)
    signs = ks[0] if ks else '(nothing)'
    print(f'keyswitch: {len(ks)} key(s) trusted, the first one signs: {signs}'
          + (f' -- {OFFICIAL[signs]}' if signs in OFFICIAL else ''))
    for i, k in enumerate(ks):
        print(f'  {i + 1}. {k}  {OFFICIAL.get(k, "another key")}{"  <- signs" if i == 0 else ""}')
    both = OLD in ks and NEW in ks
    print('keyswitch: the replacement (1.6.3 Part 0):')
    print(f'  1. both official keys are trusted: {"yes" if both else "NO"}'
          + ('' if both else ' (nothing to switch: ' + ('the old key alone still signs' if NEW not in ks else
             'the new key is alone in the list') + ')'))
    print(f'  2. a stable release that trusts both has been out for a while: the owner\'s call. mkuki.py has carried '
          f'both in recovery.efi since the list did, so a computer on that release trusts either key.')
    print('  3. only main can use the signing key: `keyswitch-check` runs this tool and its test for every change of '
          'the list, fails on main if RPK_SIGNING_KEY is missing, and on demand from a branch (expect=refused) fails '
          'unless the branch is refused it (Settings > Environments > release: Deployment branches = main).')
    if both and signs == OLD:
        print('keyswitch: next, when 1 to 3 hold -- the switch, then the owner\'s two clicks, then the merge:')
        print('  python3 iso/systools/keyswitch.py --switch --yes')
        print('  the owner: Settings > Environments > release > RPK_SIGNING_KEY = the new key, and the repository '
              'secret RPK_SIGNING_KEY deleted')
        print('  the release after the merge is signed by the new key; the old one leaves the list (Part 0 item 3e) '
              'once every supported stable release trusts the new one')
    elif both:
        print('keyswitch: switched -- the new key signs; the old one may leave the list once every supported stable '
              'release trusts the new one (1.6.3 Part 0 item 3e)')


def switch(path, yes):
    bad = problems(path)
    if bad:
        for b in bad:
            print('keyswitch:', b)
        return 1
    ks = keys(path)
    if OLD not in ks or NEW not in ks:
        print(f'keyswitch: nothing to switch: {path} does not hold both official keys')
        return 1
    if ks[0] == NEW:
        print('keyswitch: already switched: the new key is the first in the list, so it signs')
        return 0
    if not yes:
        print('keyswitch: the switch is one change of order and the owner makes it at the moment the environment\'s '
              'secret changes: give --yes to write it')
        return 1
    rest = [k for k in ks if k not in (OLD, NEW)]
    text = HEAD + (
        '# 1.6.3 Part 0: the release key was replaced on 2026-11-24 (made offline by the owner). While every supported\n'
        '# release trusts both, only the first key signs (keycheck.py), so the new key leads and the old one follows it.\n'
        f'# The old key may leave this list once the stable releases every computer may still be on trust the new one\n'
        f'# (Part 0 item 3e).\n'
        f'{NEW}\n'
        f'# The key of 1.4.9 to 1.6.3: kept until no supported release is signed by it.\n'
        f'{OLD}\n')
    for k in rest:
        text += k + '\n'
    open(path, 'w').write(text)
    print(f'keyswitch: switched: {path} names the new key first, so the new key signs')
    print('keyswitch: now the owner: the release environment\'s RPK_SIGNING_KEY = the new key, and the repository '
          'secret deleted; then this change merges and the next release is signed by the new key')
    return 0


def main(argv):
    args = [a for a in argv[1:] if not a.startswith('-')]
    path = args[0] if args else PATH
    if '--help' in argv or '-h' in argv:
        print(__doc__ or __file__)
        print('usage: keyswitch.py [--status | --check | --switch [--yes]] [TRUSTED-KEYS]')
        return 0
    if '--check' in argv:
        bad = problems(path)
        for b in bad:
            print('keyswitch:', b)
        print(f'keyswitch: {"FAILED" if bad else "OK"} ({len(bad)} found)')
        return 1 if bad else 0
    if '--switch' in argv:
        return switch(path, '--yes' in argv)
    status(path)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
