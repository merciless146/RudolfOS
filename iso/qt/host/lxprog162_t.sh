#!/bin/sh
# 1.6.2 (docs/plan-1.6.2.md Part 25b): Linux apps' progress on their taskbar buttons, through the real listener. A stand-in app
# (fixtures/lpsend.c) says its progress with the Unity LauncherEntry signal on a private session bus (dbus-daemon started here,
# as the account's own user); the account's own rudolf-portal hears it and leaves its note; rudolfd's own portal/progress
# (fixtures/lxprogapi.c, rudolfd.c compiled) passes it on. Checked: the percentage, its changes, hidden, the app leaving the
# bus; malformed signals (a wrong signature, a link that is not application://ID.desktop, an ID with other characters, a
# progress that is a string, not finite); Flatpak's permissions (a stand-in sandbox's /.flatpak-info: an app allowed to talk to
# com.canonical.Unity, one allowed the whole bus, one not allowed, one naming another app's desktop file); another account's
# portal and app (its own bus): never listed, its note left alone; forged notes (no portal running, another program's pid, not
# 0600, a link, other characters, a number over 100); StartupWMClass from Flatpak's exported desktop file; a portal started
# again clears its user's old notes. Then, with the stand-in app at 42 %: Standard's page (lxprog62_t.js, Playwright: its
# window's button fills) and the native desktop (rudolf-native on Xvfb, a stand-in window: its button fills while minimized,
# by the same rule), and Standard's fill against Native's (the same fraction, the same colour). Needs root (a private /run/rudolf,
# two test users), gcc, GLib, dbus-daemon; the page part Playwright; the native part Xvfb, xdotool, ImageMagick, WebKitGTK.
#   sh lxprog162_t.sh            (LXP_NO_PAGE=1, LXP_NO_NATIVE=1 leave those parts out)
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$LXP_INNER" ] && exec env LXP_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; T=$(mktemp -d /tmp/lxprog162.XXXXXX); chmod 755 $T
mount --make-rprivate / 2>/dev/null; mount -t tmpfs none /run; mkdir -p /run/rudolf/portal/req /run/rudolf/portal/resp; chmod 1733 /run/rudolf/portal/req /run/rudolf/portal/resp
REQ=/run/rudolf/portal/req; MADE=""
for U in lxpann lxpbob; do id $U >/dev/null 2>&1 || { useradd -M -s /usr/sbin/nologin $U 2>/dev/null && MADE="$MADE $U"; }; done
id lxpann >/dev/null 2>&1 && id lxpbob >/dev/null 2>&1 || { echo "SKIP: could not make the test users"; exit 0; }
UA=$(id -u lxpann); UB=$(id -u lxpbob)
PIDS=""
cleanup() { for p in $PIDS; do kill $p 2>/dev/null; done; for p in $(cat $T/kill.* 2>/dev/null); do kill $p 2>/dev/null; done; timeout 0.5 tail -f /dev/null; for U in $MADE; do userdel -f $U 2>/dev/null; done; rm -rf $T; }
trap cleanup EXIT
CF="$(pkg-config --cflags --libs gio-2.0 gio-unix-2.0)"
mkdir -p $T/bin $T/exports; chmod 755 $T/bin
gcc -O2 -w -o $T/bin/rudolf-portal $SYS/rudolf-portal.c $CF -lcrypto && gcc -O2 -w -o $T/bin/lpsend $HERE/fixtures/lpsend.c $CF \
	&& gcc -O1 -w -DLXP_EXPORTS="\"$T/exports\"" -o $T/bin/lxprogapi $HERE/fixtures/lxprogapi.c -lz || { echo "FAIL build"; exit 1; }
