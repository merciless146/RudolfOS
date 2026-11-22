#!/bin/sh
# 1.6.2: rudolfd's native/run with a game from Emu's library (Nintendo 64 in Rosalie's Mupen GUI, Dreamcast in Flycast),
# with a stand-in flatpak (it records how it was called). 1.6.1 freed qparam's own buffer there and rudolfd ended
# ("free(): invalid pointer") on every game started; now: rudolfd answers and lives on, the emulator's command line
# (--die-with-parent so native/stop ends the sandbox; RMG's --quit-after-emulation), emu-native-setup run first as the
# desktop user, a file outside the library refused, native/stop. The real emulators: emunat162_t.py.
# The real rudolfd, built here, in a private mount namespace (an overlay on /usr/bin holds the stand-in), as peer59_t.sh.
# Needs root, curl, port 8080 free.   sh emunatrun162_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
id rudolf >/dev/null 2>&1 || { echo "SKIP: needs a user rudolf"; exit 0; }
[ -z "$EMURUN_INNER" ] && exec env EMURUN_INNER=1 unshare -m --propagation private sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; T=$(mktemp -d /tmp/emurun.XXXXXX); chmod 755 $T
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
gcc -O2 -w -o $T/rudolfd $SYS/rudolfd.c -lz || { echo "FAIL build rudolfd"; exit 1; }
mkdir -p /run/rudolf /home/rudolf /persist /usr/share/rudolf/www /usr/lib/rudolf $T/up $T/work
for d in /run/rudolf /home/rudolf /persist; do mount -t tmpfs none $d; done
mount --bind $HERE/../../www /usr/share/rudolf/www; mount --bind $SYS/root/usr/lib/rudolf /usr/lib/rudolf
chown rudolf /home/rudolf
# the stand-in: "info" knows the two emulators; "run" writes down its arguments and who ran it, then waits
cat > $T/up/flatpak <<'EOF'
#!/bin/sh
case "$1" in
info) case "$*" in *com.github.Rosalie241.RMG*|*org.flycast.Flycast*) echo "     Version: v1"; exit 0;; esac; exit 1;;
run) shift; printf '%s\n' "$@" > /run/rudolf/flatpak-args; echo "$(id -un) $HOME" > /run/rudolf/flatpak-user; exec sleep 300;;
esac
exit 1
EOF
chmod 755 $T/up/flatpak
mount -t overlay overlay -o lowerdir=/usr/bin,upperdir=$T/up,workdir=$T/work /usr/bin || { echo "SKIP: no overlay file system"; exit 0; }
echo "DISPLAY=:99 XDG_RUNTIME_DIR=/run/user/1000" > /run/rudolf/session.env
G=/home/rudolf/Games; mkdir -p "$G/N64" "$G/DC/Made Up Disc"; echo n64 > "$G/N64/Made Up (USA).z64"; echo gdi > "$G/DC/Made Up Disc/Made Up Disc.gdi"; chown -R rudolf $G
echo outside > /home/rudolf/elsewhere.z64
env RUDOLFD_DESKTOP_CG=$T/none $T/rudolfd > $T/rd.log 2>&1 & RDP=$!
trap 'kill $RDP 2>/dev/null; pkill -f "^sleep 300$" -U rudolf 2>/dev/null; umount /usr/bin /usr/share/rudolf/www /usr/lib/rudolf /home/rudolf /persist /run/rudolf 2>/dev/null; rm -rf $T' EXIT
w8 'curl -s http://127.0.0.1:8080/favicon.png -o /dev/null' 40
TOK=$(curl -s http://127.0.0.1:8080/ | sed -n 's/.*RUDOLFD={token:"\([0-9a-f]*\)".*/\1/p' | head -1)
api() { curl -s -H "X-Rudolf-Token: $TOK" ${2:+-X POST -d ''} "http://127.0.0.1:8080/api/$1"; }
enc() { python3 -c 'import sys,urllib.parse;print(urllib.parse.quote(sys.argv[1]))' "$1"; }
o=$(api "native/run?id=com.github.Rosalie241.RMG&file=$(enc "$G/N64/Made Up (USA).z64")" post)
w8 '[ -s /run/rudolf/flatpak-args ]' 40
ok "1. native/run with an N64 game: started, and rudolfd lives on (1.6.1 ended here)" "$(echo "$o" | grep -q '"ok":true' && kill -0 $RDP 2>/dev/null && api ping | grep -q '"ok":true' && echo 1)" "$o $(tail -3 $T/rd.log)"
A=$(cat /run/rudolf/flatpak-args 2>/dev/null | tr '\n' '|')
ok "2. ...Rosalie's Mupen GUI with the game, full screen, no menus, quitting when the game ends, its sandbox ending with su" "$([ "$A" = "--die-with-parent|--filesystem=/home/rudolf/Games:ro|com.github.Rosalie241.RMG|--fullscreen|--nogui|--quit-after-emulation|$G/N64/Made Up (USA).z64|" ] && echo 1)" "$A"
ok "3. ...as the desktop user, with emu-native-setup run first: RMG's controller profile in its home" "$(grep -q "^rudolf /home/rudolf$" /run/rudolf/flatpak-user && grep -q "Input Plugin Profile 0" /home/rudolf/.var/app/com.github.Rosalie241.RMG/config/RMG/mupen64plus.cfg && [ "$(stat -c %U /home/rudolf/.var/app/com.github.Rosalie241.RMG/config/RMG/mupen64plus.cfg)" = rudolf ] && echo 1)" "$(cat /run/rudolf/flatpak-user; ls -la /home/rudolf/.var/app/*/config/RMG 2>&1)"
o=$(api "native/status?id=com.github.Rosalie241.RMG"); ok "4. native/status: running" "$(echo "$o" | grep -q '"running":true' && echo 1)" "$o"
o=$(api "native/stop?id=com.github.Rosalie241.RMG" post)
ok "5. native/stop: the emulator ends, and native/status says so" "$(echo "$o" | grep -q stopped && w8 '! pgrep -U rudolf -f "^sleep 300$" >/dev/null' 40 && w8 'api "native/status?id=com.github.Rosalie241.RMG" | grep -q "\"running\":false"' 40 && echo 1)" "$o $(pgrep -a -U rudolf)"
rm -f /run/rudolf/flatpak-args; o=$(api "native/run?id=org.flycast.Flycast&file=$(enc "$G/DC/Made Up Disc/Made Up Disc.gdi")" post); w8 '[ -s /run/rudolf/flatpak-args ]' 40
A=$(cat /run/rudolf/flatpak-args 2>/dev/null | tr '\n' '|')
ok "6. a Dreamcast .gdi: Flycast with it (and its sandbox ending with su); rudolfd lives on" "$([ "$A" = "--die-with-parent|--filesystem=/home/rudolf/Games:ro|org.flycast.Flycast|$G/DC/Made Up Disc/Made Up Disc.gdi|" ] && kill -0 $RDP && echo 1)" "$A $o"
api "native/stop?id=org.flycast.Flycast" post >/dev/null; w8 'api "native/status?id=org.flycast.Flycast" | grep -q "\"running\":false"' 40
o=$(api "native/run?id=com.github.Rosalie241.RMG&file=$(enc /home/rudolf/elsewhere.z64)" post); o2=$(api "native/run?id=com.github.Rosalie241.RMG&file=$(enc "$G/N64/../elsewhere.z64")" post)
ok "7. a file outside the library is refused, and rudolfd lives on" "$(echo "$o" | grep -q 'must be in the game library' && echo "$o2" | grep -q 'must be in the game library' && kill -0 $RDP && api ping | grep -q '"ok":true' && echo 1)" "$o $o2"
echo "$((N-F))/$N passed"; [ $F = 0 ]
