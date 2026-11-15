#!/bin/sh
# 1.6.0 (plan Part 2, stage A item 2): the SIM and the network, through rudolfd's mobile requests (compiled from
# rudolfd.c) and /usr/lib/rudolf/modem, against the stand-in ModemManager (../mmstub.c) with the real mmcli: the PIN
# (administrators only, three tries, the codes never on a command line), the PUK with a new PIN (ten tries, then the
# SIM blocked for good), System health told; the APN from the operators' database by the SIM's operator code and name,
# Rudolf's corrections first, the letters' case kept (docs/plan-1.6.1.md Part 7), or typed in; roaming off by default;
# the network type; airplane mode (Wi-Fi, Bluetooth and the modem off together, and back as they were).
# Needs root, node, dbus-daemon, mmcli, libmm-glib-dev and mobile-broadband-provider-info.   sh sim60_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$S60_INNER" ] && exec env S60_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); M=$HERE/../../sys/root/usr/lib/rudolf/modem; T=$(mktemp -d /tmp/sim60.XXXXXX)
MBPI=/usr/share/mobile-broadband-provider-info/serviceproviders.xml
[ -r $MBPI ] || { echo "SKIP: needs mobile-broadband-provider-info"; exit 0; }
MMSTUB_HERE=$HERE/x; . $HERE/mmstub.sh
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 700)"; F=$((F+1)); fi; }
trap 'mmstub_down; rm -rf $T' EXIT
mount -t tmpfs none /run; mkdir -p /run/rudolf
mkdir -p $T/b $T/p $T/rf/rfkill0 $T/rf/rfkill1 $T/rf/rfkill2 $T/sys
echo wlan > $T/rf/rfkill0/type; echo 0 > $T/rf/rfkill0/soft; echo bluetooth > $T/rf/rfkill1/type; echo 1 > $T/rf/rfkill1/soft; echo wwan > $T/rf/rfkill2/type; echo 0 > $T/rf/rfkill2/soft
printf '#!/bin/sh\n[ "$2" = ModemManager ] && exit 0; exit 1\n' > $T/b/pgrep; chmod +x $T/b/pgrep
export RUDOLF_TEST_PATH=$T/b RUDOLF_MODEM_SYS=$T/sys RUDOLF_MODEM_PERSIST=$T/p RUDOLF_MODEM_LOG=$T/mlog RUDOLF_RUN=/run/rudolf RUDOLF_RFKILL=$T/rf RUDOLF_MBPI=$MBPI RUDOLF_APN_FIXES=$HERE/../../sys/root/usr/share/rudolf/apn-fixes
cat > $T/init <<E
add 0 pin=1234 puk=12345678 iccid=8901260123456789012 spn=T-Mobile
add 1 pin=4321 puk=87654321 iccid=8901260999999999999 spn=US%20Mobile modes=3g,4g,5g
E
mmstub_up $T/mm $T/init || exit 0
gcc -O2 -w -DMODEM_SH="\"sh $M\"" -o $T/api $HERE/fixtures/mobileapi.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
node $HERE/fixtures/deskls.js $T/ls.json $T/keys.json || { echo "FAIL node"; exit 1; }
a() { $T/api $T/ls.json /run/rudolf "$@"; }
body() { printf '%s' "$1" > $T/b.json; }
st() { sh $M status; }
js() { printf '%s' "$1" | sed 's/^HTTP[^|]*|//' | node -e "let s='';process.stdin.on('data',d=>s+=d).on('end',()=>{try{const d=JSON.parse(s),m=d.modems;console.log(($2)?1:0)}catch(e){console.log(0)}})"; }
health() { cat /run/rudolf/health*.log 2>/dev/null; }
ADM='"n":"Ann","p":"pw-ann ü"'

