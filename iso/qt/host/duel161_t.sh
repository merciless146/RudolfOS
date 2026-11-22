#!/bin/sh
# 1.6.1: duels on the local network for the Library's games (Ember Reach 0.5.2): the duel service (rudolf-duel), the
# firewall's hole for it, its multicast DNS, and rudolfd's duel requests.
# Part 1, in network namespaces of this test's own (never the host's network or firewall): this computer's with
# "wlan0" (a veth to a home network, B: 192.168.77.0/24), "wwan0" (a veth marked as a cellular modem's, C, whose
# addresses are private too: an operator's) and "eth1" (a veth to the internet, D: 203.0.113.0/24); the firewall's real
# rules loaded with nft; rudolf-duel as built from this tree, run as nobody (never root).
# Part 2, the real rudolfd built here, in private mount and network namespaces (so port 8080 is its own): duel/host,
# duel, duel/find and duel/stop through the API (refused without the token, or to a program that is not the desktop),
# the service started as a user that is not root, the firewall's hole opened (a stand-in firewall that says how it was
# called) and closed again when the service ends, by itself or stopped.
# Needs unshare, ip (iproute2), nft, nsenter, setpriv, python3, gcc, zlib, curl. As root (CI: sudo). Only processes
# this test started are stopped.
#   sh duel161_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; WS=$HERE/fixtures/duelws.py
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
for c in unshare ip nft python3 gcc nsenter setpriv curl; do command -v $c >/dev/null || { echo "SKIP: needs $c"; exit 0; }; done
if [ -z "$DUEL_INNER" ]; then exec env DUEL_INNER=1 unshare -n -m sh "$0" "$@"; fi
T=$(mktemp -d /tmp/duel161.XXXXXX); chmod 755 $T; PIDS=""
cleanup() { for p in $PIDS; do kill $p 2>/dev/null; done; [ -n "$RDP" ] && kill $RDP 2>/dev/null; umount /usr/share/rudolf/www /home/rudolf /persist /run 2>/dev/null; rm -rf "$T"; }   # (only what this test started)
trap cleanup EXIT INT TERM
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 1500)"; F=$((F+1)); fi; }
w8() { i=0; while [ $i -lt ${2:-50} ]; do eval "$1" && return 0; timeout 0.1 tail -f /dev/null; i=$((i+1)); done; return 1; }
. $SYS/../systools/hardflags.sh 2>/dev/null
gcc -O2 $HARD -o $T/rudolf-duel $SYS/rudolf-duel.c 2>$T/cc || { echo "FAIL: rudolf-duel does not build: $(cat $T/cc)"; exit 1; }
chmod 755 $T/rudolf-duel; cp $WS $T/ws.py; chmod 755 $T/ws.py
uid() { awk '/^Uid:/{print $2}' /proc/$1/status 2>/dev/null; }   # (its real user: /proc/PID's owner is root's once a process changes user)
NB() { setpriv --reuid=65534 --regid=65534 --clear-groups "$@"; }   # (as nobody)

# ---- part 1: the networks
ip link set lo up
for n in B C D; do unshare -n tail -f /dev/null & eval P$n=$!; PIDS="$PIDS $!"; done
w8 "[ \"\$(readlink /proc/$PB/ns/net)\" != \"\$(readlink /proc/self/ns/net)\" ] && [ \"\$(readlink /proc/$PD/ns/net)\" != \"\$(readlink /proc/self/ns/net)\" ]"
B() { nsenter -t $PB -n "$@"; }; C() { nsenter -t $PC -n "$@"; }; D() { nsenter -t $PD -n "$@"; }
ip link add wlan0 type veth peer name pr0 && ip link set pr0 netns $PB && ip link add wwan0 type veth peer name cell0 && ip link set cell0 netns $PC && ip link add eth1 type veth peer name net0 && ip link set net0 netns $PD || { echo "SKIP: veth pairs cannot be made here"; exit 0; }
ip addr add 192.168.77.1/24 dev wlan0; ip link set wlan0 up; ip addr add 10.64.0.1/24 dev wwan0; ip link set wwan0 up; ip addr add 203.0.113.1/24 dev eth1; ip link set eth1 up
B ip link set lo up; B ip addr add 192.168.77.2/24 dev pr0; B ip link set pr0 up
C ip link set lo up; C ip addr add 10.64.0.2/24 dev cell0; C ip link set cell0 up
D ip link set lo up; D ip addr add 203.0.113.2/24 dev net0; D ip link set net0 up
for x in wlan0 wwan0 eth1; do w8 "ip -o link show $x | grep -q LOWER_UP"; done

