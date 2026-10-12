#!/bin/bash
# Test disk for the Emu phase (upd_t.py E): homebrew games only (no commercial ROMs, no Nintendo firmware).
#   SNES: Peter Lemon's test programs (github.com/PeterLemon/SNES, cloned into src/hb-snes)
#   DS:   qt/ndstest (Rudolf OS's own test program: colours for buttons, a white square where touched)
#   mkgames.sh OUT.img
set -e
I=$(cd "$(dirname "$0")/.." && pwd); D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
[ -d $I/src/hb-snes ] || git clone -q --depth 1 --filter=blob:none https://github.com/PeterLemon/SNES $I/src/hb-snes
[ -f /tmp/rudolftest.nds ] || bash $I/qt/ndstest/build.sh
cp $I/src/hb-snes/HelloWorld/HelloWorld.sfc $I/src/hb-snes/Games/MonsterFarmJump/MonsterFarmJump.sfc /tmp/rudolftest.nds $D/
(cd $I/src/hb-snes/CPUTest/CPU/ADC && zip -q $D/cputests.zip CPUADC.sfc)
rm -f "$1"; truncate -s 16M "$1"; mkfs.ext4 -q -F -d $D "$1"; ls -la "$1"
