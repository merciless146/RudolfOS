#!/bin/bash
# 1.6.0 (plan-1.6.1 Part 16, moved to 1.6.0): each other's builds, with rudolf-update over a local server: the update
# address check (the official key; another build's key published beside its list; an unknown key; a missing file; the
# other channel), Get beta versions keeping the repository that is set, a USB stick's release downloaded and checked
# against its own system's key, and the official recovery system kept at the newest official stable release.
#   bash ownbuilds60_t.sh   (gcc, libcrypto, python3 with cryptography)
set -u; cd "$(dirname "$0")"; I=$(cd ../.. && pwd); W=$(mktemp -d); P=0; F=0; SP=
trap '[ -n "$SP" ] && kill $SP 2>/dev/null; rm -rf "$W"' EXIT
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
gcc -O2 -w -o $W/ru $I/sys/rudolf-update.c -lcrypto -ldl || exit 1
python3 - $W <<'P'
import sys,base64
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization as s
for n in ('off','oth'):
    k=Ed25519PrivateKey.generate()
    open(f'{sys.argv[1]}/{n}.key','w').write(base64.b64encode(k.private_bytes(s.Encoding.Raw,s.PrivateFormat.Raw,s.NoEncryption())).decode()+'\n')
    open(f'{sys.argv[1]}/{n}.pub','w').write(base64.b64encode(k.public_key().public_bytes(s.Encoding.Raw,s.PublicFormat.Raw)).decode()+'\n')
P
mkdir -p $W/srv
rel() {   # rel DIR CHANNEL KEY SERIAL VERSION
	local L=$W/l$4; mkdir -p $L; for f in base chromium desktop modules; do head -c 40000 /dev/urandom > $L/$f.sfs; done; head -c 20000 /dev/urandom > $L/vmlinuz
	rm -rf $W/o$4; python3 $I/systools/make_release.py --layers $L --kernel $L/vmlinuz --out $W/o$4 --key $W/$3.key --serial $4 --version $5 >/dev/null 2>&1 || { echo "make_release failed"; exit 1; }
	mkdir -p $W/srv/$1/files; rm -rf $W/srv/$1/$2; mv $W/o$4/channel $W/srv/$1/$2; cp -a $W/o$4/files/. $W/srv/$1/files/; head -c 1100000 /dev/urandom > $W/srv/$1/files/$4/boot.iso; }
