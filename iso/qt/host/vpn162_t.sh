#!/bin/sh
# 1.6.2 (docs/plan-1.6.2.md Part 33): a VPN for the whole computer. The real rudolfd and its VPN helper (rudolf-vpn.c),
# built here, in a private mount and network namespace of this test's own ("this computer": never the host's network,
# its /run, /persist, /home/rudolf and name servers), with a veth "wlan0" to a second namespace, "the internet": a
# WireGuard server there (the kernel's, set up with wg, which is only this test's: Rudolf OS ships none), the network's
# own name server (192.168.77.2), the VPN's (10.66.0.1), and a server that answers with the address it was reached
# from (203.0.113.5, behind the VPN's server or not). The accounts are made as the page makes them (fixtures/vpnls.js:
# Ann an administrator, Bob a standard account, Kim one with parental controls); rudolfd's requests made as the desktop
# (a process listed in RUDOLFD_DESKTOP_CG, with the token).
#   imported: malformed files refused with the line they fail on; only with an administrator's password; the file
#   root's only, its key in no answer, log or page; connected (a handshake with the server); the whole computer's
#   traffic and its name lookups through the tunnel, a capture on wlan0 showing only the tunnel's packets; the local
#   network still reached directly; IPv6 not around it; disconnected: the network as it was; "Administrators only",
#   "Always on", a child account, another account's VPN; Always on again at rudolfd's start; removed.
# Needs root, unshare, nsenter, ip, wg (wireguard-tools), tcpdump, python3, gcc, node and a kernel with WireGuard.
# Ports 8631 and 8632 in the test's namespaces only; rudolfd's 8080 inside its own.   sh vpn162_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys
for c in unshare ip wg tcpdump python3 gcc nsenter node; do command -v $c >/dev/null || { echo "SKIP: needs $c"; exit 0; }; done
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$V162_INNER" ] && exec env V162_INNER=1 unshare -m -n sh "$0" "$@"
T=$(mktemp -d /tmp/vpn162.XXXXXX); chmod 755 $T; PIDS=""; RDP=""
cleanup() { for p in $RDP $PIDS; do kill $p 2>/dev/null; done; umount /home/rudolf /persist /run /etc 2>/dev/null; rm -rf "$T"; }   # (only what this test started)
trap cleanup EXIT INT TERM
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 1500)"; F=$((F+1)); fi; }
w8() { i=0; while [ $i -lt ${2:-50} ]; do eval "$1" && return 0; timeout 0.1 tail -f /dev/null; i=$((i+1)); done; return 1; }
ip link add wgprobe type wireguard 2>/dev/null || { echo "SKIP: this kernel has no WireGuard (modprobe wireguard)"; exit 0; }; ip link del wgprobe
. $SYS/../systools/hardflags.sh 2>/dev/null
mkdir -p $T/root
gcc -O2 $HARD -Wall -Wno-misleading-indentation -Werror -o $T/root/vpn $SYS/rudolf-vpn.c 2>$T/cc || { echo "FAIL: the VPN helper does not build: $(cat $T/cc)"; exit 1; }
chmod 700 $T/root/vpn
gcc -O2 -w -DRD_ROOT_DIR="\"$T/root\"" -o $T/rudolfd $SYS/rudolfd.c -lz 2>$T/cc || { echo "FAIL: rudolfd does not build: $(cat $T/cc)"; exit 1; }

