#!/bin/sh
# 1.5.9 (plan 15.5): the list of everything that listens (sys/root/usr/share/rudolf/listeners). 1) the sources: every
# C program in sys/ that binds and listens, and every listening daemon the system's scripts start, is named in the list
# (a new one fails here until it is listed, with who may use it and how it checks); 2) rudolf-selftest's "listeners"
# line on a stand-in /proc: passes with what the list names, fails naming what it does not.   sh listeners59_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; L=$SYS/root/usr/share/rudolf/listeners
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 800)"; F=$((F+1)); fi; }
progs=$(grep -v '^#' $L | awk -F'\t' 'NF>=6{print $3}' | sort -u)
listed() { echo "$progs" | grep -qx "$1"; }
bad=$(grep -v '^#' $L | awk -F'\t' 'NF&&NF!=6{print NR": "$0}')
ok "1. the list: six columns on every line (kind, where, program, when, who, how)" "$([ -z "$bad" ] && [ -n "$progs" ] && echo 1)" "$bad"
# C programs that listen (bind and listen on a socket of their own)
miss=""; for c in $SYS/*.c; do grep -q 'listen(' $c && grep -q 'bind(' $c || continue; p=$(basename $c .c); listed $p || miss="$miss $p"; done
ok "2. every C program in sys/ that listens is in the list" "$([ -z "$miss" ] && echo 1)" "not listed:$miss"
# what the C programs start that listens (llama-server's --port)
miss=""; grep -l -- '"--port"' $SYS/*.c | while read -r c; do grep -q llama-server $c && { listed llama-server || echo llama-server; }; done > $W/m; miss=$(cat $W/m)
ok "3. a server a program starts with a port (Blitzen's llama-server) is in the list" "$([ -z "$miss" ] && echo 1)" "not listed: $miss"
ok "3b. ...and Tor's SOCKS port, which rudolfd starts for Oracle's Tor windows (16.1): loopback only" "$(if grep -q -- '"--SocksPort"' $SYS/rudolfd.c; then grep -q -- '"--SocksPort", "127.0.0.1:9050"' $SYS/rudolfd.c && listed tor && echo 1; else echo 1; fi)" "$(grep -n -- '--SocksPort' $SYS/rudolfd.c)"
# the daemons the scripts start: each known kind must be listed; anything else that looks like a listener fails
S=$(find $SYS/root -type f \( -path '*/usr/lib/rudolf/*' -o -path '*/sbin/*' -o -path '*/etc/*' \) ! -name '*.js' ! -name '*.json' ! -name '*.html' ! -name '*.css' ! -name listeners)
miss=""
for d in Xorg:Xorg Xvfb:Xorg dbus-daemon:dbus-daemon pulseaudio:pulseaudio dnsmasq:dnsmasq hostapd:hostapd wpa_supplicant:wpa_supplicant remote-debugging-port:chromium; do
	k=${d%%:*}; p=${d#*:}
	if grep -lqE "(^|[ \t(;&|])$k( |=|$)|--$k" $S 2>/dev/null; then listed $p || miss="$miss $k($p)"; fi
done
ok "4. every listening daemon the system's scripts start is in the list (X, D-Bus, PulseAudio, the hotspot's, Wi-Fi's, Chromium's test port)" "$([ -z "$miss" ] && echo 1)" "not listed:$miss"
unk=$(grep -nE '(^|[ \t;&|(])(nc|ncat|socat|httpd|busybox httpd|sshd|dropbear|telnetd|inetd|x11vnc|vncserver)( |$)|-m http\.server|--listen[= ]|-listen tcp' $S 2>/dev/null | grep -v '^[^:]*:[0-9]*:\s*#' | head -5)
ok "5. nothing else in the scripts that serves on a port (nc -l, socat, httpd, sshd, VNC, X's -listen tcp)" "$([ -z "$unk" ] && echo 1)" "$unk"
ok "6. X is started without TCP (-nolisten tcp), each time" "$(n1=$(grep -cE '^\s*(Xorg|Xvfb) ' $SYS/root/usr/lib/rudolf/session); n2=$(grep -E '^\s*(Xorg|Xvfb) ' $SYS/root/usr/lib/rudolf/session | grep -c -- '-nolisten tcp'); [ "$n1" -gt 0 ] && [ "$n1" = "$n2" ] && echo 1)" ""
ok "6b. X is started with authorization (15.3), each time: -auth with the session's cookie, never -ac on its line" "$(n1=$(grep -cE '^\s*(Xorg|Xvfb) ' $SYS/root/usr/lib/rudolf/session); n2=$(grep -E '^\s*(Xorg|Xvfb) ' $SYS/root/usr/lib/rudolf/session | grep -- '\$XAUTH' | grep -vc -- ' -ac '); [ "$n1" -gt 0 ] && [ "$n1" = "$n2" ] && grep -q 'XAUTH="-auth \$XAUTHF"' $SYS/root/usr/lib/rudolf/session && echo 1)" ""
# the self-test's line on a stand-in /proc
P=$W/proc; mkdir -p $P/net
H='  sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode'
printf '%s\n   0: 0100007F:1F90 00000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 1001 1 0 100 0 0 10 0\n   1: 0100007F:1F90 0100007F:9C40 01 00000000:00000000 00:00000000 00000000  1000        0 1002 1 0 100 0 0 10 0\n   2: 0100007F:1FA3 00000000:0000 0A 00000000:00000000 00:00000000 00000000   950        0 1003 1 0 100 0 0 10 0\n' "$H" > $P/net/tcp
printf '%s\n   0: 00000000000000000000000000000000:20FB 00000000000000000000000000000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 1004 1 0 100 0 0 10 0\n' "$H" > $P/net/tcp6
printf 'Num       RefCount Protocol Flags    Type St Inode Path\n0000000000000000: 00000002 00000000 00010000 0001 01 2001 @/tmp/.X11-unix/X0\n0000000000000000: 00000002 00000000 00010000 0001 01 2002 /tmp/.X11-unix/X0\n0000000000000000: 00000002 00000000 00010000 0001 01 2003 /run/user/1000/bus\n0000000000000000: 00000003 00000000 00000000 0001 03 2004 /run/user/1000/bus\n0000000000000000: 00000002 00000000 00010000 0001 01 2005\n0000000000000000: 00000002 00000000 00010000 0001 01 2006 /tmp/.org.chromium.Chromium.aB3dE9/SingletonSocket\n' > $P/net/unix
st() { ST_SYS=$W/sys ST_RUN=$W/run ST_PERSIST=$W/persist ST_HOME=$W/home ST_DEV=$W/dev ST_PROC=$P ST_LISTENERS=$L sh $SYS/root/usr/lib/rudolf/selftest 2>/dev/null | grep '^[A-Z]* listeners '; }
o=$(st)
ok "7. the self-test: rudolfd's 8080, Blitzen's 8099, remote access on 8443 (IPv6), X, the session bus, Chromium: all named, passes" "$(echo "$o" | grep -q '^PASS listeners - 7 listening' && echo 1)" "$o"
printf '   3: 00000000:115C 00000000:0000 0A 00000000:00000000 00:00000000 00000000  1001        0 1005 1 0 100 0 0 10 0\n' >> $P/net/tcp
printf '0000000000000000: 00000002 00000000 00010000 0001 01 2007 /home/rudolf/.evil/sock\n' >> $P/net/unix
o=$(st)
ok "8. a port and a socket the list does not name (a program listening on 0.0.0.0:4444, a socket in a home folder): fails, naming both" "$(echo "$o" | grep -q '^FAIL listeners - .*tcp 0.0.0.0:4444' && echo "$o" | grep -q '/home/rudolf/.evil/sock' && echo 1)" "$o"
printf '   4: 0100007F:2406 00000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 1006 1 0 100 0 0 10 0\n' > $P/net/tcp.x; head -1 $P/net/tcp > $P/net/tcp.y; cat $P/net/tcp.y $P/net/tcp.x > $P/net/tcp; : > $P/net/tcp6; head -1 $P/net/unix > $P/net/unix.y; mv $P/net/unix.y $P/net/unix
o=$(st); ok "9. Chromium's test port (9222, test discs only) is named too" "$(echo "$o" | grep -q '^PASS listeners - 1 listening' && echo 1)" "$o"
# 2026-11-14 (found on the Vivobook): a Linux app's own sockets inside Debian's container (the Claude app's, an Electron
# app's SingletonSocket) are the app's; Chromium's newer folder name is named; a socket in /tmp held by a program
# outside the container still fails
mkdir -p $W/run/debian $W/debroot $W/other $P/200 $P/300/fd $P/400/fd; echo 200 > $W/run/debian/init.pid; echo debian-init > $P/200/comm
ln -s $W/debroot $P/200/root; ln -s $W/debroot $P/300/root; ln -s $W/other $P/400/root
ln -s 'socket:[3001]' $P/300/fd/5; ln -s 'socket:[3002]' $P/300/fd/6; ln -s 'socket:[3004]' $P/400/fd/3; chown -h 1001 $P/300 2>/dev/null; chown 1001 $P/300; chown 1000 $P/400
{ head -1 $P/net/unix; printf '0000000000000000: 00000002 00000000 00010000 0001 01 3001 /run/user/1000/claude-cowork-vm.sock\n0000000000000000: 00000002 00000000 00010000 0001 01 3002 /tmp/scoped_dirhldzNi/SingletonSocket\n0000000000000000: 00000002 00000000 00010000 0001 01 3003 /tmp/org.chromium.Chromium.5rKOls/SingletonSocket\n'; } > $P/net/unix.y; mv $P/net/unix.y $P/net/unix
o=$(st); ok "9b. a Linux app's own sockets in Debian's container: its, said; Chromium's newer socket folder named: passes" "$(echo "$o" | grep -q "^PASS listeners - 4 listening, each named in the list (who may use it and how it checks) (and 2 of Linux apps' own, inside their containers)" && echo 1)" "$o"
printf '0000000000000000: 00000002 00000000 00010000 0001 01 3004 /tmp/sneaky/sock\n' >> $P/net/unix
o=$(st); ok "...a socket in /tmp held by a program outside the containers still fails" "$(echo "$o" | grep -q '^FAIL listeners - .*unix /tmp/sneaky/sock' && ! echo "$o" | grep -q 'scoped_dir' && echo 1)" "$o"
o=$(ST_SYS=$W/sys ST_RUN=$W/run ST_PROC=$P ST_LISTENERS=$W/none sh $SYS/root/usr/lib/rudolf/selftest 2>/dev/null | grep ' listeners ')
ok "10. without the list, the self-test says so" "$(echo "$o" | grep -q '^FAIL listeners - the list of listeners is missing' && echo 1)" "$o"
echo "$((N-F))/$N passed"; [ $F = 0 ]