# ---- the firewall: its real rules (its paths moved into this test's folder), wwan0 a cellular modem's
mkdir -p $T/p $T/run; sed -e "s#/persist/etc#$T/p#g" -e "s#/run/rudolf#$T/run#g" $SYS/root/usr/lib/rudolf/firewall > $T/fw; chmod +x $T/fw
$T/fw start && $T/fw cellular add wwan0
IN=$(nft list chain inet rudolf input); DU=$(nft list chain inet rudolf duel)
ok "1. the firewall: a duel chain, jumped to before mDNS's and the final drop, and empty while nothing is hosted" "$(echo "$IN" | grep -A1 'jump duel' | grep -q 'jump mdns' && echo "$IN" | grep -A1 'jump hotspot' | grep -q 'jump duel' && [ "$(echo "$DU" | grep -c 'accept\|return')" = 0 ] && echo 1)" "$IN $DU"
$T/fw duel 8080 2>/dev/null; r1=$?; $T/fw duel 47870 2>/dev/null; r2=$?; $T/fw duel '47861;x' 2>/dev/null; r3=$?
ok "2. ...it opens only a duel's port (47860-47869): 8080, 47870 or anything else refused" "$([ $r1$r2$r3 = 222 ] && [ ! -e $T/run/firewall-duel ] && echo 1)" "$r1$r2$r3"

