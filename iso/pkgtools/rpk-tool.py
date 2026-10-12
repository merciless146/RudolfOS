#!/usr/bin/env python3
"""Build tools for Rudolf OS packages (rpk). GPL-2.0-or-later.

  rpk-tool.py keygen KEYDIR                      make a signing key pair (keep the .key file secret)
  rpk-tool.py from-recipe RECIPE.json OUTDIR     lay out a package directory from Debian packages or files
        --debs DIR      folder with the .deb files the recipe names
        --rootfs DIR    Rudolf root file system, to find libraries the package must bring itself
  rpk-tool.py build PKGDIR OUT.rpk --key KEY     squashfs image + signature trailer
  rpk-tool.py index REPODIR --key KEY [--serial N] [--expires-days D]
                                                 write index.txt and index.sig for every .rpk in REPODIR
  rpk-tool.py local-deb PACKAGE OUTDIR           rebuild a .deb from a package installed on this machine
                                                 (for building without network access to a Debian mirror)

Package file layout: [squashfs][64-byte Ed25519 signature][b"RPKSIG01"]; the signature covers
b"rpk1 " + hex SHA-256 of the squashfs part. index.sig is the Ed25519 signature of index.txt."""
import argparse, base64, datetime, glob, hashlib, json, os, re, shutil, subprocess, sys, tempfile
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization

HERE = os.path.dirname(os.path.abspath(__file__))
SQ = os.path.join(HERE, '..', 'src', 'squashfs-tools', 'squashfs-tools')
MKSQ = os.path.join(SQ, 'mksquashfs') if os.path.exists(os.path.join(SQ, 'mksquashfs')) else 'mksquashfs'
UNSQ = os.path.join(SQ, 'unsquashfs') if os.path.exists(os.path.join(SQ, 'unsquashfs')) else 'unsquashfs'
MAGIC = b'RPKSIG01'
NAME = re.compile(r'^[a-z0-9][a-z0-9_-]{0,39}$')
KINDS = ('cli', 'app', 'webapp', 'addon')

def die(m): sys.exit('rpk-tool: ' + m)

def load_key(path):
    raw = base64.b64decode(open(path).read().strip())
    return Ed25519PrivateKey.from_private_bytes(raw)

def keygen(d):
    os.makedirs(d, exist_ok=True)
    k = Ed25519PrivateKey.generate()
    priv = k.private_bytes(serialization.Encoding.Raw, serialization.PrivateFormat.Raw, serialization.NoEncryption())
    pub = k.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
    kp = os.path.join(d, 'rpk-signing.key')
    with open(kp, 'w') as f: f.write(base64.b64encode(priv).decode() + '\n')
    os.chmod(kp, 0o600)
    with open(os.path.join(d, 'rpk-signing.pub'), 'w') as f: f.write(base64.b64encode(pub).decode() + '\n')
    print('public key:', base64.b64encode(pub).decode())

def read_info(text):
    info = {}
    for line in text.splitlines():
        if '=' in line and not line.lstrip().startswith('#'):
            k, v = line.split('=', 1); info[k.strip()] = v.strip()
    return info

def check_info(info, where):
    for k in ('name', 'version', 'kind', 'title', 'summary', 'license'):
        if not info.get(k): die(f'{where}: PKGINFO needs {k}')
    if not NAME.match(info['name']): die(f'{where}: bad name {info["name"]!r}')
    if info['kind'] not in KINDS: die(f'{where}: kind must be one of {KINDS}')
    for k, v in info.items():
        if '\t' in v or '\n' in v: die(f'{where}: {k} has a tab or newline')

def elf(path):
    try:
        with open(path, 'rb') as f: return f.read(4) == b'\x7fELF'
    except OSError: return False

def needed_libs(path):
    out = subprocess.run(['objdump', '-p', path], capture_output=True, text=True).stdout
    return re.findall(r'^\s*NEEDED\s+(\S+)', out, re.M)

