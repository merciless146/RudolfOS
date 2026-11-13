#!/bin/sh
# 1.5.9 (plan Part 4): Share this connection (/usr/lib/rudolf/hotspot), with stand-ins for iw, ip, hostapd, hostapd_cli,
# dnsmasq, nft, wpa_cli, the firewall and wifi.sh. Checks: what the chip can do; sharing a cable or a phone's tethering
# with the Wi-Fi as the access point; sharing the Wi-Fi itself only with a chip that can do both; the access point's
# settings (WPA2/WPA3, the band); NAT and the firewall's opening; the password kept root-only; off puts everything
# back.   sh hotspot59_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); H=$HERE/../../sys/root/usr/lib/rudolf/hotspot; T=$(mktemp -d)
trap 'for p in $T/*.pid; do [ -s "$p" ] && kill $(cat $p) 2>/dev/null; done; for p in $(cat $T/pids 2>/dev/null); do kill $p 2>/dev/null; done; rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
B=$T/bin; mkdir -p $B; export ST=$T
cat > $B/iw <<'E'
#!/bin/sh
case "$*" in
"phy phy0 info") printf 'Wiphy phy0\n\tSupported interface modes:\n\t\t * managed\n\t\t * AP\n\t\t * monitor\n\tBand 1:\n\t\tFrequencies:\n'
	[ -e $ST/both ] && printf '\tvalid interface combinations:\n\t\t * #{ managed } <= 1, #{ AP } <= 1,\n\t\t   total <= 2, #channels <= 1\n\n'
	[ -e $ST/both ] || printf '\tvalid interface combinations:\n\t\t * #{ managed, AP } <= 1,\n\t\t   total <= 1, #channels <= 1\n\n' ;;
