#!/bin/bash
# Build the Xorg server (21.1, with the modesetting driver, glamor and GLX) into src/xorg/stage.
# 1.4.3: GLX on. Without it, apps and games that draw with OpenGL through X11 (Oolite, through SDL) could not
# open a window ("GLX is not supported").
# Sources: github.com/NetBSD/xsrc (external/mit/xorg-server/dist and the library headers listed in
# xshim.sh). Two NetBSD build-file differences are undone (fb/fbtile.c is listed again). GPL-2.0-or-later.
set -e
HERE=$(cd "$(dirname "$0")" && pwd); XS=$HERE/src/xorg; D=$XS/xsrc/external/mit/xorg-server/dist
bash "$HERE/xshim.sh"
grep -q "'fbtile.c'" "$D/fb/meson.build" || sed -i "s/^\t'fbsolid.c',$/\t'fbsolid.c',\n\t'fbtile.c',/" "$D/fb/meson.build"
export PKG_CONFIG_PATH=$XS/shim/lib/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig
MESON="python3 $HERE/src/fp/meson-1.12.1/meson.py"; [ -f "$HERE/src/fp/meson-1.12.1/meson.py" ] || MESON=meson   # (the release build: Ubuntu's meson)
cd "$D"; rm -rf b
CFLAGS="-I$XS/shim/include" $MESON setup b --prefix=/usr --sysconfdir=/etc --localstatedir=/var --libdir=lib/x86_64-linux-gnu \
	-Dxorg=true -Dxephyr=false -Dxnest=false -Dxvfb=false -Dxwin=false -Dxquartz=false -Dglamor=true -Dglx=true \
	-Dxdmcp=false -Dxdm-auth-1=false -Dsecure-rpc=false -Dipv6=true -Dlisten_tcp=false -Dint10=false -Dsuid_wrapper=false \
	-Dpciaccess=true -Dudev=true -Dudev_kms=true -Dhal=false -Dsystemd_logind=false -Dvgahw=false -Dxselinux=false \
	-Dxcsecurity=false -Ddga=false -Dxvmc=false -Dlinux_apm=false -Dlinux_acpi=false -Dagp=false -Dsha1=libcrypto \
	-Ddri1=false -Ddri2=true -Ddri3=true -Ddrm=true -Dlibunwind=false -Ddocs=false -Ddevel-docs=false -Ddocs-pdf=false \
	-Ddtrace=false -Dxkb_dir=/usr/share/X11/xkb -Dxkb_output_dir=/var/lib/xkb -Dxkb_bin_dir=/usr/bin -Dlog_dir=/run/rudolf \
	-Dmodule_dir=/usr/lib/xorg/modules -Ddefault_font_path=/usr/share/fonts/X11/misc,built-ins \
	-Dvendor_name="Rudolf OS" -Dvendor_name_short=Rudolf
ninja -C b
DESTDIR=$XS/stage $MESON install -C b --quiet
