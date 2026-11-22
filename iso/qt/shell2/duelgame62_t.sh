#!/bin/sh
# 1.6.2 (docs/plan-1.6.2.md Part 30): Ember Reach itself dueling on a LAN through Rudolf OS's relay. The Library's own
# two-page test (rudolfos-library tests/emberreach_duel_net_browser_t.js: two Chromium pages, the desktop's lanHost and
# lanFind answered as Rudolf OS would) with its LAN relay, relay.mjs --lan, swapped for rudolf-duel as built from this
# tree (run here as this user, on loopback). Every step of the Library's test must pass with it: the LAN duel, found and
# joined, and the service stopped after. Needs a rudolfos-library checkout next to this repository (or at
# RUDOLFOS_LIBRARY), Node with Playwright, python3 and gcc. Ports 8860-8862.   sh duelgame62_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; LIBR=${RUDOLFOS_LIBRARY:-$HERE/../../../../rudolfos-library}
TEST=$LIBR/tests/emberreach_duel_net_browser_t.js
[ -f "$TEST" ] || { echo "SKIP: no rudolfos-library checkout with Ember Reach's network duels (0.5.2)"; exit 0; }
T=$(mktemp -d /tmp/duelgame62.XXXXXX); trap 'rm -rf "$T"' EXIT
. $SYS/../systools/hardflags.sh 2>/dev/null
gcc -O2 $HARD -o $T/rudolf-duel $SYS/rudolf-duel.c || { echo "FAIL: rudolf-duel does not build"; exit 1; }
VER=$(sed -n "s/^const BUILD = {name: '[^']*', version: '\([^']*\)'};/\1/p" $LIBR/apps/rudolf-emberreach/www/play.js)
LINE="const lan = spawn('node', [path.join(TOP, 'tools/duel-relay/relay.mjs'), '--port', String(LAN), '--host', '127.0.0.1', '--lan', 'HANDUE'], {stdio: 'ignore'});"
grep -qF "$LINE" $TEST || { echo "FAIL: the Library's test no longer starts its LAN relay as this test expects (update duelgame62_t.sh)"; exit 1; }
# (as root, rudolf-duel refuses to run unless told it is a test)
NEW="const lan = spawn('$T/rudolf-duel', ['serve', '--port', String(LAN), '--code', 'HANDUE', '--v', '$VER', '--game', 'ember-reach', '--name', 'test', '--addr', '127.0.0.1', '--wait', '600'], {stdio: ['ignore', 'ignore', 'inherit'], env: Object.assign({RD_TEST_ROOT: '1'}, process.env)});"
python3 - "$TEST" "$T/t.js" "$LINE" "$NEW" "$LIBR" <<'PY'
import sys
src, out, line, new, top = sys.argv[1:6]
s = open(src).read().replace(line, new).replace("TOP = path.join(__dirname, '..')", "TOP = " + repr(top))
open(out, 'w').write(s)
PY
o=$(cd $T && node t.js 8860 2>&1); echo "$o" | grep -E '^(PASS|FAIL)'
echo "$o" | grep -q '^FAIL' && { echo "FAIL: Ember Reach's duels through rudolf-duel"; exit 1; }
echo "$o" | grep -q '^PASS 18\. LAN' || { echo "FAIL: the LAN step did not run"; exit 1; }
echo "$o" | tail -1
