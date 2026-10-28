#!/bin/sh
# layout.sh (1.4.1) on the host: the installed layout (partition 1 cleared, a disk signature of its own, partitions 2
# and 3 kept) and a stick's table (partition 1 back after copying an installed drive's image). sh layout_t.sh
L=$(cd "$(dirname "$0")/../../sys/root/usr/lib/rudolf" && pwd)/layout.sh; T=$(mktemp -d); P=0; N=0
ok() { N=$((N + 1)); if [ "$1" = 0 ]; then P=$((P + 1)); echo "PASS $2"; else echo "FAIL $2 - $3"; fi; }
hx() { dd if="$1" bs=1 skip=$2 count=$3 2>/dev/null | od -An -tx1 | tr -d ' \n'; }
# a drive as Install here wrote it before 1.4.1: the image's hybrid table (as on the Vivobook) and RUDOLFDATA
truncate -s 64M $T/d.img
printf '\075\121\314\152\000\000\200\376\377\377\027\376\377\377\000\000\000\000\000\200\014\000\000\376\377\377\357\376\377\377\130\114\000\000\000\140\000\000\000\376\377\377\203\376\377\377\000\200\014\000\260\072\333\016' | dd of=$T/d.img bs=1 seek=440 conv=notrunc 2>/dev/null
printf '\125\252' | dd of=$T/d.img bs=1 seek=510 conv=notrunc 2>/dev/null
. "$L"
layout_removable $T/d.img; ok $([ $? != 0 ]; echo $?) "a drive that is not on USB or a memory card is internal"
layout_installed $T/d.img
ok $([ "$(hx $T/d.img 446 16)" = 00000000000000000000000000000000 ]; echo $?) "installed layout: partition 1 is cleared" "$(hx $T/d.img 446 16)"
ok $([ "$(hx $T/d.img 462 16)" = 00feffffeffeffff584c000000600000 ] && [ "$(hx $T/d.img 478 16)" = 00feffff83feffff00800c00b03adb0e ]; echo $?) "...the EFI system partition and RUDOLFDATA are as they were" "$(hx $T/d.img 462 32)"
S1=$(hx $T/d.img 440 4); ok $([ "$S1" != 3d51cc6a ] && [ "$S1" != 00000000 ]; echo $?) "...the drive gets a disk signature of its own" "$S1"
layout_installed $T/d.img; S2=$(hx $T/d.img 440 4)
ok $([ "$S2" != "$S1" ] && [ "$(hx $T/d.img 510 2)" = 55aa ]; echo $?) "...a new one each time, and the table stays valid (55 aa)" "$S2"
layout_stick $T/d.img 204800
ok $([ "$(hx $T/d.img 446 16)" = 80feffff17feffff0000000000800c00 ]; echo $?) "a stick written from an installed drive's image gets partition 1 back (start 0, the image's size)" "$(hx $T/d.img 446 16)"
cp $T/d.img $T/e.img; printf '\001' | dd of=$T/e.img bs=1 seek=458 conv=notrunc 2>/dev/null; layout_stick $T/e.img 4
ok $([ "$(hx $T/e.img 446 16)" = 80feffff17feffff0000000001800c00 ]; echo $?) "...and a stick's own partition 1 is left as it is" "$(hx $T/e.img 446 16)"
rm -rf $T; echo "$P/$N passed"; [ $P = $N ]
