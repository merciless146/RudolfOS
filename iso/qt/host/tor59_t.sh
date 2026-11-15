#!/bin/sh
# 1.5.9 (docs/plan-1.5.9.md 16.1): rudolfd's Tor for Oracle's Tor windows (tor/start, tor/status, tor/stop and its loop's
# check), compiled from rudolfd.c (fixtures/torapi.c). With a stand-in tor that writes Tor's bootstrap lines: the options
# (127.0.0.1:9050 only, the user, the folder in /run, client only), starting with its percentage, on at 100 %, the health
# log's started, ready, stopped and failed, the stop a minute after the last window (and not when a window asks again),
# stop now, a Tor that ended by itself, no Tor on the system. With the real tor (when installed): it listens on
# 127.0.0.1:9050 and nothing else, as the user given, its files in /run, removed when it stops. Then ci_release's budget and
# the rest of the system layer (the user, the listeners list, the CI's packages). In mount and network namespaces of its
# own (/run is a tmpfs; nothing leaves the computer). Needs root and gcc.   sh tor59_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$TOR59_INNER" ] && exec env TOR59_INNER=1 unshare -m -n sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); REPO=$(cd "$HERE/../../.." && pwd); T=$(mktemp -d /tmp/tor59.XXXXXX)
trap 'pkill -f "$T/" 2>/dev/null; pkill -x tor 2>/dev/null; rm -rf $T' EXIT
ip link set lo up 2>/dev/null
mount --make-rprivate / 2>/dev/null; mount -t tmpfs none /run && mkdir -p /run/rudolf || { echo "FAIL /run"; exit 1; }
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 800)"; F=$((F+1)); fi; }
# the stand-in: its arguments kept; Tor's bootstrap lines into its --Log file, 100 % once $T/go is there
mkdir -p $T/bin; cat > $T/bin/tor <<EOS
#!/bin/sh
printf '%s\n' "\$@" > $T/args; id -u > $T/uid0
while [ \$# -gt 0 ]; do [ "\$1" = --Log ] && L=\${2#notice file }; shift; done
echo "Nov 15 10:00:00.000 [notice] Bootstrapped 0% (starting): Starting" >> "\$L"
echo "Nov 15 10:00:00.000 [notice] Bootstrapped 45% (requesting_descriptors): Asking for relay descriptors" >> "\$L"
while [ ! -e $T/go ]; do timeout 0.2 tail -f /dev/null; done
echo "Nov 15 10:00:01.000 [notice] Bootstrapped 100% (done): Done" >> "\$L"
[ -e $T/die ] && { echo "Nov 15 10:00:02.000 [err] Could not bind to 127.0.0.1:9050: Address already in use" >> "\$L"; exit 1; }
while :; do timeout 1 tail -f /dev/null; done
EOS
chmod 755 $T/bin/tor
build() { gcc -O2 -w -DTOR_BIN="\"$1\"" -DTOR_USER="\"$2\"" -o $3 $HERE/fixtures/torapi.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }; }
build $T/bin/tor nobody $T/t
t() { $T/t "$@"; }
js() { echo "$1" | sed 's/^[^|]*|//' | python3 -c "import json,sys;j=json.load(sys.stdin);print($2)" 2>/dev/null; }
hl() { cat /run/rudolf/health.log 2>/dev/null; }
alive() { [ -e /proc/$1/stat ] && ! awk '{exit $3=="Z"?0:1}' /proc/$1/stat; }
w8() { i=0; while [ $i -lt ${2:-40} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
o=$(t "GET tor/status")
ok "1. tor/status with no Tor window: off" "$([ "$(js "$o" "j['state']")" = off ] && echo 1)" "$o"
o=$(t "GET tor/start")
ok "2. tor/start needs POST" "$(echo "$o" | grep -q ' 405 ' && echo 1)" "$o"
o=$(t "POST tor/start"); w8 '[ -s $T/args ]'
A=$(tr '\n' ' ' < $T/args)
ok "3. tor/start: Tor started for the Tor window: SOCKS on 127.0.0.1:9050 only, as the user tor (here nobody), its folder in /run, client only, nothing written it can avoid, no torrc" "$(echo "$A" | grep -q -- '--SocksPort 127.0.0.1:9050 --DataDirectory /run/rudolf/tor --Log notice file /run/rudolf/tor/log --User nobody --ClientOnly 1 --AvoidDiskWrites 1 --RunAsDaemon 0' && echo "$A" | grep -q -- '--ignore-missing-torrc' && ! echo "$A" | grep -qi 'ORPort\|DNSPort\|ControlPort\|TransPort' && echo 1)" "$A"
ok "...its folder only Tor's (0700, the user's), in memory" "$([ "$(stat -c '%a %U' /run/rudolf/tor)" = '700 nobody' ] && echo 1)" "$(stat -c '%a %U' /run/rudolf/tor)"
w8 '[ "$(js "$(t "GET tor/status")" "j['"'"'pct'"'"']")" = 45 ]'; o=$(t "GET tor/status")
ok "4. while it connects: starting, with Tor's percentage and line" "$([ "$(js "$o" "j['state']")" = starting ] && [ "$(js "$o" "j['pct']")" = 45 ] && js "$o" "j['line']" | grep -q 'Bootstrapped 45%' && echo 1)" "$o"
ok "...the health log: started" "$(hl | grep -q '"k":"tor","m":"Tor started' && echo 1)" "$(hl)"
: > $T/go; w8 '[ "$(js "$(t "GET tor/status")" "j['"'"'state'"'"']")" = on ]'; t tick >/dev/null; o=$(t "GET tor/status")
ok "5. connected (Bootstrapped 100%): on; the health log: ready, once" "$([ "$(js "$o" "j['state']")" = on ] && [ "$(js "$o" "j['pct']")" = 100 ] && t tick >/dev/null && [ "$(hl | grep -c 'Tor is ready')" = 1 ] && echo 1)" "$o $(hl)"
P=$(cat /run/rudolf/tor.pid)
o=$(t "POST tor/start"); ok "6. a second Tor window: the same Tor (not a second one)" "$([ "$(cat /run/rudolf/tor.pid)" = "$P" ] && [ "$(pgrep -fc "$T/bin/tor")" = 1 ] && echo 1)" "$o $(pgrep -fa "$T/bin/tor")"
o=$(t "POST tor/stop" tick "GET tor/status")
ok "7. the last Tor window closed (tor/stop): Tor still runs for a minute, saying it stops" "$(echo "$o" | tail -1 | grep -q '"state":"on".*"stopping":true' && alive $P && echo 1)" "$o"
o=$(t "POST tor/start" "GET tor/status")
ok "...a Tor window again within the minute: it keeps running" "$(echo "$o" | tail -1 | grep -q '"stopping":false' && [ ! -e /run/rudolf/tor.stopat ] && echo 1)" "$o"
t "POST tor/stop" >/dev/null; echo $(( $(date +%s) - 1 )) > /run/rudolf/tor.stopat; t tick >/dev/null
o=$(t "GET tor/status")
ok "8. a minute after: stopped, its folder gone (nothing kept), the health log says so" "$([ "$(js "$o" "j['state']")" = off ] && ! alive $P && [ ! -e /run/rudolf/tor ] && hl | grep -q 'Tor stopped (a minute after the last Tor window closed)' && echo 1)" "$o $(hl | tail -2)"
t "POST tor/start" >/dev/null; w8 '[ -s /run/rudolf/tor.pid ]'; P=$(cat /run/rudolf/tor.pid); o=$(t "POST tor/stop?now=1")
ok "9. tor/stop?now=1: at once" "$([ "$(js "$o" "j['state']")" = off ] && ! alive $P && echo 1)" "$o"
: > $T/die; t "POST tor/start" >/dev/null; w8 '! alive $(cat /run/rudolf/tor.pid 2>/dev/null || echo 1)' 40; timeout 0.5 tail -f /dev/null; t tick >/dev/null; o=$(t "GET tor/status")
ok "10. a Tor that ended by itself: failed, with its log's last line; the health log: failed" "$([ "$(js "$o" "j['state']")" = failed ] && js "$o" "j['line']" | grep -q 'Address already in use' && hl | grep -q 'Tor failed: Could not bind' && [ ! -e /run/rudolf/tor ] && echo 1)" "$o $(hl | tail -1)"
rm -f $T/die; o=$(t "POST tor/start" "GET tor/status"); t "POST tor/stop?now=1" >/dev/null
ok "...started again: no longer failed" "$(echo "$o" | tail -1 | grep -q '"state":"starting\|"state":"on' && echo 1)" "$o"
build /nonexistent/tor nobody $T/t2; o=$($T/t2 "POST tor/start")
ok "11. no Tor on the system: tor/start says so" "$(echo "$o" | grep -q ' 500 .*Tor is not on this system' && echo 1)" "$o"
# ---- the real Tor (Ubuntu's package), when installed: only 127.0.0.1:9050, as the given user, its files in /run
if [ -x /usr/bin/tor ] && command -v ss >/dev/null && id debian-tor >/dev/null 2>&1; then
	build /usr/bin/tor debian-tor $T/t3; o=$($T/t3 "POST tor/start"); w8 'ss -ltn | grep -q 127.0.0.1:9050' 60
	P=$(cat /run/rudolf/tor.pid 2>/dev/null); L=$(ss -ltnup 2>/dev/null | grep "pid=$P," | awk '{print $1, $5}' | sort -u | tr '\n' ' ')
	ok "12. the real Tor: it listens on 127.0.0.1:9050 (SOCKS) and nothing else" "$([ "$L" = 'tcp 127.0.0.1:9050 ' ] && echo 1)" "$o | $L | $(cat /run/rudolf/tor/log 2>/dev/null | tail -3)"
	ok "...as the user given (it drops root), its files only in /run/rudolf/tor" "$([ "$(ps -o user= -p $P)" = debian-tor ] && [ -s /run/rudolf/tor/log ] && echo 1)" "$(ps -o user=,args= -p $P)"
	$T/t3 "POST tor/stop?now=1" >/dev/null
	ok "...stopped: gone, its folder with it" "$(! alive $P && [ ! -e /run/rudolf/tor ] && ! ss -ltn | grep -q 9050 && echo 1)" "$(ls -la /run/rudolf)"
else echo "SKIP 12: no real tor here (apt-get install tor), only the stand-in"; fi
# ---- the system layer: the user, the base layer's budget, the CI's packages, the listeners list, the health log's kinds
ok "13. the user tor (951) and its group, made as ai's (mkrootfs.sh), and added to a base layer that lacks them (ci_release.sh)" "$(grep -q '^tor:x:951:951:' $REPO/iso/mkrootfs.sh && grep -q '^tor:x:951:$' $REPO/iso/mkrootfs.sh && grep -q "tor:x:951:951:" $REPO/iso/systools/ci_release.sh && echo 1)" ""
ok "14. ci_release.sh copies Tor into the base layer with a budget of its own, 6 MB; the CI installs tor" "$(grep -q 'TRB -lt 6291456' $REPO/iso/systools/ci_release.sh && grep -q '/usr/bin/tor' $REPO/iso/systools/ci_release.sh && grep 'apt-get install' $REPO/.github/workflows/rudolf-release.yml | grep -qw tor && echo 1)" ""
ok "15. the listeners list names Tor's 127.0.0.1:9050 (who may use it, how it checks)" "$(grep -q "^tcp	127.0.0.1:9050	tor	" $REPO/iso/sys/root/usr/share/rudolf/listeners && echo 1)" ""
ok "16. the health log knows Tor's events (rudolfd) and System health shows them (sec50.js)" "$(grep -q '"security", "tor", NULL' $REPO/iso/sys/rudolfd.c && grep -q "tor:\['" $REPO/desktop/sec50.js && echo 1)" ""
echo "$((N-F))/$N passed"; [ $F = 0 ]
