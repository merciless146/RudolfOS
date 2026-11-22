#!/bin/sh
# 1.6.2 (docs/plan-1.6.2.md Part 30): the LAN game relay for two-player Library games. The real rudolfd and rudolf-duel,
# built here, in a private mount and network namespace of this test's own ("this computer": never the host's network or
# firewall, port 8080 and 8850 only inside it) with the firewall's real rules (nft), a veth "wlan0" to the other
# player's computer (a namespace) and a veth "wwan0" marked as a cellular modem's (another). rudolfd's requests made as
# the desktop (a process listed in RUDOLFD_DESKTOP_CG, with the token) and as another program; the relay started,
# running as the account's user (not root, no new privileges), found by multicast DNS from the other computer and not
# from the cellular network, the firewall's port open only while it runs, a duel through it (and another version
# refused), stopped when the hosting game leaves and by duel/stop; duel/find through rudolfd; and the fuzz test of its
# parsing (fuzz/duel_fuzz.c, ASan and UBSan).
# Needs root (rudolfd starts the relay as another user), unshare, ip, nft, python3, gcc, nsenter. Ports: 8080 (rudolfd)
# and 8850 (the relay, built in for the test) inside the namespace only.   sh duel162_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys
for c in unshare ip nft python3 gcc nsenter; do command -v $c >/dev/null || { echo "SKIP: needs $c"; exit 0; }; done
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root (rudolfd starts the relay as the account's own user)"; exit 0; }
[ -z "$D162_INNER" ] && exec env D162_INNER=1 unshare -m -n sh "$0" "$@"
T=$(mktemp -d /tmp/duel162.XXXXXX); chmod 755 $T; PIDS=""; RDP=""
cleanup() { for p in $RDP $PIDS; do kill $p 2>/dev/null; done; [ -s /run/rudolf/duel/pid ] && kill "$(cat /run/rudolf/duel/pid)" 2>/dev/null; umount /usr/share/rudolf/www /home/rudolf /persist /usr/lib/rudolf /run 2>/dev/null; rm -rf "$T"; }   # (only what this test started)
trap cleanup EXIT INT TERM
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 1500)"; F=$((F+1)); fi; }
w8() { i=0; while [ $i -lt ${2:-50} ]; do eval "$1" && return 0; timeout 0.1 tail -f /dev/null; i=$((i+1)); done; return 1; }
. $SYS/../systools/hardflags.sh 2>/dev/null
gcc -O2 $HARD -o $T/rudolf-duel $SYS/rudolf-duel.c 2>$T/cc || { echo "FAIL: rudolf-duel does not build: $(cat $T/cc)"; exit 1; }
gcc -O2 -w -DDUEL_BIN="\"$T/rudolf-duel\"" -DDUEL_PORT=8850 -o $T/rudolfd $SYS/rudolfd.c -lz 2>$T/cc || { echo "FAIL: rudolfd does not build: $(cat $T/cc)"; exit 1; }
chmod 755 $T/rudolf-duel

# ---- the fuzz test (no network)
if gcc -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -o $T/duel_fuzz $HERE/fuzz/duel_fuzz.c 2>$T/fcc; then
	o=$($T/duel_fuzz ${DUEL_FUZZ_SECS:-20} 2>&1); ok "1. the relay's parsing: its own announcement read back; loops, pointers and lengths past the end, every cut and byte, the games' JSON and UTF-8, ${DUEL_FUZZ_SECS:-20} s of mutations: no crash, no read past the end, no loop (ASan, UBSan)" "$(echo "$o" | grep -q '^found Test PC' && echo "$o" | grep -q '^ok [0-9]' && echo 1)" "$o"
