#!/usr/bin/env python3
"""Publish a Rudolf OS system release for automatic updates. GPL-2.0-or-later.

  make_release.py --layers DIR --kernel BZIMAGE --out DIR --key KEY --serial N --version V
                  [--chromium VER] [--kernel-version VER] [--notes TEXT] [--urgent]
                  [--expires-days 14] [--file-base URL] [--datafmt FILE]

Writes
  OUT/files/<serial>/{vmlinuz,base.sfs,chromium.sfs,desktop.sfs,modules.sfs} and a .rzs delta control file
  for each (blocks of 4 KB: rolling checksum + first 8 bytes of SHA-256, see sys/rudolf-update.c);
  OUT/channel/manifest.txt and manifest.sig (Ed25519, the key that signs rpk packages).
--file-base puts absolute addresses in the manifest (for example a GitHub release:
https://github.com/OWNER/REPO/releases/download/r<serial>); otherwise files/<serial>/ next to the channel.
The serial must only ever go up: devices refuse a release list older than one they have seen.
1.6.0: the list also says the layout of the data the release keeps and the layouts it reads ("datafmt N", "readsfmt
LO-HI"), from --datafmt (default: iso/datafmt), for Roll back to the last stable version.
1.6.2 (docs/plan-1.6.2.md Part 31): the compressions the release's kernel reads system layers in ("squashfs xz zlib zstd",
from --kernel-config, the kernel's build configuration, or --squashfs, carried from the release whose kernel it reuses)
and each layer's own (rows "C NAME COMPRESSION BLOCKSIZE", read from its squashfs superblock). A layer packed in a way
the kernel cannot read is refused here, as rudolf-update refuses it on the computer; with neither option, the kernel is
taken to read what every kernel before 1.6.2 read (xz and zlib)."""
import argparse, base64, datetime, hashlib, os, shutil, subprocess, sys
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

HERE = os.path.dirname(os.path.abspath(__file__)); ISO = os.path.dirname(HERE)
NAMES = ['vmlinuz', 'base', 'chromium', 'desktop', 'modules']
# 1.6.2 (Part 31): squashfs's compression numbers (the superblock's 16 bits at byte 20) and the kernel's options for them
SQ_COMP = {1: 'zlib', 2: 'lzma', 3: 'lzo', 4: 'xz', 5: 'lz4', 6: 'zstd'}
SQ_OLD = ['xz', 'zlib']   # (what every Rudolf OS kernel before 1.6.2 reads: iso/kconfig.sh)

def sq_layer(p):
    """(compression, block size) from a squashfs image's superblock, or None when it is not one"""
    with open(p, 'rb') as f: h = f.read(96)
    if len(h) < 32 or h[:4] != b'hsqs': return None
    return SQ_COMP.get(int.from_bytes(h[20:22], 'little'), 'unknown'), int.from_bytes(h[12:16], 'little')

def sq_kernel(config, given):
    """what the kernel reads: from its .config (CONFIG_SQUASHFS_ZLIB/LZO/XZ/LZ4/ZSTD=y), else the list given, else None"""
    if config:
        on = {l.split('=')[0] for l in open(config).read().splitlines() if l.endswith('=y')}
        if 'CONFIG_SQUASHFS' not in on: sys.exit(f'{config}: squashfs is not built into this kernel')
        return [c for c in ('xz', 'zstd', 'zlib', 'lz4', 'lzo') if f'CONFIG_SQUASHFS_{c.upper()}' in on]
    w = [c for c in given.replace(',', ' ').split() if c in SQ_COMP.values()]
    return w or None

def tool():
    t = os.path.join(ISO, 'out', 'rudolf-update-host')
    src = os.path.join(ISO, 'sys', 'rudolf-update.c')
    if not os.path.exists(t) or os.path.getmtime(t) < os.path.getmtime(src):
        os.makedirs(os.path.dirname(t), exist_ok=True)
        subprocess.run(['gcc', '-O2', '-o', t, src, '-lcrypto', '-ldl'], check=True)
    return t

def sha(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''): h.update(b)
    return h.hexdigest()

