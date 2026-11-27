#!/bin/bash
# Add WebKitGTK (the desktop's Lite engine, System Settings › Desktop engine; 1.5.7, plan Part 4) to a base layer
# tree, from this Ubuntu 24.04 (apt-get install libwebkit2gtk-4.1-dev gstreamer1.0-plugins-base
# gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-libav glib-networking):   addwebkit.sh ROOT BINARY [NATIVE]
# BINARY is rudolf-webkit, built here against libwebkit2gtk-4.1 (1.5.9: and NATIVE, rudolf-native, the native desktop
# engine's prototype, plan Part 12, built against the same; it opens WebKit's libwebp itself, which WebKit brings). Copied into ROOT: the binary (usr/bin), WebKit's
# helper processes and injected bundle (usr/lib/MULTIARCH/webkit2gtk-4.1), the GStreamer plug-ins the
# desktop's media need (sound, Ogg, WebM, MP4 containers, WAV; no H.264 or AAC: Chromium plays those, the Lite engine
# not yet) and its plug-in scanner, GLib's TLS module (https for the desktop's own requests, such as the weather),
# GTK's settings schemas (compiled) and the image loaders; then every library any of them needs that ROOT lacks.
# Prints the size added. Fails when WebKitGTK is not installed here. GPL-2.0-or-later.
set -e
MA=${MA:-$(gcc -print-multiarch)}; [ -n "$MA" ] || MA=x86_64-linux-gnu   # (1.5.9: the build machine's library folder; aarch64-linux-gnu on ARM64)
R=$1; BIN=$2; NAT=$3; [ -d "$R/usr/lib/$MA" ] && [ -x "$BIN" ] && { [ -z "$NAT" ] || [ -x "$NAT" ]; } || { echo "usage: $0 ROOT BINARY [NATIVE]"; exit 1; }
ML=/usr/lib/$MA; WK=$ML/webkit2gtk-4.1
[ -x "$WK/WebKitWebProcess" ] && [ -x "$WK/WebKitNetworkProcess" ] || { echo "addwebkit: WebKitGTK 4.1 is not installed here (apt-get install libwebkit2gtk-4.1-dev)"; exit 1; }
SUDO=; [ -w "$R/usr/lib/$MA" ] || SUDO=sudo
B0=$(du -sk "$R" | cut -f1)
LIST=$(mktemp)
$SUDO install -m 0755 "$BIN" "$R/usr/bin/rudolf-webkit"; echo "$BIN" >> "$LIST"
[ -n "$NAT" ] && { $SUDO install -m 0755 "$NAT" "$R/usr/bin/rudolf-native"; echo "$NAT" >> "$LIST"; }
# WebKit's helper processes and its injected bundle
$SUDO mkdir -p "$R$WK"; $SUDO cp -a "$WK/." "$R$WK/"; find "$WK" -type f >> "$LIST"
# GStreamer: the core's plug-in scanner and the plug-ins the desktop's sound and video need.
# 1.5.8: and, from gst-plugins-bad, the parsers WebKit's streaming video (Media Source: YouTube) needs: without opusparse
# WebKit offers no Opus there, so YouTube (VP9 with Opus) said "Your browser can't play this video" (the Vivobook,
# Lite); vp9parse (videoparsersbad, with h264parse and av1parse) completes the video's codec. scaletempo (audiofx) for
# playback speed, fakevideosink (debugutilsbad) and the WebVTT encoder (subenc) for subtitles, which WebKit asked for.
# And FFmpeg's decoders (libav: H.264, AAC, MP3, as Chromium has them) and deinterlace: on the Vivobook YouTube stopped
# ("Something went wrong") when it played MP4 with AAC or H.264 (and MP3 sounds): "missing a plug-in" (2026-11-12).
# 1.6.0: and "inter" (gst-plugins-bad), which WebKit's audio mixer needs: the Native desktop mixes each window's sounds in it, as
# Chromium does, instead of a PulseAudio stream for each media element whose volume PulseAudio kept (the owner's report 2).
# 1.6.3: and "interleave" (gstreamer1.0-plugins-good), whose `deinterleave` AudioSourceProviderGStreamer splits a capture
# into WebAudio's channels with: without it every AudioContext that reads the microphone (the check's level meter, Settings'
# mic test) ends "not-linked" (findings N35; "deinterlace" and "inter" are different plug-ins and were not it). And
# "transcode" (gstreamer1.0-plugins-bad), whose `uritranscodebin` MediaRecorderPrivateGStreamer builds its recording
# pipeline with: without it MediaRecorder cannot be created, `rec.start()` throws NotSupportedError and the Recorder's
# button stays dead (N35 point 3 -- a fault of its own, not the microphone's).
# 1.6.3: and "pipewire" (gstreamer1.0-pipewire). On the Vivobook's build (2026-11-27; checklist V2 and V3) the Native
# desktop's WebKit could not capture at all -- Camera said "No camera is available or access is blocked" with the privacy
# switch on, and the microphone gave only zeros. This WebKitGTK captures through **PipeWire alone**: its library names
# "pipewiresrc" and "pipewiredeviceprovider" and its classes are PipeWireCaptureDeviceManager and "WebKit PipeWire Capture
# Device Manager", it has OpenPipeWireRemote, and it carries "PipeWire Device Provider is missing or too old. Please
# install PipeWire >= 0.3.64." -- while "v4l2src" and "v4l2deviceprovider" appear nowhere in it (devlink 000652, 000653;
# N33 and N35 in docs/native-findings-2026-11-26.md). "video4linux2" was added here on 2026-11-27 and taken out again the
# same day: nothing would ever request it. The plug-in alone is still not enough -- the image also needs libpipewire-0.3
# and a running PipeWire with WirePlumber (docs/plan-1.6.3.md Part 9).
# The two the desktop cannot do without: an image built without them ships a silent microphone and a Recorder whose button
# does nothing, and every host that builds an image installs their packages already (above), so a host without them is the
# wrong host and stops the build -- the warning alone is what let a release go out with no capture path. "pipewire" is not
# in this list yet: its plug-in would have no sound server to reach until Part 9 gives the image one, so today its absence
# on the build host is the normal state and stays a warning (it joins this list with the server).
FATAL=" interleave transcode "; fatal=
GS=$ML/gstreamer-1.0; $SUDO mkdir -p "$R$GS" "$R$ML/gstreamer1.0/gstreamer-1.0"
for s in $ML/gstreamer1.0/gstreamer-1.0/gst-plugin-scanner; do [ -x "$s" ] && $SUDO cp -L "$s" "$R$s" && echo "$s" >> "$LIST"; done
# One loop copies what it finds and says what is lost for what it does not: a plug-in named here and missing on the
# build host used to disappear without a word, which is how a release shipped with no capture path at all.
for p in coreelements app audioconvert audioresample audiotestsrc volume playback typefindfunctions videoconvertscale videoscale videoconvert \
	ogg vorbis opus theora vpx matroska isomp4 wavparse audioparsers autodetect pulseaudio alsa pipewire id3demux gio rawparse videorate videofilter \
	audiomixer compositor opengl gdkpixbuf jpeg png webrtcdsp \
	opusparse videoparsersbad audiofx debugutilsbad subenc libav deinterlace inter interleave transcode; do
	f=$GS/libgst$p.so
	if [ -f "$f" ]; then $SUDO cp -L "$f" "$R$f"; echo "$f" >> "$LIST"; continue; fi
	case $p in
	pipewire) why="the Native desktop cannot capture from the camera (apt-get install gstreamer1.0-pipewire)";;
	interleave) why="the Native desktop's level meters get no microphone audio (apt-get install gstreamer1.0-plugins-good)";;
	transcode) why="the Native desktop's Recorder cannot start, because MediaRecorder builds uritranscodebin (apt-get install gstreamer1.0-plugins-bad)";;
	*) why="the desktop's media may lack it";;
	esac
	echo "addwebkit: no libgst$p.so: $why"
	case $FATAL in *" $p "*) fatal=1;; esac