"dev wlan0 link") echo "Connected to bc:96:e5:88:1f:d9 (on wlan0)"; echo "	freq: 5220" ;;
*) echo "iw $*" >> $ST/log ;;
esac
E
printf '#!/bin/sh\ncase "$*" in "route show default") cat $ST/route;; *) echo "ip $*" >> $ST/log;; esac\n' > $B/ip
printf '#!/bin/sh\nwhile [ $# -gt 0 ]; do [ "$1" = -P ] && P=$2; C=$1; shift; done; cp "$C" $ST/hostapd.seen; sleep 300 >/dev/null 2>&1 & echo $! > "$P"; echo $! >> $ST/pids\n' > $B/hostapd
printf '#!/bin/sh\nfor a; do case "$a" in --pid-file=*) P=${a#--pid-file=};; esac; done; echo "dnsmasq $*" >> $ST/log; sleep 300 >/dev/null 2>&1 & echo $! > "$P"; echo $! >> $ST/pids\n' > $B/dnsmasq
printf '#!/bin/sh\necho "aa:bb:cc:dd:ee:01"; echo "aa:bb:cc:dd:ee:02"\n' > $B/hostapd_cli
printf '#!/bin/sh\n[ "$1" = -f ] && cp "$2" $ST/nat.seen; echo "nft $*" >> $ST/log\n' > $B/nft
printf '#!/bin/sh\necho "wpa_cli $*" >> $ST/log\n' > $B/wpa_cli
printf '#!/bin/sh\necho "firewall $*" >> $ST/log\n' > $T/fw; printf '#!/bin/sh\necho "wifi.sh $*" >> $ST/log\n' > $T/ws
chmod +x $B/* $T/fw $T/ws
NET=$T/net; mkdir -p $NET/wlan0/wireless $NET/wlan0/phy80211 $NET/usb0; echo phy0 > $NET/wlan0/phy80211/name
export RUDOLF_TEST_PATH=$B RUDOLF_WIFI_NET=$NET RUDOLF_HOTSPOT_RUN=$T/run RUDOLF_HOTSPOT_SAVED=$T/saved RUDOLF_HOTSPOT_LOG=$T/hlog RUDOLF_FIREWALL=$T/fw RUDOLF_WIFISH=$T/ws RUDOLF_IPFWD=$T/fwd
J() { sh $H status | python3 -c "import json,sys;d=json.load(sys.stdin);print(d$1)"; }
echo "default via 10.0.0.1 dev wlan0" > $T/route
ok "1. connected by Wi-Fi with a chip that cannot do both: it says so (connect by cable or a phone instead)" $([ "$(J "['canAP']")" = True ] && [ "$(J "['both']")" = False ] && J "['why']" | grep -q "cannot share the Wi-Fi it is connected to" && echo 1 || echo 0)
printf 'Rudolf hotspot\nsecret123\n2.4\n' > $T/req; o=$(sh $H start $T/req); ok "2. ...and refuses to start" $(echo "$o" | grep -q "cannot share" && [ ! -e $T/run/hostapd.pid ] && echo 1 || echo 0) "$o"
echo "default via 192.168.42.129 dev usb0" > $T/route; : > $T/log
printf 'Rudolf hotspot\nsecret123\n2.4\n' > $T/req; o=$(sh $H start $T/req)
ok "3. a phone's USB tethering: shared, with the Wi-Fi as the access point (its connection let go)" $(echo "$o" | grep -q "Sharing this connection as Rudolf hotspot" && grep -q "interface=wlan0" $T/hostapd.seen && grep -q "wpa_cli -p /run/rudolf/wpa -i wlan0 terminate" $T/log && echo 1 || echo 0) "$o $(cat $T/log)"
ok "4. ...WPA2 and WPA3 with the password, 2.4 GHz channel 6, the name as hex" $(grep -q "wpa_key_mgmt=WPA-PSK SAE" $T/hostapd.seen && grep -q "wpa_passphrase=secret123" $T/hostapd.seen && grep -q "hw_mode=g" $T/hostapd.seen && grep -q "channel=6" $T/hostapd.seen && grep -q "ssid2=$(printf 'Rudolf hotspot' | od -An -tx1 | tr -d ' \n')" $T/hostapd.seen && echo 1 || echo 0) "$(cat $T/hostapd.seen)"
ok "5. ...addresses handed out on 10.42.0.x, NAT out through usb0, forwarding on, the firewall lets DHCP and DNS in from it" $(grep -q "dnsmasq --interface=wlan0 --bind-interfaces" $T/log && grep -q 'oifname "usb0" masquerade' $T/nat.seen && [ "$(cat $T/fwd)" = 1 ] && grep -q "firewall hotspot wlan0" $T/log && echo 1 || echo 0) "$(cat $T/log)"
ok "6. status: on, its name, the devices connected, the connection shared" $([ "$(J "['on']")" = True ] && [ "$(J "['name']")" = "Rudolf hotspot" ] && [ "$(J "['devices']")" = 2 ] && [ "$(J "['uplink']")" = usb0 ] && echo 1 || echo 0) "$(sh $H status)"
ok "7. the password kept root-only for next time" $([ "$(stat -c %a $T/saved)" = 600 ] && grep -qx secret123 $T/saved && [ "$(J "['saved']")" = True ] && echo 1 || echo 0)
: > $T/log; o=$(sh $H stop)
ok "8. off: the access point and addresses stopped, NAT and the firewall's opening removed, the Wi-Fi given back" $(echo "$o" | grep -q "The hotspot is off" && grep -q "nft delete table ip rudolf_hotspot" $T/log && grep -q "firewall hotspot off" $T/log && grep -q "wifi.sh reconnect" $T/log && [ ! -e $T/run/hostapd.pid ] && echo 1 || echo 0) "$(cat $T/log)"
printf 'Rudolf hotspot\n\n5\n' > $T/req; sh $H start $T/req >/dev/null
ok "9. on again with the saved password (none typed), on 5 GHz" $(grep -q "wpa_passphrase=secret123" $T/hostapd.seen && grep -q "hw_mode=a" $T/hostapd.seen && echo 1 || echo 0) "$(cat $T/hostapd.seen)"
sh $H stop >/dev/null; touch $T/both; echo "default via 10.0.0.1 dev wlan0" > $T/route; : > $T/log
printf 'Rudolf hotspot\nsecret123\n2.4\n' > $T/req; o=$(sh $H start $T/req)
ok "10. a chip that can do both: the Wi-Fi itself shared, through a second interface on the connection's channel (5 GHz, 44)" $(grep -q "iw dev wlan0 interface add ap0 type __ap" $T/log && grep -q "interface=ap0" $T/hostapd.seen && grep -q "channel=44" $T/hostapd.seen && ! grep -q "terminate" $T/log && echo 1 || echo 0) "$o $(cat $T/log)"
sh $H stop >/dev/null
printf 'x\nshort\n2.4\n' > $T/req; o=$(sh $H start $T/req); ok "11. a password under 8 characters: refused" $(echo "$o" | grep -q "8 to 63" && echo 1 || echo 0) "$o"
echo "$((N-F))/$N passed"; [ $F = 0 ]
