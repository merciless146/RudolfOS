#!/usr/bin/env python3
"""Rudolf OS image builder: FAT16 EFI boot image + hybrid ISO9660/El Torito.

    mkimage.py fat  OUT.img  FILE:/PATH/IN/FAT [...]
    mkimage.py iso  OUT.iso  TREE_DIR  --mbr mbr.bin --boot BOOT/RBOOT.BIN
                    --kernel BOOT/VMLINUZ --efi EFI/EFIBOOT.IMG --label RUDOLF_OS [--sums RUDOLF/SHA256]
--sums names the disc's checksum list (sha256sum format, lowercase paths). The boot loader is patched with the
kernel's place on the disc while the image is written, so its line in that list is rewritten to match (1.2.6).

The ISO boots from CD/DVD on BIOS (Rudolf Boot) and UEFI (El Torito EFI
entry), and the same file written to a USB stick boots on BIOS (hybrid MBR
that loads Rudolf Boot) and UEFI (MBR partition of type 0xEF that covers
the FAT image). GPL-2.0-or-later.
"""
import os, sys, struct, time, math, hashlib

SEC = 2048


# ----------------------------------------------------------------- FAT16
def fat_image(out, files, label=b'RUDOLF EFI '):
    """files: list of (host_path, fat_path like 'EFI/BOOT/BOOTX64.EFI')."""
    payload = sum(os.path.getsize(h) for h, _ in files)
    size = max(8 << 20, (payload + (2 << 20) + (1 << 20) - 1) // (1 << 20) * (1 << 20))
    total = size // 512
    spc = 4 if total > 32768 else 1
    while True:
        rootsec = 32
        fatsz = 1
        for _ in range(20):
            clusters = (total - 1 - rootsec - 2 * fatsz) // spc
            fatsz = math.ceil((clusters + 2) * 2 / 512)
        if clusters < 4085:
            spc = max(1, spc // 2); continue
        if clusters > 65524:
            spc *= 2; continue
        break
    data0 = 1 + 2 * fatsz + rootsec
    csize = spc * 512
    fat = [0xFFF8, 0xFFFF]
    img = bytearray(data0 * 512)
    clusters_data = {}
    next_cl = [2]

    def alloc(nbytes):
        n = max(1, (nbytes + csize - 1) // csize)
        first = next_cl[0]
        for i in range(n):
            c = first + i
            while len(fat) <= c:
                fat.append(0)
            fat[c] = c + 1 if i < n - 1 else 0xFFFF
        next_cl[0] += n
        return first, n

    def entry(name83, attr, cl, size):
        t = time.localtime()
        tm = (t.tm_hour << 11) | (t.tm_min << 5) | (t.tm_sec // 2)
        dt = ((t.tm_year - 1980) << 9) | (t.tm_mon << 5) | t.tm_mday
        return struct.pack('<11sBBBHHHHHHHI', name83, attr, 0, 0, tm, dt, dt, 0, tm, dt, cl, size)

    def n83(name):
        if name in ('.', '..'):
            return name.encode().ljust(11)
        b, _, e = name.upper().partition('.')
        assert len(b) <= 8 and len(e) <= 3, name
        return b.encode().ljust(8) + e.encode().ljust(3)

    def is83(name):
        b, dot, e = name.partition('.')
        ok = lambda x, n: 0 < len(x) <= n and all(c.isalnum() or c in '_-~' for c in x)
        return ok(b, 8) and (not dot or (ok(e, 3) and '.' not in e))

    # long names (VFAT): systemd-boot reads loader/loader.conf and loader/entries/*.conf (Rudolf OS 1.2.2)
    def lfn(name, short):
        ck = 0
        for c in short:
            ck = ((ck >> 1) | ((ck & 1) << 7)) + c & 0xFF
        u = list(name.encode('utf-16-le'))
        chars = [u[i] | (u[i + 1] << 8) for i in range(0, len(u), 2)]
        n = (len(chars) + 12) // 13
        chars += [0] + [0xFFFF] * (n * 13 - len(chars) - 1) if len(chars) % 13 else []
        out = []
        for i in range(n, 0, -1):
            ch = chars[(i - 1) * 13:i * 13]
            e = struct.pack('<B5HBBB6HH2H', i | (0x40 if i == n else 0), *ch[0:5], 0x0F, 0, ck, *ch[5:11], 0, *ch[11:13])
            out.append(e)
        return b''.join(out)

    def names(d):
        """(name, short 8.3 bytes, long-name entries) for each entry of a directory"""
        used, res = set(), []
        for name in sorted(d):
            if is83(name):
                sn = n83(name)
                # a name with small letters gets a long-name entry too (1.2.4): without it the firmware shows
                # RUDOLF.CER, and shim's MokManager rejects the key on firmware that does not lowercase ".CER"
                res.append((name, sn, b'' if name == name.upper() else lfn(name, sn))); used.add(sn); continue
            b, _, e = name.upper().rpartition('.') if '.' in name else (name.upper(), '', '')
            b = ''.join(c for c in b if c.isalnum())[:6] or 'FILE'
            e = ''.join(c for c in e if c.isalnum())[:3]
            for k in range(1, 100):
                sn = (b + '~%d' % k).encode().ljust(8)[:8] + e.encode().ljust(3)
                if sn not in used:
                    break
            used.add(sn); res.append((name, sn, lfn(name, sn)))
        return res

    def write_dir(d, self_cl, parent_cl):
        ents = []
        if self_cl is not None:
            ents.append(entry(n83('.'), 0x10, self_cl, 0))
            ents.append(entry(n83('..'), 0x10, parent_cl or 0, 0))
        for name, sn, ln in names(d):
            v = d[name]
            if isinstance(v, dict):
                need = (2 + sum(1 + len(x[2]) // 32 for x in names(v))) * 32
                cl, _ = alloc(need)
                ents.append(ln + entry(sn, 0x10, cl, 0))
                clusters_data[cl] = write_dir(v, cl, self_cl)
            else:
                sz = os.path.getsize(v)
                cl, _ = alloc(sz)
                ents.append(ln + entry(sn, 0x20, cl, sz))
                clusters_data[cl] = ('file', v)
        return b''.join(ents)

    # build a tree
    tree = {}
    for h, p in files:
        parts = p.strip('/').split('/')
        d = tree
        for x in parts[:-1]:
            d = d.setdefault(x, {})
        d[parts[-1]] = h

    root = entry(label, 0x08, 0, 0) + write_dir(tree, None, None)
    assert len(root) <= rootsec * 512
    assert next_cl[0] - 2 <= clusters, 'FAT image too small'
    bs = bytearray(512)
    bs[0:3] = b'\xEB\x3C\x90'
    bs[3:11] = b'RUDOLFOS'
    struct.pack_into('<HBHBHHBHHHII', bs, 11, 512, spc, 1, 2, 512,
                     total if total < 65536 else 0, 0xF8, fatsz, 32, 64, 0,
                     0 if total < 65536 else total)
    struct.pack_into('<BBBI11s8s', bs, 36, 0x80, 0, 0x29, 0x52554446, label, b'FAT16   ')
    bs[510:512] = b'\x55\xAA'
    img[0:512] = bs
    fatb = bytearray(fatsz * 512)
    for i, v in enumerate(fat):
        struct.pack_into('<H', fatb, i * 2, v)
    img[512:512 + len(fatb)] = fatb
    img[512 + len(fatb):512 + 2 * len(fatb)] = fatb
    r0 = (1 + 2 * fatsz) * 512
    img[r0:r0 + len(root)] = root
    with open(out, 'wb') as f:
        f.write(img)
        f.truncate(total * 512)
        for cl, v in clusters_data.items():
            f.seek((data0 + (cl - 2) * spc) * 512)
            if isinstance(v, tuple):
                with open(v[1], 'rb') as src:
                    while True:
                        b = src.read(1 << 20)
                        if not b:
                            break
                        f.write(b)
            else:
                f.write(v)
    return total


# ----------------------------------------------------------------- ISO9660
def both16(v): return struct.pack('<H', v) + struct.pack('>H', v)
def both32(v): return struct.pack('<I', v) + struct.pack('>I', v)


def dtime(t):
    return struct.pack('7B', t.tm_year - 1900, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, 0)


def vtime(t):
    return ('%04d%02d%02d%02d%02d%02d00' % (t.tm_year, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec)).encode() + b'\0'


def iso_name(n, isdir):
    u = n.upper()
    ok = set('ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.')
    assert set(u) <= ok, 'bad ISO name ' + n
    if isdir:
        assert '.' not in u and len(u) <= 31, n
        return u.encode()
    b, _, e = u.partition('.')
    assert len(b) <= 30 and len(e) <= 3, n
    return (b + '.' + e + ';1').encode()


class Node:
    def __init__(s, name, path, isdir, parent):
        s.name, s.path, s.isdir, s.parent = name, path, isdir, parent
        s.children = []
        s.lba = 0
        s.size = os.path.getsize(path) if not isdir else 0


def dir_record(lba, size, isdir, ident, t):
    ln = 33 + len(ident)
    pad = 1 if ln % 2 else 0
    r = struct.pack('<BB', ln + pad, 0) + both32(lba) + both32(size) + dtime(t) + \
        struct.pack('<BBB', 2 if isdir else 0, 0, 0) + both16(1) + struct.pack('<B', len(ident)) + ident
    return r + b'\0' * pad


def patch_boot(data, kern):
    b = bytearray(data)
    j = b.find(b'RBTB')
    assert j >= 0
    struct.pack_into('<II', b, j + 4, kern.lba, kern.size)
    return bytes(b)


def build_iso(out, tree, mbr, boot_rel, kernel_rel, efi_rel, label, sums_rel=None):
    t = time.gmtime()
    root = Node('', tree, True, None)
    nodes = {}

    def scan(n):
        for name in sorted(os.listdir(n.path), key=str.upper):
            p = os.path.join(n.path, name)
            c = Node(name, p, os.path.isdir(p), n)
            n.children.append(c)
            rel = os.path.relpath(p, tree).replace(os.sep, '/').upper()
            nodes[rel] = c
            if c.isdir:
                scan(c)
    scan(root)
    dirs = []
    q = [root]
    while q:  # breadth first
        d = q.pop(0)
        dirs.append(d)
        q += [c for c in d.children if c.isdir]

    def dir_bytes(d):
        recs = [dir_record(0, 0, True, b'\0', t), dir_record(0, 0, True, b'\1', t)]
        for c in sorted(d.children, key=lambda c: iso_name(c.name, c.isdir)):
            recs.append(dir_record(0, 0, c.isdir, iso_name(c.name, c.isdir), t))
        # sector packing: records may not cross a 2048 boundary
        size = 0
        for r in recs:
            if size % SEC + len(r) > SEC:
                size += SEC - size % SEC
            size += len(r)
        return (size + SEC - 1) // SEC * SEC

    lba = 16 + 3  # PVD, boot record, terminator
    cat_lba = lba; lba += 1
    # path tables
    def path_table(big):
        num = {id(d): i + 1 for i, d in enumerate(dirs)}
        out = b''
        for d in dirs:
            ident = b'\0' if d is root else iso_name(d.name, True)
            par = num[id(d.parent)] if d.parent else 1
            e = struct.pack('<BB', len(ident), 0) + (struct.pack('>I', d.lba) if big else struct.pack('<I', d.lba)) + \
                (struct.pack('>H', par) if big else struct.pack('<H', par)) + ident
            if len(ident) % 2:
                e += b'\0'
            out += e
        return out
    pt_size = len(path_table(False))
    pt_secs = (pt_size + SEC - 1) // SEC
    lpt = lba; lba += pt_secs
    mpt = lba; lba += pt_secs
    for d in dirs:
        d.size = dir_bytes(d)
        d.lba = lba
        lba += d.size // SEC
    # files: boot loader first, kernel, EFI image, then the rest
    files = [n for n in nodes.values() if not n.isdir]
    pri = {boot_rel.upper(): 0, kernel_rel.upper(): 1, efi_rel.upper(): 2}
    files.sort(key=lambda n: (pri.get(os.path.relpath(n.path, tree).replace(os.sep, '/').upper(), 9), n.path))
    for f in files:
        f.lba = lba
        lba += max(1, (f.size + SEC - 1) // SEC)
    total = lba
    # pad to a whole MiB (USB friendly)
    total = (total * SEC + (1 << 20) - 1) // (1 << 20) * (1 << 20) // SEC

    boot = nodes[boot_rel.upper()]
    kern = nodes[kernel_rel.upper()]
    efi = nodes[efi_rel.upper()]
    if sums_rel:   # the boot loader's line in the checksum list: as the file will be on the disc
        sums = nodes[sums_rel.upper()].path
        bl = boot_rel.lower().strip('/')
        h = hashlib.sha256(patch_boot(open(boot.path, 'rb').read(), kern)).hexdigest()
        old = open(sums, 'rb').read(); lines = old.split(b'\n'); hit = 0
        for i, ln in enumerate(lines):
            if ln.endswith(b'  ' + bl.encode()):
                lines[i] = h.encode() + b'  ' + bl.encode(); hit += 1
        new = b'\n'.join(lines)
        assert hit == 1 and len(new) == len(old), 'the checksum list has no single line for ' + bl
        open(sums, 'wb').write(new)

    with open(out, 'wb') as f:
        f.truncate(total * SEC)
        # ---- system area: hybrid MBR
        m = bytearray(open(mbr, 'rb').read().ljust(512, b'\0'))
        i = m.find(b'RBMB')
        assert i > 0
        struct.pack_into('<I', m, i + 4 + 8, boot.lba * 4)
        struct.pack_into('<I', m, 0x1B8, int(time.time()) & 0xFFFFFFFF)  # disk signature

        def part(slot, boot_flag, ptype, start, count):
            o = 0x1BE + 16 * slot
            m[o:o + 16] = struct.pack('<B3sB3sII', boot_flag, b'\xFE\xFF\xFF', ptype, b'\xFE\xFF\xFF', start, count)
        part(0, 0x80, 0x17, 0, total * 4)
        efisec = (efi.size + 511) // 512
        part(1, 0x00, 0xEF, efi.lba * 4, efisec)
        m[510:512] = b'\x55\xAA'
        f.seek(0); f.write(m)
        # ---- volume descriptors
        rootrec = dir_record(root.lba, root.size, True, b'\0', t)
        pvd = bytearray(SEC)
        pvd[0] = 1; pvd[1:6] = b'CD001'; pvd[6] = 1
        pvd[8:40] = b'LINUX'.ljust(32)
        pvd[40:72] = label.encode().ljust(32)
        pvd[80:88] = both32(total)
        pvd[120:124] = both16(1); pvd[124:128] = both16(1); pvd[128:132] = both16(SEC)
        pvd[132:140] = both32(pt_size)
        struct.pack_into('<I', pvd, 140, lpt); struct.pack_into('>I', pvd, 148, mpt)
        pvd[156:190] = rootrec
        pvd[190:318] = b'RUDOLF_OS'.ljust(128)
        pvd[318:446] = b'RUDOLF OS CONTRIBUTORS'.ljust(128)
        pvd[446:574] = b'RUDOLF OS IMAGE BUILDER'.ljust(128)
        pvd[574:702] = b'RUDOLF OS 25 - GPL-2.0-OR-LATER'.ljust(128)
        for o in (702, 739, 776):
            pvd[o:o + 37] = b' ' * 37
        pvd[813:830] = vtime(t); pvd[830:847] = vtime(t)
        pvd[847:864] = b'0' * 16 + b'\0'; pvd[864:881] = vtime(t)
        pvd[881] = 1
        f.seek(16 * SEC); f.write(pvd)
        br = bytearray(SEC)
        br[0] = 0; br[1:6] = b'CD001'; br[6] = 1
        br[7:39] = b'EL TORITO SPECIFICATION'.ljust(32, b'\0')
        struct.pack_into('<I', br, 0x47, cat_lba)
        f.seek(17 * SEC); f.write(br)
        term = bytearray(SEC); term[0] = 255; term[1:6] = b'CD001'; term[6] = 1
        f.seek(18 * SEC); f.write(term)
        # ---- boot catalog
        val = bytearray(32)
        val[0] = 1; val[1] = 0
        val[4:28] = b'RUDOLF OS'.ljust(24, b'\0')
        val[30] = 0x55; val[31] = 0xAA
        s = sum(struct.unpack('<16H', bytes(val))) & 0xFFFF
        struct.pack_into('<H', val, 28, (-s) & 0xFFFF)
        bios = struct.pack('<BBHBBHI', 0x88, 0, 0, 0, 0, 32, boot.lba).ljust(32, b'\0')
        sech = struct.pack('<BBH', 0x91, 0xEF, 1) + b'RUDOLF OS UEFI'.ljust(28, b'\0')
        efie = struct.pack('<BBHBBHI', 0x88, 0, 0, 0, 0, min(efisec, 0xFFFF), efi.lba).ljust(32, b'\0')
        f.seek(cat_lba * SEC); f.write(bytes(val) + bios + sech + efie)
        # ---- path tables
        f.seek(lpt * SEC); f.write(path_table(False))
        f.seek(mpt * SEC); f.write(path_table(True))
        # ---- directories
        for d in dirs:
            par = d.parent or d
            recs = [dir_record(d.lba, d.size, True, b'\0', t), dir_record(par.lba, par.size, True, b'\1', t)]
            for c in sorted(d.children, key=lambda c: iso_name(c.name, c.isdir)):
                recs.append(dir_record(c.lba, c.size, c.isdir, iso_name(c.name, c.isdir), t))
            buf = bytearray()
            for r in recs:
                if len(buf) % SEC + len(r) > SEC:
                    buf += b'\0' * (SEC - len(buf) % SEC)
                buf += r
            f.seek(d.lba * SEC); f.write(buf)
        # ---- files
        for n in files:
            f.seek(n.lba * SEC)
            with open(n.path, 'rb') as src:
                data = src.read() if n is boot else None
                if n is boot:
                    f.write(patch_boot(data, kern))
                else:
                    while True:
                        b = src.read(4 << 20)
                        if not b:
                            break
                        f.write(b)
    print('ISO %s: %.1f MiB, boot %d, kernel %d (%d bytes), EFI image %d (%d sectors)' %
          (out, total * SEC / 1048576, boot.lba, kern.lba, kern.size, efi.lba, efisec))


if __name__ == '__main__':
    a = sys.argv[1:]
    if a[0] == 'fat':
        files = [tuple(x.split(':', 1)) for x in a[2:]]
        n = fat_image(a[1], files)
        print('FAT image', a[1], n, 'sectors')
    elif a[0] == 'iso':
        opt = dict(zip(a[3::2], a[4::2]))
        build_iso(a[1], a[2], opt['--mbr'], opt['--boot'], opt['--kernel'], opt['--efi'], opt.get('--label', 'RUDOLF_OS'), opt.get('--sums'))
