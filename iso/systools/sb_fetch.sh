#!/bin/bash
# Fetch the Secure Boot parts Rudolf OS ships (Rudolf OS 1.2.2), from Ubuntu 24.04's packages:
#   shimx64.efi   shim, signed by Microsoft (Ubuntu's shim-signed)  - the first program the PC starts
#   mmx64.efi     MokManager, signed by Microsoft                   - where a person enrolls the Rudolf key
#   systemd-bootx64.efi                                             - the boot menu (signed by Rudolf OS later)
#   linuxx64.efi.stub                                               - the stub of the kernel bundle (UKI)
# and, for the tests only (never on a disc): OVMF with Secure Boot (OVMF_CODE_4M.secboot.fd, OVMF_VARS_4M.ms.fd
# with Microsoft's keys enrolled) and sbsign/sbverify.
#   sb_fetch.sh OUTDIR          (on Ubuntu 24.04, as in the release workflow; needs apt-get download)
# Writes OUTDIR/sb-parts.txt: each file's package, version and SHA-256. GPL-2.0-or-later.
set -e
OUT=$(realpath -m "$1"); [ -n "$1" ] || { echo "usage: $0 OUTDIR"; exit 1; }
W=$(mktemp -d); mkdir -p "$OUT"
cd "$W"
apt-get download shim-signed systemd-boot-efi ovmf sbsigntools >/dev/null
for d in *.deb; do dpkg-deb -x "$d" x; done
find x -type f \( -name '*.efi*' -o -name '*.fd' -o -name 'sbsign' -o -name 'sbverify' \) | sort
pick() { local f; f=$(find x -type f -path "*$1" | head -1); [ -n "$f" ] || { echo "not found in the packages: $1"; exit 1; }; cp "$f" "$OUT/$2"; }
pick shim/shimx64.efi.signed.latest shimx64.efi
pick shim/mmx64.efi mmx64.efi
pick boot/efi/systemd-bootx64.efi systemd-bootx64.efi
pick boot/efi/linuxx64.efi.stub linuxx64.efi.stub
pick OVMF/OVMF_CODE_4M.secboot.fd OVMF_CODE_4M.secboot.fd
pick OVMF/OVMF_VARS_4M.ms.fd OVMF_VARS_4M.ms.fd
pick bin/sbsign sbsign
pick bin/sbverify sbverify
{ for d in *.deb; do dpkg-deb -f "$d" Package Version | paste -sd' '; done
  echo; (cd "$OUT" && sha256sum shimx64.efi mmx64.efi systemd-bootx64.efi linuxx64.efi.stub OVMF_CODE_4M.secboot.fd OVMF_VARS_4M.ms.fd sbsign sbverify); } > "$OUT/sb-parts.txt"
# the licence texts that go with what a disc ships
mkdir -p "$OUT/licences"
for p in shim-signed systemd-boot-efi; do f=$(find x -path "*/doc/$p/copyright" | head -1); [ -n "$f" ] && cp "$f" "$OUT/licences/$p.copyright"; done
cat "$OUT/sb-parts.txt"
rm -rf "$W"
