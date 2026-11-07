#!/bin/sh
# 1.5.3: wifi.sh with two Wi-Fi adapters, a built-in one (PCI) and a USB dongle (the Vivobook's RT5370): the built-in
# one by default, the chosen one (saved by MAC address) when it is plugged in, the built-in one again when it is
# unplugged, one adapter at a time, and start-up waiting a little for a chosen USB adapter. Stand-ins for
# wpa_supplicant, wpa_cli and ip; a stand-in /sys/class/net. Needs root (a private /run, in a mount namespace).
[ "$(id -u)" = 0 ] && [ -z "$WAT_INNER" ] && exec env WAT_INNER=1 unshare -m sh -c 'mount -t tmpfs none /run && mkdir -p /run/rudolf && exec sh "$0"' "$0"
HERE=$(cd "$(dirname "$0")" && pwd); W=$HERE/../../sys/root/usr/lib/rudolf/wifi.sh
T=$(mktemp -d); trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
B=$T/bin; mkdir -p $B; export STATE=$T/state; mkdir -p $STATE; LOG=$T/log; export LOG
cat > $B/wpa_supplicant <<'X'
#!/bin/sh
while [ $# -gt 0 ]; do [ "$1" = -i ] && I=$2; shift; done; touch $STATE/run-$I; echo "start $I" >> $LOG
X
cat > $B/wpa_cli <<'X'
#!/bin/sh
while [ $# -gt 0 ]; do case "$1" in -p) shift 2;; -i) I=$2; shift 2;; -a|-P) shift 2;; -B) shift;; *) break;; esac; done
case "$1" in
ping) [ -e $STATE/run-$I ] && echo PONG || echo FAIL ;;
terminate) rm -f $STATE/run-$I; echo "terminate $I" >> $LOG; echo OK ;;
status) printf 'wpa_state=COMPLETED\nssid=Home\n' ;;
list_networks) echo "network id / ssid / bssid / flags" ;;
"") echo "events $I" >> $LOG ;;
*) echo "$I $*" >> $LOG; echo OK ;;
esac
X
printf '#!/bin/sh\necho "ip $*" >> $LOG\n' > $B/ip; printf '#!/bin/sh\nexit 1\n' > $B/mountpoint
chmod +x $B/*
# the stand-in sysfs: wlan0 built-in (rtw88_8821ce on PCI), wlan1 a USB dongle (rt2800usb, Ralink 802.11 n WLAN)
S=$T/sys; NET=$S/class/net; mkdir -p $NET $S/bus/drivers/rtw88_8821ce $S/bus/drivers/rt2800usb
P=$S/devices/pci0000:00/0000:02:00.0; mkdir -p $P; ln -s $S/bus/drivers/rtw88_8821ce $P/driver
U=$S/devices/pci0000:00/0000:03:00.4/usb3/3-3; mkdir -p $U/3-3:1.0; echo Ralink > $U/manufacturer; echo "802.11 n WLAN" > $U/product; ln -s $S/bus/drivers/rt2800usb $U/3-3:1.0/driver
mkif() { mkdir -p $NET/$1/wireless; echo $2 > $NET/$1/address; ln -s $3 $NET/$1/device; }
mkif wlan0 f8:54:f6:2f:6a:1f $P
plug() { mkif wlan1 00:0f:13:52:70:01 $U/3-3:1.0; }; unplug() { rm -rf $NET/wlan1; }; plug
export RUDOLF_TEST_PATH=$B RUDOLF_WIFI_NET=$NET RUDOLF_WIFI_SEL=$T/persist/wifi-adapter
run() { sh $W "$@" 2>&1; }; started() { grep -c "^start $1$" $LOG; }
o=$(run status)
ok "two adapters, nothing chosen: the built-in one" $(has "$o" "adapter=wlan0") "$o"
ok "...both listed with their bus, driver and name" $([ "$(has "$o" "$(printf 'wlan=wlan0\tf8:54:f6:2f:6a:1f\tpci\trtw88_8821ce\trtw88_8821ce')")$(has "$o" "$(printf 'wlan=wlan1\t00:0f:13:52:70:01\tusb\trt2800usb\tRalink 802.11 n WLAN')")" = 11 ] && echo 1 || echo 0) "$o"
ok "...and no choice saved" $([ -z "$(echo "$o" | grep chosen)" ] && echo 1 || echo 0) "$o"
o=$(run adapter wlan1)
ok "choosing the dongle" $(has "$o" "Using Ralink 802.11 n WLAN for Wi-Fi") "$o"
ok "...saves its MAC address" $([ "$(cat $T/persist/wifi-adapter)" = 00:0f:13:52:70:01 ] && echo 1 || echo 0) "$(cat $T/persist/wifi-adapter 2>&1)"
ok "...starts it and stops the built-in one (one at a time)" $([ -e $STATE/run-wlan1 ] && [ ! -e $STATE/run-wlan0 ] && grep -q "terminate wlan0" $LOG && echo 1 || echo 0) "$(cat $LOG)"
ok "...and takes the built-in one's link down" $(grep -q "ip link set wlan0 down" $LOG && echo 1 || echo 0) "$(cat $LOG)"
o=$(run status)
ok "afterwards the dongle is used by default" $([ "$(has "$o" adapter=wlan1)$(has "$o" chosen=00:0f:13:52:70:01)" = 11 ] && [ -z "$(echo "$o" | grep chosen_missing)" ] && echo 1 || echo 0) "$o"
unplug; rm -f $STATE/run-wlan1
o=$(run status)
ok "unplugged: the built-in Wi-Fi takes over" $([ "$(has "$o" adapter=wlan0)$(has "$o" chosen_missing=1)" = 11 ] && [ -e $STATE/run-wlan0 ] && echo 1 || echo 0) "$o"
plug; n0=$(grep -c "terminate wlan0" $LOG)
o=$(run status)
ok "plugged in again: the dongle again, the built-in one stops" $([ "$(has "$o" adapter=wlan1)" = 1 ] && [ $(grep -c "terminate wlan0" $LOG) -gt $n0 ] && [ -e $STATE/run-wlan1 ] && echo 1 || echo 0) "$o"
o=$(run adapter wlan9); ok "an adapter that is not there is refused" $(has "$o" "No such Wi-Fi adapter.") "$o"
o=$(run adapter "../x"); ok "...and so is a path" $(has "$o" "No such Wi-Fi adapter.") "$o"
# start-up: the chosen dongle shows up 2 seconds after the built-in Wi-Fi
unplug; rm -f $STATE/run-*; : > $LOG
(sleep 2; plug) & sh $W boot; wait
ok "start-up waits for the chosen USB adapter and uses it" $([ "$(started wlan1)" -ge 1 ] && [ "$(started wlan0)" = 0 ] && echo 1 || echo 0) "$(cat $LOG)"
# the choice gone for good (another dongle): start-up gives up after 10 seconds and uses the built-in one
unplug; rm -f $STATE/run-*; : > $LOG; t0=$(date +%s); sh $W boot; t1=$(date +%s)
ok "...not plugged in: the built-in one, after waiting at most about 10 seconds" $([ "$(started wlan0)" -ge 1 ] && [ $((t1-t0)) -le 13 ] && echo 1 || echo 0) "$((t1-t0)) s; $(cat $LOG)"
rm -f $T/persist/wifi-adapter $STATE/run-*; : > $LOG; plug; t0=$(date +%s); sh $W boot; t1=$(date +%s)
ok "nothing chosen: start-up does not wait, and uses the built-in one" $([ "$(started wlan0)" -ge 1 ] && [ $((t1-t0)) -le 2 ] && echo 1 || echo 0) "$((t1-t0)) s; $(cat $LOG)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