def main():
    a = argparse.ArgumentParser()
    a.add_argument('--layers', required=True); a.add_argument('--kernel', required=True); a.add_argument('--out', required=True)
    a.add_argument('--key', required=True); a.add_argument('--serial', type=int, required=True); a.add_argument('--version', required=True)
    a.add_argument('--chromium', default=''); a.add_argument('--kernel-version', default=''); a.add_argument('--notes', default='')
    a.add_argument('--urgent', action='store_true'); a.add_argument('--expires-days', type=int, default=14); a.add_argument('--file-base', default='')
    a.add_argument('--datafmt', default=os.path.join(ISO, 'datafmt'))
    # 1.6.0 (plan Part 18): the release's own source (a .tar.xz), what Blitzen looks things up in; the row S in the list
    a.add_argument('--source', default='')
    # 1.6.2 (Part 31): what the kernel reads (its build configuration, or the list carried from the release it comes from)
    a.add_argument('--kernel-config', default=''); a.add_argument('--squashfs', default='')
    o = a.parse_args()
    fmt = {}
    for l in open(o.datafmt).read().splitlines():
        k, _, v = l.partition(' ')
        if k in ('datafmt', 'readsfmt'): fmt[k] = v.strip()
    lo, _, hi = fmt.get('readsfmt', '').partition('-')
    if not (fmt.get('datafmt', '').isdigit() and lo.isdigit() and hi.isdigit() and int(lo) <= int(fmt['datafmt']) <= int(hi)):
        sys.exit(f'{o.datafmt}: needs "datafmt N" and "readsfmt LO-HI" with LO <= N <= HI')
    fd = os.path.join(o.out, 'files', str(o.serial)); os.makedirs(fd, exist_ok=True); os.makedirs(os.path.join(o.out, 'channel'), exist_ok=True)
    reads = sq_kernel(o.kernel_config, o.squashfs)
    rz = tool(); rows = []; comps = []
    for n in NAMES:
        src = o.kernel if n == 'vmlinuz' else os.path.join(o.layers, n + '.sfs')
        fn = 'vmlinuz' if n == 'vmlinuz' else n + '.sfs'; dst = os.path.join(fd, fn)
        shutil.copyfile(src, dst)
        subprocess.run([rz, 'rzs-make', dst, dst + '.rzs'], check=True)
        ref = (o.file_base.rstrip('/') + '/' + fn) if o.file_base else f'../files/{o.serial}/{fn}'
        rows.append('\t'.join(['L', n, str(os.path.getsize(dst)), sha(dst), ref]))
        print(f'{n}: {os.path.getsize(dst)//1024} KB')
        c = sq_layer(dst) if n != 'vmlinuz' else None
        if c:
            comps.append('\t'.join(['C', n, c[0], str(c[1])]))
            if c[0] not in (reads or SQ_OLD):
                sys.exit(f'{n}.sfs is packed with {c[0]}, which this release\'s kernel cannot read (it reads {" ".join(reads or SQ_OLD)}'
                         f'{"" if reads else ", as every kernel before 1.6.2: give --kernel-config with its build configuration"}): no release written')
    # the dm-verity root hash of each layer that carries its hash tree (1.2.2; systools/verity.py): the start-up
    # checks every block against it
    sys.path.insert(0, HERE); import verity
    for n in NAMES[1:]:
        try:
            d, root = verity.info(os.path.join(fd, n + '.sfs'), n)
            rows.append('\t'.join(['V', n, str(d), root.hex(), verity.salt_for(n).hex()]))
        except SystemExit as e:
            print(f'{n}: no hash tree ({e}); the start-up cannot check it')
    if o.source:
        fn = f'rudolf-os-{o.serial}-source.tar.xz'; dst = os.path.join(fd, fn); shutil.copyfile(o.source, dst)
        ref = (o.file_base.rstrip('/') + '/' + fn) if o.file_base else f'../files/{o.serial}/{fn}'
        rows.append('\t'.join(['S', 'source', str(os.path.getsize(dst)), sha(dst), ref]))
        print(f'source: {os.path.getsize(dst)//1024} KB')
    today = datetime.date.today()
    for v in (o.version, o.chromium, o.kernel_version, o.notes):
        if '\n' in v or '\t' in v: sys.exit('no tabs or new lines in the release text')
    head = ['rudolf-system 1', f'serial {o.serial}', f'version {o.version}', f'released {today.isoformat()}',
            f'expires {(today + datetime.timedelta(days=o.expires_days)).isoformat()}', f'urgent {1 if o.urgent else 0}',
            f'chromium {o.chromium}', f'kernel {o.kernel_version}', f'notes {o.notes}',
            f"datafmt {fmt['datafmt']}", f"readsfmt {fmt['readsfmt']}"] + ([f"squashfs {' '.join(reads)}"] if reads else [])
    text = ('\n'.join(head + rows + comps) + '\n').encode()
    key = Ed25519PrivateKey.from_private_bytes(base64.b64decode(open(o.key).read().strip()))
    ch = os.path.join(o.out, 'channel')
    open(os.path.join(ch, 'manifest.txt'), 'wb').write(text); open(os.path.join(ch, 'manifest.sig'), 'wb').write(key.sign(text))
    print(f'release {o.serial} ({o.version}) written to {o.out}')

if __name__ == '__main__': main()
