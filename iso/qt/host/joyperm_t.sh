#!/bin/sh
# 1.5.4: joyperm gives game controllers to the desktop user and leaves keyboards, mice and touchpads alone, from the
# key bitmaps real devices report (/sys/class/input/eventN/device/capabilities/key). A stand-in /sys and /dev.
HERE=$(cd "$(dirname "$0")" && pwd); J=$HERE/../../sys/root/usr/lib/rudolf/joyperm
T=$(mktemp -d); trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
export RUDOLF_TEST_SYS=$T/sys RUDOLF_TEST_DEV=$T/dev; mkdir -p $T/dev/input
dev() { mkdir -p $T/sys/class/input/$1/device/capabilities; echo "$2" > $T/sys/class/input/$1/device/capabilities/key; touch $T/dev/input/$1; chmod 640 $T/dev/input/$1; }
dev event1 "1000000000007 ff9f207ac14057ff febeffdfffefffff fffffffffffffffe"     # a laptop keyboard
dev event2 "1f0000 0 0 0 0"                                                     # a mouse
dev event3 "e520 10000 0 0 0 0"                                                 # a touchpad
dev event4 "7cdb000000000000 0 0 0 0"                                           # an Xbox controller (xpad)
dev event5 "3fff000000000000 0 0 0 0"                                           # a DualSense (hid-playstation)
dev event6 "ff00000000 0 0 0 0 0 0 7fdb000000000000 0 0 0 0"                    # a Switch Pro controller with extra triggers
dev event7 "ffffffff 0 0 0 0 0 0 0 0 0 0 0"                                     # a dance mat: extra triggers only (0x2c0-)
dev event8 "10000000000 0 0 0 0"                                                # a joystick: BTN_TRIGGER (0x120) only
dev event9 "0"                                                                  # a sensor with no keys
for c in "event1:1:keyboard" "event2:1:mouse" "event3:1:touchpad" "event4:0:Xbox controller" "event5:0:DualSense" \
         "event6:0:Switch Pro controller" "event7:0:extra triggers only" "event8:0:joystick" "event9:1:no keys"; do
	e=${c%%:*}; r=${c#*:}; want=${r%%:*}; what=${r#*:}
	sh $J check $e; got=$?; [ $got != 0 ] && got=1
	ok "$what: $( [ $want = 0 ] && echo a controller || echo not a controller)" "$([ $got = $want ] && echo 1)" "exit $got"
done
id -u | grep -qx 0 && {
	MDEV=input/event4 ACTION=add sh $J; MDEV=event1 ACTION=add sh $J
	ok "mdev: the controller is the desktop user's (group 1000, 660)" "$([ "$(stat -c %g:%a $T/dev/input/event4)" = 1000:660 ] && echo 1)" "$(stat -c %g:%a $T/dev/input/event4)"
	ok "mdev: the keyboard stays root's" "$([ "$(stat -c %g:%a $T/dev/input/event1)" = 0:640 ] && echo 1)" "$(stat -c %g:%a $T/dev/input/event1)"
}
MDEV=input/event10 ACTION=add sh $J; ok "a device that has gone: nothing happens" "$([ $? = 0 ] && echo 1)"
ok "mdev.conf runs it for event devices, before the rule for other input devices" "$(awk '/joyperm/{a=NR} /^input\/\.\* /{b=NR} END{print (a && b && a<b)?1:0}' $HERE/../../sys/root/etc/mdev.conf)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
