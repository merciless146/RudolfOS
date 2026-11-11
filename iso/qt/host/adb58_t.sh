#!/bin/sh
# 1.5.8 (plan 7.2): an Android phone with USB debugging on (an ADB interface) is given to the desktop user (group 1000)
# so scrcpy and adb in Debian can reach it; other USB devices stay root's.   sh adb58_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); DP=$HERE/../../sys/root/usr/lib/rudolf/devplug
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; P=0; F=0
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
[ "$(id -u)" = 0 ] || { echo "SKIP (needs root to change groups)"; exit 0; }
mk() { s=$W/sys/$1; mkdir -p "$s/$1:1.0" "$W/dev/bus/usb/001"; printf 'DEVNAME=bus/usb/001/%s\n' $2 > $s/uevent
	printf '%s' $3 > "$s/$1:1.0/bInterfaceClass"; printf '%s' $4 > "$s/$1:1.0/bInterfaceSubClass"; printf '%s' $5 > "$s/$1:1.0/bInterfaceProtocol"
	mknod "$W/dev/bus/usb/001/$2" c 189 $(( $2 )) 2>/dev/null || : > "$W/dev/bus/usb/001/$2"; chmod 600 "$W/dev/bus/usb/001/$2"; }
# devplug's adb_perm alone (the rest of devplug acts on the real system)
sed -n '/^adb_perm() {/,/^}/p' $DP > $W/f.sh
mk 1-2 005 ff 42 01; mk 1-3 006 03 01 02
( . $W/f.sh; RUDOLF_DEV=$W/dev adb_perm $W/sys/1-2; RUDOLF_DEV=$W/dev adb_perm $W/sys/1-3 )
g=$(stat -c %g:%a $W/dev/bus/usb/001/005); ok "1. a phone with USB debugging: group 1000, 660" $([ "$g" = 1000:660 ]; echo $?) "$g"
g=$(stat -c %g:%a $W/dev/bus/usb/001/006); ok "2. a keyboard: unchanged (root's)" $([ "$g" = 0:600 ]; echo $?) "$g"
grep -q 'bindmnt("/dev/bus/usb"' $HERE/../../sys/rudolf-debian.c; ok "3. Debian sees /dev/bus/usb" $?
echo "$P/$((P+F)) passed"; [ $F = 0 ]
