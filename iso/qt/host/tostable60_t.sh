#!/bin/bash
# 1.6.0 (the owner, 2026-11-16): Roll back to the last stable version (rudolf-update stable-info and to-stable). A stand-in
# official server with a stable and a beta channel and the stable releases' signed history (a throwaway key made here,
# never kept): which system is a published stable release (the stable list, the signed history; never an unsigned or
# another key's list, nor an older one sent again), the other slot already holding the stable release (switched to, no
# download), the stable release downloaded (only the changes) when it is not, Get beta versions off afterwards, and the
# data check: a stable release that cannot read this computer's data is refused with why, nothing changed.
#   bash tostable60_t.sh
set -u; cd "$(dirname "$0")"; I=$(cd ../.. && pwd); T=$(mktemp -d); P=0; F=0
SRV=""; trap '[ -n "$SRV" ] && kill $SRV 2>/dev/null; rm -rf "$T"' EXIT
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
RU=$T/ru; gcc -O2 -w -o $RU $I/sys/rudolf-update.c -lcrypto -ldl || exit 1
python3 - "$T" <<'P'
import sys,base64
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization as s
t=sys.argv[1]
for n in ('test','other'):
    k=Ed25519PrivateKey.generate()
    open(f'{t}/{n}.key','w').write(base64.b64encode(k.private_bytes(s.Encoding.Raw,s.PrivateFormat.Raw,s.NoEncryption())).decode())
    open(f'{t}/{n}.pub','w').write(base64.b64encode(k.public_key().public_bytes(s.Encoding.Raw,s.PublicFormat.Raw)).decode()+'\n')
