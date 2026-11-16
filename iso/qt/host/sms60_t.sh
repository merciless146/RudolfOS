#!/bin/sh
# 1.6.0 (plan Part 6, stage A item 6): messages through the stand-in ModemManager's SMS interface (../mmstub.c, the
# real mmcli) and rudolfd's mobile/sms requests (compiled from rudolfd.c): a message received (the list, its text in any
# language, kept by the modem until deleted), its notice (the sender only; the text only after an administrator allowed
# it), one sent (the number and text in a root-only file, Unicode and long: the modem's parts, a delivery report
# asked for and given), deleted; a number or text that cannot be sent refused.
# Needs root, node, dbus-daemon, mmcli, libmm-glib-dev, mobile-broadband-provider-info.   sh sms60_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$M60S_INNER" ] && exec env M60S_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); SR=$HERE/../../sys/root/usr/lib/rudolf; M=$SR/modem; T=$(mktemp -d /tmp/sms60.XXXXXX)
MBPI=/usr/share/mobile-broadband-provider-info/serviceproviders.xml
MMSTUB_HERE=$HERE/x; . $HERE/mmstub.sh
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
trap '[ -s /run/rudolf/modem-watch.pid ] && kill $(cat /run/rudolf/modem-watch.pid) 2>/dev/null; mmstub_down; rm -rf $T' EXIT
w8() { i=0; while [ $i -lt ${2:-40} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
mount -t tmpfs none /run; mkdir -p /run/rudolf
mkdir -p $T/b $T/p; printf '#!/bin/sh\n[ "$1" = "^ModemManager\\$" ] && exit 0; exit 1\n' > $T/b/pgrep; chmod +x $T/b/pgrep
export RUDOLF_TEST_PATH=$T/b RUDOLF_MODEM_PERSIST=$T/p RUDOLF_MODEM_LOG=$T/mlog RUDOLF_RUN=/run/rudolf RUDOLF_MBPI=$MBPI RUDOLF_HEALTH=true RUDOLF_WATCH_TICK=0.3 RUDOLF_RESOLV=$T/r RUDOLF_FIREWALL=true
echo "add 0 iccid=8901260123456789012 enabled=1" > $T/init
mmstub_up $T/mm $T/init || exit 0
gcc -O2 -w -DMODEM_SH="\"sh $M\"" -DMOBILE_RUN="\"/run/rudolf\"" -o $T/api $HERE/fixtures/mobileapi.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
node $HERE/fixtures/deskls.js $T/ls.json $T/keys.json || { echo "FAIL node"; exit 1; }
a() { $T/api $T/ls.json /run/rudolf "$@"; }
js() { printf '%s' "$1" | sed 's/^HTTP[^|]*|//' | node -e "let s='';process.stdin.on('data',d=>s+=d).on('end',()=>{try{const d=JSON.parse(s),L=d.messages||[];console.log(($2)?1:0)}catch(e){console.log(0)}})"; }
body() { printf '%s' "$1" > $T/b.json; }
sh $M watch & WP=$!; w8 "[ -s /run/rudolf/modem-watch.pid ]"
mmstub_cmd "sms 0 +15559876543 Your code is 123456 — ünïcödé ✓\nsecond line"; w8 "a 'GET mobile/notes?since=0' | grep -q 'New message'" 40
o=$(a "GET mobile/sms?id=0")
ok "1. a message received: listed with its number, its text whole (any language, its new line), received" "$(js "$o" "L.length==1&&L[0].sms.content.number=='+15559876543'&&L[0].sms.content.text.startsWith('Your code is 123456 — ünïcödé ✓')&&L[0].sms.content.text.split(String.fromCharCode(10)).length==2&&L[0].sms.content.text.endsWith('second line')&&L[0].sms.properties.state=='received'")" "$o"
o=$(a "GET mobile/notes?since=0")
ok "2. its notice for both desktops (and the lock screen): the sender only, not the text" "$(echo "$o" | grep -q '"t":"💬 New message from +15559876543","b":"Open Messages to read it."' && ! echo "$o" | grep -q 123456 && echo 1)" "$o"
o=$(a "POST mobile/sms/preview?on=1"); body '{"n":"Bob","p":"bob-pass"}'; o2=$(a "POST mobile/sms/preview?on=1 <$T/b.json")
ok "3. the text in notices: refused without an administrator" "$(echo "$o" | grep -q '^HTTP/1.1 403' && echo "$o2" | grep -q 'only an administrator' && [ ! -e $T/p/sms-preview ] && echo 1)" "$o $o2"
body '{"n":"Ann","p":"pw-ann ü"}'; o=$(a "POST mobile/sms/preview?on=1 <$T/b.json"); mmstub_cmd "sms 0 +15550001111 See you at 6"; w8 "a 'GET mobile/notes?since=1' | grep -q 'See you'" 40
o2=$(a "GET mobile/notes?since=1")
ok "...allowed by an administrator: the next notice has the text" "$(echo "$o" | grep -q '^HTTP/1.1 200' && echo "$o2" | grep -q '"t":"💬 New message from +15550001111","b":"See you at 6"' && echo 1)" "$o $o2"
a "POST mobile/sms/preview?on=0" >/dev/null; ok "...turned off again (nothing needed)" "$([ ! -e $T/p/sms-preview ] && echo 1)"
L=$(a "GET mobile/notes?since=0" | grep -o '"n":' | wc -l); timeout 3 tail -f /dev/null
ok "4. each message noted once" "$([ "$(a 'GET mobile/notes?since=0' | grep -o '"n":' | wc -l)" = "$L" ] && echo 1)"
TXT=$(node -e "console.log('Привет! '+'Это длинное сообщение. '.repeat(6))"); node -e "require('fs').writeFileSync('$T/b.json',JSON.stringify({number:'+1 (555) 222-3333',text:process.argv[1]}))" "$TXT"
: > $MMS_D/calls; o=$(a "POST mobile/sms/send?id=0 <$T/b.json")
ok "5. sent: a long Unicode message (the modem's parts: 3), a delivery report asked for; the text in a root-only file, removed" "$(echo "$o" | grep -q '"ok":true,"id":[0-9]' && grep -q "Create number=+15552223333 text-length=$(printf '%s' "$TXT" | node -e "let s='';process.stdin.on('data',d=>s+=d).on('end',()=>console.log([...s].length))") report=1" $MMS_D/calls && grep -q '0 Send parts=3' $MMS_D/calls && [ ! -e /run/rudolf/sms-req ] && [ ! -e /run/rudolf/sms/out.txt ] && echo 1)" "$o $(mmstub_log)"
ID=$(echo "$o" | sed -n 's/.*"id":\([0-9]*\).*/\1/p'); timeout 1 tail -f /dev/null; o=$(a "GET mobile/sms?id=0")
ok "...listed as sent, its delivery report in (received by the phone)" "$(js "$o" "L.some(x=>x.id==$ID&&x.sms.properties.state=='sent'&&x.sms.properties['delivery-state'].startsWith('completed-received')&&x.sms.content.text.startsWith('Привет!'))")" "$o"
o=$(a "POST mobile/sms/delete?id=0&sms=$ID"); o2=$(a "GET mobile/sms?id=0")
ok "6. deleted from the modem" "$(echo "$o" | grep -q deleted && js "$o2" "!L.some(x=>x.id==$ID)" | grep -q 1 && echo 1)" "$o $o2"
node -e "require('fs').writeFileSync('$T/b.json',JSON.stringify({number:'call me',text:'x'}))"; o=$(a "POST mobile/sms/send?id=0 <$T/b.json")
node -e "require('fs').writeFileSync('$T/b.json',JSON.stringify({number:'+15551112222',text:''}))"; o2=$(a "POST mobile/sms/send?id=0 <$T/b.json"); o3=$(a "POST mobile/sms/delete?id=0&sms=x1")
ok "7. refused: not a number, an empty text, a message id that is not one" "$(printf '%s\n' "$o" "$o2" "$o3" | grep -c '^HTTP/1.1 400' | grep -qx 3 && echo 1)" "$o | $o2 | $o3"
mmstub_cmd "set 0 reg=searching"; node -e "require('fs').writeFileSync('$T/b.json',JSON.stringify({number:'+15551112222',text:'hello'}))"; o=$(a "POST mobile/sms/send?id=0 <$T/b.json")
ok "...no network: not sent, said" "$(echo "$o" | grep -q '^HTTP/1.1 400.*not sent' && echo 1)" "$o"
kill $WP 2>/dev/null
echo "$((N-F))/$N passed"; [ $F = 0 ]