printf '[Desktop Entry]\nName=Chrome\nStartupWMClass=google-chrome\n[Desktop Action new]\nStartupWMClass=wrong\n' > $T/exports/com.google.Chrome.desktop
printf '[Desktop Entry]\nName=Odd\nStartupWMClass=<b>odd</b>\n' > $T/exports/org.example.Odd.desktop
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
w8() { i=0; while [ $i -lt ${2:-40} ]; do eval "$1" && return 0; timeout 0.1 tail -f /dev/null; i=$((i+1)); done; return 1; }
api() { $T/bin/lxprogapi $UA; }
# each account's own session bus, as rudolfd starts it (its user's)
bus() { U=$1; mkdir -p $T/h-$U; chown $U $T/h-$U
	setpriv --reuid=$U --regid=$(id -g $U) --clear-groups env HOME=$T/h-$U dbus-daemon --session --fork --print-address=1 --print-pid=1 > $T/bus-$U
	sed -n 2p $T/bus-$U >> $T/kill.bus; sed -n 1p $T/bus-$U; }
BA=$(bus lxpann); BB=$(bus lxpbob)
[ -n "$BA" ] && [ -n "$BB" ] || { echo "FAIL the test buses did not start"; exit 1; }
asu() { U=$1; A=$2; shift 2; setpriv --reuid=$U --regid=$(id -g $U) --clear-groups env HOME=$T/h-$U DBUS_SESSION_BUS_ADDRESS=$A "$@"; }
portal() { setpriv --reuid=$1 --regid=$(id -g $1) --clear-groups env HOME=$T/h-$1 DBUS_SESSION_BUS_ADDRESS=$2 $T/bin/rudolf-portal 2>>$T/portal-$1.log & echo $! > $T/kill.portal-$1
	w8 "asu $1 $2 gdbus introspect --session -d org.freedesktop.portal.Desktop -o /org/freedesktop/portal/desktop >/dev/null 2>&1" 80; }