# ---- this computer: its folders (a copy of /etc: rudolfd writes the accounts' users into it), its name servers, its network
cp -a /etc $T/etc && mount --bind $T/etc /etc || { echo "SKIP: /etc cannot be copied here"; exit 0; }
RES=$(readlink -f /etc/resolv.conf)
mount -t tmpfs none /run; mkdir -p /run/rudolf/dns /home/rudolf /persist; mount -t tmpfs none /home/rudolf; mount -t tmpfs none /persist
case "$RES" in /etc/*) ;; *) rm -f /etc/resolv.conf; RES=/etc/resolv.conf;; esac
echo "nameserver 192.168.77.2" > "$RES"
cp /etc/resolv.conf /run/rudolf/dns/wlan0   # (as udhcpc.sh keeps a connection's own)
: > /run/rudolf/events; chmod 644 /run/rudolf/events
node $HERE/fixtures/vpnls.js /home/rudolf/.rudolf-ls.json || { echo "FAIL: the accounts"; exit 1; }
ip link set lo up
unshare -n tail -f /dev/null & PB=$!; PIDS="$PIDS $PB"
w8 "[ \"\$(readlink /proc/$PB/ns/net)\" != \"\$(readlink /proc/self/ns/net)\" ]"
B() { nsenter -t $PB -n "$@"; }
ip link add wlan0 type veth peer name pr0 && ip link set pr0 netns $PB || { echo "SKIP: veth pairs cannot be made here"; exit 0; }
ip addr add 192.168.77.1/24 dev wlan0; ip -6 addr add fd77::1/64 dev wlan0 nodad; ip link set wlan0 up
B ip link set lo up; B ip addr add 192.168.77.2/24 dev pr0; B ip -6 addr add fd77::2/64 dev pr0 nodad; B ip link set pr0 up
B ip addr add 203.0.113.5/32 dev lo; B ip -6 addr add 2001:db8::5/128 dev lo
w8 "ip -o link show wlan0 | grep -q LOWER_UP"
ip route add default via 192.168.77.2 dev wlan0 metric 600; ip -6 route add default via fd77::2 dev wlan0 metric 600
# the VPN's server: its keys made now, for this run only (never kept)
SK=$(wg genkey); SP=$(echo "$SK" | wg pubkey); CK=$(wg genkey); CP=$(echo "$CK" | wg pubkey); PSK=$(wg genpsk)
echo "$SK" > $T/sk; echo "$PSK" > $T/psk; chmod 600 $T/sk $T/psk
B ip link add wg0 type wireguard; B wg set wg0 listen-port 51820 private-key $T/sk peer "$CP" preshared-key $T/psk allowed-ips 10.66.0.2/32
B ip addr add 10.66.0.1/24 dev wg0; B ip link set wg0 up
B python3 $HERE/fixtures/vpnnet.py dns 192.168.77.2 192.168.77.2 $T/dns-isp & PIDS="$PIDS $!"
B python3 $HERE/fixtures/vpnnet.py dns 10.66.0.1 10.66.0.53 $T/dns-vpn & PIDS="$PIDS $!"
B python3 $HERE/fixtures/vpnnet.py who 203.0.113.5 8631 & PIDS="$PIDS $!"
B python3 $HERE/fixtures/vpnnet.py who 192.168.77.2 8632 & PIDS="$PIDS $!"
B python3 $HERE/fixtures/vpnnet.py who 2001:db8::5 8631 & PIDS="$PIDS $!"
who() { python3 -c "import urllib.request,sys; print(urllib.request.urlopen('http://$1/', timeout=${2:-3}).read().decode())" 2>/dev/null || echo none; }
look() { python3 -c "import socket; print(socket.gethostbyname('$1'))" 2>/dev/null || echo none; }
w8 '[ "$(who 203.0.113.5:8631)" = 192.168.77.1 ]' 60
ok "0. the test's network: the internet reached through wlan0, from 192.168.77.1; names from the network's name server" "$([ "$(who 203.0.113.5:8631)" = 192.168.77.1 ] && [ "$(look who.test)" = 192.168.77.2 ] && echo 1)" "$(who 203.0.113.5:8631) $(look who.test)"

# ---- rudolfd: requests as the desktop (a process of its group, with the token), signed in as SIGNED
mkdir -p $T/cg; : > $T/cg/cgroup.procs
cat > $T/cl.py <<'EOF'
import os, sys, time, urllib.request, urllib.error
name, method, path, d = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
body = open(sys.argv[5], 'rb').read() if len(sys.argv) > 5 else (b'' if method == 'POST' else None)
os.setgid(65534); os.setuid(65534)
open(d + '/pid.' + name, 'w').write(str(os.getpid()))
while not os.path.exists(d + '/go.' + name): time.sleep(.05)
tok = open(d + '/tok').read().strip() if os.path.exists(d + '/tok') else ''
q = urllib.request.Request('http://127.0.0.1:8080' + path, method=method, data=body, headers={'X-Rudolf-Token': tok})
try: r = urllib.request.urlopen(q, timeout=40); print(r.status, r.read().decode('utf8', 'replace')[:6000])
except urllib.error.HTTPError as e: print(e.code, e.read().decode('utf8', 'replace')[:3000])
EOF
chmod 644 $T/cl.py; chmod 777 $T
K=0
api() { K=$((K+1)); rm -f $T/pid.$K $T/go.$K; if [ -n "$3" ]; then printf '%s' "$3" > $T/body.$K; chmod 644 $T/body.$K; python3 $T/cl.py $K $1 "$2" $T $T/body.$K > $T/out.$K 2>&1 & else python3 $T/cl.py $K $1 "$2" $T > $T/out.$K 2>&1 & fi
  w8 "[ -s $T/pid.$K ]" 40; echo "$(cat $T/pid.$K)" >> $T/cg/cgroup.procs; touch $T/go.$K; wait $!; cat $T/out.$K; }
as() { printf '%s\n' "$1" > /run/rudolf/home-session; }
code() { echo "$1" | head -1 | cut -d' ' -f1; }
js() { echo "$1" | cut -d' ' -f2- | node -e "const j=JSON.parse(require('fs').readFileSync(0,'utf8'));console.log(String($2))" 2>/dev/null; }
start_rd() { env RUDOLFD_DESKTOP_CG=$T/cg RUDOLFD_DESKTOP_UID=65534 $T/rudolfd >> $T/rd.log 2>&1 & RDP=$!
  w8 'python3 -c "import socket; socket.create_connection((\"127.0.0.1\", 8080), 1)" 2>/dev/null' 80 || { echo "FAIL: rudolfd did not start: $(tail -5 $T/rd.log)"; exit 1; }
  o=$(api GET /); echo "$o" | sed -n 's/.*RUDOLFD={token:"\([0-9a-f]*\)".*/\1/p' | head -1 > $T/tok; chmod 644 $T/tok; }
