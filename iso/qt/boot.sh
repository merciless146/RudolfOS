#!/bin/bash
# boot.sh NAME MODE(bios|uefi) MEDIA(cd|usb) [extra qemu args]
Q=/tmp/claude-0/-home-claude/dbf93716-fba6-5fcb-93f8-76b4ca76f210/scratchpad/iso/src/qemu
ISO=/tmp/claude-0/-home-claude/dbf93716-fba6-5fcb-93f8-76b4ca76f210/scratchpad/iso/out/rudolf-os-1.1.iso
N=$1; MODE=$2; MEDIA=$3; shift 3
FW="-bios $Q/pc-bios/bios-256k.bin"
if [ "$MODE" = uefi ]; then [ -f ovmf.fd ] || bzcat $Q/pc-bios/edk2-x86_64-code.fd.bz2 > ovmf.fd; FW="-drive if=pflash,format=raw,readonly=on,file=ovmf.fd"; fi
if [ "$MEDIA" = none ]; then DRV=""; elif [ "$MEDIA" = cd ]; then DRV="-drive file=$ISO,media=cdrom,readonly=on -boot d"; else cp $ISO usb-$N.img; truncate -s 2G usb-$N.img; DRV="-drive file=usb-$N.img,format=raw,if=none,id=u -device qemu-xhci -device usb-storage,drive=u,bootindex=0"; fi
exec $Q/build/qemu-system-x86_64 -L $Q/pc-bios -machine pc -m 3072 -smp 2 -accel tcg,thread=multi $FW $DRV -vga std -display none -qmp unix:/tmp/qmp-$N.sock,server,nowait ${SERIAL:--serial file:serial-$N.log} -device e1000,netdev=n0 -netdev hubport,id=n0,hubid=0 -device ich9-intel-hda ${AUDIO:--device hda-duplex} -usb -device usb-tablet "$@" > qemu-$N.log 2>&1
