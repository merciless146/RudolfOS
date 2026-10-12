#!/bin/bash
# Build PulseAudio 16.1 into src/snd/stage for Rudolf OS (sound for the desktop and Linux apps).
# Needs src/snd/pulseaudio-16.1 (github.com/pulseaudio/pulseaudio, tag v16.1) and the header
# sources in src/snd/hdr (see sndshim.sh), plus meson in src/fp. GPL-2.0-or-later.
set -e
HERE=$(cd "$(dirname "$0")" && pwd); S=$HERE/src/snd; cd "$S/pulseaudio-16.1"
bash "$HERE/sndshim.sh"
export PKG_CONFIG_PATH=$S/shim/lib/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig
MESON="python3 $HERE/src/fp/meson-1.12.1/meson.py"
echo 16.1 > .tarball-version; rm -rf b
CFLAGS="-I$S/shim/include" LDFLAGS="-L$S/shim/lib" $MESON setup b --prefix=/usr --sysconfdir=/etc --localstatedir=/var \
	--libdir=lib/x86_64-linux-gnu -Ddaemon=true -Dclient=true -Ddoxygen=false -Dman=false -Dtests=false -Ddatabase=simple \
	-Dalsa=enabled -Dudev=enabled -Dasyncns=disabled -Davahi=disabled -Dbluez5=disabled -Ddbus=disabled -Delogind=disabled \
	-Dfftw=disabled -Dglib=disabled -Dgsettings=disabled -Dgstreamer=disabled -Dgtk=disabled -Dhal-compat=false -Djack=disabled \
	-Dlirc=disabled -Dopenssl=disabled -Dorc=disabled -Doss-output=disabled -Dsamplerate=disabled -Dsoxr=disabled -Dspeex=disabled \
	-Dsystemd=disabled -Dtcpwrap=disabled -Dvalgrind=disabled -Dx11=disabled -Dwebrtc-aec=disabled \
	-Dbashcompletiondir=no -Dzshcompletiondir=no
ninja -C b
rm -rf "$S/stage"; DESTDIR=$S/stage $MESON install -C b --quiet
