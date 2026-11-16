#!/bin/sh
# 1.6.0: rudolf-fetch in the Terminal (Debian): Rudolf's logo and this computer's details, read only. rudolf-debian
# binds Rudolf's own files (/usr/share/rudolf: the logo, VERSION) read-only into Debian as /usr/share/rudolf-os, the
# command as /usr/local/bin/rudolf-fetch, and writes the facts Debian cannot see (the desktop engine, the update
# channel) into Debian's own /run. Nothing else in Debian changes.
#   sh debfetch60_t.sh   (root; builds rudolf-debian; runs in a mount namespace of its own, with /persist, /home,
#                         /run/rudolf, /usr/lib/rudolf and /usr/share/rudolf as stand-ins, and a stand-in Debian made of
#                         this computer's sh and the tools rudolf-fetch uses)
set -u
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys
[ "$(id -u)" = 0 ] || { echo "run as root"; exit 2; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
gcc -O2 -w -o "$W/rudolf-debian" "$SYS/rudolf-debian.c" -lcrypto -ldl 2>"$W/cc.txt" || { cat "$W/cc.txt"; exit 1; }
cp "$SYS/root/usr/lib/rudolf/rudolf-fetch" "$SYS/root/usr/share/rudolf/logo.txt" "$SYS/root/usr/share/rudolf/logo.ansi" "$W/"
unshare -m --propagation private sh -s "$W" <<'EOS'
W=$1; P=0; F=0
ok() { if [ "$2" = 1 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
mkdir -p /persist /home /run/rudolf /usr/lib/rudolf /usr/share/rudolf 2>/dev/null
for d in /persist /home /run/rudolf /usr/lib/rudolf /usr/share/rudolf; do mount -t tmpfs t $d || { echo "cannot make $d"; exit 1; }; done
cp $W/rudolf-fetch /usr/lib/rudolf/; chmod 755 /usr/lib/rudolf/rudolf-fetch; cp $W/logo.txt $W/logo.ansi /usr/share/rudolf/; echo 1.6.0.261115 > /usr/share/rudolf/VERSION
echo native > /run/rudolf/engine-choice; mkdir -p /persist/os; printf 'url=https://merciless146.github.io/RudolfOS/beta\nauto=1\n' > /persist/os/update.conf
mkdir -p /persist/home/rudolf /home/rudolf && mount --bind /persist/home/rudolf /home/rudolf
D=/persist/debian; mkdir -p $D/bin $D/etc $D/proc $D/sys $D/dev $D/tmp $D/run $D/home/rudolf $D/root $D/usr/local/bin
for b in sh cat sed awk head wc df uname id hostname ls; do p=$(command -v $b) || continue; cp -L "$p" $D/bin/$b; for l in $(ldd "$p" 2>/dev/null | grep -o '/[^ ]*'); do mkdir -p $D$(dirname $l); cp -L $l $D$l; done; done
echo 'rudolf:x:1000:1000::/home/rudolf:/bin/sh' > $D/etc/passwd; echo 'rudolf:x:1000:' > $D/etc/group; echo test > $D/etc/rudolf-debian; touch $D/.rudolf-ready
ls -A $D/usr/local/bin > $W/before
chown -R 200000:200000 $D; chown 1000:1000 $D/home/rudolf
R="$W/rudolf-debian"; RUN() { PATH=/usr/local/bin:/bin $R run -- sh -c "PATH=/usr/local/bin:/usr/bin:/bin; $1" 2>&1; }
o=$(RUN 'rudolf-fetch --plain')
ok "1. rudolf-fetch runs in the Terminal: the logo and Rudolf OS's version" "$(echo "$o" | grep -q 'OS: Rudolf OS 1.6.0.261115' && echo "$o" | grep -q '\]@ \] @@ \[ @\[' && echo 1)" "$o"
ok "2. ...the facts Debian cannot see: the desktop engine (Native) and the update channel (beta)" "$(echo "$o" | grep -q 'Desktop: Native' && echo "$o" | grep -q 'Updates: beta' && echo 1)" "$o"
ok "3. ...and its own details: the kernel, the memory and the uptime" "$(echo "$o" | grep -q "Kernel: $(uname -r)" && echo "$o" | grep -q 'Memory: .* GiB / .* GiB' && echo "$o" | grep -q 'Uptime: ' && echo 1)" "$o"
o=$(RUN 'rudolf-fetch' | head -16 | tail -1)
ok "4. --plain or not a terminal: no colour codes (the nose is red only in a terminal)" "$(printf '%s' "$o" | grep -q "$(printf '\033')" && echo 0 || echo 1)" "$o"
o=$(RUN 'echo x > /usr/local/bin/rudolf-fetch; echo x > /usr/share/rudolf-os/VERSION; cat /usr/share/rudolf-os/VERSION')
ok "5. read only: Debian's programs cannot change the command or Rudolf's files" "$(echo "$o" | grep -q '1.6.0.261115' && echo "$o" | grep -q -v '^x$' && [ "$(cat /usr/share/rudolf/VERSION)" = 1.6.0.261115 ] && cmp -s $W/rudolf-fetch /usr/lib/rudolf/rudolf-fetch && echo 1)" "$o"
o=$(RUN 'ls /usr/local/bin; ls /run/rudolf-os')
ok "6. nothing else in Debian: /usr/local/bin holds only rudolf-fetch besides what was there" "$(echo "$o" | head -1 | grep -qx rudolf-fetch && [ "$(echo "$o" | sed -n 1p)" = rudolf-fetch ] && echo 1)" "$o $(cat $W/before)"
$R stop >/dev/null 2>&1
echo "$P/$((P+F)) passed"; [ $F = 0 ]
EOS
