#!/bin/sh
# 1.6.1 (plan Part 1): several monitors on both engines, compared (monbeh_t.js). The X server: Xorg with the dummy driver
# (xserver-xorg-video-dummy), whose outputs RandR lights like real ones: DUMMY0 1920x1080 at 0,0 (the main one) and DUMMY1 1280x800 to its
# right; rudolf-display --monitors (built here) gives the list both engines read (Standard through the stand-in rudolfd's screen
# request, Native as /run/rudolf/monitors: RUDOLF_MON_RUN). Xorg needs root (or sudo without a password). Needs too what same_t.sh needs.
#   sh monbeh_t.sh [OUT] [RUDOLF-NATIVE]      (MON_SCALE=125: at that display scaling; MON_DM=dark: in dark mode, else light)
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/monbeh_t.XXXXXX); PORT=${SAME_PORT:-8612}; DSP=${SAME_DSP:-:84}
OUT=${1:-$T/out}; BIN=$2
command -v xdotool >/dev/null && command -v import >/dev/null && command -v xrandr >/dev/null || { echo "SKIP: needs xdotool, ImageMagick and xrandr"; exit 0; }
[ -f /usr/lib/xorg/modules/drivers/dummy_drv.so ] && command -v Xorg >/dev/null || { echo "SKIP: needs Xorg's dummy driver (xserver-xorg-video-dummy)"; exit 0; }
SUDO=; [ "$(id -u)" = 0 ] || { sudo -n true 2>/dev/null && SUDO="sudo -n" || { echo "SKIP: Xorg needs root"; exit 0; }; }
python3 -c 'import PIL' 2>/dev/null || { echo "SKIP: needs python3's PIL"; exit 0; }
if [ -z "$BIN" ]; then BIN=$T/rudolf-native
	gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $BIN $HERE/../../sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
fi
gcc -O2 -o $T/rudolf-display $HERE/../../sys/rudolf-display.c -lX11 -lXtst -lXfixes -lXrandr -lm 2>$T/cc2 || { cat $T/cc2; echo "FAIL build rudolf-display"; exit 1; }
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
D=$T/d; mkdir -p $D; node $HERE/../host/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
python3 $HERE/same_rd.py $PORT $D > $T/rd.log 2>&1 & RDP=$!
$SUDO Xorg $DSP -config $T/xorg.conf -noreset -nolisten tcp -logfile $T/xorg.log >/dev/null 2>&1 &
trap 'kill $RDP 2>/dev/null; $SUDO pkill -f "Xorg $DSP -config $T/" 2>/dev/null; pkill -f "$T/" 2>/dev/null; rm -rf $T/d $T/rudolf-native $T/rudolf-display $T/cc $T/cc2' EXIT
export DISPLAY=$DSP
i=0; while [ $i -lt 60 ] && ! xrandr >/dev/null 2>&1; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
# (Xorg's dummy driver gives every account the display: the test's own user too, when started with sudo)
xrandr --output DUMMY0 --mode 1920x1080 --pos 0x0 --primary && xrandr --addmode DUMMY1 1280x800 && xrandr --fb 3200x1080 --output DUMMY1 --mode 1280x800 --pos 1920x0 || { echo "FAIL xrandr"; exit 1; }
# (a probe of the outputs, as Xorg makes on a real monitor's hot plug: the dummy's second output is "connected" only from then on, for GDK)
xrandr >/dev/null
RUDOLF_MON_CONF=/nonexistent $T/rudolf-display --monitors > $D/monitors && grep -q '"DUMMY1"' $D/monitors || { cat $D/monitors; echo "FAIL rudolf-display --monitors"; exit 1; }
node $HERE/monbeh_t.js $BIN $D $OUT $PORT $DSP; r=$?
echo "pictures: $OUT"; exit $r
