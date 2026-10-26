#!/bin/sh
# Join a downloaded Rudolf OS disc image's parts (1.2.7). GPL-2.0-or-later.
#   joinparts.sh /home/rudolf/Downloads/rudolf-os-X.Y.Z.iso
# The disc images on GitHub come in parts (NAME.iso.part00, part01, ...: GitHub's file size limit). This joins them in
# order into NAME.iso, as JOIN-PARTS.TXT and join.ps1 do. RUDOLF_IMG_SHA (64 hex digits, from NAME.iso.sha256, which the
# page reads because a file that small stays in the browser's storage) is compared when given; without it the image's
# signed release list is still checked by install.sh before anything is written.
export PATH=/usr/sbin:/usr/bin:/sbin:/bin
IMG=$1; D=${IMG%/*}; B=${IMG##*/}
case "$IMG" in /home/rudolf/*.iso|/home/rudolf/*.ISO) ;; *) echo "Not a disc image in the home folder: $IMG"; exit 2;; esac
case "$IMG" in *..*) echo "Bad file name."; exit 2;; esac
[ -e "$IMG" ] && { echo "$B is already there."; exit 0; }
# the parts, in order, with no gaps (names compared without case: NAME.iso.partNN)
LIST=$(ls "$D" 2>/dev/null | awk -v b="$B" 'BEGIN{b=tolower(b)} {n=tolower($0); if (substr(n,1,length(b)+5)==b".part" && substr(n,length(b)+6) ~ /^[0-9][0-9]$/) print substr(n,length(b)+6) " " $0}' | sort -n)
[ -n "$LIST" ] || { echo "No parts of $B were found in Downloads."; exit 3; }
N=0; FIRST=0; TOTAL=0
echo "$LIST" > /tmp/joinparts.$$
while read -r num name; do
	want=$(printf '%02d' $N)
	[ "$num" = "$want" ] || { echo "Part $want of $B is missing: download it again (the parts are $B.part00, part01, ...)."; rm -f /tmp/joinparts.$$; exit 4; }
	sz=$(stat -c %s "$D/$name")
	[ $N = 0 ] && FIRST=$sz
	TOTAL=$((TOTAL + sz)); N=$((N + 1))
done < /tmp/joinparts.$$
# every part but the last has the first one's size: a shorter one did not finish downloading
LAST=$(printf '%02d' $((N - 1)))
while read -r num name; do
	[ "$num" = "$LAST" ] && break
	[ "$(stat -c %s "$D/$name")" = "$FIRST" ] || { echo "Part $num ($name) is shorter than the others: download it again."; rm -f /tmp/joinparts.$$; exit 5; }
done < /tmp/joinparts.$$
FREE=$(df -k "$D" | awk 'NR==2 {print $4}')
[ $((TOTAL / 1024 + 65536)) -lt "$FREE" ] || { echo "Not enough room in Downloads to join the parts: $((TOTAL / 1048576)) MiB are needed."; rm -f /tmp/joinparts.$$; exit 6; }
echo "Joining $N parts of $B ($((TOTAL / 1048576)) MiB) ..."
rm -f "$IMG.joining"
while read -r num name; do cat "$D/$name" >> "$IMG.joining" || { echo "Part $num could not be read."; rm -f "$IMG.joining" /tmp/joinparts.$$; exit 7; }; done < /tmp/joinparts.$$
rm -f /tmp/joinparts.$$
if [ -n "$RUDOLF_IMG_SHA" ]; then
	H=$(sha256sum "$IMG.joining" | cut -c1-64)
	[ "$H" = "$RUDOLF_IMG_SHA" ] || { echo "The joined image does not match its .sha256: one of the parts is damaged. Download the parts again."; rm -f "$IMG.joining"; exit 8; }
	echo "The joined image matches its .sha256."
else
	echo "Joined without a checksum (no .sha256 was found); the image's signed release list is checked next."
fi
mv "$IMG.joining" "$IMG" && chown 1000:1000 "$IMG" 2>/dev/null
echo "Joined: $B. The parts can be deleted now (Downloads)."
