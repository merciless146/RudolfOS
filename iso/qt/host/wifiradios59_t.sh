#!/bin/sh
# 1.5.9: Settings › Network › Wi-Fi › Radios: the radios of the network in use (2.4 / 5 GHz under one name), and turning
# one off or on for this computer (wifi.sh radios / radio). Off by hand stays off (wifi-event keeps it out from every
# join); a radio left out for not answering shows as such, and turning it on by hand lets it in again. Stand-ins for
# wpa_supplicant, wpa_cli, iw and a /sys/class/net. Needs root (a private /run).   sh wifiradios59_t.sh
[ "$(id -u)" = 0 ] && [ -z "$WAT_INNER" ] && exec env WAT_INNER=1 unshare -m sh -c 'mount -t tmpfs none /run && mkdir -p /run/rudolf && exec sh "$0"' "$0"
HERE=$(cd "$(dirname "$0")" && pwd); WS=$HERE/../../sys/root/usr/lib/rudolf/wifi.sh
T=$(mktemp -d); trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
B=$T/bin; mkdir -p $B; export ST=$T/st; mkdir -p $ST; LOG=$T/log; export LOG; echo bc:96:e5:88:1f:d9 > $ST/cur
cat > $B/wpa_supplicant <<'X'
#!/bin/sh
exit 0
X
cat > $B/wpa_cli <<'X'
#!/bin/sh
while [ $# -gt 0 ]; do case "$1" in -p|-i|-a|-P) shift 2;; -B) shift;; *) break;; esac; done
case "$1" in
ping) echo PONG ;;
status) printf 'wpa_state=COMPLETED\nid=0\nssid=SpectrumSetup-3E\nbssid=%s\n' "$(cat $ST/cur)" ;;
scan_results) printf 'bssid / frequency / signal level / flags / ssid\nbc:96:e5:88:1f:d9\t5220\t-53\t[WPA2-PSK-CCMP][ESS]\tSpectrumSetup-3E\nbc:96:e5:88:1f:da\t2412\t-60\t[WPA2-PSK-CCMP][ESS]\tSpectrumSetup-3E\nf6:18:df:e8:27:ef\t2462\t-44\t[WPA2-PSK][ESS]\tOther\n' ;;
list_networks) printf 'network id / ssid / bssid / flags\n0\tSpectrumSetup-3E\tany\t[CURRENT]\n' ;;
get_network) echo FAIL ;;
*) echo "wpa $*" >> $LOG; echo OK ;;
esac
X
printf '#!/bin/sh\nprintf "Wiphy phy0\\n\\tBand 1:\\n\\t\\tCapabilities: 0x1\\n\\tBand 2:\\n\\t\\tCapabilities: 0x1\\n"\n' > $B/iw
printf '#!/bin/sh\nexit 0\n' > $B/ip; printf '#!/bin/sh\nexit 1\n' > $B/mountpoint
chmod +x $B/*
NET=$T/sys/class/net; mkdir -p $NET/wlan0/wireless $NET/wlan0/phy80211; echo phy0 > $NET/wlan0/phy80211/name; echo f8:54:f6:2f:6a:1f > $NET/wlan0/address
export PATH=$B:$PATH RUDOLF_TEST_PATH=$B RUDOLF_WIFI_NET=$NET RUDOLF_WIFI_OFF=$T/off RUDOLF_WIFI_PI=$T/pi
run() { sh $WS "$@" wlan0 2>&1; }; run2() { sh $WS "$1" wlan0 "$2" "$3" 2>&1; }
o=$(run radios)
ok "1. this computer's bands: 2.4 and 5 GHz" "$(echo "$o" | grep -qx 'bands=2.4 5 ' && echo 1)" "$o"
ok "2. the network's two radios, the one in use marked; another network's not listed" "$(echo "$o" | grep -q "$(printf 'radio=bc:96:e5:88:1f:d9\t5220\t-53\tinuse')" && echo "$o" | grep -q "$(printf 'radio=bc:96:e5:88:1f:da\t2412\t-60\ton')" && ! echo "$o" | grep -q f6:18 && echo 1)" "$o"
H=$(printf %s SpectrumSetup-3E | od -An -tx1 | tr -d ' \n'); echo "$H bc:96:e5:88:1f:da $(date +%s)" > $T/pi
o=$(run radios)
ok "3. a radio left out for not answering shows when it is let in again" "$(echo "$o" | grep -q "$(printf 'bc:96:e5:88:1f:da\t2412\t-60\tfailed:')" && echo 1)" "$o"
o=$(run2 radio bc:96:e5:88:1f:da on)
ok "4. turned on by hand: no longer left out (the day's list), and the network told" "$(! grep -q 1f:da $T/pi && echo "$o" | grep -q 'turned on' && grep -q 'wpa set_network 0 bssid_ignore $' $LOG && echo 1)" "$o $(cat $LOG)"
: > $LOG; o=$(run2 radio bc:96:e5:88:1f:da off)
ok "5. turned off by hand: kept (network name in hex, the radio), and left out on the network" "$(grep -qx "$H bc:96:e5:88:1f:da" $T/off && grep -q 'wpa set_network 0 bssid_ignore bc:96:e5:88:1f:da$' $LOG && echo 1)" "$o $(cat $LOG)"
ok "6. ...and shows as off" "$(run radios | grep -q "$(printf 'bc:96:e5:88:1f:da\t2412\t-60\toff')" && echo 1)"
: > $LOG; o=$(run2 radio bc:96:e5:88:1f:d9 off)
ok "7. the radio in use turned off: it moves to another (reassociate)" "$(grep -q 'wpa reassociate' $LOG && echo 1)" "$(cat $LOG)"
o=$(run2 radio f6:18:df:e8:27:ef off)
ok "8. a radio of another network is refused" "$(echo "$o" | grep -q "not one of this network's" && echo 1)" "$o"
o=$(run2 radio 'x;rm' off)
ok "9. not a radio's address: refused" "$(echo "$o" | grep -q "Not a radio's address" && echo 1)" "$o"
# wifi-event keeps those turned off by hand out from every join
grep -q 'OFF=${WIFI_EVENT_OFF:-/persist/etc/wifi-radios-off}' $HERE/../../sys/root/usr/lib/rudolf/wifi-event && grep -q '(1.5.9: those turned off by hand stay off)' $HERE/../../sys/root/usr/lib/rudolf/wifi-event
ok "10. wifi-event keeps radios turned off by hand out (from every join, and when off the network)" "$([ $? = 0 ] && echo 1)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