def from_recipe(recipe, outdir, debs, rootfs):
    r = json.load(open(recipe)); rdir = os.path.dirname(os.path.abspath(recipe))
    if os.path.exists(outdir): shutil.rmtree(outdir)
    os.makedirs(outdir)
    info = {k: str(r[k]) for k in ('name', 'version', 'kind', 'title', 'summary', 'icon', 'license', 'homepage', 'exec', 'entry') if k in r}
    if r.get('commands'): info['commands'] = ' '.join(r['commands'])
    lic = os.path.join(outdir, 'licenses'); os.makedirs(lic)
    if r.get('debs'):
        ex = tempfile.mkdtemp()
        for d in r['debs']:
            m = glob.glob(os.path.join(debs, f'{d}_*.deb'))
            if not m: die(f'no {d}_*.deb in {debs}')
            subprocess.run(['dpkg-deb', '-x', m[0], ex], check=True)
            cp = os.path.join(ex, 'usr/share/doc', d, 'copyright')
            if os.path.exists(cp): shutil.copy(cp, os.path.join(lic, d + '.copyright'))
        os.makedirs(os.path.join(outdir, 'bin')); os.makedirs(os.path.join(outdir, 'lib'))
        for c in r.get('commands', []):
            src = os.path.join(ex, 'usr/bin', c)
            if not os.path.exists(src): die(f'command {c} is not in the packages')
            shutil.copy2(os.path.realpath(src), os.path.join(outdir, 'bin', c))
        for so in glob.glob(os.path.join(ex, 'usr/lib/x86_64-linux-gnu/*.so*')):
            dst = os.path.join(outdir, 'lib', os.path.basename(so))
            if os.path.islink(so): os.symlink(os.readlink(so), dst)
            else: shutil.copy2(so, dst)
        for src, dst in r.get('keep', {}).items():
            s = os.path.join(ex, src); t = os.path.join(outdir, dst)
            if not os.path.exists(s): die(f'keep: {src} is not in the packages')
            os.makedirs(os.path.dirname(t), exist_ok=True); shutil.copy2(os.path.realpath(s), t)
        shutil.rmtree(ex)
    if r.get('files'):
        shutil.copytree(os.path.join(rdir, r['files']), os.path.join(outdir, os.path.basename(r['files'].rstrip('/'))))
    if r.get('license_file'):
        shutil.copy(os.path.join(rdir, r['license_file']), os.path.join(lic, os.path.basename(r['license_file'])))
    if r.get('env'):
        with open(os.path.join(outdir, 'ENV'), 'w') as f:
            for k, v in r['env'].items(): f.write(f'{k}={v}\n')
    with open(os.path.join(outdir, 'PKGINFO'), 'w') as f:
        for k, v in info.items(): f.write(f'{k} = {v}\n')
    check_info(info, recipe)
    # every library a program needs must come from Rudolf's root file system or from the package itself
    if rootfs and os.path.isdir(os.path.join(outdir, 'lib')):
        have = set(os.listdir(os.path.join(outdir, 'lib')))
        sysl = set()
        for d in ('usr/lib/x86_64-linux-gnu', 'lib64', 'usr/lib'):
            p = os.path.join(rootfs, d)
            if os.path.isdir(p): sysl |= set(os.listdir(p))
        missing = set()
        for top in ('bin', 'lib'):
            for f in os.listdir(os.path.join(outdir, top)):
                p = os.path.join(outdir, top, f)
                if os.path.isfile(p) and not os.path.islink(p) and elf(p):
                    missing |= {n for n in needed_libs(p) if n not in have and n not in sysl}
        if missing: die('libraries neither in the package nor in Rudolf OS: ' + ', '.join(sorted(missing)) + ' (add their Debian packages to the recipe)')
    if info['kind'] == 'cli':
        for c in r.get('commands', []): os.chmod(os.path.join(outdir, 'bin', c), 0o755)
    print(f'laid out {info["name"]} {info["version"]} in {outdir}')

def build(pkgdir, out, key):
    info = read_info(open(os.path.join(pkgdir, 'PKGINFO')).read()); check_info(info, pkgdir)
    tmp = out + '.sfs'
    if os.path.exists(tmp): os.unlink(tmp)
    env = dict(os.environ, SOURCE_DATE_EPOCH='1760000000')
    # 128 KB blocks and fixed times and owners: unchanged files give identical bytes (good for deltas)
    subprocess.run([MKSQ, pkgdir, tmp, '-comp', 'xz', '-b', '128K', '-noappend', '-all-root', '-quiet', '-no-progress', '-no-xattrs'], check=True, env=env)
    h = hashlib.sha256(open(tmp, 'rb').read()).hexdigest()
    sig = load_key(key).sign(b'rpk1 ' + h.encode())
    with open(out, 'wb') as f:
        f.write(open(tmp, 'rb').read()); f.write(sig); f.write(MAGIC)
    os.unlink(tmp)
    print(f'{out}: {info["name"]} {info["version"]}, {os.path.getsize(out)//1024} KB')

