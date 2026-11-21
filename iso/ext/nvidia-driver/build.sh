#!/bin/sh
# Rudolf OS 1.6.1 (docs/plan-1.6.1.md Part 19): the system extension nvidia-driver's files, for one Rudolf OS kernel:
#   - NVIDIA's open kernel modules (kernel-open/ in NVIDIA's own .run, MIT/GPL-2.0), built against the release's kernel
#     (its version and Rudolf OS's configuration, iso/kconfig.sh) and, when MODSIGN_KEY and MODSIGN_CERT are given, signed
#     with them (the kernel's own sign-file); unsigned otherwise, which every Rudolf OS kernel refuses to load;
#   - NVIDIA's user-space driver (OpenGL/GLX, EGL, Vulkan, the Xorg driver, the GSP firmware, the libraries games use), copied
#     from the .run byte for byte: its licence allows redistribution only of unmodified binary files, with the licence
#     given to each recipient (its section 1.d, checked below in the .run's own licence: the build stops if it is gone);
#   - usr/share/nvidia/rudolf-gpus.txt: the cards offered (the base's usr/share/rudolf/nvidia-gpus.txt is a copy of it) (GeForce 16 and newer: the driver's supported-gpus.json, those the
#     open kernel modules support), PCI ID and name, tab-separated.
#   sh build.sh OUTDIR      environment: KVER (the release kernel, 6.18.y-rudolf), LINUX_SRC (an unpacked tree of that
#   version, else fetched as ci_release.sh does), MODULES_SFS (the release's modules.sfs: its modules' vermagic must be the
#   same as these, or the build stops), MODSIGN_KEY, MODSIGN_CERT (PEM), NV_RUN (the .run already downloaded), WORK.
# The driver's version and the .run's SHA-256: nvidia.txt beside this script. GPL-2.0-or-later.
set -e
D=$(cd "$(dirname "$0")" && pwd); I=$(cd "$D/../.." && pwd); OUT=$1
[ -n "$OUT" ] && [ -n "$KVER" ] || { echo "usage: KVER=6.18.y-rudolf sh build.sh OUTDIR"; exit 2; }
V=$(awk '$1=="V"{print $2}' "$D/nvidia.txt"); S=$(awk '$1=="S"{print $2}' "$D/nvidia.txt")
W=${WORK:-$(mktemp -d)}; mkdir -p "$W" "$OUT"
RUNF=${NV_RUN:-$W/NVIDIA-Linux-x86_64-$V.run}
[ -s "$RUNF" ] || curl -fsSL --retry 3 -o "$RUNF" "https://download.nvidia.com/XFree86/Linux-x86_64/$V/NVIDIA-Linux-x86_64-$V.run"
GOT=$(sha256sum "$RUNF" | cut -c1-64)
if [ "$S" != "$GOT" ]; then echo "nvidia: NVIDIA-Linux-x86_64-$V.run has SHA-256 $GOT; nvidia.txt pins ${S:-nothing}"; [ -n "$NV_UNPINNED" ] || exit 1; echo "::warning title=nvidia.txt::not pinned yet: S $GOT"; fi
rm -rf "$W/x"; sh "$RUNF" --extract-only --target "$W/x" >/dev/null
X=$W/x
# ---- the licence: it must still allow redistribution of the unmodified files. Since the 590s, NVIDIA's own licence (the
# .run's LICENSE, "License for Customer Use of NVIDIA Software") says, in section 1, "you may ... d. Distribute the SOFTWARE
# provided for use with operating system kernels distributed under the terms of an OSI-approved open source license as
# listed by the Open Source Initiative at http://opensource.org, provided that (i) the binary files thereof are not modified
# in any way (except for uncompressing of compressed files) and (ii) this Agreement is provided to each SOFTWARE recipient."
# (Linux is GPL-2.0.) Older drivers had it as 2.1.2, the "Linux/FreeBSD Exception". The build stops if neither is there.
L=$X/LICENSE; [ -f "$L" ] || { echo "nvidia: the .run has no LICENSE"; exit 1; }
tr -s ' \n\t' '   ' < "$L" > "$W/lic1"
if grep -q 'Distribute the SOFTWARE provided for use with operating system kernels distributed under the terms of an OSI-approved open source license' "$W/lic1" &&
   grep -q 'provided that (i) the binary files thereof are not modified in any way (except for uncompressing of compressed files) and (ii) this Agreement is provided to each SOFTWARE recipient' "$W/lic1"; then
	echo "nvidia: $V's licence, section 1.d: redistribution of the unmodified files with an OSI-licensed kernel, the licence given to each recipient"