done
[ -z "$fatal" ] || { echo "addwebkit: stopping: a plug-in the desktop cannot do without is missing (see above; apt-get install"
	"gstreamer1.0-plugins-good gstreamer1.0-plugins-bad). A build without it would ship the desktop with a silent microphone or"
	"a Recorder that never starts, so this build is refused rather than made and posted." >&2; exit 1; }
# (1.5.9, plan 7.1: no WebRTC plug-ins: Ubuntu's WebKitGTK 2.52 has no WebRTC, so Lite opens call sites in Chromium's
# window; the Lite engine check's call test says whether a newer WebKitGTK has it)
# GLib: the TLS module (https), with its loader cache made for ROOT
for m in $ML/gio/modules/libgiognutls.so; do [ -f "$m" ] && { $SUDO mkdir -p "$R${m%/*}"; $SUDO cp -L "$m" "$R$m"; echo "$m" >> "$LIST"; }; done
# 1.5.7: the libraries WebKit opens itself while running, which no ldd sees. libGLESv2.so.2 (GLVND's OpenGL ES, from
# libgles2): without it WebKit's web process aborts as it starts ("Couldn't open libGLESv2.so.2"), and the Lite desktop
# closed every few seconds on the Vivobook (the release machine and Debian had it; the base layer did not). With it,
# WebKit draws with the GPU when GTK can, and in software otherwise. libdrm's list of AMD GPUs too (only a warning).
# 1.5.8: and Mesa's GLX (libGLX_mesa.so.0, from libglx-mesa0, with libxcb-glx.so.0): GTK 3 checks OpenGL through GLX, and
# without Mesa's GLX it found none ("No available configurations for the given pixel format"), so WebKit turned its
# hardware acceleration off on every computer ("Disabled hardware acceleration because GTK failed to initialize GL").
for l in libGLESv2.so.2 libEGL.so.1 libGLdispatch.so.0 libgbm.so.1 libepoxy.so.0 libGLX_mesa.so.0 libxcb-glx.so.0; do
	f=$(readlink -f $ML/$l 2>/dev/null) || continue; [ -f "$f" ] || { echo "addwebkit: $ML/$l is missing here (apt-get install libgles2 libegl1 libgbm1 libglx-mesa0)"; exit 1; }
	[ -e "$R$f" ] || { $SUDO cp -L "$f" "$R$f"; echo "$f" >> "$LIST"; }; [ -e "$R$ML/$l" ] || $SUDO ln -sf "${f##*/}" "$R$ML/$l"; done
