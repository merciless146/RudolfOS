#!/bin/sh
# 1.5.9 (plan Part 1): /usr/lib/rudolf/modem: ModemManager runs only while a modem is there (mdev's hotplug), and
# status reads each modem (maker, model, SIM, operator, signal) from mmcli. Stand-ins for ModemManager, mmcli,
# dbus-daemon, pgrep and pkill.   sh modem59_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); M=$HERE/../../sys/root/usr/lib/rudolf/modem; T=$(mktemp -d); trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
B=$T/bin; mkdir -p $B $T/dev; export ST=$T
printf '#!/bin/sh\necho "MM $*" >> $ST/log; touch $ST/mm-running\n' > $B/ModemManager
printf '#!/bin/sh\n[ "$1" = -x ] && [ "$2" = ModemManager ] && [ -e $ST/mm-running ] && exit 0\n[ "$1" = -f ] && exit 0\nexit 1\n' > $B/pgrep
printf '#!/bin/sh\n[ "$2" = ModemManager ] && rm -f $ST/mm-running && echo "killed" >> $ST/log\n' > $B/pkill
printf '#!/bin/sh\nexit 0\n' > $B/dbus-daemon; printf '#!/bin/sh\nexec "$@"\n' > $B/setsid
cat > $B/mmcli <<'E'
#!/bin/sh
case "$*" in
"-J -L") echo '{"modem-list":["/org/freedesktop/ModemManager1/Modem/0"]}' ;;
"-J -m 0") echo '{"modem":{"3gpp":{"imei":"861234567890123","operator-code":"310260","operator-name":"T-Mobile","registration-state":"home"},"generic":{"access-technologies":["lte"],"drivers":["qmi_wwan","option"],"equipment-identifier":"861234567890123","manufacturer":"Quectel","model":"EG25","revision":"EG25GGBR07A08M2G","sim":"/org/freedesktop/ModemManager1/SIM/0","signal-quality":{"recent":"yes","value":"74"},"state":"registered","state-failed-reason":"--","unlock-required":"--"}}}' ;;
"-J -i 0") echo '{"sim":{"properties":{"active":"yes","operator-name":"T-Mobile"}}}' ;;
esac
E
chmod +x $B/*
export RUDOLF_TEST_PATH=$B RUDOLF_MODEM_LOG=$T/mlog RUDOLF_MODEM_DEV=$T/dev
o=$(ACTION=add sh $M hotplug); ok "1. no modem device: ModemManager is not started" $([ ! -e $T/mm-running ] && echo 1 || echo 0) "$o"
touch $T/dev/cdc-wdm0; ACTION=add sh $M hotplug; sleep 0.3
ok "2. a modem's device appears: ModemManager starts (on the system bus)" $([ -e $T/mm-running ] && grep -q "MM --log-level=WARN" $T/log && echo 1 || echo 0) "$(cat $T/log)"
o=$(sh $M status)
ok "3. status: the modem with maker, model, firmware, drivers, state, SIM, operator, signal, network type" $(echo "$o" | python3 -c '
import json,sys;d=json.load(sys.stdin);m=d["modems"][0]
print(1 if d["running"] and m["maker"]=="Quectel" and m["model"]=="EG25" and m["firmware"]=="EG25GGBR07A08M2G" and m["driver"]=="qmi_wwan,option" and m["state"]=="registered" and m["sim"] and m["operator"]=="T-Mobile" and m["signal"]=="74" and m["tech"]=="lte" else 0)' 2>/dev/null || echo 0) "$o"
ok "4. ...the IMEI shortened (it identifies the device)" $(echo "$o" | grep -q '"imei":"861234…"' && echo 1 || echo 0) "$o"
rm $T/dev/cdc-wdm0; ACTION=remove sh $M hotplug
ok "5. the last modem gone: ModemManager stops" $([ ! -e $T/mm-running ] && grep -q killed $T/log && echo 1 || echo 0) "$(cat $T/log)"
o=$(sh $M status); ok "6. not running: an empty list" $([ "$o" = '{"running":false,"modems":[]}' ] && echo 1 || echo 0) "$o"
grep -q '^-cdc-wdm\[0-9\]+ 0:0 660 \*/usr/lib/rudolf/modem hotplug' $HERE/../../sys/root/etc/mdev.conf; ok "7. mdev runs it for modem devices" $([ $? = 0 ] && echo 1 || echo 0)
echo "$((N-F))/$N passed"; [ $F = 0 ]