# ---- the service, as nobody
$T/rudolf-duel serve --port 47861 --code HANDUE --ver 0.5.2 --name testpc > $T/asroot 2>&1; rr=$?
ok "3. the service never runs as root" "$([ $rr = 2 ] && grep -q 'not as root' $T/asroot && echo 1)" "$rr $(cat $T/asroot)"
NB env RDUEL_CELLULAR=$T/run/firewall-cellular $T/rudolf-duel serve --port 47861 --code HANDUE --ver 0.5.2 --name 'Den PC' > $T/serve.log 2>&1 & SV=$!; PIDS="$PIDS $SV"
w8 "grep -q hosting $T/serve.log"; SV=$(pgrep -f "rudolf-duel serve --port 47861" | head -1)   # (the service itself: NB's process is a shell's)
ok "4. ...as nobody it hosts, announced on the home network only (not the cellular link)" "$(grep -q 'hosting HANDUE on port 47861 (2 interfaces announced)' $T/serve.log && [ "$(uid $SV)" = 65534 ] && echo 1)" "$(cat $T/serve.log) $(uid $SV)"
# (2: wlan0 and eth1; wwan0 left out: a cellular one)
fb=$(B env RDUEL_CELLULAR=$T/none setpriv --reuid=65534 --regid=65534 --clear-groups $T/rudolf-duel find); fc=$(C env RDUEL_CELLULAR=$T/none setpriv --reuid=65534 --regid=65534 --clear-groups $T/rudolf-duel find)
ok "5. found by mDNS from the home network: its name, address, port, code, version and the address to join" "$(echo "$fb" | grep -q '"games":\[{"name":"Den PC","addr":"192.168.77.1","port":47861,"url":"ws://192.168.77.1:47861/","code":"HANDUE","ver":"0.5.2"}\]' && echo 1)" "$fb"
ok "...and not over the cellular link (not announced there, nor answered)" "$(echo "$fc" | grep -q '"ok":true,"games":\[\]' && echo 1)" "$fc"
pb0=$(B python3 $T/ws.py probe 192.168.77.1 47861); pc0=$(C python3 $T/ws.py probe 10.64.0.1 47861); pd0=$(D python3 $T/ws.py probe 203.0.113.1 47861)
ok "6. while the firewall has no hole for it, nobody else reaches it (the home network, the cellular link, the internet)" "$([ "$pb0$pc0$pd0" = blockedblockedblocked ] && echo 1)" "$pb0 $pc0 $pd0"
$T/fw duel 47861; DU=$(nft list chain inet rudolf duel)
pb=$(B python3 $T/ws.py probe 192.168.77.1 47861); pc=$(C python3 $T/ws.py probe 10.64.0.1 47861); pd=$(D python3 $T/ws.py probe 203.0.113.1 47861)
ok "7. hosting: the hole lets the home network in, and nothing else: not the cellular link (private addresses, but not this home's), not the internet" "$([ "$pb" = open ] && [ "$pc" = blocked ] && [ "$pd" = blocked ] && echo "$DU" | grep -q 'iifname @cellular return' && echo "$DU" | grep 'tcp dport 47861 accept' | grep 'ip saddr {' | grep '10.0.0.0/8' | grep '172.16.0.0/12' | grep '192.168.0.0/16' | grep -q '169.254.0.0/16' && [ "$(echo "$DU" | grep -c 'accept')" = 1 ] && [ "$(cat $T/run/firewall-duel)" = 47861 ] && echo 1)" "$pb $pc $pd $DU"
# a match: someone on the network can't take the host's place; the host (this computer) hosts, the other joins
: > $T/hb.log; B python3 $T/ws.py host 192.168.77.1 47861 0.5.2 $T/hb.log $T/stop; python3 $T/ws.py host 127.0.0.1 47861 0.5.2 $T/h.log $T/stop & HP=$!; PIDS="$PIDS $HP"
w8 "grep -qs room $T/h.log"
B python3 $T/ws.py join 192.168.77.1 47861 0.5.1 HANDUE $T/jv.log; B python3 $T/ws.py join 192.168.77.1 47861 0.5.2 ABCDEF $T/jx.log
ok "8. only the game on this computer hosts (one from the network is turned away), and it gets the room's code; a game of another version, or with another code, can't join" "$(grep -q '"why":"busy"' $T/hb.log && grep -q '{"t":"room","code":"HANDUE"}' $T/h.log && grep -q '"why":"version".*"ver":"0.5.2"' $T/jv.log && grep -q '"why":"room"' $T/jx.log && echo 1)" "$(cat $T/hb.log $T/h.log $T/jv.log $T/jx.log)"
B python3 $T/ws.py join 192.168.77.1 47861 0.5.2 HANDUE $T/j.log
w8 "grep -q left $T/h.log" 40
ok "9. the other joins from the home network: both paired, their game's messages pass both ways, and when it leaves the host is told" "$(grep -q '"role":"host"' $T/h.log && grep -q '"role":"guest"' $T/j.log && grep -q '{"t":"g","k":"hi"}' $T/h.log && grep -q '{"t":"g","k":"st"}' $T/j.log && grep -q '{"t":"left"}' $T/h.log && echo 1)" "$(cat $T/h.log; echo ---; cat $T/j.log)"
ok "...nothing either heard holds the other's address" "$(! grep -q '192.168.77\|127.0.0.1' $T/h.log $T/j.log && echo 1)" "$(cat $T/h.log $T/j.log)"
w8 "! kill -0 $SV 2>/dev/null" 40
ok "10. the match over: the service ends by itself" "$(! kill -0 $SV 2>/dev/null && grep -q 'ended (the match is over)' $T/serve.log && echo 1)" "$(cat $T/serve.log)"
$T/fw duel off; DU=$(nft list chain inet rudolf duel); pb2=$(B python3 $T/ws.py probe 192.168.77.1 47861)
ok "11. ...and the hole closed (rudolfd's keeper does this): the chain empty again, nothing reachable" "$([ "$(echo "$DU" | grep -c 'accept\|return')" = 0 ] && [ "$pb2" = blocked ] && [ ! -e $T/run/firewall-duel ] && echo 1)" "$pb2 $DU"
$T/fw duel 47862; $T/fw off; $T/fw on; DU=$(nft list chain inet rudolf duel); $T/fw duel off
ok "12. a hole while hosting stays through the firewall turned off and on again (and goes with off)" "$(echo "$DU" | grep -q 'tcp dport 47862 accept' && echo 1)" "$DU"
# nobody hosting within a minute: it ends by itself (faster here: the host's game connects and goes at once)
NB $T/rudolf-duel serve --port 47863 --code HANDUE --ver 0.5.2 --name x > $T/s2.log 2>&1 & S2=$!; PIDS="$PIDS $S2"
w8 "grep -q hosting $T/s2.log"; touch $T/stop2; python3 $T/ws.py host 127.0.0.1 47863 0.5.2 $T/h2.log $T/stop2; w8 "! kill -0 $S2 2>/dev/null" 40
ok "13. the host's game gone before anyone joined: the service ends too" "$(! kill -0 $S2 2>/dev/null && grep -q 'ended (the host left)' $T/s2.log && echo 1)" "$(cat $T/s2.log)"