start_rd
as uBobC3d4
conf() { printf '[Interface]\nPrivateKey = %s\nAddress = 10.66.0.2/32\nDNS = 10.66.0.1\n\n[Peer]\nPublicKey = %s\nPresharedKey = %s\nAllowedIPs = 0.0.0.0/0\nEndpoint = %s\n' "$CK" "$SP" "$PSK" "${1:-192.168.77.2:51820}"; }
jbody() { node -e "console.log(JSON.stringify({n:process.argv[1],p:process.argv[2],name:process.argv[3],conf:process.argv[4]}))" "$@"; }

# ---- importing
o=$(api GET /api/vpn)
ok "1. GET vpn: none yet, WireGuard there; Bob is signed in, not an administrator" "$([ "$(code "$o")" = 200 ] && [ "$(js "$o" 'j.vpns.length+" "+j.wireguard+" "+j.me.name+" "+j.me.admin')" = "0 true Bob false" ] && echo 1)" "$o"
o=$(api POST /api/vpn/add "$(jbody Bob pw-bob Home "$(conf)")"); o2=$(api POST /api/vpn/add "$(jbody Ann wrong Home "$(conf)")"); o3=$(api POST /api/vpn/add "{\"name\":\"Home\",\"conf\":\"x\"}")
ok "2. adding a VPN: refused without an administrator's password (Bob's own, a wrong one, none)" "$([ "$(code "$o")" = 403 ] && [ "$(code "$o2")" = 403 ] && [ "$(code "$o3")" = 403 ] && [ -z "$(ls /persist/etc/vpn 2>/dev/null)" ] && echo 1)" "$o / $o2 / $o3"
bad1=$(api POST /api/vpn/add "$(jbody Ann pw-ann X "$(printf '[Interface]\nPrivateKey = nope\n')")")
bad2=$(api POST /api/vpn/add "$(jbody Ann pw-ann X "$(conf | sed 's/^Endpoint/Endpoit/')")")
bad3=$(api POST /api/vpn/add "$(jbody Ann pw-ann X "$(conf | sed '/^Endpoint/d')")")
bad4=$(api POST /api/vpn/add "$(jbody Ann pw-ann X "$(conf | sed 's/^AllowedIPs = .*/AllowedIPs = 0.0.0.0\/33/')")")
bad5=$(api POST /api/vpn/add "$(jbody Ann pw-ann X "client
remote vpn.example.com 1194")")
bad6=$(api POST /api/vpn/add "$(jbody Ann pw-ann X "$(conf | sed 's/^\[Peer\]/[Server]/')")")
ok "3. malformed files refused, each saying why (a key that is not one, an unknown setting, no endpoint, a bad network, not a WireGuard file, an unknown section)" "$(for b in "$bad1" "$bad2" "$bad3" "$bad4" "$bad5" "$bad6"; do [ "$(code "$b")" = 409 ] || exit; done; echo "$bad1" | grep -q 'line 2: the private key' && echo "$bad2" | grep -q 'line 9: Endpoit is not a setting' && echo "$bad3" | grep -q 'no Endpoint' && echo "$bad4" | grep -q 'not an address or network' && echo "$bad5" | grep -q 'not' && echo "$bad6" | grep -q 'not a section' && [ -z "$(ls /persist/etc/vpn 2>/dev/null)" ] && echo 1)" "$bad1 / $bad2 / $bad3 / $bad4 / $bad5 / $bad6"
o=$(api POST /api/vpn/add "$(jbody Ann pw-ann 'Home <VPN>' "$(conf vpn.test:51820 | sed 's/^DNS/PostUp = touch \/tmp\/pwned\nDNS/')")")
ID=$(js "$o" j.id)
ok "4. added with Ann's password: its id, and a note that its PostUp is never run" "$([ "$(code "$o")" = 200 ] && echo "$ID" | grep -qx 'v[0-9a-f]\{8\}' && echo "$o" | grep -q 'never run' && echo 1)" "$o"
P=$(stat -c '%u %a' /persist/etc/vpn 2>&1); C=$(stat -c '%u %a' /persist/etc/vpn/$ID.conf 2>&1); M=$(stat -c '%u %a' /persist/etc/vpn/$ID.meta 2>&1)
rd=$(python3 -c "import os; os.setgid(65534); os.setuid(65534)
try: open('/persist/etc/vpn/$ID.conf').read(); print('read')
except Exception as e: print('refused')")
ok "5. kept where only root reads it: the folder root's 0700, the file root's 0600; another user cannot read it" "$([ "$P" = '0 700' ] && [ "$C" = '0 600' ] && [ "$M" = '0 600' ] && [ "$rd" = refused ] && echo 1)" "$P / $C / $M / $rd"
o=$(api GET /api/vpn)
ok "6. listed: its name as given, the server, the address, the DNS server, the whole computer's, off, Everyone; Bob may turn it on" "$([ "$(js "$o" 'j.vpns[0].name+"|"+j.vpns[0].server+"|"+j.vpns[0].addresses[0]+"|"+j.vpns[0].dns[0]+"|"+j.vpns[0].whole+"|"+j.vpns[0].on+"|"+j.vpns[0].policy+"|"+j.vpns[0].mayOn')" = "Home <VPN>|vpn.test:51820|10.66.0.2/32|10.66.0.1|true|false|everyone|true" ] && ! echo "$o" | grep -q '<VPN>' && echo 1)" "$o"

# ---- connected
tcpdump -i wlan0 -n -U -w $T/cap.pcap > $T/tcpdump.log 2>&1 & TD=$!; PIDS="$PIDS $TD"; w8 "grep -q listening $T/tcpdump.log" 50
: > $T/dns-isp; : > $T/dns-vpn
o=$(api POST "/api/vpn/on?id=$ID")
ok "7. Bob turns it on (Everyone: no password): connected, a handshake with the server (its name looked up first)" "$([ "$(code "$o")" = 200 ] && [ "$(js "$o" j.state)" = connected ] && B wg show wg0 latest-handshakes | awk '{print \$2}' | grep -qv '^0$' && grep -q '^vpn.test 1 ' $T/dns-isp && echo 1)" "$o $(B wg show wg0) $(tail -3 $T/rd.log)"
IF=$(ip -d link show rvpn0 2>&1); RU=$(ip rule 2>&1); RT=$(ip route show table 52566 2>&1)
ok "8. ...the kernel's own WireGuard (rvpn0), the whole computer's table and rules, the server's route outside the tunnel (its mark)" "$(echo "$IF" | grep -q wireguard && echo "$RU" | grep -q 'not from all fwmark 0x52564e lookup 52566' && echo "$RU" | grep -q 'lookup main suppress_prefixlength 0' && echo "$RT" | grep -q 'default dev rvpn0' && echo 1)" "$IF / $RU / $RT"
ok "9. all traffic through the tunnel: the internet sees the tunnel's address (10.66.0.2), not wlan0's" "$([ "$(who 203.0.113.5:8631)" = 10.66.0.2 ] && echo 1)" "$(who 203.0.113.5:8631)"
: > $T/dns-isp
ok "10. name lookups through the VPN's name server (10.66.0.1, in the tunnel), the network's own not asked; resolv.conf says so" "$([ "$(look who.test)" = 10.66.0.53 ] && grep -q '^who.test 1 10.66.0.2' $T/dns-vpn && [ ! -s $T/dns-isp ] && grep -qx 'nameserver 10.66.0.1' /etc/resolv.conf && echo 1)" "$(look who.test) / vpn: $(cat $T/dns-vpn) / isp: $(cat $T/dns-isp) / $(cat /etc/resolv.conf)"
ok "11. the local network still reached directly (192.168.77.2 from 192.168.77.1)" "$([ "$(who 192.168.77.2:8632)" = 192.168.77.1 ] && echo 1)" "$(who 192.168.77.2:8632)"
W6=$(who '[2001:db8::5]:8631' 2); R6=$(ip -6 route 2>&1)
ok "12. IPv6 does not go round the tunnel (the file has no IPv6): the internet's IPv6 address unreachable, the local network's still there" "$([ "$W6" = none ] && echo "$R6" | grep -q '^unreachable ::/1' && echo "$R6" | grep -q '^unreachable 8000::/1' && ping -6 -c1 -W2 fd77::2 >/dev/null 2>&1 && echo 1)" "$W6 / $R6"
kill $TD 2>/dev/null; wait $TD 2>/dev/null
CAP=$(tcpdump -n -r $T/cap.pcap 2>/dev/null | grep -v -i 'ARP\|ICMP6\|IP6 fe80' )
OUT=$(echo "$CAP" | grep -v '192.168.77.1.[0-9]* > 192.168.77.2.51820: UDP\|192.168.77.2.51820 > 192.168.77.1.[0-9]*: UDP\|192.168.77.1.[0-9]* > 192.168.77.2.53: \|192.168.77.2.53 > 192.168.77.1.[0-9]*: \|192.168.77.1.[0-9]* > 192.168.77.2.8632\|192.168.77.2.8632 > 192.168.77.1\|fd77::1 > fd77::2: ICMP6\|fd77::2 > fd77::1: ICMP6' | grep -v '^$')
NWG=$(echo "$CAP" | grep -c '51820: UDP')
ok "13. a capture on wlan0 (outside the tunnel) while it was on: only the tunnel's packets, the server's name looked up before, and the local network's own requests; never 203.0.113.5 or a name lookup in the clear" "$([ "$NWG" -ge 4 ] && [ -z "$OUT" ] && ! echo "$CAP" | grep -q '203.0.113.5\|who.test' && [ "$(echo "$CAP" | grep -c '> 192.168.77.2.53:')" -le 4 ] && echo 1)" "wg=$NWG other: $OUT"
o=$(api GET /api/vpn); L=$(cat $T/rd.log /persist/var/log/health.log /run/rudolf/health.log /run/rudolf/events 2>/dev/null)
ok "14. GET vpn: on, connected, turned on by Bob; Bob may turn it off" "$([ "$(js "$o" 'j.vpns[0].on+" "+j.vpns[0].state+" "+j.vpns[0].by+" "+j.vpns[0].mayOff+" "+j.active')" = "true connected Bob true $ID" ] && echo 1)" "$o"
ok "15. the private and preshared keys in no answer, log or page: not in GET vpn, rudolfd's log, the health log, /run/rudolf, the events" "$(for k in "$CK" "$PSK" "$(echo "$CK" | cut -c1-20)"; do echo "$o$L" | grep -qF "$k" && exit; grep -rqF "$k" /run/rudolf 2>/dev/null && exit; done; [ ! -e /tmp/pwned ] && echo 1)" "a key was found"

# ---- off
o=$(api POST /api/vpn/off)
ok "16. Bob turns it off: the interface, its rules and table gone; resolv.conf the network's own again" "$([ "$(code "$o")" = 200 ] && ! ip link show rvpn0 >/dev/null 2>&1 && ! ip rule | grep -q 52566 && ! ip rule | grep -q suppress_prefixlength && [ -z "$(ip route show table 52566 2>/dev/null)" ] && grep -qx 'nameserver 192.168.77.2' /etc/resolv.conf && [ ! -e /run/rudolf/dns-vpn ] && echo 1)" "$o / $(ip rule) / $(cat /etc/resolv.conf)"
ok "17. ...the network as it was: the internet sees wlan0's address, names from the network's server, IPv6 reached again" "$([ "$(who 203.0.113.5:8631)" = 192.168.77.1 ] && [ "$(look who.test)" = 192.168.77.2 ] && [ "$(who '[2001:db8::5]:8631')" = fd77::1 ] && echo 1)" "$(who 203.0.113.5:8631) $(look who.test) $(who '[2001:db8::5]:8631') $(ip -6 route)"

# ---- who may do what
o=$(api POST /api/vpn/set "{\"n\":\"Bob\",\"p\":\"pw-bob\",\"id\":\"$ID\",\"policy\":\"admins\"}")
o2=$(api POST /api/vpn/set "{\"n\":\"Ann\",\"p\":\"pw-ann\",\"id\":\"$ID\",\"policy\":\"admins\"}")
o3=$(api POST "/api/vpn/on?id=$ID")
ok "18. Administrators only (set with Ann's password, not Bob's): Bob alone cannot turn it on, and is told an administrator can" "$([ "$(code "$o")" = 403 ] && [ "$(code "$o2")" = 200 ] && [ "$(code "$o3")" = 403 ] && [ "$(js "$o3" j.needAdmin)" = true ] && ! ip link show rvpn0 >/dev/null 2>&1 && echo 1)" "$o / $o2 / $o3"
o=$(api POST "/api/vpn/on?id=$ID" '{"n":"Ann","p":"pw-ann"}')
ok "19. ...with Ann's password Bob's turning it on goes through" "$([ "$(code "$o")" = 200 ] && ip link show rvpn0 >/dev/null 2>&1 && echo 1)" "$o"
as uAnnA1b2; o=$(api GET /api/vpn)
ok "20. Ann signed in: the VPN is Bob's, on for everyone on this computer (by: Bob), and Ann may turn it off" "$([ "$(js "$o" 'j.vpns[0].by+" "+j.vpns[0].mayOff+" "+j.me.admin')" = "Bob true true" ] && echo 1)" "$o"
o=$(api POST /api/vpn/set "{\"n\":\"Ann\",\"p\":\"pw-ann\",\"id\":\"$ID\",\"policy\":\"always\"}"); o2=$(api POST /api/vpn/off)
ok "21. Always on: nobody can turn it off, an administrator neither" "$([ "$(code "$o")" = 200 ] && [ "$(code "$o2")" = 403 ] && ip link show rvpn0 >/dev/null 2>&1 && echo "$o2" | grep -q 'always on' && echo 1)" "$o / $o2"
o=$(api POST /api/vpn/off '{"n":"Ann","p":"pw-ann"}')
ok "22. ...not even with an administrator's password" "$([ "$(code "$o")" = 403 ] && ip link show rvpn0 >/dev/null 2>&1 && echo 1)" "$o"
# rudolfd started again: an Always on VPN comes back by itself (the interface removed first, as at a restart)
kill $RDP; wait $RDP 2>/dev/null; ip link del rvpn0; rm -f /run/rudolf/vpn/on
start_rd
w8 "ip link show rvpn0 >/dev/null 2>&1" 100
o=$(api GET /api/vpn)
ok "23. Always on at rudolfd's start: on again by itself, connected" "$(ip link show rvpn0 >/dev/null 2>&1 && w8 "[ \"\$(who 203.0.113.5:8631)\" = 10.66.0.2 ]" 50 && echo 1)" "$o $(tail -3 $T/rd.log)"
o=$(api POST /api/vpn/set "{\"n\":\"Ann\",\"p\":\"pw-ann\",\"id\":\"$ID\",\"policy\":\"everyone\"}")
as uKimE5f6; o2=$(api GET /api/vpn); o3=$(api POST /api/vpn/off)
ok "24. a child account (parental controls) cannot turn a VPN off, even one for Everyone" "$([ "$(code "$o")" = 200 ] && [ "$(js "$o2" 'j.me.child+" "+j.vpns[0].mayOff+" "+j.vpns[0].mayOn')" = "true false true" ] && [ "$(code "$o3")" = 403 ] && echo "$o3" | grep -q 'parental controls' && ip link show rvpn0 >/dev/null 2>&1 && echo 1)" "$o2 / $o3"
o=$(api POST /api/vpn/off '{"n":"Ann","p":"pw-ann"}')
ok "25. ...not even with an administrator's password typed in for it" "$([ "$(code "$o")" = 403 ] && ip link show rvpn0 >/dev/null 2>&1 && echo 1)" "$o"
as uBobC3d4; o=$(api POST /api/vpn/off)
ok "26. Bob (Everyone) turns it off" "$([ "$(code "$o")" = 200 ] && ! ip link show rvpn0 >/dev/null 2>&1 && echo 1)" "$o"
o=$(api POST /api/vpn/on?id=v00000000); o2=$(api POST '/api/vpn/on?id=../x'); o3=$(api GET /api/vpn/on?id=$ID); o4=$(api POST /api/vpn/set "{\"n\":\"Ann\",\"p\":\"pw-ann\",\"id\":\"$ID\",\"policy\":\"root\"}")
ok "27. a VPN that is not there, an id that is not one, a GET to turn on, a policy that is not one: refused" "$([ "$(code "$o")" = 404 ] && [ "$(code "$o2")" = 400 ] && [ "$(code "$o3")" = 405 ] && [ "$(code "$o4")" = 400 ] && echo 1)" "$o / $o2 / $o3 / $o4"
o=$(api POST /api/vpn/set "{\"n\":\"Ann\",\"p\":\"pw-ann\",\"id\":\"$ID\",\"name\":\"Work\"}"); o2=$(api GET /api/vpn)
ok "28. renamed by an administrator" "$([ "$(code "$o")" = 200 ] && [ "$(js "$o2" 'j.vpns[0].name')" = Work ] && echo 1)" "$o / $o2"
o=$(api POST /api/vpn/remove "{\"n\":\"Bob\",\"p\":\"pw-bob\",\"id\":\"$ID\"}"); o2=$(api POST /api/vpn/remove "{\"n\":\"Ann\",\"p\":\"pw-ann\",\"id\":\"$ID\"}")
ok "29. removed (Ann's password; not Bob's): its files gone" "$([ "$(code "$o")" = 403 ] && [ "$(code "$o2")" = 200 ] && [ -z "$(ls /persist/etc/vpn)" ] && echo 1)" "$o / $o2"
echo "$((N-F))/$N passed"; [ $F = 0 ]
