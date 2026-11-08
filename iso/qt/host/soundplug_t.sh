#!/bin/sh
# 1.5.4: /usr/lib/rudolf/sound with cards that come and go (mdev's "sound hotplug"): Intel's sound processor, whose
# card appears only once its firmware runs, and a USB headset. Plain HD Audio keeps PulseAudio's own profiles
# (use_ucm=0), the others use ALSA's use case files; a card already open is not opened twice; a card that goes away
# is closed. Stand-ins for su, pactl and pulseaudio, and for /proc/asound. Needs root (a private /run).
[ "$(id -u)" = 0 ] && [ -z "$SPT_INNER" ] && exec env SPT_INNER=1 unshare -m sh -c 'mount -t tmpfs none /run && mkdir -p /run/rudolf && exec sh "$0"' "$0"
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
HERE=$(cd "$(dirname "$0")" && pwd); S=$HERE/../../sys/root/usr/lib/rudolf/sound
T=$(mktemp -d); trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
B=$T/bin; mkdir -p $B; export ST=$T/state; mkdir -p $ST; : > $ST/mods; : > $ST/log; echo 0 > $ST/next
cat > $B/su <<'X'
#!/bin/sh
while [ $# -gt 1 ]; do shift; done; exec sh -c "$1"
X
cat > $B/pulseaudio <<'X'
#!/bin/sh
mkdir -p "$XDG_RUNTIME_DIR/pulse"; python3 -c 'import socket,sys; s=socket.socket(socket.AF_UNIX); s.bind(sys.argv[1])' "$XDG_RUNTIME_DIR/pulse/native"
# the start-up script's cards, as module-alsa-card would load them
sed -n 's/^load-module module-alsa-card //p' "$(echo "$@" | sed 's/.*-F \([^ ]*\).*/\1/')" | while read -r a; do pactl load-module module-alsa-card $a; done >/dev/null
X
cat > $B/pactl <<'X'
#!/bin/sh
echo "pactl $*" >> $ST/log
case "$1 $2 $3" in
"list short modules") cat $ST/mods;;
"list short sinks") awk -F'\t' '{split($3,a," "); for(i in a) if (a[i] ~ /^name=/) {sub(/name=/,"",a[i]); print $1 "\talsa_output." a[i] ".analog-stereo"}}' $ST/mods;;
"list short sources") awk -F'\t' '{split($3,a," "); for(i in a) if (a[i] ~ /^name=/) {sub(/name=/,"",a[i]); print $1 "\talsa_input." a[i] ".analog-stereo"; print $1 "\talsa_output." a[i] ".analog-stereo.monitor"}}' $ST/mods;;
"load-module module-alsa-card"*) shift 2; i=$(cat $ST/next); echo $((i+1)) > $ST/next; printf '%s\tmodule-alsa-card\t%s\n' "$i" "$*" >> $ST/mods; echo $i;;
"unload-module "*) awk -F'\t' -v m="$2" '$1!=m' $ST/mods > $ST/m2; mv $ST/m2 $ST/mods;;
esac
exit 0
X
chmod +x $B/*
A=$T/asound; mkdir -p $A/card0 $A/card1 $A/card2; touch $A/card0/pcm0p $A/card1/pcm0p
cat > $A/cards <<'X'
 0 [Generic        ]: HDA-Intel - HD-Audio Generic
                      HD-Audio Generic at 0xfe600000 irq 75
 1 [sofhdadsp      ]: sof-hda-dsp - sof-hda-dsp
                      LENOVO-21AH
X
export RUDOLF_TEST_PATH=$B RUDOLF_TEST_ASOUND=$A RUDOLF_SOUND_WAIT=10 RUDOLF_SOUND_SETTLE=0
sh $S hotplug </dev/null; ok "no sound server: nothing to do" "$([ ! -s $ST/log ] && echo 1)" "$(cat $ST/log)"
MDEV=snd/controlC1 ACTION=add sh $S hotplug; sleep 0.5
ok "no sound server: hotplug does nothing" "$([ ! -s $ST/mods ] && echo 1)" "$(cat $ST/mods)"
sh $S start >/dev/null 2>&1
M=$(cat $ST/mods); L=$(cat $ST/log)
ok "start: the HD Audio card keeps PulseAudio's profiles" "$(has "$M" "device_id=0 name=card0 tsched=1 deferred_volume=0 use_ucm=0")" "$M"
ok "start: the sound processor uses the use case files" "$(echo "$M" | grep 'device_id=1 ' | grep -vq use_ucm && echo 1)" "$M"
ok "start: each card once" "$([ "$(wc -l < $ST/mods)" = 2 ] && echo 1)" "$M"
ok "start: outputs opened" "$(has "$L" "set-sink-mute alsa_output.card1.analog-stereo 0")" "$L"
ok "start: inputs at 80%, monitors left alone" "$([ "$(has "$L" "set-source-volume alsa_input.card0.analog-stereo 80%")" = 1 ] && [ "$(has "$L" "monitor 0")" = 0 ] && echo 1)" "$L"
# a USB headset: the control device first, its sound devices a moment later
printf ' 2 [Headset        ]: USB-Audio - USB Headset\n                      Logitech USB Headset at usb-0000:00:14.0-2\n' >> $A/cards
: > $ST/log; MDEV=snd/controlC2 ACTION=add sh $S hotplug; R=$?; sleep 0.3; touch $A/card2/pcm0p; sleep 1.5
M=$(cat $ST/mods); L=$(cat $ST/log)
ok "hotplug returns at once (mdev waits for it)" "$([ $R = 0 ] && echo 1)"
ok "hotplug: the headset is opened, with the use case files" "$(echo "$M" | grep 'device_id=2 name=card2 ' | grep -vq use_ucm && echo 1)" "$M"
ok "hotplug: only the new card's outputs are touched" "$([ "$(has "$L" "set-sink-mute alsa_output.card2.analog-stereo 0")" = 1 ] && [ "$(has "$L" "card0")" = 0 ] && echo 1)" "$L"
: > $ST/log; MDEV=snd/controlC1 ACTION=add sh $S hotplug; sleep 1.2
ok "hotplug: a card already open is not opened again" "$([ "$(grep -c 'device_id=1 ' $ST/mods)" = 1 ] && echo 1)" "$(cat $ST/mods)"
MDEV=snd/controlC2 ACTION=remove sh $S hotplug
ok "unplugged: the headset is closed" "$([ "$(grep -c 'device_id=2 ' $ST/mods)" = 0 ] && [ "$(wc -l < $ST/mods)" = 2 ] && echo 1)" "$(cat $ST/mods)"
MDEV=snd/pcmC0D0p ACTION=add sh $S hotplug; MDEV=snd/controlC12 ACTION=remove sh $S hotplug
ok "other devices and unknown cards: nothing changes" "$([ "$(wc -l < $ST/mods)" = 2 ] && echo 1)" "$(cat $ST/mods)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
