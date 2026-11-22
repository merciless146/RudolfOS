#!/bin/sh
# 1.6.2: emu-native-setup (run by rudolfd's native/run before an emulator starts): Rosalie's Mupen GUI gets player 1's
# controller profile once (RMG plugs in no controller until its settings have been saved), next to what RMG wrote,
# and a profile the player saved is never touched; other apps are left alone. The real RMG playing with it:
# emunat162_t.py (emu-native-check.yml).   sh emunatsetup162_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); P=$HERE/../../sys/root/usr/lib/rudolf/emu-native-setup
T=$(mktemp -d); trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
C=.var/app/com.github.Rosalie241.RMG/config/RMG/mupen64plus.cfg; S="[Rosalie's Mupen GUI - Input Plugin Profile 0]"
mkdir -p $T/a; HOME=$T/a sh $P com.github.Rosalie241.RMG; rc=$?
ok "1. a first start: the profile, plugged in, Automatic" "$([ $rc = 0 ] && grep -qxF "$S" $T/a/$C && grep -qx 'PluggedIn = True' $T/a/$C && grep -qx 'DeviceType = 2' $T/a/$C && echo 1)" "$(cat $T/a/$C 2>&1 | head -20)"
ok "...A: the gamepad's south button (SDL 0) and the X key (scancode 27); Start: start (6) and Return (40)" "$(grep -qx 'A_InputType = "0;-1"' $T/a/$C && grep -qx 'A_Data = "0;27"' $T/a/$C && grep -qx 'Start_Data = "6;40"' $T/a/$C && grep -qx 'Start_Name = "start;Return"' $T/a/$C && echo 1)" "$(grep '^A_\|^Start_' $T/a/$C)"
ok "...the stick: the left stick's axes and the arrow keys" "$(grep -qx 'AnalogStickUp_InputType = "1;-1"' $T/a/$C && grep -qx 'AnalogStickUp_Data = "1;82"' $T/a/$C && grep -qx 'AnalogStickRight_ExtraData = "1;0"' $T/a/$C && echo 1)" "$(grep '^AnalogStick' $T/a/$C)"
ok "...every one of the 18 inputs has its four lines" "$([ "$(grep -c '_InputType = \|_Name = \|_Data = \|_ExtraData = ' $T/a/$C)" = 72 ] && echo 1)" "$(grep -c '_InputType = \|_Name = \|_Data = \|_ExtraData = ' $T/a/$C)"
cp $T/a/$C $T/before; HOME=$T/a sh $P com.github.Rosalie241.RMG
ok "2. the next start: nothing added (one profile)" "$(cmp -s $T/a/$C $T/before && echo 1)" "$(grep -c 'Profile 0' $T/a/$C)"
mkdir -p "$T/b/$(dirname $C)"; printf '[Core]\n\nVersion = 1.01\n\n[Rosalie'"'"'s Mupen GUI - Input Plugin]\n\nProfiles = ""\n' > $T/b/$C; HOME=$T/b sh $P com.github.Rosalie241.RMG
ok "3. RMG's own file there (no profile): the profile is added after what it wrote, which stays" "$(head -7 $T/b/$C | grep -q 'Version = 1.01' && grep -qxF "$S" $T/b/$C && [ "$(grep -c '^\[' $T/b/$C)" = 3 ] && echo 1)" "$(cat $T/b/$C | head -12)"
mkdir -p "$T/c/$(dirname $C)"; printf '%s\n\nPluggedIn = True\nDeviceType = 4\nA_Data = "1"\n' "$S" > $T/c/$C; cp $T/c/$C $T/c0; HOME=$T/c sh $P com.github.Rosalie241.RMG
ok "4. a profile the player saved is never touched" "$(cmp -s $T/c/$C $T/c0 && echo 1)" "$(cat $T/c/$C)"
mkdir -p $T/d; HOME=$T/d sh $P org.flycast.Flycast; rc=$?
ok "5. Flycast (and any other app): nothing written, and a start is never stopped" "$([ $rc = 0 ] && [ -z "$(ls -A $T/d)" ] && echo 1)" "$(ls -AR $T/d)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
