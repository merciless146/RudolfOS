#!/usr/bin/env python3
"""Publish a Rudolf OS system release for automatic updates. GPL-2.0-or-later.

  make_release.py --layers DIR --kernel BZIMAGE --out DIR --key KEY --serial N --version V
                  [--chromium VER] [--kernel-version VER] [--notes TEXT] [--urgent]
                  [--expires-days 14] [--file-base URL]

Writes
  OUT/files/<serial>/{vmlinuz,base.sfs,chromium.sfs,desktop.sfs,modules.sfs} and a .rzs delta control file
  for each (blocks of 4 KB: rolling checksum + first 8 bytes of SHA-256, see sys/rudolf-update.c);
  OUT/channel/manifest.txt and manifest.sig (Ed25519, the key that signs rpk packages).
--file-base puts absolute addresses in the manifest (for example a GitHub release:
https://github.com/OWNER/REPO/releases/download/r<serial>); otherwise files/<serial>/ next to the channel.
The serial must only ever go up: devices refuse a release list older than one they have seen."""
import argparse, base64, datetime, hashlib, os, shutil, subprocess, sys
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

HERE = os.path.dirname(os.path.abspath(__file__)); ISO = os.path.dirname(HERE)
NAMES = ['vmlinuz', 'base', 'chromium', 'desktop', 'modules']

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
    o = a.parse_args()
    fd = os.path.join(o.out, 'files', str(o.serial)); os.makedirs(fd, exist_ok=True); os.makedirs(os.path.join(o.out, 'channel'), exist_ok=True)
    rz = tool(); rows = []
    for n in NAMES:
        src = o.kernel if n == 'vmlinuz' else os.path.join(o.layers, n + '.sfs')
        fn = 'vmlinuz' if n == 'vmlinuz' else n + '.sfs'; dst = os.path.join(fd, fn)
        shutil.copyfile(src, dst)
        subprocess.run([rz, 'rzs-make', dst, dst + '.rzs'], check=True)
        ref = (o.file_base.rstrip('/') + '/' + fn) if o.file_base else f'../files/{o.serial}/{fn}'
        rows.append('\t'.join(['L', n, str(os.path.getsize(dst)), sha(dst), ref]))
        print(f'{n}: {os.path.getsize(dst)//1024} KB')
    today = datetime.date.today()
    for v in (o.version, o.chromium, o.kernel_version, o.notes):
        if '\n' in v or '\t' in v: sys.exit('no tabs or new lines in the release text')
    head = ['rudolf-system 1', f'serial {o.serial}', f'version {o.version}', f'released {today.isoformat()}',
            f'expires {(today + datetime.timedelta(days=o.expires_days)).isoformat()}', f'urgent {1 if o.urgent else 0}',
            f'chromium {o.chromium}', f'kernel {o.kernel_version}', f'notes {o.notes}']
    text = ('\n'.join(head + rows) + '\n').encode()
    key = Ed25519PrivateKey.from_private_bytes(base64.b64decode(open(o.key).read().strip()))
    ch = os.path.join(o.out, 'channel')
    open(os.path.join(ch, 'manifest.txt'), 'wb').write(text); open(os.path.join(ch, 'manifest.sig'), 'wb').write(key.sign(text))
    print(f'release {o.serial} ({o.version}) written to {o.out}')

if __name__ == '__main__': main()
