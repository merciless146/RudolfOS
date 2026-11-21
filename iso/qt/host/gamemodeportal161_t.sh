#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 19): GameMode's D-Bus API in rudolf-portal, on a private session bus, as games ask it
# (fixtures/gmclient.c, as libgamemode does): com.feralinteractive.GameMode's RegisterGame, QueryStatus, UnregisterGame and
# ClientCount; the portal's GameMode by pid and by pidfd (Flatpak games); each registered game a request file for
# /usr/lib/rudolf/gamemode (UID-PORTALPID-GAMEPID, its owner's), which counts it (the processor's preference raised) and
# stops counting when the game ends; refused: a process that does not run, another user's (as a normal user), a game twice;
# the caller leaving the bus lets go; a portal started again clears its user's old files. Needs gcc, GLib, dbus-run-session.
HERE=$(cd "$(dirname "$0")" && pwd); D=$HERE/../../sys; T=$(mktemp -d /tmp/gmp161.XXXXXX); chmod 755 $T
CF="$(pkg-config --cflags --libs gio-2.0 gio-unix-2.0) -lcrypto"
mkdir -p $T/req; chmod 1777 $T/req
gcc -O2 -w -o $T/rudolf-portal $D/rudolf-portal.c $CF && gcc -O2 -w -o $T/client $HERE/fixtures/gmclient.c $CF || { echo "FAIL build"; exit 1; }
cat > $T/t.sh <<'EOS'
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 600)"; F=$((F+1)); fi; }
export RUDOLF_GAMEREQ_DIR=$T/req
G=$HERE/../../sys/root/usr/lib/rudolf/gamemode
export GM_RUN=$T/run GM_REQ=$T/req GM_CONF=$T/gm.conf GM_CG=$T/cg GM_POWER=$T/power GM_NOWATCH=1; mkdir -p $T/run $T/cg/desktop; echo 100 > $T/cg/desktop/cpu.weight
printf '#!/bin/sh\necho "$*" >> %s/power.log\n' $T > $T/power; chmod +x $T/power
up() { $T/rudolf-portal 2>>$T/portal.log & PID=$!; i=0; until gdbus introspect --session -d com.feralinteractive.GameMode -o /com/feralinteractive/GameMode >/dev/null 2>&1 || [ $i -ge 50 ]; do i=$((i+1)); timeout 0.1 tail -f /dev/null; done; }
hold() { rm -f $T/f$1; mkfifo $T/f$1; ( $T/client $2 $3 < $T/f$1 > $T/o$1 ) & sleep 300 > $T/f$1 & echo $! > $T/w$1; i=0; until grep -q '^Q ' $T/o$1 2>/dev/null || [ $i -ge 50 ]; do i=$((i+1)); timeout 0.1 tail -f /dev/null; done; }
say() { echo "$2" > $T/f$1; timeout 0.5 tail -f /dev/null; case "$2" in exit|bye) kill $(cat $T/w$1) 2>/dev/null; timeout 0.5 tail -f /dev/null;; esac; }
files() { ls $T/req | wc -l; }
cc() { gdbus call --session -d com.feralinteractive.GameMode -o /com/feralinteractive/GameMode -m org.freedesktop.DBus.Properties.Get com.feralinteractive.GameMode ClientCount 2>&1; }
up
hold 1 feral; f=$(ls $T/req | head -1)
ok "1. RegisterGame (its own pid): 0, QueryStatus 2 (registered), a request file named UID-PORTALPID-GAMEPID" "$(grep -qx 'REG 0' $T/o1 && grep -qx 'Q 2' $T/o1 && echo "$f" | grep -qE "^$(id -u)-$PID-[0-9]+$" && echo 1)" "$(cat $T/o1; ls $T/req)"
ok "...ClientCount 1" "$( [ "$(cc)" = '(<1>,)' ] && echo 1)" "$(cc)"
S=$(sh $G status); ok "...gamemode counts it: on, game=1 asked, the desktop not slowed" "$(echo "$S" | grep -q '"on":true' && echo "$S" | grep -q '"throttled":false' && grep -qx 'set game=1' $T/power.log && echo 1)" "$S"
say 1 unreg; ok "...UnregisterGame: 0, QueryStatus 0, the file gone; gamemode off" "$(grep -qx 'UNREG 0' $T/o1 && tail -1 $T/o1 | grep -qx 'Q 0' && [ "$(files)" = 0 ] && sh $G status | grep -q '"on":false' && echo 1)" "$(cat $T/o1; ls $T/req)"
say 1 bye
hold 2 feral; say 2 exit
ok "2. a game that leaves the bus without unregistering (it ended): let go" "$([ "$(files)" = 0 ] && echo 1)" "$(ls $T/req)"
sleep 300 & G2=$!; hold 3 feral $G2
ok "3. RegisterGame for another process of the same user (a launcher for its game): 0" "$(grep -qx 'REG 0' $T/o3 && echo 1)" "$(cat $T/o3)"
kill $G2; wait $G2 2>/dev/null; S=$(sh $G status)
ok "...the game crashes: gamemode stops counting it at once (and removes its file)" "$(echo "$S" | grep -q '"on":false' && [ "$(files)" = 0 ] && echo 1)" "$S $(ls $T/req)"
say 3 bye
hold 4 feral 999999; ok "4. a process that does not run: refused (-1), no file" "$(grep -qx 'REG -1' $T/o4 && [ "$(files)" = 0 ] && echo 1)" "$(cat $T/o4)"; say 4 bye
if [ "$(id -u)" != 0 ]; then hold 5 feral 1; ok "5. another user's process (pid 1, root's): refused" "$(grep -qx 'REG -1' $T/o5 && [ "$(files)" = 0 ] && echo 1)" "$(cat $T/o5)"; say 5 bye; fi
hold 6 portal; ok "6. the portal's GameMode (a pid, outside a sandbox): 0, then 2" "$(grep -qx 'REG 0' $T/o6 && grep -qx 'Q 2' $T/o6 && [ "$(files)" = 1 ] && echo 1)" "$(cat $T/o6)"
say 6 bye
hold 7 pidfd; ok "7. the portal's GameMode by pidfd (Flatpak's libgamemode): 0, then 2, a file" "$(grep -qx 'REG 0' $T/o7 && grep -qx 'Q 2' $T/o7 && [ "$(files)" = 1 ] && echo 1)" "$(cat $T/o7)"
say 7 unreg; ok "...UnregisterGameByPIDFd: 0, the file gone" "$(grep -qx 'UNREG 0' $T/o7 && [ "$(files)" = 0 ] && echo 1)" "$(cat $T/o7)"; say 7 bye
hold 8 feral; r=$(gdbus call --session -d com.feralinteractive.GameMode -o /com/feralinteractive/GameMode -m com.feralinteractive.GameMode.RegisterGame $(ls $T/req | sed "s/.*-//") 2>&1)
ok "8. the same game twice: refused (-1)" "$( [ "$r" = '(-1,)' ] && echo 1)" "$r"; say 8 bye
echo x > $T/req/$(id -u)-99999-1; kill $PID; wait $PID 2>/dev/null; up; o=$(ls $T/req)
ok "9. a portal started again clears its user's old files" "$([ -z "$o" ] && echo 1)" "$o"
r=$(gdbus call --session -d com.feralinteractive.GameMode -o /com/feralinteractive/GameMode -m com.feralinteractive.GameMode.SetGovernor 1 2>&1)
ok "10. nothing else is there (no governor, no scripts)" "$(echo "$r" | grep -qi 'unknown\|No such method\|doesn' && echo 1)" "$r"
kill $PID; echo "$((N-F))/$N passed"; [ $F = 0 ]
EOS
T=$T HERE=$HERE dbus-run-session -- sh $T/t.sh > $T/out 2>&1; r=$?; grep -v dbus-daemon $T/out; rm -rf $T; exit $r
