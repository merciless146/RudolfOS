#!/bin/sh
# 1.5.6: Debian and the signed-in account's folders. rudolfd binds the account's Documents (and the rest) over
# /home/rudolf's at sign-in; when Debian starts after that, it must not take that bind in with its copy of
# /home/rudolf: copied into Debian's user namespace it is locked in place, so rebind cannot take it off at sign-out and
# the account's folder stays in use (QEMU t156 on 2026110918: its folders' names stayed readable after sign-out).
#   sh debhome_t.sh      (root; builds rudolf-debian; runs in a mount namespace of its own, with /persist, /home and
#                         /run/rudolf as empty folders, and a stand-in Debian made of this computer's sh and ls)
set -u
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys
[ "$(id -u)" = 0 ] || { echo "run as root"; exit 2; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
gcc -O2 -w -o "$W/rudolf-debian" "$SYS/rudolf-debian.c" -lcrypto 2>"$W/cc.txt" || { cat "$W/cc.txt"; exit 1; }
unshare -m --propagation private sh -s "$W" <<'EOF'
W=$1; P=0; F=0
ok() { if [ "$2" = 1 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
mkdir -p /persist /home /run/rudolf /usr/bin 2>/dev/null
mount -t tmpfs t /persist && mount -t tmpfs t /home && mount -t tmpfs t /run/rudolf || { echo "cannot make the folders"; exit 1; }
# as rudolf-init: /home/rudolf is /persist/home/rudolf
mkdir -p /persist/home/rudolf/Documents /home/rudolf && mount --bind /persist/home/rudolf /home/rudolf
# a stand-in Debian (Debian's own root is 200000, as rudolf-debian unpacks it)
D=/persist/debian; mkdir -p $D/bin $D/etc $D/proc $D/sys $D/dev $D/tmp $D/run $D/home/rudolf $D/root
for b in sh ls cat; do p=$(command -v $b); cp -L "$p" $D/bin/$b; for l in $(ldd "$p" 2>/dev/null | grep -o '/[^ ]*'); do mkdir -p $D$(dirname $l); cp -L $l $D$l; done; done
echo 'rudolf:x:1000:1000::/home/rudolf:/bin/sh' > $D/etc/passwd; echo test > $D/etc/rudolf-debian; touch $D/.rudolf-ready
chown -R 200000:200000 $D; chown 1000:1000 $D/home/rudolf
# signed in: the account's Documents over /home/rudolf's (as rudolfd's home_bind)
A=/persist/accounts/uAbC12345; mkdir -p $A/Documents && echo "the account's note" > $A/Documents/NOTE.TXT
mount --bind $A/Documents /home/rudolf/Documents && echo uAbC12345 > /run/rudolf/home-session
R="$W/rudolf-debian"
o=$($R run -- sh -c 'cat /home/rudolf/Documents/NOTE.TXT' 2>&1)
ok "1. Debian started after sign-in sees the account's Documents" "$(echo "$o" | grep -c "the account's note")" "$o"
K=$(cat /run/rudolf/debian/init.pid 2>/dev/null)
n=$(grep -c " /home/rudolf/Documents " /proc/$K/mountinfo 2>/dev/null)
ok "2. once (rebind's), not also a copy of the sign-in's bind" "$([ "$n" = 1 ] && echo 1 || echo 0)" "$n mounts"
# signed out (as rudolfd's home_unbind)
umount -l /home/rudolf/Documents; rm -f /run/rudolf/home-session; $R rebind 2>&1
n=$(grep -c " /home/rudolf/Documents " /proc/$K/mountinfo 2>/dev/null)
ok "3. signed out: the account's Documents is gone from Debian (nothing keeps the folder in use)" "$([ "$n" = 0 ] && echo 1 || echo 0)" "$n mounts: $(grep ' /home/rudolf/Documents ' /proc/$K/mountinfo)"
o=$($R run -- sh -c 'ls /home/rudolf/Documents; echo DONE' 2>&1)
ok "4. Debian's /home/rudolf/Documents is the folder underneath again" "$(echo "$o" | grep -q NOTE.TXT && echo 0 || echo 1)" "$o"
# signed in again: rebind puts it back
mount --bind $A/Documents /home/rudolf/Documents && echo uAbC12345 > /run/rudolf/home-session; $R rebind 2>&1
o=$($R run -- sh -c 'cat /home/rudolf/Documents/NOTE.TXT' 2>&1)
ok "5. signed in again: the account's Documents is back in Debian" "$(echo "$o" | grep -c "the account's note")" "$o"
# Debian's other files in /home/rudolf are still there (the folder underneath)
echo kept > /persist/home/rudolf/.profile-test
o=$($R run -- sh -c 'cat /home/rudolf/.profile-test' 2>&1)
ok "6. the rest of /home/rudolf is shared as before" "$(echo "$o" | grep -c kept)" "$o"
kill $K 2>/dev/null
echo "$P/$((P+F)) passed"; [ $F = 0 ]
EOF
