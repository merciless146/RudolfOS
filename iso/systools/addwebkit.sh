#!/bin/bash
# Add WebKitGTK (the desktop's Lite engine, System Settings › Desktop engine; 1.5.7, plan Part 4) to a base layer
# tree, from this Ubuntu 24.04 (apt-get install libwebkit2gtk-4.1-dev gstreamer1.0-plugins-base
# gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-libav glib-networking):   addwebkit.sh ROOT BINARY
# BINARY is rudolf-webkit, built here against libwebkit2gtk-4.1. Copied into ROOT: the binary (usr/bin), WebKit's
# helper processes and injected bundle (usr/lib/x86_64-linux-gnu/webkit2gtk-4.1), the GStreamer plug-ins the
# desktop's media need (sound, Ogg, WebM, MP4 containers, WAV; no H.264 or AAC: Chromium plays those, the Lite engine
# not yet) and its plug-in scanner, GLib's TLS module (https for the desktop's own requests, such as the weather),
# GTK's settings schemas (compiled) and the image loaders; then every library any of them needs that ROOT lacks.
# Prints the size added. Fails when WebKitGTK is not installed here. GPL-2.0-or-later.
set -e
R=$1; BIN=$2; [ -d "$R/usr/lib/x86_64-linux-gnu" ] && [ -x "$BIN" ] || { echo "usage: $0 ROOT BINARY"; exit 1; }
ML=/usr/lib/x86_64-linux-gnu; WK=$ML/webkit2gtk-4.1
[ -x "$WK/WebKitWebProcess" ] && [ -x "$WK/WebKitNetworkProcess" ] || { echo "addwebkit: WebKitGTK 4.1 is not installed here (apt-get install libwebkit2gtk-4.1-dev)"; exit 1; }
SUDO=; [ -w "$R/usr/lib/x86_64-linux-gnu" ] || SUDO=sudo
B0=$(du -sk "$R" | cut -f1)
LIST=$(mktemp)
$SUDO install -m 0755 "$BIN" "$R/usr/bin/rudolf-webkit"; echo "$BIN" >> "$LIST"
# WebKit's helper processes and its injected bundle
$SUDO mkdir -p "$R$WK"; $SUDO cp -a "$WK/." "$R$WK/"; find "$WK" -type f >> "$LIST"
# GStreamer: the core's plug-in scanner and the plug-ins the desktop's sound and video need.
# 1.5.8: and, from gst-plugins-bad, the parsers WebKit's streaming video (Media Source: YouTube) needs: without opusparse
# WebKit offers no Opus there, so YouTube (VP9 with Opus) said "Your browser can't play this video" (the Vivobook,
# Lite); vp9parse (videoparsersbad, with h264parse and av1parse) completes the video's codec. scaletempo (audiofx) for
# playback speed, fakevideosink (debugutilsbad) and the WebVTT encoder (subenc) for subtitles, which WebKit asked for.
# And FFmpeg's decoders (libav: H.264, AAC, MP3, as Chromium has them) and deinterlace: on the Vivobook YouTube stopped
# ("Something went wrong") when it played MP4 with AAC or H.264 (and MP3 sounds): "missing a plug-in" (2026-11-12).
GS=$ML/gstreamer-1.0; $SUDO mkdir -p "$R$GS" "$R$ML/gstreamer1.0/gstreamer-1.0"
for s in $ML/gstreamer1.0/gstreamer-1.0/gst-plugin-scanner; do [ -x "$s" ] && $SUDO cp -L "$s" "$R$s" && echo "$s" >> "$LIST"; done
for p in coreelements app audioconvert audioresample audiotestsrc volume playback typefindfunctions videoconvertscale videoscale videoconvert \
	ogg vorbis opus theora vpx matroska isomp4 wavparse audioparsers autodetect pulseaudio alsa id3demux gio rawparse videorate videofilter \
	audiomixer compositor opengl gdkpixbuf jpeg png webrtcdsp \
	opusparse videoparsersbad audiofx debugutilsbad subenc libav deinterlace; do
	f=$GS/libgst$p.so; [ -f "$f" ] || continue; $SUDO cp -L "$f" "$R$f"; echo "$f" >> "$LIST"; done
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
