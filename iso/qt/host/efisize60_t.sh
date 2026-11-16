#!/bin/sh
# 1.6.0 (docs/plan-1.6.0.md Part 15): the EFI image's size. An El Torito entry names its image in 16 bits of 512-byte
# sectors (at most 32 MiB less 512 bytes), and the EFI image holds two kernels since 1.5.9 (rudolf.efi, recovery.efi).
# mkimage.py stops above that (it cut the entry short before), warns from 30 MiB, and prints the size. With BOOTISO
# (a release's boot.iso), its EFI entry is measured too.   sh efisize60_t.sh
command -v nasm >/dev/null || { echo "SKIP: needs nasm"; exit 0; }
HERE=$(cd "$(dirname "$0")" && pwd); I=$HERE/../..; T=$(mktemp -d); trap 'rm -rf $T' EXIT
P=0; F=0; ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 - $3"; fi; }
mkdir -p $T/t/BOOT $T/t/EFI; nasm -f bin -o $T/t/BOOT/RBOOT.BIN $I/boot/rboot.asm && nasm -f bin -o $T/mbr.bin $I/boot/mbr.asm || exit 1
head -c 300000 /dev/urandom > $T/t/BOOT/VMLINUZ
mk() { truncate -s $1 $T/t/EFI/EFIBOOT.IMG; python3 $I/mkimage.py iso $T/a.iso $T/t --mbr $T/mbr.bin --boot BOOT/RBOOT.BIN --kernel BOOT/VMLINUZ --efi EFI/EFIBOOT.IMG --label T 2>&1; }
# the El Torito catalogue's EFI entry: its sector count
ent() { python3 - "$1" <<'P'
import sys, struct
f = open(sys.argv[1], 'rb'); f.seek(17 * 2048); bv = f.read(2048); f.seek(struct.unpack('<I', bv[0x47:0x4b])[0] * 2048); c = f.read(2048)
efi = [struct.unpack('<H', c[i + 6:i + 8])[0] for i in range(32, 256, 32) if c[i] == 0x88 and c[i - 32] == 0x91]
print(efi[0] if efi else 'none')
P
}
o=$(mk 28M); r=$?; ok "1. a 28 MiB EFI image: made, its size said, the El Torito entry names all of it" "$([ $r = 0 ] && echo "$o" | grep -q '57344 sectors, 28.0 of 32 MiB' && [ "$(ent $T/a.iso)" = 57344 ] && echo 0)" "$o"
o=$(mk 31M); r=$?; ok "2. 31 MiB: made, with a warning (near the limit)" "$([ $r = 0 ] && echo "$o" | grep -q 'WARNING: the EFI image is 31.0 MiB' && [ "$(ent $T/a.iso)" = 63488 ] && echo 0)" "$o"
o=$(mk 33M); r=$?; ok "3. 33 MiB: refused, saying why (before, the entry was cut to 65535 sectors without a word)" "$([ $r != 0 ] && echo "$o" | grep -q 'more than the 32 MiB an El Torito entry can name' && echo 0)" "$o"
if [ -f "$BOOTISO" ]; then n=$(ent "$BOOTISO"); echo "the release's EFI image: $n sectors, $(awk "BEGIN{printf \"%.1f\", $n*512/1048576}") MiB of 32 MiB"; fi
echo "$P/$((P+F)) passed"; [ $F = 0 ]
