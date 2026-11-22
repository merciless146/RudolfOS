#!/bin/sh
# 1.6.2 (docs/plan-1.6.2.md Part 29): Rudolf's title bar on Standard's windows from outside the page. rudolf-display (Standard's
# window manager) on a virtual screen (Xvfb), the desktop page in Chromium as the desktop's window (class RudolfDesktop) with a
# stand-in rudolfd (xframe162_t.js: apps/windows, show, desktop, close and web/strut as rudolfd answers them, from rudolf-display's
# own files), and stand-in apps (fixtures/xwmclient.c): one asking for a frame, one drawing its own bar, a game, a dialog; then real
# apps when the host has them (GTK 3, GTK 4, Chromium with the system's title bar and Chromium as an Electron-style app with its
# own). The strip, _NET_SUPPORTING_WM_CHECK, the frame drawn by the page around each (title, icon, the title-bar styles), the X
# window kept in its frame's inside, moving, resizing from each edge and corner, maximize and restore, minimize and the taskbar
# button, close (WM_DELETE_WINDOW), another window or the desktop clicked, the workspaces, a frame dragged past the screen's edge, full screen and
# games untouched, the dialog above its window, never over the lock screen; then rudolfd's own code for apps/show's place and
# web/strut's bar (the real rudolfd, root's requests), and Native's frame of the same app compared (rudolf-native, when it
# builds: WebKitGTK's headers). Needs root (a private /run in a mount namespace), Xvfb, xdotool, xwininfo, xprop,
# ImageMagick, gcc, node with Playwright and its Chromium (or XF_CHROME).   sh xframe162_t.sh [OUT]   (ports 8602, 8603; display :91)
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$XF_INNER" ] && exec env XF_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; T=$(mktemp -d /tmp/xf162.XXXXXX); chmod 755 $T; DSP=${XF_DSP:-:91}
OUT=${1:-$T/out}; mkdir -p "$OUT"
for c in Xvfb xdotool xwininfo xprop import convert node gcc; do command -v $c >/dev/null || { echo "SKIP: needs $c"; exit 0; }; done
mount --make-rprivate / 2>/dev/null; mount -t tmpfs none /run; mkdir -p /run/rudolf   # (rudolf-display's files: this namespace's own)
L=/usr/lib/x86_64-linux-gnu
gcc -O2 -Wall -o $T/rds $SYS/rudolf-display.c -I$SYS -lX11 $L/libXtst.so.6 $L/libXfixes.so.3 $L/libXrandr.so.2 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build rudolf-display"; exit 1; }
gcc -O2 -Wall -o $T/xc $HERE/fixtures/xwmclient.c -lX11 2>$T/cc || { cat $T/cc; echo "FAIL build xwmclient"; exit 1; }
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=$DSP
trap 'kill $RDS $XV 2>/dev/null; pkill -f "$T/" 2>/dev/null; umount -l /usr/share/rudolf/www /home/rudolf /persist /usr/bin /run 2>/dev/null; rm -rf $T' EXIT
i=0; while [ $i -lt 40 ] && ! xdotool getmouselocation >/dev/null 2>&1; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
echo "bottom 48" > /run/rudolf/strut; echo desktop > /run/rudolf/wm; rm -f /run/rudolf/wm-native
$T/rds --input > $T/rds.log 2>&1 & RDS=$!
XF_T=$T XF_XC=$T/xc XF_OUT=$OUT node $HERE/xframe162_t.js; r=$?   # (XF_CHROME: a Chromium of your own; NODE_PATH: Playwright's)
# ---- rudolfd's own code: apps/show's place and web/strut's bar (the real rudolfd; root's requests are never peer-checked)
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 600)"; F=$((F+1)); fi; }
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
kill $RDS 2>/dev/null
mkdir -p $T/up $T/wk /usr/share/rudolf/www /home/rudolf /persist
if mount -t overlay overlay -o lowerdir=/usr/bin,upperdir=$T/up,workdir=$T/wk /usr/bin 2>/dev/null && gcc -O2 -w -o $T/rudolfd $SYS/rudolfd.c -lz 2>$T/cc; then
	printf '#!/bin/sh\nexit 0\n' > /usr/bin/flatpak; chmod 755 /usr/bin/flatpak   # (this namespace's /usr/bin only: rudolfd's apps/* need Flatpak there)
	mount --bind $HERE/../../www /usr/share/rudolf/www; mount -t tmpfs none /home/rudolf; mount -t tmpfs none /persist; mkdir -p /persist/etc /persist/var/log
	$T/rudolfd > $T/rd.log 2>&1 & RDP=$!; w8 'curl -s http://127.0.0.1:8080/favicon.png -o /dev/null' 60
	TOK=$(curl -s http://127.0.0.1:8080/ | sed -n 's/.*RUDOLFD={token:"\([0-9a-f]*\)".*/\1/p' | head -1)
	api() { curl -s -o $T/body -w '%{http_code}' -X POST -H "X-Rudolf-Token: $TOK" "http://127.0.0.1:8080/api/$1"; }
	c=$(api 'apps/show?win=1e00007&x=3&y=33&w=1270&h=700'); ok "R1. rudolfd: apps/show with a place writes \"win HEX X Y W H\" for rudolf-display" "$([ "$c" = 200 ] && [ "$(cat /run/rudolf/wm)" = "win 1e00007 3 33 1270 700" ] && echo 1)" "$c $(cat /run/rudolf/wm)"
	c=$(api 'apps/show?win=1e00007&x=-200&y=40&w=600&h=400'); ok "R2. ...a place partly off the screen's left (a window dragged there)" "$([ "$c" = 200 ] && [ "$(cat /run/rudolf/wm)" = "win 1e00007 -200 40 600 400" ] && echo 1)" "$c $(cat /run/rudolf/wm)"
	bad=""; for q in 'x=3&y=33&w=10&h=700' 'x=3&y=33&w=1270' 'x=3;id&y=33&w=1270&h=700' 'x=3&y=33&w=99999999&h=700' 'x=3&y=33&w=1270&h=700%0Adesktop' 'x=0x10&y=33&w=1270&h=700'; do
		api "apps/show?win=1e00007&$q" >/dev/null; [ "$(cat /run/rudolf/wm)" = "win 1e00007" ] || bad="$bad [$q: $(cat /run/rudolf/wm)]"; done
	ok "R3. ...a place that is not four numbers in range (too small, missing, other characters, too large, a new line, hexadecimal): \"win HEX\" alone, as before" "$([ -z "$bad" ] && echo 1)" "$bad"
	c=$(api 'apps/show?win=1e00007'); ok "R4. ...without a place: \"win HEX\" as before (the taskbar's buttons, Super+Tab's)" "$([ "$c" = 200 ] && [ "$(cat /run/rudolf/wm)" = "win 1e00007" ] && echo 1)" "$c $(cat /run/rudolf/wm)"
	c=$(api 'web/strut?side=bottom&size=48&bar=36'); c2=$(cat /run/rudolf/strut); api 'web/strut?side=bottom&size=48' >/dev/null; c3=$(cat /run/rudolf/strut); api 'web/strut?side=top&size=40&bar=500' >/dev/null; c4=$(cat /run/rudolf/strut)
	ok "R5. rudolfd: web/strut's bar (the strip for Rudolf's title bar): \"bottom 48 bar 36\"; without it as before; out of range left out" "$([ "$c" = 200 ] && [ "$c2" = "bottom 48 bar 36" ] && [ "$c3" = "bottom 48" ] && [ "$c4" = "top 40" ] && echo 1)" "$c [$c2] [$c3] [$c4]"
	kill $RDP 2>/dev/null; wait $RDP 2>/dev/null
else echo "SKIP R1-R5: rudolfd's own code (no overlay mount here, or rudolfd did not build: $(head -c 200 $T/cc 2>/dev/null))"; fi
echo "rudolfd: $((N-F))/$N passed"; [ $F = 0 ] && exit $r; exit 1
