#!/usr/bin/env python3
"""Firmware that goes in the modules layer, next to the kernel that asks for it (Rudolf OS 1.2.3). GPL-2.0-or-later.

  fwextra.py list LINUX_FIRMWARE > firmware-extra.txt     write the list from a linux-firmware checkout
  fwextra.py copy LIST LINUX_FIRMWARE DEST                 copy the listed files (checked) and make the links

The list (iso/firmware-extra.txt) pins every file by SHA-256, so a release always carries the same bytes wherever
linux-firmware was fetched from; "L path target" lines are the links linux-firmware's WHENCE describes (its
install script makes them; a git checkout has none). What is listed:
  - NVIDIA GSP firmware (570.144) for GeForce 20 series and newer. Without it nouveau runs these cards with no
    acceleration at all (no 3D, so Xorg has no glamor and everything is drawn by the processor). The booter
    files already sit in the base layer; these are the large GSP images and their boot loaders.
  - Realtek Ethernet (r8169) firmware: small fixes the driver loads for most of its chips.
"""
import hashlib, os, re, shutil, sys

PATTERNS = [r'rtl_nic/.+',
            r'nvidia/[^/]+/gsp/(gsp|bootloader|gen_bootloader|scrubber|fmc)-570\.144\.bin',
            r'nvidia/[^/]+/gsp', r'nvidia/ad10[3-7]', r'nvidia/gb102', r'nvidia/gb20[3-7]']   # whole-folder links (ga104/gsp -> ../ga102/gsp, ad104 -> ad102, gb203 -> gb202)


def want(p):
    return any(re.fullmatch(x, p) for x in PATTERNS)


def whence(fw):
    files, links = [], []
    for line in open(os.path.join(fw, 'WHENCE'), errors='replace'):
        m = re.match(r'(File|RawFile):\s*(\S+)', line)
        if m and want(m.group(2)):
            files.append(m.group(2))
        m = re.match(r'Link:\s*(\S+)\s*->\s*(\S+)', line)
        if m and want(m.group(1)):
            links.append((m.group(1), m.group(2)))
    return files, links


def sha(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()


def cmd_list(fw):
    files, links = whence(fw)
    print('# firmware in the modules layer (iso/systools/fwextra.py); made by: fwextra.py list LINUX_FIRMWARE')
    for f in files:
        print('F', sha(os.path.join(fw, f)), f)
    for a, b in links:
        print('L', a, b)


def cmd_copy(lst, fw, dest):
    n = size = 0
    for line in open(lst):
        t = line.split()
        if not t or t[0].startswith('#'):
            continue
        if t[0] == 'F':
            h, f = t[1], t[2]
            src = os.path.join(fw, f)
            if not os.path.isfile(src):
                sys.exit('fwextra: %s is missing from linux-firmware' % f)
            if sha(src) != h:
                sys.exit('fwextra: %s is not the listed file (linux-firmware changed it; run fwextra.py list again)' % f)
            d = os.path.join(dest, f); os.makedirs(os.path.dirname(d), exist_ok=True)
            shutil.copyfile(src, d); os.chmod(d, 0o644); n += 1; size += os.path.getsize(d)
        elif t[0] == 'L':
            d = os.path.join(dest, t[1]); os.makedirs(os.path.dirname(d), exist_ok=True)
            if os.path.lexists(d):
                os.remove(d)
            os.symlink(t[2], d)
    print('fwextra: %d files, %.1f MB' % (n, size / 1048576))


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == 'list':
        cmd_list(sys.argv[2])
    elif len(sys.argv) == 5 and sys.argv[1] == 'copy':
        cmd_copy(*sys.argv[2:])
    else:
        sys.exit(__doc__)
