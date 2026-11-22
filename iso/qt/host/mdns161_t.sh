#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 9): Find printers with the firewall on. Found on the owner's Vivobook (1.6.0): find
# asked from an ephemeral port and the firewall dropped every answer (a multicast one was not let in; a unicast one
# from the printer's address is not a reply to conntrack), so no printer was found; the tests had pointed RP_MDNS at a
# stand-in by unicast. Here, in network namespaces of this test's own (never the host's network or firewall): this
# computer's namespace with "wlan0" (a veth to a printers' network) and "wwan0" (a veth marked as a cellular modem's);
# the firewall's real rules loaded with nft; stand-in printers (fixtures/mdnsnet.py) that answer by multicast and by
# unicast; rudolf-print find as built from this tree. And the fuzz test of its mDNS parsing (fuzz/mdns_fuzz.c).
# Needs unshare, ip (iproute2), nft, python3, gcc. As root (CI: sudo) or, where user namespaces are allowed, as anyone.
#   sh mdns161_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; FX=$HERE/fixtures/mdnsnet.py
for c in unshare ip nft python3 gcc nsenter; do command -v $c >/dev/null || { echo "SKIP: needs $c"; exit 0; }; done
if [ -z "$M161_INNER" ]; then
	if [ "$(id -u)" = 0 ]; then exec env M161_INNER=1 unshare -n sh "$0" "$@"; else exec env M161_INNER=1 unshare -rn sh "$0" "$@"; fi
fi
T=$(mktemp -d); PIDS=""
cleanup() { for p in $PIDS; do kill $p 2>/dev/null; done; rm -rf "$T"; }   # (only what this test started)
trap cleanup EXIT INT TERM
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 1500)"; F=$((F+1)); fi; }
w8() { i=0; while [ $i -lt 50 ]; do eval "$1" && return 0; timeout 0.1 tail -f /dev/null; i=$((i+1)); done; return 1; }
. $SYS/../systools/hardflags.sh 2>/dev/null
gcc -O2 $HARD -o $T/rudolf-print $SYS/rudolf-print.c -ldl 2>$T/cc || { echo "FAIL: rudolf-print does not build: $(cat $T/cc)"; exit 1; }

# ---- the fuzz test (any user; no network)
if gcc -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -o $T/mdns_fuzz $HERE/fuzz/mdns_fuzz.c -ldl 2>$T/fcc; then
	o=$($T/mdns_fuzz ${MDNS_FUZZ_SECS:-20} 2>&1); ok "1. parse_mdns: a real answer read; loops, pointers and lengths past the end, every cut and byte, ${MDNS_FUZZ_SECS:-20} s of mutations: no crash, no read past the end, no loop (ASan, UBSan)" "$(echo "$o" | grep -q '^found Office Printer' && echo "$o" | grep -q '^ok [0-9]' && echo 1)" "$o"
else ok "1. the fuzz test builds (ASan, UBSan)" 0 "$(cat $T/fcc)"; fi

# ---- the networks: B the printers' (veth pr0), C a cellular network's (veth cell0)
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

# ---- the firewall: its real rules (its paths moved into this test's folder), wwan0 a cellular modem's
mkdir -p $T/p $T/run; sed -e "s#/persist/etc#$T/p#g" -e "s#/run/rudolf#$T/run#g" $SYS/root/usr/lib/rudolf/firewall > $T/fw; chmod +x $T/fw
$T/fw start && $T/fw cellular add wwan0
ok "2. the firewall's rules load (nft) in this namespace, wwan0 in its cellular set" "$(nft list table inet rudolf >/dev/null 2>&1 && nft list set inet rudolf cellular | grep -q '"wwan0"' && echo 1)" "$(nft list ruleset 2>&1 | head -40)"
IN=$(nft list chain inet rudolf input); MD=$(nft list chain inet rudolf mdns)
ok "3. ...the input chain as before with only the mdns chain's jump added, before the drop; mdns: not on a cellular interface, then only 5353 from 5353 to the group, or from the same link (TTL 255, private addresses)" "$(echo "$IN" | grep -q 'iif "lo" accept' && echo "$IN" | grep -q 'ct state established,related accept' && echo "$IN" | grep -q 'udp sport 67 udp dport 68 accept' && echo "$IN" | grep -q 'jump remote' && echo "$IN" | grep -q 'jump hotspot' && [ "$(echo "$IN" | grep -c ' accept$')" = 6 ] && echo "$IN" | grep -A1 'jump mdns' | grep -q 'counter name "blocked" drop' && [ "$(echo "$MD" | grep -c ' accept$')" = 2 ] && echo "$MD" | grep -A1 'iifname @cellular return' | grep -q 'ip daddr 224.0.0.251 udp sport 5353 udp dport 5353 accept' && echo "$MD" | grep -q 'ip ttl 255 udp sport 5353 udp dport 5353 accept' && ! echo "$IN$MD" | grep -qE 'udp sport 5353 accept|udp sport 5353 udp dport != ' && echo 1)" "$IN $MD"
blocked() { $T/fw status | awk '{print $2}'; }

