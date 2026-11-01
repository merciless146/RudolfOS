#!/bin/bash
# deadcheck.py (1.4.4) on the repository as it is, and on copies with something unused added: each is found.
cd "$(dirname "$0")/../../.."; R=0; n=0; ok(){ n=$((n+1)); if [ "$2" = 0 ]; then echo "PASS $1"; R=$((R+1)); else echo "FAIL $1"; fi; }
python3 iso/systools/deadcheck.py >/dev/null; ok "the repository has nothing unused" $?
T=$(mktemp -d); git archive HEAD | tar -x -C "$T"; cp iso/systools/deadcheck.py "$T/iso/systools/"; cp iso/systools/base-remove.txt iso/mkrootfs.sh "$T/iso/" 2>/dev/null; cp iso/systools/base-remove.txt "$T/iso/systools/"; cp iso/mkrootfs.sh "$T/iso/mkrootfs.sh"
(cd "$T" && git init -q && git add -A >/dev/null)
echo "function zzUnused(){return 1}" > "$T/desktop/zz.js"; python3 "$T/iso/systools/deadcheck.py" | grep -q "zzUnused is defined but never used"; ok "an unused desktop function is found" $?; rm "$T/desktop/zz.js"
echo "static int zz_unused(void) { return 1; }" >> "$T/iso/sys/rudolf-fscheck.c"; python3 "$T/iso/systools/deadcheck.py" | grep -q "zz_unused"; ok "an unused C function is found" $?; git -C "$T" checkout -q iso/sys/rudolf-fscheck.c
mkdir -p "$T/iso/sys/root/usr/lib/rudolf/frames/_metadata"; echo x > "$T/iso/sys/root/usr/lib/rudolf/frames/_metadata/r"; git -C "$T" add -f iso/sys/root/usr/lib/rudolf/frames/_metadata/r; python3 "$T/iso/systools/deadcheck.py" | grep -q "_metadata/r: build output"; ok "build output committed into sys/root is found" $?
echo "usr/bin/flatpak" >> "$T/iso/systools/base-remove.txt"; python3 "$T/iso/systools/deadcheck.py" | grep -q "still copies usr/bin/flatpak"; ok "a removed program that mkrootfs.sh still copies is found" $?
rm -rf "$T"; echo "$R/$n passed"; [ $R = $n ]
