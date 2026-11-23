#!/bin/bash
# 1.6.2 (docs/plan-1.6.2.md Part 31, item 3): the system's images packed with zstd (systools/sqopts.sh). mklayers.sh makes
# the four layers with it, the same bytes twice (SOURCE_DATE_EPOCH); ci_release.sh's repack_layers makes a layer kept from
# an xz release again with it (and the same bytes as a fresh one from the same files), and leaves one already packed so;
# make_release.py writes the release only with a kernel that reads zstd. A small root file system made here; needs
# mksquashfs and unsquashfs with zstd, and sudo (as ci_release.sh).   bash zsswitch162_t.sh
set -u; cd "$(dirname "$0")"; I=$(cd ../.. && pwd); T=$(mktemp -d); P=0; F=0; trap 'sudo rm -rf "$T"' EXIT
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
command -v mksquashfs >/dev/null && mksquashfs -help 2>&1 | grep -q zstd || { echo "SKIP: needs mksquashfs with zstd"; exit 0; }
. $I/systools/sqopts.sh
sb() { echo "$(od -An -tu2 -j20 -N2 "$1" | tr -d ' ') $(od -An -tu4 -j12 -N4 "$1" | tr -d ' ')"; }
R=$T/root; mkdir -p $R/opt/chromium $R/usr/share/rudolf/www $R/usr/lib/modules/6.18.9-rudolf $R/usr/bin $R/etc
for d in opt/chromium usr/share/rudolf/www usr/lib/modules/6.18.9-rudolf usr/bin; do head -c 3000000 /dev/urandom | base64 > $R/$d/f; done; echo x > $R/etc/hostname
o=$(bash $I/systools/mklayers.sh $R $T/l1 2>&1); bash $I/systools/mklayers.sh $R $T/l2 >/dev/null 2>&1
s=$(for n in base chromium desktop modules; do sb $T/l1/$n.sfs; done | sort -u)
ok "1. mklayers.sh: the four layers packed with zstd ($SQ_OPTS)" "$([ "$s" = "$SQ_COMP_ID $SQ_BLOCK" ] && echo 0)" "$s $o"
ok "2. ...the same bytes twice" "$(for n in base chromium desktop modules; do cmp -s $T/l1/$n.sfs $T/l2/$n.sfs || echo $n; done | grep -q . || echo 0)"
# repack_layers, as ci_release.sh has it
rm -rf $T/w/new; mkdir -p $T/w/new $T/x
for n in base chromium desktop modules; do sudo unsquashfs -q -n -d $T/x/$n $T/l1/$n.sfs >/dev/null; done
for n in base chromium; do sudo mksquashfs $T/x/$n $T/w/new/$n.sfs -comp xz -Xbcj x86 -b 1M -noappend -all-root -quiet -no-progress >/dev/null; done
for n in desktop modules; do cp $T/l1/$n.sfs $T/w/new/; done
sed -n '/^repack_layers() {/,/^}/p' $I/systools/ci_release.sh > $T/fn.sh; [ -s $T/fn.sh ] || ok "3. repack_layers is in ci_release.sh" 1
o=$(cd $I && SOURCE_DATE_EPOCH=1760000000 W=$T/w MK=mksquashfs SQ="$SQ_OPTS -noappend -all-root -quiet -no-progress" bash -c ". systools/sqopts.sh; NOTES=(); . $T/fn.sh; repack_layers; printf 'NOTE %s\n' \"\${NOTES[@]}\"" 2>&1)
s=$(for n in base chromium desktop modules; do sb $T/w/new/$n.sfs; done | sort -u)
ok "3. ci_release.sh's repack_layers: the two xz layers kept from a release made again with zstd, the two others left" "$([ "$s" = "$SQ_COMP_ID $SQ_BLOCK" ] && [ "$(echo "$o" | grep -c 'packed another way')" = 2 ] && echo "$o" | grep -q 'NOTE the system.s images packed with zstd' && echo 0)" "$o"
ok "4. ...the layer made again is the same bytes as mklayers.sh's from the same files (the next release's unchanged layer stays the same)" "$(cmp -s $T/w/new/base.sfs $T/l1/base.sfs && cmp -s $T/w/new/chromium.sfs $T/l1/chromium.sfs && echo 0)"
o=$(cd $I && SOURCE_DATE_EPOCH=1760000000 W=$T/w MK=mksquashfs SQ="$SQ_OPTS -noappend -all-root -quiet -no-progress" bash -c ". systools/sqopts.sh; NOTES=(); . $T/fn.sh; repack_layers; echo \"N=\${#NOTES[@]}\"" 2>&1)
ok "5. ...run again: nothing to do, no note" "$(echo "$o" | grep -qx 'N=0' && ! echo "$o" | grep -q 'packed another way' && echo 0)" "$o"
python3 -c "
import base64;from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey as K;from cryptography.hazmat.primitives import serialization as s
open('$T/k','w').write(base64.b64encode(K.generate().private_bytes(s.Encoding.Raw,s.PrivateFormat.Raw,s.NoEncryption())).decode())"
head -c 9000 /dev/urandom > $T/l1/vmlinuz
o=$(python3 $I/systools/make_release.py --layers $T/l1 --kernel $T/l1/vmlinuz --out $T/o1 --key $T/k --serial 2026120201 --version 1.6.2 --squashfs "xz zlib" 2>&1); r1=$?
o2=$(python3 $I/systools/make_release.py --layers $T/l1 --kernel $T/l1/vmlinuz --out $T/o2 --key $T/k --serial 2026120201 --version 1.6.2 --squashfs "xz zstd zlib" 2>&1); r2=$?
ok "6. make_release.py: the zstd layers with a kernel that does not read zstd: refused; with one that does: written" "$([ $r1 != 0 ] && [ $r2 = 0 ] && grep -q "^C	base	zstd	$SQ_BLOCK$" $T/o2/channel/manifest.txt && echo 0)" "$o $o2"
echo "$P/$((P+F)) passed"; [ $F = 0 ]