body '{"pin":"1234"}'; o=$(a "POST mobile/pin?id=0 <$T/b.json")
ok "1. the PIN: refused without an administrator's name and password" "$(echo "$o" | grep -q '^HTTP/1.1 403' && ! grep -q SendPin $MMS_D/calls && echo 1)" "$o"
body '{"n":"Bob","p":"bob-pass","pin":"1234"}'; o=$(a "POST mobile/pin?id=0 <$T/b.json")
ok "...and to an account that is not an administrator" "$(echo "$o" | grep -q 'only an administrator' && ! grep -q SendPin $MMS_D/calls && echo 1)" "$o"
body "{$ADM,\"pin\":\"12a4\"}"; o=$(a "POST mobile/pin?id=0 <$T/b.json")
ok "...a PIN that is not 4 to 8 digits is refused before the SIM is asked" "$(echo "$o" | grep -q '^HTTP/1.1 400.*\|4 to 8 digits' && ! grep -q SendPin $MMS_D/calls && echo 1)" "$o"
body "{$ADM,\"pin\":\"1111\"}"; o=$(a "POST mobile/pin?id=0 <$T/b.json")
ok "2. a wrong PIN: said, with the tries left (2)" "$(echo "$o" | grep -q '^HTTP/1.1 400' && echo "$o" | grep -q 'Wrong PIN. 2 tries left.' && echo 1)" "$o"
ok "...System health told (not the PIN)" "$(health | grep -q 'a wrong PIN for the SIM, given by Ann' && ! health | grep -q 1111 && echo 1)" "$(health)"
ok "...the codes never on a command line: given in a root-only file, removed after" "$([ ! -e /run/rudolf/mobile-codes ] && grep -q 'SendPin 1111' $MMS_D/calls && echo 1)" "$(ls -la /run/rudolf)"
body "{$ADM,\"pin\":\"1234\"}"; o=$(a "POST mobile/pin?id=0 <$T/b.json"); timeout 0.5 tail -f /dev/null
ok "3. the right PIN: unlocked, the modem turned on and on the network" "$(echo "$o" | grep -q '^HTTP/1.1 200' && echo "$o" | grep -q 'PIN accepted' && grep -qx '0 Enable 1' $MMS_D/calls && js "$(st)" "m.find(x=>x.id==0).state=='registered'" | grep -q 1 && echo 1)" "$o $(st | head -c 300)"
ok "...System health: unlocked by whom" "$(health | grep -q "the SIM's PIN was given by Ann: unlocked" && echo 1)" "$(health)"
for c in 1111 2222; do body "{$ADM,\"pin\":\"$c\"}"; a "POST mobile/pin?id=1 <$T/b.json" >/dev/null; done
body "{$ADM,\"pin\":\"3333\"}"; o=$(a "POST mobile/pin?id=1 <$T/b.json")
ok "4. three wrong PINs: the SIM blocked, its PUK needed (said, and in System health)" "$(echo "$o" | grep -q 'The SIM is now blocked: its PUK is needed' && js "$(st)" "m.find(x=>x.id==1).locked=='sim-puk'&&m.find(x=>x.id==1).retries.puk==10" | grep -q 1 && health | grep -q 'PIN is blocked: three wrong PINs' && echo 1)" "$o"
body "{$ADM,\"pin\":\"4321\"}"; o=$(a "POST mobile/pin?id=1 <$T/b.json")
ok "...then even the right PIN is refused: the PUK is needed" "$(echo "$o" | grep -q 'its PUK is needed' && echo 1)" "$o"
body "{$ADM,\"puk\":\"11112222\",\"pin\":\"5678\"}"; o=$(a "POST mobile/puk?id=1 <$T/b.json")
ok "5. a wrong PUK: said, with the tries left before the SIM is blocked for good" "$(echo "$o" | grep -q 'Wrong PUK. 9 tries left before the SIM is blocked for good.' && echo 1)" "$o"
body "{$ADM,\"puk\":\"1234\",\"pin\":\"5678\"}"; o=$(a "POST mobile/puk?id=1 <$T/b.json")
ok "...a PUK that is not 8 digits is refused before the SIM is asked" "$(echo "$o" | grep -q 'A PUK is 8 digits' && [ "$(grep -c SendPuk $MMS_D/calls)" = 1 ] && echo 1)" "$o"
body "{$ADM,\"puk\":\"87654321\",\"pin\":\"5678\"}"; o=$(a "POST mobile/puk?id=1 <$T/b.json"); timeout 0.5 tail -f /dev/null
ok "6. the right PUK with a new PIN: unlocked, the modem on" "$(echo "$o" | grep -q 'PUK accepted: the SIM is unlocked, with the new PIN.' && grep -qx '1 SendPuk 87654321 5678' $MMS_D/calls && js "$(st)" "m.find(x=>x.id==1).state=='registered'" | grep -q 1 && echo 1)" "$o"
mmstub_cmd "add 2 pin=1111 puk=99998888"; for i in 1 2 3; do mmcli -i 2 --pin=0000 >/dev/null 2>&1; done
for i in 1 2 3 4 5 6 7 8 9; do mmcli -i 2 --puk=00000000 --pin=1111 >/dev/null 2>&1; done
body "{$ADM,\"puk\":\"00000000\",\"pin\":\"1111\"}"; o=$(a "POST mobile/puk?id=2 <$T/b.json")
ok "7. the tenth wrong PUK: the SIM blocked for good, said, and in System health" "$(echo "$o" | grep -q 'blocked for good: only your operator can replace it' && js "$(st)" "m.find(x=>x.id==2).state=='failed'&&m.find(x=>x.id==2).failed=='sim-error'" | grep -q 1 && health | grep -q 'SIM is blocked for good' && echo 1)" "$o $(st | head -c 400)"
mmstub_cmd "remove 2"

