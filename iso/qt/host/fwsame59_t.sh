#!/bin/sh
# 1.5.9: linux-firmware dropped qca/nvm_usb_00130200_0105.bin (2026-11-12), the same bytes as _0104 and _0110, and the
# release build stopped. A missing listed file is now taken from another listed file with the same hash; a missing
# file whose bytes are nowhere still stops the build, and a changed file is still refused.   sh fwsame59_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); X=$HERE/../../systools/fwextra.py; W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; P=0; F=0
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1"; fi; }
mkdir -p "$W/fw/qca"; printf 'AAAA' > "$W/fw/qca/a_0104.bin"; printf 'BBBB' > "$W/fw/qca/b.bin"
hA=$(printf 'AAAA' | sha256sum | cut -c1-64); hB=$(printf 'BBBB' | sha256sum | cut -c1-64); hC=$(printf 'CCCC' | sha256sum | cut -c1-64)
printf 'F %s qca/a_0104.bin\nF %s qca/a_0105.bin\nF %s qca/b.bin\nL qca/link.bin b.bin\n' "$hA" "$hA" "$hB" > "$W/list"
python3 "$X" copy "$W/list" "$W/fw" "$W/out" > "$W/log" 2>&1; r=$?
[ $r = 0 ] && [ "$(cat "$W/out/qca/a_0105.bin")" = AAAA ] && grep -q "a_0105.bin is missing .*the same bytes from qca/a_0104.bin" "$W/log"; ok "1. a dropped file is taken from a listed file with the same bytes (said in the log)" $?
[ -L "$W/out/qca/link.bin" ] && [ "$(cat "$W/out/qca/b.bin")" = BBBB ]; ok "2. the other files and links as before" $?
printf 'F %s qca/c.bin\n' "$hC" >> "$W/list"; rm -rf "$W/out"
python3 "$X" copy "$W/list" "$W/fw" "$W/out" > "$W/log" 2>&1; r=$?
[ $r != 0 ] && grep -q "c.bin is missing" "$W/log"; ok "3. a missing file whose bytes are nowhere still stops the build" $?
printf 'F %s qca/a_0104.bin\nF %s qca/a_0105.bin\n' "$hB" "$hA" > "$W/list2"; rm -rf "$W/out"
python3 "$X" copy "$W/list2" "$W/fw" "$W/out" > "$W/log" 2>&1; r=$?
[ $r != 0 ]; ok "4. a file that changed is still refused (and is never the source for another)" $?
echo "$P/$((P+F)) passed"; [ $F = 0 ]
