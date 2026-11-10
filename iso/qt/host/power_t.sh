#!/bin/sh
# 1.4.4 part 6: /usr/lib/rudolf/power against a stand-in /sys (an ASUS laptop: platform profile, amd-pstate energy
# preference, a battery with a charge threshold, the charger; plus a mouse's battery at -1 %). sh power_t.sh
SH=${SH:-sh}; HERE=$(cd "$(dirname "$0")" && pwd); P=$HERE/../../sys/root/usr/lib/rudolf/power
T=$(mktemp -d); trap 'rm -rf $T' EXIT
export POWER_SYS=$T/sys POWER_RUN=$T/run POWER_CONF=$T/persist/power.conf POWER_SUSPEND="touch $T/suspended" POWER_PROC=$T/proc
mkdir -p $T/run $T/persist $T/sys/firmware/acpi $T/sys/class/power_supply/BAT0 $T/sys/class/power_supply/ACAD $T/sys/class/power_supply/hidpp_battery_0 $T/proc/acpi/button/lid/LID0
for c in 0 1 2 3; do mkdir -p $T/sys/devices/system/cpu/cpu$c/cpufreq; echo balance_performance > $T/sys/devices/system/cpu/cpu$c/cpufreq/energy_performance_preference
	echo "default performance balance_performance balance_power power" > $T/sys/devices/system/cpu/cpu$c/cpufreq/energy_performance_available_preferences; done
