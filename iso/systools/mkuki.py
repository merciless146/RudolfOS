#!/usr/bin/env python3
"""Make the Rudolf OS kernel bundle (a unified kernel image, UKI) for the UEFI boot menu (Rudolf OS 1.2.2).
GPL-2.0-or-later.

  mkuki.py STUB KERNEL VERSION OUT [--recovery KEYFILE]     (then sign OUT with sbsign)

The bundle is systemd's stub (linuxx64.efi.stub) with the kernel (which already holds its initramfs) and fixed
start-up options added as sections. Under Secure Boot the stub ignores options passed by the boot menu, so the
options cannot be changed from the keyboard; the menu's choice reaches the start-up through the firmware
variable LoaderEntrySelected instead (see initramfs/init). The .sbat section is the stub's plus a Rudolf OS
line, so Rudolf OS can revoke its own old bundles if one ever has to be withdrawn (raise the generation).

1.5.9 (docs/plan-1.5.9.md 8.4): --recovery KEYFILE makes the official recovery start-up file (recovery.efi): the
same kernel, and a small second initramfs in the bundle's .initrd section (unpacked over the kernel's own) with
/etc/rudolf-recovery (the mark: the start-up starts only the checked official recovery system, system/r) and
/etc/rudolf-official-key (KEYFILE: the official release key alone, one Ed25519 public key in base64), and
/etc/rudolf-kexec-initrd (an empty initramfs the handover to system/r gives the new kernel, see rudolf-kexec.c)."""
import os, re, subprocess, sys, tempfile

SBAT_RUDOLF = 'rudolf,1,Rudolf OS,rudolf,{v},https://github.com/merciless146/RudolfOS\n'
CMDLINE = 'rudolf.boot=uefi'
RECOVERY_CMDLINE = 'rudolf.boot=uefi rudolf.recovery=1'
# local tests only (never in the workflow): extra fixed options, for example rudolf.debug=1 for the serial shell
TEST_CMDLINE = os.environ.get('RUDOLF_UKI_TEST_CMDLINE', '')
# local tests only (never in the workflow): a cpio archive put in front of the recovery initramfs (a test's own
# start-up script over a released kernel's)
TEST_INITRD = os.environ.get('RUDOLF_UKI_TEST_INITRD', '')


def sections(path):
    out = subprocess.run(['objdump', '-h', path], check=True, capture_output=True, text=True).stdout
    res = []
    for m in re.finditer(r'^\s*\d+\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s', out, re.M):
        res.append((m.group(1), int(m.group(2), 16), int(m.group(3), 16)))
    return res


def cpio(entries):
    """A newc cpio archive of (name, mode, data) entries (data None: a directory), owned by root, time 0."""
    out = b''
    for ino, (name, mode, data) in enumerate(entries + [('TRAILER!!!', 0, b'')], 1):
        body = data or b''
        n = name.encode() + b'\0'
        hdr = b'070701' + b''.join(b'%08X' % v for v in (ino, mode, 0, 0, 1 if data is not None else 2, 0, len(body), 0, 0, 0, 0, len(n), 0))
        rec = hdr + n
        rec += b'\0' * (-len(rec) % 4) + body
        out += rec + b'\0' * (-len(rec) % 4)
    return out + b'\0' * (-len(out) % 512)


def official_key(path):
    """The one key in KEYFILE (comments allowed); refuses anything else (a second key, a private key)."""
    keys = [l.strip() for l in open(path) if l.strip() and not l.startswith('#')]
    if len(keys) != 1 or not re.fullmatch(r'[A-Za-z0-9+/]{43}=', keys[0]):
        sys.exit(f'{path}: the recovery start-up file carries exactly one public key (base64), not {len(keys)} lines')
    return keys[0]


def main():
    a = sys.argv[1:]
    rec = None
    if '--recovery' in a:
        i = a.index('--recovery'); rec = a[i + 1]; del a[i:i + 2]
    if len(a) != 4:
        sys.exit(__doc__)
    stub, kernel, ver, out = a
    secs = sections(stub)
    end = max(v + s for _, v, s in secs)
    align = lambda x: (x + 0xFFFF) & ~0xFFFF
    stub_sbat = subprocess.run(['objcopy', '-O', 'binary', '--only-section=.sbat', stub, '/dev/stdout'], check=True, capture_output=True).stdout
    stub_sbat = stub_sbat.rstrip(b'\0')
    with tempfile.TemporaryDirectory() as t:
        name = f'Rudolf OS {ver}' + (' (recovery)' if rec else '')
        osrel = os.path.join(t, 'osrel'); open(osrel, 'w').write(f'NAME="Rudolf OS"\nID=rudolf\nVERSION_ID="{ver}"\nPRETTY_NAME="{name}"\n')
        cmd = os.path.join(t, 'cmdline'); open(cmd, 'w').write(((RECOVERY_CMDLINE if rec else CMDLINE) + ' ' + TEST_CMDLINE).strip() + '\0')
        sbat = os.path.join(t, 'sbat'); open(sbat, 'wb').write(stub_sbat + (b'' if stub_sbat.endswith(b'\n') else b'\n') + SBAT_RUDOLF.format(v=ver).encode())
        parts = [('.osrel', osrel), ('.cmdline', cmd), ('.sbat', sbat), ('.linux', kernel)]
        if rec:
            ird = os.path.join(t, 'initrd')
            data = open(TEST_INITRD, 'rb').read() if TEST_INITRD else b''
            data += cpio([('etc', 0o40755, None),
                          ('etc/rudolf-recovery', 0o100644, f'official recovery start-up file, Rudolf OS {ver}\n'.encode()),
                          ('etc/rudolf-official-key', 0o100644, (official_key(rec) + '\n').encode()),
                          # (an empty initramfs for the handover to system/r: see rudolf-kexec.c)
                          ('etc/rudolf-kexec-initrd', 0o100644, cpio([]))])
            open(ird, 'wb').write(data)
            parts.append(('.initrd', ird))
        args = ['objcopy', '--remove-section', '.sbat']
        vma = align(end)
        for sec, path in parts:
            args += ['--add-section', f'{sec}={path}', '--set-section-flags', f'{sec}=data,readonly',
                     '--change-section-vma', f'{sec}={vma:#x}']
            vma = align(vma + os.path.getsize(path))
        args += [stub, out]
        subprocess.run(args, check=True)
    print('kernel bundle', out, os.path.getsize(out), 'bytes')


if __name__ == '__main__':
    main()
