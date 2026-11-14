#!/bin/sh
# 1.5.9 (plan Part 15.1): only the desktop gets rudolfd's page (with the API token and the page's storage) and its API.
# The real rudolfd, built here, in a private mount namespace; RUDOLFD_DESKTOP_CG stands for the desktop's memory group
# (a cgroup.procs file listing the "desktop" processes). Clients run as nobody (uid 65534), as a Linux app would run
# as an unprivileged user: one listed as the desktop's, one not; and root.
# Needs root, python3, curl; port 8080 free.   sh peer59_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$PEER_INNER" ] && exec env PEER_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; T=$(mktemp -d /tmp/peer59.XXXXXX); chmod 755 $T
mount -t tmpfs none /run; mkdir -p /run/rudolf /usr/share/rudolf/www /home/rudolf /persist
mount --bind $HERE/../../www /usr/share/rudolf/www; mount -t tmpfs none /home/rudolf; mount -t tmpfs none /persist
gcc -O2 -w -o $T/rudolfd $SYS/rudolfd.c -lz || { echo "FAIL build rudolfd"; exit 1; }
mkdir -p $T/cg; : > $T/cg/cgroup.procs
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 600)"; F=$((F+1)); fi; }
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
start() { env $1 $T/rudolfd > $T/rd.log 2>&1 & RDP=$!; w8 'curl -s http://127.0.0.1:8080/favicon.png -o /dev/null' 40; }
trap 'kill $RDP 2>/dev/null; umount /usr/share/rudolf/www /home/rudolf /persist 2>/dev/null; rm -rf $T' EXIT
# a client as nobody: waits for $T/go.NAME, then GETs PATH (with the token in $T/tok if there is one), prints "CODE BODY"
cat > $T/cl.py <<'EOF'
import os, sys, time, urllib.request, urllib.error
name, path = sys.argv[1], sys.argv[2]
os.setgid(65534); os.setuid(65534)
open(sys.argv[3] + '/pid.' + name, 'w').write(str(os.getpid()))
while not os.path.exists(sys.argv[3] + '/go.' + name): time.sleep(.05)
tok = open(sys.argv[3] + '/tok').read().strip() if os.path.exists(sys.argv[3] + '/tok') else ''
q = urllib.request.Request('http://127.0.0.1:8080' + path, headers={'X-Rudolf-Token': tok})
try: r = urllib.request.urlopen(q, timeout=10); print(r.status, r.read().decode('utf8', 'replace')[:200000])
except urllib.error.HTTPError as e: print(e.code, e.read().decode('utf8', 'replace')[:2000])
EOF
chmod 644 $T/cl.py; chmod 777 $T
# run(NAME PATH DESKTOP?): start the client, list it as the desktop's if asked, let it go, its answer
run() { rm -f $T/pid.$1 $T/go.$1; python3 $T/cl.py $1 "$2" $T > $T/out.$1 2>&1 & w8 "[ -s $T/pid.$1 ]" 40
  [ "$3" = desk ] && echo "$(cat $T/pid.$1)" >> $T/cg/cgroup.procs; touch $T/go.$1; wait $!; cat $T/out.$1; }
