#!/bin/bash
# Test disk for the Debian phase (upd_t.py D): the signed Debian base (systools/mkdebian.sh, signed with the
# workspace test key) and a small APT repository standing in for Debian's mirrors:
#   rudolf-hello   a command-line program          rudolf-xtest   a graphical app with a Start-menu launcher
#   mkdebrepo.sh OUT.img
set -e
I=$(cd "$(dirname "$0")/.." && pwd); D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
mkdir -p $D/base $D/apt
[ -f /tmp/debout/debian.txt ] || bash $I/systools/mkdebian.sh /tmp/debout $I/keys/rpk-signing.key
cp /tmp/debout/debian.txt /tmp/debout/debian.txt.sig /tmp/debout/debian-13-rudolf.tar.xz $D/base/
cp $I/keys/rpk-signing.pub $D/base/testkey.pub   # the test adds this key; the disc itself trusts only trusted-keys.txt
P=$D/pkg; mkdir -p $P/hello/DEBIAN $P/hello/usr/bin $P/x/DEBIAN $P/x/usr/bin $P/x/usr/share/applications $P/x/usr/share/icons/hicolor/48x48/apps
printf '#!/bin/sh\necho "Hello from Debian on Rudolf OS"\n' > $P/hello/usr/bin/rudolf-hello; chmod 755 $P/hello/usr/bin/rudolf-hello
printf 'Package: rudolf-hello\nVersion: 1.0\nArchitecture: all\nMaintainer: Rudolf OS <test@rudolf.invalid>\nDescription: test program for Rudolf OS\n' > $P/hello/DEBIAN/control
gcc -O2 -static -o $P/x/usr/bin/rudolf-xtest $I/qt/xtest.c
printf '[Desktop Entry]\nType=Application\nName=Rudolf X Test\nExec=rudolf-xtest %%f\nIcon=rudolf-xtest\nStartupWMClass=RudolfXTest\nMimeType=text/plain;\nCategories=Utility;\n' > $P/x/usr/share/applications/rudolf-xtest.desktop
python3 - $P/x/usr/share/icons/hicolor/48x48/apps/rudolf-xtest.png <<'PY'
import sys,zlib,struct
w=h=48;raw=b''.join(b'\0'+b''.join(bytes([200,40+x*3,60,255]) if (x-24)**2+(y-24)**2<22**2 else b'\0\0\0\0' for x in range(w)) for y in range(h))
c=lambda t,d:struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d))
open(sys.argv[1],'wb').write(b'\x89PNG\r\n\x1a\n'+c(b'IHDR',struct.pack('>IIBBBBB',w,h,8,6,0,0,0))+c(b'IDAT',zlib.compress(raw))+c(b'IEND',b''))
PY
printf 'Package: rudolf-xtest\nVersion: 1.0\nArchitecture: amd64\nMaintainer: Rudolf OS <test@rudolf.invalid>\nDescription: test window for Rudolf OS\n' > $P/x/DEBIAN/control
dpkg-deb -Zxz --root-owner-group -b $P/hello $D/apt/rudolf-hello_1.0_all.deb >/dev/null
dpkg-deb -Zxz --root-owner-group -b $P/x $D/apt/rudolf-xtest_1.0_amd64.deb >/dev/null
(cd $D/apt && dpkg-scanpackages -m . /dev/null 2>/dev/null > Packages && gzip -k Packages)
rm -rf $P
rm -f "$1"; truncate -s 96M "$1"; mkfs.ext4 -q -F -d $D "$1"; ls -la "$1"
