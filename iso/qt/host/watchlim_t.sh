#!/bin/sh
# 1.5.6: how long the session's watchdog waits for the desktop before restarting the browser (the lines that set LIM
# in /usr/lib/rudolf/session, run with a stand-in /run/rudolf). On the Vivobook it restarted the browser 126 s into a
# memory shortage with Among Us in front.   sh watchlim_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); S=$HERE/../../sys/root/usr/lib/rudolf/session; T=$(mktemp -d); trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
sed -n '/^\t*LIM=60;/,/^\t*case "$(cat \/run\/rudolf\/wm/p' "$S" | sed "s#/run/rudolf#$T#g" > $T/lim.sh
[ "$(grep -c . $T/lim.sh)" -ge 3 ] || { echo "FAIL the LIM lines were not found"; exit 1; }
lim() {   # wm, seconds since memory was critical (or none) -> LIM
	echo "$1" > $T/wm; rm -f $T/memguard.critical; [ "$2" != none ] && touch -d "@$(( $(date +%s) - $2 ))" $T/memguard.critical
	sh -c ". $T/lim.sh; echo \$LIM"; }
r=$(lim desktop none); ok "the desktop in front, memory fine: 60 s" "$([ "$r" = 60 ] && echo 1)" "$r"
r=$(lim desktop 10); ok "the desktop in front, memory short now: 120 s" "$([ "$r" = 120 ] && echo 1)" "$r"
r=$(lim desktop 100); ok "the desktop in front, memory short 100 s ago: 60 s" "$([ "$r" = 60 ] && echo 1)" "$r"
r=$(lim "win 3a00001" 10); ok "a game in front, memory short now: 300 s" "$([ "$r" = 300 ] && echo 1)" "$r"
r=$(lim "win 3a00001" 200); ok "a game in front, memory short 200 s ago: 300 s" "$([ "$r" = 300 ] && echo 1)" "$r"
r=$(lim "win 3a00001" none); ok "a game in front, memory fine: 60 s (a desktop that hangs is still restarted)" "$([ "$r" = 60 ] && echo 1)" "$r"
r=$(lim "win 3a00001" 400); ok "a game in front, memory short over 5 minutes ago: 60 s" "$([ "$r" = 60 ] && echo 1)" "$r"
echo "$((N-F))/$N passed"; [ $F = 0 ]
