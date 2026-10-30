#!/usr/bin/env python3
"""deadcheck.py: nothing unused gets into a release (1.4.4). Run by ci_release.sh before anything is built, and as a
host test. Exits 1 and names what it found:
  - a top-level function (or arrow function) in desktop/*.js that nothing else in desktop/ names (its own module, another
    one, build19.py's patches or the base page base_v24.html);
  - a C program in sys/ with an unused static function (gcc -Wunused-function);
  - build output committed into sys/root (such as Chromium's _metadata cache of an extension);
  - a program listed in systools/base-remove.txt that mkrootfs.sh still copies.
GPL-2.0-or-later."""
import glob, os, re, subprocess, sys

ISO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOP = os.path.dirname(ISO)
DESK = os.path.join(TOP, 'desktop')
bad = []

# ---- desktop functions
texts = {}
for f in sorted(glob.glob(os.path.join(DESK, '*.js')) + [os.path.join(DESK, 'build19.py'), os.path.join(DESK, 'base_v24.html')]):
    if os.path.exists(f):
        texts[f] = open(f, encoding='utf-8', errors='ignore').read()
alltext = '\n'.join(texts.values())
WORD = set('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_$')
def uses(n, stop=2):
    """how often n is named as itself: not part of a longer name, not a property of the same name (x.n), but ...n counts"""
    c, i = 0, alltext.find(n)
    while i >= 0 and c < stop:
        a, b = alltext[i - 1] if i else ' ', alltext[i + len(n)] if i + len(n) < len(alltext) else ' '
        if a not in WORD and b not in WORD and (a != '.' or alltext[max(0, i - 3):i] == '...'):
            c += 1
        i = alltext.find(n, i + 1)
    return c
for f, s in texts.items():
    if not f.endswith('.js'):
        continue
    names = set(re.findall(r'^(?:async\s+)?function\s+(\w+)\s*\(', s, re.M))
    names |= set(re.findall(r'^(?:const|let|var)\s+(\w+)\s*=\s*(?:async\s*)?(?:\([^)]*\)|\w+)\s*=>', s, re.M))
    for n in sorted(names):
        if uses(n) <= 1:
            bad.append(f'desktop/{os.path.basename(f)}: {n} is defined but never used')

# ---- C programs
gio = subprocess.run(['pkg-config', '--cflags', 'gio-2.0', 'gio-unix-2.0'], capture_output=True, text=True).stdout.split()
for c in sorted(glob.glob(os.path.join(ISO, 'sys', '*.c'))):
    r = subprocess.run(['gcc', '-c', '-o', '/dev/null', '-Wunused-function'] + gio + [c], capture_output=True, text=True)   # (-fsyntax-only does not report these)
    for line in r.stderr.splitlines():
        if 'defined but not used' in line:
            bad.append('sys/' + line.split('/sys/', 1)[-1])

# ---- build output committed into sys/root (Chromium's extension cache, Python caches, editor files)
GEN = re.compile(r'(^|/)(_metadata|__pycache__)/|\.pyc$|~$|\.swp$|(^|/)\.DS_Store$')
r = subprocess.run(['git', '-C', TOP, 'ls-files', 'iso/sys/root'], capture_output=True, text=True)
for f in r.stdout.split():
    if GEN.search(f):
        bad.append(f'{f}: build output committed into sys/root (it is made where it is used)')

# ---- removed programs still copied by full builds
rm = [l.strip() for l in open(os.path.join(ISO, 'systools', 'base-remove.txt')) if l.strip() and not l.startswith('#')]
mk = open(os.path.join(ISO, 'mkrootfs.sh')).read()
for p in rm:
    if p.startswith('usr/bin/') and re.search(r'(?<![\w.-])' + re.escape(os.path.basename(p)) + r'(?![\w.-])', mk):
        bad.append(f'mkrootfs.sh still copies {p}, which base-remove.txt removes')

for b in bad:
    print('deadcheck:', b)
print(f'deadcheck: {"FAILED" if bad else "ok"} ({len(bad)} found)')
sys.exit(1 if bad else 0)
