#!/bin/sh
# 1.5.6: the Wi-Fi keeps its address through a short drop (wifi-event, udhcpc.sh) and looks for a better radio only
# when the signal is weak (wifi.sh's bgscan). On the Vivobook the router's 2.4 and 5 GHz radios took turns a few
# seconds apart and every change dropped the address, breaking every open connection. Stand-ins for udhcpc, ip and
# wpa_cli.   sh wifievent_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); L=$HERE/../../sys/root/usr/lib/rudolf
T=$(mktemp -d); trap 'pkill -f "$T/bin/udhcpc" 2>/dev/null; rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
B=$T/bin; mkdir -p $B $T/run; LOG=$T/log; : > $LOG; export LOG T
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
chmod +x $B/*
export WIFI_EVENT_PATH=$B:/usr/bin:/bin WIFI_EVENT_RUN=$T/run WIFI_EVENT_GRACE=1
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
# udhcpc.sh: the same address on renewal is kept as it is
: > $LOG; echo 10.0.0.5 > $T/addr
PATH=$B:$PATH interface=wlan0 ip=10.0.0.5 mask=24 router=10.0.0.1 sh -c "sed 's#/etc/resolv.conf#$T/resolv#g' $L/udhcpc.sh > $T/u.sh; sh $T/u.sh renew"
ok "8. udhcpc.sh: a renewal with the same address does not take it off first" "$([ "$(cnt 'ip flush')" = 0 ] && echo 1)" "$(cat $LOG)"
PATH=$B:$PATH interface=wlan0 ip=10.0.0.9 mask=24 sh $T/u.sh bound
ok "9. ...another address replaces it" "$([ "$(cnt 'ip flush')" = 1 ] && grep -q '10.0.0.9/24' $LOG && echo 1)" "$(cat $LOG)"
# wifi.sh: bgscan on saved networks and on a new one
grep -q "BGSCAN='\"simple:30:-70:3600\"'" $L/wifi.sh && grep -q 'bgscan_all$' $L/wifi.sh && grep -q 'set_network "$ID" bgscan "$BGSCAN"' $L/wifi.sh
ok "10. wifi.sh sets the background scan (weak signal only) on saved networks and new ones" "$([ $? = 0 ] && echo 1)"
grep -q 'disable_aspm' $L/wifi.sh && [ "$(cat $HERE/../../sys/root/etc/modules/rtw88_pci)" = "disable_aspm=1" ]
ok "11. Realtek rtw88: no PCIe link power saving (at load and for a loaded driver)" "$([ $? = 0 ] && echo 1)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
