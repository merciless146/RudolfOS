#!/bin/sh
# 1.5.9 (plan Part 12, step 5): the session's Native desktop (rudolf-native) with Lite's safeguards, from the session's
# own functions with /persist and /run/rudolf in memory: chosen and taken at start-up, never in recovery mode, not after
# it failed this start; on trial until its heartbeat; given up for good (back to Standard, saying why, Settings naming
# Native) after closing three times in a row as it starts, or a start that never answered; Lite unchanged. And the
# session's start of it (the window manager's mark for rudolf-display, the kiosk pattern, the 90 s rule), the safe
# desktop, and rudolfd's desktop/engine.   sh engine59_t.sh (root)
HERE=$(cd "$(dirname "$0")" && pwd); S=$HERE/../../sys/root/usr/lib/rudolf/session; SYS=$HERE/../../sys
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
for f in webkit_usable webkit_give_up webkit_trial_check webkit_trial_mark webkit_trial_ok native_usable native_give_up native_trial_check native_trial_mark native_trial_ok desk_exited; do
	sed -n "/^$f() {/,/^}/p; /^$f() {.*}$/p" "$S" | sed "s#/usr/bin/rudolf-webkit#$W/rudolf-webkit#; s#/usr/bin/rudolf-native#$W/rudolf-native#"; done > $W/f.sh
for f in native_usable native_give_up native_trial_check native_trial_mark native_trial_ok; do grep -q "^$f()" $W/f.sh || { echo "FAIL $f is not in the session"; exit 1; }; done
printf '#!/bin/sh\n' > $W/rudolf-webkit; chmod 755 $W/rudolf-webkit
gcc -O1 -w -o $W/rd -x c - -lz <<EOF || { echo "FAIL build rudolfd"; exit 1; }
#define main rudolfd_main
#include "$SYS/rudolfd.c"
#undef main
int main(int c, char **v) { struct req r; memset(&r, 0, sizeof r); snprintf(r.method, sizeof r.method, "%s", v[1]); snprintf(r.path, sizeof r.path, "/api/desktop/engine");
	if (c > 2) snprintf(r.query, sizeof r.query, "%s", v[2]); char tf[] = "/tmp/eng59-XXXXXX"; r.fd = mkstemp(tf); unlink(tf); api(&r);
	lseek(r.fd, 0, SEEK_SET); char b[4096] = {0}; if (read(r.fd, b, sizeof b - 1) < 0) return 1; char *e = strstr(b, "\r\n\r\n"); printf("%.12s|%s\n", b + 9, e ? e + 4 : ""); return 0; }
