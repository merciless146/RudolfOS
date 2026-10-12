#!/bin/sh
# Wi-Fi for Rudolf OS (run by rudolfd as root, and at start-up). GPL-2.0-or-later.
#   wifi.sh boot                     start Wi-Fi when an adapter appears; join saved networks
#   wifi.sh status [IF]              state, current network, signal, address, saved networks
#   wifi.sh scan [IF]                networks in range (wpa_cli scan_results)
#   wifi.sh connect IF REQUEST-FILE  join: line 1 network name, line 2 password, line 3 open|owe|psk|hidden-psk|hidden-open
#   wifi.sh forget IF REQUEST-FILE   remove a saved network (line 1 network name)
#   wifi.sh disconnect|reconnect [IF]
#   wifi.sh country [IF] CC          set the country (which channels and power the adapter may use)
# Saved networks live in /persist/etc/wifi.conf (passwords are stored there as keys, readable by root only).
export PATH=/usr/sbin:/usr/bin:/sbin:/bin
CTRL=/run/rudolf/wpa
CONF=/run/rudolf/wifi.conf; mountpoint -q /persist 2>/dev/null && CONF=/persist/etc/wifi.conf
firstif() { for i in /sys/class/net/*; do [ -e "$i/wireless" ] && { echo "${i##*/}"; return; }; done; }
CMD=$1; IF=$2
case "$CMD" in country) [ -n "$3" ] || { CC=$2; IF=; } ;; esac
[ -n "$IF" ] || IF=$(firstif)
W() { wpa_cli -p $CTRL -i "$IF" "$@"; }
need() {
	[ -x /usr/sbin/wpa_supplicant ] || { echo "Wi-Fi tools are not part of this build."; exit 2; }
	[ -n "$IF" ] && [ -e "/sys/class/net/$IF/wireless" ] || { echo "No Wi-Fi adapter was found."; exit 3; }
}
start() {
	need
	if [ ! -s "$CONF" ]; then umask 077; printf 'ctrl_interface=%s\nupdate_config=1\nsae_pwe=2\n' $CTRL > "$CONF"; fi
	chmod 600 "$CONF"
	for r in /sys/class/rfkill/*; do [ "$(cat $r/type 2>/dev/null)" = wlan ] && echo 0 > $r/soft 2>/dev/null; done
	ip link set "$IF" up 2>/dev/null
	if ! W ping 2>/dev/null | grep -q PONG; then
		wpa_supplicant -B -s -i "$IF" -c "$CONF" -D nl80211 -P /run/rudolf/wpa-$IF.pid >/dev/null 2>&1
		for i in 1 2 3 4 5 6 7 8 9 10; do W ping 2>/dev/null | grep -q PONG && break; sleep 0.3; done
	fi
	# address by DHCP whenever a network is joined
	[ -f /run/rudolf/wpa-ev-$IF.pid ] && kill -0 "$(cat /run/rudolf/wpa-ev-$IF.pid)" 2>/dev/null || \
		W -a /usr/lib/rudolf/wifi-event -B -P /run/rudolf/wpa-ev-$IF.pid >/dev/null 2>&1
	W ping 2>/dev/null | grep -q PONG || { echo "The Wi-Fi service did not start."; exit 4; }
}
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
	[ -x /usr/sbin/wpa_supplicant ] || exit 0
	for i in $(seq 40); do IF=$(firstif); [ -n "$IF" ] && break; sleep 0.5; done
	[ -n "$IF" ] && start >/dev/null 2>&1; exit 0 ;;
status)
	if [ ! -x /usr/sbin/wpa_supplicant ]; then echo "tools=no"; exit 0; fi
	[ -n "$IF" ] && [ -e "/sys/class/net/$IF/wireless" ] || { echo "adapter="; exit 0; }
	start >/dev/null
	echo "adapter=$IF"
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
		for i in $(seq 40); do sleep 0.5; [ "$(W status | sed -n 's/^wpa_state=//p')" = COMPLETED ] && break; done
		W enable_network all >/dev/null
		[ "$(W status | sed -n 's/^wpa_state=//p')" = COMPLETED ] || { echo "Could not join $SSID (out of range, or its password changed: forget it and connect again)."; exit 1; }
		for i in $(seq 20); do ip -4 -o addr show dev "$IF" | grep -q inet && break; sleep 0.5; done
		echo "Connected to $SSID"; exit 0
	fi
	ID=$(W add_network | tail -1)
	W set_network "$ID" ssid "$(hexname "$SSID")" >/dev/null
	case "$SEC" in
	*psk) W set_network "$ID" key_mgmt "WPA-PSK WPA-PSK-SHA256 SAE" >/dev/null; W set_network "$ID" ieee80211w 1 >/dev/null
		W set_network "$ID" psk "\"$PSK\"" >/dev/null || { W remove_network "$ID" >/dev/null; echo "That password cannot be used."; exit 1; } ;;
	owe) W set_network "$ID" key_mgmt OWE >/dev/null; W set_network "$ID" ieee80211w 2 >/dev/null ;;
	*) W set_network "$ID" key_mgmt NONE >/dev/null ;;
	esac
	case "$SEC" in hidden*) W set_network "$ID" scan_ssid 1 >/dev/null ;; esac
	PSK=
	W select_network "$ID" >/dev/null
	OK=; WRONG=
	for i in $(seq 40); do
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
disconnect) start; W disconnect >/dev/null; echo "Disconnected" ;;
reconnect) start; W enable_network all >/dev/null; W reconnect >/dev/null; echo "Reconnecting" ;;
country)
	start; CC=${CC:-$3}
	case "$CC" in [A-Z][A-Z]|00) ;; *) echo "Use a two-letter country code."; exit 1;; esac
	W set country "$CC" >/dev/null && W save_config >/dev/null && echo "Country set to $CC" ;;
*) echo "usage: wifi.sh boot|status|scan|connect|forget|disconnect|reconnect|country"; exit 1 ;;
esac
