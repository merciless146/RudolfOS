#!/bin/sh
# 1.5.9 (plan 15.3, second step): Debian (and Wine in it) as the signed-in account's own Linux user. While an account
# is signed in with its files on the disk, Debian's "rudolf" (1000) is the account's user (acct1001 here) on Rudolf
# OS, its home is the account's folder, its runtime folder has the account's session bus and the accounts' sound
# socket; signing out stops Debian, and the next command starts it as the desktop's user again.
#   sh debacct59_t.sh      (root; builds rudolf-debian; runs in a mount namespace of its own, with /persist, /home and
#                           /run as empty folders, and a stand-in Debian made of this computer's programs)
set -u
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys
[ "$(id -u)" = 0 ] || { echo "SKIP: run as root"; exit 0; }
W=$(mktemp -d); chmod 755 "$W"; trap 'rm -rf "$W"' EXIT
gcc -O2 -w -o "$W/rudolf-debian" "$SYS/rudolf-debian.c" -lcrypto 2>"$W/cc.txt" || { cat "$W/cc.txt"; exit 1; }
unshare -m --propagation private sh -s "$W" <<'EOF'
W=$1; P=0; F=0
ok() { if [ "$2" = 1 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
mkdir -p /persist /home /run 2>/dev/null
mount -t tmpfs t /persist && mount -t tmpfs t /home && mount -t tmpfs t /run && mkdir -p /run/rudolf || { echo "cannot make the folders"; exit 1; }
# as rudolf-init: /home/rudolf is /persist/home/rudolf, the desktop's (with a file of its own no account may read)
mkdir -p /persist/home/rudolf /home/rudolf && mount --bind /persist/home/rudolf /home/rudolf
echo "the desktop's own" > /persist/home/rudolf/.desk-secret; chmod 600 /persist/home/rudolf/.desk-secret; chown -R 1000:1000 /persist/home/rudolf
# a stand-in Debian (Debian's own root is 200000, as rudolf-debian unpacks it)
D=/persist/debian; mkdir -p $D/bin $D/etc/skel $D/proc $D/sys $D/dev $D/tmp $D/run $D/home/rudolf $D/root
for b in sh ls cat id touch sleep env cp; do p=$(command -v $b); cp -L "$p" $D/bin/$b; for l in $(ldd "$p" 2>/dev/null | grep -o '/[^ ]*'); do mkdir -p $D$(dirname $l); cp -L $l $D$l; done; done
echo 'rudolf:x:1000:1000::/home/rudolf:/bin/sh' > $D/etc/passwd; echo test > $D/etc/rudolf-debian; touch $D/.rudolf-ready
echo '# skel' > $D/etc/skel/.profile
chown -R 200000:200000 $D; chown 1000:1000 $D/home/rudolf
# an account signed in, as rudolfd's au_session_start makes it: its folder at /home/acct1001 (its user's, 0700), its
# runtime folder with its session bus, the accounts' sound socket, and the session's environment
A=/persist/accounts/uAnnA1b2; mkdir -p $A/Documents && echo "Ann's note" > $A/Documents/NOTE.TXT; chown -R 1001:1001 $A; chmod 700 $A
mkdir -p /home/acct1001 && mount --bind $A /home/acct1001
mkdir -p /run/user/1001 /run/rudolf/pulse; chown 1001:1001 /run/user/1001; chmod 700 /run/user/1001; chown 1000:29 /run/rudolf/pulse; chmod 750 /run/rudolf/pulse
python3 -c "import socket,os,time,sys
a=socket.socket(socket.AF_UNIX); a.bind('/run/user/1001/bus'); a.listen(); os.chown('/run/user/1001/bus',1001,1001)
b=socket.socket(socket.AF_UNIX); b.bind('/run/rudolf/pulse/native'); b.listen(); os.chmod('/run/rudolf/pulse/native',0o777)
sys.stdout.close(); time.sleep(120)" & SOCKS=$!
for i in 1 2 3 4 5 6 7 8 9 10; do [ -S /run/rudolf/pulse/native ] && break; timeout 0.2 tail -f /dev/null; done
signin() { printf 'DISPLAY=:0\nHOME=/home/acct1001\nUSER=acct1001\nLOGNAME=acct1001\nXDG_RUNTIME_DIR=/run/user/1001\n' > /run/rudolf/acct-session.env; echo uAnnA1b2 > /run/rudolf/home-session; }
signin
R="$W/rudolf-debian"
o=$($R run -- sh -c 'id -u; cat /home/rudolf/Documents/NOTE.TXT; touch /home/rudolf/made-by-debian' 2>&1)
ok "1. signed in: Debian's rudolf (1000 inside), its home the account's folder" "$(echo "$o" | head -1 | grep -qx 1000 && echo "$o" | grep -q "Ann's note" && echo 1)" "$o"
ok "...a file it makes is the account's user's on Rudolf OS (1001), in the account's folder" "$([ "$(stat -c %u $A/made-by-debian 2>/dev/null)" = 1001 ] && echo 1)" "$(ls -ln $A)"
$R run --bg -- sleep 30; timeout 0.5 tail -f /dev/null
u=$(ps -o uid= -C sleep | tr -d ' ' | sort -u | tr '\n' ' ')
ok "...its programs run as the account's user (acct1001), not the desktop's" "$([ "$u" = '1001 ' ] && echo 1)" "uids: $u"
o=$($R run -- sh -c 'cat /home/rudolf/.desk-secret; ls -a /home/rudolf' 2>&1)
ok "2. the desktop's own files are not Debian's home any more (none of them in it)" "$(echo "$o" | grep -q "the desktop's own" && echo 0 || echo 1)" "$o"
o=$($R run -- sh -c 'cat /home/rudolf/.profile' 2>&1)
ok "...its shell settings put in the account's folder when it has none" "$(echo "$o" | grep -q '# skel' && [ "$(stat -c %u $A/.profile)" = 1001 ] && echo 1)" "$o"
o=$($R run -- sh -c '[ -S /run/user/1000/bus ] && echo BUS; [ -S /run/user/1000/pulse/native ] && echo SOUND; env' 2>&1)
ok "3. its runtime folder: the account's session bus and the sound socket, the environment pointing at them" "$(echo "$o" | grep -q BUS && echo "$o" | grep -q SOUND && echo "$o" | grep -q 'PULSE_SERVER=unix:/run/user/1000/pulse/native' && echo "$o" | grep -q 'DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus' && echo 1)" "$o"
K=$(cat /run/rudolf/debian/init.pid 2>/dev/null)
ok "...Debian was started for that user (kept beside its process id)" "$([ "$(cat /run/rudolf/debian/init.uid 2>/dev/null)" = 1001 ] && echo 1)" "$(cat /run/rudolf/debian/init.uid 2>&1)"
o=$($R apps 2>&1); mkdir -p $A/.local/share/applications; printf '[Desktop Entry]\nType=Application\nName=Ann Tool\nExec=sh\n' > $A/.local/share/applications/anntool.desktop
o=$($R apps 2>&1)
ok "4. the launchers Wine makes in the home folder are the account's (read from its folder)" "$(echo "$o" | grep -q '"name":"Ann Tool"' && echo 1)" "$o"
# signed out, as rudolfd's home_unbind: the session's environment gone, then rebind
pkill -u 1001 sleep 2>/dev/null; rm -f /run/rudolf/acct-session.env /run/rudolf/home-session; $R rebind 2>&1
ok "5. signed out: Debian stops (nothing of it keeps the account's folder in use)" "$([ ! -e /run/rudolf/debian/init.pid ] && ! kill -0 $K 2>/dev/null && echo 1)" "$(ls /run/rudolf/debian)"
o=$($R run -- sh -c 'id -u; cat /home/rudolf/.desk-secret; ls /home/rudolf; touch /home/rudolf/made2' 2>&1)
ok "...the next command starts it as the desktop's user, with the desktop's home, as before" "$(echo "$o" | grep -q "the desktop's own" && ! echo "$o" | grep -q NOTE && [ "$(stat -c %u /persist/home/rudolf/made2)" = 1000 ] && [ "$(cat /run/rudolf/debian/init.uid)" = 1000 ] && echo 1)" "$o"
o=$($R apps 2>&1); ok "...and the account's launchers are not listed" "$(echo "$o" | grep -q 'Ann Tool' && echo 0 || echo 1)" "$o"
K=$(cat /run/rudolf/debian/init.pid 2>/dev/null)
signin; $R rebind 2>&1
o=$($R run -- sh -c 'cat /home/rudolf/Documents/NOTE.TXT' 2>&1)
ok "6. signed in again: the desktop's Debian stopped, the account's started" "$(! kill -0 $K 2>/dev/null && echo "$o" | grep -q "Ann's note" && [ "$(cat /run/rudolf/debian/init.uid)" = 1001 ] && echo 1)" "$o"
# an environment that does not hold up (its home is not its user's): the desktop's user, never someone else's uid
$R stop; sed -i 's#^HOME=.*#HOME=/home/rudolf#' /run/rudolf/acct-session.env
o=$($R run -- sh -c 'touch /home/rudolf/made3' 2>&1)
ok "7. an account's environment that does not match its user is not taken" "$([ "$(stat -c %u /persist/home/rudolf/made3 2>/dev/null)" = 1000 ] && [ "$(cat /run/rudolf/debian/init.uid)" = 1000 ] && echo 1)" "$o $(ls -ln /persist/home/rudolf)"
$R stop; kill $SOCKS 2>/dev/null
echo "$P/$((P+F)) passed"; [ $F = 0 ]
EOF
