#!/bin/sh
# 1.5.9 (found on the Vivobook, 2026-11-14): a wired or USB tethering connection takes the default route from a working
# Wi-Fi only once the internet answers through it (udhcpc.sh). A stand-in `ip` (a route and address table in files)
# and `ping` (answers through an interface when $W/online.IF exists); udhcpc.sh run as udhcpc runs it.   sh netroute59_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); UD=$HERE/../../sys/root/usr/lib/rudolf/udhcpc.sh
W=$(mktemp -d); trap 'kill $(cat $W/run/nointernet/*.pid 2>/dev/null) 2>/dev/null; rm -rf "$W"' EXIT; P=0; F=0
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
B=$W/bin; mkdir -p $B $W/run
cat > $B/ip <<EOF
#!/bin/sh
R=$W/routes; A=$W/addrs; touch \$R \$A
case "\$*" in
"route") cat \$R;;
"route del default dev "*) grep -v "^default .*dev \$5 " \$R > \$R.n; mv \$R.n \$R;;
"route add default via "*) echo "default via \$5 dev \$7 metric \$9 " >> \$R;;
"-4 -o addr show dev "*) grep "^\$6 " \$A | awk '{print "1: " \$1 "    inet " \$2 " brd x"}';;
"addr flush dev "*) grep -v "^\$4 " \$A > \$A.n; mv \$A.n \$A;;
"addr add "*) d=\$(echo "\$*" | sed 's/.* dev //'); echo "\$d \$3" >> \$A;;
"link set "*) ;;
*) echo "ip \$*" >> $W/other;;
esac
EOF
cat > $B/ping <<EOF
#!/bin/sh
while [ \$# -gt 0 ]; do [ "\$1" = -I ] && i=\$2; shift; done; [ -e $W/online.\$i ]
EOF
chmod +x $B/ip $B/ping
u() { env PATH=$B:$PATH RUDOLF_RUN=$W/run RUDOLF_RESOLV=$W/resolv RUDOLF_NETCHECK_EVERY=1 "$@" sh $UD $EV; }
def() { awk '/^default/{d="";m=0;for(i=1;i<NF;i++){if($i=="dev")d=$(i+1);if($i=="metric")m=$(i+1)}print m+0, d}' $W/routes | sort -n | head -1 | cut -d' ' -f2; }
# the Wi-Fi in use: 192.168.1.11 through the home router, its DNS
echo "default via 192.168.1.1 dev wlan0 metric 600 " > $W/routes; echo "wlan0 192.168.1.11/24" > $W/addrs; printf 'nameserver 192.168.1.1\n' > $W/run/dns.wlan0
mkdir -p $W/run/dns; mv $W/run/dns.wlan0 $W/run/dns/wlan0; cp $W/run/dns/wlan0 $W/resolv
# the MiFi without a data plan: the same network's addresses, no internet through it
EV=bound; u interface=eth9 ip=192.168.1.55 mask=24 router=192.168.1.1 dns=10.9.9.9
ok "1. a USB tethering link without internet while the Wi-Fi works: the Wi-Fi keeps the default route" $([ "$(def)" = wlan0 ]; echo $?) "$(cat $W/routes)"
ok "...the new link is behind it (metric 900), marked no internet" $(grep -q 'dev eth9 metric 900' $W/routes && [ -e $W/run/nointernet/eth9 ]; echo $?) "$(cat $W/routes)"
ok "...and the DNS stays the Wi-Fi's" $(grep -qx 'nameserver 192.168.1.1' $W/resolv && ! grep -q 10.9.9.9 $W/resolv; echo $?) "$(cat $W/resolv)"
# the data plan comes back: the link is tried again and takes over
: > $W/online.eth9; i=0; while [ $i -lt 30 ] && [ -e $W/run/nointernet/eth9 ]; do timeout 0.2 tail -f /dev/null; i=$((i+1)); done
ok "2. the internet answers through it later: it takes over (metric 100), the mark goes" $([ "$(def)" = eth9 ] && [ ! -e $W/run/nointernet/eth9 ]; echo $?) "$(cat $W/routes)"
ok "...with its DNS" $(grep -qx 'nameserver 10.9.9.9' $W/resolv; echo $?) "$(cat $W/resolv)"
# unplugged: the Wi-Fi again, with its DNS
EV=deconfig; u interface=eth9; sed -i '/dev eth9 /d' $W/routes; EV=deconfig; u interface=eth9
ok "3. unplugged: the Wi-Fi carries the default route again, with its DNS" $([ "$(def)" = wlan0 ] && grep -qx 'nameserver 192.168.1.1' $W/resolv; echo $?) "$(cat $W/routes) / $(cat $W/resolv)"
# a cable that works at once
EV=bound; u interface=eth9 ip=10.0.0.5 mask=24 router=10.0.0.1 dns=10.0.0.1
ok "4. a cable with internet while the Wi-Fi works: it takes over at once, as before" $([ "$(def)" = eth9 ] && grep -q 'dev eth9 metric 100' $W/routes && [ ! -e $W/run/nointernet/eth9 ]; echo $?) "$(cat $W/routes)"
EV=renew; u interface=eth9 ip=10.0.0.5 mask=24 router=10.0.0.1 dns=10.0.0.1
ok "...a renewal keeps it (one default route through it)" $([ "$(grep -c 'dev eth9' $W/routes)" = 1 ] && [ "$(def)" = eth9 ]; echo $?) "$(cat $W/routes)"
# alone: used at once, even before the internet answers (nothing to keep)
rm -f $W/online.eth9; EV=deconfig; u interface=eth9; : > $W/routes; : > $W/addrs
EV=bound; u interface=eth9 ip=10.0.0.5 mask=24 router=10.0.0.1 dns=10.0.0.1
ok "5. a cable alone (no Wi-Fi): used at once" $([ "$(def)" = eth9 ] && [ ! -e $W/run/nointernet/eth9 ]; echo $?) "$(cat $W/routes)"
# the checker ends when the link goes
echo "default via 192.168.1.1 dev wlan0 metric 600 " > $W/routes; echo "wlan0 192.168.1.11/24" > $W/addrs
EV=bound; u interface=eth8 ip=192.168.1.56 mask=24 router=192.168.1.1; p=$(cat $W/run/nointernet/eth8.pid 2>/dev/null)
EV=deconfig; u interface=eth8; timeout 1 tail -f /dev/null
ok "6. the link goes while it has no internet: its trying stops, its mark goes" $([ -n "$p" ] && { ! kill -0 $p 2>/dev/null || grep -q "^[0-9]* ([^)]*) Z" /proc/$p/stat; } && [ ! -e $W/run/nointernet/eth8 ]; echo $?) "pid $p"
ok "7. the marks are root's to write (folders 0755)" $([ "$(stat -c %a $W/run/nointernet)" = 755 ] && [ "$(stat -c %a $W/run/dns)" = 755 ]; echo $?) "$(stat -c %a $W/run/nointernet $W/run/dns)"
echo "$P/$((P+F)) passed"; [ $F = 0 ]
