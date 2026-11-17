#!/bin/sh
# 1.6.0 (plan Part 12, "Becoming the default"): a new installation starts with the native desktop (install.sh writes the
# choice, on trial as any chosen engine), an update keeps the engine the computer had, and Settings offers Native once:
# rudolfd's desktop/engine "offer" (Native there, Standard in use by default, never given up, an installed Rudolf OS, not
# offered before) and POST offered=1 (and any choice made) keeping it as made.   sh nativedefault60_t.sh (root)
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
# (rudolfd with rudolf-native at a place of the test's: there or not)
sed "s#/usr/bin/rudolf-native#$W/rudolf-native#g" $SYS/rudolfd.c > $W/rudolfd.c
gcc -O1 -w -DNATIVE_OFFER=1 -I$SYS -o $W/rd -x c - -lz <<EOF || { echo "FAIL build rudolfd"; exit 1; }
#define main rudolfd_main
#include "$W/rudolfd.c"
#undef main
int main(int c, char **v) { struct req r; memset(&r, 0, sizeof r); snprintf(r.method, sizeof r.method, "%s", v[1]); snprintf(r.path, sizeof r.path, "/api/desktop/engine");
	if (c > 2) snprintf(r.query, sizeof r.query, "%s", v[2]); char tf[] = "/tmp/natdef60-XXXXXX"; r.fd = mkstemp(tf); unlink(tf); api(&r);
	lseek(r.fd, 0, SEEK_SET); char b[4096] = {0}; if (read(r.fd, b, sizeof b - 1) < 0) return 1; char *e = strstr(b, "\r\n\r\n"); printf("%.12s|%s\n", b + 9, e ? e + 4 : ""); return 0; }
EOF
unshare -m --propagation private sh -s "$W" "$SYS" <<'XEOF'
W=$1; SYS=$2; P=0; F=0; ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
mkdir -p /persist /run/rudolf; mount -t tmpfs t /persist; mount -t tmpfs t /run/rudolf; mkdir -p /persist/etc; touch /run/rudolf/persistent
printf '#!/bin/sh\n' > $W/rudolf-native; chmod 755 $W/rudolf-native
o=$($W/rd GET); ok "1. updated on Standard (no choice kept), Native there: offered" $(echo "$o" | grep -q '"choice":"chromium".*"nativeAvailable":true.*"offer":true}'; echo $?) "$o"
o=$($W/rd POST offered=1); ok "2. the offer kept as made (POST offered=1)" $(echo "$o" | head -1 | grep -q '^200' && [ -s /persist/etc/rudolf-native-offered ]; echo $?) "$o"
o=$($W/rd GET); ok "3. ...and not offered again" $(echo "$o" | grep -q '"offer":false}'; echo $?) "$o"
rm -f /persist/etc/rudolf-native-offered; o=$($W/rd POST engine=chromium); o2=$($W/rd GET)
ok "4. an engine chosen in Settings: Native not offered after it" $([ -s /persist/etc/rudolf-native-offered ] && echo "$o2" | grep -q '"offer":false}'; echo $?) "$o $o2"
rm -f /persist/etc/rudolf-native-offered /persist/etc/rudolf-desktop-engine; echo native > /persist/etc/rudolf-desktop-engine; o=$($W/rd GET)
ok "5. Native already chosen (a new installation): not offered" $(echo "$o" | grep -q '"choice":"native".*"offer":false}'; echo $?) "$o"
rm -f /persist/etc/rudolf-desktop-engine; echo "2026-11-14T10:00:00Z no answer from it in 90 s (Native)" > /persist/etc/rudolf-desktop-engine.failed; o=$($W/rd GET)
ok "6. Native given up on this computer: not offered" $(echo "$o" | grep -q '"offer":false}'; echo $?) "$o"
rm -f /persist/etc/rudolf-desktop-engine.failed; rm $W/rudolf-native; o=$($W/rd GET)
ok "7. a Rudolf OS without rudolf-native: not offered" $(echo "$o" | grep -q '"nativeAvailable":false.*"offer":false}'; echo $?) "$o"
printf '#!/bin/sh\n' > $W/rudolf-native; chmod 755 $W/rudolf-native; rm /run/rudolf/persistent; o=$($W/rd GET)
ok "8. not installed (the live disc): not offered" $(echo "$o" | grep -q '"offer":false}'; echo $?) "$o"
# install.sh's choice for a new installation (its own line, with the test's places)
L=$(grep -n 'rudolf-desktop-engine' $SYS/root/usr/lib/rudolf/install.sh | grep -v '^[0-9]*:#' | cut -d: -f2-)
[ -n "$L" ] || { echo "FAIL install.sh has no default engine"; exit 1; }
mkdir -p $W/new/etc $W/chrome; printf '#!/bin/sh\n' > $W/chrome/chrome; chmod 755 $W/chrome/chrome
run() { echo "$L" | sed "s#/usr/bin/rudolf-native#$W/rudolf-native#g; s#/opt/chromium/chrome#$W/chrome/chrome#g; s#/mnt/newdata#$W/new#g" > $W/l.sh; NATIVE_DEFAULT=${ND:-1} sh $W/l.sh; }
# (1.6.0: the default as shipped is Standard, NATIVE_DEFAULT=0 in install.sh; the mechanism, for 1.6.1, with 1)
ok "0. 1.6.0 ships Standard as the default: install.sh's NATIVE_DEFAULT is 0, rudolfd's NATIVE_OFFER 0" $(grep -qx 'NATIVE_DEFAULT=0' $SYS/root/usr/lib/rudolf/install.sh && grep -q '^#define NATIVE_OFFER 0$' $SYS/rudolfd.c; echo $?)
ND=0 run; ok "0b. ...a new installation: nothing written, Standard, nothing offered" $([ ! -e $W/new/etc/rudolf-desktop-engine ]; echo $?)
rm -f $W/new/etc/*; run; ok "9. a new installation starts with Native (the choice written, on trial at its first start; nothing offered)" $([ "$(cat $W/new/etc/rudolf-desktop-engine)" = native ] && [ -s $W/new/etc/rudolf-native-offered ]; echo $?)
rm -f $W/new/etc/*; rm $W/rudolf-native; run; ok "10. ...a Rudolf OS without rudolf-native: Standard (nothing written)" $([ ! -e $W/new/etc/rudolf-desktop-engine ]; echo $?)
printf '#!/bin/sh\n' > $W/rudolf-native; chmod 755 $W/rudolf-native; rm -f $W/new/etc/*; rm $W/chrome/chrome; run
ok "11. ...nor without Chromium (Lite is the only engine there, Standard the fallback is missing)" $([ ! -e $W/new/etc/rudolf-desktop-engine ]; echo $?)
echo "$P/$((P+F)) passed"; [ $F = 0 ]
XEOF
r=$?
# an update never writes the choice: only the installer does (rudolf-update keeps /persist as it is)
grep -q 'rudolf-desktop-engine' $SYS/rudolf-update.c && { echo "FAIL 12. rudolf-update touches the engine's choice"; r=1; } || echo "PASS 12. an update keeps the engine the computer had (rudolf-update never writes the choice)"
exit $r
