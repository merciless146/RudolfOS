#!/bin/sh
# 1.6.2 (docs/plan-1.6.2.md Part 33 step 3): OpenVPN 2.6 for the whole computer. As vpn162_t.sh (the real rudolfd and its
# VPN helper in a private mount and network namespace, a veth "wlan0" to "the internet"), with an OpenVPN server there
# (this test's own, its certificates made in the run): a .ovpn file imported (malformed and dangerous ones refused:
# scripts, plugins, files of its own, a tap VPN), kept root's only; connected (OpenVPN run by the helper, outside any
# sandbox, nobody once connected), the whole computer's traffic and name lookups through it and a capture on wlan0 showing
# only the tunnel, disconnected (OpenVPN gone, the network as it was), the kill switch; OpenVPN's memory while connected
# and nothing of it while off; the disk it adds (the program and the libraries the base layer lacks are said by
# ci_release.sh). Needs root, openvpn, openssl, ip, tcpdump, nft, python3, gcc, node.   sh vpnovpn162_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys
for c in unshare ip openvpn openssl tcpdump python3 gcc nsenter node nft; do command -v $c >/dev/null || { echo "SKIP: needs $c"; exit 0; }; done
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$O162_INNER" ] && exec env O162_INNER=1 unshare -m -n sh "$0" "$@"
T=$(mktemp -d /tmp/ovpn162.XXXXXX); chmod 755 $T; PIDS=""; RDP=""
cleanup() { [ -s /run/rudolf/vpn/ovpn.pid ] && kill "$(cat /run/rudolf/vpn/ovpn.pid)" 2>/dev/null; for p in $RDP $PIDS; do kill $p 2>/dev/null; done; umount /usr/share/rudolf/www /home/rudolf /persist /run /etc 2>/dev/null; rm -rf "$T"; }   # (only what this test started)
trap cleanup EXIT INT TERM
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 1500)"; F=$((F+1)); fi; }
w8() { i=0; while [ $i -lt ${2:-50} ]; do eval "$1" && return 0; timeout 0.1 tail -f /dev/null; i=$((i+1)); done; return 1; }
[ -c /dev/net/tun ] || { echo "SKIP: no /dev/net/tun"; exit 0; }
. $SYS/../systools/hardflags.sh 2>/dev/null
mkdir -p $T/root
gcc -O2 $HARD -Wall -Wno-misleading-indentation -Werror -o $T/root/vpn $SYS/rudolf-vpn.c 2>$T/cc || { echo "FAIL: the VPN helper does not build: $(cat $T/cc)"; exit 1; }
chmod 700 $T/root/vpn
sed "s#RR_DIR \"/vpn\"#\"$T/root/vpn\"#" $SYS/rudolf-vpn.c > $T/vpn-t.c && gcc -O2 -w -I$SYS -o $T/root/vpn $T/vpn-t.c 2>$T/cc || { echo "FAIL: the VPN helper (its own path for OpenVPN's hook) does not build: $(cat $T/cc)"; exit 1; }; chmod 700 $T/root/vpn
gcc -O2 -w -DRD_ROOT_DIR="\"$T/root\"" -o $T/rudolfd $SYS/rudolfd.c -lz 2>$T/cc || { echo "FAIL: rudolfd does not build: $(cat $T/cc)"; exit 1; }

# ---- this computer: its folders (a copy of /etc: rudolfd writes the accounts' users into it), its name servers, its network
cp -a /etc $T/etc && mount --bind $T/etc /etc || { echo "SKIP: /etc cannot be copied here"; exit 0; }
RES=$(readlink -f /etc/resolv.conf)
mount -t tmpfs none /run; mkdir -p /run/rudolf/dns /home/rudolf /persist /usr/share/rudolf/www; mount -t tmpfs none /home/rudolf; mount -t tmpfs none /persist
mount --bind $HERE/../../www /usr/share/rudolf/www   # (the page rudolfd serves, with its token)
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
# the VPN's server (OpenVPN, this test's own): its certificate authority, certificates and tls-crypt key made now, for
# this run only (never kept)
P=$T/pki; mkdir -p $P; cd $P
openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -nodes -keyout ca.key -out ca.crt -days 2 -subj /CN=test-ca >/dev/null 2>&1
for n in server client; do openssl req -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -nodes -keyout $n.key -out $n.csr -subj /CN=$n >/dev/null 2>&1
  printf 'keyUsage=digitalSignature,keyAgreement\nextendedKeyUsage=%s\n' "$([ $n = server ] && echo serverAuth || echo clientAuth)" > $n.ext; openssl x509 -req -in $n.csr -CA ca.crt -CAkey ca.key -CAcreateserial -days 2 -out $n.crt -extfile $n.ext >/dev/null 2>&1; done