# the operator's settings
o=$(a "GET mobile/settings?id=0")
ok "8. the APN found by itself: T-Mobile's from the operators' database, by the SIM's operator code (310260) and name" "$(js "$o" "d.ok&&d.simOp=='310260'&&d.auto&&d.auto.name=='T-Mobile'&&d.auto.apn=='fast.t-mobile.com'&&d.auto.source=='database'&&d.apn==''")" "$o"
ok "...roaming off by default; the network types this modem offers (3G and 4G: no 5G)" "$(js "$o" "d.roaming===false&&d.mode=='auto'&&d.modes.join()=='auto,4g,3g'")" "$o"
o=$(a "GET mobile/settings?id=1")
ok "9. a reseller on the same network (US Mobile): Rudolf's correction first, its APN in capitals as it must be (PWG)" "$(js "$o" "d.simName=='US Mobile'&&d.auto.apn=='PWG'&&d.auto.source=='rudolf'")" "$o"
for l in "310260|Mint|Mint Mobile|wholesale|database" "310260|T-Mobile|T-Mobile|fast.t-mobile.com|database" "310260|US Mobile|US Mobile|PWG|rudolf" "310260|SIMPLE MOBILE|Simple Mobile|Simple|rudolf" "310410|Straight Talk|Straight Talk|att.mvno|database" "26201||T-Mobile(Telekom)|internet.t-d1.de|database" "23410|O2 - UK|O2|mobile.o2.co.uk|database"; do
	IFS='|' read -r mm sp nm ap src <<E
$l
E
	r=$(sh $M apn "$mm" "$sp"); [ "$r" = "$(sh $M apn "$mm" "$sp" | head -1)" ] && [ "$(echo "$r" | cut -d'|' -f1,2,5)" = "$nm|$ap|$src" ] || break; done
