#!/bin/sh
# 1.5.8: rudolf-shellfix puts the system's shell into a Chromium profile that has an older copy: Chromium (157) never
# replaces one installed from its file ("App is already installed"), so changes to Oracle's tabs never arrived.
#   CHROME=/path/to/chrome sh shellfix58_t.sh     (needs Xvfb, python3 with cryptography; Chromium 157 as the releases
#   ship is the one that matters: its layer, chromium.sfs, unpacked)
HERE=$(cd "$(dirname "$0")" && pwd); ISO=$HERE/../..; W=$(mktemp -d); P=0; F=0
trap 'pkill -f "Xvfb :93" 2>/dev/null; rm -rf "$W"' EXIT
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
[ -x "$CHROME" ] || { echo "SKIP (set CHROME to a Chromium)"; exit 0; }
command -v Xvfb >/dev/null || { echo "SKIP (no Xvfb)"; exit 0; }
gcc -O2 -o $W/fix $ISO/sys/rudolf-shellfix.c || { echo "FAIL build"; exit 1; }
python3 -c "
from cryptography.hazmat.primitives.asymmetric import ed25519;from cryptography.hazmat.primitives import serialization as s
open('$W/k.pem','wb').write(ed25519.Ed25519PrivateKey.generate().private_bytes(s.Encoding.PEM,s.PrivateFormat.PKCS8,s.NoEncryption()))"
python3 $ISO/systools/mkiwa.py derive $W/k.pem $W/iwa.key > $W/id; ID=$(cat $W/id)
for v in 1.0.0 2.0.0; do cp -a $ISO/shell $W/s$v; python3 - $W/s$v/.well-known/manifest.webmanifest $v <<'PY'
import json,sys;p,v=sys.argv[1:];m=json.load(open(p));m['version']=v;open(p,'w').write(json.dumps(m))
PY
python3 $ISO/systools/mkiwa.py build $W/s$v $W/$v.swbn $W/iwa.key >/dev/null; done
Xvfb :93 -screen 0 1280x800x24 >/dev/null 2>&1 & export DISPLAY=:93; sleep 2
NS=""; [ "$(id -u)" = 0 ] && NS=--no-sandbox
inst() { for f in $W/prof/Default/iwa/*/main.swbn; do [ -f "$f" ] && cmp -s "$f" "$1" && return 0; done; return 1; }
timeout 20 "$CHROME" $NS --user-data-dir=$W/prof --no-first-run --enable-features=IsolatedWebApps,IsolatedWebAppDevMode --install-isolated-web-app-from-file=$W/1.0.0.swbn about:blank >/dev/null 2>&1
inst $W/1.0.0.swbn; ok "1. the old shell installed from its file, as the session does" $?
timeout 20 "$CHROME" $NS --user-data-dir=$W/prof --no-first-run --enable-features=IsolatedWebApps,IsolatedWebAppDevMode --install-isolated-web-app-from-file=$W/2.0.0.swbn about:blank >/dev/null 2>&1
inst $W/1.0.0.swbn && echo "     (this Chromium keeps the old copy when started with the new file: the problem)"
timeout 100 $W/fix "$CHROME" $W/prof $W/2.0.0.swbn $ID $NS >$W/log 2>&1; r=$?
ok "2. rudolf-shellfix: the new shell in the profile (exit $r)" $(inst $W/2.0.0.swbn && [ $r = 0 ]; echo $?) "$(grep shellfix $W/log | tail -2)"
timeout 100 $W/fix "$CHROME" $W/prof $W/2.0.0.swbn $ID $NS >$W/log 2>&1; ok "3. again (already current): still fine" $(inst $W/2.0.0.swbn && [ $? = 0 ]; echo $?)
timeout 100 $W/fix "$CHROME" $W/prof /bad/../x $ID $NS >/dev/null 2>&1; ok "4. a bad bundle path: refused (2)" $([ $? = 2 ]; echo $?)
grep -q 'rudolf-shellfix' $ISO/sys/root/usr/lib/rudolf/session; ok "5. the session runs it when the profile's copy differs" $?
echo "$P/$((P+F)) passed"; [ $F = 0 ]
