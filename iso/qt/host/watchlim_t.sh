#!/bin/sh
# 1.5.6: how long the session's watchdog waits for the desktop before restarting the browser (the lines that set LIM
# in /usr/lib/rudolf/session, run with a stand-in /run/rudolf). On the Vivobook it restarted the browser 126 s into a
# memory shortage with Among Us in front.   sh watchlim_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); S=$HERE/../../sys/root/usr/lib/rudolf/session; T=$(mktemp -d); trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
sed -n '/^\t*LIM=60;/,/^\t*\[ \$((W - BUSY_T))/p' "$S" | sed "s#/run/rudolf#$T#g" > $T/lim.sh
[ "$(grep -c BUSY_T $T/lim.sh)" -ge 1 ] || { echo "FAIL the LIM lines were not found"; exit 1; }
lim() {   # wm, seconds since memory was critical (or none), [seconds since the desktop was last busy] -> LIM
	echo "$1" > $T/wm; rm -f $T/memguard.critical; [ "$2" != none ] && touch -d "@$(( $(date +%s) - $2 ))" $T/memguard.critical
	W=$(date +%s); BUSY_T=$((W - ${3:-9999})) W=$W sh -c ". $T/lim.sh; echo \$LIM"; }
r=$(lim desktop none); ok "the desktop in front, memory fine: 60 s" "$([ "$r" = 60 ] && echo 1)" "$r"
r=$(lim desktop 10); ok "the desktop in front, memory short now: 120 s" "$([ "$r" = 120 ] && echo 1)" "$r"
r=$(lim desktop 100); ok "the desktop in front, memory short 100 s ago: 60 s" "$([ "$r" = 60 ] && echo 1)" "$r"
r=$(lim "win 3a00001" 10); ok "a game in front, memory short now: 300 s" "$([ "$r" = 300 ] && echo 1)" "$r"
r=$(lim "win 3a00001" 200); ok "a game in front, memory short 200 s ago: 300 s" "$([ "$r" = 300 ] && echo 1)" "$r"
r=$(lim "win 3a00001" none); ok "a game in front, memory fine: 60 s (a desktop that hangs is still restarted)" "$([ "$r" = 60 ] && echo 1)" "$r"
r=$(lim "win 3a00001" 400); ok "a game in front, memory short over 5 minutes ago: 60 s" "$([ "$r" = 60 ] && echo 1)" "$r"
# 1.5.6: busy getting its memory back (Steam's window closed on the Vivobook: white for over a minute, memory never
# critical, restarted after 62 s)
r=$(lim desktop none 10); ok "the desktop busy reading its memory back 10 s ago: 300 s" "$([ "$r" = 300 ] && echo 1)" "$r"
r=$(lim desktop none 60); ok "busy a minute ago, nothing since: 60 s (a desktop that hangs is still restarted)" "$([ "$r" = 60 ] && echo 1)" "$r"
# desk_faults: the major faults of the desktop page and the browser (field 12 of /proc/PID/stat)
sed -n '/^desk_faults()/,/; }$/p' "$S" | sed "s#/run/rudolf#$T#g" > $T/df.sh
sleep 30 & SP=$!; echo $SP > $T/desktop-renderer
r=$(sh -c ". $T/df.sh; desk_faults"); x=$(awk '{print $12}' /proc/$SP/stat); kill $SP
ok "desk_faults reads the desktop page's major faults" "$([ "$r" = "$x" ] && [ -n "$r" ] && echo 1)" "$r vs $x"
# 1.5.6: back in front after a game, the desktop is judged from that moment (its page was hidden: Chromium slows a
# hidden page's timers, and its heartbeat stops; the Vivobook's browser was restarted the moment Steam's game closed)
grep 'WMN=$(cut' "$S" | sed "s#/run/rudolf#$T#g" > $T/front.sh; grep -A1 'FRONT_T -gt $MT' "$S" > $T/age.sh
[ -s $T/front.sh ] && [ -s $T/age.sh ] || ok "the front lines were found" 0 "missing"
front() {   # wm before, wm now -> FRONT_T set (W) or not (0)
	echo "$2" > $T/wm; WM0=$1 FRONT_T=0 W=1000 sh -c ". $T/front.sh; echo \$FRONT_T"; }
r=$(front "win 3a00001" desktop); ok "a game closed: the desktop's time in front starts now" "$([ "$r" = 1000 ] && echo 1)" "$r"
r=$(front desktop desktop); ok "the desktop in front all along: nothing changes" "$([ "$r" = 0 ] && echo 1)" "$r"
r=$(front desktop "win 3a00001"); ok "a game comes in front: nothing changes" "$([ "$r" = 0 ] && echo 1)" "$r"
age() { W=$1 MT=$2 FRONT_T=$3 WMN=${4:-desktop} AGE=$(($1 - $2)) sh -c ". $T/age.sh; echo \$AGE"; }
r=$(age 1000 787 1000); ok "heartbeat 213 s old, the game closed just now: judged as 0 s (not restarted)" "$([ "$r" = 0 ] && echo 1)" "$r"
r=$(age 1070 787 1000); ok "...70 s later and still no heartbeat: 70 s (over the 60 s limit: restarted)" "$([ "$r" = 70 ] && echo 1)" "$r"
r=$(age 1070 1065 1000); ok "a heartbeat since it came back: its own age as usual" "$([ "$r" = 5 ] && echo 1)" "$r"
r=$(age 1400 787 0 win); ok "behind a game the desktop is not judged at all (a restart would close the game too)" "$([ "$r" = 0 ] && echo 1)" "$r"
echo "$((N-F))/$N passed"; [ $F = 0 ]
