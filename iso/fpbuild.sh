#!/bin/bash
# Build Flatpak 1.18.4, OSTree 2026.4 and bubblewrap 0.13.0 into src/fp/stage for Rudolf OS.
# Needs the release tarballs in src/fp (flatpak.tar.xz, ostree.tar.xz, bwrap.tar.xz, meson.tar.gz)
# and the header sources in src/fp/hdr (see fpshim.sh). GPL-2.0-or-later.
set -e
HERE=$(cd "$(dirname "$0")" && pwd); F=$HERE/src/fp; cd "$F"
for t in flatpak.tar.xz ostree.tar.xz bwrap.tar.xz meson.tar.gz; do tar -xf $t; done
bash "$HERE/fpshim.sh"
SHIM=$F/shim; ST=$F/stage; rm -rf "$ST"; mkdir -p "$ST"
export PKG_CONFIG_PATH=$SHIM/lib/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig
MESON="python3 $F/meson-1.12.1/meson.py"
# bubblewrap
(cd bubblewrap-0.13.0 && rm -rf b && CPPFLAGS=-I$SHIM/include $MESON setup b --prefix=/usr -Dselinux=disabled -Dman=disabled \
	-Dbash_completion=disabled -Dzsh_completion=disabled -Dtests=false && $MESON compile -C b && DESTDIR=$ST $MESON install -C b)
# OSTree
(cd libostree-2026.4 && ./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var --libdir=/usr/lib/x86_64-linux-gnu \
	--with-curl --without-soup --without-soup3 --with-openssl --without-libsystemd --without-selinux --without-avahi \
	--with-libarchive --without-composefs --disable-gtk-doc --disable-man --without-dracut --without-mkinitcpio \
	--disable-introspection --without-ed25519-libsodium --enable-shared CPPFLAGS=-I$SHIM/include \
	&& make -j"$(nproc)" CPPFLAGS=-I$SHIM/include && make install DESTDIR=$ST)
sed -e "s#^prefix=.*#prefix=$ST/usr#" -e "s#^libdir=.*#libdir=$ST/usr/lib/x86_64-linux-gnu#" \
	$ST/usr/lib/x86_64-linux-gnu/pkgconfig/ostree-1.pc > $SHIM/lib/pkgconfig/ostree-1.pc
# Flatpak (with its bundled bubblewrap and xdg-dbus-proxy)
(cd flatpak-1.18.4 && rm -rf b && CPPFLAGS=-I$SHIM/include LDFLAGS=-L$ST/usr/lib/x86_64-linux-gnu $MESON setup b --prefix=/usr \
	--sysconfdir=/etc --localstatedir=/var --libdir=lib/x86_64-linux-gnu -Dsystem_helper=disabled -Ddconf=disabled \
	-Dsystemd=disabled -Dmalcontent=disabled -Dwayland_security_context=disabled -Dgir=disabled -Dgtkdoc=disabled \
	-Ddocbook_docs=disabled -Dman=disabled -Dtests=false -Dinstalled_tests=false -Dselinux_module=disabled \
	-Dseccomp=enabled -Dxauth=enabled -Dauto_sideloading=false && $MESON compile -C b && DESTDIR=$ST $MESON install -C b)
LD_LIBRARY_PATH=$ST/usr/lib/x86_64-linux-gnu $ST/usr/bin/flatpak --version
