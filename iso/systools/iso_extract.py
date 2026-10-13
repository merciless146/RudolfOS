#!/usr/bin/env python3
"""Copy files out of a Rudolf OS disc image without mounting it (plain ISO 9660, no extra tools).
GPL-2.0-or-later.

  iso_extract.py IMAGE.iso OUTDIR PATH...      e.g. BOOT/VMLINUZ RUDOLF/BASE.SFS RUDOLF/RELEASE
  iso_extract.py IMAGE.iso OUTDIR --all        every file, with its folders (used to make a new disc from an old one)
Each PATH is written to OUTDIR/<last part of PATH>. Names are matched without case and without ';1'."""
import os, sys

S = 2048

def records(f, lba, size):
    f.seek(lba * S); data = f.read(size); i = 0
    while i < len(data):
        n = data[i]
        if n == 0: i = (i // S + 1) * S; continue
        r = data[i:i + n]; i += n
        nl = r[32]; name = r[33:33 + nl]
        if name in (b'\0', b'\1'): continue
        yield name.decode('ascii', 'replace').split(';')[0].rstrip('.').upper(), int.from_bytes(r[2:6], 'little'), int.from_bytes(r[10:14], 'little'), bool(r[25] & 2)

def find(f, path):
    f.seek(16 * S); pvd = f.read(S)
    if pvd[1:6] != b'CD001': sys.exit('not an ISO 9660 image')
    root = pvd[156:190]; lba, size, d = int.from_bytes(root[2:6], 'little'), int.from_bytes(root[10:14], 'little'), True
    for part in path.upper().strip('/').split('/'):
        for nm, l, sz, isdir in records(f, lba, size):
            if nm == part: lba, size, d = l, sz, isdir; break
        else: sys.exit(f'{path}: not on the disc image')
    return lba, size

def copy(f, lba, size, dest):
    f.seek(lba * S); left = size
    with open(dest, 'wb') as o:
        while left:
            b = f.read(min(left, 1 << 20)); o.write(b); left -= len(b)

def extract_all(f, out):
    f.seek(16 * S); pvd = f.read(S)
    if pvd[1:6] != b'CD001': sys.exit('not an ISO 9660 image')
    root = pvd[156:190]; n = 0
    def walk(lba, size, dest):
        nonlocal n
        os.makedirs(dest, exist_ok=True)
        for nm, l, sz, isdir in list(records(f, lba, size)):
            if isdir: walk(l, sz, os.path.join(dest, nm))
            else: copy(f, l, sz, os.path.join(dest, nm)); n += 1
    walk(int.from_bytes(root[2:6], 'little'), int.from_bytes(root[10:14], 'little'), out)
    print(f'{n} files')

def main():
    if len(sys.argv) < 4: sys.exit(__doc__)
    img, out = sys.argv[1], sys.argv[2]; os.makedirs(out, exist_ok=True)
    with open(img, 'rb') as f:
        if sys.argv[3] == '--all': return extract_all(f, out)
        for p in sys.argv[3:]:
            lba, size = find(f, p); f.seek(lba * S); left = size
            with open(os.path.join(out, p.split('/')[-1]), 'wb') as o:
                while left:
                    b = f.read(min(left, 1 << 20)); o.write(b); left -= len(b)
            print(f'{p}: {size // 1024} KB')

if __name__ == '__main__': main()