def pkg_info(path):
    t = subprocess.run([UNSQ, '-cat', path, 'PKGINFO'], capture_output=True, text=True)
    if t.returncode: die(f'{path}: cannot read PKGINFO')
    return read_info(t.stdout)

def index(repo, key, serial, days):
    rows = []
    for p in sorted(glob.glob(os.path.join(repo, '*.rpk'))):
        i = pkg_info(p); check_info(i, p)
        h = hashlib.sha256(open(p, 'rb').read()).hexdigest()
        rows.append('\t'.join(['P', i['name'], i['version'], i['kind'], str(os.path.getsize(p)), h, os.path.basename(p),
                               i['title'], i['summary'], i.get('icon', ''), i['license']]))
    head = ['rpk-index 1', f'serial {serial}']
    if days: head.append('expires ' + (datetime.date.today() + datetime.timedelta(days=days)).isoformat())
    text = ('\n'.join(head + rows) + '\n').encode()
    open(os.path.join(repo, 'index.txt'), 'wb').write(text)
    open(os.path.join(repo, 'index.sig'), 'wb').write(load_key(key).sign(text))
    print(f'{repo}: {len(rows)} packages, serial {serial}')

def local_deb(pkg, outdir):
    os.makedirs(outdir, exist_ok=True)
    st = subprocess.run(['dpkg-query', '-W', '-f', '${Package}\t${Version}\t${Architecture}', pkg], capture_output=True, text=True, check=True).stdout.split('\t')
    files = subprocess.run(['dpkg', '-L', pkg], capture_output=True, text=True, check=True).stdout.split('\n')
    root = tempfile.mkdtemp(); os.makedirs(os.path.join(root, 'DEBIAN'))
    for f in files:
        if f and os.path.isfile(f) or os.path.islink(f):
            d = os.path.join(root, f.lstrip('/')); os.makedirs(os.path.dirname(d), exist_ok=True)
            if os.path.islink(f): os.symlink(os.readlink(f), d)
            else: shutil.copy2(f, d)
    with open(os.path.join(root, 'DEBIAN', 'control'), 'w') as c:
        c.write(f'Package: {st[0]}\nVersion: {st[1]}\nArchitecture: {st[2]}\nMaintainer: rebuilt locally\nDescription: {st[0]} (rebuilt from the installed files)\n')
    out = os.path.join(outdir, f'{st[0]}_{st[1].split(":")[-1]}_{st[2]}.deb')
    subprocess.run(['dpkg-deb', '--root-owner-group', '-Zxz', '--build', root, out], check=True, capture_output=True)
    shutil.rmtree(root); print(out)

if __name__ == '__main__':
    a = argparse.ArgumentParser(); s = a.add_subparsers(dest='cmd', required=True)
    p = s.add_parser('keygen'); p.add_argument('dir')
    p = s.add_parser('from-recipe'); p.add_argument('recipe'); p.add_argument('outdir'); p.add_argument('--debs', default='.'); p.add_argument('--rootfs')
    p = s.add_parser('build'); p.add_argument('pkgdir'); p.add_argument('out'); p.add_argument('--key', required=True)
    p = s.add_parser('index'); p.add_argument('repo'); p.add_argument('--key', required=True); p.add_argument('--serial', type=int, default=int(datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%d%H'))); p.add_argument('--expires-days', type=int, default=0)
    p = s.add_parser('local-deb'); p.add_argument('package'); p.add_argument('outdir')
    o = a.parse_args()
    if o.cmd == 'keygen': keygen(o.dir)
    elif o.cmd == 'from-recipe': from_recipe(o.recipe, o.outdir, o.debs, o.rootfs)
    elif o.cmd == 'build': build(o.pkgdir, o.out, o.key)
    elif o.cmd == 'index': index(o.repo, o.key, o.serial, o.expires_days)
    elif o.cmd == 'local-deb': local_deb(o.package, o.outdir)
