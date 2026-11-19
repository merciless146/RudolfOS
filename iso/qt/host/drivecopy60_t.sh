#!/bin/sh
# 1.6.0 (2026-11-19): Copy to another drive (drivecopy.sh): a Windows drive's personal files onto a small "stick" (a
# 3 MB tmpfs): structure kept, links not followed, Windows/programs/AppData/.exe left out with "personal", the copy
# stopping when the stick is full and continuing onto a second one, a cut-off copy going on where it stopped.
# Needs root (mount).   sh drivecopy60_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); R=$(cd "$HERE/../../.." && pwd); DC=$R/iso/sys/root/usr/lib/rudolf/drivecopy.sh
T=$(mktemp -d); P=0; F=0; trap 'umount $T/s1 $T/s2 2>/dev/null; rm -rf "$T"' EXIT
ok() { if [ "$2" = 1 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 ($3)"; fi; }
export RUDOLF_DRIVECOPY_STATE=$T/state
W=$T/sda2; mkdir -p "$W/Windows/System32" "$W/Program Files/App" "$W/Users/Grandpa/Documents/Taxes 2019" "$W/Users/Grandpa/Pictures/Trip" "$W/Users/Grandpa/AppData/Local/Temp" "$W/Users/Grandpa/Videos" "$W/\$Recycle.Bin"
echo k > "$W/Windows/System32/kernel32.dll"; echo a > "$W/Program Files/App/app.exe"; echo t > "$W/Users/Grandpa/AppData/Local/Temp/x.txt"
echo "letter" > "$W/Users/Grandpa/Documents/Letter to Mary.docx"; echo "taxes" > "$W/Users/Grandpa/Documents/Taxes 2019/return.pdf"
echo i > "$W/Users/Grandpa/Documents/setup.exe"; echo d > "$W/Users/Grandpa/Pictures/desktop.ini"
for i in 1 2 3 4 5 6; do head -c 400000 /dev/urandom > "$W/Users/Grandpa/Pictures/Trip/IMG_000$i.JPG"; done
head -c 1200000 /dev/urandom > "$W/Users/Grandpa/Videos/birthday.mp4"
ln -s "$W/Users/Grandpa" "$W/Users/Grandpa/Documents/My Music"   # (a junction: would loop)
touch -d '2019-01-26 10:00' "$W/Users/Grandpa/Documents/Letter to Mary.docx"
mkdir $T/s1 $T/s2 && mount -t tmpfs -o size=3m tmpfs $T/s1 && mount -t tmpfs -o size=8m tmpfs $T/s2 || { echo "FAIL needs root to mount"; exit 1; }
L() { printf 'personal=%s continue=%s\n%s\n' "$1" "$2" "$3" > $T/list; shift 3; for s in "$@"; do echo "$s" >> $T/list; done; }
# 1. the whole drive, personal files only, onto a 3 MB stick: fills up
L 1 0 "$T/s1/Grandpa's files" "$W"; sh $DC $T/list > $T/o1 2>&1; r1=$?
D=$T/s1/"Grandpa's files"/sda2/Users/Grandpa
ok "1. the copy keeps the folders (Documents/Taxes 2019/return.pdf)" "$( [ -f "$D/Documents/Taxes 2019/return.pdf" ] && echo 1)" "$(find $T/s1 | head)"
ok "2. personal: no Windows, Program Files, AppData, recycle bin, .exe, desktop.ini" "$( [ ! -e $T/s1/*/sda2/Windows ] && [ ! -e "$T/s1/Grandpa's files/sda2/Program Files" ] && [ ! -e "$D/AppData" ] && [ ! -e "$D/Documents/setup.exe" ] && [ ! -e "$D/Pictures/desktop.ini" ] && echo 1)" "$(find $T/s1 -type f | sed "s#$T/s1/##" | tr '\n' ' ')"
ok "3. a link (Windows junction) not followed" "$( [ ! -e "$D/Documents/My Music" ] && echo 1)" ""
ok "4. the file's date kept" "$( [ "$(stat -c %Y "$D/Documents/Letter to Mary.docx")" = "$(stat -c %Y "$W/Users/Grandpa/Documents/Letter to Mary.docx")" ] && echo 1)" ""
ok "5. the stick full: stops (28), says so, no half-written file" "$( [ $r1 = 28 ] && grep -q "The drive is full" $T/o1 && [ -z "$(find $T/s1 -name '*.part')" ] && echo 1)" "rc $r1 $(tail -3 $T/o1)"
ok "6. progress lines for the page" "$(grep -q '^PROGRESS [0-9]* 9 ' $T/o1 && echo 1)" "$(grep PROGRESS $T/o1 | tail -2)"
# 2. continue onto a second stick: only the rest
L 1 1 "$T/s2/Grandpa's files" "$W"; sh $DC $T/list > $T/o2 2>&1; r2=$?
n1=$(find $T/s1 -type f | wc -l); n2=$(find $T/s2 -type f | wc -l)
ok "7. continued on a second stick: the rest, nothing twice (first $n1, second $n2, of 9)" "$( [ $r2 = 0 ] && [ $((n1 + n2)) = 9 ] && [ $n2 -gt 0 ] && grep -q Continuing $T/o2 && echo 1)" "rc $r2 $(tail -3 $T/o2)"
# 3. again onto the second stick without continue: the files already there (same size) are kept, nothing copied twice
L 1 0 "$T/s2/Grandpa's files" "$W"; touch $T/mark; sleep 1; sh $DC $T/list > $T/o3 2>&1; r3=$?
ch=$(find $T/s2 -type f -newer $T/mark | wc -l)
ok "8. a copy run again goes on where it stopped (already there: kept)" "$( [ $r3 = 0 ] && grep -q "^Done: 9 files" $T/o3 && echo 1)" "rc $r3 changed $ch $(tail -2 $T/o3)"
# 4. everything (not personal), one folder
umount $T/s2; mount -t tmpfs -o size=8m tmpfs $T/s2
L 0 0 "$T/s2/copy" "$W/Program Files"; sh $DC $T/list > $T/o4 2>&1
ok "9. not personal: a program's folder copied as it is" "$( [ -f "$T/s2/copy/Program Files/App/app.exe" ] && echo 1)" "$(tail -2 $T/o4)"
ok "10. the source unchanged" "$( [ -f "$W/Users/Grandpa/Documents/setup.exe" ] && [ -z "$(find $W -name '*.part')" ] && echo 1)" ""
echo "$P passed, $F failed"; [ $F = 0 ]
