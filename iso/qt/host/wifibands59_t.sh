#!/bin/sh
# 1.5.9: Settings › Network › Wi-Fi › This computer's Wi-Fi radios: the adapter's own bands (2.4 / 5 GHz), each on or
# off (wifi.sh bands / band). Off: kept by the adapter's MAC, applied as wpa_supplicant's freq_list (globally and for
# each network, a new one too), never saved into wifi.conf, re-applied when wpa_supplicant starts again; one radio
# always stays on; a router radio on a band that is off cannot be turned on from "Radios of this network"; nothing
# automatic writes the setting. Stand-ins for wpa_supplicant, wpa_cli, iw. Needs root (a private /run).
[ "$(id -u)" = 0 ] && [ -z "$WAT_INNER" ] && exec env WAT_INNER=1 unshare -m sh -c 'mount -t tmpfs none /run && mkdir -p /run/rudolf && exec sh "$0"' "$0"
HERE=$(cd "$(dirname "$0")" && pwd); R=$HERE/../../sys/root/usr/lib/rudolf; WS=$R/wifi.sh
T=$(mktemp -d); trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
B=$T/bin; mkdir -p $B; export ST=$T/st; mkdir -p $ST; LOG=$T/log; export LOG; echo 5220 > $ST/freq
printf '#!/bin/sh\nexit 0\n' > $B/wpa_supplicant
cat > $B/wpa_cli <<'X'
#!/bin/sh
while [ $# -gt 0 ]; do case "$1" in -p|-i|-a|-P) shift 2;; -B) shift;; *) break;; esac; done
case "$1" in
ping) echo PONG ;;
status) f=$(cat $ST/freq); b=bc:96:e5:88:1f:d9; [ "$f" = 2412 ] && b=bc:96:e5:88:1f:da; printf 'wpa_state=COMPLETED\nid=0\nssid=SpectrumSetup-3E\nbssid=%s\nfreq=%s\n' $b $f ;;
scan_results) printf 'bssid / frequency / signal level / flags / ssid\nbc:96:e5:88:1f:d9\t5220\t-53\t[WPA2-PSK-CCMP][ESS]\tSpectrumSetup-3E\nbc:96:e5:88:1f:da\t2412\t-60\t[WPA2-PSK-CCMP][ESS]\tSpectrumSetup-3E\n' ;;
list_networks) printf 'network id / ssid / bssid / flags\n0\tSpectrumSetup-3E\tany\t[CURRENT]\n1\tCafe\tany\t\n' ;;
get_network) echo FAIL ;;
add_network) echo 2 ;;
save_config) echo "wpa save_config" >> $LOG; echo OK ;;
*) echo "wpa $*" >> $LOG; echo OK ;;
esac
X
cat > $B/iw <<'X'
#!/bin/sh
cat <<E
Wiphy phy0
	Band 1:
		Capabilities: 0x1
		Bitrates (non-HT):
			* 1.0 Mbps
		Frequencies:
			* 2412.0 MHz [1] (20.0 dBm)
			* 2437.0 MHz [6] (20.0 dBm)
			* 2484.0 MHz [14] (disabled)
	Band 2:
		VHT Capabilities (0x00000000):
			short GI (80 MHz)
		Frequencies:
			* 5180.0 MHz [36] (20.0 dBm)
			* 5220.0 MHz [44] (20.0 dBm)
