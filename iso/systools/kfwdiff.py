#!/usr/bin/env python3
"""WendigOS (2.0): the firmware a new kernel's drivers ask for, against the old kernel's and what Rudolf ships.

  kfwdiff.py OLD-MODULES NEW-MODULES FIRMWARE-LIST [BASE-FIRMWARE-NAMES]

OLD-MODULES and NEW-MODULES: lib/modules/VERSION folders (the 1.6.x release's and the test build's). FIRMWARE-LIST:
iso/firmware-extra.txt. BASE-FIRMWARE-NAMES: a file with the base layer's usr/lib/firmware paths, one per line. For every
file a new kernel's module asks for (modinfo's "firmware") that no old module asked for, it says whether Rudolf has it:
in the modules layer's list (firmware-extra.txt), in the base layer, in a family fwextra.py takes from linux-firmware but
missing from the list (the list must be made again: "REGENERATE"), or outside what Rudolf ships (said, not a failure).
Exit status 1 when the list must be made again. Tests and the kryptonite branch only. GPL-2.0-or-later.
"""
import os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fwextra

def wanted(moddir):
    kos = []
    for d, _, fs in os.walk(moddir):
        kos += [os.path.join(d, f) for f in fs if re.search(r'\.ko(\.[a-z]+)?$', f)]
    req = {}
    for i in range(0, len(kos), 200):
        part = kos[i:i + 200]
        for k in part:
            o = subprocess.run(['modinfo', '-F', 'firmware', k], capture_output=True, text=True).stdout
            for f in o.split():
                req.setdefault(f, set()).add(os.path.basename(k).split('.ko')[0])
    return req

def main():
    old, new, lst = sys.argv[1:4]; base = set()
    if len(sys.argv) > 4:
        base = {l.strip().lstrip('./').replace('usr/lib/firmware/', '', 1).replace('squashfs-root/', '', 1) for l in open(sys.argv[4])}
    have = set()
    for l in open(lst):
        w = l.split()
        if len(w) >= 3 and w[0] == 'F': have.add(w[2])
        if len(w) >= 3 and w[0] == 'L': have.add(w[1])
    def shipped(f, s):
        if any(f == h or f.startswith(h + '/') for h in s): return True
        return any(f.startswith(h.rstrip('/') + '/') for h in s if '/' in h and not h.endswith(('.bin', '.fw')))
    ro, rn = wanted(old), wanted(new)
    print(f'==== firmware asked for: {len(ro)} names by the old kernel\'s modules, {len(rn)} by the new one\'s')
    regen = []
    for f in sorted(set(rn) - set(ro)):
        mods = ' '.join(sorted(rn[f]))
        pat = bool(re.search(r'[*?\[]', f))
        if shipped(f, have): st = 'LISTED'
        elif shipped(f, base): st = 'BASE'
        elif fwextra.want(f) and not pat: st = 'REGENERATE'; regen.append(f)
        else: st = 'NOT-SHIPPED'
        print(f'{st} {f} ({mods})')
    gone = sorted(set(ro) - set(rn))
    print(f'---- asked for by the old kernel only ({len(gone)})')
    for f in gone: print(f'OLDONLY {f} ({" ".join(sorted(ro[f]))})')
    if regen:
        print(f'==== {len(regen)} new files in the families firmware-extra.txt takes: make the list again (fwextra.py list)')
        sys.exit(1)
    print('==== nothing new for firmware-extra.txt')

main()
