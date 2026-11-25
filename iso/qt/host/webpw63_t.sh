#!/bin/sh
# 1.6.3 (docs/plan-1.6.3.md Part 7, step 1): Oracle's own password manager's store: rudolf-portal --webpw list|get|set|del with
# the real gnome-keyring-daemon (the account's login keyring, as /usr/lib/rudolf/keyring makes it at sign-in): kept for the
# site's exact origin and user name, read back, replaced, listed by name only (never a password), removed; the origin's rules
# (https only, a host and a port, nothing else); everything on standard input, nothing of it in a command line. Made-up
# passwords only. (CI: apt-get install gnome-keyring libsecret-tools dbus libglib2.0-dev libssl-dev)   sh webpw63_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); KR=$HERE/../../sys/root/usr/lib/rudolf/keyring; T=$(mktemp -d /tmp/wp63.XXXXXX)
trap 'kill $(cat $T/pids 2>/dev/null) 2>/dev/null; rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 600)"; F=$((F+1)); fi; }
command -v gnome-keyring-daemon >/dev/null && command -v secret-tool >/dev/null && command -v dbus-daemon >/dev/null || { echo "SKIP: gnome-keyring-daemon, secret-tool or dbus-daemon is not here (CI installs them)"; exit 0; }
gcc -O2 -w -o $T/rp $HERE/../../sys/rudolf-portal.c $(pkg-config --cflags --libs gio-2.0 gio-unix-2.0) -lcrypto || { echo "FAIL: rudolf-portal does not build"; exit 1; }
w8() { i=0; while [ $i -lt ${2:-120} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
H=$T/home; mkdir -p $H; RT=$T/rt; mkdir -m 700 $RT
cat > $T/bus.conf <<EOC
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN" "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig><type>session</type><keep_umask/><listen>unix:path=$RT/bus</listen><auth>EXTERNAL</auth>
<policy context="default"><allow send_destination="*" eavesdrop="true"/><allow eavesdrop="true"/><allow own="*"/></policy></busconfig>
EOC
dbus-daemon --config-file=$T/bus.conf --fork --nopidfile --print-pid > $T/dpid; cat $T/dpid >> $T/pids
S="env -i PATH=$PATH HOME=$H XDG_RUNTIME_DIR=$RT DBUS_SESSION_BUS_ADDRESS=unix:path=$RT/bus"
owned() { $S gdbus call --session -d org.freedesktop.DBus -o /org/freedesktop/DBus -m org.freedesktop.DBus.NameHasOwner org.freedesktop.secrets 2>/dev/null | grep -q true; }
$S sh $KR start > $T/kr.log 2>&1 & echo $! >> $T/pids
w8 'owned && $S secret-tool search --all x x >/dev/null 2>&1' 240 || { echo "FAIL: the keyring did not start - $(cat $T/kr.log)"; exit 1; }
P="$S timeout 10 $T/rp --webpw"
o=$($P list); ok "1. none kept yet: an empty list" "$([ "$o" = '{"ok":true,"locked":false,"items":[]}' ] && echo 1)" "$o"
# a password kept, its arguments watched: nothing of it in a command line (any user reads /proc/PID/cmdline)
( while [ ! -e $T/stop ]; do for c in /proc/[0-9]*/cmdline; do { tr '\0' ' ' < $c; } 2>/dev/null; echo; done; done > $T/cmdlines ) & echo $! >> $T/pids
o=$(printf 'https://shop.example\nann@example.com\nMade-up-Pass-1!\n' | $P set 2>&1); touch $T/stop
ok "2. kept for the site's origin and user name" "$(echo "$o" | grep -q '"ok":true' && echo 1)" "$o"
ok "...nothing of it in a command line" "$(! grep -q 'Made-up-Pass-1\|shop\.example\|ann@example' $T/cmdlines && echo 1)" "$(grep -m2 'Made-up\|shop' $T/cmdlines)"
o=$(printf 'https://shop.example\nann@example.com\n' | $P get 2>&1); ok "3. read back for that origin and user" "$([ "$o" = 'Made-up-Pass-1!' ] && echo 1)" "$o"
for other in 'http://shop.example' 'https://www.shop.example' 'https://shop.example:8443' 'https://example' 'https://shop.example.evil.example' 'https://shop.examp1e'; do
	o=$(printf '%s\nann@example.com\n' "$other" | $P get 2>&1); r=$?; [ $r = 0 ] && { echo "given for $other: $o"; break; }; done > $T/other
ok "4. never for another origin: http, a subdomain, another port, a parent or a look-alike host" "$([ ! -s $T/other ] && echo 1)" "$(cat $T/other)"
o=$(printf 'https://shop.example\nbob\n' | $P get 2>&1); r=$?; ok "...nor for another user name of the same site (exit 1: none kept)" "$([ $r = 1 ] && echo "$o" | grep -q 'none kept' && echo 1)" "$r $o"
printf 'https://shop.example\nann@example.com\nMade-up-Pass-2?\n' | $P set >/dev/null 2>&1; printf 'https://news.example:8443\n\nMade-up-Pass-3\n' | $P set >/dev/null 2>&1
o=$(printf 'https://shop.example\nann@example.com\n' | $P get 2>&1); l=$($P list)
ok "5. kept again: replaced (one item for the site and user), a site with a port and no user name kept too" "$([ "$o" = 'Made-up-Pass-2?' ] && [ "$(echo "$l" | grep -o '"origin":"https://shop.example"' | wc -l)" = 1 ] && echo "$l" | grep -q '"origin":"https://news.example:8443","user":""' && echo 1)" "$o / $l"
ok "6. the list names the sites and user names, with when, never a password" "$(echo "$l" | grep -q '"origin":"https://shop.example","user":"ann@example.com","when":[1-9]' && ! echo "$l" | grep -q 'Made-up' && echo 1)" "$l"
l2=$($S timeout 10 $T/rp --keyring-list); ok "...and Settings › Privacy's list of the keyring names them by label (Oracle: the site), never a password" "$(echo "$l2" | grep -q 'Oracle: shop.example (ann@example.com)' && ! echo "$l2" | grep -q 'Made-up' && echo 1)" "$l2"
bad=''; for o0 in 'http://shop.example' 'https://' 'https://Shop.example' 'https://shop.example/' 'https://shop.example/path' 'https://user@shop.example' 'https://shop.example:0' 'https://shop.example:99999' 'https://-x.example' 'ftp://shop.example' 'https://shop example'; do
	r=$(printf '%s\nann\nMade-up-Pass-4\n' "$o0" | $P set 2>&1); [ $? = 2 ] || bad="$bad [$o0: $r]"; done
ok "7. only a site's own origin is kept: https, a lower-case host, a port; never a path, a user, http, another scheme" "$([ -z "$bad" ] && echo 1)" "$bad"
r=$(printf 'https://shop.example\nann\001x\nMade-up\n' | $P set 2>&1); r1=$?; r=$(printf 'https://shop.example\nann\n\n' | $P set 2>&1); r2=$?
ok "...nor a user name with control characters, nor an empty password" "$([ $r1 = 2 ] && [ $r2 = 2 ] && echo 1)" "$r1 $r2"
o=$(printf 'https://shop.example\nann@example.com\n' | $P del 2>&1); g=$(printf 'https://shop.example\nann@example.com\n' | $P get 2>&1); r=$?
ok "8. removed: gone" "$(echo "$o" | grep -q '"ok":true' && [ $r = 1 ] && ! $P list | grep -q 'shop.example' && $P list | grep -q 'news.example' && echo 1)" "$o / $g"
ok "9. kept in the account's own keyring (its folder, its user's only)" "$([ "$(stat -c %a $H/.local/share/keyrings/login.keyring)" = 600 ] && grep -q 'Oracle: news.example' $H/.local/share/keyrings/login.keyring && echo 1)"
o=$($P nothing 2>&1); ok "10. only list, get, set and del" "$([ $? = 2 ] && echo 1)" "$o"
echo "$((N-F))/$N passed"; [ $F = 0 ]
