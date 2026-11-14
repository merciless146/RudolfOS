#!/bin/sh
# 1.5.9 (plan 15.3, second step): X with authorization instead of -ac. The session's own x_auth and x_allow (taken
# from usr/lib/rudolf/session) and rudolf-display --xcookie/--xallow/--xdeny, on an Xvfb started as the session
# starts it: only root, the desktop's user and the signed-in account's user may use the display; another account's
# programs (and any other user's) are refused; signing out takes the display back.
# Needs root, Xvfb, xdpyinfo, setpriv and libX11's development files.   sh xauth59_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$XA59_INNER" ] && exec env XA59_INNER=1 unshare -m sh "$0" "$@"
command -v Xvfb >/dev/null && command -v xdpyinfo >/dev/null || { echo "SKIP: needs Xvfb and xdpyinfo"; exit 0; }
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; T=$(mktemp -d /tmp/xauth59.XXXXXX); chmod 755 $T
trap 'kill $XP 2>/dev/null; umount /etc 2>/dev/null; rm -rf $T' EXIT
MA=$(gcc -dumpmachine)
gcc -O1 -w -o $T/rudolf-display $SYS/rudolf-display.c -lX11 /usr/lib/$MA/libXtst.so.6 /usr/lib/$MA/libXfixes.so.3 /usr/lib/$MA/libXrandr.so.2 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
# the users as Rudolf OS has them (this namespace's /etc is a copy): the desktop's rudolf (1000), an account's acct1001
mkdir $T/etc; cp -a /etc/. $T/etc/; mount --bind $T/etc /etc
grep -q '^rudolf:' /etc/passwd || echo 'rudolf:x:1000:1000::/home/rudolf:/bin/sh' >> /etc/passwd
grep -q '^acct1001:' /etc/passwd || echo 'acct1001:x:1001:1001::/home/acct1001:/bin/sh' >> /etc/passwd
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 600)"; F=$((F+1)); fi; }
D=:97; export DISPLAY=$D
# the session's functions, with this test's folder for /run/rudolf
sed -n '/^XAUTHF=/,/^keys_evdev() {/p' $SYS/root/usr/lib/rudolf/session | sed '$d' | sed "s#/run/rudolf/#$T/#g" > $T/fns.sh
grep -q 'x_allow()' $T/fns.sh || { echo "FAIL the session has no x_allow"; exit 1; }
PATH=$T:$PATH; . $T/fns.sh
# a signed-in account's session, as rudolfd writes it
printf 'DISPLAY=:0\nHOME=/home/acct1001\nUSER=acct1001\n' > $T/acct-session.env
x_auth
ok "1. the session makes a new cookie, root's only (0600), and starts X with it, not -ac" "$([ "$XAUTH" = "-auth $T/xauth" ] && [ "$(stat -c %u:%a $T/xauth)" = 0:600 ] && echo 1)" "$XAUTH $(ls -l $T/xauth)"
Xvfb $D -screen 0 640x480x24 -nolisten tcp $XAUTH -noreset >$T/x.log 2>&1 & XP=$!
for i in $(seq 50); do [ -e /tmp/.X11-unix/X${D#:} ] && break; timeout 0.1 tail -f /dev/null; done
can() { u=$1; shift; setpriv --reuid=$u --regid=$u --clear-groups env DISPLAY=$D XAUTHORITY=/nonexistent xdpyinfo >/dev/null 2>&1 && echo yes || echo no; }
ok "2. before the session gives it out, nobody gets the display without the cookie (not even root)" "$([ "$(XAUTHORITY=/nonexistent xdpyinfo >/dev/null 2>&1 && echo yes)" != yes ] && [ "$(can 1000)" = no ] && echo 1)"
x_allow 2>$T/allow.err
ok "3. x_allow: root and the desktop's user (rudolf) may use the display, without the cookie" "$([ "$(XAUTHORITY=/nonexistent xdpyinfo >/dev/null 2>&1 && echo yes)" = yes ] && xhost 2>/dev/null | grep -q 'SI:localuser:root' && xhost 2>/dev/null | grep -q 'SI:localuser:rudolf' && echo 1)" "$(xhost 2>&1) $(cat $T/allow.err)"
ok "...and the signed-in account's user from the session's environment (an X that starts again keeps it)" "$(xhost 2>/dev/null | grep -q 'SI:localuser:acct1001' && echo 1)" "$(xhost 2>&1)"
ok "...whose programs may then use it" "$([ "$(can 1001)" = yes ] && [ "$(can 1000)" = yes ] && echo 1)"
ok "4. any other user's programs are refused (another account's, a service's)" "$([ "$(can 4242)" = no ] && [ "$(can 65534)" = no ] && echo 1)"
rudolf-display --xdeny acct1001 >/dev/null 2>&1
ok "5. at sign-out (rudolfd: --xdeny): the account's user's programs are refused" "$([ "$(can 1001)" = no ] && ! xhost 2>/dev/null | grep -q 'localuser:acct1001' && [ "$(can 1000)" = yes ] && echo 1)" "$(xhost 2>&1)"
rudolf-display --xallow acct1001 >/dev/null 2>&1
ok "6. at the next sign-in (--xallow): given again" "$([ "$(can 1001)" = yes ] && [ "$(can 4242)" = no ] && echo 1)" "$(xhost 2>&1)"
o=$(rudolf-display --xallow 'bad name' 2>&1; echo "rc=$?")
ok "7. a user name with a space or a colon is refused (nothing added)" "$(echo "$o" | grep -q 'rc=1' && ! xhost 2>/dev/null | grep -q 'bad' && echo 1)" "$o"
rm -f $T/xauth; mkdir $T/xauth.tmp; o=$(sh -c ". $T/fns.sh; x_auth; echo \"XAUTH=\$XAUTH\"" 2>&1); rmdir $T/xauth.tmp
ok "8. no cookie can be made: X starts open as before (-ac), and the session log says why" "$(echo "$o" | grep -q 'XAUTH=-ac' && echo "$o" | grep -q 'no authorization file could be made' && echo 1)" "$o"
echo "$((N-F))/$N passed"; [ $F = 0 ]