portal lxpann "$BA"; portal lxpbob "$BB"
# a stand-in app, held on the bus (commands through its fifo); FLATPAK-INFO: in a stand-in Flatpak sandbox (its own root)
hold() { K=$1; U=$2; A=$3; FI=$4; rm -f $T/f$K; mkfifo $T/f$K; chmod 666 $T/f$K
	if [ -z "$FI" ]; then ( asu $U $A $T/bin/lpsend < $T/f$K > $T/o$K 2>&1 ) &
	else R=$T/root$K; mkdir -p $R; ( unshare -m sh -c "mount --make-rprivate /; mount -t tmpfs none $R; for d in usr etc tmp proc dev run var; do mkdir $R/\$d; mount --rbind /\$d $R/\$d; done
		for d in bin sbin lib lib64 lib32 libx32; do if [ -L /\$d ]; then cp -a /\$d $R/\$d; elif [ -d /\$d ]; then mkdir $R/\$d; mount --rbind /\$d $R/\$d; fi; done
		cp $FI $R/.flatpak-info; exec chroot $R setpriv --reuid=$U --regid=$(id -g $U) --clear-groups env HOME=$T/h-$U DBUS_SESSION_BUS_ADDRESS=$A $T/bin/lpsend" < $T/f$K > $T/o$K 2>&1 ) & fi
	timeout 600 tail -f /dev/null > $T/f$K & echo $! > $T/kill.w$K
	w8 "grep -q '^ready' $T/o$K 2>/dev/null" 50; }
say() { echo "$2" > $T/f$1; n=$(grep -c '^sent' $T/o$1); timeout 0.4 tail -f /dev/null; }
bye() { echo q > $T/f$1; timeout 0.4 tail -f /dev/null; kill $(cat $T/kill.w$1) 2>/dev/null; rm -f $T/kill.w$1; timeout 0.4 tail -f /dev/null; }
notes() { ls $REQ | grep -c '^lp-'; }
U1='application://org.example.Fetch.desktop'
hold 1 lxpann "$BA"
o=$(api); ok "0. nothing in progress: portal/progress lists nothing" "$([ "$o" = '{"ok":true,"apps":[],"windows":[]}' ] && echo 1)" "$o"
say 1 "u $U1 0.42 1"; o=$(api); f=$(ls $REQ | grep '^lp-')
ok "1. an app's LauncherEntry Update (progress 0.42, progress-visible true): org.example.Fetch at 42 %" "$([ "$o" = '{"ok":true,"apps":[{"id":"org.example.Fetch","wmclass":"","v":42}],"windows":[]}' ] && echo 1)" "$o $(cat $T/portal-lxpann.log)"
ok "...the note: lp-UID-PORTALPID-N.prog, its user's, 0600, ID and percentage only" "$(echo "$f" | grep -qE "^lp-$UA-$(cat $T/kill.portal-lxpann)-[0-9]+\.prog$" && [ "$(stat -c '%u %a' $REQ/$f)" = "$UA 600" ] && [ "$(cat $REQ/$f)" = "$(printf 'org.example.Fetch\t42')" ] && echo 1)" "$f $(ls -l $REQ)"
say 1 "u $U1 0.756 -"; o=$(api); ok "2. progress only (Unity: the other properties as they were): 76 %" "$(echo "$o" | grep -q '"v":76}' && echo 1)" "$o"
say 1 "u $U1 - 0"; o=$(api); ok "3. progress-visible false: gone, and its note" "$([ "$o" = '{"ok":true,"apps":[],"windows":[]}' ] && [ "$(notes)" = 0 ] && echo 1)" "$o $(ls $REQ)"
say 1 "u $U1 1.7 1"; o=$(api); ok "4. a progress over 1: 100 %" "$(echo "$o" | grep -q '"v":100}' && echo 1)" "$o"
say 1 "u $U1 -0.5 1"; o=$(api); ok "...under 0: 0 %" "$(echo "$o" | grep -q '"v":0}' && echo 1)" "$o"
say 1 "u $U1 0.3 1"; bye 1; o=$(api); ok "5. the app leaves the bus (it ended, or crashed): its progress gone" "$([ "$o" = '{"ok":true,"apps":[],"windows":[]}' ] && [ "$(notes)" = 0 ] && echo 1)" "$o $(ls $REQ)"
hold 2 lxpann "$BA"
for s in "one $U1" "ps $U1 half" "vs $U1 yes" "u $U1 nan 1" "u $U1 inf 1" "u application://org.example.Fetch 0.5 1" "u https://example.org/x.desktop 0.5 1" \
	"u application://../etc/passwd.desktop 0.5 1" "u application://a/b.desktop 0.5 1" "u application://.hidden.desktop 0.5 1" "u application://%3Cimg%3E.desktop 0.5 1" "u application://.desktop 0.5 1" "u $U1 - -"; do say 2 "$s"; done
gdbus emit --session --object-path /x --signal com.canonical.Unity.LauncherEntry.Update "'$U1'" "{'progress': <'0.5'>, 'progress-visible': <true>}" >/dev/null 2>&1   # (root's: not on the account's bus at all)
asu lxpann "$BA" gdbus emit --session --object-path /x --signal com.canonical.Unity.LauncherEntry.Update "'$U1'" "{'progress': <0.5>, 'progress-visible': <'true'>}" >/dev/null 2>&1
asu lxpann "$BA" gdbus emit --session --object-path /x --signal com.canonical.Unity.LauncherEntry.Update "'$U1'" "<0.5>" >/dev/null 2>&1
timeout 0.5 tail -f /dev/null; o=$(api)
ok "6. malformed signals ignored (a wrong signature, a link that is not application://ID.desktop, .., /, a leading dot, other characters, a progress or progress-visible of the wrong type, not finite, neither given)" "$([ "$o" = '{"ok":true,"apps":[],"windows":[]}' ] && [ "$(notes)" = 0 ] && [ "$(grep -c '^sent' $T/o2)" -ge 13 ] && echo 1)" "$o $(ls $REQ) $(cat $T/o2)"
ok "...and the portal still runs" "$(kill -0 $(cat $T/kill.portal-lxpann) 2>/dev/null && echo 1)" "$(tail -5 $T/portal-lxpann.log)"
bye 2
# Flatpak's permissions (a stand-in sandbox: its /.flatpak-info, as Flatpak writes it)
fi_() { printf '[Application]\nname=%s\nruntime=runtime/org.freedesktop.Platform/x86_64/24.08\n\n[Context]\nsockets=x11;wayland;pulseaudio;%s\n\n[Session Bus Policy]\n%s\n' "$1" "$2" "$3" > $T/fi-$4; }
fi_ org.example.Fetch "" "com.canonical.Unity=talk
org.freedesktop.Notifications=talk" ok; fi_ org.example.Fetch "" "org.freedesktop.Notifications=talk" no; fi_ org.example.Fetch "session-bus;" "" whole; fi_ org.example.Fetch "" "com.canonical.Unity=see" see
if unshare -m true 2>/dev/null && command -v chroot >/dev/null; then
	hold 3 lxpann "$BA" $T/fi-ok; say 3 "u $U1 0.5 1"; o=$(api)
	ok "7. a Flatpak app allowed to talk to com.canonical.Unity, its own desktop file: shown (50 %)" "$(echo "$o" | grep -q '"id":"org.example.Fetch","wmclass":"","v":50}' && echo 1)" "$o $(cat $T/o3) $(tail -3 $T/portal-lxpann.log)"
	say 3 "u application://org.example.Fetch.Helper.desktop 0.2 1"; say 3 "u application://org.mozilla.firefox.desktop 0.9 1"; say 3 "u application://org.example.FetchX.desktop 0.9 1"; o=$(api)
	ok "...one of its own (org.example.Fetch.Helper: shown); another app's (org.mozilla.firefox, org.example.FetchX): ignored" "$(echo "$o" | grep -q '"id":"org.example.Fetch.Helper","wmclass":"","v":20}' && ! echo "$o" | grep -q 'firefox\|FetchX' && grep -q 'org.mozilla.firefox from .* ignored (another app' $T/portal-lxpann.log && echo 1)" "$o"
	bye 3; o=$(api); ok "...it leaves: gone" "$([ "$o" = '{"ok":true,"apps":[],"windows":[]}' ] && echo 1)" "$o"
	hold 4 lxpann "$BA" $T/fi-no; say 4 "u $U1 0.5 1"; o=$(api)
	ok "8. a Flatpak app not allowed to talk to com.canonical.Unity: ignored" "$([ "$o" = '{"ok":true,"apps":[],"windows":[]}' ] && grep -q 'org.example.Fetch may not talk to com.canonical.Unity' $T/portal-lxpann.log && echo 1)" "$o $(tail -3 $T/portal-lxpann.log)"
	bye 4; hold 5 lxpann "$BA" $T/fi-see; say 5 "u $U1 0.5 1"; o=$(api); ok "...one that may only see it: ignored" "$([ "$o" = '{"ok":true,"apps":[],"windows":[]}' ] && echo 1)" "$o"; bye 5
	hold 6 lxpann "$BA" $T/fi-whole; say 6 "u $U1 0.33 1"; o=$(api); ok "9. a Flatpak app given the whole session bus: shown (33 %)" "$(echo "$o" | grep -q '"v":33}' && echo 1)" "$o"; bye 6
else echo "SKIP 7-9: no mount namespace and chroot for a stand-in Flatpak sandbox"; fi
# another account: its own bus and portal, its own app
hold 7 lxpbob "$BB"; say 7 "u $U1 0.9 1"; hold 8 lxpann "$BA"; say 8 "u application://org.example.Mine.desktop 0.1 1"; o=$(api)
ok "10. another account's app (its own bus, its own portal, a real note): never listed; the signed-in account's is" "$([ "$o" = '{"ok":true,"apps":[{"id":"org.example.Mine","wmclass":"","v":10}],"windows":[]}' ] && ls -n $REQ | grep -q " $UB .*lp-$UB-" && echo 1)" "$o $(ls -ln $REQ)"
ok "...its note left alone (its portal runs: it is that account's)" "$(ls $REQ | grep -q "^lp-$UB-" && $T/bin/lxprogapi $UB | grep -q '"v":90}' && echo 1)" "$(ls $REQ)"
ok "...the account's bus is its own: the other account cannot even connect to it" "$(! asu lxpbob "$BA" gdbus emit --session --object-path /x --signal com.canonical.Unity.LauncherEntry.Update "'$U1'" "{'progress': <0.5>, 'progress-visible': <true>}" >/dev/null 2>&1 && echo 1)"
bye 7
# forged notes
PA=$(cat $T/kill.portal-lxpann); timeout 600 tail -f /dev/null & SL=$!; echo $SL > $T/kill.sl
mk() { printf "$2" > $REQ/$1; chown $UA $REQ/$1; chmod ${3:-600} $REQ/$1; }
mk lp-$UA-999999-1.prog 'org.example.Gone\t50\n'; mk lp-$UA-$SL-1.prog 'org.example.Sleep\t50\n'; mk lp-$UA-$PA-900.prog 'org.example.Open\t50\n' 644
mk lp-$UA-$PA-901.prog 'org.example.Big\t101\n'; mk lp-$UA-$PA-902.prog '<img src=x>\t50\n'; mk lp-$UA-$PA-903.prog 'org.example.Two\t5\nx\t6\n'; mk lp-$UA-$PA-904.prog 'org.example.Neg\t-5\n'
mk lp-$UB-$PA-905.prog 'org.example.Owner\t50\n'; ln -s /etc/hostname $REQ/lp-$UA-$PA-906.prog
mk lp-$UA-$PA-907.prog 'com.google.Chrome\t30\n'; mk lp-$UA-$PA-908.prog 'org.example.Odd\t40\n'; mk lp-$UA-$PA-909.prog 'org.example.Mine\t30\n'
o=$(api)
ok "11. forged notes ignored: its portal not running (removed), another program's pid (removed), not 0600, over 100, other characters, two lines, negative, named for another user, a link" "$(! echo "$o" | grep -q 'Gone\|Sleep\|Open\|Big\|img\|Two\|Neg\|Owner' && [ ! -e $REQ/lp-$UA-999999-1.prog ] && [ ! -e $REQ/lp-$UA-$SL-1.prog ] && echo 1)" "$o $(ls $REQ)"
ok "12. StartupWMClass from Flatpak's exported desktop file (its [Desktop Entry]'s only; one of other characters left out)" "$(echo "$o" | grep -q '{"id":"com.google.Chrome","wmclass":"google-chrome","v":30}' && echo "$o" | grep -q '{"id":"org.example.Odd","wmclass":"","v":40}' && echo 1)" "$o"
ok "13. the same app from two of its programs: their average (10 and 30: 20)" "$(echo "$o" | grep -q '{"id":"org.example.Mine","wmclass":"","v":20}' && echo 1)" "$o"
kill $SL 2>/dev/null; rm -f $REQ/lp-$UA-$PA-9* $REQ/lp-$UB-$PA-905.prog
ok "14. rudolfd's portal/poll does not list the notes (they are not questions)" "$(cd $SYS && grep -q 'strcmp(e->d_name + l - 5, \".json\")' rudolfd.c && echo 1)"
say 8 "u application://org.example.Mine.desktop 0.15 1"; bye 8; hold 8 lxpann "$BA"; say 8 "u application://org.example.Mine.desktop 0.25 1"
kill -9 $PA; wait $PA 2>/dev/null; rm -f $T/kill.portal-lxpann; had=$(ls $REQ | grep -c "^lp-$UA-$PA-"); o=$(api)
ok "15. the account's portal ends (killed, nothing cleaned up): its notes no longer listed, and removed" "$([ "$had" = 1 ] && [ "$o" = '{"ok":true,"apps":[],"windows":[]}' ] && ! ls $REQ | grep -q "^lp-$UA-$PA-" && echo 1)" "$had $o $(ls -ln $REQ)"
bye 8; portal lxpann "$BA"
# ---- the page and the native desktop, with the stand-in app at 42 %
hold 9 lxpann "$BA"; say 9 "u $U1 0.42 1"
ok "16. the stand-in app at 42 % for the desktops" "$(api | grep -q '"id":"org.example.Fetch","wmclass":"","v":42}' && echo 1)" "$(api)"
# ---- an app window's own progress on Standard: the XApp hint (_NET_WM_XAPP_PROGRESS, as Firefox sets it on X11), read by rudolf-display
if [ -z "$LXP_NO_NATIVE" ] && command -v Xvfb >/dev/null && pkg-config --exists x11 xtst xfixes xrandr; then
	gcc -O2 -w -o $T/bin/rdisp $SYS/rudolf-display.c -I$SYS -lX11 -lXtst -lXfixes -lXrandr -lm && gcc -O2 -o $T/bin/xc $HERE/fixtures/xwmclient.c -lX11 && gcc -O2 -o $T/bin/lxwin $HERE/fixtures/lxwin.c -lX11 || { echo "FAIL build rudolf-display"; exit 1; }
	XD=${LXP_SDSP:-:84}; Xvfb $XD -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & echo $! > $T/kill.sxv
	w8 "DISPLAY=$XD $T/bin/xc getfocus >/dev/null 2>&1" 80; echo "bottom 48" > /run/rudolf/strut; echo desktop > /run/rudolf/wm
	DISPLAY=$XD $T/bin/xc desktop > $T/desk.log & echo $! > $T/kill.sdk; timeout 0.4 tail -f /dev/null
	DISPLAY=$XD $T/bin/rdisp --input > $T/rdisp.log 2>&1 & echo $! > $T/kill.rdisp; timeout 1 tail -f /dev/null
	rm -f $T/fx; mkfifo $T/fx; DISPLAY=$XD $T/bin/lxwin firefox Navigator "Downloads - Firefox" < $T/fx > $T/fx.out & echo $! > $T/kill.fxw
	timeout 600 tail -f /dev/null > $T/fx & echo $! > $T/kill.fxh
	w8 "grep -q '^id' $T/fx.out 2>/dev/null" 50; WF=$(sed -n 's/^id 0x//p' $T/fx.out); w8 "grep -q '^$WF' /run/rudolf/windows" 40
	ok "S1. rudolf-display lists the stand-in Firefox window; no progress yet" "$(grep -q "^$WF" /run/rudolf/windows && ! grep -q "^$WF" /run/rudolf/winprog 2>/dev/null && echo 1)" "$(cat /run/rudolf/windows /run/rudolf/winprog 2>&1)"
	echo "p 64" > $T/fx; w8 "grep -q '^$WF	64$' /run/rudolf/winprog" 40; o=$(api)
	ok "S2. the window sets _NET_WM_XAPP_PROGRESS 64: rudolf-display's winprog, rudolfd's portal/progress windows: that window at 64 %" "$(echo "$o" | grep -q "\"windows\":\[{\"win\":\"$WF\",\"v\":64}\]" && echo 1)" "$o $(cat /run/rudolf/winprog 2>&1)"
	echo "p 0" > $T/fx; w8 "! grep -q '^$WF' /run/rudolf/winprog" 40; o=$(api)
	ok "S3. 0 (Firefox's no progress): gone" "$(echo "$o" | grep -q '"windows":\[\]' && echo 1)" "$o"
	echo "p 30" > $T/fx; w8 "grep -q '^$WF	30$' /run/rudolf/winprog" 40; echo d > $T/fx; w8 "! grep -q '^$WF' /run/rudolf/winprog" 40
	ok "S4. the hint removed: gone" "$(! grep -q "^$WF" /run/rudolf/winprog && echo 1)" "$(cat /run/rudolf/winprog)"
	for k in kill.fxh kill.fxw kill.rdisp kill.sdk kill.sxv; do kill $(cat $T/$k) 2>/dev/null; rm -f $T/$k; done
	printf 'zz\t50\n1e00001\t101\n1e00002\t0\n1e00003\t5x\n1e00004\t77\n<b>\t5\n1e00005 9\n' > /run/rudolf/winprog; o=$(api)
	ok "S5. rudolfd passes on only a window ID in hex and 1-100" "$(echo "$o" | grep -q '"windows":\[{"win":"1e00004","v":77}\]}$' && echo 1)" "$o"
	rm -f /run/rudolf/winprog