EOF
unshare -m --propagation private sh -s "$W" "$HERE" <<'XEOF'
W=$1; HERE=$2; P=0; F=0; ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
mkdir -p /persist /run/rudolf; mount -t tmpfs t /persist; mount -t tmpfs t /run/rudolf; mkdir -p /persist/etc
echo quiet > $W/cmdline; RCMD=$W/cmdline; touch /run/rudolf/persistent
rudolfd() { echo "$*" >> $W/health; }
. $W/f.sh
native() { echo native > /persist/etc/rudolf-desktop-engine; cp /persist/etc/rudolf-desktop-engine /run/rudolf/engine-choice; rm -f /run/rudolf/native-failed /run/rudolf/webkit-failed /persist/etc/rudolf-desktop-engine.*; }
start() { FIRST=$1; native_trial_check; webkit_trial_check; ENGINE=chromium; SHELLON=0; webkit_usable && ENGINE=webkit; native_usable && ENGINE=native; webkit_trial_mark; native_trial_mark; }
t() { native_usable && echo yes || echo no; }
native; ok "1. Native chosen, but this Rudolf OS has no rudolf-native: Standard" $([ "$(t)" = no ]; echo $?)
printf '#!/bin/sh\n' > $W/rudolf-native; chmod 755 $W/rudolf-native
ok "2. ...with it: Native" $([ "$(t)" = yes ]; echo $?)
echo 'quiet rudolf.recovery=1' > $W/cmdline; ok "3. never in recovery mode" $([ "$(t)" = no ]; echo $?); echo quiet > $W/cmdline
touch /run/rudolf/native-failed; ok "4. not after it failed this start" $([ "$(t)" = no ]; echo $?); rm /run/rudolf/native-failed
echo native > /persist/etc/rudolf-desktop-engine; echo chromium > /run/rudolf/engine-choice; ok "5. chosen but not taken at start-up: only after a restart" $([ "$(t)" = no ]; echo $?)
native; FAST=0; start 1
ok "6. Native chosen: started, on trial (marked)" $([ $ENGINE = native ] && [ -s /persist/etc/rudolf-desktop-engine.trying ]; echo $?)
touch /run/rudolf/heartbeat; native_trial_ok; rm -f /run/rudolf/heartbeat
ok "7. its heartbeat (its sign-in screen is up) ends the trial" $([ ! -e /persist/etc/rudolf-desktop-engine.trying ] && [ -e /persist/etc/rudolf-desktop-engine ]; echo $?)
native; FAST=0; : > $W/health; start 1; desk_exited 8 >/dev/null; start 0; desk_exited 7 >/dev/null; start 0
ok "8. two quick closes: still tried" $([ $ENGINE = native ] && [ ! -e /run/rudolf/native-failed ]; echo $?)
desk_exited 8 >/dev/null; start 0
ok "9. the third quick close in a row: Standard from now on" $([ $ENGINE = chromium ] && [ -e /run/rudolf/native-failed ]; echo $?)
ok "10. ...for good: the choice removed, why kept for Settings (marked as Native's)" $([ ! -e /persist/etc/rudolf-desktop-engine ] && grep -q 'closed 3 times in a row as it started (Native)$' /persist/etc/rudolf-desktop-engine.failed && [ ! -e /persist/etc/rudolf-desktop-engine.trying ]; echo $?)
ok "11. ...and System health says so" $(grep -q 'the Native desktop did not start (it closed 3 times in a row as it started), so the standard engine is used from now on' $W/health; echo $?)
native; start 1; rm -f /run/rudolf/native-failed; start 1 >/dev/null
ok "12. a start that never answered (switched off at a grey screen): given up at the next start" $([ $ENGINE = chromium ] && grep -q 'did not reach the sign-in screen' /persist/etc/rudolf-desktop-engine.failed; echo $?)
echo webkit > /persist/etc/rudolf-desktop-engine; cp /persist/etc/rudolf-desktop-engine /run/rudolf/engine-choice; rm -f /run/rudolf/*-failed /persist/etc/rudolf-desktop-engine.*; start 1
ok "13. Lite chosen: still Lite (Native plays no part)" $([ $ENGINE = webkit ] && [ -s /persist/etc/rudolf-desktop-engine.trying ]; echo $?)
# rudolfd's desktop/engine
rm -f /persist/etc/rudolf-desktop-engine*; o=$($W/rd POST engine=native)
ok "14. rudolfd: Native cannot be chosen without rudolf-native in the system" $(echo "$o" | head -1 | grep -q '^409' && echo "$o" | grep -q 'no Native engine'; echo $?) "$o"
echo native > /persist/etc/rudolf-desktop-engine; echo chromium > /run/rudolf/desktop-engine; touch /run/rudolf/native-failed; o=$($W/rd GET)
ok "15. rudolfd: the choice Native, what is in use, Native's own failure this start" $(echo "$o" | grep -q '"choice":"native","now":"chromium","available":[a-z]*,"nativeAvailable":false,"failed":true'; echo $?) "$o"
echo native > /run/rudolf/desktop-engine; rm /run/rudolf/native-failed; o=$($W/rd GET); ok "16. ...and Native in use" $(echo "$o" | grep -q '"now":"native".*"failed":false'; echo $?) "$o"
# the safe desktop (Ctrl+Alt+Backspace) turns Native off too
mkdir -p $W/p/etc $W/run; touch $W/run/persistent $W/run/wm-native; echo native > $W/p/etc/rudolf-desktop-engine
printf '#!/bin/sh\n' > $W/nop; chmod 755 $W/nop
DS_PERSIST=$W/p DS_RUN=$W/run DS_PKILL=$W/nop DS_HEALTH=$W/nop sh $HERE/../../sys/root/usr/lib/rudolf/desktop-safe keys >/dev/null
ok "17. the safe desktop turns Native off (this start and for good) and rudolf-display manages windows again" $([ -e $W/run/native-failed ] && [ ! -e $W/run/wm-native ] && [ ! -e $W/p/etc/rudolf-desktop-engine ]; echo $?)
echo "$P/$((P+F)) passed"; [ $F = 0 ]
XEOF
r=$?
# how the session starts it
grep -q 'if \[ $ENGINE = native \]; then touch /run/rudolf/wm-native; else rm -f /run/rudolf/wm-native; fi' $S && echo "PASS 18. the session marks the native window manager for rudolf-display (and removes the mark for the other engines)" || { echo "FAIL 18"; r=1; }
grep -q 'exec /usr/bin/rudolf-native --rudolf-kiosk --size=${SCW}x$SCH $WKSOFT --base=http://127.0.0.1:8080' $S && echo "--rudolf-kiosk" | grep -qE '[-]-rudolf-kiosk' && echo "PASS 19. rudolf-native is started as the desktop user, with the kiosk mark the session's restarts look for, at the screen's size" || { echo "FAIL 19"; r=1; }
grep -q 'if \[ $ENGINE = native \] && \[ $((NOW - START_T)) -gt 90 \] && \[ ! -e /run/rudolf/heartbeat \]; then' $S && grep -q 'native_give_up "no answer from it in 90 s"' $S && echo "PASS 20. no heartbeat in 90 s: Native given up" || { echo "FAIL 20"; r=1; }
grep -q 'native_trial_check; webkit_trial_check' $S && echo "PASS 21. the trial check comes before either engine is chosen" || { echo "FAIL 21"; r=1; }
grep -q 'unlink(PERSIST "/etc/rudolf-desktop-engine.failed"); unlink(PERSIST "/etc/rudolf-desktop-engine.trying")' $SYS/rudolfd.c && echo "PASS 22. choosing an engine again starts a new trial (rudolfd)" || { echo "FAIL 22"; r=1; }
exit $r