ok "...the table of carriers: each SIM's operator code and name gives its APN exactly (case kept), resellers apart" "$([ "$(echo "$r" | cut -d'|' -f1,2,5)" = "$nm|$ap|$src" ] && echo 1)" "$mm $sp: $r (wanted $nm|$ap|$src)"
o=$(a "POST mobile/settings?id=0&k=apn&v=Wholesale"); o2=$(a "GET mobile/settings?id=0")
ok "10. an APN typed in: kept exactly, capitals and all" "$(echo "$o" | grep -q '^HTTP/1.1 200' && js "$o2" "d.apn=='Wholesale'&&d.auto.apn=='fast.t-mobile.com'" | grep -q 1 && echo 1)" "$o $o2"
o=$(a "POST mobile/settings?id=0&k=apnuser&v=me")$(a "POST mobile/settings?id=0&k=apnpass&v=s3cret pass"); o2=$(a "GET mobile/settings?id=0")
ok "...its user name and password; the password never given back" "$(js "$o2" "d.apnuser=='me'&&d.apnpass===true" | grep -q 1 && ! echo "$o2" | grep -q s3cret && echo 1)" "$o $o2"
CF=$(ls $T/p/mobile/*.conf | head -1)
ok "...kept in the SIM's own file: named by a hash (never the SIM's number), root's only" "$([ "$(stat -c %a $CF)" = 600 ] && [ "$(stat -c %a $T/p/mobile)" = 700 ] && ! ls $T/p/mobile | grep -q 8901260 && grep -qx 'apn=Wholesale' $CF && echo 1)" "$(ls -la $T/p/mobile)"
o=$(a "POST mobile/settings?id=0&k=apn&v=bad'apn"); o2=$(a "POST mobile/settings?id=0&k=apn&v=has%20space"); o3=$(a "POST mobile/settings?id=0&k=apnpass&v=a,b"); o4=$(a "POST mobile/settings?id=0&k=evil&v=1")
ok "...refused: a quote, a space in an APN, a comma in a password (the modem cannot take it), an unknown setting" "$(printf '%s\n' "$o" "$o2" "$o3" "$o4" | grep -c '^HTTP/1.1 400' | grep -qx 4 && grep -qx 'apn=Wholesale' $CF && echo 1)" "$o | $o2 | $o3 | $o4"
o=$(a "POST mobile/settings?id=0&k=apn&v="); o2=$(a "GET mobile/settings?id=0")
ok "...emptied: automatic again (the user name and password go with it)" "$(js "$o2" "d.apn==''&&d.apnuser==''&&d.apnpass===false")" "$o2"
: > $MMS_D/calls; o=$(a "POST mobile/settings?id=0&k=mode&v=4g"); o2=$(a "GET mobile/settings?id=0")
ok "11. the network type: 4G only, given to the modem at once, and kept" "$(echo "$o" | grep -q '^HTTP/1.1 200' && grep -qx '0 SetCurrentModes 8 0' $MMS_D/calls && js "$o2" "d.mode=='4g'" | grep -q 1 && grep -qx 'mode=4g' $CF && echo 1)" "$o $(mmstub_log)"
o=$(a "POST mobile/settings?id=0&k=mode&v=5g")
ok "...5G refused by a modem without it (said)" "$(echo "$o" | grep -q 'cannot use only 5g' && echo 1)" "$o"
: > $MMS_D/calls; o=$(a "POST mobile/settings?id=1&k=mode&v=5g"); o2=$(a "GET mobile/settings?id=1")
ok "...a 5G modem: 5G preferred (with 4G)" "$(grep -qx '1 SetCurrentModes 24 16' $MMS_D/calls && js "$o2" "d.mode=='5g'&&d.modes.join()=='auto,5g,4g,3g'" | grep -q 1 && echo 1)" "$o $o2 $(mmstub_log)"
a "POST mobile/settings?id=1&k=mode&v=auto" >/dev/null; o2=$(a "GET mobile/settings?id=1")
ok "...automatic again: all its types" "$(js "$o2" "d.mode=='auto'")" "$o2"
o=$(a "POST mobile/settings?id=0&k=roaming&v=1"); o2=$(a "GET mobile/settings?id=0")
ok "12. roaming turned on: kept, and System health says so (it may cost)" "$(js "$o2" "d.roaming===true" | grep -q 1 && health | grep -q 'when roaming was turned on' && echo 1)" "$o2"
o=$(a "POST mobile/settings?id=9&k=roaming&v=1"); ok "...a modem that is not there: refused" "$(echo "$o" | grep -q '^HTTP/1.1 400' && echo 1)" "$o"

# airplane mode
: > $MMS_D/calls; o=$(a "POST mobile/airplane?on=1"); o2=$(a "GET mobile/airplane")
ok "13. airplane mode on: Wi-Fi, Bluetooth and the cellular radio blocked (rfkill), the modems turned off" "$(echo "$o2" | grep -q '"on":true' && [ "$(cat $T/rf/rfkill0/soft)$(cat $T/rf/rfkill1/soft)$(cat $T/rf/rfkill2/soft)" = 111 ] && grep -qx '0 Enable 0' $MMS_D/calls && grep -qx '1 Enable 0' $MMS_D/calls && echo 1)" "$o $o2 $(mmstub_log)"
o=$(sh $HERE/../../sys/root/usr/lib/rudolf/wifi.sh reconnect 2>&1; echo rc=$?)
ok "...the Wi-Fi does not start under it (said)" "$(echo "$o" | grep -q 'Airplane mode is on' && echo "$o" | grep -q rc=5 && echo 1)" "$o"
: > $MMS_D/calls; o=$(a "POST mobile/airplane?on=0"); o2=$(a "GET mobile/airplane")
ok "14. off: back as they were (Bluetooth was off before, so it stays off), the modems on again" "$(echo "$o2" | grep -q '"on":false' && [ "$(cat $T/rf/rfkill0/soft)$(cat $T/rf/rfkill1/soft)$(cat $T/rf/rfkill2/soft)" = 010 ] && grep -qx '0 Enable 1' $MMS_D/calls && grep -qx '1 Enable 1' $MMS_D/calls && echo 1)" "$o $o2 $(mmstub_log)"
o=$(a "POST mobile/airplane?on=yes"); ok "...(on=1 or 0 only)" "$(echo "$o" | grep -q '^HTTP/1.1 400' && echo 1)" "$o"
echo "$((N-F))/$N passed"; [ $F = 0 ]
