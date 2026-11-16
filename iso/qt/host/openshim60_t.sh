#!/bin/sh
# 1.6.0: rudolf-open (xdg-open, xdg-email, x-www-browser... for Debian's programs) on the host, with this tree's
# rudolf-portal on a private session bus: what the desktop is asked (rudolf-portal's requests in /run/rudolf/portal/req,
# here a tmpfs of a private mount namespace: the host's /run is not touched). As Debian's programs run it: as user 1000 of
# a user namespace that the bus outside knows by another number (Electron's shell.openExternal runs xdg-open, so Claude's
# "Continue with Google" did nothing: no browser in Debian); GIO's default for web links (gio open: GTK's gtk_show_uri)
# from the launcher rudolf-debian writes; $BROWSER (Python's webbrowser); a file; mail; an app's own scheme; refusals; and
# Debian's own xdg-open when the desktop cannot be reached.
#   sh openshim60_t.sh   (root: unshare; gcc, glib, dbus-run-session, gio, python3)
[ "$(id -u)" = 0 ] || { echo "run as root (a private mount namespace for /run/rudolf)"; exit 2; }
Q=$(cd "$(dirname "$0")/../.." && pwd); T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
. "$Q/systools/hardflags.sh"
gcc -O2 -Wall $HARD_STATIC -o "$T/rudolf-open" "$Q/sys/rudolf-open.c" || exit 1
gcc -O2 -w -o "$T/rudolf-portal" "$Q/sys/rudolf-portal.c" $(pkg-config --cflags --libs gio-2.0 gio-unix-2.0) -lcrypto || exit 1
mkdir -p "$T/bin" "$T/real" "$T/share/applications"
for n in xdg-open xdg-email x-www-browser sensible-browser www-browser; do ln -s "$T/rudolf-open" "$T/bin/$n"; done
# (rudolf-debian's launcher and default list for GIO, as it writes them, with this folder's xdg-open in it)
python3 - "$Q/sys/rudolf-debian.c" "$T/share/applications" "$T/bin/xdg-open" <<'EOF' || exit 1
import re, sys
s = open(sys.argv[1]).read()
for name in ('rudolf-open.desktop', 'mimeapps.list'):
    m = re.search(r'write_str\(ROOT OPEN_SHARE "/applications/' + re.escape(name) + r'", ((?:"(?:[^"\\]|\\.)*"\s*)+)\);', s)
    text = ''.join(re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1))).encode().decode('unicode_escape')
    open(sys.argv[2] + '/' + name, 'w').write(text.replace('/usr/local/bin/xdg-open', sys.argv[3]))