code() { echo "$1" | head -1 | cut -d' ' -f1; }
start "RUDOLFD_DESKTOP_CG=$T/cg"
o=$(run d1 / desk)
ok "1. the desktop (a process of its memory group) gets the page, with the token" "$([ "$(code "$o")" = 200 ] && echo "$o" | grep -q 'RUDOLFD={token:"' && echo 1)" "$(echo "$o" | head -c 300)"
echo "$o" | sed -n 's/.*RUDOLFD={token:"\([0-9a-f]*\)".*/\1/p' | head -1 > $T/tok; chmod 644 $T/tok; TOK=$(cat $T/tok)
ok "...a token of 32 hex digits" "$([ ${#TOK} = 32 ] && echo 1)" "$TOK"
o=$(run d2 /api/ping desk)
ok "2. ...and the API with it" "$([ "$(code "$o")" = 200 ] && echo 1)" "$o"
rm -f $T/tok; o=$(run x1 / no)
ok "3. another program (not the desktop's, an ordinary user) is refused the page" "$([ "$(code "$o")" = 403 ] && echo 1)" "$(echo "$o" | head -c 300)"
ok "...and the answer carries neither the token nor the page's storage" "$(echo "$o" | grep -q "$TOK\|rudolf-ls\|RUDOLFD" && echo 0 || echo 1)" "$o"
echo "$TOK" > $T/tok; chmod 644 $T/tok; o=$(run x2 /api/ping no)
ok "4. ...and the API, even with the right token (read some other way)" "$([ "$(code "$o")" = 403 ] && echo 1)" "$o"
o=$(run x3 /api/system/info no)
ok "...any API request" "$([ "$(code "$o")" = 403 ] && echo 1)" "$o"
o=$(run x4 /favicon.png no)
ok "5. what is for everyone stays so: the page's files (here its icon)" "$([ "$(code "$o")" = 200 ] && echo 1)" "$(echo "$o" | head -c 100)"
o=$(run x5 /lib/ no); c=$(code "$o")
ok "...and /lib/ is not refused as the page is" "$([ "$c" != 403 ] && echo 1)" "$c"
c=$(curl -s -o /dev/null -w '%{http_code}' http://127.0.0.1:8080/); c2=$(curl -s -o /dev/null -w '%{http_code}' -H "X-Rudolf-Token: $TOK" http://127.0.0.1:8080/api/ping)
ok "6. root (rudolfd's helpers, the devlink) is allowed: the page and the API" "$([ "$c" = 200 ] && [ "$c2" = 200 ] && echo 1)" "$c $c2"
o=$(run d3 /api/ping desk)
ok "7. a second desktop process (another of the browser's) is allowed too" "$([ "$(code "$o")" = 200 ] && echo 1)" "$o"
ok "...and the last one that answered is remembered (tried first next time), root-only" "$([ "$(cat /run/rudolf/peer-hint 2>/dev/null)" = "$(cat $T/pid.d3)" ] && [ "$(stat -c %a /run/rudolf/peer-hint)" = 600 ] && echo 1)" "$(cat /run/rudolf/peer-hint 2>/dev/null) $(cat $T/pid.d3)"
o=$(run x6 /api/ping no)
ok "8. a program that is not the desktop is still refused after the remembered one" "$([ "$(code "$o")" = 403 ] && echo 1)" "$o"
H=$(cat /run/rudolf/health.log 2>/dev/null)
ok "9. the health log says so (Security, the user), once a minute at most, without the token" "$(echo "$H" | grep -c 'security.*not the desktop (Linux user 65534)' | grep -qx 1 && ! echo "$H" | grep -q "$TOK" && echo 1)" "$H"
kill $RDP; wait $RDP 2>/dev/null
# no desktop group: not enforced, as before 1.5.9, and said once
rm -f /run/rudolf/health.log; start "RUDOLFD_DESKTOP_CG=$T/none"
rm -f $T/tok; o=$(run y1 / no)
ok "10. no desktop memory group (an old kernel): not enforced, the page as before" "$([ "$(code "$o")" = 200 ] && echo 1)" "$(echo "$o" | head -c 200)"
run y2 / no >/dev/null
ok "...said once in the health log" "$(grep -c 'memory group is missing' /run/rudolf/health.log 2>/dev/null | grep -qx 1 && echo 1)" "$(cat /run/rudolf/health.log 2>/dev/null)"
kill $RDP; wait $RDP 2>/dev/null
# an empty desktop group (the session could not put the browser in it): not enforced either, so the desktop is never shut out
mkdir -p $T/cg0; : > $T/cg0/cgroup.procs; rm -f /run/rudolf/peer-unchecked; start "RUDOLFD_DESKTOP_CG=$T/cg0"
o=$(run z1 / no)
ok "11. an empty desktop group: not enforced (the desktop is never shut out of its own page)" "$([ "$(code "$o")" = 200 ] && echo 1)" "$(echo "$o" | head -c 200)"
kill $RDP; wait $RDP 2>/dev/null
echo "$((N-F))/$N passed"; [ $F = 0 ]
