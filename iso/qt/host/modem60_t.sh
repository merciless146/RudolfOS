#!/bin/sh
# 1.6.0 (plan Part 1, stage A item 1): ModemManager's D-Bus API read for Settings (/usr/lib/rudolf/modem status, the
# real mmcli talking to the stand-in ModemManager, ../mmstub.c, on a private bus): a modem with a locked SIM, unlocked,
# on the network, its signal and network type changing, its SIM taken out, a second modem, one that disappears; the
# FCC unlock (rudolfd's mobile/fcc: administrators only, their password checked by rudolfd, kept, put in place when
# ModemManager starts); the modules Linux supports only partly named. Needs root (a private mount namespace for
# rudolfd's files), node, dbus-daemon, mmcli and libmm-glib-dev.   sh modem60_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$M60_INNER" ] && exec env M60_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); M=$HERE/../../sys/root/usr/lib/rudolf/modem; T=$(mktemp -d /tmp/modem60.XXXXXX)
MMSTUB_HERE=$HERE/x; . $HERE/mmstub.sh
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 700)"; F=$((F+1)); fi; }
trap 'mmstub_down; rm -rf $T' EXIT
mount -t tmpfs none /run; mkdir -p /run/rudolf
# the computer: a USB modem (Quectel EM05-G, 2c7c:030a, which needs the FCC unlock in some laptops) and an Intel
# XMM7360 on PCI (Linux has no driver for it); ModemManager's FCC scripts (stand-ins), rudolfd's persistent folder
S=$T/sys; U=$S/devices/pci0000:00/0000:00:14.0/usb1/1-2; mkdir -p $U $S/bus/pci/devices/0000:05:00.0 $S/bus/usb/devices $T/fa $T/fd $T/p $T/b
echo 2c7c > $U/idVendor; echo 030a > $U/idProduct; ln -s ../../../devices/pci0000:00/0000:00:14.0/usb1/1-2 $S/bus/usb/devices/1-2
echo 0x8086 > $S/bus/pci/devices/0000:05:00.0/vendor; echo 0x7360 > $S/bus/pci/devices/0000:05:00.0/device
printf '#!/bin/sh\nexit 0\n' > $T/fa/2c7c; ln -s 2c7c $T/fa/2c7c:030a
# (ModemManager "runs": the stand-in is on the bus; pgrep answers for it, so nothing of the computer's is looked at)
printf '#!/bin/sh\n[ "$1" = "^ModemManager\$" ] && [ ! -e %s/mm-off ] && exit 0; exit 1\n' $T > $T/b/pgrep; chmod +x $T/b/pgrep
export RUDOLF_TEST_PATH=$T/b RUDOLF_MODEM_SYS=$S RUDOLF_MM_FCC_AVAIL=$T/fa RUDOLF_MM_FCC_DIR=$T/fd RUDOLF_MODEM_PERSIST=$T/p RUDOLF_MODEM_LOG=$T/mlog RUDOLF_RUN=$T/run
echo "add 0 pin=1234 puk=12345678 device=/sys/devices/pci0000:00/0000:00:14.0/usb1/1-2 model=EM05-G rev=EM05GFAR07A07M1G" > $T/init
mmstub_up $T/mm $T/init || exit 0
st() { sh $M status; }
js() { printf '%s' "$1" | node -e "let s='';process.stdin.on('data',d=>s+=d).on('end',()=>{try{const d=JSON.parse(s),m=d.modems;console.log(($2)?1:0)}catch(e){console.log(0)}})"; }

