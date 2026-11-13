#!/bin/bash
# 1.5.9 (plan 8.4): the official recovery system, slot r. rudolf-update makes it from the running slot when that one
# proved itself (boot-ok), only from a release signed by the official key, only a newer one, by hard links; a layer the
# slot takes from this drive's disc comes from there; never in recovery mode. Throwaway keys made here, never kept.
#   bash recovery59_t.sh
set -u; cd "$(dirname "$0")"; I=$(cd ../.. && pwd); T=$(mktemp -d); P=0; F=0; trap 'rm -rf "$T"' EXIT
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
RU=$T/ru; gcc -O2 -w -o $RU $I/sys/rudolf-update.c -lcrypto -ldl || exit 1
python3 - "$T" <<'P'
import sys,base64
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization as s
t=sys.argv[1]
for n in ('official','other'):
    k=Ed25519PrivateKey.generate()
    open(f'{t}/{n}.key','w').write(base64.b64encode(k.private_bytes(s.Encoding.Raw,s.PrivateFormat.Raw,s.NoEncryption())).decode())
    open(f'{t}/{n}.pub','w').write(base64.b64encode(k.public_key().public_bytes(s.Encoding.Raw,s.PublicFormat.Raw)).decode()+'\n')
P
cp $T/official.pub $T/keys   # the system image's list: the official key only
mkdir -p $T/media/rudolf
# slot SLOT: a release SERIAL signed with KEY; NOCHROM=1: its chromium layer is the disc's (on this drive)
slot() { local S=$1 SER=$2 K=$3; rm -rf $T/l $T/out $T/sys/$S; mkdir -p $T/l $T/sys/$S
	for f in base chromium desktop modules; do head -c 200000 /dev/urandom > $T/l/$f.sfs; done; head -c 100000 /dev/urandom > $T/l/vmlinuz
	python3 $I/systools/make_release.py --layers $T/l --kernel $T/l/vmlinuz --out $T/out --key $T/$K.key --serial $SER --version 1.5.9 >/dev/null 2>&1 || { echo "make_release failed"; exit 1; }
	cp $T/l/*.sfs $T/l/vmlinuz $T/out/channel/manifest.txt $T/out/channel/manifest.sig $T/sys/$S/
	printf 'serial %s\nversion 1.5.9\n' $SER > $T/sys/$S/RELEASE; echo 1 > $T/sys/$S/complete
	[ "${NOCHROM:-}" = 1 ] && mv $T/sys/$S/chromium.sfs $T/media/rudolf/chromium.sfs; return 0; }
run() { RU_SYSD=$T/sys RU_MEDIA=$T/media RU_CONF=$T/conf RU_KEYS=$T/keys RU_SLOT=$T/slot RU_CMDLINE=$T/cmd RU_MOUNTS=$T/mounts RU_SYSBLOCK=$T/sysblock $RU "$@" 2>&1; }
: > $T/cmd; : > $T/mounts; mkdir -p $T/sys
slot a 2026111301 official; printf 'good=a\nprev=\nnext=\ntries=0\n' > $T/sys/state; echo a > $T/slot
o=$(run boot-ok)
ok "1. the good official slot started: the recovery system is made from it" "$([ -e $T/sys/r/complete ] && grep -q 'release 2026111301, from slot a' $T/sys/r/official && grep -q 'serial 2026111301' $T/sys/r/RELEASE && echo 0)" "$o"
ok "2. ...by hard links (no extra room), with its signed release list" "$([ $(stat -c %i $T/sys/r/base.sfs) = $(stat -c %i $T/sys/a/base.sfs) ] && cmp -s $T/sys/r/manifest.sig $T/sys/a/manifest.sig && echo 0)"
o=$(run boot-ok); ok "3. started again: nothing to do (not newer)" "$([ -z "$o" ] && echo 0)" "$o"
slot b 2026111302 other; printf 'good=a\nprev=\nnext=b\ntries=1\n' > $T/sys/state; echo b > $T/slot
o=$(run boot-ok)
ok "4. an update signed by another key proves itself: kept as the system, but never the recovery system" "$(echo "$o" | grep -q 'Keeping the updated system (slot b)' && echo "$o" | grep -q 'not an official release' && grep -q 'release 2026111301' $T/sys/r/official && echo 0)" "$o"
NOCHROM=1 slot a 2026111303 official; printf 'good=b\nprev=\nnext=a\ntries=1\n' > $T/sys/state; echo a > $T/slot
o=$(run boot-ok)
ok "5. a newer official update proves itself: the recovery system follows it" "$(grep -q 'release 2026111303, from slot a' $T/sys/r/official && echo "$o" | grep -q 'Recovery system: Rudolf OS release 2026111303' && echo 0)" "$o"
ok "6. ...its layer that the slot takes from this drive's disc comes from the disc" "$(cmp -s $T/sys/r/chromium.sfs $T/media/rudolf/chromium.sfs && echo 0)"
slot b 2026111300 official; printf 'good=a\nprev=\nnext=b\ntries=1\n' > $T/sys/state; echo b > $T/slot; run boot-ok >/dev/null
ok "7. an older official release (a rollback to it) does not replace a newer recovery system" "$(grep -q 'release 2026111303' $T/sys/r/official && echo 0)"
slot a 2026111304 official; printf 'good=a\nprev=\nnext=\ntries=0\n' > $T/sys/state; echo a > $T/slot; echo 'quiet rudolf.recovery=1' > $T/cmd
o=$(run boot-ok); o2=$(run recovery)
ok "8. in recovery mode: never refreshed (boot-ok, nor by hand)" "$(grep -q 'release 2026111303' $T/sys/r/official && echo "$o2" | grep -q 'Not while in recovery mode' && echo 0)" "$o $o2"
: > $T/cmd; rm $T/sys/a/desktop.sfs; o=$(run recovery)
ok "9. a slot with a file missing: the recovery system kept as it was, said so" "$(grep -q 'release 2026111303' $T/sys/r/official && [ ! -e $T/sys/r.new ] && echo "$o" | grep -q 'could not be refreshed' && echo 0)" "$o"
ok "10. updates never write slot r (the spare slot is a or b only)" "$(grep -q 'const char \*T = strcmp(bs, "a") && strcmp(kv(&st, "good"), "a") ? "a" : strcmp(bs, "b") && strcmp(kv(&st, "good"), "b") ? "b" : NULL;' $I/sys/rudolf-update.c && echo 0)"
echo "$P/$((P+F)) passed"; [ $F = 0 ]
