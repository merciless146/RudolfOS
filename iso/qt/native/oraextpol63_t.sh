#!/bin/sh
# 1.6.3 (docs/plan-1.6.3.md Part 7, step 2, first): what Chromium's extension policies do to the extensions Rudolf OS's desktop loads
# itself with --load-extension (Ruffle for Oracle, the web frames extension). A stand-in extension (a content script that asks the test's
# server for /ran, so nothing but the server is needed to see it run; its ID fixed by a public key made here, the private half deleted at
# once) loaded with --load-extension into a real Chromium, under a managed policy file of each kind, and the page's load watched:
#   none, neutral (a policy unrelated to extensions), block (ExtensionInstallBlocklist "*"), settings (ExtensionSettings "*" blocked),
#   allow ("*" blocked, its ID allowed), allowset (ExtensionSettings: "*" blocked, its ID allowed), sources (ExtensionInstallSources empty,
#   BlockExternalExtensions, the Web Store's addresses blocked).
# Prints FOUND lines and a screenshot's text when a dialog stops Chromium; PASS when the findings are as recorded in the plan (the day a
# Chromium release changes them, this fails with what changed). A writable policy folder, Xvfb (xvfb-run), python3, openssl, ImageMagick's import optional.
#   CHROME=... POLICY_DIR=... sh oraextpol63_t.sh     (Chromium for Testing: /etc/opt/chrome_for_testing/policies/managed; the snap and
#   Debian's Chromium: /etc/chromium/policies/managed). Port 8722.
CH=${CHROME:-$(ls -d /opt/pw-browsers/chromium-*/chrome-linux*/chrome 2>/dev/null | tail -1)}; PD=${POLICY_DIR:-/etc/opt/chrome_for_testing/policies/managed}
mkdir -p $PD 2>/dev/null; [ -w "$PD" ] || { echo "SKIP: the policy folder $PD is not writable (root, or made writable for the test)"; exit 0; }
[ -x "$CH" ] || [ -x "$(command -v "$CH")" ] || { echo "SKIP: no Chromium"; exit 0; }
T=$(mktemp -d /tmp/oraextpol.XXXXXX); mkdir -p $T/ext $PD; chmod 755 $T
case "$CH" in /snap/*) NOSB=''; [ "$(id -u)" = 0 ] && NOSB=--no-sandbox; H=$HOME; T2=$(mktemp -d $H/oraextpol.XXXXXX); cp -r $T/ext $T2/; T=$T2;; *) NOSB=--no-sandbox;; esac
trap 'rm -f $PD/oraextpol63.json; kill $S 2>/dev/null; rm -rf $T' EXIT
openssl genrsa -out $T/k.pem 2048 2>/dev/null && openssl rsa -in $T/k.pem -pubout -outform DER -out $T/k.der 2>/dev/null; rm -f $T/k.pem
KEY=$(base64 -w0 $T/k.der); ID=$(python3 -c "import hashlib,sys;h=hashlib.sha256(open(sys.argv[1],'rb').read()).hexdigest()[:32];print(''.join(chr(97+int(c,16)) for c in h))" $T/k.der)
printf '{"manifest_version":3,"key":"%s","name":"Stand-in","version":"1.0","content_scripts":[{"matches":["http://127.0.0.1/*"],"js":["c.js"],"run_at":"document_start"}]}\n' "$KEY" > $T/ext/manifest.json
echo 'fetch("/ran")' > $T/ext/c.js
python3 - $T/log <<'P' &
import http.server, sys
class H(http.server.BaseHTTPRequestHandler):
    def do_GET(s):
        open(sys.argv[1], 'a').write(s.path + '\n'); s.send_response(200); s.end_headers(); s.wfile.write(b'<p>x')
    def log_message(*a): pass
http.server.HTTPServer(('127.0.0.1', 8722), H).serve_forever()
P
S=$!; timeout 1 tail -f /dev/null
R=''
for c in none neutral block settings allow allowset sources; do
	rm -f $PD/oraextpol63.json $T/log
	case $c in
		neutral) echo '{"ShowHomeButton":true}';;
		block) echo '{"ExtensionInstallBlocklist":["*"]}';;
		settings) echo '{"ExtensionSettings":{"*":{"installation_mode":"blocked"}}}';;
		allow) echo "{\"ExtensionInstallBlocklist\":[\"*\"],\"ExtensionInstallAllowlist\":[\"$ID\"]}";;
		allowset) echo "{\"ExtensionSettings\":{\"*\":{\"installation_mode\":\"blocked\"},\"$ID\":{\"installation_mode\":\"allowed\"}}}";;
		sources) echo '{"ExtensionInstallSources":[],"BlockExternalExtensions":true,"URLBlocklist":["chromewebstore.google.com","chrome.google.com/webstore"]}';;
		*) echo '';;
	esac > $T/pol; [ -s $T/pol ] && cp $T/pol $PD/oraextpol63.json
	$CH --user-data-dir=$T/prof-$c --no-first-run --no-default-browser-check $NOSB --load-extension=$T/ext http://127.0.0.1:8722/page >$T/err-$c 2>&1 & C=$!
	timeout 12 tail -f /dev/null
	command -v import >/dev/null && import -window root $T/shot-$c.png 2>/dev/null
	kill $C 2>/dev/null; timeout 3 tail -f /dev/null; kill -9 $C 2>/dev/null
	p=$(grep -c '^/page' $T/log 2>/dev/null); r=$(grep -c '^/ran' $T/log 2>/dev/null); p=${p:-0}; r=${r:-0}
	echo "FOUND $c: the page loaded $p, the --load-extension's script ran $r"; R="$R $c=$p$r"
done
echo "FOUND $("$CH" --version 2>/dev/null)"
# as recorded (docs/plan-1.6.3.md Part 7, step 2): any policy that blocks extensions ("*", by either policy, even with the extension's own
# ID allowed) refuses every --load-extension one ("Loading of unpacked extensions is disabled by the administrator") with a dialog that
# stops Chromium's start; the install sources' policy and the Web Store's addresses blocked leave it running
want=" none=11 neutral=11 block=00 settings=00 allow=00 allowset=00 sources=11"
[ "$R" = "$want" ] && echo PASS || { echo "FAIL: found$R, recorded$want"; exit 1; }