else ok "1. the fuzz test builds (ASan, UBSan)" 0 "$(cat $T/fcc)"; fi
o=$($T/rudolf-duel serve --port 8851 --code ABC234 --v 0.5.2 --game rudolf-emberreach --name X --addr 127.0.0.1 2>&1; echo "rc=$?")
o2=$(RD_TEST_ROOT=1 $T/rudolf-duel serve --port 80 --code ABC234 --v 0.5.2 --game x --name X --addr 127.0.0.1 2>&1; echo "rc=$?"); o3=$(RD_TEST_ROOT=1 $T/rudolf-duel serve --port 8851 --code ABC234 --v 0.5.2 --game x --name X --addr 8.8.8.8 2>&1; echo "rc=$?")
ok "2. rudolf-duel never runs as root; a port under 1024, or an address off the local network, refused" "$(echo "$o" | grep -q 'never as root' && echo "$o" | grep -q 'rc=2' && echo "$o2$o3" | grep -c 'rc=2' | grep -qx 2 && echo 1)" "$o $o2 $o3"

# ---- this computer: its folders, its network, the firewall
mount -t tmpfs none /run; mkdir -p /run/rudolf /usr/share/rudolf/www /home/rudolf /persist /usr/lib/rudolf
mount --bind $HERE/../../www /usr/share/rudolf/www; mount -t tmpfs none /home/rudolf; mount -t tmpfs none /persist; mount -t tmpfs none /usr/lib/rudolf
cp $SYS/root/usr/lib/rudolf/firewall /usr/lib/rudolf/firewall; chmod 755 /usr/lib/rudolf/firewall
ip link set lo up
unshare -n tail -f /dev/null & PB=$!; PIDS="$PIDS $PB"
unshare -n tail -f /dev/null & PC=$!; PIDS="$PIDS $PC"
w8 "[ \"\$(readlink /proc/$PB/ns/net)\" != \"\$(readlink /proc/self/ns/net)\" ] && [ \"\$(readlink /proc/$PC/ns/net)\" != \"\$(readlink /proc/self/ns/net)\" ]"
B() { nsenter -t $PB -n "$@"; }; C() { nsenter -t $PC -n "$@"; }
ip link add wlan0 type veth peer name pr0 && ip link set pr0 netns $PB && ip link add wwan0 type veth peer name cell0 && ip link set cell0 netns $PC || { echo "SKIP: veth pairs cannot be made here"; exit 0; }
ip addr add 192.168.77.1/24 dev wlan0; ip link set wlan0 up; ip addr add 10.64.0.1/24 dev wwan0; ip link set wwan0 up
B ip link set lo up; B ip addr add 192.168.77.2/24 dev pr0; B ip link set pr0 up
C ip link set lo up; C ip addr add 10.64.0.2/24 dev cell0; C ip link set cell0 up
w8 "ip -o link show wlan0 | grep -q LOWER_UP && ip -o link show wwan0 | grep -q LOWER_UP"
/usr/lib/rudolf/firewall start && /usr/lib/rudolf/firewall cellular add wwan0
IN=$(nft list chain inet rudolf input 2>&1); DU=$(nft list chain inet rudolf duel 2>&1)
ok "3. the firewall: a duel chain, jumped to before mDNS's and the drop, empty while nobody hosts" "$(echo "$IN" | grep -A1 'jump hotspot' | grep -q 'jump duel' && echo "$IN" | grep -A1 'jump duel' | grep -q 'jump mdns' && ! echo "$DU" | grep -q 'accept' && echo 1)" "$IN $DU"
: > /run/rudolf/fw-fail; ok "3b. ...its duel action takes only a port of 1024-65535" "$(/usr/lib/rudolf/firewall duel on 80 2>/dev/null; a=$?; /usr/lib/rudolf/firewall duel on 'x;y' 2>/dev/null; b=$?; [ $a = 2 ] && [ $b = 2 ] && ! nft list chain inet rudolf duel | grep -q accept && echo 1)"

