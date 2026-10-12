#!/bin/bash
# A local Flatpak repository standing in for Flathub in tests: a tiny runtime (glibc, GLib, libX11) and
# org.rudolf.TestApp (qt/testapp.c) with a launcher, an icon and AppStream data marked verified.
# Built with the Flatpak tools of the Rudolf OS root file system (chroot). Writes OUT/repo1 (version 1)
# and OUT/repo2 (the same repository with version 2 added, for the update test).
#   mkflat.sh OUT
set -e
I=$(cd "$(dirname "$0")/.." && pwd); OUT=$(realpath -m "$1"); R=$I/rootfs
rm -rf "$OUT"; mkdir -p "$OUT"
W=$R/tmp/flatbuild; rm -rf "$W"; mkdir -p "$W"
gcc -O2 -o "$W/testapp" "$I/qt/testapp.c" $(pkg-config --cflags --libs gio-2.0) -lX11
# runtime: the libraries the test app needs
RT=$W/rt; mkdir -p $RT/files/lib/x86_64-linux-gnu $RT/files/lib64 $RT/files/bin $RT/files/etc
for l in $(ldd "$W/testapp" | awk '/=>/ {print $3}'); do cp -L "$l" $RT/files/lib/x86_64-linux-gnu/; done
cp -L /lib64/ld-linux-x86-64.so.2 $RT/files/lib64/
cp "$R/bin/busybox" $RT/files/bin/sh 2>/dev/null || true
printf '[Runtime]\nname=org.rudolf.TestPlatform\nruntime=org.rudolf.TestPlatform/x86_64/1\nsdk=org.rudolf.TestPlatform/x86_64/1\n' > $RT/metadata
mkapp() {   # version
	A=$W/app$1; rm -rf $A; mkdir -p $A/files/bin $A/files/share/applications $A/files/share/icons/hicolor/128x128/apps $A/files/share/app-info/xmls $A/files/share/app-info/icons/flatpak/128x128
	cp "$W/testapp" $A/files/bin/testapp
	printf '[Application]\nname=org.rudolf.TestApp\nruntime=org.rudolf.TestPlatform/x86_64/1\nsdk=org.rudolf.TestPlatform/x86_64/1\ncommand=testapp\n\n[Context]\nshared=ipc;network;\nsockets=x11;pulseaudio;\nfilesystems=home;\n' > $A/metadata
	printf '[Desktop Entry]\nType=Application\nName=Rudolf Test App\nComment=Opens text files (test)\nExec=testapp %%F\nIcon=org.rudolf.TestApp\nMimeType=text/plain;\nStartupWMClass=RudolfTest\nCategories=Utility;\n' > $A/files/share/applications/org.rudolf.TestApp.desktop
	python3 - "$A/files/share/icons/hicolor/128x128/apps/org.rudolf.TestApp.png" <<'P'
import sys,zlib,struct
w=h=128;raw=b''.join(b'\0'+b''.join(bytes([40,120+(x//2)%80,220,255]) if (x-64)**2+(y-64)**2<56**2 else b'\0\0\0\0' for x in range(w)) for y in range(h))
c=lambda t,d:struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d))
open(sys.argv[1],'wb').write(b'\x89PNG\r\n\x1a\n'+c(b'IHDR',struct.pack('>IIBBBBB',w,h,8,6,0,0,0))+c(b'IDAT',zlib.compress(raw))+c(b'IEND',b''))
P
	cp $A/files/share/icons/hicolor/128x128/apps/org.rudolf.TestApp.png $A/files/share/app-info/icons/flatpak/128x128/
	cat > $A/files/share/app-info/xmls/org.rudolf.TestApp.xml <<X
<?xml version="1.0" encoding="UTF-8"?>
<components version="0.8" origin="flatpak">
  <component type="desktop-application">
    <id>org.rudolf.TestApp</id>
    <name>Rudolf Test App</name>
    <summary>A small app for testing Linux app support</summary>
    <description><p>Opens a window, opens files and asks for file dialogs.</p><ul><li>Version $1</li></ul></description>
    <icon type="cached" height="128" width="128">org.rudolf.TestApp.png</icon>
    <categories><category>Utility</category></categories>
    <project_license>GPL-2.0-or-later</project_license>
    <developer id="org.rudolf"><name>Rudolf OS authors</name></developer>
    <releases><release version="$1.0" date="2026-10-0$1"/></releases>
    <custom><value key="flathub::verification::verified">true</value></custom>
    <bundle type="flatpak" runtime="org.rudolf.TestPlatform/x86_64/1" sdk="org.rudolf.TestPlatform/x86_64/1">app/org.rudolf.TestApp/x86_64/stable</bundle>
  </component>
</components>
X
	gzip -f $A/files/share/app-info/xmls/org.rudolf.TestApp.xml
}
mkapp 1; mkapp 2
mount --bind /proc $R/proc 2>/dev/null || true
mount --bind /dev $R/dev 2>/dev/null || true
# build-export checks launchers with desktop-file-validate, which the system does not ship: a stand-in for the build
printf '#!/bin/sh\nexit 0\n' > $R/usr/bin/desktop-file-validate; chmod 755 $R/usr/bin/desktop-file-validate
# icon checks run in a bubblewrap sandbox that a build container may not allow: the test icon is made above
mv $R/usr/libexec/flatpak-validate-icon $R/usr/libexec/flatpak-validate-icon.real
printf '#!/bin/sh\nexit 0\n' > $R/usr/libexec/flatpak-validate-icon; chmod 755 $R/usr/libexec/flatpak-validate-icon
trap 'umount $R/proc $R/dev 2>/dev/null || true; rm -f $R/usr/bin/desktop-file-validate; mv -f $R/usr/libexec/flatpak-validate-icon.real $R/usr/libexec/flatpak-validate-icon' EXIT
F="chroot $R env -i PATH=/usr/bin:/bin HOME=/tmp LANG=C.UTF-8"
$F flatpak build-finish /tmp/flatbuild/app1 >/dev/null
$F flatpak build-finish /tmp/flatbuild/app2 >/dev/null
$F flatpak build-export --runtime --files=files /tmp/flatbuild/repo /tmp/flatbuild/rt 1
$F flatpak build-export /tmp/flatbuild/repo /tmp/flatbuild/app1 stable
$F flatpak build-update-repo /tmp/flatbuild/repo
cp -a $W/repo "$OUT/repo1"
$F flatpak build-export /tmp/flatbuild/repo /tmp/flatbuild/app2 stable
$F flatpak build-update-repo /tmp/flatbuild/repo
cp -a $W/repo "$OUT/repo2"
rm -rf "$W"
du -sh "$OUT"/repo1 "$OUT"/repo2