for d in /usr/share/libdrm; do [ -d "$d" ] && { $SUDO mkdir -p "$R$d"; $SUDO cp -a "$d/." "$R$d/"; }; done
# GTK 3's settings schemas (it reads them at start), compiled in ROOT
$SUDO mkdir -p "$R/usr/share/glib-2.0/schemas"
for s in /usr/share/glib-2.0/schemas/org.gtk.Settings.*.gschema.xml /usr/share/glib-2.0/schemas/org.gnome.desktop.interface.gschema.xml /usr/share/glib-2.0/schemas/gschema.dtd; do
	[ -f "$s" ] && $SUDO cp "$s" "$R/usr/share/glib-2.0/schemas/"; done
$SUDO glib-compile-schemas "$R/usr/share/glib-2.0/schemas"
# the image loaders (GTK's own icons), with their cache rewritten for ROOT's paths (the same as here)
PB=$(ls -d $ML/gdk-pixbuf-2.0/2.10.0 2>/dev/null || true)
if [ -n "$PB" ]; then $SUDO mkdir -p "$R$PB"; $SUDO cp -a "$PB/." "$R$PB/"; find "$PB/loaders" -name '*.so' >> "$LIST"; fi
# every library they need that ROOT lacks
n=0; while read -r f; do
	for l in $(ldd "$f" 2>/dev/null | awk '/=>/{print $3}'); do
		[ -e "$R$l" ] || [ -e "$R/usr$l" ] || [ -e "$R$ML/${l##*/}" ] || { $SUDO cp -L "$l" "$R$ML/"; n=$((n+1)); echo "$R$ML/${l##*/}" >> "$LIST.more"; }
	done
done < "$LIST"
# (and what those new libraries need in turn)
while [ -s "$LIST.more" ]; do mv "$LIST.more" "$LIST.now"; while read -r f; do
	for l in $(ldd "$f" 2>/dev/null | awk '/=>/{print $3}'); do
		[ -e "$R$l" ] || [ -e "$R/usr$l" ] || [ -e "$R$ML/${l##*/}" ] || { $SUDO cp -L "$l" "$R$ML/"; n=$((n+1)); echo "$R$ML/${l##*/}" >> "$LIST.more"; }
	done; done < "$LIST.now"; rm -f "$LIST.now"; done
rm -f "$LIST" "$LIST.more"
echo "addwebkit: WebKitGTK $(pkg-config --modversion webkit2gtk-4.1 2>/dev/null) added, with $n libraries; $(( ($(du -sk "$R" | cut -f1) - B0) / 1024 )) MB"
