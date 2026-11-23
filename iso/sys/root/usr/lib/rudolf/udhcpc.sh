#!/bin/sh
# udhcpc event script for Rudolf OS
#
# 1.5.9 (found on the Vivobook, 2026-11-14): a cable or a USB tethering link wins over Wi-Fi only once it reaches the
# internet. A MiFi without a data plan, plugged in by USB, handed out 192.168.1.55 on the same network as the Wi-Fi
# (192.168.1.0/24, gateway 192.168.1.1) and took the default route (metric 100 against Wi-Fi's 600): everything went
# to the MiFi and nothing came back. Now, while another connection carries the default route, a new wired one is
# added behind it (metric 900) and tried (a ping through it); it takes over (metric 100, its DNS) only when the
# internet answers through it, and is tried again every 30 s until it does or goes (RUN/nointernet/IF while it has
# none: Settings and the network menu say "No internet"). Alone, it is used at once, as before.
# The DNS servers of each connection are kept in RUN/dns/IF; /etc/resolv.conf is the one of the connection that
# carries the default route.
# test hooks (qt/host/netroute59_t.sh): RUDOLF_RUN, RUDOLF_RESOLV, RUDOLF_NETCHECK_EVERY (seconds)
RUN=${RUDOLF_RUN:-/run/rudolf}; RESOLV=${RUDOLF_RESOLV:-/etc/resolv.conf}; EVERY=${RUDOLF_NETCHECK_EVERY:-30}
umask 022; mkdir -p "$RUN/dns" "$RUN/nointernet" 2>/dev/null; chmod 755 "$RUN/dns" "$RUN/nointernet" 2>/dev/null

# the interface that carries the default route (the lowest metric)
best() { ip route 2>/dev/null | awk '/^default/{d="";m=0;for(i=1;i<NF;i++){if($i=="dev")d=$(i+1);if($i=="metric")m=$(i+1)}if(d!="")print m+0, d}' | sort -n | head -1 | cut -d' ' -f2; }
# /etc/resolv.conf from the connection that carries the default route (1.6.2 Part 33: while a VPN is on, its own
# name servers, RUN/dns-vpn: the VPN helper's, so a new address on the Wi-Fi never puts the network's back)
resolv() { [ -s "$RUN/dns-vpn" ] && { cat "$RUN/dns-vpn" > "$RESOLV" 2>/dev/null; return 0; }; b=$(best); [ -n "$b" ] && [ -f "$RUN/dns/$b" ] && cat "$RUN/dns/$b" > "$RESOLV" 2>/dev/null; true; }
# does the internet answer through this interface (and only through it)?
online() { ping -c1 -W3 -I "$1" 1.1.1.1 >/dev/null 2>&1 || ping -c1 -W3 -I "$1" 8.8.8.8 >/dev/null 2>&1; }
# another connection's default route
others() { ip route 2>/dev/null | awk -v me="$1" '/^default/{for(i=1;i<NF;i++)if($i=="dev"&&$(i+1)!=me){print;exit}}'; }
stopcheck() { p=$(cat "$RUN/nointernet/$1.pid" 2>/dev/null); [ -n "$p" ] && kill "$p" 2>/dev/null; rm -f "$RUN/nointernet/$1.pid" "$RUN/nointernet/$1"; }
route() { ip route del default dev "$interface" 2>/dev/null; for r in $router; do ip route add default via "$r" dev "$interface" metric "$1" && break; done; }

case "$1" in
	deconfig)
		stopcheck "$interface"
		ip addr flush dev "$interface"; ip link set "$interface" up
		rm -f "$RUN/dns/$interface"; resolv ;;
	bound|renew)
		# 1.5.6: the same address again (a renewal, or the Wi-Fi back on the same network) is kept as it is, so open
		# connections go on; another address replaces it
		if ! ip -4 -o addr show dev "$interface" | grep -q " inet $ip/${mask:-24} "; then
			ip addr flush dev "$interface"
			ip addr add "$ip/${mask:-24}" ${broadcast:+broadcast $broadcast} dev "$interface"
		fi
		{ for d in $dns; do echo "nameserver $d"; done; [ -n "$domain" ] && echo "search $domain"; } > "$RUN/dns/$interface"
		if [ -n "$router" ]; then
			if [ -e "/sys/class/net/$interface/wireless" ]; then route 600
			# 1.6.0 (Part 3): a cellular modem's interface that takes DHCP (/usr/lib/rudolf/modem): behind Wi-Fi
			elif [ -e "$RUN/cellular/$interface" ]; then route 700
			# a renewal of a wired connection already in use (or still being tried) keeps its place
			elif [ "$1" = renew ] && ip route 2>/dev/null | grep -q "^default .*dev $interface .*metric 100"; then :
			elif [ "$1" = renew ] && [ -e "$RUN/nointernet/$interface" ]; then :
			elif [ -z "$(others "$interface")" ]; then stopcheck "$interface"; route 100
			else
				# behind the connection in use, tried first
				stopcheck "$interface"; route 900
				if online "$interface"; then route 100
				else
					: > "$RUN/nointernet/$interface"
					( while sleep "$EVERY"; do
						ip -4 -o addr show dev "$interface" 2>/dev/null | grep -q " inet $ip/" || break
						if online "$interface"; then route 100; rm -f "$RUN/nointernet/$interface"; resolv; break; fi
					done; rm -f "$RUN/nointernet/$interface.pid" ) >/dev/null 2>&1 &
					echo $! > "$RUN/nointernet/$interface.pid"
				fi
			fi
		fi
		resolv
		# 1.5.8: a network that says it is metered (Android's hotspots: ANDROID_METERED in option 43): netplug
		M=${RUDOLF_RUN:-/run/rudolf}/metered; mkdir -p "$M" 2>/dev/null
		case "$opt43" in *414e44524f49445f4d455445524544*|*ANDROID_METERED*) : > "$M/$interface";; *) rm -f "$M/$interface";; esac ;;
esac
exit 0