# ---- part 2: rudolfd's requests (its own mount namespace: /run, /persist, /home/rudolf and the page's files; this
# network namespace: its port 8080 is its own)
mount -t tmpfs none /run; mkdir -p /run/rudolf /usr/share/rudolf/www /home/rudolf /persist
mount --bind $HERE/../../www /usr/share/rudolf/www; mount -t tmpfs none /home/rudolf; mount -t tmpfs none /persist
printf '#!/bin/sh\necho "$*" >> %s/fwcalls\n' $T > $T/fwstub; chmod 755 $T/fwstub
gcc -O2 -w -DDUEL_BIN="\"$T/rudolf-duel\"" -DDUEL_FW="\"$T/fwstub\"" -o $T/rudolfd $SYS/rudolfd.c -lz || { echo "FAIL build rudolfd"; exit 1; }
# (the desktop's memory group: a stand-in process of its own listed, so the peer check is enforced)
mkdir -p $T/cg; tail -f /dev/null & DK=$!; PIDS="$PIDS $DK"; echo $DK > $T/cg/cgroup.procs
env RUDOLFD_DESKTOP_CG=$T/cg RUDOLFD_DESKTOP_UID=65534 $T/rudolfd > $T/rd.log 2>&1 & RDP=$!
w8 'curl -s http://127.0.0.1:8080/favicon.png -o /dev/null' 80
TOK=$(curl -s http://127.0.0.1:8080/ | sed -n 's/.*RUDOLFD={token:"\([0-9a-f]*\)".*/\1/p' | head -1)
api() { curl -s -o $T/body -w '%{http_code}' -X "$1" ${3:+-H "X-Rudolf-Token: $3"} "http://127.0.0.1:8080/api/$2"; }
c1=$(api POST 'duel/host?ver=0.5.2'); b1=$(cat $T/body); c2=$(NB curl -s -o /dev/null -w '%{http_code}' -X POST -H "X-Rudolf-Token: $TOK" 'http://127.0.0.1:8080/api/duel/host?ver=0.5.2')
ok "14. duel/host is refused without rudolfd's token, and to a program that is not the desktop even with it (the peer check): nothing started" "$([ ${#TOK} = 32 ] && [ "$c1" = 403 -o "$c1" = 401 ] && [ "$c2" = 403 ] && ! pgrep -f "$T/rudolf-duel serve" >/dev/null && [ ! -s $T/fwcalls ] && echo 1)" "$c1 $c2 $b1"
c3=$(api POST 'duel/host?ver=0.5.2;x' $TOK); c4=$(api GET 'duel/host?ver=0.5.2' $TOK)
ok "15. ...a version that is not one, or a GET: refused" "$([ "$c3" = 400 ] && [ "$c4" = 405 ] && echo 1)" "$c3 $c4"
c=$(api POST 'duel/host?ver=0.5.2' $TOK); H=$(cat $T/body); SP=$(pgrep -f "$T/rudolf-duel serve" | head -1); PORT=$(echo "$H" | sed -n 's/.*"port":\([0-9]*\).*/\1/p'); CODE=$(echo "$H" | sed -n 's/.*"code":"\([A-Z0-9]*\)".*/\1/p')
ok "16. with the token (root here stands for the desktop): the service started for one match, on a duel port, with a new code, the address the host's game joins" "$([ "$c" = 200 ] && echo "$H" | grep -q '"ok":true,"url":"ws://127.0.0.1:4786[0-9]/","code":"[A-HJKMNP-Z2-9]\{6\}","port":4786[0-9]' && [ -n "$SP" ] && echo 1)" "$c $H"
ok "...as a user that is not root (the account's own; here the desktop's, 1000), in a session of its own" "$([ -n "$SP" ] && [ "$(uid $SP)" = 1000 ] && [ "$(ps -o sid= -p $SP | tr -d ' ')" != "$(ps -o sid= -p $RDP | tr -d ' ')" ] && echo 1)" "$(uid $SP) $(ps -o sid= -p $SP) $(ps -o sid= -p $RDP)"
ok "...and the firewall's hole opened for its port" "$([ "$(cat $T/fwcalls)" = "duel $PORT" ] && echo 1)" "$(cat $T/fwcalls)"
c=$(api GET duel $TOK); s1=$(cat $T/body)
ok "17. GET duel: hosting, its port and code" "$([ "$c" = 200 ] && [ "$s1" = "{\"ok\":true,\"hosting\":true,\"port\":$PORT,\"code\":\"$CODE\"}" ] && echo 1)" "$c $s1"
: > $T/h3.log; python3 $T/ws.py host 127.0.0.1 $PORT 0.5.2 $T/h3.log $T/stop3 & HP3=$!; PIDS="$PIDS $HP3"; w8 "grep -q room $T/h3.log"
ok "18. the host's game joins it on this computer and gets the code rudolfd gave" "$(grep -q "{\"t\":\"room\",\"code\":\"$CODE\"}" $T/h3.log && echo 1)" "$(cat $T/h3.log)"
c=$(api POST duel/stop $TOK); w8 "! kill -0 $SP 2>/dev/null" 40
ok "19. duel/stop: the service ended, the hole closed again, nothing hosted" "$([ "$c" = 200 ] && ! kill -0 $SP 2>/dev/null && [ "$(tail -1 $T/fwcalls)" = "duel off" ] && [ ! -e /run/rudolf/duel ] && api GET duel $TOK >/dev/null && grep -q '"hosting":false' $T/body && echo 1)" "$c $(cat $T/fwcalls) $(cat $T/body)"
: > $T/fwcalls; c=$(api POST 'duel/host?ver=0.5.2' $TOK); PORT=$(sed -n 's/.*"port":\([0-9]*\).*/\1/p' $T/body); SP=$(pgrep -f "$T/rudolf-duel serve" | head -1)
: > $T/h4.log; touch $T/stop4; python3 $T/ws.py host 127.0.0.1 $PORT 0.5.2 $T/h4.log $T/stop4; w8 "! kill -0 $SP 2>/dev/null" 40; w8 "grep -q 'duel off' $T/fwcalls" 40
ok "20. a match that ends by itself (the host's game gone): the service ends, and rudolfd's keeper closes the hole (no request needed)" "$([ "$c" = 200 ] && ! kill -0 $SP 2>/dev/null && [ "$(cat $T/fwcalls | tr '\n' '#')" = "duel $PORT#duel off#" ] && [ ! -e /run/rudolf/duel ] && echo 1)" "$c $(cat $T/fwcalls)"
c=$(api GET duel/find $TOK); fd=$(cat $T/body)
ok "21. duel/find: the search's answer (here no network but this computer's own: none found)" "$([ "$c" = 200 ] && [ "$fd" = '{"ok":true,"games":[]}' ] && echo 1)" "$c $fd"
H=$(cat /run/rudolf/health.log 2>/dev/null)
ok "22. the health log says when the port was opened to the local network and closed again" "$(echo "$H" | grep -q 'opened to the local network for a game.s duel' && echo "$H" | grep -q "the duel's port $PORT closed again" && echo 1)" "$H"
kill $RDP 2>/dev/null; wait $RDP 2>/dev/null; RDP=""
echo "$((N-F))/$N passed"; [ $F = 0 ]
