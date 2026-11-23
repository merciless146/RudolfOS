#!/bin/bash
# 1.6.2 (docs/plan-1.6.2.md Part 31, item 2): a release's four layers made again from the same root with one compression,
# for the measurements (zs162_t.py, .github/workflows/zstd-measure.yml). Tests only: never part of a release.
#   sudo bash zs162_layers.sh REL-DIR KERNEL-DIR SPEC OUT-DIR
# REL-DIR: the release (base.sfs, chromium.sfs, desktop.sfs); KERNEL-DIR: the test kernel's modules.sfs (testkernel.sh:
# the release's kernel with zstd and lz4); SPEC: xz-1m, xz-256k, zstd15-1m, zstd19-256k, lz4-1m, ... (xz with the x86
# branch filter as mklayers.sh; lz4 with its high-compression mode, -Xhc: the same reading speed, smaller).
# Each layer is made twice (SOURCE_DATE_EPOCH kept, as mklayers.sh) and the two compared byte for byte; xz-1m is also
# compared with the release's own layer (without its hash tree). Then the hash tree is added as a release's (verity.py).
# OUT-DIR gets the four layers and info.json: each layer's size, the time to make it, whether the builds were the same.
set -e
REL=$(realpath "$1"); KER=$(realpath "$2"); SPEC=$3; OUT=$(realpath -m "$4"); HERE=$(cd "$(dirname "$0")/../.." && pwd)
case "$SPEC" in
	xz-*) OPT="-comp xz -Xbcj x86";;
	zstd[0-9]*-*) L=${SPEC%%-*}; OPT="-comp zstd -Xcompression-level ${L#zstd}";;
	lz4-*) OPT="-comp lz4 -Xhc";;
	*) echo "zs162_layers: unknown compression in $SPEC"; exit 2;;
esac
case "${SPEC##*-}" in 1m) B=1M;; 256k) B=256K;; 128k) B=128K;; *) echo "zs162_layers: unknown block size in $SPEC"; exit 2;; esac
export SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH:-1760000000}
W=$(realpath -m "$OUT.work"); rm -rf "$W" "$OUT"; mkdir -p "$W" "$OUT/1" "$OUT/2"
for n in base chromium desktop; do unsquashfs -q -n -d "$W/$n" "$REL/$n.sfs" >/dev/null; done
unsquashfs -q -n -d "$W/modules" "$KER/modules.sfs" >/dev/null
J="{\"spec\":\"$SPEC\",\"options\":\"$OPT -b $B\",\"mksquashfs\":\"$(mksquashfs -version 2>&1 | head -1)\",\"layers\":{"
first=1
for n in base chromium desktop modules; do
	for b in 1 2; do
		S=$(date +%s.%N)
		mksquashfs "$W/$n" "$OUT/$b/$n.sfs" $OPT -b $B -noappend -all-root -quiet -no-progress >/dev/null
		E=$(date +%s.%N); [ $b = 1 ] && T=$(echo "$E $S" | awk '{printf "%.1f", $1 - $2}')
	done
	cmp -s "$OUT/1/$n.sfs" "$OUT/2/$n.sfs" && SAME=true || SAME=false
	SZ=$(stat -c %s "$OUT/1/$n.sfs"); RAW=$(du -sb "$W/$n" | cut -f1)
	REL_SAME=null
	if [ "$SPEC" = xz-1m ] && [ "$n" != modules ]; then
		U=$(python3 -c "import struct,sys;print(struct.unpack_from('<Q',open(sys.argv[1],'rb').read(96),40)[0])" "$REL/$n.sfs")
		head -c "$U" "$REL/$n.sfs" | cmp -s - <(head -c "$U" "$OUT/1/$n.sfs") && [ "$U" = "$(python3 -c "import struct,sys;print(struct.unpack_from('<Q',open(sys.argv[1],'rb').read(96),40)[0])" "$OUT/1/$n.sfs")" ] && REL_SAME=true || REL_SAME=false
	fi
	mv "$OUT/1/$n.sfs" "$OUT/$n.sfs"; python3 "$HERE/iso/systools/verity.py" add "$OUT/$n.sfs" $n >/dev/null
	echo "$SPEC $n: $SZ bytes ($RAW unpacked), made in $T s, the two builds the same: $SAME$([ $REL_SAME = null ] || echo ", the same as the release's own: $REL_SAME")"
	[ $first = 1 ] || J="$J,"; first=0
	J="$J\"$n\":{\"bytes\":$SZ,\"file\":$(stat -c %s "$OUT/$n.sfs"),\"unpacked\":$RAW,\"make_s\":$T,\"reproducible\":$SAME,\"same_as_release\":$REL_SAME}"
done
echo "$J}}" > "$OUT/info.json"
rm -rf "$W" "$OUT/1" "$OUT/2"
