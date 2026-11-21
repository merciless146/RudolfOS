#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 19): /usr/lib/rudolf/gamemode with a stand-in /proc, cgroup tree and power command: a full
# screen said again (on), ended (off) and no longer said (expired: the page or the native desktop stopped), a game process
# that crashes, the desktop slowed only behind a Linux game (not a page's own full screen, not a game in the desktop's own
# group), the state before and after (cpu.weight and the cpu controller as they were), GameMode's D-Bus requests (counted
# while the game and its portal run and are the request's owner's; never slowing the desktop), Game mode off, the watcher
# putting everything back by itself, and the power settings asked for game=1 and game=0. No root needed.
#   sh gamemode161_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); G=$HERE/../../sys/root/usr/lib/rudolf/gamemode
T=$(mktemp -d /tmp/gamemode161.XXXXXX); trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 600)"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
ME=$(id -u)
export GM_RUN=$T/run GM_REQ=$T/req GM_CONF=$T/gamemode.conf GM_CG=$T/cg GM_PROC=$T/proc GM_POWER=$T/power GM_NOW=$T/now GM_NOWATCH=1
printf '#!/bin/sh\necho "$*" >> %s/power.log\n' $T > $T/power; chmod +x $T/power
proc() { mkdir -p $T/proc/$1; printf 'Name:\t%s\nState:\tS (sleeping)\nUid:\t%s\t%s\t%s\t%s\n' "$2" "${3:-$ME}" "${3:-$ME}" "${3:-$ME}" "${3:-$ME}" > $T/proc/$1/status; echo "$2" > $T/proc/$1/comm; echo "0::${4:-/}" > $T/proc/$1/cgroup; }
reset() { rm -rf $T/run $T/req $T/cg $T/proc $T/power.log $T/gamemode.conf; mkdir -p $T/run $T/req $T/cg/desktop $T/proc; echo memory > $T/cg/cgroup.subtree_control; echo 100 > $T/cg/desktop/cpu.weight; date +%s > $T/now; }
st() { sh $G status; }
w() { cat $T/cg/desktop/cpu.weight; }
pw() { tail -1 $T/power.log 2>/dev/null; }

