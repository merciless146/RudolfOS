#!/bin/bash
# The ARM64 base layer (1.5.9, docs/plan-1.5.9.md Part 3, step 2), built from scratch on an ARM64 machine (GitHub's
# ubuntu-24.04-arm) with the same scripts as x86: BusyBox, the X server (xfetch.sh, xbuild.sh, at the release's X
# source commit), the sound server (PulseAudio 16.1, sndbuild.sh), the Wi-Fi tools (wifibuild.sh), then mkrootfs.sh
# (the system programs, WebKitGTK: the Lite engine) and mklayers.sh. No Chromium yet (CHROME_DIR=none), no Linux apps
# (Flatpak), no speech or Blitzen engine: those come in later steps. Not a release: the "ARM64 check" workflow builds
# it and checks every program in it can start on ARM64; (step 4) Chromium from the snap's arm64 build (each one's libraries are all there, read by the layer's own
# program loader), then the main ones run.   OUT=DIR arm64_base.sh    writes OUT/base.sfs, OUT/report.txt
# GPL-2.0-or-later.
set -e
[ "$(uname -m)" = aarch64 ] || { echo "arm64_base.sh: run it on an ARM64 machine"; exit 1; }
[ -n "$OUT" ] || { echo "OUT is needed"; exit 1; }
HERE=$(cd "$(dirname "$0")/.." && pwd); cd "$HERE"
export MA=$(gcc -print-multiarch); [ "$MA" = aarch64-linux-gnu ] || { echo "multiarch is $MA"; exit 1; }
export SOURCE_DATE_EPOCH=1760000000
OUT=$(realpath -m "$OUT"); mkdir -p "$OUT" src
clone() { # URL BRANCH-OR-TAG DIR [COMMIT]
	local i; for i in 1 2 3; do rm -rf "$3"; git clone -q --depth 1 -b "$2" "$1" "$3" && break; sleep 10; done
	[ -d "$3/.git" ] || { echo "could not fetch $1 ($2)"; exit 1; }
	[ -z "$4" ] || [ "$(git -C "$3" rev-parse HEAD)" = "$4" ] || { echo "$1 $2 is not $4"; exit 1; }
	echo "$3: $(git -C "$3" rev-parse HEAD)" >> "$OUT/report.txt"
}
: > "$OUT/report.txt"
echo "== BusyBox"
BB_COMMIT=1a64f6a20aaf6ea4dbba68bbfa8cc1ab7e5c57c4   # (as arm64_kernel.sh)
clone https://github.com/mirror/busybox 1_36_1 src/busybox $BB_COMMIT
cp systools/busybox.config src/busybox/.config; make -C src/busybox -s -j$(nproc) >/dev/null 2>&1
echo "== the X server"
XREV=$(sed -n 's/^XREV=//p' systools/ci_release.sh); [ -n "$XREV" ] || { echo "no XREV in ci_release.sh"; exit 1; }
systools/xfetch.sh "$XREV" >/dev/null
bash xbuild.sh > "$OUT/xbuild.log" 2>&1 || { tail -40 "$OUT/xbuild.log"; echo "the X server did not build"; exit 1; }
echo "== the sound server"
mkdir -p src/snd; clone https://github.com/pulseaudio/pulseaudio v16.1 src/snd/pulseaudio-16.1
bash sndbuild.sh > "$OUT/sndbuild.log" 2>&1 || { tail -40 "$OUT/sndbuild.log"; echo "PulseAudio did not build"; exit 1; }
echo "== the Wi-Fi tools"
clone https://github.com/LineageOS/android_external_wpa_supplicant_8 lineage-24.0 src/hostap
clone https://github.com/thom311/libnl libnl3_9_0 src/libnl
bash wifibuild.sh > "$OUT/wifibuild.log" 2>&1 || { tail -40 "$OUT/wifibuild.log"; echo "the Wi-Fi tools did not build"; exit 1; }
echo "== Chromium (the snap's, for ARM64)"
CH=none; if bash systools/chromium_fetch.sh snap "$OUT/chrome" > "$OUT/chromium.log" 2>&1; then CH=$OUT/chrome; echo "Chromium $(cat "$OUT/chrome.version") (arm64 snap)" | tee -a "$OUT/report.txt"
else tail -5 "$OUT/chromium.log"; echo "::warning title=ARM64 Chromium::the arm64 snap could not be fetched: the layer is built without it"; fi
echo "== the root file system"
mkdir -p sys/root/usr/share/rudolf sys/root/etc/rudolf; cp boot/sb/rudolf-sb.crt sys/root/usr/share/rudolf/rudolf-sb.crt; cp trusted-keys.txt sys/root/etc/rudolf/rpk-keys
sudo -E env "PATH=$PATH" CHROME_DIR=$CH MA=$MA bash mkrootfs.sh "$OUT/rootfs" www > "$OUT/mkrootfs.log" 2>&1 || { tail -40 "$OUT/mkrootfs.log"; echo "mkrootfs.sh failed"; exit 1; }
tail -3 "$OUT/mkrootfs.log"
R=$OUT/rootfs; LD=/usr/lib/ld-linux-aarch64.so.1
[ -e "$R$LD" ] && [ -L "$R/lib" ] || { echo "the program loader is not at $LD"; exit 1; }
echo "== checks"
# 1. every program and library is ARM64 (nothing from x86 came along)
X86=$(sudo find "$R" -type f \( -perm -u+x -o -name '*.so*' \) -exec file {} + | grep ELF | grep -v "ARM aarch64" || true)
[ -z "$X86" ] || { echo "$X86" | head; echo "files in the layer that are not ARM64"; exit 1; }
# 2. every program and library finds all it needs, through the layer's own loader (inside the layer: chroot)
MISS=$(sudo chroot "$R" /bin/busybox sh -c "for f in \$(/bin/busybox find /usr/bin /usr/sbin /usr/lib/xorg /usr/lib/$MA/webkit2gtk-4.1 /usr/lib/$MA/pulseaudio /usr/lib/$MA/gstreamer-1.0 -type f); do
	/bin/busybox head -c 4 \$f | /bin/busybox grep -q ELF || continue; o=\$($LD --list \$f 2>&1) || { /bin/busybox echo \"\$o\" | /bin/busybox grep -q 'not a dynamic executable' && continue; }
	/bin/busybox echo \"\$o\" | /bin/busybox grep 'not found' | /bin/busybox sed \"s#^#\$f: #\"; done" || true)
[ -z "$MISS" ] || { echo "$MISS" | head -30; echo "libraries missing in the layer"; exit 1; }
echo "every program and library in the layer has its libraries" | tee -a "$OUT/report.txt"
# 3. the main programs run inside the layer
run() { local o; o=$(sudo chroot "$R" "$@" 2>&1 | head -3) || true; echo "$*: $(echo "$o" | head -1)" | tee -a "$OUT/report.txt"; echo "$o"; }
run /bin/busybox | grep -q "BusyBox v1.36.1" || { echo "BusyBox does not run"; exit 1; }
run /usr/bin/Xorg -version | grep -q "X.Org X Server" || { echo "Xorg does not run"; exit 1; }
run /usr/bin/pulseaudio --version | grep -q "pulseaudio 16.1" || { echo "PulseAudio does not run"; exit 1; }
run /usr/sbin/wpa_supplicant -v | grep -q "wpa_supplicant v" || { echo "wpa_supplicant does not run"; exit 1; }
run /usr/bin/rudolf-memguard --warn 3415 340 3415 2387 | grep -qx 1 || { echo "rudolf-memguard does not run"; exit 1; }
[ -x "$R/usr/bin/rudolf-webkit" ] && [ -x "$R/usr/lib/$MA/webkit2gtk-4.1/WebKitWebProcess" ] || { echo "no WebKitGTK (the Lite engine) in the layer"; exit 1; }
[ -x "$R/usr/bin/rudolf-native" ] || { echo "no rudolf-native (the Native engine, 1.5.9) in the layer"; exit 1; }
# Chromium starts inside the layer and renders a page (as the release's start test does)
if [ "$CH" != none ]; then
	# (its libraries first: the snap's Chromium brings some of its own, the rest must be in the layer)
	CLD=$LD; G=$(ls "$R"/opt/chromium/glibc/ld-linux*.so.* 2>/dev/null | head -1); [ -n "$G" ] && { CLD=${G#$R}; echo "Chromium with its own C library: $(cat "$OUT/chrome.glibc" 2>/dev/null)" | tee -a "$OUT/report.txt"; }
	CMISS=$(sudo chroot "$R" $CLD --list /opt/chromium/chrome 2>&1 | grep -E 'not found|version .* not found' || true)
	[ -z "$CMISS" ] || { echo "$CMISS" | head -20; echo "Chromium's libraries missing in the ARM64 layer"; exit 1; }
	# /dev with its /dev/shm and /dev/pts (rbind), /proc and /sys, as at start-up
	sudo mount --rbind /dev "$R/dev"; sudo mount --make-rslave "$R/dev"; sudo mount -t proc proc "$R/proc"; sudo mount --rbind /sys "$R/sys"; sudo mount --make-rslave "$R/sys"
	OUTC=$(sudo chroot "$R" /opt/chromium/chrome --headless=new --no-sandbox --disable-gpu --user-data-dir=/tmp/ct --dump-dom 'data:text/html,<p>rudolf-ok</p>' 2>&1 || true)
	OK=$(echo "$OUTC" | grep -c rudolf-ok || true)
	BR=$(sudo chroot "$R" /opt/chromium/chrome --version 2>&1 | head -1)
	sudo umount -R "$R/sys" "$R/proc" "$R/dev" 2>/dev/null || sudo umount -l "$R/sys" "$R/proc" "$R/dev"; sudo rm -rf "$R/tmp/ct"
	echo "Chromium in the layer: $BR, renders a page: $([ "$OK" -ge 1 ] && echo yes || echo no)" | tee -a "$OUT/report.txt"
	[ "$OK" -ge 1 ] || { echo "$OUTC" | grep -v '^\s*$' | tail -25; echo "Chromium does not start inside the ARM64 layer"; exit 1; }
fi
echo "== the layer"
sudo bash systools/mklayers.sh "$R" "$OUT/layers" | tee -a "$OUT/report.txt"
sudo cp "$OUT/layers/base.sfs" "$OUT/base.sfs"; sudo chown "$(id -u)" "$OUT/base.sfs"
echo "base.sfs: $(( $(stat -c %s "$OUT/base.sfs") / 1048576 )) MB, $(sudo du -sm "$R" | cut -f1) MB unpacked" | tee -a "$OUT/report.txt"