EOF
printf '#!/bin/sh\necho "debian-own-xdg-open $*" > %s/real.out\n' "$T" > "$T/real/xdg-open"; chmod 755 "$T/real/xdg-open"
cat > "$T/t.sh" <<'EOS'
P=0; N=0; ok() { N=$((N + 1)); if [ "$1" = 0 ]; then P=$((P + 1)); echo "PASS $2"; else echo "FAIL $2 - $3"; fi; }
R=/run/rudolf/portal/req
mkdir -p $R /run/rudolf/portal/resp
"$T/rudolf-portal" 2>"$T/portal.log" & PID=$!
for i in 1 2 3 4 5 6 7 8 9 10; do gdbus introspect --session -d org.freedesktop.portal.Desktop -o /org/freedesktop/portal/desktop >/dev/null 2>&1 && break; timeout 0.5 tail -f /dev/null; done
# the last request's kind and link (rudolf-portal's JSON, as the desktop gets it)
last() { f=$(ls -t $R/*.json 2>/dev/null | head -1); [ -n "$f" ] && python3 -c 'import json,sys;d=json.load(open(sys.argv[1]));print(d.get("kind"),d.get("uri"))' "$f"; }
count() { ls $R/*.json 2>/dev/null | wc -l; }
clear() { rm -f $R/*.json; }
B=$T/bin
clear; "$B/xdg-open" 'https://claude.ai/login?from=desktop&x=1'; r=$?; o=$(last)
[ $r = 0 ] && [ "$o" = "url https://claude.ai/login?from=desktop&x=1" ]; ok $? "xdg-open https://...: the desktop is asked to open it (OpenURI), exit 0" "$r $o"
clear; o=$(unshare -U --map-user=1000 --map-group=1000 sh -c 'id -u; "$0" https://accounts.example/o/oauth2/auth?client_id=c\&redirect_uri=claude%3A%2F%2Fdone; echo "rc=$?"' "$B/xdg-open" 2>&1); l=$(last)
echo "$o" | grep -q '^1000$' && echo "$o" | grep -q 'rc=0' && [ "$l" = "url https://accounts.example/o/oauth2/auth?client_id=c&redirect_uri=claude%3A%2F%2Fdone" ]; ok $? "...as user 1000 of a user namespace (Debian's), which the bus knows by another number" "$o / $l"
clear; "$B/xdg-open" 'teststand://done?code=X42'; o=$(last); [ "$o" = "url teststand://done?code=X42" ]; ok $? "an app's own scheme is handed over as it is (the desktop decides)" "$o"
clear; mkdir -p "$T/home/Documents"; echo hi > "$T/home/Documents/a b#1.txt"; (cd "$T/home" && "$B/xdg-open" 'Documents/a b#1.txt'); r=$?; o=$(last)
[ $r = 0 ] && [ "$o" = "url file://$T/home/Documents/a%20b%231.txt" ]; ok $? "a file (a relative name): as file:// with its whole path, escaped" "$r $o"
clear; "$B/xdg-open" "$T/no/such/file" 2>"$T/e"; r=$?; [ $r = 2 ] && [ "$(count)" = 0 ] && grep -q 'does not exist' "$T/e"; ok $? "a file that does not exist: exit 2, nothing asked" "$r $(cat $T/e)"
clear; "$B/xdg-open" "$(printf 'https://x.example/\nrm -rf /')" 2>"$T/e"; r=$?; [ $r = 1 ] && [ "$(count)" = 0 ]; ok $? "a link with a line break or control character: refused (exit 1), nothing asked" "$r $(cat $T/e)"
clear; "$B/xdg-open" a b 2>/dev/null; r=$?; [ $r = 1 ] && [ "$(count)" = 0 ]; ok $? "xdg-open takes one link: two are a usage error (exit 1)" "$r"
clear; "$B/xdg-email" --subject 'Hello there' --cc b@example.org --body 'Line & more' a@example.org; r=$?; o=$(last)
[ $r = 0 ] && [ "$o" = "url mailto:a@example.org?cc=b@example.org&subject=Hello%20there&body=Line%20%26%20more" ]; ok $? "xdg-email (Electron's mailto: links): a mailto: link for Mail" "$r $o"
clear; "$B/xdg-email" 'mailto:x@example.org?subject=Hi'; o=$(last); [ "$o" = "url mailto:x@example.org?subject=Hi" ]; ok $? "...a mailto: link as it is" "$o"
clear; "$B/x-www-browser" https://a.example/ https://b.example/; r=$?; [ $r = 0 ] && [ "$(count)" = 2 ]; ok $? "x-www-browser with two links: both asked" "$r $(count)"
clear; "$B/sensible-browser" https://c.example/; o=$(last); [ "$o" = "url https://c.example/" ]; ok $? "sensible-browser" "$o"
clear; BROWSER="$B/x-www-browser" python3 -c 'import webbrowser,sys; sys.exit(0 if webbrowser.open("https://py.example/doc") else 1)'; r=$?; o=$(last)
[ $r = 0 ] && [ "$o" = "url https://py.example/doc" ]; ok $? "\$BROWSER (Python's webbrowser): the desktop is asked" "$r $o"
# GIO (gtk_show_uri, gio open): with rudolf-debian's launcher first in XDG_DATA_DIRS and no other browser
clear; XDG_DATA_DIRS="$T/share:/nonexistent" XDG_CONFIG_HOME="$T/cfg" XDG_DATA_HOME="$T/data" XDG_CONFIG_DIRS="$T/nocfg" gio open https://gio.example/page 2>"$T/e"; r=$?
for i in 1 2 3 4 5 6 7 8 9 10; do [ "$(count)" != 0 ] && break; timeout 0.3 tail -f /dev/null; done; o=$(last)
[ $r = 0 ] && [ "$o" = "url https://gio.example/page" ]; ok $? "GIO (gtk_show_uri, gio open): rudolf-debian's launcher is the default for web links" "$r $o $(cat $T/e)"
clear; o=$(XDG_DATA_DIRS="$T/share:/nonexistent" XDG_CONFIG_HOME="$T/cfg" XDG_DATA_HOME="$T/data" XDG_CONFIG_DIRS="$T/nocfg" gio mime x-scheme-handler/mailto 2>&1); echo "$o" | grep -q 'rudolf-open.desktop'; ok $? "...and for mailto:" "$o"
# the desktop cannot be reached (Debian's root, nobody signed in)
clear; DBUS_SESSION_BUS_ADDRESS=unix:path=$T/nobus XDG_RUNTIME_DIR=$T/nort RUDOLF_OPEN_REAL=$T/none "$B/xdg-open" https://x.example/ 2>"$T/e"; r=$?
[ $r = 3 ] && grep -q 'session bus' "$T/e"; ok $? "no session bus: exit 3, said why" "$r $(cat $T/e)"
rm -f "$T/real.out"; DBUS_SESSION_BUS_ADDRESS=unix:path=$T/nobus RUDOLF_OPEN_REAL=$T/real "$B/xdg-open" https://y.example/ 2>/dev/null
grep -qx 'debian-own-xdg-open https://y.example/' "$T/real.out" 2>/dev/null; ok $? "...xdg-open then runs Debian's own, when it is installed" "$(cat $T/real.out 2>&1)"
# the portal cannot leave the question for the desktop (its folder not writable: an account's portal before 1.6.0's fix)
rm -rf $R; touch $R; "$B/xdg-open" https://lost.example/ 2>"$T/e"; r=$?; rm -f $R; mkdir -p $R
[ $r = 4 ] && grep -q 'could not be asked' "$T/e"; ok $? "the portal cannot leave the request for the desktop: said (exit 4), not lost without a word" "$r $(cat $T/e)"
clear; "$B/xdg-open" https://perm.example/; f=$(ls $R/*.json | head -1); [ "$(stat -c %a "$f")" = 600 ]; ok $? "a request is readable by its owner only (0600: the folder takes every account's portal's)" "$(stat -c %a "$f")"
kill $PID; wait $PID 2>/dev/null
clear; DBUS_SESSION_BUS_ADDRESS=$DBUS_SESSION_BUS_ADDRESS RUDOLF_OPEN_REAL=$T/none timeout 30 "$B/xdg-open" https://z.example/ 2>"$T/e"; r=$?
[ $r = 3 ] && grep -qi 'portal\|ServiceUnknown\|not provided\|activat' "$T/e"; ok $? "a bus without the portal: exit 3, at once" "$r $(cat $T/e)"
"$B/xdg-open" --version | grep -q 'rudolf-open'; ok $? "--version names it"
echo "$P/$N passed"; [ $P = $N ]
EOS
T="$T" unshare -m sh -c 'mount --make-rprivate / && mount -t tmpfs none /run && dbus-run-session -- sh "$0"' "$T/t.sh" > "$T/out" 2>&1
grep -v 'dbus-daemon\[' "$T/out"; tail -1 "$T/out" | grep -q '^\([0-9]*\)/\1 passed$'
