#!/bin/sh
# 1.6.0 (the owner's photo of 2026-11-15): Oracle's and its Tor windows' Chromium, started by the native desktop, end when that desktop
# has ended, before the next desktop starts (the session's oracle_leftovers): left running, Oracle's held Standard's profile, and
# Standard's Chromium, started next with it, only handed its address over and ended (the desktop in plain Chromium windows). From
# the session's own functions, in a PID namespace of its own (only the test's own processes are seen and ended): stand-ins with
# Oracle's, Tor's and Standard's classes; one that does not end on TERM; and nothing to end.   sh oraleft60_t.sh (root)
HERE=$(cd "$(dirname "$0")" && pwd); S=$HERE/../../sys/root/usr/lib/rudolf/session
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
command -v unshare >/dev/null || { echo "SKIP: needs unshare"; exit 0; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
for f in oracle_left oracle_leftovers; do sed -n "/^$f() {/,/^}/p; /^$f() {.*}$/p" "$S"; done > $W/f.sh
for f in oracle_left oracle_leftovers; do grep -q "^$f()" $W/f.sh || { echo "FAIL $f is not in the session"; exit 1; }; done
grep -q '^	\[ "$ENGINE" = native \] && oracle_leftovers' "$S" || { echo "FAIL the session's loop does not end them after a native desktop"; exit 1; }
unshare -p -f --mount-proc sh -s "$W" <<'XEOF'
W=$1; P=0; F=0; ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
sleep() { command sleep 0.2; }   # (the session's own waits, shortened)
. $W/f.sh
fake() { sh -c 'trap "" TERM; while :; do command sleep 1; done' fake "$@" & }
fake2() { sh -c 'while :; do command sleep 1; done' fake "$@" & }
fake2 /opt/chromium/chrome --class=RudolfOracle --user-data-dir=/x; O=$!; fake2 /opt/chromium/chrome --class=RudolfTor; T=$!; fake2 /opt/chromium/chrome --class=RudolfDesktop --rudolf-kiosk; K=$!
command sleep 0.3
oracle_leftovers > $W/out
command sleep 0.3
ok "1. Oracle's Chromium ended" $(! kill -0 $O 2>/dev/null; echo $?)
ok "2. its Tor windows' Chromium ended" $(! kill -0 $T 2>/dev/null; echo $?)
ok "3. Standard's own Chromium left alone" $(kill -0 $K 2>/dev/null; echo $?)
ok "4. the session's log says so" $(grep -q "ending Oracle's Chromium left by the native desktop" $W/out; echo $?)
fake /opt/chromium/chrome --class=RudolfOracle; O=$!; command sleep 0.3
oracle_leftovers > /dev/null; command sleep 0.3
ok "5. one that does not end on TERM: ended all the same (KILL)" $(! kill -0 $O 2>/dev/null; echo $?)
: > $W/out; oracle_leftovers > $W/out; r=$?
ok "6. nothing to end: nothing done" $([ $r = 0 ] && [ ! -s $W/out ]; echo $?)
kill $K 2>/dev/null
echo "$P passed, $F failed"; [ $F = 0 ]
XEOF
