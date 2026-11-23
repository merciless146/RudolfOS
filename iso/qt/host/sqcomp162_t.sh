#!/bin/bash
# 1.6.2 (docs/plan-1.6.2.md Part 31, item 1): the system layers' compression against the kernel that reads them.
# make_release.py writes what the release's kernel reads ("squashfs ..." from its build configuration, or carried) and
# each layer's compression (rows C, from its squashfs superblock), and refuses a layer its kernel cannot read;
# rudolf-update refuses such a release when it checks (the signed list), when it has downloaded it (each layer's own
# superblock, also for a list without the rows) and for the recovery system and a stick, each against its own
# release's kernel; and a stick's base layer, which the running kernel mounts, against the running kernel. The layers
# here are squashfs superblocks over random bytes (nothing mounts them); with mksquashfs there, also real images.
# A stand-in server on 127.0.0.1:8621, a throwaway key made here and deleted.   bash sqcomp162_t.sh
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
# sqfs FILE COMPRESSION-NUMBER: a squashfs superblock (magic, 1 MB blocks, the compression at byte 20) and random bytes
sqfs() { python3 -c "
import sys,os,struct
h=bytearray(96);h[0:4]=b'hsqs';struct.pack_into('<I',h,12,1048576);struct.pack_into('<H',h,20,int(sys.argv[2]));struct.pack_into('<HH',h,28,4,0);struct.pack_into('<Q',h,40,96+20000)
open(sys.argv[1],'wb').write(bytes(h)+os.urandom(20000))" "$1" "$2"; }
layers() {   # DIR COMP...: the four layers (base chromium desktop modules) packed with these numbers, and a kernel
	rm -rf $1; mkdir -p $1; local d=$1; shift; local i=0
	for n in base chromium desktop modules; do i=$((i+1)); sqfs $d/$n.sfs ${!i:-$1}; done; head -c 20000 /dev/urandom > $d/vmlinuz; }
printf 'CONFIG_SQUASHFS=y\nCONFIG_SQUASHFS_XZ=y\nCONFIG_SQUASHFS_ZSTD=y\nCONFIG_SQUASHFS_ZLIB=y\n# CONFIG_SQUASHFS_LZ4 is not set\n' > $T/zstd.config
printf 'CONFIG_SQUASHFS=y\nCONFIG_SQUASHFS_XZ=y\nCONFIG_SQUASHFS_ZLIB=y\n' > $T/old.config
MR() { python3 $I/systools/make_release.py --key $T/test.key "$@" 2>&1; }
pub() {   # OUT CHANNEL: a written release into the server
	mkdir -p $T/srv/RudolfOS/$2 $T/srv/RudolfOS/files; cp $1/channel/* $T/srv/RudolfOS/$2/; cp -r $1/files/* $T/srv/RudolfOS/files/; }
resign() {   # CHANNEL PYTHON-EXPR: the channel's list changed by the expression (t: its text) and signed again
	python3 - $T/test.key $T/srv/RudolfOS/$1/manifest.txt "$2" <<'P'
import sys,base64
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
k=Ed25519PrivateKey.from_private_bytes(base64.b64decode(open(sys.argv[1]).read()));t=open(sys.argv[2]).read();t=eval(sys.argv[3])
open(sys.argv[2],'w').write(t);open(sys.argv[2][:-3]+'sig','wb').write(k.sign(t.encode()))
P
}

# ---- make_release.py
layers $T/l 4; o=$(MR --layers $T/l --kernel $T/l/vmlinuz --out $T/o1 --serial 2026120101 --version 1.6.1); M=$T/o1/channel/manifest.txt
ok "1. xz layers, the kernel not said: written, a row C per layer (xz, 1 MB blocks), no squashfs line (devices take xz and zlib)" \
	"$([ "$(grep -c "^C	[a-z]*	xz	1048576$" $M)" = 4 ] && ! grep -q '^squashfs ' $M && ! grep -q '^C	vmlinuz' $M && echo 0)" "$o $(cat $M)"
layers $T/l 6; o=$(MR --layers $T/l --kernel $T/l/vmlinuz --out $T/o2 --serial 2026120102 --version 1.6.2); rc=$?
ok "2. zstd layers, the kernel not said: refused, saying what the kernel reads and how to say it" \
	"$([ $rc != 0 ] && echo "$o" | grep -q 'base.sfs is packed with zstd, which this release.s kernel cannot read (it reads xz zlib, as every kernel before 1.6.2: give --kernel-config' && [ ! -e $T/o2/channel/manifest.txt ] && echo 0)" "$o"
o=$(MR --layers $T/l --kernel $T/l/vmlinuz --out $T/o2 --serial 2026120102 --version 1.6.2 --kernel-config $T/old.config); rc=$?
ok "3. ...and with a kernel configuration without SQUASHFS_ZSTD: refused" "$([ $rc != 0 ] && echo "$o" | grep -q 'it reads xz zlib)' && echo 0)" "$o"
o=$(MR --layers $T/l --kernel $T/l/vmlinuz --out $T/o2 --serial 2026120102 --version 1.6.2 --kernel-config $T/zstd.config); M=$T/o2/channel/manifest.txt
ok "4. ...with one that has it: written, \"squashfs xz zstd zlib\" and the rows say zstd" \
	"$(grep -qx 'squashfs xz zstd zlib' $M && [ "$(grep -c '^C	[a-z]*	zstd	1048576$' $M)" = 4 ] && echo 0)" "$o"
o=$(MR --layers $T/l --kernel $T/l/vmlinuz --out $T/o3 --serial 2026120103 --version 1.6.2 --kernel-config "" --squashfs "xz zstd zlib")
ok "5. a kernel kept from the release before: its line carried (--squashfs), as ci_release.sh does" "$(grep -qx 'squashfs xz zstd zlib' $T/o3/channel/manifest.txt && echo 0)" "$o"
printf 'CONFIG_SQUASHFS=m\n' > $T/mod.config; o=$(MR --layers $T/l --kernel $T/l/vmlinuz --out $T/o4 --serial 2026120104 --version 1.6.2 --kernel-config $T/mod.config); rc=$?
ok "6. a kernel whose squashfs is a module (it cannot start the system): refused" "$([ $rc != 0 ] && echo "$o" | grep -q 'squashfs is not built into this kernel' && echo 0)" "$o"
if command -v mksquashfs >/dev/null; then
	mkdir -p $T/real/d && head -c 300000 /dev/urandom > $T/real/d/f; for c in xz zstd lz4; do mksquashfs $T/real/d $T/real/$c.sfs -comp $c -b 256K -noappend -quiet -no-progress >/dev/null 2>&1; done
	r=$(python3 -c "
import sys;sys.path.insert(0,'$I/systools');import make_release as m
print(' '.join(f'{a}/{b}' for a,b in (m.sq_layer('$T/real/'+c+'.sfs') for c in ('xz','zstd','lz4'))))")
	ok "7. real images made by mksquashfs: their compression and block size read from the superblock" "$([ "$r" = "xz/262144 zstd/262144 lz4/262144" ] && echo 0)" "$r"
else echo "SKIP 7. (no mksquashfs here)"; fi

# ---- rudolf-update on the computer
pub $T/o1 stable
PORT=8621; (cd $T/srv && exec python3 -m http.server $PORT --bind 127.0.0.1 >/dev/null 2>&1) & SRV=$!
for i in $(seq 30); do python3 -c "import urllib.request;urllib.request.urlopen('http://127.0.0.1:$PORT/RudolfOS/stable/manifest.txt')" 2>/dev/null && break; timeout 0.3 tail -f /dev/null; done
OFF=http://127.0.0.1:$PORT/RudolfOS
mkdir -p $T/media/rudolf $T/sys; printf 'serial 2026110100\nversion 1.6.0\n' > $T/media/rudolf/release
for n in base chromium desktop modules; do sqfs $T/media/rudolf/$n.sfs 4; done   # (the disc: xz, as every release so far)
echo disc > $T/slot; echo "rudolf.boot=uefi" > $T/cmd; printf "/dev/nvme0n1 $T/media iso9660 ro 0 0\n/dev/nvme0n1p3 /persist ext4 rw 0 0\n" > $T/mounts
printf "url=$OFF/stable\n" > $T/conf
run() { RU_NOAUTOFETCH=1 RU_NOESP=1 RU_OFFICIAL_URL=$OFF/stable RU_SYSD=$T/sys RU_MEDIA=$T/media RU_CONF=$T/conf RU_KEYS=$T/keys RU_EXTRA=$T/extra RU_SLOT=$T/slot RU_CMDLINE=$T/cmd RU_MOUNTS=$T/mounts RU_SYSBLOCK=$T/sysblock "$@" 2>&1; }
st() { sed -n "s/^$1=//p" $T/sys/state 2>/dev/null; }
started() { local s=$(st next); echo $s > $T/slot; run $RU boot-ok >/dev/null; }
o=$(run $RU download); started
ok "8. an xz release whose list has no squashfs line (a kernel before 1.6.2): downloaded and started, as before" "$([ "$(st good)" = a ] && grep -q 'version 1.6.1' $T/sys/a/RELEASE && echo 0)" "$o"
pub $T/o2 stable; o=$(run $RU check)
ok "9. a zstd release whose kernel reads zstd (its signed line): offered" "$(echo "$o" | grep -q 'Update available: Rudolf OS 1.6.2' && echo 0)" "$o"
o=$(run $RU download); n=$(st next)
ok "10. ...downloaded, and ready to start" "$([ "$n" = b ] && echo "$o" | grep -q 'Ready: Rudolf OS 1.6.2' && echo 0)" "$o"
started
# a zstd release whose list has no squashfs line (as a release made by an older make_release.py would have, its kernel
# from before 1.6.2): refused when checked, from the signed rows, before anything is downloaded
layers $T/l 6; MR --layers $T/l --kernel $T/l/vmlinuz --out $T/o5 --serial 2026120105 --version 1.6.3 --kernel-config $T/zstd.config --kernel-version 6.18.9-rudolf >/dev/null
pub $T/o5 stable; resign stable "t.replace('squashfs xz zstd zlib\n','')"
o=$(run $RU check); rc=$?
ok "11. a release whose kernel cannot read its layers' zstd (no line: before 1.6.2): refused when checked, saying why" \
	"$([ $rc = 4 ] && echo "$o" | grep -q "Refused: Rudolf OS 1.6.3's base layer is packed with zstd, which its own kernel (Linux 6.18.9-rudolf) cannot read (it reads zlib and xz, as every kernel before Rudolf OS 1.6.2), so it could not start. Nothing was changed." && echo 0)" "$o"
j=$(run $RU status | python3 -c "import json,sys;print(json.load(sys.stdin)['available'])")
ok "12. ...and not offered in Settings" "$([ "$j" = None ] && echo 0)" "$j"
resign stable "t.replace('squashfs xz zstd zlib\n','').replace('\tzstd\t','\txz\t').replace('L\tvmlinuz','squashfs xz zlib\nL\tvmlinuz',1)"
o=$(run $RU download); rc=$?
ok "13. a list whose rows say xz but whose layers are zstd: refused once downloaded (each layer's own superblock), nothing scheduled" \
	"$([ $rc = 9 ] && echo "$o" | grep -q "Refused: Rudolf OS 1.6.3's base layer is packed with zstd, not with xz as its signed release list says" && [ -z "$(st next)" ] && [ ! -e $T/sys/a.new ] && echo 0)" "$o"
resign stable "'\n'.join(l for l in t.split('\n') if not l.startswith('C\t'))"
o=$(run $RU download); rc=$?
ok "14. ...and one without rows (made before 1.6.2) whose kernel reads xz and zlib, its layers zstd: refused, nothing scheduled" \
	"$([ $rc = 9 ] && echo "$o" | grep -q "base layer is packed with zstd, which its own kernel (Linux 6.18.9-rudolf) cannot read (it reads zlib and xz)" && [ -z "$(st next)" ] && echo 0)" "$o"
# the recovery system (the official address's newest stable: its own kernel reads it)
resign stable "t"; rm -rf $T/sys/r; o=$(run $RU recovery-fetch); rc=$?
ok "15. the recovery system: a release whose kernel cannot read its layers is not downloaded for it" "$([ $rc = 9 ] && echo "$o" | grep -q 'cannot read' && [ ! -e $T/sys/r/complete ] && echo 0)" "$o"
pub $T/o2 stable; o=$(run $RU recovery-fetch)
ok "16. ...the zstd release whose kernel reads zstd is" "$([ -e $T/sys/r/complete ] && grep -q 'serial 2026120102' $T/sys/r/RELEASE && echo 0)" "$o"
# a stick: its release's own kernel, and the running kernel for the base layer it mounts
head -c 1100000 /dev/urandom > $T/srv/RudolfOS/files/2026120102/boot.iso
echo disc > $T/slot; mkdir -p $T/st1
o=$(run $RU stick-fetch $OFF/stable $T/st1); rc=$?
ok "17. a stick of the zstd release made from a system on the xz disc (its kernel reads xz and zlib): refused before mounting, saying to update first" \
	"$([ $rc = 9 ] && echo "$o" | grep -q "cannot read that release's base layer (packed with zstd, which its kernel does not read): update this computer first" && echo 0)" "$o"
echo b > $T/slot; o=$(run $RU stick-fetch $OFF/stable $T/st2); rc=$?
ok "18. ...from the zstd system (slot b, its list says its kernel reads zstd): not refused for that (it goes on to mount the layer)" "$(! echo "$o" | grep -q 'update this computer first' && echo 0)" "$o"
echo "$P/$((P+F)) passed"; [ $F = 0 ]
