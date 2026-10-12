#!/bin/bash
# Test disk for the Wi-Fi phase (upd_t.py F): hostapd (a simulated access point on a mac80211_hwsim radio)
# and its settings: WPA2/WPA3 mixed, WPA3 only, and DHCP for the test network.   mkwifi.sh OUT.img
set -e
I=$(cd "$(dirname "$0")/.." && pwd); D=$(mktemp -d)
cp $I/src/wifi/hostapd $D/
common='interface=wlan1
driver=nl80211
ctrl_interface=/run/hostapd
ssid=RudolfTest
hw_mode=g
channel=6
ieee80211n=1
wpa=2
rsn_pairwise=CCMP
wpa_passphrase=correct horse battery
'
printf '%swpa_key_mgmt=WPA-PSK SAE\nieee80211w=1\n' "$common" > $D/ap-mixed.conf
printf '%swpa_key_mgmt=SAE\nieee80211w=2\nsae_pwe=2\n' "$common" > $D/ap-wpa3.conf
printf 'interface wlan1\nstart 10.42.0.10\nend 10.42.0.50\nlease_file /tmp/udhcpd.leases\noption subnet 255.255.255.0\noption router 10.42.0.1\noption dns 10.42.0.1\n' > $D/udhcpd.conf
rm -f "$1"; truncate -s 32M "$1"; mkfs.ext4 -q -F -d $D "$1"; rm -rf $D; ls -la "$1"
