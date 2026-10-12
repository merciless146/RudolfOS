#!/bin/bash
# Build-time header shim for the Xorg server: library headers from the NetBSD X source tree
# (github.com/NetBSD/xsrc, which carries unmodified upstream releases) plus pkg-config files that
# point at the host's runtime libraries, which are the ones copied into Rudolf OS.
# libxcvt is small and not on the host, so it is compiled here and shipped. GPL-2.0-or-later.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
X=$HERE/src/xorg/xsrc/external/mit
P=$HERE/src/xorg/shim
ST=$HERE/src/xorg/stage
L=/usr/lib/x86_64-linux-gnu
MESON="python3 $HERE/src/fp/meson-1.12.1/meson.py"
rm -rf "$P"; mkdir -p "$P/include" "$P/lib/pkgconfig"
pc(){ # name version libname host-so cflags-extra [requires]
	local libs=''; [ -n "$3" ] && { ln -sf "$4" "$P/lib/lib$3.so"; libs="-L\${prefix}/lib -l$3"; }
	cat > "$P/lib/pkgconfig/$1.pc" <<E
prefix=$P
includedir=\${prefix}/include
Name: $1
Description: shim for $1
Version: $2
Requires: $6
Libs: $libs
Cflags: -I\${includedir} $5
E
}
# libdrm (headers 2.4.134; host runtime 2.4.125)
mkdir -p "$P/include/libdrm"
cp "$X/libdrm/dist/xf86drm.h" "$X/libdrm/dist/xf86drmMode.h" "$X/libdrm/dist/libsync.h" "$P/include/"
cp "$X/libdrm/dist/include/drm/"*.h "$P/include/libdrm/"
# NetBSD's xf86drm.h has two "#if 1" blocks for NetBSD atomics (<sys/atomic.h>, atomic_cas_uint);
# turning them off restores the upstream Linux code
sed -i 's/^#if 1$/#if 0/' "$P/include/xf86drm.h"
pc libdrm 2.4.125 drm "$L/libdrm.so.2" "-I\${includedir}/libdrm"
# GBM (header from Mesa 21.3.7; host runtime is Mesa 25.2, which is backward compatible)
cp "$X/MesaLib/dist/src/gbm/main/gbm.h" "$P/include/"; pc gbm 25.2.8 gbm "$L/libgbm.so.1" ""
# Khronos EGL platform headers (from Mesa), needed by the generated epoxy headers
cp -r "$X/MesaLib/dist/include/EGL" "$X/MesaLib/dist/include/KHR" "$P/include/"
# libepoxy (headers generated from the Khronos registry; host runtime 1.5.10)
mkdir -p "$P/include/epoxy"; E=$X/libepoxy/dist
cp "$E/include/epoxy/common.h" "$E/include/epoxy/gl.h" "$E/include/epoxy/egl.h" "$E/include/epoxy/glx.h" "$P/include/epoxy/"
for r in gl egl glx; do python3 "$E/src/gen_dispatch.py" --header --no-source --outputdir="$P/include/epoxy" "$E/registry/$r.xml"; done
pc epoxy 1.5.10 epoxy "$L/libepoxy.so.0" ""
sed -i 's/^Name: epoxy/Name: epoxy\nepoxy_has_glx=1\nepoxy_has_egl=1/' "$P/lib/pkgconfig/epoxy.pc"
# libpciaccess, libxkbfile, libXfont2
cp "$X/libpciaccess/dist/include/pciaccess.h" "$P/include/"; pc pciaccess 0.17 pciaccess "$L/libpciaccess.so.0" ""
mkdir -p "$P/include/X11/extensions" "$P/include/X11/fonts"
cp "$X/libxkbfile/dist/include/X11/extensions/"*.h "$P/include/X11/extensions/"; pc xkbfile 1.1.0 xkbfile "$L/libxkbfile.so.1" "" x11
cp "$X/libXfont2/dist/include/X11/fonts/libxfont2.h" "$P/include/X11/fonts/"; pc xfont2 2.0.6 Xfont2 "$L/libXfont2.so.2" "" "xproto fontsproto"
# libxshmfence: its public interface is these seven calls (struct xshmfence is opaque)
cat > "$P/include/X11/xshmfence.h" <<'E'
#ifndef _XSHMFENCE_H_
#define _XSHMFENCE_H_
#include <X11/Xfuncproto.h>
#define HAVE_STRUCT_XSHMFENCE 1
struct xshmfence;
_X_EXPORT int xshmfence_trigger(struct xshmfence *f);
_X_EXPORT int xshmfence_await(struct xshmfence *f);
_X_EXPORT int xshmfence_query(struct xshmfence *f);
_X_EXPORT void xshmfence_reset(struct xshmfence *f);
_X_EXPORT int xshmfence_alloc_shm(void);
_X_EXPORT struct xshmfence *xshmfence_map_shm(int fd);
_X_EXPORT void xshmfence_unmap_shm(struct xshmfence *f);
#endif
E
pc xshmfence 1.3 xshmfence "$L/libxshmfence.so.1" ""
# libudev 255 (header from systemd, as for the sound server)
cp "$HERE/src/snd/hdr/systemd/src/libudev/libudev.h" "$P/include/"; pc libudev 255 udev "$L/libudev.so.1" ""
# libxcvt 0.1.3: built and installed into the X stage (shipped in Rudolf OS)
(cd "$X/libxcvt/dist" && rm -rf b && $MESON setup b --prefix=/usr --libdir=lib/x86_64-linux-gnu >/dev/null && ninja -C b >/dev/null && DESTDIR=$ST $MESON install -C b --quiet >/dev/null)
cp -r "$ST/usr/include/libxcvt" "$P/include/"
pc libxcvt 0.1.3 xcvt "$ST/usr/lib/x86_64-linux-gnu/libxcvt.so.0" ""
echo "X shim ready: $P"