else echo "SKIP S1-S5: no Xvfb or X libraries for rudolf-display"; fi
if [ -z "$LXP_NO_PAGE" ] && node -e "require('playwright')" 2>/dev/null; then
	echo > $T/step; LXP_API="$T/bin/lxprogapi	$UA" LXP_OUT=$T/std.json LXP_STEP=$T/step LXP_PORT=8611 node $HERE/../shell2/lxprog62_t.js > $T/page.out 2>&1 & PG=$!; echo $PG > $T/kill.pg
	w8 "grep -q measured $T/step" 400; say 9 "u $U1 - 0"; echo done > $T/step; wait $PG; r=$?; rm -f $T/kill.pg; sed 's/^/  page: /' $T/page.out
	ok "17. Standard (Playwright, the stand-in rudolfd answering with rudolfd's own portal/progress): its window's button fills to 42 %, then goes" "$([ $r = 0 ] && echo 1)" "$(tail -3 $T/page.out)"
	say 9 "u $U1 0.42 1"
else echo "SKIP 17: no Playwright"; fi
if [ -z "$LXP_NO_NATIVE" ] && command -v Xvfb >/dev/null && command -v xdotool >/dev/null && command -v import >/dev/null && pkg-config --exists webkit2gtk-4.1; then
	NB=$T/bin/rudolf-native; gcc -O2 -w -o $NB $SYS/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/ncc && gcc -O2 -o $T/bin/lxwin $HERE/fixtures/lxwin.c -lX11 || { cat $T/ncc; echo "FAIL build native"; exit 1; }
	D=$T/d; mkdir -p $D; node $HERE/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
	PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)"); printf '%s\t%s\n' $T/bin/lxprogapi $UA > $D/lxprog-fix
	printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","b2":"#000080","tb":"bottom","accent":"#2563eb"},"pins":["notepad"],"notes":0,"wallpaper":false}' > $D/settings.json
	PORT=${LXP_NPORT:-8612}; DSP=${LXP_DSP:-:83}
	python3 $HERE/fixtures/native_rd.py $PORT $D > $T/rd.log 2>&1 & echo $! > $T/kill.rd
	Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & echo $! > $T/kill.xv
	export DISPLAY=$DSP LIBGL_ALWAYS_SOFTWARE=1; w8 'xdotool getmouselocation >/dev/null 2>&1' 80
	(cd $D && HOME=$D RUDOLF_NATIVE_STATE=$D/state exec $NB --size=1280x800 --draw=soft --base=http://127.0.0.1:$PORT > $T/native.log 2>&1) & echo $! > $T/kill.np
	st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
	w8 '[ "$(st signin)" = 1 ]' 600 || { echo "FAIL the native sign-in screen did not come: $(tail $T/native.log)"; exit 1; }
	LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return; w8 '[ "$(st user)" = Ann ]' 600
	ok "18. the native desktop signed in" "$([ "$(st user)" = Ann ] && echo 1)" "$(head -3 $D/state)"
	$T/bin/lxwin Fetch fetch "Fetch - downloads" > $T/w1.out & echo $! > $T/kill.w1; $T/bin/lxwin Other other "Another app" > $T/w3.out & echo $! > $T/kill.w3
	w8 "grep -q '^id' $T/w1.out && grep -q '^id' $T/w3.out" 50; W1=$(sed -n 's/^id 0x//p' $T/w1.out); W3=$(sed -n 's/^id 0x//p' $T/w3.out)
	w8 "grep -q '^client $W1 ' $D/state && grep -q '^client $W3 ' $D/state" 80; timeout 2.5 tail -f /dev/null
	# the fill's width on a button (from its first to its last pixel of the fill's green on the button's middle row: the title is drawn over it), from a screenshot
	fillw() { import -window root $T/shot.png 2>/dev/null; set -- $(grep "^hit win $1 " $D/state | head -1 | cut -d' ' -f4-7)
		convert $T/shot.png -crop "$(( $3 - 6 ))x1+$(( $1 + 3 ))+$(( $2 + $4 / 2 ))" txt:- 2>/dev/null | python3 -c "
import sys
xs = []
for l in sys.stdin.read().splitlines()[1:]:
    h = l.split('#')[1][:6]; r, g, b = (int(h[i:i + 2], 16) for i in (0, 2, 4))
    if g > r + 40 and g > b + 25: xs.append(int(l.split(',')[0]))
print(max(xs) - min(xs) + 1 if xs else 0, $3)"; }
	f0=$(fillw $W1)
	ok "19. Native: the window shown (not minimized): no fill drawn, though its app is at 42 % (rudolfd asked; kept for when it is minimized)" "$([ "${f0%% *}" = 0 ] && grep -q '"v":42' $D/lxprog.log && grep -q "^hit win $W1 " $D/state && echo 1)" "$f0 $(grep "$W1" $D/state) $(tail -c 300 $D/lxprog.log)"
	xdotool windowminimize $W1 2>/dev/null; w8 "grep -q '^client $W1 .* min=1 ' $D/state" 20
	for k in 1 2 3; do grep -q "^client $W1 .* min=1 " $D/state && break; set -- $(grep "^hit win $W1 " $D/state | head -1 | cut -d' ' -f4-7)   # (its taskbar button: in front, then minimized)
		xdotool mousemove $(( $1 + $3 / 2 )) $(( $2 + $4 / 2 )) click 1; w8 "grep -q '^client $W1 .* min=1 ' $D/state" 15; done
	w8 "grep -q '^clientprog $W1 1 42' $D/state" 40; timeout 1 tail -f /dev/null
	ok "20. minimized: its button's fill at 42 % (tbp_set: a percentage, as Rudolf's own windows')" "$(grep -q "^client $W1 .* min=1 " $D/state && grep -q "^clientprog $W1 1 42" $D/state && echo 1)" "$(grep "$W1" $D/state)"
	ok "...the other app's window: nothing" "$(! grep -q "^clientprog $W3 " $D/state && echo 1)" "$(grep "$W3" $D/state)"
	f1=$(fillw $W1); NF=$(echo "$f1" | python3 -c "import sys;a,b=map(float,sys.stdin.read().split());print(a/(b-8))" 2>/dev/null)
	ok "21. Native: the fill drawn on the minimized window's button" "$([ "${f1%% *}" -gt 20 ] 2>/dev/null && echo 1)" "$f1"
	if [ -s $T/std.json ]; then SF=$(python3 -c "import json;print(json.load(open('$T/std.json'))['frac'])")
		ok "22. Standard and Native the same: the fill 42 % of its button on both (Standard $SF, Native ${NF:-none})" "$(python3 -c "import sys;sys.exit(0 if abs($SF-.42)<.015 and abs(${NF:-9}-.42)<.03 else 1)" && echo 1)" "std $SF, native $f1"
	fi
	set -- $(grep "^hit win $W1 " $D/state | head -1 | cut -d' ' -f4-7); xdotool mousemove $(( $1 + $3 / 2 )) $(( $2 + $4 / 2 )) click 1; w8 "grep -q '^client $W1 .* min=0 ' $D/state" 40; timeout 1 tail -f /dev/null
	f2=$(fillw $W1); ok "23. Native: restored (its button pressed): no fill drawn (only while minimized)" "$(grep -q "^client $W1 .* min=0 " $D/state && [ "${f2%% *}" = 0 ] && echo 1)" "$f2 $(grep "$W1" $D/state)"
	say 9 "u $U1 - 0"; w8 "! grep -q '^clientprog $W1 ' $D/state" 40
	rm -f $T/fn; mkfifo $T/fn; $T/bin/lxwin firefox Navigator "Downloads - Firefox" < $T/fn > $T/fn.out & echo $! > $T/kill.fnw; timeout 600 tail -f /dev/null > $T/fn & echo $! > $T/kill.fnh
	w8 "grep -q '^id' $T/fn.out 2>/dev/null" 50; WN=$(sed -n 's/^id 0x//p' $T/fn.out); w8 "grep -q '^client $WN ' $D/state" 80
	echo "p 64" > $T/fn; w8 "grep -q '^clientprog $WN 1 64' $D/state" 40
	ok "N1. Native: a window's own _NET_WM_XAPP_PROGRESS (64, as Firefox's): its progress, for its button" "$(grep -q "^clientprog $WN 1 64" $D/state && echo 1)" "$(grep "$WN" $D/state)"
	echo "p 0" > $T/fn; w8 "! grep -q '^clientprog $WN ' $D/state" 40
	ok "N2. ...0: ended and gone" "$(! grep -q "^clientprog $WN " $D/state && echo 1)" "$(grep "$WN" $D/state)"
	ok "24. Native: the app done: its progress ended and gone" "$(! grep -q "^clientprog $W1 " $D/state && echo 1)" "$(grep "$W1" $D/state)"
else echo "SKIP 18-24: no Xvfb, xdotool, ImageMagick or WebKitGTK"; fi
bye 9
echo "$((N-F))/$N passed"; [ $F = 0 ]
