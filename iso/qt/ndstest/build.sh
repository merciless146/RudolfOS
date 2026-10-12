#!/bin/bash
# Build the DS test game (rudolftest.nds) with clang for the DS's two ARM processors.   build.sh OUT.nds
set -e; cd "$(dirname "$0")"
F="-O2 -ffreestanding -nostdlib -fno-builtin -marm -Wno-empty-body"
clang --target=armv5te-none-eabi $F -c arm9.c -o arm9.o && ld.lld -T arm9.ld -o arm9.elf arm9.o && llvm-objcopy -O binary arm9.elf arm9.bin
clang --target=armv4t-none-eabi $F -c arm7.c -o arm7.o && ld.lld -T arm7.ld -o arm7.elf arm7.o && llvm-objcopy -O binary arm7.elf arm7.bin
python3 mkrom.py "$1"; rm -f *.o *.elf *.bin
