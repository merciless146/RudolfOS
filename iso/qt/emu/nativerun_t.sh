#!/bin/bash
# 1.6.2: Emu's Nintendo 64 and Dreamcast games, played for real in their native emulators from Flathub (Rosalie's Mupen
# GUI and Flycast), started the way Rudolf OS starts them: the real rudolfd, built here and run in a private mount and
# network namespace (as iso/qt/host/peer59_t.sh does), adds the made-up test games (n64test.py, dctest.py: no Nintendo
# or Sega material) to the library with games/import and starts each with native/run, which runs the emulator as the
# desktop's user with the session's display. Under Xvfb, screenshots show the test game's colour (red; green while A
# is held, blue while Start is): from the keyboard (XTEST) and from a virtual gamepad (uinput). Then quitting: the
# emulator's own way and native/stop, with no process of the desktop's user left. Every N64 byte order (.z64 .n64
# .v64), every Dreamcast disc format (.gdi with its tracks, .cdi, .chd). GPL-2.0-or-later.
# Needs root, the user "rudolf", flatpak with both apps installed system-wide (as native/install installs them),
# Xvfb, xdotool, ImageMagick, python3 with evdev and Xlib, gcc, zlib, curl; chdman for the .chd.
#   sudo bash nativerun_t.sh [OUTDIR]     (the screenshots and logs go in OUTDIR, default /tmp/nativerun)
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
id rudolf >/dev/null 2>&1 || { echo "SKIP: needs the user rudolf (the desktop's user)"; exit 0; }
command -v flatpak >/dev/null || { echo "SKIP: needs flatpak"; exit 0; }
[ -z "$NR_INNER" ] && exec env NR_INNER=1 unshare -m -n bash "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; OUT=${1:-/tmp/nativerun}; mkdir -p "$OUT"
T=$(mktemp -d /tmp/nativerun.XXXXXX); chmod 755 $T; RU=$(id -u rudolf)
ip link set lo up
mount --make-rprivate / 2>/dev/null
mkdir -p /run/rudolf /usr/share/rudolf/www /home/rudolf /persist /usr/lib/rudolf
mount -t tmpfs none /usr/lib/rudolf; cp $SYS/root/usr/lib/rudolf/games-import.sh $SYS/root/usr/lib/rudolf/rmg-controller.sh /usr/lib/rudolf/   # this tree's
mount -t tmpfs none /run/rudolf; mount --bind $HERE/../../www /usr/share/rudolf/www; mount -t tmpfs -o mode=755 none /home/rudolf; mount -t tmpfs none /persist
chown rudolf: /home/rudolf; mkdir -p /run/user/$RU; chown rudolf: /run/user/$RU; chmod 700 /run/user/$RU
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 800)"; F=$((F+1)); fi; }
note() { echo "NOTE $*"; echo "$*" >> "$OUT/notes.txt"; }
w8() { local i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
cleanup() { [ -n "$PADP" ] && { echo quit > $T/pad.fifo & timeout 2 tail -f /dev/null; kill $PADP 2>/dev/null; }
	kill $RDP 2>/dev/null; pkill -u rudolf 2>/dev/null; kill $XP $DBP 2>/dev/null
	cp /run/rudolf/native.log "$OUT/native.log" 2>/dev/null; cp $T/rd.log "$OUT/rudolfd.log" 2>/dev/null
	umount /usr/lib/rudolf /usr/share/rudolf/www /home/rudolf /persist /run/rudolf 2>/dev/null; rm -rf $T; }
trap cleanup EXIT
# the display (1280x720, no window manager: as a full-screen app sees Rudolf's), the desktop user's session bus
export DISPLAY=:99; Xvfb :99 -screen 0 1280x720x24 -nolisten tcp > $T/xvfb.log 2>&1 & XP=$!
w8 '[ -S /tmp/.X11-unix/X99 ]' 40 || { echo "FAIL Xvfb did not start: $(cat $T/xvfb.log)"; exit 1; }
DBA=""; if command -v dbus-daemon >/dev/null; then
	su -s /bin/sh rudolf -c "XDG_RUNTIME_DIR=/run/user/$RU dbus-daemon --session --address=unix:path=/run/user/$RU/bus --nofork --print-address" > $T/dbus.addr 2>/dev/null & DBP=$!
	w8 '[ -S /run/user/'$RU'/bus ]' 40 && DBA=unix:path=/run/user/$RU/bus; fi
echo "DISPLAY=:99 XDG_RUNTIME_DIR=/run/user/$RU DBUS_SESSION_BUS_ADDRESS=$DBA XDG_CURRENT_DESKTOP=Rudolf" > /run/rudolf/session.env
# rudolfd
gcc -O2 -w -o $T/rudolfd $SYS/rudolfd.c -lz || { echo "FAIL build rudolfd"; exit 1; }
$T/rudolfd > $T/rd.log 2>&1 & RDP=$!
w8 'curl -s http://127.0.0.1:8080/favicon.png -o /dev/null' 60 || { echo "FAIL rudolfd did not start: $(tail -5 $T/rd.log)"; exit 1; }
TOK=$(curl -s http://127.0.0.1:8080/ | sed -n 's/.*RUDOLFD={token:"\([0-9a-f]*\)".*/\1/p' | head -1)
api() { curl -s -H "X-Rudolf-Token: $TOK" "${@:2}" "http://127.0.0.1:8080/api/$1"; }
post() { api "$1" -X POST "${@:2}"; }
enc() { python3 -c 'import sys, urllib.parse; print(urllib.parse.quote(sys.argv[1], safe=""))' "$1"; }
ok "0. rudolfd runs here, with its token" "$([ ${#TOK} = 32 ] && echo 1)" "$(tail -3 $T/rd.log)"
for a in com.github.Rosalie241.RMG org.flycast.Flycast; do
	s=$(api "native/status?id=$a"); ok "0. native/status: $a installed (Flathub: $(flatpak info --system $a 2>/dev/null | sed -n 's/^ *Version: //p'))" "$(echo "$s" | grep -q '"installed":true' && echo 1)" "$s"
	flatpak info --system $a > "$OUT/$a.info" 2>&1; done

# ---- the test games, made here, added to the library as Emu's "Add games" adds them (games/info, games/import)
D=/home/rudolf/Downloads/made; G=/home/rudolf/Games; mkdir -p $D/RudolfDC
python3 $HERE/n64test.py $D/RudolfN64.z64 --n64 $D/RudolfN64.n64 --v64 $D/RudolfN64.v64
python3 $HERE/dctest.py --gdi $D/RudolfDC/RudolfDC.gdi --cdi $D/RudolfDC.cdi
HAVECHD=0; command -v chdman >/dev/null && chdman createcd -i $D/RudolfDC/RudolfDC.gdi -o $D/RudolfDC.chd > $T/chd.log 2>&1 && HAVECHD=1
chown -R rudolf: /home/rudolf/Downloads
gi=$(post games/info); [ -d $G/N64 ] || echo "games/info: $gi"
L="$D/RudolfN64.z64
$D/RudolfN64.n64
$D/RudolfN64.v64
$D/RudolfDC/RudolfDC.gdi
$D/RudolfDC.cdi"; [ $HAVECHD = 1 ] && L="$L
$D/RudolfDC.chd"
r=$(post games/import --data-binary "$L"); w8 '! api job | grep -q "\"running\":true"' 120; j=$(api job)
ok "1. games/import adds the made games (the job's log)" "$(echo "$j" | grep -q "done $((5 + HAVECHD)) added, 0 skipped" && echo 1)" "$r $j"
ok "...the N64 games in N64/, every byte order" "$([ -f $G/N64/RudolfN64.z64 ] && [ -f $G/N64/RudolfN64.n64 ] && [ -f $G/N64/RudolfN64.v64 ] && echo 1)" "$(ls -R $G)"
ok "...the .gdi in DC/RudolfDC/ with its three tracks, the .cdi (and .chd) in DC/" "$([ -f $G/DC/RudolfDC/RudolfDC.gdi ] && [ -f $G/DC/RudolfDC/track01.bin ] && [ -f $G/DC/RudolfDC/track02.raw ] && [ -f $G/DC/RudolfDC/track03.bin ] && [ -f $G/DC/RudolfDC.cdi ] && { [ $HAVECHD = 0 ] || [ -f $G/DC/RudolfDC.chd ]; } && echo 1)" "$(ls -R $G)"
[ $HAVECHD = 1 ] || note "no chdman: the .chd was not made or tried"
l=$(api "fs/list?path=$(enc $G/DC)")
ok "...and the library's listing (fs/list, as Emu reads it) has them" "$(echo "$l" | grep -q '"RudolfDC"' && echo "$l" | grep -q 'RudolfDC.cdi' && echo 1)" "$l"

# ---- helpers: the screen, the emulator's window, its processes
shot() { import -display :99 -window root "$OUT/$1.png" 2>/dev/null; }
# the share of the screen within 12% of a colour (0-1)
frac() { convert "$OUT/$1.png" -fuzz 12% -fill black +opaque "$2" -fill white -opaque "$2" -colorspace gray -format '%[fx:mean]' info: 2>/dev/null || echo 0; }
colour_wait() {   # NAME COLOUR SECONDS: until the colour fills most of the screen (> 50%), the last shot as NAME
	local i=0; while [ $i -lt $(($3 * 2)) ]; do shot "$1"; awk -v f="$(frac "$1" "$2")" 'BEGIN { exit !(f > 0.5) }' && return 0; timeout 0.5 tail -f /dev/null; i=$((i+1)); done; return 1; }
bigwin() { for w in $(xdotool search --onlyvisible --name '' 2>/dev/null); do eval "$(xdotool getwindowgeometry --shell $w 2>/dev/null)"; echo "$((WIDTH * HEIGHT)) $w"; done | sort -n | tail -1 | cut -d' ' -f2; }
upprocs() { pgrep -u rudolf -a | grep -v 'dbus-daemon\|systemd --user\|(sd-pam)' ; }   # (su's PAM starts systemd --user)
desc() {   # NAME: the screenshot described in the log, 8x4 cells of colour, and where its pixels are
	note "$1.png: $(convert "$OUT/$1.png" -scale '8x4!' -depth 8 txt:- 2>/dev/null | sed -n 's/.*#\([0-9A-F]\{6\}\).*/\1/p' | tr '\n' ' ')"; }
gone() { w8 '[ -z "$(upprocs)" ]' ${1:-40}; }
key() { local w=$(bigwin); [ -n "$w" ] && xdotool windowfocus --sync $w 2>/dev/null; xdotool keydown $1; }
unkey() { xdotool keyup $1; }
PADP=""; mkfifo $T/pad.fifo; chmod 666 $T/pad.fifo
if python3 -c 'import evdev' 2>/dev/null && [ -e /dev/uinput ]; then
	python3 $HERE/nrhelp.py pad $T/pad.fifo > $T/pad.dev 2> $T/pad.err & PADP=$!; w8 '[ -s $T/pad.dev ]' 20
	[ -s $T/pad.dev ] && note "the virtual gamepad: $(cat $T/pad.dev)" || { note "no virtual gamepad: $(cat $T/pad.err)"; PADP=""; }
else note "no virtual gamepad (python3-evdev or /dev/uinput missing)"; fi
pad() { [ -n "$PADP" ] && echo "$1 $2" > $T/pad.fifo; }
run() { : > /run/rudolf/native.log; post "native/run?id=$1&file=$(enc "$2")"; }
stop() { post "native/stop?id=$1"; }
RED='rgb(248,0,0)'; GREEN='rgb(0,248,0)'; BLUE='rgb(0,0,248)'
inputs() {   # NAME APP KEY-A KEY-START: held buttons change the colour, from the keyboard and the gamepad
	local n=$1; key $3; colour_wait $n-key-a "$GREEN" 10; local ka=$?; unkey $3; colour_wait $n-released "$RED" 10
	key $4; colour_wait $n-key-start "$BLUE" 10; local ks=$?; unkey $4; colour_wait $n-released "$RED" 10
	ok "$n: the keyboard's A ($3) held turns the screen green, Start ($4) blue" "$([ $ka = 0 ] && [ $ks = 0 ] && echo 1)" "A: $(frac $n-key-a "$GREEN") Start: $(frac $n-key-start "$BLUE")"
	if [ -n "$PADP" ]; then pad down A; colour_wait $n-pad-a "$GREEN" 10; local pa=$?; pad up A; colour_wait $n-released "$RED" 10
		pad down START; colour_wait $n-pad-start "$BLUE" 10; local ps=$?; pad up START; colour_wait $n-released "$RED" 10
		ok "$n: the gamepad's A held turns the screen green, Start blue" "$([ $pa = 0 ] && [ $ps = 0 ] && echo 1)" "A: $(frac $n-pad-a "$GREEN") Start: $(frac $n-pad-start "$BLUE")"; fi; }
screen_full() {   # NAME: the emulator's window covers the screen and asks for full screen
	local w=$(bigwin); eval "$(xdotool getwindowgeometry --shell $w 2>/dev/null)"; local fs=$(python3 $HERE/nrhelp.py fullscreen $w 2>/dev/null)
	note "$1: the window $w is ${WIDTH}x$HEIGHT at $X,$Y on the 1280x720 screen; it asks for full screen: $fs; the colour covers $(frac $1 "$RED") of the screen"
	echo "${WIDTH}x$HEIGHT $fs"; }

# ---- Nintendo 64: Rosalie's Mupen GUI
M=com.github.Rosalie241.RMG; MC=/home/rudolf/.var/app/$M
note "RMG's options: $(su -s /bin/sh rudolf -c "set -a; . /run/rudolf/session.env; set +a; timeout 20 flatpak run $M --help" 2>&1 | grep -E '^ +-' | tr -s ' ' | tr '\n' '|')"
# first as 1.6.1's rudolfd started it (its native_game_args, by hand: 1.6.1's native/run never got this far), fresh
su -s /bin/sh rudolf -c "set -a; . /run/rudolf/session.env; set +a; exec flatpak run --filesystem=/home/rudolf/Games:ro $M --fullscreen --nogui \"$G/N64/RudolfN64.z64\"" > $T/n64-161.log 2>&1 &
colour_wait n64-161 "$RED" 90; c=$?; desc n64-161
ok "2. N64 .z64 with 1.6.1's arguments: the test game's red fills the screen within 90 s (its boot code, video, RDP ran)" "$([ $c = 0 ] && echo 1)" "red: $(frac n64-161 "$RED") $(tail -20 $T/n64-161.log)"
fs=$(screen_full n64-161); ok "...full screen (--fullscreen: the window is the screen's size)" "$(echo "$fs" | grep -q '^1280x720' && echo 1)" "$fs"
key x; colour_wait n64-161-x "$GREEN" 6; fx=$?; unkey x; key Shift_L; colour_wait n64-161-shift "$GREEN" 4; fsh=$?; unkey Shift_L
fp=1; [ -n "$PADP" ] && { pad down A; colour_wait n64-161-pad "$GREEN" 6; fp=$?; pad up A; }
note "N64 as 1.6.1 started it, a fresh install (no controller set up in its settings): keyboard X $([ $fx = 0 ] && echo works || echo 'does nothing'), Left Shift $([ $fsh = 0 ] && echo works || echo 'does nothing'), the gamepad's A $([ $fp = 0 ] && echo works || echo 'does nothing')"
grep -n 'Input Plugin Profile' $MC/config/RMG/mupen64plus.cfg > $T/prof 2>/dev/null; note "...its settings' controller profiles: $(cat $T/prof | tr '\n' ' ')"
key Escape; timeout 0.3 tail -f /dev/null; unkey Escape; timeout 6 tail -f /dev/null; shot n64-161-esc; desc n64-161-esc
note "...after Esc (6 s): $(upprocs | grep -c 'RMG') RMG processes left$(upprocs | grep -q RMG && echo ' (an empty window, no menus: --nogui)')"
pkill -u rudolf -f 'RMG|flatpak|bwrap' 2>/dev/null; gone 30; pkill -9 -u rudolf -f 'RMG|flatpak|bwrap' 2>/dev/null; gone 10
cp $MC/config/RMG/mupen64plus.cfg "$OUT/rmg-fresh.cfg" 2>/dev/null; find $MC -maxdepth 5 > "$OUT/rmg-files.txt" 2>/dev/null
# then by this tree's rudolfd: native/run (controller 1 set up by rmg-controller.sh, --quit-after-emulation)
r=$(run $M $G/N64/RudolfN64.z64)
ok "3. N64 .z64: native/run starts Rosalie's Mupen GUI with it (it answers: 1.6.1's rudolfd gave an empty answer, freeing a buffer it did not own)" "$(echo "$r" | grep -q '"ok":true' && echo 1)" "$r $(tail -3 $T/rd.log)"
colour_wait n64-z64 "$RED" 90; c=$?; desc n64-z64; cp /run/rudolf/native.log "$OUT/n64-z64.log"
ok "...and the test game's red fills the screen within 90 s" "$([ $c = 0 ] && echo 1)" "red: $(frac n64-z64 "$RED") $(tail -20 /run/rudolf/native.log)"
pgrep -u rudolf -a > "$OUT/n64-processes.txt"
ok "...controller 1 set up once in its settings (rmg-controller.sh)" "$(grep -q "Input Plugin Profile 0" $MC/config/RMG/mupen64plus.cfg 2>/dev/null && echo 1)" "$(cat $T/prof)"
inputs n64 RMG x Return
key Escape; timeout 0.3 tail -f /dev/null; unkey Escape; gone 30; s=$(api "native/status?id=$M")
ok "...Esc ends the game and closes the emulator (nothing of it left, native/status not running)" "$([ -z "$(upprocs)" ] && echo "$s" | grep -q '"running":false' && echo 1)" "left: $(upprocs | head -5); $s"
EEP=$(find $MC -iname '*.eep' 2>/dev/null | head -1)
ok "...the game's EEPROM write is saved by the emulator (its own save folder): RUDOLF64" "$([ -n "$EEP" ] && head -c 8 "$EEP" | grep -q RUDOLF64 && echo 1)" "$(find $MC -ipath '*save*' 2>/dev/null | head)"
[ -n "$EEP" ] && note "N64 EEPROM save: ~/${EEP#/home/rudolf/}"
r=$(stop $M); gone 40
cp $MC/config/RMG/mupen64plus.cfg "$OUT/rmg-after.cfg" 2>/dev/null
for o in n64 v64 z64; do
	r=$(run $M $G/N64/RudolfN64.$o); colour_wait n64-$o "$RED" 90; c=$?; desc n64-$o
	ok "4. N64 .$o: the test game's red fills the screen" "$([ $c = 0 ] && echo 1)" "$r red: $(frac n64-$o "$RED") $(tail -10 /run/rudolf/native.log)"
	r=$(stop $M); gone 40; ok "...native/stop (Emu's way of stopping it): nothing of it left" "$([ -z "$(upprocs)" ] && echo 1)" "$r $(upprocs)"
done

# ---- Dreamcast: Flycast (its HLE BIOS: no BIOS files anywhere)
FC=org.flycast.Flycast; FCD=/home/rudolf/.var/app/$FC
n=0; for f in RudolfDC/RudolfDC.gdi RudolfDC.cdi RudolfDC.chd; do
	[ $f = RudolfDC.chd ] && [ $HAVECHD = 0 ] && continue
	x=${f##*.}; r=$(run $FC $G/DC/$f)
	ok "4. Dreamcast .$x: native/run starts Flycast with it" "$(echo "$r" | grep -q '"ok":true' && echo 1)" "$r"
	colour_wait dc-$x "$RED" 90; c=$?; cp /run/rudolf/native.log "$OUT/dc-$x.log"; desc dc-$x
	ok "...and the test disc's red fills the screen within 90 s (the HLE BIOS booted its IP.BIN)" "$([ $c = 0 ] && echo 1)" "red: $(frac dc-$x "$RED") $(tail -20 /run/rudolf/native.log)"
	if [ $x = gdi ]; then pgrep -u rudolf -a > "$OUT/dc-processes.txt"; fs=$(screen_full dc-$x)
		ok "...full screen (the window is the screen's size)" "$(echo "$fs" | grep -q '^1280x720' && echo 1)" "$fs"
		inputs dc Flycast x Return; fi
	if [ $x = cdi ]; then   # its own way out: the window's close (a title bar's), then nothing left
		w=$(bigwin); python3 $HERE/nrhelp.py close $w; gone 30
		ok "...closing its window ends Flycast (nothing left)" "$([ -z "$(upprocs)" ] && echo 1)" "$(upprocs)"; fi
	r=$(stop $FC); gone 40; ok "...native/stop: nothing left" "$([ -z "$(upprocs)" ] && echo 1)" "$r $(upprocs)"
	s=$(api "native/status?id=$FC"); ok "...native/status: not running" "$(echo "$s" | grep -q '"running":false' && echo 1)" "$s"
done
note "Flycast's settings: $(grep -v '^ *$' $FCD/config/flycast/emu.cfg 2>/dev/null | grep -i -E 'fullscreen|cable|region|bios|hle|width|height' | tr '\n' ' ')"
note "Dreamcast: Flycast's files: $(cd /home/rudolf && find .var/app/$FC -maxdepth 4 -type f 2>/dev/null | tr '\n' ' ')"
ls -la $FCD/data/flycast 2>/dev/null > "$OUT/flycast-data.txt"
ok "5. no Dreamcast BIOS files anywhere in Flycast's folders (its HLE BIOS played the disc)" "$(find $FCD -iname 'dc_boot*' 2>/dev/null | grep -q . && echo 0 || echo 1)" "$(find $FCD -iname 'dc_*')"
echo "---- notes"; cat "$OUT/notes.txt"; [ $F = 0 ] || { echo "---- rudolfd's log"; tail -20 $T/rd.log; echo "---- the emulators' log"; tail -40 /run/rudolf/native.log; }
echo "$((N-F))/$N passed"; [ $F = 0 ]
