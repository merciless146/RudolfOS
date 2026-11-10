#!/bin/sh
# Wi-Fi for Rudolf OS (run by rudolfd as root, and at start-up). GPL-2.0-or-later.
#   wifi.sh boot                     start Wi-Fi when an adapter appears; join saved networks
#   wifi.sh status [IF]              state, current network, signal, address, saved networks
#   wifi.sh scan [IF]                networks in range (wpa_cli scan_results)
#   wifi.sh connect IF REQUEST-FILE  join: line 1 network name, line 2 password, line 3 open|owe|psk|hidden-psk|hidden-open
#   wifi.sh forget IF REQUEST-FILE   remove a saved network (line 1 network name)
#   wifi.sh disconnect|reconnect [IF]
#   wifi.sh country [IF] CC          set the country (which channels and power the adapter may use)
#   wifi.sh adapter IF               use this adapter from now on (1.5.3: a USB dongle next to the built-in Wi-Fi)
# With no IF: the adapter chosen in Settings when it is plugged in (saved by its MAC address, as a USB adapter's
# name changes with the port), otherwise the built-in one, otherwise the first. One adapter is used at a time.
# Saved networks live in /persist/etc/wifi.conf (passwords are stored there as keys, readable by root only).
export PATH=${RUDOLF_TEST_PATH:+$RUDOLF_TEST_PATH:}/usr/sbin:/usr/bin:/sbin:/bin
CTRL=/run/rudolf/wpa
# test hooks (iso/qt/host/wifiadapter_t.sh): RUDOLF_TEST_PATH, RUDOLF_WIFI_NET, RUDOLF_WIFI_SEL
NET=${RUDOLF_WIFI_NET:-/sys/class/net}
CONF=/run/rudolf/wifi.conf; mountpoint -q /persist 2>/dev/null && CONF=/persist/etc/wifi.conf
SEL=/run/rudolf/wifi-adapter; mountpoint -q /persist 2>/dev/null && SEL=/persist/etc/wifi-adapter
[ -n "$RUDOLF_WIFI_SEL" ] && SEL=$RUDOLF_WIFI_SEL
wlans() { for i in "$NET"/*; do [ -e "$i/wireless" ] && echo "${i##*/}"; done; }
mac_of() { cat "$NET/$1/address" 2>/dev/null; }
bus_of() { case "$(readlink -f "$NET/$1/device" 2>/dev/null)" in */usb*) echo usb;; "") echo other;; *) echo pci;; esac; }
saved_if() { M=$(cat "$SEL" 2>/dev/null); [ -n "$M" ] || return; for i in $(wlans); do [ "$(mac_of $i)" = "$M" ] && { echo $i; return; }; done; }
firstif() {
	S=$(saved_if); [ -n "$S" ] && { echo "$S"; return; }
	for i in $(wlans); do [ "$(bus_of $i)" != usb ] && { echo "$i"; return; }; done
	wlans | head -1
}
# the adapter's name in words: USB adapters by their maker and product, the others by their driver
name_of() {
	D=$(readlink -f "$NET/$1/device" 2>/dev/null)
	if [ "$(bus_of $1)" = usb ]; then echo "$(cat "$D/../manufacturer" 2>/dev/null) $(cat "$D/../product" 2>/dev/null)" | sed 's/^ *//; s/ *$//'; else basename "$(readlink "$D/driver" 2>/dev/null)" 2>/dev/null; fi
}
CMD=$1; IF=$2
case "$CMD" in country) [ -n "$3" ] || { CC=$2; IF=; } ;; esac
[ -n "$IF" ] || IF=$(firstif)
W() { wpa_cli -p $CTRL -i "$IF" "$@"; }
need() {
	[ -x "${RUDOLF_TEST_PATH:-/usr/sbin}/wpa_supplicant" ] || { echo "Wi-Fi tools are not part of this build."; exit 2; }
	[ -n "$IF" ] && [ -e "$NET/$IF/wireless" ] || { echo "No Wi-Fi adapter was found."; exit 3; }
}
start() {
	need
	if [ ! -s "$CONF" ]; then umask 077; printf 'ctrl_interface=%s\nupdate_config=1\nsae_pwe=2\n' $CTRL > "$CONF"; fi
	chmod 600 "$CONF"
	for r in /sys/class/rfkill/*; do [ "$(cat $r/type 2>/dev/null)" = wlan ] && echo 0 > $r/soft 2>/dev/null; done
	# Realtek rtw88 chips (RTL8821CE and others): their deep power saving misses the router's beacons, so a good
	# 5 GHz connection looks weak, wpa_supplicant tries the 2.4 GHz radio (which the Bluetooth half of the chip
	# shares), and the connection drops every few minutes. Ordinary power saving stays on.
	[ -w /sys/module/rtw88_core/parameters/disable_lps_deep ] && echo Y > /sys/module/rtw88_core/parameters/disable_lps_deep 2>/dev/null
	# 1.5.6: and their PCIe link power saving (ASPM), a known cause of dropped connections on the RTL8821CE (the
	# Vivobook's) and its relatives; /etc/modules/rtw88_pci sets it when the driver loads, this for one already loaded
	[ -w /sys/module/rtw88_pci/parameters/disable_aspm ] && echo Y > /sys/module/rtw88_pci/parameters/disable_aspm 2>/dev/null
	ip link set "$IF" up 2>/dev/null
	if ! W ping 2>/dev/null | grep -q PONG; then
		wpa_supplicant -B -s -i "$IF" -c "$CONF" -D nl80211 -P /run/rudolf/wpa-$IF.pid >/dev/null 2>&1
		for i in 1 2 3 4 5 6 7 8 9 10; do W ping 2>/dev/null | grep -q PONG && break; sleep 0.3; done
	fi
	# address by DHCP whenever a network is joined
	[ -f /run/rudolf/wpa-ev-$IF.pid ] && kill -0 "$(cat /run/rudolf/wpa-ev-$IF.pid)" 2>/dev/null || \
		W -a /usr/lib/rudolf/wifi-event -B -P /run/rudolf/wpa-ev-$IF.pid >/dev/null 2>&1
	W ping 2>/dev/null | grep -q PONG || { echo "The Wi-Fi service did not start."; exit 4; }
	bgscan_all
	# 1.5.3: one adapter at a time: the others stop (their address too), so a dongle and the built-in Wi-Fi do not
	# both join
	for o in $(wlans); do
		[ "$o" = "$IF" ] && continue
		wpa_cli -p $CTRL -i "$o" ping 2>/dev/null | grep -q PONG || continue
		wpa_cli -p $CTRL -i "$o" terminate >/dev/null 2>&1
		/usr/lib/rudolf/wifi-event "$o" DISCONNECTED now >/dev/null 2>&1
		ip link set "$o" down 2>/dev/null
	done
}
# 1.5.6: look for a better radio of the same network only when the signal is weak: below -70 dBm every 30 s, otherwise
# once an hour. On the Vivobook the router's 2.4 and 5 GHz radios (one network) took turns a few seconds apart, and a
# change that failed left it off the network for 30 s.
BGSCAN='"simple:30:-70:3600"'
bgscan_all() { W list_networks | tail -n +2 | cut -f1 | while read -r id; do
	[ -n "$id" ] && [ "$(W get_network "$id" bgscan)" != "$BGSCAN" ] && W set_network "$id" bgscan "$BGSCAN" >/dev/null; done; }
hexname() { printf '%s' "$1" | od -An -tx1 | tr -d ' \n'; }
# ids of saved networks with this name (list_networks shows names escaped; compare via the hex form)
ids_for() {
	H=$(hexname "$1")
	W list_networks | tail -n +2 | while IFS="$(printf '\t')" read -r id ssid rest; do
		[ "$(W get_network "$id" ssid | tr -d '"')" = "$1" ] && echo "$id" && continue
		[ "$(W get_network "$id" ssid)" = "$H" ] && echo "$id"
	done
}
case "$CMD" in
boot)
	[ -x "${RUDOLF_TEST_PATH:-/usr/sbin}/wpa_supplicant" ] || exit 0
	for i in $(seq 40); do IF=$(firstif); [ -n "$IF" ] && break; sleep 0.5; done
	# a chosen USB adapter can show up a few seconds after the built-in one
	if [ -s "$SEL" ] && [ -z "$(saved_if)" ]; then for i in $(seq 20); do [ -n "$(saved_if)" ] && { IF=$(saved_if); break; }; sleep 0.5; done; fi
	[ -n "$IF" ] && start >/dev/null 2>&1; exit 0 ;;
status)
	if [ ! -x "${RUDOLF_TEST_PATH:-/usr/sbin}/wpa_supplicant" ]; then echo "tools=no"; exit 0; fi
	[ -n "$IF" ] && [ -e "$NET/$IF/wireless" ] || { echo "adapter="; exit 0; }
	start >/dev/null
	echo "adapter=$IF"
	# 1.5.3: every adapter (name, MAC, usb|pci, driver, in words), the one chosen, and whether it is missing
	for i in $(wlans); do D=$(readlink -f "$NET/$i/device" 2>/dev/null); printf 'wlan=%s\t%s\t%s\t%s\t%s\n' "$i" "$(mac_of $i)" "$(bus_of $i)" "$(basename "$(readlink "$D/driver" 2>/dev/null)" 2>/dev/null)" "$(name_of $i)"; done
	[ -s "$SEL" ] && { echo "chosen=$(cat "$SEL")"; [ -z "$(saved_if)" ] && echo "chosen_missing=1"; }
	W status | grep -E '^(wpa_state|ssid|bssid|freq|key_mgmt|ip_address)='
	W signal_poll 2>/dev/null | grep -E '^(RSSI|LINKSPEED)='
	echo "country=$(W get country 2>/dev/null | grep -v FAIL)"
	W list_networks | tail -n +2 | sed 's/^/saved=/' ;;
scan)
	start
	W scan >/dev/null
	# results come in after a few seconds (longer while a scan is already running); stop early once there are some
	for i in $(seq 30); do sleep 0.5; [ $i -ge 4 ] && W scan_results | tail -n +2 | grep -q . && break; done
	W scan_results ;;
connect)
	start
	SSID=$(sed -n 1p "$3"); PSK=$(sed -n 2p "$3"); SEC=$(sed -n 3p "$3"); rm -f "$3"
	[ -n "$SSID" ] || { echo "No network name."; exit 1; }
	case "$SEC" in *psk) L=${#PSK}; if [ $L -lt 8 ] || [ $L -gt 63 ]; then echo "A Wi-Fi password has 8 to 63 characters."; exit 1; fi;; esac
	OLD=$(ids_for "$SSID")
	if [ "$SEC" = saved ]; then
		ID=$(echo $OLD | awk '{print $1}'); [ -n "$ID" ] || { echo "$SSID is not saved."; exit 1; }
		W select_network "$ID" >/dev/null
		for i in $(seq 120); do sleep 0.5; [ "$(W status | sed -n 's/^wpa_state=//p')" = COMPLETED ] && break; done
		W enable_network all >/dev/null
		[ "$(W status | sed -n 's/^wpa_state=//p')" = COMPLETED ] || { echo "Could not join $SSID (out of range, or its password changed: forget it and connect again)."; exit 1; }
		for i in $(seq 20); do ip -4 -o addr show dev "$IF" | grep -q inet && break; sleep 0.5; done
		echo "Connected to $SSID"; exit 0
	fi
	ID=$(W add_network | tail -1)
	W set_network "$ID" ssid "$(hexname "$SSID")" >/dev/null
	W set_network "$ID" bgscan "$BGSCAN" >/dev/null
	case "$SEC" in
	*psk) W set_network "$ID" key_mgmt "WPA-PSK WPA-PSK-SHA256 SAE" >/dev/null; W set_network "$ID" ieee80211w 1 >/dev/null
		W set_network "$ID" psk "\"$PSK\"" >/dev/null || { W remove_network "$ID" >/dev/null; echo "That password cannot be used."; exit 1; } ;;
	owe) W set_network "$ID" key_mgmt OWE >/dev/null; W set_network "$ID" ieee80211w 2 >/dev/null ;;
	*) W set_network "$ID" key_mgmt NONE >/dev/null ;;
	esac
	case "$SEC" in hidden*) W set_network "$ID" scan_ssid 1 >/dev/null ;; esac
	PSK=
	W select_network "$ID" >/dev/null
	# up to 60 seconds: the first join after the Wi-Fi service starts scans every channel first, and WPA3 (SAE) is
	# slow on small processors (1.2.5: 20 seconds was not always enough, and a join still under way was abandoned)
	OK=; WRONG=
	for i in $(seq 120); do
		sleep 0.5
		S=$(W status | sed -n 's/^wpa_state=//p')
		[ "$S" = COMPLETED ] && { OK=1; break; }
		[ "$S" = 4WAY_HANDSHAKE ] && WRONG=1
		# a rejected password makes wpa_supplicant set the network aside for a while
		W list_networks | awk -F'\t' -v id="$ID" '$1==id' | grep -q TEMP-DISABLED && { WRONG=1; break; }
	done
	if [ -z "$OK" ]; then
		W remove_network "$ID" >/dev/null; W enable_network all >/dev/null; W reassociate >/dev/null
		if [ -n "$WRONG" ]; then echo "Could not join $SSID: the password is wrong."; else echo "Could not join $SSID (out of range, or the password is wrong)."; fi
		exit 1
	fi
	for o in $OLD; do W remove_network "$o" >/dev/null; done
	W enable_network all >/dev/null
	W save_config >/dev/null
	for i in $(seq 20); do ip -4 -o addr show dev "$IF" | grep -q inet && break; sleep 0.5; done
	A=$(ip -4 -o addr show dev "$IF" | awk '{print $4}' | head -1)
	if [ -n "$A" ]; then echo "Connected to $SSID ($A)"; else echo "Joined $SSID, but no address yet (the network did not answer DHCP)."; fi ;;
forget)
	start
	SSID=$(sed -n 1p "$3"); rm -f "$3"; N=0
	for o in $(ids_for "$SSID"); do W remove_network "$o" >/dev/null; N=$((N+1)); done
	W save_config >/dev/null
	[ $N -gt 0 ] && echo "Forgot $SSID" || { echo "$SSID is not saved."; exit 1; } ;;
adapter)
	[ -n "$2" ] && [ -e "$NET/$2/wireless" ] || { echo "No such Wi-Fi adapter."; exit 1; }
	M=$(mac_of "$IF"); [ -n "$M" ] || { echo "No such Wi-Fi adapter."; exit 1; }
	umask 077; mkdir -p "$(dirname "$SEL")"; echo "$M" > "$SEL"
	start; W enable_network all >/dev/null; W reconnect >/dev/null
	echo "Using $(name_of "$IF") for Wi-Fi" ;;
disconnect) start; W disconnect >/dev/null; echo "Disconnected" ;;
reconnect) start; W enable_network all >/dev/null; W reconnect >/dev/null; echo "Reconnecting" ;;
country)
	start; CC=${CC:-$3}
	case "$CC" in [A-Z][A-Z]|00) ;; *) echo "Use a two-letter country code."; exit 1;; esac
	W set country "$CC" >/dev/null && W save_config >/dev/null && echo "Country set to $CC" ;;
*) echo "usage: wifi.sh boot|status|scan|connect|forget|disconnect|reconnect|country|adapter"; exit 1 ;;
esac
