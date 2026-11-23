#!/bin/bash
# 1.6.2 (docs/plan-1.6.2.md Part 31, item 4): the first update after the system's images are packed another way (xz to
# zstd, or another block size) downloads them whole: rudolf-update's status says how much before downloading ("whole",
# "wholeLayers", "packed": from the layers' signed rows against this computer's own layers' superblocks), the download
# says so and does not try a delta first; the update after that is a delta again (whole 0). Squashfs superblocks over
# random bytes; a stand-in server on 127.0.0.1:8623, a throwaway key made here and deleted.   bash updsize162_t.sh
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
sqfs() { python3 -c "
import sys,os,struct
n=int(sys.argv[4]);h=bytearray(96);h[0:4]=b'hsqs';struct.pack_into('<I',h,12,int(sys.argv[3]));struct.pack_into('<H',h,20,int(sys.argv[2]));struct.pack_into('<HH',h,28,4,0);struct.pack_into('<Q',h,40,96+n)
open(sys.argv[1],'wb').write(bytes(h)+os.urandom(n))" "$@"; }
printf 'CONFIG_SQUASHFS=y\nCONFIG_SQUASHFS_XZ=y\nCONFIG_SQUASHFS_ZSTD=y\nCONFIG_SQUASHFS_ZLIB=y\n' > $T/k.config
rel() {   # SERIAL VERSION COMP BLOCK [SAME-AS-DIR]: a release (the layers' bytes new, or the previous release's kept)
	rm -rf $T/l; mkdir -p $T/l; for n in base chromium desktop modules; do
		if [ -n "${5:-}" ]; then cp $5/$n.sfs $T/l/; else sqfs $T/l/$n.sfs $3 $4 $(( ${#n} * 30000 )); fi; done
	head -c 20000 /dev/urandom > $T/l/vmlinuz
	python3 $I/systools/make_release.py --layers $T/l --kernel $T/l/vmlinuz --out $T/o --key $T/test.key --serial $1 --version $2 --kernel-config $T/k.config >/dev/null || { echo "make_release failed"; exit 1; }
	mkdir -p $T/srv/RudolfOS/stable $T/srv/RudolfOS/files; cp $T/o/channel/* $T/srv/RudolfOS/stable/; cp -r $T/o/files/* $T/srv/RudolfOS/files/; rm -rf $T/keep; cp -r $T/l $T/keep; }
PORT=8623; mkdir -p $T/srv; (cd $T/srv && exec python3 -m http.server $PORT --bind 127.0.0.1 >/dev/null 2>&1) & SRV=$!
OFF=http://127.0.0.1:$PORT/RudolfOS
mkdir -p $T/media/rudolf $T/sys; printf 'serial 2026110100\nversion 1.6.1\n' > $T/media/rudolf/release
echo disc > $T/slot; echo "rudolf.boot=uefi" > $T/cmd; printf "/dev/nvme0n1 $T/media iso9660 ro 0 0\n/dev/nvme0n1p3 /persist ext4 rw 0 0\n" > $T/mounts
printf "url=$OFF/stable\n" > $T/conf
run() { RU_NOAUTOFETCH=1 RU_NOESP=1 RU_OFFICIAL_URL=$OFF/stable RU_SYSD=$T/sys RU_MEDIA=$T/media RU_CONF=$T/conf RU_KEYS=$T/keys RU_EXTRA=$T/extra RU_SLOT=$T/slot RU_CMDLINE=$T/cmd RU_MOUNTS=$T/mounts RU_SYSBLOCK=$T/sysblock "$@" 2>&1; }
st() { sed -n "s/^$1=//p" $T/sys/state 2>/dev/null; }
started() { local s=$(st next); echo $s > $T/slot; run $RU boot-ok >/dev/null; }
av() { run $RU status | python3 -c "import json,sys;a=json.load(sys.stdin)['available'];print(a and a['whole'], a and a['wholeLayers'], a and a['packed'], a and a['size'])"; }
for i in $(seq 30); do python3 -c "import urllib.request;urllib.request.urlopen('http://127.0.0.1:$PORT/')" 2>/dev/null && break; timeout 0.3 tail -f /dev/null; done
rel 2026120101 1.6.2b 4 1048576; run $RU download >/dev/null; started
ok "0. the xz release (1 MB blocks) installed in slot a" "$([ "$(st good)" = a ] && echo 0)"
rel 2026120102 1.6.2 6 1048576; run $RU check >/dev/null; a=$(av)
ok "1. the first zstd release: the status says all four layers come whole, their size, and how they are packed now" "$([ "$a" = "$(python3 -c "import os;print(sum(os.path.getsize('$T/keep/'+n+'.sfs') for n in ('base','chromium','desktop','modules')))") base,chromium,desktop,modules zstd, 1 MB blocks $(python3 -c "import os;print(sum(os.path.getsize('$T/keep/'+n) for n in ('vmlinuz','base.sfs','chromium.sfs','desktop.sfs','modules.sfs')))")" ] && echo 0)" "$a"
o=$(run $RU download); started
ok "2. ...the download says it, layer by layer, and goes straight to the whole file (no delta tried first: only the kernel's)" "$(echo "$o" | grep -c 'packed in a new way (zstd, 1 MB blocks), so it is downloaded whole this once' | grep -qx 4 && [ "$(echo "$o" | grep -c 'delta did not work out')" = 1 ] && [ "$(echo "$o" | grep -c 'already here')" = 1 ] && [ "$(st good)" = b ] && echo 0)" "$o"
rel 2026120103 1.6.2.1 6 1048576 $T/keep; run $RU check >/dev/null; a=$(av)
ok "3. the next update (zstd again, the layers unchanged): nothing whole" "$(echo "$a" | grep -q '^0  ' && echo 0)" "$a"
o=$(run $RU download); started
ok "4. ...its layers are found on this computer (unchanged)" "$(echo "$o" | grep -c ': unchanged' | grep -qx 4 && echo 0)" "$o"
rel 2026120104 1.6.2.2 6 262144; run $RU check >/dev/null; a=$(av)
ok "5. only the block size changed (1 MB to 256 KB): whole too, said with the new block size" "$(echo "$a" | grep -q ' base,chromium,desktop,modules zstd, 256 KB blocks ' && echo 0)" "$a"
rel 2026120105 1.6.2.3 6 262144; run $RU download >/dev/null; started; rel 2026120106 1.6.2.4 6 262144; run $RU check >/dev/null; a=$(av)
ok "6. a release packed as this computer's (zstd, 256 KB): whole 0, the changes only" "$(echo "$a" | grep -q '^0  ' && echo 0)" "$a"
grep -q 'sysuWholeNote' $I/../desktop/sysupd25.js && grep -q "Download (about '+w+' MB)" $I/../desktop/sysupd25.js
ok "7. Settings › Update says the size on the button and why in the card (desktop/sysupd25.js; its page test: updsize162_t.js)" $?
echo "$P/$((P+F)) passed"; [ $F = 0 ]
