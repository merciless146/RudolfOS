#!/usr/bin/env python3
"""1.6.1 (docs/plan-1.6.1.md Part 11): the release build installs rudolf-rootd and the root helpers as it must. The
helpers rudolf-rootd knows (its HELPERS list) are the ones mkrootfs.sh and ci_release.sh build, from sources the
base layer's checksum covers, with the hardening flags checked; mkrootfs.sh's lines run here make them root's 0700
(rudolf-rootd 0755, never set-user-ID); rudolf-init starts rudolfd through rudolf-rootd.   python3 rootbuild161_t.py"""
import os, re, subprocess, sys, tempfile, shutil
ISO = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../..'); SYS = os.path.join(ISO, 'sys'); R = []
def ok(n, c, d=''): R.append(bool(c)); print(('PASS ' if c else 'FAIL ') + n + ('' if c else ' - ' + str(d)[:900]))
rd = open(f'{SYS}/rudolf-rootd.c').read(); mk = open(f'{ISO}/mkrootfs.sh').read(); ci = open(f'{ISO}/systools/ci_release.sh').read()
init = open(f'{SYS}/root/sbin/rudolf-init').read()
H = re.search(r'HELPERS\[\] = \{ (.*?), NULL \}', rd).group(1).replace('"', '').replace(',', ' ').split()
ok('1. rudolf-rootd knows some helpers, each with its source (sys/rudolf-NAME.c)', H and all(os.path.exists(f'{SYS}/rudolf-{h}.c') for h in H), H)
mh = re.search(r'^for h in ([\w ]+); do gcc .*usr/lib/rudolf/root/\$h', mk, re.M); ch = re.search(r'^\s*RHELP="([\w ]*)"', ci, re.M)
ok('2. mkrootfs.sh and ci_release.sh build exactly those', mh and ch and sorted(mh.group(1).split()) == sorted(H) == sorted(ch.group(1).split()), [mh and mh.group(1), ch and ch.group(1), H])
# (1.6.1 drives part 2: programs only a helper runs, never rudolf-rootd: drives-home, rudolfd's own functions for the accounts' folders)
I = ['drives-home']; ci_i = re.search(r'^\s*RINNER="([\w -]*)"', ci, re.M)
ok('2b. the programs only a helper runs (drives-home) are built by both too, root\'s 0700 beside the helpers, and are not among rudolf-rootd\'s helpers', ci_i and ci_i.group(1).split() == I and not set(I) & set(H)
   and all(re.search(r'^gcc .*-o usr/lib/rudolf/root/' + i + r' .*&& chmod 700 usr/lib/rudolf/root/' + i + '$', mk, re.M) and f'-o "$B/root-{i}"' in ci for i in I), [ci_i and ci_i.group(1), H])
bs = re.search(r'^BSRC=.*$', ci, re.M).group(0)
ok('3. the base layer is rebuilt when any of their sources change (BSRC)', all(f'sys/{f}' in bs for f in ['rudolf-rootd.c', 'rudolf-root.h', 'rudolfd-root.inc', 'rudolfd-roothome.inc', 'rudolfd-rootpower.inc', 'rudolfd-rootnet.inc', 'rudolfd-rootdev.inc'] + [f'rudolf-{h}.c' for h in H + I]), bs[:200])
hc = re.search(r'^\s*python3 systools/hardcheck.py .*$', ci, re.M).group(0)
ok('4. their hardening flags are checked (hardcheck.py), and the release checks the modes in the base layer', '"$B/rudolf-rootd"' in hc and '"$B/root-$h"' in hc and '$RHELP $RINNER' in hc and '"0:0 700"' in ci and '-rwx------ root/root' in ci
   and re.search(r'for h in \$RHELP \$RINNER; do\n\s*for l in \$\(ldd "\$B/root-\$h"', ci) and re.search(r'for h in \$RHELP \$RINNER; do unsquashfs -lls', ci), hc[-200:])
ok('5. rudolf-init starts rudolfd through rudolf-rootd (rudolfd alone where there is none)', re.search(r'if \[ -x /usr/bin/rudolf-rootd \]; then rudolf-rootd /usr/bin/rudolfd > \$RDLOG 2>&1 &\nelse rudolfd > \$RDLOG 2>&1 &\nfi', init), '')
# mkrootfs.sh's own lines, run in a stand-in root
T = tempfile.mkdtemp(); lines = [l for l in mk.split('\n') if 'rudolf-rootd' in l and l.startswith('gcc') or l.startswith('mkdir -p usr/lib/rudolf/root') or (l.startswith('for h in') and 'usr/lib/rudolf/root' in l) or (l.startswith('gcc') and 'usr/lib/rudolf/root/' in l)]
os.makedirs(f'{T}/usr/bin')
r = subprocess.run(['bash', '-c', 'set -e; cd "$1"; SYS="$2"; . "$3"; ' + '\n'.join(lines), '-', T, SYS, f'{ISO}/systools/hardflags.sh'], capture_output=True, text=True)
st = lambda p: os.stat(f'{T}/{p}')
try:
    ok('6. built by mkrootfs.sh\'s lines: each helper 0700 in usr/lib/rudolf/root (0755), rudolf-rootd 0755, nothing set-user-ID (the layer makes them root\'s: -all-root)',
       r.returncode == 0 and all(st(f'usr/lib/rudolf/root/{h}').st_mode & 0o7777 == 0o700 for h in H + I)
       and st('usr/lib/rudolf/root').st_mode & 0o7777 == 0o755 and st('usr/bin/rudolf-rootd').st_mode & 0o7777 == 0o755, r.stderr + str(lines))
    hk = subprocess.run([sys.executable, f'{ISO}/systools/hardcheck.py', f'{T}/usr/bin/rudolf-rootd'] + [f'{T}/usr/lib/rudolf/root/{h}' for h in H + I], capture_output=True, text=True)
    ok('7. ...with the hardening flags (hardcheck.py)', hk.returncode == 0, hk.stdout + hk.stderr)
except OSError as e: ok('6. built by mkrootfs.sh\'s lines', False, f'{e} {r.stderr} {lines}')
shutil.rmtree(T)
print(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if all(R) else 1)
