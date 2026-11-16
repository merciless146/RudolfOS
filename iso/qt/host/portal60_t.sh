#!/bin/sh
# 1.6.0 (the owner's Vivobook, 2026-11-16: Sober's sign-in spun for ever, then Sober was force-quit): the Rudolf portal's side,
# with this tree's rudolf-portal, the real gnome-keyring-daemon (as /usr/lib/rudolf/keyring starts it) and the stand-in app
# (iso/qt/portalstand.c, the D-Bus calls libsecret and a Flatpak app make) on a private session bus. The desktop's part is
# played as rudolfd does it (an answer in resp/ID.json, the request removed):
#   1. an app asks for a new keyring (CreateCollection, its prompt): the prompt is the desktop's request (kind keyring, a new
#      password); answered with a password: the app gets its keyring and keeps its secret in it (secret-tool finds it)
#   2. an app force-quit while its prompt waits: gnome-keyring-daemon stops prompting and the request goes at once; the next
#      app's prompt is asked (nothing is left blocking the keyring's prompts: Sober started again asks again)
#   3. a file dialog (FileChooser.OpenFile): asked; answered with a file: the app gets it
#   4. a file dialog whose app is force-quit (or closes its request): withdrawn at once (no dialog for an app that is gone)
#   5. a link (OpenURI) from a caller that leaves at once (xdg-open): kept, the desktop still opens it
# Runs in a private mount namespace (/run/rudolf a tmpfs of its own: rudolf-portal's folder is fixed); stops only what it
# started. Needs root, gcc, GLib's and OpenSSL's headers, dbus-daemon, gnome-keyring-daemon, secret-tool.   sh portal60_t.sh
HERE=$(cd "$(dirname "$0")" && pwd)
if [ "$PORTAL60_NS" != 1 ]; then
	[ "$(id -u)" = 0 ] || { echo "SKIP: needs root (a private /run/rudolf)"; exit 0; }
	for c in gnome-keyring-daemon secret-tool dbus-daemon unshare; do command -v $c >/dev/null || { echo "SKIP: needs $c"; exit 0; }; done
	exec env PORTAL60_NS=1 unshare -m --propagation private sh "$0" "$@"