elif grep -q 'Linux/FreeBSD Exception' "$W/lic1" && grep -q 'may be copied and redistributed, provided that the binary files thereof are not modified in any way' "$W/lic1"; then
	echo "nvidia: $V's licence, 2.1.2 (Linux/FreeBSD Exception): redistribution of the unmodified files"
else echo "nvidia: the licence of $V no longer allows redistribution in the words checked here: stop, and read it again"
	ls "$X" | grep -i 'licen\|copying\|eula'; grep -n -i -B3 -A8 'distribut\|FreeBSD\|Linux' "$L" | head -150; exit 1; fi
grep -q 'licensed for use only on GeForce or Titan hardware products' "$W/lic1" && echo "nvidia: (its 2.8: GeForce and Titan software for GeForce and Titan cards only: Rudolf OS offers it only for those)"
# ---- the kernel tree (the release's version and configuration), prepared for external modules
if [ -n "$LINUX_SRC" ]; then K=$LINUX_SRC; else
	KV0=${KVER%-rudolf}; K=$W/linux; rm -rf "$K"
	for i in 1 2; do git clone -q --depth 1 -b "v$KV0" https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git "$K" && break; rm -rf "$K"; done
	if [ ! -d "$K/kernel" ]; then T=linux-$KV0.tar.xz; M=${KV0%%.*}
		curl -fsSL --retry 3 -o "$W/$T" "https://cdn.kernel.org/pub/linux/kernel/v$M.x/$T"; curl -fsSL --retry 3 -o "$W/sums" "https://cdn.kernel.org/pub/linux/kernel/v$M.x/sha256sums.asc"
		[ "$(awk -v f="$T" '$2==f{print $1}' "$W/sums")" = "$(sha256sum "$W/$T" | cut -c1-64)" ] || { echo "nvidia: $T does not match kernel.org's checksum"; exit 1; }
		mkdir -p "$K" && tar -xJf "$W/$T" -C "$K" --strip-components=1 && rm -f "$W/$T"; fi
	# (the configuration as the release's: kconfig.sh with the Secure Boot certificate the kernel trusts; no initramfs is built)
	# (an empty start-up image: this kernel is only built for its Module.symvers, never started)
	echo 'dir /dev 0755 0 0' > "$W/initramfs.list"
	SB_CERT=${SB_CERT:-$I/boot/sb/rudolf-sb.crt} sh "$I/kconfig.sh" "$K" "$W/initramfs.list" >/dev/null
	# (the whole kernel and its modules are built, for Module.symvers: the open modules' build needs it, and so does a
	#  dependency on one of the kernel's own modules; the release's kernel itself is not replaced, only compared)
	make -C "$K" -s olddefconfig >/dev/null; make -C "$K" -s -j"$(nproc)" vmlinux modules >/dev/null 2>"$W/kernel.log" || { tail -30 "$W/kernel.log"; exit 1; }
fi
[ "$(make -C "$K" -s kernelrelease)" = "$KVER" ] || { echo "nvidia: the kernel tree is $(make -C "$K" -s kernelrelease), not $KVER"; exit 1; }
# ---- the open kernel modules
make -C "$X/kernel-open" -s -j"$(nproc)" SYSSRC="$K" SYSOUT="$K" KBUILD_MODPOST_WARN=1 modules >/dev/null 2>"$W/kbuild.log" || { tail -40 "$W/kbuild.log"; exit 1; }
MD=$OUT/lib/modules/$KVER/extra/nvidia; mkdir -p "$MD"
for m in nvidia nvidia-modeset nvidia-drm nvidia-uvm nvidia-peermem; do f=$X/kernel-open/$m.ko; [ -f "$f" ] || continue
	[ "$m" = nvidia-peermem ] && continue   # (InfiniBand only)
	strip --strip-debug "$f"; cp "$f" "$MD/"; done
for m in nvidia nvidia-modeset nvidia-drm nvidia-uvm; do [ -f "$MD/$m.ko" ] || { echo "nvidia: $m.ko was not built"; exit 1; }; done
VM=$(modinfo -F vermagic "$MD/nvidia.ko")
if [ -n "$MODULES_SFS" ]; then REF=$(unsquashfs -l "$MODULES_SFS" | grep -m1 '/kernel/drivers/gpu/drm/nouveau/nouveau.ko$' | sed 's#^squashfs-root/##')
	rm -rf "$W/ref"; unsquashfs -q -d "$W/ref" "$MODULES_SFS" "$REF" >/dev/null; RV=$(modinfo -F vermagic "$W/ref/$REF")
	[ "$RV" = "$VM" ] || { echo "nvidia: these modules are for \"$VM\", the release's for \"$RV\": not the same kernel"; exit 1; }
	echo "nvidia: vermagic as the release's modules: $VM"; fi
