#!/bin/sh
# 1.5.9 (plan 7.2): the echo canceller (sound echo on|off|status, set_default moving it) and a Bluetooth headset's
# microphone (bluetooth audio: the A2DP output, the SCO microphone, call mode while an app records from it). Stand-ins
# for su, pactl (with modules, sinks, sources, streams and defaults kept in files) and bluetoothctl.
# sh sound59_t.sh   (a few seconds)
HERE=$(cd "$(dirname "$0")" && pwd); SND=$HERE/../../sys/root/usr/lib/rudolf/sound; BT=$HERE/../../sys/root/usr/lib/rudolf/bluetooth
T=$(mktemp -d); trap 'kill $(cat $T/run/bt-callmode.pid 2>/dev/null) 2>/dev/null; rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
B=$T/bin; mkdir -p $B $T/run $T/persist $T/xdg/pulse; export ST=$T/state; mkdir -p $ST
cat > $B/su <<'X'
#!/bin/sh
while [ $# -gt 1 ]; do shift; done; exec sh -c "$1"
X
# pactl: modules in $ST/mods (id TAB name TAB args); sinks and sources come from them; streams in $ST/sins, $ST/souts
# (id TAB device TAB client TAB driver); the defaults in $ST/dsink, $ST/dsrc
cat > $B/pactl <<'X'
#!/bin/sh
echo "pactl $*" >> $ST/log
arg() { echo "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
devs() { # sinks|sources
	while IFS='	' read -r id name args; do
		case "$name" in
		module-alsa-card) c=$(arg "$args" name); [ $1 = sinks ] && printf '%s\talsa_output.%s.analog-stereo\n' $id $c || { printf '%s\talsa_input.%s.analog-stereo\n' $id $c; printf '%s\talsa_output.%s.analog-stereo.monitor\n' $((id+500)) $c; };;
		module-alsa-sink) [ $1 = sinks ] && printf '%s\t%s\n' $id "$(arg "$args" sink_name)";;
		module-alsa-source) [ $1 = sources ] && printf '%s\t%s\n' $id "$(arg "$args" source_name)";;
		module-echo-cancel) [ $1 = sinks ] && printf '%s\techocancel_sink\n' $id || printf '%s\techocancel_source\n' $id;;
		esac
	done < $ST/mods
}
case "$1 $2 $3" in
"list short modules") cat $ST/mods;;
"list short sinks") devs sinks;;
"list short sources") devs sources;;
"list short sink-inputs") cat $ST/sins 2>/dev/null;;
"list short source-outputs") cat $ST/souts 2>/dev/null;;
"info  "*|"info "*) echo "Server Name: pulseaudio"; echo "Default Sink: $(cat $ST/dsink 2>/dev/null)"; echo "Default Source: $(cat $ST/dsrc 2>/dev/null)";;
"load-module "*) [ -e $ST/fail-$2 ] && exit 1; i=$(cat $ST/next 2>/dev/null || echo 1); echo $((i+1)) > $ST/next; n=$2; shift 2; printf '%s\t%s\t%s\n' $i $n "$*" >> $ST/mods
	[ $n = module-echo-cancel ] && printf '%s\t%s\t-\tmodule-echo-cancel.c\n' 9$i "$(arg "$*" sink_master)" >> $ST/sins; echo $i;;
