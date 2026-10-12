#!/usr/bin/env python3
"""Copy only the Wi-Fi firmware the built kernel modules ask for, keeping the disc lean.

    wififw.py MODULES_DIR LINUX_FIRMWARE_DIR DEST_FIRMWARE_DIR

Each Wi-Fi module names its firmware files (MODULE_FIRMWARE). For a name that linux-firmware does not have
in exactly that version (Intel's -NN.ucode, Realtek's _fw-N.bin), the newest older version is taken, as
the driver falls back the same way. Intel .pnvm files that go with the chosen ucode are copied too.
"""
import os, re, sys, shutil

mods, fw, dest = sys.argv[1:4]
links = {}   # name -> target, from WHENCE "Link:" lines
for line in open(os.path.join(fw, 'WHENCE'), errors='replace'):
    m = re.match(r'Link:\s*(\S+)\s*->\s*(\S+)', line)
    if m: links[m.group(1)] = os.path.normpath(os.path.join(os.path.dirname(m.group(1)), m.group(2)))

def find(name):
    for c in (name, links.get(name), 'intel/iwlwifi/' + os.path.basename(name)):
        if c and os.path.isfile(os.path.join(fw, c)): return c
    return None

def older(name):
    m = re.match(r'^(.*?)(\d+)(\.ucode|\.bin)$', name)
    if not m: return None
    base, n, ext = m.group(1), int(m.group(2)), m.group(3)
    for k in range(n - 1, max(n - 40, -1), -1):
        c = find(f'{base}{k}{ext}')
        if c: return f'{base}{k}{ext}', c
    if base.endswith('-'):
        c = find(base[:-1] + ext)
        if c: return base[:-1] + ext, c
    return None

wanted = set()
for root, _, files in os.walk(mods):
    if '/net/wireless/' not in root + '/': continue
    for f in files:
        if not f.endswith('.ko'): continue
        data = open(os.path.join(root, f), 'rb').read()
        wanted.update(x.decode() for x in re.findall(rb'firmware=([^\0]+)\0', data))

copied, missing, size = [], [], 0
def put(name, src):
    global size
    d = os.path.join(dest, name); os.makedirs(os.path.dirname(d), exist_ok=True)
    if not os.path.exists(d):
        shutil.copy(os.path.join(fw, src), d); size += os.path.getsize(d); copied.append(name)
for name in sorted(wanted):
    src = find(name); real = name
    if not src:
        o = older(name)
        if o: real, src = o
    if not src: missing.append(name); continue
    put(real, src)
    if real.startswith('iwlwifi-') and real.endswith('.ucode'):
        p = re.sub(r'-\d+\.ucode$', '.pnvm', real); s = find(p)
        if s: put(p, s)
print(f'wififw: {len(copied)} files, {size // 1024} KB; not in linux-firmware: {len(missing)}')
for m in missing: print('  missing', m)
