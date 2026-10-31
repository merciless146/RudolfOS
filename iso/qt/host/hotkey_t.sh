#!/bin/bash
# hotkey (1.4.4) with a stand-in pactl and a stand-in backlight, under BusyBox's sh (as on Rudolf OS).
cd "$(dirname "$0")/../../.."; H=$PWD/iso/sys/root/usr/lib/rudolf/hotkey; T=$(mktemp -d); R=0; n=0
ok(){ n=$((n+1)); if [ "$2" = "$3" ]; then echo "PASS $1"; R=$((R+1)); else echo "FAIL $1 - got [$2] want [$3]"; fi; }
SH="busybox sh"; command -v busybox >/dev/null || SH=sh
cat > $T/pactl <<'P'
#!/bin/sh
S=$(dirname "$0")/pa; [ -f $S ] || echo "50 0 0" > $S; read v m mic < $S
case "$1" in
get-sink-volume) echo "Volume: front-left: 1 / $v% / 0.00 dB,   front-right: 1 / $v% / 0.00 dB";;
get-sink-mute) [ $m = 1 ] && echo "Mute: yes" || echo "Mute: no";;
set-sink-volume) v=${3%\%};;
set-sink-mute) case $3 in toggle) m=$((1-m));; *) m=$3;; esac;;
set-source-mute) mic=$((1-mic));;
get-source-mute) [ $mic = 1 ] && echo "Mute: yes" || echo "Mute: no";;
esac; echo "$v $m $mic" > $S
P
chmod +x $T/pactl; mkdir -p $T/bl/amdgpu_bl1; echo 65535 > $T/bl/amdgpu_bl1/max_brightness; echo 19005 > $T/bl/amdgpu_bl1/brightness
export HOTKEY_PACTL=$T/pactl HOTKEY_EVENTS=$T/events HOTKEY_BACKLIGHT=$T/bl
hk(){ $SH $H "$@"; }
ok "volume get" "$(hk volume get)" "50"
ok "volume up: 55" "$(hk volume up)" "55"
echo "53 0 0" > $T/pa; ok "volume up from 53: 55 (to the next step)" "$(hk volume up)" "55"
echo "53 0 0" > $T/pa; ok "volume down from 53: 50" "$(hk volume down)" "50"
echo "98 0 0" > $T/pa; ok "volume up from 98: 100, never above" "$(hk volume up)" "100"; ok "...and again: 100" "$(hk volume up)" "100"
echo "3 0 0" > $T/pa; ok "volume down from 3: 0" "$(hk volume down)" "0"; ok "...and again: 0" "$(hk volume down)" "0"
echo "40 0 0" > $T/pa; ok "mute" "$(hk volume mute)" "40 muted"; ok "...unmute" "$(hk volume mute)" "40"
echo "40 1 0" > $T/pa; hk volume up >/dev/null; ok "volume up unmutes" "$(hk volume get)" "45"
ok "volume set 70" "$(hk volume set 70)" "70"; ok "volume set 140: 100" "$(hk volume set 140)" "100"
ok "volume set x: refused" "$(hk volume set x; echo $?)" "$(printf 'a number 0-100\n2')"
ok "mic mute" "$(hk mic mute)" "muted"; ok "...and on" "$(hk mic mute)" "on"
ok "brightness get: 29 %" "$(hk brightness get)" "29"
ok "brightness up: 34 %" "$(hk brightness up)" "34"; ok "...the backlight value" "$(cat $T/bl/amdgpu_bl1/brightness)" "$((19005+3276))"
echo 4000 > $T/bl/amdgpu_bl1/brightness; ok "brightness down from 6 %: 5 %, never below" "$(hk brightness down)" "5"; ok "...and again: 5 %" "$(hk brightness down)" "5"
echo 64000 > $T/bl/amdgpu_bl1/brightness; ok "brightness up near the top: 100 %" "$(hk brightness up)" "100"
ok "the events for the indicator" "$(grep -c '^osd ' $T/events)" "$(wc -l < $T/events)"
ok "...the last ones" "$(tail -2 $T/events | tr '\n' '|')" "osd brightness 5|osd brightness 100|"
rm -rf $T/bl/amdgpu_bl1; ok "no backlight: none" "$(hk brightness up)" "none"; ok "...and the event says so" "$(tail -1 $T/events)" "osd brightness none"
rm -rf "$T"; echo "$R/$n passed"; [ $R = $n ]
