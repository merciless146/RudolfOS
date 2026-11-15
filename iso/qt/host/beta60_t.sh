#!/bin/bash
# 1.6.0 (owner, 2026-11-15): beta versions are opt-in. "Get beta versions" (rudolf-update set channel beta, after
# rudolfd checks an administrator's name and password) sets the official beta address; turned off (set channel
# stable), the next update brings the latest stable release even though its number is lower than the beta's. A stand-in
# official server here with a stable and a beta channel (throwaway key made here, never kept).   bash beta60_t.sh
set -u; cd "$(dirname "$0")"; I=$(cd ../.. && pwd); T=$(mktemp -d); P=0; F=0
SRV=""; trap '[ -n "$SRV" ] && kill $SRV 2>/dev/null; rm -rf "$T"' EXIT
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
RU=$T/ru; gcc -O2 -w -o $RU $I/sys/rudolf-update.c -lcrypto -ldl || exit 1
python3 - "$T" <<'P'
import sys,base64
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization as s
t=sys.argv[1];k=Ed25519PrivateKey.generate()
open(t+'/test.key','w').write(base64.b64encode(k.private_bytes(s.Encoding.Raw,s.PrivateFormat.Raw,s.NoEncryption())).decode())
open(t+'/keys','w').write(base64.b64encode(k.public_key().public_bytes(s.Encoding.Raw,s.PublicFormat.Raw)).decode()+'\n')
P
rel() {   # SERIAL VERSION CHANNEL: a release, its list in srv/RudolfOS/CHANNEL and its files in srv/RudolfOS/files
	rm -rf $T/l $T/o; mkdir -p $T/l; for f in base chromium desktop modules; do head -c 20000 /dev/urandom > $T/l/$f.sfs; done; head -c 20000 /dev/urandom > $T/l/vmlinuz
	python3 $I/systools/make_release.py --layers $T/l --kernel $T/l/vmlinuz --out $T/o --key $T/test.key --serial $1 --version $2 >/dev/null 2>&1 || { echo "make_release failed"; exit 1; }
	mkdir -p $T/srv/RudolfOS/$3 $T/srv/RudolfOS/files; cp $T/o/channel/* $T/srv/RudolfOS/$3/; cp -r $T/o/files/* $T/srv/RudolfOS/files/; }
rel 2026111503 1.5.9 stable; rel 2026111506 1.6.0 beta; rm -f $T/test.key
PORT=8457; (cd $T/srv && exec python3 -m http.server $PORT --bind 127.0.0.1 >/dev/null 2>&1) & SRV=$!
for i in $(seq 20); do python3 -c "import urllib.request;urllib.request.urlopen('http://127.0.0.1:$PORT/RudolfOS/stable/manifest.txt')" 2>/dev/null && break; sleep 0.3; done
OFF=http://127.0.0.1:$PORT/RudolfOS
mkdir -p $T/media/rudolf $T/sys; printf 'serial 2026110100\nversion 1.5.7\n' > $T/media/rudolf/release
echo disc > $T/slot; echo "rudolf.boot=uefi" > $T/cmd; printf "/dev/nvme0n1 $T/media iso9660 ro 0 0\n/dev/nvme0n1p3 /persist ext4 rw 0 0\n" > $T/mounts
printf "url=$OFF/stable\n" > $T/conf
run() { RU_NOAUTOFETCH=1 RU_NOESP=1 RU_OFFICIAL_URL=$OFF/stable RU_SYSD=$T/sys RU_MEDIA=$T/media RU_CONF=$T/conf RU_KEYS=$T/keys RU_EXTRA=$T/extra RU_SLOT=$T/slot RU_CMDLINE=$T/cmd RU_MOUNTS=$T/mounts RU_SYSBLOCK=$T/sysblock "$@" 2>&1; }
js() { run $RU status | python3 -c "import json,sys;d=json.load(sys.stdin);a=d['available'];print(d['beta'],d['toStable'],a and a['version'])"; }
st() { sed -n "s/^$1=//p" $T/sys/state; }
started() {   # the slot the update went into started and was kept
	local s=$(st next); echo $s > $T/slot; run $RU boot-ok >/dev/null; }

o=$(run $RU check); j=$(js)
ok "1. by default (the stable address): the stable release is offered, not the beta" "$([ "$j" = "False False 1.5.9" ] && echo 0)" "$j $o"
o=$(run $RU set channel beta); j=$(js)
ok "2. beta versions on: the official beta address, and nothing offered until it is checked" "$(grep -qx "url=$OFF/beta" $T/conf && [ "$j" = "True False None" ] && echo 0)" "$j $o"
o=$(run $RU check)
ok "3. ...the check offers the beta (1.6.0)" "$(echo "$o" | grep -q 'Update available: Rudolf OS 1.6.0' && echo 0)" "$o"
o=$(run $RU download); started
ok "4. ...it downloads and starts (slot a, 1.6.0)" "$([ "$(st good)" = a ] && grep -q 'version 1.6.0' $T/sys/a/RELEASE && echo 0)" "$o"
o=$(run $RU check)
ok "5. on the beta: up to date (the stable 1.5.9 is not offered while beta versions are on)" "$(echo "$o" | grep -q 'up to date' && echo 0)" "$o"
o=$(run $RU set channel stable); j=$(js)
ok "6. beta versions off: the stable address, and the latest stable release marked to come next" "$(grep -qx "url=$OFF/stable" $T/conf && grep -qx 'tostable=1' $T/conf && echo "$o" | grep -q 'next update brings the latest stable release' && echo 0)" "$j $o"
o=$(run $RU check); j=$(js)
ok "7. ...the check offers 1.5.9 though its number is lower than the beta's (the seen beta list is not held against it)" "$(echo "$o" | grep -q 'Update available: Rudolf OS 1.5.9, the latest stable release' && [ "$j" = "False True 1.5.9" ] && echo 0)" "$j $o"
o=$(run $RU download); n=$(st next)
ok "8. ...it downloads into the spare slot, to start next, and the mark is cleared" "$([ "$n" = b ] && grep -q 'version 1.5.9' $T/sys/b/RELEASE && ! grep -q 'tostable=1' $T/conf && echo 0)" "$o"
started; o=$(run $RU check)
ok "9. ...1.5.9 started; afterwards only newer stable releases are offered (no going round in circles)" "$([ "$(st good)" = b ] && echo "$o" | grep -q 'up to date' && echo 0)" "$o"
o=$(run $RU set channel stable)
ok "10. turning it off again when it was off marks nothing" "$(! grep -q 'tostable=1' $T/conf && echo 0)" "$o"
touch $T/extra; o=$(run $RU set channel beta); r=$?
ok "11. Developer mode on: refused (its own address is kept)" "$(echo "$o" | grep -q 'Developer mode is on' && grep -qx "url=$OFF/stable" $T/conf && echo 0)" "$o"
rm -f $T/extra
grep -q 'merciless146.github.io/RudolfOS/beta' $I/sys/rudolfd.c && grep -q 'strcmp(w, "beta")' $I/sys/rudolfd.c && grep -q 'desk_admin_check(an, ap' <(sed -n '/strcmp(w, "beta")/,/^\t\t}/p' $I/sys/rudolfd.c)
ok "12. rudolfd: turning beta versions on checks an administrator's name and password, and the beta address cannot be typed in as the update address" $?
echo "$P/$((P+F)) passed"; [ $F = 0 ]
