#!/usr/bin/env python3
# 1.6.1 (docs/plan-1.6.1.md Part 12): is the kernel build reproducible? A checksum of each section of vmlinux and of each module
# with its signature taken off, so two builds from the same tree can be compared line by line (testkernel.sh REPRO=1; the
# workflow kernel-tuning.yml). Each build makes its own module-signing key and builds its certificate into the kernel (in
# .init.data, with the build id in .notes; found by the first comparison, run 44007517463), so those two sections are expected to differ and nothing else.
#   kreprohash.py VMLINUX MODULES-DIR     GPL-2.0-or-later.
import hashlib, os, struct, sys
MAGIC = b'~Module signature appended~\n'
def sections(path):
    d = open(path, 'rb').read()
    assert d[:4] == b'\x7fELF' and d[4] == 2, 'a 64-bit ELF file'
    shoff, = struct.unpack_from('<Q', d, 0x28); shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x3A)
    sh = [struct.unpack_from('<IIQQQQIIQQ', d, shoff + i * shentsize) for i in range(shnum)]
    so = sh[shstrndx][4]
    for name, typ, flags, addr, off, size, *_ in sh:
        n = d[so + name:d.index(b'\0', so + name)].decode()
        if typ == 8: yield n, hashlib.sha256(str(size).encode()).hexdigest()   # (NOBITS: only its size)
        elif n: yield n, hashlib.sha256(d[off:off + size]).hexdigest()
def unsigned(path):
    d = open(path, 'rb').read()
    if d.endswith(MAGIC):
        siglen, = struct.unpack_from('>I', d, len(d) - len(MAGIC) - 4)
        d = d[:len(d) - len(MAGIC) - 12 - siglen]
    return hashlib.sha256(d).hexdigest()
vm, md = sys.argv[1], sys.argv[2]
for n, h in sections(vm): print(f'vmlinux {n} {h}')
for root, _, files in sorted(os.walk(md)):
    for f in sorted(files):
        if f.endswith('.ko'): p = os.path.join(root, f); print(f'module {os.path.relpath(p, md)} {unsigned(p)}')
