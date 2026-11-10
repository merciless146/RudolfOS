#!/bin/sh
# 1.5.8: Ctrl+Alt+Backspace held for a second (rudolf-display) starts the desktop again with the standard engine and
# no shell (desktop-safe), for a desktop that shows nothing or keeps restarting.   sh desksafe58_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); S=$HERE/../../sys/root/usr/lib/rudolf/desktop-safe; C=$HERE/../../sys/rudolf-display.c
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; P=0; F=0
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
mkdir -p $W/p/etc $W/run; touch $W/run/persistent; echo webkit > $W/p/etc/rudolf-desktop-engine; date > $W/p/etc/rudolf-desktop-engine.trying
printf '#!/bin/sh\necho "$*" >> %s/pk\n' $W > $W/pkill; printf '#!/bin/sh\necho "$*" >> %s/he\n' $W > $W/health; chmod 755 $W/pkill $W/health
run() { DS_PERSIST=$W/p DS_RUN=$W/run DS_PKILL=$W/pkill DS_HEALTH=$W/health sh $S "$@"; }
o=$(run keys)
ok "1. Lite and the shell are off at once" $([ -e $W/run/webkit-failed ] && [ -e $W/run/shell-failed ]; echo $?)
ok "2. Lite is off for good, saying why (Settings › Desktop engine), and its trial mark goes" $([ ! -e $W/p/etc/rudolf-desktop-engine ] && [ ! -e $W/p/etc/rudolf-desktop-engine.trying ] && grep -q 'Ctrl+Alt+Backspace was held' $W/p/etc/rudolf-desktop-engine.failed; echo $?)
ok "3. the desktop is started again (either engine), and System health and the session log say why" $(grep -q -- '-f \[-\]-rudolf-kiosk' $W/pk && grep -q 'Ctrl+Alt+Backspace was held' $W/he && grep -q 'standard engine and no shell' $W/run/session.log; echo $?)
rm -f $W/run/persistent $W/p/etc/rudolf-desktop-engine.failed; echo webkit > $W/p/etc/rudolf-desktop-engine; run devlink >/dev/null
ok "4. a live start keeps no setting (nothing on the disk changes)" $([ -e $W/p/etc/rudolf-desktop-engine ] && [ ! -e $W/p/etc/rudolf-desktop-engine.failed ]; echo $?)
run other >/dev/null; ok "5. anything else: usage, exit 2" $([ $? = 2 ]; echo $?)
grep -q 'c == KEY_BACKSPACE && mod_ctrl && mod_alt' $C && grep -q 'now_ms() - held >= 1000) runbg("/usr/lib/rudolf/desktop-safe", "keys"' $C; ok "6. rudolf-display: Ctrl+Alt+Backspace held for a second runs it; the key is not passed on" $?
echo "$P/$((P+F)) passed"; [ $F = 0 ]
