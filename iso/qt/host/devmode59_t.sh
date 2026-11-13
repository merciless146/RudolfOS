#!/bin/bash
# 1.5.9 (plan 8.3): Developer mode in rudolf-update: one extra trusted key (a developer's publisher key) and their
# update address, kept on RUDOLFDATA, never the official key; releases signed by it are accepted only while it is on;
# the running system says who signed it (official / custom); the recovery system never takes a custom release.
# Throwaway keys made here, never kept.   bash devmode59_t.sh
set -u; cd "$(dirname "$0")"; I=$(cd ../.. && pwd); T=$(mktemp -d); P=0; F=0; trap 'rm -rf "$T"' EXIT
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
RU=$T/ru; gcc -O2 -w -o $RU $I/sys/rudolf-update.c -lcrypto -ldl || exit 1
python3 - "$T" <<'P'
import sys,base64
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization as s
t=sys.argv[1]
for n in ('official','dev'):
    k=Ed25519PrivateKey.generate()
    open(f'{t}/{n}.key','w').write(base64.b64encode(k.private_bytes(s.Encoding.Raw,s.PrivateFormat.Raw,s.NoEncryption())).decode())
    open(f'{t}/{n}.pub','w').write(base64.b64encode(k.public_key().public_bytes(s.Encoding.Raw,s.PublicFormat.Raw)).decode()+'\n')
P
cp $T/official.pub $T/keys; OFF=$(cat $T/official.pub); DEV=$(cat $T/dev.pub)
mkdir -p $T/media/rudolf $T/sys
slot() { local S=$1 SER=$2 K=$3; rm -rf $T/l $T/out $T/sys/$S; mkdir -p $T/l $T/sys/$S
	for f in base chromium desktop modules; do head -c 100000 /dev/urandom > $T/l/$f.sfs; done; head -c 50000 /dev/urandom > $T/l/vmlinuz
	python3 $I/systools/make_release.py --layers $T/l --kernel $T/l/vmlinuz --out $T/out --key $T/$K.key --serial $SER --version 1.5.9 >/dev/null 2>&1 || { echo "make_release failed"; exit 1; }
	cp $T/l/*.sfs $T/l/vmlinuz $T/out/channel/manifest.txt $T/out/channel/manifest.sig $T/sys/$S/
	printf 'serial %s\nversion 1.5.9\n' $SER > $T/sys/$S/RELEASE; echo 1 > $T/sys/$S/complete; }
run() { RU_SYSD=$T/sys RU_MEDIA=$T/media RU_CONF=$T/conf RU_KEYS=$T/keys RU_EXTRA=$T/extra-keys RU_SLOT=$T/slot RU_CMDLINE=$T/cmd RU_MOUNTS=$T/mounts RU_SYSBLOCK=$T/sysblock $RU "$@" 2>&1; }
: > $T/cmd; : > $T/mounts
slot a 2026111301 official; printf 'good=a\nprev=\nnext=\ntries=0\n' > $T/sys/state; echo a > $T/slot; run boot-ok >/dev/null
j() { run status | python3 -c "import json,sys;d=json.load(sys.stdin);print(d$1)"; }
ok "1. an official system: signed by the official key, Developer mode off, the recovery system official" "$([ "$(j "['signer']")" = official ] && [ "$(j "['developer']")" = False ] && [ "$(j "['recoveryOfficial']")" = True ] && [ "$(j "['signerKey']")" = "$OFF" ] && echo 0)"
o=$(run developer on "$OFF" https://example.com/beta); ok "2. the official key is refused as a developer's key" "$(echo "$o" | grep -q 'that is the official Rudolf OS key' && [ ! -e $T/extra-keys ] && echo 0)" "$o"
o=$(run developer on 'not-a-key' https://example.com/beta); ok "3. something that is not a key: refused" "$(echo "$o" | grep -q 'publisher key' && echo 0)" "$o"
o=$(run developer on "$DEV" http://example.com/beta); ok "4. an address without https: refused" "$(echo "$o" | grep -q 'starts with https' && [ ! -e $T/extra-keys ] && echo 0)" "$o"
slot b 2026111302 dev
o=$(run verify-manifest $T/sys/b/manifest.txt $T/sys/b/manifest.sig); ok "5. before Developer mode, a developer's release is not accepted" "$(echo "$o" | grep -q 'not signed by Rudolf OS' && echo 0)" "$o"
o=$(run developer on "$DEV" https://dev.example.com/rudolf/beta)
ok "6. Developer mode on: the key kept on RUDOLFDATA, the address set" "$(grep -qx "$DEV" $T/extra-keys && grep -q 'url=https://dev.example.com/rudolf/beta' $T/conf && [ "$(j "['developer']")" = True ] && echo 0)" "$o"
o=$(run verify-manifest $T/sys/b/manifest.txt $T/sys/b/manifest.sig); ok "7. ...then a developer's release is accepted" "$(echo "$o" | grep -q 'serial 2026111302' && echo 0)" "$o"
! grep -q "$DEV" $T/keys; ok "8. the system image's key list is never changed" $?
printf 'good=a\nprev=\nnext=b\ntries=1\n' > $T/sys/state; echo b > $T/slot; o=$(run boot-ok)
ok "9. the custom build started: it says Custom, with the developer's key" "$([ "$(j "['signer']")" = custom ] && [ "$(j "['signerKey']")" = "$DEV" ] && echo 0)" "$o"
ok "10. ...and the recovery system stays the official release" "$(grep -q 'release 2026111301' $T/sys/r/official && echo "$o" | grep -q 'not an official release' && echo 0)" "$o"
o=$(run developer off)
ok "11. Developer mode off: the key gone, the official address (none set: the system's own)" "$([ ! -e $T/extra-keys ] && grep -q '^url=$' $T/conf && [ "$(j "['developer']")" = False ] && echo 0)" "$o"
o=$(run verify-manifest $T/sys/b/manifest.txt $T/sys/b/manifest.sig); ok "12. ...and a developer's release is refused again" "$(echo "$o" | grep -q 'not signed by Rudolf OS' && echo 0)" "$o"
echo "$P/$((P+F)) passed"; [ $F = 0 ]
