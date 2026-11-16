#!/bin/sh
# 1.6.0 (plan Part 3, stage A item 3): the connection, in network namespaces of its own (nothing of this computer's
# network is touched): the stand-in ModemManager (../mmstub.c) with the real mmcli; a "cellular" link (wwan0, raw IP as
# a modem's) and a "Wi-Fi" link (wlan0) to a far side that holds "the internet" (203.0.113.1), both made of TUN devices
# (fixtures/tunpair.py). Checks: connect through rudolfd's mobile/connect (the SIM's APN, roaming not allowed), the
# interface set up as the modem says (address, default route behind Wi-Fi's, DNS), the firewall's cellular set; Wi-Fi
# in use while it works; Wi-Fi taken down: traffic on cellular within seconds, with the notice (rudolfd's mobile/notes),
# and back; Disconnect keeps it off, Connect again; connected again by itself after the network drops it; not while
# roaming unless allowed; the APN refused in the wrong capitals, said; "Connect automatically" off; IPv6-only with a
# CLAT (tayga) through a NAT64 on the far side (where this kernel has IPv6 and tayga is there).
# Needs root, python3, node, nft, dbus-daemon, mmcli, libmm-glib-dev, mobile-broadband-provider-info; tayga for the
# CLAT.   sh conn60_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$C60_INNER" ] && exec env C60_INNER=1 unshare -m -n sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); SR=$HERE/../../sys/root/usr/lib/rudolf; M=$SR/modem; T=$(mktemp -d /tmp/conn60.XXXXXX)
MBPI=/usr/share/mobile-broadband-provider-info/serviceproviders.xml
[ -r $MBPI ] && command -v nft >/dev/null && command -v ping >/dev/null && [ -e /dev/net/tun ] || { echo "SKIP: needs mobile-broadband-provider-info, nft, ping and /dev/net/tun"; exit 0; }
MMSTUB_HERE=$HERE/x; . $HERE/mmstub.sh
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
PIDS=""; trap 'kill $PIDS 2>/dev/null; [ -s /run/rudolf/modem-watch.pid ] && kill $(cat /run/rudolf/modem-watch.pid) 2>/dev/null; [ -s /run/rudolf/clat/tayga.pid ] && kill $(cat /run/rudolf/clat/tayga.pid) 2>/dev/null; mmstub_down; rm -rf $T' EXIT
w8() { i=0; while [ $i -lt ${2:-40} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
mount -t tmpfs none /run; mkdir -p /run/rudolf /run/dbus; ip link set lo up
# the far side: its own namespace (a process that waits), "the internet" on its loopback
unshare -n sh -c "echo \$\$ > $T/far.pid; exec sleep 3600" & PIDS="$PIDS $!"; w8 "[ -s $T/far.pid ]"; FP=$(cat $T/far.pid); PIDS="$PIDS $FP"
far() { nsenter -t $FP -n sh -c "$*"; }
far "ip link set lo up; ip addr add 203.0.113.1/32 dev lo; echo 1 > /proc/sys/net/ipv4/ip_forward"
link() { python3 $HERE/fixtures/tunpair.py $1 /proc/$FP/ns/net $2 $T/$1.ready & PIDS="$PIDS $!"; w8 "[ -e $T/$1.ready ]"; }
link wwan0 cfar; link wlan0 wfar
far "ip link set cfar up; ip addr add 10.64.12.33/32 dev cfar; ip route add 10.64.12.34/32 dev cfar; ip link set wfar up; ip addr add 192.168.50.1/32 dev wfar; ip route add 192.168.50.2/32 dev wfar"
# Wi-Fi as udhcpc.sh leaves it (metric 600)
wifi_up() { ip link set wlan0 up; ip addr add 192.168.50.2/24 dev wlan0 2>/dev/null; ip route replace default via 192.168.50.1 dev wlan0 metric 600; }
wifi_up
mkdir -p $T/b $T/p $T/sys; printf '#!/bin/sh\n[ "$1" = "^ModemManager\\$" ] && exit 0; exit 1\n' > $T/b/pgrep; chmod +x $T/b/pgrep
export RUDOLF_TEST_PATH=$T/b RUDOLF_MODEM_SYS=$T/sys RUDOLF_MODEM_PERSIST=$T/p RUDOLF_MODEM_LOG=$T/mlog RUDOLF_RUN=/run/rudolf RUDOLF_MBPI=$MBPI \
	RUDOLF_APN_FIXES=$SR/../../share/rudolf/apn-fixes RUDOLF_RESOLV=$T/resolv.conf RUDOLF_FIREWALL=$T/fw RUDOLF_RFKILL=$T/rf
sed "s#/persist/etc#$T/p#" $SR/firewall > $T/fw; chmod +x $T/fw; $T/fw start || echo "firewall did not load"
echo "add 0 iccid=8901260123456789012 iface=wwan0 ip4=10.64.12.34/30 gw4=10.64.12.33 dns=10.177.0.34 ip6= enabled=1" > $T/init
mmstub_up $T/mm $T/init || exit 0
gcc -O2 -w -DMODEM_SH="\"sh $M\"" -DMOBILE_RUN="\"/run/rudolf\"" -o $T/api $HERE/fixtures/mobileapi.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
node $HERE/fixtures/deskls.js $T/ls.json $T/keys.json || { echo "FAIL node"; exit 1; }
a() { $T/api $T/ls.json /run/rudolf "$@"; }
via() { ip route get 203.0.113.1 2>/dev/null | sed -n 's/.* dev \([^ ]*\).*/\1/p' | head -1; }
reach() { ping -c1 -W2 203.0.113.1 >/dev/null 2>&1; }
notes() { a "GET mobile/notes?since=${1:-0}"; }
timeout 0.5 tail -f /dev/null
o=$(a "POST mobile/connect?id=0&on=1")
ok "1. Connect (rudolfd's mobile/connect): ModemManager asked with the SIM's APN (T-Mobile's), roaming not allowed" "$(echo "$o" | grep -q '^HTTP/1.1 200.*Connected' && grep -qx '0 Connect apn=fast.t-mobile.com ip-type=ipv4v6 allow-roaming=0' $MMS_D/calls && echo 1)" "$o $(mmstub_log)"
r=$(ip route)
ok "2. ...the interface set up as the modem says: its address, the default route behind Wi-Fi's (700 against 600), its DNS" "$(ip -4 -o addr show dev wwan0 | grep -q 'inet 10.64.12.34/30' && echo "$r" | grep -q '^default via 10.64.12.33 dev wwan0 .*metric 700' && grep -qx 'nameserver 10.177.0.34' /run/rudolf/dns/wwan0 && [ "$(cat /run/rudolf/cellular/wwan0)" = 0 ] && echo 1)" "$r $(ip addr show dev wwan0)"
ok "3. ...the firewall filters it as every other interface, and never counts it as the local network (remote access)" "$(nft list set inet rudolf cellular | grep -q '"wwan0"' && nft list chain inet rudolf remote | grep -q 'iifname @cellular return' && echo 1)" "$(nft list table inet rudolf 2>&1 | head -30)"
ok "4. Wi-Fi works: the traffic goes through Wi-Fi, cellular stays connected but unused" "$([ "$(via)" = wlan0 ] && reach && echo 1)" "$(via) $(ip route)"
sh $M watch & PIDS="$PIDS $!"; w8 "[ -s /run/rudolf/modem-watch.pid ]"; timeout 3 tail -f /dev/null
L=$(notes | sed -n 's/.*"last":\([0-9]*\).*/\1/p')
t0=$(date +%s); ip link set wlan0 down; w8 '[ "$(via)" = wwan0 ]' 40; t1=$(date +%s)
ok "5. Wi-Fi taken down: the traffic on cellular at once ($((t1 - t0)) s), the internet reached through it" "$([ "$(via)" = wwan0 ] && [ $((t1 - t0)) -le 3 ] && reach && echo 1)" "$(via) $(ip route)"
w8 "notes $L | grep -q 'Using mobile data'" 40; o=$(notes $L)
ok "...and the notice for both desktops (mobile/notes): mobile data carries the traffic, Wi-Fi gone, metered" "$(echo "$o" | grep -q '"t":"📶 Using mobile data","b":"The Wi-Fi connection is gone: mobile data carries the traffic now (metered' && echo 1)" "$o"
o=$(sh $M status); ok "...status says so (the route in use, cellular)" "$(echo "$o" | grep -q '"route":"wwan0","cellular":true' && echo "$o" | grep -q '"iface":"wwan0"' && echo 1)" "$o"
L=$(notes | sed -n 's/.*"last":\([0-9]*\).*/\1/p'); wifi_up; w8 '[ "$(via)" = wlan0 ]' 20
ok "6. Wi-Fi back: the traffic on Wi-Fi again" "$([ "$(via)" = wlan0 ] && reach && echo 1)" "$(via)"
w8 "notes $L | grep -q 'Back on Wi-Fi'" 40; o=$(notes $L)
ok "...noted (mobile data stays connected, unused)" "$(echo "$o" | grep -q '"t":"📶 Back on Wi-Fi"' && ip route | grep -q 'dev wwan0 .*metric 700' && echo 1)" "$o"
: > $MMS_D/calls; o=$(a "POST mobile/connect?id=0&on=0"); timeout 8 tail -f /dev/null
ok "7. Disconnect: ModemManager told, the interface undone, out of the firewall's set; not connected again by itself" "$(echo "$o" | grep -q 'Disconnected' && grep -qx '0 Disconnect' $MMS_D/calls && ! grep -q Connect\ apn $MMS_D/calls && ! ip route | grep -q wwan0 && ! ip -4 -o addr show dev wwan0 | grep -q inet && ! nft list set inet rudolf cellular | grep -q wwan0 && echo 1)" "$o $(mmstub_log) $(ip route)"
o=$(a "GET mobile/settings?id=0"); ok "...the settings say it is off until Connect" "$(echo "$o" | grep -q '"off":true' && echo 1)" "$o"
o=$(a "POST mobile/connect?id=0&on=1"); ok "8. Connect again: connected" "$(echo "$o" | grep -q Connected && ip route | grep -q 'dev wwan0 .*metric 700' && echo 1)" "$o"
: > $MMS_D/calls; mmstub_cmd "set 0 reg=searching"; timeout 2 tail -f /dev/null; mmstub_cmd "set 0 reg=home"
w8 "grep -q 'Connect apn' $MMS_D/calls" 60
ok "9. the network drops the connection, then the modem is back on it: connected again by itself" "$(grep -q '0 Connect apn=fast.t-mobile.com' $MMS_D/calls && w8 'ip route | grep -q "dev wwan0 .*metric 700"' 20 && echo 1)" "$(mmstub_log) $(ip route)"
a "POST mobile/connect?id=0&on=0" >/dev/null; mmstub_cmd "set 0 reg=roaming opname=Telcel mccmnc=334020"; o=$(a "POST mobile/connect?id=0&on=1")
ok "10. roaming (abroad): not connected, said (roaming is off by default)" "$(echo "$o" | grep -q '^HTTP/1.1 400.*mobile data is off while roaming' && echo 1)" "$o"
a "POST mobile/settings?id=0&k=roaming&v=1" >/dev/null; : > $MMS_D/calls; o=$(a "POST mobile/connect?id=0&on=1")
ok "...roaming allowed: connected, ModemManager told roaming is allowed" "$(echo "$o" | grep -q Connected && grep -q 'allow-roaming=1' $MMS_D/calls && echo 1)" "$o $(mmstub_log)"
a "POST mobile/connect?id=0&on=0" >/dev/null; mmstub_cmd "set 0 reg=home apn=PWG"; a "POST mobile/settings?id=0&k=apn&v=pwg" >/dev/null; o=$(a "POST mobile/connect?id=0&on=1")
ok "11. an APN the network refuses in lowercase (US Mobile's): said, pointing at the capitals" "$(echo "$o" | grep -q 'The network refused the APN (pwg)' && echo "$o" | grep -q 'capitals' && echo 1)" "$o"
a "POST mobile/settings?id=0&k=apn&v=PWG" >/dev/null; o=$(a "POST mobile/connect?id=0&on=1")
ok "...typed as PWG: connected" "$(echo "$o" | grep -q Connected && echo 1)" "$o"
a "POST mobile/settings?id=0&k=autoconnect&v=0" >/dev/null; : > $MMS_D/calls; mmstub_cmd "set 0 reg=searching"; timeout 2 tail -f /dev/null; mmstub_cmd "set 0 reg=home"; timeout 10 tail -f /dev/null
ok "12. Connect automatically turned off: after a drop it stays off" "$(! grep -q 'Connect apn' $MMS_D/calls && echo 1)" "$(mmstub_log)"
o=$(a "POST mobile/connect?id=7&on=1"); o2=$(a "POST mobile/connect?id=0&on=2"); o3=$(a "GET mobile/connect?id=0&on=1")
ok "...(a modem that is not there, on= other than 1 or 0, a GET: refused)" "$(echo "$o" | grep -q '^HTTP/1.1 400' && echo "$o2" | grep -q '^HTTP/1.1 400' && echo "$o3" | grep -q '^HTTP/1.1 405' && echo 1)" "$o | $o2 | $o3"
# IPv6 only, with a CLAT through the far side's NAT64
if [ ! -e /proc/sys/net/ipv6 ] || ! command -v tayga >/dev/null; then echo "SKIP 13-14. IPv6-only with a CLAT: this kernel has no IPv6 or tayga is not installed"
else
	link wwan1 cfar1
	far "ip link set cfar1 up; ip -6 addr add 2001:db8:1::1/64 dev cfar1; ip -6 route replace 2001:db8:1::/64 dev cfar1; echo 1 > /proc/sys/net/ipv6/conf/all/forwarding
		printf 'tun-device nat64\nipv4-addr 192.168.255.1\nprefix 64:ff9b::/96\ndynamic-pool 192.168.255.0/24\ndata-dir $T/nat64\n' > $T/nat64.conf; mkdir -p $T/nat64
		tayga -c $T/nat64.conf --mktun >/dev/null; ip link set nat64 up; ip route add 192.168.255.0/24 dev nat64; ip -6 route add 64:ff9b::/96 dev nat64; tayga -c $T/nat64.conf -p $T/nat64.pid"
	PIDS="$PIDS $(cat $T/nat64.pid 2>/dev/null)"
	mmstub_cmd "add 1 iccid=8901260555555555555 iface=wwan1 ip4= ip6=2001:db8:1::2/64 gw6=2001:db8:1::1 enabled=1"
	a "POST mobile/connect?id=0&on=0" >/dev/null; ip link set wlan0 down
	o=$(a "POST mobile/connect?id=1&on=1"); w8 '[ "$(via)" = clat ]' 20
	ok "13. an IPv6-only network: connected with its IPv6 address, and a CLAT (464XLAT) carries IPv4" "$(echo "$o" | grep -q Connected && ip -6 addr show dev wwan1 | grep -q '2001:db8:1::2/64' && [ "$(via)" = clat ] && grep -q '^map 192.0.0.1 2001:db8:1:0::c1a7' /run/rudolf/clat/tayga.conf && echo 1)" "$o $(ip route) $(ip -6 route) $(cat /run/rudolf/clat/tayga.conf 2>&1)"
	ok "14. ...an IPv4 site reached through the operator's NAT64 (the well-known prefix, none found in its DNS)" "$(w8 reach 20 && grep -q '^prefix 64:ff9b::/96' /run/rudolf/clat/tayga.conf && echo 1)" "$(ping -c1 -W2 203.0.113.1 2>&1 | tail -2) $(far 'ip route; ip -6 route' 2>&1)"
	a "POST mobile/connect?id=1&on=0" >/dev/null
	ok "...disconnected: the CLAT goes too" "$(! ip link show clat >/dev/null 2>&1 && [ ! -e /run/rudolf/clat ] && echo 1)" "$(ip link)"
fi
echo "$((N-F))/$N passed"; [ $F = 0 ]
