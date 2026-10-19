#!/bin/bash
# Add NVK (Mesa's Vulkan driver for NVIDIA cards) and the Vulkan loader to a base layer tree (Rudolf OS 1.2.3).
#   addnvk.sh ROOT
# Mesa 25 draws OpenGL on GeForce 20 series and newer through Zink on NVK (Zink is in libgallium, which the base
# layer has); without a Vulkan driver those cards get no OpenGL, so Xorg has no glamor. Takes the files from this
# Ubuntu 24.04 (mesa-vulkan-drivers, libvulkan1) and the libraries they need that ROOT lacks. Only NVK: the other
# Vulkan drivers are not needed for this. GPL-2.0-or-later.
set -e
# local test builds without Ubuntu's packages (never set in the workflow)
[ -n "$RUDOLF_NO_NVK" ] && { echo "addnvk: skipped (RUDOLF_NO_NVK)"; exit 0; }
R=$1; [ -d "$R/usr/lib/x86_64-linux-gnu" ] || { echo "usage: $0 ROOT"; exit 1; }
ML=/usr/lib/x86_64-linux-gnu; ICD=/usr/share/vulkan/icd.d/nouveau_icd.x86_64.json
for f in "$ML/libvulkan.so.1" "$ML/libvulkan_nouveau.so" "$ICD"; do [ -e "$f" ] || { echo "addnvk: $f is missing (apt-get install mesa-vulkan-drivers libvulkan1)"; exit 1; }; done
SUDO=; [ -w "$R/usr/lib/x86_64-linux-gnu" ] || SUDO=sudo
$SUDO cp -L "$ML/libvulkan.so.1" "$ML/libvulkan_nouveau.so" "$R/usr/lib/x86_64-linux-gnu/"
$SUDO mkdir -p "$R/usr/share/vulkan/icd.d"; $SUDO cp "$ICD" "$R/usr/share/vulkan/icd.d/"
# the libraries NVK and the loader need, from this system when ROOT does not have them
n=0
for l in $(ldd "$ML/libvulkan_nouveau.so" "$ML/libvulkan.so.1" | awk '/=>/{print $3}' | sort -u); do
	b=$(basename "$l")
	[ -e "$R$ML/$b" ] || [ -e "$R/lib/x86_64-linux-gnu/$b" ] || [ -e "$R/usr/lib/$b" ] || { $SUDO cp -L "$l" "$R$ML/"; n=$((n + 1)); echo "addnvk: added $b"; }
done
echo "addnvk: NVK $(dpkg-query -W -f '${Version}' mesa-vulkan-drivers 2>/dev/null), $n libraries added"
