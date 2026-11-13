#!/bin/bash
# Build PulseAudio 16.1 into src/snd/stage for Rudolf OS (sound for the desktop and Linux apps).
# Needs src/snd/pulseaudio-16.1 (github.com/pulseaudio/pulseaudio, tag v16.1) and the header
# sources in src/snd/hdr (see sndshim.sh), plus meson in src/fp. GPL-2.0-or-later.
set -e
MA=${MA:-$(gcc -print-multiarch)}; [ -n "$MA" ] || MA=x86_64-linux-gnu   # (1.5.9: the build machine's library folder; aarch64-linux-gnu on ARM64)
HERE=$(cd "$(dirname "$0")" && pwd); S=$HERE/src/snd; cd "$S/pulseaudio-16.1"
# (1.5.9: on a build machine with the -dev packages of the same libraries, the ARM64 check, no shim is needed)
if [ -d "$S/hdr" ]; then bash "$HERE/sndshim.sh"; else echo "sndbuild: no header sources in src/snd/hdr: the system's (libasound2-dev libsndfile1-dev libudev-dev libltdl-dev)"; mkdir -p "$S/shim/include" "$S/shim/lib/pkgconfig"; fi
export PKG_CONFIG_PATH=$S/shim/lib/pkgconfig:/usr/lib/$MA/pkgconfig:/usr/share/pkgconfig
MESON="python3 $HERE/src/fp/meson-1.12.1/meson.py"; [ -f "$HERE/src/fp/meson-1.12.1/meson.py" ] || MESON=meson   # (Ubuntu's meson)
echo 16.1 > .tarball-version; rm -rf b
CFLAGS="-I$S/shim/include" LDFLAGS="-L$S/shim/lib" $MESON setup b --prefix=/usr --sysconfdir=/etc --localstatedir=/var \
	--libdir=lib/$MA -Ddaemon=true -Dclient=true -Ddoxygen=false -Dman=false -Dtests=false -Ddatabase=simple \
	-Dalsa=enabled -Dudev=enabled -Dasyncns=disabled -Davahi=disabled -Dbluez5=disabled -Ddbus=disabled -Delogind=disabled \
	-Dfftw=disabled -Dglib=disabled -Dgsettings=disabled -Dgstreamer=disabled -Dgtk=disabled -Dhal-compat=false -Djack=disabled \
	-Dlirc=disabled -Dopenssl=disabled -Dorc=disabled -Doss-output=disabled -Dsamplerate=disabled -Dsoxr=disabled -Dspeex=disabled \
	-Dsystemd=disabled -Dtcpwrap=disabled -Dvalgrind=disabled -Dx11=disabled -Dwebrtc-aec=disabled \
	-Dbashcompletiondir=no -Dzshcompletiondir=no
ninja -C b
rm -rf "$S/stage"; DESTDIR=$S/stage $MESON install -C b --quiet
