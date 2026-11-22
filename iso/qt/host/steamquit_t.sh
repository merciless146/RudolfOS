#!/bin/sh
# 1.6.2 Part 14 (the owner's Vivobook, 2026-11-22: Steam hard to shut down): Steam's watcher (rudolf-apps steamwatch, as rudolf-apps
# launch starts it beside Steam) on Xvfb, with stand-ins only: "Steam's sandbox" is a folder in place of /proc (RA_PROC) naming a process
# this test started (its root/.flatpak-info says com.valvesoftware.Steam), Steam's window is an X client of class or name "steam"
# (xclock -name steam), and the shutdown (RA_STEAM_CMD) is a script that marks that it ran and, for a Steam that obeys, ends the
# test's own stand-in. Nothing this test did not start is stopped.   sh steamquit_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; T=$(mktemp -d /tmp/steamquit_t.XXXXXX); DSP=${SQ_DSP:-:87}
command -v Xvfb >/dev/null && command -v xclock >/dev/null || { echo "SKIP: needs Xvfb and xclock (x11-apps)"; rm -rf $T; exit 0; }
gcc -O2 -w -o $T/ra $SYS/rudolf-apps.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
Xvfb $DSP -screen 0 800x600x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
MINE=""; trap 'kill $XV $MINE 2>/dev/null; rm -rf $T' EXIT
export DISPLAY=$DSP XDG_RUNTIME_DIR=$T RA_PROC=$T/proc RA_STEAM_GRACE_MS=1500 RA_STEAM_POLL_MS=200 RA_STEAM_WAIT_MS=5000 RA_STEAM_QUIT_MS=3000
i=0; while [ $i -lt 40 ] && ! xclock -help >/dev/null 2>&1 && [ ! -e /tmp/.X11-unix/X${DSP#:} ]; do timeout 0.25 tail -f /dev/null; i=$((i+1)); done
timeout 1 tail -f /dev/null
P=0; F=0; ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
# the stand-in for Steam: a process of the test's own, and its entry in the folder in place of /proc (gone with it)
steam_up() { timeout 120 tail -f /dev/null >/dev/null 2>&1 & SP=$!; MINE="$MINE $SP"; mkdir -p $T/proc/$SP/root; printf '[Application]\nname=com.valvesoftware.Steam\nruntime=runtime/org.freedesktop.Platform/x86_64/24.08\n' > $T/proc/$SP/root/.flatpak-info
	printf 'steam\0-silent\0' > $T/proc/$SP/cmdline; ( while kill -0 $SP 2>/dev/null; do timeout 0.1 tail -f /dev/null; done; rm -rf $T/proc/$SP ) >/dev/null 2>&1 & MINE="$MINE $!"; }
win_up() { xclock -name steam -geometry 120x120+10+10 >/dev/null 2>&1 & XC=$!; MINE="$MINE $XC"; timeout 1 tail -f /dev/null; }
gone() { ! kill -0 "$1" 2>/dev/null; }
# (an obedient Steam: the shutdown ends it; a stubborn one: the shutdown only marks)
OBEY='echo ran >> '$T'/shut; kill $(ls '$T'/proc) 2>/dev/null; true'; STUB='echo ran >> '$T'/shut'
watch() { [ -n "$WP" ] && waitfor "gone $WP" 5; rm -f $T/shut; RA_STEAM_CMD="$1" $T/ra steamwatch 2>>$T/w.log & WP=$!; MINE="$MINE $WP"; }
waitfor() { i=0; while [ $i -lt $(( $2 * 10 )) ]; do eval "$1" && return 0; timeout 0.1 tail -f /dev/null; i=$((i+1)); done; return 1; }

# 1. Steam's window shown: nothing is done while it is there (4 times the grace)
steam_up; win_up; watch "$OBEY"; timeout 6 tail -f /dev/null
[ ! -e $T/shut ] && ! gone $SP; ok "its window shown: Steam is left running" $?
# 2. its last window closed (Steam gone to its tray): asked to quit within the grace, and it quits; the watcher ends
kill $XC; t0=$(date +%s%N); waitfor "[ -e $T/shut ]" 6; r=$?; ms=$(( ($(date +%s%N) - t0) / 1000000 ))
ok "its last window closed: steam -shutdown asked (after ${ms} ms, the grace 1500 ms)" $r
waitfor "gone $SP" 3; ok "Steam quit by itself" $?
waitfor "gone $WP" 3; ok "the watcher ended with it" $?
# 3. a game of Steam's running (its launcher, reaper SteamLaunch, in the sandbox) with no window of Steam's: nothing is done
steam_up; win_up; timeout 30 tail -f /dev/null >/dev/null 2>&1 & GP=$!; MINE="$MINE $GP"; mkdir -p $T/proc/$GP/root; cp $T/proc/$SP/root/.flatpak-info $T/proc/$GP/root/; printf '/home/x/.steam/reaper\0SteamLaunch\0AppId=570\0--\0game\0' > $T/proc/$GP/cmdline
watch "$OBEY"; timeout 1.5 tail -f /dev/null; kill $XC; timeout 5 tail -f /dev/null
[ ! -e $T/shut ] && ! gone $SP; ok "a game running: Steam is left running when its window is closed" $?
# ...and once the game has ended, Steam is asked to quit
kill $GP; rm -rf $T/proc/$GP; waitfor "[ -e $T/shut ]" 6; ok "the game ended: steam -shutdown asked" $?
waitfor "gone $SP" 3; ok "Steam quit" $?
# 4. a Steam that does not quit when asked: Rudolf's escalation (TERM) after RA_STEAM_QUIT_MS
steam_up; win_up; watch "$STUB"; timeout 1.5 tail -f /dev/null; kill $XC
waitfor "[ -e $T/shut ]" 6; ok "a stubborn Steam: steam -shutdown asked" $? "$(cat $T/w.log; ls $T/proc)"
timeout 1 tail -f /dev/null; ! gone $SP; ok "...and given its time (still running 1 s after)" $?
waitfor "gone $SP" 8; ok "...then ended (TERM)" $?
grep -q "did not quit within 3 s" $T/w.log; ok "...and said so in its log" $?
# 5. Steam's sandbox there but no window of Steam's ever seen (still starting): nothing is done
steam_up; watch "$OBEY"; timeout 5 tail -f /dev/null
[ ! -e $T/shut ] && ! gone $SP; ok "no window seen yet: nothing done" $?
kill $WP $SP 2>/dev/null
# 6. one watcher per user: a second one ends at once
steam_up; win_up; watch "$OBEY"; W1=$WP; timeout 0.5 tail -f /dev/null; watch "$OBEY"; waitfor "gone $WP" 2; r=$?; ! gone $W1; ok "a second watcher ends at once; the first stays" $(( r || $? ))
kill $W1 $SP $XC 2>/dev/null
# 7. a Steam started again while the one asked is still quitting: the old one is ended, the new one is not
waitfor "gone $W1" 5; steam_up; OLD=$SP; win_up; watch "$STUB"; timeout 1.5 tail -f /dev/null; kill $XC; waitfor "[ -e $T/shut ]" 6
steam_up; waitfor "gone $OLD" 8; r=$?; timeout 1 tail -f /dev/null; ! gone $SP; ok "Steam started again meanwhile: the old one ended, the new one left" $(( r || $? ))
kill $WP $SP 2>/dev/null
# 8. its window minimised (the window manager's WM_STATE Iconic, the window unmapped, as Native minimises): Steam is left running
if command -v xdotool >/dev/null && command -v xprop >/dev/null; then
	waitfor "gone $WP" 5; steam_up; win_up; watch "$OBEY"; timeout 1 tail -f /dev/null
	XW=$(xdotool search --classname '^steam$' 2>/dev/null | head -1)
	xprop -id "$XW" -f WM_STATE 32c -set WM_STATE 3 2>/dev/null; xdotool windowunmap "$XW" 2>/dev/null; timeout 5 tail -f /dev/null
	[ -n "$XW" ] && [ ! -e $T/shut ] && ! gone $SP; ok "its window minimised: Steam is left running" $?
	kill $WP $SP $XC 2>/dev/null
else echo "SKIP its window minimised: needs xdotool and xprop"; fi
echo "$P/$((P+F)) passed"; [ $F = 0 ]
