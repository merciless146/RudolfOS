#!/bin/sh
# 1.6.2 (docs/plan-1.6.2.md Part 29, its left part): an app's own maximize and minimize buttons and a drag of its own title bar or edges
# on Chromium, as on Native. The same stand-in app (fixtures/xwmclient.c csd: it draws its own bar and asks _NET_WM_MOVERESIZE on a
# press there, or with the edge in XWM_DIR; _NET_WM_STATE maximized; WM_CHANGE_STATE iconic; _NET_WM_STATE_HIDDEN) asks each on:
#   Chromium: rudolf-display (its window manager) on Xvfb, the desktop page in Chromium with a stand-in rudolfd (xfask162_t.js);
#   Native:   rudolf-native on Xvfb with its stand-in rudolfd (fixtures/native_rd.py), as csd161_t.sh; the stand-in given Rudolf's
#             title bar there (decorations on), so its frame is the one the page draws on Chromium (1 1 35 1);
# and what each did to the app's window (moved, sized, maximized, the state, minimized, brought back) is compared, line by line.
# Then rudolfd's own code: apps/show's max and apps/windows' asks (forged lines left out). Needs root (a private /run, in a mount
# namespace), Xvfb, xdotool, xwininfo, ImageMagick, gcc, node with Playwright, python3 and WebKitGTK's headers (for Native; else
# the Chromium half alone, against Native's rules).   sh xfask162_t.sh [OUT]   (ports 8604, 8605, 8606; displays :92, :93)
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$XA_INNER" ] && exec env XA_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; T=$(mktemp -d /tmp/xa162.XXXXXX); chmod 755 $T
OUT=${1:-$T/out}; mkdir -p "$OUT"
for c in Xvfb xdotool xwininfo node gcc python3; do command -v $c >/dev/null || { echo "SKIP: needs $c"; exit 0; }; done
mount --make-rprivate / 2>/dev/null; mount -t tmpfs none /run; mkdir -p /run/rudolf
L=/usr/lib/x86_64-linux-gnu
gcc -O2 -Wall -o $T/rds $SYS/rudolf-display.c -I$SYS -lX11 $L/libXtst.so.6 $L/libXfixes.so.3 $L/libXrandr.so.2 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build rudolf-display"; exit 1; }
XC=$T/xc; gcc -O2 -Wall -o $XC $HERE/fixtures/xwmclient.c -lX11 2>$T/cc || { cat $T/cc; echo "FAIL build xwmclient"; exit 1; }
trap 'kill $RDS $XV $XV2 $NP $RDP $A1 $RDD 2>/dev/null; pkill -f "$T/" 2>/dev/null; umount -l /usr/share/rudolf/www /home/rudolf /persist /usr/bin /run 2>/dev/null; rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
# ---- Chromium
Xvfb :92 -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=:92; w8 'xdotool getmouselocation >/dev/null 2>&1' 40
echo "bottom 48" > /run/rudolf/strut; echo desktop > /run/rudolf/wm; rm -f /run/rudolf/wm-native /run/rudolf/wmask
$T/rds --input > $T/rds.log 2>&1 & RDS=$!
XA_T=$T XA_XC=$XC XA_OUT=$OUT node $HERE/xfask162_t.js; r=$?
kill $RDS $XV 2>/dev/null; wait $RDS $XV 2>/dev/null
# ---- Native: the same requests, as csd161_t.sh makes them
NAT=0
if pkg-config --exists webkit2gtk-4.1 gtk+-3.0 && gcc -O2 -w -o $T/rudolf-native $SYS/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc; then NAT=1
	D=$T/d; mkdir -p $D; node $HERE/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
	PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)")
	printf '%s\n' '{"s":{"dm":"light","bs":"solid","b1":"#112233","b2":"#000080","tb":"bottom","accent":"#2563eb","h24":true,"df":"ymd"},"pins":["notepad","calc","settings"],"notes":2,"wallpaper":false}' > $D/settings.json
	python3 $HERE/fixtures/native_rd.py 8605 $D > $T/nrd.log 2>&1 & RDP=$!
	Xvfb :93 -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV2=$!
	export DISPLAY=:93 LIBGL_ALWAYS_SOFTWARE=1 XWM_DIR=$T/dir; w8 'xdotool getmouselocation >/dev/null 2>&1' 40
	(cd $D && HOME=$D RUDOLF_NATIVE_STATE=$D/state exec $T/rudolf-native --size=1280x800 --draw=soft --base=http://127.0.0.1:8605 > $T/native.log 2>&1) & NP=$!
	st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
	cl() { grep "^client $1 " $D/state | head -1; }
	flag() { cl $1 | grep -o "$2=[0-9]" | cut -d= -f2; }
	hit() { grep "^hit $1 $2 " $D/state | head -1 | awk '{print $4+int($6/2), $5+int($7/2)}'; }
	xg() { xwininfo -id 0x$1 2>/dev/null | awk '/Absolute upper-left X/{x=$4} /Absolute upper-left Y/{y=$4} /Width:/{w=$2} /Height:/{h=$2} END{print x, y, w, h}'; }
	dl() { echo "$1 $2" | awk '{print $5-$1, $6-$2, $7-$3, $8-$4}'; }
	nst() { $XC props 0x$1 | sed -n 's/^state //p'; }
	pdrag() { echo $5 > $T/dir; xdotool mousemove $1 $2 mousedown 1; timeout 0.3 tail -f /dev/null; k=1; while [ $k -le 5 ]; do xdotool mousemove $(($1+$3*k/5)) $(($2+$4*k/5)); timeout 0.08 tail -f /dev/null; k=$((k+1)); done; xdotool mouseup 1; timeout 0.4 tail -f /dev/null; }
	w8 '[ "$(st signin)" = 1 ]' 120; LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return
	w8 '[ "$(st user)" = Ann ] && [ "$(st signin)" = 0 ]' 120; timeout 1 tail -f /dev/null
	echo 8 > $T/dir; $XC csd 500 360 > $T/a1.log & A1=$!; w8 "grep -q '^id' $T/a1.log" 40; ID=$(sed -n 's/^id 0x//p' $T/a1.log | head -1); w8 '[ -n "$(cl $ID)" ]' 40
	$XC deco 0x$ID 1; timeout 1 tail -f /dev/null   # (Rudolf's title bar on it: the frame the page draws on Chromium)
	: > $OUT/native.txt; rec() { echo "$1 $2" >> $OUT/native.txt; }
	G0=$(xg $ID); set -- $G0; pdrag $(($1+100)) $(($2+15)) 150 90 8; g=$(xg $ID); rec move "$(dl "$G0" "$g")"
	set -- $g; pdrag $(($1+250)) $(($2+15)) 0 -400 8; g=$(xg $ID); set -- $g; rec top "$(($2-35))"
	pdrag $(($1+250)) $(($2+15)) -150 140 8
	for d in 0 1 2 3 4 5 6 7; do a=$(xg $ID); set -- $a; pdrag $(($1+250)) $(($2+200)) 40 30 $d; rec rs$d "$(dl "$a" "$(xg $ID)")"; done
	G0=$(xg $ID); set -- $G0; echo 10 > $T/dir; xdotool mousemove $(($1+250)) $(($2+200)) click 1; timeout 0.4 tail -f /dev/null; xdotool key Right Right Right Down Down; timeout 0.3 tail -f /dev/null; xdotool key Return; timeout 0.5 tail -f /dev/null; rec kmove "$(dl "$G0" "$(xg $ID)")"
	G0=$(xg $ID); set -- $G0; echo 9 > $T/dir; xdotool mousemove $(($1+250)) $(($2+200)) click 1; timeout 0.4 tail -f /dev/null; xdotool key Right Right Down; timeout 0.3 tail -f /dev/null; xdotool key Return; timeout 0.5 tail -f /dev/null; rec ksize "$(dl "$G0" "$(xg $ID)")"
	G0=$(xg $ID); set -- $G0; echo 10 > $T/dir; xdotool mousemove $(($1+250)) $(($2+200)) click 1; timeout 0.4 tail -f /dev/null; xdotool key Left Left Up; timeout 0.3 tail -f /dev/null; xdotool key Escape; timeout 0.5 tail -f /dev/null; rec kesc "$(dl "$G0" "$(xg $ID)")"
	xdotool key a; echo 8 > $T/dir; timeout 0.3 tail -f /dev/null
	G0=$(xg $ID); $XC max 0x$ID 1; timeout 1.5 tail -f /dev/null; GM=$(xg $ID); rec maxgeo "$GM"; rec maxst "$(nst $ID)"
	set -- $GM; pdrag 400 $(($2+15)) 100 100 8; rec maxdrag "$(dl "$GM" "$(xg $ID)")"
	$XC max 0x$ID 2; timeout 1.5 tail -f /dev/null; rec restore "$(dl "$G0" "$(xg $ID)")"; rec resst "$(nst $ID)"
	$XC iconify 0x$ID; timeout 1.2 tail -f /dev/null; rec iconify "$([ "$(flag $ID min)" = 1 ] && echo min || echo -)"
	xdotool mousemove $(hit win $ID) click 1; timeout 1.2 tail -f /dev/null; rec back "$(dl "$G0" "$(xg $ID)")"
	$XC hide 0x$ID; timeout 1.2 tail -f /dev/null; rec hide "$([ "$(flag $ID min)" = 1 ] && echo min || echo -)"
	kill $A1 $NP $RDP $XV2 2>/dev/null
else echo "SKIP N. Native (rudolf-native did not build here: $(head -c 200 $T/cc)): the Chromium half is checked against Native's rules alone"; fi
# ---- the two compared: what each engine did to the app's window
if [ $NAT = 1 ]; then
	D1=$(diff $OUT/native.txt $OUT/chromium.txt)
	ok "C1. Chromium did what Native did with the app's own requests: moved by its bar (the pointer's way, not above the desktop), sized from each edge and corner, the keyboard's move and size and Escape, maximized (the same place, the state), not moved maximized, restored, minimized twice, brought back (line by line: $(wc -l < $OUT/native.txt) lines)" "$([ -s $OUT/native.txt ] && [ -s $OUT/chromium.txt ] && [ -z "$D1" ] && echo 1)" "$D1"
fi
# (and both as Native's own rules say: csd161_t.sh's numbers)
EXP="move 150 90 0 0
top 0
kmove 30 20 0 0
ksize 0 0 22 12
kesc 0 0 0 0
maxst MAXIMIZED_VERT,MAXIMIZED_HORZ
maxdrag 0 0 0 0
restore 0 0 0 0
resst -
iconify min
back 0 0 0 0
hide min"
touch $OUT/chromium.txt; M=$(echo "$EXP" | while read k v; do grep -qx "$k $v" $OUT/chromium.txt || echo "[$k: want $v, got $(grep "^$k " $OUT/chromium.txt)]"; done)
ok "C2. Chromium's numbers are Native's rules (csd161_t.sh 10-28): the pointer's way, 10 px a key, +2 px of edges on a size, Escape back, the state" "$([ -z "$M" ] && echo 1)" "$M"
RS=$(grep '^rs' $OUT/chromium.txt | tr '\n' ' ')
ok "C3. ...sizing by 40 x 30 from each edge and corner: the edges' 2 px added and the way the edge went (0 top-left .. 7 left)" "$([ "$RS" = "rs0 40 30 -38 -28 rs1 0 30 0 -28 rs2 0 30 42 -28 rs3 0 0 42 0 rs4 0 0 42 32 rs5 0 0 0 32 rs6 40 0 -38 32 rs7 40 0 -38 0 " ] && echo 1)" "$RS"
# ---- rudolfd's own code: apps/show's max, apps/windows' asks
mkdir -p $T/up $T/wk /usr/share/rudolf/www /home/rudolf /persist
if mount -t overlay overlay -o lowerdir=/usr/bin,upperdir=$T/up,workdir=$T/wk /usr/bin 2>/dev/null && gcc -O2 -w -o $T/rudolfd $SYS/rudolfd.c -lz 2>$T/cc; then
	printf '#!/bin/sh\nexit 0\n' > /usr/bin/flatpak; chmod 755 /usr/bin/flatpak
	mount --bind $HERE/../../www /usr/share/rudolf/www; mount -t tmpfs none /home/rudolf; mount -t tmpfs none /persist; mkdir -p /persist/etc /persist/var/log
	$T/rudolfd > $T/rd.log 2>&1 & RDD=$!; w8 'curl -s http://127.0.0.1:8080/favicon.png -o /dev/null' 60
	TOK=$(curl -s http://127.0.0.1:8080/ | sed -n 's/.*RUDOLFD={token:"\([0-9a-f]*\)".*/\1/p' | head -1)
	api() { curl -s -o $T/body -w '%{http_code}' -X POST -H "X-Rudolf-Token: $TOK" "http://127.0.0.1:8080/api/$1"; }
	c=$(api 'apps/show?win=1e00007&x=3&y=33&w=1270&h=700&max=1'); c2=$(cat /run/rudolf/wm); api 'apps/show?win=1e00007&x=3&y=33&w=1270&h=700&max=yes' >/dev/null; c3=$(cat /run/rudolf/wm)
	ok "R6. rudolfd: apps/show's max=1 (the page's frame maximized) adds \" max\"; any other value nothing" "$([ "$c" = 200 ] && [ "$c2" = "win 1e00007 3 33 1270 700 max" ] && [ "$c3" = "win 1e00007 3 33 1270 700" ] && echo 1)" "$c [$c2] [$c3]"
	printf 'desktop\n' > /run/rudolf/wm; : > /run/rudolf/windows
	printf '%s\n' '1700000000001 at 1e00007 10 40 500 360 0' '1700000000002 max 1e00007 2' '1700000000003 min 1e00007' '1700000000004 at 1e00007 10 40 500 360 1' \
		'1700000000005 at 1e00007 10 40 5 360 0' '1700000000006 max 1e00007 7' '1700000000007 min 1e00007 x' '1700000000008 run 1e00007' '1700000000009 at 1e00007 10 40 500 360 0 9' \
		'x at 1e00007 1 1 100 100 0' '1700000000010 at 0 10 40 500 360 0' '1700000000011 at 1e00007 10 40 500 360 0","k":"min' > /run/rudolf/wmask
	api 'apps/windows' >/dev/null; A=$(python3 -c "import json;d=json.load(open('$T/body'));print(json.dumps(d.get('asks')))" 2>&1)
	ok "R7. rudolfd: apps/windows passes rudolf-display's asks on (at, max, min, checked); forged or odd lines left out (a size below 40, an action not 0-2, more fields, another kind, no number, window 0, quotes)" "$([ "$A" = '[{"n": 1700000000001, "k": "at", "win": "1e00007", "x": 10, "y": 40, "w": 500, "h": 360, "end": false}, {"n": 1700000000002, "k": "max", "win": "1e00007", "act": 2}, {"n": 1700000000003, "k": "min", "win": "1e00007"}, {"n": 1700000000004, "k": "at", "win": "1e00007", "x": 10, "y": 40, "w": 500, "h": 360, "end": true}]' ] && echo 1)" "$A"
	rm -f /run/rudolf/wmask; api 'apps/windows' >/dev/null; A=$(python3 -c "import json;d=json.load(open('$T/body'));print(d['ok'], d.get('asks'))" 2>&1)
	ok "R8. ...none: an empty list" "$([ "$A" = "True []" ] && echo 1)" "$A"
	kill $RDD 2>/dev/null; wait $RDD 2>/dev/null
else echo "SKIP R6-R8: rudolfd's own code (no overlay mount here, or rudolfd did not build)"; fi
echo "compared and rudolfd: $((N-F))/$N passed"; [ $F = 0 ] && exit $r; exit 1