reset; S=$(st); ok "1. nothing: off, the power settings not asked" $( [ "$(has "$S" '"on":false')$( [ ! -e $T/power.log ] && echo 1)" = 11 ] && echo 1) "$S"
S=$(sh $G on page linux); ok "2. Standard's page: a Linux game full screen: on, game=1 asked of the power settings" $( [ "$(has "$S" '"on":true')$( [ "$(pw)" = "set game=1" ] && echo 1)" = 11 ] && echo 1) "$S $(pw)"
ok "3. ...the desktop slowed behind it: the cpu controller on for its group, cpu.weight 20" $( [ "$(has "$S" '"throttled":true')$(has "$(cat $T/cg/cgroup.subtree_control)" cpu)$( [ "$(w)" = 20 ] && echo 1)" = 111 ] && echo 1) "$S / $(cat $T/cg/cgroup.subtree_control) / $(w)"
ok "4. ...the source in the status (page, linux), the event for the pages" $( [ "$(has "$S" '{"src":"page","kind":"linux","pid":null}')$(has "$(cat $T/run/events)" 'game on')" = 11 ] && echo 1) "$S"
S=$(sh $G off page); ok "5. off: game=0, cpu.weight back to 100, the cpu controller off again (as it was), the event" $( [ "$(has "$S" '"on":false')$( [ "$(pw)" = "set game=0" ] && echo 1)$( [ "$(w)" = 100 ] && echo 1)$(has "$(cat $T/cg/cgroup.subtree_control)" -cpu)$(has "$(cat $T/run/events)" 'game off')" = 11111 ] && echo 1) "$S $(pw) $(w)"
reset; echo 50 > $T/cg/desktop/cpu.weight; echo "memory cpu" > $T/cg/cgroup.subtree_control
sh $G on native linux >/dev/null; sh $G off native >/dev/null
ok "6. the state before kept: a weight of 50 back to 50, a cpu controller already on left on" $( [ "$(w)" = 50 ] && [ "$(cat $T/cg/cgroup.subtree_control)" = "memory cpu" ] && echo 1) "$(w) / $(cat $T/cg/cgroup.subtree_control)"
reset; S=$(sh $G on page page); ok "7. a page's own full screen (a game in an Oracle tab): on, the desktop not slowed (the game is the desktop's)" $( [ "$(has "$S" '"on":true')$(has "$S" '"throttled":false')$( [ "$(w)" = 100 ] && echo 1)" = 111 ] && echo 1) "$S"
sh $G off page >/dev/null
proc 4242 chrome 1000 /desktop; S=$(sh $G on native oracle 4242)
ok "8. Native: Oracle's window full screen (its process in the desktop's group): on, not slowed" $( [ "$(has "$S" '"on":true')$(has "$S" '"throttled":false')" = 11 ] && echo 1) "$S"
sh $G off native >/dev/null
# expiry and crashes
reset; sh $G on page linux >/dev/null; echo $(( $(date +%s) + 7 )) > $T/now; S=$(st)
ok "9. a source not said again for 6 s (the page stopped or crashed): off, everything back" $( [ "$(has "$S" '"on":false')$( [ "$(w)" = 100 ] && echo 1)$( [ "$(pw)" = "set game=0" ] && echo 1)" = 111 ] && echo 1) "$S $(w)"
reset; proc 5151 game; S=$(sh $G on native linux 5151); ok "10. Native: a Linux game's window full screen, its process named: on, slowed" $( [ "$(has "$S" '"on":true')$(has "$S" '"throttled":true')$(has "$S" '"pid":5151')" = 111 ] && echo 1) "$S"
rm -rf $T/proc/5151; S=$(st); ok "11. the game crashes (its process gone): off at once, cpu.weight back" $( [ "$(has "$S" '"on":false')$( [ "$(w)" = 100 ] && echo 1)" = 11 ] && echo 1) "$S"
reset; proc 5252 game; sh $G on native linux 5252 >/dev/null; sed -i 's/^State:.*/State:\tZ (zombie)/' $T/proc/5252/status; S=$(st)
ok "12. ...or a zombie: off" $(has "$S" '"on":false') "$S"
# GameMode's D-Bus requests
reset; proc 700 rudolf-portal; proc 701 supertux2; : > $T/req/$ME-700-701; S=$(st)
ok "13. a game that asked through GameMode (its portal running, both the owner's): on, game=1, the desktop NOT slowed" $( [ "$(has "$S" '"on":true')$(has "$S" '"throttled":false')$(has "$S" '"name":"supertux2"')$( [ "$(pw)" = "set game=1" ] && echo 1)" = 1111 ] && echo 1) "$S"
rm -rf $T/proc/701; S=$(st); ok "14. ...the game ends (or crashes): off, its request removed" $( [ "$(has "$S" '"on":false')$( [ ! -e $T/req/$ME-700-701 ] && echo 1)" = 11 ] && echo 1) "$S"
proc 702 supertux2; : > $T/req/$ME-700-702; rm -rf $T/proc/700; S=$(st); ok "15. a request whose portal is gone: not counted, removed" $( [ "$(has "$S" '"on":false')$( [ ! -e $T/req/$ME-700-702 ] && echo 1)" = 11 ] && echo 1) "$S"
proc 700 rudolf-portal; proc 703 othergame 4321; : > $T/req/$ME-700-703; S=$(st); ok "16. a request for another user's process: not counted" $(has "$S" '"on":false') "$S"
proc 704 notportal; proc 705 game; : > $T/req/$ME-704-705; S=$(st); ok "17. a request file made by something that is not the portal: not counted" $(has "$S" '"on":false') "$S"
: > $T/req/$ME-700-705; : > "$T/req/../x"; : > $T/req/junk-file; S=$(st); ok "18. ...a good one beside junk files: counted, the junk removed" $( [ "$(has "$S" '"on":true')$( [ ! -e $T/req/junk-file ] && echo 1)" = 11 ] && echo 1) "$S"
# the setting
reset; S=$(sh $G set enabled=0); S=$(sh $G on page linux); ok "19. Game mode off (Settings): a full screen changes nothing" $( [ "$(has "$S" '"on":false')$(has "$S" '"enabled":false')$( [ ! -e $T/power.log ] && echo 1)$( [ "$(w)" = 100 ] && echo 1)" = 1111 ] && echo 1) "$S"
S=$(sh $G set enabled=1); ok "20. ...on again: the full screen still said counts at once" $(has "$S" '"on":true') "$S"
sh $G on page linux >/dev/null; S=$(sh $G set enabled=0); ok "21. turned off during a game: everything back at once" $( [ "$(has "$S" '"on":false')$( [ "$(w)" = 100 ] && echo 1)$( [ "$(pw)" = "set game=0" ] && echo 1)" = 111 ] && echo 1) "$S"
R=$(sh $G on 'pa;ge' linux; echo rc=$?); ok "22. a bad source or kind is refused" $(has "$R" 'rc=2') "$R"
# the watcher: everything back by itself, with no one asking
reset; unset GM_NOWATCH; export GM_TICK=1; sh $G on page linux >/dev/null; i=0; while [ $i -lt 30 ] && [ ! -e $T/run/game/watch/pid ]; do i=$((i+1)); timeout 0.1 tail -f /dev/null; done
ok "23. the watcher runs while game mode is on" $( [ -e $T/run/game/watch/pid ] && echo 1)
echo $(( $(date +%s) + 7 )) > $T/now; i=0; while [ $i -lt 50 ] && [ -e $T/run/game/on ]; do i=$((i+1)); timeout 0.1 tail -f /dev/null; done
ok "24. ...and puts everything back by itself when the source stops (a crash of the page or rudolfd: no one asks again)" $( [ ! -e $T/run/game/on ] && [ "$(w)" = 100 ] && [ "$(pw)" = "set game=0" ] && echo 1) "$(ls $T/run/game) $(w) $(pw)"
i=0; while [ $i -lt 40 ] && [ -e $T/run/game/watch ]; do i=$((i+1)); timeout 0.1 tail -f /dev/null; done
ok "25. ...then it ends" $( [ ! -e $T/run/game/watch ] && echo 1)
echo "$((N-F))/$N passed"; [ $F = 0 ]
