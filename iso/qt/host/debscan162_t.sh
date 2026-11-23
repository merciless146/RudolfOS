#!/bin/sh
# 1.6.2 (docs/plan-1.6.2.md Part 34, step 4): Linux apps (Simple Scan in the Debian sandbox) and the scanners Rudolf OS
# found. 1-2: rudolf-scan conf (rudolf-scanconf.h, as rudolfd writes /persist/etc/rudolf/scanners.conf after Find
# scanners): the network scanners as sane-airscan's settings, discovery off, no USB scanner, names and addresses kept to
# safe characters. 3-6: rudolf-debian (as debhome_t.sh: a mount namespace of its own, /persist, /home and /run/rudolf as
# empty folders, a stand-in Debian of this computer's sh and cat): Debian's /etc/sane.d/airscan.conf is that list, read
# only, and follows it when rudolfd rewrites it in place; a Debian without SANE gets nothing; no list, nothing.
#   sh debscan162_t.sh      (root for 3-6)
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 1200)"; F=$((F+1)); fi; }
gcc -O2 -Wno-format-truncation -o $W/rudolf-scan $SYS/rudolf-scan.c -ldl -lm -lz 2>$W/cc || { echo "FAIL: rudolf-scan does not build: $(cat $W/cc)"; exit 1; }
J='{"ok":true,"scanners":[{"id":"escl:http://192.168.1.30:80/eSCL","name":"Office \"Scanner\"\u0001 [3rd]","model":"X","via":"network","proto":"eSCL"},{"id":"wsd:http://192.168.1.31:5357/wsd/scan","name":"Hall (WSD)","model":"Y","via":"network","proto":"WSD"},{"id":"escl:http://127.0.0.1:60001/eSCL","name":"USB: Canon","model":"Z","via":"usb","proto":"eSCL"},{"id":"sane:usb:04A9:190F","name":"USB: LiDE","model":"L","via":"usb","proto":"driver"},{"id":"escl:http://x/\";rm","name":"Evil\nName","model":"","via":"network","proto":"eSCL"}]}'
o=$(printf '%s' "$J" | $W/rudolf-scan conf)
ok "1. the network scanners as sane-airscan's settings (eSCL and WSD, by their addresses), its discovery off" "$(echo "$o" | grep -qx 'discovery = disable' && echo "$o" | grep -qx '"Office Scanner 3rd" = http://192.168.1.30:80/eSCL, escl' && echo "$o" | grep -qx '"Hall (WSD)" = http://192.168.1.31:5357/wsd/scan, wsd' && echo 1)" "$o"
ok "2. ...no USB scanner (Linux apps get no USB devices); names and addresses kept to safe characters (no quote, semicolon or new line from the network)" "$(! echo "$o" | grep -q '127.0.0.1\|04A9\|USB:' && ! echo "$o" | grep '^"' | grep -q ';' && [ "$(echo "$o" | grep -c '^"')" -le 3 ] && [ -z "$(echo "$o" | grep '^"' | grep -v '^"[A-Za-z0-9 ._()-]*" = https\{0,1\}://[A-Za-z0-9:/._~%-]*, \(escl\|wsd\)$')" ] && echo 1)" "$o"
[ "$(id -u)" = 0 ] || { echo "SKIP 3-6: as root"; echo "$((N-F))/$N passed"; [ $F = 0 ]; exit $?; }
gcc -O2 -w -o "$W/rudolf-debian" "$SYS/rudolf-debian.c" -lcrypto -ldl 2>"$W/dcc" || { cat "$W/dcc"; exit 1; }
printf '%s' "$J" | $W/rudolf-scan conf > $W/scanners.conf
unshare -m --propagation private sh -s "$W" <<'EOF' > $W/inner.txt 2>&1
W=$1
mkdir -p /persist /home /run/rudolf 2>/dev/null
mount -t tmpfs t /persist && mount -t tmpfs t /home && mount -t tmpfs t /run/rudolf || { echo "cannot make the folders"; exit 1; }
mkdir -p /persist/home/rudolf /home/rudolf && mount --bind /persist/home/rudolf /home/rudolf
D=/persist/debian; mkdir -p $D/bin $D/etc/sane.d $D/proc $D/sys $D/dev $D/tmp $D/run $D/home/rudolf $D/root
for b in sh cat; do p=$(command -v $b); cp -L "$p" $D/bin/$b; for l in $(ldd "$p" 2>/dev/null | grep -o '/[^ ]*'); do mkdir -p $D$(dirname $l); cp -L $l $D$l; done; done
echo 'rudolf:x:1000:1000::/home/rudolf:/bin/sh' > $D/etc/passwd; echo test > $D/etc/rudolf-debian; touch $D/.rudolf-ready
echo '# Debian own' > $D/etc/sane.d/airscan.conf
chown -R 200000:200000 $D; chown 1000:1000 $D/home/rudolf
R="$W/rudolf-debian"
echo "== 3"; $R run -- sh -c 'cat /etc/sane.d/airscan.conf' 2>&1
mkdir -p /persist/etc/rudolf; cp $W/scanners.conf /persist/etc/rudolf/scanners.conf
$R stop >/dev/null 2>&1
echo "== 4"; $R run -- sh -c 'cat /etc/sane.d/airscan.conf; echo x >> /etc/sane.d/airscan.conf && echo WROTE' 2>&1
echo '"Later Scanner" = http://192.168.1.40:80/eSCL, escl' > /tmp/s.$$; cat /tmp/s.$$ > /persist/etc/rudolf/scanners.conf; rm -f /tmp/s.$$
echo "== 5"; $R run -- sh -c 'cat /etc/sane.d/airscan.conf' 2>&1
$R stop >/dev/null 2>&1
rm -rf $D/etc/sane.d
echo "== 6"; $R run -- sh -c 'cat /etc/sane.d/airscan.conf 2>&1; echo END' 2>&1
$R stop >/dev/null 2>&1
EOF
sec() { sed -n "/^== $1\$/,/^== /p" $W/inner.txt | sed '1d;/^== /d'; }
ok "3. no list yet (Find scanners not run): Debian keeps its own settings" "$(sec 3 | grep -qx '# Debian own' && echo 1)" "$(cat $W/inner.txt)"
ok "4. Debian with SANE: its /etc/sane.d/airscan.conf is Rudolf OS's list, read only" "$(sec 4 | grep -q '"Hall (WSD)" = http://192.168.1.31:5357/wsd/scan, wsd' && ! sec 4 | grep -q '^WROTE' && echo 1)" "$(sec 4)"
ok "5. rudolfd rewriting the list in place: Debian sees the new one" "$(sec 5 | grep -q 'Later Scanner' && echo 1)" "$(sec 5)"
ok "6. a Debian without SANE: nothing made in it" "$(sec 6 | grep -q 'No such file' && echo 1)" "$(sec 6)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
