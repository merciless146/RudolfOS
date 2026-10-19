#!/bin/bash
# Build the next Rudolf OS system release from the previous one (run by the GitHub Actions workflow on
# ubuntu-24.04, the same system the disc images are built on). Only what changed is rebuilt:
#   kernel + modules  when a newer 6.18.y is out, or kconfig.sh / the initramfs changed
#   chromium          when a newer stable Chromium is out
#   desktop           when iso/www changed in the repo
#   base              when the system programs (rudolf-display, rudolfd, rudolf-update) or sys/root changed:
#                     the previous base layer with those rebuilt and the files of sys/root copied over it
#                     (everything else in it comes from build_iso.sh's full builds)
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
# CHROMIUM_FROM=none skips Chromium (local test builds)
[ "${CHROMIUM_FROM:-snap}" = none ] || try_chromium "${CHROMIUM_FROM:-snap}" || { [ "${CHROMIUM_FROM:-snap}" != playwright ] && try_chromium playwright; } || NOTES+=("Chromium unchanged: the new build failed the start test")

# ---- kernel and modules
# what the kernel is built from besides its version: when it changes, the kernel is rebuilt
# Secure Boot (1.2.2): the certificate built into the kernel as trusted (kexec), and the key that signs the kernel
# (SB_KEY: the secret RUDOLF_SB_KEY; local test builds pass a test key and certificate)
SB_CERT=$(realpath "${SB_CERT:-boot/sb/rudolf-sb.crt}"); export SB_CERT
[ -n "$SB_KEY" ] && [ -f "$SB_KEY" ] || { echo "SB_KEY: the Secure Boot signing key is needed"; exit 1; }
python3 systools/sbkeycheck.py "$SB_KEY" "$SB_CERT"
SBSIGN=${SBSIGN:-sbsign}
KSRC=$(cat kconfig.sh initramfs.list initramfs/init initramfs/etc/rudolf-id sys/rudolf-kexec.c sys/rudolf-verity.c trusted-keys.txt "$SB_CERT" ${RUDOLF_TEST_KEYS:+"$RUDOLF_TEST_KEYS"} | sha256sum | cut -c1-16)
PKSRC=$(unsquashfs -cat "$PREV/modules.sfs" "usr/lib/modules/$KV/rudolf-ksrc" 2>/dev/null || true)
[ -n "$KERNEL_VERSION" ] || KERNEL_VERSION=${KV%-rudolf}
if [ "$KERNEL_VERSION-rudolf" != "$KV" ] || [ "$KSRC" != "$PKSRC" ]; then
	# LINUX_SRC: an unpacked kernel tree of that version to build in instead of downloading one (local test builds)
	if [ -n "$LINUX_SRC" ]; then ln -s "$(realpath "$LINUX_SRC")" "$W/linux"; else git clone -q --depth 1 -b "v$KERNEL_VERSION" https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git "$W/linux"; fi
	[ -x initramfs/bin/busybox ] || { git clone -q --depth 1 -b 1_36_1 https://git.busybox.net/busybox "$W/bb"; cp systools/busybox.config "$W/bb/.config"; make -C "$W/bb" -s -j$(nproc) >/dev/null; mkdir -p initramfs/bin; cp "$W/bb/busybox" initramfs/bin/; }
	gcc -Os -static -nostdlib -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -o initramfs/bin/rudolf-kexec sys/rudolf-kexec.c && strip initramfs/bin/rudolf-kexec
	gcc -Os -static -o initramfs/bin/rudolf-verity sys/rudolf-verity.c && strip initramfs/bin/rudolf-verity
	cp trusted-keys.txt initramfs/etc/rudolf-keys
	# local test builds only (never set in the workflow): also trust the test release key, so that a test
	# release's layers pass the start-up check
	[ -n "$RUDOLF_TEST_KEYS" ] && cat "$RUDOLF_TEST_KEYS" >> initramfs/etc/rudolf-keys
	./kconfig.sh "$W/linux" "$HERE/initramfs $HERE/initramfs.list"
	make -C "$W/linux" -s -j$(nproc) bzImage modules
	make -C "$W/linux" -s modules_install INSTALL_MOD_PATH="$W/mods" DEPMOD=true >/dev/null
	# the module index in busybox's format (modules.dep.bb), which the system's busybox modprobe reads, as on the disc
	KR=$(ls "$W/mods/lib/modules"); cp initramfs/bin/busybox "$W/mods/depmod"; sudo chroot "$W/mods" /depmod "$KR"; rm -f "$W/mods/depmod"
	[ -s "$W/mods/lib/modules/$KR/modules.dep.bb" ] || { echo "no module index"; exit 1; }
	echo "$KSRC" > "$W/mods/lib/modules/$KR/rudolf-ksrc"
	mkdir -p "$W/ms/usr/lib"; cp -a "$W/mods/lib/modules" "$W/ms/usr/lib/"
	$MK "$W/ms" "$W/new/modules.sfs" $SQ
	# signed with the Rudolf OS Secure Boot key: the disc's boot menu starts it (inside the kernel bundle), and a
	# Rudolf OS kernel under Secure Boot hands over (kexec) only to a kernel with this signature
	"$SBSIGN" --key "$SB_KEY" --cert "$SB_CERT" --output "$W/new/vmlinuz" "$W/linux/arch/x86/boot/bzImage"
	[ "$KR" = "$KV" ] && NOTES+=("Linux $KERNEL_VERSION with new drivers") || NOTES+=("Linux $KERNEL_VERSION"); KV=$KR
fi

# ---- base: the system programs and sys/root
# the Secure Boot certificate, for Settings > Security > Prepare Secure Boot (rudolfd enrolls it through MokManager)
mkdir -p sys/root/usr/share/rudolf && cp "$SB_CERT" sys/root/usr/share/rudolf/rudolf-sb.crt
BSRC=$( (cat sys/rudolf-display.c sys/rrcompat.h sys/rudolf-launch.c sys/rudolfd.c sys/rudolf-update.c sys/cdp-eval.c trusted-keys.txt VERSION; cd sys/root && find . -type f | sort | xargs sha256sum) | sha256sum | cut -c1-16)
PBSRC=$(unsquashfs -cat "$PREV/base.sfs" usr/share/rudolf/base-src 2>/dev/null || true)
if [ "$BSRC" != "$PBSRC" ]; then
	sudo rm -rf "$W/broot"; sudo unsquashfs -q -d "$W/broot" "$PREV/base.sfs" >/dev/null
	B=$W/bbin; mkdir -p "$B"
	# the same commands as mkrootfs.sh, on the same Ubuntu 24.04 the base layer's libraries come from
	gcc -O2 -o "$B/rudolf-display" sys/rudolf-display.c -lX11 /usr/lib/x86_64-linux-gnu/libXtst.so.6 /usr/lib/x86_64-linux-gnu/libXfixes.so.3 /usr/lib/x86_64-linux-gnu/libXrandr.so.2 -lm
	gcc -O2 -o "$B/rudolfd" sys/rudolfd.c -Wl,-Bstatic -lz -Wl,-Bdynamic
	gcc -O2 -o "$B/rudolf-update" sys/rudolf-update.c -lcrypto -ldl
	gcc -O2 -o "$B/rudolf-launch" sys/rudolf-launch.c
	gcc -O2 -static -o "$B/cdp-eval" sys/cdp-eval.c
	for b in rudolf-display rudolfd rudolf-update rudolf-launch cdp-eval; do
		for l in $(ldd "$B/$b" | awk '/=>/{print $3}'); do [ -e "$W/broot$l" ] || [ -e "$W/broot/usr$l" ] || { echo "$b needs $l, which the base layer does not have"; exit 1; }; done
		sudo install -m 0755 "$B/$b" "$W/broot/usr/bin/$b"
	done
	sudo cp -a sys/root/. "$W/broot/"
	sudo mkdir -p "$W/broot/etc/rudolf"; sudo cp trusted-keys.txt "$W/broot/etc/rudolf/rpk-keys"
	sudo cp VERSION "$W/broot/usr/share/rudolf/VERSION"
	echo "$BSRC" | sudo tee "$W/broot/usr/share/rudolf/base-src" >/dev/null
	sudo $MK "$W/broot" "$W/new/base.sfs" $SQ; sudo chown "$(id -u)" "$W/new/base.sfs"; sudo rm -rf "$W/broot"
	NOTES+=("system programs")
fi

# ---- desktop (byte-identical when iso/www and iso/shell did not change)
mkdir -p "$W/ds/usr/share/rudolf"; cp -a www "$W/ds/usr/share/rudolf/www"
# the shell (an Isolated Web App that gives Oracle real tabs), signed with a key derived from the release key;
# Chromium installs it from these files (policy written by the session). Ed25519 signatures are deterministic,
# so an unchanged shell gives the same bytes.
if [ -d shell ]; then
	I="$W/ds/usr/share/rudolf/www/iwa"; mkdir -p "$I"
	python3 systools/mkiwa.py derive "$KEYFILE" "$W/iwa.key" > "$I/id"
	python3 systools/mkiwa.py build shell "$I/shell.swbn" "$W/iwa.key" >/dev/null
	python3 systools/mkiwa.py update shell http://127.0.0.1:8080/iwa/shell.swbn "$I/update.json" >/dev/null
	rm -f "$W/iwa.key"
fi
$MK "$W/ds" "$W/desk.sfs" $SQ; python3 systools/verity.py add "$W/desk.sfs" desktop >/dev/null
if ! cmp -s "$W/desk.sfs" "$W/new/desktop.sfs"; then cp "$W/desk.sfs" "$W/new/desktop.sfs"; NOTES+=("desktop update"); fi

# every layer carries its dm-verity hash tree (1.2.2); the same data always gives the same tree
for n in base chromium desktop modules; do python3 systools/verity.py add "$W/new/$n.sfs" $n >/dev/null; done
CHANGED=0; for f in vmlinuz base.sfs chromium.sfs desktop.sfs modules.sfs; do cmp -s "$PREV/$f" "$W/new/$f" || CHANGED=1; done
if [ $CHANGED = 0 ]; then echo "nothing changed: no release"; echo "released=0" >> "${GITHUB_OUTPUT:-/dev/null}"; exit 0; fi
NOTE=$(printf '%s, ' "${NOTES[@]}"); NOTE=${NOTE%, }; NOTE=${NOTE:-security fixes}
# the first release of a new version says what is new in it (CHANGELOG.md, the "Summary:" line)
if [ "$(pv version | cut -d. -f1-3)" != "$(cat VERSION)" ] && [ "$(pv version)" != "$(cat VERSION)" ]; then
	SUM=$(awk -v v="$(cat VERSION)" '$1=="##"&&$2==v{f=1;next} /^## /{f=0} f&&/^Summary:/{sub(/^Summary: */,"");print;exit}' ../CHANGELOG.md 2>/dev/null)
	[ -n "$SUM" ] && NOTE="$(cat VERSION): $SUM"
fi
NOTE=$(printf '%s' "$NOTE" | tr -d '\n\r' | cut -c1-290)
python3 systools/make_release.py --layers "$W/new" --kernel "$W/new/vmlinuz" --out "$OUT" --key "$KEYFILE" --serial "$SERIAL" \
	--version "$(cat VERSION).$(date -u +%y%m%d)" --chromium "$CH" --kernel-version "$KV" --notes "$NOTE" --file-base "$FILE_BASE" ${URGENT:+--urgent}
echo "released=1" >> "${GITHUB_OUTPUT:-/dev/null}"; echo "notes=$NOTE" >> "${GITHUB_OUTPUT:-/dev/null}"
