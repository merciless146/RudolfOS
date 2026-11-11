#!/bin/sh
# 1.5.8 (plan Part 6): network adapters plugged in after start-up get an address and are announced; metered
# connections (cellular modems and tethered phones by their driver, Android hotspots by DHCP, any network by hand);
# the data used on them this month; rudolfd's net/metered answer.   sh netplug58_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); NP=$HERE/../../sys/root/usr/lib/rudolf/netplug; UD=$HERE/../../sys/root/usr/lib/rudolf/udhcpc.sh
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; P=0; F=0
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
S=$W/sys; mkdir -p $S/class/net $S/bus/usb/drivers $S/devices/pci0000:00/usb1/1-2/1-2:1.0 $S/devices/pci0000:00/0000:02:00.0 $W/run $W/etc $W/var
for d in rndis_host qmi_wwan cdc_ether r8169 ipheth; do mkdir -p $S/bus/usb/drivers/$d; done
echo "MiFi 7730L" > $S/devices/pci0000:00/usb1/1-2/product
# an adapter: NAME DRIVER WHERE(usb|pci|none) MAC [wireless]
mk() { n=$S/class/net/$1; mkdir -p $n/statistics; echo $4 > $n/address; echo up > $n/operstate; echo 0 > $n/statistics/rx_bytes; echo 0 > $n/statistics/tx_bytes
	case $3 in usb) dv=$S/devices/pci0000:00/usb1/1-2/1-2:1.$1; mkdir -p $dv;; pci) dv=$S/devices/pci0000:00/0000:02:00.0;; *) dv=;; esac
	if [ -n "$dv" ]; then ln -sfn $dv $n/device; rm -f $dv/driver; ln -s $S/bus/usb/drivers/$2 $dv/driver; fi
	[ "$5" = wireless ] && mkdir -p $n/wireless; true; }
