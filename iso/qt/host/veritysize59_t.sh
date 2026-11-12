#!/bin/sh
# 1.5.9 (plan 11.3): a system layer of another size than its release list says is reported as such ("modules does
# not match the release list (… MB …)"), not as the device-mapper's "table: Argument list too long" (2026-11-12, the
# test kernel's modules on the Vivobook). A throwaway key signs a list for a small fake layer.   sh veritysize59_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); R=$HERE/../..
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
P=0; N=0; ok() { N=$((N+1)); if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else echo "FAIL $1"; fi; }
gcc -Os -static -o "$W/rv" "$R/sys/rudolf-verity.c" || exit 1
mkdir "$W/d" && head -c 2097152 /dev/urandom > "$W/d/r" && mksquashfs "$W/d" "$W/layer.sfs" -quiet -no-progress -noappend >/dev/null || exit 1
python3 "$R/systools/verity.py" add "$W/layer.sfs" modules > "$W/vline" || exit 1
python3 - "$W" <<'P'
import sys, base64
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization
w = sys.argv[1]; k = Ed25519PrivateKey.generate()
pub = k.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
open(w + '/keys', 'w').write(base64.b64encode(pub).decode() + '\n')
b, root, salt = open(w + '/vline').read().split()
text = ('version 0.0.0\nV\tmodules\t%s\t%s\t%s\n' % (b, root, salt)).encode()
open(w + '/manifest.txt', 'wb').write(text); open(w + '/manifest.sig', 'wb').write(k.sign(text))
P
grep -q "^V	modules	" "$W/manifest.txt"; ok "0. a signed list for the fake layer" $?
head -c 1048576 "$W/layer.sfs" > "$W/short.sfs"
"$W/rv" open "$W/manifest.txt" "$W/manifest.sig" "$W/keys" modules "$W/short.sfs" 2>"$W/e"; r=$?
[ $r = 5 ] && grep -q "modules does not match the release list (1 MB, the list says more than 2 MB)" "$W/e" && ! grep -q "Argument list" "$W/e"; ok "1. a smaller layer: 'does not match the release list', with its size (exit 5)" $?
"$W/rv" open "$W/manifest.txt" "$W/manifest.sig" "$W/keys" modules "$W/layer.sfs" 2>"$W/e2" >/dev/null; r=$?
[ $r != 5 ] && ! grep -q "does not match" "$W/e2"; ok "2. the right layer passes the size check (the device-mapper step follows)" $?
grep -q "verity.err | tail -1" "$R/initramfs/init"; ok "3. the start-up shows the last, plainest line of the reason" $?
echo "$P/$N passed"; [ $P = $N ]
