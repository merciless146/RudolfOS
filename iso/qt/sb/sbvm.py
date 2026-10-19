# Rudolf OS 1.2.2 Secure Boot in QEMU: a UEFI computer with Secure Boot on (OVMF with Microsoft's keys, as
# most PCs ship). Helpers for the tests in this folder.
#   SB_PARTS   the files of the draft release sb-parts (OVMF_CODE_4M.secboot.fd, OVMF_VARS_4M.ms.fd, ...)
#   QEMU       the QEMU source tree with build/ (default: ../../src/qemu)
import os, struct, subprocess, sys, time, shutil
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, QT)
from qmp import QMP


def esp_disk(fat_img, out, size_mb=64):
    """a disk with one EFI system partition (MBR, type 0xEF) holding fat_img"""
    data = open(fat_img, 'rb').read()
    start = 2048; secs = (len(data) + 511) // 512
    total = max(size_mb * 2048, start + secs + 2048)
    with open(out, 'wb') as f:
        mbr = bytearray(512)
        struct.pack_into('<BBBBBBBBII', mbr, 446, 0x80, 0, 2, 0, 0xEF, 0xFE, 0xFF, 0xFF, start, secs)
        mbr[510:512] = b'\x55\xAA'
        f.write(mbr); f.seek(start * 512); f.write(data); f.truncate(total * 512)
    return out


class VM:
    def __init__(s, name, parts, drives, secure=True, vars_from=None, mem=3072, extra=()):
        s.name = name; s.dir = os.path.abspath(name); os.makedirs(s.dir, exist_ok=True)
        q = os.environ.get('QEMU') or os.path.join(QT, '..', 'src', 'qemu')
        s.vars = os.path.join(s.dir, 'vars.fd')
        if vars_from:
            if os.path.abspath(vars_from) != s.vars: shutil.copy(vars_from, s.vars)
        elif not os.path.exists(s.vars):
            shutil.copy(os.path.join(parts, 'OVMF_VARS_4M.ms.fd'), s.vars)
        s.qmp_path = f'/tmp/qmp-{name}.sock'; s.ser_path = f'/tmp/ser-{name}.sock'
        for p in (s.qmp_path, s.ser_path):
            try: os.unlink(p)
            except OSError: pass
        code = os.path.join(parts, 'OVMF_CODE_4M.secboot.fd')
        a = [os.path.join(q, 'build', 'qemu-system-x86_64'), '-L', os.path.join(q, 'pc-bios'),
             '-machine', 'q35,smm=on', '-global', 'driver=cfi.pflash01,property=secure,value=on',
             '-drive', f'if=pflash,format=raw,unit=0,file={code},readonly=on',
             '-drive', f'if=pflash,format=raw,unit=1,file={s.vars}',
             '-m', str(mem), '-smp', '2', '-accel', 'tcg,thread=multi', '-vga', 'std', '-display', 'none',
             '-qmp', f'unix:{s.qmp_path},server,nowait', '-serial', f'unix:{s.ser_path},server,nowait',
             '-device', 'e1000,netdev=n0', '-netdev', 'user,id=n0', '-usb', '-device', 'usb-tablet']
        for i, d in enumerate(drives):
            if d.endswith('.iso'):
                a += ['-drive', f'file={d},media=cdrom,readonly=on,if=none,id=cd{i}', '-device', f'ide-cd,drive=cd{i}']
            else:
                a += ['-drive', f'file={d},format=raw,if=none,id=hd{i}', '-device', f'ide-hd,drive=hd{i}']
        a += list(extra)
        s.log = open(os.path.join(s.dir, 'qemu.log'), 'w')
        s.p = subprocess.Popen(a, stdout=s.log, stderr=subprocess.STDOUT)
        for _ in range(50):
            if os.path.exists(s.qmp_path): break
            time.sleep(0.2)
        time.sleep(0.5)
        s.q = QMP(s.qmp_path); s.n = 0

    def shot(s, tag):
        s.n += 1; f = os.path.join(s.dir, f'{s.n:02d}_{tag}.png'); s.q.shot(f); time.sleep(0.6); return f

    def keys(s, *seq, gap=0.4):
        for k in seq:
            s.q.key(*k.split('+')); time.sleep(gap)

    def stop(s):
        try: s.q.cmd('quit')
        except Exception: pass
        try: s.p.wait(10)
        except Exception: s.p.kill()
