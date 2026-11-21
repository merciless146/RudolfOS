#!/bin/sh
# 1.6.1 (plan Part 1): several monitors, rudolf-display's part. The plan (--monitors-plan: the built-in panel first, a saved arrangement
# used only when it names every monitor with a workspace each); on Xorg with the dummy driver (xserver-xorg-video-dummy, whose outputs
# RandR lights as real ones; Xorg needs root or sudo without a password): --arrange lights every connected monitor side by side (the
# main one at 0,0) and writes the list; a second --arrange changes nothing; --input arranges again when a monitor is plugged in and when
# the saved arrangement changes; screenctl arrange takes only an arrangement of the monitors plugged in (a copy with the test's paths).
# Stops only what it started.   sh monitors161_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; T=$(mktemp -d /tmp/monitors161.XXXXXX); DSP=${MON_DSP:-:86}; F=0
ok() { if [ "$1" = 0 ]; then echo "PASS $2"; else echo "FAIL $2${3:+ - $3}"; F=$((F+1)); fi; }
gcc -O2 -o $T/rd $SYS/rudolf-display.c -lX11 -lXtst -lXfixes -lXrandr -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
# --- the plan
P() { printf "$1" | RUDOLF_MON_CONF=$T/conf $T/rd --monitors-plan; }
: > $T/conf
o=$(P 'HDMI-1 2560 1440\neDP-1 1920 1080\n'); echo "$o" | grep -q '"name":"eDP-1","x":0,"y":0,"w":1920,"h":1080,"ws":1,"main":true},{"name":"HDMI-1","x":1920,"y":0,"w":2560,"h":1440,"ws":2' ; ok $? "the built-in panel first, the other to its right" "$o"
echo "$o" | grep -q '"key":"HDMI-1+eDP-1"'; ok $? "the set's key: the names sorted" "$o"
printf 'HDMI-1+eDP-1\tHDMI-1=3,eDP-1=1\n' > $T/conf
o=$(P 'HDMI-1 2560 1440\neDP-1 1920 1080\n'); echo "$o" | grep -q '"name":"HDMI-1","x":0,"y":0,"w":2560,"h":1440,"ws":3,"main":true},{"name":"eDP-1","x":2560,"y":0,"w":1920,"h":1080,"ws":1'; ok $? "the saved arrangement: HDMI-1 the main one, showing 3" "$o"
printf 'HDMI-1+eDP-1\tHDMI-1=1,eDP-1=1\n' > $T/conf
o=$(P 'HDMI-1 2560 1440\neDP-1 1920 1080\n'); echo "$o" | grep -q '"name":"eDP-1","x":0,.*"ws":1,"main":true},{"name":"HDMI-1".*"ws":2'; ok $? "one workspace on two monitors: not used" "$o"
printf 'HDMI-1+eDP-1\tHDMI-1=2\n' > $T/conf
o=$(P 'HDMI-1 2560 1440\neDP-1 1920 1080\n'); echo "$o" | grep -q '"name":"eDP-1","x":0'; ok $? "an arrangement of other monitors: not used" "$o"
o=$(P 'DP-1 1920 1080\nDP-2 1920 1080\nDP-3 1280 1024\nDP-4 800 600\n'); [ "$(echo "$o" | grep -o '"name"' | wc -l)" = 3 ]; ok $? "at most three" "$o"
# --- screenctl arrange (its paths the test's)
mkdir -p $T/run; sed -e "s#RUN=/run/rudolf#RUN=$T/run#" -e "s#CONF=/etc/rudolf-monitors#CONF=$T/sconf#" $SYS/root/usr/lib/rudolf/screenctl > $T/screenctl
printf '{"key":"DUMMY0+DUMMY1","monitors":[]}\n' > $T/run/monitors
sh $T/screenctl arrange 'DUMMY1=2,DUMMY0=1' 2>/dev/null; ok $? "screenctl arrange: the monitors plugged in"
grep -q "^DUMMY0+DUMMY1	DUMMY1=2,DUMMY0=1$" $T/sconf; ok $? "kept for the set" "$(cat $T/sconf)"
for bad in 'DUMMY1=2,DUMMY0=2' 'DUMMY1=2' 'DUMMY1=2,DUMMY1=1' 'DUMMY1=4,DUMMY0=1' "DUMMY1=2,DUMMY0=1;reboot" 'a b=1'; do
	sh $T/screenctl arrange "$bad" 2>/dev/null; [ $? != 0 ]; ok $? "screenctl arrange refuses '$bad'"; done
