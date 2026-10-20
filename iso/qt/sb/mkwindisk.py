#!/usr/bin/env python3
"""A stand-in Windows disk for the boot-menu test (Rudolf OS 1.2.5): a GPT disk with one EFI system partition
holding \\EFI\\Microsoft\\Boot\\bootmgfw.efi (winstub.c), and the firmware boot entry that points at it, as Windows
Setup would add it ("Windows Boot Manager", a short-form HD() device path).
   mkwindisk.py BOOTMGFW.EFI OUT.img      writes OUT.img; the boot entry is in the partition as RUDOLF/BOOTOPT.BIN
GPL-2.0-or-later."""
import os, sys, struct, uuid, zlib, tempfile
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
import mkimage

ESP_TYPE = uuid.UUID('c12a7328-f81f-11d2-ba4b-00a0c93ec93b')
PART = uuid.UUID('5ee1f5a8-2b6c-4c0e-9d3a-125000000001')   # fixed, so the boot entry matches
DISK = uuid.UUID('5ee1f5a8-2b6c-4c0e-9d3a-125000000000')
START = 2048


def load_option(start, sectors):
    path = '\\EFI\\Microsoft\\Boot\\bootmgfw.efi'.encode('utf-16-le') + b'\0\0'
    hd = struct.pack('<BBH', 4, 1, 42) + struct.pack('<IQQ', 1, start, sectors) + PART.bytes_le + bytes([2, 2])
    fp = struct.pack('<BBH', 4, 4, 4 + len(path)) + path
    dp = hd + fp + bytes([0x7f, 0xff, 4, 0])
    return struct.pack('<IH', 1, len(dp)) + 'Windows Boot Manager'.encode('utf-16-le') + b'\0\0' + dp


def gpt(out, fat):
    data = open(fat, 'rb').read(); n = (len(data) + 511) // 512
    total = START + n + 2048
    ent = ESP_TYPE.bytes_le + PART.bytes_le + struct.pack('<QQQ', START, START + n - 1, 0) + 'EFI system partition'.encode('utf-16-le').ljust(72, b'\0')
    table = ent.ljust(128 * 128, b'\0')
    def header(cur, alt, tbl):
        h = struct.pack('<8sIIIIQQQQ16sQIII', b'EFI PART', 0x10000, 92, 0, 0, cur, alt, 34, total - 34, DISK.bytes_le, tbl, 128, 128, zlib.crc32(table))
        return h[:16] + struct.pack('<I', zlib.crc32(h)) + h[20:]
    mbr = bytearray(512); mbr[446:462] = bytes([0, 0, 2, 0, 0xee, 0xff, 0xff, 0xff]) + struct.pack('<II', 1, min(total - 1, 0xffffffff)); mbr[510:] = b'\x55\xaa'
    with open(out, 'wb') as f:
        f.write(mbr); f.write(header(1, total - 1, 2).ljust(512, b'\0')); f.write(table)
        f.seek(START * 512); f.write(data.ljust(n * 512, b'\0'))
        f.seek((total - 33) * 512); f.write(table); f.write(header(total - 1, 1, total - 33).ljust(512, b'\0'))
    return n


if __name__ == '__main__':
    efi, out = sys.argv[1], sys.argv[2]
    d = tempfile.mkdtemp(); opt = os.path.join(d, 'bootopt.bin'); fat = os.path.join(d, 'esp.img')
    open(opt, 'wb').write(b'\0' * 64)   # sized now, written once the partition size is known
    sectors = mkimage.fat_image(fat, [(efi, 'EFI/Microsoft/Boot/bootmgfw.efi'), (opt, 'RUDOLF/BOOTOPT.BIN')], label=b'WINSTANDIN ')
    n = (os.path.getsize(fat) + 511) // 512
    open(opt, 'wb').write(load_option(START, n))
    os.remove(fat); mkimage.fat_image(fat, [(efi, 'EFI/Microsoft/Boot/bootmgfw.efi'), (opt, 'RUDOLF/BOOTOPT.BIN')], label=b'WINSTANDIN ')
    assert (os.path.getsize(fat) + 511) // 512 == n
    gpt(out, fat); print(out, os.path.getsize(out), 'bytes; boot entry', len(load_option(START, n)), 'bytes')