rel off stable off 2026120100 1.6.0; rel off beta off 2026120200 1.6.1-beta
rel oth stable oth 2026120300 1.6.0-alex; cp $W/oth.pub $W/srv/oth/stable/key.txt
rel bad stable oth 2026120400 1.6.0-bob
rel miss stable off 2026120500 1.6.0; rm $W/srv/miss/files/2026120500/desktop.sfs
PORT=$((20000 + RANDOM % 20000)); (cd $W/srv && exec python3 -m http.server $PORT --bind 127.0.0.1 >/dev/null 2>&1) & SP=$!; sleep 1
U=http://127.0.0.1:$PORT
mkdir -p $W/sys; : > $W/cmd; : > $W/mounts; echo a > $W/slot
export RU_SYSD=$W/sys RU_MEDIA=$W/media RU_CONF=$W/conf RU_KEYS=$W/off.pub RU_EXTRA=$W/extra-keys RU_SLOT=$W/slot RU_CMDLINE=$W/cmd RU_MOUNTS=$W/mounts RU_SYSBLOCK=$W/sb RU_NOAUTOFETCH=1 RU_OFFICIAL_URL=$U/off/stable
J() { python3 -c "import json,sys;d=json.load(sys.stdin);print($1)"; }
o=$($W/ru check-url $U/off/stable)
ok "1. the official address: good, the official key, its beta channel there" "$(echo "$o" | J "0 if d['good'] and d['key']=='official' and d['serial']==2026120100 and any('beta channel is there' in l[1] for l in d['lines']) else 1")" "$o"
o=$($W/ru check-url $U/oth/stable)
fp=$(python3 -c "import hashlib,base64;h=hashlib.sha256(base64.b64decode(open('$W/oth.pub').read().strip())).hexdigest()[:32];print(' '.join(h[i:i+4] for i in range(0,32,4)))")
ok "2. another build: its own key (published beside its list) named, updates said not to install here, a stick offered" "$(echo "$o" | J "0 if d['key']=='other' and d['fpr']=='$fp' and any(l[0]=='warn' and 'another build' in l[1] and 'USB stick' in l[1] for l in d['lines']) else 1")" "$o"
o=$($W/ru check-url $U/bad/stable)
ok "3. a list signed by an unknown key, no key published: refused" "$(echo "$o" | J "0 if not d['good'] and d['key']=='none' and any(l[0]=='fail' and 'not signed by a key' in l[1] for l in d['lines']) else 1")" "$o"
o=$($W/ru check-url $U/miss/stable)
ok "4. a file missing: said which" "$(echo "$o" | J "0 if not d['good'] and any(l[0]=='fail' and l[1].startswith('desktop cannot be downloaded') for l in d['lines']) else 1")" "$o"
o=$($W/ru check-url $U/oth/stable)
ok "5. no beta channel beside it: a warning only" "$(echo "$o" | J "0 if any(l[0]=='warn' and 'no beta channel' in l[1] for l in d['lines']) else 1")" "$o"
o=$($W/ru check-url $U/nothing/stable)
ok "6. an address that does not answer" "$(echo "$o" | J "0 if not d['good'] and d['lines'][0][0]=='fail' else 1")" "$o"
# Get beta versions keeps the repository
$W/ru set url $U/oth/stable >/dev/null; $W/ru set channel beta >/dev/null
ok "7. Get beta versions on: the set repository's /beta" "$(grep -qx "url=$U/oth/beta" $W/conf && echo 0)" "$(cat $W/conf)"
$W/ru set channel stable >/dev/null
ok "8. ...off: its /stable" "$(grep -qx "url=$U/oth/stable" $W/conf && echo 0)" "$(cat $W/conf)"
$W/ru set url $U/oth/custom >/dev/null; o=$($W/ru set channel beta)
ok "9. an address ending in neither: used as it is, and said" "$(grep -qx "url=$U/oth/custom" $W/conf && echo "$o" | grep -q 'neither /stable nor /beta' && echo 0)" "$o"
# a stick's release
cp $W/oth.pub $W/basekeys
o=$(RU_STICK_BASEKEYS=$W/basekeys $W/ru stick-fetch $U/oth/stable $W/st1 | tail -1)
ok "10. a stick from another build: downloaded, checked with its own system's key, its address kept for its updates" "$(echo "$o" | J "0 if d['key']=='other' and d['serial']==2026120300 else 1")" "$o"
ok "11. ...its slot complete, boot image there, state and update address set" "$([ -s $W/st1/os/system/a/base.sfs ] && [ -e $W/st1/os/system/a/complete ] && [ -s $W/st1/boot.iso ] && grep -qx good=a $W/st1/os/system/state && grep -qx "url=$U/oth/stable" $W/st1/os/update.conf && echo 0)" "$(ls -R $W/st1 | head -20)"
cp $W/off.pub $W/basekeys2; RU_STICK_BASEKEYS=$W/basekeys2 $W/ru stick-fetch $U/oth/stable $W/st2 >/dev/null; rc=$?
ok "12. a list its own system would refuse (not signed by the key in its base layer): refused" "$([ $rc = 4 ] && echo 0)" "rc $rc"
# the official recovery system, kept newest
o=$($W/ru recovery-fetch); ok "13. the official recovery system downloaded (release 2026120100)" "$(grep -qx 'serial 2026120100' $W/sys/r/RELEASE && echo 0)" "$o"
o=$($W/ru recovery-fetch); ok "14. ...not again while it is the newest" "$(echo "$o" | grep -q 'is the newest official stable release (2026120100)' && echo 0)" "$o"
rel off stable off 2026120600 1.6.0.1
o=$($W/ru recovery-fetch); ok "15. a newer official stable release: the recovery system replaced by it" "$(grep -qx 'serial 2026120600' $W/sys/r/RELEASE && [ -e $W/sys/r/complete ] && [ -e $W/sys/r/official ] && [ ! -e $W/sys/r.new ] && echo 0)" "$o"
rel off beta off 2026120700 1.6.1-beta2
o=$($W/ru recovery-fetch); ok "16. a newer beta does not touch it" "$(grep -qx 'serial 2026120600' $W/sys/r/RELEASE && echo 0)" "$o"
RU_OFFICIAL_URL=$U/oth/stable $W/ru recovery-fetch >/dev/null 2>&1
ok "17. another key's release never becomes the recovery system" "$(grep -qx 'serial 2026120600' $W/sys/r/RELEASE && echo 0)"
# a stick's Recovery mode (oscopy.sh os_rescue): the official recovery system, not a copy of the stick's system
. $I/sys/root/usr/lib/rudolf/oscopy.sh
mkdir -p $W/stk/os/system/a; cp $W/st1/os/system/a/* $W/stk/os/system/a/; echo good=a > $W/stk/os/system/state
o=$(RUDOLF_SYSD=$W/sys RUDOLF_UPDATE=$W/ru os_rescue $W/stk)
ok "18. a new stick (no recovery system copied): the official one, from this computer's, newest" "$(grep -qx 'serial 2026120600' $W/stk/os/system/r/RELEASE && [ -e $W/stk/os/system/r/official ] && [ ! -e $W/stk/os/system/r/rescue ] && echo "$o" | grep -q 'official Rudolf OS' && [ "$(stat -c %i $W/sys/r/base.sfs)" = "$(stat -c %i $W/stk/os/system/r/base.sfs)" ] && echo 0)" "$o"
mkdir -p $W/stk2/os/system/a; cp $W/st1/os/system/a/* $W/stk2/os/system/a/; echo good=a > $W/stk2/os/system/state
o=$(RU_OFFICIAL_URL=$U/nothing/stable RUDOLF_SYSD=$W/none RUDOLF_UPDATE=$W/ru os_rescue $W/stk2)
ok "19. no official recovery system to be had: a rescue copy, with a warning" "$([ -e $W/stk2/os/system/r/rescue ] && echo "$o" | grep -q 'Warning: the official recovery system could not be had' && echo 0)" "$o"
echo "$P/$((P+F)) passed"; [ $F = 0 ]
