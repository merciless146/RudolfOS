#!/bin/bash
# Build the next Rudolf OS system release from the previous one (run by the GitHub Actions workflow on
# ubuntu-24.04, the same system the disc images are built on). Only what changed is rebuilt:
#   kernel + modules  when a newer 6.18.y is out          chromium  when a newer stable Chromium is out
#   desktop           when iso/www changed in the repo     base      reused (full rebuilds come from build_iso.sh)
# Every new Chromium must start inside the real base layer before it is released; otherwise the previous
# Chromium layer is kept and the release notes say so. GPL-2.0-or-later.
#
# Environment: PREV (folder with the previous release: manifest.txt and its five files), OUT, KEYFILE,
# SERIAL, FILE_BASE (where devices download the files), CHROMIUM_FROM (snap|playwright), KERNEL_VERSION.
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd); cd "$HERE"
W=$(realpath -m work); rm -rf "$W"; mkdir -p "$W/new"
export SOURCE_DATE_EPOCH=1760000000
pv() { sed -n "s/^$1 //p" "$PREV/manifest.txt"; }
MK="mksquashfs"; SQ="-comp xz -Xbcj x86 -b 1M -noappend -all-root -quiet -no-progress"
NOTES=(); CH=$(pv chromium); KV=$(pv kernel)
for f in vmlinuz base.sfs chromium.sfs desktop.sfs modules.sfs; do cp "$PREV/$f" "$W/new/$f"; done

# ---- Chromium
try_chromium() {
	systools/chromium_fetch.sh "$1" "$W/chrome" || return 1
	NV=$(cat "$W/chrome.version"); [ "$NV" = "$CH" ] && { echo "Chromium $NV unchanged"; return 0; }
	rm -rf "$W/cs"; mkdir -p "$W/cs/opt/chromium"; cp -a "$W/chrome/." "$W/cs/opt/chromium/"
	( cd "$W/cs/opt/chromium/locales" && for f in *.pak; do case $f in en-US.pak|de.pak|es.pak|fr.pak|pt-BR.pak|ru.pak|zh-CN.pak|ja.pak|ko.pak|ar.pak|he.pak) ;; *) rm -f "$f";; esac; done )
	rm -f "$W/cs/opt/chromium/chrome-wrapper" "$W/cs/opt/chromium/xdg-mime" "$W/cs/opt/chromium/xdg-settings" "$W/cs/opt/chromium"/locales/*.info
	strip "$W/cs/opt/chromium/chrome" "$W/cs/opt/chromium/chrome_crashpad_handler" 2>/dev/null || true
	[ -e "$W/cs/opt/chromium/chrome_sandbox" ] && chmod 4755 "$W/cs/opt/chromium/chrome_sandbox"
	# start test: the new Chromium inside the real base layer must render a page
	sudo rm -rf "$W/root"; sudo unsquashfs -q -d "$W/root" "$W/new/base.sfs" >/dev/null
	sudo cp -a "$W/cs/opt/chromium/." "$W/root/opt/chromium/"
	sudo mount --bind /dev "$W/root/dev"; sudo mount -t proc proc "$W/root/proc"
	OK=$(sudo chroot "$W/root" /opt/chromium/chrome --headless=new --no-sandbox --disable-gpu --user-data-dir=/tmp/ct --dump-dom 'data:text/html,<p>rudolf-ok</p>' 2>/dev/null | grep -c rudolf-ok || true)
	sudo umount "$W/root/proc" "$W/root/dev"; sudo rm -rf "$W/root"
	[ "$OK" -ge 1 ] || { echo "Chromium $NV from $1 did not start inside Rudolf OS"; return 1; }
	$MK "$W/cs" "$W/new/chromium.sfs" $SQ; CH=$NV; NOTES+=("Chromium $NV"); return 0
}
try_chromium "${CHROMIUM_FROM:-snap}" || { [ "${CHROMIUM_FROM:-snap}" != playwright ] && try_chromium playwright; } || NOTES+=("Chromium unchanged: the new build failed the start test")

# ---- kernel and modules
if [ -n "$KERNEL_VERSION" ] && [ "$KERNEL_VERSION-rudolf" != "$KV" ]; then
	git clone -q --depth 1 -b "v$KERNEL_VERSION" https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git "$W/linux"
	[ -x initramfs/bin/busybox ] || { git clone -q --depth 1 -b 1_36_1 https://git.busybox.net/busybox "$W/bb"; cp systools/busybox.config "$W/bb/.config"; make -C "$W/bb" -s -j$(nproc) >/dev/null; mkdir -p initramfs/bin; cp "$W/bb/busybox" initramfs/bin/; }
	gcc -Os -static -nostdlib -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -o initramfs/bin/rudolf-kexec sys/rudolf-kexec.c && strip initramfs/bin/rudolf-kexec
	./kconfig.sh "$W/linux" "$HERE/initramfs $HERE/initramfs.list"
	make -C "$W/linux" -s -j$(nproc) bzImage modules
	make -C "$W/linux" -s modules_install INSTALL_MOD_PATH="$W/mods" >/dev/null
	KR=$(ls "$W/mods/lib/modules"); depmod -b "$W/mods" "$KR"
	mkdir -p "$W/ms/usr/lib"; cp -a "$W/mods/lib/modules" "$W/ms/usr/lib/"
	$MK "$W/ms" "$W/new/modules.sfs" $SQ; cp "$W/linux/arch/x86/boot/bzImage" "$W/new/vmlinuz"
	KV=$KR; NOTES+=("Linux $KERNEL_VERSION")
fi

# ---- desktop (byte-identical when iso/www did not change)
mkdir -p "$W/ds/usr/share/rudolf"; cp -a www "$W/ds/usr/share/rudolf/www"; $MK "$W/ds" "$W/desk.sfs" $SQ
if ! cmp -s "$W/desk.sfs" "$W/new/desktop.sfs"; then cp "$W/desk.sfs" "$W/new/desktop.sfs"; NOTES+=("desktop update"); fi

CHANGED=0; for f in vmlinuz base.sfs chromium.sfs desktop.sfs modules.sfs; do cmp -s "$PREV/$f" "$W/new/$f" || CHANGED=1; done
if [ $CHANGED = 0 ]; then echo "nothing changed: no release"; echo "released=0" >> "${GITHUB_OUTPUT:-/dev/null}"; exit 0; fi
NOTE=$(printf '%s, ' "${NOTES[@]}"); NOTE=${NOTE%, }; NOTE=${NOTE:-security fixes}
python3 systools/make_release.py --layers "$W/new" --kernel "$W/new/vmlinuz" --out "$OUT" --key "$KEYFILE" --serial "$SERIAL" \
	--version "$(cat VERSION).$(date -u +%y%m%d)" --chromium "$CH" --kernel-version "$KV" --notes "$NOTE" --file-base "$FILE_BASE" ${URGENT:+--urgent}
echo "released=1" >> "${GITHUB_OUTPUT:-/dev/null}"; echo "notes=$NOTE" >> "${GITHUB_OUTPUT:-/dev/null}"