fi
mkdir -p /run/rudolf && mount -t tmpfs -o mode=755 none /run/rudolf || { echo "FAIL: no private /run/rudolf"; exit 1; }
T=$(mktemp -d /tmp/portal60.XXXXXX); PIDS=""
trap 'for p in $PIDS; do kill $p 2>/dev/null; done; rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.1 tail -f /dev/null; i=$((i+1)); done; return 1; }
gcc -O2 -w -o $T/rp $HERE/../../sys/rudolf-portal.c $(pkg-config --cflags --libs gio-2.0 gio-unix-2.0) -lcrypto || { echo "FAIL build rudolf-portal"; exit 1; }
gcc -O2 -static -o $T/ps $HERE/../portalstand.c 2>/dev/null || gcc -O2 -o $T/ps $HERE/../portalstand.c || { echo "FAIL build portalstand"; exit 1; }
mkdir -m 700 $T/rt; mkdir $T/home
export HOME=$T/home XDG_RUNTIME_DIR=$T/rt DBUS_SESSION_BUS_ADDRESS=unix:path=$T/rt/bus PORTALSTAND_DIR=$T/home
dbus-daemon --session --fork --nopidfile --print-pid --address=$DBUS_SESSION_BUS_ADDRESS > $T/dpid; PIDS="$(cat $T/dpid)"
$T/rp > $T/rp.log 2>&1 & PIDS="$PIDS $!"
sh $HERE/../../sys/root/usr/lib/rudolf/keyring start > $T/kr.log 2>&1 & PIDS="$PIDS $!"
w8 'secret-tool search --all x x >/dev/null 2>&1' 100
R=/run/rudolf/portal
reqs() { ls $R/req 2>/dev/null | grep '\.json$' | sed 's/\.json$//'; }
answer() { printf '%s' "$2" > $R/resp/$1.json.t && mv $R/resp/$1.json.t $R/resp/$1.json && rm -f $R/req/$1.json; }
log() { cat $T/home/portalstand.log 2>/dev/null; }
# ---- 1. a new keyring
$T/ps keyring Sober s3cret-1 > /dev/null 2>&1 & A=$!; PIDS="$PIDS $A"
w8 '[ -n "$(reqs)" ]' 50; id=$(reqs | head -1); q=$(cat $R/req/$id.json 2>/dev/null)
ok "1a. an app's new keyring: its prompt is the desktop's request (a new password, who asked)" "$(echo "$q" | grep -q '"kind":"keyring","type":"password"' && echo "$q" | grep -q '"newpass":true' && echo "$q" | grep -q 'keyring called “Sober”' && echo "$q" | grep -q '"proc":"ps"' && echo 1)" "$q $(cat $T/rp.log)"
answer $id '{"response":0,"password":"pw-1"}'
w8 'log | grep -q "keyring created\|keyring error\|keyring dismissed"' 100; wait $A 2>/dev/null
ok "1b. answered with a password: the app has its keyring and its secret kept in it" "$(log | grep -q 'keyring created /org/freedesktop/secrets/collection/Sober item ' && [ "$(secret-tool lookup app portalstand)" = s3cret-1 ] && [ -z "$(reqs)" ] && echo 1)" "$(log) $(reqs)"
# ---- 2. force-quit while its prompt waits; the next one is asked
: > $T/home/portalstand.log
$T/ps keyring Stuck s3 > /dev/null 2>&1 & A=$!; PIDS="$PIDS $A"
w8 '[ -n "$(reqs)" ]' 50; id=$(reqs | head -1)
kill -9 $A; w8 '[ -z "$(reqs)" ]' 30
ok "2a. the app force-quit while its prompt waits: the request goes at once (the desktop's prompt closes)" "$([ -n "$id" ] && [ -z "$(reqs)" ] && echo 1)" "$id / $(reqs) / $(cat $T/rp.log)"
$T/ps keyring Again s3cret-2 > /dev/null 2>&1 & A=$!; PIDS="$PIDS $A"
w8 '[ -n "$(reqs)" ]' 50; id=$(reqs | head -1); q=$(cat $R/req/$id.json 2>/dev/null)
ok "2b. the app started again: its prompt is asked (nothing left blocking the keyring's prompts)" "$(echo "$q" | grep -q 'keyring called “Again”' && echo 1)" "$q $(reqs)"
answer $id '{"response":0,"password":"pw-2"}'; w8 'log | grep -q "keyring created\|keyring error"' 100; wait $A 2>/dev/null
ok "2c. ...answered: the app has its keyring" "$(log | grep -q 'keyring created /org/freedesktop/secrets/collection/Again' && echo 1)" "$(log)"
# ---- 3. a file dialog
: > $T/home/portalstand.log
$T/ps open > /dev/null 2>&1 & A=$!; PIDS="$PIDS $A"
w8 '[ -n "$(reqs)" ]' 50; id=$(reqs | head -1); q=$(cat $R/req/$id.json 2>/dev/null)
ok "3a. a file dialog: the desktop's request (kind open, its title)" "$(echo "$q" | grep -q '"kind":"open","title":"PortalStand: open a file"' && echo 1)" "$q"
answer $id '{"response":0,"uris":["file:///home/rudolf/Documents/level%201.txt"]}'; w8 'log | grep -q "open response\|open error"' 100; wait $A 2>/dev/null
ok "3b. answered with a file: the app gets it" "$(log | grep -q "open response 0, .*'uris': <\['file:///home/rudolf/Documents/level%201.txt'\]>" && [ -z "$(ls $R/resp)" ] && echo 1)" "$(log) / $(ls $R/resp)"
# ---- 4. withdrawn: the app force-quit
$T/ps open > /dev/null 2>&1 & A=$!; PIDS="$PIDS $A"
w8 '[ -n "$(reqs)" ]' 50; id=$(reqs | head -1); kill -9 $A; w8 '[ -z "$(reqs)" ]' 30
ok "4. a file dialog whose app is force-quit: withdrawn at once" "$([ -n "$id" ] && [ -z "$(reqs)" ] && grep -q "left before its question $id was answered: withdrawn" $T/rp.log && echo 1)" "$id / $(reqs) / $(cat $T/rp.log)"
# ---- 5. a link from a caller that leaves at once
$T/ps link https://example.org/signin > /dev/null 2>&1 & A=$!; PIDS="$PIDS $A"
w8 '[ -n "$(reqs)" ]' 50; id=$(reqs | head -1); kill -9 $A; timeout 1.5 tail -f /dev/null; q=$(cat $R/req/$id.json 2>/dev/null)
ok "5. a link from a caller that left at once (xdg-open): kept for the desktop to open" "$(echo "$q" | grep -q '"kind":"url","uri":"https://example.org/signin"' && echo 1)" "$id $q $(reqs)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