E
X
printf '#!/bin/sh\nexit 0\n' > $B/ip; printf '#!/bin/sh\nexit 1\n' > $B/mountpoint; chmod +x $B/*
NET=$T/sys/class/net; mkdir -p $NET/wlan0/wireless $NET/wlan0/phy80211; echo phy0 > $NET/wlan0/phy80211/name; echo f8:54:f6:2f:6a:1f > $NET/wlan0/address
export PATH=$B:$PATH RUDOLF_TEST_PATH=$B RUDOLF_WIFI_NET=$NET RUDOLF_WIFI_OFF=$T/off RUDOLF_WIFI_PI=$T/pi RUDOLF_WIFI_BANDS=$T/boff
w() { sh $WS "$@" 2>&1; }
o=$(w bands wlan0)
ok "1. this computer's radios: 2.4 and 5 GHz, both on; the adapter named" "$(echo "$o" | grep -qx "$(printf 'band=2.4\ton')" && echo "$o" | grep -qx "$(printf 'band=5\ton')" && echo "$o" | grep -q "^adapter=wlan0	f8:54:f6:2f:6a:1f" && echo 1)" "$o"
ok "2. with every radio on, no frequency limit is set" "$(! grep -q 'freq_list [0-9]' $LOG && echo 1)" "$(cat $LOG)"
: > $LOG; o=$(w band wlan0 5 off)
ok "3. the 5 GHz radio turned off: kept by the adapter's MAC" "$(grep -qx 'f8:54:f6:2f:6a:1f 5' $T/boff && echo 1)" "$(cat $T/boff)"
ok "4. ...applied: wpa_supplicant scans and joins 2.4 GHz only (globally and for each saved network)" "$(grep -qx 'wpa set freq_list 2412 2437' $LOG && grep -qx 'wpa set_network 0 freq_list 2412 2437' $LOG && grep -qx 'wpa set_network 1 freq_list 2412 2437' $LOG && echo 1)" "$(cat $LOG)"
ok "5. ...a disabled channel (14) is never listed; the connection on 5 GHz moves" "$(! grep -q 2484 $LOG && grep -q 'wpa reassociate' $LOG && echo "$o" | grep -q 'moving to a radio of the network on 2.4 GHz' && echo 1)" "$o"
ok "6. ...logged as an administrator's change" "$(grep -q "this computer's 5 GHz radio turned off by an administrator" /run/rudolf/wifi.log && echo 1)"
o=$(w bands wlan0); ok "7. bands shows it off" "$(echo "$o" | grep -qx "$(printf 'band=5\toff')" && echo 1)" "$o"
o=$(w band wlan0 2.4 off)
ok "8. the last radio on cannot be turned off" "$(echo "$o" | grep -q 'has to stay on' && grep -qx 'f8:54:f6:2f:6a:1f 5' $T/boff && [ $(wc -l < $T/boff) = 1 ] && echo 1)" "$o"
echo 2412 > $ST/freq; o=$(w radios wlan0)
ok "9. the network's radios: the 5 GHz one shows as off on this computer's side" "$(echo "$o" | grep -qx 'bandsoff=5 ' && echo "$o" | grep -q "$(printf 'bc:96:e5:88:1f:d9\t5220\t-53\tbandoff')" && echo 1)" "$o"
o=$(w radio wlan0 bc:96:e5:88:1f:d9 on)
ok "10. ...and cannot be turned on from there (only the administrator's setting)" "$(echo "$o" | grep -q "5 GHz radio is turned off" && echo 1)" "$o"
: > $LOG; sh $WS forget wlan0 /dev/null >/dev/null 2>&1; printf 'Cafe\n' > $T/req; w forget wlan0 $T/req >/dev/null
ok "11. saving wifi.conf: the limit is taken off while saving and put back (the setting lives only in its own file)" "$(awk '/freq_list $/{a=NR} /save_config/{s=NR} /freq_list 2412 2437/{p=NR} END{exit !(a && s>a && p>s)}' $LOG && echo 1)" "$(cat $LOG)"
: > $LOG; w status wlan0 >/dev/null
ok "12. not set again on every status read" "$(! grep -q 'freq_list' $LOG && echo 1)" "$(cat $LOG)"
rm -f /run/rudolf/wifi-bands-wlan0; : > $LOG; w status wlan0 >/dev/null
ok "13. wpa_supplicant started again: set again" "$(grep -qx 'wpa set freq_list 2412 2437' $LOG && echo 1)" "$(cat $LOG)"
: > $LOG; printf 'New\npassword123\npsk\n' > $T/req; w connect wlan0 $T/req >/dev/null
ok "14. a new network joined gets the limit too" "$(grep -qx 'wpa set_network 2 freq_list 2412 2437' $LOG && echo 1)" "$(cat $LOG)"
o=$(w band wlan0 5 on)
ok "15. turned on again: no limit, the file has no line for this adapter" "$(echo "$o" | grep -q '5 GHz radio turned on' && ! grep -q f8:54 $T/boff && grep -q 'wpa set freq_list $' $LOG && echo 1)" "$o"
o=$(w band wlan0 6 off); ok "16. a band this computer does not have: refused" "$(echo "$o" | grep -q 'has no 6 GHz radio' && echo 1)" "$o"
ok "17. nothing automatic writes the setting (only wifi.sh band)" "$(! grep -sl 'wifi-bands-off\|RUDOLF_WIFI_BANDS\|BOFF' $R/* | grep -v '/wifi.sh$' | grep -q . && [ $(grep -c '> "\$BOFF.n"' $WS) = 1 ] && echo 1)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
