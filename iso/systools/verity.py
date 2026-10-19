#!/usr/bin/env python3
"""dm-verity hash trees for the Rudolf OS system layers (Rudolf OS 1.2.2). GPL-2.0-or-later.

  verity.py add FILE.sfs NAME     append (or rebuild) the hash tree after the squashfs image
  verity.py info FILE.sfs NAME    print "<data blocks> <root hash> <salt>" (checks the tree in the file)

The image keeps working as a plain squashfs file (squashfs ignores what follows its data). At start-up the
initramfs maps it through dm-verity: data blocks 0..D-1 are the squashfs image (padded to 4 KB), the hash tree
follows at block D. Format: dm-verity version 1 (the one veritysetup makes), SHA-256, 4 KB data and hash
blocks, no superblock; the salt is fixed per layer so that an unchanged layer gives the same bytes. The release
list (manifest.txt, signed) carries "V <name> <data blocks> <root hash> <salt>" for every layer."""
import hashlib, os, struct, sys

B = 4096
PER = B // 32


def salt_for(name):
    return hashlib.sha256(b'rudolf-verity\0' + name.encode()).digest()


def data_blocks(path):
    with open(path, 'rb') as f:
        sb = f.read(96)
    if sb[:4] != b'hsqs':
        raise SystemExit('%s is not a squashfs image' % path)
    used = struct.unpack_from('<Q', sb, 40)[0]
    return (used + B - 1) // B


def tree(path, d, salt):
    """the hash levels (lowest first, each a list of blocks) and the root hash"""
    def h(block):
        return hashlib.sha256(salt + block).digest()
    with open(path, 'rb') as f:
        digests = []
        for _ in range(d):
            b = f.read(B)
            if len(b) < B:
                b = b.ljust(B, b'\0')
            digests.append(h(b))
    levels = []
    while True:
        blocks = [b''.join(digests[i:i + PER]).ljust(B, b'\0') for i in range(0, len(digests), PER)]
        levels.append(blocks)
        if len(blocks) == 1:
            break
        digests = [h(b) for b in blocks]
    return levels, h(levels[-1][0])


def add(path, name):
    d = data_blocks(path)
    with open(path, 'r+b') as f:
        f.truncate(d * B)             # the squashfs image, padded to 4 KB; an earlier tree goes
    levels, root = tree(path, d, salt_for(name))
    with open(path, 'ab') as f:
        for lv in reversed(levels):   # the kernel expects the top level first
            for b in lv:
                f.write(b)
    return d, root


def info(path, name):
    d = data_blocks(path)
    levels, root = tree(path, d, salt_for(name))
    want = b''.join(b for lv in reversed(levels) for b in lv)
    with open(path, 'rb') as f:
        f.seek(d * B)
        have = f.read()
    if have != want:
        raise SystemExit('%s: no hash tree, or not the one its data gives (run verity.py add)' % path)
    return d, root


if __name__ == '__main__':
    if len(sys.argv) != 4 or sys.argv[1] not in ('add', 'info'):
        raise SystemExit(__doc__)
    d, root = (add if sys.argv[1] == 'add' else info)(sys.argv[2], sys.argv[3])
    print(d, root.hex(), salt_for(sys.argv[3]).hex())
