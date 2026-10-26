#!/bin/bash
# Assemble the Rudolf OS root file system (later compressed to root.sfs).
# Usage: mkrootfs.sh OUTDIR WWWDIR
# GPL-2.0-or-later.
set -e
OUT=$(realpath -m "$1"); WWW=$(realpath "$2")
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$HERE/src
SYS=$HERE/sys
CHROME=${CHROME_DIR:-/opt/pw-browsers/chromium-1194/chrome-linux}
rm -rf "$OUT"; mkdir -p "$OUT"
cd "$OUT"
mkdir -p bin sbin usr/bin usr/sbin usr/lib usr/share etc dev proc sys run tmp root home/rudolf media mnt live persist opt var/lib/xkb var/log lib64
ln -s usr/lib lib
chmod 1777 tmp

# ---------- copy an ELF file and every library it needs
declare -A DONE
copy_deps() {
	local f
	for f in "$@"; do
		[ -e "$f" ] || continue
		ldd "$f" 2>/dev/null | awk '/=>/ {print $3} /^\t\/lib64/ {print $1}' | while read -r l; do
			[ -n "$l" ] && [ -e "$l" ] || continue
			d="$OUT/usr/lib/x86_64-linux-gnu/${l##*/}"
			case "$l" in /lib64/*) d="$OUT/lib64/${l##*/}";; esac
			[ -e "$d" ] || cp -L "$l" "$d"
		done
	done
}
mkdir -p usr/lib/x86_64-linux-gnu
copy_deps /bin/ls   # glibc + loader first
cp -L /lib64/ld-linux-x86-64.so.2 lib64/
echo /usr/lib/x86_64-linux-gnu > etc/ld.so.conf

# ---------- BusyBox (static) provides the base tools
cp "$SRC/busybox/busybox" bin/busybox
for a in $("$SRC/busybox/busybox" --list-full); do
	[ -e "${a#/}" ] || ln -s /bin/busybox "${a#/}"
done

# ---------- X server, keyboard maps, fonts
for b in /usr/bin/Xvfb /usr/bin/xkbcomp /usr/bin/setxkbmap; do cp -L $b usr/bin/; done
copy_deps /usr/bin/Xvfb /usr/bin/xkbcomp /usr/bin/setxkbmap
mkdir -p usr/share/X11
cp -a /usr/share/X11/xkb usr/share/X11/
cp -a /usr/share/X11/locale usr/share/X11/ 2>/dev/null || true
cp -a /usr/share/X11/XErrorDB usr/share/X11/ 2>/dev/null || true
mkdir -p usr/share/fonts/truetype usr/share/fonts/opentype/noto
cp -a /usr/share/fonts/truetype/dejavu /usr/share/fonts/truetype/liberation usr/share/fonts/truetype/
cp -a /usr/share/fonts/truetype/noto usr/share/fonts/truetype/ 2>/dev/null || true
mkdir -p usr/share/fonts/truetype/freefont && cp /usr/share/fonts/truetype/freefont/FreeSans.ttf /usr/share/fonts/truetype/freefont/FreeSerif.ttf usr/share/fonts/truetype/freefont/   # symbols such as the power sign
cp /usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc /usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc usr/share/fonts/opentype/noto/
cp -a /etc/fonts etc/
mkdir -p usr/share/fontconfig && cp -a /usr/share/fontconfig/conf.avail usr/share/fontconfig/
cp -a /usr/share/alsa usr/share/
mkdir -p usr/share/glib-2.0/schemas
cp /usr/share/glib-2.0/schemas/gschemas.compiled usr/share/glib-2.0/schemas/ 2>/dev/null || true

