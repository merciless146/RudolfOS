#!/bin/sh
# 1.5.6 (1.5.5's plan item 6): rudolfd's fs/search, File Explorer's "Everywhere" on a plugged-in drive: names in any
# case, at most 3 folder levels below the drive and 2,000 entries looked at, drives (/media) only, nothing outside.
# Compiled from rudolfd.c. Needs root (a private mount namespace with a stand-in /media).   sh fssearch_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$FSS_INNER" ] && exec env FSS_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/fss.XXXXXX)
mount -t tmpfs none /media
gcc -O1 -w -o $T/t $HERE/fixtures/fsapi.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
D=/media/STICK; mkdir -p $D/Photos/2026/May/deep $D/Docs
touch $D/Holiday.JPG $D/Photos/holiday-beach.jpg $D/Photos/2026/HOLIDAY.png $D/Photos/2026/May/holiday3.jpg $D/Photos/2026/May/deep/holiday4.jpg $D/Docs/notes.txt $D/.hidden-holiday
ln -s /etc/passwd $D/holiday-link
o=$($T/t "search?path=$D&q=holiday")
ok "1. names in any case, with their place on the drive" "$(echo "$o" | grep -q '"p":"Holiday.JPG"' && echo "$o" | grep -q '"p":"Photos/holiday-beach.jpg"' && echo "$o" | grep -q '"p":"Photos/2026/HOLIDAY.png"' && echo 1)" "$o"
ok "2. at most 3 folder levels below the drive (May is the 3rd: holiday3 found, deep/holiday4 not)" "$(echo "$o" | grep -q 'May/holiday3.jpg' && ! echo "$o" | grep -q 'holiday4' && echo 1)" "$o"
ok "3. hidden files and links are left out" "$(! echo "$o" | grep -q 'hidden-holiday' && ! echo "$o" | grep -q 'holiday-link' && echo 1)" "$o"
mkdir -p $D/many; i=0; while [ $i -lt 2100 ]; do : > $D/many/f$i; i=$((i+1)); done
o=$($T/t "search?path=$D&q=zzz")
ok "4. a large drive: it stops after 2,000 entries and says so" "$(echo "$o" | grep -q '"seen":2000,"truncated":true' && echo 1)" "$(echo "$o" | cut -c1-200)"
o=$($T/t "search?path=/home/rudolf/Documents&q=a" "search?path=/media/../etc&q=pass" "search?path=$D&q=")
ok "5. only drives: the shared folders, a path leaving /media and an empty search are refused" "$([ "$(echo "$o" | grep -c ' 40[03] ')" = 3 ] && echo 1)" "$o"
rm -rf $T; echo "$((N-F))/$N passed"; [ $F = 0 ]