"unload-module "*) awk -F'\t' -v m="$2" '$1!=m' $ST/mods > $ST/m2; mv $ST/m2 $ST/mods; awk -F'\t' -v m="9$2" '$1!=m' $ST/sins > $ST/s2 2>/dev/null; mv $ST/s2 $ST/sins;;
"set-default-sink "*) echo "$2" > $ST/dsink;;
"set-default-source "*) echo "$2" > $ST/dsrc;;
"move-sink-input "*) awk -F'\t' -v OFS='\t' -v i="$2" -v d="$3" '$1==i {$2=d} 1' $ST/sins > $ST/s2; mv $ST/s2 $ST/sins;;
"move-source-output "*) awk -F'\t' -v OFS='\t' -v i="$2" -v d="$3" '$1==i {$2=d} 1' $ST/souts > $ST/s2; mv $ST/s2 $ST/souts;;
esac
exit 0
X
chmod +x $B/*
reset() { : > $ST/mods; : > $ST/log; : > $ST/sins; : > $ST/souts; echo 1 > $ST/next; rm -f $ST/dsink $ST/dsrc $T/run/echo-masters; }
export RUDOLF_TEST_PATH=$B RUDOLF_SOUND_ECHO=$T/persist/sound-echo RUDOLF_SOUND_RUN=$T/run
# the sound server's socket: a stand-in
python3 -c 'import socket,sys; socket.socket(socket.AF_UNIX).bind(sys.argv[1])' $T/pulse.sock; export RUDOLF_PULSE_SOCK=$T/pulse.sock
SOCKOK=1
# ---- the echo canceller
reset; printf '1\tmodule-alsa-card\tdevice_id=0 name=card0 tsched=1\n2\tmodule-alsa-card\tdevice_id=1 name=card1 tsched=1\n' > $ST/mods; echo 3 > $ST/next
echo alsa_output.card0.analog-stereo > $ST/dsink; echo alsa_input.card0.analog-stereo > $ST/dsrc
printf '40\talsa_output.card0.analog-stereo\t7\tprotocol-native.c\n' > $ST/sins; printf '41\talsa_input.card0.analog-stereo\t7\tprotocol-native.c\n' > $ST/souts
s=$(sh $SND echo status); ok "1. off by default" "$(has "$s" '{"on":false,"active":false}')" "$s"
if [ $SOCKOK = 1 ]; then
	o=$(sh $SND echo on); L=$(cat $ST/log); M=$(cat $ST/mods)
	ok "2. on: Adrian's canceller on the default output and microphone" "$(echo "$M" | grep module-echo-cancel | grep -q 'aec_method=adrian use_master_format=1 sink_master=alsa_output.card0.analog-stereo source_master=alsa_input.card0.analog-stereo' && echo 1)" "$M"
	ok "3. ...its names quoted for PulseAudio (a name with spaces)" "$(has "$L" "sink_properties='device.description=\"Speakers (echo cancelled)\"'")" "$L"
	ok "4. ...it becomes the default, and the app's sound and microphone move to it" "$([ "$(cat $ST/dsink)" = echocancel_sink ] && [ "$(cat $ST/dsrc)" = echocancel_source ] && grep -q '^40	echocancel_sink' $ST/sins && grep -q '^41	echocancel_source' $ST/souts && echo 1)" "$(cat $ST/sins $ST/souts)"
	ok "5. ...but not its own stream (no loop)" "$(grep 'module-echo-cancel.c' $ST/sins | grep -q 'alsa_output.card0' && echo 1)" "$(cat $ST/sins)"
	s=$(sh $SND echo status); ok "6. status: on and active" "$(has "$s" '{"on":true,"active":true}')" "$s"
	# the sound card changes: the canceller moves onto it
	sh $SND default 1 >/dev/null
	M=$(cat $ST/mods); ok "7. a new sound card: the canceller moves to its output and microphone" "$([ "$(grep -c module-echo-cancel $ST/mods)" = 1 ] && echo "$M" | grep module-echo-cancel | grep -q 'sink_master=alsa_output.card1.analog-stereo source_master=alsa_input.card1.analog-stereo' && [ "$(cat $ST/dsink)" = echocancel_sink ] && echo 1)" "$M"
	sh $SND echo off >/dev/null; M=$(cat $ST/mods)
	ok "8. off: removed, the card's own output and microphone are the defaults again, the apps on them" "$([ "$(grep -c module-echo-cancel $ST/mods)" = 0 ] && [ "$(cat $ST/dsink)" = alsa_output.card1.analog-stereo ] && [ "$(cat $ST/dsrc)" = alsa_input.card1.analog-stereo ] && [ ! -e $T/persist/sound-echo ] && echo 1)" "$M $(cat $ST/dsink $ST/dsrc)"
	# a Bluetooth headset's microphone: not used (the headset cancels echo, and would stay in call mode)
	sh $SND echo on >/dev/null; sh $SND echo off >/dev/null; : > $T/persist/sound-echo
	echo bt_in_AA_BB_CC_DD_EE_10 > $ST/dsrc; o=$(sh $SND echo on); M=$(cat $ST/mods)
	ok "9. not on a Bluetooth headset's microphone" "$([ "$(grep -c module-echo-cancel $ST/mods)" = 0 ] && echo 1)" "$o $M"
	rm -f $T/persist/sound-echo
	# it fails to load: said, and nothing changed
	echo alsa_input.card1.analog-stereo > $ST/dsrc; touch $ST/fail-module-echo-cancel; o=$(sh $SND echo on); r=$?
	ok "10. could not be started: says so, the defaults are left alone" "$([ $r != 0 ] && [ "$(has "$o" "could not be started")" = 1 ] && [ "$(cat $ST/dsink)" = alsa_output.card1.analog-stereo ] && echo 1)" "$r $o"
	rm -f $ST/fail-module-echo-cancel $T/persist/sound-echo
else echo "SKIP 2-10: /run/user/1000/pulse is not writable here (run as root)"; fi
# ---- a Bluetooth headset's microphone
reset; printf '1\tmodule-alsa-card\tdevice_id=0 name=card0 tsched=1\n' > $ST/mods; echo 2 > $ST/next
HS=AA:BB:CC:DD:EE:10; PH=DC:F0:90:56:8E:AE; BUDS=AA:BB:CC:DD:EE:20
cat > $B/bluetoothctl <<X
#!/bin/sh
case "\$1" in
devices) for m in \$(cat $ST/btcon); do echo "Device \$m x"; done;;
info) case "\$2" in
	$HS) printf 'Device $HS\n\tAlias: Galaxy Buds Pro\n\tUUID: Audio Sink                (0000110b-0000-1000-8000-00805f9b34fb)\n\tUUID: Handsfree                 (0000111e-0000-1000-8000-00805f9b34fb)\n';;
	$BUDS) printf 'Device $BUDS\n\tAlias: Speaker\n\tUUID: Audio Sink                (0000110b-0000-1000-8000-00805f9b34fb)\n';;
	$PH) printf 'Device $PH\n\tAlias: Phone\n\tUUID: Handsfree Audio Gateway   (0000111f-0000-1000-8000-00805f9b34fb)\n\tUUID: Headset AG                (00001112-0000-1000-8000-00805f9b34fb)\n';;
	esac;;
esac
X
chmod +x $B/bluetoothctl
export RUDOLF_BLUETOOTHCTL=$B/bluetoothctl RUDOLF_RUN=$T/run RUDOLF_BT_CALLPOLL=0.3
echo "$HS $PH $BUDS" > $ST/btcon
o=$(sh $BT audio); M=$(cat $ST/mods); L=$(cat $ST/log)
ok "11. a headset: its A2DP output, named with its spaces quoted" "$(echo "$M" | grep -q "module-alsa-sink	device=bluealsa:DEV=$HS,PROFILE=a2dp sink_name=bt_AA_BB_CC_DD_EE_10 tsched=0 sink_properties='device.description=\"Galaxy Buds Pro\"'" && echo 1)" "$M"
ok "12. ...and its microphone (SCO), the default microphone" "$(echo "$M" | grep -q "module-alsa-source	device=bluealsa:DEV=$HS,PROFILE=sco source_name=bt_in_AA_BB_CC_DD_EE_10 " && [ "$(cat $ST/dsrc)" = bt_in_AA_BB_CC_DD_EE_10 ] && echo 1)" "$M"
ok "13. a speaker (no Hands-Free): output only; a phone (the gateway side): nothing" "$([ "$(grep -c 'bt_in_' $ST/mods)" = 1 ] && grep -q 'sink_name=bt_AA_BB_CC_DD_EE_20 ' $ST/mods && ! grep -q 'DC_F0' $ST/mods && echo 1)" "$M"
ok "14. no call output until a call" "$(grep -q bt_call_ $ST/mods && echo 0 || echo 1)"
sleep 0.5; P=$(cat $T/run/bt-callmode.pid 2>/dev/null)
ok "15. call mode is watched" "$([ -n "$P" ] && kill -0 $P 2>/dev/null && echo 1)" "$P"
o=$(sh $BT audio); ok "16. audio again: nothing loaded twice, one watcher" "$([ "$(grep -c 'bt_' $ST/mods)" = 3 ] && [ "$(cat $T/run/bt-callmode.pid)" = "$P" ] && echo 1)" "$(cat $ST/mods)"
# a call: an app records from the headset's microphone
IN=$(awk -F'\t' '$3 ~ /source_name=bt_in_/ {print $1}' $ST/mods)
printf '60\tbt_AA_BB_CC_DD_EE_10\t8\tprotocol-native.c\n' > $ST/sins; printf '61\t%s\t8\tprotocol-native.c\n' $IN > $ST/souts
for i in 1 2 3 4 5 6 7 8 9 10; do grep -q bt_call_ $ST/mods && break; sleep 0.2; done; sleep 0.2
ok "17. a call: the call output is made and what plays moves to it" "$(grep -q "device=bluealsa:DEV=$HS,PROFILE=sco sink_name=bt_call_AA_BB_CC_DD_EE_10 " $ST/mods && [ "$(cat $ST/dsink)" = bt_call_AA_BB_CC_DD_EE_10 ] && grep -q '^60	bt_call_' $ST/sins && echo 1)" "$(cat $ST/mods $ST/sins $ST/dsink)"
: > $ST/souts; for i in 1 2 3 4 5 6 7 8 9 10; do grep -q bt_call_ $ST/mods || break; sleep 0.2; done; sleep 0.2
ok "18. the call ends: back to A2DP, the call output removed" "$(grep -q bt_call_ $ST/mods && echo 0 || { [ "$(cat $ST/dsink)" = bt_AA_BB_CC_DD_EE_10 ] && grep -q '^60	bt_AA_' $ST/sins && echo 1; })" "$(cat $ST/mods $ST/sins $ST/dsink)"
echo "$BUDS" > $ST/btcon; sh $BT audio >/dev/null; sleep 0.8
ok "19. the headset disconnects: its output and microphone go, the speaker stays, the watcher stops" "$(grep -q 'AA_BB_CC_DD_EE_10' $ST/mods && echo 0 || { grep -q 'bt_AA_BB_CC_DD_EE_20 ' $ST/mods && [ ! -e $T/run/bt-callmode.pid ] && echo 1; })" "$(cat $ST/mods)"
grep -q 'hfp-ag' $BT && grep -q -- "--help 2>&1 | grep -q 'hfp-ag'" $BT; ok "20. bluealsa gets the headset profiles only when it has them" "$([ $? = 0 ] && echo 1)"
echo "$N tests, $F failed"; [ $F = 0 ]