# ---- the stand-in printers: one answers by multicast, one by unicast (to the question's source); a cellular one
nsenter -t $PB -n python3 $FX respond multicast 192.168.77.2 "Multicast Printer" 192.168.77.20 $T/r1 & PIDS="$PIDS $!"
# (the unicast one bound last: both share port 5353, and the kernel gives a question sent by unicast (16, RP_MDNS) to the
#  socket bound last; started together, either could get it, and the multicast one's answer never reached the asker)
w8 "[ -s $T/r1 ]"
nsenter -t $PB -n python3 $FX respond unicast 192.168.77.2 "Unicast Printer" 192.168.77.21 $T/r2 & PIDS="$PIDS $!"
nsenter -t $PC -n python3 $FX respond multicast 10.64.0.2 "Cell Printer" 10.64.0.20 $T/r3 & PIDS="$PIDS $!"
w8 "[ -s $T/r1 ] && [ -s $T/r2 ] && [ -s $T/r3 ]"
find() { env RP_USB=$T/none RP_CELLULAR=${1:-$T/run/firewall-cellular} $T/rudolf-print find; }

b0=$(blocked); o=$(find); b1=$(blocked)
ok "4. the firewall on: find lists the printer that answers by multicast and the one that answers by unicast, with their addresses, models and locations" "$(echo "$o" | grep -q '"name":"Multicast Printer","uri":"ipp://192.168.77.20:631/ipp/print","model":"Multicast Printer Model","location":"Test lab"' && echo "$o" | grep -q '"name":"Unicast Printer","uri":"ipp://192.168.77.21:631/ipp/print","model":"Unicast Printer Model"' && echo 1)" "$o"
ok "5. ...and not the cellular network's printer (find does not ask on a cellular interface)" "$(echo "$o" | grep -q '"ok":true' && ! echo "$o" | grep -q 'Cell Printer' && echo 1)" "$o"
ok "6. ...the firewall's blocked counter did not rise for mDNS ($b0 -> $b1)" "$([ -n "$b0" ] && [ "$b0" = "$b1" ] && echo 1)" "$b0 $b1"

# while find runs: port 5353 and the group on wlan0 only; when it ends: neither
find > $T/bg & FP=$!; w8 "grep -q ':14E9 ' /proc/net/udp"; U=$(cat /proc/net/udp); G=$(cat /proc/net/igmp); wait $FP
ok "7. while find runs: listening on UDP 5353, joined to 224.0.0.251 (FB0000E0) on wlan0, not on wwan0 (the cellular one)" "$(echo "$U" | grep -q ':14E9 ' && echo "$G" | awk '/^[0-9]/{d=$2} /FB0000E0/{print d}' | grep -qx wlan0 && ! echo "$G" | awk '/^[0-9]/{d=$2} /FB0000E0/{print d}' | grep -qx wwan0 && echo 1)" "$U $G"
ok "8. ...when it ends: the port closed and the group left" "$(! grep -q ':14E9 ' /proc/net/udp && ! grep -q FB0000E0 /proc/net/igmp && grep -q 'Multicast Printer' $T/bg && echo 1)" "$(cat /proc/net/udp /proc/net/igmp)"

# the firewall itself, on the cellular interface: find made to ask there too (as if wwan0 were not marked): still dropped
b0=$(blocked); o=$(find /dev/null); b1=$(blocked)
ok "9. nothing accepted on a cellular-marked interface: find asking on wwan0 too still lists no cellular printer, and its answers were counted as blocked ($b0 -> $b1)" "$(echo "$o" | grep -q 'Multicast Printer' && ! echo "$o" | grep -q 'Cell Printer' && [ "$b1" -gt "$b0" ] && echo 1)" "$o"

