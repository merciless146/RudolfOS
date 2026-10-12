#!/bin/bash
# Two test releases on top of the disc's layers; each changes only the desktop (adds window.__RELTEST).
set -e
I=$(cd "$(dirname "$0")/.." && pwd); R=/tmp/rel; rm -rf $R; mkdir -p $R
MK=$I/src/squashfs-tools/squashfs-tools/mksquashfs; UN=$I/src/squashfs-tools/squashfs-tools/unsquashfs
export SOURCE_DATE_EPOCH=1760000000
for s in 1 2; do
	mkdir -p $R/l$s; for f in base chromium modules; do ln $I/layers/$f.sfs $R/l$s/$f.sfs; done
	$UN -q -d $R/d$s $I/layers/desktop.sfs >/dev/null
	python3 - "$R/d$s/usr/share/rudolf/www/index.html" "r$s" <<'P'
import sys;p,m=sys.argv[1],sys.argv[2];s=open(p,encoding='utf-8').read();i=s.index('</head>');open(p,'w',encoding='utf-8').write(s[:i]+f'<script>window.__RELTEST="{m}"</script>'+s[i:])
P
	$MK $R/d$s $R/l$s/desktop.sfs -comp xz -Xbcj x86 -b 1M -noappend -all-root -quiet -no-progress
	python3 $I/systools/make_release.py --layers $R/l$s --kernel $I/src/linux/arch/x86/boot/bzImage --out $R/srv$s --key $I/keys/rpk-signing.key \
		--serial 202610111$s --version 1.1.$s --chromium 141.0.7390.37 --kernel-version 6.18.55-rudolf --notes "test release $s" >/dev/null
done
rm -rf $R/srv; cp -a $R/srv1 $R/srv; cp -a $R/srv2/files/2026101112 $R/srv/files/
ls -la $R/srv/files/*/ | head -20