openvpn --genkey secret tc.key >/dev/null 2>&1; cd - >/dev/null
printf 'pw-ann\n' > $T/upw; printf 'pw-user-ok\n' > /dev/null
cat > $T/server.conf <<EOF2
dev tun0
proto udp
port 1194
server 10.67.0.0 255.255.255.0
topology subnet
ca $P/ca.crt
cert $P/server.crt
key $P/server.key
dh none
tls-crypt $P/tc.key
push "redirect-gateway def1"
push "dhcp-option DNS 10.67.0.1"
keepalive 5 20
verify-client-cert require
verb 3
EOF2
B openvpn --config $T/server.conf --log $T/server.log --daemon --writepid $T/server.pid
w8 "grep -q 'Initialization Sequence Completed' $T/server.log" 100 || { echo "FAIL: the test's OpenVPN server did not start: $(tail -5 $T/server.log)"; exit 1; }
PIDS="$PIDS $(cat $T/server.pid)"
B python3 $HERE/fixtures/vpnnet.py dns 192.168.77.2 192.168.77.2 $T/dns-isp & PIDS="$PIDS $!"
w8 "B ip -o addr show tun0 | grep -q 10.67.0.1" 50; B python3 $HERE/fixtures/vpnnet.py dns 10.67.0.1 10.67.0.53 $T/dns-vpn & PIDS="$PIDS $!"
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
ovpn() { printf 'client\ndev tun\nproto udp\nremote %s 1194\nnobind\npersist-key\npersist-tun\nremote-cert-tls server\nverb 3\n<ca>\n%s\n</ca>\n<cert>\n%s\n</cert>\n<key>\n%s\n</key>\n<tls-crypt>\n%s\n</tls-crypt>\n' "${1:-vpn.test}" "$(cat $P/ca.crt)" "$(cat $P/client.crt)" "$(cat $P/client.key)" "$(cat $P/tc.key)"; }
jbody() { node -e "console.log(JSON.stringify({n:process.argv[1],p:process.argv[2],name:process.argv[3],conf:process.argv[4],type:'openvpn'}))" "$@"; }