P
cp $T/test.pub $T/keys
SIGN() { python3 -c "
import sys,base64
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
k=Ed25519PrivateKey.from_private_bytes(base64.b64decode(open(sys.argv[1]).read().strip()))
open(sys.argv[3],'wb').write(k.sign(open(sys.argv[2],'rb').read()))" "$@"; }
# SERIAL VERSION DIR [norecord]: a release in srv/RudolfOS/DIR (its files in srv/RudolfOS/files); norecord: as 1.5.9 was
# published, without the data record (the lines taken out and the list signed again)
rel() {
	rm -rf $T/l $T/o; mkdir -p $T/l; for f in base chromium desktop modules; do head -c 40000 /dev/urandom > $T/l/$f.sfs; done; head -c 40000 /dev/urandom > $T/l/vmlinuz
	python3 $I/systools/make_release.py --layers $T/l --kernel $T/l/vmlinuz --out $T/o --key $T/test.key --serial $1 --version $2 >/dev/null 2>&1 || { echo "make_release failed"; exit 1; }
	if [ "${4:-}" = norecord ]; then sed -i '/^datafmt /d; /^readsfmt /d' $T/o/channel/manifest.txt; SIGN $T/test.key $T/o/channel/manifest.txt $T/o/channel/manifest.sig; fi
	mkdir -p $T/srv/RudolfOS/$3 $T/srv/RudolfOS/files; cp $T/o/channel/* $T/srv/RudolfOS/$3/; cp -r $T/o/files/* $T/srv/RudolfOS/files/; }
# the stable releases' history: SERIAL... signed with KEY
hist() { local k=$1; shift; { echo 'rudolf-stable-history 1'; for s in "$@"; do echo "S $s 1.x 2026-11-01"; done; } > $T/srv/RudolfOS/stable/history.txt; SIGN $T/$k.key $T/srv/RudolfOS/stable/history.txt $T/srv/RudolfOS/stable/history.sig; }
rel 2026111503 1.5.9.261115 stable norecord; rel 2026111611 1.6.0.261116 beta
hist test 2026111000 2026111503
PORT=8461; (cd $T/srv && exec python3 -m http.server $PORT --bind 127.0.0.1 >/dev/null 2>&1) & SRV=$!
for i in $(seq 20); do python3 -c "import urllib.request;urllib.request.urlopen('http://127.0.0.1:$PORT/RudolfOS/stable/manifest.txt')" 2>/dev/null && break; sleep 0.3; done
OFF=http://127.0.0.1:$PORT/RudolfOS
mkdir -p $T/media/rudolf $T/sys; printf 'serial 2026110100\nversion 1.5.7\n' > $T/media/rudolf/release
echo disc > $T/slot; echo "rudolf.boot=uefi" > $T/cmd; printf "/dev/nvme0n1 $T/media iso9660 ro 0 0\n/dev/nvme0n1p3 /persist ext4 rw 0 0\n" > $T/mounts
printf "url=$OFF/stable\n" > $T/conf
run() { RU_NOAUTOFETCH=1 RU_NOESP=1 RU_OFFICIAL_URL=$OFF/stable RU_SYSD=$T/sys RU_MEDIA=$T/media RU_CONF=$T/conf RU_KEYS=$T/keys RU_EXTRA=$T/extra RU_SLOT=$T/slot RU_CMDLINE=$T/cmd RU_MOUNTS=$T/mounts RU_SYSBLOCK=$T/sysblock "$@" 2>&1; }
J() { python3 -c "import json,sys;d=json.load(sys.stdin);print($1)" 2>&1; }
st() { sed -n "s/^$1=//p" $T/sys/state; }
started() { local s=$(st next); echo $s > $T/slot; run $RU boot-ok >/dev/null; }

# a computer on the stable 1.5.9 (as installed from it: slot a)
run $RU check >/dev/null; run $RU download >/dev/null; started
o=$(run $RU status)
ok "1. on a published stable release (1.5.9, slot a), with beta versions off: not offered (runningStable)" "$([ "$(echo "$o" | J "d['runningStable'],d['beta'],d['stableVersion']")" = "True False 1.5.9.261115" ] && echo 0)" "$o"
ok "2. boot-ok writes down the layout of this computer's data (2: each account its own Linux user)" "$([ "$(cat $T/datafmt 2>/dev/null)" = 2 ] && echo 0)" "$(cat $T/datafmt 2>&1)"
# Get beta versions on: the beta comes and starts in slot b
run $RU set channel beta >/dev/null; run $RU check >/dev/null; run $RU download >/dev/null; started
o=$(run $RU status)
ok "3. on the beta (1.6.0, slot b): not a stable release, the last stable one is 1.5.9" "$([ "$(echo "$o" | J "d['runningStable'],d['beta'],d['stableVersion'],d['booted']")" = "False True 1.5.9.261115 b" ] && echo 0)" "$o"
i=$(run $RU stable-info)
ok "4. stable-info: 1.5.9 (released without a data record: known to read layouts 1 and 2) is still in slot a, it reads this computer's data, and says what it does not use" \
	"$([ "$(echo "$i" | J "d['how'],d['slot'],d['compatible'],d['stable']['recorded'],d['stable']['reads'],len(d['notes']),d['dataFmt']")" = "slot a True False [1, 2] 3 2" ] && echo 0)" "$i"
cp $T/sys/state $T/state0
o=$(run $RU to-stable); rc=$?
ok "5. to-stable: 1.5.9 is in the other slot: it starts next (two tries), nothing downloaded, beta versions off" \
	"$([ $rc = 0 ] && [ "$(st next)" = a ] && [ "$(st tries)" = 2 ] && [ "$(st good)" = b ] && grep -qx "url=$OFF/stable" $T/conf && ! grep -q 'tostable=1' $T/conf && echo "$o" | grep -q 'nothing to download' && ! echo "$o" | grep -q downloading && echo 0)" "$o"
ok "6. ...and says what 1.5.9 does not use (the keyring's sign-ins, system extensions, Messages)" "$(echo "$o" | grep -q "keyring" && echo "$o" | grep -q "system extensions" && echo 0)" "$o"
started; o=$(run $RU status)
ok "7. 1.5.9 started again from slot a and is kept; it is a stable release (the button hides), no update offered" "$([ "$(st good)" = a ] && [ "$(st prev)" = b ] && [ "$(echo "$o" | J "d['runningStable'],d['beta'],d['available']")" = "True False None" ] && echo 0)" "$o"
# and back up to the beta
run $RU set channel beta >/dev/null; o=$(run $RU check)$(run $RU download); started
ok "8. back up: beta versions on again bring 1.6.0 (from what is on this computer: slot b's files reused)" "$([ "$(st good)" = b ] && grep -q 'version 1.6.0' $T/sys/b/RELEASE && echo "$o" | grep -q 'base: unchanged' && echo 0)" "$o"
# a stable release in the other slot that the start-up gave up on is not switched to: downloaded again
sed -i 's/^failed=.*/failed=a/' $T/sys/state; i=$(run $RU stable-info | J "d['how']"); sed -i 's/^failed=.*/failed=/' $T/sys/state
ok "8b. ...but not one the start-up gave up on (failed=a): it would be downloaded again" "$([ "$i" = download ] && echo 0)" "$i"
# the other slot no longer holds the stable release: it is downloaded
rm -rf $T/sys/a; o=$(run $RU to-stable); rc=$?
ok "9. the other slot without it: 1.5.9 downloaded into it (checked as updates are), to start next; beta versions off" \
	"$([ $rc = 0 ] && [ "$(st next)" = a ] && grep -q 'version 1.5.9' $T/sys/a/RELEASE && [ -e $T/sys/a/complete ] && grep -qx "url=$OFF/stable" $T/conf && ! grep -q 'tostable=1' $T/conf && echo "$o" | grep -q 'downloading' && ! echo "$o" | grep -q 'another update' && echo 0)" "$o"
started; run $RU set channel beta >/dev/null; run $RU check >/dev/null; run $RU download >/dev/null; started
ok "10. ...started; and back up to the beta again" "$([ "$(st good)" = b ] && grep -q 'version 1.6.0' $T/sys/b/RELEASE && echo 0)" "$(cat $T/sys/state)"
# the signed history: an older stable release is a stable release; lists that cannot be trusted change nothing
o=$(run $RU check >/dev/null; cat $T/sys/stable-history.txt)
ok "11. the history checked with the official key is kept" "$(echo "$o" | grep -q '^S 2026111000 ' && echo 0)" "$o"
hist other 2026111000 2026111503 2026111611; o=$(run $RU check >/dev/null; cat $T/sys/stable-history.txt; run $RU status | J "d['runningStable']")
ok "12. a history signed by another key is not used (the beta is not made a stable release by it)" "$(! echo "$o" | grep -q 2026111611 && echo "$o" | grep -qx False && echo 0)" "$o"
echo 'rudolf-stable-history 1' > $T/srv/RudolfOS/stable/history.txt; echo "S 2026111611 1.6.0 x" >> $T/srv/RudolfOS/stable/history.txt; rm -f $T/srv/RudolfOS/stable/history.sig
o=$(run $RU check >/dev/null; cat $T/sys/stable-history.txt; run $RU status | J "d['runningStable']")
ok "13. ...nor an unsigned one" "$(! echo "$o" | grep -q 2026111611 && echo "$o" | grep -qx False && echo 0)" "$o"
hist test 2026111503; o=$(run $RU check >/dev/null; cat $T/sys/stable-history.txt)
ok "14. ...and an older list sent again (signed, without 2026111000) does not take a release off the kept one" "$(echo "$o" | grep -q '^S 2026111000 ' && echo 0)" "$o"
# a system on an older stable release (in the history): no button
cp -r $T/sys/a $T/sys/a.keep; sed -i 's/^serial .*/serial 2026111000/' $T/sys/a/manifest.txt; SIGN $T/test.key $T/sys/a/manifest.txt $T/sys/a/manifest.sig
sed -i 's/^serial .*/serial 2026111000/' $T/sys/a/RELEASE; echo a > $T/slot
o=$(run $RU status | J "d['runningStable']"); ok "15. an older stable release named in the signed history is a stable release" "$([ "$o" = True ] && echo 0)" "$o"
# a build signed by another key with the stable release's own number: not a stable release
sed -i 's/^serial .*/serial 2026111503/' $T/sys/a/manifest.txt $T/sys/a/RELEASE; SIGN $T/other.key $T/sys/a/manifest.txt $T/sys/a/manifest.sig
o=$(run $RU status | J "d['runningStable'],d['signer']"); ok "16. a build signed by another key with the stable release's number: not a stable release" "$([ "$o" = "False unknown" ] && echo 0)" "$o"
rm -rf $T/sys/a; mv $T/sys/a.keep $T/sys/a; echo b > $T/slot
# Developer mode: the way back is Return to official Rudolf OS
cp $T/other.pub $T/extra; cp $T/sys/state $T/state1; cp $T/conf $T/conf1
o=$(run $RU to-stable); rc=$?; i=$(run $RU stable-info | J "d['developer'],d['how']")
ok "17. Developer mode on: to-stable refused, pointing to Return to official Rudolf OS; nothing changed" "$([ $rc = 9 ] && echo "$o" | grep -q 'Return to official' && cmp -s $T/state1 $T/sys/state && cmp -s $T/conf1 $T/conf && echo 0)" "$rc $o"
rm -f $T/extra
# the data check: a stable release that cannot read this computer's data
rel 2026111700 1.5.8.261117 stable norecord; hist test 2026111000 2026111503 2026111700
cp $T/sys/state $T/state1; cp $T/conf $T/conf1
i=$(run $RU check >/dev/null; run $RU stable-info); o=$(run $RU to-stable); rc=$?
ok "18. a stable release that cannot read this computer's data (1.5.8: one Linux user for every account) is refused, saying why; nothing changed" \
	"$([ $rc = 8 ] && [ "$(echo "$i" | J "d['compatible']")" = False ] && echo "$o" | grep -q 'cannot read the accounts and files on this computer' && echo "$o" | grep -q 'own Linux user' && cmp -s $T/state1 $T/sys/state && cmp -s $T/conf1 $T/conf && [ ! -e $T/sys/a.new ] && echo 0)" "$rc $o"
rel 2026111800 1.6.1.261118 stable; echo 3 > $T/datafmt; cp $T/sys/state $T/state1
o=$(run $RU check >/dev/null; run $RU to-stable); rc=$?
ok "19. ...and with the record: a release reading layouts 1-2 refused on a computer whose data a newer release keeps in layout 3" "$([ $rc = 8 ] && echo "$o" | grep -q 'newer way' && cmp -s $T/state1 $T/sys/state && echo 0)" "$rc $o"
echo 2 > $T/datafmt
o=$(run $RU to-stable); rc=$?
ok "20. the record read from the release list: 1.6.1 (datafmt 2, readsfmt 1-2) downloaded, its record kept with the slot" "$([ $rc = 0 ] && grep -qx 'datafmt 2' $T/sys/a/RELEASE && grep -qx 'readsfmt 1-2' $T/sys/a/RELEASE && echo 0)" "$rc $o"
# the build's record and the program's agree; rudolfd asks an administrator
d=$(sed -n 's/^datafmt //p' $I/datafmt); r=$(sed -n 's/^readsfmt //p' $I/datafmt)
c="$(sed -n 's/^#define DATAFMT //p' $I/sys/rudolf-update.c) $(sed -n 's/^#define READS_MIN //p' $I/sys/rudolf-update.c)-$(sed -n 's/^#define READS_MAX //p' $I/sys/rudolf-update.c)"
ok "21. iso/datafmt says what rudolf-update's DATAFMT and READS say ($d $r)" "$([ "$c" = "$d $r" ] && echo 0)" "$c / $d $r"
sed -n '/strcmp(w, "tostable")/,/^\t\t}/p' $I/sys/rudolfd.c | grep -q 'desk_admin_check(an, ap' && sed -n '/strcmp(w, "tostable")/,/^\t\t}/p' $I/sys/rudolfd.c | grep -q 'if (adm) {.*return; }'
ok "22. rudolfd: to-stable only with an administrator's name and password, checked by rudolfd" $?
echo "$P/$((P+F)) passed"; [ $F = 0 ]