echo "quiet balanced performance" > $T/sys/firmware/acpi/platform_profile_choices; echo balanced > $T/sys/firmware/acpi/platform_profile
B=$T/sys/class/power_supply/BAT0; echo Battery > $B/type; echo 45 > $B/capacity; echo Charging > $B/status; echo System > $B/scope; echo 100 > $B/charge_control_end_threshold
echo 30000000 > $B/energy_now; echo 40000000 > $B/energy_full; echo 42000000 > $B/energy_full_design; echo 8000000 > $B/power_now; echo 120 > $B/cycle_count
A=$T/sys/class/power_supply/ACAD; echo Mains > $A/type; echo 1 > $A/online
M=$T/sys/class/power_supply/hidpp_battery_0; echo Battery > $M/type; echo -1 > $M/capacity; echo Device > $M/scope; echo Discharging > $M/status
echo "state:      open" > $T/proc/acpi/button/lid/LID0/state
L=$T/sys/class/backlight/amdgpu_bl1; mkdir -p $L; echo 65535 > $L/max_brightness; echo 40000 > $L/brightness
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
s=$($SH $P status); echo "$s" | python3 -c 'import json,sys; json.load(sys.stdin)' 2>/dev/null; ok "status is JSON" $([ $? = 0 ] && echo 1 || echo 0) "$s"
ok "the laptop's battery (BAT0, 45 %, Charging), not the mouse's -1 %" $(has "$s" '"battery":{"name":"BAT0","capacity":45,"status":"Charging"') "$s"
ok "on the charger" $(has "$s" '"ac":true') "$s"
ok "what it can do: platform profile, energy preference, battery care, lid" $(has "$s" '"can":{"profile":true,"profileChoices":"quiet balanced performance","epp":true,"care":true,"lid":true') "$s"
s=$($SH $P set mode_ac=performance); ok "Performance plugged in: platform profile performance" $([ "$(cat $T/sys/firmware/acpi/platform_profile)" = performance ] && echo 1 || echo 0) "$(cat $T/sys/firmware/acpi/platform_profile)"
ok "...and every processor's energy preference performance" $([ "$(cat $T/sys/devices/system/cpu/cpu*/cpufreq/energy_performance_preference | sort -u)" = performance ] && echo 1 || echo 0)
ok "...kept in power.conf" $(grep -q '^mode_ac=performance$' $POWER_CONF && echo 1 || echo 0) "$(cat $POWER_CONF)"
$SH $P set mode_ac=saver >/dev/null; ok "Power saver: the quiet profile (this laptop has no low-power) and the power preference" $([ "$(cat $T/sys/firmware/acpi/platform_profile)" = quiet ] && [ "$(cat $T/sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference)" = power ] && echo 1 || echo 0)
$SH $P set care=80 >/dev/null; ok "Battery care 80 %: the charge threshold" $([ "$(cat $B/charge_control_end_threshold)" = 80 ] && echo 1 || echo 0)
for c in 70 90 100; do $SH $P set care=$c >/dev/null; ok "Battery care $c %: the charge threshold" $([ "$(cat $B/charge_control_end_threshold)" = $c ] && echo 1 || echo 0); done
# 1.5.7 (plan stage 6): a battery that keeps another limit than the one asked (some ASUS laptops: 60, 80 or 100 % only)
s=$(POWER_CARE_FW=80 $SH $P set care=70); ok "1.5.7: 70 % asked, 80 % kept: noted, and said in the status (careKept)" $(has "$s" '"careKept":{"70":80}') "$s"
s=$(POWER_CARE_FW=100 $SH $P set care=90); ok "1.5.7: ...90 % asked, 100 % kept: both noted" $(has "$s" '"careKept":{"70":80,"90":100}') "$s"
s=$($SH $P set care=70); ok "1.5.7: ...70 % kept later (a firmware update): taken off the list" $(has "$s" '"careKept":{"90":100}') "$s"
s=$($SH $P set care=90); ok "1.5.7: ...and when every limit is kept, nothing is noted" $(has "$s" '"careKept":{}') "$s"
s=$($SH $P set care=60 2>&1); ok "60 % is no longer a choice (1.5.6): refused" $(has "$s" '"ok":false') "$s"
sed -i 's/^care=.*/care=60/' $POWER_CONF; s=$($SH $P status)
ok "a 60 kept before 1.5.6 is read as 70, not 100, and the battery reports 70 (now.care)" $([ "$(has "$s" '"sleep":0,"care":70}')" = 1 ] && [ "$(has "$s" '"care":70}}')" = 1 ] && [ "$(cat $B/charge_control_end_threshold)" = 70 ] && echo 1 || echo 0) "$s"
$SH $P set button=ask >/dev/null; ok "...and written as 70 at the next change" $(grep -q '^care=70$' $POWER_CONF && echo 1 || echo 0) "$(cat $POWER_CONF)"
s=$($SH $P set mode_ac=fast 2>&1); ok "a value that is not a choice is refused and nothing changes" $([ "$(has "$s" '"ok":false')" = 1 ] && grep -q '^mode_ac=saver$' $POWER_CONF && echo 1 || echo 0) "$s"
# unplugged: battery settings, battery saver at 20 %
$SH $P set mode_ac=balanced mode_bat=balanced saver_at=20 screen_bat=2 >/dev/null; : > $T/run/events
echo 0 > $A/online; echo Discharging > $B/status; echo 50 > $B/capacity; $SH $P apply
ok "unplugged: the battery's settings (balanced, screen off after 2 min)" $(grep -q '^src=bat$' $T/run/power-now && grep -q '^screen=2$' $T/run/power-now && [ "$(cat $T/sys/firmware/acpi/platform_profile)" = balanced ] && echo 1 || echo 0) "$(cat $T/run/power-now)"
ok "...and the event \"power battery\"" $(grep -q '^power battery$' $T/run/events && echo 1 || echo 0) "$(cat $T/run/events)"
echo 19 > $B/capacity; $SH $P apply
ok "at 19 % battery saver goes on by itself: Power saver mode, event" $(grep -q '^saver=1$' $T/run/power-now && [ "$(cat $T/sys/firmware/acpi/platform_profile)" = quiet ] && grep -q '^power saver on$' $T/run/events && echo 1 || echo 0) "$(cat $T/run/power-now; cat $T/run/events)"
ok "...and the screen 30 % dimmer" $([ "$(cat $L/brightness)" = 28000 ] && echo 1 || echo 0) "$(cat $L/brightness)"
echo 1 > $A/online; echo Charging > $B/status; $SH $P apply
ok "...the brightness back as it was" $([ "$(cat $L/brightness)" = 40000 ] && echo 1 || echo 0) "$(cat $L/brightness)"
ok "plugged in again: battery saver off, the charger's settings, events" $(grep -q '^saver=0$' $T/run/power-now && grep -q '^src=ac$' $T/run/power-now && grep -q '^power ac$' $T/run/events && grep -q '^power saver off$' $T/run/events && echo 1 || echo 0) "$(cat $T/run/power-now)"
$SH $P set saver_now=1 >/dev/null; ok "battery saver on now is turned off by itself while plugged in" $(grep -q '^saver_now=0$' $POWER_CONF && echo 1 || echo 0)
echo 0 > $A/online; echo Discharging > $B/status; echo 80 > $B/capacity; $SH $P set saver_now=1 >/dev/null
ok "battery saver on now, on battery at 80 %" $(grep -q '^saver=1$' $T/run/power-now && echo 1 || echo 0)
$SH $P set saver_now=0 >/dev/null
# lid and power button
$SH $P set lid_bat=sleep >/dev/null; rm -f $T/suspended; $SH $P lid closed
ok "lid closed on battery (Sleep): it sleeps, and the event" $([ -e $T/suspended ] && grep -q '^lid closed$' $T/run/events && echo 1 || echo 0)
$SH $P set lid_bat=nothing >/dev/null; rm -f $T/suspended; $SH $P lid closed
ok "lid closed with Do nothing: no sleep" $([ ! -e $T/suspended ] && echo 1 || echo 0)
: > $T/run/events; $SH $P set button=ask >/dev/null; $SH $P button
ok "power button with Ask: the desktop's Shut down dialog (event power <ms>)" $(grep -Eq '^power [0-9]+$' $T/run/events && echo 1 || echo 0) "$(cat $T/run/events)"
$SH $P set button=sleep >/dev/null; rm -f $T/suspended; $SH $P button
ok "power button with Sleep: it sleeps" $([ -e $T/suspended ] && echo 1 || echo 0)
# a computer without any of it (a desktop PC, QEMU)
rm -rf $T/sys/firmware/acpi/* $T/sys/devices $B $A $T/proc/acpi $T/sys/class/backlight; s=$($SH $P status)
ok "without battery, profile or energy preference: reported, not failed" $([ "$(has "$s" '"battery":null')" = 1 ] && [ "$(has "$s" '"profile":false')" = 1 ] && [ "$(has "$s" '"epp":false')" = 1 ] && [ "$(has "$s" '"care":false')" = 1 ] && [ "$(has "$s" '"ac":true')" = 1 ] && echo 1 || echo 0) "$s"
echo "$((N-F))/$N passed"; [ $F = 0 ]
