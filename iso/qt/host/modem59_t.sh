#!/bin/sh
# 1.5.9 (plan Part 1): /usr/lib/rudolf/modem: ModemManager runs only while a modem is there (mdev's hotplug), and
# status reads each modem (maker, model, SIM, operator, signal) from mmcli. Stand-ins for ModemManager, mmcli,
# dbus-daemon, pgrep and pkill.   sh modem59_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); M=$HERE/../../sys/root/usr/lib/rudolf/modem; T=$(mktemp -d); trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
B=$T/bin; mkdir -p $B $T/dev; export ST=$T
printf '#!/bin/sh\n[ "$1" = --help-all ] && { [ -e $ST/old-mm ] || echo "  --test-no-udev   Run without udev support even if available"; exit 0; }\necho "MM $*" >> $ST/log; touch $ST/mm-running\n' > $B/ModemManager
printf '#!/bin/sh\n[ "$1" = -x ] && [ "$2" = ModemManager ] && [ -e $ST/mm-running ] && exit 0\n[ "$1" = -f ] && exit 0\nexit 1\n' > $B/pgrep
printf '#!/bin/sh\n[ "$2" = ModemManager ] && rm -f $ST/mm-running && echo "killed" >> $ST/log\n' > $B/pkill
printf '#!/bin/sh\nexit 0\n' > $B/dbus-daemon; printf '#!/bin/sh\nexec "$@"\n' > $B/setsid
cat > $B/mmcli <<'E'
#!/bin/sh
case "$*" in
--report-kernel-event=*) echo "${1#*=}" >> $ST/events ;;
"-L") exit 0 ;;
"-J -L") echo '{"modem-list":["/org/freedesktop/ModemManager1/Modem/0"]}' ;;
"-J -m 0") echo '{"modem":{"3gpp":{"imei":"861234567890123","operator-code":"310260","operator-name":"T-Mobile","registration-state":"home"},"generic":{"access-technologies":["lte"],"drivers":["qmi_wwan","option"],"equipment-identifier":"861234567890123","manufacturer":"Quectel","model":"EG25","revision":"EG25GGBR07A08M2G","sim":"/org/freedesktop/ModemManager1/SIM/0","signal-quality":{"recent":"yes","value":"74"},"state":"registered","state-failed-reason":"--","unlock-required":"--"}}}' ;;
"-J -i 0") echo '{"sim":{"properties":{"active":"yes","operator-name":"T-Mobile"}}}' ;;
esac
E
chmod +x $B/*
S=$T/sys; mkdir -p $S/class/usbmisc $S/class/tty $S/class/net/eth0 $S/class/wwan $S/drv/qmi_wwan $S/drv/option
export RUDOLF_TEST_PATH=$B RUDOLF_MODEM_LOG=$T/mlog RUDOLF_MODEM_DEV=$T/dev RUDOLF_MODEM_SYS=$S RUDOLF_RUN=$T/run
o=$(ACTION=add sh $M hotplug); ok "1. no modem device: ModemManager is not started" $([ ! -e $T/mm-running ] && echo 1 || echo 0) "$o"
touch $T/dev/cdc-wdm0; mkdir -p $S/class/usbmisc/cdc-wdm0 $S/class/net/wwan0 $S/class/net/usb0/device $S/class/tty/ttyUSB2/device
ln -s $S/drv/qmi_wwan $S/class/net/usb0/device/driver; ln -s $S/drv/option $S/class/tty/ttyUSB2/device/driver
MDEV=cdc-wdm0 ACTION=add sh $M hotplug; sleep 0.3
ok "2. a modem's device appears: ModemManager starts (on the system bus), without udev" $([ -e $T/mm-running ] && grep -q "MM --log-level=WARN --test-no-udev" $T/log && echo 1 || echo 0) "$(cat $T/log)"
ok "2a. ...and is told of every port already there: cdc-wdm, the qmi_wwan and wwan interfaces, the serial port; not eth0" $(grep -qx 'action=add,subsystem=usbmisc,name=cdc-wdm0' $T/events && grep -qx 'action=add,subsystem=net,name=usb0' $T/events && grep -qx 'action=add,subsystem=net,name=wwan0' $T/events && grep -qx 'action=add,subsystem=tty,name=ttyUSB2' $T/events && ! grep -q eth0 $T/events && echo 1 || echo 0) "$(cat $T/events)"
: > $T/events; touch $T/dev/wwan0at0; mkdir -p $S/class/wwan/wwan0at0; MDEV=wwan0at0 ACTION=add sh $M hotplug
ok "2b. a port that comes later is reported once, ModemManager not started again" $([ "$(cat $T/events)" = 'action=add,subsystem=wwan,name=wwan0at0' ] && [ $(grep -c '^MM ' $T/log) = 1 ] && [ ! -d $T/run/modem.lock ] && echo 1 || echo 0) "$(cat $T/events; cat $T/log)"
o=$(sh $M status)
ok "3. status: the modem with maker, model, firmware, drivers, state, SIM, operator, signal, network type" $(echo "$o" | python3 -c '
import json,sys;d=json.load(sys.stdin);m=d["modems"][0]
print(1 if d["running"] and m["maker"]=="Quectel" and m["model"]=="EG25" and m["firmware"]=="EG25GGBR07A08M2G" and m["driver"]=="qmi_wwan,option" and m["state"]=="registered" and m["sim"] and m["operator"]=="T-Mobile" and m["signal"]=="74" and m["tech"]=="lte" else 0)' 2>/dev/null || echo 0) "$o"
ok "4. ...the IMEI shortened (it identifies the device)" $(echo "$o" | grep -q '"imei":"861234…"' && echo 1 || echo 0) "$o"
rm $T/dev/cdc-wdm0 $T/dev/wwan0at0; rm -rf $S/class/tty/ttyUSB2; : > $T/events; MDEV=cdc-wdm0 ACTION=remove sh $M hotplug
ok "5. the last modem gone: ModemManager is told, then stops" $([ ! -e $T/mm-running ] && grep -q killed $T/log && grep -qx 'action=remove,subsystem=usbmisc,name=cdc-wdm0' $T/events && echo 1 || echo 0) "$(cat $T/log; cat $T/events)"
o=$(sh $M status); ok "6. not running: an empty list" $([ "$o" = '{"running":false,"modems":[]}' ] && echo 1 || echo 0) "$o"
grep -q '^-cdc-wdm\[0-9\]+ 0:0 660 \*/usr/lib/rudolf/modem hotplug' $HERE/../../sys/root/etc/mdev.conf; ok "7. mdev runs it for modem devices" $([ $? = 0 ] && echo 1 || echo 0)
touch $T/old-mm; rm -f $T/log; touch $T/dev/cdc-wdm0; MDEV=cdc-wdm0 ACTION=add sh $M hotplug
ok "8. an older ModemManager without --test-no-udev: --no-auto-scan" $(grep -q "MM --log-level=WARN --no-auto-scan" $T/log && echo 1 || echo 0) "$(cat $T/log)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