if [ -n "$MODSIGN_KEY" ] && [ -n "$MODSIGN_CERT" ]; then
	for f in "$MD"/*.ko; do "$K/scripts/sign-file" sha256 "$MODSIGN_KEY" "$MODSIGN_CERT" "$f"; done
	echo "nvidia: modules signed with $(openssl x509 -in "$MODSIGN_CERT" -noout -subject)"
else echo "nvidia: modules NOT signed (a check build): no Rudolf OS kernel loads them"; fi
# ---- the user-space driver, unmodified (cp keeps every byte; links made beside them)
LB=$OUT/usr/lib/x86_64-linux-gnu; mkdir -p "$LB" "$OUT/usr/lib/xorg/modules/drivers" "$OUT/usr/lib/xorg/modules/extensions" \
	"$OUT/usr/share/vulkan/icd.d" "$OUT/usr/share/vulkan/implicit_layer.d" "$OUT/usr/share/glvnd/egl_vendor.d" "$OUT/usr/share/egl/egl_external_platform.d" \
	"$OUT/usr/lib/x86_64-linux-gnu/gbm" "$OUT/usr/lib/x86_64-linux-gnu/nvidia/wine" "$OUT/lib/firmware/nvidia/$V" "$OUT/usr/bin" "$OUT/usr/share/nvidia" "$OUT/licenses"
c() { [ -e "$X/$1" ] || { [ -n "$2" ] && return 0; echo "nvidia: $1 is not in $V"; exit 1; }; cp -p "$X/$1" "$3${3:+/}"; }
lib() { f=$1; [ -e "$X/$f" ] || { [ "$2" = opt ] && return 0; echo "nvidia: $f is not in $V"; exit 1; }; cp -p "$X/$f" "$LB/"; }
for f in libGLX_nvidia libEGL_nvidia libGLESv2_nvidia libGLESv1_CM_nvidia; do lib "$f.so.$V"; done
for f in libnvidia-glcore libnvidia-eglcore libnvidia-glsi libnvidia-tls libnvidia-glvkspirv libnvidia-gpucomp libnvidia-allocator \
	libnvidia-ml libcuda libnvidia-ptxjitcompiler libnvidia-rtcore libnvoptix libnvidia-ngx libnvidia-encode libnvcuvid libnvidia-opticalflow \
	libnvidia-cfg libnvidia-api; do lib "$f.so.$V" opt; done
for f in "$X"/libnvidia-egl-gbm.so.* "$X"/libnvidia-egl-xcb.so.* "$X"/libnvidia-egl-xlib.so.* "$X"/libnvidia-api.so.*; do [ -f "$f" ] && cp -p "$f" "$LB/"; done
[ -f "$LB/libGLX_nvidia.so.$V" ] && [ -f "$LB/libnvidia-glcore.so.$V" ] || { echo "nvidia: the OpenGL libraries are missing"; exit 1; }
# the names programs and glvnd ask for
( cd "$LB" && for f in *.so.$V; do b=${f%.so.$V}; case "$b" in libGLX_nvidia|libEGL_nvidia) ln -sf "$f" "$b.so.0";; libGLESv2_nvidia) ln -sf "$f" "$b.so.2";;
	libGLESv1_CM_nvidia) ln -sf "$f" "$b.so.1";; libnvidia-ml|libcuda|libnvidia-encode|libnvcuvid|libnvidia-opticalflow|libnvidia-ptxjitcompiler|libnvidia-cfg|libnvidia-ngx) ln -sf "$f" "$b.so.1"; ln -sf "$b.so.1" "$b.so";; esac; done
  [ -f libnvidia-allocator.so.$V ] && ln -sf ../libnvidia-allocator.so.$V gbm/nvidia-drm_gbm.so; : )
c nvidia_drv.so '' "$OUT/usr/lib/xorg/modules/drivers"
c "libglxserver_nvidia.so.$V" '' "$OUT/usr/lib/xorg/modules/extensions"; ln -sf "libglxserver_nvidia.so.$V" "$OUT/usr/lib/xorg/modules/extensions/libglxserver_nvidia.so"
# Vulkan (the ICD file names the library by name: found through the system's library path), EGL's vendor files
sed "s#\"library_path\": *\"[^\"]*\"#\"library_path\": \"libGLX_nvidia.so.0\"#" "$X/nvidia_icd.json" > "$OUT/usr/share/vulkan/icd.d/nvidia_icd.json"
[ -f "$X/nvidia_layers.json" ] && cp -p "$X/nvidia_layers.json" "$OUT/usr/share/vulkan/implicit_layer.d/"
c 10_nvidia.json '' "$OUT/usr/share/glvnd/egl_vendor.d"
for f in 15_nvidia_gbm.json 20_nvidia_xcb.json 20_nvidia_xlib.json; do c "$f" opt "$OUT/usr/share/egl/egl_external_platform.d"; done
# Proton's DLSS (nvngx.dll) where Proton looks for it, the application profiles, the GSP firmware, nvidia-smi
for f in nvngx.dll _nvngx.dll nvngx_dlssg.dll; do c "$f" opt "$OUT/usr/lib/x86_64-linux-gnu/nvidia/wine"; done
c "nvidia-application-profiles-$V-rc" opt "$OUT/usr/share/nvidia"; c "nvidia-application-profiles-$V-key-documentation" opt "$OUT/usr/share/nvidia"
for f in "$X"/firmware/*.bin; do cp -p "$f" "$OUT/lib/firmware/nvidia/$V/"; done
ls "$OUT/lib/firmware/nvidia/$V/"gsp_*.bin >/dev/null 2>&1 || { echo "nvidia: no GSP firmware in $V (the open modules need it)"; exit 1; }
c nvidia-smi '' "$OUT/usr/bin"
# (NVIDIA's libraries still ask for librt.so.1, which glibc 2.34 folded into libc and the base leaves out: the build machine's
#  stub, Ubuntu 24.04's libc6 as the base's own C library, LGPL-2.1-or-later; mkext.sh leaves it out if a base has it)
MA=$(gcc -print-multiarch 2>/dev/null || echo x86_64-linux-gnu)
[ -f "/usr/lib/$MA/librt.so.1" ] && cp -L "/usr/lib/$MA/librt.so.1" "$LB/librt.so.1" && echo "librt.so.1 is glibc's (Ubuntu's libc6, LGPL-2.1-or-later): the C library's own stub, for NVIDIA's libraries" > "$OUT/licenses/librt"
# (not shipped: nvidia-modprobe and nvidia-persistenced, setuid or daemons; nvidia-settings; the 32-bit libraries: 32-bit
#  games run in Steam's Flatpak, which installs NVIDIA's matching libraries itself; Wayland's EGL platform: Rudolf OS is X11)
cp -p "$L" "$OUT/licenses/NVIDIA-LICENSE"; cp -p "$X/kernel-open/COPYING" "$OUT/licenses/NVIDIA-open-kernel-modules-COPYING" 2>/dev/null || true
printf 'The kernel modules (nvidia, nvidia-modeset, nvidia-drm, nvidia-uvm) are built from NVIDIA open GPU kernel modules %s\n(MIT/GPL-2.0, dual-licensed): https://github.com/NVIDIA/open-gpu-kernel-modules/tree/%s\n(the same source is in kernel-open/ of NVIDIA-Linux-x86_64-%s.run). The other files are NVIDIA'"'"'s, unmodified\n(NVIDIA-LICENSE, section 1.d).\n' "$V" "$V" "$V" > "$OUT/licenses/SOURCE"
# ---- the cards offered: GeForce (and TITAN) cards the open kernel modules support ("kernelopen"): GeForce 16 and newer
python3 - "$X/supported-gpus/supported-gpus.json" "$OUT/usr/share/nvidia/rudolf-gpus.txt" "$V" <<'EOF'
import json, sys
j = json.load(open(sys.argv[1])); seen = {}
for c in j.get('chips', []):
    if 'kernelopen' not in c.get('features', []): continue
    n = c.get('name', ''); i = c.get('devid', '').lower().replace('0x', '')
    if not i or not any(w in n for w in ('GeForce', 'TITAN')): continue
    # (Turing and newer, the GTX 16 and RTX 20 series on: device IDs from 0x1e00; 595's list also marks older chips, which
    #  the open modules cannot start: they have no GSP)
    if int(i, 16) < 0x1e00: continue
    seen.setdefault(i, n)
import os; os.makedirs(os.path.dirname(sys.argv[2]), exist_ok=True)
with open(sys.argv[2], 'w') as f:
    f.write(f'# GeForce 16 and newer, by PCI ID: NVIDIA {sys.argv[3]}\'s supported-gpus.json ("kernelopen"), GeForce and TITAN\n# names, Turing and newer (from 10de:1e00). Made by iso/ext/nvidia-driver/build.sh. ID<TAB>name\n')
    for i in sorted(seen): f.write(f'10de:{i}\t{seen[i]}\n')
print(f'nvidia: {len(seen)} cards offered'); print(open(sys.argv[2]).read(), end='')
EOF
echo "nvidia: $(du -sm "$OUT" | cut -f1) MB of files for $KVER"
