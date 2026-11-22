#!/bin/bash
# Rudolf Kryptonite (2.0): the Secure Boot parts iso/boot/sb/parts.txt pins, into DIR, for the QEMU checks only
# (.github/workflows/kryptonite-kernel.yml): Ubuntu's packages by systools/sb_fetch.sh, and when Ubuntu has a newer package
# than the pinned one, the pinned version's package from Launchpad's archive (which keeps every published version). Every
# file is checked against its pin either way.   kr20_sbparts.sh DIR   GPL-2.0-or-later.
set -e
D=$(realpath -m "${1:-parts}"); I=$(cd "$(dirname "$0")/../.." && pwd); PT=$I/boot/sb/parts.txt; T=$(mktemp -d)
bash "$I/systools/sb_fetch.sh" "$D" >/dev/null
sed -n 's/^#.*\(shim-signed [^ ,]*\), \(systemd-boot-efi [^ ]*\)\. .*/\1\n\2/p' "$PT" | while read -r P V; do
	need=0
	case $P in shim-signed) F="shimx64.efi:shimx64.efi.signed.latest mmx64.efi:mmx64.efi";; *) F="systemd-bootx64.efi:systemd-bootx64.efi linuxx64.efi.stub:linuxx64.efi.stub";; esac
	for f in $F; do grep " ${f%%:*}\$" "$PT" | (cd "$D" && sha256sum -c --quiet) >/dev/null 2>&1 || need=1; done
	[ $need = 0 ] && { echo "kr20_sbparts: $P: Ubuntu's is the pinned $V"; continue; }
	echo "kr20_sbparts: $P: Ubuntu's is newer; the pinned $V from Launchpad"
	curl -fsSL --retry 3 -o "$T/p.deb" "https://launchpad.net/ubuntu/+archive/primary/+files/${P}_${V}_amd64.deb"
	rm -rf "$T/x"; dpkg-deb -x "$T/p.deb" "$T/x"
	for f in $F; do x=$(find "$T/x" -name "${f#*:}" | head -1); [ -n "$x" ] && cp "$x" "$D/${f%%:*}"; done
done
rm -rf "$T"
for f in shimx64.efi mmx64.efi systemd-bootx64.efi linuxx64.efi.stub; do
	grep " $f\$" "$PT" | (cd "$D" && sha256sum -c --quiet) || { echo "::error::$f is not the pinned one"; exit 1; }
done
echo "kr20_sbparts: the four pinned parts are in $D"
