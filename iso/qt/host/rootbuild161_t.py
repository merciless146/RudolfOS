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
# (1.6.3 Part 2: a helper with libraries of its own, fprint with libfprint, is built on a line of its own in mkrootfs.sh)
mo = re.findall(r'gcc [^\n]*-o usr/lib/rudolf/root/([\w-]+) "\$SYS/rudolf-\1\.c"[^\n]*&& chmod 700 usr/lib/rudolf/root/\1', mk)
mo = [m for m in mo if m != 'drives-home']
ok('2. mkrootfs.sh and ci_release.sh build exactly those', mh and ch and sorted(mh.group(1).split() + mo) == sorted(H) == sorted(ch.group(1).split()), [mh and mh.group(1), mo, ch and ch.group(1), H])
# (1.6.1 drives part 2: programs only a helper runs, never rudolf-rootd: drives-home, rudolfd's own functions for the accounts' folders)
I = ['drives-home']; ci_i = re.search(r'^\s*RINNER="([\w -]*)"', ci, re.M)
ok('2b. the programs only a helper runs (drives-home) are built by both too, root\'s 0700 beside the helpers, and are not among rudolf-rootd\'s helpers', ci_i and ci_i.group(1).split() == I and not set(I) & set(H)
   and all(re.search(r'^gcc .*-o usr/lib/rudolf/root/' + i + r' .*&& chmod 700 usr/lib/rudolf/root/' + i + '$', mk, re.M) and f'-o "$B/root-{i}"' in ci for i in I), [ci_i and ci_i.group(1), H])
bs = re.search(r'^BSRC=.*$', ci, re.M).group(0)
ok('3. the base layer is rebuilt when any of their sources change (BSRC)', all(f'sys/{f}' in bs for f in ['rudolf-rootd.c', 'rudolf-root.h', 'rudolfd-root.inc', 'rudolfd-roothome.inc', 'rudolfd-rootpower.inc', 'rudolfd-rootnet.inc', 'rudolfd-rootdev.inc', 'rudolfd-rootother.inc', 'rudolfd-rootscreen.inc'] + [f'rudolf-{h}.c' for h in H + I]), bs[:200])
hc = re.search(r'^\s*python3 systools/hardcheck.py .*$', ci, re.M).group(0)
ok('4. their hardening flags are checked (hardcheck.py), and the release checks the modes in the base layer', '"$B/rudolf-rootd"' in hc and '"$B/root-$h"' in hc and '$RHELP $RINNER' in hc and '"0:0 700"' in ci and '-rwx------ root/root' in ci
   and re.search(r'for h in \$RHELP \$RINNER; do\n\s*for l in \$\(ldd "\$B/root-\$h"', ci) and re.search(r'for h in \$RHELP \$RINNER; do unsquashfs -lls', ci), hc[-200:])
ok('5. rudolf-init starts rudolfd through rudolf-rootd (rudolfd alone where there is none)', re.search(r'if \[ -x /usr/bin/rudolf-rootd \]; then rudolf-rootd /usr/bin/rudolfd > \$RDLOG 2>&1 &\nelse rudolfd > \$RDLOG 2>&1 &\nfi', init), '')
# mkrootfs.sh's own lines, run in a stand-in root
# 1.6.3: the release runner's packages against what these scripts ask for. libfprint-2 was never installed there and
# stopped four releases (2026-11-25 to 11-27); the next such package fails here, named, before the build starts.
DEPS = {  # what a script asks for -> the package that provides it (None: glibc's, in the runner's base image)
 'pkg-config libfprint-2': 'libfprint-2-dev',
 'pkg-config gio-2.0': 'libglib2.0-dev',
 'pkg-config gio-unix-2.0': 'libglib2.0-dev',
 'pkg-config gtk+-3.0': 'libgtk-3-dev',
 'pkg-config fontconfig': 'libfontconfig-dev',
 'pkg-config webkit2gtk-4.1': 'libwebkit2gtk-4.1-dev',
 '-lX11': 'libx11-dev',
 '-lXext': 'libxext-dev',
 '-lssl': 'libssl-dev',
 '-lcrypto': 'libssl-dev',
 '-lz': 'zlib1g-dev',
 '-lm': None,
 '-ldl': None,
}
VIA = {'libgtk-3-dev': 'libwebkit2gtk-4.1-dev',      # apt-cache show libwebkit2gtk-4.1-dev: Depends: libgtk-3-dev
       'libfontconfig-dev': 'libgtk-3-dev'}          # apt-cache depends libgtk-3-dev: Depends: libfontconfig-dev
