#!/bin/bash
# Build the package repository that goes on the Rudolf OS disc (RUDOLF/PKG) from the recipes in pkgs/.
# Debian packages come from DEBS (default pkgtools/debs); without network access to a Debian mirror they are
# rebuilt from this machine's installed packages ("local-deb"). The signing key lives in keys/ and is never
# copied into the disc image or the source archive. GPL-2.0-or-later.
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd); cd "$HERE"
T=pkgtools/rpk-tool.py; DEBS=${DEBS:-pkgtools/debs}; KEY=keys/rpk-signing.key
[ -f $KEY ] || python3 $T keygen keys
mkdir -p sys/root/etc/rudolf; cp trusted-keys.txt sys/root/etc/rudolf/rpk-keys   # the system trusts only trusted-keys.txt
mkdir -p $DEBS repo; rm -f repo/*.rpk repo/index.*
for r in pkgs/*/recipe.json; do
	for d in $(python3 -c "import json,sys;print(' '.join(json.load(open(sys.argv[1])).get('debs',[])))" $r); do
		ls $DEBS/${d}_*.deb >/dev/null 2>&1 || python3 $T local-deb $d $DEBS >/dev/null
	done
	n=$(basename $(dirname $r)); w=$(mktemp -d)
	python3 $T from-recipe $r $w/pkg --debs $DEBS --rootfs rootfs
	python3 $T build $w/pkg repo/${n//-/_}.rpk --key $KEY
	rm -rf $w
done
python3 $T index repo --key $KEY
