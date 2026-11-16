#!/bin/sh
# 1.6.0 (plan Part 5, stage A item 5): signal and status, from the stand-in ModemManager's changing values (../mmstub.c,
# the real mmcli), in network namespaces of its own: rudolfd's mobile/signal (LTE's RSRP, RSRQ, SINR, RSSI; 5G's),
# rudolfd's net list with the cellular connection's operator, type and signal for the taskbar (written by the watch,
# following the modem's changes), System health's modem events (the SIM taken out and put back, the PIN blocked, no
# network for a minute, the module gone and back), and the self-test's modem line (driver, SIM, network, data).
# Needs root, python3, node, dbus-daemon, mmcli, libmm-glib-dev, mobile-broadband-provider-info.   sh signal60_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$G60_INNER" ] && exec env G60_INNER=1 unshare -m -n sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); SR=$HERE/../../sys/root/usr/lib/rudolf; M=$SR/modem; T=$(mktemp -d /tmp/signal60.XXXXXX)
MBPI=/usr/share/mobile-broadband-provider-info/serviceproviders.xml
[ -r $MBPI ] && [ -e /dev/net/tun ] || { echo "SKIP: needs mobile-broadband-provider-info and /dev/net/tun"; exit 0; }
MMSTUB_HERE=$HERE/x; . $HERE/mmstub.sh
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
PIDS=""; trap 'kill $PIDS 2>/dev/null; [ -s /run/rudolf/modem-watch.pid ] && kill $(cat /run/rudolf/modem-watch.pid) 2>/dev/null; mmstub_down; rm -rf $T' EXIT
w8() { i=0; while [ $i -lt ${2:-40} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
mount -t tmpfs none /run; mkdir -p /run/rudolf /run/dbus; mount -t sysfs none /sys; ip link set lo up
unshare -n sh -c "echo \$\$ > $T/far.pid; exec sleep 3600" & PIDS="$PIDS $!"; w8 "[ -s $T/far.pid ]"; FP=$(cat $T/far.pid); PIDS="$PIDS $FP"
python3 $HERE/fixtures/tunpair.py wwan0 /proc/$FP/ns/net cfar $T/r & PIDS="$PIDS $!"; w8 "[ -e $T/r ]"
mkdir -p $T/b $T/p; printf '#!/bin/sh\n[ "$1" = "^ModemManager\\$" ] && exit 0; exit 1\n' > $T/b/pgrep; chmod +x $T/b/pgrep
printf '#!/bin/sh\nexit 0\n' > $T/fw; printf '#!/bin/sh\nshift; echo "$1 | $2" >> %s/health.log\n' $T > $T/health; chmod +x $T/fw $T/health
export RUDOLF_TEST_PATH=$T/b RUDOLF_MODEM_SYS=/sys RUDOLF_MODEM_PERSIST=$T/p RUDOLF_MODEM_LOG=$T/mlog RUDOLF_RUN=/run/rudolf RUDOLF_MBPI=$MBPI \
	RUDOLF_APN_FIXES=$SR/../../share/rudolf/apn-fixes RUDOLF_RESOLV=$T/resolv.conf RUDOLF_FIREWALL=$T/fw RUDOLF_HEALTH="sh $T/health --health" RUDOLF_WATCH_TICK=0.3 RUDOLF_NETUSE=$T/netusage
echo "add 0 iccid=8901260123456789012 iface=wwan0 ip4=10.64.12.34/30 gw4=10.64.12.33 enabled=1 signal=74 rsrp=-87 rsrq=-9 sinr=17 rssi=-61" > $T/init
mmstub_up $T/mm $T/init || exit 0
gcc -O2 -w -DMODEM_SH="\"sh $M\"" -DMOBILE_RUN="\"/run/rudolf\"" -o $T/api $HERE/fixtures/mobileapi.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
node $HERE/fixtures/deskls.js $T/ls.json $T/keys.json || { echo "FAIL node"; exit 1; }
a() { $T/api $T/ls.json /run/rudolf "$@"; }
js() { printf '%s' "$1" | sed 's/^HTTP[^|]*|//' | node -e "let s='';process.stdin.on('data',d=>s+=d).on('end',()=>{try{const d=JSON.parse(s);const w=(d.interfaces||[]).find(i=>i.name=='wwan0')||{};console.log(($2)?1:0)}catch(e){console.log(0)}})"; }
H() { cat $T/health.log 2>/dev/null; }
timeout 0.5 tail -f /dev/null; a "POST mobile/connect?id=0&on=1" >/dev/null
o=$(a "GET mobile/signal?id=0")
ok "1. the signal's details (mobile/signal): LTE's RSRP, RSRQ, SINR and RSSI; ModemManager asked to measure them" "$(js "$o" "d.ok&&d.lte.rsrp==-87&&d.lte.rsrq==-9&&d.lte.sinr==17&&d.lte.rssi==-61&&d.nr5g.rsrp===null&&d.rate==5" | grep -q 1 && grep -q '0 SignalSetup 5' $MMS_D/calls && echo 1)" "$o"
mmstub_cmd "set 0 nr=1 tech=5gnr rsrp=-101"; o=$(a "GET mobile/signal?id=0")
ok "...5G's too, as the modem changes (a weaker signal)" "$(js "$o" "d.lte.rsrp==-101&&d.nr5g.rsrp==-98&&d.nr5g.sinr==19")" "$o"
mmstub_cmd "set 0 nr=0 tech=lte rsrp=-87"
sh $M watch & PIDS="$PIDS $!"; w8 "[ -s /run/rudolf/cellinfo/wwan0 ]" 40
o=$(a "GET net")
ok "2. the network list (the taskbar's): the cellular connection, its operator, type and signal" "$(js "$o" "w.cellular===true&&w.carrier=='T-Mobile'&&w.tech=='LTE'&&w.signal==74")" "$o"
mmstub_cmd "set 0 signal=31 tech=5gnr opname=Mint"; w8 "grep -q '|5G|31' /run/rudolf/cellinfo/wwan0" 40; o=$(a "GET net")
ok "...following the modem's changes (5G, 31 %, another name)" "$(js "$o" "w.tech=='5G'&&w.signal==31&&w.carrier=='Mint'")" "$o"
mmstub_cmd "set 0 tech=umts"; w8 "grep -q '|3G|' /run/rudolf/cellinfo/wwan0" 40; ok "...3G named so" "$(grep -q '|3G|31' /run/rudolf/cellinfo/wwan0 && echo 1)" "$(cat /run/rudolf/cellinfo/wwan0)"
# System health's modem events
mmstub_cmd "set 0 sim=0"; w8 "H | grep -q 'taken out'" 40
ok "3. System health: the SIM card taken out" "$(H | grep -q '^modem | the SIM card was taken out of the modem (Quectel EG25-G)' && echo 1)" "$(H)"
mmstub_cmd "set 0 sim=1"; w8 "H | grep -q 'was put in'" 40
ok "...and put back" "$(H | grep -q 'a SIM card was put in the modem' && echo 1)" "$(H)"
mmstub_cmd "add 1 pin=1111 puk=22223333 device=/sys/devices/x"; w8 "[ -f /run/rudolf/modemstate/1 ]" 40; for i in 1 2 3; do mmcli -i 1 --pin=0000 >/dev/null 2>&1; done; w8 "H | grep -q 'PIN is blocked'" 40
ok "4. ...the PIN blocked (three wrong PINs, wherever they were given)" "$(H | grep -q "the SIM's PIN is blocked (three wrong PINs): its PUK is needed" && echo 1)" "$(H)"
mmstub_cmd "remove 1"; mmstub_cmd "set 0 reg=searching"; w8 "H | grep -q 'no network for a minute'" 80
ok "5. ...no network for a minute (ten of the watch's looks)" "$(H | grep -q 'no network for a minute: the modem (Quectel EG25-G) has found no network it may use (registration: searching)' && echo 1)" "$(H)"
mmstub_cmd "set 0 reg=home"; mmstub_cmd "remove 0"; w8 "H | grep -q 'went away'" 40; timeout 1 tail -f /dev/null
mmstub_cmd "add 0 iccid=8901260123456789012 iface=wwan0 ip4=10.64.12.34/30 gw4=10.64.12.33 enabled=1"; w8 "H | grep -q 'restarted'" 40
ok "6. ...the module gone, and back within minutes: it restarted" "$(H | grep -q 'the cellular modem went away' && H | grep -q 'the cellular module (Quectel EG25-G) restarted: it was gone for [0-9]* s' && echo 1)" "$(H)"
ok "...(System health knows the kind: rudolfd --health modem is accepted)" "$(grep -q '"security", "tor", "extension", "modem", NULL' $HERE/../../sys/rudolfd.c && echo 1)"
# the self-test's modem line
w8 "[ \"\$(sh $M status | grep -o '\"iface\":\"wwan0\"')\" ]" 60
S=$T/st; mkdir -p $S/dev $S/proc/400 $S/run $S/sys $S/persist; touch $S/dev/cdc-wdm0; echo ModemManager > $S/proc/400/comm; : > $S/run/modem.log
st() { ST_SYS=$S/sys ST_RUN=$S/run ST_PERSIST=$S/persist ST_HOME=$S ST_DEV=$S/dev ST_PROC=$S/proc ST_MODEM="sh $M" sh $SR/selftest 2>/dev/null | grep ' modem - '; }
o=$(st); ok "7. the self-test's modem line: the modem, its driver, the SIM, the network, the data connection" "$(echo "$o" | grep -q '^PASS modem - Quectel EG25-G (qmi_wwan,option): SIM T-Mobile, on T-Mobile (home, lte, signal [0-9]* %), connected on wwan0' && echo 1)" "$o"
a "POST mobile/connect?id=0&on=0" >/dev/null; o=$(st)
ok "...no data connection: a failure saying where to look" "$(echo "$o" | grep -q '^FAIL modem - .*no data connection: Connect in Settings › Mobile network, or check the APN there' && echo 1)" "$o"
mmstub_cmd "set 0 sim=0"; o=$(st); ok "...no SIM: a failure" "$(echo "$o" | grep -q '^FAIL modem - .*: no SIM card' && echo 1)" "$o"
echo "$((N-F))/$N passed"; [ $F = 0 ]
