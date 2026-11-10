#!/bin/sh
# udhcpc event script for Rudolf OS
case "$1" in
	deconfig) ip addr flush dev "$interface"; ip link set "$interface" up ;;
	bound|renew)
		# 1.5.6: the same address again (a renewal, or the Wi-Fi back on the same network) is kept as it is, so open
		# connections go on; another address replaces it
		if ! ip -4 -o addr show dev "$interface" | grep -q " inet $ip/${mask:-24} "; then
			ip addr flush dev "$interface"
			ip addr add "$ip/${mask:-24}" ${broadcast:+broadcast $broadcast} dev "$interface"
		fi
		# a cable wins over Wi-Fi when both are connected
		M=100; [ -e "/sys/class/net/$interface/wireless" ] && M=600
		[ -n "$router" ] && { ip route del default dev "$interface" 2>/dev/null; for r in $router; do ip route add default via "$r" dev "$interface" metric $M && break; done; }
		: > /etc/resolv.conf
		for d in $dns; do echo "nameserver $d" >> /etc/resolv.conf; done
		[ -n "$domain" ] && echo "search $domain" >> /etc/resolv.conf ;;
esac
exit 0
