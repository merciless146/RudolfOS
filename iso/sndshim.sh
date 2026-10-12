#!/bin/bash
# Build-time header shim for the sound server: upstream headers matching the host's runtime
# libraries (ALSA 1.2.11, libsndfile 1.2.2, libudev 255) plus pkg-config files pointing at those
# host libraries, which are the same ones copied into Rudolf OS. GPL-2.0-or-later.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
H=$HERE/src/snd/hdr
P=$HERE/src/snd/shim
L=/usr/lib/x86_64-linux-gnu
rm -rf "$P"; mkdir -p "$P/include" "$P/lib/pkgconfig"
pc(){ # name version libname host-so [requires]
	ln -sf "$L/$4" "$P/lib/lib$3.so"
	cat > "$P/lib/pkgconfig/$1.pc" <<E
prefix=$P
Name: $1
Description: shim for $1
Version: $2
Requires: $5
Libs: -L\${prefix}/lib -l$3
Cflags: -I\${prefix}/include
E
}
# ALSA 1.2.11
A=$P/include/alsa; mkdir -p "$A/sound"
cp "$H/alsa/include/"*.h "$A/"; cp -r "$H/alsa/include/sound/." "$A/sound/" 2>/dev/null || true
rm -f "$A/local.h" "$A/alsa-symbols.h" "$A/aserver.h" "$A/bswap.h" "$A/search.h" "$A/list.h"
cat > "$A/version.h" <<'E'
#define SND_LIB_MAJOR		1
#define SND_LIB_MINOR		2
#define SND_LIB_SUBMINOR	11
#define SND_LIB_EXTRAVER	1000000
#define SND_LIB_VER(maj, min, sub) (((maj)<<16)|((min)<<8)|(sub))
#define SND_LIB_VERSION SND_LIB_VER(SND_LIB_MAJOR, SND_LIB_MINOR, SND_LIB_SUBMINOR)
#define SND_LIB_VERSION_STR	"1.2.11"
E
{ cat "$H/alsa/include/asoundlib-head.h"; echo '#include <endian.h>'
  printf '\n#ifndef DOC_HIDDEN\n#ifndef __GNUC__\n#define __inline__ inline\n#endif\n#endif\n\n'
  for h in asoundef version global input output error conf pcm rawmidi ump timer hwdep control mixer seq_event seq seqmid seq_midi_event; do echo "#include <alsa/$h.h>"; done
  cat "$H/alsa/include/asoundlib-tail.h"; } > "$A/asoundlib.h"
pc alsa 1.2.11 asound libasound.so.2
# libsndfile 1.2.2
cp "$H/sndfile/include/sndfile.h" "$P/include/"; pc sndfile 1.2.2 sndfile libsndfile.so.1
# libudev 255
cp "$H/systemd/src/libudev/libudev.h" "$P/include/"; pc libudev 255 udev libudev.so.1
# libltdl 2.4.7 (PulseAudio's module loader)
cp "$H/libtool/libltdl/ltdl.h" "$P/include/"; mkdir -p "$P/include/libltdl"
cp "$H/libtool/libltdl/libltdl/lt_system.h" "$H/libtool/libltdl/libltdl/lt_error.h" "$H/libtool/libltdl/libltdl/lt_dlloader.h" "$P/include/libltdl/"
ln -sf "$L/libltdl.so.7" "$P/lib/libltdl.so"
echo "sound shim ready: $P"