# ---- rudolfd: the desktop (a process of its memory group, with the token) and another program
mkdir -p $T/cg; : > $T/cg/cgroup.procs
cat > $T/cl.py <<'EOF'
import os, sys, time, urllib.request, urllib.error
name, method, path, d = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
os.setgid(65534); os.setuid(65534)
open(d + '/pid.' + name, 'w').write(str(os.getpid()))
while not os.path.exists(d + '/go.' + name): time.sleep(.05)
tok = open(d + '/tok').read().strip() if os.path.exists(d + '/tok') else ''
q = urllib.request.Request('http://127.0.0.1:8080' + path, method=method, data=b'' if method == 'POST' else None, headers={'X-Rudolf-Token': tok})
try: r = urllib.request.urlopen(q, timeout=20); print(r.status, r.read().decode('utf8', 'replace')[:4000])
except urllib.error.HTTPError as e: print(e.code, e.read().decode('utf8', 'replace')[:2000])
EOF
chmod 644 $T/cl.py; chmod 777 $T
run() { rm -f $T/pid.$1 $T/go.$1; python3 $T/cl.py $1 $2 "$3" $T > $T/out.$1 2>&1 & w8 "[ -s $T/pid.$1 ]" 40
  [ "$4" = desk ] && echo "$(cat $T/pid.$1)" >> $T/cg/cgroup.procs; touch $T/go.$1; wait $!; cat $T/out.$1; }
