#!/bin/bash
# Write and sign Blitzen's model list (blitzen.txt + blitzen.txt.sig) for a model file, the way the Debian base is
# published: upload the model file and both list files to the GitHub release "blitzen-1" (or the address in
# /persist/os/blitzen.conf). rudolf-ai refuses a list not signed by a key in /etc/rudolf/rpk-keys.
#   mkblitzen-list.sh MODEL.gguf KEYFILE "MODEL NAME" LICENCE [OUTDIR]
# KEYFILE: the raw base64 Ed25519 key (iso/keys/rpk-signing.key) or a PEM file (the RPK_SIGNING_KEY secret).
# GPL-2.0-or-later.
set -e
M=$1; KEY=$2; NAME=$3; LIC=$4; OUT=${5:-$(dirname "$M")}
[ -f "$M" ] && [ -f "$KEY" ] && [ -n "$NAME" ] && [ -n "$LIC" ] || { echo "usage: $0 MODEL.gguf KEYFILE \"MODEL NAME\" LICENCE [OUTDIR]"; exit 1; }
# BLZ_NOCOPY=1: the model is not uploaded (too big for a release; the list links to it), so do not copy it.
# BLZ_EXTRA: more lines for the list, e.g. "url ..." or "part FILE SIZE SHA256 URL" (one per part of a split model).
mkdir -p "$OUT"; F=$(basename "$M")
[ -n "$BLZ_NOCOPY" ] || [ "$(cd "$(dirname "$M")" && pwd)" = "$(cd "$OUT" && pwd)" ] || cp "$M" "$OUT/$F"
printf 'model %s\nfile %s\nsize %s\nsha256 %s\nlicence %s\nbuilt %s\n' "$NAME" "$F" "$(stat -c %s "$M")" "$(sha256sum "$M" | cut -d' ' -f1)" "$LIC" "$(date -u +%F)" > "$OUT/blitzen.txt"
[ -z "$BLZ_EXTRA" ] || printf '%s\n' "$BLZ_EXTRA" >> "$OUT/blitzen.txt"
python3 - "$KEY" "$OUT/blitzen.txt" <<'PY'
import sys,base64
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives.serialization import load_pem_private_key
raw=open(sys.argv[1],'rb').read()
k=load_pem_private_key(raw,None) if raw.lstrip().startswith(b'-----BEGIN') else Ed25519PrivateKey.from_private_bytes(base64.b64decode(raw.strip()))
open(sys.argv[2]+'.sig','wb').write(k.sign(open(sys.argv[2],'rb').read()))
PY
cat "$OUT/blitzen.txt"; ls -la "$OUT"