# ---------- Chromium (locales trimmed to the Rudolf OS languages)
mkdir -p opt/chromium
cp -a "$CHROME"/. opt/chromium/
cd opt/chromium/locales
for f in *.pak; do case $f in en-US.pak|de.pak|es.pak|fr.pak|pt-BR.pak|ru.pak|zh-CN.pak|ja.pak|ko.pak|ar.pak|he.pak) ;; *) rm -f "$f";; esac; done
cd "$OUT"
rm -f opt/chromium/chrome-wrapper opt/chromium/xdg-mime opt/chromium/xdg-settings opt/chromium/locales/*.info
strip opt/chromium/chrome opt/chromium/chrome_crashpad_handler 2>/dev/null || true
find usr/lib/x86_64-linux-gnu -name "*.so*" -type f -exec strip --strip-unneeded {} + 2>/dev/null || true
[ -e opt/chromium/chrome_sandbox ] && chmod 4755 opt/chromium/chrome_sandbox
copy_deps opt/chromium/chrome opt/chromium/chrome_crashpad_handler opt/chromium/libEGL.so opt/chromium/libGLESv2.so opt/chromium/libvk_swiftshader.so
# libraries Chromium opens at run time (file dialogs, NSS)
G=$(ls /usr/lib/x86_64-linux-gnu/libgtk-3.so.0 2>/dev/null || true)
if [ -n "$G" ]; then cp -L "$G" usr/lib/x86_64-linux-gnu/; copy_deps "$G"; fi
cp -a /usr/lib/x86_64-linux-gnu/nss usr/lib/x86_64-linux-gnu/ 2>/dev/null || true
for l in /usr/lib/x86_64-linux-gnu/libsoftokn3.so /usr/lib/x86_64-linux-gnu/libfreeblpriv3.so /usr/lib/x86_64-linux-gnu/libnssckbi.so; do [ -e "$l" ] && cp -L "$l" usr/lib/x86_64-linux-gnu/; done
mkdir -p usr/lib/x86_64-linux-gnu/gdk-pixbuf-2.0
cp -a /usr/lib/x86_64-linux-gnu/gdk-pixbuf-2.0/2.10.0 usr/lib/x86_64-linux-gnu/gdk-pixbuf-2.0/ 2>/dev/null || true
for l in usr/lib/x86_64-linux-gnu/gdk-pixbuf-2.0/2.10.0/loaders/*.so; do [ -e "$l" ] && copy_deps "$l"; done
# libraries Chromium dlopens for X11 (missing them makes the browser abort at start-up)
for l in libX11-xcb.so.1 libxcb-randr.so.0 libxcb-xfixes.so.0 libxcb-present.so.0 libxcb-sync.so.1 libxcb-dri3.so.0 libxshmfence.so.1 libXxf86vm.so.1 libva.so.2 libva-drm.so.2 libxcb-shm.so.0 libxcb-render.so.0 libxcb-xkb.so.1 libxkbcommon-x11.so.0 libXss.so.1 libXinerama.so.1 libXcursor.so.1 libXrender.so.1; do
	f=/usr/lib/x86_64-linux-gnu/$l; [ -e "$f" ] || continue
	[ -e "usr/lib/x86_64-linux-gnu/$l" ] || cp -L "$f" usr/lib/x86_64-linux-gnu/; copy_deps "$f"
done
mkdir -p usr/share/icons/Adwaita usr/share/icons/default
cp -a /usr/share/icons/Adwaita/cursors usr/share/icons/Adwaita/ 2>/dev/null || true
printf '[Icon Theme]\nName=Adwaita\n' > usr/share/icons/Adwaita/index.theme
printf '[Icon Theme]\nInherits=Adwaita\n' > usr/share/icons/default/index.theme
cp -a /usr/share/zoneinfo usr/share/ 2>/dev/null || true
mkdir -p usr/share/locale && cp /usr/share/locale/locale.alias usr/share/locale/ 2>/dev/null || true
mkdir -p etc/gtk-3.0 && printf '[Settings]\ngtk-cursor-theme-name=Adwaita\ngtk-font-name=DejaVu Sans 10\n' > etc/gtk-3.0/settings.ini
mkdir -p etc/ssl/certs && cp -L /etc/ssl/certs/ca-certificates.crt etc/ssl/certs/ 2>/dev/null || true

# ---------- disk tools for the installer
cp -L /usr/sbin/mke2fs usr/sbin/ && copy_deps /usr/sbin/mke2fs
cp -a /etc/mke2fs.conf etc/ 2>/dev/null || true

# ---------- Rudolf OS system programs and scripts
gcc -O2 -o usr/bin/rudolf-display "$SYS/rudolf-display.c" -lX11 /usr/lib/x86_64-linux-gnu/libXtst.so.6 /usr/lib/x86_64-linux-gnu/libXfixes.so.3 /usr/lib/x86_64-linux-gnu/libXrandr.so.2 -lm
gcc -O2 -o usr/bin/rudolf-launch "$SYS/rudolf-launch.c"
gcc -O2 -o usr/bin/rudolfd "$SYS/rudolfd.c" -Wl,-Bstatic -lz -Wl,-Bdynamic
gcc -O2 -o usr/bin/rpk "$SYS/rpk.c" -lcrypto
gcc -O2 -o usr/bin/rudolf-update "$SYS/rudolf-update.c" -lcrypto -ldl
gcc -O2 -o usr/bin/rudolf-print "$SYS/rudolf-print.c" -ldl
gcc -O2 -o usr/bin/rudolf-apps "$SYS/rudolf-apps.c" -lz
gcc -O2 -o usr/bin/rudolf-debian "$SYS/rudolf-debian.c" -lcrypto -ldl
gcc -O2 -o usr/bin/rudolf-fscheck "$SYS/rudolf-fscheck.c"
gcc -O2 -o usr/bin/rudolf-ai "$SYS/rudolf-ai.c" -lcrypto -ldl
# Blitzen's model engine (systools/mkllama.sh: llama.cpp's llama-server, static); the model is downloaded later
mkdir -p usr/lib/rudolf; [ -x "$SRC/llama-build/bin/llama-server" ] && cp "$SRC/llama-build/bin/llama-server" usr/lib/rudolf/
gcc -O2 -o usr/bin/rudolf-portal "$SYS/rudolf-portal.c" $(pkg-config --cflags --libs gio-2.0 gio-unix-2.0)
# speech: eSpeak NG 1.52.0 (built from src/espeak-ng, see BUILD.TXT) with the nine desktop languages only
ES=$SRC/espeak-ng/inst/usr
if [ -x $ES/bin/espeak-ng ]; then
	cp $ES/bin/espeak-ng usr/bin/; D=$ES/share/espeak-ng-data; mkdir -p usr/share/espeak-ng-data
	cp -a $D/phontab $D/phonindex $D/phondata $D/intonations $D/lang $D/voices usr/share/espeak-ng-data/
	for l in en es pt fr de ru cmn ja ko; do cp $D/${l}_dict usr/share/espeak-ng-data/; done
fi
# where this build looks for system updates (set UPDATE_URL when building releases)
mkdir -p etc/rudolf; printf "%s\n" "${UPDATE_URL:-}" > etc/rudolf/update-url
gcc -O2 -static -o usr/bin/cdp-eval "$SYS/cdp-eval.c"
cp -L /usr/lib/x86_64-linux-gnu/libXtst.so.6 usr/lib/x86_64-linux-gnu/
copy_deps usr/bin/rudolf-display usr/bin/rudolfd usr/bin/rpk usr/bin/rudolf-update usr/bin/rudolf-apps usr/bin/rudolf-portal usr/bin/rudolf-debian usr/bin/rudolf-ai
cp -a "$SYS/root/." .
mkdir -p usr/share/rudolf
cp -a "$WWW" usr/share/rudolf/www
[ -f "$HERE/VERSION" ] && cp "$HERE/VERSION" usr/share/rudolf/VERSION

# ---------- kernel modules (Wi-Fi and other optional drivers)
if [ -d "$HERE/modules/lib/modules" ]; then cp -a "$HERE/modules/lib/modules" usr/lib/; fi
KV=$(ls usr/lib/modules 2>/dev/null | head -1); [ -n "$KV" ] && chroot "$OUT" /bin/busybox depmod "$KV" || true
if [ -d "$HERE/firmware" ]; then mkdir -p usr/lib/firmware && cp -a "$HERE/firmware/." usr/lib/firmware/; fi
# Wi-Fi: wpa_supplicant + wpa_cli (wifibuild.sh), the firmware the Wi-Fi modules ask for, and the signed
# wireless regulatory database (which channels and power each country allows)
if [ -x "$SRC/wifi/wpa_supplicant" ]; then cp "$SRC/wifi/wpa_supplicant" "$SRC/wifi/wpa_cli" usr/sbin/; copy_deps usr/sbin/wpa_supplicant; fi
if [ -d "$SRC/linux-firmware" ] && [ -d "$HERE/modules" ]; then python3 "$HERE/systools/wififw.py" "$HERE/modules" "$SRC/linux-firmware" usr/lib/firmware >/dev/null; fi
if [ -f "$SRC/wireless-regdb/regulatory.db.p7s" ]; then mkdir -p usr/lib/firmware usr/share/doc/rudolf/wireless-regdb; cp "$SRC/wireless-regdb/regulatory.db" "$SRC/wireless-regdb/regulatory.db.p7s" usr/lib/firmware/; cp "$SRC/wireless-regdb/LICENSE" usr/share/doc/rudolf/wireless-regdb/ 2>/dev/null || true; fi

# ---------- graphics firmware for the GPU drivers (Intel, AMD, NVIDIA open driver)
FW=$SRC/linux-firmware
if [ -d "$FW" ]; then
	mkdir -p usr/lib/firmware usr/share/doc/rudolf/firmware
	for d in i915 xe amdgpu radeon nvidia; do [ -d "$FW/$d" ] && cp -a "$FW/$d" usr/lib/firmware/; done
	cp "$FW"/WHENCE "$FW"/LICENSE* "$FW"/LICENCE* usr/share/doc/rudolf/firmware/ 2>/dev/null || true
fi

# ---------- native Linux apps: Flatpak 1.18 + OSTree + bubblewrap (built by build_iso.sh), D-Bus, GnuPG
FPS=$SRC/fp/stage
if [ -x "$FPS/usr/bin/flatpak" ]; then
	cp -a "$FPS/usr/bin/flatpak" "$FPS/usr/bin/bwrap" "$FPS/usr/bin/ostree" usr/bin/
	mkdir -p usr/libexec usr/share/dbus-1 etc/profile.d etc/flatpak/remotes.d var/lib/flatpak
	cp -a "$FPS/usr/libexec/." usr/libexec/
	cp -a "$FPS/usr/lib/x86_64-linux-gnu/"libflatpak.so* "$FPS/usr/lib/x86_64-linux-gnu/"libostree-1.so* usr/lib/x86_64-linux-gnu/
	cp -a "$FPS/usr/share/flatpak" usr/share/
	cp -a "$FPS/usr/share/dbus-1/." usr/share/dbus-1/
	cp -a "$FPS/etc/profile.d/." etc/profile.d/ 2>/dev/null || true
	copy_deps usr/bin/flatpak usr/bin/bwrap usr/bin/ostree usr/libexec/flatpak-* usr/libexec/revokefs-fuse \
		usr/lib/x86_64-linux-gnu/libflatpak.so.0 usr/lib/x86_64-linux-gnu/libostree-1.so.1
fi
for b in dbus-daemon dbus-uuidgen dbus-send dbus-run-session gpg gpgv gpgconf gpg-agent fusermount3; do
	[ -x /usr/bin/$b ] && cp -L /usr/bin/$b usr/bin/ && copy_deps usr/bin/$b
done
mkdir -p usr/share/dbus-1 var/lib/dbus
cp /usr/share/dbus-1/session.conf /usr/share/dbus-1/system.conf usr/share/dbus-1/ 2>/dev/null || true
# GLib looks for these schemas/modules only optionally; the shared-mime database helps app icons
[ -d /usr/share/mime ] && [ ! -d usr/share/mime ] && cp -a /usr/share/mime usr/share/ || true

# ---------- Xorg on the graphics card (built by xbuild.sh) + Mesa for glamor and DRI3 apps
XS=$SRC/xorg/stage
if [ -x "$XS/usr/bin/Xorg" ]; then
	cp -a "$XS/usr/bin/Xorg" usr/bin/
	mkdir -p usr/lib/xorg var/lib/xkb
	cp -a "$XS/usr/lib/xorg/modules" usr/lib/xorg/
	cp -a "$XS/usr/lib/x86_64-linux-gnu/"libxcvt.so* usr/lib/x86_64-linux-gnu/
	# Mesa 25 (all GPU drivers are in libgallium; dri/*.so and gbm/dri_gbm.so are small loaders)
	ML=/usr/lib/x86_64-linux-gnu
	mkdir -p usr/lib/x86_64-linux-gnu/dri usr/lib/x86_64-linux-gnu/gbm usr/share/glvnd/egl_vendor.d usr/share/drirc.d
	cp -a $ML/dri/. usr/lib/x86_64-linux-gnu/dri/
	cp -a $ML/gbm/dri_gbm.so usr/lib/x86_64-linux-gnu/gbm/
	cp -L $ML/libEGL.so.1 $ML/libEGL_mesa.so.0 $ML/libGLdispatch.so.0 $ML/libgbm.so.1 $ML/libepoxy.so.0 usr/lib/x86_64-linux-gnu/
	for l in $ML/libgallium-*.so; do cp -L "$l" usr/lib/x86_64-linux-gnu/; done
	cp /usr/share/glvnd/egl_vendor.d/50_mesa.json usr/share/glvnd/egl_vendor.d/
	cp -a /usr/share/drirc.d/. usr/share/drirc.d/ 2>/dev/null || true
	copy_deps usr/bin/Xorg usr/lib/xorg/modules/*.so usr/lib/xorg/modules/drivers/*.so usr/lib/x86_64-linux-gnu/libgallium-*.so \
		usr/lib/x86_64-linux-gnu/dri/libdril_dri.so usr/lib/x86_64-linux-gnu/gbm/dri_gbm.so usr/lib/x86_64-linux-gnu/libEGL_mesa.so.0 \
		usr/lib/x86_64-linux-gnu/libEGL.so.1 usr/lib/x86_64-linux-gnu/libepoxy.so.0
fi

# ---------- sound server: PulseAudio 16.1 (built by sndbuild.sh)
SNS=$SRC/snd/stage
if [ -x "$SNS/usr/bin/pulseaudio" ]; then
	cp -a "$SNS/usr/bin/pulseaudio" "$SNS/usr/bin/pactl" "$SNS/usr/bin/pacat" usr/bin/
	ln -sf pacat usr/bin/paplay; ln -sf pacat usr/bin/parecord
	mkdir -p usr/lib/x86_64-linux-gnu etc/pulse usr/share/pulseaudio
	cp -a "$SNS/usr/lib/x86_64-linux-gnu/"libpulse.so* "$SNS/usr/lib/x86_64-linux-gnu/"libpulse-simple.so* usr/lib/x86_64-linux-gnu/
	cp -a "$SNS/usr/lib/x86_64-linux-gnu/pulseaudio" usr/lib/x86_64-linux-gnu/
	cp -a "$SNS/etc/pulse/." etc/pulse/
	# only the session starts the sound server
	printf "\nautospawn = no\n" >> etc/pulse/client.conf
	cp -a "$SNS/usr/share/pulseaudio/." usr/share/pulseaudio/
	copy_deps usr/bin/pulseaudio usr/bin/pactl usr/bin/pacat usr/lib/x86_64-linux-gnu/pulseaudio/*.so usr/lib/x86_64-linux-gnu/pulseaudio/modules/*.so
fi
[ -d /usr/share/alsa ] && [ ! -d usr/share/alsa/ucm2 ] && cp -a /usr/share/alsa/. usr/share/alsa/ 2>/dev/null || true

# ---------- configuration
cat > etc/passwd <<'E'
root:x:0:0:root:/root:/bin/sh
rudolf:x:1000:1000:Rudolf OS:/home/rudolf:/bin/sh
messagebus:x:100:102::/nonexistent:/bin/false
ai:x:950:950:Blitzen model engine:/persist/ai/cache:/bin/false
nobody:x:65534:65534:nobody:/:/bin/false
E
cat > etc/group <<'E'
root:x:0:
tty:x:5:
audio:x:29:rudolf
video:x:44:rudolf
input:x:101:
rudolf:x:1000:
messagebus:x:102:
ai:x:950:
nogroup:x:65534:
E
echo 'root:*:19000:0:99999:7:::' > etc/shadow; echo 'rudolf:*:19000:0:99999:7:::' >> etc/shadow; chmod 600 etc/shadow
echo rudolf > etc/hostname
printf '127.0.0.1 localhost rudolf\n::1 localhost\n' > etc/hosts
printf 'hosts: files dns\npasswd: files\ngroup: files\n' > etc/nsswitch.conf
: > etc/resolv.conf
printf 'export PATH=/usr/sbin:/usr/bin:/sbin:/bin\nexport LANG=C.UTF-8\n' > etc/profile
chown -R 1000:1000 home/rudolf
ldconfig -r "$OUT" 2>/dev/null || true
# fontconfig cache so the first start does not scan every font
mkdir -p var/cache/fontconfig; fc-cache -s -y "$OUT" >/dev/null 2>&1 || true
# ---------- record which distribution packages the copied files came from (for licences / source offer)
python3 "$HERE/pkgmanifest.py" "$OUT" "$OUT/usr/share/doc/rudolf"
node "$HERE/credits.js" "$CHROME/resources.pak" usr/share/doc/rudolf/chromium-credits.txt >/dev/null
echo "Rudolf OS root file system: $(du -sh "$OUT" | cut -f1)"