code() { echo "$1" | head -1 | cut -d' ' -f1; }
env RUDOLFD_DESKTOP_CG=$T/cg RUDOLFD_DESKTOP_UID=65534 $T/rudolfd > $T/rd.log 2>&1 & RDP=$!
w8 'python3 -c "import socket; socket.create_connection((\"127.0.0.1\", 8080), 1)" 2>/dev/null' 80 || { echo "FAIL: rudolfd did not start: $(tail -5 $T/rd.log)"; exit 1; }
o=$(run d0 GET / desk); echo "$o" | sed -n 's/.*RUDOLFD={token:"\([0-9a-f]*\)".*/\1/p' | head -1 > $T/tok; chmod 644 $T/tok
mv $T/tok $T/tok.k; o=$(run x1 POST "/api/duel/host?v=0.5.2&game=rudolf-emberreach" desk); mv $T/tok.k $T/tok
o2=$(run x2 POST "/api/duel/host?v=0.5.2&game=rudolf-emberreach" no)
ok "4. duel/host refused without the token (the desktop's own process), and to another program even with it" "$([ "$(code "$o")" = 403 ] && [ "$(code "$o2")" = 403 ] && [ ! -e /run/rudolf/duel/pid ] && ! nft list chain inet rudolf duel | grep -q accept && echo 1)" "$o / $o2"
o=$(run d1 POST "/api/duel/host?v=0.5.2&game=rudolf-emberreach" desk)
J=$(echo "$o" | cut -d' ' -f2-); CODE=$(echo "$J" | sed -n 's/.*"code":"\([A-Z0-9]*\)".*/\1/p')
ok "5. the desktop asks: the relay started for that game and version; its address on the home network (not the cellular one), a room code, the computer's name" "$([ "$(code "$o")" = 200 ] && echo "$J" | grep -q '"url":"ws://192.168.77.1:8850/"' && [ ${#CODE} = 6 ] && echo "$J" | grep -q '"name":"' && echo 1)" "$o $(tail -3 $T/rd.log)"
RP=$(cat /run/rudolf/duel/pid 2>/dev/null); ST=$(cat /proc/$RP/status 2>/dev/null)
U=$(echo "$ST" | awk '/^Uid:/{print $2" "$3" "$4" "$5}'); NNP=$(echo "$ST" | awk '/^NoNewPrivs:/{print $2}'); G=$(echo "$ST" | awk '/^Groups:/{print $2}')
ok "6. ...running as the account's own user (here the desktop's, nobody signed in: uid 1000), never root, no new privileges, no extra groups" "$([ -n "$RP" ] && [ "$(cat /proc/$RP/comm)" = rudolf-duel ] && [ "$U" = "1000 1000 1000 1000" ] && [ "$NNP" = 1 ] && [ -z "$G" ] && echo 1)" "pid=$RP uid=$U nnp=$NNP groups=$G"
DU=$(nft list chain inet rudolf duel)
ok "7. ...the firewall's duel chain open for its port, from private addresses, never on the cellular interface" "$(echo "$DU" | grep -q 'iifname @cellular return' && echo "$DU" | grep 'tcp dport 8850 accept' | grep -q 'ip saddr {.*10.0.0.0/8' && for a in 172.16.0.0/12 192.168.0.0/16 169.254.0.0/16; do echo "$DU" | grep 'tcp dport 8850' | grep -q "$a" || exit; done && [ "$(echo "$DU" | grep -c accept)" = 1 ] && echo 1)" "$DU"
fb=$(B env RD_CELLULAR=/dev/null RD_TEST_ROOT=1 $T/rudolf-duel find --game rudolf-emberreach --ms 2500); fo=$(B env RD_CELLULAR=/dev/null $T/rudolf-duel find --game another-game --ms 1200)
ok "8. found by multicast DNS from the other computer: its name, ws://192.168.77.1:8850/, the room code and the version (and not for another game)" "$(echo "$fb" | grep -q "\"url\":\"ws://192.168.77.1:8850/\",\"code\":\"$CODE\",\"v\":\"0.5.2\",\"game\":\"rudolf-emberreach\"" && echo "$fo" | grep -q '"games":\[\]' && echo 1)" "$fb / $fo"
fc=$(C env RD_CELLULAR=/dev/null $T/rudolf-duel find --ms 1500)
tb=$(B python3 -c "import socket; socket.create_connection(('192.168.77.1', 8850), 2); print('open')" 2>&1); tc=$(C python3 -c "import socket; socket.create_connection(('10.64.0.1', 8850), 2); print('open')" 2>&1)
ok "9. ...not from the cellular network: no answer there, and its port shut to it; open to the home network" "$(echo "$fc" | grep -q '"games":\[\]' && [ "$tb" = open ] && [ "$tc" != open ] && echo 1)" "$fc / $tb / $tc"
# a duel through it: the hosting game here (at the address the desktop gave it), the other from the other computer
python3 $HERE/fixtures/wsduel.py ws://192.168.77.1:8850/ host - 0.5.2 $T/h '{"t":"st","r":1,"x":"host"}' & PH=$!; PIDS="$PIDS $PH"
w8 "grep -q '\"t\":\"room\"' $T/h 2>/dev/null"
B python3 $HERE/fixtures/wsduel.py ws://192.168.77.1:8850/ join "$CODE" 0.5.1 $T/old & PO=$!; PIDS="$PIDS $PO"
w8 "grep -q 'error' $T/old 2>/dev/null"
B python3 $HERE/fixtures/wsduel.py ws://192.168.77.1:8850/ join "$(echo $CODE | tr A-Z a-z)" 0.5.2 $T/g '{"t":"st","r":1,"x":"guest"}' & PG=$!; PIDS="$PIDS $PG"
w8 "grep -q '\"x\":\"host\"' $T/g 2>/dev/null && grep -q '\"x\":\"guest\"' $T/h 2>/dev/null"
ok "10. a duel through it: the host's room has the code; another version refused with a message; the same version joins (the code in any case), and each one's messages reach the other" "$(grep -q "\"code\":\"$CODE\"" $T/h && grep -q '"why":"version"' $T/old && grep -q 'same version' $T/old && grep -q '"role":"host"' $T/h && grep -q '"role":"guest"' $T/g && grep -q '"x":"host"' $T/g && grep -q '"x":"guest"' $T/h && echo 1)" "$(cat $T/h $T/old $T/g 2>&1 | head -20)"
ok "10b. ...and nothing the relay sends carries an address" "$(! grep -q '192\.168\|10\.64' $T/h $T/g && echo 1)" "$(cat $T/h $T/g)"
# the hosting game leaves: the relay stops by itself, and the port is shut again
touch $T/h.stop; w8 "[ ! -e /proc/$RP ] || [ \"\$(cat /proc/$RP/comm 2>/dev/null)\" != rudolf-duel ]" 50
w8 "! nft list chain inet rudolf duel | grep -q accept" 30
o=$(run d2 GET /api/duel/status desk)
ok "11. the hosting game leaves: the relay stops by itself, the other game is told, the firewall's port is shut again, and status says so" "$([ ! -e /proc/$RP ] && ! nft list chain inet rudolf duel | grep -q accept && grep -q '"t":"gone"' $T/g && echo "$o" | grep -q '"running":false' && [ ! -e /run/rudolf/duel/pid ] && echo 1)" "$(nft list chain inet rudolf duel) $o $(cat $T/g | tail -3)"
fb2=$(B env RD_CELLULAR=/dev/null $T/rudolf-duel find --ms 1200)
ok "12. ...and it is no longer found on the network" "$(echo "$fb2" | grep -q '"games":\[\]' && echo 1)" "$fb2"
o=$(run d3 POST "/api/duel/host?v=0.5.2&game=rudolf-emberreach" desk); RP2=$(cat /run/rudolf/duel/pid 2>/dev/null)
o2=$(run d4 POST /api/duel/stop desk); w8 "[ ! -e /proc/$RP2 ]" 30
ok "13. hosted again, then duel/stop (the game's lanStop): stopped, the port shut" "$([ "$(code "$o")" = 200 ] && [ -n "$RP2" ] && [ "$(code "$o2")" = 200 ] && [ ! -e /proc/$RP2 ] && ! nft list chain inet rudolf duel | grep -q accept && echo 1)" "$o / $o2 / $(nft list chain inet rudolf duel)"
o=$(run d5 POST "/api/duel/host?v=0.5.2&game=rudolf-emberreach" desk); RP3=$(cat /run/rudolf/duel/pid 2>/dev/null)
w8 "[ \$(date +%s) -ge $(( $(date +%s) + 75 )) ] || [ ! -e /proc/$RP3 ]" 800
ok "14. hosted, but no game connects within a minute: it stops by itself, the port shut" "$([ -n "$RP3" ] && [ ! -e /proc/$RP3 ] && ! nft list chain inet rudolf duel | grep -q accept && echo 1)" "$RP3 $(nft list chain inet rudolf duel)"
# duel/find through rudolfd: a game hosted on the other computer
B env RD_CELLULAR=/dev/null RD_TEST_ROOT=1 $T/rudolf-duel serve --port 8852 --code XYZ789 --v 0.5.2 --game rudolf-emberreach --name "Other PC" --addr 192.168.77.2 --wait 30 > $T/b.log 2>&1 & PBS=$!; PIDS="$PIDS $PBS"
w8 "grep -q ok $T/b.log"
o=$(run d6 GET "/api/duel/find?game=rudolf-emberreach" desk); o2=$(run x3 GET "/api/duel/find?game=rudolf-emberreach" no)
ok "15. duel/find (as the account's user): the other computer's game, its address, code and version; refused to another program" "$([ "$(code "$o")" = 200 ] && echo "$o" | grep -q '"name":"Other PC","url":"ws://192.168.77.2:8852/","code":"XYZ789","v":"0.5.2"' && [ "$(code "$o2")" = 403 ] && echo 1)" "$o / $o2"
o=$(run d7 POST "/api/duel/host?v=%3Cb%3E&game=rudolf-emberreach" desk); o2=$(run d8 POST "/api/duel/host?v=0.5.2" desk); o3=$(run d9 GET "/api/duel/host?v=0.5.2&game=x" desk); o4=$(run d10 GET "/api/duel/find?game=..%2Fx" desk)
ok "16. a version or package that is not one, a missing package, a GET to host: refused" "$([ "$(code "$o")" = 400 ] && [ "$(code "$o2")" = 400 ] && [ "$(code "$o3")" = 405 ] && [ "$(code "$o4")" = 400 ] && echo 1)" "$o / $o2 / $o3 / $o4"
echo "$((N-F))/$N passed"; [ $F = 0 ]
