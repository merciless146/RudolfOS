#!/bin/sh
# 1.6.2 (docs/plan-1.6.2.md Part 25b, its left part): what Linux apps need to send Rudolf their progress. The stand-in app of
# lxprog62_t.js and lxprog162_t.sh (fixtures/lpsend.c) is started as Rudolf starts a Linux app (rudolfd's apps/run: su to the
# account's user, its session's environment file, "rudolf-apps launch ID" through the app's launcher), on the account's own bus
# with its own rudolf-portal, and follows Chromium's, Electron 43's or Electron's newest rule before sending (LPSEND_AS: as their
# sources say), so it sends only when it believes the desktop allows it:
#   the environment the session gives (XDG_CURRENT_DESKTOP Rudolf:GNOME, or Rudolf), and with Unity added (the change considered);
#   Chromium believes nothing of it (the desktop is not Unity or KDE; with Unity, still no libunity), nor Electron 43 (libunity and
#   a com.canonical.Unity owner); Electron's newest sends and the portal hears it.
# Then Rudolf's own Flatpak override (/usr/lib/rudolf/appprogress): only --talk-name=com.canonical.Unity, written by the real
# flatpak in a folder of its own (FLATPAK_SYSTEM_DIR), for each installed app with --all (a stand-in flatpak), nothing for an ID
# that is not one; a stand-in Flatpak sandbox whose /.flatpak-info has what the override gives is heard, one without it is not;
# rudolfd runs it after an install, Update all and a native app's install. Needs root (a private /run, a test user), gcc, GLib,
# dbus-daemon, su; the Flatpak part flatpak (else skipped).   sh lxenv162_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$LXE_INNER" ] && exec env LXE_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; T=$(mktemp -d /tmp/lxenv162.XXXXXX); chmod 755 $T
mount --make-rprivate / 2>/dev/null; mount -t tmpfs none /run; mkdir -p /run/rudolf/portal/req /run/rudolf/portal/resp; chmod 1733 /run/rudolf/portal/req /run/rudolf/portal/resp
REQ=/run/rudolf/portal/req; MADE=""
id lxeann >/dev/null 2>&1 || { useradd -M -s /usr/sbin/nologin lxeann 2>/dev/null && MADE=lxeann; }
id lxeann >/dev/null 2>&1 || { echo "SKIP: could not make the test user"; exit 0; }
UA=$(id -u lxeann)
cleanup() { for p in $(cat $T/kill.* 2>/dev/null); do kill $p 2>/dev/null; done; timeout 0.5 tail -f /dev/null; [ -n "$MADE" ] && userdel -f $MADE 2>/dev/null; rm -rf $T; }
trap cleanup EXIT
CF="$(pkg-config --cflags --libs gio-2.0 gio-unix-2.0)"
mkdir -p $T/bin $T/exports/applications $T/h $T/io; chmod 755 $T/bin $T/exports $T/exports/applications; chown lxeann $T/h $T/io
gcc -O2 -w -o $T/bin/rudolf-portal $SYS/rudolf-portal.c $CF -lcrypto && gcc -O2 -w -o $T/bin/lpsend $HERE/fixtures/lpsend.c $CF -ldl \
	&& gcc -O1 -w -DLXP_EXPORTS="\"$T/exports\"" -o $T/bin/lxprogapi $HERE/fixtures/lxprogapi.c -lz && gcc -O2 -w -o $T/bin/rudolf-apps $SYS/rudolf-apps.c -lz || { echo "FAIL build"; exit 1; }
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
w8() { i=0; while [ $i -lt ${2:-40} ]; do eval "$1" && return 0; timeout 0.1 tail -f /dev/null; i=$((i+1)); done; return 1; }
api() { $T/bin/lxprogapi $UA; }
NONE='{"ok":true,"apps":[],"windows":[]}'
# the account's own session bus and portal, as rudolfd starts them (its user's)
setpriv --reuid=$UA --regid=$(id -g lxeann) --clear-groups env HOME=$T/h dbus-daemon --session --fork --print-address=1 --print-pid=1 > $T/bus
sed -n 2p $T/bus >> $T/kill.bus; BA=$(sed -n 1p $T/bus); [ -n "$BA" ] || { echo "FAIL the test bus did not start"; exit 1; }
asu() { setpriv --reuid=$UA --regid=$(id -g lxeann) --clear-groups env HOME=$T/h DBUS_SESSION_BUS_ADDRESS=$BA "$@"; }
setpriv --reuid=$UA --regid=$(id -g lxeann) --clear-groups env HOME=$T/h DBUS_SESSION_BUS_ADDRESS=$BA $T/bin/rudolf-portal 2>>$T/portal.log & echo $! > $T/kill.portal
w8 "asu gdbus introspect --session -d org.freedesktop.portal.Desktop -o /org/freedesktop/portal/desktop >/dev/null 2>&1" 80
# the session's environment files, as rudolfd (au_session_start) and the session script write them
F1=$SYS/rudolfd-users.inc; F2=$SYS/root/usr/lib/rudolf/session
ok "0. what the session gives its apps (the sources): the account's XDG_CURRENT_DESKTOP Rudolf:GNOME (with its keyring) or Rudolf; the desktop user's Rudolf" "$(grep -q 'XDG_CURRENT_DESKTOP=%s' $F1 && grep -q 'kr ? "Rudolf:GNOME" : "Rudolf"' $F1 && grep -q 'XDG_CURRENT_DESKTOP=Rudolf"' $F2 && echo 1)"
envf() { printf 'DISPLAY=:0\nHOME=%s\nUSER=lxeann\nLOGNAME=lxeann\nXDG_RUNTIME_DIR=%s\nXDG_CURRENT_DESKTOP=%s\nPULSE_SERVER=unix:/run/rudolf/pulse/native\nDBUS_SESSION_BUS_ADDRESS=%s\n' $T/h $T/h "$1" "$BA" > $T/env; chmod 644 $T/env; }
# the stand-in app's launcher (Flatpak's exported .desktop), its program reading its commands from a fifo
printf '[Desktop Entry]\nName=Fetch\nExec=%s/bin/fetch %%U\nType=Application\n' $T > $T/exports/applications/org.example.Fetch.desktop
printf '#!/bin/sh\nenv > %s/io/seen; LPSEND_AS=$(cat %s/as) exec %s/bin/lpsend < %s/fifo > %s/io/out 2>&1\n' $T $T $T $T $T > $T/bin/fetch; chmod 755 $T/bin/fetch
U1='application://org.example.Fetch.desktop'
# started as rudolfd's apps/run starts it: su to the account's user, its environment file, rudolf-apps launch
start() { echo "$1" > $T/as; chmod 644 $T/as; rm -f $T/fifo $T/io/out; mkfifo $T/fifo; chmod 666 $T/fifo
	setsid su -s /bin/sh lxeann -c "set -a; . $T/env; set +a; RA_EXPORTS=$T/exports exec $T/bin/rudolf-apps launch org.example.Fetch" </dev/null >>$T/launch.log 2>&1 &
	timeout 600 tail -f /dev/null > $T/fifo & echo $! > $T/kill.w
	w8 "grep -q '^ready' $T/io/out 2>/dev/null" 80 || echo "(the stand-in did not start: $(cat $T/launch.log $T/io/out 2>/dev/null | tail -5))"; }