o=$(mmcli -L 2>&1); ok "1. the stand-in ModemManager answers the real mmcli on its own bus" "$(echo "$o" | grep -q 'Modem/0 \[Quectel\] EM05-G' && echo 1)" "$o"
o=$(st)
ok "2. status: the modem with a locked SIM: the PIN needed, three tries and ten for the PUK, the SIM's operator code" "$(js "$o" "d.running&&m.length==1&&m[0].maker=='Quectel'&&m[0].model=='EM05-G'&&m[0].state=='locked'&&m[0].locked=='sim-pin'&&m[0].retries.pin==3&&m[0].retries.puk==10&&m[0].sim&&m[0].simOp=='310260'&&m[0].simName=='T-Mobile'")" "$o"
ok "3. ...its drivers, plug-in, port, firmware, and the IMEI shortened (it identifies the device)" "$(js "$o" "m[0].driver=='qmi_wwan,option'&&m[0].plugin=='quectel'&&m[0].port=='cdc-wdm0'&&m[0].firmware=='EM05GFAR07A07M1G'&&m[0].imei=='861234…'")" "$o"
mmcli -i 0 --pin=1234 >/dev/null 2>&1; mmcli -m 0 -e >/dev/null 2>&1; timeout 0.4 tail -f /dev/null; o=$(st)
ok "4. unlocked and on: registered on its home network, the operator, LTE, the signal" "$(js "$o" "m[0].state=='registered'&&!m[0].locked&&m[0].operator=='T-Mobile'&&m[0].opcode=='310260'&&m[0].reg=='home'&&m[0].tech=='lte'&&m[0].signal=='70'&&m[0].power=='on'")" "$o"
mmstub_cmd "set 0 signal=38 tech=5gnr reg=roaming opname=AT&T mccmnc=310410"; o=$(st)
ok "5. the values change with the modem's: 5G, roaming on another operator, a weaker signal" "$(js "$o" "m[0].signal=='38'&&m[0].tech=='lte,5gnr'&&m[0].reg=='roaming'&&m[0].operator=='AT&T'")" "$o"
mmstub_cmd "set 0 sim=0"; o=$(st)
ok "6. the SIM taken out: no SIM, failed (sim-missing)" "$(js "$o" "!m[0].sim&&m[0].state=='failed'&&m[0].failed=='sim-missing'")" "$o"
mmstub_cmd "add 1 maker=Sierra model=EM7455 drivers=cdc_mbim port=cdc-wdm1 iface=wwan1 device=/sys/devices/none"; mmstub_cmd "remove 0"; o=$(st)
ok "7. a second modem; the first one disappears (unplugged): only the second is listed" "$(js "$o" "m.length==1&&m[0].id==1&&m[0].model=='EM7455'&&m[0].state=='disabled'&&m[0].usb==''&&m[0].fcc=='none'")" "$o"
mmstub_cmd "remove 1"; o=$(st)
ok "8. none left: an empty list (ModemManager still running until udev's last remove)" "$(js "$o" "d.running&&m.length==0")" "$o"
ok "9. the modules Linux supports only partly are named, with what is known (an Intel XMM7360 on PCI here)" "$(js "$o" "d.notes.length==1&&d.notes[0].id=='8086:7360'&&/XMM7360/.test(d.notes[0].name)&&/no driver/.test(d.notes[0].text)")" "$o"
mmstub_cmd "add 2 device=/sys/devices/pci0000:00/0000:00:14.0/usb1/1-2 fcc=1"; o=$(st)
ok "10. a module whose radio starts locked (FCC): ModemManager's script for it is known, the unlock off" "$(js "$o" "m[0].usb=='2c7c:030a'&&m[0].fcc=='off'&&m[0].fccId=='2c7c:030a'")" "$o"
o=$(mmcli -m 2 -e 2>&1); ok "...(the stand-in refuses to turn its radio on, as such a module does)" "$(echo "$o" | grep -qi 'FCC unlock required' && echo 1)" "$o"

