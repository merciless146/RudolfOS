#!/bin/bash
# Add NVK (Mesa's Vulkan driver for NVIDIA cards) and the Vulkan loader to a base layer tree (Rudolf OS 1.2.3).
#   addnvk.sh ROOT
# Mesa 25 draws OpenGL on GeForce 20 series and newer through Zink on NVK (Zink is in libgallium, which the base
# layer has); without a Vulkan driver those cards get no OpenGL, so Xorg has no glamor. NVK comes from Ubuntu
# 24.04's mesa-vulkan-drivers when that package has it, else from the kisak-mesa PPA's build for 24.04 (only
# libvulkan_nouveau.so and its ICD file are taken). The libraries it needs that ROOT lacks are copied from this
# system. Without NVK the release is still made (a warning): NVIDIA cards then draw as before. GPL-2.0-or-later.
set -e
# local test builds without Ubuntu's packages (never set in the workflow)
[ -n "$RUDOLF_NO_NVK" ] && { echo "addnvk: skipped (RUDOLF_NO_NVK)"; exit 0; }
R=$1; [ -d "$R/usr/lib/x86_64-linux-gnu" ] || { echo "usage: $0 ROOT"; exit 1; }
ML=/usr/lib/x86_64-linux-gnu
warn() { echo "::warning title=NVK::$*"; echo "addnvk: $*"; exit 0; }
# where NVK is: the installed mesa-vulkan-drivers, or the PPA's package unpacked in a temporary folder
find_nvk() {   # $1: a root to look in; prints the library and the ICD file
	local lib icd
	lib=$(find "$1/usr/lib/x86_64-linux-gnu" -maxdepth 1 -name 'libvulkan_nouveau.so' 2>/dev/null | head -1)
	icd=$(find "$1/usr/share/vulkan/icd.d" -maxdepth 1 -name 'nouveau_icd*.json' 2>/dev/null | head -1)
	[ -n "$lib" ] && [ -n "$icd" ] && echo "$lib $icd"; return 0
}
FROM="Ubuntu $(dpkg-query -W -f '${Version}' mesa-vulkan-drivers 2>/dev/null || true)"
NV=$(find_nvk "")
if [ -z "$NV" ]; then
	echo "addnvk: Ubuntu's mesa-vulkan-drivers has no NVK: $(ls /usr/share/vulkan/icd.d/ 2>/dev/null | tr '\n' ' ')"
	T=$(mktemp -d)
	if sudo add-apt-repository -y -n ppa:kisak/kisak-mesa >/dev/null 2>&1 && sudo apt-get update -qq >/dev/null 2>&1 &&
	   (cd "$T" && apt-get download mesa-vulkan-drivers >/dev/null 2>&1); then
		dpkg-deb -x "$T"/mesa-vulkan-drivers_*.deb "$T/x"
		NV=$(find_nvk "$T/x"); FROM="kisak-mesa PPA $(dpkg-deb -f "$T"/mesa-vulkan-drivers_*.deb Version)"
	fi
	sudo add-apt-repository -y -r ppa:kisak/kisak-mesa >/dev/null 2>&1 || true
	[ -n "$NV" ] || warn "no NVK found (Ubuntu or the kisak-mesa PPA); NVIDIA GeForce 20+ cards get no OpenGL acceleration in this release"
fi
set -- $NV; LIB=$1; ICD=$2
[ -e "$ML/libvulkan.so.1" ] || warn "the Vulkan loader (libvulkan1) is not installed"
SUDO=; [ -w "$R/usr/lib/x86_64-linux-gnu" ] || SUDO=sudo
$SUDO cp -L "$ML/libvulkan.so.1" "$LIB" "$R/usr/lib/x86_64-linux-gnu/"
$SUDO mkdir -p "$R/usr/share/vulkan/icd.d"
# the ICD file names the library by path or by name; Rudolf OS keeps it in the usual place
$SUDO sh -c "sed 's#\"library_path\": *\"[^\"]*\"#\"library_path\": \"$ML/libvulkan_nouveau.so\"#' '$ICD' > '$R/usr/share/vulkan/icd.d/nouveau_icd.x86_64.json'"
# the libraries NVK and the loader need, from this system when ROOT does not have them
n=0; miss=""
for l in $(ldd "$LIB" "$ML/libvulkan.so.1" | awk '/=>/{print $3}' | sort -u); do
	b=$(basename "$l")
	[ -e "$R$ML/$b" ] || [ -e "$R/lib/x86_64-linux-gnu/$b" ] || [ -e "$R/usr/lib/$b" ] || { $SUDO cp -L "$l" "$R$ML/"; n=$((n + 1)); echo "addnvk: added $b"; }
done
miss=$(ldd "$LIB" | awk '/not found/{print $1}' | tr '\n' ' ')
[ -z "$miss" ] || echo "::warning title=NVK::libraries not found on the build system: $miss"
echo "::notice title=NVK::NVK from $FROM, $n libraries added to the base layer"
echo "addnvk: NVK from $FROM, $n libraries added"