def installed(pkg):                                     # the chain, until a package the runner installs
    seen = []
    while pkg not in apt:
        seen.append(pkg)
        if pkg not in VIA or pkg in seen[:-1]: return False, seen
        pkg = VIA[pkg]
    return True, seen
asked = set()
for f in ('mkrootfs.sh', 'systools/ci_release.sh', 'sys/ui/build.sh'):
    src = open(f'{ISO}/{f}').read()
    for m in re.finditer(r'pkg-config\s+(?:--[a-z-]+\s+)*([^)\n;|&]*)', src):   # (up to the shell's ; then | & and the ) )
        for tok in m.group(1).split():
            if tok.startswith('-') or '/' in tok: continue
            asked.add('pkg-config ' + tok)
    for l in src.split('\n'):
        if l.lstrip().startswith('gcc'):
            asked.update('-l' + x for x in re.findall(r'(?:^|\s)-l([A-Za-z][A-Za-z0-9_]*)\b', l))
apt = set(x for x in re.search(r'apt-get install -y -qq ([^\n]+)', open(f'{ISO}/../.github/workflows/rudolf-release.yml').read()).group(1).split() if '/' not in x)
bad = [f'{k} is asked for and not in this test\'s table, so its package is not checked' for k in sorted(asked) if k not in DEPS]
for k in sorted(DEPS):
    if k not in asked or not DEPS[k]: continue
    good, chain = installed(DEPS[k])
    if not good:
        bad.append(f'{k} needs {DEPS[k]}' + (f' (through {" -> ".join(chain)})' if len(chain) > 1 else '') + ', which rudolf-release.yml does not install')
ok('8. every pkg-config module and -l library mkrootfs.sh and ci_release.sh ask for is installed by rudolf-release.yml', not bad, '; '.join(bad))

# mkrootfs.sh's own lines, run in a stand-in root
T = tempfile.mkdtemp(); lines = [l for l in mk.split('\n') if 'rudolf-rootd' in l and l.startswith('gcc') or l.startswith('mkdir -p usr/lib/rudolf/root') or (l.startswith('for h in') and 'usr/lib/rudolf/root' in l) or (l.startswith('gcc') and 'usr/lib/rudolf/root/' in l)]
os.makedirs(f'{T}/usr/bin')
r = subprocess.run(['bash', '-c', 'set -e; cd "$1"; SYS="$2"; . "$3"; ' + '\n'.join(lines), '-', T, SYS, f'{ISO}/systools/hardflags.sh'], capture_output=True, text=True)
mode = lambda p: (os.stat(f'{T}/{p}').st_mode & 0o7777) if os.path.exists(f'{T}/{p}') else None
why = []
if r.returncode != 0:
    err = [l.strip() for l in (r.stderr or '').split('\n') if l.strip()]
    first = next((l for l in err if 'not found' in l or 'error:' in l or 'No such file' in l or 'Package ' in l), err[-1] if err else 'no message')
    why.append(f'replaying mkrootfs.sh\'s lines exited {r.returncode}: {first}')
for h in H + I:
    m = mode(f'usr/lib/rudolf/root/{h}')
    if m is None: why.append(f'usr/lib/rudolf/root/{h} was not built')
    elif m != 0o700: why.append(f'usr/lib/rudolf/root/{h} is {m:04o}, not 0700')
for p, want in (('usr/lib/rudolf/root', 0o755), ('usr/bin/rudolf-rootd', 0o755)):
    m = mode(p)
    if m is None: why.append(f'{p} is missing')
    elif m != want: why.append(f'{p} is {m:04o}, not {want:04o}')
ok('6. built by mkrootfs.sh\'s lines: each helper 0700 in usr/lib/rudolf/root (0755), rudolf-rootd 0755, nothing set-user-ID (the layer makes them root\'s: -all-root)', not why, '; '.join(why))
hk = subprocess.run([sys.executable, f'{ISO}/systools/hardcheck.py', f'{T}/usr/bin/rudolf-rootd'] + [f'{T}/usr/lib/rudolf/root/{h}' for h in H + I], capture_output=True, text=True)
hk_out = (hk.stdout + hk.stderr).split('\n')
miss = [l.strip() for l in hk_out if 'built without' in l or 'not an ELF program' in l]
ls_ = [l.strip() for l in hk_out if l.strip()]
first = (ls_[-1] if 'Traceback' in '\n'.join(ls_) else ls_[0]) if ls_ else 'no output'
ok('7. ...with the hardening flags (hardcheck.py)', hk.returncode == 0, '; '.join(miss) or f'hardcheck exited {hk.returncode}: {first}')
shutil.rmtree(T)
print(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if all(R) else 1)