sh $T/screenctl arrange 'DUMMY0=3,DUMMY1=1' 2>/dev/null; [ "$(grep -c '^DUMMY0+DUMMY1	' $T/sconf)" = 1 ] && grep -q 'DUMMY0=3,DUMMY1=1' $T/sconf; ok $? "a new arrangement replaces the set's line" "$(cat $T/sconf)"
# --- on Xorg (the dummy driver)
SUDO=; [ "$(id -u)" = 0 ] || { sudo -n true 2>/dev/null && SUDO="sudo -n"; }
if [ ! -f /usr/lib/xorg/modules/drivers/dummy_drv.so ] || ! command -v Xorg >/dev/null || ! command -v xrandr >/dev/null || { [ "$(id -u)" != 0 ] && [ -z "$SUDO" ]; }; then
	echo "SKIP the Xorg part: needs Xorg's dummy driver, xrandr and root"; rm -rf $T; [ $F = 0 ] && echo "PASS all" || echo "FAIL $F"; exit $([ $F = 0 ] && echo 0 || echo 1); fi
cat > $T/xorg.conf <<X
Section "Device"
  Identifier "d"
  Driver "dummy"
  VideoRam 256000
EndSection
Section "Monitor"
  Identifier "m"
  HorizSync 5.0-1000.0
  VertRefresh 5.0-200.0
EndSection
Section "Screen"
  Identifier "s"
  Device "d"
  Monitor "m"
  DefaultDepth 24
  SubSection "Display"
    Depth 24
    Virtual 8192 4096
  EndSubSection
EndSection
X
$SUDO Xorg $DSP -config $T/xorg.conf -noreset -nolisten tcp -logfile $T/xorg.log >/dev/null 2>&1 &
IP=
trap '[ -n "$IP" ] && kill $IP 2>/dev/null; $SUDO pkill -f "Xorg $DSP -config $T/" 2>/dev/null; rm -rf $T' EXIT
export DISPLAY=$DSP; i=0; while [ $i -lt 60 ] && ! xrandr >/dev/null 2>&1; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
# (a monitor "plugged in": the dummy's output given a mode and lit once, then a probe; it stays connected)
plug() { xrandr --addmode $1 1280x720 2>/dev/null; xrandr --fb 8000x2000 --output $1 --mode 1280x720 --pos 4000x0 && xrandr --output $1 --off && xrandr >/dev/null; }
export RUDOLF_MON_CONF=$T/mconf RUDOLF_MON_RUN=$T/mons
plug DUMMY1
$T/rd --arrange 2>$T/a1.log; ok $? "--arrange"
xrandr --listmonitors > $T/lm; grep -q "DUMMY0 .*+0+0" $T/lm && grep -q "DUMMY1 .*+2048+0" $T/lm; ok $? "side by side: DUMMY0 at 0,0, DUMMY1 to its right" "$(cat $T/lm)"
grep -q '"key":"DUMMY0+DUMMY1"' $T/mons && grep -q '"name":"DUMMY1","x":2048,"y":0,"w":2048,"h":1536,"ws":2,"main":false' $T/mons; ok $? "the list written (/run/rudolf/monitors)" "$(cat $T/mons)"
[ "$(stat -c %a $T/mons)" = 644 ]; ok $? "the list readable by the desktop's user"
xrandr | grep -q "current 4096 x 1536"; ok $? "the screen as large as both" "$(xrandr | head -1)"
$T/rd --arrange 2>$T/a2.log; ! grep -q arranged $T/a2.log; ok $? "a second --arrange changes nothing" "$(cat $T/a2.log)"
$T/rd --monitors | grep -q '"name":"DUMMY0".*"name":"DUMMY1"'; ok $? "--monitors lists both"
# --input: a third monitor plugged in, then another arrangement saved
$T/rd --input >$T/in.log 2>&1 & IP=$!
timeout 2 tail -f /dev/null; plug DUMMY2
i=0; while [ $i -lt 20 ] && ! grep -q DUMMY2 $T/mons; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
grep -q '"name":"DUMMY2","x":4096,"y":0,"w":2048,"h":1536,"ws":3' $T/mons; ok $? "--input: a monitor plugged in is lit, to the right, showing 3" "$(cat $T/mons)"
printf 'DUMMY0+DUMMY1+DUMMY2\tDUMMY2=1,DUMMY0=2,DUMMY1=3\n' > $T/mconf
i=0; while [ $i -lt 20 ] && ! grep -q '"name":"DUMMY2","x":0' $T/mons; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
grep -q '{"name":"DUMMY2","x":0,"y":0,"w":2048,"h":1536,"ws":1,"main":true},{"name":"DUMMY0","x":2048,"y":0,"w":2048,"h":1536,"ws":2' $T/mons; ok $? "--input: the saved arrangement taken (DUMMY2 the main one)" "$(cat $T/mons)"
xrandr --listmonitors | grep -q "DUMMY2 .*+0+0"; ok $? "and the monitors moved so" "$(xrandr --listmonitors)"
kill $IP 2>/dev/null; IP=
[ $F = 0 ] && echo "PASS all" || echo "FAIL $F"; [ $F = 0 ]
