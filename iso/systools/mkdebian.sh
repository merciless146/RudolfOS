#!/bin/bash
# Build the Debian 13 base for Rudolf OS's Linux terminal (rudolf-debian setup downloads it).
#   mkdebian.sh OUTDIR KEYFILE
# Starts from Debian's official minimal image for trixie/amd64 as published by the Debian Docker image
# maintainers (github.com/debuerreotype/docker-debian-artifacts, branch dist-amd64), checks it against the
# image's own layer checksum, adds the Rudolf OS settings below, and writes:
#   debian-13-rudolf.tar.xz   the root file system (numeric owners, as Debian made them)
#   debian.txt                version, file name, size and SHA-256
#   debian.txt.sig            Ed25519 signature of debian.txt (the key the system updates are signed with)
# GPL-2.0-or-later (this script; Debian's packages keep their own licences).
set -e
OUT=$(realpath -m "$1"); KEY=$2; HERE=$(cd "$(dirname "$0")/.." && pwd)
[ -f "$KEY" ] || { echo "usage: mkdebian.sh OUTDIR KEYFILE"; exit 1; }
SRC=${DEBIAN_ARTIFACTS:-$HERE/src/debian-artifacts}
if [ ! -f "$SRC/trixie/oci/blobs/rootfs.tar.gz" ]; then
	rm -rf "$SRC"
	git clone -q --depth 1 --branch dist-amd64 --filter=blob:none --sparse https://github.com/debuerreotype/docker-debian-artifacts "$SRC"
	git -C "$SRC" sparse-checkout set trixie
fi
A=$SRC/trixie
# the layer's checksum (of the uncompressed tar) is recorded in the image configuration
WANT=$(python3 -c "import json,sys;print(json.load(open(sys.argv[1]))['rootfs']['diff_ids'][0].split(':')[1])" $A/oci/blobs/image-config.json)
GOT=$(gzip -dc $A/oci/blobs/rootfs.tar.gz | sha256sum | cut -d' ' -f1)
[ "$WANT" = "$GOT" ] || { echo "the Debian image does not match its checksum"; exit 1; }
VER=$(cat $A/rootfs.debian_version)
R=$(mktemp -d); trap 'rm -rf "$R"' EXIT
tar -C "$R" --numeric-owner -xzf $A/oci/blobs/rootfs.tar.gz

# ---- Rudolf OS settings
echo rudolf > $R/etc/hostname
printf '127.0.0.1\tlocalhost\n127.0.1.1\trudolf\n::1\t\tlocalhost ip6-localhost ip6-loopback\n' > $R/etc/hosts
: > $R/etc/resolv.conf            # the system's own file is placed over this one at start
# the user: same name and number as on Rudolf OS, so files in the home folder belong to them on both sides
grep -q '^rudolf:' $R/etc/passwd || {
	echo 'rudolf:x:1000:1000:Rudolf:/home/rudolf:/bin/bash' >> $R/etc/passwd
	echo 'rudolf:x:1000:' >> $R/etc/group
	echo 'rudolf:!:20000:0:99999:7:::' >> $R/etc/shadow
}
for g in sudo audio video plugdev users; do sed -i "s/^\($g:x:[0-9]*:\)\(.*\)$/\1\2,rudolf/;s/:,rudolf$/:rudolf/" $R/etc/group; done
mkdir -p $R/etc/sudoers.d; echo 'rudolf ALL=(ALL:ALL) ALL' > $R/etc/sudoers.d/rudolf; chmod 440 $R/etc/sudoers.d/rudolf
mkdir -p $R/home/rudolf
# "sudo" before Debian's sudo package is installed: su to root (same password as the user)
cat > $R/usr/local/bin/sudo <<'S'
#!/bin/sh
# Rudolf OS: runs a command as root inside Debian (asks for the password chosen at setup).
# "sudo apt install sudo" replaces this with Debian's own sudo.
[ -x /usr/bin/sudo ] && exec /usr/bin/sudo "$@"
case "$1" in -*) shift;; esac
[ $# -gt 0 ] || { echo "usage: sudo COMMAND"; exit 1; }
[ "$(id -u)" = 0 ] && exec "$@"
q=""; for a in "$@"; do q="$q '$(printf %s "$a" | sed "s/'/'\\\\''/g")'"; done
printf "[sudo] your Linux password (set when Debian was set up)\n" >&2
exec su root -s /bin/sh -c "exec $q"
S
chmod 755 $R/usr/local/bin/sudo
# packages must not try to start background services (there is no systemd inside)
printf '#!/bin/sh\nexit 101\n' > $R/usr/sbin/policy-rc.d; chmod 755 $R/usr/sbin/policy-rc.d
# packages that only make sense on a whole computer are kept out
cat > $R/etc/apt/preferences.d/rudolf <<'P'
# Rudolf OS: Debian runs inside Rudolf OS, which provides the kernel, boot loader and init system.
Package: linux-image-* linux-headers-* grub* systemd-sysv init firmware-* snapd
Pin: release *
Pin-Priority: -1
P
cat > $R/etc/profile.d/rudolf.sh <<'P'
export LANG=${LANG:-C.UTF-8}
export DISPLAY=${DISPLAY:-:0}
export XDG_RUNTIME_DIR=/run/user/$(id -u)
[ -S /run/user/1000/pulse/native ] && export PULSE_SERVER=unix:/run/user/1000/pulse/native
[ -S /run/user/1000/bus ] && [ "$(id -u)" = 1000 ] && export DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus
P
cat > $R/etc/motd <<'M'

Debian GNU/Linux 13 on Rudolf OS. Install software with:  sudo apt install NAME
Your Documents, Pictures and Downloads are in your home folder (~). Graphical apps you
install appear in the Start menu under "Linux (Debian)". Type "exit" to close this tab.

M
# a coloured prompt for the user
sed -i 's/^#force_color_prompt=yes/force_color_prompt=yes/' $R/etc/skel/.bashrc 2>/dev/null || true
echo "$VER" > $R/etc/rudolf-debian
# the real home folder is placed over this one; setup copies /etc/skel into it when it has no .bashrc
mkdir -p $R/home/rudolf $R/run/user/1000
chown 1000:1000 $R/home/rudolf

mkdir -p "$OUT"
F=debian-13-rudolf.tar.xz
tar -C "$R" --numeric-owner --format=gnu -cf - . | xz -9 -T0 > "$OUT/$F"
printf 'debian %s\nfile %s\nsize %s\nsha256 %s\nbuilt %s\n' "$VER" "$F" "$(stat -c %s "$OUT/$F")" "$(sha256sum "$OUT/$F" | cut -d' ' -f1)" "$(date -u +%F)" > "$OUT/debian.txt"
python3 - "$KEY" "$OUT/debian.txt" <<'PY'
import sys,base64
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
k=Ed25519PrivateKey.from_private_bytes(base64.b64decode(open(sys.argv[1]).read().strip()))
open(sys.argv[2]+'.sig','wb').write(k.sign(open(sys.argv[2],'rb').read()))
PY
cat "$OUT/debian.txt"; ls -la "$OUT"
