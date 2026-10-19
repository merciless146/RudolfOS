#!/usr/bin/env python3
"""Make the Rudolf OS kernel bundle (a unified kernel image, UKI) for the UEFI boot menu (Rudolf OS 1.2.2).
GPL-2.0-or-later.

  mkuki.py STUB KERNEL VERSION OUT      (then sign OUT with sbsign)

The bundle is systemd's stub (linuxx64.efi.stub) with the kernel (which already holds its initramfs) and fixed
start-up options added as sections. Under Secure Boot the stub ignores options passed by the boot menu, so the
options cannot be changed from the keyboard; the menu's choice reaches the start-up through the firmware
variable LoaderEntrySelected instead (see initramfs/init). The .sbat section is the stub's plus a Rudolf OS
line, so Rudolf OS can revoke its own old bundles if one ever has to be withdrawn (raise the generation)."""
import os, re, subprocess, sys, tempfile

SBAT_RUDOLF = 'rudolf,1,Rudolf OS,rudolf,{v},https://github.com/merciless146/RudolfOS\n'
CMDLINE = 'rudolf.boot=uefi'


def sections(path):
    out = subprocess.run(['objdump', '-h', path], check=True, capture_output=True, text=True).stdout
    res = []
    for m in re.finditer(r'^\s*\d+\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s', out, re.M):
        res.append((m.group(1), int(m.group(2), 16), int(m.group(3), 16)))
    return res


def main():
    if len(sys.argv) != 5:
        sys.exit(__doc__)
    stub, kernel, ver, out = sys.argv[1:]
    secs = sections(stub)
    end = max(v + s for _, v, s in secs)
    align = lambda x: (x + 0xFFFF) & ~0xFFFF
    stub_sbat = subprocess.run(['objcopy', '-O', 'binary', '--only-section=.sbat', stub, '/dev/stdout'], check=True, capture_output=True).stdout
    stub_sbat = stub_sbat.rstrip(b'\0')
    with tempfile.TemporaryDirectory() as t:
        osrel = os.path.join(t, 'osrel'); open(osrel, 'w').write(f'NAME="Rudolf OS"\nID=rudolf\nVERSION_ID="{ver}"\nPRETTY_NAME="Rudolf OS {ver}"\n')
        cmd = os.path.join(t, 'cmdline'); open(cmd, 'w').write(CMDLINE + '\0')
        sbat = os.path.join(t, 'sbat'); open(sbat, 'wb').write(stub_sbat + (b'' if stub_sbat.endswith(b'\n') else b'\n') + SBAT_RUDOLF.format(v=ver).encode())
        args = ['objcopy', '--remove-section', '.sbat']
        vma = align(end)
        for name, path in (('.osrel', osrel), ('.cmdline', cmd), ('.sbat', sbat), ('.linux', kernel)):
            args += ['--add-section', f'{name}={path}', '--set-section-flags', f'{name}=data,readonly',
                     '--change-section-vma', f'{name}={vma:#x}']
            vma = align(vma + os.path.getsize(path))
        args += [stub, out]
        subprocess.run(args, check=True)
    print('kernel bundle', out, os.path.getsize(out), 'bytes')


if __name__ == '__main__':
    main()
