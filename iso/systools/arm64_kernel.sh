#!/bin/bash
# The ARM64 kernel and its modules layer (1.5.9, docs/plan-1.5.9.md Part 3), built on an ARM64 machine (GitHub's
# ubuntu-24.04-arm): the same initramfs as x86 (BusyBox, rudolf-kexec with ARM64 system calls, rudolf-verity, the
# release keys) and kconfig-arm64.sh. Not a release yet: the "ARM64 check" workflow builds it and starts it in QEMU.
# Environment: KERNEL_VERSION (6.18.y), OUT. Writes OUT/Image (for QEMU's -kernel), OUT/vmlinuz.efi (for UEFI),
# OUT/modules.sfs. GPL-2.0-or-later.
set -e
[ "$(uname -m)" = aarch64 ] || { echo "arm64_kernel.sh: run it on an ARM64 machine"; exit 1; }
HERE=$(cd "$(dirname "$0")/.." && pwd); cd "$HERE"
[ -n "$KERNEL_VERSION" ] && [ -n "$OUT" ] || { echo "KERNEL_VERSION and OUT are needed"; exit 1; }
W=$(realpath -m work-arm64); rm -rf "$W"; mkdir -p "$W" "$OUT"
export SOURCE_DATE_EPOCH=1760000000
for i in 1 2; do rm -rf "$W/linux"; git clone -q --depth 1 -b "v$KERNEL_VERSION" https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git "$W/linux" && break; done
if [ ! -d "$W/linux/kernel" ]; then
	KM=${KERNEL_VERSION%%.*}; T=linux-$KERNEL_VERSION.tar.xz
	curl -fsSL --retry 3 -o "$W/$T" "https://cdn.kernel.org/pub/linux/kernel/v$KM.x/$T"
	curl -fsSL --retry 3 -o "$W/sha256sums.asc" "https://cdn.kernel.org/pub/linux/kernel/v$KM.x/sha256sums.asc"
	[ "$(awk -v f="$T" '$2==f{print $1}' "$W/sha256sums.asc")" = "$(sha256sum "$W/$T" | cut -c1-64)" ] || { echo "kernel: checksum"; exit 1; }
	rm -rf "$W/linux"; mkdir -p "$W/linux" && tar -xJf "$W/$T" -C "$W/linux" --strip-components=1
fi
BB_COMMIT=1a64f6a20aaf6ea4dbba68bbfa8cc1ab7e5c57c4
for u in https://git.busybox.net/busybox https://github.com/mirror/busybox; do
	rm -rf "$W/bb"; git clone -q --depth 1 -b 1_36_1 "$u" "$W/bb" && [ "$(git -C "$W/bb" rev-parse HEAD)" = "$BB_COMMIT" ] && break
done
[ "$(git -C "$W/bb" rev-parse HEAD 2>/dev/null)" = "$BB_COMMIT" ] || { echo "busybox 1_36_1 could not be fetched"; exit 1; }
cp systools/busybox.config "$W/bb/.config"; make -C "$W/bb" -s -j$(nproc) >/dev/null 2>&1; mkdir -p initramfs/bin; cp "$W/bb/busybox" initramfs/bin/
gcc -Os -static -nostdlib -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -o initramfs/bin/rudolf-kexec sys/rudolf-kexec.c && strip initramfs/bin/rudolf-kexec
gcc -Os -static -o initramfs/bin/rudolf-verity sys/rudolf-verity.c && strip initramfs/bin/rudolf-verity
cp trusted-keys.txt initramfs/etc/rudolf-keys
file initramfs/bin/busybox initramfs/bin/rudolf-kexec | grep -q "ARM aarch64" || { echo "the initramfs programs are not ARM64"; exit 1; }
export SB_CERT=$(realpath boot/sb/rudolf-sb.crt)
./kconfig-arm64.sh "$W/linux" "$HERE/initramfs $HERE/initramfs.list"
make -C "$W/linux" ARCH=arm64 -s -j$(nproc) Image vmlinuz.efi modules
make -C "$W/linux" ARCH=arm64 -s modules_install INSTALL_MOD_PATH="$W/mods" DEPMOD=true >/dev/null
KR=$(ls "$W/mods/lib/modules"); cp initramfs/bin/busybox "$W/mods/depmod"; sudo chroot "$W/mods" /depmod "$KR"; rm -f "$W/mods/depmod"
mkdir -p "$W/ms/usr/lib"; cp -a "$W/mods/lib/modules" "$W/ms/usr/lib/"
mksquashfs "$W/ms" "$OUT/modules.sfs" -comp xz -b 1M -noappend -all-root -quiet -no-progress
cp "$W/linux/arch/arm64/boot/Image" "$OUT/Image"; cp "$W/linux/arch/arm64/boot/vmlinuz.efi" "$OUT/vmlinuz.efi"
echo "ARM64 kernel $KR: Image $(du -m "$OUT/Image" | cut -f1) MB, vmlinuz.efi $(du -m "$OUT/vmlinuz.efi" | cut -f1) MB, modules $(du -m "$OUT/modules.sfs" | cut -f1) MB"