# other packets: from port 5353 to another port, to the group's other ports, unicast to 5353 from off the link
lis() { python3 $FX listen 192.168.77.1 $1 2 > $T/l.$1 & LP=$!; PIDS="$PIDS $LP"; w8 "grep -q ':$(printf %04X $1) ' /proc/net/udp"; }
lis 9999; b0=$(blocked); B python3 $FX send 192.168.77.2 5353 192.168.77.1 9999 255; B python3 $FX send 192.168.77.2 5353 224.0.0.251 9999 255; wait $LP; b1=$(blocked)
ok "10. a packet from source port 5353 to another UDP port (unicast and to the group, TTL 255): dropped, counted" "$([ "$(cat $T/l.9999)" = 0 ] && [ "$b1" -ge $((b0 + 2)) ] && echo 1)" "got $(cat $T/l.9999); $b0 -> $b1"
lis 5353; b0=$(blocked); B python3 $FX send 192.168.77.2 5353 192.168.77.1 5353 64; B python3 $FX send 192.168.77.2 1234 224.0.0.251 5353 255; B python3 $FX send 192.168.77.2 1234 192.168.77.1 5353 255; wait $LP; b1=$(blocked)
ok "11. to port 5353, dropped: unicast with a TTL a router lowered (64), from a port other than 5353 (to the group and unicast)" "$([ "$(cat $T/l.5353)" = 0 ] && [ "$b1" -ge $((b0 + 3)) ] && echo 1)" "got $(cat $T/l.5353); $b0 -> $b1"
lis 5353; C python3 $FX send 10.64.0.2 5353 224.0.0.251 5353 255; C python3 $FX send 10.64.0.2 5353 10.64.0.1 5353 255; wait $LP
ok "12. ...and on the cellular interface even a proper mDNS answer (to the group, and unicast from the same link): dropped" "$([ "$(cat $T/l.5353)" = 0 ] && echo 1)" "got $(cat $T/l.5353)"
lis 5353; B python3 $FX send 192.168.77.2 5353 224.0.0.251 5353 255; B python3 $FX send 192.168.77.2 5353 192.168.77.1 5353 255; wait $LP
ok "13. (the listener of 10-12 does see what the firewall accepts: an answer to the group and one from the same link)" "$([ "$(cat $T/l.5353)" = 2 ] && echo 1)" "got $(cat $T/l.5353)"

# a program on this computer saying it is a printer (sent to the group from this computer's address): not listed
python3 $FX respond fake 192.168.77.1 "Local Fake" 192.168.77.1 $T/r4 & FK=$!; PIDS="$PIDS $FK"; w8 "[ -s $T/r4 ]"
o=$(find); kill $FK
ok "14. a program on this computer pretending to be a printer (to the group, from this computer's own address): not listed" "$(echo "$o" | grep -q 'Multicast Printer' && ! echo "$o" | grep -q 'Local Fake' && echo 1)" "$o"

# two at once (rudolfd's find and the print queue's): both find both
# (a unicast answer reaches one of the two sockets on 5353: the kernel gives a unicast packet to one; multicast to both)
find > $T/f1 & P1=$!; find > $T/f2 & P2=$!; wait $P1; wait $P2
ok "15. two finds at once (rudolfd's and the print queue's: SO_REUSEADDR): both run and list the multicast printer; the unicast one's answers reach one of them" "$(grep -q 'Multicast Printer' $T/f1 && grep -q 'Multicast Printer' $T/f2 && cat $T/f1 $T/f2 | grep -q 'Unicast Printer' && echo 1)" "$(cat $T/f1 $T/f2)"

# the tests' way (RP_MDNS) unchanged: the question by unicast to a stand-in from an ephemeral port
# (one question by UDP: asked up to three times, as a person would press Find printers again, so one lost packet is not a failure)
for i in 1 2 3; do o=$(env RP_USB=$T/none RP_MDNS=192.168.77.2:5353 $T/rudolf-print find); echo "$o" | grep -q 'Unicast Printer' && break; done
ok "16. RP_MDNS (the other tests' stand-ins) still asks the address given: the unicast stand-in found" "$(echo "$o" | grep -q 'Unicast Printer' && echo 1)" "$o"

# the control: with the firewall off, find asking on wwan0 does see the cellular printer (so 9 and 12 test the firewall)
$T/fw off; o=$(find /dev/null)
ok "17. (control) the firewall off: find asking on wwan0 lists the cellular printer, so 9 and 12 were the firewall's doing" "$(echo "$o" | grep -q 'Cell Printer' && echo 1)" "$o"
echo "$((N-F))/$N passed"; [ $F = 0 ]