say() { timeout 3 sh -c "echo '$1' > $T/fifo"; w8 "grep -q '^sent\|^skipped' $T/io/out" 20; timeout 0.4 tail -f /dev/null; }
stop() { timeout 3 sh -c "echo q > $T/fifo"; timeout 0.4 tail -f /dev/null; kill $(cat $T/kill.w) 2>/dev/null; rm -f $T/kill.w; timeout 0.3 tail -f /dev/null; }
bel() { sed -n 's/^believes //p' $T/io/out; }
envf "Rudolf:GNOME"; start chromium; say "u $U1 0.42 1"; o=$(api)
ok "1. started as Rudolf starts a Linux app (its user, rudolf-apps launch through its launcher): it sees the session's XDG_CURRENT_DESKTOP Rudolf:GNOME" "$(grep -qx 'XDG_CURRENT_DESKTOP=Rudolf:GNOME' $T/io/seen && grep -qx "USER=lxeann" $T/io/seen && echo 1)" "$(cat $T/io/seen | head -20) $(cat $T/launch.log)"
ok "2. Chromium's rule (download_status_updater_linux.cc): the desktop is GNOME, not Unity or KDE: nothing sent, nothing shown" "$([ "$(bel)" = "0 the desktop is GNOME, not Unity or KDE" ] && grep -q '^skipped' $T/io/out && [ "$o" = "$NONE" ] && echo 1)" "$(cat $T/io/out) $o"
stop; envf "Rudolf:Unity:GNOME"; start chromium; say "u $U1 0.42 1"; o=$(api)
ok "3. ...with Unity added to the apps' XDG_CURRENT_DESKTOP (the change considered): still nothing, as libunity is not there (nor in Flathub's runtimes) or nothing owns com.canonical.Unity" "$(bel | grep -qx '0 no libunity\|0 com.canonical.Unity has no owner' && [ "$o" = "$NONE" ] && echo 1)" "$(cat $T/io/out) $o"
stop; envf "Rudolf:GNOME"; start electron43; say "u $U1 0.42 1"; o=$(api)
ok "4. Electron up to 43 (unity_service.cc): libunity and a com.canonical.Unity owner, whatever the desktop: nothing sent" "$(bel | grep -qx '0 no libunity\|0 com.canonical.Unity has no owner' && [ "$o" = "$NONE" ] && echo 1)" "$(cat $T/io/out) $o"
stop; start electron; say "u $U1 0.42 1"; o=$(api)
ok "5. Electron's newest (launcher_entry.cc: the signal straight on the bus): sent, heard by the account's portal, shown (42 %)" "$([ "$(bel)" = 1 ] && grep -q '^sent' $T/io/out && [ "$o" = '{"ok":true,"apps":[{"id":"org.example.Fetch","wmclass":"","v":42}],"windows":[]}' ] && echo 1)" "$(cat $T/io/out) $o $(tail -3 $T/portal.log)"
stop; o=$(api); ok "...it ends: gone" "$([ "$o" = "$NONE" ] && echo 1)" "$o"
envf "Rudolf"; start chromium; stop
ok "6. the desktop user's session (XDG_CURRENT_DESKTOP Rudolf): Chromium's rule believes nothing either" "$(grep -qx 'XDG_CURRENT_DESKTOP=Rudolf' $T/io/seen && [ "$(bel)" = "0 the desktop is OTHER, not Unity or KDE" ] && echo 1)" "$(cat $T/io/out)"
# ---- Rudolf's own Flatpak override
AP=$SYS/root/usr/lib/rudolf/appprogress
mkdir -p $T/sb; SB=$T/sb; cat > $SB/flatpak <<Y
#!/bin/sh
echo "flatpak \$*" >> $T/fplog
case "\$1" in list) printf 'org.example.Fetch\ncom.discordapp.Discord\n';; override) exit 0;; esac
Y
chmod 755 $SB/flatpak
: > $T/fplog; RUDOLF_TEST_PATH=$SB sh $AP --all; r=$?
ok "7. appprogress --all (Update all): the one permission for each installed app" "$([ $r = 0 ] && [ "$(grep -c 'override --system --talk-name=com.canonical.Unity ' $T/fplog)" = 2 ] && grep -q 'talk-name=com.canonical.Unity org.example.Fetch$' $T/fplog && grep -q 'talk-name=com.canonical.Unity com.discordapp.Discord$' $T/fplog && [ "$(grep -c override $T/fplog)" = 2 ] && echo 1)" "$(cat $T/fplog)"
: > $T/fplog; bad=""; for x in "" "-x" "../evil" ".hidden" "a b" "org.x;rm"; do RUDOLF_TEST_PATH=$SB sh $AP "$x" 2>/dev/null && bad="$bad [$x]"; done
ok "...an ID that is not one: refused, flatpak not run" "$([ -z "$bad" ] && [ ! -s $T/fplog ] && echo 1)" "$bad $(cat $T/fplog)"
if command -v flatpak >/dev/null; then
	export FLATPAK_SYSTEM_DIR=$T/fpsys; mkdir -p $FLATPAK_SYSTEM_DIR
	sh $AP org.example.Fetch; r=$?; S=$(flatpak override --system --show org.example.Fetch 2>&1)
	ok "8. appprogress ID with the real flatpak ($(flatpak --version)): its override is talk to com.canonical.Unity and nothing else" "$([ $r = 0 ] && [ "$(printf '%s' "$S" | grep -v '^$')" = "$(printf '[Session Bus Policy]\ncom.canonical.Unity=talk')" ] && echo 1)" "$r $S"
	sh $AP org.example.Fetch; ok "...again: the same (nothing added)" "$([ "$(flatpak override --system --show org.example.Fetch 2>&1)" = "$S" ] && echo 1)" "$(flatpak override --system --show org.example.Fetch)"
	ok "...other apps untouched (no override for them, none for all)" "$([ -z "$(flatpak override --system --show org.example.Other 2>/dev/null | grep -v '^$')" ] && [ -z "$(flatpak override --system --show 2>/dev/null | grep -v '^$')" ] && echo 1)" "$(ls $FLATPAK_SYSTEM_DIR/overrides)"
	unset FLATPAK_SYSTEM_DIR
	# a stand-in Flatpak sandbox: its /.flatpak-info with the app's own context and what the override gives (Flatpak writes the two
	# together), and one without it: the portal hears only the first
	fi_() { printf '[Application]\nname=org.example.Fetch\nruntime=runtime/org.freedesktop.Platform/x86_64/24.08\n\n[Context]\nsockets=x11;pulseaudio;\n\n%s\n' "$1" > $T/fi-$2; }
	fi_ "$(printf '%s\norg.freedesktop.Notifications=talk' "$S")" ok; fi_ "$(printf '[Session Bus Policy]\norg.freedesktop.Notifications=talk')" no
	box() { R=$T/root$2; mkdir -p $R; echo electron > $T/as; rm -f $T/fifo $T/io/out; mkfifo $T/fifo; chmod 666 $T/fifo
		( unshare -m sh -c "mount --make-rprivate /; mount -t tmpfs none $R; for d in usr etc tmp proc dev run var; do mkdir $R/\$d; mount --rbind /\$d $R/\$d; done
		for d in bin sbin lib lib64 lib32 libx32; do if [ -L /\$d ]; then cp -a /\$d $R/\$d; elif [ -d /\$d ]; then mkdir $R/\$d; mount --rbind /\$d $R/\$d; fi; done
		cp $1 $R/.flatpak-info; exec chroot $R setpriv --reuid=$UA --regid=$(id -g lxeann) --clear-groups env HOME=$T/h DBUS_SESSION_BUS_ADDRESS=$BA LPSEND_AS=electron $T/bin/lpsend" < $T/fifo > $T/io/out 2>&1 ) &
		timeout 600 tail -f /dev/null > $T/fifo & echo $! > $T/kill.w; w8 "grep -q '^ready' $T/io/out 2>/dev/null" 80; }
	if command -v chroot >/dev/null; then
		box $T/fi-ok 1; say "u $U1 0.5 1"; o=$(api)
		ok "9. a Flatpak app with Rudolf's override (its /.flatpak-info's Session Bus Policy): its progress heard and shown (50 %)" "$(echo "$o" | grep -q '"id":"org.example.Fetch","wmclass":"","v":50}' && echo 1)" "$o $(cat $T/io/out) $(tail -3 $T/portal.log)"
		stop; box $T/fi-no 2; say "u $U1 0.5 1"; o=$(api)
		ok "...without it (as Flathub's apps are): not heard" "$([ "$o" = "$NONE" ] && grep -q 'org.example.Fetch may not talk to com.canonical.Unity' $T/portal.log && echo 1)" "$o $(tail -3 $T/portal.log)"
		stop
	else echo "SKIP 9: no chroot for a stand-in Flatpak sandbox"; fi
else echo "SKIP 8-9: flatpak is not installed here"; fi
C=$SYS/rudolfd.c
ok "10. rudolfd runs it after an app's install, after Update all (before appextra --all) and after a native app's install" "$(grep -q '"/usr/lib/rudolf/appprogress %s || true; "' $C && grep -q 'appprogress --all; /usr/lib/rudolf/appextra --all; echo Done' $C && grep -q 'FP_INSTALL "; /usr/lib/rudolf/appprogress %s || true' $C && echo 1)"
ok "11. nothing on Rudolf OS owns com.canonical.Unity (the permission gives no service to call)" "$(! grep -rn 'own_name.*com.canonical.Unity\|g_bus_own_name[^;]*"com.canonical.Unity"\|--own-name=com.canonical.Unity' $SYS --include=*.c --include=*.inc --include=*.h 2>/dev/null | grep -q . && ! grep -rqs 'own-name=com.canonical.Unity' $SYS/root && echo 1)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
