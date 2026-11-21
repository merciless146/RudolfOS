#!/bin/sh
# 1.6.1 (File Explorer: files dragged between windows and drives): rudolfd's requests for the transfers, compiled from
# rudolfd.c: fs/copy (a file, whole, its time kept, never over something, never into itself, only reachable paths),
# fs/move (a rename; another drive answers 409), fs/hash (SHA-256), fs/write in pieces, and dl/file to=copy (a file kept on
# the disk copied in the account's own folder only). Needs root (a private mount namespace).   sh fsxfer161_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$FXT_INNER" ] && exec env FXT_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/fsxfer.XXXXXX)
mount -t tmpfs none /run; mkdir -p /run/rudolf /media /home/rudolf
mount -t tmpfs none /media; mount -t tmpfs none /home/rudolf
mkdir -p /media/sdb1/Games /media/sdc1 /home/rudolf/Games/DC /home/rudolf/Downloads/.accounts/u-ann /home/rudolf/Downloads/.accounts/u-bob /tmp/fxt-outside
# a second file system: a move onto it is not a rename
mount -t tmpfs none /media/sdc1
gcc -O1 -w -o $T/t $HERE/fixtures/fsxfer.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
hs() { case "$1" in *"$2"*) return 0;; *) return 1;; esac; }
t() { $T/t "$@"; }
head -c 3000000 /dev/urandom > /media/sdb1/Games/disc.gdi; touch -d '2024-05-06 07:08:09' /media/sdb1/Games/disc.gdi
head -c 2000000 /dev/urandom > "/home/rudolf/Downloads/.accounts/u-ann/BIG.ISO"
S=$(sha256sum < /media/sdb1/Games/disc.gdi | cut -c1-64)
o=$(t "POST fs/copy?from=/media/sdb1/Games/disc.gdi&to=/home/rudolf/Games/DC/disc.gdi")
ok "1. a file copied onto another place, whole, its time kept" "$(hs "$o" '"size":3000000' && cmp -s /media/sdb1/Games/disc.gdi /home/rudolf/Games/DC/disc.gdi && [ "$(stat -c %Y /home/rudolf/Games/DC/disc.gdi)" = "$(stat -c %Y /media/sdb1/Games/disc.gdi)" ] && echo 1)" "$o"
ok "2. ...no temporary file left; in the shared folders owned by the desktop user" "$([ -z "$(ls -a /home/rudolf/Games/DC | grep rudolf-tmp)" ] && [ "$(stat -c %u /home/rudolf/Games/DC/disc.gdi)" = 1000 ] && echo 1)" "$(ls -la /home/rudolf/Games/DC)"
o=$(t "GET fs/hash?path=/home/rudolf/Games/DC/disc.gdi")
ok "3. fs/hash: its SHA-256, the same as the original's" "$(has "$o" "\"sha256\":\"$S\"")" "$o $S"
o=$(t "POST fs/copy?from=/media/sdb1/Games/disc.gdi&to=/home/rudolf/Games/DC/disc.gdi")
ok "4. never over something that is there (409)" "$(has "$o" ' 409 ')" "$o"
o=$(t "POST fs/copy?from=/etc/hostname&to=/media/sdb1/h" "POST fs/copy?from=/media/sdb1/Games/disc.gdi&to=/tmp/fxt-outside/x" "POST fs/move?from=/media/sdb1/../../etc/hostname&to=/media/sdb1/h" "POST fs/move?from=/media/sdb1/Games&to=/tmp/fxt-outside/G" "POST fs/copy?from=/home/rudolf/Downloads/.accounts/u-ann/BIG.ISO&to=/media/sdb1/x" "GET fs/hash?path=/home/rudolf/Downloads/.accounts/u-ann/BIG.ISO")
ok "5. only /media and the shared folders, from and to, no .., nothing hidden (Downloads/.accounts)" "$([ "$(echo "$o" | grep -c ' 403 ')" = 6 ] && [ ! -e /media/sdb1/h ] && [ ! -e /tmp/fxt-outside/x ] && echo 1)" "$o"
o=$(t "POST fs/move?from=/media/sdb1/Games&to=/media/sdb1/Games/inside" "POST fs/copy?from=/media/sdb1/Games&to=/media/sdb1/G2")
ok "6. a folder never into itself; fs/copy takes a file only" "$(echo "$o" | sed -n 1p | grep -q ' 400 ' && echo "$o" | sed -n 2p | grep -q ' 400 ' && echo 1)" "$o"
mkdir -p /media/sdb1/Saves; echo s1 > /media/sdb1/Saves/a.sav
o=$(t "POST fs/move?from=/media/sdb1/Saves&to=/media/sdb1/Games/Saves")
ok "7. fs/move on the same drive: a rename (a folder with what is in it)" "$(hs "$o" '"moved"' && [ -f /media/sdb1/Games/Saves/a.sav ] && [ ! -e /media/sdb1/Saves ] && echo 1)" "$o"
o=$(t "POST fs/move?from=/media/sdb1/Games/Saves&to=/media/sdc1/Saves")
ok "8. fs/move onto another drive: 409, nothing moved (the desktop copies, checks, deletes)" "$(hs "$o" ' 409 ' && [ -f /media/sdb1/Games/Saves/a.sav ] && [ ! -e /media/sdc1/Saves ] && echo 1)" "$o"
ln -s /etc/hostname /media/sdb1/link
o=$(t "POST fs/copy?from=/media/sdb1/link&to=/media/sdc1/l" "GET fs/hash?path=/media/sdb1/link")
ok "9. a link is not followed (copy, hash)" "$([ "$(echo "$o" | grep -c ' 404 ')" = 2 ] && [ ! -e /media/sdc1/l ] && echo 1)" "$o"
head -c 1000 /dev/urandom > $T/p1; head -c 500 /dev/urandom > $T/p2; cat $T/p1 $T/p2 > $T/p12
o=$(t "POST fs/write?path=/media/sdc1/big.bin&part=0 <$T/p1" "POST fs/write?path=/media/sdc1/big.bin&part=1000&final=1&mtime=1700000000000 <$T/p2")
ok "10. fs/write in pieces: whole, put in place on the last piece, its time set" "$(cmp -s $T/p12 /media/sdc1/big.bin && [ ! -e /media/sdc1/big.bin.rudolf-tmp ] && [ "$(stat -c %Y /media/sdc1/big.bin)" = 1700000000 ] && hs "$o" '"size":1500' && echo 1)" "$o"
o=$(t "POST fs/write?path=/media/sdc1/b2.bin&part=0 <$T/p1" "POST fs/write?path=/media/sdc1/b2.bin&part=1500 <$T/p2")
ok "11. ...a piece where none ended is refused (409), and the file is not there" "$(echo "$o" | sed -n 2p | grep -q ' 409 ' && [ ! -e /media/sdc1/b2.bin ] && echo 1)" "$o"
o=$(t "POST fs/write?path=/media/sdc1/b3.bin <$T/p12")
ok "12. a whole write as before" "$(cmp -s $T/p12 /media/sdc1/b3.bin && hs "$o" '"size":1500' && echo 1)" "$o"
# dl/file to=copy: a large download kept on the disk, copied as a file of its own, in the account's own folder
o=$(t "POST dl/file?name=BIG.ISO&from=home&to=copy&as=BIG%20-%20COPY.ISO&acct=u-ann")
ok "13. dl/file to=copy: a file of its own beside it, named as asked" "$(hs "$o" '"path":"/home/rudolf/Downloads/.accounts/u-ann/BIG - COPY.ISO"' && cmp -s /home/rudolf/Downloads/.accounts/u-ann/BIG.ISO "/home/rudolf/Downloads/.accounts/u-ann/BIG - COPY.ISO" && echo 1)" "$o"
o=$(t "POST dl/file?name=BIG.ISO&from=home&to=copy&as=BIG%20-%20COPY.ISO&acct=u-ann")
ok "14. ...again: the next free name, the first copy kept" "$(has "$o" 'BIG - COPY (2).ISO')" "$o"
o=$(t "POST dl/file?name=BIG.ISO&from=home&to=copy&acct=u-bob" "POST dl/file?name=BIG.ISO&from=root&to=copy&acct=u-ann" "POST dl/file?name=BIG.ISO&from=home&to=copy&as=../u-bob/X&acct=u-ann" "POST dl/file?name=BIG.ISO&to=copy&acct=u-ann")
ok "15. ...only the account's own files, within its folder (another account's, Downloads itself, ../ refused)" "$([ "$(echo "$o" | grep -c ' 40[04] ')" = 4 ] && [ -z "$(ls /home/rudolf/Downloads/.accounts/u-bob)" ] && echo 1)" "$o"
rm -rf $T /tmp/fxt-outside; echo "$((N-F))/$N passed"; [ $F = 0 ]
