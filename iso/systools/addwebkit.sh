#!/bin/bash
# Add WebKitGTK (the desktop's Lite engine, System Settings › Desktop engine; 1.5.7, plan Part 4) to a base layer
# tree, from this Ubuntu 24.04 (apt-get install libwebkit2gtk-4.1-dev gstreamer1.0-plugins-base
# gstreamer1.0-plugins-good glib-networking):   addwebkit.sh ROOT BINARY
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
# GStreamer: the core's plug-in scanner and the plug-ins the desktop's sound and video need
GS=$ML/gstreamer-1.0; $SUDO mkdir -p "$R$GS" "$R$ML/gstreamer1.0/gstreamer-1.0"
for s in $ML/gstreamer1.0/gstreamer-1.0/gst-plugin-scanner; do [ -x "$s" ] && $SUDO cp -L "$s" "$R$s" && echo "$s" >> "$LIST"; done
for p in coreelements app audioconvert audioresample audiotestsrc volume playback typefindfunctions videoconvertscale videoscale videoconvert \
	ogg vorbis opus theora vpx matroska isomp4 wavparse audioparsers autodetect pulseaudio alsa id3demux gio rawparse videorate videofilter \
	audiomixer compositor opengl gdkpixbuf jpeg png webrtcdsp; do
	f=$GS/libgst$p.so; [ -f "$f" ] || continue; $SUDO cp -L "$f" "$R$f"; echo "$f" >> "$LIST"; done
# GLib: the TLS module (https), with its loader cache made for ROOT
for m in $ML/gio/modules/libgiognutls.so; do [ -f "$m" ] && { $SUDO mkdir -p "$R${m%/*}"; $SUDO cp -L "$m" "$R$m"; echo "$m" >> "$LIST"; }; done
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
