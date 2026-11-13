#!/bin/bash
# 1.5.9 (plan 8.5): Return to official Rudolf OS. From a custom build (Developer mode), or from Recovery mode: Developer
# mode off, the official recovery system into the spare slot, started at the next restart; once it started, the custom
# build's slot is cleared (no rollback into it); "erase" also asks for a factory reset. Throwaway keys made here.
#   bash return59_t.sh
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
cp $T/official.pub $T/keys; DEV=$(cat $T/dev.pub)
slot() { local S=$1 SER=$2 K=$3; rm -rf $T/l $T/out $T/p/os/system/$S; mkdir -p $T/l $T/p/os/system/$S
	for f in base chromium desktop modules; do head -c 100000 /dev/urandom > $T/l/$f.sfs; done; head -c 50000 /dev/urandom > $T/l/vmlinuz
	python3 $I/systools/make_release.py --layers $T/l --kernel $T/l/vmlinuz --out $T/out --key $T/$K.key --serial $SER --version $4 >/dev/null 2>&1 || { echo "make_release failed"; exit 1; }
	cp $T/l/*.sfs $T/l/vmlinuz $T/out/channel/manifest.txt $T/out/channel/manifest.sig $T/p/os/system/$S/
	printf 'serial %s\nversion %s\n' $SER $4 > $T/p/os/system/$S/RELEASE; echo 1 > $T/p/os/system/$S/complete; }
run() { RU_SYSD=$T/p/os/system RU_MEDIA=$T/media RU_CONF=$T/p/os/update.conf RU_KEYS=$T/keys RU_EXTRA=$T/p/etc/extra-keys RU_SLOT=$T/slot RU_CMDLINE=$T/cmd RU_MOUNTS=$T/mounts RU_SYSBLOCK=$T/sysblock $RU "$@" 2>&1; }
st() { sed -n "s/^$1=//p" $T/p/os/system/state; }
setup() { rm -rf $T/p $T/media; mkdir -p $T/p/os/system $T/media/rudolf; : > $T/cmd; : > $T/mounts
	slot a 2026111301 official 1.5.9; printf 'good=a\nprev=\nnext=\ntries=0\n' > $T/p/os/system/state; echo a > $T/slot; run boot-ok >/dev/null
	run developer on "$DEV" https://dev.example.com/beta >/dev/null
	slot b 2026111399 dev 1.5.9-custom; printf 'good=a\nprev=\nnext=b\ntries=1\n' > $T/p/os/system/state; echo b > $T/slot; run boot-ok >/dev/null; }
setup
ok "0. a custom build (slot b) runs; slot a and the recovery system are official" "$([ "$(st good)" = b ] && [ -e $T/p/os/system/r/official ] && [ -e $T/p/etc/extra-keys ] && echo 0)"
o=$(run return-official keep)
ok "1. Return to official: Developer mode off (the key and the address gone)" "$([ ! -e $T/p/etc/extra-keys ] && grep -q '^url=$' $T/p/os/update.conf && echo 0)" "$o"
ok "2. ...the official recovery system put into the spare slot (a), started at the next restart, though the custom build's number is higher" "$([ "$(st next)" = a ] && [ "$(st tries)" = 2 ] && [ "$(st ret)" = 1 ] && grep -q 'serial 2026111301' $T/p/os/system/a/RELEASE && [ $(stat -c %i $T/p/os/system/a/base.sfs) = $(stat -c %i $T/p/os/system/r/base.sfs) ] && echo "$o" | grep -q 'Returning to the official Rudolf OS 1.5.9: it starts the next time the computer restarts, with your accounts, files and apps' && echo 0)" "$o"
ok "3. ...nothing is erased with keep" "$([ ! -e $T/p/.factory-reset ] && echo 0)"
echo a > $T/slot; o=$(run boot-ok)
ok "4. the official system started: kept, and the custom build's slot cleared (no rollback into it)" "$([ "$(st good)" = a ] && [ -z "$(st prev)" ] && [ ! -e $T/p/os/system/b ] && [ -z "$(st ret)" ] && echo "$o" | grep -q 'Cleared the custom build in slot b' && echo "$o" | grep -q 'Back to the official Rudolf OS' && echo 0)" "$o"
j=$(run status | python3 -c "import json,sys;d=json.load(sys.stdin);print(d['signer'],d['developer'])")
ok "5. ...and it says Official, Developer mode off" "$([ "$j" = "official False" ] && echo 0)" "$j"
setup; echo r > $T/slot; echo 'quiet rudolf.recovery=1' > $T/cmd; o=$(run return-official erase)
ok "6. from Recovery mode (the official recovery system running), with erase: the same, and a factory reset asked for" "$([ "$(st next)" = a ] && [ -e $T/p/.factory-reset ] && [ ! -e $T/p/etc/extra-keys ] && echo "$o" | grep -q 'everything erased' && echo 0)" "$o"
setup; echo disc > $T/slot; o=$(run return-official keep)
ok "7. from a USB stick or disc that is not this computer's drive: refused, nothing changed" "$(echo "$o" | grep -q 'Not while Rudolf OS runs from a USB drive' && [ -e $T/p/etc/extra-keys ] && echo 0)" "$o"
setup; rm -rf $T/p/os/system/r; echo b > $T/slot; o=$(run return-official keep)
ok "8. no official recovery system: it says so and gets the newest official release (here no address: it fails, saying so)" "$(echo "$o" | grep -q 'No official recovery system on this computer yet' && [ -z "$(st next)" ] && echo 0)" "$o"
echo "$P/$((P+F)) passed"; [ $F = 0 ]
