#!/bin/sh
# 1.6.0 (plan Part 4, stage A item 4): data and money, in network namespaces of its own: the stand-in ModemManager
# (../mmstub.c) with the real mmcli, a cellular link of TUN devices (fixtures/tunpair.py) to a far side. Checks: the
# cellular connection metered by itself; what it sends and receives counted from the interface's counters (netplug
# tick) per SIM, by the day; kept across a restart (the counters start again from 0); the month from the start day the
# person chose; the limit: a notice at 80 % (once a month), at 100 % mobile data off if chosen (not connected again by
# itself; Connect goes on and says so), or a notice only; the next month starts again.
# Needs root, python3, node, ping, dbus-daemon, mmcli, libmm-glib-dev, mobile-broadband-provider-info.   sh data60_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$D60_INNER" ] && exec env D60_INNER=1 unshare -m -n sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); SR=$HERE/../../sys/root/usr/lib/rudolf; M=$SR/modem; T=$(mktemp -d /tmp/data60.XXXXXX)
MBPI=/usr/share/mobile-broadband-provider-info/serviceproviders.xml
[ -r $MBPI ] && command -v ping >/dev/null && [ -e /dev/net/tun ] || { echo "SKIP: needs mobile-broadband-provider-info, ping and /dev/net/tun"; exit 0; }
MMSTUB_HERE=$HERE/x; . $HERE/mmstub.sh
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
PIDS=""; trap 'kill $PIDS 2>/dev/null; [ -s /run/rudolf/modem-watch.pid ] && kill $(cat /run/rudolf/modem-watch.pid) 2>/dev/null; mmstub_down; rm -rf $T' EXIT
w8() { i=0; while [ $i -lt ${2:-40} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
mount -t tmpfs none /run; mkdir -p /run/rudolf /run/dbus; mount -t sysfs none /sys; ip link set lo up
unshare -n sh -c "echo \$\$ > $T/far.pid; exec sleep 3600" & PIDS="$PIDS $!"; w8 "[ -s $T/far.pid ]"; FP=$(cat $T/far.pid); PIDS="$PIDS $FP"
far() { nsenter -t $FP -n sh -c "$*"; }
link() { rm -f $T/$1.ready; python3 $HERE/fixtures/tunpair.py $1 /proc/$FP/ns/net $2 $T/$1.ready & LP=$!; PIDS="$PIDS $LP"; w8 "[ -e $T/$1.ready ]"; }
link wwan0 cfar; far "ip link set lo up; ip addr add 203.0.113.1/32 dev lo; ip link set cfar up; ip addr add 10.64.12.33/32 dev cfar; ip route add 10.64.12.34/32 dev cfar"
mkdir -p $T/b $T/p $T/var; printf '#!/bin/sh\n[ "$1" = "^ModemManager\\$" ] && exit 0; exit 1\n' > $T/b/pgrep; chmod +x $T/b/pgrep
printf '#!/bin/sh\nexit 0\n' > $T/fw; chmod +x $T/fw
export RUDOLF_TEST_PATH=$T/b RUDOLF_MODEM_SYS=/sys RUDOLF_MODEM_PERSIST=$T/p RUDOLF_MODEM_LOG=$T/mlog RUDOLF_RUN=/run/rudolf RUDOLF_MBPI=$MBPI \
	RUDOLF_APN_FIXES=$SR/../../share/rudolf/apn-fixes RUDOLF_RESOLV=$T/resolv.conf RUDOLF_FIREWALL=$T/fw RUDOLF_NETUSE=$T/var/netusage RUDOLF_VAR=$T/var RUDOLF_ETC=$T/p RUDOLF_DAY=2026-11-16 RUDOLF_MONTH=2026-11
echo "add 0 iccid=8901260123456789012 iface=wwan0 ip4=10.64.12.34/30 gw4=10.64.12.33 dns=10.177.0.34 enabled=1" > $T/init
mmstub_up $T/mm $T/init || exit 0
gcc -O2 -w -DMODEM_SH="\"sh $M\"" -DMOBILE_RUN="\"/run/rudolf\"" -o $T/api $HERE/fixtures/mobileapi.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
node $HERE/fixtures/deskls.js $T/ls.json $T/keys.json || { echo "FAIL node"; exit 1; }
a() { $T/api $T/ls.json /run/rudolf "$@"; }
js() { printf '%s' "$1" | sed 's/^HTTP[^|]*|//' | node -e "let s='';process.stdin.on('data',d=>s+=d).on('end',()=>{try{const d=JSON.parse(s),u=d.usage||d;console.log(($2)?1:0)}catch(e){console.log(0)}})"; }
NP="sh $SR/netplug"
timeout 0.5 tail -f /dev/null; o=$(a "POST mobile/connect?id=0&on=1")
ok "1. connected; the cellular connection metered by itself (a modem's)" "$(echo "$o" | grep -q Connected && [ "$($NP is wwan0)" = 1 ] && [ "$($NP auto wwan0)" = modem ] && echo 1)" "$o $($NP is wwan0) $($NP auto wwan0)"
o=$($NP state); ok "...netplug's state: the connection in use is the modem's, metered, its key the SIM's (never its number)" "$(echo "$o" | grep -q '"if":"wwan0","kind":"[a-z]*","key":"sim:[0-9a-f]\{16\}"' && echo "$o" | grep -q '"metered":true' && ! echo "$o" | grep -q 8901260 && echo 1)" "$o"
K=$(cat /run/rudolf/cellkey/wwan0)
$NP tick; ping -c 60 -i 0.01 -s 8000 -W 2 203.0.113.1 >/dev/null 2>&1; $NP tick
B=$(( $(cat /sys/class/net/wwan0/statistics/rx_bytes) + $(cat /sys/class/net/wwan0/statistics/tx_bytes) ))
o=$(a "GET mobile/settings?id=0")
ok "2. what the link sent and received (about 1 MB) counted from its counters, by the day, for the SIM" "$(grep -q "^2026-11-16 $K [0-9]" $T/var/netusage && js "$o" "u.used>=900000&&Math.abs(u.used-$B)<4000&&u.start=='2026-11-01'&&u.next=='2026-12-01'&&u.limit==0" | grep -q 1 && echo 1)" "$B $(cat $T/var/netusage) $o"
U1=$(echo "$o" | sed 's/.*"used":\([0-9]*\).*/\1/')
# a restart: the link and its counters start again from 0, the run folder is empty again; the persistent count stays
kill $LP; w8 '! ip link show wwan0 >/dev/null 2>&1'; rm -rf /run/rudolf/netcount; link wwan0 cfar; ip link set wwan0 up; ip addr add 10.64.12.34/30 dev wwan0 2>/dev/null; ip route replace default via 10.64.12.33 dev wwan0 metric 700; far "ip link set cfar up; ip addr add 10.64.12.33/32 dev cfar; ip route replace 10.64.12.34/32 dev cfar"
$NP tick; ping -c 20 -i 0.01 -s 8000 -W 2 203.0.113.1 >/dev/null 2>&1; $NP tick
B2=$(( $(cat /sys/class/net/wwan0/statistics/rx_bytes) + $(cat /sys/class/net/wwan0/statistics/tx_bytes) )); o=$(a "GET mobile/settings?id=0")
ok "3. across a restart (the counters from 0 again): the month's count kept, what came after added" "$(js "$o" "$B2>300000&&Math.abs(u.used-($U1+$B2))<4000" | grep -q 1 && echo 1)" "$U1 + $B2: $o"
U3=$(echo "$o" | sed 's/.*"used":\([0-9]*\).*/\1/')
printf '2026-10-25 %s 3000000\n2026-10-10 %s 7000000\n2026-11-20 %s 0\n' "$K" "$K" "$K" >> $T/var/netusage
a "POST mobile/settings?id=0&k=startday&v=20" >/dev/null; o=$(a "GET mobile/settings?id=0")
ok "4. the month from the start day chosen (the 20th): from 20 October to 20 November" "$(js "$o" "u.startday==20&&u.start=='2026-10-20'&&u.next=='2026-11-20'&&u.used==$U3+3000000" | grep -q 1 && echo 1)" "$o"
o=$(a "POST mobile/settings?id=0&k=startday&v=31"); o2=$(a "POST mobile/settings?id=0&k=limit&v=-5"); ok "...(a day after the 28th, a limit that is not a number of MB: refused)" "$(printf '%s\n' "$o" "$o2" | grep -c '^HTTP/1.1 400' | grep -qx 2 && echo 1)" "$o $o2"
a "POST mobile/settings?id=0&k=startday&v=1" >/dev/null
# the limit: 10 MB; this month 8.5 MB
U=$(js "$(a 'GET mobile/settings?id=0')" "1" >/dev/null; a "GET mobile/settings?id=0" | sed 's/.*"used":\([0-9]*\).*/\1/'); printf '2026-11-02 %s %s\n' "$K" $((8500000 - U)) >> $T/var/netusage
a "POST mobile/settings?id=0&k=limit&v=10" >/dev/null; sh $M limits; o=$(a "GET mobile/notes?since=0")
ok "5. 85 % of a 10 MB limit: the notice (both desktops), once a month" "$(echo "$o" | grep -q '"t":"📶 85 % of this month.s mobile data used","b":"8.5 MB of 10 MB, until 2026-12-01."' && sh $M limits && [ "$(a 'GET mobile/notes?since=0' | grep -o '85 %' | wc -l)" = 1 ] && echo 1)" "$o"
o=$(a "GET mobile/settings?id=0"); ok "...the settings say how much (85 %)" "$(js "$o" "u.pct==85&&u.limit==10&&!u.dataoff&&!u.off")" "$o"
a "POST mobile/settings?id=0&k=dataoff&v=1" >/dev/null; printf '2026-11-03 %s 2000000\n' "$K" >> $T/var/netusage; : > $MMS_D/calls; sh $M limits
o=$(a "GET mobile/notes?since=0"); o2=$(a "GET mobile/settings?id=0")
ok "6. 100 % with \"Turn mobile data off at the limit\": disconnected, said (until when, Connect to go on, it may cost)" "$(grep -qx '0 Disconnect' $MMS_D/calls && ! ip route | grep -q wwan0 && echo "$o" | grep -q 'Mobile data is off: this month.s limit is reached' && echo "$o" | grep -q 'It comes back on 2026-12-01' && js "$o2" "u.off&&u.pct==105" | grep -q 1 && echo 1)" "$o $o2 $(mmstub_log)"
mmstub_cmd "set 0 reg=searching"; timeout 1 tail -f /dev/null; mmstub_cmd "set 0 reg=home"; : > $MMS_D/calls; sh $M watch & WP=$!; PIDS="$PIDS $WP"; timeout 9 tail -f /dev/null; kill $WP 2>/dev/null
ok "...not connected again by itself" "$(! grep -q 'Connect apn' $MMS_D/calls && echo 1)" "$(mmstub_log)"
o=$(a "POST mobile/connect?id=0&on=1"); o2=$(a "GET mobile/settings?id=0")
ok "...Connect by the person: connected (the limit no longer holds it off this month)" "$(echo "$o" | grep -q Connected && js "$o2" "!u.off" | grep -q 1 && echo 1)" "$o $o2"
a "POST mobile/settings?id=0&k=dataoff&v=0" >/dev/null; sed -i '/warned100/d' $(ls $T/p/mobile/*.conf); : > $MMS_D/calls; sh $M limits; o=$(a "GET mobile/notes?since=0")
ok "7. 100 % without it: only the notice; still connected" "$(echo "$o" | grep -q 'This month.s mobile data limit is reached' && ! grep -q Disconnect $MMS_D/calls && ip route | grep -q wwan0 && echo 1)" "$o"
o=$(RUDOLF_DAY=2026-12-02 a "GET mobile/settings?id=0")
ok "8. the next month: from 0 again" "$(js "$o" "u.start=='2026-12-01'&&u.used==0&&u.pct==0")" "$o"
echo "$((N-F))/$N passed"; [ $F = 0 ]