E="RUDOLF_SYS=$S RUDOLF_RUN=$W/run RUDOLF_ETC=$W/etc RUDOLF_VAR=$W/var RUDOLF_NOUP=1 RUDOLF_MONTH=2026-11"
np() { env $E "$@" sh $NP; }
npa() { env $E sh $NP "$@"; }
mk eth0 r8169 pci 00:11:22:33:44:55
mk wlan0 x none 00:aa:bb:cc:dd:ee wireless; ln -sfn $S/devices/pci0000:00/0000:02:00.0 $S/class/net/wlan0/device
mk tun0 x none 00:00:00:00:00:01
npa boot; ok "1. at start-up: DHCP on the cable only (not Wi-Fi, not a tunnel)" $([ "$(cat $W/run/up.log)" = eth0 ]; echo $?) "$(cat $W/run/up.log)"
: > $W/run/up.log; rm -f $W/run/events
mk usb0 rndis_host usb 02:00:00:00:00:01; np ACTION=add INTERFACE=usb0
ok "2. a phone's USB tethering plugged in later: DHCP on it" $(grep -qx usb0 $W/run/up.log; echo $?) "$(cat $W/run/up.log)"
ok "3. ...the desktop is told: a phone, its name, metered" $(grep -q '"t":"net","if":"usb0","kind":"phone","name":"MiFi 7730L","metered":true' $W/run/events; echo $?) "$(cat $W/run/events)"
np ACTION=remove INTERFACE=usb0; ok "4. ...unplugged: its DHCP stops, the desktop is told" $(grep -qx -- -usb0 $W/run/up.log && grep -q '"a":"remove","t":"net","if":"usb0"' $W/run/events; echo $?)
mk wwan0 qmi_wwan usb 02:00:00:00:00:02; mk eth1 cdc_ether usb 02:00:00:00:00:03; mk enx0 ipheth usb 02:00:00:00:00:04
ok "5. kinds: cellular modem, USB Ethernet, iPhone, wired, Wi-Fi, virtual" $([ "$(npa kind wwan0) $(npa kind eth1) $(npa kind enx0) $(npa kind eth0) $(npa kind wlan0) $(npa kind tun0)" = "usbmodem usb iphone ethernet wifi virtual" ]; echo $?) "$(npa kind wwan0) $(npa kind eth1) $(npa kind enx0) $(npa kind eth0) $(npa kind wlan0) $(npa kind tun0)"
ok "6. metered by itself: modem, iPhone; not: the cable, a USB Ethernet dongle, Wi-Fi" $([ "$(npa is wwan0)$(npa is enx0)$(npa is eth0)$(npa is eth1)$(npa is wlan0)" = 11000 ]; echo $?)
# an Android hotspot over Wi-Fi: its DHCP answer says ANDROID_METERED
# (udhcpc.sh's own lines, run alone: the whole script would change this computer's addresses)
sed -n '/1.5.8: a network that says it is metered/,/esac ;;/p' $UD | sed 's/ ;;$//; s/^\t*#.*//' > $W/meter.sh
env RUDOLF_RUN=$W/run interface=wlan0 opt43=414e44524f49445f4d455445524544 sh $W/meter.sh
ok "7. an Android hotspot (ANDROID_METERED in DHCP): metered" $([ "$(npa is wlan0)" = 1 ]; echo $?)
grep -q '414e44524f49445f4d455445524544' $UD; ok "...udhcpc.sh notes it" $?
# by hand, per network
env RUDOLF_SSID_wlan0="Home Net" $E sh $NP set wlan0 off; ok "8. by hand: the hotspot's Wi-Fi network not metered" $([ "$(env RUDOLF_SSID_wlan0="Home Net" $E sh $NP is wlan0)" = 0 ] && grep -qx 'wifi:Home Net=0' $W/etc/rudolf-metered; echo $?) "$(cat $W/etc/rudolf-metered)"
npa set eth0 on; ok "...the cable metered" $([ "$(npa is eth0)" = 1 ]; echo $?)
npa set eth0 auto; ok "...back to automatic" $([ "$(npa is eth0)" = 0 ] && ! grep -q '^if:00:11:22:33:44:55=' $W/etc/rudolf-metered; echo $?) "$(cat $W/etc/rudolf-metered)"
npa set nope on 2>/dev/null; ok "...an adapter that is not there: refused" $([ $? != 0 ]; echo $?)
# data used this month on metered connections
npa tick; echo 1000000 > $S/class/net/wwan0/statistics/rx_bytes; echo 500000 > $S/class/net/wwan0/statistics/tx_bytes; echo 9000000 > $S/class/net/eth0/statistics/rx_bytes
npa tick; echo 2000000 > $S/class/net/wwan0/statistics/rx_bytes; npa tick
o=$(env $E RUDOLF_DEFIF=wwan0 sh $NP state)
ok "9. state: the modem in use, metered (modem), 2.5 MB this month" $(echo "$o" | grep -q '"if":"wwan0","kind":"usbmodem","key":"if:02:00:00:00:00:02","name":"MiFi 7730L","metered":true,"manual":false,"auto":"modem","used":2500000'; echo $?) "$o"
o=$(env $E RUDOLF_DEFIF=eth0 sh $NP state); ok "...the cable's data is not counted (not metered)" $(echo "$o" | grep -q '"metered":false,"manual":false,"auto":"","used":0'; echo $?) "$o"
echo 100 > $S/class/net/wwan0/statistics/rx_bytes; echo 0 > $S/class/net/wwan0/statistics/tx_bytes; npa tick
o=$(env $E RUDOLF_DEFIF=wwan0 sh $NP state); ok "...counters that started again (the modem replugged) do not subtract" $(echo "$o" | grep -q '"used":2500000'; echo $?) "$o"
o=$(env $E RUDOLF_DEFIF=wwan0 RUDOLF_MONTH=2026-12 sh $NP state); ok "...a new month starts at 0" $(echo "$o" | grep -q '"used":0'; echo $?) "$o"
grep -q 'netplug' $HERE/../../sys/root/etc/mdev.conf; ok "10. mdev runs it for network adapters" $?
grep -q 'net/metered' $HERE/../../sys/rudolfd.c; ok "...rudolfd answers net/metered" $?
echo "$P/$((P+F)) passed"; [ $F = 0 ]
