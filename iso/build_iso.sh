#!/bin/bash
# Build rudolf-os-1.1.iso: kernel + Rudolf Boot + EFI image + the system in four compressed layers.
set -e
HERE=$(cd "$(dirname "$0")" && pwd); cd "$HERE"
VER=$(cat VERSION); OUT=${1:-$HERE/out}; mkdir -p "$OUT"
T=$HERE/isotree; rm -rf "$T"; mkdir -p $T/BOOT $T/EFI $T/RUDOLF $T/LICENSES $T/SOURCE
echo "== boot loader"; nasm -f bin -o boot/rboot.bin boot/rboot.asm; nasm -f bin -o boot/mbr.bin boot/mbr.asm
cp boot/rboot.bin $T/BOOT/RBOOT.BIN
cp src/linux/arch/x86/boot/bzImage $T/BOOT/VMLINUZ
echo "== EFI image (Secure Boot ready: shim, the menu and the signed kernel bundle; SB_PARTS and SB_KEY, see systools/mkefi.sh)"
bash systools/mkefi.sh src/linux/arch/x86/boot/bzImage "$VER" "${SB_PARTS:?SB_PARTS: the folder with the Secure Boot parts}" $T/EFI/EFIBOOT.IMG
echo "== root file system"
# key that signs this disc's package list (kept out of the disc and the source archive); the system trusts
# only trusted-keys.txt, so the disc's packages are accepted by their checksums in the disc's own list
[ -f keys/rpk-signing.key ] || python3 pkgtools/rpk-tool.py keygen keys
mkdir -p sys/root/etc/rudolf; cp trusted-keys.txt sys/root/etc/rudolf/rpk-keys   # trust only the keys listed in trusted-keys.txt
if [ "${REBUILD_ROOT:-1}" = 1 ] || [ ! -f layers/base.sfs ]; then bash mkrootfs.sh rootfs www >/dev/null 2>&1; rm -rf layers; bash systools/mklayers.sh rootfs layers; fi
echo "== packages"; bash pkgtools/build_repo.sh
# the system in four layers (the start-up script mounts them as one); the kernel is BOOT/VMLINUZ
for n in base chromium desktop modules; do python3 systools/verity.py add layers/$n.sfs $n >/dev/null; cp layers/$n.sfs $T/RUDOLF/$(echo $n | tr a-z A-Z).SFS; done
# the signed release list the start-up checks the layers against (1.2.2); RELEASE_KEY: the release signing key
# (raw base64, as the workflow uses). Without it the layers start unchecked, and not at all with Secure Boot on.
if [ -n "$RELEASE_KEY" ]; then mkdir -p "$OUT/rl"; python3 systools/make_release.py --layers layers --kernel src/linux/arch/x86/boot/bzImage --out "$OUT/rl" --key "$RELEASE_KEY" --serial "${SERIAL:-$(date -u +%Y%m%d%H)}" --version "$VER" >/dev/null; cp "$OUT/rl/channel/manifest.txt" $T/RUDOLF/MANIFEST.TXT; cp "$OUT/rl/channel/manifest.sig" $T/RUDOLF/MANIFEST.SIG; rm -rf "$OUT/rl"; else echo "warning: no RELEASE_KEY, the layers cannot be checked at start-up"; fi
cp initramfs/etc/rudolf-id $T/RUDOLF/ID   # must match the ID built into the kernel's initramfs (rudolf-1.1 for every 1.1.x)
# which release this disc is: updates must be newer (serial) to install
CHROMV=$(rootfs/opt/chromium/chrome --version 2>/dev/null | awk '{print $2}')
printf 'serial %s\nversion %s\nreleased %s\nchromium %s\nkernel %s\nnotes %s\n' "${SERIAL:-$(date -u +%Y%m%d%H)}" "$VER" "$(date -u +%F)" "${CHROMV:-141.0.7390.37}" "$(ls modules/lib/modules | head -1)" "${NOTES:-disc release}" > $T/RUDOLF/RELEASE
echo "== licences and sources"
cp $HERE/../libs/od/OFL.txt $T/LICENSES/OFL_OPENDYSLEXIC.TXT
cp $HERE/../libs/pdfjs/LICENSE $T/LICENSES/PDFJS_APACHE2.TXT
cat www/rudolfq/librequake-copying.txt www/rudolfq/librequake-credits.txt > $T/LICENSES/LIBREQUAKE.TXT
cat src/linux/COPYING src/linux/LICENSES/preferred/GPL-2.0 src/linux/LICENSES/exceptions/Linux-syscall-note > $T/LICENSES/LINUX_GPL2.TXT; cp src/busybox/LICENSE $T/LICENSES/BUSYBOX.TXT
cp $HERE/LICENSE.TXT $T/LICENSES/RUDOLF_OS.TXT 2>/dev/null || true
[ -f src/espeak-ng/COPYING ] && cp src/espeak-ng/COPYING $T/LICENSES/ESPEAKNG.TXT
[ -f www/lib/ja-yomi-COPYING.txt ] && cp www/lib/ja-yomi-COPYING.txt $T/LICENSES/KAKASI.TXT
[ -f src/fp/flatpak-1.18.4/COPYING ] && cp src/fp/flatpak-1.18.4/COPYING $T/LICENSES/FLATPAK.TXT
[ -f src/fp/libostree-2026.4/COPYING ] && cp src/fp/libostree-2026.4/COPYING $T/LICENSES/OSTREE.TXT
[ -f src/fp/bubblewrap-0.13.0/COPYING ] && cp src/fp/bubblewrap-0.13.0/COPYING $T/LICENSES/BWRAP.TXT
[ -f src/snd/pulseaudio-16.1/LICENSE ] && cat src/snd/pulseaudio-16.1/LICENSE src/snd/pulseaudio-16.1/LGPL > $T/LICENSES/PULSEAUD.TXT
[ -f src/xorg/xsrc/external/mit/xorg-server/dist/COPYING ] && cp src/xorg/xsrc/external/mit/xorg-server/dist/COPYING $T/LICENSES/XORG.TXT
[ -f src/linux-firmware/WHENCE ] && cp src/linux-firmware/WHENCE $T/LICENSES/FIRMWARE.TXT
[ -f src/hostap/README ] && cp src/hostap/README $T/LICENSES/WPA_SUPP.TXT
[ -f src/libnl/COPYING ] && cp src/libnl/COPYING $T/LICENSES/LIBNL.TXT
[ -f src/wireless-regdb/LICENSE ] && cp src/wireless-regdb/LICENSE $T/LICENSES/REGDB.TXT
[ -f src/llama.cpp/LICENSE ] && cat src/llama.cpp/LICENSE src/llama.cpp/licenses/* > $T/LICENSES/LLAMACPP.TXT 2>/dev/null
[ -f src/lakesnes/LICENSE.txt ] && cp src/lakesnes/LICENSE.txt $T/LICENSES/LAKESNES.TXT
[ -f src/desmume-wasm/desmume/COPYING ] && cat src/desmume-wasm/desmume/AUTHORS src/desmume-wasm/desmume/COPYING > $T/LICENSES/DESMUME.TXT
[ -f src/zlib/LICENSE ] && cp src/zlib/LICENSE $T/LICENSES/ZLIB.TXT
[ -f www/lib/fengari-LICENSE.txt ] && cp www/lib/fengari-LICENSE.txt $T/LICENSES/FENGARI.TXT
[ -f www/lib/xterm-LICENSE.txt ] && cp www/lib/xterm-LICENSE.txt $T/LICENSES/XTERMJS.TXT
cp $HERE/README.TXT $T/README.TXT 2>/dev/null || true
# the disc's own package repository (rpk); ISO names are upper case and the kernel shows them in lower case
if [ -f repo/index.txt ]; then mkdir -p $T/RUDOLF/PKG; for f in repo/*; do cp "$f" "$T/RUDOLF/PKG/$(basename "$f" | tr a-z A-Z)"; done; fi
echo "== source archives"
SD=$(mktemp -d); mkdir -p $SD/rudolf-os-$VER/desktop $SD/rudolf-os-$VER/libs/od $SD/rudolf-os-$VER/iso
cp $HERE/../v10/*.js $HERE/../v10/styles25.css $HERE/../v10/build19.py $HERE/../v10/base_v24.html $SD/rudolf-os-$VER/desktop/
cp $HERE/../libs/pinyin.min.json $SD/rudolf-os-$VER/libs/; mkdir -p $SD/rudolf-os-$VER/libs/od/compiled; cp $HERE/../libs/od/compiled/OpenDyslexic-Regular.woff2 $HERE/../libs/od/compiled/OpenDyslexic-Bold.woff2 $SD/rudolf-os-$VER/libs/od/compiled/; cp $HERE/../libs/od/OFL.txt $SD/rudolf-os-$VER/libs/od/
cp -a boot pkgtools pkgs systools kconfig.sh wifibuild.sh fpshim.sh fpbuild.sh sndshim.sh sndbuild.sh xshim.sh xbuild.sh sys mkrootfs.sh mkimage.py credits.js pkgmanifest.py build_iso.sh initramfs.list VERSION LICENSE.TXT README.TXT SOURCE-OFFER.TXT BUILD.TXT $SD/rudolf-os-$VER/iso/
mkdir -p $SD/rudolf-os-$VER/iso/qt; cp qt/*.py qt/*.sh qt/*.c $SD/rudolf-os-$VER/iso/qt/; cp -a qt/ndstest $SD/rudolf-os-$VER/iso/qt/
cp initramfs/init $SD/rudolf-os-$VER/iso/sys/initramfs-init 2>/dev/null || true
rm -rf $SD/rudolf-os-$VER/iso/pkgtools/debs
rm -f $SD/rudolf-os-$VER/iso/sys/rudolfd $SD/rudolf-os-$VER/iso/sys/rudolf-display $SD/rudolf-os-$VER/iso/boot/*.bin
cp BUILD.TXT $SD/rudolf-os-$VER/BUILD.TXT
tar -C $SD -cJf $HERE/source.tar.xz --owner=0 --group=0 rudolf-os-$VER; rm -rf $SD
[ -f $HERE/busybox-src.tar.xz ] || git -C src/busybox archive --format=tar --prefix=busybox-1.36.1/ HEAD | xz -9 > $HERE/busybox-src.tar.xz
[ -f $HERE/source.tar.xz ] && cp $HERE/source.tar.xz $T/SOURCE/RUDOLF.TXZ
[ -f $HERE/busybox-src.tar.xz ] && cp $HERE/busybox-src.tar.xz $T/SOURCE/BUSYBOX.TXZ
[ -f src/fp/flatpak.tar.xz ] && cp src/fp/flatpak.tar.xz $T/SOURCE/FLATPAK.TXZ
[ -f src/fp/ostree.tar.xz ] && cp src/fp/ostree.tar.xz $T/SOURCE/OSTREE.TXZ
[ -f src/snd/pulseaudio.tar.xz ] && cp src/snd/pulseaudio.tar.xz $T/SOURCE/PULSEAUD.TXZ
[ -f src/xorg/xorg-src.tar.xz ] && cp src/xorg/xorg-src.tar.xz $T/SOURCE/XORG.TXZ
[ -f src/fp/bwrap.tar.xz ] && cp src/fp/bwrap.tar.xz $T/SOURCE/BWRAP.TXZ
[ -f src/wifi-src.tar.xz ] && cp src/wifi-src.tar.xz $T/SOURCE/WIFI.TXZ
[ -f src/emu-src.tar.xz ] && cp src/emu-src.tar.xz $T/SOURCE/EMU.TXZ
[ -f src/llama-src.tar.xz ] && cp src/llama-src.tar.xz $T/SOURCE/LLAMA.TXZ
cp src/linux/.config $T/SOURCE/LINUX.CFG; cp src/busybox/.config $T/SOURCE/BUSYBOX.CFG
cp $HERE/SOURCE-OFFER.TXT $T/SOURCE/OFFER.TXT 2>/dev/null || true
cp rootfs/usr/share/doc/rudolf/packages-versions.txt $T/SOURCE/PACKAGES.TXT
cp rootfs/usr/share/doc/rudolf/third-party-licences.txt $T/LICENSES/THIRDPTY.TXT
cp rootfs/usr/share/doc/rudolf/chromium-credits.txt $T/LICENSES/CHROMIUM.TXT
echo "== checksums"; (cd $T && find . -type f ! -path ./RUDOLF/SHA256 | sed 's#^\./##' | sort | while read f; do l=$(echo "$f" | tr 'A-Z' 'a-z'); echo "$(sha256sum "$f" | cut -d' ' -f1)  $l"; done) > $T/RUDOLF/SHA256
echo "== ISO"; python3 mkimage.py iso "$OUT/rudolf-os-$VER.iso" $T --mbr boot/mbr.bin --boot BOOT/RBOOT.BIN --kernel BOOT/VMLINUZ --efi EFI/EFIBOOT.IMG --label RUDOLF_OS_${VER//./_}
(cd "$OUT" && sha256sum rudolf-os-$VER.iso > rudolf-os-$VER.iso.sha256)
ls -la "$OUT"
