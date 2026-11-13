#!/bin/sh
# 1.5.6: the Wi-Fi keeps its address through a short drop (wifi-event, udhcpc.sh) and looks for a better radio only
# when the signal is weak (wifi.sh's bgscan). On the Vivobook the router's 2.4 and 5 GHz radios took turns a few
# seconds apart and every change dropped the address, breaking every open connection. Stand-ins for udhcpc, ip and
# wpa_cli.   sh wifievent_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); L=$HERE/../../sys/root/usr/lib/rudolf
T=$(mktemp -d); trap 'pkill -f "$T/bin/udhcpc" 2>/dev/null; rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
B=$T/bin; mkdir -p $B $T/run; LOG=$T/log; : > $LOG; TT=$T; export LOG T TT
# udhcpc: records its start, keeps a pid file, notes SIGUSR1 (renew)
cat > $B/udhcpc <<'X'
#!/bin/sh
while [ $# -gt 0 ]; do [ "$1" = -p ] && PF=$2; shift; done
echo "udhcpc start" >> $LOG; echo 10.0.0.5 > $T/addr
sh -c 'echo $$ > "$1"; trap "echo udhcpc renew >> \"$2\"" USR1; trap "exit 0" TERM; while :; do sleep 0.1; done' x "$PF" "$LOG" &
sleep 0.3
X
cat > $B/ip <<'X'
#!/bin/sh
case "$*" in
*"addr flush"*) echo "ip flush" >> $LOG; rm -f $T/addr ;;
*"addr show"*) [ -s $T/addr ] && echo "2: wlan0    inet $(cat $T/addr)/24 brd 10.0.0.255 scope global wlan0" ;;
*"addr add"*) echo "ip add $*" >> $LOG ;;
*) echo "ip $*" >> $LOG ;;
esac
X
# wpa_cli: the state and the network list come from files; select_network and enable_network are logged
cat > $B/wpa_cli <<'X'
#!/bin/sh
while [ $# -gt 0 ]; do case "$1" in -p|-i) shift 2;; *) break;; esac; done
case "$1" in
status) echo "wpa_state=$(cat $TT/wstate 2>/dev/null || echo COMPLETED)"; [ -s $TT/wbssid ] && echo "bssid=$(cat $TT/wbssid)"; [ -s $TT/wssid ] && echo "ssid=$(cat $TT/wssid)" ;;
bssid_ignore) cat $TT/wign 2>/dev/null ;;
set_network) echo "wpa $*" >> $LOG; [ "$3" = bssid_ignore ] && echo "$4" > $TT/wset; echo OK ;;
get_network) [ "$3" = bssid_ignore ] && cat $TT/wset 2>/dev/null || echo FAIL ;;
list_networks) printf 'network id / ssid / bssid / flags\n0\tHome\tany\t%s\n' "$(cat $TT/wflags 2>/dev/null)" ;;
select_network|enable_network) echo "wpa $*" >> $LOG; echo OK ;;
*) echo OK ;;
esac
X
chmod +x $B/*
export WIFI_EVENT_PATH=$B:/usr/bin:/bin WIFI_EVENT_RUN=$T/run WIFI_EVENT_GRACE=1 WIFI_EVENT_RETRY=1 WIFI_EVENT_PERSIST=$T/persist-ignore
printf '#!/bin/sh\ncat $TT/dmesg 2>/dev/null\n' > $B/dmesg; chmod +x $B/dmesg
E="sh $L/wifi-event"; cnt() { grep -c "$1" $LOG; }
WPA_ID=0 $E wlan0 CONNECTED
ok "1. joining a network asks DHCP for an address" "$([ "$(cnt 'udhcpc start')" = 1 ] && echo 1)" "$(cat $LOG)"
# a short drop: the other radio of the same network 4 s later (here 0.3 s, the grace 1 s)
$E wlan0 DISCONNECTED; sleep 0.3; WPA_ID=0 $E wlan0 CONNECTED; sleep 1.5
ok "2. a short drop keeps the address (no flush)" "$([ "$(cnt 'ip flush')" = 0 ] && [ -s $T/addr ] && echo 1)" "$(cat $LOG)"
ok "3. ...and back on the same network renews the lease it has, without starting DHCP over" "$([ "$(cnt 'udhcpc start')" = 1 ] && [ "$(cnt 'udhcpc renew')" = 1 ] && echo 1)" "$(cat $LOG)"
ok "4. ...and the desktop is not told the Wi-Fi went away" "$(grep -q 'wifi disconnected' $T/run/events && echo 0 || echo 1)" "$(cat $T/run/events)"
# a real drop: still off after the grace
$E wlan0 DISCONNECTED; sleep 1.6
ok "5. off the network for longer than the grace: the address goes, and the desktop is told" "$([ "$(cnt 'ip flush')" = 1 ] && grep -q 'wifi disconnected' $T/run/events && echo 1)" "$(cat $LOG; cat $T/run/events)"
# another network: starts over
WPA_ID=0 $E wlan0 CONNECTED; c0=$(cnt 'udhcpc start'); WPA_ID=3 $E wlan0 CONNECTED
ok "6. another network: DHCP starts over" "$([ "$(cnt 'udhcpc start')" = $((c0 + 1)) ] && echo 1)" "$(cat $LOG)"
# switching adapters: at once
f0=$(cnt 'ip flush'); $E wlan0 DISCONNECTED now
ok "7. switching adapters (now): the address goes at once" "$([ "$(cnt 'ip flush')" = $((f0 + 1)) ] && echo 1)" "$(cat $LOG)"
# 2026111014: the move to the 2.4 GHz radio failed and wpa_supplicant set the whole network aside for 5.5 minutes
WPA_ID=0 $E wlan0 CONNECTED; : > $LOG; echo DISCONNECTED > $T/wstate; echo '[TEMP-DISABLED]' > $T/wflags
$E wlan0 DISCONNECTED; sleep 1.5
ok "12. still off after the grace and the network set aside: it is chosen again (the others enabled again too)" "$(grep -q 'wpa select_network 0' $LOG && grep -q 'wpa enable_network all' $LOG && echo 1)" "$(cat $LOG)"
grep -q "set aside" $T/run/wifi.log; ok "13. ...and that is written to the Wi-Fi log" "$([ $? = 0 ] && echo 1)"
echo COMPLETED > $T/wstate; : > $T/wflags; WPA_ID=0 $E wlan0 CONNECTED; n=$(grep -c select_network $LOG); sleep 2.5
ok "14. back on the network: no more tries" "$([ "$(grep -c select_network $LOG)" = "$n" ] && echo 1)" "$(cat $LOG)"
: > $LOG; echo SCANNING > $T/wstate; : > $T/wflags; $E wlan0 DISCONNECTED; sleep 3
ok "15. off but not set aside (still looking): left to wpa_supplicant" "$(grep -q select_network $LOG && echo 0 || echo 1)" "$(cat $LOG)"
echo DISCONNECTED > $T/wstate; echo '[TEMP-DISABLED]' > $T/wflags; : > $LOG; WPA_ID=0 $E wlan0 CONNECTED; $E wlan0 DISCONNECTED; sleep 6.5
ok "16. at most 4 tries for one drop" "$([ "$(grep -c 'select_network' $LOG)" = 4 ] && echo 1)" "$(grep -c select_network $LOG)"
echo INACTIVE > $T/wstate; : > $T/wflags; : > $LOG; WPA_ID=0 $E wlan0 CONNECTED; $E wlan0 DISCONNECTED; sleep 1.5
ok "17. off and wpa_supplicant no longer trying (INACTIVE): chosen again too" "$(grep -q 'wpa select_network 0' $LOG && echo 1)" "$(cat $LOG)"
echo COMPLETED > $T/wstate; WPA_ID=0 $E wlan0 CONNECTED; sleep 1
# 2026111016: the 2.4 GHz radio tried twice in 25 minutes; it stays out for the session
: > $LOG; rm -f $T/run/wifi.log; echo bc:96:e5:88:1f:d9 > $T/wbssid; printf 'bc:96:e5:88:1f:da\nbc:96:e5:88:1f:d9\n' > $T/wign
WPA_ID=0 $E wlan0 CONNECTED
ok "18. joined: the radio that did not answer is left out of the network for this session (not the one joined)" "$(grep -q 'wpa set_network 0 bssid_ignore bc:96:e5:88:1f:da$' $LOG && echo 1)" "$(cat $LOG)"
ok "19. ...written to the Wi-Fi log" "$(grep -q 'left out (did not answer): bc:96:e5:88:1f:da' $T/run/wifi.log && echo 1)" "$(cat $T/run/wifi.log 2>/dev/null)"
c=$(grep -c 'set_network' $LOG); WPA_ID=0 $E wlan0 CONNECTED
ok "20. joining again: nothing new, nothing set again" "$([ "$(grep -c 'set_network' $LOG)" = "$c" ] && echo 1)" "$(cat $LOG)"
: > $LOG; echo SCANNING > $T/wstate; : > $T/wign; $E wlan0 DISCONNECTED; sleep 1.5
ok "21. off for longer than the grace: every radio allowed again (maybe only that one reaches here)" "$(grep -q 'wpa set_network 0 bssid_ignore $' $LOG && [ ! -s $T/run/wifi-ignore-wlan0 ] && echo 1)" "$(cat $LOG)"
echo COMPLETED > $T/wstate; : > $LOG; $E wlan0 DISCONNECTED; sleep 0.2; WPA_ID=0 $E wlan0 CONNECTED; sleep 1.3
ok "22. a short drop does not let it in again" "$(grep -q 'bssid_ignore $' $LOG && echo 0 || echo 1)" "$(cat $LOG)"
: > $T/wbssid
# wifi.sh: never saved
grep -q '^save_cfg()' $L/wifi.sh && [ "$(grep -c 'W save_config' $L/wifi.sh)" = 1 ] && grep -q 'rm -f /run/rudolf/wifi-ignore-$IF' $L/wifi.sh
ok "23. wifi.sh saves without the session's left-out radios (and a new wpa_supplicant starts with none)" "$([ $? = 0 ] && echo 1)"
# udhcpc.sh: the same address on renewal is kept as it is
: > $LOG; echo 10.0.0.5 > $T/addr
PATH=$B:$PATH interface=wlan0 ip=10.0.0.5 mask=24 router=10.0.0.1 sh -c "sed 's#/etc/resolv.conf#$T/resolv#g' $L/udhcpc.sh > $T/u.sh; sh $T/u.sh renew"
ok "8. udhcpc.sh: a renewal with the same address does not take it off first" "$([ "$(cnt 'ip flush')" = 0 ] && echo 1)" "$(cat $LOG)"
PATH=$B:$PATH interface=wlan0 ip=10.0.0.9 mask=24 sh $T/u.sh bound
ok "9. ...another address replaces it" "$([ "$(cnt 'ip flush')" = 1 ] && grep -q '10.0.0.9/24' $LOG && echo 1)" "$(cat $LOG)"
# wifi.sh: bgscan on saved networks and on a new one
grep -q "BGSCAN='\"simple:30:-78:86400\"'" $L/wifi.sh && grep -q 'bgscan_all$' $L/wifi.sh && grep -q 'set_network "$ID" bgscan "$BGSCAN"' $L/wifi.sh
ok "10. wifi.sh sets the background scan (weak signal only) on saved networks and new ones" "$([ $? = 0 ] && echo 1)"
grep -q 'disable_aspm' $L/wifi.sh && [ "$(cat $HERE/../../sys/root/etc/modules/rtw88_pci)" = "disable_aspm=1" ]
ok "11. Realtek rtw88: no PCIe link power saving (at load and for a loaded driver)" "$([ $? = 0 ] && echo 1)"
# 1.5.9 (2026111302): the 2.4 GHz radio did not answer again; wpa_supplicant's own list had forgotten it by then
rm -rf $T/run; mkdir -p $T/run; rm -f $T/wign $T/wset $T/persist-ignore; : > $LOG
echo bc:96:e5:88:1f:d9 > $T/wbssid; echo 'Spectrum 3E' > $T/wssid; echo COMPLETED > $T/wstate
printf '[   87.849511] wlan0: authentication with bc:96:e5:88:1f:da timed out\n' > $T/dmesg
WPA_ID=0 $E wlan0 CONNECTED
ok "24. the kernel's 'authentication with X timed out' counts too (wpa_supplicant's list already empty)" "$(grep -q 'wpa set_network 0 bssid_ignore bc:96:e5:88:1f:da$' $LOG && echo 1)" "$(cat $LOG)"
H=$(printf %s 'Spectrum 3E' | od -An -tx1 | tr -d ' \n')
ok "25. ...and kept for a day, by network name (in hex), across restarts" "$(grep -q "^$H bc:96:e5:88:1f:da [0-9]*$" $T/persist-ignore && echo 1)" "$(cat $T/persist-ignore 2>/dev/null)"
# a restart: a new session, no kernel message yet: the radio is left out from the first join
rm -rf $T/run; mkdir -p $T/run; rm -f $T/wset $T/dmesg; : > $LOG
WPA_ID=0 $E wlan0 CONNECTED
ok "26. after a restart it is left out from the first join (kept from earlier)" "$(grep -q 'wpa set_network 0 bssid_ignore bc:96:e5:88:1f:da$' $LOG && grep -q 'kept from earlier' $T/run/wifi.log && echo 1)" "$(cat $LOG; cat $T/run/wifi.log)"
# reached through that radio after all (the only one in reach): it is taken off the list
echo bc:96:e5:88:1f:da > $T/wbssid; rm -f $T/wset; WPA_ID=0 $E wlan0 CONNECTED
ok "27. reached through that radio after all: taken off the list" "$(grep -qi 'bc:96:e5:88:1f:da' $T/persist-ignore && echo 0 || echo 1)" "$(cat $T/persist-ignore)"
# a day later it is forgotten
echo "$H aa:bb:cc:dd:ee:ff $(( $(date +%s) - 90000 ))" >> $T/persist-ignore; echo bc:96:e5:88:1f:d9 > $T/wbssid; rm -f $T/wset; : > $LOG
WPA_ID=0 $E wlan0 CONNECTED
ok "28. after a day a radio is tried again" "$(grep -q 'aa:bb:cc:dd:ee:ff' $T/persist-ignore $LOG && echo 0 || echo 1)" "$(cat $T/persist-ignore; cat $LOG)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