# rudolfd's mobile requests, compiled from rudolfd.c with the tree's script; the page's accounts (Ann: an administrator)
gcc -O2 -w -DMODEM_SH="\"sh $M\"" -o $T/api $HERE/fixtures/mobileapi.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
node $HERE/fixtures/deskls.js $T/ls.json $T/keys.json || { echo "FAIL node"; exit 1; }
mkdir -p $T/run; a() { $T/api $T/ls.json $T/run "$@"; }
body() { printf '%s' "$1" > $T/b.json; }
o=$(a "GET mobile/status")
ok "11. rudolfd: mobile/status gives the script's answer" "$(echo "$o" | grep -q '^HTTP/1.1 200' && echo "$o" | grep -q '"ok":true,"running":true,"modems":\[{"id":2,' && echo 1)" "$o"
o=$(a "POST mobile/fcc?id=2c7c:030a&on=1")
ok "12. the FCC unlock: refused without an administrator's name and password" "$(echo "$o" | grep -q '^HTTP/1.1 403' && [ ! -e $T/fd/2c7c:030a ] && echo 1)" "$o"
body '{"n":"Bob","p":"bob-pass"}'; o=$(a "POST mobile/fcc?id=2c7c:030a&on=1 <$T/b.json")
ok "...refused to an account that is not an administrator" "$(echo "$o" | grep -q '^HTTP/1.1 403.*\|only an administrator' && [ ! -e $T/fd/2c7c:030a ] && echo 1)" "$o"
body '{"n":"Ann","p":"wrong"}'; o=$(a "POST mobile/fcc?id=2c7c:030a&on=1 <$T/b.json")
ok "...refused with a wrong password" "$(echo "$o" | grep -q 'Wrong password' && [ ! -e $T/fd/2c7c:030a ] && echo 1)" "$o"
for bad in '..%2Fx' '2C7C:030A' '2c7c:030' 'ffff:ffff'; do body '{"n":"Ann","p":"pw-ann ü"}'; o=$(a "POST mobile/fcc?id=$bad&on=1 <$T/b.json"); echo "$o" | grep -q '^HTTP/1.1 400' || break; done
ok "...an id that is not a module's (or one ModemManager has no script for) is refused" "$(echo "$o" | grep -q '^HTTP/1.1 400' && [ -z "$(ls $T/fd)" ] && echo 1)" "$bad: $o"
: > $MMS_D/calls; body '{"n":"Ann","p":"pw-ann ü"}'; o=$(a "POST mobile/fcc?id=2c7c:030a&on=1 <$T/b.json")
ok "13. an administrator turns it on: ModemManager's script linked where it runs it, kept, the modem turned on" "$(echo "$o" | grep -q '^HTTP/1.1 200' && [ "$(readlink $T/fd/2c7c:030a)" = "$T/fa/2c7c:030a" ] && grep -qx 2c7c:030a $T/p/modem-fcc && grep -qx '2 Enable 1' $MMS_D/calls && echo 1)" "$o $(ls -l $T/fd) $(mmstub_log)"
ok "...and System health says who did it" "$(grep -rqs 'FCC unlock of the cellular module 2c7c:030a was turned on by Ann' /run/rudolf/ $T/run && echo 1)" "$(ls -R /run/rudolf $T/run)"
o=$(st); ok "14. ...status says it is on" "$(js "$o" "m[0].fcc=='on'")" "$o"
o=$(a "POST mobile/fcc?id=2c7c:030a&on=0 <$T/b.json")
ok "15. turned off: the link goes, and it is no longer kept" "$(echo "$o" | grep -q '^HTTP/1.1 200' && [ ! -e $T/fd/2c7c:030a ] && ! grep -q 2c7c $T/p/modem-fcc && echo 1)" "$o"
o=$(a "GET mobile/fcc?id=2c7c:030a&on=1"); ok "...(a GET changes nothing)" "$(echo "$o" | grep -q '^HTTP/1.1 405' && echo 1)" "$o"
# kept across a restart: ModemManager's start puts the kept ones in place first (the link removed as a fresh start has it)
echo 2c7c:030a > $T/p/modem-fcc; echo 'nonsense/../x' >> $T/p/modem-fcc; rm -f $T/fd/*; touch $T/mm-off
printf '#!/bin/sh\n[ "$1" = --help ] && exit 0\necho "MM $*" >> %s/mmlog\n' $T > $T/b/ModemManager; printf '#!/bin/sh\nexec "$@"\n' > $T/b/setsid; printf '#!/bin/sh\nexit 0\n' > $T/b/dbus-daemon; chmod +x $T/b/*
o=$(RUDOLF_DEVICES=$T/udev sh -c "echo udev > $T/udev; sh $M start" 2>&1)
ok "16. at ModemManager's start the kept FCC unlocks are put in place (only valid ids), then it starts" "$([ "$(readlink $T/fd/2c7c:030a)" = "$T/fa/2c7c:030a" ] && [ "$(ls $T/fd | wc -l)" = 1 ] && grep -q '^MM --log-level=WARN' $T/mmlog && echo 1)" "$o $(ls $T/fd) $(cat $T/mmlog 2>&1)"
rm -f $T/mm-off
# no ModemManager at all (no bus): an empty list, the notes still there
o=$(DBUS_SYSTEM_BUS_ADDRESS=unix:path=$T/nobus sh $M status)
ok "17. ModemManager not there: not running, no modems, the notes still given" "$(js "$o" "!d.running&&m.length==0&&d.notes.length==1")" "$o"
echo "$((N-F))/$N passed"; [ $F = 0 ]
