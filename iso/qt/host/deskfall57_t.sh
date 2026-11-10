#!/bin/sh
# 1.5.7: the session gives up on a desktop that cannot start: Lite (WebKit) that keeps closing as it starts (the
# Vivobook: every 8 s, without end), Lite that never answered before the computer was switched off, the shell that
# keeps closing, and a desktop asking for the shell again and again. The session's own functions, with /persist and
# /run/rudolf in memory.   sh deskfall57_t.sh (root)
HERE=$(cd "$(dirname "$0")" && pwd); S=$HERE/../../sys/root/usr/lib/rudolf/session
[ "$(id -u)" = 0 ] || { echo "run as root"; exit 2; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
for f in webkit_usable webkit_give_up webkit_trial_check webkit_trial_mark webkit_trial_ok shell_switch_limit desk_exited; do
	sed -n "/^$f() {/,/^}/p" "$S" | sed "s#/usr/bin/rudolf-webkit#$W/rudolf-webkit#"; done > $W/f.sh
printf '#!/bin/sh\n' > $W/rudolf-webkit; chmod 755 $W/rudolf-webkit
unshare -m --propagation private sh -s "$W" <<'XEOF'
W=$1; P=0; F=0; ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1"; fi; }
mkdir -p /persist /run/rudolf; mount -t tmpfs t /persist; mount -t tmpfs t /run/rudolf; mkdir -p /persist/etc
echo quiet > $W/cmdline; RCMD=$W/cmdline; touch /run/rudolf/persistent
rudolfd() { echo "$*" >> $W/health; }
. $W/f.sh
lite() { echo webkit > /persist/etc/rudolf-desktop-engine; cp /persist/etc/rudolf-desktop-engine /run/rudolf/engine-choice; rm -f /run/rudolf/webkit-failed /persist/etc/rudolf-desktop-engine.*; }
start() { FIRST=$1; webkit_trial_check; ENGINE=chromium; SHELLON=${2:-0}; webkit_usable && { ENGINE=webkit; SHELLON=0; }; webkit_trial_mark; }
# the Vivobook: Lite closes within seconds, again and again
lite; FAST=0; start 1
ok "1. Lite chosen: on trial (marked) as it starts" $([ $ENGINE = webkit ] && [ -s /persist/etc/rudolf-desktop-engine.trying ]; echo $?)
desk_exited 8 >/dev/null; start 0; desk_exited 7 >/dev/null; start 0
ok "2. two quick closes: still tried" $([ $ENGINE = webkit ] && [ ! -e /run/rudolf/webkit-failed ]; echo $?)
desk_exited 8 >/dev/null; start 0
ok "3. the third quick close in a row: Standard from now on" $([ $ENGINE = chromium ] && [ -e /run/rudolf/webkit-failed ]; echo $?)
ok "4. ...for good: the choice is removed and why is kept for Settings" $([ ! -e /persist/etc/rudolf-desktop-engine ] && grep -q 'closed 3 times in a row' /persist/etc/rudolf-desktop-engine.failed && [ ! -e /persist/etc/rudolf-desktop-engine.trying ]; echo $?)
ok "5. ...and the user is told (System health)" $(grep -q 'standard engine is used from now on' $W/health; echo $?)
# a slow first start is not a failure: it answered
lite; FAST=0; start 1; touch /run/rudolf/heartbeat; webkit_trial_ok; desk_exited 8 >/dev/null; desk_exited 8 >/dev/null; desk_exited 8 >/dev/null; rm /run/rudolf/heartbeat
ok "6. Lite that answered once is kept (the trial ends), and closes after it answered do not count" $([ ! -e /persist/etc/rudolf-desktop-engine.trying ] && [ -e /persist/etc/rudolf-desktop-engine ] && [ $FAST = 0 ]; echo $?)
desk_exited 300 >/dev/null; ok "7. a close after a long run resets the count" $([ $FAST = 0 ]; echo $?)
# switched off at a grey screen: the next start gives Lite up before trying it
lite; start 1; rm -f /run/rudolf/webkit-failed; start 1
ok "8. Lite that never answered before the computer was switched off: given up at the next start" $([ $ENGINE = chromium ] && grep -q 'did not reach the sign-in screen' /persist/etc/rudolf-desktop-engine.failed; echo $?)
# the shell: closing as it starts
rm -f /run/rudolf/engine-choice /run/rudolf/shell-failed; FAST=0; ENGINE=chromium; SHELLON=1
desk_exited 5 >/dev/null; desk_exited 5 >/dev/null; desk_exited 5 >/dev/null
ok "9. the shell closing 3 times as it starts: the desktop page by itself" $([ -e /run/rudolf/shell-failed ]; echo $?)
SHELLON=0; FAST=0; : > $W/health; for i in 1 2 3 4 5; do desk_exited 5 >/dev/null; done
ok "10. Chromium by itself closing: reported once" $([ $(grep -c 'closed 3 times' $W/health) = 1 ]; echo $?)
# a desktop asking for the shell again and again
rm -f /run/rudolf/shell-failed; SW_T=0; SW_N=0; n=0
for i in 1 2 3 4 5 6; do touch /run/rudolf/shell-switch; shell_switch_limit >/dev/null; [ -e /run/rudolf/shell-switch ] && n=$((n+1)); rm -f /run/rudolf/shell-switch; done
ok "11. shell restarts: 3 in 5 minutes are allowed, then ignored and the shell given up" $([ $n = 3 ] && [ -e /run/rudolf/shell-failed ]; echo $?)
echo "$P/$((P+F)) passed"; [ $F = 0 ]
XEOF
r=$?
grep -q 'unlink(PERSIST "/etc/rudolf-desktop-engine.failed"); unlink(PERSIST "/etc/rudolf-desktop-engine.trying")' $HERE/../../sys/rudolfd.c && echo "PASS 12. choosing an engine again starts a new trial (rudolfd)" || { echo "FAIL 12"; r=1; }
exit $r
