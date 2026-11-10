#!/bin/sh
# 1.5.6: rudolfd's apps_locked() and debian_locked(): with Encrypt my data, /persist/flatpak is a link into the encrypted folder and
# /var/lib/flatpak is mounted from it at sign-in; until then the apps' list and icons answer "locked" (423) instead of
# an empty list. The function from rudolfd.c, in a mount namespace with stand-in folders.   sh applocked_t.sh (root)
HERE=$(cd "$(dirname "$0")" && pwd); C=$HERE/../../sys/rudolfd.c
[ "$(id -u)" = 0 ] || { echo "run as root"; exit 2; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
{ echo '#include <sys/stat.h>'; echo '#include <stdio.h>'; echo '#define PERSIST "/persist"'
  sed -n '/^static int apps_locked(void)/,/^}/p;/^static int debian_locked(void)/,/^}/p' "$C"; printf '%s\n' 'int main(int c,char**v){return printf("%d", c>1?debian_locked():apps_locked()) < 0;}'; } > $W/t.c
grep -q apps_locked $W/t.c && gcc -o $W/t $W/t.c || { echo "FAIL apps_locked not found"; exit 1; }
unshare -m --propagation private sh -s "$W" <<'EOF'
W=$1; P=0; F=0; ok() { if [ "$2" = "$3" ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 - $2"; fi; }
mkdir -p /persist /var/lib/flatpak 2>/dev/null; mount -t tmpfs t /persist; mount -t tmpfs t /var/lib
mkdir -p /var/lib/flatpak /persist/vault/flatpak
mkdir -p /persist/flatpak; ok "1. apps not in the encrypted folder: never locked" "$($W/t)" 0
rmdir /persist/flatpak; ln -s vault/flatpak /persist/flatpak
ok "2. in the encrypted folder, not open yet (/var/lib/flatpak not mounted): locked" "$($W/t)" 1
mount --bind /persist/vault/flatpak /var/lib/flatpak
ok "3. opened at sign-in (mounted): not locked" "$($W/t)" 0
# Debian: /persist/debian a link into the encrypted folder, whose names are scrambled until it is open
mkdir -p /persist/debian; ok "4. Debian not in the encrypted folder: never locked" "$($W/t d)" 0
rmdir /persist/debian; ln -s /persist/vault/debian /persist/debian
ok "5. Debian in the encrypted folder, not open (its folder not to be found): locked" "$($W/t d)" 1
mkdir -p /persist/vault/debian; ok "6. open: not locked" "$($W/t d)" 0
echo "$P/$((P+F)) passed"; [ $F = 0 ]
EOF
