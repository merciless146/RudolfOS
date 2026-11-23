#!/usr/bin/env python3
"""WendigOS (2.0, docs/plan-2.0.md): kconfig.sh moved from one kernel to another, option by option. Tests only.

  kconfdiff.py OLD-TREE NEW-TREE [KCONFIG.SH ...]

Both trees have been configured by kconfig.sh (OLD: the 1.6.x kernel, 6.18.y; NEW: 7.3.y). For every option kconfig.sh
and kconfig-required.txt name, it prints what each kernel made of it: still there with the same value, a value that
changed (a dependency moved, a default changed), or gone (no "config NAME" in the new tree's Kconfig files: removed or
renamed). Then the options the new kernel turned on that the old one does not have at all (new options), grouped by
the folder of their Kconfig file, so each can be decided. Exit status 0 always: the report is read, kconfig.sh decides.
GPL-2.0-or-later.
"""
import os, re, sys

def symbols(tree):
    """Every Kconfig symbol a tree defines, with the Kconfig file that defines it."""
    s = {}
    for d, _, fs in os.walk(tree):
        if '/.git' in d: continue
        for f in fs:
            if not f.startswith('Kconfig'): continue
            p = os.path.join(d, f)
            try: t = open(p, errors='replace').read()
            except OSError: continue
            for m in re.finditer(r'^\s*(?:menu)?config\s+([A-Za-z0-9_]+)\s*$', t, re.M):
                s.setdefault(m.group(1), os.path.relpath(p, tree))
    return s

def config(tree):
    c = {}
    for l in open(os.path.join(tree, '.config'), errors='replace'):
        m = re.match(r'^CONFIG_([A-Za-z0-9_]+)=(.*)$', l)
        if m: c[m.group(1)] = m.group(2)
        m = re.match(r'^# CONFIG_([A-Za-z0-9_]+) is not set$', l)
        if m: c[m.group(1)] = 'n'
    return c

def asked(paths):
    """The names kconfig.sh and the required list use (any word that looks like an option)."""
    names = set()
    for p in paths:
        for l in open(p, errors='replace'):
            l = l.split('#', 1)[0] if not p.endswith('.txt') else (l if not l.startswith('#') else '')
            if p.endswith('.txt'):
                w = l.split()
                if w: names.add(w[0])
                continue
            for w in re.findall(r'\b[A-Z][A-Z0-9_]*[a-z]?[A-Z0-9_]*\b', l):
                names.add(w)
    return names

def main():
    old, new = sys.argv[1], sys.argv[2]
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    files = sys.argv[3:] or [os.path.join(here, 'kconfig.sh'), os.path.join(here, 'kconfig-required.txt')]
    so, sn = symbols(old), symbols(new); co, cn = config(old), config(new)
    names = sorted(n for n in asked(files) if n in so or n in sn)
    same, changed, gone, added = [], [], [], []
    for n in names:
        vo, vn = co.get(n, '-'), cn.get(n, '-')
        if n in so and n not in sn: gone.append((n, vo, so[n]))
        elif n not in so and n in sn: added.append((n, vn, sn[n]))
        elif vo != vn: changed.append((n, vo, vn, sn.get(n)))
        else: same.append(n)
    print(f'==== kconfig.sh and kconfig-required.txt: {len(names)} options; {len(same)} the same, {len(changed)} changed, '
          f'{len(gone)} gone from the new kernel, {len(added)} only in the new kernel')
    print('---- GONE (no "config NAME" in the new tree: removed or renamed; old value, where it was)')
    for n, v, f in gone: print(f'GONE {n} {v} {f}')
    print('---- CHANGED (old value -> new value, where it is now)')
    for n, a, b, f in changed: print(f'CHANGED {n} {a} -> {b} {f}')
    print('---- ONLY NEW (named by kconfig.sh, unknown to the old kernel)')
    for n, v, f in added: print(f'ONLYNEW {n} {v} {f}')
    # options the new kernel has turned on that the old kernel does not know at all
    fresh = [(n, v, sn.get(n, '?')) for n, v in cn.items() if v not in ('n', '') and n not in so and n not in co]
    by = {}
    for n, v, f in fresh: by.setdefault(os.path.dirname(f), []).append(f'{n}={v}')
    print(f'---- NEW OPTIONS ON in the new kernel ({len(fresh)}), by Kconfig folder')
    for d in sorted(by): print(f'NEW {d}: ' + ' '.join(sorted(by[d])))
    # and the old kernel's options that are on but gone from the new one entirely (dropped by the kernel itself)
    lost = sorted(n for n, v in co.items() if v not in ('n', '') and n not in sn)
    print(f'---- OLD OPTIONS ON that the new kernel no longer has ({len(lost)})')
    for n in lost: print(f'LOST {n}={co[n]} {so.get(n, "?")}')
    # on in the old, off (or not set) in the new: a dependency or default moved
    off = sorted(n for n, v in co.items() if v in ('y', 'm') and n in sn and cn.get(n, 'n') in ('n', '-'))
    print(f'---- ON IN THE OLD, OFF IN THE NEW ({len(off)})')
    for n in off: print(f'OFF {n}={co[n]} -> {cn.get(n, "not set")} {sn.get(n)}')

main()
