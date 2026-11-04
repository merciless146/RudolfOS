#!/bin/sh
# 1.5.1: the Bluetooth pairing agent (/usr/lib/rudolf/bluetooth agent-run, pair, answer, status) against a stand-in
# bluetoothctl (fixtures/fake-bluetoothctl-agent) and a stand-in bluetoothd, with busybox's tools as on Rudolf OS.
# sh btagent_t.sh (about 50 s: one question is left to run out of time)
HERE=$(cd "$(dirname "$0")" && pwd); B=$HERE/../../sys/root/usr/lib/rudolf/bluetooth
T=$(mktemp -d); mkdir -p $T/run $T/bt $T/bin $T/bd
cleanup() { : > $T/run/bluetooth-stop; kill $AP $BDP 2>/dev/null; sleep 1; rm -rf $T; }; trap cleanup EXIT
for a in $(busybox --list); do case $a in sh|ash) ;; *) ln -s "$(command -v busybox)" $T/bin/$a;; esac; done
mkdir -p $T/sys/hci0; export RUDOLF_BT_SYSFS=$T/sys RUDOLF_TEST_PATH=$T/bin RUDOLF_RUN=$T/run RUDOLF_BT_BUS=$T/bus FAKEBT=$T/bt RUDOLF_BLUETOOTHCTL=$HERE/fixtures/fake-bluetoothctl-agent
python3 -c "import socket,sys;socket.socket(socket.AF_UNIX).bind(sys.argv[1])" $T/bus
cp "$(command -v sleep)" $T/bd/bluetoothd; (cd $T/bd && exec ./bluetoothd 600) & BDP=$!
PHONE=DC:F0:90:56:8E:AE; HP=AA:BB:CC:DD:EE:10; KB=AA:BB:CC:DD:EE:11
echo "REDMAGIC 9 Pro" > $T/bt/name_$PHONE; echo "WH-1000XM4" > $T/bt/name_$HP; echo "K380" > $T/bt/name_$KB
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
waitf() { for i in $(seq ${2:-40}); do [ -e "$1" ] && return 0; sleep 0.25; done; return 1; }
SH="busybox sh"
$SH $B agent-run > $T/agent.log 2>&1 & AP=$!
waitf $T/run/btagent.pid; sleep 3
ok "1. the agent starts and registers as the default agent (KeyboardDisplay)" $(grep -q "^agent KeyboardDisplay" $T/bt/commands && grep -q "^default-agent" $T/bt/commands && echo 1 || echo 0) "$(cat $T/bt/commands $T/agent.log 2>&1)"
# a pairing from Settings: the phone's code is asked; Yes
$SH $B pair $PHONE > $T/p1 2>&1 & P=$!
waitf $T/run/bt-ask; a=$(cat $T/run/bt-ask 2>/dev/null)
ok "2. a phone's code to compare is asked, with its name" $( [ "$(has "$a" '"kind":"confirm","code":"482913","mac":"'$PHONE'","name":"REDMAGIC 9 Pro"')" = 1 ] && echo 1 || echo 0) "$a"
s=$($SH $B status); echo "$s" | python3 -c 'import json,sys;json.load(sys.stdin)' 2>/dev/null; j=$?
ok "3. status is JSON and carries the question" $([ $j = 0 ] && [ "$(has "$s" '"ask":{"kind":"confirm"')" = 1 ] && echo 1 || echo 0) "$s"
ok "4. ...and the phone, connected but not bonded yet, is not shown as paired" $(has "$s" '"mac":"'$PHONE'","name":"REDMAGIC 9 Pro","icon":"","paired":false') "$s"
$SH $B answer yes; wait $P; r=$?
ok "5. Yes: the pairing finishes, is kept (bonded) and the phone is trusted" $([ $r = 0 ] && [ -e $T/bt/bonded_$PHONE ] && [ -e $T/bt/trusted_$PHONE ] && grep -q "Pairing successful" $T/p1 && echo 1 || echo 0) "$r $(cat $T/p1)"
ok "6. the question is gone once answered" $([ ! -e $T/run/bt-ask ] && echo 1 || echo 0)
# No
$SH $B pair $HP > $T/p2 2>&1 & P=$!
waitf $T/run/bt-ask; $SH $B answer no; wait $P; r=$?
ok "7. No: the pairing fails, says so, and nothing is kept" $([ $r != 0 ] && [ ! -e $T/bt/bonded_$HP ] && echo 1 || echo 0) "$r $(cat $T/p2)"
ok "8. an answer with no question is refused" $($SH $B answer yes >/dev/null 2>&1 && echo 0 || echo 1)
# no answer: no after 30 s
rm -f $T/bt/connected_$HP; $SH $B pair $HP > $T/p3 2>&1 & P=$!
waitf $T/run/bt-ask; t0=$(date +%s); wait $P; r=$?; t1=$(date +%s)
ok "9. a question nobody answers is answered no after 30 s" $([ $r != 0 ] && [ $((t1 - t0)) -ge 28 ] && [ $((t1 - t0)) -le 40 ] && tail -1 $T/bt/answers | grep -qx no && echo 1 || echo 0) "$r $((t1 - t0)) s $(tail -1 $T/bt/answers)"
# the other side starts it: a device asks to pair
rm -f $T/bt/connected_*; echo "incoming $HP" > $T/run/bt-agent.in
waitf $T/run/bt-ask; a=$(cat $T/run/bt-ask 2>/dev/null)
ok "10. a device that asks to pair is asked about, with its name" $(has "$a" '"kind":"accept","code":"","mac":"'$HP'","name":"WH-1000XM4"') "$a"
$SH $B answer yes; sleep 14
ok "11. ...Pair: it is bonded, and trusted afterwards so it can connect again by itself" $([ -e $T/bt/bonded_$HP ] && [ -e $T/bt/trusted_$HP ] && echo 1 || echo 0) "$(ls $T/bt)"
# a keyboard that needs a code typed on it
rm -f $T/bt/connected_*; echo "pairkb $KB" > $T/run/bt-agent.in
waitf $T/run/bt-ask; a=$(cat $T/run/bt-ask 2>/dev/null)
ok "12. a keyboard's code to type is shown" $(has "$a" '"kind":"keyboard","code":"019283","mac":"'$KB'"') "$a"
sleep 4
ok "13. ...and goes away when the pairing finishes" $([ ! -e $T/run/bt-ask ] && echo 1 || echo 0) "$(cat $T/run/bt-ask 2>/dev/null)"
# services: yes only within two minutes of a pairing someone asked for
echo authsvc > $T/run/bt-agent.in; sleep 2; y=$(tail -1 $T/bt/answers)
rm -f $T/run/bt-pairing; echo authsvc > $T/run/bt-agent.in; sleep 2; n=$(tail -1 $T/bt/answers)
ok "14. a service is allowed right after a pairing, refused otherwise" $([ "$y" = yes ] && [ "$n" = no ] && echo 1 || echo 0) "$y $n"
# bluetoothd restarts: the agent ends (the loop in start starts it again)
kill $BDP; (cd $T/bd && exec ./bluetoothd 600) & BDP=$!
for i in $(seq 20); do kill -0 $AP 2>/dev/null || break; sleep 0.5; done
ok "15. when bluetoothd restarts the agent ends, and cleans up" $(! kill -0 $AP 2>/dev/null && [ ! -e $T/run/btagent.pid ] && [ ! -e $T/run/bt-agent.in ] && echo 1 || echo 0)
echo "$((N-F))/$N passed"; [ $F = 0 ]
