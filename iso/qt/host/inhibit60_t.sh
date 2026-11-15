#!/bin/sh
# 1.6.0 (docs/plan-1.6.0.md Part 11): apps keeping the screen on or the computer awake. rudolf-portal on a private
# session bus as org.freedesktop.ScreenSaver, PowerManagement.Inhibit and the portal's Inhibit: each held request a file
# (its flags, the app, its reason), gone when the app lets go or leaves the bus, and a portal started again clears its
# own; rudolfd's inhibit_scan (compiled from rudolfd.c) counts them for the media answer, and leaves out a file that is
# not its owner's or whose portal is gone. Needs gcc, GLib, dbus-run-session.   sh inhibit60_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); D=$HERE/../../sys; T=$(mktemp -d /tmp/inh60.XXXXXX); chmod 755 $T
CF=$(pkg-config --cflags --libs gio-2.0 gio-unix-2.0)
mkdir -p $T/inh; chmod 1777 $T/inh
gcc -O2 -w -o $T/rudolf-portal $D/rudolf-portal.c $CF && gcc -O2 -w -o $T/client $HERE/fixtures/inhibitclient.c $CF && gcc -O1 -w -DINHIBIT_DIR="\"$T/inh\"" -o $T/scan $HERE/fixtures/inhibitscan.c -lz || { echo "FAIL build"; exit 1; }
cat > $T/t.sh <<'EOS'
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 600)"; F=$((F+1)); fi; }
export RUDOLF_INHIBIT_DIR=$T/inh
up() { $T/rudolf-portal 2>>$T/portal.log & PID=$!; i=0; until gdbus introspect --session -d org.freedesktop.ScreenSaver -o /org/freedesktop/ScreenSaver >/dev/null 2>&1 || [ $i -ge 50 ]; do i=$((i+1)); timeout 0.1 tail -f /dev/null; done; }
# (a client holds while its stdin, a fifo kept open by a sleeping writer, says nothing; say sends it a line)
hold() { rm -f $T/f$1; mkfifo $T/f$1; ( $T/client $2 $3 < $T/f$1 > $T/o$1 ) & sleep 300 > $T/f$1 & echo $! > $T/w$1; i=0; until grep -q HELD $T/o$1 2>/dev/null || [ $i -ge 50 ]; do i=$((i+1)); timeout 0.1 tail -f /dev/null; done; }
say() { echo "$2" > $T/f$1; timeout 0.5 tail -f /dev/null; case "$2" in exit|bye) kill $(cat $T/w$1) 2>/dev/null;; esac; }
files() { ls $T/inh | wc -l; }
up
hold 1 ss
f=$(ls $T/inh | head -1)
ok "1. ScreenSaver.Inhibit: held, a file with the idle flag, the app and its reason, the portal's own" "$(grep -q '^HELD [0-9]' $T/o1 && [ "$(files)" = 1 ] && grep -qx 'flags=8' $T/inh/$f && grep -qx 'app=Movie Player' $T/inh/$f && grep -qx 'reason=a film' $T/inh/$f && echo "$f" | grep -q "^$(id -u)-$PID-" && echo 1)" "$(cat $T/o1; ls -l $T/inh; cat $T/inh/* 2>/dev/null)"
o=$($T/scan); ok "...rudolfd counts it: the screen held, by Movie Player" "$([ "$o" = 'idle 1 sleep 0 by ["Movie Player"]' ] && echo 1)" "$o"
say 1 release
ok "...UnInhibit: gone" "$(grep -q RELEASED $T/o1 && [ "$(files)" = 0 ] && echo 1)" "$(cat $T/o1; ls $T/inh)"
say 1 bye
hold 2 pm; o=$($T/scan)
ok "2. PowerManagement.Inhibit: the computer held awake (the sleep flag)" "$([ "$o" = 'idle 0 sleep 1 by ["Movie Player"]' ] && echo 1)" "$o $(cat $T/o2)"
say 2 exit; timeout 1 tail -f /dev/null
ok "...the app leaves the bus without letting go: gone too" "$([ "$(files)" = 0 ] && echo 1)" "$(ls $T/inh)"
hold 3 portal 12; o=$($T/scan)
ok "3. the portal's Inhibit (Flatpak apps): both flags held, the Request's path given" "$(grep -q '^HELD /org/freedesktop/portal/desktop/request/' $T/o3 && [ "$o" = 'idle 1 sleep 1 by ["a Linux app"]' ] && grep -qx 'reason=playing a video' $T/inh/* && echo 1)" "$o $(cat $T/o3)"
say 3 release
ok "...Close on the Request: gone" "$([ "$(files)" = 0 ] && echo 1)" "$(ls $T/inh)"
say 3 bye
hold 4 portal 3; o=$($T/scan); say 4 exit
ok "...only the screen and sleep flags are kept (log-out and user-switch are not held)" "$([ "$o" = 'idle 0 sleep 0 by []' ] && echo 1)" "$o"
hold 5 ss; kill $PID; wait $PID 2>/dev/null; o=$($T/scan)
ok "4. its portal gone: rudolfd does not count it (and clears it)" "$([ "$o" = 'idle 0 sleep 0 by []' ] && [ "$(files)" = 0 ] && echo 1)" "$o $(ls $T/inh)"
say 5 exit
echo "flags=8" > $T/inh/$(id -u)-99999-1; up; o=$(ls $T/inh)
ok "...a portal started again clears its user's old files" "$([ -z "$o" ] && echo 1)" "$o"
if [ "$(id -u)" = 0 ]; then printf 'flags=8\napp=sneaky\n' > $T/inh/1234-$PID-9; o=$($T/scan)
	ok "5. a file named for another user than its owner is not counted" "$([ "$o" = 'idle 0 sleep 0 by []' ] && echo 1)" "$o"; rm -f $T/inh/1234-*; fi
r=$(gdbus call --session -d org.freedesktop.ScreenSaver -o /org/freedesktop/ScreenSaver -m org.freedesktop.ScreenSaver.GetActive 2>&1)
ok "6. ScreenSaver.GetActive answers (no saver of its own)" "$([ "$r" = '(false,)' ] && echo 1)" "$r"
kill $PID; echo "$((N-F))/$N passed"; [ $F = 0 ]
EOS
T=$T dbus-run-session -- sh $T/t.sh > $T/out 2>&1; r=$?; grep -v dbus-daemon $T/out; rm -rf $T; exit $r
