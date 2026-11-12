#!/bin/bash
# A test kernel: the release's kernel (kconfig.sh, the same Linux 6.18.y, the same initramfs) with extra options, and
# its own modules layer. Never part of a release and never installed by an update: the workflow "Test kernel"
# publishes it as a pre-release named test-*, and it is started by hand, with Secure Boot off, for one measurement.
# 2026-11-12: allocation profiling (/proc/allocinfo: how much memory each place in the kernel holds), to name the
# 140 to 180 MB that no counter shows on the Vivobook (docs/plan-memdebug.md).
# The kernel part of ci_release.sh, kept apart so that a test option can never reach a release.
# Environment: KERNEL_VERSION (6.18.y), EXTRA (kconfig options to turn on, by name), OUT. GPL-2.0-or-later.
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd); cd "$HERE"
[ -n "$KERNEL_VERSION" ] && [ -n "$OUT" ] || { echo "KERNEL_VERSION and OUT are needed"; exit 1; }
W=$(realpath -m work-test); rm -rf "$W"; mkdir -p "$W" "$OUT"
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
if [ ! -x initramfs/bin/busybox ]; then
	for u in https://git.busybox.net/busybox https://github.com/mirror/busybox; do
		rm -rf "$W/bb"; git clone -q --depth 1 -b 1_36_1 "$u" "$W/bb" && [ "$(git -C "$W/bb" rev-parse HEAD)" = "$BB_COMMIT" ] && break
	done
	[ "$(git -C "$W/bb" rev-parse HEAD 2>/dev/null)" = "$BB_COMMIT" ] || { echo "busybox 1_36_1 could not be fetched"; exit 1; }
	cp systools/busybox.config "$W/bb/.config"; make -C "$W/bb" -s -j$(nproc) >/dev/null; mkdir -p initramfs/bin; cp "$W/bb/busybox" initramfs/bin/
fi
gcc -Os -static -nostdlib -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -o initramfs/bin/rudolf-kexec sys/rudolf-kexec.c && strip initramfs/bin/rudolf-kexec
gcc -Os -static -o initramfs/bin/rudolf-verity sys/rudolf-verity.c && strip initramfs/bin/rudolf-verity
cp trusted-keys.txt initramfs/etc/rudolf-keys
FW=$W/linux-firmware
for u in https://gitlab.com/kernel-firmware/linux-firmware.git https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git; do
	rm -rf "$FW"; git clone -q --depth 1 --filter=blob:none --no-checkout "$u" "$FW" || continue
	awk '$1=="F"{print "/"$3}' firmware-extra.txt | { echo /WHENCE; cat; } | git -C "$FW" sparse-checkout set --no-cone --stdin && git -C "$FW" checkout -q && break
done
python3 systools/fwextra.py copy firmware-extra.txt "$FW" "$W/fw"
SB=$W/sof-bin; git clone -q --filter=blob:none --no-checkout https://github.com/thesofproject/sof-bin.git "$SB"
awk '$1=="F"{print "/"$4}' firmware-sof.txt | git -C "$SB" sparse-checkout set --no-cone --stdin; git -C "$SB" checkout -q fcd5ae85ead7ecd67ff199335b61fac825843ca0
python3 systools/fwextra.py copy firmware-sof.txt "$SB" "$W/fw"
# the Rudolf OS certificate built in as trusted, as in a release (public: the certificate only, never the key)
export SB_CERT=$(realpath boot/sb/rudolf-sb.crt)
UCODE_DIR="$W/fw" ./kconfig.sh "$W/linux" "$HERE/initramfs $HERE/initramfs.list"
# the test options, on top of the release's configuration
for o in $EXTRA; do "$W/linux/scripts/config" --file "$W/linux/.config" --enable "$o"; done
make -C "$W/linux" -s olddefconfig
for o in $EXTRA; do grep -q "^CONFIG_$o=y$" "$W/linux/.config" || { echo "testkernel: $o is not on after olddefconfig"; exit 1; }; done
sed -i 's/^CONFIG_LOCALVERSION=.*/CONFIG_LOCALVERSION="-rudolf-test"/' "$W/linux/.config"
make -C "$W/linux" -s olddefconfig
make -C "$W/linux" -s -j$(nproc) bzImage modules
make -C "$W/linux" -s modules_install INSTALL_MOD_PATH="$W/mods" DEPMOD=true >/dev/null
KR=$(ls "$W/mods/lib/modules"); cp initramfs/bin/busybox "$W/mods/depmod"; sudo chroot "$W/mods" /depmod "$KR"; rm -f "$W/mods/depmod"
[ -s "$W/mods/lib/modules/$KR/modules.dep.bb" ] || { echo "no module index"; exit 1; }
mkdir -p "$W/ms/usr/lib"; cp -a "$W/mods/lib/modules" "$W/ms/usr/lib/"
mkdir -p "$W/ms/usr/lib/firmware" && cp -a "$W/fw/." "$W/ms/usr/lib/firmware/"
mksquashfs "$W/ms" "$OUT/modules.sfs" -comp xz -Xbcj x86 -b 1M -noappend -all-root -quiet -no-progress
cp "$W/linux/arch/x86/boot/bzImage" "$OUT/vmlinuz"; cp initramfs/bin/rudolf-kexec "$OUT/rudolf-kexec"
grep -E "^CONFIG_($(echo $EXTRA | tr ' ' '|'))=" "$W/linux/.config" > "$OUT/config-extra.txt"
(cd "$OUT" && sha256sum vmlinuz modules.sfs rudolf-kexec > SHA256SUMS)
echo "test kernel $KR: $(du -m "$OUT/vmlinuz" | cut -f1) MB, modules $(du -m "$OUT/modules.sfs" | cut -f1) MB"
