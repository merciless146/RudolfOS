#!/bin/sh
# 1.5.7 (plan Part 4, step 2): the session's choice of the desktop engine (webkit_usable, from the session itself):
# WebKit only when chosen and taken at start-up (a change applies at the next restart), with rudolf-webkit in the system, not in recovery mode and not after
# it failed this start; and the pattern that ends the desktop matches either engine but never itself.   sh engine57_t.sh (root)
HERE=$(cd "$(dirname "$0")" && pwd); S=$HERE/../../sys/root/usr/lib/rudolf/session
[ "$(id -u)" = 0 ] || { echo "run as root"; exit 2; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
sed -n '/^webkit_usable() {/,/^}/p' "$S" | sed "s#/usr/bin/rudolf-webkit#$W/rudolf-webkit#" > $W/f.sh;   # (the program: a stand-in, made below)
grep -q 'webkit_usable' $W/f.sh || { echo "FAIL webkit_usable not in the session"; exit 1; }
unshare -m --propagation private sh -s "$W" <<'XEOF'
W=$1; P=0; F=0; ok() { if [ "$2" = "$3" ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 - got $2"; fi; }
mkdir -p /persist /run/rudolf; mount -t tmpfs t /persist; mount -t tmpfs t /run/rudolf
echo 'quiet' > $W/cmdline; RCMD=$W/cmdline
. $W/f.sh
t() { webkit_usable && echo yes || echo no; }
ok "1. nothing chosen: Chromium" "$(t)" no
mkdir -p /persist/etc; echo webkit > /persist/etc/rudolf-desktop-engine
ok "1b. chosen in System Settings but not taken at start-up: still Chromium until the computer restarts" "$(t)" no
cp /persist/etc/rudolf-desktop-engine /run/rudolf/engine-choice   # (what rudolf-init does at start-up)
ok "2. Lite chosen, but this Rudolf OS has no rudolf-webkit: Chromium" "$(t)" no
printf '#!/bin/sh\n' > $W/rudolf-webkit; chmod 755 $W/rudolf-webkit
ok "3. ...with rudolf-webkit in the system: WebKit (Linux and the encrypted folder play no part)" "$(t)" yes
touch /run/rudolf/webkit-failed; ok "5. ...but it failed this start: Chromium" "$(t)" no; rm /run/rudolf/webkit-failed
echo 'quiet rudolf.recovery=1' > $W/cmdline; ok "6. recovery mode: Chromium" "$(t)" no; echo quiet > $W/cmdline
echo chromium > /persist/etc/rudolf-desktop-engine; ok "7. Chromium chosen again: WebKit until the computer restarts" "$(t)" yes
echo chromium > /run/rudolf/engine-choice; ok "8. ...and Chromium after it" "$(t)" no
echo "$P/$((P+F))"; [ $F = 0 ]
XEOF
r=$?
# the pattern
m1=$(echo "/usr/bin/rudolf-webkit --rudolf-kiosk --size=1920x1080 http://127.0.0.1:8080/" | grep -cE '[-]-rudolf-kiosk'); m2=$(echo "/opt/chromium/chrome --rudolf-kiosk --class=RudolfDesktop" | grep -cE '[-]-rudolf-kiosk'); m3=$(echo "sh -c pkill -f '[-]-rudolf-kiosk'" | grep -cE '[-]-rudolf-kiosk')
if [ "$m1$m2$m3" = 110 ]; then echo "PASS 9. '[-]-rudolf-kiosk' matches either desktop and never its own text"; else echo "FAIL 9. pattern - $m1$m2$m3"; r=1; fi
grep -q "chrom\[e\].*--rudolf-kiosk" "$S" $HERE/../../sys/root/usr/lib/rudolf/shutdown $HERE/../../sys/rudolfd.c && { echo "FAIL 10. a Chromium-only desktop pattern is left"; r=1; } || echo "PASS 10. no Chromium-only desktop pattern is left (session, shutdown, rudolfd)"
grep -q 'cp /persist/etc/rudolf-desktop-engine /run/rudolf/engine-choice' $HERE/../../sys/root/sbin/rudolf-init && ! grep -q 'engine-switch' "$S" && echo "PASS 11. rudolf-init takes the choice at start-up; nothing switches the engine before a restart" || { echo "FAIL 11. start-up"; r=1; }
exit $r