# ---- importing
o=$(api POST /api/vpn/add "$(jbody Bob pw-bob Work "$(ovpn)")")
ok "O1. adding an OpenVPN file: refused without an administrator's password" "$([ "$(code "$o")" = 403 ] && echo 1)" "$o"
B1=$(api POST /api/vpn/add "$(jbody Ann pw-ann X "$(ovpn | sed 's/^verb 3/up \/bin\/sh/')")")
B2=$(api POST /api/vpn/add "$(jbody Ann pw-ann X "$(ovpn | sed 's/^verb 3/script-security 3/')")")
B3=$(api POST /api/vpn/add "$(jbody Ann pw-ann X "$(ovpn | sed 's/^verb 3/plugin \/tmp\/x.so/')")")
B4=$(api POST /api/vpn/add "$(jbody Ann pw-ann X "$(ovpn | sed 's/^verb 3/ca \/etc\/shadow/')")")
B5=$(api POST /api/vpn/add "$(jbody Ann pw-ann X "$(ovpn | sed 's/^dev tun/dev tap/')")")
B6=$(api POST /api/vpn/add "$(jbody Ann pw-ann X "$(ovpn | sed 's/^verb 3/management 127.0.0.1 7505/')")")
B7=$(api POST /api/vpn/add "$(jbody Ann pw-ann X "$(ovpn | sed '/^<ca>/,/^<\/ca>/d')")")
ok "O2. malformed and dangerous files refused, each saying why (a script, script-security, a plugin, a file of its own, a tap VPN, the management port, no <ca>)" "$(for b in "$B1" "$B2" "$B3" "$B4" "$B5" "$B6" "$B7"; do [ "$(code "$b")" = 409 ] || exit; done; echo "$B1" | grep -q 'up is not taken' && echo "$B2" | grep -q 'script-security is not taken' && echo "$B3" | grep -q 'plugin is not taken' && echo "$B4" | grep -q 'must be inside the file' && echo "$B5" | grep -q 'only a tun VPN' && echo "$B6" | grep -q 'management is not taken' && echo "$B7" | grep -q 'certificate authority' && [ -z "$(ls /persist/etc/vpn 2>/dev/null)" ] && echo 1)" "$B1 / $B2 / $B3 / $B4 / $B5 / $B6 / $B7"
o=$(api POST /api/vpn/add "$(jbody Ann pw-ann Work "$(ovpn)")"); ID=$(js "$o" j.id)
C=$(stat -c '%u %a' /persist/etc/vpn/$ID.conf 2>&1); o2=$(api GET /api/vpn)
ok "O3. added with Ann's password, root's 0600; listed as OpenVPN with its server" "$([ "$(code "$o")" = 200 ] && [ "$C" = '0 600' ] && [ "$(js "$o2" 'j.vpns[0].type+"|"+j.vpns[0].server')" = "openvpn|vpn.test:1194 (UDP)" ] && echo 1)" "$o / $C / $o2"
O_RSS0=$(pgrep -x openvpn | while read p; do [ "$p" = "$(cat $T/server.pid)" ] || echo $p; done)
ok "O4. off: no OpenVPN of this computer's running" "$([ -z "$O_RSS0" ] && echo 1)" "$O_RSS0"
# ---- connected
tcpdump -i wlan0 -n -U -w $T/cap.pcap > $T/tcpdump.log 2>&1 & TD=$!; PIDS="$PIDS $TD"; w8 "grep -q listening $T/tcpdump.log" 50
: > $T/dns-isp; : > $T/dns-vpn
o=$(api POST "/api/vpn/on?id=$ID")
OP=$(cat /run/rudolf/vpn/ovpn.pid 2>/dev/null); U=$(awk '/^Uid:/{print $2" "$3}' /proc/$OP/status 2>/dev/null); RSS=$(awk '/^VmRSS:/{print $2}' /proc/$OP/status 2>/dev/null)
ok "O5. Bob turns it on: connected; OpenVPN runs as nobody (its own process, no sandbox of the apps'), on rvpn0" "$([ "$(code "$o")" = 200 ] && [ "$(js "$o" j.state)" = connected ] && [ "$U" = "65534 65534" ] && ip link show rvpn0 >/dev/null 2>&1 && echo 1)" "$o / uid $U / $(tail -5 /run/rudolf/vpn/ovpn.log 2>/dev/null)"
echo "OpenVPN's memory while connected: ${RSS:-?} KB (VmRSS)"
ok "O6. all traffic through the tunnel: the internet sees the tunnel's address (10.67.0.x)" "$(who 203.0.113.5:8631 | grep -q '^10\.67\.0\.' && echo 1)" "$(who 203.0.113.5:8631) / $(tail -40 /run/rudolf/vpn/ovpn.log) / $(ip route)"
: > $T/dns-isp
ok "O7. name lookups through the VPN's name server (pushed by the server), the network's own not asked" "$([ "$(look who.test)" = 10.67.0.53 ] && [ ! -s $T/dns-isp ] && grep -qx 'nameserver 10.67.0.1' /etc/resolv.conf && echo 1)" "$(look who.test) / $(cat /etc/resolv.conf) / isp: $(cat $T/dns-isp)"
W6=$(who '[2001:db8::5]:8631' 2)
ok "O8. IPv6 does not go round it (the server gives none): unreachable while it is on" "$([ "$W6" = none ] && echo 1)" "$W6 $(ip -6 route)"
kill $TD 2>/dev/null; wait $TD 2>/dev/null
CAP=$(tcpdump -n -r $T/cap.pcap 2>/dev/null | grep -v -i 'ARP\|ICMP6\|IP6 fe80')
OUT=$(echo "$CAP" | grep -v '192.168.77.1.[0-9]* > 192.168.77.2.1194: UDP\|192.168.77.2.1194 > 192.168.77.1.[0-9]*: UDP\|192.168.77.1.[0-9]* > 192.168.77.2.53: \|192.168.77.2.53 > 192.168.77.1.[0-9]*: ' | grep -v '^$')
ok "O9. a capture on wlan0 while it was on: only the tunnel's packets (and the server's name looked up first)" "$([ "$(echo "$CAP" | grep -c '1194: UDP')" -ge 4 ] && [ -z "$OUT" ] && ! echo "$CAP" | grep -q '203.0.113.5\|who.test' && echo 1)" "other: $OUT / $(echo "$CAP" | head -5) / $(echo "$CAP" | grep -c "1194: UDP")"
o=$(api GET /api/vpn); L=$(cat $T/rd.log /run/rudolf/health.log /run/rudolf/events 2>/dev/null)
KEY=$(sed -n '2p' $P/client.key)
ok "O10. GET vpn: on, connected; the client's key and the tls-crypt key in no answer, log, page or OpenVPN's log" "$([ "$(js "$o" 'j.vpns[0].on+" "+j.vpns[0].state')" = "true connected" ] && ! echo "$o$L" | grep -qF "$KEY" && ! grep -qF "$KEY" /run/rudolf/vpn/ovpn.log && ! grep -qF "$(sed -n '5p' $P/tc.key)" /run/rudolf/vpn/ovpn.log && echo 1)" "$o"
# ---- the kill switch, then off
o=$(api POST /api/vpn/set "{\"n\":\"Ann\",\"p\":\"pw-ann\",\"id\":\"$ID\",\"kill\":true}"); KT=$(nft list table inet rudolf_vpnkill 2>&1)
kill -STOP $(cat $T/server.pid)   # (the server stops answering)
kill $OP; w8 "! kill -0 $OP 2>/dev/null" 50
a=$(who 203.0.113.5:8631 2); b=$(look who.test)
kill -CONT $(cat $T/server.pid)
ok "O11. the kill switch: OpenVPN gone while it is meant to be on, nothing goes out (the table lets out only the server, by UDP 1194)" "$([ "$(code "$o")" = 200 ] && echo "$KT" | grep -q 'ip daddr 192.168.77.2 udp dport 1194 accept' && [ "$a" = none ] && [ "$b" = none ] && echo 1)" "$o / $KT / $a / $b"
w8 "[ -s /run/rudolf/vpn/ovpn.pid ] && kill -0 \$(cat /run/rudolf/vpn/ovpn.pid) 2>/dev/null && grep -q 'Initialization Sequence Completed' /run/rudolf/vpn/ovpn.log" 900
ok "O12. ...started again by itself (vpn tick), connected again" "$(w8 "who 203.0.113.5:8631 | grep -q '^10\\.67\\.0\\.'" 100 && echo 1)" "$(tail -5 /run/rudolf/vpn/ovpn.log 2>/dev/null)"
o=$(api POST /api/vpn/off); OP2=$(cat /run/rudolf/vpn/ovpn.pid 2>/dev/null)
ok "O13. off: OpenVPN stopped (nothing of it in memory), rvpn0 gone, the kill switch's table gone, the network direct, its name servers back" "$([ "$(code "$o")" = 200 ] && [ -z "$(pgrep -x openvpn | grep -vx "$(cat $T/server.pid)")" ] && ! ip link show rvpn0 >/dev/null 2>&1 && ! nft list table inet rudolf_vpnkill >/dev/null 2>&1 && [ "$(who 203.0.113.5:8631)" = 192.168.77.1 ] && grep -qx 'nameserver 192.168.77.2' /etc/resolv.conf && echo 1)" "$o $(pgrep -a openvpn)"
# ---- a provider's login (auth-user-pass)
o=$(api POST /api/vpn/add "$(node -e "console.log(JSON.stringify({n:'Ann',p:'pw-ann',name:'Login',type:'openvpn',conf:process.argv[1]}))" "$(ovpn | sed 's/^nobind/nobind\nauth-user-pass/')")")
o2=$(api POST /api/vpn/add "$(node -e "console.log(JSON.stringify({n:'Ann',p:'pw-ann',name:'Login',type:'openvpn',user:'ann',pass:'secret-pw-77',conf:process.argv[1]}))" "$(ovpn | sed 's/^nobind/nobind\nauth-user-pass/')")"); I2=$(js "$o2" j.id)
A=$(stat -c '%u %a' /persist/etc/vpn/$I2.auth 2>&1)
ok "O14. a file that signs in with a username and password: asked for them; kept root's 0600, never in an answer" "$([ "$(code "$o")" = 409 ] && echo "$o" | grep -q 'username and password' && [ "$(code "$o2")" = 200 ] && [ "$A" = '0 600' ] && ! api GET /api/vpn | grep -q 'secret-pw-77' && echo 1)" "$o / $o2 / $A"
o=$(api POST /api/vpn/remove "{\"n\":\"Ann\",\"p\":\"pw-ann\",\"id\":\"$I2\"}")
ok "O15. removed: its file and its login gone" "$([ "$(code "$o")" = 200 ] && [ ! -e /persist/etc/vpn/$I2.auth ] && [ ! -e /persist/etc/vpn/$I2.conf ] && echo 1)" "$o"
echo "$((N-F))/$N passed"; [ $F = 0 ]
