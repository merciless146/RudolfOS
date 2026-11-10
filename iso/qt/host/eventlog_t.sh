#!/bin/sh
# 1.5.7 (plan stage 1): the desktop's errors and rudolfd's log on the disk.
# - rudolfd: a POST to /api/clientlog reaches the writer (the reading handler caught it, so page.log was never written);
# - rudolf-init: rudolfd's log is /persist/var/log/rudolfd.log, the 3 before it kept as .1-.3, and
#   /run/rudolf/rudolfd.log points to it; without /persist it stays in /run.   sh eventlog_t.sh (root)
HERE=$(cd "$(dirname "$0")" && pwd); C=$HERE/../../sys/rudolfd.c; I=$HERE/../../sys/root/sbin/rudolf-init
[ "$(id -u)" = 0 ] || { echo "run as root"; exit 2; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; P=0; F=0
ok() { if [ "$2" = 1 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 - $3"; fi; }
# the first "clientlog" handler in rudolfd's dispatch must be for reading only, and the writer must follow it
L1=$(grep -n 'strcmp(p, "clientlog")' "$C" | sed -n 1p); L2=$(grep -n 'strcmp(p, "clientlog")' "$C" | sed -n 2p)
ok "1. the reading handler of clientlog leaves POST to the writer" "$(echo "$L1" | grep -q '&& !post' && echo 1)" "$L1"
ok "2. ...and the writer appends to page.log" "$(sed -n "${L2%%:*},+5p" "$C" | grep -q 'O_APPEND' && echo 1)" "$L2"
# rudolf-init's block, with rudolfd replaced by a stand-in that writes one line
sed -n "/^# 1.5.7: rudolfd's log/,/^rudolfd > /p" "$I" | sed 's/^rudolfd > /echo "start $N" > /; s/ 2>&1 &$//' > $W/blk
grep -q 'RDLOG' $W/blk || { echo "FAIL the block is not in rudolf-init"; exit 1; }
unshare -m --propagation private sh -s "$W" > $W/out <<'XEOF'
W=$1; mount -t tmpfs t /run; mkdir -p /run/rudolf /persist; mount -t tmpfs t /persist
for N in 1 2 3 4 5; do export N; sh $W/blk; done
echo "cur=$(cat /run/rudolf/rudolfd.log) link=$(readlink /run/rudolf/rudolfd.log) 1=$(cat /persist/var/log/rudolfd.log.1) 3=$(cat /persist/var/log/rudolfd.log.3) 4=$([ -e /persist/var/log/rudolfd.log.4 ] && echo yes || echo no) mode=$(stat -c %a /persist/var/log/rudolfd.log)"
umount /persist; rm -f /run/rudolf/rudolfd.log; N=6 sh $W/blk
echo "nopersist=$(cat /run/rudolf/rudolfd.log) islink=$([ -L /run/rudolf/rudolfd.log ] && echo yes || echo no)"
XEOF
o=$(cat $W/out)
ok "3. the log is on the disk and /run/rudolf/rudolfd.log points to it" "$(echo "$o" | grep -q 'cur=start 5 link=/persist/var/log/rudolfd.log ' && echo 1)" "$o"
ok "4. the 3 start-ups before it are kept (.1 the last, .3 the oldest), no more" "$(echo "$o" | grep -q '1=start 4 3=start 2 4=no mode=600' && echo 1)" "$o"
ok "5. without /persist (a live start) the log stays in /run" "$(echo "$o" | grep -q 'nopersist=start 6 islink=no' && echo 1)" "$o"
echo "$P/$((P+F)) passed"; [ $F = 0 ]
